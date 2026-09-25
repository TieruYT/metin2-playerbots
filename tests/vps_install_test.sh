#!/bin/sh
# linux-port-mt2009/tools/vps-install.sh on throwaway containers, with docker,
# apt-get, curl, systemctl, the swap tools, df and uname stubbed on PATH.
#
# What is pinned: the .env a fresh install writes (the address, two random
# 48-digit database passwords, the panels on 127.0.0.1 and the game on
# 0.0.0.0, the bot count by memory), that a second run keeps every password,
# that an older .env only gets the security keys it lacks, the refusals (ARM,
# a folder that is not the 2.x line, too little memory with no swap, an empty
# database password beside an existing database), the background job and its
# status, the admin/test passwords changed into a root-only file and never
# written to the log, a friend's account, the masked logs, and Docker
# installed from Docker's repository where it is missing.
#
# From a machine with Docker (Git Bash on Windows included):
#     sh tests/vps_install_test.sh
# It runs itself in debian:12 and ubuntu:24.04 and checks the syntax under
# dash and busybox. Inside a container it is `sh /w/tests/vps_install_test.sh --inside'.
set -u

if [ "${1:-}" != --inside ]; then
    HERE=$(cd "$(dirname "$0")" && pwd)
    REPO=$(cd "$HERE/.." && { pwd -W 2>/dev/null || pwd; })
    rc=0
    for image in debian:12 ubuntu:24.04; do
        echo "==== $image"
        MSYS_NO_PATHCONV=1 docker run --rm -v "$REPO:/w:ro" "$image" sh /w/tests/vps_install_test.sh --inside || rc=1
    done
    echo "==== sh -n under dash and busybox"
    MSYS_NO_PATHCONV=1 docker run --rm -v "$REPO:/w:ro" debian:12 dash -n /w/linux-port-mt2009/tools/vps-install.sh || rc=1
    MSYS_NO_PATHCONV=1 docker run --rm -v "$REPO:/w:ro" busybox:latest sh -n /w/linux-port-mt2009/tools/vps-install.sh || rc=1
    [ "$rc" -eq 0 ] && echo "ALL OK" || echo "SOME FAILED"
    exit "$rc"
fi

SRC=/w
V=/tmp/vt
STUB_BIN=$V/bin
STUB_DIR=$V/stub
SERVER=$V/Serwer
SCRIPT=$SERVER/linux-port/tools/vps-install.sh
export STUB_BIN STUB_DIR

passed=0
failed=0
ok() { passed=$((passed + 1)); echo "ok   - $1"; }
bad() { failed=$((failed + 1)); echo "FAIL - $1"; [ -n "${2:-}" ] && printf '       %s\n' "$2"; }
check() { if eval "$2"; then ok "$1"; else bad "$1" "${3:-}"; fi; }

. /etc/os-release
echo "# system: $PRETTY_NAME"

# ---- the stubs -----------------------------------------------------------------
make_stubs() {
    rm -rf "$V"
    mkdir -p "$STUB_BIN" "$STUB_DIR"
    : > "$STUB_DIR/calls.log"
    : > "$STUB_DIR/sql.log"
    printf 'admin\ntest\n' > "$STUB_DIR/shipped"

    cat > "$STUB_DIR/docker.stub" <<'EOF'
#!/bin/sh
printf 'docker %s\n' "$*" >> "$STUB_DIR/calls.log"
case "${1:-}" in
    --version) echo "Docker version 27.3.1, build stub"; exit 0 ;;
    info) [ -f "$STUB_DIR/docker_down" ] && exit 1; exit 0 ;;
    volume) if [ "${2:-}" = inspect ]; then [ -f "$STUB_DIR/db_volume" ] && exit 0; exit 1; fi; exit 0 ;;
    compose) shift ;;
    *) exit 0 ;;
esac
case "${1:-}" in
    version) echo "Docker Compose version v2.29.7"; exit 0 ;;
    up)
        sleep 1
        if [ -f "$STUB_DIR/up_fails" ]; then echo "failed to solve: exit code: 2" >&2; exit 17; fi
        echo " Container metin2-game  Started"; exit 0 ;;
    ps) echo "NAME          SERVICE   STATUS"; echo "metin2-game   game      Up 2 minutes"; exit 0 ;;
    logs) echo "game-1  | PASSWORD=supersecret123"; echo "game-1  | boot ok"; exit 0 ;;
    exec)
        sql=$(cat)
        printf '%s\n' "$sql" >> "$STUB_DIR/sql.log"
        [ -f "$STUB_DIR/db_down" ] && exit 1
        case "$sql" in
            *"login IN ('admin','test')"*) echo 2 ;;
            *"SELECT login FROM account.account WHERE (login='admin'"*) cat "$STUB_DIR/shipped" ;;
            *"UPDATE account.account SET password="*)
                l=$(printf '%s' "$sql" | sed -n "s/.*WHERE login='\([a-z0-9]*\)'.*/\1/p")
                grep -v "^$l\$" "$STUB_DIR/shipped" > "$STUB_DIR/shipped.new"
                mv "$STUB_DIR/shipped.new" "$STUB_DIR/shipped" ;;
            *"SELECT COUNT(*) FROM account.account WHERE login="*)
                l=$(printf '%s' "$sql" | sed -n "s/.*WHERE login='\([a-z0-9]*\)'.*/\1/p")
                if grep -qx "$l" "$STUB_DIR/taken" 2>/dev/null; then echo 1; else echo 0; fi ;;
        esac
        exit 0 ;;
esac
exit 0
EOF

    cat > "$STUB_BIN/curl" <<'EOF'
#!/bin/sh
printf 'curl %s\n' "$*" >> "$STUB_DIR/calls.log"
out=''
while [ "$#" -gt 0 ]; do
    case "$1" in -o) out=$2; shift ;; esac
    shift
done
if [ -n "$out" ]; then echo "-----BEGIN PGP PUBLIC KEY BLOCK-----" > "$out"; exit 0; fi
[ -f "$STUB_DIR/no_ip" ] && exit 7
echo "203.0.113.7"
EOF

    cat > "$STUB_BIN/apt-get" <<'EOF'
#!/bin/sh
printf 'apt-get %s\n' "$*" >> "$STUB_DIR/calls.log"
case " $* " in
    *" docker-ce "*) cp "$STUB_DIR/docker.stub" "$STUB_BIN/docker"; chmod +x "$STUB_BIN/docker" ;;
esac
exit 0
EOF

    cat > "$STUB_BIN/systemctl" <<'EOF'
#!/bin/sh
printf 'systemctl %s\n' "$*" >> "$STUB_DIR/calls.log"
exit 0
EOF

    cat > "$STUB_BIN/uname" <<'EOF'
#!/bin/sh
if [ "${1:-}" = -m ] && [ -n "${STUB_ARCH:-}" ]; then echo "$STUB_ARCH"; exit 0; fi
for u in /usr/bin/uname /bin/uname; do [ -x "$u" ] && exec "$u" "$@"; done
exit 1
EOF

    cat > "$STUB_BIN/df" <<'EOF'
#!/bin/sh
echo "Filesystem 1024-blocks Used Available Capacity Mounted on"
echo "overlay 104857600 1000 ${STUB_DISK_KB:-83886080} 1% /"
EOF

    for tool in fallocate dd mkswap swapon; do
        cat > "$STUB_BIN/$tool" <<'EOF'
#!/bin/sh
printf '%s %s\n' "$(basename "$0")" "$*" >> "$STUB_DIR/calls.log"
[ -f "$STUB_DIR/swap_fails" ] && exit 1
case "$(basename "$0")" in
    fallocate) eval "f=\${$#}"; : > "$f" ;;
    dd) for a in "$@"; do case "$a" in of=*) : > "${a#of=}" ;; esac; done ;;
esac
exit 0
EOF
    done
    chmod +x "$STUB_BIN"/* "$STUB_DIR/docker.stub"
}

docker_present() { cp "$STUB_DIR/docker.stub" "$STUB_BIN/docker"; chmod +x "$STUB_BIN/docker"; }

make_tree() {
    rm -rf "$SERVER"
    mkdir -p "$SERVER/linux-port/docker/panel" "$SERVER/linux-port/tools" "$SERVER/files"
    echo 2.2.12 > "$SERVER/VERSION"
    echo mt2009 > "$SERVER/linux-port/docker/ENGINE"
    echo "name: metin2" > "$SERVER/linux-port/docker/docker-compose.yml"
    cp "$SRC/linux-port-mt2009/docker/.env.example" "$SERVER/linux-port/docker/.env.example"
    cp "$SRC/linux-port-mt2009/tools/vps-install.sh" "$SCRIPT"
    cp "$SRC/linux-port-mt2009/tools/update.sh" "$SERVER/linux-port/tools/update.sh"
    echo "# panel" > "$SERVER/files/admin_panel.py"
}

meminfo() { # MemTotal_kB SwapTotal_kB
    printf 'MemTotal:       %s kB\nMemFree:         100000 kB\nSwapTotal:      %s kB\n' "$1" "$2" > "$V/meminfo"
}

run() { # the script as root with the stubs; output in $V/out, code in $rc
    PATH="$STUB_BIN:$PATH" \
    M2_VPS_MEMINFO="$V/meminfo" M2_VPS_ACCOUNTS="$V/metin2-accounts.txt" \
    M2_VPS_SWAPFILE="$V/swapfile" M2_VPS_FSTAB="$V/fstab" M2_VPS_DB_WAIT=10 M2_VPS_POLL=1 \
        sh "$SCRIPT" "$@" > "$V/out" 2>&1
    rc=$?
}

envv() { sed -n "s/^$1=//p" "$SERVER/linux-port/docker/.env" | head -n 1; }
envcount() { grep -c "^$1=" "$SERVER/linux-port/docker/.env"; }
is_hex48() { printf '%s' "$1" | grep -Eq '^[0-9a-f]{48}$'; }

wait_done() { # the background job, up to 40 seconds
    i=0
    while [ "$i" -lt 40 ]; do
        run status
        grep -q '^state=running$' "$V/out" || return 0
        sleep 1
        i=$((i + 1))
    done
    return 1
}

show_out() { sed 's/^/       | /' "$V/out" | tail -n 25; }

# ---- 1. a fresh install on 8 GB ------------------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
# Before any install: the launcher parses --raw's stdout as accounts, so the
# note that there is no file yet must not be on it.
PATH="$STUB_BIN:$PATH" M2_VPS_MEMINFO="$V/meminfo" M2_VPS_ACCOUNTS="$V/metin2-accounts.txt" \
    sh "$SCRIPT" passwords --raw > "$V/raw-out" 2> "$V/raw-err"
check "passwords --raw before any install: nothing on stdout, the note on stderr" '[ ! -s "$V/raw-out" ] && grep -q "Nie ma jeszcze" "$V/raw-err"' "stdout: $(head -c 120 "$V/raw-out")"
: > "$V/fstab"
run install --no-follow
check "fresh install exits 0" '[ "$rc" -eq 0 ]' "rc=$rc"
[ "$rc" -eq 0 ] || show_out
ENVF=$SERVER/linux-port/docker/.env
check ".env is written" '[ -f "$ENVF" ]'
check ".env is root-only (600)" '[ "$(stat -c %a "$ENVF")" = 600 ]' "mode $(stat -c %a "$ENVF" 2>/dev/null)"
check "M2_PUBLIC_ADDRESS is the detected IPv4" '[ "$(envv M2_PUBLIC_ADDRESS)" = 203.0.113.7 ]' "got $(envv M2_PUBLIC_ADDRESS)"
ROOTPW=$(envv M2_DB_ROOT_PASSWORD); DBPW=$(envv M2_DB_PASSWORD)
check "M2_DB_ROOT_PASSWORD is 48 hex digits" 'is_hex48 "$ROOTPW"' "got '$ROOTPW'"
check "M2_DB_PASSWORD is 48 hex digits" 'is_hex48 "$DBPW"' "got '$DBPW'"
check "the two database passwords differ" '[ "$ROOTPW" != "$DBPW" ]'
check "panels on 127.0.0.1" '[ "$(envv M2_PANEL_BIND_ADDRESS)" = 127.0.0.1 ]'
check "game ports on 0.0.0.0" '[ "$(envv M2_HOST_BIND_ADDRESS)" = 0.0.0.0 ]'
check "400 bots on 8 GB" '[ "$(envv PLAYERBOT_AUTOSPAWN_COUNT)" = 400 ]' "got $(envv PLAYERBOT_AUTOSPAWN_COUNT)"
for key in M2_PUBLIC_ADDRESS M2_DB_ROOT_PASSWORD M2_DB_PASSWORD M2_PANEL_BIND_ADDRESS M2_HOST_BIND_ADDRESS PLAYERBOT_AUTOSPAWN_COUNT; do
    check "$key appears once" '[ "$(envcount $key)" = 1 ]' "count $(envcount $key)"
done
check "the rest of .env.example is kept" 'grep -q "^M2_PLAYERBOT_WORLD_LAYOUT=unified$" "$ENVF"'
check "no swap file on 8 GB" '! grep -q "^fallocate" "$STUB_DIR/calls.log"'
check "the panel context is staged" '[ -f "$SERVER/linux-port/docker/panel/app/admin_panel.py" ]'
check "the empty package directory exists" '[ -d "$SERVER/linux-port/docker/game/src/serverfiles/share/package" ]'
check "the build ran detached" 'grep -q "^docker compose up -d --build" "$STUB_DIR/calls.log" || { sleep 3; grep -q "^docker compose up -d --build" "$STUB_DIR/calls.log"; }'
wait_done
check "status says done" 'grep -q "^state=done$" "$V/out"' "$(head -n 3 "$V/out" | tr '\n' ' ')"
check "status names the panels' address" 'grep -q "^panel_bind=127.0.0.1$" "$V/out"'
check "status carries the log" 'grep -q "^--- log ---$" "$V/out" && grep -q "koniec: done" "$V/out"'
ACC=$V/metin2-accounts.txt
check "accounts file written" '[ -f "$ACC" ]'
check "accounts file is 600" '[ "$(stat -c %a "$ACC")" = 600 ]' "mode $(stat -c %a "$ACC" 2>/dev/null)"
ADMINPW=$(awk '$1 == "admin" { print $2 }' "$ACC")
TESTPW=$(awk '$1 == "test" { print $2 }' "$ACC")
check "admin got a 12-character password" 'printf "%s" "$ADMINPW" | grep -Eq "^[A-Za-z0-9]{12}$"' "got '$ADMINPW'"
check "test got a 12-character password" 'printf "%s" "$TESTPW" | grep -Eq "^[A-Za-z0-9]{12}$"' "got '$TESTPW'"
check "two UPDATEs reached the database" '[ "$(grep -c "^UPDATE account.account SET password=" "$STUB_DIR/sql.log")" = 2 ]'
check "the UPDATE carries the new password's hash, not the password on a command line" 'grep -q "SHA1(UNHEX(SHA1(.$ADMINPW.)))" "$STUB_DIR/sql.log" && ! grep -q "$ADMINPW" "$STUB_DIR/calls.log"'
check "no password in the install log" '! grep -qF "$ADMINPW" "$SERVER/.vps-install.log" && ! grep -qF "$TESTPW" "$SERVER/.vps-install.log" && ! grep -qF "$ROOTPW" "$SERVER/.vps-install.log"'
run passwords --raw
check "passwords --raw prints the file's accounts" 'grep -q "^admin $ADMINPW " "$V/out" && grep -q "^test $TESTPW " "$V/out"'
run passwords
check "passwords prints them for a person" 'grep -q "login: admin" "$V/out" && grep -q "$ADMINPW" "$V/out"'

# ---- 2. the second run keeps everything ----------------------------------------
run install --no-follow
check "second run exits 0" '[ "$rc" -eq 0 ]' "rc=$rc"
wait_done
check "second run keeps M2_DB_ROOT_PASSWORD" '[ "$(envv M2_DB_ROOT_PASSWORD)" = "$ROOTPW" ]'
check "second run keeps M2_DB_PASSWORD" '[ "$(envv M2_DB_PASSWORD)" = "$DBPW" ]'
check "second run keeps the admin password" '[ "$(awk '"'"'$1 == "admin" { print $2 }'"'"' "$ACC")" = "$ADMINPW" ]'
check "second run adds no duplicate keys" '[ "$(envcount M2_PANEL_BIND_ADDRESS)" = 1 ] && [ "$(envcount M2_DB_PASSWORD)" = 1 ]'
check "second run changes no password in the database" '[ "$(grep -c "^UPDATE account.account SET password=" "$STUB_DIR/sql.log")" = 2 ]'

# ---- 3. a friend's account ---------------------------------------------------------
echo janek > "$STUB_DIR/taken"
run add-account "Janek!" "znajomy Janek"
check "add-account exits 0" '[ "$rc" -eq 0 ]' "$(cat "$V/out")"
check "a taken login gets a number" 'grep -q "^login=janek2$" "$V/out"'
FRIENDPW=$(sed -n 's/^password=//p' "$V/out")
check "the friend's password is in the accounts file" 'grep -q "^janek2 $FRIENDPW znajomy Janek$" "$ACC"'
check "the friend's account was inserted" 'grep -q "INSERT INTO account.account (login, password, social_id, status) VALUES (.janek2." "$STUB_DIR/sql.log"'

# ---- 4. the logs are masked ------------------------------------------------------
run logs 50
check "logs mask a password" 'grep -q "PASSWORD=\*\*\*" "$V/out" && ! grep -q supersecret123 "$V/out"'

# ---- 5. an older .env gets only the missing security keys --------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
ENVF=$SERVER/linux-port/docker/.env
grep -v -e '^M2_PANEL_BIND_ADDRESS=' -e '^M2_HOST_BIND_ADDRESS=' "$SERVER/linux-port/docker/.env.example" |
    sed -e 's/^M2_DB_ROOT_PASSWORD=.*/M2_DB_ROOT_PASSWORD=oldroot/' -e 's/^M2_DB_PASSWORD=.*/M2_DB_PASSWORD=oldgame/' \
        -e 's/^M2_PUBLIC_ADDRESS=.*/M2_PUBLIC_ADDRESS=game.example.org/' -e 's/^PLAYERBOT_AUTOSPAWN_COUNT=.*/PLAYERBOT_AUTOSPAWN_COUNT=999/' > "$ENVF"
run install --no-follow
check "older .env: exits 0" '[ "$rc" -eq 0 ]' "rc=$rc"
check "older .env: panels put on 127.0.0.1" '[ "$(envv M2_PANEL_BIND_ADDRESS)" = 127.0.0.1 ]'
check "older .env: game ports on 0.0.0.0" '[ "$(envv M2_HOST_BIND_ADDRESS)" = 0.0.0.0 ]'
check "older .env: database passwords untouched" '[ "$(envv M2_DB_ROOT_PASSWORD)" = oldroot ] && [ "$(envv M2_DB_PASSWORD)" = oldgame ]'
check "older .env: its address and bot count untouched" '[ "$(envv M2_PUBLIC_ADDRESS)" = game.example.org ] && [ "$(envv PLAYERBOT_AUTOSPAWN_COUNT)" = 999 ]'
wait_done

# ---- 6. an empty database password beside an existing database --------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
sed -e 's/^M2_DB_PASSWORD=.*/M2_DB_PASSWORD=/' -e 's/^M2_DB_ROOT_PASSWORD=.*/M2_DB_ROOT_PASSWORD=kept/' \
    "$SERVER/linux-port/docker/.env.example" > "$SERVER/linux-port/docker/.env"
touch "$STUB_DIR/db_volume"
run install --no-follow
check "empty password + existing database: refused" '[ "$rc" -ne 0 ] && grep -q "baza juz istnieje" "$V/out"' "rc=$rc"
check "and .env keeps the empty password" '[ -z "$(envv M2_DB_PASSWORD)" ]'

# ---- 7. an ARM VPS is refused ---------------------------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
STUB_ARCH=aarch64; export STUB_ARCH
run install --no-follow
unset STUB_ARCH
check "ARM refused" '[ "$rc" -ne 0 ] && grep -q "ARM" "$V/out"' "rc=$rc $(tail -n 1 "$V/out")"
check "ARM: nothing written" '[ ! -f "$SERVER/linux-port/docker/.env" ] && ! grep -q "^apt-get" "$STUB_DIR/calls.log"'

# ---- 8. a folder that is not the 2.x line --------------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
rm "$SERVER/linux-port/docker/ENGINE"
run install --no-follow
check "a 1.x folder is refused" '[ "$rc" -ne 0 ] && grep -q "2.x" "$V/out"'

# ---- 9. 2 GB: a swap file, 150 bots; and refused where swap cannot be made -------------
make_stubs; make_tree; docker_present
meminfo 2000000 0
: > "$V/fstab"
run install --no-follow
check "2 GB with a swap file: exits 0" '[ "$rc" -eq 0 ]' "rc=$rc"
check "2 GB: a swap file was made" 'grep -q "^fallocate -l 4096M $V/swapfile" "$STUB_DIR/calls.log" && grep -q "^swapon $V/swapfile" "$STUB_DIR/calls.log"'
check "2 GB: the swap file is in fstab" 'grep -q "^$V/swapfile none swap sw 0 0$" "$V/fstab"'
check "2 GB: 150 bots" '[ "$(envv PLAYERBOT_AUTOSPAWN_COUNT)" = 150 ]'
wait_done
make_stubs; make_tree; docker_present
meminfo 2000000 0
: > "$V/fstab"
touch "$STUB_DIR/swap_fails"
run install --no-follow
check "2 GB, swap refused by the system: install refused" '[ "$rc" -ne 0 ] && grep -q "pamieci" "$V/out"' "rc=$rc"
check "and nothing installed" '! grep -q "^apt-get" "$STUB_DIR/calls.log" && [ ! -f "$SERVER/linux-port/docker/.env" ]'
check "and no swap file left behind" '[ ! -e "$V/swapfile" ]'

# ---- 10. a small disk --------------------------------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
STUB_DISK_KB=8000000; export STUB_DISK_KB
run install --no-follow
unset STUB_DISK_KB
check "8 GB of free disk: refused" '[ "$rc" -ne 0 ] && grep -q "wolne tylko" "$V/out"'

# ---- 11. Docker missing: installed from Docker's repository ------------------------------
make_stubs; make_tree
rm -f /etc/apt/sources.list.d/docker.list /etc/apt/sources.list.d/docker.sources
meminfo 16500000 2097148
run install --no-follow
check "no Docker: exits 0" '[ "$rc" -eq 0 ]' "rc=$rc"
[ "$rc" -eq 0 ] || show_out
check "no Docker: docker-ce and the compose plugin from apt" 'grep -q "^apt-get -q install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin" "$STUB_DIR/calls.log"'
CODENAME=${UBUNTU_CODENAME:-$VERSION_CODENAME}
check "no Docker: the repository names this system" 'grep -q "https://download.docker.com/linux/$ID $CODENAME stable" /etc/apt/sources.list.d/docker.list' "$(cat /etc/apt/sources.list.d/docker.list 2>/dev/null)"
check "no Docker: the repository's key fetched" 'grep -q "download.docker.com/linux/$ID/gpg" "$STUB_DIR/calls.log" && [ -s /etc/apt/keyrings/docker.asc ]'
check "no Docker: the service enabled" 'grep -q "^systemctl enable --now docker" "$STUB_DIR/calls.log" || grep -q "^docker info" "$STUB_DIR/calls.log"'
check "16 GB: 800 bots" '[ "$(envv PLAYERBOT_AUTOSPAWN_COUNT)" = 800 ]'
wait_done

# ---- 12. a build that fails says so --------------------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
touch "$STUB_DIR/up_fails"
run install --no-follow
wait_done
check "a failed build: status says failed" 'grep -q "^state=failed$" "$V/out" && grep -q "^phase=build$" "$V/out"' "$(head -n 4 "$V/out" | tr '\n' ' ')"
check "a failed build: the passwords are not touched" '[ ! -f "$V/metin2-accounts.txt" ]'

# ---- 13. no address can be read, and --address -----------------------------------------
make_stubs; make_tree; docker_present
meminfo 8000000 0
touch "$STUB_DIR/no_ip"
run install --no-follow
check "no public address: refused with advice" '[ "$rc" -ne 0 ] && grep -q -- "--address" "$V/out"'
run install --no-follow --address play.example.net
check "--address is taken as given" '[ "$rc" -eq 0 ] && [ "$(envv M2_PUBLIC_ADDRESS)" = play.example.net ]' "rc=$rc"
wait_done

echo "# $passed passed, $failed failed"
[ "$failed" -eq 0 ]
