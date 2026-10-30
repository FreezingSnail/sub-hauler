# Underwater setting audit

2026-10-30. Source images visually inspected before edits. Built-in imagegen used; prompts/provenance in JSON files. Existing approved assets remain untouched.

- Keep pump-survey/approach-internal-v2.png and cracked-impeller.png: fully submerged machinery and submarine.
- Keep pump-delivery/loading-impeller-balanced-v4.png: accepted underwater-station moon pool; cargo enters internal hold.
- Keep pump-delivery/water-restored.png: enclosed pressurized hydroponics interior.
- Keep lift-survey/freight-yard-balanced-v3.png and ruined-cable-clear-sub-v3.png: submerged industrial structures.
- Keep beacon-delivery/descent.png and beacon-lit.png: submerged seabed station, no surface or sky.
- Replace lift-delivery/loading-cable.png with loading-cable.png here: enclosed pressure-bay moon pool; one bold reel on dock cradle next to internal cargo hatch. Second imagegen edit enlarged cable wrap bands and white porthole surrounds to survive native conversion.
- Replace lift-delivery/lift-restored.png with lift-restored.png here: removed reflective surface and open garage, added rocky seabed and sealed pressure door.

Native conversion: `node docs/assets/story-underwater-support-v7/render-native.cjs loading-cable lift-restored`. Each native is 128×64 binary grayscale; each 4x preview is 512×256 nearest-neighbor. Cable white/black pixels: 2771/5421. Lift: 1058/7134. Sources, native outputs and 4x previews visually inspected. No game, generated shipping set, HTML or animation files changed.
