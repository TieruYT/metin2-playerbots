# syntax=docker/dockerfile:1.6
# =============================================================================
# mt2009 game image -- the martysama0134 r41023 server built for Linux/i386.
#
#  deps    : the 32-bit static dependency set (build-deps-mt2009.sh)
#  builder : the seven modules of the source tree, in the order the package's
#            own top-level Makefile uses; gcc -m32 -std=c++20 instead of clang
#  runtime : (later) the cores, share/, the rendered CONFIGs
#
# The source under src/server is the package's tree with linux-port-mt2009/
# port/linuxify.py applied (the launcher stages it that way); the build refuses
# a tree that has not been through it, because a kqueue libthecore compiles to
# nothing here and the failure would surface as a hundred link errors.
# =============================================================================
ARG BASE_IMAGE=ubuntu:24.04
# An apt mirror to use instead of archive.ubuntu.com, e.g.
# http://pl.archive.ubuntu.com/ubuntu/ -- the default archive crawls from some
# networks and this build installs ~600 MB of toolchain. Empty keeps the
# image's own sources.
ARG APT_MIRROR=

# -----------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS deps

ARG APT_MIRROR
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV M2_EXTERN=/opt/m2extern

# Both Canonical hosts, or apt-get update still hangs on the security pocket
# while the main one answers from the mirror.
RUN if [ -n "${APT_MIRROR}" ]; then \
      sed -i -e "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             -e "s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             /etc/apt/sources.list.d/ubuntu.sources; \
      grep -q "${APT_MIRROR}" /etc/apt/sources.list.d/ubuntu.sources; \
      ! grep -E '^URIs:.*ubuntu\.com' /etc/apt/sources.list.d/ubuntu.sources; \
    fi

COPY build-deps-mt2009.sh /build/build-deps-mt2009.sh
COPY src/extern-tarballs /build/extern-tarballs
COPY src/extern/cryptopp /build/cryptopp

# CRLF guard: this repository is edited on Windows.
RUN sed -i 's/\r$//' /build/build-deps-mt2009.sh && chmod +x /build/build-deps-mt2009.sh

RUN PREFIX="${M2_EXTERN}" BUILD=/build/work TARBALLS=/build/extern-tarballs \
    CRYPTOPP_SRC=/build/cryptopp \
    /build/build-deps-mt2009.sh apt zlib openssl libmd cryptopp mysql devil verify \
 && rm -rf /build/work/src /build/work/stage /build/cryptopp /var/lib/apt/lists/*

# -----------------------------------------------------------------------------
# libs: the support libraries and the db core. A stage of its own so that
# `docker build --target libs` gives an image to iterate the game core in
# (bind-mount game/, run make) while the overlay is being ported.
# -----------------------------------------------------------------------------
FROM deps AS libs

# A job per core ran out of memory on an 8 GB laptop: Docker Desktop gives
# the build VM half the host, and cmd_general.cpp or char.cpp with -O2 -g
# takes over a gigabyte per compiler, so four of them on four gigabytes
# thrashed until the build looked stuck at "game builder 2/3 67%"
# (.unright, 12 September). MAKE_JOBS (M2_MAKE_JOBS in .env) still wins;
# the default is the smaller of the cores and the memory divided by 1400 MB.
# The game core's own step, below, sizes itself by the memory that is free.
ARG MAKE_JOBS=

RUN apt-get update \
 && apt-get install -y --no-install-recommends ccache \
 && rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/lib/ccache:${PATH}"
ENV CCACHE_DIR=/root/.ccache
# Every module Makefile hard-codes clang; the command line wins over the file.
# ENABLE_GCC_AUTODEPEND=0 keeps game/db off the `sed -i ''` BSD-ism in their
# autodepend recipe (a fresh build compiles every file anyway).
ENV M2_MAKE="CC=gcc CXX=g++ ENABLE_GCC_AUTODEPEND=0"

# The Makefiles reach the dependencies as ../../../Extern relative to */src,
# so the tree is laid out exactly like the package: /src/Server, /src/Extern.
WORKDIR /src/Server

COPY src/extern/include /src/Extern/include
RUN set -eux; \
    mkdir -p /src/Extern/lib; \
    cp -a "${M2_EXTERN}/lib/." /src/Extern/lib/; \
    # The shipped mysql headers belong to another connector build; ours must
    # match the archive we link.  IL ships 1.7.8 headers, the archive is 1.8.0.
    rm -rf /src/Extern/include/mysql; \
    cp -a "${M2_EXTERN}/include/mysql" /src/Extern/include/mysql; \
    cp -a "${M2_EXTERN}/include/IL/." /src/Extern/include/IL/; \
    # libsql includes <mysql/mysql.h> through -I/usr/local/include only.
    mkdir -p /usr/local/include; ln -sfn /src/Extern/include/mysql /usr/local/include/mysql; \
    # game/db link /usr/lib32/lib{ssl,crypto}.a by absolute path.
    mkdir -p /usr/lib32; \
    ln -sfn /usr/lib/i386-linux-gnu/libssl.a /usr/lib32/libssl.a; \
    ln -sfn /usr/lib/i386-linux-gnu/libcrypto.a /usr/lib32/libcrypto.a

# Layer 1: the support libraries, built once and cached.
COPY src/server/__REVISION__ __REVISION__
COPY src/server/common common
COPY src/server/libthecore libthecore
COPY src/server/libpoly libpoly
COPY src/server/libgame libgame
COPY src/server/liblua liblua
COPY src/server/libsql libsql

RUN grep -q 'fdwatch_sndbuf_left' libthecore/src/fdwatch.c \
 && grep -q 'defined(__linux__)' libthecore/src/signal.c \
 || { echo "FATAL: the tree has not been through port/linuxify.py"; exit 1; }

RUN --mount=type=cache,target=/root/.ccache set -eux; \
    find . -name '*.o' -delete; \
    find . \( -name Makefile -o -name config -o -name Depend -o -name __REVISION__ \) -exec sed -i 's/\r$//' {} +; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M="$(awk '/MemTotal/ { print int($2 / 1024 / 1400) }' /proc/meminfo)"; [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; echo "make -j$J (cores $(nproc), memory $(awk '/MemTotal/ { print int($2 / 1024) }' /proc/meminfo) MB)"; \
    make -C liblua/5.0 CC=gcc MYCFLAGS="-O2 -g -fno-strict-aliasing"; \
    for m in libsql libgame/src libpoly libthecore/src; do \
      : > "$m/Depend"; \
      make -C "$m" dep ${M2_MAKE}; \
      make -C "$m" -j"$J" ${M2_MAKE}; \
    done; \
    for a in liblua/5.0/lib/liblua.a liblua/5.0/lib/liblualib.a libsql/libsql.a \
             libgame/lib/libgame.a libpoly/libpoly.a libthecore/lib/libthecore.a; do \
      [ -f "$a" ] || { echo "FATAL: $a was not produced"; exit 1; }; \
    done; \
    echo "support archives built"

# Layer 2: the db core. GuildManager.cpp includes one header from the game
# core (../../game/src/locale.hpp, self-contained); copying just that keeps
# this layer cached across game-source edits.
COPY src/server/db db
COPY src/server/game/src/locale.hpp game/src/locale.hpp
RUN --mount=type=cache,target=/root/.ccache set -eux; \
    sed -i 's/\r$//' db/src/Makefile; \
    rm -rf db/src/.obj db/src/Depend; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M="$(awk '/MemTotal/ { print int($2 / 1024 / 1400) }' /proc/meminfo)"; [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; echo "make -j$J (cores $(nproc), memory $(awk '/MemTotal/ { print int($2 / 1024) }' /proc/meminfo) MB)"; \
    make -C db/src -j"$J" ${M2_MAKE}; \
    mkdir -p /out; cp "db/db_r$(cat __REVISION__)" /out/db

# -----------------------------------------------------------------------------
FROM libs AS builder

ARG MAKE_JOBS=
ARG STRIP_BINARIES=1

# Layer 3: the game core. Its job count is sized by what is free, not by the
# VM's total, for two reasons measured on 24 September. playerbot_manager.cpp
# is the whole bot AI in one translation unit and takes about 2.3 GB at
# -O2 -g, where an engine file takes 0.5-0.7 GB (the whole core from nothing:
# 98 s and 3 GB at -j4). And an update builds while the old server still runs
# - `up --build` recreates the containers only once the images are built - so
# the bots and the database hold gigabytes of the same VM while this compiles
# (4.5 GB at 1100 bots). Sized by MemTotal, a laptop swapped until the build
# looked stuck at "game builder 2/3 (67%)" again (Drip, 24 September). So:
# room for the heavy file and 700 MB a job after it, out of MemAvailable, and
# a line in the log when even the heavy file does not fit.
COPY src/server/game game
RUN --mount=type=cache,target=/root/.ccache set -eux; \
    sed -i 's/\r$//' game/src/Makefile; \
    rm -rf game/src/.obj game/src/Depend; \
    T="$(awk '/^MemTotal:/ { print int($2 / 1024) }' /proc/meminfo)"; \
    A="$(awk '/^MemAvailable:/ { print int($2 / 1024) }' /proc/meminfo)"; [ -n "$A" ] && [ "$A" -gt 0 ] || A="$T"; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M=$(( 1 + (A - 2600) / 700 )); [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; \
    echo "make -j$J (cores $(nproc), memory $T MB, free $A MB)"; \
    if [ "$A" -lt 2600 ]; then echo "UWAGA: w maszynie Dockera wolne jest tylko $A MB pamieci, a najciezszy plik rdzenia potrzebuje okolo 2300 MB. Kompilacja moze trwac bardzo dlugo. Zatrzymaj serwer przed aktualizacja (w launcherze: ZATRZYMAJ I ZAPISZ, potem GRAJ albo SPRAWDZ AKTUALIZACJE)."; fi; \
    make -C game/src -j"$J" ${M2_MAKE}; \
    mkdir -p /out; cp "game/game_r$(cat __REVISION__)" /out/game

# Both cores must be 32-bit with a 32-bit time_t: the wire and DB structs
# depend on it.
RUN set -eux; \
    for b in /out/game /out/db; do \
      file -b "$b" | grep -q 'ELF 32-bit LSB .* Intel 80386' \
        || { echo "FATAL: $b is not a 32-bit i386 ELF"; file "$b"; exit 1; }; \
    done; \
    if strings -a /out/game | grep -q '_TIME_BITS=64'; then \
      echo "FATAL: _TIME_BITS=64 leaked into the build"; exit 1; \
    fi; \
    echo "unstripped: $(du -sh /out | cut -f1)"; \
    if [ "${STRIP_BINARIES}" = "1" ]; then strip --strip-unneeded /out/game /out/db; fi; \
    ls -la /out

# -----------------------------------------------------------------------------
# quests: the panel's in-game helper (web_admin.quest), the movement-speed
# bonus and the starter chest, compiled by the tree's own qc into object files
# the cores load at boot. High Risk is NOT on the list on purpose: an update
# never deletes a file, so every 2.0.0-2.0.5 tree still carries
# quest/high_risk.quest, and 2.0.6 - which only removed the file from the
# repository - kept compiling it on every player's machine ("po wgraniu update
# risk mode nadal jest"). The loop is the gate, not the directory. qc ships as a FreeBSD binary in the package's quest
# directory, so it is built here from game/src/quest against the in-tree Lua
# 5.0. The 300-odd stock quests keep the object files they came with; this
# stage produces only its own three and they are copied over as an addition.
# -----------------------------------------------------------------------------
FROM builder AS quests

COPY quest/ /quest-add/
COPY src/serverfiles/share/locale/poland/quest/quest_functions /quest-src/quest_functions

RUN set -eux; \
    mkdir -p /out-quest/poland/quest; \
    if [ ! -f /quest-add/web_admin.quest ]; then \
      echo "note: no web_admin.quest in the build context -- the panel will use"; \
      echo "      its database fallback and Teleport/Running speed will not work."; \
      exit 0; \
    fi; \
    sed -i 's/\r$//' game/src/quest/Makefile; \
    rm -f game/src/quest/qc game/src/quest/*.o; \
    make -C game/src/quest qc CXX="g++ -m32" \
         CPPFLAGS="-g -w -std=c++20 -include cstring -include cstdio -include cstdlib"; \
    test -x game/src/quest/qc; \
    file -b game/src/quest/qc | grep -q 'ELF 32-bit'; \
    out=/out-quest/poland/quest; \
    rm -rf /tmp/qc-work; mkdir -p /tmp/qc-work/object; \
    sed 's/\r$//' /quest-src/quest_functions > /tmp/qc-work/quest_functions; \
    # Functions the engine has and the package's list forgot, which qc would
    # otherwise refuse (gm_profile.quest: equipping, bonus lines, stack size,
    # the item's use path).
    # pc.open_special_shop for fishing_pass_shop (2.0.68); affect.add_new and
    # affect.remove_new for the panel's speed in web_admin (2.0.76).
    for fn in mysql_direct_query pc.equip_slot pc.unequip_slot item.set_attr item.get_max_stack item.use pc.open_special_shop affect.add_new affect.remove_new; do \
      grep -qx "$fn" /tmp/qc-work/quest_functions || echo "$fn" >> /tmp/qc-work/quest_functions; \
    done; \
    # pony_levelup is the package's own stable-keeper quest with the 18-hour
    # wait between two medals taken out (2.0.12); compiled here under its own
    # name, its object files land over the stock ones in the runtime copy.
    # gm_profile brings the admin account's game masters to one profile
    # through the engine (2.0.13).
    # new_quest_lv19 is the package's level-19 quest with its bear group
    # completed: the stock group names 140 and 141 only, so the Cursed Bear
    # (139) and the Cursed Brown Bear (142) never counted towards the five
    # bear skins (Pabloo, from Dixdros' report, 2.0.47).
    # teleport_ring answers the use of Pierscien Teleportacji (70058), which the
    # package drops and no stock quest handled (2.0.48).
    # pony_buy, horse_upgrade, horse_upgrade2 and horse_levelup are the
    # package's stable-keeper quests with the wait for the horse taken out:
    # twelve hours for the pony and each upgrade, twenty-one between two
    # trainings (Tieru, 16 September, 2.0.56).
    # fishing_pass_shop opens the Fisherman's special shop, the only seller
    # of Karta Wedkarska, which no stock quest ever opened (2.0.68).
    # collect_quest_lv85 is the Biologist's red-branch row with its hand-in
    # asking for the right item: the stock quest counted 30166, the level-80
    # row's, so nobody could hand in a single 30167 (2.0.95).
    for q in web_admin speed_boost starter_chest pony_levelup gm_profile new_quest_lv19 teleport_ring pony_buy horse_upgrade horse_upgrade2 horse_levelup deviltower_zone fishing_pass_shop collect_quest_lv85; do \
      [ -f "/quest-add/$q.quest" ] || continue; \
      sed 's/\r$//' "/quest-add/$q.quest" > "/tmp/qc-work/$q.quest"; \
      ( cd /tmp/qc-work && /src/Server/game/src/quest/qc "$q.quest" ); \
      test -s "/tmp/qc-work/object/state/$q" \
        || { echo "FATAL: qc produced no state file for $q"; find /tmp/qc-work/object | head -50; exit 1; }; \
      cp -a "/tmp/qc-work/$q.quest" "$out/$q.quest"; \
    done; \
    test -d /tmp/qc-work/object/notarget/login \
      || { echo "FATAL: qc produced no login handler for web_admin"; exit 1; }; \
    cp -a /tmp/qc-work/object "$out/object"; \
    chmod -R a+rX "$out"; \
    echo "quest objects:"; find "$out/object" -type f | wc -l

# -----------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS runtime

ARG APT_MIRROR
ENV DEBIAN_FRONTEND=noninteractive

RUN if [ -n "${APT_MIRROR}" ]; then \
      sed -i -e "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             -e "s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             /etc/apt/sources.list.d/ubuntu.sources; \
    fi

# What `ldd` resolves on the two binaries: libstdc++, libgcc_s, libc, and -
# because the package's Makefiles link them dynamically (-lz, -lmd) - zlib and
# libmd. tzdata so the logs carry the operator's clock.
# Check-Date off, as in the panel: a Windows clock running behind makes apt
# refuse every Release file as "not valid yet" (Xewi, 19 September). Only this
# stage and the panel carry it - the deps stage is cached on every install that
# has built once, and a changed line there would rebuild every library.
RUN dpkg --add-architecture i386 \
 && apt-get -o Acquire::Check-Date=false update \
 && apt-get install -y --no-install-recommends \
        libc6:i386 libstdc++6:i386 libgcc-s1:i386 zlib1g:i386 libmd0:i386 \
        tzdata ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Non-root: the game parses untrusted network input and needs no privilege
# beyond binding ports above 1024. The entrypoint starts as root only long
# enough to fix ownership of the state volume, then drops to this account.
RUN groupadd -r -g 2000 metin2 && useradd -r -u 2000 -g metin2 -d /opt/metin2 -s /usr/sbin/nologin metin2
# The rate spool is shared with the panel container; a FIXED gid in both
# images is what makes one volume usable from both.
RUN groupadd -r -g 2050 m2spool && usermod -a -G m2spool metin2

# ---- static half: never written at runtime, replaced by rebuilding the image
#      share/bin      the two binaries this build produced
#      share/CMD      the command permission table (a file, not a directory)
#      share/data     mob spawns, dungeons, pc tables
#      share/locale   quest objects, maps, translations (locale/poland)
#      share/package  (empty, kept because every core symlinks it)
#  There is no share/conf: the protos live in the database (PROTO_FROM_DB).
COPY --from=builder /out/game /opt/metin2/share/bin/game
COPY --from=builder /out/db   /opt/metin2/share/bin/db
COPY src/serverfiles/share/CMD     /opt/metin2/share/CMD
COPY src/serverfiles/share/data    /opt/metin2/share/data
COPY src/serverfiles/share/locale  /opt/metin2/share/locale
COPY src/serverfiles/share/package /opt/metin2/share/package

# What the r40250 image adds to its share, for the poland locale here
# (port/shareify.py renders the three files and this step):
#  * mob_drop_item.m3.append.txt - the level-30 weapon ladder's kill groups on
#    the guild map's cursed wolves;
#  * special_item_group.moonlight.txt - the Moonlight chest (50011) the bots
#    open, replacing the stock block (the reader keeps the first group per
#    vnum, so the stock one is cut out first);
#  * special_item_group.starter.txt - the starter chest chain the playerbot
#    seed hands out and the package has no group for.
COPY mob_drop_item.m3.append.txt special_item_group.moonlight.txt special_item_group.starter.txt /tmp/share-add/
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in /tmp/share-add/*.txt; do sed -i 's/\r$//; s/$/\r/' "$f"; done \
 && cat /tmp/share-add/mob_drop_item.m3.append.txt >> "$L/mob_drop_item.txt" \
 && f="$L/special_item_group.txt" \
 && awk 'BEGIN{keep=1} /^Group/{blk=""; keep=1} {blk=blk $0 "\n"} /Vnum[ \t]+(50011|50187|50212|50213|50188|50189|50190|50191|50192|50193|50194|50195|50196)([^0-9]|$)/{keep=0} /^}/{ if (keep) printf "%s", blk; blk=""; keep=1 }' "$f" > "$f.new" \
 && cat /tmp/share-add/special_item_group.moonlight.txt /tmp/share-add/special_item_group.starter.txt >> "$f.new" \
 && mv "$f.new" "$f" \
 && rm -rf /tmp/share-add \
 && echo "share: moonlight + starter chests, M3 drops appended"

# Maska Sabaha goes with the Hwang curse (playerbotify apply_hwang_curse_removed,
# port/shareify.py renders this step): its drop lines and the Hwang loot box roll
# it at nothing - a group stops reading at the first index it lacks, so a line is
# zeroed, not deleted - and the temple's introduction hands none out.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && sed -i -E 's/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/mob_drop_item.txt" \
 && sed -i -E '/^Group[[:blank:]]+LootBox_Hwang/,/^}/ s/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/special_item_group.txt" \
 && sed -i '/^reward_data\.hwang_introduction/,/^}/ { /{72731, 1},/d }' "$L/quest/libs/other/reward_data.lua" \
 && ! grep -E '^[[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+[1-9]' "$L/mob_drop_item.txt" \
 && ! grep -q '{72731, 1}' "$L/quest/libs/other/reward_data.lua" \
 && echo "share: Maska Sabaha removed"

# The quest texts name this world, not Metin2009 (port/shareify.py renders this
# step): the substitution l0st3k's translate.lua of 15 September makes, on both
# copies of the file.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in "$L/translate.lua" "$L/quest/libs/translate/translate.lua"; do \
      [ -f "$f" ] || continue; \
      LC_ALL=C sed -i 's/Metin2009/Metin2 SinglePlayer/g' "$f"; \
      if LC_ALL=C grep -q 'Metin2009' "$f"; then echo "share: Metin2009 left in $f" >&2; exit 1; fi; \
    done \
 && echo "share: Metin2 SinglePlayer in the quest texts"

# The world's difficulty (port/shareify.py renders this step): m2_difficulty.lua
# is dofile'd after the quest libraries and puts the Biologist's wait and the
# stable keeper's waits on event flags the migrator writes from M2_DIFFICULTY.
# It replaces the 2.0.55 sed that made collect_data.is_research_in_progress
# answer false for everybody (namiot_, 15 September): that is what "easy" is.
COPY quest/m2_difficulty.lua /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua
RUN set -eu; L=/opt/metin2/share/locale/poland/quest/libs/other/_otherModuleLoader.lua \
 && LC_ALL=C grep -q 'other/collect_data.lua' "$L" \
 && LC_ALL=C sed -i 's/\r$//' "$L" \
 && printf '\ndofile( LIBDIR .. "other/m2_difficulty.lua")\n' >> "$L" \
 && LC_ALL=C grep -q 'other/m2_difficulty.lua' "$L" \
 && test -s /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua \
 && echo "share: difficulty hooked into the quest libraries"

# The monkey curse (port/shareify.py renders this step): the package's
# monkey_curse quest turned anybody 55 minutes into a Monkey Dungeon (35 on
# 108, 25 on 109) into a monkey and warped them out, bots included. Its event
# handlers go; its state table stays for the flags characters already carry.
RUN set -eu; O=/opt/metin2/share/locale/poland/quest/object \
 && n=$(find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -print | wc -l) \
 && find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -exec rm -f {} + \
 && rm -rf "$O/monkey_curse" \
 && ! find "$O" -path "$O/state" -prune -o -name 'monkey_curse*' -print | grep -q . \
 && echo "share: monkey curse removed ($n handlers)"

# The cores run unprivileged and read this tree by path; a directory only root
# can enter costs every quest below it, silently. Repair, then assert.
RUN set -eu; \
    find /opt/metin2/share -type d ! -perm -005 -exec chmod a+rx {} + ; \
    find /opt/metin2/share -type f ! -perm -004 -exec chmod a+r  {} + ; \
    bad="$(find /opt/metin2/share \( -type d ! -perm -005 \) -o \( -type f ! -perm -004 \) | head -5)"; \
    if [ -n "$bad" ]; then \
      echo "FATAL: still not readable by the server account:"; echo "$bad"; exit 1; \
    fi; \
    echo "share tree is readable by the server account"

# The three quests compiled in the quests stage, as a pure addition on top of
# the package's own object tree (new files under quest/object/, the sources
# beside the stock ones). Empty when web_admin.quest was left out.
COPY --from=quests /out-quest/ /opt/metin2/share/locale/

# Seed for a core's mark/ directory (guild symbols): copied into the state
# volume on first run only.
COPY src/serverfiles/mark-default /opt/metin2/share/mark-default

COPY bin/ /usr/local/bin/
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /opt/metin2/share/bin/game /opt/metin2/share/bin/db

# ---- mutable half: everything the cores write lives here, and only here.
RUN mkdir -p /opt/metin2/var && chown metin2:metin2 /opt/metin2/var
VOLUME ["/opt/metin2/var"]

ENV M2_SHARE_DIR=/opt/metin2/share \
    M2_VAR_DIR=/opt/metin2/var \
    TZ=UTC

EXPOSE 11000/tcp 13000/tcp 13001/tcp 13002/tcp

HEALTHCHECK --interval=15s --timeout=5s --start-period=180s --retries=4 \
    CMD ["/usr/local/bin/m2-healthcheck"]

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["m2-supervise"]
