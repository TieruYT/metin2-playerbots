# syntax=docker/dockerfile:1.7
# =============================================================================
#  Metin2 r40250 game server -- 32-bit Linux, built from source.
#
#  Three stages:
#     deps     the 32-bit dependency tree, built by the project's own
#              build-deps-40250.sh.  Depends only on that script and on the
#              shipped tarballs under extern/, so editing the server source
#              never re-runs it.
#     builder  the eight server modules, compiled and linked.  Produces
#              exactly two files: game and db.
#     runtime  ubuntu:24.04 + the three i386 shared libraries the binaries
#              actually need + those two binaries + the static data tree.
#
#  The build context is prepared by ../prepare-context.sh, which stages:
#     src/build-deps-40250.sh   the dependency builder
#     src/extern/               boost/cryptopp/DevIL tarballs + shipped headers
#     src/server/               the eight modules (port40250/server)
#     src/serverfiles/share/    conf/ data/ locale/ package/  (no binaries)
# =============================================================================

ARG BASE_IMAGE=ubuntu:24.04

# =============================================================================
# Stage 1 -- deps
# =============================================================================
FROM ${BASE_IMAGE} AS deps

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Where the 32-bit dependency set is installed.  The module Makefiles find it
# through M2_EXTERN_LINUX, which the builder stage passes on every make line.
ENV M2_EXTERN=/opt/m2extern

# build-deps-40250.sh's own `apt' component runs `dpkg --add-architecture i386'
# and installs the package list documented in its header, so there is no
# separate apt layer to keep in sync with it.
COPY src/build-deps-40250.sh /build/build-deps-40250.sh

# boost 1.72 / cryptopp 8.4.0 / DevIL 1.8.0 are shipped as tarballs inside the
# source tree at port40250/extern; only the MariaDB Connector/C tarball is
# fetched from the network.  extern/include also carries the authoritative
# headers that get overlaid on top of the built ones.
COPY src/extern /build/port40250/extern

# The `lua' component builds a 32-bit mirror of the in-tree forked Lua 5.0.3.
# It is the only part of the server source this stage needs, and it is stable.
COPY src/server/liblua /build/port40250/server/liblua

# CRLF guard: this repository is edited on Windows and a stray \r in a shell
# script is a silent, baffling failure inside a container.
RUN sed -i 's/\r$//' /build/build-deps-40250.sh && chmod +x /build/build-deps-40250.sh

# `verify' is deliberately NOT run here -- it inspects game/src/lzo headers,
# which only exist in the builder stage.  It is run there instead.
RUN PREFIX="${M2_EXTERN}" BUILD=/build/work PORT=/build/port40250 \
    /build/build-deps-40250.sh apt zlib openssl libmd lua cryptopp mysql devil boost \
 && rm -rf /build/work/src /build/work/stage /var/lib/apt/lists/*

# =============================================================================
# Stage 2 -- builder
# =============================================================================
FROM deps AS builder

ARG MAKE_JOBS=
ARG STRIP_BINARIES=1

RUN apt-get update \
 && apt-get install -y --no-install-recommends xutils-dev ccache \
 && rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/lib/ccache:${PATH}"
ENV CCACHE_DIR=/root/.ccache
ENV M2_MAKEVARS="M2_EXTERN_LINUX=/opt/m2extern EXTERN_DIR=/opt/m2extern"

WORKDIR /src/server

# Cache Layer 1: Copy only support libraries and compile them once
COPY src/server/common /src/server/common
COPY src/server/libthecore /src/server/libthecore
COPY src/server/libpoly /src/server/libpoly
COPY src/server/libgame /src/server/libgame
COPY src/server/liblua /src/server/liblua
COPY src/server/libsql /src/server/libsql
COPY src/server/libserverkey /src/server/libserverkey

RUN grep -q 'return fdwatch_sndbuf_left(fd);' libthecore/src/fdwatch.c \
 || { echo "FATAL: the fdwatch send-buffer fix is missing from libthecore/src/fdwatch.c."; exit 1; }

RUN --mount=type=cache,target=/root/.ccache set -eux; \
    find /src/server \( -name '*.o' -o -name '*.a' \) -delete || true; \
    rm -rf libthecore/src/OBJDIR libgame/src/OBJDIR; \
    J="${MAKE_JOBS:-$(nproc)}"; \
    for m in libthecore/src libpoly libgame/src libsql libserverkey; do \
      make -C "$m" dep ${M2_MAKEVARS} >/dev/null 2>&1 || true; \
    done; \
    make -C libthecore/src -j"$J" ${M2_MAKEVARS}; \
    make -C libpoly        -j"$J" ${M2_MAKEVARS}; \
    make -C libgame/src    -j"$J" ${M2_MAKEVARS}; \
    make -C liblua                ${M2_MAKEVARS}; \
    make -C libsql         -j"$J" ${M2_MAKEVARS}; \
    make -C libserverkey   -j"$J" ${M2_MAKEVARS}; \
    for a in libthecore/lib/libthecore.a libpoly/libpoly.a libgame/lib/libgame.a \
             liblua/lib/liblua.a liblua/lib/liblualib.a libsql/libsql.a \
             libserverkey/libserverkey.a; do \
      [ -f "$a" ] || { echo "FATAL: $a was not produced"; exit 1; }; \
    done; \
    echo "all seven support archives present and cached"

# Cache Layer 2: Copy and build db core
COPY src/server/db /src/server/db
RUN --mount=type=cache,target=/root/.ccache set -eux; \
    rm -rf db/src/.obj; \
    find db/src -name '*.o' -delete || true; \
    mkdir -p /out; \
    J="${MAKE_JOBS:-$(nproc)}"; \
    make -C db/src -j"$J" ${M2_MAKEVARS} DBDIR=/out

# Cache Layer 3: Copy and build game core
COPY src/server/game /src/server/game
RUN set -eux; \
    ln -sfn /src/server/game /build/port40250/server/game; \
    PREFIX="${M2_EXTERN}" BUILD=/build/work PORT=/build/port40250 \
      /build/build-deps-40250.sh verify

RUN --mount=type=cache,target=/root/.ccache set -eux; \
    rm -rf game/src/OBJDIR; \
    find game/src -name '*.o' -delete || true; \
    J="${MAKE_JOBS:-$(nproc)}"; \
    mkdir -p /out; \
    make -C game/src -j"$J" ${M2_MAKEVARS}; \
    make -C game/src install GAMEDIR=/out ${M2_MAKEVARS}

# Both binaries must be 32-bit, and sizeof(time_t) must still be 4 -- a 64-bit
# time_t under -m32 silently changes the size of packed wire and DB structs.
# _TIME_BITS is checked rather than assumed because it can be injected from a
# CFLAGS override as easily as from a Makefile edit.
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
    echo "delivered:  $(du -sh /out | cut -f1)"; \
    ls -la /out

# =============================================================================
# Stage 2b -- quests
#
#  The admin panel's in-game helper, web_admin.quest, compiled into every
#  locale the data tree carries.
#
#  Why it needs a stage of its own: a .quest file is not read by the server, it
#  is compiled by `qc' into a tree of small files under quest/object/ that the
#  cores load at boot.  qc ships with the server files as a FreeBSD i386 binary
#  which cannot run here, so it is built from the same source tree the cores are
#  built from (game/src/quest/qc.cc) -- which is why this stage sits on top of
#  `builder' and not somewhere cheaper.
#
#  Nothing about the stock quests changes.  qc is run in a scratch directory
#  with an EMPTY object/, so the tree this stage produces contains only
#  web_admin's own output and is copied over the runtime's locale tree as a pure
#  addition.  The 237 quests the pack ships keep the object files they came
#  with, byte for byte.
# =============================================================================
FROM builder AS quests

# The panel's helper. Staged by ../prepare-context.sh from files/web_admin.quest.
# Optional: an operator who does not want it deletes it from the context and
# everything else here becomes a no-op.
COPY quest/ /quest-add/

# The stock data tree, for its quest_functions (the list of names qc will
# accept) and for the locale layout.
COPY src/serverfiles/share/locale /quest-src/locale

#  What the RUN below does, in order -- kept here rather than as comments
#  inside it, because a `#' line inside a backslash-continued RUN is a way to
#  lose the rest of the command:
#
#    1. No quest in the context -> produce an empty /out-quest and stop. The
#       runtime copies nothing and the panel falls back to the database.
#    2. Delete the shipped FreeBSD qc and build our own from qc.cc. Its
#       Makefile hardcodes the porting host's compiler name, so CPP is
#       overridden on the command line rather than the Makefile edited -- that
#       file is upstream's and stays that way. -m32 because liblua in this
#       stage is 32-bit, like everything else here.
#
#       qc.cc calls strlen/strcmp without including <cstring>; it built in 2019
#       because the headers of the day dragged it in for free, and it does not
#       under GCC 13. CPPFLAGS force-includes the three headers rather than
#       adding #include lines to a file that is not ours -- same effect, and
#       nothing about the upstream source has to change. -w silences the
#       decades of warnings, which are not news and not ours to fix.
#    3. For each locale: qc reads `quest_functions' from its working directory
#       and refuses any name not listed there ("Calls undeclared function!"),
#       so mysql_direct_query is declared before the compile.
#    4. Compile in a scratch directory with an empty object/, so the result is
#       only web_admin's own output.
#    5. qc exits 0 whether or not it wrote anything, so the outputs that must
#       exist are checked by hand. A silent no-op here would look exactly like
#       a working build and fail only at runtime. Both timer handlers are
#       checked: the server timer that ages rows out, and the PLAYER timer
#       that carries the five actions out. The offline path can be tested
#       without a game client and the acting path cannot, so a missing player
#       timer would pass every check an operator can run.
#    6. locale_list gets the file appended: that is what make.py walks when an
#       operator recompiles the tree by hand, and leaving it out would silently
#       drop the helper the next time they do.
#    7. The output tree is made world-readable, and that is not cosmetic. qc
#       creates object/state with mode 0700 (S_IRWXU), and the runtime COPY
#       below lands this tree ON TOP of the stock one -- so an unchmodded
#       object/state replaces the pack's readable directory with a root-only
#       one, and the server, which runs as an unprivileged account, then loads
#       NO quests at all. Every core still starts and still listens, so
#       nothing looks wrong until a player clicks an NPC. Hence the explicit
#       chmod and the assertion under it.
RUN set -eux; \
    mkdir -p /out-quest; \
    if [ ! -f /quest-add/web_admin.quest ]; then \
      echo "note: no web_admin.quest in the build context -- the panel will use"; \
      echo "      its database fallback and Teleport/Running speed will not work."; \
      mkdir -p /out-quest; exit 0; \
    fi; \
    rm -f game/src/quest/qc; \
    make -C game/src/quest CPP="g++ -m32" \
         CPPFLAGS="-g -w -include cstring -include cstdio -include cstdlib"; \
    test -x game/src/quest/qc; \
    file -b game/src/quest/qc | grep -q 'ELF 32-bit'; \
    for questdir in /quest-src/locale/*/quest; do \
      [ -d "$questdir" ] || continue; \
      lang="$(basename "$(dirname "$questdir")")"; \
      out="/out-quest/$lang/quest"; \
      rm -rf /tmp/qc-work; \
      mkdir -p "$out" /tmp/qc-work/object; \
      cp "$questdir/quest_functions" /tmp/qc-work/quest_functions; \
      grep -qx 'mysql_direct_query' /tmp/qc-work/quest_functions \
        || echo 'mysql_direct_query' >> /tmp/qc-work/quest_functions; \
      sed 's/\r$//' /quest-add/web_admin.quest > /tmp/qc-work/web_admin.quest; \
      ( cd /tmp/qc-work && /src/server/game/src/quest/qc web_admin.quest ); \
      test -s /tmp/qc-work/object/state/web_admin \
        || { echo "FATAL: qc produced no state file for web_admin ($lang)"; \
             find /tmp/qc-work/object; exit 1; }; \
      test -d /tmp/qc-work/object/notarget/login \
        || { echo "FATAL: qc produced no login handler for web_admin ($lang)"; exit 1; }; \
      find /tmp/qc-work/object -type d -name server_timer | grep -q . \
        || { echo "FATAL: qc produced no server_timer handler for web_admin ($lang)"; \
             find /tmp/qc-work/object; exit 1; }; \
      find /tmp/qc-work/object -type d -name timer | grep -q . \
        || { echo "FATAL: qc produced no player-timer handler for web_admin ($lang) --"; \
             echo "       that is the handler the five actions run in; without it the"; \
             echo "       helper would only ever report players offline"; \
             find /tmp/qc-work/object; exit 1; }; \
      if [ -f /quest-add/speed_boost.quest ]; then \
        sed 's/\r$//' /quest-add/speed_boost.quest > /tmp/qc-work/speed_boost.quest; \
        ( cd /tmp/qc-work && /src/server/game/src/quest/qc speed_boost.quest ); \
        test -s /tmp/qc-work/object/state/speed_boost \
          || { echo "FATAL: qc produced no state file for speed_boost ($lang)"; \
               find /tmp/qc-work/object; exit 1; }; \
        cp -a /tmp/qc-work/speed_boost.quest "$out/speed_boost.quest"; \
      fi; \
      if [ -f /quest-add/high_risk.quest ]; then \
        sed 's/\r$//' /quest-add/high_risk.quest > /tmp/qc-work/high_risk.quest; \
        ( cd /tmp/qc-work && /src/server/game/src/quest/qc high_risk.quest ); \
        test -s /tmp/qc-work/object/state/high_risk \
          || { echo "FATAL: qc produced no state file for high_risk ($lang)"; \
               find /tmp/qc-work/object; exit 1; }; \
        test -d /tmp/qc-work/object/20354/chat \
          || { echo "FATAL: qc produced no Guardian chat handler for high_risk ($lang) --"; \
               echo "       that is the only way a player can leave High Risk again"; \
               find /tmp/qc-work/object; exit 1; }; \
        cp -a /tmp/qc-work/high_risk.quest "$out/high_risk.quest"; \
      fi; \
      cp -a /tmp/qc-work/object "$out/object"; \
      cp -a /tmp/qc-work/web_admin.quest "$out/web_admin.quest"; \
      cp -a /tmp/qc-work/quest_functions "$out/quest_functions"; \
      cp -a "$questdir/locale_list" "$out/locale_list"; \
      grep -qx 'web_admin.quest' "$out/locale_list" || echo 'web_admin.quest' >> "$out/locale_list"; \
      if [ -f /quest-add/speed_boost.quest ]; then \
        grep -qx 'speed_boost.quest' "$out/locale_list" \
          || echo 'speed_boost.quest' >> "$out/locale_list"; \
      fi; \
      if [ -f /quest-add/high_risk.quest ]; then \
        grep -qx 'high_risk.quest' "$out/locale_list" \
          || echo 'high_risk.quest' >> "$out/locale_list"; \
      fi; \
      find "$out" -type d -exec chmod 0755 {} +; \
      find "$out" -type f -exec chmod 0644 {} +; \
      [ "$(stat -c '%a' "$out/object/state")" = "755" ] \
        || { echo "FATAL: $out/object/state is not world-readable; the game user could not load any quest"; exit 1; }; \
      echo "compiled quests for locale '$lang':"; \
      find "$out/object" -type f | sed 's|^|    |'; \
      rm -rf /tmp/qc-work; \
    done

# =============================================================================
# Stage 3 -- runtime
# =============================================================================
FROM ${BASE_IMAGE} AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# The binaries link everything else statically.  `ldd game' resolves exactly
# these three, plus the loader:
#     libstdc++.so.6  libgcc_s.so.1  libc.so.6
# tzdata is here so the server's log timestamps match the operator's clock;
# everything else the entrypoint and supervisor need (bash, coreutils,
# util-linux/setpriv) is already in the base image.
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
        libc6:i386 libstdc++6:i386 libgcc-s1:i386 \
        tzdata ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Non-root: the game parses untrusted network input and needs no privilege
# beyond binding ports above 1024.  The entrypoint starts as root only long
# enough to fix ownership of the state volume, then drops to this account.
RUN groupadd -r -g 2000 metin2 && useradd -r -u 2000 -g metin2 -d /opt/metin2 -s /usr/sbin/nologin metin2

# The rate spool is the one directory two containers write to.  They run as
# different users (metin2 here, panel there) and always will, so a shared group
# with a FIXED gid in both images is what makes the volume usable from both --
# a name alone would not, since the kernel only sees the number.
RUN groupadd -r -g 2050 m2spool && usermod -a -G m2spool metin2

# ---- static half: never written at runtime, replaced by rebuilding the image
#      share/bin      the two binaries this build produced
#      share/conf     CMD, item_proto/mob_proto/item_names/mob_names
#      share/data     mob spawns, dungeons, pc tables
#      share/locale   quest objects, maps, translations
#      share/package  (empty in this fork, kept because every core symlinks it)
COPY --from=builder /out/game /opt/metin2/share/bin/game
COPY --from=builder /out/db   /opt/metin2/share/bin/db
COPY src/serverfiles/share/conf    /opt/metin2/share/conf
COPY src/serverfiles/share/data    /opt/metin2/share/data
COPY src/serverfiles/share/locale  /opt/metin2/share/locale
COPY src/serverfiles/share/package /opt/metin2/share/package

# The admin panel's in-game helper, compiled in stage 2b. A pure addition on
# top of the line above: new files under quest/object/, the quest source next
# to the 300 that ship with the pack, and two list files (quest_functions,
# locale_list) that gain one line each. No stock quest object is touched.
#
# Empty when the operator removed web_admin.quest from the build context, in
# which case this COPY is a no-op and the panel keeps its database fallback.
COPY --from=quests /out-quest/ /opt/metin2/share/locale/

# The cores run as an unprivileged account and read this tree by path. A single
# directory in it that root alone can enter costs every quest below that point,
# silently -- the server starts, listens, and accepts players who then find that
# no NPC does anything at all.
#
# It is not hypothetical in either direction. quest/object/state arrives mode
# 0700 from qc (which is why stage 2b chmods its own output), and the pack's own
# make.py ends with `chmod -R 770 object', so a context staged on a filesystem
# that keeps Unix modes -- i.e. any Linux host -- carries 0770 root-owned
# directories in from upstream too. On Windows the bind mount hides that by
# presenting everything as 0755, which is the only reason it has not bitten yet.
#
# So: repair, then assert. `find ... ! -perm' touches only what is actually
# wrong, so on a context that is already fine this adds an empty layer rather
# than a second copy of an 84 MB tree.
RUN set -eu; \
    find /opt/metin2/share -type d ! -perm -005 -exec chmod a+rx {} + ; \
    find /opt/metin2/share -type f ! -perm -004 -exec chmod a+r  {} + ; \
    bad="$(find /opt/metin2/share \( -type d ! -perm -005 \) -o \( -type f ! -perm -004 \) | head -5)"; \
    if [ -n "$bad" ]; then \
      echo "FATAL: still not readable by the server account:"; echo "$bad"; exit 1; \
    fi; \
    echo "share tree is readable by the server account"

# Seed for a core's mark/ directory (guild symbols).  Copied into the state
# volume on first run only, so guild marks uploaded by players survive.
COPY src/serverfiles/mark-default /opt/metin2/share/mark-default

# The server-files profile -- the same file the FreeBSD installer uses.  It is
# in here for exactly one function, pack_apply_rates(), which knows which
# columns of which tables carry experience, yang and drop chances.  Copying it
# rather than reimplementing it is the point: there is then only one piece of
# code in the project that scales a rate, and the FreeBSD server and this one
# cannot drift apart.  Staged into the build context by ../prepare-context.sh.
COPY rates/ /opt/metin2/rates/

COPY bin/ /usr/local/bin/
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /opt/metin2/share/bin/game /opt/metin2/share/bin/db \
 && if [ -f /opt/metin2/rates/pack.sh ]; then sed -i 's/\r$//' /opt/metin2/rates/pack.sh; fi

# ---- mutable half: everything the cores write lives here, and only here.
#      Declared as a VOLUME so an operator who forgets the compose file still
#      does not lose guild marks and logs on `docker rm'.
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
