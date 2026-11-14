#pragma once

#include <stdint.h>

#include "generated/level.hpp"

// Runtime access to the baked world tables. This header is pure C++: it has no
// Arduino headers and never stores a world-sized grid. A Reader supplies one
// byte at a time, so host tests can inject a synthetic world and device code
// can supply an FX-cart adapter (see world_fx.hpp).
//
// Cart contract: Reader::tileByte/Reader::chartByte may perform FX cart reads.
// Chart composition additionally uses Reader::chartBytes for bounded bursts.
// Call these from update/render only, never during FX::display.
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

// Floor pixel coordinate to its containing tile. Arithmetic right shift by 3
// is floor division for every int16 value on AVR-GCC and the host compilers;
// the static_assert below is the compile-time proof of that implementation
// contract. Negative pixels round down, matching the world border.
static_assert((-1 >> 3) == -1 && (-8 >> 3) == -1 && (-9 >> 3) == -2, "signed right shift must floor");
inline int16_t tileFromPixel(int16_t pixel) {
    return static_cast<int16_t>(pixel >> 3);
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
template <typename Reader> inline uint8_t rowByte(Reader &reader, int16_t column, int16_t row) {
    return column < 0 || column >= WORLD_ROW_BYTES || row < 0 || row >= TILES_Y
        ? 0xFF : reader.tileByte(static_cast<uint16_t>(row) * WORLD_ROW_BYTES + column);
}

// Device readers can fetch a packed row in one cart transaction. Pure host
// readers keep the scalar interface and identical byte/read-count semantics.
template <typename Reader> inline auto tileBytes(Reader &reader, uint16_t index,
    uint8_t *dest, uint8_t count, int) -> decltype(reader.tileBytes(index, dest, count), void()) {
    reader.tileBytes(index, dest, count);
}
template <typename Reader> inline void tileBytes(Reader &reader, uint16_t index,
    uint8_t *dest, uint8_t count, ...) {
    for (uint8_t i = 0; i < count; ++i) dest[i] = reader.tileByte(index + i);
}

template <typename Reader> __attribute__((noinline)) inline bool refreshCache(TileCache &cache, Reader &reader, int16_t cameraTileX, int16_t cameraTileY) {
    if (cache.valid && cache.originTileX == cameraTileX && cache.originTileY == cameraTileY)
        return false;

    cache.originTileX = cameraTileX;
    cache.originTileY = cameraTileY;
    cache.valid = 1;

    for (uint8_t i = 0; i < CACHE_BYTES; ++i)
        cache.bits[i] = 0;

    const int16_t sourceX = cameraTileX >> 3;
    const uint8_t shift = cameraTileX & 7;
    const bool interior = cameraTileX >= 0 && cameraTileX + CACHE_TILES_X <= TILES_X &&
                          cameraTileY >= 0 && cameraTileY + CACHE_TILES_Y <= TILES_Y;
    for (uint8_t row = 0; row < CACHE_TILES_Y; ++row) {
        const int16_t ty = static_cast<int16_t>(cameraTileY + row);
        uint32_t bits;
        if (interior) {
            uint8_t bytes[4] = {};
            tileBytes(reader, static_cast<uint16_t>(ty) * WORLD_ROW_BYTES + sourceX,
                      bytes, shift == 7 ? 4 : 3, 0);
            bits = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                   (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
        } else {
            bits = static_cast<uint32_t>(rowByte(reader, sourceX, ty)) |
            (static_cast<uint32_t>(rowByte(reader, sourceX + 1, ty)) << 8) |
            (static_cast<uint32_t>(rowByte(reader, sourceX + 2, ty)) << 16);
            if (shift == 7)
                bits |= static_cast<uint32_t>(rowByte(reader, sourceX + 3, ty)) << 24;
        }
        bits = (bits >> shift) & 0x3FFFFUL;
        const uint8_t destBit = row * CACHE_TILES_X;
        bits <<= destBit & 7;
        uint8_t *dest = cache.bits + (destBit >> 3);
        dest[0] |= static_cast<uint8_t>(bits);
        dest[1] = static_cast<uint8_t>(bits >> 8);
        dest[2] = static_cast<uint8_t>(bits >> 16);
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

inline uint8_t berthCount() {
    return BERTH_COUNT;
}

inline uint8_t serviceBerthCount() {
    return SERVICE_BERTH_COUNT;
}

inline uint8_t surveyCount() {
    return SURVEY_COUNT;
}

inline uint8_t currentCount() {
    return CURRENT_COUNT;
}

// True when this zone is a tide gate rather than a room-drift hazard. Gated
// zones block upstream travel until the current thruster is owned.
inline bool currentGated(uint8_t id) {
    return ((CURRENT_GATE_MASK >> id) & 1u) != 0;
}

inline int16_t spawnTileX() {
    return SPAWN_TILE_X;
}

inline int16_t spawnTileY() {
    return SPAWN_TILE_Y;
}

// Metadata accessors. The generated header owns the representation: host
// builds copy from constexpr records, AVR builds read position-only PROGMEM
// records (Settlement::name is null on device). Both paths return the same
// values for the same id, and neither one drags a world-sized table into SRAM.
inline bool settlementInfo(uint8_t id, Settlement &out) {
    return settlementMeta(id, out);
}

inline bool currentInfo(uint8_t id, Current &out) {
    return currentMeta(id, out);
}

// A berth is either a freight settlement (0..SETTLEMENT_COUNT-1, milestone 0)
// or a generated service-only berth (4.., gated by its restoration milestone).
inline bool berthInfo(uint8_t id, Berth &out) {
    return berthMeta(id, out);
}

inline bool serviceBerthInfo(uint8_t index, Berth &out) {
    return serviceBerthMeta(index, out);
}

inline bool surveyInfo(uint8_t index, Survey &out) {
    return surveyMeta(index, out);
}

static_assert(CACHE_BITS == 180, "18x10 tile cache must hold 180 tiles");
static_assert(CACHE_BYTES == 23, "18x10 packed bitmap must be 23 bytes");
static_assert(sizeof(TileCache) <= 56, "visible-tile cache must stay small");
static_assert(sizeof(TileCache) < WORLD_BYTES, "cache must not be a full world grid");

}   // namespace world
