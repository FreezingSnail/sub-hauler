#pragma once

#include <stdint.h>

#include "game.hpp"
#include "campaign.hpp"

// Campaign contract rules. Pure C++: no heap, no dynamic containers, and no
// float. The only contract is an explicit story delivery accepted at its source
// berth; there are no generated freight offers, rewards or currency.
namespace jobs {

using game::ActiveJob;
using game::State;

static_assert(sizeof(ActiveJob) == 5, "active job payload is five bytes");

enum DeliveryResult : uint8_t {
    kDeliveryNone,
    kDeliveryDelivered,
    kDeliveryFailed
};

// True only for the most recent failed delivery, so the station board can show
// its one-line SHORT note. A fresh acceptance clears it.
inline bool deliveryFailed(const State &state) {
    return state.lastDelivery == kDeliveryFailed;
}

inline void clearActive(State &state) {
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
}

inline void countDelivered(State &state) {
    if (state.delivered != 0xFFFFu)
        ++state.delivered;
}

// Delivery is only evaluated at the active contract's destination. Docking at
// the origin or any other station leaves the contract and hold intact. There
// is no clock and no payout: a loaded arrival completes the campaign mission
// exactly once.
inline DeliveryResult deliver(State &state, uint8_t stationId) {
    if (!game::hasActiveJob(state) || state.activeJob.to != stationId)
        return kDeliveryNone;

    const bool loaded = state.cargo >= state.activeJob.crates;
    bool delivered = false;
    if (state.activeJob.kind == game::kJobKindCampaign) {
        // A campaign load completes exactly once. A malformed or already
        // completed job cannot complete; it still resolves and clears the hold.
        delivered = loaded && campaign::activeJobValid(state) && campaign::completeMission(state, state.activeJob.mission);
    }

    if (delivered)
        countDelivered(state);

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    state.lastDelivery = delivered ? kDeliveryDelivered : kDeliveryFailed;
    return delivered ? kDeliveryDelivered : kDeliveryFailed;
}

// Dock hook: resolve any contract ending here. The station board has no offers
// to refresh.
inline DeliveryResult onDock(State &state, uint8_t stationId) {
    return deliver(state, stationId);
}

}   // namespace jobs
