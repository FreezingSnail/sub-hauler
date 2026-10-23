// Sub-hauler — Arduboy FX.
//
// M2 pilot slice: world-aware submarine, HAULER silhouette, compact HUD, and
// the D-pad flight contract. The sketch owns pilot state, the FX table reader,
// and the visible-window cache; rules and drawing stay in src/ headers.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/pilot.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"

decltype(arduboy) arduboy;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    world::clearCache(s_cache);
    game::resetWorld(s_state);
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();

    // D-pad only: left/right thrust, up blows ballast (rise), down floods
    // (sink). A/B stay reserved for later context actions.
    const game::Input input{arduboy.pressed(UP_BUTTON), arduboy.pressed(DOWN_BUTTON), arduboy.pressed(LEFT_BUTTON), arduboy.pressed(RIGHT_BUTTON)};

    game::stepWorld(s_state, input, s_tables, &s_cache);

    const cave::Camera camera = pilot_view::cameraForState(s_state);
    view::frame(s_state, camera, s_cache, s_tables);
    hud::draw(s_state);

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction.
    FX::display(CLEAR_BUFFER);
}
