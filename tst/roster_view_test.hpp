#pragma once

#include "../src/game.hpp"
#include "../src/hud.hpp"
#include "../src/pilot_view.hpp"
#include "test.hpp"


// HUD layout coverage retained from the former title/roster suite. The hull
// selector is gone; title row behavior lives in VoyageViewSuite, while this
// suite keeps the shared 128x64 HUD fit checks against the HAULER footprint.
inline void TitleSuite(TestRunner &runner) {
    {
        Test test("HUD: panels fit 128x64 and clear the HAULER footprint");
        test.assertTrue(hud::kAirLabelX + 3 * 6 <= 128 && hud::kAirLabelY + 7 <= 64, "AIR label inside");
        test.assertTrue(hud::kAirX + hud::kAirW <= 128 && hud::kAirY + hud::kAirH <= 64, "air bar inside");
        test.assertTrue(hud::kGaugeX + hud::kGaugeW + 3 <= 128 && hud::kGaugeY + hud::kGaugeH <= 64, "ballast gauge plus notch inside");
        test.assertEqual(hud::neutralNotchY(0), 24, "empty neutral notch sits high");
        test.assertEqual(hud::neutralNotchY(game::kCargoMax), 37, "full-hold neutral notch drops toward the loaded trim");
        test.assertTrue(hud::neutralNotchY(0) > hud::kGaugeY, "empty notch clears the gauge top");
        test.assertTrue(hud::neutralNotchY(game::kCargoMax) < hud::kGaugeY + hud::kGaugeH, "loaded notch clears the gauge bottom");
        test.assertTrue(hud::kHintY + 7 <= 64, "hint inside");

        test.assertTrue(hud::kTopBackingX == 0 && hud::kTopBackingX + hud::kTopBackingW <= 128 && hud::kTopBackingY + hud::kTopBackingH <= 64, "top backing inside");
        test.assertTrue(hud::kTopBackingY <= hud::kAirLabelY && hud::kTopBackingY + hud::kTopBackingH >= hud::kAirLabelY + 7, "top backing covers AIR label");
        test.assertTrue(hud::kTopBackingY <= hud::kAirY && hud::kTopBackingY + hud::kTopBackingH >= hud::kAirY + hud::kAirH, "top backing covers air bar");
        test.assertTrue(hud::kGaugeBackingX <= hud::kGaugeX && hud::kGaugeBackingX + hud::kGaugeBackingW >= hud::kGaugeX + hud::kGaugeW + 3, "gauge backing covers notch");
        test.assertTrue(hud::kGaugeBackingY <= hud::kGaugeY && hud::kGaugeBackingY + hud::kGaugeBackingH >= hud::kGaugeY + hud::kGaugeH, "gauge backing covers gauge");
        test.assertTrue(hud::kHintBackingX == 0 && hud::kHintBackingX + hud::kHintBackingW <= 128, "hint backing inside");
        test.assertTrue(hud::kHintBackingY <= hud::kHintY && hud::kHintBackingY + hud::kHintBackingH >= hud::kHintY + 7, "hint backing covers text");

        game::State state{};
        game::resetWorld(state);
        const cave::Camera camera = pilot_view::cameraForState(state);
        const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
        const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);
        const int16_t right = static_cast<int16_t>(left + game::kHullW);
        const int16_t bottom = static_cast<int16_t>(top + game::kHullH);

        const auto overlaps = [&](int16_t x, int16_t y, int16_t w, int16_t h) {
            return left < x + w && right > x && top < y + h && bottom > y;
        };
        test.assertTrue(!overlaps(hud::kAirX, hud::kAirY, hud::kAirW, hud::kAirH), "air bar clear");
        test.assertTrue(!overlaps(hud::kGaugeX, hud::kGaugeY, hud::kGaugeW, hud::kGaugeH), "gauge clear");
        test.assertTrue(!overlaps(hud::kHintX, hud::kHintY, 125, 7), "hint clear");
        test.assertTrue(!overlaps(hud::kTopBackingX, hud::kTopBackingY, hud::kTopBackingW, hud::kTopBackingH), "top backing clear");
        test.assertTrue(!overlaps(hud::kGaugeBackingX, hud::kGaugeBackingY, hud::kGaugeBackingW, hud::kGaugeBackingH), "gauge backing clear");
        test.assertTrue(!overlaps(hud::kHintBackingX, hud::kHintBackingY, hud::kHintBackingW, hud::kHintBackingH), "hint backing clear");
        runner.add(test);
    }
}
