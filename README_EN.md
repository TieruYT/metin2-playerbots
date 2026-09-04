# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | **English**

A local Metin2 singleplayer world populated by genuine, autonomous player characters (Playerbots): leveling up, grinding solo and in squads, looting items, refining gear at the Blacksmith, hunting Metin stones, and persisting their full progression in the standard database.

## 💬 Community & Project Support

- **[Join our Discord server](https://discord.gg/6v4WkDY6a)** — discuss the project, share playtests and ideas, and follow playerbot development updates.
- **[Support development on Zrzutka.pl](https://zrzutka.pl/rw4g7p)** — voluntary donations help cover the tools and AI models used to develop the project.

Every contribution — testing, bug reports, ideas, code, or financial support — helps us build a more autonomous and lively Metin2 world.

> [!IMPORTANT]
> This project supports the **native Windows client only**. It neither contains nor automatically downloads Metin2 files, the r40250 package, or the withdrawn upstream WebClient. Installation requires your own compatible files. See [project provenance and attribution](docs/ATTRIBUTION.md).

Unlike conventional external client-bot scripts, bots in this project are **first-class PC entities controlled by AI logic embedded directly inside the game core server engine**. Real players connecting to the server observe their natural movement, combat combos, skill casts, and equipment via standard Metin2 network packets.

---

## 🌟 Bot Capabilities

- ⚔️ **Smart Combat Engine**: Full support for all character classes (Warrior, Sura BM/WP, Ninja Dagger/Archer, Shaman), smooth combo animations, projectile-based bow attacks consuming arrows, active buff maintenance, and skill rotations.
- 🗺️ **Global 2D NavGrid (A*)**: High-resolution collision grid generated from engine map attributes (`server_attr`) coupled with Global A* and String-Pulling line-of-sight trajectory smoothing.
- 🚪 **Multi-map Travel**: Autonomous portal traversal across M1 ↔ M2 ↔ M3 and the internal teleports of the Easy Monkey Dungeon. Bots choose zones according to level and equipment readiness.
- 💎 **Metin Stone Hunting**: Dedicated roving Metin hunters patrolling the map, shattering stones, and clearing spawned add waves.
- 🎒 **Looting & Town Economy**: Post-battle loot collection, automated equipment evaluation including shields, appropriate merchant visits, potion restocking, and gear refinement at the **Blacksmith**.
- 🐴 **Horse Progression**: Real Horse Medal expeditions into the Monkey Dungeon, delivery to the nearest Stable Boy, and mounted long-distance travel.
- 👥 **Party & Squad Dynamics**: Dynamic 2–3 player squad formations, cooperative exping, and pulling mobs in dense monster camps.
- 🏪 **Stalls and a bot-to-bot market**: Bots open private stalls in Bokjung and **buy from each other** — the refine material someone is short of, or a piece of gear better than what they are wearing. Anything at +7 or above is never sold to an NPC merchant.
- 🧬 **Biologist missions**: Collecting specimens and handing them in stage by stage, driven without a quest dialog.
- 🎣 **Fishing**: A full session — bait written into the rod's socket, waiting for the bite, and pulling inside the six-second window.
- ✨ **Bonus rerolling**: Bots use Change and Add Attribute stones on gear they are not currently wearing.
- 🧠 **Personality and goals**: Every bot has its own character and ambition (Metin hunter, collector, horse breeder) that decides what it does with a given hour.
- 💾 **Native MariaDB Persistence**: Each bot has its own persistent account and character entry in MariaDB, retaining Level, EXP, Yang, items, and quest flags across server restarts.

---

## 📊 Project Status

The project is under active research and development.

> [!NOTE]
> **Supported Kingdom:** The autonomous world currently covers **Chunjo**: Joan (M1, map 21), Bokjung (M2, map 23), Waryong/M3 (map 24), the Easy Monkey Dungeon (map 25, levels 18–26), **Orc Valley** (map 64), and the **Yongbi Desert** (map 63). More Chunjo regions and the other kingdoms (*Shinsoo – Reds* and *Jinno – Blues*) remain future work.
>
> In Orc Valley the bots use the main contiguous landmass. The bridge-linked islands stay out of reach: bridge decks are a client-side object and are absent from the server collision layer the bots route on.

### Resource Footprint (measured with 742 bots alive)
- **Game Engine (`game core`)**: ~1.76 GiB RAM, ~32% of one core
- **Database (`MariaDB`)**: ~89 MiB RAM
- **Web Admin Panel & Live Map**: ~270 MiB RAM
- Runs smoothly in the background on standard modern developer PCs.
- `PLAYERBOT_AUTOSPAWN_COUNT` asks for a number, but the ceiling is how many canonical identities exist in the database (`BOT_COUNT` in `generate_seed.py`), not the slider.

---

## 🚀 Quickstart

### 1. Prepare the files

Have a compatible local r40250 server archive ready. A native Windows client archive, preferably from the exact same release, is optional if you already have a configured client. The “r40250” label alone does not guarantee matching packets and proto files. Neither file belongs in this repository.

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
Point the client from the same compatible r40250 set to `127.0.0.1` (Auth port `11000`, Game ports `13000–13002`) and jump into the living world in Joan (Chunjo)!

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
- [x] **Phase 2A**: M1/M2/M3 traversal, level-based zones, the Easy Monkey Dungeon, and real Horse Medal expeditions.
- [ ] **Phase 2B**: Orc Valley ✅ and Yongbi Desert ✅ — full **Demon Tower (DT)** runs still to come.
- [ ] **Phase 3**: Reading Skill Books (KU) and Soul Stones (KD), advanced skill priority trees.
- [x] **Phase 4A**: Early Biologist missions and first-stage horse progression backed by real Horse Medal drops.
- [ ] **Phase 4B** *(up next)*: Orc Tooth and later Biologist missions plus Combat and Military Horse progression. Bots are to keep the same cooldowns a player has and use the potion that shortens the wait, with the same success chance the quests use.
- [ ] **Phase 5**: Fishing ✅ — mining ore veins and alchemy crafting still to come.
- [x] **Phase 6**: Private bot stalls in town and bot-to-bot trading. Pricing is still fixed-tier rather than demand-driven.

---

## 📚 Project Documentation

Detailed guides separated into dedicated documentation modules:

- 📖 **[Installation & Setup Guide (docs/INSTALL_EN.md)](docs/INSTALL_EN.md)** – Docker, WSL2, Linux, `.env`, client connection.
- 💻 **[Developer Guide & Diagnostics (docs/DEVELOPMENT_EN.md)](docs/DEVELOPMENT_EN.md)** – 8-second fast C++ builds (`fast-game-build`), debugging, and telemetry logs.
- 🧩 **[Architecture and refactoring path (docs/ARCHITECTURE.md)](docs/ARCHITECTURE.md)** – module boundaries and the incremental path toward testable AI components.
- 🧾 **[Provenance & Attribution (docs/ATTRIBUTION.md)](docs/ATTRIBUTION.md)** – fork history, licensing boundary, and WebClient status.

---

## 🤝 Credits & Acknowledgements

- **AzzlackSyndicate** — author of the original Linux port foundation, installers, and panel. The source repository is now private; its Git history and attribution are retained.
- **OskarPWA** — the bot depot window and the skill icons on the site come from a panel he built and shared for merging back.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Research inspiration for autonomous MMO agent design.
- The Metin2 emulation and research community.
