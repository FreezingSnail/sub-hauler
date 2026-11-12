#pragma once

#include "../src/bubble_view.hpp"
#include "test.hpp"

inline void BubbleViewSuite(TestRunner &runner) {
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
}
