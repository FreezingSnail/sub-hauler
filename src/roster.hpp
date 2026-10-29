#pragma once

#include <stdint.h>


// Hull roster: pure ids, order, and the compact class record.
//
// MINNOW/HAULER/LEVIATHAN keep the browser roster order and ids. HAULER is the
// default in every build.
// HAULER only: classCount() is 1, classAt() always returns HAULER, and the
// selection helpers are no-ops. No class table is emitted; spec() and
// nameLength() fold to immediates, and the display names live behind the device
// view's F() strings so single-hull builds never link MINNOW/LEVIATHAN text.
//
// The helper bodies stay C++11-compatible (single return, no switch) because
// the AVR toolchain still compiles this header as gnu++11.
namespace roster {

enum HullClass : uint8_t {
    kMinnow = 0,
    kHauler = 1,
    kLeviathan = 2,
};

constexpr HullClass kDefault = kHauler;

constexpr uint8_t kClassCount = 3;

// Reference browser dimensions and the playable pilot-art footprint. The
// collision hull is shared (26x8); pilotW/pilotH only describe the drawn body.
struct ClassSpec {
    uint8_t length;
    uint8_t height;
    uint8_t pilotW;
    uint8_t pilotH;
};

constexpr uint8_t length(HullClass hull) {
    return hull == kMinnow ? 64 : hull == kLeviathan ? 118 : 94;
}
constexpr uint8_t height(HullClass hull) {
    return hull == kMinnow ? 16 : hull == kLeviathan ? 30 : 22;
}
constexpr uint8_t pilotW(HullClass hull) {
    return hull == kMinnow ? 24 : 26;
}
constexpr uint8_t pilotH(HullClass hull) {
    return hull == kMinnow ? 7 : 8;
}

constexpr ClassSpec spec(HullClass hull) {
    return ClassSpec{length(hull), height(hull), pilotW(hull), pilotH(hull)};
}

// Display-name width in Arduboy font columns. The device view owns the actual
// PROGMEM string; this keeps title layout host-testable without class data.
constexpr uint8_t nameLength(HullClass hull) {
    return hull == kMinnow ? 7 : hull == kLeviathan ? 9 : 6;
}

// One-letter browser code: SMALL / MED / LARGE.
constexpr char code(HullClass hull) {
    return hull == kMinnow ? 'S' : hull == kLeviathan ? 'L' : 'M';
}

constexpr bool valid(HullClass hull) {
    return hull == kMinnow || hull == kHauler || hull == kLeviathan;
}
constexpr uint8_t indexOf(HullClass hull) {
    return valid(hull) ? static_cast<uint8_t>(hull) : static_cast<uint8_t>(kHauler);
}
constexpr HullClass classAt(uint8_t index) {
    return index == 0 ? kMinnow : index == 2 ? kLeviathan : kHauler;
}
// Wrap through the roster. A disabled class never becomes reachable.
constexpr HullClass next(HullClass hull) {
    return hull == kMinnow ? kHauler : hull == kLeviathan ? kMinnow : kLeviathan;
}
constexpr HullClass previous(HullClass hull) {
    return hull == kMinnow ? kLeviathan : hull == kLeviathan ? kHauler : kMinnow;
}

constexpr HullClass sanitize(HullClass hull) {
    return valid(hull) ? hull : kDefault;
}

#if !defined(__AVR__)
// Host-only names keep the pure tests readable. On AVR the device view supplies
// the same text through F(); this branch is compiled out, so no class name is
// ever placed in SRAM. Single-hull builds fold every query to HAULER.
constexpr const char *nameOf(HullClass hull) {
    return hull == kMinnow ? "MINNOW" : hull == kLeviathan ? "LEVIATHAN" : "HAULER";
}
#endif

}   // namespace roster
