// Sub-hauler — Arduboy FX.
//
// M3 station slice: world-aware submarine, HAULER silhouette, compact HUD, and
// the D-pad flight contract. Slow boats inside a settlement ring dock; the
// station board refills, repairs, and undocks. M6 adds a paused B-key chart
// with persistent fog exploration. The sketch owns
// pilot state, the FX table reader, the visible-window cache, and the screen
// state machine; rules and drawing stay in src/ headers.

#include <Arduboy2.h>
#include <ArduboyFX.h>

// The device flight benchmark times the real scene/HUD calls from inside the
// shipping draw path. The hooks compile out of every normal image.
#ifndef SH_FLIGHT_PROBE
#define SH_FLIGHT_PROBE 0
#endif
#if SH_FLIGHT_PROBE
namespace flight_probe {
extern uint32_t sceneUs;
extern uint32_t hudUs;
}
#endif

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
#if SH_FEATURE_JOBS
#include "src/work_view.hpp"
#endif
#if SH_FEATURE_CAMPAIGN
#include "src/campaign.hpp"
#include "src/campaign_view.hpp"
#include "src/ending_view.hpp"
#endif
#if SH_FEATURE_MUSIC
#include "src/music.hpp"
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

#if SH_FEATURE_SAVE_VOYAGE && !SH_DEV_MODE
#define SH_VOYAGE_UI 1
#include "src/voyage_flow.hpp"
#include "src/voyage_view.hpp"
#include "src/startup_view.hpp"
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
#if SH_FEATURE_JOBS
    Work,
#endif
#if SH_FEATURE_CAMPAIGN
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

#if SH_FEATURE_JOBS || SH_VOYAGE_UI
// Mutually exclusive screen transients share one row byte: only one of WORK,
// pause, save-failure and wreck can be active at a time. The station/shop row
// stays separate because a failed shop save resumes the same item row.
union UiRow {
    uint8_t work;
    uint8_t pause;
    uint8_t save;
    uint8_t failure;
};
static UiRow s_uiRow;
#endif
#if SH_FEATURE_CAMPAIGN
union UiAux {
    uint8_t endingCard;
};
static UiAux s_uiAux = {0};
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
static bool s_pauseConfirm = false;
static bool s_abandonConfirm = false;
static uint8_t s_savedNoticeTicks = 0;
#endif

#if SH_FEATURE_MULTI_HULL && !SH_VOYAGE_UI
// Roster title row: hull selection or the explicit loader action.
static uint8_t s_titleRow = roster_view::kTitleHull;
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
// Static screens own a mutating buffer: they are composed once on entry and
// again only when an input edge can change what is on screen. s_present is
// cleared on idle frames, so the loop omits the OLED transaction too; the
// SSD1306 keeps the last image and the buffer is already clear from the
// previous display.
static bool s_present = true;
#endif

// One button-edge snapshot per frame. Every screen consumes fresh edges only.
static uint8_t s_just = 0;

static inline bool just(uint8_t mask) {
    return (s_just & mask) != 0;
}

// Exact justPressed(mask) semantics for multi-button chord/guard masks: all
// masked buttons were up last frame and at least one is down now.
static inline bool justChord(uint8_t mask) {
    return (Arduboy2Base::previousButtonState & mask) == 0 && (Arduboy2Base::currentButtonState & mask) != 0;
}

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
#endif

#if SH_FEATURE_FOG_CHART
static chart::State s_chart;
#endif

#if SH_FEATURE_MULTI_HULL || SH_VOYAGE_UI
// In-sketch bootloader entry for the release runtime. The Arduboy2 API sets
// the bootloader magic key and resets through the watchdog, so uploaders need
// no runtime USB stack. The same action is reachable from the title LOADER row
// and from the library's DOWN-button check at power-on. Keep this after the
// title/state declarations: the sketch preprocessor inserts its auto-generated
// prototypes at the first function definition.
static void enterLoader() {
    Arduboy2Core::exitToBootloader();
}
#endif

#if SH_VOYAGE_UI
static void drawVoyageNotice(uint24_t label) {
    // The HUD hint plate is already black; reuse its row for the brief notice.
    text::at(3, 56);
    text::print(label);
}
#endif

static void drawPilot() {
    const cave::Camera camera = pilot_view::cameraForState(s_state);
#if SH_FLIGHT_PROBE
    const uint32_t probeSceneStart = micros();
#endif
    view::frame(s_state, camera, s_cache, s_tables);
#if SH_FLIGHT_PROBE
    flight_probe::sceneUs = micros() - probeSceneStart;
    const uint32_t probeHudStart = micros();
#endif
    hud::draw(s_state);
#if SH_FLIGHT_PROBE
    flight_probe::hudUs = micros() - probeHudStart;
#endif
#if SH_VOYAGE_UI
    uint24_t notice = 0;
    if (s_savedNoticeTicks != 0)
        notice = txt_saved_berth;
    else if (s_voyageDirty && s_state.discoveryTicks != 0)
        notice = txt_unsaved_find;
    if (notice)
        drawVoyageNotice(notice);
#endif
}

#if SH_FEATURE_STATIONS
// One station-board entry: shared screen tag, row and draw sequence.
static void showStationBoard(uint8_t row) {
    s_stationRow = row;
    s_screen = Screen::Station;
    station_view::draw(s_state, s_stationRow);
}

static void showPilot() {
#if SH_FEATURE_MUSIC
    music::resume();
#endif
    s_screen = Screen::Pilot;
    s_stationRow = station::kFirstRow;
    drawPilot();
}

static void showStation() {
#if SH_VOYAGE_UI
    s_savedNoticeTicks = 0;
#endif
#if SH_FEATURE_MUSIC
    uint8_t cue = music_logic::kHarbor;
    if (s_state.docked >= 3) {
        if (s_state.docked == 3) {
#if SH_FEATURE_CAMPAIGN
            cue = campaign::hasMilestone(s_state, campaign::kMilestoneBeacon) ? music_logic::kTidepool : music_logic::kAbyssal;
#else
            cue = music_logic::kAbyssal;
#endif
        } else {
            cue = music_logic::kTidepool;
        }
    }
    music::overrideCue(cue);
    music::resume();
#endif
    showStationBoard(station::initialRow(s_state));
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
// One checkpoint transaction. Storage touches only the cart save partition;
// the caller owns the UI decision after the result. The write is synchronous,
// and music is paused first, so no world step, chart read, audio refill or
// departure input can run during an erase/page program.
static voyage_save::SaveResult saveVoyageNow() {
    voyage_save::FxStorage storage;
#if SH_FEATURE_MUSIC
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
#endif
    const voyage_save::SaveResult result = voyage_flow::saveCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                                                       ,
                                                                       s_chart
#endif
                                                                       ,
                                                                       storage);
#if SH_FEATURE_MUSIC
    music::resumeAfterTransaction(musicToken);
#endif
    return result;
}

static void beginSaveFailure(SaveResume resume) {
#if SH_FEATURE_MUSIC
    music::pause();
#endif
    s_saveResume = resume;
    s_uiRow.save = 0;
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
    ending_view::draw(s_uiAux.endingCard, s_state);
}

static bool endingPending() {
    return s_state.ending == 0 && campaign::missionCompleted(s_state, campaign::kHome);
}

static void beginEnding() {
#if SH_FEATURE_MUSIC
    music::overrideCue(music_logic::kTidepool);
    music::resume();
#endif
    s_uiAux.endingCard = 0;
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
    if (!justChord(A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }
    const uint8_t next = ending_view::nextCard(s_uiAux.endingCard);
    if (next >= ending_view::kCardCount) {
        finishEnding();
        return;
    }
    s_uiAux.endingCard = next;
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
#if SH_FEATURE_MUSIC
        music::resume();
#endif
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
#if SH_FEATURE_MUSIC
    music::overrideCue(music_logic::kHarbor);
    music::resume();
#endif
    s_screen = Screen::Title;
#if SH_FEATURE_MULTI_HULL
    const uint8_t hullClass = s_state.hullClass;
#else
    const uint8_t hullClass = 0;
#endif
    voyage_view::drawTitle(s_bootStatus, s_titleRow, hullClass, s_titleMode);
}

static void refreshBootStatus() {
    voyage_save::FxStorage storage;
    s_bootStatus = voyage_flow::inspectBoot(storage);
    s_titleMode = voyage_view::kTitleMenu;
    s_titleRow = s_bootStatus.hasValid ? voyage_view::kTitleContinue : voyage_view::kTitleNewVoyage;
}

static void startNewVoyage() {
    voyage_save::FxStorage storage;
#if SH_FEATURE_MUSIC
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
#endif
    const voyage_save::SaveResult result = voyage_flow::startNewVoyage(s_state
#if SH_FEATURE_FOG_CHART
                                                                       ,
                                                                       s_chart
#endif
                                                                       ,
                                                                       storage);
#if SH_FEATURE_MUSIC
    music::resumeAfterTransaction(musicToken);
#endif
    if (result == voyage_save::kSaved) {
        // newVoyage() persisted the spawn reveal in the first checkpoint.
        s_voyageDirty = false;
        showStation();
    } else {
        beginSaveFailure(kSaveResumeStation);
    }
}

static void continueVoyage() {
    voyage_save::FxStorage storage;
#if SH_FEATURE_MUSIC
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
#endif
    const voyage_save::LoadResult result = voyage_flow::loadCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                                                       ,
                                                                       s_chart
#endif
                                                                       ,
                                                                       storage);
#if SH_FEATURE_MUSIC
    music::resumeAfterTransaction(musicToken);
#endif
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
    voyage_save::FxStorage storage;
#if SH_FEATURE_MUSIC
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
#endif
    const voyage_flow::Recovery result = voyage_flow::recoverCheckpoint(s_state
#if SH_FEATURE_FOG_CHART
                                                                        ,
                                                                        s_chart
#endif
                                                                        ,
                                                                        storage);
#if SH_FEATURE_MUSIC
    music::resumeAfterTransaction(musicToken);
#endif
    if (result.load != voyage_save::kLoaded) {
        refreshBootStatus();
        drawTitleScreen();
        return;
    }
    if (result.save == voyage_save::kSaved) {
        s_voyageDirty = false;
#if SH_FEATURE_CAMPAIGN
        if (endingPending()) {
            beginEnding();
            return;
        }
#endif
        showStation();
    } else {
        // The ending stays pending in the restored state. RETRY re-enters
        // finishSaveResume(), which shows the ending only after the commit
        // succeeds.
        beginSaveFailure(kSaveResumeStation);
    }
}
#endif   // SH_VOYAGE_UI

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
// Paused/failure frames deliberately draw no world and read no cart: the
// buffer is blank after the previous FX::display(CLEAR_BUFFER), so one short
// text line is enough at 128x64.
static void drawPause() {
#if SH_FEATURE_MUSIC
    music::pause();
#endif
#if SH_VOYAGE_UI
    if (s_pauseConfirm)
        voyage_view::drawRescueConfirm();
    else
        voyage_view::drawPauseRescue(s_uiRow.pause
#if SH_FEATURE_MUSIC
                                     ,
                                     music::enabled()
#endif
        );
#else
    text::normal();
    text::at(10, 30);
    text::print(txt_paused);
#endif
}

#if SH_FEATURE_HULL_DAMAGE
static void drawFailure() {
#if SH_FEATURE_MUSIC
    music::pause();
#endif
#if SH_VOYAGE_UI
    voyage_view::drawWreck(s_uiRow.failure);
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
// only selectable for a valid checkpoint; replacing a valid checkpoint asks for
// confirmation. Blank, corrupt and incompatible partitions start a clean New
// Voyage immediately. A new voyage always starts with an empty chart; no legacy
// chart prompt or import exists.
static void updateTitle() {
    if (s_titleMode == voyage_view::kTitleConfirmReplace) {
        if (just(A_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            startNewVoyage();
            return;
        }
        if (just(B_BUTTON)) {
            s_titleMode = voyage_view::kTitleMenu;
            drawTitleScreen();
            return;
        }
        s_present = false;
        return;
    }

#if SH_FEATURE_MULTI_HULL
    roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(s_state.hullClass));
    if (just(LEFT_BUTTON))
        hull = roster::previous(hull);
    if (just(RIGHT_BUTTON))
        hull = roster::next(hull);
    if (just(B_BUTTON))
        hull = roster::kDefault;
    s_state.hullClass = static_cast<uint8_t>(hull);
#endif

    if (just(UP_BUTTON))
        s_titleRow = voyage_view::previousRow(s_titleRow, s_bootStatus);
    if (just(DOWN_BUTTON))
        s_titleRow = voyage_view::nextRow(s_titleRow, s_bootStatus);

    if (just(A_BUTTON)) {
        if (s_titleRow == voyage_view::kTitleLoader) {
            enterLoader();
            return;
        }
        if (s_titleRow == voyage_view::kTitleContinue && s_bootStatus.hasValid) {
            continueVoyage();
            return;
        }
        if (s_bootStatus.needsReplaceConfirm())
            s_titleMode = voyage_view::kTitleConfirmReplace;
        else {
            startNewVoyage();
            return;
        }
    }

    drawTitleScreen();
}
#elif SH_FEATURE_MULTI_HULL
// Title frame: no world step. The only cart reads are the selected hull's
// baked sprite and the external labels, all before the display transaction.
// Left/right cycles the roster; up/down selects the hull or LOADER row; A
// confirms or enters the bootloader; B backs to the HAULER default. The hull
// selection lives in game::State, so it survives every dock/undock for the
// rest of the run.
static void updateTitle() {
    roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(s_state.hullClass));

    if (just(UP_BUTTON) || just(DOWN_BUTTON))
        s_titleRow = roster_view::nextRow(s_titleRow);

    if (just(LEFT_BUTTON)) {
        hull = roster::previous(hull);
        s_titleRow = roster_view::kTitleHull;
    }
    if (just(RIGHT_BUTTON)) {
        hull = roster::next(hull);
        s_titleRow = roster_view::kTitleHull;
    }
    // B backs out of a non-default choice. Title is the root screen, so the
    // only sensible back target is the safe HAULER default.
    if (just(B_BUTTON)) {
        hull = roster::kDefault;
        s_titleRow = roster_view::kTitleHull;
    }

    s_state.hullClass = static_cast<uint8_t>(hull);

    if (just(A_BUTTON)) {
        if (s_titleRow == roster_view::kTitleLoader) {
            enterLoader();
            return;
        }
        s_screen = Screen::Pilot;
        drawPilot();
        return;
    }

    roster_view::draw(s_state.hullClass, s_titleRow);
}
#endif

#if SH_FEATURE_FOG_CHART
// The chart is a paused world, not a paused soundtrack: cart water reads happen
// here, before the single display transaction, and no world step runs. Music
// keeps its current flight cue and the loop's service hooks refill the stream;
// chart water bursts service the ring between reads.
static void drawChart() {
    // The frame counter is the blink phase: no extra state, and a map redraws
    // on blink changes from the frame number rather than the frozen
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
        if (just(A_BUTTON)) {
            s_pauseConfirm = false;
            performRecovery();
            return;
        }
        if (just(B_BUTTON)) {
            s_pauseConfirm = false;
            drawPause();
            return;
        }
        s_present = false;
        return;
    }

    if (just(UP_BUTTON) || just(DOWN_BUTTON)) {
        s_uiRow.pause = static_cast<uint8_t>(s_uiRow.pause + 1);
#if SH_FEATURE_MUSIC
        if (s_uiRow.pause >= 3)
            s_uiRow.pause = 0;
#else
        if (s_uiRow.pause >= 2)
            s_uiRow.pause = 0;
#endif
        drawPause();
        return;
    }

    if (just(A_BUTTON)) {
        if (s_uiRow.pause == 0) {
#if SH_FEATURE_MUSIC
            music::resume();
#endif
            s_screen = Screen::Pilot;
            drawPilot();
            return;
        }
#if SH_FEATURE_MUSIC
        if (s_uiRow.pause == 2) {
            music::toggleEnabled();
            drawPause();
            return;
        }
#endif
        s_pauseConfirm = true;
        voyage_view::drawRescueConfirm();
        return;
    }
    if (just(B_BUTTON)) {
        showPilot();
        return;
    }
    s_present = false;
#else
    if (just(A_BUTTON) || just(B_BUTTON)) {
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
    if (just(UP_BUTTON) || just(DOWN_BUTTON)) {
        s_uiRow.failure = s_uiRow.failure == 0 ? 1 : 0;
        voyage_view::drawWreck(s_uiRow.failure);
        return;
    }
    if (just(A_BUTTON)) {
        if (s_uiRow.failure == 0) {
            performRecovery();
        } else {
            refreshBootStatus();
            drawTitleScreen();
        }
        return;
    }
    s_present = false;
#else
    if (just(A_BUTTON)) {
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
    if (just(UP_BUTTON) || just(DOWN_BUTTON)) {
        s_uiRow.save = s_uiRow.save == 0 ? 1 : 0;
        voyage_view::drawSaveFailed(s_uiRow.save);
        return;
    }
    if (just(A_BUTTON)) {
        if (s_uiRow.save == 0) {
            if (saveVoyageNow() == voyage_save::kSaved) {
                s_voyageDirty = false;
                finishSaveResume();
            } else {
                voyage_view::drawSaveFailed(s_uiRow.save);
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
#if SH_VOYAGE_UI
    if (s_savedNoticeTicks != 0)
        --s_savedNoticeTicks;
#endif
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    // A+B is the free-flight pause gesture. Require fresh edges: a held chord
    // must not re-enter Pause after a resume or station transition.
    if (pause::resolve(false, just(A_BUTTON), just(B_BUTTON)) == pause::kEnter) {
        s_screen = Screen::Pause;
#if SH_VOYAGE_UI
        s_uiRow.pause = 0;
        s_pauseConfirm = false;
#endif
        drawPause();
        return;
    }
#endif
#if SH_FEATURE_FOG_CHART
    // B only opens the chart in free flight. Consume the edge before sampling
    // flight input so travel, air, and ballast do not tick on that frame.
    if (just(B_BUTTON)) {
        s_screen = Screen::Chart;
        drawChart();
        return;
    }
#endif

#if SH_FEATURE_MUSIC
    music::updateFlight(s_state);
    music::resume();
#endif

    // D-pad only: left/right thrust, up blows ballast (rise), down floods
    // (sink). A/B stay free for station actions once docked.
    const uint8_t held = Arduboy2Base::currentButtonState;
    const game::Input input{static_cast<bool>(held & UP_BUTTON), static_cast<bool>(held & DOWN_BUTTON), static_cast<bool>(held & LEFT_BUTTON), static_cast<bool>(held & RIGHT_BUTTON)};

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
#if SH_VOYAGE_UI
        s_uiRow.failure = 0;
#endif
        s_screen = Screen::Failure;
        drawFailure();
        return;
    }
#endif

    drawPilot();
#endif   // SH_DEV_MODE
}

#if SH_FEATURE_FOG_CHART
// Chart frame: no world step, so travel time/air/ballast/jobClock are frozen
// and D-pad/A are contained. B closes. The destination X blinks from the
// Arduboy frame counter because game::State.ticks cannot advance here.
static void updateChart() {
    if (just(B_BUTTON)) {
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
        s_savedNoticeTicks = 90;
        showPilot();
    }
}
#endif

#if SH_FEATURE_JOBS
static void drawWork() {
    work_view::draw(s_state, s_uiRow.work);
}

// Single WORK board: eligible restoration rows come first, then the one
// repeatable freight row. A accepts the selected row directly; the selected
// job auto-loads its four cargo units and returns to the station board, where
// UNDOCK/B starts travel. B returns without accepting.
static void updateWork() {
    if (!justChord(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (just(B_BUTTON)) {
        showStationBoard(station::initialRow(s_state));
        return;
    }

    if (game::hasActiveJob(s_state)) {
        drawWork();
        return;
    }

    const uint8_t count = work_view::rowCount(s_state);
    if (count != 0 && (just(UP_BUTTON) || just(DOWN_BUTTON))) {
        if (just(UP_BUTTON))
            s_uiRow.work = static_cast<uint8_t>(s_uiRow.work == 0 ? count - 1 : s_uiRow.work - 1);
        else
            s_uiRow.work = static_cast<uint8_t>(s_uiRow.work + 1 >= count ? 0 : s_uiRow.work + 1);
        drawWork();
        return;
    }

    if (just(A_BUTTON) && count != 0) {
        if (s_uiRow.work >= count)
            s_uiRow.work = 0;
        const work_view::Row row = work_view::rowAt(s_state, s_uiRow.work);
#if SH_FEATURE_CAMPAIGN
        if (row.kind == work_view::kRowEnding) {
            beginEnding();
            return;
        }
#endif
        if (work_view::accept(s_state, row)) {
            s_stationRow = station::initialRow(s_state);
            s_screen = Screen::Station;
#if SH_VOYAGE_UI
            if (!requestSave(kSaveResumeStation))
                return;
#endif
            station_view::draw(s_state, s_stationRow);
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
        if (just(A_BUTTON)) {
            s_abandonConfirm = false;
            station::activate(station::kAbandonRow, s_state);
            s_stationRow = station::initialRow(s_state);
            if (!requestSave(kSaveResumeStation))
                return;
            station_view::draw(s_state, s_stationRow);
            return;
        }
        if (just(B_BUTTON)) {
            s_abandonConfirm = false;
            station_view::draw(s_state, s_stationRow);
            return;
        }
        s_present = false;
        return;
    }
#endif

    if (!justChord(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (just(UP_BUTTON))
        s_stationRow = station::selectPrevious(s_stationRow, s_state);
    if (just(DOWN_BUTTON))
        s_stationRow = station::selectNext(s_stationRow, s_state);

    if (just(A_BUTTON)) {
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
#if SH_FEATURE_JOBS
        else if (action == station::kActionWork) {
            s_uiRow.work = 0;
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

    if (just(B_BUTTON)) {
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
    if (!justChord(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (just(UP_BUTTON))
        s_stationRow = shop::selectPrevious(s_stationRow);
    if (just(DOWN_BUTTON))
        s_stationRow = shop::selectNext(s_stationRow);

    if (just(A_BUTTON)) {
        if (shop::buy(s_state, static_cast<shop::Item>(s_stationRow)) == shop::kBought) {
#if SH_VOYAGE_UI
            // Only a successful purchase is a save boundary. A failed write
            // stays frozen on RETRY / CONTINUE UNSAVED.
            if (!requestSave(kSaveResumeShop))
                return;
#endif
        }
    }

    if (just(B_BUTTON)) {
        showStationBoard(station::shopRow(s_state));
        return;
    }

    shop_view::draw(s_state, s_stationRow);
}
#endif
#endif

// The Arduboy2 USB-free entry point must be placed immediately before setup():
// the sketch preprocessor inserts function prototypes above this point. The
// macro removes the runtime USB stack, not the bootloader. Development builds
// keep the USB runtime so the serial device harness is unaffected.
#if SH_RELEASE_NO_USB && !SH_DEV_MODE
ARDUBOY_NO_USB
#endif

void setup() {
    arduboy.boot();
#if SH_FEATURE_MUSIC
    arduboy.audio.begin();
#endif
    arduboy.setFrameRate(60);
#if SH_VOYAGE_UI
    voyage_save::FxStorage::begin();
#else
    FX::begin(FX_DATA_PAGE);
#endif
    FX::setCursorRange(0, 32767);
#if SH_VOYAGE_UI
    startup_view::show();
#endif
    text::begin();
#if SH_FEATURE_MUSIC
    music::begin();
#endif

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
#if SH_FEATURE_MUSIC
    music::service();
#endif
    if (!arduboy.nextFrame()) {
#if SH_FEATURE_MUSIC
        music::service();
#endif
        return;
    }
    arduboy.pollButtons();
    s_just = static_cast<uint8_t>(Arduboy2Base::currentButtonState & ~Arduboy2Base::previousButtonState);
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    s_present = true;
#endif

#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    switch (s_screen) {
#if SH_FEATURE_MULTI_HULL || SH_VOYAGE_UI
    case Screen::Title:
        updateTitle();
        break;
#endif
#if SH_VOYAGE_UI
    case Screen::SaveFailed:
        updateSaveFailed();
        break;
#endif
    case Screen::Pause:
        updatePause();
        break;
#if SH_FEATURE_HULL_DAMAGE
    case Screen::Failure:
        updateFailure();
        break;
#endif
#if SH_FEATURE_STATIONS
    case Screen::Station:
        updateStation();
        break;
#endif
#if SH_FEATURE_JOBS
    case Screen::Work:
        updateWork();
        break;
#endif
#if SH_FEATURE_CAMPAIGN
    case Screen::Ending:
        updateEnding();
        break;
#endif
#if SH_FEATURE_SHOP
    case Screen::Shop:
        updateShop();
        break;
#endif
#if SH_FEATURE_FOG_CHART
    case Screen::Chart:
        updateChart();
        break;
#endif
    case Screen::Pilot:
        updateFlight();
        break;
    default:
        break;
    }
#else
    updateFlight();
#endif

    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction. A static screen with no input
    // edge clears s_present and keeps the previous panel image; the buffer is
    // already clear from the last display.
    // The OLED and the FX cart share SPI: all cart reads above happen before
    // this single buffered display transaction. A static screen with no input
    // edge clears s_present and keeps the previous panel image; the buffer is
    // already clear from the last display.
#if SH_FEATURE_STATIONS || SH_FEATURE_FOG_CHART || SH_FEATURE_MULTI_HULL || SH_FEATURE_HULL_DAMAGE || SH_VOYAGE_UI
    if (s_present)
#endif
        FX::display(CLEAR_BUFFER);

#if SH_FEATURE_MUSIC
    music::service();
#endif
}
