#pragma once

#include <stdint.h>

// ARCHIVAL SHOP ABI SOURCE.
//
// The runtime shop was removed with the currency economy. This header is kept
// only because tools/gen_menu.py reads the frozen shop price/level table to
// regenerate the shipped shop menu frames, which remain cart ABI. It is not
// included by the firmware, the host tests, or any runtime header. Do not add
// new uses.

namespace shop {

// One donated air-tank row, three levels, flat price. Frozen.
enum Item : uint8_t {
    kAir,
    kItemCount
};

constexpr uint8_t kMaxLevel = 3;
constexpr uint8_t kFirstItem = 0;
constexpr uint8_t kAirPrice = 60;

static_assert(kMaxLevel > 0, "shop needs at least one upgrade level");
static_assert(kItemCount == 1, "shop board has one item row");
static_assert(kAirPrice > 0, "prices are positive");

}   // namespace shop
