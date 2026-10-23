#pragma once

#include <ArduboyFX.h>

#include "cargo.hpp"
#include "fxdata.h"

// Device-side Reader for the cargo arrangement bank. Call readCargo from the
// update/render phase only, never during FX::display. One seek/read/readEnd
// transaction loads a whole arrangement record.
namespace cargo {

class FxCargoReader {
  public:
    bool readCargo(uint16_t offset, uint8_t *dst, uint8_t count) {
        FX::seekData(static_cast<uint24_t>(cargoBank + offset));
        for (uint8_t i = 0; i < count; ++i)
            dst[i] = FX::readPendingUInt8();
        FX::readEnd();
        return true;
    }
};

}   // namespace cargo
