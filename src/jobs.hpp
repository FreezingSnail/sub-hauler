#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_JOBS

#include "game.hpp"
#include "world.hpp"
#if SH_FEATURE_EXPLORATION
#include "exploration.hpp"
#endif

// Contract economy. Pure C++: no heap, no dynamic containers, and no float.
// Destination choice is a deterministic hash of the origin id and a
// monotonically increasing offer serial, so the same reset and the same dock
// sequence produce the same two offers on host and device.
//
// Browser reference (`spike-trim/game.js`, makeOffer):
//   distance = hypot(dx, dy) / tile            (tiles)
//   reward   = round((20 + crates*10 + 0.5*distance) / 5) * 5
//   deadline = round(80 + 0.55*distance + 8*crates)        (seconds)
//
// Rewards keep the integer floor/round shape but bake the result for the
// fixed generated settlement set. Distance is the integer tile count (floor),
// the 0.5 term is distance/2, the 0.55 term is distance*55/100, and deadline
// seconds are floored before the 60 Hz conversion. The offer path therefore
// reads one compact route record and retires the runtime coordinate subtract,
// square, square root and reward/deadline arithmetic.
// Active authored-map deadlines use conservative navigable-route lengths,
// excluding cutter/jet shortcuts, with a 7 px/s budget and 45 s maneuver slack.
// The legacy seed-4242 generator retains its historical straight-line recipe.
namespace jobs {

using game::ActiveJob;
using game::JobOffer;
using game::State;

constexpr uint8_t kCrates = game::kJobCrates;
constexpr uint8_t kOfferAttempts = 12;   // browser guard against duplicate destinations

// Kept as the economy's public rounding unit; route rewards are pre-rounded
// multiples of five and tests assert the invariant.
constexpr uint16_t kRewardRound = 5;

// Generated route bounds live in src/generated/world.hpp, so the host and
// device consume one contract instead of duplicated literals. Credits stay
// wide even though each contract reward fits one byte.
constexpr uint16_t kCreditsMax = 0xFFFFu;

// Route economy record. Reward is one byte because the generated economy tops
// out well below 255; deadlines stay 16-bit flight-tick counts.
struct RouteEconomy {
    uint8_t reward;
    uint16_t deadline;
};

static_assert(world::ROUTE_COUNT == static_cast<uint8_t>(world::SETTLEMENT_COUNT * world::SETTLEMENT_COUNT), "route table covers every ordered settlement pair");
static_assert(world::ROUTE_CRATES == game::kJobCrates, "generated route recipe uses the shipping crate count");
static_assert(world::ROUTE_STEP_HZ == game::kStepHz, "generated route recipe uses the shipping step rate");
static_assert(game::kJobElapsedLate > 0, "late marker must leave a payable range");

// Generated-bound proof for the narrow reward byte and the late sentinel.
// These fail at compile time if a future tuning pass exceeds either width.
static_assert(world::ROUTE_REWARD_MAX <= 250, "route reward keeps uint8_t headroom");
static_assert(world::ROUTE_DEADLINE_MAX < game::kJobElapsedLate, "route deadline must be strictly before the saturated late marker");

#if !defined(__AVR__)
constexpr bool routeRewardsRoundToFive() {
    for (uint8_t i = 0; i < world::ROUTE_COUNT; ++i) {
        if (world::ROUTE_REWARD[i] % kRewardRound != 0)
            return false;
    }
    return true;
}

static_assert(routeRewardsRoundToFive(), "route rewards keep the five-credit quantum");
#endif

// Consume the generated ordered-pair table. Destination selection and the
// serial cursor stay in this header; only the economy is generated.
inline RouteEconomy routeEconomy(uint8_t from, uint8_t to) {
    RouteEconomy out{0, 0};
    world::routeEconomy(from, to, out.reward, out.deadline);
    return out;
}

enum DeliveryResult : uint8_t {
    kDeliveryNone,
    kDeliveryPaid,
    kDeliveryFailed
};

// True only for the most recent failed delivery, so the station board can show
// its one-line LATE/SHORT note. Accepting or abandoning clears it.
inline bool deliveryFailed(const State &state) {
    return state.lastDelivery == kDeliveryFailed;
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

// Build one offer and advance the serial. Destination selection and the serial
// cursor are unchanged; the economy comes from the baked ordered-pair table.
// Returns false only when the world cannot supply the origin or destination.
inline bool makeOffer(State &state, uint8_t fromId, JobOffer &out) {
    ++state.offerSerial;

    if (fromId >= world::settlementCount())
        return false;

    uint8_t toId = game::kJobNone;
    if (!pickDestination(fromId, state.offerSerial, toId))
        return false;

#if SH_FEATURE_EXPLORATION
    // Do not send a new captain through a pressure boundary without the seal.
    if (!exploration::stationAvailable(state, toId)) {
        for (uint8_t offset = 1; offset < world::settlementCount(); ++offset) {
            const uint8_t candidate = static_cast<uint8_t>((toId + offset) % world::settlementCount());
            if (candidate != fromId && exploration::stationAvailable(state, candidate)) {
                toId = candidate;
                break;
            }
        }
    }
#endif
    const RouteEconomy route = routeEconomy(fromId, toId);
    out.from = fromId;
    out.to = toId;
    out.crates = kCrates;
    out.reward = route.reward;
    out.deadline = route.deadline;
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
    state.activeJob.elapsed = 0;
}

// Age is maintained by game::tickJobElapsed() and saturates at the late
// marker, so it cannot wrap.
inline uint16_t elapsedTicks(const State &state) {
    return state.activeJob.elapsed;
}

inline uint16_t remainingTicks(const State &state) {
    if (!game::hasActiveJob(state))
        return 0;
    const uint16_t elapsed = elapsedTicks(state);
    if (elapsed >= state.activeJob.deadline)
        return 0;
    return static_cast<uint16_t>(state.activeJob.deadline - elapsed);
}

// Accept an offer row. Auto-loads the contract's four cargo units, neutralizes
// trim for that load, starts the contract clock, and consumes the offer. The
// boat stays docked; the player starts travel with an explicit UNDOCK or B.
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
    state.activeJob.elapsed = 0;
    state.lastDelivery = kDeliveryNone;

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
    state.lastDelivery = kDeliveryNone;
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
    }

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
    state.lastDelivery = paid ? kDeliveryPaid : kDeliveryFailed;
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
