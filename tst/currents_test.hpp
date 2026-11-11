#pragma once

#include <array>
#include <fstream>

#include "../src/currents.hpp"
#include "../src/hazard_view.hpp"
#include "../src/pilot.hpp"
#include "test.hpp"
#include "world_test.hpp"


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
        Test test("generated current zones span their actual wall-to-wall cross-section");
        std::array<uint8_t, world::WORLD_BYTES> tiles{};
        std::ifstream file("fxdata/tables/exploration.bin", std::ios::binary);
        test.assertTrue(static_cast<bool>(file), "generated collision plane loads");
        file.read(reinterpret_cast<char *>(tiles.data()), static_cast<std::streamsize>(tiles.size()));
        const auto solid = [&](int16_t tx, int16_t ty) {
            return ((tiles[ty * world::WORLD_ROW_BYTES + (tx >> 3)] >> (tx & 7)) & 1u) != 0;
        };

        world::Current zone;
        bool horizontal = false;
        bool vertical = false;
        for (uint8_t id = 0; id < world::CURRENT_COUNT; ++id) {
            test.assertTrue(world::currentInfo(id, zone), "zone loads");
            test.assertTrue(world::currentGated(id) || (zone.w >= world::CURRENT_WIDTH_PX && zone.h >= world::CURRENT_HEIGHT_PX),
                            "zone keeps the authored flow length on at least one axis");
            test.assertTrue(zone.w > world::CURRENT_WIDTH_PX || zone.h > world::CURRENT_HEIGHT_PX,
                            "zone widens beyond the retired compact cross-section");
            if (zone.ax_q8 != 0)
                horizontal = true;
            else
                vertical = true;
            const int16_t cx = static_cast<int16_t>((zone.x + zone.w / 2) / 8);
            const int16_t cy = static_cast<int16_t>((zone.y + zone.h / 2) / 8);
            test.assertTrue(!solid(cx, cy), "zone centre is navigable water");
            test.assertTrue(static_cast<int16_t>(game::thrustAccelFor(game::kJobCrates)) > static_cast<int16_t>(zone.ax_q8 < 0 ? -zone.ax_q8 : zone.ax_q8) / game::kQ8One,
                            "full-cargo thrust beats the current force");
        }
        test.assertTrue(horizontal && vertical, "both current axes are covered");
        runner.add(test);
    }

    {
        Test test("without the thruster a current blocks upstream motion only");
        game::State state;
        game::reset(state);
        state.tools = 0;
        state.vxQ8 = -200;
        state.vyQ8 = 0;
        world::Current right = {0, 0, 32, 16, 4096, 0};
        currents::applyZone(state, right);   // raw helper stays unconditional
        test.assertEqual(state.vxQ8, static_cast<int16_t>(-200 + 4096 / game::kStepHz), "raw zone helper still accelerates");

        state.vxQ8 = -200;
        currents::blockUpstream(state, right);
        test.assertEqual(state.vxQ8, 0, "upstream velocity is clamped");
        state.vxQ8 = 200;
        currents::blockUpstream(state, right);
        test.assertEqual(state.vxQ8, 200, "downstream velocity is untouched");
        state.vyQ8 = -200;
        world::Current down = {0, 0, 32, 16, 0, 4096};
        currents::blockUpstream(state, down);
        test.assertEqual(state.vyQ8, 0, "vertical upstream velocity is clamped");
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
        Test test("without the thruster a generated tide blocks upstream and pushes idle downstream");
        game::State state;
        game::resetWorld(state);
        world::Current first;
        test.assertTrue(world::currentInfo(0, first), "generated tide loads");
        place(state, static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2));

        state.tools = 0;
        state.vxQ8 = first.ax_q8 > 0 ? -200 : (first.ax_q8 < 0 ? 200 : 0);
        state.vyQ8 = first.ay_q8 > 0 ? -200 : (first.ay_q8 < 0 ? 200 : 0);
        currents::apply(state);
        if (first.ax_q8 != 0)
            test.assertEqual(state.vxQ8, 0, "upstream x is blocked without the thruster");
        if (first.ay_q8 != 0)
            test.assertEqual(state.vyQ8, 0, "upstream y is blocked without the thruster");

        state.vxQ8 = 0;
        state.vyQ8 = 0;
        currents::apply(state);
        if (first.ax_q8 != 0)
            test.assertTrue((state.vxQ8 > 0) == (first.ax_q8 > 0), "tide pushes downstream on x");
        if (first.ay_q8 != 0)
            test.assertTrue((state.vyQ8 > 0) == (first.ay_q8 > 0), "tide pushes downstream on y");
        runner.add(test);
    }

    {
        Test test("owning the thruster keeps tide drift but stops upstream blocking");
        game::State state;
        game::resetWorld(state);
        world::Current first;
        test.assertTrue(world::currentInfo(0, first), "generated tide loads");
        place(state, static_cast<int16_t>(first.x + first.w / 2), static_cast<int16_t>(first.y + first.h / 2));
        state.tools = exploration_data::THRUSTER;

        // Start moving upstream: the tide still decays that velocity toward
        // downstream, but it no longer clamps it to zero.
        state.vxQ8 = first.ax_q8 > 0 ? -200 : (first.ax_q8 < 0 ? 200 : 0);
        state.vyQ8 = first.ay_q8 > 0 ? -200 : (first.ay_q8 < 0 ? 200 : 0);
        const int16_t startVx = state.vxQ8;
        const int16_t startVy = state.vyQ8;
        currents::apply(state);
        if (first.ax_q8 != 0)
            test.assertTrue(first.ax_q8 > 0 ? state.vxQ8 > startVx : state.vxQ8 < startVx,
                            "tide still accelerates an upstream-moving boat downstream");
        if (first.ay_q8 != 0)
            test.assertTrue(first.ay_q8 > 0 ? state.vyQ8 > startVy : state.vyQ8 < startVy,
                            "tide still accelerates an upstream-moving boat downstream");

        // From rest the tide nudges the boat downstream exactly as it does
        // without the thruster.
        state.vxQ8 = 0;
        state.vyQ8 = 0;
        currents::apply(state);
        if (first.ax_q8 != 0)
            test.assertTrue((state.vxQ8 > 0) == (first.ax_q8 > 0), "idle drift follows the tide on x");
        if (first.ay_q8 != 0)
            test.assertTrue((state.vyQ8 > 0) == (first.ay_q8 > 0), "idle drift follows the tide on y");

        for (uint8_t i = 0; i < 200; ++i)
            currents::apply(state);
        test.assertTrue(state.vxQ8 >= -currents::kMaxDriftQ8 && state.vxQ8 <= currents::kMaxDriftQ8,
                        "post-thruster drift keeps the existing cap on x");
        test.assertTrue(state.vyQ8 >= -currents::kMaxDriftQ8 && state.vyQ8 <= currents::kMaxDriftQ8,
                        "post-thruster drift keeps the existing cap on y");
        runner.add(test);
    }

    {
        Test test("idle boat drifts downstream after the thruster");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Current first;
        test.assertTrue(world::currentInfo(0, first), "generated tide loads");
        const int16_t startX = static_cast<int16_t>(first.x + first.w / 2);
        const int16_t startY = static_cast<int16_t>(first.y + first.h / 2);
        place(state, startX, startY);
        state.tools = exploration_data::THRUSTER;

        for (uint16_t i = 0; i < 60; ++i)
            game::stepWorld(state, idle(), fake);

        if (first.ax_q8 != 0)
            test.assertTrue(first.ax_q8 > 0 ? state.x > startX : state.x < startX,
                            "idle x drift follows the tide");
        if (first.ay_q8 != 0)
            test.assertTrue(first.ay_q8 > 0 ? state.y > startY : state.y < startY,
                            "idle y drift follows the tide");
        runner.add(test);
    }

    {
        Test test("thrust makes upstream headway after the thruster");
        Fixture fake;
        game::State state;
        game::resetWorld(state);
        world::Current zone;
        bool found = false;
        for (uint8_t id = 0; id < world::CURRENT_COUNT; ++id) {
            test.assertTrue(world::currentInfo(id, zone), "current metadata loads");
            if (zone.ax_q8 != 0 && zone.w > 8) {
                found = true;
                break;
            }
        }
        test.assertTrue(found, "a horizontal generated tide exists");
        const int16_t startX = static_cast<int16_t>(zone.x + zone.w / 2);
        const int16_t startY = static_cast<int16_t>(zone.y + zone.h / 2);
        place(state, startX, startY);
        state.tools = exploration_data::THRUSTER;
        const game::Input upstream{false, false, zone.ax_q8 > 0, zone.ax_q8 < 0};

        for (uint16_t i = 0; i < 240; ++i)
            game::stepWorld(state, upstream, fake);

        test.assertTrue(zone.ax_q8 > 0 ? state.x < startX : state.x > startX,
                        "thrust outruns the strongest authored tide upstream");
        test.assertTrue(game::thrustAccelFor(game::kJobCrates) > (zone.ax_q8 < 0 ? -zone.ax_q8 : zone.ax_q8) / game::kQ8One,
                        "full-cargo thrust acceleration exceeds the tide acceleration");
        runner.add(test);
    }

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
        Test test("pilot view draws readable flow marks inside a zone after the thruster");
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

        uint8_t unthrusted[cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8 + 8];
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            unthrusted[i] = buffer[i];

        state.tools = exploration_data::THRUSTER;
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            buffer[i] = 0xA5;
        hazard_view::drawCurrents(buffer, state, pilot_view::cameraForState(state));
        bool identical = true;
        for (uint16_t i = 0; i < sizeof(buffer); ++i)
            identical = identical && buffer[i] == unthrusted[i];
        test.assertTrue(identical, "the thruster keeps the same visible flow marks");
        runner.add(test);
    }
}

