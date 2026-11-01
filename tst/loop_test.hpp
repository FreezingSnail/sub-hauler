#pragma once

#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/work_view.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Whole-loop integration tests: campaign delivery has to survive the station
// and flight transitions, not only the individual job rules. Each case starts
// from the baked spawn, uses station::stepFlight() for auto-dock, and leaves
// through the station board actions. Travel itself is compressed by placing the
// boat at a berth (the host rules are position-based); the flight clock still
// advances through real steps, and no job has a deadline.
namespace loop_test {

using Fixture = world_test::FakeWorld;

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

// Spawn is inside a generated settlement ring. One idle flight step must dock
// there, matching the first frame of a device run.
inline bool dockFromSpawn(game::State &state, Fixture &fake, uint8_t &stationId) {
    game::resetWorld(state);
    if (!station::stepFlight(state, idle(), fake))
        return false;
    if (state.docked < 0)
        return false;
    stationId = static_cast<uint8_t>(state.docked);
    return true;
}

// Park the boat exactly on a settlement berth with no drift. The caller then
// steps once so the normal auto-dock path owns the transition.
inline bool placeAtBerth(game::State &state, uint8_t settlementId) {
    world::Settlement settlement;
    if (!world::settlementInfo(settlementId, settlement))
        return false;

    state.x = settlement.pixel_x;
    state.y = settlement.pixel_y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    state.dockCooldown = 0;
    game::snapCamera(state);
    return true;
}

}   // namespace loop_test

inline void LoopSuite(TestRunner &runner) {
    using namespace loop_test;

    {
        Test test("story loop: spawn auto-dock, WORK, accept, undock, deliver");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(station, campaign::kTidewall, "spawn is Tidewall");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "no-job board is WORK/UNDOCK");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "WORK is the first station row");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionWork, "A on WORK opens the work board");

        test.assertEqual(work_view::rowCount(state), 1, "FIRST is the only WORK row");
        const work_view::Row first = work_view::rowAt(state, 0);
        test.assertEqual(first.mission, campaign::kFirst, "the story row is FIRST");
        test.assertTrue(work_view::accept(state, first), "A on the FIRST row accepts");

        test.assertEqual(state.activeJob.to, campaign::kBenthos, "active contract keeps the mission destination");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "the board switches to the active shape");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row reports the contract");
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "the auto-load leaves neutral trim");
        test.assertEqual(state.docked, static_cast<int8_t>(station), "acceptance keeps the boat docked");
        test.assertEqual(station::initialRow(state), station::kActiveUndockRow, "selection starts on safe UNDOCK");
        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "A on UNDOCK leaves the berth");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "undock returns to free flight");
        test.assertEqual(state.dockCooldown, station::kUndockCooldown, "undock arms the re-dock cooldown");
        test.assertTrue(!station::stepFlight(state, idle(), fake), "the cooldown block prevents an instant re-dock");

        for (uint16_t i = 0; i < 300; ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the origin ring stays inert during cooldown");

        test.assertTrue(placeAtBerth(state, campaign::kBenthos), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kBenthos), "the destination station is current");
        test.assertEqual(state.delivered, 1, "delivery completes once");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryDelivered, "the delivery is recorded as complete");
        test.assertTrue(!station::jobActive(state), "the contract is cleared on delivery");
        test.assertEqual(state.cargo, 0, "the delivered hold is empty");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "the board is back to WORK shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to WORK");
        runner.add(test);
    }

    {
        Test test("abandon loop: ABANDON clears the hold and restores the work row");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        const work_view::Row first = work_view::rowAt(state, 0);
        test.assertTrue(work_view::accept(state, first), "FIRST accepted");
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads");

        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON ends the contract");
        test.assertTrue(!station::jobActive(state), "abandon clears the active contract");
        test.assertEqual(state.cargo, 0, "abandon clears the hold");
        test.assertEqual(state.ballast, game::neutralFor(0), "abandon restores unladen trim");
        test.assertEqual(state.delivered, 0, "abandon counts no delivery");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "the board returns to WORK shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to WORK");

        test.assertEqual(work_view::rowCount(state), 1, "FIRST is available again");
        test.assertTrue(work_view::accept(state, work_view::rowAt(state, 0)), "the mission is acceptable after abandon");
        test.assertEqual(state.cargo, game::kJobCrates, "the replacement contract auto-loads");
        runner.add(test);
    }

    {
        Test test("all playable hulls complete dock, accept, auto-load, undock, and deliver");
        const uint8_t hullCount = 3;
        for (uint8_t h = 0; h < hullCount; ++h) {
            Fixture fake;
            game::State state;
            uint8_t station = game::kJobNone;
            test.assertTrue(dockFromSpawn(state, fake, station), "class loop docks at spawn");
            state.hullClass = h;
            const work_view::Row first = work_view::rowAt(state, 0);
            test.assertTrue(work_view::accept(state, first), "class loop accepts the story contract");
            const uint8_t destination = state.activeJob.to;

            test.assertEqual(state.cargo, game::kJobCrates, "class auto-load carries four units");
            test.assertEqual(state.ballast, game::neutralFor(state.cargo), "class auto-load leaves neutral trim");
            test.assertEqual(state.docked, static_cast<int8_t>(station), "class acceptance stays docked");
            test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked,
                             "class contract undocks explicitly");
            test.assertTrue(placeAtBerth(state, destination), "class destination metadata loads");
            test.assertTrue(station::stepFlight(state, idle(), fake), "class destination auto-docks");
            test.assertEqual(state.delivered, 1, "class delivery completes");
            test.assertEqual(state.lastDelivery, jobs::kDeliveryDelivered, "class delivery records completion");
            test.assertTrue(!station::jobActive(state), "class delivery closes the contract");
        }
        runner.add(test);
    }
}
