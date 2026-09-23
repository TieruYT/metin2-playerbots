# 🛠️ Installation & Setup Guide

[Polski (INSTALL.md)](INSTALL.md) | **English**

Step-by-step setup and configuration guide for the local Metin2 server with the Playerbots AI system.

> [!IMPORTANT]
> **Two lines, two ways to install.** The current **2.x** line (the mt2009 server) ships as the
> full package `Metin2-Singleplayer-<version>.zip` — the client and the server with its launcher —
> and is installed as the section "The 2.x line (full package)" below describes. The rest of this
> document — `installer/install.ps1`, `installer/install.sh`, the `/opt/metin2/stack` directory,
> the BYOF archives — is the **1.x** line (r40250). The 1.x installer cannot set up the 2.x line.

---

## 📋 System Requirements

| Requirement | Specification |
|---|---|
| **Operating System** | Windows 10/11 (with Docker Desktop + WSL2) or Linux (Ubuntu / Debian with Docker) |
| **CPU Architecture** | x86-64 (Intel / AMD). Game core compiles as a 32-bit x86 ELF. |
| **RAM** | Minimum 8 GB (16 GB+ recommended for 350+ bots) |
| **Disk Space** | ~25–40 GB for containers, databases, and build contexts |
| **Game Client** | Any standard client compatible with r40250 server files (e.g. classic `Metin2Client` v1.0.28249.1) |

---

## 📦 Required external files (BYOF)

The repository contains Playerbots, patches, installers, and Docker packaging,
but no game code or data. For a clean installation, prepare:

- a compatible r40250 server archive containing `metin2_server+src.tar.gz` and `metin2_mysql_dump.zip`, or an unpacked `[40250] Reference Serverfile` directory;
- optionally, a compatible native Windows client archive. You may instead use your own client that is already configured.

The current baseline `metin2_server+src.tar.gz` against which the port is made
has this SHA-256:

```text
6e9e7339935058f73fead81e609219b496adbc867dfeca70f633031730313001
```

Check it in PowerShell with
`Get-FileHash .\metin2_server+src.tar.gz -Algorithm SHA256`. The 2025-03-31
TMP4 refresh (`e72d7881...`) contains changed sources and must not be forced
through the baseline patch. The installer recognises that variant and, if its
dry run fails, exports a safe report to
`C:\Metin2Server\diagnostics\compatibility-report.txt` for issue #5.

“An r40250 client” does not mean every download carrying that label. Its packet
protocol, `item_proto`/`mob_proto`, and locale must match the server. A client
from the same release as the supplied server files is the most reliable pair.

The WebClient is not part of this fork and is always disabled by the installer.
Do not add game archives to Git; see [NOTICE.md](../NOTICE.md) and
[ATTRIBUTION.md](ATTRIBUTION.md).

---

## 📦 The 2.x line (full package)

### Windows

Unpack the whole package into one folder, run `Serwer\Metin2-Launcher-GUI.bat`, then
**ZAINSTALUJ / PRZYGOTUJ** and **GRAJ**. The first start builds the server (a dozen minutes or
so); later starts take moments. Updates: the **ZAINSTALUJ AKTUALIZACJE** button.

### Linux / VPS

You need Docker with the compose plugin, and `python3` or `curl` + `unzip` + `sha256sum`.

1. Unpack the `Serwer` folder of the full package into a new, empty directory, e.g.
   `/opt/metin2/serwer`.
2. Create `.env` from the example:
   ```sh
   cd /opt/metin2/serwer
   cp linux-port/docker/.env.example linux-port/docker/.env
   ```
   and set:
   - `M2_DB_ROOT_PASSWORD` and `M2_DB_PASSWORD` — passwords of your own, random ones
     (`openssl rand -hex 16`); compose will not start without them,
   - `M2_PUBLIC_ADDRESS` — the server's public IP address or domain; without it clients hang at
     "connecting to server",
   - `M2_PANEL_BIND_ADDRESS=127.0.0.1` — on the 2.x line neither panel asks for a passphrase by
     default, so do not publish them. Open them through an SSH tunnel:
     ```sh
     ssh -L 7788:127.0.0.1:7788 -L 7790:127.0.0.1:7790 user@your-server
     ```
     and browse to `http://127.0.0.1:7788` (classic panel) and `http://127.0.0.1:7790`
     (advanced panel).
3. In the same folder run:
   ```sh
   sh linux-port/tools/update.sh
   ```
   It fetches the newest version published on GitHub, checks its SHA-256, stages the panel's
   build context (`linux-port/docker/panel/app` and `panel/schema` from `files/`) and runs
   `docker compose up -d --build`. Later updates are the same command;
   `sh linux-port/tools/update.sh check` only compares versions.
   If it answers that the version is already the newest and the build stops at `COPY schema/`,
   stage the panel's context alone and build:
   ```sh
   sh linux-port/tools/update.sh stage
   cd linux-port/docker && docker compose up -d --build
   ```
4. Open TCP `11000` (login) and `13000-13002` (channel 1; `13000-13012` with the second channel)
   in the firewall.

Do not run the repository's `installer/install.sh` or `m2-updater` on the 2.x line — they are the
1.x line's tools and refuse a 2.x folder.

---

## 🚀 Installation of the 1.x line (r40250, from the repository)

### 1. Windows 10 / 11 (Recommended)

1. Install **Docker Desktop** and enable the **WSL2 backend**.
2. Start Docker Desktop and wait until the status indicates **Engine running**.
3. Clone the repository and run the PowerShell installer with your own files:

```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1 `
    -Archive 'C:\Metin2Files\Reference_Server.zip' `
    -ClientArchive 'C:\Metin2Files\Reference_Client.zip' `
    -NoWebClient
```

If you already use a configured client, install the server without preparing a
new one:

```powershell
& .\installer\install.ps1 `
    -Archive 'C:\Metin2Files\Reference_Server.zip' `
    -NoClient -NoWebClient
```

An unpacked baseline can be passed as
`-ReferenceDir 'C:\path\[40250] Reference Serverfile'`. After the first
assembly, the source is cached in a Docker volume, so updates do not need the
original local archive again.

The installer will automatically:
- Set up the Docker Compose stack.
- Build the game core image with integrated Playerbots support.
- Bind all services safely to `127.0.0.1` (local loopback).

---

### 2. Linux (Ubuntu / Debian)

```sh
git clone https://github.com/TieruYT/metin2-playerbots.git
cd metin2-playerbots
sudo sh ./installer/install.sh --local \
  --archive '/path/Reference_Server.zip' \
  --no-client --no-web-client
```

The `--local` flag ensures services are strictly bound to `127.0.0.1` without opening public firewall ports.

---

## ⚙️ Configuration (.env)

After installation, core settings are stored in
`%USERPROFILE%\Metin2Server\.env` on Windows or `/opt/metin2/stack/.env` on Linux.

Key parameters:
```ini
# Number of bots spawned automatically on server startup
PLAYERBOT_AUTOSPAWN_COUNT=350

# Optional: minimum bot count that must already exist in a persistent world
# (0 disables the check; set it after first startup or restoring a backup)
PLAYERBOT_EXPECT_MIN_EXISTING_BOTS=0

# Login Authentication port
M2_AUTH_PORT=11000

# Channel 1 Game core port range
M2_GAME_PORT_RANGE=13000-13002

# Web Admin Panel port (number only; its bind address is a separate setting)
M2_PANEL_PUBLIC_PORT=7788

# Max character level
M2_MAX_LEVEL=120

# Default language for server-sent monster/item/quest text
M2_DEFAULT_GAME_LANGUAGE=en
```

For an established world, set `PLAYERBOT_EXPECT_MIN_EXISTING_BOTS` to a safe
minimum. Startup then stops if Docker points at another daemon or a fresh
volume containing fewer bots. Keep `0` for a first installation.

After modifying `.env`, restart the game container:
```powershell
Set-Location .\linux-port\docker
docker compose up -d --force-recreate game
```

---

## 🎮 Connecting Your Game Client

1. Open `serverinfo.py` or your launcher configuration and set the IP address to `127.0.0.1`.
2. Auth port: `11000`.
3. Channel ports: `13000`, `13001`, `13002`.
4. Launch `Metin2Distribute.exe`.
5. Create a character and start playing alongside autonomous bots in Joan (Chunjo)!

---

## 🔄 Daily Operation & Commands

Run these commands in the installed stack directory (by default
`%USERPROFILE%\Metin2Server` on Windows):

```powershell
# Start server containers in background
docker compose up -d

# Check container status
docker compose ps

# Follow live server logs
docker compose logs -f game

# Safe shutdown (preserves all character and database state)
docker compose stop
```

> [!WARNING]
> Never run `docker compose down -v` unless you intentionally wish to erase all character data and databases!
