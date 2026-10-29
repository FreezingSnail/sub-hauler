#pragma once

#include <stdint.h>

#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"
#include "pilot_view.hpp"
#include "world.hpp"
#include "currents.hpp"

// Current-flow presentation for the pilot view. Everything here is
// screen-space and clipped by cave::setPixel; no cart read and no extra state.
// Flow marks draw before the HAULER silhouette so the boat always stays
// readable.
namespace hazard_view {

// Three short dashes with a one-pixel arrow head. Marks drift with the flight
// tick counter so a zone reads as flow rather than a static decal. Outlined so
// the call site does not inline the clipped writes.
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

}   // namespace hazard_view
