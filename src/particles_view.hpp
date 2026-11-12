#pragma once

#include <stdint.h>

#include "cave_render.hpp"
#include "game.hpp"
#include "particles.hpp"

// Debris presentation for the pilot view. Screen-space only, clipped by
// cave::setPixel. No cart reads, no state mutation, no strings.
namespace particles_view {

// Fresh impact specks show an outward spark head before fading to a single
// pixel. The pool already carries slot parity and lifetime, so the burst needs
// no extra state.
constexpr uint8_t kSparkLife = static_cast<uint8_t>(particles::kMaxLife - 3);

// Impact specks. The pool anchor is a world pixel; the bounded offsets move the
// four slots outward, and the camera conversion matches the sub exactly.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const game::ParticlePool &pool = state.particles;
    const int16_t originX = cave::pixelOriginX(camera);
    const int16_t originY = cave::pixelOriginY(camera);
    for (uint8_t i = 0; i < game::kParticleCount; ++i) {
        const game::Particle &particle = pool.particle[i];
        if (particle.life == 0)
            continue;
        const int16_t x = static_cast<int16_t>(pool.x + particle.dx - originX);
        const int16_t y = static_cast<int16_t>(pool.y + particle.dy - originY);
        cave::setPixel(buffer, x, y);
        if (particle.life >= kSparkLife) {
            const int8_t stepX = static_cast<int8_t>((i & 1u) != 0 ? 1 : -1);
            const int8_t stepY = static_cast<int8_t>((i & 2u) != 0 ? 1 : -1);
            cave::setPixel(buffer, static_cast<int16_t>(x + stepX), y);
            cave::setPixel(buffer, x, static_cast<int16_t>(y + stepY));
        }
    }
}

}   // namespace particles_view
