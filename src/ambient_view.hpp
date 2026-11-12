#pragma once

#include <stdint.h>

#include "cave_render.hpp"

// Tiny animated extensions of baked scenery; all positions are world pixels.
namespace ambient_view {
struct Site { int16_t x, y; };
#if defined(__AVR__)
const Site kSites[] PROGMEM = {
#else
constexpr Site kSites[] = {
#endif
    {1312, 203}, {1372, 205}, {1349, 216}, {1377, 216}, // Kelp Cathedral frond tips
    {404, 1208}, {1740, 1152},                           // Deep Anchor and Jet Approach vents
    {1344, 144}, {1056, 1144},                           // drifting motes in clear water
};
constexpr uint8_t kKelpCount = 4;
constexpr uint8_t kVentCount = 2;
constexpr uint8_t kDriftCount = 2;
constexpr uint8_t kSiteCount = sizeof(kSites) / sizeof(kSites[0]);
static_assert(kSiteCount == kKelpCount + kVentCount + kDriftCount, "ambient site table matches its kind counts");

inline void site(uint8_t i, int16_t &x, int16_t &y) {
#if defined(__AVR__)
    x = static_cast<int16_t>(pgm_read_word(&kSites[i].x));
    y = static_cast<int16_t>(pgm_read_word(&kSites[i].y));
#else
    x = kSites[i].x;
    y = kSites[i].y;
#endif
}

// Three sparse flecks on a slow diagonal. The offset envelope is x -7..+8 and
// y -5..+4 around the anchor; the host test checks that entire box is water.
inline void drawDrift(uint8_t *buffer, int16_t x, int16_t y, uint16_t ticks) {
    const uint8_t phase = static_cast<uint8_t>(ticks >> 5);
    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t step = static_cast<uint8_t>((phase + i * 3) & 7);
        const int16_t px = static_cast<int16_t>(x + (static_cast<int16_t>(i) * 6 - 6) + (step & 3) - 1);
        const int16_t py = static_cast<int16_t>(y + (static_cast<int16_t>(i) * 4 - 4) + (step >> 2) - 1);
        cave::setPixel(buffer, px, py);
    }
}

inline void draw(uint8_t *buffer, const cave::Camera &camera, uint16_t ticks) {
    const int16_t ox = cave::pixelOriginX(camera), oy = cave::pixelOriginY(camera);
    for (uint8_t i = 0; i < kSiteCount; ++i) {
        int16_t x, y;
        site(i, x, y);
        x = static_cast<int16_t>(x - ox);
        y = static_cast<int16_t>(y - oy);
        if (x < -8 || x >= cave::VIEW_WIDTH + 8 || y < -8 || y >= cave::VIEW_HEIGHT + 16)
            continue;
        if (i < kKelpCount) {
            // Fronds extend from existing growth; only their tips sway.
            for (uint8_t h = 1; h <= 5; ++h)
                cave::setPixel(buffer, x, static_cast<int16_t>(y - h));
            const int8_t sway = ((ticks >> 5) & 1) ? 1 : -1;
            cave::setPixel(buffer, static_cast<int16_t>(x + sway), static_cast<int16_t>(y - 6));
            cave::setPixel(buffer, static_cast<int16_t>(x + sway), static_cast<int16_t>(y - 7));
            cave::setPixel(buffer, static_cast<int16_t>(x - sway), static_cast<int16_t>(y - 4));
            // A slower lower leaf keeps the bed alive without a second loop.
            const int8_t curl = ((ticks >> 7) & 1) ? 1 : -1;
            cave::setPixel(buffer, static_cast<int16_t>(x + curl * 2), static_cast<int16_t>(y - 3));
        } else if (i < kKelpCount + kVentCount) {
            // Bubbles rise from each vent and sway as they climb.
            const uint8_t phase = static_cast<uint8_t>((ticks >> 3) & 15);
            const int8_t sway = ((ticks >> 4) & 1) ? 1 : -1;
            cave::setPixel(buffer, static_cast<int16_t>(x + sway), static_cast<int16_t>(y - phase));
            cave::setPixel(buffer, x, static_cast<int16_t>(y - ((phase + 12) & 15)));
            cave::setPixel(buffer, static_cast<int16_t>(x - sway), static_cast<int16_t>(y - ((phase + 8) & 15)));
        } else {
            drawDrift(buffer, x, y, ticks);
        }
    }
}
} // namespace ambient_view
