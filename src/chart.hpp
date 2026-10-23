#pragma once

#include "features.hpp"

#if SH_FEATURE_FOG_CHART

#include <stdint.h>

#include "game.hpp"
#include "world.hpp"

// Persistent 64x32 fog exploration. The explored set is the same 1 bpp shape
// as the baked chart water mask: 8 bytes per row, LSB = leftmost cell, one bit
// per 5x5 tile block (40x40 pixels). Nothing else is stored: the chart view
// reads water bits from the cart on demand instead of caching the map.
//
// This header is pure C++ (no Arduino headers, heap, String, or float), so the
// host tests exercise the exact reveal rules the firmware uses. The one byte
// of metadata only makes the spawn reveal idempotent.
namespace chart {

constexpr uint8_t kCellPixels = static_cast<uint8_t>(world::CHART_CELL_TILES * world::TILE_PIXELS);
constexpr int16_t kInvalidCell = -1;

// A reveal every half second of flight is cheap and keeps the trail dense
// enough to navigate. Sonar owns larger reveal radii later.
constexpr uint8_t kMoveRevealIntervalTicks = 30;
constexpr uint8_t kMoveRevealRadius = 1;
constexpr uint8_t kSpawnRevealRadius = 1;

struct State {
    uint8_t explored[world::CHART_BYTES];
    uint8_t initialised;   // spawn reveal already applied
};

// ---------------------------------------------------------------------------
// Cell mapping
// ---------------------------------------------------------------------------

// Floor/limit a single world pixel axis to its chart cell. Outside the baked
// world is not a cell: callers get kInvalidCell rather than a wrapped index.
inline int16_t cellFromPixelX(int16_t pixel) {
    if (pixel < 0)
        return kInvalidCell;
    const int16_t cell = static_cast<int16_t>(pixel / kCellPixels);
    if (cell >= static_cast<int16_t>(world::CHART_TILES_X))
        return kInvalidCell;
    return cell;
}

inline int16_t cellFromPixelY(int16_t pixel) {
    if (pixel < 0)
        return kInvalidCell;
    const int16_t cell = static_cast<int16_t>(pixel / kCellPixels);
    if (cell >= static_cast<int16_t>(world::CHART_TILES_Y))
        return kInvalidCell;
    return cell;
}

inline bool cellFromPixels(int16_t x, int16_t y, int16_t &cellX, int16_t &cellY) {
    const int16_t cx = cellFromPixelX(x);
    const int16_t cy = cellFromPixelY(y);
    if (cx == kInvalidCell || cy == kInvalidCell)
        return false;
    cellX = cx;
    cellY = cy;
    return true;
}

inline bool cellFromTile(int16_t tileX, int16_t tileY, int16_t &cellX, int16_t &cellY) {
    if (!world::inTileBounds(tileX, tileY))
        return false;
    cellX = static_cast<int16_t>(tileX / world::CHART_CELL_TILES);
    cellY = static_cast<int16_t>(tileY / world::CHART_CELL_TILES);
    return true;
}

// ---------------------------------------------------------------------------
// Explored bits
// ---------------------------------------------------------------------------

inline bool isExplored(const State &state, int16_t cellX, int16_t cellY) {
    if (!world::inChartBounds(cellX, cellY))
        return false;
    const uint8_t value = state.explored[world::chartByteIndex(cellX, cellY)];
    return ((value >> (cellX & 7)) & 1u) != 0;
}

// Returns true only when the bit was clear, so timers can count new cells.
inline bool revealCell(State &state, int16_t cellX, int16_t cellY) {
    if (!world::inChartBounds(cellX, cellY))
        return false;
    uint8_t &byte = state.explored[world::chartByteIndex(cellX, cellY)];
    const uint8_t mask = static_cast<uint8_t>(1u << (cellX & 7));
    if ((byte & mask) != 0)
        return false;
    byte |= mask;
    return true;
}

// Square (Chebyshev) reveal centred on the pixel's chart cell. A zero radius
// still reveals the centre cell. The returned count is the number of cells
// that changed from unexplored to explored, never a pixel count.
inline uint16_t revealAtPixels(State &state, int16_t x, int16_t y, uint8_t radius) {
    int16_t cellX;
    int16_t cellY;
    if (!cellFromPixels(x, y, cellX, cellY))
        return 0;

    const int16_t reach = static_cast<int16_t>(radius);
    uint16_t revealed = 0;
    for (int16_t dy = static_cast<int16_t>(-reach); dy <= reach; ++dy) {
        for (int16_t dx = static_cast<int16_t>(-reach); dx <= reach; ++dx) {
            if (revealCell(state, static_cast<int16_t>(cellX + dx), static_cast<int16_t>(cellY + dy)))
                ++revealed;
        }
    }
    return revealed;
}

// ---------------------------------------------------------------------------
// Lifecycle and movement reveal
// ---------------------------------------------------------------------------

inline void reset(State &state) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        state.explored[i] = 0;
    state.initialised = 0;
}

// Initial reveal at the baked spawn. Idempotent so a re-entered chart screen
// can call it without wiping the persistent map.
inline void revealInitial(State &state, const game::State &gameState) {
    if (state.initialised)
        return;
    revealAtPixels(state, gameState.x, gameState.y, kSpawnRevealRadius);
    state.initialised = 1;
}

// Coarse movement reveal. Call once after each flight step; the modulo makes
// the cadence a property of flight ticks, not frame rate or chart visibility.
inline void revealOnStep(State &state, const game::State &gameState) {
    if ((gameState.ticks % kMoveRevealIntervalTicks) != 0)
        return;
    revealAtPixels(state, gameState.x, gameState.y, kMoveRevealRadius);
}

static_assert(kCellPixels == 40, "chart cell is a 5x5 tile block");
static_assert(world::CHART_TILES_X * kCellPixels == world::PIXELS_X, "chart cells span the world width");
static_assert(world::CHART_TILES_Y * kCellPixels == world::PIXELS_Y, "chart cells span the world height");
static_assert(sizeof(State) == world::CHART_BYTES + 1, "chart state is exactly 256 B plus one metadata byte");

}   // namespace chart

#endif   // SH_FEATURE_FOG_CHART
