#pragma once

#include <stdint.h>

#include "features.hpp"
#include "roster.hpp"

// Hull-selection title screen, 128x64. The layout is pure and host-checkable;
// draw() is device-only because it uses the external FX font and blits the
// baked pilot silhouette.
//
// Body:   NAME over the playable 26x8 silhouette and one short stat line
//         (reference length).
// Hint:   left/right cycles the roster, A confirms and starts the run.
//
// Only compiled when MULTI_HULL=1. Single-hull builds boot straight to Pilot
// and compile out the MINNOW/LEVIATHAN label and art calls.
#if SH_FEATURE_MULTI_HULL

#if defined(__AVR__)
#include "common.hpp"
#include "pilot_view.hpp"
#endif

namespace roster_view {

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kScreenH = 64;
constexpr uint8_t kCharW = 6;

constexpr uint8_t kNameY = 6;
constexpr uint8_t kArtLeft = 51;
constexpr uint8_t kArtTop = 18;
constexpr uint8_t kStatY = 34;
constexpr uint8_t kHintChars = 16;     // "D-PAD HULL  A GO"
constexpr uint8_t kHintY = 54;

constexpr uint8_t centerX(uint8_t chars) {
    return static_cast<uint8_t>((kScreenW - chars * kCharW) / 2);
}

constexpr uint8_t nameX(roster::HullClass hull) {
    return centerX(roster::nameLength(hull));
}

constexpr uint8_t digits(uint16_t value) {
    return value >= 100 ? 3 : value >= 10 ? 2 : 1;
}

// "LEN 94": 4 fixed glyphs plus the length digits.
constexpr uint8_t statChars(roster::HullClass hull) {
    return static_cast<uint8_t>(4 + digits(roster::length(hull)));
}

constexpr uint8_t statX(roster::HullClass hull) {
    return centerX(statChars(hull));
}

static_assert(kNameY + 7 <= kArtTop, "name clears the art");
static_assert(kArtTop + 8 <= kStatY, "art clears the stat line");
static_assert(kStatY + 7 <= kHintY, "stat line clears the hint");
static_assert(kHintY + 7 <= kScreenH, "hint stays on screen");

static_assert(kArtLeft >= 1 && kArtLeft + 26 <= kScreenW - 1, "art footprint is inside the screen");

#if defined(__AVR__)

inline uint24_t nameFor(roster::HullClass hull) {
    if (hull == roster::kMinnow)
        return txt_minnow;
    if (hull == roster::kLeviathan)
        return txt_leviathan;
    return txt_hauler;
}

inline void draw(uint8_t hullClass) {
    const roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(hullClass));

    text::normal();
    text::at(nameX(hull), kNameY);
    text::print(nameFor(hull));

    pilot_view::drawAt(Arduboy2Base::sBuffer, kArtLeft, kArtTop, false, false, hull);

    text::at(statX(hull), kStatY);
    text::print(txt_len);
    text::number(roster::length(hull));

    text::at(centerX(kHintChars), kHintY);
    text::print(txt_title_hint);
}

#endif   // __AVR__

}   // namespace roster_view

#endif   // SH_FEATURE_MULTI_HULL
