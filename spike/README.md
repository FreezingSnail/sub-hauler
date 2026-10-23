# Sub-hauler spike

**Superseded by `spike-trim/`** for the submarine-feel direction. Kept for
comparison.

Browser feel test for **Tight Haul**: an auto-forward cave flyer where cargo
makes the submarine heavier, taller, and harder to place. This is a design
probe, not firmware and not the final game.

## Run

Open `spike/index.html` directly in a browser. No server, build step, or
dependencies; the scripts are classic `<script>` tags so `file://` works.

Optional query parameters make A/B runs repeatable:

```
spike/index.html?seed=1337&crates=4
```

## Controls

| Input | Action |
| --- | --- |
| `←` / `→` or `A` / `D` | brake / throttle |
| `↑` / `↓` or `W` / `S` | rise / dive |
| `Space` | sonar ping (3 charges) |
| `1` / `2` / `3` | restart with 0 / 2 / 4 crates |
| `R` | restart same seed and cargo |
| `N` | new random cave |

Neutral throttle returns to cruise. There is no reverse.

## What the spike tests

- **Mass feel:** empty vs loaded sink rate, acceleration, top speed, and
  vertical response should be obvious without reading the debug panel.
- **Cargo profile:** every crate adds 2 px of collision height on deck, so a
  loaded sub is physically taller as well as heavier.
- **Fairness:** a perfect gap-follower clears every generated seed. The
  corridor is smooth and mines are wall-mounted so a minimum-gap cave stays
  passable.
- **Sonar readability:** mines are faint 1 px dots at range; a ping draws an
  expanding ring and promotes mines in radius to bright 3 px crosses for 5 s.

## Suggested pass criteria

Play the same seed at 0, 2, and 4 crates (the `1`/`2`/`3` keys keep the seed):

1. Loaded runs take visibly longer and demand active depth control.
2. Cargo height changes which gaps feel safe.
3. The cave never feels impossible; collisions read as mistakes.
4. Sonar has a clear cost and a clear payoff.

`node spike/bench.js` is a repeatable sanity check, not a substitute for play.
It reports delivery rate, hits, time, and integrity for perfect and
reaction-limited gap followers across 50 seeds. Mines are cleared there so it
measures the corridor and the sub, not the autopilot's aim.

## Tuning

All handling constants live in `CFG` and `targetsFor()` in `game.js`:

- `sink` 4 → 20 px/s across 0–4 crates.
- `maxSpeed` 88 → 68 px/s.
- `accel` 100 → 60 px/s²; `vertAccel` 150 → 78 px/s².
- `liftSpeed` 60 → 40 px/s.
- `damageSpeed` 14 px/s: only a loaded sub damages itself by resting on the
  floor, which is the intended "keep holding depth" pressure.
- `minGap` 26 px; `hullH` 8 px; each crate adds 2 px.

## Scope

In: one corridor, cargo mass/profile, wall mines, sonar, hull/cargo damage,
finish score, fixed-step loop, debug readout, A/B controls.

Out: economy, contracts, upgrades, menus, docking speed check, art polish,
audio, save data, Arduboy implementation.

## Tests

```sh
node --test spike/*.test.js
node spike/bench.js
```

`game.test.js` covers determinism, minimum gap, cargo handling deltas, sonar
charges, collision damage, passive loaded-sink damage, finish scoring, and
stopped-run behavior. `main.test.js` boots the real browser scripts against a
minimal DOM/canvas stub and renders one frame.
