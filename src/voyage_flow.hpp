#pragma once



#include <stdint.h>

#include "campaign.hpp"
#include "game.hpp"
#include "jobs.hpp"
#include "pilot.hpp"
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

// RAM-only record of the last successfully written or loaded checkpoint,
// identified by the exact CRC the save transaction encodes. When the cache is
// valid and the live state produces the same CRC, the cart already holds this
// byte-identical checkpoint and the save transaction can skip its 4 KiB erase
// and page programs. Every serialized field (including voyage ticks and the
// full chart) feeds the CRC, so there is no partial/field-level comparison. The
// cache is never persisted and is invalidated on reset or any failed write.
struct CheckpointCache {
    uint16_t crc;
    bool valid;

    CheckpointCache() : crc(0), valid(false) {}

    void clear() {
        crc = 0;
        valid = false;
    }

    void note(const State &state, const chart::State &chart) {
        crc = voyage_save::checkpointCrc(state, chart);
        valid = true;
    }

    bool matches(const State &state, const chart::State &chart) const {
        return valid && crc == voyage_save::checkpointCrc(state, chart);
    }
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

// Cached load: a valid checkpoint becomes the new baseline for redundant-write
// skipping; anything else invalidates the cache.
template <typename Storage>
inline voyage_save::LoadResult loadCheckpoint(State &state
                                              ,
                                              chart::State &chart
                                              ,
                                              Storage &storage
                                              ,
                                              CheckpointCache &cache) {
    const voyage_save::LoadResult result = voyage_save::load(state
                                                             ,
                                                             chart
                                                             ,
                                                             storage);
    if (result == voyage_save::kLoaded)
        cache.note(state, chart);
    else
        cache.clear();
    return result;
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

// Cached save: report success without touching the cart when the last
// committed checkpoint already has this exact CRC. Only a real save updates the
// cache; a failure invalidates it so the next attempt always rewrites.
template <typename Storage>
inline voyage_save::SaveResult saveCheckpointIfChanged(const State &state
                                                       ,
                                                       const chart::State &chart
                                                       ,
                                                       Storage &storage
                                                       ,
                                                       CheckpointCache &cache) {
    if (cache.matches(state, chart))
        return voyage_save::kSaved;
    const voyage_save::SaveResult result = saveCheckpoint(state
                                                          ,
                                                          chart
                                                          ,
                                                          storage);
    if (result == voyage_save::kSaved)
        cache.note(state, chart);
    else
        cache.clear();
    return result;
}

// The committed checkpoint is the Tidewall berth, so Continue always restores
// a valid berth record. Only the live new voyage begins in free flight beside
// the home gantry; a failed write leaves the docked state for SaveFailed.
template <typename Storage>
inline voyage_save::SaveResult startNewVoyage(State &state
                                              ,
                                              chart::State &chart
                                              ,
                                              Storage &storage) {
    const voyage_save::SaveResult result = voyage_save::newVoyage(state
                                                                  ,
                                                                  chart
                                                                  ,
                                                                  storage);
    if (result == voyage_save::kSaved)
        game::resetWorld(state);
    return result;
}

// Cached new voyage: a new voyage always writes, but the cache then records the
// freshly committed spawn checkpoint. A failure invalidates it.
template <typename Storage>
inline voyage_save::SaveResult startNewVoyage(State &state
                                              ,
                                              chart::State &chart
                                              ,
                                              Storage &storage
                                              ,
                                              CheckpointCache &cache) {
    const voyage_save::SaveResult result = voyage_save::newVoyage(state
                                                                  ,
                                                                  chart
                                                                  ,
                                                                  storage);
    if (result == voyage_save::kSaved) {
        // Cache the docked checkpoint before the live state moves beside it.
        cache.note(state, chart);
        game::resetWorld(state);
    } else {
        cache.clear();
    }
    return result;
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

// Cached rescue: the load seeds the baseline, the rescue increment makes the
// commit differ, and the successful commit becomes the new baseline. A failed
// load or commit invalidates it.
template <typename Storage>
inline Recovery recoverCheckpoint(State &state
                                  ,
                                  chart::State &chart
                                  ,
                                  Storage &storage
                                  ,
                                  CheckpointCache &cache) {
    Recovery result;
    result.load = loadCheckpoint(state
                                 ,
                                 chart
                                 ,
                                 storage
                                 ,
                                 cache);
    result.save = voyage_save::kInvalidState;
    if (result.load == voyage_save::kLoaded) {
        if (state.rescues != 0xFFFFu)
            ++state.rescues;
        result.save = saveCheckpointIfChanged(state
                                              ,
                                              chart
                                              ,
                                              storage
                                              ,
                                              cache);
    }
    return result;
}

}   // namespace voyage_flow

