#pragma once



#include "../src/campaign_view.hpp"
#include "../src/game.hpp"
#include "../src/world.hpp"
#include "test.hpp"

// Native campaign presentation tests: generated survey rings, service-berth
// moorings and restoration lamps must appear only from saved milestone bits.
namespace campaign_view_test {

constexpr uint16_t kBufferBytes = 128 * 64 / 8;

inline void clear(uint8_t *buffer) {
    for (uint16_t i = 0; i < kBufferBytes; ++i)
        buffer[i] = 0;
}

inline uint16_t lit(const uint8_t *buffer) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < kBufferBytes; ++i) {
        uint8_t value = buffer[i];
        while (value != 0) {
            count = static_cast<uint16_t>(count + (value & 1u));
            value >>= 1;
        }
    }
    return count;
}

inline bool same(const uint8_t *a, const uint8_t *b) {
    for (uint16_t i = 0; i < kBufferBytes; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

}   // namespace campaign_view_test

inline void CampaignViewSuite(TestRunner &runner) {
    using namespace campaign_view_test;
    static uint8_t a[kBufferBytes];
    static uint8_t b[kBufferBytes];

    {
        Test test("survey ring appears from generated metadata and quiets once recorded");
        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "generated pump survey");
        game::State state;
        game::reset(state);
        const cave::Camera camera = cave::cameraCenteredAtPixel(survey.pixel_x, survey.pixel_y);
        clear(a);
        campaign_view::drawRestoration(a, state, camera);
        clear(b);
        state.surveys = campaign::kSurveyPump;
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(lit(a) > 0, "unrecorded survey ring is visible");
        test.assertTrue(!same(a, b), "recorded survey marker changes shape");
        test.assertTrue(lit(a) >= lit(b), "recorded marker is the quieter form");
        runner.add(test);
    }

    {
        Test test("active survey ring blinks after FIRST and stays steady before it");
        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "generated pump survey");
        game::State state;
        game::reset(state);
        const cave::Camera camera = cave::cameraCenteredAtPixel(survey.pixel_x, survey.pixel_y);

        state.ticks = 0;
        clear(a);
        campaign_view::drawRestoration(a, state, camera);
        state.ticks = 0x10;
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(same(a, b), "early survey ring does not blink");

        state.campaign = campaign::kMilestoneFirst;
        state.ticks = 0;
        clear(a);
        campaign_view::drawRestoration(a, state, camera);
        state.ticks = 0x10;
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(lit(a) > 0, "unrecorded survey ring is visible on the on phase");
        test.assertTrue(lit(b) < lit(a), "survey ring disappears on the off phase");
        runner.add(test);
    }

    {
        Test test("pump restoration activates the Pump Refuge mooring and flow cue");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(0, berth), "generated Pump Refuge");
        game::State state;
        game::reset(state);
        const cave::Camera camera = cave::cameraCenteredAtPixel(berth.pixel_x, berth.pixel_y);
        clear(a);
        campaign_view::drawRestoration(a, state, camera);
        state.campaign = campaign::kMilestoneFirst;
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(same(a, b), "inactive mooring ignores unrelated milestones");
        state.campaign = campaign::kMilestonePump;
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(!same(a, b), "PUMP activates the mooring");
        test.assertTrue(lit(b) > lit(a), "active gantry and flow draw more than the inactive posts");
        runner.add(test);
    }

    {
        Test test("LIFT and BEACON light their own overlays and no unrelated ones");
        world::Berth liftBerth;
        test.assertTrue(world::serviceBerthInfo(1, liftBerth), "generated Lower Gantry");
        game::State state;
        game::reset(state);
        const cave::Camera liftCamera = cave::cameraCenteredAtPixel(liftBerth.pixel_x, liftBerth.pixel_y);
        clear(a);
        campaign_view::drawRestoration(a, state, liftCamera);
        state.campaign = campaign::kMilestonePump;
        clear(b);
        campaign_view::drawRestoration(b, state, liftCamera);
        test.assertTrue(same(a, b), "PUMP does not open the Lower Gantry gantry");
        state.campaign = campaign::kMilestoneLift;
        clear(b);
        campaign_view::drawRestoration(b, state, liftCamera);
        test.assertTrue(!same(a, b), "LIFT opens the Lower Gantry gantry");
        test.assertTrue(lit(b) > lit(a), "lift gantry and cable draw more than the inactive posts");

        world::Settlement anchor;
        test.assertTrue(world::settlementInfo(campaign::kDeepAnchor, anchor), "Deep Anchor settlement");
        const cave::Camera anchorCamera = cave::cameraCenteredAtPixel(anchor.pixel_x, anchor.pixel_y);
        game::reset(state);
        clear(a);
        campaign_view::drawRestoration(a, state, anchorCamera);
        state.campaign = static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift);
        clear(b);
        campaign_view::drawRestoration(b, state, anchorCamera);
        test.assertTrue(same(a, b), "repairs alone leave the beacon dark");
        state.campaign |= campaign::kMilestoneBeacon;
        clear(b);
        campaign_view::drawRestoration(b, state, anchorCamera);
        test.assertTrue(!same(a, b), "BEACON lights the anchor mast");
        runner.add(test);
    }

    {
        Test test("Benthos lamps follow FIRST and PUMP flags only");
        world::Settlement benthos;
        test.assertTrue(world::settlementInfo(campaign::kBenthos, benthos), "Benthos settlement");
        game::State state;
        game::reset(state);
        const cave::Camera camera = cave::cameraCenteredAtPixel(benthos.pixel_x, benthos.pixel_y);
        clear(a);
        campaign_view::drawRestoration(a, state, camera);
        test.assertEqual(lit(a), 0, "unrestored Benthos adds no lamp pixels");
        state.campaign = campaign::kMilestoneFirst;
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        const uint16_t firstLit = lit(b);
        test.assertTrue(firstLit > 0, "FIRST lights the first lamp bank");
        state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump);
        clear(b);
        campaign_view::drawRestoration(b, state, camera);
        test.assertTrue(lit(b) > firstLit, "PUMP completes the lamp row");
        runner.add(test);
    }
}

