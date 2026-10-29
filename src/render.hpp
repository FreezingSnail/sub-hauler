#pragma once

#include "cave.hpp"
#include "cave_render.hpp"
#include "campaign_view.hpp"
#include "common.hpp"
#include "fxdata.h"
#include "game.hpp"
#include "exploration_view.hpp"
#include "hazard_view.hpp"
#include "particles_view.hpp"
#include "pilot_view.hpp"
#include "world.hpp"
#include "world_fx.hpp"

// M2 pilot view: compose the visible 16x8 tile window and the HAULER at the
// camera derived from the pilot state.
//
// Terrain streams from a baked native image at the camera's exact pixel origin;
// page-aligned Y frames keep eight 128-byte bursts, fractional Y frames combine page
// rows through a small stack scratch. The 18x10 collision cache remains
// tile-based and refreshes only when the camera tile moves. All cart reads
// finish before the single FX::display(CLEAR_BUFFER) in the sketch.
namespace view {

inline void frame(const game::State &state, const cave::Camera &camera, world::TileCache &cache, world::FxTables &tables) {
    world::refreshCache(cache, tables, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
    cave::streamFrame(camera, tables, Arduboy2Base::sBuffer);
    campaign_view::drawRestoration(Arduboy2Base::sBuffer, state, camera);
    hazard_view::drawCurrents(Arduboy2Base::sBuffer, state, camera);
    exploration_view::draw(Arduboy2Base::sBuffer, state, camera);
    pilot_view::draw(Arduboy2Base::sBuffer, state, camera);
    particles_view::draw(Arduboy2Base::sBuffer, state, camera);
}

}   // namespace view
