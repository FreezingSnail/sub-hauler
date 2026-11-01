#pragma once

#include <stdint.h>

#include "game.hpp"
#include "station.hpp"
#include "jobs.hpp"
#include "generated/exploration.hpp"

// The generated station menu frames and hint rows remain cart ABI, but the
// runtime board is native: the shipped frames still carry the retired SHOP row
// and no longer match the WORK/UNDOCK and summary/ABANDON/WORK/UNDOCK shapes.
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
// Rows:   WORK/UNDOCK, or summary/ABANDON/WORK/UNDOCK when a contract is
//         active. Service-only berths show UNDOCK only. One inverted selection
//         bar. WORK opens the restoration board. B always undocks.
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
// Four rows (summary/ABANDON/WORK/UNDOCK) must fit above the hint:
// header/status end at y=21, rows are 22+7*row, and the hint starts at 57.
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;
constexpr uint8_t kArchivalOfferChars = 20;   // "DEEP ANCHOR 240"

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

inline bool panelValidFrame(uint8_t berth, bool active, uint8_t selected) {
    if (panelMainBerth(berth)) {
        if (active)
            return selected >= station::kAbandonRow && selected <= station::kActiveUndockRow;
        return selected < station::kStationRowCount;
    }
    return berth < kPanelMainBerths + kPanelServiceBerths && !active && selected == 0;
}

inline uint16_t panelFrame(uint8_t berth, bool active, uint8_t selected) {
    if (!panelMainBerth(berth))
        return static_cast<uint16_t>(kPanelMainBerths * kPanelFramesPerBerth + (berth - kPanelMainBerths));
    const uint16_t base = static_cast<uint16_t>(berth * kPanelFramesPerBerth);
    return active ? static_cast<uint16_t>(base + 3 + (selected - station::kAbandonRow)) : static_cast<uint16_t>(base + selected);
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
// An if-chain, not a switch: the six labels would otherwise become an 18-byte
// RAM-resident CSWTCH table copied by the startup code. Proportional string
// addresses make a PROGMEM table no smaller than these branches.
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
    return txt_station;
}

inline uint24_t rowLabel(uint8_t row, const game::State &state) {
    const station::RowKind kind = station::rowKind(row, state);
    if (kind == station::kRowWork)
        return txt_work;
    if (kind == station::kRowUndock)
        return txt_undock;
    if (kind == station::kRowAbandon)
        return txt_abandon;
    return 0;
}

inline void drawStatus(const game::State &state) {
    text::normal();
    text::at(kStatusX, kStatusY);
    text::print(txt_air_status);
    text::number(state.air);
    text::character('/');
    text::number(game::airCapacity(state));
}

// Native board: name, fixed-capacity air status, live rows and the generated
// hint row. The active summary shows only the destination; there is no payout.
inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    arduboy.fillRect(0, 0, 128, 64, BLACK);
    text::normal();
    text::at(kNameX, kHeaderY);
    text::print(nameFor(static_cast<uint8_t>(state.docked)));
    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);
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
        text::at(kRowX, y);
        if (kind == station::kRowJob)
            text::print(nameFor(state.activeJob.to));
        else
            text::print(rowLabel(row, state));
    }

    panel_view::drawHint7(station_hints, panelHint(state));
}

#endif   // __AVR__

}   // namespace station_view
