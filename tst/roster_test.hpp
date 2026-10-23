#pragma once

#include "../src/game.hpp"
#include "../src/hud.hpp"
#include "../src/roster.hpp"
#include "test.hpp"

#if SH_FEATURE_STATIONS
#include "../src/station.hpp"
#endif

// Roster rules, default reset, and the single-hull collapse. The state field,
// title layout, and class art are covered by roster_view_test.hpp and
// pilot_view_test.hpp.
#if !SH_FEATURE_MULTI_HULL
static_assert(roster::kClassCount == 1, "single-hull build exposes one class");
static_assert(roster::classAt(0) == roster::kHauler, "single-hull index 0 is HAULER");
static_assert(roster::classAt(2) == roster::kHauler, "single-hull out-of-range clamps to HAULER");
static_assert(roster::next(roster::kMinnow) == roster::kHauler, "single-hull next stays HAULER");
static_assert(roster::previous(roster::kLeviathan) == roster::kHauler, "single-hull previous stays HAULER");
static_assert(!roster::valid(roster::kMinnow), "MINNOW is absent when MULTI_HULL=0");
static_assert(!roster::valid(roster::kLeviathan), "LEVIATHAN is absent when MULTI_HULL=0");
static_assert(roster::indexOf(roster::kLeviathan) == 0, "single-hull index is always 0");
static_assert(roster::nameOf(roster::kMinnow)[0] == 'H', "single-hull host names fold to HAULER");
#endif

inline void RosterSuite(TestRunner &runner) {
    {
        Test test("roster: HAULER is the default class");
        test.assertEqual(static_cast<int>(roster::kDefault), static_cast<int>(roster::kHauler), "default id");

        game::State state{};
        game::reset(state);
#if SH_FEATURE_MULTI_HULL
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kHauler), "reset selects HAULER");
#else
        test.assertEqual(roster::kClassCount, 1, "single-hull roster count");
#endif
        runner.add(test);
    }

    {
        Test test("roster: HAULER record and code");
        const roster::ClassSpec spec = roster::spec(roster::kHauler);
        test.assertEqual(spec.length, 94, "HAULER reference length");
        test.assertEqual(spec.height, 22, "HAULER reference height");
        test.assertEqual(spec.pilotW, 26, "HAULER pilot width");
        test.assertEqual(spec.pilotH, 8, "HAULER pilot height");
        test.assertEqual(roster::nameLength(roster::kHauler), 6, "HAULER name columns");
        test.assertEqual(static_cast<int>(roster::code(roster::kHauler)), static_cast<int>('M'), "HAULER code");
        test.assertTrue(roster::valid(roster::kHauler), "HAULER is valid");
        runner.add(test);
    }

#if SH_FEATURE_MULTI_HULL
    {
        Test test("roster: MINNOW/HAULER/LEVIATHAN order and records");
        test.assertEqual(roster::kClassCount, 3, "three classes");
        test.assertEqual(static_cast<int>(roster::kMinnow), 0, "MINNOW id");
        test.assertEqual(static_cast<int>(roster::kHauler), 1, "HAULER id");
        test.assertEqual(static_cast<int>(roster::kLeviathan), 2, "LEVIATHAN id");

        test.assertEqual(roster::classAt(0), roster::kMinnow, "index 0");
        test.assertEqual(roster::classAt(1), roster::kHauler, "index 1");
        test.assertEqual(roster::classAt(2), roster::kLeviathan, "index 2");
        test.assertEqual(roster::indexOf(roster::kMinnow), 0, "MINNOW index");
        test.assertEqual(roster::indexOf(roster::kHauler), 1, "HAULER index");
        test.assertEqual(roster::indexOf(roster::kLeviathan), 2, "LEVIATHAN index");

        const roster::ClassSpec minnow = roster::spec(roster::kMinnow);
        test.assertEqual(minnow.length, 64, "MINNOW reference length");
        test.assertEqual(minnow.height, 16, "MINNOW reference height");
        test.assertEqual(minnow.pilotW, 24, "MINNOW pilot width");
        test.assertEqual(minnow.pilotH, 7, "MINNOW pilot height");

        const roster::ClassSpec leviathan = roster::spec(roster::kLeviathan);
        test.assertEqual(leviathan.length, 118, "LEVIATHAN reference length");
        test.assertEqual(leviathan.height, 30, "LEVIATHAN reference height");
        test.assertEqual(leviathan.pilotW, 26, "LEVIATHAN pilot width");
        test.assertEqual(leviathan.pilotH, 8, "LEVIATHAN pilot height");

        test.assertEqual(roster::nameLength(roster::kMinnow), 7, "MINNOW name columns");
        test.assertEqual(roster::nameLength(roster::kLeviathan), 9, "LEVIATHAN name columns");
        test.assertEqual(static_cast<int>(roster::code(roster::kMinnow)), static_cast<int>('S'), "MINNOW code");
        test.assertEqual(static_cast<int>(roster::code(roster::kLeviathan)), static_cast<int>('L'), "LEVIATHAN code");
        test.assertTrue(roster::nameOf(roster::kMinnow)[0] == 'M', "host MINNOW name");
        test.assertTrue(roster::nameOf(roster::kLeviathan)[0] == 'L', "host LEVIATHAN name");
        runner.add(test);
    }

    {
        Test test("roster: next/previous wrap and invalid selection sanitizes");
        roster::HullClass hull = roster::kHauler;
        hull = roster::next(hull);
        test.assertEqual(hull, roster::kLeviathan, "HAULER -> LEVIATHAN");
        hull = roster::next(hull);
        test.assertEqual(hull, roster::kMinnow, "LEVIATHAN -> MINNOW");
        hull = roster::next(hull);
        test.assertEqual(hull, roster::kHauler, "MINNOW wraps to HAULER");

        hull = roster::previous(roster::kHauler);
        test.assertEqual(hull, roster::kMinnow, "HAULER -> MINNOW backwards");
        hull = roster::previous(hull);
        test.assertEqual(hull, roster::kLeviathan, "MINNOW wraps to LEVIATHAN backwards");
        hull = roster::previous(hull);
        test.assertEqual(hull, roster::kHauler, "LEVIATHAN -> HAULER backwards");

        test.assertEqual(roster::sanitize(static_cast<roster::HullClass>(7)), roster::kHauler, "invalid id falls back");
        test.assertTrue(!roster::valid(static_cast<roster::HullClass>(7)), "invalid id is rejected");
        runner.add(test);
    }

#if SH_FEATURE_STATIONS
    {
        Test test("roster: selection persists across dock and undock");
        game::State state{};
        game::resetWorld(state);
        state.hullClass = static_cast<uint8_t>(roster::kLeviathan);
        state.docked = 0;
        test.assertTrue(station::undock(state), "undock accepted");
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kLeviathan), "hull survives undock");
        state.docked = 1;
        state.dockCooldown = 0;
        test.assertEqual(state.hullClass, static_cast<uint8_t>(roster::kLeviathan), "hull survives a second dock");
        runner.add(test);
    }
#endif
#else
    {
        Test test("roster: single-hull build collapses to HAULER");
        test.assertEqual(roster::classAt(0), roster::kHauler, "index 0");
        test.assertEqual(roster::classAt(9), roster::kHauler, "out-of-range index");
        test.assertEqual(roster::next(roster::kHauler), roster::kHauler, "next stays HAULER");
        test.assertEqual(roster::previous(roster::kHauler), roster::kHauler, "previous stays HAULER");
        test.assertEqual(roster::sanitize(roster::kMinnow), roster::kHauler, "disabled class sanitizes");
        test.assertTrue(!roster::valid(roster::kLeviathan), "LEVIATHAN invalid");
        test.assertTrue(roster::nameOf(roster::kLeviathan)[0] == 'H', "host name folds to HAULER");
        runner.add(test);
    }
#endif
}
