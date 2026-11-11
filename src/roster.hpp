#pragma once

#include <stdint.h>


// Hull roster: the shipped fleet keeps one playable hull, HAULER.
//
// The frozen schema-8 save still carries a hull-class byte at scalar offset 1.
// Retired Minnow (0) and Leviathan (2) ids remain valid inputs only for
// checkpoint compatibility: loading normalizes them to HAULER, while unknown
// ids above 2 are rejected. No class table, selection helper or display name
// is compiled into the fixed build.
namespace roster {

enum HullClass : uint8_t {
    kHauler = 1,
};

constexpr HullClass kDefault = kHauler;
constexpr uint8_t kClassCount = 1;

// Highest id the frozen schema ever wrote: 0 = retired small hull,
// 1 = current HAULER, 2 = retired large hull.
constexpr uint8_t kCompatibleIdMax = 2;

constexpr bool isValidId(uint8_t value) {
    return value <= kCompatibleIdMax;
}

// Every accepted id maps to the only playable hull. Callers normalize before
// using the state hull byte, so legacy Minnow/Leviathan voyages keep their
// progress without retaining an unreachable active class.
constexpr HullClass normalize(uint8_t) {
    return kHauler;
}

#if !defined(__AVR__)
// Host-only name keeps the pure tests readable. On AVR the baked title panel
// owns the label, so no hull name is ever placed in SRAM.
constexpr const char *nameOf(HullClass) {
    return "HAULER";
}
#endif

}   // namespace roster
