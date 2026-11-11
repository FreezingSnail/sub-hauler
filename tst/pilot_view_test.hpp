#pragma once

#include <string>

#include "../src/pilot_view.hpp"
#include "test.hpp"

// Pilot view math and silhouette tests. These exercise the pure screen mapping
// that the cave renderer and the firmware share; no Arduino headers are used.
namespace pilot_view_test {

inline uint8_t pixelAt(const uint8_t *buffer, int16_t x, int16_t y) {
    return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u;
}

}   // namespace pilot_view_test

inline void PilotViewSuite(TestRunner &runner) {
    using namespace pilot_view_test;

    {
        Test test("pilot camera preserves the whole-pixel camera origin");
        game::State state;
        game::resetWorld(state);

        const cave::Camera camera = pilot_view::cameraForState(state);
        test.assertEqual(camera.tileX, static_cast<int16_t>((world::SPAWN_SUB_X - game::kCameraScreenX) / world::TILE_PIXELS), "spawn camera tile x");
        test.assertEqual(camera.tileY, static_cast<int16_t>((world::SPAWN_SUB_Y - game::kCameraScreenY) / world::TILE_PIXELS), "spawn camera tile y");
        test.assertEqual(cave::pixelOriginX(camera), game::cameraPixelX(state), "spawn camera x keeps its pixel origin");
        test.assertEqual(cave::pixelOriginY(camera), game::cameraPixelY(state), "spawn camera y keeps its pixel origin");
        test.assertEqual(pilot_view::screenX(state, camera), static_cast<int16_t>(44), "spawn screen x");
        test.assertEqual(pilot_view::screenY(state, camera), static_cast<int16_t>(32), "spawn screen y");

        // Sub-tile camera offsets keep the remainder for rendering while the
        // collision tile still floors to the containing 8-px cell.
        state.cameraXQ4 = game::cameraPixelToQ4(247);
        state.cameraYQ4 = game::cameraPixelToQ4(359);
        const cave::Camera mid = pilot_view::cameraForState(state);
        test.assertEqual(mid.tileX, static_cast<int16_t>(30), "sub-tile x floors");
        test.assertEqual(mid.tileY, static_cast<int16_t>(44), "sub-tile y floors");
        test.assertEqual(mid.subX, static_cast<uint8_t>(7), "sub-tile x remainder");
        test.assertEqual(mid.subY, static_cast<uint8_t>(7), "sub-tile y remainder");
        test.assertEqual(pilot_view::screenX(state, mid), static_cast<int16_t>(state.x - 247), "sub-pixel x maps from the exact origin");
        test.assertEqual(pilot_view::screenY(state, mid), static_cast<int16_t>(state.y - 359), "sub-pixel y maps from the exact origin");

        state.cameraXQ4 = 0xFFFF;
        state.cameraYQ4 = 0xFFFF;
        const cave::Camera far = pilot_view::cameraForState(state);
        test.assertEqual(far.tileX, cave::MAX_CAMERA_TILE_X, "far x clamps to display window");
        test.assertEqual(far.tileY, cave::MAX_CAMERA_TILE_Y, "far y clamps to display window");
        test.assertEqual(cave::pixelOriginX(far), cave::MAX_CAMERA_PIXEL_X, "far pixel x clamps");
        test.assertEqual(cave::pixelOriginY(far), cave::MAX_CAMERA_PIXEL_Y, "far pixel y clamps");
        runner.add(test);
    }

    {
        Test test("HAULER art stays inside the generated sprite box");
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t buffer[kBufferBytes];
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            buffer[i] = 0;

        game::State state;
        game::resetWorld(state);
        const cave::Camera camera = pilot_view::cameraForState(state);
        pilot_view::draw(buffer, state, camera);

        const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH - pilot_view::kArtAboveHull);

        uint16_t whitePixels = 0;
        int16_t minX = cave::VIEW_WIDTH;
        int16_t maxX = -1;
        int16_t minY = cave::VIEW_HEIGHT;
        int16_t maxY = -1;
        bool outside = false;
        for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
            for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
                if (pixelAt(buffer, x, y) == 0)
                    continue;
                ++whitePixels;
                if (x < minX)
                    minX = x;
                if (x > maxX)
                    maxX = x;
                if (y < minY)
                    minY = y;
                if (y > maxY)
                    maxY = y;
                if (x < left || x >= static_cast<int16_t>(left + HAULER_WIDTH) || y < top || y >= static_cast<int16_t>(top + HAULER_HEIGHT))
                    outside = true;
            }
        }

        test.assertTrue(whitePixels > 60, "HAULER has a solid body");
        test.assertTrue(!outside, "HAULER stays in the art box");
        test.assertEqual(maxX - minX + 1, static_cast<int16_t>(HAULER_WIDTH), "HAULER art width");
        test.assertEqual(maxY - minY + 1, static_cast<int16_t>(HAULER_HEIGHT), "HAULER art height");
        test.assertEqual(maxY, static_cast<int16_t>(top + HAULER_HEIGHT - 1), "HAULER art reaches the hull baseline");
        runner.add(test);
    }

    {
        Test test("HAULER has a cargo stripe and a mirrored facing cue");
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
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH - pilot_view::kArtAboveHull);
        const int16_t hullTop = static_cast<int16_t>(top + pilot_view::kArtAboveHull);

        pilot_view::draw(empty, state, camera);
        state.cargo = 2;
        pilot_view::draw(loaded, state, camera);
        state.cargo = 0;
        state.facingLeft = true;
        pilot_view::draw(mirrored, state, camera);

        uint16_t emptyWhite = 0;
        uint16_t loadedWhite = 0;
        uint16_t stripePixels = 0;
        uint16_t mirroredDiff = 0;
        uint16_t topLeftMass = 0;
        uint16_t topRightMass = 0;
        uint16_t mirrorTopLeft = 0;
        uint16_t mirrorTopRight = 0;
        for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
            for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
                const uint8_t e = pixelAt(empty, x, y);
                const uint8_t l = pixelAt(loaded, x, y);
                const uint8_t m = pixelAt(mirrored, x, y);
                emptyWhite = static_cast<uint16_t>(emptyWhite + e);
                loadedWhite = static_cast<uint16_t>(loadedWhite + l);
                if (e != l) {
                    ++stripePixels;
                    test.assertTrue(x >= left && x < left + HAULER_WIDTH && y >= top && y < top + HAULER_HEIGHT, "HAULER stripe stays inside the art box");
                }
                if (e != m)
                    ++mirroredDiff;

                if (y >= hullTop && y < hullTop + 2) {
                    if (e != 0) {
                        if (x < left + game::kHullHalfW)
                            ++topLeftMass;
                        else
                            ++topRightMass;
                    }
                    if (m != 0) {
                        if (x < left + game::kHullHalfW)
                            ++mirrorTopLeft;
                        else
                            ++mirrorTopRight;
                    }
                }
            }
        }

        test.assertTrue(stripePixels > 0, "HAULER cargo stripe changes pixels");
        test.assertTrue(loadedWhite > emptyWhite, "HAULER loaded stripe adds white");
        test.assertTrue(topRightMass > topLeftMass, "HAULER deckhouse reads toward the bow");
        test.assertTrue(mirrorTopLeft > mirrorTopRight, "HAULER mirror flips the deckhouse");
        test.assertTrue(mirroredDiff > 0, "HAULER facing changes the silhouette");
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
