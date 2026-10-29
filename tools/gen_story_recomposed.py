#!/usr/bin/env python3
"""New human-led, native dithered story compositions; preview only, via make gen.

Tone fields are authored at the final 128x45 illustration size. Directional
lighting models distinct planes; native diffusion adds intermediate tones.
No existing scene sprites, geometry, image resampling or runtime assets are used.
"""
from pathlib import Path
from math import hypot

from PIL import Image, ImageDraw

from gen_panels import BG_COLOR, TEXT_COLOR, draw_text

OUTPUT = Path("docs/assets/story-recomposed")
W, B = TEXT_COLOR, BG_COLOR


def plane(image, points, light, dark, axis="y", reverse=False):
    """A lit native material plane, bounded by a hard pixel silhouette."""
    mask = Image.new("1", image.size)
    ImageDraw.Draw(mask).polygon(points, fill=1)
    box = mask.getbbox()
    if not box:
        return
    x0, y0, x1, y1 = box
    dst, cover = image.load(), mask.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if cover[x, y]:
                t = (x - x0) / max(1, x1 - x0 - 1) if axis == "x" else (y - y0) / max(1, y1 - y0 - 1)
                if reverse:
                    t = 1 - t
                dst[x, y] = int(light + (dark - light) * t)


def ellipsoid(image, box, crown=225, shadow=30):
    mask = Image.new("1", image.size)
    ImageDraw.Draw(mask).ellipse(box, fill=1)
    dst, cover = image.load(), mask.load()
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for y in range(max(0, y0), min(image.height, y1 + 1)):
        for x in range(max(0, x0), min(image.width, x1 + 1)):
            if cover[x, y]:
                t = min(1, hypot((x - cx + 3) / max(1, (x1 - x0) / 2),
                                 (y - cy + 3) / max(1, (y1 - y0) / 2)))
                dst[x, y] = int(crown + (shadow - crown) * t)


def worker(d, x, y, ink=0, small=False, wave=False):
    if small:
        d.ellipse((x, y, x + 3, y + 3), fill=ink)
        d.rectangle((x, y + 4, x + 3, y + 8), fill=ink)
        d.line((x, y + 8, x - 1, y + 10), fill=ink)
        d.line((x + 3, y + 8, x + 4, y + 10), fill=ink)
        if wave:
            d.line((x + 3, y + 5, x + 6, y + 3, x + 6, y), fill=ink)
        return
    d.ellipse((x, y, x + 6, y + 6), fill=ink)
    d.polygon(((x - 1, y + 7), (x + 6, y + 7), (x + 8, y + 14), (x - 2, y + 14)), fill=ink)
    d.rectangle((x - 1, y + 14, x + 1, y + 18), fill=ink)
    d.rectangle((x + 5, y + 14, x + 8, y + 18), fill=ink)
    if wave:
        d.line((x + 6, y + 9, x + 11, y + 5, x + 11, y + 1), fill=ink, width=2)


def benthos():
    image = Image.new("L", (128, 45), 0)
    d = ImageDraw.Draw(image)
    # A domestic foreground frames a broad, oblique harbor window.
    plane(image, ((35, 0), (127, 0), (127, 39), (35, 35)), 85, 15)
    plane(image, ((42, 5), (127, 1), (127, 35), (42, 32)), 245, 100)
    # The visiting boat is outside: oblique stern-to-bow hull, rather than an icon.
    ellipsoid(image, (77, 11, 119, 27), 240, 40)
    d.polygon(((77, 15), (70, 11), (70, 24), (77, 23)), fill=25)
    d.rectangle((93, 6, 103, 13), fill=55)
    d.line((96, 5, 96, 3, 101, 3), fill=80)
    for x in (87, 97, 107):
        d.ellipse((x, 16, x + 3, 19), fill=0)
    d.line((84, 23, 112, 23), fill=15)
    d.line((45, 29, 125, 32), fill=65)
    # Someone has stopped at the glass, with a small hand against its light.
    worker(d, 54, 17, small=False, wave=True)
    d.line((56, 19, 58, 19), fill=190)
    # Near table, a mug and books establish an ordinary occupied room.
    plane(image, ((2, 36), (31, 31), (48, 36), (17, 43)), 90, 25)
    d.polygon(((3, 37), (17, 44), (17, 41), (4, 35)), fill=0)
    d.rectangle((21, 29, 26, 34), fill=210)
    d.arc((24, 29, 29, 33), 270, 90, fill=160)
    d.line((9, 34, 17, 32, 21, 34), fill=130)
    d.rectangle((6, 39, 9, 44), fill=0)
    # Recessed window frame is close and almost black against waterlight.
    d.line((38, 1, 38, 34, 126, 39), fill=0, width=4)
    d.line((40, 8, 40, 31), fill=155)
    d.line((119, 1, 119, 9), fill=60)
    return image


def pump():
    image = Image.new("L", (128, 45), 0)
    d = ImageDraw.Draw(image)
    # Deep service hall. Floor planes converge at the bright occupied opening.
    plane(image, ((21, 44), (62, 19), (91, 19), (118, 44)), 75, 5, reverse=True)
    plane(image, ((52, 44), (68, 23), (80, 23), (91, 44)), 175, 30, reverse=True)
    plane(image, ((42, 3), (65, 11), (65, 31), (43, 44)), 45, 110)
    plane(image, ((86, 10), (114, 0), (114, 44), (86, 30)), 90, 15)
    d.rectangle((65, 8, 87, 29), fill=205)
    d.rectangle((68, 11, 84, 28), fill=135)
    d.rectangle((69, 12, 82, 15), fill=240)
    worker(d, 71, 18, small=True)
    worker(d, 91, 25, ink=190, small=True)
    # Reflections are broad native bands, interrupted by the working water.
    for box, shade in (((64, 32, 80, 32), 170), ((59, 36, 83, 36), 95), ((60, 40, 85, 41), 145)):
        d.rectangle(box, fill=shade)
    d.line((50, 6, 67, 12), fill=170, width=2)
    d.line((53, 8, 53, 28), fill=70)
    d.line((61, 11, 61, 29), fill=110)
    # Curved foreground pipe feeds the newly live channel.
    ellipsoid(image, (109, -12, 139, 56), 115, 5)
    d.arc((99, 19, 120, 38), 170, 285, fill=185, width=3)
    d.line((101, 32, 94, 35), fill=170, width=2)
    # Returning crew member seen from behind, carrying a square tool bag.
    ellipsoid(image, (13, 16, 30, 32), 215, 0)
    d.polygon(((13, 28), (28, 27), (39, 44), (1, 44)), fill=0)
    d.line((29, 30, 35, 41), fill=210, width=2)
    d.rectangle((35, 36, 44, 43), fill=100)
    d.line((37, 35, 41, 35), fill=110)
    d.line((16, 19, 24, 18), fill=185)
    d.rectangle((10, 34, 14, 42), fill=35)
    return image


def lift():
    image = Image.new("L", (128, 45), 0)
    d = ImageDraw.Draw(image)
    # Looking up from the bay floor: heavy columns converge toward the gantry.
    plane(image, ((0, 0), (54, 0), (47, 11), (0, 37)), 35, 100)
    plane(image, ((128, 0), (93, 0), (96, 11), (127, 41)), 90, 15)
    plane(image, ((42, 2), (96, 2), (102, 37), (31, 37)), 165, 0)
    d.polygon(((0, 44), (9, 44), (57, 6), (53, 4)), fill=0)
    d.polygon(((119, 44), (128, 44), (94, 4), (91, 5)), fill=0)
    d.line((13, 38, 57, 7), fill=130)
    d.line((115, 35, 95, 8), fill=100)
    d.polygon(((46, 5), (97, 5), (100, 10), (42, 10)), fill=0)
    d.line((48, 5, 95, 5), fill=190)
    # Twin lifting lines disappear upward; load has lit top, side and underside.
    d.line((65, 10, 65, 18), fill=0, width=2)
    d.line((80, 10, 80, 18), fill=0, width=2)
    d.line((64, 10, 64, 18), fill=220)
    d.line((65, 16, 55, 23), fill=0)
    d.line((80, 16, 91, 23), fill=0)
    plane(image, ((53, 22), (75, 16), (95, 23), (72, 31)), 255, 210, axis="x")
    plane(image, ((53, 23), (72, 30), (72, 40), (53, 32)), 130, 60)
    plane(image, ((73, 30), (95, 23), (95, 32), (73, 40)), 200, 80)
    d.polygon(((53, 22), (75, 16), (95, 23), (95, 32), (73, 40), (53, 32)), outline=0, width=2)
    d.line((57, 23, 74, 19, 89, 24), fill=30)
    d.line((62, 22, 79, 28, 79, 36), fill=0, width=2)
    d.line((72, 31, 72, 39), fill=0)
    # Tiny signaler under the load gives its overhead scale.
    worker(d, 31, 33, ink=230, small=True, wave=True)
    d.line((10, 44, 49, 38), fill=80)
    d.line((102, 37, 127, 44), fill=60)
    return image


def beacon():
    image = Image.new("L", (128, 45), 0)
    d = ImageDraw.Draw(image)
    # A radio watchkeeper rather than a free-standing beacon icon.
    plane(image, ((0, 0), (127, 0), (127, 44), (0, 44)), 30, 0)
    plane(image, ((69, 0), (127, 0), (127, 27), (69, 24)), 90, 10)
    d.polygon(((75, 3), (125, 1), (125, 22), (75, 21)), fill=0)
    # Small answering lights beyond the glass; most of the water stays dark.
    for x, y in ((92, 14), (100, 13), (108, 14)):
        d.rectangle((x, y, x + 2, y + 1), fill=225)
    d.line((115, 16, 115, 8), fill=130)
    d.rectangle((113, 7, 117, 9), fill=255)
    d.point((111, 6), fill=120)
    d.point((120, 10), fill=140)
    # Lit console in the near plane, with a soft spill across the seated figure.
    plane(image, ((49, 29), (95, 25), (115, 36), (65, 43)), 180, 30)
    d.polygon(((54, 28), (81, 25), (86, 34), (58, 37)), fill=0)
    d.polygon(((58, 29), (77, 27), (81, 32), (61, 35)), fill=240)
    d.line((62, 31, 66, 31, 68, 29, 70, 33, 72, 30, 77, 30), fill=0)
    for x, y in ((89, 32), (96, 34), (104, 35)):
        d.ellipse((x, y, x + 2, y + 2), fill=10)
    # Operator's head, headset, forearm and chair overlap the illuminated desk.
    ellipsoid(image, (28, 13, 43, 28), 135, 0)
    d.polygon(((33, 26), (44, 26), (52, 37), (43, 44), (22, 44)), fill=0)
    d.line((40, 17, 44, 23, 46, 24), fill=165)
    d.arc((28, 12, 44, 29), 175, 320, fill=135)
    d.rectangle((40, 20, 43, 24), fill=220)
    d.line((44, 31, 56, 35, 63, 33), fill=105, width=3)
    d.rounded_rectangle((16, 28, 31, 44), radius=3, fill=0)
    d.line((17, 29, 17, 39), fill=65)
    # A mug and hanging cable keep the station inhabited rather than schematic.
    d.rectangle((104, 25, 108, 30), fill=90)
    d.arc((107, 26, 112, 29), 270, 90, fill=80)
    d.line((10, 10, 10, 21, 13, 26), fill=60)
    return image


def homecoming():
    image = Image.new("L", (128, 45), 0)
    d = ImageDraw.Draw(image)
    # Water-level beneath the pier; the berth and its people are above the bow.
    plane(image, ((0, 16), (127, 16), (127, 44), (0, 44)), 65, 0)
    plane(image, ((0, 0), (127, 0), (127, 14), (0, 4)), 10, 90)
    plane(image, ((39, 4), (108, 4), (115, 17), (32, 17)), 235, 150)
    d.rectangle((36, 17, 114, 20), fill=15)
    d.line((37, 18, 113, 18), fill=195)
    # Small welcoming silhouettes are lit from the harbor entrance behind them.
    worker(d, 54, 6, small=True, wave=True)
    worker(d, 75, 5, small=True, wave=True)
    worker(d, 94, 6, small=True)
    # Pilings and diagonal underside cross-braces recede toward the far berth.
    plane(image, ((8, 7), (18, 9), (24, 44), (8, 44)), 120, 10, axis="x")
    plane(image, ((113, 11), (121, 7), (120, 44), (105, 44)), 80, 0, axis="x")
    d.polygon(((0, 24), (0, 17), (43, 14), (43, 18)), fill=0)
    d.polygon(((125, 22), (128, 16), (105, 14), (104, 18)), fill=0)
    d.line((19, 43, 40, 20), fill=0, width=3)
    d.line((109, 43, 101, 20), fill=0, width=3)
    # A near, head-on pressure bow: crown light, dark visor, rounded ballast.
    ellipsoid(image, (43, 26, 91, 53), 245, 0)
    plane(image, ((59, 24), (75, 24), (78, 30), (55, 30)), 170, 55)
    d.rounded_rectangle((60, 25, 73, 28), radius=1, fill=0)
    for x, y in ((51, 33), (63, 32), (75, 33)):
        d.ellipse((x, y, x + 7, y + 7), fill=0)
        d.line((x + 2, y + 1, x + 4, y + 1), fill=230)
    d.line((49, 39, 45, 40), fill=175)
    d.line((83, 39, 87, 40), fill=175)
    # Broken reflections leave clear black water around the arriving bow.
    for x, y, width in ((26, 30, 9), (91, 29, 10), (29, 37, 9), (93, 38, 7)):
        d.line((x, y, x + width, y), fill=140)
    return image


SCENES = (
    ("01-benthos", "1 INSIDE BENTHOS", "BENTHOS", "A HAND AT THE GLASS.", benthos),
    ("02-pump", "2 RETURNING CREW", "PUMP REFUGE", "THE CREWS COME BACK.", pump),
    ("03-lift", "3 UNDER THE LOAD", "CORMORANT", "CRATES CLIMB AGAIN.", lift),
    ("04-beacon", "4 THE RADIO WATCH", "DEEP ANCHOR", "A LIGHT ANSWERS.", beacon),
    ("05-homecoming", "5 UNDER THE PIER", "TIDEWALL", "THEY KNOW YOUR HULL.", homecoming),
)


def card(title, caption, painter):
    tone = painter()
    # Exposure belongs to each scene's lighting, keeping shadow planes solid.
    exposure = {"BENTHOS": (32, 210), "PUMP REFUGE": (72, 210),
                "CORMORANT": (82, 210), "TIDEWALL": (72, 210)}
    if title in exposure:
        black, white = exposure[title]
        tone = tone.point([max(0, min(255, (value - black) * 255 // (white - black)))
                           for value in range(256)])
    image = Image.new("RGBA", (128, 64), B)
    plate = tone.convert("1", dither=Image.Dither.FLOYDSTEINBERG).convert("RGBA")
    image.paste(plate, (0, 0))
    ImageDraw.Draw(image).rectangle((0, 0, len(title) * 6 + 6, 9), fill=B)
    draw_text(image, 4, 1, title)
    draw_text(image, 4, 46, caption)
    draw_text(image, 3, 56, "A NEXT")
    assert image.size == (128, 64)
    assert {color for _count, color in image.getcolors()} == {W, B}
    return image


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (3 * 144, 2 * 86), B)
    for index, (slug, label, title, caption, painter) in enumerate(SCENES):
        image = card(title, caption, painter)
        image.save(OUTPUT / (slug + ".png"))
        image.resize((512, 256), Image.Resampling.NEAREST).save(OUTPUT / (slug + "-4x.png"))
        x, y = (index % 3) * 144 + 8, (index // 3) * 86
        draw_text(sheet, x, y, label)
        sheet.paste(image, (x, y + 12))
    sheet.save(OUTPUT / "contact-sheet.png")
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.Resampling.NEAREST).save(
        OUTPUT / "contact-sheet-4x.png")
    print("gen_story_recomposed: 5 new native scenes, 5 4x cards, 2 labeled sheets (preview only)")


if __name__ == "__main__":
    main()
