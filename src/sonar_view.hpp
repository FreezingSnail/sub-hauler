#pragma once

#include "features.hpp"

#if SH_FEATURE_SONAR

#include <stdint.h>

#include "cave_render.hpp"
#include "game.hpp"
#include "pilot_view.hpp"
#include "sonar.hpp"

// Pilot-view sonar ring. This is presentation only: the phase and radius come
// from game::State.sonarCooldown, and the centre from the same pilot mapping
// the HAULER uses, so no extra state and no cart reads are involved.
//
// One-page writes go through cave::setPixel, which clips to 128x64.
// view::frame draws the ring before the silhouette so the boat stays solid and
// readable on top of it.
namespace sonar_view {

constexpr int16_t kFrameWidth = cave::VIEW_WIDTH;
constexpr int16_t kFrameHeight = cave::VIEW_HEIGHT;

static_assert(kFrameWidth == 128 && kFrameHeight == 64, "sonar ring uses the display frame");

// Midpoint circle plotted as eight symmetric points. Duplicate octant joins
// are harmless: setPixel only sets an already-set bit. Outlined so LTO does not
// re-expand eight clipped writes at this single call site.
__attribute__((noinline)) inline void plotOctants(uint8_t *buffer, int16_t centerX, int16_t centerY, int16_t x, int16_t y) {
    cave::setPixel(buffer, static_cast<int16_t>(centerX + x), static_cast<int16_t>(centerY + y));
    cave::setPixel(buffer, static_cast<int16_t>(centerX - x), static_cast<int16_t>(centerY + y));
    cave::setPixel(buffer, static_cast<int16_t>(centerX + x), static_cast<int16_t>(centerY - y));
    cave::setPixel(buffer, static_cast<int16_t>(centerX - x), static_cast<int16_t>(centerY - y));
    cave::setPixel(buffer, static_cast<int16_t>(centerX + y), static_cast<int16_t>(centerY + x));
    cave::setPixel(buffer, static_cast<int16_t>(centerX - y), static_cast<int16_t>(centerY + x));
    cave::setPixel(buffer, static_cast<int16_t>(centerX + y), static_cast<int16_t>(centerY - x));
    cave::setPixel(buffer, static_cast<int16_t>(centerX - y), static_cast<int16_t>(centerY - x));
}

inline void drawRing(uint8_t *buffer, int16_t centerX, int16_t centerY, int16_t radius) {
    if (radius <= 0)
        return;

    int16_t x = radius;
    int16_t y = 0;
    int16_t error = static_cast<int16_t>(1 - radius);
    while (x >= y) {
        plotOctants(buffer, centerX, centerY, x, y);
        ++y;
        if (error < 0) {
            error = static_cast<int16_t>(error + 2 * y + 1);
        } else {
            --x;
            error = static_cast<int16_t>(error + 2 * (y - x) + 1);
        }
    }
}

// Ring centred on the sub's screen position, clipped at the frame edges.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const uint8_t radius = sonar::ringRadius(state);
    if (radius == 0)
        return;

    const int16_t centerX = pilot_view::screenX(state, camera);
    const int16_t centerY = pilot_view::screenY(state, camera);
    const int16_t reach = static_cast<int16_t>(radius);
    if (centerX + reach < 0 || centerY + reach < 0 || centerX - reach >= kFrameWidth || centerY - reach >= kFrameHeight)
        return;

    drawRing(buffer, centerX, centerY, reach);
}

}   // namespace sonar_view

#endif   // SH_FEATURE_SONAR
