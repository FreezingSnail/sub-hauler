#pragma once

#include "common.hpp"
#include "fxdata.h"
#include "game.hpp"
#include "generated/sprites.hpp"

namespace view {

inline void background() {
    arduboy.fillScreen(BLACK);
    arduboy.fillRect(0, 38, 128, 6, WHITE);
    arduboy.fillRect(0, 44, 128, 4, BLACK);
    arduboy.fillRect(0, game::kGroundY, 128, game::kScreenHeight - game::kGroundY, WHITE);
    arduboy.fillRect(0, game::kGroundY, 128, 1, BLACK);
    arduboy.fillCircle(104, 12, 6, WHITE);
}

inline void drawHero(const game::State &state) {
    // The generated blob starts with a two-byte width/height header; Blit
    // addresses the headerless pixel bytes, so skip it once.
    Blit::draw(
        state.x - game::kHalfWidth,
        state.y - game::kHalfHeight,
        HERO_WIDTH,
        HERO_HEIGHT,
        ::hero + 2,
        FRAME(state.frame),
        Blit::PLUSMASK);
}

inline void hud(const game::State &state) {
    arduboy.setTextColor(WHITE);
    arduboy.setCursor(2, 2);
    arduboy.print(F("SUB-HAULER"));
    arduboy.setCursor(2, 56);
    arduboy.print(F("A JUMP"));
    arduboy.setCursor(112, 56);
    arduboy.print(state.facingLeft ? F("<") : F(">"));
}

inline void frame(const game::State &state) {
    background();
    drawHero(state);
    hud(state);
}

}   // namespace view
