#pragma once

#include "../src/game.hpp"
#include "../src/hazard_view.hpp"
#include "../src/hazards.hpp"
#include "../src/pilot.hpp"
#include "test.hpp"
#include "world_test.hpp"

#if SH_FEATURE_MINES

namespace hazards_test {

using Fixture = world_test::FakeWorld;

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

inline void place(game::State &state, int16_t x, int16_t y) {
    state.x = x;
    state.y = y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    game::snapCamera(state);
}

inline bool pixelSet(const uint8_t *buffer, int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= cave::VIEW_WIDTH || y >= cave::VIEW_HEIGHT)
        return false;
    return ((buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u) != 0;
}

}   // namespace hazards_test

inline void HazardsSuite(TestRunner &runner) {
    using namespace hazards_test;

    static_assert(world::MINE_COUNT == 8, "the shipped world arms eight mines");
    static_assert(hazards::allMines() == 0xFF, "all eight mine bits are armed");

    {
        Test test("mine collision is circle versus the 26x8 hull");
        game::State state;
        game::reset(state);
        place(state, 100, 100);
        world::Mine mine;

        mine.x = 100;
        mine.y = 100;
        test.assertTrue(hazards::hullOverlapsMine(state, mine), "centre hit");

        mine.x = 112;   // right hull column
        mine.y = 100;
        test.assertTrue(hazards::hullOverlapsMine(state, mine), "touching the right hull edge");

        mine.x = 115;   // 3 px clear of the right edge: boundary hit
        mine.y = 100;
        test.assertTrue(hazards::hullOverlapsMine(state, mine), "radius boundary counts as a hit");

        mine.x = 116;
        mine.y = 100;
        test.assertTrue(!hazards::hullOverlapsMine(state, mine), "one pixel outside the radius misses");

        mine.x = 100;
        mine.y = 107;   // bottom hull row 103, 4 px away
        test.assertTrue(!hazards::hullOverlapsMine(state, mine), "outside vertical radius misses");

        mine.x = 87;    // left hull column
        mine.y = 96;    // top hull row
        test.assertTrue(hazards::hullOverlapsMine(state, mine), "corner overlap");
        runner.add(test);
    }

    {
        Test test("a hit removes one mine and cannot double-hit");
        game::State state;
        game::reset(state);
#if SH_FEATURE_HULL_DAMAGE
        state.hull = game::kHullMax;
#endif
        world::Mine mine;
        test.assertTrue(world::mineInfo(0, mine), "metadata mine 0 loads");

        place(state, mine.x, mine.y);
        const uint8_t first = hazards::collide(state);
        test.assertEqual(first, static_cast<uint8_t>(0), "first armed mine consumed");
        test.assertTrue(!hazards::alive(state, 0), "bit 0 is cleared");
        test.assertTrue(hazards::alive(state, 1), "other mines stay armed");
        test.assertEqual(state.impactAxis, hazards::kMineImpactAxis, "mine impact axis recorded");
        test.assertEqual(state.impactSpeedQ8, hazards::kMineImpactSpeedQ8, "deterministic impact speed");

        const uint8_t second = hazards::collide(state);
        test.assertEqual(second, hazards::kNoMine, "the same hit cannot fire twice");
        test.assertTrue(!hazards::alive(state, 0), "the consumed mine stays removed");
#if SH_FEATURE_HULL_DAMAGE
        test.assertEqual(state.hull, static_cast<uint8_t>(game::kHullMax - 1), "one point of hull damage");
#endif
        runner.add(test);
    }

    {
        Test test("reset re-arms every generated mine");
        game::State state;
        game::reset(state);
        test.assertEqual(state.minesAlive, hazards::allMines(), "game::reset arms all mines");
        test.assertTrue(hazards::remove(state, 3), "remove an armed mine");
        test.assertTrue(!hazards::alive(state, 3), "removed bit is clear");
        hazards::reset(state);
        test.assertEqual(state.minesAlive, hazards::allMines(), "hazards::reset restores the full mask");
        test.assertTrue(hazards::remove(state, 0), "remove reports the armed transition");
        test.assertTrue(!hazards::remove(state, 0), "second remove of the same mine is false");
        runner.add(test);
    }

    {
        Test test("world step consumes a mine in free flight");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Mine mine;
        world::mineInfo(2, mine);
        place(state, mine.x, mine.y);

        game::stepWorld(state, idle(), fake);
        test.assertTrue(!hazards::alive(state, 2), "stepWorld removed the overlapping mine");
        test.assertEqual(state.impactAxis, hazards::kMineImpactAxis, "impact feedback is set");
        test.assertEqual(state.impactSpeedQ8, hazards::kMineImpactSpeedQ8, "mine feedback carries its speed");

        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.impactAxis, 0, "the next step clears the one-frame spark");
        runner.add(test);
    }

#if SH_FEATURE_STATIONS
    {
        Test test("docked boats cannot consume a mine");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Mine mine;
        world::mineInfo(1, mine);
        place(state, mine.x, mine.y);
        state.docked = 0;

        game::stepWorld(state, idle(), fake);
        test.assertTrue(hazards::alive(state, 1), "mine stays armed while docked");
        test.assertEqual(state.impactAxis, 0, "no mine impact while docked");
        runner.add(test);
    }
#endif

    {
        Test test("pilot view draws armed mines and hides removed ones");
        game::State state;
        game::resetWorld(state);
        world::Mine mine;
        world::mineInfo(4, mine);
        place(state, mine.x, mine.y);

        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t originX = static_cast<int16_t>(camera.tileX * world::TILE_PIXELS);
        const int16_t originY = static_cast<int16_t>(camera.tileY * world::TILE_PIXELS);
        const int16_t screenX = static_cast<int16_t>(mine.x - originX);
        const int16_t screenY = static_cast<int16_t>(mine.y - originY);

        uint8_t buffer[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 8];
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0;
        hazard_view::drawMines(buffer, state, camera);
        test.assertTrue(pixelSet(buffer, screenX, screenY), "armed mine centre is visible");
        test.assertTrue(pixelSet(buffer, static_cast<int16_t>(screenX + 2), screenY), "mine glyph is wider than one pixel");

        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0;
        hazards::remove(state, 4);
        hazard_view::drawMines(buffer, state, camera);
        test.assertTrue(!pixelSet(buffer, screenX, screenY), "removed mine is not drawn");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_MINES
