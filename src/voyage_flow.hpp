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
//   * distinguish an erased cart save partition from a written one;
//   * increment the rescue counter and commit the recovered berth checkpoint.
//
// Pure C++: Storage supplies cart save reads and page/erase commands and the
// host tests inject a NOR-flash fake. No Arduino headers and no display access.
namespace voyage_flow {

using game::State;

enum BootState : uint8_t {
    kBootBlank,
    kBootValid,
};

struct BootStatus {
    BootState state;
    bool hasValid;

    bool needsReplaceConfirm() const { return hasValid; }
};

// RAM-only record of the last checkpoint written or loaded,
// identified by the exact CRC the save transaction encodes. When the cache is
// valid and the live state produces the same CRC, the cart already holds this
// byte-identical checkpoint and the save transaction can skip its 4 KiB erase
// and page programs. Every serialized field (including voyage ticks and the
// full chart) feeds the CRC, so there is no partial/field-level comparison. The
// cache is never persisted and is invalidated on reset or a blank load.
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
    BootStatus status;
    status.hasValid = voyage_save::hasSave(storage);
    status.state = status.hasValid ? kBootValid : kBootBlank;
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

// Cached save: report success without touching the cart when the last written
// checkpoint already has this exact CRC. A real save updates the cache.
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
    saveCheckpoint(state
                   ,
                   chart
                   ,
                   storage);
    cache.note(state, chart);
    return voyage_save::kSaved;
}

// The committed checkpoint is the Tidewall berth, so Continue always restores
// a valid berth record. Only the live new voyage begins in free flight beside
// the home gantry; a failed write still leaves the docked state live.
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
    game::resetWorld(state);
    return result;
}

// Cached new voyage: a new voyage always writes, then records the freshly
// written spawn checkpoint before the live state moves beside it.
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
    cache.note(state, chart);
    game::resetWorld(state);
    return result;
}

// Free rescue recovery: restore the complete newest checkpoint, count the
// request once, and commit the new counter before the caller resumes. Cargo and
// campaign progress come from the checkpoint itself, so a rescue never pays
// twice and never strands a zero-air boat.
template <typename Storage>
inline voyage_save::LoadResult recoverCheckpoint(State &state
                                                 ,
                                                 chart::State &chart
                                                 ,
                                                 Storage &storage) {
    const voyage_save::LoadResult result = loadCheckpoint(state
                                                          ,
                                                          chart
                                                          ,
                                                          storage);
    if (result == voyage_save::kLoaded) {
        if (state.rescues != 0xFFFFu)
            ++state.rescues;
        saveCheckpoint(state
                       ,
                       chart
                       ,
                       storage);
    }
    return result;
}

// Cached rescue: the load seeds the baseline, the rescue increment makes the
// commit differ, and the written commit becomes the new baseline. A blank load
// invalidates it.
template <typename Storage>
inline voyage_save::LoadResult recoverCheckpoint(State &state
                                                 ,
                                                 chart::State &chart
                                                 ,
                                                 Storage &storage
                                                 ,
                                                 CheckpointCache &cache) {
    const voyage_save::LoadResult result = loadCheckpoint(state
                                                          ,
                                                          chart
                                                          ,
                                                          storage
                                                          ,
                                                          cache);
    if (result == voyage_save::kLoaded) {
        if (state.rescues != 0xFFFFu)
            ++state.rescues;
        saveCheckpointIfChanged(state
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

