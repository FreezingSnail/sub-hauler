#pragma once

#include <stdint.h>



#include "campaign.hpp"
#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "generated/exploration.hpp"
#include "world.hpp"

#if defined(__AVR__)
#include "cart_service.hpp"
#include "common.hpp"
#include "generated/campaign_sprites.hpp"
#endif

// Campaign presentation: generated survey rings, service-berth moorings and
// small restoration overlays. Every overlay is derived from the saved
// milestone bits only; none of it affects collision, chart water or docking.
// The unified WORK board lives in work_view.hpp.
namespace campaign_view {

inline bool restored(const game::State &state, uint8_t milestone) {
    return milestone != 0 && (state.campaign & milestone) != 0;
}

inline void drawSurveyRings(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t ox = cave::pixelOriginX(camera);
    const int16_t oy = cave::pixelOriginY(camera);
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
        // An active objective blinks so it reads as a task, not scenery.
        if (campaign::hasMilestone(state, campaign::kMilestoneFirst) && !(state.surveys & (static_cast<uint8_t>(1u) << survey.id)) && (state.ticks & 0x10) != 0)
            continue;
        // Small ring with four approach brackets, distinct from a pickup.
#if defined(__AVR__)
        Blit::draw(static_cast<int16_t>(x - 7), static_cast<int16_t>(y - 6), SURVEY_RING_WIDTH, SURVEY_RING_HEIGHT, survey_ring + 2, 0, Blit::PLUSMASK);
#else
        cave::fillRect(buffer, static_cast<int16_t>(x - 4), static_cast<int16_t>(y - 2), 9, 5, true);
        cave::fillRect(buffer, static_cast<int16_t>(x - 3), static_cast<int16_t>(y - 1), 7, 3, false);
        cave::setPixel(buffer, x, y);
        cave::setPixel(buffer, static_cast<int16_t>(x - 7), y);
        cave::setPixel(buffer, static_cast<int16_t>(x + 7), y);
        cave::setPixel(buffer, x, static_cast<int16_t>(y - 6));
        cave::setPixel(buffer, x, static_cast<int16_t>(y + 6));
#endif
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
#if defined(__AVR__)
            Blit::draw(static_cast<int16_t>(x - 9), static_cast<int16_t>(y - 2), MOORING_WIDTH, MOORING_HEIGHT, mooring + 2, 0, Blit::PLUSMASK);
#else
            cave::fillRect(buffer, static_cast<int16_t>(x - 9), static_cast<int16_t>(y - 2), 3, 8, true);
            cave::fillRect(buffer, static_cast<int16_t>(x + 6), static_cast<int16_t>(y - 2), 3, 8, true);
            for (int16_t dx = -4; dx <= 4; dx += 4)
                cave::fillRect(buffer, static_cast<int16_t>(x + dx), static_cast<int16_t>(y + 2), 2, 2, true);
#endif
            continue;
        }
        cave::drawGantry(buffer, x, y);
        if (berth.milestone == campaign::kMilestonePump) {
#if defined(__AVR__)
            Blit::draw(static_cast<int16_t>(x - 18), static_cast<int16_t>(y + 5), PUMP_FLOW_WIDTH, PUMP_FLOW_HEIGHT, pump_flow + 2, 0, Blit::PLUSMASK);
#else
            drawFlow(buffer, x, static_cast<int16_t>(y + 5));
#endif
        } else if (berth.milestone == campaign::kMilestoneLift) {
            // Restored cable run: a short vertical hoist above the gantry.
#if defined(__AVR__)
            Blit::draw(static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 14), LIFT_CABLE_WIDTH, LIFT_CABLE_HEIGHT, lift_cable + 2, 0, Blit::PLUSMASK);
#else
            cave::fillRect(buffer, x, static_cast<int16_t>(y - 14), 1, 9, true);
            cave::fillRect(buffer, static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 14), 11, 1, true);
            cave::fillRect(buffer, static_cast<int16_t>(x - 3), static_cast<int16_t>(y - 11), 7, 1, true);
#endif
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



}   // namespace campaign_view

