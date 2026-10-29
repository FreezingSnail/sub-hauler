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
// Rows:   AIR TANK and HULL PLATE at y=22+7*row with one 7 px invert bar.
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

// Baked full-profile layout. Two 11-px selection bars with 2-px padding and a
// 12-px row pitch give the two items more vertical separation than the native
// fallback board. Item label, level and price/MAX are baked per level pair.
constexpr uint8_t kPanelRowX = 4;
constexpr uint8_t kPanelRowY0 = 26;
constexpr uint8_t kPanelRowStep = 12;
constexpr uint8_t kPanelInsetY = 2;
constexpr uint8_t kPanelInvertH = 11;
constexpr uint8_t kPanelLevelX = 68;
constexpr uint8_t kPanelPriceX = 94;

#ifndef SH_SHOP_MENU_PANELS
#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS && SH_FEATURE_SHOP && SH_FEATURE_HULL_DAMAGE && SH_FEATURE_CAMPAIGN
#define SH_SHOP_MENU_PANELS 1
#else
#define SH_SHOP_MENU_PANELS 0
#endif
#endif

constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

constexpr uint8_t panelRowY(uint8_t row) {
    return static_cast<uint8_t>(kPanelRowY0 + row * kPanelRowStep);
}

#if SH_SHOP_MENU_PANELS
// One byte selects one of 4 x 4 level pairs x 2 selected items. Both rows'
// level and price/MAX text live in the frame; only credits, air and hull stay
// native, so the device never draws a per-row label or price.
constexpr uint8_t kPanelFramesPerSelection = (shop::kMaxLevel + 1) * (shop::kMaxLevel + 1);
constexpr uint8_t kPanelFrameCount = kPanelFramesPerSelection * shop::kItemCount;

constexpr uint16_t panelFrame(uint8_t airLevel, uint8_t hullLevel, uint8_t selected) {
    return static_cast<uint16_t>(((airLevel * (shop::kMaxLevel + 1) + hullLevel) * shop::kItemCount) + selected);
}

constexpr bool validPanelFrame(uint8_t airLevel, uint8_t hullLevel, uint8_t selected) {
    return airLevel <= shop::kMaxLevel && hullLevel <= shop::kMaxLevel && selected < shop::kItemCount;
}

// Hint order matches tools/gen_menu.SHOP_HINTS.
enum PanelHint : uint8_t {
    kPanelBuy = 0,
    kPanelLowCredits,
    kPanelMaxLevel,
};

inline uint8_t panelHint(const game::State &state, uint8_t selected) {
    const shop::Result status = shop::canBuy(state, static_cast<shop::Item>(selected));
    if (status == shop::kSoldOut)
        return kPanelMaxLevel;
    if (status == shop::kNoCredits)
        return kPanelLowCredits;
    return kPanelBuy;
}
#endif

static_assert(shop::kItemCount == 2, "shop board has two rows");
static_assert(rowY(0) - kRowInsetY >= kStatusY + 7, "rows start below the status line");
static_assert(rowY(static_cast<uint8_t>(shop::kItemCount - 1)) + kRowInvertH <= kHintY, "last row clears the hint");
static_assert(panelRowY(0) - kPanelInsetY >= kStatusY + 7, "baked rows clear the status line");
static_assert(panelRowY(static_cast<uint8_t>(shop::kItemCount - 1)) + kPanelInvertH - kPanelInsetY <= kHintY, "baked rows clear the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");
static_assert(kCreditsX + 8 * 6 <= 128, "CR 65535 fits at the right edge");
static_assert(kPriceX + 3 * 6 <= 128, "MAX fits inside 128 columns");
static_assert(kPanelPriceX + 3 * 6 <= 128, "baked MAX fits inside 128 columns");

#if defined(__AVR__)

#include "common.hpp"

inline uint24_t itemLabel(shop::Item item) {
    if (item == shop::kAir)
        return txt_air_tank;
    if (item == shop::kHull)
        return txt_hull_plate;
    return txt_item;
}

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

#if SH_SHOP_MENU_PANELS
    panel_view::draw(shop_menu, panelFrame(state.airLevel, state.hullLevel, selected), 64);
    station_view::drawPanelValues(state);
    panel_view::drawHint7(shop_hints, panelHint(state, selected));
#else
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
#endif
}

#endif   // __AVR__

}   // namespace shop_view

#endif   // SH_FEATURE_SHOP
