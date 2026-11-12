#pragma once

#include <stdint.h>

#include "cave_render.hpp"

// Small, distant schools in authored clear-water pockets. They are scenery:
// no collision, cart reads, persistent state, or independent simulation.
namespace fish_view {

struct Site { int16_t x, y; };
#if defined(__AVR__)
const Site kSites[] PROGMEM = {
#else
constexpr Site kSites[] = {
#endif
    {1016, 232}, {336, 376}, {1048, 384}, {1008, 984}, // original schools
    {632, 144}, {688, 408}, {1392, 376}, {1768, 424},
    {2136, 408}, {1376, 592}, {2192, 568}, {1344, 712},
    {2200, 760}, {1496, 816}, {1056, 1144}, {1688, 1112},
    {1712, 208}, {344, 960}, {352, 1160}, {2160, 168},
};
constexpr uint8_t kSchoolCount = sizeof(kSites) / sizeof(kSites[0]);

// Centres are in world pixels. The entire motion envelope is clear in the
// baked terrain; the host test checks it after every terrain regeneration.
inline void schoolCentre(uint8_t index, int16_t &x, int16_t &y) {
#if defined(__AVR__)
    x = static_cast<int16_t>(pgm_read_word(&kSites[index].x));
    y = static_cast<int16_t>(pgm_read_word(&kSites[index].y));
#else
    x = kSites[index].x;
    y = kSites[index].y;
#endif
}

// A tiny bell and trailing tentacles. The bell expands on alternate beats.
inline void jelly(uint8_t *buffer, int16_t x, int16_t y, bool pulse) {
    cave::setPixel(buffer, x, y - 2);
    cave::setPixel(buffer, x - 1, y - 1);
    cave::setPixel(buffer, x + 1, y - 1);
    cave::setPixel(buffer, x - 2, y);
    cave::setPixel(buffer, x - 1, y);
    cave::setPixel(buffer, x, y);
    cave::setPixel(buffer, x + 1, y);
    cave::setPixel(buffer, x + 2, y);
    cave::setPixel(buffer, x - 1, y + (pulse ? 2 : 1));
    cave::setPixel(buffer, x + 1, y + (pulse ? 3 : 2));
}

// A triangular path avoids a teleport at the ends. A cycle takes 512 flight
// ticks; the tail alternates independently every eight ticks.
inline int8_t drift(uint16_t ticks) {
    const uint8_t phase = static_cast<uint8_t>((ticks >> 3) & 63);
    return static_cast<int8_t>(phase < 32 ? phase - 16 : 47 - phase);
}

inline void fish(uint8_t *buffer, int16_t x, int16_t y, bool right, bool tail) {
    // Five-pixel body and a one-pixel flicking tail, deliberately smaller than
    // the boat and the objective glyphs.
    cave::setPixel(buffer, x, y);
    cave::setPixel(buffer, x + 1, y);
    cave::setPixel(buffer, x + 2, y);
    cave::setPixel(buffer, x + 1, y - 1);
    cave::setPixel(buffer, x + 1, y + 1);
    cave::setPixel(buffer, right ? x - 1 : x + 3, static_cast<int16_t>(y + (tail ? 1 : -1)));
}

inline void draw(uint8_t *buffer, const cave::Camera &camera, uint16_t ticks) {
    const int16_t ox = cave::pixelOriginX(camera), oy = cave::pixelOriginY(camera);
    for (uint8_t i = 0; i < kSchoolCount; ++i) {
        int16_t x, y;
        schoolCentre(i, x, y);
        x = static_cast<int16_t>(x - ox);
        y = static_cast<int16_t>(y - oy);
        if (x < -32 || x >= cave::VIEW_WIDTH + 32 || y < -20 || y >= cave::VIEW_HEIGHT + 20)
            continue;
        const uint16_t phase = static_cast<uint16_t>(ticks + static_cast<uint16_t>(i) * 83);
        const int8_t motion = drift(phase);
        const bool tail = ((phase >> 3) & 1) != 0;
        if (i >= 4 && (i & 1)) {
            jelly(buffer, x, static_cast<int16_t>(y + motion / 2), (phase & 32) != 0);
            jelly(buffer, x + 12, static_cast<int16_t>(y - 5 + motion / 2), (phase & 32) == 0);
        } else {
            const bool right = ((phase >> 8) & 1) == 0;
            x = static_cast<int16_t>(x + motion);
            fish(buffer, x, y, right, tail);
            fish(buffer, x - 9, y - 5, right, !tail);
            fish(buffer, x + 9, y + 4, right, !tail);
        }
    }
}

} // namespace fish_view
