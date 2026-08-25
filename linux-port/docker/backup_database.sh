#!/usr/bin/env bash
set -euo pipefail

output=${1:?usage: backup_database.sh /absolute/path/database-all.sql.gz}
tmp_output="${output}.tmp.$$"
umask 077
trap 'rm -f "$tmp_output"' EXIT

# Credentials remain inside the database container. Stop the game and panel
# before invoking this script when a point-in-time checkpoint is required.
docker exec metin2-db sh -c '
    export MYSQL_PWD="$M2_DB_PASSWORD"
    exec mariadb-dump \
        --user="$M2_DB_USER" \
        --all-databases \
        --routines \
        --events \
        --triggers \
        --quick
' | gzip -9 > "$tmp_output"

test -s "$tmp_output"
gzip -t "$tmp_output"
mv -f "$tmp_output" "$output"
trap - EXIT
printf 'database backup verified: %s\n' "$output"
