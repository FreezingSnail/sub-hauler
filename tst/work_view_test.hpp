#pragma once

#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/work_view.hpp"
#include "../src/world.hpp"
#include "../src/generated/exploration.hpp"
#include "jobs_test.hpp"
#include "test.hpp"

// The single WORK board is a pure row model: story restoration only. These
// cases pin the order and the no-active-job gate without needing a display.
namespace work_view_test {

using jobs_test::dockAt;

inline uint8_t kindAt(const game::State &state, uint8_t index) {
    return work_view::rowAt(state, index).kind;
}

inline void firstComplete(game::State &state) {
    state.campaign = campaign::kMilestoneFirst;
    state.surveys = campaign::kSurveyMask;
}

}   // namespace work_view_test

inline void WorkViewSuite(TestRunner &runner) {
    using namespace work_view_test;

    {
        Test test("WORK has no rows when no story job is eligible");
        for (uint8_t berth = 0; berth < world::settlementCount(); ++berth) {
            game::State state;
            game::reset(state);
            test.assertTrue(dockAt(state, berth), "freight berth docks");
            if (berth == campaign::kBenthos || berth == campaign::kCormorant || berth == campaign::kDeepAnchor) {
                test.assertEqual(work_view::rowCount(state), 0, "no freight or story row before FIRST");
                test.assertEqual(kindAt(state, 0), work_view::kRowNone, "no row is exposed");
            }
        }
        runner.add(test);
    }

    {
        Test test("restoration rows are listed at every source berth");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "Tidewall docks");
        test.assertEqual(work_view::rowCount(state), 1, "FIRST only");
        test.assertEqual(kindAt(state, 0), work_view::kRowStory, "story first");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kFirst, "FIRST story");

        firstComplete(state);
        test.assertTrue(dockAt(state, campaign::kBenthos), "Benthos docks");
        test.assertEqual(work_view::rowCount(state), 1, "LIFT only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kLift, "LIFT is the Benthos story");

        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks");
        test.assertEqual(work_view::rowCount(state), 1, "PUMP only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kPump, "PUMP is the Cormorant story");

        state.campaign |= static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift);
        state.tools = exploration_data::SEAL;
        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks after both repairs");
        test.assertEqual(work_view::rowCount(state), 1, "BEACON only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kBeacon, "BEACON replaces the repairs");
        runner.add(test);
    }

    {
        Test test("every story beat has a WORK row and survey rows never load cargo");
        game::State state;
        game::reset(state);
        state.campaign = campaign::kMilestoneFirst;   // surveys still unrecorded
        test.assertTrue(dockAt(state, campaign::kBenthos), "Benthos docks");
        test.assertEqual(work_view::rowCount(state), 1, "pump survey only");
        const work_view::Row pumpSurvey = work_view::rowAt(state, 0);
        test.assertEqual(pumpSurvey.kind, work_view::kRowStory, "survey is a story row");
        test.assertEqual(pumpSurvey.mission, campaign::kPumpSurvey, "pump survey row");
        test.assertTrue(!campaign::isCargoMission(pumpSurvey.mission), "survey does not load cargo");
        test.assertTrue(!work_view::accept(state, pumpSurvey), "survey row does not accept a cargo job");
        test.assertEqual(state.activeJob.kind, game::kJobKindNone, "survey row leaves the hold free");

        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks");
        test.assertEqual(work_view::rowCount(state), 1, "lift survey only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kLiftSurvey, "lift survey row");

        test.assertTrue(campaign::recordSurvey(state, campaign::kPumpSurveyId), "pump survey recorded");
        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks after pump survey");
        test.assertEqual(work_view::rowCount(state), 2, "PUMP and LIFT_SURVEY");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kPump, "PUMP cargo comes first");
        test.assertEqual(work_view::rowAt(state, 1).mission, campaign::kLiftSurvey, "lift survey second");

        test.assertTrue(campaign::recordSurvey(state, campaign::kLiftSurveyId), "lift survey recorded");
        test.assertTrue(dockAt(state, campaign::kBenthos), "Benthos docks after lift survey");
        test.assertEqual(work_view::rowCount(state), 1, "LIFT only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kLift, "LIFT cargo replaces the survey lead");
        runner.add(test);
    }

    {
        Test test("completed branches leave the remaining story and VIEW ENDING");
        game::State state;
        game::reset(state);
        firstComplete(state);
        state.campaign |= static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
        state.tools = exploration_data::SEAL;
        test.assertTrue(dockAt(state, campaign::kDeepAnchor), "Deep Anchor docks");
        test.assertEqual(work_view::rowCount(state), 1, "HOME only");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kHome, "HOME is the story row");

        state.campaign |= campaign::kMilestoneHome;
        test.assertTrue(dockAt(state, campaign::kTidewall), "Tidewall docks after HOME");
        test.assertEqual(work_view::rowCount(state), 1, "VIEW ENDING only");
        test.assertEqual(work_view::rowAt(state, 0).kind, work_view::kRowEnding, "VIEW ENDING is a presentation row");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kWorkEnding, "ending id");
        runner.add(test);
    }

    {
        Test test("active work shows no selectable rows and accepts nothing twice");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "Tidewall docks");
        test.assertTrue(work_view::accept(state, work_view::rowAt(state, 0)), "FIRST accepted through WORK");
        test.assertEqual(work_view::rowCount(state), 0, "active work has no row list");
        test.assertEqual(kindAt(state, 0), work_view::kRowNone, "no row is exposed while carrying");
        test.assertTrue(!work_view::accept(state, work_view::rowAt(state, 0)), "second acceptance is refused");
        runner.add(test);
    }
}
