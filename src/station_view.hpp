#pragma once

#include <stdint.h>

#include "game.hpp"
#include "station.hpp"
#include "jobs.hpp"
#include "generated/exploration.hpp"

// The generated station menu frames and hint rows remain cart ABI, but the
// runtime board is native: the shipped frames still carry the retired SHOP row
// and no longer match the live WORK/UNDOCK and summary/WORK/UNDOCK shapes.
// Keeping the frame selectors here documents that ABI for the menu tests while
// the draw path below uses the live row rules.

#if defined(__AVR__)
#include "panel_view.hpp"
#endif

// Station board, 128x64. The layout is pure and host-checkable; the draw
// function is device-only because it uses the external FX font.
//
// Header: settlement name.
// Status: current air against the fixed 300-unit tank.
// Rows:   WORK/UNDOCK, or summary/WORK/UNDOCK when a contract is active.
//         Service-only berths show UNDOCK only. One inverted selection bar.
//         WORK opens the restoration board. B always undocks.
// Hint:   A confirms the row and B always undocks. Contract acceptance
//         auto-loads the four cargo units, so the active board needs no load
//         row or screen.
namespace station_view {

constexpr uint8_t kNameX = 2;
constexpr uint8_t kHeaderY = 2;
// Archival layout: the generated frames still bake the retired credits caption
// at this column. Runtime text no longer prints a balance.
constexpr uint8_t kCreditsX = 80;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kStatusX = 2;
constexpr uint8_t kStatusY = 15;
constexpr uint8_t kRowX = 4;
// Three rows (summary/WORK/UNDOCK) must fit above the hint:
// header/status end at y=21, rows are 22+7*row, and the hint starts at 57.
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;
constexpr uint8_t kArchivalOfferChars = 20;   // "DEEP ANCHOR 240"

// Native console chrome. The header keeps the longest berth label
// ("LOWER GANTRY" / "FREIGHT YARD", 12 cells) clear of a 16x8 berth icon. The
// air tank gauge sits under that icon so the live numbers stay readable at the
// left, and the inert summary row gets its own small marker.
constexpr uint8_t kNameChars = 12;
constexpr uint8_t kBerthIconCount = 8;
constexpr uint8_t kIconX = 110;
constexpr uint8_t kIconY = 2;
constexpr uint8_t kIconW = 16;
constexpr uint8_t kIconH = 8;
constexpr uint8_t kTankX = 104;
constexpr uint8_t kTankY = 16;
constexpr uint8_t kTankW = 20;
constexpr uint8_t kTankH = 5;
constexpr uint8_t kTankNozzleW = 3;
constexpr uint8_t kSummaryMarkX = 1;
constexpr uint8_t kSummaryMarkY = 2;
constexpr uint8_t kSummaryMarkW = 2;
constexpr uint8_t kSummaryMarkH = 3;
constexpr uint8_t kSelectMarkX = 121;
constexpr uint8_t kSelectMarkW = 3;

// Row i's text baseline is rowY(i); the invert bar extends kRowInsetY above
// it. Keep the last row clear of the hint and the hint inside the display.
constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

static_assert(station::kStationRowCount > 0, "station board needs at least one row");
static_assert(rowY(0) - kRowInsetY >= kStatusY + 7, "rows start below the status line");
static_assert(rowY(static_cast<uint8_t>(station::kStationRowCount - 1)) + kRowInvertH <= kHintY, "last row clears the hint");
static_assert(rowY(static_cast<uint8_t>(station::kActiveRowCount - 1)) + kRowInvertH <= kHintY, "active board clears the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");
static_assert(kRowX + kArchivalOfferChars * 6 <= 128, "archival offer text fits inside 128 columns");
static_assert(kNameX + kNameChars * 6 <= kIconX, "berth names clear the header icon");
static_assert(kIconX + kIconW <= 128, "header icon fits inside the display");
static_assert(kIconY + kIconH <= kDividerY, "header icon stays above the divider");
static_assert(kTankX + kTankW + kTankNozzleW <= 128, "air gauge and nozzle fit inside the display");
static_assert(kTankY + kTankH <= kRowY0, "air gauge clears the row block");
static_assert(kSummaryMarkX + kSummaryMarkW <= kRowX, "summary marker leaves the row label clear");
static_assert(kSelectMarkX + kSelectMarkW <= 128, "selection marker stays inside the display");

// Frame ABI: four main berths with three idle selections and four active
// selections, then one selected UNDOCK frame for each service berth. The
// retired row is never selected by the live board; the selector remains so the
// generated sheet can still be addressed and host-checked.
constexpr uint8_t kPanelMainBerths = 4;
constexpr uint8_t kPanelServiceBerths = 2;
constexpr uint8_t kPanelFramesPerBerth = 3 + 4;
constexpr uint8_t kPanelFrameCount = kPanelMainBerths * kPanelFramesPerBerth + kPanelServiceBerths;

// Hint order matches tools/gen_menu.STATION_HINTS so one byte selects the
// generated 128x7 row. The priority is the shipped board contract.
enum PanelHint : uint8_t {
    kPanelServiceHint = 0,
    kPanelShortLoad,
    kPanelCutterRumor,
    kPanelThrusterRumor,
    kPanelSealRumor,
    kPanelAutoload,
    kPanelStation,
};

constexpr bool panelMainBerth(uint8_t berth) {
    return berth < kPanelMainBerths;
}

// Archival frame selector: the shipped sheet still has four active selections
// (summary/retired ABANDON/WORK/UNDOCK). Runtime never selects the retired
// frame; the live board uses only summary/WORK/UNDOCK.
constexpr uint8_t kPanelArchivalActiveFirst = 1;

inline bool panelValidFrame(uint8_t berth, bool active, uint8_t selected) {
    if (panelMainBerth(berth)) {
        if (active)
            return selected >= kPanelArchivalActiveFirst && selected <= kPanelArchivalActiveFirst + 2;
        return selected < station::kStationRowCount;
    }
    return berth < kPanelMainBerths + kPanelServiceBerths && !active && selected == 0;
}

inline uint16_t panelFrame(uint8_t berth, bool active, uint8_t selected) {
    if (!panelMainBerth(berth))
        return static_cast<uint16_t>(kPanelMainBerths * kPanelFramesPerBerth + (berth - kPanelMainBerths));
    const uint16_t base = static_cast<uint16_t>(berth * kPanelFramesPerBerth);
    return active ? static_cast<uint16_t>(base + 3 + (selected - kPanelArchivalActiveFirst)) : static_cast<uint16_t>(base + selected);
}

inline uint8_t panelHint(const game::State &state) {
    if (station::serviceBerth(state))
        return kPanelServiceHint;
    if (jobs::deliveryFailed(state))
        return kPanelShortLoad;
    if (!station::jobActive(state)) {
        if (!(state.tools & exploration_data::CUTTER))
            return kPanelCutterRumor;
        if (state.docked == 1 && !(state.tools & exploration_data::THRUSTER))
            return kPanelThrusterRumor;
        if (state.docked == 2 && !(state.tools & exploration_data::SEAL))
            return kPanelSealRumor;
    }
    return station::jobActive(state) ? kPanelAutoload : kPanelStation;
}

#if defined(__AVR__)

#include "common.hpp"

// Generated AVR settlement records carry position only (name is null), so the
// station board owns the display names. Order matches the generated flash ABI.
// An if-chain, not a switch: the seven labels would otherwise become a
// RAM-resident CSWTCH table copied by the startup code. The appended Freight
// Yard label fits the alignment padding before the save partition, so every
// earlier FX offset and the save offset stay frozen.
inline uint24_t nameFor(uint8_t id) {
    if (id == 0)
        return txt_tidewall;
    if (id == 1)
        return txt_benthos;
    if (id == 2)
        return txt_cormorant;
    if (id == 3)
        return txt_deep_anchor;
    if (id == 4)
        return txt_pump_refuge;
    if (id == 5)
        return txt_lower_gantry;
    if (id == 6)
        return txt_freight_yard;
    if (id == 7)
        return txt_deep_relay;
    return txt_station;
}

inline uint24_t rowLabel(uint8_t row, const game::State &state) {
    const station::RowKind kind = station::rowKind(row, state);
    if (kind == station::kRowWork)
        return txt_work;
    if (kind == station::kRowUndock)
        return txt_undock;
    return 0;
}

// Berth silhouettes are 16x8 row-major bitmaps in program flash. The header
// icon is native chrome, not a generated panel, and every berth has its own
// silhouette so a dock reads as a place rather than a generic list.
constexpr uint8_t kIconRows = 8;
static const uint16_t kBerthIcons[kBerthIconCount][kIconRows] PROGMEM = {
    {0x739CU, 0x739CU, 0x739CU, 0x739CU, 0x739CU, 0x739CU, 0xFFFFU, 0x0000U},   // Tidewall seawall
    {0x0100U, 0x0380U, 0x0FF0U, 0x1FF8U, 0x3FFCU, 0x3FFCU, 0x1FF8U, 0x0000U},   // Benthos lamp dome
    {0x0800U, 0x0800U, 0x0FFFU, 0x0800U, 0x0800U, 0x080EU, 0x080EU, 0xFFFFU},   // Cormorant crane
    {0x0100U, 0x0380U, 0x0100U, 0x0FE0U, 0x0100U, 0x0920U, 0x1930U, 0x0000U},   // Deep Anchor
    {0x0080U, 0x0080U, 0x0880U, 0x0880U, 0x0F80U, 0x0880U, 0x38E0U, 0x3FF8U},   // Pump Refuge
    {0xFFFFU, 0x4002U, 0x2004U, 0x1008U, 0x0810U, 0x0420U, 0x0240U, 0x0000U},   // Lower Gantry
    {0x0000U, 0x0F80U, 0x0880U, 0x0F80U, 0x3EF8U, 0x2288U, 0x3EF8U, 0x0000U},   // Freight Yard crates
    {0x8001U, 0x43C2U, 0x27E4U, 0x17E8U, 0x0FF0U, 0x0180U, 0x0180U, 0x0000U},   // Deep Relay dish
};

inline void drawBerthIcon(uint8_t id) {
    if (id >= kBerthIconCount)
        return;
    for (uint8_t row = 0; row < kIconRows; ++row) {
        const uint16_t bits = pgm_read_word(&kBerthIcons[id][row]);
        for (uint8_t column = 0; column < kIconW; ++column) {
            if (bits & (0x8000U >> column))
                arduboy.drawPixel(static_cast<int16_t>(kIconX + column), static_cast<int16_t>(kIconY + row), WHITE);
        }
    }
}

// Air remains the live numeric reading; the small tank under the berth icon
// makes the fraction visible at a glance without moving the text baseline.
inline void drawAirGauge(const game::State &state) {
    arduboy.drawRect(kTankX, kTankY, kTankW, kTankH, WHITE);
    const int16_t capacity = game::airCapacity(state);
    if (capacity > 0) {
        const uint8_t innerW = static_cast<uint8_t>(kTankW - 2);
        const uint8_t fill = static_cast<uint8_t>(static_cast<uint16_t>(state.air) * innerW / capacity);
        if (fill > 0)
            arduboy.fillRect(static_cast<int16_t>(kTankX + 1), static_cast<int16_t>(kTankY + 1), fill, static_cast<uint8_t>(kTankH - 2), WHITE);
    }
    arduboy.drawFastHLine(static_cast<int16_t>(kTankX + kTankW), kTankY + 2, kTankNozzleW, WHITE);
}

inline void drawStatus(const game::State &state) {
    text::normal();
    text::at(kStatusX, kStatusY);
    text::print(txt_air_status);
    text::number(state.air);
    text::character('/');
    text::number(game::airCapacity(state));
    drawAirGauge(state);
}

inline void drawHeader(const game::State &state) {
    text::normal();
    text::at(kNameX, kHeaderY);
    text::print(nameFor(static_cast<uint8_t>(state.docked)));
    drawBerthIcon(static_cast<uint8_t>(state.docked));
    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);
    arduboy.drawFastVLine(0, static_cast<int16_t>(kDividerY - 2), 3, WHITE);
    arduboy.drawFastVLine(127, static_cast<int16_t>(kDividerY - 2), 3, WHITE);
}

// Inert contract summary: a small left marker keeps it visually distinct from
// the selectable WORK/UNDOCK rows without adding copy or a false action.
inline void drawSummaryMark(uint8_t y) {
    arduboy.fillRect(kSummaryMarkX, static_cast<int16_t>(y + kSummaryMarkY), kSummaryMarkW, kSummaryMarkH, WHITE);
}

// The inverted row still fills the width; a black wedge at the right edge is
// the console cursor and never approaches the longest row label.
inline void drawSelectionMark(uint8_t y) {
    arduboy.fillRect(kSelectMarkX, static_cast<int16_t>(y + 1), 1, 5, BLACK);
    arduboy.fillRect(static_cast<int16_t>(kSelectMarkX + 1), static_cast<int16_t>(y + 2), 1, 3, BLACK);
    arduboy.fillRect(static_cast<int16_t>(kSelectMarkX + 2), static_cast<int16_t>(y + 3), 1, 1, BLACK);
}

// Native board: name, berth icon, fixed-capacity air status, live rows and the
// generated hint row. The active summary shows only the destination; there is
// no payout.
inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    arduboy.fillRect(0, 0, 128, 64, BLACK);
    drawHeader(state);
    drawStatus(state);

    const uint8_t rows = station::rowCount(state);
    for (uint8_t row = 0; row < rows; ++row) {
        const uint8_t y = rowY(row);
        const station::RowKind kind = station::rowKind(row, state);
        if (row == selected && kind != station::kRowJob) {
            arduboy.fillRect(0, static_cast<int16_t>(y - kRowInsetY), 128, kRowInvertH, WHITE);
            text::inverted();
        } else {
            text::normal();
        }
        if (kind == station::kRowJob)
            drawSummaryMark(y);
        text::at(kRowX, y);
        if (kind == station::kRowJob)
            text::print(nameFor(state.activeJob.to));
        else
            text::print(rowLabel(row, state));
        if (row == selected && kind != station::kRowJob)
            drawSelectionMark(y);
    }
    text::normal();

    panel_view::drawHint7(station_hints, panelHint(state));
}

#endif   // __AVR__

}   // namespace station_view
