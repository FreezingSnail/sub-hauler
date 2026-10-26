"""Native-resolution cave contours, baked offline through make gen.

The collision field is sampled at tile centres, then reconstructed as a
continuous bicubic field before thresholding to native 1-bit pixels. This
removes the eight-pixel stair steps without blurring the OLED image. Tile
centres and thin walls/channels retain their solid/water identity.
"""
from PIL import Image

WIDTH, HEIGHT = 2560, 1280


def solid_image(grid):
    tiles = Image.frombytes('L', (320, 160), bytes(255*v for v in grid))
    field = tiles.resize((WIDTH, HEIGHT), Image.Resampling.BICUBIC)
    return field.point(lambda value: 255 if value >= 128 else 0, mode='1')


def pack_native(image):
    pixels = image.load()
    data = bytearray(WIDTH*HEIGHT//8)
    for page in range(HEIGHT//8):
        for x in range(WIDTH):
            value = 0
            for bit in range(8):
                if pixels[x, page*8+bit]:
                    value |= 1 << bit
            data[page*WIDTH+x] = value
    return bytes(data)
