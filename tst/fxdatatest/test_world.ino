// Active authored-world FX reads and PROGMEM metadata; all SPI precedes display.
#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/world_fx.hpp"
#include "src/generated/exploration.hpp"
Arduboy2 arduboy;
void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    world::FxTables tables;
    bool ok = world::tileSolid(tables, 0, 0) && !world::tileSolid(tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && !world::chartCellWater(tables, 0, 0) && world::tileWater(tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && !world::rectHits(tables, world::SPAWN_SUB_X - 13, world::SPAWN_SUB_Y - 4, 26, 8);
    world::TileCache cache;
    world::clearCache(cache);
    const int16_t cx = world::SPAWN_TILE_X - 8, cy = world::SPAWN_TILE_Y - 4;
    ok = world::refreshCache(cache, tables, cx, cy) && ok;
    ok = ok && !world::tileSolidCached(cache, tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = !world::refreshCache(cache, tables, cx, cy) && ok;
    ok = world::refreshCache(cache, tables, cx + 1, cy) && ok;
    world::Settlement station;
    ok = world::settlementInfo(0, station) && ok;
    ok = ok && station.pixel_x == exploration_data::DOCK_T_X && station.pixel_y == exploration_data::DOCK_T_Y;
    ok = world::settlementInfo(3, station) && ok;
    ok = ok && station.pixel_x == exploration_data::DOCK_D_X && station.pixel_y == exploration_data::DOCK_D_Y;
    ok = ok && station.pixel_y > exploration_data::PRESSURE_Y && !world::settlementInfo(world::SETTLEMENT_COUNT, station);
    world::Berth berth;
    ok = world::serviceBerthInfo(0, berth) && ok;
    ok = ok && berth.id == world::BERTH_PUMP_REFUGE && berth.milestone == 2 && berth.pixel_x == 2160 && berth.pixel_y == 208;
    ok = world::berthInfo(world::BERTH_LOWER_GANTRY, berth) && ok;
    ok = ok && berth.milestone == 4 && berth.pixel_x == 1776 && berth.pixel_y == 808;
    ok = ok && berth.pixel_y < exploration_data::PRESSURE_Y;
    ok = world::berthInfo(world::BERTH_FREIGHT_YARD, berth) && ok;
    ok = ok && berth.milestone == 4 && berth.pixel_x == 2168 && berth.pixel_y == 616;
    ok = world::berthInfo(world::BERTH_DEEP_RELAY, berth) && ok;
    ok = ok && berth.milestone == 32 && berth.pixel_x == 1992 && berth.pixel_y == 1224;
    ok = ok && world::berthCount() == 8 && world::serviceBerthCount() == 4 && !world::berthInfo(8, berth);
    world::Survey survey;
    ok = world::surveyInfo(0, survey) && ok;
    ok = ok && survey.id == world::SURVEY_PUMP && survey.pixel_x == 2192 && survey.pixel_y == 176;
    ok = ok && world::surveyInfo(1, survey) && survey.id == world::SURVEY_LIFT;
    ok = ok && world::surveyCount() == 2 && !world::surveyInfo(2, survey);
    world::Current current;
    bool wideCurrent = false;
    bool gatedCurrent = false;
    for (uint8_t id = 0; id < world::CURRENT_COUNT; ++id) {
        ok = world::currentInfo(id, current) && ok;
        wideCurrent = wideCurrent || current.w > world::CURRENT_WIDTH_PX || current.h > world::CURRENT_HEIGHT_PX;
        gatedCurrent = gatedCurrent || world::currentGated(id);
        ok = ok && (current.ax_q8 != 0 || current.ay_q8 != 0);
        // Currents now span the common passage cross-section, so the exact
        // authored width/height and all four edge tiles are no longer fixed.
        // The centre is still navigable water for every zone.
        ok = ok && !world::tileSolid(tables,
                                     (current.x + current.w / 2) / world::TILE_PIXELS,
                                     (current.y + current.h / 2) / world::TILE_PIXELS) && ok;
    }
    ok = ok && wideCurrent;
    ok = ok && gatedCurrent;
    ok = ok && !world::currentInfo(world::CURRENT_COUNT, current);
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {
}
