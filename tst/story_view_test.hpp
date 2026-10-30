#pragma once

#include "test.hpp"

#include "../src/story_view.hpp"

// Pure seven-beat deck selection and layout. The dock transition supplies the
// milestone mask before and after delivery; the in-world survey dwell supplies
// the survey mask. No story cursor is serialized.
inline void StoryViewSuite(TestRunner &runner) {
    {
        Test test("story decks map each new cargo milestone once");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneFirst), story_view::kCardFirst, "first delivery");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestonePump), story_view::kCardPump, "pump");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneLift), story_view::kCardLift, "lift");
        test.assertEqual(story_view::cardForNewMilestone(0, campaign::kMilestoneBeacon), story_view::kCardBeacon, "beacon");
        runner.add(test);
    }

    {
        Test test("story decks require a new bit and skip home/repeats");
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
        Test test("story deck order is stable for malformed multi-bit gains");
        const uint8_t many = campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon;
        test.assertEqual(story_view::cardForNewMilestone(0, many), story_view::kCardFirst, "first has priority");
        test.assertEqual(story_view::cardForNewMilestone(campaign::kMilestoneFirst,
                                                         campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift),
                         story_view::kCardPump, "pump before lift");
        runner.add(test);
    }

    {
        Test test("survey dwell maps only newly recorded facts");
        test.assertEqual(story_view::cardForNewSurvey(0, campaign::kSurveyPump), story_view::kCardPumpSurvey, "pump survey");
        test.assertEqual(story_view::cardForNewSurvey(0, campaign::kSurveyLift), story_view::kCardLiftSurvey, "lift survey");
        test.assertEqual(story_view::cardForNewSurvey(campaign::kSurveyPump, campaign::kSurveyPump), story_view::kCardNone, "repeat survey");
        test.assertEqual(story_view::cardForNewSurvey(campaign::kSurveyPump, campaign::kSurveyMask), story_view::kCardLiftSurvey, "only the new bit counts");
        test.assertEqual(story_view::cardForNewSurvey(0, campaign::kSurveyMask), story_view::kCardPumpSurvey, "pump wins malformed dual gain");
        runner.add(test);
    }

    {
        Test test("seven alternating decks total thirty cards and seventy-five frames");
        test.assertEqual(story_view::cardCount(story_view::kCardFirst), 6, "FIRST has three image/text pairs");
        test.assertEqual(story_view::cardCount(story_view::kCardPumpSurvey), 4, "survey and delivery beats have two pairs");
        test.assertEqual(story_view::cardCount(story_view::kCardHome), 4, "HOME has two pairs");
        test.assertEqual(story_view::cardCount(story_view::kCardNone), 0, "no deck");
        test.assertEqual(story_view::cardCount(story_view::kCardCount + 1), 0, "unknown deck");

        uint8_t cards = 0;
        uint8_t images = 0;
        uint8_t frames = 0;
        for (uint8_t beat = story_view::kCardFirst; beat <= story_view::kCardCount; ++beat) {
            const uint8_t count = story_view::cardCount(beat);
            cards = static_cast<uint8_t>(cards + count);
            test.assertEqual(story_view::cardStart(beat), static_cast<uint8_t>(cards - count), "deck starts are contiguous");
            for (uint8_t card = 0; card < count; ++card) {
                const uint8_t global = story_view::globalCard(beat, card);
                const bool image = story_view::isImageCard(global);
                test.assertEqual(image, (card & 1u) == 0, "every deck alternates image/text");
                test.assertEqual(story_view::firstFrameFor(beat, card), frames, "frame layout is contiguous");
                images = static_cast<uint8_t>(images + (image ? 1 : 0));
                frames = static_cast<uint8_t>(frames + (image ? 4 : 1));
            }
        }
        test.assertEqual(cards, STORY_BEAT_CARDS, "generated card count");
        test.assertEqual(images, STORY_BEAT_IMAGES, "generated image count");
        test.assertEqual(frames, STORY_BEAT_FRAMES, "generated frame count");
        test.assertEqual(story_view::firstFrame(STORY_BEAT_CARDS - 1) + 1, STORY_BEAT_FRAMES, "last card owns the last frame");
        runner.add(test);
    }
}
