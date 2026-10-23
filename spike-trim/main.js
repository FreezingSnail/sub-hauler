// Canvas, fixed-step loop, input, particles, and instrument panel.
(function () {
    'use strict';

    const CFG = SubHaulerTrim.CFG;
    const W = CFG.viewW;
    const H = CFG.viewH;
    const T = CFG.tile;
    const DT = 1 / 60;
    const SUBS = window.SubHaulerSubs || {
        list: [
            {
                name: 'MINNOW',
                design: 'TADPOLE',
                pilotW: 26,
                pilotH: 8,
            },
            {
                name: 'HAULER',
                design: 'FLATBED',
                pilotW: 26,
                pilotH: 8,
            },
            {
                name: 'LEVIATHAN',
                design: 'FORTRESS',
                pilotW: 26,
                pilotH: 8,
            },
        ],
        getSelected: () => 1,
        setSelected() {},
        subscribe() {},
    };
    let subIndex = SUBS.getSelected();
    let cargoManifest = {
        packed: 0,
        capacity: SUBS.list[subIndex].capacity || 0,
    };
    const subButtons =
        typeof document.querySelectorAll === 'function'
            ? Array.from(
                  document.querySelectorAll('[data-pilot-sub]')
              )
            : [];

    const canvas = document.getElementById('game');
    const ctx = canvas.getContext('2d');
    const overlay = document.getElementById('overlay');
    const overlayText = document.getElementById('overlayText');
    const hintEl = document.getElementById('hint');
    const debugEl = document.getElementById('debug');

    const params = new URLSearchParams(window.location.search);
    const showWholeMap = params.get('map') === 'all';
    let seed = Number(params.get('seed'));
    if (!Number.isFinite(seed) || seed <= 0)
        seed = 2027;
    let crates = Number(params.get('crates'));
    if (!Number.isFinite(crates))
        crates = 0;
    crates = SubHaulerTrim.clamp(
        Math.round(crates),
        0,
        CFG.maxCrates
    );
    let startCredits = Number(params.get('credits'));
    if (!Number.isFinite(startCredits))
        startCredits = 0;
    startCredits = SubHaulerTrim.clamp(
        Math.round(startCredits),
        0,
        99999
    );

    const keys = new Set();
    let state = SubHaulerTrim.createGame(
        seed,
        crates,
        startCredits
    );
    let overlayKey = '';
    let mapOpen = showWholeMap;
    let mapRevealAll = showWholeMap;
    let mapWater = null;
    let chartSaveTimer = 0;
    let paused = false;
    let stationIndex = 0;
    let shopOpen = false;
    let shopIndex = 0;
    let shopNote = '';
    let shopNoteUntil = 0;
    const SHOP_ITEMS = SubHaulerTrim.SHOP_ITEMS;

    const silt = [];
    const wash = [];
    const bubbles = [];
    let washTimer = 0;
    let bubbleTimer = 0;
    let blockedTimer = 0;

    function syncUrl() {
        try {
            const url = new URL(window.location.href);
            url.searchParams.set('seed', String(seed));
            url.searchParams.set('crates', String(crates));
            if (startCredits > 0)
                url.searchParams.set(
                    'credits',
                    String(startCredits)
                );
            else
                url.searchParams.delete('credits');
            window.history.replaceState(null, '', url);
        } catch (error) {
            // file:// history updates can be blocked; the game still works.
        }
    }

    const CHART_KEY = 'subHaulerTrimChart:v2:';

    function loadStoredChart(seedValue) {
        try {
            const raw = window.localStorage.getItem(
                CHART_KEY + seedValue
            );
            if (!raw)
                return null;
            const binary = window.atob(raw);
            const bytes = new Uint8Array(binary.length);
            for (let i = 0; i < binary.length; i++)
                bytes[i] = binary.charCodeAt(i);
            const explored = new Uint8Array(
                CFG.mapW * CFG.mapH
            );
            for (let i = 0; i < explored.length; i++)
                explored[i] = (bytes[i >> 3] >> (i & 7)) & 1;
            return explored;
        } catch (error) {
            return null;
        }
    }

    function saveStoredChart(seedValue, explored) {
        try {
            const bytes = new Uint8Array(
                Math.ceil(explored.length / 8)
            );
            for (let i = 0; i < explored.length; i++) {
                if (explored[i])
                    bytes[i >> 3] |= 1 << (i & 7);
            }
            let binary = '';
            for (let i = 0; i < bytes.length; i++)
                binary += String.fromCharCode(bytes[i]);
            window.localStorage.setItem(
                CHART_KEY + seedValue,
                window.btoa(binary)
            );
        } catch (error) {
            // Storage is optional; session persistence still works.
        }
    }

    function revealCurrentChart() {
        SubHaulerTrim.revealMap(state, CFG.mapRevealRadius);
    }

    function settlementName(id) {
        const settlement = state.settlements[id];
        return settlement ? settlement.name : '?';
    }

    function openCargoLoading(job) {
        if (!job)
            return false;
        if (
            !window.cargoSlice ||
            !window.cargoSlice.startJob
        ) {
            state.stationNote = 'LOADER OFFLINE';
            return false;
        }
        window.cargoSlice.startJob({
            crates: job.crates,
            toName: settlementName(job.toId),
            reward: job.reward,
        });
        if (window.subHaulerSetMode)
            window.subHaulerSetMode('cargo');
        return true;
    }

    function updatePilotSubButtons() {
        for (const button of subButtons) {
            button.classList.toggle(
                'active',
                Number(button.dataset.pilotSub) === subIndex
            );
        }
    }

    function selectPilotSub(index, sync = true) {
        const count = SUBS.list.length;
        const previous = subIndex;
        subIndex =
            ((Math.round(index) % count) + count) % count;
        if (previous !== subIndex) {
            cargoManifest = {
                packed: 0,
                capacity:
                    SUBS.list[subIndex].capacity || 0,
            };
            setCratesLive(0);
            crates = 0;
        }
        if (sync)
            SUBS.setSelected(subIndex);
        updatePilotSubButtons();
        updateDebug();
    }

    function updateHint() {
        let text = '';
        if (!state.over && state.docked === null) {
            if (mapOpen) {
                text = 'CHART · M CLOSE';
            } else {
                const near = SubHaulerTrim.nearestDock(state);
                if (near) {
                    const speed = Math.hypot(
                        state.vx,
                        state.vy
                    );
                    text =
                        speed <= CFG.dockSpeed
                            ? `E DOCK · ${near.name}`
                            : `SLOW DOWN · ${near.name}`;
                }
                if (state.activeJob) {
                    const job = state.activeJob;
                    const left = Math.max(
                        0,
                        Math.ceil(job.deadlineAt - state.time)
                    );
                    const late = state.time > job.deadlineAt;
                    const jobText = `JOB ${job.crates} → ${settlementName(
                        job.toId
                    )} · ${late ? 'LATE' : `${left}s`}`;
                    text = text
                        ? `${text}\n${jobText}`
                        : jobText;
                }
            }
        }
        if (text) {
            hintEl.textContent = text;
            hintEl.classList.remove('hidden');
        } else {
            hintEl.textContent = '';
            hintEl.classList.add('hidden');
        }
    }

    function buySelected() {
        const result = SubHaulerTrim.buyShopItem(
            state,
            shopIndex
        );
        shopNoteUntil = performance.now() + 2200;
        if (result.ok) {
            shopNote = `BOUGHT ${result.item.name}`;
        } else if (result.reason === 'CREDITS') {
            shopNote = 'NOT ENOUGH CREDITS';
        } else if (result.reason === 'SOLD') {
            shopNote = 'SOLD OUT';
        } else {
            shopNote = 'DOCK AT A STATION';
        }
        updateDebug();
    }

    function spawnSilt(particle) {
        for (let attempt = 0; attempt < 24; attempt++) {
            const x =
                state.cameraX -
                40 +
                Math.random() * (W + 80);
            const y =
                state.cameraY -
                40 +
                Math.random() * (H + 80);
            if (SubHaulerTrim.solidAt(state.grid, x, y))
                continue;
            particle.x = x;
            particle.y = y;
            particle.drift = 0.5 + Math.random() * 1.5;
            particle.rise = (Math.random() * 2 - 1) * 0.6;
            return;
        }
        particle.x = state.cameraX + W / 2;
        particle.y = state.cameraY + H / 2;
        particle.drift = 1;
        particle.rise = 0;
    }

    function resetParticles() {
        silt.length = 0;
        for (let i = 0; i < 38; i++) {
            const particle = {};
            spawnSilt(particle);
            silt.push(particle);
        }
        wash.length = 0;
        bubbles.length = 0;
    }

    function updateParticles(dt) {
        for (const particle of silt) {
            particle.x +=
                (particle.drift + (state.currentOn ? 7 : 0)) * dt;
            particle.y += particle.rise * dt;
            if (
                SubHaulerTrim.solidAt(
                    state.grid,
                    particle.x,
                    particle.y
                ) ||
                particle.x < state.cameraX - 48 ||
                particle.x > state.cameraX + W + 48 ||
                particle.y < state.cameraY - 48 ||
                particle.y > state.cameraY + H + 48
            )
                spawnSilt(particle);
        }

        for (let i = wash.length - 1; i >= 0; i--) {
            wash[i].age += dt;
            if (wash[i].age > 4)
                wash.splice(i, 1);
        }

        for (let i = bubbles.length - 1; i >= 0; i--) {
            const bubble = bubbles[i];
            bubble.age += dt;
            bubble.y -= (7 + bubble.rise) * dt;
            bubble.x += (state.currentOn ? 6 : 0) * dt;
            if (bubble.age > 2.5)
                bubbles.splice(i, 1);
        }
    }

    function readInput() {
        return {
            left: keys.has('ArrowLeft') || keys.has('KeyA'),
            right: keys.has('ArrowRight') || keys.has('KeyD'),
            up: keys.has('ArrowUp') || keys.has('KeyW'),
            down: keys.has('ArrowDown') || keys.has('KeyS'),
        };
    }

    function restart() {
        const oldSeed = state.seed;
        const oldChart = state.mapExplored;
        saveStoredChart(oldSeed, oldChart);

        crates = 0;
        state = SubHaulerTrim.createGame(
            seed,
            crates,
            startCredits
        );

        if ((seed >>> 0) === oldSeed) {
            state.mapExplored.set(oldChart);
        } else {
            const stored = loadStoredChart(seed);
            if (stored)
                state.mapExplored.set(stored);
        }
        revealCurrentChart();

        mapOpen = false;
        mapWater = null;
        chartSaveTimer = 0;
        accumulator = 0;
        shopOpen = false;
        shopIndex = 0;
        shopNote = '';
        stationIndex = 0;
        overlayKey = '';
        cargoManifest = { packed: 0, capacity: 0 };
        resetParticles();
        updateDebug();
        syncUrl();
        saveStoredChart(seed, state.mapExplored);
    }

    function setCratesLive(value) {
        const next = SubHaulerTrim.clamp(
            Math.round(value),
            0,
            CFG.maxCrates
        );
        if (SubHaulerTrim.setCrates(state, next)) {
            crates = state.crates;
            syncUrl();
            return true;
        }
        blockedTimer = 1;
        return false;
    }

    function setCargoLoad(packed, capacity) {
        const cap = Math.max(
            1,
            Number(capacity) ||
                SUBS.list[subIndex].capacity ||
                CFG.maxCrates
        );
        const cells = Math.max(0, Number(packed) || 0);
        const units = SubHaulerTrim.clamp(
            Math.ceil(cells / (cap / CFG.maxCrates)),
            0,
            CFG.maxCrates
        );
        cargoManifest = { packed: cells, capacity: cap };
        const ok = setCratesLive(units);
        updateDebug();
        return ok;
    }

    function tileHash(tx, ty) {
        let h =
            Math.imul(tx + 1, 374761393) ^
            Math.imul(ty + 1, 668265263);
        h = Math.imul(h ^ (h >>> 13), 1274126177);
        return (h ^ (h >>> 16)) >>> 0;
    }

    function drawCave() {
        const camX = state.cameraX;
        const camY = state.cameraY;
        const firstX = Math.floor(camX / T) - 1;
        const lastX = Math.floor((camX + W) / T) + 1;
        const firstY = Math.floor(camY / T) - 1;
        const lastY = Math.floor((camY + H) / T) + 1;
        ctx.fillStyle = '#fff';

        for (let ty = firstY; ty <= lastY; ty++) {
            for (let tx = firstX; tx <= lastX; tx++) {
                if (
                    !SubHaulerTrim.tileSolid(state.grid, tx, ty)
                )
                    continue;
                const sx = Math.round(tx * T - camX);
                const sy = Math.round(ty * T - camY);
                const top = SubHaulerTrim.tileSolid(
                    state.grid,
                    tx,
                    ty - 1
                );
                const bottom = SubHaulerTrim.tileSolid(
                    state.grid,
                    tx,
                    ty + 1
                );
                const left = SubHaulerTrim.tileSolid(
                    state.grid,
                    tx - 1,
                    ty
                );
                const right = SubHaulerTrim.tileSolid(
                    state.grid,
                    tx + 1,
                    ty
                );

                if (!top)
                    ctx.fillRect(sx, sy, T, 1);
                if (!bottom)
                    ctx.fillRect(sx, sy + T - 1, T, 1);
                if (!left)
                    ctx.fillRect(sx, sy, 1, T);
                if (!right)
                    ctx.fillRect(sx + T - 1, sy, 1, T);

                const hash = tileHash(tx, ty);
                ctx.fillRect(sx + (hash % 8), sy + ((hash >>> 3) % 8), 1, 1);
                ctx.fillRect(sx + ((hash >>> 6) % 8), sy + ((hash >>> 9) % 8), 1, 1);
                ctx.fillRect(sx + ((hash >>> 12) % 8), sy + ((hash >>> 15) % 8), 1, 1);
                ctx.fillRect(sx + ((hash >>> 18) % 8), sy + ((hash >>> 21) % 8), 1, 1);
            }
        }
    }

    function drawSilt() {
        ctx.fillStyle = '#fff';
        for (const particle of silt) {
            const sx = Math.round(particle.x - state.cameraX);
            const sy = Math.round(particle.y - state.cameraY);
            if (sx < 0 || sx >= W || sy < 0 || sy >= H)
                continue;
            ctx.fillRect(sx, sy, 1, 1);
        }
    }

    function drawWash() {
        ctx.fillStyle = '#fff';
        for (const point of wash) {
            if (
                point.age > 2.4 &&
                Math.floor(point.age * 5) % 2 === 1
            )
                continue;
            const sx = Math.round(point.x - state.cameraX);
            const sy = Math.round(point.y - state.cameraY);
            if (sx < 0 || sx >= W || sy < 0 || sy >= H)
                continue;
            ctx.fillRect(sx, sy, 1, 1);
        }
        for (const bubble of bubbles) {
            const sx = Math.round(bubble.x - state.cameraX);
            const sy = Math.round(bubble.y - state.cameraY);
            if (sx < 0 || sx >= W || sy < 0 || sy >= H)
                continue;
            ctx.fillRect(sx, sy, 1, 1);
        }
    }

    function drawSettlements() {
        const blink = Math.floor(performance.now() / 380) % 4;
        for (const settlement of state.settlements) {
            const sx = Math.round(
                settlement.dockX - state.cameraX
            );
            const sy = Math.round(
                settlement.dockY - state.cameraY
            );
            if (
                sx < -40 ||
                sx > W + 40 ||
                sy < -40 ||
                sy > H + 40
            )
                continue;
            const destination =
                state.activeJob &&
                state.activeJob.toId === settlement.id;
            ctx.fillStyle = '#fff';
            for (let y = sy - 18; y < sy + 9; y += 4) {
                ctx.fillRect(sx - 15, y, 1, 2);
                ctx.fillRect(sx + 14, y, 1, 2);
            }
            for (let x = -15; x < 15; x += 6)
                ctx.fillRect(sx + x, sy - 18, 3, 1);
            ctx.fillRect(sx - 15, sy + 9, 30, 1);
            ctx.fillRect(sx - 13, sy + 10, 26, 1);
            ctx.fillRect(sx - 15, sy + 11, 1, 2);
            ctx.fillRect(sx + 14, sy + 11, 1, 2);
            ctx.fillRect(sx - 12, sy - 4, 1, 1);
            ctx.fillRect(sx + 11, sy - 4, 1, 1);
            if (blink === 0 || destination)
                ctx.fillRect(sx, sy - 21, 1, 3);
        }
    }

    function drawMines() {
        const height = SubHaulerTrim.totalHeightFor(state.crates);
        const subX = state.x + CFG.hullW / 2;
        const subY = state.y - height / 2;
        const nearSq = CFG.nearMine * CFG.nearMine;

        for (const mine of state.mines) {
            if (!mine.alive)
                continue;
            const sx = Math.round(mine.x - state.cameraX);
            const sy = Math.round(mine.y - state.cameraY);
            if (sx < -6 || sx > W + 6 || sy < -6 || sy > H + 6)
                continue;
            const dx = mine.x - subX;
            const dy = mine.y - subY;
            const near = dx * dx + dy * dy < nearSq;
            const revealed = mine.revealedUntil > state.time;
            if (!near && !revealed) {
                ctx.fillStyle = '#fff';
                ctx.fillRect(sx, sy, 1, 1);
                continue;
            }
            ctx.fillStyle = '#000';
            ctx.fillRect(sx - 2, sy - 2, 5, 5);
            ctx.fillStyle = '#fff';
            ctx.fillRect(sx - 1, sy, 3, 1);
            ctx.fillRect(sx, sy - 1, 1, 3);
        }
    }

    function drawSonar() {
        if (state.sonar.ringT < 0)
            return;
        const progress = state.sonar.ringT / CFG.sonarCooldown;
        const radius = progress * CFG.sonarRadius;
        const cx = state.sonar.originX - state.cameraX;
        const cy = state.sonar.originY - state.cameraY;
        ctx.fillStyle = '#fff';
        for (let i = 0; i < 40; i++) {
            const angle = (i / 40) * Math.PI * 2;
            const x = Math.round(cx + Math.cos(angle) * radius);
            const y = Math.round(cy + Math.sin(angle) * radius);
            if (x >= 0 && x < W && y >= 0 && y < H)
                ctx.fillRect(x, y, 1, 1);
        }
    }

    function drawPilotBody(x, y, w, h, radius) {
        ctx.fillStyle = '#fff';
        for (let row = 0; row < h; row++) {
            let inset = 0;
            if (row < radius)
                inset = radius - row;
            else if (row >= h - radius)
                inset = row - (h - radius - 1);
            if (inset * 2 >= w)
                continue;
            ctx.fillRect(x + inset, y + row, w - inset * 2, 1);
        }
    }

    function drawPilotCargoDoor(x, y, w, h, loaded) {
        ctx.fillStyle = '#000';
        ctx.fillRect(x, y, w, h);
        ctx.fillStyle = '#fff';
        ctx.fillRect(x + 1, y + 1, w - 2, 1);
        ctx.fillRect(
            x + 1,
            y + h - 2,
            w - 2,
            1
        );
        if (loaded) {
            ctx.fillStyle = '#fff';
            ctx.fillRect(x + Math.floor(w / 2), y + 2, 1, h - 4);
        }
    }

    function drawSub() {
        const sx = Math.round(state.x - state.cameraX);
        const bottom = Math.round(state.y - state.cameraY);
        const sub = SUBS.list[subIndex];
        const w = Math.min(CFG.hullW, sub.pilotW || CFG.hullW);
        const h = Math.min(CFG.hullH, sub.pilotH || CFG.hullH);
        const left = sx + Math.floor((CFG.hullW - w) / 2);
        const top = bottom - h;
        const design = sub.design;
        const loaded = state.crates > 0;

        drawPilotBody(left, top, w, h, 3);

        ctx.fillStyle = '#fff';
        if (design === 'TADPOLE') {
            // Shuttle sail and landing skids.
            ctx.fillRect(left + 8, top - 3, 6, 3);
            ctx.fillRect(left + 12, top - 5, 1, 2);
            ctx.fillRect(left + 3, bottom, 4, 1);
            ctx.fillRect(left + 15, bottom, 4, 1);
        } else if (design === 'FLATBED') {
            // Deckhouse and cargo doors.
            ctx.fillRect(left + 14, top - 3, 8, 3);
            ctx.fillRect(left + 16, top - 5, 1, 2);
            drawPilotCargoDoor(
                left + 4,
                top + 2,
                5,
                4,
                loaded
            );
            drawPilotCargoDoor(
                left + 20,
                top + 2,
                4,
                4,
                loaded
            );
        } else {
            // Fortress deckhouse, gantry, and outrigger pods.
            ctx.fillRect(left + 9, top - 3, 9, 3);
            ctx.fillRect(left + 11, top - 6, 1, 3);
            ctx.fillRect(left + 23, top - 5, 1, 5);
            ctx.fillRect(left + 22, top - 5, 3, 1);
            ctx.fillRect(left + 2, bottom, 5, 2);
            ctx.fillRect(left + 18, bottom, 5, 2);
            drawPilotCargoDoor(
                left + 4,
                top + 2,
                6,
                4,
                loaded
            );
            drawPilotCargoDoor(
                left + 18,
                top + 2,
                5,
                4,
                loaded
            );
        }

        if (design === 'TADPOLE') {
            drawPilotCargoDoor(
                left + 15,
                top + 2,
                4,
                3,
                loaded
            );
        }

        ctx.fillStyle = '#000';
        if (design === 'TADPOLE') {
            ctx.fillRect(left + 10, top - 2, 2, 1);
            ctx.fillRect(left + 3, top + 2, 3, 2);
            ctx.fillRect(left + 13, top + 2, 1, 2);
        } else if (design === 'FLATBED') {
            ctx.fillRect(left + 16, top - 2, 2, 1);
            ctx.fillRect(left + 18, top - 2, 2, 1);
            ctx.fillRect(left + 3, top + 3, 1, 1);
            ctx.fillRect(left + 12, top + 3, 1, 1);
        } else {
            ctx.fillRect(left + 11, top - 2, 2, 1);
            ctx.fillRect(left + 14, top - 2, 2, 1);
            ctx.fillRect(left + 6, top + 3, 2, 2);
            ctx.fillRect(left + 17, top + 3, 2, 2);
        }
    }

    function drawHud() {
        const neutral = SubHaulerTrim.neutralFor(state.crates);

        // Air bar, top left. A dark track with a white fill and baseline.
        const ax = 3;
        const ay = 3;
        const aw = 20;
        ctx.fillStyle = '#000';
        ctx.fillRect(ax - 1, ay - 1, aw + 2, 6);
        ctx.fillStyle = '#fff';
        ctx.fillRect(ax, ay + 3, aw, 1);
        const airWidth = Math.round(
            aw *
                SubHaulerTrim.clamp(
                    state.air / state.airMax,
                    0,
                    1
                )
        );
        if (airWidth > 0)
            ctx.fillRect(ax, ay, airWidth, 2);

        // Ballast gauge, left edge. Fill grows from the bottom.
        const gx = 3;
        const gy = 13;
        const gw = 7;
        const gh = 32;
        ctx.fillStyle = '#000';
        ctx.fillRect(gx - 1, gy - 1, gw + 6, gh + 2);
        ctx.fillStyle = '#fff';
        ctx.fillRect(gx, gy + gh - 1, gw, 1);

        const innerHeight = gh - 2;
        const fillHeight = Math.round(
            innerHeight *
                SubHaulerTrim.clamp(state.ballast / 100, 0, 1)
        );
        if (fillHeight > 0)
            ctx.fillRect(
                gx + 1,
                gy + gh - 1 - fillHeight,
                2,
                fillHeight
            );

        const notchY =
            gy +
            gh -
            1 -
            Math.round(
                innerHeight *
                    SubHaulerTrim.clamp(neutral / 100, 0, 1)
            );
        ctx.fillStyle = '#fff';
        ctx.fillRect(gx + gw, notchY, 3, 1);

        // Hull pips, top right.
        ctx.fillStyle = '#000';
        ctx.fillRect(95, 2, 31, 5);
        const pips = Math.min(state.maxHp, 9);
        for (let i = 0; i < pips; i++) {
            ctx.fillStyle = i < state.hp ? '#fff' : '#000';
            ctx.fillRect(97 + i * 3, 3, 2, 2);
        }
    }

    const SHOP_FONT = {
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

    function drawShopText(text, x, y, color) {
        const value = String(text).toUpperCase();
        let cursor = x;
        ctx.fillStyle = color === 0 ? '#000' : '#fff';
        for (let i = 0; i < value.length; i++) {
            const glyph =
                SHOP_FONT[value[i]] || SHOP_FONT['?'];
            for (let row = 0; row < 7; row++) {
                const bits = glyph[row];
                for (let col = 0; col < 5; col++) {
                    if (bits[col] === '1')
                        ctx.fillRect(
                            cursor + col,
                            y + row,
                            1,
                            1
                        );
                }
            }
            cursor += 6;
        }
    }

    function shopTextWidth(text) {
        return String(text).length * 6 - 1;
    }

    function drawShopTextRight(text, right, y, color) {
        drawShopText(
            text,
            right - shopTextWidth(text),
            y,
            color
        );
    }

    function drawShop() {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        ctx.fillStyle = '#fff';
        ctx.fillRect(0, 0, W, 1);
        ctx.fillRect(0, H - 1, W, 1);
        ctx.fillRect(0, 0, 1, H);
        ctx.fillRect(W - 1, 0, 1, H);

        drawShopText('SHOP', 6, 3, 1);
        drawShopTextRight(`$${state.credits}`, 122, 3, 1);
        ctx.fillStyle = '#fff';
        ctx.fillRect(4, 11, W - 8, 1);

        for (let i = 0; i < SHOP_ITEMS.length; i++) {
            const item = SHOP_ITEMS[i];
            const y = 14 + i * 9;
            const selected = i === shopIndex;
            if (selected) {
                ctx.fillStyle = '#fff';
                ctx.fillRect(4, y - 1, W - 8, 9);
            }
            const color = selected ? 0 : 1;
            drawShopText(item.name, 7, y, color);
            const level = state[item.levelKey] || 0;
            if (level >= item.max) {
                drawShopTextRight('SOLD', 122, y, color);
            } else {
                drawShopTextRight(
                    `${level}/${item.max}`,
                    91,
                    y,
                    color
                );
                drawShopTextRight(
                    `$${item.price}`,
                    122,
                    y,
                    color
                );
            }
        }

        const item = SHOP_ITEMS[shopIndex];
        const note =
            shopNote &&
            performance.now() < shopNoteUntil
                ? shopNote
                : item
                  ? item.desc
                  : '';
        drawShopText(note, 6, 45, 1);
        drawShopText('UP/DN  A BUY  B BACK', 4, 55, 1);
    }

    function settlementCode(id) {
        const settlement = state.settlements[id];
        if (!settlement)
            return '?';
        return settlement.name.split(' ')[0].slice(0, 4);
    }

    function stationRows() {
        if (state.docked === null)
            return [];
        if (state.activeJob) {
            return [
                {
                    kind: 'load',
                    label:
                        state.crates >= state.activeJob.crates
                            ? 'RELOAD CARGO'
                            : 'LOAD CARGO',
                },
                { kind: 'shop', label: 'SHOP' },
                { kind: 'abandon', label: 'ABANDON' },
                { kind: 'undock', label: 'UNDOCK' },
            ];
        }
        const offers = state.jobs[state.docked] || [];
        const rows = offers.map((offer, index) => ({
            kind: 'job',
            index,
            offer,
        }));
        rows.push({ kind: 'shop', label: 'SHOP' });
        rows.push({ kind: 'undock', label: 'UNDOCK' });
        return rows;
    }

    function activateStationRow() {
        const rows = stationRows();
        const row = rows[stationIndex];
        if (!row)
            return;
        if (row.kind === 'job') {
            if (SubHaulerTrim.acceptJob(state, row.index)) {
                stationIndex = 0;
                openCargoLoading(state.activeJob);
            }
        } else if (row.kind === 'load') {
            openCargoLoading(state.activeJob);
        } else if (row.kind === 'shop') {
            shopOpen = true;
            shopIndex = 0;
            shopNote = '';
        } else if (row.kind === 'abandon') {
            SubHaulerTrim.abandonJob(state);
            stationIndex = 0;
        } else if (row.kind === 'undock') {
            SubHaulerTrim.undock(state);
            stationIndex = 0;
        }
        updateDebug();
    }

    function drawStation() {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        ctx.fillStyle = '#fff';
        ctx.fillRect(0, 0, W, 1);
        ctx.fillRect(0, H - 1, W, 1);
        ctx.fillRect(0, 0, 1, H);
        ctx.fillRect(W - 1, 0, 1, H);

        const settlement = state.settlements[state.docked];
        const job = state.activeJob;
        drawShopText(settlement.name, 6, 3, 1);
        drawShopTextRight(`$${state.credits}`, 122, 3, 1);

        let rowStart;
        if (job) {
            const left = Math.max(
                0,
                Math.ceil(job.deadlineAt - state.time)
            );
            const late = state.time > job.deadlineAt;
            drawShopText(
                `JOB ${job.crates}CR>${settlementCode(
                    job.toId
                )} $${job.reward}`,
                4,
                11,
                1
            );
            drawShopText(
                late ? 'LATE' : `TIME ${left}S`,
                4,
                19,
                1
            );
            ctx.fillStyle = '#fff';
            ctx.fillRect(4, 27, W - 8, 1);
            rowStart = 29;
        } else {
            const status =
                state.stationNote ||
                `AIR ${Math.round(state.air)}/${
                    state.airMax
                } HULL ${state.hp}/${state.maxHp}`;
            drawShopText(status, 4, 11, 1);
            ctx.fillStyle = '#fff';
            ctx.fillRect(4, 19, W - 8, 1);
            rowStart = 21;
        }

        const rows = stationRows();
        const rowGap = 8;
        if (rows.length)
            stationIndex = ((stationIndex % rows.length) + rows.length) %
                rows.length;
        for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            const y = rowStart + i * rowGap;
            const selected = i === stationIndex;
            if (selected) {
                ctx.fillStyle = '#fff';
                ctx.fillRect(4, y - 1, W - 8, rowGap);
            }
            const color = selected ? 0 : 1;
            let label = row.label;
            if (row.kind === 'job') {
                const offer = row.offer;
                label = `${offer.crates}CR>${settlementCode(
                    offer.toId
                )} $${offer.reward} ${offer.deadline}S`;
            }
            drawShopText(label, 7, y, color);
        }

        if (!job)
            drawShopText(
                'UP/DN A SEL B UNDOCK',
                4,
                55,
                1
            );
    }

    function buildMapWater() {
        const cells = new Uint8Array(CFG.mapW * CFG.mapH);
        const cellW = CFG.worldW / CFG.mapW;
        const cellH = CFG.worldH / CFG.mapH;
        for (let cy = 0; cy < CFG.mapH; cy++) {
            for (let cx = 0; cx < CFG.mapW; cx++) {
                const x0 = Math.floor((cx * cellW) / T);
                const x1 = Math.floor(
                    ((cx + 1) * cellW - 0.001) / T
                );
                const y0 = Math.floor((cy * cellH) / T);
                const y1 = Math.floor(
                    ((cy + 1) * cellH - 0.001) / T
                );
                let water = false;
                for (let ty = y0; ty <= y1 && !water; ty++) {
                    for (let tx = x0; tx <= x1; tx++) {
                        if (
                            !SubHaulerTrim.tileSolid(
                                state.grid,
                                tx,
                                ty
                            )
                        ) {
                            water = true;
                            break;
                        }
                    }
                }
                cells[cy * CFG.mapW + cx] = water ? 1 : 0;
            }
        }
        return cells;
    }

    function drawMap() {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        const water = mapWater || (mapWater = buildMapWater());

        ctx.fillStyle = '#fff';
        for (let cy = 0; cy < CFG.mapH; cy++) {
            for (let cx = 0; cx < CFG.mapW; cx++) {
                const index = cy * CFG.mapW + cx;
                if (!water[index])
                    continue;
                const px = cx * 2;
                const py = cy * 2;
                if (state.mapExplored[index]) {
                    ctx.fillRect(px, py, 2, 2);
                } else if (mapRevealAll) {
                    ctx.fillRect(px, py, 1, 1);
                    ctx.fillRect(px + 1, py + 1, 1, 1);
                }
            }
        }

        const cellW = CFG.worldW / CFG.mapW;
        const cellH = CFG.worldH / CFG.mapH;
        for (const settlement of state.settlements) {
            const mx = SubHaulerTrim.clamp(
                Math.floor(settlement.dockX / cellW),
                0,
                CFG.mapW - 1
            );
            const my = SubHaulerTrim.clamp(
                Math.floor(settlement.dockY / cellH),
                0,
                CFG.mapH - 1
            );
            const destination =
                state.activeJob &&
                state.activeJob.toId === settlement.id;
            if (
                destination &&
                Math.floor(performance.now() / 350) % 2 === 1
            )
                continue;
            ctx.fillStyle = '#000';
            ctx.fillRect(mx - 1, my - 1, 3, 3);
            ctx.fillStyle = '#fff';
            ctx.fillRect(mx, my - 1, 1, 3);
            ctx.fillRect(mx - 1, my, 3, 1);
        }

        const cell = SubHaulerTrim.mapCellFor(state);
        const px = cell.x * 2;
        const py = cell.y * 2;
        ctx.fillStyle = '#000';
        ctx.fillRect(px - 1, py - 1, 4, 4);
        ctx.fillStyle = '#fff';
        if (Math.floor(performance.now() / 300) % 2 === 0)
            ctx.fillRect(px, py, 2, 2);
        else
            ctx.fillRect(px, py, 1, 1);

        ctx.fillStyle = '#fff';
        ctx.fillRect(0, 0, W, 1);
        ctx.fillRect(0, H - 1, W, 1);
        ctx.fillRect(0, 0, 1, H);
        ctx.fillRect(W - 1, 0, 1, H);
    }

    function render() {
        if (shopOpen && state.docked !== null) {
            drawShop();
            return;
        }
        if (state.docked !== null) {
            drawStation();
            return;
        }
        if (mapOpen) {
            drawMap();
            return;
        }
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);
        drawCave();
        drawSilt();
        drawWash();
        drawSettlements();
        drawMines();
        drawSonar();
        drawSub();
        drawHud();

        if (state.flash > 0) {
            ctx.fillStyle = '#fff';
            ctx.fillRect(0, 0, W, 1);
            ctx.fillRect(0, H - 1, W, 1);
            ctx.fillRect(0, 0, 1, H);
            ctx.fillRect(W - 1, 0, 1, H);
        }
    }

    function updateDebug() {
        if (state.crates === 0 && cargoManifest.packed > 0) {
            cargoManifest = {
                packed: 0,
                capacity:
                    SUBS.list[subIndex].capacity || 0,
            };
        }
        const target = SubHaulerTrim.targetsFor(state.crates);
        const trim = state.ballast - target.neutral;
        const trimLabel =
            trim > 0.5 ? 'sink' : trim < -0.5 ? 'rise' : 'level';
        const depth = Math.round(state.y * 0.4);
        const near = SubHaulerTrim.nearestDock(state);
        const lines = [
            `hull ${SUBS.list[subIndex].name} · ${SUBS.list[subIndex].design}`,
            `seed ${seed}  cargo ${state.crates}`,
            `hold ${cargoManifest.packed}/${cargoManifest.capacity} cells`,
            `depth ${depth} m  pos ${Math.round(state.x)},${Math.round(state.y)}`,
            `vx ${state.vx.toFixed(1)}  vy ${state.vy.toFixed(1)}`,
            `ballast ${state.ballast.toFixed(0)}%  neutral ${target.neutral}%`,
            `trim ${trim > 0 ? '+' : ''}${trim.toFixed(0)} ${trimLabel}`,
            `air ${state.air.toFixed(1)}/${state.airMax}  pings ${state.sonar.pings}`,
            `hull ${state.hp}/${state.maxHp}  cargo ${Math.round(state.integrity)}%`,
            `credits $${state.credits}  deliveries ${state.deliveries}`,
            `upgrades air ${state.airLevel} hull ${state.hullLevel} sonar ${state.sonarLevel}`,
            `current ${state.currentOn ? 'on' : 'off'}  hits ${state.collisions}  mines ${state.mineHits}`,
        ];
        if (state.activeJob) {
            const job = state.activeJob;
            const left = Math.max(
                0,
                Math.ceil(job.deadlineAt - state.time)
            );
            const late = state.time > job.deadlineAt;
            lines.push(
                `job ${job.crates} -> ${settlementName(
                    job.toId
                )} $${job.reward} ${
                    late ? 'LATE' : `${left}s`
                }`
            );
        }
        if (state.docked !== null) {
            lines.push(
                `docked ${state.settlements[state.docked].name} — E undock`
            );
        } else if (near) {
            const speed = Math.hypot(state.vx, state.vy);
            lines.push(
                `dock ${near.name} — ${
                    speed <= CFG.dockSpeed
                        ? 'E dock'
                        : 'slow to dock'
                }`
            );
        }
        if (blockedTimer > 0)
            lines.push('cargo change blocked: no clearance');
        if (mapRevealAll)
            lines.push('map debug: reveal all (G)');
        if (shopOpen)
            lines.push('shop open — Z buy / X back');
        if (mapOpen)
            lines.push('chart open — M close');
        if (state.over)
            lines.push('status HULL BREACH');
        else if (shopOpen)
            lines.push('status shop');
        else if (state.docked !== null)
            lines.push('status docked');
        else if (state.activeJob)
            lines.push('status contract');
        else
            lines.push('status free run');
        lines.push(`fps ${fps.toFixed(1)}`);
        debugEl.textContent = lines.join('\n');
        updateHint();
    }

    function updateOverlay() {
        const key = state.over ? 'over' : 'running';
        if (key === overlayKey)
            return;
        overlayKey = key;
        if (key === 'running') {
            overlay.classList.add('hidden');
            return;
        }
        overlayText.innerHTML =
            '<h2>HULL BREACH</h2>' +
            `<p>depth ${Math.round(state.y * 0.4)} m · cargo ${state.crates} · integrity ${Math.round(state.integrity)}%</p>` +
            `<p>air ${state.air.toFixed(1)} · pings ${state.sonar.pings} · hits ${state.collisions}</p>` +
            '<p class="hint">R reset · N new cave · M chart · C current</p>';
        overlay.classList.remove('hidden');
        updateHint();
    }

    const blocked = new Set([
        'ArrowLeft',
        'ArrowRight',
        'ArrowUp',
        'ArrowDown',
        'Space',
    ]);

    window.addEventListener('keydown', (event) => {
        if (window.__uiMode && window.__uiMode !== 'pilot')
            return;
        if (blocked.has(event.code))
            event.preventDefault();
        if (event.repeat)
            return;
        keys.add(event.code);

        if (shopOpen) {
            if (state.docked === null) {
                shopOpen = false;
                return;
            }
            switch (event.code) {
                case 'ArrowUp':
                    shopIndex =
                        (shopIndex +
                            SHOP_ITEMS.length -
                            1) %
                        SHOP_ITEMS.length;
                    shopNote = '';
                    break;
                case 'ArrowDown':
                    shopIndex =
                        (shopIndex + 1) %
                        SHOP_ITEMS.length;
                    shopNote = '';
                    break;
                case 'KeyZ':
                    buySelected();
                    break;
                case 'KeyX':
                case 'Escape':
                    shopOpen = false;
                    shopNote = '';
                    stationIndex = 0;
                    break;
                default:
                    break;
            }
            return;
        }

        if (state.docked !== null) {
            const rows = stationRows();
            if (!rows.length)
                return;
            switch (event.code) {
                case 'ArrowUp':
                    stationIndex =
                        (stationIndex +
                            rows.length -
                            1) %
                        rows.length;
                    break;
                case 'ArrowDown':
                    stationIndex =
                        (stationIndex + 1) %
                        rows.length;
                    break;
                case 'KeyZ':
                    activateStationRow();
                    break;
                case 'KeyX':
                case 'Escape':
                case 'KeyE':
                    SubHaulerTrim.undock(state);
                    stationIndex = 0;
                    break;
                case 'KeyR':
                    restart();
                    break;
                case 'KeyN':
                    seed = (Math.random() * 1e9) | 0 || 1;
                    restart();
                    break;
                default:
                    break;
            }
            return;
        }

        switch (event.code) {
            case 'Space':
                if (!mapOpen)
                    SubHaulerTrim.ping(state);
                break;
            case 'KeyE':
                if (
                    !mapOpen &&
                    !state.over &&
                    SubHaulerTrim.dock(state)
                )
                    stationIndex = 0;
                break;
            case 'KeyM':
                if (!state.over) {
                    mapOpen = !mapOpen;
                    accumulator = 0;
                    if (mapOpen)
                        saveStoredChart(
                            seed,
                            state.mapExplored
                        );
                }
                break;
            case 'KeyG':
                if (!state.over) {
                    mapRevealAll = !mapRevealAll;
                    mapOpen = true;
                    accumulator = 0;
                }
                break;
            case 'Escape':
                mapOpen = false;
                break;
            case 'KeyR':
                restart();
                break;
            case 'KeyN':
                seed = (Math.random() * 1e9) | 0 || 1;
                restart();
                break;
            case 'KeyC':
                if (!mapOpen)
                    SubHaulerTrim.toggleCurrent(state);
                break;
            case 'Digit1':
                if (!mapOpen)
                    selectPilotSub(0);
                break;
            case 'Digit2':
                if (!mapOpen)
                    selectPilotSub(1);
                break;
            case 'Digit3':
                if (!mapOpen)
                    selectPilotSub(2);
                break;
            default:
                break;
        }
    });

    window.addEventListener('keyup', (event) => {
        keys.delete(event.code);
    });

    window.addEventListener('blur', () => {
        keys.clear();
    });

    let last = performance.now();
    let accumulator = 0;
    let fps = 60;
    let debugTimer = 0;

    function frame(now) {
        const raw = Math.min((now - last) / 1000, 0.25);
        last = now;
        if (raw > 0)
            fps = fps * 0.9 + (1 / raw) * 0.1;

        accumulator += raw;
        if (!mapOpen && !paused && state.docked === null) {
            while (accumulator >= DT) {
                const input = readInput();
                SubHaulerTrim.step(state, input, DT);

                const totalHeight = SubHaulerTrim.totalHeightFor(
                    state.crates
                );
                washTimer -= DT;
                if (
                    washTimer <= 0 &&
                    (Math.abs(state.vx) > 1 ||
                        Math.abs(state.vy) > 1)
                ) {
                    wash.push({
                        x:
                            state.x +
                            (state.vx >= 0 ? -1 : CFG.hullW + 1),
                        y: state.y - totalHeight / 2,
                        age: 0,
                    });
                    washTimer = 0.12;
                }

                bubbleTimer -= DT;
                if (
                    bubbleTimer <= 0 &&
                    (input.left || input.right)
                ) {
                    bubbles.push({
                        x:
                            state.x +
                            (input.right ? -2 : CFG.hullW + 2),
                        y: state.y - CFG.hullH / 2,
                        rise: Math.random() * 3,
                        age: 0,
                    });
                    bubbleTimer = 0.09;
                }

                updateParticles(DT);
                blockedTimer = Math.max(0, blockedTimer - DT);
                accumulator -= DT;
            }
        } else {
            accumulator = 0;
        }

        chartSaveTimer -= raw;
        if (chartSaveTimer <= 0) {
            saveStoredChart(seed, state.mapExplored);
            chartSaveTimer = 5;
        }

        render();
        updateOverlay();

        debugTimer -= raw;
        if (debugTimer <= 0) {
            updateDebug();
            debugTimer = 0.1;
        }

        requestAnimationFrame(frame);
    }

    const storedChart = loadStoredChart(seed);
    if (storedChart)
        state.mapExplored.set(storedChart);
    revealCurrentChart();

    resetParticles();
    for (const button of subButtons) {
        button.addEventListener('click', () => {
            selectPilotSub(Number(button.dataset.pilotSub));
        });
    }
    SUBS.subscribe((index) => {
        if (index === subIndex)
            return;
        selectPilotSub(index, false);
    });
    selectPilotSub(subIndex, false);
    updateHint();
    requestAnimationFrame(frame);

    window.addEventListener('beforeunload', () => {
        saveStoredChart(seed, state.mapExplored);
    });

    window.subHaulerTrim = {
        state: () => state,
        restart,
        selectSub: (index) => selectPilotSub(index),
        setCrates: setCratesLive,
        setCargoLoad,
        setSeed: (value) => {
            seed = value >>> 0 || 1;
            restart();
        },
        dock: () => {
            const ok = SubHaulerTrim.dock(state);
            if (ok)
                stationIndex = 0;
            return ok;
        },
        undock: () => {
            shopOpen = false;
            shopNote = '';
            stationIndex = 0;
            const ok = SubHaulerTrim.undock(state);
            return ok;
        },
        acceptJob: (index) => {
            const ok = SubHaulerTrim.acceptJob(state, index);
            if (ok)
                stationIndex = 0;
            return ok;
        },
        abandonJob: () => {
            const ok = SubHaulerTrim.abandonJob(state);
            if (ok)
                stationIndex = 0;
            return ok;
        },
        nearestDock: () => {
            const settlement = SubHaulerTrim.nearestDock(state);
            return settlement ? settlement.name : null;
        },
        toggleMap: () => {
            if (!state.over) {
                mapOpen = !mapOpen;
                accumulator = 0;
            }
            return mapOpen;
        },
        toggleMapAll: () => {
            mapRevealAll = !mapRevealAll;
            mapOpen = true;
            accumulator = 0;
            return mapRevealAll;
        },
        mapOpen: () => mapOpen,
        mapRevealAll: () => mapRevealAll,
        setPaused: (value) => {
            paused = !!value;
            if (paused)
                keys.clear();
            accumulator = 0;
            return paused;
        },
        paused: () => paused,
        openShop: () => {
            if (state.docked !== null) {
                shopOpen = true;
                shopIndex = 0;
                shopNote = '';
            }
            return shopOpen;
        },
        closeShop: () => {
            shopOpen = false;
            shopNote = '';
            stationIndex = 0;
            return shopOpen;
        },
        selectShop: (index) => {
            shopIndex =
                ((Math.round(index) % SHOP_ITEMS.length) +
                    SHOP_ITEMS.length) %
                SHOP_ITEMS.length;
            shopNote = '';
            return shopIndex;
        },
        buyShop: () => {
            buySelected();
            return shopNote;
        },
        shopOpen: () => shopOpen,
        shopIndex: () => shopIndex,
        stationIndex: () => stationIndex,
        selectStation: (index) => {
            const rows = stationRows();
            if (!rows.length)
                return 0;
            stationIndex =
                ((Math.round(index) % rows.length) +
                    rows.length) %
                rows.length;
            return stationIndex;
        },
        activateStation: () => {
            activateStationRow();
            return stationIndex;
        },
    };
})();
