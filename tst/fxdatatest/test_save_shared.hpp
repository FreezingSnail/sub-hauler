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

static void blankPage(uint8_t *page) {
    for (uint16_t i = 0; i < voyage_save::kPageBytes; ++i)
        page[i] = 0xFF;
}

// Clear the commit byte with a page program, leaving magic/payload intact.
static void interruptCommit() {
    uint8_t page[voyage_save::kPageBytes];
    blankPage(page);
    page[255] = 0x00;
    FX::writeSavePage(1, page);
    FX::waitWhileBusy();
}

// Clear page 0 with a page program; the commit stays 0x5A, so the slot is
// corrupt rather than blank.
static void corruptPageZero() {
    uint8_t page[voyage_save::kPageBytes];
    for (uint16_t i = 0; i < voyage_save::kPageBytes; ++i)
        page[i] = 0x00;
    FX::writeSavePage(0, page);
    FX::waitWhileBusy();
}

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
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotBlank;

    // First fresh voyage.
    chart::reset(s_chart);
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    ok = ok && saveNow();
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotValid;

    // Continue restores the berth checkpoint.
    game::reset(s_state);
    s_state.credits = 0;
    ok = ok && loadNow();
    ok = ok && s_state.docked == campaign::kTidewall;

    // Repeated cycles on the single slot.
    for (uint8_t i = 0; i < 8; ++i) {
        s_state.credits = static_cast<uint16_t>(100 + i);
        ok = ok && saveNow();
        ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotValid;
        ok = ok && loadNow();
        ok = ok && s_state.credits == static_cast<uint16_t>(100 + i);
    }

    // Interrupted commit: magic and payload present, commit cleared. Never load.
    const uint16_t beforeCredits = s_state.credits;
    interruptCommit();
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotCorrupt;
    ok = ok && !loadNow() && s_state.credits == beforeCredits;

    // Clean New Voyage recovers from the interrupted commit.
    chart::reset(s_chart);
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    ok = ok && saveNow();
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotValid;

    // Corrupt payload/magic with a valid commit is rejected, never loaded.
    corruptPageZero();
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotCorrupt;
    ok = ok && !loadNow();

    // A further New Voyage replaces the corrupt record cleanly.
    chart::reset(s_chart);
    game::reset(s_state);
    s_state.docked = static_cast<int8_t>(campaign::kTidewall);
    ok = ok && saveNow();
    ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotValid;
    ok = ok && loadNow();

    if (runtimePage) {
        // Point programSavePage at the 4 KiB data tail immediately before the
        // generated save region, so seekSave/erase and page writes must agree
        // on a runtime page different from compile-time FX_SAVE_PAGE.
        FX::begin(FX_DATA_PAGE, static_cast<uint16_t>(FX_SAVE_PAGE - voyage_save::kPagesPerBlock));
        FX::eraseSaveBlock(0);
        FX::waitWhileBusy();
        ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotBlank;
        chart::reset(s_chart);
        game::reset(s_state);
        s_state.docked = static_cast<int8_t>(campaign::kTidewall);
        ok = ok && saveNow();
        ok = ok && voyage_save::inspect(s_storage).status == voyage_save::kSlotValid;
        ok = ok && loadNow();
    }


    return ok;
}
