#pragma once

#include <stdint.h>

#include "cargo.hpp"
#include "features.hpp"

#if SH_FEATURE_CARGO_LOAD

// Fixed cargo screen state. One HAULER arrangement is loaded through the same
// Reader abstraction as cargo::loadArrangement, so host tests inject a bank and
// the device supplies cargo::FxCargoReader. No heap, String, or float.
namespace cargo_screen {

struct State {
    cargo::CargoPuzzle puzzle;
    uint8_t hull;
    uint8_t arrangement;
    uint8_t selected;
    bool loaded;
};

static_assert(sizeof(State) <= sizeof(cargo::CargoPuzzle) + 8, "screen state stays within one puzzle plus alignment");

inline void reset(State &state) {
    state.puzzle = cargo::CargoPuzzle{};
    state.hull = cargo::HULL_HAULER;
    state.arrangement = 0;
    state.selected = 0;
    state.loaded = false;
}

// Load HAULER arrangement 0. The caller supplies the Reader so this header
// stays free of Arduino/FX headers.
template <typename Reader> inline bool load(State &state, Reader &reader) {
    reset(state);
    if (!cargo::loadArrangement(state.puzzle, reader, cargo::HULL_HAULER, 0))
        return false;
    state.loaded = true;
    return true;
}

}   // namespace cargo_screen

#endif   // SH_FEATURE_CARGO_LOAD
