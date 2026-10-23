// Shared hull roster for pilot, cargo pack, and fleet views.
// Browser: globalThis.SubHaulerSubs. Node: CommonJS export.
(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports)
        module.exports = api;
    else
        root.SubHaulerSubs = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const LIST = [
        {
            code: 'SMALL',
            name: 'MINNOW',
            design: 'TADPOLE',
            style: 'teardrop',
            shape: 'rounded shuttle',
            length: 64,
            height: 16,
            sailW: 12,
            sailH: 6,
            hatches: [0.68],
            portholes: 3,
            vents: 3,
            doors: 1,
            cols: 6,
            rows: 3,
            holdW: 6,
            holdH: 3,
            capacity: 14,
            load: 12,
            pieces: 4,
            required: 3,
            bonus: 1,
            maxArea: 4,
            maxW: 3,
            maxH: 2,
            role: 'hex hold',
            cell: 7,
            side: 24,
            blocked: [
                [0, 0],
                [5, 0],
                [0, 2],
                [5, 2],
            ],
            tower: 'sail',
            towerX: 0.38,
            viewport: true,
            skids: true,
            pilotW: 24,
            pilotH: 7,
        },
        {
            code: 'MED',
            name: 'HAULER',
            design: 'FLATBED',
            style: 'flatdeck',
            shape: 'flat-deck freighter',
            length: 94,
            height: 22,
            sailW: 16,
            sailH: 8,
            hatches: [0.14, 0.4],
            portholes: 5,
            vents: 4,
            doors: 2,
            cols: 11,
            rows: 3,
            holdW: 11,
            holdH: 3,
            capacity: 28,
            load: 26,
            pieces: 7,
            required: 6,
            bonus: 1,
            maxArea: 6,
            maxW: 4,
            maxH: 3,
            role: 'split hold',
            cell: 6,
            side: 22,
            blocked: [
                [0, 0],
                [5, 0],
                [5, 1],
                [5, 2],
                [10, 0],
            ],
            tower: 'deckhouse',
            towerX: 0.58,
            pilotW: 26,
            pilotH: 8,
        },
        {
            code: 'LARGE',
            name: 'LEVIATHAN',
            design: 'FORTRESS',
            style: 'teardrop',
            shape: 'fortress heavy',
            length: 118,
            height: 30,
            sailW: 18,
            sailH: 9,
            hatches: [0.12, 0.7, 0.88],
            portholes: 6,
            vents: 4,
            doors: 3,
            cols: 14,
            rows: 4,
            holdW: 14,
            holdH: 4,
            capacity: 44,
            load: 40,
            pieces: 8,
            required: 6,
            bonus: 2,
            maxArea: 8,
            maxW: 4,
            maxH: 4,
            role: 'spine hold',
            cell: 5,
            side: 24,
            blocked: [
                [0, 0],
                [1, 0],
                [6, 0],
                [6, 1],
                [6, 2],
                [6, 3],
                [7, 0],
                [7, 1],
                [7, 2],
                [7, 3],
                [12, 0],
                [13, 0],
            ],
            tower: 'deckhouse',
            towerX: 0.5,
            outriggers: true,
            gantry: true,
            gantryX: 0.82,
            pilotW: 26,
            pilotH: 8,
        },
    ];

    let selected = 1;
    const listeners = new Set();

    function clampIndex(index) {
        const n = Math.round(Number(index));
        if (!Number.isFinite(n))
            return selected;
        return ((n % LIST.length) + LIST.length) % LIST.length;
    }

    function getSelected() {
        return selected;
    }

    function getSelectedSub() {
        return LIST[selected];
    }

    function setSelected(index) {
        const next = clampIndex(index);
        if (next === selected)
            return selected;
        selected = next;
        for (const listener of listeners)
            listener(selected);
        return selected;
    }

    function subscribe(listener) {
        if (typeof listener !== 'function')
            return function () {};
        listeners.add(listener);
        return function () {
            listeners.delete(listener);
        };
    }

    return {
        list: LIST,
        getSelected,
        getSelectedSub,
        setSelected,
        subscribe,
    };
});
