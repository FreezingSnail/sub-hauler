# Detailed cave backdrop

Original submerged rock, coral and kelp artwork for the distant cave layer.
Reference direction: Porgy from UFO 50. This is newly generated artwork, with
no game screenshots or extracted game assets used as image inputs.

`source.png` was generated with the built-in image generator. The local
pixelgen model could not run because Metal was unavailable in this session.
Pixelgen converted the source into the authoritative native `final.png`:
128x64, opaque, one-bit black and white. `preview.png`, `manifest.json` and
`report.json` are retained unmodified from that conversion. Artistic review
is **unreviewed**.

Validation used `pixelgen check` on the original and replayed final PNGs;
both returned `hard_valid: true`. Replay returned `conversion_replay:
matched`, with no failure and byte-identical PNGs. The transient conversion
and replay runs are under `.pixelgen/runs/20261112-cave-parallax-v2-convert`
and `.pixelgen/runs/20261112-cave-parallax-v2-replay`.

`make gen` packs the native columns visited by the fixed distant-layer screen
into program flash. Source pixels are not regenerated or recolored by the
packing tool. The live renderer supplies the depth, occlusion and half-speed
camera movement.

## Final source prompt

Use case: stylized-concept. Asset type: a native monochrome scrolling backdrop
for an Arduboy underwater game. Create a 2:1 landscape pixel art texture,
intended to be reduced to exactly 128x64 pixels. Primary request: much more
detailed background environment, inspired by Porgy from UFO 50. Subject: a
distant, fully submerged cavern wall covering the scene, with broad irregular
stratified basalt slabs, shelves, cracks, pillars, pockets of coral and rooted
kelp. It should feel like a rich continuous underwater place behind a
submarine, not scattered symbols and not a foreground obstacle course.
Composition: rock faces occupy the full canvas with asymmetrical overlapping
masses; one broad diagonal ledge crosses the middle left, a deeply cracked
buttress on the right, thin kelp blades and little coral fans emerging from
crevices. Avoid a blank open central tunnel or empty center. Style: authentic
tiny one-bit game pixel art, crisp chunky pixels representing a 128x64 grid,
2 colors ONLY black and white, no gray or antialiasing. Approximately 80%
black, 20% white: sparse broken white contours, small white stipple clusters
and short highlights describe large dark rocks. Background subdued and dark,
no large white-filled regions; rich recognizable rock forms and varied detail
despite low contrast. Texture should continue through the image edges for
repeating scrolling. Constraints: no submarine, no fish, no characters, no
text, no letters, no UI, no border, no surface waterline, no sky, no bright
collision outlines, no vignette, no photographic realism. Opaque black
background.
