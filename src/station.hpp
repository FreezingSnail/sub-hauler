#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_STATIONS

#include "damage.hpp"
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
using game::airCapacity;
using game::kQ8One;
using game::neutralFor;
using game::snapCamera;
using game::State;
#if SH_FEATURE_HULL_DAMAGE
using game::hullCapacity;
#endif

// The generated dock ring is the single source of truth for both the drawn
// gantry and the docking check.
constexpr int16_t kDockRadius = world::DOCK_CLEARANCE_PX;
constexpr int32_t kDockRadiusSq = static_cast<int32_t>(kDockRadius) * kDockRadius;

// Browser CFG.dockSpeed: 16 px/s. Q8.8 keeps the comparison integer.
constexpr int16_t kDockSpeedQ8 = 16 * kQ8One;
constexpr int32_t kDockSpeedSq = static_cast<int32_t>(kDockSpeedQ8) * kDockSpeedQ8;

// Undocking arms the ring latch. Auto-dock stays disarmed until the boat has
// left every settlement ring, so the player can take as long as they need to
// thrust clear; re-entering after that exit docks normally.
constexpr uint8_t kUndockCooldown = 1;

// Row extension point for M3 jobs and the M7 shop. Offers occupy the leading
// rows; SERVICE and the shop follow, and UNDOCK stays last so its action never
// moves when jobs or the shop are compiled out. With an active contract the
// board instead shows a summary row followed by the abandon/shop/undock
// actions, so the board shapes are:
//
//   no job:       [offer 0] [offer 1] SERVICE SHOP UNDOCK
//   active:       [summary] ABANDON SHOP UNDOCK
//
// A SHOP=0 build drops the SHOP row and leaves the M3 shapes untouched. Only
// selectable rows participate in wrap-around; the summary row is inert.
// Acceptance auto-loads the contract hold, so there is no LOAD CARGO row.
#if SH_FEATURE_JOBS
constexpr uint8_t kOfferRows = game::kJobOffers;
#else
constexpr uint8_t kOfferRows = 0;
#endif
constexpr uint8_t kServiceRow = kOfferRows;
#if SH_FEATURE_SHOP
constexpr uint8_t kShopRow = static_cast<uint8_t>(kOfferRows + 1);
constexpr uint8_t kUndockRow = static_cast<uint8_t>(kOfferRows + 2);
constexpr uint8_t kStationRowCount = static_cast<uint8_t>(kOfferRows + 3);
#else
constexpr uint8_t kUndockRow = static_cast<uint8_t>(kOfferRows + 1);
constexpr uint8_t kStationRowCount = static_cast<uint8_t>(kOfferRows + 2);
#endif
constexpr uint8_t kFirstRow = 0;

#if SH_FEATURE_JOBS
constexpr uint8_t kActiveSummaryRow = 0;
constexpr uint8_t kAbandonRow = 1;
#if SH_FEATURE_SHOP
constexpr uint8_t kActiveShopRow = 2;
constexpr uint8_t kActiveUndockRow = 3;
constexpr uint8_t kActiveRowCount = 4;
#else
constexpr uint8_t kActiveUndockRow = 2;
constexpr uint8_t kActiveRowCount = 3;
#endif
#endif

enum RowKind : uint8_t {
    kRowOffer,
    kRowService,
#if SH_FEATURE_SHOP
    kRowShop,
#endif
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
#if SH_FEATURE_SHOP
    ,
    kActionShop
#endif
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
#if SH_FEATURE_SHOP
        if (row == kActiveShopRow)
            return kRowShop;
#endif
        return kRowUndock;
    }
#endif
    if (row < kOfferRows)
        return kRowOffer;
    if (row == kServiceRow)
        return kRowService;
#if SH_FEATURE_SHOP
    if (row == kShopRow)
        return kRowShop;
#endif
    return kRowUndock;
}

// First selectable row for a freshly docked boat. No job starts on offer 0. An
// active campaign contract starts on the safe UNDOCK row, never on ABANDON; the
// sketch intercepts ABANDON for its own confirmation. The pre-campaign jobs
// contract keeps its historical ABANDON start.
inline uint8_t initialRow(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state)) {
#if SH_FEATURE_CAMPAIGN
        return kActiveUndockRow;
#else
        return kAbandonRow;
#endif
    }
#endif
    return kFirstRow;
}

// First row in the selection cycle. The active campaign board keeps its inert
// summary row out of the cycle but still allows UNDOCK to be the initial
// selection, so ABANDON stays reachable but never accidental.
inline uint8_t firstSelectable(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state))
        return kAbandonRow;
#else
    (void)state;
#endif
    return kFirstRow;
}

#if SH_FEATURE_SHOP
// The SHOP row index depends on the board shape. The sketch uses this when B
// backs out of the shop, so an active board returns to SHOP rather than the
// UNDOCK row that shares the no-job index.
inline uint8_t shopRow(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state))
        return kActiveShopRow;
#endif
    return kShopRow;
}
#endif

inline uint8_t selectableCount(const State &state) {
#if SH_FEATURE_JOBS
    if (jobActive(state)) {
#if SH_FEATURE_CAMPAIGN
        return static_cast<uint8_t>(kActiveRowCount - kAbandonRow);
#else
        return static_cast<uint8_t>(kActiveRowCount - initialRow(state));
#endif
    }
#endif
    (void)state;
    return kStationRowCount;
}

// Selection wraps across selectable rows only, so a two-row active board never
// dead-ends. The caller keeps the selected row in range via initialRow().
inline uint8_t selectPrevious(uint8_t selected, const State &state) {
    const uint8_t first = firstSelectable(state);
    const uint8_t last = static_cast<uint8_t>(first + selectableCount(state) - 1);
    if (selected <= first || selected > last)
        return last;
    return static_cast<uint8_t>(selected - 1);
}

inline uint8_t selectNext(uint8_t selected, const State &state) {
    const uint8_t first = firstSelectable(state);
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
    state.air = airCapacity(state);
#if SH_FEATURE_HULL_DAMAGE
    // Clamp on repair: a hull already above the current capacity is brought
    // back in range, including the run's hull-plate upgrades.
    damage::repairFull(state);
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
    if (state.docked >= 0)
        return false;

    uint8_t settlementId = 0;
    const bool inRing = nearestDock(state, settlementId);
    if (state.dockCooldown > 0) {
        if (inRing)
            return false;
        state.dockCooldown = 0;
    }
    if (!inRing || !slowEnoughToDock(state))
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
// screen is showing. Returns true only on the frame that docked. The ring
// latch is resolved inside dock() after the step, so an exit frame and a
// re-entry frame behave the same as a direct dock() call.
template <typename Reader> inline bool stepFlight(State &state, const Input &input, Reader &reader, const world::TileCache *cache = nullptr) {
    if (state.docked >= 0)
        return false;

    game::stepWorld(state, input, reader, cache);
    return dock(state);
}

// A confirms the selected row. Offer rows accept the contract (auto-loading
// the four contract units), ABANDON clears the contract, SHOP opens the shop
// screen (the sketch owns it), SERVICE refills/repairs, and UNDOCK leaves the
// station.
inline Action activate(uint8_t selected, State &state) {
    if (state.docked < 0 || selected >= rowCount(state))
        return kActionNone;

    switch (rowKind(selected, state)) {
    case kRowService:
        service(state);
        return kActionServiced;
#if SH_FEATURE_SHOP
    case kRowShop:
        return kActionShop;
#endif
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
