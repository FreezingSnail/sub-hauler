#!/usr/bin/env python3
"""Compose two opaque startup frames from the original logo and game art."""
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

    game = Image.new("RGBA", (128, 64), "black")
    title = Image.new("RGBA", (60, 8), "black")
    draw_text(title, 0, 0, "SUB-HAULER")
    game.paste(title.resize((120, 16), Image.Resampling.NEAREST), (4, 5))
    pen = ImageDraw.Draw(game)
    # Rock ledges frame the established HAULER silhouette. All scenery is
    # baked here; the device only copies a complete native 1-bit frame.
    pen.line([(0, 27), (7, 27), (7, 30), (17, 30), (17, 33), (24, 33)], fill="white")
    pen.line([(127, 28), (119, 28), (119, 32), (109, 32), (109, 35), (103, 35)], fill="white")
    pen.line([(0, 50), (13, 50), (13, 48), (28, 48), (28, 51), (49, 51),
              (49, 52), (84, 52), (84, 50), (111, 50), (111, 47), (127, 47)], fill="white")
    ship = Image.open(root / "images/pilot/hauler_26x8.png").convert("RGBA").crop((0, 0, 26, 8))
    ship = ship.resize((52, 16), Image.Resampling.NEAREST)
    game.paste(ship, (38, 30), ship)
    for x, y in ((29, 34), (23, 29), (18, 39)):
        pen.rectangle((x, y, x + 2, y + 2), outline="white")
    draw_text(game, 7, 56, "PRESS A TO START")
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
