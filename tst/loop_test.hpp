#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_STATIONS && SH_FEATURE_JOBS

#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"
#if SH_FEATURE_CARGO_LOAD
#include "../src/cargo_handoff.hpp"
#include "../src/cargo_screen.hpp"
#include "cargo_test.hpp"
#endif

// Whole-loop integration tests: the M3 contract economy has to survive the
// station and flight transitions, not only the individual job rules. Each
// case starts from the baked spawn, uses station::stepFlight() for auto-dock,
// and leaves through the station board actions. Travel itself is compressed by
// placing the boat at a berth (the host rules are position-based); the flight
// clock still advances through real steps so deadlines are exercised.
//
// With SH_FEATURE_CARGO_LOAD the accept path leaves the hold empty and these
// tests drive the real handoff: accept -> cargo screen -> required packing ->
// A+B commit -> auto-undock -> deliver. The CARGO_LOAD=0 build keeps the M3
// auto-load path and is tested through the same loop.
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

#if SH_FEATURE_CARGO_LOAD
// Install the known HAULER target arrangement, required pieces only. The bonus
// crate stays in the tray, proving the commit accepts a required-only load.
inline bool solveRequired(cargo_screen::State &screen) {
    const uint8_t order[6] = {4, 5, 0, 1, 2, 3};
    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t index = order[i];
        const cargo::CargoPiece &piece = screen.puzzle.pieces[index];
        if (!cargo::place(screen.puzzle, index, piece.x, piece.y))
            return false;
    }
    return screen.puzzle.held < 0 && cargo::requiredComplete(screen.puzzle);
}

// Load HAULER arrangement 0 and solve its required pieces. Mirrors what the
// player does on the cargo screen; the cart-read stand-in is the host bank.
inline bool loadAndSolve(cargo_screen::State &screen, cargo_test::FakeBank &bank) {
    cargo_test::loadAllHulls(bank);
    return cargo_screen::load(screen, bank) && solveRequired(screen);
}
#endif

}   // namespace loop_test

inline void LoopSuite(TestRunner &runner) {
    using namespace loop_test;

    {
        Test test("contract loop: spawn auto-dock, accept, load, deliver, and accept again");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(state.offerCount, game::kJobOffers, "first dock posts the full offer board");

        const game::JobOffer first = state.offers[0];
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts the first offer");
        test.assertEqual(state.activeJob.to, first.to, "active contract keeps the offered destination");
        test.assertEqual(state.offerCount, game::kJobOffers - 1, "the accepted offer is consumed");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "the board switches to the active shape");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row reports the contract");
        test.assertEqual(jobs::remainingTicks(state), state.activeJob.deadline, "the contract clock is frozen on the station");
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(state.cargo, 0, "accept leaves the hold for the cargo screen");
        test.assertEqual(station::initialRow(state), station::kLoadRow, "selection starts on LOAD CARGO");
        test.assertEqual(station::activate(station::kLoadRow, state), station::kActionLoadCargo, "LOAD CARGO reopens the screen");

        cargo_screen::State cargo;
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        test.assertTrue(cargo_screen::load(cargo, bank), "HAULER arrangement loads");
        test.assertTrue(!cargo_handoff::commitReady(cargo), "empty hold is not commit-ready");
        test.assertTrue(solveRequired(cargo), "required HAULER pieces install");
        test.assertTrue(cargo_handoff::commitReady(cargo), "required packing is commit-ready");
        test.assertEqual(cargo_handoff::commit(state, cargo), cargo_handoff::kCommitted, "A+B commits LOAD & UNDOCK");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the commit auto-undocks");
        test.assertEqual(state.cargo, game::kJobCrates, "the committed hold carries four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "the committed load leaves neutral trim");
        test.assertTrue(!cargo.loaded, "the committed loading state is cleared");
        test.assertTrue(station::jobActive(state), "the contract stays active in flight");
#else
        test.assertEqual(state.cargo, game::kJobCrates, "the fallback auto-load fills the hold");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "the auto-load leaves neutral trim");
        test.assertEqual(station::initialRow(state), station::kAbandonRow, "selection starts on ABANDON");
        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "A on UNDOCK leaves the berth");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "undock returns to free flight");
#endif
        test.assertEqual(state.dockCooldown, station::kUndockCooldown, "undock arms the re-dock cooldown");
        test.assertTrue(!station::stepFlight(state, idle(), fake), "the cooldown block prevents an instant re-dock");

        // Spend a few real flight ticks before the compressed trip, so the
        // deadline is measured through the same clock the device uses.
        for (uint8_t i = 0; i < 30; ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the origin ring stays inert during cooldown");
        test.assertTrue(jobs::remainingTicks(state) > 0, "the contract is still live in flight");

        test.assertTrue(placeAtBerth(state, first.to), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.docked, static_cast<int8_t>(first.to), "the destination station is current");
        test.assertEqual(state.credits, first.reward, "on-time delivery pays the contract reward");
        test.assertEqual(state.deliveries, 1, "the delivery counter advances");
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
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(state.cargo, 0, "the second contract also waits for the cargo screen");
        test.assertTrue(loadAndSolve(cargo, bank), "second HAULER arrangement loads and solves");
        test.assertEqual(cargo_handoff::commit(state, cargo), cargo_handoff::kCommitted, "the second load commits and auto-undocks");
        test.assertEqual(state.cargo, game::kJobCrates, "the second committed hold carries four units");
#else
        test.assertEqual(state.cargo, game::kJobCrates, "the second contract auto-loads four units");
        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "the second contract can undock");
#endif
        test.assertTrue(placeAtBerth(state, second.to), "second destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the second destination auto-docks");
        test.assertEqual(state.deliveries, 2, "the loop completes a second contract");
        test.assertEqual(state.credits, static_cast<uint16_t>(first.reward + second.reward), "credits accumulate across contracts");
        test.assertEqual(state.offerCount, game::kJobOffers, "the loop leaves another offer board ready");
        runner.add(test);
    }

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
        test.assertEqual(state.jobClock, static_cast<uint32_t>(state.activeJob.acceptedAt + deadline + 1), "flight time reaches the deadline");
        test.assertEqual(jobs::remainingTicks(state), 0, "the contract has no time left");

        test.assertTrue(placeAtBerth(state, destination), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.credits, creditsBefore, "a late delivery pays nothing");
        test.assertEqual(state.deliveries, 0, "a late delivery does not count");
        test.assertTrue(!station::jobActive(state), "the late contract is cleared");
        test.assertEqual(state.cargo, 0, "the late hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "the failure still refreshes offers");
        test.assertTrue(offersAllFrom(state, destination), "recovery offers originate at the destination");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowOffer, "the board recovers to offers");

        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "a new contract is available after failure");
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(state.cargo, 0, "the recovery contract waits for the cargo screen");
#else
        test.assertEqual(state.cargo, game::kJobCrates, "the recovery contract auto-loads");
#endif
        runner.add(test);
    }

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
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(state.cargo, 0, "the replacement contract waits for the cargo screen");
#else
        test.assertEqual(state.cargo, game::kJobCrates, "the replacement contract auto-loads");
#endif
        runner.add(test);
    }

#if SH_FEATURE_CARGO_LOAD
    {
        Test test("cargo handoff: reject short load, cancel keeps job, commit auto-undocks and delivers");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts an offer");
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;
        const int8_t berth = state.docked;

        test.assertEqual(station::initialRow(state), station::kLoadRow, "active board starts on LOAD CARGO");
        test.assertEqual(station::activate(station::kLoadRow, state), station::kActionLoadCargo, "LOAD CARGO action opens the screen");

        cargo_screen::State cargo;
        cargo_test::FakeBank bank;
        cargo_test::loadAllHulls(bank);
        test.assertTrue(cargo_screen::load(cargo, bank), "HAULER arrangement loads");

        // Below the required piece count: the gesture is rejected with clear
        // feedback and neither the contract nor the berth changes.
        cargo_screen::Input gesture{};
        gesture.a = true;
        gesture.b = true;
        test.assertTrue(cargo_handoff::commitPressed(gesture), "A+B is the commit gesture");
        test.assertEqual(cargo_handoff::commit(state, cargo), cargo_handoff::kIncomplete, "short load is rejected");
        test.assertEqual(static_cast<int>(cargo.message), static_cast<int>(cargo_screen::Message::Incomplete),
                         "short load reports LOAD SHORT");
        test.assertTrue(station::jobActive(state), "rejection keeps the contract");
        test.assertEqual(state.docked, berth, "rejection keeps the boat docked");
        test.assertEqual(state.cargo, 0, "rejection does not fill the pilot hold");

        // Cancel/back leaves the cargo screen without committing. The job and
        // berth survive, and the station row reopens the same handoff.
        test.assertEqual(station::activate(station::kLoadRow, state), station::kActionLoadCargo, "LOAD CARGO reopens after cancel");
        test.assertTrue(station::jobActive(state), "cancel keeps the job open");
        test.assertEqual(state.docked, berth, "cancel returns to the station board");
        test.assertEqual(state.cargo, 0, "cancel keeps the pilot hold empty");

        // Solve only the required pieces; bonus cargo stays optional.
        test.assertTrue(solveRequired(cargo), "required HAULER pieces install");
        test.assertTrue(cargo_handoff::commitReady(cargo), "required-only load is ready");
        test.assertEqual(cargo_handoff::commit(state, cargo), cargo_handoff::kCommitted, "required-only load commits");
        test.assertEqual(state.cargo, game::kJobCrates, "commit fills the four-unit hold");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "commit sets neutral trim");
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "commit auto-undocks");
        test.assertEqual(state.dockCooldown, station::kUndockCooldown, "auto-undock arms the cooldown");
        test.assertTrue(!cargo.loaded, "commit clears the loading state");
        test.assertTrue(station::jobActive(state), "the job is active in flight");

        test.assertTrue(placeAtBerth(state, destination), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.credits, reward, "the committed load delivers for the full reward");
        test.assertEqual(state.deliveries, 1, "the committed load counts one delivery");
        test.assertTrue(!station::jobActive(state), "delivery closes the contract");
        runner.add(test);
    }
#else
    {
        Test test("fallback: accept auto-loads four units and delivers without the cargo screen");
        Fixture fake;
        game::State state;

        uint8_t station = game::kJobNone;
        test.assertTrue(dockFromSpawn(state, fake, station), "spawn auto-docks without input");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A accepts an offer");
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;

        test.assertEqual(state.cargo, game::kJobCrates, "fallback fills four units on accept");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "fallback sets neutral trim");
        test.assertEqual(station::initialRow(state), station::kAbandonRow, "fallback board has no LOAD CARGO row");

        test.assertEqual(station::activate(station::kActiveUndockRow, state), station::kActionUndocked, "the contract undocks");
        test.assertTrue(placeAtBerth(state, destination), "destination metadata loads");
        test.assertTrue(station::stepFlight(state, idle(), fake), "the destination ring auto-docks");
        test.assertEqual(state.credits, reward, "fallback load delivers for the full reward");
        test.assertEqual(state.deliveries, 1, "fallback load counts one delivery");
        runner.add(test);
    }
#endif
}

#endif   // SH_FEATURE_STATIONS && SH_FEATURE_JOBS
