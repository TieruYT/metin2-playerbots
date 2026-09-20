# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | **English**

[![Website](https://img.shields.io/badge/Website-metin2singleplayer.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://metin2singleplayer.com)
[![Discord](https://img.shields.io/badge/Discord-Join_Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/pt5tvnrN6)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Support_the_Project-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)

A local Metin2 singleplayer world populated by genuine, autonomous player characters (Playerbots): leveling up, grinding solo and in squads, looting items, refining gear at the Blacksmith, hunting Metin stones, and persisting their full progression in the standard database.

## 💬 Community & Project Support

- **[Project website — metin2singleplayer.com](https://metin2singleplayer.com)** — what the project is, the roadmap, the install guide and the FAQ, in Polish and English.
- **[Join our Discord server](https://discord.gg/pt5tvnrN6)** — discuss the project, share playtests and ideas, and follow playerbot development updates.
- **[Support development on buycoffee.to](https://buycoffee.to/metin2-playerbots)** — voluntary donations help cover the tools and AI models used to develop the project.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Support on buycoffee.to"></a>

Every contribution — testing, bug reports, ideas, code, or financial support — helps us build a more autonomous and lively Metin2 world.

> [!IMPORTANT]
> This project supports the **native Windows client only**. It neither contains nor automatically downloads Metin2 files, the r40250 package, or the withdrawn upstream WebClient. Installation requires your own compatible files. See [project provenance and attribution](docs/ATTRIBUTION.md).

Unlike conventional external client-bot scripts, bots in this project are **first-class PC entities controlled by AI logic embedded directly inside the game core server engine**. Real players connecting to the server observe their natural movement, combat combos, skill casts, and equipment via standard Metin2 network packets.

---

## 🌟 Bot Capabilities

- ⚔️ **Smart Combat Engine**: Full support for all character classes (Warrior, Sura BM/WP, Ninja Dagger/Archer, Shaman), smooth combo animations, projectile-based bow attacks consuming arrows, active buff maintenance, and skill rotations.
- 🗺️ **Global 2D NavGrid (A*)**: High-resolution collision grid generated from engine map attributes (`server_attr`) coupled with Global A* and String-Pulling line-of-sight trajectory smoothing.
- 🚪 **Multi-map Travel**: Autonomous crossings M1 ↔ M2 ↔ M3, the Easy Monkey Dungeon, Orc Valley, the Yongbi Desert, Mount Sohan and the Spider Dungeon V1. Bots pick the map by level and the hub on it by the population's shared memory of where the monsters stand. When they run out of potions far from town, a bot with a Teleport Ring recalls to its village at once instead of walking to the exit portal.
- 🏹 **Biologist missions and level-appropriate hunting**: seven Biologist missions including the Orc Tooth and the Soul Stone, taken without a dialog; ordinary hunting picks the frontier map and hub by the bot's level. (On the 2.x files the `levelup` quest ships in `_unused`, so the "Hunting: Lv X" missions are disabled — they never showed progress and sent under-geared bots at their monster.)
- 🎁 **Chests and bosses**: Moonlight Treasure Chests and boss caskets are opened and their contents used (bonus scrolls, speed potions, the Blessing Scroll for refines from +6). Parties, guild mates first, raid the Orc Chief and the Spider Queen.
- 💎 **Metin Stone Hunting**: Dedicated roving Metin hunters patrolling the map, shattering stones, and clearing spawned add waves.
- 🎒 **Looting & Town Economy**: Post-battle loot collection, automated equipment evaluation including shields, buying armour from the merchant (and, when the exact tier is not stocked, the best available for the class) and refining it to at least +6 at the **Blacksmith**, appropriate merchant visits, potion restocking, and a tidied bag (potions on the first lines, then boosters and chests).
- 🐴 **Horse Progression**: Real Horse Medal expeditions into the Monkey Dungeon, delivery to the nearest Stable Boy, and mounted long-distance travel.
- 👥 **Party & Squad Dynamics**: Dynamic 2–3 player squads, and up to 8 in Orc Valley for the Black Orc camps; cooperative exping and pulling mobs in dense monster camps.
- 🏪 **Stalls and a bot-to-bot market**: Bots open private stalls in Bokjung and **buy from each other** — the refine material someone is short of, or a piece of gear better than what they are wearing. Anything at +7 or above is never sold to an NPC merchant. A worse duplicate of a better piece (two of the same +9) is put up for sale.
- 🐉 **ItemShop and Dragon Coins**: the built-in ItemShop runs on Dragon Coins, and SM vouchers drop with a small chance from broken Metin stones and slain bosses.
- 🧬 **Biologist missions**: Collecting specimens and handing them in stage by stage, driven without a quest dialog.
- 🎣 **Fishing**: A full session — bait written into the rod's socket, waiting for the bite, and pulling inside the six-second window.
- ✨ **Bonus rerolling**: Bots use Change and Add Attribute stones on gear they are not currently wearing.
- 🧠 **Personality and goals**: Every bot has its own character and ambition (Metin hunter, collector, horse breeder, a "dropper" of Metins, M3, M2 or medals) that decides what it does with a given hour.
- 🎛️ **The panel steers behaviour live**: goal weight sliders, a switch for the bots' overhead chat, scrap keepers selling cheap fodder for the blacksmith, and the Moonlight chest chance — all read by the core within five seconds, no restart. On the single-player line the panels grant full access at once, with no passphrase to type.
- 💾 **Native MariaDB Persistence**: Each bot has its own persistent account and character entry in MariaDB, retaining Level, EXP, Yang, items, and quest flags across server restarts.

---

## 📊 Project Status

The project is under active research and development.

> [!NOTE]
> **Supported Kingdoms:** The autonomous world covers **all three kingdoms** (**Chunjo** – Yellows, **Shinsoo** – Reds, and **Jinno** – Blues) including M1, M2, M3, Monkey Dungeons, **Orc Valley**, **Yongbi Desert**, **Mount Sohan**, **Hwang Temple**, and **Spider Dungeons**!

### Resource Footprint (measured with 843 bots alive)
- **Game Engine (`game core`)**: ~1.8 GiB RAM, ~25–38% of one core (a route planner on its own grid, with a memory of planned routes)
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

After the start two web panels are running: the classic admin panel at `http://127.0.0.1:7788` and the **Metin2 Singleplayer Panel** by seban latino at `http://127.0.0.1:7790` — a live map of the bot world, character profiles with inventories and item tooltips, rankings, economy history, host telemetry, live bot controls and bulk item grants. On the single-player line both panels open with full access and no passphrase; an operator who exposes the panel can still turn a password on.

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
- [x] **Phase 4B**: the Orc Tooth and the Soul Stone at the Biologist (with the quest's own chance of a spoiled tooth), hunting missions to level 55, the battle horse from the desert trial.
- [ ] **Phase 4C** *(up next)*: later Biologist missions, three kingdoms and kingdom wars.
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
- **seban latino** — author of the Metin2 Singleplayer Panel (`linux-port/docker/seban-panel`), the second panel in this install: live map, profiles, rankings, economy, telemetry and bulk grants.
- **Iwakura** — help with the price and shop name systems, the bots' nicknames and the item value algorithms.
- **ĹŌŞƬĒĶ** — the client's new login screen (since 2.0.6): the animated background, the logo and Discord Rich Presence.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Research inspiration for autonomous MMO agent design.
- The Metin2 emulation and research community.
