#!/usr/bin/env python3
"""Normalize the authored HAULER pilot sheet and emit host art.

``images/pilot/hauler_26x8.png`` is authored art: four 26-pixel-wide frames
packed left to right in this order:

    right empty, right loaded, left empty, left loaded

The source canvas may be taller than the 26x8 collision footprint. Fully
transparent rows above and below the union of the opaque pixels are trimmed,
and the normalized sheet is written to ``build/gen/pilot/hauler_<w>x<h>.png``
for ``gen_sprites.py``. The game keeps the 26x8 collision hull and bottom-aligns
the taller art, so a mast may extend above the hull.

The same normalized pixels are emitted as a host-only C++ bitmap. Host pixel
tests and the device therefore draw one authored source.

Run from ``make gen``; never edit the generated files.
"""

import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FRAME_WIDTH = 26
FRAMES = 4
MIN_HEIGHT = 8
SOURCE = ROOT / "images/pilot/hauler_26x8.png"
OUTPUT_DIR = ROOT / "build/gen/pilot"
HOST_HEADER = ROOT / "src/generated/pilot_host.hpp"

# Retired MINNOW/LEVIATHAN fill lists stay as archival provenance (the audio
# precedent). They are not rendered, packed or referenced by the game.
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


def load_frames(path=SOURCE):
    """Return the four authored RGBA frames, untrimmed."""
    path = Path(path)
    image = Image.open(path).convert("RGBA")
    expected_width = FRAME_WIDTH * FRAMES
    if image.width != expected_width:
        raise ValueError(
            f"{path}: expected {FRAMES} frames across, {expected_width} px wide; "
            f"got {image.width}x{image.height}"
        )
    if image.height < MIN_HEIGHT:
        raise ValueError(
            f"{path}: expected at least {MIN_HEIGHT} px of vertical art; "
            f"got {image.height}"
        )

    frames = []
    for frame in range(FRAMES):
        frames.append(image.crop((frame * FRAME_WIDTH, 0, (frame + 1) * FRAME_WIDTH, image.height)))
    return frames


def validate_frame(frame, path, number):
    """Require the plus-mask palette: transparent, opaque black, opaque white."""
    for pixel in frame.getdata():
        red, green, blue, alpha = pixel
        if alpha not in (0, 255):
            raise ValueError(f"{path} frame {number}: alpha must be 0 or 255, got {alpha}")
        if alpha == 0:
            continue
        if red not in (0, 255) or green != red or blue != red:
            raise ValueError(
                f"{path} frame {number}: opaque pixels must be black or white, "
                f"got ({red}, {green}, {blue})"
            )


def trim_bounds(frames, path):
    """Return the shared content bounds; every frame must span the full width."""
    top = None
    bottom = None
    for number, frame in enumerate(frames):
        opaque = frame.getbbox()
        if opaque is None:
            raise ValueError(f"{path} frame {number}: frame is fully transparent")
        left, first, right, last = opaque
        if left != 0 or right != FRAME_WIDTH:
            raise ValueError(
                f"{path} frame {number}: content must span the full {FRAME_WIDTH}px width; "
                f"got x={left}..{right - 1}"
            )
        top = first if top is None else min(top, first)
        bottom = last if bottom is None else max(bottom, last)
    height = bottom - top
    if height < MIN_HEIGHT:
        raise ValueError(
            f"{path}: trimmed art is {height}px tall; "
            f"the 26x8 collision hull needs at least {MIN_HEIGHT}px"
        )
    return top, bottom, height


def normalize(path=SOURCE):
    """Return a 104xH sheet trimmed to the shared opaque content."""
    path = Path(path)
    frames = load_frames(path)
    for number, frame in enumerate(frames):
        validate_frame(frame, path, number)

    top, bottom, height = trim_bounds(frames, path)
    normalized = Image.new("RGBA", (FRAME_WIDTH * FRAMES, height), (0, 0, 0, 0))
    trimmed = []
    for frame_number, frame in enumerate(frames):
        cropped = frame.crop((0, top, FRAME_WIDTH, bottom))
        normalized.paste(cropped, (frame_number * FRAME_WIDTH, 0))
        trimmed.append(cropped)

    for facing, empty in ((2, 0), (3, 1)):
        flipped = trimmed[empty].transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if list(flipped.getdata()) != list(trimmed[facing].getdata()):
            raise ValueError(
                f"{path}: frame {facing} must mirror frame {empty} for left-facing art"
            )
    if list(trimmed[0].getdata()) == list(trimmed[1].getdata()):
        raise ValueError(f"{path}: loaded frame must differ from empty frame")
    return normalized


def host_header(sheet):
    """Render the host-only packed data and mask arrays."""
    height = sheet.height
    pages = (height + 7) // 8
    data = [[[0 for _ in range(FRAME_WIDTH)] for _ in range(pages)] for _ in range(FRAMES)]
    mask = [[[0 for _ in range(FRAME_WIDTH)] for _ in range(pages)] for _ in range(FRAMES)]

    pixels = list(sheet.getdata())
    for frame in range(FRAMES):
        frame_x = frame * FRAME_WIDTH
        for page in range(pages):
            for x in range(FRAME_WIDTH):
                for bit in range(8):
                    y = page * 8 + bit
                    if y >= height:
                        break
                    red, _green, _blue, alpha = pixels[y * sheet.width + frame_x + x]
                    if alpha:
                        mask[frame][page][x] |= 1 << bit
                        if red >= 128:
                            data[frame][page][x] |= 1 << bit

    def array(name, values):
        lines = [f"constexpr uint8_t {name}[{FRAMES}][{pages}][{FRAME_WIDTH}] = {{"]
        for frame in range(FRAMES):
            lines.append("    {")
            for page in range(pages):
                row = ", ".join(str(value) for value in values[frame][page])
                lines.append(f"        {{{row}}},")
            lines.append("    },")
        lines.append("};")
        return "\n".join(lines)

    return "\n".join(
        [
            "// Generated by tools/gen_pilot.py. Do not edit.",
            "#pragma once",
            "",
            "#include <stdint.h>",
            "",
            f"constexpr uint8_t HAULER_ART_PAGES = {pages};",
            "",
            array("haulerHostData", data),
            "",
            array("haulerHostMask", mask),
            "",
        ]
    )


def write_art(source=SOURCE, output_dir=OUTPUT_DIR, header=HOST_HEADER):
    sheet = normalize(source)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"hauler_{sheet.width // FRAMES}x{sheet.height}.png"
    sheet.save(output)

    header = Path(header)
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(host_header(sheet), encoding="utf-8")
    return output, header


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default=str(SOURCE))
    parser.add_argument("--output", default=str(OUTPUT_DIR))
    parser.add_argument("--header", default=str(HOST_HEADER))
    args = parser.parse_args(argv)
    output, header = write_art(args.source, args.output, args.header)
    print(f"gen_pilot: normalized {args.source} -> {output} and {header}")


if __name__ == "__main__":
    main()
