#pragma once

#include <ArduboyFX.h>

#include "fxdata.h"
#include "world.hpp"

// Device-side Reader for the baked world tables. Every byte access performs one
// FX::seekData/readPendingUInt8/readEnd transaction, so callers must stay in the
// update/render phase and never call this during FX::display.
namespace world {

class FxTables {
  public:
    uint8_t tileByte(uint16_t index) {
        FX::seekData(static_cast<uint24_t>(worldTiles + index));
        const uint8_t value = FX::readPendingUInt8();
        FX::readEnd();
        return value;
    }

    uint8_t chartByte(uint16_t index) {
        FX::seekData(static_cast<uint24_t>(worldChart + index));
        const uint8_t value = FX::readPendingUInt8();
        FX::readEnd();
        return value;
    }
};

}   // namespace world
