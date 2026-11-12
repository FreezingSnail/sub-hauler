#pragma once

#include <stdint.h>



#include "campaign.hpp"
#include "game.hpp"
#include "panel_view.hpp"

// Homecoming presentation: two input-advanced cards. Both fixed cards (restored
// stations, voyage complete) are baked 128x64 FX panels; the retired voyage-stats overlay
// is no longer presented. Card advancement is pure so the host tests can pin the
// sequence independently of the external font.
namespace ending_view {

constexpr uint8_t kCardNetwork = 0;
constexpr uint8_t kCardThanks = 1;
constexpr uint8_t kCardCount = 2;

static_assert(kCardThanks + 1 == kCardCount, "two ending cards are frozen");

inline uint8_t nextCard(uint8_t card) {
    if (card + 1 < kCardCount)
        return static_cast<uint8_t>(card + 1);
    return kCardCount;
}

#if defined(__AVR__)

#include "common.hpp"

inline void draw(uint8_t card) {
    if (card == kCardThanks)
        panel_view::draw(panel_ending_thanks);
    else
        panel_view::draw(panel_ending_network);
}

#endif   // __AVR__

}   // namespace ending_view

