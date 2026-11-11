#pragma once

#include "test.hpp"

#include "../src/story_view.hpp"

// Pure story scene selection and layout. The dock transition supplies the
// milestone mask before and after delivery; the in-world survey dwell supplies
// the survey mask. Card kinds and text pages come from generated source
// metadata, so no story cursor is serialized.
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
        Test test("story scenes use generated card kinds and a frozen frame map");
        test.assertEqual(story_view::kCardCount, 7, "seven beat decks are frozen");
        test.assertEqual(story_view::kHandoffCount, 8, "six cargo departures plus two joint variants");
        test.assertEqual(story_view::kSceneExpedition, 15, "existing relay result keeps its scene id");
        test.assertEqual(story_view::kSceneExpeditionDeparture, 16, "relay briefing appends after all existing scenes");
        test.assertEqual(story_view::kSceneOpening, 17, "opening deck appends last");
        test.assertEqual(story_view::kSceneMax, 17, "opening deck is the newest appended scene");
        test.assertEqual(story_view::cardCount(story_view::kSceneOpening), 2, "opening is one image and one instruction");
        test.assertTrue(story_view::isImageCard(story_view::kSceneOpening, 0), "opening starts with arrival art");
        test.assertTrue(!story_view::isImageCard(story_view::kSceneOpening, 1), "opening ends on the Benthos instruction");
        test.assertEqual(story_view::cardCount(story_view::kSceneFirst), 6, "FIRST keeps three image/text pairs");
        test.assertEqual(story_view::cardCount(story_view::kSceneNone), 0, "no scene");
        test.assertEqual(story_view::cardCount(story_view::kSceneMax + 1), 0, "unknown scene");

        uint8_t cards = 0;
        uint8_t imageCards = 0;
        uint8_t textCards = 0;
        for (uint8_t scene = story_view::kSceneFirst; scene <= story_view::kSceneMax; ++scene) {
            const uint8_t count = story_view::cardCount(scene);
            cards = static_cast<uint8_t>(cards + count);
            test.assertTrue(count > 0, "every scene has cards");
            test.assertTrue(story_view::isImageCard(scene, 0), "every scene starts with an illustration");
            uint8_t text = 0;
            for (uint8_t card = 0; card < count; ++card) {
                const uint8_t kind = story_view::cardKind(scene, card);
                test.assertTrue(kind == STORY_KIND_IMAGE || kind == STORY_KIND_TEXT, "card kind is image or text");
                if (kind == STORY_KIND_IMAGE)
                    ++imageCards;
                else
                    ++textCards;
            }
            for (uint8_t card = 1; card < count; ++card) {
                if (!story_view::isImageCard(scene, card))
                    ++text;
            }
            test.assertTrue(text > 0, "every scene has prose");
        }
        test.assertEqual(cards, STORY_CARD_COUNT, "generated card count");
        test.assertEqual(imageCards, STORY_IMAGE_CARD_COUNT, "generated image-card count");
        test.assertEqual(textCards, STORY_TEXT_CARD_COUNT, "generated text-card count");

        // Consecutive text pages are legal source output, not a malformed deck.
        bool consecutiveText = false;
        for (uint8_t scene = story_view::kSceneFirst; scene <= story_view::kSceneMax; ++scene) {
            const uint8_t count = story_view::cardCount(scene);
            for (uint8_t card = 1; card + 1 < count; ++card) {
                if (!story_view::isImageCard(scene, card) && !story_view::isImageCard(scene, card + 1))
                    consecutiveText = true;
            }
        }
        test.assertTrue(consecutiveText, "pagination emits consecutive text pages where needed");

        bool seen[STORY_FRAME_COUNT] = {};
        for (uint8_t scene = story_view::kSceneFirst; scene <= story_view::kSceneMax; ++scene) {
            for (uint8_t card = 0; card < story_view::cardCount(scene); ++card) {
                const bool image = story_view::isImageCard(scene, card);
                const uint8_t first = story_view::firstFrameFor(scene, card);
                for (uint8_t phase = 0; phase < (image ? 4 : 1); ++phase) {
                    const uint8_t frame = static_cast<uint8_t>(first + phase);
                    test.assertTrue(frame < STORY_FRAME_COUNT, "card frame is inside the blob");
                    // Reused loading/restoration art intentionally shares poses.
                    if (!image)
                        test.assertTrue(!seen[frame], "each text frame has one owning card");
                    seen[frame] = true;
                }
            }
        }
        bool allFrames = true;
        for (uint8_t frame = 0; frame < STORY_FRAME_COUNT; ++frame)
            allFrames = allFrames && seen[frame];
        test.assertTrue(allFrames, "all generated frames are addressed");
        test.assertEqual(STORY_BEAT_IMAGES, STORY_IMAGE_COUNT, "compatibility image alias");
        test.assertTrue(STORY_BEAT_MAX_CARDS >= 6, "scene card budget is generated");
        runner.add(test);
    }

    {
        Test test("cargo acceptance selects the six departure handoffs");
        test.assertEqual(story_view::handoffForMission(campaign::kFirst), story_view::kHandoffFirst, "FIRST departure");
        test.assertEqual(story_view::handoffForMission(campaign::kPump), story_view::kHandoffPump, "PUMP pickup");
        test.assertEqual(story_view::handoffForMission(campaign::kLift), story_view::kHandoffLift, "LIFT pickup");
        test.assertEqual(story_view::handoffForMission(campaign::kBeacon), story_view::kHandoffBeacon, "BEACON departure");
        test.assertEqual(story_view::handoffForMission(campaign::kHome), story_view::kHandoffHome, "HOME departure");
        test.assertEqual(story_view::handoffForMission(campaign::kExpedition), story_view::kHandoffExpedition, "DEEP RELAY departure");
        test.assertEqual(story_view::handoffForMission(campaign::kPumpSurvey), story_view::kSceneNone, "survey is not cargo");
        test.assertEqual(story_view::handoffForMission(campaign::kLiftSurvey), story_view::kSceneNone, "survey is not cargo");

        test.assertEqual(story_view::jointHandoff(false), story_view::kHandoffJointMissing, "joint scene names the missing seal");
        test.assertEqual(story_view::jointHandoff(true), story_view::kHandoffJointReady, "joint scene directs core collection");
        test.assertTrue(story_view::validScene(story_view::kHandoffHome), "last handoff is a valid saved scene");
        test.assertTrue(story_view::validScene(story_view::kHandoffExpedition), "expedition briefing is a valid saved scene");
        test.assertTrue(!story_view::validScene(story_view::kSceneMax + 1), "unknown saved scene is rejected");
        test.assertTrue(!story_view::validScene(0xFF), "wide unknown scene is rejected");
        runner.add(test);
    }
}
