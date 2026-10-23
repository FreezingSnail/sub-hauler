// Sub-hauler — Arduboy FX.
//
// M3 station slice: world-aware submarine, HAULER silhouette, compact HUD, and
// the D-pad flight contract. Slow boats inside a settlement ring dock; the
// station board refills, repairs, and undocks. The sketch owns pilot state,
// the FX table reader, the visible-window cache, and the screen state machine;
// rules and drawing stay in src/ headers.

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

decltype(arduboy) arduboy;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

#if SH_FEATURE_STATIONS
enum class Screen : uint8_t {
    Pilot,
    Station
};

static Screen s_screen = Screen::Pilot;
static uint8_t s_stationRow = station::kFirstRow;
#endif

static void drawPilot() {
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    view::frame(s_state, camera, s_cache, s_tables);
    hud::draw(s_state);
}

// Flight frame: step once, then either redraw the cave or switch to the
// station board on the frame that docked.
static void updateFlight() {
    // D-pad only: left/right thrust, up blows ballast (rise), down floods
    // (sink). A/B stay free for station actions once docked.
    const game::Input input{arduboy.pressed(UP_BUTTON), arduboy.pressed(DOWN_BUTTON), arduboy.pressed(LEFT_BUTTON), arduboy.pressed(RIGHT_BUTTON)};

#if SH_FEATURE_STATIONS
    if (station::stepFlight(s_state, input, s_tables, &s_cache)) {
        s_screen = Screen::Station;
        s_stationRow = station::initialRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }
#else
    game::stepWorld(s_state, input, s_tables, &s_cache);
#endif

    drawPilot();
}

#if SH_FEATURE_STATIONS
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
#if SH_FEATURE_JOBS
        else if (action == station::kActionAccepted || action == station::kActionAbandoned) {
            // The board shape changed: offers -> summary or summary -> offers.
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
#endif

void setup() {
    arduboy.boot();
    arduboy.setFrameRate(60);
    FX::begin(FX_DATA_PAGE);
    FX::setCursorRange(0, 32767);

    world::clearCache(s_cache);
    game::resetWorld(s_state);
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();

#if SH_FEATURE_STATIONS
    if (s_screen == Screen::Station)
        updateStation();
    else
        updateFlight();
#else
    updateFlight();
#endif

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction.
    FX::display(CLEAR_BUFFER);
}
