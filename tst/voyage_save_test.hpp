#pragma once

// Native storage tests for the raw single-slot voyage checkpoint.
//
// The fixture models one 4 KiB external-NOR erase block: erased state 0xFF and
// page programs can only clear bits. The payload starts at offset 0 with no
// header, CRC, commit or cache; every save erases once and programs two pages.

#include <fstream>

#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "../src/voyage_save.hpp"
#include "../src/story_view.hpp"
#include "test.hpp"


#include "../src/campaign.hpp"
#include "../src/game.hpp"
#include "../src/jobs.hpp"
#include "../src/station.hpp"
#include "../src/world.hpp"
#include "jobs_test.hpp"
#include "world_test.hpp"

namespace voyage_save_test {

using game::State;
using jobs_test::dockAt;
using jobs_test::idle;
using jobs_test::travelTo;

constexpr uint16_t kSaveBytes = voyage_save::kSaveBytes;

// The fixture models the external NOR save block: erased state 0xFF, page
// programs can only clear bits, and a 4 KiB block erase restores 0xFF.
struct FakeStorage {
    uint8_t bytes[kSaveBytes];
    uint16_t reads;
    uint16_t pageBytes;
    uint16_t pageOps;
    uint16_t eraseOps;
    uint16_t erasedBlocks;
    uint16_t page;
    uint16_t pageIndex;

    explicit FakeStorage(uint8_t fill = 0xFF)
        : reads(0), pageBytes(0), pageOps(0), eraseOps(0), erasedBlocks(0), page(0), pageIndex(0) {
        for (uint16_t i = 0; i < kSaveBytes; ++i)
            bytes[i] = fill;
    }

    uint8_t read(uint16_t offset) {
        ++reads;
        return offset < kSaveBytes ? bytes[offset] : 0xFF;
    }

    void eraseBlock(uint8_t block) {
        ++eraseOps;
        if (block != 0)
            return;
        for (uint16_t i = 0; i < kSaveBytes; ++i)
            bytes[i] = 0xFF;
        ++erasedBlocks;
    }

    void pageBegin(uint16_t pageNumber) {
        page = pageNumber;
        pageIndex = 0;
    }

    void pageByte(uint8_t value) {
        const uint16_t offset = static_cast<uint16_t>(page * voyage_save::kPageBytes + pageIndex);
        if (offset < kSaveBytes)
            bytes[offset] &= value;
        ++pageBytes;
        ++pageIndex;
    }

    void pageEnd() {
        ++pageOps;
    }

    void resetCounters() {
        reads = 0;
        pageBytes = 0;
        pageOps = 0;
        eraseOps = 0;
        erasedBlocks = 0;
        page = 0;
        pageIndex = 0;
    }
};

struct Fixture {
    State state;
    chart::State chart;

    Fixture() {
        reset();
    }

    void reset() {
        game::reset(state);
        state.docked = static_cast<int8_t>(campaign::kTidewall);
        chart::reset(chart);
    }

    void save(FakeStorage &storage) const {
        voyage_save::save(state, chart, storage);
    }

    voyage_save::LoadResult load(FakeStorage &storage) {
        return voyage_save::load(state, chart, storage);
    }

    void newVoyage(FakeStorage &storage) {
        voyage_save::newVoyage(state, chart, storage);
    }
};

inline void fillPattern(uint8_t *bytes, uint16_t count, uint8_t seed) {
    for (uint16_t i = 0; i < count; ++i)
        bytes[i] = static_cast<uint8_t>(seed + i * 37u);
}

inline bool sameChart(const chart::State &a, const chart::State &b) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (a.explored[i] != b.explored[i])
            return false;
    }
    return true;
}

// Dirty pre-existing bytes in the save block must not be imported into a new
// voyage.
inline void fillOldChartBlock(FakeStorage &storage) {
    for (uint16_t i = 0; i < 260; ++i)
        storage.bytes[i] = static_cast<uint8_t>(i * 11u + 3u);
}

inline bool chartEmpty(const chart::State &chart) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (chart.explored[i] != 0)
            return false;
    }
    return true;
}

inline bool spawnExplored(const State &state, const chart::State &chart) {
    int16_t cellX = 0;
    int16_t cellY = 0;
    return chart::cellFromPixels(state.x, state.y, cellX, cellY) && chart::isExplored(chart, cellX, cellY);
}

inline bool checkpointMatches(const State &a, const State &b) {
    if (a.docked != b.docked || a.campaign != b.campaign || a.surveys != b.surveys || a.tools != b.tools || a.pickups != b.pickups ||
        a.openGates != b.openGates || a.seenGates != b.seenGates || a.ending != b.ending || a.pendingStory != b.pendingStory || a.cargo != b.cargo)
        return false;
    return a.activeJob.kind == b.activeJob.kind && a.activeJob.mission == b.activeJob.mission && a.activeJob.from == b.activeJob.from &&
           a.activeJob.to == b.activeJob.to && a.activeJob.crates == b.activeJob.crates;
}

inline bool allBytesAre(const FakeStorage &storage, uint16_t first, uint16_t last, uint8_t value) {
    for (uint16_t i = first; i <= last; ++i) {
        if (storage.bytes[i] != value)
            return false;
    }
    return true;
}

inline void setValidPayload(Fixture &fixture) {
    fixture.state.tools = static_cast<uint8_t>(voyage_save::toolsMask());
    fixture.state.pickups = static_cast<uint8_t>(voyage_save::pickupsMask() & 0xA5u);
    fixture.state.openGates = static_cast<uint16_t>(voyage_save::gatesMask() & 0x155u);
    fixture.state.seenGates = static_cast<uint16_t>(voyage_save::gatesMask() & 0x0AAu);
    fixture.state.campaign = campaign::kMilestoneFirst;
    fixture.state.surveys = campaign::kSurveyMask;
    fixture.state.ending = 0;
}

inline bool makeFirstJob(Fixture &fixture) {
    fixture.reset();
    return campaign::accept(fixture.state, campaign::kFirst);
}

}   // namespace voyage_save_test

inline void VoyageSaveSuite(TestRunner &runner) {
    using namespace voyage_save_test;

    {
        Test test("raw single-slot layout is 16 scalar bytes plus the chart");
        test.assertEqual(voyage_save::kSaveBytes, 4096, "one erase block");
        test.assertEqual(voyage_save::kBlockBytes, 4096, "block width");
        test.assertEqual(voyage_save::kPageBytes, 256, "flash page");
        test.assertEqual(voyage_save::kWritePages, 2, "two programmed pages");
        test.assertEqual(voyage_save::kPendingStoryMax, story_view::kSceneMax, "pending scene ids match the frozen decks and handoffs");
        test.assertEqual(voyage_save::kPendingStoryNone, story_view::kCardNone, "zero is the no-deck value");
        test.assertEqual(voyage_save::kScalarBytes, 16, "scalar payload width");
        test.assertEqual(voyage_save::kPayloadBytes, static_cast<uint16_t>(16 + world::CHART_BYTES), "payload width follows the chart");
        test.assertEqual(voyage_save::kPayloadBytes, 272, "272 meaningful bytes today");
        test.assertEqual(voyage_save::kFieldPendingStory + 1, voyage_save::kScalarBytes, "field table covers the scalar");
        for (uint8_t berth = 0; berth < campaign::kBerthCount; ++berth) {
            world::Berth info;
            test.assertTrue(world::berthInfo(berth, info), "every valid campaign berth has generated metadata");
        }
        runner.add(test);
    }

    {
        Test test("first save erases one block and only programs two pages");
        FakeStorage storage(0xFF);

        Fixture fixture;
        setValidPayload(fixture);
        fixture.save(storage);
        test.assertEqual(storage.eraseOps, 1, "one block erase");
        test.assertEqual(storage.erasedBlocks, 1, "one block erased");
        test.assertEqual(storage.pageOps, 2, "two page programs");
        test.assertEqual(storage.pageBytes, 2 * voyage_save::kPageBytes, "two full pages");
        test.assertTrue(allBytesAre(storage, voyage_save::kWriteBytes, kSaveBytes - 1, 0xFF), "block tail stays erased");
        test.assertTrue(allBytesAre(storage, voyage_save::kPayloadBytes, voyage_save::kWriteBytes - 1, 0xFF), "payload is FF padded");

        test.assertTrue(voyage_save::hasSave(storage), "slot is written");

        Fixture restored;
        restored.reset();
        storage.resetCounters();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "first save loads");
        test.assertEqual(storage.pageBytes, 0, "load never programs a page");
        test.assertEqual(storage.eraseOps, 0, "load never erases");
        test.assertTrue(checkpointMatches(restored.state, fixture.state), "all saved fields round-trip");
        test.assertEqual(restored.state.cargo, 0, "no active job means no cargo");
        test.assertTrue(chartEmpty(restored.chart), "empty chart round-trips");
        runner.add(test);
    }

    {
        Test test("new voyage and Continue place berth position and camera on generated terrain");
        world_test::FakeWorld reader;
        std::ifstream terrain("fxdata/tables/exploration.bin", std::ios::binary);
        test.assertTrue(static_cast<bool>(terrain.read(reinterpret_cast<char *>(reader.tiles), world::WORLD_BYTES)), "actual collision plane loads");

        world::Berth berth;
        test.assertTrue(world::berthInfo(campaign::kTidewall, berth), "generated Tidewall berth");
        FakeStorage storage(0xFF);
        Fixture fresh;
        fresh.state.x = 0;
        fresh.state.y = 0;
        fresh.state.cameraXQ4 = 0;
        fresh.state.cameraYQ4 = 0;
        fresh.newVoyage(storage);
        test.assertEqual(fresh.state.docked, campaign::kTidewall, "new voyage docks at Tidewall");
        test.assertEqual(fresh.state.x, berth.pixel_x, "new voyage berth x");
        test.assertEqual(fresh.state.y, berth.pixel_y, "new voyage berth y");
        test.assertEqual(fresh.state.cameraXQ4, game::cameraTargetXQ4(fresh.state), "new voyage camera x is snapped");
        test.assertEqual(fresh.state.cameraYQ4, game::cameraTargetYQ4(fresh.state), "new voyage camera y is snapped");
        test.assertTrue(spawnExplored(fresh.state, fresh.chart), "first checkpoint includes the spawn reveal");
        test.assertTrue(!world::tileSolid(reader, berth.tile_x, berth.tile_y), "new voyage berth is open water in the packed plane");
        const cave::Camera newCamera = pilot_view::cameraForState(fresh.state);
        test.assertTrue(pilot_view::visible(pilot_view::screenX(fresh.state, newCamera), pilot_view::screenY(fresh.state, newCamera)), "new voyage hull is visible in the world view");

        Fixture restored;
        restored.reset();
        restored.state.x = 64;
        restored.state.y = 32;
        restored.state.cameraXQ4 = 0;
        restored.state.cameraYQ4 = 0;
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "Continue loads the new voyage");
        test.assertEqual(restored.state.x, berth.pixel_x, "Continue berth x");
        test.assertEqual(restored.state.y, berth.pixel_y, "Continue berth y");
        test.assertEqual(restored.state.cameraXQ4, game::cameraTargetXQ4(restored.state), "Continue camera x is snapped");
        test.assertEqual(restored.state.cameraYQ4, game::cameraTargetYQ4(restored.state), "Continue camera y is snapped");
        test.assertTrue(spawnExplored(restored.state, restored.chart), "Continue restores the saved spawn reveal");
        test.assertTrue(sameChart(fresh.chart, restored.chart), "first checkpoint chart round-trips exactly");
        const cave::Camera restoredCamera = pilot_view::cameraForState(restored.state);
        test.assertTrue(pilot_view::visible(pilot_view::screenX(restored.state, restoredCamera), pilot_view::screenY(restored.state, restoredCamera)), "Continue hull is visible in the world view");
        runner.add(test);
    }

    {
        Test test("payload is encoded field by field at the raw offsets");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.state.ending = 1;
        fixture.state.pendingStory = story_view::kCardLift;
        fixture.save(storage);
        const uint16_t payload = 0;

        test.assertEqual(storage.bytes[payload + 0], campaign::kTidewall, "berth offset 0");
        test.assertEqual(storage.bytes[payload + 1], voyage_save::toolsMask(), "tools offset 1");
        test.assertEqual(storage.bytes[payload + 2], static_cast<uint8_t>(voyage_save::pickupsMask() & 0xA5u), "pickups offset 2");
        test.assertEqual(static_cast<uint16_t>(storage.bytes[payload + 3] | (static_cast<uint16_t>(storage.bytes[payload + 4]) << 8)),
                         static_cast<uint16_t>(voyage_save::gatesMask() & 0x155u), "open gates offset 3");
        test.assertEqual(static_cast<uint16_t>(storage.bytes[payload + 5] | (static_cast<uint16_t>(storage.bytes[payload + 6]) << 8)),
                         static_cast<uint16_t>(voyage_save::gatesMask() & 0x0AAu), "seen gates offset 5");
        test.assertEqual(storage.bytes[payload + 7], campaign::kMilestoneFirst, "milestones offset 7");
        test.assertEqual(storage.bytes[payload + 8], campaign::kSurveyMask, "surveys offset 8");
        test.assertEqual(storage.bytes[payload + 9], game::kJobKindNone, "job kind offset 9");
        test.assertEqual(storage.bytes[payload + 10], game::kMissionNone, "mission offset 10");
        test.assertEqual(storage.bytes[payload + 11], game::kJobNone, "origin offset 11");
        test.assertEqual(storage.bytes[payload + 12], game::kJobNone, "destination offset 12");
        test.assertEqual(storage.bytes[payload + 13], 0, "job load offset 13");
        test.assertEqual(storage.bytes[payload + 14], 1, "ending offset 14");
        test.assertEqual(storage.bytes[payload + 15], story_view::kCardLift, "pending story offset 15");
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            test.assertEqual(storage.bytes[payload + voyage_save::kScalarBytes + i], fixture.chart.explored[i], "chart follows scalar bytes");
        runner.add(test);
    }

    {
        Test test("pending story deck round-trips and clears on a new voyage");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.state.pendingStory = story_view::kHandoffJointMissing;
        fixture.save(storage);
        test.assertEqual(voyage_save::hasSave(storage), true, "pending handoff checkpoint is written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "pending handoff checkpoint loads");
        test.assertEqual(restored.state.pendingStory, story_view::kHandoffJointMissing, "pending handoff round-trips");

        Fixture fresh;
        fresh.newVoyage(storage);
        Fixture replacement;
        replacement.reset();
        replacement.state.pendingStory = story_view::kCardBeacon;
        test.assertEqual(replacement.load(storage), voyage_save::kLoaded, "replacement checkpoint loads");
        test.assertEqual(replacement.state.pendingStory, story_view::kCardNone, "new voyage writes no pending deck");
        runner.add(test);
    }

    {
        Test test("repeated saves overwrite the single slot and load the latest payload");
        FakeStorage storage(0xFF);
        Fixture first;
        setValidPayload(first);
        first.save(storage);
        first.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump);
        first.state.pendingStory = story_view::kCardPump;
        first.state.ending = 1;
        first.save(storage);
        test.assertEqual(storage.eraseOps, 2, "each save erases the block");
        test.assertEqual(storage.pageOps, 4, "each save programs two pages");
        test.assertEqual(voyage_save::hasSave(storage), true, "single slot written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "latest loads");
        test.assertEqual(restored.state.campaign, static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump), "latest campaign");
        test.assertEqual(restored.state.pendingStory, story_view::kCardPump, "latest pending deck");
        test.assertEqual(restored.state.ending, 1, "latest ending flag");
        runner.add(test);
    }

    {
        Test test("an identical repeated save still erases and programs both pages");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.save(storage);
        FakeStorage before = storage;
        storage.resetCounters();
        fixture.save(storage);
        test.assertEqual(storage.eraseOps, 1, "identical save erases");
        test.assertEqual(storage.erasedBlocks, 1, "identical save erases the block");
        test.assertEqual(storage.pageOps, 2, "identical save programs two pages");
        test.assertEqual(storage.pageBytes, 2 * voyage_save::kPageBytes, "identical save writes two full pages");
        bool same = true;
        for (uint16_t i = 0; i < voyage_save::kWriteBytes; ++i)
            same = storage.bytes[i] == before.bytes[i] && same;
        test.assertTrue(same, "identical save reproduces the same two pages");
        runner.add(test);
    }

    {
        Test test("early survey discoveries save and load before FIRST");
        FakeStorage storage(0xFF);
        Fixture fixture;
        fixture.reset();
        test.assertTrue(campaign::recordSurvey(fixture.state, campaign::kPumpSurveyId), "pump survey records before FIRST");
        test.assertTrue(campaign::recordSurvey(fixture.state, campaign::kLiftSurveyId), "lift survey records before FIRST");
        test.assertEqual(fixture.state.campaign, 0, "no milestone yet");
        fixture.save(storage);
        test.assertEqual(voyage_save::hasSave(storage), true, "early-survey checkpoint is written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "early-survey checkpoint loads");
        test.assertEqual(restored.state.surveys, campaign::kSurveyMask, "both early survey bits persist");
        test.assertEqual(restored.state.campaign, 0, "FIRST is still incomplete");
        runner.add(test);
    }

    {
        Test test("active cargo restored from mission metadata and fixed crates");
        FakeStorage storage(0xFF);
        Fixture fixture;
        test.assertTrue(makeFirstJob(fixture), "FIRST accepted");
        fixture.save(storage);

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "loaded campaign checkpoint");
        test.assertEqual(restored.state.cargo, game::kJobCrates, "active cargo restored");
        test.assertEqual(restored.state.activeJob.kind, game::kJobKindCampaign, "campaign kind restored");
        test.assertEqual(restored.state.activeJob.mission, campaign::kFirst, "mission restored");
        test.assertEqual(restored.state.activeJob.from, campaign::kTidewall, "origin restored");
        test.assertEqual(restored.state.activeJob.to, campaign::kBenthos, "destination restored");
        test.assertEqual(restored.state.activeJob.crates, game::kJobCrates, "fixed crate requirement restored");
        test.assertEqual(restored.state.ballast, game::neutralForNormalized(game::kJobCrates), "loaded trim restored");
        runner.add(test);
    }

    {
        Test test("blank slot is missing and written slot loads");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.campaign = campaign::kMilestoneFirst;
        test.assertEqual(probe.load(blank), voyage_save::kNoSave, "blank storage is missing");
        test.assertEqual(probe.state.campaign, campaign::kMilestoneFirst, "missing save leaves state untouched");
        test.assertEqual(voyage_save::hasSave(blank), false, "blank slot");

        FakeStorage storage(0xFF);
        Fixture saved;
        setValidPayload(saved);
        saved.save(storage);
        test.assertEqual(voyage_save::hasSave(storage), true, "written slot");
        Fixture loaded;
        loaded.reset();
        test.assertEqual(loaded.load(storage), voyage_save::kLoaded, "written slot loads");
        test.assertTrue(checkpointMatches(loaded.state, saved.state), "written payload loads");
        runner.add(test);
    }

    {
        Test test("legacy chart bytes are ignored by a new voyage");
        FakeStorage storage(0xFF);
        fillOldChartBlock(storage);

        Fixture fresh;
        fresh.newVoyage(storage);
        test.assertEqual(voyage_save::hasSave(storage), true, "new voyage is written");
        test.assertTrue(spawnExplored(fresh.state, fresh.chart), "new voyage persists the spawn reveal");
        test.assertEqual(fresh.state.campaign, 0, "new voyage grants no campaign progress");
        test.assertEqual(fresh.state.tools, 0, "new voyage grants no tools");
        test.assertEqual(fresh.state.docked, campaign::kTidewall, "new voyage starts at Tidewall");

        FakeStorage existing(0xFF);
        Fixture oldVoyage;
        setValidPayload(oldVoyage);
        oldVoyage.save(existing);
        fillOldChartBlock(existing);
        Fixture replacement;
        replacement.newVoyage(existing);
        test.assertTrue(spawnExplored(replacement.state, replacement.chart), "replacement persists the spawn reveal");
        runner.add(test);
    }

    {
        Test test("written payload loads raw with safe berth and story bounds");
        FakeStorage storage(0xFF);
        storage.bytes[voyage_save::kFieldBerth] = 250;
        storage.bytes[voyage_save::kFieldTools] = 0xAB;
        storage.bytes[voyage_save::kFieldCampaign] = 0x7F;
        storage.bytes[voyage_save::kFieldJobKind] = game::kJobKindCampaign;
        storage.bytes[voyage_save::kFieldJobMission] = campaign::kFirst;
        storage.bytes[voyage_save::kFieldJobFrom] = campaign::kTidewall;
        storage.bytes[voyage_save::kFieldJobTo] = campaign::kBenthos;
        storage.bytes[voyage_save::kFieldJobCrates] = 9;
        storage.bytes[voyage_save::kFieldPendingStory] = 250;
        storage.bytes[voyage_save::kScalarBytes + 3] = 0x5A;

        Fixture probe;
        probe.reset();
        test.assertEqual(probe.load(storage), voyage_save::kLoaded, "arbitrary written bytes still load");
        test.assertEqual(probe.state.docked, static_cast<int8_t>(-1), "unknown berth stays in free flight");
        test.assertEqual(probe.state.x, game::kSpawnX, "unknown berth keeps the reset x");
        test.assertEqual(probe.state.y, game::kSpawnY, "unknown berth keeps the reset y");
        test.assertEqual(probe.state.tools, 0xAB, "raw tools byte survives");
        test.assertEqual(probe.state.campaign, 0x7F, "raw campaign byte survives");
        test.assertEqual(probe.state.pendingStory, voyage_save::kPendingStoryNone, "out-of-range story id is dropped");
        test.assertEqual(probe.state.cargo, game::kCargoMax, "raw crate byte clamps to the hold");
        test.assertEqual(probe.chart.explored[3], 0x5A, "raw chart byte loads");

        storage.bytes[voyage_save::kFieldPendingStory] = voyage_save::kPendingStoryMax;
        test.assertEqual(probe.load(storage), voyage_save::kLoaded, "max story id loads");
        test.assertEqual(probe.state.pendingStory, voyage_save::kPendingStoryMax, "max story id is bounded");
        runner.add(test);
    }

    {
        Test test("written save is never rejected by a retired header or CRC contract");
        FakeStorage storage(0xFF);
        Fixture source;
        setValidPayload(source);
        source.save(storage);
        storage.bytes[voyage_save::kFieldCampaign] = 0x3F;
        storage.bytes[voyage_save::kFieldPendingStory] = story_view::kCardHome;

        Fixture probe;
        probe.reset();
        test.assertEqual(probe.load(storage), voyage_save::kLoaded, "mutated payload still loads");
        test.assertEqual(probe.state.campaign, 0x3F, "mutated campaign is read raw");
        test.assertEqual(probe.state.pendingStory, story_view::kCardHome, "mutated story id is read raw");
        runner.add(test);
    }

    {
        Test test("blank load leaves live state and written load applies payload");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.campaign = campaign::kMilestoneFirst;
        fillPattern(probe.chart.explored, world::CHART_BYTES, 71);
        chart::State chartBefore = probe.chart;
        const State stateBefore = probe.state;
        test.assertEqual(probe.load(blank), voyage_save::kNoSave, "blank load");
        test.assertTrue(checkpointMatches(probe.state, stateBefore), "blank load leaves state");
        test.assertTrue(sameChart(probe.chart, chartBefore), "blank load leaves chart");

        FakeStorage storage(0xFF);
        Fixture source;
        setValidPayload(source);
        source.save(storage);
        probe.reset();
        probe.state.campaign = campaign::kMilestoneFirst;
        fillPattern(probe.chart.explored, world::CHART_BYTES, 71);
        test.assertEqual(probe.load(storage), voyage_save::kLoaded, "written load");
        test.assertTrue(checkpointMatches(probe.state, source.state), "written load applies state");
        test.assertTrue(sameChart(probe.chart, source.chart), "written load applies chart");
        runner.add(test);
    }

    {
        Test test("save touches only the generated 4 KiB block");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.save(storage);
        test.assertEqual(storage.eraseOps, 1, "one erase");
        test.assertEqual(storage.pageOps, 2, "two pages");
        test.assertEqual(storage.pageBytes, 2 * voyage_save::kPageBytes, "two page programs");
        test.assertTrue(allBytesAre(storage, voyage_save::kWriteBytes, kSaveBytes - 1, 0xFF), "unused block tail is erased");
        runner.add(test);
    }
}
