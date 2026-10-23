// Sub-hauler trim sandbox: pure simulation. No DOM, no canvas.
// Browser: exposes globalThis.SubHaulerTrim. Node: CommonJS export for tests.
(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports)
        module.exports = api;
    else
        root.SubHaulerTrim = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const CFG = Object.freeze({
        viewW: 128,
        viewH: 64,
        tile: 8,
        worldTilesX: 320,
        worldTilesY: 160,
        worldW: 2560,
        worldH: 1280,
        mapW: 64,
        mapH: 32,
        mapRevealRadius: 56,
        mapSonarRadius: 120,
        dockRadius: 42,
        dockSpeed: 16,
        jobOffers: 2,
        hullW: 26,
        hullH: 8,
        crateH: 2,
        maxCrates: 4,
        startHp: 6,
        startIntegrity: 100,
        startAir: 180,
        airDrain: 0.6,
        airBlow: 3,
        airSonar: 5,
        ballastRate: 18,
        ballastDrive: 35,
        ballastK: 0.5,
        vertDrag: 1.2,
        maxVy: 20,
        thrustAccel: 34,
        thrustDrag: 1.2,
        reverseFactor: 0.5,
        maxVx: 30,
        currentAccel: 12,
        cameraTauX: 0.28,
        cameraTauY: 0.38,
        cameraScreenX: 44,
        cameraScreenY: 32,
        sonarRadius: 46,
        sonarCooldown: 0.8,
        revealTime: 5,
        nearMine: 24,
        hitCooldown: 0.4,
        wallDamageX: 12,
        wallDamageY: 8,
        mineRadius: 3,
    });

    const SHOP_ITEMS = Object.freeze([
        Object.freeze({
            id: 'air',
            name: 'O2 TANK',
            desc: '+60 MAX AIR',
            price: 120,
            max: 2,
            levelKey: 'airLevel',
        }),
        Object.freeze({
            id: 'hull',
            name: 'HULL PLATE',
            desc: '+1 HULL MAX',
            price: 150,
            max: 2,
            levelKey: 'hullLevel',
        }),
        Object.freeze({
            id: 'sonar',
            name: 'SONAR',
            desc: 'PING -1 / CHART +30',
            price: 100,
            max: 1,
            levelKey: 'sonarLevel',
        }),
    ]);

    function clamp(value, lo, hi) {
        return value < lo ? lo : value > hi ? hi : value;
    }

    function mulberry32(seed) {
        let a = seed | 0;
        return function () {
            a = (a + 0x6d2b79f5) | 0;
            let t = Math.imul(a ^ (a >>> 15), 1 | a);
            t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        };
    }

    function neutralFor(crates) {
        return 50 - 5 * clamp(Math.round(crates), 0, CFG.maxCrates);
    }

    function totalHeightFor(crates) {
        return (
            CFG.hullH +
            CFG.crateH * clamp(Math.round(crates), 0, CFG.maxCrates)
        );
    }

    function targetsFor(crates) {
        const n = clamp(Math.round(crates), 0, CFG.maxCrates);
        return {
            neutral: neutralFor(n),
            totalHeight: totalHeightFor(n),
            accel: CFG.thrustAccel - 3 * n,
            topSpeed: (CFG.thrustAccel - 3 * n) / CFG.thrustDrag,
        };
    }

    function carveDisk(grid, cx, cy, radius) {
        const firstX = Math.max(0, Math.floor(cx - radius));
        const lastX = Math.min(
            CFG.worldTilesX - 1,
            Math.ceil(cx + radius)
        );
        const firstY = Math.max(0, Math.floor(cy - radius));
        const lastY = Math.min(
            CFG.worldTilesY - 1,
            Math.ceil(cy + radius)
        );
        const rSq = radius * radius;
        for (let y = firstY; y <= lastY; y++) {
            for (let x = firstX; x <= lastX; x++) {
                const dx = x - cx;
                const dy = y - cy;
                if (dx * dx + dy * dy <= rSq)
                    grid[y * CFG.worldTilesX + x] = 0;
            }
        }
    }

    function carveSegment(grid, ax, ay, bx, by, radius) {
        const dx = bx - ax;
        const dy = by - ay;
        const distance = Math.hypot(dx, dy);
        const steps = Math.max(1, Math.ceil(distance / 0.4));
        for (let i = 0; i <= steps; i++) {
            const t = i / steps;
            carveDisk(grid, ax + dx * t, ay + dy * t, radius);
        }
    }

    function tileSolid(grid, tx, ty) {
        if (
            tx < 0 ||
            ty < 0 ||
            tx >= CFG.worldTilesX ||
            ty >= CFG.worldTilesY
        )
            return true;
        return grid[ty * CFG.worldTilesX + tx] === 1;
    }

    function solidAt(grid, x, y) {
        return tileSolid(
            grid,
            Math.floor(x / CFG.tile),
            Math.floor(y / CFG.tile)
        );
    }

    function rectHits(grid, left, top, right, bottom) {
        const firstX = Math.floor(left / CFG.tile);
        const lastX = Math.floor((right - 0.001) / CFG.tile);
        const firstY = Math.floor(top / CFG.tile);
        const lastY = Math.floor((bottom - 0.001) / CFG.tile);
        for (let ty = firstY; ty <= lastY; ty++) {
            for (let tx = firstX; tx <= lastX; tx++) {
                if (tileSolid(grid, tx, ty))
                    return true;
            }
        }
        return false;
    }

    function generateCave(seed) {
        const rand = mulberry32((seed >>> 0) || 1);
        const W = CFG.worldTilesX;
        const H = CFG.worldTilesY;
        const grid = new Uint8Array(W * H);
        grid.fill(1);
        const chambers = [];
        const tunnels = [];
        const branches = [];
        const mineSites = [];

        function chamber(x, y, r, name, isSettlement) {
            const c = {
                x: clamp(x, r + 4, W - r - 4),
                y: clamp(y, r + 4, H - r - 4),
                r,
            };
            if (name)
                c.name = name;
            if (isSettlement)
                c.settlement = true;
            chambers.push(c);
            return c;
        }

        // Four settlements on a rough ring, joined by junction chambers.
        const tidewall = chamber(38, 48, 10, 'TIDEWALL', true);
        const benthos = chamber(150, 30, 10, 'BENTHOS', true);
        const cormorant = chamber(282, 62, 10, 'CORMORANT', true);
        const deepAnchor = chamber(172, 132, 11, 'DEEP ANCHOR', true);
        const j0 = chamber(91, 72, 6.5);
        const j1 = chamber(220, 32, 6.5);
        const j2 = chamber(252, 105, 7);
        const j3 = chamber(100, 138, 7);
        const s0 = chamber(72, 22, 5.5);
        const s1 = chamber(168, 72, 6.5);
        const s2 = chamber(292, 140, 5.5);
        const s3 = chamber(44, 108, 6);

        for (const c of chambers)
            carveDisk(grid, c.x, c.y, c.r);

        function link(a, b, radius, wiggle) {
            const mx = clamp(
                (a.x + b.x) / 2 + (rand() * 2 - 1) * wiggle,
                8,
                W - 8
            );
            const my = clamp(
                (a.y + b.y) / 2 + (rand() * 2 - 1) * wiggle,
                8,
                H - 8
            );
            carveSegment(grid, a.x, a.y, mx, my, radius);
            carveSegment(grid, mx, my, b.x, b.y, radius);
            tunnels.push({
                ax: a.x,
                ay: a.y,
                bx: mx,
                by: my,
                r: radius,
            });
            tunnels.push({
                ax: mx,
                ay: my,
                bx: b.x,
                by: b.y,
                r: radius,
            });
        }

        const ring = [
            tidewall,
            j0,
            benthos,
            j1,
            cormorant,
            j2,
            deepAnchor,
            j3,
        ];
        for (let i = 0; i < ring.length; i++) {
            link(
                ring[i],
                ring[(i + 1) % ring.length],
                3.6,
                7
            );
        }

        // A central shortcut plus three dead-end side chambers.
        link(benthos, s1, 3.4, 4);
        link(s1, j2, 3.4, 4);
        link(j0, s0, 3.4, 3);
        link(j2, s2, 3.4, 3);
        link(j3, s3, 3.4, 3);
        branches.push(s0, s2, s3);

        // Keep the world sealed.
        for (let y = 0; y < H; y++) {
            for (let x = 0; x < W; x++) {
                if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2)
                    grid[y * W + x] = 1;
            }
        }

        const mineChambers = chambers.filter(
            (c) => !c.settlement
        );
        for (let i = 0; i < 8; i++) {
            const chamber =
                mineChambers[i % mineChambers.length];
            const angle = rand() * Math.PI * 2;
            const distance = (chamber.r - 1.5) * CFG.tile;
            let x =
                chamber.x * CFG.tile +
                Math.cos(angle) * distance;
            let y =
                chamber.y * CFG.tile +
                Math.sin(angle) * distance;
            for (
                let k = 0;
                k < 12 && solidAt(grid, x, y);
                k++
            ) {
                x += (chamber.x * CFG.tile - x) * 0.15;
                y += (chamber.y * CFG.tile - y) * 0.15;
            }
            if (!solidAt(grid, x, y))
                mineSites.push({ x, y });
        }

        const settlements = [
            { id: 0, name: 'TIDEWALL', chamber: tidewall },
            { id: 1, name: 'BENTHOS', chamber: benthos },
            { id: 2, name: 'CORMORANT', chamber: cormorant },
            { id: 3, name: 'DEEP ANCHOR', chamber: deepAnchor },
        ].map((settlement) => {
            const c = settlement.chamber;
            return {
                id: settlement.id,
                name: settlement.name,
                x: c.x * CFG.tile,
                y: c.y * CFG.tile,
                dockX: c.x * CFG.tile,
                dockY: c.y * CFG.tile,
            };
        });

        return {
            grid,
            chambers,
            tunnels,
            branches,
            mines: mineSites,
            settlements,
        };
    }

    function overlaps(state) {
        const height = totalHeightFor(state.crates);
        return rectHits(
            state.grid,
            state.x,
            state.y - height,
            state.x + CFG.hullW,
            state.y
        );
    }

    function applyDamage(state, hpLoss, integrityLoss) {
        state.hp = Math.max(0, state.hp - hpLoss);
        state.integrity = Math.max(0, state.integrity - integrityLoss);
        state.flash = Math.max(state.flash, 0.2);
        if (state.hp <= 0 && !state.over) {
            state.over = true;
            state.message = 'HULL BREACH';
        }
    }

    function hurt(state, speed, threshold) {
        if (state.hitCooldown > 0)
            return;
        const amount = speed > threshold * 2 ? 2 : 1;
        state.hitCooldown = CFG.hitCooldown;
        state.collisions++;
        applyDamage(state, amount, amount * 4);
    }

    function moveX(state, amount) {
        if (amount === 0)
            return;
        const previous = state.x;
        state.x += amount;
        if (overlaps(state)) {
            const speed = Math.abs(state.vx);
            state.x = previous;
            state.vx = 0;
            if (speed > CFG.wallDamageX)
                hurt(state, speed, CFG.wallDamageX);
        }
    }

    function moveY(state, amount) {
        if (amount === 0)
            return;
        const previous = state.y;
        state.y += amount;
        if (overlaps(state)) {
            const speed = Math.abs(state.vy);
            state.y = previous;
            state.vy = 0;
            if (speed > CFG.wallDamageY)
                hurt(state, speed, CFG.wallDamageY);
        }
    }

    function collideMines(state) {
        if (state.over)
            return;
        const height = totalHeightFor(state.crates);
        const top = state.y - height;
        for (const mine of state.mines) {
            if (!mine.alive)
                continue;
            const nearestX = clamp(
                mine.x,
                state.x,
                state.x + CFG.hullW
            );
            const nearestY = clamp(mine.y, top, state.y);
            const dx = mine.x - nearestX;
            const dy = mine.y - nearestY;
            if (dx * dx + dy * dy <= CFG.mineRadius * CFG.mineRadius) {
                mine.alive = false;
                state.mineHits++;
                state.collisions++;
                applyDamage(state, 2, 10);
                if (state.over)
                    break;
            }
        }
    }

    function mapCellFor(state) {
        const height = totalHeightFor(state.crates);
        return {
            x: clamp(
                Math.floor(
                    ((state.x + CFG.hullW / 2) / CFG.worldW) *
                        CFG.mapW
                ),
                0,
                CFG.mapW - 1
            ),
            y: clamp(
                Math.floor(
                    ((state.y - height / 2) / CFG.worldH) *
                        CFG.mapH
                ),
                0,
                CFG.mapH - 1
            ),
        };
    }

    function revealMap(state, radius) {
        const height = totalHeightFor(state.crates);
        const centerX = state.x + CFG.hullW / 2;
        const centerY = state.y - height / 2;
        const cellW = CFG.worldW / CFG.mapW;
        const cellH = CFG.worldH / CFG.mapH;
        const firstX = clamp(
            Math.floor((centerX - radius) / cellW),
            0,
            CFG.mapW - 1
        );
        const lastX = clamp(
            Math.floor((centerX + radius) / cellW),
            0,
            CFG.mapW - 1
        );
        const firstY = clamp(
            Math.floor((centerY - radius) / cellH),
            0,
            CFG.mapH - 1
        );
        const lastY = clamp(
            Math.floor((centerY + radius) / cellH),
            0,
            CFG.mapH - 1
        );
        const rSq = radius * radius;
        for (let cy = firstY; cy <= lastY; cy++) {
            for (let cx = firstX; cx <= lastX; cx++) {
                const px = (cx + 0.5) * cellW;
                const py = (cy + 0.5) * cellH;
                const dx = px - centerX;
                const dy = py - centerY;
                if (dx * dx + dy * dy <= rSq)
                    state.mapExplored[cy * CFG.mapW + cx] = 1;
            }
        }
    }

    function cameraTargetX(state) {
        return clamp(
            state.x +
                CFG.hullW / 2 -
                CFG.cameraScreenX +
                clamp(state.vx * 0.5, -10, 10),
            0,
            CFG.worldW - CFG.viewW
        );
    }

    function cameraTargetY(state) {
        const height = totalHeightFor(state.crates);
        return clamp(
            state.y -
                height / 2 -
                CFG.cameraScreenY +
                clamp(state.vy * 0.7, -10, 10),
            0,
            CFG.worldH - CFG.viewH
        );
    }

    function jobDistance(a, b) {
        return Math.hypot(a.x - b.x, a.y - b.y) / CFG.tile;
    }

    function makeOffer(state, fromId, crates) {
        const from = state.settlements[fromId];
        state.offerSerial++;
        const rand = mulberry32(
            (state.seed ^
                Math.imul(state.offerSerial, 0x9e3779b1)) >>>
                0
        );
        let toId = Math.floor(
            rand() * (state.settlements.length - 1)
        );
        if (toId >= fromId)
            toId++;
        const distance = jobDistance(
            from,
            state.settlements[toId]
        );
        const reward =
            Math.round(
                (20 + crates * 10 + distance * 0.5) / 5
            ) * 5;
        const deadline = Math.round(
            80 + distance * 0.55 + crates * 8
        );
        return {
            id: `${fromId}:${state.offerSerial}`,
            fromId,
            toId,
            crates,
            reward,
            deadline,
        };
    }

    function refreshOffers(state, settlementId) {
        const list = state.jobs[settlementId];
        let guard = 0;
        while (
            list.length < CFG.jobOffers &&
            guard < 12
        ) {
            const offer = makeOffer(
                state,
                settlementId,
                4
            );
            guard++;
            if (
                list.some(
                    (other) => other.toId === offer.toId
                )
            )
                continue;
            list.push(offer);
        }
        while (list.length < CFG.jobOffers) {
            list.push(
                makeOffer(
                    state,
                    settlementId,
                    4
                )
            );
        }
    }

    function nearestDock(state) {
        if (state.docked !== null)
            return state.settlements[state.docked];
        const height = totalHeightFor(state.crates);
        const cx = state.x + CFG.hullW / 2;
        const cy = state.y - height / 2;
        let best = null;
        let bestSq = CFG.dockRadius * CFG.dockRadius;
        for (const settlement of state.settlements) {
            const dx = settlement.dockX - cx;
            const dy = settlement.dockY - cy;
            const dSq = dx * dx + dy * dy;
            if (dSq <= bestSq) {
                bestSq = dSq;
                best = settlement;
            }
        }
        return best;
    }

    function dock(state) {
        if (state.over || state.docked !== null)
            return false;
        const settlement = nearestDock(state);
        if (!settlement)
            return false;
        if (Math.hypot(state.vx, state.vy) > CFG.dockSpeed)
            return false;

        state.docked = settlement.id;
        state.x = settlement.dockX - CFG.hullW / 2;
        state.y =
            settlement.dockY +
            totalHeightFor(state.crates) / 2;
        state.vx = 0;
        state.vy = 0;
        state.ballast = neutralFor(state.crates);
        state.air = state.airMax;
        state.hp = state.maxHp;
        state.integrity = CFG.startIntegrity;
        state.sonar.ringT = -1;
        state.stationNote = '';

        const job = state.activeJob;
        if (job && job.toId === settlement.id) {
            if (
                state.time <= job.deadlineAt &&
                state.crates >= job.crates
            ) {
                state.credits += job.reward;
                state.deliveries++;
                state.stationNote = `DELIVERED +$${job.reward}`;
            } else {
                state.stationNote = 'CONTRACT FAILED';
            }
            state.activeJob = null;
            setCrates(state, 0);
            state.ballast = neutralFor(state.crates);
        }
        refreshOffers(state, settlement.id);
        return true;
    }

    function undock(state) {
        if (state.docked === null)
            return false;
        state.docked = null;
        state.stationNote = '';
        state.vx = 0;
        state.vy = 0;
        return true;
    }

    function acceptJob(state, index) {
        if (state.over || state.docked === null || state.activeJob)
            return false;
        const list = state.jobs[state.docked];
        const offer = list[index];
        if (!offer)
            return false;
        if (!setCrates(state, 0))
            return false;
        state.activeJob = {
            ...offer,
            acceptedAt: state.time,
            deadlineAt: state.time + offer.deadline,
        };
        state.jobs[state.docked] = list.filter(
            (item) => item !== offer
        );
        state.ballast = neutralFor(state.crates);
        state.stationNote = '';
        return true;
    }

    function abandonJob(state) {
        if (state.over || state.docked === null || !state.activeJob)
            return false;
        state.activeJob = null;
        setCrates(state, 0);
        state.ballast = neutralFor(state.crates);
        state.stationNote = 'CONTRACT ABANDONED';
        refreshOffers(state, state.docked);
        return true;
    }

    function buyShopItem(state, index) {
        if (state.over || state.docked === null)
            return { ok: false, reason: 'STATION' };
        const item = SHOP_ITEMS[index];
        if (!item)
            return { ok: false, reason: 'ITEM' };
        const level = state[item.levelKey] || 0;
        if (level >= item.max)
            return { ok: false, reason: 'SOLD', item };
        if (state.credits < item.price)
            return { ok: false, reason: 'CREDITS', item };

        state.credits -= item.price;
        state[item.levelKey] = level + 1;
        if (item.id === 'air') {
            state.airMax += 60;
            state.air = state.airMax;
        } else if (item.id === 'hull') {
            state.maxHp += 1;
            state.hp = state.maxHp;
        }
        return { ok: true, item, level: level + 1 };
    }

    function createGame(seed, crates, credits) {
        const cave = generateCave(seed);
        const n = clamp(Math.round(crates || 0), 0, CFG.maxCrates);
        const height = totalHeightFor(n);
        const start = cave.settlements[0];
        const state = {
            seed: seed >>> 0,
            crates: n,
            x: start.dockX - CFG.hullW / 2,
            y: start.dockY + height / 2,
            vx: 0,
            vy: 0,
            ballast: neutralFor(n),
            air: CFG.startAir,
            airMax: CFG.startAir,
            hp: CFG.startHp,
            maxHp: CFG.startHp,
            airLevel: 0,
            hullLevel: 0,
            sonarLevel: 0,
            integrity: CFG.startIntegrity,
            grid: cave.grid,
            chambers: cave.chambers,
            tunnels: cave.tunnels,
            branches: cave.branches,
            settlements: cave.settlements,
            docked: null,
            jobs: cave.settlements.map(() => []),
            activeJob: null,
            offerSerial: 0,
            credits: clamp(
                Math.round(credits || 0),
                0,
                99999
            ),
            deliveries: 0,
            stationNote: '',
            mines: cave.mines.map((mine) => ({
                x: mine.x,
                y: mine.y,
                alive: true,
                revealedUntil: 0,
            })),
            cameraX: 0,
            cameraY: 0,
            time: 0,
            mapExplored: new Uint8Array(CFG.mapW * CFG.mapH),
            sonar: {
                cooldown: 0,
                ringT: -1,
                originX: 0,
                originY: 0,
                pings: 0,
            },
            currentOn: false,
            hitCooldown: 0,
            collisions: 0,
            mineHits: 0,
            flash: 0,
            over: false,
            message: '',
        };
        state.cameraX = cameraTargetX(state);
        state.cameraY = cameraTargetY(state);
        revealMap(state, CFG.mapRevealRadius);
        for (let i = 0; i < state.settlements.length; i++)
            refreshOffers(state, i);
        return state;
    }

    function step(state, input, dt) {
        if (state.over || state.docked !== null)
            return state;
        input = input || {};
        dt = clamp(dt || 1 / 60, 1 / 240, 1 / 30);

        state.time += dt;
        state.air = Math.max(0, state.air - CFG.airDrain * dt);
        state.hitCooldown = Math.max(0, state.hitCooldown - dt);
        state.flash = Math.max(0, state.flash - dt);
        state.sonar.cooldown = Math.max(0, state.sonar.cooldown - dt);

        if (state.sonar.ringT >= 0) {
            state.sonar.ringT += dt;
            if (state.sonar.ringT > CFG.sonarCooldown)
                state.sonar.ringT = -1;
        }

        const ballastInput =
            (input.up ? 1 : 0) - (input.down ? 1 : 0);
        if (ballastInput > 0) {
            if (state.air > 0) {
                const change = Math.min(
                    CFG.ballastRate * dt,
                    state.ballast
                );
                state.ballast -= change;
                state.air = Math.max(
                    0,
                    state.air - CFG.airBlow * dt
                );
            }
        } else if (ballastInput < 0) {
            state.ballast = Math.min(
                100,
                state.ballast + CFG.ballastRate * dt
            );
        }

        const neutral = neutralFor(state.crates);
        const drive = clamp(
            state.ballast - neutral,
            -CFG.ballastDrive,
            CFG.ballastDrive
        );
        state.vy = clamp(
            state.vy +
                (drive * CFG.ballastK - state.vy * CFG.vertDrag) * dt,
            -CFG.maxVy,
            CFG.maxVy
        );

        const thrust =
            (input.right ? 1 : 0) - (input.left ? 1 : 0);
        const accel =
            CFG.thrustAccel - 3 * clamp(state.crates, 0, CFG.maxCrates);
        const power =
            thrust > 0
                ? accel
                : thrust < 0
                  ? -accel * CFG.reverseFactor
                  : 0;
        state.vx = clamp(
            state.vx +
                (power -
                    state.vx * CFG.thrustDrag +
                    (state.currentOn ? CFG.currentAccel : 0)) *
                    dt,
            -CFG.maxVx,
            CFG.maxVx
        );

        moveX(state, state.vx * dt);
        moveY(state, state.vy * dt);
        collideMines(state);
        revealMap(state, CFG.mapRevealRadius);

        const targetX = cameraTargetX(state);
        const targetY = cameraTargetY(state);
        const alphaX = 1 - Math.exp(-dt / CFG.cameraTauX);
        const alphaY = 1 - Math.exp(-dt / CFG.cameraTauY);
        state.cameraX += (targetX - state.cameraX) * alphaX;
        state.cameraY += (targetY - state.cameraY) * alphaY;

        return state;
    }

    function setCrates(state, crates) {
        if (state.over)
            return false;
        const next = clamp(Math.round(crates), 0, CFG.maxCrates);
        if (next === state.crates)
            return true;

        const previous = state.crates;
        const previousY = state.y;
        state.crates = next;

        if (!overlaps(state))
            return true;

        let found = false;
        for (let offset = 1; offset <= 16 && !found; offset++) {
            state.y = previousY + offset;
            if (!overlaps(state)) {
                found = true;
                break;
            }
        }

        if (!found) {
            state.y = previousY;
            state.crates = previous;
            return false;
        }

        state.collisions++;
        applyDamage(state, 1, 4);
        return true;
    }

    function ping(state) {
        if (state.over || state.docked !== null)
            return false;
        const sonarCost = Math.max(
            2,
            CFG.airSonar - (state.sonarLevel || 0)
        );
        if (state.air < sonarCost || state.sonar.cooldown > 0)
            return false;

        const height = totalHeightFor(state.crates);
        state.air = Math.max(0, state.air - sonarCost);
        state.sonar.cooldown = CFG.sonarCooldown;
        state.sonar.ringT = 0;
        state.sonar.originX = state.x + CFG.hullW / 2;
        state.sonar.originY = state.y - height / 2;
        state.sonar.pings++;
        revealMap(
            state,
            CFG.mapSonarRadius +
                (state.sonarLevel || 0) * 30
        );

        for (const mine of state.mines) {
            if (!mine.alive)
                continue;
            const dx = mine.x - state.sonar.originX;
            const dy = mine.y - state.sonar.originY;
            if (dx * dx + dy * dy <= CFG.sonarRadius * CFG.sonarRadius)
                mine.revealedUntil = state.time + CFG.revealTime;
        }
        return true;
    }

    function toggleCurrent(state) {
        state.currentOn = !state.currentOn;
        return state.currentOn;
    }

    return {
        CFG,
        SHOP_ITEMS,
        clamp,
        neutralFor,
        totalHeightFor,
        targetsFor,
        generateCave,
        tileSolid,
        solidAt,
        rectHits,
        overlaps,
        mapCellFor,
        revealMap,
        createGame,
        step,
        setCrates,
        ping,
        toggleCurrent,
        nearestDock,
        dock,
        undock,
        acceptJob,
        abandonJob,
        refreshOffers,
        buyShopItem,
    };
});
