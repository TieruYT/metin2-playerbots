"""Draw a frontier map with what drops on it and where the crowds are.

One-shot analysis, not part of the server build. Companion to
analyse_map_bridges.py, which answers "what joins the pieces of this map";
this one answers "what is on each piece, and is anybody standing there".

The terrain comes from server_attr, the same file the bot navigation reads, so
green is ground a bot may stand on, navy is river and gold is a crossing. On
top of that go the spawns from regen.txt, coloured by the refine material the
monster carries in mob_proto column 33 (DROP_ITEM) - which is how the picture
answers the question it was written for. Orc Valley's bots all warped to one
point on the middle island and never left it; the Orc Amulet+ that 755 spawns
of them carry is almost entirely on the outer islands, and it took a drawing to
make that obvious.

Reading a regen line: the first field is the type, `m`/`b`/`e` naming a mob
directly, `g` a group and `r` a group_group, and the id is the last field. The
two group files are not the same shape - see the note above parse_blocks.

    python render_valley_spots.py [share_root] [out.png] [map folder]

Needs python-lzo and pillow, neither of which builds on Windows. Run it in a
container:

    docker run --rm -v "$(pwd -W)/../m2src-cache/tree/server40250/share":/share \
      -v "$(pwd -W)/out":/out -v "$(pwd -W)/tools":/scripts m2/maptools:1 \
      python /scripts/render_valley_spots.py
"""
import sys
import collections
import io
import re
import struct

import lzo
from PIL import Image, ImageDraw, ImageFont

SHARE = (sys.argv[1] if len(sys.argv) > 1 else '/share').rstrip('/') + '/'
OUT = sys.argv[2] if len(sys.argv) > 2 else '/out/dolina_orkow.png'
FOLDER = sys.argv[3] if len(sys.argv) > 3 else 'map_n_threeway'
L = SHARE + 'locale/english/'
BX, BY = 256000, 665600
SECTOR_CELLS = 128
CELL = 50
ATTR_BLOCK = 1 << 0
ATTR_WATER = 1 << 1
OUT_W = 3000

HUBS = [(315800, 732600), (342600, 729800), (335500, 758000), (328000, 743600),
        (277800, 793500), (347700, 797500), (334000, 800200), (343200, 743100),
        (391700, 696600), (330500, 727300), (292200, 751000), (365100, 777800),
        (271500, 683700), (297600, 716400), (302500, 777000), (336500, 703600)]
ARRIVAL = (327200, 742300)

ITEM_COLOR = {
    30007: (255, 60, 60),
    30076: (255, 145, 55),
    30008: (105, 205, 255),
    30078: (60, 135, 255),
    30006: (255, 230, 75),
    30077: (215, 185, 35),
    30046: (195, 105, 255),
    30051: (110, 255, 145),
    30079: (60, 210, 105),
    30047: (255, 115, 215),
}
NAMES = {
    30007: 'Amulet Orka', 30076: 'Amulet Orka +',
    30008: 'Ezot. Przewodnik', 30078: 'Ezot. Przewodnik +',
    30006: 'Zab Orka', 30077: 'Zab Orka +',
    30046: 'Ogon Skorpiona', 30051: 'Nieznany Talizman',
    30079: 'Nieznany Talizman +', 30047: 'Ksiega Klatw',
}


def font(size):
    for path in ('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
                 '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            pass
    return ImageFont.load_default()


def load_attr(path):
    data = open(path, 'rb').read()
    w, h = struct.unpack('<ii', data[:8])
    pos = 8
    grid = {}
    for sy in range(h):
        for sx in range(w):
            (size,) = struct.unpack('<I', data[pos:pos + 4])
            pos += 4
            blob = data[pos:pos + size]
            pos += size
            raw = lzo.decompress(blob, False, SECTOR_CELLS * SECTOR_CELLS * 4)
            grid[(sx, sy)] = struct.unpack('<%dI' % (SECTOR_CELLS * SECTOR_CELLS), raw)
    return w, h, grid


def blocks(path, middle):
    """Vnum -> the numbers its block names.

    The two files disagree about which field that is. A group.txt member reads
    `<idx> "<name>" <mob vnum>` and the mob is the last field; a
    group_group.txt member reads `<idx> <group vnum> <probability>` and the
    group is the middle one. Taking the last of both reads a probability as a
    group id, every `r` line resolves to nothing, and a map with four thousand
    spawns on it comes out empty."""
    out = collections.defaultdict(set)
    v = None
    mem = set()
    for line in io.open(path, encoding='cp949', errors='replace'):
        t = line.strip()
        if not t or t == '{':
            continue
        if t == '}':
            if v is not None:
                out[v] |= mem
            v = None
            mem = set()
            continue
        m = re.match(r'(?i)^vnum\s+(\d+)\s*$', t)
        if m:
            v = int(m.group(1))
            continue
        parts = t.split()
        if middle:
            if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
                mem.add(int(parts[1]))
        elif re.match(r'(?i)^(leader|\d+)\s', t):
            try:
                mem.add(int(parts[-1]))
            except ValueError:
                pass
    return out


w, h, grid = load_attr(L + 'map/' + FOLDER + '/server_attr')
CW, CH = w * SECTOR_CELLS, h * SECTOR_CELLS
base = Image.new('RGB', (CW, CH), (18, 20, 25))
p = base.load()
for sy in range(h):
    for sx in range(w):
        cells = grid[(sx, sy)]
        for cy in range(SECTOR_CELLS):
            row = cy * SECTOR_CELLS
            for cx in range(SECTOR_CELLS):
                a = cells[row + cx]
                if a & ATTR_BLOCK:
                    c = (28, 45, 80) if (a & ATTR_WATER) else (46, 48, 55)
                elif a & ATTR_WATER:
                    c = (210, 170, 60)
                else:
                    c = (64, 86, 60)
                p[sx * SECTOR_CELLS + cx, sy * SECTOR_CELLS + cy] = c

img = base.resize((OUT_W, OUT_W), Image.LANCZOS).convert('RGB')
d = ImageDraw.Draw(img)
K = float(OUT_W) / (CW * CELL)          # jednostki swiata -> piksele


def wx(x):
    return int((x - BX) * K)


def wy(y):
    return int((y - BY) * K)


groups = blocks(L + 'group.txt', False)
gg = blocks(L + 'group_group.txt', True)
mob = {}
for n, line in enumerate(io.open(SHARE + 'conf/mob_proto.txt', encoding='cp949',
                                 errors='replace')):
    if n == 0:
        continue
    c = line.rstrip('\n').split('\t')
    if len(c) < 33 or not c[0].isdigit():
        continue
    mob[int(c[0])] = {
        'lvl': int(c[5]) if c[5].lstrip('-').isdigit() else 0,
        'drop': int(c[32]) if c[32].lstrip('-').isdigit() else 0,
    }

spawns = []
for line in io.open(L + 'map/' + FOLDER + '/regen.txt', encoding='cp949',
                    errors='replace'):
    t = line.strip()
    if not t or t.startswith('#'):
        continue
    pr = t.split()
    if len(pr) < 11:
        continue
    k = pr[0].lower()[0]
    try:
        cx, cy, vid = int(pr[1]), int(pr[2]), int(pr[-1])
    except ValueError:
        continue
    ms = set()
    if k in 'mbe':
        ms = {vid}
    elif k == 'g':
        ms = groups.get(vid, set())
    elif k == 'r':
        for sub in gg.get(vid, set()):
            ms |= groups.get(sub, set())
    for m in ms:
        spawns.append((BX + cx * 100, BY + cy * 100, m))

# zwykle spawny w tle
for x, y, m in spawns:
    if not ITEM_COLOR.get(mob.get(m, {}).get('drop', 0)):
        px_, py_ = wx(x), wy(y)
        d.ellipse([px_ - 2, py_ - 2, px_ + 2, py_ + 2], fill=(128, 138, 128))
# te z ulepszaczem na wierzchu
for x, y, m in spawns:
    col = ITEM_COLOR.get(mob.get(m, {}).get('drop', 0))
    if col:
        px_, py_ = wx(x), wy(y)
        d.ellipse([px_ - 5, py_ - 5, px_ + 5, py_ + 5], fill=col,
                  outline=(20, 20, 20))

# najgestsze spoty: siatka 4000 jednostek
GRID = 4000
dens = collections.Counter()
for x, y, m in spawns:
    dens[(x // GRID, y // GRID)] += 1
top = dens.most_common(8)
f_spot = font(46)
f_hub = font(30)
for rank, ((gx, gy), n) in enumerate(top, 1):
    cx = gx * GRID + GRID // 2
    cy = gy * GRID + GRID // 2
    x, y = wx(cx), wy(cy)
    r = 40
    d.ellipse([x - r, y - r, x + r, y + r], outline=(255, 255, 255), width=6)
    d.ellipse([x - r + 6, y - r + 6, x + r - 6, y + r - 6],
              outline=(255, 60, 60), width=4)
    d.text((x - 14, y - 26), str(rank), fill=(255, 255, 255), font=f_spot)

for i, (hx, hy) in enumerate(HUBS):
    x, y = wx(hx), wy(hy)
    d.ellipse([x - 13, y - 13, x + 13, y + 13], outline=(235, 235, 235), width=3)
    d.text((x + 18, y - 14), 'hub %d' % i, fill=(235, 235, 235), font=f_hub)

ax, ay = wx(ARRIVAL[0]), wy(ARRIVAL[1])
d.ellipse([ax - 26, ay - 26, ax + 26, ay + 26], outline=(255, 55, 55), width=7)
d.text((ax + 34, ay - 16), 'WEJSCIE', fill=(255, 80, 80), font=f_spot)

# legenda
counts = collections.Counter(mob.get(m, {}).get('drop', 0) for _, _, m in spawns)
f_t = font(40)
f_l = font(30)
lw, lh = 660, 130 + 46 * len(ITEM_COLOR) + 46 * len(top) + 60
d.rectangle([26, 26, 26 + lw, 26 + lh], fill=(12, 14, 18), outline=(120, 120, 120),
            width=3)
x0, y0 = 52, 52
d.text((x0, y0), 'DOLINA ORKOW  (mapa 64)', fill=(255, 255, 255), font=f_t)
y0 += 52
d.text((x0, y0), '4041 spawnow, 32 rodzaje mobow, 22 mosty',
       fill=(190, 190, 190), font=f_l)
y0 += 58
for vnum, col in sorted(ITEM_COLOR.items(), key=lambda kv: -counts.get(kv[0], 0)):
    d.ellipse([x0, y0 + 6, x0 + 22, y0 + 28], fill=col, outline=(20, 20, 20))
    d.text((x0 + 38, y0), NAMES.get(vnum, str(vnum)), fill=(230, 230, 230), font=f_l)
    d.text((x0 + 430, y0), '%d' % counts.get(vnum, 0), fill=(230, 230, 230), font=f_l)
    y0 += 46
y0 += 20
d.text((x0, y0), 'Najgestsze spoty', fill=(255, 255, 255), font=f_t)
y0 += 52
for rank, ((gx, gy), n) in enumerate(top, 1):
    d.text((x0, y0), '%d.  %d spawnow  na (%d, %d)'
           % (rank, n, gx * GRID + GRID // 2, gy * GRID + GRID // 2),
           fill=(230, 230, 230), font=f_l)
    y0 += 46

img.save(OUT)
print('wrote %s  %dx%d  spawns=%d  busiest spot=%d'
      % (OUT, img.size[0], img.size[1], len(spawns), top[0][1]))
