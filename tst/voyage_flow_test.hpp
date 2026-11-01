#pragma once

// Native integration tests for checkpoint application (sub-hauler-7px.4).
// These exercise the same voyage_flow policy the sketch calls: boot
// classification, Continue restoration, loaded campaign contracts, rescue
// commits and failed-write retry. Storage fault injection stays in
// voyage_save_test's FakeStorage so there is one cart-save fixture.

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

inline bool makeFirstAtIntermediate(Fixture &fixture) {
    fixture.reset();
    if (!campaign::accept(fixture.state, campaign::kFirst))
        return false;
    fixture.state.docked = static_cast<int8_t>(campaign::kCormorant);
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
        Test test("Continue restores the intermediate berth and loaded campaign contract");
        Fixture source;
        test.assertTrue(makeFirstAtIntermediate(source), "valid loaded checkpoint at Cormorant");
        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "intermediate checkpoint saves");

        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
                                                     ,
                                                     restored.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads");
        test.assertEqual(restored.state.docked, campaign::kCormorant, "intermediate berth restored");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo remains active");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindCampaign, "campaign kind restored");
        test.assertEqual(restored.state.activeJob.mission, campaign::kFirst, "mission restored");
        test.assertEqual(restored.state.activeJob.to, campaign::kBenthos, "destination restored");
        test.assertEqual(restored.state.x, game::cameraPixelX(restored.state) + 44, "intermediate berth x feeds the camera");
        test.assertEqual(restored.state.cameraXQ4, game::cameraTargetXQ4(restored.state), "intermediate camera x is snapped");
        test.assertEqual(restored.state.cameraYQ4, game::cameraTargetYQ4(restored.state), "intermediate camera y is snapped");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "berth restore refills air");
        test.assertEqual(restored.state.vxQ8, 0, "transient velocity resets");
        test.assertEqual(restored.state.vyQ8, 0, "transient vertical velocity resets");
        runner.add(test);
    }

    {
        Test test("free rescue restores the checkpoint, counts once and commits");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        source.state.delivered = 95;
        test.assertTrue(campaign::accept(source.state, campaign::kFirst), "loaded campaign baseline");
        source.state.docked = static_cast<int8_t>(campaign::kCormorant);
        source.state.rescues = 0;
        test.assertEqual(source.save(storage), voyage_save::kSaved, "rescue-free checkpoint saves");

        Fixture live = source;
        live.state.air = 0;
        live.state.delivered = 999;
        live.state.rescues = 0;
        const voyage_flow::Recovery recovery = voyage_flow::recoverCheckpoint(live.state
                                                                              ,
                                                                              live.chart
                                                                              ,
                                                                              storage);
        test.assertEqual(recovery.load, voyage_save::kLoaded, "zero-air rescue finds the checkpoint");
        test.assertEqual(recovery.save, voyage_save::kSaved, "rescue counter commits");
        test.assertEqual(live.state.air, game::airCapacity(live.state), "zero air becomes full air");
        test.assertEqual(live.state.delivered, 95, "unsaved delivery change rolls back");
        test.assertEqual(live.state.cargo, game::kJobCrates, "cargo survives rescue");
        test.assertEqual(live.state.rescues, 1, "rescue count increments once");

        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
                                                     ,
                                                     reloaded.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "committed rescue reloads");
        test.assertEqual(reloaded.state.rescues, 1, "committed rescue count persists");
        test.assertEqual(reloaded.state.delivered, 95, "rescue does not duplicate a delivery");
        runner.add(test);
    }

    {
        Test test("Continue at Pump Refuge restores the generated berth and loaded cargo");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(0, berth), "generated Pump Refuge berth");

        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kPumpRefuge);
        source.state.campaign = campaign::kMilestonePump;
        source.state.activeJob.kind = game::kJobKindCampaign;
        source.state.activeJob.mission = campaign::kFirst;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.cargo = game::kJobCrates;
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));
        test.assertTrue(campaign::activeJobValid(source.state), "service-berth cargo checkpoint is semantically valid");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "Pump Refuge cargo checkpoint saves");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
                                                     ,
                                                     restored.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads the service-berth checkpoint");
        test.assertEqual(restored.state.docked, campaign::kPumpRefuge, "Pump Refuge id restored");
        test.assertEqual(restored.state.x, berth.pixel_x, "generated service berth x restored");
        test.assertEqual(restored.state.y, berth.pixel_y, "generated service berth y restored");
        test.assertEqual(restored.state.cameraXQ4, game::cameraTargetXQ4(restored.state), "service berth camera x is snapped");
        test.assertEqual(restored.state.cameraYQ4, game::cameraTargetYQ4(restored.state), "service berth camera y is snapped");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo survives the service berth");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindCampaign, "campaign job restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "service berth restores full air");
        test.assertEqual(restored.state.vxQ8, 0, "transient velocity resets");
        runner.add(test);
    }

    {
        Test test("Continue at Lower Gantry restores its loaded service checkpoint");
        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(1, berth), "generated Lower Gantry berth");

        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kLowerGantry);
        source.state.campaign = campaign::kMilestoneLift;
        source.state.activeJob.kind = game::kJobKindCampaign;
        source.state.activeJob.mission = campaign::kFirst;
        source.state.activeJob.from = campaign::kTidewall;
        source.state.activeJob.to = campaign::kBenthos;
        source.state.activeJob.crates = game::kJobCrates;
        source.state.cargo = game::kJobCrates;
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));
        test.assertTrue(campaign::activeJobValid(source.state), "Lower Gantry cargo checkpoint is semantically valid");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "Lower Gantry cargo checkpoint saves");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
                                                     ,
                                                     restored.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "Continue loads the Lower Gantry checkpoint");
        test.assertEqual(restored.state.docked, campaign::kLowerGantry, "Lower Gantry id restored");
        test.assertEqual(restored.state.x, berth.pixel_x, "generated Lower Gantry x restored");
        test.assertEqual(restored.state.y, berth.pixel_y, "generated Lower Gantry y restored");
        test.assertEqual(restored.state.cameraXQ4, game::cameraTargetXQ4(restored.state), "Lower Gantry camera x is snapped");
        test.assertEqual(restored.state.cameraYQ4, game::cameraTargetYQ4(restored.state), "Lower Gantry camera y is snapped");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "loaded cargo survives the service berth");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindCampaign, "campaign job restored");
        test.assertEqual(restored.state.air, game::airCapacity(restored.state), "service berth restores full air");
        runner.add(test);
    }

    {
        Test test("HOME completion and ending presentation persist without a second payment");
        Fixture source;
        setValidPayload(source);
        source.state.docked = static_cast<int8_t>(campaign::kTidewall);
        source.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
        source.state.ending = 0;
        source.state.delivered = 5;
        source.state.activeJob.kind = game::kJobKindNone;
        source.state.activeJob.mission = campaign::kMissionNone;
        source.state.activeJob.from = game::kJobNone;
        source.state.activeJob.to = game::kJobNone;
        source.state.activeJob.crates = 0;
        source.state.cargo = 0;
        test.assertTrue(campaign::stateValid(source.state), "post-HOME checkpoint is valid");
        test.assertEqual(campaign::workMissionAt(source.state, campaign::kTidewall, 0), campaign::kWorkEnding, "VIEW ENDING is available");

        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "HOME checkpoint commits before cards");
        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
                                                     ,
                                                     reloaded.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "reload between HOME and cards succeeds");
        test.assertEqual(reloaded.state.ending, 0, "cards have not been marked yet");
        test.assertEqual(reloaded.state.delivered, 5, "five deliveries are recorded");
        test.assertTrue(campaign::markEndingPresented(reloaded.state), "cards can be marked after reload");
        test.assertEqual(reloaded.state.ending, 1, "ending flag set");
        test.assertEqual(reloaded.save(storage), voyage_save::kSaved, "ending flag commits");
        test.assertEqual(reloaded.save(storage), voyage_save::kSaved, "repeat save remains idempotent");

        Fixture postgame;
        postgame.reset();
        test.assertEqual(postgame.load(storage), voyage_save::kLoaded, "postgame checkpoint loads");
        test.assertEqual(postgame.state.ending, 1, "ending presentation persists");
        test.assertEqual(postgame.state.delivered, 5, "postgame keeps the delivery count");
        test.assertEqual(campaign::workMissionAt(postgame.state, campaign::kTidewall, 0), campaign::kWorkEnding, "ending remains viewable");
        test.assertEqual(jobs::deliver(postgame.state, campaign::kTidewall), jobs::kDeliveryNone, "postgame docking cannot complete HOME again");
        test.assertTrue(!campaign::completeMission(postgame.state, campaign::kHome), "HOME cannot complete twice");
        test.assertEqual(postgame.state.delivered, 5, "replay never counts twice");
        test.assertTrue(postgame.state.docked == static_cast<int8_t>(campaign::kTidewall), "postgame continues at Tidewall");
        runner.add(test);
    }

    {
        Test test("failed rescue commit keeps the restored state for an honest retry");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        source.state.docked = static_cast<int8_t>(campaign::kBenthos);
        source.state.delivered = 77;
        test.assertEqual(source.save(storage), voyage_save::kSaved, "baseline rescue checkpoint");

        Fixture live;
        live.reset();
        storage.failedAt = 5;
        const voyage_flow::Recovery failed = voyage_flow::recoverCheckpoint(live.state
                                                                            ,
                                                                            live.chart
                                                                            ,
                                                                            storage);
        test.assertEqual(failed.load, voyage_save::kLoaded, "recovery load still succeeds");
        test.assertEqual(failed.save, voyage_save::kWriteFailed, "failed rescue commit is reported");
        test.assertEqual(live.state.delivered, 77, "restored delivery count remains live after failed commit");
        test.assertEqual(live.state.rescues, 1, "rescue counter remains pending for retry");

        storage.failed = false;
        storage.failedAt = kNoFault;
        test.assertEqual(voyage_flow::saveCheckpoint(live.state
                                                     ,
                                                     live.chart
                                                     ,
                                                     storage),
                         voyage_save::kSaved, "retry commits the recovered checkpoint");
        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state
                                                     ,
                                                     reloaded.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "retried rescue loads");
        test.assertEqual(reloaded.state.delivered, 77, "retry does not alter the delivery count");
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
                                                                            ,
                                                                            live.chart
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
                                                     ,
                                                     live.chart
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
                                                     ,
                                                     fixture.chart
                                                     ,
                                                     storage),
                         voyage_save::kInvalidState, "midwater snapshot is rejected before writing");
        test.assertEqual(storage.pageBytes, 0, "rejected snapshot performs no flash page program");

        fixture.state.docked = static_cast<int8_t>(campaign::kTidewall);
        storage.failedAt = 7;
        test.assertEqual(voyage_flow::saveCheckpoint(fixture.state
                                                     ,
                                                     fixture.chart
                                                     ,
                                                     storage),
                         voyage_save::kWriteFailed, "injected failure is reported honestly");
        storage.failed = false;
        storage.failedAt = kNoFault;
        test.assertEqual(voyage_flow::saveCheckpoint(fixture.state
                                                     ,
                                                     fixture.chart
                                                     ,
                                                     storage),
                         voyage_save::kSaved, "explicit retry succeeds");
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state
                                                     ,
                                                     restored.chart
                                                     ,
                                                     storage),
                         voyage_save::kLoaded, "retried checkpoint loads");
        runner.add(test);
    }
}

