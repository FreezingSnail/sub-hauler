#!/usr/bin/env python3
"""Full-canvas native story art, and optional local captions; preview only.

Run via make gen. Scenes are drawn directly into 128x64 tone fields. Diffusion
models lit surfaces; shadow planes stay solid. There are no title/footer bands.
"""
from pathlib import Path

from PIL import Image, ImageDraw

from gen_panels import BG_COLOR, TEXT_COLOR, draw_text
from gen_story_recomposed import plane, ellipsoid

OUTPUT = Path("docs/assets/story-fullbleed")
W, B = TEXT_COLOR, BG_COLOR


def room():
    image = Image.new("L", (128, 64), 0)
    d = ImageDraw.Draw(image)
    # The liked room/window/person/visiting-hull staging, newly drawn full height.
    plane(image, ((35, 0), (127, 0), (127, 60), (35, 54)), 70, 0)
    plane(image, ((41, 5), (127, 1), (127, 55), (41, 49)), 255, 200)
    ellipsoid(image, (77, 19, 126, 41), 240, 15)
    d.polygon(((78, 25), (70, 19), (70, 37), (78, 34)), fill=15)
    d.rectangle((94, 12, 107, 23), fill=65)
    d.rectangle((97, 14, 103, 17), fill=0)
    d.line((100, 11, 100, 7, 106, 7), fill=95, width=2)
    for x in (86, 99, 112):
        d.ellipse((x, 27, x + 4, 31), fill=0)
    d.line((83, 37, 119, 37), fill=0)
    d.line((43, 48, 127, 52), fill=90)
    # Small person at the glass: full body and a distinctly raised palm.
    d.ellipse((53, 25, 61, 34), fill=0)
    d.polygon(((52, 35), (61, 35), (65, 49), (51, 49)), fill=0)
    d.line((61, 38, 69, 34, 69, 27), fill=0, width=3)
    d.line((67, 28, 71, 28), fill=0)
    d.rectangle((52, 49, 56, 61), fill=0)
    d.rectangle((60, 49, 64, 61), fill=0)
    d.line((55, 27, 58, 27), fill=150)
    # Domestic near plane: table and cup, beneath the much taller window.
    plane(image, ((0, 49), (26, 43), (43, 50), (11, 61)), 135, 20)
    d.polygon(((0, 50), (11, 59), (11, 64), (0, 54)), fill=0)
    d.rectangle((21, 39, 27, 46), fill=225)
    d.arc((25, 40, 31, 44), 270, 90, fill=160)
    d.line((7, 47, 14, 45, 18, 47), fill=150)
    d.line((37, 0, 37, 53, 127, 60), fill=0, width=4)
    d.line((40, 8, 40, 49), fill=165)
    return image


def refuge():
    image = Image.new("L", (128, 64), 0)
    d = ImageDraw.Draw(image)
    # One unmistakable destination. Most of the approach is quiet black.
    d.rounded_rectangle((76, 4, 123, 55), radius=5, fill=100)
    d.rounded_rectangle((80, 7, 119, 52), radius=3, fill=245)
    d.line((83, 11, 115, 11), fill=160)
    plane(image, ((79, 53), (119, 53), (127, 63), (49, 63)), 145, 0)
    d.line((79, 55, 120, 55), fill=230)
    # Large returning worker, helmet/rucksack/arms/boots readable in silhouette.
    ellipsoid(image, (19, 14, 43, 36), 240, 30)
    d.rounded_rectangle((23, 22, 41, 29), radius=2, fill=0)
    d.line((26, 23, 36, 23), fill=145)
    d.polygon(((19, 34), (43, 33), (50, 51), (43, 55), (18, 55), (12, 47)), fill=185)
    d.polygon(((22, 36), (39, 36), (43, 53), (20, 53)), fill=0)
    d.rounded_rectangle((22, 37, 35, 49), radius=2, fill=95)
    d.line((25, 39, 33, 39), fill=190)
    d.rectangle((20, 54, 27, 62), fill=140)
    d.rectangle((36, 54, 44, 62), fill=140)
    d.line((19, 63, 29, 63), fill=230)
    d.line((36, 63, 46, 63), fill=230)
    d.line((46, 40, 51, 49), fill=230, width=3)
    d.rectangle((48, 49, 59, 57), fill=115)
    d.line((51, 47, 56, 47), fill=190)
    # A second, smaller crew member is already crossing the doorway's light.
    d.ellipse((85, 19, 92, 26), fill=0)
    d.polygon(((84, 27), (94, 27), (97, 39), (82, 39)), fill=0)
    d.line((85, 38, 83, 47), fill=0, width=3)
    d.line((93, 38, 96, 47), fill=0, width=3)
    d.line((93, 29, 100, 25, 100, 21), fill=0, width=2)
    return image


def freight():
    image = Image.new("L", (128, 64), 0)
    d = ImageDraw.Draw(image)
    # A single isolated cable and hook, with no competing roof texture.
    d.line((64, 0, 64, 13), fill=255, width=2)
    d.arc((61, 11, 68, 20), 0, 245, fill=240, width=2)
    d.line((64, 20, 37, 27), fill=235)
    d.line((65, 20, 91, 26), fill=235)
    # Large, broad front face; the small side face explains a physical crate.
    plane(image, ((32, 27), (82, 27), (82, 47), (32, 47)), 225, 140)
    plane(image, ((83, 27), (96, 22), (96, 42), (83, 47)), 130, 50)
    plane(image, ((32, 26), (46, 22), (96, 22), (82, 26)), 255, 210)
    d.rectangle((33, 28, 81, 46), outline=0, width=2)
    for x in (41, 71):
        d.rectangle((x, 27, x + 4, 47), fill=0)
        d.line((x + 1, 29, x + 1, 44), fill=100)
    d.line((48, 30, 67, 44), fill=0, width=2)
    d.line((67, 30, 48, 44), fill=0, width=2)
    d.line((88, 26, 88, 42), fill=0, width=2)
    # Small signaler and distant floor lights leave real air beneath the load.
    d.ellipse((15, 45, 20, 50), fill=240)
    d.rectangle((14, 51, 21, 58), fill=200)
    d.line((15, 58, 13, 63), fill=230, width=2)
    d.line((20, 58, 23, 63), fill=230, width=2)
    d.line((21, 52, 27, 48, 27, 43), fill=240, width=2)
    d.line((5, 63, 39, 63), fill=75)
    d.line((101, 61, 125, 61), fill=70)
    return image


def radio():
    image = Image.new("L", (128, 64), 0)
    d = ImageDraw.Draw(image)
    # Readable operator profile, a large headset and an extended arm at the set.
    d.rectangle((85, 3, 125, 20), outline=90)
    for x, y in ((94, 13), (103, 12), (113, 13)):
        d.rectangle((x, y, x + 2, y + 1), fill=230)
    ellipsoid(image, (19, 10, 45, 34), 210, 20)
    d.polygon(((40, 19), (48, 24), (43, 27), (40, 27)), fill=180)
    d.arc((17, 8, 44, 35), 160, 305, fill=245, width=2)
    d.rounded_rectangle((37, 19, 44, 30), radius=2, fill=245)
    d.rectangle((39, 22, 42, 27), fill=0)
    d.line((43, 28, 49, 31, 53, 29), fill=220)
    plane(image, ((23, 34), (42, 33), (52, 52), (17, 60)), 100, 0)
    d.line((40, 36, 53, 43, 67, 38), fill=215, width=5)
    d.line((43, 39, 53, 46, 66, 40), fill=0)
    d.ellipse((65, 35, 71, 40), fill=235)
    # A console, not an abstract floating waveform: screen, knobs and casing.
    plane(image, ((62, 25), (116, 25), (123, 46), (54, 46)), 155, 45)
    d.rounded_rectangle((67, 28, 98, 39), radius=1, fill=245)
    d.line((70, 34, 75, 34, 78, 30, 82, 37, 87, 32, 94, 32), fill=0)
    for x in (104, 113):
        d.ellipse((x - 2, 31, x + 2, 35), fill=0)
        d.point((x, 31), fill=240)
    d.line((59, 46, 123, 46), fill=205)
    # Hand is on top of the left console edge, in front of its glow.
    d.line((50, 42, 65, 38), fill=220, width=4)
    d.ellipse((64, 35, 70, 40), fill=240)
    d.rounded_rectangle((9, 35, 20, 62), radius=3, fill=80)
    d.line((10, 37, 10, 55), fill=160)
    return image


def harbor():
    image = Image.new("L", (128, 64), 0)
    d = ImageDraw.Draw(image)
    # Welcoming quay above the arriving hull; no busy water or distorted bow.
    plane(image, ((65, 0), (127, 0), (127, 19), (65, 19)), 245, 175)
    d.rectangle((60, 19, 127, 23), fill=55)
    d.line((61, 20, 127, 20), fill=240)
    for x in (81, 105):
        d.ellipse((x, 3, x + 5, 8), fill=0)
        d.rectangle((x, 9, x + 5, 15), fill=0)
        d.line((x, 15, x - 1, 19), fill=0, width=2)
        d.line((x + 5, 15, x + 6, 19), fill=0, width=2)
        d.line((x + 5, 11, x + 10, 7, x + 10, 3), fill=0, width=2)
    d.rectangle((120, 23, 124, 59), fill=70)
    d.line((120, 24, 120, 51), fill=145)
    # The rounded submarine is intentionally broad and unmistakable at 1x.
    ellipsoid(image, (21, 27, 108, 48), 240, 20)
    d.polygon(((24, 31), (12, 27), (16, 37), (12, 46), (25, 43)), fill=155)
    d.line((11, 31, 11, 42), fill=215)
    plane(image, ((48, 18), (65, 18), (71, 28), (45, 28)), 220, 100)
    d.rectangle((51, 20, 62, 24), fill=0)
    d.line((56, 17, 56, 12, 63, 12), fill=190, width=2)
    for x in (38, 58, 78):
        d.ellipse((x, 32, x + 8, 40), fill=0)
        d.line((x + 2, 33, x + 5, 33), fill=245)
    d.line((32, 44, 96, 44), fill=0)
    d.line((41, 50, 84, 50), fill=125)
    d.line((101, 46, 110, 46), fill=185)
    d.line((2, 51, 19, 51), fill=95)
    d.line((109, 55, 124, 55), fill=95)
    return image


SCENES = (
    ("01-benthos", "1 BENTHOS", room, ("A HAND", "AT THE", "GLASS."), "left"),
    ("02-pump", "2 PUMP REFUGE", refuge, ("THE CREWS", "COME BACK."), "right"),
    ("03-lift", "3 CORMORANT LIFT", freight, ("CRATES CLIMB", "AGAIN."), "right"),
    ("04-beacon", "4 RADIO WATCH", radio, ("A LIGHT", "ANSWERS."), "right"),
    ("05-homecoming", "5 TIDEWALL", harbor, ("THEY KNOW", "YOUR HULL."), "left"),
)


def render(painter):
    tone = painter()
    # Crush low shadow values; screen only the lit surfaces and their falloff.
    tone = tone.point([max(0, min(255, (value - 40) * 255 // 185)) for value in range(256)])
    image = tone.convert("1", dither=Image.Dither.FLOYDSTEINBERG).convert("RGBA")
    assert image.size == (128, 64)
    assert {color for _count, color in image.getcolors()} == {W, B}
    return image


def captioned(image, rows, corner):
    image = image.copy()
    width, height = max(map(len, rows)) * 6 + 4, len(rows) * 8
    x, y = (2 if corner == "left" else 126 - width), 64 - height
    ImageDraw.Draw(image).rectangle((x, y, x + width - 1, 63), fill=B)
    for index, row in enumerate(rows):
        draw_text(image, x + 2, y + index * 8, row)
    return image


def write_set(directory, cards):
    directory.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (3 * 144, 2 * 86), B)
    for index, (slug, label, image) in enumerate(cards):
        image.save(directory / (slug + ".png"))
        image.resize((512, 256), Image.Resampling.NEAREST).save(directory / (slug + "-4x.png"))
        x, y = (index % 3) * 144 + 8, (index // 3) * 86
        draw_text(sheet, x, y, label)
        sheet.paste(image, (x, y + 12))
    sheet.save(directory / "contact-sheet.png")
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.Resampling.NEAREST).save(
        directory / "contact-sheet-4x.png")


def main():
    primary, optional = [], []
    for slug, label, painter, rows, corner in SCENES:
        image = render(painter)
        primary.append((slug, label, image))
        optional.append((slug, label, captioned(image, rows, corner)))
    write_set(OUTPUT, primary)
    write_set(OUTPUT / "caption", optional)
    print("gen_story_fullbleed: 5 image-only + 5 optional-caption cards, native/4x, 4 sheets (preview only)")


if __name__ == "__main__":
    main()
