#pragma once

#include <stdint.h>

#include <ArduboyFX.h>

#include "fxdata.h"

// Device text boundary. Labels live in the external FX font/string sections and
// are read during composition, before the single display transaction. The
// external font keeps the Arduboy2 6x8 cell so existing layout constants and
// 7 px row pitch stay valid; dcmInvert draws black glyphs on a white selection
// bar without an opaque cell background.
namespace text {

constexpr uint8_t kCellWidth = 6;
constexpr uint8_t kCellHeight = 8;

inline void begin() {
    FX::setFont(font_6x8, dcmWhite);
}

inline void normal() {
    FX::setFontMode(dcmWhite);
}

inline void inverted() {
    FX::setFontMode(dcmInvert);
}

inline void at(int16_t x, int16_t y) {
    FX::setCursor(x, y);
}

inline void print(uint24_t address) {
    FX::drawString(address);
}

inline void number(uint16_t value) {
    FX::drawNumber(value);
}

inline void character(uint8_t value) {
    FX::drawChar(value);
}

}   // namespace text
