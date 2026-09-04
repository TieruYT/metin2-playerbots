"""Which parts of a map can a bot actually reach from a given point?

One-shot analysis, not part of the server build. Decodes server_attr the way
SECTREE_MANAGER::LoadAttribute does, applies the same blocking rule the bot
navigation uses (ATTR_BLOCK | ATTR_WATER | ATTR_OBJECT), flood-fills from a
start point and reports how much of the map's spawn table ends up on the same
side of the walls.
"""

import collections
import struct
import sys

import lzo

SECTOR_CELLS = 128
CELL_SIZE = 50
ATTR_BLOCK = 1 << 0
ATTR_WATER = 1 << 1
ATTR_OBJECT = 1 << 7
BLOCKING = ATTR_BLOCK | ATTR_WATER | ATTR_OBJECT


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


def blocked(sectors, sw, sh, cx, cy):
    if cx < 0 or cy < 0 or cx >= sw * SECTOR_CELLS or cy >= sh * SECTOR_CELLS:
        return True
    sec = sectors.get((cx // SECTOR_CELLS, cy // SECTOR_CELLS))
    if sec is None:
        return True
    attr = sec[(cy % SECTOR_CELLS) * SECTOR_CELLS + (cx % SECTOR_CELLS)]
    return bool(attr & BLOCKING)


def flood(sectors, sw, sh, start_cx, start_cy):
    if blocked(sectors, sw, sh, start_cx, start_cy):
        # Snap to the nearest open cell, as the navigation does.
        for radius in range(1, 40):
            for dx in range(-radius, radius + 1):
                for dy in (-radius, radius):
                    if not blocked(sectors, sw, sh, start_cx + dx, start_cy + dy):
                        start_cx, start_cy = start_cx + dx, start_cy + dy
                        radius = -1
                        break
                if radius == -1:
                    break
            if radius == -1:
                break
        else:
            return set()

    seen = {(start_cx, start_cy)}
    queue = collections.deque([(start_cx, start_cy)])
    while queue:
        cx, cy = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if (nx, ny) in seen or blocked(sectors, sw, sh, nx, ny):
                continue
            seen.add((nx, ny))
            queue.append((nx, ny))
    return seen


def main():
    attr_path, base_x, base_y, regen_path = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    candidates = []
    for pair in sys.argv[5:]:
        gx, gy = pair.split(",")
        candidates.append((int(gx), int(gy)))

    sw, sh, sectors = load(attr_path)
    print("mapa: %d x %d sektorow (%d x %d komorek)" %
          (sw, sh, sw * SECTOR_CELLS, sh * SECTOR_CELLS))

    spawns = []
    with open(regen_path, "r", errors="replace") as handle:
        for line in handle:
            parts = line.split("\t")
            if len(parts) > 4 and parts[1].strip().isdigit() and parts[2].strip().isdigit():
                lx, ly = int(parts[1]), int(parts[2])
                spawns.append((base_x + lx * 100, base_y + ly * 100))
    print("grup spawnu w regen.txt: %d" % len(spawns))

    total_open = None
    for gx, gy in candidates:
        cx = (gx - base_x) // CELL_SIZE
        cy = (gy - base_y) // CELL_SIZE
        component = flood(sectors, sw, sh, cx, cy)
        if not component:
            print("\n(%d, %d): nie ma tam chodliwej komorki" % (gx, gy))
            continue
        reachable = 0
        for sx, sy in spawns:
            scx, scy = (sx - base_x) // CELL_SIZE, (sy - base_y) // CELL_SIZE
            hit = False
            for dx in range(-6, 7):
                for dy in range(-6, 7):
                    if (scx + dx, scy + dy) in component:
                        hit = True
                        break
                if hit:
                    break
            if hit:
                reachable += 1
        xs = [c[0] for c in component]
        ys = [c[1] for c in component]
        print("\n(%d, %d):" % (gx, gy))
        print("  komorek w spojnej czesci: %d" % len(component))
        print("  zasieg globalny: x %d..%d   y %d..%d" %
              (base_x + min(xs) * CELL_SIZE, base_x + max(xs) * CELL_SIZE,
               base_y + min(ys) * CELL_SIZE, base_y + max(ys) * CELL_SIZE))
        print("  osiagalnych grup spawnu: %d z %d  (%.0f%%)" %
              (reachable, len(spawns), 100.0 * reachable / max(1, len(spawns))))


if __name__ == "__main__":
    main()
