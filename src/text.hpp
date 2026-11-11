#pragma once

#include <stdint.h>

#include <ArduboyFX.h>

#include "cart_service.hpp"
#include "fxdata.h"
#include "lib/Blit.hpp"

// Device text boundary. The shipping game bakes every runtime label into
// 128x8 plus-mask FX stamps, so production issues no string, number or
// character draw calls and the external font renderer is garbage-collected
// from release flash. The legacy begin/print/number/character API and the
// `font_6x8` FX font stay for host/device reference tests and future use.
//
// `at()` still positions the legacy cursor and also records the stamp origin.
// `stamp()` selects the white or black frame from the last normal/inverted
// mode; `stampFrame()` takes an explicit sheet frame and screen position.
// `Blit::draw` skips the generated two-byte sprite header and reads the
// headerless page-major plus-mask bytes.
namespace text {

constexpr uint8_t kCellWidth = 6;
constexpr uint8_t kCellHeight = 8;
constexpr uint8_t kStampWidth = 128;
constexpr uint8_t kStampHeight = 8;

inline int16_t &stampX() {
    static int16_t x = 0;
    return x;
}

inline int16_t &stampY() {
    static int16_t y = 0;
    return y;
}

inline bool &invertedFlag() {
    static bool inverted = false;
    return inverted;
}

inline void begin() {
    FX::setFont(font_6x8, dcmWhite);
}

inline void normal() {
    FX::setFontMode(dcmWhite);
    invertedFlag() = false;
}

inline void inverted() {
    FX::setFontMode(dcmInvert);
    invertedFlag() = true;
}

inline void at(int16_t x, int16_t y) {
    FX::setCursor(x, y);
    stampX() = x;
    stampY() = y;
}

// Draw one baked 128x8 row at the current cursor. Frame 0 is white glyphs and
// frame 1 is the black/inverted selection variant.
inline void stampFrame(uint24_t image, uint8_t frame, int16_t x, int16_t y) {
    cartService();
    Blit::draw(x, y, kStampWidth, kStampHeight, image + 2, frame, Blit::PLUSMASK);
}

inline void stamp(uint24_t image) {
    stampFrame(image, invertedFlag() ? 1 : 0, stampX(), stampY());
}

inline void print(uint24_t address) {
    cartService();
    FX::drawString(address);
}

inline void number(uint16_t value) {
    FX::drawNumber(value);
}

inline void character(uint8_t value) {
    FX::drawChar(value);
}

}   // namespace text
