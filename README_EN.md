# Metin2 Playerbots

[Polski (README.md)](README.md) | **English**

A local, hobbyist Metin2 singleplayer world where autonomous player characters genuinely play on the server: gaining experience, fighting solo and in squads, looting items, upgrading gear, visiting NPCs, and persisting their full progression in the standard game database alongside real players.

This project extends the classic r40250 server files with a native, server-side **Playerbots** architecture. A bot is not a monster dummy nor an external client-clicking script. It is an authentic PC entity controlled by AI logic embedded directly inside the game core. Consequently, real players observe their movement, combat animations, equipment, and progression via standard game network protocol packets.

> [!IMPORTANT]
> The primary way to play and observe the world is the **native Metin2 Windows Client**. The web administration panel is designed solely for management, live world monitoring, and diagnostics. The browser-based client is an optional, experimental add-on — neither the server nor bot AI depends on it.

---

## Project Status

This is an active research and hobby prototype. The current testing focuses primarily on the M1 ecosystem (Joan / Chunjo), transit to M2, and the Monkey Dungeon. The main priority is believable, organic behavior for hundreds of independent characters, rather than scripting every single in-game quest.

> [!NOTE]
> **Supported Kingdom:** The current version of the project has been implemented and tested exclusively for the **Chunjo Kingdom (Yellows / Map 21 – Joan)**, including the dedicated 2D NavGrid collision matrix, Metin spawn clusters, and 32 regional hunting hubs. The other kingdoms (**Shinsoo / Reds – Map 1 Yongan** and **Jinno / Blues – Map 41 Pyungmoo**) have not yet been implemented and are planned for future roadmap phases.

| Component | Status | Notes |
|---|---|---|
| Bot Core & Character Persistence | Operational | Authentic PC entities; saves Level, EXP, Yang, Equipment, Inventory items, and Quest flags into standard MariaDB tables. |
| Navigation & 2D NavGrid Collision | Operational | 2D NavGrid directly loaded from engine map collision attributes (`server_attr`), Global A* pathfinding with String-Pulling line-of-sight smoothing. |
| Melee Combat & Target Selection | Operational | Attack combos, AoE splash, target distribution to avoid clustering, risk evaluation, and squad coordination. |
| Ninja Dagger/Archer & Skill Animations | Operational | Basic attacks, daggers, bow & arrows, projectiles, and offensive/buff skills render correctly in the native client. |
| Loot, Inventory & NPC Economy | Operational | Post-combat loot sweep, equipment evaluation, town visits to sell junk, buy potions/arrows, and refine gear at the Blacksmith. |
| Classes, Stat Distribution & Skills | Operational | Job selection, automatic stat building (VIT/INT/STR/DEX), and class-tailored skill usage. |
| Biologist & Horse Progression | Early Implementation | Initial Biologist research missions, Monkey Dungeon horse medals, and stableboy horse training. |
| M1, M2 & Monkey Dungeon | Integration Testing | Multi-map pathfinding and zone transit; portal transitions under active testing. |
| Admin Panel & Live Map | Operational | Live interactive world map, real-time status telemetry, equipment/stat inspection, rankings, and GM controls. |
| Browser-based Client | Optional / Experimental | Standalone WebAssembly client with WebSocket bridge; disabled by default. |

### Resource Consumption Snapshot
The bot population is fully configurable. The baseline testing scenario uses **350 active bots**. On the test system (**AMD Ryzen 7 9700X, 32 GB RAM, NVIDIA GeForce RTX 4070 Ti**), the runtime stack uses approximately **1.60 GiB RAM**:
- `game core`: ~1.05 GiB
- `mariadb`: ~154 MiB
- `panel (Flask)`: ~383 MiB
- `wsbridge`: ~27 MiB
- Total WSL2 working set: ~5.85 GiB (including Docker daemon, build caches, and container overlays).
- Native Windows Game Client: ~158 MiB.

*Note: The headless game core executes entirely on the CPU; GPU performance only affects the active graphical client.*

---

## Current Bot Capabilities

### Character Life & Persistence
- Automatically creates persistent characters of diverse classes and genders, spawning in proper starting territories.
- Earns levels, EXP, and Yang; survives server restarts and seamlessly reloads state from MariaDB.
- Automatically revives after death, heals up with potions before re-engaging, and retreats when HP falls into critical thresholds.
- Allocates status points and skill points suited for their specific class build.
- Operates solo or forms dynamic 2–3 player squads with squad formations and joint focus fire.

### Combat & Skills
- Evaluates target selection based on level differential, danger rating, engagement status by other bots, and squad objectives.
- Executes full attack motion combos, area-of-effect cleaves, offensive skills, and supportive buffs.
- Utilizes class-appropriate weaponry (swords, two-handed weapons, daggers, bows with arrow consumption, bells, fans).
- Maintains active buffs (Aura of the Sword, Enchanted Blade, Flame Spirit, Berserk) without spamming them prematurely.
- Identifies and prioritizes **Metin Stones** across the map, clearing spawned add waves and shattering the stone.

### Loot, Gear & Town Economy
- Conducts a thorough post-battle loot sweep, collecting Yang and dropped items belonging to the character.
- Compares looted weapons and armor against currently equipped gear, equipping upgrades automatically.
- Stores valuable backup equipment and shares usable items with bots of matching classes.
- Triggers **Town Visits** when inventory fills up or potion supply is depleted:
  - Sells accumulated junk to the General Store Merchant (Handlarka).
  - Restocks red and blue potions and arrows.
  - Visits the **Blacksmith (Kowal)** to upgrade equipment using standard Yang costs and refinement probabilities.
  - Takes brief, randomized pauses at NPCs to maintain an organic, living-world appearance.

### Global 2D Grid Navigation & World Traversal
- Ingests server map collision matrices (`server_attr` and `SECTREE::IsAttr`) into a 2-meter resolution NavGrid.
- Computes shortest 8-directional paths using **Global A\***, smoothing trajectories into corner waypoints via line-of-sight ray tracing (String Pulling).
- Navigates through city gates, narrow mountain corridors, bridges, and open valleys.
- Features multi-role distribution (Dedicated Metin Hunters, Squad Fighters, Area Mob Grinders across 32 regional hubs in all 8 quadrants).

---

## Architecture

```text
┌──────────────────────────┐          Standard Metin2 TCP Protocol
│ Native Windows Client    │ ─────────────────────────────────┐
└──────────────────────────┘                                  │
                                                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Docker `game` Container                                                 │
│                                                                         │
│  auth core          channel/game cores          db core                 │
│                         │                          │                    │
│                         ▼                          │                    │
│           PlayerBotManager + AI State Engine       │                    │
│              │          │          │               │                    │
│              ▼          ▼          ▼               │                    │
│           CHARACTER   ITEM   PARTY/QUEST/SHOP/REFINE                    │
│              │                                     │                    │
│              └──────── Standard Persistence ───────┘                    │
└─────────────────────────────────────┬───────────────────────────────────┘
                                      │
                                      ▼
                             ┌────────────────┐
                             │ MariaDB        │
                             │ accounts/chars │
                             │ items/progress │
                             └────────┬───────┘
                                      │ Administrative Read-Only
                                      ▼
                             ┌────────────────┐
                             │ Flask Panel    │
                             │ Live Map / GM  │
                             └────────────────┘

Optional: Web Browser ── WebSocket ── wsbridge ── TCP ── game
```

### Key Architectural Principle
An autonomous Playerbot **does not run a separate headless game client nor open an external TCP socket**. All AI decision-making runs directly inside the game server loop. Results are dispatched to connected observing human players via standard movement, combat, and state packets.

### Core Components

| Component | Location | Role |
|---|---|---|
| PlayerBot AI Engine | `linux-port/overlays/playerbot/src/game/src/playerbot_manager.cpp` | AI state machine, combat, target selection, inventory, NPC visits, and A* NavGrid pathfinding. |
| Game Core Integration | `linux-port/overlays/playerbot/patches/0001-core-integration.patch` | PC entity hooks, virtual descriptors, GM bot management commands, and lifecycle callbacks. |
| Web Panel & Live Map | `files/admin_panel.py` | Full administration dashboard, real-time map visualization, telemetry, player rankings, and GM toolset. |
| Docker Compose Stack | `linux-port/docker/docker-compose.yml` | MariaDB, game core, web panel, and optional toolchain services. |
| Automated Installers | `installer/` | One-click setup scripts for Windows (`install.ps1`) and Linux (`install.sh`). |
| Incremental Fast Build | `tools/fast-game-build/` | 8-second C/C++ compiler toolchain directly building binary changes into Docker. |

---

## Requirements

| Requirement | Details |
|---|---|
| **CPU Architecture** | x86-64 (Intel/AMD). The Metin2 game core compiles as a 32-bit x86 ELF. ARM is not supported. |
| **Operating System** | Windows 10/11 with Docker Desktop & WSL2 backend, or Linux (Ubuntu 22.04/24.04, Debian 12/13). |
| **RAM** | Minimum 8 GB. Recommended 16–32 GB for 350+ bot populations and active development. |
| **Disk Space** | ~40 GB free space for source context, Docker images, database, and client assets. |
| **Client** | Legally obtained, compatible Metin2 r40250 Windows client files. |

---

## Installation & Setup

### Windows 10/11 (Recommended Local Setup)

1. Install **Docker Desktop** and enable the **WSL2 backend**.
2. Start Docker Desktop and wait until the status shows **Engine running**.
3. Clone the repository and execute the PowerShell installer:

```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1
```

The installer builds the complete Docker environment and binds auth, game channels, and the web panel strictly to `127.0.0.1` (local loopback), ensuring offline security without opening firewall ports.

If you have pre-existing server source archives or reference directories:
```powershell
$env:M2_SRC_ARCHIVE = 'D:\Metin2\serverfiles.zip'
& .\installer\install.ps1
```

### Linux (Offline / Local)

```sh
git clone https://github.com/TieruYT/metin2-playerbots.git
cd metin2-playerbots
sudo sh ./installer/install.sh --local
```

---

## Daily Operation & Server Management

### Start Server
```powershell
Set-Location .\linux-port\docker
docker compose up -d
docker compose ps
```

### View Live Logs
```powershell
docker compose logs -f game
docker compose logs -f panel
docker compose logs -f mariadb
```

### Safe Shutdown (Preserving Database & Progress)
```powershell
docker compose stop
```
> [!CAUTION]
> Do NOT use `docker compose down -v` unless you intentionally want to delete all character progress and MariaDB databases!

---

## Windows Client Connection

1. Prepare your compatible r40250 Windows game client.
2. Configure `serverinfo.py` or your launcher configuration to connect to `127.0.0.1` with Auth port `11000` and Channel 1 ports (`13000–13002`).
3. Launch the client once `game`, `mariadb`, and `panel` show healthy in `docker compose ps`.

*Reference Client*: The project is tested with `Client/Metin2Distribute.exe` from the standard r40250 client (`Metin2Client` file version `1.0.28249.1`). The repository does not distribute game client binaries or pack assets.

---

## Web Admin Panel & Live Map

Access the local administration dashboard in your browser:

👉 **`http://127.0.0.1:7788`**

Features provided by the web panel:
- **Live World Map**: Real-time rendering of all bot coordinates, destinations, and active targets.
- **Bot Filtering**: Filter by solo grinders, party members, Metin hunters, and level brackets.
- **Character Inspector**: View real-time inventory slots, equipped gear, status attributes, and skill points.
- **Leaderboards**: Top Level, Yang, Equipment, Horse levels, Biologist progress, and monster kill counts.
- **Server Controls**: Adjust EXP, Drop, and Yang multipliers on the fly, manage GM rights, and execute bot commands.

---

## In-Game Commands (GM Commands)

Playerbots can also be managed directly from the in-game chat interface (requires GM / Administrator permissions):

| Command | Permission | Description | Example Usage |
|---|---|---|---|
| `/bot_spawn <player_id> <empire: 1-3>` | GM | Spawns a specific bot by Player ID into the designated empire (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <player_id>` | GM | Disconnects and despawns a specific bot from the world. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count: 1-500> <empire: 1-3>` | GM | Spawns a batch of bots starting from `start_id` up to `count` into the designated empire. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count: 1-500>` | GM | Despawns a batch of bots starting from `start_id`. | `/bot_despawn_many 4 350` |
| `/bot_rank` | All Players | Displays the current level leaderboard and coordinates of active bots in the chat window. | `/bot_rank` |

---

## Fast C/C++ Development Workflow

For rapid development of AI logic in `playerbot_manager.cpp`:

1. **Initial Setup (One-time)**:
```sh
bash tools/fast-game-build/setup.sh
```

2. **Incremental Compilation (Takes ~8 seconds)**:
```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp
docker restart metin2-game
```

---

## Project Roadmap

### Phase 1: M1 Stabilization & Core Polish
- [x] Full animation compatibility for all classes (Warrior, Sura BM/WP, Ninja Dagger/Archer, Shaman).
- [x] Arrow projectile trajectory and bow attack timing.
- [x] 2D NavGrid collision matrix with Global A* and String-Pulling path simplification.
- [x] Natural Blacksmith refinement cycle: unequip item -> refine attempt series -> equip best result.
- [x] Deterministic role assignment (Metin Hunters, Party Squads, 32-Hub Regional Grinders).

### Phase 2: Autonomous Player Ecosystem & Market
- [ ] Personality profiles: aggressive levelers, traders, Metin hunters, dungeon runners, socializers.
- [ ] Private Player Shops (Kaszmirowy Tobołek): bots setting up offline market stalls in town.
- [ ] Dynamic pricing engine based on item rarity, upgrade plus level, bonus stats, supply, and demand.
- [ ] Inter-bot trading and item lending within parties.

### Phase 3: World Progression & Demon Tower (DT)
- [ ] Zone transit to M2 (Bakra / Bokjung / Yayang), Orc Valley (Seungryong), Desert, and Spiders Dungeon.
- [ ] Full Demon Tower (Wieża Demonów) progression: entrance, floor mechanics, seal unlocking, boss clears, and DT Blacksmith upgrades.
- [ ] Level-bracket progression up to max server level.

### Phase 4: Skills, Skill Books & Soul Stones
- [ ] Comprehensive skill trees with proper targeting and priority rotations.
- [ ] Reading Skill Books (Księgi Umiejętności) and Soul Stones (Kamienie Duchowe) with realistic cooldowns.
- [ ] Advanced stat and skill builds tailored for PvP and PvE archetypes.

### Phase 5: Quests, Biologist & Horse Evolution
- [ ] Biologist quest line progression (Orc Teeth, Curse Books, Demon Keepsakes, etc.) with cooldown handling.
- [ ] Full horse training cycle: Beginner Horse -> Combat Horse (Bojowiec) -> Military Horse (Militarny).
- [ ] Mounted combat mechanics and mounted vs on-foot tactical decision making.

### Phase 6: Non-Combat Activities & Life Skills
- [ ] Fishing system: purchasing fishing rods, baiting hooks, fishing minigame, and campfire cooking.
- [ ] Mining system: locating ore veins, pickaxe upgrades, mining extraction, and alchemist refining.
- [ ] Gathering and crafting economy.

---

## Security, Privacy & Secret Management

- For local singleplayer play, always bind all server services strictly to `127.0.0.1`.
- Never expose MariaDB database ports or internal DB core ports to external networks.
- Do not commit `.env` files, production database dumps, GM account credentials, or private keys.
- Always store database backups outside the git repository tree.

---

## Legal Notice & Scope

**Metin2, trademarks, logos, game client assets, pack archives, textures, 3D models, audio, and original game server files are the intellectual property of their respective copyright holders, notably Ymir Interactive / Webzen. This project is not affiliated with, endorsed by, or associated with Webzen.**

This repository contains exclusively original Playerbots code, integration patches, configuration templates, utility tooling, and technical documentation authored by the project contributors. It does not distribute, host, or license:
- Proprietary Metin2 client files, pack archives, or graphical assets.
- Pre-compiled proprietary server binaries or original copyrighted source code of r40250.
- Database dumps containing proprietary game secrets or user account data.

Users are solely responsible for providing legally obtained, compatible game files and ensuring compliance with applicable copyright laws and software regulations in their jurisdiction. This project is intended strictly for private research, education, and offline singleplayer hobbyist study.

---

## Credits

- [AzzlackSyndicate/metin2-singleplayer-serverfiles-linux](https://github.com/AzzlackSyndicate/metin2-singleplayer-serverfiles-linux) — Upstream Linux port, Docker stack, and base management panel.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Research inspiration for autonomous MMO player agent design.
- The Metin2 reverse-engineering and emulation research community.
