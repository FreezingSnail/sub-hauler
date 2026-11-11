#pragma once

#include "../src/world.hpp"
#include "../src/generated/exploration.hpp"
#include "test.hpp"

namespace world_test {

// Synthetic reader: the same interface as the FX adapter, with full tables in
// host memory and read counters so cache refreshes are observable.
struct FakeWorld {
    uint8_t tiles[world::WORLD_BYTES];
    uint8_t chart[world::CHART_BYTES];
    uint16_t tileReads;
    uint16_t chartReads;

    FakeWorld() : tileReads(0), chartReads(0) {
        for (uint16_t i = 0; i < world::WORLD_BYTES; ++i)
            tiles[i] = 0;
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            chart[i] = 0;
    }

    uint8_t tileByte(uint16_t index) {
        ++tileReads;
        return tiles[index];
    }

    uint8_t chartByte(uint16_t index) {
        ++chartReads;
        return chart[index];
    }

    void setTile(int16_t tx, int16_t ty, bool solid) {
        if (!world::inTileBounds(tx, ty))
            return;
        const uint16_t index = world::tileByteIndex(tx, ty);
        const uint8_t mask = static_cast<uint8_t>(1u << (tx & 7));
        if (solid)
            tiles[index] |= mask;
        else
            tiles[index] &= static_cast<uint8_t>(~mask);
    }

    void setChart(int16_t cx, int16_t cy, bool water) {
        if (!world::inChartBounds(cx, cy))
            return;
        const uint16_t index = world::chartByteIndex(cx, cy);
        const uint8_t mask = static_cast<uint8_t>(1u << (cx & 7));
        if (water)
            chart[index] |= mask;
        else
            chart[index] &= static_cast<uint8_t>(~mask);
    }
};

}   // namespace world_test

inline void WorldSuite(TestRunner &runner) {
    // The runtime cache is a fixed 18x10 bitmap, not a world-sized grid.
    static_assert(world::CACHE_TILES_X == 18, "cache width is part of the budget");
    static_assert(world::CACHE_TILES_Y == 10, "cache height is part of the budget");
    static_assert(world::CACHE_BITS == 180, "cache holds 180 tiles");
    static_assert(world::CACHE_BYTES == 23, "cache bitmap is 23 bytes");
    static_assert(sizeof(world::TileCache) <= 32, "cache struct stays under 32 bytes");
    static_assert(sizeof(world::TileCache) < world::WORLD_BYTES, "cache must be far smaller than the baked world");

    {
        Test test("world tile and chart lookups with bounds");
        world_test::FakeWorld fake;
        fake.setTile(2, 3, true);
        fake.setTile(3, 3, false);

        test.assertTrue(world::tileSolid(fake, 2, 3), "solid bit reads as solid");
        test.assertTrue(!world::tileSolid(fake, 3, 3), "clear bit reads as open");

        const uint16_t readsBeforeBounds = fake.tileReads;
        test.assertTrue(world::tileSolid(fake, -1, 0), "negative x is sealed");
        test.assertTrue(world::tileSolid(fake, world::TILES_X, 0), "east edge is sealed");
        test.assertTrue(world::tileSolid(fake, 0, world::TILES_Y), "south edge is sealed");
        test.assertTrue(world::tileSolid(fake, 0, -1), "negative y is sealed");
        test.assertEqual(fake.tileReads, readsBeforeBounds, "bounds checks never read cart");

        fake.setChart(5, 4, true);
        test.assertTrue(world::chartCellWater(fake, 5, 4), "chart water bit");
        test.assertTrue(!world::chartCellWater(fake, 6, 4), "adjacent land bit");
        test.assertTrue(world::tileWater(fake, 27, 22), "tile maps to its 5x5 chart cell");
        test.assertTrue(!world::tileWater(fake, 30, 22), "next chart cell is land");

        const uint16_t chartReadsBeforeBounds = fake.chartReads;
        test.assertTrue(!world::chartCellWater(fake, -1, 0), "negative chart x is land");
        test.assertTrue(!world::chartCellWater(fake, world::CHART_TILES_X, 0), "east chart edge is land");
        test.assertTrue(!world::chartCellWater(fake, 0, world::CHART_TILES_Y), "south chart edge is land");
        test.assertEqual(fake.chartReads, chartReadsBeforeBounds, "chart bounds checks never read cart");
        runner.add(test);
    }

    {
        Test test("tileFromPixel floors every int16 pixel");
        bool matches = true;
        for (int32_t pixel = -32768; pixel <= 32767; ++pixel) {
            const int32_t expected = pixel >= 0 ? pixel / 8 : -(((-pixel) + 7) / 8);
            matches = matches && world::tileFromPixel(static_cast<int16_t>(pixel)) == expected;
        }
        test.assertTrue(matches, "260k/8 floors the same at every pixel value");
        runner.add(test);
    }

    {
        Test test("pixel rectangle collision uses tile overlap");
        world_test::FakeWorld fake;
        fake.setTile(4, 5, true);   // pixels x 32..39, y 40..47

        test.assertTrue(!world::rectHits(fake, 24, 32, 8, 8), "rect before wall is clear");
        test.assertTrue(world::rectHits(fake, 32, 40, 8, 8), "rect on wall hits");
        test.assertTrue(world::rectHits(fake, 31, 39, 2, 2), "one-pixel overlap hits");
        test.assertTrue(!world::rectHits(fake, 32, 48, 8, 8), "rect below wall is clear");
        test.assertTrue(!world::rectHits(fake, 32, 40, 0, 8), "zero-width rect cannot hit");
        test.assertTrue(!world::rectHits(fake, 32, 40, 8, -1), "negative-height rect cannot hit");

        test.assertTrue(world::rectHits(fake, -8, 40, 8, 8), "rect outside west is sealed");
        test.assertTrue(world::rectHits(fake, 2556, 40, 8, 8), "rect straddling east edge is sealed");
        runner.add(test);
    }

    {
        Test test("visible cache refreshes only when the window moves");
        world_test::FakeWorld fake;
        fake.setTile(10, 20, true);
        fake.setTile(27, 29, true);
        fake.setTile(0, 0, true);

        world::TileCache cache;
        world::clearCache(cache);
        test.assertTrue(!cache.valid, "cache starts empty");

        fake.tileReads = 0;
        test.assertTrue(world::refreshCache(cache, fake, 10, 20), "first refresh fills");
        test.assertEqual(fake.tileReads, 30, "three packed bytes per row, each read once");
        test.assertTrue(world::cacheContains(cache, 10, 20), "origin is in window");
        test.assertTrue(world::cacheContains(cache, 27, 29), "far corner is in window");
        test.assertTrue(!world::cacheContains(cache, 28, 20), "just outside width misses");
        test.assertTrue(world::tileSolidCached(cache, fake, 10, 20), "cached solid tile");
        test.assertTrue(!world::tileSolidCached(cache, fake, 11, 20), "cached open tile");

        test.assertTrue(!world::refreshCache(cache, fake, 10, 20), "same window is reused");
        test.assertEqual(fake.tileReads, 30, "reusing a window performs no cart reads");

        test.assertTrue(world::refreshCache(cache, fake, 11, 20), "a moved window refreshes");
        test.assertEqual(fake.tileReads, 60, "moved window reads a fresh packed window");

        test.assertTrue(world::tileSolidCached(cache, fake, 0, 0), "fallback reads wall");
        test.assertEqual(fake.tileReads, 61, "outside-window lookup falls back to cart");
        runner.add(test);
    }

    {
        Test test("packed cache refill preserves all alignments and seals out-of-world tiles");
        world_test::FakeWorld fake;
        for (uint16_t i = 0; i < world::WORLD_BYTES; ++i)
            fake.tiles[i] = static_cast<uint8_t>(i * 71 + i / 40 + 43);
        const int16_t ys[] = {-12, -1, 0, 20, 151, 159, 160};
        const int16_t xs[] = {-20, -1, 0, 1, 2, 3, 4, 5, 6, 7, 302, 319, 320};
        bool matches = true;
        for (int16_t y : ys)
            for (int16_t x : xs) {
                world::TileCache cache;
                world::clearCache(cache);
                fake.tileReads = 0;
                world::refreshCache(cache, fake, x, y);
                uint16_t expectedReads = 0;
                for (int16_t row = 0; row < world::CACHE_TILES_Y; ++row) {
                    int16_t lastByte = -1;
                    for (int16_t col = 0; col < world::CACHE_TILES_X; ++col) {
                        const int16_t tx = x + col, ty = y + row;
                        bool expected = true;
                        if (world::inTileBounds(tx, ty)) {
                            const int16_t source = world::tileByteIndex(tx, ty);
                            if (source != lastByte)
                                ++expectedReads;
                            lastByte = source;
                            expected = (fake.tiles[source] & (1u << (tx & 7))) != 0;
                        }
                        bool actual = false;
                        matches = world::cacheLookup(cache, tx, ty, actual) && matches;
                        matches = matches && actual == expected;
                    }
                }
                matches = matches && fake.tileReads == expectedReads;
            }
        test.assertTrue(matches, "every bit matches the source, with exactly one read per unique in-bounds byte");
        runner.add(test);
    }

    {
        Test test("generated settlement metadata accessors");
        test.assertEqual(world::settlementCount(), 4, "settlement count");
        test.assertEqual(world::spawnTileX(), exploration_data::DOCK_T_X / 8, "spawn tile x");
        test.assertEqual(world::spawnTileY(), exploration_data::DOCK_T_Y / 8, "spawn tile y");

        world::Settlement settlement;
        test.assertTrue(world::settlementInfo(0, settlement), "settlement 0 exists");
        test.assertEqual(settlement.id, 0, "settlement id");
        test.assertEqual(settlement.tile_x, exploration_data::DOCK_T_X / 8, "settlement tile x");
        test.assertEqual(settlement.tile_y, exploration_data::DOCK_T_Y / 8, "settlement tile y");
        test.assertEqual(settlement.pixel_x, exploration_data::DOCK_T_X, "settlement pixel x");
        test.assertEqual(settlement.pixel_y, exploration_data::DOCK_T_Y, "settlement pixel y");
        test.assertTrue(settlement.name != nullptr, "settlement name present");
        test.assertTrue(!world::settlementInfo(4, settlement), "settlement bounds");
        runner.add(test);
    }

    {
        Test test("generated service berth and survey metadata stays symbolic");
        test.assertEqual(world::berthCount(), 8, "four freight berths plus four work/service berths");
        test.assertEqual(world::serviceBerthCount(), 4, "both restoration berths and both work sites generated");
        test.assertEqual(world::surveyCount(), 2, "both survey rings generated");

        world::Berth berth;
        test.assertTrue(world::serviceBerthInfo(0, berth), "Pump Refuge metadata");
        test.assertEqual(berth.id, world::BERTH_PUMP_REFUGE, "Pump Refuge berth id");
        test.assertEqual(berth.milestone, 2, "Pump Refuge requires the PUMP bit");
        test.assertEqual(berth.tile_x, 270, "Pump Refuge dock tile x");
        test.assertEqual(berth.tile_y, 26, "Pump Refuge dock tile y");
        test.assertEqual(berth.pixel_x, 2160, "Pump Refuge dock pixel x");
        test.assertEqual(berth.pixel_y, 208, "Pump Refuge dock pixel y");
        test.assertEqual(world::SETTLEMENTS[0].id, 0, "freight ids remain contiguous");

        test.assertTrue(world::berthInfo(world::BERTH_PUMP_REFUGE, berth), "berthInfo resolves Pump Refuge");
        test.assertEqual(berth.id, world::BERTH_PUMP_REFUGE, "Pump Refuge id through berthInfo");
        test.assertTrue(world::berthInfo(world::BERTH_LOWER_GANTRY, berth), "berthInfo resolves Lower Gantry");
        test.assertEqual(berth.milestone, 4, "Lower Gantry requires the LIFT bit");
        test.assertTrue(world::berthInfo(world::BERTH_FREIGHT_YARD, berth), "berthInfo resolves the Freight Yard lift");
        test.assertEqual(berth.milestone, 4, "Freight Yard is restored by the LIFT bit");
        test.assertEqual(berth.tile_x, 271, "Freight Yard dock tile x");
        test.assertEqual(berth.tile_y, 77, "Freight Yard dock tile y");
        test.assertTrue(world::berthInfo(world::BERTH_DEEP_RELAY, berth), "berthInfo resolves the Deep Relay");
        test.assertEqual(berth.milestone, 32, "Deep Relay is unlocked by the expedition bit");
        test.assertEqual(berth.tile_x, 249, "Deep Relay dock tile x");
        test.assertEqual(berth.tile_y, 153, "Deep Relay dock tile y");
        test.assertTrue(world::berthInfo(2, berth), "berthInfo resolves a freight settlement");
        test.assertEqual(berth.milestone, 0, "freight berths have no restoration milestone");
        test.assertTrue(!world::berthInfo(8, berth), "no phantom berth id");

        world::Survey survey;
        test.assertTrue(world::surveyInfo(0, survey), "pump survey metadata");
        test.assertEqual(survey.id, world::SURVEY_PUMP, "pump survey id");
        test.assertEqual(survey.pixel_x, 2192, "pump survey pixel x");
        test.assertEqual(survey.pixel_y, 176, "pump survey pixel y");
        test.assertTrue(world::surveyInfo(1, survey), "lift survey metadata");
        test.assertEqual(survey.id, world::SURVEY_LIFT, "lift survey id");
        test.assertTrue(!world::surveyInfo(2, survey), "survey bounds");
        runner.add(test);
    }
}
