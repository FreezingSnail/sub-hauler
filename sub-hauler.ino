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
#include "src/pilot.hpp"
#include "src/render.hpp"
#include "src/world_fx.hpp"

#if SH_FEATURE_STATIONS
#include "src/station.hpp"
#include "src/station_view.hpp"
#endif
#if SH_FEATURE_SHOP
#include "src/shop.hpp"
#include "src/shop_view.hpp"
#endif
#if SH_FEATURE_CARGO_LOAD
#include "src/cargo_fx.hpp"
#include "src/cargo_handoff.hpp"
#include "src/cargo_screen.hpp"
#include "src/cargo_view.hpp"
#endif
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#endif
#if SH_FEATURE_SONAR
#include "src/sonar.hpp"
#endif

decltype(arduboy) arduboy;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

// The screen tag exists whenever a second screen can be reached. With both
// station and chart features off the pilot frame is the only path, matching the
// pre-feature image exactly.
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART
enum class Screen : uint8_t {
    Pilot,
#if SH_FEATURE_STATIONS
    Station,
#endif
#if SH_FEATURE_SHOP
    Shop,
#endif
#if SH_FEATURE_CARGO_LOAD
    Cargo,
#endif
#if SH_FEATURE_FOG_CHART
    Chart,
#endif
};

static Screen s_screen = Screen::Pilot;
#endif

#if SH_FEATURE_STATIONS
static uint8_t s_stationRow = station::kFirstRow;
#if SH_FEATURE_CARGO_LOAD
static cargo::FxCargoReader s_cargoReader;
static cargo_screen::State s_cargo;
static uint8_t s_cargoBHold = 0;
constexpr uint8_t kCargoExitHold = 40; // 0.67 s at 60 fps
#endif
#endif

#if SH_FEATURE_FOG_CHART
static chart::State s_chart;
static uint8_t s_chartBlink = 0;
#endif

static void drawPilot() {
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    view::frame(s_state, camera, s_cache, s_tables);
    hud::draw(s_state);
}

#if SH_FEATURE_FOG_CHART
// The chart is a paused map: cart water reads happen here, before the single
// display transaction, and no world step runs.
static void drawChart() {
    chart_view::draw(Arduboy2Base::sBuffer, s_chart, s_state, s_tables, s_chartBlink);
}
#endif

// Flight frame: step once, then either redraw the cave or switch to the
// station board on the frame that docked.
static void updateFlight() {
#if SH_FEATURE_FOG_CHART
    // B only opens the chart in free flight. Consume the edge before sampling
    // flight input so travel, air, and ballast do not tick on that frame.
    if (arduboy.justPressed(B_BUTTON)) {
        s_chartBlink = 0;
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
// and D-pad/A are contained. B closes. The blink counter is frame-owned
// because game::State.ticks cannot advance while the chart is open.
static void updateChart() {
    if (arduboy.justPressed(B_BUTTON)) {
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }

    ++s_chartBlink;
    drawChart();
}
#endif

#if SH_FEATURE_STATIONS
#if SH_FEATURE_CARGO_LOAD
// Load HAULER arrangement 0 and switch to the cargo screen. The cart read
// happens here in update, before the single display transaction. Failure
// leaves the station board selected on LOAD CARGO so A retries.
static bool enterCargo() {
    if (!cargo_screen::load(s_cargo, s_cargoReader))
        return false;
    s_screen = Screen::Cargo;
    s_cargoBHold = 0;
    cargo_view::draw(s_cargo.puzzle, s_cargo.selected, s_cargo.message);
    return true;
}
#endif

// Station frame: no world step, so travel time, air, and ballast are frozen.
// D-pad moves the selection, A confirms, B always undocks.
static void updateStation() {
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
        }
#if SH_FEATURE_CARGO_LOAD
        else if (action == station::kActionAccepted || action == station::kActionLoadCargo) {
            // Acceptance opens the loading screen with an empty hold; LOAD
            // CARGO reopens it for an already-active contract.
            if (enterCargo())
                return;
            s_stationRow = station::kLoadRow;
        }
#else
        else if (action == station::kActionAccepted) {
            s_stationRow = station::initialRow(s_state);
        }
#endif
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

#if SH_FEATURE_CARGO_LOAD
// Cargo frame: fixed state is loaded once on entry, no world step. A/B and the
// D-pad drive the pure cargo_screen rules. B taps rotate (or undo with nothing
// held); holding B for kCargoExitHold returns to the station board with the job
// open. A+B with nothing held is LOAD & UNDOCK. R/N are omitted because the
// Arduino pad has no spare key.
static void updateCargo() {
    if (arduboy.pressed(B_BUTTON)) {
        if (s_cargoBHold < 255)
            ++s_cargoBHold;
    } else {
        s_cargoBHold = 0;
    }

    if (s_cargoBHold >= kCargoExitHold) {
        s_cargoBHold = 0;
        s_screen = Screen::Station;
        // Back/cancel leaves the job open and selects the safe LOAD CARGO row.
        s_stationRow = station::initialRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }

    cargo_screen::Input input{};
    input.up = arduboy.justPressed(UP_BUTTON);
    input.down = arduboy.justPressed(DOWN_BUTTON);
    input.left = arduboy.justPressed(LEFT_BUTTON);
    input.right = arduboy.justPressed(RIGHT_BUTTON);
    input.a = arduboy.justPressed(A_BUTTON);
    input.b = arduboy.justPressed(B_BUTTON);
    input.ccw = input.b && arduboy.pressed(UP_BUTTON);
    if (input.ccw)
        input.up = false; // UP is a rotation modifier on this frame

    // LOAD & UNDOCK: A+B with nothing held. Routed before cargo_screen::apply
    // so it cannot be mistaken for pick/release/rotate/undo. A short load is
    // rejected with LOAD SHORT and the job stays open.
    if (cargo_handoff::commitPressed(input) && s_cargo.puzzle.held < 0) {
        const cargo_handoff::Result result = cargo_handoff::commit(s_state, s_cargo);
        if (result == cargo_handoff::kCommitted) {
            s_cargoBHold = 0;
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
            drawPilot();
            return;
        }
        if (result == cargo_handoff::kIncomplete) {
            cargo_view::draw(s_cargo.puzzle, s_cargo.selected, s_cargo.message);
            return;
        }
    }

    cargo_screen::apply(s_cargo, input);

    cargo_view::draw(s_cargo.puzzle, s_cargo.selected, s_cargo.message);
}
#endif
#endif

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    world::clearCache(s_cache);
    game::resetWorld(s_state);
#if SH_FEATURE_FOG_CHART
    chart::reset(s_chart);
    chart::revealInitial(s_chart, s_state);
#endif
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();

#if SH_FEATURE_FOG_CHART
    // Chart is a pilot sub-screen, so the dispatch gains one branch while the
    // station/cargo chain keeps its shape.
    if (s_screen == Screen::Pilot)
        updateFlight();
#if SH_FEATURE_STATIONS
    else if (s_screen == Screen::Station)
        updateStation();
#endif
#if SH_FEATURE_SHOP
    else if (s_screen == Screen::Shop)
        updateShop();
#endif
#if SH_FEATURE_CARGO_LOAD
    else if (s_screen == Screen::Cargo)
        updateCargo();
#endif
    else if (s_screen == Screen::Chart)
        updateChart();
#else
#if SH_FEATURE_STATIONS
    if (s_screen == Screen::Station)
        updateStation();
#if SH_FEATURE_SHOP
    else if (s_screen == Screen::Shop)
        updateShop();
#endif
#if SH_FEATURE_CARGO_LOAD
    else if (s_screen == Screen::Cargo)
        updateCargo();
#endif
    else
        updateFlight();
#else
    updateFlight();
#endif
#endif

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction.
    FX::display(CLEAR_BUFFER);
}
