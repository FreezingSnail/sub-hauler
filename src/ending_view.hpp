#pragma once

#include <stdint.h>



#include "campaign.hpp"
#include "game.hpp"
#include "panel_view.hpp"

// Homecoming presentation: two input-advanced cards. Both fixed cards (lit
// network, thanks) are baked 128x64 FX panels; the retired voyage-stats overlay
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

constexpr uint8_t kTitleY = 2;
constexpr uint8_t kDividerY = 12;
constexpr uint8_t kHintY = 56;
constexpr uint8_t kNodeY = 26;
constexpr uint8_t kNodeX0 = 20;
constexpr uint8_t kNodeStep = 29;
constexpr uint8_t kNodeSize = 7;
constexpr uint8_t kGlyphH = 8;
constexpr uint8_t kNetworkOnlineY = 46;
constexpr uint8_t kThanksY = 8;
constexpr uint8_t kThanksFromY = 22;
constexpr uint8_t kThanksFrom2Y = 32;
constexpr uint8_t kThanksKeepY = 44;

// The hint owns the final 8-pixel row. Every content row above it must clear
// the hint baseline by a full glyph cell, including the thanks card. These
// asserts compile on host and device so a layout edit cannot ship overlapping
// rows.
static_assert(kHintY + kGlyphH <= 64, "ending hint stays on screen");
static_assert(kNetworkOnlineY + kGlyphH <= kHintY, "network caption clears the hint");
static_assert(kThanksKeepY + kGlyphH <= kHintY, "thanks caption clears the hint");
static_assert(kThanksFromY + kGlyphH <= kThanksFrom2Y, "thanks rows do not overlap");
static_assert(kThanksFrom2Y + kGlyphH <= kThanksKeepY, "thanks rows do not overlap");

#if defined(__AVR__)

#include "common.hpp"

inline void hint(uint24_t label) {
    text::normal();
    text::at(3, kHintY);
    text::print(label);
}

inline void draw(uint8_t card, const game::State &) {
    if (card == kCardThanks)
        panel_view::draw(panel_ending_thanks);
    else
        panel_view::draw(panel_ending_network);
}

#endif   // __AVR__

}   // namespace ending_view

