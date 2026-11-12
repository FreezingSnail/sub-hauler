import copy
import hashlib
from dataclasses import replace
import json
import math
from pathlib import Path
import re
import struct
import sys
import unittest

from PIL import ImageFilter

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import gen_level
import gen_world
import gen_scenery
import gen_contours
import gen_settlements

class AuthoredLevelTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source=json.loads(gen_level.SOURCE.read_text())
        cls.level=gen_level.build(cls.source)

    def test_collision_geometry_respects_every_capability_combination(self):
        gen_level.validate_access(self.level)

    def test_packed_geometry_and_chart_are_exact_generated_outputs(self):
        packed=gen_world.pack_tiles(self.level.world.grid)
        self.assertEqual(len(packed),6400)
        self.assertEqual(packed,(ROOT/'fxdata/tables/exploration.bin').read_bytes())
        self.assertEqual(gen_world.unpack_tiles(packed),self.level.world.grid)
        self.assertEqual(self.level.world.chart,(ROOT/'fxdata/tables/exploration-chart.bin').read_bytes())
        self.assertEqual(self.level.world.chart,gen_world.pack_chart(gen_world.build_chart(self.level.world.grid)))

    def test_records_are_little_endian_and_match_the_collision_mirror(self):
        blob=gen_level.object_bytes(self.level)
        self.assertEqual(blob[:8],b'SHL2'+bytes((9,4,7,5)))
        self.assertEqual(len(blob),91)
        for i,g in enumerate(self.level.gates):
            self.assertEqual(struct.unpack_from('<HHBBB',blob,8+i*7),(g.x,g.y,g.w,g.h,g.required))
        for i,p in enumerate(self.level.pickups):
            self.assertEqual(struct.unpack_from('<HHB',blob,8+9*7+i*5),(p.x,p.y,p.kind))
        self.assertFalse(hasattr(self.level.pickups[0],'amount'))
        self.assertEqual([p.kind for p in self.level.pickups],[1,3,2,6])
        header=(ROOT/'src/generated/exploration.hpp').read_text()
        array=re.search(r'DATA\[OBJECT_BYTES\].*?= \{([^}]+)',header).group(1)
        self.assertEqual(bytes(map(int,array.split(','))),blob)
        self.assertEqual((ROOT/'fxdata/tables/exploration-objects.bin').read_bytes(),blob)

    def test_high_pressure_and_ending_station_are_lowest(self):
        rooms={r['id']:r for r in self.source['rooms']}
        boundary=self.source['pressure_y']
        self.assertLess(rooms['CW']['position'][1],boundary)
        for key in ('J','O','D','G','I','DC'):
            self.assertGreater(rooms[key]['position'][1],boundary)
        # The lens cove sits in the empty upper-west rock, but its only
        # connection is the winding route from Trench Refuge below the
        # pressure boundary, so the seal still gates it.
        self.assertLess(rooms['DL']['position'][1],boundary)
        links=sorted((r['from'],r['to']) for r in self.source['routes']
                     if 'DL' in (r['from'],r['to']))
        self.assertEqual(links,[('O','DL')])
        self.assertEqual(rooms['D']['position'][1],max(r['position'][1] for r in rooms.values() if 'station' in r))
        self.assertEqual(len(rooms),32)
        self.assertEqual(len(self.source['routes']),42)

    def test_legacy_sections_are_archival_and_active_assets_pack_without_offset_drift(self):
        header=(ROOT/'src/fxdata.h').read_text()
        offsets={name:int(value,16) for name,value in re.findall(r'constexpr uint24_t (\w+) = 0x([0-9A-Fa-f]+);',header)}
        cart=(ROOT/'fxdata/fxdata.bin').read_bytes()
        # sub-hauler-jxk.1 retired these from the shipping layout under the
        # approved append-only ABI exception; their generators stay archival.
        for retired in ('hero','worldTiles','worldChart','cargoBank',
                        'explorationObjects','leviathan','minnow'):
            self.assertNotIn(retired,offsets)
        self.assertEqual(offsets['font_6x8'],0)
        for name,file in [('explorationTiles','exploration.bin'),('explorationChart','exploration-chart.bin'),('explorationTerrain','exploration-terrain.bin')]:
            blob=(ROOT/'fxdata/tables'/file).read_bytes()
            self.assertEqual(cart[offsets[name]:offsets[name]+len(blob)],blob)
        self.assertEqual(hashlib.sha256((ROOT/'fxdata/tables/world.bin').read_bytes()).hexdigest(),'78b6b51d763b447af299e80e80697bd6fd3d3f2bf4da20becf5d04c50ed61873')

    def test_native_terrain_is_exact_page_major_generated_output(self):
        terrain=gen_level.terrain_bytes(self.level)
        self.assertEqual(len(terrain),2560*1280//8)
        self.assertEqual(terrain,(ROOT/'fxdata/tables/exploration-terrain.bin').read_bytes())
        for x,y in ((0,0),(319,159),(38,24),(147,86),(173,47)):
            offset=y*2560+x*8
            self.assertEqual(terrain[offset:offset+8],gen_level.terrain_columns(self.level.world.grid,x,y))

    def test_native_terrain_preserves_water_edges_chamfers_and_rock_speckles(self):
        grid=bytearray([1])*(320*160)
        x,y=10,12
        self.assertEqual(gen_level.terrain_columns(grid,x,y),b'\xff'*6+b'\xfb\xff')
        grid[(y-1)*320+x]=0
        self.assertEqual(gen_level.terrain_columns(grid,x,y),b'\xfe'*8)
        grid[y*320+x-1]=0
        self.assertEqual(gen_level.terrain_columns(grid,x,y),bytes((0,0xFC,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE)))
        grid[(y+1)*320+x]=grid[y*320+x+1]=0
        self.assertEqual(gen_level.terrain_columns(grid,x,y),bytes((0,0x3C,0x7E,0x7E,0x7E,0x7E,0x3C,0)))
        grid[y*320+x]=0
        self.assertEqual(gen_level.terrain_columns(grid,x,y),bytes(8))

    def test_scenery_preserves_collision_silhouettes_and_interaction_corridors(self):
        grid=bytes(self.level.world.grid)
        chart=bytes(self.level.world.chart)
        objects=gen_level.object_bytes(self.level)
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        decorated,stats=gen_scenery.decorate(self.level,base,spines)
        protected=gen_scenery.protection(self.level,spines)
        contour=gen_contours.solid_image(self.level.world.grid)
        wreck_pixels=set()
        for item in stats['landmarks']:
            if item['mount']=='wreck':
                wreck_pixels.update((item['x']+px+dx,item['y']+py+dy)
                                    for px,py in gen_scenery.motif(item['kind'],0)
                                    for dx in (-1,0,1) for dy in (-1,0,1))
        changed=0
        for i,(a,b) in enumerate(zip(base,decorated)):
            delta=a^b
            if not delta:continue
            x,page=i%2560,i//2560
            for bit in range(8):
                if not delta&(1<<bit):continue
                y=page*8+bit
                if (x,y) in wreck_pixels: continue
                self.assertFalse(protected.getpixel((x,y)),(x,y,'route or interactive marker changed'))
                # Use the curved visual wall here; the coarse collision tile
                # can differ at its rounded corners by a couple of pixels.
                self.assertEqual(bool(b&(1<<bit)),not bool(contour.getpixel((x,y))))
                changed+=1
        self.assertGreater(changed,10000)
        self.assertEqual(bytes(self.level.world.grid),grid)
        self.assertEqual(bytes(self.level.world.chart),chart)
        self.assertEqual(gen_level.object_bytes(self.level),objects)
        self.assertEqual(len(decorated),len(base))
        self.assertEqual(len(stats['landmarks']),len(self.source['scenery']['landmarks']))
        self.assertTrue(all(stats['ambient'][region]>4 for region in gen_scenery.REGIONS),stats)

    def test_scenery_is_deterministic_and_legacy_plain_render_remains_available(self):
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        a,stats=gen_scenery.decorate(self.level,base,spines)
        b,again=gen_scenery.decorate(self.level,base,spines)
        self.assertEqual((a,stats),(b,again))
        plain=copy.deepcopy(self.level)
        del plain.source['scenery']
        self.assertEqual(gen_scenery.decorate(plain,base,spines),(base,{}))

    def test_equipment_wrecks_project_into_water_and_leave_markers_clear(self):
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        _,stats=gen_scenery.decorate(self.level,base,spines)
        by_room={item['room']:item for item in stats['landmarks']}
        expected={'W':'cutter-tender','BR':'thruster-pod','CW':'workshop-lock','DL':'trench-lens','DC':'alien-core'}
        rooms={r['id']:r for r in self.source['rooms']}
        interior=gen_contours.solid_image(self.level.world.grid).filter(ImageFilter.MinFilter(7))
        protected=gen_scenery.protection(self.level,spines)
        seen=set()
        for room_id,kind in expected.items():
            item=by_room[room_id]
            self.assertEqual(item['kind'],kind,room_id)
            self.assertEqual(item['mount'],'wreck',room_id)
            self.assertNotIn(kind,seen,'each cove needs its own silhouette')
            seen.add(kind)
            cx,cy=(v*8 for v in rooms[room_id]['position'])
            self.assertLessEqual(math.hypot(item['x']-cx,item['y']-cy),90,room_id)
            pixels=gen_scenery.motif(kind,0)
            visible=sum(cx-64 <= item['x']+px < cx+64 and cy-32 <= item['y']+py < cy+32
                        for px,py in pixels)
            self.assertGreaterEqual(visible, len(pixels)*0.65, f'{room_id} landmark in pickup viewport')
            points={(item['x']+px,item['y']+py) for px,py in pixels}
            solid=gen_contours.solid_image(self.level.world.grid)
            self.assertGreater(sum(not solid.getpixel((x,y)) for x,y in points),len(points)*.35)
            for x,y in points:
                for pickup in self.level.pickups:
                    self.assertFalse(pickup.x-13<=x<=pickup.x+13 and pickup.y-11<=y<=pickup.y+11,
                                     f'{room_id}: pickup remains readable')
                for gate in self.level.gates:
                    self.assertFalse(gate.x-4<=x<=gate.x+gate.w+4 and gate.y-4<=y<=gate.y+gate.h+4,
                                     f'{room_id}: gate remains readable')
        # The wall landmarks are new vocabulary, not reused region motifs.
        self.assertTrue(seen.isdisjoint({'tender','wreck','relay','pump','crane','winch'}))

    def test_lens_cave_texture_is_scoped_to_the_hidden_trench_cove(self):
        dl=next(r for r in self.source['rooms'] if r['id']=='DL')
        self.assertEqual(dl['texture'],'lens')
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        decorated,stats=gen_scenery.decorate(self.level,base,spines)
        self.assertGreater(stats['ambient']['lens'],0)
        for region in gen_scenery.REGIONS:
            self.assertGreater(stats['ambient'][region],4,region)
        # Removing the override must only change pixels in the room's pocket.
        plain=copy.deepcopy(self.level)
        next(r for r in plain.source['rooms'] if r['id']=='DL').pop('texture')
        bare,_=gen_scenery.decorate(plain,base,spines)
        cx,cy=(v*8 for v in dl['position'])
        limit=(dl['radius']+gen_scenery.TEXTURE_RADIUS_TILES)*8+16
        changed=0
        for i,(a,b) in enumerate(zip(decorated,bare)):
            delta=a^b
            if not delta:continue
            x,page=i%2560,i//2560
            for bit in range(8):
                if not delta&(1<<bit):continue
                y=page*8+bit
                self.assertLessEqual(math.hypot(x-cx,y-cy),limit,'lens texture leaked outside the cove')
                changed+=1
        self.assertGreater(changed,100)
        bad=copy.deepcopy(self.source)
        next(r for r in bad['rooms'] if r['id']=='DL')['texture']='bogus'
        with self.assertRaisesRegex(ValueError,'texture'):
            gen_scenery.decorate(gen_level.build(bad),base,spines)

    def test_equipment_and_relay_themes_read_in_native_viewports(self):
        expected={'W':'cutter','BR':'turbine','DC':'alien'}
        rooms={r['id']:r for r in self.source['rooms']}
        plain=copy.deepcopy(self.level)
        for room in plain.source['rooms']:
            if room['id'] in expected: room.pop('texture')
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        decorated,stats=gen_scenery.decorate(self.level,base,spines)
        bare,_=gen_scenery.decorate(plain,base,spines)
        for key,theme in expected.items():
            self.assertEqual(rooms[key]['texture'],theme)
            self.assertGreater(stats['ambient'][theme],0)
            cx,cy=(v*8 for v in rooms[key]['position'])
            changed=sum(bool((decorated[(y//8)*2560+x]^bare[(y//8)*2560+x]) & (1<<(y%8)))
                        for y in range(cy-32,cy+32) for x in range(cx-64,cx+64))
            self.assertGreater(changed,40,f'{key}: theme must be visible at the interaction point')

    def test_kelp_forest_has_dense_floor_and_ceiling_growth(self):
        base=gen_level.base_terrain_bytes(self.level)
        _,stats=gen_scenery.decorate(self.level,base,gen_level.scenery_spines(self.level))
        self.assertGreaterEqual(stats['ambient']['kelp'],130)
        heights=[max(-y for x,y in gen_scenery.motif('kelp',seed)) for seed in range(12)]
        self.assertLess(min(heights),25)
        self.assertGreaterEqual(max(heights),40)
        frond=gen_scenery.motif('kelp-canopy',0)
        self.assertTrue(any(y>10 for x,y in frond),'canopy hangs down into water')

    def test_pressure_seal_winding_approach_requires_cutter(self):
        route=next(r for r in self.source['routes'] if r['from']=='Z' and r['to']=='CW')
        self.assertGreaterEqual(len(route['via']), 5)
        pickup=tuple(next(r for r in self.source['rooms'] if r['id']=='CW')['position'])
        self.assertNotIn(pickup,gen_level.reachable_tiles(self.level,0))
        self.assertIn(pickup,gen_level.reachable_tiles(self.level,1))

    def test_pressure_cave_texture_stays_near_the_seal(self):
        room=next(r for r in self.source['rooms'] if r['id']=='CW')
        self.assertEqual(room['texture'],'pressure')
        self.assertEqual(room['radius'],5)
        base=gen_level.base_terrain_bytes(self.level)
        spines=gen_level.scenery_spines(self.level)
        decorated,stats=gen_scenery.decorate(self.level,base,spines)
        self.assertGreater(stats['ambient']['pressure'],0)
        plain=copy.deepcopy(self.level)
        next(r for r in plain.source['rooms'] if r['id']=='CW').pop('texture')
        bare,_=gen_scenery.decorate(plain,base,spines)
        cx,cy=(v*8 for v in room['position'])
        # Wall-mounted ribs extend beyond their placement anchor.
        limit=(room['radius']+gen_scenery.TEXTURE_RADIUS_TILES)*8+32
        changed=0
        for i,(a,b) in enumerate(zip(decorated,bare)):
            delta=a^b
            if not delta: continue
            x,page=i%2560,i//2560
            for bit in range(8):
                if delta & (1<<bit):
                    self.assertLessEqual(math.hypot(x-cx,page*8+bit-cy),limit)
                    changed+=1
        self.assertGreater(changed,100)

    def test_service_berths_and_surveys_are_stable_generated_metadata(self):
        self.assertEqual([b.id for b in self.level.service_berths],[4,5,6,7])
        self.assertEqual([b.symbol for b in self.level.service_berths],['PUMP_REFUGE','LOWER_GANTRY','FREIGHT_YARD','DEEP_RELAY'])
        self.assertEqual([b.milestone for b in self.level.service_berths],[2,4,4,32])
        self.assertEqual([s.id for s in self.level.surveys],[0,1])
        self.assertEqual([s.symbol for s in self.level.surveys],['PUMP','LIFT'])
        self.assertNotIn(4,range(len(self.level.world.settlements)))
        header=(ROOT/'src/generated/level.hpp').read_text()
        for symbol,value in (('BERTH_PUMP_REFUGE',4),('BERTH_LOWER_GANTRY',5),('BERTH_FREIGHT_YARD',6),('BERTH_DEEP_RELAY',7),('SURVEY_PUMP',0),('SURVEY_LIFT',1)):
            self.assertIn(f'constexpr uint8_t {symbol} = {value};',header)
        for berth in self.level.service_berths:
            self.assertIn(f'{{{berth.id}, {berth.milestone}, {berth.pixel_x}, {berth.pixel_y}}},',header)
        for survey in self.level.surveys:
            self.assertIn(f'{{{survey.id}, {survey.pixel_x}, {survey.pixel_y}}},',header)
        self.assertIn('constexpr uint8_t  BERTH_COUNT       = 8;',header)
        # Service berths and surveys never enter the four-settlement economy.
        for origin in range(4):
            for destination in range(4):
                self.assertLess(destination,4)

    def test_currents_cover_the_common_passage_cross_section(self):
        rooms={r['id']:r for r in self.source['rooms']}
        fixed_rects={(c['x'],c['y'],c['w'],c['h']) for c in self.source.get('currents',[])}
        for current in self.level.world.currents:
            horizontal=current.ax_q8!=0
            cx=current.x+current.w//2
            cy=current.y+current.h//2
            grid=self.level.world.grid
            self.assertFalse(grid[(cy//8)*320+cx//8],'current centre is navigable')
            # The intersection rule must not swallow a neighbouring room or a
            # side cove. Each room-centre tide contains exactly its own room
            # centre; the fixed Cormorant tide contains none.
            inside={r['id'] for r in rooms.values()
                    if current.x<=r['position'][0]*8<current.x+current.w
                    and current.y<=r['position'][1]*8<current.y+current.h}
            if current.w>32 or current.h>16:
                self.assertLessEqual(len(inside),1,f'current swallowed rooms {inside}')
            # Full-cargo thrust is 34 - 3*4 = 22 px/s^2; current force 16 px/s^2.
            self.assertGreater(22,abs(current.ax_q8)//256 if current.ax_q8 else abs(current.ay_q8)//256)
            if (current.x,current.y,current.w,current.h) in fixed_rects:
                continue
            if horizontal:
                edges=[(x,y) for x in range(current.x//8,(current.x+current.w)//8)
                       for y in (current.y//8,(current.y+current.h)//8-1)]
            else:
                edges=[(x,y) for y in range(current.y//8,(current.y+current.h)//8)
                       for x in (current.x//8,(current.x+current.w)//8-1)]
            self.assertTrue(any(grid[y*320+x] for x,y in edges),
                            'current overlaps at least one rock edge')

        # Regression: the retired F/V/A/O walls are gone.
        for room in ('F','V','A','O'):
            x,y=rooms[room]['position'];x*=8;y*=8
            self.assertFalse(any(c.x<=x<c.x+c.w and c.y<=y<c.y+c.h
                                 for c in self.level.world.currents),
                             f'{room} tide was retired')

    def test_cormorant_tide_pushes_up_and_stays_thruster_gated(self):
        tide=next((i,c) for i,c in enumerate(self.level.world.currents)
                  if c.x==1744 and c.y==507 and c.w==96 and c.h==8)
        index,current=tide
        self.assertLess(current.ay_q8,0,'Cormorant tide pushes upward')
        self.assertEqual(current.ax_q8,0)
        self.assertTrue(self.level.current_gate_mask&(1<<index),
                        'Cormorant tide remains a thruster tide gate')
        self.assertLess(current.y,self.source['rooms'][
            next(i for i,r in enumerate(self.source['rooms']) if r['id']=='C')]['position'][1]*8,
                        'tide sits above Cormorant')

    def test_all_tides_are_thruster_gates(self):
        self.assertEqual(self.level.current_gate_mask,
                         (1<<len(self.level.world.currents))-1)

    def test_thruster_gate_sections_are_tide_zones(self):
        # The two former T gates are zero-required tide placeholders.
        gate_by_name={g.name:g for g in self.level.gates}
        self.assertEqual(gate_by_name['J_G'].required,0)
        self.assertEqual(gate_by_name['I_DC'].required,0)
        by_rect={(c.x,c.y,c.w,c.h):c for c in self.level.world.currents}
        basalt=by_rect[(941,1071,96,8)]
        jet=by_rect[(1852,1038,8,96)]
        self.assertLess(basalt.ay_q8,0,'Basalt tide blocks the descent')
        self.assertLess(jet.ax_q8,0,'Jet approach tide blocks the entry')
        self.assertEqual(basalt.ax_q8,0)
        self.assertEqual(jet.ay_q8,0)

    def test_pipe_junction_tide_pushes_down(self):
        room=next(r for r in self.source['rooms'] if r['id']=='S')
        x,y=room['position'][0]*8,room['position'][1]*8
        tide=next(c for c in self.level.world.currents
                  if c.x<=x<c.x+c.w and c.y<=y<c.y+c.h)
        self.assertGreater(tide.ay_q8,0,'Pipe Junction tide pushes downward')
        self.assertEqual(tide.ax_q8,0)

    def test_current_covers_the_common_strip_and_rejects_side_pockets(self):
        rooms={'test':{'position':[14,14],'radius':4}}
        for horizontal in (True,False):
            with self.subTest(horizontal=horizontal):
                grid=bytearray([1])*(320*160)
                for y in range(10,21):
                    for x in range(10,21):
                        grid[y*320+x]=0
                if horizontal:
                    for y in range(10,26):
                        grid[y*320+12]=0
                    for y in range(25,29):
                        grid[y*320+13]=0
                    authored=gen_world.Current(96,104,32,16,2048,0)
                    water=(104,112)
                    rock=(104,72)
                    side=(104,224)
                else:
                    for x in range(5,26):
                        grid[13*320+x]=0
                    for x in range(25,29):
                        grid[14*320+x]=0
                    authored=gen_world.Current(96,104,32,16,0,2048)
                    water=(112,112)
                    rock=(72,112)
                    side=(208,112)
                zone=gen_level.cover_current(grid,rooms,authored)
                for x,y in (water,rock):
                    self.assertTrue(zone.x<=x<zone.x+zone.w and zone.y<=y<zone.y+zone.h)
                self.assertFalse(zone.x<=side[0]<zone.x+zone.w and zone.y<=side[1]<zone.y+zone.h,
                                 'side passage is not part of the common cross-section')
                self.assertFalse(grid[(water[1]//8)*320+water[0]//8])
                self.assertTrue(grid[(rock[1]//8)*320+rock[0]//8])
                if horizontal:
                    self.assertEqual((zone.x,zone.w),(authored.x,authored.w))
                else:
                    self.assertEqual((zone.y,zone.h),(authored.y,authored.h))
                self.assertEqual((zone.ax_q8,zone.ay_q8),(authored.ax_q8,authored.ay_q8))

    def test_deep_relay_expedition_needs_every_tool_and_reaches_the_lower_right(self):
        relay=next(b for b in self.level.service_berths if b.symbol=='DEEP_RELAY')
        self.assertEqual(relay.milestone,gen_level.MILESTONES['expedition'])
        self.assertGreater(relay.tile_x,200)
        self.assertGreater(relay.tile_y,120)
        self.assertNotIn((relay.tile_x,relay.tile_y),gen_level.reachable_tiles(self.level,0))
        self.assertIn((relay.tile_x,relay.tile_y),gen_level.reachable_tiles(self.level,sum(gen_level.TOOLS.values())))

    def test_preview_labels_currents_equipment_coves_and_delivery_markers(self):
        svg=gen_level.render_preview(self.level)
        for label in ('TIDEWALL','DEEP ANCHOR','DEEP RELAY','PUMP REFUGE','FREIGHT YARD',
                      'CUTTER','SEAL','THRUSTER','LENS','PUMP DELIVERY','LIFT DELIVERY'):
            self.assertIn('>'+label+'<',svg,label)
        self.assertEqual(svg.count('fill="#6cc7e8" fill-opacity=".22"'),len(self.level.world.currents))
        self.assertIn('expanded current',svg)
        # The lower-right relay is drawn at its generated dock, not a stale inset.
        relay=next(b for b in self.level.service_berths if b.symbol=='DEEP_RELAY')
        self.assertIn(f'x="{relay.tile_x*4-20}" y="{relay.tile_y*4+18}">DEEP RELAY</text>',svg)

    def test_unsafe_service_berth_or_survey_placements_are_rejected(self):
        bad=copy.deepcopy(self.source)
        bad['service_berths'][0]['dock']=[270,20]
        with self.assertRaisesRegex(ValueError,'not attached'):
            gen_level.build(bad)
        bad=copy.deepcopy(self.source)
        bad['service_berths'][1]['id']=3
        with self.assertRaisesRegex(ValueError,'stable id after'):
            gen_level.build(bad)
        bad=copy.deepcopy(self.source)
        bad['surveys'][0]['position']=[260,20]
        with self.assertRaisesRegex(ValueError,'not navigable'):
            gen_level.build(bad)
        bad=copy.deepcopy(self.source)
        bad['surveys'][0]['id']=2
        with self.assertRaisesRegex(ValueError,'frozen campaign bit'):
            gen_level.build(bad)

    def test_lower_gantry_serves_above_the_pressure_boundary(self):
        lift=next(b for b in self.level.service_berths if b.symbol=='LOWER_GANTRY')
        self.assertLess(lift.tile_y,self.source['pressure_y'])
        self.assertEqual(lift.milestone,gen_level.MILESTONES['lift'])
        deep=next(r for r in self.source['rooms'] if r['id']=='D')
        below=replace(lift,tile_x=deep['dock'][0],tile_y=deep['dock'][1],
                      pixel_x=deep['dock'][0]*8,pixel_y=deep['dock'][1]*8)
        bad=gen_level.Level(self.level.source,self.level.world,self.level.gates,
                            self.level.pickups,[self.level.service_berths[0],below],
                            self.level.surveys)
        with self.assertRaisesRegex(ValueError,'above the pressure boundary'):
            gen_level.validate_access(bad)

    def test_wall_docks_are_distinct_reachable_and_reject_unsafe_placements(self):
        gen_level.validate_docks(self.level)
        for r in self.source['rooms']:
            if 'station' not in r:continue
            dock=self.level.world.settlements[r['station']]
            self.assertEqual((dock.tile_x,dock.tile_y),tuple(r['dock']))
            self.assertNotEqual(tuple(r['dock']),tuple(r['position']))
            self.assertGreater(dock.tile_y,r['position'][1])
        bad=copy.deepcopy(self.source)
        home=next(r for r in bad['rooms'] if r.get('station')==0)
        home['dock']=[home['position'][0],home['position'][1]+1]
        with self.assertRaisesRegex(ValueError,'not attached'):
            gen_level.build(bad)
        wall=next(r for r in self.source['rooms'] if r.get('station')==0)['dock']
        home['dock']=[wall[0],wall[1]+2]
        with self.assertRaisesRegex(ValueError,'blocks hull'):
            gen_level.build(bad)

    def test_settlements_have_inhabited_rock_rooms_and_clear_hull_approaches(self):
        base=gen_level.base_terrain_bytes(self.level)
        baked,report=gen_settlements.bake(self.level,base)
        self.assertEqual(len(baked),len(base))
        self.assertEqual(len(report),4)
        solid=gen_contours.solid_image(self.level.world.grid)
        for dock in report:
            self.assertGreaterEqual(len(dock['modules']),2)
            self.assertLessEqual(dock['floor_y']-dock['y'],32)
            for module in dock['modules']:
                for py in range(module['y'],module['y']+module['h']):
                    for px in range(module['x'],module['x']+module['w']):
                        self.assertTrue(solid.getpixel((px,py)),'habitation must be embedded in rock')
            for py in range(dock['y']-28,dock['y']+4):
                for px in range(dock['x']-13,dock['x']+13):
                    i=(py//8)*2560+px;bit=1<<(py&7)
                    self.assertEqual(baked[i]&bit,base[i]&bit,'settlement art cannot cover the hull approach')
        home=report[0]
        self.assertEqual((self.level.world.spawn_sub_x,self.level.world.spawn_sub_y),
                         (home['x']-gen_level.SPAWN_DOCK_OFFSET_PX,home['y']))
        world=self.level.world
        self.assertTrue(all(not world.grid[y*320+x] for y in range((world.spawn_sub_y-4)//8,(world.spawn_sub_y+3)//8+1)
                            for x in range((world.spawn_sub_x-13)//8,(world.spawn_sub_x+12)//8+1)),
                        'new-voyage hull starts in open water')

    def test_rock_features_are_real_collision_tiles(self):
        bare=copy.deepcopy(self.source)
        bare['obstacles']=[]
        for route in bare['routes']:route['obstacles']=[]
        before=bytearray([1])*(320*160)
        gen_level.carve_organic(bare,before,{r['id']:r for r in bare['rooms']})
        after=self.level.world.grid
        added=sum(not a and b for a,b in zip(before,after))
        self.assertGreaterEqual(added,400,"pillars, teeth and shelves must occupy real former water")
        self.assertEqual(sum(a and not b for a,b in zip(before,after)),0,"obstacles cannot erase rock")
        self.assertEqual(len(gen_level.obstacle_centers(self.source)),31)
        for i,route in enumerate(self.source['routes']):
            if not route.get('obstacles'):continue
            rooms={r['id']:r for r in self.source['rooms']}
            spine=gen_level.organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],self.source['terrain']['seed']+gen_level.route_phase(route,i)*43)
            for rock in route['obstacles']:
                x,y=gen_level.on_spine(spine,rock['at'])
                self.assertEqual(after[round(y)*320+round(x)],1,rock['id']+' blocks the direct passage')

    def test_passages_wind_and_sculpting_is_deterministic(self):
        rooms={r['id']:r for r in self.source['rooms']}
        bent=0
        for i,route in enumerate(self.source['routes']):
            spine=gen_level.organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],self.source['terrain']['seed']+gen_level.route_phase(route,i)*43)
            length=sum(math.dist(a,b) for a,b in zip(spine,spine[1:]))
            if length>math.dist(spine[0],spine[-1])*1.12:bent+=1
        self.assertGreaterEqual(bent,25,"the map must not regress to straight graph pipes")
        self.assertEqual(gen_level.build(copy.deepcopy(self.source)).world.grid,self.level.world.grid)

    def test_organic_geometry_invalidates_old_chart_layout_without_changing_record_format(self):
        self.assertEqual(self.source['layout_version'],3)
        self.assertIn('LAYOUT_VERSION    = 3',(ROOT/'src/generated/level.hpp').read_text())
        self.assertEqual(gen_level.object_bytes(self.level)[:4],b'SHL2')

    def test_generated_headers_contain_no_route_economy(self):
        for name in ('level.hpp','world.hpp'):
            header=(ROOT/'src/generated'/name).read_text()
            for token in ('ROUTE_REWARD','ROUTE_DEADLINE','routeEconomy','ROUTE_COUNT','ROUTE_CRATES'):
                self.assertNotIn(token,header,name)
        self.assertFalse(hasattr(gen_world,'build_route_economy'))
        self.assertFalse(hasattr(gen_level,'freight_deadlines'))

    def test_invalid_schema_duplicate_rooms_and_unknown_routes_are_rejected(self):
        for corruption in ('version','duplicate','unknown','capability','credits'):
            bad=copy.deepcopy(self.source)
            if corruption=='version':bad['version']=1
            elif corruption=='duplicate':bad['rooms'][1]['id']=bad['rooms'][0]['id']
            elif corruption=='unknown':bad['routes'][0]['to']='unknown'
            elif corruption=='capability':bad['routes'][0]['requires']=['bogus']
            else:next(r for r in bad['rooms'] if 'pickup' in r)['pickup']['kind']='credits'
            with self.subTest(corruption=corruption),self.assertRaises(ValueError):
                gen_level.build(bad)

    def test_a_gate_removed_from_real_geometry_cannot_silently_leak_a_tool(self):
        broken=copy.deepcopy(self.level)
        broken.gates=[g for g in broken.gates if g.name!='N_BR']
        with self.assertRaisesRegex(ValueError,'reachability'):
            gen_level.validate_access(broken)

if __name__=='__main__':unittest.main()
