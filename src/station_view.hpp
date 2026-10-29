#pragma once

#include <stdint.h>



#include "game.hpp"
#include "station.hpp"
#include "jobs.hpp"
#include "generated/exploration.hpp"

// The full profile bakes every fixed station-board pixel into 30 opaque FX
// frames: three idle selections and four active selections for each of the four
// freight berths, plus one UNDOCK frame for each service berth. Lean/core
// profiles keep the native rows; they have different board shapes and do not
// carry the generated menu set.

#if defined(__AVR__)
#include "panel_view.hpp"
#endif

// Station board, 128x64. The layout is pure and host-checkable; the draw
// function is device-only because it uses the external FX font.
//
// Header: settlement name and the credits placeholder.
// Status: current air, including run-only shop upgrades.
// Rows:   WORK/SHOP/UNDOCK, or summary/ABANDON/WORK/SHOP/UNDOCK when a contract
//         is active. One inverted selection bar. WORK opens the single board
//         that lists restoration jobs first and the repeatable freight row
//         beneath. B always undocks.
// Hint:   A confirms the row and B always undocks. Contract acceptance
//         auto-loads the four cargo units, so the active board needs no load
//         row or screen.
namespace station_view {

constexpr uint8_t kNameX = 2;
constexpr uint8_t kHeaderY = 2;
constexpr uint8_t kCreditsX = 80;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kStatusX = 2;
constexpr uint8_t kStatusY = 15;
constexpr uint8_t kRowX = 4;
// Five rows (summary/ABANDON/WORK/SHOP/UNDOCK) must fit above the hint:
// header/status end at y=21, rows are 22+7*row, and the hint starts at 57.
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;
constexpr uint8_t kOfferChars = 20;   // "DEEP ANCHOR 240"
constexpr uint8_t kCreditsNumberX = static_cast<uint8_t>(kCreditsX + 3 * 6);
constexpr uint8_t kAirNumberX = static_cast<uint8_t>(kStatusX + 4 * 6);

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
static_assert(kCreditsX + 8 * 6 <= 128, "CR 65535 fits at the right edge");
static_assert(kRowX + kOfferChars * 6 <= 128, "offer text fits inside 128 columns");

// Frame ABI: four main berths with three idle selections (WORK/SHOP/UNDOCK)
// and four active selections (ABANDON/WORK/SHOP/UNDOCK), then one selected
// UNDOCK frame for each service berth. The inert active summary row is never a
// frame selector; its destination/reward stays a native overlay.
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
    // The shop label is folded into an if-chain instead of the switch: adding a
    // sixth RowKind would widen the compiler's jump table by two bytes of RAM.
    if (kind == station::kRowWork)
        return txt_work;
    if (kind == station::kRowShop)
        return txt_shop;
    if (kind == station::kRowUndock)
        return txt_undock;
    if (kind == station::kRowAbandon)
        return txt_abandon;
    return 0;
}

inline void printDestination(uint8_t toId, uint16_t reward) {
    text::print(nameFor(toId));
    text::character(' ');
    text::number(reward);
}

inline void printRow(uint8_t row, const game::State &state) {
    switch (station::rowKind(row, state)) {
    case station::kRowJob:
        printDestination(state.activeJob.to, state.activeJob.reward);
        break;
    case station::kRowWork:
        text::print(txt_work);
        break;
    case station::kRowAbandon:
        text::print(txt_abandon);
        break;
    default:
        text::print(rowLabel(row, state));
        break;
    }
}

inline void drawPanelValues(const game::State &state) {
    text::normal();
    text::at(kCreditsX + 3 * 6, kHeaderY);
    text::number(state.credits);
    text::at(kStatusX + 4 * 6, kStatusY);
    text::number(state.air);
    text::character('/');
    text::number(game::airCapacity(state));
}

inline void drawStatus(const game::State &state) {
    text::at(kStatusX, kStatusY);
    text::print(txt_air_status);
    text::number(state.air);
    text::character('/');
    text::number(game::airCapacity(state));
}

// Full-profile board: one opaque 128x64 frame carries the berth name, credits
// and status captions, divider, row labels and the selected bar. Live numbers,
// the active-job summary and the 7-px hint row are the only native overlays.
inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    const uint8_t berth = static_cast<uint8_t>(state.docked);
    const bool service = station::serviceBerth(state);
    const bool active = !service && station::jobActive(state);

    panel_view::draw(station_menu, panelFrame(berth, active, selected), 64);
    drawPanelValues(state);

    if (active) {
        text::at(kRowX, rowY(0));
        printDestination(state.activeJob.to, state.activeJob.reward);
    }

    panel_view::drawHint7(station_hints, panelHint(state));
}

#endif   // __AVR__

}   // namespace station_view

