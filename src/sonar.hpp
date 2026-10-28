#pragma once

#include "features.hpp"

#if SH_FEATURE_SONAR

#include <stdint.h>

#include "chart.hpp"
#include "game.hpp"

// Sonar ping rules: a navigation tool that spends air, reveals a larger chart
// radius than coarse travel, and starts a cooldown. The pilot-view ring is
// derived from the cooldown alone, so no extra state is stored.
//
// Pure integer C++ with no Arduino headers, heap, or float. The upgrade level
// is a caller argument today (always 0 in the M6 sketch); M7's shop will pass
// the purchased level without changing this interface.
namespace sonar {

// Air cost falls by one unit per upgrade level, never below kMinAirCost.
// Reveal radius grows by one chart cell per level, capped so a single ping
// stays bounded work on the ATmega.
constexpr uint8_t kBaseAirCost = 6;
constexpr uint8_t kUpgradeStep = 1;
constexpr uint8_t kMinAirCost = 1;
constexpr uint8_t kBaseRevealRadius = 3;
constexpr uint8_t kMaxRevealRadius = 12;

// 120 flight ticks is 2 s of cooldown at the fixed 60 Hz step. The visible
// expanding ring is the first kRingTicks of that cooldown; it is a readout of
// the same counter, not a separate animation clock.
constexpr uint8_t kCooldownTicks = 120;
constexpr uint8_t kRingTicks = 24;
constexpr uint8_t kRingMinRadius = 4;
constexpr uint8_t kRingMaxRadius = 20;

enum Result : uint8_t {
    kFired,
    kNoAir,
    kCooldown,
    kDocked,
};

// Level 0 is the starting sonar. Every level removes one air unit, to a floor
// of one, and adds one reveal cell, to the cap.
inline uint8_t costForLevel(uint8_t upgradeLevel) {
    static_assert(kUpgradeStep == 1, "sonar discount is one unit per level");
    if (upgradeLevel >= kBaseAirCost)
        return kMinAirCost;
    return static_cast<uint8_t>(kBaseAirCost - upgradeLevel);
}

inline uint8_t radiusForLevel(uint8_t upgradeLevel) {
    static_assert(kUpgradeStep == 1, "sonar radius is one cell per level");
    if (upgradeLevel >= kMaxRevealRadius - kBaseRevealRadius)
        return kMaxRevealRadius;
    return static_cast<uint8_t>(kBaseRevealRadius + upgradeLevel);
}

// A ping is atomic: a rejected ping spends no air, starts no cooldown, and
// reveals nothing. A station screen never calls this, and a docked boat is
// rejected here as a second line of defence.
inline Result fire(game::State &state, chart::State &chartState, uint8_t upgradeLevel) {
#if SH_FEATURE_STATIONS
    if (state.docked >= 0)
        return kDocked;
#endif
    if (state.sonarCooldown > 0)
        return kCooldown;

    const uint8_t cost = costForLevel(upgradeLevel);
    if (state.air < static_cast<int16_t>(cost))
        return kNoAir;

    state.air = static_cast<int16_t>(state.air - cost);
    state.sonarCooldown = kCooldownTicks;
    uint8_t revealLevel = upgradeLevel;
#if SH_FEATURE_EXPLORATION
    if (state.tools & exploration_data::LENS)
        revealLevel = static_cast<uint8_t>(revealLevel + 2);
#endif
    chart::revealAtPixels(chartState, state.x, state.y, radiusForLevel(revealLevel));
    return kFired;
}

// The ring is visible only inside the first kRingTicks of a cooldown. A zero
// or out-of-range cooldown (for example a stale save) draws nothing.
inline bool ringVisible(const game::State &state) {
    // Unsigned wrap rejects both zero and out-of-range cooldowns too.
    return static_cast<uint8_t>(kCooldownTicks - state.sonarCooldown) < kRingTicks;
}

// Radius expands linearly from kRingMinRadius to kRingMaxRadius over the ring
// window. Returns 0 when the ring is not visible so callers can skip drawing.
inline uint8_t ringRadius(const game::State &state) {
    if (!ringVisible(state))
        return 0;
    const uint8_t elapsed = static_cast<uint8_t>(kCooldownTicks - state.sonarCooldown);
    const uint16_t span = kRingMaxRadius - kRingMinRadius;
    return static_cast<uint8_t>(kRingMinRadius + static_cast<uint16_t>(elapsed) * span / (kRingTicks - 1));
}

static_assert(kRingMinRadius < kRingMaxRadius, "ring expands");
static_assert(kCooldownTicks > kRingTicks, "ring finishes before the cooldown");

}   // namespace sonar

#endif   // SH_FEATURE_SONAR
