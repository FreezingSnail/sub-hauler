#pragma once

#include "../src/cave_render.hpp"
#include "../src/game.hpp"
#include "../src/pilot_view.hpp"
#include "../src/world.hpp"
#include "test.hpp"

#include "../src/campaign_view.hpp"
#include "../src/exploration_view.hpp"
#include "../src/hazard_view.hpp"
#include "../src/particles_view.hpp"

// Permanent pixel tests for the single camera origin shared by every world
// layer. Each layer is positioned with a deliberately fractional camera and
// checked against the same world-minus-origin mapping the terrain uses.
namespace pixel_layer_test {

constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;

inline uint8_t pixelAt(const uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= cave::VIEW_WIDTH || y >= cave::VIEW_HEIGHT)
        return 0;
    return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u;
}

inline void clear(uint8_t *buffer) {
    for (uint16_t i = 0; i < kBufferBytes; ++i)
        buffer[i] = 0;
}

// Put a world point at roughly (61,31) and keep at least one origin remainder
// non-zero so the test cannot silently fall back to tile-aligned scrolling.
inline cave::Camera cameraNear(int16_t worldX, int16_t worldY, int16_t &screenX, int16_t &screenY) {
    int16_t ox = static_cast<int16_t>(worldX - 61);
    int16_t oy = static_cast<int16_t>(worldY - 31);
    if ((ox & 7) == 0 && (oy & 7) == 0)
        ox = static_cast<int16_t>(ox - 1);
    const cave::Camera camera = cave::cameraAtPixel(ox, oy);
    screenX = static_cast<int16_t>(worldX - cave::pixelOriginX(camera));
    screenY = static_cast<int16_t>(worldY - cave::pixelOriginY(camera));
    return camera;
}

}   // namespace pixel_layer_test

inline void PixelLayerSuite(TestRunner &runner) {
    using namespace pixel_layer_test;

    {
        Test test("camera origin is precise and shared by pilot and dock mapping");
        game::State state;
        game::resetWorld(state);
        state.cameraXQ4 = game::cameraPixelToQ4(1001);
        state.cameraYQ4 = game::cameraPixelToQ4(733);
        state.x = 1100;
        state.y = 800;
        const cave::Camera camera = pilot_view::cameraForState(state);
        test.assertEqual(cave::pixelOriginX(camera), static_cast<int16_t>(1001), "pixel origin x");
        test.assertEqual(cave::pixelOriginY(camera), static_cast<int16_t>(733), "pixel origin y");
        test.assertEqual(camera.tileX, static_cast<int16_t>(125), "collision tile still floors x");
        test.assertEqual(camera.tileY, static_cast<int16_t>(91), "collision tile still floors y");
        test.assertEqual(camera.subX, static_cast<uint8_t>(1), "x remainder retained");
        test.assertEqual(camera.subY, static_cast<uint8_t>(5), "y remainder retained");
        test.assertEqual(pilot_view::screenX(state, camera), static_cast<int16_t>(99), "pilot uses exact pixel origin");

        world::Settlement settlement;
        test.assertTrue(world::settlementInfo(0, settlement), "generated settlement");
        test.assertEqual(cave::dockScreenX(camera, settlement.pixel_x), static_cast<int16_t>(settlement.pixel_x - 1001), "dock x uses the same origin");
        test.assertEqual(cave::dockScreenY(camera, settlement.pixel_y), static_cast<int16_t>(settlement.pixel_y - 733), "dock y uses the same origin");
        runner.add(test);
    }

    {
        Test test("settlement, particle and pilot pixels share one fractional origin");
        uint8_t buffer[kBufferBytes];
        game::State state;
        game::resetWorld(state);

        world::Settlement settlement;
        test.assertTrue(world::settlementInfo(0, settlement), "generated settlement");
        int16_t sx, sy;
        cave::Camera camera = cameraNear(settlement.pixel_x, settlement.pixel_y, sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "settlement camera is fractional");
        clear(buffer);
        cave::drawSettlements(buffer, camera);
        test.assertEqual(pixelAt(buffer, sx, static_cast<int16_t>(sy - 2)), 1u, "gantry ring top at pixel origin");
        test.assertEqual(pixelAt(buffer, sx, sy), 0u, "dock centre stays open");

        state.particles.x = 1111;
        state.particles.y = 777;
        state.particles.particle[0].dx = 2;
        state.particles.particle[0].dy = 3;
        state.particles.particle[0].life = 1;
        camera = cameraNear(static_cast<int16_t>(state.particles.x + 2), static_cast<int16_t>(state.particles.y + 3), sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "particle camera is fractional");
        clear(buffer);
        particles_view::draw(buffer, state, camera);
        test.assertEqual(pixelAt(buffer, sx, sy), 1u, "particle at pixel origin");

        state.x = 1200;
        state.y = 900;
        camera = cameraNear(state.x, state.y, sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "pilot camera is fractional");
        test.assertEqual(pilot_view::screenX(state, camera), sx, "pilot maps from pixel origin x");
        test.assertEqual(pilot_view::screenY(state, camera), sy, "pilot maps from pixel origin y");
        clear(buffer);
        pilot_view::draw(buffer, state, camera);
        bool hullInside = false;
        for (int16_t y = static_cast<int16_t>(sy - game::kHullHalfH); y < sy + game::kHullHalfH; ++y)
            for (int16_t x = static_cast<int16_t>(sx - game::kHullHalfW); x < sx + game::kHullHalfW; ++x)
                hullInside = hullInside || pixelAt(buffer, x, y) != 0;
        test.assertTrue(hullInside, "hull pixels surround the exact origin");
        runner.add(test);
    }

    {
        Test test("exploration markers scroll with the same pixel origin");
        uint8_t buffer[kBufferBytes];
        game::State state;
        game::reset(state);
        const exploration_data::Pickup pickup = exploration_data::pickup(0);
        int16_t sx, sy;
        const cave::Camera camera = cameraNear(pickup.x, pickup.y, sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "pickup camera is fractional");
        clear(buffer);
        exploration_view::draw(buffer, state, camera);
        test.assertEqual(pixelAt(buffer, static_cast<int16_t>(sx + 7), sy), 1u, "pickup bracket at pixel origin");
        runner.add(test);
    }

    {
        Test test("current flow marks scroll with the same pixel origin");
        uint8_t buffer[kBufferBytes];
        game::State state;
        game::resetWorld(state);
        state.ticks = 0;
        world::Current zone;
        test.assertTrue(world::currentInfo(0, zone), "generated current");
        int16_t sx, sy;
        const cave::Camera camera = cameraNear(zone.x, zone.y, sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "current camera is fractional");
        clear(buffer);
        hazard_view::drawCurrents(buffer, state, camera);
        test.assertEqual(pixelAt(buffer, static_cast<int16_t>(sx + 3), static_cast<int16_t>(sy + 3)), 1u, "flow mark anchored at pixel origin");
        runner.add(test);
    }

    {
        Test test("campaign survey rings scroll with the same pixel origin");
        uint8_t buffer[kBufferBytes];
        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "generated survey");
        game::State state;
        game::reset(state);
        int16_t sx, sy;
        const cave::Camera camera = cameraNear(survey.pixel_x, survey.pixel_y, sx, sy);
        test.assertTrue((camera.subX | camera.subY) != 0, "survey camera is fractional");
        clear(buffer);
        campaign_view::drawSurveyRings(buffer, state, camera);
        test.assertEqual(pixelAt(buffer, sx, sy), 1u, "survey centre at pixel origin");
        runner.add(test);
    }
}
