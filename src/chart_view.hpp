#pragma once



#include <stdint.h>

#include "campaign.hpp"
#include "cave_render.hpp"
#include "chart.hpp"
#include "game.hpp"
#include "world.hpp"
#include "exploration.hpp"

// Paused 128x64 chart. The pure page-major drawing code is shared by the host
// tests and the firmware.
//
// Water comes from the baked 64x32 cart mask through Reader::chartBytes: eight
// 32-byte bursts, with one scratch page on the stack and no persistent cache
// beyond the 256 B explored set. Chart cells expand to 2x2 pixel blocks.
//
// Markers are black cut-outs with white strokes so they stay visible on lit
// water. Settlement crosses only appear once their cell is explored. The sub
// block and the active job destination X blink from blinkPhase; chart frames do
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
    cave::fillRect(buffer, x, y, 2, 2, true);
}

// A black 3x3 backing makes marker strokes readable on lit water and keeps
// them distinct from the filled 2x2 cells. Clipped at the chart edges.
inline void clearMarkerBox(uint8_t *buffer, int16_t centerX, int16_t centerY) {
    cave::fillRect(buffer, static_cast<int16_t>(centerX - 1), static_cast<int16_t>(centerY - 1), 3, 3, false);
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

inline void drawDiscoveries(uint8_t *buffer, const chart::State &chartState, const game::State &s) {
    for (uint8_t i = 0; i < exploration_data::PICKUP_COUNT; ++i) {
        const exploration_data::Pickup p = exploration_data::pickup(i);
        int16_t cx, cy;
        if (!chart::cellFromPixels(p.x, p.y, cx, cy) || !chart::isExplored(chartState, cx, cy))
            continue;
        if (s.pickups & (1u << i))
            setPixel(buffer, cx * 2 + 1, cy * 2 + 1);
        else
            drawDestinationMark(buffer, cx, cy);
    }
    for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
        if (!(s.seenGates & (static_cast<uint16_t>(1u) << i)))
            continue;
        const exploration_data::Gate g = exploration_data::gate(i);
        int16_t cx, cy;
        if (!chart::cellFromPixels(g.x + g.w / 2, g.y + g.h / 2, cx, cy))
            continue;
        if (exploration::closed(s, i, g))
            drawDestinationMark(buffer, cx, cy);
        else
            setPixel(buffer, cx * 2, cy * 2);
    }
}

// Campaign survey targets use the same blinking X as an active delivery, so a
// pilot who has finished FIRST can find the pump and lift without reading the
// WORK clue first. A recorded survey disappears from the chart.
inline void drawSurveyTargets(uint8_t *buffer, const game::State &state, uint8_t blinkPhase) {
    if ((blinkPhase & kBlinkMask) != 0)
        return;
    for (uint8_t i = 0; i < world::surveyCount(); ++i) {
        world::Survey survey;
        if (!world::surveyInfo(i, survey))
            continue;
        if (!campaign::hasMilestone(state, campaign::kMilestoneFirst) || (state.surveys & (static_cast<uint8_t>(1u) << survey.id)))
            continue;
        int16_t cellX;
        int16_t cellY;
        if (!chart::cellFromPixels(survey.pixel_x, survey.pixel_y, cellX, cellY))
            continue;
        drawDestinationMark(buffer, cellX, cellY);
    }
    // Optional late expedition: after the beacon is restored, mark the
    // lower-right Deep Relay work site until the expedition completes.
    if (campaign::hasMilestone(state, campaign::kMilestoneBeacon) &&
        !campaign::missionCompleted(state, campaign::kExpedition)) {
        world::Berth relay;
        if (world::berthInfo(world::BERTH_DEEP_RELAY, relay)) {
            int16_t cellX;
            int16_t cellY;
            if (chart::cellFromPixels(relay.pixel_x, relay.pixel_y, cellX, cellY))
                drawDestinationMark(buffer, cellX, cellY);
        }
    }
}

// ---------------------------------------------------------------------------
// Frame composition
// ---------------------------------------------------------------------------

template <typename Reader> inline void drawWater(uint8_t *buffer, const chart::State &chartState, Reader &reader) {
    constexpr uint8_t pageBytes = world::CHART_ROW_BYTES * 4;
    uint8_t water[pageBytes];
    for (uint8_t page = 0; page < kFrameHeight / 8; ++page) {
        const uint16_t index = static_cast<uint16_t>(page) * pageBytes;
        reader.chartBytes(index, water, pageBytes);
        for (uint8_t i = 0; i < pageBytes; ++i)
            water[i] &= chartState.explored[index + i];
        uint8_t *out = buffer + static_cast<uint16_t>(page) * kFrameWidth;
        for (uint8_t byte = 0; byte < world::CHART_ROW_BYTES; ++byte) {
            uint8_t a = water[byte], b = water[byte + 8];
            uint8_t c = water[byte + 16], d = water[byte + 24];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const uint8_t pixels = ((a & 1) ? 0x03 : 0) | ((b & 1) ? 0x0C : 0) |
                                       ((c & 1) ? 0x30 : 0) | ((d & 1) ? 0xC0 : 0);
                *out++ = pixels;
                *out++ = pixels;
                a >>= 1; b >>= 1; c >>= 1; d >>= 1;
            }
        }
    }
}

// Entry always composes immediately. Once paused, only the markers can change:
// redraw at each 16-frame blink boundary, including counter wrap.
inline bool needsRedraw(const game::State &state, uint8_t phase) {
    (void)state;
    return (phase & (kBlinkMask - 1)) == 0;
}

// Settlements are landmarks, not omniscient map data: an unexplored cell draws
// nothing. Once explored, a cross sits on the settlement's chart cell.
inline void drawSettlements(uint8_t *buffer, const chart::State &chartState) {
    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        world::Settlement settlement;
        // Loop id is bounded by SETTLEMENT_COUNT; the generated table has the
        // same extent, so the metadata accessor cannot fail here.
        (void)world::settlementInfo(id, settlement);
        int16_t cellX;
        int16_t cellY;
        if (!chart::cellFromPixels(settlement.pixel_x, settlement.pixel_y, cellX, cellY))
            continue;
        if (!chart::isExplored(chartState, cellX, cellY))
            continue;
        drawCross(buffer, cellX, cellY);
    }
}

// The contract knows its destination, so its blinking X is drawn even when the
// cell is still fogged. The blink phase is a frame counter owned by the caller
// because chart frames never advance game::State.
inline void drawDestination(uint8_t *buffer, const game::State &gameState, uint8_t blinkPhase) {
    if (!game::hasActiveJob(gameState))
        return;
    if ((blinkPhase & kBlinkMask) != 0)
        return;
    world::Berth berth;
    if (!world::berthInfo(gameState.activeJob.to, berth))
        return;
    int16_t cellX;
    int16_t cellY;
    if (!chart::cellFromPixels(berth.pixel_x, berth.pixel_y, cellX, cellY))
        return;
    drawDestinationMark(buffer, cellX, cellY);
}

inline void drawSubMarker(uint8_t *buffer, const game::State &gameState, uint8_t blinkPhase) {
    if ((blinkPhase & kBlinkMask) != 0)
        return;
    int16_t cellX;
    int16_t cellY;
    if (!chart::cellFromPixels(gameState.x, gameState.y, cellX, cellY))
        return;
    drawSub(buffer, cellX, cellY);
}

// Compose the whole 128x64 chart. Cart reads happen here, before the single
// display transaction. The caller owns the persistent chart::State.
template <typename Reader> inline void draw(uint8_t *buffer, const chart::State &chartState, const game::State &gameState, Reader &reader, uint8_t blinkPhase) {
    // Water expansion overwrites all 1024 bytes, including fogged/land cells.
    drawWater(buffer, chartState, reader);
    drawDiscoveries(buffer, chartState, gameState);
    drawSettlements(buffer, chartState);
    drawSurveyTargets(buffer, gameState, blinkPhase);
    drawDestination(buffer, gameState, blinkPhase);
    drawSubMarker(buffer, gameState, blinkPhase);
}

}   // namespace chart_view

