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
| `files/admin_panel.py` | Flask admin panel. The copy under
`linux-port/docker/panel/app/` is staged there by `prepare-context.sh` and is
gitignored -- editing that one changes the running panel and commits nothing. |
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
| `playerbot_battle_horse.h` | Earning the horse that can fight: the desert trial, and what the stable keeper does at the end of it. |
| `playerbot_config.h` | The weights an operator moves in the panel while the world runs. Re-read from a file every five seconds; neutral when it is missing. |
| `playerbot_empire_rules.h` | The three kingdoms as pure policy: which map belongs to whom and what it is for, the town services and gates of all six villages, the Teleporter's per-kingdom arrivals, and how two characters stand to one another. No engine types, unit-tested. |
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
| `playerbot_market.h` | Buying from another bot's counter: what is worth having, the walk to the stall, and the purchase. |
| `playerbot_chat_trade.h` | Trading over the chat: what a bot shouts about its counter and its wants, and the whisper it answers a player's "Kupie"/"Sprzedam" with. Fed by patch 0007. |
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
- **The route planner reads its own grid, never the sectree.** `m_blocked` in
  `playerbot_navigation.h` is `ATTR_BLOCK|ATTR_OBJECT` sampled at every cell's
  centre when the grid is built, and `IsLiveBlockedCell` asks the engine for the
  same point - so inside A*, string pulling and goal snapping they are the same
  answer at a thousand times the price. A corridor search that asked live for
  every neighbour of every node cost 31 s of every 60 at 850 bots. Only the
  walk (`SegmentClearWorld`, one segment per tick) asks live, as the safety net
  for anything placed after the grid was built.
- **Hubs are placed by hand, chosen by memory.** A hunting hub is a
  `TPlayerBotHuntingHub` - a spawn-point coordinate, a level band, and whether
  it is a party's work - and `ChoosePlayerBotHuntingHub` picks among the ones a
  bot qualifies for by what `playerbot_world_memory.h` has seen there: every
  target search records the monsters within reach of the bot that ran it in a
  6400-unit cell, halved every ten minutes. The memory says how full a place
  is; the table says where a place is. Do not let it invent places - a bot
  standing where a pack respawned round it is not standing where the pack
  lives. Orc Valley's table is banded: Fanatic islands 30-39, the density hubs
  36+, the three Black Orc camps 40+ with a party, the Curse Book island 45+
  with a party. `PLAYERBOT_SPOT:` logs each choice and, every ten minutes, the
  richest cells per map; a cell that keeps coming top with no hub on it is a
  hub the table is missing. A boss hub is the exception:
  `TPlayerBotHuntingHub::wBossRace` names the boss and
  `IsPlayerBotBossAlive` asks the sector whether he stands, because one
  monster every half hour is a density of nothing and the boss hubs were
  never chosen in a day of logs. Bokjung has no hub table, so its Bestial
  Captain is a detour in the M2 wander branch instead. Parties form on
  every frontier map (`IsPlayerBotPartyEligible`) because a map change
  dissolves one - a party made in the valley never reached V1.
- **A hub is chosen by share, distance and time, in that order.** The score
  is the monsters in reach divided among the bots already there, halved at
  20 km, and a choice is kept for four minutes. The first version scored by
  share alone and re-chose on every wander decision: bots crossed the valley
  for a slightly better camp and back again - 160 to 334 far plans a minute
  against 26 to 65, and the tick at 57 s of every 60.
- **The engine patches under `overlays/playerbot/patches/` are applied with
  `patch --fuzz=0` by prepare-context.sh**, in order, on top of the staged
  port source - so a new one is diffed against the builder container's copy
  of the file (which is that state), not against `m2src-cache`. That state
  is **before** the High Risk step, which runs later in the same script and
  inserts `#include "high_risk.h"`: 0006 once carried that line as context,
  applied by luck on any machine whose cache already held a High Risk tree,
  and failed every fresh `installer/install.sh`. Dry-run a new patch against
  `m2src-cache/tree/port40250/server` with `--fuzz=0` before shipping it. 0006 adds two
  CONFIG tokens (`MOONLIGHT_CHEST_PERMILLE`, `..._STONE_PERMILLE`, rendered by
  `m2-render-config` from `M2_MOONLIGHT_CHEST_*`) and, in `CreateDropItem`,
  tops a Metin stone up to three skill books and rolls the chest. The chest's
  contents come from `serverfiles/special_item_group.moonlight.txt`; the
  engine keeps the *first* group it reads for a vnum, so the Dockerfile cuts
  the stock 50011 block out before appending ours. `playerbot_consumables.h`
  opens the chest and drinks the boosters; the bonus scrolls, speed potions
  and big potions it holds go through the code that already handled them.
- **An engine patch reaches a player only as the staged file.** On Windows
  the launcher stages the overlay itself and `prepare-context.sh` never runs,
  so a patch under `overlays/playerbot/patches` is applied on this machine
  and nowhere else; and `patch` inside the image cannot be trusted either -
  the staged engine files carry mixed line endings and the core patch fails
  its own reverse check on them. What ships is the patched file itself,
  listed in `launcher/server-update-files.txt` next to the `playerbot_*`
  sources (`item_manager.cpp`, `config.cpp` for 0006). A new engine patch
  means: apply it to `linux-port/docker/game/src/server` with `patch -p1`,
  and add every file it touches to that list. 1.29.0 shipped without this and
  nobody got a chest.
- **The second Spider Dungeon is V1's sibling, entered without the pass.**
  Map 71 (`metin2_map_spiderdungeon_02`, base 665600,435200, 16x16
  sectors, one connected component): poison spiders of 60-68 that never
  attack first, no `stone.txt`, Pung-Ho at the Town.txt cell (384,273) to
  send people back and a warp to V3 by the Elite Spider Queen (2093, level
  97, 2.5M hp - no hub). A player enters through Chuk-Sal at the end of V1
  with a pass; a bot holds no pass and is warped server-side from the
  Kuahlo gate exactly as into V1 - `IsPlayerBotSpiderMap` is what the
  desert-crossing rules in `TransitionPlayerBotMap` and the crossing walk
  test. `PLAYERBOT_SPIDER_V2_MIN_LEVEL` (54) takes V1's draw in
  `GetPlayerBotFrontierMapForLevel`; the eleven hubs are the richest
  6400-unit cells of its regen.txt on the actual spawn point nearest each
  centre, checked free on server_attr (`scratchpad/check_v2_points.py`
  is the shape of that measurement). Adding it touched: the constants and
  four helpers in types.h, the draw and both crossing rules in travel.h,
  the navigation whitelist, status names, the hub table, `MAPS_game1` in
  m2-render-config (71 came off game2), apply.sh's allowed maps, both
  panels' names/bounds and the classic panel's tile
  (`tools/render_map_tiles.py <share>/locale/english/map`, needs Pillow
  and python-lzo).
- **What the merchant will take is not worth a refine, and what the
  counter lists is not merchant scrap.** The refine pass took any bag
  piece the bot could wear and raised it towards its +6..+9 ambition; the
  junk rule vendored any bag piece under +6 that was not an upgrade. In six
  hours 12 534 pieces were refined in the bag and then vendored, of 60 121
  refines. The bag loop skips `IsPlayerBotJunkItem` now,
  `IsPlayerBotHigherTierSpare` (one per slot, above the worn piece's level
  limit) is kept by the junk rule so the blacksmith can make an upgrade of
  it, and `PLAYERBOT_PRECIOUS_REFINE` is four - the counter's own
  threshold. Materials ask the ledger: `GetPlayerBotLedgerDemand` (declared
  in economy.h, defined in market.h) keeps a material anybody is short of
  off the merchant's table - a Scorpion Tail vendored for pennies beside a
  stall selling one for 58 894.
- **Joan is home for a share of the bots.** Every frontier services trip
  went to Bokjung, so the first town emptied past level thirty.
  `PLAYERBOT_JOAN_HOME_PER_MILLE` of the bots (by pid) take
  `frontier_services_to_m1`; the other reasons (medal, weapon hunt,
  graduation) stay Bokjung's.
- **The frontier is four maps and one table.** `GetPlayerBotFrontierArrival`,
  `GetPlayerBotFrontierExit` and `GetPlayerBotFrontierName` in
  `playerbot_types.h` answer for Orc Valley, the desert, Mount Sohan (61,
  `map_n_snowm_01`: the Infected of 49-58 in the south, ice creatures of
  62-66 in the north) and the Spider Dungeon V1 (104, spiders 50-58);
  `GetPlayerBotFrontierMapForLevel` routes by level and pid. V1 is reached
  across the desert: `TransitionPlayerBotMap` turns any warp into or out of
  104 into a warp onto 63 with the real destination kept in
  `lDesertCrossingTo`, and `ManagePlayerBotWorldTravel` walks the bot
  between the two gates (`PLAYERBOT_DESERT_V1_GATE_*`, the Bokjung gate)
  fighting nothing but a stone within `PLAYERBOT_CROSSING_STONE_RANGE`.
  **Map 43 is not
  Sohan** - `metin2_map_c3` is the second Jinno village with soldiers of
  26-36, and 1.29.0 to 1.29.6 sent the 26-39 band there under Sohan's name;
  the map index says nothing, read `map/index` and the regen before naming
  a map. 104 was moved onto the game1 core in `m2-render-config` - a bot
  cannot walk onto a map its own core does not host; 61 was there already.
  `apply.sh` keeps the list of maps a bot may stand on and sends the rest
  back to Bokjung at every start - add a map there too, or its bots walk
  home on the next restart. Adding a fifth map: a row in each helper, a hub table in
  `playerbot_wandering.h`, the navigation whitelist, the panel's bounds,
  names and tiles, and the core's MAP_ALLOW.
- **A mission is only as good as the map its monster stands on.** The
  level-up hunt (`PLAYERBOT_HUNTING_MISSIONS`, rows to 55) is driven by the
  engine's own `levelup` quest flags and its kill hook; the bot only picks
  the row and the option. Before the row was chosen by pid alone, and nearly
  every bot at forty was found holding a mission from fifteen with `remain`
  untouched, waiting for a wolf on a map it had left for good. Now
  `PLAYERBOT_HUNTING_MOB_HOMES` says where each monster stands (read out of
  the regen files - measure, do not guess), the option whose monster is on
  the bot's map wins, and a row is passed over without reward when it is
  outgrown by `PLAYERBOT_HUNTING_OUTGROWN_LEVELS`, has no hosted monster, or
  has hung for `PLAYERBOT_HUNTING_STALL_SECONDS`. The Biologist's chain has
  the same shape: the quest's kill hook drops the specimen only once the
  state says `go_to_disciple`, so the mission is taken wherever the bot
  stands (663 of 876 bots had never taken the mushroom mission because they
  passed level 25 outside Joan); only the hand-in walks to Joan.
  `GetActivePlayerBotBiologistMission` prefers what the bot already carries,
  then what stands on its map, then the first row undone. The Orc Tooth
  row has a second half, `key_item`, that waits for the Soul Stone; the
  state index of a compiled quest is a hash, not a position (see
  `quest/object/state/`), so never compare it with a small integer.
- **The three Monkey Dungeons are one maze, and a medal is a kill-group
  roll.** `metin2_map_monkey_dungeon_12`, `_2` and `_3` share one
  `server_attr`, the same regen cells and the same GOTO portals, so every
  place the easy dungeon was hard-coded is now a local offset from
  `GetPlayerBotMonkeyBase` and `IsPlayerBotMonkeyMap` names the three;
  `GetPlayerBotMonkeyMapForLevel` picks by band (18/33/46). The band is not
  taste: the medal is a `Type kill` group in `mob_drop_item.txt` and
  `CreateDropItem` multiplies every kill-group roll by `aiPercentByDeltaLev`,
  which is 1 at fifteen levels above the monster - a bot of forty-five in
  the easy dungeon got one medal per 140 trips. 109 was moved onto game1
  in `m2-render-config` like 104; `apply.sh` allows 108 and 109.
- **That maze is eleven chambers, and only the GOTO NPCs join them.** The nine
  rooms of `regen.txt` and the two boss rooms are separate connected
  components of the shared `server_attr`; `warp_npc_event` teleports any
  player within 300 units of a GOTO NPC, twice a second, so walking up to one
  is the whole crossing. Room choice used to ask
  `CPlayerBotNavigation::CanReach`, which only answers for the component the
  bot stands in, so every bot found one room - its own - and nine tenths of
  the dungeon was never walked: 130 trips in an hour and no planned crossing,
  every `PLAYERBOT_SPOT` density cell of all three dungeons in the entrance
  chamber. `PLAYERBOT_MONKEY_CHAMBERS` in `playerbot_movement.h` holds the
  eleven rooms as their spawn cells off the dungeon base, and
  `GetPlayerBotMonkeyChamberExits` gives the doors of one.
- **The three dungeons share the NPC cells and not the wiring.** The door at
  cell (80,308) leads to (106,547) on 25 and to (520,352) on 108, and 108/109
  carry two doors 25 does not have - which is how they reach the eleventh
  chamber. A table lifted from one map and used for the other two put bots in
  chambers nothing had planned. `GetPlayerBotMonkeyGeometry` therefore walks
  the map's sectrees once, takes every `IsGoto()` NPC, and reads its
  destination out of its own name exactly as `CHARACTER::StartWarpNPCEvent`
  does. Chamber and door components come off the navigation grid at the same
  time; `GetPlayerBotMonkeyChamberAt` asks at radius 1, because two chambers
  can run side by side with one blocked cell between them and a wide search
  answers with the corridor over the wall.
- **The crossing is noticed on the tick, not in the wander pass.**
  `UpdatePlayerBotMonkeyChamber` runs for every bot in a dungeon before
  anything can claim the tick. A portal moves the bot without touching its
  route, and re-planning towards the old destination finds the door it came
  in by; the wander pass runs only on a tick no subsystem took, and a bot
  dropped among aggressive monkeys is fighting, not wandering.
- **Whether a fight is worth having is one question with one answer.**
  `playerbot_combat_value_policy.h` decides it and nothing else does:
  `BuildPlayerBotCombatContext` is the only place a Context is built, and the
  three callers - the target collector, the multi-pull and
  `IsPlayerBotHeldTargetStillWorth` on a three-second clock - all go through
  it. Filtering only new candidates left the hole open: a monster picked up
  before the errand changed was fought to the end. The exceptions are
  countable (quest, material, stone, bounded defence) and each is bounded by
  something the engine can be asked about, not by intent: the material one by
  `PERCENT_LVDELTA` (a drop obeys the same level curve as experience, so a
  need is not a reason to farm what cannot drop it), the defence one by
  **one episode per bot** - keyed per attacker it was renewed for ever by two
  monsters taking turns, so it now ends only after
  `PLAYERBOT_DEFENCE_QUIET_TIME` without a combat action.
  **Self-defence is bounded by the leash and not by the clock**, and that
  distinction was learned the hard way: with a ten-second bound on it, nine
  strong monsters dropped next to a group of bots killed all of them, because
  after ten seconds each monster was refused as a target while it was still
  killing its bot. What the episode exists to stop is a bot being walked
  across a map by a chain of attackers, and `PLAYERBOT_DEFENCE_LEASH` stops
  exactly that; hitting back at what is hitting you is not a choice to
  ration. Helping a party member keeps the clock - defending yourself is
  compulsory in a way that helping is not - and RETREAT outranks both.
  The level cap has the same shape: `bTargetNeedsParty` decides what a bot
  walks up to, never what it answers, so a monster whose victim is this bot
  passes it.
  `PLAYERBOT_M2: census` is how this is measured: a count of level-40 bots in
  Bokjung says nothing, the reason each one is there says everything.
- **A Biologist hand-in is one specimen at a time, so the trip is too.** The
  walk to Joan was gated on carrying the whole remaining count - ten Orc Teeth
  at once - and the world had ten such bots against 700 carrying 2219 teeth
  between them, so the counter sat at 0/10 for everybody. The hand-in itself
  has always taken one specimen per interaction with a 60% accept roll;
  `PLAYERBOT_BIOLOGIST_MIN_HANDIN` is what the gate should have been, and the
  same threshold has to be used in `NeedsPlayerBotM1OnlyServices` or the trip
  from Bokjung never starts.
- **A shell holds what the engine says it holds.** `char_item.cpp` case 27987
  picks between two tables by `g_iUseLocale`: `{80,90,97}` when false and
  `{95,97,99}` when true. This world's `common.locale` is "english" and
  `__LocaleService_Init_English` sets the flag, so the live odds are 50% stone,
  45% nothing, 2/2/1 percent pearls - not the 10/7/3 the AI constants carried,
  which made opening one look four times better than it is. Check the flag
  before trusting `PLAYERBOT_SHELLFISH_*_PERMILLE`.
- **A skill book is not one commodity.** `PlayerBotSaleKey` was
  `vnum * 16 + refine`, and every ordinary book is vnum 50300 with the skill in
  socket 0 - so one cheap sale of a spare set the price of Aura Miecza, and a
  sale of Aura moved every other book. The key carries the skill now (seven
  bits, skills run 1..111) through the sale memory, the ask-step limiter and
  the asking price. Priors per named book and per pearl are the starting
  calibration; the sale memory blends them away as transactions arrive.
  `WantsPlayerBotStallItem` had no ITEM_SKILLBOOK branch at all, so no bot ever
  bought one - raising the price without that would only have made expensive
  unsold stalls.
- **`LimitPlayerBotAskStep` stepped on every call.** `1 + elapsed/interval`
  gives a full step at elapsed zero, and an accepted step resets the clock, so
  forty counters opening together moved the shared anchor forty times. Whole
  intervals only.
- **An arrival radius below `PLAYERBOT_NAV_ARRIVAL_DISTANCE` strands the bot.**
  `MovePlayerBot` reports success and stops moving a hundred units from its
  goal; a caller that keeps asking until twenty-five leaves the bot standing in
  the gap for ever, with `stuck=0` and nothing in any log - the walk did
  succeed, by its own rule. Cutting `PLAYERBOT_FISHING_ARRIVE` to twenty-five
  for the sake of spacing anglers froze most of them: forty-three at the water
  and eleven fishing, two of them measured stuck at seventy-one and seventy-six
  units from a destination neither reached. Back at a hundred it is fifty-three
  and fifty-three. The rule is a `static_assert` in `playerbot_activities.h`
  now, because this is the third shape the same mistake has taken - the goal
  snap for the town leg, the goal snap for the portal walk, and now the arrival
  test itself - and a comment has stopped three times being enough.
- **The planner and the walk have to agree about the same segment.**
  A* strings its corners straight between cell centres on the static grid;
  `SegmentClearWorld` runs a supercover traversal from the character's exact
  interpolated position and counts a cell grazed by a millimetre of corner.
  Where they disagree the walk refuses the waypoint, `MovePlayerBot` throws the
  route away and replans two hundred milliseconds later - identically, for
  ever. A portal destination is a raw constant sitting on a cell boundary, so
  every portal in the world had this: measured at five of them on four maps,
  forty-two refusals in twenty seconds with no movement and **nothing in any
  log**, because that branch logged nothing and the unreachable branch beside
  it speaks at failures one and three and then goes quiet. The walk aims at the
  destination's cell centre now (thirty-five units at most, against a switch
  distance of two hundred), a refused segment gets two rescues before the route
  is dropped, and the branch says so. Stalls went from ninety a minute to none
  and 569 transitions ran in eight minutes; the tick fell from 9.5 s to 4.4.
  When a diagnosis needs three deploys to find, record the outcome
  (`bLastNavOutcome`) rather than deducing it - every way this function can
  decline looks identical from outside.
- **Progress towards a portal is a waypoint consumed, not a shorter straight
  line.** A portal stands against scenery far more often than in open ground,
  so walking round a building is the normal case; measuring only the straight
  line threw a bot at route 5/7 off its route every twenty seconds. And the
  stall clock is wall time, which runs while the bot is fighting or shopping -
  `PLAYERBOT_PORTAL_WALK_MIN_TICKS` makes it count attempts, after one bot was
  caught being declared stalled on its first walk step with 88 km to go.
- **A purchase priced per bundle refuses the whole bundle.** The Rybak sells
  bait twenty at a time for eight hundred yang and `RestockPlayerBotTackle`
  bought all of it or none, so a bot holding 556 - thirteen worms and a
  session's fishing - stood at the counter buying nothing. Reported as "they
  stand under the Rybak and do not buy bait", and unresolvable from the report
  because three quite different failures (no price on this world, no money, no
  bag cell) all left through one door as "cannot_afford_tackle". Naming each
  refusal found the cause on our own server in two minutes. Any stack bought
  from an NPC wants the same shape: take what the purse reaches, re-price the
  smaller count and re-check it, and let a single item stay all-or-nothing.
- **`AutoGiveItem` never refuses a full bag: it drops the item at the
  character's feet and returns it as a success.** `char_item.cpp` -
  `AddToGround` + `StartDestroyEvent` on the no-cell branch. Every purchase
  therefore has to test `GetEmptyInventory` *before* paying, or the bot pays,
  the item lies on the grass, and the bot buys again on the next pass. The
  arrow purchase in `playerbot_gear.h` learned this and says so; the tackle
  purchase was written a day later without it and produced the Discord
  photograph of a herd of summoned horses round the Rybak standing in
  "Wedka+1". A stackable that already has a stack merges and needs no cell;
  potions are capped at free cells * 200 plus the partial stack's headroom.
- **"Carrying" a specimen has to mean carrying enough.** The Biologist's
  first pass took any bot holding one of a row's items to that row, ahead of
  every other rule, so a single Gango Root from a Joan fishing trip pinned a
  level-40 bot to the level-15 row for good: outgrown, so never hunted, so
  never five, so never handed in. 38 bots of 26+ held roots, 36 of them one
  to four. An outgrown row is taken only when the bag holds the whole hand-in.
  The classic panel's "Etap Biologa" labelled the *first incomplete* row with
  no reference to the core's choice, which is what "the panel says one thing
  and the ranking another" was; it applies the same rule now, reading the bag.
  A panel change is only live after `docker compose up --build panel` - a
  recreate runs the old image, and the first verification did.
- **The level-30 weapon was a thing a bot got young or never.** M3 stopped at
  24 and the Bestials at 35; past that the only route was a counter the bot
  never walked to, because the market trip refuses anything further than
  `PLAYERBOT_MARKET_TRIP_RANGE` from a pitch and a frontier bot is a map away.
  205 of 393 rich bots of 36+ had none. `PLAYERBOT_LEVEL30_WEAPON_HUNT_MAX_LEVEL`
  carries the farm to 40 (the kill-drop curve is still 70% ten levels over
  the mob) and the frontier hands a weaponless bot back to M2
  (`frontier_weapon_to_m2`), where the existing M3 branch takes over. Six
  weapons found in the first three minutes. Pricing: an unrefined one fell
  into the "under +4" scrap branch at merchant x2; `PLAYERBOT_PRIOR_LEVEL30_WEAPON`
  floors it at 250 000 and a damage line in the upper half of what rolls
  (average >= 24, skill >= 15) adds `PLAYERBOT_SHOP_BONUS_PRIZE_LINE`.
- **The ItemShop is a service beside the game, not a patch to it.** The
  r40250 core already sends `mall http://<MALL_URL>/ishop?pid=..&sas=..`
  from `ACMD(do_in_game_mall)` with the literal key "GF9001", and the
  client already maps "mall" to its `WebWindow` (checked against the
  unpacked root under the client's Eternexus folder: uiweb, uishop,
  uisafebox, localeinfo identical to Oskar's). What ships is
  `linux-port/docker/itemshop/` (his PHP app, 17 MB of it icons, password
  and key from the environment), `mariadb/playerbot/itemshop_schema.sql`
  (the schema the package never had, derived from what the PHP reads, plus
  `player.item_award` and a seed that only fills an empty shop) applied by
  `apply.sh` as root - the metin2 user cannot create a database - and the
  `M2_MALL_URL` default in compose. A purchase is a row in
  `player.item_award`; the db core's ItemAwardManager delivers it. Test it
  with the signed link: `md5(pid . account_id . "GF9001")`.
- **The client's scripts live in `pack/root.epk`, and `tools/eterpack.py`
  rewrites it.** The index (`root.eix`) is one LZO object under XTEA with
  the stock r40250 index key, and each file is an LZO object: in this
  client type 1 is plain ("MCOZ" + stream, no key), the encrypted form is
  "MCOZ" + stream padded to eight under the key. The stock TEA variant is
  XTEA, not TEA, and the first four bytes of a decrypted region are the
  fourcc again - the two things that cost an hour. `repack` keeps every
  file and type, swaps in what `linux-port/client-root/` holds, and the round trip
  is checked by extracting both archives and diffing (only the swapped
  files may differ). A client change ships as `pack/root.eix`+`.epk` in
  a `-Type client` package and the `client` component of the manifest,
  which the launcher applies over the client folder; never the loose .py
  files, which the client does not read.
- **The F9 GM panel is Oskar's, merged as text.** His `cmd_gm.cpp` and
  `cmd.cpp` are our files re-encoded by an editor (the Korean comments no
  longer round-trip), so they could not be diffed or copied whole; the
  twenty-one `gmpanel_*` commands and their two helpers are pure ASCII and
  were appended (`0009-gm-panel-commands.patch`, dry-run clean against
  `m2src-cache`; both files ship staged too). His `botadmin_*` commands
  were left out - they call manager methods of his fork. `GetAvailableBots`
  is the one thing the panel needed from ours.
- **Scroll odds are the engine's, not the wiki's.** `DoRefineWithScroll`:
  Blessing Scroll (25040) keeps `refine_proto` prob (40/30 at +7/+8) and
  hands the piece back a level down; Zwoj Boga Smokow (39022/71032/76009,
  YONGSIN) 25/20; Podrecznik Kowala (39007/70039, YAGONG) 30/20; Magiczny
  Kamien (25041/39001, HYUNIRON) destroys on failure. None needs a
  blacksmith, which is why the scroll pass runs anywhere. The operator
  asked for the Dragon God scroll from +7 (`FindPlayerBotRefineScrollCell`)
  and got it, with these numbers on record.
- **The panel's VERSION is staged on the path the click never runs.**
  `start-server.ps1` copies VERSION, CHANGELOG.md and the panel sources into
  `linux-port/docker/panel/app/` - below its `-IdentityOnly` return, which is
  how the launcher calls it before building on its own. So a player who only
  ever pressed GRAJ or AKTUALIZUJ had a panel image baked from the VERSION the
  installer left, and a `--no-cache` rebuild could not help: "Masz uruchomiona
  1.29.0. Dostepna jest 1.30.38", ten releases in. `Sync-M2PlayerbotOverlay`
  stages the panel context now, next to the bot sources it always staged. The
  advanced panel's number is a different pipe: `PLAYERBOTS_VERSION` from
  compose, whose default nobody bumped after 1.30.29;
  `Set-M2PlayerbotsVersionEnvironment` puts VERSION into the process
  environment (compose reads it ahead of `.env`, which is never rewritten)
  and the release still bumps the default. The live-log filter in the classic
  panel is the same family of bug from the other side: `"botgrom" in line`
  matched botgrom2..botgrom6, so one keeper's log was five bots' log.
  And the support bundle's `playerbot-syslog.txt` shipped empty in 1.30.40:
  Windows PowerShell 5.1 wraps a native command's argument in double quotes
  without escaping the ones inside it, so the first `"` in an `sh -c`
  script ends the argument. No double quotes inside a command handed to
  docker from PowerShell - `grep -e` per pattern, unquoted `$f`.
- **A guard belongs on the path that does the thing, not beside it.**
  The build-context check went into `start-server.ps1` and the report came back
  unchanged, because `Metin2-Launcher.ps1` calls that script with
  `-IdentityOnly` - which returns at line 557 having written the .env, while the
  check sat at 713 - and then runs `docker compose up --build` itself. Two code
  paths reach a build; only one had the guard. Before adding a precondition,
  find every caller that performs the operation, not every caller that looks
  like it should.
- **A status line that is a plain `else` will lie.** "Wychodze z Lochu Malp"
  was the fallback for BOT_ACTION_TRAVEL on a monkey map, so every bot crossing
  the maze - which is what travel means in an eleven-chamber dungeon joined
  only by GOTO NPCs - announced an exit. Gating it on the goal was not enough
  either: BOT_GOAL_HORSE is what a medal expedition carries for its whole
  visit, and twenty-one of thirty bots still claimed it. The exit is a direct
  map change on the tick it is decided, so a bot anybody can still see in the
  dungeon is by definition not leaving, and the word does not belong there at
  all. Same shape as the angler who announced baiting a rod it was not holding.
- **Weigh a bonus line against what it can actually roll.**
  `ScorePlayerBotBonusLine` gave skill damage 12 and average damage 10 for
  every character. Measured across every attribute on every item in this world,
  average damage rolls to 46 and skill damage to 18 - so a maximum average roll
  scored 460 against a maximum skill roll's 216, and a Shaman, whose damage is
  nearly all skills, rerolled away the best line its build can have. The
  weights are per build now and derived from those two ceilings. Worth knowing
  before tuning further: `APPLY_ATTBONUS_MONSTER` - the one line that raises
  damage against monsters *and* Metin stones, which is all a bot ever fights -
  does not roll here at all, so no reroll can ever produce one.
- **Never delete a build context before proving it can be rebuilt.**
  `prepare-context.sh` did `rm -rf game/src` and discovered a missing engine
  module a hundred lines later, so a truncated porting tree turned a working
  install into a broken one. What the operator is left with is the context the
  update package alone provides - `src/server/game` and nothing beside it,
  because that is where the playerbot sources belong - and a build that fails
  with fifteen "failed to calculate checksum ... not found" lines naming paths
  nobody deleted on purpose. Reported from the Discord with a 1.61 MB build
  context against hundreds of megabytes for a complete one, and the 38 files
  the package puts under `game/src` are exactly that 1.61 MB. The nine modules
  and four share directories are now checked first, and `start-server.ps1`
  refuses the build with one sentence naming what is missing rather than
  letting Docker say it fifteen times.
- **`StopRiding()` summons the horse as a follower.** So a bot that dismounts
  keeps its horse trotting behind it until something mounts again or dismisses
  it - which is what "bots walk long distances and the horse runs after them"
  was. The cause was the default: `MovePlayerBot`'s seventh argument is
  `allowHorse` and every wander leg passed six, leaving it false, while
  `ChoosePlayerBotHuntingHub` picks ground up to twenty kilometres away. Long
  legs ask for the horse now (wander, known Metin, boss hub, the walk back from
  a respawn); `UpdatePlayerBotTravelMount` still refuses inside
  `PLAYERBOT_HORSE_MOUNT_DISTANCE`, so short hops are unchanged. Mounts for
  long travel went from 7 in twenty-five minutes to 304 in five.
- **A Metin stone is not a monster, and the recovery pass did not know it.**
  `IsStone()` is `CHAR_TYPE_STONE`; the emergency recovery drops the target at
  `PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT` whatever it is. Right for a monster,
  which chases; wrong for a stone, which does not, and which is then found at
  full health or gone. A stone under `PLAYERBOT_STONE_FINISH_STONE_HP_PERCENT`
  is finished while the bot is above `PLAYERBOT_STONE_FINISH_OWN_HP_PERCENT`.
- **The old woman is one town errand and no new machinery.**
  `skill_reset2.quest` (NPC 9006, map 21 cell 588,633) charges
  `10000 + level * 2000`, refuses under five and over thirty, then
  `clear_skill()` and `set_skill_group(0)` - and a group of zero is already
  what sends a bot to the trainer, so `BOT_TOWN_PHASE_SKILL_RESET` only has to
  pay and let the existing trainer phase follow. `ClearSkill()` refunds
  `4 + (level - 5)` points. Worth doing only when nothing else is left: a skill
  at seventeen that will not turn Master *and* no skill points in hand, because
  the reset costs every skill level the character has. Verified live end to
  end: cost 64000 at level 27, 26 points back, at the trainer twenty-six
  seconds later. Note `GetJob()` maps the DB's race column through `RaceToJob`
  and returns 0..3 - gating on the raw column would refuse every character.
- **Being tracked by Git is not being delivered.** An install assembled from an
  update package holds exactly what `server-update-files.txt` lists;
  `panel/bin/apply_rates.sh` was tracked and still missing on the machine that
  reported it, and `game/rates/` was the next one along - "failed to compute
  cache key ... /rates: not found", unrepairable by reinstalling because the
  reinstall uses the same package. Six build inputs were missing across the
  contexts. `tools/check-update-covers-build.py` reads every `COPY` of every
  Dockerfile and fails when one of them would not arrive; run it before a
  release rather than trusting this list to stay complete.
- **The spawn queue was filled once and never looked at again.**
  `SpawnRegistered` queues the cohort at startup and drains it over
  `PLAYERBOT_SPAWN_WINDOW`; nothing counted the world afterwards, so a bot whose
  load failed or which left later was gone until a restart. Reported as "a
  thousand asked for, six hundred and fifty arrived, three hundred and fifty an
  hour later". `TopUpMissingBots` re-counts once a minute against
  `CHARACTER_MANAGER::FindByPID` and re-queues the missing through the same
  staggered path, bounded by the original window so it restores the cohort and
  never grows it. On a healthy server eleven of eight hundred and fifty were
  missing from the first fill.
- **A conjunction that rejects tells nobody which clause did it.**
  `LoadRegisteredBots` accepts an identity only when six conditions hold at
  once, and printed one number. `ReportPlayerBotRegistryShortfall` runs the same
  joins with conditional sums and says which clause dropped what: on this world
  `rows=1182 usable=1012 login=170 social_id=101 other_characters=62`, so the
  ceiling is accounts whose login or social id does not match the generator's
  pattern - not the launcher's slider.
- **The desert was the richest map in the world and nobody hunted on it.**
  14026 spawn points against Orc Valley's 8122, levels 37 to 51 across its own
  bands, and `PLAYERBOT_DESERT_MAX_LEVEL` capped it at 36 - so everyone from
  thirty-six up went to the valley and the desert was a corridor to the Spider
  Dungeon. Raising the cap to 47 and splitting the band moved Orc Valley from
  308-388 bots to 141 and the desert from 97-123 to 232, and cut the tick from
  13-19 s of every 60 to 5.2: spreading the population over more maps is worth
  more than any navigation tuning done so far.
- **An engine patch is only tested where prepare-context.sh runs, and that is
  never Windows.** `0008-warp-npc-ignores-playerbots.patch` shipped truncated
  from 1.30.13 to 1.30.20: the header said `@@ -6447,7 +6447,19 @@` and the body
  carried six old and eighteen new lines, one context line short of the closing
  brace. Every Linux and VPS install stopped at "Hunk #1 FAILED" and could not
  update; no Windows install noticed, because there the launcher stages the
  already-patched `char.cpp` and the patch is never applied. Dry-run every patch
  with `--fuzz=0` against `m2src-cache/tree/port40250/server` before shipping it -
  the note above this one already said so, and this is what skipping it costs.
- **Every build context a player builds has to ship, not just the game's.**
  `docker compose up` bakes game, panel and seban-panel together, so a
  two-byte `linux-port/docker/panel/Dockerfile` failed the whole bake and the
  other two were CANCELED with it: nothing started, and "click PLAY again"
  could not help because no update replaced the broken file - only
  `game/Dockerfile` was in `server-update-files.txt`. All seven are now, with
  their `.dockerignore` files.
- **The point you verified must be the point the engine samples.** The
  navigation grid calls a cell blocked by testing its centre,
  `base + n * PLAYERBOT_NAV_CELL + PLAYERBOT_NAV_CELL / 2`. Fishing stands
  generated on the multiples of fifty sit on cell corners, so server_attr said
  "standable" about one point and the grid judged a different one; the walk then
  snapped them - with a twelve-cell radius against an arrival radius of
  twenty-five - and two anglers ended up eight units apart on one stand. Both
  halves are the same lesson as the portal walk: generate on cell centres, and
  keep the snap inside the radius that tests arrival.
- **A plan budget counted in plans starves the plans that matter.** Measured
  over a minute at 839 bots: 7440 route plans, of which 7086 were sub-64-cell
  hops to the next monster costing 41 milliseconds between them, while 148 long
  ones cost eleven of the twelve seconds. Every one of them charged the same
  slot against `PLAYERBOT_NAV_MAX_HEAVY_PLANS_PER_TICK`, so the hops filled the
  tick and the crossings - the walk to a portal, the trip to a merchant - were
  refused: half of all requests deferred and one waiting thirteen minutes.
  `PLAYERBOT_NAV_PLAN_COST` charges by distance bucket and a hop is free;
  `PLAYERBOT_NAV_STARVED_ATTEMPTS` lets a request that has been turned away that
  many times past the count, still bounded by the microsecond budget and the
  far-plan minute. Deferrals fell to 264 a minute and starvation to none.
- **A pass that yields the tick must not leave something for the next pass to
  claim.** `MovePlayerBotToWorldPortal` returns false after
  `PLAYERBOT_PORTAL_WALK_TIMEOUT` so the bot falls through to wandering and
  plans afresh from somewhere else. The manager's "a transport horse must not
  fight" dismount took that tick instead - it dismounts and `continue`s - so the
  bot spent its escape getting off the horse and remounted on the next travel
  pass. Forty-six bots at the Sohan exit, mounting and dismounting every twenty
  seconds without a step. The walk dismounts itself before yielding.
- **A snapped goal must stay inside the radius that tests arrival - the portal
  walk too.** It asked for a 24-cell snap (1200 world units) against
  `PLAYERBOT_PORTAL_SWITCH_DISTANCE` of 200, so a route could end a kilometre
  short and the transition never fired. `PLAYERBOT_PORTAL_SNAP_CELLS` derives
  the snap from the switch distance instead. `MovePlayerBotTownLeg` sprang this
  first; anything that walks to a point and then tests a small radius around it
  is the same shape.
- **The shop pass judges an item before the equipment pass does.**
  `ManagePlayerBotPrivateShop` runs near the top of the tick and
  `ManagePlayerBotEquipment` near the bottom, and the stall treated any weapon
  or armour whose slot was already filled as a spare - which a gift is not: it
  beats a full slot rather than filling an empty one, and a spare at +6 or
  better is the highest-scoring thing a counter can carry.
  `IsPlayerBotWearableUpgrade` asks the engine's own `CanEquipNow` and the
  equipment pass's own score, so the race stops mattering. The line is "can wear
  it now": a level-30 weapon in a level-5 bag stays goods.
- **A fast build that ships only the core ships a different server.**
  `tools/fast-game-build` swapped the compiled binary into whatever the last
  full build left behind, so the test server ran month-old container scripts
  without saying so: an `m2-render-config` from before the Spider Dungeon and
  the hard Monkey Dungeon moved onto game1, a `MAP_ALLOW` without them, and
  every bot that reached either gate refused by `TransitionPlayerBotMap` ten
  thousand times a minute - one throttled log line a minute with the real count
  hidden in its `[+N more]`. A day of measurements concluded those maps were
  empty for reasons that were never true. The fast build copies
  `linux-port/docker/game/bin/` now, exactly as the real image does.
- **A price of one yang is permanent.** `GetPlayerBotNpcSellUnitPrice` returns
  zero for anything `item_proto` prices at zero - the horse medal 50050, every
  chest and casket - so the asking price came out `max(1, 0 * markup)`, and the
  median wallet is zero until `RefreshPlayerBotMarketLedger` has run for the
  first time, which removes the only other floor. `LimitPlayerBotAskStep` then
  made it permanent: five percent of one yang is zero in integer arithmetic, so
  no anchor under four could ever move, and every stall listing the item
  refreshed the clock before the hour of staleness could run out. Three things
  hold it now - a prior for the goods with no merchant price, an anchor under
  `PLAYERBOT_MARKET_ASK_FLOOR` treated as an accident rather than a price, and a
  step of at least one yang per whole interval.
- **A pass that refuses must also back off.** The private shop returned without
  setting `dwNextShopKeepTime` when the bag had no cell for the shop bundle, so
  it asked again on the next tick: eight log lines a second for one bot, and the
  bot pacing its pitch for ever - because the pass that would have emptied the
  bag is the merchant leg of the town visit, and this one kept claiming the
  tick ahead of it.
- **The Biologist's seven rows are seven quests, not one chain.** Picking "the
  first row left undone" as the fallback handed a bot of forty-two in Orc Valley
  the Gango Root of level fifteen, whose monster stands in Joan; it never goes
  there, so the panel read "Korzen Gango 0/5" while the bot hit Orcs, for ever.
  A row outgrown by `PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS` is stepped over by
  both middle passes - including "its monster is on this map", or a hand-in trip
  to Joan would leave a bot of forty camped on level-fifteen ground - and when
  every row is outgrown the highest one left is taken instead of none.
- **The advanced panel's restart console wrote to a name nobody reads.**
  `queue_server_settings` published `server-settings.request`; the game
  container watches `request` and only `request` (see `m2-rates`). Nothing
  deleted the orphan either, so the exclusive `os.link` that guarded against
  double clicks refused every click after the first one, permanently. Both
  buttons now go through `queue_rate_restart`, which is the path the rates page
  has always used, and the double-click guard is a bounded look at
  `rates.status` instead of a lock nothing releases. The map respawn half of
  that console still has no consumer in this image and the panel now says so.
- **An unfinished errand is somebody's job until it is done.** The 8 September
  audit traced the loop that kept level-40 bots fighting in Bokjung, and it is
  not the map choice: the bot needs a merchant, the route is deferred for want
  of planning budget, the inactivity watchdog fires on the stillness, the visit
  is thrown away, and one second later the bot casts an attack skill at
  whatever is nearby with the need it came for still unmet. The watchdog still
  clears the dead route - that is what it is for - but `bServicePending` now
  carries the errand across the reset with its own retry, and the combat
  adapter puts such a bot in COMMITTED_TRAVEL so the reset cannot hand an unmet
  need to the target picker. It gives up loudly after
  `PLAYERBOT_SERVICE_GIVE_UP` rather than wedging.
- **"Is this fight worth it" and "may I grind here at all" are two rules.**
  The combat value policy answers the first; `IsPlayerBotGrindAllowedHere`
  answers the second, and above `PLAYERBOT_M2_COHORT_MAX_LEVEL` in Bokjung the
  answer is no. That needed a fourth mode in the policy - `SERVICE_ONLY`,
  placed after the named objectives and before the experience checks, because
  `COMMITTED_TRAVEL` refuses everything including the quest the bot is
  legitimately there for. The rule has to cover every road to a fight, not just
  the collector: party focus, the held target, the engaged target, the
  multi-pull and the Archer's lure all ask it. Self-defence never does.
- **A departure intent outlives the errand that delays it.**
  `lDepartureMap`/`dwDepartureSince` are set when travel is held back by a
  purchase and are not cleared by the watchdog, so after the visit the bot
  leaves instead of waiting for ambition to be rolled again.
- **A deferral is not an unreachable destination.** Three budgets -
  plans per tick, planning time per tick, far plans per minute - returned the
  same `PLAYERBOT_NAV_PLAN_DEFERRED`, and the log could not tell them apart.
  `s_szPlayerBotNavDeferReason` and `dwFirstNavDeferTime` put the reason and the
  wait into the line. The fair queue with per-request ageing that the audit also
  asked for is *not* built: it is a hot-path redesign and belongs in its own
  measured change.
- **A buff cast claims the tick, so the set has to be gathered quickly.**
  `ManagePlayerBotCombatBuffs` casts one buff and returns; at five seconds
  between passes a Warrior needed ten seconds for aura and berserk and a
  weapon Sura fifteen for three enchantments. Aura of the Sword lasts
  `30+50*k` seconds on a `30+10*k` cooldown, so a bot was spending as long
  putting it back up as it stayed up, and was usually seen without it.
  `PLAYERBOT_BUFF_RECHECK_FAST` brings the bot straight back after a cast;
  the ordinary five seconds resume on the first pass that finds nothing
  missing. Mana is the other half and is not solved here: 213 bots of 1300
  hold less than the 300 SP a mastered aura costs.
- **A splash skill is the weakest thing in the rotation against one target.**
  `IsPlayerBotSplashSkill` asks the engine for `SKILL_FLAG_SPLASH` and the
  rotation skips those against a Metin stone, which is never a crowd. The
  rotation takes the first skill off cooldown, and a stone lasts long enough
  to put the good ones there, so what actually landed on stones was Poison
  Cloud - `-(lv*2 + (atk + str*3 + dex*18)*k)` against Fast Attack's
  `-(atk + (1.6*atk + ...))`, for the same animation lock.
- **The wallet floor cannot rank two materials; the merchant's price can.**
  `GetPlayerBotShopAskingPrice` raises a material to a share of the median
  wallet, and that share was the same number for everything - ~38 000 at a
  2.6M median - so a shellfish (merchant 3 000) and a white pearl (12 000)
  stood on the counters at the same price. It is scaled by `npcUnit` against
  `PLAYERBOT_MARKET_WALLET_REFERENCE_PRICE`, in hundredths and clamped to
  [100, 800] percent so nothing gets cheaper and nothing runs away. And
  `PLAYERBOT_MARKET_REGULATOR_MAX` went from 1.35 to 2.0: a third above the
  prior is not a market answering five hundred bots short of a thing no
  counter carries.
- **A stall that ran out is followed by another on the same pitch.** A
  stand of `PLAYERBOT_SHOP_MIN..MAX_DURATION` (10-25 min) was followed by
  `PLAYERBOT_SHOP_REST_MIN..MAX` (30-90 min), and the open pass only fires
  when a town visit has just ended or the bot is already at the pitch - so
  after a restart, with every keeper standing where its last stall was,
  ninety opened at once, and an hour later eleven were left: a fifth of the
  keepers, which is what the numbers say. Measured on our own world with no
  restart: 94 keepers at ten minutes, 33 at twenty-five.
  `ClosePlayerBotShop` now reopens an expired stand after
  `PLAYERBOT_SHOP_REOPEN_MS` for up to `PLAYERBOT_SHOP_STANDS_IN_ROW` stands
  (`bShopStandsInRow`), ends the row after two dry stands
  (`bShopLastStandSold`), and only then rests. Sold out, off the pitch and
  a refused open still rest at once. Measure it as keepers in
  `playerbot_status.tsv` over an hour without a restart, not as the count
  right after one. And the reopening was only half of it:
  `PLAYERBOT_SHOP_M2_MAX_STALLS` (seven) counted every stall on the map, so
  every reopening after the first burst was refused at "Bokjung full" -
  229 a minute - and the keeper walked its goods to Joan, where the planner
  sent it shopping; ninety stalls were twenty-eight ninety minutes later
  with the reopening in place. The cap is a floor under
  `PLAYERBOT_SHOP_M2_STALLS_PER_MILLE` of `GetPlayerBotsAlive()` now.
- **A refusal on the travel pass must set the travel clock.** The
  Teleporter refused a bot short of the fee and returned false, and the
  travel pass asked again on the next tick: 24 000 refusals a minute from
  one world, the bots standing in Bokjung with "Ide na Gore Sohan" above
  their heads - which an operator reads as bots that cannot find the
  portal. `PLAYERBOT_TELEPORTER_RETRY_MS` on `dwNextWorldTravelTime` lets
  the town visit and the stall run and earn the fee, and the status says
  "Zbieram yang na Teleporter". Same shape as "a pass that refuses must
  also back off" above.
- **A keeper trades in the town it is standing in.** The stall used to roll
  a town and then refuse to open unless the bot was already there - nine
  rolls in ten chose Joan while the bots with goods stood in Bokjung, so Joan
  got no stalls at all. The market browse has always read the ring of its own
  bot's map, so a stall opens where its customers are.
- **`player.map_index` is where a bot was last saved, not where one is.**
  The table holds every registered bot, and most of them are not spawned: it
  said four hundred bots on map 21 while `playerbot_status.tsv` - which only
  carries live characters - said nineteen of eight hundred and thirty-seven.
  A conclusion about where the population is must come from the status file
  or the core, never from a count over `player`. Joan is thin because the
  live cohort is mostly level 40+, and everyone past the M2 band leaves for
  the frontier; what actually takes a bot there is fishing, whose bank, bait
  merchant and market ring are all on that one map.
- **A boss is news, and the news travels through a guild.** A boss hub scored
  `PLAYERBOT_RAID_WORTH` for everybody, which outran every hunting ground by
  two orders of magnitude, so a whole level band walked to one monster - 145
  of them in two minutes - and the ones that arrived late stood about. Worse,
  the chosen hub was kept for `PLAYERBOT_HUB_STICK_TIME` **without asking
  again whether the boss was still standing**, so four minutes of a column of
  bots on empty ground was the normal end of every raid. The stick now
  re-asks (`boss down, going back to work`), the first bot to find him
  standing calls its own guild through `CGuild::Chat`, and that guild may
  fill `PLAYERBOT_RAID_CROWD` places while everyone else gets half of them.
  Count the bots that have *decided* to go, not the ones standing on the hub:
  `CountPlayerBotRaiders` keeps a roster per race, because a hundred bots
  choosing in the same second all see an empty hub and all set off.
- **A pull is what came back, not what was shot at.** `playerbot_lure.h` is
  the Archer's party role as a whole errand - PLAN, APPROACH, TAG, CONFIRM,
  RETURN, HANDOFF, RECOVER - and CONFIRM counts the live monsters actually
  chasing the bot, so a miss, a one-shot kill and a pack that never woke up
  all count as nothing. It reuses the ordinary bow shot
  (`ExecutePlayerBotBasicAttack`, which owns range, arrows and rhythm) rather
  than growing a second damage path; the old `ExecutePlayerBotArcherLuring`
  had one, complete with an invented damage number when the real one came out
  under five. Two things it must not do: hold a character pointer across
  ticks (the roster is copied out of the party every tick, and the claim on
  the role is keyed by leader PID), and let the party follow it - the lurer
  keeps `dwTargetVID` at zero so the shared party focus never sees the pack it
  is waking up. The multi-pull cannot run at the same time by construction:
  that one refuses a bot in a party and this one needs five.
  `FindPlayerBotLurePack` is its own finder for a reason - the multi-pull's
  looks for what is at a solo bot's feet, and on a map carrying eight hundred
  bots that describes the ground the party is already standing on.
- **A bot cannot be warped by a warp NPC, and now it is not asked to be.**
  `WarpSet` tells the client to reconnect to whichever core hosts the target
  map; a bot descriptor has nobody to answer that, so the map change is made
  server-side instead. It used to be made 900 units short of the portal,
  which is what a player sees as a bot vanishing out of clear ground. Patch
  0008 makes `warp_npc_event` skip a character whose descriptor `IsBot()` -
  only in the `m_bUseWarp` branch, because the GOTO branch below it is a
  local `Show()` and the Monkey Dungeon is walked with it - so the margin
  only has to cover a tick of running: `PLAYERBOT_PORTAL_SWITCH_DISTANCE`.
  `char.cpp` had to join `server-update-files.txt` for that, which also
  delivers patch 0004 to players for the first time.
- **One box the engine will not open stopped every other.**
  `ManagePlayerBotChests` returned false on a failed `UseItem`, so the first
  giftbox in the bag that cannot be opened - 50192 and 50193, six thousand
  refusals a minute between them - hid every Moonlight chest behind it: 587
  bots holding 9723 of them, stacks of 106, 190 opened in an hour, and bags
  only 29 cells of 90 full. A refusal now skips that box, is remembered by
  vnum for `PLAYERBOT_CHEST_REFUSED_RETRY`, and says so in the log; the pass
  also moved up into the upkeep group, because at the bottom of the tick it
  sat behind five subsystems that each claim the tick. The backlog cleared to
  887 in eight minutes.
- **A price keyed by vnum and refine cannot tell two pieces apart.**
  `GetPlayerBotShopAskingPrice` ran on the merchant table, the sale memory and
  `LimitPlayerBotAskStep` - all three keyed by that pair - so boots +7 with
  five bonus lines and boots +7 with none both asked 150 000.
  `GetPlayerBotBonusPricePercent` in `playerbot_bonus.h` adds a percentage for
  the lines and for the rolls a player stops on, and it has to be applied to
  every way out of that function: the flat +7/+8/+9 prices return early, and
  those are exactly the items people look at.
- **A launcher check must read the answer, not the exit code.** `docker info
  --format "{{.ServerVersion}}"` exits 0 while printing "Error response from
  daemon: Docker Desktop is unable to start" where the version belongs, and
  `Invoke-M2DiagnosticProcess` concatenates stderr into Output. The check
  reported that as "OK: Docker Engine odpowiada (wersja Error response...)",
  which also suppressed the WSL remedy - it is only raised when the engine is
  known to be down. A version is digits and dots.
- **Yang goes straight to the purse, for everybody, by patch 0010.**
  `CHARACTER::RewardGold` gave a kill's yang to the killer only with the
  premium or `IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT)` (72016..72018 on
  these serverfiles - not the 71010 an item shop sells); the bots wore and
  wound a 72018 for it (`ManagePlayerBotThirdHand`, a timed item whose
  `ITEM_SOCKET_UNIQUE_REMAIN_TIME` counts down while worn). Since 1.31.6 the
  patch makes `isAutoLoot` true for every killer, the same pass takes the
  Third Hand off every bot and removes it, and `char_battle.cpp` ships in
  `server-update-files.txt` like `char.cpp`. The operator's rule: most
  servers run this by default, and no bot slot is to be spent on it.
- **A snapped goal must stay inside the radius that tests arrival.**
  `MovePlayerBotTownLeg` asked for a sixteen-cell target snap and then
  checked arrival at 350 to 850 units. A goal behind a counter snapped
  further than the check, so the bot walked its route, arrived at nothing,
  cleared the route and planned the same one - and consuming a waypoint
  resets `bStuckCounter`, so the six-failure service rescue never fired.
  One bot stood at the Joan skill trainer for three days, reset by the
  inactivity watchdog every 90 s without taking a step. The snap is now a
  tenth of the arrival distance in cells.
- **Droppers are personalities, not roles.** `IsPlayerBotDropper` names the
  four; `GetPlayerBotPersonalityByPID` is how a rule that only has a
  character asks. The travel gates (`ShouldPlayerBotVisitM3`,
  `ShouldPlayerBotHuntM2Bestials`, `ShouldPlayerBotPursueHorseExpedition`)
  answer for them by level alone, the stall treats them as keepers, and the
  Metin dropper's books are never junk. Appending to the personality enum is
  safe; inserting shifts every id in the panel's status file.
- **A route a fight interrupts is parked, not dropped.** `ClearPlayerBotRoute`
  keeps a route with `PLAYERBOT_NAV_PARK_MIN_WAYPOINTS` left in
  `vecParkedRoute`, and `MovePlayerBot` takes it up again from the nearest
  waypoint the bot can walk straight to when the same destination is asked
  for on the same map. Before this a valley crossing to a hub 70 km away was
  planned from scratch after every fight along the way - a far plan costs
  150-250 ms, and there were 100-170 of them a minute, 20-29 s of every 60.
  `PLAYERBOT_LOAD` reports `resumed=`; `PLAYERBOT_NAV: far plan` names the
  destination of every plan over 1024 cells, which is how the pattern was
  found (the same hub from the same few hundred metres, every two seconds).
- **Measure before tuning a budget.** `CPlayerBotManager::Update` logs
  `PLAYERBOT_LOAD:` once a minute: tick time, plans by distance bucket with
  their cost, deferrals, target searches, snapshot, map scans, saves, watchdog
  resets. CPU alone said "A*" once and the fix put every bot's map scan in the
  same second; the line says which plans, and how many milliseconds each.

- **A transport horse comes off for a fight, not for the tick.** The wander
  pass mounts for a long leg at the bottom of the tick; the manager's
  "a normal horse is for transport only" dismount sat at the top of the next
  one and fired for any rider, and both halves clear the route. 1.30.28
  allowed the horse on wander legs and shipped that loop to everyone:
  133 000 mount/dismount pairs in twenty-eight minutes across 261 bots, six
  seconds of every sixty, and not one long leg walked - which is why the
  crowd raid never reached the Spider Queen. The dismount now needs a target
  or a victim, the target section climbs down on the tick it picks one
  (`dismount_for_target`), and the buff and multi-pull passes stay out of a
  transport saddle themselves (`CHARACTER::UseSkill` refuses every non-horse
  skill while riding).
- **A boss is looked for on the whole map.** `IsPlayerBotBossAlive` asked nine
  sectors round the hub point; the Spider Queen chases what hits her and was
  found five kilometres from her hub, "down" three minutes later with no
  BOSS_KILL in the log, standing again eleven kilometres away - and every
  raid was sent home while she stood. `SECTREE_MAP::for_each` over one
  snapshot of the map's entities, cached per race for
  `PLAYERBOT_RAID_BOSS_CHECK_INTERVAL`.
- **A book is kept for a skill that can read it.** `PLAYERBOT_BOOK_KEEP_PER_SKILL`
  (twelve) applied to every own skill, readable or not, and the bots at forty
  still had their skills in the teens: 2636 books in 929 bags, five read in
  three hours, four sold. `GetPlayerBotBookKeepLimit` gives the twelve only
  while the skill is at Master, `PLAYERBOT_BOOK_KEEP_UNREADABLE` before that,
  none at Grand Master; the surplus is counter goods, and merchant scrap only
  under bag pressure. The junk rule, the stall and the market all ask it.
- **A stone hunter is never sent where there are no stones.**
  `PlayerBotMapHasMetinStones` is false for the Spider Dungeon and the three
  Monkey Dungeons (no `stone.txt`); the frontier draw gives a Metin hunter by
  role Sohan instead of the Spider Dungeon, and an expedition is not rolled
  from or towards such a map. "Ide do Lochu Pajakow (cel: Metiny)" was a
  real status line.
- **The advanced panel's restart works without Seban's game-side helper.**
  1.38 of his panel gates every order of its restart console on a
  `server-settings.ready` heartbeat that only his `integration/` scripts
  write, and this image does not ship them. `queue_server_settings` lets a
  plain restart and a rates-only apply through `queue_rate_restart` (the
  path the game container has always watched) and refuses only a respawn
  change; an untouched respawn field arrives as `reset` for every map, so
  "no respawn change" means "nothing but resets". Its update button needs
  the `update-spool` volume mounted (compose) and the `updater` profile
  running; the request format (`id`, `version`) is our `m2-updater`'s own.

- **The fast build stages only the files it is told about.**
  `tools/fast-game-build/build.sh` docker-cps each argument into the builder
  and nothing else, so `build.sh playerbot_manager.cpp` after editing
  `playerbot_economy.h` compiles the new manager against the old header - it
  either fails ("not declared in this scope") or, worse, links cleanly with
  the header change missing. Three test deploys in one evening measured a
  horse fix that was only half there. Pass every changed fragment, or all of
  them: `build.sh $(cd linux-port/docker/game/src/server/game/src && ls playerbot_*)`.
- **A bot's stacks merge on a clock and split for the counter.** The engine
  merges two stacks only when a hand drags one onto the other (`MoveItem`:
  same vnum, every socket equal, two hundred to a stack), and a bot has no
  hand - partial purchases, partial sales and pick-ups into a full stack all
  leave a second stack behind. `ManagePlayerBotStackMerge` pours them
  together every `PLAYERBOT_STACK_MERGE_INTERVAL`, never behind an open
  counter; `SplitPlayerBotStallSingles` does the opposite for scrolls and
  soul stones before the lines are chosen, because a private shop sells a
  line whole. The split is idempotent - the stall scan re-runs on every tick
  of the walk to the pitch - and keeps `PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS`
  free for the bundle and the loot. Two stacks that differ in a socket will
  never merge for a player either; that is the engine, not the bots.

- **The walk does not own the goal.** Five world-travel legs (the desert
  crossing, the M3 graduation, the two frontier departures, the walk back
  to Joan) stamped `BOT_GOAL_LEVEL_UP` on every tick they ran, and the
  planner put its own answer back five seconds later: 350 bots on the
  frontier flipped "zapasy" <-> "poziom" for as long as a crossing took,
  twelve thousand of twenty thousand `PLAYERBOT_GOAL` lines, and a status
  that read "to town for supplies" and "to the Spider Dungeon" by turns -
  which players read as "the bots do not know what to do". Nothing read
  `dwGoalStartedTime`, and only the multi-pull and one gear rule ask for
  LEVEL_UP by name, so every `SetPlayerBotGoal` in `playerbot_travel.h`
  was removed - nine of them, and taking five left 1373 flips in nine
  minutes from the frontier tail alone; the planner decides, the walk
  walks. The fishing session was the same thing from the other side - it
  stamped FISHING every tick against the planner's RESTOCK - so the planner
  now defers to an active session (`bFishingSession`), the way it already
  defers to a committed town errand. Measure goal churn as pairs of
  `PLAYERBOT_GOAL` lines for one pid under eight seconds apart.
- **A level-30 weapon is rerolled until its average line, score or no
  score.** `PLAYERBOT_BONUS_KEEP_SCORE` is a sum of good lines, and a
  level-30 weapon full of them at twelve percent average passed it while
  every player looked at the one line that sells it.
  `PLAYERBOT_BONUS_KEEP_AVERAGE` is twenty (the Discord's number, and thirty
  is a roll most weapons never see); the worn one is rerolled past the
  score until finished, and the ones in the bag - goods - are worked on
  after it in the same pass, no unequipping needed. The advanced panel's
  weapon ranking was checked against raw rows: type 72 carries the average
  (to +46), 71 the skill damage, negative on this family - the two come as
  a pair, which is the "-5%" in the screenshots.
- **A farm is one map, so it takes a share of the population.**
  `ShouldPlayerBotVisitM3` said "everyone past thirty-five without the
  level-30 weapon" - right for a world whose bots are mostly fifty, and
  "sixty percent of the server in M3" with a map to prove it on a world
  whose bots are mostly thirty-six. `PLAYERBOT_M3_CROWD_SHARE_PERCENT` of
  `GetPlayerBotsAlive()` (never under `PLAYERBOT_M3_CROWD_MIN`) gates the
  door; a bot inside stays until the crowd is `PLAYERBOT_M3_CROWD_STAY_PERCENT`
  of the share, so the door does not flap. The same shape applies to any
  errand that names a single map.
- **A material errand is for a map the bot may hunt on.**
  `StartPlayerBotMaterialHunt` sent a level-61 Metin hunter, back in Bokjung
  for a weapon, after a Black Wind Yak-To for a refine material: seven
  minutes circling the spawns behind other bots before the Teleporter.
  It asks `IsPlayerBotGrindAllowedHere` and `lDepartureMap` first now.

- **A sign says what kind of counter it is.** Mostly books is a bookshop,
  mostly materials a smith's supplier, a mixed counter takes a market cry
  drawn by pid (`s_apszMarketCries`, "zobacz kotku co mam w srodku");
  the item-name signs stay for the goods people cross a market for. A
  poor keeper - under `PLAYERBOT_SHOP_POOR_GOLD` - opens with one line,
  at `PLAYERBOT_SHOP_POOR_DISCOUNT_PERCENT` of the asking price, under a
  "Wyprzedaz:" prefix; the discount is applied after the price is asked so
  the sale memory keeps learning the market's price, not the sale's.
- **A specimen is quest progress until its row is handed in, then goods.**
  `IsPlayerBotBiologistSpecimenSurplus` in `playerbot_missions.h`; the
  stall collector and the scorer both ask it. The Orc Tooth is also a refine
  material and takes the material branch and the ledger - the market, not
  the Biologist's doorstep, which is where the Discord wanted the surplus
  and where the buyers are. The soul stone is never surplus.

- **Joan's hubs are banded by measured monster level.** The 32 grinder hubs
  and 8 party camps in `playerbot_wandering.h` were picked by pid alone, so
  a bot of ten hunted tigers and a bot of twenty hunted dogs ("boty bija na
  9/10 lvlach nadal psy"). Each carries the median monster level within
  2500 units, measured from `regen.txt` through `group.txt` and
  `group_group.txt` with `mob_proto` levels (the parser shapes in "Engine
  facts" apply); `IsPlayerBotM1HubForLevel` admits a hub from two under its
  median to seven over, and the pid spreads the bot over the admitted ones.
  Re-measure before moving a hub; do not guess a band.
- **A keeper's status is "Prowadze stragan", never "Planuje: poziom".** The
  overhead sign is what the world sees, but `playerbot_status.tsv` is what
  the panel and an operator see, and thirty-nine keepers at the Joan ring
  read as thirty-nine idle bots in the safe zone.

- **A silent `continue` in the tick is a bot that stands for good.** Twelve
  archers stood at arrival points for twenty minutes, reset by the watchdog
  every ninety seconds, ticked and logging nothing. The tick left through
  `!PrepareWeapon` -> `StartPlayerBotTownVisit` -> `continue`, and a town
  visit cannot start on a frontier map. Underneath: `CountPlayerBotArrows`
  counted arrows the bot could not nock (a progression chest hands 8003 -
  level forty - to a bot of thirty-four), so `NeedsPlayerBotArrows` said no,
  `BlocksPlayerBotTravel` said no, and nothing ever sent it to town.
  `IsPlayerBotUsableArrow` counts by level limit now, and off Joan/Bokjung
  that branch hands the tick to the world travel and then the wander. The
  watchdog line carries `equip_pending/service/riding/nav_out/wander_in` so
  the next silent exit can be named from the log; `nav_out=0` means the walk
  was never even asked.
- **A shield slot is not a slot for a bow.** `NeedsPlayerBotCriticalTownServices`
  and the manager's `bMissingCoreWearSlot` counted `WEAR_SHIELD == NULL` for
  every archer, so an archer was critically short of town services for life:
  out of M3 the moment it arrived, back by the weapon hunt fifteen seconds
  later. `PlayerBotWantsShield` is false for a bow or a two-handed weapon.
- **A dropper's counter is not a bag without a bottom.** The Metin dropper
  kept every book for its stall, and a dropper whose stall roll never came
  kept eighty of them and picked up nothing. Under bag pressure a dropper
  opens a stall whatever the roll, and past `PLAYERBOT_DROPPER_BOOK_KEEP`
  the merchant takes the rest; another class's books go to the merchant
  under pressure for everybody.

- **The market is a ledger, not a shelf.** `RefreshPlayerBotMarketLedger`
  (`playerbot_market.h`, once a minute from the tick) counts the units on
  every open counter and the bots short of each material with money to buy
  it. `DecidePlayerBotMaterialListing` in `playerbot_town.h` lists a
  material only while supply is under 1.5 x 5 x demand, allows one probe
  stack when nobody is short, and logs `PLAYERBOT_MARKET: held ... reason=`
  for the rest. `GetPlayerBotShopAskingPrice` blends the prior with the
  sale median by n/(n+4) in log space, applies ((D+5)/(S+5))^0.2 within
  [0.75, 1.35] for materials, and `LimitPlayerBotAskStep` lets a price move
  five percent per ten minutes. The prior is the merchant's price times
  three or a share of the median shopping wallet, whichever is more -
  the merchant pays pennies and the bots hold millions. `PLAYERBOT_MARKET:
  ledger` every ten minutes is the report. A human buying from a counter
  is invisible to all of it: the purchase goes through CShopManager and
  never reaches this code.
- **A counter line is a grid slot and an item id, not an index and a vnum.**
  `CShop::SetShopItems` lays a private shop out on a five-column grid by
  item height (a weapon is three cells, an armour two) and silently drops a
  line whose `display_pos` is already covered ("not empty position" in
  syserr); `CShopManager::Buy` indexes by that position. Numbering the
  lines 0, 1, 2 lost every second-row line under a weapon, and buyers were
  refused at empty slots with a hacker log line: 2158 refusals to 501
  purchases in one hour. `FindPlayerBotShopSlot` places lines the way
  `CGrid::FindBlank` does and `TPlayerBotShopOffer::bSlot` carries the
  result; `dwItemID` and `FindPlayerBotOfferItem` (the item exists and the
  keeper still owns it - the engine's own test) say whether the line is
  still for sale, because a sold stack's vnum is often still in the bag as
  a second stack. The buyer re-reads the counter on the tick it arrives,
  whatever the browse clock says.

- **A book is read whenever there is one, unless the panel says otherwise.**
  The engine puts SKILLBOOK_DELAY_MIN..MAX (eighteen to thirty hours)
  between two reads of one skill; `ManagePlayerBotSkillBooks` waves that
  away with `SetSkillNextReadTime` while the `BOOKS` key in the weights file
  is on (the default), and imposes no wait of its own.
  What still paces a skill is `CHARACTER::LearnSkillByBook` and is not ours:
  20 000 experience taken per read, a roll on each one, and a number of
  successful reads per master level. Nor is the wait what limits it in
  practice - of 480 bots with a skill at Master, 326 carry books and 44
  carry a book of that skill, so the supply of the right book is the
  bottleneck and always was. A bot keeps
  `PLAYERBOT_BOOK_KEEP_PER_SKILL` books of each own skill and the rest are
  goods; a keyless treasure chest is junk once the bag is down to
  `PLAYERBOT_BAG_PRESSURE_FREE_CELLS`. The Moonlight chest opens by itself
  but only into a free cell, which is what a bag full of books and chests
  had stopped.
- **The chat reaches the bots through patch 0007, and only there.**
  `CInputMain::Chat` hands a player's shout to
  `CPlayerBotManager::OnPlayerShout` after `SendShout`, and
  `CInputMain::Whisper` hands a whisper addressed to a bot to
  `OnPlayerWhisper` instead of a descriptor with no client; `shop.h` gets
  `GetItemVector()` so a bot can read a player's counter and buy from it
  (capped at `PLAYERBOT_MARKET_STACK_WALLET_PERCENT` of the median wallet
  per line). The staged `input_main.cpp` and `shop.h` ship in
  `server-update-files.txt` like 0006's files. A bot's own shouts go out
  through `SendShout` directly and never come back through the hook. Text
  is CP1250; `FoldPlayerBotChatText` is how names are compared.

- **Hunting stones is a state, not only a role.** `IsPlayerBotMetinHunting`
  in `playerbot_types.h` is what the planner, the target scoring and the
  wandering ask; it is true for the hunter role for life and for any other
  bot during a Metin expedition (`RollPlayerBotMetinExpedition` in
  `playerbot_planner.h`: an hourly roll, `PLAYERBOT_METIN_EXPEDITION_*`, the
  METIN weight scales the chance). A rule that tests `bBotRole ==
  BOT_ROLE_METIN_HUNTER` directly is a rule the expedition does not reach.

- **A status line that names an errand must ask whether the errand is
  possible.** "Ide do najblizszego Stajennego z Medalem" fired for any
  travelling bot with a medal in the bag, and a medal a bot cannot hand in
  stays there for hours: a horse at ten waits for level thirty-five
  (`GetPlayerBotNextHorseRequiredLevel`), and the medal dropper carries them
  for its counter. Reported as "a bot at horse ten shouts »ide do stajennego«
  and rides in circles for an hour"; it was riding its ordinary legs. The
  line asks `CanPlayerBotAdvanceHorse` now, the battle-horse trial has its
  own words, and the stable status measures against the stable of the map
  the bot is on - Bokjung's hand-in used to read as a walk to Joan.
- **Night is the Christmas flag on a clock.** The engine has no day cycle;
  `xmas_snow` is what a GM raises with `/xmas_snow 1`, and the client
  answers it with the night sky and snow. `ManagePlayerBotNight` in
  `playerbot_config.h` raises it between `PLAYERBOT_NIGHT_START_HOUR` and
  `PLAYERBOT_NIGHT_END_HOUR` of the container's local time (`M2_TZ`) and
  lowers it outside them, once a minute and only when the flag disagrees,
  while the `NIGHT` key in the weights file is on. `RequestSetEventFlag` is
  a round trip through the DB core - the game's own `GetEventFlag` moves
  only when the broadcast comes back - so the check compares intent with
  the flag, never with what it last asked for. With the switch off the flag
  is left to the GM, except a night this clock raised, which it lowers once.

- **A waypoint the bot is standing on cannot be walked to.** The
  consumption loop kept the current waypoint when the segment to the next
  one was obstructed from the bot's exact point - right when the bot is
  near the waypoint, wrong when it is *on* it: `CHARACTER::Goto` refuses a
  destination equal to the position, the walk called a refused Goto within
  arrival distance "moved", and the bot stood on its own first waypoint
  for good. Sixteen of eighteen watchdog resets in an afternoon were
  `nav_out=11 route=0/2` on a cell centre, one of them twenty minutes at
  the Joan blacksmith. A waypoint under the bot's feet is consumed; the
  obstructed segment then goes through the blocked-segment branch, which
  has the rescues and counts the failure. `nav_out=11` in a watchdog line
  means exactly this shape.
- **A skill book never goes to the merchant.** `IsPlayerBotJunkItem` says
  so outright now; the surplus (`IsPlayerBotSurplusSkillBook`: another
  class's, or its own past `GetPlayerBotBookKeepLimit`) is counter goods,
  and what the bag cannot hold past `PLAYERBOT_SAFEBOX_BOOK_KEEP` of those
  goes to the storekeeper. The safebox is opened the way the Dozorca's
  quest does it - `SetSafeboxOpenPosition`, `ReqSafeboxLoad("000000")`
  (the DB accepts that for an account that never set a password) - and
  the answer comes back from the DB core on a later tick, so
  `BOT_TOWN_PHASE_SAFEBOX_WAIT` deposits when `GetSafebox()` is set and
  `CancelSafeboxLoad`s after `PLAYERBOT_SAFEBOX_LOAD_WAIT_MS`, or the next
  request is refused as overlapped for ever. Books put in are never taken
  out; a full page leaves the rest in the bag as goods. **The safebox is
  keyed by the descriptor's account id**, and `CreateBotDesc` left it at
  zero: the first test put every bot's books into one box under account 0.
  `LoadRegisteredBots` now reads `a.id, a.login` with the pid and
  `SpawnBot` writes them into the bot's `TAccountTable` - anything else
  the engine keys by account (the login log, `safebox.account_id`) had
  been seeing zero too.
- **The Forgetting Scroll has no source in this world.** No shop row, no
  drop table carries 70037, so "a scroll from the market" past the old
  woman's thirty meant a skill at seventeen for life - 81 bots carried
  eighteens and nineteens from before the cap and nothing could move them.
  `BuyPlayerBotForgetScroll` creates one for `PLAYERBOT_SKILL_FORGET_SCROLL_PRICE`
  above `PLAYERBOT_SKILL_RESET_MAX_LEVEL`, socketed with the skill, and it
  is read on the spot; the engine's roll is `1/(21-level)` at every point
  from seventeen, so a point at seventeen is a quarter, at twenty a
  certainty.
- **Hwang's boss is a party's raid twenty levels up.** boss.txt group 2110
  at cell (374,420), every two hours: the Yellow Tiger Spectre (1304, level
  75, 178 040 hp) with two Frog Generals and two Tree Frog Chiefs. A solo
  target is capped at `PLAYERBOT_MAX_TARGET_LEVEL_DELTA` over the bot;
  only a party leader's `iChallengeMaxLevel` (highest + 5 per member,
  capped by half the total) reaches him, so the hub row is `bNeedsParty`
  like the Queen's and Nine Tails'. The Demon Tower entrance has no spawn
  within 2500 units and gets no hub; the middle, the south-east corner and
  the frog field north of the entrance do, from a regen measurement that
  found 800-1200 spawn points nineteen kilometres from any hub.

- **A town goal on ground the bot's terrain does not join is walked to
  nowhere.** Bokjung's misc merchant approach point sits on a strip of
  `server_attr` cut off from the square; every bot coming from the armour
  merchant planned it, was told "unreachable" three times, and was
  relocated by the service rescue on the sixth failure - 260 rescues in a
  morning. `MovePlayerBotTownLeg` asks `CanReach` first and walks to the
  nearest cell of the bot's own component inside the arrival radius
  (`PLAYERBOT_TOWN: goal moved onto reachable ground`). The rescue stays
  as the net for the cases a component lookup cannot see.
- **A counter line is a pack, and the keeper is whoever has the books.**
  `GetPlayerBotStallLineUnits` says how many units of a stackable make a
  line - one for what a player buys singly (USE, METIN, the pearls), two
  for a material - and `SplitPlayerBotStallSingles` cuts up to
  `PLAYERBOT_SHOP_PACK_LINES` of them before the lines are chosen. Closing
  the counter schedules a merge in seconds, and a merge pass that used
  its whole budget comes straight back. `ShouldPlayerBotKeepShop` is true
  for any bot holding `PLAYERBOT_SHOP_BOOK_PRESSURE_MIN` surplus books,
  bag pressure or not: a thousand bots made
  twenty-one stalls with a few books between them while the roll picked
  one in ten and the books rode round the stones with the other nine.

- **The stall pass runs before the world travel, so what it walks to
  pre-empts where the bot was going.** With Bokjung's ring full
  (`PLAYERBOT_SHOP_M2_MAX_STALLS`, seven) it walked every keeper with goods
  to the M1 portal to open in Joan - harmless while a keeper was one bot
  in ten, and a loop for hundreds once every bot with six surplus books
  became one: "a bot that wants Sohan heads for the M1 portal, turns back,
  circles M2". Only `IsPlayerBotStallKeeper` personalities with no
  `lDepartureMap` take that walk; everyone else backs off for
  `PLAYERBOT_SHOP_RING_FULL_RETRY`. A rule that makes more bots eligible
  for a pass has to be checked against everything that pass does.
- **A grazed corner is walked, not replanned.** When the bot stands on a
  cell centre, the next waypoint is a cell centre, the corner after it is
  not in reach, and the live supercover still calls the segment blocked,
  no replan will ever answer differently. The third rescue in the
  blocked-segment branch issues the Goto anyway (`PLAYERBOT_NAV_OUT_FORCED`,
  "forced through a grazed corner"): the server moves a bot in a straight
  line with no collision. Consuming the own-cell waypoint (1.30.34) turned
  the silent stand at these corners into a visible turn-back at the Monkey
  Dungeon exit; this is the other half of that fix.

- **A rod is not one vnum.** `fishing.cpp` rolls on every catch and turns
  the rod into its `GetRefinedVnum` - a new item - so `CountSpecifyItem(27400)`
  said "no rod" to a bot whose Wedka+2 lay in the bag, and it bought one per
  session until the bag was full of them. `CountPlayerBotRods` counts
  `ITEM_ROD`; `EquipPlayerBotRod` takes the highest vnum; a rod that another
  rod matches or beats is junk.

- **The database's half of the context is five files, not a directory.**
  `Test-ContextComplete` in the installer tested that `mariadb/initdb.d/dumps`
  existed and the launcher never looked; MariaDB initialised an empty world
  behind a green healthcheck and `playerbot-migrate` waited thirty minutes
  for a schema that could not appear, with the only honest line in the
  MariaDB log. `Get-M2MissingSqlDumps` (module) names the missing dumps;
  `start-server.ps1`, the launcher's click path and the GUI's package check
  all ask it before `docker compose up` - both build paths, per the guard
  note above - and skip the refusal only when the DB volume is already
  initialised, because initdb.d runs once and never again. The migrate
  loop now tells "answering with none of the tables" apart from "import in
  progress" and "not answering yet".

- **A full bag is an errand.** `IsPlayerBotBagFull` (occupied cells at
  `PLAYERBOT_BAG_FULL_PERCENT`) is read by the two drop expeditions
  (`ShouldPlayerBotPursueHorseExpedition`, which the Monkey Dungeon's exit
  decision and the planner both consult, and `ShouldPlayerBotVisitM3`), by
  `ShouldPlayerBotKeepShop` (a counter whatever the roll, one line is
  enough) and by the safebox collector. `NeedsPlayerBotCriticalTownServices`
  already sent a bot to town at 45% occupancy; what it found there was a
  visit that scrapped nothing, because a collector's spares are goods, so
  the errand has to be the counter and the storekeeper, not the merchant.

- **A gate is where the warp NPC stands, not where npc.txt said.**
  `FindPlayerBotWarpNpc` walks the map's entities for `IsWarp()` characters,
  reads the destination out of the name the way `FuncCheckWarp` does
  (`"%s %ld %ld"`, cells, absolute), resolves the map with
  `SECTREE_MANAGER::GetMapIndex(x, y)` and hands `MovePlayerBotToWorldPortal`
  the nearest one to the point it was asked for, cached per (map, target)
  for `PLAYERBOT_WARP_NPC_CACHE_MS`. The Teleporter is a quest NPC, not a
  warp, so `IsPlayerBotTeleporterPoint` keeps the constant and the trip
  pays `GetPlayerBotTeleporterFee` (map_warp.quest: floor(level/5)*1000,
  at least 1000, level 11+) on a successful transition. "Portal walk
  stalled" goes to syserr too - support bundles carry only syserr.

- **A bot's gear history is `log.log`, and the core has to write its half.**
  The engine logs refines (`REFINE SUCCESS`/`FAIL`, `REMOVE (REFINE FAIL)`
  for a burn), the buyer's `SHOP_BUY`, and every `ITEM_MANAGER::RemoveItem`
  reason as `how` - which is where `PLAYERBOT_SHOP_SELL` and `PLAYERBOT_BONUS`
  come from. What it never saw is what a player asks about: the swap into a
  wear slot, a gift, the keeper's side of a stall sale, our own safebox
  deposit. `LogManager::instance().ItemLog` writes those (`PLAYERBOT_EQUIP`,
  `PLAYERBOT_GIFT_OUT/IN`, `PLAYERBOT_STALL_SOLD` once per line via
  `TPlayerBotShopOffer::bSoldLogged`, `SAFEBOX PUT`); the classic panel's
  `/api/bot_gear_history` reads them by `who` with an IN-list of `how`,
  because the table holds eighteen million rows and GET/SET_SOCKET/GET_GOLD
  are most of them.

- **A weapon's percent lines multiply its own damage, and a better weapon
  needs a window to go on.** `GetPlayerBotEquipmentScore` scaled attack at a
  thousand a point and added `APPLY_NORMAL_HIT_DAMAGE_BONUS` at 250-500 a
  point beside it - a +47% line on a 151-244 bow was worth two percent of
  the bow. On a weapon those two lines now multiply the attack score by
  `100 + avg*weight + skill*weight` (`PLAYERBOT_WEAPON_OWN_LINE_PERCENT` /
  `_OTHER_LINE_PERCENT` by `GetPlayerBotSchoolStyle`), and the flat loop
  skips them. Separately: `CHARACTER::EquipItem` refuses within 1.5 s of an
  attack or a cast, the manager only paused combat for an *empty* core slot,
  and 247 of 970 bots carried a weapon a third stronger than the one in
  hand. The pause now covers any `bEquipPending`, bounded by
  `PLAYERBOT_EQUIP_PENDING_MAX_MS` and retried after
  `PLAYERBOT_EQUIP_PENDING_RETRY_MS`. And the pass itself had to move: at
  the bottom of the tick it sat behind the stall, the loot, the horse, the
  fishing, the travel, the town visit and the wander, each of which claims
  the tick, so a bot always busy with one of them never read its bag at
  all (a warrior of 28 swinging the level-one sword +6 with a Long Sword +4
  beside it). It runs in the upkeep group now, the same shape as the chest
  pass above, guarded against an open counter, a town visit, a rod in the
  hand and the stable; `HoldPlayerBotForEquipWindow` is the shared wait.
- **Every failed refine destroys the item.** `CHARACTER::DoRefine` has one
  failure branch, `RemoveItem(item, "REMOVE (REFINE FAIL)")`, at every
  grade; only `DoRefineWithScroll` under a Blessing Scroll (25040) hands
  the item back a level down. `refine_proto` runs 90/90/90/90/80/60/50/40/30
  from +1 to +9. `IsPlayerBotPrizeWeapon` (average from
  `PLAYERBOT_BONUS_KEEP_AVERAGE`, skill from
  `PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT`) is refined only under a scroll
  when `prt->prob < 100`; no NPC shop sells the scroll, so it comes from
  drops and chests and 222 sat in bags. `IsPlayerBotPrizeItem` widens that
  to any piece with `PLAYERBOT_PRIZE_LINES` lines, and `CanPlayerBotRerollItem`
  refuses a stone below `PLAYERBOT_BONUS_MIN_REFINE` - lines before the
  refine are lines a burn takes with it.
- **An Archer breaks a Metin with a dagger, and the fight asks the hand.**
  A bow cannot break a stone (Kuszaa: "pada na glebe x razy i rezygnuje"):
  the stone stands still, the arrows run out, and the shot's rhythm is a
  fraction of a swing's. `IsPlayerBotArcherBuild` (job and skill group, not
  the weapon - `IsPlayerBotArcher` in `playerbot_targeting.h` asks for the
  bow and the lure needs that) plus `bMeleeForStone`: `ManagePlayerBotEquipment`
  flips it on when the target `IsStone()` and `FindPlayerBotStoneWeapon`
  finds a dagger or sword (a dagger first, whatever the score), and the
  candidate loop then scores the bow in hand as nothing. Every combat path
  judges by the weapon in the hand, so a dagger swings and the bow shoots
  without a second damage path; the one exception is the attack-skill
  rotation, which must refuse an Archer without a bow - every Archer skill
  is `SKILL_FLAG_USE_ARROW_DAMAGE` and `ComputeSkill` sets `atk` to 0 without
  one. `PrepareWeapon` asks `PlayerBotWeaponFitsNow`, or it would unequip the
  dagger as a profession mismatch on the next tick. The junk rule and the
  stall keep the chosen stone weapon; the weapon merchant sells a dagger of
  the bot's level when the bag has none.
- **A portal walk asked for once is a route somebody else finishes.**
  `MovePlayerBotToWorldPortal` plans the route and makes the map change only
  when the pass that called it calls it again within
  `PLAYERBOT_PORTAL_SWITCH_DISTANCE`; in between, the odd-tick continuation in
  the manager and the wander's route continuation walk the route to its last
  waypoint and hand the bot to the wander. The shopping pass asked for the
  Joan gate once per `PLAYERBOT_SHOPPING_INTERVAL` ("Joan first") and never
  again: 152 of 160 walks to the Bokjung gate in ten minutes, one crossing,
  and a crowd of bots with "Sohan" or "Loch Malp" over their heads riding up
  to the gate, climbing down, and riding off (Kuszaa's video, twice). Any
  caller of the portal walk must be a state that re-asks every tick until
  the map changes - `bMarketToJoan` here, `dwStallWalkUntil` for the stall,
  the travel pass by construction. `bRouteKeepsHorse` is the other half: a
  continuation pass passing `keepHorseAtDestination=false` dismounted the
  rider a kilometre short of a gate the walk meant to ride through. The
  diagnostic that found it was three throttled lines - who reaches the
  travel hook at the gate, which branch refuses, and who asked for the
  portal from where - and it is worth putting back before guessing again.
- **A fare the bot cannot pay is a hunt it must be allowed.** The Teleporter's
  refusal set `dwNextWorldTravelTime` since 1.30.42 and the M2 frontier
  branch never read it, so the wait announced in the changelog was a wait of
  one tick: 26 000 refusals a minute across the cohort. Underneath, 268 of
  362 bots of 40+ in Bokjung held less than one fare (79 yang the poorest),
  because a bot back from the frontier for services spent everything at the
  blacksmith and above the cohort ceiling `IsPlayerBotGrindAllowedHere` said
  no - so it could neither pay nor earn. `GetPlayerBotReservedGold` keeps
  `PLAYERBOT_TELEPORTER_FARE_RESERVE_COUNT` fares for any bot whose
  `GetPlayerBotFrontierMapForLevel` is not zero (the fare estimate lives in
  `playerbot_battle_horse.h` because every spender is included before
  `playerbot_travel.h`), the grind rule makes an exception for a bot short
  of that, and the frontier branch neither sends a bot short of the fare nor
  one inside the refusal's wait. Measure with `teleporter refuses [+N more]`.
- **An item nobody owns is everybody's, and that is the policy.** `CItem::IsOwnership(ch)`
  returns true for any character once the ten-second ownership event is
  gone, so a bot picks up a player's leftover drop like any player would.
  1.31.4 restricted free items to the bot that saw them while owned (a
  probe-pid trick, because nothing public says whether the event is alive);
  the operator reverted it in 1.31.5 - a bot taking what a player left is
  wanted. Do not put it back without asking.
- **A rider is served at every counter.** The engine refuses a rider only a
  skill book (`LearnSkillByBook`), a costume and a second mount; the shop,
  the blacksmith, the storekeeper and every quest NPC answer from the
  saddle. `MovePlayerBotTownLeg` and the Biologist keep the horse
  (`keepHorseAtDestination`), the book pass dismounts itself, and the
  fishing session sends the horse away with `HorseSummon(false)` because
  `StopRiding()` alone parks it beside the angler for the whole session.
- **Dead stock is counted by item id across stands.** `mapStallUnsold` in
  the state: `ClosePlayerBotShop` adds a stand to every line that came home
  (`FindPlayerBotOfferItem` still finds it), the open pass takes
  `PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT` per stand off the asked price
  (after the sale memory has seen the real price), and the junk rule vendors
  gear under `PLAYERBOT_PRECIOUS_REFINE` after `PLAYERBOT_SHOP_UNSOLD_SCRAP_STANDS`.
  The map is pruned against the bag past sixty-four entries.
- **A base image tag moves under you.** `php:8.2-apache` became trixie in
  2026 and trixie's apt verifies InRelease with sequoia, which failed on a
  player's Docker Desktop and cancelled the whole compose build ("target
  itemshop: failed to solve"), leaving him on the old server with no way to
  update. Pin every Dockerfile to a Debian codename (`-bookworm`) and run
  no `apt-get` where nothing needs a package - the ItemShop's healthcheck
  asks PHP itself now.
- **"Finished" has to mean the same thing for every weapon.**
  `HasPlayerBotFinishedBonus` called a weapon finished by its average line
  only for the level-30 family, so a bow of forty-five with a 40% average
  was rerolled towards `PLAYERBOT_BONUS_KEEP_SCORE` until the average was
  gone - 37 000 rerolls a day on this world, and "boty zmixowaly wysokie
  srednie 35+" on the Discord. Any weapon at `PLAYERBOT_BONUS_KEEP_AVERAGE`
  is finished now, and a level-30 weapon from `PLAYERBOT_SCROLL_REFINE_MIN_PLUS`
  waits for a scroll whatever its lines.
- **A skill priority is worth nothing to points already spent.** The
  players' order (`ApplyPlayerBotSkillPriority`) only steered new points, so
  a bot with sixteen in the third skill kept them for good.
  `ReallocatePlayerBotSkillPoint` moves one point per
  `PLAYERBOT_SKILL_REALLOCATE_INTERVAL` from the lowest-ranked skill above
  its unlock point to the highest-ranked one short of Master, with a
  Forgetting Book bought at `PLAYERBOT_SKILL_REALLOCATE_PRICE` -
  `SkillLevelDown` refunds the point and refuses a skill at Master, which
  is why Master skills stay where they are. Only when there is no free
  point: a free point goes to the same place for nothing.
- **The town crowd is deliberate, and it is not capped.**
  `PLAYERBOT_TOWN_LINGER_PERCENT` is 100 and its comment does the arithmetic
  for the angler trigger alone - a session ends about once a minute, so
  "three or four bots on the square". The same linger is also set by every
  completed town visit in Joan, and those run twenty-five a minute: measured
  32-48 bots in `BOT_ACTION_TOWN_REST` at every moment and 74 different ones
  in three minutes, which is the crowd players photograph ("Bots just running
  in Safe Zone", twice). 1.31.8 briefly capped it and the operator reverted
  the cap along with the Bokjung stall cap: a town is meant to fill up, and a
  keeper refused a pitch is a bot with nothing to do. So the number on the
  square is a feature - what was actually wrong there was the horses. Do not
  add a cap back without asking.
- **A dismount in a town square parks a horse there.** `StopRiding()` summons
  the horse as a follower, so 295 dismounts on M1 in a quarter of an hour
  left 295 horses standing in Joan - the herd in every screenshot of the
  square. `SetPlayerBotRidingForTravel(false)` sends the horse away with
  `HorseSummon(false)` inside `IsPlayerBotSafeZone` and nowhere else, because
  on a hunting map the bot wants it back in a minute. `StartRiding()` does
  not need the horse summoned, so nothing else has to change (the stall
  opener has done this since it was written).
- **A boss blinks to its victim's map, not its own.** `char_state.cpp`, the
  BOSS branch: race 2191 (the desert's Giant Turtle) rolls one in twenty and
  calls `Show(victim->GetMapIndex(), new_x, new_y, 0, true)`, so a victim that
  changed map in the meantime - a warp NPC, the Teleporter, or our own
  server-side `TransitionPlayerBotMap` - drags the boss onto the new map. That
  is how a desert boss appears in Bokjung, for players as much as for bots.
  Patch 0011 gates the whole boss branch on `GetVictim()->GetMapIndex() ==
  GetMapIndex()`, which also stops `__CHARACTER_GotoNearTarget` walking the
  boss towards coordinates that belong to a map it is not on.
- **A dry run of one patch is not a dry run of the series.**
  `prepare-context.sh` rehearsed every engine patch separately against the
  untouched tree, and 0009's first hunk carries the `#include
  "playerbot_manager.h"` that 0001 adds as context - so it failed alone and
  applies perfectly in sequence. Every Linux and VPS install stopped there
  from 1.31.0 on and could not update; Windows never saw it, because the
  launcher stages already-patched files. The rehearsal copies the files the
  series touches into `mktemp -d` (outside the build context, or it ships in
  the image) and applies the whole series there for real, so a dependent
  patch passes and the real tree is still all-or-nothing. Proven both ways
  on a two-patch case with the same dependency.
- **MyISAM is what this game runs on, and it does not survive a kill.**
  73 of 75 tables; one unclean stop marks a table crashed and every reader
  fails from then on. MariaDB's default `myisam_recover_options=BACKUP,QUICK`
  only rebuilds the index file, so a damaged data file ends as "last
  (automatic?) repair failed" - which is what a player saw through the panel.
  `99-metin2.cnf` asks for `BACKUP,FORCE`. To repair an install that is
  already in that state: `mysqlcheck --auto-repair --check --all-databases`.
- **The three kingdoms are mirrors in shape and nothing else in coordinates.**
  `map/index` gives M1/M2/M3/easy as 1,3,4,5 (Shinsoo), 21,23,24,25 (Chunjo)
  and 41,43,44,45 (Jinno) - M3 is the guild map, which is what our
  `PLAYERBOT_MAP_CHUNJO_M3 = 24` already meant. Measured: each M1 carries
  ~9200 spawn points of level 1-31, each M2 ~6000 of 18-36, each guild map
  ~300 of 8-24, and the three easy dungeons are the same 1404 points of
  22-30 with identical `server_attr`. But every town is laid out differently
  and the Teleporter (NPC 9012, on all six village maps) lands each kingdom
  on its **own** point of a shared map - `map_warp.quest` holds a table
  indexed by empire. So a Shinsoo or Jinno number is never Chunjo's plus an
  offset. `tools/dump_world_catalog.py` reads all of it out of the game's
  files; it reproduces four constants the AI has been using for months
  (both Teleporters, the desert and the valley arrival) to the unit, which
  is what says the reader agrees with the engine.
- **The registry says which kingdom a bot belongs to; the caller does not.**
  `LoadRegisteredBots` reads `pi.empire` with the row and keeps it per PID, and
  `CPlayerBotManager::Spawn` takes the empire from there - an argument that
  disagrees is refused with a line in syserr. That is what stops a stray call
  starting a seeded character into somebody else's kingdom, and it is why
  `SpawnRegistered` can be asked for one kingdom at a time.
  `CountRegisteredPerEmpire` **loads the registry itself**: the bootstrap asks
  it for the counts before it asks for any spawn, and the first version left
  that out - the core came up with no bots at all and not one PLAYERBOT line
  in the log, because the split had nothing to divide.
- **Each core starts the kingdoms whose village it hosts.** `m2-render-config`
  puts Shinsoo's four maps (1,3,4,5) on `first`, Chunjo's plus every shared map
  on `game1`, and Jinno's (41,43,44,45) on `game2`, so a kingdom's whole local
  life fits inside one process and needs no transfer. The bootstrap in
  `input_db.cpp` therefore loops the three kingdoms and asks `map_allow_find`
  for each village, instead of naming map 21; the operator's one number is
  split by `playerbot_empire_rules::SplitPopulation` between the kingdoms that
  have identities, so with only Chunjo seeded it all still goes to Chunjo.
  Measured after the change: all three cores load the registry, only game1
  spawns, 970 of 970 asked for, 968 in the world a minute later.
  `TopUpMissingBots` counts the world against `m_setScheduledBots` - exactly
  what this core asked for - and not against the first N of a registry that
  now holds three kingdoms.
- **The seed carries the kingdom, and the two new ones are opt-in.**
  `generate_seed.py` renders one canonical cohort in PID order:
  Chunjo's original 1500 (PID 4..1503, unchanged to the byte), then 500
  Shinsoo (1504..2003, map 1) and 500 Jinno (2004..2503, map 41). The spec
  table carries `empire` and `map_index`, every `pi.empire = 2` in the SQL
  became `= s.empire`, and the character insert takes its village from the
  spec. `@playerbot_seed_kingdoms` (set by apply.sh from
  `M2_PLAYERBOT_KINGDOMS`, default 0) deletes the non-Chunjo rows from the
  spec before anything is validated, so a player's server keeps the cohort it
  has until the operator asks for more. Proven on the test database: the
  identity fingerprints of the 1500 Chunjo characters and their accounts are
  byte-identical across two runs with the switch on and one with it off, and
  the second run changes nothing at all.
  The spawn grids were picked by walking `server_attr`: 500/500 points stand
  on open ground in both new villages (of Chunjo's 1500, 153 do not).
- **apply.sh moves a stranded bot to its OWN kingdom.** Its allow-list is what
  decides where a bot may be parked at start, and it named only Chunjo's maps -
  every Shinsoo and Jinno bot would have been teleported to Bokjung on every
  start, into a town with none of its services. The list now holds all twelve
  kingdom maps and the fallback is a CASE on `pi.empire`; Chunjo keeps the
  exact point it always used.
- **The navigation grid is built per map, and it refused eight of the twelve.**
  `CPlayerBotNavigation::Init` named Chunjo's three maps explicitly, so a bot
  on map 1 could not plan a single step. It asks
  `playerbot_empire_rules::IsKingdomMap` now; the shared maps stay named one
  by one, because only some of them are ours to walk.

## Engine facts worth not re-deriving

- Item types/subtypes live in `common/item_length.h`; map attributes and
  `SECTREE_SIZE`/`CELL_SIZE` in `game/src/sectree.h`.
- Map world coordinates: `world = BasePosition + cell * 100`, with `BasePosition`
  from the map's `Setting.txt` and `cell` from `npc.txt`. Map 21 is `metin2_map_b1`.
- `server_attr` is per-sector lzo1x: `int32 width, height`, then per sector a
  `uint32` size and a block expanding to 128×128 `uint32` attributes, one per
  50 world units. `ATTR_BLOCK = 1<<0`, `ATTR_WATER = 1<<1`. Decoder and a terrain
  dump: `tools/decode_server_attr.py`.
- **`ATTR_WATER` is terrain, not a wall.** The engine tests it in exactly one
  place -- whether there is water in front of a fishing rod -- and never for
  movement. A river carries `BLOCK|WATER`; a bridge deck or a ford carries
  `WATER` with the block bit cleared, and that is how a map says "cross here".
  Orc Valley is 23 islands joined by 22 such crossings, so the navigation
  treating water as blocking left every bot on the island the entrance opens
  onto: 17.6% of the walkable ground, 161 of 532 spawn groups. It blocks on
  `ATTR_BLOCK|ATTR_OBJECT` and charges `PLAYERBOT_NAV_WATER_PENALTY` per wet
  cell instead, so a bridge is used and the desert shallows are walked round.
  `tools/analyse_map_bridges.py` measures this for any map.
- **Reading a spawn file: `group.txt` and `group_group.txt` are not the same
  shape.** In `group.txt` a member line is `<idx> "<name>" <mob vnum>` and the
  mob is the **last** field; in `group_group.txt` it is `<idx> <group vnum>
  <probability>` and the group is the **middle** one. Taking the last field of
  both - which reads a probability as a group id - makes every `r` line in every
  regen.txt resolve to nothing, and the map then looks empty. It is not: Orc
  Valley alone has 4041 spawn points over 32 mob types. A regen line's type is
  its first field, `m`/`b`/`e` naming a mob directly, `g` a group, `r` a
  group_group; the id is the last field of that line.
- **Where the refine materials come from.** The 407 recipes in
  `player.refine_proto` name 84 distinct materials; 64 of them drop from a
  monster that stands on one of the fourteen hosted maps (`mob_proto` column 33
  through `etc_drop_item.txt`, or `mob_drop_item.txt`), and the fishbone comes
  from gutting a fish, which no drop table records. Orc Valley alone carries
  ten - the Orc Amulet from Elite Orc (631) and Big Bald Orc (651), the
  Esoteric Guide from 701/751, Orc Tooth, Snake Tail, Curse Book - and the Bear
  Hide that 943 pieces of +6 gear were waiting on comes from ordinary bears on
  maps 21 and 24. What was in shortage was never the drop; it was the killing:
  bots warped in at one point and never left it, and a spare material was
  vendored on the next town visit. An earlier version of this note said 28 of
  84 were obtainable. That number came from the broken group_group parser
  described above; measure again before believing any count of this kind.
- Drop tables answer three different questions and only one of them uses vnums.
  `mob_drop_item.txt` is per-mob and by vnum; `etc_drop_item.txt` and
  `common_drop_item.txt` key on the **Korean item name**, and the etc table is
  reached through `mob_proto` column 33 (`DROP_ITEM`), one designated item per
  mob. Grepping a vnum finds nothing in two of the three. The chance is
  `prob * 10000 * PERCENT_LVDELTA * rate / 100` against `number(1, 4000000)` -
  and the `PRIV_ITEM_DROP` term in `GetDropPct` is defeated by an operator
  precedence bug in the engine, so it never applies.
- Shellfish (27987) and the three pearls (27992-27994) are hardcoded in
  `fishing.cpp` and `char_item.cpp`, not in `fishing.txt`. They carry 26 and 14
  recipes respectively, so fishing is the only route past the high refines.
- **The rest of the fishing chain, as the engine does it.** A live fish
  used (`fishing::UseFish`) becomes a dead fish, a bone, a shellfish or
  nothing. A dead fish is grilled by handing it to a campfire: Dried Wood
  (27600, the Fisherman's shop, 20 000 yang) used once spawns mob 12000 for
  forty seconds, `CHARACTER::GiveItem(campfire, cell)` runs `fishing::Grill`
  and the grilled fish (27863-27883) comes back - potions of 180/350/230 HP,
  180/500 SP, a Carp for movement speed and a Rudd for dexterity
  (USE_ABILITY_UP). A shellfish (27987, `char_item.cpp` case 27987) holds a
  Stone Piece half the time, nothing 30%, then a white, blue or blood pearl
  at 10/7/3% - so opening one is a bet the bot weighs against selling it
  whole, and the population counts every outcome including the empty ones
  (`PLAYERBOT_SHELLFISH:` every ten minutes).
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
