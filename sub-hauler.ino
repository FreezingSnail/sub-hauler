// Arduboy FX game template: native 1-bit build.
//
// The rules live in src/game.hpp (host-tested); this sketch owns hardware
// setup, input sampling, and the update/render split.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/game.hpp"
#include "src/render.hpp"

decltype(arduboy) arduboy;

static game::State s_game;

static game::Input readInput() {
    game::Input input;
    input.left = arduboy.pressed(LEFT_BUTTON);
    input.right = arduboy.pressed(RIGHT_BUTTON);
    input.jump = arduboy.justPressed(A_BUTTON);
    return input;
}

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    game::reset(s_game);
}

void run() {
    game::step(s_game, readInput());
}

void render() {
    view::frame(s_game);
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();
    run();
    render();
    // The OLED and the FX cart share SPI: all cart reads happen in the
    // render/update phase, then one buffered display transaction sends it.
    FX::display(CLEAR_BUFFER);
}
