#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_STATIONS

#include "game.hpp"
#include "pilot.hpp"
#include "world.hpp"
#if SH_FEATURE_JOBS
#include "jobs.hpp"
#endif

// Docking, station service, and station-row rules. Pure C++: no Arduino
// headers, no cart reads here, and no display access. The sketch owns the
// screen state machine; this header owns the state transition and its
// invariants.
//
// Auto-dock is the selected M3 input contract: a boat inside a settlement
// ring, at or below the dock speed after a world step, docks without spending
// a button. This mirrors the browser ring radius and speed threshold.
namespace station {

using game::Input;
using game::kAirCapacity;
using game::kQ8One;
using game::neutralFor;
using game::snapCamera;
using game::State;
#if SH_FEATURE_HULL_DAMAGE
using game::kHullMax;
#endif

// The generated dock ring is the single source of truth for both the drawn
// gantry and the docking check.
constexpr int16_t kDockRadius = world::DOCK_CLEARANCE_PX;
constexpr int32_t kDockRadiusSq = static_cast<int32_t>(kDockRadius) * kDockRadius;

// Browser CFG.dockSpeed: 16 px/s. Q8.8 keeps the comparison integer.
constexpr int16_t kDockSpeedQ8 = 16 * kQ8One;
constexpr int32_t kDockSpeedSq = static_cast<int32_t>(kDockSpeedQ8) * kDockSpeedQ8;

// Undocking must not instantly re-dock on the next idle frame. Give the
// player a short window to thrust clear of the ring; this is 1.5 s at 60 Hz.
constexpr uint8_t kUndockCooldown = 90;

// Row extension point for M3 jobs. Offers occupy the leading rows; the
// service and undock rows stay last so their actions never move when jobs are
// compiled out. With an active contract the board instead shows a summary row
// followed by ABANDON and UNDOCK, so the two board shapes are:
//
//   no job:  [offer 0] [offer 1] SERVICE UNDOCK
//   active:  [summary] ABANDON UNDOCK
//
// Only selectable rows participate in wrap-around; the summary row is inert.
#if SH_FEATURE_JOBS
constexpr uint8_t kOfferRows = game::kJobOffers;
#else
constexpr uint8_t kOfferRows = 0;
#endif
constexpr uint8_t kServiceRow = kOfferRows;
constexpr uint8_t kUndockRow = static_cast<uint8_t>(kOfferRows + 1);
constexpr uint8_t kStationRowCount = static_cast<uint8_t>(kOfferRows + 2);
constexpr uint8_t kFirstRow = 0;

#if SH_FEATURE_JOBS
constexpr uint8_t kActiveSummaryRow = 0;
constexpr uint8_t kAbandonRow = 1;
constexpr uint8_t kActiveUndockRow = 2;
constexpr uint8_t kActiveRowCount = 3;
#endif

enum RowKind : uint8_t {
    kRowOffer,
    kRowService,
    kRowUndock
#if SH_FEATURE_JOBS
    ,
    kRowJob,
    kRowAbandon
#endif
};

enum Action : uint8_t {
    kActionNone,
    kActionServiced,
    kActionUndocked
#if SH_FEATURE_JOBS
    ,
    kActionAccepted,
    kActionAbandoned
#endif
};

inline bool jobActive(const State &state) {
#if SH_FEATURE_JOBS
    return game::hasActiveJob(state);
#else
    (void)state;
    return false;
#endif
}

inline uint8_t rowCount(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state))
        return kActiveRowCount;
#endif
    return kStationRowCount;
}

inline RowKind rowKind(uint8_t row, const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state)) {
        if (row == kActiveSummaryRow)
            return kRowJob;
        if (row == kAbandonRow)
            return kRowAbandon;
        return kRowUndock;
    }
#endif
    if (row < kOfferRows)
        return kRowOffer;
    if (row == kServiceRow)
        return kRowService;
    return kRowUndock;
}

// First selectable row for a freshly docked boat: offer 0, or ABANDON when a
// contract is already running (wrong-station dock).
inline uint8_t initialRow(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state))
        return kAbandonRow;
#endif
    return kFirstRow;
}

inline uint8_t selectableCount(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state))
        return static_cast<uint8_t>(kActiveRowCount - kAbandonRow);
#endif
    return kStationRowCount;
}

// Selection wraps across selectable rows only, so a two-row active board never
// dead-ends. The caller keeps the selected row in range via initialRow().
inline uint8_t selectPrevious(uint8_t selected, const State &state) {
    const uint8_t first = initialRow(state);
    const uint8_t last = static_cast<uint8_t>(first + selectableCount(state) - 1);
    if (selected <= first || selected > last)
        return last;
    return static_cast<uint8_t>(selected - 1);
}

inline uint8_t selectNext(uint8_t selected, const State &state) {
    const uint8_t first = initialRow(state);
    const uint8_t last = static_cast<uint8_t>(first + selectableCount(state) - 1);
    if (selected < first || selected >= last)
        return first;
    return static_cast<uint8_t>(selected + 1);
}

// Squared speed in Q8.8 units. Two int16 velocities squared fit in int32.
inline int32_t speedSqQ8(const State &state) {
    return static_cast<int32_t>(state.vxQ8) * state.vxQ8 + static_cast<int32_t>(state.vyQ8) * state.vyQ8;
}

inline bool slowEnoughToDock(const State &state) {
    return speedSqQ8(state) <= kDockSpeedSq;
}

// Nearest settlement whose dock pixel is within the ring. A tie keeps the
// lower settlement id, which makes the result deterministic without relying
// on generated table order beyond the documented id order.
inline bool nearestDock(const State &state, uint8_t &settlementId) {
    int32_t bestSq = 0;
    bool found = false;

    for (uint8_t id = 0; id < world::settlementCount(); ++id) {
        world::Settlement settlement;
        if (!world::settlementInfo(id, settlement))
            continue;

        const int32_t dx = static_cast<int32_t>(settlement.pixel_x) - state.x;
        const int32_t dy = static_cast<int32_t>(settlement.pixel_y) - state.y;
        const int32_t distanceSq = dx * dx + dy * dy;
        if (distanceSq > kDockRadiusSq)
            continue;
        if (!found || distanceSq < bestSq) {
            bestSq = distanceSq;
            settlementId = id;
            found = true;
        }
    }

    return found;
}

// Full tank, full hull, and neutral trim. Carries are cleared so a held valve
// cannot move the boat on the first frame after undocking.
inline void service(State &state) {
    state.air = kAirCapacity;
#if SH_FEATURE_HULL_DAMAGE
    state.hull = kHullMax;
#endif
    state.ballast = static_cast<uint8_t>(neutralFor(state.cargo));
    state.ballastCarry = 0;
    state.airBlowCarry = 0;
    state.airDrainTicks = 0;
}

// Dock at the nearest ring. Position snaps to the generated dock pixel, which
// is the same anchor the gantry renderer uses; the camera snaps with it so the
// first station frame and any later undock frame cannot show a stale view.
inline bool dock(State &state) {
    if (state.docked >= 0 || state.dockCooldown > 0)
        return false;

    uint8_t settlementId = 0;
    if (!nearestDock(state, settlementId))
        return false;
    if (!slowEnoughToDock(state))
        return false;

    world::Settlement settlement;
    if (!world::settlementInfo(settlementId, settlement))
        return false;

    state.docked = static_cast<int8_t>(settlementId);
    state.dockCooldown = 0;
    state.x = settlement.pixel_x;
    state.y = settlement.pixel_y;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;

    service(state);
#if SH_FEATURE_JOBS
    // Resolve a contract ending at this station (if any), then restock offers.
    jobs::onDock(state, settlementId);
#endif
    snapCamera(state);
    return true;
}

// Undock leaves the boat at the station with no residual drift. Travel, air,
// and ballast resume on the next world step.
inline bool undock(State &state) {
    if (state.docked < 0)
        return false;

    state.docked = -1;
    state.dockCooldown = kUndockCooldown;
    state.xFrac = 0;
    state.yFrac = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;
    return true;
}

// World step plus auto-dock, in that order. Docked state pauses the step:
// travel time, air drain, and ballast valves do not advance while the station
// screen is showing. Returns true only on the frame that docked.
template <typename Reader> inline bool stepFlight(State &state, const Input &input, Reader &reader, const world::TileCache *cache = nullptr) {
    if (state.docked >= 0)
        return false;

    if (state.dockCooldown > 0)
        --state.dockCooldown;

    game::stepWorld(state, input, reader, cache);
    if (state.dockCooldown > 0)
        return false;
    return dock(state);
}

// A confirms the selected row. Offer rows accept the contract, ABANDON clears
// it, SERVICE refills/repairs, and UNDOCK leaves the station.
inline Action activate(uint8_t selected, State &state) {
    if (state.docked < 0 || selected >= rowCount(state))
        return kActionNone;

    switch (rowKind(selected, state)) {
    case kRowService:
        service(state);
        return kActionServiced;
    case kRowUndock:
        return undock(state) ? kActionUndocked : kActionNone;
#if SH_FEATURE_JOBS
    case kRowOffer:
        return jobs::accept(state, selected) ? kActionAccepted : kActionNone;
    case kRowAbandon:
        return jobs::abandon(state) ? kActionAbandoned : kActionNone;
    case kRowJob:
        return kActionNone;
#endif
    default:
        return kActionNone;
    }
}

}   // namespace station

#endif   // SH_FEATURE_STATIONS
