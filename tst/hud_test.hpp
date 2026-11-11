#pragma once

#include "../src/hud.hpp"
#include "test.hpp"

// HUD notice selection is pure so prompt removal and equipment notices stay
// covered without a display.
inline void HudSuite(TestRunner &runner) {
    {
        Test test("survey dwell selects no HOLD STILL prompt");
        game::State state;
        game::resetWorld(state);
        state.campaign = campaign::kMilestonePump | campaign::kMilestoneLift;
        state.contactId = exploration::kSurveyBase;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone, "survey dwell is silent");
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone, "no backing notice is selected");

        bool cutterGate = false;
        for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
            const exploration_data::Gate g = exploration_data::gate(i);
            if (g.required != exploration_data::CUTTER)
                continue;
            state.tools = exploration_data::CUTTER;
            state.openGates = 0;
            state.contactId = exploration::kNone;
            state.x = static_cast<int16_t>(g.x + g.w / 2);
            state.y = static_cast<int16_t>(g.y + g.h / 2);
            test.assertEqual(hud::noticeFor(state), hud::kNoticeNone, "cutter-owned gate shows no cut prompt");
            cutterGate = true;
            break;
        }
        test.assertTrue(cutterGate, "generated world has a cutter gate");
        runner.add(test);
    }

    {
        Test test("a later tool suppresses a stale cutter-gate hint");
        game::State state;
        game::resetWorld(state);
        state.tools = exploration_data::THRUSTER;
        state.openGates = 0;
        state.contactId = exploration::kNone;
        const exploration_data::Gate gate = exploration_data::gate(exploration_data::GATE_N_BR);
        state.x = static_cast<int16_t>(gate.x + gate.w / 2);
        state.y = static_cast<int16_t>(gate.y + gate.h / 2);
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                         "owning a downstream tool hides the cutter hint");
        state.tools = 0;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNeedCutter,
                         "a fresh boat still sees the cutter hint");
        runner.add(test);
    }

    {
        Test test("useful discovery, salvage and equipment notices survive");
        game::State state;
        game::resetWorld(state);
        state.contactId = 0;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeSalvageHint, "pickup dwell keeps salvage hint");

        state.contactId = exploration::kNone;
        state.lastDiscovery = 0;
        state.discoveryTicks = 1;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeFoundCutter, "discovery notice wins");

        state.discoveryTicks = 0;
        bool sealGate = false;
        for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
            const exploration_data::Gate g = exploration_data::gate(i);
            if ((g.required & exploration_data::SEAL) == 0)
                continue;
            state.tools = 0;
            state.contactId = exploration::kNone;
            state.x = static_cast<int16_t>(g.x + g.w / 2);
            state.y = static_cast<int16_t>(g.y + g.h / 2);
            test.assertEqual(hud::noticeFor(state), hud::kNoticeNeedSeal, "missing tool keeps NEED notice");
            sealGate = true;
            break;
        }
        test.assertTrue(sealGate, "generated world has a seal gate");
        runner.add(test);
    }

    {
        Test test("a pending survey silences its whole inactive work site");
        game::State state;
        game::resetWorld(state);

        world::Berth liftBerth;
        test.assertTrue(world::berthInfo(world::BERTH_FREIGHT_YARD, liftBerth), "generated Freight Yard berth");
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestonePump;
        state.x = liftBerth.pixel_x;
        state.y = liftBerth.pixel_y;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                         "the unrecorded lift survey owns the Freight Yard site");
        state.x = static_cast<int16_t>(liftBerth.pixel_x - 39);
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                         "the lift repair line stays hidden across the berth approach");
        state.surveys = campaign::kSurveyLift;
        state.x = liftBerth.pixel_x;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeRepairLift,
                         "recording the survey restores the genuine lift repair line");

        world::Berth pumpBerth;
        test.assertTrue(world::berthInfo(world::BERTH_PUMP_REFUGE, pumpBerth), "generated Pump Refuge berth");
        game::resetWorld(state);
        state.campaign = campaign::kMilestoneFirst;
        state.x = pumpBerth.pixel_x;
        state.y = pumpBerth.pixel_y;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                         "the unrecorded pump survey owns the Pump Refuge site");
        state.surveys = campaign::kSurveyPump;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeRepairPump,
                         "recording the survey restores the genuine pump repair line");
        runner.add(test);
    }

    {
        Test test("the Deep Relay wreck posts no pump or lift repair clue");
        game::State state;
        game::resetWorld(state);
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift;
        world::Berth berth;
        test.assertTrue(world::berthInfo(world::BERTH_DEEP_RELAY, berth), "generated Deep Relay berth");
        state.x = berth.pixel_x;
        state.y = berth.pixel_y;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                         "an inactive Deep Relay is not a broken lift");
        runner.add(test);
    }
}
