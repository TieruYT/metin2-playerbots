"""Render each map's terrain into a small PNG the panel can use as a backdrop.

Not a screenshot of the game: this is drawn from the same server_attr the bots
navigate by, so what a viewer sees is exactly what the bots can walk on. Land,
water and blocked ground each get a colour, and the result is downsampled to a
tile small enough to embed in the panel source.
"""

import base64
import io as _io
import struct
import sys

import lzo
from PIL import Image

SECTOR_CELLS = 128
CELL_SIZE = 50
ATTR_BLOCK = 1 << 0
ATTR_WATER = 1 << 1
ATTR_OBJECT = 1 << 7

TILE = 256

# index -> (folder, base_x, base_y, width, height) -- the bounds the panel uses.
MAPS = [
    (21, "metin2_map_b1", 0, 102400, 102400, 128000),
    (23, "metin2_map_b3", 102400, 204800, 102400, 102400),
    (24, "metin2_map_guild_02", 179200, 0, 51200, 51200),
    (25, "metin2_map_monkey_dungeon_12", 844800, 435200, 76800, 76800),
    (63, "metin2_map_n_desert_01", 204800, 486400, 153600, 153600),
    (64, "map_n_threeway", 256000, 665600, 153600, 153600),
]

COLOUR_LAND = (58, 74, 44)
COLOUR_LAND_ALT = (68, 84, 50)
COLOUR_WATER = (30, 58, 92)
COLOUR_BLOCK = (26, 24, 18)
COLOUR_OUTSIDE = (14, 12, 8)


def load(path):
    with open(path, "rb") as handle:
        data = handle.read()
    width, height = struct.unpack_from("<ii", data, 0)
    offset = 8
    sectors = {}
    for sy in range(height):
        for sx in range(width):
            (size,) = struct.unpack_from("<I", data, offset)
            offset += 4
            raw = lzo.decompress(data[offset:offset + size], False,
                                 SECTOR_CELLS * SECTOR_CELLS * 4)
            offset += size
            sectors[(sx, sy)] = struct.unpack("<%dI" % (SECTOR_CELLS * SECTOR_CELLS), raw)
    return width, height, sectors


def render(root, folder, base_x, base_y, width, height):
    sw, sh, sectors = load("%s/%s/server_attr" % (root, folder))
    W, H = sw * SECTOR_CELLS, sh * SECTOR_CELLS

    def attr(cx, cy):
        if cx < 0 or cy < 0 or cx >= W or cy >= H:
            return None
        sec = sectors.get((cx // SECTOR_CELLS, cy // SECTOR_CELLS))
        if sec is None:
            return None
        return sec[(cy % SECTOR_CELLS) * SECTOR_CELLS + (cx % SECTOR_CELLS)]

    image = Image.new("RGB", (TILE, TILE), COLOUR_OUTSIDE)
    pixels = image.load()
    # Each tile pixel covers a square of world units; sample its centre.
    for py in range(TILE):
        wy = base_y + (py + 0.5) * height / TILE
        cy = int((wy - base_y) // CELL_SIZE)
        for px in range(TILE):
            wx = base_x + (px + 0.5) * width / TILE
            cx = int((wx - base_x) // CELL_SIZE)
            a = attr(cx, cy)
            if a is None:
                continue
            if a & ATTR_WATER:
                pixels[px, py] = COLOUR_WATER
            elif a & (ATTR_BLOCK | ATTR_OBJECT):
                pixels[px, py] = COLOUR_BLOCK
            else:
                pixels[px, py] = COLOUR_LAND_ALT if ((px ^ py) & 8) else COLOUR_LAND

    image = image.convert("P", palette=Image.ADAPTIVE, colors=16)
    buffer = _io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def main():
    root = sys.argv[1]
    print("PLAYERBOT_MAP_TILES = {")
    total = 0
    for index, folder, bx, by, w, h in MAPS:
        try:
            png = render(root, folder, bx, by, w, h)
        except Exception as exc:                      # noqa: BLE001
            sys.stderr.write("  %s: %s\n" % (folder, exc))
            continue
        total += len(png)
        sys.stderr.write("  %-32s %6d B\n" % (folder, len(png)))
        encoded = base64.b64encode(png).decode("ascii")
        print('    %d: (' % index)
        for offset in range(0, len(encoded), 96):
            print('        "%s"' % encoded[offset:offset + 96])
        print('    ),')
    print("}")
    sys.stderr.write("razem %d B\n" % total)


if __name__ == "__main__":
    main()
