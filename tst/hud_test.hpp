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
        Test test("a pending lift survey outranks the inactive-berth repair notice");
        game::State state;
        game::resetWorld(state);
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestonePump;
        world::Survey survey;
        bool liftSurvey = false;
        for (uint8_t i = 0; i < world::surveyCount(); ++i) {
            if (!world::surveyInfo(i, survey) || survey.id != world::SURVEY_LIFT)
                continue;
            liftSurvey = true;
            break;
        }
        test.assertTrue(liftSurvey, "generated world has the Freight Yard lift survey");
        if (liftSurvey) {
            state.x = survey.pixel_x;
            state.y = survey.pixel_y;
            test.assertEqual(hud::noticeFor(state), hud::kNoticeNone,
                             "an unrecorded nearby survey suppresses the lift repair line");
        }
        runner.add(test);
    }

    {
        Test test("the lift repair notice survives once its survey is recorded");
        game::State state;
        game::resetWorld(state);
        state.campaign = campaign::kMilestoneFirst | campaign::kMilestonePump;
        state.surveys = campaign::kSurveyLift;
        world::Berth berth;
        test.assertTrue(world::berthInfo(world::BERTH_FREIGHT_YARD, berth), "generated Freight Yard berth");
        state.x = berth.pixel_x;
        state.y = berth.pixel_y;
        test.assertEqual(hud::noticeFor(state), hud::kNoticeRepairLift,
                         "no pending survey leaves the genuine lift repair line");
        runner.add(test);
    }
}
