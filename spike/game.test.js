'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const G = require('./game.js');

test('same seed produces an identical cave', () => {
    const a = G.generateCave(1234);
    const b = G.generateCave(1234);
    assert.deepEqual(a.columns, b.columns);
    assert.deepEqual(a.mines, b.mines);
});

test('cave never narrows below the minimum gap', () => {
    for (const seed of [1, 7, 42, 999, 123456]) {
        const { columns } = G.generateCave(seed);
        for (const column of columns) {
            const gap = column.floor - column.ceil;
            assert.ok(
                gap >= G.CFG.minGap,
                `seed ${seed} x ${column.x}: gap ${gap} < ${G.CFG.minGap}`
            );
            assert.ok(column.ceil >= 0);
            assert.ok(column.floor <= G.CFG.height);
        }
    }
});

test('cargo lowers speed and raises sink', () => {
    const empty = G.targetsFor(0);
    const loaded = G.targetsFor(G.CFG.maxCrates);
    assert.ok(loaded.maxSpeed < empty.maxSpeed);
    assert.ok(loaded.cruise < empty.cruise);
    assert.ok(loaded.accel < empty.accel);
    assert.ok(loaded.brake < empty.brake);
    assert.ok(loaded.sink > empty.sink);
    assert.ok(loaded.vertAccel < empty.vertAccel);
    assert.equal(
        loaded.totalHeight,
        G.CFG.hullH + G.CFG.maxCrates * G.CFG.crateH
    );
    assert.ok(loaded.totalHeight < G.CFG.minGap);
});

test('neutral throttle settles at cruise and advances', () => {
    const state = G.createGame(5, 0);
    for (let i = 0; i < 120; i++)
        G.step(state, {});
    assert.ok(Math.abs(state.vx - G.targetsFor(0).cruise) < 0.5);
    assert.ok(state.x > G.CFG.startX);
});

test('loaded sub sinks faster than empty under neutral input', () => {
    const empty = G.createGame(5, 0);
    const loaded = G.createGame(5, G.CFG.maxCrates);
    const emptyStart = empty.y;
    const loadedStart = loaded.y;
    for (let i = 0; i < 30; i++) {
        G.step(empty, {});
        G.step(loaded, {});
    }
    const emptyDrop = empty.y - emptyStart;
    const loadedDrop = loaded.y - loadedStart;
    assert.ok(
        loadedDrop > emptyDrop + 3,
        `empty ${emptyDrop.toFixed(2)} px, loaded ${loadedDrop.toFixed(2)} px`
    );
    assert.equal(empty.collisions, 0);
    assert.equal(loaded.collisions, 0);
});

test('throttle raises speed above cruise', () => {
    const state = G.createGame(5, 1);
    for (let i = 0; i < 180; i++)
        G.step(state, { right: true });
    assert.ok(state.vx > G.targetsFor(1).cruise + 10);
    assert.ok(state.vx <= G.targetsFor(1).maxSpeed + 0.001);
});

test('loaded sub damages itself when it rests on the floor', () => {
    const empty = G.createGame(5, 0);
    const loaded = G.createGame(5, G.CFG.maxCrates);
    for (let i = 0; i < 180; i++) {
        G.step(empty, {});
        G.step(loaded, {});
    }
    assert.equal(empty.collisions, 0);
    assert.ok(loaded.collisions > 0);
    assert.ok(loaded.integrity < 100);
});

test('finished run does not keep stepping', () => {
    const state = G.createGame(5, 0);
    state.finished = true;
    const x = state.x;
    G.step(state, { right: true });
    assert.equal(state.x, x);
});

test('sonar consumes a charge and respects cooldown', () => {
    const state = G.createGame(5, 2);
    assert.equal(G.ping(state), true);
    assert.equal(state.sonar.charges, G.CFG.sonarCharges - 1);
    assert.equal(G.ping(state), false);
    for (let i = 0; i < 60; i++)
        G.step(state, {});
    assert.equal(G.ping(state), true);
    assert.equal(state.sonar.charges, G.CFG.sonarCharges - 2);
});

test('ceiling impact clamps the sub and damages the hull', () => {
    const state = G.createGame(5, 0);
    const totalHeight = G.targetsFor(state.crates).totalHeight;
    const column = G.columnAt(state.columns, state.x + G.CFG.hullW / 2);
    state.y = column.ceil + totalHeight - 6;
    state.vy = -50;
    const hp = state.hp;
    const integrity = state.integrity;
    G.step(state, { up: true });
    assert.ok(state.y - totalHeight >= 0);
    assert.ok(state.vy >= 0);
    assert.ok(state.hp < hp);
    assert.ok(state.integrity < integrity);
});

test('mine hit damages the hull and removes the mine', () => {
    const state = G.createGame(5, 1);
    const target = G.targetsFor(state.crates);
    state.mines = [
        {
            x: state.x + 5,
            y: state.y - target.totalHeight / 2,
            alive: true,
            revealedUntil: 0,
        },
    ];
    const hp = state.hp;
    G.step(state, {});
    assert.equal(state.mines[0].alive, false);
    assert.equal(state.hp, hp - 2);
    assert.equal(state.mineHits, 1);
});

test('finish stops the run and scores cargo', () => {
    const state = G.createGame(5, G.CFG.maxCrates);
    const target = G.targetsFor(state.crates);
    const column = G.columnAt(state.columns, G.CFG.routeLength - 2);
    state.x = G.CFG.routeLength - 0.1;
    state.y =
        (column.ceil + column.floor) / 2 + target.totalHeight / 2;
    state.integrity = 80;
    G.step(state, {});
    assert.equal(state.finished, true);
    assert.equal(state.over, false);
    assert.equal(state.payout, 320);
    assert.ok(state.score >= state.payout);
    assert.equal(state.x, G.CFG.routeLength);
});
