#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_JOBS

#include "game.hpp"
#include "world.hpp"

// Contract economy. Pure C++: no Arduino headers, no heap, no dynamic
// containers, and no float. Destination choice is a deterministic hash of the
// origin id and a monotonically increasing offer serial, so the same reset and
// the same dock sequence produce the same two offers on host and device.
//
// Browser reference (`spike-trim/game.js`, makeOffer):
//   distance = hypot(dx, dy) / tile            (tiles)
//   reward   = round((20 + crates*10 + 0.5*distance) / 5) * 5
//   deadline = round(80 + 0.55*distance + 8*crates)        (seconds)
//
// Device approximation keeps the same shape with integer-only arithmetic:
// distance is the integer tile count (floor), the 0.5 term is distance/2, the
// 0.55 term is distance*55/100, and deadline seconds are floored before the
// conversion to 60 Hz flight ticks. All constants are explicit below.
namespace jobs {

using game::ActiveJob;
using game::JobOffer;
using game::State;

constexpr uint8_t kTilePixels = 8;
constexpr uint8_t kCrates = game::kJobCrates;
constexpr uint8_t kOfferAttempts = 12; // browser guard against duplicate destinations

constexpr uint16_t kRewardBase = 20;
constexpr uint16_t kRewardPerCrate = 10;
constexpr uint16_t kRewardRound = 5;
constexpr uint16_t kRewardDistanceDivisor = 2; // 0.5 * distance

constexpr uint16_t kDeadlineBaseSeconds = 80;
constexpr uint16_t kDeadlinePercentPerTile = 55; // 0.55 * distance
constexpr uint16_t kDeadlinePercentScale = 100;
constexpr uint16_t kDeadlineSecondsPerCrate = 8;
constexpr uint16_t kDeadlineMaxTicks = 0xFFFFu;

constexpr uint16_t kCreditsMax = 0xFFFFu;

enum DeliveryResult : uint8_t {
    kDeliveryNone,
    kDeliveryPaid,
    kDeliveryFailed
};

// Integer square root. World distances fit comfortably in 16 bits, but the
// squared numerator uses 32-bit arithmetic to avoid overflow.
inline uint16_t isqrt32(uint32_t value) {
    uint32_t result = 0;
    uint32_t bit = 1UL << 30;
    while (bit > value)
        bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint16_t>(result);
}

// Deterministic 16-bit mix of origin id and offer serial.
inline uint16_t offerKey(uint8_t fromId, uint16_t serial) {
    uint32_t hash = static_cast<uint32_t>(fromId) * 40503u + static_cast<uint32_t>(serial) * 25173u + 13849u;
    hash ^= hash >> 7;
    hash *= 0x9E3779B1u;
    hash ^= hash >> 9;
    return static_cast<uint16_t>(hash);
}

// Pick an id from [0, settlementCount) that is not fromId. The rank mapping is
// stable, so a retry only needs a different serial.
inline bool pickDestination(uint8_t fromId, uint16_t serial, uint8_t &toId) {
    const uint8_t count = world::settlementCount();
    if (count < 2)
        return false;

    const uint8_t others = static_cast<uint8_t>(count - 1);
    const uint8_t rank = static_cast<uint8_t>(offerKey(fromId, serial) % others);
    toId = static_cast<uint8_t>(rank >= fromId ? rank + 1 : rank);
    return true;
}

// Build one offer and advance the serial. Returns false only when the world
// metadata cannot supply the origin or destination.
inline bool makeOffer(State &state, uint8_t fromId, JobOffer &out) {
    ++state.offerSerial;

    world::Settlement from;
    if (!world::settlementInfo(fromId, from))
        return false;

    uint8_t toId = game::kJobNone;
    if (!pickDestination(fromId, state.offerSerial, toId))
        return false;

    world::Settlement to;
    if (!world::settlementInfo(toId, to))
        return false;

    const int32_t dx = static_cast<int32_t>(to.pixel_x) - from.pixel_x;
    const int32_t dy = static_cast<int32_t>(to.pixel_y) - from.pixel_y;
    const uint32_t distanceSq = static_cast<uint32_t>(dx * dx + dy * dy);
    const uint16_t distancePixels = isqrt32(distanceSq);
    const uint16_t distanceTiles = static_cast<uint16_t>(distancePixels / kTilePixels);

    const uint16_t base = static_cast<uint16_t>(kRewardBase + kCrates * kRewardPerCrate + distanceTiles / kRewardDistanceDivisor);
    const uint16_t reward = static_cast<uint16_t>(((base + kRewardRound / 2) / kRewardRound) * kRewardRound);

    uint32_t seconds = static_cast<uint32_t>(kDeadlineBaseSeconds) +
                       (static_cast<uint32_t>(distanceTiles) * kDeadlinePercentPerTile) / kDeadlinePercentScale +
                       static_cast<uint32_t>(kCrates) * kDeadlineSecondsPerCrate;
    uint32_t ticks = seconds * game::kStepHz;
    if (ticks > kDeadlineMaxTicks)
        ticks = kDeadlineMaxTicks;

    out.from = fromId;
    out.to = toId;
    out.crates = kCrates;
    out.reward = reward;
    out.deadline = static_cast<uint16_t>(ticks);
    return true;
}

// Regenerate the fixed two-offer board for a station. Destinations are retried
// when they collide with an earlier row; a settlement set with only one other
// berth necessarily repeats and falls through after kOfferAttempts.
inline bool refreshOffers(State &state, uint8_t stationId) {
    state.offerCount = 0;
    for (uint8_t i = 0; i < game::kJobOffers; ++i) {
        state.offers[i].from = 0;
        state.offers[i].to = game::kJobNone;
        state.offers[i].crates = 0;
        state.offers[i].reward = 0;
        state.offers[i].deadline = 0;
    }

    for (uint8_t row = 0; row < game::kJobOffers; ++row) {
        bool placed = false;
        for (uint8_t attempt = 0; attempt < kOfferAttempts && !placed; ++attempt) {
            JobOffer offer;
            if (!makeOffer(state, stationId, offer))
                return false;

            bool duplicate = false;
            for (uint8_t i = 0; i < state.offerCount; ++i) {
                if (state.offers[i].to == offer.to)
                    duplicate = true;
            }

            if (!duplicate || attempt + 1 == kOfferAttempts) {
                state.offers[state.offerCount] = offer;
                ++state.offerCount;
                placed = true;
            }
        }
        if (!placed)
            return false;
    }

    return state.offerCount == game::kJobOffers;
}

inline void clearActive(State &state) {
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
    state.activeJob.reward = 0;
    state.activeJob.deadline = 0;
    state.activeJob.acceptedAt = 0;
}

inline uint32_t elapsedTicks(const State &state) {
    return state.jobClock - state.activeJob.acceptedAt;
}

inline uint16_t remainingTicks(const State &state) {
    if (!game::hasActiveJob(state))
        return 0;
    const uint32_t elapsed = elapsedTicks(state);
    if (elapsed >= state.activeJob.deadline)
        return 0;
    return static_cast<uint16_t>(state.activeJob.deadline - elapsed);
}

// Accept an offer row. Clears the hold, starts the contract clock, consumes the
// offer, and auto-loads the required four units until the M5 cargo screen owns
// loading.
inline bool accept(State &state, uint8_t offerIndex) {
    if (state.docked < 0 || game::hasActiveJob(state) || offerIndex >= state.offerCount)
        return false;

    const JobOffer offer = state.offers[offerIndex];
    if (offer.to == game::kJobNone || offer.crates == 0)
        return false;
    if (offer.from != static_cast<uint8_t>(state.docked))
        return false;

    game::setCargo(state, 0);
    state.activeJob.to = offer.to;
    state.activeJob.crates = offer.crates;
    state.activeJob.reward = offer.reward;
    state.activeJob.deadline = offer.deadline;
    state.activeJob.acceptedAt = state.jobClock;

    // MILESTONE FALLBACK: auto-load the contract hold. This is deliberately
    // unconditional for M3, including with SH_FEATURE_CARGO_LOAD=0: the loader
    // screen does not exist yet, so the fallback must keep the contract loop
    // playable in every profile. M5 replaces this with the loading-screen
    // handoff and then the flag can gate the fill.
    game::setCargo(state, offer.crates);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));

    for (uint8_t i = offerIndex; i + 1 < state.offerCount; ++i)
        state.offers[i] = state.offers[i + 1];
    --state.offerCount;
    state.offers[state.offerCount].from = 0;
    state.offers[state.offerCount].to = game::kJobNone;
    state.offers[state.offerCount].crates = 0;
    state.offers[state.offerCount].reward = 0;
    state.offers[state.offerCount].deadline = 0;
    return true;
}

// Abandon the active contract: clear the job and hold, then refresh offers for
// the station the boat is still docked at.
inline bool abandon(State &state) {
    if (state.docked < 0 || !game::hasActiveJob(state))
        return false;

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
    refreshOffers(state, static_cast<uint8_t>(state.docked));
    return true;
}

// Delivery is only evaluated at the active contract's destination. Docking at
// the origin or any other station leaves the contract and hold intact.
inline DeliveryResult deliver(State &state, uint8_t stationId) {
    if (!game::hasActiveJob(state) || state.activeJob.to != stationId)
        return kDeliveryNone;

    const bool onTime = elapsedTicks(state) <= state.activeJob.deadline;
    const bool loaded = state.cargo >= state.activeJob.crates;
    const bool paid = onTime && loaded;

    if (paid) {
        const uint16_t reward = state.activeJob.reward;
        if (state.credits > static_cast<uint16_t>(kCreditsMax - reward))
            state.credits = kCreditsMax;
        else
            state.credits = static_cast<uint16_t>(state.credits + reward);
        ++state.deliveries;
    }

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
    return paid ? kDeliveryPaid : kDeliveryFailed;
}

// Dock hook: resolve any contract ending here, then refresh this station's
// offers. Returns the delivery outcome for the sketch/tests.
inline DeliveryResult onDock(State &state, uint8_t stationId) {
    const DeliveryResult result = deliver(state, stationId);
    refreshOffers(state, stationId);
    return result;
}

}   // namespace jobs

#endif   // SH_FEATURE_JOBS
