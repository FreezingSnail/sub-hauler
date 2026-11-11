#pragma once



#include "../src/ending_view.hpp"
#include "test.hpp"

// Pure homecoming card contract: exactly two cards in network-then-thanks order.
// The retired voyage-stats card is no longer part of the sequence. Device-only
// panel layout is pinned by the static_asserts in ending_view.hpp, which compile
// in this host build too.
inline void EndingViewSuite(TestRunner &runner) {
    {
        Test test("ending cards advance twice and then finish");
        test.assertEqual(ending_view::kCardCount, 2, "two cards");
        uint8_t card = ending_view::kCardNetwork;
        test.assertEqual(card, 0, "network card first");
        card = ending_view::nextCard(card);
        test.assertEqual(card, ending_view::kCardThanks, "thanks card second");
        test.assertEqual(ending_view::nextCard(card), ending_view::kCardCount, "thanks advances to finish");
        test.assertEqual(ending_view::kCardCount, 2, "voyage stats no longer has a slot");
        runner.add(test);
    }
}
