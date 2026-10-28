#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_SHOP

#include "game.hpp"
#if SH_FEATURE_HULL_DAMAGE
#include "damage.hpp"
#endif

// Station shop transactions. Pure integer C++: no Arduino headers, no heap,
// no strings, and no display access. The sketch owns the Shop screen; this
// header owns the item table, prices, purchase result, and the run-only level
// updates.
//
// Prices are sized against the M3 delivery rewards (~70-90 credits for a
// four-crate contract at the generated settlement distances). One delivery
// buys the first upgrade; buying all nine levels costs 540 credits, about
// seven deliveries. There is no stock besides the level counter and no save:
// levels last for the current run only.
namespace shop {

using game::State;

enum Item : uint8_t {
    kAir,
    kHull,
    kSonar,
    kItemCount
};

constexpr uint8_t kMaxLevel = 3;
constexpr uint8_t kFirstItem = 0;

constexpr uint8_t kAirPrice = 60;
constexpr uint8_t kHullPrice = 70;
constexpr uint8_t kSonarPrice = 50;

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
    if (item == kHull)
        return kHullPrice;
    if (item == kSonar)
        return kSonarPrice;
    return 0;
}

inline uint8_t levelFor(const State &state, Item item) {
    if (item == kAir)
        return state.airLevel;
    if (item == kHull)
        return state.hullLevel;
    if (item == kSonar)
        return state.sonarLevel;
    return kMaxLevel;
}

inline uint8_t &levelRef(State &state, Item item) {
    if (item == kHull)
        return state.hullLevel;
    if (item == kSonar)
        return state.sonarLevel;
    return state.airLevel;
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
// refill. A successful air purchase fills the new tank; a hull purchase
// repairs to the new maximum. Sonar has no immediate state beyond its level,
// which the ping rules already consume via sonar::costForLevel() and
// sonar::radiusForLevel().
inline Result buy(State &state, Item item) {
    const Result result = canBuy(state, item);
    if (result != kBought)
        return result;

    state.credits = static_cast<uint16_t>(state.credits - priceFor(item));
    ++levelRef(state, item);

    if (item == kAir)
        state.air = game::airCapacity(state);
#if SH_FEATURE_HULL_DAMAGE
    else if (item == kHull)
        damage::repairFull(state);
#endif

    return kBought;
}

// Three-row selection wrap. The sketch reuses its station row byte as the
// shop selection, so no extra static state is needed.
inline uint8_t selectNext(uint8_t selected) {
    if (selected >= static_cast<uint8_t>(kItemCount - 1))
        return kFirstItem;
    return static_cast<uint8_t>(selected + 1);
}

inline uint8_t selectPrevious(uint8_t selected) {
    if (selected == kFirstItem || selected >= kItemCount)
        return static_cast<uint8_t>(kItemCount - 1);
    return static_cast<uint8_t>(selected - 1);
}

static_assert(kMaxLevel > 0, "shop needs at least one upgrade level");
static_assert(kItemCount == 3, "shop board has three item rows");
static_assert(kAirPrice > 0 && kHullPrice > 0 && kSonarPrice > 0, "prices are positive");

}   // namespace shop

#endif   // SH_FEATURE_SHOP
