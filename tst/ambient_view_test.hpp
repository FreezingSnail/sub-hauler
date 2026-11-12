#pragma once

#include <fstream>
#include <vector>
#include "../src/ambient_view.hpp"
#include "test.hpp"

inline void AmbientViewSuite(TestRunner &runner) {
    Test test("kelp, vent and drifting motes stay in clear scenery and change over time");
    std::ifstream input("fxdata/tables/exploration-terrain.bin", std::ios::binary);
    std::vector<uint8_t> terrain((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    test.assertEqual(terrain.size(), cave::TERRAIN_BYTES, "terrain loaded");
    for (uint8_t i = 0; i < ambient_view::kSiteCount; ++i) {
        int16_t x, y;
        ambient_view::site(i, x, y);
        const bool kelp = i < ambient_view::kKelpCount;
        const bool vent = i >= ambient_view::kKelpCount &&
                          i < ambient_view::kKelpCount + ambient_view::kVentCount;
        const int16_t bottom = kelp ? static_cast<int16_t>(y - 1) : (vent ? y : static_cast<int16_t>(y + 5));
        const int16_t top = kelp ? static_cast<int16_t>(y - 8) : (vent ? static_cast<int16_t>(y - 15) : static_cast<int16_t>(y - 6));
        const int16_t width = kelp ? 3 : (vent ? 1 : 8);
        if (terrain.size() == cave::TERRAIN_BYTES) {
            for (int16_t py = top; py <= bottom; ++py)
                for (int16_t px = x - width; px <= x + width; ++px)
                    test.assertTrue((terrain[(py / 8) * cave::TERRAIN_ROW_BYTES + px] & (1u << (py & 7))) == 0, "animation envelope is clear");
        }
        const cave::Camera camera = cave::cameraCenteredAtPixel(x, y);
        uint8_t a[1024] = {}, b[1024] = {};
        ambient_view::draw(a, camera, 0);
        ambient_view::draw(b, camera, 32);
        bool any = false, changed = false;
        for (uint16_t j = 0; j < 1024; ++j) {
            any |= a[j] != 0;
            changed |= a[j] != b[j];
        }
        test.assertTrue(any && changed, "site is visible and animated");
    }
    runner.add(test);
}
