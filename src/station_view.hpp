#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_STATIONS

#include "game.hpp"
#include "station.hpp"
#if SH_FEATURE_JOBS
#include "jobs.hpp"
#endif

// Station board, 128x64. The layout is pure and host-checkable; the draw
// function is device-only because it uses the external FX font.
//
// Header: settlement name and the credits placeholder.
// Status: current air and hull, including run-only shop upgrades.
// Rows:   offers then SERVICE/SHOP/UNDOCK, or summary/ABANDON/SHOP/UNDOCK when
//         a contract is active. One inverted selection bar. The SHOP row opens
//         the shop screen; B there returns.
// Hint:   A confirms the row and B always undocks. Contract acceptance
//         auto-loads the four cargo units, so the active board needs no load
//         row or screen.
namespace station_view {

constexpr uint8_t kNameX = 2;
constexpr uint8_t kHeaderY = 2;
constexpr uint8_t kCreditsX = 80;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kStatusX = 2;
constexpr uint8_t kHullX = 72;
constexpr uint8_t kStatusY = 15;
constexpr uint8_t kRowX = 4;
#if SH_FEATURE_SHOP
// Five rows (two offers + SERVICE + SHOP + UNDOCK) must fit above the hint:
// header/status end at y=21, rows are 22+7*row, and the hint starts at 57.
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;
#else
constexpr uint8_t kRowY0 = 24;
constexpr uint8_t kRowStep = 8;
constexpr uint8_t kRowInsetY = 1;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 56;
#endif
constexpr uint8_t kOfferChars = 20; // "DEEP ANCHOR 240 5:32"

// Row i's text baseline is rowY(i); the invert bar extends kRowInsetY above
// it. Keep the last row clear of the hint and the hint inside the display.
constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

static_assert(station::kStationRowCount > 0, "station board needs at least one row");
static_assert(rowY(0) - kRowInsetY >= kStatusY + 7, "rows start below the status line");
static_assert(rowY(static_cast<uint8_t>(station::kStationRowCount - 1)) + kRowInvertH <= kHintY, "last row clears the hint");
#if SH_FEATURE_JOBS
static_assert(rowY(static_cast<uint8_t>(station::kActiveRowCount - 1)) + kRowInvertH <= kHintY, "active board clears the hint");
#endif
static_assert(kHintY + 7 <= 64, "hint stays on screen");
#if SH_FEATURE_JOBS
static_assert(kCreditsX + 8 * 6 <= 128, "CR 65535 fits at the right edge");
#endif
static_assert(kRowX + kOfferChars * 6 <= 128, "offer text fits inside 128 columns");

#if defined(__AVR__)

#include "common.hpp"

// Generated AVR settlement records carry position only (name is null), so the
// station board owns the display names. Order matches the generated flash ABI.
inline uint24_t nameFor(uint8_t id) {
    switch (id) {
    case 0:
        return txt_tidewall;
    case 1:
        return txt_benthos;
    case 2:
        return txt_cormorant;
    case 3:
        return txt_deep_anchor;
    default:
        return txt_station;
    }
}

inline uint24_t rowLabel(uint8_t row, const game::State &state) {
    const station::RowKind kind = station::rowKind(row, state);
#if SH_FEATURE_SHOP
    // The shop label is folded into an if-chain instead of the switch: adding a
    // sixth RowKind would widen the compiler's jump table by two bytes of RAM.
    if (kind == station::kRowService)
        return txt_service;
    if (kind == station::kRowShop)
        return txt_shop;
    if (kind == station::kRowUndock)
        return txt_undock;
#if SH_FEATURE_JOBS
    if (kind == station::kRowAbandon)
        return txt_abandon;
#endif
    return 0;
#else
    switch (kind) {
    case station::kRowService:
        return txt_service;
    case station::kRowUndock:
        return txt_undock;
#if SH_FEATURE_JOBS
    case station::kRowAbandon:
        return txt_abandon;
#endif
    default:
        return 0;
    }
#endif
}

#if SH_FEATURE_JOBS
inline void printClock(uint16_t ticks) {
    const uint16_t seconds = static_cast<uint16_t>(ticks / game::kStepHz);
    text::number(static_cast<uint16_t>(seconds / 60));
    text::character(':');
    const uint8_t rest = static_cast<uint8_t>(seconds % 60);
    if (rest < 10)
        text::character('0');
    text::number(rest);
}

inline void printDestination(uint8_t toId, uint16_t reward, uint16_t ticks) {
    text::print(nameFor(toId));
    text::character(' ');
    text::number(reward);
    text::character(' ');
    printClock(ticks);
}

inline void printRow(uint8_t row, const game::State &state) {
    switch (station::rowKind(row, state)) {
    case station::kRowOffer:
        if (row < state.offerCount)
            printDestination(state.offers[row].to, state.offers[row].reward, state.offers[row].deadline);
        else
            text::print(txt_offer);
        break;
    case station::kRowJob:
        printDestination(state.activeJob.to, state.activeJob.reward, jobs::remainingTicks(state));
        break;
    case station::kRowAbandon:
        text::print(txt_abandon);
        break;
    default:
        text::print(rowLabel(row, state));
        break;
    }
}
#else
inline void printRow(uint8_t row, const game::State &state) {
    text::print(rowLabel(row, state));
}
#endif

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    text::normal();
    text::at(kNameX, kHeaderY);
    text::print(nameFor(static_cast<uint8_t>(state.docked)));

#if SH_FEATURE_JOBS
    text::at(kCreditsX, kHeaderY);
    text::print(txt_credits);
    text::number(state.credits);
#endif

    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);

    text::at(kStatusX, kStatusY);
    text::print(txt_air_status);
    text::number(state.air);
    text::character('/');
    text::number(game::airCapacity(state));

#if SH_FEATURE_HULL_DAMAGE
    text::at(kHullX, kStatusY);
    text::print(txt_hull_status);
    text::number(state.hull);
    text::character('/');
    text::number(game::hullCapacity(state));
#endif

    const uint8_t rowCount = station::rowCount(state);
    for (uint8_t row = 0; row < rowCount; ++row) {
        const uint8_t y = rowY(row);
        const bool chosen = row == selected;

        if (chosen) {
            arduboy.fillRect(0, static_cast<int16_t>(y - kRowInsetY), 128, kRowInvertH, WHITE);
            // The bar is the cell background. Invert mode paints only the
            // glyph strokes black, so the cell cannot spill into the next row.
            text::inverted();
        } else {
            text::normal();
        }

        text::at(kRowX, y);
        printRow(row, state);
    }
    text::normal();

    text::at(kHintX, kHintY);
#if SH_FEATURE_JOBS
    // A failed delivery replaces the control hint until the next accept or
    // abandon. Treat it as a transient board note, not a permanent flag.
    if (jobs::deliveryFailed(state)) {
        text::print(txt_late_short);
        return;
    }
    // Acceptance already fills the hold; the active board only needs the
    // abandon/undock actions.
    if (station::jobActive(state))
        text::print(txt_autoload_hint);
    else
        text::print(txt_station_hint);
#else
    text::print(txt_station_hint);
#endif
}

#endif   // __AVR__

}   // namespace station_view

#endif   // SH_FEATURE_STATIONS
