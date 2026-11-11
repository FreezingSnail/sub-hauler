// Device smoke test: boots the FX cart, checks the generated sprite table and
// the active baked world tables, and prints exactly one P/F marker for the
// Ardens serial harness. Cart reads stay here in setup (outside any display
// transaction); the world grid is never loaded into SRAM.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"
#include "src/generated/campaign_sprites.hpp"
#include "src/generated/music_regions.hpp"
#include "src/generated/panel_sprites.hpp"
#include "src/generated/pause_sprites.hpp"
#include "src/generated/pilot_sprites.hpp"
#include "src/generated/text_stamp_sprites.hpp"
#include "src/generated/world.hpp"
#include "src/generated/music.hpp"
#include "src/generated/hud_sprites.hpp"
#include "src/generated/splash_sprites.hpp"
#include "src/generated/story_beats.hpp"

// First byte of fxdata/tables/exploration.bin: tile row 0 is inside the sealed
// two-tile border, so all eight tiles are solid (bit 1).
constexpr uint8_t WORLD_FIRST_BYTE = 0xFF;
// First byte of fxdata/tables/exploration-chart.bin: chart row 0 covers tile
// rows 0..4, all inside the sealed border, so the first eight cells are land.
constexpr uint8_t CHART_FIRST_BYTE = 0x00;
// Each pilot sheet is a two-byte header plus four plus-mask frames.
constexpr uint24_t PILOT_SPRITE_BYTES = 2 + HAULER_FRAMES * HAULER_WIDTH * ((HAULER_HEIGHT + 7) / 8) * (HAULER_MASKED ? 2 : 1);

// Flash ABI order: the external font is now the first shipped symbol (legacy
// world tables, cargo bank and the cart object stream were retired under the
// approved one-time ABI exception). Text, the HAULER pilot sheet, music, the
// active level sections and labels follow in declaration order.
static_assert(font_6x8 == 0, "external font is the first shipped symbol");
static_assert(txt_paused > font_6x8, "text must follow the font");
static_assert(txt_tidewall == txt_hauler + sizeof("HAULER"),
              "berth labels follow HAULER; the retired AIR status string leaves no gap");
static_assert(hauler == txt_undock + sizeof("UNDOCK"),
              "pilot sheet must follow the shipped strings");
static_assert(sh_harbor_bubbles == hauler + PILOT_SPRITE_BYTES,
              "music must follow the pilot sheet");
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
static_assert(txt_found_cutter == explorationChart + world::CHART_BYTES, "labels follow the active chart; the cart object stream is retired");
constexpr uint32_t NATIVE_TERRAIN_BYTES = static_cast<uint32_t>(world::PIXELS_X) * world::PIXELS_Y / 8;
static_assert(explorationTerrain == txt_seal_rumor + sizeof("SEAL: WEST WORKSHOP"), "native terrain appends after the retained labels");
static_assert(txt_sub_hauler == explorationTerrain + NATIVE_TERRAIN_BYTES, "voyage labels append after native terrain");
constexpr uint24_t spriteBytes(uint8_t width, uint8_t height, uint8_t frames, bool masked) {
    return static_cast<uint24_t>(2) + static_cast<uint24_t>(width) * ((height + 7) / 8) * frames * (masked ? 2 : 1);
}
static_assert(berth_icons == txt_a_next + sizeof("A NEXT"), "berth icon sheet is the first packed panel sprite");
static_assert(panel_abandon_confirm == berth_icons + spriteBytes(BERTH_ICONS_WIDTH, BERTH_ICONS_HEIGHT, BERTH_ICONS_FRAMES, BERTH_ICONS_MASKED), "story panels append after the berth icons");
static_assert(panel_ending_network == panel_abandon_confirm + spriteBytes(PANEL_ABANDON_CONFIRM_WIDTH, PANEL_ABANDON_CONFIRM_HEIGHT, PANEL_ABANDON_CONFIRM_FRAMES, PANEL_ABANDON_CONFIRM_MASKED), "ending network follows the first prompt");
static_assert(panel_ending_thanks == panel_ending_network + spriteBytes(PANEL_ENDING_NETWORK_WIDTH, PANEL_ENDING_NETWORK_HEIGHT, PANEL_ENDING_NETWORK_FRAMES, PANEL_ENDING_NETWORK_MASKED), "ending thanks follow the network card");
static_assert(panel_rescue_confirm == panel_ending_thanks + spriteBytes(PANEL_ENDING_THANKS_WIDTH, PANEL_ENDING_THANKS_HEIGHT, PANEL_ENDING_THANKS_FRAMES, PANEL_ENDING_THANKS_MASKED), "rescue confirmation follows the ending cards");
static_assert(panel_save_continue == panel_rescue_confirm + spriteBytes(PANEL_RESCUE_CONFIRM_WIDTH, PANEL_RESCUE_CONFIRM_HEIGHT, PANEL_RESCUE_CONFIRM_FRAMES, PANEL_RESCUE_CONFIRM_MASKED), "save failure variants follow the confirmations");
static_assert(panel_save_retry == panel_save_continue + spriteBytes(PANEL_SAVE_CONTINUE_WIDTH, PANEL_SAVE_CONTINUE_HEIGHT, PANEL_SAVE_CONTINUE_FRAMES, PANEL_SAVE_CONTINUE_MASKED), "save retry follows save continue");
static_assert(title_menu == panel_save_retry + spriteBytes(PANEL_SAVE_RETRY_WIDTH, PANEL_SAVE_RETRY_HEIGHT, PANEL_SAVE_RETRY_FRAMES, PANEL_SAVE_RETRY_MASKED), "title menu follows the prompts");
static_assert(title_replace == title_menu + spriteBytes(TITLE_MENU_WIDTH, TITLE_MENU_HEIGHT, TITLE_MENU_FRAMES, TITLE_MENU_MASKED), "replace confirmation follows the compact menu sheet");
static_assert(lift_cable == title_replace + spriteBytes(TITLE_REPLACE_WIDTH, TITLE_REPLACE_HEIGHT, TITLE_REPLACE_FRAMES, TITLE_REPLACE_MASKED), "restoration sprites follow the title sheet");
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
static_assert(txt_work_accept_hint == txt_story + sizeof("STORY"), "WORK accept hint follows the story label");
static_assert(hud_notices == txt_work_accept_hint + sizeof("A ACCEPT  B BACK"), "HUD notice stamps append after every shipped WORK label");
static_assert(startup == hud_notices + spriteBytes(HUD_NOTICES_WIDTH, HUD_NOTICES_HEIGHT, HUD_NOTICES_FRAMES, HUD_NOTICES_MASKED), "startup appends after the HUD notice stamp");
static_assert(STARTUP_FRAMES == 2 && !STARTUP_MASKED, "two opaque startup frames");
static_assert(txt_pump_survey == startup + spriteBytes(STARTUP_WIDTH, STARTUP_HEIGHT, STARTUP_FRAMES, STARTUP_MASKED), "survey labels append after startup");
static_assert(txt_lift_survey == txt_pump_survey + sizeof("PUMP SURVEY"), "lift survey follows the pump survey");
static_assert(txt_survey_chart_hint == txt_lift_survey + sizeof("LIFT SURVEY"), "chart hint follows the survey labels");
static_assert(storyBeats == txt_survey_chart_hint + sizeof("A CHART  B BACK"), "story decks append after the chart hint");
static_assert(storyMeta == storyBeats + STORY_BEAT_BYTES, "story metadata appends after the story deck blob");
static_assert(txt_freight_yard == storyMeta + STORY_META_BYTES, "work-site labels append after the story metadata");
static_assert(txt_deep_relay == txt_freight_yard + sizeof("FREIGHT YARD"), "relay label follows the Freight Yard label");
static_assert(img_berth_names == txt_deep_relay + sizeof("DEEP RELAY"), "baked text stamps append after the last shipped string");
static_assert(img_berth_work == img_berth_names + spriteBytes(IMG_BERTH_NAMES_WIDTH, IMG_BERTH_NAMES_HEIGHT, IMG_BERTH_NAMES_FRAMES, IMG_BERTH_NAMES_MASKED), "berth-WORK header follows the berth-name sheet");
static_assert(img_carry_row == img_berth_work + spriteBytes(IMG_BERTH_WORK_WIDTH, IMG_BERTH_WORK_HEIGHT, IMG_BERTH_WORK_FRAMES, IMG_BERTH_WORK_MASKED), "CARRY rows follow the berth-WORK sheet");
static_assert(img_dock_barks == img_carry_row + spriteBytes(IMG_CARRY_ROW_WIDTH, IMG_CARRY_ROW_HEIGHT, IMG_CARRY_ROW_FRAMES, IMG_CARRY_ROW_MASKED), "bark sheet follows the CARRY rows");
static_assert(img_row_labels == img_dock_barks + spriteBytes(IMG_DOCK_BARKS_WIDTH, IMG_DOCK_BARKS_HEIGHT, IMG_DOCK_BARKS_FRAMES, IMG_DOCK_BARKS_MASKED), "row labels follow the bark sheet");
static_assert(img_story_row == img_row_labels + spriteBytes(IMG_ROW_LABELS_WIDTH, IMG_ROW_LABELS_HEIGHT, IMG_ROW_LABELS_FRAMES, IMG_ROW_LABELS_MASKED), "STORY rows follow the row labels");
static_assert(img_txt_a_next == img_story_row + spriteBytes(IMG_STORY_ROW_WIDTH, IMG_STORY_ROW_HEIGHT, IMG_STORY_ROW_FRAMES, IMG_STORY_ROW_MASKED), "individual string stamps follow the STORY sheet");
static_assert(IMG_BERTH_NAMES_FRAMES == 9 && IMG_BERTH_WORK_FRAMES == 8 && IMG_CARRY_ROW_FRAMES == 9 &&
              IMG_DOCK_BARKS_FRAMES == 73 && IMG_ROW_LABELS_FRAMES == 4 && IMG_STORY_ROW_FRAMES == 18 &&
              IMG_TXT_AIR_WIDTH == 128 && IMG_TXT_AIR_HEIGHT == 8 && IMG_TXT_AIR_FRAMES == 2 && IMG_TXT_AIR_MASKED,
              "baked stamp sheets and two-frame string rows");
static_assert(FX_DATA_BYTES == img_txt_yes_no + spriteBytes(IMG_TXT_YES_NO_WIDTH, IMG_TXT_YES_NO_HEIGHT, IMG_TXT_YES_NO_FRAMES, IMG_TXT_YES_NO_MASKED), "FX image ends after the last text stamp");
static_assert(STORY_CARD_COUNT == 43 && STORY_IMAGE_CARD_COUNT == 21 && STORY_TEXT_CARD_COUNT == 22 && STORY_IMAGE_COUNT == 21 && STORY_FRAME_COUNT == 106, "source-backed story cards");
static_assert(FX_SAVE_BYTES == 4096, "save partition is one 4 KiB erase block");
static_assert(FX_SAVE_PAGE == 0xFFF0, "save partition is the last 16 cart pages");
static_assert((FX_DATA_BYTES + 255) / 256 + FX_SAVE_BYTES / 256 + FX_DATA_PAGE == 65536, "data and save partitions tile the cart image");

Arduboy2 arduboy;

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    bool ok = true;

    // External text and pilot art ship before the music streams. Probe the
    // font cell and the sole pilot sheet header; the static asserts above pin
    // the complete append chain.
    FX::seekData(font_6x8);
    const uint16_t fontWidth = FX::readPendingUInt16();
    const uint16_t fontHeight = FX::readPendingLastUInt16();
    ok = ok && fontWidth == 6 && fontHeight == 8;

    FX::seekData(hauler);
    const uint8_t pilotWidth = FX::readPendingUInt8();
    const uint8_t pilotHeight = FX::readEnd();
    ok = ok && pilotWidth == HAULER_WIDTH && pilotHeight == HAULER_HEIGHT;

    // Probe the first byte of each active world section. The static asserts
    // above tie their offsets and sizes to the generated metadata.
    FX::seekData(explorationTiles);
    ok = (FX::readPendingUInt8()==WORLD_FIRST_BYTE) && ok;
    FX::readEnd();
    FX::seekData(explorationChart);
    const uint8_t chartFirst = FX::readPendingUInt8();
    FX::readEnd();
    ok = ok && chartFirst == CHART_FIRST_BYTE;
    // The origin tile is interior rock with its deterministic dark speck at
    // pixel (0,0). Probe both ends of the much larger streamed section.
    FX::seekData(explorationTerrain);
    ok = (FX::readPendingUInt8() == 0xFE) && ok;
    FX::readEnd();
    FX::seekData(static_cast<uint24_t>(explorationTerrain + NATIVE_TERRAIN_BYTES - 1));
    ok = (FX::readPendingUInt8() == 0xFF) && ok;
    FX::readEnd();

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

    // Berth icons are the first packed panel sprite: 16x8, eight frames.
    FX::seekData(berth_icons);
    const uint8_t iconWidth = FX::readPendingUInt8();
    const uint8_t iconHeight = FX::readEnd();
    ok = ok && iconWidth == BERTH_ICONS_WIDTH && iconHeight == BERTH_ICONS_HEIGHT;

    // Story metadata starts with scene 0's card count (none). The first baked
    // text sheet carries the generated 128x8 two-byte sprite header.
    FX::seekData(storyMeta);
    ok = (FX::readPendingUInt8() == 0) && ok;
    FX::readEnd();
    FX::seekData(img_berth_names);
    const uint8_t stampWidth = FX::readPendingUInt8();
    const uint8_t stampHeight = FX::readEnd();
    ok = ok && stampWidth == 128 && stampHeight == 8;

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
