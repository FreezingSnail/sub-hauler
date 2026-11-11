#pragma once

#include <stdint.h>



#include "game.hpp"
#include "jobs.hpp"
#include "campaign.hpp"
#include "generated/exploration.hpp"

#if defined(__AVR__)

#include "common.hpp"
#include "station_view.hpp"
#include "text.hpp"

#endif

// The single WORK board. Pure row rules stay host-testable; the device draw is
// below the __AVR__ boundary because it uses the external FX font.
//
// Story restoration jobs are listed in stable mission order. There are no
// offer rows, no detail step, no deadline, no clock and no reward.
namespace work_view {

constexpr uint8_t kNoMission = 0xFF;
constexpr uint8_t kRows = 4;
constexpr uint8_t kRowX = 2;
constexpr uint8_t kRowY0 = 14;
constexpr uint8_t kRowStep = 8;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kClueY = 47;
constexpr uint8_t kHintY = 56;

static_assert(kRowY0 + (kRows - 1) * kRowStep + kRowInvertH <= kClueY, "four work rows clear the clue");
static_assert(kClueY + 7 <= kHintY, "clue clears the hint");
static_assert(kHintY + 7 <= 64, "work hint stays on screen");
static_assert(kRowX + 21 * 6 == 128, "the longest CARGO row fills exactly 128 columns");

enum RowKind : uint8_t {
    kRowNone,
    kRowStory,
    kRowEnding,
};

struct Row {
    uint8_t kind;
    uint8_t mission;   // story mission id, or kMissionNone
};

inline bool jobActive(const game::State &state) {
    return game::hasActiveJob(state);
}

inline uint8_t storyCount(const game::State &state) {
    if (state.docked < 0)
        return 0;
    uint8_t count = 0;
    while (count < 0xFF && campaign::workMissionAt(state, static_cast<uint8_t>(state.docked), count) != campaign::kMissionNone)
        ++count;
    return count;
}

// Accepting any job loads the cargo and clears the row list. The WORK screen
// then shows the carrying summary, so there is no second active list.
inline uint8_t rowCount(const game::State &state) {
    if (jobActive(state))
        return 0;
    return storyCount(state);
}

inline Row rowAt(const game::State &state, uint8_t index) {
    Row row{kRowNone, kNoMission};
    if (jobActive(state))
        return row;

    if (index < storyCount(state)) {
        const uint8_t mission = campaign::workMissionAt(state, static_cast<uint8_t>(state.docked), index);
        row.kind = mission == campaign::kWorkEnding ? kRowEnding : kRowStory;
        row.mission = mission;
    }
    return row;
}

// Dispatch A on a WORK row. Ending rows are presentation-only and return false;
// the sketch opens the cards itself. Survey story rows are also presentation
// leads: the sketch opens the chart, and recording still happens in the field.
inline bool accept(game::State &state, const Row &row) {
    if (row.kind == kRowStory)
        return campaign::accept(state, row.mission);
    return false;
}

// Persistent, host-testable clue selection. The board shows this at every
// berth, including when the row list is empty: a completed repair must never
// hide the next survey, source, seal or beacon step. Deep Anchor keeps a
// beacon-state clue before and after the core delivery.
// The WORK clue and the dock bark are one selector and one enum. Campaign owns
// the pure choice; these aliases keep the frozen WORK-board tests and callers.
using Clue = campaign::DockBark;
constexpr Clue kClueNone = campaign::kDockBarkNone;
constexpr Clue kClueFirst = campaign::kDockBarkFirst;
constexpr Clue kClueBenthosLamps = campaign::kDockBarkBenthosLamps;
constexpr Clue kCluePumpSurvey = campaign::kDockBarkPumpSurvey;
constexpr Clue kCluePumpSource = campaign::kDockBarkPumpSource;
constexpr Clue kClueLiftSurvey = campaign::kDockBarkLiftSurvey;
constexpr Clue kClueLiftSource = campaign::kDockBarkLiftSource;
constexpr Clue kCluePumpPartReady = campaign::kDockBarkPumpPartReady;
constexpr Clue kClueLiftRestored = campaign::kDockBarkLiftRestored;
constexpr Clue kClueBeaconNeedsSeal = campaign::kDockBarkBeaconNeedsSeal;
constexpr Clue kClueBeaconReady = campaign::kDockBarkBeaconReady;
constexpr Clue kClueBeaconPlan = campaign::kDockBarkBeaconPlan;
constexpr Clue kClueBeaconLit = campaign::kDockBarkBeaconLit;
constexpr Clue kClueNetworkOn = campaign::kDockBarkNetworkOn;
constexpr Clue kClueDeepRelay = campaign::kDockBarkDeepRelay;
constexpr Clue kClueNoWork = campaign::kDockBarkNoWork;

enum EquipmentClue : uint8_t {
    kEquipmentNone = 0,
    kEquipmentCutter,
    kEquipmentSeal,
};

inline Clue clueFor(const game::State &state, uint8_t berth) {
    return campaign::dockBark(state, berth);
}

inline EquipmentClue equipmentClue(const game::State &state) {
    if (!(state.tools & exploration_data::CUTTER))
        return kEquipmentCutter;
    if (!campaign::sealOwned(state))
        return kEquipmentSeal;
    return kEquipmentNone;
}

#if defined(__AVR__)

// Branches, not a switch: the story labels would emit a RAM-resident CSWTCH
// table. Survey rows use their own labels and never load cargo.
inline uint24_t missionLabel(uint8_t mission) {
    if (mission == campaign::kFirst)
        return txt_first;
    if (mission == campaign::kPumpSurvey)
        return txt_pump_survey;
    if (mission == campaign::kPump)
        return txt_pump;
    if (mission == campaign::kLiftSurvey)
        return txt_lift_survey;
    if (mission == campaign::kLift)
        return txt_lift;
    if (mission == campaign::kBeacon)
        return txt_beacon;
    if (mission == campaign::kHome)
        return txt_home;
    if (mission == campaign::kExpedition)
        return txt_deep_relay;
    if (mission == campaign::kWorkEnding)
        return txt_view_ending;
    return 0;
}

// One PROGMEM dispatch shared with the station bark. The pure clue order is
// fixed by docs/progression.md and tested on the host, including the empty-row
// board case.
inline uint24_t clueLabel(Clue clue) {
    return station_view::dockBarkText(clue);
}

inline uint24_t equipmentLabel(EquipmentClue clue) {
    if (clue == kEquipmentCutter)
        return txt_cutter_rumor;
    if (clue == kEquipmentSeal)
        return txt_seal_rumor;
    return 0;
}

inline void drawHeader(const game::State &state) {
    text::normal();
    text::at(2, 2);
    text::print(station_view::nameFor(static_cast<uint8_t>(state.docked)));
    text::character(' ');
    text::print(txt_work);
    arduboy.drawFastHLine(0, 11, 128, WHITE);
}

inline void drawActive(const game::State &state) {
    text::normal();
    text::at(kRowX, kRowY0);
    text::print(txt_carrying);
    text::character(' ');
    text::print(missionLabel(state.activeJob.mission));

    text::at(kRowX, static_cast<uint8_t>(kRowY0 + kRowStep));
    text::print(txt_deliver_to);
    text::at(kRowX, static_cast<uint8_t>(kRowY0 + 2 * kRowStep));
    text::print(station_view::nameFor(state.activeJob.to));

    const uint24_t clue = clueLabel(clueFor(state, static_cast<uint8_t>(state.docked)));
    if (clue) {
        text::at(kRowX, kClueY);
        text::print(clue);
    }
    text::at(3, kHintY);
    text::print(txt_back_hint);
}

inline void drawStoryRow(uint8_t y, const Row &row, bool selected) {
    if (selected) {
        arduboy.fillRect(0, y, 128, kRowInvertH, WHITE);
        text::inverted();
    } else {
        text::normal();
    }
    text::at(kRowX, y);
    text::print(txt_story);
    text::character(' ');
    text::print(missionLabel(row.mission));
    text::normal();
}

inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    drawHeader(state);

    if (jobActive(state)) {
        drawActive(state);
        return;
    }

    const uint8_t count = rowCount(state);
    if (count == 0) {
        // A completed repair can empty this berth before the next branch is
        // ready. Keep the persistent clue and the equipment lead visible so a
        // revisit never dead-ends on the back hint alone.
        const uint24_t clue = clueLabel(clueFor(state, static_cast<uint8_t>(state.docked)));
        const uint24_t equipment = clue ? 0 : equipmentLabel(equipmentClue(state));
        const uint24_t lead = clue ? clue : equipment;
        if (lead) {
            text::at(3, kClueY);
            text::print(lead);
        }
        text::at(3, kHintY);
        text::print(txt_back_hint);
        return;
    }
    if (selected >= count)
        selected = 0;

    // Keep the selection visible without a second page: the window starts at
    // the selected row and shows up to kRows entries. Current mission counts
    // fit, but the rule stays correct if a berth gains another story row.
    const uint8_t first = selected >= kRows ? static_cast<uint8_t>(selected - kRows + 1) : 0;
    uint8_t visible = 0;
    for (uint8_t index = first; index < count && visible < kRows; ++index, ++visible) {
        const Row row = rowAt(state, index);
        const uint8_t y = static_cast<uint8_t>(kRowY0 + visible * kRowStep);
        if (row.kind == kRowStory || row.kind == kRowEnding)
            drawStoryRow(y, row, index == selected);
    }

    const uint24_t clue = clueLabel(clueFor(state, static_cast<uint8_t>(state.docked)));
    const uint24_t hint = clue ? clue : equipmentLabel(equipmentClue(state));
    if (hint) {
        text::at(3, kClueY);
        text::print(hint);
    }

    text::at(3, kHintY);
    const uint8_t mission = rowAt(state, selected).mission;
    text::print(mission == campaign::kPumpSurvey || mission == campaign::kLiftSurvey ? txt_survey_chart_hint : txt_work_accept_hint);
}

#endif   // __AVR__

}   // namespace work_view

