#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_STATIONS

#include "game.hpp"
#include "pilot.hpp"
#include "world.hpp"

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

// Row extension point for M3 jobs. Offers will occupy the leading rows; the
// service and undock rows stay last so their actions never move.
constexpr uint8_t kOfferRows = 0;
constexpr uint8_t kServiceRow = kOfferRows;
constexpr uint8_t kUndockRow = static_cast<uint8_t>(kOfferRows + 1);
constexpr uint8_t kStationRowCount = static_cast<uint8_t>(kOfferRows + 2);
constexpr uint8_t kFirstRow = 0;

enum RowKind : uint8_t {
    kRowOffer,
    kRowService,
    kRowUndock
};

enum Action : uint8_t {
    kActionNone,
    kActionServiced,
    kActionUndocked
};

inline RowKind rowKind(uint8_t row) {
    if (row < kOfferRows)
        return kRowOffer;
    if (row == kServiceRow)
        return kRowService;
    return kRowUndock;
}

// Selection wraps so a two-row board never dead-ends. Both functions assume
// the caller keeps the selected row in range; the app initializes to
// kFirstRow and only moves through these.
inline uint8_t selectPrevious(uint8_t selected) {
    return selected == 0 ? static_cast<uint8_t>(kStationRowCount - 1) : static_cast<uint8_t>(selected - 1);
}

inline uint8_t selectNext(uint8_t selected) {
    return static_cast<uint8_t>((selected + 1) % kStationRowCount);
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

// A confirms the selected row. Offers are inert until M3 jobs adds them.
inline Action activate(uint8_t selected, State &state) {
    if (state.docked < 0 || selected >= kStationRowCount)
        return kActionNone;

    switch (rowKind(selected)) {
    case kRowService:
        service(state);
        return kActionServiced;
    case kRowUndock:
        return undock(state) ? kActionUndocked : kActionNone;
    default:
        return kActionNone;
    }
}

}   // namespace station

#endif   // SH_FEATURE_STATIONS
