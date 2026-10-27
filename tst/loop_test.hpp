#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS

#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Whole-loop integration tests: the contract economy has to survive the
// station and flight transitions, not only the individual job rules. Each
// case starts from the baked spawn, uses station::stepFlight() for auto-dock,
// and leaves through the station board actions. Travel itself is compressed by
// placing the boat at a berth (the host rules are position-based); the flight
// clock still advances through real steps so deadlines are exercised.
//
// Acceptance auto-loads the contract's four cargo units and leaves the boat
// docked; travel starts only with an explicit UNDOCK row action or B. The
// packing screen and its CARGO_LOAD flag were removed in sub-hauler-gl1.1.
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

// A point one ring-radius-plus-margin away from every settlement. Used only to
// spend deadline ticks away from a berth.
inline void parkClearOfStations(game::State &state) {
    state.x = world::SPAWN_SUB_X;
    state.y = static_cast<int16_t>(world::SPAWN_SUB_Y - 100);
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    state.dockCooldown = 0;
    game::snapCamera(state);
}

inline bool offersAllFrom(const game::State &state, uint8_t stationId) {
    for (uint8_t i = 0; i < state.offerCount; ++i) {
        if (state.offers[i].from != stationId)
            return false;
    }
    return true;
}

}   // namespace loop_test

inline void LoopSuite(TestRunner &runner) {
    using namespace loop_test;

    {
        Test test("contract loop: spawn auto-dock, accept, auto-load, undock, deliver, and accept again");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(state.offerCount, game::kJobOffers, "first dock posts the full offer board");
#if SH_FEATURE_SHOP
        test.assertEqual(station::rowCount(state), 5, "shop board has five selectable rows");
        test.assertEqual(station::rowKind(station::kShopRow, state), station::kRowShop, "SHOP sits before UNDOCK");
        test.assertEqual(station::activate(station::kShopRow, state), station::kActionShop, "SHOP row opens the shop screen");
#endif

        const game::JobOffer first = state.offers[0];
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts the first offer");
        test.assertEqual(state.activeJob.to, first.to, "active contract keeps the offered destination");
        test.assertEqual(state.offerCount, game::kJobOffers - 1, "the accepted offer is consumed");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "the board switches to the active shape");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row reports the contract");
#if SH_FEATURE_SHOP
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(station::kActiveRowCount, 5, "campaign active shop board has five rows");
#else
        test.assertEqual(station::kActiveRowCount, 4, "active shop board has four rows");
#endif
        test.assertEqual(station::rowKind(station::kActiveShopRow, state), station::kRowShop, "active board keeps a SHOP row");
        test.assertEqual(station::activate(station::kActiveShopRow, state), station::kActionShop, "active SHOP row opens the shop");
        test.assertEqual(station::shopRow(state), station::kActiveShopRow, "active board returns to its SHOP row");
#endif
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(jobs::remainingTicks(state), 0, "ordinary work has no clock on the station");
        test.assertEqual(state.activeJob.deadline, 0, "ordinary work stores an untimed deadline");
#else
        test.assertEqual(jobs::remainingTicks(state), state.activeJob.deadline, "the contract clock is frozen on the station");
#endif
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "the auto-load leaves neutral trim");
        test.assertEqual(state.docked, static_cast<int8_t>(station), "acceptance keeps the boat docked");
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(station::initialRow(state), station::kActiveUndockRow, "selection starts on safe UNDOCK");
#else
        test.assertEqual(station::initialRow(state), station::kAbandonRow, "selection starts on ABANDON");
#endif
        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "A on UNDOCK leaves the berth");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "undock returns to free flight");
        test.assertEqual(state.dockCooldown, station::kUndockCooldown, "undock arms the re-dock cooldown");
        test.assertTrue(!station::stepFlight(state, idle(), fake), "the cooldown block prevents an instant re-dock");

        // Spend a few real flight ticks before the compressed trip, so the
        // deadline is measured through the same clock the device uses.
        for (uint8_t i = 0; i < 30; ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the origin ring stays inert during cooldown");
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(jobs::remainingTicks(state), 0, "ordinary work never runs out of time");
        test.assertEqual(state.activeJob.elapsed, 0, "ordinary work does not age in flight");
#else
        test.assertTrue(jobs::remainingTicks(state) > 0, "the contract is still live in flight");
#endif

        test.assertTrue(placeAtBerth(state, first.to), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.docked, static_cast<int8_t>(first.to), "the destination station is current");
        test.assertEqual(state.credits, first.reward, "on-time delivery pays the contract reward");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "the delivery is recorded as paid");
        test.assertTrue(!station::jobActive(state), "the contract is cleared on delivery");
        test.assertEqual(state.cargo, 0, "the delivered hold is empty");
        test.assertEqual(state.offerCount, game::kJobOffers, "the destination posts a fresh offer board");
        test.assertTrue(offersAllFrom(state, first.to), "new offers originate at the destination");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowOffer, "the board is back to offer shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to the first offer");

        const game::JobOffer second = state.offers[0];
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "the next contract can be accepted");
        test.assertEqual(state.activeJob.to, second.to, "the second contract carries its own destination");
        test.assertTrue(second.to != first.to, "the next destination differs from the berth");
        test.assertEqual(state.cargo, game::kJobCrates, "the second contract auto-loads four units");
        test.assertTrue(station::undock(state), "B undock starts the second contract");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the second contract leaves the berth");
        test.assertTrue(placeAtBerth(state, second.to), "second destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the second destination auto-docks");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "the second contract is paid");
        test.assertEqual(state.credits, static_cast<uint16_t>(first.reward + second.reward), "credits accumulate across contracts");
        test.assertEqual(state.offerCount, game::kJobOffers, "the loop leaves another offer board ready");
        runner.add(test);
    }

#if !SH_FEATURE_CAMPAIGN
    {
        Test test("late contract loop: failed delivery clears the job and restores offers");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts an offer");
        const uint8_t destination = state.activeJob.to;
        const uint16_t deadline = state.activeJob.deadline;
        const uint16_t creditsBefore = state.credits;

        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "the contract undocks");
        parkClearOfStations(state);
        uint8_t nearby = 0;
        test.assertTrue(!station::nearestDock(state, nearby), "the flight point is outside every ring");

        // Spend the entire deadline plus one tick in real flight steps. This is
        // intentionally the slow path: the same counter is charged on device.
        for (uint32_t i = 0; i <= static_cast<uint32_t>(deadline); ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "open water does not auto-dock");
        test.assertEqual(state.activeJob.elapsed, static_cast<uint16_t>(deadline + 1), "flight age passes the deadline");
        test.assertEqual(jobs::remainingTicks(state), 0, "the contract has no time left");

        test.assertTrue(placeAtBerth(state, destination), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.credits, creditsBefore, "a late delivery pays nothing");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "the late delivery is recorded as failed");
        test.assertTrue(!station::jobActive(state), "the late contract is cleared");
        test.assertEqual(state.cargo, 0, "the late hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "the failure still refreshes offers");
        test.assertTrue(offersAllFrom(state, destination), "recovery offers originate at the destination");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowOffer, "the board recovers to offers");

        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "a new contract is available after failure");
        test.assertEqual(state.cargo, game::kJobCrates, "the recovery contract auto-loads");
        runner.add(test);
    }
#endif   // !SH_FEATURE_CAMPAIGN

    {
        Test test("abandon loop: ABANDON clears the hold and restores offers");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts an offer");
        const uint16_t creditsBefore = state.credits;

        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON ends the contract");
        test.assertTrue(!station::jobActive(state), "abandon clears the active contract");
        test.assertEqual(state.cargo, 0, "abandon clears the hold");
        test.assertEqual(state.ballast, game::neutralFor(0), "abandon restores unladen trim");
        test.assertEqual(state.credits, creditsBefore, "abandon pays nothing");
        test.assertEqual(state.offerCount, game::kJobOffers, "abandon refreshes the offer board");
        test.assertTrue(offersAllFrom(state, station), "refreshed offers originate at this station");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowOffer, "the board returns to offer shape");
        test.assertEqual(station::initialRow(state), station::kFirstRow, "selection returns to the first offer");

        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "the next offer is acceptable after abandon");
        test.assertEqual(state.cargo, game::kJobCrates, "the replacement contract auto-loads");
        runner.add(test);
    }

    {
        Test test("all playable hulls complete dock, accept, auto-load, undock, and deliver");
#if SH_FEATURE_MULTI_HULL
        const uint8_t hullCount = 3;
#else
        const uint8_t hullCount = 1;
#endif
        for (uint8_t h = 0; h < hullCount; ++h) {
            Fixture fake;
            game::State state;
            uint8_t station = game::kJobNone;
            test.assertTrue(dockFromSpawn(state, fake, station), "class loop docks at spawn");
#if SH_FEATURE_MULTI_HULL
            state.hullClass = h;
#endif
            test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted,
                             "class loop accepts a contract");
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

#endif   // SH_FEATURE_STATIONS && SH_FEATURE_JOBS
