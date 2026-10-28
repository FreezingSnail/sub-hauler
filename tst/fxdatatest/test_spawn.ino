// Permanent device regression for new-voyage/Continue world entry.
//
// The checkpoint stores only a berth id. This suite exercises the real
// generated berth table and packed collision plane, then checks that both
// newVoyage() and load() restore the boat position and snap the follow camera
// so the first undocked frame is inside the authored world, not its sealed
// corner. It uses a RAM storage adapter for the single slot; no EEPROM is touched.

#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "src/features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include "src/campaign.hpp"
#include "src/cave.hpp"
#include "src/fxdata.h"
#include "src/game.hpp"
#include "src/pilot.hpp"
#include "src/station.hpp"
#include "src/voyage_save.hpp"

Arduboy2Base arduboy;

namespace {

// The suite only ever targets the single slot, so a 512-byte backing store is
// enough. Reads outside it behave as erased cart bytes.
struct RamStorage {
    static uint8_t bytes[voyage_save::kSlotBytes];
    uint16_t pageOffset;
    uint16_t pageIndex;

    uint8_t read(uint16_t offset) {
        if (offset < voyage_save::kSlotBytes)
            return bytes[offset];
        return 0xFF;
    }
    void eraseBlock(uint8_t block) {
        if (block == 0) {
            for (uint16_t i = 0; i < voyage_save::kSlotBytes; ++i)
                bytes[i] = 0xFF;
        }
    }
    void pageBegin(uint16_t page) {
        pageOffset = static_cast<uint16_t>(page * voyage_save::kPageBytes);
        pageIndex = 0;
    }
    void pageByte(uint8_t value) {
        if (pageOffset + pageIndex < voyage_save::kSlotBytes)
            bytes[pageOffset + pageIndex] &= value;
        ++pageIndex;
    }
    void pageEnd() {
    }
};

uint8_t RamStorage::bytes[voyage_save::kSlotBytes];
RamStorage storage;
game::State state;
#if SH_FEATURE_FOG_CHART
chart::State chartState;
#endif

struct FxReader {
    uint8_t tileByte(uint16_t index) {
        FX::seekData(static_cast<uint24_t>(explorationTiles + index));
        const uint8_t value = FX::readPendingUInt8();
        FX::readEnd();
        return value;
    }
} tables;

bool checkPlacement(const world::Berth &berth) {
    const cave::Camera camera = cave::cameraAtTile(world::tileFromPixel(state.cameraX), world::tileFromPixel(state.cameraY));
    const int16_t sx = static_cast<int16_t>(state.x - camera.tileX * world::TILE_PIXELS);
    const int16_t sy = static_cast<int16_t>(state.y - camera.tileY * world::TILE_PIXELS);
    const bool visible = sx < cave::VIEW_WIDTH && sy < cave::VIEW_HEIGHT && static_cast<int16_t>(sx + game::kHullW) > 0 && static_cast<int16_t>(sy + game::kHullH) > 0;
    return state.docked == static_cast<int8_t>(berth.id) && state.x == berth.pixel_x && state.y == berth.pixel_y && state.cameraX == game::cameraTargetX(state) &&
           state.cameraY == game::cameraTargetY(state) && visible && !world::tileSolid(tables, berth.tile_x, berth.tile_y);
}

}   // namespace

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    delay(100);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    for (uint16_t i = 0; i < voyage_save::kSlotBytes; ++i)
        storage.bytes[i] = 0xFF;
#if SH_FEATURE_FOG_CHART
    chart::reset(chartState);
#endif

    bool ok = true;
    world::Berth tidewall;
    ok = world::berthInfo(static_cast<uint8_t>(campaign::kTidewall), tidewall) && ok;
    ok = (voyage_save::newVoyage(state
#if SH_FEATURE_FOG_CHART
                                 ,
                                 chartState
#endif
                                 ,
                                 storage) == voyage_save::kSaved) &&
         ok;
    ok = checkPlacement(tidewall) && ok;

    // Continue must reproduce the same berth position, camera and visible hull.
    state.x = 64;
    state.y = 32;
    state.cameraX = 0;
    state.cameraY = 0;
    ok = (voyage_save::load(state
#if SH_FEATURE_FOG_CHART
                            ,
                            chartState
#endif
                            ,
                            storage) == voyage_save::kLoaded) &&
         ok;
    ok = checkPlacement(tidewall) && ok;

    // Undock must leave the same valid camera/world view; docking only changes
    // the screen tag, not the stored position or snap.
    ok = station::undock(state) && ok;
    ok = (state.docked < 0) && ok;
    const cave::Camera undockedCamera = cave::cameraAtTile(world::tileFromPixel(state.cameraX), world::tileFromPixel(state.cameraY));
    const int16_t undockedX = static_cast<int16_t>(state.x - undockedCamera.tileX * world::TILE_PIXELS);
    const int16_t undockedY = static_cast<int16_t>(state.y - undockedCamera.tileY * world::TILE_PIXELS);
    ok = (state.cameraX == game::cameraTargetX(state) && state.cameraY == game::cameraTargetY(state) && undockedX < cave::VIEW_WIDTH && undockedY < cave::VIEW_HEIGHT &&
          static_cast<int16_t>(undockedX + game::kHullW) > 0 && static_cast<int16_t>(undockedY + game::kHullH) > 0) &&
         ok;

    Serial.println(ok ? F("P") : F("F"));
}

void loop() {
}

#else

Arduboy2Base arduboy;

void setup() {
    Serial.begin(9600);
    arduboy.begin();
    Serial.println(F("save disabled / skipped"));
    Serial.println('P');
}

void loop() {
}

#endif
