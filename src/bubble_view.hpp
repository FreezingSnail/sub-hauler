#pragma once

#include "pilot_view.hpp"

// A short animated wake, projected from the boat without any particle state.
// Motion is derived from flight ticks, so it freezes with the rest of flight.
namespace bubble_view {

constexpr int16_t kMinSpeedQ8 = 2 * game::kQ8One;

inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    if (state.docked >= 0)
        return;
    if (state.vxQ8 < kMinSpeedQ8 && state.vxQ8 > -kMinSpeedQ8)
        return;

    const int16_t sx = pilot_view::screenX(state, camera);
    const int16_t sy = pilot_view::screenY(state, camera);
    const int8_t direction = state.vxQ8 > 0 ? -1 : 1;
    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t age = static_cast<uint8_t>(((state.ticks >> 2) + i * 5) & 15);
        const int16_t x = static_cast<int16_t>(sx + direction * (game::kHullHalfW + 3 + age));
        const int16_t y = static_cast<int16_t>(sy - age / 4 + (i == 1 ? 2 : 0));
        cave::setPixel(buffer, x, y);
        if (age >= 8)
            cave::setPixel(buffer, static_cast<int16_t>(x + 1), y);
    }
}

} // namespace bubble_view
