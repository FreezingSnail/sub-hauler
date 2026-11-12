#pragma once

// Native integration tests for checkpoint application.
// These exercise the same voyage_flow policy the sketch calls: boot
// classification, Continue restoration, loaded campaign contracts and
// load-only rescue recovery. Storage stays in voyage_save_test's FakeStorage
// so there is one cart-save fixture.

#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/voyage_flow.hpp"
#include "../src/world.hpp"
#include "test.hpp"
#include "voyage_save_test.hpp"
#include "world_test.hpp"

namespace voyage_flow_test {

using voyage_save_test::FakeStorage;
using voyage_save_test::Fixture;
using voyage_save_test::setValidPayload;

inline bool makeFirstAtIntermediate(Fixture &fixture) {
    fixture.reset();
    if (!campaign::accept(fixture.state, campaign::kFirst))
        return false;
    fixture.state.docked = static_cast<int8_t>(campaign::kCormorant);
    return true;
}

}   // namespace voyage_flow_test

inline void VoyageFlowSuite(TestRunner &runner) {
    using namespace voyage_flow_test;

    {
        Test test("boot distinguishes blank from written saves");
        FakeStorage blank(0xFF);
        test.assertEqual(voyage_flow::inspectBoot(blank), voyage_flow::kBootBlank, "blank save partition");
        test.assertEqual(voyage_flow::kBootBlank, 0, "blank maps to title frame row 0");
        test.assertEqual(voyage_flow::kBootWritten, 1, "written maps to title frame row 1");

        FakeStorage storage(0xFF);
        Fixture saved;
        setValidPayload(saved);
        saved.save(storage);
        test.assertEqual(voyage_flow::inspectBoot(storage), voyage_flow::kBootWritten, "written save partition");
        runner.add(test);
    }

    {
        Test test("every explicit save erases once and programs two pages");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        storage.resetCounters();
        voyage_flow::saveCheckpoint(fixture.state, fixture.chart, storage);
        test.assertEqual(storage.eraseOps, 1, "first save erases");
        test.assertEqual(storage.pageOps, 2, "first save programs two pages");

        storage.resetCounters();
        voyage_flow::saveCheckpoint(fixture.state, fixture.chart, storage);
        test.assertEqual(storage.eraseOps, 1, "identical save erases again");
        test.assertEqual(storage.pageOps, 2, "identical save programs both pages again");
        test.assertTrue(voyage_save::hasSave(storage), "written record stays written");
        runner.add(test);
    }

    {
        Test test("a new voyage enters free flight beside the home gantry");
        FakeStorage storage(0xFF);
        Fixture fresh;
        voyage_flow::startNewVoyage(fresh.state, fresh.chart, storage);
        test.assertEqual(fresh.state.docked, static_cast<int8_t>(-1), "new voyage starts undocked");
        test.assertEqual(fresh.state.pendingStory, voyage_save::kPendingStoryNone, "opening deck is never written to the new checkpoint");
        test.assertEqual(fresh.state.x, world::SPAWN_SUB_X, "new voyage spawn x");
        test.assertEqual(fresh.state.y, world::SPAWN_SUB_Y, "new voyage spawn y");
        test.assertEqual(fresh.state.cameraXQ4, game::cameraTargetXQ4(fresh.state), "spawn camera x is snapped");
        test.assertEqual(fresh.state.cameraYQ4, game::cameraTargetYQ4(fresh.state), "spawn camera y is snapped");

        uint8_t nearby = 0;
        test.assertTrue(!station::nearestDock(fresh.state, nearby), "spawn is outside the dock ring");
        world::Settlement home;
        test.assertTrue(world::settlementInfo(campaign::kTidewall, home), "home settlement metadata");
        test.assertTrue(fresh.state.x < home.pixel_x, "spawn sits left of the gantry");
        test.assertTrue(home.pixel_x - fresh.state.x <= 2 * station::kDockRadius, "spawn stays beside the gantry");

        world_test::FakeWorld fake;
        test.assertTrue(!station::stepFlight(fresh.state, game::Input{false, false, false, false}, fake), "an idle spawn never docks");
        test.assertEqual(fresh.state.docked, static_cast<int8_t>(-1), "spawn remains in free flight");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "Continue loads the new voyage checkpoint");
        test.assertEqual(restored.state.pendingStory, voyage_save::kPendingStoryNone, "Continue never replays the RAM-only opening deck");
        test.assertEqual(restored.state.docked, campaign::kTidewall, "Continue restores the Tidewall berth");
        test.assertEqual(restored.state.x, home.pixel_x, "Continue restores the berth x");
        test.assertEqual(restored.state.y, home.pixel_y, "Continue restores the berth y");
        runner.add(test);
    }

    {
        Test test("Continue restores the intermediate berth and loaded campaign contract");
        Fixture source;
        test.assertTrue(makeFirstAtIntermediate(source), "valid loaded checkpoint at Cormorant");
        FakeStorage storage(0xFF);
        source.save(storage);

        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state, restored.chart, storage), voyage_save::kLoaded, "Continue loads");
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
        Test test("free rescue restores the checkpoint without writing");
        FakeStorage storage(0xFF);
        Fixture source;
        source.reset();
        test.assertTrue(campaign::accept(source.state, campaign::kFirst), "loaded campaign baseline");
        source.state.docked = static_cast<int8_t>(campaign::kCormorant);
        source.state.campaign = campaign::kMilestoneFirst;
        source.state.tools = exploration_data::CUTTER;
        source.state.pendingStory = story_view::kCardFirst;
        source.save(storage);

        Fixture live = source;
        live.state.air = 0;
        live.state.campaign = 0;
        live.state.tools = 0;
        live.state.pendingStory = story_view::kCardNone;
        storage.resetCounters();
        const voyage_save::LoadResult recovery = voyage_flow::recoverCheckpoint(live.state, live.chart, storage);
        test.assertEqual(recovery, voyage_save::kLoaded, "zero-air rescue finds the checkpoint");
        test.assertEqual(storage.eraseOps, 0, "rescue never erases");
        test.assertEqual(storage.pageOps, 0, "rescue never programs a page");
        test.assertEqual(live.state.air, game::airCapacity(live.state), "zero air becomes full air");
        test.assertEqual(live.state.campaign, campaign::kMilestoneFirst, "unsaved campaign change rolls back");
        test.assertEqual(live.state.tools, exploration_data::CUTTER, "unsaved tool change rolls back");
        test.assertEqual(live.state.pendingStory, story_view::kCardFirst, "pending deck rolls back exactly");
        test.assertEqual(live.state.cargo, game::kJobCrates, "cargo survives rescue");

        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state, reloaded.chart, storage), voyage_save::kLoaded, "checkpoint still loads");
        test.assertEqual(reloaded.state.campaign, campaign::kMilestoneFirst, "rescue did not rewrite the checkpoint");
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
        game::setCargo(source.state, game::kJobCrates);
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));

        FakeStorage storage(0xFF);
        source.save(storage);
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state, restored.chart, storage), voyage_save::kLoaded, "Continue loads the service-berth checkpoint");
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
        game::setCargo(source.state, game::kJobCrates);
        source.state.ballast = static_cast<uint8_t>(game::neutralFor(source.state.cargo));

        FakeStorage storage(0xFF);
        source.save(storage);
        Fixture restored;
        restored.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(restored.state, restored.chart, storage), voyage_save::kLoaded, "Continue loads the Lower Gantry checkpoint");
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
        source.state.activeJob.kind = game::kJobKindNone;
        source.state.activeJob.mission = campaign::kMissionNone;
        source.state.activeJob.from = game::kJobNone;
        source.state.activeJob.to = game::kJobNone;
        source.state.activeJob.crates = 0;
        source.state.cargo = 0;
        test.assertEqual(campaign::workMissionAt(source.state, campaign::kTidewall, 0), campaign::kWorkEnding, "VIEW ENDING is available");

        FakeStorage storage(0xFF);
        source.save(storage);
        Fixture reloaded;
        reloaded.reset();
        test.assertEqual(voyage_flow::loadCheckpoint(reloaded.state, reloaded.chart, storage), voyage_save::kLoaded, "reload between HOME and cards succeeds");
        test.assertEqual(reloaded.state.ending, 0, "cards have not been marked yet");
        test.assertTrue(campaign::markEndingPresented(reloaded.state), "cards can be marked after reload");
        test.assertEqual(reloaded.state.ending, 1, "ending flag set");
        storage.resetCounters();
        reloaded.save(storage);
        test.assertEqual(storage.eraseOps, 1, "ending flag commits with a normal save");
        storage.resetCounters();
        reloaded.save(storage);
        test.assertEqual(storage.eraseOps, 1, "repeat save still erases");
        test.assertEqual(storage.pageOps, 2, "repeat save still programs both pages");

        Fixture postgame;
        postgame.reset();
        test.assertEqual(postgame.load(storage), voyage_save::kLoaded, "postgame checkpoint loads");
        test.assertEqual(postgame.state.ending, 1, "ending presentation persists");
        test.assertEqual(campaign::workMissionAt(postgame.state, campaign::kTidewall, 0), campaign::kWorkEnding, "ending remains viewable");
        test.assertTrue(!jobs::deliver(postgame.state, campaign::kTidewall), "postgame docking cannot complete HOME again");
        test.assertTrue(!campaign::completeMission(postgame.state, campaign::kHome), "HOME cannot complete twice");
        test.assertTrue(postgame.state.docked == static_cast<int8_t>(campaign::kTidewall), "postgame continues at Tidewall");
        runner.add(test);
    }
}
