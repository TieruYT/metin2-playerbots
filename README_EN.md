# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | **English**

A local Metin2 singleplayer world populated by genuine, autonomous player characters (Playerbots): leveling up, grinding solo and in squads, looting items, refining gear at the Blacksmith, hunting Metin stones, and persisting their full progression in the standard database.

> [!IMPORTANT]
> This project supports the **native Windows client only**. It neither contains nor automatically downloads Metin2 files, the r40250 package, or the withdrawn upstream WebClient. Installation requires your own compatible files. See [project provenance and attribution](docs/ATTRIBUTION.md).

Unlike conventional external client-bot scripts, bots in this project are **first-class PC entities controlled by AI logic embedded directly inside the game core server engine**. Real players connecting to the server observe their natural movement, combat combos, skill casts, and equipment via standard Metin2 network packets.

---

## 🌟 Bot Capabilities

- ⚔️ **Smart Combat Engine**: Full support for all character classes (Warrior, Sura BM/WP, Ninja Dagger/Archer, Shaman), smooth combo animations, projectile-based bow attacks consuming arrows, active buff maintenance, and skill rotations.
- 🗺️ **Global 2D NavGrid (A*)**: High-resolution collision grid generated from engine map attributes (`server_attr`) coupled with Global A* and String-Pulling line-of-sight trajectory smoothing.
- 💎 **Metin Stone Hunting**: Dedicated roving Metin hunters patrolling the map, shattering stones, and clearing spawned add waves.
- 🎒 **Looting & Town Economy**: Post-battle loot collection, automated equipment evaluation and upgrades, routine town visits to sell junk inventory, restock potions, and refine gear at the **Blacksmith**.
- 👥 **Party & Squad Dynamics**: Dynamic 2–3 player squad formations, cooperative exping, and pulling mobs in dense monster camps.
- 💾 **Native MariaDB Persistence**: Each bot has its own persistent account and character entry in MariaDB, retaining Level, EXP, Yang, items, and quest flags across server restarts.

---

## 📊 Project Status

The project is under active research and development.

> [!NOTE]
> **Supported Kingdom:** The current version features full 2D NavGrid collision navigation, 32 regional hunting hubs, and Metin clusters implemented specifically for the **Chunjo Kingdom (Yellows / Map 21 – Joan)**. Support for other kingdoms (*Shinsoo – Reds* and *Jinno – Blues*) is planned for future development phases.

### Resource Footprint (350 Bot Snapshot)
- **Game Engine (`game core`)**: ~1.05 GiB RAM
- **Database (`MariaDB`)**: ~154 MiB RAM
- **Web Admin Panel & Live Map**: ~383 MiB RAM
- Runs smoothly in the background on standard modern developer PCs.

---

## 🚀 Quickstart

### 1. Prepare the files

Have a compatible local r40250 server archive ready. A native Windows client archive is optional if you already have a configured client. Neither file belongs in this repository.

### 2. Clone and Install (Windows)
```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1 `
    -Archive 'C:\path\Reference_Server.zip' `
    -ClientArchive 'C:\path\Reference_Client.zip' `
    -NoWebClient
```

If you already have a configured client, replace `-ClientArchive` with `-NoClient`.

### 3. Start the Server after installation
```powershell
Set-Location "$env:USERPROFILE\Metin2Server"
docker compose up -d
```

### 4. Join the Game
Point any standard r40250-compatible game client to `127.0.0.1` (Auth port `11000`, Game ports `13000–13002`) and jump into the living world in Joan (Chunjo)!

👉 **Detailed installation, `.env` options, and client setup instructions: [docs/INSTALL_EN.md](docs/INSTALL_EN.md)**

---

## 🎮 In-Game Commands (GM Commands)

Manage bots directly via in-game chat (requires GM / Administrator permissions):

| Command | Permission | Description | Example |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | GM | Spawns a specific bot by Player ID into the designated empire (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | GM | Despawns and logs out a specific bot. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | GM | Spawns a batch range of bots into the designated empire. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | GM | Despawns a batch range of bots starting from `start_id`. | `/bot_despawn_many 4 350` |
| `/bot_rank` | All Players | Prints the top level leaderboard and bot coordinates in chat. | `/bot_rank` |

---

## 🗺️ Roadmap

- [x] **Phase 1**: Full combat animations for all classes, Archer projectiles, 2D NavGrid (A*), Blacksmith refinement cycle, and 32 regional hubs in Chunjo.
- [ ] **Phase 2**: Multi-map zone traversal (M2, Orc Valley, Desert) and full **Demon Tower (DT)** runs.
- [ ] **Phase 3**: Reading Skill Books (KU) and Soul Stones (KD), advanced skill priority trees.
- [ ] **Phase 4**: Biologist quest progression and complete horse training cycle (Beginner, Combat, Military).
- [ ] **Phase 5**: Non-combat life skills: Fishing minigame, mining ore veins, and alchemy crafting.
- [ ] **Phase 6**: Private offline player shops in town centers, inter-bot trading, and dynamic market pricing.

---

## 📚 Project Documentation

Detailed guides separated into dedicated documentation modules:

- 📖 **[Installation & Setup Guide (docs/INSTALL_EN.md)](docs/INSTALL_EN.md)** – Docker, WSL2, Linux, `.env`, client connection.
- 💻 **[Developer Guide & Diagnostics (docs/DEVELOPMENT_EN.md)](docs/DEVELOPMENT_EN.md)** – 8-second fast C++ builds (`fast-game-build`), debugging, and telemetry logs.
- 🧾 **[Provenance & Attribution (docs/ATTRIBUTION.md)](docs/ATTRIBUTION.md)** – fork history, licensing boundary, and WebClient status.

---

## 🤝 Credits & Acknowledgements

- **AzzlackSyndicate** — author of the original Linux port foundation, installers, and panel. The source repository is now private; its Git history and attribution are retained.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Research inspiration for autonomous MMO agent design.
- The Metin2 emulation and research community.
