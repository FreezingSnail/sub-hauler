#pragma once

// Native storage tests for the atomic two-slot voyage save.
//
// The fixture mirrors the device adapter's read/write/update contract. Its
// fault injector fails the Nth physical change and then makes every later
// update a no-op, which is exactly the power-loss shape the commit protocol
// has to survive. Tests run under SAVE_VOYAGE=0 as a compile-out check and
// under explicit full/lean/core campaign+voyage overrides for the real cases.

#include "../src/features.hpp"
#include "../src/voyage_save.hpp"
#include "test.hpp"

#if SH_FEATURE_SAVE_VOYAGE

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

constexpr uint16_t kEepromBytes = 1024;
constexpr uint16_t kNoFault = 0xFFFF;

// Blank EEPROM is 0xFF. update() changes a byte only when it differs; the fault
// injector counts successful physical changes, so every interruption boundary
// is a real EEPROM write rather than an update() no-op.
struct FakeStorage {
    uint8_t bytes[kEepromBytes];
    bool written[kEepromBytes];
    uint16_t reads;
    uint16_t writes;
    uint16_t updateCalls;
    uint16_t changed;
    uint16_t failedAt;
    bool failed;
    uint16_t systemWrites;

    explicit FakeStorage(uint8_t fill = 0xFF) : reads(0), writes(0), updateCalls(0), changed(0), failedAt(kNoFault), failed(false), systemWrites(0) {
        for (uint16_t i = 0; i < kEepromBytes; ++i) {
            bytes[i] = fill;
            written[i] = false;
        }
    }

    uint8_t read(uint16_t address) {
        ++reads;
        return address < kEepromBytes ? bytes[address] : 0xFF;
    }

    void write(uint16_t address, uint8_t value) {
        ++writes;
        if (address < voyage_save::kSystemBytes)
            ++systemWrites;
        if (address >= kEepromBytes)
            return;
        bytes[address] = value;
        written[address] = true;
    }

    void update(uint16_t address, uint8_t value) {
        ++updateCalls;
        if (address < voyage_save::kSystemBytes)
            ++systemWrites;
        if (address >= kEepromBytes)
            return;
        if (bytes[address] == value)
            return;
        if (failed || changed >= failedAt) {
            failed = true;
            return;
        }
        bytes[address] = value;
        written[address] = true;
        ++changed;
    }

    void resetCounters() {
        reads = 0;
        writes = 0;
        updateCalls = 0;
        changed = 0;
        failed = false;
        systemWrites = 0;
    }
};

struct Fixture {
    State state;
#if SH_FEATURE_FOG_CHART
    chart::State chart;
#endif

    Fixture() {
        reset();
    }

    void reset() {
        game::reset(state);
        state.docked = static_cast<int8_t>(campaign::kTidewall);
#if SH_FEATURE_FOG_CHART
        chart::reset(chart);
#endif
    }

    voyage_save::SaveResult save(FakeStorage &storage) const {
        return voyage_save::save(state
#if SH_FEATURE_FOG_CHART
                                 ,
                                 chart
#endif
                                 ,
                                 storage);
    }

    voyage_save::LoadResult load(FakeStorage &storage) {
        return voyage_save::load(state
#if SH_FEATURE_FOG_CHART
                                 ,
                                 chart
#endif
                                 ,
                                 storage);
    }

    voyage_save::SaveResult newVoyage(FakeStorage &storage, voyage_save::LegacyChart legacy) {
        return voyage_save::newVoyage(state
#if SH_FEATURE_FOG_CHART
                                      ,
                                      chart
#endif
                                      ,
                                      legacy, storage);
    }
};

inline void fillPattern(uint8_t *bytes, uint16_t count, uint8_t seed) {
    for (uint16_t i = 0; i < count; ++i)
        bytes[i] = static_cast<uint8_t>(seed + i * 37u);
}

#if SH_FEATURE_FOG_CHART
inline bool sameChart(const chart::State &a, const chart::State &b) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (a.explored[i] != b.explored[i])
            return false;
    }
    return true;
}

inline bool chartEmpty(const chart::State &chart) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (chart.explored[i] != 0)
            return false;
    }
    return true;
}
#endif

inline bool checkpointMatches(const State &a, const State &b) {
    if (a.docked != b.docked || a.credits != b.credits || a.campaign != b.campaign || a.surveys != b.surveys || a.visited != b.visited || a.tools != b.tools || a.pickups != b.pickups ||
        a.openGates != b.openGates || a.seenGates != b.seenGates || a.ending != b.ending || a.voyageTicks != b.voyageTicks || a.delivered != b.delivered || a.rescues != b.rescues ||
        a.offerCursor != b.offerCursor || a.boardSeed != b.boardSeed || a.cargo != b.cargo)
        return false;
#if SH_FEATURE_MINES
    if (a.minesAlive != b.minesAlive)
        return false;
#endif
#if SH_FEATURE_SHOP
    if (a.airLevel != b.airLevel || a.hullLevel != b.hullLevel || a.sonarLevel != b.sonarLevel)
        return false;
#endif
#if SH_FEATURE_MULTI_HULL
    if (a.hullClass != b.hullClass)
        return false;
#endif
    return a.activeJob.kind == b.activeJob.kind && a.activeJob.mission == b.activeJob.mission && a.activeJob.from == b.activeJob.from && a.activeJob.to == b.activeJob.to &&
           a.activeJob.crates == b.activeJob.crates && a.activeJob.reward == b.activeJob.reward && a.activeJob.deadline == b.activeJob.deadline && a.activeJob.elapsed == b.activeJob.elapsed;
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

inline void writeU32(FakeStorage &storage, uint16_t address, uint32_t value) {
    storage.bytes[address] = static_cast<uint8_t>(value);
    storage.bytes[address + 1] = static_cast<uint8_t>(value >> 8);
    storage.bytes[address + 2] = static_cast<uint8_t>(value >> 16);
    storage.bytes[address + 3] = static_cast<uint8_t>(value >> 24);
}

inline void refreshCrc(FakeStorage &storage, uint8_t slot) {
    const uint16_t base = voyage_save::slotAddress(slot);
    const uint16_t length = readU16(storage, static_cast<uint16_t>(base + voyage_save::kLengthOffset));
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < voyage_save::kCrcOffset; ++i)
        crc = voyage_save::crc16Update(crc, storage.bytes[base + i]);
    for (uint16_t i = 0; i < length; ++i)
        crc = voyage_save::crc16Update(crc, storage.bytes[base + voyage_save::kPayloadOffset + i]);
    writeU16(storage, static_cast<uint16_t>(base + voyage_save::kCrcOffset), crc);
}

inline uint16_t writeBoundaries(const Fixture &fixture, const FakeStorage &baseline) {
    FakeStorage scratch = baseline;
    scratch.resetCounters();
    scratch.failedAt = kNoFault;
    fixture.save(scratch);
    return scratch.changed;
}

inline bool allBytesAre(const FakeStorage &storage, uint16_t first, uint16_t last, uint8_t value) {
    for (uint16_t i = first; i <= last; ++i) {
        if (storage.bytes[i] != value)
            return false;
    }
    return true;
}

inline void seedSystemBytes(FakeStorage &storage) {
    for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
        storage.bytes[i] = static_cast<uint8_t>(0xA0 + i);
}

inline bool systemBytesUnchanged(const FakeStorage &storage, const uint8_t *snapshot) {
    for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i) {
        if (storage.bytes[i] != snapshot[i])
            return false;
    }
    return true;
}

inline void setValidPayload(Fixture &fixture) {
    fixture.state.credits = 0x1234;
    fixture.state.tools = static_cast<uint8_t>(voyage_save::toolsMask());
    fixture.state.pickups = static_cast<uint8_t>(voyage_save::pickupsMask() & 0xA5u);
    fixture.state.openGates = static_cast<uint16_t>(voyage_save::gatesMask() & 0x155u);
    fixture.state.seenGates = static_cast<uint16_t>(voyage_save::gatesMask() & 0x0AAu);
    fixture.state.campaign = campaign::kMilestoneFirst;
    fixture.state.surveys = campaign::kSurveyMask;
    fixture.state.visited = 0x0A;
    fixture.state.ending = 0;
    fixture.state.offerCursor = 0xBEEF;
    fixture.state.voyageTicks = 0x12345678u;
    fixture.state.delivered = 0x0102;
    fixture.state.rescues = 0x0304;
#if SH_FEATURE_MINES
    fixture.state.minesAlive = 0x55;
#endif
#if SH_FEATURE_SHOP
    fixture.state.airLevel = 1;
    fixture.state.hullLevel = 2;
    fixture.state.sonarLevel = 3;
#endif
#if SH_FEATURE_MULTI_HULL
    fixture.state.hullClass = static_cast<uint8_t>(roster::kLeviathan);
#endif
}

inline bool makeFirstJob(Fixture &fixture) {
    fixture.reset();
    return campaign::accept(fixture.state, campaign::kFirst);
}

}   // namespace voyage_save_test

inline void VoyageSaveSuite(TestRunner &runner) {
    using namespace voyage_save_test;

    {
        Test test("CRC-16/CCITT-FALSE vector and frozen slot layout");
        const uint8_t vector[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        test.assertEqual(voyage_save::crc16Bytes(vector, 9), 0x29B1, "CRC check vector");
        test.assertEqual(voyage_save::kSystemBytes, 16, "system prefix");
        test.assertEqual(voyage_save::kSlotAAddress, 16, "slot A start");
        test.assertEqual(voyage_save::kSlotBAddress, 336, "slot B start");
        test.assertEqual(voyage_save::kSlotBytes, 320, "slot width");
        test.assertEqual(voyage_save::kPayloadCapacity, 303, "payload capacity");
        test.assertEqual(voyage_save::kCommitOffset, 319, "commit byte offset");
        test.assertEqual(voyage_save::kSlotEnd, 656, "slot end");
        test.assertEqual(voyage_save::kScalarBytes, 40, "scalar payload width");
        test.assertEqual(voyage_save::kPayloadBytes, static_cast<uint16_t>(40 + (SH_FEATURE_FOG_CHART ? world::CHART_BYTES : 0)), "payload width follows chart bit");
        runner.add(test);
    }

    {
        Test test("legacy chart block remains readable without chart_save");
        uint8_t legacy[260];
        for (uint16_t i = 0; i < sizeof(legacy); ++i)
            legacy[i] = static_cast<uint8_t>(i * 11u + 3u);
        legacy[0] = voyage_save::kLegacyMagic0;
        legacy[1] = voyage_save::kLegacyMagic1;
        legacy[2] = voyage_save::kLegacyVersion;
        uint8_t crc = 0;
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            crc = voyage_save::crc8Update(crc, legacy[voyage_save::kLegacyDataOffset + i]);
        legacy[voyage_save::kLegacyCrcOffset] = crc;
        test.assertEqual(voyage_save::kLegacyTotalBytes, 260, "legacy block size");
        test.assertEqual(crc, legacy[voyage_save::kLegacyCrcOffset], "legacy CRC is self-consistent");
        runner.add(test);
    }

    {
        Test test("first save uses slot B and never writes system bytes");
        FakeStorage storage(0xFF);
        seedSystemBytes(storage);
        uint8_t system[voyage_save::kSystemBytes];
        for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
            system[i] = storage.bytes[i];

        Fixture fixture;
        setValidPayload(fixture);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "first save succeeds");
        test.assertEqual(storage.systemWrites, 0, "no system-prefix update");
        test.assertTrue(systemBytesUnchanged(storage, system), "bytes 0-15 are unchanged");
        test.assertTrue(allBytesAre(storage, voyage_save::kSlotAAddress, voyage_save::kSlotBAddress - 1, 0xFF), "slot A stays blank on the first save");

        const voyage_save::Inventory inventory = voyage_save::inspect(storage);
        test.assertEqual(inventory.slot[0].status, voyage_save::kSlotBlank, "slot A is blank");
        test.assertEqual(inventory.slot[1].status, voyage_save::kSlotValid, "slot B is valid");
        test.assertEqual(inventory.slot[1].sequence, 1, "first sequence is one");
        test.assertTrue(!storage.written[0], "no write below the system prefix");

        Fixture restored;
        restored.reset();
        storage.resetCounters();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "first save loads");
        test.assertEqual(storage.updateCalls, 0, "load never writes");
        test.assertEqual(storage.writes, 0, "load never calls write");
        test.assertTrue(checkpointMatches(restored.state, fixture.state), "all saved fields round-trip");
        test.assertEqual(restored.state.cargo, 0, "no active job means no cargo");
#if SH_FEATURE_FOG_CHART
        test.assertTrue(chartEmpty(restored.chart), "empty chart round-trips");
#endif
        runner.add(test);
    }

    {
        Test test("payload is encoded field by field at the frozen offsets");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "layout save");
        const uint16_t base = voyage_save::kSlotBAddress;
        const uint16_t payload = static_cast<uint16_t>(base + voyage_save::kPayloadOffset);

        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset], voyage_save::kMagic0, "magic 0");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 1], voyage_save::kMagic1, "magic 1");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 2], voyage_save::kMagic2, "magic 2");
        test.assertEqual(storage.bytes[base + voyage_save::kMagicOffset + 3], voyage_save::kMagic3, "magic 3");
        test.assertEqual(storage.bytes[base + voyage_save::kSchemaOffset], voyage_save::kSchemaVersion, "schema");
        test.assertEqual(storage.bytes[base + voyage_save::kLayoutOffset], world::LAYOUT_VERSION, "world layout");
        test.assertEqual(storage.bytes[base + voyage_save::kContentOffset], voyage_save::kCampaignContentVersion, "content version");
        test.assertEqual(storage.bytes[base + voyage_save::kSignatureOffset], voyage_save::kProfileSignature, "signature");
        test.assertEqual(readU32(storage, static_cast<uint16_t>(base + voyage_save::kSequenceOffset)), 1, "sequence");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(base + voyage_save::kLengthOffset)), voyage_save::kPayloadBytes, "payload length");

        test.assertEqual(storage.bytes[payload + 0], campaign::kTidewall, "berth offset 0");
#if SH_FEATURE_MULTI_HULL
        test.assertEqual(storage.bytes[payload + 1], roster::kLeviathan, "hull class offset 1");
#else
        test.assertEqual(storage.bytes[payload + 1], roster::kHauler, "single-hull class offset 1");
#endif
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 2)), 0x1234, "credits offset 2");
#if SH_FEATURE_SHOP
        test.assertEqual(storage.bytes[payload + 4], 1, "air level offset 4");
        test.assertEqual(storage.bytes[payload + 5], 2, "hull level offset 5");
        test.assertEqual(storage.bytes[payload + 6], 3, "sonar level offset 6");
#else
        test.assertEqual(storage.bytes[payload + 4], 0, "no-shop air level offset 4");
        test.assertEqual(storage.bytes[payload + 5], 0, "no-shop hull level offset 5");
        test.assertEqual(storage.bytes[payload + 6], 0, "no-shop sonar level offset 6");
#endif
        test.assertEqual(storage.bytes[payload + 7], voyage_save::toolsMask(), "tools offset 7");
        test.assertEqual(storage.bytes[payload + 8], static_cast<uint8_t>(voyage_save::pickupsMask() & 0xA5u), "pickups offset 8");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 9)), static_cast<uint16_t>(voyage_save::gatesMask() & 0x155u), "open gates offset 9");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 11)), static_cast<uint16_t>(voyage_save::gatesMask() & 0x0AAu), "seen gates offset 11");
        test.assertEqual(storage.bytes[payload + 13], campaign::kMilestoneFirst, "milestones offset 13");
        test.assertEqual(storage.bytes[payload + 14], campaign::kSurveyMask, "surveys offset 14");
        test.assertEqual(storage.bytes[payload + 15], 0x0A, "visited offset 15");
        test.assertEqual(storage.bytes[payload + 16], game::kJobKindNone, "job kind offset 16");
        test.assertEqual(storage.bytes[payload + 17], game::kMissionNone, "mission offset 17");
        test.assertEqual(storage.bytes[payload + 18], game::kJobNone, "origin offset 18");
        test.assertEqual(storage.bytes[payload + 19], game::kJobNone, "destination offset 19");
        test.assertEqual(storage.bytes[payload + 20], 0, "job load offset 20");
        test.assertEqual(storage.bytes[payload + 21], 0, "job reward offset 21");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 22)), 0, "job deadline offset 22");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 24)), 0, "job age offset 24");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 26)), 0xBEEF, "offer cursor offset 26");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 28)), 0, "board seed offset 28");
#if SH_FEATURE_MINES
        test.assertEqual(storage.bytes[payload + 30], 0x55, "mines offset 30");
#else
        test.assertEqual(storage.bytes[payload + 30], 0, "no-mines offset 30");
#endif
        test.assertEqual(readU32(storage, static_cast<uint16_t>(payload + 31)), 0x12345678u, "voyage ticks offset 31");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 35)), 0x0102, "delivered offset 35");
        test.assertEqual(readU16(storage, static_cast<uint16_t>(payload + 37)), 0x0304, "rescues offset 37");
        test.assertEqual(storage.bytes[payload + 39], 0, "ending offset 39");
#if SH_FEATURE_FOG_CHART
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            test.assertEqual(storage.bytes[payload + voyage_save::kScalarBytes + i], fixture.chart.explored[i], "chart follows scalar bytes");
#endif
        runner.add(test);
    }

    {
        Test test("second save uses slot A and load selects the unchanged newest slot");
        FakeStorage storage(0xFF);
        Fixture first;
        setValidPayload(first);
        test.assertEqual(first.save(storage), voyage_save::kSaved, "first save");
        first.state.credits = 0x4321;
        first.state.offerCursor = 0xCAFE;
        test.assertEqual(first.save(storage), voyage_save::kSaved, "second save");

        voyage_save::Inventory inventory = voyage_save::inspect(storage);
        test.assertEqual(inventory.slot[0].status, voyage_save::kSlotValid, "slot A valid");
        test.assertEqual(inventory.slot[1].status, voyage_save::kSlotValid, "slot B valid");
        test.assertEqual(inventory.slot[0].sequence, 2, "slot A sequence two");
        test.assertEqual(inventory.slot[1].sequence, 1, "slot B sequence one");
        test.assertEqual(inventory.newestSlot, 0, "slot A is newest");

        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "newest loads");
        test.assertEqual(restored.state.credits, 0x4321, "newest credits");
        test.assertEqual(restored.state.offerCursor, 0xCAFE, "newest cursor");
        test.assertEqual(restored.state.offerSerial, restored.state.offerCursor, "working serial restores from cursor");
        runner.add(test);
    }

    {
        Test test("sequence wrap selects the logical newest and wraps to zero");
        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        fixture.save(storage);
        fixture.state.credits = 99;
        fixture.save(storage);

        writeU32(storage, voyage_save::kSlotBAddress + voyage_save::kSequenceOffset, 0xFFFFFFFEu);
        refreshCrc(storage, 1);
        writeU32(storage, voyage_save::kSlotAAddress + voyage_save::kSequenceOffset, 0xFFFFFFFFu);
        refreshCrc(storage, 0);

        voyage_save::Inventory inventory = voyage_save::inspect(storage);
        test.assertEqual(inventory.newestSlot, 0, "0xFFFFFFFF is newer than 0xFFFFFFFE");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "wrap-aware load");
        test.assertEqual(restored.state.credits, 99, "wrap-aware payload");

        fixture.state.credits = 123;
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "save after sequence wrap");
        test.assertEqual(voyage_save::inspect(storage).slot[1].sequence, 0, "next sequence wraps to zero");
        test.assertEqual(fixture.load(storage), voyage_save::kLoaded, "wrapped slot loads");
        test.assertEqual(fixture.state.credits, 123, "wrapped slot payload");
        runner.add(test);
    }

    {
        Test test("blank, torn, corrupt and incompatible slots are distinguished");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.credits = 7;
        test.assertEqual(probe.load(blank), voyage_save::kNoSave, "blank storage is missing");
        test.assertEqual(probe.state.credits, 7, "missing save leaves state untouched");
        test.assertEqual(voyage_save::inspect(blank).slot[0].status, voyage_save::kSlotBlank, "slot A blank");
        test.assertEqual(voyage_save::inspect(blank).slot[1].status, voyage_save::kSlotBlank, "slot B blank");

        FakeStorage base(0xFF);
        Fixture saved;
        setValidPayload(saved);
        test.assertEqual(saved.save(base), voyage_save::kSaved, "baseline save");

        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress] ^= 0xFF;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "torn magic with committed byte is corrupt");
            test.assertEqual(probe.load(storage), voyage_save::kCorruptSave, "torn magic load result");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kCommitOffset] = 0;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "valid magic with in-progress commit is corrupt");
            test.assertEqual(probe.load(storage), voyage_save::kCorruptSave, "torn commit load result");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kPayloadOffset + 3] ^= 0x01;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "payload CRC mismatch is corrupt");
        }
        {
            FakeStorage storage = base;
            writeU16(storage, voyage_save::kSlotBAddress + voyage_save::kLengthOffset, 41);
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "wrong payload length is corrupt");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kSignatureOffset] ^= 0x80;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotIncompatible, "signature mismatch is incompatible");
            test.assertEqual(probe.load(storage), voyage_save::kIncompatibleSave, "incompatible load result");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kSchemaOffset] = 2;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotIncompatible, "schema mismatch is incompatible");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kLayoutOffset] ^= 0x01;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotIncompatible, "layout mismatch is incompatible");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kContentOffset] = 2;
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotIncompatible, "content mismatch is incompatible");
        }
        {
            FakeStorage storage = base;
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kPayloadOffset] = 9;
            refreshCrc(storage, 1);
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "out-of-range berth is semantically corrupt");
        }
        runner.add(test);
    }

    {
        Test test("corrupt newest slot falls back to the older checkpoint and is replaced next");
        FakeStorage storage(0xFF);
        Fixture older;
        setValidPayload(older);
        older.state.credits = 111;
        test.assertEqual(older.save(storage), voyage_save::kSaved, "older save");
        Fixture newer;
        setValidPayload(newer);
        newer.state.credits = 222;
        test.assertEqual(newer.save(storage), voyage_save::kSaved, "newer save");

        storage.bytes[voyage_save::kSlotAAddress + voyage_save::kPayloadOffset] = 9;
        refreshCrc(storage, 0);
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "older slot loads after newest corruption");
        test.assertEqual(restored.state.credits, 111, "older payload survives");

        newer.state.credits = 333;
        test.assertEqual(newer.save(storage), voyage_save::kSaved, "one valid slot means the corrupt slot is replaced");
        test.assertEqual(voyage_save::inspect(storage).slot[0].sequence, 2, "replacement uses newest valid + 1");
        test.assertEqual(newer.load(storage), voyage_save::kLoaded, "replacement loads");
        test.assertEqual(newer.state.credits, 333, "replacement payload");
        runner.add(test);
    }

    {
        Test test("every interrupted write boundary keeps one complete checkpoint");
        FakeStorage baseline(0xFF);
        Fixture oldState;
        setValidPayload(oldState);
        oldState.state.credits = 444;
        test.assertEqual(oldState.save(baseline), voyage_save::kSaved, "baseline save");
        seedSystemBytes(baseline);
        uint8_t system[voyage_save::kSystemBytes];
        for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
            system[i] = baseline.bytes[i];

        Fixture nextState;
        setValidPayload(nextState);
        nextState.state.credits = 555;
        const uint16_t boundaries = writeBoundaries(nextState, baseline);
        const uint16_t expectedWrites = static_cast<uint16_t>(1 + voyage_save::kPayloadBytes + 10 + 2 + 4 + 1);
        test.assertTrue(boundaries > voyage_save::kScalarBytes, "checkpoint writes the payload");
        test.assertTrue(boundaries <= expectedWrites, "checkpoint never exceeds the frozen transaction");

        bool allFailedCleanly = true;
        bool allLoadOld = true;
        bool noSystemWrites = true;
        for (uint16_t fail = 0; fail < boundaries; ++fail) {
            FakeStorage storage = baseline;
            storage.resetCounters();
            storage.failedAt = fail;
            if (nextState.save(storage) != voyage_save::kWriteFailed)
                allFailedCleanly = false;
            if (storage.systemWrites != 0 || !systemBytesUnchanged(storage, system))
                noSystemWrites = false;

            Fixture restored;
            restored.reset();
            if (restored.load(storage) != voyage_save::kLoaded || !checkpointMatches(restored.state, oldState.state))
                allLoadOld = false;
        }
        test.assertTrue(allFailedCleanly, "every interrupted save reports failure, never optimistic success");
        test.assertTrue(allLoadOld, "the preceding valid slot is complete after every interruption boundary");
        test.assertTrue(noSystemWrites, "interrupted writes still never touch bytes 0-15");

        FakeStorage complete = baseline;
        complete.resetCounters();
        test.assertEqual(nextState.save(complete), voyage_save::kSaved, "uninterrupted save succeeds");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(complete), voyage_save::kLoaded, "uninterrupted save loads");
        test.assertTrue(checkpointMatches(restored.state, nextState.state), "uninterrupted payload");
        runner.add(test);
    }

    {
        Test test("interrupted delivery recovers exactly pre or post, with no duplicate reward");
        FakeStorage baseline(0xFF);
        Fixture pre;
        test.assertTrue(makeFirstJob(pre), "FIRST accepted at Tidewall");
        test.assertEqual(pre.state.credits, 0, "pre-delivery has no payment");
        test.assertEqual(pre.state.cargo, game::kJobCrates, "pre-delivery cargo is loaded");
        test.assertEqual(pre.save(baseline), voyage_save::kSaved, "pre-delivery checkpoint saved");

        Fixture post = pre;
        test.assertTrue(travelTo(post.state, campaign::kBenthos), "dock at Benthos");
        test.assertEqual(post.state.credits, campaign::missionReward(campaign::kFirst), "delivery paid once");
        test.assertEqual(post.state.cargo, 0, "delivery clears cargo");
        test.assertTrue(campaign::missionCompleted(post.state, campaign::kFirst), "delivery sets FIRST");
        test.assertTrue(!game::hasActiveJob(post.state), "delivery clears the active job");

        const uint16_t boundaries = writeBoundaries(post, baseline);
        const uint16_t expectedWrites = static_cast<uint16_t>(1 + voyage_save::kPayloadBytes + 10 + 2 + 4 + 1);
        test.assertTrue(boundaries > voyage_save::kScalarBytes, "post-delivery writes the payload");
        test.assertTrue(boundaries <= expectedWrites, "post-delivery never exceeds the frozen transaction");

        bool exact = true;
        for (uint16_t fail = 0; fail < boundaries; ++fail) {
            FakeStorage storage = baseline;
            storage.resetCounters();
            storage.failedAt = fail;
            test.assertEqual(post.save(storage), voyage_save::kWriteFailed, "interrupted delivery reports failure");

            Fixture restored;
            restored.reset();
            if (restored.load(storage) != voyage_save::kLoaded)
                exact = false;
            const bool isPre =
                restored.state.credits == 0 && restored.state.cargo == game::kJobCrates && game::hasActiveJob(restored.state) && !campaign::missionCompleted(restored.state, campaign::kFirst);
            const bool isPost = restored.state.credits == campaign::missionReward(campaign::kFirst) && restored.state.cargo == 0 && !game::hasActiveJob(restored.state) &&
                                campaign::missionCompleted(restored.state, campaign::kFirst);
            if (!isPre && !isPost)
                exact = false;
        }
        test.assertTrue(exact, "no interrupted delivery can combine cargo, payment and completion");

        FakeStorage complete = baseline;
        complete.resetCounters();
        test.assertEqual(post.save(complete), voyage_save::kSaved, "delivery checkpoint completes");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(complete), voyage_save::kLoaded, "delivery checkpoint loads");
        test.assertEqual(restored.state.credits, campaign::missionReward(campaign::kFirst), "payment is present once");
        test.assertEqual(restored.state.cargo, 0, "cargo is clear");
        test.assertTrue(campaign::missionCompleted(restored.state, campaign::kFirst), "completion is present once");

        test.assertEqual(post.save(complete), voyage_save::kSaved, "repeat save does not re-run delivery");
        Fixture again;
        again.reset();
        test.assertEqual(again.load(complete), voyage_save::kLoaded, "repeat save loads");
        test.assertEqual(again.state.credits, campaign::missionReward(campaign::kFirst), "repeat load pays nothing");
        runner.add(test);
    }

    {
        Test test("legacy chart imports only on the explicit first new voyage");
#if SH_FEATURE_FOG_CHART
        FakeStorage storage(0xFF);
        chart::State legacyChart;
        fillPattern(legacyChart.explored, world::CHART_BYTES, 19);
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            storage.bytes[voyage_save::kLegacyDataOffset + i] = legacyChart.explored[i];
        storage.bytes[0] = voyage_save::kLegacyMagic0;
        storage.bytes[1] = voyage_save::kLegacyMagic1;
        storage.bytes[2] = voyage_save::kLegacyVersion;
        uint8_t crc = 0;
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            crc = voyage_save::crc8Update(crc, storage.bytes[voyage_save::kLegacyDataOffset + i]);
        storage.bytes[voyage_save::kLegacyCrcOffset] = crc;
        uint8_t system[voyage_save::kSystemBytes];
        for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
            system[i] = storage.bytes[i];

        Fixture fresh;
        test.assertEqual(fresh.newVoyage(storage, voyage_save::kKeepLegacyChart), voyage_save::kSaved, "first new voyage with legacy chart saves");
        test.assertEqual(storage.systemWrites, 0, "migration never writes system bytes");
        test.assertTrue(systemBytesUnchanged(storage, system), "system prefix unchanged by migration");
        test.assertEqual(voyage_save::inspect(storage).slot[0].status, voyage_save::kSlotBlank, "legacy bytes do not make slot A a committed voyage");
        test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotValid, "first voyage still starts in slot B");
        test.assertTrue(sameChart(fresh.chart, legacyChart), "legacy mask copied exactly");
        test.assertTrue(!storage.written[0], "no write in the legacy block");
        test.assertEqual(fresh.state.campaign, 0, "migration grants no campaign progress");
        test.assertEqual(fresh.state.credits, 0, "migration grants no credits");
        test.assertEqual(fresh.state.tools, 0, "migration grants no tools");
        test.assertEqual(fresh.state.pickups, 0, "migration grants no claims");
        test.assertEqual(fresh.state.docked, campaign::kTidewall, "new voyage starts at Tidewall");

        FakeStorage bad(0xFF);
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            bad.bytes[voyage_save::kLegacyDataOffset + i] = static_cast<uint8_t>(i + 5u);
        bad.bytes[0] = voyage_save::kLegacyMagic0;
        bad.bytes[1] = voyage_save::kLegacyMagic1;
        bad.bytes[2] = voyage_save::kLegacyVersion;
        bad.bytes[voyage_save::kLegacyCrcOffset] = 0;   // deliberately wrong
        Fixture invalidFresh;
        test.assertEqual(invalidFresh.newVoyage(bad, voyage_save::kKeepLegacyChart), voyage_save::kSaved, "invalid legacy still creates a voyage");
        test.assertTrue(chartEmpty(invalidFresh.chart), "invalid legacy starts with an empty chart");

        FakeStorage existing(0xFF);
        Fixture oldVoyage;
        setValidPayload(oldVoyage);
        test.assertEqual(oldVoyage.save(existing), voyage_save::kSaved, "existing voyage");
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            existing.bytes[voyage_save::kLegacyDataOffset + i] = static_cast<uint8_t>(i * 3u + 1u);
        existing.bytes[0] = voyage_save::kLegacyMagic0;
        existing.bytes[1] = voyage_save::kLegacyMagic1;
        existing.bytes[2] = voyage_save::kLegacyVersion;
        uint8_t existingCrc = 0;
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            existingCrc = voyage_save::crc8Update(existingCrc, existing.bytes[voyage_save::kLegacyDataOffset + i]);
        existing.bytes[voyage_save::kLegacyCrcOffset] = existingCrc;

        Fixture replacement;
        test.assertEqual(replacement.newVoyage(existing, voyage_save::kKeepLegacyChart), voyage_save::kSaved, "replacement new voyage saves");
        test.assertTrue(chartEmpty(replacement.chart), "legacy chart is not imported once a voyage exists");
#else
        FakeStorage storage(0xFF);
        Fixture fresh;
        test.assertEqual(fresh.newVoyage(storage, voyage_save::kKeepLegacyChart), voyage_save::kSaved, "no-chart new voyage saves");
        test.assertEqual(voyage_save::kPayloadBytes, 40, "no-chart payload is 40 bytes");
        test.assertTrue(allBytesAre(storage, voyage_save::kSlotAAddress, voyage_save::kSlotBAddress - 1, 0xFF), "no-chart first voyage starts in slot B");
#endif
        runner.add(test);
    }

    {
        Test test("new voyage replacement preserves the prior valid slot on interruption");
        FakeStorage baseline(0xFF);
        Fixture oldVoyage;
        setValidPayload(oldVoyage);
        oldVoyage.state.credits = 808;
        test.assertEqual(oldVoyage.save(baseline), voyage_save::kSaved, "existing voyage");

        Fixture fresh;
        fresh.state.credits = 1010;
        Fixture boundaryProbe = fresh;
        FakeStorage boundaryStorage = baseline;
        boundaryStorage.resetCounters();
        boundaryStorage.failedAt = kNoFault;
        boundaryProbe.newVoyage(boundaryStorage, voyage_save::kDropLegacyChart);
        const uint16_t boundaries = boundaryStorage.changed;
        bool oldSurvives = true;
        for (uint16_t fail = 0; fail < boundaries; ++fail) {
            FakeStorage storage = baseline;
            storage.resetCounters();
            storage.failedAt = fail;
            test.assertEqual(fresh.newVoyage(storage, voyage_save::kDropLegacyChart), voyage_save::kWriteFailed, "interrupted new voyage reports failure");
            Fixture restored;
            restored.reset();
            if (restored.load(storage) != voyage_save::kLoaded || !checkpointMatches(restored.state, oldVoyage.state))
                oldSurvives = false;
        }
        test.assertTrue(oldSurvives, "existing checkpoint survives every replacement boundary");

        FakeStorage complete = baseline;
        complete.resetCounters();
        test.assertEqual(fresh.newVoyage(complete, voyage_save::kDropLegacyChart), voyage_save::kSaved, "replacement completes");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(complete), voyage_save::kLoaded, "replacement loads");
        test.assertTrue(checkpointMatches(restored.state, fresh.state), "replacement payload");
        runner.add(test);
    }

    {
        Test test("service berth checkpoints require their restoration milestone");
        Fixture fixture;
        fixture.state.docked = static_cast<int8_t>(campaign::kPumpRefuge);
        fixture.state.campaign = 0;
        FakeStorage storage(0xFF);
        storage.resetCounters();
        test.assertEqual(fixture.save(storage), voyage_save::kInvalidState, "Pump Refuge without PUMP is invalid");
        test.assertEqual(storage.updateCalls, 0, "invalid service checkpoint writes nothing");

        fixture.state.campaign = campaign::kMilestonePump;
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "Pump Refuge after PUMP saves");
        Fixture loaded;
        loaded.reset();
        test.assertEqual(loaded.load(storage), voyage_save::kLoaded, "Pump Refuge checkpoint loads");
        test.assertEqual(loaded.state.docked, campaign::kPumpRefuge, "Pump Refuge berth restored");

        fixture.state.docked = static_cast<int8_t>(campaign::kLowerGantry);
        fixture.state.campaign = campaign::kMilestonePump;
        test.assertEqual(fixture.save(storage), voyage_save::kInvalidState, "Lower Gantry without LIFT is invalid");
        fixture.state.campaign = static_cast<uint8_t>(campaign::kMilestonePump | campaign::kMilestoneLift);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "Lower Gantry after LIFT saves");
        test.assertEqual(loaded.load(storage), voyage_save::kLoaded, "Lower Gantry checkpoint loads");
        test.assertEqual(loaded.state.docked, campaign::kLowerGantry, "Lower Gantry berth restored");
        runner.add(test);
    }

    {
        Test test("invalid state combinations are rejected before any storage write");
        Fixture base;
        setValidPayload(base);
        FakeStorage storage(0xFF);

        const auto expectInvalid = [&](const Fixture &fixture, const char *message) {
            FakeStorage probe = storage;
            probe.resetCounters();
            Test local(message);
            local.assertEqual(fixture.save(probe), voyage_save::kInvalidState, message);
            local.assertEqual(probe.updateCalls, 0, "invalid save writes nothing");
            runner.add(local);
        };

        Fixture bad = base;
        bad.state.tools = 0x80;
        expectInvalid(bad, "out-of-mask tools");

        bad = base;
        bad.state.openGates = 0x200;
        expectInvalid(bad, "out-of-mask open gates");

        bad = base;
        bad.state.seenGates = 0x200;
        expectInvalid(bad, "out-of-mask seen gates");

        bad = base;
        bad.state.visited = 0x10;
        expectInvalid(bad, "out-of-mask visited settlements");

        bad = base;
        bad.state.ending = 1;
        expectInvalid(bad, "ending without HOME");

        bad = base;
        bad.state.campaign = campaign::kMilestoneHome;
        expectInvalid(bad, "HOME without BEACON");

        bad = base;
        bad.state.boardSeed = 1;
        expectInvalid(bad, "schema 1 board seed must be zero");

        bad = base;
        bad.state.docked = -1;
        expectInvalid(bad, "midwater checkpoint");

        bad = base;
        bad.state.docked = static_cast<int8_t>(campaign::kBerthCount);
        expectInvalid(bad, "out-of-range berth");

        bad = base;
        bad.state.activeJob.kind = game::kJobKindOrdinary;
        bad.state.activeJob.mission = game::kMissionNone;
        bad.state.activeJob.from = campaign::kTidewall;
        bad.state.activeJob.to = campaign::kBenthos;
        bad.state.activeJob.crates = game::kJobCrates;
        bad.state.activeJob.reward = 1;
        bad.state.activeJob.deadline = 0;
        bad.state.activeJob.elapsed = 0;
        expectInvalid(bad, "ordinary job with the wrong route reward");

#if SH_FEATURE_SHOP
        bad = base;
        bad.state.airLevel = 4;
        expectInvalid(bad, "level above the shop cap");
#endif
        runner.add(test);
    }

    {
        Test test("profile signature and absent chart/shop/roster behavior");
        uint8_t expected = 0;
#if SH_FEATURE_FOG_CHART
        expected |= voyage_save::kSignatureFogChart;
#endif
#if SH_FEATURE_SONAR
        expected |= voyage_save::kSignatureSonar;
#endif
#if SH_FEATURE_MINES
        expected |= voyage_save::kSignatureMines;
#endif
#if SH_FEATURE_SHOP
        expected |= voyage_save::kSignatureShop;
#endif
#if SH_FEATURE_HULL_DAMAGE
        expected |= voyage_save::kSignatureHullDamage;
#endif
#if SH_FEATURE_MULTI_HULL
        expected |= voyage_save::kSignatureMultiHull;
#endif
#if SH_FEATURE_EXPLORATION
        expected |= voyage_save::kSignatureExploration;
#endif
        test.assertEqual(voyage_save::kProfileSignature, expected, "profile signature bits");
        test.assertEqual(SH_FEATURE_SAVE_CHART, 0, "voyage save excludes chart save");
#if SH_FEATURE_FOG_CHART
        test.assertEqual(voyage_save::kPayloadBytes, 296, "chart profile payload is 296 B");
        test.assertEqual(voyage_save::kChartBytes, world::CHART_BYTES, "chart bytes are part of the payload");
#else
        test.assertEqual(voyage_save::kPayloadBytes, 40, "no-chart payload is 40 B");
        test.assertEqual(voyage_save::kChartBytes, 0, "no chart bytes");
#endif
#if !SH_FEATURE_SHOP
        {
            FakeStorage storage(0xFF);
            Fixture fixture;
            setValidPayload(fixture);
            test.assertEqual(fixture.save(storage), voyage_save::kSaved, "no-shop save");
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kPayloadOffset + 4] = 1;
            refreshCrc(storage, 1);
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "no-shop level must be zero");
        }
#endif
#if !SH_FEATURE_MULTI_HULL
        {
            FakeStorage storage(0xFF);
            Fixture fixture;
            setValidPayload(fixture);
            test.assertEqual(fixture.save(storage), voyage_save::kSaved, "single-hull save");
            storage.bytes[voyage_save::kSlotBAddress + voyage_save::kPayloadOffset + 1] = static_cast<uint8_t>(roster::kLeviathan);
            refreshCrc(storage, 1);
            test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotCorrupt, "single-hull class must be HAULER");
        }
#endif

        FakeStorage storage(0xFF);
        Fixture fixture;
        setValidPayload(fixture);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "signature baseline save");
        storage.bytes[voyage_save::kSlotBAddress + voyage_save::kSignatureOffset] ^= 0x01;
        test.assertEqual(voyage_save::inspect(storage).slot[1].status, voyage_save::kSlotIncompatible, "changed profile signature is incompatible");
        runner.add(test);
    }

    {
        Test test("load validates before mutating live state or chart");
        FakeStorage blank(0xFF);
        Fixture probe;
        probe.reset();
        probe.state.credits = 4242;
        probe.state.campaign = campaign::kMilestoneFirst;
#if SH_FEATURE_FOG_CHART
        fillPattern(probe.chart.explored, world::CHART_BYTES, 71);
        chart::State chartBefore = probe.chart;
#endif
        const State stateBefore = probe.state;
        test.assertEqual(probe.load(blank), voyage_save::kNoSave, "blank load");
        test.assertTrue(checkpointMatches(probe.state, stateBefore), "blank load leaves state");
#if SH_FEATURE_FOG_CHART
        test.assertTrue(sameChart(probe.chart, chartBefore), "blank load leaves chart");
#endif

        FakeStorage storage(0xFF);
        Fixture source;
        setValidPayload(source);
        test.assertEqual(source.save(storage), voyage_save::kSaved, "valid baseline");
        storage.bytes[voyage_save::kSlotBAddress + voyage_save::kPayloadOffset + 1] ^= 0x01;
        refreshCrc(storage, 1);
        probe.reset();
        probe.state.credits = 4242;
        probe.state.campaign = campaign::kMilestoneFirst;
#if SH_FEATURE_FOG_CHART
        fillPattern(probe.chart.explored, world::CHART_BYTES, 71);
        chartBefore = probe.chart;
#endif
        const State corruptBefore = probe.state;
        test.assertEqual(probe.load(storage), voyage_save::kCorruptSave, "corrupt load");
        test.assertTrue(checkpointMatches(probe.state, corruptBefore), "corrupt load leaves state");
#if SH_FEATURE_FOG_CHART
        test.assertTrue(sameChart(probe.chart, chartBefore), "corrupt load leaves chart");
#endif
        runner.add(test);
    }

    {
        Test test("system prefix remains untouched across every save and load path");
        FakeStorage storage(0xFF);
        seedSystemBytes(storage);
        uint8_t system[voyage_save::kSystemBytes];
        for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
            system[i] = storage.bytes[i];

        Fixture fixture;
        setValidPayload(fixture);
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "first save");
        fixture.state.credits = 1;
        test.assertEqual(fixture.save(storage), voyage_save::kSaved, "second save");
        Fixture restored;
        restored.reset();
        test.assertEqual(restored.load(storage), voyage_save::kLoaded, "load");
        test.assertEqual(storage.systemWrites, 0, "no system writes");
        test.assertTrue(systemBytesUnchanged(storage, system), "system bytes unchanged");
        for (uint16_t i = 0; i < voyage_save::kSystemBytes; ++i)
            test.assertTrue(!storage.written[i], "never written below the system prefix");
        runner.add(test);
    }
}

#else

inline void VoyageSaveSuite(TestRunner &runner) {
    Test test("voyage save compiles out when SAVE_VOYAGE=0");
    test.assertEqual(SH_FEATURE_SAVE_VOYAGE, 0, "feature is off in this build");
    runner.add(test);
}

#endif   // SH_FEATURE_SAVE_VOYAGE
