#pragma once

#include <fstream>
#include <vector>

#include "../src/fish_view.hpp"
#include "test.hpp"

inline void FishViewSuite(TestRunner &runner) {
    {
        Test test("fish motion is continuous and repeats at the clock wrap");
        for (uint32_t t = 0; t < 65536; ++t) {
            const int a = fish_view::drift(static_cast<uint16_t>(t));
            const int b = fish_view::drift(static_cast<uint16_t>(t + 1));
            test.assertTrue(a >= -16 && a <= 15 && b - a >= -1 && b - a <= 1, "bounded one-pixel movement");
        }
        runner.add(test);
    }
    {
        Test test("fish schools stay in clear baked terrain");
        std::ifstream input("fxdata/tables/exploration-terrain.bin", std::ios::binary);
        std::vector<uint8_t> terrain((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        test.assertEqual(terrain.size(), cave::TERRAIN_BYTES, "terrain loaded");
        if (terrain.size() == cave::TERRAIN_BYTES) {
            for (uint8_t i = 0; i < fish_view::kSchoolCount; ++i) {
                int16_t x, y;
                fish_view::schoolCentre(i, x, y);
                for (int16_t py = y - 16; py <= y + 16; ++py)
                    for (int16_t px = x - 30; px <= x + 30; ++px)
                        test.assertTrue((terrain[(py / 8) * cave::TERRAIN_ROW_BYTES + px] & (1u << (py & 7))) == 0, "motion envelope is open water");
            }
        }
        runner.add(test);
    }
    {
        Test test("every wildlife site is visible and animated");
        for (uint8_t site = 0; site < fish_view::kSchoolCount; ++site) {
            int16_t x, y;
            fish_view::schoolCentre(site, x, y);
            const cave::Camera camera = cave::cameraCenteredAtPixel(x, y);
            uint8_t a[1024] = {}, b[1024] = {};
            fish_view::draw(a, camera, 0);
            fish_view::draw(b, camera, 32);
            bool any = false, changed = false;
            for (uint16_t i = 0; i < 1024; ++i) {
                any |= a[i] != 0;
                changed |= a[i] != b[i];
            }
            test.assertTrue(any && changed, "site renders and changes over time");
        }
        runner.add(test);
    }
    {
        Test test("fish render in view, animate, and clip off screen");
        uint8_t a[1024] = {}, b[1024] = {}, empty[1024] = {};
        const cave::Camera camera = cave::cameraAtPixel(960, 200);
        fish_view::draw(a, camera, 0);
        fish_view::draw(b, camera, 8);
        bool any = false, changed = false;
        for (uint16_t i = 0; i < 1024; ++i) {
            any |= a[i] != 0;
            changed |= a[i] != b[i];
        }
        test.assertTrue(any && changed, "school is visible and moves");
        fish_view::draw(empty, cave::cameraAtPixel(0, 0), 0);
        for (uint16_t i = 0; i < 1024; ++i)
            test.assertEqual(empty[i], 0, "distant school is not visible");
        runner.add(test);
    }
}
