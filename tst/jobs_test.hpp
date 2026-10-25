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
    constexpr uint16_t kDeadlineBaseSeconds = 80;
    constexpr uint16_t kDeadlinePercentPerTile = 55;
    constexpr uint16_t kDeadlinePercentScale = 100;
    constexpr uint16_t kDeadlineSecondsPerCrate = 8;

    world::Settlement from;
    world::Settlement to;
    if (!world::settlementInfo(fromId, from) || !world::settlementInfo(toId, to))
        return jobs::RouteEconomy{0, 0};

    const int32_t dx = static_cast<int32_t>(to.pixel_x) - from.pixel_x;
    const int32_t dy = static_cast<int32_t>(to.pixel_y) - from.pixel_y;
    const uint16_t distancePixels = isqrt32(static_cast<uint32_t>(dx * dx + dy * dy));
    const uint16_t distanceTiles = static_cast<uint16_t>(distancePixels / 8);

    const uint16_t base = static_cast<uint16_t>(kRewardBase + game::kJobCrates * kRewardPerCrate +
                                                distanceTiles / kRewardDistanceDivisor);
    const uint16_t reward = static_cast<uint16_t>(((base + kRewardRound / 2) / kRewardRound) * kRewardRound);
    const uint32_t seconds = static_cast<uint32_t>(kDeadlineBaseSeconds) +
                            (static_cast<uint32_t>(distanceTiles) * kDeadlinePercentPerTile) / kDeadlinePercentScale +
                            static_cast<uint32_t>(game::kJobCrates) * kDeadlineSecondsPerCrate;
    constexpr uint32_t kLegacyDeadlineMaxTicks = 0xFFFF;
    const uint32_t ticks = seconds * game::kStepHz;
    return jobs::RouteEconomy{static_cast<uint8_t>(reward),
                              static_cast<uint16_t>(ticks > kLegacyDeadlineMaxTicks ? kLegacyDeadlineMaxTicks : ticks)};
}

// Simulated flight fixtures never need a separate load step: acceptance owns
// the coarse four-unit cargo fill, so delivery rules can run directly from the
// accepted contract.

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
        Test test("baked routes preserve the canonical reward and deadline rules");
        bool rewardsMatch = true;
        bool deadlinesMatch = true;
        bool symmetric = true;
        bool widthsHold = true;

        for (uint8_t fromId = 0; fromId < world::settlementCount(); ++fromId) {
            for (uint8_t toId = 0; toId < world::settlementCount(); ++toId) {
                const jobs::RouteEconomy actual = jobs::routeEconomy(fromId, toId);
                if (fromId == toId) {
                    test.assertEqual(actual.reward, 0, "diagonal route is unused");
                    test.assertEqual(actual.deadline, 0, "diagonal deadline is unused");
                    continue;
                }

                const jobs::RouteEconomy expected = referenceRoute(fromId, toId);
                rewardsMatch = rewardsMatch && actual.reward == expected.reward;
                deadlinesMatch = deadlinesMatch && actual.deadline == expected.deadline;
                symmetric = symmetric && jobs::routeEconomy(toId, fromId).reward == actual.reward &&
                            jobs::routeEconomy(toId, fromId).deadline == actual.deadline;
                widthsHold = widthsHold && actual.reward <= 255 && actual.deadline < game::kJobElapsedLate;
            }
        }

        test.assertTrue(rewardsMatch, "every ordered pair keeps the old reward");
        test.assertTrue(deadlinesMatch, "every ordered pair keeps the old deadline");
        test.assertTrue(symmetric, "routes are symmetric");
        test.assertTrue(widthsHold, "reward fits one byte and every deadline is before the late marker");

        // Offers expose exactly the table values for their chosen pair.
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "origin metadata loads");
        game::State state;
        game::reset(state);
        test.assertTrue(jobs::refreshOffers(state, origin.id), "offers generate");
        for (uint8_t i = 0; i < state.offerCount; ++i) {
            const jobs::RouteEconomy route = jobs::routeEconomy(state.offers[i].from, state.offers[i].to);
            test.assertEqual(state.offers[i].reward, route.reward, "offer reward comes from the table");
            test.assertEqual(state.offers[i].deadline, route.deadline, "offer deadline comes from the table");
        }
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
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "A on an offer accepts");

        test.assertTrue(station::jobActive(state), "contract is active");
        test.assertEqual(state.activeJob.to, chosen.to, "destination comes from the offer");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "contract requires four units");
        test.assertEqual(state.activeJob.reward, chosen.reward, "reward is fixed at acceptance");
        test.assertEqual(state.activeJob.deadline, chosen.deadline, "deadline is stored in flight ticks");
        test.assertEqual(state.activeJob.elapsed, 0, "contract age starts at acceptance");
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "trim is neutral for the auto-load");
        test.assertEqual(state.docked, static_cast<int8_t>(origin.id), "acceptance stays docked");
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
        state.lastDelivery = jobs::kDeliveryFailed; // simulate a stale note before abandon

        const uint16_t credits = state.credits;
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON clears the contract");
        test.assertTrue(!station::jobActive(state), "contract is gone");
        test.assertEqual(state.cargo, 0, "hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "offers are refreshed");
        test.assertEqual(state.credits, credits, "abandonment pays nothing");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryNone, "abandon clears the delivery note");
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
        const uint8_t destination = state.activeJob.to;
        const uint16_t reward = state.activeJob.reward;

        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.docked, static_cast<int8_t>(destination), "destination station is current");
        test.assertEqual(state.credits, reward, "reward is paid");
        test.assertTrue(!station::jobActive(state), "contract is cleared after delivery");
        test.assertEqual(state.cargo, 0, "delivered hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "destination offers are refreshed");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryPaid, "paid delivery records success");
        test.assertTrue(!jobs::deliveryFailed(state), "a paid delivery has no failure note");
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
        state.activeJob.elapsed = 2;
        test.assertTrue(travelTo(state, destination), "dock at destination");
        test.assertEqual(state.credits, 0, "late delivery pays nothing");
        test.assertTrue(!station::jobActive(state), "late contract is cleared");
        test.assertEqual(state.cargo, 0, "late hold is cleared");
        test.assertEqual(state.offerCount, game::kJobOffers, "offers are refreshed");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "late delivery records the failure");
        test.assertTrue(jobs::deliveryFailed(state), "late delivery shows the LATE/SHORT note");
        test.assertEqual(station::activate(station::kFirstRow, state), station::kActionAccepted, "acceptance clears the note");
        test.assertTrue(!jobs::deliveryFailed(state), "the note does not outlive acceptance");
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
        test.assertTrue(!station::jobActive(state), "failed contract is cleared");
        test.assertEqual(state.cargo, 0, "failed hold is cleared");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "short hold records the failure");
        test.assertTrue(jobs::deliveryFailed(state), "short hold shows the LATE/SHORT note");
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
        Test test("deadlines pause while docked and advance with flight ticks");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        Fixture fake;
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(state, 0), "contract accepted");

        const uint16_t dockedElapsed = state.activeJob.elapsed;
        for (uint8_t i = 0; i < 60; ++i)
            test.assertTrue(!station::stepFlight(state, idle(), fake), "docked steps do not re-dock");
        test.assertEqual(state.activeJob.elapsed, dockedElapsed, "job age is frozen while docked");
        test.assertTrue(station::jobActive(state), "contract survives a paused station visit");

        test.assertTrue(station::undock(state), "undock");
        const uint16_t flightElapsed = state.activeJob.elapsed;
        for (uint8_t i = 0; i < 10; ++i)
            station::stepFlight(state, idle(), fake);
        test.assertEqual(state.activeJob.elapsed, static_cast<uint16_t>(flightElapsed + 10),
                         "job age advances one tick per flight step");
        runner.add(test);
    }

    {
        Test test("deadline is inclusive and a late contract never revives");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        // Exactly at the deadline still pays.
        game::State onTime;
        game::reset(onTime);
        test.assertTrue(dockAt(onTime, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(onTime, 0), "contract accepted");
        const uint8_t destination = onTime.activeJob.to;
        const uint16_t reward = onTime.activeJob.reward;
        onTime.dockCooldown = 0;
        onTime.activeJob.elapsed = onTime.activeJob.deadline;
        test.assertEqual(jobs::remainingTicks(onTime), 0, "zero remaining ticks at the deadline");
        test.assertTrue(travelTo(onTime, destination), "dock exactly on time");
        test.assertEqual(onTime.credits, reward, "exact-deadline delivery is paid");
        test.assertEqual(onTime.lastDelivery, jobs::kDeliveryPaid, "exact-deadline result is paid");

        // One tick later is permanently late.
        game::State late;
        game::reset(late);
        test.assertTrue(dockAt(late, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(late, 0), "late contract accepted");
        const uint8_t lateDestination = late.activeJob.to;
        late.activeJob.elapsed = static_cast<uint16_t>(late.activeJob.deadline + 1);
        test.assertEqual(jobs::remainingTicks(late), 0, "one tick past the deadline has no time");
        test.assertTrue(travelTo(late, lateDestination), "dock past the deadline");
        test.assertEqual(late.credits, 0, "overdue delivery pays nothing");
        test.assertEqual(late.lastDelivery, jobs::kDeliveryFailed, "overdue delivery is failed");

        // Saturation is a dead end, not a wrap into a live window.
        game::State saturated;
        game::reset(saturated);
        test.assertTrue(dockAt(saturated, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(saturated, 0), "saturating contract accepted");
        const uint8_t saturatedDestination = saturated.activeJob.to;
        saturated.activeJob.elapsed = static_cast<uint16_t>(game::kJobElapsedLate - 1);
        test.assertTrue(station::undock(saturated), "saturation contract undocks so flight ages it");
        game::step(saturated, idle());
        test.assertEqual(saturated.activeJob.elapsed, game::kJobElapsedLate, "age reaches the late marker");
        game::step(saturated, idle());
        test.assertEqual(saturated.activeJob.elapsed, game::kJobElapsedLate, "late marker saturates");
        test.assertEqual(jobs::remainingTicks(saturated), 0, "saturated age has no time");
        test.assertTrue(travelTo(saturated, saturatedDestination), "dock after saturation");
        test.assertEqual(saturated.credits, 0, "saturated contract never pays");

        // A new acceptance resets the age, and reset/abandon clear it.
        game::State fresh;
        game::reset(fresh);
        test.assertTrue(dockAt(fresh, origin.id), "dock at origin");
        test.assertTrue(jobs::accept(fresh, 0), "fresh contract accepted");
        test.assertEqual(fresh.activeJob.elapsed, 0, "acceptance resets the age");
        test.assertTrue(jobs::abandon(fresh), "abandon clears the contract");
        test.assertEqual(fresh.activeJob.elapsed, 0, "abandon clears the age");
        runner.add(test);
    }

    {
        Test test("balance: every contract is reachable on one tank and pays for an upgrade");
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
            test.assertTrue(jobs::refreshOffers(state, originId), "offers generate for the balance check");

            for (uint8_t i = 0; i < state.offerCount; ++i) {
                world::Settlement destination;
                test.assertTrue(world::settlementInfo(state.offers[i].to, destination), "destination metadata loads");

                const int32_t dx = static_cast<int32_t>(destination.pixel_x) - origin.pixel_x;
                const int32_t dy = static_cast<int32_t>(destination.pixel_y) - origin.pixel_y;
                const uint16_t distancePixels = isqrt32(static_cast<uint32_t>(dx * dx + dy * dy));
                const uint16_t loadedSpeed = static_cast<uint16_t>(game::topSpeedQ8For(game::kJobCrates) / game::kQ8One);
                const uint32_t straightTicks = static_cast<uint32_t>(distancePixels) * game::kStepHz / loadedSpeed;
                const uint32_t airTicks = static_cast<uint32_t>(game::kAirCapacity) * game::kAirDrainTicks;

                test.assertTrue(loadedSpeed > 0, "a loaded boat still moves");
                test.assertTrue(state.offers[i].deadline > straightTicks, "deadline exceeds straight-line travel");
                test.assertTrue(state.offers[i].deadline <= airTicks, "a full tank covers the whole deadline");
#if SH_FEATURE_SHOP
                test.assertTrue(state.offers[i].reward >= shop::kSonarPrice, "a contract pays for the cheapest upgrade");
                test.assertTrue(state.offers[i].reward >= shop::kAirPrice, "a contract pays for an air tank");
                test.assertTrue(state.offers[i].reward >= shop::kHullPrice, "a contract pays for a hull plate");
#endif
            }
        }
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
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "active board has summary, ABANDON, SHOP, UNDOCK");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row");
        test.assertEqual(station::initialRow(state), station::kAbandonRow, "selection starts on ABANDON");
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
