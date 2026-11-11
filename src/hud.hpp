#pragma once

#include <stdint.h>

#include "game.hpp"
#include "world.hpp"
#include "exploration_view.hpp"

#if defined(__AVR__)
#include "cave_render.hpp"
#include "common.hpp"
#include "generated/hud_sprites.hpp"
#endif

// Compact pilot HUD. Drawn after the cave and sub but before the one
// FX::display transaction, so it never competes with cart reads.
//
// Air is a top-left bar and ballast is a left-edge gauge with a neutral notch
// tied to the current load. The bottom row is reserved for contextual notices;
// with no notice the map stays visible. Labels come from the external FX text
// section; there is no float, heap, or String. Placement keeps the 26x8 boat
// (screen x ~44, y ~32) clear.
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

constexpr uint8_t kHintX = 3;
constexpr uint8_t kHintY = 56;

// Pixel row of the neutral-trim notch for a normalized cargo load. Pure so the
// host tests can pin the empty-vs-loaded position; draw() uses the same value.
inline uint8_t neutralNotchY(uint8_t cargo) {
    const uint8_t gaugeInnerH = static_cast<uint8_t>(kGaugeH - 2);
    const int16_t neutral = game::neutralForNormalized(cargo);
    return static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - static_cast<uint8_t>(neutral * gaugeInnerH / game::kBallastMax));
}

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

// Notice frame order is generated ABI (gen_hud.NOTICE_STRINGS), and the
// generated sheet is contiguous: notice value N selects frame N - 1. The
// retired credits, survey-hint and cut-hint frames were removed by
// sub-hauler-jxk.3, so no unused stamp remains between live notices.
enum Notice : uint8_t {
    kNoticeNone = 0,
    kNoticeSurveyRecorded,
    kNoticeFoundCutter,
    kNoticeFoundSeal,
    kNoticeFoundThruster,
    kNoticeFoundLens,
    kNoticeSalvageHint,
    kNoticeNeedSeal,
    kNoticeNeedThruster,
    kNoticeNeedCutter,
    kNoticeRepairPump,
    kNoticeRepairLift,
};

// True while an unrecorded survey ring is inside the same slow-dwell window
// exploration::update uses to record it. That ring is the actionable
// interaction at its site, so the inactive service berth beside the Freight
// Yard ring must not post its repair clue over it.
inline bool pendingSurveyNear(const game::State &state) {
    for (uint8_t i = 0; i < world::surveyCount(); ++i) {
        world::Survey survey;
        if (!world::surveyInfo(i, survey) || (state.surveys & (static_cast<uint8_t>(1u) << survey.id)))
            continue;
        const int16_t dx = static_cast<int16_t>(state.x - survey.pixel_x);
        const int16_t dy = static_cast<int16_t>(state.y - survey.pixel_y);
        if (dx >= -18 && dx <= 18 && dy >= -12 && dy <= 12)
            return true;
    }
    return false;
}

// Pure notice selection shared by the device HUD and host tests. Discovery
// notices win over context. Survey dwell prompts are deliberately not selected;
// salvage and missing-equipment notices remain actionable.
inline uint8_t noticeFor(const game::State &state) {
    if (state.discoveryTicks) {
        if (state.lastDiscovery == exploration::kNone)
            return kNoticeSurveyRecorded;
        const uint8_t kind = exploration_data::pickup(state.lastDiscovery).kind;
        return kind == 1 ? kNoticeFoundCutter : kind == 2 ? kNoticeFoundSeal : kind == 3 ? kNoticeFoundThruster : kind == 6 ? kNoticeFoundLens : kNoticeNone;
    }
    if (state.contactId != exploration::kNone) {
        return state.contactId < exploration_data::PICKUP_COUNT ? kNoticeSalvageHint : kNoticeNone;
    }
    for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
        const exploration_data::Gate g = exploration_data::gate(i);
        if (!exploration::closed(state, i, g) || !exploration::nearGate(state, g, 24))
            continue;
        // A cutter-owned cutter gate opens automatically; it needs no prompt.
        // Any other owned tool also proves the cutter branch was already
        // completed, so a stale/debug no-clip state must not post NEED CUTTER.
        if (g.required == exploration_data::CUTTER && state.tools != 0)
            continue;
        return ((g.required & exploration_data::SEAL) && !(state.tools & exploration_data::SEAL))         ? kNoticeNeedSeal
               : ((g.required & exploration_data::THRUSTER) && !(state.tools & exploration_data::THRUSTER)) ? kNoticeNeedThruster
                                                                                                            : kNoticeNeedCutter;
    }
    // An inactive service mooring still reads as a clue, never as a dockable
    // station. It never blocks travel or hides a pickup.
    for (uint8_t i = 0; i < world::serviceBerthCount(); ++i) {
        world::Berth berth;
        if (!world::serviceBerthInfo(i, berth) || (state.campaign & berth.milestone) != 0)
            continue;
        const int16_t dx = static_cast<int16_t>(berth.pixel_x - state.x);
        const int16_t dy = static_cast<int16_t>(berth.pixel_y - state.y);
        if (dx > -40 && dx < 40 && dy > -32 && dy < 32) {
            if (pendingSurveyNear(state))
                return kNoticeNone;
            return berth.milestone == 2 ? kNoticeRepairPump : kNoticeRepairLift;
        }
    }
    return kNoticeNone;
}

#if defined(__AVR__)
// One byte selects the 128-byte row; no per-notice 24-bit addresses live in
// flash. Frames start after the two-byte sprite header, so notice 1 is frame 0.
inline void drawNotice(uint8_t notice) {
    cartService();
    const uint24_t address = hud_notices + 2 + static_cast<uint24_t>(notice - 1) * 128;
    FX::readDataBytes(address, Arduboy2Base::sBuffer + 7 * 128, 128);
}

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

    if (state.tools & exploration_data::CUTTER)
        exploration_view::glyph(Arduboy2Base::sBuffer, 57, 2, 1);
    if (state.tools & exploration_data::SEAL)
        exploration_view::glyph(Arduboy2Base::sBuffer, 64, 2, 2);
    if (state.tools & exploration_data::THRUSTER)
        exploration_view::glyph(Arduboy2Base::sBuffer, 71, 2, 3);
    // Ballast gauge: white fill grows from the bottom, the notch marks neutral
    // trim for the current cargo load.
    arduboy.drawRect(kGaugeX, kGaugeY, kGaugeW, kGaugeH, WHITE);
    const uint8_t gaugeInnerH = static_cast<uint8_t>(kGaugeH - 2);
    const uint8_t ballastFill = static_cast<uint8_t>(static_cast<uint16_t>(state.ballast) * gaugeInnerH / game::kBallastMax);
    if (ballastFill > 0) {
        const uint8_t fillY = static_cast<uint8_t>(kGaugeY + kGaugeH - 1 - ballastFill);
        cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(kGaugeX + 1), fillY, static_cast<uint8_t>(kGaugeW - 2), ballastFill, true);
    }
    const uint8_t notchY = neutralNotchY(state.cargo);
    cave::fillRect(Arduboy2Base::sBuffer, static_cast<int16_t>(kGaugeX + kGaugeW), notchY, 3, 1, true);

    const uint8_t notice = noticeFor(state);
    if (notice) {
        // The stamp replaces the whole 8-px text page; clear the one backing
        // row above it (y55) to preserve the old 9-px plate exactly.
        cave::fillRect(Arduboy2Base::sBuffer, kHintBackingX, kHintBackingY, kHintBackingW, 1, false);
        drawNotice(notice);
        return;
    }
    // No notice: leave the bottom row untouched so the map shows through.
}

#endif   // __AVR__

}   // namespace hud
