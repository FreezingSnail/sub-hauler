#pragma once



#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/work_view.hpp"
#include "../src/world.hpp"
#include "../src/generated/exploration.hpp"
#include "jobs_test.hpp"
#include "test.hpp"

// The single WORK board is a pure row model: story restoration first, then the
// one repeatable freight row. These cases pin the order and the no-active-job
// gate without needing a display.
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
        Test test("WORK carries exactly one freight row when no story job is eligible");
        for (uint8_t berth = 0; berth < world::settlementCount(); ++berth) {
            game::State state;
            game::reset(state);
            test.assertTrue(dockAt(state, berth), "freight berth docks");
            // Benthos has no cargo source before FIRST; the other berths also
            // have no eligible restoration job at voyage start except Tidewall.
            if (berth == campaign::kBenthos) {
                test.assertEqual(work_view::rowCount(state), 1, "only the freight row exists before FIRST");
                test.assertEqual(kindAt(state, 0), work_view::kRowFreight, "the freight row is first");
                test.assertEqual(work_view::rowAt(state, 0).mission, work_view::kNoMission, "freight has no mission id");
            }
        }
        runner.add(test);
    }

    {
        Test test("restoration rows are listed before the freight row at every source berth");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "Tidewall docks");
        test.assertEqual(work_view::rowCount(state), 2, "FIRST and freight");
        test.assertEqual(kindAt(state, 0), work_view::kRowStory, "story first");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kFirst, "FIRST story");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "freight beneath");

        firstComplete(state);
        test.assertTrue(dockAt(state, campaign::kBenthos), "Benthos docks");
        test.assertEqual(work_view::rowCount(state), 2, "LIFT plus freight");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kLift, "LIFT is the Benthos story");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "Benthos freight beneath");

        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks");
        test.assertEqual(work_view::rowCount(state), 2, "PUMP and freight");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kPump, "PUMP is the Cormorant story");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "freight beneath");

        state.campaign |= static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift);
        state.tools = exploration_data::SEAL;
        test.assertTrue(dockAt(state, campaign::kCormorant), "Cormorant docks after both repairs");
        test.assertEqual(work_view::rowCount(state), 2, "BEACON and freight");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kBeacon, "BEACON replaces the repairs");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "freight beneath BEACON");
        runner.add(test);
    }

    {
        Test test("completed branches leave the remaining story and VIEW ENDING above freight");
        game::State state;
        game::reset(state);
        firstComplete(state);
        state.campaign |= static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
        state.tools = exploration_data::SEAL;
        test.assertTrue(dockAt(state, campaign::kDeepAnchor), "Deep Anchor docks");
        test.assertEqual(work_view::rowCount(state), 2, "HOME plus freight");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kHome, "HOME is the story row");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "freight beneath HOME");

        state.campaign |= campaign::kMilestoneHome;
        test.assertTrue(dockAt(state, campaign::kTidewall), "Tidewall docks after HOME");
        test.assertEqual(work_view::rowCount(state), 2, "VIEW ENDING plus freight");
        test.assertEqual(work_view::rowAt(state, 0).kind, work_view::kRowEnding, "VIEW ENDING is a presentation row");
        test.assertEqual(work_view::rowAt(state, 0).mission, campaign::kWorkEnding, "ending id");
        test.assertEqual(kindAt(state, 1), work_view::kRowFreight, "freight still beneath the ending");
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

