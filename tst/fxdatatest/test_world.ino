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
    world::Mine mine;
    ok = world::mineInfo(1, mine) && ok;
    ok = ok && mine.x == exploration_data::ROOM_K_X + 20 && mine.y == exploration_data::ROOM_K_Y - 20;
    world::Current current;
    ok = world::currentInfo(0, current) && ok;
    ok = ok && current.w == 32 && current.h == 16 && current.ax_q8 == 4096 && current.ay_q8 == 0;
    ok = ok && !world::currentInfo(world::CURRENT_COUNT, current);
    FX::seekData(explorationObjects);
    for (uint16_t i = 0; i < exploration_data::OBJECT_BYTES; ++i)
        ok = (FX::readPendingUInt8() == exploration_data::byteAt(i)) && ok;
    FX::readEnd();
    Serial.println(ok ? F("P") : F("F"));
}
void loop() {
}
