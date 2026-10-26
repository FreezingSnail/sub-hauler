"""Offline native pixel scenery. Called by gen_level through make gen only.

Solid collision silhouettes stay authoritative. Background objects are hollow
line art, kept outside the hull's route corridor and all interactive markers.
Nothing from this module executes on the device.
"""
from collections import deque
import math

from PIL import Image, ImageChops, ImageDraw, ImageFilter
import gen_contours

WIDTH, HEIGHT = 2560, 1280
REGIONS = ('tide', 'kelp', 'works', 'deep')


def protection(level, spines):
    mask = Image.new('1', (WIDTH, HEIGHT))
    draw = ImageDraw.Draw(mask)
    draw.rectangle((0, 0, WIDTH-1, HEIGHT-1), outline=255, width=8)
    for spine in spines:
        draw.line([(round(x*8), round(y*8)) for x, y in spine], fill=255, width=30)
    for room in level.source['rooms']:
        x, y = (v*8 for v in room['position'])
        draw.rectangle((x-20, y-12, x+20, y+12), fill=255)
        if 'station' in room:
            draw.rectangle((x-32, y-27, x+32, y+17), fill=255)
    for g in level.gates:
        draw.rectangle((g.x-7, g.y-7, g.x+g.w+7, g.y+g.h+7), fill=255)
    for p in level.pickups:
        draw.rectangle((p.x-13, p.y-13, p.x+13, p.y+13), fill=255)
    for dock in level.world.settlements:
        x,y=dock.pixel_x,dock.pixel_y
        draw.rectangle((x-30,y-20,x+30,y+36),fill=255)
    x, y = level.world.spawn_sub_x, level.world.spawn_sub_y
    draw.rectangle((x-28, y-16, x+28, y+16), fill=255)
    return mask


def motif(kind, seed):
    """Small hollow silhouettes, anchored at their bottom centre."""
    image = Image.new('1', (64, 56))
    d = ImageDraw.Draw(image)
    ox, oy = 31, 54
    def line(points):
        d.line([(ox+x, oy+y) for x, y in points], fill=255)
    def box(x, y, w, h):
        d.rectangle((ox+x, oy+y, ox+x+w-1, oy+y+h-1), outline=255)
    def oval(x, y, w, h):
        d.ellipse((ox+x, oy+y, ox+x+w-1, oy+y+h-1), outline=255)
    if kind == 'coral':
        height = 9 + seed % 8
        line([(0, 0), (0, -height)])
        for n in range(3, height, 4):
            side = 1 if n % 8 else -1
            line([(0, -n), (side*4, -n-3), (side*4, -n-5)])
        line([(-5, 0), (-2, -2), (2, -2), (5, 0)])
    elif kind == 'kelp':
        height = 22 + seed % 15
        stem = [(round(2*math.sin(n/6+seed)), -n) for n in range(height)]
        line(stem)
        for n in range(6, height-2, 6):
            x, y = stem[n]
            side = 1 if n % 12 else -1
            line([(x, y), (x+side*6, y-5), (x+side*5, y-1), (x, y+1)])
        line([(-4, 0), (0, -2), (4, 0)])
    elif kind == 'scrap':
        box(-8, -10, 17, 9)
        line([(-7, -3), (1, -9), (7, -3)])
        line([(-11, 0), (-4, 0), (-3, -2), (7, -2), (10, 0)])
    elif kind == 'crystal':
        for x, h, w in [(-8, 13, 4), (0, 25+seed%7, 5), (8, 17, 4)]:
            line([(x-w, 0), (x-w, -h+5), (x, -h), (x+w, -h+5), (x+w, 0)])
            line([(x, -h), (x+1, -3)])
        line([(-13, 0), (12, 0)])
    elif kind == 'buoy':
        for y in range(0, -16, -4):
            oval(-1, y-3, 3, 4)
        oval(-5, -27, 11, 10)
        line([(-6, -22), (6, -22)])
        line([(-2, -28), (0, -31), (2, -28)])
    elif kind == 'anchor':
        oval(-3, -29, 7, 7)
        line([(0, -23), (0, -3), (-10, -10), (-11, -15)])
        line([(0, -3), (10, -10), (11, -15)])
        line([(-7, -20), (7, -20)])
        line([(-13, -13), (-11, -16), (-8, -13)])
        line([(8, -13), (11, -16), (13, -13)])
    elif kind in ('tender', 'wreck'):
        line([(-23, -12), (-17, -2), (15, -2), (24, -11)])
        line([(-23, -12), (-4, -12)])
        line([(3, -12), (24, -11)])
        for x in range(-15, 18, 8):
            line([(x, -3), (x-3, -10)])
        box(2, -20, 14, 8)
        box(5, -18, 3, 3)
        box(11, -18, 3, 3)
        if kind == 'wreck':
            line([(-5, -12), (-9, -23), (-4, -26)])
            line([(16, -2), (21, 0)])
        else:
            line([(9, -21), (9, -28), (14, -25)])
    elif kind == 'pump':
        oval(-8, -29, 17, 6)
        line([(-8, -26), (-8, -7), (8, -7), (8, -26)])
        oval(-8, -11, 17, 6)
        box(-4, -22, 8, 6)
        line([(9, -18), (17, -18), (17, -3), (21, -3)])
        line([(-9, -13), (-15, -13), (-15, -4), (-20, -4)])
        line([(-11, 0), (-11, -5), (11, -5), (11, 0)])
    elif kind == 'relay':
        box(-10, -21, 21, 18)
        box(-7, -18, 14, 7)
        for x in (-6, 0, 6):
            line([(x, -7), (x+1, -7)])
        line([(-6, -22), (-6, -30), (-13, -34)])
        line([(-14, -30), (-11, -36), (-5, -36)])
        line([(-12, 0), (-12, -2), (12, -2), (12, 0)])
    elif kind == 'crane':
        line([(-15, 0), (-15, -32), (16, -32), (16, -28), (-11, -28), (-11, 0)])
        line([(-11, -28), (4, -32)])
        line([(11, -27), (11, -11), (7, -8), (7, -5), (12, -5), (14, -8)])
        line([(-20, 0), (-20, -3), (-6, -3), (-6, 0)])
    elif kind == 'winch':
        oval(-12, -25, 25, 24)
        oval(-6, -19, 13, 12)
        line([(-10, -13), (10, -13)])
        line([(0, -23), (0, -3)])
        line([(-15, 0), (-15, -3), (15, -3), (15, 0)])
    elif kind == 'vent':
        line([(-11, 0), (-9, -17), (-4, -19), (-2, -1)])
        line([(1, 0), (3, -25), (8, -23), (11, 0)])
        line([(-10, -4), (10, -4)])
        for x, y in ((4, -31), (-2, -37), (6, -44)):
            oval(x, y, 3, 3)
    else:
        raise ValueError(f'unknown scenery motif: {kind}')
    return {(x-ox, y-oy) for y in range(image.height) for x in range(image.width) if image.getpixel((x, y))}


def worn_plating(draw, x, y, seed, gradient):
    """Broken wall-aligned seams, rather than a repeating grid of boxes."""
    bits = ((x*73856093) ^ (y*19349663) ^ (seed*83492791)) & 0xFFFFFFFF
    bits = ((bits ^ (bits >> 13))*1274126177) & 0xFFFFFFFF
    if bits & 3 == 0:
        return
    x += ((bits >> 5) % 11)-5
    y += ((bits >> 10) % 9)-4
    gx, gy = gradient
    length = math.hypot(gx,gy)
    tx, ty = (-gy/length, gx/length) if length else ((1,0) if bits & 16 else (0,1))
    def point(u,v):
        return round(x+u*tx-v*ty), round(y+u*ty+v*tx)
    def line(points):
        draw.line([point(u,v) for u,v in points],fill=255)
    kind = (bits >> 17) % 6
    if kind == 0:
        # A torn overlapping edge, with its middle missing.
        line([(-13,-3),(-6,-3),(-3,0)])
        line([(3,1),(9,1),(12,4)])
        line([(-10,-6),(-5,-6)])
    elif kind == 1:
        # Buckled joint; the offset leaves the plates visibly uneven.
        line([(-10,-6),(-2,-6),(2,-2),(2,6)])
        line([(5,-1),(5,3)])
    elif kind == 2:
        # A short rivet run with one missing fastener.
        for u in (-9,-3,9):
            draw.point(point(u,0),fill=255)
            draw.point(point(u,1),fill=255)
        line([(-7,5),(1,5)])
    elif kind == 3:
        # Bent structural rib, mostly worn away.
        line([(-12,2),(-5,-1),(3,-1),(10,-4)])
        line([(-4,2),(3,2)])
    elif kind == 4:
        # Corrosion chips around a small plate tear.
        line([(-6,-4),(-2,-6),(1,-4)])
        line([(0,1),(5,2),(8,0)])
        draw.point(point(-9,3),fill=255)
    else:
        # Partial T seam. No closed panel or central drawer-like handle.
        line([(-11,-3),(-4,-3)])
        line([(0,-3),(9,-3),(12,0)])
        line([(-2,0),(-2,6)])


def decorate(level, base, spines):
    config = level.source.get('scenery')
    if not config:
        return base, {}
    if config.get('version') != 1:
        raise ValueError('unsupported scenery version')
    rooms = {r['id']: r for r in level.source['rooms']}
    if any(r['region'] not in REGIONS for r in rooms.values()):
        raise ValueError('unknown scenery region')
    seed = config['seed']
    grid = level.world.grid
    solid = gen_contours.solid_image(grid)
    solid_pixels = solid.load()
    protected = protection(level, spines)
    safe = protected.load()
    white, black = set(), set()
    stats = {'landmarks': [], 'ambient': dict.fromkeys(REGIONS, 0)}
    occupied = Image.new('1', (WIDTH, HEIGHT))
    occupied_draw = ImageDraw.Draw(occupied)

    def region(tx, ty):
        return min(rooms.values(), key=lambda r: (r['position'][0]-tx)**2+(r['position'][1]-ty)**2)['region']
    def water(x, y):
        return 8 <= x < WIDTH-8 and 8 <= y < HEIGHT-8 and not solid_pixels[x,y]
    floors = []
    for y in range(2,158):
        for x in range(2,318):
            if not grid[y*320+x] or grid[(y-1)*320+x]: continue
            px = x*8+4
            # Re-anchor scenery to the actual curved floor, rather than the
            # old tile edge. No floating foundations or tile-shaped cut-outs.
            candidates = [py for py in range(y*8-7,y*8+8)
                          if solid_pixels[px,py] and not solid_pixels[px,py-1]]
            if candidates:
                py = min(candidates,key=lambda v:abs(v-y*8))
                floors.append((px,py,region(x,y)))

    def place(kind, x, y, variant):
        points = {(x+px, y+py) for px, py in motif(kind, variant)}
        if any(not (8 <= px < WIDTH-8 and 8 <= py < HEIGHT-8) or safe[px, py] for px, py in points):
            return False
        if sum(water(px, py) for px, py in points) < len(points)*.75:
            return False
        if any(occupied.getpixel((px, py)) for px, py in points):
            return False
        white.update((px, py) for px, py in points if water(px, py))
        xs, ys = zip(*points)
        occupied_draw.rectangle((min(xs)-3, min(ys)-3, max(xs)+3, max(ys)+3), fill=255)
        return True

    for i, item in enumerate(config['landmarks']):
        room = rooms[item['room']]
        cx, cy = (v*8 for v in room['position'])
        dx, dy = item.get('offset', [0, 40])
        candidates = sorted((p for p in floors if p[2] == room['region'] and abs(p[0]-cx) < 100 and abs(p[1]-cy) < 100),
                            key=lambda p: (p[0]-cx-dx)**2+(p[1]-cy-dy)**2)
        for x, y, _ in candidates:
            if place(item['kind'], x, y, seed+i):
                stats['landmarks'].append({'id': item['id'], 'room': item['room'], 'kind': item['kind'], 'x': x, 'y': y})
                break
        else:
            raise ValueError(f'no safe scenery placement for {item["id"]}')

    ambient = {'tide': 'coral', 'kelp': 'kelp', 'works': 'scrap', 'deep': 'crystal'}
    for x, y, theme in floors:
        # Choose growths from the stable coarse floor identity, so refining a
        # contour moves their roots instead of reshuffling the whole biome.
        coarse_y = round(y/8)*8+1
        value = (x*71+coarse_y*113+seed) & 0xFFFF
        if value % (3 if theme == 'kelp' else 5):
            continue
        if place(ambient[theme], x, y, value):
            stats['ambient'][theme] += 1

    # Rock decoration stays near the water and three pixels inside the actual
    # wall. It never nicks the terrain silhouette or makes a false opening.
    distance = bytearray([255])*len(grid)
    queue = deque(i for i, solid in enumerate(grid) if not solid)
    for i in queue: distance[i] = 0
    while queue:
        i = queue.popleft()
        if distance[i] >= 6: continue
        x, y = i % 320, i // 320
        for nx, ny in ((x-1,y), (x+1,y), (x,y-1), (x,y+1)):
            if 0 <= nx < 320 and 0 <= ny < 160:
                j = ny*320+nx
                if distance[j] > distance[i]+1:
                    distance[j] = distance[i]+1
                    queue.append(j)
    interior = solid.filter(ImageFilter.MinFilter(7))
    ink = Image.new('1', (WIDTH,HEIGHT))
    d = ImageDraw.Draw(ink)
    for y in range(24, HEIGHT-24, 24):
        for x in range(24, WIDTH-24, 32):
            tx, ty = x//8, y//8
            if not 1 <= distance[ty*320+tx] <= 6: continue
            theme = region(tx, ty)
            phase = (x*13+y*7+seed) % 7
            if theme == 'tide':
                d.line([(x-13,y+2), (x-5,y), (x+4,y+1), (x+14,y-1)], fill=255)
                d.line([(x-7,y+5), (x+5,y+5)], fill=255)
            elif theme == 'kelp':
                d.ellipse((x-8,y-3,x+2,y+2), outline=255)
                d.ellipse((x+5,y+4,x+11,y+8), outline=255)
                d.line((x-7,y+9,x-2,y+9), fill=255)
            elif theme == 'works':
                gradient = (int(distance[ty*320+tx+1])-int(distance[ty*320+tx-1]),
                            int(distance[(ty+1)*320+tx])-int(distance[(ty-1)*320+tx]))
                worn_plating(d,x,y,seed,gradient)
            else:
                d.line([(x-10,y-9), (x-3,y-3), (x+1,y+5), (x+11,y+11)], fill=255)
                d.line([(x-3,y-3), (x+5,y-8), (x+10,y-8)], fill=255)
                if phase < 3: d.line([(x+1,y+5), (x-7,y+10)], fill=255)
    ink = ImageChops.logical_and(ink, interior.convert('1'))
    ink = ImageChops.logical_and(ink, ImageChops.invert(protected))
    pixels = ink.load()
    for y in range(8, HEIGHT-8):
        for x in range(8, WIDTH-8):
            if pixels[x,y]: black.add((x,y))
    result = bytearray(base)
    for points, on in ((black, False), (white, True)):
        for x, y in points:
            index, bit = (y//8)*WIDTH+x, 1 << (y&7)
            if on: result[index] |= bit
            else: result[index] &= ~bit
    stats['white_pixels'], stats['rock_pixels'] = len(white), len(black)
    return bytes(result), stats
