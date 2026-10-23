#pragma once

#include <stdint.h>

#include "features.hpp"

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

constexpr uint8_t kCargoMax = 4;
constexpr int16_t kBallastMax = 100;
constexpr int16_t kNeutralBase = 50;     // ballast at zero cargo
constexpr int16_t kNeutralPerCargo = 5;  // neutral falls 5 per cargo unit

constexpr int16_t kAirCapacity = 180;       // integer air units at full tank
constexpr uint16_t kAirDrainTicks = 100;    // 0.6 units/s -> one unit per 100 ticks

#if SH_FEATURE_HULL_DAMAGE
// Browser startHp. Damage sources arrive with SH_FEATURE_HULL_DAMAGE (M7);
// the station already repairs the value so the M3 board has a real readout.
constexpr uint8_t kHullMax = 6;
#endif
constexpr uint8_t kBlowCarryStep = 3;       // 3.0 air units/s while blowing
constexpr uint8_t kBallastCarryStep = 18;   // 18 ballast units/s blow or flood
constexpr uint8_t kRateTicks = 60;          // carry denominator for per-second rates

constexpr int16_t kBallastDriveMax = 35;    // ballast units -> drive clamp
constexpr int16_t kBallastAccelQ8 = 128;    // 0.5 px/s^2, in Q8.8
constexpr int16_t kMaxVyQ8 = 20 * kQ8One;   // 20 px/s down (browser maxVy)

constexpr int16_t kThrustAccel = 34;        // px/s^2 at zero cargo
constexpr int16_t kThrustAccelPerCargo = 3;// px/s^2 lost per cargo unit
constexpr int16_t kTopSpeedBase = 30;       // px/s at zero cargo
constexpr int16_t kTopSpeedPerCargo = 3;    // px/s lost per cargo unit
constexpr int16_t kMaxVxBaseQ8 = kTopSpeedBase * kQ8One;
constexpr int16_t kMaxVxPerCargoQ8 = kTopSpeedPerCargo * kQ8One;

// 1.2 s^-1 drag at 60 Hz is 2% of velocity per tick. A one-unit minimum keeps
// small velocities decaying to rest instead of sticking above zero.
constexpr int16_t kDragDivisor = 50;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct Input {
    bool up;     // blow ballast (rise); spends air
    bool down;   // flood ballast (sink); free
    bool left;   // reverse thrust (weaker)
    bool right;  // forward thrust
};

#if SH_FEATURE_JOBS
// Contract storage is fixed and integer-only. Offers are generated for the
// station the boat is docked at; the active job keeps the deadline as a flight
// tick duration so docking pauses it with the rest of the world.
constexpr uint8_t kJobOffers = 2;
constexpr uint8_t kJobCrates = 4;
constexpr uint8_t kJobNone = 0xFF;

struct JobOffer {
    uint8_t from;      // origin settlement id
    uint8_t to;        // destination settlement id
    uint8_t crates;    // required cargo units
    uint16_t reward;   // credits, a multiple of 5
    uint16_t deadline; // allowed flight ticks from acceptance
};

struct ActiveJob {
    uint8_t to;
    uint8_t crates;
    uint16_t reward;
    uint16_t deadline;   // allowed flight ticks from acceptance
    uint32_t acceptedAt; // jobClock at acceptance
};
#endif

struct State {
    int16_t x;          // whole-pixel world position
    int16_t y;
    int16_t xFrac;      // Q12 sub-pixel remainder, 0..4095
    int16_t yFrac;
    int16_t vxQ8;       // Q8.8 pixels per second
    int16_t vyQ8;       // positive is down
    uint8_t ballast;    // 0..100
#if SH_FEATURE_SONAR
    // Sonar state is two bytes total. The host build files these into existing
    // alignment holes; the AVR layout is byte-packed, so the device image grows
    // by exactly these two bytes (1,678 -> 1,680 B).
    uint8_t sonarCooldown;   // flight ticks until the next ping is allowed
#endif
    int16_t air;        // integer units, 0..kAirCapacity
    uint8_t cargo;      // 0..4
    bool facingLeft;
    uint16_t ticks;
    uint8_t ballastCarry;    // 0..59 fractional ballast units
    uint8_t airBlowCarry;    // 0..59 fractional blown air units
    uint8_t airDrainTicks;   // 0..99 ticks toward the next drained unit

    // Follow camera and collision feedback. The camera is owned by update, not
    // render, so it can be reproduced by the host tests and stays independent
    // of display timing.
    int16_t cameraX;         // whole-pixel world offset
    int16_t cameraY;
    int16_t impactSpeedQ8;   // speed on the blocked axis before it was zeroed
    uint8_t impactAxis;      // 0 none, 1 x, 2 y
    uint8_t impacts;         // saturating count of blocked-axis contacts

#if SH_FEATURE_STATIONS
    int8_t docked;           // -1 = free flight, else the settlement id
    uint8_t dockCooldown;    // flight ticks before auto-dock is armed again
#endif
#if SH_FEATURE_HULL_DAMAGE
    uint8_t hull;            // 0..kHullMax, repaired at a station
#endif
#if SH_FEATURE_SONAR
    uint8_t sonarPings;      // saturating count of accepted pings this run
#endif
#if SH_FEATURE_JOBS
    uint16_t credits;
    uint16_t deliveries;
    uint16_t offerSerial;    // deterministic offer generator cursor
    uint32_t jobClock;       // flight ticks; paused while docked
    uint8_t offerCount;      // 0..kJobOffers valid entries in offers
    JobOffer offers[kJobOffers];
    ActiveJob activeJob;     // activeJob.to == kJobNone means no contract
#endif
};

#if SH_FEATURE_JOBS
inline bool hasActiveJob(const State &state) {
    return state.activeJob.to != kJobNone;
}
#endif

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

// Trim target: heavier cargo lowers neutral ballast, so the same ballast
// setting makes a loaded boat sink.
inline int16_t neutralFor(int16_t cargo) {
    const int16_t value = static_cast<int16_t>(kNeutralBase - kNeutralPerCargo * clampCargo(cargo));
    return clampI16(value, 0, kBallastMax);
}

inline int16_t thrustAccelFor(uint8_t cargo) {
    const uint8_t n = clampCargo(cargo);
    return static_cast<int16_t>(kThrustAccel - kThrustAccelPerCargo * n);
}

inline int16_t topSpeedQ8For(uint8_t cargo) {
    const uint8_t n = clampCargo(cargo);
    return static_cast<int16_t>(kMaxVxBaseQ8 - kMaxVxPerCargoQ8 * n);
}

// Cargo changes mass and trim but never silently moves ballast: the player
// must re-trim after loading.
inline void setCargo(State &state, int16_t cargo) {
    state.cargo = clampCargo(cargo);
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
inline void advanceAxis(int16_t &position, int16_t &fracQ12, int16_t velocityQ8) {
    const int32_t stepQ12 = static_cast<int32_t>(velocityQ8) * kSubpixelOne / (kQ8One * kStepHz);
    const int32_t accumulated = static_cast<int32_t>(fracQ12) + stepQ12;
    int32_t whole = accumulated / kSubpixelOne;
    if (accumulated < 0 && accumulated % kSubpixelOne != 0)
        --whole;
    position = static_cast<int16_t>(position + whole);
    fracQ12 = static_cast<int16_t>(accumulated - whole * kSubpixelOne);
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

inline void reset(State &state) {
    state.x = kSpawnX;
    state.y = kSpawnY;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    state.ballast = static_cast<uint8_t>(neutralFor(0));
    state.air = kAirCapacity;
    state.cargo = 0;
    state.facingLeft = false;
    state.ticks = 0;
    state.ballastCarry = 0;
    state.airBlowCarry = 0;
    state.airDrainTicks = 0;
    state.cameraX = 0;
    state.cameraY = 0;
    state.impactSpeedQ8 = 0;
    state.impactAxis = 0;
    state.impacts = 0;
#if SH_FEATURE_STATIONS
    state.docked = -1;
    state.dockCooldown = 0;
#endif
#if SH_FEATURE_HULL_DAMAGE
    state.hull = kHullMax;
#endif
#if SH_FEATURE_SONAR
    state.sonarCooldown = 0;
    state.sonarPings = 0;
#endif
#if SH_FEATURE_JOBS
    state.credits = 0;
    state.deliveries = 0;
    state.offerSerial = 0;
    state.jobClock = 0;
    state.offerCount = 0;
    for (uint8_t i = 0; i < kJobOffers; ++i) {
        state.offers[i].from = 0;
        state.offers[i].to = kJobNone;
        state.offers[i].crates = 0;
        state.offers[i].reward = 0;
        state.offers[i].deadline = 0;
    }
    state.activeJob.to = kJobNone;
    state.activeJob.crates = 0;
    state.activeJob.reward = 0;
    state.activeJob.deadline = 0;
    state.activeJob.acceptedAt = 0;
#endif
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
    const int16_t neutral = neutralFor(state.cargo);
    const int16_t drive = clampI16(static_cast<int16_t>(state.ballast - neutral), -kBallastDriveMax, kBallastDriveMax);
    const int16_t accelQ8 = static_cast<int16_t>(drive * kBallastAccelQ8 / kStepHz);
    state.vyQ8 = clampI16(static_cast<int16_t>(state.vyQ8 + accelQ8 - dragStep(state.vyQ8)), -kMaxVyQ8, kMaxVyQ8);

    // Horizontal thrust: forward is full, reverse is half. Cargo lowers both
    // acceleration and the clamp speed.
    const int16_t direction = static_cast<int16_t>(input.right) - static_cast<int16_t>(input.left);
    int16_t power = 0;
    if (direction > 0) {
        power = thrustAccelFor(state.cargo);
        state.facingLeft = false;
    } else if (direction < 0) {
        power = static_cast<int16_t>(-(thrustAccelFor(state.cargo) / 2));
        state.facingLeft = true;
    }
    const int16_t powerQ8 = static_cast<int16_t>(power * kQ8One / kStepHz);
    state.vxQ8 = clampI16(static_cast<int16_t>(state.vxQ8 + powerQ8 - dragStep(state.vxQ8)), -topSpeedQ8For(state.cargo),
                          topSpeedQ8For(state.cargo));
}

inline void step(State &state, const Input &input) {
    stepPhysics(state, input);
    advanceAxis(state.x, state.xFrac, state.vxQ8);
    advanceAxis(state.y, state.yFrac, state.vyQ8);
    ++state.ticks;
#if SH_FEATURE_SONAR
    // One flight tick, one cooldown tick. stepWorld below owns the other path.
    if (state.sonarCooldown > 0)
        --state.sonarCooldown;
#endif
#if SH_FEATURE_JOBS
    // Job deadlines measure flight ticks; the docked station screen never
    // reaches a world step, so this clock pauses with the rest of the world.
    ++state.jobClock;
#endif
}

}   // namespace game
