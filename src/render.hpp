#pragma once

#include "cave.hpp"
#include "cave_render.hpp"
#include "common.hpp"
#include "fxdata.h"
#include "game.hpp"
#include "pilot_view.hpp"
#include "world.hpp"
#include "world_fx.hpp"

// M2 pilot view: compose the visible 16x8 tile window and the HAULER at the
// camera derived from the pilot state.
//
// The world grid is never loaded into SRAM. world::refreshCache() reads the
// 18x10 tile window through world::FxTables (one FX transaction per tile), and
// only when the camera tile changes. All cart reads happen here, in render,
// before the single FX::display(CLEAR_BUFFER) transaction in the sketch.
namespace view {

inline void frame(const game::State &state, const cave::Camera &camera, world::TileCache &cache, world::FxTables &tables) {
    world::refreshCache(cache, tables, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
    cave::composeFrame(camera, cache, Arduboy2Base::sBuffer);
    pilot_view::draw(Arduboy2Base::sBuffer, state, camera);
}

}   // namespace view
