#pragma once

#include <stdint.h>

#include "game.hpp"

// Station shop transactions. Pure integer C++: no Arduino headers, no heap,
// no strings, and no display access. The sketch owns the Shop screen; this
// header owns the item table, prices, purchase result, and the run-only level
// updates.
//
// Prices are sized against the M3 delivery rewards (~70-90 credits for a
// four-crate contract at the generated settlement distances). There is no
// stock besides the level counter; levels persist in voyage checkpoints
// (schema 6) and reset on a new voyage.
namespace shop {

using game::State;

enum Item : uint8_t {
    kAir,
    kItemCount
};

constexpr uint8_t kMaxLevel = 3;
constexpr uint8_t kFirstItem = 0;

constexpr uint8_t kAirPrice = 60;

// kSoldOut is the max-level result: every level has been purchased, so the
// row can only be read. Insufficient credits is a separate failure.
enum Result : uint8_t {
    kBought,
    kNoCredits,
    kSoldOut,
    kNotDocked,
    kInvalidItem
};

inline uint8_t priceFor(Item item) {
    if (item == kAir)
        return kAirPrice;
    return 0;
}

inline uint8_t levelFor(const State &state, Item item) {
    if (item == kAir)
        return state.airLevel;
    return kMaxLevel;
}

inline bool validItem(Item item) {
    return item < kItemCount;
}

// Same result vocabulary as buy(), so the UI can show the available price or
// the current failure without mutating state.
inline Result canBuy(const State &state, Item item) {
    if (!validItem(item))
        return kInvalidItem;
    if (state.docked < 0)
        return kNotDocked;
    if (levelFor(state, item) >= kMaxLevel)
        return kSoldOut;
    if (state.credits < priceFor(item))
        return kNoCredits;
    return kBought;
}

// Atomic purchase. A rejected buy changes nothing: no credits, no level, no
// refill. A successful air purchase fills the new tank.
inline Result buy(State &state, Item item) {
    const Result result = canBuy(state, item);
    if (result != kBought)
        return result;

    state.credits = static_cast<uint16_t>(state.credits - priceFor(item));
    ++state.airLevel;
    state.air = game::airCapacity(state);
    return kBought;
}

// Single-row selection is a no-op that keeps the sketch's shared row byte in
// range.
inline uint8_t selectNext(uint8_t selected) {
    (void)selected;
    return kFirstItem;
}

inline uint8_t selectPrevious(uint8_t selected) {
    (void)selected;
    return kFirstItem;
}

static_assert(kMaxLevel > 0, "shop needs at least one upgrade level");
static_assert(kItemCount == 1, "shop board has one item row");
static_assert(kAirPrice > 0, "prices are positive");

}   // namespace shop
