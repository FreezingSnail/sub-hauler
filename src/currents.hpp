#pragma once



#include <stdint.h>

#include "game.hpp"
#include "world.hpp"
#include "generated/exploration.hpp"

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
inline bool findAt(int16_t x, int16_t y, world::Current &out, uint8_t *idOut = nullptr) {
    const uint8_t count = world::currentCount();
    for (uint8_t id = 0; id < count; ++id) {
        if (!world::currentInfo(id, out))
            continue;
        if (contains(out, x, y)) {
            if (idOut)
                *idOut = id;
            return true;
        }
    }
    return false;
}

// A boat without the current thruster can drift with a tide and retreat
// downstream, but it cannot make headway against the flow. Clamping only the
// upstream velocity component (rather than adding a huge opposing force)
// leaves every perpendicular and downstream escape open, so a pocket cannot
// pin the boat when the zone edge is reached. Owning the thruster disables
// this upstream clamp but leaves the tide acceleration in place, so the
// player's stronger thrust can still fight the flow.
inline void blockUpstream(game::State &state, const world::Current &zone) {
    if (zone.ax_q8 > 0) {
        if (state.vxQ8 < 0)
            state.vxQ8 = 0;
    } else if (zone.ax_q8 < 0) {
        if (state.vxQ8 > 0)
            state.vxQ8 = 0;
    }
    if (zone.ay_q8 > 0) {
        if (state.vyQ8 < 0)
            state.vyQ8 = 0;
    } else if (zone.ay_q8 < 0) {
        if (state.vyQ8 > 0)
            state.vyQ8 = 0;
    }
}

// World-step hook. Outside every zone this leaves velocity untouched (zero
// acceleration), and a docked boat is never pushed. Every generated tide is
// gated: without the thruster it pushes downstream and blocks upstream motion;
// once the thruster is owned the tide still pushes, but upstream blocking is
// disabled so thrust can make headway against the flow.
inline void apply(game::State &state) {
    if (state.docked >= 0)
        return;
    world::Current zone;
    uint8_t id = 0;
    if (!findAt(state.x, state.y, zone, &id))
        return;
    if (!world::currentGated(id)) {
        applyZone(state, zone);
        return;
    }
    applyZone(state, zone);
    if ((state.tools & exploration_data::THRUSTER) == 0)
        blockUpstream(state, zone);
}

}   // namespace currents

