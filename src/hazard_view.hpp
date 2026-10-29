#pragma once

#include "features.hpp"

#if SH_FEATURE_MINES || SH_FEATURE_CURRENTS

#include <stdint.h>

#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"
#include "pilot_view.hpp"
#include "world.hpp"
#if SH_FEATURE_MINES
#include "hazards.hpp"
#endif
#if SH_FEATURE_CURRENTS
#include "currents.hpp"
#endif

// Hazard presentation for the pilot view. Everything here is screen-space and
// clipped by cave::setPixel; no cart read and no extra state. Mines and flow
// marks draw before the HAULER silhouette so the boat always stays readable.
namespace hazard_view {

#if SH_FEATURE_MINES
// Outlined: the nine clipped writes cost more inlined than one call.
__attribute__((noinline)) inline void mineGlyph(uint8_t *buffer, int16_t x, int16_t y) {
    cave::setPixel(buffer, x, y);
    cave::setPixel(buffer, static_cast<int16_t>(x - 1), y);
    cave::setPixel(buffer, static_cast<int16_t>(x + 1), y);
    cave::setPixel(buffer, x, static_cast<int16_t>(y - 1));
    cave::setPixel(buffer, x, static_cast<int16_t>(y + 1));
    cave::setPixel(buffer, static_cast<int16_t>(x - 2), y);
    cave::setPixel(buffer, static_cast<int16_t>(x + 2), y);
    cave::setPixel(buffer, x, static_cast<int16_t>(y - 2));
    cave::setPixel(buffer, x, static_cast<int16_t>(y + 2));
}

inline void drawMines(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t originX = cave::pixelOriginX(camera);
    const int16_t originY = cave::pixelOriginY(camera);
    world::Mine mine;
    const uint8_t count = world::mineCount();
    for (uint8_t id = 0; id < count; ++id) {
        if (!hazards::alive(state, id))
            continue;
        // id is bounded by MINE_COUNT, the exact generated table extent.
        (void)world::mineInfo(id, mine);
        const int16_t x = static_cast<int16_t>(mine.x - originX);
        const int16_t y = static_cast<int16_t>(mine.y - originY);
        if (x < -2 || y < -2 || x > static_cast<int16_t>(cave::VIEW_WIDTH + 2) || y > static_cast<int16_t>(cave::VIEW_HEIGHT + 2))
            continue;
        mineGlyph(buffer, x, y);
    }
}

// One-frame spark at the hull corners. The step clears impactAxis before the
// next physics pass, so this cannot become a permanent decoration.
inline void drawImpact(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    if (state.impactAxis != hazards::kMineImpactAxis)
        return;
    const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);
    const int16_t right = static_cast<int16_t>(left + game::kHullW);
    const int16_t bottom = static_cast<int16_t>(top + game::kHullH);
    cave::setPixel(buffer, static_cast<int16_t>(left - 1), static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, right, static_cast<int16_t>(top - 1));
    cave::setPixel(buffer, static_cast<int16_t>(left - 1), bottom);
    cave::setPixel(buffer, right, bottom);
}
#endif

#if SH_FEATURE_CURRENTS
// Three short dashes with a one-pixel arrow head. Marks drift with the flight
// tick counter so a zone reads as flow rather than a static decal. Outlined for
// the same reason as mineGlyph.
__attribute__((noinline)) inline void flowMark(uint8_t *buffer, int16_t x, int16_t y, bool horizontal, int8_t direction) {
    for (int8_t i = 0; i < 3; ++i) {
        if (horizontal)
            cave::setPixel(buffer, static_cast<int16_t>(x + i * direction), y);
        else
            cave::setPixel(buffer, x, static_cast<int16_t>(y + i * direction));
    }
    const int16_t tipX = horizontal ? static_cast<int16_t>(x + 3 * direction) : x;
    const int16_t tipY = horizontal ? y : static_cast<int16_t>(y + 3 * direction);
    if (horizontal) {
        cave::setPixel(buffer, tipX, static_cast<int16_t>(tipY - 1));
        cave::setPixel(buffer, tipX, static_cast<int16_t>(tipY + 1));
    } else {
        cave::setPixel(buffer, static_cast<int16_t>(tipX - 1), tipY);
        cave::setPixel(buffer, static_cast<int16_t>(tipX + 1), tipY);
    }
}

inline void drawCurrents(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t originX = cave::pixelOriginX(camera);
    const int16_t originY = cave::pixelOriginY(camera);
    const int16_t shift = static_cast<int16_t>((state.ticks >> 3) & 3);

    world::Current zone;
    const uint8_t count = world::currentCount();
    for (uint8_t id = 0; id < count; ++id) {
        // id is bounded by CURRENT_COUNT, the exact generated table extent.
        (void)world::currentInfo(id, zone);
        const int16_t sx = static_cast<int16_t>(zone.x - originX);
        const int16_t sy = static_cast<int16_t>(zone.y - originY);
        if (sx >= cave::VIEW_WIDTH || sy >= cave::VIEW_HEIGHT || static_cast<int16_t>(sx + zone.w) <= 0 || static_cast<int16_t>(sy + zone.h) <= 0)
            continue;

        const bool horizontal = zone.ax_q8 != 0;
        const int8_t direction = static_cast<int8_t>((horizontal ? zone.ax_q8 : zone.ay_q8) > 0 ? 1 : -1);
        for (uint8_t mark = 0; mark < 3; ++mark) {
            if (horizontal)
                flowMark(buffer, static_cast<int16_t>(sx + 3 + mark * 9 + shift), static_cast<int16_t>(sy + 3 + mark * 4), true, direction);
            else
                flowMark(buffer, static_cast<int16_t>(sx + 3 + mark * 4), static_cast<int16_t>(sy + 3 + mark * 9 + shift), false, direction);
        }
    }
}
#endif

}   // namespace hazard_view

#endif   // SH_FEATURE_MINES || SH_FEATURE_CURRENTS
