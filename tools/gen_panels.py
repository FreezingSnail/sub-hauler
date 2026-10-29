#!/usr/bin/env python3
"""Render the fixed story panels and restoration overlays as source art.

The external 6x8 font already owns the glyphs. This tool bakes text that never
changes into opaque 128x64 FX images, so the device draws one panel instead of
repeating cursor/string calls. Selection variants bake the inverted row bar too;
the renderer only picks an image.

Restoration sprites are plus-mask blobs (transparent background) for the survey
ring, inactive mooring, pump flow and lift cable. Dynamic labels, rewards,
status and clocks stay native text.

Panel text is resolved from ``fxdata/fxdata.txt`` so the declarations remain the
single source of truth. The output is deterministic: fixed specs, fixed glyphs,
and no timestamps.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image

from gen_font import FRAME_HEIGHT, FRAME_WIDTH, GLYPHS

SIZE = (128, 64)
TEXT_COLOR = (255, 255, 255, 255)
BG_COLOR = (0, 0, 0, 255)
CLEAR = (0, 0, 0, 0)

STRING_RE = re.compile(r"^string\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*'(?P<text>[^']*)'", re.M)
MISSION_BLOCK_RE = re.compile(r"kMissions\[kMissionCount\]\s*=\s*\{(?P<body>.*?)\};", re.M | re.S)
MISSION_ENTRY_RE = re.compile(r"\{\s*(?P<from>\w+)\s*,\s*(?P<to>\w+)\s*,\s*true\s*,")
ROUTE_BLOCK_RE = re.compile(r"constexpr uint8_t ROUTE_REWARD\[ROUTE_COUNT\]\s*=\s*\{(?P<body>.*?)\};", re.M | re.S)

# Cargo missions in kMissions order. The route pair and reward are read from
# campaign.hpp and generated level.hpp; only the label/purpose symbol names and
# the generated panel stem stay here.
MISSIONS = (
    ("kFirst", "first", "txt_first", "txt_first_purpose"),
    ("kPump", "pump", "txt_pump", "txt_pump_purpose"),
    ("kLift", "lift", "txt_lift", "txt_lift_purpose"),
    ("kBeacon", "beacon", "txt_beacon", "txt_beacon_purpose"),
    ("kHome", "home", "txt_home", "txt_home_purpose"),
)
BERTH_NAMES = {
    "kTidewall": "txt_tidewall",
    "kBenthos": "txt_benthos",
    "kCormorant": "txt_cormorant",
    "kDeepAnchor": "txt_deep_anchor",
}
SETTLEMENT_COUNT = 4

# The game font intentionally keeps only the glyphs its text used before the
# story panels. The standard Arduboy 5x7 question mark is needed by the two
# confirmation prompts, so bake it here instead of changing the shared font
# image (its frame count and FX offsets are part of the shipped layout).
EXTRA_GLYPHS = {
    0x3F: (0x02, 0x01, 0x59, 0x09, 0x06),
    0x2E: (0x00, 0x00, 0x00, 0x00, 0x60),
    0x2C: (0x00, 0x00, 0x00, 0x60, 0x40),
}


def load_strings(path):
    text = Path(path).read_text(encoding="utf-8")
    return {match.group("name"): match.group("text") for match in STRING_RE.finditer(text)}


def load_routes(campaign_path):
    """Return the five cargo mission (from, to) berth pairs in mission order."""
    text = Path(campaign_path).read_text(encoding="utf-8")
    block = MISSION_BLOCK_RE.search(text)
    if not block:
        raise ValueError("campaign.hpp: kMissions table not found")
    entries = MISSION_ENTRY_RE.findall(block.group("body"))
    if len(entries) != len(MISSIONS):
        raise ValueError("campaign.hpp: expected {} cargo missions, found {}".format(len(MISSIONS), len(entries)))
    return entries


def load_rewards(level_path):
    """Read the generated host mirror of the route reward economy."""
    text = Path(level_path).read_text(encoding="utf-8")
    block = ROUTE_BLOCK_RE.search(text)
    if not block:
        raise ValueError("level.hpp: ROUTE_REWARD host mirror not found")
    values = [int(value) for value in re.findall(r"\d+", block.group("body"))]
    if len(values) != SETTLEMENT_COUNT * SETTLEMENT_COUNT:
        raise ValueError("level.hpp: expected {} route rewards, found {}".format(SETTLEMENT_COUNT * SETTLEMENT_COUNT, len(values)))
    return values


def glyph_columns(char):
    try:
        code = ord(char)
        if code in EXTRA_GLYPHS:
            return EXTRA_GLYPHS[code]
        return GLYPHS[code]
    except KeyError as exc:
        raise ValueError("panel text uses unsupported character {!r}".format(char)) from exc


def draw_text(image, x, y, text, color=TEXT_COLOR):
    """Draw one 6x8 row without any anti-aliasing or background cell."""
    if x < 0 or y < 0 or y >= image.height:
        raise ValueError("panel row {!r} at ({}, {}) leaves the image".format(text, x, y))
    if x + text_width(text) > image.width:
        raise ValueError("panel row {!r} is too wide for {} px".format(text, image.width))
    pixels = image.load()
    for index, char in enumerate(text):
        base = x + index * FRAME_WIDTH
        for column, bits in enumerate(glyph_columns(char)):
            for row in range(FRAME_HEIGHT):
                if bits & (1 << row):
                    px = base + column
                    py = y + row
                    if 0 <= px < image.width and 0 <= py < image.height:
                        pixels[px, py] = color


def text_width(text):
    return len(text) * FRAME_WIDTH


def checked_text(strings, name):
    try:
        return strings[name]
    except KeyError as exc:
        raise ValueError("missing fxdata string {}".format(name)) from exc


def line(strings, name, x, y, color=TEXT_COLOR):
    return (checked_text(strings, name), x, y, color)


def centered(strings, name, y, color=TEXT_COLOR):
    return line(strings, name, 4, y, color)


def add_divider(image, y):
    pixels = image.load()
    for x in range(image.width):
        pixels[x, y] = TEXT_COLOR


def add_network(image, strings):
    for index in range(3):
        x0 = 20 + index * 29 + 7
        x1 = 20 + (index + 1) * 29
        for x in range(x0, x1):
            image.putpixel((x, 26 + 7 // 2), TEXT_COLOR)
    for index in range(4):
        left = 20 + index * 29
        for x in range(left, left + 7):
            for y in range(26, 26 + 7):
                image.putpixel((x, y), TEXT_COLOR)


def ending_network(strings):
    image = Image.new("RGBA", SIZE, BG_COLOR)
    for item in (line(strings, "txt_network_lit", 4, 2),):
        draw_text(image, item[1], item[2], item[0], item[3])
    add_divider(image, 12)
    add_network(image, strings)
    draw_text(image, 4, 46, checked_text(strings, "txt_net_online"))
    draw_text(image, 3, 56, checked_text(strings, "txt_a_next"))
    return image


def story_motif(image, kind):
    """Author native pixel clusters in the scene plate; never resample art.

    Solid light faces, broken edge highlights and sparse ordered shade establish
    depth without greys or a second plane. Keep water and text margins quiet.
    """
    from PIL import ImageDraw
    d = ImageDraw.Draw(image)
    w, b = TEXT_COLOR, BG_COLOR

    def shade(box, color=w, step=4):
        # Deliberately bounded half-tone on a material, never on the whole plate.
        x0, y0, x1, y1 = box
        for y in range(y0, y1 + 1, 2):
            for x in range(x0 + (2 if y % 4 else 0), x1 + 1, step):
                d.point((x, y), fill=color)

    def rivets(points):
        for x, y in points:
            d.point((x, y), fill=w)

    def crew(x, y, wave=False):
        # A round helmet, black faceplate, padded shoulders, tank and split boots.
        d.rounded_rectangle((x, y, x + 4, y + 4), radius=1, fill=w)
        d.line((x + 1, y + 2, x + 3, y + 2), fill=b)
        d.rectangle((x, y + 5, x + 4, y + 9), fill=w)
        d.line((x + 1, y + 6, x + 1, y + 8), fill=b)
        d.line((x - 1, y + 6, x - 1, y + 9), fill=w)
        d.rectangle((x, y + 10, x + 1, y + 12), fill=w)
        d.rectangle((x + 3, y + 10, x + 5, y + 12), fill=w)
        if wave:
            d.line((x + 4, y + 6, x + 7, y + 4, x + 7, y + 1), fill=w)
            d.point((x + 6, y + 1), fill=w)
        else:
            d.line((x + 5, y + 6, x + 6, y + 9), fill=w)

    def boat(x, y):
        # A curved pressure hull: lit crown, dark belly, recessed glass and fins.
        d.polygon(((x - 3, y + 4), (x + 2, y + 4), (x + 2, y + 12),
                   (x - 3, y + 12), (x - 1, y + 8)), fill=w)
        d.rounded_rectangle((x, y, x + 35, y + 15), radius=7, fill=w)
        d.line((x + 7, y + 2, x + 26, y + 2), fill=b)
        d.rectangle((x + 12, y - 5, x + 21, y), fill=w)
        d.rectangle((x + 14, y - 3, x + 19, y - 1), fill=b)
        d.line((x + 17, y - 6, x + 17, y - 8, x + 21, y - 8), fill=w)
        for px in (x + 8, x + 17, x + 26):
            d.ellipse((px - 2, y + 5, px + 3, y + 10), fill=b)
            d.line((px, y + 6, px + 1, y + 6), fill=w)
        d.line((x + 4, y + 12, x + 30, y + 12), fill=b)
        shade((x + 6, y + 13, x + 28, y + 14), b)
        d.line((x + 9, y + 16, x + 27, y + 16), fill=w)
        d.line((x - 5, y + 4, x - 5, y + 11), fill=w)
        d.point((x + 34, y + 7), fill=b)

    if kind == "first":
        # Receding occupied modules and the near pressure-glass hand.
        d.polygon(((2, 18), (48, 11), (61, 11), (61, 43), (2, 43)), outline=w)
        d.line((3, 39, 58, 39), fill=w)
        d.line((3, 41, 58, 41), fill=w)
        for x in (6, 25, 46):
            d.line((x, 18, x, 37), fill=w)
            rivets([(x + 2, 19), (x + 2, 35)])
        for x, y, width in ((11, 22, 10), (32, 18, 13)):
            d.rounded_rectangle((x, y, x + width, y + 13), radius=3, outline=w)
            d.rounded_rectangle((x + 2, y + 2, x + width - 2, y + 10), radius=2, fill=w)
            d.rectangle((x + 4, y + 5, x + 6, y + 7), fill=b)
            d.rectangle((x + 3, y + 9, x + 7, y + 10), fill=b)
        shade((8, 36, 53, 37))
        d.line((53, 12, 53, 35, 60, 35), fill=w, width=2)
        d.rectangle((55, 27, 59, 31), outline=w)
        d.rectangle((64, 11, 121, 43), outline=w)
        d.line((118, 13, 118, 41), fill=w)
        shade((109, 14, 116, 40))
        d.ellipse((66, 11, 106, 43), fill=w)
        d.ellipse((69, 13, 103, 42), fill=b)
        d.ellipse((72, 15, 100, 40), fill=w)
        d.arc((74, 17, 98, 38), 180, 260, fill=b)
        rivets([(86, 12), (68, 27), (104, 27), (86, 42)])
        d.rectangle((81, 27, 89, 33), fill=b)
        d.polygon(((83, 33), (89, 32), (94, 39), (83, 39)), fill=b)
        for x, top in ((80, 23), (83, 20), (86, 19), (89, 22)):
            d.line((x, top, x, 29), fill=b, width=2)
        d.line((81, 30, 77, 27, 76, 24), fill=b, width=2)
        d.rectangle((122, 18, 125, 35), outline=w)
    elif kind == "pump":
        # Heavy cast casing, flanged pipe and outflow over a recessed sump.
        d.line((3, 43, 124, 43), fill=w)
        d.rectangle((4, 15, 9, 38), outline=w)
        d.line((5, 16, 8, 16), fill=w)
        d.rounded_rectangle((10, 17, 34, 37), radius=5, fill=w)
        d.rounded_rectangle((12, 19, 32, 35), radius=4, fill=b)
        d.ellipse((15, 21, 29, 34), outline=w)
        d.ellipse((19, 25, 25, 31), fill=w)
        d.line((22, 23, 22, 33), fill=b)
        d.line((17, 28, 27, 28), fill=b)
        d.line((13, 38, 13, 40, 32, 40, 32, 38), fill=w, width=2)
        d.line((34, 24, 49, 24, 49, 29), fill=w, width=5)
        d.line((35, 24, 47, 24), fill=b)
        for x in (35, 41):
            d.line((x, 20, x, 27), fill=w)
        d.line((19, 16, 19, 13, 33, 13), fill=w, width=2)
        d.ellipse((29, 11, 35, 15), fill=w)
        d.point((32, 13), fill=b)
        for x, y in ((46, 31), (49, 32), (52, 30)):
            d.line((x, y, x, y + 4), fill=w)
        d.polygon(((39, 39), (45, 36), (55, 36), (62, 41), (39, 41)), outline=w)
        d.line((45, 38, 55, 38), fill=w)
        rivets([(43, 40), (50, 40), (59, 40)])
        # Refuge at a different depth, with a lit lintel and shadowed doorway.
        d.polygon(((70, 16), (77, 12), (121, 12), (124, 16)), fill=w)
        d.rectangle((72, 17, 122, 42), outline=w)
        d.line((75, 18, 75, 39), fill=w)
        d.line((74, 22, 120, 22), fill=w)
        d.rectangle((84, 18, 103, 42), fill=w)
        d.rectangle((87, 22, 100, 41), fill=b)
        d.rectangle((108, 18, 118, 25), outline=w)
        d.line((110, 20, 116, 20), fill=w)
        shade((77, 25, 82, 38))
        d.rectangle((105, 40, 121, 41), fill=w)
        crew(89, 27)
        crew(110, 29)
        d.line((64, 13, 64, 32, 69, 32), fill=w)
        d.line((66, 13, 66, 30), fill=w)
    elif kind == "lift":
        # Back wall and receding floor leave the moving load in clear water.
        d.line((6, 23, 6, 39, 16, 39), fill=w)
        d.line((9, 23, 16, 23), fill=w)
        d.rectangle((94, 15, 122, 25), outline=w)
        for x in (98, 108, 118):
            d.line((x, 17, x, 23), fill=w)
        shade((95, 27, 121, 29))
        d.line((4, 43, 124, 43), fill=w)
        d.line((34, 40, 120, 40), fill=w)
        for x in (38, 58, 98, 120):
            d.line((x, 41, x - 5, 43), fill=w)
        # Riveted lattice mast and braced overhead boom.
        d.rectangle((19, 11, 27, 42), fill=w)
        d.rectangle((21, 13, 25, 40), fill=b)
        for y in range(15, 39, 8):
            d.line((21, y, 25, y + 5, 21, y + 8), fill=w)
        d.rectangle((19, 11, 84, 17), fill=w)
        d.rectangle((29, 13, 81, 15), fill=b)
        for x in range(31, 78, 10):
            d.line((x, 13, x + 5, 15), fill=w)
        d.line((27, 35, 48, 18), fill=w, width=2)
        d.line((29, 36, 50, 19), fill=w)
        d.ellipse((74, 13, 82, 21), fill=w)
        d.ellipse((76, 15, 80, 19), fill=b)
        d.line((77, 21, 77, 25), fill=w)
        d.line((79, 21, 79, 25), fill=w)
        d.arc((75, 23, 81, 28), 0, 160, fill=w)
        d.line((78, 27, 66, 31), fill=w)
        d.line((78, 27, 89, 31), fill=w)
        d.polygon(((64, 32), (68, 29), (90, 29), (90, 38), (86, 41), (64, 41)), fill=w)
        d.rectangle((66, 33, 85, 39), fill=b)
        d.line((67, 34, 84, 38), fill=w)
        d.line((84, 34, 67, 38), fill=w)
        d.line((87, 32, 89, 30), fill=b)
        d.line((88, 34, 88, 37), fill=b)
        crew(44, 27, True)
        crew(110, 28)
        d.rectangle((97, 35, 105, 39), outline=w)
    elif kind == "beacon":
        # Foreground crag, shaded facets, anchored lattice mast.
        d.polygon(((2, 43), (7, 38), (15, 37), (20, 33), (29, 35),
                   (33, 39), (42, 39), (48, 43)), fill=w)
        d.polygon(((4, 43), (15, 40), (19, 36), (24, 38), (32, 41), (40, 41), (43, 43)), fill=b)
        shade((7, 41, 36, 43))
        d.polygon(((18, 36), (22, 21), (29, 21), (34, 37)), fill=w)
        d.polygon(((22, 33), (25, 24), (27, 24), (30, 35)), fill=b)
        d.line((22, 28, 29, 31, 21, 34), fill=w)
        d.rectangle((18, 16, 33, 23), fill=w)
        d.rectangle((20, 18, 31, 21), fill=b)
        d.rectangle((24, 18, 27, 21), fill=w)
        d.polygon(((17, 15), (25, 11), (34, 15)), fill=w)
        d.line((21, 14, 29, 14), fill=b)
        d.line((20, 24, 11, 37), fill=w)
        d.line((31, 24, 40, 39), fill=w)
        # Sparse broken light fans, not a filled cone across quiet water.
        for x, y in ((39, 17), (46, 15), (54, 13), (41, 22), (51, 22),
                     (61, 22), (39, 26), (46, 29), (54, 32)):
            d.line((x, y, x + 3, y), fill=w)
        # A small occupied outpost behind the answering lantern.
        d.polygon(((79, 43), (85, 39), (119, 39), (125, 43)), outline=w)
        d.rectangle((89, 35, 115, 40), outline=w)
        for x in (92, 100, 108):
            d.rectangle((x, 36, x + 3, 38), fill=w)
        d.line((101, 34, 101, 24), fill=w)
        d.rectangle((98, 24, 104, 27), fill=w)
        d.point((101, 25), fill=b)
        for x, y in ((89, 24), (83, 22), (111, 24), (118, 22), (89, 29), (115, 30)):
            d.line((x, y, x + 3, y), fill=w)
        rivets([(62, 40), (65, 41), (71, 37), (73, 37)])
    elif kind == "home":
        # Distant pressure buildings with warm windows, a closer welcoming quay.
        d.polygon(((80, 16), (87, 12), (121, 12), (125, 16)), outline=w)
        d.rectangle((84, 17, 123, 30), outline=w)
        d.line((87, 17, 87, 28), fill=w)
        for x in (91, 102, 113):
            d.rounded_rectangle((x, 18, x + 6, 25), radius=1, fill=w)
            d.line((x + 1, 24, x + 5, 24), fill=b)
        shade((86, 28, 122, 30))
        boat(12, 26)
        d.rectangle((62, 39, 125, 42), fill=w)
        d.line((63, 41, 124, 41), fill=b)
        for x in (66, 92, 119):
            d.line((x, 42, x + 2, 43), fill=w)
        for x in (67, 83, 99, 116):
            d.point((x, 40), fill=b)
        d.line((60, 16, 60, 36), fill=w)
        d.rectangle((57, 16, 63, 20), fill=w)
        d.point((60, 18), fill=b)
        d.line((56, 14, 64, 14), fill=w)
        crew(72, 26, True)
        crew(90, 26, True)
        crew(110, 26)
        d.line((5, 43, 52, 43), fill=w)
        d.line((49, 34, 54, 34), fill=w)
        rivets([(4, 24), (6, 22), (51, 40), (54, 41)])


def story_card(strings, title, lines, motif):
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 1, title)
    story_motif(image, motif)
    for index, text in enumerate(lines):
        draw_text(image, 4, 46 + index * 8, text)
    draw_text(image, 3, 56, checked_text(strings, "txt_a_next"))
    return ("story_" + motif + "_128x64", image)


STORY_CARDS = (
    ("first", "BENTHOS", ("A HAND AT THE GLASS.",)),
    ("pump", "PUMP REFUGE", ("THE CREWS COME BACK.",)),
    ("lift", "CORMORANT", ("CRATES CLIMB AGAIN.",)),
    ("beacon", "DEEP ANCHOR", ("A LIGHT ANSWERS.",)),
)


def story_panels(strings):
    return [story_card(strings, title, lines, motif) for motif, title, lines in STORY_CARDS]


def story_preview(strings):
    """Five native cards; enlargement uses only integer nearest-neighbor pixels."""
    cards = story_panels(strings) + [("home", ending_thanks(strings))]
    sheet = Image.new("RGBA", (SIZE[0] * len(cards), SIZE[1]), BG_COLOR)
    for index, (_name, image) in enumerate(cards):
        sheet.paste(image, (SIZE[0] * index, 0))
    return sheet


def ending_thanks(strings):
    """Homecoming recognition: the first Tidewall card after the lit network."""
    return story_card(strings, "TIDEWALL", ("THEY KNOW YOUR HULL.",), "home")[1]


def prompt(strings, title, rows, hint, selected):
    """Compose a two-row prompt, baking either row's inverted selection bar."""
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 8, checked_text(strings, title))
    for index, row in enumerate(rows):
        y = 24 + index * 12
        if index == selected:
            for px in range(128):
                for py in range(y - 1, y - 1 + 8):
                    image.putpixel((px, py), TEXT_COLOR)
            draw_text(image, 4, y, checked_text(strings, row), BG_COLOR)
        else:
            draw_text(image, 4, y, checked_text(strings, row))
    draw_text(image, 4, 57, checked_text(strings, hint))
    return image


def confirm(strings, title, body, y_body, y_line):
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 8, checked_text(strings, title))
    draw_text(image, 4, y_body, checked_text(strings, body))
    draw_text(image, 4, y_line, checked_text(strings, "txt_yes_no"))
    return image


def panels(strings):
    rows = [
        ("panel_ending_network_128x64", ending_network(strings)),
        ("panel_ending_thanks_128x64", ending_thanks(strings)),
        ("panel_ending_stats_128x64", ending_stats(strings)),
        ("panel_save_retry_128x64", prompt(strings, "txt_save_failed", ("txt_retry", "txt_continue_unsaved"), "txt_save_hint", 0)),
        ("panel_save_continue_128x64", prompt(strings, "txt_save_failed", ("txt_retry", "txt_continue_unsaved"), "txt_save_hint", 1)),
        ("panel_pause_resume_128x64", prompt(strings, "txt_paused", ("txt_resume", "txt_rescue"), "txt_pause_hint", 0)),
        ("panel_pause_rescue_128x64", prompt(strings, "txt_paused", ("txt_resume", "txt_rescue"), "txt_pause_hint", 1)),
        ("panel_rescue_confirm_128x64", confirm(strings, "txt_back_last_berth", "txt_unsaved_lost", 24, 38)),
        ("panel_abandon_confirm_128x64", confirm(strings, "txt_abandon_job", "txt_yes_no", 24, 24)),
    ]
    return rows


def blank_sprite(width, height):
    return Image.new("RGBA", (width, height), CLEAR)


def survey_ring():
    image = blank_sprite(15, 13)
    for x in range(3, 12):
        for y in range(4, 9):
            image.putpixel((x, y), TEXT_COLOR)
    for x in range(4, 11):
        for y in range(5, 8):
            image.putpixel((x, y), CLEAR)
    image.putpixel((7, 6), TEXT_COLOR)
    for x, y in ((0, 6), (14, 6), (7, 0), (7, 12)):
        image.putpixel((x, y), TEXT_COLOR)
    return image


def mooring():
    image = blank_sprite(18, 8)
    for x in range(0, 3):
        for y in range(8):
            image.putpixel((x, y), TEXT_COLOR)
    for x in range(15, 18):
        for y in range(8):
            image.putpixel((x, y), TEXT_COLOR)
    for x in (5, 9, 13):
        for dx in range(2):
            for y in (4, 5):
                image.putpixel((x + dx, y), TEXT_COLOR)
    return image


def pump_flow():
    image = blank_sprite(37, 12)
    for index in range(3):
        y = index * 4
        for x in range(30 + index, 34 + index):
            image.putpixel((x, y), TEXT_COLOR)
        for x in range(2 - index, 6 - index):
            image.putpixel((x, y), TEXT_COLOR)
    return image


def lift_cable():
    image = blank_sprite(11, 10)
    for y in range(9):
        image.putpixel((5, y), TEXT_COLOR)
    for x in range(11):
        image.putpixel((x, 0), TEXT_COLOR)
    for x in range(2, 9):
        image.putpixel((x, 3), TEXT_COLOR)
    return image


STATUS_STRINGS = (
    "txt_no_save",
    "txt_save_ready",
    "txt_save_corrupt",
    "txt_save_incompatible",
    "txt_save_unreadable",
)
HULL_STRINGS = ("txt_minnow", "txt_hauler", "txt_leviathan")
TITLE_ROWS = ("txt_continue", "txt_new_voyage")


def ending_stats(strings):
    """Static labels; the saved time, cove and job values overlay natively."""
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 2, checked_text(strings, "txt_voyage_stats"))
    add_divider(image, 12)
    draw_text(image, 4, 16, checked_text(strings, "txt_time"))
    draw_text(image, 4, 28, checked_text(strings, "txt_coves"))
    draw_text(image, 46, 28, "/8")
    draw_text(image, 4, 40, checked_text(strings, "txt_jobs"))
    draw_text(image, 3, 56, checked_text(strings, "txt_a_finish"))
    return image


def title_menu_frame(strings, status, hull, selected):
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 3, checked_text(strings, "txt_sub_hauler"))
    draw_text(image, 4, 12, checked_text(strings, HULL_STRINGS[hull]))
    for index, row in enumerate(TITLE_ROWS):
        y = 22 + index * 9
        if index == selected:
            for px in range(128):
                for py in range(y - 1, y + 7):
                    image.putpixel((px, py), TEXT_COLOR)
            draw_text(image, 4, y, checked_text(strings, row), BG_COLOR)
        else:
            draw_text(image, 4, y, checked_text(strings, row))
    draw_text(image, 4, 48, checked_text(strings, STATUS_STRINGS[status]))
    draw_text(image, 4, 57, checked_text(strings, "txt_title_save_hint"))
    return image


def title_panels(strings):
    """One frame per status x hull x selected menu row, plus both confirmations."""
    menu = Image.new("RGBA", (128 * 30, 64), BG_COLOR)
    for status in range(5):
        for hull in range(3):
            for selected in range(2):
                frame = title_menu_frame(strings, status, hull, selected)
                menu.paste(frame, (128 * (status * 6 + hull * 2 + selected), 0))
    replace = Image.new("RGBA", (128 * 5, 64), BG_COLOR)
    for status in range(5):
        frame = Image.new("RGBA", SIZE, BG_COLOR)
        draw_text(frame, 4, 24, checked_text(strings, "txt_replace_voyage"))
        draw_text(frame, 4, 36, checked_text(strings, STATUS_STRINGS[status]))
        draw_text(frame, 4, 48, checked_text(strings, "txt_yes_no"))
        replace.paste(frame, (128 * status, 0))
    keep = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(keep, 4, 24, checked_text(strings, "txt_keep_chart"))
    draw_text(keep, 4, 36, checked_text(strings, "txt_yes_no"))
    return [
        ("title_menu_128x64", menu),
        ("title_replace_128x64", replace),
        ("title_keep_128x64", keep),
    ]


def pause_panels(strings):
    """MUSIC ON/OFF pause menu: six frames (two states x three selected rows)."""
    rows = []
    for music_on in (True, False):
        for selected in range(3):
            image = Image.new("RGBA", SIZE, BG_COLOR)
            draw_text(image, 4, 8, checked_text(strings, "txt_paused"))
            labels = (
                checked_text(strings, "txt_resume"),
                checked_text(strings, "txt_rescue"),
                checked_text(strings, "txt_music_on" if music_on else "txt_music_off"),
            )
            for index, label in enumerate(labels):
                y = 24 + index * 12
                if index == selected:
                    for px in range(128):
                        for py in range(y - 1, y + 7):
                            image.putpixel((px, py), TEXT_COLOR)
                    draw_text(image, 4, y, label, BG_COLOR)
                else:
                    draw_text(image, 4, y, label)
            draw_text(image, 4, 57, checked_text(strings, "txt_pause_hint"))
            rows.append(image)
    sheet = Image.new("RGBA", (128 * len(rows), 64), BG_COLOR)
    for index, image in enumerate(rows):
        sheet.paste(image, (128 * index, 0))
    return [("pause_menu_128x64", sheet)]


def campaign_panels():
    return [
        ("survey_ring_15x13", survey_ring()),
        ("mooring_18x8", mooring()),
        ("pump_flow_37x12", pump_flow()),
        ("lift_cable_11x10", lift_cable()),
    ]


def work_detail(strings, routes, rewards, index):
    """A WORK detail is fully static: label, purpose, reward and A/B hint."""
    _enum, _stem, label, purpose = MISSIONS[index]
    from_name, to_name = routes[index]
    from_index = list(BERTH_NAMES).index(from_name)
    to_index = list(BERTH_NAMES).index(to_name)
    reward = rewards[from_index * SETTLEMENT_COUNT + to_index]
    image = Image.new("RGBA", SIZE, BG_COLOR)
    draw_text(image, 4, 4, checked_text(strings, label))
    draw_text(image, 4, 18, checked_text(strings, purpose))
    draw_text(image, 4, 30, "{} {}".format(checked_text(strings, "txt_pays"), reward))
    draw_text(image, 3, 56, checked_text(strings, "txt_load_hint"))
    return image


def work_active(strings, routes, index):
    """Carrying summary as one strip; the station header stays native text."""
    _enum, _stem, label, _purpose = MISSIONS[index]
    _from_name, to_name = routes[index]
    image = Image.new("RGBA", (128, 44), BG_COLOR)
    draw_text(image, 4, 0, "{} {}".format(checked_text(strings, "txt_carrying"), checked_text(strings, label)))
    draw_text(image, 4, 9, checked_text(strings, "txt_deliver_to"))
    draw_text(image, 4, 18, checked_text(strings, BERTH_NAMES[to_name]))
    draw_text(image, 3, 36, checked_text(strings, "txt_back_hint"))
    return image


def work_row(text):
    """One 256x8 sheet: frame 0 normal, frame 1 selected inverted row."""
    image = Image.new("RGBA", (256, 8), BG_COLOR)
    draw_text(image, 4, 0, text)
    for x in range(128, 256):
        for y in range(8):
            image.putpixel((x, y), TEXT_COLOR)
    draw_text(image, 132, 1, text, BG_COLOR)
    return image


def work_panels(strings, routes, rewards):
    """Three sheets in kMissions order (then VIEW ENDING for the row sheet)."""
    details = Image.new("RGBA", (128 * 5, 64), BG_COLOR)
    actives = Image.new("RGBA", (128 * 5, 44), BG_COLOR)
    rows = Image.new("RGBA", (128 * 12, 8), BG_COLOR)
    for index, (_enum, _stem, label, _purpose) in enumerate(MISSIONS):
        details.paste(work_detail(strings, routes, rewards, index), (128 * index, 0))
        actives.paste(work_active(strings, routes, index), (128 * index, 0))
        from_name, to_name = routes[index]
        route_reward = rewards[list(BERTH_NAMES).index(from_name) * SETTLEMENT_COUNT + list(BERTH_NAMES).index(to_name)]
        rows.paste(work_row("{} {}".format(checked_text(strings, label), route_reward)), (128 * index * 2, 0))
    # VIEW ENDING has no reward and no detail/active page (A opens the cards).
    rows.paste(work_row(checked_text(strings, "txt_view_ending")), (128 * 10, 0))
    return [
        ("work_details_128x64", details),
        ("work_active_128x44", actives),
        ("work_rows_128x8", rows),
    ]


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
    parser.add_argument("--panels", default="images/panels")
    parser.add_argument("--campaign", default="images/campaign")
    parser.add_argument("--pause", default="images/pause")
    parser.add_argument("--story", default="images/story")
    parser.add_argument("--preview", default="docs/assets/story-flourishes-preview.png")
    parser.add_argument("--fxdata", default="fxdata/fxdata.txt")
    parser.add_argument("--campaign-src", default="src/campaign.hpp")
    parser.add_argument("--level", default="src/generated/level.hpp")
    args = parser.parse_args(argv)
    strings = load_strings(args.fxdata)
    routes = load_routes(args.campaign_src)
    rewards = load_rewards(args.level)
    panel_rows = panels(strings) + work_panels(strings, routes, rewards) + title_panels(strings)
    story_rows = story_panels(strings)
    write_dir(args.panels, panel_rows)
    write_dir(args.campaign, campaign_panels())
    write_dir(args.pause, pause_panels(strings))
    write_dir(args.story, story_rows)
    Path(args.preview).parent.mkdir(parents=True, exist_ok=True)
    preview = story_preview(strings)
    preview.save(args.preview)
    preview.resize((preview.width * 4, preview.height * 4), Image.Resampling.NEAREST).save(
        Path(args.preview).with_name(Path(args.preview).stem + "-4x.png"))
    print("gen_panels: wrote {} panels, {} story cards, {} campaign sprites and {} pause frames".format(
        len(panel_rows), len(story_rows), len(campaign_panels()), 6))


if __name__ == "__main__":
    main()
