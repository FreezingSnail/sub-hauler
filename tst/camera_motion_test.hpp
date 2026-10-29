#pragma once

#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "test.hpp"

// Temporal camera traces, not just target formulas. These drive the same Q4
// follow state and the same fractional boat projection the device uses, then
// assert terrain origin cadence and hull-screen stability. The world is open
// water: collision itself is covered by collision_test, while this suite checks
// what the pixels do at steady speed, slow travel, reversal, and a stop.
namespace camera_motion_test {

inline game::State boatAt(int16_t x) {
    game::State state;
    game::resetWorld(state);
    state.x = x;
    state.y = 300;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    state.docked = -1;
    state.contactId = exploration::kNone;
    state.discoveryTicks = 0;
    game::snapCamera(state);
    return state;
}

inline void advance(game::State &state) {
    game::advanceAxis(state.x, state.xFrac, state.vxQ8);
    game::advanceAxis(state.y, state.yFrac, state.vyQ8);
    game::updateCamera(state);
}

inline int16_t hullScreenX(const game::State &state) {
    const cave::Camera camera = pilot_view::cameraForState(state);
    return pilot_view::screenX(state, camera);
}

inline int16_t cameraScreenOrigin(const game::State &state) {
    return game::cameraPixelX(state);
}

inline int16_t cameraScreenOriginY(const game::State &state) {
    return game::cameraPixelY(state);
}

inline int16_t hullScreenY(const game::State &state) {
    const cave::Camera camera = pilot_view::cameraForState(state);
    return pilot_view::screenY(state, camera);
}

}   // namespace camera_motion_test

inline void CameraMotionSuite(TestRunner &runner) {
    using namespace camera_motion_test;

    {
        Test test("steady top speed keeps terrain cadence and a stable hull screen");
        game::State state = boatAt(500);
        state.vxQ8 = game::kMaxVxBaseQ8;
        for (int i = 0; i < 60; ++i)
            advance(state);
        int16_t previousCamera = cameraScreenOrigin(state);
        const int16_t startX = state.x;
        const int16_t startCamera = previousCamera;
        const int16_t steadyHull = hullScreenX(state);
        test.assertEqual(steadyHull, 31, "fractional projection settles the hull at 31 px");
        for (int i = 0; i < 120; ++i) {
            advance(state);
            const int16_t camera = cameraScreenOrigin(state);
            test.assertTrue(camera == previousCamera || camera == static_cast<int16_t>(previousCamera + 1), "terrain advances 0 or 1 px, never backward");
            test.assertEqual(hullScreenX(state), steadyHull, "hull screen does not alternate between adjacent pixels");
            previousCamera = camera;
        }
        test.assertEqual(state.x - startX, 60, "top speed covers exactly 60 px in 120 ticks");
        test.assertEqual(cameraScreenOrigin(state) - startCamera, 60, "terrain origin follows the same 60 px");
        runner.add(test);
    }

    {
        Test test("slow travel keeps one-way cadence without a hull twitch");
        game::State state = boatAt(500);
        state.vxQ8 = 2560;   // 10 px/s: about one pixel every six ticks
        for (int i = 0; i < 120; ++i)
            advance(state);
        const int16_t startX = state.x;
        const int16_t startCamera = cameraScreenOrigin(state);
        int16_t previousCamera = startCamera;
        int16_t previousHull = hullScreenX(state);
        uint16_t ticksSinceStep = 0;
        bool havePreviousStep = false;
        for (int i = 0; i < 240; ++i) {
            ++ticksSinceStep;
            advance(state);
            const int16_t camera = cameraScreenOrigin(state);
            const int16_t hull = hullScreenX(state);
            test.assertTrue(camera >= previousCamera && camera <= static_cast<int16_t>(previousCamera + 1), "slow terrain cadence is regular");
            test.assertTrue(hull >= previousHull, "slow hull never twitches backward");
            if (camera != previousCamera) {
                // The trace can start partway through a pixel interval. Only
                // compare complete intervals between observed pixel steps.
                if (havePreviousStep)
                    test.assertTrue(ticksSinceStep >= 5 && ticksSinceStep <= 7, "slow pixel spacing stays bounded (5..7 ticks)");
                havePreviousStep = true;
                ticksSinceStep = 0;
            }
            previousCamera = camera;
            previousHull = hull;
        }
        test.assertTrue(state.x - startX >= 39 && state.x - startX <= 40, "slow travel covers about 40 px in 240 ticks");
        test.assertTrue(cameraScreenOrigin(state) - startCamera >= 39 && cameraScreenOrigin(state) - startCamera <= 40, "slow camera follows the same distance");
        runner.add(test);
    }

    {
        Test test("steady reverse keeps terrain cadence and no forward hull twitch");
        game::State state = boatAt(700);
        state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
        for (int i = 0; i < 60; ++i)
            advance(state);
        int16_t previousCamera = cameraScreenOrigin(state);
        const int16_t steadyHull = hullScreenX(state);
        test.assertTrue(steadyHull > 44, "reverse framing moves the hull right of centre");
        for (int i = 0; i < 120; ++i) {
            advance(state);
            const int16_t camera = cameraScreenOrigin(state);
            test.assertTrue(camera == previousCamera || camera == static_cast<int16_t>(previousCamera - 1), "reverse terrain advances 0 or 1 px left, never forward");
            test.assertEqual(hullScreenX(state), steadyHull, "reverse hull screen stays stable");
            previousCamera = camera;
        }
        runner.add(test);
    }

    {
        Test test("collision-like stop bounds the first camera correction");
        game::State state = boatAt(700);
        state.vxQ8 = game::kMaxVxBaseQ8;
        for (int i = 0; i < 60; ++i)
            advance(state);
        const int16_t cameraBefore = cameraScreenOrigin(state);
        const int16_t hullBefore = hullScreenX(state);
        state.vxQ8 = 0;   // what moveAxis leaves after a blocked hit
        advance(state);
        const int16_t cameraAfter = cameraScreenOrigin(state);
        const int16_t hullAfter = hullScreenX(state);
        test.assertTrue(cameraBefore - cameraAfter >= 0 && cameraBefore - cameraAfter <= 2, "stop correction is at most 2 px");
        test.assertTrue(hullAfter >= hullBefore && hullAfter - hullBefore <= 2, "hull screen moves toward centre without a jolt");
        for (int i = 0; i < 120; ++i) {
            const int16_t previous = cameraScreenOrigin(state);
            advance(state);
            const int16_t correction = static_cast<int16_t>(previous - cameraScreenOrigin(state));
            test.assertTrue(correction >= 0 && correction <= 2, "every settling frame moves toward centre, at most 2 px");
        }
        test.assertEqual(hullScreenX(state), 44, "stop settles at centred framing");
        runner.add(test);
    }

    {
        Test test("vertical travel keeps Y cadence and a stable hull screen");
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            game::State state = boatAt(500);
            state.vyQ8 = static_cast<int16_t>(direction * game::kMaxVyQ8);
            for (int i = 0; i < 60; ++i) {
                game::advanceAxis(state.y, state.yFrac, state.vyQ8);
                game::updateCamera(state);
            }
            const int16_t startY = state.y;
            int16_t previousCamera = cameraScreenOriginY(state);
            int16_t minimumHull = hullScreenY(state);
            int16_t maximumHull = minimumHull;
            for (int i = 0; i < 60; ++i) {
                game::advanceAxis(state.y, state.yFrac, state.vyQ8);
                game::updateCamera(state);
                const int16_t camera = cameraScreenOriginY(state);
                const int16_t hull = hullScreenY(state);
                test.assertTrue(camera == previousCamera || camera == static_cast<int16_t>(previousCamera + direction), "vertical terrain advances in the travel direction only");
                if (hull < minimumHull)
                    minimumHull = hull;
                if (hull > maximumHull)
                    maximumHull = hull;
                previousCamera = camera;
            }
            test.assertTrue(maximumHull - minimumHull <= 1, "vertical hull screen does not alternate more than one pixel");
            test.assertTrue(direction > 0 ? state.y - startY == 20 : startY - state.y == 20, "vertical travel covers 20 px in 60 ticks");
        }
        runner.add(test);
    }

    {
        Test test("fractional projection handles negative and world-edge coordinates");
        game::State state = boatAt(20);
        state.x = 10;
        state.xFrac = 2048;
        state.cameraXQ4 = game::cameraPixelToQ4(100);
        state.cameraYQ4 = 0;
        cave::Camera camera = pilot_view::cameraForState(state);
        test.assertEqual(pilot_view::screenX(state, camera), -89, "offscreen negative projection rounds the fraction");
        test.assertTrue(pilot_view::screenX(state, camera) < 0, "hull projection can be negative");

        state.x = world::PIXELS_X - 2;
        state.xFrac = 4095;
        state.cameraXQ4 = 0;
        camera = pilot_view::cameraForState(state);
        test.assertEqual(pilot_view::screenX(state, camera), static_cast<int16_t>(world::PIXELS_X - 1), "far positive projection rounds the fraction exactly");

        state.cameraXQ4 = game::kCameraMaxXQ4;
        camera = pilot_view::cameraForState(state);
        test.assertEqual(game::cameraPixelX(state), game::kCameraMaxX, "maximum Q4 rounds to the pixel clamp");
        test.assertEqual(game::cameraResidualXQ4(state), 0, "maximum Q4 has no residual");
        test.assertEqual(pilot_view::screenX(state, camera), static_cast<int16_t>((world::PIXELS_X - 1) - game::kCameraMaxX), "far camera projection subtracts the shared origin");

        state.cameraXQ4 = game::cameraPixelToQ4(2047) + 8;
        test.assertEqual(game::cameraPixelX(state), 2048, "half-Q4 rounds up");
        test.assertEqual(game::cameraResidualXQ4(state), -8, "round-up residual is -8 Q4");
        state.cameraXQ4 = game::cameraPixelToQ4(2047) + 7;
        test.assertEqual(game::cameraPixelX(state), 2047, "seven Q4 stays on the lower pixel");
        test.assertEqual(game::cameraResidualXQ4(state), 7, "round-down residual is +7 Q4");
        runner.add(test);
    }
}
