# Area background drawings

These native 256x128 one-bit plates are rendered from the hand-authored shapes
in [tools/parallax_art.py](../../tools/parallax_art.py). Edit those shapes and
run `make gen` to regenerate the PNGs, FX layout and metadata together.

- [Tidewall harbour](tide.png): pressure modules, pilings, chains and cables.
- [Kelp Galleries](kelp.png): broad-leaf kelp, rock arches and rooted stones.
- [Broken Works](works.png): braced gantries, pipe circuits and turbines.
- [Glass Trench](deep.png): basalt columns and large crystal outcrops.

The game screens alternating white pixels to black for a dotted appearance.
The screen is anchored to the plate. Each plate scrolls at half camera speed
and is selected by the authored scenery area. The background-only and in-game
views are available through `make preview-parallax`; see
[the renderer notes](../../docs/cave-parallax.md).
