// Shared save fixtures for the normal and emulator-only runtime-page suites.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/fxdata.h"

Arduboy2 arduboy;


#include "src/campaign.hpp"
#include "src/chart.hpp"
#include "src/game.hpp"
#include "src/voyage_save.hpp"

static game::State s_state;
static chart::State s_chart;
static voyage_save::FxStorage s_storage;

static bool saveNow() {
    return voyage_save::save(s_state, s_chart, s_storage) == voyage_save::kSaved;
}

static bool loadNow() {
    return voyage_save::load(s_state, s_chart, s_storage) == voyage_save::kLoaded;
}


static bool runSaveTests(bool runtimePage) {

    voyage_save::FxStorage::begin();
    FX::setCursorRange(0, 32767);

    bool ok = true;

    // Erased cart partition.
    ok = ok && !voyage_save::hasSave(s_storage);

    // First fresh voyage.
    chart::reset(s_chart);
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    ok = ok && saveNow();
    ok = ok && voyage_save::hasSave(s_storage);

    // Continue restores the berth checkpoint.
    game::reset(s_state);
    s_state.voyageTicks = 0;
    ok = ok && loadNow();
    ok = ok && s_state.docked == campaign::kTidewall;

    // Repeated cycles on the single slot.
    for (uint8_t i = 0; i < 8; ++i) {
        s_state.voyageTicks = static_cast<uint32_t>(100 + i);
        ok = ok && saveNow();
        ok = ok && voyage_save::hasSave(s_storage);
        ok = ok && loadNow();
        ok = ok && s_state.voyageTicks == static_cast<uint32_t>(100 + i);
    }

    if (runtimePage) {
        // Point programSavePage at the 4 KiB data tail immediately before the
        // generated save region, so seekSave/erase and page writes must agree
        // on a runtime page different from compile-time FX_SAVE_PAGE.
        FX::begin(FX_DATA_PAGE, static_cast<uint16_t>(FX_SAVE_PAGE - voyage_save::kPagesPerBlock));
        FX::eraseSaveBlock(0);
        FX::waitWhileBusy();
        ok = ok && !voyage_save::hasSave(s_storage);
        chart::reset(s_chart);
        game::reset(s_state);
        s_state.docked = static_cast<int8_t>(campaign::kTidewall);
        ok = ok && saveNow();
        ok = ok && voyage_save::hasSave(s_storage);
        ok = ok && loadNow();
    }


    return ok;
}
