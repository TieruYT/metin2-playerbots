# Set up your own Metin2 server

One command. Ten to sixty minutes, mostly waiting. You do not need to know
Linux, Docker, MySQL or anything else — the script does all of it and tells you
what it is doing.

At the end you get a running Playerbots server, an **admin panel** link and its
password. You connect with a compatible native Windows client.

> **One thing before you start.** Game files are not part of this project and
> are not downloaded automatically. Have your own compatible r40250 server
> archive ready. The upstream WebClient was withdrawn and is not supported by
> this fork; use a native Windows client. See [docs/INSTALL_EN.md](docs/INSTALL_EN.md).

---

## First: which of these are you?

**"I just want to play on my own PC."**
→ [Windows](#windows--playing-on-your-own-pc). Nothing gets opened to the
internet. Nobody else can join, and nothing about your PC becomes reachable that
was not before.

**"I want a server my friends can join."**
→ [Linux server](#linux--a-server-others-can-join). You need to rent a small
virtual server. About 5 € a month.

You can do the Windows one first to see the game running, and move to a real
server later. Nothing is wasted.

---

## Windows — playing on your own PC

### What you need

- Windows 10 or 11, 64-bit, on an Intel or AMD processor (not ARM)
- **At least 15 GB free** on your system drive — the script stops below that,
  and 25 GB is comfortable. Docker keeps its images in a virtual disk on the
  system drive no matter which folder you install into.
- 8 GB of memory is comfortable; 4 GB works but the build will be slow
- Docker Desktop. If you do not have it, the script installs it for you — but
  Docker Desktop wants a restart after it is first installed, so you may have to
  run the command twice. That is normal.

### The command

Open **PowerShell** (press Start, type `powershell`, press Enter) and paste:

```powershell
$env:M2_SRC_ARCHIVE = 'C:\path\Reference_Server.zip'
irm https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1 | iex
```

That is it. Now read the next section while it works.

### What it is doing, in plain words

1. Checks your PC has enough memory and disk space, and stops early with a clear
   explanation if it does not.
2. Installs Docker Desktop if it is missing.
3. Assembles the server from your local archive, builds it, and starts it.
4. Uses your native client archive when supplied, or leaves client setup to you.

### Important: this server is for you alone

Everything listens on `127.0.0.1`, which means *this computer and nothing else*.

- **Nobody else can join.** Not friends over the internet, not someone on the
  same Wi-Fi.
- **No port was opened.** No firewall rule was created.
- **Your home IP address was not given out.**

The script says all of this at the end too, because it matters.

If you later want friends to play: **do not open ports on your home router.**
That hands your home address to every player, your upload speed becomes the
bottleneck, and the server disappears whenever your PC does. Rent a small Linux
server instead and follow the section below.

---

## Linux — a server others can join

### What you need

- A virtual server (a "VPS") with **Debian 12/13 or Ubuntu 22.04/24.04**
- **4 GB of memory** and about **40 GB of disk**. The script stops if fewer than
  15 GB are free: 8 GB go on building the server and another 7 GB on preparing
  the client.
- An **x86 (Intel/AMD) machine — not ARM.** The game server is 32-bit x86 code
  from 2014 and cannot run on ARM. The script checks this and stops with an
  explanation rather than failing confusingly halfway through.
- Root access (you get this by default on a fresh VPS)

Around 5 € a month at Hetzner, Netcup or Contabo. Any provider works.

### The command

Connect to your server over SSH, then paste:

```sh
curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | \
  sudo sh -s -- --archive /path/Reference_Server.zip --no-client
```

### With your own domain name (recommended)

If you own a domain, point it at your server's IP address first, then run:

```sh
curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sudo sh -s -- \
    --archive /path/Reference_Server.zip --no-client \
    --domain panel.yourdomain.com --email you@example.com
```

This gets a free certificate and your admin panel runs over **HTTPS** instead of
plain HTTP. Worth doing: without it, your panel password travels across the
internet unencrypted, and anyone in between can read it.

> **One thing to know about domains and the game.** If you use Cloudflare with
> the orange cloud switched on, that works for the panel but **not** for the game
> itself — Cloudflare's proxy only carries web traffic, and Metin2 is not web
> traffic. Your players connect to the server's plain IP address, which the
> script bakes into the client for you. If you want a hostname in the client too,
> add a second DNS record with the **grey cloud** (DNS only).

### What gets opened

Ports **11000** and **13000–13002** for the game, and **80/443** if you gave a
domain. The script handles `ufw`, `firewalld` and plain `iptables`, and if it
finds no firewall it tells you the ports may need opening in your provider's
control panel instead.

---

## What you get at the end

The script prints three things. Write them down.

**1. The game client download link** — give this to your players. They download
it, unpack it anywhere they like, and run `Metin2Distribute.exe`. No
installation. The server address is already inside; there is nothing to
configure. It even works from a USB stick.

> This link does **not** work immediately. The client is over a gigabyte and has
> to be downloaded and repacked, which takes 20–60 minutes depending on your
> server. Until then the page politely says the download is not ready. Nothing is
> broken.

**2. Your admin panel link** — where you run the server.

**3. Your admin panel password** — freshly generated for your server, different
every time. It is also stored in the stack's `.env` file, so you can look it up
again.

You can pick your own instead: on the admin page, just under the introduction,
there is an **Admin passphrase** card. Updating never changes it, and the
installer prints whichever one is current every time you run it — including one
you chose yourself.

> **Where the stack lives.** On Linux that is `/opt/metin2/stack`, readable only
> by root. On Windows it is `Metin2Server` in your user folder. Every `cd
> /opt/metin2/stack` below is that folder on Windows instead.

---

## Your first five minutes in the panel

Open the panel link, enter the password, and you are in.

- **Server rates** — how much experience, how many item drops and how much yang
  the whole server gives. `100%` is exactly how the game shipped. There are three
  one-tap settings, or type any number. Saving restarts the game for under a
  minute.
- **Players** — every character, with level, yang and which account it belongs
  to. Click one to give items or yang, set the level, or make them a game
  master.
- **Game language** — the language the server speaks: quest text, system
  messages, item and monster names. Fifteen of them ship with the server files.
  Changing it restarts the game for well under a minute. The client is the other
  half: new downloads are built to match, and anyone who already has the game
  renames one file — the panel shows which one.
- **Password reset link** — when a player forgets their password, type their
  username here and send them the link it makes. It works once and expires after
  24 hours.

The same page also offers **Teleport**, **Running speed** and **Game master**.
The first two need the player to be logged in at the time; the panel says so
when they are not. See *The honest limits* below.

Almost every button and field has an explanation when you hover the mouse over
it, in **English, German and Turkish**.

Two accounts already exist for testing: `admin` and `test`, both with the
password `123456789`. **Change or delete them before anyone else can reach your
server** — those passwords are published in the original server files, so
everybody knows them, and nothing in this installation changes them for you.

---

## When something goes wrong

**"Players cannot connect."** Almost always the provider's own firewall, which
sits outside your server and which the script cannot reach. Check your provider's
control panel for ports 11000 and 13000–13002. Second most likely: the client is
pointed at the wrong address — rebuild it with the right one (see below).

**"The download link does not work."** It is probably still building. On the
server:

```sh
tail -f /opt/metin2/stack/client-build.log
```

**"I moved the server to a new address."** Rebuild the client with the new one —
this takes about a minute, it does **not** download the gigabyte again:

```sh
cd /opt/metin2/stack
M2_CLIENT_ADDRESS=<new address> docker compose run --rm client-builder
```

**"I want to start over."** Careful — this deletes all characters and accounts:

```sh
cd /opt/metin2/stack && docker compose down -v
```

Without the `-v` it only stops the server and keeps everything.

**Everyday commands:**

```sh
cd /opt/metin2/stack
docker compose ps                  # what is running
docker compose logs game --tail 50 # what the game server is saying
docker compose restart game        # restart just the game
docker compose down                # stop everything (data is kept)
docker compose up -d               # start it again
```

Running the installer a second time is safe. It keeps your data, your passwords
and your settings, and tells you what it found.

---

## The honest limits

This is a hobby project and it is written to be honest with you rather than to
sell you anything. So:

- **Game files are deliberately not distributed.** Supply a compatible r40250
  server baseline and native client that you are authorised to use. The
  installer does not depend on a project-owned mirror.
- **The game server is 2014 code.** It works, and it has been ported to Linux
  carefully with real tests, but it is old software with old assumptions. It is
  32-bit and it will not run on ARM.
- **Teleport and running speed need the player to be in the game.** The panel
  has buttons for both. They work by leaving a request in the database for a
  small script running *inside the game* to pick up, and that script is part of
  the server now. But neither one can be done to somebody who is logged out:
  there is nowhere to write "is now standing over there" that the game would
  believe, and a speed buff only exists while the character does. So if the
  player is offline the panel tells you so instead of pretending. The technical
  detail is in [files/ADD_SQL_BINDING.md](files/ADD_SQL_BINDING.md).
- **Taking a game master rank away needs a logout.** Giving one works there and
  then, even mid-game. Removing one does not: the game re-reads its list of game
  masters and re-applies the ranks in it, and somebody who has just been taken
  out is no longer in that list to be visited. They keep the commands until they
  log out and back in.
- **Items, yang and levels take a moment.** These go straight into the database,
  which the game only reads when a character is loaded — so the player has to
  log out and back in before they appear. The panel says so rather than
  pretending it was instant.
- **Nobody is maintaining this for you.** If you run a server that people rely
  on, you are the one who has to look after it.

---

## What next

- [README.md](README.md) — what this project is and how it is put together
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to change something and test it safely
- [linux-port/docker/README.md](linux-port/docker/README.md) — the compose stack
  in full: every service, every volume, every setting
- [linux-port/VPS-DEPLOYMENT.md](linux-port/VPS-DEPLOYMENT.md) — a real
  deployment written up in full, including everything that went wrong
- [files/README.md](files/README.md) — the admin panel's own settings
