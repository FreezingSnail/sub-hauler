#pragma once

// Atomic two-slot voyage storage (sub-hauler-7px.3).
//
// The schema, slot addresses, commit protocol and legacy migration are frozen
// by docs/progression-implementation.md sections 3-7 and 10. This header is
// pure C++ so the host tests exercise the exact code the device compiles; the
// Storage adapter is the only place that touches EEPROM.
//
// Contract summary:
//   * two 320 B slots at addresses 16..335 (A) and 336..655 (B); system EEPROM
//     0..15 is never written;
//   * 16 B header + 303 B payload area + one commit byte at slot+319;
//   * CRC-16/CCITT-FALSE over header bytes 0..13 and exactly payload_length
//     payload bytes; the commit byte is outside the CRC;
//   * payload is encoded field by field, never as a raw C++ struct;
//   * load validates first and only then mutates the caller's state;
//   * a save targets the invalid slot or the older valid slot and commits
//     last, so an interrupted write cannot replace the other valid checkpoint;
//   * the legacy chart block (0..259) is read only before the first voyage
//     exists and only through the explicit new-voyage path.
//
// The feature is default-off. SH_FEATURE_SAVE_VOYAGE requires CAMPAIGN and is
// mutually exclusive with SH_FEATURE_SAVE_CHART (features.hpp enforces both).

#include "features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include <stdint.h>

#include "campaign.hpp"
#include "game.hpp"
#include "roster.hpp"
#include "world.hpp"
#if SH_FEATURE_FOG_CHART
#include "chart.hpp"
#endif
#if SH_FEATURE_EXPLORATION
#include "generated/exploration.hpp"
#endif

namespace voyage_save {

using game::State;

// ---------------------------------------------------------------------------
// Frozen layout
// ---------------------------------------------------------------------------

constexpr uint16_t kSystemBytes = 16;
constexpr uint16_t kSlotBytes = 320;
constexpr uint16_t kSlotAAddress = 16;
constexpr uint16_t kSlotBAddress = 336;
constexpr uint16_t kSlotEnd = kSlotBAddress + kSlotBytes;
constexpr uint8_t kSlotCount = 2;
constexpr uint16_t kHeaderBytes = 16;
constexpr uint16_t kPayloadCapacity = 303;
constexpr uint16_t kCommitOffset = 319;
constexpr uint8_t kCommitValid = 0x5A;
constexpr uint8_t kCommitInvalid = 0x00;

constexpr uint8_t kMagic0 = 0x53;   // 'S'
constexpr uint8_t kMagic1 = 0x48;   // 'H'
constexpr uint8_t kMagic2 = 0x56;   // 'V'
constexpr uint8_t kMagic3 = 0x59;   // 'Y'
constexpr uint8_t kSchemaVersion = 1;
constexpr uint8_t kCampaignContentVersion = 1;

constexpr uint16_t kMagicOffset = 0;
constexpr uint16_t kSchemaOffset = 4;
constexpr uint16_t kLayoutOffset = 5;
constexpr uint16_t kContentOffset = 6;
constexpr uint16_t kSignatureOffset = 7;
constexpr uint16_t kSequenceOffset = 8;
constexpr uint16_t kLengthOffset = 12;
constexpr uint16_t kCrcOffset = 14;
constexpr uint16_t kPayloadOffset = 16;

constexpr uint16_t kScalarBytes = 40;
#if SH_FEATURE_FOG_CHART
constexpr uint16_t kChartBytes = world::CHART_BYTES;
#else
constexpr uint16_t kChartBytes = 0;
#endif
constexpr uint16_t kPayloadBytes = kScalarBytes + kChartBytes;

static_assert(kSlotBytes == kHeaderBytes + kPayloadCapacity + 1, "slot is header + payload area + commit");
static_assert(kSlotBAddress == kSlotAAddress + kSlotBytes, "slots are adjacent");
static_assert(kSlotEnd == 656, "two slots end at EEPROM byte 655");
static_assert(kSlotEnd <= 1024, "voyage slots fit the ATmega32u4 EEPROM");
static_assert(kPayloadBytes <= kPayloadCapacity, "schema 1 payload fits its area");
static_assert(kScalarBytes == 40, "scalar payload is frozen at 40 bytes");

// Profile signature: one byte, schema 1. Chart presence is derived from bit 0;
// music has no saved field and is deliberately absent.
constexpr uint8_t kSignatureFogChart = 1u << 0;
constexpr uint8_t kSignatureSonar = 1u << 1;
constexpr uint8_t kSignatureMines = 1u << 2;
constexpr uint8_t kSignatureShop = 1u << 3;
constexpr uint8_t kSignatureHullDamage = 1u << 4;
constexpr uint8_t kSignatureMultiHull = 1u << 5;
constexpr uint8_t kSignatureExploration = 1u << 6;

constexpr uint8_t profileSignature() {
    return static_cast<uint8_t>((SH_FEATURE_FOG_CHART ? kSignatureFogChart : 0u) | (SH_FEATURE_SONAR ? kSignatureSonar : 0u) | (SH_FEATURE_MINES ? kSignatureMines : 0u) |
                                (SH_FEATURE_SHOP ? kSignatureShop : 0u) | (SH_FEATURE_HULL_DAMAGE ? kSignatureHullDamage : 0u) | (SH_FEATURE_MULTI_HULL ? kSignatureMultiHull : 0u) |
                                (SH_FEATURE_EXPLORATION ? kSignatureExploration : 0u));
}
constexpr uint8_t kProfileSignature = profileSignature();

// Result vocabulary shared by the storage transaction and the later title
// integration. A missing slot is not a corrupt save; the title needs to tell
// blank, incompatible and corrupt data apart before offering NEW VOYAGE.
enum SlotStatus : uint8_t {
    kSlotBlank,
    kSlotValid,
    kSlotCorrupt,
    kSlotIncompatible,
};

struct SlotInfo {
    SlotStatus status;
    uint32_t sequence;
};

struct Inventory {
    SlotInfo slot[kSlotCount];
    uint8_t validCount;
    uint8_t presentCount;   // non-blank slots, including incompatible/corrupt
    int8_t newestSlot;      // -1 when no valid slot exists
    uint32_t newestSequence;
    bool anyIncompatible;
    bool anyCorrupt;
};

enum LoadResult : uint8_t {
    kLoaded,
    kNoSave,
    kCorruptSave,
    kIncompatibleSave,
};

enum SaveResult : uint8_t {
    kSaved,
    kInvalidState,
    kWriteFailed,
};

// Explicit chart migration choice. Keeping legacy chart bits is only allowed
// before the first voyage slot exists.
enum LegacyChart : uint8_t {
    kDropLegacyChart,
    kKeepLegacyChart,
};

// Legacy standalone chart block (chart_save.hpp): 'S' 'H', version, CRC-8,
// then 256 explored bytes. It is read, never written, by this module.
constexpr uint16_t kLegacyMagic0 = 0x53;
constexpr uint16_t kLegacyMagic1 = 0x48;
constexpr uint16_t kLegacyVersion = world::LAYOUT_VERSION;
constexpr uint16_t kLegacyCrcOffset = 3;
constexpr uint16_t kLegacyDataOffset = 4;
constexpr uint16_t kLegacyTotalBytes = kLegacyDataOffset + world::CHART_BYTES;

static_assert(kLegacyTotalBytes == 260, "legacy chart block is 260 bytes");
static_assert(kLegacyTotalBytes <= kSlotAAddress + kSlotBytes, "legacy block overlaps only slot A, never slot B");

// ---------------------------------------------------------------------------
// Checksums and little-endian helpers
// ---------------------------------------------------------------------------

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR.
// The check vector "123456789" is 0x29B1 and is asserted by the host tests.
inline uint16_t crc16Update(uint16_t crc, uint8_t value) {
    crc ^= static_cast<uint16_t>(value) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = static_cast<uint16_t>((crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1));
    return crc;
}

inline uint16_t crc16Bytes(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; ++i)
        crc = crc16Update(crc, data[i]);
    return crc;
}

// CRC-8/ATM for the legacy chart block. Kept local so voyage_save does not
// depend on the standalone chart writer, which is compiled out in this build.
inline uint8_t crc8Update(uint8_t crc, uint8_t value) {
    crc ^= value;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = static_cast<uint8_t>((crc & 0x80u) ? ((crc << 1) ^ 0x07u) : (crc << 1));
    return crc;
}

inline uint16_t slotAddress(uint8_t index) {
    return index == 0 ? kSlotAAddress : kSlotBAddress;
}

// Wrap-aware sequence comparison. Equal values are not newer.
inline bool isNewer(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0;
}

// ---------------------------------------------------------------------------
// Scalar payload (40 B, fixed field order)
// ---------------------------------------------------------------------------

// Transient only: 40 bytes, never persisted as a struct and never chart-sized.
// Bytes are field-encoded in the frozen order below; the C++ struct layout is
// deliberately not serialized.
struct Scalar {
    uint8_t bytes[kScalarBytes];
};

enum ScalarOffset : uint8_t {
    kFieldBerth = 0,
    kFieldHullClass = 1,
    kFieldCredits = 2,
    kFieldAirLevel = 4,
    kFieldHullLevel = 5,
    kFieldSonarLevel = 6,
    kFieldTools = 7,
    kFieldPickups = 8,
    kFieldOpenGates = 9,
    kFieldSeenGates = 11,
    kFieldCampaign = 13,
    kFieldSurveys = 14,
    kFieldVisited = 15,
    kFieldJobKind = 16,
    kFieldJobMission = 17,
    kFieldJobFrom = 18,
    kFieldJobTo = 19,
    kFieldJobCrates = 20,
    kFieldJobReward = 21,
    kFieldJobDeadline = 22,
    kFieldJobElapsed = 24,
    kFieldOfferCursor = 26,
    kFieldBoardSeed = 28,
    kFieldMines = 30,
    kFieldVoyageTicks = 31,
    kFieldDelivered = 35,
    kFieldRescues = 37,
    kFieldEnding = 39,
};

static_assert(kFieldEnding + 1 == kScalarBytes, "scalar field table covers the payload");

inline uint8_t scalarU8(const Scalar &scalar, uint8_t offset) {
    return scalar.bytes[offset];
}

inline uint16_t scalarU16(const Scalar &scalar, uint8_t offset) {
    return static_cast<uint16_t>(scalar.bytes[offset] | (static_cast<uint16_t>(scalar.bytes[offset + 1]) << 8));
}

inline uint32_t scalarU32(const Scalar &scalar, uint8_t offset) {
    uint32_t value = scalar.bytes[offset];
    value |= static_cast<uint32_t>(scalar.bytes[offset + 1]) << 8;
    value |= static_cast<uint32_t>(scalar.bytes[offset + 2]) << 16;
    value |= static_cast<uint32_t>(scalar.bytes[offset + 3]) << 24;
    return value;
}

inline void scalarSetU16(Scalar &scalar, uint8_t offset, uint16_t value) {
    scalar.bytes[offset] = static_cast<uint8_t>(value);
    scalar.bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

inline void scalarSetU32(Scalar &scalar, uint8_t offset, uint32_t value) {
    scalar.bytes[offset] = static_cast<uint8_t>(value);
    scalar.bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
    scalar.bytes[offset + 2] = static_cast<uint8_t>(value >> 16);
    scalar.bytes[offset + 3] = static_cast<uint8_t>(value >> 24);
}

inline bool validHullClass(uint8_t value) {
    return roster::valid(static_cast<roster::HullClass>(value));
}

inline uint8_t toolsMask() {
#if SH_FEATURE_EXPLORATION
    return static_cast<uint8_t>(exploration_data::CUTTER | exploration_data::SEAL | exploration_data::THRUSTER | exploration_data::LENS);
#else
    return 0;
#endif
}

inline uint8_t pickupsMask() {
#if SH_FEATURE_EXPLORATION
    return static_cast<uint8_t>((1u << exploration_data::PICKUP_COUNT) - 1u);
#else
    return 0;
#endif
}

inline uint16_t gatesMask() {
#if SH_FEATURE_EXPLORATION
    return static_cast<uint16_t>((1u << exploration_data::GATE_COUNT) - 1u);
#else
    return 0;
#endif
}

inline uint8_t minesMask() {
#if SH_FEATURE_MINES
    return static_cast<uint8_t>((1u << world::MINE_COUNT) - 1u);
#else
    return 0;
#endif
}

inline bool levelsValid(const Scalar &scalar) {
#if SH_FEATURE_SHOP
    return scalarU8(scalar, kFieldAirLevel) <= 3 && scalarU8(scalar, kFieldHullLevel) <= 3 && scalarU8(scalar, kFieldSonarLevel) <= 3;
#else
    return scalarU8(scalar, kFieldAirLevel) == 0 && scalarU8(scalar, kFieldHullLevel) == 0 && scalarU8(scalar, kFieldSonarLevel) == 0;
#endif
}

inline bool scalarValid(const Scalar &scalar) {
    if (scalarU8(scalar, kFieldBerth) >= campaign::kBerthCount)
        return false;
    if (!validHullClass(scalarU8(scalar, kFieldHullClass)))
        return false;
    if (!levelsValid(scalar))
        return false;
    if ((scalarU8(scalar, kFieldTools) & static_cast<uint8_t>(~toolsMask())) != 0)
        return false;
    if ((scalarU8(scalar, kFieldPickups) & static_cast<uint8_t>(~pickupsMask())) != 0)
        return false;
    if ((scalarU16(scalar, kFieldOpenGates) & static_cast<uint16_t>(~gatesMask())) != 0)
        return false;
    if ((scalarU16(scalar, kFieldSeenGates) & static_cast<uint16_t>(~gatesMask())) != 0)
        return false;
    if ((scalarU8(scalar, kFieldMines) & static_cast<uint8_t>(~minesMask())) != 0)
        return false;
    if (scalarU16(scalar, kFieldBoardSeed) != 0)
        return false;   // schema 1 serial origin is frozen at zero

    // The campaign rules own milestone monotonicity, job-kind semantics,
    // route rewards, visits and the ending/HOME relationship. A checkpoint at
    // a service berth is valid only after its repair milestone.
    // Only the campaign-owned fields are needed by stateValid(); other probe
    // bytes are deliberately left untouched so the save path does not clear a
    // full State object just to re-validate a few serialized fields.
    State probe;
    probe.campaign = scalarU8(scalar, kFieldCampaign);
    probe.surveys = scalarU8(scalar, kFieldSurveys);
    probe.visited = scalarU8(scalar, kFieldVisited);
    probe.tools = scalarU8(scalar, kFieldTools);
    probe.ending = scalarU8(scalar, kFieldEnding);
    probe.activeJob.kind = scalarU8(scalar, kFieldJobKind);
    probe.activeJob.mission = scalarU8(scalar, kFieldJobMission);
    probe.activeJob.from = scalarU8(scalar, kFieldJobFrom);
    probe.activeJob.to = scalarU8(scalar, kFieldJobTo);
    probe.activeJob.crates = scalarU8(scalar, kFieldJobCrates);
    probe.activeJob.reward = scalarU8(scalar, kFieldJobReward);
    probe.activeJob.deadline = scalarU16(scalar, kFieldJobDeadline);
    probe.activeJob.elapsed = scalarU16(scalar, kFieldJobElapsed);
    if (!campaign::stateValid(probe))
        return false;
    if (!campaign::berthAvailable(probe, scalarU8(scalar, kFieldBerth)))
        return false;
    return true;
}

inline bool captureState(const State &state, Scalar &scalar) {
    if (state.docked < 0 || state.docked >= static_cast<int8_t>(campaign::kBerthCount))
        return false;

    scalar.bytes[kFieldBerth] = static_cast<uint8_t>(state.docked);
#if SH_FEATURE_MULTI_HULL
    scalar.bytes[kFieldHullClass] = state.hullClass;
#else
    scalar.bytes[kFieldHullClass] = static_cast<uint8_t>(roster::kDefault);
#endif
    scalarSetU16(scalar, kFieldCredits, state.credits);
#if SH_FEATURE_SHOP
    scalar.bytes[kFieldAirLevel] = state.airLevel;
    scalar.bytes[kFieldHullLevel] = state.hullLevel;
    scalar.bytes[kFieldSonarLevel] = state.sonarLevel;
#else
    scalar.bytes[kFieldAirLevel] = 0;
    scalar.bytes[kFieldHullLevel] = 0;
    scalar.bytes[kFieldSonarLevel] = 0;
#endif
    scalar.bytes[kFieldTools] = state.tools;
    scalar.bytes[kFieldPickups] = state.pickups;
    scalarSetU16(scalar, kFieldOpenGates, state.openGates);
    scalarSetU16(scalar, kFieldSeenGates, state.seenGates);
    scalar.bytes[kFieldCampaign] = state.campaign;
    scalar.bytes[kFieldSurveys] = state.surveys;
    scalar.bytes[kFieldVisited] = state.visited;
    scalar.bytes[kFieldJobKind] = state.activeJob.kind;
    scalar.bytes[kFieldJobMission] = state.activeJob.mission;
    scalar.bytes[kFieldJobFrom] = state.activeJob.from;
    scalar.bytes[kFieldJobTo] = state.activeJob.to;
    scalar.bytes[kFieldJobCrates] = state.activeJob.crates;
    scalar.bytes[kFieldJobReward] = state.activeJob.reward;
    scalarSetU16(scalar, kFieldJobDeadline, state.activeJob.deadline);
    scalarSetU16(scalar, kFieldJobElapsed, state.activeJob.elapsed);
    scalarSetU16(scalar, kFieldOfferCursor, state.offerCursor);
    scalarSetU16(scalar, kFieldBoardSeed, state.boardSeed);
#if SH_FEATURE_MINES
    scalar.bytes[kFieldMines] = state.minesAlive;
#else
    scalar.bytes[kFieldMines] = 0;
#endif
    scalarSetU32(scalar, kFieldVoyageTicks, state.voyageTicks);
    scalarSetU16(scalar, kFieldDelivered, state.delivered);
    scalarSetU16(scalar, kFieldRescues, state.rescues);
    scalar.bytes[kFieldEnding] = state.ending;
    return scalarValid(scalar);
}

template <typename Storage> inline void readScalar(Storage &storage, uint16_t payload, Scalar &scalar) {
    for (uint8_t i = 0; i < kScalarBytes; ++i)
        scalar.bytes[i] = storage.read(static_cast<uint16_t>(payload + i));
}

// Stream one payload byte to storage while folding it into the CRC. The save
// never builds a second payload buffer (and never a chart-sized one).
template <typename Storage> inline void streamByte(Storage &storage, uint16_t address, uint8_t value, uint16_t &crc) {
    storage.update(address, value);
    crc = crc16Update(crc, value);
}

template <typename Storage> inline void writeScalar(Storage &storage, uint16_t payload, const Scalar &scalar, uint16_t &crc) {
    for (uint8_t i = 0; i < kScalarBytes; ++i)
        streamByte(storage, static_cast<uint16_t>(payload + i), scalar.bytes[i], crc);
}

// Read the 16-byte header once. Header fields and the CRC share these bytes,
// so validation does not issue the same EEPROM reads twice.
template <typename Storage> inline void readHeader(Storage &storage, uint16_t base, uint8_t *header) {
    for (uint8_t i = 0; i < kHeaderBytes; ++i)
        header[i] = storage.read(static_cast<uint16_t>(base + i));
}

// Build header bytes 0..13. The CRC is computed over this block; the magic is
// still physically written last so a torn header cannot look committed.
inline void buildHeader(uint8_t *header, uint32_t sequence) {
    header[kMagicOffset] = kMagic0;
    header[kMagicOffset + 1] = kMagic1;
    header[kMagicOffset + 2] = kMagic2;
    header[kMagicOffset + 3] = kMagic3;
    header[kSchemaOffset] = kSchemaVersion;
    header[kLayoutOffset] = world::LAYOUT_VERSION;
    header[kContentOffset] = kCampaignContentVersion;
    header[kSignatureOffset] = kProfileSignature;
    header[kSequenceOffset] = static_cast<uint8_t>(sequence);
    header[kSequenceOffset + 1] = static_cast<uint8_t>(sequence >> 8);
    header[kSequenceOffset + 2] = static_cast<uint8_t>(sequence >> 16);
    header[kSequenceOffset + 3] = static_cast<uint8_t>(sequence >> 24);
    header[kLengthOffset] = static_cast<uint8_t>(kPayloadBytes);
    header[kLengthOffset + 1] = static_cast<uint8_t>(kPayloadBytes >> 8);
}

template <typename Storage> inline SlotInfo inspectSlot(Storage &storage, uint8_t index) {
    SlotInfo info;
    info.status = kSlotBlank;
    info.sequence = 0;

    const uint16_t base = slotAddress(index);
    uint8_t header[kHeaderBytes];
    readHeader(storage, base, header);
    const uint8_t commit = storage.read(static_cast<uint16_t>(base + kCommitOffset));

    // Blank requires both a non-magic prefix and a non-committed byte. A
    // present magic or commit byte means the slot belongs to this game even if
    // it is torn, corrupt, incompatible or in progress.
    const bool magic = header[kMagicOffset] == kMagic0 && header[kMagicOffset + 1] == kMagic1 && header[kMagicOffset + 2] == kMagic2 && header[kMagicOffset + 3] == kMagic3;
    if (!magic) {
        info.status = commit == kCommitValid ? kSlotCorrupt : kSlotBlank;
        return info;
    }

    info.sequence = static_cast<uint32_t>(header[kSequenceOffset]) | (static_cast<uint32_t>(header[kSequenceOffset + 1]) << 8) | (static_cast<uint32_t>(header[kSequenceOffset + 2]) << 16) |
                    (static_cast<uint32_t>(header[kSequenceOffset + 3]) << 24);
    if (commit != kCommitValid) {
        info.status = kSlotCorrupt;
        return info;
    }

    if (header[kSchemaOffset] != kSchemaVersion || header[kLayoutOffset] != world::LAYOUT_VERSION || header[kContentOffset] != kCampaignContentVersion || header[kSignatureOffset] != kProfileSignature) {
        info.status = kSlotIncompatible;
        return info;
    }

    const uint16_t length = static_cast<uint16_t>(header[kLengthOffset] | (static_cast<uint16_t>(header[kLengthOffset + 1]) << 8));
    if (length != kPayloadBytes) {
        info.status = kSlotCorrupt;
        return info;
    }

    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < kCrcOffset; ++i)
        crc = crc16Update(crc, header[i]);
    for (uint16_t i = 0; i < length; ++i)
        crc = crc16Update(crc, storage.read(static_cast<uint16_t>(base + kPayloadOffset + i)));
    const uint16_t expected = static_cast<uint16_t>(header[kCrcOffset] | (static_cast<uint16_t>(header[kCrcOffset + 1]) << 8));
    if (crc != expected) {
        info.status = kSlotCorrupt;
        return info;
    }

    Scalar scalar;
    readScalar(storage, static_cast<uint16_t>(base + kPayloadOffset), scalar);
    info.status = scalarValid(scalar) ? kSlotValid : kSlotCorrupt;
    return info;
}

template <typename Storage> inline Inventory inspect(Storage &storage) {
    Inventory inventory;
    inventory.validCount = 0;
    inventory.presentCount = 0;
    inventory.newestSlot = -1;
    inventory.newestSequence = 0;
    inventory.anyIncompatible = false;
    inventory.anyCorrupt = false;

    for (uint8_t i = 0; i < kSlotCount; ++i) {
        const SlotInfo info = inspectSlot(storage, i);
        inventory.slot[i] = info;
        if (info.status != kSlotBlank)
            ++inventory.presentCount;
        if (info.status == kSlotValid) {
            ++inventory.validCount;
            if (inventory.newestSlot < 0 || isNewer(info.sequence, inventory.newestSequence)) {
                inventory.newestSlot = static_cast<int8_t>(i);
                inventory.newestSequence = info.sequence;
            }
        } else if (info.status == kSlotIncompatible) {
            inventory.anyIncompatible = true;
        } else if (info.status == kSlotCorrupt) {
            inventory.anyCorrupt = true;
        }
    }
    return inventory;
}

// Exactly one valid slot: use the invalid one. Both valid: use the older
// sequence. Neither valid: slot B first, because slot A overlaps the legacy
// chart block and must never be the first-ever target.
inline uint8_t chooseTarget(const Inventory &inventory) {
    if (inventory.validCount == 1) {
        for (uint8_t i = 0; i < kSlotCount; ++i) {
            if (inventory.slot[i].status != kSlotValid)
                return i;
        }
    }
    if (inventory.validCount == 2) {
        // Lower sequence is the target. On an equal pair, A is deterministic.
        return isNewer(inventory.slot[0].sequence, inventory.slot[1].sequence) ? 1 : 0;
    }
    return 1;
}

inline uint32_t nextSequence(const Inventory &inventory) {
    if (inventory.validCount == 0)
        return 1;
    return static_cast<uint32_t>(inventory.newestSequence + 1u);
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

inline void applyScalar(const Scalar &scalar, State &state) {
    game::reset(state);

    state.docked = static_cast<int8_t>(scalarU8(scalar, kFieldBerth));
    state.dockCooldown = 0;
#if SH_FEATURE_MULTI_HULL
    state.hullClass = scalarU8(scalar, kFieldHullClass);
#endif
#if SH_FEATURE_SHOP
    state.airLevel = scalarU8(scalar, kFieldAirLevel);
    state.hullLevel = scalarU8(scalar, kFieldHullLevel);
    state.sonarLevel = scalarU8(scalar, kFieldSonarLevel);
#endif
    state.credits = scalarU16(scalar, kFieldCredits);
    state.tools = scalarU8(scalar, kFieldTools);
    state.pickups = scalarU8(scalar, kFieldPickups);
    state.openGates = scalarU16(scalar, kFieldOpenGates);
    state.seenGates = scalarU16(scalar, kFieldSeenGates);
    state.campaign = scalarU8(scalar, kFieldCampaign);
    state.surveys = scalarU8(scalar, kFieldSurveys);
    state.visited = scalarU8(scalar, kFieldVisited);
    state.activeJob.kind = scalarU8(scalar, kFieldJobKind);
    state.activeJob.mission = scalarU8(scalar, kFieldJobMission);
    state.activeJob.from = scalarU8(scalar, kFieldJobFrom);
    state.activeJob.to = scalarU8(scalar, kFieldJobTo);
    state.activeJob.crates = scalarU8(scalar, kFieldJobCrates);
    state.activeJob.reward = scalarU8(scalar, kFieldJobReward);
    state.activeJob.deadline = scalarU16(scalar, kFieldJobDeadline);
    state.activeJob.elapsed = scalarU16(scalar, kFieldJobElapsed);
    state.offerCursor = scalarU16(scalar, kFieldOfferCursor);
    state.offerSerial = state.offerCursor;
    state.boardSeed = scalarU16(scalar, kFieldBoardSeed);
    state.offerCount = 0;
    state.lastDelivery = 0;
#if SH_FEATURE_MINES
    state.minesAlive = scalarU8(scalar, kFieldMines);
#endif
    state.voyageTicks = scalarU32(scalar, kFieldVoyageTicks);
    state.delivered = scalarU16(scalar, kFieldDelivered);
    state.rescues = scalarU16(scalar, kFieldRescues);
    state.ending = scalarU8(scalar, kFieldEnding);

    // A checkpoint is always at a berth. Cargo is derived from the active
    // job; service restores full air/hull and neutral trim for that load.
    state.cargo = state.activeJob.kind == game::kJobKindNone ? 0 : state.activeJob.crates;
    state.ballast = static_cast<uint8_t>(game::neutralFor(state.cargo));
    state.air = game::airCapacity(state);
#if SH_FEATURE_HULL_DAMAGE
    state.hull = game::hullCapacity(state);
#endif

    // Restore both freight settlements and generated service berths. The
    // scalar validator has already accepted the berth only when its
    // restoration milestone is present.
    world::Berth berth;
    if (world::berthInfo(state.docked, berth)) {
        state.x = berth.pixel_x;
        state.y = berth.pixel_y;
        state.xFrac = 0;
        state.yFrac = 0;
        state.vxQ8 = 0;
        state.vyQ8 = 0;
    }
}

template <typename Storage>
inline LoadResult load(State &state
#if SH_FEATURE_FOG_CHART
                       ,
                       chart::State &chart
#endif
                       ,
                       Storage &storage) {
    const Inventory inventory = inspect(storage);
    if (inventory.validCount == 0) {
        if (inventory.anyIncompatible)
            return kIncompatibleSave;
        if (inventory.presentCount != 0)
            return kCorruptSave;
        return kNoSave;
    }

    const uint8_t chosen = static_cast<uint8_t>(inventory.newestSlot);
    const uint16_t base = slotAddress(chosen);
    Scalar scalar;
    readScalar(storage, static_cast<uint16_t>(base + kPayloadOffset), scalar);
    applyScalar(scalar, state);
#if SH_FEATURE_FOG_CHART
    for (uint16_t i = 0; i < kChartBytes; ++i)
        chart.explored[i] = storage.read(static_cast<uint16_t>(base + kPayloadOffset + kScalarBytes + i));
#endif
    return kLoaded;
}

// ---------------------------------------------------------------------------
// Save transaction
// ---------------------------------------------------------------------------

// Transaction order is fixed by the contract:
//   1. invalidate the target commit byte;
//   2. stream the payload from live state (no second payload buffer);
//   3. write schema/layout/content/signature/sequence/length, then CRC;
//   4. write the magic, then the commit byte last;
//   5. re-read and validate before reporting success.
template <typename Storage>
inline SaveResult save(const State &state
#if SH_FEATURE_FOG_CHART
                       ,
                       const chart::State &chart
#endif
                       ,
                       Storage &storage) {
    Scalar scalar;
    if (!captureState(state, scalar))
        return kInvalidState;

    const Inventory inventory = inspect(storage);
    const uint8_t target = chooseTarget(inventory);
    const uint32_t sequence = nextSequence(inventory);
    const uint16_t base = slotAddress(target);

    // A torn or stale target is invalidated before any payload byte changes.
    storage.update(static_cast<uint16_t>(base + kCommitOffset), kCommitInvalid);

    uint8_t header[kCrcOffset];
    buildHeader(header, sequence);
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < kCrcOffset; ++i)
        crc = crc16Update(crc, header[i]);

    const uint16_t payload = static_cast<uint16_t>(base + kPayloadOffset);
    writeScalar(storage, payload, scalar, crc);
#if SH_FEATURE_FOG_CHART
    for (uint16_t i = 0; i < kChartBytes; ++i)
        streamByte(storage, static_cast<uint16_t>(payload + kScalarBytes + i), chart.explored[i], crc);
#endif

    // Header write order is intentionally not address order: the magic and
    // commit bytes are last, so no torn header can look committed.
    for (uint8_t i = kMagicOffset + 4; i < kCrcOffset; ++i)
        storage.update(static_cast<uint16_t>(base + i), header[i]);
    storage.update(static_cast<uint16_t>(base + kCrcOffset), static_cast<uint8_t>(crc));
    storage.update(static_cast<uint16_t>(base + kCrcOffset + 1), static_cast<uint8_t>(crc >> 8));
    for (uint8_t i = kMagicOffset; i < kMagicOffset + 4; ++i)
        storage.update(static_cast<uint16_t>(base + i), header[i]);
    storage.update(static_cast<uint16_t>(base + kCommitOffset), kCommitValid);

    const SlotInfo written = inspectSlot(storage, target);
    if (written.status != kSlotValid || written.sequence != sequence)
        return kWriteFailed;
    return kSaved;
}

// ---------------------------------------------------------------------------
// Legacy chart migration and explicit new voyage
// ---------------------------------------------------------------------------

#if SH_FEATURE_FOG_CHART
template <typename Storage> inline bool legacyChartValid(Storage &storage) {
    if (storage.read(0) != kLegacyMagic0 || storage.read(1) != kLegacyMagic1)
        return false;
    if (storage.read(2) != kLegacyVersion)
        return false;
    uint8_t crc = 0;
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        crc = crc8Update(crc, storage.read(static_cast<uint16_t>(kLegacyDataOffset + i)));
    return crc == storage.read(kLegacyCrcOffset);
}

template <typename Storage> inline bool loadLegacyChart(Storage &storage, chart::State &chart) {
    if (!legacyChartValid(storage))
        return false;
    for (uint16_t i = 0; i < world::CHART_BYTES; ++i)
        chart.explored[i] = storage.read(static_cast<uint16_t>(kLegacyDataOffset + i));
    return true;
}

template <typename Storage> inline SaveResult newVoyage(State &state, chart::State &chart, LegacyChart legacy, Storage &storage) {
    const Inventory inventory = inspect(storage);
#if SH_FEATURE_MULTI_HULL
    const uint8_t hull = state.hullClass;
#endif
    game::reset(state);
#if SH_FEATURE_MULTI_HULL
    state.hullClass = validHullClass(hull) ? hull : static_cast<uint8_t>(roster::kDefault);
#endif
    state.docked = static_cast<int8_t>(campaign::kTidewall);

    // Legacy bits may be imported only before the first voyage exists. Slot B is
    // the first target and does not overlap the legacy block, so a blank slot B
    // plus no valid voyage means there is no earlier checkpoint to preserve.
    const bool firstVoyage = inventory.validCount == 0 && inventory.slot[1].status == kSlotBlank;
    if (legacy == kKeepLegacyChart && firstVoyage && loadLegacyChart(storage, chart))
        return save(state, chart, storage);
    chart::reset(chart);
    return save(state, chart, storage);
}
#else
template <typename Storage> inline SaveResult newVoyage(State &state, LegacyChart, Storage &storage) {
    const Inventory inventory = inspect(storage);
    (void)inventory;
#if SH_FEATURE_MULTI_HULL
    const uint8_t hull = state.hullClass;
#endif
    game::reset(state);
#if SH_FEATURE_MULTI_HULL
    state.hullClass = validHullClass(hull) ? hull : static_cast<uint8_t>(roster::kDefault);
#endif
    state.docked = static_cast<int8_t>(campaign::kTidewall);
    return save(state, storage);
}
#endif

#if defined(ARDUINO)
#include <EEPROM.h>

// Device adapter. The sketch names only voyage_save::save/load/newVoyage; the
// pure code above stays free of Arduino headers for the host tests.
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

}   // namespace voyage_save

#endif   // SH_FEATURE_SAVE_VOYAGE
