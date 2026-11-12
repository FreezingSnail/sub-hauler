#pragma once

#include <stdint.h>



#include "game.hpp"
#include "panel_view.hpp"
#include "voyage_flow.hpp"

// Focused 128x64 menu frames for the voyage app slice: title/Continue, save
// failure and pause rescue confirmation. Layout constants are pure; draw() is
// device-only because it uses the external FX font.
namespace voyage_view {

enum TitleMode : uint8_t {
    kTitleMenu,
    kTitleConfirmReplace,
};

enum TitleRow : uint8_t {
    kTitleContinue,
    kTitleNewVoyage,
    kTitleRowCount,
};

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kTitleY = 3;
constexpr uint8_t kHullY = 12;
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 9;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kStatusY = 48;
constexpr uint8_t kHintY = 57;

static_assert(kTitleY + 7 <= kHullY, "title clears the hull line");
static_assert(kHullY + 7 <= kRowY0, "hull clears the menu");
static_assert(kRowY0 + kRowStep + kRowInvertH <= kStatusY, "two title rows clear status");
static_assert(kStatusY + 7 <= kHintY, "status clears the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");

inline bool continueAvailable(voyage_flow::BootState status) {
    return status == voyage_flow::kBootWritten;
}

// Skip CONTINUE when no written slot exists. NEW VOYAGE is always selectable.
inline uint8_t nextRow(uint8_t selected, voyage_flow::BootState status) {
    if (selected == kTitleContinue)
        return kTitleNewVoyage;
    return continueAvailable(status) ? kTitleContinue : kTitleNewVoyage;
}

inline uint8_t previousRow(uint8_t selected, voyage_flow::BootState status) {
    if (selected == kTitleContinue)
        return kTitleNewVoyage;
    return continueAvailable(status) ? kTitleContinue : kTitleNewVoyage;
}

#if defined(__AVR__)
#include "common.hpp"

// Baked title frames: blank/written boot state x selected row, HAULER fixed. The
// replace prompt exists only when a valid save is present, so it is one frame.
inline void drawTitle(voyage_flow::BootState status, uint8_t selected, TitleMode mode) {
    if (mode == kTitleConfirmReplace) {
        panel_view::draw(title_replace, 0, 64);
        return;
    }
    const uint16_t frame = static_cast<uint16_t>(static_cast<uint8_t>(status) * 2u + selected);
    panel_view::draw(title_menu, frame, 64);
}

// Pause keeps the frozen prompt chrome and adds a live MUSIC row. With music
// enabled the six generated frames cover both setting states and every row.
inline void drawPauseRescue(uint8_t selected, bool musicOn = true) {
#if defined(__AVR__)
    panel_view::draw(pause_menu, static_cast<uint16_t>((musicOn ? 3u : 0u) + selected), 64);
#endif
}

inline void drawRescueConfirm() {
    panel_view::draw(panel_rescue_confirm);
}

#endif   // __AVR__

}   // namespace voyage_view
