#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_JOBS

#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Contract economy rules: deterministic offers, acceptance/load handoff,
// abandonment, delivery outcomes, deadline pause, and board row selection.
// Settlement ids come from the generated metadata walk, never a literal index.
namespace jobs_test {

using Fixture = world_test::FakeWorld;

inline bool anySettlement(world::Settlement &settlement) {
    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        if (world::settlementInfo(id, settlement))
            return true;
    }
    return false;
}

inline bool otherSettlement(uint8_t origin, uint8_t skip, uint8_t &id) {
    for (uint8_t candidate = 0; candidate < world::settlementCount(); ++candidate) {
        if (candidate != origin && candidate != skip) {
            id = candidate;
            return true;
        }
    }
    return false;
}

inline bool dockAt(game::State &state, uint8_t settlementId) {
    world::Settlement settlement;
    if (!world::settlementInfo(settlementId, settlement))
        return false;
    state.docked = -1;
    state.dockCooldown = 0;
    state.x = settlement.pixel_x;
    state.y = settlement.pixel_y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    return station::dock(state);
}

// Leave the current berth and dock at another settlement without spending
// flight time, so a test can isolate the dock/delivery rules.
inline bool travelTo(game::State &state, uint8_t settlementId) {
    if (state.docked >= 0) {
        if (!station::undock(state))
            return false;
    }
    state.dockCooldown = 0;
    return dockAt(state, settlementId);
}

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

// Simulate the cargo-screen commit for rule-level tests: with the loading UI
// compiled, acceptance alone leaves the hold empty, so delivery fixtures must
// load the contract hold explicitly. The CARGO_LOAD=0 build already auto-loads
// inside accept(), so this is a no-op there.
inline void loadContractHold(game::State &state) {
#if SH_FEATURE_CARGO_LOAD
    game::setCargo(state, state.activeJob.crates);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
#else
    (void)state;
#endif
}

}   // namespace jobs_test

inline void JobsSuite(TestRunner &runner) {
    using namespace jobs_test;

    {
        Test test("offers are deterministic and well formed");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State first;
        game::reset(first);
        test.assertTrue(jobs::refreshOffers(first, origin.id), "offers generate");
        test.assertEqual(first.offerCount, game::kJobOffers, "two offers are stored");

        game::State second;
        game::reset(second);
        test.assertTrue(jobs::refreshOffers(second, origin.id), "offers regenerate from the same state");

        for (uint8_t i = 0; i < game::kJobOffers; ++i) {
            test.assertEqual(second.offers[i].from, first.offers[i].from, "origin is deterministic");
            test.assertEqual(second.offers[i].to, first.offers[i].to, "destination is deterministic");
            test.assertEqual(second.offers[i].crates, first.offers[i].crates, "crate count is deterministic");
            test.assertEqual(second.offers[i].reward, first.offers[i].reward, "reward is deterministic");
            test.assertEqual(second.offers[i].deadline, first.offers[i].deadline, "deadline is deterministic");
        }
        test.assertEqual(second.offerSerial, first.offerSerial, "offer serial advances identically");

        for (uint8_t i = 0; i < first.offerCount; ++i) {
            test.assertEqual(first.offers[i].crates, game::kJobCrates, "contracts require four units");
            test.assertTrue(first.offers[i].reward > 0, "reward is positive");
            test.assertEqual(first.offers[i].reward % jobs::kRewardRound, 0, "reward is a multiple of five");
            test.assertTrue(first.offers[i].deadline > 0, "deadline is positive");
        }
        runner.add(test);
    }

    {
        Test test("offers exclude the origin and avoid duplicate destinations");
        for (uint8_t originId = 0; originId < world::settlementCount(); ++originId) {
            world::Settlement origin;
            test.assertTrue(world::settlementInfo(originId, origin), "settlement metadata loads");

            game::State state;
            game::reset(state);
            test.assertTrue(jobs::refreshOffers(state, originId), "offers generate for every origin");

            for (uint8_t i = 0; i < state.offerCount; ++i) {
                test.assertEqual(state.offers[i].from, originId, "offer records its origin");
                test.assertTrue(state.offers[i].to != originId, "destination differs from origin");
                test.assertTrue(state.offers[i].to < world::settlementCount(), "destination is a real settlement");
            }
            if (world::settlementCount() > 2) {
                test.assertTrue(state.offers[0].to != state.offers[1].to, "the two rows use different destinations");
            }
        }
        runner.add(test);
    }

    {
        Test test("acceptance clears cargo, consumes the offer, and hands off the load");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at the offer station");
        test.assertTrue(!station::jobActive(state), "no contract at a fresh dock");

        const uint16_t creditsBefore = state.credits;
        const uint8_t offersBefore = state.offerCount;
        const game::JobOffer chosen = state.offers[0];

        state.cargo = 2;
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A on an offer accepts");

        test.assertTrue(station::jobActive(state), "contract is active");
        test.assertEqual(state.activeJob.to, chosen.to, "destination comes from the offer");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "contract requires four units");
        test.assertEqual(state.activeJob.reward, chosen.reward, "reward is fixed at acceptance");
        test.assertEqual(state.activeJob.deadline, chosen.deadline, "deadline is stored in flight ticks");
        test.assertEqual(state.activeJob.acceptedAt, state.jobClock, "contract clock starts at acceptance");
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(state.cargo, 0, "accept leaves the hold empty for the cargo screen");
        test.assertEqual(state.ballast, game::neutralFor(0), "empty hold starts at neutral trim");
#else
        test.assertEqual(state.cargo, game::kJobCrates, "fallback auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "trim is neutral for the auto-load");
#endif
        test.assertEqual(state.offerCount, offersBefore - 1, "accepted offer is consumed");
        test.assertEqual(state.credits, creditsBefore, "acceptance pays nothing");

        game::State free;
        game::reset(free);
        test.assertTrue(jobs::refreshOffers(free, origin.id), "offers available in flight");
        test.assertTrue(!jobs::accept(free, 0), "acceptance requires a docked boat");
        test.assertTrue(!jobs::accept(state, 0), "a second contract is refused while one is active");
        runner.add(test);
    }

    {
        Test test("abandon clears the contract and hold and refreshes offers");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock succeeds");
        test.assertTrue(!jobs::abandon(state), "abandon is inert without a contract");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");

        const uint16_t credits = state.credits;
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON clears the contract");
        test.assertTrue(!station::jobActive(state), "contract is gone");
        test.assertEqual(state.cargo, 0, "hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "offers are refreshed");
        test.assertEqual(state.credits, credits, "abandonment pays nothing");
        runner.add(test);
    }

    {
        Test test("on-time delivery at the destination pays credits and clears the hold");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        loadContractHold(state);
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;

        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.docked, static_cast<int8_t>(destination), "destination station is current");
        test.assertEqual(state.credits, reward, "reward is paid");
        test.assertEqual(state.deliveries, 1, "delivery count increments");
        test.assertTrue(!station::jobActive(state), "contract is cleared after delivery");
        test.assertEqual(state.cargo, 0, "delivered hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "destination offers are refreshed");
        runner.add(test);
    }

    {
        Test test("late arrival fails without paying and clears the contract");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        const uint8_t destination = state.activeJob.to;

        state.activeJob.deadline = 1;
        state.jobClock = state.activeJob.acceptedAt + 2;
        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.credits, 0, "late delivery pays nothing");
        test.assertEqual(state.deliveries, 0, "late delivery does not count");
        test.assertTrue(!station::jobActive(state), "late contract is cleared");
        test.assertEqual(state.cargo, 0, "late hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "offers are refreshed");
        runner.add(test);
    }

    {
        Test test("insufficient cargo fails an otherwise on-time delivery");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        const uint8_t destination = state.activeJob.to;
        state.cargo = static_cast<uint8_t>(state.activeJob.crates - 1);

        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.credits, 0, "a short hold pays nothing");
        test.assertEqual(state.deliveries, 0, "a short hold does not count");
        test.assertTrue(!station::jobActive(state), "failed contract is cleared");
        test.assertEqual(state.cargo, 0, "failed hold is cleared");
        runner.add(test);
    }

    {
        Test test("origin and wrong-station docks do not deliver");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        loadContractHold(state);
        const uint8_t destination = state.activeJob.to;

        test.assertTrue(travelTo(state, origin.id), "dock back at the origin");
        test.assertTrue(station::jobActive(state), "origin dock keeps the contract");
        test.assertEqual(state.cargo, game::kJobCrates, "origin dock keeps the hold");
        test.assertEqual(state.credits, 0, "origin dock pays nothing");

        uint8_t wrong = game::kJobNone;
        if (otherSettlement(origin.id, destination, wrong)) {
            test.assertTrue(travelTo(state, wrong), "dock at a third station");
            test.assertTrue(station::jobActive(state), "wrong-station dock keeps the contract");
            test.assertEqual(state.activeJob.to, destination, "contract destination is unchanged");
            test.assertEqual(state.cargo, game::kJobCrates, "wrong-station dock keeps the hold");
            test.assertEqual(state.credits, 0, "wrong-station dock pays nothing");
        }
        runner.add(test);
    }

    {
        Test test("deadlines pause while docked and advance with flight ticks");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        Fixture fake;
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");

        const uint32_t dockedClock = state.jobClock;
        for (uint8_t i = 0; i < 60; ++i)
            test.assertTrue(!station::stepFlight(state, idle(), fake), "docked steps do not re-dock");
        test.assertEqual(state.jobClock, dockedClock, "job clock is frozen while docked");
        test.assertTrue(station::jobActive(state), "contract survives a paused station visit");

        test.assertTrue(station::undock(state), "undock");
        const uint32_t flightClock = state.jobClock;
        for (uint8_t i = 0; i < 10; ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.jobClock, static_cast<uint32_t>(flightClock + 10), "job clock advances one tick per flight step");
        runner.add(test);
    }

    {
        Test test("active contract board selects ABANDON and UNDOCK only");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "offer board shows offers, SERVICE, SHOP, UNDOCK");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowOffer, "first row is an offer");
        test.assertEqual(station::rowKind(station::kServiceRow, state), station::kRowService, "SERVICE row");
        test.assertEqual(station::rowKind(station::kUndockRow, state), station::kRowUndock, "UNDOCK row");
        test.assertEqual(station::selectPrevious(station::kFirstRow, state), station::kUndockRow, "offer board wraps upward");
        test.assertEqual(station::selectNext(station::kUndockRow, state), station::kFirstRow, "offer board wraps downward");

        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "active board has summary, load, ABANDON, SHOP, UNDOCK");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row");
#if SH_FEATURE_CARGO_LOAD
        test.assertEqual(station::rowKind(station::kLoadRow, state), station::kRowLoad, "LOAD CARGO row");
        test.assertEqual(station::initialRow(state), station::kLoadRow, "selection starts on LOAD CARGO");
        test.assertEqual(station::activate(station::kLoadRow, state), station::kActionLoadCargo, "LOAD CARGO action reopens loading");
#else
        test.assertEqual(station::initialRow(state), station::kAbandonRow, "selection starts on ABANDON");
#endif
        test.assertEqual(station::rowKind(station::kAbandonRow, state), station::kRowAbandon, "ABANDON row");
        test.assertEqual(station::rowKind(station::kActiveUndockRow, state), station::kRowUndock, "UNDOCK row");
        test.assertEqual(station::selectPrevious(station::initialRow(state), state), station::kActiveUndockRow, "active board wraps upward");
        test.assertEqual(station::selectNext(station::kActiveUndockRow, state), station::initialRow(state), "active board wraps downward");
        test.assertEqual(station::activate(station::kActiveSummaryRow, state), station::kActionNone, "summary row is inert");
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "ABANDON action works");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "board returns to offers after abandonment");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_JOBS
