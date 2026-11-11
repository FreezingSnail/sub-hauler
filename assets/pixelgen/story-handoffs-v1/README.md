# Story handoff artwork v1

Open [index.html](index.html) for native and 4× animated previews, all four
poses, and links to each pixelgen contact sheet. Human artistic review remains
**unreviewed**. Existing artwork is retained unchanged.

| Scene | Intended trigger | Four-frame RLE bytes |
| --- | --- | ---: |
| `tidewall-filters` | FIRST acceptance at Tidewall: pleated filters beside open hold | 2,417 |
| `deep-anchor-dispatch` | HOME acceptance at Deep Anchor: sealed records case and return supplies | 2,754 |
| `cormorant-core` | BEACON acceptance at Cormorant: sealed ridged beacon core | 2,528 |
| `restoration-ready` | Both joint-restoration variants: completed pump and running lift | 2,607 |

Each authoritative path is `<scene>/bundle/frames/000.png` through `003.png`.
All frames are opaque 128×64 1-bit PNGs, four distinct poses at 300 ms each.
Cargo remains on a dock cradle beside the open hold in enclosed underwater
bays. No people or burned-in text. Rounded white hulls retain three dark
portholes and black bellies. The machinery view is location-neutral.

The image-generation tool supplied raster anchors using the original loading
scene as a visual reference. `provenance.json` retains the exact prompts and
records that this tool exposes no raster seed. The attempted seeded local
backend could not access Metal in the sandbox; no failed asset is packaged.
`source/generated.png` is the raster, `source/conversion/` retains pixelgen
conversion inputs and provenance, and `source/native.png` is its native anchor.
`author.py` creates isolated native cargo/machinery layers and clean plates,
including removal of nonessential bilge grit below the dock. It does not
compose animation frames. Numeric recipe seeds identify deterministic native
poses and do not claim seeded raster generation.

Pixelgen renders the retained `source/recipe.json` using `pixelgen.motion/1`,
derives masks, and publishes each complete `bundle/`. Bundles are copied intact.
All manifests/reports and retained bundle inputs remain tool-authored.

## Verification

The existing sibling checkout CLI was used because the installed CLI lacks
`animate`; no package installs or model downloads were performed.

```sh
python3 assets/pixelgen/story-handoffs-v1/author.py
pixelgen animate motion render --recipe assets/pixelgen/story-handoffs-v1/tidewall-filters/source/recipe.json --out .pixelgen/new-filters-run
python3 assets/pixelgen/story-handoffs-v1/validate.py --pixelgen pixelgen --replay-root .pixelgen/new-handoffs-replay
```

Render each scene into a new transient run. The validation command runs, for
every scene: still check/replay/check, animation check/replay/check, and motion
check/replay/check. It checks byte-identical frames for both replays, conversion
replay `matched`, no failure, and four distinct poses. Retained exact JSON
outputs are in `<scene>/validation.json`; condensed measurements are in
[validation-summary.json](validation-summary.json). Every command exited 0.
Every reported hard validation was true. No shipping regeneration or firmware
build was performed by the artwork worker.

Native pixel changes over consecutive transitions, including the wrap:
filters **172/172/268/268**; dispatch **248/248/348/348**;
core **142/141/159/160**; restoration **57/58/58/57**.
The cargo lowers four pixels before returning; the fourth pose also translates
one pixel sideways. The completed pump rotor and lift cable marker run together.

## Integration budget

New art totals **10,306 B** under the production RLE encoder. After mapping the
new departures, obsolete `beacon-descent` (**1,810 B**) and `home-return`
(**1,805 B**) can leave the packed active story set while their source bundles
remain intact. Estimated net art growth: **6,691 B**, plus **16 B** for eight
additional frame offsets, leaving **1,485 B** from the prior **8,192 B** cart
headroom before text pipeline changes. These are measurements of art payloads,
not a combined cart fit claim. Final source mapping/regeneration and whole-image
measurement belong to integration.

Keep `delivery-loading` for the PUMP impeller departure and `repair-bay` for
FIRST's result request. Do not remove them from the active set merely because
FIRST departure and the joint restoration now use new artwork.
