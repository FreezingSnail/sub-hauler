#pragma once

#include <stdint.h>

#include "roster.hpp"
#include "generated/exploration.hpp"

// Pure submarine rules. No Arduino headers: this file is compiled into the
// host test binary and the firmware from the same source.
//
// Fixed step: one step() call is exactly 1/60 s. There is no dt argument, no
// float/double, and no heap or dynamic container. Positions are whole pixels
// plus a Q12 sub-pixel remainder; velocities are Q8.8 pixels per second.
namespace game {

// ---------------------------------------------------------------------------
// Tuning (all integer; units documented)
// ---------------------------------------------------------------------------

constexpr int16_t kStepHz = 60;
constexpr int16_t kQ8One = 256;          // Q8.8 one pixel per second
constexpr int16_t kSubpixelOne = 4096;   // Q12 sub-pixel one

constexpr int16_t kSpawnX = 64;
constexpr int16_t kSpawnY = 32;

// HAULER hull footprint, shared by world collision and debris placement.
// The 26x8 hull is centred on the whole-pixel state position.
constexpr int16_t kHullHalfW = 13;
constexpr int16_t kHullHalfH = 4;
constexpr int16_t kHullW = kHullHalfW * 2;
constexpr int16_t kHullH = kHullHalfH * 2;

constexpr uint8_t kCargoMax = 4;
constexpr int16_t kBallastMax = 100;
constexpr int16_t kNeutralBase = 50;      // ballast at zero cargo
constexpr int16_t kNeutralPerCargo = 5;   // neutral falls 5 per cargo unit

constexpr int16_t kAirCapacity = 300;      // fixed integer air units at full tank
constexpr uint16_t kAirDrainTicks = 150;   // 0.4 units/s -> one unit per 150 ticks

constexpr uint8_t kBlowCarryStep = 3;       // 3.0 air units/s while blowing
constexpr uint8_t kBallastCarryStep = 18;   // 18 ballast units/s blow or flood
constexpr uint8_t kRateTicks = 60;          // carry denominator for per-second rates

constexpr int16_t kBallastDriveMax = 35;    // ballast units -> drive clamp
constexpr int16_t kBallastAccelQ8 = 128;    // 0.5 px/s^2, in Q8.8
constexpr int16_t kMaxVyQ8 = 20 * kQ8One;   // 20 px/s down (browser maxVy)

constexpr int16_t kThrustAccel = 34;          // px/s^2 at zero cargo
constexpr int16_t kThrustAccelPerCargo = 3;   // px/s^2 lost per cargo unit
constexpr int16_t kTopSpeedBase = 30;         // px/s at zero cargo
constexpr int16_t kTopSpeedPerCargo = 3;      // px/s lost per cargo unit
constexpr int16_t kMaxVxBaseQ8 = kTopSpeedBase * kQ8One;
constexpr int16_t kMaxVxPerCargoQ8 = kTopSpeedPerCargo * kQ8One;

// 1.2 s^-1 drag at 60 Hz is 2% of velocity per tick. A one-unit minimum keeps
// small velocities decaying to rest instead of sticking above zero.
constexpr int16_t kDragDivisor = 50;

// A blocked-axis speed at or above 16 px/s is a hard wall hit. That is well
// under the 30 px/s base top speed but above a gentle nudge, so deliberate
// collisions puff debris while scrapes stay free. Shared by the particle spawn
// trigger and the wall-impact presentation.
constexpr int16_t kHardImpactSpeedQ8 = 16 * kQ8One;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct Input {
    bool up;      // blow ballast (rise); spends air
    bool down;    // flood ballast (sink); free
    bool left;    // reverse thrust (weaker)
    bool right;   // forward thrust
};

// Fixed debris pool: four particles, no heap, no per-particle velocity bytes.
// Deltas are signed offsets from one shared anchor captured at the impact, so
// each particle fits in three bytes; slot parity owns the fixed outward
// diagonal. The pool is 4 B anchor + 4 x 3 B = 16 B, integer-only.
constexpr uint8_t kParticleCount = 4;

struct Particle {
    int8_t dx;   // bounded pixel offset from the pool anchor
    int8_t dy;
    uint8_t life;   // 0 = slot inactive
};

struct ParticlePool {
    int16_t x;   // spawn anchor in world pixels
    int16_t y;
    Particle particle[kParticleCount];
};

// Contract storage is fixed and integer-only. Campaign story deliveries are
// the only contracts: the WORK board lists restoration rows, and accepting one
// loads the four-unit hold. Every job is untimed: air and handling are the
// expedition pressure.
constexpr uint8_t kJobCrates = 4;
constexpr uint8_t kJobNone = 0xFF;

// Explicit job kind. The save schema records only NONE or CAMPAIGN; there is no
// ordinary freight kind and no timed job kind. See
// docs/progression-implementation.md.
constexpr uint8_t kJobKindNone = 0;
constexpr uint8_t kJobKindCampaign = 2;

constexpr uint8_t kMissionNone = 0xFF;

struct ActiveJob {
    uint8_t kind;        // kJobKindNone / CAMPAIGN
    uint8_t mission;     // campaign mission id or kMissionNone
    uint8_t from;        // origin berth or kJobNone
    uint8_t to;
    uint8_t crates;
};

struct State {
    uint8_t tools;        // CUTTER / SEAL / THRUSTER / LENS
    uint8_t pickups;      // one bit per unique cove reward
    uint16_t openGates;   // cutter grates opened during this run
    uint16_t seenGates;   // encountered gate markers retained on the chart
    uint8_t contactId;    // current pickup or gate dwell target, 0xff = none
    uint8_t contactTicks;
    uint8_t discoveryTicks;
    uint8_t lastDiscovery;   // pickup id, valid only while discoveryTicks != 0
    uint8_t blockedGate;     // gate id, valid only when impactAxis == 4

    int16_t x;   // whole-pixel world position
    int16_t y;
    int16_t xFrac;   // Q12 sub-pixel remainder, 0..4095
    int16_t yFrac;
    int16_t vxQ8;      // Q8.8 pixels per second
    int16_t vyQ8;      // positive is down
    uint8_t ballast;   // 0..100
    int16_t air;     // integer units, 0..airCapacity(state)
    uint8_t cargo;   // 0..4
    // Selected roster class, owned by the title screen and kept across
    // dock/undock.
    uint8_t hullClass;
    bool facingLeft;
    uint16_t ticks;
    uint8_t ballastCarry;    // 0..59 fractional ballast units
    uint8_t airBlowCarry;    // 0..59 fractional blown air units
    uint8_t airDrainTicks;   // 0..99 ticks toward the next drained unit

    // Follow camera and collision feedback. The camera is owned by update, not
    // render, so it can be reproduced by the host tests and stays independent
    // of display timing.
    uint16_t cameraXQ4;   // unsigned Q4 world origin (1/16 px)
    uint16_t cameraYQ4;
    int16_t impactSpeedQ8;   // speed on the blocked axis before it was zeroed
    uint8_t impactAxis;      // 0 none, 1 x, 2 y, 4 blocking gate (one frame)

    int8_t docked;          // -1 = free flight, else the settlement id
    uint8_t dockCooldown;   // non-zero while the undock ring latch is armed
    ParticlePool particles;   // fixed debris pool, 4 slots
    uint8_t lastDelivery;   // jobs::DeliveryResult from the last resolved contract
    // Campaign/voyage scalar state (the storage payload owns the fixed byte
    // order; this struct order is not serialized).
    uint8_t campaign;       // milestone bits FIRST..HOME
    uint8_t surveys;        // recorded survey bits PUMP..LIFT
    uint32_t voyageTicks;   // flight ticks, saturating
    uint16_t delivered;     // completed campaign deliveries, saturating
    uint16_t rescues;       // recovery requests, saturating
    uint8_t ending;         // ending cards presented
    ActiveJob activeJob;
};

inline bool hasActiveJob(const State &state) {
    return state.activeJob.kind != kJobKindNone;
}

// Charge one flight tick to the campaign voyage clock. Job age is not part of
// the simulation: every story delivery is untimed. Docked/chart frames never
// call a world step, and the station guard keeps a direct step() call honest
// too.
inline void tickVoyage(State &state) {
    if (state.docked >= 0)
        return;
    if (state.voyageTicks != 0xFFFFFFFFu)
        ++state.voyageTicks;
}

// True only for an x/y wall impact at or above the hard-hit speed. The impact
// fields are latched by moveAxis(), so callers test this immediately after the
// axis move that produced it.
inline bool hardWallImpact(const State &state) {
    return (state.impactAxis == 1 || state.impactAxis == 2) && state.impactSpeedQ8 >= kHardImpactSpeedQ8;
}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

inline int16_t clampI16(int16_t value, int16_t lo, int16_t hi) {
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

inline uint8_t clampCargo(int16_t cargo) {
    if (cargo < 0)
        return 0;
    if (cargo > kCargoMax)
        return kCargoMax;
    return static_cast<uint8_t>(cargo);
}

// State invariant: state.cargo is always 0..kCargoMax. setCargo() is the only
// runtime writer; the save loader writes it from a validated active job whose
// crates field is either 0 or kJobCrates. The *Normalized helpers encode that
// invariant so the hot rules do not re-clamp a value that is already in range.
// The public helpers keep the clamping contract for direct callers/tests.
static_assert(kNeutralBase - kNeutralPerCargo * kCargoMax >= 0, "normalized trim cannot go negative");
static_assert(kNeutralBase <= kBallastMax, "normalized trim cannot exceed ballast max");

inline int16_t neutralForNormalized(uint8_t cargo) {
    return static_cast<int16_t>(kNeutralBase - kNeutralPerCargo * cargo);
}

// Trim target: heavier cargo lowers neutral ballast, so the same ballast
// setting makes a loaded boat sink.
inline int16_t neutralFor(int16_t cargo) {
    return neutralForNormalized(clampCargo(cargo));
}

inline int16_t thrustAccelForNormalized(uint8_t cargo) {
    return static_cast<int16_t>(kThrustAccel - kThrustAccelPerCargo * cargo);
}

inline int16_t thrustAccelFor(uint8_t cargo) {
    return thrustAccelForNormalized(clampCargo(cargo));
}

inline int16_t topSpeedQ8ForNormalized(uint8_t cargo) {
    return static_cast<int16_t>(kMaxVxBaseQ8 - kMaxVxPerCargoQ8 * cargo);
}

inline int16_t topSpeedQ8For(uint8_t cargo) {
    return topSpeedQ8ForNormalized(clampCargo(cargo));
}

// Cargo changes mass and trim but never silently moves ballast: the player
// must re-trim after loading.
inline void setCargo(State &state, int16_t cargo) {
    state.cargo = clampCargo(cargo);
}

// Fixed 300-unit tank.
inline int16_t airCapacity(const State &state) {
    (void)state;
    return kAirCapacity;
}

// ---------------------------------------------------------------------------
// Motion integration
// ---------------------------------------------------------------------------

inline int16_t dragStep(int16_t velocityQ8) {
    const int16_t drop = static_cast<int16_t>(velocityQ8 / kDragDivisor);
    if (drop == 0 && velocityQ8 > 0)
        return 1;
    if (drop == 0 && velocityQ8 < 0)
        return -1;
    return drop;
}

// Advance one axis by velocityQ8 * (1/60) s. The Q12 remainder keeps slow
// velocities moving smoothly instead of dead-zoning below one Q8 unit.
//
// Width proof for any int16 velocity:
//   stepQ12    = velocity * 4 / 15  in -8738..8737 (truncates toward zero)
//   accumulated = fracQ12 + stepQ12 in -8738..12832 (fracQ12 is 0..4095)
//   whole       = floor(accumulated / 4096) in -3..3
// so int16 holds every intermediate. Quotient/remainder decomposition avoids
// the wide product while preserving truncation for both velocity signs.
inline void advanceAxis(int16_t &position, int16_t &fracQ12, int16_t velocityQ8) {
    // Split before multiplying: both terms keep the sign of velocity, so
    // truncation equals (int32_t(velocity) * 4) / 15 for every int16 value.
    const int16_t stepQ12 = static_cast<int16_t>((velocityQ8 / 15) * 4 + ((velocityQ8 % 15) * 4) / 15);
    const int16_t accumulated = static_cast<int16_t>(fracQ12 + stepQ12);
    static_assert(kSubpixelOne == 4096 && (-1 >> 12) == -1, "Q12 uses arithmetic floor shift");
    const int16_t whole = static_cast<int16_t>(accumulated >> 12);
    position = static_cast<int16_t>(position + whole);
    fracQ12 = static_cast<int16_t>(accumulated - whole * kSubpixelOne);
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

inline void reset(State &state) {
    // Zero the whole aggregate first, then restore the non-zero defaults. This
    // keeps every byte (including inactive feature state) deterministic without
    // a long field-by-field clear in the device image.
    state = State{};
    state.contactId = 0xFF;
    state.x = kSpawnX;
    state.y = kSpawnY;
    state.ballast = static_cast<uint8_t>(neutralForNormalized(0));
    state.air = airCapacity(state);
    state.hullClass = roster::kDefault;
    state.docked = -1;
    state.activeJob.to = kJobNone;
    state.activeJob.kind = kJobKindNone;
    state.activeJob.mission = kMissionNone;
    state.activeJob.from = kJobNone;
}

// Physics-only half of the fixed step. Callers that own collision use this
// and then advance each axis through the world; step() below is the pure
// open-water path used by the existing physics tests.
inline void stepPhysics(State &state, const Input &input) {
    // Air always drains, even while holding or flooding.
    if (++state.airDrainTicks >= kAirDrainTicks) {
        state.airDrainTicks = 0;
        if (state.air > 0)
            --state.air;
    }

    const bool blow = input.up && !input.down;
    const bool flood = input.down && !input.up;

    if (blow && state.air > 0) {
        // Blowing spends air and lightens the boat.
        state.airBlowCarry = static_cast<uint8_t>(state.airBlowCarry + kBlowCarryStep);
        if (state.airBlowCarry >= kRateTicks) {
            state.airBlowCarry = static_cast<uint8_t>(state.airBlowCarry - kRateTicks);
            --state.air;
        }

        if (state.ballast > 0) {
            state.ballastCarry = static_cast<uint8_t>(state.ballastCarry + kBallastCarryStep);
            if (state.ballastCarry >= kRateTicks) {
                state.ballastCarry = static_cast<uint8_t>(state.ballastCarry - kRateTicks);
                --state.ballast;
            }
        } else {
            state.ballastCarry = 0;
        }
    } else if (flood) {
        // Flooding is free: no air cost.
        if (state.ballast < kBallastMax) {
            state.ballastCarry = static_cast<uint8_t>(state.ballastCarry + kBallastCarryStep);
            if (state.ballastCarry >= kRateTicks) {
                state.ballastCarry = static_cast<uint8_t>(state.ballastCarry - kRateTicks);
                ++state.ballast;
            }
        } else {
            state.ballastCarry = 0;
        }
    } else {
        // Release holds the current ballast.
        state.ballastCarry = 0;
    }

    // Vertical drive: positive ballast difference is heavier and sinks.
    const int16_t neutral = neutralForNormalized(state.cargo);
    const int16_t drive = clampI16(static_cast<int16_t>(state.ballast - neutral), -kBallastDriveMax, kBallastDriveMax);
    const int16_t accelQ8 = static_cast<int16_t>(drive * kBallastAccelQ8 / kStepHz);
    state.vyQ8 = clampI16(static_cast<int16_t>(state.vyQ8 + accelQ8 - dragStep(state.vyQ8)), -kMaxVyQ8, kMaxVyQ8);

    // Horizontal thrust: forward is full, reverse is half. Cargo lowers both
    // acceleration and the clamp speed.
    const int16_t direction = static_cast<int16_t>(input.right) - static_cast<int16_t>(input.left);
    int16_t power = 0;
    if (direction > 0) {
        power = thrustAccelForNormalized(state.cargo);
        state.facingLeft = false;
    } else if (direction < 0) {
        power = static_cast<int16_t>(-(thrustAccelForNormalized(state.cargo) / 2));
        state.facingLeft = true;
    }
    const int16_t powerQ8 = static_cast<int16_t>(power * kQ8One / kStepHz);
    state.vxQ8 = clampI16(static_cast<int16_t>(state.vxQ8 + powerQ8 - dragStep(state.vxQ8)), -topSpeedQ8ForNormalized(state.cargo), topSpeedQ8ForNormalized(state.cargo));
}

inline void step(State &state, const Input &input) {
    stepPhysics(state, input);
    advanceAxis(state.x, state.xFrac, state.vxQ8);
    advanceAxis(state.y, state.yFrac, state.vyQ8);
    ++state.ticks;
    // Campaign voyage clock; job timers are gone.
    tickVoyage(state);
}

}   // namespace game
