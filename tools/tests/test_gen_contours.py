"""Native contour geometry and packing contracts, independent of decoration."""
from pathlib import Path
import sys
import unittest

from PIL import Image, ImageChops, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import gen_contours
import gen_world


class NativeContourTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.grid = gen_world.unpack_tiles((ROOT/'fxdata/tables/exploration.bin').read_bytes())
        cls.contour = gen_contours.solid_image(cls.grid)

    def test_actual_tile_centres_keep_their_collision_class(self):
        for y in range(160):
            for x in range(320):
                self.assertEqual(bool(self.contour.getpixel((x*8+4,y*8+4))), bool(self.grid[y*320+x]), (x,y))

    def test_actual_contour_stays_within_three_pixels_of_collision(self):
        raw = Image.frombytes('L', (320,160), bytes(255*v for v in self.grid)).resize(self.contour.size, Image.Resampling.NEAREST).convert('1')
        # Check solid and water in both directions: rounding must not hide a
        # wall or visually close a route beyond this narrow boundary band.
        for a,b in ((raw,self.contour),(self.contour,raw),
                    (ImageChops.invert(raw),ImageChops.invert(self.contour)),
                    (ImageChops.invert(self.contour),ImageChops.invert(raw))):
            outside = ImageChops.logical_and(a,ImageChops.invert(b.filter(ImageFilter.MaxFilter(7))))
            self.assertIsNone(outside.getbbox())

    def test_diagonal_slope_does_not_keep_eight_pixel_square_steps(self):
        grid = bytearray(x >= y for y in range(160) for x in range(320))
        contour = gen_contours.solid_image(grid)
        edges = [next(x for x in range(y-16,y+17) if contour.getpixel((x,y))) for y in range(320,480)]
        longest = current = 1
        for a,b in zip(edges,edges[1:]):
            current = current+1 if a == b else 1
            longest = max(longest,current)
        self.assertLess(longest,5,'continuous diagonal contour, rather than 8-pixel tread/riser blocks')

    def test_native_pack_has_correct_page_stride_and_vertical_bit_order(self):
        image = Image.new('1',(2560,1280))
        for point in ((0,0),(127,7),(128,8),(2559,1279)):
            image.putpixel(point,255)
        packed = gen_contours.pack_native(image)
        self.assertEqual(len(packed),409600)
        self.assertEqual(packed[0],1)
        self.assertEqual(packed[127],128)
        self.assertEqual(packed[2560+128],1)
        self.assertEqual(packed[-1],128)
        self.assertEqual(sum(bool(value) for value in packed),4)


if __name__ == '__main__':
    unittest.main()
