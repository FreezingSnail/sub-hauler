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

test('trim sandbox boots and renders a frame', () => {
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
            location: { href: 'file:///spike-trim/index.html', search: '' },
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

    assert.ok(sandbox.SubHaulerTrim);
    assert.equal(typeof sandbox.window.subHaulerTrim.state, 'function');
    assert.equal(typeof rafCallback, 'function');

    rafCallback(16.67);

    const state = sandbox.window.subHaulerTrim.state();
    assert.ok(state.time > 0);
    assert.ok(state.air < sandbox.SubHaulerTrim.CFG.startAir);
    assert.ok(calls.some((call) => call.prop === 'fillRect'));
    assert.match(elements.debug.textContent, /seed 2027/);
    assert.match(elements.debug.textContent, /ballast/);
    assert.ok(rafCount >= 2);

    const api = sandbox.window.subHaulerTrim;
    assert.equal(api.toggleMap(), true);
    rafCallback(33.34);
    assert.equal(api.mapOpen(), true);
    assert.equal(api.toggleMap(), false);
});

test('live cargo change keeps the sub inside the cave', () => {
    const { ctx } = makeCtx();
    const elements = {
        game: makeElement(ctx),
        overlay: makeElement(ctx),
        overlayText: makeElement(ctx),
        debug: makeElement(ctx),
    };
    let rafCallback = null;
    const sandbox = {
        console,
        Math,
        Number,
        Set,
        URL,
        URLSearchParams,
        performance: { now: () => 0 },
        window: {
            location: { href: 'file:///spike-trim/index.html', search: '' },
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
        },
    };
    vm.createContext(sandbox);
    const read = (name) =>
        fs.readFileSync(path.join(__dirname, name), 'utf8');
    vm.runInContext(read('game.js'), sandbox);
    vm.runInContext(read('main.js'), sandbox);

    const api = sandbox.window.subHaulerTrim;
    api.setCrates(4);
    const state = api.state();
    assert.equal(state.crates, 4);
    assert.equal(sandbox.SubHaulerTrim.overlaps(state), false);
});
