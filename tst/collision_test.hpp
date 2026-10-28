#pragma once

#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "test.hpp"
#include "world_test.hpp"

// World/collision and camera rules tests. The fixture is a synthetic 320x160
// tile world (the same shape as the baked cart) supplied through the Reader
// interface, so these tests never touch generated blob contents.
namespace collision_test {

using Fixture = world_test::FakeWorld;

inline void fillTiles(Fixture &fake, int16_t tx0, int16_t ty0, int16_t tx1, int16_t ty1, bool solid) {
    for (int16_t ty = ty0; ty <= ty1; ++ty)
        for (int16_t tx = tx0; tx <= tx1; ++tx)
            fake.setTile(tx, ty, solid);
}

// Position the boat without changing tuning, and snap the camera exactly as
// resetWorld would after a spawn.
inline void place(game::State &state, int16_t x, int16_t y) {
    state.x = x;
    state.y = y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    game::snapCamera(state);
}

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

inline game::Input right() {
    return game::Input{false, false, false, true};
}

inline game::Input down() {
    return game::Input{false, true, false, false};
}

}   // namespace collision_test

inline void CollisionSuite(TestRunner &runner) {
    using namespace collision_test;

    {
        Test test("world step preserves pure physics in open water");
        Fixture fake;
        game::State pure;
        game::State stepped;
        game::reset(pure);
        game::reset(stepped);

        for (int i = 0; i < 600; ++i) {
            game::Input input = idle();
            if (i < 30)
                input.up = true;
            else if (i < 60)
                input.down = true;
            else if (i < 300)
                input.right = true;
            else if (i < 360)
                input.left = true;
            else if (i < 430)
                input.up = true;

            game::step(pure, input);
            game::stepWorld(stepped, input, fake);
        }

        test.assertEqual(stepped.x, pure.x, "x matches pure step");
        test.assertEqual(stepped.y, pure.y, "y matches pure step");
        test.assertEqual(stepped.xFrac, pure.xFrac, "x remainder matches");
        test.assertEqual(stepped.yFrac, pure.yFrac, "y remainder matches");
        test.assertEqual(stepped.vxQ8, pure.vxQ8, "vx matches");
        test.assertEqual(stepped.vyQ8, pure.vyQ8, "vy matches");
        test.assertEqual(stepped.ballast, pure.ballast, "ballast matches");
        test.assertEqual(stepped.air, pure.air, "air matches");
        test.assertEqual(stepped.ticks, pure.ticks, "one step is one tick");
        test.assertEqual(stepped.impactAxis, 0, "open water has no impact");
        test.assertTrue(stepped.cameraX >= 0 && stepped.cameraX <= game::kCameraMaxX, "camera x in bounds");
        test.assertTrue(stepped.cameraY >= 0 && stepped.cameraY <= game::kCameraMaxY, "camera y in bounds");
        runner.add(test);
    }

    {
        Test test("right wall stops the hull at the last valid pixel");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);   // solid column at x=80
        game::State state;
        game::resetWorld(state);
        place(state, 64, 80);

        bool crossed = false;
        for (int i = 0; i < 600; ++i) {
            game::stepWorld(state, right(), fake);
            if (state.x > 67)
                crossed = true;
        }
        test.assertTrue(!crossed, "hull never enters the wall");
        test.assertEqual(state.x, 67, "stops with the 13 px half-width clear");
        test.assertEqual(state.xFrac, 0, "blocked remainder is zero");
        test.assertEqual(state.vxQ8, 0, "blocked axis velocity is zero");
        test.assertEqual(state.impactAxis, 1, "impact recorded on x");
        test.assertTrue(state.impactSpeedQ8 > 0, "impact speed captured");
        runner.add(test);
    }

    {
        Test test("max thrust cannot tunnel through a wall");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);
        game::State state;
        game::resetWorld(state);
        place(state, 60, 80);
        state.vxQ8 = game::topSpeedQ8For(0);
        state.xFrac = game::kSubpixelOne - 1;

        bool crossed = false;
        for (int i = 0; i < 60; ++i) {
            game::stepWorld(state, idle(), fake);
            if (state.x > 67)
                crossed = true;
        }
        test.assertTrue(!crossed, "no overshoot at top speed");
        test.assertEqual(state.x, 67, "resolves to last valid pixel");
        test.assertEqual(state.impactAxis, 1, "wall impact is horizontal");
        test.assertTrue(state.impactSpeedQ8 > 0, "fast impact reported");
        test.assertEqual(state.xFrac, 0, "remainder cleared after impact");
        runner.add(test);
    }

    {
        Test test("corner blocks each axis separately");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);    // solid vertical face
        fillTiles(fake, 0, 3, 319, 3, true);      // solid floor below the boat
        game::State state;
        game::resetWorld(state);
        place(state, 60, 20);

        for (int i = 0; i < 600; ++i)
            game::stepWorld(state, game::Input{false, true, false, true}, fake);

        test.assertEqual(state.x, 67, "x stops at the vertical face");
        test.assertEqual(state.y, 20, "y stops at the horizontal face");
        test.assertEqual(state.vxQ8, 0, "x velocity zeroed at corner");
        test.assertEqual(state.vyQ8, 0, "y velocity zeroed at corner");
        test.assertEqual(state.impactAxis, 2, "last blocked axis is y");
        test.assertTrue(state.impactSpeedQ8 > 0, "the blocked axis reports an impact speed");
        runner.add(test);
    }

    {
        Test test("3-tile shaft is too narrow for the 26 px hull");
        Fixture fake;
        fillTiles(fake, 0, 12, 319, 20, true);   // slab rows 12..20
        fillTiles(fake, 11, 12, 13, 20, false);  // 24 px shaft
        game::State state;
        game::resetWorld(state);
        place(state, 104, 88);

        for (int i = 0; i < 300; ++i)
            game::stepWorld(state, down(), fake);

        test.assertEqual(state.y, 92, "hull stops above the narrow shaft");
        test.assertEqual(state.vyQ8, 0, "vertical velocity zeroed");
        test.assertEqual(state.impactAxis, 2, "impact is vertical");
        runner.add(test);
    }

    {
        Test test("4-tile shaft admits the hull");
        Fixture fake;
        fillTiles(fake, 0, 12, 319, 20, true);
        fillTiles(fake, 11, 12, 14, 20, false);  // 32 px shaft
        game::State state;
        game::resetWorld(state);
        place(state, 104, 88);

        for (int i = 0; i < 300; ++i)
            game::stepWorld(state, down(), fake);
        test.assertTrue(state.y > 100, "travels through the shaft");
        test.assertTrue(state.impactAxis != 2, "does not catch on the shaft mouth");
        test.assertEqual(state.x, 104, "shaft keeps the boat centred");
        runner.add(test);
    }

    {
        Test test("camera follows with a minimum step and clamps at edges");
        Fixture fake;
        game::State state;
        game::resetWorld(state);

        test.assertEqual(state.cameraX, game::cameraTargetX(state), "spawn camera x snapped");
        test.assertEqual(state.cameraY, game::cameraTargetY(state), "spawn camera y snapped");

        place(state, 200, 200);
        state.cameraX = 0;
        state.cameraY = 0;
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.cameraX, game::cameraTargetX(state) / 4, "integer quarter lerp on x");
        test.assertEqual(state.cameraY, game::cameraTargetY(state) / 4, "integer quarter lerp on y");

        // A one-pixel error still moves: the camera cannot stick short of its
        // target on the integer grid.
        state.cameraX = static_cast<int16_t>(game::cameraTargetX(state) - 1);
        state.cameraY = static_cast<int16_t>(game::cameraTargetY(state) + 1);
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.cameraX, game::cameraTargetX(state), "one pixel error moves x");
        test.assertEqual(state.cameraY, game::cameraTargetY(state), "one pixel error moves y");

        place(state, 2500, 1250);
        state.cameraX = 0;
        state.cameraY = 0;
        for (int i = 0; i < 200; ++i) {
            game::stepWorld(state, idle(), fake);
            test.assertTrue(state.cameraX >= 0 && state.cameraX <= game::kCameraMaxX, "camera x stays in world");
            test.assertTrue(state.cameraY >= 0 && state.cameraY <= game::kCameraMaxY, "camera y stays in world");
        }
        test.assertEqual(state.cameraX, game::kCameraMaxX, "right edge clamps");
        test.assertEqual(state.cameraY, game::kCameraMaxY, "bottom edge clamps");

        place(state, 10, 10);
        state.cameraX = game::kCameraMaxX;
        state.cameraY = game::kCameraMaxY;
        for (int i = 0; i < 200; ++i)
            game::stepWorld(state, idle(), fake);
        test.assertEqual(state.cameraX, 0, "left edge clamps");
        test.assertEqual(state.cameraY, 0, "top edge clamps");
        runner.add(test);
    }

    {
        Test test("camera look-ahead leads travel and eases to a stop");
        game::State state;
        game::resetWorld(state);
        place(state, 500, 300);

        // Zero velocity reproduces the centred framing, including at spawn.
        test.assertEqual(game::cameraLookAheadQ8(0), 0, "zero velocity has no look-ahead");
        test.assertEqual(game::cameraLookAheadQ8(state.vxQ8), 0, "idle x look-ahead is zero");
        test.assertEqual(game::cameraLookAheadQ8(state.vyQ8), 0, "idle y look-ahead is zero");
        test.assertEqual(game::cameraLookAheadQ8(1), 0, "+1 truncates to zero");
        test.assertEqual(game::cameraLookAheadQ8(-1), 0, "-1 truncates to zero");
        test.assertEqual(game::cameraLookAheadQ8(511), 0, "+511 truncates to zero");
        test.assertEqual(game::cameraLookAheadQ8(-511), 0, "-511 truncates to zero");
        test.assertEqual(game::cameraLookAheadQ8(512), 1, "+512 leads one pixel");
        test.assertEqual(game::cameraLookAheadQ8(-512), -1, "-512 leads one pixel symmetrically");
        test.assertEqual(game::cameraTargetX(state), 456, "idle target keeps centred framing");
        test.assertEqual(game::cameraTargetY(state), 268, "idle target keeps centred framing y");

        state.vxQ8 = game::kMaxVxBaseQ8;
        state.vyQ8 = game::kMaxVyQ8;
        test.assertEqual(game::cameraLookAheadQ8(state.vxQ8), 15, "base top speed leads 15 px");
        test.assertEqual(game::cameraLookAheadQ8(state.vyQ8), 10, "max vertical speed leads 10 px");
        test.assertEqual(game::cameraTargetX(state), 471, "forward target adds x look-ahead");
        test.assertEqual(game::cameraTargetY(state), 278, "down target adds y look-ahead");

        for (int i = 0; i < 100; ++i)
            game::updateCamera(state);
        test.assertEqual(state.cameraX, 471, "camera converges to forward look-ahead");
        test.assertEqual(state.cameraY, 278, "camera converges to down look-ahead");

        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t left = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS - game::kHullHalfH);
        test.assertTrue(pilot_view::visible(left, top), "hull stays visible at full look-ahead");
        test.assertEqual(left, 23, "look-ahead hull left stays on screen");
        test.assertEqual(top, 24, "look-ahead hull top stays on screen");
        runner.add(test);
    }

    {
        Test test("camera reversal moves partway then converges to the opposite lead");
        game::State state;
        game::resetWorld(state);
        place(state, 500, 300);
        state.vxQ8 = game::kMaxVxBaseQ8;
        state.vyQ8 = game::kMaxVyQ8;
        for (int i = 0; i < 100; ++i)
            game::updateCamera(state);

        const int16_t aheadX = state.cameraX;
        const int16_t aheadY = state.cameraY;
        state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
        state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
        test.assertEqual(game::cameraTargetX(state), 441, "reverse target flips x lead");
        test.assertEqual(game::cameraTargetY(state), 258, "reverse target flips y lead");

        game::updateCamera(state);
        test.assertEqual(state.cameraX, 464, "reverse moves only one quarter step");
        test.assertEqual(state.cameraY, 273, "reverse moves only one quarter step y");
        test.assertTrue(state.cameraX > 441 && state.cameraX < aheadX, "reversed x stays between targets");
        test.assertTrue(state.cameraY > 258 && state.cameraY < aheadY, "reversed y stays between targets");

        for (int i = 0; i < 100; ++i) {
            game::updateCamera(state);
            test.assertTrue(state.cameraX >= 441 && state.cameraX <= aheadX, "reversed x never snaps");
            test.assertTrue(state.cameraY >= 258 && state.cameraY <= aheadY, "reversed y never snaps");
        }
        test.assertEqual(state.cameraX, 441, "camera converges to reverse x lead");
        test.assertEqual(state.cameraY, 258, "camera converges to reverse y lead");

        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t left = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS - game::kHullHalfH);
        test.assertTrue(pilot_view::visible(left, top), "reversed hull stays visible");
        runner.add(test);
    }

    {
        Test test("camera stop eases back to the centred target");
        game::State state;
        game::resetWorld(state);
        place(state, 500, 300);
        state.vxQ8 = game::kMaxVxBaseQ8;
        state.vyQ8 = game::kMaxVyQ8;
        for (int i = 0; i < 100; ++i)
            game::updateCamera(state);

        state.vxQ8 = 0;
        state.vyQ8 = 0;
        test.assertEqual(game::cameraTargetX(state), 456, "stopped target returns to centre");
        test.assertEqual(game::cameraTargetY(state), 268, "stopped target returns to centre y");

        game::updateCamera(state);
        test.assertEqual(state.cameraX, 468, "stop moves only one quarter step");
        test.assertEqual(state.cameraY, 276, "stop moves only one quarter step y");
        test.assertTrue(state.cameraX > 456, "stopped x eases back");
        test.assertTrue(state.cameraY > 268, "stopped y eases back");

        for (int i = 0; i < 100; ++i) {
            game::updateCamera(state);
            test.assertTrue(state.cameraX >= 456, "stopped x never overshoots centre");
            test.assertTrue(state.cameraY >= 268, "stopped y never overshoots centre");
        }
        test.assertEqual(state.cameraX, 456, "camera settles on centred x");
        test.assertEqual(state.cameraY, 268, "camera settles on centred y");
        runner.add(test);
    }

    {
        Test test("camera look-ahead clamps at world edges and keeps the hull visible");
        game::State state;
        game::resetWorld(state);

        // Lead right/down from an unfollowed camera and park on both maxima.
        place(state, 2500, 1250);
        state.cameraX = 0;
        state.cameraY = 0;
        state.vxQ8 = game::kMaxVxBaseQ8;
        state.vyQ8 = game::kMaxVyQ8;
        test.assertEqual(game::cameraTargetX(state), game::kCameraMaxX, "right edge target clamps");
        test.assertEqual(game::cameraTargetY(state), game::kCameraMaxY, "bottom edge target clamps");
        for (int i = 0; i < 200; ++i) {
            game::updateCamera(state);
            test.assertTrue(state.cameraX >= 0 && state.cameraX <= game::kCameraMaxX, "camera x stays in world");
            test.assertTrue(state.cameraY >= 0 && state.cameraY <= game::kCameraMaxY, "camera y stays in world");
        }
        test.assertEqual(state.cameraX, game::kCameraMaxX, "right edge camera clamps");
        test.assertEqual(state.cameraY, game::kCameraMaxY, "bottom edge camera clamps");
        {
            const cave::Camera camera = pilot_view::cameraForState(state);
            const int16_t left = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS - game::kHullHalfW);
            const int16_t top = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS - game::kHullHalfH);
            test.assertTrue(pilot_view::visible(left, top), "hull visible at right/bottom edge");
        }

        // Lead left/up from the far corner and park on both minima.
        place(state, 50, 30);
        state.cameraX = game::kCameraMaxX;
        state.cameraY = game::kCameraMaxY;
        state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
        state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
        test.assertEqual(game::cameraTargetX(state), 0, "left edge target clamps");
        test.assertEqual(game::cameraTargetY(state), 0, "top edge target clamps");
        for (int i = 0; i < 200; ++i)
            game::updateCamera(state);
        test.assertEqual(state.cameraX, 0, "left edge camera clamps");
        test.assertEqual(state.cameraY, 0, "top edge camera clamps");
        {
            const cave::Camera camera = pilot_view::cameraForState(state);
            const int16_t left = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS - game::kHullHalfW);
            const int16_t top = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS - game::kHullHalfH);
            test.assertTrue(pilot_view::visible(left, top), "hull visible at left/top edge");
        }
        runner.add(test);
    }

    {
        Test test("zero velocity spawn target matches snapCamera");
        game::State state;
        game::resetWorld(state);
        test.assertEqual(state.vxQ8, 0, "spawn velocity x is zero");
        test.assertEqual(state.vyQ8, 0, "spawn velocity y is zero");
        test.assertEqual(game::cameraLookAheadQ8(state.vxQ8), 0, "spawn x has no look-ahead");
        test.assertEqual(game::cameraLookAheadQ8(state.vyQ8), 0, "spawn y has no look-ahead");
        test.assertEqual(state.cameraX, game::cameraTargetX(state), "spawn camera x snapped");
        test.assertEqual(state.cameraY, game::cameraTargetY(state), "spawn camera y snapped");
        test.assertEqual(state.cameraX, state.x - game::kCameraScreenX, "spawn x uses centred framing");
        test.assertEqual(state.cameraY, state.y - game::kCameraScreenY, "spawn y uses centred framing");

        state.cameraX = 0;
        state.cameraY = 0;
        game::snapCamera(state);
        test.assertEqual(state.cameraX, game::cameraTargetX(state), "snap lands on idle target x");
        test.assertEqual(state.cameraY, game::cameraTargetY(state), "snap lands on idle target y");
        runner.add(test);
    }

    {
        Test test("impact speed is captured before the axis is zeroed");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);
        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);
        state.xFrac = game::kSubpixelOne - 1;
        state.vxQ8 = game::topSpeedQ8For(0);

        const int16_t expected = static_cast<int16_t>(state.vxQ8 - game::dragStep(state.vxQ8));
        game::stepWorld(state, idle(), fake);

        test.assertEqual(state.x, 67, "boat held at the wall");
        test.assertEqual(state.vxQ8, 0, "velocity cleared");
        test.assertEqual(state.xFrac, 0, "position remainder cleared");
        test.assertEqual(state.impactAxis, 1, "impact axis captured");
        test.assertEqual(state.impactSpeedQ8, expected, "impact speed is pre-zero velocity");
        runner.add(test);
    }
}
