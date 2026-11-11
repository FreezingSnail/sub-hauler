#pragma once

#include "../src/cave.hpp"
#include "../src/cave_render.hpp"
#include "test.hpp"
#include <cstring>
#include <fstream>
#include <vector>

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

// Deliberately slow, independent pixel composition for the packed-row renderer.
inline void referenceFrame(const cave::Camera &camera, const world::TileCache &cache, uint8_t *buffer) {
    for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
        for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
            const int16_t tx = camera.tileX + x / 8, ty = camera.tileY + y / 8;
            bool solid = false;
            bool white = false;
            if (world::cacheLookup(cache, tx, ty, solid) && solid) {
                const uint8_t mask = cave::openMask(cache, tx, ty);
                white = (referenceColumn(mask, x % 8) & (1u << (y % 8))) != 0;
                if (!mask && x % 8 == ((tx * 5 + ty * 3) & 7) && y % 8 == ((tx * 3 + ty * 7) & 7))
                    white = false;
            }
            if (white)
                cave::setPixel(buffer, x, y);
            else
                cave::clearPixel(buffer, x, y);
        }
    }
    cave::drawSettlements(buffer, camera);
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
        Test test("byte-combine kernel matches direct shifts for every factor and byte pair");
        bool matches = true;
        for (uint8_t shift = 1; shift < 8 && matches; ++shift) {
            const uint8_t factor = static_cast<uint8_t>(1u << (8 - shift));
            for (uint16_t a = 0; a < 256 && matches; ++a)
                for (uint16_t b = 0; b < 256; ++b) {
                    const uint8_t expected = static_cast<uint8_t>((static_cast<uint8_t>(a) >> shift) | (static_cast<uint8_t>(b) << (8 - shift)));
                    if (cave::combineShiftedBytes(static_cast<uint8_t>(a), static_cast<uint8_t>(b), factor) != expected) {
                        matches = false;
                        break;
                    }
                }
        }
        test.assertTrue(matches, "all 7 x 256 x 256 combinations match");
        runner.add(test);
    }

    {
        Test test("streamed scenery matches native image crops across the actual world");
        struct NativeTerrain {
            std::vector<uint8_t> pixels;
            uint8_t reads = 0;
            uint16_t bytes = 0;
            bool bounds = true;
            void terrainBytes(uint32_t offset, uint8_t *dest, uint16_t length) {
                ++reads;
                bytes += length;
                if (offset + length > pixels.size()) {
                    bounds = false;
                    return;
                }
                std::memcpy(dest, pixels.data() + offset, length);
            }
        } terrain;
        terrain.pixels.resize(cave::TERRAIN_BYTES);
        std::ifstream image("fxdata/tables/exploration-terrain.bin", std::ios::binary);
        const bool imageLoaded = static_cast<bool>(image.read(reinterpret_cast<char *>(terrain.pixels.data()), terrain.pixels.size())) && image.peek() == std::char_traits<char>::eof();
        test.assertTrue(imageLoaded, "exact generated native scenery image loads");
        bool matches = imageLoaded;
        if (matches) {
            uint8_t streamed[1026], composed[1024];
            // Every legal horizontal and vertical camera origin appears,
            // including all four map corners and each source page boundary.
            for (int16_t y = 0; y <= cave::MAX_CAMERA_TILE_Y; ++y) {
                for (int16_t x = 0; x <= cave::MAX_CAMERA_TILE_X; x += (y == 0 || y == cave::MAX_CAMERA_TILE_Y ? 1 : 8)) {
                    const cave::Camera camera = cave::cameraAtTile(x, y);
                    // Independent per-pixel crop: scenery intentionally differs
                    // from collision-tile art, but streaming must preserve every
                    // pixel and the same native page/bit orientation.
                    for (int16_t sy = 0; sy < cave::VIEW_HEIGHT; ++sy) {
                        for (int16_t sx = 0; sx < cave::VIEW_WIDTH; ++sx) {
                            const int16_t wx = static_cast<int16_t>(cave::pixelOriginX(camera) + sx), wy = static_cast<int16_t>(cave::pixelOriginY(camera) + sy);
                            const uint8_t value = terrain.pixels[(static_cast<uint32_t>(wy) / 8) * world::PIXELS_X + wx];
                            if (value & (1u << (wy % 8)))
                                cave::setPixel(composed, sx, sy);
                            else
                                cave::clearPixel(composed, sx, sy);
                        }
                    }
                    cave::drawSettlements(composed, camera);
                    terrain.reads = terrain.bytes = 0;
                    streamed[0] = streamed[1025] = 0xA5;
                    cave::streamFrame(camera, terrain, streamed + 1);
                    matches = matches && terrain.bounds && terrain.reads == 8 && terrain.bytes == 1024;
                    matches = matches && streamed[0] == 0xA5 && streamed[1025] == 0xA5;
                    matches = matches && std::memcmp(streamed + 1, composed, 1024) == 0;
                }
            }
        }
        test.assertTrue(matches, "all scene pixels identical, exactly eight bounded reads into guarded framebuffer");
        runner.add(test);
    }

    {
        Test test("sub-pixel camera streams the exact native crop at every remainder");
        struct NativeTerrain {
            std::vector<uint8_t> pixels;
            uint8_t reads = 0;
            uint16_t bytes = 0;
            bool bounds = true;
            void terrainBytes(uint32_t offset, uint8_t *dest, uint16_t length) {
                ++reads;
                bytes = static_cast<uint16_t>(bytes + length);
                if (offset + length > pixels.size()) {
                    bounds = false;
                    return;
                }
                std::memcpy(dest, pixels.data() + offset, length);
            }
        } terrain;
        terrain.pixels.resize(cave::TERRAIN_BYTES);
        std::ifstream image("fxdata/tables/exploration-terrain.bin", std::ios::binary);
        const bool imageLoaded = static_cast<bool>(image.read(reinterpret_cast<char *>(terrain.pixels.data()), terrain.pixels.size())) && image.peek() == std::char_traits<char>::eof();
        test.assertTrue(imageLoaded, "exact generated native scenery image loads");

        const int16_t baseX[] = {0, 1, 7, 123, 1234, 2425, cave::MAX_CAMERA_PIXEL_X};
        const int16_t baseY[] = {0, 1, 7, 400, 1200, 1209, cave::MAX_CAMERA_PIXEL_Y};
        uint8_t streamed[1026], composed[1024];
        bool matches = imageLoaded;
        uint16_t cases = 0;
        for (int16_t bx : baseX)
            for (int16_t by : baseY)
                for (int8_t sx = 0; sx < 8; ++sx)
                    for (int8_t sy = 0; sy < 8; ++sy) {
                        const cave::Camera camera = cave::cameraAtPixel(static_cast<int16_t>(bx + sx), static_cast<int16_t>(by + sy));
                        const int16_t ox = cave::pixelOriginX(camera), oy = cave::pixelOriginY(camera);
                        for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
                            for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
                                const int16_t wx = static_cast<int16_t>(ox + x), wy = static_cast<int16_t>(oy + y);
                                const uint8_t value = terrain.pixels[(static_cast<uint32_t>(wy) / 8) * cave::TERRAIN_ROW_BYTES + wx];
                                if (value & (1u << (wy % 8)))
                                    cave::setPixel(composed, x, y);
                                else
                                    cave::clearPixel(composed, x, y);
                            }
                        }
                        cave::drawSettlements(composed, camera);
                        terrain.reads = terrain.bytes = 0;
                        streamed[0] = streamed[1025] = 0xA5;
                        cave::streamFrame(camera, terrain, streamed + 1);
                        const uint8_t expectedReads = static_cast<uint8_t>(camera.subY ? 12 : 8);
                        const uint16_t expectedBytes = static_cast<uint16_t>(camera.subY ? 1152 : 1024);
                        matches = matches && terrain.bounds && terrain.reads == expectedReads && terrain.bytes == expectedBytes;
                        matches = matches && streamed[0] == 0xA5 && streamed[1025] == 0xA5;
                        matches = matches && std::memcmp(streamed + 1, composed, 1024) == 0;
                        ++cases;
                    }
        test.assertTrue(matches, "every X/Y remainder matches the native crop through bounded reads");
        test.assertEqual(cases, 7 * 7 * 8 * 8, "all fractional origin cases ran");
        runner.add(test);
    }

    {
        Test test("page-span rectangles preserve pixels and clip every display edge");
        const int16_t xs[] = {-140, -8, -1, 0, 1, 7, 8, 120, 127, 128, 140};
        const int16_t ys[] = {-80, -8, -1, 0, 1, 7, 8, 55, 63, 64, 80};
        const uint8_t sizes[][2] = {{0, 9}, {9, 0}, {1, 255}, {255, 1}, {7, 7}, {8, 8}, {9, 9}, {128, 9}, {32, 3}, {3, 32}, {255, 255}};
        uint8_t guarded[1026], expected[1024];
        bool matches = true;
        for (int16_t x : xs)
            for (int16_t y : ys)
                for (const auto &size : sizes) {
                    for (uint8_t color = 0; color < 2; ++color) {
                        guarded[0] = guarded[1025] = 0xA5;
                        for (uint16_t i = 0; i < 1024; ++i)
                            guarded[i + 1] = expected[i] = static_cast<uint8_t>(i * 37 + 51);
                        const uint8_t width = size[0], height = size[1];
                        cave::fillRect(guarded + 1, x, y, width, height, color);
                        for (int16_t py = 0; py < 64; ++py)
                            for (int16_t px = 0; px < 128; ++px) {
                                if (px >= x && px < x + width && py >= y && py < y + height) {
                                    const uint16_t i = (py / 8) * 128 + px;
                                    const uint8_t mask = 1u << (py % 8);
                                    if (color)
                                        expected[i] |= mask;
                                    else
                                        expected[i] &= static_cast<uint8_t>(~mask);
                                }
                            }
                        matches = matches && guarded[0] == 0xA5 && guarded[1025] == 0xA5;
                        for (uint16_t i = 0; i < 1024; ++i)
                            matches = matches && guarded[i + 1] == expected[i];
                    }
                }
        test.assertTrue(matches, "set/clear, page boundaries, clipping and guard bytes match pixel reference");
        runner.add(test);
    }

    {
        Test test("packed cave rows match pixel reference for dense and sparse terrain");
        uint8_t actual[1024], expected[1024];
        world::TileCache cache;
        uint32_t random = 7419;
        bool matches = true;
        const cave::Camera cameras[] = {cave::cameraAtTile(0, 0), cave::cameraAtTile(304, 152), cave::cameraAtTile(10, 20), cave::cameraCenteredAtPixel(world::SPAWN_SUB_X, world::SPAWN_SUB_Y)};
        for (const auto &camera : cameras)
            for (uint8_t pattern = 0; pattern < 34; ++pattern) {
                cache.valid = 1;
                cache.originTileX = cave::cacheOriginTileX(camera);
                cache.originTileY = cave::cacheOriginTileY(camera);
                for (uint8_t &byte : cache.bits) {
                    random = random * 1664525u + 1013904223u;
                    byte = pattern == 0 ? 0 : pattern == 1 ? 0xFF : static_cast<uint8_t>(random >> 24);
                }
                cave::composeFrame(camera, cache, actual);
                cave_test::referenceFrame(camera, cache, expected);
                for (uint16_t i = 0; i < 1024; ++i)
                    matches = matches && actual[i] == expected[i];
            }
        test.assertTrue(matches, "all 136 windows match including four row bit alignments and cache padding");
        runner.add(test);
    }

    {
        Test test("cave composition retains invalid and partial cache behavior");
        uint8_t actual[1024], expected[1024];
        world::TileCache cache;
        const cave::Camera camera = cave::cameraAtTile(10, 20);
        bool matches = true;
        for (uint8_t valid = 0; valid < 2; ++valid)
            for (int16_t offset = -20; offset <= 20; ++offset) {
                cache.valid = valid;
                cache.originTileX = camera.tileX + offset;
                cache.originTileY = camera.tileY - offset;
                for (uint8_t i = 0; i < world::CACHE_BYTES; ++i)
                    cache.bits[i] = static_cast<uint8_t>(i * 71 + 43);
                cave::composeFrame(camera, cache, actual);
                cave_test::referenceFrame(camera, cache, expected);
                for (uint16_t i = 0; i < 1024; ++i)
                    matches = matches && actual[i] == expected[i];
            }
        test.assertTrue(matches, "missing current tiles stay water; missing neighbours stay solid");
        runner.add(test);
    }

    {
        Test test("camera clamps the 16x8 window inside the baked map");
        const cave::Camera origin = cave::cameraCenteredAtPixel(0, 0);
        test.assertEqual(origin.tileX, static_cast<int16_t>(0), "origin clamps x to 0");
        test.assertEqual(origin.tileY, static_cast<int16_t>(0), "origin clamps y to 0");

        const cave::Camera spawn = cave::cameraCenteredAtPixel(world::SPAWN_PIXEL_X, world::SPAWN_PIXEL_Y);
        test.assertEqual(spawn.tileX, static_cast<int16_t>(world::SPAWN_TILE_X - 8), "spawn camera tile x");
        test.assertEqual(spawn.tileY, static_cast<int16_t>(world::SPAWN_TILE_Y - 4), "spawn camera tile y");

        const cave::Camera farCorner = cave::cameraCenteredAtPixel(world::PIXELS_X - 1, world::PIXELS_Y - 1);
        test.assertEqual(farCorner.tileX, cave::MAX_CAMERA_TILE_X, "far east clamps");
        test.assertEqual(farCorner.tileY, cave::MAX_CAMERA_TILE_Y, "far south clamps");
        test.assertEqual(static_cast<int16_t>(farCorner.tileX + cave::VIEW_TILES_X - 1), static_cast<int16_t>(world::TILES_X - 1), "last visible column is baked");
        test.assertEqual(static_cast<int16_t>(farCorner.tileY + cave::VIEW_TILES_Y - 1), static_cast<int16_t>(world::TILES_Y - 1), "last visible row is baked");

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
        test.assertTrue(world::refreshCache(cache, fake, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera)), "cache fills from the margin origin");

        bool allVisible = true;
        for (uint8_t row = 0; row < cave::VIEW_TILES_Y; ++row) {
            for (uint8_t column = 0; column < cave::VIEW_TILES_X; ++column) {
                allVisible = allVisible && world::cacheContains(cache, static_cast<int16_t>(camera.tileX + column), static_cast<int16_t>(camera.tileY + row));
            }
        }
        test.assertTrue(allVisible, "all 128 visible tiles are cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX - 1, camera.tileY - 1), "north-west ring cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX + cave::VIEW_TILES_X, camera.tileY), "east ring cached");
        test.assertTrue(world::cacheContains(cache, camera.tileX, camera.tileY + cave::VIEW_TILES_Y), "south ring cached");
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
                matches = matches && cave::CAVE_ATLAS[mask][column] == cave_test::referenceColumn(mask, column);
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
        const cave::Camera camera = cave::cameraCenteredAtPixel(world::SPAWN_PIXEL_X, world::SPAWN_PIXEL_Y);

        world::Settlement tidewall;
        test.assertTrue(world::settlementInfo(0, tidewall), "TIDEWALL record");
        test.assertEqual(cave::dockScreenX(camera, tidewall.pixel_x), static_cast<int16_t>(64), "dock screen x");
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
            mappingHolds =
                mappingHolds && x == static_cast<int16_t>(settlement.pixel_x - cave::pixelOriginX(camera)) && y == static_cast<int16_t>(settlement.pixel_y - cave::pixelOriginY(camera));
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
        // A 4x4 rock island at screen tile (2,2), left of the home berth.
        for (int16_t tileY = world::SPAWN_TILE_Y - 2; tileY <= world::SPAWN_TILE_Y + 1; ++tileY) {
            for (int16_t tileX = world::SPAWN_TILE_X - 6; tileX <= world::SPAWN_TILE_X - 3; ++tileX)
                fake.setTile(tileX, tileY, true);
        }

        world::TileCache cache;
        world::clearCache(cache);
        const cave::Camera camera = cave::cameraCenteredAtPixel(world::SPAWN_PIXEL_X, world::SPAWN_PIXEL_Y);
        world::refreshCache(cache, fake, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));

        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        uint8_t buffer[kBufferBytes];
        cave::composeFrame(camera, cache, buffer);

        const auto pixel = [&buffer](int16_t x, int16_t y) { return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u; };

        // TIDEWALL spawns at screen (64, 32): open water inside the dock ring,
        // white structure on the beam and ring.
        test.assertTrue(pixel(64, 32) == 0, "dock centre stays open water");
        test.assertTrue(pixel(64, 30) == 1, "dock ring top edge is white");
        test.assertTrue(pixel(64, 24) == 1, "gantry beam is white");
        test.assertTrue(pixel(64, 20) == 1, "station body is white");
        test.assertTrue(pixel(59, 20) == 0, "station window is a black pane");
        test.assertTrue(pixel(67, 20) == 0, "second station window is a black pane");
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
        const auto pixel = [&buffer](int16_t x, int16_t y) { return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u; };

        render(cave::cameraCenteredAtPixel(0, 0));
        test.assertTrue(pixel(1, 0) == 1, "origin renders the sealed top-left border");
        test.assertTrue(pixel(16, 16) == 0, "origin opens into water past the border");

        render(cave::cameraCenteredAtPixel(world::PIXELS_X - 1, world::PIXELS_Y - 1));
        test.assertTrue(pixel(127, 63) == 1, "far edge renders the sealed bottom-right border");
        test.assertTrue(pixel(100, 0) == 0, "far edge still shows water above the border");
        runner.add(test);
    }
}
