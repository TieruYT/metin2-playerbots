#!/bin/sh
# Rendered for the mt2009 world by linux-port-mt2009/port/migratorify.py from
# linux-port/docker/mariadb/playerbot/apply.sh. DO NOT EDIT; edit the original.
# Apply the tracked Playerbot seed once per Compose start. The SQL itself is
# idempotent and conflict-safe, so this also handles existing databases.
set -eu

: "${M2_DB_HOST:?M2_DB_HOST is required}"
: "${M2_DB_PORT:?M2_DB_PORT is required}"
: "${M2_DB_USER:?M2_DB_USER is required}"
: "${M2_DB_PASSWORD:?M2_DB_PASSWORD is required}"

strict=${PLAYERBOT_SEED_STRICT:-0}
case "$strict" in
    0|1) ;;
    *)
        echo "[playerbot-migrate] FATAL: PLAYERBOT_SEED_STRICT must be 0 or 1" >&2
        exit 1
        ;;
esac

expected_existing_bots=${PLAYERBOT_EXPECT_MIN_EXISTING_BOTS:-0}
case "$expected_existing_bots" in
    ''|*[!0-9]*)
        echo "[playerbot-migrate] FATAL: PLAYERBOT_EXPECT_MIN_EXISTING_BOTS must be a non-negative integer" >&2
        exit 1
        ;;
esac

seed=/opt/playerbot/playerbots_seed.sql
[ -s "$seed" ] || {
    echo "[playerbot-migrate] FATAL: $seed is missing or empty" >&2
    exit 1
}

db() {
    # mariadb(1) inherits MYSQL_PWD; MARIADB_PWD is not a client variable.
    # Keeping it out of argv avoids exposing the secret in `docker top`/ps.
    MYSQL_PWD="$M2_DB_PASSWORD" mariadb \
        --protocol=tcp \
        --host="$M2_DB_HOST" \
        --port="$M2_DB_PORT" \
        --user="$M2_DB_USER" \
        --default-character-set=latin1 \
        --batch --skip-column-names "$@"
}

# The official image can answer its healthcheck while its temporary first-run
# server is still importing dumps. Wait for tables at the end of every shipped
# dump, then require the item prototypes used by the starter rows.
echo "[playerbot-migrate] waiting for the complete mt2009 schema"
attempt=0
# Consecutive probes refused for authentication; see the check inside the
# loop. Reset by any probe that fails for a different reason, so a login
# that starts working is not held against it.
auth_failures=0
while :; do
    attempt=$((attempt + 1))
    # Keep the error instead of discarding it. A refused login looks exactly
    # like a schema that has not finished importing, and silently waiting five
    # minutes for a permissions problem to fix itself helps nobody.
    probe_err=/tmp/playerbot-probe.err
    ready=$(db -e "
        SELECT COUNT(*)
          FROM information_schema.tables
         WHERE (table_schema='account' AND table_name='account')
            OR (table_schema='common'  AND table_name='gmlist')
            OR (table_schema='player'  AND table_name IN
                ('player','player_index','item','item_proto','string'))
            OR (table_schema='log'     AND table_name='hack_log');
    " 2>"$probe_err" || true)
    if [ -s "$probe_err" ] && [ "$attempt" -eq 3 ]; then
        echo "[playerbot-migrate] the database is not answering yet:" >&2
        head -3 "$probe_err" >&2
    fi

    # A refused login is not a slow import, and waiting thirty minutes for it
    # to fix itself tells the operator the wrong thing twice: once by the wait
    # and once by the message at the end, which blames a large world still
    # recovering and suggests starting again. It never recovers - the password
    # in .env and the one the volume was initialised with simply differ.
    #
    # MariaDB error 1045 is "access denied for user" and 1044 is "access denied
    # to database"; both are permanent until somebody changes the credentials.
    # Confirmed over a few attempts rather than on the first, because a server
    # in the middle of starting can refuse a connection once for other reasons,
    # and then given up on with a message about the thing that is actually
    # wrong. Everything else - a refused connection, a missing schema - keeps
    # the long budget, which is what it was for.
    if [ -s "$probe_err" ] && grep -qiE "1045|1044|access denied" "$probe_err"; then
        auth_failures=$((auth_failures + 1))
    else
        auth_failures=0
    fi
    if [ "$auth_failures" -ge 5 ]; then
        echo "[playerbot-migrate] FATAL: the database refuses this login." >&2
        head -1 "$probe_err" >&2
        echo "[playerbot-migrate] This is a credentials problem, not a slow import: the password in" >&2
        echo "[playerbot-migrate] linux-port/docker/.env and the one this database was created with" >&2
        echo "[playerbot-migrate] are not the same. Waiting will not change it." >&2
        echo "[playerbot-migrate] In the launcher: NAPRAW DOSTEP DO BAZY." >&2
        exit 1
    fi
    if [ "$ready" = "8" ]; then
        protos=$(db -e "SELECT COUNT(*) FROM player.item_proto;" 2>/dev/null || true)
        [ -n "$protos" ] && [ "$protos" -gt 0 ] 2>/dev/null && break
    fi
    # A large world that was not shut down cleanly can spend many minutes in
    # InnoDB recovery while the image's healthcheck already answers. Five
    # minutes was not enough for it, and because compose treats this container
    # as a hard dependency, the whole "up" failed and the launcher reported
    # that Docker had not built the server -- while starting it again by hand a
    # minute later worked. Wait far longer, and say what is happening.
    if [ "$attempt" -ge 900 ]; then
        echo "[playerbot-migrate] FATAL: database not ready after 30 minutes" >&2
        echo "[playerbot-migrate] the server is probably still recovering a large world; start it again" >&2
        exit 1
    fi
    if [ $((attempt % 30)) -eq 0 ]; then
        # Three different waits look the same from outside; say which this is.
        # "answering, with none of the eight tables" is not a slow import - it
        # is a MariaDB that initialised without the dumps, and no amount of
        # waiting changes it. One operator watched this for thirty minutes.
        if [ ! -s "$probe_err" ] && [ "${ready:-0}" = "0" ] && [ "$attempt" -ge 90 ]; then
            echo "[playerbot-migrate] MariaDB is answering but holds NONE of the r40250 tables ($((attempt * 2))s)." >&2
            echo "[playerbot-migrate] The database initialised without the SQL dumps: mariadb/initdb.d/dumps" >&2
            echo "[playerbot-migrate] was missing or empty on the very first start, and initdb.d never runs again." >&2
            echo "[playerbot-migrate] Waiting will not fix this. Stage the five dumps (the launcher and the" >&2
            echo "[playerbot-migrate] installer now check for them) and re-create the database volume." >&2
        elif [ ! -s "$probe_err" ] && [ "${ready:-0}" != "0" ]; then
            echo "[playerbot-migrate] still waiting for the schema ($((attempt * 2))s): ${ready}/8 tables so far - the first-run import is in progress"
        else
            echo "[playerbot-migrate] still waiting for the database ($((attempt * 2))s) - a large world can take a while to recover"
        fi
    fi
    sleep 2
done

# Repair anything MyISAM left marked as crashed.
#
# Seventy-three of this game's seventy-five tables are MyISAM, and one unclean
# stop marks a table crashed: every reader then fails until somebody repairs
# it. `myisam_recover_options = BACKUP,FORCE` in 99-metin2.cnf handles the
# common case, but only for a table the server itself opens AFTER that setting
# took effect - so a database that was already running when the option arrived
# keeps the old behaviour until it is restarted, and a data file damaged beyond
# what QUICK will touch stays broken either way. What the operator sees then is
# not a database error but a blank "Internal Server Error" from the advanced
# panel, which reads everything from these tables while the classic panel,
# which reads files, keeps working (archonek2137, 10 September: log.log marked
# as crashed).
#
# --fast only looks at tables that were not closed properly, so on a healthy
# world this is one open per table and repairs nothing. Failures are reported
# and never fatal: a world that starts with one damaged log table is far better
# than a world that refuses to start at all.
if [ -n "${M2_DB_ROOT_PASSWORD:-}" ]; then
    repair_log=/tmp/playerbot-repair.log
    if MYSQL_PWD="$M2_DB_ROOT_PASSWORD" mariadb-check \
            --protocol=tcp --host="$M2_DB_HOST" --port="$M2_DB_PORT" --user=root \
            --auto-repair --fast --silent \
            --databases account common player log >"$repair_log" 2>&1; then
        if [ -s "$repair_log" ]; then
            echo "[playerbot-migrate] repaired tables left crashed by an unclean stop:"
            head -20 "$repair_log"
        fi
    else
        echo "[playerbot-migrate] WARNING: table check failed; continuing" >&2
        head -5 "$repair_log" >&2
    fi
fi

# The ItemShop's own database, and the item_award table its purchases are
# delivered through. Created as root because the metin2 user cannot create a
# database, and only when the root password is in the environment (it is,
# from .env, on every install the launcher made); a world without it keeps
# running - the shop then answers with an empty page, not the game with an
# error. Idempotent: CREATE IF NOT EXISTS, and the seed only fills an empty
# shop, so an operator's own catalogue survives every restart.
social_len=$(db -e "
    SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.columns
     WHERE table_schema='account' AND table_name='account' AND column_name='social_id';
")
if [ -n "$social_len" ] && [ "$social_len" -lt 18 ] 2>/dev/null; then
    echo "[playerbot-migrate] widening account.social_id from $social_len to 18 characters"
    db -e "ALTER TABLE account.account MODIFY social_id VARCHAR(18) NOT NULL DEFAULT '';"
fi
# The ItemShop reads mileage and jackpot off the account; this schema has
# cash alone. IF NOT EXISTS keeps it a no-op after the first time.
db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS mileage INT NOT NULL DEFAULT 0;"
db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS jackpot INT NOT NULL DEFAULT 0;"
# Fishing from thirty, which is what the wiki says and what the operator
# asked for. This line shipped fifty in three places and moving two was not
# enough: CHARACTER::fishing() (playerbotify.py lowers it), the AI gate, and
# the rod LIMIT_LEVEL - the one that refuses the equip, so a bot of thirty
# could neither wear a rod nor be drawn as an angler. item_proto is read out
# of world.item_proto here (PROTO_FROM_DB = 1), which is why this sticks;
# idempotent, and it touches only rods still carrying the old fifty.
db -e "UPDATE world.item_proto SET limitvalue0 = 30 WHERE type = 13 AND limittype0 = 1 AND limitvalue0 = 50;"
# And the pass the rod needs. Karta Wedkarska (27620), which CHARACTER::fishing()
# wants worn, is sold in one place, the Fisherman's special shop (9009, opened
# by fishing_pass_shop.quest), and the package asks level fifty for it - so a
# player of thirty to forty-nine could wear the rod the line above allows and
# never fish (Tieru, 17 September). The db core reads shop_special_proto at
# boot, so this is live on the next start; idempotent, and only a fifty moves.
db -e "UPDATE world.shop_special_proto SET limitvalue0 = 30 WHERE item_vnum = 27620 AND limittype0 = 'LEVEL' AND limitvalue0 = 50; UPDATE world.shop_special_proto SET limitvalue1 = 30 WHERE item_vnum = 27620 AND limittype1 = 'LEVEL' AND limitvalue1 = 50;"
# Pierscien Teleportacji (70058) carries ITEM_FLAG_APPLICABLE (8192) in this
# package, and under ENABLE_QUEST_DND_EVENT that flag makes UseItemEx treat an
# ITEM_QUEST as "drop it onto another item": a plain use finds no target cell
# and returns before the quest is asked, so teleport_ring.quest never ran for
# a player ("caly czas nie dziala pierscien teleportu", Tieru, 16 September).
# The ring is dragged onto nothing; the flag comes off. Idempotent.
db -e "UPDATE world.item_proto SET flag = flag & ~8192 WHERE vnum = 70058 AND (flag & 8192) <> 0;"
# Maska Sabaha left the world with the Hwang curse (playerbotify
# apply_hwang_curse_removed, the share step of the game Dockerfile): the shop
# that sold one sells it no more. The db core reads the shops at boot, so this
# is live on the next start; idempotent.
db -e "DELETE FROM world.shop_item WHERE item_vnum IN (72731, 72735);"
# And nobody keeps one: every Maska Sabaha still in a bag, on a character, in a
# safebox or on a counter is removed (Tieru, 15 September, "usun" to the masks
# players already held). On every start, so a mask an old core still held while
# an update ran this beside it goes on the next one.
masks=$(db -e "DELETE FROM player.item WHERE vnum IN (72731, 72735); SELECT ROW_COUNT();" || echo x)
masks=$(printf '%s' "$masks" | tr -d '[:space:]')
if [ "$masks" = "x" ]; then
    echo "[playerbot-migrate] WARNING: could not remove the Maska Sabaha items" >&2
elif [ -n "$masks" ] && [ "$masks" != "0" ]; then
    echo "[playerbot-migrate] removed $masks Maska Sabaha item(s)"
fi
# The market of Shinsoo's and Jinno's villages moved onto the kingdom's guard
# in 2.0.52 (GetTownPitch, playerbot_empire_rules.h), and nothing would ever
# have moved the shops standing round the old pitch: an offline shop stands
# where its keeper stood when it was opened (OpenOfflineShop takes the
# character's position, a reopen included) and a keeper walks to its shop to
# serve it. So each bot's shop of the old ring is carried across by the
# distance between the two pitches, which keeps the ring's shape and spacing,
# and pulled in to 1650 of the guard where it stood further out - the ring of
# 400 to 1700 round each guard is open ground inside the safe zone on
# server_attr. A shop already inside the new ring and outside the old one
# belongs to the new pitch and stays. Once, marked in
# player.playerbot_migrations in the same transaction as the move; on every
# start after that only a bot's shop still within 2000 of an old pitch and more
# than 2000 from the new one moves - a keeper that reopened on the old spot
# while an update ran this beside the old game container (update.sh does not
# stop the game first). The db core writes a position only when a shop is
# opened or moved, so an old core cannot write the moved ones back. A player's
# own shop is left where its owner put it. Before the game container starts,
# because the db core reads the shops at boot.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_migrations (name VARCHAR(64) NOT NULL PRIMARY KEY, done_at DATETIME NOT NULL) ENGINE=InnoDB;"
# The bot guilds' tiers (playerbot_guild.h): a guild outlives every core
# restart, so its tier and kingdom live here; the core reads the table once
# and writes a row when it founds or adopts a guild.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_guild (guild_id INT UNSIGNED NOT NULL PRIMARY KEY, tier TINYINT UNSIGNED NOT NULL DEFAULT 3, empire TINYINT UNSIGNED NOT NULL DEFAULT 0, founder_pid INT UNSIGNED NOT NULL DEFAULT 0, founded_at DATETIME NOT NULL) ENGINE=InnoDB;"
# And when each last went to war (playerbot_guild_war.h), in unix seconds, so
# the pick that keeps a kingdom's last pair out of its next war survives the
# restart every update makes.
db -e "ALTER TABLE player.playerbot_guild ADD COLUMN IF NOT EXISTS last_war_at INT UNSIGNED NOT NULL DEFAULT 0;" \
    || echo "playerbot-migrate: could not add last_war_at to player.playerbot_guild" >&2
# The second channel's pins (playerbot_channel_rules.h): every bot that has
# ever kept an offline shop lives on the first channel for good, because the
# shops are the first channel's. The table only grows - each core adds the
# owners it sees before it reads it - and this adds them before any core has
# started, so the start that switches the second channel on finds every keeper
# of the last session already pinned. Written whatever the switch says.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_channel_pin (pid INT UNSIGNED NOT NULL PRIMARY KEY, pinned_at DATETIME NOT NULL) ENGINE=InnoDB;"
db -e "INSERT IGNORE INTO player.playerbot_channel_pin (pid, pinned_at) SELECT owner, NOW() FROM player.ikashop_offlineshop;" 2>/dev/null \
    || echo "playerbot-migrate: could not pin the shop keepers to the first channel" >&2
pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = 'pitch_on_guard_2052';" 2>/dev/null || echo x)
case "$pitch_done" in
    0) pitch_near=1700; pitch_far=1700 ;;
    1) pitch_near=-1; pitch_far=2000 ;;
    *) pitch_near= ;;
esac
if [ -n "$pitch_near" ]; then
    if pitch_moved=$(db -e "
        CREATE TEMPORARY TABLE player.tmp_pitch_moves AS
        SELECT d.owner,
               d.nx + ROUND(d.dx * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS tx,
               d.ny + ROUND(d.dy * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS ty
          FROM (SELECT s.owner, m.nx, m.ny,
                       CAST(s.x AS SIGNED) - m.ox AS dx,
                       CAST(s.y AS SIGNED) - m.oy AS dy,
                       SQRT(POW(CAST(s.x AS SIGNED) - m.ox, 2) + POW(CAST(s.y AS SIGNED) - m.oy, 2)) AS d_old,
                       SQRT(POW(CAST(s.x AS SIGNED) - m.nx, 2) + POW(CAST(s.y AS SIGNED) - m.ny, 2)) AS d_new
                  FROM player.ikashop_offlineshop AS s
                  JOIN player.player AS p ON p.id = s.owner
                  JOIN account.account AS a ON a.id = p.account_id
                  JOIN (SELECT 1 AS map, 473625 AS ox, 954925 AS oy, 474325 AS nx, 954225 AS ny
                        UNION ALL SELECT 3, 353987, 880012, 353025, 882325
                        UNION ALL SELECT 41, 961212, 270162, 959925, 268825
                        UNION ALL SELECT 43, 865500, 244975, 863425, 246025) AS m ON m.map = s.map
                 WHERE a.login LIKE 'playerbot%') AS d
         WHERE d.d_old <= 2000 AND (d.d_old <= $pitch_near OR d.d_new > $pitch_far);
        START TRANSACTION;
        UPDATE player.ikashop_offlineshop AS s
          JOIN player.tmp_pitch_moves AS t ON t.owner = s.owner
           SET s.x = t.tx, s.y = t.ty;
        SELECT ROW_COUNT();
        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('pitch_on_guard_2052', NOW());
        COMMIT;
        DROP TEMPORARY TABLE player.tmp_pitch_moves;
    "); then
        pitch_moved=$(printf '%s' "$pitch_moved" | tr -d '[:space:]')
        if [ "${pitch_moved:-0}" != "0" ]; then
            echo "[playerbot-migrate] $pitch_moved bot offline shop(s) in Yongan, Jayang, Pyongmoo and Bakra carried onto the guard's square"
        fi
    else
        echo "[playerbot-migrate] WARNING: could not move the bots' offline shops onto the new pitches" >&2
    fi
fi
# The package's player dump carries the guild lands and buildings of the
# server it was taken from - 28 player.guild_land rows and 62 player.object
# rows - and none of the guilds they belong to. The engine stands its land
# agent (NPC 20040) only on a land nobody owns (building::CManager, at boot),
# so those lands could never be bought and their buildings stood on ground
# nobody held, while a bot guild founded later under one of those numbers
# (2, 3, 5, ...) held a land and buildings it never paid for ("stoja juz
# budynki, pomimo ze teren nie jest zajety", Mat, 19 September; NerrVoVy
# cleared his by hand). Once, and the dump's own rows exactly: a land a
# player's guild has bought and the buildings it put up since (ids past the
# dump's last) are left alone. Before the game container starts, because the
# db core reads both at boot.
lands_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = 'package_guild_lands_2081';" 2>/dev/null || echo x)
if [ "$lands_done" = "0" ]; then
    if lands_out=$(db -e "
        START TRANSACTION;
        DELETE FROM player.object WHERE (id, land_id, vnum) IN (
            (1, 14, 14100), (2, 214, 14120), (3, 214, 14014), (4, 14, 14013), (5, 215, 14120), (6, 215, 14013), (7, 218, 14120), (8, 218, 14043),
            (9, 16, 14100), (10, 16, 14014), (11, 16, 14043), (12, 108, 14100), (13, 108, 14014), (14, 214, 14050), (15, 14, 14051), (16, 215, 14051),
            (17, 217, 14100), (18, 218, 14014), (19, 217, 14015), (20, 109, 14100), (21, 109, 14051), (22, 17, 14100), (23, 17, 14015), (24, 207, 14110),
            (25, 207, 14014), (26, 15, 14100), (27, 15, 14015), (28, 217, 14051), (29, 18, 14110), (30, 18, 14055), (31, 115, 14120), (32, 115, 14014),
            (33, 108, 14043), (34, 18, 14015), (35, 116, 14120), (36, 116, 14013), (37, 216, 14110), (38, 109, 14015), (39, 8, 14120), (40, 216, 14013),
            (41, 212, 14100), (42, 117, 14110), (43, 216, 14055), (44, 117, 14055), (45, 117, 14014), (46, 205, 14120), (47, 205, 14055), (48, 15, 14055),
            (49, 216, 14200), (50, 216, 14300), (51, 216, 14300), (52, 205, 14015), (53, 212, 14015), (54, 206, 14100), (55, 206, 14015), (56, 8, 14015),
            (57, 115, 14050), (58, 212, 14055), (59, 207, 14055), (60, 8, 14055), (61, 208, 14110), (62, 201, 14100));
        SELECT ROW_COUNT();
        DELETE FROM player.guild_land WHERE (land_id, guild_id) IN (
            (2, 408), (8, 78), (9, 108), (10, 69), (14, 3), (15, 395), (16, 2), (17, 52),
            (18, 18), (108, 5), (109, 6), (115, 92), (116, 93), (117, 20), (118, 13), (201, 212),
            (204, 712), (205, 57), (206, 9), (207, 58), (208, 25), (212, 19), (213, 14), (214, 15),
            (215, 344), (216, 47), (217, 33), (218, 7));
        SELECT ROW_COUNT();
        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('package_guild_lands_2081', NOW());
        COMMIT;
    "); then
        lands_objects=$(printf '%s\n' "$lands_out" | awk 'NR == 1')
        lands_rows=$(printf '%s\n' "$lands_out" | awk 'NR == 2')
        echo "[playerbot-migrate] the package's guild lands cleared: ${lands_rows:-0} land(s), ${lands_objects:-0} building(s)"
    else
        echo "[playerbot-migrate] WARNING: could not clear the package's guild lands" >&2
    fi
fi
# fish_log came from r40250's dump and has that engine's eight columns,
# while this one writes six - so every catch failed with errno 1136 and the
# table is empty on every 2.x world that ever ran. CREATE IF NOT EXISTS
# cannot repair a table that already exists with the wrong shape, so the
# old one is dropped here, before log_schema.sql below recreates it.
# Recognised by a column this engine never writes; a table already in the
# right shape, and whatever history it holds, is left alone.
fish_old=$(db -e "
    SELECT COUNT(*) FROM information_schema.columns
     WHERE table_schema='log' AND table_name='fish_log' AND column_name='map_index';
" 2>/dev/null || echo 0)
if [ "$fish_old" = "1" ]; then
    echo "[playerbot-migrate] fish_log has the r40250 shape and cannot be written; rebuilding it"
    db -e "DROP TABLE IF EXISTS log.fish_log;"
fi
# The log tables the engine writes and the package dump lacks (port/logschemify.py).
if [ -s /opt/playerbot/log_schema.sql ]; then
    if db < /opt/playerbot/log_schema.sql 2>/tmp/logschema.err; then
        echo "[playerbot-migrate] log schema checked"
    else
        echo "[playerbot-migrate] WARNING: log schema failed:" >&2
        head -3 /tmp/logschema.err >&2
    fi
fi

itemshop_schema=/opt/playerbot/itemshop_schema.sql
if [ -s "$itemshop_schema" ]; then
    if [ -n "${M2_DB_ROOT_PASSWORD:-}" ]; then
        if MYSQL_PWD="$M2_DB_ROOT_PASSWORD" mariadb --protocol=tcp --host="$M2_DB_HOST" \
                --port="$M2_DB_PORT" --user=root --default-character-set=utf8mb4 \
                < "$itemshop_schema" 2>/tmp/itemshop.err; then
            echo "[playerbot-migrate] itemshop schema applied"
        else
            echo "[playerbot-migrate] WARNING: itemshop schema failed:" >&2
            head -3 /tmp/itemshop.err >&2
        fi
    else
        echo "[playerbot-migrate] WARNING: M2_DB_ROOT_PASSWORD not set; itemshop schema skipped" >&2
    fi
fi

# A developer may keep more persistent bots than the public 350-row seed. When
# that world matters, make its minimum size explicit in .env. This catches the
# easy-to-miss case where Docker is pointed at another daemon or a fresh volume:
# fail before the canonical seed can make the empty world look legitimate.
existing_bot_count=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE name LIKE 'bot%';
")
if [ "$expected_existing_bots" -gt 0 ] && [ "$existing_bot_count" -lt "$expected_existing_bots" ]; then
    echo "[playerbot-migrate] FATAL: persistent-world guard expected at least $expected_existing_bots bots, found $existing_bot_count" >&2
    echo "[playerbot-migrate] FATAL: check the Docker context/daemon and the db-data volume before starting the game" >&2
    exit 1
fi
if [ "$expected_existing_bots" -gt 0 ]; then
    echo "[playerbot-migrate] persistent-world guard satisfied: $existing_bot_count bots present (minimum $expected_existing_bots)"
fi

# A bot whose saved map is not one this server hosts can never be spawned: the
# character load asks the sectree manager for the position, gets nothing, and
# gives up - the same two bots failed on all seventeen starts of one day, with
# no way to recover because the AI tick only ever sees bots that did spawn.
# Put them back on Bokjung's arrival point before the game core starts.
echo "[playerbot-migrate] checking for bots parked on maps this server does not host"
stranded=$(db -e "
    SELECT COUNT(*)
      FROM player.player p
      JOIN account.account a ON a.id = p.account_id
     WHERE LEFT(a.login, 10) = 'playerbot_'
       AND p.map_index NOT IN (1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,
                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,
                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351);
")
if [ -n "$stranded" ] && [ "$stranded" -gt 0 ] 2>/dev/null; then
    # Back to its OWN kingdom's second map, not always Chunjo's: a Jinno bot
    # dropped on Bokjung's arrival point is a bot in a foreign town with none
    # of its services in reach. The three points are the arrivals of each
    # kingdom's M1->M2 gate, read out of npc.txt (tools/dump_world_catalog.py);
    # Chunjo keeps the exact point this step has always used.
    db -e "
        UPDATE player.player p
          JOIN account.account a ON a.id = p.account_id
          LEFT JOIN player.player_index pi ON pi.id = a.id
           SET p.map_index = CASE pi.empire WHEN 1 THEN 3 WHEN 3 THEN 43 ELSE 23 END,
               p.x = CASE pi.empire WHEN 1 THEN 400200 WHEN 3 THEN 906400 ELSE 145500 END,
               p.y = CASE pi.empire WHEN 1 THEN 899500 WHEN 3 THEN 221400 ELSE 240000 END
         WHERE LEFT(a.login, 10) = 'playerbot_'
           AND p.map_index NOT IN (1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,
                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,
                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351);
    "
    echo "[playerbot-migrate] moved $stranded bot(s) back to their own kingdom"
fi

# A negative alignment on a bot is a bug's footprint, not a history: a bot has
# no quarrel with its own kingdom. From 2.0.39 a duellist's blow went through
# CHARACTER::Damage without asking whether the engine would allow it, so a
# challenger struck before the other side had agreed and a winner went on
# striking the respawned loser. The engine counted each such kill as a murder -
# minus twenty thousand, shared over the killer's party - and bots of level
# nine walked about as "Zlosliwy" (98 on our own world, the lowest at -151002).
# Cleared here, before any core holds the bots in memory: a running core writes
# its cached alignment back over an UPDATE.
negative=$(db -e "
    SELECT COUNT(*)
      FROM player.player p
      JOIN account.account a ON a.id = p.account_id
     WHERE LEFT(a.login, 10) = 'playerbot_'
       AND p.alignment < 0;
")
if [ -n "$negative" ] && [ "$negative" -gt 0 ] 2>/dev/null; then
    db -e "
        UPDATE player.player p
          JOIN account.account a ON a.id = p.account_id
           SET p.alignment = 0
         WHERE LEFT(a.login, 10) = 'playerbot_'
           AND p.alignment < 0;
    "
    echo "[playerbot-migrate] cleared the negative alignment of $negative bot(s)"
fi

# There used to be a step here that pulled every bot outside Orc Valley's
# central island back onto it, from the days when the navigation refused
# water and the island was all a bot could reach. The bridges are crossings
# now and the hubs span the whole valley - the Fanatic islands in the north,
# the Black Orc camps in the south - so that step moved 207 bots off their
# hunting grounds at every start. Gone on purpose.

# The registry's own size is written into the seed, so the wrapper never has to
# be edited in step with it. Hardcoding 350 here survived the move to a
# 1000-character cohort only because the old range happened to be a prefix of
# the new one.
pid_range=$(grep -o 'registry is not exactly PID [0-9]*\.\.[0-9]*' "$seed" | head -1 | sed 's/.*PID //')
first_pid=${pid_range%%..*}
last_pid=${pid_range##*..}
case "${first_pid:-}${last_pid:-}" in
    ''|*[!0-9]*)
        echo "[playerbot-migrate] FATAL: cannot read the PID range from $seed" >&2
        exit 1
        ;;
esac

before=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE id BETWEEN $first_pid AND $last_pid;
")

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

# The world's difficulty, as event flags in seconds (player.quest, dwPID 0 -
# what the db core loads at boot and pushes to every game core, the package's
# own idiom for a world-wide switch). quest/m2_difficulty.lua reads them: the
# Biologist's wait between two hand-ins and the stable keeper's four waits
# (the pony, each Horse Book, the medal trainings of 1-10 and of 11-19). The
# presets scale the package's own numbers - hard is what it shipped with,
# medium a third of it, easy none (what 2.0.55 and 2.0.56 gave everybody) -
# and custom takes the hour counts from .env, the horse's for every wait.
# The wait between two skill books is the engine's (m2_book_wait, playerbotify
# apply_book_wait) and the bots' own (m2_bot_book_wait), the package's 21 hours
# on hard (drip9660, 23 September).
# Written before the seed, which may leave early on a foreign cohort.
#
# The classic panel's difficulty card sets the same flags live, so .env is
# applied only when it changed since the last start (m2_difficulty_env holds
# what it said): a change made in the panel survives a restart until the
# launcher's difficulty is changed, and the one changed last is the one kept.
difficulty=$(printf '%s' "${M2_DIFFICULTY:-easy}" | tr 'A-Z' 'a-z' | tr -d ' \r')
hours_to_seconds() {
    printf '%s\n' "$1" | tr -d ' \r' | awk '{ h = $1 + 0; if (h < 0) h = 0; if (h > 8760) h = 8760; printf "%d", h * 3600 }'
}
case "$difficulty" in
    medium) dlevel=1; bio=28800; hbuy=14400; hup=14400; htr=21600; htr2=25200; book=25200; botbook=25200 ;;
    hard)   dlevel=2; bio=86400; hbuy=43200; hup=43200; htr=64800; htr2=75600; book=75600; botbook=75600 ;;
    custom)
        dlevel=3
        bio=$(hours_to_seconds "${M2_BIOLOGIST_WAIT_HOURS:-0}")
        hbuy=$(hours_to_seconds "${M2_HORSE_WAIT_HOURS:-0}")
        hup=$hbuy; htr=$hbuy; htr2=$hbuy
        book=$(hours_to_seconds "${M2_BOOK_WAIT_HOURS:-0}")
        botbook=$(hours_to_seconds "${M2_BOT_BOOK_WAIT_HOURS:-0}") ;;
    *)      difficulty=easy; dlevel=0; bio=0; hbuy=0; hup=0; htr=0; htr2=0; book=0; botbook=0 ;;
esac
dsig=$(printf '%s|%s|%s|%s|%s|%s|%s|%s|%s' "$difficulty" "$bio" "$hbuy" "$hup" "$htr" "$htr2" "$book" "$botbook" 1 | cksum | awk '{ print $1 % 2000000000 }')
dprev=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_difficulty_env' LIMIT 1" 2>/dev/null | tr -d ' \r')
if [ -n "$dprev" ] && [ "$dprev" = "$dsig" ]; then
    echo "[playerbot-migrate] difficulty: .env unchanged since the last start - the flags stay as the panel or the last start left them"
elif db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
        (0, 'm2_difficulty', '', $dlevel),
        (0, 'm2_biologist_wait', '', $bio),
        (0, 'm2_horse_buy_wait', '', $hbuy),
        (0, 'm2_horse_upgrade_wait', '', $hup),
        (0, 'm2_horse_train_wait', '', $htr),
        (0, 'm2_horse_train2_wait', '', $htr2),
        (0, 'm2_book_wait', '', $book),
        (0, 'm2_bot_book_wait', '', $botbook),
        (0, 'm2_difficulty_env', '', $dsig);"; then
    echo "[playerbot-migrate] difficulty: $difficulty (Biologist wait ${bio}s, horse: buy ${hbuy}s upgrade ${hup}s train ${htr}s/${htr2}s, books: players ${book}s bots ${botbook}s)"
else
    echo "[playerbot-migrate] WARNING: could not write the difficulty flags; the quests keep the last ones" >&2
fi

# Whether a player's new character gets the apprentice chest at its first
# login (starter_chest.quest reads m2_starter_chest_off). Asked with the
# rates when a world is made (seban latino's idea, 22 September); on unless
# .env says M2_STARTER_CHEST=0. An event flag like the difficulty, so a
# change reaches the quests at the next start.
starter=$(printf '%s' "${M2_STARTER_CHEST:-1}" | tr 'A-Z' 'a-z' | tr -d ' \r')
case "$starter" in
    0|off|no|false) starter_off=1 ;;
    *)              starter_off=0 ;;
esac
if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
        (0, 'm2_starter_chest_off', '', $starter_off);"; then
    echo "[playerbot-migrate] apprentice chest for new characters: $([ "$starter_off" = 1 ] && echo off || echo on)"
else
    echo "[playerbot-migrate] WARNING: could not write the apprentice chest flag; the quest keeps the last one" >&2
fi

echo "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"
result=/tmp/playerbot-seed.out
trap 'rm -f "$result"' EXIT HUP INT TERM
# Shinsoo and Jinno are opt-in: M2_PLAYERBOT_KINGDOMS=1 lets the seed create
# their cohorts, anything else keeps the file to the Chunjo cohort it has
# always been. The variable goes in ahead of the file, in the same session,
# because a SET is per-connection.
kingdoms=0
case "${M2_PLAYERBOT_KINGDOMS:-0}" in
    1|true|TRUE|yes|YES) kingdoms=1 ;;
esac
echo "[playerbot-migrate] kingdoms (Shinsoo/Jinno) cohorts: $kingdoms"
if { printf 'SET @playerbot_seed_kingdoms = %s;
' "$kingdoms"; cat "$seed"; } |
        db --show-warnings >"$result" 2>&1; then
    [ ! -s "$result" ] || cat "$result"
else
    rc=$?
    cat "$result" >&2
    if grep -Fq 'playerbot seed conflict:' "$result"; then
        if [ "$strict" = "1" ]; then
            echo "[playerbot-migrate] FATAL: canonical cohort conflict (strict mode)" >&2
            exit "$rc"
        fi
        echo "[playerbot-migrate] WARNING: existing non-canonical Playerbot cohort detected" >&2
        echo "[playerbot-migrate] WARNING: preserving it unchanged; canonical seed skipped" >&2
        echo "[playerbot-migrate] WARNING: set PLAYERBOT_SEED_STRICT=1 to make this fatal" >&2
        exit 0
    fi
    echo "[playerbot-migrate] FATAL: seed failed for a non-conflict reason" >&2
    exit "$rc"
fi

count=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE id BETWEEN $first_pid AND $last_pid;
")
if [ -z "$count" ] || [ "$count" -eq 0 ] 2>/dev/null; then
    echo "[playerbot-migrate] FATAL: post-check found no bot characters at all" >&2
    exit 1
fi
added=$((count - before))
if [ "$added" -gt 0 ]; then
    echo "[playerbot-migrate] created $added new bot character(s)"
fi
echo "[playerbot-migrate] seed complete: $count bot character(s) in PID $first_pid..$last_pid"

# ---------------------------------------------------------------------------
# Human nicknames.
#
# "Pozdrawiam pana botarek7 jest kotem ale brzmi jak bot" - a world of botX7
# reads as a world of bots however well they behave. The pool and the rules are
# in playerbot_names.sql; this only chooses which of its three modes to run and
# reports what it did.
#
# It runs after the seed on purpose: a bot created a minute ago is renamed on
# the same start, and a bot the seed decided to preserve is left with whatever
# name it has, because the SQL only touches characters still called bot*.
#
# A failure here is not fatal. Names are the one part of a bot's identity
# nothing depends on - the core matches on the account login - so a server that
# could not rename its bots is a server that works with the old names.
# ---------------------------------------------------------------------------
names=/opt/playerbot/playerbot_names.sql
if [ -s "$names" ]; then
    human=1
    case "${M2_PLAYERBOT_HUMAN_NAMES:-1}" in
        0|false|FALSE|no|NO) human=0 ;;
        restore|RESTORE) human=restore ;;
    esac
    echo "[playerbot-migrate] human nicknames: $human"
    names_out=/tmp/playerbot-names.out
    if { printf 'SET @playerbot_human_names = %s;
' "'$human'"; cat "$names"; } |
            db --show-warnings >"$names_out" 2>&1; then
        [ ! -s "$names_out" ] || cat "$names_out"
    else
        cat "$names_out" >&2
        echo "[playerbot-migrate] WARNING: nicknames not applied; bots keep their seed names" >&2
    fi
    rm -f "$names_out"
else
    echo "[playerbot-migrate] no playerbot_names.sql; bots keep their seed names"
fi

# The tester account's own game masters, mt2009 only (see the file).
if [ -s /opt/playerbot/gm_characters.sql ]; then
    if gm_out=$(db < /opt/playerbot/gm_characters.sql 2>&1); then
        echo "[playerbot-migrate] $gm_out"
    else
        echo "[playerbot-migrate] WARNING: gm_characters.sql failed:" >&2
        echo "$gm_out" | head -3 >&2
    fi
fi

# ---------------------------------------------------------------------------
# A game master for the tester account.
#
# GM rights are one row in common.gmlist naming an account AND a character,
# and the mt2009 package ships neither a row nor a character on `admin' - so
# a world initialised before initdb created one (2.0.0, 2.0.1) has an admin
# who can log in and command nothing, and the first report was exactly that:
# "loguje sie admin admin, a tam nie ma postaci gm". The character the player
# made in the meantime is the one they want the rights on, so this grants
# IMPLEMENTOR to the tester account's oldest character, once, and only while
# the list is empty: a world that has ever named a GM is left as it is, and a
# world whose tester account was removed (M2_KEEP_DEMO_ACCOUNTS=0) has nothing
# to grant to. A world that has no character on the account yet gets the row
# on the first start after one is created. The db core reads the list at
# boot, and this runs before the game container starts.
# ---------------------------------------------------------------------------
gm_rows=$(db -e "SELECT COUNT(*) FROM common.gmlist;" 2>/dev/null || echo x)
if [ "$gm_rows" = "0" ]; then
    gm_name=$(db -e "
        SELECT p.name
          FROM player.player AS p
          JOIN account.account AS a ON a.id = p.account_id
         WHERE a.login = 'admin'
         ORDER BY p.id
         LIMIT 1;
    " 2>/dev/null || true)
    if [ -n "$gm_name" ]; then
        if db -e "
            INSERT INTO common.gmlist (mAccount, mName, mContactIP, mServerIP, mAuthority)
            VALUES ('admin', '$gm_name', '', 'ALL', 'IMPLEMENTOR');
        "; then
            echo "[playerbot-migrate] gmlist was empty: '$gm_name' on the admin account is IMPLEMENTOR now"
        else
            echo "[playerbot-migrate] WARNING: could not grant GM rights to '$gm_name'" >&2
        fi
    else
        echo "[playerbot-migrate] gmlist is empty and the admin account has no character yet; the first one it gets becomes GM on the next start"
    fi
fi
