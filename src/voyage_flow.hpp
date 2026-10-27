#pragma once

#include "features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include <stdint.h>

#include "campaign.hpp"
#include "game.hpp"
#include "jobs.hpp"
#include "world.hpp"
#include "voyage_save.hpp"
#if SH_FEATURE_FOG_CHART
#include "chart.hpp"
#endif

// Checkpoint application flow (sub-hauler-7px.4). The storage task owns the
// slot format and the atomic transaction; this header owns the small amount of
// policy between storage and the sketch:
//
//   * distinguish a blank EEPROM from an incompatible or corrupt voyage so the
//     title can warn before NEW VOYAGE;
//   * regenerate the deterministic offer board after a load without moving the
//     saved cursor;
//   * increment the rescue counter and commit the recovered berth checkpoint.
//
// Pure C++: Storage supplies read/update and the host tests inject a fake
// EEPROM. No Arduino headers and no display access.
namespace voyage_flow {

using game::State;

enum BootState : uint8_t {
    kBootBlank,
    kBootValid,
    kBootCorrupt,
    kBootIncompatible,
    kBootUnreadable,
};

struct BootStatus {
    BootState state;
    bool hasValid;
    bool hasCorrupt;
    bool hasIncompatible;

    // True when NEW VOYAGE would replace data and therefore needs the explicit
    // replacement confirmation. A blank EEPROM starts immediately.
    bool needsReplaceConfirm() const {
        return hasValid || hasCorrupt || hasIncompatible;
    }
};

template <typename Storage> inline BootStatus inspectBoot(Storage &storage) {
    const voyage_save::Inventory inventory = voyage_save::inspect(storage);
    BootStatus status;
    status.hasValid = inventory.validCount != 0;
    status.hasCorrupt = inventory.anyCorrupt || (inventory.presentCount != 0 && inventory.validCount == 0 && !inventory.anyIncompatible);
    status.hasIncompatible = inventory.anyIncompatible;

    if (status.hasValid)
        status.state = kBootValid;
    else if (status.hasIncompatible && status.hasCorrupt)
        status.state = kBootUnreadable;
    else if (status.hasIncompatible)
        status.state = kBootIncompatible;
    else if (status.hasCorrupt)
        status.state = kBootCorrupt;
    else
        status.state = kBootBlank;
    return status;
}

// A checkpoint is always at a berth. Freight berths regenerate their two
// deterministic rows from the saved cursor; service berths (future generated
// geography) have no freight board. An active job keeps its summary shape.
inline void prepareBoard(State &state) {
    if (state.docked < 0) {
        state.offerCount = 0;
        return;
    }
    if (!game::hasActiveJob(state) && campaign::freightBerth(static_cast<uint8_t>(state.docked)))
        jobs::refreshOffers(state, static_cast<uint8_t>(state.docked));
    else
        state.offerCount = 0;
}

template <typename Storage>
inline voyage_save::LoadResult loadCheckpoint(State &state
#if SH_FEATURE_FOG_CHART
                                              ,
                                              chart::State &chart
#endif
                                              ,
                                              Storage &storage) {
    const voyage_save::LoadResult result = voyage_save::load(state
#if SH_FEATURE_FOG_CHART
                                                             ,
                                                             chart
#endif
                                                             ,
                                                             storage);
    if (result == voyage_save::kLoaded)
        prepareBoard(state);
    return result;
}

template <typename Storage>
inline voyage_save::SaveResult saveCheckpoint(const State &state
#if SH_FEATURE_FOG_CHART
                                              ,
                                              const chart::State &chart
#endif
                                              ,
                                              Storage &storage) {
    return voyage_save::save(state
#if SH_FEATURE_FOG_CHART
                             ,
                             chart
#endif
                             ,
                             storage);
}

template <typename Storage>
inline voyage_save::SaveResult startNewVoyage(State &state
#if SH_FEATURE_FOG_CHART
                                              ,
                                              chart::State &chart
#endif
                                              ,
                                              voyage_save::LegacyChart legacy, Storage &storage) {
    const voyage_save::SaveResult result = voyage_save::newVoyage(state
#if SH_FEATURE_FOG_CHART
                                                                  ,
                                                                  chart
#endif
                                                                  ,
                                                                  legacy, storage);
    // The board is derived state, not serialized. Generate it even when the
    // initial write fails so CONTINUE UNSAVED still reaches a valid station
    // with the same deterministic offers.
    prepareBoard(state);
    return result;
}

struct Recovery {
    voyage_save::LoadResult load;
    voyage_save::SaveResult save;
};

// Free rescue/wreck recovery: restore the complete newest checkpoint, count the
// request once, and commit the new counter before the caller resumes. Credits
// and cargo come from the checkpoint itself, so a rescue never pays twice and
// never strands a zero-air boat. If the save fails the recovered state remains
// live and the caller offers RETRY / CONTINUE UNSAVED.
template <typename Storage>
inline Recovery recoverCheckpoint(State &state
#if SH_FEATURE_FOG_CHART
                                  ,
                                  chart::State &chart
#endif
                                  ,
                                  Storage &storage) {
    Recovery result;
    result.load = loadCheckpoint(state
#if SH_FEATURE_FOG_CHART
                                 ,
                                 chart
#endif
                                 ,
                                 storage);
    result.save = voyage_save::kInvalidState;
    if (result.load == voyage_save::kLoaded) {
        if (state.rescues != 0xFFFFu)
            ++state.rescues;
        result.save = saveCheckpoint(state
#if SH_FEATURE_FOG_CHART
                                     ,
                                     chart
#endif
                                     ,
                                     storage);
    }
    return result;
}

}   // namespace voyage_flow

#endif   // SH_FEATURE_SAVE_VOYAGE
