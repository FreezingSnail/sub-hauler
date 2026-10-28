#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_CAMPAIGN

#include "campaign.hpp"
#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#if SH_FEATURE_EXPLORATION
#include "generated/exploration.hpp"
#endif
#if SH_FEATURE_STATIONS
#include "station_view.hpp"
#endif
#include "world.hpp"

// Campaign presentation: generated survey rings, service-berth moorings and
// small restoration overlays. Every overlay is derived from the saved
// milestone bits only; none of it affects collision, chart water or docking.
// The WORK page is device-only because it uses the external FX font.
namespace campaign_view {

constexpr uint8_t kWorkRows = 2;
constexpr uint8_t kRowY0 = 20;
constexpr uint8_t kRowStep = 9;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kClueY = 39;
constexpr uint8_t kClue2Y = 47;
constexpr uint8_t kHintY = 56;

static_assert(kRowY0 + kRowStep + kRowInvertH <= kClueY, "work rows clear the clue");
static_assert(kClue2Y + 7 <= kHintY, "second clue clears the hint");
static_assert(kHintY + 7 <= 64, "work hint stays on screen");

inline bool restored(const game::State &state, uint8_t milestone) {
    return milestone != 0 && (state.campaign & milestone) != 0;
}

inline void drawSurveyRings(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t ox = camera.tileX * world::TILE_PIXELS;
    const int16_t oy = camera.tileY * world::TILE_PIXELS;
    for (uint8_t i = 0; i < world::surveyCount(); ++i) {
        world::Survey survey;
        if (!world::surveyInfo(i, survey))
            continue;
        const int16_t x = static_cast<int16_t>(survey.pixel_x - ox);
        const int16_t y = static_cast<int16_t>(survey.pixel_y - oy);
        if (x < -8 || y < -8 || x > cave::VIEW_WIDTH + 7 || y > cave::VIEW_HEIGHT + 7)
            continue;
        if (state.surveys & (static_cast<uint8_t>(1u) << survey.id)) {
            // Recorded fact: a single quiet marker, never a repeat prompt.
            cave::setPixel(buffer, x, y);
            continue;
        }
        // Small ring with four approach brackets, distinct from a pickup.
        cave::fillRect(buffer, static_cast<int16_t>(x - 4), static_cast<int16_t>(y - 2), 9, 5, true);
        cave::fillRect(buffer, static_cast<int16_t>(x - 3), static_cast<int16_t>(y - 1), 7, 3, false);
        cave::setPixel(buffer, x, y);
        cave::setPixel(buffer, static_cast<int16_t>(x - 7), y);
        cave::setPixel(buffer, static_cast<int16_t>(x + 7), y);
        cave::setPixel(buffer, x, static_cast<int16_t>(y - 6));
        cave::setPixel(buffer, x, static_cast<int16_t>(y + 6));
    }
}

inline void drawFlow(uint8_t *buffer, int16_t x, int16_t y) {
    // Three short pressure ripples: a static cue, not an animation.
    for (uint8_t i = 0; i < 3; ++i) {
        const int16_t yy = static_cast<int16_t>(y + i * 4);
        cave::fillRect(buffer, static_cast<int16_t>(x + 12 + i), yy, 4, 1, true);
        cave::fillRect(buffer, static_cast<int16_t>(x - 16 - i), yy, 4, 1, true);
    }
}

inline void drawServiceBerths(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    for (uint8_t i = 0; i < world::serviceBerthCount(); ++i) {
        world::Berth berth;
        if (!world::serviceBerthInfo(i, berth))
            continue;
        const int16_t x = cave::dockScreenX(camera, berth.pixel_x);
        const int16_t y = cave::dockScreenY(camera, berth.pixel_y);
        if (!cave::gantryVisible(x, y))
            continue;
        if (!restored(state, berth.milestone)) {
            // Inactive mooring: two posts and a dashed crossbar. Readable as a
            // place that will matter later, never as a working berth.
            cave::fillRect(buffer, static_cast<int16_t>(x - 9), static_cast<int16_t>(y - 2), 3, 8, true);
            cave::fillRect(buffer, static_cast<int16_t>(x + 6), static_cast<int16_t>(y - 2), 3, 8, true);
            for (int16_t dx = -4; dx <= 4; dx += 4)
                cave::fillRect(buffer, static_cast<int16_t>(x + dx), static_cast<int16_t>(y + 2), 2, 2, true);
            continue;
        }
        cave::drawGantry(buffer, x, y);
        if (berth.milestone == campaign::kMilestonePump)
            drawFlow(buffer, x, static_cast<int16_t>(y + 5));
        else if (berth.milestone == campaign::kMilestoneLift) {
            // Restored cable run: a short vertical hoist above the gantry.
            cave::fillRect(buffer, x, static_cast<int16_t>(y - 14), 1, 9, true);
            cave::fillRect(buffer, static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 14), 11, 1, true);
            cave::fillRect(buffer, static_cast<int16_t>(x - 3), static_cast<int16_t>(y - 11), 7, 1, true);
        }
    }
}

// One table drives the three settlement overlays. Keeping the settlement id,
// its opening milestone and the shared screen transform in one loop avoids
// three copies of the same visibility test.
struct RestorationSite {
    uint8_t berth;
    uint8_t milestone;
};

#if defined(__AVR__)
const RestorationSite kRestorationSites[3] PROGMEM = {
    {campaign::kBenthos, campaign::kMilestoneFirst},
    {campaign::kCormorant, campaign::kMilestoneLift},
    {campaign::kDeepAnchor, campaign::kMilestoneBeacon},
};
#else
constexpr RestorationSite kRestorationSites[3] = {
    {campaign::kBenthos, campaign::kMilestoneFirst},
    {campaign::kCormorant, campaign::kMilestoneLift},
    {campaign::kDeepAnchor, campaign::kMilestoneBeacon},
};
#endif

inline void drawLamp(uint8_t *buffer, int16_t x, int16_t y) {
    cave::fillRect(buffer, x, y, 2, 2, true);
}

inline void drawRestoration(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    drawSurveyRings(buffer, state, camera);
    drawServiceBerths(buffer, state, camera);

    for (uint8_t i = 0; i < 3; ++i) {
#if defined(__AVR__)
        const uint8_t berth = pgm_read_byte(&kRestorationSites[i].berth);
        const uint8_t milestone = pgm_read_byte(&kRestorationSites[i].milestone);
#else
        const uint8_t berth = kRestorationSites[i].berth;
        const uint8_t milestone = kRestorationSites[i].milestone;
#endif
        if (!restored(state, milestone))
            continue;
        world::Settlement site;
        if (!world::settlementInfo(berth, site))
            continue;
        const int16_t x = cave::dockScreenX(camera, site.pixel_x);
        const int16_t y = cave::dockScreenY(camera, site.pixel_y);
        if (!cave::gantryVisible(x, y))
            continue;

        if (berth == campaign::kBenthos) {
            // FIRST lights one bank; PUMP completes the row.
            drawLamp(buffer, static_cast<int16_t>(x - 8), static_cast<int16_t>(y - 17));
            drawLamp(buffer, x, static_cast<int16_t>(y - 17));
            if (restored(state, campaign::kMilestonePump))
                drawLamp(buffer, static_cast<int16_t>(x + 8), static_cast<int16_t>(y - 17));
        } else if (berth == campaign::kCormorant) {
            // LIFT lights the workshop hoist.
            drawLamp(buffer, static_cast<int16_t>(x - 8), static_cast<int16_t>(y - 17));
            drawLamp(buffer, static_cast<int16_t>(x + 8), static_cast<int16_t>(y - 17));
        } else {
            // BEACON lights the mast; HOME completes the signal arm.
            cave::fillRect(buffer, static_cast<int16_t>(x - 1), static_cast<int16_t>(y - 22), 3, 6, true);
            cave::fillRect(buffer, x, static_cast<int16_t>(y - 21), 1, 4, false);
            if (restored(state, campaign::kMilestoneHome))
                cave::fillRect(buffer, static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 20), 3, 1, true);
        }
    }
}


#if defined(__AVR__)

#include "common.hpp"

inline uint24_t missionLabel(uint8_t mission) {
    switch (mission) {
    case campaign::kFirst:
        return txt_first;
    case campaign::kPump:
        return txt_pump;
    case campaign::kLift:
        return txt_lift;
    case campaign::kBeacon:
        return txt_beacon;
    case campaign::kHome:
        return txt_home;
    case campaign::kWorkEnding:
        return txt_view_ending;
    default:
        return 0;
    }
}

inline uint24_t missionPurpose(uint8_t mission) {
    switch (mission) {
    case campaign::kFirst:
        return txt_first_purpose;
    case campaign::kPump:
        return txt_pump_purpose;
    case campaign::kLift:
        return txt_lift_purpose;
    case campaign::kBeacon:
        return txt_beacon_purpose;
    case campaign::kHome:
        return txt_home_purpose;
    default:
        return 0;
    }
}

inline uint24_t clueFor(const game::State &state, uint8_t berth) {
    switch (berth) {
    case campaign::kTidewall:
        if (campaign::missionCompleted(state, campaign::kHome))
            return txt_network_on;
        return restored(state, campaign::kMilestoneFirst) ? txt_benthos_lamps_on : txt_first_clue;
    case campaign::kBenthos:
        if (!restored(state, campaign::kMilestoneFirst))
            return txt_no_work;
        if (!campaign::surveyRecorded(state, campaign::kPumpSurveyId))
            return txt_pump_survey_clue;
        if (!restored(state, campaign::kMilestonePump))
            return txt_pump_source_clue;
        if (!campaign::surveyRecorded(state, campaign::kLiftSurveyId))
            return txt_lift_clue;
        if (!restored(state, campaign::kMilestoneLift))
            return txt_lift_source_clue;
        return txt_lift_restored;
    case campaign::kCormorant:
        if (!restored(state, campaign::kMilestoneFirst))
            return txt_no_work;
        // Cormorant carries the lift survey clue; the pump survey clue lives
        // at Benthos. Both may be found before their repair is requested.
        if (!campaign::surveyRecorded(state, campaign::kLiftSurveyId))
            return txt_survey_lift_clue;
        if (!campaign::surveyRecorded(state, campaign::kPumpSurveyId))
            return txt_survey_pump_clue;
        if (!restored(state, campaign::kMilestonePump))
            return txt_pump_part_ready;
        if (!restored(state, campaign::kMilestoneLift))
            return txt_lift_source_clue;
        if (!campaign::sealOwned(state))
            return txt_beacon_needs_seal;
        return txt_beacon_ready;
    case campaign::kDeepAnchor:
        if (!restored(state, campaign::kMilestoneBeacon))
            return txt_beacon_clue;
        if (!campaign::missionCompleted(state, campaign::kHome))
            return txt_beacon_lit;
        return txt_network_on;
    default:
        return 0;
    }
}

inline uint24_t equipmentClue(const game::State &state) {
    if (!(state.tools & exploration_data::CUTTER))
        return txt_cutter_rumor;
    if (!campaign::sealOwned(state))
        return txt_seal_rumor;
    return 0;
}

inline void printRow(uint8_t y, uint8_t mission, bool selected) {
    if (selected) {
        arduboy.fillRect(0, static_cast<int16_t>(y - 1), 128, kRowInvertH, WHITE);
        text::inverted();
    } else {
        text::normal();
    }
    text::at(4, y);
    text::print(missionLabel(mission));
    if (mission != campaign::kWorkEnding) {
        text::character(' ');
        text::number(campaign::missionReward(mission));
    }
}

inline void drawWorkList(const game::State &state, uint8_t selected) {
    text::normal();
    text::at(2, 2);
    text::print(station_view::nameFor(static_cast<uint8_t>(state.docked)));
    text::character(' ');
    text::print(txt_work);
    arduboy.drawFastHLine(0, 11, 128, WHITE);

    if (game::hasActiveJob(state) && state.activeJob.kind == game::kJobKindCampaign) {
        text::at(4, kRowY0);
        text::print(txt_carrying);
        text::character(' ');
        text::print(missionLabel(state.activeJob.mission));
        text::at(4, static_cast<uint8_t>(kRowY0 + kRowStep));
        text::print(txt_deliver_to);
        text::character(' ');
        uint8_t destination = state.activeJob.to;
        text::print(station_view::nameFor(destination));
        text::at(3, kHintY);
        text::print(txt_back_hint);
        return;
    }

    uint8_t visible = 0;
    for (uint8_t row = 0; row < kWorkRows; ++row) {
        const uint8_t mission = campaign::workMissionAt(state, static_cast<uint8_t>(state.docked), row);
        if (mission == campaign::kMissionNone)
            continue;
        printRow(static_cast<uint8_t>(kRowY0 + row * kRowStep), mission, row == selected);
        ++visible;
    }
    text::normal();

    const uint24_t clue = clueFor(state, static_cast<uint8_t>(state.docked));
    const uint24_t equipment = equipmentClue(state);
    if (visible == 0) {
        text::at(4, kClueY);
        text::print(clue ? clue : txt_no_work);
    } else if (clue) {
        text::at(4, kClueY);
        text::print(clue);
    }
    if (equipment && visible <= 1) {
        text::at(4, kClue2Y);
        text::print(equipment);
    }
    text::at(3, kHintY);
    text::print(visible ? txt_work_hint : txt_back_hint);
}

inline void drawWorkDetail(const game::State &state, uint8_t mission) {
    (void)state;
    text::normal();
    text::at(4, 4);
    text::print(missionLabel(mission));
    text::at(4, 18);
    text::print(missionPurpose(mission));
    text::at(4, 30);
    text::print(txt_pays);
    text::character(' ');
    text::number(campaign::missionReward(mission));
    text::at(3, kHintY);
    text::print(txt_load_hint);
}

#endif   // __AVR__

}   // namespace campaign_view

#endif   // SH_FEATURE_CAMPAIGN
