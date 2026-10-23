#pragma once

#include <stdint.h>

#include "cave.hpp"
#include "world.hpp"

// Frame composition for the M1 cave view.
//
// The frame is composed into a page-major 1-bit buffer with the same layout as
// the display buffer: byte (y / 8) * 128 + x, bit (y & 7). Keeping this pure
// C++ lets the host tests render a window and inspect exact pixels; the device
// passes Arduboy2Base::sBuffer and then presents it once.
namespace cave {

inline uint8_t atlasByte(uint8_t mask, uint8_t column) {
#if defined(__AVR__)
    return pgm_read_byte(&CAVE_ATLAS[mask][column]);
#else
    return CAVE_ATLAS[mask][column];
#endif
}

inline void setPixel(uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= VIEW_WIDTH || y >= VIEW_HEIGHT)
        return;
    buffer[(y >> 3) * VIEW_WIDTH + x] |= static_cast<uint8_t>(1u << (y & 7));
}

inline void clearPixel(uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= VIEW_WIDTH || y >= VIEW_HEIGHT)
        return;
    buffer[(y >> 3) * VIEW_WIDTH + x] &= static_cast<uint8_t>(~(1u << (y & 7)));
}

inline void fillRect(uint8_t *buffer, int16_t x, int16_t y, uint8_t width, uint8_t height, bool color) {
    if (width == 0 || height == 0)
        return;
    int16_t right = static_cast<int16_t>(x + width);
    int16_t bottom = static_cast<int16_t>(y + height);
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (right > VIEW_WIDTH)
        right = VIEW_WIDTH;
    if (bottom > VIEW_HEIGHT)
        bottom = VIEW_HEIGHT;
    for (int16_t py = y; py < bottom; ++py) {
        for (int16_t px = x; px < right; ++px) {
            if (color)
                setPixel(buffer, px, py);
            else
                clearPixel(buffer, px, py);
        }
    }
}

inline void drawTile(uint8_t *buffer, uint8_t column, uint8_t row, uint8_t mask) {
    uint8_t *page = &buffer[row * VIEW_WIDTH + column * 8];
    for (uint8_t i = 0; i < 8; ++i)
        page[i] = atlasByte(mask, i);
}

// Interior rock is a flat white mass; one deterministic dark speck per tile
// breaks up the fill without extending the atlas or adding animation.
inline void speckleTile(uint8_t *buffer, int16_t tileX, int16_t tileY, uint8_t column, uint8_t row) {
    const uint8_t x = static_cast<uint8_t>((tileX * 5 + tileY * 3) & 7);
    const uint8_t y = static_cast<uint8_t>((tileX * 3 + tileY * 7) & 7);
    clearPixel(buffer, static_cast<int16_t>(column * 8 + x), static_cast<int16_t>(row * 8 + y));
}

// A settlement gantry: a station body over a dock ring, centred on the
// generated settlement pixel. White structure on open black water.
inline void drawGantry(uint8_t *buffer, int16_t x, int16_t y) {
    // Station body with two lit windows (black panes).
    fillRect(buffer, static_cast<int16_t>(x - 9), static_cast<int16_t>(y - 14), 18, 6, true);
    fillRect(buffer, static_cast<int16_t>(x - 6), static_cast<int16_t>(y - 12), 3, 2, false);
    fillRect(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y - 12), 3, 2, false);

    // Beam and its two supports.
    fillRect(buffer, static_cast<int16_t>(x - GANTRY_HALF_WIDTH), static_cast<int16_t>(y - 8),
             GANTRY_HALF_WIDTH * 2, 2, true);
    fillRect(buffer, static_cast<int16_t>(x - GANTRY_HALF_WIDTH), static_cast<int16_t>(y - 6), 2, 4, true);
    fillRect(buffer, static_cast<int16_t>(x + GANTRY_HALF_WIDTH - 2), static_cast<int16_t>(y - 6), 2, 4, true);

    // Dock ring; its centre is the generated dock pixel.
    fillRect(buffer, static_cast<int16_t>(x - 7), static_cast<int16_t>(y - 2), 14, 5, true);
    fillRect(buffer, static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 1), 10, 3, false);

    // Approach clamps.
    fillRect(buffer, static_cast<int16_t>(x - 9), static_cast<int16_t>(y + 3), 3, 2, true);
    fillRect(buffer, static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 3), 3, 2, true);
}

inline void drawSettlements(uint8_t *buffer, const Camera &camera) {
    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        world::Settlement settlement;
        if (!world::settlementInfo(id, settlement))
            continue;
        const int16_t x = dockScreenX(camera, settlement.pixel_x);
        const int16_t y = dockScreenY(camera, settlement.pixel_y);
        if (!gantryVisible(x, y))
            continue;
        drawGantry(buffer, x, y);
    }
}

// Compose the visible 16x8 tile window and the settlement gantries. Reads only
// the supplied cache, so the caller owns cart access through refreshCache().
inline void composeFrame(const Camera &camera, const world::TileCache &cache, uint8_t *buffer) {
    for (uint16_t i = 0; i < static_cast<uint16_t>(VIEW_WIDTH) * VIEW_HEIGHT / 8; ++i)
        buffer[i] = 0;

    for (uint8_t row = 0; row < VIEW_TILES_Y; ++row) {
        const int16_t tileY = static_cast<int16_t>(camera.tileY + row);
        for (uint8_t column = 0; column < VIEW_TILES_X; ++column) {
            const int16_t tileX = static_cast<int16_t>(camera.tileX + column);

            bool solid = false;
            if (!world::cacheLookup(cache, tileX, tileY, solid) || !solid)
                continue;

            const uint8_t mask = openMask(cache, tileX, tileY);
            drawTile(buffer, column, row, mask);
            if (mask == 0)
                speckleTile(buffer, tileX, tileY, column, row);
        }
    }

    drawSettlements(buffer, camera);
}

}   // namespace cave
