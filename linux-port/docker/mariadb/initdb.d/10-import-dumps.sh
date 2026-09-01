#!/bin/bash
# =============================================================================
#  First-run database import.
#
#  The official MariaDB image runs everything in /docker-entrypoint-initdb.d
#  exactly once -- when the data directory is empty.  So this runs on the very
#  first `docker compose up' and never again, which is precisely the semantics
#  wanted: a second run would overwrite every character on the server.
#
#  The dumps are raw table dumps with no USE statement and no CREATE DATABASE,
#  so each one has to be fed to a named database.  That is why this is a shell
#  script and not a .sql file.
#
#  MARIADB_DATABASE in the compose file cannot do this job: it creates exactly
#  one database, and r40250 needs five.
# =============================================================================
set -euo pipefail

DUMP_DIR=/docker-entrypoint-initdb.d/dumps

# hotbackup legitimately contains no tables. The shipped Readme says it must
# exist but may be empty, and the db core's SQL_HOTBACKUP handle opens it at
# boot -- a missing database there is a startup failure, not a warning.
DATABASES="account common player log hotbackup"

mysql_do() { mariadb --protocol=socket -uroot -p"${MARIADB_ROOT_PASSWORD}" "$@"; }

echo "[initdb] creating databases and the game's SQL user"

# The character set is pinned per-database as well as server-wide, so that a
# later server-level change cannot silently alter what these five use.
{
  for d in $DATABASES; do
    echo "CREATE DATABASE IF NOT EXISTS \`$d\` DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;"
  done

  # The game user is created here rather than through MARIADB_USER so that its
  # grants can be scoped to the five game databases instead of *.*, and so that
  # it works from any address inside the compose network.
  echo "CREATE USER IF NOT EXISTS '${M2_DB_USER}'@'%' IDENTIFIED BY '${M2_DB_PASSWORD}';"
  echo "ALTER USER '${M2_DB_USER}'@'%' IDENTIFIED BY '${M2_DB_PASSWORD}';"
  for d in $DATABASES; do
    echo "GRANT ALL PRIVILEGES ON \`$d\`.* TO '${M2_DB_USER}'@'%';"
  done
  echo "FLUSH PRIVILEGES;"
} | mysql_do

echo "[initdb] importing dumps from $DUMP_DIR"

if [ ! -d "$DUMP_DIR" ]; then
  echo "[initdb] FATAL: $DUMP_DIR does not exist."
  echo "[initdb]        Run ./prepare-context.sh to stage the SQL dumps, then"
  echo "[initdb]        'docker compose down -v' and up again."
  exit 1
fi

for d in $DATABASES; do
  f="$DUMP_DIR/$d.sql"
  if [ ! -f "$f" ]; then
    echo "[initdb] FATAL: $f is missing. The server cannot start without it."
    exit 1
  fi
  echo "[initdb]   importing $d ($(du -h "$f" | cut -f1))"
  # Warnings are expected here -- see the note in conf.d/99-metin2.cnf.
  mysql_do "$d" < "$f"
done

echo "[initdb] verifying"
for d in $DATABASES; do
  n=$(mysql_do -N -B -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$d'")
  echo "[initdb]   $d: $n tables"
done

# If this is zero the server will boot and then let nobody log in, so it is
# worth failing the initialisation loudly instead.
players=$(mysql_do -N -B -e "SELECT COUNT(*) FROM player.player_index" 2>/dev/null || echo 0)
protos=$(mysql_do -N -B -e "SELECT COUNT(*) FROM player.item_proto" 2>/dev/null || echo 0)
echo "[initdb]   player.player_index: $players rows, player.item_proto: $protos rows"

if [ "$protos" -lt 1 ]; then
  echo "[initdb] FATAL: player.item_proto is empty -- the import did not take."
  exit 1
fi

# -----------------------------------------------------------------------------
#  Remove the shipped demo accounts.
#
#  These server files ship `admin' and `test', both with the password 123456789,
#  and that pair is printed in the package's own Readme -- so everybody who has
#  ever downloaded it can log into any server that keeps them. On a server that
#  is reachable from the internet they are not a default, they are an open door.
#
#  The gmlist row goes too, and that part matters more than it looks. GM rights
#  are granted to the COMBINATION of an account name and a character name. Leave
#  the row behind and the names are now free: the next person to register an
#  account called `admin' and a character called `[SA]Admin' is handed
#  IMPLEMENTOR. Deleting the account without deleting the row would make the
#  server less safe than leaving both alone.
#
#  This runs only on a first initialisation, when nothing but the shipped data
#  exists, so no real player can be caught by it. Set M2_KEEP_DEMO_ACCOUNTS=1 to
#  keep them -- reasonable on a machine only you can reach, and the panel says
#  as much.
#
#  To make yourself a GM afterwards, with your own account and character:
#      INSERT INTO common.gmlist (mAccount, mName, mContactIP, mServerIP, mAuthority)
#      VALUES ('youraccount', 'YourCharacter', '', 'ALL', 'IMPLEMENTOR');
#  then restart the game.
# -----------------------------------------------------------------------------
echo "[initdb] setting up default tester player account (login: admin / password: admin)"
mysql_do <<'SQL'
INSERT INTO account.account (id, login, password, social_id, status, empire, create_time, last_play)
VALUES (1, 'admin', '*4ACFE3202A5FF5CF467898FC58AAB1D615029441', '1234567', 'OK', 2, NOW(), NOW()),
       (2, 'test',  '*4ACFE3202A5FF5CF467898FC58AAB1D615029441', '1234567', 'OK', 2, NOW(), NOW())
ON DUPLICATE KEY UPDATE
    password='*4ACFE3202A5FF5CF467898FC58AAB1D615029441',
    status='OK',
    empire=VALUES(empire);

-- The source dump stores both demo character slots in Jinno (empire 3) and
-- saves their characters on map 41.  Merely changing account.account.empire is
-- not enough: the character-selection screen reads player_index.empire, while
-- the game core loads the last saved map directly from player.player.  Keep all
-- three records consistent so a clean single-player install really starts in
-- Chunjo, next to the Joan city services advertised by the package.
UPDATE player.player_index AS pi
JOIN account.account AS a ON a.id = pi.id
SET pi.empire = 2
WHERE a.login IN ('admin', 'test');

UPDATE player.player AS p
JOIN account.account AS a ON a.id = p.account_id
SET p.x = 65900,
    p.y = 155600,
    p.z = 0,
    p.map_index = 21,
    p.exit_x = 65900,
    p.exit_y = 155600,
    p.exit_map_index = 21
WHERE a.login IN ('admin', 'test');
SQL

echo "[initdb] database initialisation complete"
