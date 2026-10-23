#pragma once

#include "../src/game.hpp"
#include "test.hpp"

inline game::Input idleInput() {
    return game::Input{false, false, false};
}

inline void GameSuite(TestRunner &runner) {
    {
        Test test("move clamps at both screen edges");
        game::State state;
        game::reset(state);
        for (int i = 0; i < 200; i++)
            game::step(state, game::Input{true, false, false});
        test.assertEqual(state.x, game::kHalfWidth, "left edge");
        for (int i = 0; i < 200; i++)
            game::step(state, game::Input{false, true, false});
        test.assertEqual(state.x, game::kScreenWidth - game::kHalfWidth, "right edge");
        runner.add(test);
    }

    {
        Test test("jump leaves the ground and lands on it");
        game::State state;
        game::reset(state);
        game::step(state, game::Input{false, false, true});
        test.assertEqual(state.y, game::kGroundY - 4, "first jump step");
        test.assertTrue(!state.grounded, "airborne after jump");
        for (int i = 0; i < 20; i++)
            game::step(state, idleInput());
        test.assertEqual(state.y, game::kGroundY, "lands on ground");
        test.assertTrue(state.grounded, "grounded after landing");
        runner.add(test);
    }

    {
        Test test("jump only fires while grounded");
        game::State state;
        game::reset(state);
        game::step(state, game::Input{false, false, true});
        const int16_t airborne = state.y;
        game::step(state, game::Input{false, false, true});
        test.assertTrue(state.y < airborne, "second jump input cannot reset velocity");
        runner.add(test);
    }

    {
        Test test("walking cycles frames and idle resets them");
        game::State state;
        game::reset(state);
        game::step(state, game::Input{false, true, false});
        test.assertEqual(state.frame, 1, "first walk frame");
        game::step(state, game::Input{false, true, false});
        test.assertEqual(state.frame, 2, "second walk frame");
        game::step(state, game::Input{false, true, false});
        test.assertEqual(state.frame, 0, "third walk frame wraps");
        game::step(state, idleInput());
        test.assertEqual(state.frame, 0, "idle frame");
        runner.add(test);
    }
}
