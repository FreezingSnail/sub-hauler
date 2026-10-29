#pragma once

#include <stdint.h>

#include "features.hpp"

#ifndef SH_MUSIC_STREAMING
#define SH_MUSIC_STREAMING 0
#endif

#if SH_FEATURE_MUSIC

#include <EEPROM.h>

#include "campaign.hpp"
#include "common.hpp"
#include "generated/music.hpp"
#include "music_logic.hpp"

#if SH_MUSIC_STREAMING
#include <ArduboyStreamFX.h>
#include "generated/streamfx.hpp"
#else
#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}
#endif

// Device music boundary. The production backend is the ATMlib2FX compact
// synth: one Timer3/oscillator ISR and one Timer4 PWM carrier, fed from the six
// authored C1 streams by a foreground FX refill. The optional
// SH_MUSIC_STREAMING build keeps the earlier ADPCM stream player as a
// comparison target; both expose the same cue/pause/mute contract here.
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

#if SH_MUSIC_STREAMING
inline void begin() {
    streamfx::begin();
}
inline bool play(uint8_t cue) {
    return streamfx::start(game_audio::kStreams[cue]);
}
inline void stop() {
    streamfx::stop();
}
inline void pause() {
    streamfx::pause();
}
inline void resume() {
    streamfx::resume();
}
inline void service() {
    streamfx::service();
}
inline bool active() {
    return streamfx::active();
}
inline bool error() {
    return streamfx::error();
}
inline bool underrun() {
    return streamfx::underrun();
}
inline uint16_t maxGapTicks() {
#if STREAMFX_DIAG
    return streamfx::diagMaxGapTicks;
#else
    return 0;
#endif
}
inline uint16_t maxServiceUs() {
#if STREAMFX_DIAG
    return streamfx::diagMaxServiceUs;
#else
    return 0;
#endif
}
inline uint16_t maxIsrTicks() {
#if STREAMFX_DIAG
    return streamfx::diagMaxIsrTicks;
#else
    return 0;
#endif
}
#else
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
inline bool underrun() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.underruns != 0;
#else
    return false;
#endif
}
inline uint16_t maxGapTicks() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.max_service_gap;
#else
    return 0;
#endif
}
inline uint16_t maxServiceUs() {
    return 0;
}
inline uint16_t maxIsrTicks() {
    return 0;
}
#endif

} // namespace backend

#if !SH_MUSIC_STREAMING
extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t *dst, uint8_t n);
#endif

static bool s_enabled = true;
static bool s_paused = false;
static bool s_muted = false;
static music_logic::FlightState s_flight;

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
    s_muted = !arduboy.audio.enabled();
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

inline bool underrun() {
    return backend::underrun();
}

inline uint16_t maxGapTicks() {
    return backend::maxGapTicks();
}

inline uint16_t maxServiceUs() {
    return backend::maxServiceUs();
}

inline uint16_t maxIsrTicks() {
    return backend::maxIsrTicks();
}

inline void begin() {
    const uint8_t stored = EEPROM.read(kSettingAddress);
    s_enabled = stored != kSettingOff;
    music_logic::resetFlight(s_flight, music_logic::kHarbor);
    s_paused = false;
    s_muted = !arduboy.audio.enabled();
    backend::begin();
    if (!s_enabled)
        backend::stop();
}

inline void setCue(uint8_t cue) {
    if (!s_enabled) {
        s_flight.currentCue = cue;
        return;
    }
    if (cue == s_flight.currentCue && backend::active()) {
        applyBackendPause();
        return;
    }
    if (backend::play(cue)) {
        s_flight.currentCue = cue;
        applyBackendPause();
    }
}

inline void pause() {
    if (!s_paused) {
        s_paused = true;
        backend::pause();
    }
}

inline void resume() {
    if (s_paused) {
        s_paused = false;
        applyBackendPause();
    }
}

inline bool paused() {
    return s_paused;
}

inline music_logic::PauseToken pauseForTransaction() {
    const music_logic::PauseToken token = {s_paused};
    if (!s_paused)
        pause();
    return token;
}

inline void resumeAfterTransaction(music_logic::PauseToken token) {
    if (!token.wasPaused)
        resume();
}

inline void service() {
    backend::service();
}

inline uint8_t readRegion(int16_t pixelX, int16_t pixelY) {
    const uint16_t tileX = static_cast<uint16_t>(pixelX >> 3);
    const uint16_t tileY = static_cast<uint16_t>(pixelY >> 3);
    const uint16_t index = static_cast<uint16_t>(tileY * music_regions::kTilesX + tileX);
    SH_CART_SERVICE();
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
    if (!s_enabled) {
        backend::stop();
    } else {
        s_muted = !arduboy.audio.enabled();
        if (backend::play(s_flight.currentCue))
            applyBackendPause();
    }
}

}   // namespace music

#endif   // SH_FEATURE_MUSIC
