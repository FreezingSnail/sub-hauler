#pragma once

#include <stdint.h>

#include "campaign.hpp"
#include "game.hpp"
#include "pilot.hpp"
#include "voyage_save.hpp"
#include "chart.hpp"

// Checkpoint application flow. The storage task owns the raw slot format and
// the save transaction; this header owns the small amount of policy between
// storage and the sketch:
//
//   * distinguish an erased cart save partition from a written one;
//   * restore a checkpoint on Continue or rescue.
//
// Rescue is load-only: it never rewrites the checkpoint.
//
// Pure C++: Storage supplies cart save reads and page/erase commands and the
// host tests inject a NOR-flash fake. No Arduino headers and no display access.
namespace voyage_flow {

using game::State;

// One value, used directly as the title frame selector: blank is 0, written is
// 1. There is no duplicate validity flag.
enum BootState : uint8_t {
    kBootBlank = 0,
    kBootWritten = 1,
};

template <typename Storage> inline BootState inspectBoot(Storage &storage) {
    return voyage_save::hasSave(storage) ? kBootWritten : kBootBlank;
}

template <typename Storage> inline voyage_save::LoadResult loadCheckpoint(State &state, chart::State &chart, Storage &storage) {
    return voyage_save::load(state, chart, storage);
}

template <typename Storage> inline void saveCheckpoint(const State &state, const chart::State &chart, Storage &storage) {
    voyage_save::save(state, chart, storage);
}

// The committed checkpoint is the Tidewall berth, so Continue always restores
// a valid berth record. Only the live new voyage begins in free flight beside
// the home gantry.
template <typename Storage> inline void startNewVoyage(State &state, chart::State &chart, Storage &storage) {
    voyage_save::newVoyage(state, chart, storage);
    game::resetWorld(state);
}

// Free rescue recovery: restore the newest checkpoint without writing it.
template <typename Storage> inline voyage_save::LoadResult recoverCheckpoint(State &state, chart::State &chart, Storage &storage) {
    return loadCheckpoint(state, chart, storage);
}

}   // namespace voyage_flow
