#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_STATIONS

#include "../src/station.hpp"
#include "../src/station_view.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Station/dock rules and board layout. The world is the synthetic open-water
// fixture, so these tests never read the generated cart. Settlement ids are
// symbolic: every loop walks world::settlementCount() and asks the generated
// metadata for the position.
namespace station_test {

using Fixture = world_test::FakeWorld;

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

// The single-settlement cases do not care which berth they use; take the first
// record the generated table offers instead of writing a literal index.
inline bool anySettlement(world::Settlement &settlement) {
    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        if (world::settlementInfo(id, settlement))
            return true;
    }
    return false;
}

}   // namespace station_test

inline void StationSuite(TestRunner &runner) {
    using namespace station_test;

    {
        Test test("nearest dock uses the gantry capture radius");
        for (uint8_t id = 0; id < world::settlementCount(); ++id) {
            world::Settlement settlement;
            test.assertTrue(world::settlementInfo(id, settlement), "settlement metadata loads");

            game::State state;
            game::reset(state);

            place(state, settlement.pixel_x, settlement.pixel_y);
            uint8_t found = 0;
            test.assertTrue(station::nearestDock(state, found), "dock centre is inside the ring");
            test.assertEqual(found, id, "centre resolves to its own settlement");

            place(state, static_cast<int16_t>(settlement.pixel_x + station::kDockRadius), settlement.pixel_y);
            test.assertTrue(station::nearestDock(state, found), "exact radius is inside");
            test.assertEqual(found, id, "radius edge resolves to its own settlement");

            place(state, static_cast<int16_t>(settlement.pixel_x + station::kDockRadius + 1), settlement.pixel_y);
            test.assertTrue(!station::nearestDock(state, found), "one pixel past the radius is outside");
        }
        runner.add(test);
    }

    {
        Test test("docking accepts the speed threshold and rejects a fast boat");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        game::State slow;
        game::reset(slow);
        place(slow, settlement.pixel_x, settlement.pixel_y);
        slow.vxQ8 = station::kDockSpeedQ8;
        test.assertTrue(station::dock(slow), "speed exactly at the threshold docks");
        test.assertEqual(slow.docked, static_cast<int8_t>(settlement.id), "slow boat tracks the settlement id");

        game::State fast;
        game::reset(fast);
        place(fast, settlement.pixel_x, settlement.pixel_y);
        fast.vxQ8 = static_cast<int16_t>(station::kDockSpeedQ8 + 1);
        test.assertTrue(!station::dock(fast), "one Q8 unit over the threshold is too fast");
        test.assertEqual(fast.docked, static_cast<int8_t>(-1), "rejected docking leaves free flight");

        game::State diagonal;
        game::reset(diagonal);
        place(diagonal, settlement.pixel_x, settlement.pixel_y);
        diagonal.vxQ8 = 12 * game::kQ8One;
        diagonal.vyQ8 = 12 * game::kQ8One;
        test.assertTrue(!station::dock(diagonal), "combined speed uses both velocity axes");
        runner.add(test);
    }

    {
        Test test("dock refills air, repairs hull, and neutralizes trim");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        const int16_t approachX = static_cast<int16_t>(settlement.pixel_x + 5);
        const int16_t approachY = static_cast<int16_t>(settlement.pixel_y - 3);

        game::State state;
        game::reset(state);
        place(state, approachX, approachY);
        state.cargo = 2;
        state.air = 3;
        state.ballast = 100;
        state.xFrac = 1234;
        state.yFrac = 2345;
        state.ballastCarry = 59;
        state.airBlowCarry = 59;
        state.airDrainTicks = 99;
        const uint16_t approachCameraX = state.cameraXQ4;
        const uint16_t approachCameraY = state.cameraYQ4;
#if SH_FEATURE_HULL_DAMAGE
        state.hull = 1;
#endif

        test.assertTrue(station::dock(state), "dock succeeds");

        test.assertEqual(state.air, game::airCapacity(state), "air is refilled to capacity");
#if SH_FEATURE_HULL_DAMAGE
        test.assertEqual(state.hull, game::hullCapacity(state), "hull is repaired to maximum");
#endif
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "ballast resets to neutral for the load");
        test.assertEqual(state.ballastCarry, 0, "ballast carry is cleared");
        test.assertEqual(state.airBlowCarry, 0, "air-blow carry is cleared");
        test.assertEqual(state.airDrainTicks, 0, "air drain counter is cleared");
        test.assertEqual(state.x, approachX, "docking preserves the approach x");
        test.assertEqual(state.y, approachY, "docking preserves the approach y");
        test.assertEqual(state.xFrac, 1234, "docking preserves the x remainder");
        test.assertEqual(state.yFrac, 2345, "docking preserves the y remainder");
        test.assertEqual(state.cameraXQ4, approachCameraX, "docking leaves the camera x in place");
        test.assertEqual(state.cameraYQ4, approachCameraY, "docking leaves the camera y in place");
        test.assertEqual(state.vxQ8, 0, "vx is zero");
        test.assertEqual(state.vyQ8, 0, "vy is zero");
        runner.add(test);
    }

    {
        Test test("dock and undock round trip works at every settlement");
        for (uint8_t id = 0; id < world::settlementCount(); ++id) {
            world::Settlement settlement;
            test.assertTrue(world::settlementInfo(id, settlement), "settlement metadata loads");

            const int16_t approachX = static_cast<int16_t>(settlement.pixel_x + 4);
            const int16_t approachY = static_cast<int16_t>(settlement.pixel_y + 2);

            game::State state;
            game::reset(state);
            place(state, approachX, approachY);

            test.assertTrue(station::dock(state), "dock succeeds");
            test.assertEqual(state.docked, static_cast<int8_t>(id), "docked id matches the settlement");

            state.vxQ8 = 123;
            state.vyQ8 = -456;
            test.assertTrue(station::undock(state), "undock succeeds");
            test.assertEqual(state.docked, static_cast<int8_t>(-1), "undock clears the docked id");
            test.assertEqual(state.vxQ8, 0, "undock zeroes vx");
            test.assertEqual(state.vyQ8, 0, "undock zeroes vy");
            test.assertEqual(state.xFrac, 0, "undock clears x remainder");
            test.assertEqual(state.yFrac, 0, "undock clears y remainder");
            test.assertEqual(state.x, approachX, "undock keeps the approach x");
            test.assertEqual(state.y, approachY, "undock keeps the approach y");
            test.assertTrue(!station::undock(state), "second undock is a no-op");

            // An idle frame immediately after undock must not snap straight
            // back to the station board.
            Fixture fake;
            test.assertTrue(!station::stepFlight(state, idle(), fake),
                            "undock cooldown prevents immediate re-dock");
            test.assertEqual(state.docked, static_cast<int8_t>(-1),
                             "cooldown leaves free flight active");
        }
        runner.add(test);
    }

    {
        Test test("undock ring latch waits for a clear exit before auto-dock");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        Fixture fake;
        game::State state;
        game::reset(state);
        place(state, settlement.pixel_x, settlement.pixel_y);
        test.assertTrue(station::dock(state), "dock succeeds");
        test.assertTrue(station::undock(state), "undock succeeds");

        bool redocked = false;
        for (uint16_t i = 0; i < 240; ++i)
            redocked = station::stepFlight(state, idle(), fake) || redocked;
        test.assertTrue(!redocked, "idling in the berth ring never re-docks");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "boat stays in free flight");
        test.assertEqual(state.dockCooldown, 1, "ring latch holds the cooldown at one");

        place(state, static_cast<int16_t>(settlement.pixel_x + station::kDockRadius + 1), settlement.pixel_y);
        test.assertTrue(!station::stepFlight(state, idle(), fake), "leaving the ring does not dock");
        test.assertEqual(state.dockCooldown, 0, "clearing the ring re-arms auto-dock");

        place(state, settlement.pixel_x, settlement.pixel_y);
        test.assertTrue(station::stepFlight(state, idle(), fake), "re-entering the ring docks again");
        test.assertEqual(state.docked, settlement.id, "dock returns to the berth");
        runner.add(test);
    }

    {
        Test test("auto-dock fires after a slow world step");
        for (uint8_t id = 0; id < world::settlementCount(); ++id) {
            world::Settlement settlement;
            test.assertTrue(world::settlementInfo(id, settlement), "settlement metadata loads");

            Fixture fake;
            game::State state;
            game::reset(state);
            place(state, static_cast<int16_t>(settlement.pixel_x - 8), settlement.pixel_y);
            state.vxQ8 = 10 * game::kQ8One;

            test.assertTrue(station::stepFlight(state, idle(), fake), "slow step inside the ring docks");
            test.assertEqual(state.docked, static_cast<int8_t>(id), "auto-dock selects the settlement");
        }
        runner.add(test);
    }

    {
        Test test("fast pass does not dock; releasing thrust does");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        Fixture fake;
        game::State state;
        game::reset(state);
        place(state, static_cast<int16_t>(settlement.pixel_x - station::kDockRadius - 6), settlement.pixel_y);
        state.vxQ8 = game::topSpeedQ8For(0);

        bool dockedWhileFast = false;
        for (int i = 0; i < 20; ++i)
            dockedWhileFast = station::stepFlight(state, right(), fake) || dockedWhileFast;
        test.assertTrue(!dockedWhileFast, "a fast boat crosses the ring without docking");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "still in free flight after the crossing");

        bool dockedAfterRelease = false;
        for (int i = 0; i < 60 && !dockedAfterRelease; ++i)
            dockedAfterRelease = station::stepFlight(state, idle(), fake);
        test.assertTrue(dockedAfterRelease, "drag brings the boat under the threshold and auto-dock fires");
        test.assertEqual(state.docked, static_cast<int8_t>(settlement.id), "released boat docks at the ring");
        runner.add(test);
    }

    {
        Test test("docked flight pauses travel, air, and ballast");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        Fixture fake;
        game::State state;
        game::reset(state);
        place(state, settlement.pixel_x, settlement.pixel_y);
        test.assertTrue(station::dock(state), "dock succeeds");

        const uint16_t ticks = state.ticks;
        const int16_t air = state.air;
        const uint8_t ballast = state.ballast;
        const int16_t x = state.x;

        for (int i = 0; i < 60; ++i)
            test.assertTrue(!station::stepFlight(state, right(), fake), "docked steps never re-dock");

        test.assertEqual(state.ticks, ticks, "travel ticks are frozen");
        test.assertEqual(state.air, air, "air is frozen");
        test.assertEqual(state.ballast, ballast, "ballast is frozen");
        test.assertEqual(state.x, x, "position is frozen");
        runner.add(test);
    }

    {
        Test test("station rows wrap and actions service or undock");
        world::Settlement settlement;
        test.assertTrue(anySettlement(settlement), "settlement metadata loads");

        test.assertEqual(station::kStationRowCount,
#if SH_FEATURE_JOBS && SH_FEATURE_SHOP
                         3,
#elif SH_FEATURE_JOBS
                         2,
#elif SH_FEATURE_SHOP
                         3,
#else
                         2,
#endif
                         "WORK/SERVICE, SHOP and UNDOCK rows are present");
        game::State board;
        game::reset(board);
#if SH_FEATURE_JOBS
        test.assertEqual(station::rowKind(station::kFirstRow, board), station::kRowWork, "first row is WORK");
#else
        test.assertEqual(station::rowKind(station::kFirstRow, board), station::kRowService, "first row is SERVICE");
#endif
#if SH_FEATURE_SHOP
        test.assertEqual(station::rowKind(station::kShopRow, board), station::kRowShop, "SHOP follows WORK/SERVICE");
        test.assertEqual(station::shopRow(board), station::kShopRow, "no-job board returns to the SHOP row");
#endif
        test.assertEqual(station::rowKind(station::kUndockRow, board), station::kRowUndock, "last row is UNDOCK");
        test.assertEqual(station::selectNext(station::kUndockRow, board), station::kFirstRow, "down wraps to the first row");
        test.assertEqual(station::selectPrevious(station::kFirstRow, board), station::kUndockRow, "up wraps to the last row");

        game::State state;
        game::reset(state);
        place(state, settlement.pixel_x, settlement.pixel_y);
        test.assertTrue(station::dock(state), "dock succeeds");
        state.air = 1;
#if SH_FEATURE_HULL_DAMAGE
        state.hull = 0;
#endif

#if SH_FEATURE_JOBS
        test.assertEqual(station::activate(station::kWorkRow, state), station::kActionWork, "A on WORK opens the work board");
        test.assertEqual(state.docked, static_cast<int8_t>(settlement.id), "opening WORK keeps the boat docked");
#else
        test.assertEqual(station::activate(station::kServiceRow, state), station::kActionServiced, "A on SERVICE services");
        test.assertEqual(state.air, game::airCapacity(state), "service refills air");
#if SH_FEATURE_HULL_DAMAGE
        test.assertEqual(state.hull, game::hullCapacity(state), "service repairs hull");
#endif
        test.assertEqual(state.docked, static_cast<int8_t>(settlement.id), "service keeps the boat docked");
#endif

#if SH_FEATURE_SHOP
        test.assertEqual(station::activate(station::kShopRow, state), station::kActionShop, "A on SHOP opens the shop");
        test.assertEqual(state.docked, static_cast<int8_t>(settlement.id), "opening the shop keeps the boat docked");
#endif

        test.assertEqual(station::activate(station::kUndockRow, state), station::kActionUndocked, "A on UNDOCK undocks");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "undock action clears the docked id");
        test.assertEqual(station::activate(station::kUndockRow, state), station::kActionNone, "undocking twice is inert");
        runner.add(test);
    }

    {
        Test test("station board rows fit above the hint at 128x64");
        const uint8_t lastRow = static_cast<uint8_t>(station::kStationRowCount - 1);
        test.assertEqual(station_view::rowY(station::kFirstRow), station_view::kRowY0, "first row uses the layout origin");
        test.assertTrue(station_view::rowY(station::kFirstRow) - station_view::kRowInsetY >= station_view::kStatusY + 7, "first invert bar clears status");
        test.assertTrue(station_view::rowY(lastRow) + station_view::kRowInvertH <= station_view::kHintY, "last row clears the hint");
#if SH_FEATURE_JOBS
        test.assertTrue(station_view::rowY(static_cast<uint8_t>(station::kActiveRowCount - 1)) + station_view::kRowInvertH <= station_view::kHintY,
                        "active board's last row clears the hint");
#endif
        test.assertTrue(station_view::kHintY + 7 <= 64, "hint fits inside 64 rows");
        test.assertTrue(station_view::kRowX + 6 * 6 < 128, "row labels fit inside 128 columns");
#if SH_FEATURE_SHOP
        test.assertEqual(station_view::kRowInvertH, 7, "shop board uses a 7 px invert");
        test.assertEqual(station_view::kRowStep, 7, "shop board rows are 7 px apart");
        test.assertEqual(station_view::kHintY, 57, "shop board hint starts at 57");
        test.assertEqual(station::kStationRowCount, 3, "no-job shop board has WORK, SHOP and UNDOCK");
#endif
        runner.add(test);
    }
}

#endif   // SH_FEATURE_STATIONS
