#pragma once

#include <stdint.h>

#include "game.hpp"
#include "pilot.hpp"
#include "world.hpp"
#include "jobs.hpp"
#include "campaign.hpp"

// Docking, station service, and station-row rules. Pure C++: no Arduino
// headers, no cart reads here, and no display access. The sketch owns the
// screen state machine; this header owns the state transition and its
// invariants.
//
// Auto-dock is the selected M3 input contract: a boat inside a settlement
// ring, at or below the dock speed after a world step, docks without spending
// a button. The capture radius is the visible gantry footprint; the dock
// speed threshold mirrors the browser.
namespace station {

using game::Input;
using game::airCapacity;
using game::kQ8One;
using game::neutralForNormalized;
using game::State;

// The gantry spans 13 px either side of its dock pixel. The capture radius
// matches that footprint with one pixel of approach margin; the generated
// world clearance is wider and only keeps the berth approach clear.
constexpr int16_t kDockRadius = 14;
constexpr int32_t kDockRadiusSq = static_cast<int32_t>(kDockRadius) * kDockRadius;

// Browser CFG.dockSpeed: 16 px/s. Q8.8 keeps the comparison integer.
constexpr int16_t kDockSpeedQ8 = 16 * kQ8One;
constexpr int32_t kDockSpeedSq = static_cast<int32_t>(kDockSpeedQ8) * kDockSpeedQ8;

// Undocking arms the ring latch. Auto-dock stays disarmed until the boat has
// left every settlement ring, so the player can take as long as they need to
// thrust clear; re-entering after that exit docks normally.
constexpr uint8_t kUndockCooldown = 1;

// Station board shapes. WORK is the single access to restoration missions, so
// the no-job board posts no separate freight rows:
//
//   no job:       WORK UNDOCK          (service-only berth: UNDOCK)
//   active:       [summary] ABANDON WORK UNDOCK
//
// Only selectable rows participate in wrap-around; the summary row is inert.
// Acceptance auto-loads the contract hold, so there is no LOAD row.
constexpr uint8_t kWorkRow = 0;
constexpr uint8_t kUndockRow = 1;
constexpr uint8_t kStationRowCount = 2;
constexpr uint8_t kFirstRow = 0;

constexpr uint8_t kActiveSummaryRow = 0;
constexpr uint8_t kAbandonRow = 1;
constexpr uint8_t kActiveWorkRow = 2;
constexpr uint8_t kActiveUndockRow = 3;
constexpr uint8_t kActiveRowCount = 4;

enum RowKind : uint8_t {
    kRowWork,
    kRowUndock,
    kRowJob,
    kRowAbandon
};

enum Action : uint8_t {
    kActionNone,
    kActionUndocked,
    kActionWork,
    kActionAbandoned
};

inline bool jobActive(const State &state) {
    return game::hasActiveJob(state);
}

inline bool serviceBerth(const State &state) {
    return state.docked >= 0 && !campaign::freightBerth(static_cast<uint8_t>(state.docked));
}

inline uint8_t rowCount(const State &state) {
    if (serviceBerth(state))
        return 1;   // service-only berth: automatic service, B/A undocks
    if (jobActive(state))
        return kActiveRowCount;
    return kStationRowCount;
}

inline RowKind rowKind(uint8_t row, const State &state) {
    if (serviceBerth(state))
        return kRowUndock;
    if (jobActive(state)) {
        if (row == kActiveSummaryRow)
            return kRowJob;
        if (row == kAbandonRow)
            return kRowAbandon;
        if (row == kActiveWorkRow)
            return kRowWork;
        return kRowUndock;
    }
    if (row == kWorkRow)
        return kRowWork;
    return kRowUndock;
}

// First selectable row for a freshly docked boat. The no-job board starts on
// WORK; an active contract starts on the safe UNDOCK row, never on ABANDON or
// WORK. The sketch intercepts ABANDON for its own confirmation.
inline uint8_t initialRow(const State &state) {
    if (serviceBerth(state))
        return kFirstRow;
    if (jobActive(state))
        return kActiveUndockRow;
    return kFirstRow;
}

// First row in the selection cycle. The active board keeps its inert summary
// row out of the cycle but still allows UNDOCK to be the initial selection, so
// ABANDON stays reachable but never accidental.
inline uint8_t firstSelectable(const State &state) {
    if (serviceBerth(state))
        return kFirstRow;
    if (jobActive(state))
        return kAbandonRow;
    return kFirstRow;
}

inline uint8_t selectableCount(const State &state) {
    if (serviceBerth(state))
        return 1;
    if (jobActive(state))
        return static_cast<uint8_t>(kActiveRowCount - kAbandonRow);
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

    const uint8_t berthCount = world::berthCount();
    for (uint8_t id = 0; id < berthCount; ++id) {
        world::Berth berth;
        if (!world::berthInfo(id, berth) || !campaign::berthAvailable(state, id))
            continue;
        const int16_t pixelX = berth.pixel_x;
        const int16_t pixelY = berth.pixel_y;

        const int32_t dx = static_cast<int32_t>(pixelX) - state.x;
        const int32_t dy = static_cast<int32_t>(pixelY) - state.y;
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

// Full tank and neutral trim. Carries are cleared so a held valve cannot move
// the boat on the first frame after undocking.
inline void service(State &state) {
    state.air = airCapacity(state);
    state.ballast = static_cast<uint8_t>(neutralForNormalized(state.cargo));
    state.ballastCarry = 0;
    state.airBlowCarry = 0;
    state.airDrainTicks = 0;
}

// Dock at the nearest ring without moving the hull. Stopping in place keeps
// the approach continuous, so returning from the station cannot warp the boat
// to the gantry centre.
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

    state.docked = static_cast<int8_t>(settlementId);
    state.dockCooldown = 0;
    state.vxQ8 = 0;
    state.vyQ8 = 0;

    service(state);
    // Freight berths resolve a contract ending here. Service-only berths
    // refill and save but never trade.
    if (campaign::freightBerth(settlementId))
        jobs::onDock(state, settlementId);
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

// A confirms the selected row. ABANDON clears the contract, WORK opens the
// restoration board (the sketch owns it), and UNDOCK leaves the station.
inline Action activate(uint8_t selected, State &state) {
    if (state.docked < 0 || selected >= rowCount(state))
        return kActionNone;

    switch (rowKind(selected, state)) {
    case kRowWork:
        return kActionWork;
    case kRowUndock:
        return undock(state) ? kActionUndocked : kActionNone;
    case kRowAbandon:
        return jobs::abandon(state) ? kActionAbandoned : kActionNone;
    case kRowJob:
        return kActionNone;
    default:
        return kActionNone;
    }
}

}   // namespace station

