#!/usr/bin/env python3
"""Author native story layers/recipes; pixelgen owns rendering and replay.

The previous still compositions remain the clean-plate references. This tool
draws isolated replacement parts and their mattes, never animation frames.
Run pixelgen animate motion render/check/replay for each emitted recipe.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ANCHORS = ROOT / "docs/assets/first-beat-animation/all-beats-v3"
SCENES = (
    "arrival", "grow-tanks", "repair-bay", "pump-approach",
    "pump-cracked-impeller", "delivery-loading", "delivery-water",
    "lift-yard", "lift-cable", "lift-delivery-cable",
    "lift-delivery-working", "beacon-descent", "beacon-lit",
    "home-return", "home-lights",
)

# Exact native crops, with no scaling, redrawing or threshold changes. The
# surrounding water is black; the HOME foreground bollard is restored above
# the moving hull as a separate static layer.
ORIGINAL_SUB_BOXES = {
    "arrival": (4, 4, 67, 45),
    "pump-approach": (10, 9, 62, 30),
    "lift-yard": (10, 10, 64, 28),
    "beacon-descent": (22, 12, 57, 31),
    "home-return": (28, 15, 73, 41),
    "home-lights": (9, 9, 80, 45),
}


def canvas(size):
    return Image.new("1", size, 0)


def shift(x=0, y=0, turn=0):
    return {"translate": {"x": x, "y": y}, "turn_16ths": turn}


class Scene:
    def __init__(self, name, output):
        self.name = name
        self.root = output / name
        (self.root / "layers").mkdir(parents=True, exist_ok=True)
        self.background = Image.open(ANCHORS / (name + "-0.png")).convert("1")
        self.draw = ImageDraw.Draw(self.background)
        self.layers = []

    def asset(self, name, image):
        path = self.root / name
        image.save(path)
        return {"path": name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

    def part(self, name, image, origin, track=None, matte=None, pivot=None, poses=None):
        if matte is None:
            matte = Image.new("1", image.size, 255)
        layer = {
            "layer_id": name, "role": "part" if track else "static",
            "z": len(self.layers) * 10 + 10,
            "source": self.asset("layers/" + name + ".png", image),
            "matte": self.asset("layers/" + name + "-matte.png", matte),
            "origin": {"x": origin[0], "y": origin[1]},
        }
        if track:
            layer["track"] = track
            layer["clip"] = "error"
        if pivot:
            layer["pivot"] = {"x": pivot[0], "y": pivot[1]}
        if poses:
            layer["poses"] = {
                key: {
                    "source": self.asset("layers/" + name + "-" + key + ".png", img),
                    "matte": layer["matte"],
                } for key, img in poses.items()
            }
        self.layers.append(layer)

    def bubbles(self, positions, rise=4):
        for i, (x, y) in enumerate(positions):
            size = 5 if i % 2 == 0 else 4
            image = canvas((size, size))
            d = ImageDraw.Draw(image)
            # A dark rim and bright centre remain visible against both glass
            # and black water. The matte includes the dark rim.
            d.ellipse((1, 1, size - 2, size - 2), fill=1)
            matte = canvas((size, size))
            ImageDraw.Draw(matte).ellipse((0, 0, size - 1, size - 1), fill=1)
            # Stagger birth/rebirth along independent short lanes.
            track = [shift((p + i) % 2, -rise * ((p + i) % 4)) for p in range(4)]
            self.part("bubble-" + str(i), image, (x, y), track, matte=matte)

    def original_sub(self):
        box = ORIGINAL_SUB_BOXES[self.name]
        image = self.background.crop(box)
        foreground_box = (0, 28, 18, 49) if self.name == "home-lights" else None
        foreground = self.background.crop(foreground_box) if foreground_box else None
        self.draw.rectangle((box[0], box[1], box[2]-1, box[3]-1), fill=0)
        track = [shift(y=y) for y in (0, -1, 0, 1)]
        self.part("original-pressure-hull", image, box[:2], track)
        if foreground_box:
            self.part("foreground-bollard", foreground, foreground_box[:2])

    def reel(self, origin, diameter=22):
        image = canvas((diameter, diameter))
        d = ImageDraw.Draw(image)
        d.ellipse((1, 1, diameter-2, diameter-2), fill=1)
        d.ellipse((4, 4, diameter-5, diameter-5), fill=0)
        d.ellipse((6, 6, diameter-7, diameter-7), outline=1)
        self.part("reel-rim", image, origin)
        disk = canvas((diameter-10, diameter-10))
        d = ImageDraw.Draw(disk)
        c = disk.width // 2
        d.line((c, 0, c, disk.height-1), fill=1, width=2)
        d.line((0, c, disk.width-1, c), fill=1, width=2)
        d.rectangle((c-1, c-1, c+1, c+1), fill=1)
        self.part("reel-spokes", disk, (origin[0]+5, origin[1]+5),
                  [shift(turn=p) for p in range(4)], matte=disk,
                  pivot=(origin[0]+5+c, origin[1]+5+c))

    def write(self):
        recipe = {
            "schema_version": "pixelgen.motion/1", "recipe_id": "story-v5-" + self.name,
            "revision": 2, "canvas": {"width": 128, "height": 64, "depth": 1},
            "background": self.asset("background.png", self.background),
            "master_seed": 4100 + SCENES.index(self.name) * 4,
            "phases": [{"phase_id": "pose-" + str(p), "duration_ms": 300} for p in range(4)],
            "layers": self.layers,
        }
        (self.root / "recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")


def author(name, output):
    s = Scene(name, output)
    if name == "arrival":
        s.original_sub()
        s.bubbles([(4, 54), (81, 20)], rise=3)
    elif name == "home-return":
        s.original_sub()
        s.bubbles([(23, 37), (82, 50)], rise=3)
    elif name == "home-lights":
        s.original_sub()
        s.bubbles([(82, 18), (20, 55)], rise=3)
        # Modules light in sequence; the building geometry stays fixed.
        for i, (x, y) in enumerate(((89, 40), (102, 35), (116, 43))):
            on, off = canvas((4, 3)), canvas((4, 3))
            ImageDraw.Draw(on).rectangle((0, 0, 3, 2), fill=1)
            track = [{"pose": "lit" if (p+i)%4 < 3 else "dark"} for p in range(4)]
            s.part("window-"+str(i), on, (x, y), track, poses={"lit": on, "dark": off})
    elif name == "pump-approach":
        s.original_sub()
        s.bubbles([(68, 32), (114, 27)], rise=4)
    elif name == "lift-yard":
        s.original_sub()
        s.bubbles([(67, 27), (80, 48)], rise=4)
    elif name == "beacon-descent":
        s.original_sub()
        s.bubbles([(77, 38), (90, 23)], rise=4)
    elif name in ("grow-tanks", "delivery-water"):
        if name == "grow-tanks":
            s.bubbles([(35, 33), (85, 36), (26, 20)], rise=4)
        else:
            s.bubbles([(34, 28), (61, 37), (18, 27)], rise=4)
    elif name == "pump-cracked-impeller":
        # Separate the fracture from the surrounding speckled casing.
        s.draw.line((48, 24, 55, 29), fill=0, width=3)
        s.draw.line((51, 30, 57, 35), fill=0, width=2)
        s.bubbles([(109, 39), (119, 28), (102, 20)], rise=4)
    elif name == "repair-bay":
        s.draw.rectangle((108, 6, 117, 46), fill=0)
        s.draw.line((112, 0, 112, 17), fill=1, width=2)
        hook = canvas((11, 24)); d = ImageDraw.Draw(hook)
        d.line((5, 0, 5, 14), fill=1, width=2)
        d.arc((2, 12, 9, 20), 0, 260, fill=1, width=2)
        s.part("lift-hook", hook, (107, 17),
               [shift(x=x, y=y) for x, y in ((0, 0), (1, 0), (0, 1), (-1, 0))])
    elif name == "delivery-loading":
        # Crane lowers the replacement into the internal hold, beside the dock.
        s.draw.rectangle((44, 5, 59, 24), fill=0)
        s.draw.line((51, 0, 51, 18), fill=1)
        load = canvas((15, 18)); d = ImageDraw.Draw(load)
        d.line((7, 0, 7, 7), fill=1)
        d.ellipse((2, 6, 12, 16), fill=1)
        d.ellipse((4, 8, 10, 14), fill=0)
        d.line((7, 8, 7, 14), fill=1)
        d.line((4, 11, 10, 11), fill=1)
        s.part("lowered-impeller", load, (44, 9),
               [shift(x=x, y=y) for x, y in ((0, 0), (1, 2), (0, 4), (-1, 2))])
    elif name == "lift-cable":
        # Two broad cable silhouettes and a clear gap read as a snapped cable.
        s.draw.rectangle((32, 0, 73, 61), fill=0)
        s.draw.polygon([(35,0),(62,0),(58,8),(41,8)], fill=1)
        s.draw.line((47, 8, 47, 20), fill=1, width=3)
        s.draw.line((51, 8, 51, 19), fill=1, width=2)
        s.draw.line((47, 19, 44, 24), fill=1, width=2)
        s.draw.line((51, 18, 54, 23), fill=1)
        s.draw.line((36, 55, 69, 55), fill=1, width=2)
        end = canvas((15, 30)); d = ImageDraw.Draw(end)
        d.line((7, 10, 7, 29), fill=1, width=3)
        d.line((4, 9, 4, 29), fill=1)
        d.line((7, 10, 11, 4), fill=1, width=2)
        d.line((5, 10, 1, 6), fill=1)
        s.part("frayed-cable", end, (43, 27), [shift(x=x) for x in (0, 1, 0, -1)])
        s.bubbles([(73, 39)], rise=4)
    elif name == "lift-delivery-cable":
        s.draw.rectangle((26, 22, 49, 43), fill=0)
        s.reel((26, 22), diameter=22)
        # Two separated cable runs make the reel direction readable.
        s.draw.line((46, 33, 55, 33), fill=1)
        s.draw.line((46, 36, 53, 36), fill=1)
    elif name == "lift-delivery-working":
        s.draw.rectangle((41, 12, 78, 44), fill=0)
        s.draw.line((42, 7, 42, 52), fill=1)
        s.draw.line((64, 7, 64, 52), fill=1)
        s.draw.line((53, 4, 53, 28), fill=1)
        cage = canvas((19, 17)); d = ImageDraw.Draw(cage)
        d.rectangle((1, 1, 17, 14), outline=1)
        d.rectangle((3, 4, 15, 12), fill=1)
        d.line((4, 5, 14, 11), fill=0)
        d.line((14, 5, 4, 11), fill=0)
        d.rectangle((0, 15, 18, 16), fill=1)
        s.part("freight-carriage", cage, (44, 24), [shift(y=y) for y in (0, -2, 0, 2)])
        s.bubbles([(73, 36)], rise=3)
    elif name == "beacon-lit":
        beam = canvas((35, 14)); d = ImageDraw.Draw(beam)
        d.line((0, 0, 34, 6), fill=1)
        d.line((0, 13, 34, 6), fill=1)
        d.line((8, 6, 34, 6), fill=1)
        s.part("beacon-sweep", beam, (70, 5),
               [shift(turn=t) for t in (0, 15, 14, 15)], matte=beam, pivot=(104, 11))
        s.bubbles([(83, 45), (63, 31)], rise=3)
    s.write()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    for name in SCENES:
        author(name, args.out)
    print("authored {} story recipes (no frames rendered)".format(len(SCENES)))


if __name__ == "__main__":
    main()
