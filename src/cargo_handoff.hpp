#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_CARGO_LOAD

#include "cargo_screen.hpp"
#include "game.hpp"
#include "jobs.hpp"
#include "station.hpp"

// Accepted-contract handoff between the cargo screen and the pilot.
//
// Gesture: A+B on the same input edge while nothing is held. A alone still
// picks/lifts/releases, B alone still rotates or undoes, and hold B still exits
// to the station, so the commit cannot collide with a manipulation. The screen
// draws the prompt only when commitReady() is true; an attempt below the
// required piece count is rejected with Message::Incomplete.
//
// On success the packed hold maps to the contract's pilot cargo units, trim is
// neutralized for that load, the loading state is cleared, and the boat
// auto-undocks. The contract stays active, so the normal travel/delivery path
// takes over on the next phase; B (hold) on the cargo screen instead returns to
// the station with the job still open.
namespace cargo_handoff {

enum Result : uint8_t {
    kNone,
    kCommitted,
    kIncomplete,
    kUnavailable
};

// A+B is the documented commit gesture. Kept pure so host tests cover it
// without an Arduino pad.
inline bool commitPressed(const cargo_screen::Input &input) {
    return input.a && input.b;
}

// The prompt and gesture are valid only with a loaded arrangement, nothing
// held, and every required piece installed. Bonus cargo is optional.
inline bool commitReady(const cargo_screen::State &screen) {
    return screen.loaded && screen.puzzle.held < 0 && cargo::requiredComplete(screen.puzzle);
}

// Try to close the loading screen and start travel. Below the requirement the
// screen keeps ownership and reports Message::Incomplete; the contract and the
// docked boat are untouched.
inline Result commit(game::State &state, cargo_screen::State &screen) {
    if (!screen.loaded || !game::hasActiveJob(state) || state.docked < 0)
        return kUnavailable;
    if (screen.puzzle.held >= 0)
        return kUnavailable;
    if (!cargo::requiredComplete(screen.puzzle)) {
        screen.message = cargo_screen::Message::Incomplete;
        return kIncomplete;
    }

    const uint8_t units = state.activeJob.crates;
    game::setCargo(state, units);
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
    if (!station::undock(state))
        return kUnavailable;

    // The screen is done with this arrangement; a later LOAD CARGO reloads it.
    screen.loaded = false;
    screen.message = cargo_screen::Message::None;
    return kCommitted;
}

}   // namespace cargo_handoff

#endif   // SH_FEATURE_CARGO_LOAD
