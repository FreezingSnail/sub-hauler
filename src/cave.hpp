#pragma once

#include <stdint.h>

#include "world.hpp"

// Visible-window math and the 8x8 cave tile atlas.
//
// The display shows 16x8 tiles (128x64 pixels). Rendering walks that fixed
// window only: there is no whole-map loop and no world-sized buffer. The
// camera is supplied in tile coordinates; sub-tile scrolling is intentionally
// out of scope for the M1 debug view.
//
// This header is pure C++ so the host tests can include it. The atlas is
// marked PROGMEM only on AVR, where a namespace-scope const array would
// otherwise be copied into SRAM at startup.
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define CAVE_ATLAS_PROGMEM PROGMEM
#else
#define CAVE_ATLAS_PROGMEM
#endif

namespace cave {

// ---------------------------------------------------------------------------
// Visible window
// ---------------------------------------------------------------------------

constexpr uint8_t VIEW_TILES_X = 16;
constexpr uint8_t VIEW_TILES_Y = 8;
constexpr int16_t VIEW_WIDTH = VIEW_TILES_X * world::TILE_PIXELS;    // 128
constexpr int16_t VIEW_HEIGHT = VIEW_TILES_Y * world::TILE_PIXELS;   // 64

constexpr int16_t MAX_CAMERA_TILE_X = static_cast<int16_t>(world::TILES_X - VIEW_TILES_X);
constexpr int16_t MAX_CAMERA_TILE_Y = static_cast<int16_t>(world::TILES_Y - VIEW_TILES_Y);

static_assert(VIEW_WIDTH == 128, "cave view is the full display width");
static_assert(VIEW_HEIGHT == 64, "cave view is the full display height");
static_assert(world::TILE_PIXELS == 8, "tile atlas assumes 8x8 tiles");
static_assert(MAX_CAMERA_TILE_X == 304, "camera clamp is part of the map contract");
static_assert(MAX_CAMERA_TILE_Y == 152, "camera clamp is part of the map contract");

struct Camera {
    int16_t tileX;
    int16_t tileY;
};

inline int16_t clampTile(int16_t value, int16_t limit) {
    if (value < 0)
        return 0;
    if (value > limit)
        return limit;
    return value;
}

// Top-left tile of the visible window, clamped so the 16x8 window always sits
// inside the baked map, including at the origin and both far edges.
inline Camera cameraAtTile(int16_t tileX, int16_t tileY) {
    Camera camera;
    camera.tileX = clampTile(tileX, MAX_CAMERA_TILE_X);
    camera.tileY = clampTile(tileY, MAX_CAMERA_TILE_Y);
    return camera;
}

// Centre the window on a pixel coordinate, then snap to the tile grid. The
// M1 camera is static; M2 owns smooth sub-tile movement.
inline Camera cameraCenteredAtPixel(int16_t pixelX, int16_t pixelY) {
    return cameraAtTile(static_cast<int16_t>((pixelX - VIEW_WIDTH / 2) / world::TILE_PIXELS),
                        static_cast<int16_t>((pixelY - VIEW_HEIGHT / 2) / world::TILE_PIXELS));
}

// The tile cache is one tile larger than the view on every side. Its origin is
// therefore the window origin minus one, which puts the four-neighbour ring of
// every visible tile inside the cache even at the map border (out-of-bounds
// tiles read as solid without a cart access).
inline int16_t cacheOriginTileX(const Camera &camera) {
    return static_cast<int16_t>(camera.tileX - 1);
}

inline int16_t cacheOriginTileY(const Camera &camera) {
    return static_cast<int16_t>(camera.tileY - 1);
}

inline int16_t cacheColumn(const Camera &camera, int16_t tileX) {
    return static_cast<int16_t>(tileX - camera.tileX + 1);
}

inline int16_t cacheRow(const Camera &camera, int16_t tileY) {
    return static_cast<int16_t>(tileY - camera.tileY + 1);
}


// ---------------------------------------------------------------------------
// Tile selection
// ---------------------------------------------------------------------------

// Bits describe which orthogonal neighbours are open water. A solid tile with
// no open neighbours is interior rock; every other combination is an edge or
// corner tile.
constexpr uint8_t OPEN_NORTH = 1;
constexpr uint8_t OPEN_SOUTH = 2;
constexpr uint8_t OPEN_WEST = 4;
constexpr uint8_t OPEN_EAST = 8;
constexpr uint8_t OPEN_MASK_COUNT = 16;

// A cache miss means the tile is outside the cached window. Treat it as solid:
// the cache margin covers every neighbour of a visible tile, so this only
// happens at the sealed world border.
inline bool cachedSolid(const world::TileCache &cache, int16_t tileX, int16_t tileY) {
    bool solid = true;
    world::cacheLookup(cache, tileX, tileY, solid);
    return solid;
}

inline uint8_t openMask(const world::TileCache &cache, int16_t tileX, int16_t tileY) {
    uint8_t mask = 0;
    if (!cachedSolid(cache, tileX, static_cast<int16_t>(tileY - 1)))
        mask |= OPEN_NORTH;
    if (!cachedSolid(cache, tileX, static_cast<int16_t>(tileY + 1)))
        mask |= OPEN_SOUTH;
    if (!cachedSolid(cache, static_cast<int16_t>(tileX - 1), tileY))
        mask |= OPEN_WEST;
    if (!cachedSolid(cache, static_cast<int16_t>(tileX + 1), tileY))
        mask |= OPEN_EAST;
    return mask;
}

// 8x8 native 1-bit tile atlas, 8 bytes per tile and one byte per column, in the
// same page-major layout as the display buffer: bit r is pixel row r (LSB is
// the top row). Index is cave::openMask().
//
// Each pattern is solid rock with the open sides trimmed by one pixel, which
// rounds the silhouette where water meets rock. Sixteen entries cost 128 B of
// program flash on AVR and no SRAM. Generated from the documented rule; the
// host test cross-checks every byte against an independent reference.
const uint8_t CAVE_ATLAS[OPEN_MASK_COUNT][8] CAVE_ATLAS_PROGMEM = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // 0x0 interior
    {0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},  // 0x1 north open
    {0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F},  // 0x2 south open
    {0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E},  // 0x3 north+south
    {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // 0x4 west open
    {0x00, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE},  // 0x5 north+west
    {0x00, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F},  // 0x6 south+west
    {0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E},  // 0x7 north+south+west
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00},  // 0x8 east open
    {0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC, 0x00},  // 0x9 north+east
    {0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x00},  // 0xA south+east
    {0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00},  // 0xB north+south+east
    {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00},  // 0xC west+east
    {0x00, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC, 0x00},  // 0xD north+west+east
    {0x00, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x00},  // 0xE south+west+east
    {0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00},  // 0xF all four open
};

static_assert(sizeof(CAVE_ATLAS) == OPEN_MASK_COUNT * 8, "atlas is 16 tiles of 8 bytes");

// ---------------------------------------------------------------------------
// Settlement gantries
// ---------------------------------------------------------------------------

// Gantry footprint around its dock centre, in pixels. The dock centre is the
// generated settlement pixel position, so the hardware dock and the drawn
// clamp share one source of truth.
constexpr int16_t GANTRY_HALF_WIDTH = 13;
constexpr int16_t GANTRY_REACH_UP = 14;
constexpr int16_t GANTRY_REACH_DOWN = 5;

inline int16_t dockScreenX(const Camera &camera, int16_t pixelX) {
    return static_cast<int16_t>(pixelX - camera.tileX * world::TILE_PIXELS);
}

inline int16_t dockScreenY(const Camera &camera, int16_t pixelY) {
    return static_cast<int16_t>(pixelY - camera.tileY * world::TILE_PIXELS);
}

inline bool gantryVisible(int16_t screenX, int16_t screenY) {
    return screenX > -GANTRY_HALF_WIDTH && screenX < VIEW_WIDTH + GANTRY_HALF_WIDTH &&
           screenY > -GANTRY_REACH_UP && screenY < VIEW_HEIGHT + GANTRY_REACH_DOWN;
}

}   // namespace cave
