#pragma once

#include "../src/game.hpp"
#include "../src/hud.hpp"
#include "../src/pilot.hpp"
#include "../src/pilot_view.hpp"
#include "../src/roster.hpp"
#include "../src/roster_view.hpp"
#include "test.hpp"

#if SH_FEATURE_MULTI_HULL

// Title-screen layout and HUD fit. All geometry is pure; draw() itself is
// device-only, so the silhouette is rendered here through the shared
// pilot_view::drawAt path.
inline void TitleSuite(TestRunner &runner) {
    {
        Test test("title: class name, stat, and hint stay inside 128x64");
        for (uint8_t index = 0; index < roster::kClassCount; ++index) {
            const roster::HullClass hull = roster::classAt(index);
            const uint8_t nameWidth = static_cast<uint8_t>(roster::nameLength(hull) * roster_view::kCharW);
            test.assertTrue(roster_view::nameX(hull) + nameWidth <= 128, "name fits the display");
            test.assertTrue(roster_view::statX(hull) + roster_view::statChars(hull) * roster_view::kCharW <= 128, "stat fits the display");
        }
        test.assertTrue(roster_view::centerX(roster_view::kHintChars) + roster_view::kHintChars * roster_view::kCharW <= 128, "hint fits");
        test.assertTrue(roster_view::kHintY + 7 <= roster_view::kScreenH, "hint clears the bottom edge");
        runner.add(test);
    }

    {
        Test test("title: every class silhouette fits between the name and stat");
        constexpr uint16_t kBufferBytes = cave::VIEW_WIDTH * cave::VIEW_HEIGHT / 8;
        for (uint8_t index = 0; index < roster::kClassCount; ++index) {
            const roster::HullClass hull = roster::classAt(index);
            const roster::ClassSpec spec = roster::spec(hull);

            uint8_t buffer[kBufferBytes];
            for (uint16_t i = 0; i < kBufferBytes; ++i)
                buffer[i] = 0;
            pilot_view::drawAt(buffer, roster_view::kArtLeft, roster_view::kArtTop, false, false, hull);

            int16_t minX = 127;
            int16_t maxX = 0;
            int16_t minY = 63;
            int16_t maxY = 0;
            bool onScreen = true;
            for (int16_t y = 0; y < cave::VIEW_HEIGHT; ++y) {
                for (int16_t x = 0; x < cave::VIEW_WIDTH; ++x) {
                    const bool white = ((buffer[(y >> 3) * cave::VIEW_WIDTH + x] >> (y & 7)) & 1u) != 0;
                    if (!white)
                        continue;
                    if (x < minX)
                        minX = x;
                    if (x > maxX)
                        maxX = x;
                    if (y < minY)
                        minY = y;
                    if (y > maxY)
                        maxY = y;
                    if (x >= 128 || y >= 64)
                        onScreen = false;
                }
            }

            test.assertTrue(onScreen, "art stays on screen");
            test.assertTrue(maxY < roster_view::kStatY, "art clears the stat line");
            test.assertEqual(maxX - minX + 1, static_cast<int16_t>(spec.pilotW), "title art width matches the class");
            test.assertEqual(maxY - minY + 1, static_cast<int16_t>(spec.pilotH), "title art height matches the class");
        }
        runner.add(test);
    }

    {
        Test test("HUD: panels fit 128x64 and clear every hull footprint");
        test.assertTrue(hud::kAirLabelX + 3 * 6 <= 128 && hud::kAirLabelY + 7 <= 64, "AIR label inside");
        test.assertTrue(hud::kAirX + hud::kAirW <= 128 && hud::kAirY + hud::kAirH <= 64, "air bar inside");
        test.assertTrue(hud::kGaugeX + hud::kGaugeW + 3 <= 128 && hud::kGaugeY + hud::kGaugeH <= 64, "ballast gauge plus notch inside");
        test.assertTrue(hud::kCargoLabelX + 3 * 6 <= 128 && hud::kCargoLabelY + 7 <= 64, "CGO label inside");
        test.assertTrue(hud::kCargoX + (game::kCargoMax - 1) * hud::kCargoStep + hud::kCargoPip <= 128, "cargo pips inside");
        test.assertTrue(hud::kHintY + 7 <= 64, "hint inside");

        test.assertTrue(hud::kTopBackingX == 0 && hud::kTopBackingX + hud::kTopBackingW <= 128 && hud::kTopBackingY + hud::kTopBackingH <= 64, "top backing inside");
        test.assertTrue(hud::kTopBackingY <= hud::kAirLabelY && hud::kTopBackingY + hud::kTopBackingH >= hud::kAirLabelY + 7, "top backing covers AIR label");
        test.assertTrue(hud::kTopBackingY <= hud::kAirY && hud::kTopBackingY + hud::kTopBackingH >= hud::kAirY + hud::kAirH, "top backing covers air bar");
        test.assertTrue(hud::kTopBackingY <= hud::kCargoLabelY && hud::kTopBackingY + hud::kTopBackingH >= hud::kCargoLabelY + 7, "top backing covers CGO label");
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
        test.assertTrue(!overlaps(hud::kCargoX, hud::kCargoY, (game::kCargoMax - 1) * hud::kCargoStep + hud::kCargoPip, hud::kCargoPip), "cargo clear");
        test.assertTrue(!overlaps(hud::kHintX, hud::kHintY, 125, 7), "hint clear");
        test.assertTrue(!overlaps(hud::kTopBackingX, hud::kTopBackingY, hud::kTopBackingW, hud::kTopBackingH), "top backing clear");
        test.assertTrue(!overlaps(hud::kGaugeBackingX, hud::kGaugeBackingY, hud::kGaugeBackingW, hud::kGaugeBackingH), "gauge backing clear");
        test.assertTrue(!overlaps(hud::kHintBackingX, hud::kHintBackingY, hud::kHintBackingW, hud::kHintBackingH), "hint backing clear");
        runner.add(test);
    }
}

#endif   // SH_FEATURE_MULTI_HULL
