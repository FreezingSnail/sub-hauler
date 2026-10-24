#pragma once

// Host tests for the optional persisted chart (M9 SAVE_CHART).
//
// The pure chart_save header is included in every profile so the feature-off
// build proves the include compiles to nothing. The enabled tests use an
// in-memory fake that mirrors the device adapter's read/write/update contract.

#include "../src/chart_save.hpp"
#include "../src/features.hpp"
#include "test.hpp"

#if SH_FEATURE_SAVE_CHART

namespace chart_save_test {

// Blank EEPROM is 0xFF. update() only changes a byte when the value differs,
// exactly like EEPROM.update, and counts both calls and physical changes.
struct FakeStorage {
    uint8_t bytes[chart_save::kTotalBytes];
    bool touched[chart_save::kTotalBytes];
    uint16_t reads;
    uint16_t writes;
    uint16_t updateCalls;
    uint16_t changed;
    uint16_t overflow;

    explicit FakeStorage(uint8_t fill) : reads(0), writes(0), updateCalls(0), changed(0), overflow(0) {
        for (uint16_t i = 0; i < chart_save::kTotalBytes; ++i) {
            bytes[i] = fill;
            touched[i] = false;
        }
    }

    uint8_t read(uint16_t address) {
        ++reads;
        if (address >= chart_save::kTotalBytes) {
            ++overflow;
            return 0xFF;
        }
        return bytes[address];
    }

    void write(uint16_t address, uint8_t value) {
        ++writes;
        if (address >= chart_save::kTotalBytes) {
            ++overflow;
            return;
        }
        bytes[address] = value;
    }

    void update(uint16_t address, uint8_t value) {
        ++updateCalls;
        if (address >= chart_save::kTotalBytes) {
            ++overflow;
            return;
        }
        touched[address] = true;
        if (bytes[address] != value) {
            bytes[address] = value;
            ++changed;
        }
    }

    void resetCounters() {
        reads = 0;
        writes = 0;
        updateCalls = 0;
        changed = 0;
        overflow = 0;
    }
};

inline void fillPattern(chart::State &state) {
    // Every explored byte is non-zero, including the 0xFF cells a fully
    // explored chart can hold, so a blank-fill fake cannot hide an update.
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        state.explored[i] = static_cast<uint8_t>((i * 73u) % 255u + 1u);
}

inline bool allZero(const chart::State &state) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (state.explored[i] != 0)
            return false;
    }
    return true;
}

inline bool sameMask(const chart::State &a, const chart::State &b) {
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
        if (a.explored[i] != b.explored[i])
            return false;
    }
    return true;
}

}   // namespace chart_save_test

inline void ChartSaveSuite(TestRunner &runner) {
    using namespace chart_save_test;

    static_assert(chart_save::kTotalBytes == 260, "4 B header plus the 256 B explored set");
    static_assert(chart_save::kDataAddress == 4, "explored data starts after the header");

    {
        Test test("blank EEPROM is ignored and leaves the chart empty");
        FakeStorage storage(0xFF);
        chart::State state;
        chart::reset(state);

        test.assertTrue(!chart_save::load(state, storage), "blank save is rejected");
        test.assertTrue(allZero(state), "rejected blank save does not touch the chart");
        test.assertEqual(storage.updateCalls, 0, "load never writes");
        test.assertEqual(storage.overflow, 0, "blank magic check stays in bounds");
        runner.add(test);
    }

    {
        Test test("bad magic, version, and checksum are rejected without a partial load");
        chart::State source;
        fillPattern(source);

        {
            FakeStorage storage(0x00);
            chart_save::save(source, storage);
            storage.bytes[0] ^= 0xFF;
            chart::State state;
            chart::reset(state);
            test.assertTrue(!chart_save::load(state, storage), "bad magic is rejected");
            test.assertTrue(allZero(state), "bad magic leaves the chart empty");
        }

        {
            FakeStorage storage(0x00);
            chart_save::save(source, storage);
            storage.bytes[2] = chart_save::kVersion + 1;
            chart::State state;
            chart::reset(state);
            test.assertTrue(!chart_save::load(state, storage), "bad version is rejected");
            test.assertTrue(allZero(state), "bad version leaves the chart empty");
        }

        {
            FakeStorage storage(0x00);
            chart_save::save(source, storage);
            storage.bytes[3] ^= 0x01;
            chart::State state;
            chart::reset(state);
            test.assertTrue(!chart_save::load(state, storage), "bad checksum is rejected");
            test.assertTrue(allZero(state), "bad checksum leaves the chart empty");
        }

        {
            FakeStorage storage(0x00);
            chart_save::save(source, storage);
            storage.bytes[chart_save::kDataAddress + 200] ^= 0x01;
            chart::State state;
            chart::reset(state);
            test.assertTrue(!chart_save::load(state, storage), "corrupt data is rejected");
            test.assertTrue(allZero(state), "corrupt data does not partially load");
        }

        runner.add(test);
    }

    {
        Test test("save round-trip restores the exact explored mask");
        chart::State source;
        fillPattern(source);

        FakeStorage storage(0x00);
        chart::State restored;
        chart::reset(restored);

        chart_save::save(source, storage);
        test.assertTrue(chart_save::load(restored, storage), "valid save loads");
        test.assertTrue(sameMask(source, restored), "every explored byte round-trips exactly");
        test.assertEqual(storage.bytes[0], chart_save::kMagic0, "magic byte 0");
        test.assertEqual(storage.bytes[1], chart_save::kMagic1, "magic byte 1");
        test.assertEqual(storage.bytes[2], chart_save::kVersion, "version byte");
        test.assertEqual(storage.bytes[3], chart_save::checksum(source), "checksum byte");
        runner.add(test);
    }

    {
        Test test("save updates every byte once, load writes nothing, repeat save is cheap");
        chart::State source;
        fillPattern(source);

        FakeStorage storage(0x00);
        test.assertEqual(storage.updateCalls, 0, "no updates before the close transition");

        chart_save::save(source, storage);
        test.assertEqual(storage.updateCalls, chart_save::kTotalBytes, "one update per save byte");
        test.assertEqual(storage.changed, chart_save::kTotalBytes, "blank storage changes every byte");
        test.assertEqual(storage.overflow, 0, "all save addresses are in bounds");
        for (uint16_t i = 0; i < chart_save::kTotalBytes; ++i)
            test.assertTrue(storage.touched[i], "every save address is written in order");

        // The layout is 4 header bytes followed by exactly 256 explored bytes:
        // the last touched address proves the data block does not overrun.
        test.assertTrue(storage.touched[chart_save::kDataAddress], "first explored byte is written");
        test.assertTrue(storage.touched[chart_save::kTotalBytes - 1], "last explored byte is written");

        storage.resetCounters();
        chart_save::save(source, storage);
        test.assertEqual(storage.updateCalls, chart_save::kTotalBytes, "repeat save still probes each byte");
        test.assertEqual(storage.changed, 0, "unchanged chart rewrites no physical byte");
        test.assertEqual(storage.overflow, 0, "repeat save stays in bounds");

        chart::State restored;
        chart::reset(restored);
        storage.resetCounters();
        test.assertTrue(chart_save::load(restored, storage), "saved block still loads");
        test.assertEqual(storage.updateCalls, 0, "load is read-only");
        test.assertEqual(storage.writes, 0, "load does not call write");
        runner.add(test);
    }
}

#else

inline void ChartSaveSuite(TestRunner &runner) {
    Test test("chart save compiles out when SAVE_CHART=0");
    test.assertEqual(SH_FEATURE_SAVE_CHART, 0, "feature is off in this build");
    runner.add(test);
}

#endif   // SH_FEATURE_SAVE_CHART
