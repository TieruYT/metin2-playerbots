#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Every file a Dockerfile COPYs must reach the player, or the build dies at the COPY.

An install put together from an update package has only what
launcher/server-update-files.txt lists.  Being tracked by Git is not the same as
being delivered: panel/bin/apply_rates.sh was tracked and still missing on the
machine that reported it, and game/rates/ was the next one along - "failed to
compute cache key ... /rates: not found", with no way to repair it by
reinstalling, because the reinstall uses the same package.

So: read every COPY out of every build context, and check each source against the
shipping list.  A path may legitimately be absent from the list when something on
the player's machine writes it - start-server.ps1 stages the overlay sources, the
panel app and the quest files - and those prefixes are named here explicitly
rather than guessed, so that a new COPY of something nobody stages is an error
and not a silent omission.

Exit code 1 if anything a build reads would not arrive.
"""
import argparse
import io
import subprocess
import fnmatch
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DOCKER = os.path.join(ROOT, 'linux-port', 'docker')
LIST = os.path.join(ROOT, 'launcher', 'server-update-files.txt')

# Prefixes, relative to a build context, that are written on the player's machine
# rather than shipped: prepare-context.sh stages them during an install and
# start-server.ps1 refreshes them before every start.
STAGED_PREFIXES = (
    'src/',            # the engine tree, from the operator's own r40250 package
    'app/',            # panel/app, staged from files/
    'schema/',         # panel/schema, staged from files/
    'quest/',          # game/quest, staged from files/*.quest
    'initdb.d/dumps/',  # the SQL dumps, from the operator's own package
)


def is_glob(source):
    return '*' in source or '?' in source or '[' in source


def stager_text(paths):
    """Everything the scripts that stage files on a player's machine say.

    Which scripts those are depends on the line, and that is the whole point:
    prepare-context.sh runs on a 1.x install and never on a 2.x one, and
    start-server.ps1 runs on Windows and never on a VPS. seban-panel/VERSION
    was written by both of those and by nothing a Linux 2.x install runs, so
    it was on every machine that had ever built here and on no stranger's -
    which is why a fresh VPS build of 2.0.88 stopped dead at it
    (superjerry925, 20 September) while every check said the file was there.
    """
    out = []
    for rel in paths:
        try:
            out.append(io.open(os.path.join(ROOT, rel.replace('/', os.sep)),
                               encoding='utf-8', errors='replace').read())
        except IOError:
            continue
    return '\n'.join(out)


def only_here(rel, tracked, staged_by):
    """A plain file git has not got and no stager of this line writes.

    Only files: a directory source (panel/app, game/quest) is staged whole and
    its contents are judged on their own. Not "the list does not name it"
    either - the package is built from a clean git export, so a wildcard line
    promising linux-port/docker/seban-panel/* still cannot deliver a file git
    has not got, and that promise is what the old check believed.
    """
    if tracked is None:
        return False
    path = os.path.join(ROOT, rel.replace('/', os.sep))
    if not os.path.isfile(path):
        return False
    if rel in tracked:
        return False
    # The destination as a stager would write it, in either slash.
    tail = rel.split('/', 1)[1] if '/' in rel else rel
    return not (rel in staged_by or tail in staged_by
                or rel.replace('/', chr(92)) in staged_by
                or tail.replace('/', chr(92)) in staged_by)


def tracked_paths():
    """Everything git has, as repo-relative forward-slash paths.

    The check used to ask the filesystem, and the filesystem lies about exactly
    the files that break a stranger's build: seban-panel/VERSION is in
    .gitignore and written by prepare-context.sh and the Windows launcher, so
    it is on every machine that has ever run one of them - including the one
    running this check - and on nobody else's. The gate said "everything is
    covered" while a fresh VPS build stopped dead at it (superjerry925,
    20 September). A source that git does not have and the package does not
    carry reaches the player only if something writes it there.
    """
    try:
        out = subprocess.check_output(['git', 'ls-files'], cwd=ROOT)
    except Exception:
        return None
    return set(out.decode('utf-8', 'replace').replace(os.sep, '/').split('\n'))


def shipping_patterns():
    out = []
    with open(LIST, encoding='utf-8') as handle:
        for line in handle:
            line = line.strip()
            if line and not line.startswith('#'):
                out.append(line)
    return out


def is_shipped(path, patterns):
    for pattern in patterns:
        if fnmatch.fnmatch(path, pattern):
            return pattern
        # "a/b/*" ships the directory, and so everything under it.
        if pattern.endswith('/*') and (path + '/').startswith(pattern[:-1]):
            return pattern
        # And a COPY of a directory needs only something under it to arrive.
        if pattern.startswith(path.rstrip('/') + '/'):
            return pattern
    return None


def copies(dockerfile):
    """Source arguments of every COPY/ADD that reads the build context."""
    out = []
    with open(dockerfile, encoding='utf-8', errors='replace') as handle:
        for raw in handle:
            line = raw.strip()
            if not re.match(r'^(COPY|ADD)\s', line):
                continue
            if '--from=' in line:      # from another stage, not from the context
                continue
            words = [w for w in line.split()[1:] if not w.startswith('--')]
            if len(words) < 2:
                continue
            out.extend(word.strip('"') for word in words[:-1])
    return out


def main():
    # The mt2009 tree has the same shape under another name and its own
    # list: --docker linux-port-mt2009/docker --list launcher/server-update-files.mt2009.txt.
    #
    # For the 2.x line, in full - the shared contexts and the only stager a
    # VPS runs, which is what makes the answer about a stranger's machine
    # rather than about this one:
    #
    #   python tools/check-update-covers-build.py \n    #       --docker linux-port-mt2009/docker \n    #       --list launcher/server-update-files.mt2009.txt \n    #       --context linux-port/docker/seban-panel \n    #       --context linux-port/docker/panel \n    #       --context linux-port/docker/itemshop \n    #       --stager linux-port-mt2009/tools/update.sh
    global DOCKER, LIST
    parser = argparse.ArgumentParser()
    parser.add_argument('--docker', default=os.path.relpath(DOCKER, ROOT))
    parser.add_argument('--list', default=os.path.relpath(LIST, ROOT))
    # The mt2009 compose builds the panels, the ItemShop and the updater from
    # linux-port/docker, outside --docker, and a 2.x install on Linux stages
    # nothing before its build: these contexts are checked with no prefix
    # assumed staged. The panel's COPY schema/ was one - every package from
    # 2.0.70 on lacked it and a VPS build stopped there (DUDU, 18 September).
    parser.add_argument('--context', action='append', default=[])
    # Who writes the staged files on this line. A file neither git nor one of
    # these provides does not exist on a stranger's machine.
    parser.add_argument('--stager', action='append', default=None)
    args = parser.parse_args()
    DOCKER = os.path.join(ROOT, args.docker.replace('/', os.sep))
    LIST = os.path.join(ROOT, args.list.replace('/', os.sep))
    docker_rel = os.path.relpath(DOCKER, ROOT).replace(os.sep, '/')
    patterns = shipping_patterns()
    tracked = tracked_paths()
    stagers = args.stager if args.stager else [
        'linux-port/docker/prepare-context.sh', 'start-server.ps1']
    staged_by = stager_text(stagers)
    problems = []
    checked = 0
    for context in sorted(os.listdir(DOCKER)):
        dockerfile = os.path.join(DOCKER, context, 'Dockerfile')
        if not os.path.isfile(dockerfile):
            continue
        for source in copies(dockerfile):
            checked += 1
            if source.startswith(STAGED_PREFIXES):
                continue
            # A glob is optional by construction: COPY tolerates a pattern that
            # matches nothing as long as another one matches, which is how
            # seban-panel's VERSION stopped being able to break a build.
            if is_glob(source):
                continue
            rel = '%s/%s/%s' % (docker_rel, context, source.rstrip('/'))
            if not os.path.exists(os.path.join(ROOT, rel.replace('/', os.sep))):
                problems.append('%s: COPY %s - nie ma tego w repozytorium' % (context, source))
                continue
            if not is_shipped(rel, patterns):
                problems.append('%s: COPY %s - %s nie jedzie w aktualizacji' % (context, source, rel))
            elif only_here(rel, tracked, staged_by):
                problems.append('%s: COPY %s - %s jest tylko na tej maszynie' % (context, source, rel))

    for extra in args.context:
        extra_rel = extra.replace(os.sep, '/').rstrip('/')
        dockerfile = os.path.join(ROOT, extra_rel.replace('/', os.sep), 'Dockerfile')
        if not os.path.isfile(dockerfile):
            problems.append('%s: nie ma Dockerfile' % extra_rel)
            continue
        for source in copies(dockerfile):
            checked += 1
            if is_glob(source):
                continue
            rel = '%s/%s' % (extra_rel, source.rstrip('/'))
            if not os.path.exists(os.path.join(ROOT, rel.replace('/', os.sep))):
                problems.append('%s: COPY %s - nie ma tego w repozytorium' % (extra_rel, source))
                continue
            if only_here(rel, tracked, staged_by):
                problems.append('%s: COPY %s - %s jest tylko na tej maszynie' % (extra_rel, source, rel))
            if not is_shipped(rel, patterns):
                problems.append('%s: COPY %s - %s nie jedzie w aktualizacji' % (extra_rel, source, rel))

    print('sprawdzone instrukcje COPY: %d' % checked)
    if problems:
        print('')
        print('BUDOWA U GRACZA SIE WYWROCI:')
        for problem in problems:
            print('  ' + problem)
        return 1
    print('kazdy plik czytany przez budowe jedzie w aktualizacji albo jest tworzony na miejscu')
    return 0


if __name__ == '__main__':
    sys.exit(main())
