#pragma once

#include "../src/game.hpp"
#include "../src/roster.hpp"
#include "test.hpp"

#include "../src/station.hpp"

// Roster rules, default reset, and the single-hull collapse. The frozen save
// field is covered by voyage_save_test.hpp; HUD geometry lives in TitleSuite.

inline void RosterSuite(TestRunner &runner) {
    {
        Test test("roster: HAULER is the only playable class and the reset default");
        test.assertEqual(static_cast<int>(roster::kDefault), static_cast<int>(roster::kHauler), "default id");
        test.assertEqual(static_cast<int>(roster::kHauler), 1, "frozen HAULER id");
        test.assertEqual(roster::kClassCount, 1, "one playable class");

        game::State state{};
        game::reset(state);
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kHauler), "reset selects HAULER");
        runner.add(test);
    }

    {
        Test test("roster: legacy save ids validate but normalize to HAULER");
        test.assertTrue(roster::isValidId(0), "retired Minnow id stays loadable");
        test.assertTrue(roster::isValidId(1), "HAULER id is valid");
        test.assertTrue(roster::isValidId(2), "retired Leviathan id stays loadable");
        test.assertTrue(!roster::isValidId(3), "unknown id above the frozen range is rejected");
        test.assertTrue(!roster::isValidId(255), "erased id is rejected");

        test.assertEqual(roster::normalize(0), roster::kHauler, "legacy small hull normalizes");
        test.assertEqual(roster::normalize(1), roster::kHauler, "HAULER normalizes");
        test.assertEqual(roster::normalize(2), roster::kHauler, "legacy large hull normalizes");
        runner.add(test);
    }

    {
        Test test("roster: HAULER persists across dock and undock");
        game::State state{};
        game::resetWorld(state);
        state.docked = 0;
        test.assertTrue(station::undock(state), "undock accepted");
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kHauler), "hull survives undock");
        state.docked = 1;
        state.dockCooldown = 0;
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kHauler), "hull survives a second dock");
        runner.add(test);
    }
}
