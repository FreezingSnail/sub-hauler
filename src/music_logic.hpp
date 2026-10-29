#pragma once

#include <stdint.h>

#include "game.hpp"
#include "generated/music_regions.hpp"

// Pure music cue policy shared by the device wrapper and host tests. Region
// ownership comes from the authored room table (tile centres), not screen
// quadrants. The nearest authored room wins everywhere, including connecting
// passages; ties use the lower room index. Flight regions must persist for two
// seconds before they change the cue.
namespace music_logic {

enum Cue : uint8_t {
    kHarbor = 0,
    kKelp = 1,
    kSonar = 2,
    kAbyssal = 3,
    kTidepool = 4,
    kBubblejet = 5,
    kCueNone = 0xFF,
};

constexpr uint8_t kRegionCount = 4;
constexpr uint8_t kHysteresisTicks = 120;
constexpr uint8_t kEvaluateFrames = 30;

struct FlightState {
    uint8_t currentCue;
    uint8_t candidateCue;
    uint8_t candidateTicks;
    uint8_t evaluateTicks;
    uint8_t region;
};

#if !defined(__AVR__)
inline uint8_t nearestRegion(int16_t pixelX, int16_t pixelY) {
    int32_t bestDistance = 0x7FFFFFFF;
    uint8_t bestRegion = music_regions::kRegionTide;
    for (uint8_t room = 0; room < music_regions::kRoomCount; ++room) {
        const int16_t centerX = static_cast<int16_t>(music_regions::kCenterX[room] * 8u + 4u);
        const int16_t centerY = static_cast<int16_t>(music_regions::kCenterY[room] * 8u + 4u);
        const int32_t dx = static_cast<int32_t>(pixelX) - centerX;
        const int32_t dy = static_cast<int32_t>(pixelY) - centerY;
        const int32_t distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestRegion = music_regions::kRegion[room];
        }
    }
    return bestRegion;
}
#endif

inline uint8_t travelCue(uint8_t region) {
    if (region == music_regions::kRegionTide)
        return kSonar;
    if (region == music_regions::kRegionKelp)
        return kKelp;
    if (region == music_regions::kRegionWorks)
        return kBubblejet;
    return kAbyssal;
}

inline void resetFlight(FlightState &state, uint8_t currentCue = kHarbor) {
    state.currentCue = currentCue;
    state.candidateCue = currentCue;
    state.candidateTicks = 0;
    state.evaluateTicks = 0;
    state.region = music_regions::kRegionTide;
}

// The device samples the baked plane only when this returns true.
inline bool evaluateDue(FlightState &state) {
    const bool due = state.evaluateTicks == 0;
    if (++state.evaluateTicks >= kEvaluateFrames)
        state.evaluateTicks = 0;
    return due;
}

// Call once per flight frame after a due sample. The cue changes only after
// 120 stable flight frames (two seconds at 60 Hz).
inline void updateFlight(FlightState &state, uint8_t sampledRegion) {
    state.region = sampledRegion;
    const uint8_t cue = travelCue(state.region);
    if (cue == state.currentCue) {
        state.candidateCue = cue;
        state.candidateTicks = 0;
        return;
    }
    if (cue != state.candidateCue) {
        state.candidateCue = cue;
        state.candidateTicks = 0;
        return;
    }
    // Reset on reaching the threshold; reachable state never saturates.
    if (++state.candidateTicks == kHysteresisTicks) {
        state.currentCue = cue;
        state.candidateTicks = 0;
    }
}


// A blocking transaction may be entered from a screen that already froze the
// cue. Preserve that prior pause: only an outer transaction owns the resume.
struct PauseToken {
    bool wasPaused;
};

inline PauseToken pauseForTransaction(bool &paused) {
    PauseToken token = {paused};
    paused = true;
    return token;
}

inline void resumeAfterTransaction(bool &paused, PauseToken token) {
    if (!token.wasPaused)
        paused = false;
}

inline void setCurrent(FlightState &state, uint8_t cue) {
    state.currentCue = cue;
    state.candidateCue = cue;
    state.candidateTicks = 0;
}

}   // namespace music_logic
