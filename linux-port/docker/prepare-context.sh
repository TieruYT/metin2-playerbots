#!/usr/bin/env bash
# =============================================================================
#  prepare-context.sh -- stage the build inputs into this directory.
#
#  Most operators never call this directly: ../fetch-sources.sh obtains the
#  upstream package, applies the port patch and then runs this script for them.
#
#  Call it yourself when the source tree, the runtime data tree and the SQL
#  dumps already exist outside this directory -- a development checkout, or a
#  tree fetch-sources.sh staged earlier.
#  It copies (never moves, never modifies) into:
#
#      game/src/build-deps-40250.sh    the dependency builder
#      game/src/extern/                boost/cryptopp/DevIL tarballs + headers
#      game/src/server/                the eight modules
#      game/src/serverfiles/share/     conf/ data/ locale/ package/
#      game/src/serverfiles/mark-default/
#      game/quest/web_admin.quest      the admin panel's in-game helper; the
#                                      game image compiles it into every locale
#                                      it finds (see game/Dockerfile, stage 2b)
#      panel/app/                      admin_panel.py, items.json, favicon.png
#      panel/app/VERSION               the repository's VERSION -- this is what
#                                      makes the running panel able to say which
#                                      build it is and whether it is behind
#      panel/app/CHANGELOG.md          so the panel's patch log can show what
#                                      you are running without the internet
#      panel/schema/                   web_admin_schema.sql
#      mariadb/initdb.d/dumps/         account/common/player/log/hotbackup.sql
#      game/rates/pack.sh              the server-files profile (rate maths)
#      client-builder/pack/pack.sh     the same file again (client preparation)
#
#  It also PATCHES the staged tree, in the only window where that works -- after
#  it is staged and before the image is built from it:
#
#      ../overlays/playerbot/           the Playerbot core integration, manager
#                                      sources and economy adjustment
#      ../../files/fixes/apply.sh      defects in the shipped files, for every
#                                      server, no switch
#      ../../files/custom/apply.sh     the Custom Experience, when
#                                      M2_CUSTOM_EXPERIENCE=1
#
#  Idempotent. Safe to re-run after editing the source.
#
#  Usage:
#      ./prepare-context.sh
#      ./prepare-context.sh --m2port /opt/m2port --panel ../../files
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

M2PORT="${M2PORT:-/opt/m2port}"
PANEL_SRC="${PANEL_SRC:-$HERE/../../files}"

# The repository root: VERSION and CHANGELOG.md live there, and both have to be
# inside the panel image. A panel that cannot read its own VERSION reports its
# version as "unknown" -- which is the honest answer, but a useless one, so this
# is staged rather than left to chance.
REPO_ROOT="${REPO_ROOT:-$(cd "$HERE/../.." && pwd)}"
PLAYERBOT_OVERLAY="$REPO_ROOT/linux-port/overlays/playerbot"
PLAYERBOT_SRC="$PLAYERBOT_OVERLAY/src/game/src"
PLAYERBOT_CORE_PATCH="$PLAYERBOT_OVERLAY/patches/0001-core-integration.patch"
PLAYERBOT_ECONOMY_PATCH="$PLAYERBOT_OVERLAY/patches/0002-economy-yang-x5.patch"
PLAYERBOT_LOG_PATCH="$PLAYERBOT_OVERLAY/patches/0003-suppress-refine-find-log.patch"
PLAYERBOT_SHOP_PATCH="$PLAYERBOT_OVERLAY/patches/0004-private-shop-guard.patch"
PLAYERBOT_SEED_GENERATOR="$PLAYERBOT_OVERLAY/tools/generate_seed.py"
PLAYERBOT_SEED="$PLAYERBOT_OVERLAY/sql/playerbots_seed.sql"
PLAYERBOT_MIGRATOR="$HERE/mariadb/playerbot/apply.sh"
PLAYERBOT_M3_DROPS="$PLAYERBOT_OVERLAY/serverfiles/mob_drop_item.m3.append.txt"

while [ $# -gt 0 ]; do
  case "$1" in
    --m2port) M2PORT="$2"; shift 2 ;;
    --panel)  PANEL_SRC="$2"; shift 2 ;;
    -h|--help) sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 1 ;;
  esac
done

say()  { printf '\n== %s\n' "$*"; }
info() { printf '   %s\n' "$*"; }
die()  { printf 'FATAL: %s\n' "$*" >&2; exit 1; }

PORT_SRC="$M2PORT/port40250"
RUNTIME_SRC="$M2PORT/server40250"
DUMP_SRC="$M2PORT/dbdump/zip"
DEPS_SCRIPT="$M2PORT/build-deps-40250.sh"

for p in "$PORT_SRC/server" "$PORT_SRC/extern" "$RUNTIME_SRC/share" "$DUMP_SRC" "$DEPS_SCRIPT"; do
  [ -e "$p" ] || die "$p not found (use --m2port to point at the porting tree)"
done
[ -f "$PANEL_SRC/admin_panel.py" ] || die "$PANEL_SRC/admin_panel.py not found (use --panel)"
command -v patch >/dev/null 2>&1 || die "GNU patch is needed to apply the Playerbot source overlay"
command -v git >/dev/null 2>&1 || die "git is needed to fingerprint the Playerbot source overlay"
command -v python3 >/dev/null 2>&1 || die "python3 is needed to verify the deterministic Playerbot seed"
# The overlay sources are discovered, never listed. Naming each file here (and
# in the copy, the chmod, the verification and the fingerprint below) meant that
# adding one source file cost five edits in this script alone - which is exactly
# why the manager grew to twelve thousand lines instead of being split up.
PLAYERBOT_SOURCES=""
for f in "$PLAYERBOT_SRC"/playerbot_*.h "$PLAYERBOT_SRC"/playerbot_*.cpp; do
  [ -e "$f" ] || continue
  PLAYERBOT_SOURCES="$PLAYERBOT_SOURCES $f"
done
[ -n "$PLAYERBOT_SOURCES" ] || die "no Playerbot overlay sources found in $PLAYERBOT_SRC"
# playerbot_manager.cpp is the one file the integration patch cannot work
# without, so its absence is a different failure from "the directory is empty".
[ -s "$PLAYERBOT_SRC/playerbot_manager.cpp" ] || die "Playerbot overlay input is missing or empty: $PLAYERBOT_SRC/playerbot_manager.cpp"

for p in \
  $PLAYERBOT_SOURCES \
  "$PLAYERBOT_CORE_PATCH" \
  "$PLAYERBOT_ECONOMY_PATCH" \
  "$PLAYERBOT_LOG_PATCH" \
  "$PLAYERBOT_SHOP_PATCH" \
  "$PLAYERBOT_SEED_GENERATOR" \
  "$PLAYERBOT_SEED" \
  "$PLAYERBOT_MIGRATOR" \
  "$PLAYERBOT_M3_DROPS"
do
  [ -s "$p" ] || die "Playerbot overlay input is missing or empty: $p"
done
python3 "$PLAYERBOT_SEED_GENERATOR" --check --output "$PLAYERBOT_SEED" \
  || die "Playerbot SQL snapshot is stale; regenerate it with $PLAYERBOT_SEED_GENERATOR"

GAME_CTX="$HERE/game/src"
rm -rf "$GAME_CTX"
mkdir -p "$GAME_CTX"

# -----------------------------------------------------------------------------
say "dependency builder"
cp -a "$DEPS_SCRIPT" "$GAME_CTX/build-deps-40250.sh"
info "build-deps-40250.sh"

# -----------------------------------------------------------------------------
say "extern (dependency sources)"
# Only the tarballs and the shipped headers are wanted. extern/lib holds the
# FreeBSD prebuilt archives -- ELF "version 1 (FreeBSD)" -- which must never
# reach a Linux link line; they are excluded rather than merely unused.
mkdir -p "$GAME_CTX/extern"
find "$PORT_SRC/extern" -maxdepth 1 -type f -name '*.tar.gz' -exec cp -a {} "$GAME_CTX/extern/" \;
if [ -d "$PORT_SRC/extern/include" ]; then
  cp -a "$PORT_SRC/extern/include" "$GAME_CTX/extern/include"
fi
info "$(find "$GAME_CTX/extern" -maxdepth 1 -name '*.tar.gz' | wc -l) tarball(s), $(du -sh "$GAME_CTX/extern" | cut -f1)"

# -----------------------------------------------------------------------------
say "server source (eight modules)"
mkdir -p "$GAME_CTX/server"
for m in common db game libgame liblua libpoly libserverkey libsql libthecore; do
  [ -d "$PORT_SRC/server/$m" ] || die "module $m missing from $PORT_SRC/server"
  cp -a "$PORT_SRC/server/$m" "$GAME_CTX/server/$m"
done
# Top-level Makefile is the untouched FreeBSD driver (CC=clang-devel, its game:
# and db: recipes commented out). The image builds per module and never uses
# it, so it is left out to remove the temptation.

# Derived artefacts are dropped here as well as in the Dockerfile: it keeps the
# context small and makes "did this really build from source" answerable by
# looking at the context.
find "$GAME_CTX/server" \( -name '*.o' -o -name '*.a' -o -name 'tags' \) -delete
rm -rf "$GAME_CTX/server"/*/src/OBJDIR "$GAME_CTX/server"/*/src/.obj \
       "$GAME_CTX/server"/*/OBJDIR "$GAME_CTX/server"/*/.obj
info "$(du -sh "$GAME_CTX/server" | cut -f1)"

# The porting cache is deliberately pristine. Playerbot is a small, tracked
# overlay applied only to the disposable build context, so a clean clone can
# reproduce it without committing the complete generated source tree. The core
# patch uses zero-context hunks so it never rewrites the legacy CP949 comments;
# the dry-run and post-apply checks make application errors fatal.
say "Playerbot server overlay"
if ! (cd "$GAME_CTX/server" && \
      patch --batch --forward --fuzz=0 -p1 --dry-run < "$PLAYERBOT_CORE_PATCH"); then
  die "the Playerbot core integration patch does not apply cleanly to the staged port source"
fi
if ! (cd "$GAME_CTX/server" && \
      patch --batch --forward --fuzz=0 -p1 --dry-run < "$PLAYERBOT_ECONOMY_PATCH"); then
  die "the Playerbot economy patch does not apply cleanly to the staged port source"
fi
if ! (cd "$GAME_CTX/server" && \
      patch --batch --forward --fuzz=0 -p1 --dry-run < "$PLAYERBOT_LOG_PATCH"); then
  die "the Playerbot log-noise patch does not apply cleanly to the staged port source"
fi
if ! (cd "$GAME_CTX/server" && \
      patch --batch --forward --fuzz=0 -p1 --dry-run < "$PLAYERBOT_SHOP_PATCH"); then
  die "the private-shop guard patch does not apply cleanly to the staged port source"
fi
(cd "$GAME_CTX/server" && \
  patch --batch --forward --fuzz=0 -p1 < "$PLAYERBOT_CORE_PATCH" && \
  patch --batch --forward --fuzz=0 -p1 < "$PLAYERBOT_ECONOMY_PATCH" && \
  patch --batch --forward --fuzz=0 -p1 < "$PLAYERBOT_LOG_PATCH" && \
  patch --batch --forward --fuzz=0 -p1 < "$PLAYERBOT_SHOP_PATCH") \
  || die "the Playerbot source overlay could not be applied"

for f in $PLAYERBOT_SOURCES; do
  cp -a "$f" "$GAME_CTX/server/game/src/$(basename "$f")"
  chmod 0644 "$GAME_CTX/server/game/src/$(basename "$f")"
done

# A source removed from the overlay has to disappear from the build context too.
# The Makefile now compiles every playerbot_*.cpp it finds there, so a file left
# behind by a previous run would still be built - against headers that no longer
# describe it.
for f in "$GAME_CTX/server/game/src"/playerbot_*.h "$GAME_CTX/server/game/src"/playerbot_*.cpp; do
  [ -e "$f" ] || continue
  [ -e "$PLAYERBOT_SRC/$(basename "$f")" ] || {
    info "removing stale overlay source $(basename "$f")"
    rm -f "$f"
  }
done

grep -q 'CPPFILE += $(wildcard playerbot_\*.cpp)' "$GAME_CTX/server/game/src/Makefile" \
  || die "Playerbot overlay validation failed: game Makefile does not pick up the overlay sources"
grep -q 'HEADER_GD_BOT_PLAYER_LOAD' "$GAME_CTX/server/common/tables.h" \
  || die "Playerbot overlay validation failed: DB protocol header is missing"
grep -q 'CPlayerBotManager::instance().OnPlayerLoaded' "$GAME_CTX/server/game/src/input_db.cpp" \
  || die "Playerbot overlay validation failed: player-load hook is missing"
grep -q 'iGold \*= 5;' "$GAME_CTX/server/game/src/char_battle.cpp" \
  || die "Playerbot overlay validation failed: economy adjustment is missing"
for f in $PLAYERBOT_SOURCES; do
  cmp -s "$f" "$GAME_CTX/server/game/src/$(basename "$f")" \
    || die "Playerbot overlay validation failed: $(basename "$f") copy differs"
done

{
  printf 'core_patch=%s\n' "$(git hash-object "$PLAYERBOT_CORE_PATCH")"
  printf 'economy_patch=%s\n' "$(git hash-object "$PLAYERBOT_ECONOMY_PATCH")"
  printf 'log_patch=%s\n' "$(git hash-object "$PLAYERBOT_LOG_PATCH")"
  # Every overlay source, sorted, so a changed or added file always invalidates
  # a stale build. Missing one here is the failure that does not announce
  # itself: the context looks fine and the image is simply not rebuilt.
  for f in $PLAYERBOT_SOURCES; do
    printf 'src_%s=%s\n' "$(basename "$f")" "$(git hash-object "$f")"
  done
  printf 'seed_generator=%s\n' "$(git hash-object "$PLAYERBOT_SEED_GENERATOR")"
  printf 'seed_sql=%s\n' "$(git hash-object "$PLAYERBOT_SEED")"
} > "$GAME_CTX/.playerbot-overlay"
info "core integration, manager sources, 5x Yang economy and quiet refine lookup staged"

# The regression that must be present. Checked here as well as in the
# Dockerfile so that a bad context is caught before a 10-minute build.
grep -q 'return fdwatch_sndbuf_left(fd);' "$GAME_CTX/server/libthecore/src/fdwatch.c" \
  || die "the fdwatch send-buffer fix is missing from libthecore/src/fdwatch.c -- no client could log in"
info "fdwatch send-buffer fix present"

# -----------------------------------------------------------------------------
say "runtime data tree"
mkdir -p "$GAME_CTX/serverfiles/share"
for d in conf data locale package; do
  [ -d "$RUNTIME_SRC/share/$d" ] || die "$RUNTIME_SRC/share/$d missing"
  cp -a "$RUNTIME_SRC/share/$d" "$GAME_CTX/serverfiles/share/$d"
  info "share/$d  $(du -sh "$GAME_CTX/serverfiles/share/$d" | cut -f1)"
done

cp -a "$PLAYERBOT_M3_DROPS" "$HERE/game/mob_drop_item.m3.append.txt"
info "M3/Waryong level-30 weapon and level-21 shield drop overlay staged"

# share/bin is deliberately NOT copied. The binaries in the image come from the
# builder stage; the tree also still carries the original FreeBSD game.freebsd
# and db.freebsd (90 MB + 20 MB) which cannot run on Linux at all.
info "share/bin skipped -- binaries come from the build, not the context"

# Guild mark seed for a fresh channel core.
mkdir -p "$GAME_CTX/serverfiles/mark-default"
if [ -d "$RUNTIME_SRC/channel1/first/mark" ]; then
  cp -a "$RUNTIME_SRC/channel1/first/mark/." "$GAME_CTX/serverfiles/mark-default/" 2>/dev/null || true
fi
info "mark-default  $(du -sh "$GAME_CTX/serverfiles/mark-default" | cut -f1)"

# -----------------------------------------------------------------------------
say "SQL dumps"
mkdir -p "$HERE/mariadb/initdb.d/dumps"
for d in account common player log hotbackup; do
  [ -f "$DUMP_SRC/$d.sql" ] || die "$DUMP_SRC/$d.sql missing"
  cp -a "$DUMP_SRC/$d.sql" "$HERE/mariadb/initdb.d/dumps/$d.sql"
  info "$d.sql  $(du -h "$DUMP_SRC/$d.sql" | cut -f1)"
done

# The generated copy lives beside Compose so installed stacks do not depend on
# repository-relative paths. Its authoritative, reproducible snapshot remains
# in overlays/playerbot/sql and is checked by generate_seed.py --check.
mkdir -p "$HERE/mariadb/playerbot"
cp -a "$PLAYERBOT_SEED" "$HERE/mariadb/playerbot/playerbots_seed.sql"
chmod 0644 "$HERE/mariadb/playerbot/playerbots_seed.sql"
cmp -s "$PLAYERBOT_SEED" "$HERE/mariadb/playerbot/playerbots_seed.sql" \
  || die "Playerbot seed staging failed: copied SQL differs from the tracked snapshot"
info "playerbots_seed.sql  $(du -h "$PLAYERBOT_SEED" | cut -f1)"

# The MariaDB image's entrypoint runs an executable *.sh from initdb.d, but
# merely *sources* a non-executable one -- which leaks this script's `set -e'
# into the entrypoint's own shell. A checkout that lost the mode bit (a zip
# round-trip, a Windows working copy) would take the second path silently, so
# the bit is asserted here rather than assumed.
chmod +x "$HERE/mariadb/initdb.d/"*.sh 2>/dev/null || true
chmod +x "$PLAYERBOT_MIGRATOR" 2>/dev/null || true

# Likewise for the scripts that go into the images. The Dockerfiles chmod them
# as well; this keeps `bash prepare-context.sh && docker compose up' honest on
# a working copy with no exec bits at all.
chmod +x "$HERE/game/bin/"* "$HERE/panel/bin/"* "$HERE/client-builder/bin/"* \
         "$HERE/updater/bin/"* 2>/dev/null || true

# -----------------------------------------------------------------------------
say "admin panel"
rm -rf "$HERE/panel/app"
mkdir -p "$HERE/panel/app" "$HERE/panel/schema"
cp -a "$PANEL_SRC/admin_panel.py" "$HERE/panel/app/"
for f in items.json favicon.png; do
  [ -f "$PANEL_SRC/$f" ] && cp -a "$PANEL_SRC/$f" "$HERE/panel/app/" && info "$f"
done
# The live map's Polish mode uses the complete server locale rather than a
# partial hand-maintained dictionary.  Keep this optional for custom source
# trees that genuinely do not ship Polish, in which case the panel falls back
# to its small built-in family translator.
if [ -f "$RUNTIME_SRC/share/conf/item_names_pl.txt" ]; then
  cp -a "$RUNTIME_SRC/share/conf/item_names_pl.txt" "$HERE/panel/app/"
  info "item_names_pl.txt"
fi
# The version, and the changelog that explains it. Both are plain text and both
# are read by the panel at runtime: VERSION is what it reports and what it
# compares against the published one, CHANGELOG.md is what its patch log shows
# for the build you are actually running. Absent, the panel says "unknown" and
# its patch log says the file is not in this build -- it never invents either.
if [ -f "$REPO_ROOT/VERSION" ]; then
  cp -a "$REPO_ROOT/VERSION" "$HERE/panel/app/VERSION"
  info "VERSION  $(tr -d ' \r\n' < "$REPO_ROOT/VERSION")"
else
  info "WARNING: $REPO_ROOT/VERSION not found -- the panel will report its"
  info "         version as \"unknown\" and cannot tell you when it is behind."
fi
if [ -f "$REPO_ROOT/CHANGELOG.md" ]; then
  cp -a "$REPO_ROOT/CHANGELOG.md" "$HERE/panel/app/CHANGELOG.md"
  info "CHANGELOG.md  $(du -h "$REPO_ROOT/CHANGELOG.md" | cut -f1)"
else
  info "WARNING: $REPO_ROOT/CHANGELOG.md not found -- the panel's patch log will"
  info "         have nothing to show for the version you are running."
fi

# -----------------------------------------------------------------------------
say "in-game helper quest"
# web_admin.quest is what makes the panel's Teleport and Running speed buttons
# work, and what makes items, yang and levels arrive immediately instead of at
# the player's next login. It is staged into the GAME context, not the panel's:
# the quest runs inside the game cores, and the game image compiles it (stage
# 2b of game/Dockerfile) with a qc built from the same source tree.
#
# Without it the panel still works -- it falls back to writing straight into the
# database after a short wait -- so a missing file is a warning, not a failure.
mkdir -p "$HERE/game/quest"
# Staged by default again as of 1.4.0, after somebody played on it.
#
# It was off between 1.3.2 and 1.3.4: the first version called pc.select() from
# a server timer, where the core has no current character, and reading it
# segfaulted the whole channel. The work now runs in a player timer, which the
# core enters with the character set, and pc.select is gone. M2_INGAME_HELPER=0
# leaves it out for anyone who would rather not have it.
if [ "${M2_INGAME_HELPER:-1}" = "1" ] && [ -f "$PANEL_SRC/web_admin.quest" ]; then
  cp -a "$PANEL_SRC/web_admin.quest" "$HERE/game/quest/web_admin.quest"
  info "web_admin.quest -> game/quest/"
elif [ -f "$PANEL_SRC/web_admin.quest" ]; then
  rm -f "$HERE/game/quest/web_admin.quest"
  info "web_admin.quest NOT staged (M2_INGAME_HELPER=0)"
else
  rm -f "$HERE/game/quest/web_admin.quest"
  info "WARNING: $PANEL_SRC/web_admin.quest not found -- the panel's Teleport"
  info "         and Running speed buttons will report that the in-game helper"
  info "         did not answer, and item/yang/level will take the slower"
  info "         database route."
fi

# -----------------------------------------------------------------------------
# The Custom Experience, resolved here because the two sections below are the
# first things it changes.
#
# Two of the settings it carries are staged rather than patched -- they are
# quests, and a quest is a file this script copies -- so they are simply given
# different defaults when the switch is on. Both of them are DEFAULTS and not
# overrides: an operator who names a movement-speed bonus of their own, or who
# says M2_HIGH_RISK=0 because they do not want that mode on their server, keeps
# what they asked for. The installer resolves both explicitly and writes them
# into .env, so these two lines are what makes a direct
# `M2_CUSTOM_EXPERIENCE=1 ./prepare-context.sh' behave the same way.
CUSTOM_EXPERIENCE="${M2_CUSTOM_EXPERIENCE:-0}"
case "$CUSTOM_EXPERIENCE" in
  1|true|yes|on|TRUE|YES|ON) CUSTOM_EXPERIENCE=1 ;;
  *)                         CUSTOM_EXPERIENCE=0 ;;
esac
if [ "$CUSTOM_EXPERIENCE" = "1" ]; then
  M2_MOVE_SPEED_BONUS="${M2_MOVE_SPEED_BONUS:-20}"
  M2_HIGH_RISK="${M2_HIGH_RISK:-1}"
fi

# -----------------------------------------------------------------------------
say "movement-speed bonus"
# A second quest, staged the same way and for the same reason: it runs inside
# the game cores, so it belongs in the GAME context and is compiled by the same
# qc. Off unless somebody asks for it.
#
# M2_MOVE_SPEED_BONUS is a percentage. The number is written INTO the file here
# rather than read at run time, because a compiled quest cannot read an
# environment variable -- so changing it means a rebuild, which the installer
# does anyway. The quest itself notices the change: it remembers what it applied
# in a quest flag and swaps the affect at each character's next login, so there
# is nothing to clean up when the number goes up, down or back to zero.
_speed="${M2_MOVE_SPEED_BONUS:-0}"
case "$_speed" in
  ''|*[!0-9]*) info "M2_MOVE_SPEED_BONUS='$_speed' is not a number -- treating it as 0"
               _speed=0 ;;
esac
if [ "$_speed" -gt 0 ] && [ -f "$PANEL_SRC/speed_boost.quest" ]; then
  sed "s/local want = .*/local want = $_speed/"       "$PANEL_SRC/speed_boost.quest" > "$HERE/game/quest/speed_boost.quest"
  if grep -q "local want = $_speed" "$HERE/game/quest/speed_boost.quest"; then
    info "speed_boost.quest -> game/quest/  (+${_speed}% for everyone, at login)"
  else
    rm -f "$HERE/game/quest/speed_boost.quest"
    info "WARNING: the bonus could not be written into speed_boost.quest --"
    info "         staging it unchanged would apply the wrong number, so it"
    info "         is left out entirely."
  fi
elif [ "$_speed" -gt 0 ]; then
  rm -f "$HERE/game/quest/speed_boost.quest"
  info "WARNING: $PANEL_SRC/speed_boost.quest not found -- no speed bonus"
else
  rm -f "$HERE/game/quest/speed_boost.quest"
  info "no movement-speed bonus (M2_MOVE_SPEED_BONUS=0)"
fi

# -----------------------------------------------------------------------------
say "High Risk mode"
# A third quest, staged the same way and for the same reason: it runs inside the
# game cores. It owns the player's choice of mode and nothing else -- what the
# choice MEANS is compiled into the cores (high_risk.h and the four places that
# include it), because this quest engine has no death event and its affect API
# cannot be shared with the movement-speed quest.
#
# Nothing is substituted into the file: the two bonus percentages are named
# constants in high_risk.h, not text in here, because they are read by the core
# and not by the quest.
#
# M2_HIGH_RISK=0 leaves it out. The core change is inert without it -- no
# character can set the flag, so every check falls through to stock behaviour --
# which is what makes the mode removable without rebuilding the source.
#
# THE CORE HALF USED TO BE MISSING HERE, AND THAT IS WORTH SPELLING OUT. For a
# while this block staged the quest and nothing else, while the comment above
# said the meaning was "compiled into the cores". It was -- but only on the one
# machine where files/high_risk/patch_core.py had been run by hand. Every server
# built from this repository got a quest that set a flag no code ever read: the
# offer appeared at level 15, the player chose High Risk, and absolutely nothing
# happened. No killer flag, so not attackable and not marked; no Cruel drop
# band; no bonuses. It looked like a broken feature rather than a missing one,
# which is exactly the kind of silence this project tries not to ship.
#
# So the C++ half is applied right here, against the tree that is about to be
# compiled, every time. --core-only keeps it off the repository: the quest above
# and the Dockerfile's qc stage are already in the checkout and must not be
# rewritten during a build.
if [ "${M2_HIGH_RISK:-1}" = "1" ] && [ -f "$PANEL_SRC/high_risk.quest" ]; then
  cp -a "$PANEL_SRC/high_risk.quest" "$HERE/game/quest/high_risk.quest"
  info "high_risk.quest -> game/quest/  (offered at level 15, switchable at any Guardian)"

  [ -f "$PANEL_SRC/high_risk/patch_core.py" ] \
    || die "M2_HIGH_RISK=1 and high_risk.quest is staged, but
  $PANEL_SRC/high_risk/patch_core.py is missing. Building now would produce the
  exact failure that script exists to prevent: a quest whose flag nothing reads."
  command -v python3 >/dev/null 2>&1 \
    || die "python3 is needed to apply the High Risk core change.
  On Debian or Ubuntu: apt-get install -y python3"
  python3 "$PANEL_SRC/high_risk/patch_core.py" --core-only --server "$HERE" \
    || die "the High Risk core change could not be applied (output above)"
elif [ "${M2_HIGH_RISK:-1}" = "1" ]; then
  rm -f "$HERE/game/quest/high_risk.quest"
  info "WARNING: $PANEL_SRC/high_risk.quest not found -- no High Risk mode"
else
  rm -f "$HERE/game/quest/high_risk.quest"
  info "High Risk mode NOT staged (M2_HIGH_RISK=0)"
fi

if [ -f "$PANEL_SRC/web_admin_schema.sql" ]; then
  cp -a "$PANEL_SRC/web_admin_schema.sql" "$HERE/panel/schema/"
  info "web_admin_schema.sql"
else
  info "WARNING: web_admin_schema.sql not found -- the panel's queue and rates"
  info "         tables will not be created and those pages will fail."
fi

# -----------------------------------------------------------------------------
say "fixes for the shipped server files"
# Defects in the package itself, applied to every server this project builds.
# There is no switch: the worst of them credited a quest's 150,000 Yang fee to
# the player instead of charging it, repeatably, which is an open money supply
# on any server running these files untouched.
#
# This runs HERE and not earlier because share/ has just been staged. It is also
# why these are scripts and not edits: fetch-sources.sh re-stages the tree from
# the pristine archive on every assembly, so anything done to it by hand is gone
# by the next update.
if [ -f "$PANEL_SRC/fixes/apply.sh" ]; then
  bash "$PANEL_SRC/fixes/apply.sh" --tree "$GAME_CTX" \
    || die "the shipped-file fixes could not be applied (output above)"
else
  info "WARNING: $PANEL_SRC/fixes/apply.sh not found -- the quest reward"
  info "         defects and the Dragon Lair money exploit are NOT fixed in"
  info "         this build."
fi

# -----------------------------------------------------------------------------
say "Custom Experience"
# Everything behind the installer's "Enable Custom Experience?" question that is
# a patch rather than a staged file: pick-up range, horse summoning, the horse
# medal time gates, the bonus drop tables, stackable skill books, the Musk Oil
# quest and the shop row that makes its new wording true. The two settings that
# ARE staged files, High Risk and the movement-speed bonus, were resolved
# further up.
#
# Turning the switch back off does not undo a database row that is already
# there -- see the note in files/custom/shop_musk_oil.sql -- but it does stop
# the file being staged, so the row is not re-applied on a server that never had
# it. Everything else is undone by the re-stage that precedes this script.
if [ "$CUSTOM_EXPERIENCE" = "1" ] && [ -f "$PANEL_SRC/custom/apply.sh" ]; then
  bash "$PANEL_SRC/custom/apply.sh" --tree "$GAME_CTX" --schema "$HERE/panel/schema" \
    || die "the Custom Experience could not be applied (output above)"
elif [ "$CUSTOM_EXPERIENCE" = "1" ]; then
  die "M2_CUSTOM_EXPERIENCE=1 but $PANEL_SRC/custom/apply.sh is not there.
  Building anyway would produce a server that quietly has none of what was
  asked for, which is the exact confusion this switch exists to end."
else
  rm -f "$HERE/panel/schema/shop_musk_oil.sql"
  info "not enabled (M2_CUSTOM_EXPERIENCE=0)"
fi

# -----------------------------------------------------------------------------
say "server-files profile (the rate maths, and the client)"
# Two images carry this file, and both carry it verbatim rather than a copy of
# the logic inside it:
#
#   game/rates/pack.sh           pack_apply_rates() -- which column of which
#                                table holds experience, yang, a drop chance
#   client-builder/pack/pack.sh  pack_prepare_client() and p_write_serverinfo()
#                                -- how to turn the shipped client into one that
#                                points at this server without breaking it
#
# so the FreeBSD server and the Docker server cannot disagree about either.
mkdir -p "$HERE/game/rates" "$HERE/client-builder/pack"
PACK_SRC="${PACK_SRC:-$PANEL_SRC/packs/tmp4-r40250.pack}"
if [ -f "$PACK_SRC" ]; then
  cp -a "$PACK_SRC" "$HERE/game/rates/pack.sh"
  cp -a "$PACK_SRC" "$HERE/client-builder/pack/pack.sh"
  info "$(basename "$PACK_SRC") -> game/rates/pack.sh  ($(du -h "$PACK_SRC" | cut -f1))"
  info "$(basename "$PACK_SRC") -> client-builder/pack/pack.sh  (md5 $(md5sum "$PACK_SRC" | cut -c1-32))"
else
  info "WARNING: $PACK_SRC not found -- the panel's rates page will report that"
  info "         these server files cannot have their rates changed, and the"
  info "         client builder will refuse to run."
fi

# -----------------------------------------------------------------------------
say "done"
printf '   total staged: %s\n' "$(du -sh "$HERE" | cut -f1)"
printf '\n   next:\n     cp .env.example .env && nano .env\n     docker compose up -d --build\n\n'
