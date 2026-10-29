#pragma once



#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/work_view.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Whole-loop integration tests: the contract economy has to survive the
// station and flight transitions, not only the individual job rules. Each
// case starts from the baked spawn, uses station::stepFlight() for auto-dock,
// and leaves through the station board actions. Travel itself is compressed by
// placing the boat at a berth (the host rules are position-based); the flight
// clock still advances through real steps, and no job has a deadline.
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

inline bool offersAllFrom(const game::State &state, uint8_t stationId) {
    return state.offerCount == 0 || state.docked == static_cast<int8_t>(stationId);
}

// The work board lists restoration rows first, then exactly one freight row.
// Find that row so the loop tests the same dispatch the sketch uses.
inline bool freightRow(const game::State &state, uint8_t &index, work_view::Row &row) {
    const uint8_t count = work_view::rowCount(state);
    for (uint8_t i = 0; i < count; ++i) {
        const work_view::Row candidate = work_view::rowAt(state, i);
        if (candidate.kind == work_view::kRowFreight) {
            index = i;
            row = candidate;
            return true;
        }
    }
    return false;
}

}   // namespace loop_test

inline void LoopSuite(TestRunner &runner) {
    using namespace loop_test;

    {
        Test test("contract loop: spawn auto-dock, WORK, accept, undock, deliver, accept again");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(state.offerCount, game::kJobOffers, "first dock posts the freight row");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "no-job board is WORK/SHOP/UNDOCK");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "WORK is the first station row");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionWork, "A on WORK opens the work board");

        uint8_t freightIndex = 0;
        work_view::Row freight{work_view::kRowNone, 0};
        test.assertTrue(freightRow(state, freightIndex, freight), "the WORK board carries a freight row");
        test.assertEqual(work_view::rowCount(state), freightIndex + 1, "freight sits beneath the story rows");
        test.assertTrue(work_view::accept(state, freight), "A on the freight row accepts");

        const game::JobOffer first = state.offers[0];
        test.assertEqual(state.activeJob.to, first.to, "active contract keeps the offered destination");
        test.assertEqual(state.offerCount, 0, "the accepted offer is consumed");
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

        test.assertTrue(placeAtBerth(state, first.to), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.docked, static_cast<int8_t>(first.to), "the destination station is current");
        test.assertEqual(state.credits, first.reward, "delivery pays the contract reward");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "the delivery is recorded as paid");
        test.assertTrue(!station::jobActive(state), "the contract is cleared on delivery");
        test.assertEqual(state.cargo, 0, "the delivered hold is empty");
        test.assertEqual(state.offerCount, game::kJobOffers, "the destination posts a fresh freight row");
        test.assertTrue(offersAllFrom(state, first.to), "the new offer originates at the destination");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "the board is back to WORK shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to WORK");

        uint8_t secondIndex = 0;
        work_view::Row secondRow{work_view::kRowNone, 0};
        test.assertTrue(freightRow(state, secondIndex, secondRow), "the work board posts the next freight row");
        const game::JobOffer second = state.offers[0];
        test.assertTrue(work_view::accept(state, secondRow), "the next contract can be accepted");
        test.assertEqual(state.activeJob.to, second.to, "the second contract carries its own destination");
        test.assertTrue(second.to != first.to, "the next destination differs from the berth");
        test.assertEqual(state.cargo, game::kJobCrates, "the second contract auto-loads four units");
        test.assertTrue(station::undock(state), "B undock starts the second contract");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the second contract leaves the berth");
        test.assertTrue(placeAtBerth(state, second.to), "second destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the second destination auto-docks");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "the second contract is paid");
        test.assertEqual(state.credits, static_cast<uint16_t>(first.reward + second.reward), "credits accumulate across contracts");
        test.assertEqual(state.offerCount, game::kJobOffers, "the loop leaves another freight row ready");
        runner.add(test);
    }

    {
        Test test("abandon loop: ABANDON clears the hold and restores the work row");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        uint8_t freightIndex = 0;
        work_view::Row freight{work_view::kRowNone, 0};
        test.assertTrue(freightRow(state, freightIndex, freight), "freight row is available");
        test.assertTrue(work_view::accept(state, freight), "freight accepted");
        const uint16_t creditsBefore = state.credits;

        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON ends the contract");
        test.assertTrue(!station::jobActive(state), "abandon clears the active contract");
        test.assertEqual(state.cargo, 0, "abandon clears the hold");
        test.assertEqual(state.ballast, game::neutralFor(0), "abandon restores unladen trim");
        test.assertEqual(state.credits, creditsBefore, "abandon pays nothing");
        test.assertEqual(state.offerCount, game::kJobOffers, "abandon refreshes the freight row");
        test.assertTrue(offersAllFrom(state, station), "the refreshed offer originates at this station");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "the board returns to WORK shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to WORK");

        test.assertTrue(freightRow(state, freightIndex, freight), "a replacement freight row is available");
        test.assertTrue(work_view::accept(state, freight), "the next offer is acceptable after abandon");
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
            uint8_t freightIndex = 0;
            work_view::Row freight{work_view::kRowNone, 0};
            test.assertTrue(freightRow(state, freightIndex, freight), "class loop finds the freight row");
            test.assertTrue(work_view::accept(state, freight), "class loop accepts a freight contract");
            const uint8_t destination = state.activeJob.to;
            const uint16_t reward = state.activeJob.reward;

            test.assertEqual(state.cargo, game::kJobCrates, "class auto-load carries four units");
            test.assertEqual(state.ballast, game::neutralFor(state.cargo), "class auto-load leaves neutral trim");
            test.assertEqual(state.docked, static_cast<int8_t>(station), "class acceptance stays docked");
            test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked,
                             "class contract undocks explicitly");
            test.assertTrue(placeAtBerth(state, destination), "class destination metadata loads");
            test.assertTrue(station::stepFlight(state, idle(), fake), "class destination auto-docks");
            test.assertEqual(state.credits, reward, "class delivery pays the reward");
            test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "class delivery records payment");
            test.assertTrue(!station::jobActive(state), "class delivery closes the contract");
        }
        runner.add(test);
    }
}

