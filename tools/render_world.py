#!/usr/bin/env python3
"""Turn a native C++ world panorama into PNG exports, with an optional map key."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def font(size: int, bold: bool = False):
    candidates = (["DejaVuSans-Bold.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf"]
                  if bold else ["DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"])
    for name in candidates:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)


def export_regions(native: Image.Image, output: Path) -> None:
    """Unretouched 128x64 game views enlarged with nearest-neighbour pixels."""
    report = json.loads((ROOT / 'docs/assets/exploration-scenery.json').read_text())
    by_id = {item['id']: item for item in report['landmarks']}
    examples = [
        ('lantern-buoys', 'LANTERN REACHES / chained light buoy'),
        ('cathedral-pump', 'KELP GALLERIES / overgrown pump'),
        ('freight-yard-crane', 'BROKEN WORKS / freight crane'),
        ('glass-chimney', 'GLASS TRENCH / crystal formations'),
        ('harbor-tender', 'TIDEWALL / grounded tender'),
        ('relay-antenna', 'BENTHOS / abandoned relay'),
        ('broken-freighter', 'CORMORANT / broken freighter'),
        ('deep-anchor-vent', 'DEEP ANCHOR / chimney vents'),
    ]
    sheet = Image.new('RGB', (1056, 1256), '#102532')
    draw = ImageDraw.Draw(sheet)
    draw.text((16, 10), 'NATIVE GAME VIEWS / 128 × 64 pixels, enlarged 4×', font=font(21, True), fill='#e4efea')
    for i, (key, title) in enumerate(examples):
        landmark = by_id[key]
        cx = max(64, min(native.width-64, landmark['x']))
        cy = max(32, min(native.height-32, landmark['y']-18))
        # Snap the crop to the same 8-pixel camera grid used by the device.
        left, top = ((cx-64)//8)*8, ((cy-32)//8)*8
        crop = native.crop((left, top, left+128, top+64)).resize((512, 256), Image.Resampling.NEAREST)
        x, y = 16+(i%2)*528, 52+(i//2)*300
        draw.text((x, y), title, font=font(17, True), fill='#c9dedc')
        sheet.paste(crop.convert('RGB'), (x, y+27))
    sheet.save(output / 'game-world-regions.png', optimize=True)
    print(f'render-world: {output / "game-world-regions.png"} (eight native game viewports)')


def export_settlements(native: Image.Image, output: Path) -> None:
    report=json.loads((ROOT/'docs/assets/exploration-scenery.json').read_text())
    sheet=Image.new('RGB',(1056,900),'#102532')
    draw=ImageDraw.Draw(sheet)
    draw.text((16,10),'WALL BERTHS / inhabited pressure settlements',font=font(23,True),fill='#e4efea')
    for i,dock in enumerate(report['settlements']):
        centre=(min(dock['x']-16,min(m['x'] for m in dock['modules']))+
                max(dock['x']+16,max(m['x']+m['w'] for m in dock['modules'])))//2
        left=max(0,min(native.width-128,((centre-64)//8)*8))
        top=max(0,min(native.height-96,((dock['y']-24)//8)*8))
        crop=native.crop((left,top,left+128,top+96)).resize((512,384),Image.Resampling.NEAREST)
        x,y=16+(i%2)*528,54+(i//2)*420
        draw.text((x,y),dock['name'],font=font(19,True),fill='#c9dedc')
        sheet.paste(crop.convert('RGB'),(x,y+26))
    sheet.save(output/'game-world-settlements.png',optimize=True)
    print(f'render-world: {output / "game-world-settlements.png"} (four wall settlements)')


def export(input_path: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    native = Image.open(input_path).convert("1")
    if native.size != (2560, 1280):
        raise ValueError("expected a 2560x1280 native world panorama")
    native.save(output / "game-world-native.png", optimize=True)
    export_regions(native, output)
    export_settlements(native, output)
    source = json.loads((ROOT / "levels/exploration.json").read_text())
    objects = (ROOT / "fxdata/tables/exploration-objects.bin").read_bytes()
    if objects[:4] != b"SHL2" or objects[6:8] != bytes((7, 6)):
        raise ValueError("unsupported exploration object records")
    gate_count, pickup_count = objects[4:6]
    header, footer = 120, 130
    canvas = Image.new("RGB", (native.width, native.height + header + footer), "#102532")
    canvas.paste(native.convert("RGB"), (0, header))
    draw = ImageDraw.Draw(canvas)
    ink, muted = "#e4efea", "#9bb8c3"
    draw.text((40, 20), "SUB-HAULER / ACTUAL GAME WORLD", font=font(34, True), fill=ink)
    draw.text((40, 70), "Native cave, station, gate, pickup and hazard renderers · all secrets shown · new-run state", font=font(21), fill=muted)
    draw.text((native.width - 40, 32), "2560 × 1280 game pixels", font=font(22, True), fill=ink, anchor="ra")
    f_label, f_small = font(19, True), font(15)

    def label(x, y, title, subtitle=None, color="#123341", side="right", offset_y=0):
        y += header
        width = max(draw.textlength(title, font=f_label), draw.textlength(subtitle or "", font=f_small)) + 24
        height = 55 if subtitle else 33
        lx = x + 20 if side == "right" else x - width - 20
        lx = max(8, min(native.width - width - 8, lx))
        ly = max(header + 6, min(header + native.height - height - 6, y - height / 2 + offset_y))
        edge_x = lx if lx > x else lx + width
        draw.line([(x, y), (edge_x, ly + height / 2)], fill=color, width=2)
        draw.rounded_rectangle((lx, ly, lx + width, ly + height), radius=6, fill="#f8fcfa", outline=color, width=2)
        draw.text((lx + 12, ly + 6), title, font=f_label, fill=color)
        if subtitle:
            draw.text((lx + 12, ly + 31), subtitle, font=f_small, fill="#4c606b")

    # Names sit on a white key plate; the underlying native export stays intact.
    for room in source["rooms"]:
        if "station" not in room:
            continue
        x, y = (v * 8 for v in room.get('dock',room['position']))
        text = room["name"].upper()
        width = draw.textlength(text, font=f_label)
        bx, by = x - width / 2 - 10, y + header - 57
        draw.rounded_rectangle((bx, by, bx + width + 20, by + 31), radius=5, fill="#f8fcfa", outline="#255e55", width=2)
        draw.text((x, by + 5), text, font=f_label, fill="#20574f", anchor="ma")
        draw.ellipse((x - 5, y + header - 5, x + 5, y + header + 5), outline="#178169", width=2)

    gate_colors = {1: "#b97a15", 2: "#8255b2", 4: "#13819c", 6: "#8255b2"}
    for i in range(gate_count):
        x, y, w, h, required = struct.unpack_from("<HHBBB", objects, 8 + i * 7)
        color = gate_colors[required]
        draw.rectangle((x - 2, y + header - 2, x + w + 2, y + header + h + 2), outline=color, width=3)
        tag = {1: "C", 2: "P", 4: "T", 6: "P+T"}[required]
        tw = draw.textlength(tag, font=f_small) + 10
        cx, cy = x + w / 2, y + header + h / 2
        draw.rounded_rectangle((cx - tw / 2, cy - 12, cx + tw / 2, cy + 12), radius=4, fill=color)
        draw.text((cx, cy - 9), tag, font=f_small, fill="white", anchor="ma")

    names = {1: "SALVAGE CUTTER", 2: "PRESSURE SEAL", 3: "CURRENT THRUSTER",
             4: "CREDITS", 5: "HULL CAPACITY +1", 6: "SURVEY LENS"}
    subtitle = {1: "opens grates", 2: "opens deep water", 3: "opens opposing jets",
                4: "one-time cache", 5: "permanent within the run", 6: "larger sonar reveal"}
    for i in range(pickup_count):
        offset = 8 + gate_count * 7 + i * 6
        x, y, kind, amount = struct.unpack_from("<HHBB", objects, offset)
        title = f"{amount} CREDITS" if kind == 4 else names[kind]
        side = "right" if x < 350 else "left"
        label(x, y, title, subtitle[kind], "#8c5c0c" if kind == 4 else "#175c61", side, offset_y=-45 if kind == 6 else 0)

    pressure_y = source["pressure_y"] * source["tile_pixels"] + header
    for x in range(0, native.width, 24):
        draw.line((x, pressure_y, x + 11, pressure_y), fill="#a88cc2", width=2)
    draw.text((native.width - 35, pressure_y - 25), "HIGH PRESSURE BELOW · SEAL REQUIRED", font=font(17, True), fill="#78549b", anchor="ra", stroke_width=3, stroke_fill="white")
    regions = [(65, 105, "LANTERN REACHES"), (1450, 105, "KELP GALLERIES"),
               (1496, 488, "BROKEN WORKS"), (664, 1024, "GLASS TRENCH")]
    for x, y, text in regions:
        draw.text((x, y + header), text, font=font(22, True), fill="#58737c", stroke_width=3, stroke_fill="white")

    y = header + native.height + 22
    draw.text((40, y), "GATE KEY", font=font(19, True), fill=ink)
    for x, color, text in [(220, "#e5af4d", "C · Cutter"), (520, "#c4a0e7", "P · Pressure seal"),
                           (930, "#69d4df", "T · Thruster"), (1270, "#c4a0e7", "P+T · Both tools")]:
        draw.rectangle((x, y + 3, x + 18, y + 21), fill=color)
        draw.text((x + 30, y), text, font=font(20), fill=ink)
    draw.text((40, y + 48), "The companion native PNG preserves the game's exact 1-bit pixels; this overview adds labels and colored object markers.", font=font(18), fill=muted)
    canvas.save(output / "game-world.png", optimize=True)
    print(f"render-world: {output / 'game-world.png'} ({canvas.width}x{canvas.height})")
    print(f"render-world: {output / 'game-world-native.png'} ({native.width}x{native.height}, 1-bit)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/world-render")
    args = parser.parse_args()
    export(args.input, args.output_dir)


if __name__ == "__main__":
    main()
