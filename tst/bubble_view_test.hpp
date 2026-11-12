#pragma once

#include "../src/bubble_view.hpp"
#include "test.hpp"

namespace bubble_view_test {

constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;

inline void clear(uint8_t *buffer) {
    for (uint16_t i = 0; i < kBufferBytes; ++i)
        buffer[i] = 0;
}

inline uint16_t setPixels(const uint8_t *buffer) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < kBufferBytes; ++i) {
        uint8_t value = buffer[i];
        while (value != 0) {
            count = static_cast<uint16_t>(count + (value & 1u));
            value = static_cast<uint8_t>(value >> 1);
        }
    }
    return count;
}

inline bool pixelAt(const uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= cave::VIEW_WIDTH || y >= cave::VIEW_HEIGHT)
        return false;
    return ((buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u) != 0;
}

inline game::Input inputFor(bool up, bool down, bool left, bool right, bool boost) {
    return game::Input{up, down, left, right, boost};
}

}   // namespace bubble_view_test

inline void BubbleViewSuite(TestRunner &runner) {
    using namespace bubble_view_test;

    Test test("movement bubbles animate behind the boat and stop when idle or docked");
    game::State state;
    game::resetWorld(state);
    state.x = 1000;
    state.y = 500;
    state.docked = -1;
    const cave::Camera camera = cave::cameraAtPixel(936, 468);
    uint8_t a[1024] = {}, b[1024] = {};
    bubble_view::draw(a, state, camera);
    bool any = false;
    for (uint16_t i = 0; i < 1024; ++i) any |= a[i] != 0;
    test.assertTrue(!any, "idle boat has no wake");
    for (uint8_t direction = 0; direction < 4; ++direction) {
        for (uint16_t i = 0; i < 1024; ++i) a[i] = b[i] = 0;
        state.vxQ8 = direction == 0 ? 3 * game::kQ8One : direction == 1 ? -3 * game::kQ8One : 0;
        state.vyQ8 = direction == 2 ? 3 * game::kQ8One : direction == 3 ? -3 * game::kQ8One : 0;
        state.ticks = 0;
        bubble_view::draw(a, state, camera);
        state.ticks = 4;
        bubble_view::draw(b, state, camera);
        if (direction >= 2) {
            for (uint16_t i = 0; i < 1024; ++i)
                test.assertTrue(a[i] == 0 && b[i] == 0, "vertical travel has no wake");
            continue;
        }
        bool changed = false;
        any = false;
        for (int16_t y = 0; y < 64; ++y) {
            for (int16_t x = 0; x < 128; ++x) {
                const bool pixel = (a[(y >> 3) * 128 + x] & (1u << (y & 7))) != 0;
                if (pixel) {
                    any = true;
                    test.assertTrue(direction == 0 ? x < 64 : x > 64, "behind velocity");
                }
            }
        }
        for (uint16_t i = 0; i < 1024; ++i) changed |= a[i] != b[i];
        test.assertTrue(any && changed, "wake visible and animated");
    }
    for (uint16_t i = 0; i < 1024; ++i) a[i] = 0;
    state.docked = 0;
    bubble_view::draw(a, state, camera);
    any = false;
    for (uint16_t i = 0; i < 1024; ++i) any |= a[i] != 0;
    test.assertTrue(!any, "docked boat has no wake");
    runner.add(test);

    {
        Test cues("flight cues follow the live thruster and valve rules");
        game::State s;
        game::resetWorld(s);
        s.docked = -1;
        s.air = 300;
        s.ballast = 50;
        s.tools = 0;

        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, false, false, true, true)), static_cast<uint8_t>(bubble_view::kCuePropeller), "A without thruster is not boosted");
        s.tools = exploration_data::THRUSTER;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, false, false, true, true)), static_cast<uint8_t>(bubble_view::kCuePropeller | bubble_view::kCueBoost), "owned thruster boosts horizontal thrust");
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, false, false, true, false)), static_cast<uint8_t>(bubble_view::kCuePropeller), "plain thrust is not boosted");
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(true, false, false, false, true)), static_cast<uint8_t>(bubble_view::kCueBlow), "blow cue does not imply boost");

        cues.assertEqual(bubble_view::cuesFor(s, inputFor(true, false, false, false, false)), static_cast<uint8_t>(bubble_view::kCueBlow), "blow valve works with air and ballast");
        s.air = 0;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(true, false, false, false, false)), static_cast<uint8_t>(0), "empty tank cannot blow");
        s.air = 300;
        s.ballast = 0;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(true, false, false, false, false)), static_cast<uint8_t>(0), "empty ballast cannot blow");
        s.ballast = 50;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(true, true, false, false, false)), static_cast<uint8_t>(0), "both valves cancel");

        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, true, false, false, false)), static_cast<uint8_t>(bubble_view::kCueFlood), "flood valve works below full");
        s.ballast = game::kBallastMax;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, true, false, false, false)), static_cast<uint8_t>(0), "full ballast cannot flood");

        s.ballast = 50;
        s.docked = 0;
        cues.assertEqual(bubble_view::cuesFor(s, inputFor(false, false, false, true, true)), static_cast<uint8_t>(0), "docked boat shows no flight cues");
        runner.add(cues);
    }

    {
        Test test("propeller flicker and boosted wash follow the cue bits");
        game::State s;
        game::resetWorld(s);
        s.x = 1000;
        s.y = 500;
        s.docked = -1;
        s.facingLeft = false;
        s.vxQ8 = 3 * game::kQ8One;
        s.ticks = 0;

        uint8_t plain[1024] = {}, prop[1024] = {}, boost[1024] = {}, flicker[1024] = {};
        bubble_view::draw(plain, s, cave::cameraAtPixel(936, 468));
        bubble_view::draw(prop, s, cave::cameraAtPixel(936, 468), bubble_view::kCuePropeller);
        bubble_view::draw(boost, s, cave::cameraAtPixel(936, 468), bubble_view::kCueBoost);
        test.assertTrue(setPixels(prop) > setPixels(plain), "propeller adds a stern pixel");
        test.assertTrue(setPixels(boost) > setPixels(plain), "boosted wash is denser than the plain wake");

        s.ticks = 2;
        bubble_view::draw(flicker, s, cave::cameraAtPixel(936, 468), bubble_view::kCuePropeller);
        bool changed = false;
        for (uint16_t i = 0; i < 1024; ++i)
            changed = changed || prop[i] != flicker[i];
        test.assertTrue(changed, "propeller blade flickers with the flight clock");

        clear(boost);
        bubble_view::draw(boost, s, cave::cameraAtPixel(936, 468), static_cast<uint8_t>(bubble_view::kCuePropeller | bubble_view::kCueBoost));
        test.assertTrue(setPixels(boost) > setPixels(flicker), "boost adds the paired blade");
        runner.add(test);
    }

    {
        Test test("blow and flood cues stay outside the hull and animate");
        game::State s;
        game::resetWorld(s);
        s.x = 1000;
        s.y = 500;
        s.docked = -1;
        s.vxQ8 = 0;
        s.vyQ8 = 0;
        const cave::Camera camera = cave::cameraAtPixel(936, 468);
        uint8_t a[1024] = {}, b[1024] = {};
        clear(a);
        bubble_view::draw(a, s, camera);
        test.assertEqual(setPixels(a), static_cast<uint16_t>(0), "no valve cue means no ballast bubbles");

        bubble_view::draw(a, s, camera, bubble_view::kCueBlow);
        bool allAbove = true;
        bool any = false;
        for (int16_t y = 0; y < 64; ++y) {
            for (int16_t x = 0; x < 128; ++x) {
                if (pixelAt(a, x, y)) {
                    any = true;
                    allAbove = allAbove && y < 32 - game::kHullHalfH;
                }
            }
        }
        test.assertTrue(any && allAbove, "blow bubbles rise above the hull");

        s.ticks = 2;
        bubble_view::draw(b, s, camera, bubble_view::kCueBlow);
        bool changed = false;
        for (uint16_t i = 0; i < 1024; ++i)
            changed = changed || a[i] != b[i];
        test.assertTrue(changed, "blow bubbles animate");

        clear(a);
        bubble_view::draw(a, s, camera, bubble_view::kCueFlood);
        bool allBelow = true;
        any = false;
        for (int16_t y = 0; y < 64; ++y) {
            for (int16_t x = 0; x < 128; ++x) {
                if (pixelAt(a, x, y)) {
                    any = true;
                    allBelow = allBelow && y > 32 + game::kHullHalfH;
                }
            }
        }
        test.assertTrue(any && allBelow, "flood intake sinks below the hull");

        s.docked = 0;
        clear(a);
        bubble_view::draw(a, s, camera, static_cast<uint8_t>(bubble_view::kCueBlow | bubble_view::kCueFlood | bubble_view::kCuePropeller));
        test.assertEqual(setPixels(a), static_cast<uint16_t>(0), "docked boat shows no cues");
        runner.add(test);
    }
}
