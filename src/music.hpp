#pragma once

#include <stdint.h>

#include <EEPROM.h>
#include "campaign.hpp"
#include "common.hpp"
#include "generated/music.hpp"
#include "music_logic.hpp"

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}

// Device music boundary. The fixed build uses the ATMlib2FX compact synth:
// one Timer3/oscillator ISR and one Timer4 PWM carrier, fed from the six
// authored C1 streams by a foreground FX refill.
//
// MUSIC ON/OFF is a one-byte user setting at EEPROM address 656, above the
// retired voyage allocation and outside the Arduboy2 system prefix (0..15).
// Voyage checkpoints live in the cart save partition, so Continue/recovery
// cannot roll this setting back. Writes happen only on the pause-menu toggle.
namespace music {

constexpr uint16_t kSettingAddress = 656;
constexpr uint8_t kSettingOff = 0;
constexpr uint8_t kSettingOn = 1;

namespace backend {

constexpr uint8_t kCueCount = 6;

// The shipped C1 cues are in the first 64 KiB and each is below 64 KiB.
// List initialization rejects narrowing if future assets exceed either bound.
// The decoder API still receives full-width offsets and lengths.

#if defined(__AVR__)
static const uint16_t kCueOffsets[kCueCount] PROGMEM = {
    static_cast<uint32_t>(sh_harbor_bubbles),
    static_cast<uint32_t>(sh_kelp_drift),
    static_cast<uint32_t>(sh_sonar_lullaby),
    static_cast<uint32_t>(sh_abyssal_glass),
    static_cast<uint32_t>(sh_tidepool_waltz),
    static_cast<uint32_t>(sh_bubblejet_run),
};
static const uint16_t kCueLengths[kCueCount] PROGMEM = {
    music_data::sh_harbor_bubbles_bytes,
    music_data::sh_kelp_drift_bytes,
    music_data::sh_sonar_lullaby_bytes,
    music_data::sh_abyssal_glass_bytes,
    music_data::sh_tidepool_waltz_bytes,
    music_data::sh_bubblejet_run_bytes,
};
inline uint32_t cueOffset(uint8_t cue) {
    return pgm_read_word(&kCueOffsets[cue]);
}
inline uint32_t cueLength(uint8_t cue) {
    return pgm_read_word(&kCueLengths[cue]);
}
#else
static const uint16_t kCueOffsets[kCueCount] = {
    static_cast<uint32_t>(sh_harbor_bubbles),
    static_cast<uint32_t>(sh_kelp_drift),
    static_cast<uint32_t>(sh_sonar_lullaby),
    static_cast<uint32_t>(sh_abyssal_glass),
    static_cast<uint32_t>(sh_tidepool_waltz),
    static_cast<uint32_t>(sh_bubblejet_run),
};
static const uint16_t kCueLengths[kCueCount] = {
    music_data::sh_harbor_bubbles_bytes,
    music_data::sh_kelp_drift_bytes,
    music_data::sh_sonar_lullaby_bytes,
    music_data::sh_abyssal_glass_bytes,
    music_data::sh_tidepool_waltz_bytes,
    music_data::sh_bubblejet_run_bytes,
};
inline uint32_t cueOffset(uint8_t cue) {
    return kCueOffsets[cue];
}
inline uint32_t cueLength(uint8_t cue) {
    return kCueLengths[cue];
}
#endif

inline void begin() {
    atm_synth_setup();
}
inline bool play(uint8_t cue) {
    atm_fx_compact_play(static_cast<uint32_t>(cueOffset(cue)), cueLength(cue));
    return atm_fx_compact_is_active() != 0 &&
           atm_fx_compact_state.error == 0;
}
inline void stop() {
    atm_fx_compact_stop();
}
inline void pause() {
    atm_fx_compact_pause();
}
inline void resume() {
    atm_fx_compact_resume();
}
inline void service() {
    atm_fx_compact_service();
}
inline bool active() {
    return atm_fx_compact_is_active() != 0;
}
inline bool error() {
    return atm_fx_compact_state.error != 0;
}
} // namespace backend

extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t *dst, uint8_t n);

static bool s_enabled = true;
static bool s_paused = false;
static bool s_muted = false;
static uint8_t s_backendCue = music_logic::kCueNone;
static music_logic::FlightState s_flight;

// Wrapper-level fade. ATMlib2FX owns four oscillator volume bytes and applies
// them per sample in its Timer3 ISR; the compact backend exposes play/stop/
// pause/resume but no master gain. The second osc tick callback (index 1) is
// unused by the library, so a 2 kHz callback scales the four live volumes
// after every music tick. The callback also owns the bounded ramp so a note
// command cannot leak one full-volume sample while the envelope is low.
// Base/last detect stream-written volume changes (notes, SET_VOLUME and
// slides) and rescale them without suppressing the stream's own volume
// effects. Pause/resume keeps the pre-fade volumes in s_base so a menu fade
// resumes the held notes instead of waiting for the next note command.
namespace fade {

static volatile uint8_t s_gain = music_logic::kFadeMax;
static volatile uint8_t s_target = music_logic::kFadeMax;
static volatile uint8_t s_ticks = 0;
static uint8_t s_base[4];
static uint8_t s_last[4];
static bool s_registered = false;
static uint8_t s_pendingCue = music_logic::kCueNone;
static bool s_pausePending = false;

inline void tick(uint8_t, void *) {
    if (s_gain != s_target && ++s_ticks >= music_logic::kFadeStepTicks) {
        s_ticks = 0;
        s_gain = music_logic::fadeStep(s_gain, s_target);
    }
    struct osc_params *param = osc_params_array;
    uint8_t *base = s_base;
    uint8_t *last = s_last;
    const uint8_t gain = s_gain;
    for (uint8_t n = 0; n < 4; ++n, ++param, ++base, ++last) {
        const uint8_t current = param->vol;
        if (current != *last)
            *base = current;
        const uint8_t out = static_cast<uint8_t>(
            (static_cast<uint16_t>(*base) * gain) >> 7);
        param->vol = out;
        *last = out;
    }
}

inline void startRamp(uint8_t target) {
    s_target = target;
    s_ticks = 0;
    if (!s_registered) {
        osc_set_tick_rate(1, music_logic::kFadeTickHz);
        osc_set_tick_callback(1, tick, NULL);
        s_registered = true;
    }
}

// Stop applying the envelope. `settle` also restores the true (unscaled)
// volumes; callers that are about to stop the backend must not do that.
inline void stopRamp() {
    if (s_registered) {
        osc_set_tick_callback(1, NULL, NULL);
        s_registered = false;
    }
    s_ticks = 0;
}

inline void settle() {
    if (!s_registered)
        return;
    stopRamp();
    for (uint8_t n = 0; n < 4; ++n)
        osc_params_array[n].vol = s_base[n];
    s_gain = music_logic::kFadeMax;
    s_target = music_logic::kFadeMax;
}

inline void abandon() {
    stopRamp();
    s_gain = music_logic::kFadeMax;
    s_target = music_logic::kFadeMax;
    s_pendingCue = music_logic::kCueNone;
    s_pausePending = false;
}

inline void idleZero() {
    stopRamp();
    s_gain = 0;
    s_target = 0;
}

inline void clearPending() {
    s_pendingCue = music_logic::kCueNone;
    s_pausePending = false;
}

// A fresh stream must not inherit the previous cue's held-note base volumes:
// on the first tick those channels would otherwise scale the old cue's volume
// into the new fade-in even though the stream has not sent a note yet.
inline __attribute__((noinline)) void resetBase() {
    for (uint8_t n = 0; n < 4; ++n)
        s_base[n] = 0;
}

// Foreground half of the transition: once the ramp reaches its target, load
// the pending cue and/or freeze the backend. Never runs mid cart/OLED
// transaction: music::service() is a main-loop entry point only.
inline __attribute__((always_inline)) void service() {
    if (s_gain != s_target)
        return;
    if (s_gain == 0) {
        if (s_pendingCue == music_logic::kCueNone && !s_pausePending)
            return;
        if (s_pendingCue != music_logic::kCueNone) {
            const uint8_t cue = s_pendingCue;
            s_pendingCue = music_logic::kCueNone;
            if (backend::play(cue)) {
                s_backendCue = cue;
                resetBase();
            }
        }
        if (s_pausePending) {
            s_pausePending = false;
            backend::pause();
            idleZero();
            return;
        }
        startRamp(music_logic::kFadeMax);
        return;
    }
    // Unity reached: no scaling is needed until the next transition.
    stopRamp();
}

} // namespace fade

inline bool enabled() {
    return s_enabled;
}

inline bool muted() {
    return s_muted;
}

inline void applyBackendPause() {
    if (s_paused || s_muted)
        backend::pause();
    else
        backend::resume();
}

inline void refreshMute() {
    const bool wasMuted = s_muted;
    s_muted = !arduboy.audio.enabled();
    if (s_muted && !wasMuted)
        fade::settle();
    applyBackendPause();
}

inline uint8_t currentCue() {
    return s_flight.currentCue;
}

inline bool active() {
    return backend::active();
}

inline bool error() {
    return backend::error();
}

inline void begin() {
    const uint8_t stored = EEPROM.read(kSettingAddress);
    s_enabled = stored != kSettingOff;
    music_logic::resetFlight(s_flight, music_logic::kHarbor);
    s_paused = false;
    s_muted = !arduboy.audio.enabled();
    s_backendCue = music_logic::kCueNone;
    fade::abandon();
    backend::begin();
    if (!s_enabled)
        backend::stop();
}

inline void setCue(uint8_t cue) {
    // The requested identity is immediate; the backend stream follows the
    // fade. Direct callers (device suites) and the policy both see it at once.
    s_flight.currentCue = cue;
    if (!s_enabled)
        return;
    // Cue policy can select the new identity before reaching this boundary.
    // A cue that is already loaded keeps its position (including under pause);
    // cancelling a pending switch fades back in instead of cutting.
    if (cue == s_backendCue && backend::active()) {
        // Cancel only a pending cue switch; a pause fade keeps running.
        fade::s_pendingCue = music_logic::kCueNone;
        if (!s_paused && !s_muted && fade::s_gain != music_logic::kFadeMax)
            fade::startRamp(music_logic::kFadeMax);
        return;
    }
    // Silent or first-load states load the requested stream immediately. The
    // next resume fades in from silence; a frozen switch never restarts the
    // old stream.
    if (!backend::active() || s_paused || s_muted) {
        fade::clearPending();
        fade::idleZero();
        if (backend::play(cue)) {
            s_backendCue = cue;
            fade::resetBase();
        }
        applyBackendPause();
        if (!s_paused && !s_muted) {
            fade::s_gain = 0;
            fade::startRamp(music_logic::kFadeMax);
        }
        return;
    }
    // Playing: fade the current stream out, then load and fade in the new one.
    fade::s_pendingCue = cue;
    fade::s_pausePending = false;
    fade::startRamp(0);
}

inline void pause() {
    if (s_paused)
        return;
    s_paused = true;
    if (!s_enabled || !backend::active() || s_muted)
        return;
    fade::s_pausePending = true;
    fade::startRamp(0);
}

inline void resume() {
    if (!s_paused)
        return;
    s_paused = false;
    fade::s_pausePending = false;
    if (!s_enabled || s_muted || !backend::active())
        return;
    backend::resume();
    if (fade::s_pendingCue != music_logic::kCueNone)
        fade::startRamp(0);
    else if (fade::s_gain < music_logic::kFadeMax)
        fade::startRamp(music_logic::kFadeMax);
}

inline bool paused() {
    return s_paused;
}

inline music_logic::PauseToken pauseForTransaction() {
    const music_logic::PauseToken token = {s_paused};
    if (!s_paused) {
        s_paused = true;
        // A blocking save cannot wait for a fade. Drop a pending switch and
        // restore true volumes before the synchronous cart transaction; the
        // resume path replays the policy cue when it differs.
        fade::s_pendingCue = music_logic::kCueNone;
        fade::s_pausePending = false;
        // Keep the envelope where it is; resume() ramps back to unity from
        // the preserved base volumes after the transaction.
        fade::stopRamp();
        backend::pause();
    }
    return token;
}

inline void resumeAfterTransaction(music_logic::PauseToken token) {
    if (token.wasPaused)
        return;
    resume();
    if (s_backendCue != s_flight.currentCue)
        setCue(s_flight.currentCue);
}

inline void service() {
    backend::service();
    fade::service();
}

inline uint8_t readRegion(int16_t pixelX, int16_t pixelY) {
    const uint16_t tileX = static_cast<uint16_t>(pixelX >> 3);
    const uint16_t tileY = static_cast<uint16_t>(pixelY >> 3);
    const uint16_t index = static_cast<uint16_t>(tileY * music_regions::kTilesX + tileX);
    cartService();
    FX::seekData(static_cast<uint24_t>(musicRegions + index));
    const uint8_t region = FX::readPendingUInt8();
    FX::readEnd();
    return region;
}

inline void overrideCue(uint8_t cue) {
    music_logic::setCurrent(s_flight, cue);
    s_flight.evaluateTicks = 0;
    setCue(cue);
}

inline void stop() {
    fade::abandon();
    s_backendCue = music_logic::kCueNone;
    backend::stop();
}

inline void beginFlight(const game::State &) {
    s_flight.evaluateTicks = 0;
}

inline void updateFlight(const game::State &state) {
    if (music_logic::evaluateDue(s_flight))
        s_flight.region = readRegion(state.x, state.y);
    const uint8_t before = s_flight.currentCue;
    music_logic::updateFlight(s_flight, s_flight.region);
    if (s_flight.currentCue != before)
        setCue(s_flight.currentCue);
}

inline void toggleEnabled() {
    s_enabled = !s_enabled;
    EEPROM.write(kSettingAddress, s_enabled ? kSettingOn : kSettingOff);
    fade::abandon();
    if (!s_enabled) {
        s_backendCue = music_logic::kCueNone;
        backend::stop();
        return;
    }
    s_muted = !arduboy.audio.enabled();
    if (backend::play(s_flight.currentCue)) {
        s_backendCue = s_flight.currentCue;
        fade::resetBase();
        applyBackendPause();
        if (!s_paused && !s_muted) {
            fade::s_gain = 0;
            fade::startRamp(music_logic::kFadeMax);
        } else {
            fade::idleZero();
        }
    }
}

}   // namespace music

