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
// function is device-only because it prints through the Arduboy font with
// PROGMEM strings.
//
// Header: settlement name and the credits placeholder.
// Status: current air and hull, including run-only shop upgrades.
// Rows:   offers then SERVICE/SHOP/UNDOCK, or summary/LOAD
//         CARGO/ABANDON/SHOP/UNDOCK when a contract is active. One inverted
//         selection bar. The SHOP row opens the shop screen; B there returns.
// Hint:   A confirms the row and B always undocks. With SH_FEATURE_JOBS the
//         active board reopens the cargo screen from LOAD CARGO.
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
inline const __FlashStringHelper *nameFor(uint8_t id) {
    switch (id) {
    case 0:
        return F("TIDEWALL");
    case 1:
        return F("BENTHOS");
    case 2:
        return F("CORMORANT");
    case 3:
        return F("DEEP ANCHOR");
    default:
        return F("STATION");
    }
}

inline const __FlashStringHelper *rowLabel(uint8_t row, const game::State &state) {
    const station::RowKind kind = station::rowKind(row, state);
#if SH_FEATURE_SHOP
    // The shop label is folded into an if-chain instead of the switch: adding a
    // sixth RowKind would widen the compiler's jump table by two bytes of RAM.
    if (kind == station::kRowService)
        return F("SERVICE");
    if (kind == station::kRowShop)
        return F("SHOP");
    if (kind == station::kRowUndock)
        return F("UNDOCK");
#if SH_FEATURE_JOBS
    if (kind == station::kRowAbandon)
        return F("ABANDON");
#if SH_FEATURE_CARGO_LOAD
    if (kind == station::kRowLoad)
        return F("LOAD CARGO");
#endif
#endif
    return nullptr;
#else
    switch (kind) {
    case station::kRowService:
        return F("SERVICE");
    case station::kRowUndock:
        return F("UNDOCK");
#if SH_FEATURE_JOBS
    case station::kRowAbandon:
        return F("ABANDON");
#if SH_FEATURE_CARGO_LOAD
    case station::kRowLoad:
        return F("LOAD CARGO");
#endif
#endif
    default:
        return nullptr;
    }
#endif
}

#if SH_FEATURE_JOBS
inline void printClock(uint16_t ticks) {
    const uint16_t seconds = static_cast<uint16_t>(ticks / game::kStepHz);
    arduboy.print(static_cast<uint16_t>(seconds / 60));
    arduboy.print(':');
    const uint8_t rest = static_cast<uint8_t>(seconds % 60);
    if (rest < 10)
        arduboy.print('0');
    arduboy.print(rest);
}

inline void printDestination(uint8_t toId, uint16_t reward, uint16_t ticks) {
    arduboy.print(nameFor(toId));
    arduboy.print(' ');
    arduboy.print(reward);
    arduboy.print(' ');
    printClock(ticks);
}

inline void printRow(uint8_t row, const game::State &state) {
    switch (station::rowKind(row, state)) {
    case station::kRowOffer:
        if (row < state.offerCount)
            printDestination(state.offers[row].to, state.offers[row].reward, state.offers[row].deadline);
        else
            arduboy.print(F("OFFER"));
        break;
    case station::kRowJob:
        printDestination(state.activeJob.to, state.activeJob.reward, jobs::remainingTicks(state));
        break;
    case station::kRowAbandon:
        arduboy.print(F("ABANDON"));
        break;
    default:
        arduboy.print(rowLabel(row, state));
        break;
    }
}
#else
inline void printRow(uint8_t row, const game::State &state) {
    arduboy.print(rowLabel(row, state));
}
#endif

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    arduboy.setCursor(kNameX, kHeaderY);
    arduboy.print(nameFor(static_cast<uint8_t>(state.docked)));

#if SH_FEATURE_JOBS
    arduboy.setCursor(kCreditsX, kHeaderY);
    arduboy.print(F("CR "));
    arduboy.print(state.credits);
#endif

    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);

    arduboy.setCursor(kStatusX, kStatusY);
    arduboy.print(F("AIR "));
    arduboy.print(state.air);
    arduboy.print('/');
    arduboy.print(game::airCapacity(state));

#if SH_FEATURE_HULL_DAMAGE
    arduboy.setCursor(kHullX, kStatusY);
    arduboy.print(F("HULL "));
    arduboy.print(state.hull);
    arduboy.print('/');
    arduboy.print(game::hullCapacity(state));
#endif

    const uint8_t rowCount = station::rowCount(state);
    for (uint8_t row = 0; row < rowCount; ++row) {
        const uint8_t y = rowY(row);
        const bool chosen = row == selected;

        if (chosen) {
            arduboy.fillRect(0, static_cast<int16_t>(y - kRowInsetY), 128, kRowInvertH, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setTextBackground(WHITE);
        } else {
            arduboy.setTextColor(WHITE);
            arduboy.setTextBackground(BLACK);
        }

        arduboy.setCursor(kRowX, y);
        printRow(row, state);
    }
    arduboy.setTextColor(WHITE);
    arduboy.setTextBackground(BLACK);

    arduboy.setCursor(kHintX, kHintY);
#if SH_FEATURE_JOBS
#if SH_FEATURE_CARGO_LOAD
    // The M5 board reopens the cargo screen from LOAD CARGO, so the same row
    // hint works for offer and active shapes.
    arduboy.print(F("UP/DN A OK B OUT"));
#else
    // CARGO_LOAD=0 keeps the M3 fallback: acceptance already fills the hold.
    if (station::jobActive(state))
        arduboy.print(F("AUTO-LOAD  B OUT"));
    else
        arduboy.print(F("UP/DN A OK B OUT"));
#endif
#else
    arduboy.print(F("UP/DN A OK B OUT"));
#endif
}

#endif   // __AVR__

}   // namespace station_view

#endif   // SH_FEATURE_STATIONS
