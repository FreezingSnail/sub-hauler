'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function makeCtx() {
    const calls = [];
    const ctx = new Proxy(
        {},
        {
            get(target, prop) {
                if (prop === 'createPattern')
                    return () => ({ pattern: true });
                if (prop === 'fillStyle' || prop === 'strokeStyle')
                    return target[prop];
                if (typeof prop !== 'string')
                    return undefined;
                return (...args) => calls.push({ prop, args });
            },
            set(target, prop, value) {
                target[prop] = value;
                return true;
            },
        }
    );
    return { ctx, calls };
}

function makeElement(ctx) {
    return {
        width: 0,
        height: 0,
        classList: { add() {}, remove() {} },
        textContent: '',
        innerHTML: '',
        getContext() {
            return ctx;
        },
    };
}

test('main.js boots and renders a frame', () => {
    const { ctx, calls } = makeCtx();
    const elements = {
        game: makeElement(ctx),
        overlay: makeElement(ctx),
        overlayText: makeElement(ctx),
        debug: makeElement(ctx),
    };

    let rafCallback = null;
    let rafCount = 0;

    const sandbox = {
        console,
        Math,
        Number,
        Set,
        URL,
        URLSearchParams,
        performance: { now: () => 0 },
        window: {
            location: { href: 'file:///spike/index.html', search: '' },
            addEventListener() {},
            history: { replaceState() {} },
        },
        document: {
            getElementById(id) {
                return elements[id] || makeElement(ctx);
            },
            createElement() {
                return makeElement(ctx);
            },
        },
        requestAnimationFrame(callback) {
            rafCallback = callback;
            rafCount++;
        },
    };

    vm.createContext(sandbox);
    const read = (name) =>
        fs.readFileSync(path.join(__dirname, name), 'utf8');
    vm.runInContext(read('game.js'), sandbox);
    vm.runInContext(read('main.js'), sandbox);

    assert.ok(sandbox.SubHauler);
    assert.equal(typeof sandbox.window.subHaulerSpike.state, 'function');
    assert.equal(typeof rafCallback, 'function');

    rafCallback(16.67);

    const state = sandbox.window.subHaulerSpike.state();
    assert.ok(state.time > 0);
    assert.ok(state.x > sandbox.SubHauler.CFG.startX);
    assert.ok(calls.some((call) => call.prop === 'fillRect'));
    assert.match(elements.debug.textContent, /seed 1337/);
    assert.ok(rafCount >= 2);
});
