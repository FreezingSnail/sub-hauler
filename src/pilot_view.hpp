#pragma once

#include <stdint.h>

#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"
#include "roster.hpp"

#if defined(__AVR__)
#include "common.hpp"
#include "generated/pilot_sprites.hpp"
#endif

// Pilot view: pure screen mapping and the 26x8 baked silhouettes.
//
// The sub is drawn at world position minus the camera's whole-pixel origin, so
// it slides with the terrain at single-pixel granularity. Collision semantics
// still use game::State.x/y and the 26x8 hull in pilot.hpp.
//
// Every hull class draws inside the same 26x8 footprint, so collision, damage
// brackets, and camera framing stay class-independent. The device reads fixed
// plus-mask art from FX (right/left x unloaded/loaded). The host keeps the
// equivalent procedural shapes so the permanent pixel tests stay pure C++.
// With MULTI_HULL=0 only the HAULER sprite is referenced by device code.
//
// No Arduino headers on the host path: tests draw into a plain page-major
// buffer and inspect exact pixels. Device code passes Arduboy2Base::sBuffer.
namespace pilot_view {

// Project the fractional boat-minus-camera position, then round once. Using the
// integer boat pixel and the rounded camera independently would alternate a
// steady hull between adjacent screen pixels when their fractional parts move
// together. Terrain and integer-world overlays still use the same rounded
// camera pixel origin. Integer pixels can be added after rounding the small
// fractional difference, avoiding a 32-bit shift on the AVR.
inline int16_t screenX(const game::State &state, const cave::Camera &camera) {
    const int16_t roundedFraction = (static_cast<int16_t>(state.xFrac >> 8) - camera.fracX + 8) >> 4;
    return static_cast<int16_t>(state.x - cave::pixelOriginX(camera) + roundedFraction);
}

inline int16_t screenY(const game::State &state, const cave::Camera &camera) {
    const int16_t roundedFraction = (static_cast<int16_t>(state.yFrac >> 8) - camera.fracY + 8) >> 4;
    return static_cast<int16_t>(state.y - cave::pixelOriginY(camera) + roundedFraction);
}

// Round the Q4 follow origin once for every layer. The transient Camera also
// carries the signed residual so the hull can be projected before rounding;
// cameraAtPixel still applies the display clamp.
inline cave::Camera cameraForState(const game::State &state) {
    cave::Camera camera = cave::cameraAtPixel(game::cameraPixelX(state), game::cameraPixelY(state));
    camera.fracX = game::cameraResidualXQ4(state);
    camera.fracY = game::cameraResidualYQ4(state);
    return camera;
}

inline bool visible(int16_t left, int16_t top) {
    return left < cave::VIEW_WIDTH && top < cave::VIEW_HEIGHT && static_cast<int16_t>(left + game::kHullW) > 0 && static_cast<int16_t>(top + game::kHullH) > 0;
}

#if !defined(__AVR__)
// Local-to-screen fill. Local x is mirrored around the hull centre when the
// boat faces left, so the deckhouse, doors, and facing marks flip as one.
inline void fillLocal(uint8_t *buffer, int16_t left, int16_t top, bool mirror, int16_t x, int16_t y, int16_t width, int16_t height, bool color) {
    for (int16_t dy = 0; dy < height; ++dy) {
        for (int16_t dx = 0; dx < width; ++dx) {
            const int16_t localX = static_cast<int16_t>(x + dx);
            const int16_t screenLocalX = mirror ? static_cast<int16_t>(game::kHullW - 1 - localX) : localX;
            const int16_t px = static_cast<int16_t>(left + screenLocalX);
            const int16_t py = static_cast<int16_t>(top + y + dy);
            if (color)
                cave::setPixel(buffer, px, py);
            else
                cave::clearPixel(buffer, px, py);
        }
    }
}

// HAULER: flat-deck freighter. The host reference shape mirrors the baked FX
// sheet one-for-one: rows 0-1 deckhouse, a rounded flat deck, two cargo doors,
// a stern thruster notch, and a bow mark.
inline void drawHauler(uint8_t *buffer, int16_t left, int16_t top, bool mirror, bool loaded) {
    fillLocal(buffer, left, top, mirror, 10, 0, 12, 2, true);
    fillLocal(buffer, left, top, mirror, 1, 2, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 0, 3, 26, 2, true);
    fillLocal(buffer, left, top, mirror, 1, 5, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 2, 6, 22, 1, true);
    fillLocal(buffer, left, top, mirror, 3, 7, 20, 1, true);

    fillLocal(buffer, left, top, mirror, 3, 3, 20, 1, false);
    fillLocal(buffer, left, top, mirror, 12, 0, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 15, 0, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 18, 0, 1, 2, false);

    fillLocal(buffer, left, top, mirror, 3, 4, 6, 3, false);
    fillLocal(buffer, left, top, mirror, 4, 4, 4, 1, true);
    fillLocal(buffer, left, top, mirror, 4, 6, 4, 1, true);
    if (loaded)
        fillLocal(buffer, left, top, mirror, 5, 4, 1, 3, true);

    fillLocal(buffer, left, top, mirror, 11, 4, 6, 3, false);
    fillLocal(buffer, left, top, mirror, 12, 4, 4, 1, true);
    fillLocal(buffer, left, top, mirror, 12, 6, 4, 1, true);
    if (loaded)
        fillLocal(buffer, left, top, mirror, 13, 4, 1, 3, true);

    fillLocal(buffer, left, top, mirror, 1, 4, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 23, 5, 2, 1, false);
}

#if SH_FEATURE_MULTI_HULL
// MINNOW: 24x7 rounded shuttle with one door and a short offset sail.
inline void drawMinnow(uint8_t *buffer, int16_t left, int16_t top, bool mirror, bool loaded) {
    fillLocal(buffer, left, top, mirror, 15, 1, 6, 1, true);
    fillLocal(buffer, left, top, mirror, 8, 2, 11, 1, true);
    fillLocal(buffer, left, top, mirror, 4, 3, 18, 2, true);
    fillLocal(buffer, left, top, mirror, 1, 5, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 2, 6, 22, 1, true);
    fillLocal(buffer, left, top, mirror, 6, 7, 14, 1, true);

    fillLocal(buffer, left, top, mirror, 17, 1, 1, 1, false);
    fillLocal(buffer, left, top, mirror, 8, 4, 6, 3, false);
    if (loaded)
        fillLocal(buffer, left, top, mirror, 10, 4, 1, 3, true);

    fillLocal(buffer, left, top, mirror, 2, 5, 1, 2, false);
}

// LEVIATHAN: 26x8 fortress with three doors and a stern notch.
inline void drawLeviathan(uint8_t *buffer, int16_t left, int16_t top, bool mirror, bool loaded) {
    fillLocal(buffer, left, top, mirror, 9, 0, 14, 2, true);
    fillLocal(buffer, left, top, mirror, 3, 2, 22, 1, true);
    fillLocal(buffer, left, top, mirror, 0, 3, 26, 3, true);
    fillLocal(buffer, left, top, mirror, 1, 6, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 2, 7, 22, 1, true);

    fillLocal(buffer, left, top, mirror, 3, 3, 20, 1, false);
    fillLocal(buffer, left, top, mirror, 12, 0, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 18, 0, 1, 2, false);

    fillLocal(buffer, left, top, mirror, 2, 4, 5, 3, false);
    fillLocal(buffer, left, top, mirror, 18, 4, 5, 3, false);
    if (loaded) {
        fillLocal(buffer, left, top, mirror, 4, 4, 1, 3, true);
        fillLocal(buffer, left, top, mirror, 20, 4, 1, 3, true);
    }

    fillLocal(buffer, left, top, mirror, 0, 4, 1, 2, false);
}
#endif
#endif   // !__AVR__

// Draw one hull body at a footprint origin. Title and pilot share this exact
// art, so a class silhouette is identical wherever it appears. Nothing is
// drawn outside local x 0..25, y 0..7.
inline void drawAt(uint8_t *buffer, int16_t left, int16_t top, bool mirror, bool loaded, roster::HullClass hull) {
#if defined(__AVR__)
    (void)buffer;
    const uint16_t frame = static_cast<uint16_t>((mirror ? 2 : 0) + (loaded ? 1 : 0));
#if SH_FEATURE_MULTI_HULL
    // if-chain, not switch: a three-case switch emits a RAM jump table.
    if (hull == roster::kMinnow) {
        Blit::draw(left, top, MINNOW_WIDTH, MINNOW_HEIGHT, minnow + 2, frame, Blit::PLUSMASK);
        return;
    }
    if (hull == roster::kLeviathan) {
        Blit::draw(left, top, LEVIATHAN_WIDTH, LEVIATHAN_HEIGHT, leviathan + 2, frame, Blit::PLUSMASK);
        return;
    }
#else
    (void)hull;
#endif
    Blit::draw(left, top, HAULER_WIDTH, HAULER_HEIGHT, hauler + 2, frame, Blit::PLUSMASK);
#else
#if SH_FEATURE_MULTI_HULL
    if (hull == roster::kMinnow) {
        drawMinnow(buffer, left, top, mirror, loaded);
        return;
    }
    if (hull == roster::kLeviathan) {
        drawLeviathan(buffer, left, top, mirror, loaded);
        return;
    }
#else
    (void)hull;
#endif
    drawHauler(buffer, left, top, mirror, loaded);
#endif
}

// Draw the selected class centred on the state position. The footprint is the
// 26x8 collision hull for every class; class art only changes the pixels inside
// it.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t left = static_cast<int16_t>(screenX(state, camera) - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(screenY(state, camera) - game::kHullHalfH);
    if (!visible(left, top))
        return;

#if SH_FEATURE_MULTI_HULL
    const roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(state.hullClass));
#else
    const roster::HullClass hull = roster::kHauler;
#endif
    drawAt(buffer, left, top, state.facingLeft, state.cargo > 0, hull);
}

}   // namespace pilot_view
