#pragma once

#include "cave_render.hpp"
#include "generated/parallax.hpp"

// Hand-authored area plates. Full native strokes move at half camera speed;
// cart rows skip empty space without discarding columns or applying a screen.
namespace parallax_view {
inline uint16_t areaIndex(const cave::Camera &camera) {
    const uint16_t x = static_cast<uint16_t>((cave::pixelOriginX(camera) + cave::VIEW_WIDTH / 2) >> 3);
    const uint16_t y = static_cast<uint16_t>((cave::pixelOriginY(camera) + cave::VIEW_HEIGHT / 2) >> 3);
    return y * world::TILES_X + x;
}

template <typename Reader> __attribute__((noinline)) inline void draw(
    uint8_t *buffer, const cave::Camera &camera, const world::TileCache &cache, Reader &reader) {
    if (!cache.valid || cache.originTileX != cave::cacheOriginTileX(camera) ||
        cache.originTileY != cave::cacheOriginTileY(camera)) return;
    const uint8_t area = reader.backgroundArea(areaIndex(camera));
    const uint8_t px = static_cast<uint8_t>(cave::pixelOriginX(camera) >> 1);
    const uint8_t py = static_cast<uint8_t>(cave::pixelOriginY(camera) >> 1) & 127;
    const uint8_t firstX = static_cast<uint8_t>(-(px & 7));
    const uint8_t blockCount = 16 + ((px & 7) != 0);
    const uint16_t firstRow = static_cast<uint16_t>(area) * parallax_data::kRowsPerArea +
                              (py & 7) * parallax_data::kRowsPerPhase;
    const uint8_t lowerMask = static_cast<uint8_t>(0xFFu >> camera.subY);
    const uint8_t subX = camera.subX;
    uint8_t masks[17];
    for (uint8_t page = 0; page < 8; ++page) {
        uint32_t lower = cave::cacheRowBits(cache, page + 1) >> 1;
        uint32_t upper = cave::cacheRowBits(cache, page + 2) >> 1;
        uint8_t anyWater = 0;
        for (uint8_t col = 0; col < 17; ++col) {
            masks[col] = static_cast<uint8_t>(~((lower & 1 ? lowerMask : 0) |
                                               (upper & 1 ? static_cast<uint8_t>(~lowerMask) : 0)));
            anyWater |= masks[col];
            lower >>= 1;
            upper >>= 1;
        }
        if (anyWater) {
            reader.beginBackground(firstRow + (((py >> 3) + page) & 15), px >> 3);
            uint8_t blockX = firstX;
            uint8_t *dest = buffer + static_cast<uint16_t>(page) * 128;
            for (uint8_t block = 0; block < blockCount; ++block) {
                uint8_t columns = reader.nextBackground();
                uint8_t x = blockX;
                while (columns) {
                    if (columns & 1) {
                        const uint8_t ink = reader.nextBackground();
                        if (x < 128) {
                            const uint8_t tile = static_cast<uint8_t>(x + subX) >> 3;
                            dest[x] |= ink & masks[tile];
                        }
                    }
                    ++x;
                    columns >>= 1;
                }
                blockX += parallax_data::kBlockWidth;
            }
            reader.endBackground();
        }
    }
}

template <typename Reader> __attribute__((noinline)) inline void clearBlack(
    uint8_t *buffer, const cave::Camera &camera, const world::TileCache &cache, Reader &reader) {
    const int16_t originX = cave::pixelOriginX(camera);
    const int16_t originY = cave::pixelOriginY(camera);
    for (uint8_t row = 0; row < world::CACHE_TILES_Y; ++row) {
        const int16_t ty = static_cast<int16_t>(cache.originTileY + row);
        if (ty < 0 || ty >= static_cast<int16_t>(world::TILES_Y))
            continue;
        const int16_t sy = static_cast<int16_t>(ty * 8 - originY);
        uint8_t flags[world::CACHE_TILES_X];
        const int16_t flagsX = cache.originTileX;
        if (flagsX >= 0 && flagsX + world::CACHE_TILES_X <= static_cast<int16_t>(world::TILES_X)) {
            reader.opaqueFlags(static_cast<uint16_t>(ty) * world::TILES_X +
                                   static_cast<uint16_t>(flagsX),
                               flags, world::CACHE_TILES_X);
        } else {
            for (uint8_t i = 0; i < world::CACHE_TILES_X; ++i) {
                const int16_t tx = static_cast<int16_t>(flagsX + i);
                flags[i] = 0;
                if (tx >= 0 && tx < static_cast<int16_t>(world::TILES_X))
                    reader.opaqueFlags(static_cast<uint16_t>(ty) * world::TILES_X +
                                           static_cast<uint16_t>(tx), flags + i, 1);
            }
        }
        for (uint8_t col = 0; col < world::CACHE_TILES_X; ++col) {
            const int16_t tx = static_cast<int16_t>(cache.originTileX + col);
            if (!flags[col] || tx < 0 || tx >= static_cast<int16_t>(world::TILES_X))
                continue;
            uint8_t black[8];
            reader.opaqueMaskBytes((static_cast<uint32_t>(ty) * world::TILES_X +
                                    static_cast<uint16_t>(tx)) * 8, black);
            const int16_t sx = static_cast<int16_t>(tx * 8 - originX);
            int16_t top = sy;
            for (uint8_t x = 0; x < 8; ++x) {
                const int16_t px = static_cast<int16_t>(sx + x);
                if (px < 0 || px >= cave::VIEW_WIDTH)
                    continue;
                uint8_t rows = black[x];
                if (top < 0)
                    rows = static_cast<uint8_t>(rows >> -top);
                const int16_t y = top < 0 ? 0 : top;
                if (y >= cave::VIEW_HEIGHT)
                    continue;
                if (y + 8 > cave::VIEW_HEIGHT)
                    rows &= static_cast<uint8_t>(0xFFu >> (y + 8 - cave::VIEW_HEIGHT));
                if (!rows)
                    continue;
                const uint8_t page = static_cast<uint8_t>(y >> 3);
                const uint8_t offset = static_cast<uint8_t>(y & 7);
                buffer[page * cave::VIEW_WIDTH + px] &= static_cast<uint8_t>(~(rows << offset));
                if (offset && page + 1 < 8)
                    buffer[(page + 1) * cave::VIEW_WIDTH + px] &= static_cast<uint8_t>(~(rows >> (8 - offset)));
            }
        }
    }
}
} // namespace parallax_view
