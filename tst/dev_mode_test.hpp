#pragma once
#include "../src/features.hpp"
#if SH_DEV_MODE
#include "../src/dev_mode.hpp"
#include "test.hpp"
#include "world_test.hpp"

inline void DevModeSuite(TestRunner &runner) {
    {
        Test t("developer movement is direct, stops immediately and supports fast travel");
        game::State s;
        game::resetWorld(s);
        const int16_t startX = s.x, startY = s.y;
        dev_mode::step(s, game::Input{true, false, false, true}, false);
        t.assertEqual(s.x, startX + dev_mode::kStepPixels, "right moves directly");
        t.assertEqual(s.y, startY - dev_mode::kStepPixels, "up moves directly");
        dev_mode::step(s, game::Input{false, true, true, false}, true);
        t.assertEqual(s.x, startX + dev_mode::kStepPixels - dev_mode::kFastStepPixels, "fast left is symmetric");
        t.assertEqual(s.y, startY - dev_mode::kStepPixels + dev_mode::kFastStepPixels, "fast down is symmetric");
        const int16_t x = s.x, y = s.y;
        dev_mode::step(s, game::Input{}, false);
        t.assertEqual(s.x, x, "release stops x immediately");
        t.assertEqual(s.y, y, "release stops y immediately");
        dev_mode::step(s, game::Input{true, true, true, true}, true);
        t.assertEqual(s.x, x, "opposite horizontal inputs cancel");
        t.assertEqual(s.y, y, "opposite vertical inputs cancel");
        t.assertTrue(s.facingLeft, "last actual horizontal direction owns facing");
        runner.add(t);
    }
    {
        Test t("developer noclip crosses solid terrain and a closed equipment gate");
        game::State s;
        game::resetWorld(s);
        world_test::FakeWorld solid;
        for (uint16_t i = 0; i < world::WORLD_BYTES; ++i)
            solid.tiles[i] = 0xFF;
        s.x = 400;
        s.y = 400;
        const int16_t start = s.x;
        for (uint8_t i = 0; i < 12; ++i)
            dev_mode::step(s, game::Input{false, false, false, true}, true);
        t.assertEqual(s.x, start + 12 * dev_mode::kFastStepPixels, "solid world cannot stop noclip");
        t.assertTrue(world::rectHits(solid, s.x - 13, s.y - 4, 26, 8), "destination really overlaps rock");
#if SH_FEATURE_EXPLORATION
        const auto gate = exploration_data::gate(exploration_data::GATE_E_V);
        s.tools = 0;
        s.openGates = 0;
        s.x = gate.x - 14;
        s.y = gate.y + gate.h / 2;
        for (uint8_t i = 0; i < 10; ++i)
            dev_mode::step(s, game::Input{false, false, false, true}, true);
        t.assertTrue(s.x - game::kHullHalfW > gate.x + gate.w, "hull crosses closed grate completely");
        t.assertTrue(exploration::closed(s, exploration_data::GATE_E_V, gate), "inspection does not open the gate");
#endif
        runner.add(t);
    }
    {
        Test t("developer travel clamps hull and camera at every map edge");
        game::State s;
        game::resetWorld(s);
        for (uint16_t i = 0; i < 1000; ++i)
            dev_mode::step(s, game::Input{true, false, true, false}, true);
        t.assertEqual(s.x, game::kHullHalfW, "west edge keeps hull inside world");
        t.assertEqual(s.y, game::kHullHalfH, "north edge keeps hull inside world");
        t.assertEqual(s.cameraX, 0, "west camera clamp");
        t.assertEqual(s.cameraY, 0, "north camera clamp");
        for (uint16_t i = 0; i < 1000; ++i)
            dev_mode::step(s, game::Input{false, true, false, true}, true);
        t.assertEqual(s.x, world::PIXELS_X - game::kHullHalfW, "east clamp");
        t.assertEqual(s.y, world::PIXELS_Y - game::kHullHalfH, "south clamp");
        t.assertEqual(s.cameraX, game::kCameraMaxX, "east camera clamp");
        t.assertEqual(s.cameraY, game::kCameraMaxY, "south camera clamp");
        runner.add(t);
    }
    {
        Test t("developer inspection cannot drain supplies, expire a contract or auto dock");
        game::State s;
        game::resetWorld(s);
        s.air = 0;
#if SH_FEATURE_HULL_DAMAGE
        s.hull = 0;
        s.damageCooldown = 20;
#endif
#if SH_FEATURE_STATIONS
        s.docked = 0;
        s.x = world::SPAWN_PIXEL_X;
        s.y = world::SPAWN_PIXEL_Y;
#endif
#if SH_FEATURE_JOBS
        s.activeJob.to = 1;
#if SH_FEATURE_CAMPAIGN
        s.activeJob.kind = game::kJobKindExpress;
        s.activeJob.mission = game::kMissionNone;
        s.activeJob.from = 0;
#endif
        s.activeJob.elapsed = 41;
        s.activeJob.deadline = 42;
#endif
        for (uint16_t i = 0; i < 600; ++i)
            dev_mode::step(s, game::Input{}, false);
        t.assertEqual(s.air, game::airCapacity(s), "air stays full");
#if SH_FEATURE_HULL_DAMAGE
        t.assertEqual(s.hull, game::hullCapacity(s), "hull stays full");
        t.assertEqual(s.damageCooldown, 0, "damage cannot linger");
#endif
#if SH_FEATURE_STATIONS
        t.assertEqual(s.docked, -1, "staying inside a berth cannot auto dock");
#endif
#if SH_FEATURE_JOBS
        t.assertEqual(s.activeJob.elapsed, 41, "inspection does not age contracts");
#endif
        t.assertEqual(s.vxQ8, 0, "no current or inertia velocity");
        t.assertEqual(s.vyQ8, 0, "no ballast velocity");
        runner.add(t);
    }
    {
        Test t("developer chart is complete while discoveries remain inspectable and unclaimed");
        game::State s;
        game::resetWorld(s);
        dev_mode::prepare(s);
#if SH_FEATURE_FOG_CHART
        chart::State chart;
        chart::reset(chart);
        dev_mode::revealChart(chart);
        bool all = true;
        for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
            all = all && chart.explored[i] == 0xFF;
        t.assertTrue(all, "full map visible immediately");
#endif
#if SH_FEATURE_EXPLORATION
        const auto item = exploration_data::pickup(exploration_data::PICKUP_W);
        s.x = item.x;
        s.y = item.y;
        for (uint8_t i = 0; i < 60; ++i)
            dev_mode::step(s, game::Input{}, false);
        t.assertEqual(s.pickups, 0, "finds stay unclaimed");
        t.assertEqual(s.tools, 0, "tool appearances remain ordinary");
        t.assertEqual(s.openGates, 0, "grates remain visible");
        t.assertEqual(s.seenGates, (1u << exploration_data::GATE_COUNT) - 1, "all gate locations charted");
#endif
        runner.add(t);
    }
}
#endif
