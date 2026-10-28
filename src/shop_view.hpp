#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_SHOP

#include "game.hpp"
#include "shop.hpp"
#include "station_view.hpp"

// Shop board, 128x64. Pure layout is host-checkable; the draw function is
// device-only because it uses the external FX font.
//
// Header: SHOP and the credit balance.
// Status: current air and hull, including the purchased capacities.
// Rows:   AIR TANK, HULL PLATE, SONAR at y=22+7*row with one 7 px invert bar.
//         Each row shows its level and either the price or MAX.
// Hint:   normal "A BUY  B BACK"; the selected row's failure is shown in the
//         same line ("LOW CREDITS" or "MAX LEVEL") so the feedback needs no
//         extra static state. B always returns to the station board.
namespace shop_view {

constexpr uint8_t kHeaderX = 2;
constexpr uint8_t kHeaderY = 2;
constexpr uint8_t kCreditsX = 78;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kStatusX = 2;
constexpr uint8_t kStatusY = 15;
constexpr uint8_t kHullX = 72;
constexpr uint8_t kRowX = 4;
constexpr uint8_t kLevelX = 76;
constexpr uint8_t kPriceX = 100;
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;

constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

static_assert(shop::kItemCount == 3, "shop board has three rows");
static_assert(rowY(0) - kRowInsetY >= kStatusY + 7, "rows start below the status line");
static_assert(rowY(static_cast<uint8_t>(shop::kItemCount - 1)) + kRowInvertH <= kHintY, "last row clears the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");
static_assert(kCreditsX + 8 * 6 <= 128, "CR 65535 fits at the right edge");
static_assert(kPriceX + 3 * 6 <= 128, "MAX fits inside 128 columns");

#if defined(__AVR__)

#include "common.hpp"

inline uint24_t itemLabel(shop::Item item) {
    if (item == shop::kAir)
        return txt_air_tank;
    if (item == shop::kHull)
        return txt_hull_plate;
    if (item == shop::kSonar)
        return txt_sonar;
    return txt_item;
}

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    if (selected >= shop::kItemCount)
        selected = shop::kFirstItem;

    text::normal();
    text::at(kHeaderX, kHeaderY);
    text::print(txt_shop);
    text::at(kCreditsX, kHeaderY);
    text::print(txt_credits);
    text::number(state.credits);

    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);
    station_view::drawStatus(state);

    for (uint8_t row = 0; row < shop::kItemCount; ++row) {
        const shop::Item item = static_cast<shop::Item>(row);
        const uint8_t y = rowY(row);
        const uint8_t level = shop::levelFor(state, item);

        if (row == selected) {
            arduboy.fillRect(0, static_cast<int16_t>(y - kRowInsetY), 128, kRowInvertH, WHITE);
            // Invert mode paints only the glyph strokes black, so the cell
            // cannot spill into the next 7 px board row.
            text::inverted();
        } else {
            text::normal();
        }

        text::at(kRowX, y);
        text::print(itemLabel(item));

        text::at(kLevelX, y);
        text::character('L');
        text::number(level);

        text::at(kPriceX, y);
        if (level >= shop::kMaxLevel)
            text::print(txt_max);
        else
            text::number(shop::priceFor(item));
    }
    text::normal();

    text::at(kHintX, kHintY);
    const shop::Result status = shop::canBuy(state, static_cast<shop::Item>(selected));
    if (status == shop::kSoldOut)
        text::print(txt_max_level);
    else if (status == shop::kNoCredits)
        text::print(txt_low_credits);
    else
        text::print(txt_buy_hint);
}

#endif   // __AVR__

}   // namespace shop_view

#endif   // SH_FEATURE_SHOP
