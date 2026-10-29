#!/usr/bin/env python3
"""Bake fixed pilot-HUD rows into opaque FX stamps.

The cleared framebuffer means HUD pixels must be written every frame. Every
fixed notice label is baked to remove repeated per-glyph cart/font transactions.
The retired control row stays in the generated set for cart-ABI compatibility
but the pilot frame no longer draws it. Dynamic amounts, air fill, tools, cargo
fills and the ballast notch stay native.

Glyphs come from the same table as gen_font/gen_panels, so baked pixels match
the external 6x8 font exactly. Text is read from fxdata.txt, the single
declaration source. Each stamp is page-aligned at y=56 (one 128-byte page).
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image

from gen_font import FRAME_HEIGHT, FRAME_WIDTH, GLYPHS

WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
ROW = (128, 8)
STRING_RE = re.compile(r"^string\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*'(?P<text>[^']*)'", re.M)

# Order is generated-file ABI only; hud.hpp maps symbols explicitly.
NOTICE_STRINGS = (
    "txt_survey_recorded",
    "txt_found_cutter",
    "txt_found_seal",
    "txt_found_thruster",
    "txt_found_credits",
    "txt_found_lens",
    "txt_survey_hint",
    "txt_salvage_hint",
    "txt_cut_hint",
    "txt_need_seal",
    "txt_need_thruster",
    "txt_need_cutter",
    "txt_repair_pump",
    "txt_repair_lift",
)


def load_strings(path):
    text = Path(path).read_text(encoding="utf-8")
    return {m.group("name"): m.group("text") for m in STRING_RE.finditer(text)}


def glyph_columns(char):
    # The external font leaves unsupported codes blank (for example '+'), so
    # mirror that instead of inventing a glyph the device would not draw.
    return GLYPHS.get(ord(char), (0, 0, 0, 0, 0))


def draw_text(image, x, y, text, color=WHITE):
    pixels = image.load()
    for index, char in enumerate(text):
        base = x + index * FRAME_WIDTH
        for column, bits in enumerate(glyph_columns(char)):
            for row in range(FRAME_HEIGHT):
                if bits & (1 << row):
                    px = base + column
                    py = y + row
                    if not (0 <= px < image.width and 0 <= py < image.height):
                        raise ValueError("HUD text {!r} leaves {}x{}".format(text, image.width, image.height))
                    pixels[px, py] = color


def text_row(text, x=3, y=0):
    image = Image.new("RGBA", ROW, BLACK)
    draw_text(image, x, y, text)
    return image


def stem(symbol):
    return symbol[len("txt_"):] if symbol.startswith("txt_") else symbol


def write_dir(directory, rows):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    expected = set()
    for name, image in rows:
        path = directory / (name + ".png")
        image.save(path)
        expected.add(path.name)
    for path in directory.glob("*.png"):
        if path.name not in expected:
            path.unlink()
    return [directory / (name + ".png") for name, _ in rows]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="images/hud")
    parser.add_argument("--fxdata", default="fxdata/fxdata.txt")
    args = parser.parse_args(argv)
    strings = load_strings(args.fxdata)
    # ABI-only retained stamp, composed at the original shipping positions so a
    # regeneration stays byte-identical. Nothing reads it at runtime.
    controls = Image.new("RGBA", ROW, BLACK)
    draw_text(controls, 3, 0, strings["txt_thrust"])
    draw_text(controls, 65, 0, strings["txt_trim"])
    sheet = Image.new("RGBA", (ROW[0] * len(NOTICE_STRINGS), ROW[1]), BLACK)
    for index, symbol in enumerate(NOTICE_STRINGS):
        sheet.paste(text_row(strings[symbol]), (ROW[0] * index, 0))
    rows = [("hud_controls_128x8", controls), ("hud_notices_128x8", sheet)]
    write_dir(args.output, rows)
    print("gen_hud: wrote {} HUD stamps ({} notice frames)".format(len(rows), len(NOTICE_STRINGS)))


if __name__ == "__main__":
    main()
