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


def export_equipment(native: Image.Image, output: Path) -> None:
    """Actual pickup/relay-centered 128x64 views for scenery review."""
    source = json.loads((ROOT / 'levels/exploration.json').read_text())
    rooms = {r['id']: r for r in source['rooms']}
    sites = [('W', 'CUTTER / salvage teeth'), ('BR', 'THRUSTER / turbine housing'),
             ('CW', 'SEAL / pressure ribs'), ('DL', 'LENS / optical rings'),
             ('DC', 'DEEP RELAY / alien machinery'),
             ('relay-berth', 'DEEP RELAY / recovery berth')]
    sheet = Image.new('RGB', (1056, 960), '#102532')
    draw = ImageDraw.Draw(sheet)
    draw.text((16, 10), '128 x 64 GAME VIEWS / equipment and final relay',
              font=font(21, True), fill='#e4efea')
    for i, (key, title) in enumerate(sites):
        position = (next(b['dock'] for b in source['service_berths']
                         if b['symbol']=='DEEP_RELAY') if key=='relay-berth'
                    else rooms[key]['position'])
        cx, cy = (v*8 for v in position)
        left, top = max(0,min(2432,cx-64)), max(0,min(1216,cy-32))
        crop = native.crop((left,top,left+128,top+64))
        x, y = 16+(i%2)*528, 52+(i//2)*300
        draw.text((x,y), title, font=font(17,True), fill='#c9dedc')
        sheet.paste(crop.resize((512,256),Image.Resampling.NEAREST).convert('RGB'), (x,y+27))
    sheet.save(output/'game-world-equipment.png', optimize=True)


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


MUSIC_REGIONS = (
    (65, 105, "LANTERN REACHES", "SONAR LULLABY", "#2f76a8"),
    (1450, 105, "KELP GALLERIES", "KELP DRIFT", "#3b7f58"),
    (1496, 488, "BROKEN WORKS", "BUBBLEJET RUN", "#a25c1f"),
    (664, 1024, "GLASS TRENCH", "ABYSSAL GLASS", "#74509d"),
)
MUSIC_TINT = ((47, 118, 168), (59, 127, 88), (162, 92, 31), (116, 80, 157))


def music_water(native: Image.Image) -> Image.Image:
    """Tint only open water with the baked cleaned cue-region plane."""
    plane = (ROOT / "fxdata/tables/music-regions.bin").read_bytes()
    if len(plane) != 320 * 160:
        raise ValueError("expected a 320x160 music-region plane")
    region = Image.frombytes("L", (320, 160), plane).resize(native.size, Image.Resampling.NEAREST)
    tint = Image.merge("RGB", [region.point(list(channel) + [0] * (256 - len(channel)))
                               for channel in zip(*MUSIC_TINT)])
    water = native.convert("L").point(lambda value: 255 if value == 0 else 0)
    # Keep the tint dark enough for the white terrain and labels to stay readable.
    shade = Image.new("RGB", native.size, "#102532")
    tint = Image.blend(shade, tint, 0.78)
    return Image.composite(tint, native.convert("RGB"), water)


def build_overview(native: Image.Image, source, objects: bytes, music: bool) -> Image.Image:
    if objects[:4] != b"SHL2" or objects[6:8] != bytes((7, 5)):
        raise ValueError("unsupported exploration object records")
    gate_count, pickup_count = objects[4:6]
    gate_stride, pickup_stride = objects[6:8]
    header, footer = 120, 230 if music else 130
    canvas = Image.new("RGB", (native.width, native.height + header + footer), "#102532")
    canvas.paste(music_water(native) if music else native.convert("RGB"), (0, header))
    draw = ImageDraw.Draw(canvas)
    ink, muted = "#e4efea", "#9bb8c3"
    draw.text((40, 20), "SUB-HAULER / MUSIC REGIONS" if music else "SUB-HAULER / ACTUAL GAME WORLD", font=font(34, True), fill=ink)
    subtitle = ("Travel regions highlighted by song · berth-board overrides in the key · all secrets shown · new-run state"
                if music else
                "Native cave, station, gate, pickup and current renderers · all secrets shown · new-run state")
    draw.text((40, 70), subtitle, font=font(21), fill=muted)
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

    gate_styles = {0: ("#13819c", "TIDE"), 1: ("#b97a15", "C"), 2: ("#8255b2", "P"),
                   4: ("#13819c", "T"), 6: ("#8255b2", "P+T")}
    for i in range(gate_count):
        x, y, w, h, required = struct.unpack_from("<HHBBB", objects, 8 + i * 7)
        color, tag = gate_styles.get(required, ("#b97a15", f"0x{required:02X}"))
        draw.rectangle((x - 2, y + header - 2, x + w + 2, y + header + h + 2), outline=color, width=3)
        tw = draw.textlength(tag, font=f_small) + 10
        cx, cy = x + w / 2, y + header + h / 2
        draw.rounded_rectangle((cx - tw / 2, cy - 12, cx + tw / 2, cy + 12), radius=4, fill=color)
        draw.text((cx, cy - 9), tag, font=f_small, fill="white", anchor="ma")

    names = {1: "SALVAGE CUTTER", 2: "PRESSURE SEAL", 3: "CURRENT THRUSTER",
             6: "SURVEY LENS"}
    subtitle = {1: "opens grates", 2: "opens deep water", 3: "disables every tide",
                6: "reads the buried relay"}
    for i in range(pickup_count):
        offset = 8 + gate_count * gate_stride + i * pickup_stride
        x, y, kind = struct.unpack_from("<HHB", objects, offset)
        title = names[kind]
        side = "right" if x < 350 else "left"
        label(x, y, title, subtitle[kind], "#175c61", side, offset_y=-45 if kind == 6 else 0)

    pressure_y = source["pressure_y"] * source["tile_pixels"] + header
    for x in range(0, native.width, 24):
        draw.line((x, pressure_y, x + 11, pressure_y), fill="#a88cc2", width=2)
    draw.text((native.width - 35, pressure_y - 25), "HIGH PRESSURE BELOW · SEAL REQUIRED", font=font(17, True), fill="#78549b", anchor="ra", stroke_width=3, stroke_fill="white")
    if music:
        for x, y, title, song, color in MUSIC_REGIONS:
            draw.text((x, y + header), title, font=font(22, True), fill="#24333a",
                      stroke_width=3, stroke_fill="white")
            tw = draw.textlength(song, font=f_label) + 24
            by = y + header + 29
            draw.rounded_rectangle((x, by, x + tw, by + 31), radius=5, fill="#f8fcfa",
                                   outline=color, width=2)
            draw.text((x + 12, by + 5), song, font=f_label, fill=color)
    else:
        regions = [(65, 105, "LANTERN REACHES"), (1450, 105, "KELP GALLERIES"),
                   (1496, 488, "BROKEN WORKS"), (664, 1024, "GLASS TRENCH")]
        for x, y, text in regions:
            draw.text((x, y + header), text, font=font(22, True), fill="#58737c",
                      stroke_width=3, stroke_fill="white")

    y = header + native.height + 22
    if music:
        draw.text((40, y), "MUSIC KEY", font=font(19, True), fill=ink)
        for x, (color, song, region) in zip((220, 820, 1420, 2020), (
                ("#4b91c4", "SONAR LULLABY", "LANTERN REACHES"),
                ("#5baa78", "KELP DRIFT", "KELP GALLERIES"),
                ("#d08a3c", "BUBBLEJET RUN", "BROKEN WORKS"),
                ("#a47bd4", "ABYSSAL GLASS", "GLASS TRENCH"))):
            draw.rectangle((x, y + 3, x + 18, y + 21), fill=color)
            draw.text((x + 30, y), f"{song} · {region}", font=font(18), fill=ink)
        draw.text((40, y + 70), "BERTH BOARDS · TITLE / NEW VOYAGE & TIDEWALL: HARBOR BUBBLES · BENTHOS / CORMORANT: HARBOR BUBBLES UNTIL REPAIRED, THEN TIDEPOOL WALTZ", font=font(17), fill=ink)
        draw.text((40, y + 96), "DEEP ANCHOR: ABYSSAL GLASS UNTIL BEACON, THEN TIDEPOOL WALTZ · RESTORED SERVICE BERTHS + PUMP / LIFT / BEACON / ENDING: TIDEPOOL WALTZ", font=font(17), fill=ink)
        draw.text((40, y + 140), "GATE KEY", font=font(19, True), fill=ink)
        for x, color, text in [(220, "#e5af4d", "C · Cutter"), (520, "#c4a0e7", "P · Pressure seal"),
                               (930, "#69d4df", "TIDE · Thruster current")]:
            draw.rectangle((x, y + 143, x + 18, y + 161), fill=color)
            draw.text((x + 30, y + 140), text, font=font(20), fill=ink)
        draw.text((40, y + 182), "Travel cues come from the cleaned nearest-room music plane; berth-board cues override travel music. The native PNG keeps the exact 1-bit pixels.", font=font(17), fill=muted)
    else:
        draw.text((40, y), "GATE KEY", font=font(19, True), fill=ink)
        for x, color, text in [(220, "#e5af4d", "C · Cutter"), (520, "#c4a0e7", "P · Pressure seal"),
                               (930, "#69d4df", "TIDE · Thruster current")]:
            draw.rectangle((x, y + 3, x + 18, y + 21), fill=color)
            draw.text((x + 30, y), text, font=font(20), fill=ink)
        draw.text((40, y + 48), "The companion native PNG preserves the game's exact 1-bit pixels; this overview adds labels and colored object markers.", font=font(18), fill=muted)
    return canvas


def export(input_path: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    native = Image.open(input_path).convert("1")
    if native.size != (2560, 1280):
        raise ValueError("expected a 2560x1280 native world panorama")
    native.save(output / "game-world-native.png", optimize=True)
    export_regions(native, output)
    export_equipment(native, output)
    export_settlements(native, output)
    source = json.loads((ROOT / "levels/exploration.json").read_text())
    objects = (ROOT / "fxdata/tables/exploration-objects.bin").read_bytes()
    for music, name in ((False, "game-world.png"), (True, "game-world-music.png")):
        canvas = build_overview(native, source, objects, music)
        canvas.save(output / name, optimize=True)
        print(f"render-world: {output / name} ({canvas.width}x{canvas.height})")
    print(f"render-world: {output / 'game-world-native.png'} ({native.width}x{native.height}, 1-bit)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/world-render")
    args = parser.parse_args()
    export(args.input, args.output_dir)


if __name__ == "__main__":
    main()
