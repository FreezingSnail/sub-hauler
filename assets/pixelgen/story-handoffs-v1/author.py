#!/usr/bin/env python3
"""Author isolated native cargo layers; pixelgen owns compositing and replay.

Run after the retained raster sources have been converted with pixelgen.
No conversion, dithering, mask derivation, or frame composition occurs here.
"""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCENES = ('tidewall-filters', 'deep-anchor-dispatch', 'cormorant-core', 'restoration-ready')


def shift(x=0, y=0, turn=0):
    return {'translate': {'x': x, 'y': y}, 'turn_16ths': turn}


def author(name):
    folder = ROOT / name / 'source'
    native = Image.open(folder / 'native.png')
    assert native.mode == '1' and native.size == (128, 64)
    background = native.copy()
    layers = []

    def asset(path, im):
        im.save(folder / path)
        return {'path': path, 'sha256': hashlib.sha256((folder / path).read_bytes()).hexdigest()}

    def part(label, im, origin, track, matte=None, pivot=None):
        layer = {'layer_id': label, 'role': 'part', 'z': len(layers)*10+10,
                 'source': asset(label+'.png', im),
                 'matte': asset(label+'-matte.png', matte or Image.new('1', im.size, 1)),
                 'origin': {'x': origin[0], 'y': origin[1]}, 'track': track, 'clip': 'error'}
        if pivot:
            layer['pivot'] = {'x': pivot[0], 'y': pivot[1]}
        layers.append(layer)

    d = ImageDraw.Draw(background)
    if name == 'tidewall-filters':
        # Clear only generated ambiguous cargo. The generated hull stays intact.
        d.rectangle((91, 18, 121, 37), fill=0)
        cargo = Image.new('1', (27, 15))
        q = ImageDraw.Draw(cargo)
        for x in (0, 15):
            q.rectangle((x, 0, x+10, 14), outline=1)
            for y in (3, 6, 9, 12):
                q.line((x+2, y, x+8, y), fill=1)
        part('pleated-filter-pair', cargo, (93, 19), [shift(), shift(y=2), shift(y=4), shift(x=-1, y=2)])
    elif name == 'deep-anchor-dispatch':
        d.rectangle((81, 20, 120, 40), fill=0)
        # Physical dispatch folio with clasp, beside two compact supply boxes.
        cargo = Image.new('1', (35, 16))
        q = ImageDraw.Draw(cargo)
        q.rectangle((0, 0, 17, 15), outline=1, width=2)
        q.line((3, 4, 14, 4), fill=1)
        q.rectangle((7, 7, 10, 10), fill=1)
        q.rectangle((21, 6, 34, 15), outline=1)
        q.line((27, 7, 27, 14), fill=1)
        q.rectangle((22, 0, 33, 4), outline=1)
        part('sealed-records-and-supplies', cargo, (83, 21),
             [shift(), shift(y=2), shift(y=4), shift(x=-1, y=2)])
    elif name == 'cormorant-core':
        d.rectangle((89, 19, 117, 39), fill=0)
        cargo = Image.new('1', (15, 17))
        q = ImageDraw.Draw(cargo)
        q.rounded_rectangle((1, 0, 13, 16), radius=3, outline=1)
        q.rectangle((0, 3, 14, 5), fill=1)
        q.rectangle((0, 11, 14, 13), fill=1)
        q.rectangle((5, 7, 9, 9), outline=1)
        part('sealed-beacon-core', cargo, (97, 20), [shift(), shift(y=2), shift(y=4), shift(x=-1, y=2)])
    else:
        # Completed pump: enclosed working rotor with an eccentric readable pin.
        d.rectangle((7, 34, 20, 45), fill=0)
        rotor = Image.new('1', (11, 11))
        q = ImageDraw.Draw(rotor)
        q.ellipse((0, 0, 10, 10), outline=1)
        q.line((5, 1, 5, 9), fill=1)
        q.line((1, 5, 9, 5), fill=1)
        q.rectangle((7, 2, 8, 3), fill=1)
        matte = Image.new('1', (11, 11))
        ImageDraw.Draw(matte).ellipse((0, 0, 10, 10), fill=1)
        part('working-pump-rotor', rotor, (8, 35),
             [shift(turn=p) for p in (0, 2, 4, 6)], matte, (13, 40))
        # Working winch's continuous circulation is distinct from a fault blink.
        dash = Image.new('1', (7, 2), 1)
        part('running-lift-cable-marker', dash, (107, 23),
             [shift(y=y) for y in (0, 3, 6, 3)])
    # Quiet black bilge below the dock removes nonessential high-entropy grit.
    # This preserves the entire pressure hull and the cargo/machinery above it.
    d.rectangle((0, 56, 127, 63), fill=0)
    recipe = {'schema_version': 'pixelgen.motion/1', 'recipe_id': 'story-handoffs-v1-'+name,
              'revision': 3, 'canvas': {'width': 128, 'height': 64, 'depth': 1},
              'background': asset('background.png', background),
              'master_seed': 73101+SCENES.index(name)*4,
              'phases': [{'phase_id': 'pose-'+str(p), 'duration_ms': 300} for p in range(4)],
              'layers': layers}
    (folder / 'recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')

if __name__ == '__main__':
    for scene in SCENES:
        author(scene)
