#pragma once

// Native integration tests for checkpoint application (sub-hauler-7px.4).
// These exercise the same voyage_flow policy the sketch calls: boot
// classification, Continue restoration, deterministic offers, loaded-freight
// preservation, rescue commits and failed-write retry. Storage fault injection
// stays in voyage_save_test's FakeStorage so there is one cart-save fixture.

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
        if (a.offers[i].from != b.offers[i].from || a.offers[i].to != b.offers[i].to || a.offers[i].crates != b.offers[i].crates || a.offers[i].reward != b.offers[i].reward)
            return false;
#if SH_FEATURE_CAMPAIGN
        if (a.offers[i].kind != b.offers[i].kind)
            return false;
#endif
    }
    return true;
}

inline bool makeFreightAtIntermediate(Fixture &fixture) {
    fixture.reset();
    fixture.state.docked = static_cast<int8_t>(campaign::kCormorant);
    fixture.state.cargo = game::kJobCrates;
    fixture.state.ballast = static_cast<uint8_t>(game::neutralFor(fixture.state.cargo));
    fixture.state.offerCursor = 0x0123;
    fixture.state.offerSerial = 0x0125;
    uint8_t routeReward = 0;
    uint16_t unusedDeadline = 0;
    if (!world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, unusedDeadline))
        return false;
    fixture.state.activeJob.kind = game::kJobKindOrdinary;
    fixture.state.activeJob.mission = game::kMissionNone;
    fixture.state.activeJob.from = campaign::kTidewall;
    fixture.state.activeJob.to = campaign::kBenthos;
    fixture.state.activeJob.crates = game::kJobCrates;
    fixture.state.activeJob.reward = routeReward;
    return campaign::activeJobValid(fixture.state);
}

}   // namespace voyage_flow_test

inline void VoyageFlowSuite(TestRunner &runner) {
    using namespace voyage_flow_test;

    {
        Test test("boot distinguishes blank, valid, corrupt and incompatible saves");
        FakeStorage blank(0xFF);
        voyage_flow::BootStatus status = voyage_flow::inspectBoot(blank);
        test.assertEqual(status.state, voyage_flow::kBootBlank, "blank save partition");
        test.assertEqual(status.hasValid, 0, "blank has no Continue");
        test.assertEqual(status.needsReplaceConfirm(), 0, "blank starts without replacement confirmation");

        FakeStorage storage(0xFF);
        Fixture saved;
        setValidPayload(saved);
        test.assertEqual(saved.save(storage), voyage_save::kSaved, "baseline voyage");
        status = voyage_flow::inspectBoot(storage);
        test.assertEqual(status.state, voyage_flow::kBootValid, "valid save partition");
        test.assertEqual(status.hasValid, 1, "valid offers Continue");

        FakeStorage corrupt = storage;
        corrupt.bytes[voyage_save::kSlotOffset] ^= 0xFF;
        status = voyage_flow::inspectBoot(corrupt);
        test.assertEqual(status.state, voyage_flow::kBootCorrupt, "torn magic is corrupt");
        test.assertEqual(status.hasValid, 0, "corrupt is not Continue");
        test.assertEqual(status.needsReplaceConfirm(), 0, "corrupt starts a clean New Voyage");

        FakeStorage incompatible = storage;
        incompatible.bytes[voyage_save::kSlotOffset + voyage_save::kSignatureOffset] ^= 0x01;
        status = voyage_flow::inspectBoot(incompatible);
        test.assertEqual(status.state, voyage_flow::kBootIncompatible, "profile mismatch is incompatible");
        test.assertEqual(status.hasIncompatible, 1, "incompatible is reported");
        test.assertEqual(status.needsReplaceConfirm(), 0, "incompatible starts a clean New Voyage");
        runner.add(test);
    }

    {
        Test test("Continue restores the intermediate berth and loaded freight");
        Fixture source;
        test.assertTrue(makeFreightAtIntermediate(source), "valid loaded checkpoint at Cormorant");
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
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindOrdinary, "ordinary kind restored");
        test.assertEqual(restored.state.activeJob.reward, source.state.activeJob.reward, "freight reward restored");
        test.assertEqual(restored.state.offerCursor, 0x0123, "saved offer cursor restored");
        test.assertEqual(restored.state.x, game::cameraTargetX(restored.state) + 44, "intermediate berth x feeds the camera");
        test.assertEqual(restored.state.cameraX, game::cameraTargetX(restored.state), "intermediate camera x is snapped");
        test.assertEqual(restored.state.cameraY, game::cameraTargetY(restored.state), "intermediate camera y is snapped");
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
        uint16_t unusedDeadline = 0;
        test.assertTrue(world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, unusedDeadline), "route exists");
        source.state.activeJob.kind = game::kJobKindOrdinary;
        source.state.activeJob.mission = game::kMissionNone;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.activeJob.reward = routeReward;
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
        uint16_t unusedDeadline = 0;
        test.assertTrue(world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, unusedDeadline), "ordinary route exists");

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
        test.assertEqual(restored.state.cameraX, game::cameraTargetX(restored.state), "service berth camera x is snapped");
        test.assertEqual(restored.state.cameraY, game::cameraTargetY(restored.state), "service berth camera y is snapped");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo survives the service berth");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindOrdinary, "active cargo job restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "service berth restores full air");
        test.assertEqual(restored.state.vxQ8, 0, "transient velocity resets");
        test.assertEqual(restored.state.offerCount, 0, "service berth has no freight board");
        runner.add(test);
    }

    {
        Test test("Continue at Lower Gantry restores its loaded service checkpoint");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(1, berth), "generated Lower Gantry berth");
        uint8_t routeReward = 0;
        uint16_t unusedDeadline = 0;
        test.assertTrue(world::routeEconomy(campaign::kTidewall, campaign::kBenthos, routeReward, unusedDeadline), "ordinary route exists");

        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kLowerGantry);
        source.state.campaign = campaign::kMilestoneLift;
        source.state.activeJob.kind = game::kJobKindOrdinary;
        source.state.activeJob.mission = game::kMissionNone;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.activeJob.reward = routeReward;
        source.state.cargo = game::kJobCrates;
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));
        test.assertTrue(campaign::activeJobValid(source.state), "Lower Gantry cargo checkpoint is semantically valid");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "Lower Gantry cargo checkpoint saves");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     restored.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads the Lower Gantry checkpoint");
        test.assertEqual(restored.state.docked, campaign::kLowerGantry, "Lower Gantry id restored");
        test.assertEqual(restored.state.x, berth.pixel_x, "generated Lower Gantry x restored");
        test.assertEqual(restored.state.y, berth.pixel_y, "generated Lower Gantry y restored");
        test.assertEqual(restored.state.cameraX, game::cameraTargetX(restored.state), "Lower Gantry camera x is snapped");
        test.assertEqual(restored.state.cameraY, game::cameraTargetY(restored.state), "Lower Gantry camera y is snapped");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo survives the service berth");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindOrdinary, "active cargo job restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "service berth restores full air");
        test.assertEqual(restored.state.offerCount, 0, "Lower Gantry has no freight board");
        runner.add(test);
    }

    {
        Test test("HOME completion and ending presentation persist without a second payment");
        Fixture source;
        setValidPayload(source);
        source.state.docked = static_cast<int8_t>(campaign::kTidewall);
        source.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
        source.state.ending = 0;
        source.state.credits = 590;
        source.state.delivered = 5;
        source.state.activeJob.kind = game::kJobKindNone;
        source.state.activeJob.mission = campaign::kMissionNone;
        source.state.activeJob.from = game::kJobNone;
        source.state.activeJob.to = game::kJobNone;
        source.state.activeJob.crates = 0;
        source.state.activeJob.reward = 0;
        source.state.cargo = 0;
        test.assertTrue(campaign::stateValid(source.state), "post-HOME checkpoint is valid");
        test.assertEqual(campaign::workMissionAt(source.state, campaign::kTidewall, 0), campaign::kWorkEnding, "VIEW ENDING is available");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "HOME checkpoint commits before cards");
        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     reloaded.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "reload between HOME and cards succeeds");
        test.assertEqual(reloaded.state.ending, 0, "cards have not been marked yet");
        test.assertEqual(reloaded.state.credits, 590, "HOME payment is committed exactly once");
        test.assertEqual(reloaded.state.delivered, 5, "five deliveries are recorded");
        test.assertTrue(campaign::markEndingPresented(reloaded.state), "cards can be marked after reload");
        test.assertEqual(reloaded.state.ending, 1, "ending flag set");
        test.assertEqual(reloaded.state.credits, 590, "marking the ending pays nothing");
        test.assertEqual(reloaded.save(storage), voyage_save::kSaved, "ending flag commits");
        test.assertEqual(reloaded.save(storage), voyage_save::kSaved, "repeat save remains idempotent");

        Fixture postgame;
        postgame.reset();
        test.assertEqual(postgame.load(storage), voyage_save::kLoaded, "postgame checkpoint loads");
        test.assertEqual(postgame.state.ending, 1, "ending presentation persists");
        test.assertEqual(postgame.state.credits, 590, "postgame keeps the same credits");
        test.assertEqual(campaign::workMissionAt(postgame.state, campaign::kTidewall, 0), campaign::kWorkEnding, "ending remains viewable");
        test.assertEqual(jobs::deliver(postgame.state, campaign::kTidewall), jobs::kDeliveryNone, "postgame docking cannot pay HOME again");
        test.assertTrue(!campaign::completeMission(postgame.state, campaign::kHome), "HOME cannot complete twice");
        test.assertEqual(postgame.state.credits, 590, "replay never pays twice");
        test.assertTrue(postgame.state.docked == static_cast<int8_t>(campaign::kTidewall), "postgame continues at Tidewall");
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
        Test test("failed recovery save preserves a pending ending");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kTidewall);
        source.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
        source.state.ending = 0;
        test.assertTrue(campaign::missionCompleted(source.state, campaign::kHome), "HOME complete");
        test.assertEqual(source.save(storage), voyage_save::kSaved, "pre-ending checkpoint saves");

        Fixture live;
        live.reset();
        storage.resetCounters();
        storage.failErase = true;
        const voyage_flow::Recovery failed = voyage_flow::recoverCheckpoint(live.state
#if SH_FEATURE_FOG_CHART
                                                                            ,
                                                                            live.chart
#endif
                                                                            ,
                                                                            storage);
        test.assertEqual(failed.load, voyage_save::kLoaded, "recovery loads before save");
        test.assertEqual(failed.save, voyage_save::kWriteFailed, "recovery save failure is reported");
        test.assertEqual(live.state.ending, 0, "pending ending is not cleared by a failed save");
        test.assertTrue(campaign::missionCompleted(live.state, campaign::kHome), "HOME remains complete for the retry");

        storage.failErase = false;
        storage.failed = false;
        storage.failedAt = kNoFault;
        test.assertEqual(voyage_flow::saveCheckpoint(live.state
#if SH_FEATURE_FOG_CHART
                                                     ,
                                                     live.chart
#endif
                                                     ,
                                                     storage),
                         voyage_save::kSaved, "retry commits the pending-ending checkpoint");
        test.assertEqual(live.state.ending, 0, "ending is still pending after the retry");
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
        test.assertEqual(storage.pageBytes, 0, "rejected snapshot performs no flash page program");

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
