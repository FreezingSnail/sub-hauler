#pragma once

#include <cstring>
#include <fstream>
#include <vector>
#include "../src/parallax_view.hpp"
#include "../tools/parallax_source.hpp"
#include "test.hpp"

namespace parallax_test {
struct PinnedSource : ParallaxSource {
    uint8_t area = parallax_data::kAreaTide;
    uint8_t backgroundArea(uint16_t) { return area; }
};
inline bool pixel(const uint8_t *buffer, int16_t x, int16_t y) {
    return (buffer[(y >> 3) * cave::VIEW_WIDTH + x] & (1u << (y & 7))) != 0;
}
inline world::TileCache water(const cave::Camera &camera) {
    world::TileCache cache{};
    cache.valid = 1;
    cache.originTileX = cave::cacheOriginTileX(camera);
    cache.originTileY = cave::cacheOriginTileY(camera);
    return cache;
}
}

inline void ParallaxViewSuite(TestRunner &runner) {
    {
        Test test("detailed backdrop equals an independent per-pixel native image crop");
        bool exact = true;
        parallax_test::PinnedSource reader;
        const int16_t origins[][2] = {{0, 0}, {7, 11}, {127, 63}, {255, 127},
                                     {cave::MAX_CAMERA_PIXEL_X, cave::MAX_CAMERA_PIXEL_Y}};
        for (uint8_t area = 0; area < parallax_data::kAreaCount; ++area) {
          reader.area = area;
          for (const auto &origin : origins) {
            const auto camera = cave::cameraAtPixel(origin[0], origin[1]);
            uint8_t guarded[1026] = {};
            guarded[0] = guarded[1025] = 0xA5;
            parallax_view::draw(guarded + 1, camera, parallax_test::water(camera), reader);
            for (int16_t y = 0; y < 64; ++y)
                for (int16_t x = 0; x < 128; ++x) {
                    const uint16_t sx = ((origin[0] >> 1) + x) % parallax_data::kWidth;
                    const uint8_t sy = ((origin[1] >> 1) + y) % parallax_data::kHeight;
                    const uint8_t byte = parallax_data::kNative[area][(sy / 8) * parallax_data::kWidth + sx];
                    const bool expected = byte & (1u << (sy & 7));
                    exact &= parallax_test::pixel(guarded + 1, x, y) == expected;
                }
            exact &= guarded[0] == 0xA5 && guarded[1025] == 0xA5;
        }
        }
        test.assertTrue(exact, "pixel projection, source wrap padding and all framebuffer edges match");
        runner.add(test);
    }
    {
        Test test("distant cave retraces at half camera speed across tile and pattern boundaries");
        bool translated = true, stable = true, visible = false;
        parallax_test::PinnedSource reader;
        // Include all sub-tile remainders and the 128-pixel repeating seam.
        for (int16_t origin = 0; origin < 270; ++origin) {
            const auto camera = cave::cameraAtPixel(origin, origin);
            const auto cache = parallax_test::water(camera);
            uint8_t a[1024] = {}, b[1024] = {}, again[1024] = {};
            parallax_view::draw(a, camera, cache, reader);
            auto next = cave::cameraAtPixel(origin + 2, origin + 2);
            parallax_view::draw(b, next, parallax_test::water(next), reader);
            parallax_view::draw(again, camera, cache, reader);
            stable &= std::memcmp(a, again, sizeof(a)) == 0;
            for (int16_t y = 2; y < 61; ++y)
                for (int16_t x = 2; x < 125; ++x) {
                    visible |= parallax_test::pixel(a, x, y);
                    translated &= parallax_test::pixel(a, x + 1, y + 1) == parallax_test::pixel(b, x, y);
                }
            if (!(origin & 1)) {
                std::memset(b, 0, sizeof(b));
                next = cave::cameraAtPixel(origin + 1, origin + 1);
                parallax_view::draw(b, next, parallax_test::water(next), reader);
                stable &= std::memcmp(a, b, sizeof(a)) == 0;
            }
        }
        test.assertTrue(visible, "formations are visible in open water");
        test.assertTrue(translated, "two pixels of camera motion move the distant face one pixel on both axes");
        test.assertTrue(stable, "one-pixel half-speed holds and reverse visits are exact");
        runner.add(test);
    }
    {
        Test test("preview-route parallax never writes inside collision rock or opaque foreground tiles");
        bool safe = true, visible = false;
        struct Terrain : ParallaxSource {
            std::vector<uint8_t> pixels, tiles;
            void terrainBytes(uint32_t offset, uint8_t *out, uint16_t length) {
                std::memcpy(out, pixels.data() + offset, length);
            }
            uint8_t tileByte(uint16_t index) { return tiles[index]; }
        } terrain;
        std::ifstream pixels("fxdata/tables/exploration-terrain.bin", std::ios::binary);
        terrain.pixels.assign(std::istreambuf_iterator<char>(pixels), {});
        std::ifstream tiles("fxdata/tables/exploration.bin", std::ios::binary);
        terrain.tiles.assign(std::istreambuf_iterator<char>(tiles), {});
        const bool loaded = terrain.pixels.size() == cave::TERRAIN_BYTES && terrain.tiles.size() == world::WORLD_BYTES;
        test.assertTrue(loaded, "canonical terrain and collision load");
        if (loaded) {
            for (uint8_t route = 0; route < parallax_data::kAreaCount; ++route) {
                for (uint8_t frame = 0; frame < 128; ++frame) {
                    const int16_t travel = frame < 64 ? frame : static_cast<int16_t>(127 - frame);
                    const auto camera = cave::cameraAtPixel(
                        static_cast<int16_t>(parallax_data::kCheckX[route] + travel),
                        static_cast<int16_t>(parallax_data::kCheckY[route] +
                                             (route == parallax_data::kAreaKelp ? travel / 2 : 0)));
                    world::TileCache cache{};
                    world::refreshCache(cache, terrain, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
                    uint8_t before[1024], after[1024];
                    cave::streamTerrain(camera, terrain, before);
                    std::memcpy(after, before, sizeof(after));
                    parallax_view::draw(after, camera, cache, terrain);
                    parallax_view::clearBlack(after, camera, cache, terrain);
                    const int16_t ox = cave::pixelOriginX(camera);
                    const int16_t oy = cave::pixelOriginY(camera);
                    for (int16_t y = 0; y < 64; ++y)
                        for (int16_t x = 0; x < 128; ++x) {
                            const bool old = parallax_test::pixel(before, x, y);
                            const bool now = parallax_test::pixel(after, x, y);
                            safe &= !old || now;
                            if (old == now)
                                continue;
                            visible = true;
                            const int16_t tx = world::tileFromPixel(static_cast<int16_t>(ox + x));
                            const int16_t ty = world::tileFromPixel(static_cast<int16_t>(oy + y));
                            safe &= !world::tileSolid(terrain, tx, ty);
                            const uint16_t idx = static_cast<uint16_t>(
                                static_cast<uint16_t>(ty) * world::TILES_X + static_cast<uint16_t>(tx));
                            const uint8_t black = terrain.mask[static_cast<uint32_t>(idx) * 8 +
                                                              static_cast<uint8_t>((ox + x) & 7)];
                            safe &= !(black & (static_cast<uint8_t>(1u) << ((oy + y) & 7)));
                        }
                }
            }
        }
        test.assertTrue(safe, "foreground survives and background never enters solid or opaque foreground tiles");
        test.assertTrue(visible, "parallax remains visible across every preview route");
        runner.add(test);
    }
    {
        Test test("parallax preserves foreground and never decorates collision rock or a missing cache");
        bool safe = true, visible = false;
        // Real terrain and collision include dark cracks, scenery and map edges.
        struct Terrain : ParallaxSource {
            std::vector<uint8_t> pixels, tiles;
            void terrainBytes(uint32_t offset, uint8_t *out, uint16_t length) {
                std::memcpy(out, pixels.data() + offset, length);
            }
            uint8_t tileByte(uint16_t index) { return tiles[index]; }
        } terrain;
        std::ifstream pixels("fxdata/tables/exploration-terrain.bin", std::ios::binary);
        terrain.pixels.assign(std::istreambuf_iterator<char>(pixels), {});
        std::ifstream tiles("fxdata/tables/exploration.bin", std::ios::binary);
        terrain.tiles.assign(std::istreambuf_iterator<char>(tiles), {});
        const bool loaded = terrain.pixels.size() == cave::TERRAIN_BYTES && terrain.tiles.size() == world::WORLD_BYTES;
        test.assertTrue(loaded, "canonical terrain and collision load");
        if (loaded) {
            for (int16_t sample = -4; sample <= cave::MAX_CAMERA_PIXEL_Y / 19; ++sample) {
                const int16_t oy = sample < 0 ? (sample & 1 ? cave::MAX_CAMERA_PIXEL_Y : 0) : sample * 19;
                const int16_t ox = sample < 0 ? (sample & 2 ? cave::MAX_CAMERA_PIXEL_X : 0) : static_cast<int16_t>((oy * 7) % (cave::MAX_CAMERA_PIXEL_X + 1));
                const auto camera = cave::cameraAtPixel(ox, oy);
                world::TileCache cache{};
                world::refreshCache(cache, terrain, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
                uint8_t before[1024], guarded[1026];
                guarded[0] = guarded[1025] = 0xA5;
                cave::streamTerrain(camera, terrain, before);
                std::memcpy(guarded + 1, before, sizeof(before));
                parallax_view::draw(guarded + 1, camera, cache, terrain);
                safe &= guarded[0] == 0xA5 && guarded[1025] == 0xA5;
                for (int16_t y = 0; y < 64; ++y)
                    for (int16_t x = 0; x < 128; ++x) {
                        const bool old = parallax_test::pixel(before, x, y);
                        const bool now = parallax_test::pixel(guarded + 1, x, y);
                        safe &= !old || now;
                        if (old != now) {
                            visible = true;
                            safe &= !world::tileSolid(terrain, world::tileFromPixel(ox + x), world::tileFromPixel(oy + y));
                        }
                    }
                cache.valid = 0;
                std::memcpy(guarded + 1, before, sizeof(before));
                parallax_view::draw(guarded + 1, camera, cache, terrain);
                safe &= std::memcmp(guarded + 1, before, sizeof(before)) == 0;
                cache.valid = 1;
                ++cache.originTileX;
                parallax_view::draw(guarded + 1, camera, cache, terrain);
                safe &= std::memcmp(guarded + 1, before, sizeof(before)) == 0;
            }
        }
        test.assertTrue(safe, "foreground survives, solid tiles remain exact, buffer guards survive");
        test.assertTrue(visible, "parallax remains visible in real caves");
        runner.add(test);
    }
}
