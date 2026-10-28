#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_CAMPAIGN

#include "campaign.hpp"
#include "game.hpp"

// Homecoming presentation: three input-advanced cards derived only from saved
// campaign counters and masks. The network card is a compact native diagram;
// no extra full-screen art or FX asset is required. Card advancement is pure so
// the host tests can pin the sequence independently of the external font.
namespace ending_view {

constexpr uint8_t kCardNetwork = 0;
constexpr uint8_t kCardThanks = 1;
constexpr uint8_t kCardStats = 2;
constexpr uint8_t kCardCount = 3;
constexpr uint8_t kCoveCount = 8;

static_assert(kCardStats + 1 == kCardCount, "three ending cards are frozen");

inline uint8_t nextCard(uint8_t card) {
    if (card + 1 < kCardCount)
        return static_cast<uint8_t>(card + 1);
    return kCardCount;
}

inline uint8_t coveCount(const game::State &state) {
    uint8_t count = 0;
    uint8_t bits = state.pickups;
    while (bits != 0) {
        count = static_cast<uint8_t>(count + (bits & 1u));
        bits >>= 1;
    }
    return count > kCoveCount ? kCoveCount : count;
}

inline uint32_t voyageSeconds(const game::State &state) {
    return state.voyageTicks / game::kStepHz;
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
constexpr uint8_t kStatsTimeY = 16;
constexpr uint8_t kStatsCovesY = 28;
constexpr uint8_t kStatsJobsY = 40;

// The hint owns the final 8-pixel row. Every content row above it must clear
// the hint baseline by a full glyph cell, including the stat block and thanks
// card. These asserts compile on host and device so a layout edit cannot ship
// overlapping rows.
static_assert(kHintY + kGlyphH <= 64, "ending hint stays on screen");
static_assert(kNetworkOnlineY + kGlyphH <= kHintY, "network caption clears the hint");
static_assert(kThanksKeepY + kGlyphH <= kHintY, "thanks caption clears the hint");
static_assert(kStatsJobsY + kGlyphH <= kHintY, "stats jobs clear the hint");
static_assert(kStatsCovesY + kGlyphH <= kStatsJobsY, "stats rows do not overlap");
static_assert(kThanksFromY + kGlyphH <= kThanksFrom2Y, "thanks rows do not overlap");
static_assert(kThanksFrom2Y + kGlyphH <= kThanksKeepY, "thanks rows do not overlap");

#if defined(__AVR__)

#include "common.hpp"

inline void hint(uint24_t label) {
    text::normal();
    text::at(3, kHintY);
    text::print(label);
}

inline void drawNetwork() {
    text::normal();
    text::at(4, kTitleY);
    text::print(txt_network_lit);
    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);

    // Four lit stations on one line: the network the campaign restored.
    for (uint8_t i = 0; i + 1 < 4; ++i) {
        const int16_t x0 = static_cast<int16_t>(kNodeX0 + i * kNodeStep + kNodeSize);
        const int16_t x1 = static_cast<int16_t>(kNodeX0 + (i + 1) * kNodeStep);
        arduboy.drawFastHLine(x0, kNodeY + kNodeSize / 2, static_cast<int16_t>(x1 - x0), WHITE);
    }
    for (uint8_t i = 0; i < 4; ++i)
        arduboy.fillRect(static_cast<int16_t>(kNodeX0 + i * kNodeStep), kNodeY, kNodeSize, kNodeSize, WHITE);

    text::at(4, kNetworkOnlineY);
    text::print(txt_net_online);
    hint(txt_a_next);
}

inline void drawThanks() {
    text::normal();
    text::at(4, kThanksY);
    text::print(txt_thank_you);
    text::at(4, kThanksFromY);
    text::print(txt_thanks_from);
    text::at(4, kThanksFrom2Y);
    text::print(txt_thanks_from2);
    text::at(4, kThanksKeepY);
    text::print(txt_keep_exploring);
    hint(txt_a_next);
}

inline void drawNumberLine(uint8_t y, uint24_t label, uint16_t value) {
    text::at(4, y);
    text::print(label);
    text::character(' ');
    text::number(value);
}

inline void drawStats(const game::State &state) {
    text::normal();
    text::at(4, kTitleY);
    text::print(txt_voyage_stats);
    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);

    uint32_t seconds = voyageSeconds(state);
    if (seconds > 59999u)
        seconds = 59999u;
    const uint16_t minutes = static_cast<uint16_t>(seconds / 60u);
    const uint8_t rest = static_cast<uint8_t>(seconds % 60u);
    text::at(4, kStatsTimeY);
    text::print(txt_time);
    text::character(' ');
    text::number(minutes);
    text::character(':');
    if (rest < 10)
        text::character('0');
    text::number(rest);

    text::at(4, kStatsCovesY);
    text::print(txt_coves);
    text::character(' ');
    text::number(coveCount(state));
    text::character('/');
    text::number(kCoveCount);

    drawNumberLine(kStatsJobsY, txt_jobs, state.delivered);
    hint(txt_a_finish);
}

inline void draw(uint8_t card, const game::State &state) {
    if (card == kCardNetwork)
        drawNetwork();
    else if (card == kCardThanks)
        drawThanks();
    else
        drawStats(state);
}

#endif   // __AVR__

}   // namespace ending_view

#endif   // SH_FEATURE_CAMPAIGN
