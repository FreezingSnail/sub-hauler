#pragma once

// Native integration tests for checkpoint application (sub-hauler-7px.4).
// These exercise the same voyage_flow policy the sketch calls: boot
// classification, Continue restoration, deterministic offers, express-age
// preservation, rescue commits and failed-write retry. Storage fault injection
// stays in voyage_save_test's FakeStorage so there is one EEPROM fixture.

#include "../src/features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/voyage_flow.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "voyage_save_test.hpp"

namespace voyage_flow_test {

using voyage_save_test::FakeStorage;
using voyage_save_test::Fixture;
using voyage_save_test::kNoFault;
using voyage_save_test::setValidPayload;

inline bool sameOffers(const game::State &a, const game::State &b) {
    if (a.offerCount != b.offerCount || a.offerCursor != b.offerCursor || a.offerSerial != b.offerSerial)
        return false;
    for (uint8_t i = 0; i < a.offerCount; ++i) {
        if (a.offers[i].from != b.offers[i].from || a.offers[i].to != b.offers[i].to || a.offers[i].crates != b.offers[i].crates || a.offers[i].reward != b.offers[i].reward ||
            a.offers[i].deadline != b.offers[i].deadline)
            return false;
#if SH_FEATURE_CAMPAIGN
        if (a.offers[i].kind != b.offers[i].kind)
            return false;
#endif
    }
    return true;
}

inline bool makeExpressAtIntermediate(Fixture &fixture, uint16_t elapsed) {
    fixture.reset();
    fixture.state.docked = static_cast<int8_t>(campaign::kCormorant);
    fixture.state.cargo = game::kJobCrates;
    fixture.state.ballast = static_cast<uint8_t>(game::neutralFor(fixture.state.cargo));
    fixture.state.offerCursor = 0x0123;
    fixture.state.offerSerial = 0x0125;
    uint8_t routeReward = 0;
    uint16_t routeDeadline = 0;
    if (!world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, routeDeadline))
        return false;
    fixture.state.activeJob.kind = game::kJobKindExpress;
    fixture.state.activeJob.mission = game::kMissionNone;
    fixture.state.activeJob.from = campaign::kTidewall;
    fixture.state.activeJob.to = campaign::kBenthos;
    fixture.state.activeJob.crates = game::kJobCrates;
    fixture.state.activeJob.reward = game::expressReward(routeReward);
    fixture.state.activeJob.deadline = routeDeadline;
    fixture.state.activeJob.elapsed = elapsed;
    return campaign::activeJobValid(fixture.state);
}

}   // namespace voyage_flow_test

inline void VoyageFlowSuite(TestRunner &runner) {
    using namespace voyage_flow_test;

    {
        Test test("boot distinguishes blank, valid, corrupt and incompatible saves");
        FakeStorage blank(0xFF);
        voyage_flow::BootStatus status = voyage_flow::inspectBoot(blank);
        test.assertEqual(status.state, voyage_flow::kBootBlank, "blank EEPROM");
        test.assertEqual(status.hasValid, 0, "blank has no Continue");
        test.assertEqual(status.needsReplaceConfirm(), 0, "blank starts without replacement confirmation");

        FakeStorage storage(0xFF);
        Fixture saved;
        setValidPayload(saved);
        test.assertEqual(saved.save(storage), voyage_save::kSaved, "baseline voyage");
        status = voyage_flow::inspectBoot(storage);
        test.assertEqual(status.state, voyage_flow::kBootValid, "valid EEPROM");
        test.assertEqual(status.hasValid, 1, "valid offers Continue");

        FakeStorage corrupt = storage;
        corrupt.bytes[voyage_save::kSlotBAddress] ^= 0xFF;
        status = voyage_flow::inspectBoot(corrupt);
        test.assertEqual(status.state, voyage_flow::kBootCorrupt, "torn magic is corrupt");
        test.assertEqual(status.hasValid, 0, "corrupt is not Continue");
        test.assertEqual(status.needsReplaceConfirm(), 1, "corrupt warns before replacement");

        FakeStorage incompatible = storage;
        incompatible.bytes[voyage_save::kSlotBAddress + voyage_save::kSignatureOffset] ^= 0x01;
        status = voyage_flow::inspectBoot(incompatible);
        test.assertEqual(status.state, voyage_flow::kBootIncompatible, "profile mismatch is incompatible");
        test.assertEqual(status.hasIncompatible, 1, "incompatible is reported");
        runner.add(test);
    }

    {
        Test test("Continue restores the intermediate berth and preserves express age");
        Fixture source;
        test.assertTrue(makeExpressAtIntermediate(source, 1234), "valid express checkpoint at Cormorant");
        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "intermediate checkpoint saves");

        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     restored.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads");
        test.assertEqual(restored.state.docked, campaign::kCormorant, "intermediate berth restored");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo remains active");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindExpress, "express kind restored");
        test.assertEqual(restored.state.activeJob.elapsed, 1234, "express age is not reset by loading");
        test.assertEqual(restored.state.offerCursor, 0x0123, "saved offer cursor restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "berth restore refills air");
        test.assertEqual(restored.state.vxQ8, 0, "transient velocity resets");
        test.assertEqual(restored.state.vyQ8, 0, "transient vertical velocity resets");
        runner.add(test);
    }

    {
        Test test("restored freight board is deterministic and does not move the saved cursor");
        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kBenthos);
        source.state.offerCursor = 4242;
        source.state.offerSerial = 4244;
        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "no-job berth checkpoint saves");

        Fixture first;
        first.reset();
        Fixture second;
        second.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(first.state
#if SH_FEATURE_FOG_CHART
                                                      ,
                                                      first.chart
#endif
                                                      ,
                                                      storage),
                         voyage_save::kLoaded, "first Continue");
        test.assertEqual(voyage_flow::loadCheckpoint(second.state
#if SH_FEATURE_FOG_CHART
                                                       ,
                                                       second.chart
#endif
                                                       ,
                                                       storage),
                         voyage_save::kLoaded, "second Continue");
        test.assertTrue(sameOffers(first.state, second.state), "same board on every Continue");
        test.assertEqual(first.state.offerCount, game::kJobOffers, "full offer board regenerated");
        test.assertEqual(first.state.offerCursor, 4242, "saved cursor is unchanged by regeneration");
        test.assertEqual(first.state.offerSerial, second.state.offerSerial, "working serial is deterministic");
        runner.add(test);
    }

    {
        Test test("free rescue restores the checkpoint, counts once and commits");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kBenthos);
        source.state.credits = 95;
        source.state.cargo = game::kJobCrates;
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));
        uint8_t routeReward = 0;
        uint16_t routeDeadline = 0;
        test.assertTrue(world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, routeDeadline), "route exists");
        source.state.activeJob.kind = game::kJobKindOrdinary;
        source.state.activeJob.mission = game::kMissionNone;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.activeJob.reward = routeReward;
        source.state.activeJob.deadline = 0;
        source.state.activeJob.elapsed = 0;
        source.state.rescues = 0;
        test.assertEqual(source.save(storage), voyage_save::kSaved, "rescue-free checkpoint saves");

        Fixture live = source;
        live.state.air = 0;
        live.state.credits = 999;
        live.state.rescues = 0;
        const voyage_flow::Recovery recovery = voyage_flow::recoverCheckpoint(live.state
#if SH_FEATURE_FOG_CHART
                                                                              ,
                                                                              live.chart
#endif
                                                                              ,
                                                                              storage);
        test.assertEqual(recovery.load, voyage_save::kLoaded, "zero-air rescue finds the checkpoint");
        test.assertEqual(recovery.save, voyage_save::kSaved, "rescue counter commits");
        test.assertEqual(live.state.air, game::airCapacity(live.state), "zero air becomes full air");
        test.assertEqual(live.state.credits, 95, "unsaved credit change rolls back");
        test.assertEqual(live.state.cargo, game::kJobCrates, "cargo survives rescue");
        test.assertEqual(live.state.rescues, 1, "rescue count increments once");

        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     reloaded.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "committed rescue reloads");
        test.assertEqual(reloaded.state.rescues, 1, "committed rescue count persists");
        test.assertEqual(reloaded.state.credits, 95, "rescue does not duplicate payment");
        runner.add(test);
    }

    {
        Test test("Continue at Pump Refuge restores the generated berth and loaded cargo");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(0, berth), "generated Pump Refuge berth");
        uint8_t routeReward = 0;
        uint16_t routeDeadline = 0;
        test.assertTrue(world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, routeDeadline), "ordinary route exists");

        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kPumpRefuge);
        source.state.campaign = campaign::kMilestonePump;
        source.state.activeJob.kind = game::kJobKindOrdinary;
        source.state.activeJob.mission = game::kMissionNone;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.activeJob.reward = routeReward;
        source.state.activeJob.deadline = 0;
        source.state.activeJob.elapsed = 0;
        source.state.cargo = game::kJobCrates;
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));
        test.assertTrue(campaign::activeJobValid(source.state), "service-berth cargo checkpoint is semantically valid");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "Pump Refuge cargo checkpoint saves");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     restored.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads the service-berth checkpoint");
        test.assertEqual(restored.state.docked, campaign::kPumpRefuge, "Pump Refuge id restored");
        test.assertEqual(restored.state.x, berth.pixel_x, "generated service berth x restored");
        test.assertEqual(restored.state.y, berth.pixel_y, "generated service berth y restored");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo survives the service berth");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindOrdinary, "active cargo job restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "service berth restores full air");
        test.assertEqual(restored.state.vxQ8, 0, "transient velocity resets");
        test.assertEqual(restored.state.offerCount, 0, "service berth has no freight board");
        runner.add(test);
    }

    {
        Test test("failed rescue commit keeps the restored state for an honest retry");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kBenthos);
        source.state.credits = 77;
        test.assertEqual(source.save(storage), voyage_save::kSaved, "baseline rescue checkpoint");

        Fixture live;
        live.reset();
        storage.failedAt = 5;
        const voyage_flow::Recovery failed = voyage_flow::recoverCheckpoint(live.state
#if SH_FEATURE_FOG_CHART
                                                                                ,
                                                                                live.chart
#endif
                                                                                ,
                                                                                storage);
        test.assertEqual(failed.load, voyage_save::kLoaded, "recovery load still succeeds");
        test.assertEqual(failed.save, voyage_save::kWriteFailed, "failed rescue commit is reported");
        test.assertEqual(live.state.credits, 77, "restored credits remain live after failed commit");
        test.assertEqual(live.state.rescues, 1, "rescue counter remains pending for retry");

        storage.failed = false;
        storage.failedAt = kNoFault;
        test.assertEqual(voyage_flow::saveCheckpoint(live.state
#if SH_FEATURE_FOG_CHART
                                                      ,
                                                      live.chart
#endif
                                                      ,
                                                      storage),
                         voyage_save::kSaved, "retry commits the recovered checkpoint");
        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     reloaded.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "retried rescue loads");
        test.assertEqual(reloaded.state.credits, 77, "retry does not alter credits");
        test.assertEqual(reloaded.state.rescues, 1, "retry commits the rescue count once");
        runner.add(test);
    }

    {
        Test test("failed checkpoint write can be retried and never moves midwater state");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.state.docked = -1;
        test.assertEqual(voyage_flow::saveCheckpoint(fixture.state
#if SH_FEATURE_FOG_CHART
                                                          ,
                                                          fixture.chart
#endif
                                                          ,
                                                          storage),
                         voyage_save::kInvalidState, "midwater snapshot is rejected before writing");
        test.assertEqual(storage.updateCalls, 0, "rejected snapshot performs no EEPROM update");

        fixture.state.docked = static_cast<int8_t>(campaign::kTidewall);
        storage.failedAt = 7;
        test.assertEqual(voyage_flow::saveCheckpoint(fixture.state
#if SH_FEATURE_FOG_CHART
                                                          ,
                                                          fixture.chart
#endif
                                                          ,
                                                          storage),
                         voyage_save::kWriteFailed, "injected failure is reported honestly");
        storage.failed = false;
        storage.failedAt = kNoFault;
        test.assertEqual(voyage_flow::saveCheckpoint(fixture.state
#if SH_FEATURE_FOG_CHART
                                                          ,
                                                          fixture.chart
#endif
                                                          ,
                                                          storage),
                         voyage_save::kSaved, "explicit retry succeeds");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     restored.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "retried checkpoint loads");
        runner.add(test);
    }
}

#else

inline void VoyageFlowSuite(TestRunner &runner) {
    Test test("voyage flow compiles out when SAVE_VOYAGE=0");
    test.assertEqual(SH_FEATURE_SAVE_VOYAGE, 0, "feature is off in this build");
    runner.add(test);
}

#endif   // SH_FEATURE_SAVE_VOYAGE
