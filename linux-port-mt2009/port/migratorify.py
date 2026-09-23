# -*- coding: utf-8 -*-
"""The playerbot migrator (mariadb/playerbot/apply.sh) for the mt2009 world.

Usage:  python migratorify.py

Copies linux-port/docker/mariadb/playerbot/{apply.sh,itemshop_schema.sql} into
linux-port-mt2009/docker/mariadb/playerbot/ and rewrites what the schema and
the map layout change:

  * the readiness probe: mt2009's log schema has hack_log, not speed_hack;
    player.item_proto is a view over world.item_proto (initdb creates it);
  * the list of maps a bot may be parked on: this stack's m2-render-config
    hosts the package's forty-six maps plus the guild villages and the high
    maps, so the list is that layout, not r40250's.

Idempotent: re-run after editing the r40250 original.
"""
import io
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', 'mariadb', 'playerbot'))
DST = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'playerbot'))

# Everything m2-render-config's MAPS_first / MAPS_game1 / MAPS_game2 host.
HOSTED = ('1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,\n'
          '                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,\n'
          '                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351')

# What the package's player dump (initdb.d/dumps/player.sql) carries of the
# server it was taken from: its guild lands as (land_id, guild_id) and the
# buildings on them as (id, land_id, vnum) - and not one of those guilds.
PACKAGE_GUILD_LANDS = (
    (2, 408), (8, 78), (9, 108), (10, 69), (14, 3), (15, 395), (16, 2), (17, 52), (18, 18),
    (108, 5), (109, 6), (115, 92), (116, 93), (117, 20), (118, 13),
    (201, 212), (204, 712), (205, 57), (206, 9), (207, 58), (208, 25), (212, 19), (213, 14),
    (214, 15), (215, 344), (216, 47), (217, 33), (218, 7))
PACKAGE_GUILD_OBJECTS = (
    (1, 14, 14100), (2, 214, 14120), (3, 214, 14014), (4, 14, 14013), (5, 215, 14120),
    (6, 215, 14013), (7, 218, 14120), (8, 218, 14043), (9, 16, 14100), (10, 16, 14014),
    (11, 16, 14043), (12, 108, 14100), (13, 108, 14014), (14, 214, 14050), (15, 14, 14051),
    (16, 215, 14051), (17, 217, 14100), (18, 218, 14014), (19, 217, 14015), (20, 109, 14100),
    (21, 109, 14051), (22, 17, 14100), (23, 17, 14015), (24, 207, 14110), (25, 207, 14014),
    (26, 15, 14100), (27, 15, 14015), (28, 217, 14051), (29, 18, 14110), (30, 18, 14055),
    (31, 115, 14120), (32, 115, 14014), (33, 108, 14043), (34, 18, 14015), (35, 116, 14120),
    (36, 116, 14013), (37, 216, 14110), (38, 109, 14015), (39, 8, 14120), (40, 216, 14013),
    (41, 212, 14100), (42, 117, 14110), (43, 216, 14055), (44, 117, 14055), (45, 117, 14014),
    (46, 205, 14120), (47, 205, 14055), (48, 15, 14055), (49, 216, 14200), (50, 216, 14300),
    (51, 216, 14300), (52, 205, 14015), (53, 212, 14015), (54, 206, 14100), (55, 206, 14015),
    (56, 8, 14015), (57, 115, 14050), (58, 212, 14055), (59, 207, 14055), (60, 8, 14055),
    (61, 208, 14110), (62, 201, 14100))

# Three steps that were written into the rendered apply.sh by hand before this
# renderer knew them - the fishing pass and the teleport ring's flag, the bot
# guilds' tier table with the second channel's pins (6629944), and the world's
# difficulty (43ca602) - so a render dropped all three without a word (found
# rendering 2.0.81). They live here now, and a render reproduces the file.
FISHING_PASS_AND_RING = (
    '# And the pass the rod needs. Karta Wedkarska (27620), which CHARACTER::fishing()\n'
    "# wants worn, is sold in one place, the Fisherman's special shop (9009, opened\n"
    '# by fishing_pass_shop.quest), and the package asks level fifty for it - so a\n'
    '# player of thirty to forty-nine could wear the rod the line above allows and\n'
    '# never fish (Tieru, 17 September). The db core reads shop_special_proto at\n'
    '# boot, so this is live on the next start; idempotent, and only a fifty moves.\n'
    'db -e "UPDATE world.shop_special_proto SET limitvalue0 = 30 WHERE item_vnum = 27620 AND limittype0 = \'LEVEL\' AND limitvalue0 = 50; UPDATE world.shop_special_proto SET limitvalue1 = 30 WHERE item_vnum = 27620 AND limittype1 = \'LEVEL\' AND limitvalue1 = 50;"\n'
    '# Pierscien Teleportacji (70058) carries ITEM_FLAG_APPLICABLE (8192) in this\n'
    '# package, and under ENABLE_QUEST_DND_EVENT that flag makes UseItemEx treat an\n'
    '# ITEM_QUEST as "drop it onto another item": a plain use finds no target cell\n'
    '# and returns before the quest is asked, so teleport_ring.quest never ran for\n'
    '# a player ("caly czas nie dziala pierscien teleportu", Tieru, 16 September).\n'
    '# The ring is dragged onto nothing; the flag comes off. Idempotent.\n'
    'db -e "UPDATE world.item_proto SET flag = flag & ~8192 WHERE vnum = 70058 AND (flag & 8192) <> 0;"\n'
)

GUILD_TIERS_AND_CHANNEL_PINS = (
    "# The bot guilds' tiers (playerbot_guild.h): a guild outlives every core\n"
    '# restart, so its tier and kingdom live here; the core reads the table once\n'
    '# and writes a row when it founds or adopts a guild.\n'
    'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_guild (guild_id INT UNSIGNED NOT NULL PRIMARY KEY, tier TINYINT UNSIGNED NOT NULL DEFAULT 3, empire TINYINT UNSIGNED NOT NULL DEFAULT 0, founder_pid INT UNSIGNED NOT NULL DEFAULT 0, founded_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
    '# And when each last went to war (playerbot_guild_war.h), in unix seconds, so\n'
    "# the pick that keeps a kingdom's last pair out of its next war survives the\n"
    '# restart every update makes.\n'
    'db -e "ALTER TABLE player.playerbot_guild ADD COLUMN IF NOT EXISTS last_war_at INT UNSIGNED NOT NULL DEFAULT 0;" \\\n'
    '    || echo "playerbot-migrate: could not add last_war_at to player.playerbot_guild" >&2\n'
    "# The second channel's pins (playerbot_channel_rules.h): every bot that has\n"
    '# ever kept an offline shop lives on the first channel for good, because the\n'
    "# shops are the first channel's. The table only grows - each core adds the\n"
    '# owners it sees before it reads it - and this adds them before any core has\n'
    '# started, so the start that switches the second channel on finds every keeper\n'
    '# of the last session already pinned. Written whatever the switch says.\n'
    'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_channel_pin (pid INT UNSIGNED NOT NULL PRIMARY KEY, pinned_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
    'db -e "INSERT IGNORE INTO player.playerbot_channel_pin (pid, pinned_at) SELECT owner, NOW() FROM player.ikashop_offlineshop;" 2>/dev/null \\\n'
    '    || echo "playerbot-migrate: could not pin the shop keepers to the first channel" >&2\n'
)

WORLD_RATES = r"""
# The rates of a world that has never had any, before the cores start. On this
# engine a rate is not a rewritten table but three event flags (player.quest,
# dwPID 0) that CQuestManager::SetEventFlag maps onto CHARACTER_MANAGER's
# multipliers, and until somebody presses "Zastosuj" in the panel those rows do
# not exist - so a fresh world ran at 100% whatever the panel's own table said.
# It said 650% experience, seeded into web_admin_rates by the panel's schema
# for a test cycle long ago, and that number reached every player as a promise
# the game never kept: the panel showed it, the bots levelled at 100%, and the
# first press of the button - even without touching a field - was what made it
# real (NerrVoVy and Tieru, 20 September).
#
# So the numbers the launcher asked for are written here, into both places at
# once, and only while the flags are absent: a world that has been set from the
# panel is never touched again, whatever this file says. That is also why the
# panel's schema no longer seeds the table.
rate_ok() {
    # A newline, because awk reads no record from an empty input and
    # the substitution would then be empty - not a number, so the SQL
    # below would be a syntax error rather than a default.
    printf '%s\n' "$1" | tr -d ' \r' | awk -v d="$2" '{ v = $1 + 0; if (v < 1 || v > 10000) v = d; printf "%d", v }'
}
r_exp=$(rate_ok "${M2_RATE_EXP:-100}" 100)
r_drop=$(rate_ok "${M2_RATE_DROP:-100}" 100)
r_yang=$(rate_ok "${M2_RATE_YANG:-100}" 100)
db -e "CREATE TABLE IF NOT EXISTS player.web_admin_rates (
        name VARCHAR(24) PRIMARY KEY, value INT NOT NULL DEFAULT 100);" >/dev/null 2>&1 \
    || echo "[playerbot-migrate] WARNING: could not make player.web_admin_rates" >&2
rates_set=$(db -e "SELECT COUNT(*) FROM player.quest WHERE dwPID = 0 AND szName = 'mob_exp';" 2>/dev/null || echo x)
if [ "$rates_set" = "x" ]; then
    echo "[playerbot-migrate] WARNING: could not read the rate flags; leaving them alone" >&2
elif [ "$rates_set" = "0" ]; then
    if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
            (0, 'mob_exp', '', $r_exp),   (0, 'mob_exp_buyer', '', $r_exp),
            (0, 'mob_item', '', $r_drop), (0, 'mob_item_buyer', '', $r_drop),
            (0, 'mob_gold', '', $r_yang), (0, 'mob_gold_buyer', '', $r_yang);
        REPLACE INTO player.web_admin_rates (name, value) VALUES
            ('exp', $r_exp), ('drop', $r_drop), ('yang', $r_yang);"; then
        echo "[playerbot-migrate] fresh world: experience ${r_exp}%, item drops ${r_drop}%, yang ${r_yang}%"
    else
        echo "[playerbot-migrate] WARNING: could not write the fresh world's rates" >&2
    fi
    # And whether that world's bots wait at the door. The core reads this file
    # on the weights clock and, the first time it is asked, before its own
    # first tick - the bootstrap spawns a cohort before any tick runs, so a
    # file written afterwards would hold a door the crowd had already walked
    # through. Written only for a fresh world, because on any other one it is
    # the panel's button that owns it.
    if [ -d /opt/m2spool ]; then
        if [ "$(printf '%s' "${M2_PLAYERBOT_START_HELD:-0}" | tr -d ' \r')" = "1" ]; then
            printf '1\n' > /opt/m2spool/playerbot_hold 2>/dev/null \
                && echo "[playerbot-migrate] the bots will wait at the door until you let them in" \
                || echo "[playerbot-migrate] WARNING: could not hold the bots (/opt/m2spool not writable)" >&2
        else
            printf '0\n' > /opt/m2spool/playerbot_hold 2>/dev/null || true
        fi
        chmod 0664 /opt/m2spool/playerbot_hold 2>/dev/null || true
    fi
fi
"""

WORLD_DIFFICULTY = (
    '\n'
    "# The world's difficulty, as event flags in seconds (player.quest, dwPID 0 -\n"
    "# what the db core loads at boot and pushes to every game core, the package's\n"
    '# own idiom for a world-wide switch). quest/m2_difficulty.lua reads them: the\n'
    "# Biologist's wait between two hand-ins and the stable keeper's four waits\n"
    '# (the pony, each Horse Book, the medal trainings of 1-10 and of 11-19). The\n'
    "# presets scale the package's own numbers - hard is what it shipped with,\n"
    '# medium a third of it, easy none (what 2.0.55 and 2.0.56 gave everybody) -\n'
    "# and custom takes the hour counts from .env, the horse's for every wait.\n"
    "# The wait between two skill books is the engine's (m2_book_wait, playerbotify\n"
    "# apply_book_wait) and the bots' own (m2_bot_book_wait), the package's 21 hours\n"
    '# on hard (drip9660, 23 September).\n'
    '# Written before the seed, which may leave early on a foreign cohort.\n'
    '#\n'
    "# The classic panel's difficulty card sets the same flags live, so .env is\n"
    '# applied only when it changed since the last start (m2_difficulty_env holds\n'
    '# what it said): a change made in the panel survives a restart until the\n'
    "# launcher's difficulty is changed, and the one changed last is the one kept.\n"
    'difficulty=$(printf \'%s\' "${M2_DIFFICULTY:-easy}" | tr \'A-Z\' \'a-z\' | tr -d \' \\r\')\n'
    'hours_to_seconds() {\n'
    '    printf \'%s\\n\' "$1" | tr -d \' \\r\' | awk \'{ h = $1 + 0; if (h < 0) h = 0; if (h > 8760) h = 8760; printf "%d", h * 3600 }\'\n'
    '}\n'
    'case "$difficulty" in\n'
    '    medium) dlevel=1; bio=28800; hbuy=14400; hup=14400; htr=21600; htr2=25200; book=25200; botbook=25200 ;;\n'
    '    hard)   dlevel=2; bio=86400; hbuy=43200; hup=43200; htr=64800; htr2=75600; book=75600; botbook=75600 ;;\n'
    '    custom)\n'
    '        dlevel=3\n'
    '        bio=$(hours_to_seconds "${M2_BIOLOGIST_WAIT_HOURS:-0}")\n'
    '        hbuy=$(hours_to_seconds "${M2_HORSE_WAIT_HOURS:-0}")\n'
    '        hup=$hbuy; htr=$hbuy; htr2=$hbuy\n'
    '        book=$(hours_to_seconds "${M2_BOOK_WAIT_HOURS:-0}")\n'
    '        botbook=$(hours_to_seconds "${M2_BOT_BOOK_WAIT_HOURS:-0}") ;;\n'
    '    *)      difficulty=easy; dlevel=0; bio=0; hbuy=0; hup=0; htr=0; htr2=0; book=0; botbook=0 ;;\n'
    'esac\n'
    'dsig=$(printf \'%s|%s|%s|%s|%s|%s|%s|%s|%s\' "$difficulty" "$bio" "$hbuy" "$hup" "$htr" "$htr2" "$book" "$botbook" 1 | cksum | awk \'{ print $1 % 2000000000 }\')\n'
    'dprev=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = \'m2_difficulty_env\' LIMIT 1" 2>/dev/null | tr -d \' \\r\')\n'
    'if [ -n "$dprev" ] && [ "$dprev" = "$dsig" ]; then\n'
    '    echo "[playerbot-migrate] difficulty: .env unchanged since the last start - the flags stay as the panel or the last start left them"\n'
    'elif db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES\n'
    "        (0, 'm2_difficulty', '', $dlevel),\n"
    "        (0, 'm2_biologist_wait', '', $bio),\n"
    "        (0, 'm2_horse_buy_wait', '', $hbuy),\n"
    "        (0, 'm2_horse_upgrade_wait', '', $hup),\n"
    "        (0, 'm2_horse_train_wait', '', $htr),\n"
    "        (0, 'm2_horse_train2_wait', '', $htr2),\n"
    "        (0, 'm2_book_wait', '', $book),\n"
    "        (0, 'm2_bot_book_wait', '', $botbook),\n"
    '        (0, \'m2_difficulty_env\', \'\', $dsig);"; then\n'
    '    echo "[playerbot-migrate] difficulty: $difficulty (Biologist wait ${bio}s, horse: buy ${hbuy}s upgrade ${hup}s train ${htr}s/${htr2}s, books: players ${book}s bots ${botbook}s)"\n'
    'else\n'
    '    echo "[playerbot-migrate] WARNING: could not write the difficulty flags; the quests keep the last ones" >&2\n'
    'fi\n'
)


STARTER_CHEST = (
    '\n'
    "# Whether a player's new character gets the apprentice chest at its first\n"
    "# login (starter_chest.quest reads m2_starter_chest_off). Asked with the\n"
    "# rates when a world is made (seban latino's idea, 22 September); on unless\n"
    "# .env says M2_STARTER_CHEST=0. An event flag like the difficulty, so a\n"
    "# change reaches the quests at the next start.\n"
    'starter=$(printf \'%s\' "${M2_STARTER_CHEST:-1}" | tr \'A-Z\' \'a-z\' | tr -d \' \\r\')\n'
    'case "$starter" in\n'
    '    0|off|no|false) starter_off=1 ;;\n'
    '    *)              starter_off=0 ;;\n'
    'esac\n'
    'if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES\n'
    "        (0, 'm2_starter_chest_off', '', $starter_off);\"; then\n"
    '    echo "[playerbot-migrate] apprentice chest for new characters: $([ "$starter_off" = 1 ] && echo off || echo on)"\n'
    'else\n'
    '    echo "[playerbot-migrate] WARNING: could not write the apprentice chest flag; the quest keeps the last one" >&2\n'
    'fi\n'
)


def sql_rows(rows, per_line=8):
    """A tuple of tuples as the SQL list of row constructors, a few to a line."""
    parts = ['(' + ', '.join(str(v) for v in r) + ')' for r in rows]
    lines = [', '.join(parts[i:i + per_line]) for i in range(0, len(parts), per_line)]
    return (',\n' + ' ' * 12).join(lines)


def guild_lands_block():
    """The migrator's step that takes the package's guild lands off the world."""
    return (
        '# The package\'s player dump carries the guild lands and buildings of the\n'
        '# server it was taken from - %d player.guild_land rows and %d player.object\n'
        '# rows - and none of the guilds they belong to. The engine stands its land\n'
        '# agent (NPC 20040) only on a land nobody owns (building::CManager, at boot),\n'
        '# so those lands could never be bought and their buildings stood on ground\n'
        '# nobody held, while a bot guild founded later under one of those numbers\n'
        '# (2, 3, 5, ...) held a land and buildings it never paid for ("stoja juz\n'
        '# budynki, pomimo ze teren nie jest zajety", Mat, 19 September; NerrVoVy\n'
        '# cleared his by hand). Once, and the dump\'s own rows exactly: a land a\n'
        '# player\'s guild has bought and the buildings it put up since (ids past the\n'
        '# dump\'s last) are left alone. Before the game container starts, because the\n'
        '# db core reads both at boot.\n'
        'lands_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = \'package_guild_lands_2081\';" 2>/dev/null || echo x)\n'
        'if [ "$lands_done" = "0" ]; then\n'
        '    if lands_out=$(db -e "\n'
        '        START TRANSACTION;\n'
        '        DELETE FROM player.object WHERE (id, land_id, vnum) IN (\n'
        '            %s);\n'
        '        SELECT ROW_COUNT();\n'
        '        DELETE FROM player.guild_land WHERE (land_id, guild_id) IN (\n'
        '            %s);\n'
        '        SELECT ROW_COUNT();\n'
        '        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES (\'package_guild_lands_2081\', NOW());\n'
        '        COMMIT;\n'
        '    "); then\n'
        '        lands_objects=$(printf \'%%s\\n\' "$lands_out" | awk \'NR == 1\')\n'
        '        lands_rows=$(printf \'%%s\\n\' "$lands_out" | awk \'NR == 2\')\n'
        '        echo "[playerbot-migrate] the package\'s guild lands cleared: ${lands_rows:-0} land(s), ${lands_objects:-0} building(s)"\n'
        '    else\n'
        '        echo "[playerbot-migrate] WARNING: could not clear the package\'s guild lands" >&2\n'
        '    fi\n'
        'fi\n'
    ) % (len(PACKAGE_GUILD_LANDS), len(PACKAGE_GUILD_OBJECTS),
         sql_rows(PACKAGE_GUILD_OBJECTS), sql_rows(PACKAGE_GUILD_LANDS))


def main():
    os.makedirs(DST, exist_ok=True)
    s = io.open(os.path.join(SRC, 'apply.sh'), encoding='utf-8', newline='').read()
    assert '\r' not in s

    n = s.count("table_name='speed_hack'")
    assert n == 1, n
    s = s.replace("table_name='speed_hack'", "table_name='hack_log'")

    pat = re.compile(r"\(1, 3, 4, 5, 21, 23, 24, 25, 41, 43, 44, 45,\s+108, 109, 61, 63, 64, 104, 65, 71\)")
    s, n = pat.subn('(' + HOSTED + ')', s)
    assert n == 2, n

    s = s.replace('echo "[playerbot-migrate] waiting for the complete r40250 schema"',
                  'echo "[playerbot-migrate] waiting for the complete mt2009 schema"')

    # A world initialised before initdb widened account.social_id gets the
    # same ALTER here, once; see 10-import-dumps.sh for why.
    anchor = 'itemshop_schema=/opt/playerbot/itemshop_schema.sql\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor,
                  'social_len=$(db -e "\n'
                  '    SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.columns\n'
                  '     WHERE table_schema=\'account\' AND table_name=\'account\' AND column_name=\'social_id\';\n'
                  '")\n'
                  'if [ -n "$social_len" ] && [ "$social_len" -lt 18 ] 2>/dev/null; then\n'
                  '    echo "[playerbot-migrate] widening account.social_id from $social_len to 18 characters"\n'
                  '    db -e "ALTER TABLE account.account MODIFY social_id VARCHAR(18) NOT NULL DEFAULT \'\';"\n'
                  'fi\n'
                  '# The ItemShop reads mileage and jackpot off the account; this schema has\n'
                  '# cash alone. IF NOT EXISTS keeps it a no-op after the first time.\n'
                  'db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS mileage INT NOT NULL DEFAULT 0;"\n'
                  'db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS jackpot INT NOT NULL DEFAULT 0;"\n'
                  '# Fishing from thirty, which is what the wiki says and what the operator\n'
                  '# asked for. This line shipped fifty in three places and moving two was not\n'
                  '# enough: CHARACTER::fishing() (playerbotify.py lowers it), the AI gate, and\n'
                  '# the rod LIMIT_LEVEL - the one that refuses the equip, so a bot of thirty\n'
                  '# could neither wear a rod nor be drawn as an angler. item_proto is read out\n'
                  '# of world.item_proto here (PROTO_FROM_DB = 1), which is why this sticks;\n'
                  '# idempotent, and it touches only rods still carrying the old fifty.\n'
                  'db -e "UPDATE world.item_proto SET limitvalue0 = 30 WHERE type = 13 AND limittype0 = 1 AND limitvalue0 = 50;"\n'
                  '# Maska Sabaha left the world with the Hwang curse (playerbotify\n'
                  '# apply_hwang_curse_removed, the share step of the game Dockerfile): the shop\n'
                  '# that sold one sells it no more. The db core reads the shops at boot, so this\n'
                  '# is live on the next start; idempotent.\n'
                  'db -e "DELETE FROM world.shop_item WHERE item_vnum IN (72731, 72735);"\n'
                  '# And nobody keeps one: every Maska Sabaha still in a bag, on a character, in a\n'
                  '# safebox or on a counter is removed (Tieru, 15 September, "usun" to the masks\n'
                  '# players already held). On every start, so a mask an old core still held while\n'
                  '# an update ran this beside it goes on the next one.\n'
                  'masks=$(db -e "DELETE FROM player.item WHERE vnum IN (72731, 72735); SELECT ROW_COUNT();" || echo x)\n'
                  'masks=$(printf \'%s\' "$masks" | tr -d \'[:space:]\')\n'
                  'if [ "$masks" = "x" ]; then\n'
                  '    echo "[playerbot-migrate] WARNING: could not remove the Maska Sabaha items" >&2\n'
                  'elif [ -n "$masks" ] && [ "$masks" != "0" ]; then\n'
                  '    echo "[playerbot-migrate] removed $masks Maska Sabaha item(s)"\n'
                  'fi\n'
                  '# The market of Shinsoo\'s and Jinno\'s villages moved onto the kingdom\'s guard\n'
                  '# in 2.0.52 (GetTownPitch, playerbot_empire_rules.h), and nothing would ever\n'
                  '# have moved the shops standing round the old pitch: an offline shop stands\n'
                  '# where its keeper stood when it was opened (OpenOfflineShop takes the\n'
                  '# character\'s position, a reopen included) and a keeper walks to its shop to\n'
                  '# serve it. So each bot\'s shop of the old ring is carried across by the\n'
                  '# distance between the two pitches, which keeps the ring\'s shape and spacing,\n'
                  '# and pulled in to 1650 of the guard where it stood further out - the ring of\n'
                  '# 400 to 1700 round each guard is open ground inside the safe zone on\n'
                  '# server_attr. A shop already inside the new ring and outside the old one\n'
                  '# belongs to the new pitch and stays. Once, marked in\n'
                  '# player.playerbot_migrations in the same transaction as the move; on every\n'
                  '# start after that only a bot\'s shop still within 2000 of an old pitch and more\n'
                  '# than 2000 from the new one moves - a keeper that reopened on the old spot\n'
                  '# while an update ran this beside the old game container (update.sh does not\n'
                  '# stop the game first). The db core writes a position only when a shop is\n'
                  '# opened or moved, so an old core cannot write the moved ones back. A player\'s\n'
                  '# own shop is left where its owner put it. Before the game container starts,\n'
                  '# because the db core reads the shops at boot.\n'
                  'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_migrations (name VARCHAR(64) NOT NULL PRIMARY KEY, done_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
                  'pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = \'pitch_on_guard_2052\';" 2>/dev/null || echo x)\n'
                  'case "$pitch_done" in\n'
                  '    0) pitch_near=1700; pitch_far=1700 ;;\n'
                  '    1) pitch_near=-1; pitch_far=2000 ;;\n'
                  '    *) pitch_near= ;;\n'
                  'esac\n'
                  'if [ -n "$pitch_near" ]; then\n'
                  '    if pitch_moved=$(db -e "\n'
                  '        CREATE TEMPORARY TABLE player.tmp_pitch_moves AS\n'
                  '        SELECT d.owner,\n'
                  '               d.nx + ROUND(d.dx * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS tx,\n'
                  '               d.ny + ROUND(d.dy * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS ty\n'
                  '          FROM (SELECT s.owner, m.nx, m.ny,\n'
                  '                       CAST(s.x AS SIGNED) - m.ox AS dx,\n'
                  '                       CAST(s.y AS SIGNED) - m.oy AS dy,\n'
                  '                       SQRT(POW(CAST(s.x AS SIGNED) - m.ox, 2) + POW(CAST(s.y AS SIGNED) - m.oy, 2)) AS d_old,\n'
                  '                       SQRT(POW(CAST(s.x AS SIGNED) - m.nx, 2) + POW(CAST(s.y AS SIGNED) - m.ny, 2)) AS d_new\n'
                  '                  FROM player.ikashop_offlineshop AS s\n'
                  '                  JOIN player.player AS p ON p.id = s.owner\n'
                  '                  JOIN account.account AS a ON a.id = p.account_id\n'
                  '                  JOIN (SELECT 1 AS map, 473625 AS ox, 954925 AS oy, 474325 AS nx, 954225 AS ny\n'
                  '                        UNION ALL SELECT 3, 353987, 880012, 353025, 882325\n'
                  '                        UNION ALL SELECT 41, 961212, 270162, 959925, 268825\n'
                  '                        UNION ALL SELECT 43, 865500, 244975, 863425, 246025) AS m ON m.map = s.map\n'
                  '                 WHERE a.login LIKE \'playerbot%\') AS d\n'
                  '         WHERE d.d_old <= 2000 AND (d.d_old <= $pitch_near OR d.d_new > $pitch_far);\n'
                  '        START TRANSACTION;\n'
                  '        UPDATE player.ikashop_offlineshop AS s\n'
                  '          JOIN player.tmp_pitch_moves AS t ON t.owner = s.owner\n'
                  '           SET s.x = t.tx, s.y = t.ty;\n'
                  '        SELECT ROW_COUNT();\n'
                  '        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES (\'pitch_on_guard_2052\', NOW());\n'
                  '        COMMIT;\n'
                  '        DROP TEMPORARY TABLE player.tmp_pitch_moves;\n'
                  '    "); then\n'
                  '        pitch_moved=$(printf \'%s\' "$pitch_moved" | tr -d \'[:space:]\')\n'
                  '        if [ "${pitch_moved:-0}" != "0" ]; then\n'
                  '            echo "[playerbot-migrate] $pitch_moved bot offline shop(s) in Yongan, Jayang, Pyongmoo and Bakra carried onto the guard\'s square"\n'
                  '        fi\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: could not move the bots\' offline shops onto the new pitches" >&2\n'
                  '    fi\n'
                  'fi\n'
                  '# fish_log came from r40250\'s dump and has that engine\'s eight columns,\n'
                  '# while this one writes six - so every catch failed with errno 1136 and the\n'
                  '# table is empty on every 2.x world that ever ran. CREATE IF NOT EXISTS\n'
                  '# cannot repair a table that already exists with the wrong shape, so the\n'
                  '# old one is dropped here, before log_schema.sql below recreates it.\n'
                  '# Recognised by a column this engine never writes; a table already in the\n'
                  '# right shape, and whatever history it holds, is left alone.\n'
                  'fish_old=$(db -e "\n'
                  '    SELECT COUNT(*) FROM information_schema.columns\n'
                  '     WHERE table_schema=\'log\' AND table_name=\'fish_log\' AND column_name=\'map_index\';\n'
                  '" 2>/dev/null || echo 0)\n'
                  'if [ "$fish_old" = "1" ]; then\n'
                  '    echo "[playerbot-migrate] fish_log has the r40250 shape and cannot be written; rebuilding it"\n'
                  '    db -e "DROP TABLE IF EXISTS log.fish_log;"\n'
                  'fi\n'
                  '# The log tables the engine writes and the package dump lacks (port/logschemify.py).\n'
                  'if [ -s /opt/playerbot/log_schema.sql ]; then\n'
                  '    if db < /opt/playerbot/log_schema.sql 2>/tmp/logschema.err; then\n'
                  '        echo "[playerbot-migrate] log schema checked"\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: log schema failed:" >&2\n'
                  '        head -3 /tmp/logschema.err >&2\n'
                  '    fi\n'
                  'fi\n'
                  '\n' + anchor)
    # The package's guild lands, after the pitch step has made sure
    # player.playerbot_migrations exists.
    anchor = '# fish_log came from r40250\'s dump and has that engine\'s eight columns,\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, guild_lands_block() + anchor)
    # The three steps that used to live only in the rendered file.
    for text, anchor in (
            (FISHING_PASS_AND_RING, '# Maska Sabaha left the world with the Hwang curse (playerbotify\n'),
            (GUILD_TIERS_AND_CHANNEL_PINS,
             'pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = '
             '\'pitch_on_guard_2052\';" 2>/dev/null || echo x)\n'),
            (WORLD_RATES,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (WORLD_DIFFICULTY,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (STARTER_CHEST,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n')):
        assert s.count(anchor) == 1, anchor
        s = s.replace(anchor, text + anchor)
    # The four game masters of the tester account (gm_characters.sql), before
    # the generic "grant the oldest character" step: on a world whose admin
    # account is still empty they are created with their gmlist rows, on a
    # world where somebody plays on admin the file does nothing and the
    # generic step grants that character.
    anchor = ('# ---------------------------------------------------------------------------\n'
              '# A game master for the tester account.\n')
    assert s.count(anchor) == 1
    s = s.replace(anchor,
                  '# The tester account\'s own game masters, mt2009 only (see the file).\n'
                  'if [ -s /opt/playerbot/gm_characters.sql ]; then\n'
                  '    if gm_out=$(db < /opt/playerbot/gm_characters.sql 2>&1); then\n'
                  '        echo "[playerbot-migrate] $gm_out"\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: gm_characters.sql failed:" >&2\n'
                  '        echo "$gm_out" | head -3 >&2\n'
                  '    fi\n'
                  'fi\n'
                  '\n' + anchor)
    head = ('#!/bin/sh\n'
            '# Rendered for the mt2009 world by linux-port-mt2009/port/migratorify.py from\n'
            '# linux-port/docker/mariadb/playerbot/apply.sh. DO NOT EDIT; edit the original.\n')
    assert s.startswith('#!/bin/sh\n'), s[:40]
    s = head + s[len('#!/bin/sh\n'):]
    io.open(os.path.join(DST, 'apply.sh'), 'w', encoding='utf-8', newline='').write(s)
    print('migratorify: apply.sh rendered')

    shutil.copyfile(os.path.join(SRC, 'itemshop_schema.sql'), os.path.join(DST, 'itemshop_schema.sql'))
    print('migratorify: itemshop_schema.sql copied')


if __name__ == '__main__':
    main()
