#pragma once
#include "../src/features.hpp"
#if SH_FEATURE_EXPLORATION
#include <fstream>
#include <queue>
#include <utility>
#include <vector>
#include "../src/exploration.hpp"
#include "../src/exploration_view.hpp"
#include "../src/pilot.hpp"
#include "../src/jobs.hpp"
#if SH_FEATURE_STATIONS
#include "../src/station.hpp"
#endif
#if SH_FEATURE_SONAR
#include "../src/sonar.hpp"
#endif
#include "test.hpp"
#include "world_test.hpp"
namespace exploration_test {
inline void place(game::State &s, int16_t x, int16_t y) {
    s.x = x;
    s.y = y;
    s.vxQ8 = 0;
    s.vyQ8 = 0;
    s.xFrac = 0;
    s.yFrac = 0;
    s.docked = -1;
}
inline void dwell(game::State &s, uint8_t count = exploration::kDwellTicks) {
    for (uint8_t i = 0; i < count; ++i)
        exploration::update(s);
}
inline bool load(world_test::FakeWorld &reader) {
    std::ifstream input("fxdata/tables/exploration.bin", std::ios::binary);
    return static_cast<bool>(input.read(reinterpret_cast<char *>(reader.tiles), world::WORLD_BYTES));
}
inline std::vector<uint8_t> reachable(world_test::FakeWorld &reader, uint8_t tools) {
    game::State s;
    game::resetWorld(s);
    s.tools = tools;
    if (tools & exploration_data::CUTTER)
        s.openGates = (1u << exploration_data::GATE_COUNT) - 1;
    std::vector<uint8_t> seen(world::TILES_X * world::TILES_Y, 0);
    std::queue<std::pair<int16_t, int16_t>> queue;
    const int16_t sx = world::SPAWN_TILE_X, sy = world::SPAWN_TILE_Y;
    seen[sy * world::TILES_X + sx] = 1;
    queue.push({sx, sy});
    const int8_t dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
    while (!queue.empty()) {
        const auto point = queue.front();
        queue.pop();
        for (uint8_t dir = 0; dir < 4; ++dir) {
            const int16_t x = point.first + dx[dir], y = point.second + dy[dir];
            if (!world::inTileBounds(x, y) || seen[y * world::TILES_X + x])
                continue;
            if (game::hullHits(reader, nullptr, x * 8, y * 8, &s))
                continue;
            seen[y * world::TILES_X + x] = 1;
            queue.push({x, y});
        }
    }
    return seen;
}
}   // namespace exploration_test
inline void ExplorationSuite(TestRunner &runner) {
    namespace data = exploration_data;
    using namespace exploration_test;
#if SH_FEATURE_STATIONS
    {
        Test t("wall berths dock and undock safely against the actual packed terrain");
        world_test::FakeWorld reader;
        t.assertTrue(load(reader), "actual collision plane loads");
        for (uint8_t id = 0; id < world::settlementCount(); ++id) {
            world::Settlement dock;
            world::settlementInfo(id, dock);
            game::State s;
            game::resetWorld(s);
            s.tools = data::CUTTER | data::SEAL | data::THRUSTER;
            place(s, dock.pixel_x, dock.pixel_y);
            t.assertTrue(!game::hullHits(reader, nullptr, s.x, s.y, &s), "snapped hull is clear at the wall");
            t.assertTrue(station::dock(s), "actual berth docks");
            t.assertEqual(s.docked, static_cast<int8_t>(id), "correct settlement opens");
            t.assertTrue(station::undock(s), "undock leaves the berth");
            t.assertTrue(!station::stepFlight(s, game::Input{}, reader), "ring latch prevents immediate redock");
            for (int16_t rise = 4; rise <= 48; rise += 4) {
                place(s, dock.pixel_x, dock.pixel_y - rise);
                t.assertTrue(!game::hullHits(reader, nullptr, s.x, s.y, &s), "whole escape column clears the hull");
                t.assertTrue(!station::dock(s), "escape column cannot redock while latched");
            }
            place(s, dock.pixel_x, dock.pixel_y);
            t.assertTrue(station::dock(s), "return to the wall docks after leaving the ring");
        }
        runner.add(t);
    }
#endif
    {
        Test t("exploration salvage requires a slow uninterrupted dwell and pays once");
        game::State s;
        game::resetWorld(s);
        const auto p = data::pickup(data::PICKUP_TC);
        place(s, p.x, p.y);
        dwell(s, exploration::kDwellTicks - 1);
        t.assertEqual(s.credits, 0, "no instant reward");
        place(s, p.x + 80, p.y);
        dwell(s, 1);
        place(s, p.x, p.y);
        dwell(s, exploration::kDwellTicks - 1);
        t.assertEqual(s.credits, 0, "leaving resets progress");
        dwell(s, 1);
        t.assertEqual(s.credits, p.amount, "complete dwell pays the purse");
        dwell(s);
        t.assertEqual(s.credits, p.amount, "standing on claimed cove cannot pay twice");
        game::resetWorld(s);
        place(s, p.x, p.y);
        s.vxQ8 = exploration::kOperateSpeed + 1;
        dwell(s);
        t.assertEqual(s.pickups, 0, "fast pass cannot salvage");
        s.vxQ8 = 0;
        s.docked = 0;
        dwell(s);
        t.assertEqual(s.pickups, 0, "docked state cannot salvage");
        runner.add(t);
    }
    {
        Test t("found cutter opens real grates after operation and cache cannot bypass a gate");
        game::State s;
        game::resetWorld(s);
        const auto g = data::gate(data::GATE_E_V);
        world_test::FakeWorld reader;
        world::TileCache cache;
        world::clearCache(cache);
        world::refreshCache(cache, reader, g.x / 8 - 5, g.y / 8 - 4);
        t.assertTrue(game::hullHits(reader, &cache, g.x + g.w / 2, g.y + g.h / 2, &s), "closed gate overrides open cached tiles");
        const auto cutter = data::pickup(data::PICKUP_W);
        place(s, cutter.x, cutter.y);
        dwell(s);
        t.assertTrue((s.tools & data::CUTTER) != 0, "salvage equips the cutter");
        t.assertTrue(exploration::closed(s, data::GATE_E_V, g), "owning cutter alone does not silently erase grate");
        place(s, g.x - 14, g.y + g.h / 2);
        dwell(s);
        t.assertTrue(!exploration::closed(s, data::GATE_E_V, g), "slow operation opens grate");
        t.assertTrue(!game::hullHits(reader, &cache, g.x + g.w / 2, g.y + g.h / 2, &s), "same tile cache now permits passage");
        t.assertTrue((s.seenGates & (1u << data::GATE_E_V)) != 0, "encounter remains charted");
        runner.add(t);
    }
    {
        Test t("all gate types stop hull motion from both directions without damaging impacts");
        world_test::FakeWorld reader;
        for (uint8_t id = 0; id < data::GATE_COUNT; ++id) {
            const auto g = data::gate(id);
            for (uint8_t side = 0; side < 2; ++side) {
                game::State s;
                game::resetWorld(s);
                const bool horizontalMove = g.w < g.h;
                const int16_t x = horizontalMove ? (side ? g.x + g.w + 13 : g.x - 13) : g.x + g.w / 2;
                const int16_t y = horizontalMove ? g.y + g.h / 2 : (side ? g.y + g.h + 4 : g.y - 4);
                place(s, x, y);
                if (horizontalMove) {
                    s.vxQ8 = side ? -5120 : 5120;
                    s.xFrac = side ? 0 : 4095;
                } else {
                    s.vyQ8 = side ? -5120 : 5120;
                    s.yFrac = side ? 0 : 4095;
                }
                t.assertTrue(game::moveAxis(s, reader, nullptr, horizontalMove), "barrier blocks attempted crossing");
                t.assertEqual(s.x, x, "blocked x stays outside");
                t.assertEqual(s.y, y, "blocked y stays outside");
                t.assertTrue(!game::hardWallImpact(s), "equipment barrier is not a damaging rock impact");
                s.tools = g.required;
                s.openGates = static_cast<uint16_t>(1u) << id;
                if (horizontalMove) {
                    s.vxQ8 = side ? -5120 : 5120;
                    s.xFrac = side ? 0 : 4095;
                } else {
                    s.vyQ8 = side ? -5120 : 5120;
                    s.yFrac = side ? 0 : 4095;
                }
                t.assertTrue(!game::moveAxis(s, reader, nullptr, horizontalMove), "correct equipment allows crossing");
            }
        }
        game::State s;
        game::resetWorld(s);
        const auto combined = data::gate(data::GATE_Q_J);
        s.tools = data::THRUSTER;
        t.assertTrue(exploration::closed(s, data::GATE_Q_J, combined), "return shaft cannot bypass pressure requirement");
        s.tools = data::SEAL;
        t.assertTrue(exploration::closed(s, data::GATE_Q_J, combined), "return shaft also needs thruster");
        runner.add(t);
    }
    {
        Test t("hidden rewards saturate money and add hull capacity beyond shop maximum");
        game::State s;
        game::resetWorld(s);
        s.credits = 65530;
        const auto purse = data::pickup(data::PICKUP_DC);
        place(s, purse.x, purse.y);
        dwell(s);
        t.assertEqual(s.credits, 65535, "large treasure cannot wrap credits");
#if SH_FEATURE_HULL_DAMAGE
#if SH_FEATURE_SHOP
        s.hullLevel = 3;
#endif
        const uint8_t oldCapacity = game::hullCapacity(s);
        const auto pod = data::pickup(data::PICKUP_CC);
        place(s, pod.x, pod.y);
        dwell(s);
        t.assertEqual(game::hullCapacity(s), oldCapacity + 1, "find remains useful at max shop level");
        t.assertEqual(s.hull, game::hullCapacity(s), "new hull capacity is repaired on pickup");
#endif
        game::resetWorld(s);
        t.assertEqual(s.tools, 0, "restart resets run-local equipment");
        t.assertEqual(s.pickups, 0, "restart resets run-local claims");
        runner.add(t);
    }
    {
        Test t("actual packed cave respects tools using native hull and gate collision");
        world_test::FakeWorld reader;
        t.assertTrue(load(reader), "active packed terrain loads");
        auto seen = reachable(reader, 0);
        t.assertTrue(seen[(data::ROOM_W_Y / 8) * world::TILES_X + data::ROOM_W_X / 8], "starting boat can reach cutter");
        t.assertTrue(!seen[(data::ROOM_BR_Y / 8) * world::TILES_X + data::ROOM_BR_X / 8], "thruster is cutter-gated");
        t.assertTrue(!seen[(data::ROOM_D_Y / 8) * world::TILES_X + data::ROOM_D_X / 8], "deep berth is pressure-gated");
        seen = reachable(reader, data::CUTTER);
        t.assertTrue(seen[(data::ROOM_CW_Y / 8) * world::TILES_X + data::ROOM_CW_X / 8], "cutter reaches pressure seal");
        t.assertTrue(seen[(data::ROOM_BR_Y / 8) * world::TILES_X + data::ROOM_BR_X / 8], "cutter reaches optional thruster");
        seen = reachable(reader, data::CUTTER | data::THRUSTER);
        t.assertTrue(!seen[(data::ROOM_D_Y / 8) * world::TILES_X + data::ROOM_D_X / 8], "jets cannot bypass pressure band");
        seen = reachable(reader, data::CUTTER | data::SEAL);
        t.assertTrue(seen[(data::ROOM_D_Y / 8) * world::TILES_X + data::ROOM_D_X / 8], "deep berth reachable without optional thruster");
        runner.add(t);
    }
    {
        Test t("organic passage pillar is a real hull obstacle with room to steer around it");
        world_test::FakeWorld reader;
        t.assertTrue(load(reader), "packed organic terrain loads");
        game::State s;
        game::resetWorld(s);
        s.tools = 7;
        s.openGates = (1u << data::GATE_COUNT) - 1;
        const int16_t x = data::ROCK_T_L_ROCK_X, y = data::ROCK_T_L_ROCK_Y;
        t.assertTrue(game::hullHits(reader, nullptr, x, y, &s), "pillar blocks the previous center line");
        bool above = false, below = false;
        for (int16_t offset = 32; offset <= 48; offset += 4) {
            above = above || !game::hullHits(reader, nullptr, x, y - offset, &s);
            below = below || !game::hullHits(reader, nullptr, x, y + offset, &s);
        }
        t.assertTrue(above, "upper flank clears the actual hull");
        t.assertTrue(below, "lower flank offers another way around");
        runner.add(t);
    }
    {
        Test t("freight offers avoid pressure-locked berths until the seal is found");
        game::State s;
        game::resetWorld(s);
        for (uint8_t origin = 0; origin < 3; ++origin) {
            for (uint8_t board = 0; board < 16; ++board) {
                jobs::refreshOffers(s, origin);
                for (uint8_t i = 0; i < s.offerCount; ++i)
                    t.assertTrue(s.offers[i].to != 3, "starting offers remain reachable");
            }
        }
        s.tools = exploration_data::SEAL;
        bool offered = false;
        for (uint8_t board = 0; board < 16; ++board) {
            jobs::refreshOffers(s, 0);
            for (uint8_t i = 0; i < s.offerCount; ++i)
                offered = offered || s.offers[i].to == 3;
        }
        t.assertTrue(offered, "seal unlocks delivery offers to the deep station");
        runner.add(t);
    }
#if SH_FEATURE_SONAR
    {
        Test t("found survey lens reveals extra contacts without extra sonar air cost");
        game::State base, lens;
        game::resetWorld(base);
        game::resetWorld(lens);
        place(base, 1000, 600);
        place(lens, 1000, 600);
        lens.tools = data::LENS;
        chart::State first, second;
        chart::reset(first);
        chart::reset(second);
        sonar::fire(base, first, 0);
        sonar::fire(lens, second, 0);
        t.assertEqual(base.air, lens.air, "lens expands survey without charging more air");
        const int16_t cx = chart::cellFromPixelX(lens.x), cy = chart::cellFromPixelY(lens.y);
        t.assertTrue(!chart::isExplored(first, cx + 5, cy), "ordinary sonar stops short");
        t.assertTrue(chart::isExplored(second, cx + 5, cy), "lens reaches the distant contact cell");
        runner.add(t);
    }
#endif
    {
        Test t("discovery rendering clips glyphs and gates at every edge of the display");
        game::State s;
        game::resetWorld(s);
        uint8_t guarded[1040];
        for (uint8_t i = 0; i < data::PICKUP_COUNT; ++i) {
            const auto p = data::pickup(i);
            place(s, p.x, p.y);
            for (uint16_t j = 0; j < 1040; ++j)
                guarded[j] = 0xA5;
            exploration_view::draw(guarded + 8, s, cave::cameraCenteredAtPixel(s.x + 60, s.y + 25));
            for (uint8_t j = 0; j < 8; ++j) {
                t.assertEqual(guarded[j], 0xA5, "prefix guard intact");
                t.assertEqual(guarded[1032 + j], 0xA5, "suffix guard intact");
            }
        }
        runner.add(t);
    }
}
#endif
