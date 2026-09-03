#!/usr/bin/env python3
"""Decode a Metin2 r40250 `server_attr` file and locate fishable bank tiles.

Format, from SECTREE_MANAGER::LoadAttribute (sectree_manager.cpp:460-541):

    int32 width, height          -- in attribute sectors
    per sector, row-major:
        uint32 compressed_size
        compressed_size bytes    -- lzo1x, expands to 128*128 uint32 attributes

SECTREE_SIZE is 6400 and CELL_SIZE is 50 (sectree.h), so a sector holds
128x128 cells and each cell covers 50 world units.  ATTR_BLOCK is bit 0 and
ATTR_WATER is bit 1.

This is a one-shot analysis helper, not part of the server build.
"""

import struct
import sys

import lzo

SECTOR_CELLS = 128
CELL_SIZE = 50
SECTOR_SIZE = SECTOR_CELLS * CELL_SIZE  # 6400

ATTR_BLOCK = 1 << 0
ATTR_WATER = 1 << 1


def load(path):
    with open(path, "rb") as handle:
        data = handle.read()

    width, height = struct.unpack_from("<ii", data, 0)
    offset = 8
    cells = {}

    for sector_y in range(height):
        for sector_x in range(width):
            (size,) = struct.unpack_from("<I", data, offset)
            offset += 4
            raw = lzo.decompress(
                data[offset:offset + size], False, SECTOR_CELLS * SECTOR_CELLS * 4
            )
            offset += size
            attrs = struct.unpack("<%dI" % (SECTOR_CELLS * SECTOR_CELLS), raw)
            cells[(sector_x, sector_y)] = attrs

    return width, height, cells


def attribute_at(cells, width, height, cx, cy):
    """Attribute of a global cell, or None when outside the map."""
    sector_x, sector_y = cx // SECTOR_CELLS, cy // SECTOR_CELLS
    if not (0 <= sector_x < width and 0 <= sector_y < height):
        return None
    sector = cells.get((sector_x, sector_y))
    if sector is None:
        return None
    return sector[(cy % SECTOR_CELLS) * SECTOR_CELLS + (cx % SECTOR_CELLS)]


def dump_grid(cells, width, height, base_x, base_y, target_x, target_y, radius):
    """ASCII view of the terrain: '~' water, '#' blocked, '.' standable."""
    centre_cx = (target_x - base_x) // CELL_SIZE
    centre_cy = (target_y - base_y) // CELL_SIZE
    span = radius // CELL_SIZE

    print("grid centred on world=(%d,%d) cell=(%d,%d), one char = %d units"
          % (target_x, target_y, centre_cx, centre_cy, CELL_SIZE))
    print("x from %d to %d, y downward from %d"
          % (base_x + (centre_cx - span) * CELL_SIZE,
             base_x + (centre_cx + span) * CELL_SIZE,
             base_y + (centre_cy - span) * CELL_SIZE))

    for cy in range(centre_cy - span, centre_cy + span + 1):
        row = []
        for cx in range(centre_cx - span, centre_cx + span + 1):
            attr = attribute_at(cells, width, height, cx, cy)
            if attr is None:
                row.append(" ")
            elif cx == centre_cx and cy == centre_cy:
                row.append("N")
            elif attr & ATTR_WATER:
                row.append("~")
            elif attr & ATTR_BLOCK:
                row.append("#")
            else:
                row.append(".")
        print("%6d %s" % (base_y + cy * CELL_SIZE, "".join(row)))


def main():
    path, base_x, base_y, target_x, target_y, radius = (
        sys.argv[1], int(sys.argv[2]), int(sys.argv[3]),
        int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]),
    )
    mode = sys.argv[7] if len(sys.argv) > 7 else "bank"

    width, height, cells = load(path)
    if mode == "grid":
        dump_grid(cells, width, height, base_x, base_y,
                  target_x, target_y, radius)
        return
    if mode == "probe":
        for raw in sys.argv[8:]:
            px, py = (int(v) for v in raw.split(","))
            attr = attribute_at(cells, width, height,
                                (px - base_x) // CELL_SIZE,
                                (py - base_y) // CELL_SIZE)
            kind = "outside" if attr is None else (
                "water" if attr & ATTR_WATER else
                ("blocked" if attr & ATTR_BLOCK else "standable"))
            print("  (%d,%d) -> %s" % (px, py, kind))
        return

    total = width * height * SECTOR_CELLS * SECTOR_CELLS
    water = sum(
        1 for sector in cells.values() for a in sector if a & ATTR_WATER
    )
    print("sectors=%dx%d cells=%d water_cells=%d (%.2f%%)"
          % (width, height, total, water, 100.0 * water / total))

    # World -> cell, relative to the map's BasePosition.
    centre_cx = (target_x - base_x) // CELL_SIZE
    centre_cy = (target_y - base_y) // CELL_SIZE
    span = radius // CELL_SIZE

    print("fisherman world=(%d,%d) cell=(%d,%d)"
          % (target_x, target_y, centre_cx, centre_cy))

    # A bank tile: standable (not blocked, not water) and within one cell of
    # water, so the line reaches the surface.  Score by distance to the NPC.
    candidates = []
    for cy in range(centre_cy - span, centre_cy + span + 1):
        for cx in range(centre_cx - span, centre_cx + span + 1):
            attr = attribute_at(cells, width, height, cx, cy)
            if attr is None or attr & (ATTR_BLOCK | ATTR_WATER):
                continue

            neighbours = []
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    if dx == 0 and dy == 0:
                        continue
                    near = attribute_at(cells, width, height, cx + dx, cy + dy)
                    if near is not None and near & ATTR_WATER:
                        neighbours.append((dx, dy))
            if not neighbours:
                continue

            wx = base_x + cx * CELL_SIZE
            wy = base_y + cy * CELL_SIZE
            dist = abs(wx - target_x) + abs(wy - target_y)
            # Face the average direction of the adjacent water.
            avg_dx = sum(d[0] for d in neighbours) / float(len(neighbours))
            avg_dy = sum(d[1] for d in neighbours) / float(len(neighbours))
            candidates.append((dist, wx, wy, avg_dx, avg_dy, len(neighbours)))

    candidates.sort()
    print("bank candidates within %d units: %d" % (radius, len(candidates)))
    for dist, wx, wy, avg_dx, avg_dy, count in candidates[:12]:
        print("  dist=%6d bank=(%d,%d) water_dir=(%+.2f,%+.2f) water_neighbours=%d"
              % (dist, wx, wy, avg_dx, avg_dy, count))


if __name__ == "__main__":
    main()
