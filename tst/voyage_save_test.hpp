#pragma once

// Native storage tests for the single-slot voyage checkpoint.
//
// The fixture models one 4 KiB external-NOR erase block: erased state 0xFF, page
// programs can only clear bits, and an interrupted erase/program is a no-op
// after the failure point. One slot lives at offset 0. A power interruption may
// lose the current checkpoint, but no torn record may ever validate or load.

#include <fstream>

#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "../src/voyage_save.hpp"
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
constexpr uint16_t kNoFault = 0xFFFF;

// The fixture models the external NOR save block: erased state 0xFF, page
// programs can only clear bits, and a 4 KiB block erase restores 0xFF. The
// fault injector stops after N programmed bytes or during an erase, then makes
// every later operation a no-op.
struct FakeStorage {
    uint8_t bytes[kSaveBytes];
    uint16_t reads;
    uint16_t pageBytes;
    uint16_t pageOps;
    uint16_t eraseOps;
    uint16_t erasedBlocks;
    uint16_t failedAt;
    bool failed;
    bool failErase;
    uint16_t eraseBytes;
    uint16_t page;
    uint16_t pageIndex;

    explicit FakeStorage(uint8_t fill = 0xFF)
        : reads(0), pageBytes(0), pageOps(0), eraseOps(0), erasedBlocks(0), failedAt(kNoFault), failed(false), failErase(false), eraseBytes(0), page(0), pageIndex(0) {
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
            failed = true;
        if (failErase && eraseOps == 1) {
            failed = true;
            for (uint16_t i = 0; i < eraseBytes && i < kSaveBytes; ++i)
                bytes[i] = 0xFF;
            return;
        }
        if (failed)
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
        if (failed || pageBytes >= failedAt) {
            failed = true;
            ++pageIndex;
            return;
        }
        const uint16_t offset = static_cast<uint16_t>(page * voyage_save::kPageBytes + pageIndex);
        if (offset < kSaveBytes)
            bytes[offset] &= value;
        ++pageBytes;
        ++pageIndex;
    }

    void pageEnd() {
        if (!failed)
            ++pageOps;
    }

    void resetCounters() {
        reads = 0;
        pageBytes = 0;
        pageOps = 0;
        eraseOps = 0;
        erasedBlocks = 0;
        failed = false;
        failErase = false;
        eraseBytes = 0;
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

    voyage_save::SaveResult save(FakeStorage &storage) const {
        return voyage_save::save(state
                                 ,
                                 chart
                                 ,
                                 storage);
    }

    voyage_save::LoadResult load(FakeStorage &storage) {
        return voyage_save::load(state
                                 ,
                                 chart
                                 ,
                                 storage);
    }

    voyage_save::SaveResult newVoyage(FakeStorage &storage) {
        return voyage_save::newVoyage(state
                                      ,
                                      chart
                                      ,
                                      storage);
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

// Dirty pre-existing bytes in the save block must not be read as chart data or
// imported into a new voyage.
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
        a.openGates != b.openGates || a.seenGates != b.seenGates || a.ending != b.ending || a.pendingStory != b.pendingStory || a.voyageTicks != b.voyageTicks ||
        a.delivered != b.delivered || a.rescues != b.rescues || a.cargo != b.cargo)
        return false;
    if (a.hullClass != b.hullClass)
        return false;
    return a.activeJob.kind == b.activeJob.kind && a.activeJob.mission == b.activeJob.mission && a.activeJob.from == b.activeJob.from && a.activeJob.to == b.activeJob.to &&
           a.activeJob.crates == b.activeJob.crates;
}

inline uint16_t readU16(const FakeStorage &storage, uint16_t address) {
    return static_cast<uint16_t>(storage.bytes[address] | (static_cast<uint16_t>(storage.bytes[address + 1]) << 8));
}

inline uint32_t readU32(const FakeStorage &storage, uint16_t address) {
    uint32_t value = storage.bytes[address];
    value |= static_cast<uint32_t>(storage.bytes[address + 1]) << 8;
    value |= static_cast<uint32_t>(storage.bytes[address + 2]) << 16;
    value |= static_cast<uint32_t>(storage.bytes[address + 3]) << 24;
    return value;
}

inline void writeU16(FakeStorage &storage, uint16_t address, uint16_t value) {
    storage.bytes[address] = static_cast<uint8_t>(value);
    storage.bytes[address + 1] = static_cast<uint8_t>(value >> 8);
}

inline void refreshCrc(FakeStorage &storage) {
    const uint16_t length = readU16(storage, voyage_save::kSlotOffset + voyage_save::kLengthOffset);
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < voyage_save::kCrcOffset; ++i)
        crc = voyage_save::crc16Update(crc, storage.bytes[voyage_save::kSlotOffset + i]);
    for (uint16_t i = 0; i < length; ++i)
        crc = voyage_save::crc16Update(crc, storage.bytes[voyage_save::kSlotOffset + voyage_save::kPayloadOffset + i]);
    writeU16(storage, voyage_save::kSlotOffset + voyage_save::kCrcOffset, crc);
}

inline uint16_t writeBoundaries(const Fixture &fixture, const FakeStorage &baseline) {
    FakeStorage scratch = baseline;
    scratch.resetCounters();
    scratch.failedAt = kNoFault;
    fixture.save(scratch);
    return scratch.pageBytes;
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
    fixture.state.voyageTicks = 0x12345678u;
    fixture.state.delivered = 0x0102;
    fixture.state.rescues = 0x0304;
    fixture.state.hullClass = static_cast<uint8_t>(roster::kHauler);
}

inline bool makeFirstJob(Fixture &fixture) {
    fixture.reset();
    return campaign::accept(fixture.state, campaign::kFirst);
}

}   // namespace voyage_save_test

inline void VoyageSaveSuite(TestRunner &runner) {
    using namespace voyage_save_test;

    {
        Test test("CRC-16/CCITT-FALSE vector and frozen single-slot layout");
        const uint8_t vector[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        test.assertEqual(voyage_save::crc16Bytes(vector, 9), 0x29B1, "CRC check vector");
        test.assertEqual(voyage_save::kSaveBytes, 4096, "one erase block");
        test.assertEqual(voyage_save::kBlockBytes, 4096, "block width");
        test.assertEqual(voyage_save::kSlotOffset, 0, "slot offset");
        test.assertEqual(voyage_save::kSlotBytes, 512, "slot width");
        test.assertEqual(voyage_save::kPayloadCapacity, 495, "payload capacity");
        test.assertEqual(voyage_save::kCommitOffset, 511, "commit byte offset");
        test.assertEqual(voyage_save::kPageBytes, 256, "flash page");
        test.assertEqual(voyage_save::kPendingStoryMax, story_view::kSceneMax, "pending scene ids match the frozen decks and handoffs");
        test.assertEqual(voyage_save::kPendingStoryNone, story_view::kCardNone, "zero is the no-deck value");
        test.assertEqual(voyage_save::kScalarBytes, 25, "scalar payload width");
        test.assertEqual(voyage_save::kPayloadBytes, static_cast<uint16_t>(25 + world::CHART_BYTES), "payload width follows chart bit");
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
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "first save succeeds");
        test.assertEqual(storage.eraseOps, 1, "one block erase");
        test.assertEqual(storage.erasedBlocks, 1, "one block erased");
        test.assertEqual(storage.pageOps, 2, "two page programs");
        test.assertEqual(storage.pageBytes, 2 * voyage_save::kPageBytes, "two full pages");
        test.assertTrue(allBytesAre(storage, voyage_save::kSlotBytes, kSaveBytes - 1, 0xFF), "slot tail stays erased");

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
        Test test("legacy Minnow/Leviathan checkpoints normalize to HAULER and keep progress");
        const uint16_t payload = static_cast<uint16_t>(voyage_save::kSlotOffset + voyage_save::kPayloadOffset);
        for (uint8_t legacy = 0; legacy <= 2; legacy += 2) {
            FakeStorage storage(0xFF);
            Fixture source;
            setValidPayload(source);
            test.assertEqual(source.save(storage), voyage_save::kSaved, "baseline legacy save");

            // Rewrite only the frozen hull byte and re-sign, as a schema-8 cart
            // checkpoint written by the retired selector would contain.
            storage.bytes[payload + voyage_save::kFieldHullClass] = legacy;
            refreshCrc(storage);
            test.assertEqual(voyage_save::hasSave(storage), true, "legacy hull id still written");

            Fixture restored;
            restored.reset();
            restored.state.delivered = 4242;
            test.assertEqual(restored.load(storage), voyage_save::kLoaded, "legacy hull id still loads");
            test.assertEqual(restored.state.hullClass, static_cast<uint8_t>(roster::kHauler), "legacy hull id normalizes to HAULER");

            State expected = source.state;
            expected.hullClass = static_cast<uint8_t>(roster::kHauler);
            test.assertTrue(checkpointMatches(restored.state, expected), "all voyage progress survives normalization");
        }

        // A live legacy id is also accepted for a deliberate rewrite and the
        // new commit always stores HAULER.
        Fixture source;
        setValidPayload(source);
        source.state.hullClass = 2;
        FakeStorage storage(0xFF);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "legacy live state writes");
        test.assertEqual(storage.bytes[payload + voyage_save::kFieldHullClass], roster::kHauler, "save always writes HAULER");
        runner.add(test);
    }

    {
        Test test("unknown hull byte loads as HAULER");
        const uint16_t payload = static_cast<uint16_t>(voyage_save::kSlotOffset + voyage_save::kPayloadOffset);
        FakeStorage storage(0xFF);
        Fixture source;
        setValidPayload(source);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "baseline save");

        storage.bytes[payload + voyage_save::kFieldHullClass] = 3;
        Fixture probe;
        probe.reset();
        test.assertEqual(probe.load(storage), voyage_save::kLoaded, "unknown hull byte loads");
        test.assertEqual(probe.state.hullClass, roster::kHauler, "unknown hull normalizes to HAULER");
        test.assertEqual(probe.state.delivered, source.state.delivered, "other fields survive");
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
        test.assertEqual(fresh.newVoyage(storage), voyage_save::kSaved, "new voyage saves");
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
        Test test("payload is encoded field by field at the frozen offsets");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "layout save");
        const uint16_t base = voyage_save::kSlotOffset;
        const uint16_t payload = static_cast<uint16_t>(base + voyage_save::kPayloadOffset);

        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset], voyage_save::kMagic0, "magic 0");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 1], voyage_save::kMagic1, "magic 1");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 2], voyage_save::kMagic2, "magic 2");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 3], voyage_save::kMagic3, "magic 3");
        test.assertEqual(storage.bytes[base + voyage_save::kSchemaOffset], voyage_save::kSchemaVersion, "schema");
        test.assertEqual(storage.bytes[base + voyage_save::kLayoutOffset], world::LAYOUT_VERSION, "world layout");
        test.assertEqual(storage.bytes[base + voyage_save::kContentOffset], voyage_save::kCampaignContentVersion, "content version");
        test.assertEqual(storage.bytes[base + voyage_save::kSignatureOffset], voyage_save::kProfileSignature, "signature");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(base + voyage_save::kLengthOffset)), voyage_save::kPayloadBytes, "payload length");
        test.assertTrue(allBytesAre(storage, static_cast<uint16_t>(base + 8), static_cast<uint16_t>(base + 11), 0xFF), "retired sequence bytes stay erased");

        test.assertEqual(storage.bytes[payload + 0], campaign::kTidewall, "berth offset 0");
        test.assertEqual(storage.bytes[payload + 1], roster::kHauler, "hull class offset 1 always writes HAULER");
        test.assertEqual(storage.bytes[payload + 2], voyage_save::toolsMask(), "tools offset 2");
        test.assertEqual(storage.bytes[payload + 3], static_cast<uint8_t>(voyage_save::pickupsMask() & 0xA5u), "pickups offset 3");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 4)), static_cast<uint16_t>(voyage_save::gatesMask() & 0x155u), "open gates offset 4");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 6)), static_cast<uint16_t>(voyage_save::gatesMask() & 0x0AAu), "seen gates offset 6");
        test.assertEqual(storage.bytes[payload + 8], campaign::kMilestoneFirst, "milestones offset 8");
        test.assertEqual(storage.bytes[payload + 9], campaign::kSurveyMask, "surveys offset 9");
        test.assertEqual(storage.bytes[payload + 10], game::kJobKindNone, "job kind offset 10");
        test.assertEqual(storage.bytes[payload + 11], game::kMissionNone, "mission offset 11");
        test.assertEqual(storage.bytes[payload + 12], game::kJobNone, "origin offset 12");
        test.assertEqual(storage.bytes[payload + 13], game::kJobNone, "destination offset 13");
        test.assertEqual(storage.bytes[payload + 14], 0, "job load offset 14");
        test.assertEqual(readU32(storage, static_cast<uint16_t>(payload + 15)), 0x12345678u, "voyage ticks offset 15");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 19)), 0x0102, "delivered offset 19");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 21)), 0x0304, "rescues offset 21");
        test.assertEqual(storage.bytes[payload + 23], 0, "ending offset 23");
        test.assertEqual(storage.bytes[payload + 24], 0, "pending story offset 24");
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
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "pending handoff checkpoint saves");
        test.assertEqual(voyage_save::hasSave(storage), true, "pending handoff checkpoint is written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "pending handoff checkpoint loads");
        test.assertEqual(restored.state.pendingStory, story_view::kHandoffJointMissing, "pending handoff round-trips");

        Fixture fresh;
        test.assertEqual(fresh.newVoyage(storage), voyage_save::kSaved, "new voyage clears the pending deck");
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
        test.assertEqual(first.save(storage), voyage_save::kSaved, "first save");
        first.state.voyageTicks = 0x87654321u;
        first.state.delivered = 0xCAFEu;
        test.assertEqual(first.save(storage), voyage_save::kSaved, "second save");
        test.assertEqual(storage.eraseOps, 2, "each save erases the block");
        test.assertEqual(voyage_save::hasSave(storage), true, "single slot written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "latest loads");
        test.assertEqual(restored.state.voyageTicks, 0x87654321u, "latest voyage ticks");
        test.assertEqual(restored.state.delivered, 0xCAFEu, "latest delivered count");
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
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "early-survey checkpoint saves");
        test.assertEqual(voyage_save::hasSave(storage), true, "early-survey checkpoint is written");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "early-survey checkpoint loads");
        test.assertEqual(restored.state.surveys, campaign::kSurveyMask, "both early survey bits persist");
        test.assertEqual(restored.state.campaign, 0, "FIRST is still incomplete");
        runner.add(test);
    }

    {
        Test test("blank slot is missing and written slot loads");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.delivered = 7;
        test.assertEqual(probe.load(blank), voyage_save::kNoSave, "blank storage is missing");
        test.assertEqual(probe.state.delivered, 7, "missing save leaves state untouched");
        test.assertEqual(voyage_save::hasSave(blank), false, "blank slot");

        FakeStorage storage(0xFF);
        Fixture saved;
        setValidPayload(saved);
        test.assertEqual(saved.save(storage), voyage_save::kSaved, "baseline save");
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
        test.assertEqual(fresh.newVoyage(storage), voyage_save::kSaved, "new voyage saves");
        test.assertEqual(voyage_save::hasSave(storage), true, "new voyage is written");
        test.assertTrue(spawnExplored(fresh.state, fresh.chart), "new voyage persists the spawn reveal");
        test.assertEqual(fresh.state.campaign, 0, "new voyage grants no campaign progress");
        test.assertEqual(fresh.state.delivered, 0, "new voyage starts with no deliveries");
        test.assertEqual(fresh.state.tools, 0, "new voyage grants no tools");
        test.assertEqual(fresh.state.docked, campaign::kTidewall, "new voyage starts at Tidewall");

        FakeStorage existing(0xFF);
        Fixture oldVoyage;
        setValidPayload(oldVoyage);
        test.assertEqual(oldVoyage.save(existing), voyage_save::kSaved, "existing voyage saves");
        fillOldChartBlock(existing);
        Fixture replacement;
        test.assertEqual(replacement.newVoyage(existing), voyage_save::kSaved, "replacement new voyage saves");
        test.assertTrue(spawnExplored(replacement.state, replacement.chart), "replacement persists the spawn reveal");
        runner.add(test);
    }

    {
        Test test("pre-linear in-flight HOME checkpoints load and complete without weakening new HOME");
        Fixture fixture;
        fixture.state.docked = static_cast<int8_t>(campaign::kDeepAnchor);
        fixture.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
        fixture.state.tools = exploration_data::SEAL;
        fixture.state.activeJob.kind = game::kJobKindCampaign;
        fixture.state.activeJob.mission = campaign::kHome;
        fixture.state.activeJob.from = campaign::kDeepAnchor;
        fixture.state.activeJob.to = campaign::kTidewall;
        fixture.state.activeJob.crates = game::kJobCrates;
        fixture.state.cargo = game::kJobCrates;
        fixture.state.delivered = 4;
        FakeStorage storage(0xFF);
        storage.resetCounters();
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "legacy in-flight HOME saves");
        test.assertEqual(storage.eraseOps, 1, "legacy save uses the normal transaction");

        Fixture loaded;
        loaded.reset();
        test.assertEqual(loaded.load(storage), voyage_save::kLoaded, "legacy in-flight HOME loads");
        test.assertEqual(loaded.state.activeJob.mission, campaign::kHome, "HOME job survives");
        test.assertEqual(loaded.state.campaign, fixture.state.campaign, "campaign mask survives");
        test.assertTrue(campaign::activeJobValid(loaded.state), "loaded legacy job passes current invariants");
        test.assertTrue((loaded.state.campaign & campaign::kMilestoneExpedition) == 0, "the loaded save has not fabricated DEEP RELAY");
        test.assertTrue(campaign::missionCompleted(loaded.state, campaign::kHome) == false, "HOME is still pending");
        test.assertTrue(travelTo(loaded.state, campaign::kTidewall), "legacy dispatch reaches Tidewall");
        test.assertEqual(loaded.state.lastDelivery, jobs::kDeliveryDelivered, "legacy dispatch completes");
        test.assertTrue(campaign::missionCompleted(loaded.state, campaign::kHome), "legacy HOME milestone set");
        test.assertEqual(loaded.state.delivered, 5, "legacy delivery count advances once");
        test.assertTrue(campaign::stateValid(loaded.state), "completed legacy HOME remains checkpoint-valid");

        Fixture completedLegacy;
        completedLegacy.reset();
        completedLegacy.state.docked = static_cast<int8_t>(campaign::kTidewall);
        completedLegacy.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon | campaign::kMilestoneHome);
        completedLegacy.state.delivered = 5;
        test.assertEqual(completedLegacy.save(storage), voyage_save::kSaved, "legacy completed HOME without EXPEDITION saves");
        test.assertTrue(campaign::missionCompleted(completedLegacy.state, campaign::kHome), "legacy ending state is preserved");

        Fixture modern;
        modern.reset();
        modern.state.docked = static_cast<int8_t>(campaign::kDeepAnchor);
        modern.state.campaign = static_cast<uint8_t>(campaign::kMilestoneFirst | campaign::kMilestonePump | campaign::kMilestoneLift | campaign::kMilestoneBeacon);
        modern.state.tools = static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
        test.assertTrue(!campaign::accept(modern.state, campaign::kHome), "new HOME cannot be accepted before DEEP RELAY");
        modern.state.campaign |= campaign::kMilestoneExpedition;
        test.assertTrue(campaign::accept(modern.state, campaign::kHome), "new HOME accepts after DEEP RELAY");
        test.assertEqual(modern.save(storage), voyage_save::kSaved, "new post-relay HOME checkpoint saves");
        runner.add(test);
    }

    {
        Test test("profile signature and absent chart/shop/roster behavior");
        uint8_t expected = 0;
        expected |= voyage_save::kSignatureFogChart;
        expected |= voyage_save::kSignatureMultiHull;
        expected |= voyage_save::kSignatureExploration;
        test.assertEqual(voyage_save::kProfileSignature, expected, "profile signature bits");
        test.assertEqual(voyage_save::kProfileSignature, 0x61, "shop bit is retired");
        test.assertEqual(voyage_save::kPayloadBytes, 281, "chart profile payload is 281 B");
        test.assertEqual(voyage_save::kChartBytes, world::CHART_BYTES, "chart bytes are part of the payload");
        runner.add(test);
    }

    {
        Test test("blank load leaves live state and written load applies payload");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.delivered = 4242;
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
        test.assertEqual(source.save(storage), voyage_save::kSaved, "valid baseline");
        probe.reset();
        probe.state.delivered = 4242;
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
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "save");
        test.assertEqual(storage.eraseOps, 1, "one erase");
        test.assertEqual(storage.pageOps, 2, "two pages");
        test.assertEqual(storage.pageBytes, 2 * voyage_save::kPageBytes, "two page programs");
        test.assertTrue(allBytesAre(storage, voyage_save::kSlotBytes, kSaveBytes - 1, 0xFF), "unused block tail is erased");
        runner.add(test);
    }
}

