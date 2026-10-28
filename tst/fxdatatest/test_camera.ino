// Permanent device regression for the follow camera (look-ahead and easing).
//
// No cart reads are needed: the camera rules and the 8-px tile floor are pure
// integer math over generated world constants. This suite covers the spawn
// snap, forward look-ahead, reversal without a snap, stopping back to centred
// framing, world-edge clamps and hull visibility at full lead.
#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "src/features.hpp"
#include "src/game.hpp"
#include "src/pilot.hpp"
#include "src/pilot_view.hpp"

Arduboy2Base arduboy;

namespace {

// Position the boat and snap exactly as resetWorld would after a spawn.
void place(game::State &state, int16_t x, int16_t y) {
    state.x = x;
    state.y = y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    game::snapCamera(state);
}

bool hullVisible(const game::State &state) {
    const cave::Camera camera = pilot_view::cameraForState(state);
    const int16_t left = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS - game::kHullHalfH);
    return pilot_view::visible(left, top);
}

}   // namespace

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);

    bool ok = true;
    game::State state;

    // Spawn/Continue framing: zero velocity snaps to the centred target.
    game::resetWorld(state);
    ok = (state.vxQ8 == 0 && state.vyQ8 == 0 && game::cameraLookAheadQ8(state.vxQ8) == 0 && game::cameraLookAheadQ8(state.vyQ8) == 0) && ok;
    ok = (game::cameraLookAheadQ8(1) == 0 && game::cameraLookAheadQ8(-1) == 0 && game::cameraLookAheadQ8(511) == 0 && game::cameraLookAheadQ8(-511) == 0 &&
          game::cameraLookAheadQ8(512) == 1 && game::cameraLookAheadQ8(-512) == -1) &&
         ok;
    ok = (state.cameraX == game::cameraTargetX(state) && state.cameraY == game::cameraTargetY(state)) && ok;
    ok = (state.cameraX == state.x - game::kCameraScreenX && state.cameraY == state.y - game::kCameraScreenY) && ok;

    // Forward/right+down travel leads by half a pixel per px/s.
    place(state, 500, 300);
    ok = (state.cameraX == 456 && state.cameraY == 268) && ok;
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    ok = (game::cameraLookAheadQ8(state.vxQ8) == 15 && game::cameraLookAheadQ8(state.vyQ8) == 10) && ok;
    ok = (game::cameraTargetX(state) == 471 && game::cameraTargetY(state) == 278) && ok;
    for (uint8_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    ok = (state.cameraX == 471 && state.cameraY == 278 && hullVisible(state)) && ok;

    // Reversal flips the lead; the first update is a quarter step, not a snap.
    state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
    state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
    ok = (game::cameraTargetX(state) == 441 && game::cameraTargetY(state) == 258) && ok;
    game::updateCamera(state);
    ok = (state.cameraX == 464 && state.cameraY == 273) && ok;
    ok = (state.cameraX > 441 && state.cameraX < 471 && state.cameraY > 258 && state.cameraY < 278) && ok;
    for (uint8_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    ok = (state.cameraX == 441 && state.cameraY == 258 && hullVisible(state)) && ok;

    // Stopping decays the target back to centred framing without overshoot.
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    for (uint8_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    ok = (game::cameraTargetX(state) == 456 && game::cameraTargetY(state) == 268) && ok;
    game::updateCamera(state);
    ok = (state.cameraX == 468 && state.cameraY == 276) && ok;
    for (uint8_t i = 0; i < 100; ++i) {
        game::updateCamera(state);
        ok = (state.cameraX >= 456 && state.cameraY >= 268) && ok;
    }
    ok = (state.cameraX == 456 && state.cameraY == 268) && ok;

    // Right/bottom edge: the target clamps and the hull stays visible.
    place(state, 2500, 1250);
    state.cameraX = 0;
    state.cameraY = 0;
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    ok = (game::cameraTargetX(state) == game::kCameraMaxX && game::cameraTargetY(state) == game::kCameraMaxY) && ok;
    for (uint8_t i = 0; i < 200; ++i)
        game::updateCamera(state);
    ok = (state.cameraX == game::kCameraMaxX && state.cameraY == game::kCameraMaxY && hullVisible(state)) && ok;

    // Left/top edge: the target clamps at zero and the hull stays visible.
    place(state, 50, 30);
    state.cameraX = game::kCameraMaxX;
    state.cameraY = game::kCameraMaxY;
    state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
    state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
    ok = (game::cameraTargetX(state) == 0 && game::cameraTargetY(state) == 0) && ok;
    for (uint8_t i = 0; i < 200; ++i)
        game::updateCamera(state);
    ok = (state.cameraX == 0 && state.cameraY == 0 && hullVisible(state)) && ok;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
