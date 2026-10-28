#pragma once

// Native title-row selection tests for the voyage entry point. The loader row
// is the supported in-sketch bootloader action; the device suite verifies the
// label and row helper, while this suite keeps the wrap/skip behavior pure.

#include "../src/features.hpp"
#include "test.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include "../src/voyage_view.hpp"

inline void VoyageViewSuite(TestRunner &runner) {
    {
        Test test("voyage title: loader row wraps with a blank save");
        voyage_flow::BootStatus blank{};
        blank.state = voyage_flow::kBootBlank;
        blank.hasValid = false;
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleLoader, "new -> loader");
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleLoader, blank), voyage_view::kTitleNewVoyage, "loader -> new");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleNewVoyage, blank), voyage_view::kTitleLoader, "previous new -> loader");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleLoader, blank), voyage_view::kTitleNewVoyage, "previous loader -> new");
        runner.add(test);
    }

    {
        Test test("voyage title: loader row joins Continue/New Voyage when valid");
        voyage_flow::BootStatus valid{};
        valid.state = voyage_flow::kBootValid;
        valid.hasValid = true;
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleContinue, valid), voyage_view::kTitleNewVoyage, "continue -> new");
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleNewVoyage, valid), voyage_view::kTitleLoader, "new -> loader");
        test.assertEqual(voyage_view::nextRow(voyage_view::kTitleLoader, valid), voyage_view::kTitleContinue, "loader -> continue");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleContinue, valid), voyage_view::kTitleLoader, "previous continue -> loader");
        test.assertEqual(voyage_view::previousRow(voyage_view::kTitleNewVoyage, valid), voyage_view::kTitleContinue, "previous new -> continue");
        runner.add(test);
    }

    {
        Test test("voyage title: loader row clears status and hint");
        test.assertTrue(voyage_view::kRowY0 + 2 * voyage_view::kRowStep + voyage_view::kRowInvertH <= voyage_view::kStatusY,
                        "three rows clear status");
        test.assertTrue(voyage_view::kStatusY + 7 <= voyage_view::kHintY, "status clears the hint");
        test.assertTrue(voyage_view::kHintY + 7 <= 64, "hint stays on screen");
        runner.add(test);
    }
}

#else

inline void VoyageViewSuite(TestRunner &) {
}

#endif   // SH_FEATURE_SAVE_VOYAGE
