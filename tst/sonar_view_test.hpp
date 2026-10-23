#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_SONAR

#include "../src/chart_view.hpp"
#include "../src/sonar.hpp"
#include "../src/sonar_view.hpp"
#include "chart_test.hpp"
#include "test.hpp"

// Sonar ring render tests: phase/radius from the cooldown alone, midpoint
// circle pixels, 128x64 clipping with guard bytes, determinism, and proof that
// a paused chart frame never ticks the sonar clock.
inline void SonarViewSuite(TestRunner &runner) {
    static_assert(sonar_view::kFrameWidth == 128, "sonar frame is display width");
    static_assert(sonar_view::kFrameHeight == 64, "sonar frame is display height");

    const auto pixel = [](const uint8_t *buffer, int16_t x, int16_t y) {
        return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u;
    };

    {
        Test test("ring radius expands with cooldown phase inside the ring window");
        game::State state;
        game::reset(state);
        state.x = 64;
        state.y = 32;
        state.cameraX = 0;
        state.cameraY = 0;
        const cave::Camera camera = pilot_view::cameraForState(state);

        uint8_t early[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8] = {0};
        state.sonarCooldown = sonar::kCooldownTicks;
        test.assertEqual(sonar::ringRadius(state), 4, "first ring tick is the minimum radius");
        sonar_view::draw(early, state, camera);
        test.assertEqual(pixel(early, 64, 28), 1u, "ring top at radius 4");
        test.assertEqual(pixel(early, 68, 32), 1u, "ring east at radius 4");
        test.assertEqual(pixel(early, 64, 32), 0u, "ring centre stays clear for the boat");
        test.assertEqual(pixel(early, 64, 29), 0u, "radius 4 is a one-pixel circle");

        uint8_t late[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8] = {0};
        state.sonarCooldown = static_cast<uint8_t>(sonar::kCooldownTicks - (sonar::kRingTicks - 1));
        test.assertEqual(sonar::ringRadius(state), 20, "last ring tick is the maximum radius");
        sonar_view::draw(late, state, camera);
        test.assertEqual(pixel(late, 64, 12), 1u, "ring top at radius 20");
        test.assertEqual(pixel(late, 84, 32), 1u, "ring east at radius 20");
        test.assertEqual(pixel(late, 64, 28), 0u, "the old radius is not still lit");
        runner.add(test);
    }

    {
        Test test("ring is hidden after the window and is deterministic");
        game::State state;
        game::reset(state);
        state.x = 64;
        state.y = 32;
        const cave::Camera camera = pilot_view::cameraForState(state);

        uint8_t hidden[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8] = {0};
        state.sonarCooldown = static_cast<uint8_t>(sonar::kCooldownTicks - sonar::kRingTicks);
        test.assertTrue(!sonar::ringVisible(state), "cooldown after the ring window draws nothing");
        test.assertEqual(sonar::ringRadius(state), 0, "hidden ring radius is zero");
        sonar_view::draw(hidden, state, camera);
        bool allClear = true;
        for (uint16_t i = 0; i < sizeof(hidden); ++i)
            allClear = allClear && hidden[i] == 0;
        test.assertTrue(allClear, "hidden ring writes no pixels");

        state.sonarCooldown = 0;
        sonar_view::draw(hidden, state, camera);
        allClear = true;
        for (uint16_t i = 0; i < sizeof(hidden); ++i)
            allClear = allClear && hidden[i] == 0;
        test.assertTrue(allClear, "zero cooldown writes no pixels");

        state.sonarCooldown = sonar::kCooldownTicks - 5;
        uint8_t first[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8] = {0};
        uint8_t second[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8] = {0};
        sonar_view::draw(first, state, camera);
        sonar_view::draw(second, state, camera);
        bool same = true;
        for (uint16_t i = 0; i < sizeof(first); ++i)
            same = same && first[i] == second[i];
        test.assertTrue(same, "the same cooldown phase produces identical pixels");
        runner.add(test);
    }

    {
        Test test("ring clips at frame edges and stays inside the framebuffer");
        game::State state;
        game::reset(state);
        state.x = 1;
        state.y = 1;
        state.cameraX = 0;
        state.cameraY = 0;
        state.sonarCooldown = static_cast<uint8_t>(sonar::kCooldownTicks - (sonar::kRingTicks - 1));
        const cave::Camera camera = pilot_view::cameraForState(state);

        uint8_t guarded[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 16];
        for (uint16_t i = 0; i < sizeof(guarded); ++i)
            guarded[i] = 0x5A;

        sonar_view::draw(&guarded[8], state, camera);

        test.assertEqual(pixel(&guarded[8], 1, 21), 1u, "in-bounds arc near the top-left corner is drawn");
        test.assertEqual(pixel(&guarded[8], 21, 1), 1u, "in-bounds arc east of the centre is drawn");
        bool guardsIntact = true;
        for (uint8_t i = 0; i < 8; ++i)
            guardsIntact = guardsIntact && guarded[i] == 0x5A;
        for (uint16_t i = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 8; i < sizeof(guarded); ++i)
            guardsIntact = guardsIntact && guarded[i] == 0x5A;
        test.assertTrue(guardsIntact, "clipping never writes outside the 1024-byte frame");
        runner.add(test);
    }

    {
        Test test("an open chart frame never ticks the sonar cooldown");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);

        state.x = 1000;
        state.y = 800;
        test.assertEqual(sonar::fire(state, fog, 0), sonar::kFired, "ping starts a cooldown");
        const uint8_t cooldownBefore = state.sonarCooldown;
        const uint16_t ticksBefore = state.ticks;
        const int16_t airBefore = state.air;

        chart_test::FakeReader reader;
        uint8_t buffer[chart_view::kFrameBytes] = {0};
        chart_view::draw(buffer, fog, state, reader, 0);

        test.assertEqual(state.sonarCooldown, cooldownBefore, "chart frame does not tick the sonar cooldown");
        test.assertEqual(state.ticks, ticksBefore, "chart frame does not advance flight ticks");
        test.assertEqual(state.air, airBefore, "chart frame does not spend or refill air");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_SONAR
