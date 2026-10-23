#pragma once

#include "../src/pilot_view.hpp"
#include "test.hpp"

// Pilot view math and silhouette tests. These exercise the pure screen mapping
// that the cave renderer and the firmware share; no Arduino headers are used.
inline void PilotViewSuite(TestRunner &runner) {
    const auto pixel = [](const uint8_t *buffer, int16_t x, int16_t y) { return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u; };

    {
        Test test("pilot camera floors the whole-pixel camera to a tile");
        game::State state;
        game::resetWorld(state);

        const cave::Camera camera = pilot_view::cameraForState(state);
        test.assertEqual(camera.tileX, static_cast<int16_t>(30), "spawn camera tile x");
        test.assertEqual(camera.tileY, static_cast<int16_t>(44), "spawn camera tile y");
        test.assertEqual(pilot_view::screenX(state, camera), static_cast<int16_t>(51), "spawn screen x");
        test.assertEqual(pilot_view::screenY(state, camera), static_cast<int16_t>(36), "spawn screen y");

        // Sub-tile camera offsets floor down; the display still clamps at the
        // same far-edge tile as the cave window.
        state.cameraX = 247;
        state.cameraY = 359;
        const cave::Camera mid = pilot_view::cameraForState(state);
        test.assertEqual(mid.tileX, static_cast<int16_t>(30), "sub-tile x floors");
        test.assertEqual(mid.tileY, static_cast<int16_t>(44), "sub-tile y floors");

        state.cameraX = game::kViewW * 100;
        state.cameraY = game::kViewH * 100;
        const cave::Camera far = pilot_view::cameraForState(state);
        test.assertEqual(far.tileX, cave::MAX_CAMERA_TILE_X, "far x clamps to display window");
        test.assertEqual(far.tileY, cave::MAX_CAMERA_TILE_Y, "far y clamps to display window");
        runner.add(test);
    }

    {
        Test test("HAULER silhouette stays inside the 26x8 collision footprint");
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t buffer[kBufferBytes];
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            buffer[i] = 0;

        game::State state;
        game::resetWorld(state);
        const cave::Camera camera = pilot_view::cameraForState(state);
        pilot_view::draw(buffer, state, camera);

        const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);

        uint16_t whitePixels = 0;
        bool outside = false;
        for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
            for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
                if (pixel(buffer, x, y) == 0)
                    continue;
                ++whitePixels;
                if (x < left || x >= static_cast<int16_t>(left + game::kHullW) || y < top || y >= static_cast<int16_t>(top + game::kHullH))
                    outside = true;
            }
        }
        test.assertTrue(whitePixels > 100, "silhouette has a solid body");
        test.assertTrue(!outside, "no art escapes the playable hull box");
        runner.add(test);
    }

    {
        Test test("cargo stripes and facing flip are visible cues");
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t empty[kBufferBytes];
        uint8_t loaded[kBufferBytes];
        uint8_t mirrored[kBufferBytes];
        for (uint16_t i = 0; i < kBufferBytes; ++i) {
            empty[i] = 0;
            loaded[i] = 0;
            mirrored[i] = 0;
        }

        game::State state;
        game::resetWorld(state);
        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);

        pilot_view::draw(empty, state, camera);
        state.cargo = 2;
        pilot_view::draw(loaded, state, camera);
        state.facingLeft = true;
        pilot_view::draw(mirrored, state, camera);

        // Door A's centre stripe is black with an empty hold and white loaded.
        test.assertEqual(pixel(empty, static_cast<int16_t>(left + 5), static_cast<int16_t>(top + 5)), 0u, "empty door centre is black");
        test.assertEqual(pixel(loaded, static_cast<int16_t>(left + 5), static_cast<int16_t>(top + 5)), 1u, "loaded door centre is white");

        // The deckhouse sits toward the bow; mirroring moves it to the stern.
        test.assertEqual(pixel(loaded, static_cast<int16_t>(left + 20), top), 1u, "deckhouse is on the right when facing right");
        test.assertEqual(pixel(mirrored, static_cast<int16_t>(left + 20), top), 0u, "deckhouse flips when facing left");
        test.assertEqual(pixel(mirrored, static_cast<int16_t>(left + 5), top), 1u, "mirrored deckhouse is on the left");
        runner.add(test);
    }

    {
        Test test("offscreen silhouette is culled without writing");
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t buffer[kBufferBytes];
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            buffer[i] = 0;

        game::State state;
        game::resetWorld(state);
        state.x = -100;
        state.y = -100;
        cave::Camera camera{0, 0};
        pilot_view::draw(buffer, state, camera);

        bool untouched = true;
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            untouched = untouched && buffer[i] == 0;
        test.assertTrue(untouched, "fully offscreen sub leaves the frame untouched");
        runner.add(test);
    }
}
