// Permanent device regression for the follow camera (look-ahead and easing).
//
// No cart reads are needed: the camera rules and the 8-px tile floor are pure
// integer math over generated world constants. This suite covers the spawn
// snap, forward look-ahead, reversal without a snap, stopping back to centred
// framing, world-edge clamps and hull visibility at full lead.
#include <Arduboy2.h>
#include <ArduboyFX.h>

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
    const int16_t left = static_cast<int16_t>(pilot_view::screenX(state, camera) - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(pilot_view::screenY(state, camera) - game::kHullHalfH);
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
    ok = (state.vxQ8 == 0 && state.vyQ8 == 0 && game::cameraLookAheadQ4(state.vxQ8) == 0 && game::cameraLookAheadQ4(state.vyQ8) == 0) && ok;
    ok = (game::cameraLookAheadQ4(1) == 0 && game::cameraLookAheadQ4(-1) == 0 && game::cameraLookAheadQ4(31) == 0 && game::cameraLookAheadQ4(-31) == 0 &&
          game::cameraLookAheadQ4(32) == 1 && game::cameraLookAheadQ4(-32) == -1 &&
          game::cameraLookAheadQ4(511) == 15 && game::cameraLookAheadQ4(-511) == -15 &&
          game::cameraLookAheadQ4(512) == 16 && game::cameraLookAheadQ4(-512) == -16) &&
         ok;
    ok = (state.cameraXQ4 == game::cameraTargetXQ4(state) && state.cameraYQ4 == game::cameraTargetYQ4(state)) && ok;
    ok = (game::cameraPixelX(state) == state.x - game::kCameraScreenX && game::cameraPixelY(state) == state.y - game::kCameraScreenY) && ok;

    // Forward/right+down travel leads by half a pixel per px/s.
    place(state, 500, 300);
    ok = (game::cameraPixelX(state) == 456 && game::cameraPixelY(state) == 268) && ok;
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    ok = (game::cameraLookAheadQ4(state.vxQ8) == game::cameraPixelToQ4(15) && game::cameraLookAheadQ4(state.vyQ8) == game::cameraPixelToQ4(10)) && ok;
    ok = (game::cameraTargetXQ4(state) == game::cameraPixelToQ4(471) && game::cameraTargetYQ4(state) == game::cameraPixelToQ4(278)) && ok;
    for (uint16_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    ok = (state.cameraXQ4 == game::cameraPixelToQ4(471) && state.cameraYQ4 == game::cameraPixelToQ4(278) && hullVisible(state)) && ok;

    // Reversal flips the lead; the first correction is bounded, not a snap.
    state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
    state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
    ok = (game::cameraTargetXQ4(state) == game::cameraPixelToQ4(441) && game::cameraTargetYQ4(state) == game::cameraPixelToQ4(258)) && ok;
    const uint16_t aheadX = state.cameraXQ4;
    game::updateCamera(state);
    ok = (state.cameraXQ4 == static_cast<uint16_t>(aheadX - game::kCameraMaxStepQ4)) && ok;
    ok = (state.cameraXQ4 > game::cameraPixelToQ4(441) && state.cameraXQ4 < aheadX) && ok;
    for (uint16_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    ok = (state.cameraXQ4 == game::cameraPixelToQ4(441) && state.cameraYQ4 == game::cameraPixelToQ4(258) && hullVisible(state)) && ok;

    // Stopping decays the target back to centred framing without overshoot.
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    for (uint16_t i = 0; i < 100; ++i)
        game::updateCamera(state);
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    ok = (game::cameraTargetXQ4(state) == game::cameraPixelToQ4(456) && game::cameraTargetYQ4(state) == game::cameraPixelToQ4(268)) && ok;
    game::updateCamera(state);
    ok = (state.cameraXQ4 == static_cast<uint16_t>(game::cameraPixelToQ4(471) - game::kCameraMaxStepQ4)) && ok;
    for (uint16_t i = 0; i < 100; ++i) {
        game::updateCamera(state);
        ok = (state.cameraXQ4 >= game::cameraPixelToQ4(456) && state.cameraYQ4 >= game::cameraPixelToQ4(268)) && ok;
    }
    ok = (state.cameraXQ4 == game::cameraPixelToQ4(456) && state.cameraYQ4 == game::cameraPixelToQ4(268)) && ok;

    // Right/bottom edge: the target clamps and the hull stays visible.
    place(state, 2500, 1250);
    state.cameraXQ4 = 0;
    state.cameraYQ4 = 0;
    state.vxQ8 = game::kMaxVxBaseQ8;
    state.vyQ8 = game::kMaxVyQ8;
    ok = (game::cameraTargetXQ4(state) == game::kCameraMaxXQ4 && game::cameraTargetYQ4(state) == game::kCameraMaxYQ4) && ok;
    for (uint16_t i = 0; i < 1400; ++i)
        game::updateCamera(state);
    ok = (state.cameraXQ4 == game::kCameraMaxXQ4 && state.cameraYQ4 == game::kCameraMaxYQ4 && hullVisible(state)) && ok;

    // Left/top edge: the target clamps at zero and the hull stays visible.
    place(state, 50, 30);
    state.cameraXQ4 = game::kCameraMaxXQ4;
    state.cameraYQ4 = game::kCameraMaxYQ4;
    state.vxQ8 = static_cast<int16_t>(-game::kMaxVxBaseQ8);
    state.vyQ8 = static_cast<int16_t>(-game::kMaxVyQ8);
    ok = (game::cameraTargetXQ4(state) == 0 && game::cameraTargetYQ4(state) == 0) && ok;
    for (uint16_t i = 0; i < 1400; ++i)
        game::updateCamera(state);
    ok = (state.cameraXQ4 == 0 && state.cameraYQ4 == 0 && hullVisible(state)) && ok;

    // Real AVR temporal trace: top speed keeps the fractional hull stable and
    // the terrain origin moves 0 or 1 px per tick, never backward.
    game::resetWorld(state);
    state.x = 500;
    state.y = 300;
    state.vxQ8 = game::kMaxVxBaseQ8;
    game::snapCamera(state);
    for (uint8_t i = 0; i < 60; ++i) {
        game::advanceAxis(state.x, state.xFrac, state.vxQ8);
        game::updateCamera(state);
    }
    const int16_t steadyHull = pilot_view::screenX(state, pilot_view::cameraForState(state));
    int16_t previousCamera = game::cameraPixelX(state);
    ok = (steadyHull == 31) && ok;
    for (uint8_t i = 0; i < 120; ++i) {
        game::advanceAxis(state.x, state.xFrac, state.vxQ8);
        game::updateCamera(state);
        const int16_t camera = game::cameraPixelX(state);
        ok = (camera == previousCamera || camera == static_cast<int16_t>(previousCamera + 1)) && ok;
        ok = (pilot_view::screenX(state, pilot_view::cameraForState(state)) == steadyHull) && ok;
        previousCamera = camera;
    }

    // Fractional 8-px page remainder stays fixed while the hull holds station.
    for (uint8_t remainder = 0; remainder < 8; ++remainder) {
        place(state, 500, static_cast<int16_t>(game::kCameraScreenY + 264 + remainder));
        for (uint8_t i = 0; i < 120; ++i) {
            game::updateCamera(state);
            ok = (static_cast<uint8_t>(pilot_view::cameraForState(state).subY & 7) == remainder) && ok;
        }
    }

    // Offscreen/negative projection and Q4 residual boundaries.
    state.x = 10;
    state.xFrac = 2048;
    state.cameraXQ4 = game::cameraPixelToQ4(100);
    ok = (pilot_view::screenX(state, pilot_view::cameraForState(state)) == -89) && ok;
    state.cameraXQ4 = game::cameraPixelToQ4(2047) + 8;
    ok = (game::cameraPixelX(state) == 2048 && game::cameraResidualXQ4(state) == -8) && ok;
    state.cameraXQ4 = game::kCameraMaxXQ4;
    ok = (game::cameraPixelX(state) == game::kCameraMaxX && game::cameraResidualXQ4(state) == 0) && ok;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
