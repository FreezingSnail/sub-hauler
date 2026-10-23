#pragma once

#include "../src/cave.hpp"
#include "../src/cave_render.hpp"
#include "test.hpp"

namespace cave_test {

// Minimal tile reader: same interface as world::FxTables, host memory only.
struct FakeTiles {
    uint8_t tiles[world::WORLD_BYTES];

    FakeTiles() {
        for (uint16_t i = 0; i < world::WORLD_BYTES; ++i)
            tiles[i] = 0;
    }

    uint8_t tileByte(uint16_t index) {
        return tiles[index];
    }

    void setTile(int16_t tileX, int16_t tileY, bool solid) {
        if (!world::inTileBounds(tileX, tileY))
            return;
        const uint16_t index = world::tileByteIndex(tileX, tileY);
        const uint8_t mask = static_cast<uint8_t>(1u << (tileX & 7));
        if (solid)
            tiles[index] |= mask;
        else
            tiles[index] &= static_cast<uint8_t>(~mask);
    }
};

// Independent reference for one atlas column. Mirrors the documented rule:
// solid rock, one-pixel trim on each open side, and a 45-degree chamfer in
// each corner where both sides are open.
inline uint8_t referenceColumn(uint8_t mask, uint8_t column) {
    uint8_t value = 0;
    for (uint8_t row = 0; row < 8; ++row) {
        bool wall = true;
        if ((mask & cave::OPEN_NORTH) && row == 0)
            wall = false;
        if ((mask & cave::OPEN_SOUTH) && row == 7)
            wall = false;
        if ((mask & cave::OPEN_WEST) && column == 0)
            wall = false;
        if ((mask & cave::OPEN_EAST) && column == 7)
            wall = false;
        if ((mask & cave::OPEN_NORTH) && (mask & cave::OPEN_WEST) && column == 1 && row == 1)
            wall = false;
        if ((mask & cave::OPEN_NORTH) && (mask & cave::OPEN_EAST) && column == 6 && row == 1)
            wall = false;
        if ((mask & cave::OPEN_SOUTH) && (mask & cave::OPEN_WEST) && column == 1 && row == 6)
            wall = false;
        if ((mask & cave::OPEN_SOUTH) && (mask & cave::OPEN_EAST) && column == 6 && row == 6)
            wall = false;
        if (wall)
            value |= static_cast<uint8_t>(1u << row);
    }
    return value;
}

}   // namespace cave_test

inline void CaveSuite(TestRunner &runner) {
    static_assert(cave::VIEW_TILES_X * world::TILE_PIXELS == 128, "view fills the display width");
    static_assert(cave::VIEW_TILES_Y * world::TILE_PIXELS == 64, "view fills the display height");
    static_assert(cave::MAX_CAMERA_TILE_X == 304, "camera east clamp");
    static_assert(cave::MAX_CAMERA_TILE_Y == 152, "camera south clamp");
    static_assert(cave::VIEW_TILES_X * cave::VIEW_TILES_Y == 128, "visible window is 128 tiles");
    static_assert(world::CACHE_TILES_X == cave::VIEW_TILES_X + 2, "cache has a one-tile margin per side");
    static_assert(world::CACHE_TILES_Y == cave::VIEW_TILES_Y + 2, "cache has a one-tile margin per side");

    {
        Test test("camera clamps the 16x8 window inside the baked map");
        const cave::Camera origin = cave::cameraCenteredAtPixel(0, 0);
        test.assertEqual(origin.tileX, static_cast<int16_t>(0), "origin clamps x to 0");
        test.assertEqual(origin.tileY, static_cast<int16_t>(0), "origin clamps y to 0");

        const cave::Camera spawn = cave::cameraCenteredAtPixel(world::SPAWN_SUB_X, world::SPAWN_SUB_Y);
        test.assertEqual(spawn.tileX, static_cast<int16_t>(28), "spawn camera tile x");
        test.assertEqual(spawn.tileY, static_cast<int16_t>(44), "spawn camera tile y");

        const cave::Camera farCorner = cave::cameraCenteredAtPixel(world::PIXELS_X - 1, world::PIXELS_Y - 1);
        test.assertEqual(farCorner.tileX, cave::MAX_CAMERA_TILE_X, "far east clamps");
        test.assertEqual(farCorner.tileY, cave::MAX_CAMERA_TILE_Y, "far south clamps");
        test.assertEqual(static_cast<int16_t>(farCorner.tileX + cave::VIEW_TILES_X - 1),
                         static_cast<int16_t>(world::TILES_X - 1), "last visible column is baked");
        test.assertEqual(static_cast<int16_t>(farCorner.tileY + cave::VIEW_TILES_Y - 1),
                         static_cast<int16_t>(world::TILES_Y - 1), "last visible row is baked");

        test.assertEqual(cave::cameraAtTile(-5, -5).tileX, static_cast<int16_t>(0), "negative tile clamps x");
        test.assertEqual(cave::cameraAtTile(400, 400).tileY, cave::MAX_CAMERA_TILE_Y, "past-end tile clamps y");
        runner.add(test);
    }

    {
        Test test("cache origin covers every neighbour of the visible window");
        cave_test::FakeTiles fake;
        world::TileCache cache;
        world::clearCache(cache);

        const cave::Camera camera = cave::cameraAtTile(10, 20);
        test.assertTrue(world::refreshCache(cache, fake, cave::cacheOriginTileX(camera),
                                            cave::cacheOriginTileY(camera)),
                        "cache fills from the margin origin");

        bool allVisible = true;
        for (uint8_t row = 0; row < cave::VIEW_TILES_Y; ++row) {
            for (uint8_t column = 0; column < cave::VIEW_TILES_X; ++column) {
                allVisible = allVisible &&
                             world::cacheContains(cache, static_cast<int16_t>(camera.tileX + column),
                                                  static_cast<int16_t>(camera.tileY + row));
            }
        }
        test.assertTrue(allVisible, "all 128 visible tiles are cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX - 1, camera.tileY - 1), "north-west ring cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX + cave::VIEW_TILES_X, camera.tileY),
                        "east ring cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX, camera.tileY + cave::VIEW_TILES_Y),
                        "south ring cached");
        test.assertEqual(cave::cacheColumn(camera, camera.tileX), static_cast<int16_t>(1), "visible column offset");
        test.assertEqual(cave::cacheRow(camera, camera.tileY), static_cast<int16_t>(1), "visible row offset");
        runner.add(test);
    }

    {
        Test test("open mask selects edge tiles from cached neighbours");
        cave_test::FakeTiles fake;
        world::TileCache cache;
        world::clearCache(cache);

        const cave::Camera camera = cave::cameraAtTile(10, 20);
        const int16_t tx = 12;
        const int16_t ty = 23;
        fake.setTile(tx, ty, true);

        const auto refresh = [&]() {
            world::clearCache(cache);
            world::refreshCache(cache, fake, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
        };

        refresh();
        test.assertEqual(cave::openMask(cache, tx, ty), static_cast<uint8_t>(0x0F), "solid tile with open neighbours");

        fake.setTile(tx, static_cast<int16_t>(ty - 1), true);
        refresh();
        test.assertEqual(cave::openMask(cache, tx, ty), static_cast<uint8_t>(0x0E), "north wall closes north edge");

        fake.setTile(static_cast<int16_t>(tx + 1), ty, true);
        refresh();
        test.assertEqual(cave::openMask(cache, tx, ty), static_cast<uint8_t>(0x06), "east wall closes east edge");

        test.assertTrue(cave::cachedSolid(cache, tx, ty), "centre tile is solid");
        test.assertTrue(cave::cachedSolid(cache, tx, static_cast<int16_t>(ty - 1)), "north tile is solid");

        // Outside the cache (sealed border) counts as rock, so no cart read is
        // needed for the neighbour ring at the world edge.
        test.assertTrue(cave::cachedSolid(cache, -1, -1), "out-of-cache tile is solid");
        runner.add(test);
    }

    {
        Test test("8x8 atlas matches the edge-trim reference for every mask");
        bool matches = true;
        for (uint8_t mask = 0; mask < cave::OPEN_MASK_COUNT; ++mask) {
            for (uint8_t column = 0; column < 8; ++column) {
                matches = matches && cave::CAVE_ATLAS[mask][column] ==
                                         cave_test::referenceColumn(mask, column);
            }
        }
        test.assertTrue(matches, "all 128 atlas bytes match");

        bool interiorSolid = true;
        for (uint8_t column = 0; column < 8; ++column)
            interiorSolid = interiorSolid && cave::CAVE_ATLAS[0][column] == 0xFF;
        test.assertTrue(interiorSolid, "interior tile is a solid 8x8 block");

        bool openSides = true;
        for (uint8_t column = 0; column < 8; ++column) {
            const uint8_t pillar = cave::CAVE_ATLAS[0x0F][column];
            if (column == 0 || column == 7)
                openSides = openSides && pillar == 0x00;
            else
                openSides = openSides && (pillar & 0x81) == 0;
        }
        test.assertTrue(openSides, "fully open tile trims every side");
        runner.add(test);
    }

    {
        Test test("settlement gantries sit on generated dock centres");
        const cave::Camera camera = cave::cameraCenteredAtPixel(world::SPAWN_SUB_X, world::SPAWN_SUB_Y);

        world::Settlement tidewall;
        test.assertTrue(world::settlementInfo(0, tidewall), "TIDEWALL record");
        test.assertEqual(cave::dockScreenX(camera, tidewall.pixel_x), static_cast<int16_t>(80), "dock screen x");
        test.assertEqual(cave::dockScreenY(camera, tidewall.pixel_y), static_cast<int16_t>(32), "dock screen y");
        test.assertTrue(cave::gantryVisible(80, 32), "spawn gantry is visible");

        // Every settlement maps through the same metadata path and stays
        // off-screen when its dock centre is far outside the window.
        bool mappingHolds = true;
        bool othersHidden = true;
        for (uint8_t id = 0; id < world::settlementCount(); ++id) {
            world::Settlement settlement;
            if (!world::settlementInfo(id, settlement)) {
                mappingHolds = false;
                continue;
            }
            const int16_t x = cave::dockScreenX(camera, settlement.pixel_x);
            const int16_t y = cave::dockScreenY(camera, settlement.pixel_y);
            mappingHolds = mappingHolds &&
                           x == static_cast<int16_t>(settlement.pixel_x - camera.tileX * world::TILE_PIXELS) &&
                           y == static_cast<int16_t>(settlement.pixel_y - camera.tileY * world::TILE_PIXELS);
            if (id != 0)
                othersHidden = othersHidden && !cave::gantryVisible(x, y);
        }
        test.assertTrue(mappingHolds, "dock mapping matches generated pixels");
        test.assertTrue(othersHidden, "far settlements are culled");
        test.assertTrue(!cave::gantryVisible(-cave::GANTRY_HALF_WIDTH, 32), "gantry edge is culled");
        test.assertTrue(cave::gantryVisible(-cave::GANTRY_HALF_WIDTH + 1, 32), "partly visible gantry is drawn");
        runner.add(test);
    }

    {
        Test test("composed frame draws rock, water, and the spawn gantry");
        cave_test::FakeTiles fake;
        // Reproduce the baked two-tile sealed border.
        for (int16_t tile = 0; tile < static_cast<int16_t>(world::TILES_Y); ++tile) {
            fake.setTile(0, tile, true);
            fake.setTile(1, tile, true);
            fake.setTile(static_cast<int16_t>(world::TILES_X - 1), tile, true);
            fake.setTile(static_cast<int16_t>(world::TILES_X - 2), tile, true);
        }
        for (int16_t tile = 0; tile < static_cast<int16_t>(world::TILES_X); ++tile) {
            fake.setTile(tile, 0, true);
            fake.setTile(tile, 1, true);
            fake.setTile(tile, static_cast<int16_t>(world::TILES_Y - 1), true);
            fake.setTile(tile, static_cast<int16_t>(world::TILES_Y - 2), true);
        }
        // A 4x4 rock island inside the spawn window (tiles 30..33, 46..49).
        for (int16_t tileY = 46; tileY <= 49; ++tileY) {
            for (int16_t tileX = 30; tileX <= 33; ++tileX)
                fake.setTile(tileX, tileY, true);
        }

        world::TileCache cache;
        world::clearCache(cache);
        const cave::Camera camera = cave::cameraCenteredAtPixel(world::SPAWN_SUB_X, world::SPAWN_SUB_Y);
        world::refreshCache(cache, fake, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));

        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t buffer[kBufferBytes];
        cave::composeFrame(camera, cache, buffer);

        const auto pixel = [&buffer](int16_t x, int16_t y) {
            return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u;
        };

        // TIDEWALL spawns at screen (80, 32): open water inside the dock ring,
        // white structure on the beam and ring.
        test.assertTrue(pixel(80, 32) == 0, "dock centre stays open water");
        test.assertTrue(pixel(80, 30) == 1, "dock ring top edge is white");
        test.assertTrue(pixel(80, 24) == 1, "gantry beam is white");
        test.assertTrue(pixel(80, 20) == 1, "station body is white");
        test.assertTrue(pixel(75, 20) == 0, "station window is a black pane");
        test.assertTrue(pixel(83, 20) == 0, "second station window is a black pane");
        test.assertTrue(pixel(120, 40) == 0, "open water stays black");

        // Rock island top-left tile (30, 46) has north and west open: column 0
        // is trimmed, column 1 carries the 45-degree chamfer.
        const uint8_t *corner = &buffer[2 * cave::VIEW_WIDTH + 2 * 8];
        test.assertEqual(corner[0], 0x00, "corner column trimmed to water");
        test.assertEqual(corner[1], 0xFC, "corner chamfer byte");
        test.assertEqual(corner[7], 0xFE, "east side keeps north trim only");

        // Interior island tile (31, 47) is solid except the deterministic
        // one-pixel speck at column 0, row 6.
        const uint8_t *interior = &buffer[3 * cave::VIEW_WIDTH + 3 * 8];
        test.assertEqual(interior[0], 0xBF, "interior tile speckle");
        uint8_t setBits = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            uint8_t value = interior[i];
            while (value) {
                setBits = static_cast<uint8_t>(setBits + (value & 1u));
                value >>= 1;
            }
        }
        test.assertEqual(setBits, 63, "interior tile is white except one speck");

        uint8_t repeat[kBufferBytes];
        cave::composeFrame(camera, cache, repeat);
        bool deterministic = true;
        for (uint16_t i = 0; i < kBufferBytes; ++i)
            deterministic = deterministic && buffer[i] == repeat[i];
        test.assertTrue(deterministic, "frame composition is deterministic");
        runner.add(test);
    }

    {
        Test test("composed window fills the screen at origin and map edges");
        cave_test::FakeTiles fake;
        for (int16_t tile = 0; tile < static_cast<int16_t>(world::TILES_Y); ++tile) {
            fake.setTile(0, tile, true);
            fake.setTile(1, tile, true);
            fake.setTile(static_cast<int16_t>(world::TILES_X - 1), tile, true);
            fake.setTile(static_cast<int16_t>(world::TILES_X - 2), tile, true);
        }
        for (int16_t tile = 0; tile < static_cast<int16_t>(world::TILES_X); ++tile) {
            fake.setTile(tile, 0, true);
            fake.setTile(tile, 1, true);
            fake.setTile(tile, static_cast<int16_t>(world::TILES_Y - 1), true);
            fake.setTile(tile, static_cast<int16_t>(world::TILES_Y - 2), true);
        }

        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        world::TileCache cache;
        uint8_t buffer[kBufferBytes];

        const auto render = [&](const cave::Camera &camera) {
            world::clearCache(cache);
            world::refreshCache(cache, fake, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
            cave::composeFrame(camera, cache, buffer);
        };
        const auto pixel = [&buffer](int16_t x, int16_t y) {
            return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u;
        };

        render(cave::cameraCenteredAtPixel(0, 0));
        test.assertTrue(pixel(1, 0) == 1, "origin renders the sealed top-left border");
        test.assertTrue(pixel(16, 16) == 0, "origin opens into water past the border");

        render(cave::cameraCenteredAtPixel(world::PIXELS_X - 1, world::PIXELS_Y - 1));
        test.assertTrue(pixel(127, 63) == 1, "far edge renders the sealed bottom-right border");
        test.assertTrue(pixel(100, 0) == 0, "far edge still shows water above the border");
        runner.add(test);
    }
}
