#pragma once

#include <stdint.h>

#include "cargo.hpp"
#include "cargo_screen.hpp"
#include "features.hpp"

#if SH_FEATURE_CARGO_LOAD

// HAULER cargo cutaway view: pure 128x64 1-bit composition plus the device text
// pass. No Arduino headers here, so host tests draw into a plain page-major
// buffer and inspect exact pixels. The shell is schematic: one pressure hull,
// the 11x3 hold at 6 px cells, bulkheads, bow cabin, engine room, ballast, and
// the top hatch. Interaction is owned by the M5 rules bead.
namespace cargo_view {

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kScreenH = 64;
constexpr uint16_t kBufferBytes = kScreenW * kScreenH / 8;

// Pressure hull. The outline is a stepped capsule; the interior stays black so
// the hold and machinery read as a cutaway.
constexpr uint8_t kHullX = 4;
constexpr uint8_t kHullY = 10;
constexpr uint8_t kHullW = 120;
constexpr uint8_t kHullH = 33;
constexpr uint8_t kCorner = 10;

// HAULER hold: 11x3 cells at 6 px, centred inside the hull.
constexpr uint8_t kCell = 6;
constexpr uint8_t kHoldX = 31;
constexpr uint8_t kHoldY = 19;
constexpr uint8_t kHoldW = kCell * CARGO_HAULER_COLS;
constexpr uint8_t kHoldH = kCell * CARGO_HAULER_ROWS;
constexpr uint8_t kHoldRight = static_cast<uint8_t>(kHoldX + kHoldW);
constexpr uint8_t kHoldBottom = static_cast<uint8_t>(kHoldY + kHoldH);

// Structural split/notch bulkheads (blocked cells: col 0 row 0, col 5 all
// rows, col 10 row 0). The column bit is drawn as a double bulkhead line.
constexpr uint8_t kSplitCol = 5;
constexpr uint8_t kSplitX = static_cast<uint8_t>(kHoldX + kSplitCol * kCell);

// Interior machinery. Both compartments sit beside the hold, inside the hull.
constexpr uint8_t kDeckY = 16;
constexpr uint8_t kBulkLX = static_cast<uint8_t>(kHoldX - 4);
constexpr uint8_t kBulkRX = static_cast<uint8_t>(kHoldRight + 3);
constexpr uint8_t kCabinX = 10;
constexpr uint8_t kCabinY = 21;
constexpr uint8_t kCabinW = 16;
constexpr uint8_t kCabinH = 8;
constexpr uint8_t kEngineX = 102;
constexpr uint8_t kEngineY = 21;
constexpr uint8_t kEngineW = 16;
constexpr uint8_t kEngineH = 9;

// Hatch posts above the hold opening.
constexpr uint8_t kHatchLX = 54;
constexpr uint8_t kHatchRX = 74;
constexpr uint8_t kHatchY = 10;
constexpr uint8_t kHatchBottom = 18;

// Text slots. Arduboy 5x7 glyphs advance 6 px; every slot is checked against
// the display edges and its neighbours with static_assert below.
constexpr uint8_t kNameX = 2;
constexpr uint8_t kNameY = 1;
constexpr uint8_t kNameChars = 6; // "HAULER"
constexpr uint8_t kStatusX = 60;
constexpr uint8_t kStatusY = 1;
constexpr uint8_t kStatusChars = 11; // "R 0/6 B 0/1"
constexpr uint8_t kLoadX = 2;
constexpr uint8_t kLoadY = 49;
constexpr uint8_t kLoadChars = 10; // "LOAD 26/26"
constexpr uint8_t kMessageX = 2;
constexpr uint8_t kMessageY = 57;
constexpr uint8_t kMessageChars = 14; // longest feedback: "NEEDS SUPPORT"
constexpr uint8_t kPromptX = 90;
constexpr uint8_t kPromptY = 57;
constexpr uint8_t kPromptChars = 6; // "HOLD B"
// Commit prompt replaces the feedback slot only while the required load is
// complete and nothing is held, so it never competes with a longer message.
constexpr uint8_t kCommitPromptX = kMessageX;
constexpr uint8_t kCommitPromptY = kMessageY;
constexpr uint8_t kCommitPromptChars = 8; // "A+B LOAD"

// Unplaced-piece tray: up to eight 8x8 slots.
constexpr uint8_t kTrayX = 64;
constexpr uint8_t kTrayY = 48;
constexpr uint8_t kTraySlot = 8;
constexpr uint8_t kTraySlots = cargo::MAX_PIECES;

static_assert(kHullX + kHullW <= kScreenW, "hull fits the display");
static_assert(kHullY > kNameY + 7, "hull clears the header text");
static_assert(kHoldX > kHullX + kCorner, "hold clears the bow shell");
static_assert(kHoldRight + kCorner < kHullX + kHullW, "hold clears the stern shell");
static_assert(kHoldY > kHullY, "hold sits below the deck");
static_assert(kHoldBottom + 4 <= kHullY + kHullH, "hold and ballast stay inside the hull");
static_assert(kHoldW == 11 * 6, "HAULER hold is 11 cells at 6 px");
static_assert(kHoldH == 3 * 6, "HAULER hold is 3 cells tall");
static_assert(kHoldX == 31, "hold stays centred in the cutaway");
static_assert(kBulkLX > kCabinX + kCabinW, "cabin sits forward of the hold");
static_assert(kEngineX > kBulkRX, "engine room sits aft of the hold");
static_assert(kEngineX + kEngineW < kHullX + kHullW - 1, "engine room stays inside the hull");
static_assert(kHoldBottom + 2 < kTrayY, "tray clears the hull");
static_assert(kTrayX + kTraySlots * kTraySlot <= kScreenW, "tray fits the display");
static_assert(kTrayY + kTraySlot <= kPromptY, "tray clears the prompt row");
static_assert(kPromptY + 7 <= kScreenH, "prompt fits the display");
static_assert(kNameX + kNameChars * 6 < kStatusX, "header name clears the counters");
static_assert(kStatusX + kStatusChars * 6 <= kScreenW - 2, "counters fit the display");
static_assert(kLoadX + kLoadChars * 6 <= kTrayX, "load text clears the tray");
static_assert(kMessageX + kMessageChars * 6 <= kPromptX, "feedback clears the exit hint");
static_assert(kCommitPromptX + kCommitPromptChars * 6 <= kScreenW - 2, "commit prompt fits the display");
static_assert(kPromptX + kPromptChars * 6 <= kScreenW - 2, "prompt fits the display");

// ---------------------------------------------------------------------------
// Page-major 1-bit primitives
// ---------------------------------------------------------------------------

inline void setPixel(uint8_t *buffer, int16_t x, int16_t y, bool color) {
    if (x < 0 || y < 0 || x >= kScreenW || y >= kScreenH)
        return;
    uint8_t &page = buffer[(static_cast<uint16_t>(y) >> 3) * kScreenW + static_cast<uint16_t>(x)];
    const uint8_t bit = static_cast<uint8_t>(1u << (y & 7));
    if (color)
        page |= bit;
    else
        page &= static_cast<uint8_t>(~bit);
}

inline void hline(uint8_t *buffer, int16_t x, int16_t y, int16_t width, bool color) {
    for (int16_t i = 0; i < width; ++i)
        setPixel(buffer, static_cast<int16_t>(x + i), y, color);
}

inline void vline(uint8_t *buffer, int16_t x, int16_t y, int16_t height, bool color) {
    for (int16_t i = 0; i < height; ++i)
        setPixel(buffer, x, static_cast<int16_t>(y + i), color);
}

inline void fillRect(uint8_t *buffer, int16_t x, int16_t y, int16_t width, int16_t height, bool color) {
    for (int16_t dy = 0; dy < height; ++dy)
        hline(buffer, x, static_cast<int16_t>(y + dy), width, color);
}

inline void outlineRect(uint8_t *buffer, int16_t x, int16_t y, int16_t width, int16_t height, bool color) {
    hline(buffer, x, y, width, color);
    hline(buffer, x, static_cast<int16_t>(y + height - 1), width, color);
    vline(buffer, x, y, height, color);
    vline(buffer, static_cast<int16_t>(x + width - 1), y, height, color);
}

// One pixel per cell is the readable minimum for a 4x4 shape inside an 8x8
// tray slot.
inline void drawTrayShape(uint8_t *buffer, int16_t x, int16_t y, uint16_t mask) {
    const cargo::MaskBounds bounds = cargo::maskBounds(mask);
    for (uint8_t sy = 0; sy < bounds.height; ++sy) {
        for (uint8_t sx = 0; sx < bounds.width; ++sx) {
            if ((mask & (uint16_t(1) << ((sy + bounds.minY) * 4 + (sx + bounds.minX)))) == 0)
                continue;
            setPixel(buffer, static_cast<int16_t>(x + sx * 2), static_cast<int16_t>(y + sy * 2), true);
        }
    }
}

// ---------------------------------------------------------------------------
// Cutaway layers
// ---------------------------------------------------------------------------

constexpr uint8_t kCornerInset[kCorner] = {10, 5, 3, 2, 2, 1, 1, 1, 0, 0};

inline void drawHull(uint8_t *buffer) {
    // Stepped capsule outline.
    for (uint8_t i = 0; i < kCorner; ++i) {
        const uint8_t inset = kCornerInset[i];
        const int16_t topY = static_cast<int16_t>(kHullY + i);
        const int16_t bottomY = static_cast<int16_t>(kHullY + kHullH - 1 - i);
        setPixel(buffer, static_cast<int16_t>(kHullX + inset), topY, true);
        setPixel(buffer, static_cast<int16_t>(kHullX + kHullW - 1 - inset), topY, true);
        setPixel(buffer, static_cast<int16_t>(kHullX + inset), bottomY, true);
        setPixel(buffer, static_cast<int16_t>(kHullX + kHullW - 1 - inset), bottomY, true);
    }
    hline(buffer, static_cast<int16_t>(kHullX + kCornerInset[0]), kHullY, static_cast<int16_t>(kHullW - 2 * kCornerInset[0]), true);
    hline(buffer, static_cast<int16_t>(kHullX + kCornerInset[0]), static_cast<int16_t>(kHullY + kHullH - 1),
          static_cast<int16_t>(kHullW - 2 * kCornerInset[0]), true);
    for (uint8_t i = kCorner; i < static_cast<uint8_t>(kHullH - kCorner); ++i) {
        setPixel(buffer, kHullX, static_cast<int16_t>(kHullY + i), true);
        setPixel(buffer, static_cast<int16_t>(kHullX + kHullW - 1), static_cast<int16_t>(kHullY + i), true);
    }

    // Deck line, open over the cargo hatch. A second row marks the deckhouse
    // sill without touching the hold boundary.
    hline(buffer, static_cast<int16_t>(kHullX + kCorner), kDeckY, static_cast<int16_t>(kBulkLX - (kHullX + kCorner)), true);
    hline(buffer, static_cast<int16_t>(kBulkRX + 1), kDeckY,
          static_cast<int16_t>(kHullX + kHullW - 1 - kCorner - (kBulkRX + 1)), true);
    hline(buffer, static_cast<int16_t>(kHullX + kCorner), static_cast<int16_t>(kDeckY + 1), static_cast<int16_t>(kBulkLX - (kHullX + kCorner)), true);
    hline(buffer, static_cast<int16_t>(kBulkRX + 1), static_cast<int16_t>(kDeckY + 1),
          static_cast<int16_t>(kHullX + kHullW - 1 - kCorner - (kBulkRX + 1)), true);

    // Hold bulkheads down to the ballast deck.
    vline(buffer, kBulkLX, static_cast<int16_t>(kDeckY + 1), static_cast<int16_t>(kHoldBottom - kDeckY), true);
    vline(buffer, kBulkRX, static_cast<int16_t>(kDeckY + 1), static_cast<int16_t>(kHoldBottom - kDeckY), true);

    // Forward bow cabin: outline, two window rows, and a hatch mark.
    outlineRect(buffer, kCabinX, kCabinY, kCabinW, kCabinH, true);
    hline(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 2), static_cast<int16_t>(kCabinW - 4), true);
    hline(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 4), static_cast<int16_t>(kCabinW - 4), true);
    setPixel(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 5), true);

    // Aft engine room: outline, three drive fins, and the shaft to the stern.
    outlineRect(buffer, kEngineX, kEngineY, kEngineW, kEngineH, true);
    for (uint8_t x = static_cast<uint8_t>(kEngineX + 2); x < static_cast<uint8_t>(kEngineX + kEngineW - 1); x = static_cast<uint8_t>(x + 3))
        vline(buffer, x, static_cast<int16_t>(kEngineY + 2), 5, true);
    hline(buffer, static_cast<int16_t>(kEngineX + kEngineW), static_cast<int16_t>(kEngineY + 7),
          static_cast<int16_t>(kHullX + kHullW - kCorner - (kEngineX + kEngineW)), true);

    // Ballast tanks under the hold, still inside the hull.
    constexpr uint8_t kTankY = static_cast<uint8_t>(kHoldBottom + 2);
    constexpr uint8_t kTankW = 24;
    outlineRect(buffer, kHoldX, kTankY, kTankW, 3, true);
    outlineRect(buffer, static_cast<int16_t>(kHoldRight - kTankW), kTankY, kTankW, 3, true);
    hline(buffer, static_cast<int16_t>(kHoldX + 2), static_cast<int16_t>(kTankY + 1), static_cast<int16_t>(kTankW - 4), true);
    hline(buffer, static_cast<int16_t>(kHoldRight - kTankW + 2), static_cast<int16_t>(kTankY + 1), static_cast<int16_t>(kTankW - 4), true);

    // Hatch posts over the split bulkhead.
    vline(buffer, kHatchLX, kHatchY, static_cast<int16_t>(kHatchBottom - kHatchY + 1), true);
    vline(buffer, kHatchRX, kHatchY, static_cast<int16_t>(kHatchBottom - kHatchY + 1), true);
    hline(buffer, kHatchLX, static_cast<int16_t>(kHatchY + 2), static_cast<int16_t>(kHatchRX - kHatchLX + 1), true);
}

inline bool blockedCell(uint8_t x, uint8_t y) {
    return (cargo::HAULER_BLOCKED & cargo::cellBit(x, y, CARGO_HAULER_COLS)) != 0;
}

inline void drawHoldBoundary(uint8_t *buffer) {
    for (uint8_t row = 0; row < CARGO_HAULER_ROWS; ++row) {
        for (uint8_t col = 0; col < CARGO_HAULER_COLS; ++col) {
            if (blockedCell(col, row))
                continue;
            const int16_t x = static_cast<int16_t>(kHoldX + col * kCell);
            const int16_t y = static_cast<int16_t>(kHoldY + row * kCell);
            if (row == 0 || blockedCell(col, static_cast<uint8_t>(row - 1)))
                hline(buffer, x, y, kCell, true);
            if (row + 1 == CARGO_HAULER_ROWS || blockedCell(col, static_cast<uint8_t>(row + 1)))
                hline(buffer, x, static_cast<int16_t>(y + kCell - 1), kCell, true);
            if (col == 0 || blockedCell(static_cast<uint8_t>(col - 1), row))
                vline(buffer, x, y, kCell, true);
            if (col + 1 == CARGO_HAULER_COLS || blockedCell(static_cast<uint8_t>(col + 1), row))
                vline(buffer, static_cast<int16_t>(x + kCell - 1), y, kCell, true);
        }
    }
    // Floor line under the full hold.
    hline(buffer, kHoldX, kHoldBottom, kHoldW, true);
}

inline void drawBlocked(uint8_t *buffer) {
    for (uint8_t row = 0; row < CARGO_HAULER_ROWS; ++row) {
        for (uint8_t col = 0; col < CARGO_HAULER_COLS; ++col) {
            const int16_t x = static_cast<int16_t>(kHoldX + col * kCell);
            const int16_t y = static_cast<int16_t>(kHoldY + row * kCell);
            if (blockedCell(col, row)) {
                bool fullColumn = true;
                for (uint8_t r = 0; r < CARGO_HAULER_ROWS; ++r) {
                    if (!blockedCell(col, r))
                        fullColumn = false;
                }
                if (fullColumn) {
                    // Structural split bulkhead: a double vertical line.
                    vline(buffer, static_cast<int16_t>(x + 2), y, kCell, true);
                    vline(buffer, static_cast<int16_t>(x + 3), y, kCell, true);
                } else {
                    // Notch block: inset frame with a cross.
                    outlineRect(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), 4, 4, true);
                    setPixel(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), true);
                    setPixel(buffer, static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 3), true);
                    setPixel(buffer, static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 2), true);
                    setPixel(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 3), true);
                }
                continue;
            }
            // Open cell hint dot.
            setPixel(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), true);
        }
    }
}

// Clip a crate cell to the hold rectangle: cargo never appears above or
// outside the hull even if a future caller passes a loose position.
inline bool insideHold(uint8_t col, uint8_t row) {
    return col < CARGO_HAULER_COLS && row < CARGO_HAULER_ROWS;
}

inline void drawInstalled(uint8_t *buffer, const cargo::CargoPuzzle &puzzle, uint8_t index, bool selected) {
    const cargo::CargoPiece &piece = puzzle.pieces[index];
    const uint16_t mask = cargo::pieceMask(piece);
    const bool held = puzzle.held == static_cast<int8_t>(index);

    for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) {
            if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                continue;
            const uint8_t col = static_cast<uint8_t>(piece.x + sx);
            const uint8_t row = static_cast<uint8_t>(piece.y + sy);
            if (!insideHold(col, row))
                continue;

            const int16_t x = static_cast<int16_t>(kHoldX + col * kCell);
            const int16_t y = static_cast<int16_t>(kHoldY + row * kCell);
            if (held) {
                // Held cue: hollow outline plus a centre dot; not a solid load.
                if (sy == 0 || (mask & (uint16_t(1) << ((sy - 1) * 4 + sx))) == 0)
                    hline(buffer, x, y, kCell, true);
                if (sy == 3 || (mask & (uint16_t(1) << ((sy + 1) * 4 + sx))) == 0)
                    hline(buffer, x, static_cast<int16_t>(y + kCell - 1), kCell, true);
                if (sx == 0 || (mask & (uint16_t(1) << (sy * 4 + (sx - 1)))) == 0)
                    vline(buffer, x, y, kCell, true);
                if (sx == 3 || (mask & (uint16_t(1) << (sy * 4 + (sx + 1)))) == 0)
                    vline(buffer, static_cast<int16_t>(x + kCell - 1), y, kCell, true);
                setPixel(buffer, static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 2), true);
                continue;
            }

            fillRect(buffer, x, y, kCell, kCell, true);
            // Black outer edges make neighbouring crate cells read as one shape.
            if (sy == 0 || (mask & (uint16_t(1) << ((sy - 1) * 4 + sx))) == 0)
                hline(buffer, x, y, kCell, false);
            if (sy == 3 || (mask & (uint16_t(1) << ((sy + 1) * 4 + sx))) == 0)
                hline(buffer, x, static_cast<int16_t>(y + kCell - 1), kCell, false);
            if (sx == 0 || (mask & (uint16_t(1) << (sy * 4 + (sx - 1)))) == 0)
                vline(buffer, x, y, kCell, false);
            if (sx == 3 || (mask & (uint16_t(1) << (sy * 4 + (sx + 1)))) == 0)
                vline(buffer, static_cast<int16_t>(x + kCell - 1), y, kCell, false);

            // Installed selection: a black inset frame keeps the crate solid
            // while making the active piece easy to find in a packed hold.
            if (selected)
                outlineRect(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                            static_cast<int16_t>(kCell - 2), static_cast<int16_t>(kCell - 2), false);
        }
    }
}

inline void drawTray(uint8_t *buffer, const cargo::CargoPuzzle &puzzle, uint8_t selected) {
    uint8_t slot = 0;
    for (uint8_t i = 0; i < puzzle.pieceCount && slot < kTraySlots; ++i) {
        if (cargo::piecePlaced(puzzle.pieces[i]))
            continue;
        const int16_t x = static_cast<int16_t>(kTrayX + slot * kTraySlot);
        if (i == selected)
            hline(buffer, x, static_cast<int16_t>(kTrayY + kTraySlot), kTraySlot, true);

        const uint16_t mask = cargo::pieceMask(puzzle.pieces[i]);
        const cargo::MaskBounds bounds = cargo::maskBounds(mask);
        const uint8_t width = static_cast<uint8_t>(bounds.width * 2 - 1);
        const uint8_t height = static_cast<uint8_t>(bounds.height * 2 - 1);
        const int16_t ix = static_cast<int16_t>(x + (kTraySlot - width) / 2);
        const int16_t iy = static_cast<int16_t>(kTrayY + (kTraySlot - height) / 2);
        drawTrayShape(buffer, ix, iy, mask);
        if (cargo::pieceBonus(puzzle.pieces[i]))
            setPixel(buffer, static_cast<int16_t>(x + kTraySlot - 1), kTrayY, true);
        ++slot;
    }
}

// Compose the cutaway into a 1024-byte page-major display buffer. Text is a
// separate device pass so this frame stays pure and testable.
inline void drawFrame(const cargo::CargoPuzzle &puzzle, uint8_t selected, uint8_t *buffer) {
    for (uint16_t i = 0; i < kBufferBytes; ++i)
        buffer[i] = 0;

    drawHull(buffer);
    drawHoldBoundary(buffer);
    drawBlocked(buffer);

    for (uint8_t i = 0; i < puzzle.pieceCount; ++i) {
        if (cargo::piecePlaced(puzzle.pieces[i]) || puzzle.held == static_cast<int8_t>(i))
            drawInstalled(buffer, puzzle, i, i == selected);
    }
    drawTray(buffer, puzzle, selected);
}

#if defined(__AVR__)

#include "common.hpp"

inline void printReqBonus(const cargo::CargoPuzzle &puzzle) {
    arduboy.setCursor(kStatusX, kStatusY);
    arduboy.print(F("R "));
    arduboy.print(puzzle.requiredPlaced);
    arduboy.print('/');
    arduboy.print(puzzle.requiredTotal);
    arduboy.print(F(" B "));
    arduboy.print(puzzle.bonusPlaced);
    arduboy.print('/');
    arduboy.print(puzzle.bonusTotal);
}

inline void printLoad(const cargo::CargoPuzzle &puzzle) {
    arduboy.setCursor(kLoadX, kLoadY);
    arduboy.print(F("LOAD "));
    arduboy.print(puzzle.loadedCells);
    arduboy.print('/');
    arduboy.print(puzzle.load);
}

inline void printMessage(cargo_screen::Message message) {
    arduboy.setCursor(kMessageX, kMessageY);
    switch (message) {
    case cargo_screen::Message::Picked:
        arduboy.print(F("PICKED"));
        break;
    case cargo_screen::Message::Lifted:
        arduboy.print(F("LIFTED"));
        break;
    case cargo_screen::Message::Released:
        arduboy.print(F("RELEASED"));
        break;
    case cargo_screen::Message::Rotated:
        arduboy.print(F("ROTATED"));
        break;
    case cargo_screen::Message::Undone:
        arduboy.print(F("UNDONE"));
        break;
    case cargo_screen::Message::NeedsSupport:
        arduboy.print(F("NEEDS SUPPORT"));
        break;
    case cargo_screen::Message::Blocked:
        arduboy.print(F("BLOCKED"));
        break;
    case cargo_screen::Message::LiftBlocked:
        arduboy.print(F("LIFT BLOCKED"));
        break;
    case cargo_screen::Message::NoRoom:
        arduboy.print(F("NO ROOM"));
        break;
    case cargo_screen::Message::NothingToUndo:
        arduboy.print(F("NO UNDO"));
        break;
    case cargo_screen::Message::NoCrate:
        arduboy.print(F("NO CRATE"));
        break;
    case cargo_screen::Message::Complete:
        arduboy.print(F("LOAD READY"));
        break;
    case cargo_screen::Message::Incomplete:
        arduboy.print(F("LOAD SHORT"));
        break;
    case cargo_screen::Message::None:
    default:
        arduboy.print(F("A LIFT B ROT"));
        break;
    }
}

inline void draw(const cargo::CargoPuzzle &puzzle, uint8_t selected, cargo_screen::Message message) {
    drawFrame(puzzle, selected, Arduboy2Base::sBuffer);

    arduboy.setTextColor(WHITE);
    arduboy.setTextBackground(BLACK);
    arduboy.setCursor(kNameX, kNameY);
    arduboy.print(F("HAULER"));
    printReqBonus(puzzle);
    printLoad(puzzle);

    // The commit prompt is shown only when the required packing is complete
    // and nothing is held. Otherwise the normal feedback line stays visible.
    const bool commitReady = puzzle.held < 0 && cargo::requiredComplete(puzzle);
    if (commitReady) {
        arduboy.setCursor(kCommitPromptX, kCommitPromptY);
        arduboy.print(F("A+B LOAD"));
    } else {
        printMessage(message);
    }

    arduboy.setCursor(kPromptX, kPromptY);
    arduboy.print(F("HOLD B"));
}

#endif   // __AVR__

}   // namespace cargo_view

#endif   // SH_FEATURE_CARGO_LOAD
