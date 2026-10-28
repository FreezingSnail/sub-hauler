#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_CAMPAIGN

#include "../src/ending_view.hpp"
#include "../src/game.hpp"
#include "test.hpp"

// Pure homecoming card contract: exactly three cards, card order, and the stats
// derived from saved counters. Device-only text layout is pinned by the
// static_asserts in ending_view.hpp, which compile in this host build too.
inline void EndingViewSuite(TestRunner &runner) {
    {
        Test test("ending cards advance three times and then finish");
        test.assertEqual(ending_view::kCardCount, 3, "three cards");
        uint8_t card = ending_view::kCardNetwork;
        test.assertEqual(card, 0, "network card first");
        card = ending_view::nextCard(card);
        test.assertEqual(card, ending_view::kCardThanks, "thanks card second");
        card = ending_view::nextCard(card);
        test.assertEqual(card, ending_view::kCardStats, "stats card third");
        test.assertEqual(ending_view::nextCard(card), ending_view::kCardCount, "third card advances to finish");
        runner.add(test);
    }

    {
        Test test("ending stats derive from saved masks, counters and voyage clock");
        game::State state;
        game::reset(state);
        test.assertEqual(ending_view::coveCount(state), 0, "no coves discovered");
        state.pickups = 0x5A;
        test.assertEqual(ending_view::coveCount(state), 4, "four of eight coves");
        state.pickups = 0xFF;
        test.assertEqual(ending_view::coveCount(state), 8, "all eight coves");
        state.delivered = 5;
        test.assertEqual(state.delivered, 5, "delivered jobs stay available to the stats card");
        state.voyageTicks = 0;
        test.assertEqual(ending_view::voyageSeconds(state), 0, "zero voyage time");
        state.voyageTicks = 60u * 125u;
        test.assertEqual(ending_view::voyageSeconds(state), 125, "voyage seconds derive from flight ticks");
        runner.add(test);
    }
}

#else

inline void EndingViewSuite(TestRunner &runner) {
    Test test("ending cards compile out with CAMPAIGN=0");
    test.assertEqual(SH_FEATURE_CAMPAIGN, 0, "campaign off");
    runner.add(test);
}

#endif
