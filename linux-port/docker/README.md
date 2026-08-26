# Running your Metin2 server with Docker

This runs a complete Metin2 server — the game, its database, and the web admin
panel — on any Linux server you can rent.

You do not need to know anything about Linux, Docker or Metin2 server files to
follow this. You need to be able to copy and paste.

---

## What you need

**A VPS.** Any provider works — Hetzner, Contabo, Strato, 1blu, DigitalOcean,
Netcup. Choose:

| | Minimum | Comfortable |
|---|---|---|
| RAM | 4 GB | 8 GB |
| Disk | 40 GB | 80 GB |
| CPU | 2 cores | 4 cores |
| OS image | **Ubuntu 24.04** or **Debian 12** | same |

Nothing exotic is required. No FreeBSD, no "own ISO", no nested virtualisation.
That is the entire point of this: the original server files only ran on FreeBSD,
which most cheap VPS providers do not offer at all.

Building the server from source needs about **8 GB of free disk** and takes
**10–25 minutes** the first time. After that, starting takes seconds. Fetching
and unpacking the server-file package beforehand needs several GB more and takes
as long as your connection does — it is a 1.4 GB download.

Building a client for your players to download (optional, and covered further
down) wants roughly 7 GB free on top of that.

---

> ### The short way
>
> `installer/install.sh` in this repository does every step on this page for
> you — Docker, the server files, the passwords, the firewall, HTTPS and the
> client — and prints the three things you need at the end. The steps below are
> what it does, for when you would rather do it yourself or something went
> wrong. On Windows, `installer/install.ps1` does the same thing bound to
> `127.0.0.1`, for a server only you can play on.

## Step 1 — install Docker

Log in to your VPS over SSH, then paste this **one line**:

```sh
curl -fsSL https://get.docker.com | sh
```

That is Docker's official installer. Wait for it to finish.

## Step 2 — assemble the server

Put this project on the VPS (upload it, or `git clone` it). **A checkout on its
own cannot be built.** This repository holds the Linux port — one patch and the
scripts around it — and none of the game: not the server source, not the data
tree, not the database dumps. Those belong to Ymir/Webzen and to whoever
assembled the r40250 package, and there is no release archive of them here and
never will be.

So the first thing to run is the script with a compatible local baseline:

```sh
cd linux-port
./fetch-sources.sh --archive /path/to/Reference_Server.zip
```

It extracts the source, the `share/` data tree and the SQL dumps, applies the
port patch — zero fuzz and zero rejects or it stops — and fills in the Docker
build context. No game archive is bundled or downloaded automatically. The
script is idempotent: a second run costs seconds, and a failed one can simply
be repeated.

If you already have the server-file package, hand it over rather than
downloading 1.4 GB again:

```sh
./fetch-sources.sh --reference-dir "/path/to/[40250] Reference Serverfile"
./fetch-sources.sh --archive /path/to/serverfiles.zip
```

> **If MEGA answers `509 (over quota)`.** An anonymous MEGA link has a bandwidth
> limit, and once the share's owner has spent the day's allowance every chunk
> comes back `509`. `fetch-sources.sh` notices and moves straight on to the next
> copy of the same archive that `artifacts.json` names — the identical file,
> checked against the same `sha256`, so this costs seconds rather than the
> afternoon it used to. Only when every copy is out of reach does it stop, and
> then the way past it is to download the package on another machine and pass it
> with `--archive`.

`./fetch-sources.sh status` says what has been acquired so far, and
`./fetch-sources.sh check` verifies the staged tree.

## Step 3 — start the server

```sh
cd docker
cp .env.example .env
nano .env
```

`nano` is a text editor. Fill in the three settings at the top:

- `M2_PUBLIC_ADDRESS` — your VPS's public IP address.
  Find it by running `curl -4 ifconfig.me`.
- `M2_DB_ROOT_PASSWORD` and `M2_DB_PASSWORD` — two long random passwords.
  Generate them by running `openssl rand -hex 24` twice.

Save with `Ctrl+O`, `Enter`, then exit with `Ctrl+X`.

Now start everything:

```sh
docker compose up -d --build
```

The first run compiles the server from source. Go and make coffee.

## Step 4 — get your admin password

```sh
docker compose logs panel | grep -A4 "ADMIN PANEL PASSWORD"
```

Save that password. It is shown once.

Then open **`http://YOUR-IP:7788`** in a browser.

## Step 5 — check it worked

```sh
docker compose ps
```

All three services should say `healthy` or `running`. Players can now connect
their client to **your IP on port 11000**.

Two accounts already exist in the shipped database, for testing:
`admin` / `123456789` and `test` / `123456789`. **Change both before you tell
anybody the address.** Those passwords are not a secret — the hashes are inside
a database dump that thousands of people have — so on a public server they are
two free administrator accounts for whoever tries them first. `admin` is an
IMPLEMENTOR.

---

## Day-to-day commands

Run all of these from the `linux-port/docker` directory.

| What you want | Command |
|---|---|
| Start the server | `docker compose up -d` |
| Stop the server | `docker compose down` |
| Restart everything | `docker compose restart` |
| See what is running | `docker compose ps` |
| Watch the game's log | `docker compose logs -f game` |
| Check for errors | `docker compose exec game m2ctl syserr` |
| See which cores are up | `docker compose exec game m2ctl status` |
| Check disk used by logs | `docker compose exec game m2ctl logsize` |
| Apply a changed `.env` | `docker compose up -d` |
| Update after a code change | `docker compose up -d --build` |

**`docker compose down` is safe.** It stops the containers but keeps every
account, character and item. `docker compose up -d` brings them all back.

> ### The one dangerous command
>
> `docker compose down -v`
>
> The `-v` deletes the **volumes** — every account, every character, every item,
> your admin panel password, and your uploaded client. There is no undo. Only
> use it when you deliberately want to start over from an empty server.

---

## Backups

Everything irreplaceable is in the database. Back it up like this:

```sh
docker compose exec -T mariadb \
  mariadb-dump -uroot -p"$(grep M2_DB_ROOT_PASSWORD .env | cut -d= -f2)" \
  --databases account player common log hotbackup \
  | gzip > backup-$(date +%F).sql.gz
```

To restore into a fresh stack:

```sh
gunzip -c backup-2026-08-07.sql.gz | docker compose exec -T mariadb \
  mariadb -uroot -p"$(grep M2_DB_ROOT_PASSWORD .env | cut -d= -f2)"
```

Guild emblems live in the `game-var` volume; save it with:

```sh
docker run --rm -v metin2_game-var:/v -v "$PWD:/out" alpine \
  tar czf /out/guildmarks-$(date +%F).tgz -C /v .
```

---

## Giving players the game client

The panel has a **Download the game** button. Until there is a client to hand
out it tells people "the game download is not ready yet". This builds one:

```sh
docker compose run --rm client-builder
```

Place a compatible native client archive that you are authorised to use in
`./client-archive/` first. The builder writes **your** address into that client
and puts the finished `client.zip` where the panel serves it. Then the button
works. This project supplies no default client download URL.

The first run takes as long as your line and your disk take: measured at four
minutes on a fast machine with a 60 Mbit link, and it can be an hour on a small
VPS with slow storage. It prints where it has got to every 30 seconds.
Interrupting it is safe — nothing reaches the panel until the whole thing has
finished and passed its checks, and the next run continues the download from
where it stopped rather than starting over.

**Changing the address later is cheap.** Edit `M2_PUBLIC_ADDRESS` in `.env`, then:

```sh
docker compose run --rm client-builder
```

Nothing is downloaded again — it rewrites the one file inside the client that
carries the address and copies the result across. Seconds, not another evening.

Two things to know:

* The panel reads the file's size and checksum once, when it starts. Run
  `docker compose restart panel` if you want those shown on the download page.
* Players do **not** have to type your address in. It is already in the client.

### When the download link stops working

The link belongs to somebody else's MEGA account, so one day it will stop
working. That is not a fault in your server, and there are two ways round it.

There is also a temporary version of the same problem: an anonymous MEGA share
has a bandwidth quota, and once it is spent every request comes back
`509 (over quota)` for a few hours. The builder tries the fallback links in
`artifacts.json` before it gives up — the same archive somewhere else — so this
is usually invisible. If every copy is out of reach, supply the archive
yourself.

If you already handed `fetch-sources.sh` an archive with `--archive`, that is
the same file this tool wants. Put a copy in `./client-archive/` and it
downloads nothing.

Download the archive on your own PC and drop it in `./client-archive/`:

```sh
ls client-archive/            # anything .rar, .zip or .7z is picked up
docker compose run --rm client-builder
```

Or point the builder at another link:

```sh
M2_CLIENT_ARCHIVE_URL=https://example.com/serverfiles.rar \
  docker compose run --rm client-builder
```

A ready-made `client.zip` from anywhere else works too — it is recognised by the
launcher `.exe` inside it and used directly. And if you already have a finished
client that needs no patching, the panel takes it without this tool at all:

```sh
docker compose cp ./client.zip panel:/usr/local/m2panel/client.zip
docker compose restart panel
```

If you would rather host the download elsewhere (Google Drive, your own web
space), put that link in `M2_CLIENT_URL` in `.env` instead, and `/download`
redirects there.

### What it does, and what it will not do

It uses the FreeBSD installer's own `pack_prepare_client()` — the same file,
copied into the image unmodified — so a client built here and a client built by
the FreeBSD installer are prepared identically. That code:

* writes `serverinfo.py` **next to the launcher `.exe`**, which is the only copy
  this client reads. A `root/serverinfo.py` is never read;
* moves `pack/root.epk` aside first and puts the 90 unpacked scripts on disk,
  because this is a `_DISTRIBUTE` build and searches the pack *before* loose
  files — without that step the address you set would simply lose;
* lists only the channels you are actually running;
* leaves out `ClientVS22.zip`, the client's C++ **source**, and the `Eternexus`
  folder. Neither is checked as an afterthought: the finished zip is refused if
  either is in it.

Useful extras:

```sh
docker compose run --rm client-builder status    # what is cached, and at which address
docker compose run --rm client-builder clean     # throw the cache away (~3 GB)

# force a full rebuild from the cached archive (no download)
M2_CLIENT_FORCE=rebuild docker compose run --rm client-builder
```

---

## Playing in the browser

Off by default, and there is a reason it takes two deliberate steps to switch
on: half of it is not in these server files.

**A browser cannot open a TCP socket.** The game speaks TCP and nothing else,
so a page that wants to play needs something beside the server that turns a
WebSocket into a TCP connection to the game. That is the `wsbridge` service. It
is in a compose profile, so `docker compose up -d` neither starts nor builds
it, and a server that never switches this on is not affected by any of it.

The other half is the client itself, built to WebAssembly. It is **not** part of
this project: it is built from game data that is not ours to hand out, the same
reason `client.zip` is not shipped here either. You supply it — 421 files and
about 1.7 GB, of which 408 are content-addressed data blobs.

**The installer does all of this for you** — `--web-client` on a first install, or
just run it again on an existing server and say yes when it offers the browser
client. It downloads the two archives `artifacts.json` names, checks their
checksums, unpacks them into a versioned directory on the panel's volume and
swings a symlink at it.

What follows is the same thing by hand, and it is only for somebody who has
built their own client:

```sh
# 1. the bridge
docker compose --profile browser up -d wsbridge

# 2. the client, onto the panel's volume.
#
#    `./browser' is a directory YOU supply -- 421 files, about 1.7 GB. It does
#    not exist in a checkout, so this line answers
#        lstat .../browser: no such file or directory
#    for anyone who has not built one. That is the command failing correctly,
#    not the install being broken: use the installer instead.
#
#    Into current/, not the parent: the panel and nginx both serve
#    browser/current, which is the symlink that makes an upgrade one atomic
#    rename. A client dropped a level above it is invisible to both.
docker compose cp ./browser panel:/usr/local/m2panel/browser/current

# 3. tell the panel to offer it
#    M2_BROWSER_PLAY=1 in .env, then
docker compose up -d panel
```

The panel then shows a **Play in the browser** button on its front page — and
only then. It checks all three: the setting, an `index.html` in that directory,
and that the bridge answers. With any of them missing it shows nothing, which
is better than a button that leads nowhere.

`install.sh` does step 1 for you whenever `M2_BROWSER_PLAY=1`, and stops the
bridge again when you set it back to 0.

### Where the page is told to connect

The client reads the bridge's address out of its own page URL, so the panel's
button links to

    /play/?serverHost=<this server>&serverPort=<port>[&serverTLS=1]

and the page boots straight into the game rather than asking the player for an
address they have no way of knowing. Those three parameters are the client's
own; the panel fills them in per request, so nothing has to be rebuilt when the
server's address or certificate changes.

**Behind a domain the port is 443 — and that is not a preference.** The client
matches `serverHost` against `[A-Za-z0-9.\-]+`, a pattern that excludes `/` on
purpose, so the bridge can only ever be named as `host:port` and never as a path
on your site. And a page served over HTTPS may not open a `ws://` connection at
all: the browser blocks it as mixed content, silently. The two together leave
one arrangement, and `install.sh` writes it — nginx on 443 routes the two URL
prefixes the client uses, `/to/` and `/ping`, through to the bridge, which
listens on 127.0.0.1 and is not otherwise reachable. Without a domain everything
is plain HTTP and the bridge is published on port 7789 directly.

nginx also serves `/play/` straight off the panel's volume, with the three cache
rules the client's own static server documents (content-addressed blobs
immutable for a year, `manifest.bin` never cached, everything else revalidated).
Pushing 1.7 GB through the panel would occupy its worker threads for as long as
a player is loading; where nginx cannot read the volume the panel does it
anyway, and it works, slowly.

### It is not an open proxy

The bridge dials one host, fixed at startup. The client names a host in every
WebSocket URL — that is its protocol — and the bridge reads that name, logs it
and throws it away: the socket goes to the game container whatever a page asks
for. The **port** from the URL is used, and it must be on a list computed at
startup from your channel count.

This matters because the client tree ships its own proxy, `m2-ws2tcp`, which
dials whatever the path names. That is correct for the player running it on
their own machine, and its `-allow` list checks the **host only** — so even
pinned, `/to/yourserver:15000` would reach the db core, which speaks an
unauthenticated protocol. Hence a second implementation on the server side, on
the same wire. `linux-port/docker/wsbridge/README.md` has the full contract, and
`wasm-port/scripts/ws_selftest.py` checks every claim in this paragraph without
a game server, a browser or a client build.

---

## Troubleshooting

### Playing in the browser does not work

Work down the list — the failures look identical from the page and have
completely different causes.

* **No button on the front page.** One of the three conditions is not met.
  `docker compose exec panel ls /usr/local/m2panel/browser` should show an
  `index.html`; `grep M2_BROWSER_PLAY .env` should say 1; and
  `docker compose ps wsbridge` should show it running.
* **The page shows its own connection dialog** asking for a proxy address.
  It was opened without the parameters — use the panel's button rather than a
  bookmark of `/play/`.
* **The dialog says "something answered, but not an m2-ws2tcp proxy".** The
  page's gate fetched `/ping` and got something else: on a domain that means
  nginx is answering it instead of passing it through, so re-run `install.sh`.
  `curl -s https://yourdomain/ping` should start with `m2-ws2tcp`.
* **The button is there and the game never connects.** Open the browser's
  developer console. `Mixed Content: ... insecure WebSocket` means the page is
  HTTPS and the address is `ws://` — re-run `install.sh`. A 502 on `/to/...`
  means nginx is configured but the bridge is not running.
* **It connects and then stops.** `docker compose logs wsbridge` names each
  destination once. A `404` for a port means the port is not on its list, which
  usually means `M2_CHANNELS` was raised without restarting the bridge.

### Players log in, then hang on "connecting to the server"

**This is the most common problem and it has one cause:** `M2_PUBLIC_ADDRESS`
is empty or wrong in your `.env`.

The login server tells the client which address to use for the game world. If
it does not know your public address, it hands out the container's internal
address — which exists only inside your VPS. The login step works because that
happens on the connection the player already made; the next step fails because
the client is being sent somewhere unreachable.

```sh
grep M2_PUBLIC_ADDRESS .env      # must be your real public IP
curl -4 ifconfig.me              # this is your real public IP
```

Fix it, then `docker compose up -d`.

### Nobody can connect at all

Your VPS provider's firewall is probably blocking the ports. You need **11000**
and **13000–13002** open for TCP. Check your provider's control panel, and if
the VPS itself runs a firewall:

```sh
sudo ufw allow 11000/tcp
sudo ufw allow 13000:13002/tcp
sudo ufw allow 7788/tcp
```

### The `game` container keeps restarting

```sh
docker compose logs game | tail -50
```

Look for these:

- **`Can not get public ip address`** — the container could not work out its own
  address. Very unusual under Docker. Set `M2_BIND_IP` to the container's
  address, or report it.
- **`Assertion ... failed`** *(from the db core)* — a database problem, not a
  code problem. The db core is built with its internal checks live, so it stops
  rather than writing bad data. Usually it means the import did not complete.
  Recreate the database from scratch — **this deletes all characters**:
  ```sh
  docker compose down
  docker volume rm metin2_db-data
  docker compose up -d
  ```
- **`multiple MAP_ALLOW setting`** — a map is listed twice. Only possible if you
  have edited the config generator.

### The server is up but the world feels frozen, or nobody can enter

Do not put more maps into one game core. Each channel deliberately runs **three**
cores splitting the maps between them. Serving all 41 maps from a single core
pushes its main loop past the 50 ms window the client allows during the initial
handshake, and then **no client can connect at all** — while the login server
keeps answering perfectly, which makes it look like a protocol bug. If you have
edited the map lists, put them back.

### The disk is filling up

The server writes a lot of logs — roughly **40 MB per hour per game core**.

```sh
docker compose exec game m2ctl logsize
docker compose exec game m2ctl prune-logs 2      # keep 2 days
```

Lower `M2_LOG_KEEP_DAYS` in `.env` to make the server itself keep fewer.

### I forgot the admin panel password

```sh
docker compose exec panel rm /usr/local/etc/m2panel.conf
docker compose restart panel
docker compose logs panel | grep -A4 "ADMIN PANEL PASSWORD"
```

Or set `M2_PANEL_PASSWORD` in `.env` first, and it will use that instead.

### The panel says the server is offline when it is not

The panel checks whether the game is up by looking at the ports named in
`M2_PANEL_STATUS_PORTS`. If you changed `M2_AUTH_PORT`, update that too.

### The rates page says the helper is missing, or nothing happens when I save

The panel and the game reach each other through a small shared directory. Check
that both containers really have it:

```sh
docker compose exec panel ls -la /opt/m2spool
docker compose exec game  ls -la /opt/m2spool
```

Both must show a directory owned by group `m2spool`. If one of them does not,
the stack was started with an older `docker-compose.yml` — `docker compose up -d`
with the current one creates the volume and mounts it in both.

What the game container made of the last change:

```sh
docker compose exec game cat /opt/m2spool/rates.status
docker compose exec game cat /var/log/m2rates.log
```

If the status says `unsupported`, the game image has no server-files profile at
`/opt/metin2/rates/pack.sh` — rebuild it after running `prepare-context.sh`.

### The build fails

First check that there is anything to build. If `game/src` or
`mariadb/initdb.d/dumps` is missing or empty, step 2 was skipped — this
repository does not contain the game:

```sh
ls game/src mariadb/initdb.d/dumps panel/app
../fetch-sources.sh status         # what has been acquired so far
../fetch-sources.sh                # do whatever is still missing
```

Otherwise: the build needs internet access (it downloads the MariaDB connector
and Ubuntu packages) and about 8 GB of free disk.

```sh
df -h /var/lib/docker      # free space
docker compose build game  # run it again; the output says what failed
```

---

## Adding more channels

One channel handles a few hundred players and uses about 1 GB of RAM. To run
more, change **two** settings in `.env` together:

```ini
M2_CHANNELS=2
M2_GAME_PORT_RANGE=13000-13012
```

Then `docker compose up -d`. Changing only the first one leaves the extra
channel running but unreachable from the internet.

Open the new ports on your provider's firewall too.

The client also has to be told, or players will see one channel when you are
running two. Rebuilding the client for it costs seconds and no download:

```sh
docker compose run --rm client-builder
```

---

## Changing the experience, drop and yang rates

Open the panel, go to **Server rates**, type three percentages, press save. The
game server restarts by itself and is back in well under a minute; players are
disconnected for that moment and can log straight back in.

- `100` everywhere means exactly the game as it shipped.
- The numbers are always worked out from the **original** tables, so setting
  200% and then 300% gives you 300% — not 600% — and going back to 100% gives
  you the shipped files back byte for byte.
- The page tells you where the change is: *being applied* while the server is
  restarting, *live* when it is done, and it says so plainly if something went
  wrong.
- The rates survive everything: `docker compose restart`, `docker compose down
  && up`, an `.env` change, and a rebuilt image. They are stored on the game's
  state volume and re-applied before the cores start.

If you want to watch it happen:

```sh
docker compose logs -f game            # the restart, live
docker compose exec game cat /var/log/m2rates.log
```

How it works is described under [Server rates across two
containers](#server-rates-across-two-containers).

---

## What the panel can and cannot do here

All five per-character actions work — but two of them need the character to be
**in game**, and there is no way around that:

| Action | Character in game | Character offline |
|---|---|---|
| Give items | delivered in game, immediately | written to the database; appears at next login |
| Give yang | delivered in game, immediately | written to the database; appears at next login |
| Set level | delivered in game, immediately | written to the database; appears at next login |
| **Teleport** | works | **refused, and the panel says why** |
| **Running speed** | works | **refused, and the panel says why** |

The in-game route is a quest, `files/web_admin.quest`, which polls
`player.web_admin_queue` every five seconds and carries the command out on the
live character. **The build installs it**: `prepare-context.sh` stages it and
stage 2b of `game/Dockerfile` compiles it with a `qc` built from the same source
tree as the cores. The `mysql_direct_query` binding it calls is in the port
patch — it is the one thing in that patch which is not part of the port, and the
patch's header says so.

Teleport and running speed cannot be faked in the database. The map a coordinate
belongs to is not stored anywhere the panel could look up, so writing raw x/y
could drop a character into empty space; a speed buff is a temporary in-memory
effect with nothing to write at all. For an offline character the panel
therefore refuses those two, out loud, instead of queueing something nobody will
execute.

To check the helper is alive without a game client, put a row in the queue by
hand and watch it move — the procedure is in
[files/ADD_SQL_BINDING.md](../../files/ADD_SQL_BINDING.md) section 4. A row that
is still `pending` a minute later means the helper is not running.

---

## Serving the panel on port 80

```ini
M2_PANEL_PUBLIC_PORT=80
```

Then `docker compose up -d`, and the panel is at `http://YOUR-IP` with no port.

For HTTPS, put a reverse proxy (Caddy or nginx) in front of it. The panel
already honours `X-Forwarded-Proto`, `X-Forwarded-Host` and `X-Forwarded-For`
from a local proxy.

---

# For the technically curious

Everything below is background. You do not need it to run a server.

## What the services are

Three that run, and three that do not: `client-builder`, `updater` and
`wsbridge` are each in a compose profile and are started only on purpose.

```
                    internet
                       │
        ┌──────────────┼───────────────┐
        │              │               │
     :11000        :13000-13002      :7788
      auth          channel cores     panel
        └──────────────┬───────────────┘
                       │
        ┌──────────────┴───────────────┐
        │      game container          │      panel container
        │  ┌────┐ ┌──────┐ ┌────────┐  │      ┌─────────────┐
        │  │ db │ │ auth │ │ 3 game │  │      │ Flask +     │
        │  │core│ │      │ │ cores  │  │      │ waitress    │
        │  └──┬─┘ └───┬──┘ └────┬───┘  │      └──────┬──────┘
        └─────┼───────┼─────────┼──────┘             │
              └───────┴─────────┴───────┬────────────┘
                                        │
                                  ┌─────┴──────┐
                                  │  MariaDB   │  (not published)
                                  └────────────┘
```

### The fourth service, which is not a service

`client-builder` has a compose **profile**, so `docker compose up` never starts
it. It exists to be run on purpose:

```sh
docker compose run --rm client-builder
```

It is a one-shot tool with no ports, no dependencies on the rest of the stack,
and nothing running while the server is. It mounts exactly two volumes: its own
cache, and the panel's data volume — and it writes into the second one only
once, at the very end.

That separation is the whole design. The download, the unpacked tree and the
repack all happen on the **cache** volume, at `/usr/local/m2panel` — which
inside *this* container is a symlink to the cache, because
`pack_prepare_client()` writes to that path and takes no argument. Only when the
finished zip has passed its checks is it copied to the panel's volume under a
dotted name and renamed into place. Until that rename the panel sees no
`client.zip`, and goes on telling players the download is not ready, which is
the truth. There is no window in which it serves half a file.

### Why all the game cores share one container

They are not independent services:

- The **db core** listens on `127.0.0.1:15000` only, and every other core dials
  exactly that during boot and gives up if nothing answers.
- Each core must run with its **working directory set to its own folder** — it
  reads `CONFIG` from there and writes every log file there.
- They reach the two binaries and the 115 MB data tree through symlinks into one
  shared layout.

Splitting them into five containers would mean five containers sharing one
writable volume plus an external orchestrator to sequence their startup — for no
isolation benefit, since they already trust each other completely.

Inside the container a small bash supervisor (`m2-supervise`) starts them in
order, waits for each one's port to open, restarts one that dies, and on
shutdown stops them in **reverse** order so the game cores flush their players
to the db core while it is still running.

### Image layout

The game image is built in three stages:

1. **deps** — Ubuntu 24.04 with i386 multiarch, running the project's own
   `build-deps-40250.sh` to produce the 32-bit dependency tree (Crypto++ 8.4.0,
   DevIL 1.8.0, MariaDB Connector/C 3.3.10, OpenSSL, zlib, libmd, and the forked
   Lua 5.0.3). Depends only on that script and the shipped tarballs, so editing
   the server source never rebuilds it.
2. **builder** — compiles the eight modules and links them. Produces exactly two
   files: `game` and `db`. Asserts both are 32-bit i386 ELF and that
   `_TIME_BITS=64` did not leak in (it would silently change the size of packed
   network and database structures).
3. **runtime** — Ubuntu 24.04 plus the three i386 shared libraries the binaries
   actually need (`libc6`, `libstdc++6`, `libgcc-s1` — everything else is
   statically linked), the two binaries, and the static data tree.

The runtime image runs as an unprivileged user; the entrypoint is root only long
enough to set ownership on the state volume.

## Volumes, and why each one exists

| Volume | Mounted at | Holds | Why |
|---|---|---|---|
| `db-data` | `/var/lib/mysql` | **all** accounts, characters, items, guilds | There is no player data on disk anywhere else. This is the backup target. |
| `game-var` | `/opt/metin2/var` | guild emblems, server logs, crash dumps, rendered configs | Guild emblems are the only player-created content outside the database. |
| `panel-conf` | `/usr/local/etc` | `m2panel.conf` | Admin password hash, per-install salt, Flask session secret. Generated once; regenerating it would log everyone out. |
| `panel-data` | `/usr/local/m2panel` | `client.zip`, `downloads.db` | The client is ~1.2 GB and is built (or uploaded) by the operator; the download quota must survive restarts. |
| `rates-spool` | `/opt/m2spool` (both) | one request file, one status file | The only thing the panel and the game share. See [Server rates](#server-rates-across-two-containers). |
| `client-cache` | `/var/cache/m2client` | the downloaded server-file archive, the built client, the build log | ~3 GB that can all be fetched again — but fetching it again means another 1.4 GB through somebody else's MEGA share. Only `client-builder` mounts it, and only while you are running it. |

The static game data (maps, item and mob tables, quests, translations) is **in
the image**, not a volume — so rebuilding the image actually updates it, instead
of a stale volume silently shadowing the new content.

Configuration files are re-rendered from the environment on **every** start.
They are derived, not operator state, so changing `.env` and running
`docker compose up -d` is all it takes; nothing needs migrating.

## The addressing model

Three addresses that are the same string on a bare-metal server are three
different values in a container:

| Role | What it is here |
|---|---|
| **listen** | `0.0.0.0` — bind every interface (`M2_BIND_IP`) |
| **public** | the container's own address — this core's identity for core-to-core traffic, auto-detected |
| **advertised** | `M2_PUBLIC_ADDRESS` — what clients are told to connect to |

The stack sets the first and the third explicitly. The second is auto-detected
and resolves to the container's address, which is what core-to-core traffic
uses and what Docker forwards published ports to — so it is already correct and
is deliberately left alone.

The generated configs therefore carry:

```
BIND_IP: 0.0.0.0                        # listen everywhere
P2P_ALLOW_IP: 127.0.0.1 172.18.0.4      # the container's own address
PROXY_IP: <M2_PUBLIC_ADDRESS>           # what clients are told
```

`P2P_ALLOW_IP` is there because the core-to-core port-security check otherwise
accepts a connection only from the core's own public address. That happens to
hold here — all the cores share one container — but writing the allowlist means
an operator who pins `M2_PUBLIC_IP` to the VPS address does not silently break
core-to-core traffic.

`PROXY_IP` accepts a hostname as well as an address; the core resolves it at
startup and logs what it resolved to.

## Ports

| Purpose | Port | Published |
|---|---|---|
| auth (login) | 11000 | **yes** |
| channel 1 cores | 13000, 13001, 13002 | **yes** |
| admin panel | 7788 | **yes** |
| WebSocket bridge | 7789 | only when playing in the browser is switched on — and on 127.0.0.1 alone when the panel has a domain, because nginx reaches it there and serves it on 443 under `/to/` and `/ping` |
| p2p between cores | 12000, 14000–14002 | no |
| db core | 15000 | no — loopback inside the container only |
| MariaDB | 3306 | no — compose network only |

The db core speaks an unauthenticated protocol: anything that can reach port
15000 can rewrite any character on the server. It binds loopback and is never
published.

## Server rates across two containers

The panel's rates page has to do two things it cannot do from inside its own
container: rewrite the game's data tables, and restart the game's cores. On
FreeBSD, where the panel and the game share a machine, it simply runs a script
that does both. Here the shell-out becomes a request and an answer across one
shared directory.

```
panel container                             game container
---------------                             --------------
someone presses "save"
  |
  v
player.web_admin_rates  (the wanted values)
  |
  v
/usr/local/bin/apply_rates.sh
  writes /opt/m2spool/request   ------->    m2-supervise's watch loop, every 5s:
  writes /opt/m2spool/rates.status              m2-rates poll     new request?
        state=running                             |
                                                  v
                                              m2-rates apply
                                                rewrites mob_proto.txt and the
                                                three drop tables, always from
                                                an untouched *.m2orig baseline
                                                  |
                                                  v
                                              stop the cores in reverse order,
                                              start them again in boot order
                                                  |
  the page reads   <---------------------     m2-rates publish
  /opt/m2spool/rates.status                     state=ok
```

Points worth knowing:

- **No Docker socket, no SSH, no extra port.** Mounting the Docker socket into
  the one public-facing web application in the stack would trade a text-file
  edit for the ability to run anything on the host. The spool can carry exactly
  one thing: three numbers between 1 and 10000.
- The spool is shared through a group, `m2spool`, created with **gid 2050 in
  both images** and containing both service accounts. The directory is `2770`,
  so files created in it belong to that group whichever container wrote them.
- The rate arithmetic is not reimplemented here. The game image carries the
  FreeBSD installer's own server-files profile at `/opt/metin2/rates/pack.sh`
  and calls its `pack_apply_rates()`, so both platforms scale the same columns
  of the same tables in the same way.
- `db/mob_proto.txt` is a symlink to `share/conf/mob_proto.txt`, so rewriting
  the one real file updates the db core and every game core at once. The
  profile follows symlinks rather than replacing them, deliberately.
- The tables live in the **image**, and a container's writable layer does not
  survive `docker compose up -d`. What the operator asked for is therefore kept
  on the game's state volume (`/opt/metin2/var/rates/wanted`) and re-applied
  before the cores start, every time the container boots. The baseline copies
  live in the same writable layer as the tables they belong to, so the two can
  never disagree: either both are there, or the tables are pristine and the
  baselines get made again from them.
- The restart goes through the supervisor, not around it: cores are stopped in
  reverse boot order so the channel cores flush their players to the db core
  while it is still up.
- If applying fails, the cores are **not** restarted — there would be nothing
  for them to pick up — and the page says so.

## Environment variable reference

Full annotated list with defaults: **`.env.example`**.

| Variable | Default | Purpose |
|---|---|---|
| `M2_PUBLIC_ADDRESS` | *(none)* | Address advertised to clients. IP or hostname. |
| `M2_DB_ROOT_PASSWORD` | *(required)* | MariaDB root password. |
| `M2_DB_PASSWORD` | *(required)* | Password for the game's SQL user. |
| `M2_DB_USER` | `metin2` | The game's SQL user. |
| `M2_PANEL_PASSWORD` | *(generated)* | Admin panel login. Generated and printed once if empty. |
| `M2_CHANNELS` | `1` | Channels to run (1–4). Three cores each. |
| `M2_GAME_PORT_RANGE` | `13000-13002` | Published channel ports. Must match `M2_CHANNELS`. |
| `M2_AUTH_PORT` | `11000` | Published login port. |
| `M2_PANEL_PUBLIC_PORT` | `7788` | Published panel port. |
| `M2_PANEL_BIND_ADDRESS` | `0.0.0.0` | Host address used only for the panel. Use `127.0.0.1` for local-only or reverse-proxy installs. |
| `M2_TZ` | `UTC` | Timezone; log directories are named by local date. |
| `M2_MAX_LEVEL` | `120` | Level cap. 120 is the highest the game can do. |
| `M2_LOG_KEEP_DAYS` | `3` | Server-side log retention. |
| `M2_HOST_BIND_ADDRESS` | `0.0.0.0` | Host address on which Docker publishes auth and game ports. |
| `M2_BIND_IP` | `0.0.0.0` | Address the cores listen on. `0.0.0.0` (the default) renders `BIND_IP: 0.0.0.0`; anything else renders `LISTEN_IP`. Either way it sets only the listen address, never what clients are told. |
| `M2_TEST_SERVER` | `0` | Test-server behaviour. |
| `M2_TABLE_POSTFIX` | *(empty)* | Suffix on every game table name. |
| `M2_MALL_URL` | *(empty)* | In-game shop link. |
| `M2_ADMINPAGE_PASSWORD` | *(empty)* | The server's built-in admin text page. |
| `M2_CORE_DUMPS` | `0` | Write core dumps on crash. |
| `M2_BRAND`, `M2_CLIENT_NAME`, `M2_CLIENT_URL` | *(empty)* | Panel branding and client link. |
| `M2_INVENTORY_SLOTS` | `45` | Inventory size the panel assumes. **The default is one page, and r40250 has four** — set this to `180`, or the panel will refuse to give items to a character whose first page is full. |
| `M2_PANEL_STATUS_PORTS` | `11000,13000` | Ports the panel probes for "is the game up". |
| `M2_MAKE_JOBS` | *(all cores)* | Build parallelism. |
| `M2_STRIP_BINARIES` | `1` | Strip debug symbols. `0` keeps them for gdb. |
| `M2_BASE_IMAGE` | `ubuntu:24.04` | Base image for build and runtime. |

The client builder — read only by `docker compose run --rm client-builder`, and
every one of them optional:

| Variable | Default | Purpose |
|---|---|---|
| `M2_CLIENT_ADDRESS` | `M2_PUBLIC_ADDRESS` | The address written into the client. `127.0.0.1` on a local install. |
| `M2_CLIENT_CHANNELS` | `M2_CHANNELS` | Channels listed in the client's server list. Must match what is running. |
| `M2_CLIENT_AUTH_PORT` | `M2_AUTH_PORT` | Login port written into the client. |
| `M2_CLIENT_SERVER_NAME` | *(the server files' own)* | The name shown in the server list. |
| `M2_CLIENT_ARCHIVE_URL` | *(the server files' own MEGA link)* | Where to fetch the archive. |
| `M2_CLIENT_ARCHIVE` | *(empty)* | Path **inside the container** to an archive to use instead. `./client-archive/` is the easier way. |
| `M2_CLIENT_ARCHIVE_SHA256` | *(empty)* | Checked before anything is built from the archive. |
| `M2_CLIENT_FORCE` | *(empty)* | `rebuild` re-unpacks from the cached archive; `redownload` also fetches it again. |
| `M2_CLIENT_KEEP_ARCHIVE` | `1` | `0` deletes the 1.4 GB archive once the client is built. |
| `M2_CLIENT_VERIFY` | `quick` | `full` re-checksums the cached archive on every run (minutes). |
| `M2_CLIENT_MIN_FREE_MB` | `7000` | Free disk insisted on before starting. |

No secret is baked into any image. Passwords come from `.env` at run time; the
panel's admin password is generated on first start if you did not choose one.

Three more are set by `docker-compose.yml` rather than by you, because they are
paths inside the containers and there is nothing to choose:
`M2PANEL_RATES_SCRIPT`, `M2PANEL_RATES_STATUS` and `M2PANEL_RATES_SPOOL` — the
panel's two rate settings, pointed at the shared spool.

## Staging the build context by hand

`prepare-context.sh` copies the source tree, the runtime data tree, the SQL
dumps and the panel into this directory so the build context is self-contained:

```sh
./prepare-context.sh --m2port /opt/m2port --panel ../../files
docker compose up -d --build
```

**Most operators never call this directly** — `../fetch-sources.sh` runs it for
you at the end, and that is what puts the staged `game/src/`, `panel/app/`,
`mariadb/initdb.d/dumps/`, `game/rates/pack.sh` and
`client-builder/pack/pack.sh` in place. Call `prepare-context.sh` yourself only
when those inputs already exist somewhere outside this directory, which in
practice means a development checkout.

There is no release archive to build here and there is not going to be one. What
this repository ships is the port: `../patches/0001-r40250-linux-port.patch`,
`../build-deps-40250.sh` and the scripts that assemble everything else on the
machine that will run it.
