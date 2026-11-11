#!/usr/bin/env python3
"""Pack the authored exploration level; invoked by make gen.

Legacy world sections are deliberately left intact. The active world uses
exploration.bin/chart.bin and a versioned little-endian object table.
"""
from __future__ import annotations

import json
import math
import struct
import re
from collections import deque
from dataclasses import dataclass, replace
from html import escape
from pathlib import Path

import gen_world as old
import gen_scenery
import gen_contours
import gen_settlements

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'levels/exploration.json'
TOOLS = {'cutter': 1, 'seal': 2, 'thruster': 4, 'lens': 8}
KINDS = {'cutter': 1, 'seal': 2, 'thruster': 3, 'lens': 6}
# Campaign milestone bits are a flash/save ABI even though they live in a
# generated header, so the names are frozen here.
MILESTONES = {'first': 1, 'pump': 2, 'lift': 4, 'beacon': 8, 'home': 16, 'expedition': 32}
SURVEY_IDS = {'pump': 0, 'lift': 1}

# A new voyage enters the water beside the home gantry, not on its dock pixel:
# 28 px clears the 14 px auto-dock ring with visible separation, and the left
# side matches the dock's clear wall approach.
SPAWN_DOCK_OFFSET_PX = 28

# Authored route phases are frozen by endpoint. Removing a retired route from
# the level must not re-phase every later surviving passage and reshape the
# whole cave; new routes fall back to their list position.
ROUTE_PHASE = {
    'T_L': 0, 'L_F': 1, 'F_E': 2, 'T_M': 3, 'M_Q': 4, 'Q_E': 5, 'L_Q': 6,
    'M_W': 7, 'K_B': 9, 'B_P': 10, 'P_S': 11, 'S_N': 12, 'N_V': 13,
    'K_V': 14, 'B_N': 15, 'N_BR': 16, 'R_C': 18, 'C_Y': 19, 'Y_A': 20,
    'A_U': 21, 'U_Z': 22, 'R_Z': 23, 'C_U': 24, 'Z_CW': 25, 'Y_CC': 26,
    'H_J': 27, 'J_O': 28, 'O_D': 29, 'D_G': 30, 'G_I': 31, 'I_J': 32,
    'J_G': 33, 'O_DL': 34, 'F_K': 36, 'E_V': 37, 'S_Y': 38, 'N_C': 39,
    'U_H': 40, 'Q_J': 41,
}

def route_phase(route, index):
    return ROUTE_PHASE.get(route['from'] + '_' + route['to'], index)

@dataclass(frozen=True)
class Gate:
    name: str
    x: int
    y: int
    w: int
    h: int
    required: int

@dataclass(frozen=True)
class Pickup:
    name: str
    x: int
    y: int
    kind: int

@dataclass(frozen=True)
class ServiceBerth:
    id: int
    name: str
    symbol: str
    tile_x: int
    tile_y: int
    pixel_x: int
    pixel_y: int
    milestone: int


@dataclass(frozen=True)
class Survey:
    id: int
    name: str
    symbol: str
    tile_x: int
    tile_y: int
    pixel_x: int
    pixel_y: int


@dataclass
class Level:
    source: dict
    world: old.World
    gates: list[Gate]
    pickups: list[Pickup]
    service_berths: list[ServiceBerth]
    surveys: list[Survey]
    current_gate_mask: int = 0


def shape_cavern(grid, cx, cy, rx, ry, phase=0.0, solid=False):
    """Lobed ellipse in tile space; roughness changes the silhouette, not art."""
    if rx<=0 or ry<=0:
        raise ValueError("cavern/rock radii must be positive")
    reach = 1.23
    for y in range(max(2, math.floor(cy-ry*reach)), min(158, math.ceil(cy+ry*reach)+1)):
        for x in range(max(2, math.floor(cx-rx*reach)), min(318, math.ceil(cx+rx*reach)+1)):
            nx, ny = (x-cx)/rx, (y-cy)/ry
            angle = math.atan2(ny,nx)
            boundary = 1 + .10*math.sin(angle*3+phase) + .055*math.sin(angle*7-phase*.7)
            if nx*nx+ny*ny <= boundary*boundary:
                grid[y*320+x] = 1 if solid else 0


def fill_rock_polygon(grid, points):
    for y in range(max(2,math.floor(min(p[1] for p in points))),min(158,math.ceil(max(p[1] for p in points))+1)):
        for x in range(max(2,math.floor(min(p[0] for p in points))),min(318,math.ceil(max(p[0] for p in points))+1)):
            inside=False
            for i,(ax,ay) in enumerate(points):
                bx,by=points[(i+1)%len(points)]
                if (ay>y)!=(by>y) and x < (bx-ax)*(y-ay)/(by-ay)+ax:
                    inside=not inside
            if inside:grid[y*320+x]=1


def organic_spine(route, a, b, seed):
    """Smooth authored waypoints; no runtime curves or floating point required."""
    if 'via' in route:
        points=[a]+route['via']+[b]
    else:
        dx,dy=b[0]-a[0],b[1]-a[1]
        length=math.hypot(dx,dy)
        bend=route.get('bend',min(6.0,max(2.5,length*.12)))
        phase=seed*.071
        points=[a]
        for t in (.25,.5,.75):
            offset=math.sin(t*math.pi*2+phase)*bend*math.sin(t*math.pi)
            points.append([a[0]+dx*t-dy/length*offset,a[1]+dy*t+dx/length*offset])
        points.append(b)
    result=[]
    extended=[points[0]]+points+[points[-1]]
    for i in range(1,len(extended)-2):
        p0,p1,p2,p3=extended[i-1:i+3]
        steps=max(8,math.ceil(math.hypot(p2[0]-p1[0],p2[1]-p1[1])*3))
        for j in range(steps):
            t=j/steps
            position=[]
            for axis in (0,1):
                value=.5*((2*p1[axis])+(-p0[axis]+p2[axis])*t+(2*p0[axis]-5*p1[axis]+4*p2[axis]-p3[axis])*t*t+(-p0[axis]+3*p1[axis]-3*p2[axis]+p3[axis])*t*t*t)
                position.append(value)
            result.append(position)
    result.append(list(b))
    return result


def on_spine(points, t):
    lengths=[math.hypot(b[0]-a[0],b[1]-a[1]) for a,b in zip(points,points[1:])]
    target=t*sum(lengths)
    for a,b,length in zip(points,points[1:],lengths):
        if target<=length:
            f=target/length if length else 0
            return [a[0]+(b[0]-a[0])*f,a[1]+(b[1]-a[1])*f]
        target-=length
    return list(points[-1])


def make_gate(route, spine, pressure_y):
    required=sum(TOOLS[t] for t in route.get('gate_required',route.get('requires',[])))
    if not required and 'gate_required' not in route:return None
    if 'gate' in route:
        gx,gy=route['gate']['position']
        horizontal=route['gate']['axis']=='y'
    elif required&TOOLS['seal']:
        crossings=[(a,b) for a,b in zip(spine,spine[1:]) if a[1]<pressure_y<=b[1] or b[1]<pressure_y<=a[1]]
        if len(crossings)!=1:raise ValueError('pressure passage must cross depth boundary once')
        a,b=crossings[0];f=(pressure_y-a[1])/(b[1]-a[1])
        gx,gy=a[0]+(b[0]-a[0])*f,pressure_y
        horizontal=abs(b[1]-a[1])>=abs(b[0]-a[0])
    else:
        gx,gy=on_spine(spine,route.get('gate_at',.5))
        a=on_spine(spine,max(0,route.get('gate_at',.5)-.02));b=on_spine(spine,min(1,route.get('gate_at',.5)+.02))
        horizontal=abs(b[1]-a[1])>=abs(b[0]-a[0])
    w,h=(96,8) if horizontal else (8,96)
    return Gate(route['from']+'_'+route['to'],round(gx*8-w/2),round(gy*8-h/2),w,h,required)


def carve_organic_route(grid, route, spine, gate, phase):
    n=max(1,len(spine)-1)
    gate_center=[(gate.x+gate.w/2)/8,(gate.y+gate.h/2)/8] if gate else None
    for i,(x,y) in enumerate(spine):
        t=i/n
        radius=route['radius']*(1+.25*math.sin(t*math.pi*4+phase)+.12*math.sin(t*math.pi*11-phase))
        radius=max(2.45,radius)
        if gate_center:
            distance=math.hypot(x-gate_center[0],y-gate_center[1])
            if distance<9:radius=2.5+(radius-2.5)*max(0,(distance-5)/4)
        old._carve_disk(grid,x,y,radius)
    for chamber in route.get('caverns',[]):
        x,y=on_spine(spine,chamber['at'])
        shape_cavern(grid,x,y,*chamber['radii'],phase)


def carve_organic(source, grid, rooms):
    seed=source['terrain']['seed']
    for i,r in enumerate(source['rooms']):
        x,y=r['position'];phase=(seed+i*17)*.13
        size=r.get('size',[r['radius'],r['radius']])
        shape_cavern(grid,x,y,*size,phase)
        for dx,dy,rx,ry in r.get('lobes',[]):shape_cavern(grid,x+dx,y+dy,rx,ry,phase+.8)
        old._carve_disk(grid,x,y,6 if 'station' in r else 3)
    gates=[];spines=[]
    for i,route in enumerate(source['routes']):
        required=route.get('requires',[])
        if len(required)!=len(set(required)) or any(t not in TOOLS or t=='lens' for t in required):raise ValueError('invalid passage tool requirements')
        if route['from'] not in rooms or route['to'] not in rooms:raise ValueError('passage references unknown room')
        phase=route_phase(route,i)
        spine=organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],seed+phase*43)
        gate=make_gate(route,spine,source['pressure_y'])
        carve_organic_route(grid,route,spine,gate,(seed+phase*43)*.11)
        if gate:gates.append(gate)
        spines.append(spine)
    for obstacle in source.get('obstacles',[]):
        origin=rooms[obstacle['room']]['position']
        if 'polygon' in obstacle:
            fill_rock_polygon(grid,[[origin[0]+x,origin[1]+y] for x,y in obstacle['polygon']])
        else:
            dx,dy=obstacle['offset'];shape_cavern(grid,origin[0]+dx,origin[1]+dy,*obstacle['radii'],solid=True)
    for route,spine in zip(source['routes'],spines):
        for rock in route.get('obstacles',[]):
            x,y=on_spine(spine,rock['at'])
            dx,dy=rock.get('offset',[0,0])
            shape_cavern(grid,x+dx,y+dy,*rock['radii'],solid=True)
    # Erase only tiny decorative water slivers trapped by a wall tooth. Larger
    # disconnected caverns remain errors in validate_world; never erase rooms.
    visited=bytearray(320*160)
    anchors={tuple(r['position']) for r in source['rooms']}
    for start,value in enumerate(grid):
        if value or visited[start]:continue
        queue=deque([start]);visited[start]=1;component=[]
        while queue:
            i=queue.popleft();component.append(i)
            for j in (i-1,i+1,i-320,i+320):
                if 0<=j<len(grid) and not grid[j] and not visited[j]:
                    visited[j]=1;queue.append(j)
        if len(component)<=3 and not any((i%320,i//320) in anchors for i in component):
            for i in component:grid[i]=1
    return gates


def open_run(is_open, low, high, centre):
    """Contiguous open interval containing (or nearest to) centre."""
    runs = []
    start = None
    for value in range(low, high + 1):
        if is_open(value):
            if start is None:
                start = value
        elif start is not None:
            runs.append((start, value - 1))
            start = None
    if start is not None:
        runs.append((start, high))
    if not runs:
        raise ValueError('no open cross-section')
    return min(runs, key=lambda run: 0 if run[0] <= centre <= run[1] else min(abs(centre - run[0]), abs(centre - run[1])))


def cover_current(grid, rooms, current):
    """Span the common cross-section of the flow strip, then overlap rock.

    Every slice of the authored flow strip must agree on the open run before
    the current covers it. Using the intersection (rather than the union) keeps
    the tide out of side coves and dead-end pockets that are open at only one
    slice; those pockets were the source of inescapable current traps. The
    final one-tile overlap into rock keeps the wall-to-wall read in the map.
    """
    cx = current.x + current.w // 2
    cy = current.y + current.h // 2
    horizontal = current.ax_q8 != 0
    if horizontal:
        low = 0
        high = 159
        columns = range(max(0, current.x // 8), min(320, (current.x + current.w - 1) // 8 + 1))
        for tx in columns:
            if not 0 <= tx < 320:
                continue
            run = open_run(lambda ty: not grid[ty * 320 + tx],
                           0, 159,
                           cy // 8)
            low = max(low, run[0])
            high = min(high, run[1])
        if high < low:
            raise ValueError('horizontal current has no common navigable cross-section')
        low = max(0, low - 1)
        high = min(159, high + 1)
        return old.Current(current.x, low * 8, current.w, (high - low + 1) * 8,
                           current.ax_q8, current.ay_q8)
    low = 0
    high = 319
    rows = range(max(0, current.y // 8), min(160, (current.y + current.h - 1) // 8 + 1))
    for ty in rows:
        if not 0 <= ty < 160:
            continue
        run = open_run(lambda tx: not grid[ty * 320 + tx],
                       0, 319,
                       cx // 8)
        low = max(low, run[0])
        high = min(high, run[1])
    if high < low:
        raise ValueError('vertical current has no common navigable cross-section')
    low = max(0, low - 1)
    high = min(319, high + 1)
    return old.Current(low * 8, current.y, (high - low + 1) * 8, current.h,
                       current.ax_q8, current.ay_q8)


def build(source: dict) -> Level:
    if source.get('format') != 'sub-hauler-level' or source.get('version') != 2:
        raise ValueError('unsupported authored level format/version')
    if source['dimensions'] != [320, 160] or source['tile_pixels'] != 8:
        raise ValueError('level must use the 320x160 native tile contract')
    if not isinstance(source['pressure_y'], int) or not 0 < source['pressure_y'] < 160:
        raise ValueError('invalid pressure depth')
    rows = source['rooms']
    rooms = {r['id']: r for r in rows}
    if len(rooms) != len(rows):
        raise ValueError('duplicate room id')
    if len(source['routes']) != len({frozenset((r['from'], r['to'])) for r in source['routes']}):
        raise ValueError('duplicate passage')
    grid = bytearray([1]) * (320 * 160)
    for r in rows:
        x,y=r['position'];radius=r['radius']
        if not (radius+2 <= x < 320-radius-2 and radius+2 <= y < 160-radius-2):raise ValueError('room crosses sealed border')
        if 'pickup' in r:
            item=r['pickup']
            if item.get('kind') not in KINDS:raise ValueError('invalid pickup kind')
    if source.get('terrain',{}).get('style')!='organic':
        raise ValueError('unsupported terrain style')
    if type(source.get('layout_version')) is not int or not 1<=source['layout_version']<=255:
        raise ValueError('invalid terrain layout version')
    gates=carve_organic(source,grid,rooms)
    pickups = [Pickup(r['id'],r['position'][0]*8,r['position'][1]*8,
                      KINDS[r['pickup']['kind']]) for r in rows if 'pickup' in r]
    if len(gates)>16 or len(pickups)>8:
        raise ValueError('object counts exceed compact progress masks')
    settlements = []
    for r in sorted((r for r in rows if 'station' in r),key=lambda r:r['station']):
        x,y = r['position']
        settlements.append(old.Settlement(r['station'],r['name'].upper(),x,y,x*8,y*8))
    # Four room-centre current zones, kept clear of berths. Their
    # perpendicular extent is widened after the cave is carved to cover the
    # common wall-to-wall passage cross-section, so a boat cannot skirt the
    # flow by hugging either wall and side coves are not swallowed. F, V, A
    # and O were retired: their walls added no routing decision.
    current_rooms = ['K','S','R','I']
    current_dirs = [(0,2048),(0,2048),(2048,0),(0,-2048)]
    currents=[]
    for key,(ax,ay) in zip(current_rooms,current_dirs):
        x,y=rooms[key]['position'];x*=8;y*=8
        currents.append(old.Current(x-16,y-8,32,16,ax,ay))
    spawn=rooms[source['spawn']]['position']
    world=old.World(0,grid,old.pack_chart(old.build_chart(grid)),
                    tuple(old.Chamber(*r['position'],r['radius'],r['name'],'station' in r) for r in rows),
                    tuple(settlements),tuple(currents),*spawn,spawn[0]*8,spawn[1]*8,
                    spawn[0]*8-13,spawn[1]*8+4)
    old.validate_world(world)
    # Authored current zones add tide gates beyond the eight room-centre
    # hazards. They are appended after the legacy compact validation, then all
    # zones share the same common-cross-section expansion unless the spec pins
    # an exact gate rectangle.
    fixed_currents = []
    for spec in source.get('currents', []):
        direction = spec.get('dir')
        if direction not in ('x+', 'x-', 'y+', 'y-'):
            raise ValueError('current needs a signed axis direction')
        if any(type(spec.get(key)) is not int for key in ('x', 'y', 'w', 'h')):
            raise ValueError('current needs integer x/y/w/h')
        if spec['w'] <= 0 or spec['h'] <= 0:
            raise ValueError('current needs a positive extent')
        ax, ay = {
            'x+': (2048, 0),
            'x-': (-2048, 0),
            'y+': (0, 2048),
            'y-': (0, -2048),
        }[direction]
        current = old.Current(spec['x'], spec['y'], spec['w'], spec['h'], ax, ay)
        if spec.get('fixed'):
            fixed_currents.append(current)
        else:
            currents.append(current)
    # Widen each zone across the carved passage only after the legacy compact
    # validation, so the legacy seed-4242 world header stays byte-identical.
    expanded=[]
    for current in currents:
        expanded.append(cover_current(grid, rooms, current))
    expanded.extend(fixed_currents)
    world=replace(world,currents=tuple(expanded))
    # Every tide is a thruster gate: upstream travel is blocked until the
    # thruster is owned, and the zone is disabled once it is.
    current_gate_mask = (1 << len(expanded)) - 1
    # Legacy validation checks the open navigation chambers. Wall berths have
    # a directional approach instead of the old six-tile empty circle.
    docks=[]
    for r in sorted((r for r in rows if 'station' in r),key=lambda r:r['station']):
        dx,dy=r.get('dock',r['position'])
        if any(type(v) is not int for v in (dx,dy)) or not (2<=dx<318 and 2<=dy<158):
            raise ValueError('invalid wall dock coordinates')
        docks.append(old.Settlement(r['station'],r['name'].upper(),dx,dy,dx*8,dy*8))
    home=next(d for d in docks if d.id==rooms[source['spawn']]['station'])
    world=replace(world,settlements=tuple(docks),spawn_tile_x=home.tile_x,spawn_tile_y=home.tile_y,
                  spawn_pixel_x=home.pixel_x,spawn_pixel_y=home.pixel_y,
                  spawn_sub_x=home.pixel_x-SPAWN_DOCK_OFFSET_PX,spawn_sub_y=home.pixel_y)

    service_berths=[]
    seen_berths={d.id for d in docks}
    for item in source.get('service_berths',[]):
        if item.get('room') not in rooms:
            raise ValueError('service berth references unknown room')
        if item.get('milestone') not in MILESTONES:
            raise ValueError('service berth milestone is not a frozen campaign bit')
        berth_id=item.get('id')
        if type(berth_id) is not int or berth_id < len(docks) or berth_id >= 255:
            raise ValueError('service berth id must be a stable id after the freight settlements')
        if berth_id in seen_berths:
            raise ValueError('duplicate berth id')
        seen_berths.add(berth_id)
        dx,dy=item.get('dock',[None,None])
        if any(type(v) is not int for v in (dx,dy)) or not (2<=dx<318 and 2<=dy<158):
            raise ValueError('invalid service berth dock coordinates')
        symbol=item.get('symbol')
        if not isinstance(symbol,str) or not re.fullmatch(r'[A-Z][A-Z0-9_]*',symbol):
            raise ValueError('service berth needs a stable C++ symbol')
        service_berths.append(ServiceBerth(berth_id,item.get('name','SERVICE').upper(),symbol,
                                           dx,dy,dx*8,dy*8,MILESTONES[item['milestone']]))

    surveys=[]
    seen_surveys=set()
    for item in source.get('surveys',[]):
        if item.get('room') not in rooms:
            raise ValueError('survey references unknown room')
        survey_id=item.get('id')
        if survey_id not in SURVEY_IDS.values():
            raise ValueError('survey id is not a frozen campaign bit')
        if survey_id in seen_surveys:
            raise ValueError('duplicate survey id')
        seen_surveys.add(survey_id)
        x,y=item.get('position',[None,None])
        if any(type(v) is not int for v in (x,y)) or not (2<=x<318 and 2<=y<158):
            raise ValueError('invalid survey marker coordinates')
        symbol=item.get('symbol')
        if not isinstance(symbol,str) or not re.fullmatch(r'[A-Z][A-Z0-9_]*',symbol):
            raise ValueError('survey needs a stable C++ symbol')
        surveys.append(Survey(survey_id,item.get('name',rooms[item['room']]['name']).upper(),symbol,
                              x,y,x*8,y*8))

    level=Level(source,world,gates,pickups,service_berths,surveys,current_gate_mask)
    validate_docks(level)
    validate_spawn(level)
    validate_surveys(level)
    return level


def _validate_one_dock(level, dock):
    grid=level.world.grid
    # Docked hull and straight water approach must both be safe. The pier is
    # scenery; the boat's 26x8 collision footprint stays in open water.
    for rise in range(0,49,4):
        px,py=dock.pixel_x,dock.pixel_y-rise
        if any(grid[y*320+x] for y in range((py-4)//8,(py+3)//8+1)
               for x in range((px-13)//8,(px+12)//8+1)):
            raise ValueError('wall dock blocks hull or approach: '+dock.name)
    floor=next((y for y in range(dock.tile_y+1,min(160,dock.tile_y+5)) if grid[y*320+dock.tile_x]),None)
    if floor is None:raise ValueError('dock is not attached to a wall: '+dock.name)
    for current in level.world.currents:
        px=max(current.x,min(dock.pixel_x,current.x+current.w))
        py=max(current.y,min(dock.pixel_y,current.y+current.h))
        if math.hypot(px-dock.pixel_x,py-dock.pixel_y)<old.DOCK_CLEARANCE_PX:
            raise ValueError('current inside wall dock approach')


def validate_docks(level):
    for dock in level.world.settlements:
        _validate_one_dock(level,dock)
    seen=set()
    for berth in level.service_berths:
        dock=old.Settlement(berth.id,berth.name,berth.tile_x,berth.tile_y,berth.pixel_x,berth.pixel_y)
        _validate_one_dock(level,dock)
        # A service berth is a distinct docking point, never a copy of a
        # freight settlement's berth.
        if (dock.pixel_x,dock.pixel_y) in seen:
            raise ValueError('service berth duplicates another dock: '+dock.name)
        seen.add((dock.pixel_x,dock.pixel_y))
        for settlement in level.world.settlements:
            if math.hypot(settlement.pixel_x-dock.pixel_x,settlement.pixel_y-dock.pixel_y)<old.DOCK_CLEARANCE_PX:
                raise ValueError('service berth overlaps freight dock: '+dock.name)


def validate_spawn(level):
    grid=level.world.grid
    px,py=level.world.spawn_sub_x,level.world.spawn_sub_y
    # The hull centre starts beside the wall gantry. Keep the same 26x8
    # navigability and current-free guarantees the legacy spawn had.
    if any(grid[ty*320+tx] for ty in range((py-4)//8,(py+3)//8+1)
           for tx in range((px-13)//8,(px+12)//8+1)):
        raise ValueError('spawn hull is not navigable')
    for current in level.world.currents:
        if px+13>current.x and px-13<current.x+current.w and py+4>current.y and py-4<current.y+current.h:
            raise ValueError('spawn hull overlaps a current')


def validate_surveys(level):
    grid=level.world.grid
    for survey in level.surveys:
        px,py=survey.pixel_x,survey.pixel_y
        # The boat's 26x8 hull must fit at the marker itself.
        if any(grid[ty*320+tx] for ty in range((py-4)//8,(py+3)//8+1)
               for tx in range((px-13)//8,(px+12)//8+1)):
            raise ValueError('survey marker is not navigable: '+survey.name)
        for settlement in level.world.settlements:
            if math.hypot(settlement.pixel_x-px,settlement.pixel_y-py)<old.DOCK_CLEARANCE_PX:
                raise ValueError('survey marker sits in a freight dock ring: '+survey.name)
        for gate in level.gates:
            if px+13>gate.x and px-13<gate.x+gate.w and py+8>gate.y and py-8<gate.y+gate.h:
                raise ValueError('survey marker overlaps a gate: '+survey.name)
        for pickup in level.pickups:
            if math.hypot(pickup.x-px,pickup.y-py)<2*old.DOCK_CLEARANCE_PX:
                raise ValueError('survey marker overlaps a pickup: '+survey.name)


def object_bytes(level: Level) -> bytes:
    data = b'SHL2'+bytes((len(level.gates),len(level.pickups),7,5))
    for g in level.gates:
        data+=struct.pack('<HHBBB',g.x,g.y,g.w,g.h,g.required)
    for p in level.pickups:
        data+=struct.pack('<HHB',p.x,p.y,p.kind)
    return data


def reachable_tiles(level: Level, tools: int) -> set[tuple[int,int]]:
    """Hull-center reachability at 8px spacing, with real 26x8 clearance/gates."""
    grid=level.world.grid
    free=bytearray(320*160)
    closed=[g for g in level.gates if tools&g.required!=g.required]
    # A tide gate has no blocking gate record: it is an authored current whose
    # passage only opens with its required tool. Treat its authored rectangle
    # as closed for the same reachability contract.
    tide_rects=[]
    for spec in level.source.get('currents', []):
        needed=sum(TOOLS[t] for t in spec.get('requires', []))
        if tools&needed!=needed:
            tide_rects.append(spec)
    for y in range(1,159):
        for x in range(2,318):
            if any(grid[yy*320+xx] for yy in (y-1,y) for xx in range(x-2,x+2)):
                continue
            px,py=x*8,y*8
            if any(px+13>g.x and px-13<g.x+g.w and py+4>g.y and py-4<g.y+g.h for g in closed):
                continue
            if any(px+13>t['x'] and px-13<t['x']+t['w'] and py+4>t['y'] and py-4<t['y']+t['h'] for t in tide_rects):
                continue
            free[y*320+x]=1
    start=tuple(next(r for r in level.source['rooms'] if r['id']==level.source['spawn'])['position'])
    seen={start};queue=deque([start])
    while queue:
        x,y=queue.popleft()
        for xx,yy in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
            if 0<=xx<320 and 0<=yy<160 and free[yy*320+xx] and (xx,yy) not in seen:
                seen.add((xx,yy));queue.append((xx,yy))
    return seen


def validate_access(level: Level) -> None:
    rooms={r['id']:r for r in level.source['rooms']}
    for tools in range(8):
        expected={level.source['spawn']}
        while True:
            count=len(expected)
            for route in level.source['routes']:
                needed=sum(TOOLS[t] for t in route.get('requires',[]))
                if tools&needed==needed:
                    a,b=route['from'],route['to']
                    if a in expected:expected.add(b)
                    if b in expected:expected.add(a)
            if len(expected)==count:break
        reached=reachable_tiles(level,tools)
        actual={key for key,r in rooms.items() if tuple(r['position']) in reached}
        if actual!=expected:
            raise ValueError(f'hull/gate reachability tools={tools}: missing {expected-actual}, leaked {actual-expected}')
        for key in expected:
            if 'station' in rooms[key] and tuple(rooms[key].get('dock',rooms[key]['position'])) not in reached:
                raise ValueError(f'unreachable wall berth {key} with tools={tools}')
    required={'W':0,'CW':1,'BR':1,'D':3}
    for key,tools in required.items():
        if tuple(rooms[key]['position']) not in reachable_tiles(level,tools):
            raise ValueError('unobtainable required equipment or destination: '+key)
    # Service berths and survey rings must be reachable with the base boat;
    # neither may put a mandatory tool behind its own restoration branch.
    for berth in level.service_berths:
        if berth.milestone == MILESTONES['lift'] and berth.tile_y >= level.source['pressure_y']:
            raise ValueError('Lower Gantry service berth must stay above the pressure boundary: '+berth.name)
    base_reach=reachable_tiles(level,0)
    full_tools=sum(TOOLS.values())
    for berth in level.service_berths:
        # The optional Deep Relay expedition is deliberately late: it is only
        # offered after every tool is held. Restoration berths stay base-boat
        # reachable so no mandatory tool sits behind its own branch.
        reach=reachable_tiles(level,full_tools) if berth.milestone==MILESTONES['expedition'] else base_reach
        if (berth.tile_x,berth.tile_y) not in reach:
            raise ValueError('unreachable service berth without optional tools: '+berth.name)
    for survey in level.surveys:
        if (survey.tile_x,survey.tile_y) not in base_reach:
            raise ValueError('unreachable survey without optional tools: '+survey.name)
    if rooms['D']['position'][1] != max(r['position'][1] for r in level.source['rooms'] if 'station' in r):
        raise ValueError('Deep Anchor must be the deepest station')


def obstacle_centers(source):
    rooms={r['id']:r for r in source['rooms']}
    centers=[]
    for obstacle in source.get('obstacles',[]):
        ox,oy=rooms[obstacle['room']]['position']
        if 'polygon' in obstacle:
            points=obstacle['polygon'];dx=sum(p[0] for p in points)/len(points);dy=sum(p[1] for p in points)/len(points)
        else:dx,dy=obstacle['offset']
        centers.append((obstacle['id'],round((ox+dx)*8),round((oy+dy)*8)))
    for i,route in enumerate(source['routes']):
        if not route.get('obstacles'):continue
        phase=route_phase(route,i)
        spine=organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],source['terrain']['seed']+phase*43)
        for obstacle in route['obstacles']:
            x,y=on_spine(spine,obstacle['at']);dx,dy=obstacle.get('offset',[0,0])
            centers.append((obstacle['id'],round((x+dx)*8),round((y+dy)*8)))
    return centers


def render_objects(level: Level) -> str:
    blob=object_bytes(level)
    constants='\n'.join(f'constexpr uint8_t GATE_{g.name} = {i};' for i,g in enumerate(level.gates))
    constants+='\n'+'\n'.join(f'constexpr uint8_t PICKUP_{p.name} = {i};' for i,p in enumerate(level.pickups))
    constants+='\n'+'\n'.join(f'constexpr int16_t ROOM_{r["id"]}_{axis} = {r["position"][i]*8};' for r in level.source['rooms'] for i,axis in enumerate(('X','Y')))
    constants+='\n'+'\n'.join(f'constexpr int16_t DOCK_{r["id"]}_{axis} = {r.get("dock",r["position"])[i]*8};' for r in level.source['rooms'] if 'station' in r for i,axis in enumerate(('X','Y')))
    constants+='\n'+f'constexpr uint8_t OBSTACLE_COUNT = {len(obstacle_centers(level.source))};'
    constants+='\n'+'\n'.join(f'constexpr int16_t ROCK_{symbol}_X = {x}, ROCK_{symbol}_Y = {y};' for name,x,y in obstacle_centers(level.source) for symbol in [re.sub(r'[^A-Za-z0-9_]', '_', name).upper()])
    numbers=', '.join(str(b) for b in blob)
    return f'''// Generated by tools/gen_level.py through make gen. Do not edit.
#pragma once
#include <stdint.h>
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define EXPLORATION_FLASH PROGMEM
#else
#define EXPLORATION_FLASH
#endif
namespace exploration_data {{
constexpr uint8_t CUTTER = 1, SEAL = 2, THRUSTER = 4, LENS = 8;
constexpr uint8_t GATE_COUNT = {len(level.gates)}, PICKUP_COUNT = {len(level.pickups)};
constexpr uint16_t OBJECT_BYTES = {len(blob)};
constexpr uint8_t GATE_STRIDE = 7, PICKUP_STRIDE = 5, HEADER_BYTES = 8;
constexpr int16_t PRESSURE_Y = {level.source['pressure_y']*8};
{constants}
// Sole runtime copy of the exploration object stream. The shipping cart copy
// (explorationObjects) was retired by sub-hauler-jxk.1; this PROGMEM mirror
// keeps collision reads free of cart transactions.
// No object array or map copy is allocated in SRAM.
const uint8_t DATA[OBJECT_BYTES] EXPLORATION_FLASH = {{{numbers}}};
inline uint8_t byteAt(uint16_t offset) {{
#if defined(__AVR__)
    return pgm_read_byte(DATA + offset);
#else
    return DATA[offset];
#endif
}}
inline int16_t wordAt(uint16_t offset) {{
    return static_cast<int16_t>(byteAt(offset) | (static_cast<uint16_t>(byteAt(offset+1)) << 8));
}}
struct Gate {{ int16_t x,y; uint8_t w,h,required; }};
struct Pickup {{ int16_t x,y; uint8_t kind; }};
inline Gate gate(uint8_t id) {{
    const uint16_t p=HEADER_BYTES+static_cast<uint16_t>(id)*GATE_STRIDE;
    Gate g={{wordAt(p),wordAt(p+2),byteAt(p+4),byteAt(p+5),byteAt(p+6)}};
    return g;
}}
inline Pickup pickup(uint8_t id) {{
    const uint16_t p=HEADER_BYTES+GATE_COUNT*GATE_STRIDE+static_cast<uint16_t>(id)*PICKUP_STRIDE;
    Pickup item={{wordAt(p),wordAt(p+2),byteAt(p+4)}};
    return item;
}}
}} // namespace exploration_data
#undef EXPLORATION_FLASH
'''


def render_preview(level: Level) -> str:
    pickup_labels={1:'CUTTER',2:'SEAL',3:'THRUSTER',6:'LENS'}
    delivery_labels={'PUMP_REFUGE':'PUMP DELIVERY','FREIGHT_YARD':'LIFT DELIVERY'}
    parts=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1280 680"><title>Encoded Sub-hauler exploration terrain</title><rect width="1280" height="680" fill="#263a47"/><g transform="scale(4)">']
    for y in range(160):
        x=0
        while x<320:
            if level.world.grid[y*320+x]:x+=1;continue
            start=x
            while x<320 and not level.world.grid[y*320+x]:x+=1
            parts.append(f'<path d="M{start} {y+.5}h{x-start}" stroke="#081c2b"/>')
    for c in level.world.currents:
        parts.append(f'<rect x="{c.x/8}" y="{c.y/8}" width="{c.w/8}" height="{c.h/8}" fill="#6cc7e8" fill-opacity=".22" stroke="#6cc7e8" stroke-opacity=".45" stroke-width=".4" vector-effect="non-scaling-stroke"/>')
    for g in level.gates:
        if g.required == 0:
            continue
        color='#ba9be8' if g.required&2 else '#6cd5df' if g.required&4 else '#edb96a'
        parts.append(f'<rect x="{g.x/8}" y="{g.y/8}" width="{g.w/8}" height="{g.h/8}" fill="{color}"/>')
    for p in level.pickups:
        parts.append(f'<circle cx="{p.x/8}" cy="{p.y/8}" r="1.5" fill="#edb96a"/>')
    for s in level.surveys:
        parts.append(f'<circle cx="{s.pixel_x/8}" cy="{s.pixel_y/8}" r="1.8" fill="none" stroke="#e5efed" stroke-width=".7"/>')
    for b in level.service_berths:
        parts.append(f'<rect x="{b.pixel_x/8-1.5}" y="{b.pixel_y/8-1.5}" width="3" height="3" fill="#f4f0c6" stroke="#081c2b" stroke-width=".6"/>')
    parts.append('</g><g font-family="sans-serif" font-size="13" fill="#e5efed">')
    for s in level.world.settlements:
        parts.append(f'<text x="{s.tile_x*4-20}" y="{s.tile_y*4-20}">{escape(s.name)}</text>')
    for b in level.service_berths:
        parts.append(f'<text x="{b.tile_x*4-20}" y="{b.tile_y*4+18}">{escape(b.name)}</text>')
        delivery=delivery_labels.get(b.symbol)
        if delivery:
            parts.append(f'<text x="{b.tile_x*4-20}" y="{b.tile_y*4+31}" font-size="11" fill="#f4d28a">{delivery}</text>')
    for p in level.pickups:
        label=pickup_labels.get(p.kind,'EQUIPMENT')
        parts.append(f'<text x="{p.x/2+8}" y="{p.y/2+4}" font-size="11" fill="#f4d28a">{label}</text>')
    for s in level.surveys:
        parts.append(f'<text x="{s.pixel_x/2+8}" y="{s.pixel_y/2+4}" font-size="11" fill="#cfe7e2">{s.symbol} SURVEY</text>')
    parts.append('<text x="20" y="648" font-size="12" fill="#9bb5bd">Cyan = expanded current · amber = cutter gate · violet = seal gate · cyan line = thruster gate · circles = equipment coves · rings = surveys · squares = service berths</text>')
    parts.append(f'<text x="20" y="670">Actual 320x160 packed tile geometry · gates and {len(level.pickups)} equipment coves · generated by make gen</text></g></svg>')
    return '\n'.join(parts)+'\n'


def terrain_columns(grid, x, y):
    """Native eight-column rock tile: same edge trim/chamfers as cave.hpp."""
    if not grid[y*320+x]:
        return bytes(8)
    def water(tx, ty):
        return 0 <= tx < 320 and 0 <= ty < 160 and not grid[ty*320+tx]
    north, south = water(x, y-1), water(x, y+1)
    west, east = water(x-1, y), water(x+1, y)
    columns = bytearray(8)
    for col in range(8):
        for row in range(8):
            trimmed = ((north and row == 0) or (south and row == 7) or
                       (west and col == 0) or (east and col == 7) or
                       (north and west and col == 1 and row == 1) or
                       (north and east and col == 6 and row == 1) or
                       (south and west and col == 1 and row == 6) or
                       (south and east and col == 6 and row == 6))
            if not trimmed:
                columns[col] |= 1 << row
    if not (north or south or west or east):
        columns[(x*5+y*3)&7] &= ~(1 << ((x*3+y*7)&7))
    return bytes(columns)


def base_terrain_bytes(level: Level) -> bytes:
    # 2560 columns x 160 eight-pixel pages. A tile-aligned viewport is eight
    # contiguous 128-byte reads, written straight into the existing screen.
    image = gen_contours.solid_image(level.world.grid)
    pixels = image.load()
    grid = level.world.grid
    # Keep the quiet interior texture; the boundary comes from the native
    # contour, independent of the legacy 8x8 edge atlas.
    for y in range(160):
        for x in range(320):
            if grid[y*320+x] and all(not (0 <= nx < 320 and 0 <= ny < 160) or grid[ny*320+nx]
                                       for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))):
                pixels[x*8+((x*5+y*3)&7), y*8+((x*3+y*7)&7)] = 0
    return gen_contours.pack_native(image)


def scenery_spines(level: Level):
    rooms = {r['id']:r for r in level.source['rooms']}
    return [organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],level.source['terrain']['seed']+route_phase(route,i)*43)
            for i,route in enumerate(level.source['routes'])]


def terrain_bytes(level: Level) -> bytes:
    terrain=gen_scenery.decorate(level, base_terrain_bytes(level), scenery_spines(level))[0]
    return gen_settlements.bake(level,terrain)[0]


def main() -> None:
    level=build(json.loads(SOURCE.read_text()))
    validate_access(level)
    root=ROOT/'fxdata/tables'
    (root/'exploration.bin').write_bytes(old.pack_tiles(level.world.grid))
    (root/'exploration-chart.bin').write_bytes(level.world.chart)
    (root/'exploration-objects.bin').write_bytes(object_bytes(level))
    terrain,scenery=gen_scenery.decorate(level,base_terrain_bytes(level),scenery_spines(level))
    terrain,scenery['settlements']=gen_settlements.bake(level,terrain)
    (root/'exploration-terrain.bin').write_bytes(terrain)
    (ROOT/'docs/assets/exploration-scenery.json').write_text(json.dumps(scenery,indent=2)+'\n')
    print(f'gen_scenery: {json.dumps(scenery, separators=(",", ":"))}')
    header=old.render_header(level.world,service_berths=level.service_berths,surveys=level.surveys,variable_current_extents=True).replace('Generated by tools/gen_world.py.','Generated by tools/gen_level.py.').replace('// Archival legacy world tables (M1); not packed.','// Active authored exploration world.').replace('LAYOUT_VERSION    = 1',f'LAYOUT_VERSION    = {level.source.get("layout_version",2)}').replace('CURRENT_ACCEL_Q8 = 4096', 'CURRENT_ACCEL_Q8 = 2048').replace('world.bin','exploration.bin').replace('world-chart.bin','exploration-chart.bin')
    header=header.replace('constexpr uint8_t  CURRENT_COUNT',
                          f'constexpr uint16_t CURRENT_GATE_MASK = 0x{level.current_gate_mask:04X};\nconstexpr uint8_t  CURRENT_COUNT',1)
    (ROOT/'src/generated/level.hpp').write_text(header)
    (ROOT/'src/generated/exploration.hpp').write_text(render_objects(level))
    (ROOT/'docs/assets/exploration-terrain.svg').write_text(render_preview(level))
    rocks=len(level.source.get('obstacles',[]))+sum(len(r.get('obstacles',[])) for r in level.source['routes'])
    print(f'gen_level: rooms={len(level.source["rooms"])} passages={len(level.source["routes"])} physical_obstacles={rocks} layout={level.source["layout_version"]} gates={len(level.gates)} pickups={len(level.pickups)} service_berths={len(level.service_berths)} surveys={len(level.surveys)} tiles=6400B chart=256B objects={len(object_bytes(level))}B terrain={len(terrain)}B hull/tool validation=pass')

if __name__=='__main__':
    main()
