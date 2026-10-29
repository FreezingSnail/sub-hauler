#pragma once

#include <stdint.h>

#include "game.hpp"


// Fixed-size debris/wash pool. Pure C++: no heap, String, float, or cart read.
//
// Four particles share one world-space anchor captured at the impact. Deltas
// advance one pixel per flight tick and are clamped, so the pool can never
// leave a bounded 48x48 box around the hit. Spawn is deterministic: slot bit 0
// owns the horizontal side and bit 1 owns the vertical side, so the four slots
// start at the hull corners and drift outward with no flash-resident tables.
namespace particles {

static_assert(sizeof(game::Particle) == 3, "particle fits in three bytes");
static_assert(sizeof(game::ParticlePool) == 16, "four particles + one anchor");

constexpr int8_t kMaxOffset = 24;    // |dx|,|dy| bound in pixels
constexpr uint8_t kMaxLife = 14;     // longest slot lifetime in flight ticks
constexpr int8_t kSpawnHalfW = 13;   // hull half extents: start at the corners
constexpr int8_t kSpawnHalfH = 4;

inline int8_t clampOffset(int16_t value) {
    if (value > kMaxOffset)
        return kMaxOffset;
    if (value < -kMaxOffset)
        return static_cast<int8_t>(-kMaxOffset);
    return static_cast<int8_t>(value);
}

// Replace the whole pool at the hull position. A second hit restarts all four
// slots; there is no growth and no allocation.
inline void spawn(game::State &state) {
    game::ParticlePool &pool = state.particles;
    pool.x = state.x;
    pool.y = state.y;
    for (uint8_t i = 0; i < game::kParticleCount; ++i) {
        game::Particle &particle = pool.particle[i];
        particle.dx = static_cast<int8_t>((i & 1u) != 0 ? kSpawnHalfW : -kSpawnHalfW);
        particle.dy = static_cast<int8_t>((i & 2u) != 0 ? kSpawnHalfH : -kSpawnHalfH);
        particle.life = static_cast<uint8_t>(kMaxLife - i);
    }
}

// One flight tick: advance each live slot outward by one pixel on each axis and
// age it. Dead slots stay at zero life so render and bounds checks skip them.
inline void tick(game::State &state) {
    game::ParticlePool &pool = state.particles;
    for (uint8_t i = 0; i < game::kParticleCount; ++i) {
        game::Particle &particle = pool.particle[i];
        if (particle.life == 0)
            continue;
        const int8_t stepX = static_cast<int8_t>((i & 1u) != 0 ? 1 : -1);
        const int8_t stepY = static_cast<int8_t>((i & 2u) != 0 ? 1 : -1);
        particle.dx = clampOffset(static_cast<int16_t>(particle.dx) + stepX);
        particle.dy = clampOffset(static_cast<int16_t>(particle.dy) + stepY);
        --particle.life;
    }
}

inline bool anyAlive(const game::State &state) {
    for (uint8_t i = 0; i < game::kParticleCount; ++i) {
        if (state.particles.particle[i].life != 0)
            return true;
    }
    return false;
}

}   // namespace particles

