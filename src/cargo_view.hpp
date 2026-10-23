#pragma once

#include <stdint.h>

#include "cargo.hpp"
#include "cargo_screen.hpp"
#include "features.hpp"

#if SH_FEATURE_CARGO_LOAD

// Cargo cutaway view: pure 128x64 1-bit composition plus the device text pass.
// No Arduino headers here, so host tests draw into a plain page-major buffer
// and inspect exact pixels. One generic pressure-hull cutaway serves every
// class: the hold dimensions and cell size are the class distinction. The
// hold is centred, blocked cells come from the loaded puzzle's open-cell mask,
// and installed/tray art scales with the cell. Interaction is owned by the M5
// rules bead.
namespace cargo_view {

constexpr uint8_t kScreenW = 128;
constexpr uint8_t kScreenH = 64;
constexpr uint16_t kBufferBytes = kScreenW * kScreenH / 8;

// Pressure hull. The outline is a stepped capsule; the interior stays black so
// the hold and machinery read as a cutaway. The shell is class-independent.
constexpr uint8_t kHullX = 4;
constexpr uint8_t kHullY = 10;
constexpr uint8_t kHullW = 120;
constexpr uint8_t kHullH = 33;
constexpr uint8_t kCorner = 10;

// The hold is centred on this row. MINNOW (3x7=21), HAULER (3x6=18), and
// LEVIATHAN (4x5=20) all centre here and stay inside the hull.
constexpr uint8_t kHoldCenterX = 64;
constexpr uint8_t kHoldCenterY = 28;
constexpr uint8_t kHoldMaxH = 21;   // MINNOW 3 rows at 7 px

// Dynamic hold geometry. Cell scale is fixed per class so the three holds read
// as different craft without per-class art: MINNOW 7 px, HAULER 6 px,
// LEVIATHAN 5 px.
struct Layout {
    uint8_t cell;
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
};

#if SH_FEATURE_MULTI_HULL
constexpr uint8_t cellFor(uint8_t cols) {
    return cols == CARGO_MINNOW_COLS ? 7 : cols == CARGO_LEVIATHAN_COLS ? 5 : 6;
}
#else
// Single-hull builds fold the scale and hold size to HAULER immediates.
constexpr uint8_t cellFor(uint8_t) {
    return 6;
}
#endif

// Single return statement: the AVR toolchain still compiles this header as
// gnu++11, where a constexpr body may not declare locals.
constexpr Layout layoutFor(uint8_t cols, uint8_t rows) {
    return Layout{cellFor(cols),
                  static_cast<uint8_t>(kHoldCenterX - (cols * cellFor(cols)) / 2),
                  static_cast<uint8_t>(kHoldCenterY - (rows * cellFor(cols)) / 2),
                  static_cast<uint8_t>(cols * cellFor(cols)),
                  static_cast<uint8_t>(rows * cellFor(cols))};
}

inline Layout layoutOf(const cargo::CargoPuzzle &puzzle) {
    return layoutFor(puzzle.cols, puzzle.rows);
}

// Structural shell features derive from the hold rectangle, so the same art
// works at every scale.
constexpr uint8_t kDeckY = 16;
constexpr uint8_t kCabinX = 10;
constexpr uint8_t kCabinY = 21;
constexpr uint8_t kCabinW = 16;
constexpr uint8_t kCabinH = 8;
constexpr uint8_t kEngineX = 102;
constexpr uint8_t kEngineY = 21;
constexpr uint8_t kEngineW = 16;
constexpr uint8_t kEngineH = 9;
constexpr uint8_t kHatchLX = 54;
constexpr uint8_t kHatchRX = 74;

// Text slots. Arduboy 5x7 glyphs advance 6 px; every slot is checked against
// the display edges and its neighbours with static_assert below.
constexpr uint8_t kNameX = 2;
constexpr uint8_t kNameY = 1;
#if SH_FEATURE_MULTI_HULL
constexpr uint8_t kNameChars = 9; // "LEVIATHAN"
#else
constexpr uint8_t kNameChars = 6; // "HAULER"
#endif
constexpr uint8_t kStatusX = 60;
constexpr uint8_t kStatusY = 1;
constexpr uint8_t kStatusChars = 11; // "R 0/6 B 0/2"
constexpr uint8_t kLoadX = 2;
constexpr uint8_t kLoadY = 49;
constexpr uint8_t kLoadChars = 10; // "LOAD 40/40"
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

constexpr Layout kHaulerHold = layoutFor(CARGO_HAULER_COLS, CARGO_HAULER_ROWS);
#if SH_FEATURE_MULTI_HULL
constexpr Layout kMinnowHold = layoutFor(CARGO_MINNOW_COLS, CARGO_MINNOW_ROWS);
constexpr Layout kLeviathanHold = layoutFor(CARGO_LEVIATHAN_COLS, CARGO_LEVIATHAN_ROWS);
#endif

static_assert(kHullX + kHullW <= kScreenW, "hull fits the display");
static_assert(kHullY > kNameY + 7, "hull clears the header text");
static_assert(kHaulerHold.cell == 6, "HAULER hold is 6 px cells");
static_assert(kHaulerHold.w == 11 * 6, "HAULER hold is 11 cells wide");
static_assert(kHaulerHold.h == 3 * 6, "HAULER hold is 3 cells tall");
static_assert(kHaulerHold.x + kHaulerHold.w / 2 == kHoldCenterX, "HAULER hold is centred");
static_assert(kHaulerHold.x > kHullX + kCorner, "HAULER hold clears the bow shell");
static_assert(kHaulerHold.x + kHaulerHold.w + kCorner < kHullX + kHullW, "HAULER hold clears the stern shell");
#if SH_FEATURE_MULTI_HULL
static_assert(kMinnowHold.cell == 7, "MINNOW hold is 7 px cells");
static_assert(kLeviathanHold.cell == 5, "LEVIATHAN hold is 5 px cells");
static_assert(kMinnowHold.w == 6 * 7, "MINNOW hold is 6 cells wide");
static_assert(kLeviathanHold.w == 14 * 5, "LEVIATHAN hold is 14 cells wide");
static_assert(kMinnowHold.h == 3 * 7, "MINNOW hold is 3 cells tall");
static_assert(kLeviathanHold.h == 4 * 5, "LEVIATHAN hold is 4 cells tall");
static_assert(kMinnowHold.x + kMinnowHold.w / 2 == kHoldCenterX, "MINNOW hold is centred");
static_assert(kLeviathanHold.x + kLeviathanHold.w / 2 == kHoldCenterX, "LEVIATHAN hold is centred");
static_assert(kMinnowHold.x > kHullX + kCorner, "MINNOW hold clears the bow shell");
static_assert(kLeviathanHold.x + kLeviathanHold.w + kCorner < kHullX + kHullW, "LEVIATHAN hold clears the stern shell");
static_assert(kCabinX + kCabinW <= kLeviathanHold.x - 3, "cabin sits forward of the widest hold");
static_assert(kEngineX > kLeviathanHold.x + kLeviathanHold.w + 2, "engine room sits aft of the widest hold");
#endif
static_assert(kHoldCenterY - kHoldMaxH / 2 > kHullY, "tallest hold sits below the deck");
static_assert(kHoldCenterY + kHoldMaxH / 2 + 4 <= kHullY + kHullH, "hold and ballast stay inside the hull");
static_assert(kEngineX + kEngineW < kHullX + kHullW - 1, "engine room stays inside the hull");
static_assert(kHoldCenterY + kHoldMaxH / 2 < kTrayY, "tray clears the hull");
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

inline void drawHull(uint8_t *buffer, const Layout &hold) {
    const uint8_t holdRight = static_cast<uint8_t>(hold.x + hold.w);
    const uint8_t holdBottom = static_cast<uint8_t>(hold.y + hold.h);
    const uint8_t bulkLX = static_cast<uint8_t>(hold.x - 3);
    const uint8_t bulkRX = static_cast<uint8_t>(holdRight + 2);

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
    hline(buffer, static_cast<int16_t>(kHullX + kCornerInset[0]), kHullY,
          static_cast<int16_t>(kHullW - 2 * kCornerInset[0]), true);
    hline(buffer, static_cast<int16_t>(kHullX + kCornerInset[0]), static_cast<int16_t>(kHullY + kHullH - 1),
          static_cast<int16_t>(kHullW - 2 * kCornerInset[0]), true);
    for (uint8_t i = kCorner; i < static_cast<uint8_t>(kHullH - kCorner); ++i) {
        setPixel(buffer, kHullX, static_cast<int16_t>(kHullY + i), true);
        setPixel(buffer, static_cast<int16_t>(kHullX + kHullW - 1), static_cast<int16_t>(kHullY + i), true);
    }

    // Deck line, open over the cargo hatch. A second row marks the deckhouse
    // sill without touching the hold boundary.
    hline(buffer, static_cast<int16_t>(kHullX + kCorner), kDeckY,
          static_cast<int16_t>(bulkLX - (kHullX + kCorner)), true);
    hline(buffer, static_cast<int16_t>(bulkRX + 1), kDeckY,
          static_cast<int16_t>(kHullX + kHullW - 1 - kCorner - (bulkRX + 1)), true);
    hline(buffer, static_cast<int16_t>(kHullX + kCorner), static_cast<int16_t>(kDeckY + 1),
          static_cast<int16_t>(bulkLX - (kHullX + kCorner)), true);
    hline(buffer, static_cast<int16_t>(bulkRX + 1), static_cast<int16_t>(kDeckY + 1),
          static_cast<int16_t>(kHullX + kHullW - 1 - kCorner - (bulkRX + 1)), true);

    // Hold bulkheads down to the ballast deck.
    vline(buffer, bulkLX, static_cast<int16_t>(kDeckY + 1), static_cast<int16_t>(holdBottom - kDeckY), true);
    vline(buffer, bulkRX, static_cast<int16_t>(kDeckY + 1), static_cast<int16_t>(holdBottom - kDeckY), true);

    // Forward bow cabin: outline, two window rows, and a hatch mark.
    outlineRect(buffer, kCabinX, kCabinY, kCabinW, kCabinH, true);
    hline(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 2),
          static_cast<int16_t>(kCabinW - 4), true);
    hline(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 4),
          static_cast<int16_t>(kCabinW - 4), true);
    setPixel(buffer, static_cast<int16_t>(kCabinX + 2), static_cast<int16_t>(kCabinY + 5), true);

    // Aft engine room: outline, three drive fins, and the shaft to the stern.
    outlineRect(buffer, kEngineX, kEngineY, kEngineW, kEngineH, true);
    for (uint8_t x = static_cast<uint8_t>(kEngineX + 2); x < static_cast<uint8_t>(kEngineX + kEngineW - 1);
         x = static_cast<uint8_t>(x + 3))
        vline(buffer, x, static_cast<int16_t>(kEngineY + 2), 5, true);
    hline(buffer, static_cast<int16_t>(kEngineX + kEngineW), static_cast<int16_t>(kEngineY + 7),
          static_cast<int16_t>(kHullX + kHullW - kCorner - (kEngineX + kEngineW)), true);

    // Ballast tanks under the hold, still inside the hull. Two equal tanks
    // split at the hold centre so the same art fits 42, 66, and 70 px holds.
    const uint8_t tankY = static_cast<uint8_t>(holdBottom + 2);
    const uint8_t tankW = static_cast<uint8_t>(hold.w / 2);
    outlineRect(buffer, hold.x, tankY, tankW, 2, true);
    outlineRect(buffer, static_cast<int16_t>(hold.x + hold.w - tankW), tankY, tankW, 2, true);

    // Hatch posts over the split bulkhead, stopped one row above the hold.
    const uint8_t hatchBottom = static_cast<uint8_t>(hold.y - 1);
    const uint8_t hatchH = static_cast<uint8_t>(hatchBottom - kHullY + 1);
    vline(buffer, kHatchLX, kHullY, hatchH, true);
    vline(buffer, kHatchRX, kHullY, hatchH, true);
    hline(buffer, kHatchLX, static_cast<int16_t>(kHullY + 2), static_cast<int16_t>(kHatchRX - kHatchLX + 1), true);
}

// Blocked cells are the cleared bits of the loaded open-cell mask; the loader
// already rejects records that touch them.
inline bool blockedCell(const cargo::CargoPuzzle &puzzle, uint8_t col, uint8_t row) {
    if (col >= puzzle.cols || row >= puzzle.rows)
        return false;
    return (puzzle.hold & cargo::cellBit(col, row, puzzle.cols)) == 0;
}

inline void drawHoldBoundary(uint8_t *buffer, const cargo::CargoPuzzle &puzzle, const Layout &hold) {
    for (uint8_t row = 0; row < puzzle.rows; ++row) {
        for (uint8_t col = 0; col < puzzle.cols; ++col) {
            if (blockedCell(puzzle, col, row))
                continue;
            const int16_t x = static_cast<int16_t>(hold.x + col * hold.cell);
            const int16_t y = static_cast<int16_t>(hold.y + row * hold.cell);
            if (row == 0 || blockedCell(puzzle, col, static_cast<uint8_t>(row - 1)))
                hline(buffer, x, y, hold.cell, true);
            if (row + 1 == puzzle.rows || blockedCell(puzzle, col, static_cast<uint8_t>(row + 1)))
                hline(buffer, x, static_cast<int16_t>(y + hold.cell - 1), hold.cell, true);
            if (col == 0 || blockedCell(puzzle, static_cast<uint8_t>(col - 1), row))
                vline(buffer, x, y, hold.cell, true);
            if (col + 1 == puzzle.cols || blockedCell(puzzle, static_cast<uint8_t>(col + 1), row))
                vline(buffer, static_cast<int16_t>(x + hold.cell - 1), y, hold.cell, true);
        }
    }
    // Floor line under the full hold.
    hline(buffer, hold.x, static_cast<int16_t>(hold.y + hold.h), hold.w, true);
}

inline void drawBlocked(uint8_t *buffer, const cargo::CargoPuzzle &puzzle, const Layout &hold) {
    for (uint8_t row = 0; row < puzzle.rows; ++row) {
        for (uint8_t col = 0; col < puzzle.cols; ++col) {
            const int16_t x = static_cast<int16_t>(hold.x + col * hold.cell);
            const int16_t y = static_cast<int16_t>(hold.y + row * hold.cell);
            if (!blockedCell(puzzle, col, row)) {
                // Open cell hint dot.
                setPixel(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), true);
                continue;
            }

            bool fullColumn = true;
            for (uint8_t r = 0; r < puzzle.rows; ++r) {
                if (!blockedCell(puzzle, col, r))
                    fullColumn = false;
            }
            if (fullColumn) {
                // Structural split bulkhead: a double vertical line.
                const uint8_t mid = static_cast<uint8_t>(hold.cell / 2);
                vline(buffer, static_cast<int16_t>(x + mid - 1), y, hold.cell, true);
                vline(buffer, static_cast<int16_t>(x + mid), y, hold.cell, true);
            } else {
                // Notch/blocked cell: a small X hatch across the cell inset.
                const uint8_t size = static_cast<uint8_t>(hold.cell - 2);
                for (uint8_t i = 0; i < size; ++i) {
                    setPixel(buffer, static_cast<int16_t>(x + 1 + i), static_cast<int16_t>(y + 1 + i), true);
                    setPixel(buffer, static_cast<int16_t>(x + size - i), static_cast<int16_t>(y + 1 + i), true);
                }
            }
        }
    }
}

// Clip a crate cell to the loaded hold rectangle: cargo never appears above or
// outside the hull even if a future caller passes a loose position.
inline bool insideHold(const cargo::CargoPuzzle &puzzle, uint8_t col, uint8_t row) {
    return col < puzzle.cols && row < puzzle.rows;
}

inline void drawInstalled(uint8_t *buffer, const cargo::CargoPuzzle &puzzle, const Layout &hold, uint8_t index,
                          bool selected) {
    const cargo::CargoPiece &piece = puzzle.pieces[index];
    const uint16_t mask = cargo::pieceMask(piece);
    const bool held = puzzle.held == static_cast<int8_t>(index);

    for (uint8_t sy = 0; sy < 4; ++sy) {
        for (uint8_t sx = 0; sx < 4; ++sx) {
            if ((mask & (uint16_t(1) << (sy * 4 + sx))) == 0)
                continue;
            const uint8_t col = static_cast<uint8_t>(piece.x + sx);
            const uint8_t row = static_cast<uint8_t>(piece.y + sy);
            if (!insideHold(puzzle, col, row))
                continue;

            const int16_t x = static_cast<int16_t>(hold.x + col * hold.cell);
            const int16_t y = static_cast<int16_t>(hold.y + row * hold.cell);
            if (held) {
                // Held cue: hollow outline plus a centre dot; not a solid load.
                if (sy == 0 || (mask & (uint16_t(1) << ((sy - 1) * 4 + sx))) == 0)
                    hline(buffer, x, y, hold.cell, true);
                if (sy == 3 || (mask & (uint16_t(1) << ((sy + 1) * 4 + sx))) == 0)
                    hline(buffer, x, static_cast<int16_t>(y + hold.cell - 1), hold.cell, true);
                if (sx == 0 || (mask & (uint16_t(1) << (sy * 4 + (sx - 1)))) == 0)
                    vline(buffer, x, y, hold.cell, true);
                if (sx == 3 || (mask & (uint16_t(1) << (sy * 4 + (sx + 1)))) == 0)
                    vline(buffer, static_cast<int16_t>(x + hold.cell - 1), y, hold.cell, true);
                setPixel(buffer, static_cast<int16_t>(x + hold.cell / 2), static_cast<int16_t>(y + hold.cell / 2), true);
                continue;
            }

            fillRect(buffer, x, y, hold.cell, hold.cell, true);
            // Black outer edges make neighbouring crate cells read as one shape.
            if (sy == 0 || (mask & (uint16_t(1) << ((sy - 1) * 4 + sx))) == 0)
                hline(buffer, x, y, hold.cell, false);
            if (sy == 3 || (mask & (uint16_t(1) << ((sy + 1) * 4 + sx))) == 0)
                hline(buffer, x, static_cast<int16_t>(y + hold.cell - 1), hold.cell, false);
            if (sx == 0 || (mask & (uint16_t(1) << (sy * 4 + (sx - 1)))) == 0)
                vline(buffer, x, y, hold.cell, false);
            if (sx == 3 || (mask & (uint16_t(1) << (sy * 4 + (sx + 1)))) == 0)
                vline(buffer, static_cast<int16_t>(x + hold.cell - 1), y, hold.cell, false);

            // Installed selection: two black corner notches keep the crate
            // solid while marking the active piece at any cell scale.
            if (selected) {
                setPixel(buffer, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), false);
                setPixel(buffer, static_cast<int16_t>(x + hold.cell - 2), static_cast<int16_t>(y + hold.cell - 2), false);
            }
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

    const Layout hold = layoutOf(puzzle);
    drawHull(buffer, hold);
    drawHoldBoundary(buffer, puzzle, hold);
    drawBlocked(buffer, puzzle, hold);

    for (uint8_t i = 0; i < puzzle.pieceCount; ++i) {
        if (cargo::piecePlaced(puzzle.pieces[i]) || puzzle.held == static_cast<int8_t>(i))
            drawInstalled(buffer, puzzle, hold, i, i == selected);
    }
    drawTray(buffer, puzzle, selected);
}

// The loaded arrangement's dimensions are the class identity, so the view does
// not need a second state byte on the draw path.
inline uint8_t hullFor(const cargo::CargoPuzzle &puzzle) {
#if SH_FEATURE_MULTI_HULL
    if (puzzle.cols == CARGO_MINNOW_COLS)
        return cargo::HULL_MINNOW;
    if (puzzle.cols == CARGO_LEVIATHAN_COLS)
        return cargo::HULL_LEVIATHAN;
#else
    (void)puzzle;
#endif
    return cargo::HULL_HAULER;
}

#if !defined(__AVR__)
// Host-only names keep the pure tests readable. On AVR the device pass uses
// F() below, and MULTI_HULL=0 compiles out the two extra class names.
inline const char *nameFor(uint8_t hull) {
#if SH_FEATURE_MULTI_HULL
    if (hull == cargo::HULL_MINNOW)
        return "MINNOW";
    if (hull == cargo::HULL_LEVIATHAN)
        return "LEVIATHAN";
#endif
    return "HAULER";
}
#endif

#if defined(__AVR__)

#include "common.hpp"

#if SH_FEATURE_MULTI_HULL
inline const __FlashStringHelper *nameFor(uint8_t hull) {
    if (hull == cargo::HULL_MINNOW)
        return F("MINNOW");
    if (hull == cargo::HULL_LEVIATHAN)
        return F("LEVIATHAN");
    return F("HAULER");
}
#else
inline const __FlashStringHelper *nameFor(uint8_t) {
    return F("HAULER");
}
#endif

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
    arduboy.print(nameFor(hullFor(puzzle)));
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
