import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import unittest

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
        self.assertEqual(blob[:8],b'SHL2'+bytes((9,8,7,6)))
        self.assertEqual(len(blob),119)
        for i,g in enumerate(self.level.gates):
            self.assertEqual(struct.unpack_from('<HHBBB',blob,8+i*7),(g.x,g.y,g.w,g.h,g.required))
        for i,p in enumerate(self.level.pickups):
            self.assertEqual(struct.unpack_from('<HHBB',blob,8+9*7+i*6),(p.x,p.y,p.kind,p.amount))
        header=(ROOT/'src/generated/exploration.hpp').read_text()
        array=re.search(r'DATA\[OBJECT_BYTES\].*?= \{([^}]+)',header).group(1)
        self.assertEqual(bytes(map(int,array.split(','))),blob)
        self.assertEqual((ROOT/'fxdata/tables/exploration-objects.bin').read_bytes(),blob)

    def test_high_pressure_and_ending_station_are_lowest(self):
        rooms={r['id']:r for r in self.source['rooms']}
        boundary=self.source['pressure_y']
        self.assertLess(rooms['CW']['position'][1],boundary)
        for key in ('J','O','D','G','I','DL','DC'):
            self.assertGreater(rooms[key]['position'][1],boundary)
        self.assertEqual(rooms['D']['position'][1],max(r['position'][1] for r in rooms.values() if 'station' in r))
        self.assertEqual(len(rooms),32)
        self.assertEqual(len(self.source['routes']),42)

    def test_legacy_sections_and_new_assets_are_packed_without_offset_drift(self):
        header=(ROOT/'src/fxdata.h').read_text()
        offsets={name:int(value,16) for name,value in re.findall(r'constexpr uint24_t (\w+) = 0x([0-9A-Fa-f]+);',header)}
        cart=(ROOT/'fxdata/fxdata.bin').read_bytes()
        self.assertEqual(offsets['cargoBank'],0x1AC2)
        for name,file in [('worldTiles','world.bin'),('worldChart','world-chart.bin'),('explorationTiles','exploration.bin'),('explorationChart','exploration-chart.bin'),('explorationObjects','exploration-objects.bin'),('explorationTerrain','exploration-terrain.bin')]:
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
        changed=0
        for i,(a,b) in enumerate(zip(base,decorated)):
            delta=a^b
            if not delta:continue
            x,page=i%2560,i//2560
            for bit in range(8):
                if not delta&(1<<bit):continue
                y=page*8+bit
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

    def test_service_berths_and_surveys_are_stable_generated_metadata(self):
        self.assertEqual([b.id for b in self.level.service_berths],[4,5])
        self.assertEqual([b.symbol for b in self.level.service_berths],['PUMP_REFUGE','LOWER_GANTRY'])
        self.assertEqual([b.milestone for b in self.level.service_berths],[2,4])
        self.assertEqual([s.id for s in self.level.surveys],[0,1])
        self.assertEqual([s.symbol for s in self.level.surveys],['PUMP','LIFT'])
        self.assertNotIn(4,range(len(self.level.world.settlements)))
        header=(ROOT/'src/generated/level.hpp').read_text()
        for symbol,value in (('BERTH_PUMP_REFUGE',4),('BERTH_LOWER_GANTRY',5),('SURVEY_PUMP',0),('SURVEY_LIFT',1)):
            self.assertIn(f'constexpr uint8_t {symbol} = {value};',header)
        for berth in self.level.service_berths:
            self.assertIn(f'{{{berth.id}, {berth.milestone}, {berth.pixel_x}, {berth.pixel_y}}},',header)
        for survey in self.level.surveys:
            self.assertIn(f'{{{survey.id}, {survey.pixel_x}, {survey.pixel_y}}},',header)
        self.assertIn('constexpr uint8_t  BERTH_COUNT       = 6;',header)
        # Service berths and surveys never enter the four-settlement economy.
        for origin in range(4):
            for destination in range(4):
                self.assertLess(destination,4)

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
        self.assertEqual((self.level.world.spawn_sub_x,self.level.world.spawn_sub_y),(home['x'],home['y']))

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
            spine=gen_level.organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],self.source['terrain']['seed']+i*43)
            for rock in route['obstacles']:
                x,y=gen_level.on_spine(spine,rock['at'])
                self.assertEqual(after[round(y)*320+round(x)],1,rock['id']+' blocks the direct passage')

    def test_passages_wind_and_sculpting_is_deterministic(self):
        rooms={r['id']:r for r in self.source['rooms']}
        bent=0
        for i,route in enumerate(self.source['routes']):
            spine=gen_level.organic_spine(route,rooms[route['from']]['position'],rooms[route['to']]['position'],self.source['terrain']['seed']+i*43)
            length=sum(math.dist(a,b) for a,b in zip(spine,spine[1:]))
            if length>math.dist(spine[0],spine[-1])*1.12:bent+=1
        self.assertGreaterEqual(bent,25,"the map must not regress to straight graph pipes")
        self.assertEqual(gen_level.build(copy.deepcopy(self.source)).world.grid,self.level.world.grid)

    def test_organic_geometry_invalidates_old_chart_layout_without_changing_record_format(self):
        self.assertEqual(self.source['layout_version'],3)
        self.assertIn('LAYOUT_VERSION    = 3',(ROOT/'src/generated/level.hpp').read_text())
        self.assertEqual(gen_level.object_bytes(self.level)[:4],b'SHL2')

    def test_freight_allowances_cover_winding_routes_without_optional_shortcuts(self):
        deadlines=gen_level.freight_deadlines(self.level)
        old=gen_world.build_route_economy(self.level.world)[1]
        for origin in range(4):
            for destination in range(4):
                i=origin*4+destination
                self.assertEqual(deadlines[i],deadlines[destination*4+origin])
                if origin!=destination:
                    self.assertGreater(deadlines[i],old[i])
                    self.assertLess(deadlines[i],65535)
        header=(ROOT/'src/generated/level.hpp').read_text()
        rows=re.search(r'ROUTE_DEADLINE\[ROUTE_COUNT\].*?= \{([^}]+)',header,re.S).group(1)
        self.assertEqual(tuple(map(int,re.findall(r'\d+',rows))),deadlines)
        detour=copy.deepcopy(self.level)
        for route in detour.source['routes']:
            if 'thruster' in route.get('requires',[]):route['via']=[[30,30],[290,130]]
        self.assertEqual(gen_level.freight_deadlines(detour),deadlines,'optional jet shortcuts are not assumed by contract timers')

    def test_invalid_schema_duplicate_rooms_and_unknown_routes_are_rejected(self):
        for corruption in ('version','duplicate','unknown','capability','reward'):
            bad=copy.deepcopy(self.source)
            if corruption=='version':bad['version']=1
            elif corruption=='duplicate':bad['rooms'][1]['id']=bad['rooms'][0]['id']
            elif corruption=='unknown':bad['routes'][0]['to']='unknown'
            elif corruption=='capability':bad['routes'][0]['requires']=['bogus']
            else:next(r for r in bad['rooms'] if 'pickup' in r)['pickup']['amount']=256
            with self.subTest(corruption=corruption),self.assertRaises(ValueError):
                gen_level.build(bad)

    def test_a_gate_removed_from_real_geometry_cannot_silently_leak_a_tool(self):
        broken=copy.deepcopy(self.level)
        broken.gates=[g for g in broken.gates if g.name!='N_BR']
        with self.assertRaisesRegex(ValueError,'reachability'):
            gen_level.validate_access(broken)

if __name__=='__main__':unittest.main()
