#!/bin/sh
# =============================================================================
#  Metin2 server -- one-command installer for Linux.
#
#      curl -fsSL https://example.com/install.sh | sh
#
#  ...or, if you want to answer no questions:
#
#      curl -fsSL https://example.com/install.sh | sh -s -- --yes
#
#  This assumes you are on a SERVER and that you want other people to be able
#  to connect and play. It publishes the game ports on every network interface
#  and opens the firewall for them. If you want a private server that only you
#  can reach -- on your own PC -- use the Windows installer instead, or pass
#  --local here.
#
#  What it does, in order:
#
#     1. checks the machine can actually run this (CPU type, memory, disk)
#     2. installs Docker if it is not already there
#     3. assembles the server: fetches the upstream server-file package,
#        applies the Linux port to it and builds the Docker context
#     4. invents a password for the admin panel and two for the database
#     5. builds and starts the server
#     6. opens the firewall
#     7. sets up HTTPS, if you gave it a domain name
#     8. starts building the game client for your players to download
#     9. prints the three things you need: client link, panel link, password
#
#  It is safe to run twice. If a server is already installed it says so and
#  offers to leave your accounts and characters alone.
#
#  Every secret is generated on this machine, at the moment you run this.
#  Nothing is baked in, and nothing is sent anywhere.
#
#  ---------------------------------------------------------------------------
#  This script is deliberately written so that a HALF-DOWNLOADED copy cannot
#  do anything: everything lives inside a shell function, and the only line
#  that actually runs anything is the very last one. If the download is cut
#  short the shell hits a parse error and stops, having changed nothing.
#  ---------------------------------------------------------------------------
# =============================================================================

set -eu

# -----------------------------------------------------------------------------
#  Where the server comes from.
#
#  There is no release archive, and there never will be one. The repository
#  holds the Linux port and nothing else -- one 109 KB patch touching 28 files,
#  plus the scripts that turn a checkout into something buildable. Everything
#  copyrighted (the game source, the runtime data tree, the SQL dumps) belongs
#  to Ymir/Webzen and to whoever assembled the r40250 server-file package, and
#  it is not ours to redistribute.
#
#  So the server is assembled here, on this machine, in two moves:
#
#     1. get the repository -- a git clone of a few megabytes -- unless a
#        checkout is already on this machine
#     2. run linux-port/fetch-sources.sh with an operator-supplied r40250
#        archive/reference directory. It extracts the source, share/ data tree
#        and SQL dumps, applies the port, and fills the Docker build context
#
#  Override any of it:
#      M2_REPO_URL=https://.../server.git            sh install.sh
#      M2_REPO_DIR=/path/to/checkout                 sh install.sh
#      M2_SRC_REFERENCE_DIR=/path/to/serverfiles     sh install.sh
#      M2_SRC_ARCHIVE=/path/to/serverfiles.zip       sh install.sh
#      M2_LOCAL_CONTEXT=/path/to/linux-port/docker   sh install.sh
# -----------------------------------------------------------------------------
M2_REPO_URL="${M2_REPO_URL:-https://github.com/TieruYT/metin2-playerbots.git}"
# Where this script itself lives. The panel shows it as the way to update, so
# an operator who fetched this from somewhere else gets told to go back there.
M2_INSTALLER_URL="${M2_INSTALLER_URL:-https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh}"
M2_REPO_DIR="${M2_REPO_DIR:-}"
M2_LOCAL_CONTEXT="${M2_LOCAL_CONTEXT:-}"

# Handed straight to fetch-sources.sh, which knows all of these names itself --
# so they only have to be exported, never translated into an argument list.
M2_SRC_ARCHIVE="${M2_SRC_ARCHIVE:-}"
M2_SRC_REFERENCE_DIR="${M2_SRC_REFERENCE_DIR:-}"
M2_SRC_URL="${M2_SRC_URL:-}"
# The same server archive elsewhere, filled from artifacts.json below and tried
# only after M2_SRC_URL has already failed -- a MEGA "509 over quota" above all.
M2_SRC_URL_FALLBACK="${M2_SRC_URL_FALLBACK:-}"
M2_SRC_URL_FALLBACK2="${M2_SRC_URL_FALLBACK2:-}"
M2_SRC_CACHE="${M2_SRC_CACHE:-/var/cache/m2src}"

# Where this script is, if it is a file at all. Run as `curl ... | sh' it is
# not, and $0 is then the name of the shell -- which must not be mistaken for a
# path into a checkout.
SELF_DIR=""
case "${0:-}" in
    ""|-*|sh|bash|dash|ash|/bin/sh|/bin/bash|/bin/dash) : ;;
    *) [ -f "$0" ] && SELF_DIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd) ;;
esac

# A working directory that was deleted underneath us. This is not a strange
# edge case: the uninstall instructions have you `cd /opt/metin2/stack' to take
# a database backup, and `rm -rf /opt/metin2' a few steps later. Run the
# installer from that same shell and it starts in a directory that no longer
# has a path.
#
# Nothing looks wrong. Linux keeps the directory's inode alive for as long as a
# process stands in it, so `pwd' prints the old name out of the shell's own
# cache and even `[ -d . ]' succeeds -- both were measured doing exactly that.
# Only getcwd() fails, and the first symptom used to be git giving up three
# minutes into the run with "Unable to read current working directory", which
# reads like a network fault and is not one.
#
# `pwd -P' is the portable way to put the question to getcwd() directly, but it
# has to be asked by its OUTPUT and not by its exit status. Measured in all
# three shells with the directory gone: dash and sh print the error and still
# exit 0; only bash exits 1. Stdout is empty in every one of them, whether this
# shell deleted the directory itself or inherited it from the one that did.
# `cd .' is no good either -- it fails in sh and dash but succeeds in bash --
# and `[ -d . ]' succeeds everywhere, because the inode outlives the name for
# as long as a process stands in it.
#
# Nothing here needs the starting directory: every path is absolute by the time
# it is used. So moving to / costs nothing and rescues the run.
if [ -z "$(pwd -P 2>/dev/null)" ]; then
    printf '\n  The directory you started this from has been deleted.\n' >&2
    printf '  That is harmless -- carrying on from / instead.\n\n' >&2
    cd / || exit 1
fi

# Defaults. Every one of these can be changed with a command-line flag.
INSTALL_DIR="${M2_INSTALL_DIR:-/opt/metin2/stack}"
PUBLIC_ADDRESS="${M2_PUBLIC_ADDRESS:-}"
DOMAIN="${M2_DOMAIN:-}"
TLS_EMAIL="${M2_TLS_EMAIL:-}"
DROP_DOMAIN=0   # --no-domain: forget the domain an earlier run set up
AUTH_PORT="${M2_AUTH_PORT:-11000}"
GAME_PORTS="${M2_GAME_PORT_RANGE:-13000-13002}"
PANEL_PORT="${M2_PANEL_PUBLIC_PORT:-7788}"
# Migrate values written by older installers (127.0.0.1:7788) and even the
# doubly-prefixed malformed form reported in Issue #2. From here onward a port
# is always digits only; its bind address has a separate environment key.
PANEL_PORT="${PANEL_PORT##*:}"
# The WebSocket bridge for the browser client. Off unless the operator turns it
# on in .env, and this installer never turns it on by itself: it is only useful
# once a browser client has been put on the panel's volume, and until then it
# would be a port open for nothing.
BRIDGE_PORT="${M2_BRIDGE_PORT:-7789}"
BROWSER_PLAY=0

# Which clients this install offers -- decided by choose_clients(): from flags
# if any were passed, from what is already installed when this is an update,
# otherwise from one question. WANT_WEB_FLAG stays empty unless the operator
# actually passed --web-client or --no-web-client, so "never asked" and
# "answered no" remain distinguishable; without that an update could not tell a
# deliberate no from a default.
WANT_WEB=0
WANT_DESKTOP=1
WANT_WEB_FLAG=""
HAVE_WEB=0
HAVE_DESKTOP=0

# The Custom Experience -- see choose_custom_experience(). Empty for the same
# reason WANT_WEB_FLAG is: it has to stay possible to tell "never said" from
# "said no", because on an update the first means "keep what this server has"
# and the second means "take it back out".
CUSTOM_EXPERIENCE_FLAG=""

# Snapshotted before the command line is read, so replay_command() can tell an
# option somebody actually gave from a default that happens to equal it. Taken
# from the variables rather than written out again, so changing a default above
# cannot leave a stale copy down here.
DEF_INSTALL_DIR="$INSTALL_DIR"
DEF_AUTH_PORT="$AUTH_PORT"
DEF_GAME_PORTS="$GAME_PORTS"
DEF_PANEL_PORT="$PANEL_PORT"
ASSUME_YES=0
DRY_RUN=0
LOCAL_ONLY=0
SKIP_CLIENT=0
SKIP_FIREWALL=0

# Filled in as we go.
OS_NAME="unknown"
OS_VERSION=""
OS_FAMILY="unknown"
FIREWALL="none"
PANEL_BIND="0.0.0.0"
FRESH_INSTALL=1
PANEL_PASSWORD=""
PANEL_PASSWORD_KNOWN=1
PANEL_PASSWORD_NEW=1
PANEL_PASSWORD_CHOSEN=0   # the operator picked this one in the panel itself
CLIENT_STATE="unavailable"
CLIENT_LOG=""
NGINX_ACCEL="no"
TMPDIR_SELF=""
REPO_DIR=""

# =============================================================================
#  Talking to the human
# =============================================================================

ui_init() {
    if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
        C_RESET=$(printf '\033[0m')
        C_BOLD=$(printf '\033[1m')
        C_DIM=$(printf '\033[2m')
        C_RED=$(printf '\033[31m')
        C_GREEN=$(printf '\033[32m')
        C_YELLOW=$(printf '\033[33m')
        C_CYAN=$(printf '\033[36m')
    else
        C_RESET='' C_BOLD='' C_DIM='' C_RED='' C_GREEN='' C_YELLOW='' C_CYAN=''
    fi
}

# All of these write to stdout, warnings and errors included.
#
# That is on purpose. This script narrates a long install, and the order the
# lines appear in is part of what it is telling you. Sending warnings to stderr
# would scramble that the moment anyone pipes the output -- `curl ... | sh |
# tee install.log' -- because the two streams are buffered differently and the
# warnings would surface in the wrong place. Nothing consumes this script's
# stdout, so there is nothing to keep clean.
say()   { printf '  %s\n' "$*"; }
info()  { printf '  %s%s%s\n' "$C_DIM" "$*" "$C_RESET"; }
good()  { printf '  %s+%s %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn()  { printf '  %s!%s %s\n' "$C_YELLOW" "$C_RESET" "$*"; }
step()  { printf '\n%s==>%s %s%s%s\n' "$C_CYAN" "$C_RESET" "$C_BOLD" "$*" "$C_RESET"; }

# A failure the operator can do something about. Always says what to do next.
# Exits non-zero, so a script wrapping this one can still tell it failed.
die() {
    printf '\n  %sSomething went wrong.%s\n\n' "$C_RED$C_BOLD" "$C_RESET"
    printf '  %s\n\n' "$*"
    exit 1
}

rule() { printf '  %s----------------------------------------------------------------%s\n' "$C_DIM" "$C_RESET"; }

# Ask a yes/no question.
#
# The catch that bites every `curl | sh' installer: our own script is on stdin,
# so a bare `read' would happily consume the rest of the script as the answer.
# We read from the terminal directly. If there is no terminal -- a cloud-init
# script, a CI job -- we take the default and say which way we went.
ask_yes_no() {
    _q="$1"; _default="$2"
    if [ "$ASSUME_YES" = "1" ]; then return 0; fi
    if [ ! -r /dev/tty ]; then
        info "$_q -- no terminal to ask on, assuming '$_default'"
        [ "$_default" = "y" ]
        return $?
    fi
    while :; do
        if [ "$_default" = "y" ]; then
            printf '  %s [Y/n] ' "$_q" > /dev/tty
        else
            printf '  %s [y/N] ' "$_q" > /dev/tty
        fi
        read -r _a < /dev/tty || _a=""
        [ -z "$_a" ] && _a="$_default"
        case "$_a" in
            y|Y|yes|YES|Yes) return 0 ;;
            n|N|no|NO|No)    return 1 ;;
            *) printf '  Please answer y or n.\n' > /dev/tty ;;
        esac
    done
}

# Ask for a value, offering a default.
ask_value() {
    _q="$1"; _default="$2"
    if [ "$ASSUME_YES" = "1" ] || [ ! -r /dev/tty ]; then
        printf '%s' "$_default"
        return 0
    fi
    if [ -n "$_default" ]; then
        printf '  %s [%s]: ' "$_q" "$_default" > /dev/tty
    else
        printf '  %s (leave empty to skip): ' "$_q" > /dev/tty
    fi
    read -r _a < /dev/tty || _a=""
    [ -z "$_a" ] && _a="$_default"
    printf '%s' "$_a"
}

have() { command -v "$1" >/dev/null 2>&1; }

# Run a command, or just describe it when --dry-run is on.
run() {
    if [ "$DRY_RUN" = "1" ]; then
        printf '  %s[dry-run]%s %s\n' "$C_DIM" "$C_RESET" "$*"
        return 0
    fi
    "$@"
}

cleanup() {
    [ -n "$TMPDIR_SELF" ] && [ -d "$TMPDIR_SELF" ] && rm -rf "$TMPDIR_SELF"
    return 0
}

# =============================================================================
#  Secrets
#
#  Three passwords, all made here, all different, none of them ever written
#  anywhere except this machine's .env file (mode 0600).
# =============================================================================

# Long and hex: this one is never typed by a human, only read by the server.
# No spaces and no quotes, because the game's config parser splits on those.
gen_secret() {
    if have openssl; then
        openssl rand -hex 24
    elif [ -r /dev/urandom ]; then
        # od is in coreutils and present even on a busybox-ish system.
        od -An -N24 -tx1 < /dev/urandom | tr -d ' \n'
    else
        die "This machine has no openssl and no /dev/urandom, so no password
  can be generated safely. That is very unusual. Please report it."
    fi
}

# The one a human has to read off the screen and type into a browser, so the
# alphabet leaves out the characters people confuse: 0/O, 1/l/I.
gen_passphrase() {
    _alphabet='abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789'
    if [ -r /dev/urandom ]; then
        LC_ALL=C tr -dc "$_alphabet" < /dev/urandom 2>/dev/null | dd bs=1 count=20 2>/dev/null
    else
        gen_secret | cut -c1-20
    fi
}

# =============================================================================
#  .env handling -- read and write single keys without disturbing the rest
# =============================================================================

env_get() {
    _file="$1"; _key="$2"
    [ -f "$_file" ] || return 1
    sed -n "s/^${_key}=//p" "$_file" | head -1
}

env_set() {
    _file="$1"; _key="$2"; _val="$3"
    touch "$_file"
    if grep -q "^${_key}=" "$_file" 2>/dev/null; then
        # A literal replacement: the value may contain / and & , which sed
        # would otherwise treat as syntax. Rebuild the line instead.
        _tmp="${_file}.tmp.$$"
        awk -v k="$_key" -v v="$_val" \
            'BEGIN{FS="="} $1==k && !done {print k "=" v; done=1; next} {print}' \
            "$_file" > "$_tmp"
        mv "$_tmp" "$_file"
    else
        printf '%s=%s\n' "$_key" "$_val" >> "$_file"
    fi
    chmod 600 "$_file"
}

# =============================================================================
#  docker compose, always in the right directory
# =============================================================================

dc() { ( cd "$INSTALL_DIR" && docker compose "$@" ); }

# The passphrase the operator chose in the panel, if they have.
#
# The panel writes it there when somebody uses its "change passphrase" form,
# because a hash cannot be printed and this script promises to show the
# passphrase at the end of every run. Empty is the normal answer: it means
# nobody has changed it and .env still has the right one.
#
# Tried twice on purpose. `exec' is the cheap way and works while the stack is
# up, which it is during an update; `run' starts a throwaway container off the
# same volume and covers a stack that happens to be stopped. --no-deps so it
# does not drag the database up to read one line, and --entrypoint because the
# panel image's own entrypoint would wait for that database.
panel_chosen_passphrase() {
    [ "$DRY_RUN" = "1" ] && return 0
    [ "$FRESH_INSTALL" = "0" ] || return 0     # nothing has run here yet
    _f="/usr/local/etc/panel.passphrase"
    _v=$(dc exec -T panel cat "$_f" 2>/dev/null | tr -d '\r\n')
    [ -n "$_v" ] || _v=$(dc run --rm -T --no-deps --entrypoint cat panel "$_f" 2>/dev/null | tr -d '\r\n')
    printf '%s' "$_v"
}

# The compose file names its own project and its own containers, so read them
# out of it rather than assuming. Getting this from the file means a stack that
# is renamed -- or a second one built for testing -- still works, and the
# volume names below are then guaranteed to be the ones compose actually uses.
stack_project() {
    _p=""
    [ -f "$INSTALL_DIR/docker-compose.yml" ] && \
        _p=$(sed -n 's/^name: *//p' "$INSTALL_DIR/docker-compose.yml" | head -1 | tr -d '"'"'"' ')
    [ -n "$_p" ] || _p=$(basename "$INSTALL_DIR")
    printf '%s' "$_p"
}

stack_container_names() {
    [ -f "$INSTALL_DIR/docker-compose.yml" ] || return 0
    sed -n 's/^ *container_name: *//p' "$INSTALL_DIR/docker-compose.yml" | tr -d '"'"'"' '
}

# =============================================================================
#  Step 0 -- command line
# =============================================================================

usage() {
    cat <<'USAGE'

  Metin2 server installer (Linux)

    curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sh
    curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sh -s -- [options]

  Options:

    --yes                 don't ask anything; accept every default
    --dry-run             show what would happen, change nothing
    --address ADDR        the IP or hostname players connect to
                          (default: detected automatically)
    --domain NAME         a domain pointing at this server. Turns on HTTPS
                          for the admin panel via Let's Encrypt.
    --email ADDR          e-mail for the Let's Encrypt account (needed with
                          --domain; only used for expiry warnings)
    --no-domain           drop the domain this server was set up with and go
                          back to plain HTTP on its address
    --local               bind everything to 127.0.0.1 and touch no firewall.
                          Only this machine can reach the server. For trying
                          it out; not for a real server.
    --dir PATH            where to install (default: /opt/metin2/stack)
    --auth-port N         login port (default: 11000)
    --game-ports A-B      channel ports (default: 13000-13002)
    --panel-port N        admin panel port (default: 7788)
    --no-client           don't build the downloadable game client
    --web-client          deprecated compatibility flag; ignored (native only)
    --no-web-client       compatibility flag; native-only is already default
    --custom-experience   turn the Custom Experience on without asking: bigger
                          pick-up range, a horse that always comes when called,
                          no waiting between the Horse Medal steps, bonus drops
                          on metins and bosses, Musk Oil in the General Store,
                          skill books that stack, High Risk offered, and
                          everyone 20% faster on foot
    --no-custom-experience
                          leave the Custom Experience off, and don't ask
    --no-firewall         don't touch the firewall
    --help                this text

  Where the server comes from:

    --reference-dir DIR   an already-unpacked "[40250] Reference Serverfile"
                          folder (the one with Server/ in it). Nothing is
                          downloaded when you give this.
    --archive PATH        the server-file package as you downloaded it --
                          the .zip/.rar/.7z, or metin2_server+src.tar.gz
    --source-url URL      explicit operator-owned source for the package
    --source-cache DIR    where the download and the working copies live
                          (default: /var/cache/m2src, needs ~4 GB)
    --repo-dir PATH       use this checkout instead of cloning one
    --repo-url URL        clone the port from here
    --local-context PATH  skip all of the above: install from a Docker build
                          context that is already prepared

USAGE
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --yes|-y)        ASSUME_YES=1; shift ;;
            --dry-run)       DRY_RUN=1; shift ;;
            --local)         LOCAL_ONLY=1; shift ;;
            --no-domain)     DOMAIN=""; DROP_DOMAIN=1; shift ;;
            --no-client)     SKIP_CLIENT=1; WANT_DESKTOP=0; shift ;;
            --web-client)    WANT_WEB_FLAG=1; shift ;;
            --no-web-client) WANT_WEB_FLAG=0; shift ;;
            --custom-experience)    CUSTOM_EXPERIENCE_FLAG=1; shift ;;
            --no-custom-experience) CUSTOM_EXPERIENCE_FLAG=0; shift ;;
            --no-firewall)   SKIP_FIREWALL=1; shift ;;
            --address)       PUBLIC_ADDRESS="${2:-}"; shift 2 ;;
            --domain)        DOMAIN="${2:-}"; shift 2 ;;
            --email)         TLS_EMAIL="${2:-}"; shift 2 ;;
            --dir)           INSTALL_DIR="${2:-}"; shift 2 ;;
            --auth-port)     AUTH_PORT="${2:-}"; shift 2 ;;
            --game-ports)    GAME_PORTS="${2:-}"; shift 2 ;;
            --panel-port)    PANEL_PORT="${2:-}"; shift 2 ;;
            --local-context) M2_LOCAL_CONTEXT="${2:-}"; shift 2 ;;
            --repo-dir)      M2_REPO_DIR="${2:-}"; shift 2 ;;
            --repo-url)      M2_REPO_URL="${2:-}"; shift 2 ;;
            --archive)       M2_SRC_ARCHIVE="${2:-}"; shift 2 ;;
            --reference-dir) M2_SRC_REFERENCE_DIR="${2:-}"; shift 2 ;;
            --source-url)    M2_SRC_URL="${2:-}"; shift 2 ;;
            --source-cache)  M2_SRC_CACHE="${2:-}"; shift 2 ;;
            --release-url)
                # There is no release archive any more -- see the top of this
                # file. Accepted so that an old command line gets an
                # explanation rather than a usage dump.
                shift 2
                warn "--release-url no longer does anything: there is no release"
                warn "archive. The server is assembled here instead -- see --help."
                ;;
            --help|-h)       usage; exit 0 ;;
            *) usage; die "I do not understand the option '$1'." ;;
        esac
    done
}

valid_port() {
    case "${1:-}" in ''|*[!0-9]*) return 1 ;; esac
    [ "$1" -ge 1 ] && [ "$1" -le 65535 ]
}

validate_ports() {
    valid_port "$AUTH_PORT" || die "--auth-port must be a number from 1 to 65535."
    valid_port "$PANEL_PORT" || die "--panel-port must be a number from 1 to 65535."
    case "$GAME_PORTS" in
        *-*) _game_first=${GAME_PORTS%%-*}; _game_last=${GAME_PORTS##*-} ;;
        *) die "--game-ports must be a range of exactly three ports, for example 13000-13002." ;;
    esac
    valid_port "$_game_first" && valid_port "$_game_last" ||
        die "--game-ports must contain valid numeric ports."
    [ "$_game_last" -eq $(( _game_first + 2 )) ] ||
        die "--game-ports must contain exactly three consecutive ports, for example 13000-13002."
}

# =============================================================================
#  Step 1 -- can this machine run a Metin2 server at all?
# =============================================================================

check_root() {
    if [ "$(id -u)" != "0" ]; then
        if have sudo; then
            die "Please run this as root. The usual way is:

      curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sudo sh

  It needs root to install Docker, open the firewall and write to
  $INSTALL_DIR."
        fi
        die "Please log in as root and run this again. It needs root to
  install Docker and open the firewall."
    fi
}

check_arch() {
    _arch="$(uname -m)"
    case "$_arch" in
        x86_64|amd64|i386|i486|i586|i686)
            info "processor: $_arch -- fine" ;;
        aarch64|arm64|armv7l|armv6l|arm*)
            die "This machine has an $_arch (ARM) processor, and the Metin2
  server cannot run on it.

  This is not something the installer can work around. The server is built
  from the original 2000s source code, which produces 32-bit x86 programs.
  There is no ARM version and making one is a large piece of work, not a
  setting.

  What to do: rent a VPS with an Intel or AMD processor. Every provider
  offers them and they are usually the cheaper option. If you are on an
  Apple Silicon Mac, a small x86 VPS (about 5 EUR a month) is the way to go.

  Providers whose standard plans are x86: Hetzner CX, Contabo, Netcup,
  DigitalOcean 'Regular', Vultr 'Cloud Compute'. Avoid anything labelled
  'Ampere', 'Graviton', 'ARM64' or 'Neoverse'." ;;
        *)
            warn "Unrecognised processor type '$_arch'."
            warn "The server needs 32-bit x86 binaries. If this is not an"
            warn "Intel or AMD machine, the build will fail later."
            ask_yes_no "Carry on anyway?" "n" || exit 1 ;;
    esac
}

check_memory() {
    _kb=$(awk '/^MemTotal:/{print $2}' /proc/meminfo 2>/dev/null || echo 0)
    _mb=$(( _kb / 1024 ))
    if [ "$_mb" -lt 100 ]; then
        warn "Could not work out how much memory this machine has; carrying on."
        return 0
    fi
    if [ "$_mb" -lt 1200 ]; then
        die "This machine has ${_mb} MB of memory, and the server needs at
  least about 1 GB just to run -- the game cores alone hold roughly 860 MB.
  Building it needs more still.

  It would start, run out of memory, and be killed part-way through, which
  looks like a random crash and wastes an afternoon. So it stops here instead.

  What to do: use a VPS with at least 2 GB, and 4 GB for comfort. Adding swap
  is not a fix; the game cores touch their memory constantly and swapping
  makes the server unplayable."
    fi
    if [ "$_mb" -lt 3600 ]; then
        warn "This machine has ${_mb} MB of memory. It will run, but 4 GB is"
        warn "the comfortable size -- one channel needs about 1 GB and the"
        warn "build itself is hungry. Watch out for the build being killed."
        ask_yes_no "Carry on?" "y" || exit 1
    else
        info "memory: ${_mb} MB -- fine"
    fi
}

check_disk() {
    # Docker's data lives under /var/lib/docker once Docker exists; before that,
    # ask about / instead. Either way we want the filesystem that will hold the
    # images.
    _target=/var/lib/docker
    [ -d "$_target" ] || _target=/var/lib
    [ -d "$_target" ] || _target=/
    _free_mb=$(df -Pm "$_target" 2>/dev/null | awk 'NR==2{print $4}')
    [ -n "$_free_mb" ] || { warn "Could not measure free disk space; carrying on."; return 0; }
    if [ "$_free_mb" -lt 15000 ]; then
        die "There are only $(( _free_mb / 1024 )) GB free on $_target, and
  this needs about 15 GB: 8 GB to build the server, and another 7 GB while the
  downloadable game client is put together -- plus room for the images and the
  game's own logs.

  It stops here rather than filling the disk up and failing three quarters of
  the way through a twenty-minute build.

  What to do: free some space, or use a VPS with at least 40 GB. If the disk
  really is bigger than this, the extra space may not be mounted -- check
  with:   df -h"
    fi
    if [ "$_free_mb" -lt 30000 ]; then
        warn "$(( _free_mb / 1024 )) GB free on $_target. Enough to build, but"
        warn "the game writes roughly 40 MB of logs per hour per channel, so"
        warn "keep an eye on it (lower M2_LOG_KEEP_DAYS if it gets tight)."
    else
        info "disk: $(( _free_mb / 1024 )) GB free on $_target -- fine"
    fi
}

detect_os() {
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        OS_NAME="${ID:-unknown}"
        OS_VERSION="${VERSION_ID:-}"
        _like="${ID_LIKE:-}"
    else
        _like=""
    fi
    case "$OS_NAME $_like" in
        *debian*|*ubuntu*) OS_FAMILY="debian" ;;
        *rhel*|*fedora*|*centos*|*rocky*|*almalinux*) OS_FAMILY="rhel" ;;
        *suse*) OS_FAMILY="suse" ;;
        *arch*) OS_FAMILY="arch" ;;
        *) OS_FAMILY="unknown" ;;
    esac
    info "system: ${OS_NAME}${OS_VERSION:+ $OS_VERSION} (${OS_FAMILY} family)"
}

# =============================================================================
#  Step 2 -- Docker
# =============================================================================

compose_ok() {
    docker compose version >/dev/null 2>&1
}

install_docker() {
    step "Docker"

    if have docker && docker info >/dev/null 2>&1 && compose_ok; then
        good "Docker is already installed and running."
        info "$(docker --version 2>/dev/null)"
        info "$(docker compose version 2>/dev/null | head -1)"
        return 0
    fi

    if have docker && ! docker info >/dev/null 2>&1; then
        say "Docker is installed but not running. Starting it..."
        run systemctl enable --now docker 2>/dev/null || run service docker start 2>/dev/null || true
        sleep 3
        if docker info >/dev/null 2>&1 && compose_ok; then
            good "Docker is running now."
            return 0
        fi
        die "Docker is installed but will not start. Find out why with:

      systemctl status docker
      journalctl -u docker -n 50

  Then run this installer again."
    fi

    case "$OS_FAMILY" in
        debian)
            say "Installing Docker with the official installer from get.docker.com."
            say "This takes a minute or two." ;;
        rhel|suse|arch)
            warn "This installer is tested on Debian and Ubuntu."
            warn "On $OS_NAME the official Docker script usually works, but if"
            warn "it does not, install Docker yourself and run this again."
            ask_yes_no "Try installing Docker anyway?" "y" || exit 1 ;;
        *)
            warn "I do not recognise this Linux distribution ('$OS_NAME')."
            warn "I cannot promise the automatic Docker install will work."
            warn "If it fails: install Docker and the compose plugin by hand,"
            warn "then run this installer again -- it will skip this step."
            ask_yes_no "Try installing Docker anyway?" "y" || exit 1 ;;
    esac

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] curl -fsSL https://get.docker.com | sh"
        return 0
    fi

    have curl || {
        case "$OS_FAMILY" in
            debian) apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq curl ca-certificates >/dev/null 2>&1 ;;
            rhel)   yum install -y -q curl ca-certificates >/dev/null 2>&1 ;;
        esac
    }
    have curl || die "curl is missing and I could not install it. Install curl,
  then run this again."

    if ! curl -fsSL https://get.docker.com -o /tmp/get-docker.sh; then
        die "Could not download Docker's installer from https://get.docker.com.
  Check that this machine can reach the internet:

      curl -I https://get.docker.com"
    fi
    sh /tmp/get-docker.sh >/tmp/docker-install.log 2>&1 || {
        tail -20 /tmp/docker-install.log
        rm -f /tmp/get-docker.sh
        die "Docker's own installer failed -- its last 20 lines are above, and
  the whole log is in /tmp/docker-install.log.

  Install Docker by hand following https://docs.docker.com/engine/install/
  and then run this installer again; it will notice Docker is there and carry
  straight on."
    }
    rm -f /tmp/get-docker.sh

    systemctl enable --now docker >/dev/null 2>&1 || service docker start >/dev/null 2>&1 || true
    sleep 3

    docker info >/dev/null 2>&1 || die "Docker installed but is not running.
  Try:  systemctl status docker"
    compose_ok || die "Docker is installed but the 'compose' plugin is not.
  Install it with:

      apt-get install -y docker-compose-plugin

  then run this installer again."

    good "Docker installed."
}

# `ports: !override' in a compose override file needs Compose v2.24 or newer.
# Only the --local path uses it; a real server publishes on 0.0.0.0, which is
# already the default, so nothing has to be overridden.
compose_supports_override() {
    _v=$(docker compose version --short 2>/dev/null | tr -d 'v')
    [ -n "$_v" ] || return 1
    _maj=$(printf '%s' "$_v" | cut -d. -f1)
    _min=$(printf '%s' "$_v" | cut -d. -f2)
    [ "${_maj:-0}" -gt 2 ] && return 0
    [ "${_maj:-0}" -eq 2 ] && [ "${_min:-0}" -ge 24 ] && return 0
    return 1
}

# =============================================================================
#  Step 3 -- get the server onto this machine
# =============================================================================

context_is_complete() {
    _d="$1"
    [ -f "$_d/docker-compose.yml" ] || return 1
    [ -d "$_d/game/src/server" ]    || return 1
    [ -f "$_d/panel/app/admin_panel.py" ] || return 1
    [ -d "$_d/mariadb/initdb.d/dumps" ]   || return 1
    [ -s "$_d/mariadb/playerbot/apply.sh" ] || return 1
    [ -s "$_d/mariadb/playerbot/playerbots_seed.sql" ] || return 1
    return 0
}

explain_incomplete_context() {
    die "The Docker build context in

      $1

  is not complete: the game source, database dumps, or Playerbot seed are
  missing from it.

  That is exactly what a bare checkout of the repository looks like. The
  repository contains the Linux port and nothing else -- the game itself is
  not ours to publish -- so a checkout has to be filled in before it can be
  built:

      cd <checkout>
      sh linux-port/fetch-sources.sh fetch

  This installer normally does that step for you. Run it without
  --local-context and it will."
}

# -----------------------------------------------------------------------------
#  The tools linux-port/fetch-sources.sh needs.
#
#  It checks for them itself and stops with a sentence naming the apt line --
#  but on a fresh VPS they are always missing, and stopping a one-command
#  install to say "now run apt-get install patch" is a poor answer when we are
#  already root on a machine with a package manager. So they go on first.
#
#  megatools is only wanted when there is really something to download; asking
#  for it on a machine that was handed a local copy would fail the install for
#  a package it never uses.
# -----------------------------------------------------------------------------
install_source_tools() {
    # python3 is on the list because prepare-context.sh applies the shipped-file
    # fixes and the Custom Experience with it. It is on every ordinary Debian
    # and Ubuntu already; a minimal cloud image is where it is missing, and
    # there the whole assembly would stop at the last step without it.
    _need="patch unzip git ca-certificates python3"
    _check="patch unzip git python3"
    if [ -z "$M2_SRC_REFERENCE_DIR" ] && [ -z "$M2_SRC_ARCHIVE" ]; then
        _need="$_need megatools p7zip-full"
        _check="$_check megatools"
    fi

    _missing=0
    for _t in $_check; do have "$_t" || _missing=1; done
    [ "$_missing" = "0" ] && return 0

    case "$OS_FAMILY" in
        debian)
            say "Installing the few tools that unpack and patch the server files..."
            if [ "$DRY_RUN" = "1" ]; then
                info "[dry-run] apt-get install -y $_need"
                return 0
            fi
            export DEBIAN_FRONTEND=noninteractive
            apt-get update -qq >/dev/null 2>&1 || true
            # shellcheck disable=SC2086
            apt-get install -y -qq $_need >/dev/null 2>&1 || true ;;
        rhel)
            [ "$DRY_RUN" = "1" ] && { info "[dry-run] yum install -y patch unzip git p7zip python3"; return 0; }
            yum install -y -q patch unzip git p7zip python3 >/dev/null 2>&1 || true ;;
        *)
            : ;;   # fetch-sources.sh will say what is missing, in its own words
    esac
    return 0
}

# -----------------------------------------------------------------------------
#  Finding the port itself.
#
#  Three ways, in order of how little they cost: told explicitly, already on
#  this machine because this script is part of a checkout, or cloned.
# -----------------------------------------------------------------------------
locate_repo() {
    if [ -n "$M2_REPO_DIR" ]; then
        [ -f "$M2_REPO_DIR/linux-port/fetch-sources.sh" ] || die \
"--repo-dir points at

      $M2_REPO_DIR

  but there is no linux-port/fetch-sources.sh in it, so that is not a checkout
  of this project. Point it at the top of the repository -- the directory with
  installer/ and linux-port/ in it."
        REPO_DIR=$(cd "$M2_REPO_DIR" && pwd)
        info "using the checkout at $REPO_DIR"
        return 0
    fi

    if [ -n "$SELF_DIR" ] && [ -f "$SELF_DIR/../linux-port/fetch-sources.sh" ]; then
        REPO_DIR=$(cd "$SELF_DIR/.." && pwd)
        info "using the checkout this installer is part of: $REPO_DIR"
        return 0
    fi

    case "$M2_REPO_URL" in
        REPLACE_ME*|"")
            die "This copy of the installer does not know where to clone the
  project from -- the repository URL placeholder was never filled in, which
  means you are running a development copy of install.sh.

  Either point it at a checkout you already have:

      sh install.sh --repo-dir /path/to/checkout

  or give it the repository:

      sh install.sh --repo-url https://.../server.git" ;;
    esac

    have git || die "git is needed to fetch the project and could not be
  installed. Install it and run this again:

      apt-get install -y git"

    REPO_DIR="$M2_SRC_CACHE/repo"
    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] git clone --depth 1 $M2_REPO_URL $REPO_DIR"
        return 0
    fi

    if [ -d "$REPO_DIR/.git" ]; then
        say "Updating the checkout in $REPO_DIR ..."
        ( cd "$REPO_DIR" && git fetch --depth 1 origin && git reset --hard FETCH_HEAD ) \
            >/dev/null 2>&1 || warn "could not update it; using what is already there"
    else
        say "Getting the project from $M2_REPO_URL ..."
        say "This is a few megabytes -- it is the port, not the game."
        mkdir -p "$(dirname "$REPO_DIR")"
        # Do not delete the ground we are standing on. Somebody who ran this
        # from inside the old checkout -- a natural thing to do, it is where
        # install.sh lives -- would otherwise lose their working directory to
        # the line below, and git would then refuse to clone into it with
        # "Unable to read current working directory". Leaving first is enough,
        # and only done when it is actually about to happen.
        case "$(pwd -P 2>/dev/null)/" in
            "$REPO_DIR"/*) cd / || die "could not leave $REPO_DIR" ;;
        esac
        rm -rf "$REPO_DIR"
        git clone --depth 1 "$M2_REPO_URL" "$REPO_DIR" || die \
"The project could not be cloned from

      $M2_REPO_URL

  Check that this machine can reach it:

      git ls-remote $M2_REPO_URL"
    fi
    good "Project checkout ready in $REPO_DIR"
}

# -----------------------------------------------------------------------------
#  Assembling the server.
#
#  All of the work is in linux-port/fetch-sources.sh, which is deliberately not
#  duplicated here. What this adds is the translation of its exit codes into
#  something an operator can act on -- especially code 4, which on the default
#  MEGA link usually means the share has hit its bandwidth quota, not that
#  anything is broken.
# -----------------------------------------------------------------------------
# ── artifacts.json, the one place that says where the big files live ─────────
#
# Same reader as linux-port/fetch-web-client.sh uses, and the same reason the
# file is kept flat: there is no JSON parser to rely on here.
pointer_file() {
    for _p in "$REPO_DIR/artifacts.json" "$INSTALL_DIR/artifacts.json"; do
        [ -n "$_p" ] && [ -f "$_p" ] && { printf '%s\n' "$_p"; return 0; }
    done
    return 1
}

pointer_get() {   # pointer_get KEY -- string or number, empty when absent
    _pf=$(pointer_file) || return 0
    _pv=$(sed -n 's/.*"'"$1"'"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$_pf" 2>/dev/null | head -1)
    if [ -z "$_pv" ]; then
        _pv=$(sed -n 's/.*"'"$1"'"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$_pf" 2>/dev/null | head -1)
    fi
    printf '%s\n' "$_pv"
}

# A value that is not a link has not been filled in. Every reader of this file
# makes that check rather than attempting a meaningless download.
pointer_link() {  # pointer_link KEY -- the link, or nothing
    _v=$(pointer_get "$1")
    case "$_v" in
        https://*|http://*) printf '%s\n' "$_v" ;;
        *) : ;;
    esac
}

# =============================================================================
#  The Custom Experience -- one question, everything behind it
# =============================================================================
#
#  Asked before anything large is downloaded, together with the other questions,
#  because the answer has to be known before the server is assembled: most of
#  what it turns on is compiled into the game image, and an image cannot be
#  asked afterwards.
#
#  Where the answer comes from, in order, first one wins:
#
#      M2_CUSTOM_EXPERIENCE in the environment   (unattended, first install)
#      --custom-experience / --no-custom-experience
#      what this server was set to last time     (.env, so an update keeps it)
#      the question, which defaults to no
#
choose_custom_experience() {
    step "Custom Experience"

    _prev_custom=$(env_get "$INSTALL_DIR/.env" M2_CUSTOM_EXPERIENCE 2>/dev/null || true)
    case "$_prev_custom" in 1|true|yes|on|TRUE|YES|ON) _prev_custom=1 ;; *) _prev_custom=0 ;; esac

    if [ -n "${M2_CUSTOM_EXPERIENCE:-}" ]; then
        _want="$M2_CUSTOM_EXPERIENCE"
        info "taken from M2_CUSTOM_EXPERIENCE in the environment"
    elif [ -n "$CUSTOM_EXPERIENCE_FLAG" ]; then
        _want="$CUSTOM_EXPERIENCE_FLAG"
        info "taken from the option you passed"
    else
        say "This server can be left exactly as the original files play, or it"
        say "can be set up the friendlier way this project has been using:"
        say ""
        say "  - items and Yang are picked up from twice as far away"
        say "  - calling your horse always works, instead of usually failing"
        say "  - the Horse Medal steps no longer make you wait until tomorrow"
        say "  - metin stones and bosses drop useful extras: blessing scrolls,"
        say "    reading potions, bravery capes and more"
        say "  - the General Store stocks the Musk Oil that one quest asks for"
        say "  - skill books for the same skill stack instead of filling the"
        say "    inventory one slot at a time"
        say "  - players may choose High Risk at level 15, and everyone walks"
        say "    and runs 20% faster"
        say ""
        say "None of this changes rates, and none of it touches your existing"
        say "characters. You can turn it off again later by running the"
        say "installer with --no-custom-experience."
        say ""
        # ask_yes_no() returns yes whenever --yes was passed, which is right for
        # every other question in this file because every other question
        # defaults to yes. This one defaults to NO, so taking ask_yes_no's
        # answer under --yes would switch the whole thing on for somebody who
        # asked for nothing but silence. --yes means "don't ask me, take the
        # default", and the default here is no.
        if [ "$ASSUME_YES" = "1" ]; then
            _want="$_prev_custom"
            info "--yes, so this keeps what is already set here: $([ "$_prev_custom" = "1" ] && echo on || echo off)"
        else
            _def=$([ "$_prev_custom" = "1" ] && echo y || echo n)
            if ask_yes_no "Enable Custom Experience?" "$_def"; then _want=1; else _want=0; fi
        fi
    fi

    case "$_want" in 1|true|yes|on|TRUE|YES|ON) M2_CUSTOM_EXPERIENCE=1 ;; *) M2_CUSTOM_EXPERIENCE=0 ;; esac
    export M2_CUSTOM_EXPERIENCE

    # The two settings the Custom Experience carries that already had switches
    # of their own. Both are given a value here rather than left to
    # prepare-context.sh, because .env has to record what this build actually
    # used -- an .env that says 0 beside a server that runs at 20 is how the
    # last round of hand edits became impossible to account for.
    #
    # Turning it ON is what brings them with it, and only the first time: a
    # number the operator has since chosen for themselves is theirs, and is
    # carried forward untouched by every later update.
    if [ "$M2_CUSTOM_EXPERIENCE" = "1" ]; then
        # An explicit M2_HIGH_RISK=0 still wins -- somebody may want everything
        # here except a mode that lets players kill each other.
        if [ -z "${M2_HIGH_RISK:-}" ]; then
            M2_HIGH_RISK=$(env_get "$INSTALL_DIR/.env" M2_HIGH_RISK 2>/dev/null || true)
        fi
        case "${M2_HIGH_RISK:-}" in 0|false|no|off|FALSE|NO|OFF) M2_HIGH_RISK=0 ;; *) M2_HIGH_RISK=1 ;; esac
        export M2_HIGH_RISK

        # Read what is already there FIRST, exactly as the High Risk branch
        # above does. Without this the test below only asks whether the
        # ENVIRONMENT carries a number -- and it never does at this point,
        # because the read-back from .env does not happen until
        # run_fetch_sources(), eighty lines further down. So a server that had
        # chosen 100 was handed 20 the first time the Custom Experience was
        # switched on, which is the opposite of what the comment above
        # promises. Measured on a live server: .env said 100 going in and 20
        # coming out.
        if [ -z "${M2_MOVE_SPEED_BONUS:-}" ]; then
            M2_MOVE_SPEED_BONUS=$(env_get "$INSTALL_DIR/.env" M2_MOVE_SPEED_BONUS 2>/dev/null || true)
        fi
        if [ -z "${M2_MOVE_SPEED_BONUS:-}" ] && [ "$_prev_custom" != "1" ]; then
            M2_MOVE_SPEED_BONUS=20
        fi
    fi

    if [ "$M2_CUSTOM_EXPERIENCE" = "1" ]; then
        good "Custom Experience: on."
    else
        info "Custom Experience: off -- the server files play as they shipped."
    fi
    return 0
}

run_fetch_sources() {
    # WHERE THE SERVER FILES COME FROM.
    #
    # No project-owned download source is built in. Anything the operator said
    # wins: --source-url, a local archive, or an unpacked directory.
    # artifacts.json is consulted only as optional metadata and normally has
    # an empty URL.
    if [ -z "$M2_SRC_URL" ] && [ -z "$M2_SRC_ARCHIVE" ] && [ -z "$M2_SRC_REFERENCE_DIR" ]; then
        _pu=$(pointer_link src_server_url)
        if [ -n "$_pu" ]; then
            M2_SRC_URL="$_pu"
            _ps=$(pointer_get src_server_sha256)
            case "$_ps" in
                ????????????????????????????????????????????????????????????????)
                    M2_SRC_SHA256="$_ps"; export M2_SRC_SHA256 ;;
            esac
            info "server files: $(pointer_get src_server_file) (from artifacts.json)"
        fi
    fi

    # Optional operator-owned fallbacks, reached only after the primary URL.
    M2_SRC_URL_FALLBACK=${M2_SRC_URL_FALLBACK:-$(pointer_link src_server_url_fallback)}
    M2_SRC_URL_FALLBACK2=${M2_SRC_URL_FALLBACK2:-$(pointer_link src_server_url_fallback2)}
    export M2_SRC_URL_FALLBACK M2_SRC_URL_FALLBACK2

    _fs="$REPO_DIR/linux-port/fetch-sources.sh"
    [ -f "$_fs" ] || die "$_fs is missing from the checkout. That file is what
  turns a checkout into a buildable server; without it there is nothing to do."

    step "Assembling the server"
    say "The game itself is not in this project and cannot be -- it belongs to"
    say "Ymir/Webzen. The compatible r40250 files you supplied (or the cached"
    say "copy from an earlier run) are validated, the Linux port is applied,"
    say "and the result is turned into something Docker can build."
    say ""
    if [ -n "$M2_SRC_REFERENCE_DIR" ]; then
        info "using the unpacked package in $M2_SRC_REFERENCE_DIR"
    elif [ -n "$M2_SRC_ARCHIVE" ]; then
        info "using the package archive $M2_SRC_ARCHIVE"
    else
        say "Looking for a compatible r40250 package already present in"
        say "$M2_SRC_CACHE. On a clean install, pass --archive or"
        say "--reference-dir; this project does not publish game files."
    fi
    say ""

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] sh $_fs fetch --cache $M2_SRC_CACHE"
        return 0
    fi

    # The movement-speed bonus is decided HERE, not at run time: the quest that
    # applies it is compiled into the game image, and a compiled quest cannot
    # read an environment variable. prepare-context.sh writes the number into
    # the file before qc sees it.
    #
    # Read back out of .env on an update, so the setting survives; an operator
    # on a first install passes it in the environment
    # (M2_MOVE_SPEED_BONUS=100 curl ... | sh) because .env does not exist yet.
    if [ -z "${M2_MOVE_SPEED_BONUS:-}" ]; then
        M2_MOVE_SPEED_BONUS=$(env_get "$INSTALL_DIR/.env" M2_MOVE_SPEED_BONUS 2>/dev/null || true)
    fi
    case "${M2_MOVE_SPEED_BONUS:-}" in
        ''|*[!0-9]*) M2_MOVE_SPEED_BONUS=0 ;;
    esac
    export M2_MOVE_SPEED_BONUS

    # The Custom Experience is decided here for the same reason, and read back
    # out of .env the same way, so an update keeps what this server was set to.
    # choose_custom_experience() has normally answered this already -- it asks
    # before anything large is downloaded -- and this is what covers the paths
    # that reach the assembly without it.
    if [ -z "${M2_CUSTOM_EXPERIENCE:-}" ]; then
        M2_CUSTOM_EXPERIENCE=$(env_get "$INSTALL_DIR/.env" M2_CUSTOM_EXPERIENCE 2>/dev/null || true)
    fi
    case "${M2_CUSTOM_EXPERIENCE:-}" in
        1|true|yes|on|TRUE|YES|ON) M2_CUSTOM_EXPERIENCE=1 ;;
        *)                         M2_CUSTOM_EXPERIENCE=0 ;;
    esac
    export M2_CUSTOM_EXPERIENCE
    [ -n "${M2_HIGH_RISK:-}" ] && export M2_HIGH_RISK

    export M2_SRC_ARCHIVE M2_SRC_REFERENCE_DIR M2_SRC_URL M2_SRC_CACHE

    _rc=0
    sh "$_fs" fetch --cache "$M2_SRC_CACHE" || _rc=$?
    [ "$_rc" = "0" ] && { good "The server is assembled."; return 0; }

    say ""
    case "$_rc" in
        3) die "A tool the assembly needs is missing from this machine -- the
  line above names it. Install it and run this installer again; everything
  done so far is kept.

  On Debian or Ubuntu that is usually:

      apt-get install -y patch unzip megatools p7zip-full python3" ;;
        4) die "The compatible r40250 server-file package was not supplied.

  This repository intentionally contains no game server source, game data or
  download mirror. Nothing was installed and nothing was left running. Use a
  local package that you are authorised to use:

          sh install.sh --archive /path/to/the-package.zip

  Or, if you have already unpacked it:

          sh install.sh --reference-dir '/path/to/[40250] Reference Serverfile'" ;;
        5) die "The server-file package was obtained but it is not the r40250
  package this port expects: the pieces it must contain are not in it. The
  line above says which one was missing.

  If you passed --archive or --reference-dir, check it really is the
  '[40250] Reference Serverfile' package -- the one with Server/ and Client/
  in it. If it was downloaded, it may have been cut short; run the installer
  again with:

      --source-cache $M2_SRC_CACHE   (and delete $M2_SRC_CACHE/archive first)" ;;
        6) die "The Linux port does not apply to this source.

  That is a specific and useful answer: the upstream package on this machine
  is not the r40250 baseline the port was made against. It is not a fault in
  the patch, and it must not be forced -- a partly-applied port produces a
  server that compiles and then does not work.

  What to do: use the supported baseline r40250 package. The full comparison
  is in $M2_SRC_CACHE/patch.log. A source-free report suitable for attaching
  to a GitHub issue is in:

      $M2_SRC_CACHE/compatibility-report.txt

  The baseline is recorded in the patch's own header:

      head -30 $REPO_DIR/linux-port/patches/0001-r40250-linux-port.patch" ;;
        7) die "There is not enough disk space to assemble the server. The line
  above says how much was needed and how much there is.

  It needs about 4 GB free in $M2_SRC_CACHE while it works, on top of the
  15 GB for Docker itself. Free some space and run this installer again --
  nothing has been changed." ;;
        8) die "The build context could not be filled in: prepare-context.sh
  failed, and its output is above. Everything before that step succeeded, so
  running this installer again will not repeat the download.

  This is a bug rather than something you did; please report it with the
  output above." ;;
        130) die "Interrupted. Nothing was installed. Run this installer again
  when you are ready -- the download continues from where it stopped." ;;
        *) die "Assembling the server failed (fetch-sources.sh exited $_rc).
  Its output is above and its log is in $M2_SRC_CACHE/fetch.log.

  Nothing was installed and nothing was left running." ;;
    esac
}

# The version this machine is running, from the build context the panel image
# was made out of. Empty when it cannot be told -- which is not an error: every
# server installed before versions existed has no such file, and there are more
# of those than of any other kind right now.
# The command that reinstalls THIS server, options and all. The panel shows it
# when an update appears, so it has to be the command that rebuilds what is
# actually here -- printing the bare one-liner to somebody who installed with
# --domain would quietly drop their certificate on the next update.
#
# Only options that describe the server are replayed. The ones that say where
# the SOURCES came from this time (--archive, --reference-dir, --source-url,
# --repo-*) are left out on purpose: they point at files that may well be gone
# by then, and the cache makes them unnecessary anyway.
_q() { # shell-quote one value, for paths with spaces in them
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
}
replay_command() {
    _opt=""
    [ "$INSTALL_DIR" != "$DEF_INSTALL_DIR" ] && _opt="$_opt --dir $(_q "$INSTALL_DIR")"
    [ -n "$DOMAIN" ]    && _opt="$_opt --domain $(_q "$DOMAIN")"
    [ -n "$TLS_EMAIL" ] && _opt="$_opt --email $(_q "$TLS_EMAIL")"
    [ "$LOCAL_ONLY" = "1" ]    && _opt="$_opt --local"
    [ "$SKIP_CLIENT" = "1" ]   && _opt="$_opt --no-client"
    # Named either way round, because this is the command the panel shows an
    # operator to update with -- and it has to rebuild the server they have,
    # not the one the defaults would produce.
    [ "${M2_CUSTOM_EXPERIENCE:-0}" = "1" ] && _opt="$_opt --custom-experience"
    [ "${M2_CUSTOM_EXPERIENCE:-0}" = "0" ] && _opt="$_opt --no-custom-experience"
    [ "$SKIP_FIREWALL" = "1" ] && _opt="$_opt --no-firewall"
    [ "$AUTH_PORT"  != "$DEF_AUTH_PORT"  ] && _opt="$_opt --auth-port $AUTH_PORT"
    [ "$GAME_PORTS" != "$DEF_GAME_PORTS" ] && _opt="$_opt --game-ports $(_q "$GAME_PORTS")"
    [ "$PANEL_PORT" != "$DEF_PANEL_PORT" ] && _opt="$_opt --panel-port $PANEL_PORT"
    if [ -n "$_opt" ]; then
        printf 'curl -fsSL %s | sudo sh -s --%s' "$M2_INSTALLER_URL" "$_opt"
    else
        printf 'curl -fsSL %s | sudo sh' "$M2_INSTALLER_URL"
    fi
}

installed_version() {
    _v=""
    [ -f "$INSTALL_DIR/panel/app/VERSION" ] && _v=$(head -1 "$INSTALL_DIR/panel/app/VERSION" 2>/dev/null | tr -d '\r\n \t')
    printf '%s' "$_v"
}

# The version published in the repository. Empty when it cannot be fetched --
# no network, GitHub down, a proxy in the way. Never a reason to stop; the
# question then simply becomes the one it used to be.
published_version() {
    have curl || { printf ''; return 0; }
    _u="${M2_VERSION_URL:-$(printf '%s' "$M2_INSTALLER_URL" | sed 's#/installer/install\.sh$#/VERSION#')}"
    _v=$(curl -fsSL --max-time 12 "$_u" 2>/dev/null | head -1 | tr -d '\r\n \t')
    case "$_v" in
        [0-9]*.[0-9]*.[0-9]*) printf '%s' "$_v" ;;
        *)                    printf '' ;;
    esac
}

# a.b.c > x.y.z ? Pure shell, no sort -V: busybox and some minimal images do
# not have it, and this has to work everywhere the installer does.
version_gt() {
    _a="${1:-0.0.0}"; _b="${2:-0.0.0}"
    for _i in 1 2 3; do
        _x=$(printf '%s' "$_a" | cut -d. -f$_i); _y=$(printf '%s' "$_b" | cut -d. -f$_i)
        _x=${_x:-0}; _y=${_y:-0}
        case "$_x$_y" in *[!0-9]*) return 1 ;; esac
        [ "$_x" -gt "$_y" ] && return 0
        [ "$_x" -lt "$_y" ] && return 1
    done
    return 1
}

fetch_stack() {
    step "The server files"

    if [ -f "$INSTALL_DIR/docker-compose.yml" ]; then
        FRESH_INSTALL=0
        good "A server is already installed in $INSTALL_DIR."
        say ""
        say "Nothing here will be deleted. Your accounts, characters, items and"
        say "guilds live in a Docker volume that this installer never touches --"
        say "only 'docker compose down -v' would remove them, and this script"
        say "never runs that. Your settings in .env are kept either way."
        say ""

        _have=$(installed_version)
        _new=$(published_version)

        if [ -n "$_have" ]; then
            say "  This server:   $_have"
        else
            # No VERSION file at all: installed before this project had them.
            say "  This server:   unknown -- installed before versions were added"
        fi
        if [ -n "$_new" ]; then
            say "  Published:     $_new"
        else
            say "  Published:     could not be checked just now"
        fi
        say ""

        # Offer the update when the published version is genuinely newer, and
        # also when the local one cannot be read at all -- an install with no
        # VERSION predates them, so anything published is newer than it.
        _outdated=0
        if [ -n "$_new" ]; then
            if [ -z "$_have" ] || version_gt "$_new" "$_have"; then _outdated=1; fi
        fi

        if [ "$_outdated" = "1" ]; then
            say "Updating rebuilds the server from the published version and"
            say "restarts it, which disconnects anyone playing for a minute or"
            say "two. Answering no re-applies your settings and restarts, and"
            say "leaves the version you have alone."
            say ""
            if ask_yes_no "Update this server to $_new?" "y"; then
                say ""
                say "Updating to $_new."
                # falls through -- see the note below
            else
                say ""
                say "Keeping the version you have."
                return 0
            fi
        else
            if [ -n "$_new" ] && [ -n "$_have" ]; then
                say "That is the newest published version."
            fi
            say ""
            if ! ask_yes_no "Re-apply the settings and restart the server?" "y"; then
                say ""
                say "Left alone. To manage the existing server:"
                say "    cd $INSTALL_DIR && docker compose ps"
                exit 0
            fi
            return 0
        fi
        # Deliberately NOT returning here. This used to stop at this point, so
        # re-running the installer rewrote .env, restarted the containers and
        # changed nothing else -- the checkout was never refreshed and the build
        # context in $INSTALL_DIR stayed exactly as it was, which meant the
        # rebuild had nothing new to build. "Run it again to update" was untrue,
        # and it is the sentence the panel now shows people. Falling through
        # does the same work a first install does: refresh the checkout, restage
        # the context, copy it over. The copy is a tar stream, so it writes only
        # what it carries and .env and docker-compose.override.yml survive it.
    fi

    # -- 1. a build context somebody prepared earlier -------------------------
    if [ -n "$M2_LOCAL_CONTEXT" ]; then
        [ -d "$M2_LOCAL_CONTEXT" ] || die "--local-context points at
  '$M2_LOCAL_CONTEXT', which is not a directory."
        context_is_complete "$M2_LOCAL_CONTEXT" || explain_incomplete_context "$M2_LOCAL_CONTEXT"
        say "Copying the server from $M2_LOCAL_CONTEXT ..."
        run mkdir -p "$INSTALL_DIR"
        if [ "$DRY_RUN" != "1" ]; then
            # -a keeps the executable bits. The MariaDB image *sources* an
            # import script that is not executable instead of running it, which
            # leaks our shell options into its entrypoint and breaks the import
            # in a way that looks like a database bug.
            (cd "$M2_LOCAL_CONTEXT" && tar cf - .) | (cd "$INSTALL_DIR" && tar xf -)
        fi
        good "Server files in place."
        return 0
    fi

    # -- 2. otherwise: get the port, then assemble the server ------------------
    install_source_tools
    locate_repo
    run_fetch_sources

    _ctx="$REPO_DIR/linux-port/docker"
    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] copy $_ctx into $INSTALL_DIR"
        return 0
    fi

    context_is_complete "$_ctx" || explain_incomplete_context "$_ctx"

    say "Copying the build context into $INSTALL_DIR ..."
    mkdir -p "$INSTALL_DIR"
    (cd "$_ctx" && tar cf - .) | (cd "$INSTALL_DIR" && tar xf -)
    good "Server files in place."
}

# =============================================================================
#  Step 4 -- what address will players use?
# =============================================================================

detect_public_address() {
    for _u in https://ifconfig.me https://api.ipify.org https://icanhazip.com; do
        _ip=$(curl -4 -fsS --max-time 8 "$_u" 2>/dev/null | tr -d ' \r\n')
        case "$_ip" in
            *[0-9].[0-9]*) printf '%s' "$_ip"; return 0 ;;
        esac
    done
    # No internet, or all three are down. Fall back to the address of whichever
    # interface carries the default route.
    _ip=$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.*src \([0-9.]*\).*/\1/p' | head -1)
    [ -n "$_ip" ] && { printf '%s' "$_ip"; return 0; }
    return 1
}

choose_address() {
    step "The address players will connect to"

    if [ "$LOCAL_ONLY" = "1" ]; then
        PUBLIC_ADDRESS="127.0.0.1"
        say "--local was given, so the address is 127.0.0.1 and only this"
        say "machine can reach the server."
        return 0
    fi

    # An update: the address is already in .env, and a server's address does not
    # change because a new version came out. Asking again is noise -- and the
    # kind of noise where a distracted Enter on a freshly detected value quietly
    # repoints every player's client at the wrong place.
    #
    # Unless the machine really did move: a restored snapshot, a migrated VPS, a
    # new address from the provider. That is worth interrupting for, so the
    # stored value is checked against what this machine answers on now, and the
    # question is only asked when the two disagree.
    if [ "$FRESH_INSTALL" = "0" ] && [ -z "$PUBLIC_ADDRESS" ]; then
        _known=$(env_get "$INSTALL_DIR/.env" M2_PUBLIC_ADDRESS || true)
        if [ -n "$_known" ]; then
            _now=$(detect_public_address || true)
            if [ -n "$_now" ] && [ "$_now" != "$_known" ]; then
                warn "Your server sends players to $_known,"
                warn "but this machine answers on $_now."
                say ""
                say "If the machine moved or its address changed, take the new"
                say "one. If $_known is a domain name, or an address that"
                say "points here by another route, keep it."
                say ""
                if ask_yes_no "Send players to $_now from now on?" "n"; then
                    PUBLIC_ADDRESS="$_now"
                else
                    PUBLIC_ADDRESS="$_known"
                fi
            else
                PUBLIC_ADDRESS="$_known"
            fi
            good "Players connect to: $PUBLIC_ADDRESS"
            say "Kept from your settings. To change it, run the installer with"
            say "--address, or edit M2_PUBLIC_ADDRESS in $INSTALL_DIR/.env"
            return 0
        fi
        # No stored address -- an install from before this key existed. Fall
        # through and ask, exactly as a first install does.
    fi

    if [ -z "$PUBLIC_ADDRESS" ]; then
        say "Working out this machine's public address..."
        PUBLIC_ADDRESS=$(detect_public_address || true)
    fi

    if [ -n "$PUBLIC_ADDRESS" ]; then
        good "Detected: $PUBLIC_ADDRESS"
    fi

    PUBLIC_ADDRESS=$(ask_value "Address players type into their client" "$PUBLIC_ADDRESS")

    [ -n "$PUBLIC_ADDRESS" ] || die "Without an address the server cannot work.
  Players would reach the login screen, log in successfully, and then hang
  forever on 'connecting to the server' -- because the login server hands out
  an address that only exists inside the container.

  Find this machine's address with:   curl -4 ifconfig.me
  Then run the installer again with:  --address YOUR.IP.HERE"

    # This is the single most common way a Metin2 server ends up broken, and
    # the symptom (login works, world does not) points at everything except the
    # cause. So it gets its own paragraph.
    say ""
    say "Players will be sent to: ${C_BOLD}${PUBLIC_ADDRESS}${C_RESET}"
    info "If this is wrong, players will log in fine and then hang on"
    info "'connecting to the server' forever. It is the number one cause of"
    info "that. You can change it later in $INSTALL_DIR/.env"
}

choose_domain() {
    [ "$LOCAL_ONLY" = "1" ] && return 0

    # The e-mail is recalled BEFORE the early return below, not inside the
    # remembered-domain block further down, because an operator who passes
    # --domain explicitly -- which is the command the panel prints, and the
    # right thing to do on an update -- would otherwise skip that block and
    # hand acme.sh an empty address. M2_TLS_EMAIL on line 133 only reads the
    # environment, and an update does not have it there; the answer lives in
    # the server's own .env. This is the same defect that once reset a chosen
    # movement-speed bonus to the default on every update.
    if [ -z "$TLS_EMAIL" ] && [ "$FRESH_INSTALL" = "0" ]; then
        TLS_EMAIL=$(env_get "$INSTALL_DIR/.env" M2_TLS_EMAIL 2>/dev/null || true)
    fi

    [ -n "$DOMAIN" ] && return 0

    # Same reasoning as the address above: on an update we already know. Here it
    # matters more, because the prompt's default is empty -- an operator who
    # updates and presses Enter would answer "no domain" for a server that has
    # one, and lose HTTPS in the process.
    if [ "$FRESH_INSTALL" = "0" ] && [ "$DROP_DOMAIN" = "0" ]; then
        _d=$(env_get "$INSTALL_DIR/.env" M2_DOMAIN || true)
        if [ -n "$_d" ]; then
            DOMAIN="$_d"
            [ -n "$TLS_EMAIL" ] || TLS_EMAIL=$(env_get "$INSTALL_DIR/.env" M2_TLS_EMAIL || true)
            step "A domain name for the admin panel"
            good "Using: $DOMAIN"
            say "Kept from your settings. To drop it, run with --no-domain."
            return 0
        fi
    fi

    [ "$ASSUME_YES" = "1" ] && return 0
    [ -r /dev/tty ] || return 0

    step "A domain name for the admin panel (optional)"
    say "If you own a domain and have already pointed it at this server, the"
    say "installer can get a free HTTPS certificate for it, so your admin"
    say "panel and your players' downloads are encrypted."
    say ""
    say "If you do not have one, just press Enter. The panel will work on this"
    say "machine's IP address instead, over plain unencrypted HTTP."
    say ""
    DOMAIN=$(ask_value "Domain name (e.g. panel.myserver.com)" "")
    if [ -n "$DOMAIN" ]; then
        TLS_EMAIL=$(ask_value "Your e-mail (only used for certificate expiry warnings)" "$TLS_EMAIL")
        if [ -z "$TLS_EMAIL" ]; then
            warn "No e-mail given, so no certificate can be requested."
            warn "Carrying on without HTTPS."
            DOMAIN=""
        fi
    fi
}

# =============================================================================
#  Step 5 -- settings and passwords
# =============================================================================

write_env() {
    step "Settings and passwords"

    _env="$INSTALL_DIR/.env"

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] write $_env with freshly generated passwords"
        PANEL_PASSWORD="(generated at install time)"
        return 0
    fi

    if [ ! -f "$_env" ] && [ -f "$INSTALL_DIR/.env.example" ]; then
        cp "$INSTALL_DIR/.env.example" "$_env"
    fi
    touch "$_env"
    chmod 600 "$_env"

    # --- database passwords: keep existing ones, or the database stops
    #     recognising itself. MariaDB stores the root password inside its data
    #     volume at first start; changing .env afterwards does not change it.
    _root_pw=$(env_get "$_env" M2_DB_ROOT_PASSWORD || true)
    _db_pw=$(env_get "$_env" M2_DB_PASSWORD || true)
    # A database volume that already exists, with NO password to go with it, is
    # the one combination that cannot work -- and it fails in a way nobody can
    # read. MariaDB only ever runs its setup on an EMPTY data directory: find a
    # populated one and it keeps whatever passwords it was built with, ignoring
    # everything we hand it. Generating fresh ones here would hand the game and
    # the panel credentials the database has never heard of, and the only trace
    # would be "Access denied for user 'metin2'" in a log nobody thinks to open.
    #
    # It happens by an ordinary route: install once, delete the install
    # directory, install again. The directory held .env; the volume did not go
    # with it.
    if [ -z "$_db_pw" ] || [ -z "$_root_pw" ]; then
        if docker volume inspect "$(stack_project)_db-data" >/dev/null 2>&1; then
            die "There is already a database here from an earlier install, but the
  passwords that go with it are gone -- they lived in the .env file in
  $INSTALL_DIR, which is no longer there.

  Nothing can recover them: they were never written anywhere else, on
  purpose. So you have two ways forward.

  Keep the characters and accounts, if you have that old .env somewhere
  (a backup, another folder), by putting it back and running this again:

      cp /path/to/old/.env $INSTALL_DIR/.env

  Or start the database over -- this DELETES every character and account
  on this server, and cannot be undone:

      cd $INSTALL_DIR && docker compose down -v

  then run this installer again. Everything else -- the built images, the
  downloaded server files, the client -- is kept either way."
        fi
    fi
    if [ -z "$_root_pw" ]; then _root_pw=$(gen_secret); good "database root password: generated"
    else info "database root password: keeping the existing one"; fi
    if [ -z "$_db_pw" ]; then _db_pw=$(gen_secret); good "database password: generated"
    else info "database password: keeping the existing one"; fi

    # --- panel passphrase.
    #
    # The panel writes a PBKDF2 hash of this into m2panel.conf on the config
    # volume at its first start and then never regenerates it, because doing so
    # would invalidate every session cookie. So on a re-install we must report
    # the OLD password, not a shiny new one that does not work.
    _panel_pw=$(env_get "$_env" M2_PANEL_PASSWORD || true)
    _conf_volume_exists=0
    docker volume inspect "$(stack_project)_panel-conf" >/dev/null 2>&1 && _conf_volume_exists=1

    # A passphrase chosen in the panel wins over the one in .env: the panel is
    # where it was last changed, and .env is only this script's note of it.
    # Writing it back below is what keeps the two in step, so every later run
    # finds it in .env without having to reach into a container at all.
    _chosen=$(panel_chosen_passphrase)

    if [ -n "$_chosen" ]; then
        _panel_pw="$_chosen"
        PANEL_PASSWORD="$_chosen"
        PANEL_PASSWORD_KNOWN=1
        PANEL_PASSWORD_NEW=0
        PANEL_PASSWORD_CHOSEN=1
        info "admin panel password: the one you chose in the panel"
    elif [ -n "$_panel_pw" ]; then
        PANEL_PASSWORD="$_panel_pw"
        PANEL_PASSWORD_KNOWN=1
        PANEL_PASSWORD_NEW=0
        info "admin panel password: keeping the existing one"
    elif [ "$_conf_volume_exists" = "1" ]; then
        # There is a panel config from a previous install but no password in
        # .env -- so nobody can tell what it is, including us.
        PANEL_PASSWORD=""
        PANEL_PASSWORD_KNOWN=0
        warn "There is an admin panel from an earlier install, but its password"
        warn "is not recorded here, so I cannot show it to you. The summary at"
        warn "the end explains how to set a new one."
    else
        _panel_pw=$(gen_passphrase)
        PANEL_PASSWORD="$_panel_pw"
        PANEL_PASSWORD_KNOWN=1
        PANEL_PASSWORD_NEW=1
        good "admin panel password: generated (shown at the end)"
    fi

    env_set "$_env" M2_DB_ROOT_PASSWORD "$_root_pw"
    env_set "$_env" M2_DB_PASSWORD      "$_db_pw"
    [ -n "$_panel_pw" ] && env_set "$_env" M2_PANEL_PASSWORD "$_panel_pw"

    env_set "$_env" M2_PUBLIC_ADDRESS "$PUBLIC_ADDRESS"
    env_set "$_env" M2_AUTH_PORT      "$AUTH_PORT"
    env_set "$_env" M2_GAME_PORT_RANGE "$GAME_PORTS"

    # Nothing in the stack reads these two -- nginx and certbot are configured
    # from files, not from here. They are written so the NEXT run of this
    # installer can find them: without them an update has no idea the server
    # ever had a domain, and re-running the plain one-liner would take the
    # certificate away from a server that had HTTPS.
    env_set "$_env" M2_DOMAIN    "$DOMAIN"
    env_set "$_env" M2_TLS_EMAIL "$TLS_EMAIL"

    # The password on the game cores' admin socket -- the one the panel's game
    # master ranks travel over. Generated once and kept, like the others.
    #
    # This is not only a feature switch. The setting used to be empty, and an
    # empty password does not mean "no admin socket": the core compares the
    # line it is sent against the configured password, so an EMPTY line matched
    # and granted full rights -- SHUTDOWN, DC, NOTICE, the lot. Today that is
    # contained, because the socket only answers M2_ADMINPAGE_IP and that is
    # 127.0.0.1. But it is one env var away from being an unauthenticated
    # remote shutdown button, and nobody widening an IP allowlist expects to be
    # turning off the password at the same time.
    _adminpw=$(env_get "$_env" M2_ADMINPAGE_PASSWORD || true)
    [ -n "$_adminpw" ] || _adminpw=$(gen_secret)
    env_set "$_env" M2_ADMINPAGE_PASSWORD "$_adminpw"

    # Tell the panel whether anybody but this machine can reach the server, so
    # its introduction says "give people this address" or "nobody else can join"
    # accordingly. Note this is NOT the same question as the panel's bind
    # address: with --domain the panel also binds to 127.0.0.1, but the server
    # is thoroughly public. Only --local means genuinely local.
    env_set "$_env" M2_LOCAL_ONLY "$([ "$LOCAL_ONLY" = "1" ] && echo 1 || echo 0)"

    # How this machine updates, in one line, for the panel to show when a newer
    # version appears. This installer is idempotent -- it pulls the published
    # version, rebuilds and restarts, and keeps the database, the passwords and
    # the settings -- so re-running it IS the update. Anyone who would rather
    # not pipe a script into a shell has the step-by-step in UPDATING.md.
    env_set "$_env" M2_UPDATE_COMMAND "$(replay_command)"

    # Where the panel listens on the host.
    #
    #   with a domain : 127.0.0.1 only, with nginx in front doing TLS. The
    #                   panel is then not reachable except through HTTPS.
    #   --local       : 127.0.0.1 only, nothing in front. Local use only.
    #   otherwise     : every interface, plain HTTP. There is no proxy to reach
    #                   it through, so binding it to loopback would just mean
    #                   nobody could ever administer the server.
    #
    if [ -n "$DOMAIN" ] || [ "$LOCAL_ONLY" = "1" ]; then
        PANEL_BIND="127.0.0.1"
    else
        PANEL_BIND="0.0.0.0"
    fi
    env_set "$_env" M2_PANEL_BIND_ADDRESS "$PANEL_BIND"
    env_set "$_env" M2_PANEL_PUBLIC_PORT "$PANEL_PORT"
    if [ "$LOCAL_ONLY" = "1" ]; then
        env_set "$_env" M2_HOST_BIND_ADDRESS "127.0.0.1"
    else
        env_set "$_env" M2_HOST_BIND_ADDRESS "0.0.0.0"
    fi

    # The client the panel offers for download is built for this address.
    env_set "$_env" M2_CLIENT_ADDRESS "$PUBLIC_ADDRESS"

    # --- the client that runs in a browser ----------------------------------
    #
    # Whatever the operator set stays set, and an install that has never heard
    # of this gets a 0. The installer does not turn it on by itself: the bridge
    # is only useful once a browser client has been placed on the panel's
    # volume, and before that it would be a port open for nothing.
    #
    # The rest is derived, because getting it wrong is invisible until a player
    # tries to play:
    #
    #   with a domain : the bridge listens on 127.0.0.1 and nginx reaches it
    #                   there, so the page's WebSocket is wss:// on the same
    #                   host and certificate. A page served over HTTPS may not
    #                   open a ws:// connection at all -- the browser refuses
    #                   it as mixed content, silently, and there is nothing the
    #                   page can do about it. So this is not a preference.
    #   otherwise     : the panel is plain HTTP anyway, so the bridge is
    #                   published on its own port and the page connects to it
    #                   directly.
    # choose_clients() has already decided; it read the old value out of what is
    # installed rather than out of this file, so an operator who removed the
    # browser client by hand is not told it is still there.
    BROWSER_PLAY="$WANT_WEB"
    case "$BROWSER_PLAY" in 1|true|yes|on) BROWSER_PLAY=1 ;; *) BROWSER_PLAY=0 ;; esac
    env_set "$_env" M2_BROWSER_PLAY "$BROWSER_PLAY"
    env_set "$_env" M2_MOVE_SPEED_BONUS "${M2_MOVE_SPEED_BONUS:-0}"

    # Written down so the next update replays the same server rather than a
    # slightly different one. This is the whole point of the switch: what a
    # server is running should be readable from the server, not remembered.
    env_set "$_env" M2_CUSTOM_EXPERIENCE "${M2_CUSTOM_EXPERIENCE:-0}"

    # High Risk is only resolved when the Custom Experience is on, so on every
    # other run this reads back what is already here rather than writing the
    # default over it. Without that, turning the Custom Experience off would
    # also quietly turn High Risk back ON for the one operator who had
    # deliberately set it to 0.
    _hr="${M2_HIGH_RISK:-}"
    [ -z "$_hr" ] && _hr=$(env_get "$_env" M2_HIGH_RISK 2>/dev/null || true)
    case "$_hr" in 0|false|no|off|FALSE|NO|OFF) _hr=0 ;; *) _hr=1 ;; esac
    env_set "$_env" M2_HIGH_RISK "$_hr"

    # WHERE THE DESKTOP CLIENT COMES FROM.
    #
    # Its own archive since the package was split, so the builder has to be told
    # -- left empty it looks for the client inside the server files, which is
    # where it used to be and no longer is. A server-only download plus a client
    # builder looking for a client it cannot find is a confusing failure, and it
    # only happens to people who asked for the desktop client.
    #
    # Not overwritten: an operator who set this by hand, to a client of their
    # own with their own patches in it, keeps theirs.
    _old_client_url=$(env_get "$_env" M2_CLIENT_ARCHIVE_URL || true)
    case "$_old_client_url" in
        'https://mega.nz/file/X7YT3BRB#uSVimr2N0y87gTrbdChLUQJWXOnZ49cIkqrc-wJU4MU'|\
        'https://dl.htpsoftware.com/Reference_Client.zip')
            env_set "$_env" M2_CLIENT_ARCHIVE_URL ""
            _old_client_url="" ;;
    esac
    if [ -z "$_old_client_url" ]; then
        _cu=$(pointer_link src_client_url)
        if [ -n "$_cu" ]; then
            env_set "$_env" M2_CLIENT_ARCHIVE_URL "$_cu"
            _cs=$(pointer_get src_client_sha256)
            case "$_cs" in
                ????????????????????????????????????????????????????????????????)
                    env_set "$_env" M2_CLIENT_ARCHIVE_SHA256 "$_cs" ;;
            esac
        fi
    fi
    # The same archive somewhere else. Refreshed every run rather than only
    # when empty, so an installation follows whatever artifacts.json names now
    # instead of keeping a link that has since been retired.
    for _fbk in FALLBACK FALLBACK2; do
        case "$_fbk" in
            FALLBACK)  _fbv=$(pointer_link src_client_url_fallback) ;;
            FALLBACK2) _fbv=$(pointer_link src_client_url_fallback2) ;;
        esac
        env_set "$_env" "M2_CLIENT_ARCHIVE_URL_$_fbk" "$_fbv"
    done
    _bp=$(env_get "$_env" M2_BRIDGE_PORT || true)
    [ -n "$_bp" ] && BRIDGE_PORT="$_bp"
    env_set "$_env" M2_BRIDGE_PORT "$BRIDGE_PORT"
    # Whether the panel may believe X-Forwarded-*. It cannot work this out for
    # itself: it runs in a container, Docker's port forwarder rewrites the
    # source address, and so a request nginx sent to 127.0.0.1 arrives from the
    # bridge gateway. The panel's own loopback test therefore never matched, and
    # it generated every link as though it were being reached directly -- which
    # is how the browser-client button came to name the bridge's own port on an
    # HTTPS page and hand players a ws:// address their browser blocks.
    #
    # Safe because it is set only here, in the arm that also installs nginx: that
    # arm publishes the panel's port on 127.0.0.1 alone, so nothing but nginx can
    # reach the socket and nothing else can forge those headers. Without a domain
    # this stays 0 and the panel keeps testing for the loopback address.
    env_set "$_env" M2_TRUST_PROXY "$([ -n "$DOMAIN" ] && echo 1 || echo 0)"

    if [ -n "$DOMAIN" ] || [ "$LOCAL_ONLY" = "1" ]; then
        env_set "$_env" M2_BRIDGE_BIND_ADDRESS "127.0.0.1"
        # Behind nginx every connection arrives from the same address, so the
        # bridge would count them all as one player and cut the eighth one off.
        # Only safe because of the line above: nothing but nginx can reach it.
        env_set "$_env" M2_BRIDGE_TRUST_PROXY "$([ -n "$DOMAIN" ] && echo 1 || echo 0)"
    else
        env_set "$_env" M2_BRIDGE_BIND_ADDRESS "0.0.0.0"
        env_set "$_env" M2_BRIDGE_TRUST_PROXY "0"
    fi

    good "Settings written to $_env (readable only by root)"
}

write_local_override() {
    # Only --local needs this. A published range is written twice in the base
    # compose file ("${RANGE}:${RANGE}"), so an address cannot be added to it
    # through the environment -- the ports list has to be replaced outright.
    _f="$INSTALL_DIR/docker-compose.override.yml"
    if [ "$LOCAL_ONLY" != "1" ]; then
        # A leftover override from an earlier --local run would silently keep
        # the server private. Take it away and say so.
        if [ -f "$_f" ] && grep -q 'written by install.sh' "$_f" 2>/dev/null; then
            info "removing the loopback-only override from an earlier --local run"
            run rm -f "$_f"
        fi
        return 0
    fi
    compose_supports_override || die "--local needs Docker Compose v2.24 or
  newer (this is $(docker compose version --short 2>/dev/null)). Update Docker,
  or leave --local off."
    [ "$DRY_RUN" = "1" ] && { info "[dry-run] write $_f"; return 0; }
    cat > "$_f" <<OVR
# written by install.sh -- do not edit; re-run the installer instead.
#
# --local was used: everything binds 127.0.0.1, so only this machine can
# reach the server. Delete this file and re-run the installer without
# --local to let other people connect.
services:
  game:
    ports: !override
      - "127.0.0.1:${AUTH_PORT}:11000/tcp"
      - "127.0.0.1:${GAME_PORTS}:${GAME_PORTS}/tcp"
OVR
    good "Loopback-only override written."
}

# =============================================================================
#  Step 6 -- build and start
# =============================================================================

start_stack() {
    step "Building and starting the server"

    if [ "$FRESH_INSTALL" = "1" ]; then
        say "The first run compiles the game server from its original source"
        say "code -- about 195 files. On a 2-core VPS that takes 15 to 25"
        say "minutes. It only happens once; starting it again later takes"
        say "seconds."
        say ""
        say "You will see a lot of compiler output. That is normal."
    fi
    say ""

    if [ "$DRY_RUN" = "1" ]; then
        [ "$FRESH_INSTALL" = "1" ] || info "[dry-run] stop game and panel before the Playerbot migration"
        info "[dry-run] cd $INSTALL_DIR && docker compose up -d --build"
        return 0
    fi

    # Names are fixed in the compose file (container_name: metin2-game and so
    # on), so a container of the same name from something else would collide
    # with a confusing error. Check for that first.
    for _n in $(stack_container_names); do
        if docker inspect "$_n" >/dev/null 2>&1; then
            _proj=$(docker inspect -f '{{index .Config.Labels "com.docker.compose.project.working_dir"}}' "$_n" 2>/dev/null || true)
            if [ -n "$_proj" ] && [ "$_proj" != "$INSTALL_DIR" ]; then
                die "A container called '$_n' already exists, and it belongs to
  another copy of this server in:

      $_proj

  Only one Metin2 server can run per machine, because the container names are
  fixed. Either use that installation, or remove it first:

      cd $_proj && docker compose down"
            fi
        fi
    done

    # `depends_on' orders newly started containers; it cannot pause an old game
    # that is already running during an update. Stop both DB consumers first so
    # the one-shot additive seed is always the only writer in its PID range.
    if [ "$FRESH_INSTALL" != "1" ]; then
        info "Stopping game and panel while the Playerbot seed is checked."
        dc stop game panel || die "The running game or panel could not be stopped.
  The Playerbot seed was not started; fix the Docker error above and retry."
    fi

    if ! dc up -d --build; then
        dc logs --no-color --tail=80 playerbot-migrate 2>/dev/null || true
        say ""
        die "The server did not build or did not start. The output above says
  why. The usual causes, most common first:

    - not enough disk space   ->  df -h /var/lib/docker
    - not enough memory during the build (the compiler is killed silently)
    - no internet access from the build (it downloads Ubuntu packages)

  Nothing was left running that would get in the way. When you have fixed it,
  run this installer again -- the parts that already built are cached, so it
  picks up where it stopped."
    fi

    good "Containers are up."
}

container_health() {
    _svc="$1"
    _cid=$(dc ps -q "$_svc" 2>/dev/null | head -1)
    [ -n "$_cid" ] || { printf 'missing'; return 0; }
    docker inspect -f '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "$_cid" 2>/dev/null || printf 'unknown'
}

wait_healthy() {
    step "Waiting for the server to come up"

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] wait for mariadb, game and panel to report healthy"
        return 0
    fi

    say "The database imports the shipped world on its very first start, and"
    say "the game cores boot one after another. Give it two or three minutes."
    say ""

    _deadline=$(( $(date +%s) + 420 ))
    _last=""
    while [ "$(date +%s)" -lt "$_deadline" ]; do
        _db=$(container_health mariadb)
        _gm=$(container_health game)
        _pn=$(container_health panel)
        _now="db=$_db game=$_gm panel=$_pn"
        [ "$_now" != "$_last" ] && { info "$_now"; _last="$_now"; }
        case "$_gm:$_pn" in
            healthy:healthy|healthy:running|running:healthy|running:running)
                good "The server is up."
                return 0 ;;
        esac
        case "$_gm" in exited|dead) break ;; esac
        sleep 5
    done

    warn "The server has not reported itself healthy yet."
    warn "It may still be starting. Check with:"
    warn "    cd $INSTALL_DIR && docker compose ps"
    warn "    cd $INSTALL_DIR && docker compose logs game --tail 50"
    return 0
}

# =============================================================================
#  Step 7 -- the firewall
# =============================================================================

detect_firewall() {
    if have ufw && ufw status 2>/dev/null | head -1 | grep -qi 'status:'; then
        FIREWALL="ufw"
    elif have firewall-cmd && firewall-cmd --state >/dev/null 2>&1; then
        FIREWALL="firewalld"
    elif have iptables; then
        FIREWALL="iptables"
    else
        FIREWALL="none"
    fi
}

open_firewall() {
    [ "$LOCAL_ONLY" = "1" ] && return 0
    [ "$SKIP_FIREWALL" = "1" ] && { info "skipping the firewall (--no-firewall)"; return 0; }

    step "The firewall"
    detect_firewall

    _game_from=$(printf '%s' "$GAME_PORTS" | cut -d- -f1)
    _game_to=$(printf '%s' "$GAME_PORTS" | cut -d- -f2)

    case "$FIREWALL" in
        ufw)
            say "Found ufw. Allowing the game ports."
            run ufw allow "${AUTH_PORT}/tcp"                    >/dev/null 2>&1 || true
            run ufw allow "${_game_from}:${_game_to}/tcp"       >/dev/null 2>&1 || true
            if [ -n "$DOMAIN" ]; then
                run ufw allow 80/tcp  >/dev/null 2>&1 || true
                run ufw allow 443/tcp >/dev/null 2>&1 || true
            else
                run ufw allow "${PANEL_PORT}/tcp" >/dev/null 2>&1 || true
                # Only when the browser client is switched on, and only without
                # a domain: with one, nginx reaches the bridge over loopback and
                # nothing needs to be opened for it.
                [ "$BROWSER_PLAY" = "1" ] &&                     run ufw allow "${BRIDGE_PORT}/tcp" >/dev/null 2>&1 || true
            fi
            good "ufw rules added."
            ufw_docker_warning ;;
        firewalld)
            say "Found firewalld. Allowing the game ports."
            run firewall-cmd --permanent --add-port="${AUTH_PORT}/tcp"          >/dev/null 2>&1 || true
            run firewall-cmd --permanent --add-port="${_game_from}-${_game_to}/tcp" >/dev/null 2>&1 || true
            if [ -n "$DOMAIN" ]; then
                run firewall-cmd --permanent --add-port=80/tcp  >/dev/null 2>&1 || true
                run firewall-cmd --permanent --add-port=443/tcp >/dev/null 2>&1 || true
            else
                run firewall-cmd --permanent --add-port="${PANEL_PORT}/tcp" >/dev/null 2>&1 || true
                [ "$BROWSER_PLAY" = "1" ] &&                     run firewall-cmd --permanent --add-port="${BRIDGE_PORT}/tcp" >/dev/null 2>&1 || true
            fi
            run firewall-cmd --reload >/dev/null 2>&1 || true
            good "firewalld rules added and reloaded."
            info "firewalld and Docker also interact: Docker adds its own rules"
            info "for published ports, so those ports are reachable whatever"
            info "firewalld's zones say." ;;
        iptables)
            say "Found plain iptables (no ufw, no firewalld)."
            run iptables -I INPUT -p tcp --dport "$AUTH_PORT" -j ACCEPT 2>/dev/null || true
            run iptables -I INPUT -p tcp --dport "$PANEL_PORT" -j ACCEPT 2>/dev/null || true
            [ "$BROWSER_PLAY" = "1" ] && [ -z "$DOMAIN" ] &&                 run iptables -I INPUT -p tcp --dport "$BRIDGE_PORT" -j ACCEPT 2>/dev/null || true
            run iptables -I INPUT -p tcp --match multiport --dports "${_game_from}:${_game_to}" -j ACCEPT 2>/dev/null || true
            if [ -n "$DOMAIN" ]; then
                run iptables -I INPUT -p tcp --dport 80 -j ACCEPT 2>/dev/null || true
                run iptables -I INPUT -p tcp --dport 443 -j ACCEPT 2>/dev/null || true
            fi
            warn "These iptables rules are NOT saved and will disappear on the"
            warn "next reboot. To keep them, install iptables-persistent:"
            warn "    apt-get install -y iptables-persistent"
            warn "(Docker's own rules come back by themselves, so the game"
            warn "ports will still work after a reboot -- see below.)" ;;
        none)
            say "This machine has no firewall running (no ufw, no firewalld,"
            say "no iptables), so there is nothing here to open."
            say ""
            warn "Your VPS provider may still have a firewall of its own, in"
            warn "their web control panel, outside this machine. Hetzner,"
            warn "Oracle Cloud, AWS and Google Cloud all do this by default."
            warn ""
            warn "If players cannot connect but the server is running, that is"
            warn "almost certainly where the block is. Open TCP ports:"
            warn "    ${AUTH_PORT}  and  ${GAME_PORTS}"
            if [ -n "$DOMAIN" ]; then warn "    80 and 443"; else warn "    ${PANEL_PORT}"; fi
            [ "$BROWSER_PLAY" = "1" ] && [ -z "$DOMAIN" ] && warn "    ${BRIDGE_PORT}  (playing in the browser)" ;;
    esac
}

# The footgun that costs people a whole evening. It deserves plain words.
ufw_docker_warning() {
    say ""
    printf '  %s%sWorth knowing about ufw and Docker%s\n' "$C_BOLD" "$C_YELLOW" "$C_RESET"
    say ""
    say "  Docker does not go through ufw. When Docker publishes a port it"
    say "  writes its own rules further up the chain, so the port is open to"
    say "  the internet whether ufw allows it or not."
    say ""
    say "  Two consequences:"
    say ""
    say "  1. The rules just added are not what makes the game reachable --"
    say "     Docker already did that. They are there so that 'ufw status'"
    say "     tells you the truth about what is open."
    say ""
    say "  2. More important: 'ufw deny 11000' will NOT close that port."
    say "     People assume it does. It does not. To really close a published"
    say "     port you must stop publishing it -- change the ports: lines in"
    say "     $INSTALL_DIR/docker-compose.yml -- or add a rule to the"
    say "     DOCKER-USER iptables chain, which Docker does consult."
    say ""
    say "  This is why the database in this stack is not published at all, and"
    say "  why the admin panel is bound to 127.0.0.1 whenever there is an"
    say "  HTTPS proxy in front of it. Those are real protections; a ufw deny"
    say "  on a published port is not."
    say ""
}

# =============================================================================
#  Step 8 -- HTTPS, if there is a domain
# =============================================================================

setup_tls() {
    [ -n "$DOMAIN" ] || return 0
    step "HTTPS for $DOMAIN"

    if [ "$OS_FAMILY" != "debian" ]; then
        warn "Automatic HTTPS is only set up for Debian and Ubuntu, and this"
        warn "is $OS_NAME. Skipping it."
        warn "The panel is still reachable -- see the summary."
        DOMAIN=""
        # Re-publish the panel, since nothing will be proxying to it.
        env_set "$INSTALL_DIR/.env" M2_PANEL_PUBLIC_PORT "$PANEL_PORT"
        env_set "$INSTALL_DIR/.env" M2_PANEL_BIND_ADDRESS "0.0.0.0"
        PANEL_BIND="0.0.0.0"
        dc up -d panel >/dev/null 2>&1 || true
        return 0
    fi

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] install nginx + acme.sh, get a certificate for $DOMAIN"
        return 0
    fi

    # The certificate authority has to reach this machine under exactly this
    # name. Check before installing anything that would need undoing.
    say "Checking that $DOMAIN points at this server..."
    _resolved=""
    _can_resolve=0
    if have getent; then
        _can_resolve=1
        _resolved=$(getent ahostsv4 "$DOMAIN" 2>/dev/null | awk 'NR==1{print $1}')
    elif have host; then
        _can_resolve=1
        _resolved=$(host -t A "$DOMAIN" 2>/dev/null | awk '/has address/{print $4; exit}')
    fi
    if [ "$_can_resolve" = "0" ]; then
        info "no DNS lookup tool here, so the name cannot be checked first."
        info "Carrying on; Let's Encrypt will tell us if it is wrong."
        _resolved="$PUBLIC_ADDRESS"
    fi
    if [ -z "$_resolved" ]; then
        warn "$DOMAIN does not resolve to anything yet."
        warn "The certificate cannot be issued until it does. Create the DNS"
        warn "record (an A record pointing at $PUBLIC_ADDRESS), wait a few"
        warn "minutes, then run this installer again."
        _tls_fallback; return 0
    fi
    good "$DOMAIN -> $_resolved"
    if [ "$_resolved" != "$PUBLIC_ADDRESS" ]; then
        info "(that is not this server's own address, which is normal if you"
        info " use Cloudflare's proxy -- it passes ports 80 and 443 through,"
        info " and the certificate check has been verified to work through it)"
    fi

    say "Installing nginx..."
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq >/dev/null 2>&1 || true
    apt-get install -y -qq nginx curl cron >/dev/null 2>&1 || {
        warn "nginx could not be installed. Carrying on without HTTPS."
        _tls_fallback; return 0
    }

    _webroot=/var/www/acme
    _ssldir="/etc/nginx/ssl/$DOMAIN"
    mkdir -p "$_webroot/.well-known/acme-challenge" "$_ssldir"
    chown -R www-data:www-data "$_webroot" 2>/dev/null || true

    # Debian's stock site owns port 80 as default_server and would shadow ours.
    rm -f /etc/nginx/sites-enabled/default

    # --- bootstrap: answer on port 80 so the challenge can be fetched
    cat > /etc/nginx/conf.d/metin2.conf <<NGX
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name $DOMAIN;
    location /.well-known/acme-challenge/ { root $_webroot; }
    location / { return 200 "setting up\n"; }
}
NGX
    nginx -t >/dev/null 2>&1 || { warn "nginx rejected its own bootstrap config."; _tls_fallback; return 0; }
    systemctl enable --now nginx >/dev/null 2>&1 || service nginx start >/dev/null 2>&1 || true
    systemctl reload nginx >/dev/null 2>&1 || service nginx reload >/dev/null 2>&1 || true

    # --- acme.sh
    _acme=""
    [ -x /root/.acme.sh/acme.sh ] && _acme=/root/.acme.sh/acme.sh
    if [ -z "$_acme" ]; then
        say "Installing acme.sh (the Let's Encrypt client)..."
        curl -fsS https://get.acme.sh 2>/dev/null | sh -s "email=$TLS_EMAIL" >/dev/null 2>&1 || true
        [ -x /root/.acme.sh/acme.sh ] && _acme=/root/.acme.sh/acme.sh
    fi
    [ -n "$_acme" ] || { warn "acme.sh could not be installed. No HTTPS."; _tls_fallback; return 0; }

    if [ ! -f "$_ssldir/fullchain.pem" ]; then
        say "Asking Let's Encrypt for a certificate. This takes about a minute."
        "$_acme" --register-account -m "$TLS_EMAIL" --server letsencrypt >/dev/null 2>&1 || true
        if ! "$_acme" --issue -d "$DOMAIN" -w "$_webroot" --server letsencrypt --keylength ec-256 >/tmp/acme-issue.log 2>&1; then
            tail -25 /tmp/acme-issue.log
            warn ""
            warn "The certificate could not be issued -- the last lines of the"
            warn "attempt are above, and all of it is in /tmp/acme-issue.log."
            warn "The usual reasons: the domain does not point here yet, or"
            warn "port 80 is blocked by the provider's firewall."
            warn "Carrying on without HTTPS; re-run the installer to try again."
            _tls_fallback; return 0
        fi
        "$_acme" --install-cert -d "$DOMAIN" --ecc \
            --key-file "$_ssldir/key.pem" --fullchain-file "$_ssldir/fullchain.pem" \
            --reloadcmd "systemctl reload nginx" >/dev/null 2>&1 || {
                warn "The certificate was issued but could not be installed."
                _tls_fallback; return 0
            }
        good "Certificate installed. It renews itself."
    else
        good "A certificate for $DOMAIN is already here -- keeping it."
    fi

    _write_nginx_conf
    nginx -t >/dev/null 2>&1 || { nginx -t; warn "nginx rejected the configuration."; _tls_fallback; return 0; }
    systemctl reload nginx >/dev/null 2>&1 || service nginx reload >/dev/null 2>&1 || true

    good "https://$DOMAIN is serving the admin panel."
}

# Called whenever HTTPS could not be set up. The panel must stay reachable
# somehow, so it goes back to being published on the machine's own address.
_tls_fallback() {
    DOMAIN=""
    PANEL_BIND="0.0.0.0"
    env_set "$INSTALL_DIR/.env" M2_PANEL_PUBLIC_PORT "$PANEL_PORT"
    env_set "$INSTALL_DIR/.env" M2_PANEL_BIND_ADDRESS "0.0.0.0"
    dc up -d panel >/dev/null 2>&1 || true
    # The firewall was opened for 80 and 443 on the assumption that nginx would
    # be answering on them. It is not, and the panel is now on its own port
    # instead, so that port has to be let through as well.
    case "$FIREWALL" in
        ufw)       ufw allow "${PANEL_PORT}/tcp" >/dev/null 2>&1 || true ;;
        firewalld) firewall-cmd --permanent --add-port="${PANEL_PORT}/tcp" >/dev/null 2>&1 || true
                   firewall-cmd --reload >/dev/null 2>&1 || true ;;
        iptables)  iptables -I INPUT -p tcp --dport "$PANEL_PORT" -j ACCEPT 2>/dev/null || true ;;
        *)         warn "Open TCP port ${PANEL_PORT} in your provider's control panel"
                   warn "so you can reach the admin panel." ;;
    esac
    warn ""
    warn "The admin panel is now on http://${PUBLIC_ADDRESS}:${PANEL_PORT} over"
    warn "plain, unencrypted HTTP. Re-run the installer once the domain"
    warn "resolves and it will switch to HTTPS."
}

_write_nginx_conf() {
    _webroot=/var/www/acme
    _ssldir="/etc/nginx/ssl/$DOMAIN"

    # The panel hands a client download over to nginx with X-Accel-Redirect
    # whenever it can see it is behind a proxy, so that a 1.2 GB transfer does
    # not occupy one of its eight worker threads for half an hour. For that to
    # work, nginx must be able to read the file out of the panel's Docker
    # volume -- and /var/lib/docker is mode 0710, so by default www-data cannot
    # even walk into it.
    #
    # We try to make that work, and if we cannot, we fall back to letting the
    # panel send the file itself: the download still works, it is just less
    # efficient. The one thing we must not do is emit an X-Accel path that
    # nginx cannot read, which would turn every download into a 404.
    NGINX_ACCEL="no"
    _zip_dir=$(docker volume inspect -f '{{.Mountpoint}}' "$(stack_project)_panel-data" 2>/dev/null || true)
    if [ -n "$_zip_dir" ] && [ -d "$_zip_dir" ]; then
        if ! su -s /bin/sh www-data -c "test -x '$_zip_dir'" 2>/dev/null; then
            # Grant traverse only (o+x, not o+r): the directory cannot be
            # listed, and only paths that are already known can be reached.
            chmod o+x /var/lib/docker 2>/dev/null || true
            chmod o+x /var/lib/docker/volumes 2>/dev/null || true
        fi
        if su -s /bin/sh www-data -c "test -x '$_zip_dir'" 2>/dev/null; then
            NGINX_ACCEL="yes"
        fi
    fi

    # ---- the browser client's 1.7 GB of static files -----------------------
    #
    # 421 files: index.html, index.js, a 14.7 MB index.wasm, a 3.1 MB
    # manifest.bin and 408 content-addressed blobs of about 4 MB each. Pushing
    # that through the panel would occupy its worker threads for as long as a
    # player is loading, which is minutes -- the same reason the client download
    # is handed to nginx. Here it is simpler: the files are static, so nginx
    # serves them itself and the panel is not in the path at all.
    #
    # The three caching rules are taken from the client's own static server
    # (dist/browser/serve-webfs.py), not invented. Getting them wrong is not
    # slow, it is broken: a cached manifest.bin makes a patched client load the
    # previous corpus, which presents as missing files rather than as a stale
    # cache.
    #
    # Only written when nginx can actually read the panel's volume -- the same
    # test the client download already does. Otherwise the panel serves /play
    # itself, which works and is slow, and that is far better than a location
    # that 404s every blob.
    if [ "$NGINX_ACCEL" = "yes" ] && [ -n "$_zip_dir" ]; then
        _play_block="    # /play without the slash goes to the PANEL, which redirects to /play/
    # with the parameters that tell the client where the bridge is. A redirect
    # here would drop them, and the player would land on the connection dialog
    # being asked for an address they have no way of knowing.
    location = /play {
        proxy_pass http://127.0.0.1:$PANEL_PORT;
        proxy_set_header Host              \$host;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_set_header X-Forwarded-Host  \$host;
        proxy_set_header X-Forwarded-For   \$remote_addr;
    }

    location /play/ {
        # browser/current, not browser. current is a symlink to the version
        # being served, and pointing at its parent would serve the directory
        # that CONTAINS the versions -- every request a 404 while the files sit
        # one level down. Swinging that symlink is how a new version goes live
        # and how a rollback happens, so this path itself never changes.
        alias $_zip_dir/browser/current/;
        index index.html;
        access_log off;

        types {
            application/wasm              wasm;
            text/javascript               js;
            text/html                     html;
            text/plain                    dev;
            application/octet-stream      bin;
        }
        default_type application/octet-stream;

        # gzip_static picks up <blob>.gz twins if the operator ever builds them
        # (build-webfs.py --gzip). The blobs are not compressed on the fly:
        # doing it per visitor for 1.7 GB costs more than the 40% it saves. The
        # small mutable files are worth it and are.
        gzip on;
        gzip_types text/javascript application/wasm text/html;
        gzip_min_length 4096;
        gzip_static on;

        # The rule per file comes from the map above, which is where the three
        # cases are written out.
        # always, so the header reaches error responses too -- it has to,
        # because on an error what it says is do not keep this. The status map
        # above is what makes that true; see it for what went wrong while this
        # line named \$m2_play_cache directly.
        add_header Cache-Control \$m2_play_cache_final always;
        add_header Access-Control-Allow-Origin \"*\" always;
    }"
    fi

    # Real visitor addresses when Cloudflare is in front. Without this the
    # panel's per-IP download quota and login rate limit would see every
    # visitor as the same one and lock everybody out together.
    _cfconf=/etc/nginx/conf.d/metin2-cloudflare.conf
    {
        for _u in https://www.cloudflare.com/ips-v4 https://www.cloudflare.com/ips-v6; do
            curl -fsS --max-time 10 "$_u" 2>/dev/null | while read -r _net; do
                [ -n "$_net" ] && printf 'set_real_ip_from %s;\n' "$_net"
            done
        done
        printf 'real_ip_header CF-Connecting-IP;\n'
    } > "$_cfconf.new" 2>/dev/null || true
    if [ "$(grep -c set_real_ip_from "$_cfconf.new" 2>/dev/null || echo 0)" -ge 10 ]; then
        mv "$_cfconf.new" "$_cfconf"
    else
        rm -f "$_cfconf.new"
        printf '# Cloudflare ranges could not be downloaded; re-run the installer to fill this in.\n' > "$_cfconf"
    fi

    if [ "$NGINX_ACCEL" = "yes" ]; then
        _accel_block="    location /_client_zip {
        internal;
        alias $_zip_dir/client.zip;
    }"
        _dl_proto='        proxy_set_header X-Forwarded-Proto $scheme;'
    else
        _accel_block="    # nginx cannot read the panel's data volume, so the panel sends the
    # client download itself. It works; it just uses one of the panel's
    # worker threads for the length of the transfer."
        # Leaving X-Forwarded-Proto off for this one route is what tells the
        # panel not to try the hand-off. X-Forwarded-For stays, so the per-IP
        # download quota still counts real visitors.
        _dl_proto='        # X-Forwarded-Proto deliberately omitted -- see above.'
    fi

    # HTTP/2 is spelled two different ways depending on the nginx version, and
    # getting it wrong is not a warning -- nginx refuses to start at all.
    #
    #   up to 1.25.0   a parameter on the listen line:  listen 443 ssl http2;
    #   1.25.1 onward  a directive of its own:          http2 on;
    #
    # Ubuntu 24.04 ships 1.24.0 and Debian 12 ships 1.22.1, so the old form is
    # what most machines running this need -- but one on nginx mainline needs
    # the new one. Ask nginx which it is rather than guessing.
    _ngxver=$(nginx -v 2>&1 | sed -n 's#.*nginx/\([0-9][0-9.]*\).*#\1#p')
    _v1=$(printf '%s' "$_ngxver" | cut -d. -f1)
    _v2=$(printf '%s' "$_ngxver" | cut -d. -f2)
    _v3=$(printf '%s' "$_ngxver" | cut -d. -f3)
    _newstyle=0
    if   [ "${_v1:-0}" -gt 1 ]; then _newstyle=1
    elif [ "${_v1:-0}" -eq 1 ] && [ "${_v2:-0}" -gt 25 ]; then _newstyle=1
    elif [ "${_v1:-0}" -eq 1 ] && [ "${_v2:-0}" -eq 25 ] && [ "${_v3:-0}" -ge 1 ]; then _newstyle=1
    fi
    if [ "$_newstyle" = "1" ]; then
        _listen443='listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    http2 on;'
    else
        _listen443='listen 443 ssl http2 default_server;
    listen [::]:443 ssl http2 default_server;'
    fi
    info "nginx ${_ngxver:-version unknown}"

    cat > /etc/nginx/conf.d/metin2.conf <<NGX
# Admin panel TLS front end. Written by install.sh -- re-run it rather than
# editing this by hand.
#
# nginx owns 80 and 443. The panel itself listens only on 127.0.0.1:$PANEL_PORT
# and is not reachable from the internet except through here.

limit_req_zone \$binary_remote_addr zone=paneldl:10m rate=10r/m;
limit_req_status 429;

map \$request_method \$auth_key { default ""; POST \$binary_remote_addr; }
limit_req_zone \$auth_key zone=panelauth:10m rate=10r/m;

# How long each of the browser client's files may be cached. The three cases are
# the client's own (dist/browser/serve-webfs.py), and they are rules rather than
# tuning: <hash>.bin can be kept forever because the name IS the hash of the
# contents, and manifest.bin must never be kept because a cached one makes a
# patched client load the previous corpus -- which presents as missing files,
# not as a stale cache.
map \$uri \$m2_play_cache {
    default                          "no-cache";
    "~^/play/[0-9a-f]{32}\.bin\$"      "public, max-age=31536000, immutable";
    "~^/play/manifest\.bin\$"          "no-store";
    "~^/play/index\.dev\$"             "no-store";
}

# ...but only for an answer that carries the file. A 404 must never be kept, and
# the rule above would have kept one for a YEAR: the header went out with
# \`always', which applies it to error responses too, so a request that arrived
# while the client was still being installed taught the CDN that a chunk does
# not exist -- permanently, and the name being a content hash means nothing can
# ever invalidate it. MEASURED on the live server: a 4.2 MB chunk present on
# disk, served as a cached 404 with cf-cache-status HIT. It presents as files
# missing from the game rather than as a caching problem, because the client
# retries and then gives up.
map \$status \$m2_play_cache_final {
    default   "no-store";
    "200"     \$m2_play_cache;
    "206"     \$m2_play_cache;
    "304"     \$m2_play_cache;
}

server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name $DOMAIN;
    # stays plain HTTP: this is how the certificate renews itself
    location /.well-known/acme-challenge/ { root $_webroot; }
    location / { return 301 https://\$host\$request_uri; }
}

server {
    $_listen443
    server_name $DOMAIN;

    ssl_certificate     $_ssldir/fullchain.pem;
    ssl_certificate_key $_ssldir/key.pem;
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_session_cache   shared:SSL:10m;
    ssl_session_timeout 1d;

    add_header Strict-Transport-Security "max-age=31536000" always;
    add_header X-Content-Type-Options    nosniff always;
    add_header X-Frame-Options           SAMEORIGIN always;

    client_max_body_size 16m;
    server_tokens off;

    location /.well-known/acme-challenge/ { root $_webroot; }

$_accel_block

    # The client download. Rate-limited here as a first line of defence; the
    # panel enforces its own per-address daily quota behind this.
    location = /download {
        limit_req zone=paneldl burst=5 nodelay;
        proxy_pass http://127.0.0.1:$PANEL_PORT;
        proxy_set_header Host             \$host;
$_dl_proto
        proxy_set_header X-Forwarded-Host \$host;
        proxy_set_header X-Forwarded-For  \$remote_addr;
        proxy_read_timeout 3600s;
        proxy_buffering    off;
    }

    # ---- the client that runs in a browser ---------------------------------
    #
    # Written whether or not anyone has switched it on: with the bridge stopped
    # these answer 502 to requests nobody makes, and having them here means
    # turning the feature on later needs no change to nginx.
    #
    # THE SHAPE IS THE CLIENT'S, NOT A CHOICE. The browser client reads the
    # bridge's address out of its own page URL with
    #     /[?&]serverHost=([A-Za-z0-9.\-]+)/
    # -- a character class that excludes "/" deliberately, so a path cannot be
    # smuggled into it. The bridge can therefore only ever be named as
    # host:port, never as a path on this site. And a page served over HTTPS may
    # not open a ws:// connection at all: the browser blocks it as mixed
    # content, silently. The two together leave exactly one arrangement that
    # works behind TLS -- this host and this port, with the two URL prefixes
    # the client actually uses routed through to the bridge:
    #
    #   /to/<host>:<port>   every game connection, as a WebSocket
    #   /ping               the page's connection gate, before it will start
    #
    # so the panel tells the page serverHost=<this domain> and serverPort=443.
    location /to/ {
        proxy_pass http://127.0.0.1:$BRIDGE_PORT;
        proxy_http_version 1.1;
        proxy_set_header Upgrade    \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host             \$host;
        proxy_set_header X-Forwarded-For  \$remote_addr;
        # A player standing still sends nothing for minutes at a time, and the
        # default of 60s would drop them mid-game.
        proxy_read_timeout  3600s;
        proxy_send_timeout  3600s;
        proxy_buffering     off;
    }

    location = /ping {
        proxy_pass http://127.0.0.1:$BRIDGE_PORT/ping;
        proxy_set_header Host            \$host;
        proxy_set_header X-Forwarded-For \$remote_addr;
    }

$_play_block

    # Everything with a password on it.
    location ~ ^/(login|register|account(/.*)?|reset(/.*)?)\$ {
        limit_req zone=panelauth burst=10 nodelay;
        proxy_pass http://127.0.0.1:$PANEL_PORT;
        proxy_set_header Host              \$host;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_set_header X-Forwarded-Host  \$host;
        proxy_set_header X-Forwarded-For   \$remote_addr;
        proxy_read_timeout 300s;
        proxy_buffering    off;
    }

    location / {
        proxy_pass http://127.0.0.1:$PANEL_PORT;
        proxy_set_header Host              \$host;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_set_header X-Forwarded-Host  \$host;
        proxy_set_header X-Forwarded-For   \$remote_addr;
        proxy_read_timeout 300s;
        proxy_buffering    off;
    }
}
NGX
}

# =============================================================================
#  The client that runs in a browser.
#
#  Two halves, and this handles the half that lives in the stack: the bridge
#  that turns a browser's WebSocket into a TCP connection to the game. The
#  other half -- the client itself, built to WebAssembly -- is placed on the
#  panel's volume by hand, because it is built from game data that is not ours
#  to redistribute.
#
#  The bridge is in a compose profile, so it is started only on purpose. An
#  install that has not switched this on never builds it, never runs it and is
#  not affected by any of this.
# =============================================================================
start_browser_bridge() {
    [ "$DRY_RUN" = "1" ] && { info "[dry-run] browser bridge: M2_BROWSER_PLAY=$BROWSER_PLAY"; return 0; }

    if [ "$BROWSER_PLAY" != "1" ]; then
        # Switched off. If a previous run started it, take it away -- leaving a
        # port open that the panel no longer points anybody at is the worst of
        # both answers.
        if docker inspect metin2-wsbridge >/dev/null 2>&1; then
            step "Playing in the browser"
            say "M2_BROWSER_PLAY is 0 now, so the WebSocket bridge is being stopped."
            dc --profile browser rm -sf wsbridge >/dev/null 2>&1 || true
            good "The bridge is stopped. The game itself is untouched."
        fi
        return 0
    fi

    step "Playing in the browser"

    if ! dc --profile browser config --services 2>/dev/null | grep -qx 'wsbridge'; then
        warn "M2_BROWSER_PLAY=1, but these server files have no wsbridge service."
        warn "They are older than this feature. Nothing else is affected."
        return 0
    fi

    say "Starting the WebSocket bridge, which is what lets a browser reach a"
    say "game server that speaks TCP."
    if dc --profile browser up -d --build wsbridge; then
        good "The bridge is running."
        if [ -n "$DOMAIN" ]; then
            info "reachable at https://$DOMAIN/to/ and /ping -- nginx gives it"
            info "the certificate, on the panel's own port."
        else
            info "reachable on port $BRIDGE_PORT."
        fi
        say ""
        say "The panel shows a 'Play in the browser' button once a browser"
        say "client is on its volume, and not before -- there would be nothing"
        say "behind it. This installer puts one there for you; if the button is"
        say "missing, run it again and say yes to the browser client:"
        say "    curl -fsSL \$M2_INSTALLER_URL | sudo sh -s -- --web-client"
    else
        warn "The bridge did not start. Playing in the browser will not work;"
        warn "everything else is unaffected. To see why:"
        warn "    cd $INSTALL_DIR && docker compose --profile browser logs wsbridge"
    fi
}

# =============================================================================
#  Step 9 -- the game client
#
#  Built by a separate compose service. It downloads a large archive, patches
#  the address the client connects to, repacks it, and leaves client.zip where
#  the panel serves it from. That takes a long time, so it runs in the
#  background and we report honestly on where it got to.
# =============================================================================

client_zip_present() {
    dc exec -T panel test -f /usr/local/m2panel/client.zip >/dev/null 2>&1
}

start_client_build() {
    step "The game client for your players"

    if [ "$SKIP_CLIENT" = "1" ]; then
        info "skipped (--no-client)"
        CLIENT_STATE="skipped"
        return 0
    fi
    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] docker compose run --rm client-builder"
        CLIENT_STATE="building"
        return 0
    fi

    if client_zip_present; then
        good "A patched client is already in place."
        CLIENT_STATE="ready"
        return 0
    fi

    # client-builder sits behind the "client" profile, so a plain
    # `config --services' does not list it at all. `run' turns the profile on
    # by itself, but this check has to ask for it explicitly.
    if ! dc --profile client config --services 2>/dev/null | grep -qx 'client-builder'; then
        warn "This release does not include the automatic client builder."
        warn "The server works; players just have nothing to download yet."
        warn ""
        warn "To provide one yourself, put a client.zip whose serverinfo.py"
        warn "points at $PUBLIC_ADDRESS in place with:"
        warn "    cd $INSTALL_DIR"
        warn "    docker compose cp ./client.zip panel:/usr/local/m2panel/client.zip"
        warn "    docker compose restart panel"
        CLIENT_STATE="unavailable"
        return 0
    fi

    CLIENT_LOG="$INSTALL_DIR/client-build.log"

    # The archive fetch_sources already downloaded is the SAME file the client
    # builder wants: one package containing both Server/ and Client/. Without
    # this it would fetch its own copy, because the two keep separate caches --
    # 1.7 GB and an hour thrown away, and one more chance for the share to
    # refuse. Mounting the cached file straight into the builder's drop folder
    # costs no disk and no copy; the builder picks up anything over 10 MB there.
    # Mount the whole cache DIRECTORY, not the file inside it. The archive is
    # published as "[40250] Reference Serverfile-....zip" -- the name has spaces
    # in it, and a path with spaces pasted into a command line comes apart at
    # them. The builder scans /archive for anything over 10 MB anyway, so it
    # never needs to be told the name. (This mount replaces the compose file's
    # ./client-archive at the same target, which is what we want here.)
    # If we were handed the unpacked server files, the client is already inside
    # them as Client/Client.zip -- about 1.2 GB that would otherwise come down
    # from MEGA a second time, on a share whose quota runs out regularly. The
    # builder scans its drop folder for a .zip/.rar/.7z and filters out the
    # ClientVS22.zip beside it (the C++ source) by name, so the folder is all
    # it needs.
    mkdir -p "$INSTALL_DIR/client-archive"
    _reuse=""
    _cachedir=""
    if [ -n "$M2_SRC_REFERENCE_DIR" ] && [ -d "$M2_SRC_REFERENCE_DIR/Client" ]; then
        _cachedir="$M2_SRC_REFERENCE_DIR/Client"
        _reuse=1
        info "the client comes from $_cachedir -- nothing to download"
    elif [ -d "$INSTALL_DIR/client-archive" ]; then
        _cachedir="$INSTALL_DIR/client-archive"
        if [ -n "$(find "$_cachedir" -maxdepth 1 -type f -size +10M \
                        ! -name '.megatmp.*' ! -name '*.part' ! -name '*.tmp' \
                        ! -name '*.meta' 2>/dev/null | head -1)" ]; then
            _reuse=1
        fi
    fi

    if [ -z "$_reuse" ] && [ -z "$(env_get "$INSTALL_DIR/.env" M2_CLIENT_ARCHIVE_URL 2>/dev/null || true)" ]; then
        warn "No native client archive was supplied."
        warn "The server is ready; use your existing compatible client, or put"
        warn "your archive in $INSTALL_DIR/client-archive and run:"
        warn "    cd $INSTALL_DIR && docker compose run --rm client-builder"
        CLIENT_STATE="unavailable"
        return 0
    fi

    say "Starting the client build in the background."
    if [ -n "$_reuse" ]; then
        say "It reuses the archive already downloaded for the server, so this"
        say "is a repack rather than another download -- but repacking a"
        say "gigabyte still takes a while, and longer on a slow disk."
    else
        say "It downloads over a gigabyte and then repacks it, so it takes a"
        say "while -- often 20 to 60 minutes depending on your connection."
    fi
    say ""
    say "You do not have to wait for it. The server is usable now; the"
    say "download link simply starts working when this finishes."

    : > "$CLIENT_LOG"
    # The builder bind-mounts ./client-archive so an operator can hand it a
    # file instead of downloading one. Docker would create that directory
    # itself, owned by root, which is a nuisance later -- so make it here.
    mkdir -p "$INSTALL_DIR/client-archive"

    # -T: no pseudo-terminal. Its output goes to a log file, not to a console.
    # nohup rather than setsid, because we want $! to be the process we are
    # about to watch: setsid forks, so $! would be a pid that exits at once and
    # every check below would think the build had finished.
    if [ "$_reuse" = 1 ]; then
        nohup sh -c "cd '$INSTALL_DIR' && docker compose run --rm -T \
                     -v '$_cachedir':/archive:ro client-builder" \
            >> "$CLIENT_LOG" 2>&1 < /dev/null &
    else
        nohup sh -c "cd '$INSTALL_DIR' && docker compose run --rm -T client-builder" \
            >> "$CLIENT_LOG" 2>&1 < /dev/null &
    fi
    _bpid=$!
    CLIENT_STATE="building"

    # Watch it for a minute and a half before saying anything about it.
    #
    # The naive version of this -- sleep a few seconds, grep the log for the
    # word "error" -- gets it wrong in both directions: it calls a slow but
    # healthy build broken because the image build printed a warning, and it
    # calls a build that died thirty seconds in "still running", so the
    # operator waits an hour for a link that was never coming. Watching the
    # process itself cannot be wrong about which of those happened.
    say ""
    printf '  watching it for a moment to be sure it really started'
    _deadline=$(( $(date +%s) + 90 ))
    while [ "$(date +%s)" -lt "$_deadline" ]; do
        if ! kill -0 "$_bpid" 2>/dev/null; then
            printf '\n'
            # It is over already. Which way?
            if client_zip_present; then
                good "The client is built and in place -- that was quick."
                CLIENT_STATE="ready"
            else
                warn "The client build stopped almost immediately, so there is"
                warn "no download for your players yet. The server itself is"
                warn "fine and everything else below still applies."
                warn ""
                warn "The last few lines of $CLIENT_LOG:"
                tail -12 "$CLIENT_LOG" 2>/dev/null | while read -r _l; do warn "    $_l"; done
                CLIENT_STATE="failed"
            fi
            return 0
        fi
        if client_zip_present; then
            printf '\n'
            good "The client is built and in place."
            CLIENT_STATE="ready"
            return 0
        fi
        printf '.'
        sleep 5
    done
    printf '\n'
    good "Still going after 90 seconds, which is what a real build looks like."
    info "Watch it with:  tail -f $CLIENT_LOG"
}

# =============================================================================
#  Step 10 -- the summary
#
#  Three things, and they have to be impossible to miss.
# =============================================================================

panel_url() {
    if [ -n "$DOMAIN" ]; then
        printf 'https://%s' "$DOMAIN"
    elif [ "$LOCAL_ONLY" = "1" ]; then
        printf 'http://127.0.0.1:%s' "$PANEL_PORT"
    else
        printf 'http://%s:%s' "$PUBLIC_ADDRESS" "$PANEL_PORT"
    fi
}

summary() {
    printf '\n'

    # ------------------------------------------------------------- the rest
    printf '  %sHow players connect%s\n' "$C_BOLD" "$C_RESET"
    printf '\n'
    if [ "$LOCAL_ONLY" = "1" ]; then
        printf '     Nobody else can. --local binds everything to 127.0.0.1, so this\n'
        printf '     server is reachable from this machine only.\n'
    else
        printf '     Server address : %s%s%s\n' "$C_BOLD" "$PUBLIC_ADDRESS" "$C_RESET"
        printf '     Login port     : %s\n' "$AUTH_PORT"
        printf '     Channel ports  : %s\n' "$GAME_PORTS"
        printf '\n'
    fi
    if [ "$BROWSER_PLAY" = "1" ]; then
        printf '     Playing in the browser is switched on, and this installer\n'
        printf '     fetched the browser client for you. If the panel does not\n'
        printf '     offer the button, run the installer again with --web-client.\n'
        printf '\n'
    fi
    printf '\n'
    printf '  %sDay to day%s\n' "$C_BOLD" "$C_RESET"
    printf '\n'
    printf '     cd %s\n' "$INSTALL_DIR"
    printf '     docker compose ps                 what is running\n'
    printf '     docker compose logs -f game       watch the game log\n'
    printf '     docker compose restart            restart everything\n'
    printf '     docker compose down               stop (keeps all player data)\n'
    printf '     docker compose up -d              start again\n'
    printf '\n'
    printf '     %sThe one dangerous command is "docker compose down -v".%s The -v\n' "$C_YELLOW" "$C_RESET"
    printf '     deletes every account, character and item, with no undo.\n'
    printf '\n'
    if [ "$LOCAL_ONLY" != "1" ]; then
        printf '  %sIf players cannot connect%s\n' "$C_BOLD" "$C_RESET"
        printf '\n'
        printf '     Almost always one of two things:\n'
        printf '     1. Your VPS provider blocks the ports in their own control\n'
        printf '        panel, outside this machine. Open TCP %s and %s there.\n' "$AUTH_PORT" "$GAME_PORTS"
        printf '     2. The address is wrong. It must be %s in\n' "$PUBLIC_ADDRESS"
        printf '        %s -- players log in fine and then hang\n' "$INSTALL_DIR/.env"
        printf '        on "connecting to the server" when it is not.\n'
        printf '\n'
    fi
    printf '  Back up your players with the recipe in %s/README.md\n' "$INSTALL_DIR"
    printf '\n'
    # The block people actually need is last, so it is what is on screen
    # when this finishes. Everything above it is read once.

    _url=$(panel_url)

    printf '\n\n'
    printf '  %s================================================================%s\n' "$C_GREEN$C_BOLD" "$C_RESET"
    printf '  %s  YOUR METIN2 SERVER IS INSTALLED%s\n' "$C_GREEN$C_BOLD" "$C_RESET"
    printf '  %s================================================================%s\n' "$C_GREEN$C_BOLD" "$C_RESET"
    printf '\n'
    printf '  Write these three things down now.\n'
    printf '\n'

    # ---------------------------------------------------------------- 1
    printf '  %s1. THE GAME CLIENT -- give this link to your players%s\n' "$C_BOLD" "$C_RESET"
    printf '\n'
    case "$CLIENT_STATE" in
        ready)
            printf '       %s%s/download%s\n' "$C_BOLD$C_CYAN" "$_url" "$C_RESET"
            printf '\n'
            printf '     Ready now. It is already set up to connect to your server.\n' ;;
        building)
            printf '       %s%s/download%s\n' "$C_BOLD$C_CYAN" "$_url" "$C_RESET"
            printf '\n'
            printf '     %sThis link does NOT work yet.%s The client is still being built\n' "$C_YELLOW$C_BOLD" "$C_RESET"
            printf '     in the background -- it is a download of over a gigabyte that\n'
            printf '     then has to be repacked, so give it 20 to 60 minutes.\n'
            printf '\n'
            printf '     Until it finishes the page politely says the download is not\n'
            printf '     ready. Nothing is broken. Check on it with:\n'
            printf '         tail -f %s\n' "$CLIENT_LOG" ;;
        failed)
            printf '       %s(not available -- the build failed)%s\n' "$C_YELLOW" "$C_RESET"
            printf '\n'
            printf '     The client build stopped with an error. The server itself is\n'
            printf '     fine. The log is at:\n'
            printf '         %s\n' "$CLIENT_LOG" ;;
        skipped)
            printf '       %s(not built -- you passed --no-client)%s\n' "$C_YELLOW" "$C_RESET" ;;
        *)
            printf '       %s(no client yet)%s\n' "$C_YELLOW" "$C_RESET"
            printf '\n'
            printf '     This release has no automatic client builder. Put your own\n'
            printf '     client.zip in place with:\n'
            printf '         cd %s\n' "$INSTALL_DIR"
            printf '         docker compose cp ./client.zip panel:/usr/local/m2panel/client.zip\n'
            printf '         docker compose restart panel\n' ;;
    esac
    printf '\n'
    rule
    printf '\n'

    # ---------------------------------------------------------------- 2
    printf '  %s2. YOUR ADMIN PANEL -- this is where you run the server%s\n' "$C_BOLD" "$C_RESET"
    printf '\n'
    printf '       %s%s%s\n' "$C_BOLD$C_CYAN" "$_url" "$C_RESET"
    printf '\n'
    if [ -n "$DOMAIN" ]; then
        printf '     Encrypted (HTTPS). The certificate renews itself.\n'
        printf '     The panel itself listens on %s:%s only, so it cannot be\n' "$PANEL_BIND" "$PANEL_PORT"
        printf '     reached except through nginx and its certificate.\n'
    elif [ "$LOCAL_ONLY" = "1" ]; then
        printf '     Only reachable from this machine.\n'
    else
        printf '     %sThis is plain HTTP -- it is NOT encrypted.%s Your password and\n' "$C_YELLOW$C_BOLD" "$C_RESET"
        printf '     everything you do in the panel travel across the internet in\n'
        printf '     the clear, and anyone between you and the server can read them.\n'
        printf '\n'
        printf '     To fix that, point a domain name at %s and run:\n' "$PUBLIC_ADDRESS"
        printf '         curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sh -s -- \\\n'
        printf '             --domain your.domain.com --email you@example.com\n'
        printf '     It will get a free certificate and switch the panel to HTTPS.\n'
    fi
    printf '\n'
    rule
    printf '\n'

    # ---------------------------------------------------------------- 3
    printf '  %s3. YOUR ADMIN PANEL PASSWORD%s\n' "$C_BOLD" "$C_RESET"
    printf '\n'
    if [ "$PANEL_PASSWORD_KNOWN" = "1" ] && [ -n "$PANEL_PASSWORD" ]; then
        printf '       %s%s%s\n' "$C_BOLD$C_CYAN" "$PANEL_PASSWORD" "$C_RESET"
        printf '\n'
        if [ "$PANEL_PASSWORD_CHOSEN" = "1" ]; then
            printf '     This is the one you chose in the admin panel. Updating\n'
            printf '     does not change it, and this line will keep showing it.\n'
        elif [ "$PANEL_PASSWORD_NEW" = "1" ]; then
            printf '     Generated on this machine just now, for this server only.\n'
            printf '     You can pick your own in the panel; it is offered right\n'
            printf '     under the introduction on the admin page.\n'
        else
            printf '     This is the password from when the server was first\n'
            printf '     installed here. It has not been changed. You can pick\n'
            printf '     your own in the panel, under the introduction.\n'
        fi
        printf '     It is also kept in %s\n' "$INSTALL_DIR/.env"
        printf '     (which only root can read) -- so you can look it up again.\n'
    else
        printf '       %s(unknown -- this server was installed before)%s\n' "$C_YELLOW" "$C_RESET"
        printf '\n'
        printf '     The panel keeps only a one-way hash of its password, so it\n'
        printf '     cannot be recovered. To set a new one:\n'
        printf '\n'
        printf '         cd %s\n' "$INSTALL_DIR"
        printf '         docker compose exec panel rm /usr/local/etc/m2panel.conf\n'
        printf '         docker compose restart panel\n'
        printf '         docker compose logs panel | grep -A4 "ADMIN PANEL PASSWORD"\n'
    fi
    printf '\n'
    printf '  %s================================================================%s\n' "$C_GREEN$C_BOLD" "$C_RESET"
}

# =============================================================================
#  Which clients this server offers
#
#  Two ways to play, and they cost very different amounts to install:
#
#      desktop   Reference_Client.zip, 1.29 GB, built into a download for the
#                panel's button. What every Metin2 server has always offered.
#      browser   Reference_WebClientEngine.zip (17.6 MB) + Reference_
#                WebClientData.zip (1.75 GB). No download, no install: a link.
#
#  A server that offers only one must not pay for the other -- choosing the
#  desktop client alone should not drag in 1.75 GB nobody will ever read. That
#  is the whole reason these are separate archives.
#
#  On an update nothing is asked when both are already there: this runs
#  unattended for some people and a question would hang it forever. When one is
#  missing it is offered, because that is the only moment the operator finds out
#  the other way exists.
# =============================================================================

# What is on the panel's volume right now. Asked of the panel container, because
# a Docker volume is a Docker object: reading it from the host means guessing at
# a storage driver, and that guess is wrong on rootless and on remote daemons.
#
# Sets HAVE_WEB / HAVE_DESKTOP. Never fails the install -- if the stack is not
# up yet, both are simply 0, which is the truth.
# Is there already a server here? fetch_stack works this out too, from the same
# file -- but the client question is asked BEFORE it now, and it needs the
# answer: on a first install there is nothing to detect and asking the panel
# would build its image just to be told so.
detect_existing_install() {
    [ -f "$INSTALL_DIR/docker-compose.yml" ] && FRESH_INSTALL=0
    return 0
}

detect_installed_clients() {
    HAVE_WEB=0
    HAVE_DESKTOP=0
    [ "$DRY_RUN" = "1" ] && return 0
    # A first install has nothing installed by definition, and asking anyway
    # would BUILD the panel image just to be told so -- minutes of work for an
    # answer we already have.
    [ "$FRESH_INSTALL" = "1" ] && return 0
    [ -f "$INSTALL_DIR/docker-compose.yml" ] || return 0

    _seen=$(dc run --rm --no-deps --entrypoint sh panel -c '
        [ -f /usr/local/m2panel/browser/current/index.html ] && echo web
        [ -f /usr/local/m2panel/client.zip ]                 && echo desktop
        exit 0' 2>/dev/null) || return 0

    case "$_seen" in *web*)     HAVE_WEB=1 ;; esac
    case "$_seen" in *desktop*) HAVE_DESKTOP=1 ;; esac
    return 0
}

choose_clients() {
    step "Native Windows client"

    # The upstream WebClient was withdrawn by its author. Keep accepting the
    # old flags so saved installer commands do not fail, but never download its
    # data or start the WebSocket bridge from this project.
    if [ "${WANT_WEB_FLAG:-}" = "1" ]; then
        warn "--web-client is no longer available and will be ignored."
        warn "Use a compatible native Windows r40250 client."
    fi
    WANT_WEB=0
    if [ "$SKIP_CLIENT" = "1" ]; then
        WANT_DESKTOP=0
        info "client preparation skipped (--no-client); the server will still be installed"
    else
        WANT_DESKTOP=1
        good "Native client mode selected."
        info "Client files must be supplied locally; they are not in this repository."
    fi
    return 0

    # An explicit flag always wins over a question, so an unattended run can say
    # exactly what it wants. --no-client already means "no desktop client".
    if [ -n "$WANT_WEB_FLAG" ] || [ "$SKIP_CLIENT" = "1" ]; then
        [ -n "$WANT_WEB_FLAG" ] && WANT_WEB="$WANT_WEB_FLAG"
        [ "$SKIP_CLIENT" = "1" ] && WANT_DESKTOP=0
        info "taken from the options you passed"
        return 0
    fi

    detect_installed_clients

    if [ "$FRESH_INSTALL" = "0" ]; then
        if [ "$HAVE_WEB" = "1" ] && [ "$HAVE_DESKTOP" = "1" ]; then
            WANT_WEB=1; WANT_DESKTOP=1
            good "Both clients are installed -- keeping both up to date."
            return 0
        fi
        WANT_WEB="$HAVE_WEB"
        WANT_DESKTOP="$HAVE_DESKTOP"
        if [ "$HAVE_WEB" = "0" ]; then
            # An install that predates the browser client falls in here. It keeps
            # working untouched; this is only an offer.
            #
            # The default follows what the operator already asked for: somebody
            # who set M2_BROWSER_PLAY=1 by hand wanted this and is missing only
            # the files, so saying "no" for them would be rude. Everyone else
            # gets "no" -- 1.8 GB is not something to talk anyone into.
            _prev=$(env_get "$INSTALL_DIR/.env" M2_BROWSER_PLAY 2>/dev/null || true)
            case "$_prev" in 1|true|yes|on) _def="y" ;; *) _def="n" ;; esac
            say "This server offers the desktop client only."
            say "The browser client lets players play from a link -- no download,"
            say "no install. It needs about 1.8 GB on this machine."
            [ "$_def" = "y" ] && \
                say "(M2_BROWSER_PLAY is already 1 here, so only the files are missing.)"
            ask_yes_no "Add the browser client?" "$_def" && WANT_WEB=1
        fi
        if [ "$HAVE_DESKTOP" = "0" ]; then
            say "This server offers the browser client only."
            say "The desktop client is the classic download behind the panel's button."
            ask_yes_no "Add the desktop client as well?" "n" && WANT_DESKTOP=1
        fi
        [ "$WANT_DESKTOP" = "0" ] && SKIP_CLIENT=1
        return 0
    fi

    # A first install: ask once, with the sizes, because that is what the
    # decision actually turns on.
    say ""
    say "  1  Browser only    players click a link and play        ~1.8 GB"
    say "  2  Desktop only    the classic download in the panel     ~1.3 GB"
    say "  3  Both                                                  ~3.1 GB"
    say ""
    while :; do
        _pick=$(ask_value "Which one" "3")
        case "$_pick" in
            1) WANT_WEB=1; WANT_DESKTOP=0; break ;;
            2) WANT_WEB=0; WANT_DESKTOP=1; break ;;
            3) WANT_WEB=1; WANT_DESKTOP=1; break ;;
            *) warn "Type 1, 2 or 3." ;;
        esac
    done
    [ "$WANT_DESKTOP" = "0" ] && SKIP_CLIENT=1
    return 0
}

# Put the browser client on the panel's volume. A task container: it runs,
# writes and exits. Only what is out of date is fetched -- an engine-only fix is
# 17.6 MB, which is why the two archives are separate in the first place.
fetch_web_client() {
    [ "$WANT_WEB" = "1" ] || return 0
    step "Fetching the browser client"

    if [ "$DRY_RUN" = "1" ]; then
        info "[dry-run] docker compose --profile webclient run --rm webclient-fetcher"
        return 0
    fi

    if ! grep -q 'webclient-fetcher' "$INSTALL_DIR/docker-compose.yml" 2>/dev/null; then
        warn "These server files have no webclient-fetcher service."
        warn "Update the repository to get the browser client."
        WANT_WEB=0
        return 0
    fi

    # The fetcher's script and pointer file have to sit beside docker-compose.yml,
    # because a bind mount can only name a path inside the directory compose was
    # given. fetch_stack() copies them there -- but it has several paths that
    # return before the copy, and an install that is already at the published
    # version takes one of them. So make sure here, every run: this is also what
    # repairs a server that updated while the copy was missing.
    # A checkout is not always around when this runs: the path that skips
    # restaging -- an install already at the published version -- never calls
    # locate_repo, so REPO_DIR is empty exactly on the servers that update
    # first. Downloading is therefore not a fallback but the normal case, and
    # the checkout is preferred only when there is one.
    # REFRESHED EVERY RUN, NOT PLACED ONCE.
    #
    # This used to skip a file that was already there, and that quietly made
    # browser-client updates impossible: artifacts.json IS the pointer that says
    # which engine version an install should have, so a stale copy reports that
    # the installed engine is current and the fetcher correctly does nothing.
    # MEASURED -- a server ran the 1.11.3 installer to completion, reported
    # success, and kept engine 1.11.0, because the pointer it consulted was the
    # one that shipped with 1.11.0.
    #
    # An existing copy is still the fallback when neither source can be reached,
    # so an update with no network to GitHub degrades to "keep what you have"
    # rather than to "lose the browser client".
    _raw="${M2_INSTALLER_URL%/installer/install.sh}"
    for _need in linux-port/fetch-web-client.sh artifacts.json; do
        _base=$(basename "$_need")

        if [ -n "$REPO_DIR" ] && [ -f "$REPO_DIR/$_need" ]; then
            cp "$REPO_DIR/$_need" "$INSTALL_DIR/$_base" && {
                info "placed $_base beside docker-compose.yml"
                continue
            }
        fi

        if curl -fsSL --max-time 60 "$_raw/$_need" -o "$INSTALL_DIR/$_base.part" 2>/dev/null &&
           [ -s "$INSTALL_DIR/$_base.part" ]; then
            mv "$INSTALL_DIR/$_base.part" "$INSTALL_DIR/$_base"
            info "placed $_base beside docker-compose.yml"
            continue
        fi
        rm -f "$INSTALL_DIR/$_base.part"

        if [ -f "$INSTALL_DIR/$_base" ]; then
            warn "$_base could not be refreshed from $_raw; using the copy already here."
            warn "A newer browser client, if one was published, will not be picked up."
            continue
        fi

        warn "$_base could not be obtained, from the checkout or from $_raw."
        warn "The browser client cannot be fetched."
        WANT_WEB=0
        return 0
    done

    # Its own exit codes say what went wrong, so this can be specific rather than
    # "it failed". The install is NOT aborted: a server whose game runs is worth
    # having even when the browser client could not be fetched.
    if dc --profile webclient run --rm webclient-fetcher; then
        # Trust the outcome, not the exit code. A run whose script was missing
        # once reported success here while nothing arrived, and the installer
        # then said "Browser client installed" to somebody who had none. Ask the
        # volume instead -- it is the thing the panel will look at.
        if dc run --rm --no-deps --entrypoint sh panel -c \
             '[ -f /usr/local/m2panel/browser/current/index.html ]' >/dev/null 2>&1; then
            good "Browser client installed."
            return 0
        fi
        warn "The fetcher reported success but no client arrived on the volume."
        warn "Look at:  cd $INSTALL_DIR && docker compose --profile webclient run --rm webclient-fetcher"
        WANT_WEB=0
        return 0
    fi
    _rc=$?
    case "$_rc" in
        3) warn "The fetcher is missing a tool it needs (unzip, curl, megatools)." ;;
        4) warn "A download did not finish. Check this machine's connection, then run the installer again." ;;
        5) warn "A downloaded archive had the wrong checksum -- damaged, or replaced." ;;
        7) warn "Not enough disk space for the browser client (~1.8 GB unpacked)." ;;
        8) warn "The MEGA link for the browser client's data has not been filled in yet." ;;
        *) warn "Fetching the browser client failed (exit $_rc)." ;;
    esac
    warn "The server itself is unaffected. Re-run the installer to try again."
    WANT_WEB=0
    return 0
}

# =============================================================================
#  main
# =============================================================================

main() {
    ui_init
    parse_args "$@"
    validate_ports
    trap cleanup EXIT INT TERM

    printf '\n'
    printf '  %sMetin2 server installer%s\n' "$C_BOLD" "$C_RESET"
    printf '  %sfor Linux servers%s\n' "$C_DIM" "$C_RESET"
    printf '\n'
    if [ "$DRY_RUN" = "1" ]; then
        printf '  %s** DRY RUN -- nothing on this machine will be changed **%s\n\n' "$C_YELLOW$C_BOLD" "$C_RESET"
    fi

    step "Checking this machine"
    check_root
    detect_os
    check_arch
    check_memory
    check_disk

    install_docker

    # ASKED BEFORE ANYTHING LARGE IS DOWNLOADED.
    #
    # The answer decides what gets fetched -- the browser client's 1.75 GB of
    # data, the desktop client's 1.29 GB, or neither -- so asking after the
    # server files have already come down means half an hour of waiting before
    # the one question the operator actually has to answer. It needs to know
    # only whether a server is already here, which is one file test.
    detect_existing_install
    choose_clients
    # Asked here for the same reason, and it must be before fetch_stack: almost
    # everything the answer turns on is patched into the server tree between
    # staging it and building the image from it, which happens in there.
    choose_custom_experience

    fetch_stack
    choose_address
    choose_domain
    # Before write_env, because the answer decides M2_BROWSER_PLAY -- the bridge
    # must not be turned on for a server that has no browser client to serve.
    write_env
    write_local_override
    start_stack
    wait_healthy
    open_firewall
    setup_tls
    # Before the bridge: the panel shows its Play button only once an index.html
    # is actually on the volume, and a bridge with nothing to serve is a port
    # open for nothing.
    fetch_web_client
    # After setup_tls, because with a domain the bridge is reached through the
    # nginx that step writes.
    start_browser_bridge
    start_client_build
    summary
}

main "$@"
