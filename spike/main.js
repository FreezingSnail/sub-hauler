// Canvas, fixed-step loop, input, and the debug readout.
(function () {
    'use strict';

    const CFG = SubHauler.CFG;
    const W = CFG.width;
    const H = CFG.height;
    const DT = 1 / 60;

    const canvas = document.getElementById('game');
    const ctx = canvas.getContext('2d');
    const overlay = document.getElementById('overlay');
    const overlayText = document.getElementById('overlayText');
    const debugEl = document.getElementById('debug');

    const params = new URLSearchParams(window.location.search);
    let seed = Number(params.get('seed'));
    if (!Number.isFinite(seed) || seed <= 0)
        seed = 1337;
    let crates = Number(params.get('crates'));
    if (!Number.isFinite(crates))
        crates = 0;
    crates = SubHauler.clamp(Math.round(crates), 0, CFG.maxCrates);

    const patternCanvas = document.createElement('canvas');
    patternCanvas.width = 2;
    patternCanvas.height = 2;
    const patternCtx = patternCanvas.getContext('2d');
    patternCtx.fillStyle = '#fff';
    patternCtx.fillRect(0, 0, 1, 1);
    patternCtx.fillRect(1, 1, 1, 1);
    const wallPattern = ctx.createPattern(patternCanvas, 'repeat');

    const keys = new Set();
    let state = SubHauler.createGame(seed, crates);
    let overlayKey = '';

    function readInput() {
        return {
            left: keys.has('ArrowLeft') || keys.has('KeyA'),
            right: keys.has('ArrowRight') || keys.has('KeyD'),
            up: keys.has('ArrowUp') || keys.has('KeyW'),
            down: keys.has('ArrowDown') || keys.has('KeyS'),
        };
    }

    function syncUrl() {
        try {
            const url = new URL(window.location.href);
            url.searchParams.set('seed', String(seed));
            url.searchParams.set('crates', String(crates));
            window.history.replaceState(null, '', url);
        } catch (error) {
            // file:// history updates can be blocked; the game still works.
        }
    }

    function restart() {
        state = SubHauler.createGame(seed, crates);
        overlayKey = '';
        updateDebug();
        syncUrl();
    }

    function setCrates(value) {
        crates = SubHauler.clamp(value, 0, CFG.maxCrates);
        restart();
    }

    function drawSub(sx, bottom, crateCount) {
        const totalHeight = CFG.hullH + CFG.crateH * crateCount;
        const hullTop = bottom - CFG.hullH;
        const cargoTop = bottom - totalHeight;

        if (crateCount > 0) {
            const cargoHeight = CFG.crateH * crateCount;
            ctx.fillStyle = '#fff';
            ctx.fillRect(sx + 2, cargoTop, 20, cargoHeight);
            ctx.fillStyle = '#000';
            for (let i = 1; i < 4; i++)
                ctx.fillRect(sx + 2 + i * 5, cargoTop, 1, cargoHeight);
            for (let j = 1; j < crateCount; j++)
                ctx.fillRect(sx + 2, cargoTop + j * CFG.crateH, 20, 1);
        }

        ctx.fillStyle = '#fff';
        ctx.fillRect(sx, hullTop, 21, 8);
        ctx.fillRect(sx + 21, hullTop + 1, 3, 6);
        ctx.fillRect(sx + 24, hullTop + 2, 2, 4);
        ctx.fillRect(sx - 2, hullTop + 3, 2, 4);

        if (crateCount > 0) {
            ctx.fillStyle = '#000';
            ctx.fillRect(sx + 2, hullTop, 20, 1);
        }

        ctx.fillStyle = '#000';
        ctx.fillRect(sx + 15, hullTop + 3, 2, 2);
        ctx.fillRect(sx + 18, hullTop + 3, 2, 2);
    }

    function drawWalls(cam) {
        const step = CFG.columnStep;
        const first = Math.max(0, Math.floor(cam / step));
        const last = Math.min(
            state.columns.length - 1,
            Math.floor((cam + W) / step)
        );

        ctx.fillStyle = wallPattern;
        for (let i = first; i <= last; i++) {
            const col = state.columns[i];
            const sx = Math.round(col.x - cam);
            if (sx > W || sx + step < 0)
                continue;
            ctx.fillRect(sx, 0, step, col.ceil);
            ctx.fillRect(sx, col.floor, step, H - col.floor);
        }

        ctx.fillStyle = '#fff';
        for (let i = first; i <= last; i++) {
            const col = state.columns[i];
            const sx = Math.round(col.x - cam);
            if (sx > W || sx + step < 0)
                continue;
            if (col.ceil > 0)
                ctx.fillRect(sx, col.ceil - 1, step, 1);
            if (col.floor < H)
                ctx.fillRect(sx, col.floor, step, 1);
        }
    }

    function drawFinish(cam) {
        const sx = Math.round(CFG.routeLength - cam);
        if (sx < -2 || sx > W + 2)
            return;
        ctx.fillStyle = '#fff';
        for (let y = 0; y < H; y += 4)
            ctx.fillRect(sx, y, 1, 2);
    }

    function drawMines(cam) {
        const target = SubHauler.targetsFor(state.crates);
        const subX = state.x + CFG.hullW / 2;
        const subY = state.y - target.totalHeight / 2;
        const nearSq = CFG.nearMine * CFG.nearMine;

        for (const mine of state.mines) {
            if (!mine.alive)
                continue;
            const sx = Math.round(mine.x - cam);
            const sy = Math.round(mine.y);
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
            ctx.fillStyle = '#fff';
            ctx.fillRect(sx - 1, sy, 3, 1);
            ctx.fillRect(sx, sy - 1, 1, 3);
        }
    }

    function drawSonar(cam) {
        if (state.sonar.ringT < 0)
            return;
        const progress = state.sonar.ringT / CFG.sonarDuration;
        const radius = progress * CFG.sonarRadius;
        const cx = state.sonar.originX - cam;
        const cy = state.sonar.originY;
        ctx.fillStyle = '#fff';
        for (let i = 0; i < 40; i++) {
            const angle = (i / 40) * Math.PI * 2;
            const x = Math.round(cx + Math.cos(angle) * radius);
            const y = Math.round(cy + Math.sin(angle) * radius);
            if (x >= 0 && x < W && y >= 0 && y < H)
                ctx.fillRect(x, y, 1, 1);
        }
    }

    function render() {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, W, H);

        const cam = state.cameraX;
        drawWalls(cam);
        drawFinish(cam);
        drawMines(cam);

        drawSub(
            Math.round(state.x - cam),
            Math.round(state.y),
            state.crates
        );

        drawSonar(cam);

        if (state.flash > 0) {
            ctx.fillStyle = '#fff';
            ctx.fillRect(0, 0, W, 1);
            ctx.fillRect(0, H - 1, W, 1);
            ctx.fillRect(0, 0, 1, H);
            ctx.fillRect(W - 1, 0, 1, H);
        }
    }

    function updateDebug() {
        const target = SubHauler.targetsFor(state.crates);
        const lines = [
            `seed ${seed}  cargo ${crates}`,
            `speed ${state.vx.toFixed(1)}  vy ${state.vy.toFixed(1)}`,
            `sink ${target.sink.toFixed(1)}  height ${target.totalHeight}`,
            `hull ${state.hp}/${CFG.startHp}  cargo ${Math.round(state.integrity)}%`,
            `hits ${state.collisions}  mines ${state.mineHits}  pings ${state.sonar.pings}/${CFG.sonarCharges}`,
            `time ${state.time.toFixed(1)}s  dist ${Math.round(state.distance)}/${CFG.routeLength - CFG.startX}`,
        ];
        if (state.over)
            lines.push('status HULL BREACH');
        else if (state.finished)
            lines.push(`status DELIVERED  score ${state.score}`);
        else
            lines.push('status running');
        lines.push(`fps ${fps.toFixed(1)}`);
        debugEl.textContent = lines.join('\n');
    }

    function updateOverlay() {
        const key = state.over ? 'over' : state.finished ? 'finish' : 'running';
        if (key === overlayKey)
            return;
        overlayKey = key;
        if (key === 'running') {
            overlay.classList.add('hidden');
            return;
        }

        const title = state.over ? 'HULL BREACH' : 'DELIVERED';
        const detail = state.over
            ? `<p>distance ${Math.round(state.distance)} / ${CFG.routeLength - CFG.startX} px</p>`
            : `<p>payout $${state.payout} · bonus $${state.bonus} · score $${state.score}</p>`;
        overlayText.innerHTML =
            `<h2>${title}</h2>` +
            `<p>cargo ${state.crates} · integrity ${Math.round(state.integrity)}%</p>` +
            `<p>time ${state.time.toFixed(1)}s · hits ${state.collisions} · pings ${state.sonar.pings}</p>` +
            detail +
            '<p class="hint">R retry · N new cave · 1 / 2 / 3 cargo</p>';
        overlay.classList.remove('hidden');
    }

    const blocked = new Set([
        'ArrowLeft',
        'ArrowRight',
        'ArrowUp',
        'ArrowDown',
        'Space',
    ]);

    window.addEventListener('keydown', (event) => {
        if (blocked.has(event.code))
            event.preventDefault();
        if (event.repeat)
            return;
        keys.add(event.code);

        switch (event.code) {
            case 'Space':
                SubHauler.ping(state);
                break;
            case 'KeyR':
                restart();
                break;
            case 'KeyN':
                seed = (Math.random() * 1e9) | 0 || 1;
                restart();
                break;
            case 'Digit1':
                setCrates(0);
                break;
            case 'Digit2':
                setCrates(2);
                break;
            case 'Digit3':
                setCrates(4);
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
        while (accumulator >= DT) {
            SubHauler.step(state, readInput(), DT);
            accumulator -= DT;
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

    updateDebug();
    requestAnimationFrame(frame);

    window.subHaulerSpike = {
        state: () => state,
        restart,
        setCrates,
        setSeed: (value) => {
            seed = value >>> 0 || 1;
            restart();
        },
    };
})();
