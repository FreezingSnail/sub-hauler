#pragma once

#include "../src/station_view.hpp"
#include "../src/generated/menu_sprites.hpp"
#include "test.hpp"

// Generated station menu frames remain cart ABI. The live board is native, but
// the selectors must still address a unique in-sheet frame for every reachable
// board selection; the retired SHOP selections are deliberately unreachable.
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
                state.activeJob.kind = carrying ? game::kJobKindCampaign : game::kJobKindNone;
                if (carrying) {
                    state.activeJob.mission = campaign::kFirst;
                    state.activeJob.from = campaign::kTidewall;
                    state.activeJob.to = campaign::kBenthos;
                    state.activeJob.crates = game::kJobCrates;
                }
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
        test.assertEqual(count, 22, "every live station selection has a unique archival frame");
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
        state.activeJob.kind = game::kJobKindCampaign;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelAutoload, "carrying shows loaded controls");
        state.docked = campaign::kPumpRefuge;
        test.assertEqual(station_view::panelHint(state), station_view::kPanelServiceHint, "service hint overrides carried-job hint");
        runner.add(test);
    }
}
