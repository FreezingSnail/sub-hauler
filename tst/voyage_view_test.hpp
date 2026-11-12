#pragma once

// Native title-row selection tests for the voyage entry point. The title only
// offers CONTINUE and NEW VOYAGE; bootloader entry is the library's DOWN check
// at power-on, not a menu action.

#include "test.hpp"


#include "../src/voyage_view.hpp"

inline void VoyageViewSuite(TestRunner &runner) {
    {
        Test test("voyage title: new voyage stays selected with a blank save");
        const voyage_flow::BootState blank = voyage_flow::kBootBlank;
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleNewVoyage, "new stays new");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleNewVoyage, "previous new stays new");
        test.assertEqual(voyage_view::kTitleRowCount, 2, "two title rows");
        runner.add(test);
    }

    {
        Test test("voyage title: continue and new voyage wrap when written");
        const voyage_flow::BootState valid = voyage_flow::kBootWritten;
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleContinue, valid), voyage_view::kTitleNewVoyage, "continue -> new");
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleNewVoyage, valid), voyage_view::kTitleContinue, "new -> continue");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleContinue, valid), voyage_view::kTitleNewVoyage, "previous continue -> new");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleNewVoyage, valid), voyage_view::kTitleContinue, "previous new -> continue");
        runner.add(test);
    }

    {
        Test test("voyage title: two rows clear status and hint");
        test.assertTrue(voyage_view::kRowY0 + voyage_view::kRowStep + voyage_view::kRowInvertH <= voyage_view::kStatusY,
                        "two rows clear status");
        test.assertTrue(voyage_view::kStatusY + 7 <= voyage_view::kHintY, "status clears the hint");
        test.assertTrue(voyage_view::kHintY + 7 <= 64, "hint stays on screen");
        runner.add(test);
    }

    {
        Test test("voyage title: sonar echo pulses inward and clears");
        test.assertEqual(voyage_view::titleEchoRadius(0), 2, "first frame starts the ring");
        test.assertEqual(voyage_view::titleEchoRadius(8), 3, "ring grows after eight frames");
        test.assertEqual(voyage_view::titleEchoRadius(16), 4, "ring keeps growing");
        test.assertEqual(voyage_view::titleEchoRadius(24), 5, "ring reaches the needle");
        test.assertEqual(voyage_view::titleEchoRadius(32), 6, "ring reaches the inner dial");
        test.assertEqual(voyage_view::titleEchoRadius(40), 7, "ring tops out inside the dial");
        test.assertEqual(voyage_view::titleEchoRadius(48), 0, "ring clears without touching the dial");
        test.assertEqual(voyage_view::titleEchoRadius(56), 0, "blank frame holds before the next pulse");
        test.assertEqual(voyage_view::titleEchoRadius(64), 2, "echo restarts from the centre");
        runner.add(test);
    }
}
