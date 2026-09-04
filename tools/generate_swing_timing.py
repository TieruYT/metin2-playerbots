# -*- coding: utf-8 -*-
"""Generate the swing-timing table from the client motion data the server ships.

DirectInputTime is the earliest moment the client will accept the next combo
input. Sending before it is what cuts the animation short; the table replaces a
flat 480 ms that was wrong for every weapon in the game.
"""
import re, io, os

BASE = ("C:/Users/dawio/Documents/Codex/2026-08-14/https-github-com-azzlacksyndicate-metin2-singleplayer/"
        "work/m2src-cache/tree/server40250/share/data/pc")
OUT = ("C:/Users/dawio/Documents/Codex/2026-08-14/https-github-com-azzlacksyndicate-metin2-singleplayer/"
       "work/metin2-suite/linux-port/overlays/playerbot/src/game/src/playerbot_swing_timing.h")

JOBS = ['warrior', 'assassin', 'sura', 'shaman']          # JOB_WARRIOR..JOB_SHAMAN
# WEAPON_SWORD, DAGGER, BOW, TWO_HANDED, BELL, FAN - the order of the engine enum.
WEAPONS = ['onehand_sword', 'dualhand_sword', 'bow', 'twohand_sword', 'bell', 'fan']
COMBOS = 4                                                 # MOTION_COMBO_ATTACK_1..4


def read(path, key):
    if not os.path.exists(path):
        return None
    txt = io.open(path, encoding='latin-1').read()
    m = re.search(key + r'\s+([0-9.]+)', txt)
    return float(m.group(1)) if m else None


table = {}
missing = []
for job in JOBS:
    for weapon in WEAPONS:
        d = '%s/%s/%s' % (BASE, job, weapon)
        if weapon == 'bow':
            # No combo chain: the bow replays attack.msa, and the arrow leaves at
            # AttackingStartTime. The whole motion has to run.
            dur = read(d + '/attack.msa', 'MotionDuration')
            table[(job, weapon)] = [dur] * COMBOS if dur else None
            if not dur:
                missing.append((job, weapon))
            continue
        vals = []
        for i in range(1, COMBOS + 1):
            f = '%s/combo_%02d.msa' % (d, i)
            v = read(f, 'DirectInputTime')
            # A zero input window means this step does not chain: it finishes the
            # sequence, and the client plays it whole. The bell's fourth combo is
            # one, and it is exactly the "last strike cut short" that started this.
            if v == 0.0:
                v = read(f, 'MotionDuration')
            vals.append(v)
        if all(v is None for v in vals):
            table[(job, weapon)] = None
            missing.append((job, weapon))
        else:
            # A class that lacks one combo step falls back to the previous one.
            last = None
            for i in range(COMBOS):
                if vals[i] is None:
                    vals[i] = last
                else:
                    last = vals[i]
            first = next(v for v in vals if v is not None)
            vals = [v if v is not None else first for v in vals]
            table[(job, weapon)] = vals

print('brakujace kombinacje (klasa/bron bez plikow):')
for m in missing:
    print('   ', m[0], m[1])

lines = []
lines.append('#ifndef __INC_METIN2_PLAYERBOT_SWING_TIMING_H__')
lines.append('#define __INC_METIN2_PLAYERBOT_SWING_TIMING_H__')
lines.append('')
lines.append('// How long a swing actually takes, per class, per weapon, per combo step.')
lines.append('//')
lines.append('// Generated from the client motion data the server itself ships in')
lines.append('// share/data/pc/<class>/<weapon>/combo_NN.msa. The number is')
lines.append('// DirectInputTime: the earliest moment the client will accept the next')
lines.append('// combo input. Sending before it means the client cannot chain, so it')
lines.append('// cuts the swing that is playing - which is why bots never finished a')
lines.append('// strike. The old code used a flat 480 ms for everything, and that is too')
lines.append('// early for every weapon in the game: a two-handed sword wants 932 ms and')
lines.append('// a bow a full second, so those were being cut roughly in half.')
lines.append('//')
lines.append('// Bows have no combo chain - they replay attack.msa - so the value there is')
lines.append('// MotionDuration; the arrow itself leaves at 0.846 of it.')
lines.append('//')
lines.append('// A combo step whose DirectInputTime is zero does not chain at all: it')
lines.append('// ends the sequence and the client plays it whole, so the value there is')
lines.append("// MotionDuration. The bell's fourth combo is one of those.")
lines.append('//')
lines.append('// Milliseconds at attack speed 100. Scale by 100/speed for anything else.')
lines.append('// Regenerate with tools/generate_swing_timing.py when the client data')
lines.append('// changes; do not hand-edit.')
lines.append('')
lines.append('namespace')
lines.append('{')
lines.append('\t// [job 0..3][weapon subtype 0..5][combo step 0..3]')
lines.append('\tconst DWORD PLAYERBOT_SWING_MS[4][6][4] = {')
for job in JOBS:
    lines.append('\t\t{ // %s' % job)
    for weapon in WEAPONS:
        vals = table[(job, weapon)]
        if vals is None:
            lines.append('\t\t\t{ 0, 0, 0, 0 },%s// %s: brak danych' % (' ' * 8, weapon))
        else:
            ms = [int(round(v * 1000)) for v in vals]
            lines.append('\t\t\t{ %s },%s// %s' % (
                ', '.join('%4d' % v for v in ms), ' ' * 4, weapon))
    lines.append('\t\t},')
lines.append('\t};')
lines.append('')
lines.append('\t// A class that has no motion for the weapon it is holding - a Shaman with')
lines.append('\t// a one-handed sword, which the client has no animation set for - still')
lines.append('\t// has to swing at some rhythm. One second is the plain MotionDuration')
lines.append('\t// shared by most of the on-foot combos.')
lines.append('\tconst DWORD PLAYERBOT_SWING_MS_FALLBACK = 1000;')
lines.append('}')
lines.append('')
lines.append('#endif')

io.open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
print()
print('zapisano', OUT)
print()
for job in JOBS:
    row = []
    for weapon in WEAPONS:
        v = table[(job, weapon)]
        row.append('%s=%s' % (weapon[:9], 'brak' if v is None else int(round(v[0] * 1000))))
    print('  %-9s %s' % (job, '  '.join(row)))
