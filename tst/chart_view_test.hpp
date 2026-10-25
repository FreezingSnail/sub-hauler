#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_FOG_CHART

#include "../src/chart_view.hpp"
#include "chart_test.hpp"
#include "test.hpp"

// Chart render tests: exact 2x2 water blocks, fog gating, settlement crosses,
// the sub/destination markers, deterministic blink phases, and frame clipping.
inline void ChartViewSuite(TestRunner &runner) {
    static_assert(chart_view::kFrameWidth == 128, "chart frame is display width");
    static_assert(chart_view::kFrameHeight == 64, "chart frame is display height");
    static_assert(chart_view::kFrameBytes == 1024, "chart buffer matches the framebuffer");

    const auto pixel = [](const uint8_t *buffer, int16_t x, int16_t y) {
        return (buffer[(y >> 3) * chart_view::kFrameWidth + x] >> (y & 7)) & 1u;
    };

    {
        Test test("chart lights only explored water at 2x2 pixels per cell");
        chart_test::FakeReader reader;
        reader.setWater(4, 5, true);
        reader.setWater(5, 5, true);    // unexplored water
        reader.setWater(4, 6, false);   // explored land

        chart::State fog;
        chart::reset(fog);
        chart::revealAtPixels(fog, 4 * chart::kCellPixels, 5 * chart::kCellPixels, 0);
        chart::revealAtPixels(fog, 4 * chart::kCellPixels, 6 * chart::kCellPixels, 0);

        game::State gameState;
        game::resetWorld(gameState);

        uint8_t buffer[chart_view::kFrameBytes];
        chart_view::draw(buffer, fog, gameState, reader, 0);

        test.assertEqual(pixel(buffer, 8, 10), 1u, "explored water top-left");
        test.assertEqual(pixel(buffer, 9, 10), 1u, "explored water top-right");
        test.assertEqual(pixel(buffer, 8, 11), 1u, "explored water bottom-left");
        test.assertEqual(pixel(buffer, 9, 11), 1u, "explored water bottom-right");
        test.assertEqual(pixel(buffer, 10, 10), 0u, "unexplored water stays black");
        test.assertEqual(pixel(buffer, 11, 11), 0u, "unexplored water block stays black");
        test.assertEqual(pixel(buffer, 8, 12), 0u, "explored land is not lit as water");
        test.assertEqual(reader.reads, world::CHART_BYTES, "one cart read per chart byte");
        runner.add(test);
    }

    {
        Test test("settlement cross and sub marker render at known cells");
        chart_test::FakeReader reader;
        reader.setWater(7, 9, true);   // settlement 0 cell
        chart::State fog;
        chart::reset(fog);
        chart::revealAtPixels(fog, 304, 384, 0);

        game::State gameState;
        game::resetWorld(gameState);
        gameState.x = 820;   // chart cell 20,10
        gameState.y = 420;

        uint8_t buffer[chart_view::kFrameBytes];
        chart_view::draw(buffer, fog, gameState, reader, 0);

        // Settlement 0 is at cell 7,9; its cross centre is pixel 15,19.
        test.assertEqual(pixel(buffer, 15, 19), 1u, "settlement cross centre");
        test.assertEqual(pixel(buffer, 14, 19), 1u, "settlement cross west arm");
        test.assertEqual(pixel(buffer, 16, 19), 1u, "settlement cross east arm");
        test.assertEqual(pixel(buffer, 15, 18), 1u, "settlement cross north arm");
        test.assertEqual(pixel(buffer, 15, 20), 1u, "settlement cross south arm");
        test.assertEqual(pixel(buffer, 14, 18), 0u, "cross leaves black corners");

        // The sub marker is a filled 2x2 block inside a black backing.
        test.assertEqual(pixel(buffer, 40, 20), 1u, "sub marker top-left");
        test.assertEqual(pixel(buffer, 41, 21), 1u, "sub marker bottom-right");
        test.assertEqual(pixel(buffer, 42, 20), 0u, "sub backing stays black");
        runner.add(test);
    }

    {
        Test test("unexplored settlement has no cross");
        chart_test::FakeReader reader;
        reader.setWater(7, 9, true);
        chart::State fog;
        chart::reset(fog);

        game::State gameState;
        game::resetWorld(gameState);
        gameState.x = 820;
        gameState.y = 420;

        uint8_t buffer[chart_view::kFrameBytes];
        chart_view::draw(buffer, fog, gameState, reader, 0);
        test.assertEqual(pixel(buffer, 15, 19), 0u, "fogged settlement cell stays black");
        test.assertEqual(pixel(buffer, 14, 19), 0u, "no cross arm leaks through fog");
        runner.add(test);
    }

#if SH_FEATURE_JOBS
    {
        Test test("destination X blinks from the supplied phase");
        chart_test::FakeReader reader;

        chart::State fog;
        chart::reset(fog);

        game::State gameState;
        game::resetWorld(gameState);
        gameState.x = 820;
        gameState.y = 420;
        gameState.activeJob.to = 2;   // settlement 2 is chart cell 56,12
        const int16_t xBefore = gameState.x;
        const int16_t yBefore = gameState.y;
        const uint16_t ticksBefore = gameState.ticks;
        const int16_t airBefore = gameState.air;
        const uint16_t jobElapsedBefore = gameState.activeJob.elapsed;

        uint8_t on[chart_view::kFrameBytes];
        uint8_t onAgain[chart_view::kFrameBytes];
        uint8_t off[chart_view::kFrameBytes];
        chart_view::draw(on, fog, gameState, reader, 0);
        chart_view::draw(onAgain, fog, gameState, reader, 0);
        chart_view::draw(off, fog, gameState, reader, chart_view::kBlinkMask);

        // Cell 56,12 has its centre at pixel 113,25. The destination marker is
        // visible in phase 0 and absent in phase kBlinkMask (cell unexplored).
        test.assertEqual(pixel(on, 113, 25), 1u, "destination X centre on phase 0");
        test.assertEqual(pixel(on, 112, 24), 1u, "destination X diagonal on phase 0");
        test.assertEqual(pixel(off, 113, 25), 0u, "destination X is hidden in the off phase");
        test.assertEqual(pixel(off, 112, 24), 0u, "destination diagonal is hidden off phase");

        bool samePhase = true;
        bool differentPhase = false;
        for (uint16_t i = 0; i < chart_view::kFrameBytes; ++i) {
            samePhase = samePhase && on[i] == onAgain[i];
            differentPhase = differentPhase || on[i] != off[i];
        }
        test.assertTrue(samePhase, "the same blink phase is deterministic");
        test.assertTrue(differentPhase, "the blink phases produce different frames");

        test.assertEqual(gameState.x, xBefore, "chart render does not move the sub");
        test.assertEqual(gameState.y, yBefore, "chart render does not move the sub vertically");
        test.assertEqual(gameState.ticks, ticksBefore, "chart render does not advance flight ticks");
        test.assertEqual(gameState.air, airBefore, "chart render does not consume air");
        test.assertEqual(gameState.activeJob.elapsed, jobElapsedBefore, "chart render does not age the job");
        runner.add(test);
    }
#endif

    {
        Test test("chart frame clips at the 128x64 edges");
        chart_test::FakeReader reader;
        reader.setWater(63, 31, true);

        chart::State fog;
        chart::reset(fog);
        chart::revealAtPixels(fog, world::PIXELS_X - 1, world::PIXELS_Y - 1, 0);

        game::State gameState;
        game::resetWorld(gameState);
        gameState.x = world::PIXELS_X - 1;
        gameState.y = world::PIXELS_Y - 1;

        uint8_t guarded[chart_view::kFrameBytes + 16];
        for (uint16_t i = 0; i < sizeof(guarded); ++i)
            guarded[i] = 0x5A;

        chart_view::draw(&guarded[8], fog, gameState, reader, 0);

        test.assertEqual(pixel(&guarded[8], 126, 62), 1u, "last water cell is drawn");
        bool guardsIntact = true;
        for (uint8_t i = 0; i < 8; ++i)
            guardsIntact = guardsIntact && guarded[i] == 0x5A;
        for (uint16_t i = chart_view::kFrameBytes + 8; i < sizeof(guarded); ++i)
            guardsIntact = guardsIntact && guarded[i] == 0x5A;
        test.assertTrue(guardsIntact, "markers and water stay inside the 1024-byte frame");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_FOG_CHART
