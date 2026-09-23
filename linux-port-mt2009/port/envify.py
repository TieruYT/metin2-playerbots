# -*- coding: utf-8 -*-
"""The .env.example of the mt2009 stack.

Usage:  python envify.py

Renders linux-port-mt2009/docker/.env.example from linux-port/docker/.env.example:
the same keys with the same defaults (the launcher writes .env from it and
Add-MissingDotEnvKeys in start-server.ps1 tops an older .env up from it), plus
what only this stack reads:

  * M2_APT_MIRROR - the Ubuntu mirror the image build installs packages from,
    for a network where archive.ubuntu.com crawls or times out (this one).

Idempotent: re-run after editing the r40250 original.

It refuses to write a file that would lose a key. Keys this stack alone reads
have been added straight to the rendered file since 2.0.1 - the difficulty,
the second channel, the medal droppers, the world layout - and they sit where
they belong by subject rather than in a block at the end, which is what makes
the file readable. A render that would drop one stops and names it, so the
choice is deliberate: either move that key into EXTRA here, or leave the
rendering alone.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', '.env.example'))
DST = os.path.normpath(os.path.join(HERE, '..', 'docker', '.env.example'))

EXTRA = """
# -----------------------------------------------------------------------------
#  mt2009 build only (rendered by linux-port-mt2009/port/envify.py)
# -----------------------------------------------------------------------------

# The Ubuntu package mirror the image build installs from. Empty means the
# Gdansk mirror (http://ubuntu.task.gda.pl/ubuntu/), which is fast from Poland;
# Ubuntu's own archive.ubuntu.com served a few kilobytes a second here on the
# day 2.0 came out and every first build sat at deps 7/7 for twenty minutes.
# Another country wants a closer mirror, e.g. http://mirrors.edge.kernel.org/ubuntu/
# or http://archive.ubuntu.com/ubuntu/ for the official one.
M2_APT_MIRROR=

# Ile razy szybciej niz w oryginalnej grze idzie doswiadczenie, dropia
# przedmioty i sypie sie yang - w procentach, 100 to dokladnie tak, jak gra
# zostala stworzona. Te trzy liczby sa uzywane TYLKO przy pierwszym starcie
# swiezego swiata (i po wyzerowaniu swiata): migrator zapisuje z nich flagi
# zdarzen, ktore rdzenie czytaja, zanim wpuszcza pierwszego bota. Swiat, ktory
# juz raz je dostal, zmienia sie wylacznie ze strony "Stawki" w panelu - tego
# pliku wtedy nikt nie czyta, wiec zmiana tutaj nic nie da.
M2_RATE_EXP=100
M2_RATE_DROP=100
M2_RATE_YANG=100

# Swiat wstaje, ale boty czekaja przy drzwiach: 1 oznacza, ze po starcie w
# swiecie nie ma ani jednego bota, dopoki operator nie wpusci ich przyciskiem
# w panelu (strona AI) albo w launcherze. Do tego czasu mozna spokojnie
# ustawic stawki, respawny i osobowosci - nic sie nie dzieje. Launcher
# proponuje to przy zerowaniu swiata; domyslnie wylaczone.
M2_PLAYERBOT_START_HELD=0
"""


# Text that is true of the r40250 stack and not of this one. The classic
# panel on this line opens without a passphrase on any address while
# M2_PANEL_LOCAL_ONLY is empty (local_open in admin_panel.py returns before it
# ever looks at the bind address), so the original's "on any other address it
# asks" told a VPS operator the one thing that is not so. Each pair must match
# the rendered text exactly once: a reworded original stops the render rather
# than carrying the old claim across.
LINE_2X_TEXT = [
    ("# Whether the panel asks for the passphrase at all. Empty (the default) lets\n"
     "# the panel decide from the address it is published on: on 127.0.0.1 nobody\n"
     "# but this machine can open it, so it asks nothing; on any other address it\n"
     "# asks. A server behind a proxy binds to 127.0.0.1 and is still public - set\n"
     "# 0 there. 1 forces the open panel.\n",
     "# Whether the panel asks for the passphrase at all. On this line empty (the\n"
     "# default) opens it without one on any address: the 2.x suite is a world for\n"
     "# one player at their own machine. A server anybody else can reach - a VPS,\n"
     "# a forwarded port, a proxy in front - sets 0 here and a M2_PANEL_PASSWORD,\n"
     "# or keeps the panels on 127.0.0.1 (M2_PANEL_BIND_ADDRESS) and opens them\n"
     "# through an SSH tunnel. The advanced panel (7790) has its own switch in its\n"
     "# settings and starts without a passphrase too. 1 forces the open panel.\n"
     "# PL: na linii 2.x panel domyslnie NIE pyta o haslo, na zadnym adresie (to\n"
     "#      swiat dla jednego gracza). Na VPS albo przy przekierowanym porcie\n"
     "#      ustaw tu 0 i haslo w M2_PANEL_PASSWORD - albo trzymaj panele na\n"
     "#      127.0.0.1 (M2_PANEL_BIND_ADDRESS) i otwieraj je przez tunel SSH.\n"
     "#      Panel zaawansowany (7790) tez startuje bez hasla; wlacza sie je w\n"
     "#      jego ustawieniach. 1 = zawsze bez hasla.\n"),
]


def keys_of(text):
    """The keys a .env file sets, in the order they appear."""
    found = []
    for line in text.splitlines():
        if '=' in line and line[:1].isalpha() and line[:1].isupper():
            found.append(line.split('=', 1)[0])
    return found


def main():
    s = io.open(SRC, encoding='utf-8', newline='').read()
    assert '\r' not in s, 'expected LF line endings'
    assert 'M2_APT_MIRROR' not in s, 'the r40250 example carries M2_APT_MIRROR now; drop it from EXTRA'
    head = ('# Rendered for the mt2009 stack by linux-port-mt2009/port/envify.py from\n'
            '# linux-port/docker/.env.example, plus the keys only this stack reads,\n'
            '# which are edited here and kept by the render (see envify.py).\n')
    out = head + s.rstrip('\n') + '\n' + EXTRA
    for old, new in LINE_2X_TEXT:
        assert out.count(old) == 1, 'the r40250 example no longer reads as LINE_2X_TEXT expects:\n' + old
        out = out.replace(old, new)
    # Nothing is written while the render would lose a key: this file is what
    # a new install's .env is written from and what Add-MissingDotEnvKeys tops
    # an older one up from, so a key dropped here is a setting that silently
    # stops reaching anybody.
    if os.path.exists(DST):
        have = keys_of(io.open(DST, encoding='utf-8', newline='').read())
        rendered = set(keys_of(out))
        lost = [k for k in have if k not in rendered]
        if lost:
            raise SystemExit(
                'envify: refusing to write - the render would drop %d key(s) that\n'
                '%s holds:\n  %s\n'
                'Move each into EXTRA in this script (or into the r40250 example),\n'
                'then run it again.' % (len(lost), os.path.relpath(DST), ', '.join(lost)))
    io.open(DST, 'w', encoding='utf-8', newline='').write(out)
    print('envify: %s (%d keys)' % (os.path.relpath(DST), len(keys_of(out))))


if __name__ == '__main__':
    main()
