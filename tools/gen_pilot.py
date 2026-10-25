#!/usr/bin/env python3
"""Bake the fixed 26x8 pilot silhouettes into plus-mask FX sprite sheets.

Each hull sheet holds four frames in this order: facing right unloaded,
facing right loaded, facing left unloaded, facing left loaded. The fill list is
the same ordered rectangle list the old procedural device renderer used, so
class pixels, facing marks, cargo stripes, and the shared collision footprint
stay identical. Pixels outside the union of the fills stay transparent; the
mask therefore clears only the black cut-outs that the procedural path wrote.

``gen_sprites.py`` packs these sheets with ``--shades 2`` into a separate
include, appended after every shipped FX symbol.
"""

import argparse
from pathlib import Path

from PIL import Image

WIDTH = 26
HEIGHT = 8
FRAMES = 4

# (x, y, width, height, white) rectangles in local, right-facing coordinates.
# Order matters where a later fill overwrites an earlier one.
HAULER = (
    (10, 0, 12, 2, True),
    (1, 2, 24, 1, True),
    (0, 3, 26, 2, True),
    (1, 5, 24, 1, True),
    (2, 6, 22, 1, True),
    (3, 7, 20, 1, True),
    (3, 3, 20, 1, False),
    (12, 0, 1, 2, False),
    (15, 0, 1, 2, False),
    (18, 0, 1, 2, False),
    (3, 4, 6, 3, False),
    (4, 4, 4, 1, True),
    (4, 6, 4, 1, True),
    (11, 4, 6, 3, False),
    (12, 4, 4, 1, True),
    (12, 6, 4, 1, True),
    (1, 4, 1, 2, False),
    (23, 5, 2, 1, False),
)
HAULER_LOADED = ((5, 4, 1, 3, True), (13, 4, 1, 3, True))

MINNOW = (
    (15, 1, 6, 1, True),
    (8, 2, 11, 1, True),
    (4, 3, 18, 2, True),
    (1, 5, 24, 1, True),
    (2, 6, 22, 1, True),
    (6, 7, 14, 1, True),
    (17, 1, 1, 1, False),
    (8, 4, 6, 3, False),
    (2, 5, 1, 2, False),
)
MINNOW_LOADED = ((10, 4, 1, 3, True),)

LEVIATHAN = (
    (9, 0, 14, 2, True),
    (3, 2, 22, 1, True),
    (0, 3, 26, 3, True),
    (1, 6, 24, 1, True),
    (2, 7, 22, 1, True),
    (3, 3, 20, 1, False),
    (12, 0, 1, 2, False),
    (18, 0, 1, 2, False),
    (2, 4, 5, 3, False),
    (18, 4, 5, 3, False),
)
LEVIATHAN_LOADED = ((4, 4, 1, 3, True), (20, 4, 1, 3, True))

HULLS = {
    "hauler": (HAULER, HAULER_LOADED),
    "leviathan": (LEVIATHAN, LEVIATHAN_LOADED),
    "minnow": (MINNOW, MINNOW_LOADED),
}


def render_frame(base, loaded):
    """Render one right-facing frame as an RGBA 26x8 image."""
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    pixels = image.load()
    for operation in base + loaded:
        x, y, width, height, white = operation
        color = (255, 255, 255, 255) if white else (0, 0, 0, 255)
        for py in range(y, y + height):
            for px in range(x, x + width):
                if 0 <= px < WIDTH and 0 <= py < HEIGHT:
                    pixels[px, py] = color
    return image


def render_sheet(name):
    base, loaded = HULLS[name]
    right_empty = render_frame(base, ())
    right_loaded = render_frame(base, loaded)
    left_empty = right_empty.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    left_loaded = right_loaded.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    sheet = Image.new("RGBA", (WIDTH * FRAMES, HEIGHT), (0, 0, 0, 0))
    for frame, image in enumerate((right_empty, right_loaded, left_empty, left_loaded)):
        sheet.paste(image, (frame * WIDTH, 0))
    return sheet


def write_sheets(output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    for name in sorted(HULLS):
        path = output_dir / "{}_26x8.png".format(name)
        render_sheet(name).save(path)
        paths.append(path)
    return paths


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="images/pilot")
    args = parser.parse_args(argv)
    paths = write_sheets(args.output)
    print("gen_pilot: wrote {}".format(", ".join(str(path) for path in paths)))


if __name__ == "__main__":
    main()
