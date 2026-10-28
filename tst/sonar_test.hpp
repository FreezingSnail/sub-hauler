#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_SONAR

#include "../src/chart.hpp"
#include "../src/game.hpp"
#include "../src/pilot.hpp"
#include "../src/sonar.hpp"
#include "cave_test.hpp"
#include "test.hpp"

// Sonar rule tests: level cost/radius, atomic rejection, reveal reach, docked
// rejection, and the one-tick-per-flight-step cooldown. The cooldown, air and
// chart are the observable evidence; no diagnostic ping counter is stored.
namespace sonar_test {

inline uint16_t exploredCells(const chart::State &state) {
    uint16_t count = 0;
    for (int16_t cellY = 0; cellY < static_cast<int16_t>(world::CHART_TILES_Y); ++cellY) {
        for (int16_t cellX = 0; cellX < static_cast<int16_t>(world::CHART_TILES_X); ++cellX) {
            if (chart::isExplored(state, cellX, cellY))
                ++count;
        }
    }
    return count;
}

}   // namespace sonar_test

inline void SonarSuite(TestRunner &runner) {
    {
        Test test("sonar arithmetic preserves every byte input");
        game::State state;
        game::reset(state);
        bool exact = true;
        for (uint16_t v = 0; v < 256; ++v) {
            const uint8_t level = static_cast<uint8_t>(v);
            const uint8_t cost = v >= 6 ? 1 : static_cast<uint8_t>(6 - v);
            const uint8_t radius = v + 3 > 12 ? 12 : static_cast<uint8_t>(v + 3);
            state.sonarCooldown = level;
            const bool visible = v != 0 && v <= 120 && 120 - v < 24;
            exact = exact && sonar::costForLevel(level) == cost && sonar::radiusForLevel(level) == radius;
            exact = exact && sonar::ringVisible(state) == visible;
        }
        test.assertTrue(exact, "all levels and cooldowns preserve cost, radius and clipping window");
        runner.add(test);
    }

    static_assert(sonar::kBaseRevealRadius > chart::kMoveRevealRadius, "sonar reveals more than travel");
    static_assert(sonar::kCooldownTicks > sonar::kRingTicks, "ring window ends before the cooldown");

    {
        Test test("sonar cost falls and radius grows with the upgrade level");
        test.assertEqual(sonar::costForLevel(0), 6, "level 0 base cost");
        test.assertEqual(sonar::costForLevel(1), 5, "level 1 discount");
        test.assertEqual(sonar::costForLevel(2), 4, "level 2 discount");
        test.assertEqual(sonar::costForLevel(5), 1, "level 5 reaches the floor");
        test.assertEqual(sonar::costForLevel(6), 1, "the floor holds past the base cost");
        test.assertEqual(sonar::costForLevel(255), 1, "the floor holds at the largest level");

        test.assertEqual(sonar::radiusForLevel(0), 3, "level 0 base radius");
        test.assertEqual(sonar::radiusForLevel(1), 4, "level 1 adds a cell");
        test.assertEqual(sonar::radiusForLevel(2), 5, "level 2 adds two cells");
        test.assertEqual(sonar::radiusForLevel(9), 12, "radius reaches the cap");
        test.assertEqual(sonar::radiusForLevel(200), 12, "radius caps for large levels");
        runner.add(test);
    }

    {
        Test test("a ping spends air, stores cooldown, and reveals");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);

        state.x = 1000;   // chart cell 25,20
        state.y = 800;

        test.assertEqual(sonar::fire(state, fog, 0), sonar::kFired, "ping is accepted");
        test.assertEqual(state.air, game::kAirCapacity - 6, "level 0 spends six air");
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks, "cooldown starts full");

        test.assertTrue(chart::isExplored(fog, 25, 20), "centre cell is revealed");
        test.assertTrue(chart::isExplored(fog, 22, 17), "radius-3 corner is revealed");
        test.assertTrue(chart::isExplored(fog, 28, 23), "far radius-3 corner is revealed");
        test.assertTrue(!chart::isExplored(fog, 21, 20), "radius 3 stops west");
        test.assertTrue(!chart::isExplored(fog, 29, 20), "radius 3 stops east");
        test.assertEqual(sonar_test::exploredCells(fog), 49, "square radius reveals 7x7 cells");

        game::State upgraded;
        game::reset(upgraded);
        chart::State upgradedFog;
        chart::reset(upgradedFog);
        upgraded.x = 1000;
        upgraded.y = 800;
        test.assertEqual(sonar::fire(upgraded, upgradedFog, 2), sonar::kFired, "upgraded ping is accepted");
        test.assertEqual(upgraded.air, game::kAirCapacity - 4, "level 2 spends four air");
        test.assertTrue(chart::isExplored(upgradedFog, 20, 15), "radius-5 corner is revealed");
        test.assertTrue(!chart::isExplored(upgradedFog, 19, 15), "radius 5 stops one cell short");
        runner.add(test);
    }

    {
        Test test("insufficient air rejects the ping without side effects");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);

        state.x = 1000;
        state.y = 800;
        state.air = 5;   // level 0 costs 6

        test.assertEqual(sonar::fire(state, fog, 0), sonar::kNoAir, "low air is rejected");
        test.assertEqual(state.air, 5, "rejected ping spends nothing");
        test.assertEqual(state.sonarCooldown, 0, "rejected ping starts no cooldown");
        test.assertEqual(sonar_test::exploredCells(fog), 0, "rejected ping reveals nothing");
        runner.add(test);
    }

    {
        Test test("cooldown rejects a second ping until the flight tick expires");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);

        state.x = 1000;
        state.y = 800;
        test.assertEqual(sonar::fire(state, fog, 0), sonar::kFired, "first ping is accepted");
        const int16_t airAfterFirst = state.air;

        chart::reset(fog);
        state.x = 1600;   // chart cell 40,25
        state.y = 1000;
        test.assertEqual(sonar::fire(state, fog, 0), sonar::kCooldown, "cooldown rejects the second ping");
        test.assertEqual(state.air, airAfterFirst, "rejected ping spends no air");
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks, "rejected ping does not recharge the cooldown");
        test.assertEqual(sonar_test::exploredCells(fog), 0, "rejected ping reveals nothing");

        state.sonarCooldown = 1;
        const game::Input idle{};
        game::step(state, idle);
        test.assertEqual(state.sonarCooldown, 0, "one flight tick clears the last cooldown tick");

        test.assertEqual(sonar::fire(state, fog, 0), sonar::kFired, "ping is accepted again after cooldown");
        test.assertEqual(state.air, airAfterFirst - 6, "second accepted ping spends air");
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks, "second accepted ping restarts the cooldown");
        test.assertTrue(chart::isExplored(fog, 40, 25), "second ping reveals at its own position");
        runner.add(test);
    }

#if SH_FEATURE_STATIONS
    {
        Test test("a docked boat cannot ping");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);

        state.docked = 0;
        state.sonarCooldown = 0;

        test.assertEqual(sonar::fire(state, fog, 0), sonar::kDocked, "docked state is rejected");
        test.assertEqual(state.air, game::kAirCapacity, "docked rejection spends no air");
        test.assertEqual(state.sonarCooldown, 0, "docked rejection starts no cooldown");
        test.assertEqual(sonar_test::exploredCells(fog), 0, "docked rejection reveals nothing");
        runner.add(test);
    }
#endif

    {
        Test test("cooldown ticks exactly once in step and once in stepWorld");
        game::State state;
        game::reset(state);
        chart::State fog;
        chart::reset(fog);
        test.assertEqual(sonar::fire(state, fog, 0), sonar::kFired, "ping starts the cooldown");

        const game::Input idle{};
        game::stepPhysics(state, idle);
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks, "physics-only half does not tick the cooldown");

        game::step(state, idle);
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks - 1, "step decrements exactly once");

        cave_test::FakeTiles tiles;
        game::stepWorld(state, idle, tiles);
        test.assertEqual(state.sonarCooldown, sonar::kCooldownTicks - 2, "stepWorld decrements exactly once");

        for (uint16_t i = 0; i < 200; ++i)
            game::step(state, idle);
        test.assertEqual(state.sonarCooldown, 0, "cooldown floors at zero without wrapping");
        runner.add(test);
    }

    {
        Test test("reset clears sonar state");
        game::State state;
        game::reset(state);
        state.sonarCooldown = 50;

        game::reset(state);
        test.assertEqual(state.sonarCooldown, 0, "reset clears the cooldown");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_SONAR
