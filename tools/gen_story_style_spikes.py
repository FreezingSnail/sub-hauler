#!/usr/bin/env python3
"""Six preview-only native Benthos art directions; never pack these into FX.

Run through make gen. All marks are authored on a 128x64 integer pixel grid.
Contact-sheet labels sit outside the game cards; 4x previews use nearest pixels.
"""
from pathlib import Path

from PIL import Image, ImageDraw

from gen_panels import BG_COLOR, TEXT_COLOR, draw_text

W, B = TEXT_COLOR, BG_COLOR
OUTPUT = Path("docs/assets/story-style-spikes")


def palm(d, x, y, color, scale=1):
    """An open hand against glass, with separated fingers and a tapered cuff."""
    def box(a, b, c, e):
        d.rectangle((x + a * scale, y + b * scale,
                     x + c * scale, y + e * scale), fill=color)
    box(2, 7, 10, 13)
    for a, b, c, e in ((1, 4, 2, 9), (4, 1, 5, 9), (7, 0, 8, 9), (10, 3, 11, 10)):
        box(a, b, c, e)
    d.line((x + 3 * scale, y + 11 * scale, x - scale, y + 8 * scale,
            x - 2 * scale, y + 5 * scale), fill=color, width=2 * scale)
    d.polygon(((x + 3 * scale, y + 12 * scale), (x + 10 * scale, y + 12 * scale),
               (x + 13 * scale, y + 20 * scale), (x + 4 * scale, y + 20 * scale)), fill=color)


def hatch(d, box, color=W, gap=5):
    x0, y0, x1, y1 = box
    for intercept in range(x0 - (y1 - y0), x1 + 1, gap):
        points = [(intercept + y - y0, y) for y in range(y0, y1 + 1)
                  if x0 <= intercept + y - y0 <= x1]
        if points:
            d.line((*points[0], *points[-1]), fill=color)


def person(d, x, y, color=W):
    d.ellipse((x, y, x + 4, y + 4), fill=color)
    d.rectangle((x - 1, y + 5, x + 5, y + 10), fill=color)
    d.line((x, y + 10, x - 1, y + 13), fill=color, width=2)
    d.line((x + 4, y + 10, x + 5, y + 13), fill=color, width=2)


def engraving(d):
    # Dense, oblique steel planes and fine incised lines: a mechanical etching.
    d.polygon(((2, 15), (55, 10), (55, 43), (2, 43)), outline=W)
    hatch(d, (4, 17, 51, 40), gap=4)
    for x in (10, 29, 48):
        d.rectangle((x, 18, x + 4, 39), fill=B, outline=W)
        for y in (20, 36):
            d.point((x + 2, y), fill=W)
    d.rectangle((3, 28, 35, 33), fill=B, outline=W)
    for x in (7, 25):
        d.line((x, 27, x, 34), fill=W)
    d.polygon(((57, 13), (112, 10), (126, 16), (126, 43), (57, 43)), outline=W)
    hatch(d, (115, 19, 123, 41), gap=3)
    d.ellipse((62, 11, 112, 43), fill=W)
    d.ellipse((65, 13, 109, 42), fill=B)
    d.ellipse((69, 16, 105, 39), fill=W)
    hatch(d, (74, 34, 101, 37), B, 3)
    palm(d, 81, 18, B)
    d.arc((72, 18, 102, 37), 180, 255, fill=B)
    for x, y in ((85, 12), (63, 27), (111, 27), (86, 42)):
        d.point((x, y), fill=B)


def cinema(d):
    # Layered black foreground and a hard pool of light at the corridor's end.
    d.polygon(((51, 42), (83, 17), (115, 17), (124, 42)), fill=W)
    d.polygon(((56, 42), (85, 22), (112, 22), (120, 42)), fill=B)
    for y, x in ((14, 39), (18, 46), (22, 53), (26, 61)):
        d.line((x, y, x, 39, x + 5, 42), fill=W)
    d.rectangle((84, 15, 116, 37), fill=W)
    d.rectangle((87, 18, 113, 34), fill=B)
    d.rectangle((91, 20, 110, 32), fill=W)
    # The raised small hand is visible against the far light.
    person(d, 98, 22, B)
    d.line((103, 28, 107, 25, 107, 22), fill=B)
    for x, y in ((63, 38), (70, 35), (78, 33)):
        d.line((x, y, x + 4, y), fill=W)
    # Cropped near-foreground helmet and shoulder, with one thin rim highlight.
    d.ellipse((3, 12, 32, 38), fill=W)
    d.ellipse((6, 14, 33, 39), fill=B)
    d.polygon(((2, 43), (5, 33), (20, 30), (36, 36), (42, 43)), fill=W)
    d.polygon(((5, 43), (8, 35), (22, 33), (35, 38), (39, 43)), fill=B)
    d.line((12, 17, 25, 17), fill=W)
    d.line((2, 43, 124, 43), fill=W)


def intimate(d):
    # One large physical gesture; face and inner room are only partial clues.
    d.ellipse((25, 7, 112, 62), fill=W)
    d.ellipse((29, 9, 108, 59), fill=B)
    d.ellipse((33, 11, 104, 55), fill=W)
    # The scene crop makes the round porthole feel close, not a UI icon.
    d.ellipse((38, 15, 59, 37), fill=B)
    d.rectangle((38, 28, 59, 43), fill=B)
    d.polygon(((47, 21), (59, 20), (63, 25), (58, 28), (47, 28)), fill=B)
    d.line((53, 21, 57, 21), fill=W)
    d.line((48, 32, 53, 36), fill=W)
    palm(d, 68, 12, B, 2)
    d.line((35, 26, 41, 17, 48, 14), fill=B)
    d.line((98, 16, 103, 22), fill=B)
    for x, y in ((30, 29), (109, 29), (67, 10)):
        d.rectangle((x, y, x + 1, y + 1), fill=W)


def woodcut(d):
    # White-dominant poster: broad cut masses, little modeling, diagonal rhythm.
    d.rectangle((2, 11, 125, 43), fill=W)
    d.polygon(((2, 11), (56, 11), (37, 43), (2, 43)), fill=B)
    d.polygon(((103, 11), (125, 11), (125, 43), (114, 43)), fill=B)
    for x, y in ((11, 18), (24, 18), (7, 30), (20, 30)):
        d.polygon(((x, y), (x + 8, y), (x + 6, y + 7), (x - 2, y + 7)), fill=W)
    d.ellipse((51, 9, 106, 47), fill=B)
    d.ellipse((56, 13, 101, 44), fill=W)
    palm(d, 71, 16, B)
    d.polygon(((89, 32), (101, 43), (84, 43), (80, 33)), fill=B)
    d.line((59, 18, 64, 15), fill=B, width=2)
    d.line((105, 12, 112, 12), fill=B)
    d.line((108, 17, 114, 17), fill=B)


def diorama(d):
    # Three-dimensional pressure modules, sliced open to reveal a small community.
    def room(x, y, width=25, height=16):
        d.polygon(((x, y), (x + 9, y - 5), (x + width + 9, y - 5),
                   (x + width, y)), fill=W)
        d.line((x + 4, y - 1, x + width - 1, y - 1), fill=B)
        d.polygon(((x + width, y), (x + width + 9, y - 5),
                   (x + width + 9, y + height - 5), (x + width, y + height)), outline=W)
        for k in (3, 7):
            d.line((x + width + k, y + 1, x + width + k, y + height - 5), fill=W)
        d.rectangle((x, y, x + width, y + height), outline=W)
        d.line((x + 2, y + height - 2, x + width - 1, y + height - 2), fill=W)
        d.rectangle((x + 3, y + 3, x + 9, y + 8), fill=W)
        d.rectangle((x + 5, y + 4, x + 7, y + 7), fill=B)
    room(6, 25)
    room(44, 15)
    room(83, 26)
    d.line((32, 34, 45, 28), fill=W, width=2)
    d.line((70, 26, 83, 32), fill=W, width=2)
    person(d, 59, 17)
    d.line((64, 22, 67, 19, 67, 16), fill=W)
    # Tiny crew beside tables and a separate lower lit window.
    d.rectangle((17, 31, 24, 33), fill=W)
    d.line((20, 34, 20, 37), fill=W)
    d.rectangle((93, 30, 103, 36), fill=W)
    d.rectangle((96, 32, 99, 35), fill=B)
    d.line((51, 33, 46, 35, 51, 35, 46, 38, 51, 38, 47, 41, 65, 41), fill=W)
    d.line((84, 43, 116, 43), fill=W)


def blackwater(d):
    # Let darkness carry the scale. A handful of windows are the whole encounter.
    d.line((75, 27, 115, 27), fill=W)
    d.line((117, 29, 120, 36), fill=W)
    d.line((72, 37, 113, 37), fill=W)
    for x in (78, 88):
        d.rectangle((x, 30, x + 4, 33), fill=W)
    d.rectangle((99, 29, 108, 35), fill=W)
    d.rectangle((101, 31, 103, 35), fill=B)
    d.line((103, 33, 106, 31, 106, 29), fill=B)
    # The approaching hull is a thin crown highlight and three small portholes.
    d.line((12, 37, 17, 34, 32, 34, 37, 37), fill=W)
    d.rectangle((23, 31, 27, 33), outline=W)
    for x in (19, 25, 31):
        d.point((x, 38), fill=W)
    d.line((17, 41, 31, 41), fill=W)
    for point in ((48, 15), (61, 26), (44, 39), (12, 18), (120, 15)):
        d.point(point, fill=W)


STYLES = (
    ("01-industrial-etching", "1 INDUSTRIAL ETCH", engraving),
    ("02-cinematic-light", "2 CINEMATIC LIGHT", cinema),
    ("03-human-closeup", "3 HUMAN CLOSEUP", intimate),
    ("04-luminous-woodcut", "4 LUMINOUS WOODCUT", woodcut),
    ("05-settlement-diorama", "5 SETTLEMENT CUTAWAY", diorama),
    ("06-blackwater", "6 BLACKWATER", blackwater),
)


def card(painter):
    # Paint into a separate native plate so deliberate close crops never touch copy.
    plate = Image.new("RGBA", (128, 64), B)
    painter(ImageDraw.Draw(plate))
    image = Image.new("RGBA", (128, 64), B)
    image.paste(plate.crop((0, 11, 128, 44)), (0, 11))
    draw_text(image, 4, 1, "BENTHOS")
    draw_text(image, 4, 46, "A HAND AT THE GLASS.")
    draw_text(image, 3, 56, "A NEXT")
    assert image.size == (128, 64)
    assert {color for _count, color in image.getcolors()} == {W, B}
    return image


def dither_tone_scene(closeup=False, lowlight=False):
    """Native-size material/light values, prior to deterministic 1-bit exposure.

    The pressure ring has a directional metal highlight; the glass is backlit;
    the room recedes into darkness. These are tone fields, not filtered outlines.
    """
    from math import sqrt
    image = Image.new("L", (128, 64), 0)
    pixels = image.load()
    cx, cy, rx, ry = (68, 28, 32, 22) if closeup else (94, 25, 20, 16)
    for y in range(11, 44):
        for x in range(2, 126):
            dx, dy = (x - cx) / rx, (y - cy) / ry
            r = sqrt(dx * dx + dy * dy)
            value = 0
            if closeup:
                # Curved wall's grazing light fades toward the black water.
                value = max(0, 46 - abs(x - cx) * 0.8 - abs(y - 20) * 0.7)
            elif y > 28 and x > 33 + (43 - y) * 3:
                # Broad pool of reflected light on the corridor's floor.
                value = max(0, 105 - abs(x - 92) * 1.8 - abs(y - 31) * 4)
            if 1.02 < r < 1.28:
                value = 78 + max(0, -dx * 54 - dy * 81)
            elif 0.91 < r <= 1.02:
                value = 15
            elif r <= 0.91:
                # Lit glass has a bright crown and a graded lower reflection.
                value = 235 - max(0, dy) * 120 - max(0, dx) * 35
                value += max(0, 25 - abs(x - cx + 8) * 8)
            if lowlight:
                # Preserve luminous glass but expose hardware one stop darker.
                value = value * (0.62 if r > 0.91 else 0.92)
            pixels[x, y] = max(0, min(255, int(value)))
    d = ImageDraw.Draw(image)
    if closeup:
        # Inner occupant stays dark and solid, facing the raised palm.
        d.ellipse((43, 17, 56, 31), fill=0)
        d.polygon(((43, 28), (55, 27), (61, 43), (39, 43)), fill=0)
        d.polygon(((53, 20), (59, 24), (54, 26)), fill=0)
        d.point((53, 21), fill=255)
        palm(d, 70, 15, 0)
        d.arc((40, 13, 96, 47), 195, 242, fill=255)
        d.line((108, 14, 108, 40), fill=95)
        d.line((112, 14, 112, 40), fill=35)
        for x, y in ((32, 25), (99, 28), (48, 12), (88, 12)):
            d.rectangle((x, y, x + 1, y + 1), fill=200)
    else:
        # Repeated tunnel ribs and a near helmet give the light a human scale.
        for x, y in ((41, 12), (53, 16), (64, 20)):
            d.line((x, y, x, 37, x + 7, 42), fill=80)
            d.line((x + 2, y, x + 2, 36), fill=25)
        d.ellipse((5, 12, 33, 37), fill=145)
        d.ellipse((8, 14, 34, 38), fill=0)
        d.polygon(((2, 43), (7, 33), (23, 31), (37, 37), (42, 43)), fill=75)
        d.polygon(((5, 43), (10, 35), (24, 33), (35, 39), (38, 43)), fill=0)
        d.line((13, 17, 25, 17), fill=105)
        palm(d, 92, 17, 0)
        d.line((77, 40, 119, 40), fill=60)
    return image


def dither_expose(tone, pattern):
    """Expose native tones with an ordered screen or deterministic diffusion."""
    if pattern == "diffusion":
        return tone.convert("1", dither=Image.Dither.FLOYDSTEINBERG).convert("RGBA")
    bayer = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
    clustered = ((12, 5, 6, 13), (4, 0, 1, 7), (11, 3, 2, 8), (15, 10, 9, 14))
    matrix = clustered if pattern == "clustered" else bayer
    image = Image.new("RGBA", tone.size, B)
    src, dst = tone.load(), image.load()
    for y in range(11, 44):
        for x in range(128):
            if src[x, y] * 16 > (matrix[y % 4][x % 4] * 255 + 127):
                dst[x, y] = W
    return image


DITHER_STYLES = (
    ("01-ordered-cinema", "1 ORDERED CINEMA", False, False, "ordered"),
    ("02-clustered-glass", "2 CLUSTERED GLASS", True, False, "clustered"),
    ("03-diffused-cinema", "3 DIFFUSED CINEMA", False, False, "diffusion"),
    ("04-lowlight-glass", "4 LOWLIGHT GLASS", True, True, "ordered"),
)


def write_dither_studies():
    directory = OUTPUT / "dither"
    directory.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (2 * 144, 2 * 86), B)
    for index, (slug, label, closeup, lowlight, pattern) in enumerate(DITHER_STYLES):
        tone = dither_tone_scene(closeup, lowlight)
        plate = dither_expose(tone, pattern)
        image = Image.new("RGBA", (128, 64), B)
        image.paste(plate.crop((0, 11, 128, 44)), (0, 11))
        draw_text(image, 4, 1, "BENTHOS")
        draw_text(image, 4, 46, "A HAND AT THE GLASS.")
        draw_text(image, 3, 56, "A NEXT")
        assert {color for _count, color in image.getcolors()} == {W, B}
        image.save(directory / (slug + ".png"))
        image.resize((512, 256), Image.Resampling.NEAREST).save(directory / (slug + "-4x.png"))
        x, y = (index % 2) * 144 + 8, (index // 2) * 86
        draw_text(sheet, x, y, label)
        sheet.paste(image, (x, y + 12))
    sheet.save(directory / "contact-sheet.png")
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.Resampling.NEAREST).save(
        directory / "contact-sheet-4x.png")
    print("gen_story_style_spikes: 4 dither cards + 4 enlarged cards + 2 sheets (preview only)")


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (3 * 144, 2 * 86), B)
    for index, (slug, label, painter) in enumerate(STYLES):
        image = card(painter)
        image.save(OUTPUT / (slug + ".png"))
        image.resize((512, 256), Image.Resampling.NEAREST).save(OUTPUT / (slug + "-4x.png"))
        x, y = (index % 3) * 144 + 8, (index // 3) * 86
        draw_text(sheet, x, y, label)
        sheet.paste(image, (x, y + 12))
    sheet.save(OUTPUT / "contact-sheet.png")
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.Resampling.NEAREST).save(
        OUTPUT / "contact-sheet-4x.png")
    print("gen_story_style_spikes: 6 native cards, 6 4x cards, native + 4x labeled sheets (preview only)")


if __name__ == "__main__":
    main()
    write_dither_studies()
