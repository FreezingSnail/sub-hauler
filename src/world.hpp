#pragma once

#include <stdint.h>

#include "generated/world.hpp"

// Runtime access to the baked world tables. This header is pure C++: it has no
// Arduino headers and never stores a world-sized grid. A Reader supplies one
// byte at a time, so host tests can inject a synthetic world and device code
// can supply an FX-cart adapter (see world_fx.hpp).
//
// Cart contract: Reader::tileByte/Reader::chartByte may perform FX cart reads.
// Call them from update/render only, never during FX::display.
namespace world {

// ---------------------------------------------------------------------------
// Coordinates and table indexing
// ---------------------------------------------------------------------------

inline bool inTileBounds(int16_t tx, int16_t ty) {
    return tx >= 0 && ty >= 0 && tx < static_cast<int16_t>(TILES_X) && ty < static_cast<int16_t>(TILES_Y);
}

inline bool inChartBounds(int16_t cx, int16_t cy) {
    return cx >= 0 && cy >= 0 && cx < static_cast<int16_t>(CHART_TILES_X) && cy < static_cast<int16_t>(CHART_TILES_Y);
}

// Only call with in-bounds tile coordinates.
inline uint16_t tileByteIndex(int16_t tx, int16_t ty) {
    return static_cast<uint16_t>(ty) * WORLD_ROW_BYTES + (static_cast<uint16_t>(tx) >> 3);
}

// Only call with in-bounds chart coordinates.
inline uint16_t chartByteIndex(int16_t cx, int16_t cy) {
    return static_cast<uint16_t>(cy) * CHART_ROW_BYTES + (static_cast<uint16_t>(cx) >> 3);
}

// Floor pixel coordinate to its containing tile. Negative pixels round down,
// matching the world border rather than truncating toward zero.
inline int16_t tileFromPixel(int16_t pixel) {
    if (pixel >= 0)
        return static_cast<int16_t>(pixel >> 3);
    return static_cast<int16_t>(-((static_cast<int16_t>(-pixel) + 7) >> 3));
}

inline int16_t chartFromTile(int16_t tile) {
    return static_cast<int16_t>(tile / CHART_CELL_TILES);
}

// ---------------------------------------------------------------------------
// Tile and chart lookups
// ---------------------------------------------------------------------------

// Outside the baked world is treated as solid so collision cannot escape the
// sealed border. No cart read happens for an out-of-bounds query.
template <typename Reader> inline bool tileSolid(Reader &reader, int16_t tx, int16_t ty) {
    if (!inTileBounds(tx, ty))
        return true;
    const uint8_t value = reader.tileByte(tileByteIndex(tx, ty));
    return ((value >> (tx & 7)) & 1u) != 0;
}

// Outside the chart is land, not water. No cart read happens out of bounds.
template <typename Reader> inline bool chartCellWater(Reader &reader, int16_t cx, int16_t cy) {
    if (!inChartBounds(cx, cy))
        return false;
    const uint8_t value = reader.chartByte(chartByteIndex(cx, cy));
    return ((value >> (cx & 7)) & 1u) != 0;
}

// Convenience: a tile's water state is the state of its 5x5 chart block.
template <typename Reader> inline bool tileWater(Reader &reader, int16_t tx, int16_t ty) {
    if (!inTileBounds(tx, ty))
        return false;
    return chartCellWater(reader, chartFromTile(tx), chartFromTile(ty));
}

// ---------------------------------------------------------------------------
// Visible-window tile cache
// ---------------------------------------------------------------------------

// The display shows 16x8 tiles; 18x10 keeps a one-tile margin for collision
// around the view. The bitmap is packed densely: 180 bits = 23 B. No full-grid
// buffer is allocated anywhere in the runtime.
constexpr uint8_t CACHE_TILES_X = 18;
constexpr uint8_t CACHE_TILES_Y = 10;
constexpr uint16_t CACHE_BITS = static_cast<uint16_t>(CACHE_TILES_X) * CACHE_TILES_Y;
constexpr uint8_t CACHE_BYTES = (CACHE_BITS + 7) / 8;

struct TileCache {
    int16_t originTileX;
    int16_t originTileY;
    uint8_t valid;
    uint8_t bits[CACHE_BYTES];
};

inline void clearCache(TileCache &cache) {
    cache.originTileX = 0;
    cache.originTileY = 0;
    cache.valid = 0;
    for (uint8_t i = 0; i < CACHE_BYTES; ++i)
        cache.bits[i] = 0;
}

inline bool cacheLookup(const TileCache &cache, int16_t tx, int16_t ty, bool &solid) {
    if (!cache.valid)
        return false;

    const int16_t dx = static_cast<int16_t>(tx - cache.originTileX);
    const int16_t dy = static_cast<int16_t>(ty - cache.originTileY);
    if (dx < 0 || dy < 0 || dx >= static_cast<int16_t>(CACHE_TILES_X) || dy >= static_cast<int16_t>(CACHE_TILES_Y))
        return false;

    const uint8_t bit = static_cast<uint8_t>(dy * CACHE_TILES_X + dx);
    solid = ((cache.bits[bit >> 3] >> (bit & 7)) & 1u) != 0;
    return true;
}

inline bool cacheContains(const TileCache &cache, int16_t tx, int16_t ty) {
    bool solid;
    return cacheLookup(cache, tx, ty, solid);
}

inline void cacheSet(TileCache &cache, uint8_t index, bool solid) {
    const uint8_t mask = static_cast<uint8_t>(1u << (index & 7));
    if (solid)
        cache.bits[index >> 3] |= mask;
    else
        cache.bits[index >> 3] &= static_cast<uint8_t>(~mask);
}

// Refresh the window when the camera tile moves. Returns true only when the
// cache was refilled, so callers can avoid paying for reads every frame.
template <typename Reader> inline bool refreshCache(TileCache &cache, Reader &reader, int16_t cameraTileX, int16_t cameraTileY) {
    if (cache.valid && cache.originTileX == cameraTileX && cache.originTileY == cameraTileY)
        return false;

    cache.originTileX = cameraTileX;
    cache.originTileY = cameraTileY;
    cache.valid = 1;

    for (uint8_t i = 0; i < CACHE_BYTES; ++i)
        cache.bits[i] = 0;

    for (uint8_t row = 0; row < CACHE_TILES_Y; ++row) {
        const int16_t ty = static_cast<int16_t>(cameraTileY + row);
        for (uint8_t col = 0; col < CACHE_TILES_X; ++col) {
            const int16_t tx = static_cast<int16_t>(cameraTileX + col);
            const uint8_t index = static_cast<uint8_t>(row * CACHE_TILES_X + col);
            cacheSet(cache, index, tileSolid(reader, tx, ty));
        }
    }
    return true;
}

// Use the cache for in-window tiles and fall back to a direct cart read for
// tiles outside the window (for example a collision probe at its edge).
template <typename Reader> inline bool tileSolidCached(const TileCache &cache, Reader &reader, int16_t tx, int16_t ty) {
    bool solid;
    if (cacheLookup(cache, tx, ty, solid))
        return solid;
    return tileSolid(reader, tx, ty);
}

// ---------------------------------------------------------------------------
// Pixel-rectangle collision
// ---------------------------------------------------------------------------

namespace detail {

// Half-open rectangle [x, x+w) x [y, y+h). A rectangle that reaches outside
// the baked world hits the sealed border. The optional cache avoids repeated
// cart reads for the common in-window case.
template <typename Reader> inline bool rectHitsImpl(Reader &reader, const TileCache *cache, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w <= 0 || h <= 0)
        return false;

    const int16_t left = tileFromPixel(x);
    const int16_t top = tileFromPixel(y);
    const int16_t right = tileFromPixel(static_cast<int16_t>(x + w - 1));
    const int16_t bottom = tileFromPixel(static_cast<int16_t>(y + h - 1));

    if (left < 0 || top < 0 || right >= static_cast<int16_t>(TILES_X) || bottom >= static_cast<int16_t>(TILES_Y))
        return true;

    for (int16_t ty = top; ty <= bottom; ++ty) {
        for (int16_t tx = left; tx <= right; ++tx) {
            bool solid;
            if (!cache || !cacheLookup(*cache, tx, ty, solid))
                solid = tileSolid(reader, tx, ty);
            if (solid)
                return true;
        }
    }
    return false;
}

}   // namespace detail

template <typename Reader> inline bool rectHits(Reader &reader, int16_t x, int16_t y, int16_t w, int16_t h) {
    return detail::rectHitsImpl(reader, nullptr, x, y, w, h);
}

template <typename Reader> inline bool rectHitsCached(const TileCache &cache, Reader &reader, int16_t x, int16_t y, int16_t w, int16_t h) {
    return detail::rectHitsImpl(reader, &cache, x, y, w, h);
}

// ---------------------------------------------------------------------------
// Generated metadata accessors
// ---------------------------------------------------------------------------

inline uint8_t settlementCount() {
    return SETTLEMENT_COUNT;
}

inline uint8_t mineCount() {
    return MINE_COUNT;
}

inline int16_t spawnTileX() {
    return SPAWN_TILE_X;
}

inline int16_t spawnTileY() {
    return SPAWN_TILE_Y;
}

// Metadata stays in the generated header for now. These accessors copy one
// record out; the current firmware does not call them, so the generated
// constexpr arrays are not odr-used on device. Move the records to PROGMEM or
// the FX image before device rendering starts calling them (see the SRAM
// warning in generated/world.hpp).
inline bool settlementInfo(uint8_t id, Settlement &out) {
    if (id >= SETTLEMENT_COUNT)
        return false;
    out = SETTLEMENTS[id];
    return true;
}

inline bool mineInfo(uint8_t id, Mine &out) {
    if (id >= MINE_COUNT)
        return false;
    out = MINES[id];
    return true;
}

static_assert(CACHE_BITS == 180, "18x10 tile cache must hold 180 tiles");
static_assert(CACHE_BYTES == 23, "18x10 packed bitmap must be 23 bytes");
static_assert(sizeof(TileCache) <= 32, "visible-tile cache must stay small");
static_assert(sizeof(TileCache) < WORLD_BYTES, "cache must not be a full world grid");

}   // namespace world
