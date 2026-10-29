#pragma once



#include <stdint.h>

#include "game.hpp"
#include "world.hpp"

// Integer current zones from generated metadata. Each zone is a pixel
// rectangle plus a signed Q8.8 px/s^2 acceleration on one axis. Applying a
// current only changes velocity before the same axis-separated collision walk
// that handles thrust, so a current can never move the hull through rock.
//
// Pure C++: no Arduino headers, heap, String, or float. There is no current
// state: the zone is a pure function of the whole-pixel boat position, and
// rendering derives flow marks from game::State.ticks.
namespace currents {

// Drift is capped independently of thrust speed. A boat that leaves a zone
// returns to its cargo-limited top speed through the normal drag clamp.
constexpr int16_t kMaxDriftQ8 = 32 * game::kQ8One;
constexpr int16_t kAccelDivisor = game::kStepHz;

inline bool contains(const world::Current &zone, int16_t x, int16_t y) {
    return x >= zone.x && y >= zone.y && x < static_cast<int16_t>(zone.x + zone.w) && y < static_cast<int16_t>(zone.y + zone.h);
}

// Apply one step of a zone's acceleration. Split out so tests can exercise a
// synthetic zone without depending on the generated table.
inline void applyZone(game::State &state, const world::Current &zone) {
    if (zone.ax_q8 != 0) {
        const int16_t delta = static_cast<int16_t>(zone.ax_q8 / kAccelDivisor);
        state.vxQ8 = game::clampI16(static_cast<int16_t>(state.vxQ8 + delta), -kMaxDriftQ8, kMaxDriftQ8);
    }
    if (zone.ay_q8 != 0) {
        const int16_t delta = static_cast<int16_t>(zone.ay_q8 / kAccelDivisor);
        state.vyQ8 = game::clampI16(static_cast<int16_t>(state.vyQ8 + delta), -kMaxDriftQ8, kMaxDriftQ8);
    }
}

// Find the first zone containing the whole-pixel boat position. The generated
// zones do not overlap; first-match keeps the rule deterministic even if a
// future generator creates an overlap.
inline bool findAt(int16_t x, int16_t y, world::Current &out) {
    const uint8_t count = world::currentCount();
    for (uint8_t id = 0; id < count; ++id) {
        if (!world::currentInfo(id, out))
            continue;
        if (contains(out, x, y))
            return true;
    }
    return false;
}

// World-step hook. Outside every zone this leaves velocity untouched (zero
// acceleration), and a docked boat is never pushed.
inline void apply(game::State &state) {
    if (state.docked >= 0)
        return;
    world::Current zone;
    if (!findAt(state.x, state.y, zone))
        return;
    applyZone(state, zone);
}

}   // namespace currents

