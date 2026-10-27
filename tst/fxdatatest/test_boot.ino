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
#include "src/generated/music.hpp"
#include "src/generated/exploration.hpp"

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
static_assert(sh_harbor_bubbles == minnow + PILOT_SPRITE_BYTES,
              "music must follow the pilot sheets");
static_assert(sh_kelp_drift == sh_harbor_bubbles + music_data::sh_harbor_bubbles_bytes,
              "Kelp Drift must follow Harbor Bubbles");
static_assert(sh_sonar_lullaby == sh_kelp_drift + music_data::sh_kelp_drift_bytes,
              "Sonar Lullaby must follow Kelp Drift");
static_assert(sh_abyssal_glass == sh_sonar_lullaby + music_data::sh_sonar_lullaby_bytes,
              "Abyssal Glass must follow Sonar Lullaby");
static_assert(sh_tidepool_waltz == sh_abyssal_glass + music_data::sh_abyssal_glass_bytes,
              "Tidepool Waltz must follow Abyssal Glass");
static_assert(sh_bubblejet_run == sh_tidepool_waltz + music_data::sh_tidepool_waltz_bytes,
              "Bubblejet Run must follow Tidepool Waltz");
static_assert(explorationTiles == sh_bubblejet_run + music_data::sh_bubblejet_run_bytes,
              "active level is appended after the unchanged music streams");
static_assert(explorationChart == explorationTiles + world::WORLD_BYTES, "active chart follows tiles");
static_assert(explorationObjects == explorationChart + world::CHART_BYTES, "objects follow active chart");
static_assert(txt_found_cutter == explorationObjects + exploration_data::OBJECT_BYTES, "labels follow object stream");
constexpr uint32_t NATIVE_TERRAIN_BYTES = static_cast<uint32_t>(world::PIXELS_X) * world::PIXELS_Y / 8;
static_assert(explorationTerrain == txt_dev_noclip + sizeof("DEV NOCLIP  A FAST"), "native terrain appends after unchanged labels");
static_assert(txt_sub_hauler == explorationTerrain + NATIVE_TERRAIN_BYTES, "voyage labels append after native terrain");
static_assert(FX_DATA_BYTES == txt_survey_recorded + sizeof("SURVEY RECORDED"), "FX image ends after the appended campaign labels");

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
    FX::seekData(explorationObjects);
    for(uint16_t i=0;i<exploration_data::OBJECT_BYTES;++i)
        ok = (FX::readPendingUInt8()==exploration_data::byteAt(i)) && ok;
    FX::readEnd();
    FX::seekData(explorationTiles);
    ok = (FX::readPendingUInt8()==WORLD_FIRST_BYTE) && ok;
    FX::readEnd();
    // The origin tile is interior rock with its deterministic dark speck at
    // pixel (0,0). Probe both ends of the much larger streamed section.
    FX::seekData(explorationTerrain);
    ok = (FX::readPendingUInt8() == 0xFE) && ok;
    FX::readEnd();
    FX::seekData(static_cast<uint24_t>(explorationTerrain + NATIVE_TERRAIN_BYTES - 1));
    ok = (FX::readPendingUInt8() == 0xFF) && ok;
    FX::readEnd();
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

    // All music streams start with compact streaming-loop framing (0xC1).
    FX::seekData(sh_harbor_bubbles);
    const uint8_t harborMagic = FX::readPendingUInt8();
    FX::readEnd();
    FX::seekData(sh_kelp_drift);
    const uint8_t kelpMagic = FX::readPendingUInt8();
    FX::readEnd();
    FX::seekData(sh_sonar_lullaby);
    const uint8_t sonarMagic = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && harborMagic == 0xC1 && kelpMagic == 0xC1 && sonarMagic == 0xC1;

    FX::seekData(sh_abyssal_glass);
    const uint8_t glassMagic = FX::readPendingUInt8();
    FX::readEnd();
    FX::seekData(sh_tidepool_waltz);
    const uint8_t waltzMagic = FX::readPendingUInt8();
    FX::readEnd();
    FX::seekData(sh_bubblejet_run);
    const uint8_t bubblejetMagic = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && glassMagic == 0xC1 && waltzMagic == 0xC1 && bubblejetMagic == 0xC1;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
