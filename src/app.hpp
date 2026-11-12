// Sub-hauler — shared fixed application.
//
// Included by the tiny release sketch and by fixed USB device fixtures.
// The release sketch owns ARDUBOY_NO_USB, setup() and loop().
//
// M3 station slice: world-aware submarine, HAULER silhouette, compact HUD, and
// the D-pad flight contract. Slow boats inside a settlement ring dock; the
// station board refills and undocks. M6 adds a paused B-key chart
// with persistent fog exploration. The sketch owns
// pilot state, the FX table reader, the visible-window cache, and the screen
// state machine; rules and drawing stay in src/ headers.

#include <Arduboy2.h>
#include <ArduboyFX.h>

#include "common.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "hud.hpp"
#include "pause.hpp"
#include "pilot.hpp"
#include "render.hpp"
#include "world_fx.hpp"


#include "station.hpp"
#include "station_view.hpp"
#include "work_view.hpp"
#include "campaign.hpp"
#include "campaign_view.hpp"
#include "story_view.hpp"
#include "ending_view.hpp"
#include "music.hpp"
#include "chart.hpp"
#include "chart_view.hpp"

#include "voyage_flow.hpp"
#include "voyage_view.hpp"
#include "startup_view.hpp"

decltype(arduboy) arduboy;

// Storage validates the pending-deck range without including story_view.hpp
// (its AVR draw path is presentation-only). Pin the two frozen counts here.
static_assert(voyage_save::kPendingStoryMax == story_view::kSceneMax, "pending scene ids match the frozen decks and handoffs");

static game::State s_state;
static world::FxTables s_tables;
static world::TileCache s_cache;

#if SH_DEV_MODE
// Developer map-inspection toggle. The flag is compiled out of the fixed build.
static bool s_debugNoClip = false;
#endif

// The screen tag exists whenever more than one screen can be reached. The
// fixed build may show the title, pilot, station, work, story, ending, chart
// and failure screens; there is no multi-hull variant.
enum class Screen : uint8_t {
    Title,
    Pilot,
    Pause,
    Station,
    Work,
    Story,
    Ending,
    Chart,
    SaveFailed,
};

static Screen s_screen = Screen::Title;

// The station row byte is screen-owned: the station board uses it for row
// selection. Every station frame restores a valid row before drawing, so there
// is no extra selection state.
static uint8_t s_stationRow = station::kFirstRow;

// Mutually exclusive screen transients share one row byte: only one of WORK,
// pause and save-failure can be active at a time.
union UiRow {
    uint8_t work;
    uint8_t pause;
    uint8_t save;
};
static UiRow s_uiRow;
union UiAux {
    uint8_t endingCard;
};
static UiAux s_uiAux = {0};

// New restoration beat awaiting its deck. bss zero is kCardNone; the byte is
// set only by a dock/survey transition and cleared when the deck finishes.
// The New Voyage opening deck is also staged here, but never written to the
// checkpoint: Continue restores the committed berth and resumes without it.
static uint8_t s_pendingStory;

// Story deck progress. The card/phase/ticks are transient; a reload never
// resumes a non-HOME deck because the milestone is already committed. HOME is
// part of the ending path and is resumable while the ending flag is clear.
static uint8_t s_storyCard;
static uint8_t s_storyTicks;
static uint8_t s_storyPhase;
static uint8_t s_storyReturnPilot;

// Save integration state. Writes are synchronous transitions; while the save
// failure frame is showing the world is frozen and departure input is ignored.
enum SaveResume : uint8_t {
    kSaveResumeStation,
    kSaveResumePilot,
};

static voyage_flow::BootStatus s_bootStatus;
static voyage_view::TitleMode s_titleMode = voyage_view::kTitleMenu;
static uint8_t s_titleRow = voyage_view::kTitleNewVoyage;
static bool s_voyageDirty = false;
// Last committed checkpoint CRC (RAM only). requestSave() skips the cart
// transaction when the live state would rewrite a byte-identical checkpoint;
// normal travel changes voyage ticks or the chart, so this only removes truly
// redundant writes (for example re-docking or a no-op station action).
static voyage_flow::CheckpointCache s_checkpoint;
static SaveResume s_saveResume = kSaveResumeStation;
static bool s_pauseConfirm = false;
static bool s_airEmptyShown = false;
static uint8_t s_savedNoticeTicks = 0;


// Static screens own a mutating buffer: they are composed once on entry and
// again only when an input edge can change what is on screen. s_present is
// cleared on idle frames, so the loop omits the OLED transaction too; the
// SSD1306 keeps the last image and the buffer is already clear from the
// previous display.
static bool s_present = true;

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


static chart::State s_chart;

static void drawVoyageNotice(uint24_t image) {
    // The hint plate is not always black: hud::draw only clears it for an active
    // HUD notice. Clear the 128x9 row here so SAVED AT BERTH / UNSAVED FIND
    // notices hide terrain and remove any stale HUD hint underneath.
    cave::fillRect(Arduboy2Base::sBuffer, hud::kHintBackingX, hud::kHintBackingY, hud::kHintBackingW, hud::kHintBackingH, false);
    text::at(3, 56);
    text::stamp(image);
}

// Map the pure campaign next-stop rule to baked FX stamps. Reusing the
// existing clue images keeps the station notice, WORK rows and chart markers on
// one source of truth without adding story cards or cart streams.
static uint24_t nextStopLabel(campaign::NextStop stop) {
    uint24_t label = 0;
    if (stop == campaign::kNextStopLiftSurvey)
        label = img_txt_lift_clue;
    else if (stop == campaign::kNextStopLiftSource)
        label = img_txt_lift_source_clue;
    else if (stop == campaign::kNextStopPumpSurvey)
        label = img_txt_pump_survey_clue;
    else if (stop == campaign::kNextStopPumpSource)
        label = img_txt_pump_source_clue;
    else if (stop == campaign::kNextStopSeal)
        label = img_txt_seal_rumor;
    else if (stop == campaign::kNextStopBeacon)
        label = img_txt_beacon_ready;
    return label;
}

// Only PUMP and LIFT decks post a next-stop notice. The board hint row is
// replaced for this one composed frame; the next station input redraw returns
// the normal generated hint.
static void drawNextStopHint(uint8_t finishedDeck) {
    uint8_t mission = campaign::kMissionNone;
    if (finishedDeck == story_view::kCardPump)
        mission = campaign::kPump;
    else if (finishedDeck == story_view::kCardLift)
        mission = campaign::kLift;
    if (mission == campaign::kMissionNone)
        return;
    const uint24_t label = nextStopLabel(campaign::nextStopAfter(s_state, mission));
    if (label != 0)
        drawVoyageNotice(label);
}

static void drawPilot() {
    const cave::Camera camera = pilot_view::cameraForState(s_state);
    view::frame(s_state, camera, s_cache, s_tables);
    hud::draw(s_state);
    uint24_t notice = 0;
    if (s_savedNoticeTicks != 0)
        notice = img_txt_saved_berth;
    else if (s_voyageDirty && s_state.discoveryTicks != 0)
        notice = img_txt_unsaved_find;
    if (notice)
        drawVoyageNotice(notice);
}

// One station-board entry: shared screen tag, row and draw sequence.
static void showStationBoard(uint8_t row) {
    s_stationRow = row;
    s_screen = Screen::Station;
    station_view::draw(s_state, s_stationRow);
}

static void showPilot() {
    music::resume();
    s_screen = Screen::Pilot;
    s_stationRow = station::kFirstRow;
    drawPilot();
}

static void showStation() {
    s_savedNoticeTicks = 0;
    s_airEmptyShown = false;
    music::overrideCue(music_logic::stationCue(static_cast<uint8_t>(s_state.docked), s_state.campaign));
    music::resume();
    showStationBoard(station::initialRow(s_state));
}

// One boot/restart path. The setup call owns the full reset.
static void resetRun() {
    game::resetWorld(s_state);
#if SH_DEV_MODE
    s_debugNoClip = false;
#endif
    s_checkpoint.clear();
    chart::reset(s_chart);
    chart::revealInitial(s_chart, s_state);
    s_pendingStory = story_view::kCardNone;
    s_storyCard = 0;
    s_storyTicks = 0;
    s_airEmptyShown = false;
    s_storyPhase = 0;
    s_storyReturnPilot = 0;
}


// One checkpoint transaction. Storage touches only the cart save partition;
// the caller owns the UI decision after the result. The write is synchronous,
// and music is paused first, so no world step, chart read, audio refill or
// departure input can run during an erase/page program. The cached variant
// returns success without erasing or programming when the live checkpoint has
// the exact CRC of the last committed one.
static voyage_save::SaveResult saveVoyageNow() {
    voyage_save::FxStorage storage;
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
    const voyage_save::SaveResult result = voyage_flow::saveCheckpointIfChanged(s_state
                                                                               ,
                                                                               s_chart
                                                                               ,
                                                                               storage
                                                                               ,
                                                                               s_checkpoint);
    music::resumeAfterTransaction(musicToken);
    return result;
}

static void beginSaveFailure(SaveResume resume) {
    music::pause();
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

// HOME is paid and committed by the normal dock checkpoint before any card is
// shown. The ending-presented flag is separate: a reload with HOME complete but
// ending still zero resumes the HOME deck and ending cards without paying
// again; Tidewall WORK replays the two ending cards after the flag commits.
static bool endingPending() {
    return s_state.ending == 0 && campaign::missionCompleted(s_state, campaign::kHome);
}

static void drawEnding() {
    ending_view::draw(s_uiAux.endingCard, s_state);
}

static void beginEndingCards() {
    music::overrideCue(music_logic::kTidepool);
    music::resume();
    s_uiAux.endingCard = 0;
    s_screen = Screen::Ending;
    drawEnding();
}

static void drawStory() {
    const uint8_t phase = static_cast<uint8_t>((s_storyTicks / STORY_BEAT_FRAME_TICKS) & 3);
    story_view::draw(s_pendingStory, s_storyCard, phase);
}

// A scene is shown after the successful dock checkpoint, after a cargo
// acceptance checkpoint, or immediately after an in-world survey dwell. A or B
// advances one card. Delivery/acceptance scenes are already in the checkpoint;
// survey scenes are transient field finds.
static void beginStory() {
    const uint8_t cue = music_logic::deliveryStoryCue(s_pendingStory);
    if (cue != music_logic::kCueNone)
        music::overrideCue(cue);
    s_storyCard = 0;
    s_storyTicks = 0;
    s_storyPhase = 0;
    s_screen = Screen::Story;
    drawStory();
}

static void finishStory() {
    // Capture the scene that just ended before the pending byte is cleared.
    const uint8_t finishedScene = s_pendingStory;
    s_pendingStory = story_view::kSceneNone;
    // The scene is no longer pending. This is RAM only: the next natural
    // checkpoint writes the cleared byte, so a cold reload before then replays
    // the scene (at-least-once).
    s_state.pendingStory = story_view::kSceneNone;
    if (endingPending()) {
        beginEndingCards();
        return;
    }
    if (s_storyReturnPilot) {
        s_storyReturnPilot = 0;
        showPilot();
        return;
    }
    // The second repair's delivery deck hands off to one joint-restoration
    // scene at whichever settlement received it. The scene is keyed to saved seal
    // ownership, so either repair order and either berth show the right lead.
    if ((finishedScene == story_view::kScenePump || finishedScene == story_view::kSceneLift) &&
        campaign::hasMilestone(s_state, campaign::kMilestonePump) &&
        campaign::hasMilestone(s_state, campaign::kMilestoneLift)) {
        s_pendingStory = story_view::jointHandoff(campaign::sealOwned(s_state));
        s_state.pendingStory = s_pendingStory;
        beginStory();
        return;
    }
    showStation();
    drawNextStopHint(finishedScene);
}

static void updateStory() {
    if (!justChord(A_BUTTON | B_BUTTON)) {
        if (story_view::isImageCard(s_pendingStory, s_storyCard)) {
            ++s_storyTicks;
            if (s_storyTicks >= STORY_BEAT_CYCLE_TICKS)
                s_storyTicks = 0;
            const uint8_t phase = static_cast<uint8_t>((s_storyTicks / STORY_BEAT_FRAME_TICKS) & 3);
            if (phase != s_storyPhase) {
                s_storyPhase = phase;
                drawStory();
                return;
            }
        }
        s_present = false;
        return;
    }
    const uint8_t next = static_cast<uint8_t>(s_storyCard + 1);
    if (next >= story_view::cardCount(s_pendingStory)) {
        finishStory();
        return;
    }
    s_storyCard = next;
    s_storyTicks = 0;
    s_storyPhase = 0;
    drawStory();
}

static void beginEnding() {
    if (endingPending()) {
        // The finalized HOME deck is the first part of the ending. It resumes
        // after a reload until markEndingPresented() commits the flag.
        s_pendingStory = story_view::kCardHome;
        s_storyReturnPilot = 0;
        beginStory();
        return;
    }
    beginEndingCards();
}

static void finishEnding() {
    if (s_state.ending == 0) {
        campaign::markEndingPresented(s_state);
        if (!requestSave(kSaveResumeStation))
            return;
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

static void finishSaveResume() {
    switch (s_saveResume) {
    case kSaveResumePilot:
        if (s_state.docked >= 0)
            station::undock(s_state);
        showPilot();
        return;
    default:
        if (endingPending()) {
            beginEnding();
            return;
        }
        if (s_pendingStory != story_view::kCardNone) {
            beginStory();
            return;
        }
        showStation();
        return;
    }
}

static void drawTitleScreen() {
    music::overrideCue(music_logic::kHarbor);
    music::resume();
    s_screen = Screen::Title;
    voyage_view::drawTitle(s_bootStatus, s_titleRow, s_titleMode);
}

static void refreshBootStatus() {
    voyage_save::FxStorage storage;
    s_bootStatus = voyage_flow::inspectBoot(storage);
    s_titleRow = s_bootStatus.hasValid ? voyage_view::kTitleContinue : voyage_view::kTitleNewVoyage;
    s_titleMode = voyage_view::kTitleMenu;
}

static void startNewVoyage() {
    voyage_save::FxStorage storage;
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
    const voyage_save::SaveResult result = voyage_flow::startNewVoyage(s_state
                                                                       ,
                                                                       s_chart
                                                                       ,
                                                                       storage
                                                                       ,
                                                                       s_checkpoint);
    music::resumeAfterTransaction(musicToken);
    s_airEmptyShown = false;
    if (result == voyage_save::kSaved) {
        // newVoyage() persisted the spawn reveal in the first checkpoint and
        // left the live boat in free flight beside the home dock. The opening
        // deck is RAM-only and plays before the first pilot frame; a cold
        // reload from here resumes the saved berth without replaying it.
        s_voyageDirty = false;
        s_savedNoticeTicks = 0;
        s_pendingStory = story_view::kSceneOpening;
        s_storyReturnPilot = 1;
        beginStory();
    } else {
        beginSaveFailure(kSaveResumeStation);
    }
}

static void continueVoyage() {
    voyage_save::FxStorage storage;
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
    const voyage_save::LoadResult result = voyage_flow::loadCheckpoint(s_state
                                                                       ,
                                                                       s_chart
                                                                       ,
                                                                       storage
                                                                       ,
                                                                       s_checkpoint);
    music::resumeAfterTransaction(musicToken);
    s_airEmptyShown = false;
    if (result != voyage_save::kLoaded) {
        refreshBootStatus();
        drawTitleScreen();
        return;
    }
    s_voyageDirty = false;
    // A reload between the committed HOME delivery and its cards resumes the
    // ending. Payment and the HOME milestone are already in the checkpoint.
    if (endingPending()) {
        beginEnding();
        return;
    }
    // A reload between a committed milestone delivery and its deck resumes the
    // persisted deck. The byte is cleared in RAM when the deck finishes; until
    // the next natural checkpoint a cold reload can replay it (at-least-once).
    if (s_state.pendingStory != story_view::kCardNone) {
        s_pendingStory = s_state.pendingStory;
        s_storyReturnPilot = 0;
        beginStory();
        return;
    }
    showStation();
}

// Free rescue recovery. The complete checkpoint is restored first, then the
// rescue count is committed before the station board resumes. A failed commit
// keeps the restored state live and asks honestly.
static void performRecovery() {
    voyage_save::FxStorage storage;
    const music_logic::PauseToken musicToken = music::pauseForTransaction();
    const voyage_flow::Recovery result = voyage_flow::recoverCheckpoint(s_state
                                                                        ,
                                                                        s_chart
                                                                        ,
                                                                        storage
                                                                        ,
                                                                        s_checkpoint);
    music::resumeAfterTransaction(musicToken);
    s_airEmptyShown = false;
    if (result.load != voyage_save::kLoaded) {
        refreshBootStatus();
        drawTitleScreen();
        return;
    }
    if (result.save == voyage_save::kSaved) {
        s_voyageDirty = false;
        if (endingPending()) {
            beginEnding();
            return;
        }
        if (s_state.pendingStory != story_view::kSceneNone) {
            s_pendingStory = s_state.pendingStory;
            s_storyReturnPilot = 0;
            beginStory();
            return;
        }
        showStation();
    } else {
        // The ending stays pending in the restored state. RETRY re-enters
        // finishSaveResume(), which shows the ending only after the commit
        // succeeds.
        beginSaveFailure(kSaveResumeStation);
    }
}

// Paused/save-failure frames deliberately draw no world and read no cart: the
// buffer is blank after the previous FX::display(CLEAR_BUFFER), so one short
// text line is enough at 128x64.
static void drawPause() {
    music::pause();
    if (s_pauseConfirm)
        voyage_view::drawRescueConfirm();
    else
        voyage_view::drawPauseRescue(s_uiRow.pause
                                     ,
                                     music::enabled()
        );
}

// Title with CONTINUE/NEW VOYAGE. No world step and no cart reads. CONTINUE is
// only selectable for a valid checkpoint; replacing a valid checkpoint asks for
// confirmation. Blank, corrupt and incompatible partitions start a clean New
// Voyage immediately. A new voyage always starts with HAULER and an empty
// chart; no hull selector or legacy chart prompt exists. Bootloader entry is
// the library's DOWN check at power-on only; the title has no loader row.
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

    if (just(UP_BUTTON))
        s_titleRow = voyage_view::previousRow(s_titleRow, s_bootStatus);
    if (just(DOWN_BUTTON))
        s_titleRow = voyage_view::nextRow(s_titleRow, s_bootStatus);

    if (just(A_BUTTON)) {
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

// Pause frame: no world step, no air/ballast drain, no job clock, and no cart
// reads. With voyage saving the pause menu offers RESUME or a confirmed free
// rescue; otherwise A or B resumes on the frozen pilot frame without stepping.
static void updatePause() {
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
        if (s_uiRow.pause >= 3)
            s_uiRow.pause = 0;
        drawPause();
        return;
    }

    if (just(A_BUTTON)) {
        if (s_uiRow.pause == 0) {
            music::resume();
            s_screen = Screen::Pilot;
            drawPilot();
            return;
        }
        if (s_uiRow.pause == 2) {
            music::toggleEnabled();
            drawPause();
            return;
        }
        s_pauseConfirm = true;
        voyage_view::drawRescueConfirm();
        return;
    }
    if (just(B_BUTTON)) {
        showPilot();
        return;
    }
    s_present = false;
}

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

// Flight frame: step once, then either redraw the cave or switch to the
// station board on the frame that docked.
static void updateFlight() {
    if (s_savedNoticeTicks != 0)
        --s_savedNoticeTicks;
    // A+B is the free-flight pause gesture. Require fresh edges: a held chord
    // must not re-enter Pause after a resume or station transition.
    if (pause::resolve(false, just(A_BUTTON), just(B_BUTTON)) == pause::kEnter) {
        s_screen = Screen::Pause;
        s_uiRow.pause = 0;
        s_pauseConfirm = false;
        drawPause();
        return;
    }
    // B only opens the chart in free flight. Consume the edge before sampling
    // flight input so travel, air, and ballast do not tick on that frame.
    if (just(B_BUTTON)) {
        s_screen = Screen::Chart;
        drawChart();
        return;
    }

    music::updateFlight(s_state);
    music::resume();

    // D-pad: left/right thrust, up blows ballast (rise), down floods (sink).
    // With the thruster owned, holding A doubles thrust both ways and the
    // ballast valve rate in release and debug builds.
    const uint8_t held = Arduboy2Base::currentButtonState;
    const game::Input input{static_cast<bool>(held & UP_BUTTON), static_cast<bool>(held & DOWN_BUTTON), static_cast<bool>(held & LEFT_BUTTON), static_cast<bool>(held & RIGHT_BUTTON), static_cast<bool>(held & A_BUTTON)};

    const uint8_t beforeCampaign = s_state.campaign;
    const uint8_t beforeSurveys = s_state.surveys;
#if SH_DEV_MODE
    // A is otherwise free in flight. Hold it for a fast traverse; only the
    // boat's position advance scales, so air, currents and the voyage clock
    // keep production pacing. The same held A also drives the thruster boost.
    const uint8_t speedScale = (held & A_BUTTON) ? game::kDevSpeedScale : 1;
    const bool docked = station::stepFlight(s_state, input, s_tables, &s_cache, !s_debugNoClip, speedScale);
#else
    const bool docked = station::stepFlight(s_state, input, s_tables, &s_cache);
#endif
    const uint8_t surveyCard = story_view::cardForNewSurvey(beforeSurveys, s_state.surveys);
    // Coarse reveal rides flight ticks, including the step that docked.
    chart::revealOnStep(s_chart, s_state);
    if (docked) {
        s_stationRow = station::initialRow(s_state);
        // Remember the one-shot beat before the dock checkpoint. If the save
        // fails, the pending card resumes only after RETRY succeeds or the
        // player explicitly continues unsaved.
        const uint8_t milestoneStory = story_view::cardForNewMilestone(beforeCampaign, s_state.campaign);
        s_pendingStory = milestoneStory;
        s_storyReturnPilot = 0;
        if (s_pendingStory == story_view::kCardNone)
            s_pendingStory = surveyCard;
        // Persist only milestone decks. Survey decks are unsaved field finds and
        // stay out of the checkpoint even when the survey completes on the dock
        // frame.
        s_state.pendingStory = milestoneStory;
        // Delivery/repair/service are resolved inside the dock step. Commit
        // that complete post-delivery checkpoint before any board celebration
        // or further input. A failed write stays frozen on the save prompt.
        if (!requestSave(kSaveResumeStation))
            return;
        if (endingPending()) {
            beginEnding();
            return;
        }
        if (s_pendingStory != story_view::kCardNone) {
            beginStory();
            return;
        }
        showStation();
        return;
    }

    // A survey dwell records a fact in the field. Present its deck immediately
    // and return to the frozen pilot frame; the survey remains an unsaved find
    // like any other field discovery.
    if (surveyCard != story_view::kCardNone) {
        s_pendingStory = surveyCard;
        s_storyReturnPilot = 1;
        beginStory();
        return;
    }

    // Zero air is not a death screen, but it must never strand the player.
    // Open the existing pause/rescue menu once on the transition so RESCUE is
    // immediately visible. If the player resumes at zero air, they can still
    // move (down/horizontal) and pause manually; a berth refill re-arms this.
    if (s_state.air <= 0 && !s_airEmptyShown) {
        s_airEmptyShown = true;
        s_screen = Screen::Pause;
        s_uiRow.pause = 1;
        s_pauseConfirm = false;
        drawPause();
        return;
    }

    drawPilot();
}

// Chart frame: no world step, so travel time/air/ballast/jobClock are frozen
// and D-pad is contained. B closes; in the debug image A toggles no-clip. The
// destination X blinks from the Arduboy frame counter because
// game::State.ticks cannot advance here.
static void updateChart() {
    if (just(B_BUTTON)) {
        // A survey WORK row opens the chart while docked; return to the
        // station board rather than dropping the pilot into the world.
        if (s_state.docked >= 0) {
            showStationBoard(station::initialRow(s_state));
            return;
        }
        showPilot();
        return;
    }

#if SH_DEV_MODE
    if (just(A_BUTTON)) {
        s_debugNoClip = !s_debugNoClip;
        drawChart();
        return;
    }
#endif

    if (chart_view::needsRedraw(s_state, static_cast<uint8_t>(Arduboy2Base::frameCount)))
        drawChart();
    else
        s_present = false;
}

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

static void drawWork() {
    work_view::draw(s_state, s_uiRow.work);
}

// Single WORK board: eligible restoration rows only. A accepts the selected
// story row directly; the selected cargo job auto-loads its four units and
// returns to the station board, where UNDOCK/B starts travel. B returns without
// accepting.
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
        if (row.kind == work_view::kRowEnding) {
            beginEnding();
            return;
        }
        // Survey rows are non-cargo leads: show the chart with its blinking
        // survey targets instead of loading a hold.
        if (row.kind == work_view::kRowStory && (row.mission == campaign::kPumpSurvey || row.mission == campaign::kLiftSurvey)) {
            s_screen = Screen::Chart;
            drawChart();
            return;
        }
        if (work_view::accept(s_state, row)) {
            s_stationRow = station::initialRow(s_state);
            // The accepted load owns its departure scene. Persist the scene id
            // with the load before playback, so a cold reload resumes the
            // handoff instead of silently skipping the briefing. Finishing the
            // scene clears it in RAM only (at-least-once, like delivery decks).
            s_pendingStory = story_view::handoffForMission(row.mission);
            s_state.pendingStory = s_pendingStory;
            s_storyReturnPilot = 0;
            if (!requestSave(kSaveResumeStation))
                return;
            if (s_pendingStory != story_view::kSceneNone) {
                beginStory();
                return;
            }
            s_screen = Screen::Station;
            station_view::draw(s_state, s_stationRow);
            return;
        }
    }
    drawWork();
}

// Station frame: no world step, so travel time, air, and ballast are frozen.
// D-pad moves the selection, A confirms, B always undocks.
static void updateStation() {
    if (!justChord(UP_BUTTON | DOWN_BUTTON | A_BUTTON | B_BUTTON)) {
        s_present = false;
        return;
    }

    if (just(UP_BUTTON))
        s_stationRow = station::selectPrevious(s_stationRow, s_state);
    if (just(DOWN_BUTTON))
        s_stationRow = station::selectNext(s_stationRow, s_state);

    if (just(A_BUTTON)) {
        if (station::rowKind(s_stationRow, s_state) == station::kRowUndock) {
            leaveStation();
            return;
        }
        const station::Action action = station::activate(s_stationRow, s_state);
        if (action == station::kActionUndocked) {
            s_screen = Screen::Pilot;
            s_stationRow = station::kFirstRow;
        }
        else if (action == station::kActionWork) {
            s_uiRow.work = 0;
            s_screen = Screen::Work;
            drawWork();
            return;
        }
    }

    if (just(B_BUTTON)) {
        leaveStation();
        return;
    }

    if (s_screen == Screen::Station) {
        station_view::draw(s_state, s_stationRow);
        return;
    }

    // A or B undocked this frame: redraw the frozen pilot view without
    // stepping, so the first visible frame after undock is the station berth.
    drawPilot();
}

// Production splash poll. The splash has no work of its own while it waits, so
// each beat sleeps through timer0 ticks instead of spinning in delay()/micros();
// one idle() returns on the next ~1 ms overflow, and 16 preserve the 16 ms beat.
// service() keeps the menu cue's ATMlib2FX ring fed between frames.
struct BootPoll {
    static uint8_t buttons() {
        return arduboy.buttonsState();
    }
    static void service() {
        music::service();
    }
    static void delay() {
        for (uint8_t i = 0; i < 16; ++i)
            arduboy.idle();
    }
    // Same 50 ms debounce as Arduboy2Base::waitNoButtons(), but idling and
    // still servicing the music ring.
    static void waitNoButtons() {
        do {
            for (uint8_t i = 0; i < 50; ++i) {
                arduboy.idle();
                music::service();
            }
        } while (buttons() != 0);
    }
};

// Shared boot sequence. Production uses the hardware poll; device fixtures may
// inject a deterministic scripted poll so the indefinite splash wait is driven
// through the exact same production path rather than bypassed.
template <typename Poll> static void applicationBootWith(Poll &poll) {
    arduboy.boot();
    arduboy.audio.begin();
    arduboy.setFrameRate(60);
    voyage_save::FxStorage::begin();
    FX::setCursorRange(0, 32767);
    // The title/menu cue starts with the first splash and keeps its position
    // into the title; prompt the same cue here so the fade-in covers the intro.
    music::begin();
    music::setCue(music_logic::kHarbor);
    startup_view::run(poll);

    resetRun();


    // Never read voyage storage in developer mode. Normal boot inspects once so
    // the title can distinguish blank, corrupt and incompatible records before
    // offering CONTINUE or NEW VOYAGE; the checkpoint itself loads only after
    // an explicit CONTINUE.
    refreshBootStatus();
}

static void applicationBoot() {
    BootPoll poll;
    applicationBootWith(poll);
}
