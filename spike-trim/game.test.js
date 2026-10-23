'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const G = require('./game.js');

function reachable(grid, startX, startY) {
    const seen = new Uint8Array(G.CFG.worldTilesX * G.CFG.worldTilesY);
    const stack = [[startX, startY]];
    while (stack.length) {
        const [x, y] = stack.pop();
        if (
            x < 0 ||
            y < 0 ||
            x >= G.CFG.worldTilesX ||
            y >= G.CFG.worldTilesY
        )
            continue;
        const index = y * G.CFG.worldTilesX + x;
        if (seen[index] || grid[index] === 1)
            continue;
        seen[index] = 1;
        stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
    }
    return seen;
}

test('same seed produces the same cave', () => {
    const a = G.generateCave(4242);
    const b = G.generateCave(4242);
    assert.deepEqual(Array.from(a.grid), Array.from(b.grid));
    assert.deepEqual(a.chambers, b.chambers);
    assert.deepEqual(a.mines, b.mines);
});

test('every chamber and branch is connected to the start', () => {
    for (const seed of [1, 77, 4242, 99999]) {
        const cave = G.generateCave(seed);
        const start = cave.chambers[0];
        const seen = reachable(
            cave.grid,
            Math.round(start.x),
            Math.round(start.y)
        );
        for (const chamber of cave.chambers) {
            assert.equal(
                seen[
                    Math.round(chamber.y) * G.CFG.worldTilesX +
                        Math.round(chamber.x)
                ],
                1,
                `seed ${seed}: chamber ${chamber.x},${chamber.y} unreachable`
            );
        }
        for (const branch of cave.branches) {
            assert.equal(
                seen[
                    Math.round(branch.y) * G.CFG.worldTilesX +
                        Math.round(branch.x)
                ],
                1,
                `seed ${seed}: branch unreachable`
            );
        }
    }
});

test('the loaded hull fits everywhere along tunnel centre lines', () => {
    const halfW = G.CFG.hullW / 2;
    const halfH = G.totalHeightFor(G.CFG.maxCrates) / 2;
    for (const seed of [1, 77, 4242, 99999]) {
        const cave = G.generateCave(seed);
        for (const tunnel of cave.tunnels) {
            for (let i = 1; i < 10; i++) {
                const t = i / 10;
                const cx =
                    (tunnel.ax + (tunnel.bx - tunnel.ax) * t) *
                    G.CFG.tile;
                const cy =
                    (tunnel.ay + (tunnel.by - tunnel.ay) * t) *
                    G.CFG.tile;
                assert.equal(
                    G.rectHits(
                        cave.grid,
                        cx - halfW,
                        cy - halfH,
                        cx + halfW,
                        cy + halfH
                    ),
                    false,
                    `seed ${seed}: tunnel pinch at ${cx},${cy}`
                );
            }
        }
    }
});

test('spawn is clear for empty and loaded hulls', () => {
    for (const crates of [0, G.CFG.maxCrates]) {
        const state = G.createGame(4242, crates);
        assert.equal(G.overlaps(state), false);
        assert.ok(state.x > 0 && state.y > 0);
        assert.ok(state.x + G.CFG.hullW < G.CFG.worldW);
        assert.ok(state.y < G.CFG.worldH);
    }
});

test('neutral trim holds depth', () => {
    const state = G.createGame(4242, 0);
    const y = state.y;
    for (let i = 0; i < 300; i++)
        G.step(state, {});
    assert.ok(Math.abs(state.y - y) < 0.25);
    assert.ok(state.air < G.CFG.startAir);
});

test('loaded cargo shifts neutral trim and makes the sub sink', () => {
    const state = G.createGame(4242, 0);
    const y = state.y;
    assert.equal(G.setCrates(state, G.CFG.maxCrates), true);
    assert.equal(state.hp, G.CFG.startHp);
    for (let i = 0; i < 120; i++)
        G.step(state, {});
    assert.ok(
        state.y > y + 4,
        `expected sink, moved ${(state.y - y).toFixed(2)} px`
    );
    assert.ok(state.ballast > G.neutralFor(G.CFG.maxCrates));
});

test('blowing spends air and lowers ballast', () => {
    const state = G.createGame(4242, 0);
    state.air = 100;
    state.ballast = 50;
    for (let i = 0; i < 60; i++)
        G.step(state, { up: true });
    assert.ok(state.ballast < 40);
    assert.ok(state.air < 98);
});

test('flooding is free apart from the clock', () => {
    const state = G.createGame(4242, 0);
    state.air = 50;
    state.ballast = 40;
    for (let i = 0; i < 60; i++)
        G.step(state, { down: true });
    assert.ok(state.ballast > 55);
    assert.ok(state.air > 49.3 && state.air < 49.5);
});

test('no air means no blowing', () => {
    const state = G.createGame(4242, 0);
    state.air = 0;
    state.ballast = 50;
    for (let i = 0; i < 60; i++)
        G.step(state, { up: true });
    assert.equal(state.ballast, 50);
    assert.equal(state.air, 0);
});

test('sonar costs air and respects cooldown', () => {
    const state = G.createGame(4242, 0);
    state.air = 100;
    assert.equal(G.ping(state), true);
    assert.equal(state.air, 95);
    assert.equal(G.ping(state), false);
    for (let i = 0; i < 60; i++)
        G.step(state, {});
    assert.ok(state.air > 94.3 && state.air < 94.5);
    assert.equal(G.ping(state), true);
    assert.ok(state.air < 90);
});

test('current pushes a drifting sub', () => {
    const state = G.createGame(4242, 0);
    G.toggleCurrent(state);
    const x = state.x;
    for (let i = 0; i < 120; i++)
        G.step(state, {});
    assert.ok(state.vx > 1);
    assert.ok(state.x > x + 4);
});

test('cargo clipping nudges the hull down and damages it', () => {
    const state = G.createGame(1, 0);
    const grid = new Uint8Array(
        G.CFG.worldTilesX * G.CFG.worldTilesY
    );
    grid.fill(1);
    for (let y = 8; y <= 16; y++) {
        for (let x = 8; x <= 20; x++)
            grid[y * G.CFG.worldTilesX + x] = 0;
    }
    state.grid = grid;
    state.x = 12 * G.CFG.tile;
    state.y = 76;
    state.hp = G.CFG.startHp;
    const hp = state.hp;

    assert.equal(G.setCrates(state, G.CFG.maxCrates), true);
    assert.equal(state.crates, G.CFG.maxCrates);
    assert.ok(state.y > 76 && state.y <= 84);
    assert.ok(state.hp < hp);
});

test('cargo change reverts when there is no room for the taller hull', () => {
    const state = G.createGame(1, 0);
    const grid = new Uint8Array(
        G.CFG.worldTilesX * G.CFG.worldTilesY
    );
    grid.fill(1);
    for (let x = 8; x <= 20; x++)
        grid[10 * G.CFG.worldTilesX + x] = 0;
    state.grid = grid;
    state.x = 12 * G.CFG.tile;
    state.y = 11 * G.CFG.tile;
    const hp = state.hp;

    assert.equal(G.setCrates(state, G.CFG.maxCrates), false);
    assert.equal(state.crates, 0);
    assert.equal(state.hp, hp);
});

test('camera stays inside the world', () => {
    const state = G.createGame(4242, 0);
    state.x = G.CFG.worldW - 20;
    state.y = G.CFG.worldH - 20;
    G.step(state, {});
    assert.ok(state.cameraX >= 0);
    assert.ok(state.cameraX <= G.CFG.worldW - G.CFG.viewW);
    assert.ok(state.cameraY >= 0);
    assert.ok(state.cameraY <= G.CFG.worldH - G.CFG.viewH);
});

test('chart starts local and grows with travel', () => {
    const state = G.createGame(4242, 0);
    const initial = state.mapExplored.reduce(
        (sum, value) => sum + value,
        0
    );
    assert.ok(initial > 0);
    for (let i = 0; i < 180; i++)
        G.step(state, { right: true });
    const after = state.mapExplored.reduce(
        (sum, value) => sum + value,
        0
    );
    assert.ok(after > initial);
});

test('sonar reveals a larger chart area', () => {
    const state = G.createGame(4242, 0);
    state.air = 100;
    const initial = state.mapExplored.reduce(
        (sum, value) => sum + value,
        0
    );
    assert.equal(G.ping(state), true);
    const after = state.mapExplored.reduce(
        (sum, value) => sum + value,
        0
    );
    assert.ok(after > initial + 10);
});

test('mines are placed in open water', () => {
    const cave = G.generateCave(4242);
    assert.ok(cave.mines.length > 0);
    for (const mine of cave.mines) {
        assert.equal(
            G.solidAt(cave.grid, mine.x, mine.y),
            false
        );
    }
});
