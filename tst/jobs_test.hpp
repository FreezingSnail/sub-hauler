#pragma once

#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "world_test.hpp"

// Campaign contract rules: explicit acceptance/load handoff, abandonment,
// untimed delivery outcomes and station row selection. There are no generated
// freight offers, rewards or currency. Settlement ids come from the generated
// metadata walk, never a literal index.
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

}   // namespace jobs_test

inline void JobsSuite(TestRunner &runner) {
    using namespace jobs_test;

    {
        Test test("campaign contract metadata is fixed and carries no payout");
        world::Settlement origin;
        test.assertTrue(anySettlement(origin), "settlement metadata loads");

        game::State state;
        game::reset(state);
        test.assertTrue(!game::hasActiveJob(state), "reset leaves no contract");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryNone, "a fresh run has no delivery note");

        campaign::MissionSpec spec;
        test.assertTrue(campaign::missionInfo(campaign::kFirst, spec), "FIRST metadata loads");
        test.assertEqual(spec.from, campaign::kTidewall, "FIRST loads at Tidewall");
        test.assertEqual(spec.to, campaign::kBenthos, "FIRST delivers at Benthos");
        test.assertEqual(game::kJobCrates, 4, "the story contract requires four units");
        test.assertEqual(state.delivered, 0, "fresh run has no deliveries");
        runner.add(test);
    }

    {
        Test test("acceptance auto-loads cargo, stays docked and pays nothing");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at the source station");
        test.assertTrue(!game::hasActiveJob(state), "no contract at a fresh dock");

        state.cargo = 2;
        test.assertTrue(campaign::accept(state, campaign::kFirst), "FIRST accepts at Tidewall");

        test.assertTrue(station::jobActive(state), "contract is active");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "destination comes from the mission");
        test.assertEqual(state.activeJob.crates, game::kJobCrates, "contract requires four units");
        test.assertEqual(state.activeJob.kind, game::kJobKindCampaign, "active kind is campaign");
        test.assertEqual(state.activeJob.mission, campaign::kFirst, "active mission is FIRST");
        test.assertEqual(state.activeJob.from, state.docked, "active contract records its origin");
        test.assertEqual(state.cargo, game::kJobCrates, "acceptance auto-loads four units");
        test.assertEqual(state.ballast, game::neutralFor(state.cargo), "trim is neutral for the auto-load");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kTidewall), "acceptance stays docked");

        game::State free;
        game::reset(free);
        test.assertTrue(!campaign::accept(free, campaign::kFirst), "acceptance requires a docked boat");
        test.assertTrue(!campaign::accept(state, campaign::kFirst), "a second contract is refused while one is active");
        runner.add(test);
    }

    {
        Test test("abandon clears the contract and hold without a payout");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock succeeds");
        test.assertTrue(!jobs::abandon(state), "abandon is inert without a contract");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");
        state.lastDelivery = jobs::kDeliveryFailed;   // simulate a stale note before abandon

        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "A on ABANDON clears the contract");
        test.assertTrue(!station::jobActive(state), "contract is gone");
        test.assertEqual(state.cargo, 0, "hold is cleared");
        test.assertEqual(state.ballast, game::neutralFor(0), "unladen trim is restored");
        test.assertEqual(state.delivered, 0, "abandonment never counts a delivery");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryNone, "abandon clears the delivery note");
        test.assertTrue(campaign::missionAvailable(state, campaign::kFirst), "abandon leaves the mission available");
        runner.add(test);
    }

    {
        Test test("delivery at the destination completes the mission and clears the hold");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");

        test.assertTrue(travelTo(state, campaign::kBenthos), "dock at destination");
        test.assertEqual(state.docked, static_cast<int8_t>(campaign::kBenthos), "destination station is current");
        test.assertTrue(!station::jobActive(state), "contract is cleared after delivery");
        test.assertEqual(state.cargo, 0, "delivered hold is cleared");
        test.assertEqual(state.delivered, 1, "delivered count increments");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryDelivered, "delivery records success");
        test.assertTrue(!jobs::deliveryFailed(state), "a completed delivery has no failure note");
        test.assertEqual(state.campaign & campaign::kMilestoneFirst, campaign::kMilestoneFirst, "FIRST milestone set");
        test.assertTrue(campaign::missionCompleted(state, campaign::kFirst), "FIRST completion flag");
        runner.add(test);
    }

    {
        Test test("a story delivery is untimed and resolves after a long flight");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");
        test.assertTrue(station::undock(state), "undock for the long trip");

        // Ten relaxed minutes in open water: no timer exists to fail the job.
        for (uint16_t i = 0; i < 10 * 60 * 60; ++i)
            game::step(state, idle());
        test.assertEqual(state.docked, static_cast<int8_t>(-1), "the long trip stays in open water");
        test.assertEqual(state.voyageTicks, 10 * 60 * 60, "the voyage clock still advances");

        test.assertTrue(travelTo(state, campaign::kBenthos), "dock after the long trip");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryDelivered, "a late-looking trip still completes");
        test.assertEqual(state.delivered, 1, "the long trip counts one delivery");
        runner.add(test);
    }

    {
        Test test("insufficient cargo fails an otherwise valid delivery");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");
        state.cargo = static_cast<uint8_t>(state.activeJob.crates - 1);

        test.assertTrue(travelTo(state, campaign::kBenthos), "dock at destination");
        test.assertTrue(!station::jobActive(state), "failed contract is cleared");
        test.assertEqual(state.cargo, 0, "failed hold is cleared");
        test.assertEqual(state.delivered, 0, "a short hold counts no delivery");
        test.assertEqual(state.lastDelivery, jobs::kDeliveryFailed, "short hold records the failure");
        test.assertTrue(jobs::deliveryFailed(state), "short hold shows the SHORT note");
        test.assertEqual(state.campaign & campaign::kMilestoneFirst, 0, "short cargo sets no milestone");
        test.assertTrue(campaign::missionAvailable(state, campaign::kFirst), "source can reissue after a short load");
        runner.add(test);
    }

    {
        Test test("origin and wrong-station docks do not deliver");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");

        test.assertTrue(travelTo(state, campaign::kTidewall), "dock back at the origin");
        test.assertTrue(station::jobActive(state), "origin dock keeps the contract");
        test.assertEqual(state.cargo, game::kJobCrates, "origin dock keeps the hold");
        test.assertEqual(state.delivered, 0, "origin dock delivers nothing");

        test.assertTrue(travelTo(state, campaign::kCormorant), "dock at a third station");
        test.assertTrue(station::jobActive(state), "wrong-station dock keeps the contract");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "contract destination is unchanged");
        test.assertEqual(state.cargo, game::kJobCrates, "wrong-station dock keeps the hold");
        test.assertEqual(state.delivered, 0, "wrong-station dock delivers nothing");
        runner.add(test);
    }

    {
        Test test("an intermediate dock refills the fixed tank and keeps the contract");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");
        test.assertTrue(station::undock(state), "undock loaded");
        state.air = 0;
        test.assertTrue(dockAt(state, campaign::kCormorant), "intermediate berth docks normally");
        test.assertEqual(state.air, game::airCapacity(state), "service refills the whole tank");
        test.assertEqual(state.air, game::kAirCapacity, "the tank is the fixed 300 units");
        test.assertEqual(state.activeJob.to, campaign::kBenthos, "resupply retains destination");
        test.assertEqual(state.cargo, game::kJobCrates, "resupply preserves the loaded hold");
        runner.add(test);
    }

    {
        Test test("station board offers WORK and active board keeps ABANDON deliberate");
        game::State state;
        game::reset(state);
        test.assertTrue(dockAt(state, campaign::kTidewall), "dock at origin");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "no-job board shape");
        test.assertEqual(station::kStationRowCount, 2, "no-job board is WORK/UNDOCK");
        test.assertEqual(station::rowKind(station::kFirstRow, state), station::kRowWork, "first row is WORK");
        test.assertEqual(station::rowKind(station::kUndockRow, state), station::kRowUndock, "last row is UNDOCK");
        test.assertEqual(station::selectPrevious(station::kFirstRow, state), station::kUndockRow, "board wraps upward");
        test.assertEqual(station::selectNext(station::kUndockRow, state), station::kFirstRow, "board wraps downward");

        test.assertTrue(campaign::accept(state, campaign::kFirst), "contract accepted");
        test.assertEqual(station::rowCount(state), station::kActiveRowCount, "active board shape");
        test.assertEqual(station::kActiveRowCount, 4, "active board is summary/ABANDON/WORK/UNDOCK");
        test.assertEqual(station::rowKind(station::kActiveSummaryRow, state), station::kRowJob, "summary row");
        test.assertEqual(station::initialRow(state), station::kActiveUndockRow, "safe UNDOCK starts the active board");
        test.assertEqual(station::selectNext(station::kActiveUndockRow, state), station::kAbandonRow, "down reaches ABANDON");
        test.assertEqual(station::selectPrevious(station::kActiveUndockRow, state), station::kActiveWorkRow, "up reaches WORK");
        test.assertEqual(station::selectPrevious(station::kActiveWorkRow, state), station::kAbandonRow, "WORK follows ABANDON");
        test.assertEqual(station::rowKind(station::kActiveWorkRow, state), station::kRowWork, "active WORK row");
        test.assertEqual(station::rowKind(station::kAbandonRow, state), station::kRowAbandon, "ABANDON row");
        test.assertEqual(station::rowKind(station::kActiveUndockRow, state), station::kRowUndock, "UNDOCK row");
        test.assertEqual(station::activate(station::kActiveSummaryRow, state), station::kActionNone, "summary row is inert");
        test.assertEqual(station::activate(station::kAbandonRow, state), station::kActionAbandoned, "ABANDON action works");
        test.assertEqual(station::rowCount(state), station::kStationRowCount, "board returns to WORK after abandonment");
        runner.add(test);
    }
}
