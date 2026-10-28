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
#if SH_DEV_MODE
#include "src/dev_mode.hpp"
#endif

#if SH_FEATURE_MULTI_HULL
#include "src/roster_view.hpp"
#endif

#if SH_FEATURE_STATIONS
#include "src/station.hpp"
#include "src/station_view.hpp"
#endif
#if SH_FEATURE_CAMPAIGN
#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/ending_view.hpp"
#endif
#if SH_FEATURE_SHOP
#include "src/shop.hpp"
#include "src/shop_view.hpp"
#endif
#if SH_FEATURE_FOG_CHART
#include "src/chart.hpp"
#include "src/chart_view.hpp"
#endif
#if SH_FEATURE_SAVE_CHART && !SH_DEV_MODE
#include "src/chart_save.hpp"
#endif
#if SH_FEATURE_SONAR
#include "src/sonar.hpp"
#endif

#if SH_FEATURE_SAVE_VOYAGE && !SH_DEV_MODE
#define SH_VOYAGE_UI 1
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"
#else
#define SH_VOYAGE_UI 0
#endif

decltype(arduboy) arduboy;

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

// The screen tag exists whenever more than one screen can be reached. With
// stations, chart, multi-hull, and hull damage all off the pilot frame is the
// only path, matching the pre-feature image exactly.
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
enum class Screen : uint8_t {
#if SH_FEATURE_MULTI_HULL || SH_VOYAGE_UI
    Title,
#endif
    Pilot,
    Pause,
#if SH_FEATURE_STATIONS
    Station,
#endif
#if SH_FEATURE_CAMPAIGN
    Work,
    Ending,
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
#if SH_VOYAGE_UI
    SaveFailed,
#endif
};

#if SH_VOYAGE_UI
static Screen s_screen = Screen::Title;
#elif SH_FEATURE_MULTI_HULL
#if SH_DEV_MODE
static Screen s_screen = Screen::Pilot;
#else
static Screen s_screen = Screen::Title;
#endif
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

#if SH_FEATURE_CAMPAIGN
// WORK page selection. The compact mission list is derived from saved flags;
// only the detail toggle and selected id are transient.
static uint8_t s_workRow = 0;
static uint8_t s_workMission = campaign::kMissionNone;
static bool s_workDetail = false;
static uint8_t s_endingCard = 0;
#endif

#if SH_VOYAGE_UI
// Save integration state. Writes are synchronous transitions; while the save
// failure frame is showing the world is frozen and departure input is ignored.
enum SaveResume : uint8_t {
    kSaveResumeStation,
    kSaveResumeShop,
    kSaveResumePilot,
};

static voyage_flow::BootStatus s_bootStatus;
static voyage_view::TitleMode s_titleMode = voyage_view::kTitleMenu;
static uint8_t s_titleRow = voyage_view::kTitleNewVoyage;
static bool s_voyageDirty = false;
static SaveResume s_saveResume = kSaveResumeStation;
static uint8_t s_saveRow = 0;
static uint8_t s_pauseRow = 0;
static bool s_pauseConfirm = false;
static uint8_t s_failureRow = 0;
static bool s_abandonConfirm = false;
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
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

#if SH_FEATURE_STATIONS
static void showPilot() {
    s_screen = Screen::Pilot;
    s_stationRow = station::kFirstRow;
    drawPilot();
}

static void showStation() {
    s_stationRow = station::initialRow(s_state);
    s_screen = Screen::Station;
    station_view::draw(s_state, s_stationRow);
}
#endif

// One boot/restart path. The setup call and the failure-screen restart share
// this function, so the full reset is linked once instead of being duplicated
// by the failure entry.
static void resetRun() {
    game::resetWorld(s_state);
#if SH_DEV_MODE
    dev_mode::prepare(s_state);
#endif
#if SH_FEATURE_FOG_CHART
    chart::reset(s_chart);
#if SH_DEV_MODE
    dev_mode::revealChart(s_chart);
#else
    chart::revealInitial(s_chart, s_state);
#endif
#endif
}

#if SH_FEATURE_SAVE_CHART && !SH_DEV_MODE
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

#if SH_VOYAGE_UI
// One checkpoint transaction. Storage only touches EEPROM bytes 16..655; the
// caller owns the UI decision after the result. The write is synchronous, so
// no world step, chart read or departure input can run during it.
static voyage_save::SaveResult saveVoyageNow() {
    voyage_save::EepromStorage storage;
    return voyage_flow::saveCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                       ,
                                       s_chart
#endif
                                       ,
                                       storage);
}

static void beginSaveFailure(SaveResume resume) {
    s_saveResume = resume;
    s_saveRow = 0;
    s_voyageDirty = true;
    s_screen = Screen::SaveFailed;
    voyage_view::drawSaveFailed(0);
}

// Report SAVED only after read-back validation. A failed write keeps the live
// state and the previous checkpoint intact, then freezes on RETRY/CONTINUE
// UNSAVED. No caller may show a success notice before this returns true.
static bool requestSave(SaveResume resume) {
    if (saveVoyageNow() == voyage_save::kSaved) {
        s_voyageDirty = false;
        return true;
    }
    beginSaveFailure(resume);
    return false;
}
#endif   // SH_VOYAGE_UI

#if SH_FEATURE_CAMPAIGN
// HOME is paid and committed by the normal dock checkpoint before any card is
// shown. The ending-presented flag is separate: cards can be replayed from
// Tidewall WORK, and a reload with HOME complete but ending still zero resumes
// the cards without paying again.
static void drawEnding() {
    ending_view::draw(s_endingCard, s_state);
}

static bool endingPending() {
    return s_state.ending == 0 && campaign::missionCompleted(s_state, campaign::kHome);
}

static void beginEnding() {
    s_endingCard = 0;
    s_screen = Screen::Ending;
    drawEnding();
}

static void finishEnding() {
    if (s_state.ending == 0) {
        campaign::markEndingPresented(s_state);
#if SH_VOYAGE_UI
        if (!requestSave(kSaveResumeStation))
            return;
#endif
    }
    showStation();
}

static void updateEnding() {
    if (!arduboy.justPressed(A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }
    const uint8_t next = ending_view::nextCard(s_endingCard);
    if (next >= ending_view::kCardCount) {
        finishEnding();
        return;
    }
    s_endingCard = next;
    drawEnding();
}
#endif

#if SH_VOYAGE_UI
static void finishSaveResume() {
    switch (s_saveResume) {
    case kSaveResumePilot:
        if (s_state.docked >= 0)
            station::undock(s_state);
        showPilot();
        return;
#if SH_FEATURE_SHOP
    case kSaveResumeShop:
        s_screen = Screen::Shop;
        if (s_stationRow >= shop::kItemCount)
            s_stationRow = shop::kFirstItem;
        shop_view::draw(s_state, s_stationRow);
        return;
#endif
    default:
#if SH_FEATURE_CAMPAIGN
        if (endingPending()) {
            beginEnding();
            return;
        }
#endif
        showStation();
        return;
    }
}

static void drawTitleScreen() {
    s_screen = Screen::Title;
#if SH_FEATURE_MULTI_HULL
    const uint8_t hullClass = s_state.hullClass;
#else
    const uint8_t hullClass = 0;
#endif
    voyage_view::drawTitle(s_bootStatus, s_titleRow, hullClass, s_titleMode);
}

static void refreshBootStatus() {
    voyage_save::EepromStorage storage;
    s_bootStatus = voyage_flow::inspectBoot(storage);
    s_titleMode = voyage_view::kTitleMenu;
    s_titleRow = s_bootStatus.hasValid ? voyage_view::kTitleContinue : voyage_view::kTitleNewVoyage;
}

#if SH_FEATURE_FOG_CHART
static bool legacyChartAvailable() {
    if (s_bootStatus.needsReplaceConfirm())
        return false;
    voyage_save::EepromStorage storage;
    return voyage_save::legacyChartValid(storage);
}
#endif

static void startNewVoyage(voyage_save::LegacyChart legacy) {
    voyage_save::EepromStorage storage;
    const voyage_save::SaveResult result = voyage_flow::startNewVoyage(s_state
#if SH_FEATURE_FOG_CHART
                                                                       ,
                                                                       s_chart
#endif
                                                                       ,
                                                                       legacy, storage);
#if SH_FEATURE_FOG_CHART
    // New voyage starts with the spawn cell revealed. The first checkpoint was
    // already committed, so keep the difference dirty and let the next berth
    // transition persist it.
    chart::revealInitial(s_chart, s_state);
#endif
    s_stationRow = station::initialRow(s_state);
    s_screen = Screen::Station;
    if (result == voyage_save::kSaved) {
        s_voyageDirty =
#if SH_FEATURE_FOG_CHART
            true;
#else
            false;
#endif
        station_view::draw(s_state, s_stationRow);
    } else {
        beginSaveFailure(kSaveResumeStation);
    }
}

static void continueVoyage() {
    voyage_save::EepromStorage storage;
    const voyage_save::LoadResult result = voyage_flow::loadCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                                                       ,
                                                                       s_chart
#endif
                                                                       ,
                                                                       storage);
    if (result != voyage_save::kLoaded) {
        refreshBootStatus();
        drawTitleScreen();
        return;
    }
    s_voyageDirty = false;
#if SH_FEATURE_CAMPAIGN
    // A reload between the committed HOME delivery and its cards resumes the
    // ending. Payment and the HOME milestone are already in the checkpoint.
    if (endingPending()) {
        beginEnding();
        return;
    }
#endif
    showStation();
}

// Free rescue and wreck recovery share one path. The complete checkpoint is
// restored first, then the rescue count is committed before the station board
// resumes. A failed commit keeps the restored state live and asks honestly.
static void performRecovery() {
    voyage_save::EepromStorage storage;
    const voyage_flow::Recovery result = voyage_flow::recoverCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                                                        ,
                                                                        s_chart
#endif
                                                                        ,
                                                                        storage);
    if (result.load != voyage_save::kLoaded) {
        refreshBootStatus();
        drawTitleScreen();
        return;
    }
#if SH_FEATURE_CAMPAIGN
    if (endingPending()) {
        beginEnding();
        return;
    }
#endif
    s_stationRow = station::initialRow(s_state);
    s_screen = Screen::Station;
    if (result.save == voyage_save::kSaved) {
        s_voyageDirty = false;
        station_view::draw(s_state, s_stationRow);
    } else {
        beginSaveFailure(kSaveResumeStation);
    }
}
#endif   // SH_VOYAGE_UI


#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
// Paused/failure frames deliberately draw no world and read no cart: the
// buffer is blank after the previous FX::display(CLEAR_BUFFER), so one short
// text line is enough at 128x64.
static void drawPause() {
#if SH_VOYAGE_UI
    if (s_pauseConfirm)
        voyage_view::drawRescueConfirm();
    else
        voyage_view::drawPauseRescue(s_pauseRow);
#else
    text::normal();
    text::at(10, 30);
    text::print(txt_paused);
#endif
}

#if SH_FEATURE_HULL_DAMAGE
static void drawFailure() {
#if SH_VOYAGE_UI
    voyage_view::drawWreck(s_failureRow);
#else
    text::normal();
    text::at(4, 30);
    text::print(txt_hull_lost);
#endif
}
#endif
#endif

#if SH_VOYAGE_UI
// Title with CONTINUE/NEW VOYAGE. No world step and no cart reads. CONTINUE is
// only selectable for a valid checkpoint; NEW VOYAGE confirms before replacing
// any present (valid, corrupt or incompatible) record. On the first blank
// voyage a valid legacy chart offers to be kept.
static void updateTitle() {
    if (s_titleMode == voyage_view::kTitleConfirmReplace) {
        if (arduboy.justPressed(A_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            startNewVoyage(voyage_save::kDropLegacyChart);
            return;
        }
        if (arduboy.justPressed(B_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            drawTitleScreen();
            return;
        }
        s_present = false;
        return;
    }

    if (s_titleMode == voyage_view::kTitleKeepChart) {
        if (arduboy.justPressed(A_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            startNewVoyage(voyage_save::kKeepLegacyChart);
            return;
        }
        if (arduboy.justPressed(B_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            startNewVoyage(voyage_save::kDropLegacyChart);
            return;
        }
        s_present = false;
        return;
    }

#if SH_FEATURE_MULTI_HULL
    roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(s_state.hullClass));
    if (arduboy.justPressed(LEFT_BUTTON))
        hull = roster::previous(hull);
    if (arduboy.justPressed(RIGHT_BUTTON))
        hull = roster::next(hull);
    if (arduboy.justPressed(B_BUTTON))
        hull = roster::kDefault;
    s_state.hullClass = static_cast<uint8_t>(hull);
#endif

    if (arduboy.justPressed(UP_BUTTON))
        s_titleRow = voyage_view::previousRow(s_titleRow, s_bootStatus);
    if (arduboy.justPressed(DOWN_BUTTON))
        s_titleRow = voyage_view::nextRow(s_titleRow, s_bootStatus);

    if (arduboy.justPressed(A_BUTTON)) {
        if (s_titleRow == voyage_view::kTitleContinue && s_bootStatus.hasValid) {
            continueVoyage();
            return;
        }
        if (s_bootStatus.needsReplaceConfirm())
            s_titleMode = voyage_view::kTitleConfirmReplace;
#if SH_FEATURE_FOG_CHART
        else if (legacyChartAvailable())
            s_titleMode = voyage_view::kTitleKeepChart;
#endif
        else {
            startNewVoyage(voyage_save::kDropLegacyChart);
            return;
        }
    }

    drawTitleScreen();
}
#elif SH_FEATURE_MULTI_HULL
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
        showPilot();
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
    // redraws on blink changes from the frame number rather than the frozen
    // simulation clock.
    chart_view::draw(Arduboy2Base::sBuffer, s_chart, s_state, s_tables, static_cast<uint8_t>(Arduboy2Base::frameCount));
}
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
// Pause frame: no world step, no air/ballast drain, no job clock, and no cart
// reads. With voyage saving the pause menu offers RESUME or a confirmed free
// rescue; otherwise A or B resumes on the frozen pilot frame without stepping.
static void updatePause() {
#if SH_VOYAGE_UI
    if (s_pauseConfirm) {
        if (arduboy.justPressed(A_BUTTON)) {
            s_pauseConfirm = false;
            performRecovery();
            return;
        }
        if (arduboy.justPressed(B_BUTTON)) {
            s_pauseConfirm = false;
            voyage_view::drawPauseRescue(s_pauseRow);
            return;
        }
        s_present = false;
        return;
    }

    if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
        s_pauseRow = s_pauseRow == 0 ? 1 : 0;
        voyage_view::drawPauseRescue(s_pauseRow);
        return;
    }

    if (arduboy.justPressed(A_BUTTON)) {
        if (s_pauseRow == 0) {
            s_screen = Screen::Pilot;
            drawPilot();
        } else {
            s_pauseConfirm = true;
            voyage_view::drawRescueConfirm();
        }
        return;
    }
    if (arduboy.justPressed(B_BUTTON)) {
        showPilot();
        return;
    }
    s_present = false;
#else
    if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
        showPilot();
        return;
    }
    s_present = false;
#endif
}

#if SH_FEATURE_HULL_DAMAGE
// Hull failure frame. With voyage saving, RETRY BERTH restores and commits the
// last checkpoint for free; TITLE leaves the wreck for CONTINUE/NEW VOYAGE.
// Without voyage saving, A restarts the run as before.
static void updateFailure() {
#if SH_VOYAGE_UI
    if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
        s_failureRow = s_failureRow == 0 ? 1 : 0;
        voyage_view::drawWreck(s_failureRow);
        return;
    }
    if (arduboy.justPressed(A_BUTTON)) {
        if (s_failureRow == 0) {
            performRecovery();
        } else {
            refreshBootStatus();
            drawTitleScreen();
        }
        return;
    }
    s_present = false;
#else
    if (arduboy.justPressed(A_BUTTON)) {
        resetRun();
        showPilot();
        return;
    }
    s_present = false;
#endif
}
#endif

#if SH_VOYAGE_UI
// SAVE FAILED freezes the world. RETRY re-runs the same checkpoint transaction;
// CONTINUE UNSAVED resumes the already-applied live state and keeps it dirty,
// so the HUD/title never claim it is safely stored.
static void updateSaveFailed() {
    if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
        s_saveRow = s_saveRow == 0 ? 1 : 0;
        voyage_view::drawSaveFailed(s_saveRow);
        return;
    }
    if (arduboy.justPressed(A_BUTTON)) {
        if (s_saveRow == 0) {
            if (saveVoyageNow() == voyage_save::kSaved) {
                s_voyageDirty = false;
                finishSaveResume();
            } else {
                voyage_view::drawSaveFailed(s_saveRow);
            }
        } else {
            s_voyageDirty = true;
            finishSaveResume();
        }
        return;
    }
    s_present = false;
}
#endif
#endif

// Flight frame: step once, then either redraw the cave or switch to the
// station board on the frame that docked.
static void updateFlight() {
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    // A+B is the free-flight pause gesture. Require fresh edges: a held chord
    // must not re-enter Pause after a resume or station transition.
    if (pause::resolve(false, arduboy.justPressed(A_BUTTON), arduboy.justPressed(B_BUTTON)) ==
        pause::kEnter) {
        s_screen = Screen::Pause;
#if SH_VOYAGE_UI
        s_pauseRow = 0;
        s_pauseConfirm = false;
#endif
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

#if SH_DEV_MODE
    dev_mode::step(s_state, input, arduboy.pressed(A_BUTTON));
    drawPilot();
#else
#if SH_FEATURE_STATIONS
#if SH_FEATURE_FOG_CHART
    const bool docked = station::stepFlight(s_state, input, s_tables, &s_cache);
    // Coarse reveal rides flight ticks, including the step that docked.
    chart::revealOnStep(s_chart, s_state);
    if (docked) {
        s_stationRow = station::initialRow(s_state);
#if SH_VOYAGE_UI
        // Delivery/repair/service are resolved inside the dock step. Commit
        // that complete post-delivery checkpoint before any board celebration
        // or further input. A failed write stays frozen on the save prompt.
        if (!requestSave(kSaveResumeStation))
            return;
#endif
#if SH_FEATURE_CAMPAIGN
        if (endingPending()) {
            beginEnding();
            return;
        }
#endif
        showStation();
        return;
    }
#else
    if (station::stepFlight(s_state, input, s_tables, &s_cache)) {
        s_stationRow = station::initialRow(s_state);
#if SH_VOYAGE_UI
        if (!requestSave(kSaveResumeStation))
            return;
#endif
#if SH_FEATURE_CAMPAIGN
        if (endingPending()) {
            beginEnding();
            return;
        }
#endif
        showStation();
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
#endif // SH_DEV_MODE
}

#if SH_FEATURE_FOG_CHART
// Chart frame: no world step, so travel time/air/ballast/jobClock are frozen
// and D-pad/A are contained. B closes. The destination X blinks from the
// Arduboy frame counter because game::State.ticks cannot advance here.
static void updateChart() {
    if (arduboy.justPressed(B_BUTTON)) {
#if SH_FEATURE_SAVE_CHART && !SH_DEV_MODE
        saveChartOnClose();
#endif
        showPilot();
        return;
    }

    if (chart_view::needsRedraw(s_state, static_cast<uint8_t>(Arduboy2Base::frameCount)))
        drawChart();
    else
        s_present = false;
}
#endif

#if SH_FEATURE_STATIONS
#if SH_VOYAGE_UI
// Departure is a save boundary only when the live state is dirty (for example
// after CONTINUE UNSAVED on an earlier failed write). Save while still docked:
// a checkpoint is never midwater. A failed write freezes on the prompt instead
// of leaving the berth.
static void leaveStation() {
    if (s_voyageDirty && !requestSave(kSaveResumePilot))
        return;
    if (station::undock(s_state)) {
        showPilot();
    }
}
#endif

#if SH_FEATURE_CAMPAIGN
static uint8_t workVisibleCount() {
    uint8_t count = 0;
    while (count < campaign_view::kWorkRows && campaign::workMissionAt(s_state, static_cast<uint8_t>(s_state.docked), count) != campaign::kMissionNone)
        ++count;
    return count;
}

static void drawWork() {
    if (s_workDetail)
        campaign_view::drawWorkDetail(s_state, s_workMission);
    else
        campaign_view::drawWorkList(s_state, s_workRow);
}

// WORK page: campaign rows are accepted only through a detail/LOAD step, so a
// row press cannot accidentally load cargo. The station board's UNDOCK row is
// still the only departure path.
static void updateWork() {
    if (!arduboy.justPressed(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (s_workDetail) {
        if (arduboy.justPressed(B_BUTTON)) {
            s_workDetail = false;
            drawWork();
            return;
        }
        if (arduboy.justPressed(A_BUTTON) && campaign::accept(s_state, s_workMission)) {
            s_workDetail = false;
            s_stationRow = station::initialRow(s_state);
            s_screen = Screen::Station;
#if SH_VOYAGE_UI
            if (!requestSave(kSaveResumeStation))
                return;
#endif
            station_view::draw(s_state, s_stationRow);
            return;
        }
        drawWork();
        return;
    }

    if (arduboy.justPressed(B_BUTTON)) {
        s_screen = Screen::Station;
        s_stationRow = station::initialRow(s_state);
        station_view::draw(s_state, s_stationRow);
        return;
    }

    if (game::hasActiveJob(s_state)) {
        drawWork();
        return;
    }

    const uint8_t count = workVisibleCount();
    if (count != 0 && (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON))) {
        if (arduboy.justPressed(UP_BUTTON))
            s_workRow = static_cast<uint8_t>(s_workRow == 0 ? count - 1 : s_workRow - 1);
        else
            s_workRow = static_cast<uint8_t>(s_workRow + 1 >= count ? 0 : s_workRow + 1);
        drawWork();
        return;
    }

    if (arduboy.justPressed(A_BUTTON) && count != 0) {
        if (s_workRow >= count)
            s_workRow = 0;
        const uint8_t mission = campaign::workMissionAt(s_state, static_cast<uint8_t>(s_state.docked), s_workRow);
        if (mission == campaign::kWorkEnding) {
            beginEnding();
            return;
        }
        if (mission != campaign::kMissionNone) {
            s_workMission = mission;
            s_workDetail = true;
            drawWork();
            return;
        }
    }
    drawWork();
}
#endif

// Station frame: no world step, so travel time, air, and ballast are frozen.
// D-pad moves the selection, A confirms, B always undocks.
static void updateStation() {
#if SH_VOYAGE_UI && SH_FEATURE_CAMPAIGN
    if (s_abandonConfirm) {
        if (arduboy.justPressed(A_BUTTON)) {
            s_abandonConfirm = false;
            station::activate(station::kAbandonRow, s_state);
            s_stationRow = station::initialRow(s_state);
            if (!requestSave(kSaveResumeStation))
                return;
            station_view::draw(s_state, s_stationRow);
            return;
        }
        if (arduboy.justPressed(B_BUTTON)) {
            s_abandonConfirm = false;
            station_view::draw(s_state, s_stationRow);
            return;
        }
        s_present = false;
        return;
    }
#endif

    if (!arduboy.justPressed(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (arduboy.justPressed(UP_BUTTON))
        s_stationRow = station::selectPrevious(s_stationRow, s_state);
    if (arduboy.justPressed(DOWN_BUTTON))
        s_stationRow = station::selectNext(s_stationRow, s_state);

    if (arduboy.justPressed(A_BUTTON)) {
#if SH_VOYAGE_UI
        if (station::rowKind(s_stationRow, s_state) == station::kRowUndock) {
            leaveStation();
            return;
        }
#if SH_FEATURE_CAMPAIGN
        // ABANDON is never the initial row and needs its own confirmation.
        if (station::rowKind(s_stationRow, s_state) == station::kRowAbandon) {
            s_abandonConfirm = true;
            voyage_view::drawAbandonConfirm();
            return;
        }
#endif
#endif
        const station::Action action = station::activate(s_stationRow, s_state);
        if (action == station::kActionUndocked) {
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
        }
#if SH_FEATURE_CAMPAIGN
        else if (action == station::kActionWork) {
            s_workRow = 0;
            s_workMission = campaign::kMissionNone;
            s_workDetail = false;
            s_screen = Screen::Work;
            drawWork();
            return;
        }
#endif
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
#if SH_VOYAGE_UI
            if (!requestSave(kSaveResumeStation))
                return;
#endif
        } else if (action == station::kActionAccepted) {
            // Acceptance auto-loads the contract hold; the boat stays docked
            // until the player confirms UNDOCK or presses B.
            s_stationRow = station::initialRow(s_state);
#if SH_VOYAGE_UI
            if (!requestSave(kSaveResumeStation))
                return;
#endif
        }
#endif
    }

    if (arduboy.justPressed(B_BUTTON)) {
#if SH_VOYAGE_UI
        leaveStation();
        return;
#else
        if (station::undock(s_state)) {
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
        }
#endif
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

    if (arduboy.justPressed(A_BUTTON)) {
        if (shop::buy(s_state, static_cast<shop::Item>(s_stationRow)) == shop::kBought) {
#if SH_VOYAGE_UI
            // Only a successful purchase is a save boundary. A failed write
            // stays frozen on RETRY / CONTINUE UNSAVED.
            if (!requestSave(kSaveResumeShop))
                return;
#endif
        }
    }

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

#if SH_FEATURE_SAVE_CHART && !SH_DEV_MODE
    // Restore the explored set once at boot; resetRun() already revealed the
    // spawn, and a valid save overwrites that with the exact stored mask.
    loadChartSave();
#endif

#if SH_VOYAGE_UI
    // Never read voyage storage in developer mode. Normal boot inspects once so
    // the title can distinguish blank, corrupt and incompatible records before
    // offering CONTINUE or NEW VOYAGE; the checkpoint itself loads only after
    // an explicit CONTINUE.
    refreshBootStatus();
#endif
}

void loop() {
    if (!arduboy.nextFrame())
        return;
    arduboy.pollButtons();
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    s_present = true;
#endif

#if SH_FEATURE_MULTI_HULL || SH_VOYAGE_UI
    // Title boots the run and owns Continue/New Voyage (and hull selection when
    // the roster is present). It has no world step and no cart reads.
    if (s_screen == Screen::Title)
        updateTitle();
    else
#endif
#if SH_VOYAGE_UI
    if (s_screen == Screen::SaveFailed)
        updateSaveFailed();
    else
#endif
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
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
#if SH_FEATURE_CAMPAIGN
    else if (s_screen == Screen::Work)
        updateWork();
    else if (s_screen == Screen::Ending)
        updateEnding();
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
#if SH_FEATURE_CAMPAIGN
    else if (s_screen == Screen::Work)
        updateWork();
    else if (s_screen == Screen::Ending)
        updateEnding();
#endif
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
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    if (s_present)
#endif
        FX::display(CLEAR_BUFFER);
}
