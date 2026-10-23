// Sub-hauler — Arduboy FX.
//
// M1 debug view: render the baked cave around the spawn settlement with a
// static camera. Pilot physics, HUD, stations, and cargo arrive later; this
// sketch only owns hardware setup and the one presentation transaction.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/cave.hpp"
#include "src/common.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"

decltype(arduboy) arduboy;

static world::FxTables s_tables;
static world::TileCache s_cache;
static cave::Camera s_camera;

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    world::clearCache(s_cache);
    // Static debug camera centred on the sub spawn at TIDEWALL. The cache
    // window and the 16x8 view derive from this tile position.
    s_camera = cave::cameraCenteredAtPixel(world::SPAWN_SUB_X, world::SPAWN_SUB_Y);
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();

    view::frame(s_camera, s_cache, s_tables);

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction.
    FX::display(CLEAR_BUFFER);
}
