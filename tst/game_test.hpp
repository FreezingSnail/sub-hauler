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

inline game::Input boosted(game::Input input) {
    input.boost = true;
    return input;
}

inline void runTicks(game::State &state, game::Input input, int ticks) {
    for (int i = 0; i < ticks; i++)
        game::step(state, input);
}

inline void GameSuite(TestRunner &runner) {
    {
        Test test("fixed air tuning is a 300-unit tank and 4 air units/s blow");
        test.assertEqual(game::kAirCapacity, 300, "tank holds 300 units");
        test.assertEqual(game::kBlowCarryStep, 4, "blowing costs four air units per second");
        test.assertEqual(game::kAirDrainTicks, 150, "passive drain stays one unit per 150 ticks");
        test.assertEqual(game::kBallastCarryStep, 18, "ballast change rate stays 18 units/s");
        runner.add(test);
    }

    {
        Test test("pre-thruster handling keeps the base thrust and ballast rates");
        game::State state;
        game::reset(state);
        test.assertTrue(!game::thrusterOwned(state), "fresh run does not own the thruster");
        test.assertEqual(game::thrustAccelForState(state, false), game::kThrustAccel, "empty forward acceleration is 34 px/s^2");
        test.assertEqual(game::topSpeedQ8ForState(state, false), game::kMaxVxBaseQ8, "empty top speed is 30 px/s");
        test.assertEqual(game::ballastCarryStepForState(state, false), 18, "ballast valve is 18 units/s");

        test.assertEqual(game::thrustAccelForState(state, true), game::kThrustAccel, "A without the thruster does not raise acceleration");
        test.assertEqual(game::topSpeedQ8ForState(state, true), game::kMaxVxBaseQ8, "A without the thruster does not raise top speed");
        test.assertEqual(game::ballastCarryStepForState(state, true), 18, "A without the thruster does not raise the ballast rate");

        game::stepPhysics(state, forwardInput());
        test.assertEqual(state.vxQ8, 145, "one forward tick adds 145 Q8 units");
        state.vxQ8 = 0;
        game::stepPhysics(state, boosted(forwardInput()));
        test.assertEqual(state.vxQ8, 145, "A without the thruster keeps the base tick");
        state.vxQ8 = 30000;
        game::stepPhysics(state, idleInput());
        test.assertEqual(state.vxQ8, game::kMaxVxBaseQ8, "forward clamp stays 30 px/s");
        state.vxQ8 = 0;
        game::stepPhysics(state, reverseInput());
        test.assertEqual(state.vxQ8, -72, "one reverse tick keeps the half-power 72 Q8 step");
        state.vxQ8 = -30000;
        game::stepPhysics(state, idleInput());
        test.assertEqual(state.vxQ8, static_cast<int16_t>(-game::kMaxVxBaseQ8), "reverse clamp stays 30 px/s");

        game::reset(state);
        runTicks(state, blowInput(), 60);
        test.assertEqual(state.ballast, 52, "60 ticks still blow 18 ballast units");
        game::reset(state);
        runTicks(state, floodInput(), 60);
        test.assertEqual(state.ballast, 88, "60 ticks still flood 18 ballast units");
        runner.add(test);
    }

    {
        Test test("held A with the thruster doubles top speed and acceleration both directions");
        game::State state;
        game::reset(state);
        state.tools = exploration_data::THRUSTER;
        test.assertTrue(game::thrusterOwned(state), "thruster bit is detected");
        test.assertEqual(game::thrustAccelForState(state, false), game::kThrustAccel, "thruster alone keeps base acceleration");
        test.assertEqual(game::topSpeedQ8ForState(state, false), game::kMaxVxBaseQ8, "thruster alone keeps base top speed");
        test.assertEqual(game::ballastCarryStepForState(state, false), 18, "thruster alone keeps the base ballast rate");
        test.assertEqual(game::thrustAccelForState(state, true), game::kThrustAccel * 2, "A doubles forward acceleration");
        test.assertEqual(game::topSpeedQ8ForState(state, true), game::kMaxVxBaseQ8 * 2, "A doubles top speed");
        test.assertEqual(game::ballastCarryStepForState(state, true), 36, "A doubles the ballast valve");

        game::stepPhysics(state, boosted(forwardInput()));
        test.assertEqual(state.vxQ8, 290, "one boosted forward tick adds twice the base 145 Q8");
        state.vxQ8 = 0;
        game::stepPhysics(state, boosted(reverseInput()));
        test.assertEqual(state.vxQ8, -145, "boosted reverse tick adds the former full-power 145 Q8");

        state.vxQ8 = 30000;
        game::stepPhysics(state, boosted(idleInput()));
        test.assertEqual(state.vxQ8, game::kMaxVxBoostQ8, "boosted forward clamp is 60 px/s");
        state.vxQ8 = -30000;
        game::stepPhysics(state, boosted(idleInput()));
        test.assertEqual(state.vxQ8, static_cast<int16_t>(-game::kMaxVxBoostQ8), "boosted reverse clamp is 60 px/s");
        runner.add(test);
    }

    {
        Test test("thruster A boost applies after the cargo penalty");
        game::State empty;
        game::reset(empty);
        empty.tools = exploration_data::THRUSTER;
        game::State loaded;
        game::reset(loaded);
        loaded.tools = exploration_data::THRUSTER;
        game::setCargo(loaded, 4);

        test.assertEqual(game::thrustAccelForState(loaded, true), 44, "loaded boosted acceleration is (34-12)*2");
        test.assertEqual(game::topSpeedQ8ForState(loaded, true), 9216, "loaded boosted top speed is (30-12)*2 px/s");
        test.assertTrue(game::topSpeedQ8ForState(loaded, true) < game::topSpeedQ8ForState(empty, true), "cargo still lowers boosted top speed");
        test.assertEqual(game::ballastCarryStepForState(loaded, true), 36, "cargo does not change the boost rate");
        runner.add(test);
    }

    {
        Test test("held A with the thruster doubles the ballast valve without changing blow air rate");
        game::State state;
        game::reset(state);
        state.tools = exploration_data::THRUSTER;
        runTicks(state, boosted(blowInput()), 30);
        test.assertEqual(state.ballast, 52, "30 boosted ticks blow 18 units, matching 60 pre-thruster ticks");
        test.assertEqual(state.air, game::kAirCapacity - 2, "blow air still drains at 4 units/s");

        game::reset(state);
        state.tools = exploration_data::THRUSTER;
        runTicks(state, boosted(blowInput()), 60);
        test.assertEqual(state.ballast, 34, "a held boost empties ballast in half the time");
        test.assertEqual(state.air, game::kAirCapacity - 4, "a full boosted blow spends the same 4 air units/s, not more");

        game::reset(state);
        state.tools = exploration_data::THRUSTER;
        runTicks(state, boosted(floodInput()), 30);
        test.assertEqual(state.ballast, 88, "30 boosted flood ticks add 18 units");
        test.assertEqual(state.air, game::kAirCapacity, "flooding stays free");

        game::reset(state);
        state.tools = exploration_data::THRUSTER;
        runTicks(state, blowInput(), 60);
        test.assertEqual(state.ballast, 52, "without A the thruster valve stays at the base 18 units/s");
        runner.add(test);
    }

    {
        Test test("held A speeds travel both directions after the thruster");
        game::State plain;
        game::State boostedState;
        game::reset(plain);
        game::reset(boostedState);
        plain.tools = exploration_data::THRUSTER;
        boostedState.tools = exploration_data::THRUSTER;
        runTicks(plain, reverseInput(), 30);
        runTicks(boostedState, boosted(reverseInput()), 30);
        test.assertTrue(boostedState.x < plain.x, "A moves the boat farther left");
        test.assertTrue(boostedState.vxQ8 < plain.vxQ8, "A raises reverse speed");

        game::reset(plain);
        game::reset(boostedState);
        plain.tools = exploration_data::THRUSTER;
        boostedState.tools = exploration_data::THRUSTER;
        runTicks(plain, forwardInput(), 30);
        runTicks(boostedState, boosted(forwardInput()), 30);
        test.assertTrue(boostedState.x > plain.x, "A moves the boat farther right");
        test.assertTrue(boostedState.vxQ8 > plain.vxQ8, "A raises forward speed");
        runner.add(test);
    }

    {
        Test test("neutral trim follows cargo and reset starts neutral");
        test.assertEqual(game::neutralFor(-3), 70, "cargo below zero clamps");
        test.assertEqual(game::neutralFor(0), 70, "empty trim");
        test.assertEqual(game::neutralFor(1), 60, "one unit");
        test.assertEqual(game::neutralFor(2), 50, "two units");
        test.assertEqual(game::neutralFor(3), 40, "three units");
        test.assertEqual(game::neutralFor(4), 30, "four units");
        test.assertEqual(game::neutralFor(9), 30, "cargo above max clamps");

        game::State state;
        game::reset(state);
        test.assertEqual(state.ballast, 70, "reset ballast is neutral");
        test.assertEqual(state.air, game::kAirCapacity, "reset air is full");
        test.assertEqual(state.cargo, 0, "reset cargo is empty");
        test.assertEqual(state.vxQ8, 0, "reset horizontal velocity");
        test.assertEqual(state.vyQ8, 0, "reset vertical velocity");
        test.assertEqual(state.ticks, 0, "reset ticks");

        runTicks(state, idleInput(), game::kAirDrainTicks);
        test.assertEqual(state.ballast, 70, "neutral ballast holds depth");
        test.assertEqual(state.y, game::kSpawnY, "depth does not drift");
        test.assertEqual(state.vyQ8, 0, "vertical velocity stays zero");
        test.assertEqual(state.air, game::kAirCapacity - 1, "air drains one unit per drain period");
        test.assertEqual(state.ticks, game::kAirDrainTicks, "fixed step counts ticks");
        runner.add(test);
    }

    {
        Test test("loaded cargo sinks at old ballast");
        game::State state;
        game::reset(state);
        game::setCargo(state, 4);
        test.assertEqual(state.cargo, 4, "cargo set");
        test.assertEqual(game::neutralFor(state.cargo), 30, "loaded neutral");
        test.assertEqual(state.ballast, 70, "cargo does not move ballast");

        runTicks(state, idleInput(), 300);
        test.assertEqual(state.ballast, 70, "ballast stays put");
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
        test.assertEqual(state.ballast, 52, "18 ballast units blown in one second");
        test.assertEqual(state.air, game::kAirCapacity - 4, "blow spends four air units");
        test.assertTrue(state.vyQ8 < 0, "blowing accelerates upward");
        test.assertTrue(state.y < startY, "blowing rises");
        test.assertEqual(state.cargo, 0, "trim unchanged");
        runner.add(test);
    }

    {
        Test test("blow air cost is flat across cargo");
        game::State empty;
        game::reset(empty);
        runTicks(empty, blowInput(), 60);
        test.assertEqual(empty.air, game::kAirCapacity - 4, "empty hold costs four air per second");

        game::State loaded;
        game::reset(loaded);
        game::setCargo(loaded, 4);
        runTicks(loaded, blowInput(), 60);
        test.assertEqual(loaded.air, game::kAirCapacity - 4, "full hold still costs four air per second");

        // The climb is slower under load because the up clamp drops from 20 to
        // 12 px/s, so the same held blow lasts longer and a long climb needs
        // more re-floods/re-blows. The per-second rate itself does not change.
        test.assertTrue(game::maxVyUpQ8ForNormalized(4) < game::maxVyUpQ8ForNormalized(0),
                        "loaded ascent is slower, so a climb spends air for longer");
        runner.add(test);
    }

    {
        Test test("cargo scales the vertical clamp both directions");
        test.assertEqual(game::maxVyDownQ8ForNormalized(0), game::kMaxVyQ8, "empty down clamp is the base");
        test.assertEqual(game::maxVyDownQ8ForNormalized(4), 28 * game::kQ8One, "full down clamp is 28 px/s");
        test.assertEqual(game::maxVyUpQ8ForNormalized(0), game::kMaxVyQ8, "empty up clamp is the base");
        test.assertEqual(game::maxVyUpQ8ForNormalized(4), 12 * game::kQ8One, "full up clamp is 12 px/s");
        test.assertTrue(game::maxVyDownQ8ForNormalized(4) > game::maxVyUpQ8ForNormalized(4), "loaded sink is faster than rise");

        for (uint8_t cargo = 0; cargo <= game::kCargoMax; cargo += 4) {
            game::State state;
            game::reset(state);
            game::setCargo(state, cargo);
            state.ballast = static_cast<uint8_t>(game::neutralFor(cargo));

            state.vyQ8 = 30000;
            game::stepPhysics(state, idleInput());
            test.assertEqual(state.vyQ8, game::maxVyDownQ8ForNormalized(cargo), "down velocity clamps by cargo");

            state.vyQ8 = -30000;
            game::stepPhysics(state, idleInput());
            test.assertEqual(state.vyQ8, static_cast<int16_t>(-game::maxVyUpQ8ForNormalized(cargo)), "up velocity clamps by cargo");
        }
        runner.add(test);
    }

    {
        Test test("flood sinks for free");
        game::State state;
        game::reset(state);
        const int16_t startY = state.y;
        runTicks(state, floodInput(), 60);
        test.assertEqual(state.ballast, 88, "18 ballast units flooded in one second");
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
        test.assertEqual(state.ballast, 70, "empty tank cannot lighten ballast");
        test.assertEqual(state.air, 0, "air stays empty");
        test.assertEqual(state.y, game::kSpawnY, "no rise without air");
        test.assertEqual(state.vyQ8, 0, "no upward velocity without air");

        runTicks(state, floodInput(), 60);
        test.assertEqual(state.ballast, 88, "flood still works without air");
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
        Test test("narrow advanceAxis matches the Q12 fixed-step reference");
        // Independent 32-bit reference for the retired wide intermediate. The
        // production step is velocity*4/15 with the same truncation and floor.
        const auto reference = [](int16_t position, int16_t frac, int16_t velocity, int16_t &outPosition,
                                  int16_t &outFrac) {
            const int32_t stepQ12 = static_cast<int32_t>(velocity) * game::kSubpixelOne /
                                    (game::kQ8One * game::kStepHz);
            const int32_t accumulated = static_cast<int32_t>(frac) + stepQ12;
            int32_t whole = accumulated / game::kSubpixelOne;
            if (accumulated < 0 && accumulated % game::kSubpixelOne != 0)
                --whole;
            outPosition = static_cast<int16_t>(position + whole);
            outFrac = static_cast<int16_t>(accumulated - whole * game::kSubpixelOne);
        };

        const int16_t fracs[] = {0, 1, 2047, 2048, 4095};

        bool positionsMatch = true;
        bool fracsInRange = true;
        for (int32_t value = -32768; value <= 32767; ++value) {
            const int16_t velocity = static_cast<int16_t>(value);
            for (int16_t frac : fracs) {
                int16_t expectedPosition = 0;
                int16_t expectedFrac = 0;
                reference(100, frac, velocity, expectedPosition, expectedFrac);

                int16_t position = 100;
                int16_t actualFrac = frac;
                game::advanceAxis(position, actualFrac, velocity);

                positionsMatch = positionsMatch && position == expectedPosition;
                fracsInRange = fracsInRange && actualFrac == expectedFrac && actualFrac >= 0 &&
                               actualFrac < game::kSubpixelOne;
            }
        }
        test.assertTrue(positionsMatch, "every velocity/remainder matches the wide reference");
        test.assertTrue(fracsInRange, "truncation and negative floor keep a normalized remainder");
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
