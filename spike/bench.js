// Dev tool, not a test: repeatable A/B read on cave fairness and cargo
// handling. Mines are cleared so this measures the corridor and the sub,
// not the autopilot's aim. Run with:
//   node spike/bench.js
'use strict';

const G = require('./game.js');

function makePilot(delay, lookAhead) {
    let held = { up: false, down: false, left: false, right: false };
    let tick = 0;
    return function (state) {
        tick++;
        if (tick % delay !== 0)
            return held;
        const target = G.targetsFor(state.crates);
        const look = G.columnAt(state.columns, state.x + lookAhead);
        const desired =
            (look.ceil + look.floor) / 2 + target.totalHeight / 2;
        const projected = state.y + state.vy * 0.18;
        const error = desired - projected;
        held = {
            up: error < -1.5,
            down: error > 1.5,
            left: false,
            right: false,
        };
        return held;
    };
}

function run(seed, crates, delay, lookAhead) {
    const state = G.createGame(seed, crates);
    state.mines = [];
    const pilot = makePilot(delay, lookAhead);
    let steps = 0;
    while (!state.over && !state.finished && steps < 60 * 90) {
        G.step(state, pilot(state), 1 / 60);
        steps++;
    }
    return {
        finished: state.finished,
        over: state.over,
        time: state.time,
        collisions: state.collisions,
        integrity: state.integrity,
    };
}

function summarize(label, results) {
    const delivered = results.filter((result) => result.finished);
    const sunk = results.filter((result) => result.over);
    const avg = (key) =>
        delivered.length
            ? delivered.reduce((sum, r) => sum + r[key], 0) /
              delivered.length
            : 0;
    console.log(
        `${label}: delivered ${delivered.length}/${results.length}` +
            ` sunk ${sunk.length}` +
            ` hits ${avg('collisions').toFixed(1)}` +
            ` time ${avg('time').toFixed(1)}s` +
            ` integrity ${avg('integrity').toFixed(0)}%`
    );
    return { delivered: delivered.length, hits: avg('collisions') };
}

const seeds = Array.from({ length: 50 }, (_, i) => 1000 + i * 37);

for (const crates of [0, 2, 4]) {
    const perfect = seeds.map((seed) => run(seed, crates, 1, 40));
    summarize(`cargo ${crates} perfect`, perfect);
}

console.log('');

const laggy = new Map();
for (const crates of [0, 2, 4]) {
    const results = seeds.map((seed) => run(seed, crates, 10, 26));
    laggy.set(crates, summarize(`cargo ${crates} laggy`, results));
}

console.log('');
console.log(
    `laggy mass delta: hits +${(
        laggy.get(4).hits - laggy.get(0).hits
    ).toFixed(1)}, deliveries ${laggy.get(4).delivered - laggy.get(0).delivered}`
);
