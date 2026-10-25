#pragma once

#include <stdint.h>

#include "game.hpp"

#if defined(__AVR__)
#include "common.hpp"
#endif

// Compact pilot HUD. Drawn after the cave and sub but before the one
// FX::display transaction, so it never competes with cart reads.
//
// Air is a top-left bar, ballast is a left-edge gauge with a neutral notch,
// cargo is a row of four pips, and the bottom line gives the D-pad contract.
// Labels come from the external FX text section; there is no float, heap, or
// String. Placement keeps the 26x8 boat (screen x ~44, y ~32) clear.
namespace hud {

constexpr uint8_t kAirLabelX = 1;
constexpr uint8_t kAirLabelY = 1;
constexpr uint8_t kAirX = 22;
constexpr uint8_t kAirY = 2;
constexpr uint8_t kAirW = 32;
constexpr uint8_t kAirH = 5;

constexpr uint8_t kGaugeX = 2;
constexpr uint8_t kGaugeY = 13;
constexpr uint8_t kGaugeW = 5;
constexpr uint8_t kGaugeH = 34;

constexpr uint8_t kCargoLabelX = 78;
constexpr uint8_t kCargoLabelY = 1;
constexpr uint8_t kCargoX = 99;
constexpr uint8_t kCargoY = 2;
constexpr uint8_t kCargoStep = 5;
constexpr uint8_t kCargoPip = 4;

constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 56;

#if defined(__AVR__)
inline void draw(const game::State &state) {
    // Air bar: outline plus a white fill proportional to the tank.
    text::normal();
    text::at(kAirLabelX, kAirLabelY);
    text::print(txt_air);
    arduboy.drawRect(kAirX, kAirY, kAirW, kAirH, WHITE);
    const uint8_t airInnerW = static_cast<uint8_t>(kAirW - 2);
    const uint8_t airFill = static_cast<uint8_t>(static_cast<uint16_t>(state.air) * airInnerW / game::airCapacity(state));
    if (airFill > 0)
        arduboy.fillRect(static_cast<int16_t>(kAirX + 1), static_cast<int16_t>(kAirY + 1), airFill, static_cast<uint8_t>(kAirH - 2), WHITE);

    // Ballast gauge: white fill grows from the bottom, the notch marks neutral
    // trim for the current cargo load.
    arduboy.drawRect(kGaugeX, kGaugeY, kGaugeW, kGaugeH, WHITE);
    const uint8_t gaugeInnerH = static_cast<uint8_t>(kGaugeH - 2);
    const uint8_t ballastFill = static_cast<uint8_t>(static_cast<uint16_t>(state.ballast) * gaugeInnerH / game::kBallastMax);
    if (ballastFill > 0) {
        const uint8_t fillY = static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - ballastFill);
        arduboy.fillRect(static_cast<int16_t>(kGaugeX + 1), fillY, static_cast<uint8_t>(kGaugeW - 2), ballastFill, WHITE);
    }
    const int16_t neutral = game::neutralFor(state.cargo);
    const uint8_t notchY = static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - static_cast<uint8_t>(neutral * gaugeInnerH / game::kBallastMax));
    arduboy.fillRect(static_cast<int16_t>(kGaugeX + kGaugeW), notchY, 3, 1, WHITE);

    // Cargo pips: four outline boxes, one filled per loaded unit.
    text::at(kCargoLabelX, kCargoLabelY);
    text::print(txt_cgo);
    for (uint8_t i = 0; i < game::kCargoMax; ++i) {
        const int16_t x = static_cast<int16_t>(kCargoX + i * kCargoStep);
        arduboy.drawRect(x, kCargoY, kCargoPip, kCargoPip, WHITE);
        if (i < state.cargo)
            arduboy.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(kCargoY + 1), static_cast<uint8_t>(kCargoPip - 2), static_cast<uint8_t>(kCargoPip - 2), WHITE);
    }

    // Locked M2 flight contract: D-pad only; A/B stay reserved. Two short
    // segments keep both thrust and ballast controls readable in one text row.
    text::at(kHintX, kHintY);
    text::print(txt_thrust);
    text::at(65, kHintY);
    text::print(txt_trim);
}

#endif   // __AVR__

}   // namespace hud
