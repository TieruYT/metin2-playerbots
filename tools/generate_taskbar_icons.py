# -*- coding: utf-8 -*-
"""The taskbar's two buttons of ours - Towarzysz and Auto Lowy - made from the
original taskbar's own art (Tieru, 25 September: "na zasadzie oryginalnych
ikonek").

Usage:
    python tools/generate_taskbar_icons.py <extracted etc pack> [--preview <png>]

<extracted etc pack> is a directory holding the client's etc pack as
tools/eterpack.py extracts it - d:/ymir work/ui/taskbar.tga and the .sub
files of d:/ymir work/ui/game/taskbar/ - under ui/ (the "d:/ymir work/"
prefix dropped). Writes linux-port-mt2009/client-root/playerbot_ui/
{sidekick,autohunt}_button_0{1,2,3}.tga, which the root pack carries and
uiscript/taskbar.py names.

How they are made, so a later hand can change them the same way:

  * The frame is the original's: the four buttons of the bottom right
    (character, inventory, community, system) share it pixel for pixel, and
    everything inside it is the art. The empty plate inside is the median of
    the four at each pixel, which is their common background wherever at
    least two of them have no art there.
  * The art is recoloured into the originals' own tones: a lookup from
    luminance to colour, taken from the character button's helmet.
  * Auto Lowy is the sword and the "A" of the auto-attack mouse button, the
    game's own picture of attacking by itself. Towarzysz is the character
    button's helmet, smaller, with two small crossed swords under it - a
    warrior at your side - so it does not read as the community button's two
    heads.
  * The three states are the originals' measured ones: hover 1.18 times the
    brightness of the plain button, pressed 0.74 times it.
  * Written as the originals are: 32x32, 32 bits, RLE (type 10), bottom-left
    origin (descriptor 0x08) - Pillow's TGA writer with compression tga_rle.
"""
import argparse
import os
import re

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root', 'playerbot_ui'))

ORIGINALS = ('character_button', 'inventory_button', 'community_button', 'system_button')
HOVER = 1.18
PRESSED = 0.74


def sub_box(root, name):
    text = open(os.path.join(root, 'ui', 'game', 'taskbar', name + '.sub'), 'rb').read().decode('latin-1')
    return tuple(int(re.search(r'%s\s+(\d+)' % key, text).group(1)) for key in ('left', 'top', 'right', 'bottom'))


def load(root):
    texture = Image.open(os.path.join(root, 'ui', 'taskbar.tga')).convert('RGBA')

    def crop(name):
        return texture.crop(sub_box(root, name))
    return crop


def luminance(p):
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) // 1000


def frame_mask(icons):
    """True where the four originals agree: the shared frame (and the
    transparent corners outside it)."""
    mask = {}
    for y in range(32):
        for x in range(32):
            px = [icon.getpixel((x, y)) for icon in icons]
            mask[(x, y)] = all(max(abs(a - b) for a, b in zip(px[0], p)) <= 6 for p in px[1:])
    return mask


def empty_plate(icons, frame):
    """The inside with no art: the median of the four at each pixel, blurred
    so the ghosts of their pictures do not show, the frame left as it is."""
    median = icons[0].copy()
    for (x, y), shared in frame.items():
        if shared:
            continue
        channels = []
        for c in range(4):
            values = sorted(icon.getpixel((x, y))[c] for icon in icons)
            channels.append((values[1] + values[2]) // 2)
        median.putpixel((x, y), tuple(channels))
    blurred = median.filter(ImageFilter.GaussianBlur(3))
    plate = median.copy()
    for (x, y), shared in frame.items():
        if not shared:
            r, g, b, _ = blurred.getpixel((x, y))
            plate.putpixel((x, y), (r, g, b, 255))
    return plate


def art_mask(icon, plate, frame, threshold=40):
    """The pixels of an original's art: inside the frame, and away from the
    plate by more than the threshold."""
    mask = Image.new('L', (32, 32), 0)
    for (x, y), shared in frame.items():
        if shared:
            continue
        a, b = icon.getpixel((x, y)), plate.getpixel((x, y))
        if max(abs(a[c] - b[c]) for c in range(3)) > threshold:
            mask.putpixel((x, y), 255)
    return mask


def tone_lookup(icon, mask):
    """Luminance -> the original art's colour at that luminance."""
    sums = {}
    for y in range(32):
        for x in range(32):
            if mask.getpixel((x, y)):
                p = icon.getpixel((x, y))
                key = luminance(p) // 8
                s = sums.setdefault(key, [0, 0, 0, 0])
                s[0] += p[0]
                s[1] += p[1]
                s[2] += p[2]
                s[3] += 1
    table = {}
    for key, s in sums.items():
        table[key] = (s[0] // s[3], s[1] // s[3], s[2] // s[3])
    keys = sorted(table)
    lut = []
    for i in range(32):
        nearest = min(keys, key=lambda k: abs(k - i))
        lut.append(table[nearest])
    return lut


def recolour(img, lut, low, high, first=6, last=26):
    """Grey art (alpha kept) into the originals' tones: its luminance range
    [low, high] onto the lookup's entries first..last - the originals' art is
    half-tones with a few highlights, and a picture mapped onto the whole
    range reads as white."""
    out = Image.new('RGBA', img.size, (0, 0, 0, 0))
    for y in range(img.height):
        for x in range(img.width):
            p = img.getpixel((x, y))
            if p[3] == 0:
                continue
            lum = luminance(p)
            t = 0.0 if high <= low else max(0.0, min(1.0, float(lum - low) / (high - low)))
            r, g, b = lut[int(first + t * (last - first))]
            out.putpixel((x, y), (r, g, b, p[3]))
    return out


def shaded(shape, shading):
    """The shape's alpha (the white state, crisp) with the shading of the
    plain state (the sepia one, which has its highlights)."""
    out = Image.new('RGBA', shape.size, (0, 0, 0, 0))
    for y in range(shape.height):
        for x in range(shape.width):
            a = shape.getpixel((x, y))[3]
            if a:
                lum = luminance(shading.getpixel((x, y)))
                out.putpixel((x, y), (lum, lum, lum, a))
    return out


def cut_art(icon, background_luminance_below=70):
    """The bright art of a mouse-mode button (white sword and letter on dark
    stone), its frame left out: alpha from how far above the stone a pixel is."""
    art = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
    for y in range(4, 28):
        for x in range(4, 28):
            p = icon.getpixel((x, y))
            lum = luminance(p)
            if lum > background_luminance_below:
                alpha = min(255, (lum - background_luminance_below) * 3)
                art.putpixel((x, y), (lum, lum, lum, alpha))
    return art


def compose(plate, frame, art, offset):
    icon = plate.copy()
    layer = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
    layer.alpha_composite(art, offset)
    inside = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
    inside.alpha_composite(icon)
    inside.alpha_composite(layer)
    # The frame stays the original's: the art only ever shows inside it.
    for (x, y), shared in frame.items():
        if not shared:
            icon.putpixel((x, y), inside.getpixel((x, y)))
    return icon


def state(icon, factor):
    out = icon.copy()
    px = out.load()
    for y in range(32):
        for x in range(32):
            r, g, b, a = px[x, y]
            px[x, y] = (min(255, int(r * factor)), min(255, int(g * factor)), min(255, int(b * factor)), a)
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('root')
    parser.add_argument('--preview')
    args = parser.parse_args()
    crop = load(args.root)

    originals = [crop(name + '_01') for name in ORIGINALS]
    frame = frame_mask(originals)
    plate = empty_plate(originals, frame)
    helmet_icon = originals[0]
    helmet = art_mask(helmet_icon, plate, frame)
    lut = tone_lookup(helmet_icon, helmet)

    # Auto Lowy: the auto-attack button's sword and "A" - the crisp shape of
    # its lit state, the shading of its plain one - in the originals' tones.
    shape = cut_art(crop('mouse_button_auto_attack_02'))
    shading = crop('mouse_button_auto_attack_01')
    auto = shaded(shape, shading)
    lums = [luminance(p) for p in auto.getdata() if p[3]]
    auto = recolour(auto, lut, min(lums), max(lums), 12, 30)
    # A pixel bolder, the blade only: the mouse button's sword is drawn for a
    # smaller inside, and at its own weight it read as a scratch beside the
    # helmet. The added pixel is the dark outline the originals' art has; the
    # "A" in the bottom right quarter keeps its own shape.
    outline = lut[4]
    bolder = auto.copy()
    for y in range(31):
        for x in range(31):
            if x >= 18 and y >= 18:
                continue
            if auto.getpixel((x, y))[3]:
                continue
            around = [auto.getpixel((x + dx, y + dy))[3] for dx, dy in ((1, 0), (0, 1), (1, 1))]
            if max(around) > 128:
                bolder.putpixel((x, y), (outline[0], outline[1], outline[2], max(around)))
    autohunt = compose(plate, frame, bolder, (0, 0))

    # Towarzysz: the character button's inside turned the other way - the one
    # at your side, facing you - with two small crossed swords before it. The
    # whole inside is mirrored, background and all, so the helmet keeps every
    # pixel of the original; the frame stays unmirrored, lit as the others.
    mirrored = plate.copy()
    for (x, y), shared in frame.items():
        if not shared and not frame[(31 - x, y)]:
            mirrored.putpixel((x, y), helmet_icon.getpixel((31 - x, y)))
    blade = cut_art(crop('mouse_button_auto_attack_02'))
    blade_shading = crop('mouse_button_auto_attack_01')
    # The sword alone: the "A" box sits in the bottom right quarter.
    for y in range(18, 32):
        for x in range(18, 32):
            blade.putpixel((x, y), (0, 0, 0, 0))
    blade = shaded(blade, blade_shading)
    blade = blade.crop(blade.getbbox())
    small = blade.resize((max(1, int(blade.width * 0.5)), max(1, int(blade.height * 0.5))), Image.LANCZOS)
    lums = [luminance(p) for p in small.getdata() if p[3]]
    small = recolour(small, lut, min(lums), max(lums), 10, 29)
    crossed = Image.new('RGBA', (small.width + 3, small.height), (0, 0, 0, 0))
    crossed.alpha_composite(small, (0, 0))
    crossed.alpha_composite(small.transpose(Image.FLIP_LEFT_RIGHT), (3, 0))
    sidekick = compose(mirrored, frame, crossed, (5, 32 - crossed.height - 5))

    os.makedirs(OUT, exist_ok=True)
    written = []
    for name, icon in (('sidekick_button', sidekick), ('autohunt_button', autohunt)):
        for suffix, factor in (('01', 1.0), ('02', HOVER), ('03', PRESSED)):
            path = os.path.join(OUT, '%s_%s.tga' % (name, suffix))
            state(icon, factor).save(path, compression='tga_rle')
            written.append(path)
    for path in written:
        print('generate_taskbar_icons: %s' % os.path.relpath(path, os.path.join(HERE, '..')))

    if args.preview:
        scale = 4
        row = originals + [sidekick, autohunt]
        sheet = Image.new('RGBA', (len(row) * (32 * scale + 8) + 8, 3 * (32 * scale + 8) + 8), (40, 40, 40, 255))
        for r, factor in enumerate((1.0, HOVER, PRESSED)):
            for c, icon in enumerate(row):
                big = state(icon, factor).resize((32 * scale, 32 * scale), Image.NEAREST)
                sheet.alpha_composite(big, (8 + c * (32 * scale + 8), 8 + r * (32 * scale + 8)))
        sheet.save(args.preview)
        print('generate_taskbar_icons: preview %s' % args.preview)


if __name__ == '__main__':
    main()
