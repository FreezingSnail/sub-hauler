// Sub-hauler cargo packing slice: standalone 128x64 1-bit packing test.
(function () {
    'use strict';

    const canvas = document.getElementById('cargoGame');
    const ctx = canvas.getContext('2d');
    const debugEl = document.getElementById('cargoDebug');
    const subButtons = Array.from(
        document.querySelectorAll('[data-cargo-sub]')
    );
    const loadButton = document.getElementById('cargoLoad');
    const cancelButton = document.getElementById('cargoCancel');
    const titleEl = document.getElementById('cargoTitle');
    const subtitleEl = document.getElementById('cargoSubtitle');
    const PILOT_UNITS = 4;

    const W = 128;
    const H = 64;
    let CELL = 6;
    const SUBS = window.SubHaulerSubs.list;

    let SUB_INDEX = window.SubHaulerSubs.getSelected();
    let COLS = 10;
    let ROWS = 3;
    let HOLD_X = 24;
    let HOLD_Y = 18;
    let AVAILABLE = 30;
    let SOLUTION = [];

    const FONT = {
        ' ': [
            '00000', '00000', '00000', '00000',
            '00000', '00000', '00000',
        ],
        A: [
            '01110', '10001', '10001', '11111',
            '10001', '10001', '10001',
        ],
        B: [
            '11110', '10001', '10001', '11110',
            '10001', '10001', '11110',
        ],
        C: [
            '01111', '10000', '10000', '10000',
            '10000', '10000', '01111',
        ],
        D: [
            '11110', '10001', '10001', '10001',
            '10001', '10001', '11110',
        ],
        E: [
            '11111', '10000', '10000', '11110',
            '10000', '10000', '11111',
        ],
        F: [
            '11111', '10000', '10000', '11110',
            '10000', '10000', '10000',
        ],
        G: [
            '01111', '10000', '10000', '10111',
            '10001', '10001', '01111',
        ],
        H: [
            '10001', '10001', '10001', '11111',
            '10001', '10001', '10001',
        ],
        I: [
            '11111', '00100', '00100', '00100',
            '00100', '00100', '11111',
        ],
        J: [
            '00111', '00010', '00010', '00010',
            '00010', '10010', '01100',
        ],
        K: [
            '10001', '10010', '10100', '11000',
            '10100', '10010', '10001',
        ],
        L: [
            '10000', '10000', '10000', '10000',
            '10000', '10000', '11111',
        ],
        M: [
            '10001', '11011', '10101', '10101',
            '10001', '10001', '10001',
        ],
        N: [
            '10001', '11001', '10101', '10011',
            '10001', '10001', '10001',
        ],
        O: [
            '01110', '10001', '10001', '10001',
            '10001', '10001', '01110',
        ],
        P: [
            '11110', '10001', '10001', '11110',
            '10000', '10000', '10000',
        ],
        Q: [
            '01110', '10001', '10001', '10001',
            '10101', '10010', '01101',
        ],
        R: [
            '11110', '10001', '10001', '11110',
            '10100', '10010', '10001',
        ],
        S: [
            '01111', '10000', '10000', '01110',
            '00001', '00001', '11110',
        ],
        T: [
            '11111', '00100', '00100', '00100',
            '00100', '00100', '00100',
        ],
        U: [
            '10001', '10001', '10001', '10001',
            '10001', '10001', '01110',
        ],
        V: [
            '10001', '10001', '10001', '10001',
            '10001', '01010', '00100',
        ],
        W: [
            '10001', '10001', '10001', '10101',
            '10101', '10101', '01010',
        ],
        X: [
            '10001', '10001', '01010', '00100',
            '01010', '10001', '10001',
        ],
        Y: [
            '10001', '10001', '01010', '00100',
            '00100', '00100', '00100',
        ],
        Z: [
            '11111', '00001', '00010', '00100',
            '01000', '10000', '11111',
        ],
        0: [
            '01110', '10001', '10011', '10101',
            '11001', '10001', '01110',
        ],
        1: [
            '00100', '01100', '00100', '00100',
            '00100', '00100', '01110',
        ],
        2: [
            '01110', '10001', '00001', '00010',
            '00100', '01000', '11111',
        ],
        3: [
            '11110', '00001', '00001', '01110',
            '00001', '00001', '11110',
        ],
        4: [
            '00010', '00110', '01010', '10010',
            '11111', '00010', '00010',
        ],
        5: [
            '11111', '10000', '10000', '11110',
            '00001', '00001', '11110',
        ],
        6: [
            '01110', '10000', '10000', '11110',
            '10001', '10001', '01110',
        ],
        7: [
            '11111', '00001', '00010', '00100',
            '01000', '01000', '01000',
        ],
        8: [
            '01110', '10001', '10001', '01110',
            '10001', '10001', '01110',
        ],
        9: [
            '01110', '10001', '10001', '01111',
            '00001', '00001', '01110',
        ],
        '$': [
            '00100', '01111', '10100', '01110',
            '00101', '11110', '00100',
        ],
        '+': [
            '00000', '00100', '00100', '11111',
            '00100', '00100', '00000',
        ],
        '-': [
            '00000', '00000', '00000', '11111',
            '00000', '00000', '00000',
        ],
        '/': [
            '00001', '00010', '00010', '00100',
            '01000', '01000', '10000',
        ],
        '>': [
            '10000', '01000', '00100', '00010',
            '00100', '01000', '10000',
        ],
        '?': [
            '01110', '10001', '00001', '00010',
            '00100', '00000', '00100',
        ],
    };

    window.SubHaulerFont = FONT;

    function drawText(text, x, y, color) {
        const value = String(text).toUpperCase();
        let cursor = x;
        ctx.fillStyle = color === 0 ? '#000' : '#fff';
        for (let i = 0; i < value.length; i++) {
            const glyph = FONT[value[i]] || FONT['?'];
            for (let row = 0; row < 7; row++) {
                const bits = glyph[row];
                for (let col = 0; col < 5; col++) {
                    if (bits[col] === '1')
                        ctx.fillRect(cursor + col, y + row, 1, 1);
                }
            }
            cursor += 6;
        }
    }

    function textWidth(text) {
        return String(text).length * 6 - 1;
    }

    function drawTextRight(text, right, y, color) {
        drawText(text, right - textWidth(text), y, color);
    }

    function px(x, y) {
        ctx.fillRect(x, y, 1, 1);
    }

    function line(x0, y0, x1, y1) {
        let x = x0;
        let y = y0;
        const dx = Math.abs(x1 - x0);
        const sx = x0 < x1 ? 1 : -1;
        const dy = -Math.abs(y1 - y0);
        const sy = y0 < y1 ? 1 : -1;
        let err = dx + dy;
        for (;;) {
            px(x, y);
            if (x === x1 && y === y1)
                break;
            const e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y += sy;
            }
        }
    }

    function outline(x, y, w, h) {
        ctx.fillStyle = '#fff';
        ctx.fillRect(x, y, w, 1);
        ctx.fillRect(x, y + h - 1, w, 1);
        ctx.fillRect(x, y, 1, h);
        ctx.fillRect(x + w - 1, y, 1, h);
    }

    function hatchBox(x, y, w, h) {
        outline(x, y, w, h);
        for (let i = 2; i < w + h - 2; i += 3) {
            const sx = x + Math.min(i, w - 1);
            const sy = y + Math.max(0, i - (w - 1));
            line(
                sx,
                sy,
                x + Math.max(0, i - (h - 1)),
                y + Math.min(i, h - 1)
            );
        }
    }

    let pieces = [];
    let grid = new Uint8Array(0);
    let stack = [];
    let held = null;
    let trayIndex = 0;
    let message = 'PICK A CRATE';
    let blockedCells = new Set();
    let jobMode = null;

    function cellIndex(x, y) {
        return y * COLS + x;
    }

    function normalizeCells(cells) {
        let minX = cells[0][0];
        let minY = cells[0][1];
        let maxX = minX;
        let maxY = minY;
        for (const [x, y] of cells) {
            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
        const normalized = cells.map(([x, y]) => [
            x - minX,
            y - minY,
        ]);
        return {
            cells: normalized,
            w: maxX - minX + 1,
            h: maxY - minY + 1,
            area: cells.length,
        };
    }

    function rotateCells(cells) {
        let maxY = 0;
        for (const [, y] of cells)
            if (y > maxY) maxY = y;
        return normalizeCells(
            cells.map(([x, y]) => [maxY - y, x])
        );
    }

    function cellSet(cells) {
        const set = new Set();
        for (const [x, y] of cells)
            set.add(`${x},${y}`);
        return set;
    }

    function canPlace(x, y, cells) {
        for (const [cx, cy] of cells) {
            const gx = x + cx;
            const gy = y + cy;
            if (
                gx < 0 ||
                gy < 0 ||
                gx >= COLS ||
                gy >= ROWS ||
                blockedCells.has(`${gx},${gy}`) ||
                grid[cellIndex(gx, gy)] !== 0
            )
                return false;
        }
        return true;
    }

    function hasSupport(x, y, cells) {
        const set = cellSet(cells);
        for (const [cx, cy] of cells) {
            if (set.has(`${cx},${cy + 1}`))
                continue;
            const below = y + cy + 1;
            if (
                below >= ROWS ||
                grid[cellIndex(x + cx, below)] !== 0
            )
                return true;
        }
        return false;
    }

    function canPickUp(piece) {
        for (let dx = 0; dx < piece.w; dx++) {
            for (let y = 0; y < piece.y; y++) {
                if (
                    grid[cellIndex(piece.x + dx, y)] !== 0
                )
                    return false;
            }
        }
        return true;
    }

    function removeFromStack(index) {
        const pos = stack.lastIndexOf(index);
        if (pos >= 0)
            stack.splice(pos, 1);
    }

    function findStart(piece) {
        const center = Math.floor((COLS - piece.w) / 2);
        for (let n = 0; n < COLS; n++) {
            const x = (center + n) % COLS;
            if (x + piece.w > COLS)
                continue;
            if (canPlace(x, 0, piece.cells))
                return x;
        }
        return -1;
    }

    function markPiece(piece, x, y, cells) {
        for (const [cx, cy] of cells)
            grid[cellIndex(x + cx, y + cy)] =
                piece.index + 2;
    }

    function clearPiece(piece) {
        for (const [cx, cy] of piece.cells)
            grid[cellIndex(piece.x + cx, piece.y + cy)] = 0;
    }

    function packedCells() {
        let total = 0;
        for (const piece of pieces) {
            if (piece.placed)
                total += piece.area;
        }
        return total;
    }

    function requiredPlaced() {
        return pieces.filter(
            (piece) => piece.placed && piece.kind === 'req'
        ).length;
    }

    function bonusPlaced() {
        return pieces.filter(
            (piece) => piece.placed && piece.kind === 'bonus'
        ).length;
    }

    function allPlaced() {
        return pieces.every((piece) => piece.placed);
    }

    function selectNext(from) {
        for (let n = 1; n <= pieces.length; n++) {
            const index = (from + n) % pieces.length;
            if (!pieces[index].placed) {
                trayIndex = index;
                return;
            }
        }
        trayIndex = 0;
    }

    function selectFirst() {
        for (let i = 0; i < pieces.length; i++) {
            if (!pieces[i].placed) {
                trayIndex = i;
                return;
            }
        }
        trayIndex = 0;
    }

    function cycleTray(dir) {
        trayIndex =
            (trayIndex + dir + pieces.length) %
            pieces.length;
    }

    function pickOrPlace() {
        if (held) {
            if (
                !hasSupport(
                    held.x,
                    held.y,
                    held.cells
                )
            ) {
                message = 'NEEDS SUPPORT';
            } else {
                const piece = pieces[held.index];
                markPiece(
                    piece,
                    held.x,
                    held.y,
                    held.cells
                );
                piece.cells = held.cells;
                piece.w = held.w;
                piece.h = held.h;
                piece.placed = true;
                piece.x = held.x;
                piece.y = held.y;
                stack.push(piece.index);
                held = null;
                message = allPlaced()
                    ? 'ALL CRATES PLACED'
                    : 'RELEASED';
                selectNext(piece.index);
            }
        } else {
            const piece = pieces[trayIndex];
            if (!piece) {
                message = 'NO CRATE SELECTED';
            } else if (piece.placed) {
                if (!canPickUp(piece)) {
                    message = 'BLOCKED ABOVE';
                } else {
                    clearPiece(piece);
                    piece.placed = false;
                    held = {
                        index: piece.index,
                        x: piece.x,
                        y: piece.y,
                        cells: piece.cells,
                        w: piece.w,
                        h: piece.h,
                    };
                    removeFromStack(piece.index);
                    message =
                        'LIFTED · D-PAD MOVE · A RELEASE';
                }
            } else {
                const x = findStart(piece);
                if (x < 0) {
                    message = 'NO TOP SPACE';
                } else {
                    held = {
                        index: piece.index,
                        x,
                        y: 0,
                        cells: piece.cells,
                        w: piece.w,
                        h: piece.h,
                    };
                    message =
                        'D-PAD LOWER · A RELEASE · B ROTATE';
                }
            }
        }
        updatePanel();
    }

    function rotateOrUndo() {
        if (held) {
            const rotated = rotateCells(held.cells);
            if (
                canPlace(
                    held.x,
                    held.y,
                    rotated.cells
                )
            ) {
                held.cells = rotated.cells;
                held.w = rotated.w;
                held.h = rotated.h;
                message = 'ROTATED';
            } else {
                message = 'NO ROOM TO ROTATE';
            }
        } else if (stack.length) {
            const index = stack.pop();
            const piece = pieces[index];
            clearPiece(piece);
            piece.placed = false;
            trayIndex = index;
            held = null;
            message = 'UNDID CRATE';
        } else {
            message = 'NOTHING TO UNDO';
        }
        updatePanel();
    }

    function moveHeld(dx, dy) {
        if (!held)
            return;
        const nextX = Math.max(
            0,
            Math.min(held.x + dx, COLS - held.w)
        );
        const nextY = Math.max(
            0,
            Math.min(held.y + dy, ROWS - held.h)
        );
        if (canPlace(nextX, nextY, held.cells)) {
            held.x = nextX;
            held.y = nextY;
            message = hasSupport(
                held.x,
                held.y,
                held.cells
            )
                ? 'A RELEASE  B ROTATE'
                : 'LOWER TO SUPPORT';
        } else {
            message = 'BLOCKED';
        }
        updatePanel();
    }

    function reset() {
        grid.fill(0);
        for (const key of blockedCells) {
            const [bx, by] = key.split(',').map(Number);
            grid[cellIndex(bx, by)] = 1;
        }
        for (const piece of pieces) {
            piece.placed = false;
            piece.x = 0;
            piece.y = 0;
            piece.cells = piece.baseCells;
            piece.w = piece.baseW;
            piece.h = piece.baseH;
        }
        stack.length = 0;
        held = null;
        trayIndex = 0;
        message = 'PICK A CRATE';
        selectFirst();
        updatePanel();
    }

    function debugPlace(index, x, y) {
        const piece = pieces[index];
        if (!piece || piece.placed)
            return false;
        if (!canPlace(x, y, piece.cells))
            return false;
        markPiece(piece, x, y, piece.cells);
        piece.placed = true;
        piece.x = x;
        piece.y = y;
        stack.push(index);
        selectNext(index);
        updatePanel();
        return true;
    }

    function debugSolve() {
        reset();
        for (const item of SOLUTION)
            debugPlace(item.index, item.x, item.y);
        message = 'ALL CRATES PLACED';
        updatePanel();
    }

    function drawShape(cells, x, y, filled) {
        const set = cellSet(cells);
        if (filled) {
            ctx.fillStyle = '#fff';
            for (const [cx, cy] of cells)
                ctx.fillRect(
                    x + cx * CELL,
                    y + cy * CELL,
                    CELL,
                    CELL
                );
            ctx.fillStyle = '#000';
            for (const [cx, cy] of cells) {
                const sx = x + cx * CELL;
                const sy = y + cy * CELL;
                if (!set.has(`${cx},${cy - 1}`))
                    ctx.fillRect(sx, sy, CELL, 1);
                if (!set.has(`${cx},${cy + 1}`))
                    ctx.fillRect(sx, sy + CELL - 1, CELL, 1);
                if (!set.has(`${cx - 1},${cy}`))
                    ctx.fillRect(sx, sy, 1, CELL);
                if (!set.has(`${cx + 1},${cy}`))
                    ctx.fillRect(
                        sx + CELL - 1,
                        sy,
                        1,
                        CELL
                    );
            }
            return;
        }
        ctx.fillStyle = '#fff';
        for (const [cx, cy] of cells) {
            const sx = x + cx * CELL;
            const sy = y + cy * CELL;
            if (!set.has(`${cx},${cy - 1}`))
                ctx.fillRect(sx, sy, CELL, 1);
            if (!set.has(`${cx},${cy + 1}`))
                ctx.fillRect(sx, sy + CELL - 1, CELL, 1);
            if (!set.has(`${cx - 1},${cy}`))
                ctx.fillRect(sx, sy, 1, CELL);
            if (!set.has(`${cx + 1},${cy}`))
                ctx.fillRect(sx + CELL - 1, sy, 1, CELL);
            px(sx + 4, sy + 4);
        }
    }

    function drawCrate(piece) {
        drawShape(
            piece.cells,
            HOLD_X + piece.x * CELL,
            HOLD_Y + piece.y * CELL,
            true
        );
    }

    function drawDashedRect(x, y, w, h) {
        ctx.fillStyle = '#fff';
        for (let i = 0; i < w; i += 2) {
            px(x + i, y);
            px(x + i, y + h - 1);
        }
        for (let i = 0; i < h; i += 2) {
            px(x, y + i);
            px(x + w - 1, y + i);
        }
    }

    function drawCapsule(x, y, w, h) {
        const r = Math.min(12, Math.floor(h / 2) - 1);
        const cy = y + Math.floor(h / 2);
        ctx.fillStyle = '#fff';
        ctx.fillRect(x + r, y, w - r * 2, 1);
        ctx.fillRect(x + r, y + h - 1, w - r * 2, 1);
        for (let d = 0; d <= r; d++) {
            const dy = Math.round(
                Math.sqrt(r * r - (r - d) * (r - d))
            );
            px(x + d, cy - dy);
            px(x + d, cy + dy);
            px(x + w - 1 - d, cy - dy);
            px(x + w - 1 - d, cy + dy);
        }
    }

    function drawHoldBoundary() {
        ctx.fillStyle = '#fff';
        for (let row = 0; row < ROWS; row++) {
            for (let col = 0; col < COLS; col++) {
                if (blockedCells.has(`${col},${row}`))
                    continue;
                const x = HOLD_X + col * CELL;
                const y = HOLD_Y + row * CELL;
                if (blockedCells.has(`${col},${row - 1}`) || row === 0)
                    ctx.fillRect(x, y, CELL, 1);
                if (
                    blockedCells.has(`${col},${row + 1}`) ||
                    row === ROWS - 1
                )
                    ctx.fillRect(x, y + CELL - 1, CELL, 1);
                if (blockedCells.has(`${col - 1},${row}`) || col === 0)
                    ctx.fillRect(x, y, 1, CELL);
                if (
                    blockedCells.has(`${col + 1},${row}`) ||
                    col === COLS - 1
                )
                    ctx.fillRect(x + CELL - 1, y, 1, CELL);
            }
        }
    }

    function drawInterior(hullX, hullY, hullW, hullH) {
        const holdW = COLS * CELL;
        const holdH = ROWS * CELL;
        const hullBottom = hullY + hullH - 1;
        const deckY = HOLD_Y - 3;
        const bulkL = HOLD_X - 4;
        const bulkR = HOLD_X + holdW + 3;
        const radius = Math.min(
            12,
            Math.floor(hullH / 2) - 1
        );
        const deckL = hullX + radius + 1;
        const deckR = hullX + hullW - radius - 2;

        // Deck line, open over the cargo hatch.
        ctx.fillStyle = '#fff';
        if (deckL < bulkL)
            ctx.fillRect(deckL, deckY, bulkL - deckL, 1);
        if (bulkR < deckR)
            ctx.fillRect(bulkR, deckY, deckR - bulkR, 1);

        // Hold bulkheads and floor.
        ctx.fillRect(
            bulkL,
            deckY + 1,
            1,
            hullBottom - deckY - 1
        );
        ctx.fillRect(
            bulkR,
            deckY + 1,
            1,
            hullBottom - deckY - 1
        );
        ctx.fillRect(
            HOLD_X,
            HOLD_Y + holdH - 1,
            holdW,
            1
        );

        // Forward crew cabin.
        const crewX = hullX + 8;
        const crewW = bulkL - crewX - 2;
        if (crewW >= 5) {
            const crewY = HOLD_Y + 2;
            outline(crewX, crewY, crewW, 7);
            ctx.fillStyle = '#fff';
            ctx.fillRect(
                crewX + 2,
                crewY + 2,
                crewW - 4,
                1
            );
            ctx.fillRect(
                crewX + 2,
                crewY + 4,
                crewW - 4,
                1
            );
            px(crewX + 2, crewY + 5);
        }

        // Aft engine room and drive shaft.
        const engineX = bulkR + 2;
        const engineW = deckR - engineX;
        if (engineW >= 5) {
            const engineY = HOLD_Y + 2;
            outline(engineX, engineY, engineW, 8);
            ctx.fillStyle = '#fff';
            for (let x = engineX + 2; x < engineX + engineW - 1; x += 3)
                ctx.fillRect(x, engineY + 2, 1, 5);
            line(
                engineX + Math.floor(engineW / 2),
                engineY + 8,
                hullX + hullW - 9,
                engineY + 8
            );
        }

        // Ballast tanks under the hold, still inside the hull.
        const tankY = HOLD_Y + holdH + 3;
        const tankH = Math.min(3, hullBottom - tankY);
        if (tankH >= 2) {
            const tankW = Math.max(
                8,
                Math.floor(holdW / 2) - 4
            );
            hatchBox(HOLD_X, tankY, tankW, tankH);
            hatchBox(
                HOLD_X + holdW - tankW,
                tankY,
                tankW,
                tankH
            );
        }
    }

    function drawHull() {
        const sub = SUBS[SUB_INDEX];
        const holdW = COLS * CELL;
        const holdH = ROWS * CELL;
        const hullW = Math.min(122, holdW + sub.side * 2);
        const hullX = Math.floor((W - hullW) / 2);
        const hullY = 11;
        const hullH = holdH + 16;

        ctx.fillStyle = '#fff';
        // Outer screen frame.
        ctx.fillRect(0, 0, W, 1);
        ctx.fillRect(0, H - 1, W, 1);
        ctx.fillRect(0, 0, 1, H);
        ctx.fillRect(W - 1, 0, 1, H);

        // Rounded hull shell, then the internal compartments.
        drawCapsule(hullX, hullY, hullW, hullH);
        drawInterior(hullX, hullY, hullW, hullH);

        // Hatch posts above the hold.
        const hatchX = HOLD_X + Math.floor(holdW / 2);
        ctx.fillRect(hatchX - 8, hullY, 1, HOLD_Y - hullY);
        ctx.fillRect(hatchX + 8, hullY, 1, HOLD_Y - hullY);
    }

    function drawHold() {
        for (let row = 0; row < ROWS; row++) {
            for (let col = 0; col < COLS; col++) {
                const x = HOLD_X + col * CELL;
                const y = HOLD_Y + row * CELL;
                if (blockedCells.has(`${col},${row}`)) {
                    let full = true;
                    for (let yy = 0; yy < ROWS; yy++) {
                        if (!blockedCells.has(`${col},${yy}`)) {
                            full = false;
                            break;
                        }
                    }
                    ctx.fillStyle = '#fff';
                    if (full) {
                        ctx.fillRect(x + 2, y, 1, CELL);
                        ctx.fillRect(
                            x + CELL - 3,
                            y,
                            1,
                            CELL
                        );
                    } else {
                        outline(x + 1, y + 1, CELL - 2, CELL - 2);
                        line(
                            x + 2,
                            y + 2,
                            x + CELL - 3,
                            y + CELL - 3
                        );
                        line(
                            x + CELL - 3,
                            y + 2,
                            x + 2,
                            y + CELL - 3
                        );
                    }
                } else if (grid[cellIndex(col, row)] === 0) {
                    ctx.fillStyle = '#fff';
                    px(x + 1, y + 1);
                }
            }
        }
    }

    function drawTray() {
        const y = 54;
        for (let i = 0; i < pieces.length; i++) {
            const x = 32 + i * 8;
            const piece = pieces[i];
            if (i === trayIndex) {
                ctx.fillStyle = '#fff';
                ctx.fillRect(x - 1, y - 1, 10, 1);
                ctx.fillRect(x - 1, y + 8, 10, 1);
                ctx.fillRect(x - 1, y - 1, 1, 10);
                ctx.fillRect(x + 8, y - 1, 1, 10);
            }
            let cell = 2;
            let gap = 1;
            if (
                piece.w * cell + (piece.w - 1) * gap > 8 ||
                piece.h * cell + (piece.h - 1) * gap > 8
            ) {
                cell = 1;
                gap = 1;
            }
            if (
                piece.w * cell + (piece.w - 1) * gap > 8 ||
                piece.h * cell + (piece.h - 1) * gap > 8
            )
                gap = 0;
            const iw =
                piece.w * cell + (piece.w - 1) * gap;
            const ih =
                piece.h * cell + (piece.h - 1) * gap;
            const ix = x + Math.floor((8 - iw) / 2);
            const iy = y + Math.floor((8 - ih) / 2);
            ctx.fillStyle = '#fff';
            for (const [cx, cy] of piece.cells) {
                ctx.fillRect(
                    ix + cx * (cell + gap),
                    iy + cy * (cell + gap),
                    cell,
                    cell
                );
            }
            if (piece.placed) {
                let first = piece.cells[0];
                for (const cellPos of piece.cells) {
                    if (
                        cellPos[1] < first[1] ||
                        (cellPos[1] === first[1] &&
                            cellPos[0] < first[0])
                    )
                        first = cellPos;
                }
                ctx.fillStyle = '#000';
                px(
                    ix + first[0] * (cell + gap) +
                        Math.floor(cell / 2),
                    iy + first[1] * (cell + gap) +
                        Math.floor(cell / 2)
                );
            }
            ctx.fillStyle = '#fff';
            if (piece.kind === 'bonus')
                px(x + 6, y + 6);
        }
    }

    function drawStatus() {
        drawText(SUBS[SUB_INDEX].name, 4, 1, 1);
        drawTextRight(
            `PACK ${packedCells()}/${AVAILABLE}`,
            124,
            1,
            1
        );
    }

    function drawCable() {
        if (!held)
            return;
        const x = HOLD_X + held.x * CELL;
        const y = HOLD_Y + held.y * CELL;
        const w = held.w * CELL;
        const cx = x + Math.floor(w / 2);
        ctx.fillStyle = '#fff';
        for (let gy = HOLD_Y; gy < y; gy += 2)
            px(cx, gy);
    }

    function drawHeld() {
        if (!held)
            return;
        drawShape(
            held.cells,
            HOLD_X + held.x * CELL,
            HOLD_Y + held.y * CELL,
            false
        );
    }

    function draw() {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        drawHull();
        drawHold();
        drawCable();
        for (const piece of pieces) {
            if (piece.placed)
                drawCrate(piece);
        }
        drawHoldBoundary();
        drawHeld();
        drawTray();
        drawStatus();
    }

    function shuffle(items) {
        for (let i = items.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            const tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
        }
        return items;
    }

    function targetFor(sub) {
        const openByCol = [];
        for (let x = 0; x < sub.cols; x++) {
            const ys = [];
            for (let y = sub.rows - 1; y >= 0; y--) {
                if (
                    !sub.blocked.some(
                        ([bx, by]) => bx === x && by === y
                    )
                )
                    ys.push(y);
            }
            openByCol.push(ys);
        }
        const openCols = [];
        for (let x = 0; x < sub.cols; x++)
            if (openByCol[x].length) openCols.push(x);

        const counts = new Array(sub.cols).fill(0);
        for (const x of openCols)
            counts[x] = 1;
        let remaining = sub.load - openCols.length;
        const available = openCols.filter(
            (x) => counts[x] < openByCol[x].length
        );
        while (remaining > 0 && available.length) {
            const pick = Math.floor(
                Math.random() * available.length
            );
            const x = available[pick];
            counts[x]++;
            remaining--;
            if (counts[x] >= openByCol[x].length)
                available.splice(pick, 1);
        }

        const cells = [];
        for (const x of openCols) {
            for (let i = 0; i < counts[x]; i++)
                cells.push([x, openByCol[x][i]]);
        }
        return cells;
    }

    function clusterFromCells(cells) {
        let minX = cells[0][0];
        let maxX = minX;
        let minY = cells[0][1];
        let maxY = minY;
        for (const [x, y] of cells) {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
        return {
            cells,
            minX,
            maxX,
            minY,
            maxY,
            area: cells.length,
            active: true,
        };
    }

    function buildLoad(sub, clusters) {
        const active = clusters.filter((c) => c.active);
        const ordered = shuffle(active.slice());
        const req = ordered.slice(0, sub.required);
        const bonus = ordered.slice(sub.required);
        const finalOrder = req.concat(bonus);
        const nextPieces = [];
        const solution = [];
        for (let i = 0; i < finalOrder.length; i++) {
            const cluster = finalOrder[i];
            const shape = normalizeCells(cluster.cells);
            nextPieces.push({
                index: i,
                kind: i < sub.required ? 'req' : 'bonus',
                cells: shape.cells,
                baseCells: shape.cells.map((cell) => cell.slice()),
                w: shape.w,
                h: shape.h,
                baseW: shape.w,
                baseH: shape.h,
                area: shape.area,
                x: 0,
                y: 0,
                placed: false,
            });
            solution.push({
                index: i,
                x: cluster.minX,
                y: cluster.minY,
            });
        }
        return { pieces: nextPieces, solution };
    }

    function fallbackLoad(sub) {
        const target = targetFor(sub);
        const runs = [];
        let run = [];
        for (let x = 0; x < sub.cols; x++) {
            const open = target.some(([tx]) => tx === x);
            if (open) {
                run.push(x);
            } else if (run.length) {
                runs.push(run);
                run = [];
            }
        }
        if (run.length) runs.push(run);

        const allocation = runs.map(() => 1);
        let remaining = sub.pieces - runs.length;
        while (remaining > 0) {
            let best = 0;
            for (let i = 1; i < runs.length; i++) {
                const room =
                    runs[i].length - allocation[i];
                const bestRoom =
                    runs[best].length - allocation[best];
                if (room > bestRoom) best = i;
            }
            allocation[best]++;
            remaining--;
        }

        const clusters = [];
        for (let i = 0; i < runs.length; i++) {
            const cols = runs[i];
            const count = Math.min(
                allocation[i],
                cols.length
            );
            for (let k = 0; k < count; k++) {
                const first = Math.floor(
                    (k * cols.length) / count
                );
                const last =
                    Math.floor(
                        ((k + 1) * cols.length) / count
                    ) - 1;
                const cells = target.filter(([x]) => {
                    const at = cols.indexOf(x);
                    return at >= first && at <= last;
                });
                if (cells.length)
                    clusters.push(clusterFromCells(cells));
            }
        }
        // If blocked central columns limited the split, fill any
        // shortfall by splitting the largest cluster.
        while (clusters.length < sub.pieces) {
            clusters.sort((a, b) => b.area - a.area);
            const largest = clusters.shift();
            const cells = largest.cells.slice();
            const half = Math.max(1, Math.floor(cells.length / 2));
            clusters.push(clusterFromCells(cells.slice(0, half)));
            clusters.push(clusterFromCells(cells.slice(half)));
        }
        return buildLoad(sub, clusters.slice(0, sub.pieces));
    }

    function generateLoad(sub) {
        for (let attempt = 0; attempt < 80; attempt++) {
            const target = targetFor(sub);
            const owner = new Int16Array(COLS * ROWS);
            owner.fill(-1);
            const clusters = target.map(([x, y]) => {
                const cluster = clusterFromCells([[x, y]]);
                return cluster;
            });
            clusters.forEach((cluster, index) => {
                const [x, y] = cluster.cells[0];
                owner[cellIndex(x, y)] = index;
            });
            let active = clusters.length;
            let failed = false;

            while (active > sub.pieces) {
                const seen = new Set();
                const candidates = [];
                for (const [x, y] of target) {
                    const a = owner[cellIndex(x, y)];
                    if (a < 0) continue;
                    for (const [nx, ny] of [
                        [x + 1, y],
                        [x, y + 1],
                    ]) {
                        if (
                            nx >= sub.cols ||
                            ny >= sub.rows
                        )
                            continue;
                        const b = owner[cellIndex(nx, ny)];
                        if (b < 0 || a === b) continue;
                        const key =
                            a < b ? `${a}:${b}` : `${b}:${a}`;
                        if (seen.has(key)) continue;
                        seen.add(key);
                        const A = clusters[a];
                        const B = clusters[b];
                        if (!A.active || !B.active) continue;
                        const minX = Math.min(A.minX, B.minX);
                        const maxX = Math.max(A.maxX, B.maxX);
                        const minY = Math.min(A.minY, B.minY);
                        const maxY = Math.max(A.maxY, B.maxY);
                        const area = A.area + B.area;
                        const w = maxX - minX + 1;
                        const h = maxY - minY + 1;
                        if (
                            area > sub.maxArea ||
                            w > sub.maxW ||
                            h > sub.maxH
                        )
                            continue;
                        candidates.push({
                            a,
                            b,
                            area,
                            minX,
                            maxX,
                            minY,
                            maxY,
                        });
                    }
                }
                if (!candidates.length) {
                    failed = true;
                    break;
                }
                candidates.sort((p, q) => p.area - q.area);
                const pool = Math.min(8, candidates.length);
                const pick =
                    candidates[Math.floor(Math.random() * pool)];
                const A = clusters[pick.a];
                const B = clusters[pick.b];
                A.cells.push(...B.cells);
                A.minX = pick.minX;
                A.maxX = pick.maxX;
                A.minY = pick.minY;
                A.maxY = pick.maxY;
                A.area = pick.area;
                B.active = false;
                for (const [x, y] of B.cells)
                    owner[cellIndex(x, y)] = pick.a;
                active--;
            }

            if (!failed && active === sub.pieces) {
                const result = buildLoad(sub, clusters);
                const cells = result.pieces.reduce(
                    (sum, piece) => sum + piece.area,
                    0
                );
                if (cells === sub.load)
                    return result;
            }
        }
        return fallbackLoad(sub);
    }

    function requiredTotal() {
        return pieces.filter((piece) => piece.kind === 'req').length;
    }

    function bonusTotal() {
        return pieces.filter((piece) => piece.kind === 'bonus').length;
    }

    function updatePanel() {
        const piece = pieces[trayIndex];
        const heldText = held
            ? `${held.w}x${held.h} at ${held.x},${held.y} ${
                  hasSupport(held.x, held.y, held.cells)
                      ? 'SUPPORTED'
                      : 'UNSUPPORTED'
              }`
            : 'none';
        const selected = piece
            ? `${piece.w}x${piece.h} ${piece.kind}${
                  piece.placed ? ' installed' : ''
              }`
            : 'none';
        const pct = Math.floor(
            (packedCells() / Math.max(1, AVAILABLE)) * 100
        );
        const lines = [
            `${SUBS[SUB_INDEX].name} · ${COLS}×${ROWS} hold · ${CELL}px cells`,
            `packed ${packedCells()}/${AVAILABLE} (${pct}%)`,
            `pilot load ${packedUnits()}/${PILOT_UNITS} units`,
            `required ${requiredPlaced()}/${requiredTotal()} · bonus ${bonusPlaced()}/${bonusTotal()}`,
            `${SUBS[SUB_INDEX].role}`,
            `held ${heldText}`,
            `tray ${selected}`,
            `status ${message}`,
        ];
        if (jobMode)
            lines.unshift(
                `JOB ${jobMode.crates} CR → ${jobMode.toName}`
            );
        debugEl.textContent = lines.join('\n');
    }

    function updateSubButtons() {
        for (const button of subButtons) {
            button.classList.toggle(
                'active',
                Number(button.dataset.cargoSub) === SUB_INDEX
            );
        }
    }

    function selectSub(index, sync = true) {
        SUB_INDEX =
            ((Math.round(index) % SUBS.length) +
                SUBS.length) %
            SUBS.length;
        if (sync)
            window.SubHaulerSubs.setSelected(SUB_INDEX);
        const sub = SUBS[SUB_INDEX];
        COLS = sub.cols;
        ROWS = sub.rows;
        CELL = sub.cell;
        HOLD_X = Math.floor((W - COLS * CELL) / 2);
        HOLD_Y = 18;
        AVAILABLE = sub.capacity;
        blockedCells = new Set(
            sub.blocked.map(([x, y]) => `${x},${y}`)
        );
        const load = generateLoad(sub);
        pieces = load.pieces;
        SOLUTION = load.solution;
        grid = new Uint8Array(COLS * ROWS);
        reset();
        if (jobMode)
            message = `PACK ${jobMode.crates} CRATES`;
        updateSubButtons();
        updateLoadingUi();
        updatePanel();
    }

    function newLoad() {
        selectSub(SUB_INDEX);
    }

    function packedUnits() {
        return Math.min(
            PILOT_UNITS,
            Math.ceil(
                (packedCells() * PILOT_UNITS) /
                    Math.max(1, AVAILABLE)
            )
        );
    }

    function updateLoadingUi() {
        if (titleEl)
            titleEl.textContent = jobMode
                ? 'LOAD CARGO'
                : 'CARGO PACK';
        if (subtitleEl)
            subtitleEl.textContent = jobMode
                ? `job ${jobMode.crates} crates → ${jobMode.toName}`
                : 'selected hull · packing & load handoff';
        if (loadButton)
            loadButton.textContent = jobMode
                ? 'LOAD & UNDOCK'
                : 'LOAD TO PILOT';
    }

    function startJob(job) {
        jobMode = job
            ? {
                  crates: Math.max(1, Math.round(job.crates)),
                  toName: job.toName || 'DESTINATION',
              }
            : null;
        selectSub(SUB_INDEX, false);
        message = jobMode
            ? `PACK ${jobMode.crates} CRATES`
            : 'PICK A CRATE';
        updateLoadingUi();
        updatePanel();
    }

    function cancelJob() {
        jobMode = null;
        message = 'LOAD CANCELLED';
        updateLoadingUi();
        updatePanel();
        if (window.subHaulerSetMode)
            window.subHaulerSetMode('pilot');
    }

    function sendLoad() {
        const api = window.subHaulerTrim;
        if (!api || !api.setCargoLoad) {
            message = 'PILOT NOT READY';
            updatePanel();
            return false;
        }
        if (
            jobMode &&
            packedUnits() < jobMode.crates
        ) {
            message = `NEED ${jobMode.crates} CRATES`;
            updatePanel();
            return false;
        }
        const ok = api.setCargoLoad(
            packedCells(),
            AVAILABLE
        );
        if (!ok) {
            message = 'NO ROOM TO LOAD';
            updatePanel();
            return false;
        }
        if (jobMode) {
            jobMode = null;
            message = 'LOADED · UNDOCKING';
            updateLoadingUi();
            updatePanel();
            if (window.subHaulerSetMode)
                window.subHaulerSetMode('pilot');
            if (api.undock)
                api.undock();
            return true;
        }
        message = 'LOADED TO PILOT';
        updatePanel();
        return true;
    }

    window.addEventListener('keydown', (event) => {
        if (window.__uiMode && window.__uiMode !== 'cargo')
            return;
        if (
            event.code === 'ArrowLeft' ||
            event.code === 'ArrowRight' ||
            event.code === 'ArrowUp' ||
            event.code === 'ArrowDown'
        )
            event.preventDefault();
        if (
            event.repeat &&
            (event.code === 'KeyZ' ||
                event.code === 'KeyX' ||
                event.code === 'KeyR')
        )
            return;

        switch (event.code) {
            case 'ArrowLeft':
                if (held)
                    moveHeld(-1, 0);
                else {
                    cycleTray(-1);
                    updatePanel();
                }
                break;
            case 'ArrowRight':
                if (held)
                    moveHeld(1, 0);
                else {
                    cycleTray(1);
                    updatePanel();
                }
                break;
            case 'ArrowUp':
                if (held)
                    moveHeld(0, -1);
                break;
            case 'ArrowDown':
                if (held)
                    moveHeld(0, 1);
                break;
            case 'KeyZ':
                pickOrPlace();
                break;
            case 'KeyX':
                rotateOrUndo();
                break;
            case 'KeyR':
                reset();
                break;
            case 'Digit1':
                selectSub(0);
                break;
            case 'Digit2':
                selectSub(1);
                break;
            case 'Digit3':
                selectSub(2);
                break;
            case 'Enter':
            case 'KeyL':
                sendLoad();
                break;
            case 'Escape':
                if (jobMode)
                    cancelJob();
                break;
            default:
                break;
        }
    });

    window.cargoSlice = {
        state: () => ({
            sub: SUBS[SUB_INDEX].name,
            cols: COLS,
            rows: ROWS,
            available: AVAILABLE,
            packed: packedCells(),
            required: requiredPlaced(),
            bonus: bonusPlaced(),
            held: held
                ? {
                      index: held.index,
                      x: held.x,
                      y: held.y,
                      w: held.w,
                      h: held.h,
                  }
                : null,
            trayIndex,
            message,
            job: jobMode ? { ...jobMode } : null,
            units: packedUnits(),
            pieces: pieces.map((piece) => ({
                index: piece.index,
                kind: piece.kind,
                placed: piece.placed,
                x: piece.x,
                y: piece.y,
                w: piece.w,
                h: piece.h,
            })),
        }),
        reset,
        newLoad,
        startJob,
        cancelJob,
        sendLoad,
        selectSub,
        select: (index) => {
            if (pieces[index])
                trayIndex = index;
            updatePanel();
        },
        pickOrPlace,
        rotateOrUndo,
        moveHeld,
        debugPlace,
        debugSolve,
    };

    for (const button of subButtons) {
        button.addEventListener('click', () => {
            selectSub(Number(button.dataset.cargoSub));
        });
    }
    if (loadButton)
        loadButton.addEventListener('click', sendLoad);
    if (cancelButton)
        cancelButton.addEventListener('click', cancelJob);
    window.SubHaulerSubs.subscribe((index) => {
        if (index !== SUB_INDEX)
            selectSub(index, false);
        else
            updateSubButtons();
    });
    selectSub(SUB_INDEX, false);
    draw();
    requestAnimationFrame(function frame() {
        draw();
        requestAnimationFrame(frame);
    });
})();
