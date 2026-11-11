#pragma once

#include "../src/game.hpp"
#include "../src/music_logic.hpp"
#include "test.hpp"

inline void MusicLogicSuite(TestRunner &runner) {
    {
        Test test("music hysteresis matches the previous bounded-counter policy");
        music_logic::FlightState actual;
        music_logic::resetFlight(actual);
        auto expected = actual;
        bool exact = true;
        for (uint32_t frame = 0; frame < 100000; ++frame) {
            if (frame % 997 == 0) {
                const uint8_t cue = static_cast<uint8_t>((frame / 997) % 6);
                music_logic::setCurrent(actual, cue);
                music_logic::setCurrent(expected, cue);
            }
            const uint8_t region = static_cast<uint8_t>((frame / 137) % 4);
            music_logic::updateFlight(actual, region);
            expected.region = region;
            const uint8_t cue = music_logic::travelCue(region);
            if (cue == expected.currentCue) {
                expected.candidateCue = cue;
                expected.candidateTicks = 0;
            } else if (cue != expected.candidateCue) {
                expected.candidateCue = cue;
                expected.candidateTicks = 0;
            } else {
                if (expected.candidateTicks < music_logic::kHysteresisTicks) ++expected.candidateTicks;
                if (expected.candidateTicks >= music_logic::kHysteresisTicks) {
                    expected.currentCue = cue;
                    expected.candidateTicks = 0;
                }
            }
            exact = exact && actual.currentCue == expected.currentCue && actual.candidateCue == expected.candidateCue;
            exact = exact && actual.candidateTicks == expected.candidateTicks && actual.candidateTicks < music_logic::kHysteresisTicks;
        }
        test.assertTrue(exact, "crossings, overrides and exact transition frames match");
        runner.add(test);
    }
    {
        Test test("east rooms beyond x=255 keep their authored region");
        test.assertEqual(music_logic::nearestRegion(270 * 8 + 4, 20 * 8 + 4), music_regions::kRegionKelp, "Pump Refuge kelp region");
        test.assertEqual(music_logic::nearestRegion(300 * 8 + 4, 83 * 8 + 4), music_regions::kRegionWorks, "east works room");
        test.assertEqual(music_logic::nearestRegion(303 * 8 + 4, 36 * 8 + 4), music_regions::kRegionKelp, "east kelp room");
        runner.add(test);
    }

    {
        Test test("travel cues map authored regions to the six cue identities");
        test.assertEqual(music_logic::travelCue(music_regions::kRegionTide), music_logic::kSonar, "tide travel");
        test.assertEqual(music_logic::travelCue(music_regions::kRegionKelp), music_logic::kKelp, "kelp travel");
        test.assertEqual(music_logic::travelCue(music_regions::kRegionWorks), music_logic::kBubblejet, "works travel");
        test.assertEqual(music_logic::travelCue(music_regions::kRegionDeep), music_logic::kAbyssal, "deep travel");
        runner.add(test);
    }

    {
        Test test("station boards follow each restoration milestone");
        const uint8_t pump = campaign::kMilestonePump;
        const uint8_t lift = campaign::kMilestoneLift;
        const uint8_t beacon = campaign::kMilestoneBeacon;
        test.assertEqual(music_logic::stationCue(campaign::kTidewall, 0), music_logic::kHarbor, "Tidewall before repairs");
        test.assertEqual(music_logic::stationCue(campaign::kTidewall, pump | lift | beacon), music_logic::kHarbor, "Tidewall after repairs");
        test.assertEqual(music_logic::stationCue(campaign::kBenthos, 0), music_logic::kHarbor, "Benthos before pump");
        test.assertEqual(music_logic::stationCue(campaign::kBenthos, campaign::kMilestoneFirst), music_logic::kHarbor, "FIRST alone does not restore Benthos");
        test.assertEqual(music_logic::stationCue(campaign::kBenthos, pump), music_logic::kTidepool, "Benthos after pump");
        test.assertEqual(music_logic::stationCue(campaign::kCormorant, 0), music_logic::kHarbor, "Cormorant before lift");
        test.assertEqual(music_logic::stationCue(campaign::kCormorant, pump), music_logic::kHarbor, "pump alone does not restore Cormorant");
        test.assertEqual(music_logic::stationCue(campaign::kCormorant, lift), music_logic::kTidepool, "Cormorant after lift");
        test.assertEqual(music_logic::stationCue(campaign::kDeepAnchor, 0), music_logic::kAbyssal, "Deep Anchor before beacon");
        test.assertEqual(music_logic::stationCue(campaign::kDeepAnchor, pump | lift), music_logic::kAbyssal, "Deep Anchor stays abyssal until beacon");
        test.assertEqual(music_logic::stationCue(campaign::kDeepAnchor, beacon), music_logic::kTidepool, "Deep Anchor after beacon");
        test.assertEqual(music_logic::stationCue(campaign::kPumpRefuge, 0), music_logic::kTidepool, "Pump Refuge service berth");
        test.assertEqual(music_logic::stationCue(campaign::kLowerGantry, 0), music_logic::kTidepool, "Lower Gantry service berth");
        test.assertEqual(music_logic::stationCue(0xFF, 0), music_logic::kHarbor, "undocked fallback keeps the harbor cue");
        runner.add(test);
    }

    {
        Test test("delivery decks switch to tidepool but surveys and FIRST do not");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardPump), music_logic::kTidepool, "PUMP deck");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardLift), music_logic::kTidepool, "LIFT deck");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardBeacon), music_logic::kTidepool, "BEACON deck");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardFirst), music_logic::kCueNone, "FIRST keeps its path");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardPumpSurvey), music_logic::kCueNone, "Pump survey keeps its path");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardLiftSurvey), music_logic::kCueNone, "Lift survey keeps its path");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardHome), music_logic::kCueNone, "HOME keeps the ending path");
        test.assertEqual(music_logic::deliveryStoryCue(story_view::kCardNone), music_logic::kCueNone, "no pending deck");
        runner.add(test);
    }

    {
        Test test("sampling a passage is deterministic from different prior regions");
        const int16_t passageX = 53 * 8 + 4;   // authored Tidewall-Lantern passage
        const int16_t passageY = 14 * 8 + 4;
        music_logic::FlightState first;
        music_logic::FlightState second;
        music_logic::resetFlight(first, music_logic::kHarbor);
        music_logic::resetFlight(second, music_logic::kTidepool);
        first.region = music_regions::kRegionTide;
        second.region = music_regions::kRegionDeep;
        game::State state;
        game::reset(state);
        state.x = passageX;
        state.y = passageY;
        if (music_logic::evaluateDue(first))
            first.region = music_logic::nearestRegion(state.x, state.y);
        if (music_logic::evaluateDue(second))
            second.region = music_logic::nearestRegion(state.x, state.y);
        music_logic::updateFlight(first, first.region);
        music_logic::updateFlight(second, second.region);
        test.assertEqual(first.region, second.region, "passage owner ignores prior region");
        test.assertEqual(music_logic::nearestRegion(passageX, passageY), first.region, "direct sample matches");
        runner.add(test);
    }

    {
        Test test("room boundary and connecting passage ownership is stable");
        // Pump Refuge room 8 is centred at (270,20), radius 5 tiles.
        test.assertEqual(music_logic::nearestRegion(270 * 8 + 4 + 5 * 8 - 1, 20 * 8 + 4), music_regions::kRegionKelp, "inside east boundary");
        const int16_t outsideX = 270 * 8 + 4 + 5 * 8 + 1;
        test.assertEqual(music_logic::nearestRegion(outsideX, 20 * 8 + 4), music_logic::nearestRegion(outsideX, 20 * 8 + 4), "passage sample is repeatable");
        runner.add(test);
    }

    {
        Test test("blocking transaction keeps a pre-existing pause");
        bool paused = true;
        const music_logic::PauseToken nested = music_logic::pauseForTransaction(paused);
        music_logic::resumeAfterTransaction(paused, nested);
        test.assertTrue(paused, "nested transaction does not resume a frozen cue");
        paused = false;
        const music_logic::PauseToken outer = music_logic::pauseForTransaction(paused);
        test.assertTrue(paused, "outer transaction pauses");
        music_logic::resumeAfterTransaction(paused, outer);
        test.assertTrue(!paused, "outer transaction resumes once");
        runner.add(test);
    }

    {
        Test test("two seconds of stable flight replace a travel cue");
        game::State state;
        game::reset(state);
        music_logic::FlightState flight;
        music_logic::resetFlight(flight, music_logic::kHarbor);
        state.x = 168 * 8 + 4;   // Kelp Cathedral
        state.y = 18 * 8 + 4;
        for (uint8_t i = 0; i < 120; ++i) {
            if (music_logic::evaluateDue(flight))
                flight.region = music_logic::nearestRegion(state.x, state.y);
            music_logic::updateFlight(flight, flight.region);
        }
        test.assertEqual(flight.currentCue, music_logic::kHarbor, "cue remains until the full hold");
        if (music_logic::evaluateDue(flight))
            flight.region = music_logic::nearestRegion(state.x, state.y);
        music_logic::updateFlight(flight, flight.region);
        test.assertEqual(flight.currentCue, music_logic::kKelp, "stable kelp region switches after the hold");
        runner.add(test);
    }

    {
        Test test("a crossing shorter than the hold does not restart the cue");
        game::State state;
        game::reset(state);
        music_logic::FlightState flight;
        music_logic::resetFlight(flight, music_logic::kHarbor);
        state.x = 168 * 8 + 4;
        state.y = 18 * 8 + 4;   // kelp
        for (uint8_t i = 0; i < 60; ++i) {
            if (music_logic::evaluateDue(flight))
                flight.region = music_logic::nearestRegion(state.x, state.y);
            music_logic::updateFlight(flight, flight.region);
        }
        state.x = 38 * 8 + 4;
        state.y = 24 * 8 + 4;   // tide
        for (uint8_t i = 0; i < 30; ++i) {
            if (music_logic::evaluateDue(flight))
                flight.region = music_logic::nearestRegion(state.x, state.y);
            music_logic::updateFlight(flight, flight.region);
        }
        test.assertEqual(flight.currentCue, music_logic::kHarbor, "short crossing keeps the old cue");
        runner.add(test);
    }
}
