// Device smoke test: boots the FX cart, checks the generated sprite table and
// the baked world tables, and prints exactly one P/F marker for the Ardens
// serial harness. Cart reads stay here in setup (outside any display
// transaction); the world grid is never loaded into SRAM.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "src/generated/sprites.hpp"
#include "src/generated/pilot_sprites.hpp"
#include "src/generated/world.hpp"
#include "src/generated/cargo.hpp"

// First byte of fxdata/tables/world.bin: tile row 0 is inside the sealed
// two-tile border, so all eight tiles are solid (bit 1).
constexpr uint8_t WORLD_FIRST_BYTE = 0xFF;
// First byte of fxdata/tables/world-chart.bin: chart row 0 covers tile rows
// 0..4, all inside the sealed border, so the first eight cells are land.
constexpr uint8_t CHART_FIRST_BYTE = 0x00;
// First record bytes of fxdata/tables/cargo.bin (MINNOW arrangement 0,
// piece 0): mask 0x0017 little-endian, posKind 0x02 (x=2, y=0, required).
constexpr uint8_t CARGO_FIRST_MASK_LO = 0x17;
constexpr uint8_t CARGO_FIRST_MASK_HI = 0x00;
constexpr uint8_t CARGO_FIRST_POSKIND = 0x02;
// Each pilot sheet is a two-byte header plus four 26x8 plus-mask frames.
constexpr uint24_t PILOT_SPRITE_BYTES = 2 + 4 * 26 * 2;

// Flash ABI order: the world tables are appended after the shipped sprite
// include and packed back to back, then the cargo bank. The external font,
// deduplicated UI strings, and pilot sheets follow the cargo bank without
// moving any earlier symbol.
static_assert(worldTiles > hero, "worldTiles must follow the shipped symbols");
static_assert(worldChart == worldTiles + world::WORLD_BYTES,
              "worldChart must immediately follow worldTiles");
static_assert(cargoBank == worldChart + world::CHART_BYTES,
              "cargoBank must immediately follow the world chart");
static_assert(font_6x8 == cargoBank + CARGO_TABLE_BYTES,
              "font must immediately follow the cargo bank");
static_assert(txt_paused > font_6x8, "text must follow the font");
static_assert(hauler > txt_paused, "pilot sheets must follow the strings");
static_assert(leviathan == hauler + PILOT_SPRITE_BYTES,
              "pilot sheets are appended in generated order");
static_assert(minnow == leviathan + PILOT_SPRITE_BYTES,
              "pilot sheets are appended in generated order");
static_assert(FX_DATA_BYTES == minnow + PILOT_SPRITE_BYTES,
              "FX image size must end after the pilot sheets");

// Cargo block layout: fixed counts and back-to-back hull blocks.
static_assert(CARGO_MINNOW_OFFSET == 0, "MINNOW is the first hull block");
static_assert(CARGO_HAULER_OFFSET ==
                  CARGO_ARRANGEMENTS_PER_HULL * CARGO_MINNOW_RECORD_BYTES,
              "HAULER must follow MINNOW");
static_assert(CARGO_LEVIATHAN_OFFSET ==
                  CARGO_HAULER_OFFSET +
                      CARGO_ARRANGEMENTS_PER_HULL * CARGO_HAULER_RECORD_BYTES,
              "LEVIATHAN must follow HAULER");
static_assert(CARGO_TABLE_BYTES ==
                  CARGO_LEVIATHAN_OFFSET +
                      CARGO_ARRANGEMENTS_PER_HULL * CARGO_LEVIATHAN_RECORD_BYTES,
              "cargo table must end after LEVIATHAN");
static_assert(CARGO_TABLE_BYTES == 3648, "cargo bank size is fixed by M4.2");

Arduboy2 arduboy;

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    bool ok = HERO_WIDTH == 16 && HERO_HEIGHT == 16 && HERO_FRAMES == 3 && HERO_MASKED;

    FX::seekData(hero);
    const uint8_t width = FX::readPendingUInt8();
    const uint8_t height = FX::readEnd();
    ok = ok && width == HERO_WIDTH && height == HERO_HEIGHT;

    // External text and pilot art live after the cargo bank. Probe the font
    // cell and the first pilot sheet header; the static asserts above pin the
    // complete append chain.
    FX::seekData(font_6x8);
    const uint16_t fontWidth = FX::readPendingUInt16();
    const uint16_t fontHeight = FX::readPendingLastUInt16();
    ok = ok && fontWidth == 6 && fontHeight == 8;

    FX::seekData(hauler);
    const uint8_t pilotWidth = FX::readPendingUInt8();
    const uint8_t pilotHeight = FX::readEnd();
    ok = ok && pilotWidth == HAULER_WIDTH && pilotHeight == HAULER_HEIGHT;

    // Probe the first byte of each appended world table. The static asserts
    // above tie their offsets and sizes to the generated metadata.
    FX::seekData(worldTiles);
    const uint8_t worldFirst = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && worldFirst == WORLD_FIRST_BYTE;

    FX::seekData(worldChart);
    const uint8_t chartFirst = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && chartFirst == CHART_FIRST_BYTE;

    // Probe the cargo bank header order and one known first record. Reads stay
    // in setup, outside any display transaction; only three bytes are copied
    // to SRAM. M4.3 owns the runtime loader.
    FX::seekData(cargoBank);
    const uint8_t cargoMaskLo = FX::readPendingUInt8();
    const uint8_t cargoMaskHi = FX::readPendingUInt8();
    const uint8_t cargoPosKind = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && cargoMaskLo == CARGO_FIRST_MASK_LO &&
         cargoMaskHi == CARGO_FIRST_MASK_HI &&
         cargoPosKind == CARGO_FIRST_POSKIND;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
