#pragma once

#include "cave_render.hpp"

// Tiny animated extensions of baked scenery; all positions are world pixels.
namespace ambient_view {
struct Site { int16_t x, y; };
#if defined(__AVR__)
const Site kSites[] PROGMEM = {
#else
constexpr Site kSites[] = {
#endif
    {1312, 203}, {1372, 205}, // Kelp Cathedral: frond tips
    {404, 1208}, {1740, 1152}, // Deep Anchor and Jet Approach vents
};
constexpr uint8_t kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

inline void site(uint8_t i, int16_t &x, int16_t &y) {
#if defined(__AVR__)
    x = static_cast<int16_t>(pgm_read_word(&kSites[i].x));
    y = static_cast<int16_t>(pgm_read_word(&kSites[i].y));
#else
    x = kSites[i].x;
    y = kSites[i].y;
#endif
}

inline void draw(uint8_t *buffer, const cave::Camera &camera, uint16_t ticks) {
    const int16_t ox = cave::pixelOriginX(camera), oy = cave::pixelOriginY(camera);
    for (uint8_t i = 0; i < kSiteCount; ++i) {
        int16_t x, y;
        site(i, x, y);
        x = static_cast<int16_t>(x - ox);
        y = static_cast<int16_t>(y - oy);
        if (x < -3 || x >= cave::VIEW_WIDTH + 3 || y < -3 || y >= cave::VIEW_HEIGHT + 16)
            continue;
        if (i < 2) {
            // Fronds extend from existing growth; only their tips sway.
            for (uint8_t h = 1; h <= 5; ++h)
                cave::setPixel(buffer, x, static_cast<int16_t>(y - h));
            const int8_t sway = ((ticks >> 5) & 1) ? 1 : -1;
            cave::setPixel(buffer, static_cast<int16_t>(x + sway), static_cast<int16_t>(y - 6));
            cave::setPixel(buffer, static_cast<int16_t>(x + sway), static_cast<int16_t>(y - 7));
            cave::setPixel(buffer, static_cast<int16_t>(x - sway), static_cast<int16_t>(y - 4));
        } else {
            // Two bubbles rise from each vent and restart below the frame.
            const uint8_t phase = static_cast<uint8_t>((ticks >> 3) & 15);
            cave::setPixel(buffer, x, static_cast<int16_t>(y - phase));
            cave::setPixel(buffer, x, static_cast<int16_t>(y - ((phase + 8) & 15)));
        }
    }
}
} // namespace ambient_view
