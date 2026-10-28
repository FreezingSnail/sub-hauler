#pragma once

// Shared controls and UI for the live-synth / sampled-PWM comparison.
//
// Both sketches include this header. SH_AUDIO_COMPARE_BACKEND selects exactly
// one real playback path:
//   1 = ATMlib2FX compact live synth (the existing sh_* C1 streams)
//   2 = src/audio/sampled_player_avr.hpp (the existing audio_* ADPCM streams)
//
// The UI is deliberately identical. A single FX::display(CLEAR_BUFFER)
// presents each changed frame; every cart read belongs to the backend service,
// called before and after that OLED transaction. The sketch never repacks,
// slices or embeds the cart image: both builds are launched with the complete
// fxdata/fxdata.bin from this worktree.

#include <Arduino.h>
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define SH_AUDIO_DIAG_PACKED __attribute__((packed))
#else
#define SH_AUDIO_DIAG_PACKED
#endif

#ifndef SH_AUDIO_COMPARE_BACKEND
#error "define SH_AUDIO_COMPARE_BACKEND (1 live, 2 sampled) before including this header"
#endif

#ifndef SH_AUDIO_COMPARE_AUTODEMO
#define SH_AUDIO_COMPARE_AUTODEMO 0
#endif

#ifndef SH_AUDIO_COMPARE_SERIAL
#define SH_AUDIO_COMPARE_SERIAL 0
#endif

// Harness-visible SRAM record. Keep this layout in sync with
// tools/audio_compare/harness.cpp. The record is global so avr-nm can resolve
// its address; it is not part of the production game.
struct SH_AUDIO_DIAG_PACKED AudioCompareDiag {
    uint16_t magic;
    uint8_t backend;
    uint8_t cue;
    uint8_t status;   // 0 stopped, 1 playing, 2 paused
    uint8_t error;
    uint8_t underrun;
    uint16_t ticks;
    uint16_t max_gap;
    uint16_t starts;
    uint16_t pauses;
    uint16_t resumes;
    uint8_t draws;
};

static_assert(sizeof(AudioCompareDiag) == 18,
              "AudioCompareDiag is a byte-level transport; no padding");
static_assert(offsetof(AudioCompareDiag, ticks) == 7, "diag ticks offset");
static_assert(offsetof(AudioCompareDiag, max_gap) == 9, "diag max_gap offset");
static_assert(offsetof(AudioCompareDiag, starts) == 11, "diag starts offset");
static_assert(offsetof(AudioCompareDiag, pauses) == 13, "diag pauses offset");
static_assert(offsetof(AudioCompareDiag, resumes) == 15, "diag resumes offset");
static_assert(offsetof(AudioCompareDiag, draws) == 17, "diag draws offset");

volatile AudioCompareDiag audioCompareDiag = {
    0xAC01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char *const kCueNames[6] = {
    "HARBOR BUBBLES",
    "KELP DRIFT",
    "SONAR LULLABY",
    "ABYSSAL GLASS",
    "TIDEPOOL WALTZ",
    "BUBBLEJET RUN",
};

Arduboy2 arduboy;
static uint8_t s_cue = 0;
static bool s_dirty = true;
static uint16_t s_starts = 0;
static uint16_t s_pauses = 0;
static uint16_t s_resumes = 0;
static uint8_t s_draws = 0;

#if SH_AUDIO_COMPARE_BACKEND == 1
// ---------------------------------------------------------------------------
// LIVE: real ATMlib2FX compact decoder + oscillator, real Timer4 PWM.
// ---------------------------------------------------------------------------
extern "C" {
#include <atm_synth.h>
#include <atm_fx_compact.h>
}

#include "../../src/fxdata.h"
#include "../../src/generated/music.hpp"

namespace backend {
constexpr uint8_t kBackend = 1;
constexpr const char *kLabel = "LIVE ATM2FX";
constexpr const char *kKind = "C1 STREAM + SYNTH";

static const uint24_t kOffset[6] = {
    sh_harbor_bubbles, sh_kelp_drift, sh_sonar_lullaby,
    sh_abyssal_glass, sh_tidepool_waltz, sh_bubblejet_run,
};
static const uint32_t kLength[6] = {
    music_data::sh_harbor_bubbles_bytes,
    music_data::sh_kelp_drift_bytes,
    music_data::sh_sonar_lullaby_bytes,
    music_data::sh_abyssal_glass_bytes,
    music_data::sh_tidepool_waltz_bytes,
    music_data::sh_bubblejet_run_bytes,
};

inline void begin() { atm_synth_setup(); }
inline void play(uint8_t cue) {
    atm_fx_compact_play(static_cast<uint32_t>(kOffset[cue]), kLength[cue]);
}
inline void stop() { atm_fx_compact_stop(); }
inline void service() { atm_fx_compact_service(); }
inline void pause() { atm_fx_compact_pause(); }
inline void resume() { atm_fx_compact_resume(); }
inline bool active() { return atm_fx_compact_is_active() != 0; }
inline bool paused() { return atm_fx_compact_is_paused() != 0; }
inline bool error() { return atm_fx_compact_state.error != 0; }
inline bool underrun() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.underruns != 0;
#else
    return false;
#endif
}
inline uint16_t ticks() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.ticks;
#else
    return 0;
#endif
}
inline uint16_t maxGap() {
#if ATM_FX_COMPACT_DIAG
    return atm_fx_compact_state.max_service_gap;
#else
    return 0;
#endif
}
} // namespace backend

extern "C" void atm_fx_compact_fetch(uint32_t offset, uint8_t *dst, uint8_t n) {
    FX::readDataBytes(static_cast<uint24_t>(offset), dst, n);
}

#elif SH_AUDIO_COMPARE_BACKEND == 2
// ---------------------------------------------------------------------------
// SAMPLED: real IMA ADPCM ring + Timer3 8 kHz sample ISR + Timer4 PWM carrier.
// ---------------------------------------------------------------------------
#include "../../src/audio/sampled_player_avr.hpp"

namespace backend {
constexpr uint8_t kBackend = 2;
constexpr const char *kLabel = "SAMPLED PWM";
constexpr const char *kKind = "ADPCM 8KHZ + PWM";

inline void begin() { audio::begin(); }
inline void play(uint8_t cue) {
    audio::stop();
    audio::start(cue);
}
inline void stop() { audio::stop(); }
inline void service() { audio::service(); }
inline void pause() { audio::pause(); }
inline void resume() { audio::resume(); }
inline bool active() { return audio::g_player.active(); }
inline bool paused() { return audio::g_player.paused(); }
inline bool error() { return audio::g_player.error(); }
inline bool underrun() {
#if SH_AUDIO_DIAG
    return audio::diagUnderruns != 0 || audio::diagErrors != 0;
#else
    return false;
#endif
}
inline uint16_t ticks() {
#if SH_AUDIO_DIAG
    return audio::diagTicks;
#else
    return 0;
#endif
}
inline uint16_t maxGap() {
#if SH_AUDIO_DIAG
    return audio::diagMaxGapTicks;
#else
    return 0;
#endif
}
} // namespace backend

#else
#error "SH_AUDIO_COMPARE_BACKEND must be 1 (live) or 2 (sampled)"
#endif

static void syncDiag() {
    audioCompareDiag.magic = 0xAC01;
    audioCompareDiag.backend = backend::kBackend;
    audioCompareDiag.cue = s_cue;
    audioCompareDiag.status = backend::paused() ? 2 : (backend::active() ? 1 : 0);
    audioCompareDiag.error = backend::error() ? 1 : 0;
    audioCompareDiag.underrun = backend::underrun() ? 1 : 0;
    audioCompareDiag.ticks = backend::ticks();
    audioCompareDiag.max_gap = backend::maxGap();
    audioCompareDiag.starts = s_starts;
    audioCompareDiag.pauses = s_pauses;
    audioCompareDiag.resumes = s_resumes;
    audioCompareDiag.draws = s_draws;
}

static void serialState(const char *event) {
#if SH_AUDIO_COMPARE_SERIAL
    syncDiag();
    Serial.print(F("AC "));
    Serial.print(event);
    Serial.print(F(" backend="));
    Serial.print((int)backend::kBackend);
    Serial.print(F(" cue="));
    Serial.print((int)s_cue);
    Serial.print(F(" status="));
    Serial.print((int)audioCompareDiag.status);
    Serial.print(F(" ticks="));
    Serial.print(audioCompareDiag.ticks);
    Serial.print(F(" ur="));
    Serial.print((int)audioCompareDiag.underrun);
    Serial.print(F(" err="));
    Serial.print((int)audioCompareDiag.error);
    Serial.print(F(" gap="));
    Serial.println(audioCompareDiag.max_gap);
#else
    (void)event;
#endif
}

static void startCue(uint8_t cue) {
    s_cue = cue;
    backend::play(cue);
    ++s_starts;
    s_dirty = true;
    syncDiag();
}

static void drawUi() {
    arduboy.clear();
    arduboy.setCursor(0, 0);
    arduboy.print(F("AUDIO COMPARE"));
    arduboy.setCursor(0, 8);
    arduboy.print(backend::kLabel);
    arduboy.setCursor(0, 16);
    arduboy.print((int)s_cue + 1);
    arduboy.print(F("/6 "));
    arduboy.print(kCueNames[s_cue]);
    arduboy.setCursor(0, 24);
    if (backend::error()) {
        arduboy.print(F("ERROR"));
    } else if (backend::paused()) {
        arduboy.print(F("PAUSED"));
    } else {
        arduboy.print(F("PLAYING"));
    }
    arduboy.setCursor(0, 32);
    arduboy.print(F("UP/DN SELECT CUE"));
    arduboy.setCursor(0, 40);
    arduboy.print(F("A RESTART"));
    arduboy.setCursor(0, 48);
    arduboy.print(F("B PAUSE/RESUME"));
    arduboy.setCursor(0, 56);
    arduboy.print(backend::kKind);
    FX::display(CLEAR_BUFFER);
    ++s_draws;
}

void setup() {
#if SH_AUDIO_COMPARE_SERIAL
    Serial.begin(115200);
#endif
    arduboy.begin();
    arduboy.setFrameRate(60);
    // Force the speaker outputs on for this audition image. on() only sets the
    // DDR pins; it does not write the saved EEPROM mute preference.
    arduboy.audio.on();

    FX::begin(FX_DATA_PAGE);
    backend::begin();
    startCue(0);
    syncDiag();
    drawUi();
    syncDiag();
    serialState("BOOT");
}

void loop() {
    // Foreground service before the frame gate and while idle.
    backend::service();

    if (!arduboy.nextFrame()) {
        backend::service();
        syncDiag();
        return;
    }

    arduboy.pollButtons();
    bool changed = false;

    if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
        startCue(static_cast<uint8_t>((s_cue + 5) % 6));
        changed = true;
        serialState("PREV");
    }
    if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
        startCue(static_cast<uint8_t>((s_cue + 1) % 6));
        changed = true;
        serialState("NEXT");
    }
    if (arduboy.justPressed(A_BUTTON)) {
        startCue(s_cue);
        changed = true;
        serialState("RESTART");
    }
    if (arduboy.justPressed(B_BUTTON)) {
        if (backend::paused()) {
            backend::resume();
            ++s_resumes;
            serialState("RESUME");
        } else {
            backend::pause();
            ++s_pauses;
            serialState("PAUSE");
        }
        changed = true;
    }

#if SH_AUDIO_COMPARE_AUTODEMO
    // Compile-time evidence hook for serial-only Ardens runs. The normal
    // delivered images keep this at 0 and are driven by the D-pad/A/B.
    {
        static uint16_t timer = 0;
        static uint8_t phase = 0;
        if (++timer >= 60) {
            timer = 0;
            phase = static_cast<uint8_t>((phase + 1) % 24);
            const uint8_t cue = static_cast<uint8_t>(phase / 4);
            switch (phase % 4) {
            case 0:
                if (s_cue != cue)
                    startCue(cue);
                else
                    startCue(s_cue);
                serialState("AUTO-CUE");
                break;
            case 1:
                startCue(s_cue);
                serialState("AUTO-RESTART");
                break;
            case 2:
                backend::pause();
                ++s_pauses;
                serialState("AUTO-PAUSE");
                break;
            default:
                if (backend::paused()) {
                    backend::resume();
                    ++s_resumes;
                }
                serialState("AUTO-RESUME");
                break;
            }
            changed = true;
        }
    }
#endif

    syncDiag();

    // Service again before the only OLED transaction of this frame.
    backend::service();

    if (changed || s_dirty) {
        s_dirty = false;
        drawUi();
    }

    // And after presentation, so cart reads never overlap the OLED.
    backend::service();
    syncDiag();
}
