#pragma once

#include "game.hpp"
#include "world.hpp"
#include "exploration.hpp"

#include "currents.hpp"
#include "particles.hpp"

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

// Directional look-ahead: shift the target half a pixel per px/s of Q8.8
// velocity. Base top speed (30 px/s) gives 15 px right and max vertical
// (20 px/s) gives 10 px down; reverse velocity mirrors that exactly. In Q4 the
// same lead is velocityQ8 / 32, so 30 px/s = 240 Q4 = 15 px. The look-ahead is
// stateless, so a stop immediately recentres the target while the camera itself
// still eases back, and a reversal flips the target without a camera snap.
constexpr uint8_t kCameraLookAheadShiftQ4 = 5;
constexpr uint16_t kCameraLookAheadMaskQ4 = (1u << kCameraLookAheadShiftQ4) - 1u;
static_assert((-1 >> kCameraLookAheadShiftQ4) == -1, "signed right shift must floor");

constexpr int16_t kCameraMaxX = static_cast<int16_t>(world::PIXELS_X) - kViewW;
constexpr int16_t kCameraMaxY = static_cast<int16_t>(world::PIXELS_Y) - kViewH;

constexpr uint8_t kCameraQ4Shift = 4;
constexpr uint16_t kCameraQ4One = 1u << kCameraQ4Shift;
constexpr uint32_t kCameraMaxXQ4Wide = static_cast<uint32_t>(kCameraMaxX) * kCameraQ4One;
constexpr uint32_t kCameraMaxYQ4Wide = static_cast<uint32_t>(kCameraMaxY) * kCameraQ4One;
static_assert(kCameraMaxXQ4Wide <= 0xFFFFu, "Q4 camera x fits the existing 16-bit fields");
static_assert(kCameraMaxYQ4Wide <= 0xFFFFu, "Q4 camera y fits the existing 16-bit fields");
constexpr uint16_t kCameraMaxXQ4 = static_cast<uint16_t>(kCameraMaxXQ4Wide);
constexpr uint16_t kCameraMaxYQ4 = static_cast<uint16_t>(kCameraMaxYQ4Wide);
constexpr uint16_t kCameraScreenXQ4 = static_cast<uint16_t>(kCameraScreenX) << kCameraQ4Shift;
constexpr uint16_t kCameraScreenYQ4 = static_cast<uint16_t>(kCameraScreenY) << kCameraQ4Shift;

// Correction bounds. Normal flight tops out at 0.5 px/frame, so 2 px/frame is
// four times the tracking requirement and still bounds a 15 px look-ahead drop
// (stop or collision) to a 2 px terrain step instead of the old 3.75 px jolt.
// Developer direct traversal moves 6 px/frame, so only that explicit path may
// use the larger bound; gameplay camera state and saves are unchanged.
constexpr uint16_t kCameraMaxStepQ4 = 2u << kCameraQ4Shift;
constexpr uint16_t kDevCameraMaxStepQ4 = 6u << kCameraQ4Shift;

// Developer fast traverse: position advance only, so the fixed physics tick,
// air, currents, docking threshold and voyage clock stay at production rates.
constexpr uint8_t kDevSpeedScale = 3;

static_assert(kCameraMaxX >= 0 && kCameraMaxY >= 0, "world must be at least one view");
static_assert(kCameraScreenX >= 0 && kCameraScreenX < static_cast<int16_t>(world::PIXELS_X), "camera target x can clamp");
static_assert(kCameraScreenY >= 0 && kCameraScreenY < static_cast<int16_t>(world::PIXELS_Y), "camera target y can clamp");
// Full look-ahead must keep the whole 26x8 hull inside the 128x64 view.
static_assert(kCameraScreenXQ4 - (kMaxVxBaseQ8 >> kCameraLookAheadShiftQ4) >= 0, "left look-ahead keeps the hull on screen");
static_assert(kCameraScreenXQ4 + (kMaxVxBaseQ8 >> kCameraLookAheadShiftQ4) + (kHullW << kCameraQ4Shift) <= (kViewW << kCameraQ4Shift), "right look-ahead keeps the hull on screen");
static_assert(kCameraScreenYQ4 - (kMaxVyQ8 >> kCameraLookAheadShiftQ4) >= 0, "top look-ahead keeps the hull on screen");
static_assert(kCameraScreenYQ4 + (kMaxVyQ8 >> kCameraLookAheadShiftQ4) + (kHullH << kCameraQ4Shift) <= (kViewH << kCameraQ4Shift), "bottom look-ahead keeps the hull on screen");

// Arithmetic shift floors, which would give -1..-31 a one-Q4 lead while
// +1..+31 have none. Correct the negative remainder so both signs truncate
// toward zero, keeping framing symmetric while the boat slows or reverses.
inline int16_t cameraLookAheadQ4(int16_t velocityQ8) {
    const int16_t lead = static_cast<int16_t>(velocityQ8 >> kCameraLookAheadShiftQ4);
    if (velocityQ8 < 0 && (velocityQ8 & kCameraLookAheadMaskQ4) != 0)
        return static_cast<int16_t>(lead + 1);
    return lead;
}

// Exact Q4 world position from the integer world pixel plus its Q12 remainder.
// xFrac is always 0..4095, so the top four Q12 bits are the Q4 fraction.
inline int32_t cameraPositionXQ4(const State &state) {
    return static_cast<int32_t>(state.x) * kCameraQ4One + (state.xFrac >> 8);
}

inline int32_t cameraPositionYQ4(const State &state) {
    return static_cast<int32_t>(state.y) * kCameraQ4One + (state.yFrac >> 8);
}

// The target adds velocity look-ahead before clamping, so the camera can lead a
// moving boat but never leaves the baked world. Zero velocity reduces both
// targets to the historical centred framing. The clamp is applied in signed
// 32-bit space before the result is stored as unsigned Q4.
inline uint16_t cameraTargetXQ4(const State &state) {
    const int32_t target = cameraPositionXQ4(state) + cameraLookAheadQ4(state.vxQ8) - kCameraScreenXQ4;
    if (target < 0)
        return 0;
    if (target > kCameraMaxXQ4)
        return kCameraMaxXQ4;
    return static_cast<uint16_t>(target);
}

inline uint16_t cameraTargetYQ4(const State &state) {
    const int32_t target = cameraPositionYQ4(state) + cameraLookAheadQ4(state.vyQ8) - kCameraScreenYQ4;
    if (target < 0)
        return 0;
    if (target > kCameraMaxYQ4)
        return kCameraMaxYQ4;
    return static_cast<uint16_t>(target);
}

// Quarter-step easing in Q4. A zero integer step uses one Q4 unit so the camera
// settles instead of sticking a few sixteenths short. maxStepQ4 bounds a large
// target jump without quantizing normal fractional motion to whole pixels.
inline uint16_t lerpCameraQ4(uint16_t current, uint16_t target, uint16_t maxStepQ4) {
    const int32_t delta = static_cast<int32_t>(target) - current;
    if (delta == 0)
        return current;
    int32_t step = delta / 4;
    if (step == 0)
        step = delta > 0 ? 1 : -1;
    if (step > static_cast<int32_t>(maxStepQ4))
        step = maxStepQ4;
    else if (step < -static_cast<int32_t>(maxStepQ4))
        step = -static_cast<int32_t>(maxStepQ4);
    return static_cast<uint16_t>(static_cast<int32_t>(current) + step);
}

// Rounded whole-pixel origin used by every renderer layer. Rounding once here
// keeps terrain and integer-world overlays on the same shared camera origin.
inline int16_t cameraPixelXQ4(uint16_t q4) {
    return static_cast<int16_t>((q4 + (1u << (kCameraQ4Shift - 1))) >> kCameraQ4Shift);
}

inline int16_t cameraPixelYQ4(uint16_t q4) {
    return static_cast<int16_t>((q4 + (1u << (kCameraQ4Shift - 1))) >> kCameraQ4Shift);
}

constexpr uint16_t cameraPixelToQ4(int16_t pixel) {
    return static_cast<uint16_t>(pixel) << kCameraQ4Shift;
}

inline int16_t cameraPixelX(const State &state) {
    return cameraPixelXQ4(state.cameraXQ4);
}

inline int16_t cameraPixelY(const State &state) {
    return cameraPixelYQ4(state.cameraYQ4);
}

// Signed residual of the exact Q4 origin from the rounded pixel, in Q4 units.
// The transient Camera carries it so the hull can be projected from fractional
// boat-minus-camera before the final pixel round.
inline int8_t cameraResidualXQ4(const State &state) {
    return static_cast<int8_t>(static_cast<int32_t>(state.cameraXQ4) - (static_cast<int32_t>(cameraPixelX(state)) * kCameraQ4One));
}

inline int8_t cameraResidualYQ4(const State &state) {
    return static_cast<int8_t>(static_cast<int32_t>(state.cameraYQ4) - (static_cast<int32_t>(cameraPixelY(state)) * kCameraQ4One));
}

// Update owns the camera. Rendering reads the Q4 fields without changing them,
// so display timing can never feed back into movement.
//
// The target moves with fractional position and velocity. Forward travel leads
// the hull, a reversal swaps the lead, and stopping decays back to centred
// framing. Quarter-step easing retains sub-pixel motion; the correction bound
// only clips large jumps. Berth snaps are the deliberate exception: spawn,
// Continue, rescue and dock zero velocity and call snapCamera() for an exact
// authored view.
inline void updateCamera(State &state, uint16_t maxStepQ4) {
    state.cameraXQ4 = lerpCameraQ4(state.cameraXQ4, cameraTargetXQ4(state), maxStepQ4);
    state.cameraYQ4 = lerpCameraQ4(state.cameraYQ4, cameraTargetYQ4(state), maxStepQ4);
}

inline void updateCamera(State &state) {
    updateCamera(state, kCameraMaxStepQ4);
}

inline void snapCamera(State &state) {
    state.cameraXQ4 = cameraTargetXQ4(state);
    state.cameraYQ4 = cameraTargetYQ4(state);
}

// Place the boat at the baked spawn and snap the camera. reset()/step() remain
// the pure physics pair used by the existing host tests.
inline void resetWorld(State &state) {
    reset(state);
    state.x = world::SPAWN_SUB_X;
    state.y = world::SPAWN_SUB_Y;
    snapCamera(state);
}

template <typename Reader> inline bool hullHits(Reader &reader, const world::TileCache *cache, int16_t x, int16_t y, State *state = nullptr) {
    if (state) {
        const uint8_t id = exploration::blockingGate(*state, x, y);
        if (id != exploration::kNone) {
            state->blockedGate = id;
            state->impactAxis = 4;
            return true;
        }
    }
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
// impact for presentation. `collide=false` is the developer map-inspection path:
// it moves the full integrated step without reading terrain or gates. Returns
// true only when this call recorded a blocked impact, so debris stays
// event-scoped even though the impact fields deliberately latch the last
// contact for presentation. `speedScale` repeats the same fixed-step advance
// without changing stored velocity or physics; only the developer image passes
// more than one.
template <typename Reader> inline bool moveAxis(State &state, Reader &reader, const world::TileCache *cache, bool horizontal, bool collide = true, uint8_t speedScale = 1) {
    int16_t &position = horizontal ? state.x : state.y;
    int16_t &frac = horizontal ? state.xFrac : state.yFrac;
    int16_t &velocity = horizontal ? state.vxQ8 : state.vyQ8;

    // Distinguish equipment barriers from rock impacts on this axis.
    if (state.impactAxis == 4)
        state.impactAxis = 0;
    const int16_t from = position;
    int16_t to = position;
    int16_t toFrac = frac;
    for (uint8_t step = 0; step < speedScale; ++step)
        advanceAxis(to, toFrac, velocity);

    if (!collide) {
        position = to;
        frac = toFrac;
        return false;
    }

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
            if (hullHits(reader, cache, px, py, &state)) {
                state.impactSpeedQ8 = static_cast<int16_t>(velocity < 0 ? -velocity : velocity);
                if (state.impactAxis != 4)
                    state.impactAxis = horizontal ? 1 : 2;
                frac = 0;
                velocity = 0;
                return true;
            }
        }
        return false;
    }

    const int16_t direction = static_cast<int16_t>(to > from ? 1 : -1);
    int16_t lastValid = from;
    int16_t probe = from;
    bool blocked = false;

    while (probe != to) {
        probe = static_cast<int16_t>(probe + direction);
        const int16_t px = horizontal ? probe : state.x;
        const int16_t py = horizontal ? state.y : probe;
        if (hullHits(reader, cache, px, py, &state)) {
            blocked = true;
            break;
        }
        lastValid = probe;
    }

    if (!blocked) {
        position = to;
        frac = toFrac;
        return false;
    }

    state.impactSpeedQ8 = static_cast<int16_t>(velocity < 0 ? -velocity : velocity);
    if (state.impactAxis != 4)
        state.impactAxis = horizontal ? 1 : 2;

    position = lastValid;
    frac = 0;
    velocity = 0;
    return true;
}

// One fixed 1/60 s step against the world. Physics is byte-identical to
// step(); only the position advance changes, because it now resolves against
// the cave. X moves first, then Y, matching the browser order. `collide=false`
// keeps the boat inside the baked world but ignores terrain and equipment
// gates; only the developer debug image passes it. `speedScale` is the
// developer fast-traverse multiplier and never alters the one-tick rules step.
template <typename Reader> inline void stepWorld(State &state, const Input &input, Reader &reader, const world::TileCache *cache = nullptr, bool collide = true, uint8_t speedScale = 1) {
    const bool freeFlight = state.docked < 0;
    exploration::beginStep(state);
    stepPhysics(state, input);
    // Currents add acceleration before collision resolves, so the same
    // screen-step cannot push the hull through rock or out of the world.
    if (freeFlight)
        currents::apply(state);
    // Resolve each axis immediately after its move. The impact fields still
    // latch the last blocked axis for existing presentation/tests, but debris
    // is event-scoped, so an old wall field can never re-fire.
    const bool hitX = moveAxis(state, reader, cache, true, collide, speedScale);
    if (freeFlight && hitX && state.impactAxis == 1 && game::hardWallImpact(state))
        particles::spawn(state);
    // moveAxis zeroes the blocked axis velocity, so capture the descent before
    // the Y move decides whether this was a floor slam.
    const bool descendingY = state.vyQ8 > 0;
    const bool hitY = moveAxis(state, reader, cache, false, collide, speedScale);
    if (freeFlight && hitY && state.impactAxis == 2 && game::hardWallImpact(state)) {
        particles::spawn(state);
        if (descendingY)
            game::spendHardFloorImpactAir(state);
    }
    if (!collide) {
        // No-clip still respects the sealed world border so the camera and
        // renderer always have a valid baked window to show.
        const int16_t minX = kHullHalfW;
        const int16_t maxX = static_cast<int16_t>(world::PIXELS_X - kHullHalfW);
        const int16_t minY = kHullHalfH;
        const int16_t maxY = static_cast<int16_t>(world::PIXELS_Y - kHullHalfH);
        if (state.x < minX || state.x > maxX || (state.x == minX && state.vxQ8 < 0) || (state.x == maxX && state.vxQ8 > 0)) {
            state.x = clampI16(state.x, minX, maxX);
            state.xFrac = 0;
            state.vxQ8 = 0;
        }
        if (state.y < minY || state.y > maxY || (state.y == minY && state.vyQ8 < 0) || (state.y == maxY && state.vyQ8 > 0)) {
            state.y = clampI16(state.y, minY, maxY);
            state.yFrac = 0;
            state.vyQ8 = 0;
        }
    }
    // Age the pool once per free-flight tick, after any spawn this tick.
    if (freeFlight)
        particles::tick(state);
    exploration::update(state);
    updateCamera(state);
    ++state.ticks;
    // Flight tick for the campaign voyage clock (docked frames never step).
    tickVoyage(state);
}

}   // namespace game
