#pragma once

#include <stdint.h>

#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"

#include "generated/pilot_sprites.hpp"

#if defined(__AVR__)
#include "common.hpp"
#else
#include "generated/pilot_host.hpp"
#endif

// Pilot view: pure screen mapping and the baked HAULER silhouette.
//
// The sub is drawn at world position minus the camera's whole-pixel origin, so
// it slides with the terrain at single-pixel granularity. Collision semantics
// still use game::State.x/y and the 26x8 hull in pilot.hpp. The art body is
// centred on the hull, and its one-pixel black outline bleeds outside it. Art
// may be taller than the hull; the body is bottom-aligned so a mast extends
// above the collision box.
//
// The device reads fixed plus-mask art from FX (right/left x unloaded/loaded).
// The host uses the same normalized pixels from generated/pilot_host.hpp, so
// permanent pixel tests exercise the authored art. The retired
// Minnow/Leviathan sheets remain archival and are never referenced.
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

static_assert(HAULER_WIDTH >= game::kHullW, "pilot art must cover the collision hull");
static_assert((HAULER_WIDTH - game::kHullW) % 2 == 0, "pilot art body must be centred on the hull");
static_assert(HAULER_HEIGHT >= game::kHullH + 2, "pilot art must hold the hull plus outline rows");
static_assert(HAULER_FRAMES == 4, "pilot sheet must hold right/left x empty/loaded frames");

inline bool visible(int16_t left, int16_t top) {
    return left < cave::VIEW_WIDTH && top < cave::VIEW_HEIGHT && static_cast<int16_t>(left + game::kHullW) > 0 && static_cast<int16_t>(top + game::kHullH) > 0;
}

inline bool visibleArt(int16_t left, int16_t top) {
    return left < cave::VIEW_WIDTH && top < cave::VIEW_HEIGHT && static_cast<int16_t>(left + HAULER_WIDTH) > 0 && static_cast<int16_t>(top + HAULER_HEIGHT) > 0;
}

// The one-pixel outline bleeds outside the 26x8 collision hull on every side.
// Align the body to the hull, not the outline, so the sub keeps its old screen
// position while the outline clears surrounding terrain.
constexpr int16_t kArtSideBleed = (HAULER_WIDTH - game::kHullW) / 2;
constexpr int16_t kArtBottomBleed = 1;
constexpr int16_t kArtAboveHull = HAULER_HEIGHT - game::kHullH - kArtBottomBleed;

#if !defined(__AVR__)
inline void drawHauler(uint8_t *buffer, int16_t left, int16_t top, uint8_t frame) {
    for (uint8_t page = 0; page < HAULER_ART_PAGES; ++page) {
        for (uint8_t x = 0; x < HAULER_WIDTH; ++x) {
            const uint8_t data = haulerHostData[frame][page][x];
            const uint8_t mask = haulerHostMask[frame][page][x];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const uint8_t y = static_cast<uint8_t>(page * 8 + bit);
                if (y >= HAULER_HEIGHT)
                    break;
                if ((mask >> bit) & 1u) {
                    if ((data >> bit) & 1u)
                        cave::setPixel(buffer, static_cast<int16_t>(left + x), static_cast<int16_t>(top + y));
                    else
                        cave::clearPixel(buffer, static_cast<int16_t>(left + x), static_cast<int16_t>(top + y));
                }
            }
        }
    }
}
#endif   // !__AVR__

// Draw the HAULER body at the art origin. Title and pilot share this exact art,
// so the silhouette is identical wherever it appears.
inline void drawAt(uint8_t *buffer, int16_t left, int16_t top, bool mirror, bool loaded) {
    const uint8_t frame = static_cast<uint8_t>((mirror ? 2 : 0) + (loaded ? 1 : 0));
#if defined(__AVR__)
    (void)buffer;
    Blit::draw(left, top, HAULER_WIDTH, HAULER_HEIGHT, hauler + 2, frame, Blit::PLUSMASK);
#else
    drawHauler(buffer, left, top, frame);
#endif
}

// Draw the HAULER with its 26x8 collision hull on the state position. The state
// hull byte is a frozen save field, not a selector.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t left = static_cast<int16_t>(screenX(state, camera) - game::kHullHalfW - kArtSideBleed);
    const int16_t top = static_cast<int16_t>(screenY(state, camera) - game::kHullHalfH - kArtAboveHull);
    if (!visibleArt(left, top))
        return;

    drawAt(buffer, left, top, state.facingLeft, state.cargo > 0);
}

}   // namespace pilot_view
