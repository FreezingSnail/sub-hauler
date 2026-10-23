#pragma once

#include "../src/currents.hpp"
#include "../src/hazard_view.hpp"
#include "../src/pilot.hpp"
#include "test.hpp"
#include "world_test.hpp"

#if SH_FEATURE_CURRENTS

namespace currents_test {

using Fixture = world_test::FakeWorld;

inline game::Input idle() {
    return game::Input{false, false, false, false};
}

inline void fillTiles(Fixture &fake, int16_t tx0, int16_t ty0, int16_t tx1, int16_t ty1, bool solid) {
    for (int16_t ty = ty0; ty <= ty1; ++ty)
        for (int16_t tx = tx0; tx <= tx1; ++tx)
            fake.setTile(tx, ty, solid);
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

inline int32_t distanceSqToRect(int16_t px, int16_t py, const world::Current &zone) {
    const int16_t right = static_cast<int16_t>(zone.x + zone.w - 1);
    const int16_t bottom = static_cast<int16_t>(zone.y + zone.h - 1);
    const int16_t nx = px < zone.x ? zone.x : (px > right ? right : px);
    const int16_t ny = py < zone.y ? zone.y : (py > bottom ? bottom : py);
    const int32_t dx = static_cast<int32_t>(px - nx);
    const int32_t dy = static_cast<int32_t>(py - ny);
    return dx * dx + dy * dy;
}

}   // namespace currents_test

inline void CurrentsSuite(TestRunner &runner) {
    using namespace currents_test;

    static_assert(world::CURRENT_COUNT > 0, "the shipped world has current zones");

    {
        Test test("current zone containment is a half-open rectangle");
        world::Current zone = {100, 200, 32, 16, 4096, 0};
        test.assertTrue(currents::contains(zone, 100, 200), "top-left pixel is inside");
        test.assertTrue(currents::contains(zone, 131, 215), "bottom-right pixel is inside");
        test.assertTrue(!currents::contains(zone, 132, 200), "right edge is outside");
        test.assertTrue(!currents::contains(zone, 100, 216), "bottom edge is outside");
        test.assertTrue(!currents::contains(zone, 99, 200), "left is outside");
        test.assertTrue(!currents::contains(zone, 100, 199), "top is outside");
        runner.add(test);
    }

    {
        Test test("zone acceleration integrates in Q8 and clamps");
        game::State state;
        game::reset(state);
        state.vxQ8 = 0;
        state.vyQ8 = 0;

        world::Current zone = {0, 0, 32, 16, 4096, 0};
        currents::applyZone(state, zone);
        test.assertEqual(state.vxQ8, static_cast<int16_t>(4096 / game::kStepHz), "one tick adds Q8 acceleration/Hz");
        test.assertEqual(state.vyQ8, 0, "the other axis is untouched");

        for (uint8_t i = 0; i < 200; ++i)
            currents::applyZone(state, zone);
        test.assertEqual(state.vxQ8, currents::kMaxDriftQ8, "drift clamps at the current cap");

        world::Current up = {0, 0, 32, 16, 0, -4096};
        state.vyQ8 = 0;
        currents::applyZone(state, up);
        test.assertEqual(state.vyQ8, static_cast<int16_t>(-4096 / game::kStepHz), "negative vertical acceleration");
        runner.add(test);
    }

    {
        Test test("generated current metadata clears docks and spawn");
        world::Settlement settlement;
        const int32_t clearanceSq = static_cast<int32_t>(world::DOCK_CLEARANCE_PX) * world::DOCK_CLEARANCE_PX;
        uint8_t horizontal = 0;
        uint8_t vertical = 0;

        world::Current zone;
        for (uint8_t id = 0; id < world::CURRENT_COUNT; ++id) {
            test.assertTrue(world::currentInfo(id, zone), "current metadata loads");
            test.assertTrue(zone.w > 0 && zone.h > 0, "zone has a positive size");
            test.assertTrue(zone.ax_q8 != 0 || zone.ay_q8 != 0, "zone has acceleration");
            if (zone.ax_q8 != 0)
                ++horizontal;
            if (zone.ay_q8 != 0)
                ++vertical;
            test.assertTrue(zone.x >= 0 && zone.y >= 0, "zone starts inside the world");
            test.assertTrue(static_cast<int32_t>(zone.x + zone.w) <= world::PIXELS_X, "zone stays inside world x");
            test.assertTrue(static_cast<int32_t>(zone.y + zone.h) <= world::PIXELS_Y, "zone stays inside world y");

            for (uint8_t s = 0; s < world::SETTLEMENT_COUNT; ++s) {
                test.assertTrue(world::settlementInfo(s, settlement), "settlement metadata loads");
                test.assertTrue(distanceSqToRect(settlement.pixel_x, settlement.pixel_y, zone) >= clearanceSq,
                                "zone stays outside every dock ring");
            }
            test.assertTrue(!currents::contains(zone, world::SPAWN_SUB_X, world::SPAWN_SUB_Y), "zone excludes spawn hull");
        }
        test.assertTrue(horizontal > 0, "at least one horizontal current");
        test.assertTrue(vertical > 0, "at least one vertical current");
        test.assertTrue(!world::currentInfo(world::CURRENT_COUNT, zone), "out-of-range metadata is rejected");
        runner.add(test);
    }

    {
        Test test("outside every zone leaves velocity unchanged");
        game::State state;
        game::resetWorld(state);

        world::Current zone;
        test.assertTrue(!currents::findAt(world::SPAWN_SUB_X, world::SPAWN_SUB_Y, zone), "spawn is outside all zones");
        state.vxQ8 = 123;
        state.vyQ8 = -45;
        currents::apply(state);
        test.assertEqual(state.vxQ8, 123, "x velocity unchanged outside zones");
        test.assertEqual(state.vyQ8, -45, "y velocity unchanged outside zones");

        world::Current first;
        world::currentInfo(0, first);
        test.assertTrue(currents::findAt(static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2), zone),
                        "zone centre is found");
        test.assertEqual(zone.x, first.x, "findAt returns the containing zone");
        runner.add(test);
    }

    {
        Test test("world step applies a generated current before collision");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Current first;
        world::currentInfo(0, first);
        place(state, static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2));

        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.vxQ8, static_cast<int16_t>(first.ax_q8 / game::kStepHz), "stepWorld applied the current acceleration");
        test.assertEqual(state.vyQ8, 0, "idle trim produced no vertical thrust");
        runner.add(test);
    }

#if SH_FEATURE_STATIONS
    {
        Test test("docked boats are never pushed by a current");
        game::State state;
        game::resetWorld(state);
        world::Current first;
        world::currentInfo(0, first);
        place(state, static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2));
        state.docked = 0;
        state.vxQ8 = 0;

        currents::apply(state);
        test.assertEqual(state.vxQ8, 0, "docked boat ignores the zone");

        Fixture fake;
        state.docked = 0;
        game::stepWorld(state, idle(), fake);
        test.assertEqual(state.vxQ8, 0, "stepWorld does not push a docked boat");
        runner.add(test);
    }
#endif

    {
        Test test("current cannot push the hull through a wall");
        Fixture fake;
        fillTiles(fake, 10, 0, 10, 159, true);   // solid column at x=80
        game::State state;
        game::resetWorld(state);
        place(state, 67, 80);                     // hull right edge at 79

        world::Current zone = {0, 0, 255, 255, 4096, 0};
        bool crossed = false;
        for (uint16_t i = 0; i < 240; ++i) {
            game::stepPhysics(state, idle());
            currents::applyZone(state, zone);
            game::moveAxis(state, fake, nullptr, true);
            game::moveAxis(state, fake, nullptr, false);
            if (state.x > 67)
                crossed = true;
        }
        test.assertTrue(!crossed, "current never pushes the hull into rock");
        test.assertEqual(state.x, 67, "hull stays at the last valid pixel");
        runner.add(test);
    }

    {
        Test test("pilot view draws readable flow marks inside a zone");
        game::State state;
        game::resetWorld(state);
        world::Current first;
        world::currentInfo(0, first);
        place(state, static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2));

        uint8_t buffer[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 8];
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0xA5;
        hazard_view::drawCurrents(buffer, state, pilot_view::cameraForState(state));
        uint16_t drawn = 0;
        for (uint16_t i = 0; i < cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8; ++i)
            drawn += static_cast<uint16_t>(buffer[i] != 0xA5);
        test.assertTrue(drawn > 0, "flow marks wrote visible pixels");
        for (uint16_t i = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8; i < sizeof(buffer); ++i)
            test.assertEqual(buffer[i], 0xA5, "flow marks never write past the frame");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_CURRENTS
