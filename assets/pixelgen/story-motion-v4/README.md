# Story motion and clarity pass

Fifteen native 128×64 monochrome loops, four poses at 300 ms each. The complete
pixelgen bundles are retained under each scene's `bundle/`; the game and
`docs/story-sketchbook.html` read the same native frames. GIFs are review aids.
The earlier v3 poses remain in `docs/assets/first-beat-animation/all-beats-v3`.

| Scenes | Action and clarity changes |
| --- | --- |
| arrival, pump-approach, lift-yard, beacon-descent, home-return | Rounded pressure hull with three separate dark ports and a dark belly; a one-pixel hull bob over a steady clean plate; staggered rising bubbles. |
| grow-tanks, delivery-water | Separate rising bubble lanes show fluid circulation without shaking the room or the plants. |
| repair-bay | A large hook sways inside the enclosed repair bay; the broken pump stays still. |
| pump-cracked-impeller | A more legible fracture and rising leak bubbles; the damaged rotor stays still. |
| delivery-loading | The crane lowers an impeller toward the sub's internal hold in an enclosed loading bay. |
| lift-cable | Two broad, frayed cable ends with a visible gap; the lower end sways independently of the housing. |
| lift-delivery-cable | Dock-side cable reel with rotating spokes and a steady outer rim. |
| lift-delivery-working | A freight carriage travels on two fixed rails; the original stationary carriage is removed from the clean plate. |
| beacon-lit | A beam sweeps across the water while the beacon structure remains fixed. |
| home-lights | The sub bobs gently and the station windows light in sequence. |

Exterior scenes are submerged. Dry enclosed rooms and moon pools are retained.
Cargo is inside the submarine; the reel beside the berth belongs to the dock.
Foreground silhouettes use broad solid shapes and sparse contour hatches;
the existing background shading is retained.

`tools/author_story_motion.py --out <new-authoring-directory>` authors the
background plates, isolated part art, mattes and `pixelgen.motion/1` recipes.
Pixelgen renders all motion; no project tool implements transforms or assembly.
Using the current pixelgen CLI, run for each recipe:

```sh
pixelgen animate motion render --recipe <scene>/recipe.json --out <new-run>
pixelgen animate motion check --manifest <new-run>/bundle/manifest.json
pixelgen animate motion replay --manifest <new-run>/bundle/manifest.json --out <new-replay>
pixelgen animate motion check --manifest <new-replay>/bundle/manifest.json
```

The installed launcher at the time of this pass predates `animate`. The current
sibling checkout was used with its source directory on `PYTHONPATH` and the
existing Python runtime reported by `pixelgen doctor`; no installation or
download was performed. Normal `make gen` consumes these retained bundles and
does not require pixelgen or a model.

All fifteen bundles passed render/check/replay/check with byte-identical replay
frames. Their reports retain **unreviewed** human review status. Agent inspection
and automated playback/pixel checks do not substitute for artistic approval.
