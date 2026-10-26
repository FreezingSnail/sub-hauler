#pragma once

#include <stdint.h>

#include "features.hpp"
#include "roster.hpp"
#if SH_FEATURE_EXPLORATION
#include "generated/exploration.hpp"
#endif

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

// HAULER hull footprint, shared by world collision and hazard collision.
// The 26x8 hull is centred on the whole-pixel state position.
constexpr int16_t kHullHalfW = 13;
constexpr int16_t kHullHalfH = 4;
constexpr int16_t kHullW = kHullHalfW * 2;
constexpr int16_t kHullH = kHullHalfH * 2;

constexpr uint8_t kCargoMax = 4;
constexpr int16_t kBallastMax = 100;
constexpr int16_t kNeutralBase = 50;      // ballast at zero cargo
constexpr int16_t kNeutralPerCargo = 5;   // neutral falls 5 per cargo unit

constexpr int16_t kAirCapacity = 180;      // integer air units at full tank
constexpr uint16_t kAirDrainTicks = 100;   // 0.6 units/s -> one unit per 100 ticks
#if SH_FEATURE_SHOP
constexpr int16_t kAirCapacityPerLevel = 30;   // air-tank upgrade step
#endif

#if SH_FEATURE_HULL_DAMAGE
// Browser startHp. Damage sources arrive with SH_FEATURE_HULL_DAMAGE (M7);
// the station already repairs the value so the M3 board has a real readout.
constexpr uint8_t kHullMax = 6;
#if SH_FEATURE_SHOP
constexpr uint8_t kHullCapacityPerLevel = 1;   // hull-plate upgrade step
#endif
#endif
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
// collisions damage the hull and puff debris while scrapes stay free. Shared
// by the damage rules and the particle spawn trigger so the two flags can be
// compiled independently.
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

#if SH_FEATURE_PARTICLES
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
#endif

#if SH_FEATURE_JOBS
// Contract storage is fixed and integer-only. Offers are generated for the
// station the boat is docked at; the active job keeps the deadline as a flight
// tick duration so docking pauses it with the rest of the world.
constexpr uint8_t kJobOffers = 2;
constexpr uint8_t kJobCrates = 4;
constexpr uint8_t kJobNone = 0xFF;

// Active-job age saturates here. Every generated deadline is strictly below
// this sentinel (asserted in jobs.hpp), so a saturated age can only mean
// "overdue": it can never wrap back into a payable window.
constexpr uint16_t kJobElapsedLate = 0xFFFF;

#if SH_FEATURE_CAMPAIGN
// Explicit job kind, replacing the implicit "six-byte timed contract". The
// saved payload freezes these values; see docs/progression-implementation.md.
constexpr uint8_t kJobKindNone = 0;
constexpr uint8_t kJobKindOrdinary = 1;
constexpr uint8_t kJobKindExpress = 2;
constexpr uint8_t kJobKindCampaign = 3;

constexpr uint8_t kMissionNone = 0xFF;

// Express pays a five-credit-rounded 125% of the generated route reward. The
// integer shape is frozen by the implementation contract.
constexpr uint8_t expressReward(uint8_t routeReward) {
    return static_cast<uint8_t>(((static_cast<uint16_t>(routeReward) * 5u + 10u) / 20u) * 5u);
}
#endif

struct JobOffer {
    uint8_t from;        // origin settlement id
    uint8_t to;          // destination settlement id
    uint8_t crates;      // required cargo units
    uint8_t reward;      // credits, a multiple of 5; generated bound-checked
#if SH_FEATURE_CAMPAIGN
    uint8_t kind;        // ORDINARY or EXPRESS; row 0 / row 1
#endif
    uint16_t deadline;   // allowed flight ticks from acceptance; 0 = untimed
};

struct ActiveJob {
#if SH_FEATURE_CAMPAIGN
    uint8_t kind;        // kJobKindNone / ORDINARY / EXPRESS / CAMPAIGN
    uint8_t mission;     // campaign mission id or kMissionNone
    uint8_t from;        // origin berth or kJobNone
#endif
    uint8_t to;
    uint8_t crates;
    uint8_t reward;      // promoted when credits are paid
    uint16_t deadline;   // allowed flight ticks from acceptance; 0 = untimed
    uint16_t elapsed;    // flight ticks since acceptance, saturating
};
#endif

struct State {
#if SH_FEATURE_EXPLORATION
    uint8_t tools;        // CUTTER / SEAL / THRUSTER / LENS
    uint8_t pickups;      // one bit per unique cove reward
    uint16_t openGates;   // cutter grates opened during this run
    uint16_t seenGates;   // encountered gate markers retained on the chart
    uint8_t contactId;    // current pickup or gate dwell target, 0xff = none
    uint8_t contactTicks;
    uint8_t discoveryTicks;
    uint8_t lastDiscovery;   // pickup id, valid only while discoveryTicks != 0
    uint8_t blockedGate;     // gate id, valid only when impactAxis == 4
#endif

    int16_t x;   // whole-pixel world position
    int16_t y;
    int16_t xFrac;   // Q12 sub-pixel remainder, 0..4095
    int16_t yFrac;
    int16_t vxQ8;      // Q8.8 pixels per second
    int16_t vyQ8;      // positive is down
    uint8_t ballast;   // 0..100
#if SH_FEATURE_SONAR
    // Sonar state is two bytes total. The host build files these into existing
    // alignment holes; the AVR layout is byte-packed, so the device image grows
    // by exactly these two bytes (1,678 -> 1,680 B).
    uint8_t sonarCooldown;   // flight ticks until the next ping is allowed
#endif
    int16_t air;     // integer units, 0..airCapacity(state)
    uint8_t cargo;   // 0..4
#if SH_FEATURE_MULTI_HULL
    // Selected roster class, owned by the title screen and kept across
    // dock/undock. One byte; the damage hull byte above is unrelated.
    uint8_t hullClass;
#endif
    bool facingLeft;
    uint16_t ticks;
    uint8_t ballastCarry;    // 0..59 fractional ballast units
    uint8_t airBlowCarry;    // 0..59 fractional blown air units
    uint8_t airDrainTicks;   // 0..99 ticks toward the next drained unit

    // Follow camera and collision feedback. The camera is owned by update, not
    // render, so it can be reproduced by the host tests and stays independent
    // of display timing.
    int16_t cameraX;   // whole-pixel world offset
    int16_t cameraY;
    int16_t impactSpeedQ8;   // speed on the blocked axis before it was zeroed
    uint8_t impactAxis;      // 0 none, 1 x, 2 y, 3 mine hit (one frame)
#if SH_FEATURE_MINES
    uint8_t minesAlive;   // one bit per generated mine, 1 = still armed
#endif

#if SH_FEATURE_STATIONS
    int8_t docked;          // -1 = free flight, else the settlement id
    uint8_t dockCooldown;   // non-zero while the undock ring latch is armed
#endif
#if SH_FEATURE_HULL_DAMAGE
    uint8_t hull;             // 0..hullCapacity(state), repaired at a station
    uint8_t damageCooldown;   // flight ticks before another wall hit can damage
#endif
#if SH_FEATURE_PARTICLES
    ParticlePool particles;   // fixed debris pool, 4 slots
#endif
#if SH_FEATURE_JOBS
    uint16_t credits;
    uint8_t lastDelivery;   // jobs::DeliveryResult from the last resolved contract
#if SH_FEATURE_CAMPAIGN
    // Campaign/voyage scalar state (the storage payload owns the fixed byte
    // order; this struct order is not serialized).
    uint8_t campaign;       // milestone bits FIRST..HOME
    uint8_t surveys;        // recorded survey bits PUMP..LIFT
    uint8_t visited;        // visited freight-settlement bits
    uint32_t voyageTicks;   // flight ticks, saturating
    uint16_t delivered;     // paid cargo jobs, saturating
    uint16_t rescues;       // recovery requests, saturating
    uint16_t boardSeed;     // offer serial origin; schema 1 writes 0
    uint8_t ending;         // ending cards presented
    uint16_t offerCursor;   // serial before the current station board
    uint16_t offerSerial;   // working offer generator serial
#else
    uint16_t offerSerial;   // deterministic offer generator cursor
#endif
    uint8_t offerCount;     // 0..kJobOffers valid entries in offers
    JobOffer offers[kJobOffers];
    ActiveJob activeJob;
#endif
#if SH_FEATURE_SHOP
    // Run-only shop levels: three bytes, no save field, no extra UI state.
    uint8_t airLevel;
    uint8_t hullLevel;
    uint8_t sonarLevel;
#endif
};

#if SH_FEATURE_JOBS
inline bool hasActiveJob(const State &state) {
#if SH_FEATURE_CAMPAIGN
    return state.activeJob.kind != kJobKindNone;
#else
    return state.activeJob.to != kJobNone;
#endif
}

// Charge one flight tick. Campaign/ordinary work is untimed and never ages;
// only express work consumes its route deadline. The voyage clock advances on
// every flight tick and saturates. Docked/chart frames never call a world step,
// and the station guard keeps a direct step() call honest too.
inline void tickJobElapsed(State &state) {
#if SH_FEATURE_STATIONS
    if (state.docked >= 0)
        return;
#endif
#if SH_FEATURE_CAMPAIGN
    if (state.voyageTicks != 0xFFFFFFFFu)
        ++state.voyageTicks;
    if (!hasActiveJob(state) || state.activeJob.kind != kJobKindExpress || state.activeJob.elapsed == kJobElapsedLate)
        return;
#else
    if (!hasActiveJob(state) || state.activeJob.elapsed == kJobElapsedLate)
        return;
#endif
    ++state.activeJob.elapsed;
}
#endif

// True only for an x/y wall impact at or above the hard-hit speed. The impact
// fields are latched by moveAxis(), so callers test this immediately after the
// axis move that produced it; a mine hit uses impactAxis 3 and never matches.
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

// Tank and hull limits include the run-only shop upgrades. With SHOP=0 the
// level fields do not exist and both helpers fold to the base constants, so
// the lean/core/min paths keep their original values and code shape.
inline int16_t airCapacity(const State &state) {
#if SH_FEATURE_SHOP
    return static_cast<int16_t>(kAirCapacity + kAirCapacityPerLevel * state.airLevel);
#else
    (void)state;
    return kAirCapacity;
#endif
}

#if SH_FEATURE_HULL_DAMAGE
inline uint8_t hullCapacity(const State &state) {
#if SH_FEATURE_SHOP
    return static_cast<uint8_t>(kHullMax + kHullCapacityPerLevel * state.hullLevel
#if SH_FEATURE_EXPLORATION
                                + ((state.pickups & (1u << exploration_data::PICKUP_CC)) != 0)
#endif
    );
#else
    (void)state;
    return kHullMax
#if SH_FEATURE_EXPLORATION
           + ((state.pickups & (1u << exploration_data::PICKUP_CC)) != 0)
#endif
        ;
#endif
}
#endif

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
// so int16 holds the step, the accumulator and the whole-pixel delta. Only the
// multiply keeps 32 bits: the result can exceed int16 before the divide.
inline void advanceAxis(int16_t &position, int16_t &fracQ12, int16_t velocityQ8) {
    const int16_t stepQ12 = static_cast<int16_t>(static_cast<int32_t>(velocityQ8) * 4 / 15);
    const int16_t accumulated = static_cast<int16_t>(fracQ12 + stepQ12);
    int16_t whole = static_cast<int16_t>(accumulated / kSubpixelOne);
    if (accumulated < 0 && accumulated % kSubpixelOne != 0)
        --whole;
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
#if SH_FEATURE_EXPLORATION
    state.contactId = 0xFF;
#endif
    state.x = kSpawnX;
    state.y = kSpawnY;
    state.ballast = static_cast<uint8_t>(neutralFor(0));
    state.air = airCapacity(state);
#if SH_FEATURE_MULTI_HULL
    state.hullClass = roster::kDefault;
#endif
#if SH_FEATURE_MINES
    // The generated world ships MINE_COUNT = 8, so all bits start armed.
    // hazards::reset() masks this to MINE_COUNT for callers that need it.
    state.minesAlive = 0xFF;
#endif
#if SH_FEATURE_STATIONS
    state.docked = -1;
#endif
#if SH_FEATURE_HULL_DAMAGE
    state.hull = hullCapacity(state);
#endif
#if SH_FEATURE_JOBS
    state.activeJob.to = kJobNone;
#if SH_FEATURE_CAMPAIGN
    state.activeJob.kind = kJobKindNone;
    state.activeJob.mission = kMissionNone;
    state.activeJob.from = kJobNone;
    // Schema 1 starts with boardSeed 0, preserving the historical sequence.
    state.offerSerial = state.boardSeed;
    state.offerCursor = state.boardSeed;
#endif
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
    state.vxQ8 = clampI16(static_cast<int16_t>(state.vxQ8 + powerQ8 - dragStep(state.vxQ8)), -topSpeedQ8For(state.cargo), topSpeedQ8For(state.cargo));
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
    // reaches a world step, so the age pauses with the rest of the world.
    tickJobElapsed(state);
#endif
}

}   // namespace game
