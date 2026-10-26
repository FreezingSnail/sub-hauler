#pragma once

// Optional persisted chart exploration (M9 SAVE_CHART).
//
// Storage choice: the AVR's internal 1,024 B EEPROM, not an FX cart partition.
// The explored set is only 256 B plus a four-byte header, which fits the
// internal EEPROM with no cart-page allocation, no FX layout change, and no
// SPI contention with the display. The platform note about FX_SAVE_PAGE applies
// only if a future save grows past the internal EEPROM or needs cart-sized
// state; chart save does not.
//
// Layout (little-endian byte order is irrelevant: every field is one byte):
//   0..1  magic 'S' 'H' (0x53 0x48)
//   2     current world layout version
//   3     CRC-8/ATM over the 256 explored bytes
//   4..   explored[world::CHART_BYTES] (256 B)
// Total 260 B. There is no migration: only the current world version is accepted,
// and blank/corrupt/bad-version/bad-checksum blocks are ignored as a whole.
//
// The save/load functions are templates over a Storage concept so the host
// tests use an in-memory fake and the device adapter below wraps the EEPROM
// library. The device adapter is the only place this header touches Arduino.
//
// Everything is gated by SH_FEATURE_SAVE_CHART (which features.hpp already
// requires FOG_CHART for). With the flag off this header contributes no code,
// state, strings, or includes beyond features.hpp.

#include "features.hpp"

#if SH_FEATURE_SAVE_CHART

#include <stdint.h>

#include "chart.hpp"
#include "world.hpp"

namespace chart_save {

constexpr uint8_t kMagic0 = 0x53;   // 'S'
constexpr uint8_t kMagic1 = 0x48;   // 'H'
constexpr uint8_t kVersion = world::LAYOUT_VERSION;
constexpr uint16_t kHeaderBytes = 4;
constexpr uint16_t kDataAddress = kHeaderBytes;
constexpr uint16_t kTotalBytes = kHeaderBytes + world::CHART_BYTES;

static_assert(kDataAddress == 4, "save header is four bytes");
static_assert(kTotalBytes == 260, "save is a 4 B header plus the 256 B chart");
static_assert(kTotalBytes <= 1024, "save block fits the ATmega32u4 EEPROM");

// CRC-8/ATM (polynomial 0x07, init 0). Streamed so neither load nor store needs
// a second 256 B buffer; that keeps the setup load's stack use flat.
inline uint8_t crc8Update(uint8_t crc, uint8_t value) {
    crc ^= value;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = static_cast<uint8_t>((crc & 0x80u) ? ((crc << 1) ^ 0x07u) : (crc << 1));
    return crc;
}

inline uint8_t checksum(const chart::State &state) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        crc = crc8Update(crc, state.explored[i]);
    return crc;
}

// Storage concept:
//   uint8_t read(uint16_t address);
//   void write(uint16_t address, uint8_t value);   // full write, if needed
//   void update(uint16_t address, uint8_t value);  // write only when changed
// save() uses update() so a close transition only costs the bytes that moved.

// Validate the whole block before touching the caller's chart. A blank,
// corrupt, wrong-version, or wrong-checksum block returns false and leaves the
// explored set exactly as it was, so the caller keeps its empty/initial map.
template <typename Storage> inline bool load(chart::State &state, Storage &storage) {
    if (storage.read(0) != kMagic0 || storage.read(1) != kMagic1)
        return false;
    if (storage.read(2) != kVersion)
        return false;

    const uint8_t storedChecksum = storage.read(3);
    uint8_t crc = 0;
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        crc = crc8Update(crc, storage.read(static_cast<uint16_t>(kDataAddress + i)));
    if (crc != storedChecksum)
        return false;

    // The header and checksum already proved the block intact; copy the exact
    // bit pattern now rather than replaying reveals.
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        state.explored[i] = storage.read(static_cast<uint16_t>(kDataAddress + i));
    return true;
}

// Write the exact 256 B mask plus the header. update() is the EEPROM.update
// contract: unchanged cells are not rewritten, so repeat saves are cheap.
template <typename Storage> inline void save(const chart::State &state, Storage &storage) {
    const uint8_t crc = checksum(state);
    storage.update(0, kMagic0);
    storage.update(1, kMagic1);
    storage.update(2, kVersion);
    storage.update(3, crc);
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        storage.update(static_cast<uint16_t>(kDataAddress + i), state.explored[i]);
}

#if defined(ARDUINO)
#include <EEPROM.h>

// Device adapter. Kept here so the sketch only names chart_save::save/load and
// the pure code above stays free of Arduino headers for the host tests.
struct EepromStorage {
    uint8_t read(uint16_t address) {
        return EEPROM.read(address);
    }

    void write(uint16_t address, uint8_t value) {
        EEPROM.write(address, value);
    }

    void update(uint16_t address, uint8_t value) {
        EEPROM.update(address, value);
    }
};
#endif

}   // namespace chart_save

#endif   // SH_FEATURE_SAVE_CHART
