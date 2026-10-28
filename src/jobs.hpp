#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_JOBS

#include "game.hpp"
#include "world.hpp"
#if SH_FEATURE_EXPLORATION
#include "exploration.hpp"
#endif
#if SH_FEATURE_CAMPAIGN
#include "campaign.hpp"
#endif

// Contract economy. Pure C++: no heap, no dynamic containers, and no float.
// Destination choice is a deterministic hash of the origin id and a
// monotonically increasing offer serial, so the same reset and the same dock
// sequence produce the same freight row on host and device.
//
// Browser reference (`spike-trim/game.js`, makeOffer):
//   distance = hypot(dx, dy) / tile            (tiles)
//   reward   = round((20 + crates*10 + 0.5*distance) / 5) * 5
//
// Rewards keep the integer floor/round shape but bake the result for the fixed
// generated settlement set. Distance is the integer tile count (floor), the 0.5
// term is distance/2. The offer path therefore reads one compact route record
// and retires the runtime coordinate subtract, square, square root and reward
// arithmetic. Every job is untimed; the generated route deadline column is no
// longer read by runtime code.
namespace jobs {

using game::ActiveJob;
using game::JobOffer;
using game::State;

constexpr uint8_t kCrates = game::kJobCrates;

// Kept as the economy's public rounding unit; route rewards are pre-rounded
// multiples of five and tests assert the invariant.
constexpr uint16_t kRewardRound = 5;

// Generated route bounds live in src/generated/level.hpp, so the host and
// device consume one contract instead of duplicated literals. Credits stay
// wide even though each contract reward fits one byte.
constexpr uint16_t kCreditsMax = 0xFFFFu;

// Route economy record. Reward is one byte because the generated economy tops
// out well below 255. The generated deadline column is no longer read.
struct RouteEconomy {
    uint8_t reward;
};

static_assert(world::ROUTE_COUNT == static_cast<uint8_t>(world::SETTLEMENT_COUNT * world::SETTLEMENT_COUNT), "route table covers every ordered settlement pair");
static_assert(world::ROUTE_CRATES == game::kJobCrates, "generated route recipe uses the shipping crate count");
static_assert(world::ROUTE_STEP_HZ == game::kStepHz, "generated route recipe uses the shipping step rate");

// Generated-bound proof for the narrow reward byte.
static_assert(world::ROUTE_REWARD_MAX <= 250, "route reward keeps uint8_t headroom");

#if SH_FEATURE_CAMPAIGN
static_assert(sizeof(ActiveJob) == 6, "active job payload is six bytes");
#endif

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
    RouteEconomy out{0};
    uint16_t ignoredDeadline = 0;
    world::routeEconomy(from, to, out.reward, ignoredDeadline);
    return out;
}

enum DeliveryResult : uint8_t {
    kDeliveryNone,
    kDeliveryPaid,
    kDeliveryFailed
};

// True only for the most recent failed delivery, so the station board can show
// its one-line SHORT note. Accepting or abandoning clears it.
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

// Build the one repeatable freight offer and advance the working serial.
// The destination must be reachable with the current equipment.
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
#if SH_FEATURE_CAMPAIGN
    out.kind = game::kJobKindOrdinary;
#endif
    return true;
}

// Regenerate the single freight row for a station.
inline bool refreshOffers(State &state, uint8_t stationId) {
    // offerCount is the only visibility gate: every reader (board, accept and
    // snapshots) iterates rows below it.
    state.offerCount = 0;

#if SH_FEATURE_CAMPAIGN
    // The saved cursor is the serial before this board; the working serial
    // advances exactly as the historical generator did. Restoring offerSerial
    // from offerCursor reproduces this board without advancing saved state.
    state.offerCursor = state.offerSerial;
#endif

    JobOffer offer;
    if (!makeOffer(state, stationId, offer))
        return false;
    state.offers[0] = offer;
    state.offerCount = 1;
    return true;
}

inline void clearActive(State &state) {
#if SH_FEATURE_CAMPAIGN
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
#endif
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
    state.activeJob.reward = 0;
}

#if SH_FEATURE_CAMPAIGN
inline void credit(State &state, uint16_t reward) {
    if (state.credits > static_cast<uint16_t>(kCreditsMax - reward))
        state.credits = kCreditsMax;
    else
        state.credits = static_cast<uint16_t>(state.credits + reward);
}

inline void countDelivered(State &state) {
    if (state.delivered != 0xFFFFu)
        ++state.delivered;
}
#endif

// Accept the freight offer. Auto-loads the contract's four cargo units,
// neutralizes trim for that load, and consumes the offer. The boat stays
// docked; the player starts travel with an explicit UNDOCK or B.
inline bool accept(State &state, uint8_t offerIndex) {
    if (state.docked < 0 || game::hasActiveJob(state) || offerIndex >= state.offerCount)
        return false;

    const JobOffer offer = state.offers[offerIndex];
    if (offer.to == game::kJobNone || offer.crates == 0)
        return false;
    if (offer.from != static_cast<uint8_t>(state.docked))
        return false;
#if SH_FEATURE_CAMPAIGN
    if (offer.kind != game::kJobKindOrdinary)
        return false;
    state.activeJob.kind = game::kJobKindOrdinary;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = offer.from;
#endif

    game::setCargo(state, 0);
    state.activeJob.to = offer.to;
    state.activeJob.crates = offer.crates;
    state.activeJob.reward = offer.reward;
    state.lastDelivery = kDeliveryNone;

    game::setCargo(state, offer.crates);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));

    if (offerIndex + 1 < state.offerCount)
        state.offers[offerIndex] = state.offers[offerIndex + 1];
    --state.offerCount;
    // The vacated tail row is above offerCount and therefore invisible; the
    // next refresh overwrites it.
    return true;
}

// Abandon the active contract: clear the job and hold, then refresh offers for
// the station the boat is still docked at.
inline bool abandon(State &state) {
    if (state.docked < 0 || !game::hasActiveJob(state))
        return false;

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    state.lastDelivery = kDeliveryNone;
    refreshOffers(state, static_cast<uint8_t>(state.docked));
    return true;
}

// Delivery is only evaluated at the active contract's destination. Docking at
// the origin or any other station leaves the contract and hold intact. There
// is no clock: a loaded arrival always pays, however long the trip took.
inline DeliveryResult deliver(State &state, uint8_t stationId) {
    if (!game::hasActiveJob(state) || state.activeJob.to != stationId)
        return kDeliveryNone;

    const bool loaded = state.cargo >= state.activeJob.crates;
    bool paid = false;
#if SH_FEATURE_CAMPAIGN
    if (state.activeJob.kind == game::kJobKindCampaign) {
        // A campaign load completes exactly once. A malformed or already
        // completed job cannot pay; it still resolves and clears the hold.
        paid = loaded && campaign::activeJobValid(state) && campaign::completeMission(state, state.activeJob.mission);
    } else {
        // Ordinary work pays its recorded reward on a loaded arrival. There is
        // no deadline and no premium.
        paid = loaded;
    }
#else
    paid = loaded;
#endif

    if (paid) {
#if SH_FEATURE_CAMPAIGN
        credit(state, state.activeJob.reward);
        countDelivered(state);
#else
        const uint16_t reward = state.activeJob.reward;
        if (state.credits > static_cast<uint16_t>(kCreditsMax - reward))
            state.credits = kCreditsMax;
        else
            state.credits = static_cast<uint16_t>(state.credits + reward);
#endif
    }

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    state.lastDelivery = paid ? kDeliveryPaid : kDeliveryFailed;
    return paid ? kDeliveryPaid : kDeliveryFailed;
}

// Dock hook: resolve any contract ending here, then refresh this station's
// offer. Returns the delivery outcome for the sketch/tests.
inline DeliveryResult onDock(State &state, uint8_t stationId) {
#if SH_FEATURE_CAMPAIGN
    campaign::markVisited(state, stationId);
#endif
    const DeliveryResult result = deliver(state, stationId);
    refreshOffers(state, stationId);
    return result;
}

}   // namespace jobs

#endif   // SH_FEATURE_JOBS
