#pragma once

#include <stdint.h>

#include "cave_render.hpp"
#include "game.hpp"

// Debris presentation for the pilot view. Screen-space only, clipped by
// cave::setPixel. No cart reads, no state mutation, no strings.
namespace particles_view {

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
    }
}

}   // namespace particles_view
