#!/bin/sh
# =============================================================================
#  fetch-sources.sh -- turn a clean checkout into a buildable Docker context.
#
#  This repository contains the Linux port and nothing else.  The thing the port
#  applies to -- the r40250 Metin2 server source, its runtime data tree and its
#  SQL dumps -- belongs to Ymir/Webzen and to whoever assembled that server-file
#  package.  None of it is here, and none of it ever will be.  The whole of our
#  side of the server source is one 107 KB patch in patches/.
#
#  So a checkout is not buildable on its own, and this is the missing step:
#
#      1. acquire   a compatible upstream archive supplied by the operator
#      2. extract   the pristine source, the share/ data tree, the extern
#                   dependency sources and the SQL dumps
#      3. patch     apply patches/0001-r40250-linux-port.patch to the pristine
#                   source.  Zero fuzz, zero rejects, or this stops.
#      4. stage     lay it out the way docker/prepare-context.sh expects, and
#                   run that, which fills in the Docker build context
#
#  Afterwards:
#      cd linux-port/docker && cp .env.example .env && docker compose up -d --build
#
#  Idempotent.  Each stage is skipped when its output is already there, so a
#  second run costs seconds and a failed run can simply be repeated.
#
#  ---------------------------------------------------------------------------
#  INTERFACE  (this is what installer/install.sh and install.ps1 should call)
#  ---------------------------------------------------------------------------
#  Subcommands:
#      fetch    (default)  do whatever is still needed, then prepare the context
#      status              what has been acquired, extracted, staged
#      check                verify the staged tree; exit non-zero if unusable
#      clean               throw the work tree away (keeps the archive)
#
#  Options -- every one has an environment variable, because an installer
#  usually finds it easier to export than to build an argument list:
#
#      --archive PATH        M2_SRC_ARCHIVE          a file: either the outer
#                                                    zip/rar/7z of the whole
#                                                    server-file folder, or
#                                                    metin2_server+src.tar.gz
#                                                    itself
#      --reference-dir DIR   M2_SRC_REFERENCE_DIR    an already-unpacked
#                                                    "[40250] Reference
#                                                    Serverfile" folder (the one
#                                                    with Server/ in it)
#      --url URL             M2_SRC_URL              an explicit operator-owned
#                                                    download source used when
#                                                    neither local form is given
#      --url-fallback URL    M2_SRC_URL_FALLBACK     the same archive somewhere
#                                                    else, tried when the one
#                                                    above will not come -- a
#                                                    MEGA "509 over quota"
#                                                    above all.
#      --cache DIR           M2_SRC_CACHE            downloads + work
#                                                    (default /var/cache/m2src)
#      --tree DIR            M2_SRC_TREE             staged tree
#                                                    (default $CACHE/tree)
#      --sha256 SUM          M2_SRC_SHA256           expected sum of the archive
#      --force WHAT          M2_SRC_FORCE            redownload | reextract |
#                                                    restage
#      --no-prepare          M2_SRC_NO_PREPARE=1     stop after staging; do not
#                                                    run prepare-context.sh
#      --no-share-bin        M2_SRC_SHARE_BIN=0      skip share/bin (105 MB of
#                                                    FreeBSD binaries that the
#                                                    Docker build never uses)
#      --keep-archive 0|1    M2_SRC_KEEP_ARCHIVE     default 1
#      --mega-user EMAIL     M2_SRC_MEGA_USER        sign in to MEGA, so the
#      --mega-pass PASS      M2_SRC_MEGA_PASS        download is charged to YOUR
#                                                    transfer quota instead of
#                                                    the file owner's.  This is
#                                                    the difference between the
#                                                    link stalling at "509 over
#                                                    quota" here and downloading
#                                                    fine in a logged-in browser.
#      --stall-minutes N     M2_SRC_STALL_MIN        give up on a download that
#                                                    has not grown for N minutes
#                                                    (default 5).  See MEGA note.
#      --yes                 M2_SRC_ASSUME_YES=1     never wait for an answer
#      --quiet               M2_SRC_QUIET=1
#
#  Exit codes -- distinct on purpose, so a caller can tell "the operator needs
#  to fetch a file" from "this repository is broken":
#      0   done
#      2   bad usage
#      3   a required tool is missing from this machine
#      4   the archive could not be obtained (no local copy, download failed)
#      5   the archive is unusable, or does not contain what it must
#      6   the patch did not apply -- the source is not the expected baseline
#      7   not enough disk space
#      8   prepare-context.sh failed
#
#  Nothing here is interactive and nothing here needs a terminal.
#
#  ---------------------------------------------------------------------------
#  A NOTE ON OPTIONAL REMOTE SOURCES
#  ---------------------------------------------------------------------------
#  megatools exits 0 when it refuses a link.  A revoked or malformed share
#  prints "WARNING: Skipping invalid Mega download link" and returns success, so
#  its exit status is worthless: an attempt counts here only when a file really
#  landed.  That check is copied from docker/client-builder/bin/m2-client-build,
#  which learned it first.
#
#  It also does NOT exit when MEGA is refusing to serve.  An anonymous share has
#  a bandwidth quota, and once it is spent every chunk comes back
#  "509 (over quota)"; megatools retries with exponential backoff, forever,
#  having written zero bytes.  Left alone that is an installer that appears to
#  hang for hours.  So the download is watched, and a transfer that has not
#  grown for --stall-minutes is killed and reported as what it is -- a quota
#  wall that clears by itself in a few hours, not a broken link and not a bug in
#  this server.
# =============================================================================
set -u
LC_ALL=C
export LC_ALL

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)

PATCH_FILE="$HERE/patches/0001-r40250-linux-port.patch"
DEPS_SCRIPT="$HERE/build-deps-40250.sh"
PREPARE="$HERE/docker/prepare-context.sh"
CLIENT_DROP="$HERE/docker/client-archive"

# The baseline the patch was generated against. Both are checked before the
# patch is applied, so that "the upstream package changed" is reported as
# exactly that instead of as a pile of rejected hunks.
BASELINE_TARBALL_SHA256=6e9e7339935058f73fead81e609219b496adbc867dfeca70f633031730313001
BASELINE_MANIFEST_SHA256=37fe257e0cb8f7e68e1a9567332ad0dff8ca21f902a0651becd88c6e325a81b8
BASELINE_FILE_COUNT=744
# Reported in issue #5. This identifies the public TMP4 2025-03-31 refresh; it
# does not pretend that the baseline patch is compatible before its dry run.
KNOWN_TMP4_20250331_TARBALL_SHA256=e72d78817432101a9f102ddac74d69b8fe6e54a49b0538e1ef43b9c98e0cc982

# No third-party server-file mirror is built in. The repository distributes
# only its own code and patches; the operator supplies a compatible r40250
# archive/reference directory, or opts into an explicit URL they may use.
DEFAULT_URL=''

ARCHIVE_GIVEN="${M2_SRC_ARCHIVE:-}"
REFDIR="${M2_SRC_REFERENCE_DIR:-}"
URL="${M2_SRC_URL:-$DEFAULT_URL}"
# The same archive somewhere else, tried in order when the one above will not
# come -- above all when MEGA answers "509 over quota", which is a wait of
# hours through nobody's fault. install.sh fills these from artifacts.json.
URL_FALLBACK="${M2_SRC_URL_FALLBACK:-}"
URL_FALLBACK2="${M2_SRC_URL_FALLBACK2:-}"
CACHE="${M2_SRC_CACHE:-/var/cache/m2src}"
TREE="${M2_SRC_TREE:-}"
WANT_SHA="${M2_SRC_SHA256:-}"
FORCE="${M2_SRC_FORCE:-}"
NO_PREPARE="${M2_SRC_NO_PREPARE:-0}"
SHARE_BIN="${M2_SRC_SHARE_BIN:-1}"
KEEP_ARCHIVE="${M2_SRC_KEEP_ARCHIVE:-1}"
# Optional MEGA sign-in. Anonymous downloads are billed to the file owner's
# quota, which is regularly exhausted; signing in bills them to yours instead.
MEGA_USER="${M2_SRC_MEGA_USER:-}"
MEGA_PASS="${M2_SRC_MEGA_PASS:-}"
STALL_MIN="${M2_SRC_STALL_MIN:-5}"
QUIET="${M2_SRC_QUIET:-0}"
MIN_FREE_MB="${M2_SRC_MIN_FREE_MB:-4000}"

CMD=""
while [ $# -gt 0 ]; do
    case "$1" in
        fetch|status|check|clean) CMD="$1"; shift ;;
        --archive)        ARCHIVE_GIVEN="${2:-}"; shift 2 ;;
        --reference-dir)  REFDIR="${2:-}";        shift 2 ;;
        --url)            URL="${2:-}";           shift 2 ;;
        --url-fallback)   URL_FALLBACK="${2:-}";  shift 2 ;;
        --cache)          CACHE="${2:-}";         shift 2 ;;
        --tree)           TREE="${2:-}";          shift 2 ;;
        --sha256)         WANT_SHA="${2:-}";      shift 2 ;;
        --force)          FORCE="${2:-}";         shift 2 ;;
        --stall-minutes)  STALL_MIN="${2:-}";     shift 2 ;;
        --keep-archive)   KEEP_ARCHIVE="${2:-}";  shift 2 ;;
        --mega-user)      MEGA_USER="${2:-}";     shift 2 ;;
        --mega-pass)      MEGA_PASS="${2:-}";     shift 2 ;;
        --no-prepare)     NO_PREPARE=1;           shift ;;
        --no-share-bin)   SHARE_BIN=0;            shift ;;
        --quiet)          QUIET=1;                shift ;;
        --yes)            shift ;;
        -h|--help) sed -n '2,105p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) printf 'fetch-sources: unknown argument: %s\n' "$1" >&2
           printf 'Try --help.\n' >&2; exit 2 ;;
    esac
done
[ -n "$CMD" ] || CMD=fetch

ARCHIVE_DIR="$CACHE/archive"
WORK="$CACHE/work"
[ -n "$TREE" ] || TREE="$CACHE/tree"
META="$CACHE/archive.meta"
STAMP="$TREE/.staged"
LOG="$CACHE/fetch.log"

# ---------------------------------------------------------------------------
#  Talking to the operator
# ---------------------------------------------------------------------------
say()  { [ "$QUIET" = 1 ] || printf '%s [src] %s\n' "$(date -u '+%Y-%m-%d %H:%M:%SZ')" "$*"; }
note() { say "$*"; printf '%s %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$*" >>"$LOG" 2>/dev/null; }
step() { note ""; note "== $*"; }
ok()   { note "  [ ok ] $*"; }
warn() { note "  [ !! ] $*"; }
die() { # die CODE MESSAGE [DETAIL...]
    _c="$1"; shift
    note "  [ XX ] $1"; shift
    for _l in "$@"; do note "         $_l"; done
    exit "$_c"
}

human() {
    awk -v b="${1:-0}" 'BEGIN{
        split("B KB MB GB TB", u, " "); i=1
        while (b >= 1024 && i < 5) { b /= 1024; i++ }
        printf (i==1 ? "%d %s\n" : "%.1f %s\n"), b, u[i]
    }'
}
fsize()   { [ -f "$1" ] || { echo 0; return; }; wc -c < "$1" 2>/dev/null | tr -d ' ' || echo 0; }
have()    { command -v "$1" >/dev/null 2>&1; }

# df needs a path that exists. Every caller here asks about a directory it is
# about to create, so walk up to the nearest one that is really there -- it is
# on the same filesystem, which is the only thing the answer depends on. (Asking
# df about a missing directory prints nothing, which read as "0 MB free" and
# aborted the very first run of this script with a disk-full error on a disk
# with 942 GB on it.)
free_mb() {
    _p="$1"
    while [ ! -d "$_p" ]; do
        _n=$(dirname "$_p")
        [ "$_n" = "$_p" ] && break
        _p="$_n"
    done
    df -Pm "$_p" 2>/dev/null | awk 'END{print $4+0}'
}

check_space() { # check_space NEED_MB DIR WHAT
    _free=$(free_mb "$2")
    # An unreadable df is not evidence of a full disk; carry on rather than
    # refuse to install.
    [ -n "$_free" ] && [ "$_free" -gt 0 ] || return 0
    [ "$_free" -ge "$1" ] && return 0
    die 7 "Not enough disk space for $3." \
        "Needed about $1 MB free on $2; there is $_free MB." \
        "Nothing has been changed."
}

# ---------------------------------------------------------------------------
#  Interruptibility
# ---------------------------------------------------------------------------
# A shell runs no trap while a foreground command is still going. With a 1.6 GB
# download in front, Ctrl-C would do nothing for an hour, so every long step is
# started in the background and waited for.
CHILD_PID=""
supervised() { "$@" & CHILD_PID=$!; wait "$CHILD_PID"; _rc=$?; CHILD_PID=""; return "$_rc"; }
on_signal() {
    [ -n "${CHILD_PID:-}" ] && kill -TERM "$CHILD_PID" 2>/dev/null
    note ""
    warn "Interrupted. Nothing was staged, so the build context is unchanged."
    warn "A download in progress is kept; the next run continues it."
    exit 130
}
trap on_signal INT TERM

# ---------------------------------------------------------------------------
#  Tools
# ---------------------------------------------------------------------------
need_tools() { # need_tools TOOL...
    _missing=""
    for _t in "$@"; do have "$_t" || _missing="$_missing $_t"; done
    [ -z "$_missing" ] || die 3 "This machine is missing:$_missing" \
        "On Debian/Ubuntu:  apt-get install -y$_missing" \
        "(megatools is only needed for a MEGA link; unzip only for the dumps" \
        " and for a .zip outer archive; p7zip-full only for .rar/.7z.)"
}

# ---------------------------------------------------------------------------
#  STAGE 1 -- the upstream archive
# ---------------------------------------------------------------------------
url_is_mega() { case "$1" in *mega.nz*|*mega.co.nz*) return 0 ;; *) return 1 ;; esac; }

# MEGA links come in two flavours:
#   modern : https://mega.nz/file/<id>#<key>      (megatools 1.11+)
#   legacy : https://mega.nz/#!<id>!<key>         (1.9/1.10)
# Same transformation m2-client-build uses.
mega_legacy_url() {
    printf '%s' "$1" | sed -e 's,/file/,/#!,' -e 's,/folder/,/#F!,' \
                           -e 's,#!\([^#]*\)#,#!\1!,' -e 's,#F!\([^#]*\)#,#F!\1!,'
}

finished_archive() { # -> a complete archive in DIR, never a download in progress
    # A download in progress is NOT one of these: megatools keeps it in
    # .megatmp.<id> and renames it only when it is complete, so filtering those
    # names out is what stops a half-downloaded file being taken for the server
    # files. The size floor is there because one of the directories searched is
    # the client builder's drop folder, which has a README in it -- and "found
    # an archive already here: README" is a confusing thing to print at someone.
    _d="${1:-$ARCHIVE_DIR}"
    [ -d "$_d" ] || return 0
    find "$_d" -maxdepth 1 -type f -size +10M \
         ! -name '.megatmp.*' ! -name '*.part' ! -name '*.tmp' \
         ! -name '*.meta' ! -name 'README*' 2>/dev/null | sort | head -1
}
partial_bytes() { # bytes downloaded so far, across any partial file in DIR
    find "$1" -maxdepth 1 -type f 2>/dev/null \
      | while IFS= read -r _f; do fsize "$_f"; done | awk '{s+=$1} END{print s+0}'
}

# The watchdog described at the top of this file. Runs CMD in the background and
# kills it if DIR stops growing for $STALL_MIN minutes.
watched_download() { # watched_download DIR LOGFILE CMD...
    _wd="$1"; _wl="$2"; shift 2
    "$@" >"$_wl" 2>&1 &
    _pid=$!
    CHILD_PID=$_pid
    _stall_s=$(( STALL_MIN * 60 ))
    _last=$(partial_bytes "$_wd"); _flat=0; _t0=$(date +%s)
    while kill -0 "$_pid" 2>/dev/null; do
        sleep 15
        kill -0 "$_pid" 2>/dev/null || break
        _now=$(partial_bytes "$_wd")
        if [ "$_now" -gt "$_last" ]; then
            _flat=0; _last=$_now
        else
            _flat=$(( _flat + 15 ))
        fi
        _el=$(( $(date +%s) - _t0 ))
        [ $(( _el % 60 )) -lt 15 ] && \
            note "        downloaded $(human "$_now"), ${_el}s elapsed"
        if [ "$_flat" -ge "$_stall_s" ]; then
            warn "no progress for $STALL_MIN minutes -- stopping this attempt"
            kill -TERM "$_pid" 2>/dev/null
            sleep 2
            kill -KILL "$_pid" 2>/dev/null
            CHILD_PID=""
            wait "$_pid" 2>/dev/null
            return 124
        fi
    done
    wait "$_pid"; _rc=$?
    CHILD_PID=""
    return "$_rc"
}

quota_advice() {
    note ""
    note "  MEGA answered '509 over quota'. That is the bandwidth cap on an"
    note "  anonymous share, not a dead link and not a fault here. It clears by"
    note "  itself, usually within a few hours. Three ways past it:"
    note ""
    note "    1. Sign in with a MEGA account of your own. The cap is on the"
    note "       OWNER of the share, and signing in charges the transfer to you"
    note "       instead -- which is why the same link often downloads fine in a"
    note "       browser you are logged into while it stalls here:"
    note "         $0 --mega-user you@example.com --mega-pass 'yourpassword'"
    note "    2. Download it in that browser and point this at the file:"
    note "         $0 --archive /path/to/the.zip"
    note "    3. Wait and run this again -- the partial download resumes."
    note "    4. Use any other copy of the same server files:"
    note "         $0 --url https://..."
}

dead_link_advice() {
    note ""
    note "  No Metin2 server-file package is bundled with or published by this"
    note "  project. Supply a compatible r40250 baseline that you may lawfully"
    note "  use. The installer accepts any of these forms:"
    note ""
    note "    already have the archive:   $0 --archive /path/to/archive.zip"
    note "    already unpacked it:        $0 --reference-dir '/path/[40250] Reference Serverfile'"
    note "    have another copy online:   $0 --url https://..."
}

download() { # download URL DIR
    _u="$1"; _d="$2"
    mkdir -p "$_d" || return 1
    _dlog="$CACHE/download.log"

    _part=$(find "$_d" -maxdepth 1 -name '.megatmp.*' -type f 2>/dev/null | head -1)
    [ -n "$_part" ] && note "  resuming an interrupted download ($(human "$(fsize "$_part")") already here)"

    if url_is_mega "$_u"; then
        need_tools megatools
        # An anonymous download is charged to the quota of whoever OWNS the file.
        # These server files sit on somebody else's free account, and that
        # account's allowance runs out regularly -- which is why the same link
        # can hang here at "509 over quota" while it downloads perfectly in a
        # browser: a logged-in browser charges the transfer to YOUR quota
        # instead. So if the operator has a MEGA account, use it.
        #
        # The credentials go into a config file rather than onto the command
        # line: an argument list is world-readable in `ps' for as long as the
        # download runs, and that is an hour.
        _mega_cfg=""
        if [ -n "$MEGA_USER" ] && [ -n "$MEGA_PASS" ]; then
            _mega_cfg="$CACHE/.megarc"
            ( umask 077; printf '[Login]\nUsername = %s\nPassword = %s\n' \
                "$MEGA_USER" "$MEGA_PASS" > "$_mega_cfg" ) || _mega_cfg=""
            [ -n "$_mega_cfg" ] && note "  signing in to MEGA as $MEGA_USER (uses your own transfer quota)"
        fi
        if [ -n "$_mega_cfg" ]; then
            watched_download "$_d" "$_dlog" megatools dl --config "$_mega_cfg" \
                             --no-progress --path "$_d" "$_u"
        else
            watched_download "$_d" "$_dlog" megatools dl --no-progress --path "$_d" "$_u"
        fi
        _rc=$?
        [ -n "$_mega_cfg" ] && rm -f "$_mega_cfg"
        # 9, not 1: the caller may have another copy of this archive to try,
        # and telling somebody to come back in a few hours is wrong -- and
        # discouraging -- when the next line of the list will fetch it now.
        # The advice is printed once, by the caller, after everything failed.
        if grep -q '509\|over quota' "$_dlog" 2>/dev/null && [ -z "$(finished_archive "$_d")" ]; then
            warn "MEGA answered '509 over quota' -- the share's daily allowance."
            return 9
        fi
        # megatools exits 0 on a link it would not even try. Only a file counts.
        [ -n "$(finished_archive "$_d")" ] && return 0
        _legacy=$(mega_legacy_url "$_u")
        if [ "$_legacy" != "$_u" ] && [ "$_rc" != 124 ]; then
            warn "MEGA would not give us that link -- retrying with the older link format"
            watched_download "$_d" "$_dlog" megatools dl --no-progress --path "$_d" "$_legacy"
            [ -n "$(finished_archive "$_d")" ] && return 0
        fi
        tail -5 "$_dlog" 2>/dev/null | while IFS= read -r _l; do note "         $_l"; done
        return 1
    fi

    need_tools curl
    _name=$(basename "$_u" | sed -e 's/[?#].*$//')
    case "$_name" in *.zip|*.rar|*.7z|*.tar.gz|*.tgz) ;; *) _name=m2files.zip ;; esac
    watched_download "$_d" "$_dlog" \
        curl -fsSL --retry 5 --retry-delay 5 -C - -o "$_d/$_name" "$_u" || return 1
    [ -s "$_d/$_name" ]
}

# ---------------------------------------------------------------------------
#  Where the two files we need actually are
# ---------------------------------------------------------------------------
# Everything downstream needs exactly two members of the upstream package:
#     Server/metin2_server+src.tar.gz     source + share/ + extern/
#     Server/metin2_mysql_dump.zip        the five SQL dumps
# They can reach us three ways -- an unpacked folder, the tar.gz on its own, or
# inside the outer archive -- and the rest of this script does not care which.
SRC_TGZ=""
DUMP_ZIP=""

# Is this file metin2_server+src.tar.gz itself, rather than an archive that
# CONTAINS it? Everything in that tarball is under metin2/, so the first handful
# of entries settle it. Do not look for "metin2/src/server" here: the tarball
# lists metin2/server/... first and there are more than a thousand entries
# before src/ appears, so a short head() would say no to the real thing -- which
# is exactly what the first version of this did.
# (And write the "./" prefix as an optional GROUP -- `^\./\?metin2/' is a
# literal dot followed by an optional slash, so it only ever matched a tarball
# whose members really do start "./", which this one does not.)
looks_like_src_tgz() {
    tar tzf "$1" 2>/dev/null | head -20 | grep -qE '^(\./)?metin2/'
}

from_reference_dir() { # from_reference_dir DIR
    _r="$1"
    for _c in "$_r/Server" "$_r"; do
        if [ -f "$_c/metin2_server+src.tar.gz" ]; then
            SRC_TGZ="$_c/metin2_server+src.tar.gz"
            [ -f "$_c/metin2_mysql_dump.zip" ] && DUMP_ZIP="$_c/metin2_mysql_dump.zip"
            return 0
        fi
    done
    # Be forgiving about one extra level of nesting -- an unpacked zip usually
    # has "[40250] Reference Serverfile/" inside the directory you point at.
    _f=$(find "$_r" -maxdepth 3 -type f -name 'metin2_server+src.tar.gz' 2>/dev/null | head -1)
    [ -n "$_f" ] || return 1
    SRC_TGZ="$_f"
    _d=$(find "$_r" -maxdepth 3 -type f -name 'metin2_mysql_dump.zip' 2>/dev/null | head -1)
    [ -n "$_d" ] && DUMP_ZIP="$_d"
    return 0
}

from_outer_archive() { # from_outer_archive FILE  -- pull the two members out
    _a="$1"
    _o="$WORK/upstream"
    mkdir -p "$_o" || return 1

    # The cheapest case first: the operator handed us the source tarball itself.
    # Nothing to unpack, and nothing to copy -- it is read where it lies.
    case "$(printf '%s' "$_a" | tr 'A-Z' 'a-z')" in
        *.tar.gz|*.tgz|*.tar)
            if looks_like_src_tgz "$_a"; then
                note "  this file is metin2_server+src.tar.gz itself"
                SRC_TGZ="$_a"
                # The dumps are a separate zip and are not inside it. If it came
                # out of a real server-file folder its sibling is right there.
                _sib=$(dirname "$_a")/metin2_mysql_dump.zip
                [ -f "$_sib" ] && { DUMP_ZIP="$_sib"; note "  and its sibling metin2_mysql_dump.zip"; }
                return 0
            fi ;;
    esac

    if [ -f "$_o/metin2_server+src.tar.gz" ]; then
        note "  the two members are already unpacked from a previous run"
        SRC_TGZ="$_o/metin2_server+src.tar.gz"
        [ -f "$_o/metin2_mysql_dump.zip" ] && DUMP_ZIP="$_o/metin2_mysql_dump.zip"
        return 0
    fi

    check_space 1200 "$WORK" "unpacking the server archives out of the outer archive"
    note "  taking metin2_server+src.tar.gz and metin2_mysql_dump.zip out of"
    note "  $(basename "$_a") -- 200 MB of copying, a minute or two"

    case "$(printf '%s' "$_a" | tr 'A-Z' 'a-z')" in
        *.zip)
            need_tools unzip
            # -j so the two files land flat regardless of how deep they sit,
            # and the patterns are exact names, never wildcards that could also
            # match the client's source archive.
            supervised unzip -o -j -q "$_a" '*metin2_server+src.tar.gz' -d "$_o" >>"$LOG" 2>&1
            supervised unzip -o -j -q "$_a" '*metin2_mysql_dump.zip'    -d "$_o" >>"$LOG" 2>&1
            ;;
        *.rar|*.7z)
            have 7z || die 3 "$(basename "$_a") is a .rar/.7z and 7z is not installed." \
                "  apt-get install -y p7zip-full p7zip-rar" \
                "Or unpack it yourself and use --reference-dir."
            supervised 7z e -y -bso0 -bsp0 -o"$_o" -r -- "$_a" \
                       metin2_server+src.tar.gz metin2_mysql_dump.zip >>"$LOG" 2>&1
            ;;
        *.tar.gz|*.tgz|*.tar)
            # Not the source tarball -- that was settled above -- so it is a tar
            # that carries it.
            supervised tar xzf "$_a" -C "$_o" --wildcards --no-anchored \
                       'metin2_server+src.tar.gz' 'metin2_mysql_dump.zip' >>"$LOG" 2>&1
            ;;
        *)
            # No useful extension. megatools keeps the remote name, and MEGA
            # serves this share as a zip, so that is the sensible guess.
            if looks_like_src_tgz "$_a"; then
                note "  this file is metin2_server+src.tar.gz itself"
                SRC_TGZ="$_a"; return 0
            fi
            have unzip && supervised unzip -o -j -q "$_a" '*metin2_server+src.tar.gz' '*metin2_mysql_dump.zip' -d "$_o" >>"$LOG" 2>&1
            ;;
    esac

    [ -f "$_o/metin2_server+src.tar.gz" ] || return 1
    SRC_TGZ="$_o/metin2_server+src.tar.gz"
    [ -f "$_o/metin2_mysql_dump.zip" ] && DUMP_ZIP="$_o/metin2_mysql_dump.zip"
    return 0
}

acquire() {
    step "Getting the upstream server files"

    # 1. An unpacked folder. Costs nothing: we read straight out of it.
    if [ -n "$REFDIR" ]; then
        [ -d "$REFDIR" ] || die 4 "--reference-dir $REFDIR is not a directory."
        from_reference_dir "$REFDIR" || die 5 \
            "There is no metin2_server+src.tar.gz under $REFDIR." \
            "Point --reference-dir at the '[40250] Reference Serverfile' folder" \
            "-- the one that has Server/ and Client/ in it."
        ok "using $SRC_TGZ ($(human "$(fsize "$SRC_TGZ")"))"
        [ -n "$DUMP_ZIP" ] && ok "using $(basename "$DUMP_ZIP")"
        return 0
    fi

    # 2. A file the operator named.
    if [ -n "$ARCHIVE_GIVEN" ]; then
        if [ -d "$ARCHIVE_GIVEN" ]; then
            from_reference_dir "$ARCHIVE_GIVEN" || die 5 \
                "$ARCHIVE_GIVEN is a directory but has no metin2_server+src.tar.gz in it."
            ok "using $SRC_TGZ"
            return 0
        fi
        [ -f "$ARCHIVE_GIVEN" ] || die 4 "--archive $ARCHIVE_GIVEN is not a file."
        verify_archive "$ARCHIVE_GIVEN" || die 5 "That archive did not pass its check (above)."
        from_outer_archive "$ARCHIVE_GIVEN" || die 5 \
            "$ARCHIVE_GIVEN does not contain metin2_server+src.tar.gz." \
            "It should be the server-file package -- the archive with Client/ and" \
            "Server/ in it -- or metin2_server+src.tar.gz itself."
        ok "using $SRC_TGZ"
        return 0
    fi

    # 3. Something already downloaded, or dropped next to the client builder's.
    for _d in "$ARCHIVE_DIR" "$CLIENT_DROP"; do
        _c=$(finished_archive "$_d")
        [ -n "$_c" ] || continue
        [ "$FORCE" = redownload ] && { note "  --force redownload: ignoring $_c"; continue; }
        note "  found an archive already here: $_c ($(human "$(fsize "$_c")"))"
        if verify_archive "$_c" && from_outer_archive "$_c"; then
            ok "using $SRC_TGZ"
            return 0
        fi
        warn "that one is not usable -- carrying on"
    done

    # 4. Download.
    [ -n "$URL$URL_FALLBACK$URL_FALLBACK2" ] || { dead_link_advice; die 4 "There is no URL to download from."; }
    check_space "$MIN_FREE_MB" "$CACHE" "downloading and unpacking the server files"
    note "  downloading the server files. This is about 1.7 GB and takes a"
    note "  while -- let it run. Interrupting is safe: it resumes."

    # One archive, up to three places it might be. A half-finished MEGA
    # transfer is thrown away before the next place is asked, because
    # `.megatmp.*' is resume state for one particular link and would otherwise
    # be mistaken for progress against the next one.
    _tried=0; _quota=0; _have=0
    for _u in $URL $URL_FALLBACK $URL_FALLBACK2; do
        case "$_u" in https://*|http://*) ;; *) continue ;; esac
        _tried=$((_tried + 1))
        if [ "$_tried" -gt 1 ]; then
            note ""
            note "  trying another copy of the same archive"
            find "$ARCHIVE_DIR" -maxdepth 1 -name '.megatmp.*' -exec rm -f {} + 2>/dev/null || true
        fi
        note "  from: $_u"
        download "$_u" "$ARCHIVE_DIR"
        _rc=$?
        if [ "$_rc" = 0 ]; then URL="$_u"; _have=1; break; fi
        [ "$_rc" = 9 ] && _quota=1
    done
    if [ "$_have" != 1 ]; then
        [ "$_quota" = 1 ] && quota_advice
        dead_link_advice
        die 4 "Could not download the server files."
    fi

    _got=$(finished_archive "$ARCHIVE_DIR")
    [ -n "$_got" ] || { dead_link_advice; die 4 "The download ended but no archive is in $ARCHIVE_DIR."; }
    ok "downloaded $(basename "$_got") ($(human "$(fsize "$_got")"))"
    {
        printf 'name=%s\n'   "$(basename "$_got")"
        printf 'bytes=%s\n'  "$(fsize "$_got")"
        printf 'url=%s\n'    "$URL"
        printf 'sha256=%s\n' "$(sha256sum "$_got" 2>/dev/null | awk '{print $1}')"
        printf 'when=%s\n'   "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    } > "$META"

    # The client builder wants this same archive. Hardlink it into its drop
    # directory so the whole install is one download rather than two. A symlink
    # would not do: m2-client-build looks for -type f.
    if [ -d "$CLIENT_DROP" ] && [ -z "$(finished_archive "$CLIENT_DROP")" ]; then
        if ln "$_got" "$CLIENT_DROP/$(basename "$_got")" 2>/dev/null; then
            ok "linked into $CLIENT_DROP -- the client builder will not download it again"
        else
            note "  (different filesystem, so the client builder cannot share this copy;"
            note "   copy it to $CLIENT_DROP yourself to save the second download)"
        fi
    fi

    from_outer_archive "$_got" || die 5 \
        "The downloaded archive does not contain metin2_server+src.tar.gz."
    ok "using $SRC_TGZ"
}

verify_archive() { # cheap truth test: big enough, and readable to the end
    _f="$1"
    _b=$(fsize "$_f")
    [ "$_b" -gt 10485760 ] || { warn "$(basename "$_f") is only $(human "$_b") -- too small"; return 1; }
    if [ -n "$WANT_SHA" ]; then
        note "  checking the archive against the checksum you gave..."
        _s=$(sha256sum "$_f" 2>/dev/null | awk '{print $1}')
        [ "$_s" = "$WANT_SHA" ] || { warn "checksum mismatch (got $_s)"; return 1; }
        ok "checksum matches"
    fi
    # A download that stopped half way cannot be listed to its end: a zip's
    # directory is at the end, a tar stream runs out mid-entry. One pass over
    # the file, and it catches the failure that actually happens.
    case "$(printf '%s' "$_f" | tr 'A-Z' 'a-z')" in
        *.tar.gz|*.tgz) tar tzf "$_f" >/dev/null 2>>"$LOG" || {
                            warn "$(basename "$_f") cannot be read to the end -- incomplete or damaged"; return 1; } ;;
        *.zip)          have unzip && { unzip -qt "$_f" >/dev/null 2>>"$LOG" || {
                            warn "$(basename "$_f") cannot be read to the end -- incomplete or damaged"; return 1; }; } ;;
        *)              have 7z && { 7z l -ba -- "$_f" >/dev/null 2>>"$LOG" || {
                            warn "$(basename "$_f") cannot be read to the end"; return 1; }; } ;;
    esac
    return 0
}

# ---------------------------------------------------------------------------
#  STAGE 2 -- extract
# ---------------------------------------------------------------------------
EXTRACT="$WORK/extract"

# Whether the file we are about to spend five minutes unpacking is the one this
# port was made against. It is only a message: a different but equivalent
# package is perfectly allowed, and the authoritative check is the source
# manifest in apply_patch(). Saying it here means the operator finds out before
# the extract rather than after it.
identify_source() {
    _s=$(sha256sum "$SRC_TGZ" 2>/dev/null | awk '{print $1}')
    if [ "$_s" = "$BASELINE_TARBALL_SHA256" ]; then
        ok "this is the exact archive the port was made against"
    elif [ "$_s" = "$KNOWN_TMP4_20250331_TARBALL_SHA256" ]; then
        warn "this is the known TMP4 2025-03-31 source refresh (issue #5)."
        warn "Its source differs from the baseline; a patch dry run will decide compatibility."
    else
        warn "this is not the archive the port was made against."
        warn "  its sha256 is $_s"
        warn "  the known one is $BASELINE_TARBALL_SHA256"
        warn "That is allowed -- the patch is what decides. Carrying on."
    fi
}

extract_all() {
    step "Extracting the source, the game data and the dumps"
    identify_source

    if [ "$FORCE" = reextract ] || [ "$FORCE" = redownload ]; then
        note "  --force $FORCE: discarding what was extracted before"
        rm -rf "$EXTRACT"
    fi

    if [ -d "$EXTRACT/metin2/src/server/libthecore" ] \
       && [ -d "$EXTRACT/metin2/server/share/conf" ]; then
        ok "already extracted"
    else
        mkdir -p "$EXTRACT" || die 1 "Could not create $EXTRACT"
        check_space 1500 "$WORK" "extracting the server files"

        _members="metin2/src/server metin2/src/extern metin2/server/share metin2/server/channel1/first/mark"
        [ "$SHARE_BIN" = 0 ] && note "  --no-share-bin: share/bin (105 MB of FreeBSD binaries) is skipped"

        note "  unpacking $(basename "$SRC_TGZ") ($(human "$(fsize "$SRC_TGZ")")). A few minutes."
        # shellcheck disable=SC2086
        if [ "$SHARE_BIN" = 0 ]; then
            supervised tar xzf "$SRC_TGZ" -C "$EXTRACT" --exclude 'metin2/server/share/bin' $_members
        else
            supervised tar xzf "$SRC_TGZ" -C "$EXTRACT" $_members
        fi
        [ $? -eq 0 ] || die 5 "Unpacking $SRC_TGZ failed. It may be truncated; try --force redownload."
        ok "unpacked $(du -sh "$EXTRACT" 2>/dev/null | cut -f1)"
    fi

    for _p in metin2/src/server/libthecore/src/fdwatch.c \
              metin2/src/extern/boost_1_72_0.tar.gz \
              metin2/server/share/conf metin2/server/share/data \
              metin2/server/share/locale metin2/server/share/package; do
        [ -e "$EXTRACT/$_p" ] || die 5 \
            "$_p is not in the archive." \
            "This does not look like the r40250 server-file package."
    done
    ok "source, extern and share/ are all present"

    # The dumps. Not in the tar.gz -- they are a zip of their own.
    mkdir -p "$EXTRACT/dumps"
    if [ -f "$EXTRACT/dumps/player.sql" ]; then
        ok "SQL dumps already extracted"
    elif [ -n "$DUMP_ZIP" ]; then
        need_tools unzip
        unzip -o -q -j "$DUMP_ZIP" -d "$EXTRACT/dumps" >>"$LOG" 2>&1 \
            || die 5 "Could not unpack $DUMP_ZIP"
        ok "SQL dumps: $(ls "$EXTRACT/dumps"/*.sql 2>/dev/null | wc -l) files, $(du -sh "$EXTRACT/dumps" | cut -f1)"
    else
        die 5 "metin2_mysql_dump.zip was not found next to the source archive." \
            "The database cannot be created without it. It sits in Server/ in" \
            "the server-file package; point --reference-dir at that folder, or" \
            "unzip it yourself into $EXTRACT/dumps."
    fi
    for _d in account common player log hotbackup; do
        [ -f "$EXTRACT/dumps/$_d.sql" ] || die 5 "$_d.sql is missing from the dump archive."
    done
    ok "account, common, player, log and hotbackup are all there"
}

# ---------------------------------------------------------------------------
#  STAGE 3 -- the patch
# ---------------------------------------------------------------------------
apply_patch() {
    step "Applying the Linux port"

    [ -f "$PATCH_FILE" ] || die 1 \
        "$PATCH_FILE is missing from this checkout." \
        "That file IS the port; without it there is nothing to build."
    need_tools patch

    PORTED="$WORK/server"
    rm -rf "$PORTED"
    cp -a "$EXTRACT/metin2/src/server" "$PORTED" || die 1 "Could not copy the source to $PORTED"

    # Is this the tree the patch was made against? Answering before patching
    # turns "38 rejected hunks" into one sentence that says what happened.
    _n=$(find "$PORTED" -type f | wc -l | tr -d ' ')
    _m=$(cd "$PORTED" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | awk '{print $1}')
    note "  baseline: $_n files, manifest $_m"
    if [ "$_m" = "$BASELINE_MANIFEST_SHA256" ]; then
        ok "this is exactly the baseline the patch was generated against"
    else
        warn "this source is NOT the baseline the patch was generated against."
        warn "expected $BASELINE_MANIFEST_SHA256 over $BASELINE_FILE_COUNT files."
        warn "The patch is tried anyway; if it applies cleanly all is well."
    fi

    note "  patch --dry-run first: nothing is written unless every hunk fits"
    if ! ( cd "$PORTED" && patch -p1 --forward --dry-run < "$PATCH_FILE" ) >"$CACHE/patch.log" 2>&1; then
        _source_sum=$(sha256sum "$SRC_TGZ" 2>/dev/null | awk '{print $1}')
        _variant=unknown
        [ "$_source_sum" = "$KNOWN_TMP4_20250331_TARBALL_SHA256" ] \
            && _variant=tmp4-2025-03-31
        _report="$CACHE/compatibility-report.txt"
        { printf 'format=metin2-playerbots-source-compatibility-v1\n'
          printf 'variant=%s\n' "$_variant"
          printf 'source_tarball_sha256=%s\n' "$_source_sum"
          printf 'source_manifest_sha256=%s\n' "$_m"
          printf 'source_file_count=%s\n' "$_n"
          printf 'baseline_tarball_sha256=%s\n' "$BASELINE_TARBALL_SHA256"
          printf 'baseline_manifest_sha256=%s\n' "$BASELINE_MANIFEST_SHA256"
          printf 'port_patch_sha256=%s\n' "$(sha256sum "$PATCH_FILE" 2>/dev/null | awk '{print $1}')"
          printf '\n[patch-dry-run]\n'
          cat "$CACHE/patch.log"
        } > "$_report"
        sed -n '1,80p' "$CACHE/patch.log" | while IFS= read -r _l; do note "         $_l"; done
        warn "Compatibility report: $_report"
        if [ "$_variant" = tmp4-2025-03-31 ]; then
            warn "Recognised TMP4 2025-03-31. Do not force the baseline patch."
            warn "Attach compatibility-report.txt to issue #5 so only the conflicting hunks are adapted."
        fi
        die 6 "The port does not apply to this source." \
            "That means the upstream package is not the one this port was made" \
            "against -- not that the patch is wrong. Do not force it." \
            "The full output is in $CACHE/patch.log and the shareable report is" \
            "in $CACHE/compatibility-report.txt." \
            "The baseline is recorded in the patch's own header:" \
            "  head -30 $PATCH_FILE"
    fi
    ok "dry run clean -- every hunk applies, no fuzz, no rejects"

    ( cd "$PORTED" && patch -p1 --forward < "$PATCH_FILE" ) >>"$CACHE/patch.log" 2>&1 \
        || die 6 "The patch failed while being written, after a clean dry run." \
                 "See $CACHE/patch.log."

    _rej=$(find "$PORTED" -name '*.rej' | wc -l | tr -d ' ')
    [ "$_rej" = 0 ] || die 6 "$_rej hunks were rejected. See the *.rej files under $PORTED."
    find "$PORTED" -name '*.orig' -delete 2>/dev/null

    # The one line that decides whether anybody can log in. Checked here, and
    # again by prepare-context.sh, and again in the Dockerfile.
    _ep=$(grep -c epoll "$PORTED/libthecore/src/fdwatch.c" 2>/dev/null || echo 0)
    [ "$_ep" -ge 20 ] || die 6 \
        "fdwatch.c has only $_ep mentions of epoll after patching -- the FreeBSD" \
        "kqueue event loop is still in there and nothing would ever accept a" \
        "connection. The patch applied but produced the wrong thing."
    grep -q 'return fdwatch_sndbuf_left(fd);' "$PORTED/libthecore/src/fdwatch.c" \
        || die 6 "the fdwatch send-buffer fix is missing after patching -- no client could log in"
    ok "$(grep -c '^patching file' "$CACHE/patch.log") files patched; epoll backend present ($_ep references)"
}

# ---------------------------------------------------------------------------
#  STAGE 4 -- stage into the layout prepare-context.sh expects
# ---------------------------------------------------------------------------
# docker/prepare-context.sh reads a "porting tree" and takes:
#     $M2PORT/build-deps-40250.sh
#     $M2PORT/port40250/server/<module>        the eight modules
#     $M2PORT/port40250/extern/*.tar.gz + include/
#     $M2PORT/server40250/share/{conf,data,locale,package}
#     $M2PORT/server40250/channel1/first/mark/
#     $M2PORT/dbdump/zip/{account,common,player,log,hotbackup}.sql
# This builds exactly that, and nothing else, so the two stay in step.
stage() {
    step "Staging the porting tree"

    if [ "$FORCE" = restage ] || [ "$FORCE" = reextract ] || [ "$FORCE" = redownload ]; then
        rm -rf "$TREE"
    fi
    rm -rf "$TREE/port40250" "$TREE/server40250" "$TREE/dbdump"
    mkdir -p "$TREE/port40250" "$TREE/server40250" "$TREE/dbdump/zip" \
        || die 1 "Could not create $TREE"

    [ -f "$DEPS_SCRIPT" ] || die 1 \
        "$DEPS_SCRIPT is missing from this checkout." \
        "It builds the 32-bit dependency tree; the game image cannot be built" \
        "without it."
    cp -a "$DEPS_SCRIPT" "$TREE/build-deps-40250.sh"
    chmod +x "$TREE/build-deps-40250.sh" 2>/dev/null || true
    note "  build-deps-40250.sh"

    cp -a "$PORTED" "$TREE/port40250/server"
    note "  port40250/server  $(du -sh "$TREE/port40250/server" | cut -f1)"

    # extern: the three dependency tarballs and the shipped headers. extern/lib
    # is deliberately NOT staged -- it holds FreeBSD prebuilt .a files, ELF
    # "version 1 (FreeBSD)", which must never reach a Linux link line.
    mkdir -p "$TREE/port40250/extern"
    find "$EXTRACT/metin2/src/extern" -maxdepth 1 -type f -name '*.tar.gz' \
        -exec cp -a {} "$TREE/port40250/extern/" \;
    [ -d "$EXTRACT/metin2/src/extern/include" ] \
        && cp -a "$EXTRACT/metin2/src/extern/include" "$TREE/port40250/extern/include"
    note "  port40250/extern  $(find "$TREE/port40250/extern" -maxdepth 1 -name '*.tar.gz' | wc -l) tarball(s), $(du -sh "$TREE/port40250/extern" | cut -f1)"

    mkdir -p "$TREE/server40250"
    cp -a "$EXTRACT/metin2/server/share" "$TREE/server40250/share"
    note "  server40250/share $(du -sh "$TREE/server40250/share" | cut -f1)"

    if [ -d "$EXTRACT/metin2/server/channel1/first/mark" ]; then
        mkdir -p "$TREE/server40250/channel1/first"
        cp -a "$EXTRACT/metin2/server/channel1/first/mark" "$TREE/server40250/channel1/first/mark"
        note "  server40250/channel1/first/mark  $(du -sh "$TREE/server40250/channel1/first/mark" | cut -f1)"
    fi

    for _d in account common player log hotbackup; do
        cp -a "$EXTRACT/dumps/$_d.sql" "$TREE/dbdump/zip/$_d.sql"
    done
    note "  dbdump/zip  $(du -sh "$TREE/dbdump/zip" | cut -f1)"

    {
        printf 'when=%s\n'     "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        printf 'source=%s\n'   "$SRC_TGZ"
        printf 'patch=%s\n'    "$PATCH_FILE"
        printf 'patchsum=%s\n' "$(sha256sum "$PATCH_FILE" 2>/dev/null | awk '{print $1}')"
    } > "$STAMP"
    ok "staged into $TREE"
}

check_tree() { # everything prepare-context.sh will demand, checked up front
    _bad=0
    for _p in "$TREE/build-deps-40250.sh" \
              "$TREE/port40250/server" "$TREE/port40250/extern" \
              "$TREE/server40250/share" "$TREE/dbdump/zip"; do
        [ -e "$_p" ] || { warn "missing: $_p"; _bad=1; }
    done
    for _m in common db game libgame liblua libpoly libserverkey libsql libthecore; do
        [ -d "$TREE/port40250/server/$_m" ] || { warn "missing module: $_m"; _bad=1; }
    done
    for _d in conf data locale package; do
        [ -d "$TREE/server40250/share/$_d" ] || { warn "missing: share/$_d"; _bad=1; }
    done
    for _d in account common player log hotbackup; do
        [ -f "$TREE/dbdump/zip/$_d.sql" ] || { warn "missing: dbdump/zip/$_d.sql"; _bad=1; }
    done
    _f="$TREE/port40250/server/libthecore/src/fdwatch.c"
    if [ -f "$_f" ]; then
        _ep=$(grep -c epoll "$_f")
        [ "$_ep" -ge 20 ] || { warn "fdwatch.c is not the ported one ($_ep epoll references)"; _bad=1; }
    else
        warn "missing: libthecore/src/fdwatch.c"; _bad=1
    fi
    return "$_bad"
}

# ---------------------------------------------------------------------------
#  Subcommands
# ---------------------------------------------------------------------------
cmd_fetch() {
    need_tools tar patch find awk sed sha256sum
    mkdir -p "$CACHE" 2>/dev/null
    { : >>"$LOG"; } 2>/dev/null || LOG=/dev/null

    # A staged tree is only current for the patch it was built from. The stamp
    # has recorded that patch's checksum since the beginning and nothing ever
    # read it, so a tree staged from an older port was taken as finished: the
    # checkout updated, the patch in it changed, and the source the image was
    # built from did not. Every C++ change since an install was therefore
    # silently dropped, and the rebuild produced the same binary as before.
    _patch_changed=0
    if [ -f "$STAMP" ] && [ -f "$PATCH_FILE" ]; then
        _was=$(sed -n 's/^patchsum=//p' "$STAMP" 2>/dev/null | head -1)
        _now=$(sha256sum "$PATCH_FILE" 2>/dev/null | awk '{print $1}')
        [ -n "$_was" ] && [ -n "$_now" ] && [ "$_was" != "$_now" ] && _patch_changed=1
    fi

    if [ -f "$STAMP" ] && [ -z "$FORCE" ] && [ "$_patch_changed" = 0 ] \
       && check_tree >/dev/null 2>&1; then
        step "Nothing to do"
        ok "a complete porting tree is already staged in $TREE"
        note "  (--force restage rebuilds it, --force redownload starts over)"
    else
        if [ "$_patch_changed" = 1 ] && [ -z "$FORCE" ]; then
            step "The port has changed"
            note "  The staged tree was built from a different version of the port,"
            note "  so it is rebuilt from the upstream source. Nothing is downloaded"
            note "  again -- the archive and the unpacked copy are kept."
            FORCE=restage
        fi
        acquire
        extract_all
        apply_patch
        stage
        check_tree || die 1 "The staged tree is incomplete (above). This is a bug; please report it."
        ok "the porting tree is complete"

        if [ "$KEEP_ARCHIVE" = 0 ]; then
            _c=$(finished_archive "$ARCHIVE_DIR")
            [ -n "$_c" ] && { rm -f "$_c"; ok "cached archive removed (--keep-archive 0)"; }
        fi
    fi

    if [ "$NO_PREPARE" = 1 ]; then
        note ""
        note "  --no-prepare, so the Docker context was not filled in. Do it with:"
        note "      $PREPARE --m2port $TREE"
        return 0
    fi

    step "Filling in the Docker build context"
    [ -f "$PREPARE" ] || die 1 "$PREPARE is missing from this checkout."
    if have bash; then _sh=bash; else
        die 3 "prepare-context.sh needs bash." "  apt-get install -y bash"
    fi
    "$_sh" "$PREPARE" --m2port "$TREE" || die 8 \
        "prepare-context.sh failed. Its output is above."

    note ""
    note "  Done. The build context is complete:"
    note "      cd $HERE/docker"
    note "      cp .env.example .env      # then edit it"
    note "      docker compose up -d --build"
    return 0
}

cmd_status() {
    say "cache      $CACHE"
    _c=$(finished_archive "$ARCHIVE_DIR")
    if [ -n "$_c" ]; then say "archive    $(basename "$_c")  $(human "$(fsize "$_c")")"
    else
        _p=$(find "$ARCHIVE_DIR" -maxdepth 1 -name '.megatmp.*' -type f 2>/dev/null | head -1)
        if [ -n "$_p" ]; then say "archive    interrupted download, $(human "$(fsize "$_p")") so far (it resumes)"
        else say "archive    not downloaded"; fi
    fi
    if [ -d "$EXTRACT/metin2/src/server" ]; then
        say "extracted  $(du -sh "$EXTRACT" 2>/dev/null | cut -f1)"
    else say "extracted  no"; fi
    if [ -f "$STAMP" ]; then
        say "staged     $TREE  ($(du -sh "$TREE" 2>/dev/null | cut -f1))"
        sed 's/^/           /' "$STAMP"
    else say "staged     no"; fi
    say "patch      $PATCH_FILE"
    [ -f "$PATCH_FILE" ] && say "           $(fsize "$PATCH_FILE") bytes, sha256 $(sha256sum "$PATCH_FILE" | awk '{print $1}')"
    say "free space $(free_mb "$CACHE") MB on the cache"
    return 0
}

cmd_check() {
    if [ ! -f "$STAMP" ]; then
        warn "nothing is staged in $TREE"
        return 1
    fi
    if check_tree; then ok "the staged tree is complete and the source is the ported one"; return 0; fi
    return 1
}

cmd_clean() {
    say "removing the work tree and the staged tree; the downloaded archive is kept"
    rm -rf "$WORK" "$TREE"
    say "done. Run this again to rebuild them without downloading anything."
    return 0
}

case "$CMD" in
    fetch)  cmd_fetch ;;
    status) cmd_status ;;
    check)  cmd_check ;;
    clean)  cmd_clean ;;
esac
