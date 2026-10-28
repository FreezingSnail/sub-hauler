#pragma once

#include <stdint.h>

#include "game.hpp"
#include "world.hpp"
#if SH_FEATURE_EXPLORATION
#include "exploration_view.hpp"
#endif

#if defined(__AVR__)
#include "cave_render.hpp"
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

// Opaque HUD plates. The map uses black rock and white water, so each cluster
// clears its own backing before drawing to keep labels readable over either.
constexpr uint8_t kTopBackingX = 0;
constexpr uint8_t kTopBackingY = 0;
constexpr uint8_t kTopBackingW = 128;
constexpr uint8_t kTopBackingH = 9;

constexpr uint8_t kGaugeBackingX = 0;
constexpr uint8_t kGaugeBackingY = static_cast<uint8_t>(kGaugeY - 2);
constexpr uint8_t kGaugeBackingW = static_cast<uint8_t>(kGaugeX + kGaugeW + 4);
constexpr uint8_t kGaugeBackingH = static_cast<uint8_t>(kGaugeH + 4);

constexpr uint8_t kHintBackingX = 0;
constexpr uint8_t kHintBackingY = static_cast<uint8_t>(kHintY - 1);
constexpr uint8_t kHintBackingW = 128;
constexpr uint8_t kHintBackingH = 9;

#if defined(__AVR__)
inline void draw(const game::State &state) {
    // Black plates first, then the HUD clusters on top.
    cave::fillRect(Arduboy2Base::sBuffer, kTopBackingX, kTopBackingY, kTopBackingW, kTopBackingH, false);
    cave::fillRect(Arduboy2Base::sBuffer, kGaugeBackingX, kGaugeBackingY, kGaugeBackingW, kGaugeBackingH, false);

    // Air bar: outline plus a white fill proportional to the tank.
    text::normal();
    text::at(kAirLabelX, kAirLabelY);
    text::print(txt_air);
    arduboy.drawRect(kAirX, kAirY, kAirW, kAirH, WHITE);
    const uint8_t airInnerW = static_cast<uint8_t>(kAirW - 2);
    const uint8_t airFill = static_cast<uint8_t>(static_cast<uint16_t>(state.air) * airInnerW / game::airCapacity(state));
    if (airFill > 0)
        cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(kAirX + 1), static_cast<int16_t>(kAirY + 1), airFill, static_cast<uint8_t>(kAirH - 2), true);

#if SH_FEATURE_EXPLORATION
    if (state.tools & exploration_data::CUTTER)
        exploration_view::glyph(Arduboy2Base::sBuffer, 57, 2, 1);
    if (state.tools & exploration_data::SEAL)
        exploration_view::glyph(Arduboy2Base::sBuffer, 64, 2, 2);
    if (state.tools & exploration_data::THRUSTER)
        exploration_view::glyph(Arduboy2Base::sBuffer, 71, 2, 3);
#endif
    // Ballast gauge: white fill grows from the bottom, the notch marks neutral
    // trim for the current cargo load.
    arduboy.drawRect(kGaugeX, kGaugeY, kGaugeW, kGaugeH, WHITE);
    const uint8_t gaugeInnerH = static_cast<uint8_t>(kGaugeH - 2);
    const uint8_t ballastFill = static_cast<uint8_t>(static_cast<uint16_t>(state.ballast) * gaugeInnerH / game::kBallastMax);
    if (ballastFill > 0) {
        const uint8_t fillY = static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - ballastFill);
        cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(kGaugeX + 1), fillY, static_cast<uint8_t>(kGaugeW - 2), ballastFill, true);
    }
    const int16_t neutral = game::neutralForNormalized(state.cargo);
    const uint8_t notchY = static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - static_cast<uint8_t>(neutral * gaugeInnerH / game::kBallastMax));
    cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(kGaugeX + kGaugeW), notchY, 3, 1, true);

    // Cargo pips: four outline boxes, one filled per loaded unit.
    text::at(kCargoLabelX, kCargoLabelY);
    text::print(txt_cgo);
    for (uint8_t i = 0; i < game::kCargoMax; ++i) {
        const int16_t x = static_cast<int16_t>(kCargoX + i * kCargoStep);
        arduboy.drawRect(x, kCargoY, kCargoPip, kCargoPip, WHITE);
        if (i < state.cargo)
            cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(kCargoY + 1), static_cast<uint8_t>(kCargoPip - 2), static_cast<uint8_t>(kCargoPip - 2), true);
    }

    cave::fillRect(Arduboy2Base::sBuffer, kHintBackingX, kHintBackingY, kHintBackingW, kHintBackingH, false);
#if SH_DEV_MODE
    text::at(kHintX, kHintY);
    text::print(txt_dev_noclip);
    return;
#endif
#if SH_FEATURE_EXPLORATION
    uint24_t notice = 0;
    if (state.discoveryTicks) {
#if SH_FEATURE_CAMPAIGN
        if (state.lastDiscovery == exploration::kNone) {
            notice = txt_survey_recorded;
        } else
#endif
        {
            const uint8_t kind = exploration_data::pickup(state.lastDiscovery).kind;
            notice = kind == 1 ? txt_found_cutter : kind == 2 ? txt_found_seal : kind == 3 ? txt_found_thruster : kind == 4 ? txt_found_credits : kind == 5 ? txt_found_hull : txt_found_lens;
        }
    } else if (state.contactId != exploration::kNone) {
#if SH_FEATURE_CAMPAIGN
        if (state.contactId >= exploration::kSurveyBase)
            notice = txt_survey_hint;
        else
#endif
            notice = state.contactId < exploration_data::PICKUP_COUNT ? txt_salvage_hint : txt_cut_hint;
    } else {
        for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
            const exploration_data::Gate g = exploration_data::gate(i);
            if (!exploration::closed(state, i, g) || !exploration::nearGate(state, g, 24))
                continue;
            notice = (g.required == exploration_data::CUTTER && (state.tools & exploration_data::CUTTER))         ? txt_cut_hint
                     : ((g.required & exploration_data::SEAL) && !(state.tools & exploration_data::SEAL))         ? txt_need_seal
                     : ((g.required & exploration_data::THRUSTER) && !(state.tools & exploration_data::THRUSTER)) ? txt_need_thruster
                                                                                                                  : txt_need_cutter;
        }
#if SH_FEATURE_CAMPAIGN
        // An inactive service mooring still reads as a clue, never as a
        // dockable station. It never blocks travel or hides a pickup.
        if (notice == 0) {
            for (uint8_t i = 0; i < world::serviceBerthCount(); ++i) {
                world::Berth berth;
                if (!world::serviceBerthInfo(i, berth) || (state.campaign & berth.milestone) != 0)
                    continue;
                const int16_t dx = static_cast<int16_t>(berth.pixel_x - state.x);
                const int16_t dy = static_cast<int16_t>(berth.pixel_y - state.y);
                if (dx > -40 && dx < 40 && dy > -32 && dy < 32) {
                    notice = berth.milestone == 2 ? txt_repair_pump : txt_repair_lift;
                    break;
                }
            }
        }
#endif
    }
    if (notice) {
        text::at(kHintX, kHintY);
        text::print(notice);
        if (state.discoveryTicks) {
            const exploration_data::Pickup p = exploration_data::pickup(state.lastDiscovery);
            if (p.kind == 4) {
                text::character(' ');
                text::number(p.amount);
            }
        }
        return;
    }
#endif
    // Locked M2 flight contract: D-pad only; A/B stay reserved. Two short
    // segments keep both thrust and ballast controls readable in one text row.
    text::at(kHintX, kHintY);
    text::print(txt_thrust);
    text::at(65, kHintY);
    text::print(txt_trim);
}

#endif   // __AVR__

}   // namespace hud
