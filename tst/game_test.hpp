#pragma once

#include "../src/game.hpp"
#include "test.hpp"

inline game::Input idleInput() {
    return game::Input{false, false, false, false};
}

inline game::Input blowInput() {
    return game::Input{true, false, false, false};
}

inline game::Input floodInput() {
    return game::Input{false, true, false, false};
}

inline game::Input forwardInput() {
    return game::Input{false, false, false, true};
}

inline game::Input reverseInput() {
    return game::Input{false, false, true, false};
}

inline void runTicks(game::State &state, game::Input input, int ticks) {
    for (int i = 0; i < ticks; i++)
        game::step(state, input);
}

inline void GameSuite(TestRunner &runner) {
    {
        Test test("neutral trim follows cargo and reset starts neutral");
        test.assertEqual(game::neutralFor(-3), 50, "cargo below zero clamps");
        test.assertEqual(game::neutralFor(0), 50, "empty trim");
        test.assertEqual(game::neutralFor(1), 45, "one unit");
        test.assertEqual(game::neutralFor(2), 40, "two units");
        test.assertEqual(game::neutralFor(3), 35, "three units");
        test.assertEqual(game::neutralFor(4), 30, "four units");
        test.assertEqual(game::neutralFor(9), 30, "cargo above max clamps");

        game::State state;
        game::reset(state);
        test.assertEqual(state.ballast, 50, "reset ballast is neutral");
        test.assertEqual(state.air, game::kAirCapacity, "reset air is full");
        test.assertEqual(state.cargo, 0, "reset cargo is empty");
        test.assertEqual(state.vxQ8, 0, "reset horizontal velocity");
        test.assertEqual(state.vyQ8, 0, "reset vertical velocity");
        test.assertEqual(state.ticks, 0, "reset ticks");

        runTicks(state, idleInput(), 120);
        test.assertEqual(state.ballast, 50, "neutral ballast holds depth");
        test.assertEqual(state.y, game::kSpawnY, "depth does not drift");
        test.assertEqual(state.vyQ8, 0, "vertical velocity stays zero");
        test.assertEqual(state.air, 179, "air drains one unit per 100 ticks");
        test.assertEqual(state.ticks, 120, "fixed step counts ticks");
        runner.add(test);
    }

    {
        Test test("loaded cargo sinks at old ballast");
        game::State state;
        game::reset(state);
        game::setCargo(state, 4);
        test.assertEqual(state.cargo, 4, "cargo set");
        test.assertEqual(game::neutralFor(state.cargo), 30, "loaded neutral");
        test.assertEqual(state.ballast, 50, "cargo does not move ballast");

        runTicks(state, idleInput(), 300);
        test.assertEqual(state.ballast, 50, "ballast stays put");
        test.assertTrue(state.vyQ8 > 0, "loaded boat accelerates down");
        test.assertTrue(state.y > game::kSpawnY + 8, "loaded boat visibly sinks");
        runner.add(test);
    }

    {
        Test test("blow lowers ballast and spends air");
        game::State state;
        game::reset(state);
        const int16_t startY = state.y;
        runTicks(state, blowInput(), 60);
        test.assertEqual(state.ballast, 32, "18 ballast units blown in one second");
        test.assertEqual(state.air, 177, "blow spends three air units");
        test.assertTrue(state.vyQ8 < 0, "blowing accelerates upward");
        test.assertTrue(state.y < startY, "blowing rises");
        test.assertEqual(state.cargo, 0, "trim unchanged");
        runner.add(test);
    }

    {
        Test test("flood sinks for free");
        game::State state;
        game::reset(state);
        const int16_t startY = state.y;
        runTicks(state, floodInput(), 60);
        test.assertEqual(state.ballast, 68, "18 ballast units flooded in one second");
        test.assertEqual(state.air, game::kAirCapacity, "flood costs no air");
        test.assertTrue(state.vyQ8 > 0, "flooding accelerates downward");
        test.assertTrue(state.y > startY, "flooding sinks");
        runner.add(test);
    }

    {
        Test test("no air means no blow");
        game::State state;
        game::reset(state);
        state.air = 0;
        runTicks(state, blowInput(), 120);
        test.assertEqual(state.ballast, 50, "empty tank cannot lighten ballast");
        test.assertEqual(state.air, 0, "air stays empty");
        test.assertEqual(state.y, game::kSpawnY, "no rise without air");
        test.assertEqual(state.vyQ8, 0, "no upward velocity without air");

        runTicks(state, floodInput(), 60);
        test.assertEqual(state.ballast, 68, "flood still works without air");
        runner.add(test);
    }

    {
        Test test("release holds ballast");
        game::State state;
        game::reset(state);
        runTicks(state, blowInput(), 30);
        const uint8_t ballast = state.ballast;
        const int16_t air = state.air;
        runTicks(state, idleInput(), 60);
        test.assertEqual(state.ballast, ballast, "release holds ballast");
        test.assertEqual(state.air, air, "release only drains slowly");
        runner.add(test);
    }

    {
        Test test("thrust and reverse with cargo speed effect");
        game::State state;
        game::reset(state);
        runTicks(state, forwardInput(), 120);
        const int16_t forwardVx = state.vxQ8;
        test.assertTrue(forwardVx > 0, "forward thrust moves right");
        test.assertTrue(state.x > game::kSpawnX, "forward thrust advances x");
        test.assertTrue(!state.facingLeft, "forward faces right");
        test.assertTrue(forwardVx <= game::topSpeedQ8For(0), "forward speed is clamped");

        game::reset(state);
        runTicks(state, reverseInput(), 120);
        const int16_t reverseVx = state.vxQ8;
        test.assertTrue(reverseVx < 0, "reverse thrust moves left");
        test.assertTrue(state.x < game::kSpawnX, "reverse thrust retreats x");
        test.assertTrue(state.facingLeft, "reverse faces left");
        test.assertTrue(-reverseVx < forwardVx, "reverse is weaker than forward");

        game::reset(state);
        runTicks(state, forwardInput(), 600);
        const int16_t emptyVx = state.vxQ8;
        game::reset(state);
        game::setCargo(state, 4);
        runTicks(state, forwardInput(), 600);
        const int16_t loadedVx = state.vxQ8;
        test.assertTrue(loadedVx > 0, "loaded forward thrust still moves");
        test.assertTrue(loadedVx < emptyVx, "cargo lowers top speed");
        test.assertTrue(loadedVx <= game::topSpeedQ8For(4), "loaded speed is clamped");

        runTicks(state, idleInput(), 600);
        test.assertEqual(state.vxQ8, 0, "no auto-forward: idle drag stops the boat");
        runner.add(test);
    }

    {
        Test test("ballast, air, cargo and velocity clamps");
        game::State state;
        game::reset(state);
        runTicks(state, blowInput(), 400);
        test.assertEqual(state.ballast, 0, "ballast floors at zero");
        test.assertTrue(state.air > 0, "air remains after emptying ballast");
        test.assertTrue(state.vyQ8 <= -1 || state.y <= game::kSpawnY, "blown boat rises");

        runTicks(state, floodInput(), 1000);
        test.assertEqual(state.ballast, 100, "ballast ceilings at 100");
        test.assertTrue(state.air > 0, "flood itself spends no air");
        test.assertTrue(state.vyQ8 >= 0, "full ballast cannot rise");

        state.air = 0;
        runTicks(state, idleInput(), 100);
        test.assertEqual(state.air, 0, "air floors at zero");

        game::setCargo(state, 9);
        test.assertEqual(state.cargo, 4, "cargo clamps high");
        game::setCargo(state, -2);
        test.assertEqual(state.cargo, 0, "cargo clamps low");

        game::reset(state);
        runTicks(state, forwardInput(), 1000);
        test.assertTrue(state.vxQ8 <= game::topSpeedQ8For(0), "velocity clamps at top speed");
        runTicks(state, reverseInput(), 1000);
        test.assertTrue(state.vxQ8 >= -game::topSpeedQ8For(0), "reverse clamps at top speed");
        runner.add(test);
    }

    {
        Test test("fixed-step determinism");
        game::State a;
        game::State b;
        game::reset(a);
        game::reset(b);
        for (int i = 0; i < 600; i++) {
            game::Input input = idleInput();
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
            game::step(a, input);
            game::step(b, input);
        }
        test.assertEqual(a.x, b.x, "x deterministic");
        test.assertEqual(a.y, b.y, "y deterministic");
        test.assertEqual(a.xFrac, b.xFrac, "x remainder deterministic");
        test.assertEqual(a.yFrac, b.yFrac, "y remainder deterministic");
        test.assertEqual(a.vxQ8, b.vxQ8, "vx deterministic");
        test.assertEqual(a.vyQ8, b.vyQ8, "vy deterministic");
        test.assertEqual(a.ballast, b.ballast, "ballast deterministic");
        test.assertEqual(a.air, b.air, "air deterministic");
        test.assertEqual(a.cargo, b.cargo, "cargo deterministic");
        test.assertEqual(a.facingLeft, b.facingLeft, "facing deterministic");
        test.assertEqual(a.ballastCarry, b.ballastCarry, "ballast carry deterministic");
        test.assertEqual(a.airBlowCarry, b.airBlowCarry, "blow carry deterministic");
        test.assertEqual(a.airDrainTicks, b.airDrainTicks, "drain ticks deterministic");
        test.assertEqual(a.ticks, 600, "one step is exactly one tick");
        test.assertEqual(b.ticks, 600, "second state ticks identically");
        runner.add(test);
    }
}
