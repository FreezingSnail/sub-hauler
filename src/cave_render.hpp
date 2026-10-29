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
    if (x >= right || y >= bottom)
        return;
    const uint8_t firstPage = static_cast<uint8_t>(y >> 3);
    const uint8_t lastPage = static_cast<uint8_t>((bottom - 1) >> 3);
    for (uint8_t page = firstPage; page <= lastPage; ++page) {
        uint8_t mask = 0xFF;
        if (page == firstPage)
            mask &= static_cast<uint8_t>(0xFFu << (y & 7));
        if (page == lastPage)
            mask &= static_cast<uint8_t>(0xFFu >> (7 - ((bottom - 1) & 7)));
        uint8_t *dest = buffer + page * VIEW_WIDTH + x;
        for (int16_t px = x; px < right; ++px, ++dest) {
            if (color)
                *dest |= mask;
            else
                *dest &= static_cast<uint8_t>(~mask);
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
    fillRect(buffer, static_cast<int16_t>(x - GANTRY_HALF_WIDTH), static_cast<int16_t>(y - 8), GANTRY_HALF_WIDTH * 2, 2, true);
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
        // id is bounded by the generated count; SETTLEMENT_META has that exact
        // extent, so the accessor cannot fail. The host cave test still draws
        // every generated settlement and checks the pixels.
        (void)world::settlementInfo(id, settlement);
        const int16_t x = dockScreenX(camera, settlement.pixel_x);
        const int16_t y = dockScreenY(camera, settlement.pixel_y);
        if (!gantryVisible(x, y))
            continue;
        drawGantry(buffer, x, y);
    }
}

// One 18-bit row from the densely packed cache. Row alignment repeats every
// four rows (0/2/4/6 bits); three bytes always cover it, including the last
// row at bytes 20..22. No temporary unpacked cache or cart reads.
inline uint32_t cacheRowBits(const world::TileCache &cache, uint8_t row) {
    const uint8_t bit = row * world::CACHE_TILES_X;
    const uint8_t *src = cache.bits + (bit >> 3);
    return (static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16)) >> (bit & 7);
}

// Compose the visible 16x8 tile window and the settlement gantries. Reads only
// the supplied cache, so the caller owns cart access through refreshCache().
// This is the tile-grid host reference for cache/atlas behavior; the device
// terrain path is streamFrame(), which owns arbitrary pixel origins.
inline void composeFrame(const Camera &camera, const world::TileCache &cache, uint8_t *buffer) {
    for (uint16_t i = 0; i < static_cast<uint16_t>(VIEW_WIDTH) * VIEW_HEIGHT / 8; ++i)
        buffer[i] = 0;

    if (cache.valid && cache.originTileX == cacheOriginTileX(camera) && cache.originTileY == cacheOriginTileY(camera)) {
        // The normal view has a complete neighbour margin. Decode each row
        // once, then slide its bits across the screen instead of making up to
        // 640 checked world-coordinate lookups per frame.
        uint32_t north = cacheRowBits(cache, 0);
        uint32_t middle = cacheRowBits(cache, 1);
        for (uint8_t row = 0; row < VIEW_TILES_Y; ++row) {
            const uint32_t south = cacheRowBits(cache, row + 2);
            uint32_t n = north >> 1, m = middle, s = south >> 1;
            for (uint8_t column = 0; column < VIEW_TILES_X; ++column) {
                if (m & 2) {
                    uint8_t mask = 0;
                    if (!(n & 1))
                        mask |= OPEN_NORTH;
                    if (!(s & 1))
                        mask |= OPEN_SOUTH;
                    if (!(m & 1))
                        mask |= OPEN_WEST;
                    if (!(m & 4))
                        mask |= OPEN_EAST;
                    drawTile(buffer, column, row, mask);
                    if (!mask)
                        speckleTile(buffer, camera.tileX + column, camera.tileY + row, column, row);
                }
                n >>= 1;
                m >>= 1;
                s >>= 1;
            }
            north = middle;
            middle = south;
        }
    } else {
        for (uint8_t row = 0; row < VIEW_TILES_Y; ++row) {
            // Keep cache-miss and partial-window behavior for other callers.
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
    }

    drawSettlements(buffer, camera);
}

constexpr uint16_t TERRAIN_ROW_BYTES = world::PIXELS_X;
constexpr uint32_t TERRAIN_BYTES = static_cast<uint32_t>(world::PIXELS_X) * world::PIXELS_Y / 8;

// Vertical page combination for a sub-page camera origin. For remainder s in
// 1..7, factor is 2^(8-s); highByte(a * factor) is a >> s and
// lowByte(b * factor) is b << (8-s). AVR's 8x8 hardware multiply computes both
// products without the repeated variable shifts. The host test checks every
// byte pair for all seven factors against the direct shift expression.
inline uint8_t combineShiftedBytes(uint8_t currentPage, uint8_t nextPage, uint8_t factor) {
#if defined(__AVR__)
    // Two 8x8 hardware multiplies. r1 is restored to zero before returning.
    uint8_t result;
    asm volatile("mul %1, %3\n\t"
                 "mov %0, r1\n\t"
                 "mul %2, %3\n\t"
                 "or %0, r0\n\t"
                 "eor r1, r1\n\t"
                 : "=&a"(result)
                 : "a"(currentPage), "a"(nextPage), "a"(factor)
                 : "r0", "cc");
    return result;
#else
    const uint16_t lower = static_cast<uint16_t>(currentPage) * factor;
    const uint16_t upper = static_cast<uint16_t>(nextPage) * factor;
    return static_cast<uint8_t>((lower >> 8) | static_cast<uint8_t>(upper));
#endif
}

// Stream the baked terrain at the camera's exact pixel origin. The panorama is
// 2560 columns x 160 eight-pixel source pages: one source byte is one screen
// column and bit b is the row b within that page. A horizontal pixel origin is
// therefore just a byte offset, so every X position still needs one contiguous
// 128-byte read per output page. A vertical sub-page offset needs the next
// source page and a per-byte bit shift.
//
// The cascade keeps the next source page in the next framebuffer page, which
// is its own destination one step later. That avoids a second 128-byte buffer:
// read page p, read page p+1 into dest p+1, combine p from p/p+1, and move on.
// The final page has no spare page, so it reads the last source page in four
// 32-byte bursts into a 32-byte stack scratch. There is no whole-frame temporary
// and no static buffer. Reader owns cart access; all reads finish before OLED
// presentation.
template <typename Reader> inline void streamFrame(const Camera &camera, Reader &reader, uint8_t *buffer) {
    const uint32_t firstLine = static_cast<uint32_t>(camera.tileY) * TERRAIN_ROW_BYTES + pixelOriginX(camera);

    if (camera.subY == 0) {
        uint32_t offset = firstLine;
        for (uint8_t page = 0; page < VIEW_TILES_Y; ++page) {
            reader.terrainBytes(offset, buffer + page * VIEW_WIDTH, VIEW_WIDTH);
            offset += TERRAIN_ROW_BYTES;
        }
        drawSettlements(buffer, camera);
        return;
    }

    // Seed every page with its lower source page. Each later read overwrites
    // the page above and leaves it as the next page's lower row.
    uint32_t offset = firstLine;
    for (uint8_t page = 0; page < VIEW_TILES_Y; ++page) {
        reader.terrainBytes(offset, buffer + page * VIEW_WIDTH, VIEW_WIDTH);
        offset += TERRAIN_ROW_BYTES;
    }

    const uint8_t shiftY = camera.subY;
    const uint8_t factor = static_cast<uint8_t>(1u << (8 - shiftY));
    for (uint8_t page = 0; page + 1 < VIEW_TILES_Y; ++page) {
        const uint8_t *lower = buffer + page * VIEW_WIDTH;
        const uint8_t *upper = buffer + (page + 1) * VIEW_WIDTH;
        uint8_t *dest = buffer + page * VIEW_WIDTH;
        for (uint8_t i = 0; i < VIEW_WIDTH; ++i)
            dest[i] = combineShiftedBytes(lower[i], upper[i], factor);
    }

    // The last destination already holds source page q+7. The upper row q+8
    // is only needed here, so read it in bounded chunks instead of keeping a
    // second full page on the stack.
    constexpr uint8_t kChunk = 32;
    uint8_t scratch[kChunk];
    uint8_t *last = buffer + (VIEW_TILES_Y - 1) * VIEW_WIDTH;
    const uint32_t upperLine = firstLine + static_cast<uint32_t>(VIEW_TILES_Y) * TERRAIN_ROW_BYTES;
    for (uint8_t x = 0; x < VIEW_WIDTH; x = static_cast<uint8_t>(x + kChunk)) {
        reader.terrainBytes(upperLine + x, scratch, kChunk);
        for (uint8_t i = 0; i < kChunk; ++i)
            last[x + i] = combineShiftedBytes(last[x + i], scratch[i], factor);
    }
    drawSettlements(buffer, camera);
}

}   // namespace cave
