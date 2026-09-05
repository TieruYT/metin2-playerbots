# metin2-suite

Single-player Metin2 server suite: a Linux port of the r40250 server, a launcher,
a Flask admin panel, and the playerbot AI that populates the world.

## Layout

The engine source is **not in this repository** and never will be — see
`linux-port/fetch-sources.sh`. Our side of the server is one overlay:

| Path | What |
|---|---|
| `linux-port/overlays/playerbot/src/game/src/` | The playerbot AI, split into implementation fragments (see below). `playerbot_manager.cpp` is the tick and whatever has not been lifted out yet. |
| `linux-port/overlays/playerbot/src/game/src/playerbot_world_rules.h` | Pure travel policy, no engine types. Unit-tested. The model for extracting logic. |
| `linux-port/patches/` | Patches applied to the pristine engine source. |
| `linux-port/docker/panel/app/admin_panel.py` | Flask admin panel. |
| `tests/playerbot_world_rules_test.cpp` | The only C++ unit test. |

Reference copies of the engine, needed to check any API before using it:

- `../m2src-cache/tree/port40250/server/` — staged, patched build tree (`game/src`, `common/`, `extern/include`)
- `../../backups/m1-best-*/game-server/game/src/` — engine sources incl. `fishing.cpp`, `char.cpp`
- `../m2src-cache/tree/server40250/share/` — runtime data: `conf/item_proto.txt`, `conf/item_names_pl.txt`, `conf/mob_names_pl.txt`, `locale/english/map/*/`

## Verifying a change

There is no local compiler; the build is 32-bit C++23 in Docker. Syntax-check the
overlay against the real headers — this catches almost everything and takes about
a minute:

```bash
cd ../m2src-cache/tree/port40250
cp <repo>/linux-port/overlays/playerbot/src/game/src/playerbot_manager.cpp server/game/src/
cp <repo>/linux-port/overlays/playerbot/src/game/src/playerbot_world_rules.h server/game/src/
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W)":/src -w /src/server/game/src gcc:13 bash -c \
  "apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq g++-multilib >/dev/null 2>&1; \
   g++ -fsyntax-only -m32 -std=c++23 -Wall -Wextra -Wno-unused-parameter -fexceptions \
   -D_THREAD_SAFE -DNDEBUG -DBOOST_BIND_GLOBAL_PLACEHOLDERS \
   -I../../../extern/include -I../../liblua/include -I../../libserverkey playerbot_manager.cpp; \
   echo EXIT=\$?"
```

`-m32` is mandatory: `packet.h` static-asserts a 32-bit `time_t` and the wire/DB
layouts depend on it. Restore the staged copies afterwards — they belong to the
build cache, not to us.

Unit test:

```bash
docker run --rm -v "$(pwd -W)":/w -w /w gcc:13 \
  bash -c "g++ -Wall -Wextra -o /tmp/t tests/playerbot_world_rules_test.cpp && /tmp/t"
```

Nothing here can be runtime-tested locally; the live signal comes from operators
running ~350 bots. Say plainly what was compiled versus what was observed.

### Adding a source file

Nothing to do. Drop a `playerbot_*.h` or `playerbot_*.cpp` into
`overlays/playerbot/src/game/src/` and every step finds it: `prepare-context.sh`
discovers the directory, the engine `Makefile` takes `$(wildcard
playerbot_*.cpp)`, the update packager expands `playerbot_*` in
`server-update-files.txt`, and the launcher copies the whole directory into the
build context before each build.

This used to cost five edits in `prepare-context.sh` alone plus a patch edit,
which is exactly why the manager grew to twelve thousand lines instead of being
split up -- and why 1.22.4 and 1.23.2 both shipped a manager without its own
headers and would not compile on any player's machine. If you add a step that
handles these files, make it discover them too. Never write another list.

## The playerbot sources

Everything shares **one translation unit and one anonymous namespace**, so
definition order *is* dependency order: a helper must appear above its callers.
The split is therefore a sequence of implementation fragments, included in
dependency order at the top of `playerbot_manager.cpp`:

| File | What |
|---|---|
| `playerbot_types.h` | Tuning constants, enums, `TPlayerBotAIState`, the state map, and the two goal/action transitions every subsystem makes. |
| `playerbot_log.h` | Saying something once for three hundred bots: a tag, a minute, and a count of what was swallowed. |
| `playerbot_config.h` | The weights an operator moves in the panel while the world runs. Re-read from a file every five seconds; neutral when it is missing. |
| `playerbot_world_rules.h` | Pure travel policy. No engine types, unit-tested. |
| `playerbot_navigation.h` | Where a bot may stand and whether two points connect. Calls nothing above it. |
| `playerbot_world_memory.h` | What the population has learned about the world, as opposed to about itself. |
| `playerbot_movement.h` | Following a route: mounts, waypoints, portals, and the known-metin registry. |
| `playerbot_gear.h` | What a bot wears and carries: equipment scoring, the progression ladder, arrows, potions. |
| `playerbot_activities.h` | The horse, and fishing. Each owns the whole tick while it runs. |
| `playerbot_missions.h` | The Biologist's collections and the level-up hunt, driven without a quest dialog. |
| `playerbot_skills.h` | The character sheet: stat points, the job's skill order, keeping buffs up. |
| `playerbot_combat.h` | How a swing or a cast is sent: the packets a bot has no client to generate. |
| `playerbot_economy.h` | Money and bag: junk, merchants, the blacksmith, market stalls. |
| `playerbot_bonus.h` | The bonus lines on worn gear: what a line is worth, what finishes an item, and what a bot will pay to change it. |
| `playerbot_travel.h` | Where a bot ought to be, and crossing between maps. |
| `playerbot_planner.h` | Which long-term goal wins: the candidates, their base priorities, and the three gates no weight can touch. |
| `playerbot_guild.h` | Founding and recruiting a guild, and who a bot has got on with. |
| `playerbot_town.h` | A town visit end to end, as a state machine that survives being interrupted. |
| `playerbot_loot.h` | Picking things up, in and out of a fight, without sweeping the floor. |
| `playerbot_survival.h` | Saving progress, breaking off a losing fight, and the walk back after dying. |
| `playerbot_wandering.h` | What a bot does on a hunting map when nothing is asking for its attention. |
| `playerbot_status.h` | What a bot shows above its head, and the words for it. |
| `playerbot_targeting.h` | Choosing what to hit and hitting it, including the claim that keeps hundreds of bots off the same monster. |
| `playerbot_manager.cpp` | Personality, party, upkeep, the watchdog - and `CPlayerBotManager` with the tick. |

These are fragments, not normal headers: each defines objects, relies on the
engine headers the manager includes above it, and reopens the same anonymous
namespace. Include each exactly once, from `playerbot_manager.cpp`, in an order
that respects what it calls. A fragment that needs something from a later
subsystem forward-declares it rather than pulling it in -- `playerbot_gear.h`
does this for `GetPlayerBotNpcApproach`.

`playerbot_manager.cpp` is now the manager: who a bot is, its party, the small
upkeep passes, the inactivity watchdog, and `CPlayerBotManager` itself. The tick
stays there on purpose - it is the one thing that has to see every subsystem, so
moving it would only mean declaring all of them somewhere else.

Find a subsystem by its entry point rather than by line number:

- Planning: `PlanPlayerBotLongTermGoal`, `GetPlayerBotWeight`, `RefreshPlayerBotWeights`
- Navigation/travel: `MovePlayerBot`, `TransitionPlayerBotMap` (an instant warp),
  `MovePlayerBotToWorldPortal` (walks), `ManagePlayerBotWorldTravel`
- Combat: `FindDistributedTarget`, `ExecutePlayerBotBasicAttack`,
  `ExecutePlayerBotAttackSkill`, `HandlePlayerBotMultiPull`
- Economy: `ManagePlayerBotEquipment`, `IsPlayerBotJunkItem`,
  `ManagePlayerBot*Merchant`, `ManagePlayerBotRefining`, `ManagePlayerBotPrivateShop`,
  `ManagePlayerBotBonusReroll`
- Activities: `ManagePlayerBotHorse`, `ManagePlayerBotFishing`,
  `ManagePlayerBotBiologist`, `HandlePlayerBotTownVisit`
- Planning/report: `PlanPlayerBotLongTermGoal`, `BuildPlayerBotStatusText`

`CPlayerBotManager::Update` is the tick. Order matters and is load-bearing:
releasing an open private shop runs first -- engine state must not depend on
which subsystem wins the tick -- then the goal planner, then the subsystem hooks
(each `continue`s to claim the tick), and target acquisition and attacking run
**last**. A subsystem that owns the tick therefore also suppresses combat and the
gear pass.

### The spawn ceiling is the registry, not the slider

`PLAYERBOT_AUTOSPAWN_COUNT` asks for a number; `CPlayerBotManager::LoadRegisteredBots`
decides what is available. It accepts a PID only when the ledger row says
`complete`/`adopted` **and** the account login is exactly `playerbot_NNN` for that
PID, the social id matches, and `player_index` holds that one character in empire 2.
Anything else -- a bot renamed by hand, a character from an older bootstrap that
used different account names -- stays in the database and never spawns. That guard
is deliberate: it is what stops a raw PID turning a real player's character into a
bot. Raising the ceiling on a world like that means growing the canonical cohort
(`BOT_COUNT` in `generate_seed.py`), not loosening the rule.

Read the two lines the core logs at startup before believing any count:

```
PLAYERBOT_AUTH: loaded 511 registered bot identities
PLAYERBOT: autospawn requested=750 registered_started=511 in Chunjo
```

### Traps this file has already sprung

- `TPlayerBotAIState` member init order must match declaration order, or `-Wreorder`
  fires (the build uses `-w`, so check with `-Wall` explicitly).
- `IsPlayerBotJunkItem` **defaults to `return true`**. Anything worth keeping needs
  an explicit exemption, or bots vendor it on the next town trip.
- The inactivity watchdog resets a bot that has not moved, fought or cast for
  ~90 s. Any activity that legitimately stands still must be exempted there.
- `TMonkeyVisitContext` is initialised positionally in the test. Inserting a field
  silently shifts every later value while still compiling.
- `CHARACTER::fishing()` dereferences the sectree map and tile with no null check.

## Engine facts worth not re-deriving

- Item types/subtypes live in `common/item_length.h`; map attributes and
  `SECTREE_SIZE`/`CELL_SIZE` in `game/src/sectree.h`.
- Map world coordinates: `world = BasePosition + cell * 100`, with `BasePosition`
  from the map's `Setting.txt` and `cell` from `npc.txt`. Map 21 is `metin2_map_b1`.
- `server_attr` is per-sector lzo1x: `int32 width, height`, then per sector a
  `uint32` size and a block expanding to 128×128 `uint32` attributes, one per
  50 world units. `ATTR_BLOCK = 1<<0`, `ATTR_WATER = 1<<1`. Decoder and a terrain
  dump: `tools/decode_server_attr.py`.
- Fishing: the rod goes in `WEAR_WEAPON`; bait is **not** consumed from the pouch
  but written into the rod's socket 2 by using a bait item. A cast bites after
  10–40 s and then leaves a 6 s window; `fishing::Compute` peaks ~3 s after the
  bite. Calling `fishing()` while a cast is live is the same as pulling.

## `PLAYERBOTS_FEATURE_SPECS.md`

Design intent, not a reference. It was written independently of the code and its
constants are frequently wrong — verify every VNUM, distance and API call against
the engine before using it. Modules 2 and 3 in it are already implemented, and
implemented differently from what it describes.

## Conventions

- Comments explain *why*, in English, in the voice of the surrounding code.
  Prefer explaining the constraint that forced the shape over restating the code.
- Log with the `PLAYERBOT_<AREA>:` prefix and include `pid=` and `name=`.
- Player-visible bot strings are Polish, ASCII-only (no diacritics).
- Tune with named constants at the top of the namespace, not inline literals.
- Commit messages are lowercase `type(scope): summary` in the imperative.


## Recent Session History & Unpushed State (2026-09-03)

- **Unpushed Commits (2 ahead of origin/main):**
  * 4812874: eat(playerbot): open the stalls, announce good refines, learn what lives on each map
  * d9fc647: ix(playerbot): let a parked keeper reopen its stall after a restart
- **Engine Patches:**
  * Added patch for char.cpp replacing GetPart(PART_MAIN) > 2 with IsPolymorphed(), registered in context build.
- **Stalls / Stragany Verification:**
  * Tested live with 350 running bots: 14-33 stalls active around the market in Bokjung with zero rejections, and successfully recover after server/container restarts.
- **Next Roadmap Priorities:**
  * From PLAYERBOTS_FEATURE_SPECS.md: Module 2 (Mounted combat tuning against Metin stones), Module 4 (Bot guilds and guild marks), Module 5 (Live AI Config sliders in admin panel without recompilation), Module 6 (Weekly season analytics).
