# -*- coding: utf-8 -*-
"""The nicknames the bots wear, and the SQL that hands them out.

"Pozdrawiam pana botarek7 jest kotem ale brzmi jak bot" - kiciamol, and
jaroszv2 right after: "najgorzej jak biegasz wsrod botow i szukasz tego jednego
nicku a zlewaja sie wszystkie z tymi numerkami". A world of botX7 reads as a
world of bots however well they behave.

The pool is one list, written by a person: Iwakura's, from screenshots of the
Polish servers of 2010-2012 (11 September 2026, "tu jest postaranie"). The
earlier pool - jaksiezabic's list, the first Iwakura list and the names
composed from their words - is gone, and so is the class-and-sex pairing:
Tieru asked for the list as written, for everybody, and for every bot named
from the old pool to be renamed from this one ("te wszystkie stare nicki z
bazy powinny zostac wywalone, ta lista powinna byc aktywna").

Since 2.0.10 the list is dealt out by kingdom, the way Iwakura asked when the
1500-name list arrived ("przypisz po 500 losowych nickow na krolestwo, a
potem, jesli gracz ma wiecej niz 500 botow w danym krolestwie, do obecnych
nickow dopisuj 2, v2, 3 lub v3"): the list is shuffled once - by a seed
derived from its own content, so a regeneration deals the same hand - and
cut into three equal shares, one per kingdom. A kingdom whose cohort is
larger than its share (Chunjo's 1500 seeded identities) continues with its
own names and a "2" or "v2" behind them, then "3"/"v3", then v4 and so on
from the whole list. Before this the pool ran Chunjo first, in list order,
so Chunjo took the first thousand names and Shinsoo and Jinno were named
from the v2/v3 rounds while a third of the list stood idle.

Names are handed out by PID within a kingdom, to the bots that have none from
the pool yet. A bot that has one keeps it, whatever becomes of the list: until
19 September a changed list was a new pool version and every bot named from an
older one was renamed on the next start, which was right the one time the whole
world was to be renamed and wrong for every list after it - Iwakura's list of
19 September adds three hundred names and drops nine, and the operator's word
was to keep the names the bots already wear ("staraj sie nickow juz
istniejacych playerbotow nie podmienic", Tieru). So a list only ever names the
bots that come after it: a new world, a grown cohort, a bot that still wears
its seed name. A name dropped from the list stays on the bot that wears it;
the pool version is still written to common.playerbot_name_history, which is
how to tell which list a name came from.

Nothing about renaming is dangerous, and it is worth writing down why:
CPlayerBotManager::LoadRegisteredBots accepts a character by its account login
(playerbot_NNN), its social id and its player_index row, and never looks at the
name; both panels ask the account too; and generate_seed.py treats a renamed
character as still the character the registry describes as long as
common.playerbot_name_history agrees. So the name is the one part of a bot's
identity nothing depends on.

    python linux-port/overlays/playerbot/tools/generate_bot_names.py
"""
from __future__ import print_function

import hashlib
import io
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OVERLAY = os.path.abspath(os.path.join(HERE, '..'))
REPO = os.path.abspath(os.path.join(OVERLAY, '..', '..', '..'))
SOURCE = os.path.join(OVERLAY, 'data', 'bot_names_iwakura.txt')
OUT_SQL = os.path.join(OVERLAY, 'sql', 'playerbot_names.sql')
MIRROR_SQL = os.path.join(REPO, 'linux-port', 'docker', 'mariadb', 'playerbot',
                          'playerbot_names.sql')

# Names per kingdom: room for the 1500 seeded identities every kingdom holds
# since 2.2.1, and then some. The margin is not for growth: a world named from
# an earlier list wears names that land in every share of this one - the list
# is shuffled by its own content, so a changed list deals a different hand -
# and a name somebody wears is never free (see the plan below). A world named
# in 2.0.10 wears some 650 names of each share of this list, which left 1150
# free for a thousand new Shinsoo or Jinno bots at 1800; 3000 leaves 2350.
# The three kingdoms of playerbot_empire_rules.h, in the order the seed
# numbers them (Chunjo's PIDs come first).
KINGDOMS = (2, 1, 3)
PER_KINGDOM = 3000
# common/length.h: CHARACTER_NAME_MAX_LEN is 24 on both engines.
MAX_LEN = 24
# A name that gets a v2/v3 behind it must still fit.
SUFFIX_MAX_BASE = MAX_LEN - 2

# What must never be in the pool: crude entries, and anything that reads as
# staff. "bot" is allowed on purpose - botmrok, WcaleNieBot and OdpalamBotaPL
# are jokes people made, and nothing identifies a bot by its name any more.
BLOCKED_STEMS = ('ksujebomoge',)
BLOCKED_PREFIXES = ('gm', 'admin', 'system', 'serwer', 'server')


def clean(name):
    """The name as the engine will take it, or None.

    check_name_alphabet (locale_service.cpp) takes letters and digits and
    nothing else, so an underscore is dropped rather than refused ("Miecz_PL"
    is "MieczPL"); a name of digits alone is left out - it reads as a number.
    """
    name = re.sub(r'[^A-Za-z0-9]', '', name.strip())
    if len(name) < 3 or len(name) > MAX_LEN:
        return None
    if not re.search(r'[A-Za-z]', name):
        return None
    low = name.lower()
    if any(stem in low for stem in BLOCKED_STEMS):
        return None
    if any(low.startswith(p) for p in BLOCKED_PREFIXES):
        return None
    return name


def read_list(path):
    out = []
    seen = set()
    dropped = []
    for line in io.open(path, encoding='utf-8'):
        raw = line.strip()
        if not raw or raw.startswith('#'):
            continue
        name = clean(raw)
        if name is None:
            dropped.append(raw)
            continue
        if name.lower() in seen:
            continue
        seen.add(name.lower())
        out.append(name)
    return out, dropped


def build_pool(base, version):
    """(empire, name) rows: each kingdom's random share of the list first,
    then that share again with 2/v2, then 3/v3, then v4.. from the whole
    list, up to PER_KINGDOM per kingdom. One name appears once in the whole
    pool, whichever kingdom it landed in."""
    order = list(base)
    random.Random(version).shuffle(order)
    share = len(order) // len(KINGDOMS)
    shares = {}
    for i, empire in enumerate(KINGDOMS):
        shares[empire] = order[i * share:(i + 1) * share]
    # The remainder (at most two names) goes to Chunjo, the largest cohort.
    shares[KINGDOMS[0]].extend(order[share * len(KINGDOMS):])
    seen = set(n.lower() for n in order)
    rows = []
    for empire in KINGDOMS:
        own = shares[empire]
        out = list(own)
        # "2" and "v2" by turns, then "3" and "v3": what Iwakura asked for.
        rounds = [(own, ('2', 'v2')), (own, ('3', 'v3'))]
        rounds += [(order, ('v%d' % k,)) for k in range(4, 10)]
        for source, suffixes in rounds:
            if len(out) >= PER_KINGDOM:
                break
            for i, name in enumerate(source):
                if len(out) >= PER_KINGDOM:
                    break
                suffix = suffixes[i % len(suffixes)]
                candidate = name[:MAX_LEN - len(suffix)] + suffix
                if candidate.lower() in seen:
                    continue
                seen.add(candidate.lower())
                out.append(candidate)
        rows.extend((empire, name) for name in out)
    return rows


def sql_literal(value):
    return "'" + value.replace('\\', '\\\\').replace("'", "''") + "'"


TEMPLATE = u"""-- Nicknames for the playerbots. GENERATED - edit
-- linux-port/overlays/playerbot/data/bot_names_iwakura.txt and re-run
-- linux-port/overlays/playerbot/tools/generate_bot_names.py.
--
-- Applied by mariadb/playerbot/apply.sh after the seed, guarded by
-- @playerbot_human_names, which apply.sh sets from M2_PLAYERBOT_HUMAN_NAMES:
--
--   1        every bot wears a name from this pool, version @@VERSION@@ (default)
--   0        leave every name exactly as it is
--   restore  put the seed names back and forget the renames
--
-- Only a character whose account login is playerbot_NNN is ever touched: no
-- character of a person is reachable from here. A bot the operator renamed
-- by hand - its name is neither the one this table gave it nor its seed name
-- - is somebody's deliberate choice and keeps it, through every pool version;
-- its name is never dealt to another bot either. Names go out by PID to the
-- bots that have none from the pool yet; a bot that has one keeps it through
-- every later list (common.playerbot_name_history.pool_version says which list
-- it came from), so a new list names a new world, a grown cohort and a bot
-- still on its seed name, and renames nobody.
-- The seed name stays in that table, which is what makes 'restore' possible.
-- Each kingdom deals from its own share of the list (player_index.empire says
-- whose a bot is), so Shinsoo and Jinno get written names and not the v2/v3
-- copies of Chunjo's.

-- A temporary table needs a default database and the client this is fed to has
-- selected none; playerbots_seed.sql opens the same way and for the same
-- reason. Every other table is named in full.
USE player;

CREATE TABLE IF NOT EXISTS common.playerbot_name_history (
    pid          INT UNSIGNED NOT NULL,
    seed_name    VARCHAR(24) NOT NULL,
    human_name   VARCHAR(24) NOT NULL,
    renamed_at   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (pid),
    KEY human_name (human_name)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
-- A world renamed by the first pool has no version column yet.
ALTER TABLE common.playerbot_name_history
    ADD COLUMN IF NOT EXISTS pool_version VARCHAR(16) NOT NULL DEFAULT '';

SET @playerbot_human_names = IFNULL(@playerbot_human_names, '1');
SET @playerbot_pool_version = @@VERSION_LITERAL@@;

-- --------------------------------------------------------------------------
-- restore: the seed name goes back, and only onto a character that still
-- wears the name this table handed it. A bot renamed again by hand afterwards
-- is somebody's deliberate choice and is left alone.
-- --------------------------------------------------------------------------
UPDATE player.player AS p
  JOIN common.playerbot_name_history AS h ON h.pid = p.id
   SET p.name = h.seed_name
 WHERE @playerbot_human_names = 'restore'
   AND BINARY p.name = BINARY h.human_name;

DELETE FROM common.playerbot_name_history
 WHERE @playerbot_human_names = 'restore';

SELECT CONCAT('playerbot names: restored ', ROW_COUNT(), ' seed name(s)')
       AS playerbot_names_note
  FROM DUAL
 WHERE @playerbot_human_names = 'restore';

-- --------------------------------------------------------------------------
-- The pool: @@BASE@@ names as written, shuffled once and cut into a share per
-- kingdom, then each share again with 2/v2 and 3/v3 behind the names and the
-- whole list with v4.. after that, @@PER_KINGDOM@@ names a kingdom (@@COUNT@@
-- in all).
-- --------------------------------------------------------------------------
DROP TEMPORARY TABLE IF EXISTS playerbot_name_pool;
CREATE TEMPORARY TABLE playerbot_name_pool (
    n      INT UNSIGNED NOT NULL PRIMARY KEY,
    empire TINYINT UNSIGNED NOT NULL,
    name   VARCHAR(24) NOT NULL,
    UNIQUE KEY name (name),
    KEY empire (empire, n)
) ENGINE=MEMORY DEFAULT CHARSET=latin1;

@@VALUES@@

-- --------------------------------------------------------------------------
-- Who gets one, and which. Every bot without a name from the pool - no
-- history row at all - waits, numbered by PID within its kingdom; every pool
-- name of that kingdom's share not worn by anybody who is not waiting is free,
-- numbered by its place in the share; the two are joined on kingdom and
-- number. One statement, stable: the first free name goes to the lowest
-- waiting PID.
--
-- "Not worn by anybody who is not waiting" is the whole point of the free
-- side. It used to exclude only people's characters, on the argument that
-- every bot was being renamed at once - which was never true of the other way
-- a bot comes to wait: a cohort seeded later. 2.0.8 switched the kingdoms on
-- and seeded a thousand Shinsoo and Jinno bots into worlds whose Chunjo bots
-- were already named; those thousand were numbered from one and dealt the
-- names of the first thousand Chunjo bots, so a world had two of each. Every
-- settled bot - a history row of any version - keeps the name it wears off
-- the free list, and so does a person's character. A later list lands its
-- names in other kingdoms' shares than the earlier one did, which is why the
-- free side asks who wears a name and not which share it came from.
-- player.name is indexed, not unique, so nothing else would have caught it.
-- --------------------------------------------------------------------------
DROP TEMPORARY TABLE IF EXISTS playerbot_name_plan;
CREATE TEMPORARY TABLE playerbot_name_plan (
    pid        INT UNSIGNED NOT NULL PRIMARY KEY,
    seed_name  VARCHAR(24) NOT NULL,
    human_name VARCHAR(24) NOT NULL,
    UNIQUE KEY human_name (human_name)
) ENGINE=MEMORY DEFAULT CHARSET=latin1;

INSERT INTO playerbot_name_plan (pid, seed_name, human_name)
SELECT waiting.pid, waiting.seed_name, free.name
  FROM (
        SELECT p.id AS pid,
               IFNULL(h.seed_name, p.name) AS seed_name,
               CASE WHEN pi.empire IN (1, 2, 3) THEN pi.empire ELSE 2 END AS empire,
               ROW_NUMBER() OVER (
                   PARTITION BY CASE WHEN pi.empire IN (1, 2, 3) THEN pi.empire ELSE 2 END
                   ORDER BY p.id) AS rn
          FROM player.player AS p
          JOIN account.account AS a ON a.id = p.account_id
          LEFT JOIN player.player_index AS pi ON pi.id = p.account_id
          LEFT JOIN common.playerbot_name_history AS h ON h.pid = p.id
         WHERE LEFT(a.login, 10) = 'playerbot_'
           -- Only a bot the pool has never named. A history row of any
           -- version is a bot that keeps its name - the pool's, or a
           -- person's choice made over it ("bot ADAM dostal ode mnie miecz
           -- +9, rano juz nie bylo bota o tym nicku" - the first version
           -- renamed it with the rest).
           AND h.pid IS NULL
       ) AS waiting
  JOIN (
        SELECT np.name, np.empire,
               ROW_NUMBER() OVER (PARTITION BY np.empire ORDER BY np.n) AS rn
          FROM playerbot_name_pool AS np
         WHERE NOT EXISTS (SELECT 1
                             FROM player.player AS px
                             JOIN account.account AS ax ON ax.id = px.account_id
                             LEFT JOIN common.playerbot_name_history AS hx ON hx.pid = px.id
                            WHERE px.name = np.name
                              AND NOT (LEFT(ax.login, 10) = 'playerbot_' AND hx.pid IS NULL))
       ) AS free
    ON free.empire = waiting.empire AND free.rn = waiting.rn
 WHERE @playerbot_human_names = '1';

INSERT INTO common.playerbot_name_history (pid, seed_name, human_name, pool_version, renamed_at)
SELECT pid, seed_name, human_name, @playerbot_pool_version, NOW() FROM playerbot_name_plan
    ON DUPLICATE KEY UPDATE human_name = VALUES(human_name),
                            pool_version = VALUES(pool_version),
                            renamed_at = VALUES(renamed_at);

UPDATE player.player AS p
  JOIN playerbot_name_plan AS pl ON pl.pid = p.id
   SET p.name = pl.human_name;

-- HAVING, not WHERE: the aggregate returns one row over an empty plan, and a
-- 'restore' run would otherwise report "gave 0" straight after "restored 2500".
SELECT CONCAT('playerbot names: gave ', COUNT(*), ' bot(s) a name from pool @@VERSION@@')
       AS playerbot_names_note
  FROM playerbot_name_plan
HAVING @playerbot_human_names = '1';

-- A cohort larger than its kingdom's share is the one way this runs out; say
-- so rather than leaving an operator to wonder why some bots kept their old
-- names. A hand-renamed bot is not counted: it was left alone on purpose.
SELECT CONCAT('playerbot names: WARNING ', COUNT(*),
              ' bot(s) got no name - a kingdom used up its @@PER_KINGDOM@@ names')
       AS playerbot_names_note
  FROM player.player AS p
  JOIN account.account AS a ON a.id = p.account_id
  LEFT JOIN common.playerbot_name_history AS h ON h.pid = p.id
 WHERE @playerbot_human_names = '1'
   AND LEFT(a.login, 10) = 'playerbot_'
   AND h.pid IS NULL
HAVING COUNT(*) > 0;

-- The hand-renamed, so the operator sees them counted rather than wondering
-- why a bot kept a name that is on no list.
SELECT CONCAT('playerbot names: ', COUNT(*),
              ' bot(s) renamed by hand keep their names')
       AS playerbot_names_note
  FROM player.player AS p
  JOIN account.account AS a ON a.id = p.account_id
  JOIN common.playerbot_name_history AS h ON h.pid = p.id
 WHERE @playerbot_human_names = '1'
   AND LEFT(a.login, 10) = 'playerbot_'
   AND BINARY p.name <> BINARY h.human_name
   AND BINARY p.name <> BINARY h.seed_name
HAVING COUNT(*) > 0;

DROP TEMPORARY TABLE IF EXISTS playerbot_name_plan;
DROP TEMPORARY TABLE IF EXISTS playerbot_name_pool;
"""


def render(base, pool, version):
    rows = ['(%d,%d,%s)' % (i, e, sql_literal(n)) for i, (e, n) in enumerate(pool, start=1)]
    chunks = []
    for start in range(0, len(rows), 100):
        chunks.append('INSERT INTO playerbot_name_pool (n, empire, name) VALUES\n    ' +
                      ',\n    '.join(rows[start:start + 100]) + ';')
    text = TEMPLATE
    text = text.replace('@@VERSION_LITERAL@@', sql_literal(version))
    text = text.replace('@@VERSION@@', version)
    text = text.replace('@@BASE@@', str(len(base)))
    text = text.replace('@@PER_KINGDOM@@', str(PER_KINGDOM))
    text = text.replace('@@COUNT@@', str(len(pool)))
    text = text.replace('@@VALUES@@', '\n'.join(chunks))
    return text


def main():
    base, dropped = read_list(SOURCE)
    if not base:
        print('brak nickow w %s' % SOURCE)
        return 1
    # The dealing scheme is part of the version: the same list dealt by
    # kingdom is a different hand, and every bot named by the old scheme has
    # to be renamed.
    version = hashlib.sha256(('kingdoms\n' + '\n'.join(n.lower() for n in base)).encode('utf-8')).hexdigest()[:12]
    pool = build_pool(base, version)
    text = render(base, pool, version)
    for path in (OUT_SQL, MIRROR_SQL):
        directory = os.path.dirname(path)
        if not os.path.isdir(directory):
            os.makedirs(directory)
        io.open(path, 'w', encoding='utf-8', newline='\n').write(text)
    plain = len(base)
    print('napisano %s: pula %s, %d nickow z listy po %d na krolestwo, %d z dopiskiem (razem %d, po %d na krolestwo); odrzucone: %d'
          % (OUT_SQL, version, plain, plain // len(KINGDOMS), len(pool) - plain, len(pool), PER_KINGDOM, len(dropped)))
    for d in dropped:
        print('  odrzucony: %r' % d)
    print('kopia:   %s' % MIRROR_SQL)
    return 0


if __name__ == '__main__':
    sys.exit(main())
