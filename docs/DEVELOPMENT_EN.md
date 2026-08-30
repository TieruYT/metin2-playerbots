# 💻 Developer Guide & Diagnostics

[Polski (DEVELOPMENT.md)](DEVELOPMENT.md) | **English**

Technical guide covering codebase layout, incremental compilation, and runtime AI telemetry diagnostics for the Metin2 Playerbots project.

---

## 📁 Codebase Layout & Overlay

All custom Playerbots logic is cleanly isolated in `linux-port/overlays/playerbot/`:

```text
linux-port/overlays/playerbot/
├── src/game/src/
│   ├── playerbot_manager.cpp    # Core AI engine, 2D NavGrid, combat, shopping, refining
│   ├── playerbot_manager.h      # Class declarations and data structures
│   └── playerbot_world_rules.h  # Pure, testable cross-map travel rules
├── sql/
│   └── playerbots_seed.sql      # MariaDB seed dataset for 350 persistent characters
├── serverfiles/
│   └── mob_drop_item.m3.append.txt # Additive M3 progression drops
├── tools/
│   └── generate_seed.py         # Cohort generator (classes, baseline gear, names)
└── patches/
    ├── 0001-core-integration.patch  # Core integration patch with Metin2 server engine
    └── 0002-economy-yang-x5.patch   # Economy multiplier balance patch
```

---

## ⚡ Fast C++ Incremental Compilation (~8 seconds!)

To accelerate development on `playerbot_manager.cpp`, a standalone persistent builder container is provided in `tools/fast-game-build/`. It compiles only modified C/C++ files without rebuilding the entire Docker image stack.

### 1. One-time Toolchain Setup
```sh
bash tools/fast-game-build/setup.sh
```

### 2. Compile Modified C++ Code
```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp
docker restart metin2-game
```

The script compiles the 32-bit ELF `game` binary in ~8 seconds and automatically updates the `metin2/game:40250` Docker image.

Keep the builder container after the first full compile. Later manager changes
can copy changed headers as well, for example:

```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp playerbot_world_rules.h
```

Pure rules have tests which do not require a running server. Their current
scope and the incremental split are documented in
[ARCHITECTURE.md](ARCHITECTURE.md).

---

## 🔨 Full Stack Rebuild

If you modified Dockerfiles, system dependencies, quests, or database structures, trigger a full rebuild:

```powershell
Set-Location .\linux-port\docker
docker compose build game
docker compose up -d --force-recreate game
docker compose logs -f game
```

---

## 🔍 Logs & AI Diagnostics

Engine server logs are located inside the `game` container at `/opt/metin2/var/channel1/game1/syslog`.

### Useful Telemetry Commands:

```sh
# Follow real-time Playerbot AI decision stream
docker exec metin2-game tail -f /opt/metin2/var/channel1/game1/syslog | grep "PLAYERBOT_AI"

# Verify 2D NavGrid collision map loading
docker exec metin2-game grep "PLAYERBOT_NAVGRID" /opt/metin2/var/channel1/game1/syslog

# Inspect target acquisition queries
docker exec metin2-game grep "target acquired" /opt/metin2/var/channel1/game1/syslog

# Inspect error logs (syserr)
docker exec metin2-game cat /opt/metin2/var/channel1/game1/syserr
```

---

## 🖥️ Web Panel (Live Map & Admin)

The administrative panel source is in `files/admin_panel.py`.

Rebuild and recreate panel container after changes:
```powershell
Set-Location .\linux-port\docker
docker compose build panel
docker compose up -d --force-recreate panel
```

Access the panel at: `http://127.0.0.1:7788`.

---

## 💾 Database Backup and Horse Diagnostics

Before a major AI change, stop state-writing services and create a verified compressed backup of the complete database:

```sh
docker compose stop game panel
./backup_database.sh /safe/path/database-all.sql.gz
docker compose start
```

The helper neither prints nor stores the database password; it reads the credential only inside `metin2-db` and validates the resulting gzip archive.

Run the complete read-only Horse Medal journey report with:

```sh
./diagnose_horse_journeys.sh
```

The report covers bot distribution across M1/M2/M3 and the Monkey Dungeon, carried medals, horse levels, and persisted medal pickup/delivery telemetry.
