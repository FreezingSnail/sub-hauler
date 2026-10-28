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
EXTRA_GLYPHS = {0x3F: (0x02, 0x01, 0x59, 0x09, 0x06)}


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


def ending_thanks(strings):
    image = Image.new("RGBA", SIZE, BG_COLOR)
    for item in (
        line(strings, "txt_thank_you", 4, 8),
        line(strings, "txt_thanks_from", 4, 22),
        line(strings, "txt_thanks_from2", 4, 32),
        line(strings, "txt_keep_exploring", 4, 44),
        line(strings, "txt_a_next", 3, 56),
    ):
        draw_text(image, item[1], item[2], item[0], item[3])
    return image


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
        ("panel_wreck_retry_128x64", prompt(strings, "txt_hull_lost", ("txt_retry_berth", "txt_title"), "txt_save_hint", 0)),
        ("panel_wreck_title_128x64", prompt(strings, "txt_hull_lost", ("txt_retry_berth", "txt_title"), "txt_save_hint", 1)),
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
TITLE_ROWS = ("txt_continue", "txt_new_voyage", "txt_loader")


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
    menu = Image.new("RGBA", (128 * 45, 64), BG_COLOR)
    for status in range(5):
        for hull in range(3):
            for selected in range(3):
                frame = title_menu_frame(strings, status, hull, selected)
                menu.paste(frame, (128 * (status * 9 + hull * 3 + selected), 0))
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
    parser.add_argument("--fxdata", default="fxdata/fxdata.txt")
    parser.add_argument("--campaign-src", default="src/campaign.hpp")
    parser.add_argument("--level", default="src/generated/level.hpp")
    args = parser.parse_args(argv)
    strings = load_strings(args.fxdata)
    routes = load_routes(args.campaign_src)
    rewards = load_rewards(args.level)
    panel_rows = panels(strings) + work_panels(strings, routes, rewards) + title_panels(strings)
    write_dir(args.panels, panel_rows)
    write_dir(args.campaign, campaign_panels())
    write_dir(args.pause, pause_panels(strings))
    print("gen_panels: wrote {} panels, {} campaign sprites and {} pause frames".format(len(panel_rows), len(campaign_panels()), 6))


if __name__ == "__main__":
    main()
