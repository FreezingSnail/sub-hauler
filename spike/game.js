// Sub-hauler spike: pure simulation. No DOM, no canvas.
// Browser: exposes globalThis.SubHauler. Node: CommonJS export for tests.
(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports)
        module.exports = api;
    else
        root.SubHauler = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const CFG = Object.freeze({
        width: 128,
        height: 64,
        routeLength: 2400,
        columnStep: 4,
        minGap: 26,
        maxGap: 42,
        startX: 48,
        hullW: 26,
        hullH: 8,
        crateH: 2,
        maxCrates: 4,
        startHp: 6,
        startIntegrity: 100,
        sonarCharges: 3,
        sonarRadius: 46,
        sonarDuration: 0.55,
        sonarCooldown: 0.8,
        revealTime: 5,
        screenSubX: 36,
        nearMine: 18,
        hitCooldown: 0.35,
        damageSpeed: 14,
        mineRadius: 3,
    });

    function clamp(value, lo, hi) {
        return value < lo ? lo : value > hi ? hi : value;
    }

    function approach(value, target, maxDelta) {
        if (value < target)
            return Math.min(value + maxDelta, target);
        return Math.max(value - maxDelta, target);
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

    // Handling targets. Cargo is the one difficulty dial: it raises sink,
    // lowers top speed, and dulls every acceleration.
    function targetsFor(crates) {
        const n = clamp(Math.round(crates), 0, CFG.maxCrates);
        return {
            cruise: 52 - 3 * n,
            maxSpeed: 88 - 5 * n,
            minSpeed: 16,
            accel: 100 - 10 * n,
            brake: 170 - 15 * n,
            sink: 4 + 4 * n,
            liftSpeed: 60 - 5 * n,
            diveSpeed: 34,
            vertAccel: 150 - 18 * n,
            totalHeight: CFG.hullH + CFG.crateH * n,
        };
    }

    function generateCave(seed) {
        const rand = mulberry32((seed >>> 0) || 1);
        const columns = [];
        const mines = [];
        const end = CFG.routeLength + 160;
        let center = 32;
        let gap = 42;
        let centerVel = 0;

        for (let x = 0; x <= end; x += CFG.columnStep) {
            const t = clamp(x / CFG.routeLength, 0, 1);
            const targetGap = clamp(
                36 - 10 * t + (rand() * 2 - 1) * 2,
                CFG.minGap,
                CFG.maxGap
            );
            gap = approach(gap, targetGap, 0.35);

            if (rand() < 0.07)
                centerVel = (rand() * 2 - 1) * (0.45 + 0.35 * t);
            center += centerVel;
            centerVel *= 0.985;

            if (x < 96) {
                gap = Math.max(gap, 36);
                center += (32 - center) * 0.2;
            }
            if (x > CFG.routeLength - 140) {
                gap = approach(gap, 48, 0.9);
                center += (32 - center) * 0.15;
            }

            const half = gap / 2;
            const minCenter = half + 4;
            const maxCenter = CFG.height - half - 4;
            if (center < minCenter) {
                center = minCenter;
                centerVel = Math.abs(centerVel) * 0.5;
            }
            if (center > maxCenter) {
                center = maxCenter;
                centerVel = -Math.abs(centerVel) * 0.5;
            }

            let ceil = Math.round(center - half);
            let floor = Math.round(center + half);
            if (floor - ceil < CFG.minGap) {
                const mid = (ceil + floor) / 2;
                ceil = Math.round(mid - CFG.minGap / 2);
                floor = ceil + CFG.minGap;
            }
            if (ceil < 3) {
                ceil = 3;
                floor = Math.max(floor, ceil + CFG.minGap);
            }
            if (floor > CFG.height - 3) {
                floor = CFG.height - 3;
                ceil = Math.min(ceil, floor - CFG.minGap);
            }

            columns.push({ x, ceil, floor });
        }

        let mineX = 150;
        while (true) {
            mineX += 150 + rand() * 170;
            if (mineX > CFG.routeLength - 120)
                break;
            const index = clamp(
                Math.floor(mineX / CFG.columnStep),
                0,
                columns.length - 1
            );
            const col = columns[index];
            if (col.floor - col.ceil < CFG.minGap + 2)
                continue;
            // Wall mines only: a centred mine would seal a minimum-gap cave.
            const ceilingMine = rand() < 0.5;
            mines.push({
                x: Math.round(mineX),
                y: ceilingMine
                    ? col.ceil + CFG.mineRadius
                    : col.floor - CFG.mineRadius,
                alive: true,
                revealedUntil: 0,
            });
        }

        return { columns, mines };
    }

    function columnAt(columns, x) {
        const index = clamp(
            Math.floor(x / CFG.columnStep),
            0,
            columns.length - 1
        );
        return columns[index];
    }

    function createGame(seed, crates) {
        const cave = generateCave(seed);
        const n = clamp(Math.round(crates || 0), 0, CFG.maxCrates);
        const target = targetsFor(n);
        const startColumn = columnAt(
            cave.columns,
            CFG.startX + CFG.hullW / 2
        );
        const center = (startColumn.ceil + startColumn.floor) / 2;

        return {
            seed: seed >>> 0,
            crates: n,
            x: CFG.startX,
            y: center + target.totalHeight / 2,
            vx: target.cruise,
            vy: 0,
            hp: CFG.startHp,
            integrity: CFG.startIntegrity,
            columns: cave.columns,
            mines: cave.mines.map((mine) => ({ ...mine })),
            time: 0,
            distance: 0,
            cameraX: CFG.startX - CFG.screenSubX,
            sonar: {
                charges: CFG.sonarCharges,
                cooldown: 0,
                ringT: -1,
                originX: 0,
                originY: 0,
                pings: 0,
            },
            hitCooldown: 0,
            collisions: 0,
            damageTaken: 0,
            mineHits: 0,
            flash: 0,
            over: false,
            finished: false,
            payout: 0,
            bonus: 0,
            score: 0,
            message: '',
        };
    }

    function applyDamage(state, hpLoss, integrityLoss) {
        state.hp = Math.max(0, state.hp - hpLoss);
        state.integrity = Math.max(0, state.integrity - integrityLoss);
        state.damageTaken += hpLoss;
        if (state.hp <= 0 && !state.over) {
            state.over = true;
            state.message = 'HULL BREACH';
        }
    }

    function hurt(state, impact) {
        if (state.hitCooldown > 0)
            return;
        const amount = impact > 44 ? 2 : 1;
        state.hitCooldown = CFG.hitCooldown;
        state.collisions++;
        state.flash = 0.15;
        applyDamage(state, amount, amount * 4);
    }

    function collideCave(state) {
        const target = targetsFor(state.crates);
        const left = state.x;
        const right = state.x + CFG.hullW;
        const first = clamp(
            Math.floor(left / CFG.columnStep),
            0,
            state.columns.length - 1
        );
        const last = clamp(
            Math.floor((right - 0.001) / CFG.columnStep),
            0,
            state.columns.length - 1
        );

        // Conservative AABB vs heightfield: use the lowest ceiling and the
        // highest floor touching the hull.
        let ceilMax = 0;
        let floorMin = CFG.height;
        for (let i = first; i <= last; i++) {
            const col = state.columns[i];
            if (col.ceil > ceilMax)
                ceilMax = col.ceil;
            if (col.floor < floorMin)
                floorMin = col.floor;
        }

        const top = state.y - target.totalHeight;
        if (ceilMax + target.totalHeight > floorMin) {
            state.y = (ceilMax + floorMin) / 2 + target.totalHeight / 2;
            state.vy = 0;
            return;
        }

        if (top < ceilMax) {
            const impact = Math.abs(state.vy);
            state.y = ceilMax + target.totalHeight;
            state.vy = 0;
            if (impact > CFG.damageSpeed)
                hurt(state, impact);
        }
        if (state.y > floorMin) {
            const impact = Math.abs(state.vy);
            state.y = floorMin;
            state.vy = 0;
            if (impact > CFG.damageSpeed)
                hurt(state, impact);
        }
    }

    function collideMines(state) {
        if (state.over)
            return;
        const target = targetsFor(state.crates);
        const top = state.y - target.totalHeight;
        for (const mine of state.mines) {
            if (!mine.alive)
                continue;
            const nearestX = clamp(mine.x, state.x, state.x + CFG.hullW);
            const nearestY = clamp(mine.y, top, state.y);
            const dx = mine.x - nearestX;
            const dy = mine.y - nearestY;
            if (dx * dx + dy * dy <= CFG.mineRadius * CFG.mineRadius) {
                mine.alive = false;
                state.mineHits++;
                state.collisions++;
                state.flash = 0.25;
                applyDamage(state, 2, 10);
                if (state.over)
                    break;
            }
        }
    }

    function step(state, input, dt) {
        if (state.over || state.finished)
            return state;
        input = input || {};
        dt = clamp(dt || 1 / 60, 1 / 240, 1 / 30);

        state.time += dt;
        state.sonar.cooldown = Math.max(0, state.sonar.cooldown - dt);
        state.hitCooldown = Math.max(0, state.hitCooldown - dt);
        state.flash = Math.max(0, state.flash - dt);
        if (state.sonar.ringT >= 0) {
            state.sonar.ringT += dt;
            if (state.sonar.ringT > CFG.sonarDuration)
                state.sonar.ringT = -1;
        }

        const target = targetsFor(state.crates);

        const throttle =
            (input.right ? 1 : 0) - (input.left ? 1 : 0);
        let targetVx;
        if (throttle > 0)
            targetVx = target.maxSpeed;
        else if (throttle < 0)
            targetVx = target.minSpeed;
        else
            targetVx = target.cruise;
        const rate = targetVx < state.vx ? target.brake : target.accel;
        state.vx = approach(state.vx, targetVx, rate * dt);

        const lift = (input.up ? 1 : 0) - (input.down ? 1 : 0);
        let targetVy = target.sink;
        if (lift > 0)
            targetVy -= target.liftSpeed;
        else if (lift < 0)
            targetVy += target.diveSpeed;
        state.vy = approach(state.vy, targetVy, target.vertAccel * dt);

        state.x += state.vx * dt;
        state.y += state.vy * dt;

        collideCave(state);
        collideMines(state);

        state.cameraX = state.x - CFG.screenSubX;
        state.distance = clamp(
            state.x - CFG.startX,
            0,
            CFG.routeLength - CFG.startX
        );

        if (!state.over && state.x >= CFG.routeLength) {
            state.x = CFG.routeLength;
            state.finished = true;
            state.payout = Math.round(
                state.crates * 100 * (state.integrity / 100)
            );
            const par = 50;
            state.bonus = Math.max(0, Math.round((par - state.time) * 4));
            state.score = state.payout + state.bonus;
            state.message = 'DELIVERED';
        }

        return state;
    }

    function ping(state) {
        if (state.over || state.finished)
            return false;
        if (state.sonar.charges <= 0 || state.sonar.cooldown > 0)
            return false;

        const target = targetsFor(state.crates);
        state.sonar.charges--;
        state.sonar.cooldown = CFG.sonarCooldown;
        state.sonar.ringT = 0;
        state.sonar.originX = state.x + CFG.hullW / 2;
        state.sonar.originY = state.y - target.totalHeight / 2;
        state.sonar.pings++;

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

    return {
        CFG,
        clamp,
        approach,
        targetsFor,
        generateCave,
        columnAt,
        createGame,
        step,
        ping,
    };
});
