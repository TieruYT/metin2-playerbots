"""What joins the pieces of a map, and what a bot's blocking rule does to it.

One-shot analysis, not part of the server build. Companion to
analyse_map_reach.py, which answers "what can a bot reach"; this one answers
"why can it not reach the rest".

server_attr carries two bits that look alike and are not. ATTR_BLOCK means
nothing may stand here. ATTR_WATER describes the terrain, and the engine tests
it for exactly one thing - whether there is water in front of a fishing rod.
A river carries both bits. A bridge deck over that river carries water with the
block bit cleared, and so does a shallow ford; that is how the map data spells
"you may cross here".

The bot navigation refused all three for years, so it refused every bridge. Orc
Valley is twenty-three islands in a delta joined by twenty-two of them: bots
came in, landed on the middle island and never left it, which is what this
script was written to prove.

    python analyse_map_bridges.py <server_attr> <base_x> <base_y> \\
        <regen.txt> <start_x> <start_y> [out.png]

It prints, for the same start point under three rules, how much of the map is
reachable and how many of its spawn groups are; then the crossings as clusters,
with the size and shape of each. With an output path it also draws the map:
rivers in blue, crossings in gold, and the ground reachable today picked out
against the ground that is not.

Needs python-lzo and pillow, neither of which builds on Windows. Run it in a
container:

    docker run --rm -v "$(pwd -W)":/w -w /w python:3.11-slim bash -c \\
      "apt-get update -qq && apt-get install -y -qq gcc python3-dev liblzo2-dev \\
       && pip install -q python-lzo pillow && python tools/analyse_map_bridges.py ..."
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

RULES = [
    ("BLOCK|WATER|OBJECT - regula sprzed poprawki",
     ATTR_BLOCK | ATTR_WATER | ATTR_OBJECT),
    ("BLOCK|OBJECT - regula obecna", ATTR_BLOCK | ATTR_OBJECT),
    ("sam BLOCK", ATTR_BLOCK),
]


def load(path):
    """server_attr as one flat grid, the way SECTREE_MANAGER::LoadAttribute reads it."""
    with open(path, "rb") as handle:
        data = handle.read()
    width, height = struct.unpack_from("<ii", data, 0)
    offset = 8
    grid = [[0] * (width * SECTOR_CELLS) for _ in range(height * SECTOR_CELLS)]
    for sy in range(height):
        for sx in range(width):
            (size,) = struct.unpack_from("<I", data, offset)
            offset += 4
            raw = lzo.decompress(data[offset:offset + size], False,
                                 SECTOR_CELLS * SECTOR_CELLS * 4)
            offset += size
            attrs = struct.unpack("<%dI" % (SECTOR_CELLS * SECTOR_CELLS), raw)
            for row in range(SECTOR_CELLS):
                target = grid[sy * SECTOR_CELLS + row]
                start = row * SECTOR_CELLS
                target[sx * SECTOR_CELLS:sx * SECTOR_CELLS + SECTOR_CELLS] = \
                    attrs[start:start + SECTOR_CELLS]
    return width * SECTOR_CELLS, height * SECTOR_CELLS, grid


def flood(grid, w, h, start, mask):
    """Everything four-connected to start, snapping to the nearest open cell."""
    sx, sy = start
    if not (0 <= sx < w and 0 <= sy < h) or (grid[sy][sx] & mask):
        found = None
        for radius in range(1, 80):
            for dx in range(-radius, radius + 1):
                for dy in (-radius, radius):
                    x, y = sx + dx, sy + dy
                    if 0 <= x < w and 0 <= y < h and not (grid[y][x] & mask):
                        found = (x, y)
                        break
                if found:
                    break
            if found:
                break
        if not found:
            return set()
        sx, sy = found
    seen = {(sx, sy)}
    queue = collections.deque([(sx, sy)])
    while queue:
        cx, cy = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if not (0 <= nx < w and 0 <= ny < h) or (nx, ny) in seen:
                continue
            if grid[ny][nx] & mask:
                continue
            seen.add((nx, ny))
            queue.append((nx, ny))
    return seen


def clusters_of(cells):
    """The cells grouped into eight-connected clumps, largest first."""
    out = []
    unseen = set(cells)
    while unseen:
        seed = unseen.pop()
        cluster = {seed}
        queue = collections.deque([seed])
        while queue:
            cx, cy = queue.popleft()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    neighbour = (cx + dx, cy + dy)
                    if neighbour in unseen:
                        unseen.discard(neighbour)
                        cluster.add(neighbour)
                        queue.append(neighbour)
        out.append(cluster)
    out.sort(key=len, reverse=True)
    return out


def read_spawns(path, base_x, base_y):
    spawns = []
    with open(path, "r", errors="replace") as handle:
        for line in handle:
            parts = line.split("\t")
            if len(parts) > 4 and parts[1].strip().isdigit() and parts[2].strip().isdigit():
                spawns.append((base_x + int(parts[1]) * 100,
                               base_y + int(parts[2]) * 100))
    return spawns


def draw(grid, w, h, reachable, path, scale=4):
    from PIL import Image

    image = Image.new("RGB", (w // scale, h // scale), (18, 16, 12))
    pixels = image.load()
    for y in range(0, h, scale):
        for x in range(0, w, scale):
            attr = grid[y][x]
            if (attr & ATTR_WATER) and not (attr & ATTR_BLOCK):
                colour = (255, 210, 60)          # a crossing
            elif attr & ATTR_WATER:
                colour = (34, 68, 110)           # river, blocked for everybody
            elif attr & ATTR_BLOCK:
                colour = (40, 36, 30)            # cliff or wall
            elif (x, y) in reachable:
                colour = (96, 132, 74)           # ground a bot can reach
            else:
                colour = (62, 84, 50)            # ground it cannot
            pixels[x // scale, y // scale] = colour
    image.save(path)


def main():
    attr_path, base_x, base_y = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    regen_path = sys.argv[4]
    start_x, start_y = int(sys.argv[5]), int(sys.argv[6])
    out = sys.argv[7] if len(sys.argv) > 7 else None

    w, h, grid = load(attr_path)
    print("mapa: %d x %d komorek (%d x %d jednostek)"
          % (w, h, w * CELL_SIZE, h * CELL_SIZE))

    histogram = collections.Counter()
    for row in grid:
        histogram.update(row)
    print("atrybuty:")
    for attr, count in histogram.most_common(12):
        names = []
        if attr & ATTR_BLOCK:
            names.append("BLOCK")
        if attr & ATTR_WATER:
            names.append("WATER")
        if attr & ATTR_OBJECT:
            names.append("OBJECT")
        for bit in range(32):
            if attr & (1 << bit) and bit not in (0, 1, 7):
                names.append("bit%d" % bit)
        print("  0x%08x %-28s %9d komorek (%5.2f%%)"
              % (attr, "+".join(names) or "wolne", count, 100.0 * count / (w * h)))

    spawns = read_spawns(regen_path, base_x, base_y)
    print("grup spawnu w regen.txt: %d" % len(spawns))

    start = ((start_x - base_x) // CELL_SIZE, (start_y - base_y) // CELL_SIZE)
    first_component = None
    for label, mask in RULES:
        component = flood(grid, w, h, start, mask)
        if first_component is None:
            first_component = component
        walkable = sum(1 for row in grid for attr in row if not (attr & mask))
        reachable = 0
        for spawn_x, spawn_y in spawns:
            scx = (spawn_x - base_x) // CELL_SIZE
            scy = (spawn_y - base_y) // CELL_SIZE
            # A spawn rectangle is wider than a cell, so allow a little slack.
            if any((scx + dx, scy + dy) in component
                   for dx in range(-6, 7) for dy in range(-6, 7)):
                reachable += 1
        print("\n%s" % label)
        print("  komorek chodliwych:      %d" % walkable)
        print("  osiagalnych ze startu:   %d (%.1f%%)"
              % (len(component), 100.0 * len(component) / max(1, walkable)))
        print("  osiagalnych grup spawnu: %d z %d (%.0f%%)"
              % (reachable, len(spawns), 100.0 * reachable / max(1, len(spawns))))

    crossings = set()
    for y in range(h):
        row = grid[y]
        for x in range(w):
            attr = row[x]
            if (attr & ATTR_WATER) and not (attr & ATTR_BLOCK):
                crossings.add((x, y))
    parts = clusters_of(crossings)
    print("\nprzeprawy (woda bez blokady): %d komorek w %d skupiskach"
          % (len(crossings), len(parts)))
    for cluster in parts[:20]:
        xs = [c[0] for c in cluster]
        ys = [c[1] for c in cluster]
        print("  %5d komorek, %6d x %6d jednostek, srodek (%d, %d)"
              % (len(cluster), (max(xs) - min(xs) + 1) * CELL_SIZE,
                 (max(ys) - min(ys) + 1) * CELL_SIZE,
                 base_x + (min(xs) + max(xs)) // 2 * CELL_SIZE,
                 base_y + (min(ys) + max(ys)) // 2 * CELL_SIZE))

    if out:
        draw(grid, w, h, first_component, out)
        print("\nzapisano %s" % out)


if __name__ == "__main__":
    main()
