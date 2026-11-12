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

inline void clearActive(State &state) {
    state.activeJob.kind = game::kJobKindNone;
    state.activeJob.mission = game::kMissionNone;
    state.activeJob.from = game::kJobNone;
    state.activeJob.to = game::kJobNone;
    state.activeJob.crates = 0;
}

// Delivery is only evaluated at the active contract's destination. Docking at
// the origin or any other station leaves the contract and hold intact. There is
// no clock and no payout: a loaded arrival with matching mission metadata
// completes the campaign mission exactly once. A malformed load is still
// cleared, but cannot complete its mission.
inline bool deliver(State &state, uint8_t stationId) {
    if (!game::hasActiveJob(state) || state.activeJob.to != stationId)
        return false;

    const bool loaded = state.cargo >= state.activeJob.crates;
    const bool delivered = loaded && campaign::cargoJobMatches(state.activeJob) &&
                           campaign::completeMission(state, state.activeJob.mission);

    clearActive(state);
    game::setCargo(state, 0);
    state.ballast = static_cast<uint8_t>(game::neutralForNormalized(state.cargo));
    return delivered;
}

// Dock hook: resolve any contract ending here. The station board has no offers
// to refresh.
inline bool onDock(State &state, uint8_t stationId) {
    return deliver(state, stationId);
}

}   // namespace jobs
