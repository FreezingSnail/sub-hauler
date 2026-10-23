#pragma once

#include "game.hpp"
#include "world.hpp"

#if SH_FEATURE_CURRENTS
#include "currents.hpp"
#endif
#if SH_FEATURE_MINES
#include "hazards.hpp"
#endif

// World-aware pilot rules: the pure submarine from game.hpp, moved through the
// baked cave with axis-separated collision and an integer follow camera.
//
// This header is the rules-side device boundary. It never touches the OLED:
// Reader::tileByte may perform FX cart reads, so call stepWorld() from update
// only, never from inside an FX::display transaction. game.hpp stays pure.
namespace game {

constexpr int16_t kViewW = 128;
constexpr int16_t kViewH = 64;

// The browser keeps the boat left of centre and slightly above centre.
constexpr int16_t kCameraScreenX = 44;
constexpr int16_t kCameraScreenY = 32;

constexpr int16_t kCameraMaxX = static_cast<int16_t>(world::PIXELS_X) - kViewW;
constexpr int16_t kCameraMaxY = static_cast<int16_t>(world::PIXELS_Y) - kViewH;

// Integer shift lerp: one quarter of the error per tick, but never less than
// one pixel while the camera is off target. Both bounds are enforced here.
inline int16_t lerpCamera(int16_t current, int16_t target) {
    const int16_t delta = static_cast<int16_t>(target - current);
    if (delta == 0)
        return current;

    int16_t stepPx = static_cast<int16_t>(delta / 4);
    if (stepPx == 0)
        stepPx = static_cast<int16_t>(delta > 0 ? 1 : -1);
    return static_cast<int16_t>(current + stepPx);
}

inline int16_t cameraTargetX(const State &state) {
    return clampI16(static_cast<int16_t>(state.x - kCameraScreenX), 0, kCameraMaxX);
}

inline int16_t cameraTargetY(const State &state) {
    return clampI16(static_cast<int16_t>(state.y - kCameraScreenY), 0, kCameraMaxY);
}

// Update owns the camera. Rendering reads cameraX/cameraY without changing
// them, so display timing can never feed back into movement.
inline void updateCamera(State &state) {
    state.cameraX = lerpCamera(state.cameraX, cameraTargetX(state));
    state.cameraY = lerpCamera(state.cameraY, cameraTargetY(state));

    // Lerp cannot leave the valid range when it starts inside, but clamp here
    // so a caller that wrote camera fields directly still cannot show void.
    state.cameraX = clampI16(state.cameraX, 0, kCameraMaxX);
    state.cameraY = clampI16(state.cameraY, 0, kCameraMaxY);
}

inline void snapCamera(State &state) {
    state.cameraX = cameraTargetX(state);
    state.cameraY = cameraTargetY(state);
}

// Place the boat at the baked spawn and snap the camera. reset()/step() remain
// the pure physics pair used by the existing host tests.
inline void resetWorld(State &state) {
    reset(state);
    state.x = world::SPAWN_SUB_X;
    state.y = world::SPAWN_SUB_Y;
    snapCamera(state);
}

template <typename Reader> inline bool hullHits(Reader &reader, const world::TileCache *cache, int16_t x, int16_t y) {
    const int16_t left = static_cast<int16_t>(x - kHullHalfW);
    const int16_t top = static_cast<int16_t>(y - kHullHalfH);
    if (cache)
        return world::rectHitsCached(*cache, reader, left, top, kHullW, kHullH);
    return world::rectHits(reader, left, top, kHullW, kHullH);
}

// Advance one axis toward its integrated position, stopping at the last whole
// pixel that keeps the hull clear. Walking pixel by pixel means a too-large
// velocity cannot tunnel through a wall, even though expected top speed only
// moves half a pixel per tick. A blocked axis zeroes its velocity and Q12
// remainder so pressure does not accumulate against the wall, and records the
// impact for later damage logic.
template <typename Reader>
inline void moveAxis(State &state, Reader &reader, const world::TileCache *cache, bool horizontal) {
    int16_t &position = horizontal ? state.x : state.y;
    int16_t &frac = horizontal ? state.xFrac : state.yFrac;
    int16_t &velocity = horizontal ? state.vxQ8 : state.vyQ8;

    const int16_t from = position;
    int16_t to = position;
    int16_t toFrac = frac;
    advanceAxis(to, toFrac, velocity);

    if (to == from) {
        frac = toFrac;
        // Pressing into a wall may not produce a whole-pixel move every tick,
        // but the blocked axis must still stop instead of accumulating speed
        // and subpixel pressure.
        if (velocity != 0) {
            const int16_t direction = static_cast<int16_t>(velocity > 0 ? 1 : -1);
            const int16_t probe = static_cast<int16_t>(position + direction);
            const int16_t px = horizontal ? probe : state.x;
            const int16_t py = horizontal ? state.y : probe;
            if (hullHits(reader, cache, px, py)) {
                state.impactSpeedQ8 = static_cast<int16_t>(velocity < 0 ? -velocity : velocity);
                state.impactAxis = horizontal ? 1 : 2;
                if (state.impacts < 255)
                    ++state.impacts;
                frac = 0;
                velocity = 0;
            }
        }
        return;
    }

    const int16_t direction = static_cast<int16_t>(to > from ? 1 : -1);
    int16_t lastValid = from;
    int16_t probe = from;
    bool blocked = false;

    while (probe != to) {
        probe = static_cast<int16_t>(probe + direction);
        const int16_t px = horizontal ? probe : state.x;
        const int16_t py = horizontal ? state.y : probe;
        if (hullHits(reader, cache, px, py)) {
            blocked = true;
            break;
        }
        lastValid = probe;
    }

    if (!blocked) {
        position = to;
        frac = toFrac;
        return;
    }

    state.impactSpeedQ8 = static_cast<int16_t>(velocity < 0 ? -velocity : velocity);
    state.impactAxis = horizontal ? 1 : 2;
    if (state.impacts < 255)
        ++state.impacts;

    position = lastValid;
    frac = 0;
    velocity = 0;
}

// One fixed 1/60 s step against the world. Physics is byte-identical to
// step(); only the position advance changes, because it now resolves against
// the cave. X moves first, then Y, matching the browser order.
template <typename Reader>
inline void stepWorld(State &state, const Input &input, Reader &reader, const world::TileCache *cache = nullptr) {
#if SH_FEATURE_MINES
    // A mine spark is a one-frame cue; clear it before the next step so it can
    // never be mistaken for a wall impact.
    hazards::clearImpact(state);
#endif
#if SH_FEATURE_STATIONS
    const bool freeFlight = state.docked < 0;
#else
    const bool freeFlight = true;
#endif
    stepPhysics(state, input);
#if SH_FEATURE_CURRENTS
    // Currents add acceleration before collision resolves, so the same
    // screen-step cannot push the hull through rock or out of the world.
    if (freeFlight)
        currents::apply(state);
#endif
    moveAxis(state, reader, cache, true);
    moveAxis(state, reader, cache, false);
#if SH_FEATURE_MINES
    // Free-flight hazard pass after movement. Docked/chart/cargo screens never
    // reach stepWorld, so they cannot consume a mine.
    if (freeFlight)
        hazards::collide(state);
#endif
    updateCamera(state);
    ++state.ticks;
#if SH_FEATURE_SONAR
    // One flight tick, one cooldown tick. step() owns the pure-physics path;
    // stepFlight calls this function exactly once per frame.
    if (state.sonarCooldown > 0)
        --state.sonarCooldown;
#endif
#if SH_FEATURE_JOBS
    // Flight tick for contract deadlines (docked frames never step the world).
    ++state.jobClock;
#endif
}

}   // namespace game
