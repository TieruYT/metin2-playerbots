#!/bin/sh
# =============================================================================
#  vps-install.sh -- the 2.x (mt2009) server on a rented Linux VPS.
#
#  Run it as root (or through sudo) from the server folder you uploaded - the
#  one with VERSION and linux-port/ in it:
#
#      sudo sh linux-port/tools/vps-install.sh             # install (default)
#      sudo sh linux-port/tools/vps-install.sh status      # still building?
#      sudo sh linux-port/tools/vps-install.sh passwords   # admin/test logins
#      sudo sh linux-port/tools/vps-install.sh update      # the next version
#
#  The launcher's "Instaluj na VPS" uploads the folder over SSH and runs the
#  same commands; by hand they do exactly the same thing.
#
#  Players did this by hand from a community guide (Docker from docker.com,
#  the folder uploaded, .env from .env.example, compose up), and the guide
#  left two holes that are the reason this is a script and not a list:
#
#    * the panels (7788 classic, 7790 advanced, 7791 ItemShop) are published
#      on 0.0.0.0 by default, and on this line the classic panel asks no
#      passphrase - on a VPS, whoever found the port had the admin panel.
#      Here M2_PANEL_BIND_ADDRESS is 127.0.0.1 and the panels are opened
#      through an SSH tunnel (ssh -L 7788:127.0.0.1:7788 ...); the game's
#      own ports (11000, 13000-13002) stay on 0.0.0.0 for the players.
#    * the game ships the accounts admin/admin (a GM) and test/test. Once the
#      database answers both get random passwords, kept in
#      /root/metin2-accounts.txt (root only) and never written to a log.
#
#  The first build compiles the game core - 15 to 40 minutes. It runs
#  detached, its output in .vps-install.log in the server folder, so an SSH
#  connection that drops does not stop it; `status' says whether it is still
#  building, done or failed. By hand the log is followed on the terminal
#  (Ctrl+C stops the watching, not the build).
#
#  The game core is built as 32-bit x86, so an ARM VPS is refused. At ~1100
#  bots the server holds about 4.6 GB and the core's compile wants ~3 GB on
#  top: 8 GB of RAM is what to rent, 4 GB works with fewer bots and a swap
#  file (made here when there is none), and the disk wants 60-80 GB.
#
#  Idempotent: a second run keeps .env and its passwords, adds only the
#  security keys .env lacks, and starts nothing while a build is running.
# =============================================================================
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
SELF="$HERE/$(basename "$0")"
ROOT=${M2_VPS_ROOT:-$(cd "$HERE/../.." && pwd)}
COMPOSE_DIR="$ROOT/linux-port/docker"
ENV_FILE="$COMPOSE_DIR/.env"
EXAMPLE_FILE="$COMPOSE_DIR/.env.example"
LOG="$ROOT/.vps-install.log"
STATE="$ROOT/.vps-install.state"

# What is read about the machine and written outside the server folder. Only
# tests/vps_install_test.sh moves these.
MEMINFO=${M2_VPS_MEMINFO:-/proc/meminfo}
OS_RELEASE=${M2_VPS_OS_RELEASE:-/etc/os-release}
ACCOUNTS=${M2_VPS_ACCOUNTS:-/root/metin2-accounts.txt}
SWAPFILE=${M2_VPS_SWAPFILE:-/swapfile}
FSTAB=${M2_VPS_FSTAB:-/etc/fstab}
DB_WAIT=${M2_VPS_DB_WAIT:-1200}
POLL=${M2_VPS_POLL:-5}

# The sizes, in MB of what the kernel reports (a VPS sold as 8 GB shows about
# 7.7 GiB, one sold as 4 GB about 3.8).
MIN_MEM_MB=3500     # under this, with no swap and none that can be made: refused
WARN_MEM_MB=7400
MIN_DISK_MB=12000   # the images and the first build alone take about this
WARN_DISK_MB=40000
SWAP_SIZE_MB=4096

# Options (the dispatch at the bottom fills them).
ADDRESS=''
BOTS=''
FOLLOW=auto
NO_SWAP=0
FORCE=0
FROM=''
RAW=0

now() { date '+%Y-%m-%d %H:%M:%S'; }
say() { printf '%s\n' "$*"; }
warn() { printf 'UWAGA: %s\n' "$*"; }
die() { printf 'BLAD: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }
kv() { sed -n "s/^$2=//p" "$1" 2>/dev/null | head -n 1 | tr -d '\r'; }
is_number() { case "${1:-}" in ''|*[!0-9]*) return 1 ;; esac; return 0; }

installed_version() { tr -d ' \r\n' < "$ROOT/VERSION" 2>/dev/null || printf 'unknown'; }

# ---- the folder and the machine ----------------------------------------------
check_tree() {
    [ -f "$ROOT/VERSION" ] || die "nie ma pliku VERSION w $ROOT - uruchom to z folderu serwera (tego z linux-port/ w srodku)"
    [ -f "$COMPOSE_DIR/docker-compose.yml" ] || die "nie ma linux-port/docker/docker-compose.yml w $ROOT"
    [ -f "$EXAMPLE_FILE" ] || die "nie ma linux-port/docker/.env.example w $ROOT - wgrany folder jest niekompletny"
    if [ "$(tr -d ' \r\n' < "$COMPOSE_DIR/ENGINE" 2>/dev/null)" != mt2009 ]; then
        die "ten folder nie jest serwerem linii 2.x (linux-port/docker/ENGINE nie mowi mt2009) - ten skrypt jest dla paczki 2.x"
    fi
}

check_arch() {
    _arch=$(uname -m 2>/dev/null)
    case "$_arch" in
        x86_64|amd64) ;;
        *) die "procesor '$_arch': rdzen gry jest budowany jako 32-bitowy x86 i na ARM (ani na niczym innym niz x86_64) nie ruszy. Wynajmij VPS z procesorem Intel albo AMD (x86_64 / amd64)." ;;
    esac
    # A 32-bit system on a 64-bit processor: Docker is not built for it.
    if have dpkg; then
        _darch=$(dpkg --print-architecture 2>/dev/null)
        case "$_darch" in
            ''|amd64) ;;
            *) die "system ma pakiety dla '$_darch', a Docker i rdzen gry potrzebuja 64-bitowego systemu (amd64)" ;;
        esac
    fi
}

osr() { kv "$OS_RELEASE" "$1" | tr -d "\"'"; }
read_os() {
    OS_ID=$(osr ID); OS_VERSION=$(osr VERSION_ID); OS_NAME=$(osr PRETTY_NAME)
    OS_LIKE=$(osr ID_LIKE); OS_CODENAME=$(osr VERSION_CODENAME); OS_UBUNTU=$(osr UBUNTU_CODENAME)
}
os_supported() {
    case "$OS_ID:$OS_VERSION" in
        debian:12|debian:13|ubuntu:22.04|ubuntu:24.04) return 0 ;;
    esac
    return 1
}
os_is_apt() {
    case " $OS_ID $OS_LIKE " in
        *' debian '*|*' ubuntu '*) have apt-get ;;
        *) return 1 ;;
    esac
}

mem_kb() { awk -v k="$1:" '$1 == k { print $2; exit }' "$MEMINFO" 2>/dev/null; }
free_disk_mb() { df -Pk "$ROOT" 2>/dev/null | awk 'NR == 2 { print int($4 / 1024) }'; }
swap_mb_now() { _s=$(mem_kb SwapTotal); is_number "$_s" || _s=0; printf '%s' $((_s / 1024)); }

check_machine() {
    read_os
    if os_supported; then
        say "System: ${OS_NAME:-$OS_ID $OS_VERSION}"
    else
        warn "system '${OS_NAME:-nieznany}' nie byl testowany - sprawdzone sa Debian 12/13 i Ubuntu 22.04/24.04; probuje dalej"
    fi
    _mem=$(mem_kb MemTotal); is_number "$_mem" || _mem=0
    MEM_MB=$((_mem / 1024))
    SWAP_MB=$(swap_mb_now)
    DISK_MB=$(free_disk_mb); is_number "$DISK_MB" || DISK_MB=0
    say "Pamiec: $MEM_MB MB, plik wymiany: $SWAP_MB MB, wolne miejsce na dysku: $DISK_MB MB"
    if [ "$DISK_MB" -lt "$MIN_DISK_MB" ]; then
        [ "$FORCE" = 1 ] || die "na dysku jest wolne tylko $DISK_MB MB, a obrazy i pierwsza budowa potrzebuja co najmniej $MIN_DISK_MB MB (zalecane 60-80 GB). --force pomija ten test."
        warn "--force: za malo miejsca na dysku, ide dalej"
    elif [ "$DISK_MB" -lt "$WARN_DISK_MB" ]; then
        warn "wolne $DISK_MB MB to malo - zalecane 60-80 GB (obrazy, logi, kopie swiata)"
    fi
    WANT_SWAP=0
    if [ "$MEM_MB" -lt "$WARN_MEM_MB" ]; then
        warn "pamieci jest $MEM_MB MB - zalecane 8 GB; przy 4 GB graj z mniejsza liczba botow"
        [ "$SWAP_MB" -eq 0 ] && WANT_SWAP=1
    fi
    if have systemd-detect-virt; then
        case "$(systemd-detect-virt 2>/dev/null)" in
            openvz|lxc|lxc-libvirt) warn "VPS jest kontenerem ($(systemd-detect-virt)) - Docker moze w nim nie dzialac; najlepszy jest VPS KVM" ;;
        esac
    fi
}

# ---- packages and Docker ------------------------------------------------------
apt_get() { DEBIAN_FRONTEND=noninteractive apt-get -q "$@"; }

ensure_tools() {
    _missing=''
    have curl || _missing="$_missing curl"
    [ -f /etc/ssl/certs/ca-certificates.crt ] || _missing="$_missing ca-certificates"
    [ -n "$_missing" ] || return 0
    os_is_apt || die "brakuje programow:$_missing, a ten system nie ma apt - zainstaluj je sam"
    say "Instaluje:$_missing"
    { apt_get update && apt_get install -y $_missing; } || die "apt-get nie zainstalowal:$_missing"
}

docker_ready() { docker info >/dev/null 2>&1; }
compose_ready() { docker compose version >/dev/null 2>&1; }

docker_repo_distro() {
    case "$OS_ID" in
        debian|ubuntu) printf '%s' "$OS_ID"; return 0 ;;
    esac
    case " $OS_LIKE " in
        *' ubuntu '*) printf 'ubuntu' ;;
        *' debian '*) printf 'debian' ;;
    esac
}

# Docker's own repository, the way docs.docker.com installs it on Debian and
# Ubuntu. A repository somebody configured already (either file name Docker
# has used) is left alone: two entries with different keys stop apt at
# "Conflicting values set for option Signed-By".
add_docker_repo() {
    [ -f /etc/apt/sources.list.d/docker.list ] && return 0
    [ -f /etc/apt/sources.list.d/docker.sources ] && return 0
    _distro=$(docker_repo_distro)
    [ -n "$_distro" ] || die "nie wiem, dla jakiego systemu wziac Dockera (ID=$OS_ID) - zainstaluj Docker Engine sam: https://docs.docker.com/engine/install/"
    _suite=$OS_CODENAME
    [ "$_distro" = ubuntu ] && [ -n "$OS_UBUNTU" ] && _suite=$OS_UBUNTU
    [ -n "$_suite" ] || die "nie znam nazwy wydania systemu (VERSION_CODENAME w /etc/os-release)"
    apt_get update || die "apt-get update nie zadzialal"
    apt_get install -y ca-certificates curl || die "apt-get nie zainstalowal ca-certificates i curl"
    install -m 0755 -d /etc/apt/keyrings || die "nie moge utworzyc /etc/apt/keyrings"
    curl -fsSL "https://download.docker.com/linux/$_distro/gpg" -o /etc/apt/keyrings/docker.asc ||
        die "nie udalo sie pobrac klucza repozytorium Dockera (download.docker.com)"
    chmod a+r /etc/apt/keyrings/docker.asc
    printf 'deb [arch=%s signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/%s %s stable\n' \
        "$(dpkg --print-architecture)" "$_distro" "$_suite" > /etc/apt/sources.list.d/docker.list ||
        die "nie moge zapisac /etc/apt/sources.list.d/docker.list"
    apt_get update || die "apt-get update (z repozytorium Dockera) nie zadzialal"
}

start_docker() {
    docker_ready && return 0
    if have systemctl; then
        systemctl enable --now docker >/dev/null 2>&1 || true
    elif have service; then
        service docker start >/dev/null 2>&1 || true
    fi
    _i=0
    while [ "$_i" -lt 30 ]; do
        docker_ready && return 0
        sleep 1
        _i=$((_i + 1))
    done
    return 1
}

ensure_docker() {
    if have docker && compose_ready; then
        say "Docker jest: $(docker --version 2>/dev/null)"
    elif have docker; then
        # A distribution's docker.io without compose: the plugin from Docker's
        # repository, or Ubuntu's own compose v2 package.
        os_is_apt || die "Docker jest, ale bez 'docker compose' - doinstaluj wtyczke compose: https://docs.docker.com/compose/install/linux/"
        say "Docker jest, ale bez 'docker compose' - doinstalowuje wtyczke."
        add_docker_repo
        apt_get install -y docker-compose-plugin docker-buildx-plugin || apt_get install -y docker-compose-v2 ||
            die "nie udalo sie doinstalowac 'docker compose'"
    else
        os_is_apt || die "nie ma Dockera, a ten system nie ma apt - zainstaluj Docker Engine sam: https://docs.docker.com/engine/install/"
        say "Instaluje Dockera z oficjalnego repozytorium (download.docker.com)..."
        add_docker_repo
        apt_get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin ||
            die "apt-get nie zainstalowal Dockera"
    fi
    start_docker || die "Docker jest zainstalowany, ale nie odpowiada (docker info) - sprawdz: systemctl status docker"
    compose_ready || die "brakuje 'docker compose'"
}

# A swap file where memory is short: the game core's compile wants ~3 GB of
# its own, and a 4 GB VPS without swap is where the build gets killed.
ensure_swap() {
    SWAP_MADE=0
    [ "$WANT_SWAP" = 1 ] || return 0
    if [ "$NO_SWAP" = 1 ]; then
        warn "--no-swap: nie zakladam pliku wymiany"
        return 0
    fi
    if [ -e "$SWAPFILE" ]; then
        # One made before and never switched on (or switched off since).
        if swapon "$SWAPFILE" 2>/dev/null; then
            say "Wlaczylem istniejacy plik wymiany $SWAPFILE."
            SWAP_MADE=1
        else
            warn "$SWAPFILE juz jest, ale nie daje sie go wlaczyc - nie ruszam go"
        fi
        return 0
    fi
    if [ "$DISK_MB" -lt $((SWAP_SIZE_MB + 4096)) ]; then
        warn "za malo miejsca na dysku na plik wymiany $SWAP_SIZE_MB MB"
        return 0
    fi
    say "Zakladam plik wymiany $SWAP_SIZE_MB MB ($SWAPFILE) - pomaga kompilacji rdzenia gry przy malej pamieci."
    for _how in fallocate dd; do
        rm -f "$SWAPFILE"
        if [ "$_how" = fallocate ]; then
            ( umask 077; fallocate -l "${SWAP_SIZE_MB}M" "$SWAPFILE" ) 2>/dev/null || continue
        else
            ( umask 077; dd if=/dev/zero of="$SWAPFILE" bs=1M count="$SWAP_SIZE_MB" ) 2>/dev/null || continue
        fi
        chmod 600 "$SWAPFILE"
        if mkswap "$SWAPFILE" >/dev/null 2>&1 && swapon "$SWAPFILE" 2>/dev/null; then
            grep -q "^$SWAPFILE[[:space:]]" "$FSTAB" 2>/dev/null ||
                printf '%s none swap sw 0 0\n' "$SWAPFILE" >> "$FSTAB"
            say "Plik wymiany wlaczony i dopisany do $FSTAB (zostanie po restarcie VPS)."
            SWAP_MADE=1
            return 0
        fi
    done
    rm -f "$SWAPFILE"
    warn "system nie pozwolil wlaczyc pliku wymiany (tak bywa na VPS typu OpenVZ/LXC)"
}

refuse_small_memory() {
    [ "$MEM_MB" -lt "$MIN_MEM_MB" ] || return 0
    [ "$SWAP_MB" -gt 0 ] && return 0
    [ "$SWAP_MADE" = 1 ] && return 0
    [ "$FORCE" = 1 ] && { warn "--force: $MEM_MB MB pamieci bez wymiany, ide dalej"; return 0; }
    die "pamieci jest $MEM_MB MB, bez pliku wymiany i bez mozliwosci jego zalozenia - kompilacja rdzenia gry tego nie przezyje. Wynajmij co najmniej 4 GB (zalecane 8 GB)."
}

# ---- .env -------------------------------------------------------------------
env_get() { kv "$ENV_FILE" "$1"; }
env_has() { grep -q "^$1=" "$ENV_FILE" 2>/dev/null; }
env_set() {
    _k=$1; _v=$2
    if env_has "$_k"; then
        _tmp="$ENV_FILE.vps-install.$$"
        ( umask 077
          awk -v k="$_k" -v v="$_v" 'BEGIN { p = k "=" } !done && index($0, p) == 1 { print p v; done = 1; next } { print }' \
              "$ENV_FILE" > "$_tmp" ) || { rm -f "$_tmp"; die "nie moge zmienic $_k w .env"; }
        cat "$_tmp" > "$ENV_FILE"
        rm -f "$_tmp"
    else
        [ -s "$ENV_FILE" ] && [ -n "$(tail -c 1 "$ENV_FILE")" ] && printf '\n' >> "$ENV_FILE"
        printf '%s=%s\n' "$_k" "$_v" >> "$ENV_FILE"
    fi
}

# 48 hex digits, what `openssl rand -hex 24' gives, from the kernel alone.
random_hex() { od -An -tx1 -N24 /dev/urandom | tr -d ' \n'; }
# A game password: twelve letters and digits, none of the ones people misread.
random_password() {
    LC_ALL=C tr -dc 'abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789' < /dev/urandom 2>/dev/null | head -c 12
}
random_social() {
    _n=$(od -An -tu4 -N4 /dev/urandom | tr -d ' \n')
    printf '%07d' $((_n % 10000000))
}

is_ipv4() {
    printf '%s' "${1:-}" | grep -Eq '^([0-9]{1,3}\.){3}[0-9]{1,3}$' || return 1
    for _o in $(printf '%s' "$1" | tr '.' ' '); do
        [ "$_o" -le 255 ] || return 1
    done
    return 0
}
is_host_name() {
    printf '%s' "${1:-}" | grep -Eq '^[A-Za-z0-9]([A-Za-z0-9.-]{0,251}[A-Za-z0-9])?$'
}

public_ipv4() {
    for _url in https://ifconfig.me https://api.ipify.org https://ipv4.icanhazip.com; do
        _ip=$(curl -4 -fsS --max-time 8 "$_url" 2>/dev/null | tr -d ' \r\n')
        if is_ipv4 "$_ip"; then
            printf '%s' "$_ip"
            return 0
        fi
    done
    return 1
}

bots_for_memory() {
    if [ "$1" -lt 6000 ]; then printf '150'
    elif [ "$1" -lt 14000 ]; then printf '400'
    else printf '800'
    fi
}

host_timezone() {
    _zone=''
    if have timedatectl; then _zone=$(timedatectl show -p Timezone --value 2>/dev/null); fi
    if [ -z "$_zone" ] && [ -f /etc/timezone ]; then _zone=$(head -n 1 /etc/timezone | tr -d ' \r'); fi
    if [ -z "$_zone" ] && [ -L /etc/localtime ]; then _zone=$(readlink /etc/localtime | sed 's|.*zoneinfo/||'); fi
    case "$_zone" in
        */*|UTC) printf '%s' "$_zone" ;;
    esac
}

db_volume_exists() {
    _project=$(env_get M2_COMPOSE_PROJECT_NAME)
    [ -n "$_project" ] || _project=metin2
    docker volume inspect "${_project}_db-data" >/dev/null 2>&1
}

resolve_address() {
    if [ -n "$ADDRESS" ]; then
        is_ipv4 "$ADDRESS" || is_host_name "$ADDRESS" || die "--address '$ADDRESS' nie jest adresem IPv4 ani nazwa domeny"
        return 0
    fi
    ADDRESS=$(public_ipv4) ||
        die "nie udalo sie odczytac publicznego adresu IPv4 tego VPS (curl -4 ifconfig.me) - podaj go: --address 1.2.3.4"
}

write_new_env() {
    ( umask 077; cp "$EXAMPLE_FILE" "$ENV_FILE" ) || die "nie moge utworzyc $ENV_FILE"
    chmod 600 "$ENV_FILE"
    env_set M2_PUBLIC_ADDRESS "$ADDRESS"
    _root_pw=$(random_hex); _pw=$(random_hex)
    [ ${#_root_pw} -eq 48 ] && [ ${#_pw} -eq 48 ] || die "nie udalo sie wylosowac hasel bazy (/dev/urandom)"
    env_set M2_DB_ROOT_PASSWORD "$_root_pw"
    env_set M2_DB_PASSWORD "$_pw"
    env_set M2_PANEL_BIND_ADDRESS 127.0.0.1
    env_set M2_HOST_BIND_ADDRESS 0.0.0.0
    env_set PLAYERBOT_AUTOSPAWN_COUNT "$BOTS"
    # The panel's updater (a compose profile) mounts the folder at the same
    # path it has here; the example says /opt/metin2.
    env_set M2_UPDATE_STACK_DIR "$ROOT"
    _zone=$(host_timezone)
    if [ -n "$_zone" ]; then
        env_set M2_TZ "$_zone"
        env_set M2_TZ_DEFAULTED 1
    fi
    say "Zapisalem linux-port/docker/.env: adres $ADDRESS, losowe hasla bazy, $BOTS botow, panele tylko na 127.0.0.1 (przez tunel SSH), porty gry na 0.0.0.0."
}

# An .env that is there already is somebody's - its passwords above all, which
# the database was created with. Only a security key it lacks is added.
secure_existing_env() {
    chmod 600 "$ENV_FILE" 2>/dev/null
    if [ -z "$(env_get M2_PUBLIC_ADDRESS)" ]; then
        env_set M2_PUBLIC_ADDRESS "$ADDRESS"
        say "M2_PUBLIC_ADDRESS bylo puste - wpisalem $ADDRESS."
    fi
    for _key in M2_DB_ROOT_PASSWORD M2_DB_PASSWORD; do
        [ -n "$(env_get "$_key")" ] && continue
        if db_volume_exists; then
            die "$_key w .env jest puste, a baza juz istnieje - wpisz haslo, z ktorym zostala zalozona (nowe odcieloby serwer od jego bazy)"
        fi
        env_set "$_key" "$(random_hex)"
        say "$_key bylo puste (a bazy jeszcze nie ma) - wylosowalem je."
    done
    # A missing or empty panel address falls back to M2_HOST_BIND_ADDRESS in
    # compose, which is 0.0.0.0 - the hole this script exists for.
    _panel=$(env_get M2_PANEL_BIND_ADDRESS)
    if [ -z "$_panel" ]; then
        env_set M2_PANEL_BIND_ADDRESS 127.0.0.1
        say "Panele tylko na 127.0.0.1 (M2_PANEL_BIND_ADDRESS) - otwierasz je przez tunel SSH."
    elif [ "$_panel" != 127.0.0.1 ]; then
        warn "M2_PANEL_BIND_ADDRESS=$_panel - panele sa wystawione na swiat, a panel klasyczny nie pyta o haslo. Zostawiam, bo to wpis z .env; bezpiecznie jest 127.0.0.1 i tunel SSH."
    fi
    _host=$(env_get M2_HOST_BIND_ADDRESS)
    if [ -z "$_host" ]; then
        env_set M2_HOST_BIND_ADDRESS 0.0.0.0
    elif [ "$_host" = 127.0.0.1 ]; then
        warn "M2_HOST_BIND_ADDRESS=127.0.0.1 - gracze z internetu nie wejda do gry; na VPS ma byc 0.0.0.0"
    fi
    say "linux-port/docker/.env jest - hasla bazy zostaja takie, jakie sa."
}

prepare_env() {
    if [ -f "$ENV_FILE" ]; then
        secure_existing_env
    else
        write_new_env
    fi
}

# What a Windows install gets from the launcher and a Linux one did not:
# the panel's build context staged from files/ (update.sh stage), and the
# one build input that is an empty directory.
stage_context() {
    [ -f "$ROOT/linux-port/tools/update.sh" ] || die "nie ma linux-port/tools/update.sh - wgrany folder jest niekompletny"
    sh "$ROOT/linux-port/tools/update.sh" stage || die "nie udalo sie przygotowac kontekstu budowy panelu (update.sh stage)"
    mkdir -p "$COMPOSE_DIR/game/src/serverfiles/share/package" 2>/dev/null || true
}

# ---- the background job --------------------------------------------------------
J_STATE=''; J_KIND=''; J_PHASE=''; J_PID=''; J_STARTED=''; J_FINISHED=''; J_EXIT=''; J_MESSAGE=''

state_write() {
    _tmp="$STATE.tmp.$$"
    { printf 'state=%s\n' "$J_STATE"
      printf 'kind=%s\n' "$J_KIND"
      printf 'phase=%s\n' "$J_PHASE"
      printf 'pid=%s\n' "$J_PID"
      printf 'started=%s\n' "$J_STARTED"
      printf 'finished=%s\n' "$J_FINISHED"
      printf 'exit=%s\n' "$J_EXIT"
      printf 'message=%s\n' "$J_MESSAGE"
    } > "$_tmp" && mv -f "$_tmp" "$STATE"
}

job_alive() {
    _pid=$(kv "$STATE" pid)
    is_number "$_pid" || return 1
    kill -0 "$_pid" 2>/dev/null || return 1
    # A pid written before a reboot can belong to anything by now.
    [ -r "/proc/$_pid/cmdline" ] || return 0
    tr '\000' ' ' < "/proc/$_pid/cmdline" | grep -q 'vps-install'
}

# none | running | done | failed | interrupted (a job that died unfinished:
# the VPS rebooted, or somebody killed it).
job_state() {
    [ -f "$STATE" ] || { printf 'none'; return 0; }
    _s=$(kv "$STATE" state)
    if [ "$_s" = running ]; then
        if job_alive; then printf 'running'; else printf 'interrupted'; fi
    else
        printf '%s' "${_s:-none}"
    fi
}

log_lines() { if [ -f "$LOG" ]; then wc -l < "$LOG" | tr -d ' '; else printf '0'; fi; }

start_job() {
    if [ "$(job_state)" = running ]; then
        say "Zadanie '$(kv "$STATE" kind)' juz trwa (pid $(kv "$STATE" pid)) - nie zaczynam drugiego."
        return 0
    fi
    # The log is kept across runs; a year of updates is not.
    if [ -f "$LOG" ] && [ "$(log_lines)" -gt 20000 ]; then
        tail -n 5000 "$LOG" > "$LOG.tmp" && mv -f "$LOG.tmp" "$LOG"
    fi
    printf '\n==== %s  %s (wersja w folderze: %s) ====\n' "$(now)" "$1" "$(installed_version)" >> "$LOG"
    J_STATE=running; J_KIND=$1; J_PHASE=start; J_PID=''; J_STARTED=$(now); J_FINISHED=''; J_EXIT=''; J_MESSAGE=''
    state_write
    # Out of this session entirely (setsid), so the SSH connection that
    # started it can drop without taking it along; nohup where there is no
    # setsid. The job writes its own pid.
    if have setsid; then
        setsid sh "$SELF" _job "$1" < /dev/null >> "$LOG" 2>&1 &
    else
        nohup sh "$SELF" _job "$1" < /dev/null >> "$LOG" 2>&1 &
    fi
    _i=0
    while [ "$_i" -lt 30 ]; do
        [ -n "$(kv "$STATE" pid)" ] && break
        sleep 1
        _i=$((_i + 1))
    done
    [ -n "$(kv "$STATE" pid)" ] || die "zadanie w tle nie wystartowalo - zobacz $LOG"
    say "Zadanie '$1' dziala w tle (pid $(kv "$STATE" pid)), log: $LOG"
}

job_phase() {
    J_PHASE=$1
    state_write
    printf '[%s] etap: %s\n' "$(now)" "$2"
}

job_end() {
    J_STATE=$1; J_EXIT=$2; J_MESSAGE=$3; J_FINISHED=$(now)
    state_write
    printf '[%s] koniec: %s (kod %s) - %s\n' "$(now)" "$1" "$2" "$3"
}

run_job() {
    J_KIND=$1; J_STATE=running; J_PHASE=start; J_PID=$$
    J_STARTED=$(kv "$STATE" started); [ -n "$J_STARTED" ] || J_STARTED=$(now)
    J_FINISHED=''; J_EXIT=''; J_MESSAGE=''
    state_write
    trap '' HUP
    trap 'job_end failed 143 "przerwane"; exit 143' INT TERM
    case "$J_KIND" in
        install)
            job_phase build "budowa obrazow i start serwera (docker compose up -d --build) - pierwszy raz 15-40 minut"
            ( cd "$COMPOSE_DIR" && docker compose up -d --build )
            _rc=$?
            if [ "$_rc" -ne 0 ]; then
                job_end failed "$_rc" "docker compose up -d --build zakonczyl sie bledem - przyczyna jest wyzej w logu"
                return "$_rc"
            fi
            ;;
        update)
            job_phase update "aktualizacja z paczki na GitHubie (linux-port/tools/update.sh run)"
            sh "$ROOT/linux-port/tools/update.sh" run
            _rc=$?
            if [ "$_rc" -ne 0 ]; then
                job_end failed "$_rc" "aktualizacja nie powiodla sie - przyczyna jest wyzej w logu"
                return "$_rc"
            fi
            ;;
        *)
            job_end failed 2 "nieznane zadanie '$J_KIND'"
            return 2
            ;;
    esac
    job_phase accounts "hasla kont admin i test"
    if ! secure_game_accounts; then
        job_end failed 3 "serwer wstal, ale hasla kont admin/test nie zostaly zmienione - uruchom: sh linux-port/tools/vps-install.sh passwords"
        return 3
    fi
    job_end done 0 "serwer dziala"
    return 0
}

# Watching the job from a terminal: new lines of the log as they come, until
# the job ends. Ctrl+C ends the watching; the job is in its own session.
follow_job() {
    _from=${1:-0}
    say "Podglad logu (Ctrl+C konczy podglad, nie budowe):"
    while :; do
        _n=$(log_lines)
        if [ "$_n" -gt "$_from" ]; then
            tail -n "+$((_from + 1))" "$LOG" | head -n "$((_n - _from))"
            _from=$_n
        fi
        [ "$(job_state)" = running ] || break
        sleep 2
    done
    _n=$(log_lines)
    [ "$_n" -gt "$_from" ] && tail -n "+$((_from + 1))" "$LOG" | head -n "$((_n - _from))"
    return 0
}

# ---- the database: the shipped passwords -------------------------------------
# SQL on stdin, as the database's root, in the running mariadb container. The
# password is the container's own MARIADB_ROOT_PASSWORD, read inside it, so no
# secret is ever an argument anybody can see in `ps'.
db_sql() {
    ( cd "$COMPOSE_DIR" && docker compose exec -T mariadb sh -c 'MYSQL_PWD="$MARIADB_ROOT_PASSWORD" exec mariadb -uroot -N -B' )
}

db_answers() {
    _n=$(printf "SELECT COUNT(*) FROM account.account WHERE login IN ('admin','test');\n" | db_sql 2>/dev/null | tr -d ' \r\n')
    is_number "$_n"
}

db_wait() {
    _deadline=$(( $(date +%s) + ${1:-$DB_WAIT} ))
    while :; do
        db_answers && return 0
        [ "$(date +%s)" -ge "$_deadline" ] && return 1
        sleep "$POLL"
    done
}

# The engine's own hash (mysql5_password): '*' and SHA1 of the raw SHA1.
hash_sql() { printf "CONCAT('*', UPPER(SHA1(UNHEX(SHA1('%s')))))" "$1"; }

shipped_logins() {
    printf "SELECT login FROM account.account WHERE (login='admin' AND password=%s) OR (login='test' AND password=%s);\n" \
        "$(hash_sql admin)" "$(hash_sql test)" | db_sql
}

# /root/metin2-accounts.txt: one account a line, login, password, a note.
accounts_set() {
    ( umask 077
      _tmp="$ACCOUNTS.tmp.$$"
      { if [ -f "$ACCOUNTS" ]; then
            awk -v l="$1" '$1 != l' "$ACCOUNTS"
        else
            printf '# Metin2 SinglePlayer - konta gry na tym serwerze: login, haslo, opis.\n'
            printf '# Tylko root moze czytac ten plik. Nie wklejaj go nigdzie.\n'
        fi
        printf '%s %s %s\n' "$1" "$2" "$3"
      } > "$_tmp" && chmod 600 "$_tmp" && mv -f "$_tmp" "$ACCOUNTS" )
}

# The shipped admin/test passwords replaced by random ones. The file is
# written before the database, so a password in effect is always one the file
# holds; a failure between the two leaves the shipped one, which the next run
# finds and replaces again. Nothing here prints a password.
secure_game_accounts() {
    db_wait "${1:-$DB_WAIT}" || { say "Baza nie odpowiada - hasla zmienie przy nastepnym uruchomieniu."; return 1; }
    _logins=$(shipped_logins) || return 1
    _changed=0
    for _login in $_logins; do
        case "$_login" in admin|test) ;; *) continue ;; esac
        _pw=$(random_password)
        [ ${#_pw} -eq 12 ] || return 1
        _note='konto testowe'
        [ "$_login" = admin ] && _note='konto GM (postacie GM w grze)'
        accounts_set "$_login" "$_pw" "$_note" || return 1
        printf "UPDATE account.account SET password=%s WHERE login='%s';\n" "$(hash_sql "$_pw")" "$_login" | db_sql > /dev/null || return 1
        say "Konto $_login: haslo z paczki zmienione na losowe (zapisane w $ACCOUNTS)."
        _changed=$((_changed + 1))
    done
    [ "$_changed" -eq 0 ] && say "Konta admin i test nie maja juz hasel z paczki."
    return 0
}

print_accounts() {
    # The note to stderr: the launcher reads --raw's stdout line by line as
    # "login password note", and "Nie ma jeszcze ..." would be an account.
    [ -f "$ACCOUNTS" ] || { say "Nie ma jeszcze $ACCOUNTS - hasla zmieniam, gdy baza wstanie." >&2; return 0; }
    if [ "$RAW" = 1 ]; then
        grep -v '^#' "$ACCOUNTS" | grep -v '^[[:space:]]*$' || true
        return 0
    fi
    say "Konta gry na tym serwerze ($ACCOUNTS):"
    awk '!/^#/ && NF >= 2 {
            note = ""
            for (i = 3; i <= NF; i++) note = note (i > 3 ? " " : "") $i
            printf "  login: %-14s haslo: %s%s\n", $1, $2, (note != "" ? "   (" note ")" : "")
        }' "$ACCOUNTS"
}

# ---- commands --------------------------------------------------------------------
cmd_install() {
    check_tree
    check_arch
    check_machine
    # The swap and the refusal before Docker: a machine that is going to be
    # refused should not get Docker installed first.
    ensure_swap
    refuse_small_memory
    ensure_tools
    ensure_docker
    resolve_address
    [ -n "$BOTS" ] || BOTS=$(bots_for_memory "$MEM_MB")
    prepare_env
    stage_context
    _from=$(log_lines)
    start_job install
    say ""
    say "Gracze lacza sie z adresem $(env_get M2_PUBLIC_ADDRESS), porty 11000 i $(env_get M2_GAME_PORT_RANGE) (TCP) - jesli dostawca VPS ma wlasna zapore, otworz je tam."
    say "Panele (7788, 7790, 7791) sluchaja tylko na tym serwerze: otwiera je launcher (tunel SSH) albo:"
    say "    ssh -L 7788:127.0.0.1:7788 -L 7790:127.0.0.1:7790 uzytkownik@$(env_get M2_PUBLIC_ADDRESS)"
    say "Stan budowy:  sh $SELF status"
    say "Hasla kont:   sh $SELF passwords"
    if [ "$FOLLOW" = yes ] || { [ "$FOLLOW" = auto ] && [ -t 1 ]; }; then
        follow_job "$_from"
        say ""
        say "Stan: $(job_state) - $(kv "$STATE" message)"
        [ "$(job_state)" = done ] && print_accounts
    fi
    return 0
}

cmd_update() {
    check_tree
    have docker || die "nie ma Dockera - najpierw: sh $SELF install"
    [ -f "$ENV_FILE" ] || die "nie ma linux-port/docker/.env - najpierw: sh $SELF install"
    _from=$(log_lines)
    start_job update
    if [ "$FOLLOW" = yes ] || { [ "$FOLLOW" = auto ] && [ -t 1 ]; }; then
        follow_job "$_from"
        say "Stan: $(job_state) - $(kv "$STATE" message)"
    fi
    return 0
}

# Machine-readable first (key=value until the first "---" line, which is what
# the launcher reads), then the log and the containers for a person.
cmd_status() {
    _s=$(job_state)
    _lines=$(log_lines)
    printf 'state=%s\n' "$_s"
    for _k in kind phase pid started finished exit message; do
        printf '%s=%s\n' "$_k" "$(kv "$STATE" "$_k")"
    done
    printf 'log_lines=%s\n' "$_lines"
    printf 'version=%s\n' "$(installed_version)"
    if [ -f "$ENV_FILE" ]; then
        printf 'public_address=%s\n' "$(env_get M2_PUBLIC_ADDRESS)"
        printf 'auth_port=%s\n' "$(env_get M2_AUTH_PORT)"
        printf 'game_port_range=%s\n' "$(env_get M2_GAME_PORT_RANGE)"
        printf 'ch2=%s\n' "$(env_get M2_PLAYERBOT_CH2)"
        printf 'panel_port=%s\n' "$(env_get M2_PANEL_PUBLIC_PORT)"
        printf 'seban_panel_port=%s\n' "$(env_get M2_SEBAN_PANEL_PORT)"
        printf 'itemshop_port=%s\n' "$(env_get M2_ITEMSHOP_PUBLIC_PORT)"
        printf 'panel_bind=%s\n' "$(env_get M2_PANEL_BIND_ADDRESS)"
        printf 'host_bind=%s\n' "$(env_get M2_HOST_BIND_ADDRESS)"
        printf 'bots=%s\n' "$(env_get PLAYERBOT_AUTOSPAWN_COUNT)"
    fi
    printf 'accounts_file=%s\n' "$([ -f "$ACCOUNTS" ] && echo 1 || echo 0)"
    printf -- '--- log ---\n'
    if [ -f "$LOG" ]; then
        if is_number "$FROM"; then
            [ "$_lines" -gt "$FROM" ] && tail -n "+$((FROM + 1))" "$LOG" | tail -n 400
        else
            tail -n 40 "$LOG"
        fi
    fi
    printf -- '--- docker compose ps ---\n'
    if have docker && [ -f "$ENV_FILE" ]; then
        ( cd "$COMPOSE_DIR" && docker compose ps ) 2>&1 | tail -n 30
    fi
    return 0
}

cmd_passwords() {
    check_tree
    # Not while a job runs: it is changing the same two passwords.
    if [ "$(job_state)" != running ] && have docker && [ -f "$ENV_FILE" ] && db_answers; then
        secure_game_accounts 5 > /dev/null || warn "nie udalo sie sprawdzic hasel w bazie" >&2
    fi
    print_accounts
}

cmd_add_account() {
    check_tree
    _base=$(printf '%s' "${1:-}" | tr 'A-Z' 'a-z' | tr -cd 'a-z0-9' | cut -c1-12)
    [ ${#_base} -ge 2 ] || die "login: co najmniej dwie litery lub cyfry (a-z, 0-9)"
    _note=$(printf '%s' "${2:-znajomy}" | tr -cd 'A-Za-z0-9 ._:-' | cut -c1-40)
    [ -n "$_note" ] || _note=znajomy
    db_answers || die "baza nie odpowiada - serwer musi dzialac"
    _login=$_base
    _n=1
    while :; do
        _count=$(printf "SELECT COUNT(*) FROM account.account WHERE login='%s';\n" "$_login" | db_sql | tr -d ' \r\n')
        is_number "$_count" || die "baza nie odpowiedziala na pytanie o login"
        [ "$_count" -eq 0 ] && break
        _n=$((_n + 1))
        [ "$_n" -gt 99 ] && die "wszystkie loginy $_base..${_base}99 sa zajete"
        _login="$_base$_n"
    done
    _pw=$(random_password)
    [ ${#_pw} -eq 12 ] || die "nie udalo sie wylosowac hasla (/dev/urandom)"
    _social=$(random_social)
    accounts_set "$_login" "$_pw" "$_note" || die "nie moge zapisac $ACCOUNTS"
    printf "INSERT INTO account.account (login, password, social_id, status) VALUES ('%s', %s, '%s', 'OK');\n" \
        "$_login" "$(hash_sql "$_pw")" "$_social" | db_sql > /dev/null || die "baza nie przyjela nowego konta"
    printf 'login=%s\npassword=%s\nsocial_id=%s\n' "$_login" "$_pw" "$_social"
}

cmd_sql() {
    check_tree
    db_sql
}

# The logs a report needs, with anything that looks like a password masked.
# The panels are left out: the classic one prints the passphrase it invents
# on its first start, on a line of its own.
cmd_logs() {
    check_tree
    _tail=${1:-200}
    is_number "$_tail" || _tail=200
    printf -- '--- %s (ostatnie 80 linii) ---\n' "$LOG"
    [ -f "$LOG" ] && tail -n 80 "$LOG"
    printf -- '--- docker compose ps ---\n'
    ( cd "$COMPOSE_DIR" && docker compose ps ) 2>&1
    printf -- '--- docker compose logs (game, playerbot-migrate, mariadb; ostatnie %s) ---\n' "$_tail"
    ( cd "$COMPOSE_DIR" && docker compose logs --no-color --tail "$_tail" game playerbot-migrate mariadb ) 2>&1 |
        sed 's/\([Pp][Aa][Ss][Ss][Ww][Oo][Rr][Dd][A-Za-z_]*[=:][[:space:]]*\)[^[:space:]]*/\1***/g; s/\([Hh][Aa][Ss][Ll][Oo][A-Za-z_]*[=:][[:space:]]*\)[^[:space:]]*/\1***/g'
}

cmd_check() {
    check_tree
    check_arch
    check_machine
    if have docker && compose_ready; then say "Docker: $(docker --version 2>/dev/null)"; else say "Docker: brak (install go zainstaluje)"; fi
    [ "$WANT_SWAP" = 1 ] && say "Install zalozy plik wymiany $SWAP_SIZE_MB MB."
    [ "$MEM_MB" -lt "$MIN_MEM_MB" ] && [ "$SWAP_MB" -eq 0 ] && warn "bez pliku wymiany install odmowi (za malo pamieci)"
    say "Liczba botow dla tej pamieci: $(bots_for_memory "$MEM_MB")"
    return 0
}

usage() {
    cat <<'EOF'
uzycie: sudo sh linux-port/tools/vps-install.sh [polecenie] [opcje]

  install      (domyslne) Docker, plik wymiany, .env, budowa i start w tle,
               potem losowe hasla kont admin i test
  status       czy budowa trwa, skonczyla sie albo nie udala (i koniec logu)
  passwords    hasla kont gry z /root/metin2-accounts.txt
  update       nowa wersja z GitHuba (update.sh run) w tle
  logs [N]     log instalacji i ostatnie N linii logow serwera
  check        tylko sprawdza maszyne, niczego nie zmienia
  add-account LOGIN [OPIS]   nowe konto gry z losowym haslem (dla znajomego)
  sql          zapytanie SQL ze standardowego wejscia, jako root bazy

opcje:
  --address ADRES   publiczny adres serwera (domyslnie: odczytany IPv4)
  --bots N          liczba botow (domyslnie: wedlug pamieci 150/400/800)
  --no-follow       nie pokazuj logu na biezaco (tak robi launcher)
  --follow          pokazuj log na biezaco nawet bez terminala
  --no-swap         nie zakladaj pliku wymiany
  --force           nie odmawiaj przy malym dysku albo malej pamieci
  --from N          (status) log od linii N+1
  --raw             (passwords) same linie: login haslo opis
EOF
}

# One command, read to its closing brace before any of it runs, and it ends in
# exit: update.sh unpacks a new copy of this file while a job of ours may be
# running it, and the shell never reads another line of the file after this.
{
_cmd=install
case "${1:-}" in
    ''|-*) ;;
    *) _cmd=$1; shift ;;
esac
# At most two words besides the options (add-account LOGIN NOTE, logs N),
# kept whole: a note may carry spaces.
P1=''; P2=''; _npos=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --address|--bots|--from)
            [ "$#" -ge 2 ] || die "$1: brak wartosci"
            case "$1" in
                --address) ADDRESS=$2 ;;
                --bots) BOTS=$2 ;;
                --from) FROM=$2 ;;
            esac
            shift ;;
        --address=*) ADDRESS=${1#*=} ;;
        --bots=*) BOTS=${1#*=} ;;
        --from=*) FROM=${1#*=} ;;
        --no-follow) FOLLOW=no ;;
        --follow) FOLLOW=yes ;;
        --no-swap) NO_SWAP=1 ;;
        --force) FORCE=1 ;;
        --raw) RAW=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *)
            _npos=$((_npos + 1))
            case "$_npos" in
                1) P1=$1 ;;
                2) P2=$1 ;;
                *) usage >&2; exit 2 ;;
            esac ;;
    esac
    shift
done
if [ -n "$BOTS" ]; then
    { is_number "$BOTS" && [ "$BOTS" -le 2500 ]; } || die "--bots: liczba od 0 do 2500"
fi
case "$_cmd" in
    help) usage; exit 0 ;;
    _job) ;;
    *)
        if [ "$(id -u)" != 0 ]; then
            have sudo || die "uruchom jako root (albo przez sudo): sudo sh linux-port/tools/vps-install.sh $_cmd"
            # The same command again as root, every option carried over.
            set --
            [ -n "$ADDRESS" ] && set -- "$@" --address "$ADDRESS"
            [ -n "$BOTS" ] && set -- "$@" --bots "$BOTS"
            [ -n "$FROM" ] && set -- "$@" --from "$FROM"
            [ "$FOLLOW" = no ] && set -- "$@" --no-follow
            [ "$FOLLOW" = yes ] && set -- "$@" --follow
            [ "$NO_SWAP" = 1 ] && set -- "$@" --no-swap
            [ "$FORCE" = 1 ] && set -- "$@" --force
            [ "$RAW" = 1 ] && set -- "$@" --raw
            exec sudo -- sh "$SELF" "$_cmd" ${P1:+"$P1"} ${P2:+"$P2"} "$@"
        fi
        ;;
esac
case "$_cmd" in
    install)     cmd_install ;;
    status)      cmd_status ;;
    passwords)   cmd_passwords ;;
    update)      cmd_update ;;
    logs)        cmd_logs "${P1:-200}" ;;
    check)       cmd_check ;;
    add-account) cmd_add_account "$P1" "$P2" ;;
    sql)         cmd_sql ;;
    _job)        run_job "$P1" ;;
    *)           usage >&2; exit 2 ;;
esac
exit $?
}
