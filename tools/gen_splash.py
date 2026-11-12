#!/usr/bin/env python3
"""Compose two opaque startup frames from the original logo and story art."""
import json
from pathlib import Path

from PIL import Image, ImageDraw

from gen_panels import draw_text

ROOT = Path(__file__).resolve().parents[1]


def frames(root=ROOT):
    logo = Image.open(root / "images/splash/source/snailware_64x64.png").convert("L")
    if logo.size != (64, 64):
        raise ValueError("Snailware source must be 64x64")
    logo = logo.point(lambda p: 255 if p >= 128 else 0).convert("RGBA")
    brand = Image.new("RGBA", (128, 64), "black")
    brand.paste(logo, (32, 0))
    draw_text(brand, 1, 10, "SNAIL")
    draw_text(brand, 100, 10, "WARE")

    # Reuse the canonical first-arrival art, with quiet bands for the title and
    # button prompt. The sheet row is named by scenes.json rather than an index.
    source = root / "story/scenes.json"
    story = json.loads(source.read_text(encoding="utf-8"))
    row = story["art"]["rows"].index("arrival")
    with Image.open(source.parent / story["art"]["file"]) as sheet:
        game = sheet.crop((0, row * 64, 128, (row + 1) * 64)).convert("RGBA")
    pen = ImageDraw.Draw(game)
    pen.rectangle((0, 0, 127, 8), fill="black")
    pen.rectangle((0, 55, 127, 63), fill="black")
    draw_text(game, 34, 1, "SUB-HAULER")
    draw_text(game, 16, 56, "PRESS ANY BUTTON")
    return brand, game


def main():
    brand, game = frames()
    output = ROOT / "images/splash"
    output.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (256, 64), "black")
    sheet.paste(brand, (0, 0))
    sheet.paste(game, (128, 0))
    sheet.save(output / "startup_128x64.png")
    preview = ROOT / "build/splash-preview.png"
    preview.parent.mkdir(parents=True, exist_ok=True)
    sheet.convert("RGB").resize((1024, 256), Image.Resampling.NEAREST).save(preview)


if __name__ == "__main__":
    main()
