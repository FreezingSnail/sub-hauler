"""Wall piers and inhabited rock cutaways, baked through make gen only."""
from PIL import Image, ImageDraw
import gen_contours


def bake(level, terrain):
    solid=gen_contours.solid_image(level.world.grid)
    pixels=solid.load()
    white,black=set(),set()
    report=[]
    rooms={r['station']:r for r in level.source['rooms'] if 'station' in r}
    for dock in level.world.settlements:
        x,y=dock.pixel_x,dock.pixel_y
        floor=next((py for py in range(y+5,min(y+37,1280)) if pixels[x,py]),None)
        if floor is None:raise ValueError('wall berth has no foundation: '+dock.name)
        art=Image.new('1',(160,88))
        d=ImageDraw.Draw(art)
        ox,oy=80,8
        # White pier posts connect the live docking ring to the rock. They
        # begin below the hull so the open approach remains visually clear.
        for dx in (-10,10):
            d.line((ox+dx,oy+5,ox+dx,oy+floor-y+3),fill=255,width=2)
        d.line((ox-14,oy+floor-y,ox+14,oy+floor-y),fill=255,width=2)
        for py in range(art.height):
            for px in range(art.width):
                if art.getpixel((px,py)):
                    white.add((x+px-ox,y+py-oy))

        # Inhabited rooms fit entirely inside rock. Cutaways are deliberately
        # narrower than the hull and connected by tiny pressure corridors.
        modules=[]
        candidates=[(x+dx,floor+dy) for dy in (4,12,20,28) for dx in (-42,-18,6,30,-66,54)]
        for mx,my in candidates:
            if not (8<=mx<2530 and 8<=my<1250):continue
            if any(abs(mx-a)<24 and abs(my-b)<19 for a,b in modules):continue
            if not all(pixels[px,py] for py in range(my,my+16) for px in range(mx,mx+22)):continue
            modules.append((mx,my))
            if len(modules)==4:break
        if len(modules)<2:raise ValueError('settlement needs rock for inhabited rooms: '+dock.name)
        theme=rooms[dock.id]['region']
        for i,(mx,my) in enumerate(modules):
            chamber=Image.new('1',(22,16))
            c=ImageDraw.Draw(chamber)
            if theme=='kelp':c.rounded_rectangle((0,0,21,15),radius=6,fill=255)
            elif theme=='deep':c.polygon(((4,0),(17,0),(21,4),(21,11),(17,15),(4,15),(0,11),(0,4)),fill=255)
            else:c.rounded_rectangle((0,0,21,15),radius=3 if theme=='tide' else 1,fill=255)
            # Black room with a white pressure shell, lit windows, floor and
            # visible equipment/occupants. White is wall, black is cutaway.
            inner=chamber.copy();c=ImageDraw.Draw(inner)
            if theme=='deep':c.polygon(((5,2),(16,2),(19,5),(19,10),(16,13),(5,13),(2,10),(2,5)),fill=0)
            else:c.rounded_rectangle((2,2,19,13),radius=5 if theme=='kelp' else 3 if theme=='tide' else 1,fill=0)
            for wx in (5,10,15):c.rectangle((wx,4,wx+1,6),fill=255)
            c.line((3,12,18,12),fill=255)
            if theme=='kelp':
                c.line((5,11,5,8),fill=255);c.line((4,8,6,8),fill=255)
                c.line((15,11,15,8),fill=255);c.line((14,8,16,8),fill=255)
            elif theme=='works':
                c.rectangle((4,8,8,11),outline=255);c.rectangle((13,8,17,11),outline=255)
            else:
                c.point((7,9),fill=255);c.line((7,10,7,11),fill=255)
                c.line((12,9,17,9),fill=255)
            for py in range(16):
                for px in range(22):
                    if chamber.getpixel((px,py)):
                        (white if inner.getpixel((px,py)) else black).add((mx+px,my+py))
        # Narrow service/airlock shaft, with a sealed round hatch at the wall.
        for py in range(y+6,floor+12):
            for dx in range(-3,4):
                (white if abs(dx)>=2 else black).add((x+dx,py))
        for py in range(floor+9,floor+12):
            for px in range(min(x,min(m[0] for m in modules)),max(x,max(m[0]+22 for m in modules))):
                if pixels[px,py]:black.add((px,py))
        for mx,my in modules:
            for py in range(floor+10,my+9):
                for px in range(mx+10,mx+13):
                    if pixels[px,py]:black.add((px,py))
        report.append({'id':dock.id,'name':dock.name,'x':x,'y':y,'floor_y':floor,
                       'modules':[{'x':mx,'y':my,'w':22,'h':16} for mx,my in modules]})
    result=bytearray(terrain)
    # Cutaways never change collision, and no ink reaches the docked hull.
    for points,on in ((black,False),(white,True)):
        for x,y in points:
            if not(0<=x<2560 and 0<=y<1280):raise ValueError('settlement art outside image')
            index,bit=(y//8)*2560+x,1<<(y&7)
            if on:result[index]|=bit
            else:result[index]&=~bit
    return bytes(result),report
