#pragma once

#include "../src/station_view.hpp"
#include "../src/shop_view.hpp"
#include "../src/generated/menu_sprites.hpp"
#include "test.hpp"

inline void MenuPanelSuite(TestRunner &runner) {
    {
        Test test("all selectable station boards map uniquely to generated frames");
        bool seen[STATION_MENU_FRAMES] = {};
        uint8_t count = 0;
        game::State state;
        game::resetWorld(state);
        for (uint8_t berth = 0; berth < campaign::kBerthCount; ++berth) {
            state.docked = berth;
            for (uint8_t carrying = 0; carrying < 2; ++carrying) {
                state.activeJob.kind = carrying ? game::kJobKindOrdinary : game::kJobKindNone;
                const bool service = station::serviceBerth(state);
                const bool active = !service && station::jobActive(state);
                const uint8_t first = station::firstSelectable(state);
                const uint8_t choices = station::selectableCount(state);
                for (uint8_t n = 0; n < choices; ++n) {
                    const uint8_t row = first + n;
                    test.assertTrue(station_view::panelValidFrame(berth, active, row), "every selectable action has a frame");
                    const uint16_t frame = station_view::panelFrame(berth, active, row);
                    test.assertTrue(frame < STATION_MENU_FRAMES, "frame stays inside generated sheet");
                    if (frame < STATION_MENU_FRAMES && !(service && carrying)) {
                        test.assertTrue(!seen[frame], "different board selections never alias");
                        seen[frame] = true;
                        ++count;
                    }
                    if (service)
                        test.assertEqual(station::rowKind(row, state), station::kRowUndock, "carrying through a service berth keeps only UNDOCK");
                }
            }
        }
        test.assertEqual(count, STATION_MENU_FRAMES, "all generated station frames are reachable");
        runner.add(test);
    }
    {
        Test test("station hint priority survives menu baking");
        game::State state;
        game::resetWorld(state);
        state.docked = 0;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelCutterRumor, "new voyage keeps cutter clue");
        state.tools = exploration_data::CUTTER | exploration_data::THRUSTER | exploration_data::SEAL;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelStation, "equipped idle board shows controls");
        state.activeJob.kind = game::kJobKindOrdinary;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelAutoload, "carrying shows loaded controls");
        state.docked = campaign::kPumpRefuge;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelServiceHint, "service hint overrides carried-job hint");
        runner.add(test);
    }
    {
        Test test("every shop level and selection has a distinct generated screen");
        bool seen[SHOP_MENU_FRAMES] = {};
        uint8_t count = 0;
        for (uint8_t air = 0; air <= shop::kMaxLevel; ++air)
            for (uint8_t row = 0; row < shop::kItemCount; ++row) {
                const uint16_t frame = shop_view::panelFrame(air, row);
                test.assertTrue(shop_view::validPanelFrame(air, row), "purchasable levels and selected rows are valid");
                test.assertTrue(frame < SHOP_MENU_FRAMES, "shop frame is in the generated sheet");
                if (frame < SHOP_MENU_FRAMES) {
                    test.assertTrue(!seen[frame], "each shop state has its own screen");
                    seen[frame] = true;
                    ++count;
                }
            }
        test.assertEqual(count, SHOP_MENU_FRAMES, "every generated shop frame is reachable");
        game::State state;
        game::resetWorld(state);
        state.docked = 0;
        state.credits = 0;
        test.assertEqual(shop_view::panelHint(state, shop::kAir), shop_view::kPanelLowCredits, "low credits stays visible");
        state.airLevel = shop::kMaxLevel;
        test.assertEqual(shop_view::panelHint(state, shop::kAir), shop_view::kPanelMaxLevel, "MAX takes precedence over low credits");
        state.airLevel = 0;
        state.credits = shop::kAirPrice;
        test.assertEqual(shop_view::panelHint(state, shop::kAir), shop_view::kPanelBuy, "affordable upgrade shows buy controls");
        runner.add(test);
    }
}
