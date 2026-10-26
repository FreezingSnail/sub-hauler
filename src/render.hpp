#pragma once

#include "cave.hpp"
#include "cave_render.hpp"
#include "common.hpp"
#if SH_FEATURE_HULL_DAMAGE || SH_FEATURE_PARTICLES
#include "damage_view.hpp"
#endif
#include "fxdata.h"
#include "game.hpp"
#if SH_FEATURE_EXPLORATION
#include "exploration_view.hpp"
#endif
#if SH_FEATURE_MINES || SH_FEATURE_CURRENTS
#include "hazard_view.hpp"
#endif
#include "pilot_view.hpp"
#if SH_FEATURE_SONAR
#include "sonar_view.hpp"
#endif
#include "world.hpp"
#include "world_fx.hpp"

// M2 pilot view: compose the visible 16x8 tile window and the HAULER at the
// camera derived from the pilot state.
//
// Terrain streams from a baked native image in eight 128-byte bursts directly
// into the existing framebuffer. The 18x10 collision cache is still refreshed
// when the camera moves (one transaction per packed byte). All cart reads
// finish before the single FX::display(CLEAR_BUFFER) in the sketch.
namespace view {

inline void frame(const game::State &state, const cave::Camera &camera, world::TileCache &cache, world::FxTables &tables) {
    world::refreshCache(cache, tables, cave::cacheOriginTileX(camera), cave::cacheOriginTileY(camera));
    cave::streamFrame(camera, tables, Arduboy2Base::sBuffer);
#if SH_FEATURE_SONAR
    // Ring first: the filled HAULER silhouette stays solid and readable.
    sonar_view::draw(Arduboy2Base::sBuffer, state, camera);
#endif
#if SH_FEATURE_CURRENTS
    hazard_view::drawCurrents(Arduboy2Base::sBuffer, state, camera);
#endif
#if SH_FEATURE_MINES
    hazard_view::drawMines(Arduboy2Base::sBuffer, state, camera);
#endif
#if SH_FEATURE_EXPLORATION
    exploration_view::draw(Arduboy2Base::sBuffer, state, camera);
#endif
    pilot_view::draw(Arduboy2Base::sBuffer, state, camera);
#if SH_FEATURE_PARTICLES
    damage_view::drawParticles(Arduboy2Base::sBuffer, state, camera);
#endif
#if SH_FEATURE_HULL_DAMAGE
    damage_view::drawFlash(Arduboy2Base::sBuffer, state, camera);
#endif
#if SH_FEATURE_MINES
    hazard_view::drawImpact(Arduboy2Base::sBuffer, state, camera);
#endif
}

}   // namespace view
