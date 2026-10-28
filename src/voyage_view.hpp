#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include "game.hpp"
#include "roster.hpp"
#include "voyage_flow.hpp"

// Focused 128x64 menu frames for the voyage app slice: title/Continue, save
// failure, pause rescue confirmation and wreck recovery. Layout constants are
// pure; draw() is device-only because it uses the external FX font.
namespace voyage_view {

enum TitleMode : uint8_t {
    kTitleMenu,
    kTitleConfirmReplace,
    kTitleKeepChart,
};

enum TitleRow : uint8_t {
    kTitleContinue,
    kTitleNewVoyage,
    kTitleRowCount,
};

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kCenterIndent = 4;
constexpr uint8_t kTitleY = 4;
constexpr uint8_t kHullY = 16;
constexpr uint8_t kRowY0 = 28;
constexpr uint8_t kRowStep = 9;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kStatusY = 47;
constexpr uint8_t kHintY = 56;

constexpr uint8_t kMenuTitleY = 8;
constexpr uint8_t kMenuRowY0 = 24;
constexpr uint8_t kMenuRowStep = 12;
constexpr uint8_t kConfirmY = 24;

static_assert(kTitleY + 7 <= kHullY, "title clears the hull line");
static_assert(kHullY + 7 <= kRowY0, "hull clears the menu");
static_assert(kRowY0 + kRowStep + kRowInvertH <= kStatusY, "two title rows clear status");
static_assert(kStatusY + 7 <= kHintY, "status clears the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");
static_assert(kMenuRowY0 + kMenuRowStep + kRowInvertH <= kHintY, "two prompt rows clear the hint");

inline bool continueAvailable(const voyage_flow::BootStatus &status) {
    return status.hasValid;
}

// Skip CONTINUE when no valid slot exists. NEW VOYAGE is always selectable.
inline uint8_t nextRow(uint8_t selected, const voyage_flow::BootStatus &status) {
    if (!continueAvailable(status))
        return kTitleNewVoyage;
    return selected == kTitleContinue ? kTitleNewVoyage : kTitleContinue;
}

inline uint8_t previousRow(uint8_t selected, const voyage_flow::BootStatus &status) {
    return nextRow(selected, status);
}

#if defined(__AVR__)
#include "common.hpp"

inline void centered(uint8_t y, uint24_t string) {
    // Callers only use fixed strings of at most 20 characters. The 6 px cell
    // and one leading space make the common labels visually centred.
    text::at(kCenterIndent, y);
    text::print(string);
}

inline uint24_t statusLabel(const voyage_flow::BootStatus &status) {
    switch (status.state) {
    case voyage_flow::kBootValid:
        return txt_save_ready;
    case voyage_flow::kBootCorrupt:
        return txt_save_corrupt;
    case voyage_flow::kBootIncompatible:
        return txt_save_incompatible;
    case voyage_flow::kBootUnreadable:
        return txt_save_unreadable;
    default:
        return txt_no_save;
    }
}

inline uint24_t hullName(uint8_t hullClass) {
#if SH_FEATURE_MULTI_HULL
    const roster::HullClass hull = roster::sanitize(static_cast<roster::HullClass>(hullClass));
    if (hull == roster::kMinnow)
        return txt_minnow;
    if (hull == roster::kLeviathan)
        return txt_leviathan;
#else
    (void)hullClass;
#endif
    return txt_hauler;
}

inline void menuRow(uint8_t y, uint24_t label, bool selected) {
    if (selected) {
        arduboy.fillRect(0, y - 1, 128, kRowInvertH, WHITE);
        text::inverted();
    } else {
        text::normal();
    }
    text::at(kCenterIndent, y);
    text::print(label);
}

inline void drawTitle(const voyage_flow::BootStatus &status, uint8_t selected, uint8_t hullClass, TitleMode mode) {
    text::normal();
    centered(kTitleY, txt_sub_hauler);

#if SH_FEATURE_MULTI_HULL
    centered(kHullY, hullName(hullClass));
#else
    (void)hullClass;
#endif

    if (mode == kTitleConfirmReplace) {
        centered(kConfirmY, txt_replace_voyage);
        centered(static_cast<uint8_t>(kConfirmY + 12), statusLabel(status));
        centered(static_cast<uint8_t>(kConfirmY + 24), txt_yes_no);
        return;
    }

    if (mode == kTitleKeepChart) {
        centered(kConfirmY, txt_keep_chart);
        centered(static_cast<uint8_t>(kConfirmY + 12), txt_yes_no);
        return;
    }

    menuRow(kRowY0, txt_continue, selected == kTitleContinue);
    menuRow(static_cast<uint8_t>(kRowY0 + kRowStep), txt_new_voyage, selected == kTitleNewVoyage);

    text::normal();
    centered(kStatusY, statusLabel(status));
#if SH_FEATURE_MULTI_HULL
    centered(kHintY, txt_title_save_hint);
#else
    centered(kHintY, txt_save_hint);
#endif
}

inline void drawPrompt(uint24_t title, uint24_t first, uint24_t second, uint8_t selected, uint24_t hintLabel) {
    text::normal();
    centered(kMenuTitleY, title);
    menuRow(kMenuRowY0, first, selected == 0);
    menuRow(static_cast<uint8_t>(kMenuRowY0 + kMenuRowStep), second, selected == 1);
    text::normal();
    centered(kHintY, hintLabel);
}

inline void drawSaveFailed(uint8_t selected) {
    drawPrompt(txt_save_failed, txt_retry, txt_continue_unsaved, selected, txt_save_hint);
}

inline void drawPauseRescue(uint8_t selected) {
    drawPrompt(txt_paused, txt_resume, txt_rescue, selected, txt_pause_hint);
}

inline void drawWreck(uint8_t selected) {
    drawPrompt(txt_hull_lost, txt_retry_berth, txt_title, selected, txt_save_hint);
}

inline void drawConfirm(uint24_t title, uint24_t line) {
    text::normal();
    centered(kMenuTitleY, title);
    centered(kConfirmY, line);
    centered(static_cast<uint8_t>(kConfirmY + 14), txt_yes_no);
}

inline void drawRescueConfirm() {
    drawConfirm(txt_back_last_berth, txt_unsaved_lost);
}

inline void drawAbandonConfirm() {
    text::normal();
    centered(kMenuTitleY, txt_abandon_job);
    centered(kConfirmY, txt_yes_no);
}

#endif   // __AVR__

}   // namespace voyage_view

#endif   // SH_FEATURE_SAVE_VOYAGE
