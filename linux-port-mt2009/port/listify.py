# -*- coding: utf-8 -*-
r"""The server update list of the mt2009 stack.

Usage:  python listify.py [--pristine <path to the package's Server Source\Server>]

Writes launcher/server-update-files.mt2009.txt: what tools/New-M2UpdatePackage.ps1
puts into a server update for a player whose tree is the mt2009 one. Paths are
repository paths; the packager is run with
    -PathMap @{ 'linux-port-mt2009/docker/docker-compose.deploy.yml' = 'linux-port/docker/docker-compose.yml';
                'linux-port-mt2009/VERSION' = 'VERSION';
                'linux-port-mt2009/PACZKA_INFO.txt' = 'PACZKA_INFO.txt';
                'linux-port-mt2009/' = 'linux-port/' }
(tools/New-M2DeployTree.ps1 does exactly that)
so on the player's machine the tree is linux-port, as the launcher expects.

Three kinds of entry:
  * what r40250's list ships that is engine-agnostic - the panels, the
    ItemShop, the updater, the overlay's playerbot sources, the launcher;
  * the mt2009 tree's own files - compose, .env.example, ENGINE, the database
    scripts, the game image's Dockerfile, scripts, quests and share additions;
  * every engine file the port changed or added, measured against the
    pristine package rather than listed by hand: the port scripts
    (linuxify.py, playerbotify.py) edit the staged tree, and a file they
    touch that does not travel is a file the player compiles unpatched.
    The engine tree itself (the rest of it, the externals, the runtime share)
    never ships in an update: it is the operator's package, staged once by
    the installer.

Idempotent.
"""
import argparse
import hashlib
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
STAGED = os.path.join(ROOT, 'linux-port-mt2009', 'docker', 'game', 'src', 'server')
OUT = os.path.join(ROOT, 'launcher', 'server-update-files.mt2009.txt')
DEFAULT_PRISTINE = r'C:\Users\dawio\Downloads\Metin2 Singleplayer\Server Source\Server'

SHARED = """\
# Files copied into a server update ZIP for the mt2009 stack. Rendered by
# linux-port-mt2009/port/listify.py - edit that, not this. Keep .env and the
# databases out. A line may use a wildcard in its last segment.
#
# Repository paths; New-M2UpdatePackage.ps1 is run with -PathMap so that
# linux-port-mt2009/ is published as linux-port/ (the player's tree has one
# server tree, under the name every launcher path uses) and
# docker-compose.deploy.yml as docker-compose.yml.

# ---- engine-agnostic, from the r40250 tree (one copy, shared) ---------------
files/admin_panel.py
files/items.json
files/favicon.png
files/web_admin_schema.sql
files/web_admin.quest
files/high_risk.quest
files/speed_boost.quest
files/static/*
linux-port/docker/seban-panel/*
linux-port/docker/itemshop/*
linux-port/docker/panel/app/admin_panel.py
# The panel's Dockerfile COPYs schema/ too, and nothing on a Linux install
# stages it - a build from the package alone stopped at "/schema: not found"
# (DUDU's VPS, 18 September). update.sh stages the rest (stage_panel_context).
linux-port/docker/panel/schema/web_admin_schema.sql
linux-port/docker/panel/bin/*
linux-port/docker/panel/Dockerfile
linux-port/docker/panel/.dockerignore
linux-port/docker/updater/Dockerfile
linux-port/docker/updater/bin/*
# The playerbot sources: the overlay is the source of truth, the staged copy
# under the engine tree (listed below with the engine files) is what compiles.
linux-port/overlays/playerbot/src/game/src/playerbot_*

# ---- the mt2009 tree ---------------------------------------------------------
linux-port-mt2009/docker/docker-compose.deploy.yml
linux-port-mt2009/docker/.env.example
linux-port-mt2009/docker/ENGINE
linux-port-mt2009/docker/mariadb/conf.d/99-metin2.cnf
linux-port-mt2009/docker/mariadb/initdb.d/10-import-dumps.sh
linux-port-mt2009/docker/mariadb/initdb.d/20-log-schema.sql
linux-port-mt2009/docker/mariadb/playerbot/apply.sh
linux-port-mt2009/docker/mariadb/playerbot/itemshop_schema.sql
linux-port-mt2009/docker/mariadb/playerbot/log_schema.sql
linux-port-mt2009/docker/mariadb/playerbot/playerbots_seed.sql
linux-port-mt2009/docker/mariadb/playerbot/playerbot_names.sql
linux-port-mt2009/docker/mariadb/playerbot/gm_characters.sql
linux-port-mt2009/docker/game/Dockerfile
linux-port-mt2009/docker/game/.dockerignore
linux-port-mt2009/docker/game/build-deps-mt2009.sh
linux-port-mt2009/docker/game/bin/*
linux-port-mt2009/docker/game/quest/*
linux-port-mt2009/docker/game/mob_drop_item.m3.append.txt
linux-port-mt2009/docker/game/special_item_group.moonlight.txt
linux-port-mt2009/docker/game/special_item_group.starter.txt

# ---- the launcher and the package's own bookkeeping --------------------------
# The line's own version and package note, published under the root names.
linux-port-mt2009/PACZKA_INFO.txt
CHANGELOG.md
linux-port-mt2009/VERSION
start-server.ps1
start.bat
stop.bat
Metin2-Launcher.ps1
Metin2-Launcher.bat
Metin2-Launcher-GUI.ps1
Metin2-Launcher-GUI.bat
Metin2-Launcher-GUI.Layout.ps1
Metin2-Launcher-GUI.Background.png
launcher/Metin2Launcher.psm1
launcher/Metin2Launcher.Diagnostics.psm1
launcher/Metin2Launcher.Coop.psm1
launcher/Metin2Launcher.Vps.psm1
launcher/launcher.config.example.json
launcher/update-manifest.example.json
launcher/server-update-files.mt2009.txt
launcher/client-update-files.example.txt
docs/LAUNCHER.md
tools/New-M2UpdatePackage.ps1
linux-port-mt2009/tools/update.sh
linux-port-mt2009/tools/vps-install.sh

# ---- engine files the port changed or added (measured against the package) --
"""

SKIP_SUFFIXES = ('.o', '.a', '.vcxproj', '.vcxproj.filters', '.sln', '.d')


def digest(path):
    with open(path, 'rb') as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def is_binary_artifact(path):
    # A core linked against the staged tree leaves its executable in it
    # (game/game_r41023, 78 MB, from a local compile on 13 September); it is not
    # in the package, so it looked like a file the port added.
    with open(path, 'rb') as handle:
        return handle.read(4) == b'\x7fELF'


def changed_engine_files(pristine):
    if not os.path.isdir(pristine):
        raise SystemExit('listify: no pristine engine tree at %s (pass --pristine)' % pristine)
    out = []
    playerbot_glob_written = False
    for root, dirs, files in os.walk(STAGED):
        dirs[:] = sorted(d for d in dirs if d not in ('.obj', 'OBJDIR'))
        for name in sorted(files):
            if name.endswith(SKIP_SUFFIXES):
                continue
            staged = os.path.join(root, name)
            if is_binary_artifact(staged):
                continue
            rel = os.path.relpath(staged, STAGED).replace(os.sep, '/')
            if rel.startswith('game/src/playerbot_'):
                # The Playerbot sources' build-context copies are one glob,
                # like the overlay's own line above and the r40250 list: the
                # packager pairs every overlay playerbot_* with a copy here,
                # and enumerating the thirty-six names meant the first new
                # file (playerbot_offline_policy.h, 2.0.26) failed that check.
                if not playerbot_glob_written:
                    out.append('# Every Playerbot source has a build-context copy here, discovered like the\n'
                               '# overlay above rather than listed: the r40250 list did this already (its\n'
                               '# line 65) and this one enumerated thirty-six names, so the first new file\n'
                               '# (playerbot_offline_policy.h, 2.0.26) failed the packager\'s pairing check.\n'
                               'linux-port-mt2009/docker/game/src/server/game/src/playerbot_*')
                    playerbot_glob_written = True
                continue
            original = os.path.join(pristine, rel.replace('/', os.sep))
            if not os.path.isfile(original) or digest(original) != digest(staged):
                out.append('linux-port-mt2009/docker/game/src/server/' + rel)
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pristine', default=DEFAULT_PRISTINE)
    args = parser.parse_args()
    engine = changed_engine_files(args.pristine)
    text = SHARED + '\n'.join(engine) + '\n'
    io.open(OUT, 'w', encoding='utf-8', newline='\n').write(text)
    print('listify: %s (%d engine files changed or added)' % (os.path.relpath(OUT, ROOT), len(engine)))
    for line in engine:
        print('  ' + line[len('linux-port-mt2009/docker/game/src/server/'):])


if __name__ == '__main__':
    main()
