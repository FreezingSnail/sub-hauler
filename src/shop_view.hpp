#pragma once

#include <stdint.h>

#include "shop.hpp"

// ARCHIVAL SHOP VIEW ABI SOURCE.
//
// The runtime shop screen was removed with the currency economy. This header is
// kept only because tools/gen_menu.py reads the frozen shop layout constants to
// regenerate the shipped shop menu frames, which remain cart ABI. It is not
// included by the firmware, the host tests, or any runtime header. Do not add
// new uses.
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

constexpr uint8_t kPanelRowX = 4;
constexpr uint8_t kPanelRowY0 = 26;
constexpr uint8_t kPanelRowStep = 12;
constexpr uint8_t kPanelInsetY = 2;
constexpr uint8_t kPanelInvertH = 11;
constexpr uint8_t kPanelLevelX = 68;
constexpr uint8_t kPanelPriceX = 94;

constexpr uint8_t kPanelFramesPerSelection = shop::kMaxLevel + 1;
constexpr uint8_t kPanelFrameCount = kPanelFramesPerSelection * shop::kItemCount;

static_assert(shop::kItemCount == 1, "shop board has one row");

// Hint order matches tools/gen_menu.SHOP_HINTS.
enum PanelHint : uint8_t {
    kPanelBuy = 0,
    kPanelLowCredits,
    kPanelMaxLevel,
};

}   // namespace shop_view
