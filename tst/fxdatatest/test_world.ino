// Device fixture for the runtime world accessors (sub-hauler-m1-runtime).
//
// Reads the real baked tables through world::FxTables in setup, which is
// outside every FX::display transaction. Samples known solid/open tiles, chart
// land/water bits, a pixel rectangle over the spawn tile, and a cache refresh.
// Prints exactly one bare P/F marker for the Ardens serial harness.
#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "src/world_fx.hpp"

Arduboy2 arduboy;

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    world::FxTables tables;
    bool ok = true;

    // Known baked cells: the sealed border is solid, the spawn chamber is open,
    // and chart cell (7,9) covers the open spawn block.
    ok = ok && world::tileSolid(tables, 0, 0);
    ok = ok && world::tileSolid(tables, 10, 10);
    ok = ok && !world::tileSolid(tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && world::tileSolid(tables, -1, 0);
    ok = ok && world::tileSolid(tables, world::TILES_X, 0);

    ok = ok && !world::chartCellWater(tables, 0, 0);
    ok = ok && world::chartCellWater(tables, 7, 9);
    ok = ok && world::tileWater(tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && !world::chartCellWater(tables, -1, 0);

    // The spawn tile's own 8x8 pixel rectangle is open.
    ok = ok && !world::rectHits(tables, world::SPAWN_PIXEL_X, world::SPAWN_PIXEL_Y, world::TILE_PIXELS, world::TILE_PIXELS);

    world::TileCache cache;
    world::clearCache(cache);
    ok = ok && world::refreshCache(cache, tables, 30, 44);
    ok = ok && world::cacheContains(cache, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && !world::tileSolidCached(cache, tables, world::SPAWN_TILE_X, world::SPAWN_TILE_Y);
    ok = ok && !world::refreshCache(cache, tables, 30, 44);
    ok = ok && world::refreshCache(cache, tables, 31, 44);

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}
