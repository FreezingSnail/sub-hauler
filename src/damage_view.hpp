#pragma once

#include "features.hpp"

#if SH_FEATURE_HULL_DAMAGE || SH_FEATURE_PARTICLES

#include <stdint.h>

#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"
#include "pilot_view.hpp"
#include "world.hpp"
#if SH_FEATURE_HULL_DAMAGE
#include "damage.hpp"
#endif
#if SH_FEATURE_PARTICLES
#include "particles.hpp"
#endif

// Damage presentation for the pilot view. Screen-space only, clipped by
// cave::setPixel, and drawn after the HAULER so the feedback reads against the
// cave. No cart reads, no state mutation, no strings.
namespace damage_view {

#if SH_FEATURE_PARTICLES
// Impact specks. The pool anchor is a world pixel; the bounded offsets move the
// four slots outward, and the camera conversion matches the sub exactly.
inline void drawParticles(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const game::ParticlePool &pool = state.particles;
    const int16_t originX = static_cast<int16_t>(camera.tileX * world::TILE_PIXELS);
    const int16_t originY = static_cast<int16_t>(camera.tileY * world::TILE_PIXELS);
    for (uint8_t i = 0; i < game::kParticleCount; ++i) {
        const game::Particle &particle = pool.particle[i];
        if (particle.life == 0)
            continue;
        const int16_t x = static_cast<int16_t>(pool.x + particle.dx - originX);
        const int16_t y = static_cast<int16_t>(pool.y + particle.dy - originY);
        cave::setPixel(buffer, x, y);
    }
}
#endif

#if SH_FEATURE_HULL_DAMAGE
// Two-pixel corner brackets just outside the 26x8 hull. The cooldown counter
// keeps them on screen for kWallDamageFlashTicks after a damaging hit, which is
// the only pilot-HUD cue that hull changed (the station board owns the readout).
inline void drawFlash(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    if (!damage::flashing(state))
        return;

    const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);
    const int16_t right = static_cast<int16_t>(left + game::kHullW);
    const int16_t bottom = static_cast<int16_t>(top + game::kHullH);

    // Top-left, top-right, bottom-left, bottom-right: an L of three pixels at
    // each corner, one pixel outside the hull.
    cave::setPixel(buffer, static_cast<int16_t>(left - 1), static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, left, static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, static_cast<int16_t>(left - 1), top);

    cave::setPixel(buffer, right, static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, static_cast<int16_t>(right - 1), static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, right, top);

    cave::setPixel(buffer, static_cast<int16_t>(left - 1), bottom);
    cave::setPixel(buffer, left, bottom);
    cave::setPixel(buffer, static_cast<int16_t>(left - 1), static_cast<int16_t>(bottom - 1));

    cave::setPixel(buffer, right, bottom);
    cave::setPixel(buffer, static_cast<int16_t>(right - 1), bottom);
    cave::setPixel(buffer, right, static_cast<int16_t>(bottom - 1));
}
#endif

}   // namespace damage_view

#endif   // SH_FEATURE_HULL_DAMAGE || SH_FEATURE_PARTICLES
