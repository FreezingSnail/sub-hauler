#pragma once

#include "features.hpp"

#if SH_FEATURE_MINES

#include <stdint.h>

#include "game.hpp"
#include "world.hpp"

// Fixed mine rules. The generated MINE_META table is the only source of
// positions: no runtime scatter, no per-mine SRAM array. State stores one byte
// of armed bits, so a removed mine stays removed for the run.
//
// This header is pure C++ (no Arduino headers, heap, String, or float). The
// collision test is a circle (mine) against the HAULER's 26x8 whole-pixel hull.
// A hit clears exactly one bit and records deterministic impact feedback; when
// SH_FEATURE_HULL_DAMAGE is on it also calls the single-point damage hook. M7
// damage may build on applyDamage() without changing these rules.
namespace hazards {

constexpr uint8_t kNoMine = 0xFF;
constexpr uint8_t kMineMaskAll = 0xFF;
constexpr uint8_t kMineDamage = 1;

// A mine hit reuses the wall-impact fields for one frame: axis 3 is distinct
// from the x/y wall axes, and the sketch clears it at the start of the next
// step so the pilot view can flash a deterministic spark.
constexpr uint8_t kMineImpactAxis = 3;
constexpr int16_t kMineImpactSpeedQ8 = 24 * game::kQ8One;

static_assert(world::MINE_COUNT <= 8, "mine-alive mask is one byte");

constexpr uint8_t allMines() {
    return static_cast<uint8_t>((1u << world::MINE_COUNT) - 1u);
}

inline bool alive(uint8_t mask, uint8_t id) {
    if (id >= world::MINE_COUNT)
        return false;
    return ((mask >> id) & 1u) != 0;
}

inline bool alive(const game::State &state, uint8_t id) {
    return alive(state.minesAlive, id);
}

// Restore every generated mine. Call from reset paths that own a run.
inline void reset(game::State &state) {
    state.minesAlive = allMines();
}

// Clear one armed bit. Returns true only on the transition from armed to hit,
// which makes "remove exactly once" directly testable.
inline bool remove(game::State &state, uint8_t id) {
    if (!alive(state, id))
        return false;
    state.minesAlive = static_cast<uint8_t>(state.minesAlive & static_cast<uint8_t>(~(1u << id)));
    return true;
}

// Circle versus the hull's half-open pixel rectangle. A mine touching a hull
// pixel is a hit (<= radius squared), so the test matches the drawn footprint.
inline bool hullOverlapsMine(const game::State &state, const world::Mine &mine) {
    const int16_t left = static_cast<int16_t>(state.x - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(state.y - game::kHullHalfH);
    const int16_t right = static_cast<int16_t>(state.x + game::kHullHalfW - 1);
    const int16_t bottom = static_cast<int16_t>(state.y + game::kHullHalfH - 1);

    const int16_t nearestX = mine.x < left ? left : (mine.x > right ? right : mine.x);
    const int16_t nearestY = mine.y < top ? top : (mine.y > bottom ? bottom : mine.y);
    const int32_t dx = static_cast<int32_t>(mine.x - nearestX);
    const int32_t dy = static_cast<int32_t>(mine.y - nearestY);
    const int32_t radius = world::MINE_RADIUS_PX;
    return dx * dx + dy * dy <= radius * radius;
}

#if SH_FEATURE_HULL_DAMAGE
// Clean one-point damage hook for the M7 damage bead. Hull is already floored
// at zero here, so the same function is safe for any future damage source.
inline void applyDamage(game::State &state, uint8_t amount) {
    state.hull = amount >= state.hull ? 0 : static_cast<uint8_t>(state.hull - amount);
}
#endif

// Remove the first armed mine overlapping the hull and record one hit.
// Returns its id or kNoMine. Only one mine is consumed per step, so overlapping
// metadata cannot double-damage the boat.
inline uint8_t collide(game::State &state) {
    world::Mine mine;
    const uint8_t count = world::mineCount();
    for (uint8_t id = 0; id < count; ++id) {
        if (!alive(state, id))
            continue;
        if (!world::mineInfo(id, mine))
            continue;
        if (!hullOverlapsMine(state, mine))
            continue;

        remove(state, id);
        state.impactSpeedQ8 = kMineImpactSpeedQ8;
        state.impactAxis = kMineImpactAxis;
        if (state.impacts < 255)
            ++state.impacts;
#if SH_FEATURE_HULL_DAMAGE
        applyDamage(state, kMineDamage);
#endif
        return id;
    }
    return kNoMine;
}

// One-frame feedback clear. Called before physics on the next world step so a
// hit flash cannot leak into later frames or be confused with a wall impact.
inline void clearImpact(game::State &state) {
    if (state.impactAxis == kMineImpactAxis) {
        state.impactAxis = 0;
        state.impactSpeedQ8 = 0;
    }
}

}   // namespace hazards

#endif   // SH_FEATURE_MINES
