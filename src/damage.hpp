#pragma once

#include <stdint.h>

#include "features.hpp"
#include "game.hpp"

#if SH_FEATURE_HULL_DAMAGE

// Hull damage rules. Pure C++: no Arduino headers, heap, String, or float.
//
// Damage is one point per qualifying hit. A wall impact only counts when the
// blocked-axis speed is at or above game::kHardImpactSpeedQ8 (16 px/s, well
// under the 30 px/s base top speed but above a gentle nudge) and the cooldown
// has expired. The cooldown is a flight-tick counter, so holding thrust against
// rock cannot drain the hull every tick. Mines stay a one-point hit in
// hazards.hpp and do not use this cooldown: a mine is consumed exactly once.
//
// Zero hull is a valid state. applyDamage() clamps at zero and never changes
// movement, so the boat stays controllable until M9 owns failure/polish.
namespace damage {

constexpr uint8_t kDamagePerHit = 1;
constexpr int16_t kWallDamageSpeedQ8 = game::kHardImpactSpeedQ8;

// Half a second at 60 Hz. A second hard impact is ignored until this expires.
constexpr uint8_t kWallDamageCooldownTicks = 24;

// The first 12 ticks after a hit draw the hull edge brackets. The cooldown is
// reused as the flash timer, so no second state byte is needed.
constexpr uint8_t kWallDamageFlashTicks = 12;

inline bool hardWallSpeed(int16_t speedQ8) {
    return speedQ8 >= kWallDamageSpeedQ8;
}

// Subtract at most the remaining hull. Repeated calls are safe and never wrap.
inline void applyDamage(game::State &state, uint8_t amount) {
    state.hull = amount >= state.hull ? 0 : static_cast<uint8_t>(state.hull - amount);
}

// Add up to the current hull capacity. An out-of-range hull (for example a
// save from an older tuning) is clamped down on the same call.
inline void repair(game::State &state, uint8_t amount) {
    const uint8_t capacity = game::hullCapacity(state);
    const uint16_t sum = static_cast<uint16_t>(state.hull) + amount;
    state.hull = sum > capacity ? capacity : static_cast<uint8_t>(sum);
}

inline void repairFull(game::State &state) {
    state.hull = game::hullCapacity(state);
    state.damageCooldown = 0;
}

// One flight tick of cooldown. Docked/chart/cargo frames never call this.
inline void tick(game::State &state) {
    if (state.damageCooldown > 0)
        --state.damageCooldown;
}

// Apply one point for a hard x/y wall impact if the cooldown has expired.
// Returns true only when damage was applied this tick.
inline bool resolveWallImpact(game::State &state) {
    if (state.damageCooldown != 0 || !game::hardWallImpact(state))
        return false;
    applyDamage(state, kDamagePerHit);
    state.damageCooldown = kWallDamageCooldownTicks;
    return true;
}

inline bool flashing(const game::State &state) {
    return state.damageCooldown > static_cast<uint8_t>(kWallDamageCooldownTicks - kWallDamageFlashTicks);
}

}   // namespace damage

#endif   // SH_FEATURE_HULL_DAMAGE
