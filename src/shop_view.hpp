#pragma once

#include <stdint.h>

#include "game.hpp"
#include "shop.hpp"
#include "station_view.hpp"

// Shop board, 128x64. Pure layout is host-checkable; the draw function is
// device-only because it uses the external FX font.
//
// Header: SHOP and the credit balance.
// Status: current air, including the purchased capacity.
// Rows:   AIR TANK at y=22 with one 7 px invert bar. The row shows its level
//         and either the price or MAX.
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
constexpr uint8_t kRowX = 4;
constexpr uint8_t kLevelX = 76;
constexpr uint8_t kPriceX = 100;
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 57;

// Baked full-profile layout. One 11-px selection bar with 2-px padding gives
// the item more vertical separation than the native fallback board. Item label,
// level and price/MAX are baked per level. Only credits and air stay native, so
// the device never draws a per-row label or price.
constexpr uint8_t kPanelRowX = 4;
constexpr uint8_t kPanelRowY0 = 26;
constexpr uint8_t kPanelRowStep = 12;
constexpr uint8_t kPanelInsetY = 2;
constexpr uint8_t kPanelInvertH = 11;
constexpr uint8_t kPanelLevelX = 68;
constexpr uint8_t kPanelPriceX = 94;


constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

constexpr uint8_t panelRowY(uint8_t row) {
    return static_cast<uint8_t>(kPanelRowY0 + row * kPanelRowStep);
}

// One byte selects one of 4 level values x 1 selected item.
constexpr uint8_t kPanelFramesPerSelection = shop::kMaxLevel + 1;
constexpr uint8_t kPanelFrameCount = kPanelFramesPerSelection * shop::kItemCount;

constexpr uint16_t panelFrame(uint8_t airLevel, uint8_t selected) {
    return static_cast<uint16_t>(airLevel * shop::kItemCount + selected);
}

constexpr bool validPanelFrame(uint8_t airLevel, uint8_t selected) {
    return airLevel <= shop::kMaxLevel && selected < shop::kItemCount;
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

static_assert(shop::kItemCount == 1, "shop board has one row");
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
    return txt_item;
}

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    panel_view::draw(shop_menu, panelFrame(state.airLevel, selected), 64);
    station_view::drawPanelValues(state);
    panel_view::drawHint7(shop_hints, panelHint(state, selected));
}

#endif   // __AVR__

}   // namespace shop_view
