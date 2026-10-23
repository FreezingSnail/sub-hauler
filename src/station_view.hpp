#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_STATIONS

#include "game.hpp"
#include "station.hpp"

// Station board, 128x64. The layout is pure and host-checkable; the draw
// function is device-only because it prints through the Arduboy font with
// PROGMEM strings.
//
// Header: settlement name and the credits placeholder.
// Status: current air and hull.
// Rows:   SERVICE then UNDOCK, one inverted selection bar.
// Hint:   A confirms the row, B always undocks.
namespace station_view {

constexpr uint8_t kNameX = 2;
constexpr uint8_t kHeaderY = 2;
constexpr uint8_t kCreditsX = 96;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kStatusX = 2;
constexpr uint8_t kHullX = 72;
constexpr uint8_t kStatusY = 15;
constexpr uint8_t kRowX = 4;
constexpr uint8_t kRowY0 = 29;
constexpr uint8_t kRowStep = 12;
constexpr uint8_t kRowInsetY = 2;
constexpr uint8_t kRowInvertH = 10;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 56;

// Row i's text baseline is rowY(i); the invert bar extends kRowInsetY above
// it. Keep the last row clear of the hint and the hint inside the display.
constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

static_assert(station::kStationRowCount > 0, "station board needs at least one row");
static_assert(rowY(0) > kStatusY + 7, "rows start below the status line");
static_assert(rowY(static_cast<uint8_t>(station::kStationRowCount - 1)) + 7 < kHintY, "last row clears the hint");
static_assert(kHintY + 7 < 64, "hint stays on screen");
static_assert(kCreditsX + 4 * 6 <= 128, "CR 0 fits at the right edge");

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

inline const __FlashStringHelper *rowLabel(uint8_t row) {
    switch (station::rowKind(row)) {
    case station::kRowService:
        return F("SERVICE");
    case station::kRowUndock:
        return F("UNDOCK");
    default:
        return F("OFFER");
    }
}

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    arduboy.setCursor(kNameX, kHeaderY);
    arduboy.print(nameFor(static_cast<uint8_t>(state.docked)));

    // Credits are a placeholder until M3 jobs lands the economy.
    arduboy.setCursor(kCreditsX, kHeaderY);
    arduboy.print(F("CR 0"));

    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);

    arduboy.setCursor(kStatusX, kStatusY);
    arduboy.print(F("AIR "));
    arduboy.print(state.air);
    arduboy.print('/');
    arduboy.print(game::kAirCapacity);

#if SH_FEATURE_HULL_DAMAGE
    arduboy.setCursor(kHullX, kStatusY);
    arduboy.print(F("HULL "));
    arduboy.print(state.hull);
    arduboy.print('/');
    arduboy.print(game::kHullMax);
#endif

    for (uint8_t row = 0; row < station::kStationRowCount; ++row) {
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
        arduboy.print(rowLabel(row));
    }
    arduboy.setTextColor(WHITE);
    arduboy.setTextBackground(BLACK);

    arduboy.setCursor(kHintX, kHintY);
    arduboy.print(F("UP/DN A OK B OUT"));
}

#endif   // __AVR__

}   // namespace station_view

#endif   // SH_FEATURE_STATIONS
