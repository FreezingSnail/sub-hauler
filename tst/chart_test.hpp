#pragma once



#include "../src/chart.hpp"
#include "../src/game.hpp"
#include "../src/world.hpp"
#include "test.hpp"

// Fog state tests: cell mapping, square reveal radius, clipping at the world
// border, persistence across reveals, and the coarse flight-tick cadence.
namespace chart_test {

struct FakeReader {
    uint8_t water[world::CHART_BYTES];
    uint16_t reads;
    uint16_t bursts = 0;

    FakeReader() : reads(0) {
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            water[i] = 0;
    }

    uint8_t chartByte(uint16_t index) {
        ++reads;
        return water[index];
    }

    void chartBytes(uint16_t index, uint8_t *buffer, uint8_t length) {
        ++bursts;
        for (uint8_t i = 0; i < length; ++i)
            buffer[i] = chartByte(index + i);
    }

    void setWater(int16_t cellX, int16_t cellY, bool value) {
        if (!world::inChartBounds(cellX, cellY))
            return;
        const uint16_t index = world::chartByteIndex(cellX, cellY);
        const uint8_t mask = static_cast<uint8_t>(1u << (cellX & 7));
        if (value)
            water[index] |= mask;
        else
            water[index] &= static_cast<uint8_t>(~mask);
    }
};

}   // namespace chart_test

inline void ChartSuite(TestRunner &runner) {
    static_assert(sizeof(chart::State) == world::CHART_BYTES, "state is exactly the 256 B explored set");
    static_assert(chart::kCellPixels == 40, "one chart cell covers 40x40 pixels");

    {
        Test test("chart cell mapping is floor-based and bounds-safe");
        test.assertEqual(chart::cellFromPixelX(0), static_cast<int16_t>(0), "pixel 0 is cell 0");
        test.assertEqual(chart::cellFromPixelX(39), static_cast<int16_t>(0), "pixel 39 stays in cell 0");
        test.assertEqual(chart::cellFromPixelX(40), static_cast<int16_t>(1), "pixel 40 starts cell 1");
        test.assertEqual(chart::cellFromPixelX(world::PIXELS_X - 1), static_cast<int16_t>(63), "last pixel column");
        test.assertEqual(chart::cellFromPixelX(world::PIXELS_X), static_cast<int16_t>(chart::kInvalidCell), "east outside is invalid");
        test.assertEqual(chart::cellFromPixelX(-1), static_cast<int16_t>(chart::kInvalidCell), "west outside is invalid");
        test.assertEqual(chart::cellFromPixelY(0), static_cast<int16_t>(0), "pixel row 0 is cell 0");
        test.assertEqual(chart::cellFromPixelY(1279), static_cast<int16_t>(31), "last pixel row");
        test.assertEqual(chart::cellFromPixelY(1280), static_cast<int16_t>(chart::kInvalidCell), "south outside is invalid");

        int16_t cellX = -1;
        int16_t cellY = -1;
        test.assertTrue(chart::cellFromPixels(world::SPAWN_SUB_X, world::SPAWN_SUB_Y, cellX, cellY), "spawn pixel maps");
        // The live spawn sits beside the home gantry, so its chart cell can
        // differ from the dock tile while the radius-1 reveal still covers it.
        test.assertEqual(cellX, static_cast<int16_t>(world::SPAWN_SUB_X / chart::kCellPixels), "spawn chart x");
        test.assertEqual(cellY, static_cast<int16_t>(world::SPAWN_SUB_Y / chart::kCellPixels), "spawn chart y");

        test.assertTrue(chart::cellFromTile(world::SPAWN_TILE_X, world::SPAWN_TILE_Y, cellX, cellY), "spawn tile maps");
        test.assertEqual(cellX, static_cast<int16_t>(world::SPAWN_TILE_X / 5), "spawn tile chart x");
        test.assertEqual(cellY, static_cast<int16_t>(world::SPAWN_TILE_Y / 5), "spawn tile chart y");
        test.assertTrue(chart::cellFromTile(0, 0, cellX, cellY), "origin tile maps");
        test.assertTrue(chart::cellFromTile(world::TILES_X - 1, world::TILES_Y - 1, cellX, cellY), "last tile maps");
        test.assertEqual(cellX, static_cast<int16_t>(63), "last tile chart x");
        test.assertEqual(cellY, static_cast<int16_t>(31), "last tile chart y");
        test.assertTrue(!chart::cellFromTile(-1, 0, cellX, cellY), "negative tile is rejected");
        test.assertTrue(!chart::cellFromTile(world::TILES_X, 0, cellX, cellY), "east tile is rejected");
        runner.add(test);
    }

    {
        Test test("square reveal radius, bounds clipping, and persistence");
        chart::State state;
        chart::reset(state);

        test.assertEqual(chart::revealAtPixels(state, 0, 0, 0), 1, "radius 0 reveals only the centre");
        test.assertTrue(chart::isExplored(state, 0, 0), "centre is explored");
        test.assertTrue(!chart::isExplored(state, 1, 0), "radius 0 does not spill east");

        test.assertEqual(chart::revealAtPixels(state, 0, 0, 1), 3, "radius 1 adds the in-bounds ring");
        test.assertTrue(chart::isExplored(state, 1, 0), "east neighbour");
        test.assertTrue(chart::isExplored(state, 0, 1), "south neighbour");
        test.assertTrue(chart::isExplored(state, 1, 1), "diagonal neighbour is inside the square radius");
        test.assertTrue(!chart::isExplored(state, 2, 0), "radius 1 stops at one cell");

        test.assertEqual(chart::revealAtPixels(state, 0, 0, 1), 0, "re-revealing the same square adds nothing");
        test.assertEqual(chart::revealAtPixels(state, -1, -1, 2), 0, "negative pixels are ignored");
        test.assertEqual(chart::revealAtPixels(state, world::PIXELS_X, 0, 2), 0, "east overflow is ignored");
        test.assertEqual(chart::revealAtPixels(state, 0, world::PIXELS_Y, 2), 0, "south overflow is ignored");

        test.assertEqual(chart::revealAtPixels(state, 1000, 800, 1), 9, "mid-map radius reveals nine cells");
        test.assertTrue(chart::isExplored(state, 25, 20), "target cell explored");
        test.assertEqual(chart::revealAtPixels(state, 1600, 1000, 0), 1, "later reveal works");
        test.assertTrue(chart::isExplored(state, 40, 25), "later target cell explored");
        test.assertTrue(chart::isExplored(state, 0, 0), "old bits persist after later reveals");

        chart::State edge;
        chart::reset(edge);
        test.assertEqual(chart::revealAtPixels(edge, world::PIXELS_X - 1, world::PIXELS_Y - 1, 1), 4, "corner reveal clips to four cells");
        test.assertTrue(chart::isExplored(edge, 63, 31), "corner cell");
        test.assertTrue(chart::isExplored(edge, 62, 30), "corner diagonal");
        test.assertTrue(!chart::isExplored(edge, 61, 31), "clip does not wrap");
        runner.add(test);
    }

    {
        Test test("spawn reveal and coarse movement reveal use flight ticks");
        game::State gameState;
        game::resetWorld(gameState);

        chart::State state;
        chart::reset(state);
        chart::revealInitial(state, gameState);
        const int16_t spawnCellX = static_cast<int16_t>(world::SPAWN_SUB_X / chart::kCellPixels);
        const int16_t spawnCellY = static_cast<int16_t>(world::SPAWN_SUB_Y / chart::kCellPixels);
        test.assertTrue(chart::isExplored(state, spawnCellX, spawnCellY), "spawn cell is explored");
        test.assertTrue(chart::isExplored(state, spawnCellX + 1, spawnCellY), "spawn reveal radius reaches the home dock cell");

        gameState.x = 1000;
        gameState.y = 800;
        gameState.ticks = 29;
        chart::revealOnStep(state, gameState);
        test.assertTrue(!chart::isExplored(state, 25, 20), "off-cadence tick does not reveal");

        gameState.ticks = 30;
        chart::revealOnStep(state, gameState);
        test.assertTrue(chart::isExplored(state, 25, 20), "30-tick cadence reveals the new cell");
        test.assertTrue(chart::isExplored(state, 24, 20), "movement reveal radius is one cell");

        chart::revealInitial(state, gameState);
        test.assertTrue(chart::isExplored(state, world::SPAWN_TILE_X / 5, world::SPAWN_TILE_Y / 5), "a later spawn reveal cannot wipe the map");
        test.assertTrue(!chart::isExplored(state, 30, 20), "the later reveal only adds cells around the current position");

        gameState.x = 1200;
        gameState.y = 840;
        gameState.ticks = 31;
        chart::revealOnStep(state, gameState);
        test.assertTrue(!chart::isExplored(state, 30, 21), "off-cadence movement stays fogged");

        gameState.ticks = 60;
        chart::revealOnStep(state, gameState);
        test.assertTrue(chart::isExplored(state, 30, 21), "the next cadence reveals again");
        runner.add(test);
    }

    {
        Test test("passive chart reveal remains after sonar retirement");
        chart::State state;
        chart::reset(state);
        game::State gameState;
        game::resetWorld(gameState);
        chart::revealInitial(state, gameState);
        const uint16_t before = [&state] {
            uint16_t count = 0;
            for (uint16_t i = 0; i < world::CHART_BYTES; ++i) {
                for (uint8_t bit = 0; bit < 8; ++bit)
                    count = static_cast<uint16_t>(count + ((state.explored[i] >> bit) & 1u));
            }
            return count;
        }();
        gameState.x = 1000;
        gameState.y = 800;
        gameState.ticks = 30;
        chart::revealOnStep(state, gameState);
        test.assertTrue(before > 0, "spawn reveal populated the chart");
        test.assertTrue(chart::isExplored(state, 25, 20), "flight-cadence reveal still works without sonar");
        runner.add(test);
    }
}

