#pragma once



#include <stdint.h>

#include "campaign.hpp"
#include "game.hpp"
#include "jobs.hpp"
#include "world.hpp"
#include "voyage_save.hpp"
#include "chart.hpp"

// Checkpoint application flow (sub-hauler-7px.4). The storage task owns the
// slot format and the save transaction; this header owns the small amount of
// policy between storage and the sketch:
//
//   * distinguish a blank cart save partition from an incompatible or corrupt
//     voyage so the title can warn before NEW VOYAGE;
//   * increment the rescue counter and commit the recovered berth checkpoint.
//
// Pure C++: Storage supplies cart save reads and page/erase commands and the
// host tests inject a NOR-flash fake. No Arduino headers and no display access.
namespace voyage_flow {

using game::State;

enum BootState : uint8_t {
    kBootBlank,
    kBootValid,
    kBootCorrupt,
    kBootIncompatible,
};

struct BootStatus {
    BootState state;
    bool hasValid;
    bool hasCorrupt;
    bool hasIncompatible;

    bool needsReplaceConfirm() const { return hasValid; }
};

template <typename Storage> inline BootStatus inspectBoot(Storage &storage) {
    const voyage_save::Inventory inventory = voyage_save::inspect(storage);
    BootStatus status;
    status.hasValid = inventory.status == voyage_save::kSlotValid;
    status.hasCorrupt = inventory.status == voyage_save::kSlotCorrupt;
    status.hasIncompatible = inventory.status == voyage_save::kSlotIncompatible;
    if (status.hasValid)
        status.state = kBootValid;
    else if (status.hasIncompatible)
        status.state = kBootIncompatible;
    else if (status.hasCorrupt)
        status.state = kBootCorrupt;
    else
        status.state = kBootBlank;
    return status;
}

template <typename Storage>
inline voyage_save::LoadResult loadCheckpoint(State &state
                                              ,
                                              chart::State &chart
                                              ,
                                              Storage &storage) {
    return voyage_save::load(state
                             ,
                             chart
                             ,
                             storage);
}

template <typename Storage>
inline voyage_save::SaveResult saveCheckpoint(const State &state
                                              ,
                                              const chart::State &chart
                                              ,
                                              Storage &storage) {
    return voyage_save::save(state
                             ,
                             chart
                             ,
                             storage);
}

template <typename Storage>
inline voyage_save::SaveResult startNewVoyage(State &state
                                              ,
                                              chart::State &chart
                                              ,
                                              Storage &storage) {
    return voyage_save::newVoyage(state
                                  ,
                                  chart
                                  ,
                                  storage);
}

struct Recovery {
    voyage_save::LoadResult load;
    voyage_save::SaveResult save;
};

// Free rescue recovery: restore the complete newest checkpoint, count the
// request once, and commit the new counter before the caller resumes. Cargo and
// campaign progress come from the checkpoint itself, so a rescue never pays
// twice and never strands a zero-air boat. If the save fails the recovered state
// remains live and the caller offers RETRY / CONTINUE UNSAVED.
template <typename Storage>
inline Recovery recoverCheckpoint(State &state
                                  ,
                                  chart::State &chart
                                  ,
                                  Storage &storage) {
    Recovery result;
    result.load = loadCheckpoint(state
                                 ,
                                 chart
                                 ,
                                 storage);
    result.save = voyage_save::kInvalidState;
    if (result.load == voyage_save::kLoaded) {
        if (state.rescues != 0xFFFFu)
            ++state.rescues;
        result.save = saveCheckpoint(state
                                     ,
                                     chart
                                     ,
                                     storage);
    }
    return result;
}

}   // namespace voyage_flow

