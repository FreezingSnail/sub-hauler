#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_MUSIC

#include <ArduboyStreamFX.h>
#include <EEPROM.h>

#include "campaign.hpp"
#include "cart_service.hpp"
#include "common.hpp"
#include "generated/streamfx.hpp"
#include "music_logic.hpp"

// Device music boundary. One streamfx player, one Timer3 ISR, one Timer4 PWM
// carrier. The foreground only selects cues and calls streamfx::service().
//
// MUSIC ON/OFF is a one-byte user setting at EEPROM address 656, above the
// retired voyage allocation and outside the Arduboy2 system prefix (0..15).
// Voyage checkpoints live in the cart save partition, so Continue/recovery
// cannot roll this setting back. Writes happen only on the pause-menu toggle.
namespace music {

constexpr uint16_t kSettingAddress = 656;
constexpr uint8_t kSettingOff = 0;
constexpr uint8_t kSettingOn = 1;

static bool s_enabled = true;
static bool s_paused = false;
static music_logic::FlightState s_flight;

inline bool enabled() {
    return s_enabled;
}

inline void refreshMute() {
    streamfx::setMuted(!s_enabled || !arduboy.audio.enabled());
}

inline uint8_t currentCue() {
    return s_flight.currentCue;
}

inline void begin() {
    const uint8_t stored = EEPROM.read(kSettingAddress);
    s_enabled = stored != kSettingOff;
    music_logic::resetFlight(s_flight, music_logic::kHarbor);
    s_paused = false;
    streamfx::begin();
    streamfx::setMuted(!s_enabled || !arduboy.audio.enabled());
}

inline void setCue(uint8_t cue) {
    if (!s_enabled) {
        s_flight.currentCue = cue;
        return;
    }
    if (streamfx::start(game_audio::kStreams[cue])) {
        s_flight.currentCue = cue;
        if (s_paused)
            streamfx::pause();
    }
}

inline void pause() {
    if (!s_paused) {
        s_paused = true;
        streamfx::pause();
    }
}

inline void resume() {
    if (s_paused) {
        s_paused = false;
        streamfx::resume();
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
    streamfx::service();
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
    streamfx::stop();
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
        streamfx::stop();
    } else {
        streamfx::setMuted(!arduboy.audio.enabled());
        if (streamfx::start(game_audio::kStreams[s_flight.currentCue])) {
            if (s_paused)
                streamfx::pause();
        }
    }
}

}   // namespace music

#endif   // SH_FEATURE_MUSIC
