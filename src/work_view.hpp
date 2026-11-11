#pragma once

#include <stdint.h>



#include "game.hpp"
#include "jobs.hpp"
#include "campaign.hpp"
#include "generated/exploration.hpp"
#include "generated/text_layout.hpp"

#if defined(__AVR__)

#include "common.hpp"
#include "station_view.hpp"
#include "text.hpp"

#endif

// The single WORK board. Pure row rules stay host-testable; the device draw is
// below the __AVR__ boundary because it stamps baked FX text images.
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
// berth, including when the row list is empty: the line always describes the
// current voyage stage. The berth chooses its own copy in story/dock_hints.json;
// campaign owns the pure point selection.
using Clue = campaign::DockPoint;
constexpr Clue kClueFresh = campaign::kDockPointFresh;
constexpr Clue kClueFirst = campaign::kDockPointFirst;
constexpr Clue kCluePump = campaign::kDockPointPump;
constexpr Clue kClueLift = campaign::kDockPointLift;
constexpr Clue kClueBoth = campaign::kDockPointBoth;
constexpr Clue kClueSeal = campaign::kDockPointSeal;
constexpr Clue kClueCore = campaign::kDockPointCore;
constexpr Clue kClueRelay = campaign::kDockPointRelay;
constexpr Clue kClueHome = campaign::kDockPointHome;

enum EquipmentClue : uint8_t {
    kEquipmentNone = 0,
    kEquipmentCutter,
    kEquipmentSeal,
};

inline Clue clueFor(const game::State &state) {
    return campaign::dockPoint(state);
}

inline EquipmentClue equipmentClue(const game::State &state) {
    if (!(state.tools & exploration_data::CUTTER))
        return kEquipmentCutter;
    if (!campaign::sealOwned(state))
        return kEquipmentSeal;
    return kEquipmentNone;
}

#if defined(__AVR__)

// Frame index for the STORY/CARRY composite sheets. Mission ids 0..7 map
// directly; the VIEW ENDING row uses the final frame (kWorkEnding is outside
// the frozen mission ids).
inline uint8_t missionFrame(uint8_t mission) {
    if (mission == campaign::kWorkEnding)
        return static_cast<uint8_t>(text_layout::kMissionFrames - 1);
    return mission;
}

// One dispatch shared with the station bark. The pure point order is fixed by
// campaign::DockPoint and tested on the host, including the empty-row board
// case. The berth selects its unique pre-rendered frame from the JSON matrix.
inline uint8_t clueFrame(const game::State &state, Clue clue) {
    return text_layout::dockBarkFrame(static_cast<uint8_t>(state.docked), static_cast<uint8_t>(clue));
}

// Equipment leads are individual img_txt_* stamps; 0 means no lead.
inline uint24_t equipmentImage(EquipmentClue clue) {
    uint24_t image = 0;
    if (clue == kEquipmentCutter)
        image = img_txt_cutter_rumor;
    else if (clue == kEquipmentSeal)
        image = img_txt_seal_rumor;
    return image;
}

// Full-row string stamps are baked with their glyph origin at x=3 and draw at
// the cursor's x. Sheets (berth names, barks, row labels and composites) take
// their exact screen position in stampFrame().
inline void drawHeader(const game::State &state) {
    text::stampFrame(img_berth_work, static_cast<uint8_t>(state.docked), 2, 2);
    arduboy.drawFastHLine(0, 11, 128, WHITE);
}

inline void drawActive(const game::State &state) {
    text::stampFrame(img_carry_row, missionFrame(state.activeJob.mission), kRowX, kRowY0);

    text::at(kRowX, static_cast<uint8_t>(kRowY0 + kRowStep));
    text::stamp(img_txt_deliver_to);
    text::stampFrame(img_berth_names, state.activeJob.to, kRowX, static_cast<uint8_t>(kRowY0 + 2 * kRowStep));

    const uint8_t clue = clueFrame(state, clueFor(state));
    if (clue != 0)
        text::stampFrame(img_dock_barks, clue, kRowX, kClueY);
    text::at(3, kHintY);
    text::stamp(img_txt_back_hint);
}

inline void drawStoryRow(uint8_t y, const Row &row, bool selected) {
    if (selected)
        arduboy.fillRect(0, y, 128, kRowInvertH, WHITE);
    const uint8_t frame = static_cast<uint8_t>(missionFrame(row.mission) + (selected ? text_layout::kStoryRowInvertedBase : 0));
    text::stampFrame(img_story_row, frame, kRowX, y);
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
        const uint8_t clue = clueFrame(state, clueFor(state));
        const uint24_t equipment = clue != 0 ? 0 : equipmentImage(equipmentClue(state));
        if (clue != 0) {
            text::stampFrame(img_dock_barks, clue, 3, kClueY);
        } else if (equipment) {
            text::at(3, kClueY);
            text::stamp(equipment);
        }
        text::at(3, kHintY);
        text::stamp(img_txt_back_hint);
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

    const uint8_t clue = clueFrame(state, clueFor(state));
    if (clue != 0) {
        text::stampFrame(img_dock_barks, clue, 3, kClueY);
    } else {
        const uint24_t equipment = equipmentImage(equipmentClue(state));
        if (equipment) {
            text::at(3, kClueY);
            text::stamp(equipment);
        }
    }

    text::at(3, kHintY);
    const uint8_t mission = rowAt(state, selected).mission;
    text::stamp(mission == campaign::kPumpSurvey || mission == campaign::kLiftSurvey ? img_txt_survey_chart_hint : img_txt_work_accept_hint);
}

#endif   // __AVR__

}   // namespace work_view

