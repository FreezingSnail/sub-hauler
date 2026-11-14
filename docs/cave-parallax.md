# Regional cave backgrounds

Each authored scenery area has its own hand-drawn 256x128 native plate:

| Area | Background |
| --- | --- |
| Tidewall / Lantern Reaches | Submerged harbour modules, mooring pilings, hanging chains and cables |
| Kelp Galleries | Tall kelp stalks with broad leaves, rock arches and rooted outcrops |
| Broken Works | Braced gantries, elbow pipes, turbine wheels and hanging hooks |
| Glass Trench | Faceted basalt columns, mineral fractures and large crystal clusters |

The editable artwork is explicit native-pixel geometry in
[parallax_art.py](../tools/parallax_art.py). `make gen` applies an
artwork-anchored checker that turns alternating white pixels black, renders
the four [native PNGs](../images/parallax/README.md), and packs the resulting
dots into the FX background section. The dots scroll with the artwork.

The area map follows the `region` on rooms in
[levels/exploration.json](../levels/exploration.json). Every world tile belongs
to its nearest authored room, with source order breaking ties. This is separate
from music overrides: Pressure Boundary retains deep scenery even though its
music region is works. The view centre selects the background, independent of
travel history. Each plate repeats every 256x128 background pixels and moves at
half the camera speed in both axes; reversing the camera retraces it exactly.

The generator stores all eight vertical pixel phases in cart flash. Each row
has an index for every eight-column block, followed by an occupancy byte and
its nonzero native column bytes. The renderer seeks to the first visible block,
skips empty blocks and clips against the existing collision cache. This avoids
reading the whole drawing. Foreground terrain
white pixels survive, solid collision tiles remain exact, and opaque gantries,
equipment and the submarine draw afterward. Water landmarks are baked with a
two-pixel black seam, so a white tender, pump or wreck outline cannot merge
into the contour-cut cave wall or read as one continuous edge with the dotted
background. `make gen` also emits `foregroundOpaque`, one bit per landmark or
settlement art tile, plus `foregroundMask`, eight column bytes per world tile;
the renderer reads the mask bytes only for tiles marked opaque. Each mask byte
marks enclosed black pixels to restore. After the background pass the renderer
clears exactly those mask bits. White silhouettes and all surrounding water
keep their values, so the fill is tight to each feature's interior while the
dotted plates remain visible immediately outside. Invalid or mismatched
collision caches suppress background drawing.

`caveBackdrops` remains in its appended FX declaration position; earlier cart
labels keep their offsets. Background data and firmware are generated together
through `make gen`. The raw voyage checkpoint contract is unchanged.

Run `make preview-parallax` and open `build/parallax-preview/index.html`.
The four area choices each show the scrolling background alone, the original
cave, and their in-game composition, at native 128x64 and exact 4x enlargement.
Play, Step and Restart cover forward and reverse camera motion. The preview
starts paused and pauses when hidden. It uses the production C++ renderers;
HUD and time-based effects are omitted for visual comparison.

Host tests compare every rendered pixel against native art across all four
areas, source-wrap boundaries and camera remainders. Tool tests verify exact
packing, all indexed blocks, all shifted phases and authored room ownership.
Device `test_render` checks the actual cart decoder against four independent
native-image CRCs. The complete-flight probe covers all four areas and every Y
remainder with cold and warm caches, alongside its existing cues, notices,
movement and reversal cases: 938 frames, including 94 cold and 844 warm frames.
Resource, stack and timing measurements live in [platform.md](platform.md).
