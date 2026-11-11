# Editing story cards

Edit these two source files, then run `make gen`:

- [scenes.json](scenes.json): story text, scene/card order and named artwork rows.
- [art.png](art.png): the canonical artwork sheet for Piskel.

Generation reads these files and never rewrites them. The retained pixelgen
bundles under `assets/pixelgen/` document the original artwork; edits to those
old frame PNGs no longer affect the game.

## Piskel artwork

The sheet is **512×1216 pixels**, arranged as **four columns × nineteen rows**.
Every cell is **128×64 pixels**. Each row is one illustration, and its columns
are animation poses 0, 1, 2 and 3. The game shows each pose for 300 ms.

Open `art.png` as a full image to edit the entire sheet, or import it as a
sprite sheet using frame width **128**, frame height **64**, and zero spacing
or margins. This creates 76 frames in row order. When exporting those frames,
use PNG sprite-sheet export with **four columns**, scale **1×**, no spacing,
and no padding. Replace `story/art.png`; retain its 512×1216 dimensions.

Use only opaque **#000000** and **#ffffff**. Piskel's RGB/RGBA PNG exports are
accepted; transparency and intermediate colors are rejected rather than
silently changed. Do not add labels, guides or borders inside the sheet.

The `art.rows` array in `scenes.json` names rows from top to bottom. For example,
row zero is `tidewall-filters`; its pixels are y=0 through y=63. Pose zero occupies
x=0 through x=127, pose one x=128 through x=255, and so on. Keep this row order
when exporting. To add a new illustration, append a name and a complete row of
four poses together. Do not rearrange the existing names without moving their
pixels too.

## Story order

The sheet follows the pump-first story route from top to bottom. The pump and
lift branches remain playable in either order. Rows are:

| Rows (counting from 1) | Beat |
| --- | --- |
| 1 | Tidewall departure: filters |
| 2–4 | Benthos arrival, grow tanks, repair request |
| 5–6 | Pump survey |
| 7–8 | Impeller pickup and restored water |
| 9–10 | Lift survey |
| 11–12 | Cable pickup and working lift |
| 13 | Both repairs complete (shared by the two seal variants) |
| 14–15 | Beacon core pickup and Deep Anchor beacon |
| 16 | Deep Relay wreck, found by the survey lens |
| 17 | Tidewall homecoming lights |
| 18 | DEEP RELAY briefing: beacon call and console |
| 19 | HOME departure: loaded sub leaving Deep Anchor |

The complete generated card sheet follows the same narrative sequence, with
text pages between the illustrations. Both alternative seal variants are
included for inspection; the game chooses one according to seal ownership.
The opening instruction is the first card: it reuses the `arrival` row and
points the player to Benthos. DEEP RELAY follows BEACON and precedes HOME.
DEEP RELAY acceptance uses the briefing row, the site result keeps the wreck
row, and HOME departure uses its own loaded-sub row; PUMP pickup remains the
only user of `delivery-loading`.

## JSON text

The version-1 document contains `art` and `scenes`. Each scene has a stable
`name`, a preview `title`, and ordered `cards`:

```json
{
  "name": "pump-pickup",
  "title": "PUMP pickup",
  "cards": [
    { "kind": "image", "art": "delivery-loading" },
    {
      "kind": "text",
      "paragraphs": [
        "Load the impeller inside the hull. Deliver to Benthos.",
        "Its crew services Pump Refuge."
      ]
    }
  ]
}
```

Edit paragraph strings directly. Each string is one paragraph; use additional
strings for paragraph breaks. The generator wraps text and creates all needed
text pages automatically. Do not insert manual card breaks to make text fit.
Cards may be images or text in any order. An image name refers to a row in
`art.rows`; shared illustrations use the same row.

The native font renders uppercase letters. Input supports A–Z, a–z, digits,
spaces, period, comma, colon, slash, hyphen and straight apostrophe. Generation
reports unsupported characters and words wider than twenty columns. Use valid
JSON: double quotes and no comments or trailing commas. Scene names map to
stable numeric IDs used by saves; append a new scene name without renumbering
existing IDs. The JSON scene array
sets the reading order for the complete card sheet independently of those IDs.

## Generated previews and game data

After `make gen`:

- Open [the sketchbook](../docs/story-sketchbook.html) for scene navigation,
  animation, and all text pages at native/enlarged size.
- Open [the complete card sheet](../docs/assets/story-cards.png) to see all
  cards joined into one image. It is 512 pixels wide, with one row per card;
  image cards have four poses and text pages use column zero. The remaining
  cells on text rows are black.
- [The card-sheet map](../docs/assets/story-cards.json) identifies each row's
  scene and card. This sheet and map are generated inspection outputs; edit
  the canonical art sheet and JSON instead.

The same native pixels feed the browser and `fxdata/tables/story-beats.bin`.
`make gen-check` checks repeatability. Further edits must still fit the cart:
the current padded image is **676,352 B**, leaving **1,420,800 B** below 2 MiB.
Pagination preserves all text; it does not remove the device's storage and
card/frame-count limits.
