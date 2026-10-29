#pragma once

#include <ArduboyFX.h>

#include "cart_service.hpp"
#include "fxdata.h"
#include "world.hpp"

// Device-side Reader for the baked world. Collision queries use single-byte
// reads; chart and native terrain composition use contiguous bursts.
// All transactions belong in update/render, never during FX::display.
namespace world {

class FxTables {
  public:
    uint8_t tileByte(uint16_t index) {
        cartService();
        FX::seekData(static_cast<uint24_t>(explorationTiles + index));
        const uint8_t value = FX::readPendingUInt8();
        FX::readEnd();
        return value;
    }

    uint8_t chartByte(uint16_t index) {
        FX::seekData(static_cast<uint24_t>(explorationChart + index));
        const uint8_t value = FX::readPendingUInt8();
        FX::readEnd();
        return value;
    }

    void chartBytes(uint16_t index, uint8_t *buffer, uint8_t length) {
        // The chart composes eight short bursts while music continues. Refill
        // the stream before each cart transaction so map rendering cannot
        // stretch the audio service gap.
        cartService();
        FX::readDataBytes(static_cast<uint24_t>(explorationChart + index), buffer, length);
    }

    // Terrain is a bounded burst set: eight 128-byte page strips when the
    // camera Y is page-aligned, or the same eight plus four 32-byte tail strips for
    // a sub-page Y shift. test_music measures that general path with the real
    // streaming player (gap remains well inside the 128-tick limit), so no
    // per-strip service call is added to the hot render loop.
    void terrainBytes(uint32_t index, uint8_t *buffer, uint16_t length) {
        FX::readDataBytes(static_cast<uint24_t>(explorationTerrain + index), buffer, length);
    }
};

}   // namespace world
