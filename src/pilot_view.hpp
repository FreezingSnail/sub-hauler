#pragma once

#include <stdint.h>

#include "cave.hpp"
#include "cave_render.hpp"
#include "game.hpp"
#include "pilot.hpp"

// HAULER pilot view: pure screen mapping and the 26x8 external silhouette.
//
// The cave renderer is tile-aligned, so the camera is derived by flooring the
// pilot's whole-pixel camera offset to its containing tile. The sub is then
// drawn at world position minus the visible tile origin; collision semantics
// still use game::State.x/y and the 26x8 hull in pilot.hpp.
//
// No Arduino headers: the host tests draw into a plain page-major buffer and
// inspect exact pixels. Device code passes Arduboy2Base::sBuffer.
namespace pilot_view {

inline int16_t screenX(const game::State &state, const cave::Camera &camera) {
    return static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS);
}

inline int16_t screenY(const game::State &state, const cave::Camera &camera) {
    return static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS);
}

// The cave renderer only has tile granularity; floor the integer camera pixel
// to the tile that owns it. cameraAtTile applies the same display clamp as the
// cave window, so both view layers always agree.
inline cave::Camera cameraForState(const game::State &state) {
    return cave::cameraAtTile(world::tileFromPixel(state.cameraX), world::tileFromPixel(state.cameraY));
}

inline bool visible(int16_t left, int16_t top) {
    return left < cave::VIEW_WIDTH && top < cave::VIEW_HEIGHT && static_cast<int16_t>(left + game::kHullW) > 0 && static_cast<int16_t>(top + game::kHullH) > 0;
}

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

// Draw the flat-deck HAULER silhouette centred on the state position. The
// 26x8 body matches the collision hull exactly: a narrow deckhouse over a
// rounded flat deck, two cargo doors, a stern thruster notch, and a bow mark.
// Cargo is internal mass: it only shows as the white stripe inside each door.
// Nothing is drawn outside the collision footprint.
inline void draw(uint8_t *buffer, const game::State &state, const cave::Camera &camera) {
    const int16_t left = static_cast<int16_t>(screenX(state, camera) - game::kHullHalfW);
    const int16_t top = static_cast<int16_t>(screenY(state, camera) - game::kHullHalfH);
    if (!visible(left, top))
        return;

    const bool mirror = state.facingLeft;
    const bool loaded = state.cargo > 0;

    // Rounded flat-deck body. Rows 0-1 are the deckhouse; rows 2-7 are the
    // hull, with the deck widening to the full 26 px at rows 3-4.
    fillLocal(buffer, left, top, mirror, 10, 0, 12, 1, true);
    fillLocal(buffer, left, top, mirror, 10, 1, 12, 1, true);
    fillLocal(buffer, left, top, mirror, 1, 2, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 0, 3, 26, 1, true);
    fillLocal(buffer, left, top, mirror, 0, 4, 26, 1, true);
    fillLocal(buffer, left, top, mirror, 1, 5, 24, 1, true);
    fillLocal(buffer, left, top, mirror, 2, 6, 22, 1, true);
    fillLocal(buffer, left, top, mirror, 3, 7, 20, 1, true);

    // Deck/trim line just below the deckhouse.
    fillLocal(buffer, left, top, mirror, 3, 3, 20, 1, false);

    // Dark cabin windows.
    fillLocal(buffer, left, top, mirror, 12, 0, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 15, 0, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 18, 0, 1, 2, false);

    // Two framed cargo doors. The white centre stripe is the internal load.
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

    // Stern thruster notch and bow mark: mirrored with the hull, they make the
    // direction of travel read at a glance.
    fillLocal(buffer, left, top, mirror, 1, 4, 1, 2, false);
    fillLocal(buffer, left, top, mirror, 23, 5, 2, 1, false);
}

}   // namespace pilot_view
