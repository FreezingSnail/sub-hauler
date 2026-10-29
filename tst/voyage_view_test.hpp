#pragma once

// Native title-row selection tests for the voyage entry point. The title only
// offers CONTINUE and NEW VOYAGE; bootloader entry is the library's DOWN check
// at power-on, not a menu action.

#include "test.hpp"


#include "../src/voyage_view.hpp"

inline void VoyageViewSuite(TestRunner &runner) {
    {
        Test test("voyage title: new voyage stays selected with a blank save");
        voyage_flow::BootStatus blank{};
        blank.state = voyage_flow::kBootBlank;
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleNewVoyage, "new stays new");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleNewVoyage, "previous new stays new");
        test.assertEqual(voyage_view::kTitleRowCount, 2, "two title rows");
        runner.add(test);
    }

    {
        Test test("voyage title: continue and new voyage wrap when valid");
        voyage_flow::BootStatus valid{};
        valid.state = voyage_flow::kBootValid;
        valid.hasValid = true;
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
}
