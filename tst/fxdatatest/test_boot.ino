// Device smoke test: boots the FX cart, checks the generated sprite table and
// the baked world tables, and prints exactly one P/F marker for the Ardens
// serial harness. Cart reads stay here in setup (outside any display
// transaction); the world grid is never loaded into SRAM.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "src/generated/campaign_sprites.hpp"
#include "src/generated/music_regions.hpp"
#include "src/generated/panel_sprites.hpp"
#include "src/generated/pause_sprites.hpp"
#include "src/generated/sprites.hpp"
#include "src/generated/pilot_sprites.hpp"
#include "src/generated/world.hpp"
#include "src/generated/cargo.hpp"
#include "src/generated/music.hpp"
#include "src/generated/hud_sprites.hpp"
#include "src/generated/menu_sprites.hpp"
#include "src/generated/splash_sprites.hpp"
#include "src/generated/story_sprites.hpp"
#include "src/generated/story_beats.hpp"
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
static_assert(txt_loader == txt_a_finish + sizeof("A FINISH"), "loader label follows the ending labels after the archival ADPCM streams were removed");
static_assert(txt_roster_load_hint == txt_loader + sizeof("LOADER"), "roster loader hint follows the loader label");
constexpr uint24_t spriteBytes(uint8_t width, uint8_t height, uint8_t frames, bool masked) {
    return static_cast<uint24_t>(2) + static_cast<uint24_t>(width) * ((height + 7) / 8) * frames * (masked ? 2 : 1);
}
static_assert(panel_abandon_confirm == txt_roster_load_hint + sizeof("UP/DN A OK  L/R HULL"), "story panels append after the loader strings");
static_assert(panel_ending_network == panel_abandon_confirm + spriteBytes(PANEL_ABANDON_CONFIRM_WIDTH, PANEL_ABANDON_CONFIRM_HEIGHT, PANEL_ABANDON_CONFIRM_FRAMES, PANEL_ABANDON_CONFIRM_MASKED), "ending network follows the first prompt");
static_assert(panel_ending_stats == panel_ending_network + spriteBytes(PANEL_ENDING_NETWORK_WIDTH, PANEL_ENDING_NETWORK_HEIGHT, PANEL_ENDING_NETWORK_FRAMES, PANEL_ENDING_NETWORK_MASKED), "ending stats follow the network card");
static_assert(panel_ending_thanks == panel_ending_stats + spriteBytes(PANEL_ENDING_STATS_WIDTH, PANEL_ENDING_STATS_HEIGHT, PANEL_ENDING_STATS_FRAMES, PANEL_ENDING_STATS_MASKED), "ending thanks follow the stats card");
static_assert(panel_pause_rescue == panel_ending_thanks + spriteBytes(PANEL_ENDING_THANKS_WIDTH, PANEL_ENDING_THANKS_HEIGHT, PANEL_ENDING_THANKS_FRAMES, PANEL_ENDING_THANKS_MASKED), "pause panel follows the ending cards");
static_assert(panel_pause_resume == panel_pause_rescue + spriteBytes(PANEL_PAUSE_RESCUE_WIDTH, PANEL_PAUSE_RESCUE_HEIGHT, PANEL_PAUSE_RESCUE_FRAMES, PANEL_PAUSE_RESCUE_MASKED), "pause selection variants stay adjacent");
static_assert(panel_rescue_confirm == panel_pause_resume + spriteBytes(PANEL_PAUSE_RESUME_WIDTH, PANEL_PAUSE_RESUME_HEIGHT, PANEL_PAUSE_RESUME_FRAMES, PANEL_PAUSE_RESUME_MASKED), "rescue confirmation follows the pause variants");
static_assert(panel_save_continue == panel_rescue_confirm + spriteBytes(PANEL_RESCUE_CONFIRM_WIDTH, PANEL_RESCUE_CONFIRM_HEIGHT, PANEL_RESCUE_CONFIRM_FRAMES, PANEL_RESCUE_CONFIRM_MASKED), "save failure variants follow the confirmations");
static_assert(panel_save_retry == panel_save_continue + spriteBytes(PANEL_SAVE_CONTINUE_WIDTH, PANEL_SAVE_CONTINUE_HEIGHT, PANEL_SAVE_CONTINUE_FRAMES, PANEL_SAVE_CONTINUE_MASKED), "save retry follows save continue");
static_assert(title_keep == panel_save_retry + spriteBytes(PANEL_SAVE_RETRY_WIDTH, PANEL_SAVE_RETRY_HEIGHT, PANEL_SAVE_RETRY_FRAMES, PANEL_SAVE_RETRY_MASKED), "title keep follows the prompts");
static_assert(title_menu == title_keep + spriteBytes(TITLE_KEEP_WIDTH, TITLE_KEEP_HEIGHT, TITLE_KEEP_FRAMES, TITLE_KEEP_MASKED), "title menu follows keep chart");
static_assert(title_replace == title_menu + spriteBytes(TITLE_MENU_WIDTH, TITLE_MENU_HEIGHT, TITLE_MENU_FRAMES, TITLE_MENU_MASKED), "replace confirmation follows the menu sheet");
static_assert(work_active == title_replace + spriteBytes(TITLE_REPLACE_WIDTH, TITLE_REPLACE_HEIGHT, TITLE_REPLACE_FRAMES, TITLE_REPLACE_MASKED), "work active strip follows the title sheet");
static_assert(work_details == work_active + spriteBytes(WORK_ACTIVE_WIDTH, WORK_ACTIVE_HEIGHT, WORK_ACTIVE_FRAMES, WORK_ACTIVE_MASKED), "work details follow the active strip");
static_assert(work_rows == work_details + spriteBytes(WORK_DETAILS_WIDTH, WORK_DETAILS_HEIGHT, WORK_DETAILS_FRAMES, WORK_DETAILS_MASKED), "work rows follow the detail sheet");
static_assert(lift_cable == work_rows + spriteBytes(WORK_ROWS_WIDTH, WORK_ROWS_HEIGHT, WORK_ROWS_FRAMES, WORK_ROWS_MASKED), "restoration sprites follow the work sheets");
static_assert(mooring == lift_cable + spriteBytes(LIFT_CABLE_WIDTH, LIFT_CABLE_HEIGHT, LIFT_CABLE_FRAMES, LIFT_CABLE_MASKED), "mooring follows the lift cable");
static_assert(pump_flow == mooring + spriteBytes(MOORING_WIDTH, MOORING_HEIGHT, MOORING_FRAMES, MOORING_MASKED), "pump flow follows the mooring");
static_assert(survey_ring == pump_flow + spriteBytes(PUMP_FLOW_WIDTH, PUMP_FLOW_HEIGHT, PUMP_FLOW_FRAMES, PUMP_FLOW_MASKED), "survey ring follows the pump flow");
static_assert(txt_music_on == survey_ring + spriteBytes(SURVEY_RING_WIDTH, SURVEY_RING_HEIGHT, SURVEY_RING_FRAMES, SURVEY_RING_MASKED), "music labels follow the restoration art");
static_assert(txt_music_off == txt_music_on + sizeof("MUSIC ON"), "music off follows music on");
static_assert(musicRegions == txt_music_off + sizeof("MUSIC OFF"), "region plane follows the music labels");
static_assert(pause_menu == musicRegions + music_regions::kPlaneBytes, "pause menu follows the region plane");
static_assert(txt_saved_berth == pause_menu + spriteBytes(PAUSE_MENU_WIDTH, PAUSE_MENU_HEIGHT, PAUSE_MENU_FRAMES, PAUSE_MENU_MASKED), "HUD saved notice follows the pause menu");
static_assert(txt_unsaved_find == txt_saved_berth + sizeof("SAVED AT BERTH"), "HUD unsaved notice follows saved notice");
static_assert(txt_story == txt_unsaved_find + sizeof("UNSAVED FIND"), "WORK story label follows the HUD notices");
static_assert(txt_cargo == txt_story + sizeof("STORY"), "WORK freight label follows the story label");
static_assert(txt_work_accept_hint == txt_cargo + sizeof("CARGO"), "WORK accept hint follows the freight label");
static_assert(txt_short_load == txt_work_accept_hint + sizeof("A ACCEPT  B BACK"), "short-load note follows the accept hint");
static_assert(hud_controls == txt_short_load + sizeof("SHORT LOAD"), "HUD stamps append after every shipped WORK label");
static_assert(hud_notices == hud_controls + spriteBytes(HUD_CONTROLS_WIDTH, HUD_CONTROLS_HEIGHT, HUD_CONTROLS_FRAMES, HUD_CONTROLS_MASKED), "HUD notices follow the controls");
static_assert(shop_hints == hud_notices + spriteBytes(HUD_NOTICES_WIDTH, HUD_NOTICES_HEIGHT, HUD_NOTICES_FRAMES, HUD_NOTICES_MASKED), "menus append after shipped HUD stamps");
static_assert(shop_menu == shop_hints + spriteBytes(SHOP_HINTS_WIDTH, SHOP_HINTS_HEIGHT, SHOP_HINTS_FRAMES, SHOP_HINTS_MASKED), "shop screens follow their hints");
static_assert(station_hints == shop_menu + spriteBytes(SHOP_MENU_WIDTH, SHOP_MENU_HEIGHT, SHOP_MENU_FRAMES, SHOP_MENU_MASKED), "station hints follow shop screens");
static_assert(station_menu == station_hints + spriteBytes(STATION_HINTS_WIDTH, STATION_HINTS_HEIGHT, STATION_HINTS_FRAMES, STATION_HINTS_MASKED), "station screens follow their hints");
static_assert(startup == station_menu + spriteBytes(STATION_MENU_WIDTH, STATION_MENU_HEIGHT, STATION_MENU_FRAMES, STATION_MENU_MASKED), "startup appends after the menu screens");
static_assert(STARTUP_FRAMES == 2 && !STARTUP_MASKED, "two opaque startup frames");
static_assert(story_beacon == startup + spriteBytes(STARTUP_WIDTH, STARTUP_HEIGHT, STARTUP_FRAMES, STARTUP_MASKED), "restoration cards append after startup");
static_assert(story_first == story_beacon + spriteBytes(STORY_BEACON_WIDTH, STORY_BEACON_HEIGHT, STORY_BEACON_FRAMES, STORY_BEACON_MASKED), "first delivery follows the beacon card");
static_assert(story_lift == story_first + spriteBytes(STORY_FIRST_WIDTH, STORY_FIRST_HEIGHT, STORY_FIRST_FRAMES, STORY_FIRST_MASKED), "lift follows the first delivery card");
static_assert(story_pump == story_lift + spriteBytes(STORY_LIFT_WIDTH, STORY_LIFT_HEIGHT, STORY_LIFT_FRAMES, STORY_LIFT_MASKED), "pump follows the lift card");
static_assert(storyBeats == txt_survey_chart_hint + sizeof("A CHART  B BACK"), "story decks append after the chart hint");
static_assert(txt_freight_yard == storyBeats + STORY_BEAT_BYTES, "work-site labels append after the story deck blob");
static_assert(txt_deep_relay == txt_freight_yard + sizeof("FREIGHT YARD"), "relay label follows the Freight Yard label");
static_assert(FX_DATA_BYTES == txt_deep_relay + sizeof("DEEP RELAY"), "FX image ends after the appended labels");
static_assert(STORY_CARD_COUNT == 43 && STORY_IMAGE_CARD_COUNT == 21 && STORY_TEXT_CARD_COUNT == 22 && STORY_IMAGE_COUNT == 19 && STORY_FRAME_COUNT == 98, "source-backed story cards");
static_assert(FX_SAVE_BYTES == 4096, "save partition is one 4 KiB erase block");
static_assert(FX_SAVE_PAGE == 0xFFF0, "save partition is the last 16 cart pages");
static_assert((FX_DATA_BYTES + 255) / 256 + FX_SAVE_BYTES / 256 + FX_DATA_PAGE == 65536, "data and save partitions tile the cart image");
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
