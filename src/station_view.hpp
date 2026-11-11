#pragma once

#include <stdint.h>

#include "game.hpp"
#include "station.hpp"
#include "generated/panel_sprites.hpp"
#include "generated/text_layout.hpp"

// The runtime station board is native: the row rules below own the live
// WORK/UNDOCK and summary/WORK/UNDOCK shapes. The retired generated menu
// frames and hint rows are no longer packed (sub-hauler-jxk.2).

#if defined(__AVR__)
#include "common.hpp"
#endif

// Station board, 128x64. The layout is pure and host-checkable; the draw
// function is device-only because it reads baked FX text stamps.
//
// Header: settlement name.
// Rows:   WORK/UNDOCK, or summary/WORK/UNDOCK when a contract is active.
//         Service-only berths show UNDOCK only. One inverted selection bar.
//         WORK opens the restoration board. B always undocks.
// Bark:   one progression-appropriate line from saved campaign/tool facts.
//         A still confirms the row and B always undocks. Contract acceptance
//         auto-loads the four cargo units, so the active board needs no load
//         row or screen.
namespace station_view {

constexpr uint8_t kNameX = 2;
constexpr uint8_t kHeaderY = 2;
constexpr uint8_t kDividerY = 11;
constexpr uint8_t kRowX = 4;
// Three rows (summary/WORK/UNDOCK) must fit above the bark:
// rows are 22+7*row and the bark starts at 56.
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 7;
constexpr uint8_t kRowInsetY = 0;
constexpr uint8_t kRowInvertH = 7;
constexpr uint8_t kHintX = 3;
constexpr uint8_t kBarkY = 56;
constexpr uint8_t kHintChars = 20;

// Native console chrome. The header keeps the longest berth label
// ("LOWER GANTRY" / "FREIGHT YARD", 12 cells) clear of a 16x8 berth icon, and
// the inert summary row gets its own small marker.
constexpr uint8_t kNameChars = 12;
constexpr uint8_t kBerthIconCount = 8;
constexpr uint8_t kIconX = 110;
constexpr uint8_t kIconY = 2;
constexpr uint8_t kIconW = 16;
constexpr uint8_t kIconH = 8;
constexpr uint8_t kSummaryMarkX = 1;
constexpr uint8_t kSummaryMarkY = 2;
constexpr uint8_t kSummaryMarkW = 2;
constexpr uint8_t kSummaryMarkH = 3;
constexpr uint8_t kSelectMarkX = 121;
constexpr uint8_t kSelectMarkW = 3;

// Row i's text baseline is rowY(i); the invert bar extends kRowInsetY above
// it. Keep the last row clear of the hint and the hint inside the display.
constexpr uint8_t rowY(uint8_t row) {
    return static_cast<uint8_t>(kRowY0 + row * kRowStep);
}

static_assert(station::kStationRowCount > 0, "station board needs at least one row");
static_assert(rowY(static_cast<uint8_t>(station::kStationRowCount - 1)) + kRowInvertH <= kBarkY, "last row clears the bark");
static_assert(rowY(static_cast<uint8_t>(station::kActiveRowCount - 1)) + kRowInvertH <= kBarkY, "active board clears the bark");
static_assert(kBarkY + 8 <= 64, "bark stays on screen");
static_assert(kHintX + kHintChars * 6 <= 128, "the longest bark fits inside 128 columns");
static_assert(kNameX + kNameChars * 6 <= kIconX, "berth names clear the header icon");
static_assert(kIconX + kIconW <= 128, "header icon fits inside the display");
static_assert(kIconY + kIconH <= kDividerY, "header icon stays above the divider");
static_assert(kSummaryMarkX + kSummaryMarkW <= kRowX, "summary marker leaves the row label clear");
static_assert(kSelectMarkX + kSelectMarkW <= 128, "selection marker stays inside the display");

#if defined(__AVR__)

#include "common.hpp"

// Generated AVR settlement records carry position only (name is null), so the
// station board owns the display names. Frame order matches the generated
// `img_berth_names` stamp sheet: campaign::Berth enum order plus STATION.
inline uint8_t nameFor(uint8_t id) {
    if (id < campaign::kBerthCount)
        return id;
    return text_layout::kBerthNameFallback;
}

inline uint8_t rowLabel(uint8_t row, const game::State &state) {
    const station::RowKind kind = station::rowKind(row, state);
    if (kind == station::kRowWork)
        return text_layout::kRowLabelWork;
    if (kind == station::kRowUndock)
        return text_layout::kRowLabelUndock;
    return 0;
}

// One shared bark order: the pure campaign selector chooses a candidate, and
// both the station board and the WORK clue stamp the same `img_dock_barks`
// frame. Frame index is the raw campaign::DockBark value; index 0 means no
// bark and is never drawn.
inline uint8_t dockBarkFrame(campaign::DockBark bark) {
    return static_cast<uint8_t>(bark);
}

// Berth silhouettes are 16x8 opaque icons packed by gen_sprites into the
// panel sprite sheet. Every berth has its own silhouette so a dock reads as a
// place rather than a generic list.
inline void drawBerthIcon(uint8_t id) {
    Blit::draw(kIconX, kIconY, BERTH_ICONS_WIDTH, BERTH_ICONS_HEIGHT,
               berth_icons + 2, id, Blit::OVERWRITE);
}

inline void drawHeader(const game::State &state) {
    text::stampFrame(img_berth_names, nameFor(static_cast<uint8_t>(state.docked)), kNameX, kHeaderY);
    drawBerthIcon(static_cast<uint8_t>(state.docked));
    arduboy.drawFastHLine(0, kDividerY, 128, WHITE);
    arduboy.drawFastVLine(0, static_cast<int16_t>(kDividerY - 2), 3, WHITE);
    arduboy.drawFastVLine(127, static_cast<int16_t>(kDividerY - 2), 3, WHITE);
}

// Inert contract summary: a small left marker keeps it visually distinct from
// the selectable WORK/UNDOCK rows without adding copy or a false action.
inline void drawSummaryMark(uint8_t y) {
    arduboy.fillRect(kSummaryMarkX, static_cast<int16_t>(y + kSummaryMarkY), kSummaryMarkW, kSummaryMarkH, WHITE);
}

// The inverted row still fills the width; a black wedge at the right edge is
// the console cursor and never approaches the longest row label.
inline void drawSelectionMark(uint8_t y) {
    arduboy.fillRect(kSelectMarkX, static_cast<int16_t>(y + 1), 1, 5, BLACK);
    arduboy.fillRect(static_cast<int16_t>(kSelectMarkX + 1), static_cast<int16_t>(y + 2), 1, 3, BLACK);
    arduboy.fillRect(static_cast<int16_t>(kSelectMarkX + 2), static_cast<int16_t>(y + 3), 1, 1, BLACK);
}

// Native board: name, berth icon, live rows and the baked bark row. The
// active summary shows only the destination; there is no payout.
inline void draw(const game::State &state, uint8_t selected) {
    if (state.docked < 0)
        return;

    arduboy.fillRect(0, 0, 128, 64, BLACK);
    drawHeader(state);

    const uint8_t rows = station::rowCount(state);
    for (uint8_t row = 0; row < rows; ++row) {
        const uint8_t y = rowY(row);
        const station::RowKind kind = station::rowKind(row, state);
        const bool highlighted = row == selected && kind != station::kRowJob;
        if (highlighted)
            arduboy.fillRect(0, static_cast<int16_t>(y - kRowInsetY), 128, kRowInvertH, WHITE);
        if (kind == station::kRowJob) {
            drawSummaryMark(y);
            text::stampFrame(img_berth_names, nameFor(state.activeJob.to), kRowX, y);
        } else {
            const uint8_t frame = static_cast<uint8_t>(rowLabel(row, state) + (highlighted ? text_layout::kRowLabelInvertedBase : 0));
            text::stampFrame(img_row_labels, frame, kRowX, y);
        }
        if (highlighted)
            drawSelectionMark(y);
    }

    // One-line baked bark: guidance for the next reachable step. It replaces
    // the retired generated control hint; B still undocks and A still confirms.
    const uint8_t bark = dockBarkFrame(campaign::dockBark(state, static_cast<uint8_t>(state.docked)));
    if (bark != campaign::kDockBarkNone)
        text::stampFrame(img_dock_barks, bark, kHintX, kBarkY);
}

#endif   // __AVR__

}   // namespace station_view
