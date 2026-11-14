"""Hand-authored native-pixel area backdrops. All geometry is intentional.

No models, noise, dithering or resampling. Edit these shapes like a drawing.
Each 256x128 plate repeats behind a 128x64 viewport at half camera speed.
"""
from PIL import Image, ImageDraw

WIDTH, HEIGHT = 256, 128
AREAS = ("tide", "kelp", "works", "deep")


class Plate:
    def __init__(self):
        self.image = Image.new("1", (WIDTH, HEIGHT))
        self.pen = ImageDraw.Draw(self.image)

    def line(self, points, width=1):
        self.pen.line(points, fill=1, width=width)

    def poly(self, points):
        self.pen.polygon(points, fill=0, outline=1)

    def box(self, x, y, w, h):
        self.pen.rectangle((x, y, x + w, y + h), fill=0, outline=1)

    def oval(self, x, y, w, h):
        self.pen.ellipse((x, y, x + w, y + h), fill=0, outline=1)


def piling(p, x):
    p.box(x, -4, 7, 140)
    for y in (12, 49, 86, 123):
        p.line([(x - 2, y), (x + 9, y)])
        p.line([(x + 2, y + 5), (x + 2, y + 20)])


def chain(p, x, top, bottom):
    for y in range(top, bottom, 6):
        p.oval(x, y, 3, 4)


def habitat(p, x, y):
    # A long pressure hull on its cradle, with a conspicuous row of ports.
    p.poly([(x, y + 8), (x + 5, y + 2), (x + 46, y + 2),
            (x + 52, y + 8), (x + 52, y + 24), (x, y + 24)])
    p.line([(x + 3, y + 26), (x + 49, y + 26)])
    for dx in (7, 20, 33):
        p.oval(x + dx, y + 9, 7, 7)
    for dx in (5, 43):
        p.line([(x + dx, y + 25), (x + dx - 2, y + 35)])
    p.line([(x + 22, y + 2), (x + 22, y - 5), (x + 30, y - 5)])


def tide():
    p = Plate()
    for x in (10, 88, 166, 244):
        piling(p, x)
    habitat(p, 28, 22)
    habitat(p, 106, 77)
    habitat(p, 184, 23)
    chain(p, 125, -4, 35)
    p.oval(119, 36, 16, 16)
    p.oval(123, 40, 8, 8)
    chain(p, 207, 75, 112)
    p.line([(201, 113), (207, 119), (213, 113)])
    # Mooring cables sag behind the structures, never a surface waterline.
    p.line([(-8, 68), (20, 72), (48, 82), (71, 79), (92, 67)])
    p.line([(165, 91), (192, 96), (221, 92), (252, 76), (264, 75)])
    p.line([(-4, 118), (31, 114), (51, 120), (73, 117)])
    return p.image


def kelp_stalk(p, x, y, bend):
    p.line([(x - 3, y + 103), (x, y + 81), (x - bend, y + 60),
            (x + bend, y + 38), (x, y + 18), (x + bend, y)], 2)
    # Broad lance-shaped leaves stay recognizable at native size.
    for dx, dy, side in ((0, 19, -1), (bend, 36, 1), (-bend, 56, -1), (0, 78, 1)):
        base = x + dx
        p.poly([(base, y + dy + 5), (base + side * 13, y + dy - 6),
                (base + side * 16, y + dy - 7), (base + side * 12, y + dy + 2)])
    p.line([(x - 9, y + 108), (x - 3, y + 103), (x + 5, y + 108)])


def kelp():
    p = Plate()
    # Two overlapping arch faces give the forest a cave behind its canopy.
    p.line([(-6, 29), (9, 15), (28, 8), (51, 9), (70, 20), (81, 39), (85, 63)])
    p.line([(109, 69), (112, 39), (122, 20), (142, 12), (162, 13), (182, 25), (191, 43)])
    p.line([(208, 60), (213, 29), (228, 12), (247, 8), (264, 12)])
    for x, y, bend in ((19, 6, 4), (70, 39, -4), (124, -14, 5), (180, 34, 4), (233, -8, -4)):
        kelp_stalk(p, x, y, bend)
    for x, y in ((41, 109), (95, 120), (151, 96), (211, 117)):
        p.poly([(x - 12, y + 7), (x - 8, y), (x + 2, y - 3), (x + 12, y + 2), (x + 15, y + 8)])
        p.line([(x - 7, y + 4), (x + 1, y + 1), (x + 7, y + 3)])
    return p.image


def gantry(p, x):
    p.box(x, -5, 14, 139)
    for y in (-1, 27, 55, 83, 111):
        p.line([(x + 1, y), (x + 13, y + 24), (x + 1, y + 24), (x + 13, y)])
        p.line([(x - 2, y + 25), (x + 16, y + 25)])


def turbine(p, x, y):
    p.oval(x, y, 28, 28)
    p.oval(x + 4, y + 4, 20, 20)
    p.oval(x + 11, y + 11, 6, 6)
    for dx, dy in ((0, -9), (9, 0), (0, 9), (-9, 0)):
        p.line([(x + 14, y + 14), (x + 14 + dx, y + 14 + dy)])
    p.box(x + 3, y + 30, 22, 4)


def works():
    p = Plate()
    for x in (10, 116, 242):
        gantry(p, x)
    # A pipe circuit, elbow bends and flange collars, all behind the playfield.
    p.line([(-4, 17), (42, 17), (42, 37), (80, 37), (80, 17), (159, 17), (159, 47), (199, 47), (199, 17), (262, 17)])
    p.line([(-4, 21), (38, 21), (38, 41), (84, 41), (84, 21), (155, 21), (155, 51), (203, 51), (203, 21), (262, 21)])
    for x in (31, 99, 219):
        p.box(x, 14, 3, 10)
    turbine(p, 49, 62)
    turbine(p, 178, 77)
    p.box(91, 59, 19, 27)
    p.line([(95, 65), (106, 65), (106, 69), (95, 69), (95, 65)])
    p.line([(101, 86), (101, 102), (149, 102), (149, 76)])
    chain(p, 168, -2, 33)
    p.line([(169, 34), (169, 39), (175, 39), (178, 35)])
    p.line([(-4, 112), (40, 112), (40, 116), (97, 116), (97, 112), (158, 112), (158, 116), (230, 116), (230, 112), (264, 112)])
    return p.image


def basalt(p, x, top, width, bottom):
    p.poly([(x, top + 9), (x + width // 2, top), (x + width, top + 7),
            (x + width - 2, bottom), (x + 2, bottom + 4)])
    p.line([(x + width // 2, top + 1), (x + width // 2 - 2, top + 24),
            (x + width // 2 + 2, top + 52), (x + width // 2, bottom)])
    for y in range(top + 24, bottom, 27):
        p.line([(x + 2, y + 2), (x + width // 2, y - 1), (x + width - 2, y + 3)])


def crystals(p, x, y):
    p.poly([(x - 13, y), (x - 22, y - 21), (x - 14, y - 26), (x + 1, y)])
    p.poly([(x - 4, y), (x - 6, y - 37), (x + 2, y - 46), (x + 9, y - 34), (x + 9, y)])
    p.line([(x + 2, y - 45), (x + 2, y - 5)])
    p.poly([(x + 10, y), (x + 15, y - 22), (x + 24, y - 28), (x + 28, y - 23), (x + 20, y)])


def deep():
    p = Plate()
    basalt(p, 8, -13, 24, 102)
    basalt(p, 65, 17, 25, 137)
    basalt(p, 145, -17, 29, 85)
    basalt(p, 219, 11, 26, 140)
    crystals(p, 48, 121)
    crystals(p, 121, 63)
    crystals(p, 196, 122)
    # Long detached fracture lines and mineral shelves connect the columns.
    p.line([(-3, 109), (16, 105), (37, 109), (55, 106)])
    p.line([(90, 88), (110, 83), (128, 87), (143, 81)])
    p.line([(173, 31), (188, 36), (203, 33), (219, 39)])
    return p.image


def plates():
    return tuple(globals()[name]() for name in AREAS)
