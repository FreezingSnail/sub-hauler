// Fleet mockups: selected hull, external profile and internal cutaway.
(function () {
    'use strict';

    const canvas = document.getElementById('fleetGame');
    if (!canvas)
        return;
    const ctx = canvas.getContext('2d');
    const debugEl = document.getElementById('fleetDebug');
    const W = 128;
    const H = 64;
    const CY = 38;
    const FONT = window.SubHaulerFont;

    const VARIANTS = window.SubHaulerSubs.list;

    let selected = window.SubHaulerSubs.getSelected();
    let view = 'external';

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
        ctx.fillRect(x, y, w, 1);
        ctx.fillRect(x, y + h - 1, w, 1);
        ctx.fillRect(x, y, 1, h);
        ctx.fillRect(x + w - 1, y, 1, h);
    }

    function hatchBox(x, y, w, h) {
        outline(x, y, w, h);
        for (let i = 2; i < w + h - 2; i += 3) {
            const startX = x + Math.min(i, w - 1);
            const startY = y + Math.max(0, i - (w - 1));
            line(startX, startY, x + Math.max(0, i - (h - 1)), y + Math.min(i, h - 1));
        }
    }

    function drawText(text, x, y) {
        if (!FONT)
            return;
        const value = String(text).toUpperCase();
        let cursor = x;
        ctx.fillStyle = '#fff';
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

    function drawTextRight(text, right, y) {
        if (!FONT)
            return;
        drawText(text, right - (String(text).length * 6 - 1), y);
    }

    function metrics(variant) {
        const bow = Math.floor((W - variant.length) / 2);
        const stern = bow + variant.length - 1;
        const radius = Math.floor(variant.height / 2);
        if (variant.style === 'teardrop' || variant.style === 'flatdeck') {
            return {
                bow,
                stern,
                radius,
                bodyL: bow + radius,
                bodyR: stern - radius,
                deck: CY - radius,
                keel: CY + radius,
            };
        }
        return {
            bow,
            stern,
            radius,
            bodyL: bow + 4,
            bodyR: stern - 3,
            deck: CY - radius,
            keel: CY + radius,
        };
    }

    function hullBounds(variant, m, x) {
        if (variant.style === 'teardrop') {
            let half = m.radius;
            if (x < m.bodyL) {
                const dx = m.bodyL - x;
                half = Math.round(
                    Math.sqrt(
                        m.radius * m.radius - dx * dx
                    )
                );
            } else if (x > m.bodyR) {
                const dx = x - m.bodyR;
                half = Math.round(
                    Math.sqrt(
                        m.radius * m.radius - dx * dx
                    )
                );
            }
            return { top: CY - half, bottom: CY + half };
        }
        let top = m.deck;
        if (x < m.bow + 4) {
            top =
                m.keel +
                Math.round(
                    ((m.deck - m.keel) * (x - m.bow)) / 4
                );
        } else if (x > m.stern - 2) {
            top =
                m.keel +
                Math.round(
                    ((m.deck - m.keel) * (m.stern - x)) /
                        2
                );
        }
        let bottom = m.keel;
        if (variant.style === 'heavy') {
            const notchStart =
                m.bow + Math.floor(variant.length * 0.42);
            const notchEnd = notchStart + 22;
            if (x >= notchStart && x <= notchEnd)
                bottom = m.keel - 4;
        }
        return { top, bottom };
    }

    function drawHull(variant, m, filled) {
        ctx.fillStyle = '#fff';
        if (filled) {
            for (let x = m.bow; x <= m.stern; x++) {
                const b = hullBounds(variant, m, x);
                if (b.bottom >= b.top)
                    ctx.fillRect(
                        x,
                        b.top,
                        1,
                        b.bottom - b.top + 1
                    );
            }
            return;
        }

        if (variant.style === 'teardrop' || variant.style === 'flatdeck') {
            for (let x = m.bodyL; x <= m.bodyR; x++) {
                px(x, m.deck);
                px(x, m.keel);
            }
            for (let d = 0; d <= m.radius; d++) {
                const dy = Math.round(
                    Math.sqrt(m.radius * m.radius - d * d)
                );
                px(m.bodyL - d, CY - dy);
                px(m.bodyR + d, CY - dy);
                if (dy > 0) {
                    px(m.bodyL - d, CY + dy);
                    px(m.bodyR + d, CY + dy);
                }
            }
            return;
        }

        ctx.fillRect(
            m.bow + 4,
            m.deck,
            variant.length - 6,
            1
        );
        if (variant.style === 'heavy') {
            const notchStart =
                m.bow + Math.floor(variant.length * 0.42);
            const notchEnd = notchStart + 22;
            ctx.fillRect(
                m.bow,
                m.keel,
                notchStart - m.bow,
                1
            );
            ctx.fillRect(
                notchEnd,
                m.keel,
                m.stern - notchEnd,
                1
            );
            ctx.fillRect(
                notchStart,
                m.keel - 4,
                notchEnd - notchStart,
                1
            );
            ctx.fillRect(
                notchStart,
                m.keel - 4,
                1,
                5
            );
            ctx.fillRect(
                notchEnd,
                m.keel - 4,
                1,
                5
            );
        } else {
            ctx.fillRect(m.bow, m.keel, variant.length, 1);
        }
        line(m.bow, m.keel, m.bow + 4, m.deck);
        line(m.stern, m.keel, m.stern - 2, m.deck);
    }

    function drawSail(variant, m, cutaway) {
        const sailX =
            m.bow +
            Math.floor(
                variant.length *
                    (variant.towerX || 0.38)
            );
        const base = m.deck - 1;
        const top = base - variant.sailH;
        line(sailX, base, sailX + 2, top);
        line(sailX + variant.sailW, base, sailX + variant.sailW - 2, top);
        ctx.fillStyle = '#fff';
        ctx.fillRect(sailX + 2, top, variant.sailW - 4, 1);
        ctx.fillRect(sailX, base, variant.sailW, 1);

        const mastX = sailX + Math.floor(variant.sailW / 2);
        line(mastX, top, mastX, top - 3);
        line(mastX, top - 3, mastX + 2, top - 3);
        px(mastX + 2, top - 2);
        line(mastX + 4, top, mastX + 4, top - 2);
        px(mastX + 4, top - 2);

        // Sail portholes / access hatch.
        px(sailX + 3, top + 3);
        px(sailX + variant.sailW - 4, top + 3);
        outline(
            sailX + Math.floor(variant.sailW / 2) - 2,
            base - 3,
            4,
            2
        );
        if (cutaway) {
            for (let y = top + 2; y < base; y += 2)
                px(mastX - 2, y);
        }
    }

    function drawDeckHatch(x, deck) {
        outline(x, deck - 2, 7, 3);
        px(x + 1, deck - 1);
        px(x + 3, deck - 1);
        px(x + 5, deck - 1);
    }

    function drawDeckhouse(variant, m, cutaway) {
        const x =
            m.bow +
            Math.floor(
                variant.length *
                    (variant.towerX || 0.58)
            );
        const w = 18;
        const h = 8;
        const top = m.deck - h;
        outline(x, top, w, h);
        ctx.fillStyle = '#fff';
        ctx.fillRect(x + 2, top + 2, 3, 1);
        ctx.fillRect(x + 7, top + 2, 3, 1);
        ctx.fillRect(x + 12, top + 2, 3, 1);
        ctx.fillRect(x + 3, top + 5, 2, 1);
        ctx.fillRect(x + 10, top + 5, 2, 1);
        line(x + 5, top, x + 5, top - 3);
        line(x + 5, top - 3, x + 8, top - 3);
        if (cutaway) {
            for (let y = top + 2; y < top + h - 1; y += 2)
                px(x + 15, y);
        }
    }

    function drawBowViewport(m) {
        const x = m.bow + 7;
        const y = CY - 2;
        ctx.fillStyle = '#fff';
        ctx.fillRect(x + 1, y, 3, 1);
        ctx.fillRect(x, y + 1, 1, 3);
        ctx.fillRect(x + 4, y + 1, 1, 3);
        ctx.fillRect(x + 1, y + 4, 3, 1);
        px(x + 2, y + 2);
    }

    function drawGantry(variant, m) {
        const x =
            m.bow +
            Math.floor(
                variant.length *
                    (variant.gantryX || 0.66)
            );
        const top = m.deck - 9;
        ctx.fillStyle = '#fff';
        ctx.fillRect(x, top, 1, 9);
        ctx.fillRect(x + 12, top, 1, 9);
        ctx.fillRect(x, top, 13, 1);
        ctx.fillRect(x + 6, top, 1, 6);
        ctx.fillRect(x + 5, top + 6, 3, 1);
        px(x + 6, top + 7);
    }

    function drawDeckTanks(variant, m) {
        for (let i = 0; i < 2; i++) {
            const x =
                m.bow +
                Math.floor(variant.length * (0.2 + i * 0.22));
            const y = m.deck - 6;
            outline(x, y, 16, 6);
            px(x + 2, y + 2);
            px(x + 13, y + 2);
            ctx.fillStyle = '#fff';
            ctx.fillRect(x + 2, y + 4, 12, 1);
        }
    }

    function drawOutriggers(variant, m) {
        for (let i = 0; i < 2; i++) {
            const x =
                m.bow +
                Math.floor(variant.length * (0.24 + i * 0.34));
            const y = m.keel - 2;
            outline(x, y, 18, 7);
            px(x + 2, y + 2);
            px(x + 15, y + 2);
            ctx.fillStyle = '#fff';
            ctx.fillRect(x + 2, y + 5, 14, 1);
        }
    }

    function drawSensorPods(variant, m) {
        const x =
            m.bow + Math.floor(variant.length * 0.56);
        outline(x, m.deck - 4, 6, 3);
        px(x + 2, m.deck - 2);
        px(x + 4, m.deck - 2);
        line(
            m.bow + 9,
            CY + 3,
            m.bow + 15,
            CY + 5
        );
    }

    function drawTailFins(variant, m) {
        line(m.stern - 10, m.deck - 3, m.stern - 10, m.deck);
        line(m.stern - 10, m.keel, m.stern - 10, m.keel + 3);
    }

    function drawExternal(variant, m) {
        drawHull(variant, m, false);

        // Longitudinal deck seam, keel line, and frame ticks.
        ctx.fillStyle = '#fff';
        ctx.fillRect(m.bodyL + 3, m.deck + 2, m.bodyR - m.bodyL - 6, 1);
        ctx.fillRect(m.bodyL + 5, m.keel - 2, m.bodyR - m.bodyL - 10, 1);
        for (let i = 1; i <= 3; i++) {
            const x =
                m.bodyL +
                Math.round(((m.bodyR - m.bodyL) * i) / 4);
            ctx.fillRect(x, m.deck + 4, 1, 2);
            ctx.fillRect(x, m.keel - 4, 1, 2);
        }
        for (let x = m.bodyL + 7; x < m.bodyR - 4; x += 14) {
            px(x, m.deck + 2);
            px(x, m.keel - 2);
        }

        // Hull-side cargo doors.
        const doorStart = m.bodyL + 10;
        const doorEnd = m.bodyR - 18;
        for (let i = 0; i < variant.doors; i++) {
            const x = Math.round(
                doorStart +
                    ((doorEnd - doorStart) * i) /
                        Math.max(1, variant.doors - 1)
            );
            ctx.fillStyle = '#fff';
            outline(x, CY - 1, 8, 4);
            px(x + 2, CY);
            px(x + 5, CY + 2);
        }

        // Portholes.
        const pStart = m.bodyL + 7;
        const pEnd = m.bodyR - 7;
        for (let i = 0; i < variant.portholes; i++) {
            const x = Math.round(
                pStart + ((pEnd - pStart) * i) / Math.max(1, variant.portholes - 1)
            );
            ctx.fillStyle = '#fff';
            px(x, CY - 3);
        }

        // Ballast vents along the lower hull.
        const vStart = m.bodyL + 9;
        const vEnd = m.bodyR - 9;
        for (let i = 0; i < variant.vents; i++) {
            const x = Math.round(
                vStart + ((vEnd - vStart) * i) / Math.max(1, variant.vents - 1)
            );
            ctx.fillStyle = '#fff';
            ctx.fillRect(x, CY + m.radius - 4, 1, 3);
        }

        // Deck cargo hatches.
        for (const t of variant.hatches) {
            drawDeckHatch(
                m.bow + Math.round(variant.length * t),
                m.deck
            );
        }

        // Tower and superstructure.
        if (variant.tower === 'deckhouse')
            drawDeckhouse(variant, m, false);
        else
            drawSail(variant, m, false);

        if (variant.viewport)
            drawBowViewport(m);
        if (variant.skids) {
            // Landing skids.
            ctx.fillStyle = '#fff';
            ctx.fillRect(m.bow + 19, m.keel + 1, 1, 3);
            ctx.fillRect(m.stern - 22, m.keel + 1, 1, 3);
        }
        if (variant.gantry)
            drawGantry(variant, m);
        if (variant.tanks)
            drawDeckTanks(variant, m);
        if (variant.outriggers)
            drawOutriggers(variant, m);
        if (variant.sensorPods)
            drawSensorPods(variant, m);
        if (variant.tailFins)
            drawTailFins(variant, m);

        // Bow dive planes.
        ctx.fillStyle = '#fff';
        line(m.bodyL + 2, CY - 2, m.bodyL + 9, CY - 5);
        line(m.bodyL + 2, CY + 2, m.bodyL + 9, CY + 5);

        // Stern planes / rudder.
        line(m.stern - 5, m.deck - 4, m.stern - 5, m.deck);
        line(m.stern - 5, m.keel, m.stern - 5, m.keel + 4);

        // Propeller and shaft.
        const propX = m.stern + 1;
        ctx.fillStyle = '#fff';
        ctx.fillRect(m.stern - 3, CY, 3, 1);
        px(propX, CY);
        line(propX, CY - 3, propX, CY + 3);
        line(propX - 2, CY - 2, propX + 2, CY + 2);
        line(propX - 2, CY + 2, propX + 2, CY - 2);
    }

    function drawCargoHold(variant, m) {
        const cell = 3;
        const gap = 1;
        const holdW =
            variant.holdW * cell + (variant.holdW - 1) * gap;
        const holdH =
            variant.holdH * cell + (variant.holdH - 1) * gap;
        const holdX = Math.floor((W - holdW) / 2);
        const holdY = CY - Math.floor(holdH / 2);
        const blocked = new Set(
            variant.blocked.map(([x, y]) => `${x},${y}`)
        );

        ctx.fillStyle = '#fff';
        const cells = [];
        for (let row = variant.holdH - 1; row >= 0; row--) {
            for (let col = 0; col < variant.holdW; col++) {
                if (blocked.has(`${col},${row}`)) continue;
                cells.push([col, row]);
            }
        }
        for (let i = 0; i < cells.length; i++) {
            const [col, row] = cells[i];
            const x = holdX + col * (cell + gap);
            const y = holdY + row * (cell + gap);
            if (i < variant.load) {
                ctx.fillStyle = '#fff';
                ctx.fillRect(x, y, cell, cell);
            } else {
                px(x + 1, y + 1);
            }
        }

        // Full-height blocked columns become bulkheads.
        for (let col = 0; col < variant.holdW; col++) {
            let full = true;
            for (let row = 0; row < variant.holdH; row++) {
                if (!blocked.has(`${col},${row}`)) {
                    full = false;
                    break;
                }
            }
            if (!full) continue;
            const bx = holdX + col * (cell + gap) + 1;
            ctx.fillStyle = '#fff';
            ctx.fillRect(bx, holdY, 1, holdH);
            for (let row = 0; row < variant.holdH; row++) {
                ctx.fillRect(
                    bx - 1,
                    holdY + row * (cell + gap),
                    3,
                    1
                );
            }
        }

        // Off-axis blocked cells: structural notches.
        for (const [col, row] of variant.blocked) {
            const x = holdX + col * (cell + gap);
            const y = holdY + row * (cell + gap);
            line(x + 1, y + 1, x + cell - 2, y + cell - 2);
            line(x + cell - 2, y + 1, x + 1, y + cell - 2);
        }

        // Outline the open hold mask, not a rectangular box.
        ctx.fillStyle = '#fff';
        for (let row = 0; row < variant.holdH; row++) {
            for (let col = 0; col < variant.holdW; col++) {
                if (blocked.has(`${col},${row}`)) continue;
                const x = holdX + col * (cell + gap);
                const y = holdY + row * (cell + gap);
                if (
                    blocked.has(`${col},${row - 1}`) ||
                    row === 0
                )
                    ctx.fillRect(x, y, cell, 1);
                if (
                    blocked.has(`${col},${row + 1}`) ||
                    row === variant.holdH - 1
                )
                    ctx.fillRect(x, y + cell - 1, cell, 1);
                if (
                    blocked.has(`${col - 1},${row}`) ||
                    col === 0
                )
                    ctx.fillRect(x, y, 1, cell);
                if (
                    blocked.has(`${col + 1},${row}`) ||
                    col === variant.holdW - 1
                )
                    ctx.fillRect(x + cell - 1, y, 1, cell);
            }
        }
        return { holdX, holdY, holdW, holdH };
    }

    function drawFlask(x, y, w, h) {
        outline(x, y, w, h);
        if (w > 4) {
            px(x + 1, y + 1);
            px(x + w - 2, y + h - 2);
        }
    }

    function drawCutaway(variant, m) {
        drawHull(variant, m, false);
        if (variant.tower === 'deckhouse')
            drawDeckhouse(variant, m, true);
        else
            drawSail(variant, m, true);

        const hold = drawCargoHold(variant, m);

        if (variant.gantry)
            drawGantry(variant, m);
        if (variant.tanks)
            drawDeckTanks(variant, m);
        if (variant.outriggers)
            drawOutriggers(variant, m);
        if (variant.sensorPods)
            drawSensorPods(variant, m);
        if (variant.tailFins)
            drawTailFins(variant, m);

        // Crew cabin forward.
        const crewW = Math.max(
            4,
            Math.min(8, hold.holdX - m.bodyL - 3)
        );
        const crewX = m.bodyL + 2;
        const crewY = CY - 3;
        outline(crewX, crewY, crewW, 7);
        line(crewX + 1, crewY + 2, crewX + crewW - 2, crewY + 2);
        line(crewX + 1, crewY + 4, crewX + crewW - 2, crewY + 4);

        // Engine room aft.
        const engineX = hold.holdX + hold.holdW + 3;
        const engineW = Math.max(5, m.bodyR - engineX - 2);
        outline(engineX, CY - 4, engineW, 9);
        for (let y = CY - 2; y < CY + 4; y += 2)
            line(engineX + 2, y, engineX + engineW - 2, y + 1);
        ctx.fillStyle = '#fff';
        ctx.fillRect(engineX + Math.floor(engineW / 2), CY + 5, 1, m.keel - CY - 5);
        line(engineX + Math.floor(engineW / 2), m.keel - 1, m.stern - 2, m.keel - 1);

        // Air flasks above the hold.
        const flaskY = Math.max(m.deck + 2, hold.holdY - 5);
        const flasks = variant.holdW > 10 ? 4 : 2;
        for (let i = 0; i < flasks; i++) {
            const x = hold.holdX + Math.round(i * (hold.holdW + 2) / flasks);
            drawFlask(x, flaskY, 3, 3);
            px(x + 1, flaskY + 3);
        }

        // Ballast tanks under the hold.
        const tankY = Math.min(m.keel - 3, hold.holdY + hold.holdH + 2);
        const tankW = Math.max(5, Math.floor(hold.holdW / 2) - 2);
        hatchBox(hold.holdX, tankY, tankW, 3);
        hatchBox(
            hold.holdX + hold.holdW - tankW,
            tankY,
            tankW,
            3
        );

        // Bulkheads around the hold.
        ctx.fillStyle = '#fff';
        ctx.fillRect(hold.holdX - 3, m.deck + 2, 1, m.keel - m.deck - 4);
        ctx.fillRect(
            hold.holdX + hold.holdW + 2,
            m.deck + 2,
            1,
            m.keel - m.deck - 4
        );

        // Low-pressure pipe run, with valves.
        const pipeY = m.keel - 1;
        line(crewX + crewW, pipeY, engineX, pipeY);
        for (let x = crewX + crewW + 4; x < engineX; x += 9) {
            px(x, pipeY - 1);
            px(x, pipeY);
        }
    }

    function render() {
        const variant = VARIANTS[selected];
        const m = metrics(variant);
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        ctx.fillStyle = '#fff';
        ctx.fillRect(0, 0, W, 1);
        ctx.fillRect(0, H - 1, W, 1);
        ctx.fillRect(0, 0, 1, H);
        ctx.fillRect(W - 1, 0, 1, H);

        drawText(variant.name, 4, 3);
        drawTextRight(
            view === 'external' ? 'EXT' : 'CUT',
            124,
            3
        );
        ctx.fillStyle = '#fff';
        ctx.fillRect(4, 11, 46, 1);
        ctx.fillRect(78, 11, 46, 1);

        if (view === 'external')
            drawExternal(variant, m);
        else
            drawCutaway(variant, m);

        updatePanel(variant);
    }

    function updatePanel(variant) {
        if (!debugEl)
            return;
        debugEl.textContent = [
            `${variant.code} — ${variant.name}`,
            `${variant.design} · ${selected + 1}/${VARIANTS.length}`,
            `view ${view}`,
            `hull ${variant.length} px · ${variant.shape}`,
            `hold ${variant.holdW}×${variant.holdH}`,
            `cargo ${variant.load}/${variant.capacity} · ${variant.role}`,
            '',
            '←/→ hull   Z/A view',
        ].join('\n');
    }

    function cycle(dir) {
        selected =
            (selected + dir + VARIANTS.length) %
            VARIANTS.length;
        window.SubHaulerSubs.setSelected(selected);
        render();
    }

    function toggleView() {
        view = view === 'external' ? 'cutaway' : 'external';
        render();
    }

    window.addEventListener('keydown', (event) => {
        if (window.__uiMode && window.__uiMode !== 'fleet')
            return;
        if (
            event.code === 'ArrowLeft' ||
            event.code === 'ArrowRight'
        )
            event.preventDefault();
        if (event.repeat && event.code === 'KeyZ')
            return;
        switch (event.code) {
            case 'ArrowLeft':
                cycle(-1);
                break;
            case 'ArrowRight':
                cycle(1);
                break;
            case 'KeyZ':
                toggleView();
                break;
            default:
                break;
        }
    });

    window.SubHaulerSubs.subscribe((index) => {
        if (index === selected)
            return;
        selected = index;
        render();
    });

    render();
})();
