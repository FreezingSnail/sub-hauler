#pragma once

#include "../src/features.hpp"

#if SH_FEATURE_JOBS

#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#if SH_FEATURE_SHOP
#include "../src/shop.hpp"
#endif
#include "test.hpp"
#include "world_test.hpp"

// Contract economy rules: deterministic freight offer, acceptance/load
// handoff, abandonment, untimed delivery outcomes, and station row selection.
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

// Test-side reference for the retired offer arithmetic. Production reads the
// baked route table; this recomputes the same floor/round rules from the
// canonical settlement pixels so the table cannot drift silently.
inline uint16_t isqrt32(uint32_t value) {
    uint32_t result = 0;
    uint32_t bit = 1UL << 30;
    while (bit > value)
        bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint16_t>(result);
}

inline jobs::RouteEconomy referenceRoute(uint8_t fromId, uint8_t toId) {
    constexpr uint16_t kRewardBase = 20;
    constexpr uint16_t kRewardPerCrate = 10;
    constexpr uint16_t kRewardRound = 5;
    constexpr uint16_t kRewardDistanceDivisor = 2;

    world::Settlement from;
    world::Settlement to;
    if (!world::settlementInfo(fromId, from) || !world::settlementInfo(toId, to))
        return jobs::RouteEconomy{0};

    const int32_t dx = static_cast<int32_t>(to.pixel_x) - from.pixel_x;
    const int32_t dy = static_cast<int32_t>(to.pixel_y) - from.pixel_y;
    const uint16_t distancePixels = isqrt32(static_cast<uint32_t>(dx * dx + dy * dy));
    const uint16_t distanceTiles = static_cast<uint16_t>(distancePixels / 8);

    const uint16_t base = static_cast<uint16_t>(kRewardBase + game::kJobCrates * kRewardPerCrate + distanceTiles / kRewardDistanceDivisor);
    const uint16_t reward = static_cast<uint16_t>(((base + kRewardRound / 2) / kRewardRound) * kRewardRound);
    return jobs::RouteEconomy{static_cast<uint8_t>(reward)};
}

// Simulated flight fixtures never need a separate load step: acceptance owns
// the coarse four-unit cargo fill, so delivery rules can run directly from the
// accepted contract.

}   // namespace jobs_test

inline void JobsSuite(TestRunner &runner) {
    using namespace jobs_test;

    {
        Test test("the single freight offer is deterministic and well formed");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State first;
        game::reset(first);
        test.assertTrue(jobs::refreshOffers(first, origin.id), "offer generates");
        test.assertEqual(first.offerCount, game::kJobOffers, "one freight offer is stored");
        test.assertEqual(game::kJobOffers, 1, "the board carries exactly one freight row");

        game::State second;
        game::reset(second);
        test.assertTrue(jobs::refreshOffers(second, origin.id), "offer regenerates from the same state");

        test.assertEqual(second.offers[0].from, first.offers[0].from, "origin is deterministic");
        test.assertEqual(second.offers[0].to, first.offers[0].to, "destination is deterministic");
        test.assertEqual(second.offers[0].crates, first.offers[0].crates, "crate count is deterministic");
        test.assertEqual(second.offers[0].reward, first.offers[0].reward, "reward is deterministic");
        test.assertEqual(second.offerSerial, first.offerSerial, "offer serial advances identically");

        test.assertEqual(first.offers[0].crates, game::kJobCrates, "the contract requires four units");
        test.assertTrue(first.offers[0].reward > 0, "reward is positive");
        test.assertEqual(first.offers[0].reward % jobs::kRewardRound, 0, "reward is a multiple of five");
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(first.offers[0].kind, game::kJobKindOrdinary, "the freight row is ordinary work");
        test.assertEqual(first.offerCursor, 0, "the fresh board records its pre-board cursor");
#endif
        runner.add(test);
    }

    {
        Test test("offers exclude the origin and use a real settlement");
        for (uint8_t originId = 0; originId < world::settlementCount(); ++originId) {
            world::Settlement origin;
            test.assertTrue(world::settlementInfo(originId, origin), "settlement metadata loads");

            game::State state;
            game::reset(state);
            test.assertTrue(jobs::refreshOffers(state, originId), "offer generates for every origin");
            test.assertEqual(state.offerCount, 1, "one row posts for every origin");
            test.assertEqual(state.offers[0].from, originId, "offer records its origin");
            test.assertTrue(state.offers[0].to != originId, "destination differs from origin");
            test.assertTrue(state.offers[0].to < world::settlementCount(), "destination is a real settlement");
        }
        runner.add(test);
    }

    {
        Test test("baked route rewards match the reference recipe and are symmetric");
        bool rewardsMatch = true;
        bool symmetric = true;
        bool widthsHold = true;

        for (uint8_t fromId = 0; fromId < world::settlementCount(); ++fromId) {
            for (uint8_t toId = 0; toId < world::settlementCount(); ++toId) {
                const jobs::RouteEconomy actual = jobs::routeEconomy(fromId, toId);
                if (fromId == toId) {
                    test.assertEqual(actual.reward, 0, "diagonal route is unused");
                    continue;
                }

                const jobs::RouteEconomy expected = referenceRoute(fromId, toId);
                rewardsMatch = rewardsMatch && actual.reward == expected.reward;
                symmetric = symmetric && jobs::routeEconomy(toId, fromId).reward == actual.reward;
                widthsHold = widthsHold && actual.reward <= 255;
            }
        }

        test.assertTrue(rewardsMatch, "every ordered pair keeps the old reward");
        test.assertTrue(symmetric, "routes are symmetric");
        test.assertTrue(widthsHold, "reward fits one byte");

        // Offers expose exactly the table value for their chosen pair.
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "origin metadata loads");
        game::State state;
        game::reset(state);
        test.assertTrue(jobs::refreshOffers(state, origin.id), "offer generates");
        const jobs::RouteEconomy route = jobs::routeEconomy(state.offers[0].from, state.offers[0].to);
        test.assertEqual(state.offers[0].reward, route.reward, "offer reward comes from the table");
        runner.add(test);
    }

    {
        Test test("acceptance auto-loads cargo, consumes the offer, and stays docked");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at the offer station");
        test.assertTrue(!station::jobActive(state), "no contract at a fresh dock");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryNone, "a fresh run has no delivery note");

        const uint16_t creditsBefore = state.credits;
        const uint8_t offersBefore = state.offerCount;
        const game::JobOffer chosen = state.offers[0];

        state.cargo = 2;
        test.assertTrue(jobs::accept(state, 0), "A on the freight row accepts");

        test.assertTrue(station::jobActive(state), "contract is active");
        test.assertEqual(state.activeJob.to, chosen.to, "destination comes from the offer");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "contract requires four units");
        test.assertEqual(state.activeJob.reward, chosen.reward, "reward is fixed at acceptance");
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(state.activeJob.kind, game::kJobKindOrdinary, "active kind is ordinary freight");
        test.assertEqual(state.activeJob.mission, game::kMissionNone, "freight has no campaign mission");
        test.assertEqual(state.activeJob.from, chosen.from, "active freight records its origin");
#endif
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "trim is neutral for the auto-load");
        test.assertEqual(state.docked, static_cast<int8_t>(origin.id), "acceptance stays docked");
        test.assertEqual(state.offerCount, offersBefore - 1, "accepted offer is consumed");
        test.assertEqual(state.credits, creditsBefore, "acceptance pays nothing");

        game::State free;
        game::reset(free);
        test.assertTrue(jobs::refreshOffers(free, origin.id), "offer available in flight");
        test.assertTrue(!jobs::accept(free, 0), "acceptance requires a docked boat");
        test.assertTrue(!jobs::accept(state, 0), "a second contract is refused while one is active");
        runner.add(test);
    }

    {
        Test test("abandon clears the contract and hold and refreshes the offer");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock succeeds");
        test.assertTrue(!jobs::abandon(state), "abandon is inert without a contract");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        state.lastDelivery = jobs::kDeliveryFailed;   // simulate a stale note before abandon

        const uint16_t credits = state.credits;
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON clears the contract");
        test.assertTrue(!station::jobActive(state), "contract is gone");
        test.assertEqual(state.cargo, 0, "hold is cleared");
        test.assertEqual(state.offerCount, 1, "the freight row is refreshed");
        test.assertEqual(state.credits, credits, "abandonment pays nothing");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryNone, "abandon clears the delivery note");
        runner.add(test);
    }

    {
        Test test("delivery at the destination pays credits and clears the hold");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;

        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.docked, static_cast<int8_t>(destination), "destination station is current");
        test.assertEqual(state.credits, reward, "reward is paid");
        test.assertTrue(!station::jobActive(state), "contract is cleared after delivery");
        test.assertEqual(state.cargo, 0, "delivered hold is cleared");
        test.assertEqual(state.offerCount, 1, "destination offer is refreshed");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "paid delivery records success");
        test.assertTrue(!jobs::deliveryFailed(state), "a paid delivery has no failure note");
        runner.add(test);
    }

    {
        Test test("again after a very long untimed trip the freight still pays");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;
        test.assertTrue(station::undock(state), "undock for the long trip");

        // Ten relaxed minutes in open water: far beyond every retired express
        // allowance. No timer remains to fail the job.
        for (uint16_t i = 0; i < 10 * 60 * 60; ++i)
            game::step(state, idle());
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the long trip stays in open water");
#if SH_FEATURE_CAMPAIGN
        test.assertEqual(state.voyageTicks, 10 * 60 * 60, "the voyage clock still advances");
#endif

        test.assertTrue(travelTo(state, destination), "dock after the long trip");
        test.assertEqual(state.credits, reward, "a late-looking trip still pays");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "long trip delivery is paid");
        runner.add(test);
    }

    {
        Test test("insufficient cargo fails an otherwise valid delivery");
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
        test.assertTrue(!station::jobActive(state), "failed contract is cleared");
        test.assertEqual(state.cargo, 0, "failed hold is cleared");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "short hold records the failure");
        test.assertTrue(jobs::deliveryFailed(state), "short hold shows the SHORT note");
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
        Test test("freight pays for upgrades and long hauls can resupply without losing the job");
        game::State fresh;
        game::reset(fresh);
        test.assertTrue(fresh.air > 0, "a fresh run starts with air");
#if SH_FEATURE_HULL_DAMAGE
        test.assertTrue(fresh.hull > 0, "a fresh run starts with hull");
#endif

        for (uint8_t originId = 0; originId < world::settlementCount(); ++originId) {
            world::Settlement origin;
            test.assertTrue(world::settlementInfo(originId, origin), "origin metadata loads");

            game::State state;
            game::reset(state);
            test.assertTrue(jobs::refreshOffers(state, originId), "offer generates for the balance check");

            world::Settlement destination;
            test.assertTrue(world::settlementInfo(state.offers[0].to, destination), "destination metadata loads");

            const int32_t dx = static_cast<int32_t>(destination.pixel_x) - origin.pixel_x;
            const int32_t dy = static_cast<int32_t>(destination.pixel_y) - origin.pixel_y;
            const uint16_t distancePixels = isqrt32(static_cast<uint32_t>(dx * dx + dy * dy));
            const uint16_t loadedSpeed = static_cast<uint16_t>(game::topSpeedQ8For(game::kJobCrates) / game::kQ8One);
            test.assertTrue(loadedSpeed > 0, "a loaded boat still moves");
            test.assertTrue(distancePixels > 0, "the route has length");

            game::State resupply;
            game::reset(resupply);
            resupply.activeJob.to = state.offers[0].to;
            resupply.activeJob.crates = game::kJobCrates;
            resupply.activeJob.reward = state.offers[0].reward;
            game::setCargo(resupply, game::kJobCrates);
            uint8_t stop = 0;
            while (stop == originId || stop == state.offers[0].to)
                ++stop;
            world::Settlement berth;
            test.assertTrue(world::settlementInfo(stop, berth), "an intermediate berth exists");
            resupply.x = berth.pixel_x;
            resupply.y = berth.pixel_y;
            resupply.air = 0;
            test.assertTrue(station::dock(resupply), "long-haul resupply docks normally");
            test.assertEqual(resupply.air, game::airCapacity(resupply), "service refills the tank");
            test.assertEqual(resupply.activeJob.to, state.offers[0].to, "resupply retains destination");
            test.assertEqual(resupply.cargo, game::kJobCrates, "resupply preserves the loaded hold");
#if SH_FEATURE_SHOP
            test.assertTrue(state.offers[0].reward >= shop::kAirPrice, "a contract pays for the cheapest upgrade");
            test.assertTrue(state.offers[0].reward >= shop::kHullPrice, "a contract pays for a hull plate");
#endif
        }
        runner.add(test);
    }

    {
        Test test("station board offers WORK and active board keeps ABANDON deliberate");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "no-job board shape");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "first row is WORK");
        test.assertEqual(station::rowKind(station::kUndockRow, state), station::kRowUndock, "last row is UNDOCK");
        test.assertEqual(station::selectPrevious(station::kFirstRow, state), station::kUndockRow, "board wraps upward");
        test.assertEqual(station::selectNext(station::kUndockRow, state), station::kFirstRow, "board wraps downward");

        test.assertTrue(jobs::accept(state, 0), "contract accepted");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "active board shape");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row");
        test.assertEqual(station::initialRow(state), station::kActiveUndockRow, "safe UNDOCK starts the active board");
        test.assertEqual(station::selectNext(station::kActiveUndockRow, state), station::kAbandonRow, "down reaches ABANDON");
#if SH_FEATURE_SHOP
        test.assertEqual(station::selectPrevious(station::kActiveUndockRow, state), station::kActiveShopRow, "up reaches SHOP");
        test.assertEqual(station::selectPrevious(station::kActiveShopRow, state), station::kActiveWorkRow, "SHOP follows WORK");
        test.assertEqual(station::selectPrevious(station::kActiveWorkRow, state), station::kAbandonRow, "WORK follows ABANDON");
#else
        test.assertEqual(station::selectPrevious(station::kActiveUndockRow, state), station::kActiveWorkRow, "up reaches WORK");
        test.assertEqual(station::selectPrevious(station::kActiveWorkRow, state), station::kAbandonRow, "WORK follows ABANDON");
#endif
        test.assertEqual(station::rowKind(station::kActiveWorkRow, state), station::kRowWork, "active WORK row");
        test.assertEqual(station::rowKind(station::kAbandonRow, state), station::kRowAbandon, "ABANDON row");
        test.assertEqual(station::rowKind(station::kActiveUndockRow, state), station::kRowUndock, "UNDOCK row");
        test.assertEqual(station::activate(station::kActiveSummaryRow, state), station::kActionNone, "summary row is inert");
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "ABANDON action works");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "board returns to WORK after abandonment");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_JOBS
