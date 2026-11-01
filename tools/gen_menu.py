#!/usr/bin/env python3
"""Bake the station and shop board chrome into opaque FX screens.

The four main berths need one frame per idle selection and one per active-board
selection; the two service berths need one frame each. The shop needs one frame
per air level. Credits, air, capacity and the active-job summary stay live
native text, and the bottom 7-px hint row is a separate generated row so it can
be redrawn without touching the last action bar.

The station layout is read from ``src/station_view.hpp``. The retired shop
frames remain cart ABI, so their frozen layout and price/level constants live
here instead of in runtime headers; text comes from ``fxdata/fxdata.txt``. The
output is deterministic: fixed specs, fixed glyphs, and no timestamps.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image

from gen_font import FRAME_HEIGHT, FRAME_WIDTH, GLYPHS
from gen_panels import draw_text, load_strings, text_width

SIZE = (128, 64)
WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)

MAIN_BERTHS = (
    "txt_tidewall",
    "txt_benthos",
    "txt_cormorant",
    "txt_deep_anchor",
)
SERVICE_BERTHS = ("txt_pump_refuge", "txt_lower_gantry")
MAIN_BERTH_COUNT = len(MAIN_BERTHS)
STATION_FRAME_COUNT = MAIN_BERTH_COUNT * 7 + len(SERVICE_BERTHS)
IDLE_SELECTIONS = (0, 1, 2)
ACTIVE_SELECTIONS = (1, 2, 3, 4)

# Generated ABI order. The C++ enums in station_view must match.
STATION_HINTS = (
    "txt_service_berth_hint",
    "txt_short_load",
    "txt_cutter_rumor",
    "txt_thruster_rumor",
    "txt_seal_rumor",
    "txt_autoload_hint",
    "txt_station_hint",
)
SHOP_HINTS = ("txt_buy_hint", "txt_low_credits", "txt_max_level")

STATION_CONSTANTS = (
    "kNameX",
    "kHeaderY",
    "kCreditsX",
    "kDividerY",
    "kStatusX",
    "kStatusY",
    "kRowX",
    "kRowY0",
    "kRowStep",
    "kRowInsetY",
    "kRowInvertH",
    "kHintX",
    "kHintY",
)
# Frozen archival shop layout. The runtime shop is gone, but these pixels are
# still baked into the shipped MenuSprites cart section, so the values may not
# drift. They intentionally live in this generator rather than a runtime header.
SHOP_LAYOUT = {
    "kPanelRowX": 4,
    "kPanelRowY0": 26,
    "kPanelRowStep": 12,
    "kPanelInsetY": 2,
    "kPanelInvertH": 11,
    "kPanelLevelX": 68,
    "kPanelPriceX": 94,
    "kHintX": 3,
    "kHintY": 57,
}
SHOP_PRICES = (60,)
SHOP_MAX_LEVEL = 3
SHOP_ITEM_COUNT = 1

CONST_RE = r"constexpr\s+uint8_t\s+{name}\s*=\s*(?P<value>\d+)\s*;"


def load_constants(path, names):
    """Read layout constants so the C++ view remains the single source."""
    text = Path(path).read_text(encoding="utf-8")
    values = {}
    for name in names:
        match = re.search(CONST_RE.format(name=re.escape(name)), text)
        if not match:
            raise ValueError("{}: constant {} not found".format(path, name))
        values[name] = int(match.group("value"))
    return values


def checked(strings, symbol):
    try:
        return strings[symbol]
    except KeyError as exc:
        raise ValueError("missing fxdata string {}".format(symbol)) from exc


def blank():
    return Image.new("RGBA", SIZE, BLACK)


def bar(image, y, height):
    pixels = image.load()
    for x in range(image.width):
        for py in range(y, y + height):
            pixels[x, py] = WHITE


def draw_header(image, strings, name_symbol, layout, credits_caption=True):
    draw_text(image, layout["kNameX"], layout["kHeaderY"], checked(strings, name_symbol))
    if credits_caption:
        draw_text(image, layout["kCreditsX"], layout["kHeaderY"], checked(strings, "txt_credits"))
    pixels = image.load()
    for x in range(image.width):
        pixels[x, layout["kDividerY"]] = WHITE
    draw_text(image, layout["kStatusX"], layout["kStatusY"], checked(strings, "txt_air_status"))


def station_rows(strings, active):
    """Rows below the header: (label, row index), or None for the live summary."""
    if active:
        return [
            (None, 0),
            (checked(strings, "txt_abandon"), 1),
            (checked(strings, "txt_work"), 2),
            (checked(strings, "txt_shop"), 3),
            (checked(strings, "txt_undock"), 4),
        ]
    return [
        (checked(strings, "txt_work"), 0),
        (checked(strings, "txt_shop"), 1),
        (checked(strings, "txt_undock"), 2),
    ]


def station_frame(strings, layout, berth, active, selected, service):
    image = blank()
    draw_header(image, strings, berth, layout)
    rows = [(checked(strings, "txt_undock"), 0)] if service else station_rows(strings, active)
    for label, row in rows:
        if label is None:
            continue
        y = layout["kRowY0"] + row * layout["kRowStep"]
        chosen = row == selected
        if chosen:
            bar(image, y - layout["kRowInsetY"], layout["kRowInvertH"])
        draw_text(image, layout["kRowX"], y, label, BLACK if chosen else WHITE)
    return image


def station_panels(strings, layout):
    frames = []
    for berth in MAIN_BERTHS:
        for selected in IDLE_SELECTIONS:
            frames.append(station_frame(strings, layout, berth, False, selected, False))
        for selected in ACTIVE_SELECTIONS:
            frames.append(station_frame(strings, layout, berth, True, selected, False))
    for berth in SERVICE_BERTHS:
        frames.append(station_frame(strings, layout, berth, False, 0, True))
    if len(frames) != STATION_FRAME_COUNT:
        raise ValueError("station frame count drifted from the ABI")
    return frames


def hint_rows(strings, symbols, layout):
    width = SIZE[0]
    height = layout["kHintY"] - 57 + 7
    if height != 7:
        raise ValueError("hint row must stay 7 px at y=57")
    sheet = Image.new("RGBA", (width * len(symbols), height), BLACK)
    for index, symbol in enumerate(symbols):
        frame = Image.new("RGBA", (width, height), BLACK)
        draw_text(frame, layout["kHintX"], 0, checked(strings, symbol))
        sheet.paste(frame, (width * index, 0))
    return sheet


def shop_frame(strings, layout, prices, max_level, labels, air, selected):
    image = blank()
    draw_header(image, strings, "txt_shop", layout)
    for item, label in enumerate(labels):
        y = layout["kPanelRowY0"] + item * layout["kPanelRowStep"]
        chosen = item == selected
        if chosen:
            bar(image, y - layout["kPanelInsetY"], layout["kPanelInvertH"])
        color = BLACK if chosen else WHITE
        draw_text(image, layout["kPanelRowX"], y, label, color)
        draw_text(image, layout["kPanelLevelX"], y, "L{}".format(air), color)
        price = checked(strings, "txt_max") if air >= max_level else str(prices[item])
        draw_text(image, layout["kPanelPriceX"], y, price, color)
    return image


def shop_panels(strings, layout, prices, max_level, item_count):
    labels = [checked(strings, "txt_air_tank")][:item_count]
    frames = []
    for air in range(max_level + 1):
        for selected in range(item_count):
            frames.append(shop_frame(strings, layout, prices, max_level, labels, air, selected))
    return frames


def sheet(frames):
    width = SIZE[0] * len(frames)
    image = Image.new("RGBA", (width, SIZE[1]), BLACK)
    for index, frame in enumerate(frames):
        image.paste(frame, (SIZE[0] * index, 0))
    return image


def preview(station_frames, shop_frames, hint_sheet, strings, layout):
    picks = [
        station_frames[0],                 # Tidewall idle, WORK selected
        station_frames[6],                 # Tidewall active, UNDOCK selected
        station_frames[28],                # Pump Refuge service berth
        station_frames[29],                # Lower Gantry service berth
        shop_frames[0],                    # L0, AIR selected
        shop_frames[1],                    # L1, AIR selected
        shop_frames[3],                    # L3, MAX
    ]
    # Illustrative live overlays make the preview representative of the game;
    # copies keep the generated source panels free of example values.
    hints = ("txt_cutter_rumor", "txt_autoload_hint", "txt_service_berth_hint",
             "txt_service_berth_hint", "txt_buy_hint", "txt_buy_hint", "txt_max_level")
    levels = (0, 0, 0, 0, 0, 1, 3)
    gap = 8
    image = Image.new("RGBA", (4 * SIZE[0] + 5 * gap, 2 * SIZE[1] + 3 * gap), (24, 24, 24, 255))
    for index, source in enumerate(picks):
        frame = source.copy()
        air = levels[index]
        draw_text(frame, layout["kCreditsX"] + 18, layout["kHeaderY"], "145")
        draw_text(frame, layout["kStatusX"] + 24, layout["kStatusY"], "{0}/{0}".format(300 + air * 30))
        if index == 1:
            draw_text(frame, layout["kRowX"], layout["kRowY0"], "BENTHOS 145")
        draw_text(frame, layout["kHintX"], layout["kHintY"], checked(strings, hints[index]))
        image.paste(frame, (gap + (index % 4) * (SIZE[0] + gap), gap + (index // 4) * (SIZE[1] + gap)))
    return image.resize((image.width * 3, image.height * 3), Image.Resampling.NEAREST)


def write_images(directory, rows):
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
    parser.add_argument("--output", default="images/menu")
    parser.add_argument("--preview", default="build/menu-preview/menu-contact.png")
    parser.add_argument("--fxdata", default="fxdata/fxdata.txt")
    parser.add_argument("--station-view", default="src/station_view.hpp")
    args = parser.parse_args(argv)

    strings = load_strings(args.fxdata)
    station_layout = load_constants(args.station_view, STATION_CONSTANTS)
    # Shop rows add their own vertical layout; header/status positions stay
    # shared with the station board.
    shop_layout = dict(station_layout)
    shop_layout.update(SHOP_LAYOUT)

    station_frames = station_panels(strings, station_layout)
    shop_frames = shop_panels(strings, shop_layout, SHOP_PRICES, SHOP_MAX_LEVEL, SHOP_ITEM_COUNT)
    station_hints = hint_rows(strings, STATION_HINTS, station_layout)
    shop_hints = hint_rows(strings, SHOP_HINTS, shop_layout)
    rows = [
        ("station_menu_128x64", sheet(station_frames)),
        ("station_hints_128x7", station_hints),
        ("shop_menu_128x64", sheet(shop_frames)),
        ("shop_hints_128x7", shop_hints),
    ]
    write_images(args.output, rows)

    preview_path = Path(args.preview)
    if preview_path:
        preview_path.parent.mkdir(parents=True, exist_ok=True)
        preview(station_frames, shop_frames, station_hints, strings, station_layout).save(preview_path)
    print(
        "gen_menu: wrote {} station frames, {} shop frames and {} hint rows".format(
            len(station_frames), len(shop_frames), len(STATION_HINTS) + len(SHOP_HINTS)
        )
    )


if __name__ == "__main__":
    main()
