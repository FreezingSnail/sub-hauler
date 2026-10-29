#pragma once

#include <stdint.h>

#include "features.hpp"

#if SH_FEATURE_SAVE_VOYAGE

#include "game.hpp"
#include "panel_view.hpp"
#include "roster.hpp"
#include "voyage_flow.hpp"

// Focused 128x64 menu frames for the voyage app slice: title/Continue, save
// failure, pause rescue confirmation and wreck recovery. Layout constants are
// pure; draw() is device-only because it uses the external FX font.
namespace voyage_view {

enum TitleMode : uint8_t {
    kTitleMenu,
    kTitleConfirmReplace,
};

enum TitleRow : uint8_t {
    kTitleContinue,
    kTitleNewVoyage,
    kTitleLoader,
    kTitleRowCount,
};

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kCenterIndent = 4;
constexpr uint8_t kTitleY = 3;
constexpr uint8_t kHullY = 12;
constexpr uint8_t kRowY0 = 22;
constexpr uint8_t kRowStep = 9;
constexpr uint8_t kRowInvertH = 8;
constexpr uint8_t kStatusY = 48;
constexpr uint8_t kHintY = 57;

constexpr uint8_t kMenuTitleY = 8;
constexpr uint8_t kMenuRowY0 = 24;
constexpr uint8_t kMenuRowStep = 12;
constexpr uint8_t kConfirmY = 24;

static_assert(kTitleY + 7 <= kHullY, "title clears the hull line");
static_assert(kHullY + 7 <= kRowY0, "hull clears the menu");
static_assert(kRowY0 + 2 * kRowStep + kRowInvertH <= kStatusY, "three title rows clear status");
static_assert(kStatusY + 7 <= kHintY, "status clears the hint");
static_assert(kHintY + 7 <= 64, "hint stays on screen");
static_assert(kMenuRowY0 + kMenuRowStep + kRowInvertH <= kHintY, "two prompt rows clear the hint");
#if SH_FEATURE_MUSIC
static_assert(kMenuRowY0 + 2 * kMenuRowStep + kRowInvertH <= kHintY, "music row clears the hint");
#endif

inline bool continueAvailable(const voyage_flow::BootStatus &status) {
    return status.hasValid;
}

// Skip CONTINUE when no valid slot exists. NEW VOYAGE and LOADER are always
// selectable; LOADER is the in-sketch bootloader entry.
inline uint8_t nextRow(uint8_t selected, const voyage_flow::BootStatus &status) {
    if (selected == kTitleContinue)
        return kTitleNewVoyage;
    if (selected == kTitleNewVoyage)
        return kTitleLoader;
    return continueAvailable(status) ? kTitleContinue : kTitleNewVoyage;
}

inline uint8_t previousRow(uint8_t selected, const voyage_flow::BootStatus &status) {
    if (selected == kTitleContinue)
        return kTitleLoader;
    if (selected == kTitleNewVoyage)
        return continueAvailable(status) ? kTitleContinue : kTitleLoader;
    return kTitleNewVoyage;
}

#if defined(__AVR__)
#include "common.hpp"

inline void centered(uint8_t y, uint24_t string) {
    // Callers only use fixed strings of at most 20 characters. The 6 px cell
    // and one leading space make the common labels visually centred.
    text::at(kCenterIndent, y);
    text::print(string);
}

// Branches, not a switch: the labels are 24-bit FX addresses and a CSWTCH
// table would be copied to SRAM at startup.
inline uint24_t statusLabel(const voyage_flow::BootStatus &status) {
    if (status.state == voyage_flow::kBootValid)
        return txt_save_ready;
    if (status.state == voyage_flow::kBootCorrupt)
        return txt_save_corrupt;
    if (status.state == voyage_flow::kBootIncompatible)
        return txt_save_incompatible;
    return txt_no_save;
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

#if SH_FEATURE_MULTI_HULL
// Baked title frames: status enum 0..4 x hull 0..2 x selected row 0..2. The two
// confirmation modes use their own sheets; replace embeds the status line.
inline void drawTitle(const voyage_flow::BootStatus &status, uint8_t selected, uint8_t hullClass, TitleMode mode) {
    if (mode == kTitleConfirmReplace) {
        panel_view::draw(title_replace, status.state, 64);
        return;
    }
    const uint8_t hull = static_cast<uint8_t>(roster::sanitize(static_cast<roster::HullClass>(hullClass)));
    const uint16_t frame = static_cast<uint16_t>(status.state * 9u + hull * 3u + selected);
    panel_view::draw(title_menu, frame, 64);
}
#else
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

    menuRow(kRowY0, txt_continue, selected == kTitleContinue);
    menuRow(static_cast<uint8_t>(kRowY0 + kRowStep), txt_new_voyage, selected == kTitleNewVoyage);
    menuRow(static_cast<uint8_t>(kRowY0 + 2 * kRowStep), txt_loader, selected == kTitleLoader);

    text::normal();
    centered(kStatusY, statusLabel(status));
#if SH_FEATURE_MULTI_HULL
    centered(kHintY, txt_title_save_hint);
#else
    centered(kHintY, txt_save_hint);
#endif
}
#endif

// The prompt body, both row labels, the hint and the selected-row bar are all
// baked into one generated panel. Selecting the other row is a different image,
// not a second composition pass.
inline void drawSaveFailed(uint8_t selected) {
    panel_view::draw(selected == 0 ? panel_save_retry : panel_save_continue);
}

// Pause keeps the frozen prompt chrome and adds a live MUSIC row. With music
// enabled the six generated frames cover both setting states and every row.
inline void drawPauseRescue(uint8_t selected, bool musicOn = true) {
#if SH_FEATURE_MUSIC
#if defined(__AVR__)
    panel_view::draw(pause_menu, static_cast<uint16_t>((musicOn ? 3u : 0u) + selected), 64);
#endif
#else
    text::normal();
    centered(kMenuTitleY, txt_paused);
    menuRow(kMenuRowY0, txt_resume, selected == 0);
    menuRow(static_cast<uint8_t>(kMenuRowY0 + kMenuRowStep), txt_rescue, selected == 1);
    text::normal();
    centered(kHintY, txt_pause_hint);
#endif
}

inline void drawWreck(uint8_t selected) {
    panel_view::draw(selected == 0 ? panel_wreck_retry : panel_wreck_title);
}

inline void drawRescueConfirm() {
    panel_view::draw(panel_rescue_confirm);
}

inline void drawAbandonConfirm() {
    panel_view::draw(panel_abandon_confirm);
}

#endif   // __AVR__

}   // namespace voyage_view

#endif   // SH_FEATURE_SAVE_VOYAGE
