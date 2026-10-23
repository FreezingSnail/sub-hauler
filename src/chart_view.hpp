#pragma once

#include "features.hpp"

#if SH_FEATURE_FOG_CHART

#include <stdint.h>

#include "chart.hpp"
#include "game.hpp"
#include "world.hpp"

// Paused 128x64 chart. The pure page-major drawing code is shared by the host
// tests and the firmware.
//
// Water comes from the baked 64x32 cart mask through Reader::chartByte: exactly
// one read per byte, eight bytes per chart row, with no map-sized cache beyond
// the 256 B explored set. Large chart cells are drawn as 2x2 pixel blocks.
//
// Markers are black cut-outs with white strokes so they stay visible on lit
// water. Settlement crosses only appear once their cell is explored. The
// active job destination is an X that blinks from blinkPhase; chart frames do
// not step the world, so the caller supplies that phase. B closes the screen;
// no text is drawn, which keeps the map area clear.
namespace chart_view {

constexpr uint8_t kCellPixels = 2;
constexpr int16_t kFrameWidth = 128;
constexpr int16_t kFrameHeight = 64;
constexpr uint16_t kFrameBytes = static_cast<uint16_t>(kFrameWidth) * kFrameHeight / 8;
constexpr uint8_t kBlinkMask = 0x10;   // 16 chart frames on, 16 off

static_assert(world::CHART_TILES_X * kCellPixels == kFrameWidth, "64 chart cells fill 128 pixels");
static_assert(world::CHART_TILES_Y * kCellPixels == kFrameHeight, "32 chart cells fill 64 pixels");

// ---------------------------------------------------------------------------
// Primitive page-major writes (clipped to 128x64)
// ---------------------------------------------------------------------------

inline void setPixel(uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= kFrameWidth || y >= kFrameHeight)
        return;
    buffer[(y >> 3) * kFrameWidth + x] |= static_cast<uint8_t>(1u << (y & 7));
}

inline void clearPixel(uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= kFrameWidth || y >= kFrameHeight)
        return;
    buffer[(y >> 3) * kFrameWidth + x] &= static_cast<uint8_t>(~(1u << (y & 7)));
}

// One chart cell is a 2x2 block. It never crosses a page boundary because the
// block starts on an even pixel row.
inline void drawCell(uint8_t *buffer, uint8_t cellX, uint8_t cellY) {
    const uint8_t x = static_cast<uint8_t>(cellX * kCellPixels);
    const uint8_t shift = static_cast<uint8_t>((cellY & 3) * kCellPixels);
    const uint8_t bits = static_cast<uint8_t>(0x03u << shift);
    uint8_t *row = &buffer[(cellY >> 2) * kFrameWidth + x];
    row[0] |= bits;
    row[1] |= bits;
}

inline void fillCell(uint8_t *buffer, int16_t x, int16_t y) {
    for (int16_t dy = 0; dy < 2; ++dy) {
        for (int16_t dx = 0; dx < 2; ++dx)
            setPixel(buffer, static_cast<int16_t>(x + dx), static_cast<int16_t>(y + dy));
    }
}

// A black 3x3 backing makes marker strokes readable on lit water and keeps
// them distinct from the filled 2x2 cells. Clipped at the chart edges.
inline void clearMarkerBox(uint8_t *buffer, int16_t centerX, int16_t centerY) {
    for (int16_t dy = -1; dy <= 1; ++dy) {
        for (int16_t dx = -1; dx <= 1; ++dx)
            clearPixel(buffer, static_cast<int16_t>(centerX + dx), static_cast<int16_t>(centerY + dy));
    }
}

inline void drawCross(uint8_t *buffer, int16_t cellX, int16_t cellY) {
    const int16_t centerX = static_cast<int16_t>(cellX * kCellPixels + 1);
    const int16_t centerY = static_cast<int16_t>(cellY * kCellPixels + 1);
    clearMarkerBox(buffer, centerX, centerY);
    setPixel(buffer, centerX, centerY);
    setPixel(buffer, static_cast<int16_t>(centerX - 1), centerY);
    setPixel(buffer, static_cast<int16_t>(centerX + 1), centerY);
    setPixel(buffer, centerX, static_cast<int16_t>(centerY - 1));
    setPixel(buffer, centerX, static_cast<int16_t>(centerY + 1));
}

// Destination X: same 3x3 footprint as a settlement cross but different shape.
inline void drawDestinationMark(uint8_t *buffer, int16_t cellX, int16_t cellY) {
    const int16_t centerX = static_cast<int16_t>(cellX * kCellPixels + 1);
    const int16_t centerY = static_cast<int16_t>(cellY * kCellPixels + 1);
    clearMarkerBox(buffer, centerX, centerY);
    setPixel(buffer, centerX, centerY);
    setPixel(buffer, static_cast<int16_t>(centerX - 1), static_cast<int16_t>(centerY - 1));
    setPixel(buffer, static_cast<int16_t>(centerX + 1), static_cast<int16_t>(centerY - 1));
    setPixel(buffer, static_cast<int16_t>(centerX - 1), static_cast<int16_t>(centerY + 1));
    setPixel(buffer, static_cast<int16_t>(centerX + 1), static_cast<int16_t>(centerY + 1));
}

// Sub marker: a filled 2x2 white cell inside the black backing.
inline void drawSub(uint8_t *buffer, int16_t cellX, int16_t cellY) {
    const int16_t x = static_cast<int16_t>(cellX * kCellPixels);
    const int16_t y = static_cast<int16_t>(cellY * kCellPixels);
    clearMarkerBox(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1));
    fillCell(buffer, x, y);
}

// ---------------------------------------------------------------------------
// Frame composition
// ---------------------------------------------------------------------------

template <typename Reader> inline void drawWater(uint8_t *buffer, const chart::State &chartState, Reader &reader) {
    for (uint8_t row = 0; row < world::CHART_TILES_Y; ++row) {
        const uint16_t rowIndex = static_cast<uint16_t>(row) * world::CHART_ROW_BYTES;
        for (uint8_t byte = 0; byte < world::CHART_ROW_BYTES; ++byte) {
            // One cart read per chart byte; the explored bitmap is the only
            // persistent map storage.
            const uint8_t water = reader.chartByte(static_cast<uint16_t>(rowIndex + byte));
            const uint8_t lit = static_cast<uint8_t>(water & chartState.explored[rowIndex + byte]);
            if (lit == 0)
                continue;
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if ((lit & (1u << bit)) != 0) {
                    const uint8_t cellX = static_cast<uint8_t>(byte * 8 + bit);
                    drawCell(buffer, cellX, row);
                }
            }
        }
    }
}

// Settlements are landmarks, not omniscient map data: an unexplored cell draws
// nothing. Once explored, a cross sits on the settlement's chart cell.
inline void drawSettlements(uint8_t *buffer, const chart::State &chartState) {
    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        world::Settlement settlement;
        if (!world::settlementInfo(id, settlement))
            continue;
        int16_t cellX;
        int16_t cellY;
        if (!chart::cellFromPixels(settlement.pixel_x, settlement.pixel_y, cellX, cellY))
            continue;
        if (!chart::isExplored(chartState, cellX, cellY))
            continue;
        drawCross(buffer, cellX, cellY);
    }
}

#if SH_FEATURE_JOBS
// The contract knows its destination, so its blinking X is drawn even when the
// cell is still fogged. The blink phase is a frame counter owned by the caller
// because chart frames never advance game::State.
inline void drawDestination(uint8_t *buffer, const game::State &gameState, uint8_t blinkPhase) {
    if (!game::hasActiveJob(gameState))
        return;
    if ((blinkPhase & kBlinkMask) != 0)
        return;
    world::Settlement settlement;
    if (!world::settlementInfo(gameState.activeJob.to, settlement))
        return;
    int16_t cellX;
    int16_t cellY;
    if (!chart::cellFromPixels(settlement.pixel_x, settlement.pixel_y, cellX, cellY))
        return;
    drawDestinationMark(buffer, cellX, cellY);
}
#endif

inline void drawSubMarker(uint8_t *buffer, const game::State &gameState) {
    int16_t cellX;
    int16_t cellY;
    if (!chart::cellFromPixels(gameState.x, gameState.y, cellX, cellY))
        return;
    drawSub(buffer, cellX, cellY);
}

// Compose the whole 128x64 chart. Cart reads happen here, before the single
// display transaction. The caller owns the persistent chart::State.
template <typename Reader>
inline void draw(uint8_t *buffer, const chart::State &chartState, const game::State &gameState, Reader &reader, uint8_t blinkPhase) {
    for (uint16_t i = 0; i < kFrameBytes; ++i)
        buffer[i] = 0;

    drawWater(buffer, chartState, reader);
    drawSettlements(buffer, chartState);
#if SH_FEATURE_JOBS
    drawDestination(buffer, gameState, blinkPhase);
#endif
    drawSubMarker(buffer, gameState);
}

}   // namespace chart_view

#endif   // SH_FEATURE_FOG_CHART
