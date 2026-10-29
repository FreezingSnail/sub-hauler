#pragma once

#include "test.hpp"

#include "../src/story_view.hpp"

// Pure one-shot restoration card selection. The dock transition supplies the
// milestone mask before and after delivery; no story cursor is serialized.
inline void StoryViewSuite(TestRunner &runner) {
    {
        Test test("story cards map each new cargo milestone once");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneFirst), story_view::kCardFirst, "first delivery");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestonePump), story_view::kCardPump, "pump");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneLift), story_view::kCardLift, "lift");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneBeacon), story_view::kCardBeacon, "beacon");
        runner.add(test);
    }

    {
        Test test("story cards require a new bit and skip home/repeats");
        const uint8_t allButFirst = campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon;
        test.assertEqual(story_view::cardForNewMilestone(0, 0), story_view::kCardNone, "no change");
        test.assertEqual(story_view::cardForNewMilestone(allButFirst, allButFirst), story_view::kCardNone, "repeat dock");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneHome), story_view::kCardNone, "home uses the ending");
        test.assertEqual(story_view::cardForNewMilestone(campaign::kMilestoneFirst, campaign::kMilestoneFirst | campaign::kMilestonePump),
                         story_view::kCardPump, "only the new bit counts");
        test.assertEqual(story_view::kCardNone, 0, "bss default is no card");
        runner.add(test);
    }

    {
        Test test("story card order is stable for malformed multi-bit gains");
        const uint8_t many = campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon;
        test.assertEqual(story_view::cardForNewMilestone(0, many), story_view::kCardFirst, "first has priority");
        test.assertEqual(story_view::cardForNewMilestone(campaign::kMilestoneFirst,
                                                         campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift),
                         story_view::kCardPump, "pump before lift");
        runner.add(test);
    }
}
