// Sub-hauler — Arduboy FX.
//
// M3 station slice: world-aware submarine, HAULER silhouette, compact HUD, and
// the D-pad flight contract. Slow boats inside a settlement ring dock; the
// station board refills, repairs, and undocks. M6 adds a paused B-key chart
// with persistent fog exploration and the A-key sonar ping. The sketch owns
// pilot state, the FX table reader, the visible-window cache, and the screen
// state machine; rules and drawing stay in src/ headers.

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include "src/common.hpp"
#include "src/features.hpp"
#include "src/game.hpp"
#include "src/hud.hpp"
#include "src/pause.hpp"
#include "src/pilot.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"

#if SH_FEATURE_MULTI_HULL
#include "src/roster_view.hpp"
#endif

#if SH_FEATURE_STATIONS
#include "src/station.hpp"
#include "src/station_view.hpp"
#endif
#if SH_FEATURE_SHOP
#include "src/shop.hpp"
#include "src/shop_view.hpp"
#endif
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#endif
#if SH_FEATURE_SAVE_CHART
#include "src/chart_save.hpp"
#endif
#if SH_FEATURE_SONAR
#include "src/sonar.hpp"
#endif

decltype(arduboy) arduboy;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

// The screen tag exists whenever more than one screen can be reached. With
// stations, chart, multi-hull, and hull damage all off the pilot frame is the
// only path, matching the pre-feature image exactly.
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
enum class Screen : uint8_t {
#if SH_FEATURE_MULTI_HULL
    Title,
#endif
    Pilot,
    Pause,
#if SH_FEATURE_STATIONS
    Station,
#endif
#if SH_FEATURE_SHOP
    Shop,
#endif
#if SH_FEATURE_FOG_CHART
    Chart,
#endif
#if SH_FEATURE_HULL_DAMAGE
    Failure,
#endif
};

#if SH_FEATURE_MULTI_HULL
static Screen s_screen = Screen::Title;
#else
static Screen s_screen = Screen::Pilot;
#endif
#endif

#if SH_FEATURE_STATIONS
// The station row byte is screen-owned: station/shop use it for row selection.
// Every station and shop frame restores a valid row before drawing, so there is
// no extra selection state.
static uint8_t s_stationRow = station::kFirstRow;
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
// Static screens own a mutating buffer: they are composed once on entry and
// again only when an input edge can change what is on screen. s_present is
// cleared on idle frames, so the loop omits the OLED transaction too; the
// SSD1306 keeps the last image and the buffer is already clear from the
// previous display.
static bool s_present = true;
#endif

#if SH_FEATURE_FOG_CHART
static chart::State s_chart;
#endif

static void drawPilot() {
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    view::frame(s_state, camera, s_cache, s_tables);
    hud::draw(s_state);
}

// One boot/restart path. The setup call and the failure-screen restart share
// this function, so the full reset is linked once instead of being duplicated
// by the failure entry.
static void resetRun() {
    game::resetWorld(s_state);
#if SH_FEATURE_FOG_CHART
    chart::reset(s_chart);
    chart::revealInitial(s_chart, s_state);
#endif
}

#if SH_FEATURE_SAVE_CHART
// Load once from EEPROM during setup. A blank, corrupt, wrong-version, or
// wrong-checksum block is ignored and the reset chart stays empty; the loader
// validates the whole block before copying, so there is never a partial map.
static void loadChartSave() {
    chart_save::EepromStorage storage;
    chart_save::load(s_chart, storage);
}

// Persist only on the chart-close transition. Nothing in the per-frame flight,
// station, or display path calls this. EEPROM.update skips bytes whose value
// already matches, so repeat closes only pay for newly explored cells.
static void saveChartOnClose() {
    chart_save::EepromStorage storage;
    chart_save::save(s_chart, storage);
}
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
// Paused/failure frames deliberately draw no world and read no cart: the
// buffer is blank after the previous FX::display(CLEAR_BUFFER), so one short
// text line is enough at 128x64.
static void drawPause() {
    text::normal();
    text::at(10, 30);
    text::print(txt_paused);
}

#if SH_FEATURE_HULL_DAMAGE
static void drawFailure() {
    text::normal();
    text::at(4, 30);
    text::print(txt_hull_lost);
}
#endif
#endif

#if SH_FEATURE_MULTI_HULL
// Title frame: no world step. The only cart reads are the selected hull's
// baked sprite and the external labels, all before the display transaction.
// Left/right cycles the roster,
// A confirms and draws the first pilot frame, and B backs to the HAULER
// default. The selection lives in game::State, so it survives every
// dock/undock for the rest of the run.
static void updateTitle() {
    roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(s_state.hullClass));

    if (arduboy.justPressed(LEFT_BUTTON))
        hull = roster::previous(hull);
    if (arduboy.justPressed(RIGHT_BUTTON))
        hull = roster::next(hull);
    // B backs out of a non-default choice. Title is the root screen, so the
    // only sensible back target is the safe HAULER default.
    if (arduboy.justPressed(B_BUTTON))
        hull = roster::kDefault;

    s_state.hullClass = static_cast<uint8_t>(hull);

    if (arduboy.justPressed(A_BUTTON)) {
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }

    roster_view::draw(s_state.hullClass);
}
#endif

#if SH_FEATURE_FOG_CHART
// The chart is a paused map: cart water reads happen here, before the single
// display transaction, and no world step runs.
static void drawChart() {
    // The frame counter is the blink phase: no extra state, and a paused chart
    // still redraws deterministically from the frame number rather than the
    // frozen simulation clock.
    chart_view::draw(Arduboy2Base::sBuffer, s_chart, s_state, s_tables, static_cast<uint8_t>(Arduboy2Base::frameCount));
}
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
// Pause frame: no world step, no air/ballast drain, no job clock, and no cart
// reads. A or B resumes on the frozen pilot frame without stepping.
static void updatePause() {
    if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }
    s_present = false;
}

#if SH_FEATURE_HULL_DAMAGE
// Hull failure frame. A restarts the run: world, job/economy state, and the
// chart return to their boot values, so the player can never be stranded at
// zero hull. No extra save byte is involved.
static void updateFailure() {
    if (arduboy.justPressed(A_BUTTON)) {
        resetRun();
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }
    s_present = false;
}
#endif
#endif

// Flight frame: step once, then either redraw the cave or switch to the
// station board on the frame that docked.
static void updateFlight() {
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
    // A+B is the free-flight pause gesture. Require fresh edges: a held chord
    // must not re-enter Pause after a resume or station transition.
    if (pause::resolve(false, arduboy.justPressed(A_BUTTON), arduboy.justPressed(B_BUTTON)) ==
        pause::kEnter) {
        s_screen = Screen::Pause;
        drawPause();
        return;
    }
#endif
#if SH_FEATURE_FOG_CHART
    // B only opens the chart in free flight. Consume the edge before sampling
    // flight input so travel, air, and ballast do not tick on that frame.
    if (arduboy.justPressed(B_BUTTON)) {
        s_screen = Screen::Chart;
        drawChart();
        return;
    }
#endif

    // D-pad only: left/right thrust, up blows ballast (rise), down floods
    // (sink). A/B stay free for station actions once docked.
    const game::Input input{arduboy.pressed(UP_BUTTON), arduboy.pressed(DOWN_BUTTON), arduboy.pressed(LEFT_BUTTON), arduboy.pressed(RIGHT_BUTTON)};

#if SH_FEATURE_STATIONS
#if SH_FEATURE_FOG_CHART
    const bool docked = station::stepFlight(s_state, input, s_tables, &s_cache);
    // Coarse reveal rides flight ticks, including the step that docked.
    chart::revealOnStep(s_chart, s_state);
    if (docked) {
        s_screen = Screen::Station;
        s_stationRow = station::initialRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }
#else
    if (station::stepFlight(s_state, input, s_tables, &s_cache)) {
        s_screen = Screen::Station;
        s_stationRow = station::initialRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }
#endif
#else
    game::stepWorld(s_state, input, s_tables, &s_cache);
#if SH_FEATURE_FOG_CHART
    // Coarse reveal rides flight ticks.
    chart::revealOnStep(s_chart, s_state);
#endif
#endif

#if SH_FEATURE_HULL_DAMAGE
    // Zero hull ends free flight immediately. A docking step above already
    // serviced the boat and returned to the station board, so this can only
    // fire on an undocked wreck.
    if (damage::lostInFlight(s_state)) {
        s_screen = Screen::Failure;
        drawFailure();
        return;
    }
#endif

#if SH_FEATURE_SONAR
    // A is the free-flight sonar ping. The chart branch above returns before
    // this point and a docking step returns before this point; fire() rejects
    // a docked state as well, so the action stays out of both screens. The
    // shop passes the purchased level; a SHOP=0 build passes 0.
#if SH_FEATURE_SHOP
    const uint8_t sonarLevel = s_state.sonarLevel;
#else
    const uint8_t sonarLevel = 0;
#endif
    if (arduboy.justPressed(A_BUTTON))
        sonar::fire(s_state, s_chart, sonarLevel);
#endif

    drawPilot();
}

#if SH_FEATURE_FOG_CHART
// Chart frame: no world step, so travel time/air/ballast/jobClock are frozen
// and D-pad/A are contained. B closes. The destination X blinks from the
// Arduboy frame counter because game::State.ticks cannot advance here.
static void updateChart() {
    if (arduboy.justPressed(B_BUTTON)) {
#if SH_FEATURE_SAVE_CHART
        saveChartOnClose();
#endif
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }

    drawChart();
}
#endif

#if SH_FEATURE_STATIONS
// Station frame: no world step, so travel time, air, and ballast are frozen.
// D-pad moves the selection, A confirms, B always undocks.
static void updateStation() {
    if (!arduboy.justPressed(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (arduboy.justPressed(UP_BUTTON))
        s_stationRow = station::selectPrevious(s_stationRow, s_state);
    if (arduboy.justPressed(DOWN_BUTTON))
        s_stationRow = station::selectNext(s_stationRow, s_state);

    if (arduboy.justPressed(A_BUTTON)) {
        const station::Action action = station::activate(s_stationRow, s_state);
        if (action == station::kActionUndocked) {
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
        }
#if SH_FEATURE_SHOP
        else if (action == station::kActionShop) {
            // The shop borrows s_stationRow as its 0..2 item selection.
            s_screen = Screen::Shop;
            s_stationRow = shop::kFirstItem;
            shop_view::draw(s_state, s_stationRow);
            return;
        }
#endif
#if SH_FEATURE_JOBS
        else if (action == station::kActionAbandoned) {
            // The board shape changed: summary -> offers.
            s_stationRow = station::initialRow(s_state);
        } else if (action == station::kActionAccepted) {
            // Acceptance auto-loads the contract hold; the boat stays docked
            // until the player confirms UNDOCK or presses B.
            s_stationRow = station::initialRow(s_state);
        }
#endif
    }

    if (arduboy.justPressed(B_BUTTON)) {
        if (station::undock(s_state)) {
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
        }
    }

    if (s_screen == Screen::Station) {
        station_view::draw(s_state, s_stationRow);
        return;
    }

    // A or B undocked this frame: redraw the frozen pilot view without
    // stepping, so the first visible frame after undock is the station berth.
    drawPilot();
}

#if SH_FEATURE_SHOP
// Shop frame: no world step and no cart reads. D-pad selects an item, A buys
// through the pure transaction, and B backs to the station board. The selected
// row is the reused station row byte; the shop has no other static state.
static void updateShop() {
    if (!arduboy.justPressed(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (arduboy.justPressed(UP_BUTTON))
        s_stationRow = shop::selectPrevious(s_stationRow);
    if (arduboy.justPressed(DOWN_BUTTON))
        s_stationRow = shop::selectNext(s_stationRow);

    if (arduboy.justPressed(A_BUTTON))
        shop::buy(s_state, static_cast<shop::Item>(s_stationRow));

    if (arduboy.justPressed(B_BUTTON)) {
        s_screen = Screen::Station;
        s_stationRow = station::shopRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }

    shop_view::draw(s_state, s_stationRow);
}
#endif
#endif

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);
    text::begin();

    resetRun();

#if SH_FEATURE_SAVE_CHART
    // Restore the explored set once at boot; resetRun() already revealed the
    // spawn, and a valid save overwrites that with the exact stored mask.
    loadChartSave();
#endif
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
    s_present = true;
#endif

#if SH_FEATURE_MULTI_HULL
    // Title boots the run and owns hull selection. It has no world step and no
    // cart reads; A draws the first pilot frame before the display transaction.
    if (s_screen == Screen::Title)
        updateTitle();
    else
#endif
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
    if (s_screen == Screen::Pause)
        updatePause();
#if SH_FEATURE_HULL_DAMAGE
    else if (s_screen == Screen::Failure)
        updateFailure();
#endif
#if SH_FEATURE_FOG_CHART
    // Chart is a pilot sub-screen, so the dispatch gains one branch while the
    // station chain keeps its shape.
    else if (s_screen == Screen::Pilot)
        updateFlight();
#if SH_FEATURE_STATIONS
    else if (s_screen == Screen::Station)
        updateStation();
#endif
#if SH_FEATURE_SHOP
    else if (s_screen == Screen::Shop)
        updateShop();
#endif
    else if (s_screen == Screen::Chart)
        updateChart();
#else
#if SH_FEATURE_STATIONS
    else if (s_screen == Screen::Station)
        updateStation();
#if SH_FEATURE_SHOP
    else if (s_screen == Screen::Shop)
        updateShop();
#endif
#endif
    else
        updateFlight();
#endif
#else
    updateFlight();
#endif

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction. A static screen with no input
    // edge clears s_present and keeps the previous panel image; the buffer is
    // already clear from the last display.
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE
    if (s_present)
#endif
        FX::display(CLEAR_BUFFER);
}
