#pragma once
#include "cave_render.hpp"
#include "exploration.hpp"
namespace exploration_view {
// Three-column glyphs: C, P, T, $, H, L. Pure 1-bit buffer drawing.
inline void glyph(uint8_t *buffer, int16_t x, int16_t y, uint8_t kind) {
    const uint16_t bits = kind == 1 ? 0x724F : kind == 2 ? 0x12EB : kind == 3 ? 0x2497 : kind == 4 ? 0x7DDF : 0x7249;
    for (uint8_t row = 0; row < 5; ++row)
        for (uint8_t col = 0; col < 3; ++col)
            if (bits & (static_cast<uint16_t>(1u) << (row * 3 + col)))
                cave::setPixel(buffer, x + col, y + row);
}
inline void draw(uint8_t *buffer, const game::State &s, const cave::Camera &camera) {
    const int16_t ox = cave::pixelOriginX(camera), oy = cave::pixelOriginY(camera);
    for (uint8_t i = 0; i < exploration_data::GATE_COUNT; ++i) {
        const exploration_data::Gate g = exploration_data::gate(i);
        const int16_t x = g.x - ox, y = g.y - oy;
        if (x >= 128 || y >= 64 || x + g.w < 0 || y + g.h < 0)
            continue;
        if (!exploration::closed(s, i, g))
            continue;
        const bool horizontal = g.w > g.h;
        // Grates/bulkheads, and opposing current marks, share the gate's bounds.
        for (uint8_t n = 0; n < (horizontal ? g.w : g.h); n += 6) {
            if (g.required & exploration_data::THRUSTER) {
                for (uint8_t p = 0; p < 4; ++p)
                    cave::setPixel(buffer, horizontal ? x + n + p : x + p, horizontal ? y + p : y + n + p);
            } else
                cave::fillRect(buffer, horizontal ? x + n : x, horizontal ? y : y + n, horizontal ? 2 : g.w, horizontal ? g.h : 2, true);
        }
        const int16_t cx = x + g.w / 2, cy = y + g.h / 2;
        cave::fillRect(buffer, cx - 3, cy - 4, 7, 9, false);
        glyph(buffer, cx - 1, cy - 2, (g.required & exploration_data::SEAL) ? 2 : (g.required & exploration_data::THRUSTER) ? 3 : 1);
    }
    for (uint8_t i = 0; i < exploration_data::PICKUP_COUNT; ++i) {
        const exploration_data::Pickup p = exploration_data::pickup(i);
        const int16_t x = p.x - ox, y = p.y - oy;
        if (x < -5 || y < -5 || x > 133 || y > 69)
            continue;
        if (s.pickups & (1u << i)) {
            cave::setPixel(buffer, x, y);
            continue;
        }
        cave::fillRect(buffer, x - 5, y - 5, 11, 11, true);
        cave::fillRect(buffer, x - 4, y - 4, 9, 9, false);
        glyph(buffer, x - 1, y - 2, p.kind);
        // Four small brackets invite a slow approach to recover the find.
        cave::setPixel(buffer, x - 7, y);
        cave::setPixel(buffer, x + 7, y);
    }
}
}   // namespace exploration_view
