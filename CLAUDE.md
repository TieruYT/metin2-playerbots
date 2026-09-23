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
| `linux-port/overlays/playerbot/src/game/src/playerbot_empire_rules.h` | The three kingdoms: maps, gates, services, trainers, pitches. Unit-tested against Chunjo's own historical constants. |
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
| `playerbot_empire_rules.h` | The three kingdoms as pure policy: which maps a kingdom owns, its gates, its town services, its trainers, its market pitch. No engine types, unit-tested. Included first, so anything may ask it. |
| `playerbot_world_rules.h` | Pure travel policy. No engine types, unit-tested. |
| `playerbot_navigation.h` | Where a bot may stand and whether two points connect. Calls nothing above it. |
| `playerbot_world_memory.h` | What the population has learned about the world, as opposed to about itself. |
| `playerbot_movement.h` | Following a route: mounts, waypoints, portals, and the known-metin registry. |
| `playerbot_gear.h` | What a bot wears and carries: equipment scoring, the progression ladder, arrows, potions. |
| `playerbot_unique_slots.h` | The two unique slots: the uniques a bot never wears, and the rings and gloves on a clock it wears only while it hunts. |
| `playerbot_activities.h` | The horse, and fishing. Each owns the whole tick while it runs. |
| `playerbot_missions.h` | The Biologist's collections and the level-up hunt, driven without a quest dialog. |
| `playerbot_skills.h` | The character sheet: stat points, the job's skill order, keeping buffs up. |
| `playerbot_combat.h` | How a swing or a cast is sent: the packets a bot has no client to generate. |
| `playerbot_economy.h` | Money and bag: junk, merchants, the blacksmith, market stalls. |
| `playerbot_bonus.h` | The bonus lines on worn gear: what a line is worth, what finishes an item, and what a bot will pay to change it. |
| `playerbot_travel.h` | Where a bot ought to be, and crossing between maps. |
| `playerbot_planner.h` | Which long-term goal wins: the candidates, their base priorities, and the three gates no weight can touch. |
| `playerbot_guild.h` | Guilds by tier: a bot's strength, the kingdom's percentiles, founding, recruiting, promotion, the hourly experience offer, the master's skill points, the guild report - and who a bot has got on with. |
| `playerbot_town.h` | A town visit end to end, as a state machine that survives being interrupted. |
| `playerbot_itemshop.h` | The 2.x line's in-game ItemShop: the Kupon SM vouchers cashed, the account's Dragon Coins and Marks, and the few things a bot buys with them. Empty on r40250. |
| `playerbot_market.h` | Buying from another bot's counter: what is worth having, the walk to the stall, and the purchase. |
| `playerbot_chat_trade.h` | Trading over the chat: what a bot shouts about its counter and its wants, and the whisper it answers a player's "Kupie"/"Sprzedam" with. Fed by patch 0007. Anything else whispered falls through to the conversation. |
| `playerbot_conv_*.h` | ĹŌŞƬĒĶ's conversation layer as pure code (text, aliases, lexicon, intents, memory, state, say, general, generator, engine): what a whisper means, what the bot remembers of the person, and the reply. `playerbot_conv_aliases.h` is the players' own words (FMS, KK, KD, bodzio; M1, V1, DT; 2kk) and is also included by `playerbot_chat_trade.h`. No engine types, unit-tested (`tests/playerbot_conversation_test.cpp`). Included by the next row only. |
| `playerbot_chat_conversation.h` | The conversation's engine side: the snapshot of the bot it answers from, the whisper packet, the short timer while replies wait. After status.h. |
| `playerbot_loot.h` | Picking things up, in and out of a fight, without sweeping the floor. |
| `playerbot_lure_order_rules.h` | What a person's whisper to a bot means: "luruj" and "przestan lurowac". No engine types, unit-tested. Included first, with the other rules headers. |
| `playerbot_survival.h` | Saving progress, breaking off a losing fight, and the walk back after dying. |
| `playerbot_wandering.h` | What a bot does on a hunting map when nothing is asking for its attention. |
| `playerbot_status.h` | What a bot shows above its head, and the words for it. |
| `playerbot_targeting.h` | Choosing what to hit and hitting it, including the claim that keeps hundreds of bots off the same monster. |
| `playerbot_guild_war.h` | The bots' guild wars: the pair picked per kingdom, the engine's field war declared and accepted, the rally on the guild map and the fight there. After targeting.h because the blows are its. |
| `playerbot_demon_tower.h` | The bots' Demon Tower: one guild's raid at a time (the call, the gathering by the stone, the stone broken together), and the floors for whoever the jump takes - the scan of the floor, the duel-shaped fight, the keys used and handed in, the smith passed. After guild_war.h because the fight and the kingdom names are its. |
| `playerbot_persona_rules.h` | Iwakura's personality system as pure policy: the moods, the Grinder's tiers and the Law of Advancement, the gambler's ambitions, the Anti-PK window, the companion's draw, the mercenary's terms and the Useful Items List. No engine types, unit-tested (`tests/playerbot_persona_rules_test.cpp`). Included first, with the other rules headers. |
| `playerbot_persona_tables.h` | Rendered from his document by `tools/generate_iwakura_persona.py`: the valuables whose drop lifts a mood, and the LPP's weapons by level band, target shields and target armours. |
| `playerbot_mood.h` | The Bot Mood System: what a mood is worth to whom, the drought, the euphoria, and the mood a bot plays by (NORMALNY in company, its own alone). |
| `playerbot_persona.h` | Which personality claims a bot now, its Grinder tier and lock, the Law of Advancement, the two habits of a weak mood (the pause and the AFK stop), and the census. |
| `playerbot_gambler.h` | The gambler's session: the pieces it takes to the anvil, the ambition rolled for each, the budget, and what it does with what survives. |
| `playerbot_lpp.h` | Iwakura's Useful Items List: what a bot keeps at the storekeeper rather than sells, what the box holds, and what it lets go. |
| `playerbot_anti_pk.h` | The Anti-PK protocol and the stone hunter's quarrel: who struck the bot, who it fights back, and the capitulation after five deaths on one spot. |
| `playerbot_companions.h` | The two social personalities: the companion's phase and its invitations to people, a companion Shaman's party buffs, and the mercenary's contracts. After demon_tower.h. |
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

### Three kingdoms, and nothing may name a village by its index

The world has three kingdoms and each has the same four maps: a first village,
a second village, a guild map and an easy Monkey Dungeon. Shinsoo is 1/3/4/5 on
the `first` core, Chunjo 21/23/24/25 on `game1`, Jinno 41/43/44/45 on `game2`.
The engine's own quests name them - `new_quest_lv52` reads the first villages out
of `{ "Yongan", "Joan", "Pyongmoo" }` by empire and `new_quest_lv7` names the
second ones Jayang, Bokjung and Bakra. (Pyongmoo is Jinno's capital. The status
table used to label Chunjo's guild map with it, which was simply wrong.)

The three are mirrors in what they hold and in nothing else. Each village has
the same eight service NPCs (9001 Handlarz Bronia, 9002 Zbrojami, 9003
Roznosci, 9005 Dozorca, 9006 Starsza Pani, 20016 Kowal, 20349 Stajenny, 9012
Teleporter), the same starter monsters under vnum 500, the same Bestial bosses
in the second village, the same eight trainers in the first - and puts every one
of them somewhere else. **A Chunjo coordinate plus an offset is wrong for every
other kingdom.** So `playerbot_empire_rules.h` answers by map or by empire and
`playerbot_types.h` carries the per-map tables: services, market pitch,
trainers (`GetSkillTrainer`, first villages only - the second villages have no
trainer at all, which is what sends a bot with no skill group back to M1), the
Biologist, the wander hubs (`GetPlayerBotVillageGround`) and the fishing bank
(`GetPlayerBotFishingBank`). Chunjo's rows are the hand-made ones unchanged; the
measurement reproduces them to the unit, which is what says the other rows can
be trusted, and `tests/playerbot_empire_rules_test.cpp` pins that.

Ask `IsPlayerBotM1Map` / `IsPlayerBotM2Map` / `IsPlayerBotM3Map` /
`IsPlayerBotVillageMap`, never `== PLAYERBOT_MAP_CHUNJO_M2`. For a leg between
two of a kingdom's maps ask `GetPlayerBotKingdomLeg`, which gives the gate NPC
to walk to and the arrival the engine will use, both read from that gate's own
name. `GetPlayerBotRoadsEmpire` says whose roads a bot is on: the map's owner
inside a kingdom (the gate in front of the bot is the one it can walk to), the
bot's own empire everywhere else (there "go home" can only mean its own home).

Tools that measure a map rather than guessing at it:
`tools/dump_world_catalog.py` (services, gates, spawns, ground),
`tools/generate_wander_hubs.py` (hunting hubs with their level band) and
`tools/generate_fishing_bank.py` (stands and the water they face). All three
read the server's own files. Note that `decode_server_attr.load` returns
**sectors**, not cells: each is 128x128 cells of fifty units, and scanning
`range(w) x range(h)` looks at the first sixteen by twenty cells of the map and
finds a river nowhere.

**One map is hosted by exactly one core, and a bot cannot cross between them.**
`WarpSet` tells a client to reconnect and a bot has no client, so a map its core
does not host is a map it can never reach. Every shared map in this world - Orc
Valley, the desert, Sohan, both Spider Dungeons, Hwang, the two harder Monkey
Dungeons - is on `game1` with Chunjo. `IsPlayerBotMapHostedHere` filters the
frontier draw so no bot is sent at one, and `IsPlayerBotGrindAllowedHere` drops
the second-village ceiling for a kingdom whose core hosts no frontier at all -
otherwise Shinsoo and Jinno would wedge at level thirty-six with nowhere they
were allowed to hunt.

What that leaves is a level wall, and the measurement is worth having to hand
before anybody proposes a fix. Counting every hosted map's spawns through
`regen.txt` and taking the fifth to ninety-fifth percentile of monster level:

| core | continuous cover | holes |
|---|---|---|
| game1, Chunjo | 1-77 | 78, 83-86, 98+ |
| first, Shinsoo | 1-36, 60-104 | **37-59** |
| game2, Jinno | 1-35, 57-60, 69-72, 95-100 | **36-56**, 61-68, 73-94 |

The band that fills the hole exists on exactly two maps in this world - Orc
Valley (34-49) and the Yongbi Desert (37-52) - and both are on Chunjo's core.
Moving them does not help; it puts the same hole in Chunjo instead. The two ways
out are to host every kingdom map and the shared world on **one** core so any
bot can reach anything (at 323 bots a core the tick is 1.6-4.3 s of 60, so one
core carrying all of them is around 8 s of 60 - affordable, at the cost of the
three-way parallelism), or to accept that the two new kingdoms are village
kingdoms that stop at thirty-six.

Since 2.0.30 the first way is an operator switch, `M2_PLAYERBOT_WORLD_LAYOUT`
(m2-render-config, passed through the game service in both compose files):
`unified` appends Shinsoo's `1 3 4 5` and Jinno's `41 43 44 45` to `MAPS_game1`
and drops them from `MAPS_first`/`MAPS_game2`, so all three villages sit on game1
and the bootstrap's `map_allow_find` loop spawns every kingdom there next to the
shared frontier - no core code changed, because that loop was already generic.
`first`/`game2` keep their guild/event/high maps and host no bots; a core that
hosts none clears its `playerbot_status.tsv` on boot so the panel counts no
phantoms. Measured at 1500 bots on one core: tick 9.4 s of 60, and a Shinsoo bot
raised to 40 walked map 1 -> 64 (Orc Valley). Default is `split`, unchanged.
`unified` is for a modest population on one machine; a 2500-bot server still
wants the split (one core would be ~25 s of 60). This is now a config choice,
not a code change.

Separately, Chunjo's own core already hosts five maps this AI has never used -
217 (60-68), 70 (66-77), 216 (79-82), 73 (87-97) and 69 (9-76) - and Shinsoo's
and Jinno's each host a high-level set of their own. Adding one is the checklist
under "The frontier is four maps and one table".

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

### Iwakura's personalities, and the switch that turns them off

Iwakura's "SYSTEM OSOBOWOSCI v2.0" (19 September, `data/iwakura_osobowosci.txt`,
rendered in part by `tools/generate_iwakura_persona.py`) replaces the drawn
personality with one the bot's own situation decides, every two seconds. The
whole of it hangs on the `PERSONA` key of the weights file - a tick box on the
classic panel's AI page, **on by default**; off is exactly the old behaviour,
which is why every rule below asks `IsPlayerBotPersonaEnabled()` first. The old
personality stays as a hidden character that only biases chances
(`bDrawnPersonality`), and the Metin, M2 and M3 droppers are gone: what they
did is the Grinder's tier locks. The operator's medal-dropper cohort stays,
and since Community Patch 2 a drawn medal dropper stays one too (below).

`DecidePersona` (pure, in `playerbot_persona_rules.h`) is the order a bot is
claimed in: a mercenary's contract, then a party (Towarzysz), the rod, the
pickaxe, a stone under the hammer, the gambler's session, the anvil for itself
(Perfekcjonista), the bag at eighty percent (Handlarz), and last the Grinder or
- with the Law of Advancement met - the Zdobywca. `PLAYERBOT_PERSONA: census`
counts them every ten minutes with the moods beside them; the status file, both
panels and the title over a bot's head read the same answer.

The moods (BMS, `playerbot_mood.h`) are SLABY, NORMALNY and BARDZO DOBRY: a
drought of anything worth having lowers one, a valuable drop or a refine that
lands raises it, a refine that burns a piece at +8 or +9 lowers it by one
(Iwakura's addition of the same evening), five deaths at a player's hands lock
it at SLABY for forty-five minutes. A party, a dungeon, a raid, a war, a duel
and a mercenary's contract are all "company", and in company a bot plays
NORMALNY whatever it feels. Only a SLABY bot takes the two habits - a pause of
two to eight seconds between packs, and a stop of two to five minutes every ten
to thirty - and only a SLABY bot rests in town (the REST slider still sets the
share of those).

Four things the personalities changed that are easy to trip over later:

- **A weight that only ranks something ranks nothing.** The PARTY slider now
  sets the share of *time* a bot spends as a companion: the cohort draw is
  rolled afresh at every phase (45-90 minutes solo, then a new draw; 3-8
  minutes alone after a party ends), a Shaman's draw is cut to 35% of the roll
  and a party fighter's to 25%, and a party has no timer at all any more - it
  ends the document's way, at eighty percent of the bag, when the levels drift
  by more than six, or when the others walk off. A bag already at eighty
  percent does not start one either: the first measurement had seventeen bots
  in three minutes joining and leaving on the next check.
- **A companion asks people, and people are rationed.** A person with no party
  is invited, a person's party with room is asked to be let into, and both are
  refused by the game options' own "block party invites" and "block party
  requests". A person is asked once in twenty minutes by anybody, once in
  forty-five after a refusal, and once in three hours by the same bot; a bot
  asks anybody once in ten minutes. Nothing of this has been seen with a real
  person yet - the test world has none - and `PLAYERBOT_PARTY: asked a player`
  is the line to look for.
- **The mercenary is a contract, not a mood.** A bot that dies to monsters
  three times in half an hour is in distress; a stronger bot of its kingdom on
  the same map - three levels up at least, within the engine's thirty, and much
  better gear by level, pluses and Iwakura's tiers - walks up and offers to
  carry it for an hour at 250 000 through the yang curve. The client keeps a
  quarter of its purse: the first cut kept seven tenths and, at m2zip's yang
  rate of 3000% (7.5 million an hour), not one of the seven bots in distress
  could have hired anybody. Neither side changes map while it runs, the
  mercenary's full bag pauses it (the party is kept, the clock stops, the
  mercenary warps back to the client the way a bot follows a player), and the
  party rules, the watchdog's break-up and the map-change quit all stand down
  for it. `PLAYERBOT_MERC: census` counts the contracts and
  `PLAYERBOT_MERC: nobody to carry` says which of the reasons stopped one.
  Two things the first contract on m2zip taught in five seconds: **a goal that
  is a state rather than progress ends a contract on the tick it starts** - the
  client's bag was already at eighty percent, which is one of the document's
  own ends ("uzbiera przedmioty z ziemi"), so it paid 7.5 million for five
  seconds of company, and a bag at eighty percent is now a refusal at the offer
  (that bot wants the town, not a carry); and "w swoim otoczeniu na mapie" is a
  distance, because the mercenary that struck it had walked forty-four
  kilometres across Orc Valley to make the offer
  (`PLAYERBOT_MERC_NOTICE_RANGE`).
- **The Useful Items List is a keep, not a ranking.** A piece the list keeps
  is neither merchant scrap nor counter goods, and one already standing on a
  counter comes home at the next service visit. Since Community Patch 2 the
  list is his by name: the twenty-four jewellery and boot families of
  `PLAYERBOT_LPP_JEWELS` (read off `world.item_proto`, where some names are cut
  short), the weapons of `PLAYERBOT_LPP_WEAPONS` (the twenty-five he names),
  every body armour over `PLAYERBOT_LPP_ARMOUR_OVER_LEVEL` and every shield
  over `PLAYERBOT_LPP_SHIELD_OVER_LEVEL`, plus anything carrying a tier 5-6
  line rolled at least half-way up. Two of a weapon or an armour and three of
  a small piece for the bot's own class, one for another class; a family worn
  at +9 needs no plain backups. The six level-30 weapons on his list are left
  out of it (`ClassifyPlayerBotLppItem`) for the operator's own rules: the bot's
  own class up to `PLAYERBOT_LEVEL30_KEEP_MAX` for its anvil
  (`PlayerBotKeepsLevel30ForAnvil`), another class's never for the anvil - the
  grind for sale and the counter are its road. The list's pieces go to the
  storekeeper **at every visit**
  (they waited for a bag at eighty percent, and the bags held them for good);
  the herbs keep the old rule. What the list lets go is counter goods at
  `PLAYERBOT_SHOP_LPP_SURPLUS_SCORE` (`IsPlayerBotLppSurplusGoods`), never the
  merchant's - "musi natychmiast trafic na sklep, aby inni Hazardzisci mogli je
  odkupic". What the box holds is remembered from the last visit
  (`TPlayerBotPersona::mapLppStored`), a box with fewer than nine free cells
  stops the list keeping anything new - or a full box would leave a full bag
  for good - and a piece the list lets go is remembered as released, because
  the dead-stock rule would otherwise send it straight back down.

Community Patch 2 (Iwakura, 22 September; his text stays with the operator,
not in `data/`) reworked these point by point. What each hangs on:

- **Every bot of thirty holds its class's level-30 weapon** (point 1).
  `PlayerBotLacksClassLevel30Weapon` sends a bot with none in hand or bag to
  the market at its next town visit, whatever its level and with or without the
  switch; the browse ranks the class's weapon at 400 plus its average line, so
  the highest average wins and the price only breaks a tie ("12% za 300k albo
  26% za 450k - wybierze drozsza"). Under the switch the purchase and the anvil
  share `PLAYERBOT_LEVEL30_BUDGET_PERCENT` of the purse the visit began with,
  and the anvil aims at `GetPlayerBotLevel30Aim`: the first plus drawn by pid
  (60/13/8/5 percent for +6/+7/+8/+9; the fourteen the sheet leaves over also
  +6) while the weapon walked into town under it, then
  `PLAYERBOT_LEVEL30_LONG_TERM_PLUS` (+9 for the +9 draw) at every visit after.
  From `PLAYERBOT_LEVEL30_BLESSING_FROM_3_AVERAGE` it goes under a Blessing
  Scroll from +3 (the operator's 37%-and-up scroll-only rule answers first),
  and a counter's piece at that average beating every one the bot holds is
  bought as well (`IsPlayerBotMandatedLevel30Offer`). A bot that can buy one is
  not sent to farm M3 for it. `PLAYERBOT_MARKET: level-30 census` counts the
  eligible, the lacking and the pluses every ten minutes. Droppers are left out.
- **Two Grinder styles** (point 2), one draw by pid (`GrinderStyleRoll`):
  thirteen percent never hold at a lock (`NeverHoldsAtLocks`), and ten percent
  more may give grinding up - a 33% roll once per tier, the moment the Law of
  Advancement is met at the lock, remembered in `PLAYERBOT_PERSONA_FLAG_QUIT`
  and `_QUIT_TIER` so a restart does not roll again. Both lift a lock already
  written (`grinder lock lifted ... why=`): 95 in the first quarter hour on
  m2zip, and two bots gave grinding up.
- **Bonuses by the sheet** (point 3). Before 45 the boots, the necklace and
  the bracelet (`IsPlayerBotEarlyBonusSlot`) are bonused at any refine and are
  finished at a health line plus one of his lines for the slot; the weapon
  waits for two of them to carry health (`PLAYERBOT_EARLY_HP_PIECES_FOR_WEAPON`)
  and aims at `PLAYERBOT_BONUS_WEAPON_TARGET_AVERAGE`. A new piece whose lines
  are worth less than the worn one's waits in the bag while a stone can change
  that (`IsPlayerBotSwapHeldForBonus`) and is bonused where it lies: on m2zip
  the first three held pieces were bonused past the worn ones inside twenty
  minutes. See "A wait must be one the pass that ends it will end" below.
- **The medal dropper came back** (point 4). Under the switch a drawn medal
  dropper used to become a Wanderer (`GetPlayerBotCharakter`), which left the
  operator's cohort as the only farm; it stays one now, and
  `PLAYERBOT_MEDAL_DROPPER_EXTRA_PER_MILLE` of the bots with no role of their
  own are drawn one besides, in their band only. They hold at the dungeon's
  lock, not a tier's, and never advance out of it
  (`GetPlayerBotMedalDropperLock`); `PLAYERBOT_MEDAL_GOAL_PERCENT` of them farm
  until the medals pay for their level's weapon and armour at +9 and helmet and
  shield at +7, the weapon first from thirty, and then play as Wanderers
  (`ManagePlayerBotMedalGoal`, `PLAYERBOT_PERSONA_FLAG_MEDAL_GOAL`). Measured on
  m2zip ten minutes after the start: 75 of the 737 bots of channel 1 outside
  the operator's 99 and 21 of 209 on channel 2 - a tenth, against the 26 in a
  thousand Iwakura counted. Their unreachable plans (the maze's chambers,
  on a retreat) are most of the core's `PLAYERBOT_NAV: unreachable` now.
- **Books for a trader** (point 5). The Handlarz persona or any bot on a town
  visit (`PlayerBotBuysBooksAsTrader`) buys its own Master skills' books
  whatever its gear, from `PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT` of the purse
  per `PLAYERBOT_BOOK_BUDGET_WINDOW_MS`, and reads them; another class's books
  score `PLAYERBOT_SHOP_OTHER_CLASS_BOOK_SCORE`, and
  `PLAYERBOT_SHOP_OTHER_CLASS_BOOK_MIN` of them open a counter.
- **Mood.** A boss's casket lifts it (point 6,
  `PLAYERBOT_MOOD_BOSS_CASKET_VNUMS`), and a SLABY bot picks up its own drop
  before it steps away (point 7) - for at most
  `PLAYERBOT_MOOD_AFK_LOOT_WAIT_MAX_MS`, because a drop it cannot reach must
  not cost it the habit.
- **Inflation** (point 8). `RefreshPlayerBotWorldYang` sums the gold of every
  character every ten minutes, GMs included ("lacznie na wszystkich
  postaciach"), and `ScalePlayerBotIwakuraPrice` adds
  `PLAYERBOT_INFLATION_STEP_PERCENT` per `PLAYERBOT_INFLATION_STEP_YANG` in
  whole steps on top of the yang curve. The step is folded into
  `GetPlayerBotPriceGeneration`, so a new one reprices every offline counter
  at its service visits. m2zip started at 3.19 billion and +5%.
- **The gambler in town** (point 10). A bot in its first village with a
  weapon at `PLAYERBOT_GAMBLE_TOWN_WEAPON_PLUS`, an armour at `_ARMOUR_PLUS`
  and `PLAYERBOT_GAMBLE_TOWN_PURSE_BASE` through the yang curve and the
  inflation begins the gambler's session without the roll (`townTrigger`).
- **The market Perfectionist** (point 11). `IsPlayerBotMarketPerfectionist`
  (fifteen percent by pid, and the never-holders, the quitters and the
  graduated goal droppers, whose gear is meant to come off the market) looks
  for a finished piece at `PLAYERBOT_READY_GEAR_MIN_PLUS` inside
  `PLAYERBOT_READY_GEAR_LEVEL_WINDOW` of its level before the blacksmith, and
  holds the anvil `PLAYERBOT_READY_GEAR_WAIT_MS` for the purchase.
- **Fifty-four junk weapons, five on the market** (point 13). His bases at
  +0..+3 (`IsPlayerBotJunkWeaponVnum`) may stand
  `PLAYERBOT_JUNK_WEAPON_MARKET_CAP` at a time on all the bots' counters
  together: the ledger counts them (`junk_weapons=` in its line), the junk rule
  sends the rest to the merchant, the service visit takes a surplus home. A
  piece with prize lines is exempt, the Useful Items List keeps first (Lwi
  Miecz is on both of his lists), and the cap is asked before the level-65
  pickup goods, which would otherwise keep those for a full market.
- **The first villages hunt what pays** (point 14). `PLAYERBOT_M1_VALUE_HUBS`
  is the 48 hubs of the three first villages whose ring holds the White Oath
  soldiers and the bears - 15, 15 and 18, measured through regen.txt,
  group.txt and group_group.txt on m2zip's share - and
  `PreferPlayerBotM1ValueHubs` takes them over the dogs when the bot hunts for
  its own level. One percent of tier 1 (`IsPlayerBotM1Farmer`) stays there
  until it can pay for the level-30 weapon at +8 and an armour of 18 or 26 at
  +9 (`PlayerBotM1FarmerGoalMet`).
- **A guild defends its own** (point 15, Amos's idea). A person who strikes a
  bot calls its guild (`TPlayerBotGuildCall`): every member within
  `PLAYERBOT_ANTIPK_GUILD_RANGE` of the attacker drops what it is doing and
  fights, for `PLAYERBOT_ANTIPK_GUILD_MEMORY_MS` after the last blow
  (`BOT_FOE_GUILD`, `PLAYERBOT_ANTIPK: guild called`). Only a person is an
  aggressor. Never watched: the test world has no person to strike a bot.

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
  tops a Metin stone up to one skill book (three until 2.0.11 - "3 KU z metina na piatym poziomie" from a player of forty-six) and rolls the chest. The chest's
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
- **Every bot account is `status='BLOCK'` by design, so that column cannot say
  who a GM banned.** Bots spawn server-side (`SpawnBot`/`CreateBotDesc`), never
  through auth, so their accounts are created BLOCK precisely to keep humans off
  them - all 2500 of them. A GM ban (`/block_player` -> `BanManager::Block`)
  writes `account.account_block` (and sets availDt/status, which for a bot is a
  no-op), so the *ledger* is the only bot-safe signal - empty until someone bans,
  zero risk of the predicate nuking the cohort. `RefreshBannedBots` reads it on
  the top-up cadence, despawns a banned registered bot and keeps it off the
  spawn queue; removing the row lets the next top-up return it. Without this a
  banned+kicked bot was resurrected by `TopUpMissingBots` a minute later
  (mateuszp211, 2.0.29). A ban is not a delete: the `player` row stays.
- **A live bot's level cannot be set with a plain SQL UPDATE.** The game core
  holds every spawned character in memory and writes its cached copy back on the
  save cycle, so `UPDATE player.player SET level=40` on a spawned bot is undone
  within seconds (the bot reverts to its cached low level). It is the same cache
  that makes `item_proto` edits stick only for offline characters. To move a live
  bot's level for a test, use the path the panel uses - a `player.web_admin_queue`
  row `cmd='LEVEL'`, which the `web_admin` quest applies in-core with
  `pc.set_level` - and it sticks. Verified: a direct UPDATE left six bots at
  their old levels; the queue raised them to 40 and held.
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
  `linux-port/docker/game/bin/` now, exactly as the real image does. What it
  still cannot copy is `share/`, and that includes the **compiled quests**: they
  are built by `qc` in an image stage and never at container start, so a test
  server kept alive on fast builds runs whatever quest the last real image build
  compiled. On 10 September that was a `web_admin.quest` three days old, and the
  panel's mass item grant answered `unknown_cmd` for every bot - which is what
  "masowe dawanie itemow botom nie dziala" was, for a player as much as here.
  After changing anything under `files/*.quest`, rebuild the image
  (`docker compose build game`) before concluding anything about a quest.
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
- **A bot takes one order from a person, and the engine has the two calls it
  needs.** "Luruj" whispered to an Archer in your own party makes its pulling
  course yours until "przestan lurowac" (`playerbot_lure_order_rules.h` is the
  words, pure and tested; `playerbot_chat_trade.h` sets the order on the state,
  `playerbot_lure.h` runs it - chat_trade.h is included first, so the course
  notices the order rather than being called into). Four things shaped it.
  **The rules that give way are only the ones that exist to keep a bot from
  luring for nobody**: three party members becomes two (the pair is the party),
  the receiver may be a person (the roster refuses one on purpose - a bot is not
  to pick a person to hold a pack for it), the frontier-map rule goes (a person
  standing somewhere has said where they hunt) and the 20-50 s between courses
  becomes 4-9. The HP floor, the safe zone, the value policy and the pack window
  are untouched. **The pack is put on the person, which the bots' own role never
  does**: `UpdateAggrPoint(player, DAMAGE_TYPE_SPECIAL, monster->GetMaxHP())` -
  more aggro than two arrows earned, a number the monster supplies, and the one
  damage type `UpdateAggrPointEx` does not spread over the victim's party - and
  then `SetVictim(player)`, because the aggro comparison in
  `ChangeVictimByAggro` is refused for three seconds after any victim change
  and a monster that has just turned to chase the Archer has had one; SetVictim
  restarts that lock, which is long enough for the person to land the blows
  that keep it. A monster `battle_is_attackable` refuses (the person is in a
  safe zone) is left alone and the person is told why. **The follow pass had to
  stand down**: `ManagePlayerBotFollowHumanLeader` runs far above the lure hook
  in the tick and would walk the Archer home the moment its course passed
  `PLAYERBOT_PARTY_FOLLOW_DISTANCE`, with the course walking it out again on the
  next tick - the errand loop Pabloo's 2.0.49 fix stopped, from the other side.
  **And a claim is released by walking the map for the bot, not by recomputing
  its key**: the key is the leader for the bots' own role and the person for
  theirs, and by the time a course ends (the party lost its leader, the order
  was taken back) what the key would be has already changed, so a claim left
  behind holds the role against every other Archer for
  `PLAYERBOT_LURE_SESSION_TTL`. The order outlives a course and is renewed by
  every course, so its deadline means "nothing has happened for forty-five
  minutes"; it ends at the word, at the party, at the map, at
  `PLAYERBOT_LURE_PLAYER_MAX_SEPARATION`, and the person is told. Compiled on
  both engines and read; never watched with a person in a party, because the
  test world has none. `PLAYERBOT_LURE: order` and `pack handed` are in the
  support bundle's grep list; the rest of the tag is not, because a course
  writes six to eight lines and the bundle keeps forty thousand.
- **An errand mode must not refuse the errand it names.** A person's standing
  lure order put the bot in `COMMITTED_TRAVEL` (2.0.89) so it would stop
  grinding between courses, and `Evaluate` refuses everything in that mode - so
  it refused the pack the course had just walked out to tag. The order was
  taken, "Juz dla ciebie luruje" was said, and every course ended `no_pack` a
  tick later while the bot stood beside the person (l0st3k and nerrvous_s,
  20 September, both with logs: `planned ... 957 ms ... finished
  stage=approach reason=no_pack`, over and over). `Context::lureCourseTarget`
  is the exception, under defence and over the errand modes, and it is set only
  while an order's course is running - so between courses the order still stops
  the hunting it was written to stop. Whenever a mode is added to say "this bot
  is busy with X", grep for the code that performs X and check it does not ask
  that mode.
- **A gate that opens must sit above the gate that breaks off.** The same
  release dropped the opening health for an order to 55% and left the break at
  the bots' own 70%, so a bot between the two opened a course and ended it
  `low_hp` on the same tick, for as long as the order stood. Two thresholds
  that bound the same activity belong next to each other in the constants, with
  the pair stated in the comment.
- **Two roles asking one finder opposite questions want the window as an
  argument.** `FindPlayerBotLurePack` was written for the bots' own role - a
  pack the party has not reached, so beyond bow range, clear of the ground the
  party is fighting over, and judged against the Archer's own level - and a
  person's order means the exact opposite: the monsters *round me*, which I
  will fight. All three windows refused exactly that, and the level one was the
  worst: a level-19 companion beside a level-33 player refused every monster on
  the map for being eight levels over *itself*. `TPlayerBotLureSearch` carries
  the four numbers now and `GetPlayerBotLureSearch` answers by who asked.
- **"A person's party" is not the same set as "serving a person", and the
  difference is exactly the bots people play with.** The rule that a bot in
  somebody's party runs no errand (2.0.48) asks whether the party's *leader* is
  human - but a companion invites the person into **its own** party, so the
  leader is a bot, and all six errand gates in the tick (the stable, the
  Biologist, the herbalist, the negative-rank hold, and both halves of a town
  visit) were silently off for those bots. What it looked like was a bot that
  took a lure order and walked to the blacksmith with it, logging
  `PLAYERBOT_LURE: waiting reason=busy` for four minutes. `bServingPerson` is
  the union: a human-led party, a mercenary contract, a companion holding a
  person in its party (`IsPlayerBotHeldForCompany` already knew those two), or
  a standing lure order. The follow-the-leader branch still asks about the
  party, because that one really is about who leads.
- **Somebody else's client pack is a superset until it is measured.**
  l0st3k's multilanguage pack (20 September) is seven new locale directories
  beside pl and en, and the whole of it went in - but only after three
  measurements, and each one decided something. **All 762 files of our own
  locale pack are byte-identical inside his**, so nothing of ours is lost (the
  character-select background of client 2.0.9, the loading logos, the rules),
  which is what made taking his `locale.index`/`locale.data` whole the safe
  move. **His root was built on an older client of ours** - our published
  2.0.22 differs from his base in `shoppricepump.py`, `uiautohunt.py` and
  `playerbot_status_tail.py` - so from his root only `configmain.py` was taken
  (the language list, 2 replacements), and with it the auto-hunt behaviour
  change he had made along the way stayed out. And **our own exe already
  exports `GetLanguage`**, so the multilanguage needed no new binary: the
  machinery was always there and what was missing was the locale data and a
  picker with more than two rows. Check all three before merging any client
  pack somebody sends: what of ours it drops, what of ours it rewinds, and what
  it needs from the exe.
- **A language overlay belongs after the locale files, and its defaults keep
  everybody else's text.** Codex's English interface is `english_gui.py` -
  two dicts applied over the loaded namespaces in `localeinfo.py` and
  `uiscriptlocale.py`, and only while `systemSetting.GetLanguage()` is "en".
  That is what makes it safe: EN's own `locale_interface.txt` is missing 213 of
  PL's keys and `locale_game.txt` 676, and the loader reads PL first, so a
  missing key is Polish text rather than a traceback - the overlay then covers
  all 144 of the missing interface keys the scripts actually use. Strings that
  stood hardcoded in the scripts move to keys with `globals().setdefault(key,
  '<the Polish text>')`, so DE/ES/IT/PT/RO/TR read exactly what they read
  before. `tests/client_locale_loader_test.py` runs the real loaders with the
  engine stubbed and asserts both halves: English wins for "en", and no string
  value moved for PL/DE/TR.
- **An edit of ours that changes its own output cannot be idempotent against
  our own published root.** `clientrootify.py` renders from the published root
  and skips an edit whose `new` it already finds. Change what an insertion
  produces - here three Polish literals of ours became locale keys - and `new`
  is not there yet while `old` (the stock anchor) was eaten by the previous
  render, so the run dies. A pair may now be marked optional (a third element):
  it migrates text one of our own earlier edits put there, and is silent both
  in a pristine stock root and in one already migrated. And `EDITS` is a plain
  dict, so **a second entry for a file silently replaces the first** - which is
  how a `uisystem.py` edit went missing for one render until the file came out
  the same size twice.
- **A party of two that loses one is a party the engine deletes.**
  `CParty::Quit` takes the member out and leaves the party standing;
  `P2PQuit` deletes it only when the leaver's role was LEADER, so that half
  was always right and the other half never was. A plain member leaving a
  pair left the other one holding a party of one: `GetParty()` still answers,
  `ManagePlayerBotParty` returns on the first line of its "already in a party"
  branch (the distance rule is skipped for the leader, `leader != ch` being
  false), and that bot never looked for a partner again as long as it lived.
  `CInputMain`'s own handler never allows it - at two members it calls
  `DeleteParty` instead - which is why a player cannot be in a party of one
  and nobody had ever seen this from the player's side. Measured on the test
  world over two days: 94 to 2 519 `created party` lines an hour with almost as
  many `left party due to distance` beside them (dist=10526..13922 -
  `ChoosePlayerBotHuntingHub` reaches twenty kilometres), and **every** census
  in that time reading `in_party` all but equal to `parties` - 143/143, 117/115,
  69/68, 113/109. Ten minutes after the fix, on the same world: 33/24.
  **Read that ratio knowing what the census counts.** `NotePlayerBotPartyCensus`
  sits below the tick's own light/full split (`(pid + sweep) % 2`), so it sees
  one half of the bots - 404 of 864 - and the two members of a party have
  unrelated pids. A world of nothing but pairs therefore reads
  (0.25*2 + 0.5*1) / 0.75 = **1.33**, not 2, and a world of nothing but
  singletons reads exactly **1.00**. Before the fix it was 1.00 to 1.04: every
  sampled party was one bot. After it, 1.375. So parties were made by the
  hundred and at any instant nearly every one of them held exactly one bot -
  the members churned through and the leaders piled up as singletons that only
  a rotation timer, a map change or the watchdog could free. The Archer's lure
  role, which needs three in a party, had not run once in five hours.
  (Count the log lines per hourly file: the core rotates `syslog` into
  `log/<date>/syslog.HH`, and a count over the live file alone is the count for
  the minutes since the hour - which is how the first reading of this said four
  an hour.)
  `LeavePlayerBotParty` is the one way a bot leaves now, in all eight places,
  and the party pass dissolves a party of one it finds so a world already full
  of them recovers in a minute. Measure it as the census's `in_party` against
  `parties`, against the 1.33 above and not against 2.
- **An empty directory is the most fragile build input there is.**
  `serverfiles/share/package` is empty on every install of both lines and the
  game Dockerfile COPYs it all the same. Git cannot track an empty directory,
  the full package carries it as one of twelve bare zip entries, and an
  extraction that drops those leaves a tree whose build dies at "failed to
  compute cache key". What the player got was the launcher refusing to prepare
  the package and naming an r40250 archive nobody on the 2.x line has ever had
  (dekri, 20 September, on 2.0.89). All three Windows paths that reach a build
  make it rather than demand it (`Restore-M2EmptyGameContextDirs`, and
  `start-server.ps1`'s own copy, which imports no module), and `update.sh` does
  the same before compose. Before adding a directory to a build context, ask
  whether it will ever hold a file.
- **An order is a job, and every rule written for an idle bot has to be read
  again against it.** 2.0.89 shipped the person's order with the four rules
  that "exist to keep a bot from luring for nobody" given way to, and three
  more were left standing that are the same kind of rule and were shut for
  ever beside a person who is hunting: the party-busy count
  (`PLAYERBOT_LURE_BUSY_MONSTERS` is three, and four monsters on a person
  standing on a spot is an ordinary Sunday), the nine-tenths health floor (the
  bot takes hits from whatever the person is fighting), and - underneath both -
  the fact that **nothing stopped the bot hunting between courses**, so a
  monster was always chasing it and `onOwner > 0` refused every start. What a
  person saw was a Ninja killing the pack it was asked to fetch and a status
  reading "Czekam, zeby lurowac dla X" for ever (marcinxboss, 20 September);
  the two are one causal chain, because a course that cannot open hands the
  tick to ordinary target acquisition. A standing order is COMMITTED_TRAVEL in
  the value policy now - self-defence never asks it and party defence is
  answered above that branch, so the bot still hits back and still helps the
  person - the bow is held in the hand while an order stands (a dagger drawn
  for one Metin makes `IsPlayerBotArcher` false and the whole course
  "ineligible"), and the health floor is `PLAYERBOT_LURE_PLAYER_START_HP_PERCENT`.
  And the wait says which gate it is waiting on, in the status and once per
  change in the log (`PLAYERBOT_LURE: waiting reason=`), because three gates
  that all look like "Czekam" is a report nobody can make. The whole feature
  is still unwatched with a person in a party: the test world has none, and
  the bots' own role needs three in a party, which on a measured world it
  often has not got (143 parties, 143 bots in them, 20 September).
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
- **What a bonus line is worth is three of the world's own tables, not taste.**
  `player.item_attr` says what may roll on which slot and how high, and it is
  the answer to most questions about gear: health does not roll on a helmet or
  an earring, critical does not roll on a wrist or an earring, attack value
  rolls on a body and nowhere else, block only on a shield, and
  `APPLY_ATTBONUS_MONSTER` and the two damage-percent lines are not in it at
  all - the first is only in `item_attr_rare` (value ten, what a Magic Metal
  adds) and the other two come from `item_addon.cpp`. `battle.cpp` says what a
  line does, and it disagrees with every wiki: the five weapon-type resistances
  never fire against a monster, because `battle_hit` reads the *attacker's*
  `WEAR_WEAPON` and a monster wears none; an elemental resistance is applied at
  thirty percent of its own number; `BLOCK` answers melee (73% of this world's
  monsters) and `DODGE`/`RESIST_BOW` only ranged (0-24% of a map). And
  `item_addon.cpp` draws the skill-damage line from a gaussian of sigma five and
  then sets the average line to **minus twice it** plus noise, so no weapon can
  carry both - +18% skill is -29% average, and a caster that only ever stopped
  on the average line never stopped at all. `HasPlayerBotFinishedBonus` used to
  ask a helmet for health and attack value and an earring for health and
  critical, none of which can roll there, so those slots could never be finished
  and were rerolled for as long as their owner had gold.
- **A race-attack line is worth what share of the map that race is, and on
  three maps it is worth nothing at all.** `CalcAttBonus` walks the races as an
  else-if chain (ANIMAL, UNDEAD, DEVIL, HUMAN, ORC, MILGYO, INSECT, FIRE, ICE,
  DESERT, TREE) so a kill pays exactly one of them, and `char.cpp` maps an
  APPLY onto only the first six: INSECT, FIRE, ICE, DESERT and TREE have a
  POINT and a place in the damage formula and nothing an item can put into
  them. `tools/analyse_map_races.py` counts every spawn point of every map a
  bot may stand on: Orc Valley 63% orcs, all three second villages 100% human,
  the first villages 77% animal, the guild maps and all five Monkey Dungeons
  100% animal, Mount Sohan 46% undead, Hwang 68% mystic - and the Yongbi Desert
  (DESERT/INSECT) and both Spider Dungeons (INSECT) pay no race line ever.
  `PLAYERBOT_MAP_RACE_TABLE` in `playerbot_types.h` is that measurement and
  `GetPlayerBotFightingRace` returns the share with the race, so the equipment
  pass and the reroll pass cannot disagree - which they did, at 600 points a
  point against one, so a bot bought a shield for the line and rerolled it off
  at the next blacksmith.
- **A bot's name is the one part of its identity nothing depends on, and five
  places in the seed disagreed.** `LoadRegisteredBots` matches on the account
  login (`playerbot_NNN`), the social id and the `player_index` row and never
  reads `p.name`; `CHARACTER::Save` does not write the name column, so a
  running core will not undo a rename; both panels ask the account. But
  `generate_seed.py` treated a renamed character as "not the character this
  registry describes" in five places - a skip rule, its matching assertion, the
  alias pass, the `player_index` insert and the final row assertion - so the
  first time nicknames were turned on the whole cohort left the seed's care:
  "preserving 2500" instead of 668. They all consult
  `common.playerbot_name_history` now and require both halves to agree, so a
  second, hand-made rename is still somebody's deliberate choice.
  `M2_PLAYERBOT_HUMAN_NAMES` is 1/0/`restore`; the pool is
  `tools/generate_bot_names.py` over `data/bot_names_iwakura.txt` - one written list, no class or sex pairing since 2.0.2, since 2.0.10 dealt by kingdom: the list shuffled once by its own hash and cut into three equal shares (`player_index.empire` says whose a bot is), a kingdom larger than its share (Chunjo, 1500 seeded) continuing with its own names and `2`/`v2`, then `3`/`v3`, then v4 from the whole list; and a pool version that is recorded but, since 2.0.84, renames nobody: Iwakura's list of 19 September grew to 1 800 lines (1 768 names in the pool) and the operator's rule was not to change a name a bot already wears ("staraj sie nickow juz istniejacych playerbotow nie podmienic") - a bot with a name keeps it, a waiting bot (no history row) takes a free one, and a name another bot wears is never free. **A waiting bot must never be dealt a name a settled bot wears**: the plan used to number waiting bots from one and free names from the top of the list, excluding only people's characters, so the thousand Shinsoo/Jinno bots seeded by 2.0.8 into an already-named world got the first thousand Chunjo names - 999 duplicates measured. A name worn by a bot with a current history row is not free.
- **`account.account.empire` is not where a bot's kingdom lives.** The seed
  wrote a literal 2 into it for the whole cohort while `player_index.empire` -
  the column the core actually reads - was right, so every Shinsoo and Jinno bot
  claimed Chunjo to anything that asked the account, and both panels did. Ask
  `player_index` first and the account only as the fallback for a hand-made
  character with no index row.
- **No double quote may reach docker from PowerShell, and the second instance
  cost a backup.** `Invoke-M2DatabaseImport`'s "does this database exist" probe
  was `sh -c "mariadb ... -e `"SELECT ...`""`; PowerShell 5.1 wraps a native
  command's argument in double quotes without escaping the ones inside it, so
  the first `"` ended the argument, `sh` got a broken script, the probe always
  came back empty and the "kopia trafi do folderu backups" the confirmation
  dialog promises was an empty folder at every import anyone ever ran. Invoke
  `mariadb` directly with the query as its own argument. Same trap as the
  support bundle's empty `playerbot-syslog.txt`.
- **The game's language lives in the image, so every update undid it.**
  `m2-lang` switches four files under `share/` - `conf/item_names.txt`,
  `conf/mob_names.txt`, `locale/*/translate.lua`, `locale/*/locale_string.txt` -
  and `share/` is baked into the game image, not on a volume. Every update
  rebuilds that image, so the English originals came back while the remembered
  choice, the panel's language page and `lang.status` all went on saying Polish.
  What a player saw was a world named half in each: our own Polish strings
  beside "Skill Book" on a bot's stall sign, quests and monsters in English.
  `cmd_prepare` remembered the choice and never re-applied it; it calls
  `cmd_apply` now, on every start, which is idempotent and keeps the shipped
  English file beside the active one as `.m2orig`. Reproduce it by copying
  `item_names.txt.m2orig` over `item_names.txt` - vnum 50300 goes from
  "Ksiega Umiejetnosci" to "Skill Book" and back. The db core reads these at
  boot and pushes them into `player.item_proto.locale_name`, which is what
  `proto->szLocaleName` - every name a bot says - comes from, so one restart
  carries the fix all the way to the stall signs.
- **The panel's passphrase can be a secret from its own operator.**
  `.env.example` ships `M2_PANEL_PASSWORD` empty; the installer fills it in and
  every other route to a `.env` does not. The panel's entrypoint then invents
  twenty characters, stores only the PBKDF2 hash in `m2panel.conf` and prints
  the plaintext once to a container log nobody reads - so the panel has a
  password that exists nowhere and `docker compose config` shows
  `M2_PANEL_PASSWORD: ""` while the operator swears it is set. The launcher
  fills the blank before Compose sees it (`Assert-PanelPassphrase`), and the
  panel button offers both the value from `.env` and a reset that deletes
  `m2panel.conf` so the entrypoint can rebuild it. Nothing in the panel has
  ever had a hard-coded login or password; there is no login at all.
- **Half a translation reads worse than either language.** The classic panel
  puts 446 strings through `t()` and writes about 530 more in Polish where they
  stand, so an English page is a Polish page with holes - "mam polowe panelu po
  angielsku polowe po polsku". It used to ask `Accept-Language` first and fall
  back on English, and a Polish player on an English Windows got English
  because Chrome sends `en-US,en;q=0.9,pl;q=0.8`. Polish is the default now and
  the browser is not consulted; the header's switch stores the choice in its
  own year-long cookie, not in the session, because marking the session
  permanent would have extended the admin login to a month as a side effect.
- **A quantity that reaches the engine as a BYTE is a quantity nobody counted.**
  `pc.give_item2` reads its count as an `int` and hands it to
  `CHARACTER::AutoGiveItem(DWORD, BYTE bCount, ...)`: 256 becomes 0, 300
  becomes 44, 65535 becomes 255, and nothing reports it, because a non-zero
  `item_id` looks like success and the panel writes "Nadano". The panels
  offered 65535. 200 is the real ceiling for one call - it is a full stack, and
  what `AutoGiveItem` tops up (`MIN(200 - GetCount(), bCount)`) - so
  `web_admin.quest` refuses anything above it with `qty_too_big` and compares
  the bag before and after, because a non-zero pointer does not prove delivery
  either: with no free cell the same function drops the item on the ground and
  still returns it. **And a new status word has to be added to the quest's own
  whitelist** near the end of the handler, or it is rewritten to `unknown_cmd`
  and the panel reports "quest wymaga aktualizacji" - which is what the first
  test of this showed.
- **`GetEmptyInventory(height)` returns a position, not a count.** Two calls
  beside each other can point at the same cell and reserve nothing, which is
  what "przedmioty ze skrzyn wypadaja na ziemie" was:
  `GiveItemFromSpecialItemGroup` hands out its rewards one by one through
  `AutoGiveItem`, and that drops what does not fit. Count the free cells
  (`CountPlayerBotFreeInventoryCells`, defined in `playerbot_consumables.h`
  because that file is included before `playerbot_economy.h`) and require
  `PLAYERBOT_CHEST_FREE_CELLS`. That was a mitigation; since 2.0.6
  `PlayerBotBagTakesGroup` (`playerbot_gear.h`) lays the group's whole set out
  on a copy of the grid the way the engine places items (height in one column
  of one page, stackables merged into their stacks first) before any chest is
  used - every line of a `Type Pct` group, the largest line of the others. It
  needs the group's lines and type, which mt2009 exposes (`GetItems`, and a
  `GetGroupType` getter playerbotify.py adds); r40250 exposes neither and
  keeps the five-cell heuristic. The engine still gives one by one, so this is
  the bot refusing to open, not the engine refusing to spill.
- **The planner and the pass that acts must ask one function, not two lists.**
  `HasPlayerBotRefineOpportunity` accepted any bag piece the equipment selector
  liked; the refining pass then also rejected anything `IsPlayerBotJunkItem`
  had marked for the merchant. A started blacksmith visit is a commitment the
  planner will not override, so the bot walked to town for nothing - "mam
  wszystko +9 zalozone, a bot dalej lezie do kowala". Both ask
  `IsPlayerBotRefineBagCandidate` now. Worn pieces are outside it: the junk
  rule does not apply to what a bot is wearing.
- **`log.log` is declared big5 and the game writes CP1250 into it.** Measured
  on this world: `TABLE_COLLATION` is `big5_chinese_ci` for `type`, `how`,
  `hint` and `ip`, and the bytes prove the conversion happened on the way in -
  "Bojowy Luk Jezdzcy" is stored as `42 6F ... A2 47 75 6B ... 3F ... 3F`,
  where CP1250's single `0xA3` became the two-byte big5 `A2 47` and every
  character big5 cannot represent became `0x3F`. 128 573 of the 279 242
  non-ASCII hints carry that question mark, and it cannot be undone. So
  changing the declaration fixes what is written next and repairs nothing that
  exists; transcoding the column would make it worse. The table is 22 million
  rows and 1.75 GB of MyISAM, so any ALTER is minutes of downtime - it is a
  planned, versioned migration with a backup, not a drive-by. Only the gear
  history's item names are affected; nothing in the game reads this column.
- **The panels' 71 and 72 are the most-repeated mistake in this project.**
  `APPLY_SKILL_DAMAGE_BONUS` is 71 and `APPLY_NORMAL_HIT_DAMAGE_BONUS` is 72
  (`common/length.h`). Both panels have had them the wrong way round at least
  twice, in three different places at once: a tooltip table that was right
  beside a ranking loop that was wrong, a label dictionary, and the two SQL
  aliases - and because `ORDER BY` used those aliases, the first hundred rows
  were chosen by the wrong column, so fixing the Python sort afterwards could
  not help. Named constants now, on both sides.
- **A ranking's candidate set decides the ranking.** The skills tab took the
  400 highest-level bots and looked for the best skill among them, so a bot of
  thirty with a Master skill stood behind four hundred fifties who had none and
  never appeared; the +9 tab filtered `vnum < 12000`, which was meant to
  exclude materials and excluded every shield (13xxx) and all jewellery with
  them - 9 items found against 17. Ask `item_proto` what is equipment
  (`type IN (1, 2)`) and score the whole set before paginating.
- **`item_proto` and `mob_proto` in the database are mirrors, rewritten from the
  txt at every boot.** `CClientManager::InitializeTables` runs
  `InitializeMobTable` -> `MirrorMobTableIntoDB` -> `InitializeItemTable` ->
  `MirrorItemTableIntoDB`, and each mirror is a `REPLACE INTO` of **every row**
  built from what `share/conf/mob_proto.txt` and `item_proto.txt` just said. So
  an operator who edits `player.item_proto` in HeidiSQL, ticks the box and
  restarts gets the shipped values back, every time, and nothing tells them why.
  Reported by Artur554 as "zmieniam bonusy w bazie ... i wraca do fabrycznych".
  The split is worth memorising, because it is exactly what he observed:

  | Read from the DATABASE (edit it, it sticks) | Read from `share/conf/*.txt` (editing the DB does nothing) |
  |---|---|
  | `refine_proto`, `shop` + `shop_item`, `item_attr`, `item_attr_rare`, `skill_proto`, `banword`, `quest_item_proto` | `item_proto` (item stats, applies, values), `mob_proto` (level, hp, exp, drops' owner) |

  And the txt files are baked into the game image, so editing them inside a
  running container is undone by the next rebuild - the same shape as the
  language switch. The only persistent path this project has for a txt table is
  `m2-rates`: keep what the operator asked for on a state volume and re-apply it
  before the cores read anything (`$STATE_DIR/wanted`, scaled from a `.m2orig`
  baseline). Anything that lets an operator change item or mob stats has to be
  built that way; there is no supported way to do it today, and saying "edit the
  database" is wrong advice.
- **Two numbers that mean the same thing must be the same number.**
  `PLAYERBOT_AUTOSPAWN_COUNT` was clamped to 1000 in `input_db.cpp` while the
  launcher's slider, its label ("LICZBA BOTOW (0-2500)") and
  `Set-PlayerbotCount` all offered 2500 - so an operator who raised it past a
  thousand got exactly a thousand bots and no line anywhere said so. Patch 0013
  makes the ceiling 2500 and logs `autospawn asked=%d, cut to the ceiling %d`
  when it fires. The real guard was never this clamp: `SplitPopulation` caps
  each kingdom at the identities it has and `SpawnRegistered` at what
  `LoadRegisteredBots` accepted, which is why asking for 3000 on this world
  yields 2012 (shinsoo=500 chunjo=1012 jinno=500) and not 2500 - the Chunjo
  cohort is 1500 seeded but 1012 usable, per the registry shortfall above.
  **Measured at that size**, because "2500 is untested" was the open question:
  2004 bots live, tick 2.5 s / 12.5 s / 3.2 s of every 60 on first / game1 /
  game2, and 33 core-seconds a minute for the whole game container - 0.55 of one
  core, on three cores' worth of world. Splitting the population between
  kingdoms is what makes that affordable; game1 carries the shared maps and
  costs four times what a village kingdom does.
- **A rollback that throws replaces the error that caused it.**
  `Invoke-M2PackageUpdate` rolled back **every** file in the package, not just
  the ones it had written - so a refused write to `pack/root.eix` was followed
  by restoring the backup onto that same unwritable file, which failed the same
  way, and *that* second exception is what reached the player. The copy loop's
  careful diagnosis was built and then thrown away one frame later. It took an
  end-to-end test with a real Deny ACL to see it: the message on screen was
  identical before and after the diagnosis was added, which reads exactly like
  "my fix did not work" and is not. Roll back only what was applied, and wrap
  each restore so the rollback can never be the thing that speaks.
  Related: `catch [UnauthorizedAccessException]` does **not** fire here.
  With `$ErrorActionPreference = 'Stop'` PowerShell 5.1 wraps a cmdlet's error
  in `ActionPreferenceStopException` and the typed catch is skipped - walk
  `.InnerException` the way `Test-M2AntivirusBlock` does. And `Copy-Item -Force`
  already overwrites read-only *and* hidden destinations, so neither is the
  cause of an access denial: what is left is an ACL, Controlled Folder Access,
  or a process holding the file. `New-M2AccessDeniedError` names which.
- **A staged engine file must stay in the engine's own encoding.** The
  engine sources are CP949 and `LC_TEXT("...")` compiles the bytes as written,
  so a file saved as UTF-8 looks up keys that `locale_string.txt` (CP949) does
  not contain. `char_battle.cpp` shipped that way from 1.31.6 to 1.33.2:
  every one of its thirteen Korean messages missed, `locale_find` returned the
  Korean itself, and the death-with-blessing message logged
  `LOCALE_ERROR: "용신의 가호로 ..."` 276 times in one support bundle. Check
  with `raw.decode('cp949')` before shipping a staged file; the patch itself
  (0010) was fine - it carries CP949 context and applies to the CP949 pristine
  on Linux, which is why only Windows installs, which get the staged file,
  saw it.
- **A count of "users" counts bot descriptors, and the client refuses a FULL
  channel.** `DESC_MANAGER::FuncWho` counted every descriptor with a character
  and `P2P_MANAGER` every remote login, so 2500 bots put the channel over
  `g_iFullUserCount` (1200) and the channel list said FULL ("if u have 2500
  bots channel is full", Dixdros). Patch 0014: the local count skips
  `IsBot()`, the P2P count subtracts registered pids at read time
  (`CountPlayerBots`, via `IsRegisteredBotPID` - which never loads the
  registry, because `IsRegistered` does and a failed load from a P2P login
  before `MapLocations` would fail the registry closed for the process), and
  `UpdateChannelStatus` logs `CHANNEL_STATUS: players= local= status=` every
  five minutes so the next screenshot has a number behind it. Verified: 969
  bots live, `players=0 status=1`.
- **A per-kingdom table can reintroduce a number a constant had already
  corrected.** `GetTeleportArrival(TELEPORT_GUILD_MAP)` carried the Teleporter
  quest's empire table, and for Chunjo that is (179500, 1000) - cell (3, 10)
  of `metin2_map_guild_02`, the unwalkable corner that `PLAYERBOT_M3_ARRIVAL_X`
  had replaced with Town.txt's (221900, 9200) long before; the three-kingdom
  travel switched `level30_weapon_to_m3` to the table and every bot sent to
  M3 stood at the corner with `nav_out=1` until the watchdog reset it, for
  ever (greess, 11 September, confirmed twice). All three rows are their
  map's own Town.txt now and the unit test pins them. When a table replaces a
  constant, diff the two before trusting the table's provenance.
- **A quest change is only live in the image, and the fast build never
  rebuilds the image.** Written down once already under "A fast build that
  ships only the core"; sprung again today: the test server's compiled
  `web_admin.quest` had no `BULK_ITEM` at all, so the mass-grant reproduction
  answered `unknown_cmd` for every online bot and `player_offline` for the
  rest, and read like a stall in the panel. `docker compose build game`
  before concluding anything about a quest - the second time this cost an
  hour of measurement.
- **The GM panel's window must not be able to stop the client loading.**
  `GMPanelWindow()` was built unconditionally inside `MakeInterface`; any
  exception in it - a widget a different client binary lacks, a locale key a
  different locale pack lacks - aborted the whole interface and the loading
  bar stopped at 100% with nothing on screen (five players on 10-11
  September; the stock root loaded on the same machines, and what fixed each
  of them was the stock root put back). The 1.33.3 root builds it in a
  try/except, writes the reason to `syserr.txt` and loads without it; the
  three entry points check `wndGMPanel` for None. The cause itself is still
  unknown - nobody has sent a `syserr.txt` yet - and the fail-safe is what
  turns the next report into one that carries it.
- **Bots are named by what the name says.** The pool is two written lists
  (jaksiezabic's, Iwakura's) plus names composed from *their* words in
  *their* shapes (`tokens_of`, `vocabulary`, four shapes in measured
  proportions), never from a hand-made word list - the first draft composed
  2100 names in one grammar and Iwakura's note was "bardziej rozne". The
  pairing SQL runs four passes: a name that names a class and a sex goes to
  that class and sex (`player.job` is the race: `% 4` is the class, 1/3/4/6
  are the female models), then class only, then sex only, then the rest to
  anyone. "ninja szamanka ale to sura" was a real screenshot. Underscore is
  refused because `check_name_alphabet` refuses it at the character screen.
- **The unsold-stock rule sat below the rule that made it unreachable.**
  `IsPlayerBotJunkItem` returned false for anything at
  `PLAYERBOT_PRECIOUS_REFINE` (+4) before it reached "scrap after six unsold
  stands", so the six-stand rule applied to nothing the counter keeps and a
  bag of +5 nobody bought was a bag for life - which is the bot that "stands
  in Joan browsing stalls and never levels" (gregoszky, davids998). The rule
  runs first now, up to `PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE` (+6); +7 and
  up is still never scrap.
- **The splitter and the merger must never share a tick without a clock
  between them.** `ManagePlayerBotPrivateShop` splits singles off every
  stack going on the counter before the walk to the pitch, and
  `ManagePlayerBotStackMerge` pours them back whenever no counter is open -
  and comes straight back after five seconds when its budget of four merges
  was used up. Two refusals at the far end of the open pass (the permanent
  bundle 71049, no yang for the 50200 bundle) returned with no
  `dwNextShopKeepTime`, so on the first 2.0.26 run of a world whose bots
  were parked on the ring the pass split, walked, refused, was merged back
  and did it again every five seconds - 8250 split lines in thirteen
  minutes, ~430 bots a core "biegaja w jedna i druga strone bez celu"
  (FanFar, 13 September). The cheap refusals (bundle, offline fee plus the
  fare reserve, two minutes after a spawn) sit before the scan now and every
  exit sets the clock. Measure it as identical `PLAYERBOT_SHOP: split`
  lines for one pid seconds apart, beside `PLAYERBOT_BAG: merged`.
- **The support bundle's syslog is a grep list, and a tag missing from it is
  a subsystem that never happened.** FanFar's bundle carried zero
  `PLAYERBOT_OFFLINE` lines while its own census counted 284 offline shops;
  `Metin2Launcher.psm1` names every tag it keeps. A new `PLAYERBOT_<AREA>:`
  tag has to be added there or no bundle will ever show it (OFFLINE, MARKET
  and BAG were added in 2.0.28). It sprang again on 13 September, and worse:
  **`PLAYERBOT_AI` was never on the list at all**, and that is the tag the
  refine pass logs under - every attempt writes `refine SUCCESS`,
  `FAILED_BURNED`, `FAILED_DOWNGRADED` or `SKIPPED` (playerbot_economy.h). So
  a bundle carrying 1165 `blacksmith visit` lines and not one refine line read
  as "the bots never upgrade anything", which is exactly what Iwakura reported
  from the rankings - and the log could neither confirm nor deny it. Skills,
  death, parties, loot and combat all log under the same tag, so the whole
  core of a bot's behaviour was invisible to every support bundle ever made.
  Before concluding that a subsystem does nothing, check that its tag is in
  that grep list.
- **`compose stop` leaves the containers, and a stopped container holds its
  volume.** `Reset-M2WorldToFreshInstall` ran `docker volume rm` on a stopped
  stack and got "volume is in use" four times for one player - the launcher
  never runs `compose down`. Remove what `docker ps -a --filter
  volume=<name>` lists first; `compose up` recreates it on the next start.
- **Every container is `restart: unless-stopped`, so an old installation takes
  the ports back on every engine start.** A machine that has ever held a second
  copy of this server carries a second compose project, and Docker Desktop
  starts all of them: five here (m2dep, m2zip, m2mt, m2fresh, metin2), each from
  its own folder, each publishing 7788, 7790, 7791, 11000, 13000-13002 and 3306.
  So "port jest juz zajety" came back after every quit of Docker Desktop -
  quitting is precisely what that policy waits for - and the only thing that
  holds is `docker stop`, whose manual-stop flag survives an engine restart.
  The preflight made it worse by asking about the panel's 7788 alone, so the
  collision that actually stopped an update was invisible to it: 7790, the
  advanced panel, reported by compose as "Bind for 127.0.0.1:7790 failed" only
  after the images had built for minutes. `Get-M2StackHostPorts` reads every
  published port out of the installation's own .env, `Get-M2DockerPortHolders`
  names the container, its project **and the folder it was started from** (the
  half that makes the advice actionable), and `Stop-M2ForeignPortHolders` stops
  the whole foreign project, because its siblings hold the other ports. Start
  and the update call it before compose runs; `start-server.ps1` imports no
  module, so it carries its own copy of the same lookup. Never a volume: the
  collision is containers, and a removed volume is the world. And note the
  tokenizer trap found writing the message: PowerShell accepts the typographic
  double quotes as string delimiters, so a pair of them inside a `"..."` string
  ends it mid-sentence and the whole module stops parsing.
- **A number in regen.txt is a group id, not a monster vnum.** The desert's
  regen.txt is 1172 lines and almost every one is type `r`, whose last field is
  a **group_group** id - and on that map those ids are 401 to 404. Read as
  monster vnums they name the Black Wind band, which lives on the three second
  villages (a3/b3/c3) and never sets foot in the desert, so the battle-horse
  trial counted kills no bot on it could ever make and every one of them read
  "Zdobywam konia bojowego na pustyni (0/100)" for ever (sosen, 13 September).
  The same misreading also produced a written claim in the source that the
  quest's own monsters "are not spawned anywhere here", and that sentence is
  what stopped anybody checking for months. Resolved through the **global**
  group_group.txt and group.txt - this map has no per-map group.txt at all -
  the desert carries exactly what the wiki says: Skorpion Lucznik 2105 at 998
  spawn points and Wezowy Lucznik 2107 at 760. Resolve the groups before naming
  a monster, and never grep: `grep -c '\b40[1-4]\b' regen.txt` answered 1168 of
  1172 lines here, because those digits are coordinates and respawn timers too.
- **Measure before tuning a budget.** `CPlayerBotManager::Update` logs
  `PLAYERBOT_LOAD:` once a minute: tick time, plans by distance bucket with
  their cost, deferrals, target searches, snapshot, map scans, saves, watchdog
  resets. CPU alone said "A*" once and the fix put every bot's map scan in the
  same second; the line says which plans, and how many milliseconds each.

- **`AddAffect` with `IsCube = false` looks an affect up by TYPE alone.**
  `CHARACTER::AddAffect(dwType, bApplyOn, .., bOverride, IsCube)` calls
  `FindAffect(dwType)` when IsCube is false and `FindAffect(dwType, bApplyOn)`
  when it is true - identically on both engines. So a second AFFECT_COLLECT
  paid with `bOverride` true and `IsCube` false overwrites whatever collect
  affect the character already had, whichever point it sat on. The Biologist
  pays one per row (ten movement speed for the Orc Tooth, five attack speed for
  the Curse Book), so the second reward would have taken the first one off.
  Copy what the engine's own `affect.add_collect` does, because that is what a
  player gets: `FindAffect(AFFECT_COLLECT, point)`, add the new value to the
  old one, then `AddAffect(.., INFINITE_AFFECT_DURATION, 0, true, true)` - sum,
  override, IsCube. `INFINITE_AFFECT_DURATION` is sixty years on both engines,
  which is where the hand-written `60L*60L*24L*365L*60L` came from.

- **A Biologist row is gated by `PLAYERBOT_HUNTING_MOB_HOMES`, and that is how
  an impossible row is switched off.** The chain runs past the Orc Tooth -
  `collect_quest_lv30` ends by starting lv40 and lv40 starts lv50 - but on this
  world lv50's specimen and key come only from 1001-1004, and all four stand
  solely on `metin2_map_deviltower1` (index 66), which `game2` hosts while every
  bot lives on `game1`. `GetActivePlayerBotBiologistMission` therefore steps
  over any row whose `mobVnum` is not hosted **in every pass, `last` included**:
  that pass takes the highest row left when the rest are outgrown, so without it
  every bot past fifty would have read "Pamiatka Po Demonie 0/15" for ever, the
  exact shape of the old "Korzen Gango 0/5". Giving 1001 a row in that table is
  all it takes to switch the row on the day the map moves. The panel keeps its
  own copy of the rule (`BIOLOGIST_UNREACHABLE`) and counts eight rows, not
  nine, or the card reads 8/9 for ever - the panel and the core have to step
  over the same rows or they describe different games.
  And measure the spawns rather than trusting either engine's table: r40250's
  Orc Valley is not mt2009's. The Orc Tooth's own `mobVnum` is 601, which stands
  there on **two** points, while what actually drops the tooth on mt2009 is
  `mob_proto.drop_item` on the Black Orcs 636/656, 197 and 196 points. Read
  every spawn file a map has (`regen.txt`, `base*_regen.txt`, `boss.txt`), give
  a map's own `group.txt` precedence over the global one, and check the answer
  against something already known to work before believing it.

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
  facts" apply); `IsPlayerBotM1HubForLevel` admits a hub from two over its
  median to three under, and the pid spreads the bot over the admitted ones.
  It admitted seven under until 2.0.12, and the seven were dogs: a bot of
  nine took the band-three hubs beside the band-nine ones, a third of every
  fight measured on the test world was six or more levels under the bot,
  and a level in the teens took two hours. `PERCENT_LVDELTA` is not what
  limits that - this engine's table still pays 90% at six under - the base
  experience is (Wild Dog 15, Blue Alpha Wolf 111). When no band holds the
  level, `CollectPlayerBotM1HubsForLevel` takes the nearest band, never the
  whole table. Re-measure before moving a hub; do not guess a band.
- **mt2009's INVENTORY_MAX_NUM is not the bag.** With
  `ENABLE_EXTEND_INVEN_SYSTEM` it is 135: two pages plus the horse inventory
  page, which `GetEmptyInventory` places into only for a character whose
  `GetInventoryMaxCount()` says so - a bot's never does. Every count the
  fragments made over `INVENTORY_MAX_NUM` saw 45 phantom free cells, so a
  chest judged to fit (`PlayerBotBagTakesGroup`) spilled on the ground, and
  bag-full, bag-pressure and stall rules were off by half a bag.
  `PLAYERBOT_BAG_CELLS` (compat: `INVENTORY_DEFAULT_MAX_NUM` there,
  `INVENTORY_MAX_NUM` on r40250) is the only bound the fragments use now;
  an engine array may still be sized by the engine's constant.
- **Yang is 64-bit on the 2.x line, and `%d` for it in a log with a `%s`
  after it is a core crash.** `YANG` is `long long` (typedef.h) and
  `GetGold()` returns it; the r40250 line's `int` habit survived in thirteen
  log lines. Twelve only printed wrong numbers, but the Teleporter refusal
  (`playerbot_travel.h`) had `gold=%d fee=%d to=%ld reason=%s`: the eight
  bytes of gold ate two slots, everything after shifted, and `reason=%s` read
  the map number as a pointer - SIGSEGV on every refusal, so the line never
  reached any log and the test world (bots with 250k yang) never refused
  anybody. kimakatsu diagnosed it from the stack trace alone (13 September).
  Cast to `(long long)` and use `%lld` for any yang in a format; the scanner
  that found the thirteen is three lines of Python over every
  `sys_log/sys_err/PlayerBotLogThrottled/snprintf` call, worth re-running
  after adding a log line with gold in it. The same shape waits in anything
  else 64-bit on this line (`GetTotalYangValue`, `price.yang`).
- **A consumable is its subtype, not its vnum.** Mikstura Ataku +10 is 39010,
  71014 and 76017; the bonus stones are 71084/71151/76023 (change) and
  71085/71152/76024 (add); the ItemShop copies (76xxx) carry no ANTI_SELL. A
  vnum list for boosters (`PLAYERBOT_BOOSTER_VNUMS`) and vnum-keyed stone
  lookups meant a bot handed the 76xxx kind from the panel vendored it
  (Pasywny). `IsPlayerBotBoosterItem` is USE_AFFECT with value0 510 (the
  engine's timed stat buff: value1 the apply, value2 the amount, value3 the
  seconds) or USE_ABILITY_UP; `FindPlayerBotBonusStoneCellLike` matches
  type+subtype of the vnum it is given; the junk rule exempts ITEM_USE of
  those kinds plus an auto potion with something left in it (see "Eliksir
  Slonca and Eliksir Ksiezyca are auto potions" below) and the Metin
  detector (counter goods). USE_AFFECT value0 512/513
  are not buffs (Rada Pustelnika, Zwój Egzorcyzmu removes affects) - the 510
  test is what keeps a bot from drinking an exorcism scroll.
- **A bot's stall on the 2.x line is a real ikashop offline shop (2.0.26).**
  `playerbot_offline_policy.h` (pure, unit-tested in
  `tests/playerbot_offline_policy_test.cpp`) is a process-wide request journal
  keyed by pid: the AI `Begin`s a request, the engine's `Send*DBPacket` marks
  it `Sent` and the `Recv*DBPacket` handlers mark it `Complete` - those hooks
  are exact-string edits in `ikarus_shop_manager.cpp` applied by
  `playerbotify.py` (`apply_playerbot_offline_shops`), so the header is
  included from an engine TU too and the map is `inline`. `EndCall` erases a
  request the engine refused synchronously (OpenMyShop returning without
  sending) and **a transmitted request is never retried on timeout**: ikashop
  has no idempotency key, so a retry could double an item or the yang; the
  bot's commerce pauses (`PLAYERBOT_OFFLINE: unresolved` in syserr) and its
  gameplay goes on. `playerbot_offline_shop.h` (after town.h) replaces the
  `OpenMyShop(sign, table, count, 0)` of the classic stall with a duration-1
  shop (8 h, 6000 yang, `aOfflineShopTime[1]`) and returns - the entity owns
  the stand, the bot hunts - then `ManagePlayerBotOfflineService` (before
  `ManagePlayerBotShopLifetime` in the tick) walks the owner back every 10-15
  minutes for one bounded visit: collect the shop safebox, reopen an expired
  stand that still has goods, add one item, reprice one item an hour, close
  edit mode. `playerbot_offline_market.h` (after market.h) is the buyer side
  over `GetPlayerBotOfflineShops()` and feeds the ledger. All of it under
  `PLAYERBOT_ENGINE_MT2009 && ENABLE_IKASHOP_RENEWAL`; r40250 keeps the
  classic stall untouched. The db core edit (`ClientManagerIkarusShop.cpp`)
  sends `SendIkarusShopBuyLockedItemPacket(peer, 0, ...)` on a refused lock,
  which the game side reads as `owner=0` = negative acknowledgement - without
  it the losing one of two buyers waited for ever. Things learned the first
  hour: the cores' logs live at `/opt/metin2/var/channel1/<core>/syslog`, not
  one level up; `OpenMyShop` refuses silently with a chat line to a bot
  descriptor, so `PLAYERBOT_OFFLINE: refused` names what it tests (a quest
  script running, the saddle, `GetPart(PART_MAIN)`, a busy window, the first
  line's antiflags) - 4 of 51 opens on the test world, all in the first
  seconds after spawn; `SetShopItems: not enough shop window` in syserr is the
  engine's own grid refusal. Both engine files and the db file ship in
  `server-update-files.mt2009.txt` (the .h and the db .cpp were missing from
  it). The classic panel's "shops" ranking reads `player.ikashop_offlineshop`
  on this line and takes the stand's map from the row, since the keeper is
  elsewhere. A sold-out shop is deleted by the engine and the owner's next
  service visit proceeds to the safebox anyway. A create for an owner whose
  shop the engine already holds is not a duplicate: `RecvShopCreateNewDBPacket`
  takes the "EXTEND DURATION" branch (duration, name and spawn refreshed, the
  entity recreated, the new lines added) and the db core's `CreateShop` does
  the same - that is how the native reopen works - so the boot race (a keeper
  parked at its pitch rolling a stall before the shop list has arrived) costs
  the bot 6000 yang and nothing else; `SubmitPlayerBotOfflineShop` still waits
  two minutes after the bot's own spawn. Restart measured: 48 shops before,
  54 after, entities recreated, 10 adds and 24 reprices in the first five
  minutes of service visits.
- **The mt2009 item finder searches offline shops; a stall is not one.**
  (r40250 semantics; on the 2.x line since 2.0.26 a bot's stall *is* an
  offline shop in `m_mapShops`, so the native finder lists it by itself.)
  `ikashop::CShopManager::RecvShopSearchItemClientPacket` walked `m_mapShops`
  (ikarus offline shops) and a playerbot's counter is a classic `CShop`
  (`OpenMyShop`, no duration). The category switch is a template now
  (`PlayerBotMatchShopCategory`) asked of an offline shop and of
  `CPlayerBotStallView` - a keeper's `GetItemVector()` seen through the three
  predicates the switch uses - and `PlayerBotSearchStalls` walks the PC map
  for keepers with a shop on the map within 7500 units. Inside `namespace
  ikashop`, `CShop` is the ikarus one: the classic class is `::CShop`.
- **The classic panel's item tables are r40250's unless the engine says
  otherwise.** `items.json` and `item_names_pl.txt` are the other engine's
  names and sizes; on mt2009 `localized_item_name`, the give-item search and
  `/static/item_defs.json` read `player.item_proto` (`locale_name` is the
  Polish name on this package, `size` the cells) - lazily, because the panel
  starts before MariaDB answers. `ENGINE_MT2009` is read near the top of the
  module for that reason; anything engine-specific above the old definition
  point used to be impossible. And the two name columns are
  `cp1250_polish_ci` while the panel's connection is latin1, so a plain
  SELECT lets the server convert and every letter latin1 lacks arrives as
  "?" - 2.0.13 shipped "Skrzyd?a Demona" after a check on a name without
  diacritics. Ask for `CAST(... AS BINARY)` and decode with `log_text`;
  verify a fix on a name with an l-stroke, never on "Gourou". The tooltip's
  base lines (attack, defence, fixed applies, level) read the same table
  through `ITEM_BASE`, and the JS fetches `/static/item_defs.json?v=<panel
  version>` because the answer is served with a ten-minute max-age - the
  first check of 2.0.14 showed the old table for exactly that reason.
- **"Teleport me to this bot" must find the character that is online, and
  the database cannot say who that is.** `last_play` is written on save,
  minutes after a login, so "newest last_play" was the previous character;
  the WARP row waited for somebody offline and stayed pending to fire on
  their next login. `api_admin_warp_me` queues for every recent human
  character, takes the first row the quest answers (it answers only for the
  online one) and deletes the rest. A WARP that times out is deleted too.
- **The panel's no-passphrase mode is decided by the bind address on the 2.x
  line.** `local_open()`: `M2PANEL_LOCAL_ONLY` (from `M2_PANEL_LOCAL_ONLY`)
  when set, else the installer's `local_only` in m2panel.conf, else never
  when `trust_proxy` (installer's nginx mode), else `M2PANEL_BIND_ADDRESS`
  is loopback. The launcher writes `M2_HOST_BIND_ADDRESS=127.0.0.1` for a
  single-player world and that is what opens the panel there.
- **A per-engine difference in a shared compose service lives in
  composify.py, not in the compose file.** `port/composify.py` renders the
  panel, seban, itemshop and updater services from `linux-port/docker/
  docker-compose.yml` into both mt2009 compose files between its BEGIN/END
  markers; an edit made to the rendered block is undone by the next render,
  which is how the updater override was lost once before it was found.
  `M2PANEL_ENGINE`, `PLAYERBOTS_ENGINE`, the version default and now the
  updater's entrypoint are all replacements in that script.
- **The 2.x line on Linux updates from the package zip, never from git.**
  The repository's root VERSION is the 1.x line's, `installer/install.sh`
  and the 1.x `m2-updater` stage `linux-port/` over whatever they find, and a
  2.x server that ran either ended with VERSION 1.33.3, the 1.x `m2-rates`
  ("the rates are being applied" for ever - the mt2009 game watches event
  flags instead), and a panel that could not update (l0st3k, 12 September).
  `linux-port-mt2009/tools/update.sh` is the launcher's package update in
  POSIX sh (manifest via the contents API, sha256, unpack over the server
  folder, `compose up -d --build`); the mt2009 updater container runs it in
  watch mode through composify's override. Test it in `python:3-alpine`
  with the folder bind-mounted - the Windows `python3` is the Store stub.
- **The mt2009 counter is ten columns wide, and the bots laid it out five
  wide.** `SHOP_PLAYER_WIDTH` doubles r40250's `SHOP_DEFAULT_WIDTH`; the
  right half (x 5-9) is locked rows 0-3 (`SHOP_SLOT_UNLOCK_PROGRESS_FLAG`)
  and premium rows 4-7, and `CanPlaceOnShopSlot` refuses either with a chat
  line a bot never reads - so every counter with a sixth line was refused
  whole, and "2500 botow, 0 sklepow" was true on every mt2009 world.
  `PLAYERBOT_SHOP_ENGINE_COLUMNS` (compat) is the engine's stride and
  `PlayerBotShopSlotToEngine` converts the bots' five-wide slot; the buyer
  side already indexed by the engine's position. The other half was
  `CanOpenShop()` - level 15 and `PLAYER_STATS_MONSTER_FLAG` >= 800, a rule
  for people that a bot re-earned after every world reset - which
  playerbotify.py now exempts a bot descriptor from. The kill flags do
  persist (`player_special_flag`, loaded on the bot path), so a settled bot
  passes anyway; a fresh world does not for hours.
- **InnoDB fsync per commit is the login hang.** The mt2009 line's player
  tables are InnoDB and the db core queues every save on one SQL_PLAYER
  connection, the same one a login's `player_index` SELECT goes through.
  With `innodb_flush_log_at_trx_commit = 1` a Docker Desktop disk managed
  ~70 fsyncs a second against ~100 writes a second from 2482 bots (special
  flags, quest flags, items): `return 1890/26/229` in the db syslog is that
  queue, and the login was answered 14-28 s later, to a client that had
  gone (`LoginSuccess - cannot find handle`). `99-metin2.cnf` sets 2. Read
  the db core's `[pulse] return q/r/f async q/r/f` line before blaming
  MyISAM locks; the db process itself sat at 0% CPU throughout.
- **A village hub is where a bot is sent, not where it stands.** The
  search range is `PLAYERBOT_SEARCH_RANGE` (6000) and a hub cell is 6400, so
  a bot at its own band's hub saw the dogs six kilometres off and the wander
  pass - which only runs on a tick nothing was worth attacking - never got
  the tick: 96 of 115 bots on Joan stood more than 2500 units from any hub,
  chain-killing whatever was next. Tightening the band alone
  (`IsPlayerBotM1HubForLevel`) doubled the level-ups and moved nobody.
  `REJECT_OUTGROWN_PREY` in the combat value policy is the other half: on a
  first village a monster `PLAYERBOT_VILLAGE_OUTGROWN_LEVELS` under the bot
  and further than `PLAYERBOT_OUTGROWN_CHAIN_RANGE` is refused, so the walk
  to the hub happens; what is under the bot's feet is still killed on the
  way, and defence, quest, material and equipment errands come before it.
  `PERCENT_LVDELTA` could not express this on the mt2009 line - its table
  still pays 90% at six under - the base experience is what falls (Wild Dog
  15, Blue Alpha Wolf 111). Measure it as the share of "Walcze z" lines six
  or more levels under the bot, and as level-ups per ten minutes, never as
  the exp column over a short window: the db core flushes the player cache
  minutes apart, so a 2.5-minute window of 350 bots read as +0.
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

- **The operator's word on an item is a file, and it sits above every
  rule.** `playerbot_item_policy.tsv` in the spool (`playerbot_config.h`,
  read like the weights: a stat every five seconds) says `keep`, `stall`,
  `merchant` or `drop` per vnum or per `type:N`. `GetPlayerBotItemPolicy` is
  asked at the top of `IsPlayerBotJunkItem` and `ScorePlayerBotShopStock`,
  and `drop` is honoured in `SellPlayerBotJunkAtMerchant` (a `RemoveItem`
  with no sale - the one place a bag is emptied on purpose). The classic
  panel edits it at `/ai/items` and refuses a malformed line by number,
  because the core skips one silently. What sent the operator asking:
  "boty sprzedaja ulepszacze i marmury handlarzowi" - a polymorph marble
  (ITEM_POLYMORPH) had no exemption in the junk rule, so the default
  `return true` sold it for three hundred yang, and a material nobody was
  short of went to the merchant from a bag with room to spare. A marble is
  counter goods now (`PLAYERBOT_SHOP_POLYMORPH_SCORE`), and a material is
  merchant scrap only under bag pressure.
- **What the anvil wants is a reserve, and the counter lists only what is
  over it.** `PlayerBotIsShortOfRefineMaterial` says "short" below twice
  the recipe count, so a bot short by one bought a pack of two, was no
  longer short, and listed both on its own counter at the price it had just
  paid (Zolc Niedzwiedzia x2 for 47 006, sizowski) - then was short again.
  `GetPlayerBotRefineMaterialReserve` is the same measure as a number; the
  scorer lists a material only when held minus reserve is at least a pack,
  and `SplitPlayerBotStallSingles` keeps the reserve in the base stack.
- **A surplus specimen is the merchant's, never the counter's.** The
  Biologist's herbs from a handed-in row scored 350 as counter goods, and
  the operator's rule is "sprzedaja u handlarza albo wyrzucaja". The junk
  rule takes them before the anti-sell test (quest items carry it, and the
  merchant leg is our own RemoveItem plus gold, not the engine's sale);
  the Orc Tooth stays a material and the soul stone is never surplus.
- **A pitch the bot's ground does not join is a loop, not a walk.** The
  stall's stable offset can land on a strip `server_attr` cuts off from
  the square; `MovePlayerBotTownLeg` then moved the goal onto the bot's own
  component, the walk ended there, the arrival test - against the pitch -
  failed, and the leg was planned again every tick for a night
  (AkhiGubernator: `goal moved onto reachable ground` 3691 a minute on map
  41). The open pass asks `CanReach` first, tries `PLAYERBOT_SHOP_PITCH_TRIES`
  salted offsets, and puts the stand off with `pitch unreachable` when none
  joins. And the rescue's search ring now fits inside the arrival radius
  corners included (`arrivalDistance / 71` cells): a corner cell 495 units
  out against a 450 arrival was the same loop from the other side.
- **The seban panel's "stuck" flag must read the action, not the words.**
  `is_stationary_activity` matched "lowi"/"ryb" in the status text and
  `BOT_ACTIONS` ended at 12, so every keeper (13), angler (14), browser
  (15), lurer (16) and rester (17) showed as `#13`..`#17` and "Mozliwie
  zawieszony". `STATIONARY_ACTIONS` names the seven actions a bot stands
  still in on purpose.
- **The db core names its own SQL charset, and it is not the database's.**
  `Main.cpp` defaults `LOCALE` to latin2 and `mysql_set_character_set`s
  every connection with it, so a CP1250 database (2.0.20) still lost l-stroke
  on the way through the db core - NPC names, item names, chat. The mt2009
  `m2-render-config` writes `LOCALE = "cp1250"` into `db/conf.txt`; verify
  with `mysql_set_character_set(cp1250)` in the db core's syslog, never with
  a name that has no diacritics.
- **The Teleport Ring recalls a stranded bot home.** A bot out of potions or a
  weapon on a frontier map (Orc Valley, desert, Sohan, Spider Dungeon) walked
  all the way to the exit portal; if it holds the Teleport Ring (70058, level
  30) `TryPlayerBotTeleportRingHome` warps it straight to its village with
  `TransitionPlayerBotMap` instead (Tieru: "musza isc po potki ... a sa w
  Dolinie Orkow czy na V1"). Same destination the walk would reach, so same
  core - only Chunjo bots stand on the shared frontier and their village is on
  game1. Not consumed; `s_mapPlayerBotTeleportRingReady` keeps it to the ring's
  30-minute cooldown, and only a *blocking* need (BlocksPlayerBotTravel) uses it.
- **The bag is tidied with MoveItem, which cannot lose an item.**
  `SortPlayerBotConsumablesToFront` (in the stack-merge pass, off behind a
  counter) pulls single-cell potions, then boosters, then chests/keys into the
  earliest empty cell before them - `ch->MoveItem` only moves, never deletes or
  overwrites, so the worst case is an item that does not move (Tieru asked for a
  sort and warned against losing items). Gear (size > 1) is left where it is.
  `GetPlayerBotSortPriority` is the order; `PLAYERBOT_SORT_MAX_MOVES` bounds it.
- **Dragon Coins get into the game from Metin stones and bosses.** The ItemShop
  currency (DRAGON_COIN) had no in-game source, so `CreateDropItem` (playerbotify
  on mt2009, the staged item_manager.cpp) rolls a Kupon SM voucher (80017, used
  it credits `pc.charge_cash`) at `g_iDragonCoinStonePermille` per stone and
  `g_iDragonCoinBossPermille` per boss - CONFIG tokens rendered by
  m2-render-config from `M2_DRAGON_COIN_STONE_PERMILLE` (default 3) and
  `M2_DRAGON_COIN_BOSS_PERMILLE` (default 50), the same shape as the Moonlight
  chest. mt2009 only; balance via .env. The vouchers are 80014/80015/80016/80017
  = 100/500/1000/50 coins (charge_cash_by_voucher.quest).
- **The armour merchant stocks three tiers, and the ladder wants ten.** NPC
  9002 sells body armour at levels 0/18/26 per class (11200/11220/11230 for a
  warrior), but `GetPlayerBotProgressionArmorVnum` walks the family by stride
  and names tiers at level 9, 34, 42 and up that no shop carries - so a bot
  between two stocked tiers, or above the top one, could never buy and a quarter
  of the cohort walked with an empty body slot (Tieru, 13 September).
  `FindPlayerBotBestMerchantSlotVnum` / `BuyPlayerBotBestMerchantSlotGear` buy
  the best piece the merchant actually stocks for the slot and class the bot
  qualifies for; the armour-merchant pass calls it when the exact tier is not
  sold. Guarded by `HasPlayerBotProgressionGear` so it never buys a second copy
  of a piece the merchant cannot better, nor a downgrade. Shields/helmets are
  shared (13xxx/12xxx), body is class-based (base+199 isolates the class).
- **A bot must be moved to open a stall for a spare it will not scrap.** The
  stall collector lists a worse duplicate of a filled slot, but only
  `GetPlayerBotShopReason` decides whether to *open* a counter, and a bot on two
  FMS +9 with an otherwise empty bag had no reason (Ciapek). `HasPlayerBotSellableSpare`
  (a weapon/armour at `PLAYERBOT_PRECIOUS_REFINE`+, slot filled by an
  equal-or-better worn piece, not itself an upgrade) is `PLAYERBOT_SHOP_REASON_SPARE`
  now, ahead of the trade roll and bag pressure.
- **A hand-tuned weapon is finished, and a change stone is not for +0..+4.**
  `PLAYERBOT_BONUS_WEAPON_LOCK_PCT` (25): a level-30 or level-75 weapon carrying
  an average-damage or average-skill line at or above it is finished for the
  reroll pass - `HasPlayerBotFinishedBonus` returns true - so USE_CHANGE_ATTRIBUTE
  never mixes it away ("dalem botowi fms z navi po 1000, debil zmienil bonusy",
  Ciapek). And `PLAYERBOT_BONUS_CHANGE_MIN_REFINE` (5) keeps the change stone off
  +0..+4 in both the worn and the bag-goods pass; the add stone is unrestricted.
- **The safebox page is a round trip behind the fee.** The first paid safebox
  visit sends HEADER_GD_SAFEBOX_CHANGE_SIZE and requests the load on the same
  tick, so the box that comes back has no valid position yet (`IsValidPosition(0)`
  false) and every deposit lands nowhere: `deposited=0`, 74 books in the bag for
  good (uxietoszef, on 2.0.17). The WAIT phase treats a not-ready box as "come
  back", keeps `bTownNeedSafebox` and reports nothing, rather than a phantom
  deposit; the next visit finds the page and fills it, no fee again.
- **The player level-up hunt is in quest/_unused on mt2009.** `levelup.quest`
  ships there and the infected mobs (901-906, 931-936) carry no kill hook, so
  `levelup.remain` never decrements and every bot reads "Polowanie: Lv X •
  Potwor: 0/40" for good, while the mission steered under-geared bots at its
  target on Mount Sohan (Tieru, 13 September). `GetActivePlayerBotHuntingMission`
  and `ManagePlayerBotHuntingProgress` return early under
  `PLAYERBOT_ENGINE_MT2009`; the classic panel's `hunting_progress_label` returns
  "" there. Bots hunt by the frontier draw and the level-banded hubs, which work.
  A bot far above its gear ("56 lvl with M2 items") was never seeded that way:
  every version of `generate_seed.py` in the history inserts `level = 1`, and
  both rendered `playerbots_seed.sql` files do too (checked 13 September - the
  operator's own world held 1500 bots of level 1-5). The only way a bot's level
  moves without experience is the classic panel's per-character "Ustaw poziom"
  card (`cmd=LEVEL` -> `UPDATE player.player SET level` / `pc.set_level`), and
  a bot raised that way keeps its village gear until the armour buy above and
  the weapon prize re-gear it. An earlier version of this note blamed "a
  level-50 cohort seeded"; there was no such cohort.
- **A skill book is vnum 50300 with the skill in socket0.** The classic panel's
  `item_full_name` spells it out ("Ksiega Umiejetnosci: Aura Miecza") from
  `SKILL_ID_NAMES` (the per-class skill tables flattened) when `ITEM_TYPES` says
  the item is ITEM_SKILLBOOK (17); the bag used to show only the bare book name.
- **The bonus history names the piece, not only the stone.** `ManagePlayerBotBonusReroll`
  writes `PLAYERBOT_BONUS_ADD` / `_CHANGE` / `_MARBLE` ItemLog rows on the target
  item beside the stone's own `PLAYERBOT_BONUS` removal, so the gear history says
  which weapon or armour a reroll was spent on (Tieru). The panel's
  `GEAR_HISTORY_HOWS` carries the three.
- **The single-player line has no panel passphrase.** `local_open()` returns
  true under `ENGINE_MT2009` (unless `M2_PANEL_LOCAL_ONLY=0` or the nginx proxy
  mode), and the seban collector seeds `setup_complete=1 auth_enabled=0` and
  migrates old installs off the wizard: one player at their own loopback-bound
  machine, no passphrase to invent (Tieru). An operator who exposes it turns
  auth back on.
- **A rod is not one vnum.** `fishing.cpp` rolls on every catch and turns
  the rod into its `GetRefinedVnum` - a new item - so `CountSpecifyItem(27400)`
  said "no rod" to a bot whose Wedka+2 lay in the bag, and it bought one per
  session until the bag was full of them. `CountPlayerBotRods` counts
  `ITEM_ROD`; `EquipPlayerBotRod` takes the highest vnum; a rod that another
  rod matches or beats is junk.
- **A tool in the weapon slot needs an exemption in the pass above it.**
  `ManagePlayerBotMining` claims the tick like fishing does, but the equipment
  pass runs in the upkeep group *above* both - and it swapped the pickaxe out
  for a sword between two swings. `mining_event` asks `GetWear(WEAR_WEAPON)`
  for an `ITEM_PICK` on the tick it fires, so every swing was refused and not
  one ore dropped, while the pass itself logged nothing but a bot re-equipping
  its pickaxe every thirty-two seconds - exactly the swing cadence, which is
  what named the cause. The rod's guard at that call is `!state.bFishingSession`;
  anything else that puts a non-weapon in that slot needs its own beside it.
- **A gate the AI cannot see is still a gate.** Fishing from thirty moved two
  numbers - `CHARACTER::fishing()` via playerbotify and the AI's own floor - and
  changed nothing, because the rod's `LIMIT_LEVEL` in `world.item_proto` is 50
  and `CanEquipNow` refuses the equip. On the 2.x line `item_proto` is read from
  **the database** (`PROTO_FROM_DB = 1`, `InitializeItemTableFromDB` on
  `SQL_WORLD`), not from `share/conf`, which does not exist there - so such a
  limit is changed with SQL in `apply.sh` and it sticks. Before concluding that
  a level rule lives in the AI, ask the item.
- **A log table taken from the other engine fails silently for ever.**
  `logschemify.py` had `fish_log` in `FROM_R40250`: eight columns, while
  mt2009's `LogManager::FishLog` writes six - so every catch failed with
  errno 1136, one syserr line each, and nothing was recorded. It went unnoticed
  for the life of the 2.x line because nobody ever fished on it. When a feature
  starts working for the first time, read `syserr` for what it woke up.

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
- **A rider reads, dresses, opens chests and is served from the saddle.**
  On mt2009 the engine refuses a rider exactly these: a skill (`UseSkill`,
  `CanUseSkill`), a blow from a horse that cannot fight, the rod
  (`fishing.cpp`), a polymorph marble, a tuxedo or a ride item
  (`EquipItem`); `CPVPManager::CanAttack` refuses a duel blow under grade
  two. `LearnSkillByBook`, a book's and a chest's use (`UseItemEx`), the rest
  of `EquipItem`, `PickupItem`, a quest NPC, the shop, the anvil and ikashop
  ask nothing about a horse, and r40250's book and chest do not either. This
  note used to say the book did, so the book pass climbed down, returned, and
  was put back in the saddle by the travel before its next visit: 5 121
  climb-downs in 36 minutes on the test world for 3 801 books, one rider down
  268 times for 15. The travel climbed down at the end of every leg too
  (near_destination and on_foot_action, 13 011 of 24 389 climb-downs), and
  14 502 of 24 379 mounts came within six seconds of a climb-down (Tieru,
  15 September: "Nie trzeba schodzic z konia by przeczytac ksiazke, sciagnac
  eq, ubrac eq, otworzyc jakies skrzynki, porozmawiac z npc, przejsc przez
  portal"). So `UpdatePlayerBotTravelMount` only mounts for a long leg, the
  book pass reads from the saddle, the stable keeper talks to a rider and the
  level is changed round a `StopRiding`/`StartRiding` the way
  `horse.advance()` does it (`SetPlayerBotHorseLevelInSaddle`), the stalled
  portal walk keeps the saddle, and every climb-down that is left holds the
  next mount for `PLAYERBOT_HORSE_TRAVEL_FLIP_HOLD_MS`
  (`SetPlayerBotRidingForTravel`). The first ten minutes after it: 214 mounts
  a minute against 682, remounts inside six seconds 20 a minute against 406,
  books read 231 a minute against 106. What is left belongs to the transport
  horse: a bot that mounts for a long leg and picks a target inside six
  seconds climbs down for it (714 in those ten minutes). The fishing session
  still sends the horse away with `HorseSummon(false)`, because `StopRiding()`
  alone parks it beside the angler for the whole session.
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
- **The town crowd is deliberate, and it is not capped - but it is the
  operator's to switch off.** The share of bots that rest after an errand is
  the `REST` key of the weights file (`GetPlayerBotRestPercent`, 100 by
  default, a slider in both panels since 2.0.9), nobody under
  `PLAYERBOT_TOWN_REST_MIN_LEVEL` (18) rests, and a rest needs counters on
  the map (`MayPlayerBotRestInTown` is the whole rule, asked when a rest is
  rolled and on every tick of one). The comment on the level floor does the
  arithmetic for the angler trigger alone - a session ends about once a minute, so
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
  **`BACKUP,FORCE` does not save an install that has already crashed**: archonek
  hit it on 1.32.5 with that setting in place, all three `log` tables gone
  (`log.log`, `log.levellog`, `log.shout_log`) and the server log repeating
  "last (automatic?) repair failed" - a damaged data file is past what the
  automatic pass can do. It is also why "update to the newest version" is the
  wrong advice and was tried first: the damage is in the volume, not the image.
  The symptom is specific - the **advanced** panel five-hundreds on its front
  page while the classic one is fine - because `dashboard` reads `log.log` for
  the fishing ranking and the classic front page never touches it. That page now
  answers with `handle_crashed_table` naming the table and the repair instead of
  Flask's bare "Internal Server Error", which is all the screenshot used to
  carry. The `log` database is history only: nothing in the game reads it, so
  truncating those three tables is a legitimate last resort, and saying so is
  what turns a dead server into a five-minute fix.
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

- **The mt2009 line has no `m2-rates`, `m2-gm` or `m2-lang` from r40250, and the
  panels did not know.** The rates page wrote the spool request, said "the
  server is restarting", and nothing in the mt2009 game image read it; the GM
  page said "works right away" of a request with no consumer. On this engine a
  rate is not a rewritten table: `CQuestManager::SetEventFlag` maps the event
  flags `mob_exp`, `mob_item`, `mob_gold` (and `_buyer` twins for premium) onto
  `CHARACTER_MANAGER`'s multipliers, and an event flag is a `player.quest` row
  with `dwPID = 0` (loaded by the db core at boot, pushed to every game core).
  So `persist_rates_mt2009` writes the six rows, the `web_admin` quest's `RATES`
  command (server timer, `game.set_event_flag`) makes them live in 2-3 s, and
  the mt2009 `m2-rates` only restarts the cores when nothing in game answered.
  GM rights: the list is re-read by `/reload a`, which the quest can run only
  as an online IMPLEMENTOR (`GM_RELOAD`); otherwise the panel says "after a
  restart". Before adding a panel feature that talks to the game container,
  check `linux-port-mt2009/docker/game/bin/` for the consumer.

- **Joan's gate is Joan's, and a first village is not a shape.** The town
  visit in a first village had an exterior/interior split with one gate leg
  at `PLAYERBOT_TOWN_GATE_*` - Joan's wall - taken for every first village, so
  on maps 1 and 41 the leg walked to coordinates that are nowhere and the
  misc merchant and the blacksmith were never reached: 0 blacksmith visits on
  `first`/`game2` against 131 on `game1` in eight minutes, bots reading "Ide
  do kowala" for good. Measured on the mt2009 `server_attr` (BFS at 50 units):
  on maps 1, 21 and 41 the blacksmith and the misc merchant sit in the weapon
  merchant's own walkable component, so `IsPlayerBotGatedVillage` names Joan
  alone and every other village takes the direct phases (`bDirect` in
  `playerbot_town.h`). When a per-map table replaces a constant, the walk
  legs *between* the table's points have to be checked too.
- **A direct village visits its trainer, or a level-five bot never leaves the
  pitch.** `GetPlayerBotFirstDirectTownPhase` was Bokjung's list - no trainer,
  no old woman - and 2.0.8 handed it to Yongan and Pyongmoo (`bDirect`). A
  Shinsoo or Jinno bot at level five, whose only need was the trainer, began
  a visit with phase NONE, finished it on the same tick, and the manager
  began it again on the next (`bNeedsProfession` bypasses the shop timer):
  five hundred bots a core standing at the pitch with `action=TRAVEL
  goal=CHOOSE_PROFESSION route=0/0`, reset by the watchdog every ninety
  seconds, never past level five (four Discord threads in one night). The
  direct list has the trainer and the skill reset first now, the trainer
  need is only set where `HasSkillTrainers`, and a direct visit with nothing
  on its list backs off instead of starting. When a phase list is reused for
  another map, walk every need the manager can set through it.
- **The support bundle is three cores, not one.** It shipped `game1`'s
  syslog and status alone, and 800 shared compose lines - fifty seconds on a
  world whose cores were resetting five hundred bots a minute and whose
  MariaDB logged an aborted connection every ninety seconds. The one line a
  crash report needs (`CORE DIED` and the backtrace under it) had scrolled
  out before the bundle was made. Since 2.0.11: syslog, syserr, status and
  `crash-*.txt` per core, the db core's syserr, a 6000-line game-only log and
  a supervisor grep over 200 000 lines.
- **A function that returns an array of one returns an object, and under
  StrictMode `.Count` on it throws.** `Get-M2DockerPreflight` took
  `Get-M2ExcludedPortRanges` bare: a Windows with exactly one reserved port
  range (or none) got "The property 'Count' cannot be found on this object"
  from Start, StartDocker and Logs alike, because all three run the
  preflight. Both launcher modules run `Set-StrictMode -Version 2.0`; wrap
  every function result you count in `@()`, at the call site, every time.
- **A crashed core leaves no core file on Docker Desktop.** The WSL kernel's
  `core_pattern` is a pipe to `/wsl-capture-crash` and the container's core
  limit is 0, so "(core dumped)" in the supervisor's log is all a support
  bundle ever carried. `libthecore/src/signal.c` (via `linuxify.py`) catches
  the fatal signals and writes `backtrace()` to `crash.txt` and stderr; the
  cores link with `-rdynamic` so the frames carry names; `m2-supervise` prints
  the file after `CORE DIED` and keeps it as `crash-<stamp>.txt`. Read the
  frames with `c++filt`. No other way to see where a player's core died.
- **`M2_PLAYERBOT_KINGDOMS` is 1 by default on the 2.x line since 2.0.8, and
  an old `.env` is migrated once.** `.env` is written at install and never
  rewritten, so a default flipped in `.env.example` reaches nobody who already
  installed; `Assert-KingdomsDefault` in `start-server.ps1` sets 1 and marks
  `M2_PLAYERBOT_KINGDOMS_DEFAULTED=1` exactly once, before
  `Add-MissingDotEnvKeys` (which would otherwise add the marker from the
  example and skip the migration). A 0 set after that is respected.
- **An update never deletes a file, so removing a quest means removing it from
  the loop.** 2.0.6 took `linux-port-mt2009/docker/game/quest/high_risk.quest`
  out of the repository and the Dockerfile's `for q in ...` still named it;
  every player's tree kept the file from 2.0.0-2.0.5 and compiled it on the
  next build ("po wgraniu update risk mode nadal jest"). Whatever a build
  reads from the player's tree is gated by the list in the Dockerfile, never
  by what the repository happens to contain.
- **Stock above the merchant's ceiling needs its own exit.** The junk rule
  scraps gear up to `PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE` after six unsold
  stands and the counter discounts to four stands; a +5 of another class had
  neither, so it rode round the stones for life (audit D14). After
  `PLAYERBOT_SHOP_UNSOLD_SAFEBOX_STANDS` unsold stands, under bag pressure,
  `CollectPlayerBotSafeboxDeadStock` sends it to the storekeeper with the
  surplus books - never scrapped, never worn (`IsPlayerBotWearableUpgrade`
  refuses it first). `mapStockFirstListed` is the registry: when a line first
  went up, so `PLAYERBOT_STOCK: to safebox` can say how long it was for sale,
  and the open line says what qualified and stayed in the bag
  (`no_line`/`no_slot`/`antiflag`). A new way for goods to leave the bag has
  to erase both maps.
- **A slider that only gates new decisions leaves the old ones standing.**
  `ShouldPlayerBotKeepShop` was asked when a stall opened and when a stand
  expired; a keeper standing on a roll taken under TRADE=250 kept standing
  after the operator set 25, for up to 25 minutes, and the four exceptions
  (Merchant, poor, full bag, dropper pressure) were never written down, so
  "minimalny suwak, a 180 z 280 handluje" had no answer. A stall carries its
  `EPlayerBotShopReason` now (status, open log, `PLAYERBOT_SHOP: census`
  every ten minutes), and `ManagePlayerBotShopLifetime` re-asks a rolled
  reason when `GetPlayerBotWeightsGeneration()` moves, ending a losing stand
  within `PLAYERBOT_SHOP_REEVALUATE_SPREAD_MS` by pid. Any other decision the
  weights file steers and then leaves standing for minutes wants the same
  generation check.
- **The four GM characters fill the admin account's free slots.**
  `gm_characters.sql` used to run only for an account with no character at
  all, so the operator who made his own character on `admin` before 2.0.4
  never got them ("tylko moja Tieru"). It now builds a temporary list of the
  classes the account lacks (class = `player.job % 4`), takes as many as the
  four slots leave free, and rewrites `player_index` with the existing
  characters first and the new ones after. Temporary tables are qualified
  (`player.tmp_gm`): apply.sh runs the file with no default database, and
  an unqualified `CREATE TEMPORARY TABLE` is "No database selected" at line
  55 with nothing else in the log.
- **The four GM characters are brought to a profile by a quest, not by the
  seed.** `gm_characters.sql` only creates what the admin account lacks; it
  cannot set a skill, wear a piece or fill an elixir, and it never touched a
  character that already existed - so the operator's own Tieru stayed a
  level-1 shaman with a fan while the three seeded ones lost pieces nothing
  put back. `linux-port-mt2009/docker/game/quest/gm_profile.quest` runs on
  every login of a GM on the `admin` account and, when the character's
  `gm_profile.version` flag is behind, brings it to the profile through the
  engine's own paths (`pc.set_level`, `pc.set_skill_level`,
  `horse.set_level`, `pc.give_item2_select` + `item.set_attr` +
  `pc.equip_slot`, `item.use`), asking first what is already there, so a
  second login and a restart change nothing (measured: ITEM_LOAD 21 on
  three consecutive logins). Bump the version when the profile changes;
  the seed's rows only make a fresh character complete on the character
  screen. Five things that cost the first test:
  `CanEquipNow` admits five equips a half second per player (the
  `ItemEquip` pulse), and a login can spend them before the login quest
  runs, so the ninja's five pieces were all refused in one tick with no
  reason anywhere but a chat line the tester never saw - the worn set is
  checked piece by piece (`pc.get_wear`, never the return value alone) and
  what a login refused is worn again from a timer a few seconds later;
  `sys_log` in a quest is `sys_log(level, text)` and a call with one
  argument is silently dropped (`_syslog` in questlua_global.cpp), so a
  quest that seems to run and log nothing usually did run; `qc` refuses any
  function not in `quest_functions`, which the package left without
  `pc.equip_slot`, `pc.unequip_slot`, `item.set_attr`, `item.get_max_stack`
  and `item.use` - the game Dockerfile appends them for the compile; the
  engine admits one elixir use a second per player (`AutoPotionUse`), so
  the two are switched on from a timer, one per tick; and
  `CHARACTER::UseItem` refuses every use while a quest script is running
  ("You cannot use this item if you're using quests"), which is exactly
  when a quest asks - `item.use` (`item_use0`, added by playerbotify.py)
  goes to `UseItemEx` after `CanUsedBy`, not to `UseItem`. Six refused
  uses in the log, and the elixirs the tester saw switched on were the
  tester's own clicks.
- **An auto elixir was born empty on this engine.** `ITEM_MANAGER::CreateItem`
  set socket 1 (the capacity used) and socket 2 (the capacity) both to
  `value0`, and the use path calls an elixir whose two are equal empty
  (`AUTOPOTION_IS_EMPTY`) - a loaded one gets its saved sockets back a
  moment later and never noticed, so only a fresh one from a quest, the
  panel or a shop failed. playerbotify.py makes a new one start with nothing
  used; `gm_profile.quest` sets socket 1 to 0 as well, as the belt to the
  braces. Socket 0 is the on/off flag; the affect (`AFFECT_AUTO_*_RECOVERY`,
  `dwFlag` = item id) is what actually heals, and only the use path makes
  it - writing socket 0 by hand gives an elixir that says "on" and does
  nothing.
- **A bot guild's name comes from Iwakura's list.** `data/guild_names_iwakura.txt`
  is rendered by `tools/generate_guild_names.py` into
  `playerbot_guild_names.h` (a `playerbot_*` file, so every stage ships
  it); `GetPlayerBotGuildName(pid, step)` walks the pool from a pid-based
  offset and `FoundPlayerBotGuild` takes the first name `FindGuildByName`
  does not know, skipping what `GUILD_NAME_MAX_LEN` refuses (fourteen on
  mt2009, twelve on r40250). Until 2.0.13 it was two names per kingdom,
  so six guilds filled the world. The whole system was measured live for
  the first time then: 100 Chunjo bots raised to level 41 with the fee,
  seven guilds founded within three seconds of spawn (one in twelve by
  `PLAYERBOT_GUILD_FOUNDER_SHARE`), 24 members three minutes later through
  `RequestAddMember`, `player.guild` and `guild_member` filled. `gold=` in
  the founding line is in thousands.
- **A guild has a tier, and the tier is a percentile, not a threshold.**
  Since 2.0.60 (`playerbot_guild.h`): `GetPlayerBotStrength` is one number a
  bot (level, the hand weapon's blow from `GetPlayerBotWeaponHitDamageAt`,
  the build's skill levels, the horse, the body armour's score);
  `RefreshPlayerBotStrengths` cuts each kingdom's bots into percentiles every
  ten minutes (`PLAYERBOT_GUILD_TIER_PERCENT`: elite 3, strong 15, medium 50)
  and the first census waits one interval, because the first minute after a
  start holds the spawn window's first bots and not the kingdom. A guild is
  founded at its founder's tier, stepped down while the kingdom's elite or
  strong count is full (`PLAYERBOT_GUILD_TIER_MAX_PER_KINGDOM`); a bot guild
  from before the tiers is adopted at its master's tier on the first check
  after the census, and only while the master is in this core's world - a
  master counted at zero would make every old guild ordinary. The tier lives
  in `player.playerbot_guild` (apply.sh creates it; `DBManager::DirectQuery`
  reads it once, an INSERT ... ON DUPLICATE KEY writes it) because a guild
  outlives every restart. Recruiting is the whole roster of the kingdom,
  strongest first, above the tier's floor - not a sector sweep - so a guild
  of the strong is not a guild of whoever stood at the pitch; a member whose
  own tier is better than its guild's leaves for a better one with room
  (`TryPlayerBotGuildPromotion`, one in six hours, three a kingdom a census).
  The experience offer is `CGuild::OfferExp`, which on both engines takes
  the amount off the member and gives the guild a hundredth: the hourly
  share (`PLAYERBOT_GUILD_EXP_OFFER_PERCENT` of what was gained since the
  last offer, never more than the level holds) is what makes twenty-five
  guilds of level one - none of them had ever been offered a point - into
  guilds that level: on m2zip the first offers came an hour after the
  census (2 100 435 exp, ten percent of the 21 million a bot of seventy had
  gained in the hour), four guilds reached level two within a minute of
  them and every one of the twenty-five held experience five minutes later.
  The first census on that world cut 366 bots a kingdom at floors of about
  91 500 (elite), 87 500 (strong) and 74-79 000 (medium), adopted all
  twenty-five guilds in a minute - three elite, four strong - and moved
  nine bots to better guilds. `AFFECT_EXP_BLOCK` exists on mt2009 and not on r40250;
  OfferExp itself refuses it where it exists, so the fragment does not ask.
  `CGuild::UseSkill` works only inside a war arena (`IsWarMap`), so the
  points the master spends (`SpendPlayerBotGuildSkillPoints`, a staircase
  from Blood of the Dragon God) are for the guild's sake and for players in
  a bot guild; the bots' own wars cannot use them.
- **A bots' guild war is the engine's field war on the kingdom's guild
  map.** `playerbot_guild_war.h`, after targeting.h: GUILD_WAR_TYPE_FIELD
  needs no map (`GuildWar_IsWarMap` says so), which matters because the
  arena maps 110/111 are hosted on `first` in both layouts and a bot cannot
  reach them. A war is `RequestDeclareWar` from one master, then the same
  call from the other once its guild reports GUILD_WAR_RECV_DECLARE (a
  round trip through the db core, so a minute later), thirty minutes on the
  db core's clock, kills counted by `CGuildManager::Kill` and the ladder
  settled by the db core; `UnderAnyWar()` with no argument means any type
  (its default is GUILD_WAR_TYPE_MAX_NUM). One war a kingdom at a time,
  the pair the closest tiers of the guilds with `PLAYERBOT_GUILD_WAR_MIN_ONLINE`
  bots in this core's world, the rally the open, fightable ground nearest the
  map's Town.txt point (`GetTeleportArrival(TELEPORT_GUILD_MAP)`), a side
  apart. **Two of the three Town.txt points are inside the map's safe zone**:
  on metin2_map_guild_02 and _03 the cell carries ATTR_BANPK for about two
  kilometres around (measured on the mt2009 server_attr, 16 September), and
  `battle_is_attackable` refuses every blow on it, so the first wars there
  ended 0:0 after thirty minutes while Shinsoo's on guild_01, whose point is
  open ground, ran to 17074:14107; the sides 1500 units off were blocked
  cells on two maps besides. `FindPlayerBotWarGround` walks the sectree's
  attributes in rings from the point and refuses BLOCK, OBJECT and BANPK;
  `GetPlayerBotWarRally` keeps the two sides per map. Any other "meet here"
  point on a guild map wants the same test. The fight is the
  duel's shape (buffs, the caster's range, the gap closer, the basic blow);
  the foe in hand is kept while it stands and the roster searched only when
  it is lost, because that search is every bot in the world on every tick.
  The WARS key of the weights file switches new declarations off; a war
  under way is fought out. No bot master accepts a player's declaration.
- **The 2.x line's ItemShop is in the game, and a bot buys there as a client
  would.** Not the PHP shop of `linux-port/docker/itemshop` (that one writes
  `player.item_award`): mt2009 has `CItemShopManager` (`/itemshop open`,
  `/itemshop buy <index> <qty>` from the client, `common.itemshop_items` -
  150 lines on this package), priced in Dragon Coins (`account.account.cash`)
  and Dragon Marks (`cash_mark`, credited one for one for every coin spent).
  The coins enter the world as Kupon SM vouchers (80014-80018, ITEM_QUEST,
  the drop `CreateDropItem` rolls by `M2_DRAGON_COIN_*_PERMILLE`) and the
  package's compiled `itemshop_manage` quest cashes one on use:
  `pc.charge_cash` -> `CItemShopManager::AddCash` -> HEADER_GD_REQUEST_CHARGE_CASH
  -> the db core's `ChargeCash` (`update account set cash = cash + n`), plus a
  row in `log.itemshop_dragon_scroll`. Its `item.remove()` takes the whole
  stack for one charge, so `playerbot_itemshop.h` cashes a voucher itself,
  one unit at a time, through the same AddCash and the same log row. The
  charge is asynchronous, so the account is read *before* a voucher is
  cashed and a purchase waits for the next look ten minutes later: the
  first build read the account right after the charge, put the old balance
  back over the coins just added, and would have bought nothing for an hour.
  And the wishes are a list, not a pick: the first build chose one wish by
  priority and a bot whose first wish was the marks' Blessing Scroll, with
  no marks to its name, never reached the hairstyle its fifty coins would
  have bought - eighty-five accounts at fifty coins and not one purchase in
  fifteen minutes. `CollectPlayerBotItemShopWishes` lists them all and the
  buyer takes the first the balance pays for; `saving_looks=` in the census
  counts the looks that found a wish and no money for it. With that in
  place on m2zip (30 permille from stones): eighty-seven vouchers cashed in
  the minute after a start, twenty hairstyles bought in eleven minutes by
  the one-in-four bots holding fifty coins, each worn on the next look
  (`EquipItem` refuses inside a second and a half of a blow, so the wear is
  retried every look), three "cannot buy now" for a full bag, no refusal
  from BuyItem, the rows in `log.itemshop`. No Kamien Duchowy was bought:
  the wish mirrors the training pass, which wants a skill already at
  G1..G9, and no bot on that world had one.
  A purchase is `BuyItem` behind `playerData->SetItemShopBrowse(true)` - the
  flag the window sets and `IsBusy` reads, so it goes back off on the same
  tick - and the goods come by `AutoGiveItem` (a full bag goes to
  `item_award`, which the bot asks about first with `HasSlotForItem`). What
  the shop holds and what each premium does, measured on 16 September: the
  VIP items (USE_VIP, value0 the PREMIUM_* type, value1 the hours) and the
  Przepustka Triumfu (72199, 299 coins, `premium_expire` for 720 hours) buy
  the engine's premium - `GetPremiumRemainSeconds`: +50 to the exp
  `rateFactor`, the `*_buyer` twin rates, a doubled gold-drop percent,
  autoloot, +10 to the fishing chance, the offline shop's premium slots and
  limit, emotions, the premium channel and NPC 20088's zone without the
  71095 ticket. **Every bot already holds the subscription for five years**
  (`SpawnBot` sets `iPremium`), so a bot never buys VIP - `BuyItem` refuses a
  VIP item to a subscriber anyway - and what it buys is Kamien Duchowy,
  the change stone, with marks the Blessing Scroll and the Dragon God's
  potions, and one bot in four a hairstyle (ITEM_COSTUME/COSTUME_HAIR, which
  the junk rule now keeps and the costume block lets through). The engine's
  own purchase log is `INSERT INTO itemshop` in the log database, a table no
  dump ever defined; logschemify makes it. At the default permilles a bot
  finds a fifty-coin voucher about once a month - 97 vouchers in three days
  across a thousand bots - so the switch shows itself on a world whose
  operator raised `M2_DRAGON_COIN_STONE_PERMILLE`, not on the defaults.
- **"Not scrap" is not "worth a refine".** The junk rule keeps a great deal
  on purpose - a collector's stock, +4 counter goods, prize lines - and
  `IsPlayerBotRefineBagCandidate` was "equipment and not junk", so the
  blacksmith pass raised all of it with the bot's own yang: 12 945 refines
  an hour on the test world, 4 227 of them on pieces ten or more levels
  under the bot, and a player's bot under a Guillotine Blade +4 raising
  level-one swords and wooden earrings +1 by +1 with its last 14 000 yang.
  A bag piece is refined only when the bot will wear it:
  `IsPlayerBotHigherTierSpare` or `IsPlayerBotWearableUpgrade`. Worn
  pieces are unchanged. A rule that decides what to *keep* must not be
  reused to decide what to *spend on*.
- **The ikashop client marks only its own shop entities.** Its search
  result carries VIDs, and the client resolves them against the list it
  built from `EncodeInsertShopEntity` - a playerbot keeper's VID lists the
  stall and highlights nothing, marks nothing on the map. What a keeper
  can get is a `HEADER_GC_SEPCIAL_EFFECT` (the header is misspelled in the
  engine) sent to the searcher's descriptor alone: `PlayerBotSearchStalls`
  puts `SE_CHINA_FIREWORK` (2.0.15 used `SE_LEVELUP_ON_14_FOR_GERMANY`, an
  advert - see the note further down) on every keeper found and a chat
  line says how many. Anything better needs the stall to be an ikashop
  entity, which is the other shop system.
- **A build job per core is a build that dies on a laptop.** Docker Desktop
  gives the build VM half the host's memory; the big translation units take
  over a gigabyte each at -O2 -g; `make -j$(nproc)` on an 8 GB machine
  thrashed at "game builder 2/3 67%" and read as a hang. Both game
  Dockerfiles take the smaller of `nproc` and MemTotal/1400 MB unless
  `MAKE_JOBS` (`M2_MAKE_JOBS`) is set, and print the choice.
- **mt2009 fishing is four gates and a minigame, and the pass was the one
  nobody could pass.** `CHARACTER::fishing()` there wants level 50, maps
  1/21/41, `fishing_onboarding.completed`, bait in the rod's socket 2 and
  `UNIQUE_ITEM_FISHING_PASS` (27620, a day of real time) worn; the catch is
  a client minigame (`FishingGameStart`, a bar the client's position must
  stay under), which `ManagePlayerBotFishing` already plays server-side by
  moving `m_iFish_position`. The session set the flag; nothing sold the
  pass; `WantsPlayerBotFishingTrip` refused without it - so no bot on an
  mt2009 world fished until 2.0.16. `EnsurePlayerBotFishingPass` creates
  one for `PLAYERBOT_FISHING_PASS_PRICE` and wears it, the way the
  Forgetting Scroll is bought. Under 50 the trip is never planned. And the
  bank tables were measured on r40250's server_attr: Joan's stand
  (67175,158125) has no water beside it on mt2009's map, and the bot that
  drew it stood there to "never_cast" every session while its neighbour
  caught fish - fishing() explains its refusals to the client only, so the
  session logs every gate itself (`fishing() refused ... water=0`), and a
  stand with no water is marked dry (`s_setPlayerBotDryFishingStands`) and
  never drawn again on that core.
- **The Metin book top-up ignored the level curve.** The engine's tables
  fade every drop by `aiPercentByDeltaLev`; the guaranteed book from a stone
  (playerbotify's CreateDropItem edit, patch 0006 on r40250) did not, so a
  player of forty-six farmed level-five stones for a book each.
  `PLAYERBOT_METIN_BOOK_LEVEL_DELTA` (15) ends the top-up; the stone's own
  roll still applies. The r40250 patch still has no such cap.
- **A refusal remembered for everybody is a feature switched off for
  everybody.** `s_mapPlayerBotChestRefused` was keyed by vnum: the first bot
  in the tick whose bag had no room for the Moonlight chest's group marked
  50011 refused for the whole population for ten minutes, and with two
  thousand bots there always was one - 165 663 unopened chests in a
  player's bags. Keyed by (pid, vnum) now, and the two boxes the map was
  written for (50192/50193) are a LIMIT_LEVEL the pass reads itself
  (`IsPlayerBotChestLevelLocked`), never asked of UseItem. Any per-world
  memory of a per-bot failure wants this check.
- **The mt2009 CONFIG never carried MOONLIGHT_CHEST_PERMILLE.** config.cpp
  there reads it (playerbotify), item_manager.cpp rolls on it, the Dockerfile
  appends the chest's group - and `linux-port-mt2009/docker/game/bin/
  m2-render-config` did not emit the token, so `g_iMoonlightChestPermille`
  stayed 0 and no Moonlight chest ever dropped on a 2.x world (zero in six
  hours on the test stack). A CONFIG token added to one line's renderer has
  to be added to the other's; the two scripts are separate files. And the
  renderer only sees what compose hands the game service: the mt2009 compose
  never passed `M2_MOONLIGHT_CHEST_*` or `M2_DRAGON_COIN_*`, so a value in
  `.env` stayed in `.env` and CONFIG carried the defaults (seen on m2zip on 15
  September: no such variable in the container, 10 and 300 in CONFIG). Both
  compose files pass them since 2.0.52; edit `docker-compose.yml` and re-run
  `port/composify.py`, which renders the deploy file.
- **SE_LEVELUP_ON_14_FOR_GERMANY is an advert.** On the mt2009 client that
  effect id draws "Noch 1 Level-Up! ... siehe www.metin2.de" over the
  character; 2.0.15 hung it over every stall the finder found. Pick effects
  from what the client actually maps them to (SE_CHINA_FIREWORK is a
  firework everywhere), and never a *_FOR_GERMANY one.
- **A queue whose head is offline looks like a queue that stopped.** The
  grants worker hands `MAX_PENDING` (ten) rows to the game and the quest's
  player timer serves only a row that names an online character; an offline
  one sits 30 s for the sweep, 60 s for the worker's withdrawal and comes
  back two minutes later. Measured with 1500 registered bots and 349 in the
  world: done=350 in two minutes, then queued=10 for good - the ten places
  were all offline names, and every online recipient waited behind them.
  That is the audit's "stops at 333". `pick_waiting` in `item_grants.py`
  serves the collector's snapshot of live bots first and lets offline probes
  hold at most `OFFLINE_MAX_PENDING` places; the page shows the worker's
  heartbeat and the age of the oldest row in each state, so the next report
  can say which of "worker dead", "nobody in game reads the queue" and "the
  rest are offline" it is.
- **A slider that reaches only a ranking is a slider that does nothing.**
  `PLAYERBOT_WEIGHT_PARTY` had one reader - the planner's rank of
  `BOT_GOAL_PARTY_CHALLENGE` for a bot already in a party - while who may
  be in a party was `IsPlayerBotPartyEligible`: the party-fighter role (a
  tenth of the population, drawn at login) anywhere, and camp level on the
  frontier. "Grupy (PT)" at 25 and at 250 gave the same thirty-seven bots
  in groups out of a thousand (jaksiezabic, 12 September). The slider now
  sets the share admitted: `PLAYERBOT_PARTY_COHORT_PER_MILLE` (200) scaled
  by the weight off the frontier, the whole map as the base on it, and
  `GetPlayerBotPartyDraw` puts the role in the first hundred places of a
  pid-stable draw so it is the last share the slider takes away. A bot
  outside the cohort leaves its party on its next party check ("left
  party outside party cohort"), which is what makes a lower setting
  visible within a minute; a higher one fills in over the 1-3 minute solo
  wait. `PLAYERBOT_PARTY: census` every ten minutes is the measurement.
  Before wiring a weight, grep for every reader of it - one reader in a
  ranking is not a feature.
- **A refined piece never sells for less than the blacksmith was paid.**
  `GetPlayerBotShopAskingPrice` scaled the merchant's price of the *base*
  item by the median wallet, and on a fresh world both are pennies: Miecz+4
  at 90 yang, Sejmitar+4 at 582 (djariczek, 12 September), where the four
  fees alone are 6100. `GetPlayerBotRefineInvestment` (playerbot_town.h)
  walks the ladder from `vnum - refine` through `dwRefinedVnum` and
  `wRefineSet` into `CRefineManager::GetRefineRecipe`, summing `cost * 100
  / prob` - the expected spend per step, which is the risk premium - and
  refuses a piece whose walk does not land back on its own vnum. It floors
  every exit of the asking price and the two markdowns at the counter. The
  mt2009 table lives in `world.refine_proto` (not `player`), Miecz +1..+9:
  400/800/1600/3300/6600/13300/20000/30000/50000 at 90/90/90/90/80/60/50/40/30;
  Boski Luk Moreli +9 comes to 1.9M this way against the flat 900k, which
  is the floor doing its job. Materials are not counted.
- **A weighted search that reopens closed cells is unbounded, and the tick
  pays.** `FindFinePathInRegionCorridor` runs weighted A* (2-3x) inside
  the abstract corridor and reopened any cell reached cheaper after it
  was expanded; with water penalties making many routes nearly equal on
  Orc Valley the same cells were expanded again and again - single plans
  of 4-5 s (`far plan map=64 cost_ms=5066`), a game1 tick of 20-32 s per
  60 and a client lagging every 10-20 s (sizowski, 12 September), while
  the same map on our stack planned in under 404 ms. A popped cell is
  closed by storing `-g - 1` in `m_nodeCost` (the neighbour test never
  reopens a negative cost, the stale test drops its heap entries), the
  search is capped at `PLAYERBOT_NAV_MAX_CORRIDOR_EXPANSIONS`, and past
  the cap it returns the partial route to the cell nearest the goal:
  `TPlayerBotAIState::bRoutePartial` makes `MovePlayerBot` replan from its
  end instead of reporting an arrival, and a partial route is never
  cached. Every far plan and every plan over `PLAYERBOT_NAV_SLOW_PLAN_MS`
  logs `abstract_ms regions fine_ms expanded partial` - read those before
  blaming the machine: target searches cost the same 42 us per search on
  both worlds, so a per-plan gap of 16-90x is the search, not the CPU.
- **`CItem::GetRefineLevel` on mt2009 is a syserr line per call for one
  potion.** It parses the plus out of the base name and the locale name
  and logs a mismatch; "Mikstura Ataku +15" (71034/76018, ITEM_USE) has a
  bare "+" in the Korean name and "+15" in the Polish one, and every bag
  scan that asked a potion for its refine wrote to disk - 2773 lines in
  twelve minutes on one core. playerbotify.py returns before the locale
  check unless the item is a weapon or armour; item.cpp ships staged
  (mixed line endings - anchor without a newline).
- **The db core writes `log.ikarusshop_log` and no dump defines it.**
  `CClientManager::IkarusShopLog` (ClientManagerIkarusShop.cpp) inserts
  who/itemid/what/shop_owner/extra/vnum/count/yang (+cheque under
  ENABLE_CHEQUE_SYSTEM) for every offline-shop action; `logschemify.py`
  carries the table now. The db syserr is where such holes show -
  `Table 'log.X' doesn't exist` - and the bundle ships it as syserr-db.txt.
- **The first crash files, read.** sizowski's 12 September bundle: six
  SIGSEGV between 09:08 and 11:30 UTC, all on 2.0.11/2.0.12 (the launcher
  log says which version ran when - map crash stamps, which are UTC, onto
  it), none in 5.5 h since 2.0.13. Three end in
  `CHARACTER::GetMoveMotionSpeed+0x181` under `Goto` from the bot tick -
  the inlined `GetWear(WEAR_WEAPON)->GetProto()` on a dangling item, which
  fits the 135-cell bag scan 2.0.13 removed. Two are libc memmove under
  four anonymous frames from `Update+0x51e2`: symbolising those needs the
  2.0.11 binary (a worktree build), not this one. The named frames come
  from `-rdynamic`; the anonymous ones are our namespace.
- **A scroll in the bag is the reason to go on, and the personality's
  ambition is not.** `GetPlayerBotRefineTarget` drew +6/+7/+8/+9 by
  personality (six in ten stop at +6) and every pass asked it - so a bot
  with Blessing Scrolls in the bag stopped at +6 and put the scrolls on its
  counter: 660 scrolls in 482 bags on the test world, 9 of 45 refines to
  +7 under one ("mnostwo zwojow, boty ich nie uzywaja"). Under a Blessing
  or Dragon God scroll `DoRefineWithScroll` on mt2009 never burns the piece
  (value0 NO_REDUCTION_WHEN_FAIL keeps it, the default hands it back a
  level down; only REFINE_BONUS_SCROLL destroys), so the target is
  `PLAYERBOT_SCROLL_REFINE_MAX_PLUS` while `CountPlayerBotSafeRefineScrolls`
  says one is there - one function, all six callers follow - and the
  stall keeps the first `PLAYERBOT_REFINE_SCROLL_KEEP` back while
  `PlayerBotWearsScrollWork`. Measure it as `PLAYERBOT_AI: refine ...
  scroll=1` by plus.
- **Starter and level chests are opened at the level they unlock, and the
  database says so.** "Bots at 30 still carry the level-1 chest" (Latino)
  was the pre-2.0.17 world - the vnum-keyed refusal map. On the test
  world after 2.0.17 every giftbox held by a bot at or above its
  LIMIT_LEVEL is gone (2319 level-20 chests, all in bags under 20; the
  only level-1 chests in level-1 bags); the two gates that can still keep
  one are room for the whole set (`PlayerBotBagTakesGroup`, silent, waits
  for a town visit) and the engine's own "You have not received anything"
  (a refusal, and then goods). Query `player.item` joined to `player` by
  `limitvalue0` before believing either side of such a report.
- **The client's night is a client option; the server's "night" is the
  Christmas flag.** This root's `game.py` has `__SetNightMode` behind
  `systemSetting.GetNightMode()` (0 off, 1 always, 2 auto 22:00-06:00 by
  the PC clock, set in uigameoption) and nothing in the packet stream
  reaches it; `xmas_snow` goes to `__XMasSnow_Enable` = the song plus
  `background.EnableSnow(1)`. Night without snow from the server needs a
  new event flag in the client's flag dict wired to `__SetNightMode` - a
  root repack and a client package - so `ManagePlayerBotNight` keeps
  raising `xmas_snow` until that ships.
- **A free cell is what the item grid says, not a cell with no item
  pointer.** `SetItem` puts `pItems[cell]` in the top cell only and marks
  `bItemGrid` for every cell the piece covers, so counting
  `!GetInventoryItem(cell)` called the bottoms of every weapon and armour
  free. `CountPlayerBotFreeInventoryCells` asks `IsEmptyItemGrid(cell, 1)`
  since 2.0.20; before that the stall pass looped on it - split "keeping
  three free cells" that were sword bottoms, no cell for the bundle, merge,
  split again, every three seconds (6066/4054 lines in fifteen minutes,
  sizowski's "2 sklepy") - and bag-full, bag-pressure and the chest
  reserve were off by the height of the gear. `PlayerBotBagTakesGroup`
  already laid items out by size; its r40250 fallback repeats the grid loop
  because consumables.h comes after gear.h in the include order.
- **Prices are Iwakura's sheet, rendered, not typed.** `data/iwakura_ceny.txt`
  is his price list v1.0 as he sent it (14 September), and
  `tools/generate_iwakura_prices.py` renders all of it into
  `playerbot_price_tables.h`: the yang-rate curve, gear by family and refine
  (weapons, armour, boots, bracelets, necklaces, earrings and shields, his
  "do handlarki" bands as a merchant mask that `IsPlayerBotMerchantOnlyGear`
  keeps off a counter), upgrade materials, books, Forgetting Scrolls, soul
  stones and their socket multipliers, marbles, herbs, guild materials, ores,
  and the bonus multipliers with the two weapon damage tiers. The three tables
  that were typed by hand into types.h and bonus.h are gone. Every price goes
  through `ScalePlayerBotIwakuraPrice`, his curve (100% x1.0, 200% x2.2, 500%
  x5.0 ... 10000% x100, straight between the points and proportional outside
  them); the books' own x1.1 line and the materials' bare rate went with the
  hand tables. The generator refuses a name it cannot bind to a vnum, a bonus
  without an APPLY, and a bonus that world.item_attr says does not roll on the
  slot he wrote it under - and that last check found three rows the hand table
  had wrong: "Szansa na kradziez PE" is MANA_BURN_PCT (it was STEAL_SP),
  "Punkty doswiadczenia +%" is MALL_EXPBONUS (EXP_DOUBLE_BONUS never rolls on
  boots or a necklace), and "Szansa na dobicie ciosu" on body armour is
  REFLECT_MELEE, because a critical line does not roll on armour. A name the
  game gives two vnums prices both (Nieznany Talizman+, Zabie Udka, Nieznane
  Lekarstwo, Ozdobna Spinka), except the two overrides the generator names.
  "Max roll" is `g_map_itemAttr[apply].lValues[bMaxLevelBySet[set] - 1]`
  (constants.h extern, both engines), the bonus product is capped at x100, and
  on mt2009 APPLY_* are the POINT_* aliases in playerbot_engine_compat.h: the
  five the sheet needed are there, three of them mt2009-only and under an #if
  in the table. A counter's price still moves through `LimitPlayerBotAskStep`,
  so a changed table reaches the stalls over hours, and
  `PLAYERBOT_PRICE_TABLE_VERSION` (3) is what makes every offline shop reprice.
- **"±" for "ą" is the client, not the data.** Checked bytes in
  `world.mob_proto`/`item_proto`: "Handlarz Bronią" ends B9, "Różności" is
  F3 BF ... 9C - CP1250, correct - and there is no mob_names.txt on this
  line to mirror it from (names come from world.sql). A client that shows
  0xB9 as "±" is rendering with another code page (Windows locale for
  non-Unicode programs, or a font/locale pack in its own packs); ask for
  the Windows locale and a screenshot of the same NPC before touching the
  server. NerrVoVy, 12 September.
- **The 2.0.11/2.0.12 crash signature is one binary and one bug.**
  Kiciamol's bundle (2.0.12, 1500 bots) has game1 dying every ~51 s with
  frames byte-identical to sizowski's (+0x41c413 +0x41cc9a +0x436b17
  +0x43844e under Update+0x51e2 into libc memmove) - the Docker build is
  deterministic, so addr2line on a rebuilt 2.0.12 binary would name them.
  That needs the staged engine tree of that version (gitignored), not a
  worktree alone. Not done: nothing on 2.0.13+ has crashed in 5.5 h.
- **A purchase asks for room the size of the item.** `BuyPlayerBotTackleItem`
  tested `GetEmptyInventory(1)` before buying a rod of three cells; a bag
  with single holes and no free column passed, paid, and `AutoGiveItem` put
  the rod on the ground - then paid again next tick (eight in eight
  seconds, sizowski) and looted them later. Use `proto->bSize`, and after
  `AutoGiveItem` check `item->GetWindow() == INVENTORY` - the pointer it
  returns is as valid for a dropped item as for a bagged one.
- **Four lines by the stone, the fifth by the marble, at the engine's
  odds.** `USE_ADD_ATTRIBUTE` (71085) adds only below four and
  `USE_ADD_ATTRIBUTE2` (Marmur Blogoslawienstwa, subtype 22) only at exactly
  four, each rolling `aiItemAttributeAddPercent[count]` (100/80/60/50/30,
  `extern` in constants.h) and spending the item either way. The bots called
  `item->AddAttribute()` straight, no odds, to five.
  `PLAYERBOT_BONUS_MAX_LINES` is four; `FindPlayerBotBlessingMarbleCell`
  is the only way to a fifth.
- **A compiled quest state index is a signed hash, so "unknown" cannot be a
  sign.** `quest/object/state/collect_quest_lv30` has key_item at
  -1726153001 and __reward negative too; every `>= 0` test on a state index
  was silently false for those. `GetPlayerBotBiologistStateIndex` returns
  `PLAYERBOT_QUEST_STATE_UNKNOWN` (INT_MIN) for a name the quest lacks - the
  engine answers 0 there, which is also "start" - and every caller tests
  that. This is what kept the Orc Tooth in go_to_disciple with
  collect_count at ten and the bot handing in twenty-two teeth.

- **A level band is not a map, and answering with one names Chunjo's.**
  `GetPlayerBotMonkeyMapForLevel` returned map 25 for "the easy dungeon",
  which is true only for Chunjo: every kingdom has its own
  (`metin2_map_monkey_dungeon_11/_12/_13` = 5/25/45, same geometry, bases
  76800 apart, all with Town.txt cell 72,125). A Shinsoo bot walked in
  through its own gate - `GetKingdomGates` had the leg right all along -
  and stood on a map `IsPlayerBotMonkeyMap` said was not a dungeon: no
  chambers, no hubs, no medal; past 33 it was sent at 108, which its core
  does not host, and the warp was refused every time. Measured before the
  fix: Shinsoo 500 characters and 0 horses, Jinno 500 and 0, Chunjo the
  only kingdom levelling one. The band is `GetPlayerBotMonkeyBandForLevel`
  (pure, in types.h) and the map is `GetPlayerBotMonkeyMapFor(ch)` in
  travel.h, because turning one into the other needs the bot's kingdom and
  `IsPlayerBotMapHostedHere`. A kingdom with no harder dungeon on its core
  keeps its own rooms until `PLAYERBOT_MONKEY_EASY_FALLBACK_MAX_LEVEL`.
- **A per-kingdom table nobody asks is a table that does not exist.**
  `playerbot_empire_rules::GetTeleportArrival` has held the three entry
  points of Orc Valley, the desert and Sohan - one per kingdom, read from
  each map's Town.txt - since the three-kingdom travel was written, and it
  is correct to the unit. Only the M3 branch ever called it: the frontier
  branches asked `GetPlayerBotFrontierArrival(map)`, which is one point per
  map and that point is Chunjo's. So every bot of every kingdom arrived
  through Chunjo's entrance and walked back to Chunjo's gate ("wszystkie
  boty ... wchodza w miejscu wejscia zoltych", SIZOWSKI). Before adding a
  per-kingdom table, grep for who will call it; after adding one, grep for
  who still does not. `GetFrontierGate` is the other half (the warp NPC
  beside each entrance, from npc.txt: 10007/10009/10011 on the valley and
  Sohan, 10008/10010/10012 on the desert).
- **A claim stamp is not an answer.** `web_admin.quest` takes a queue row by
  writing a token into its `status` column ("w" + channel + "x" + salt +
  "t" + tick), does the work, and only then writes the result;
  `queue_and_wait` returned on the first status that was not `pending` and
  handed that token to the operator as a failure - "Coś poszło nie tak
  (w1x257t780)". SPEED hit it every time because its handler is the slowest
  in the quest (32 `affect.remove_collect` calls before it writes
  anything), and the others only sometimes. The panel waits for a word from
  `QUEUE_FINAL_STATUSES`, which is the quest's own whitelist plus
  `player_offline` and `cancelled` - add a word to one and it belongs in
  the other.
- **Code that cannot be reached is code that was deleted, and the binary
  says so.** On mt2009 `ManagePlayerBotShopLifetime` begins with an offline
  migration that returns and then `if (!ch || !ch->GetMyShop()) return`,
  and the two are exhaustive - so lines 1992-2105 are unreachable and GCC
  drops them from `-O1` up. Three features went with them and nothing said
  so: the fast-sale memory ("wysoki popyt"), the `PLAYERBOT_STALL_SOLD`
  history row, and re-judging a standing stall after the TRADE slider
  moved. Found by AkhiGubernator with `strings` on the shipped binary -
  three literals from `playerbot_town.h` were missing while their
  neighbours were present, and the one `PLAYERBOT_SHOP: refused` of three
  that was gone settled it. On this line the goods on a counter belong to
  the shop entity, so a sale is noticed in `ikarus_shop_manager.cpp`
  (`playerbot_offline::NoteSold`, an exact-string edit) and drained on the
  owner's tick. When a feature is reported dead, check the binary for its
  literals before checking its logic.

- **A decided duel is still a pair, and the winner's client is locked out
  of it.** `CPVP::Win` takes the loser's agreement back and sends
  `PVP_MODE_REVENGE`: the loser may restart the fight, the winner's client
  will not attack the loser, and the pair lives until `CPVPManager::Process`
  drops it after ten minutes without a fight. A bot never takes a revenge, so
  a player who beat one could neither hit it nor challenge it again for ten
  minutes - and before 2.0.41, when the bot's swing was a bare
  `CHARACTER::Damage`, the respawned loser went on hitting a winner who could
  not hit back ("wali jakas zemste, gdzie nie moge mu oddac", Drip).
  `EndPlayerBotDuel` (playerbot_combat.h) is the only way a bot's duel ends:
  on its own death (`reason=lost`, from `HandleDeath`), on the foe's
  (`foe_fell`), and after the refusal clock (`engine_refuses`, `safe_zone`).
  It deletes the pair with `Packet(true)` and `CPVPManager::Delete` - both
  public on both engines; `GiveUp` is not safe, it erases the companion's
  whole set - and ends the other side's memory of the duel when that side is
  a bot. `pair_removed=` in the `duel over` line says whether there was one.
- **`SCROLL_FROM` is a floor, not a start.** The weights key (1-9, default 1
  = no floor; a slider in the classic panel, and the advanced panel keeps the
  line it does not know) is the lowest plus a refine under a Blessing or
  Dragon God scroll may land on. `IsPlayerBotScrollStepAllowed` is asked by
  the blacksmith pass - for the scroll and for the prize hold, which under the
  floor gives way to the plain anvil instead of holding a piece for good -
  by the field scroll pass, and by `GetPlayerBotRefineTarget`, which keeps the
  +9 ladder only for a piece standing on the first rung or whose own ambition
  carries it there. Above the floor the old rules decide where a scroll is
  worth it: from +7, earlier for a worn piece at a step that can burn and for
  a prize piece.

- **A bot's party is its own kingdom's, the way a player's is.** The party
  finder in `ManagePlayerBotParty` joins with `CParty::Join` directly, and that
  never asks `CHARACTER::IsPartyJoinableCondition`, whose first rule is
  `PERR_DIFFEMPIRE`. On the shared maps - all of them, and the villages too,
  under `unified` - the bots of the three kingdoms stand side by side, and they
  grouped as if they were one; a player saw "boty z roznych krolestw expia w
  jednym PT" and could not invite one of them himself (l0st3k). The finder asks
  the candidate's empire and the party leader's; `joined party ...
  leader_empire=` and `created party ... partner_empire=` put both in the log.
- **The mt2009 safebox does not stack, so the bot pours.**
  `ENABLE_MT2009_DISABLE_SAFEBOX_STACK` (CommonDefines.h) takes stacking out of
  `CSafebox::MoveItem`; a player merges by taking a stack out, dropping it on
  the other in the bag and putting it back. The deposit took the first empty
  slot for every stack, so the boxes filled with split stacks - 357 groups and
  683 wasted slots in 344 boxes on the test world ("boty nie lacza przedmiotow
  w magazynie", jaksiezabic). `TopUpPlayerBotSafeboxStacks` pours a bag stack
  into the box's own stacks before it takes a slot, and
  `MergePlayerBotSafeboxStacks` pours split ones together,
  `PLAYERBOT_SAFEBOX_STACK_MERGES_PER_VISIT` a visit: counts move with
  `SetCount`, the destination is flushed (`FlushDelayedSave`) before the
  source goes, and an emptied item is removed and destroyed exactly as
  `CSafebox::MoveItem` does it. `topped_up=` and `stacked=` are in the
  `safebox deposit` line.
- **A bot past the age of pennies leaves them on the ground.** Every drop in
  reach was loot, so a bot of sixty with millions ran for a small potion like a
  bot of ten ("boty rzucaja sie jak zombie po przedmioty", sizowski).
  `IsPlayerBotChoosyLooter` (level `PLAYERBOT_LOOT_CHOOSY_MIN_LEVEL`, yang
  `PLAYERBOT_LOOT_CHOOSY_MIN_GOLD`) and `IsPlayerBotLootBeneathBot` - potions,
  gear outgrown by `PLAYERBOT_LOOT_OUTGROWN_GEAR_LEVELS` under
  `PLAYERBOT_PRECIOUS_REFINE` with no prize lines, and the merchant's herbs,
  worth under `PLAYERBOT_LOOT_CHOOSY_MAX_VALUE` at the merchant; no price at
  all counts as unknown, not cheap - sit in `CCollectPlayerBotLoot`, so the
  walk and the combat Z pass skip the same drops. The operator set the numbers
  (Tieru, 14 September: "od 40k wartosci u handlarza"). This is a value filter,
  not the ownership rule: an item nobody owns is still everybody's.
  `PLAYERBOT_LOOT: left merchant fodder` is throttled population-wide.
- **A duel from a transport saddle is refused whole.** `CPVPManager::CanAttack`
  refuses every blow from a horse under grade two, and the tick's own dismount
  waits for a target, which a refused duel never sets - so a bot that agreed in
  the saddle sat there until `PLAYERBOT_PVP_REFUSED_GIVE_UP` ended the duel
  without a blow. `ManagePlayerBotDuelCombat` climbs down first when
  `CanPlayerBotEverFightOnHorse` says no (`PLAYERBOT_HORSE: dismounted ...
  reason=duel`). Before 2.0.41 the blows landed from the saddle anyway, which
  is what "nawalal hitami z konia, a ma zwyklego konia" (Drip) was.
- **"Teleportuj mnie" moves whichever human is in the game.** The live map's
  bot popup puts one green button over everything else, and
  `api_admin_warp_me` with `player_name: 'auto'` queues a `WARP` for up to eight
  human characters of the last week and lets the online one take it. A queued
  teleport cannot fire late: the button deletes its own pending rows after six
  seconds and the quest's sweep retires any pending row at thirty
  (`player_offline`) - so "I stood AFK and was suddenly in the Demon Tower"
  (sizowski, 2.0.40, a bot of his in the Tower) means a click seconds before,
  from the panel or from F9. The panel now logs `teleport me` for every such
  request, and F9's own warps log `GMPANEL:`, which the support bundle collects.
- **The support bundle's tag list was twenty-six tags behind the code.**
  `PLAYERBOT_PVP`, `PLAYERBOT_LOOT` and `GMPANEL` were added with the fixes that
  needed them. The rest were left out on purpose, because the bundle keeps the
  last 40 000 matching lines and some tags would crowd out everything else -
  `PLAYERBOT_PARTY: assist` alone writes about a hundred lines a minute at a
  thousand bots. Adding a tag is a decision about that budget, not a formality.
  The one party line a player's report needs, `PLAYERBOT_PARTY: accepted an
  invitation`, goes in as the pattern `PLAYERBOT_PARTY:.accepted` - a dot for
  the space, because no quote may reach docker from PowerShell.
- **Eliksir Slonca and Eliksir Ksiezyca are auto potions, and a use is a
  switch.** 72723-72726, 76021, 76022 and 79012 (HP) and 72727-72730, 76004,
  76005 and 79013 (SP) are `ITEM_AUTO_*_RECOVERY_*` on both engines. The
  USE_SPECIAL case in `char_item.cpp` adds `AFFECT_AUTO_HP_RECOVERY` or
  `_SP_` (534/535, `dwFlag` the item id) when none is running, takes it off
  when this item's is, and for an empty one (socket 1 equal to socket 2)
  says `AUTOPOTION_IS_EMPTY` and `break`s - so `UseItem` returns true for a
  use that did nothing. `PLAYERBOT_EXP_ELIXIR_VNUMS` filed the SP half under
  experience and `ManagePlayerBotExpElixir` used every one in the bag on
  every booster pass; 991 of the 992 in bot bags on the test world were
  empty, so the pass went on for ever: some 580 000 `exp elixir` lines an
  hour on game1, a hundred and sixty a second, and not a point of experience
  - while the one with anything left in it was switched on and off by turns.
  `ManagePlayerBotAutoPotions` switches a potion on only when its affect is
  absent and the potion is not empty, once a minute per bot, and logs only
  when the affect appears; the junk rule keeps one with something left and
  lets an empty one go. 39037-39042 share the names and no use path in
  either engine handles them. Before writing "drink it" for a vnum, read the
  case in `char_item.cpp` that handles it.
- **A Demon Tower stone is a warp for every character on its killer's map.**
  `deviltower_zone` answers a kill of 8015 (Metin Twardosci, level 50,
  167 850 hp, the only one of 8015-8019 in `metin2_map_deviltower1/regen.txt`)
  on map 66 with a six-second player timer and `d.new_jump_all(66, ...)`: a
  new instance, and `CDungeon::JumpAll` `WarpSet`s every PC on the map the
  killer stands on when the timer fires, with no bot test anywhere on the
  way. On the test world a warrior of fifty-seven took it as an ordinary
  Metin (`IsPlayerBotMetinWorthFighting` admits ten levels under to nine
  over), and on 14 September at 14:17:11 ten characters on map 66 went into
  instance 660000 and straight back out through the sectree rescue. A killer
  that changed map inside the six seconds takes that map instead, and that is
  "stalem afk pod lochem malp w m2, gdy nagle przeteleportowalo mnie do DT"
  (sizowski, the same day): his bundle has him leaving the game at 12:27:18
  and six bots on map 23 rescued for want of a sectree within the minute
  after (`sectree_rescue from=23 to=23`) - JumpAll run on Bokjung. Its bot
  syslog began five seconds too late to name the killer.
  `IsPlayerBotDungeonTriggerStone` keeps 8015-8019 out of the Metin registry,
  out of `IsPlayerBotMetinWorthFighting` (the collector, the party focus and
  the crossing's stone finder all ask it) and out of the melee sweep, which
  hits any stone within `PLAYERBOT_MELEE_SPLASH_RANGE` of its target. The
  engine's skill splash is still open (`FuncSplashDamage` asks only
  `battle_is_attackable`), so a bot fighting beside the stone can still land
  the last blow; at that health it is slow, not impossible. Any stone whose
  kill runs `d.new_jump_all` or `d.jump_all` belongs on that list.
- **A player's party is the player's, and three passes had to be told.**
  2.0.38 taught the party pass (`ManagePlayerBotParty`) that a party whose
  leader has no bot descriptor (`IsPlayerBotHumanLedParty`) is outside the
  cohort, rotation and straggler rules, and gave it
  `ManagePlayerBotFollowHumanLeader`. Two other paths went on quitting any
  party: the inactivity watchdog's reset, written for bot parties stuck in
  PARTY_ASSEMBLE, and `TransitionPlayerBotMap`, because a bot party is one
  camp and ends with the map. A bot standing beside an idle player - which
  is exactly where the follow pass leaves it, inside
  `PLAYERBOT_PARTY_FOLLOW_DISTANCE` - tripped the watchdog after ninety
  seconds and was out ("dodaje boty do PT, a po chwili z niego wychodza",
  sizowski, 14 September), and the sectree rescue after a Demon Tower warp,
  a same-map transition, took every bot out of the party he had just made.
  And the test itself asked for the leader's character, which a player has
  not got for the seconds of a warp - a logout and a login - so the party
  pass took his party for a bot party then and put the cohort rule to it:
  the engine's `PARTY P2PSetMemberLevel` lines (the bundle's login files)
  have bot 970 in his party at 13:42:40 and gone at 13:42:56, two seconds
  after his character logged in again. A leader with no character on this
  core is judged by pid now (`CPlayerBotManager::IsRegisteredBotPID`).
  `IsPlayerBotBesideHumanLeader` is legitimate stillness to the watchdog
  now, and neither the reset nor a map change quits a player's party. Grep
  every `->Quit(` before writing a rule about who stays in a party. A player
  who warps is followed: `ManagePlayerBotFollowHumanLeader` makes the move a
  bot cannot make with a client - `TransitionPlayerBotMap` onto the leader's
  spot, once the leader stands on a map this core hosts - and refuses a
  dungeon instance (an index from `PLAYERBOT_INSTANCE_MAP_INDEX_MIN`) and a
  spider map whose desert crossing is already under way, which the
  transition would otherwise restart from the desert's doorstep on every
  retry. And a Shaman buffs the player before itself
  (`ManagePlayerBotBuffHumanLeader`): `CHARACTER::UseSkill` hands a buff that
  is not SELFONLY to `ComputeSkill` on its victim and the affect carries the
  skill's own vnum, so `IsPlayerBotBuffAffectOn` - the affect half of
  `IsPlayerBotBuffActive` - reads a player as well as a bot. Neither has been
  watched with a person in the party yet: the test world has none.
- **A drop is loot only if the engine's pickup would take it.**
  `CCollectPlayerBotLoot` skipped a drop only when the bag had no free cell
  at all, and a bag with single holes and no free column cannot take a sword
  or a breastplate: `PickupItem` wrote "No empty inventory pid ... size 2"
  7736 times in two hours from 320 bots on the test world, the pass marked
  the drop failed for five seconds, came back to it, and the bot stood over
  it until the inactivity watchdog moved it - three of the four "arrived and
  stood" resets after one restart were `action=3`. `PlayerBotBagTakesDrop`
  asks the engine's own question after the stack merge and yang:
  `GetEmptyInventoryEx(item)` on mt2009, the item's size on r40250.
- **A mark nobody reads is no mark.** The attack pass gives up on a monster
  it cannot reach after three refused plans and puts the VID in
  `mapFailedTargets` for thirty seconds; `FindPlayerBotEngagedTarget` - what
  the tick, the pull and the Monkey Dungeon's spread ask for "what is
  fighting me" - never looked at that map and handed the same monster
  straight back. Pokonany stood under a ranged monkey on a ledge for thirteen
  minutes: 373 unreachable plans to one point seven hundred units away and
  eight watchdog resets. The finder takes the state now and every caller
  passes it.
- **A splash lands on stones too.** `FuncSplashDamage` asks
  `battle_is_attackable` and nothing else, so the last open road to a bot
  breaking a Demon Tower stone was an area skill cast at a monster beside
  it. `IsPlayerBotSplashNearTriggerStone` looks round the caster and the
  target on map 66 (the skill's `iSplashRange` plus
  `PLAYERBOT_SPLASH_STONE_MARGIN`) and the rotation skips the splash skill
  there; the scan is paid on that map alone.
- **The containers' clock is the operator's, set once.** Every service in
  compose takes `TZ` from `M2_TZ`, and `.env.example` said UTC, so a Polish
  panel showed times two hours behind its own machine and the logs were
  named by UTC hours ("czas jest cofniety o dwie godziny", hunmar, 14
  September). All four images carry zoneinfo. `Assert-TimezoneDefault` in
  `start-server.ps1` turns the example's UTC into the Windows zone exactly
  once (`Get-M2HostTimeZoneName`: a table of Windows ids, else a fixed
  `Etc/GMT-N`) and sets `M2_TZ_DEFAULTED`; `migrate_timezone` in
  `linux-port-mt2009/tools/update.sh` does the same from `timedatectl`,
  `/etc/timezone` or the `/etc/localtime` link when run on a host, and
  nothing inside the updater container, which cannot see the host's zone.
  Once a world has migrated, its syslog, syserr, crash stamps and support
  bundles are in local time: the UTC times in older notes here are UTC.
- **A walk to another map's coordinates is refused before the clamp.**
  `MovePlayerBot` hands its goal to `CPlayerBotNavigation::ClampWorld`, which
  pulls any point onto the map's last cell; in Bokjung that is
  (204750,307150), and bots on the town square planned it 1615 times in a
  day, each a far plan answered "unreachable". The obvious sources all check
  the map - the frontier hub tables, the 222 village ground points, the
  known-Metin registry, the walk back after a death - so the guard refuses a
  point more than `PLAYERBOT_NAV_OFF_MAP_MARGIN` outside the map and writes
  `PLAYERBOT_NAV: destination off the map` with the point as asked, the
  bot's errands and `caller=` (the return address, one line a minute per
  caller) to syserr. The shipped game binary is a stripped 32-bit PIE, and
  the address still names the call: subtract the core's load base (the
  first line of `/proc/<pid>/maps` for the `game` whose cwd is that core,
  read as the metin2 user) and disassemble the image's own binary there
  with `objdump -d --start-address`; the constants pushed round the call
  give it away. The first one found was the Bestial Captain's detour in the
  M2 wander branch (`0x43415054`, "CAPT", two instructions above it):
  `IsPlayerBotBossAlive` kept its answer by race alone, the Captain (591)
  stands in all three second villages, and a bot in Bokjung was sent to
  Jayang's Captain for the thirty seconds the answer was trusted - close to
  four thousand refused walks a minute while he stood. The answer is kept
  by map and race now; the raid roster and the guild call are still by
  race, which holds while every boss hub is the boss of one map.
- **`ManagePlayerBotCombatBuffs` has had a party branch all along**, and it
  runs only when the bot's own cast of that buff has just failed - which is
  why no player was ever buffed by it and why the Shaman's pass for the
  player (`ManagePlayerBotBuffHumanLeader`, above) is a pass of its own.
- **A stock quest is fixed by compiling its pre_qc form, not its source.**
  The package's quests carry `define` and `define group`, which its own
  `precompile.py` expands into `quest/pre_qc/` before the FreeBSD qc runs; the
  qc built in the image's quests stage does not read `define` and aborts on
  line 1. `linux-port-mt2009/docker/game/quest/new_quest_lv19.quest` is the
  pre_qc file with the bear group completed - the stock group named 140 and
  141, so the Cursed Bear (139) and the Cursed Brown Bear (142) never counted
  towards the five skins (Pabloo, from Dixdros' report) - compiled in the loop
  beside pony_levelup, and its object files land over the stock ones.
  Compared whitespace-blind with the stock objects, 27 of its 28 files are the
  same and the kill handler is the one that differs, so a character already in
  the quest keeps its state. The stock quests are gitignored serverfiles: the
  fixed copy in the quest directory is the only part that ships.
- **A release of Seban's panel carries none of our commits to it.** 1.41.0
  put back the hunting ranking that 5d853df took out and the log.log hint
  decoding that 2273c31 fixed, so a new zip is checked against our own commits
  to `linux-port/docker/seban-panel` (how many of each commit's added lines it
  still holds) before it lands. It also wires three controls to his own
  game-side scripts - m2-botcount, m2-map-regens, and a `common.m2_switches`
  row his starter-chest quest reads - and none of them is in this image: the
  manage page queried the missing table and answered 500, and the bot count and
  the respawns would have written requests nothing reads.
  `M2_PANEL_CUSTOM_PATCHES=1` (our `SEBAN_GAME_INTEGRATION` until his 1.48.0
  brought a flag of his own) in the panel's environment turns the three on for
  an install that has his scripts; without it they are hidden and refused. Two
  more things came in that zip: bounds for maps 66, 67 and 68 that are not this
  world's (taken from each map's Setting.txt instead), and a collector that
  asked `SHOW COLUMNS` of a table new in that release - an error, not an empty
  answer, on every install that never had the table, so the table was never
  made and /economy/shops answered 500 until the check asked
  information_schema.
- **A party member's pickup put the owner's item in the wrong cell under the
  wrong name.** `CHARACTER::PickupItem` on mt2009 has a branch for an item
  another member of the party owns. It put the item into the owner's bag at
  an empty cell - the owner's own pickup calls `AutoStackItem` first - so
  every potion a bot picked up for its player took a slot of its own, and it
  told the owner that the picker "receives" it, because `GetName()` there is
  the picker's. With bots in a player's party the picker is nearly always a
  bot, which is how a branch few people ever reached became a report
  (mkls6649; the analysis and the fix are Kenny's). `apply_party_pickup_to_owner`
  in playerbotify.py stacks into the owner's bag first, sends only what a full
  stack cannot take on to the empty cell, and names the owner in both
  messages; char_item.cpp ships staged because of it. The branch logs nothing
  and the test world has no player to stand in a bot's party, so this was
  compiled and read, not watched.
- **The package's `log.hack_log` could not take a single line.** Its dump
  defines time, name, server and why; `LogManager::HackLog` inserts login and
  ip as well, so every hack line failed with "Unknown column 'login' in
  'INSERT INTO'" - 262 in one day on the test world, each a bot's
  FAST_ITEM_SWAP - and the table held nothing on any 2.x world. It is the
  loginlog2 hwid shape again: `CREATE TABLE IF NOT EXISTS` never mends a table
  that exists, so logschemify.py appends `ADD COLUMN IF NOT EXISTS` for login
  and ip (where r40250's table has them) and widens name to
  CHARACTER_NAME_MAX_LEN; the migrator applies it on every start and initdb on
  a fresh world. Checked by running the migrator twice over the test world:
  the columns are there once and the second run changes nothing. After the migrator the error stopped - 16 lines in the half hour before, none in the twelve minutes after - and the table took its first six lines, every one of them FAST_ITEM_SWAP from the same bot two minutes apart, which is what the table is now there to show.
- **Gear under level thirty goes on a counter at +6 or not at all, and two
  lines of it at most.** `PLAYERBOT_SHOP_MIN_GEAR_LEVEL` had said since 1.x
  that such a piece was junk at any refine, and never ran: the precious-refine
  branch of `ScorePlayerBotShopStock` returned first for everything at +4, so
  the level test only ever saw +0 to +3. On 14 September the counters of the
  test world held 4 802 lines of it, 2 409 at +4 and +5 - Czer. Ubranie
  Mrowki+5 on 348 of them - and a player asked the Discord whether every
  server had "takie janusze biznesu". The operator's rule is
  `PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE` and `PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES`,
  asked before the bonus and the precious refine; the cap lives in
  `CollectPlayerBotShopItems` and counts what an offline shop already holds.
  Below +6 such a piece is the merchant's (`IsPlayerBotJunkItem`) - for this
  gear only, the eleventh of September's "nothing above +4 to the merchant"
  stands for the rest - or it would ride in the bag for good, because the
  unsold-stands rule only counts what went up. The shops already standing are
  cleaned by the service visit, one line a visit: `RecvShopRemoveItemClientPacket`
  through the journal's `Remove`, which needs edit mode on a running stand and
  none on an expired one, and which a bag with no cell refuses synchronously. Twenty-five minutes after it went live on the test world: 786 lines taken off the standing shops, gear under thirty at +4 and +5 down from 2 409 to 1 844 on the counters and from 487 to 253 in the bags, 67 names chosen - 31 from a list, 26 neutral rolls, 9 material names, 7 top-gear headlines - and no core died.
- **A shop's name is Iwakura's list, rendered, and the engine decides what can
  be on it.** `data/iwakura_nazwy_sklepow.txt` is his list of 14 September -
  seven categories and the rules at its head - rendered by
  `tools/generate_shop_names.py` into `playerbot_shop_names.h`;
  `playerbot_shop_name_rules.h` is those rules as pure code
  (`tests/playerbot_shop_name_rules_test.cpp`), and `playerbot_shop_signs.h`
  describes a real counter in their terms. A name that mentions goods ("ZEBY
  ORKA TANIO") has a binding in the generator's POWIAZANIA and is drawn only
  over a counter holding them; the generator refuses a binding to an item
  item_proto does not know or to a name no longer on the list, and reads the
  33%, the x1.5 and the x1.4 out of his wording. `ikashop::CShopManager::ParseShopName`
  runs a name through `EscapeString`, cuts the escaped string to 32
  (`SHOP_SIGN_MAX_LEN`, `OFFLINE_SHOP_NAME_MAX_LEN`), and refuses it on
  `has_proper_characters` (printable ASCII and the eighteen Polish CP1250
  letters, `common/utils.h`) and on any `world.banword` as a substring after
  ASCII lower-casing - so "Nauka czytania dla opornych" is refused for "porn".
  The shop keeps the escaped string, so a backslash shows doubled unless the
  cut lands on it. The generator trims decoration off both ends when that is
  enough, drops the rest and lists both in the header. A standing shop cannot
  be renamed by a bot - `RecvShopChangeNameClientPacket` wants `PREMIUM_SHOP` -
  so it is renamed when it is renewed (`RecvShopReopenClientPacket` takes a
  name). His names keep their diacritics, CP1250 like an item name: the
  ASCII-only convention is for our own strings, and
  `ikashop_offlineshop.name` is `cp1250_polish_ci`.
- **A merchant's helmets are every class's at once.** The ladder
  (`GetPlayerBotProgressionHelmetVnum`) was class-based, and the purchase that
  fills a slot from 9001-9003 (`FindPlayerBotBestMerchantSlotVnum`) took the
  highest-level helmet in stock whoever it was for: a sura of 74 was sold the
  warrior's Tradycyjny Helm, could not wear it, and went bareheaded
  (NaCoPaczysz, 14 September). On the test world 198 of 669 bots of fifty and
  up wore no helmet, 120 of them with another class's in the bag.
  `IsPlayerBotProtoForCharacter` is the anti-flag half of `CanUsedBy`, for a
  proto the bot does not hold yet. In the seventeen minutes after the change the test world bought 173 helmets, each of the buyer's class - 147 bots once, 13 twice, none more - against a shaman that had bought and sold back the warrior's helmet 99 times in the sixteen minutes before it; helmetless bots of fifty and up went from 198 to 164, and those carrying another class's helmet from 120 to 48.
- **Outgrown armour compounds towards nothing and never reaches it.** The
  penalty was five percent of the defence figure per level past
  `PLAYERBOT_ARMOR_OUTGROWN_LEVELS`, capped at all of it, so every armour
  twenty levels outgrown scored the same single point and the bonus lines
  alone decided: the same sura wore a level-1 plate +6 with a level-34 one +4
  in its bag, then put the level-34 one on its counter. Each level past the
  threshold now keeps ninety-five percent of the level before, so a higher
  tier keeps more of its defence at any level.
- **A bot in a player's party makes no plan of its own that changes map.**
  `ManagePlayerBotFollowHumanLeader` leaves the bot to the rest of the tick once
  it stands near the player, and the rest of the tick sent it away: in
  sizowski's bundle of 14 September three shamans went `m1_direct_to_hwang`,
  `m1_direct_to_sohan` and `desert_crossing_to_v1` through the Teleporter and
  came back by `follow_leader` a second later, six round trips in two minutes,
  and a buff landed once. `ManagePlayerBotWorldTravel` (both call sites), the
  offline shop's service visit (`BotOfflineBusy`) and the Joan-first market
  trip stand down for `IsPlayerBotHumanLedParty`; a desert crossing under way
  is dropped, not resumed from wherever the player has led. Compiled and
  deployed; the test world has no player to stand in a party.
- **The package's Teleport Ring had no quest.** 70058 drops from monsters and
  chests (mob_drop_item.txt, special_item_group.txt) and nothing in the
  compiled quests answered its use, so the ring did nothing.
  `linux-port-mt2009/docker/game/quest/teleport_ring.quest` is
  map_warp.quest's list, level floor and fee behind `when 70058.use`, compiled
  in the Dockerfile loop (object/70058/use in the image). qc checks only the
  dotted engine functions against quest_functions: the questlib helpers
  map_warp uses (say_split, select_table, parse_number, get_player_map1_index)
  compile without being listed. It never ran under that name: the ring's use
  goes to its special item group, and the handler is `when 10031.sig_use`
  since 2.0.95 (see "A quest item in a special item group is used under the
  group's number").
- **A GM on this line is its owner playing.** `apply_gm_gameplay` in
  playerbotify.py, from an audit of 14 September (gm_gameplayify.py): Ikarus's
  `CheckGMLevel` refused every shop operation above GM_PLAYER,
  `IsLevelViewable` hid a GM's level, `SetLevel` and the login block both forced
  PK_MODE_PROTECT, and `CanOpenShop` asked a GM for the kill count. The badge
  (AFF_YMIR) and every other check stay. Compiled and deployed; never tried in
  game, because the admin account's GM characters are not for testing.
- **The seban collector's first snapshot creates the tables the dashboard
  reads.** It slept its whole interval after a failure, and an update
  recreates it beside a database that is still starting, so the first attempt
  met "Connection refused" and the front page answered 500 for five minutes
  ("po 5 minutach zaczal dzialac", 14 September). A failure is retried after
  five seconds, doubling up to the interval, and the two pages that read
  `web_seban_shop_snapshot` treat a missing table as no data yet. Measured
  after a recreate: 200 on both pages within seconds.
- **playerbotify.py has to match the staged tree, comments and all.** The
  fishing edit's marker was a Polish comment and the staged char.cpp carries
  the English one, so the script found neither and stopped before every later
  edit - found only because the GM edits after it never ran. And mt2009's
  char.cpp is not pure CP949 (the stock `MonsterLog` lines are UTF-8 Korean),
  so the cp949 check from the r40250 note says nothing about it.
- **A refine line says what the recipe wanted.** `PLAYERBOT_AI: refine ...
  materials=vnum:need/have`, counted before the attempt takes them - "the bot
  refined to +8 without Orkowe Jadra" was read off a bag after the refine had
  consumed them. Both engine paths check and remove the materials
  (`DoRefine(false)`, `DoRefineWithScroll`); only a REFINE_BONUS_SCROLL with
  TUNING_FLAG_NO_ITEM (Gwarancja Rzemiosla, 25051-25054) skips them, and no
  bot uses one.
- **A pass the fishing just asked for is not the equipment pass's to trade.**
  `EnsurePlayerBotFishingPass` wears Karta Wedkarska (27620) in a unique slot
  and `ManagePlayerBotEquipment` scored Maska Sabaha (72735) above it for the
  same slot, so the two took turns every second or two: `equipped upgrade
  wear=8 old_vnum=27620` 83 to 1274 times an hour on the test world between
  14:00 and 20:00 on 14 September, and the engine's FAST_ITEM_SWAP check threw
  KimTyJestes out of the game nineteen times in thirty-six minutes - the first
  rows `log.hack_log` ever held. The fishing stamps
  `s_mapPlayerBotFishingPassAskedAt` and the equipment pass leaves a worn pass
  alone for `PLAYERBOT_FISHING_PASS_HOLD_MS`; none in the first twelve minutes
  after the deploy (eleven in the hour before it, so that alone is thin; the
  better proof is that KimTyJestes and Tryhard1337 were still wearing the pass
  with a Maska Sabaha in the bag, the exact shape of the loop). It is the
  pickaxe's lesson again: an activity that puts an item into a slot needs its
  own guard in the equipment pass. Unrelated and older: every restart is
  followed by a burst of `fishing pass bought` (58 in two minutes at 21:02, 41
  at 20:16) against one or two a minute otherwise.
- **Ask the anvil before taking the piece off.** `ManagePlayerBotRefining`
  unequipped a worn candidate and only then let `DoRefine` find the material
  or the fee missing (`refine SKIPPED ... materials=30057:2/21,27799:1/0`); the
  equipment pass put the piece back and the next blacksmith tick took it off
  again three seconds later, for the whole visit - about 3 000 `equipped
  upgrade wear=0 old_vnum=0` and 30 000 `refine SKIPPED` an hour on the test
  world. The worn-candidate loop and the unequip itself ask
  `CanPlayerBotAttemptRefineItem`, which `HasPlayerBotRefineOpportunity` already
  asked: "the planner and the pass that acts must ask one function", sprung
  from the other side. Twelve minutes after the deploy: 11 armour re-equips,
  each a different bot, and `refine SKIPPED` at a quarter of its old rate (the
  rest are bag pieces, which nothing unequips). The refine lines carry
  `materials=vnum:need/have` taken before the attempt, and the first hour of
  them is the answer to "refined to +8 without Orkowe Jadra" until a log says
  otherwise: 419 successful refines on recipes with materials, 87 of them +7
  to +8, every one with the materials in the bag.
- **Costumes are refused, not removed.** A costume once put on could not come
  off again and left the character drawn as a bare weapon; the operator's call
  was to stop them being worn. `apply_costume_block` (playerbotify.py) refuses
  `ITEM_COSTUME` at the top of `CanEquipNow` with a chat line, before the
  `ItemEquip` pulse is counted - EquipItem, a drag onto a costume slot and the
  item's use all pass there. A costume already worn stays worn and nothing is
  deleted; unequipping them at login would belong beside that edit. Compiled
  and found in the shipped binary, not tried in a client.
- **A playerbotify edit whose marker is its whole replacement fails the
  second run once a later edit writes inside it.** `apply_gm_gameplay` puts
  the GM's lines at the top of `CanOpenShop`, above the bot's lines from an
  older edit whose marker was its entire new text, so the next run on the
  staged tree found neither that text nor the stock anchor and stopped before
  `apply_costume_block`. Give an edit a `marker=` of one sentence no later edit
  will split.
- **A client package can carry the executable, under the name the launchers
  run.** Client 2.0.6 (ĹŌŞƬĒĶ's animated login screen and Discord Rich
  Presence) came as `pack/{root,locale}.{index,data}` and `Metin2
  SinglePlayer.exe`. Every launcher starts `clientExecutable` from its config
  or finds `Klient\metin2client.exe` (`Find-ClientExecutable`,
  `Get-M2SiblingClientExecutable`), so under the new name every player would
  have kept the old exe with the new packs; it ships as `metin2client.exe`,
  listed in `launcher/client-update-files.mt2009.txt`, and the same bytes had
  already run on the test machine as `metin2client2richpresence.exe`.
  `Test-M2ProtectedPath` guards only the launcher's own files, so the exe is
  applied like any other file. `linux-port-mt2009/client-root` and
  `client-locale` hold what changed against 2.0.5's packs (the login scripts,
  three login images, logo.tga and 76 files under `ui/animated`: 32 DDS
  frames the animation plays, the 32 JPG frames the first build played -
  frame 01 still opens the window - and 12 loading-logo PNGs), and
  `CLIENT_VERSION` had stayed at 2.0.3 through the 2.0.4 and 2.0.5 client
  releases; it only matters to a full package. The first build of this
  client loaded all 32 full-HD JPG frames synchronously as the login window
  opened, a delay anyone could see; the one that shipped loads DDS frames two
  a tick from `OnUpdate` (ĹŌŞƬĒĶ, the same evening, while the release was
  being packed). `intrologo.py` plays `loading.avi` instead of the two stock
  logo videos and goes straight to the login while that file is absent (the
  video is to come in a later client patch), and it appends a few lines to
  `login_preload.log` in the client folder on every start. The
  static-background switch asked of ĹŌŞƬĒĶ is not in this build.
- **A blow is modelled the way battle.cpp deals it, and mt2009 hides a share
  of it.** `GetPlayerBotWeaponHitDamageAt` (playerbot_gear.h) is
  `CalcAttackRating` against a monster of the bot's own level, `CalcMeleeDamage`
  (`(ATT_GRADE + roll*2 - level*2) * AR + level*2 + value5*2`), the defence off
  (about level + 15), then the average line, and for skills the same attack
  before the defence under the skill line, mixed by school
  (`PLAYERBOT_WEAPON_OWN_LINE_PERCENT` / `_OTHER_`). The old model had no
  defence - a percent line multiplies what is left after it, so a big line on a
  weak base reads better than it hits - and left skill damage out as "a PvP
  line", while `char_battle.cpp` multiplies every skill on a monster by it: a
  shaman's skill line scored nothing. And `CHARACTER::Damage` on mt2009 adds
  `levelLimit * 30 / 100 - 3` percent to a normal hit on an NPC for a weapon of
  level 32 to 65 (the level-65 elite families excepted; the engine's last range,
  7140..5149, is empty) and ten for 70 or 75: a Krwawy Miecz (45) hits ten
  percent harder than its numbers, a level-30 weapon gets nothing. No tooltip
  shows it; `GetPlayerBotWeaponLevelBonusPercent` does. Worth knowing before
  telling a player a level-30 weapon beats everything: for a body warrior of
  forty-five (ST 90, DX 3, the bots' own average) a Full Moon Sword +7 at 25%
  out-hits a Krwawy Miecz +6 (value5 45) by about eight percent, its hidden ten
  included - under the project margin, so such a bot buys one from about 33%.
- **A level-30 weapon is a project, judged at +7.** `ReadPlayerBotLevel30View`
  reads the bag each time: `toBeat` is the best blow the bot has (a level-30
  weapon in the hand at its own potential), the project is the bag's level-30
  weapon whose blow at `PLAYERBOT_LEVEL30_PROJECT_PLUS` beats that by
  `PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT`. The project and a worn level-30
  weapon are refined to +9 whatever the personality, the project is a refine bag
  candidate, and no counter or spare reason takes it; a counter's level-30
  weapon is bought only when its potential beats both (`IsPlayerBotBetterLevel30Offer`),
  and `PlayerBotCouldUseLevel30Weapon` gates the market walk with a hoped-for +7
  at 20%. Measured on the test world before this (15 September): 2315 level-30
  weapons on the counters, 2295 of them at +0..+3, 23 worn by bots. The buying
  was never the rule, it was the cap - 30% of the median wallet, several million
  under an asking price at mob_gold 3000 - so a level-30 weapon, the medal 50050
  and a safe scroll (`IsPlayerBotStrategicPurchase`) cost up to
  `PLAYERBOT_STRATEGIC_BUDGET_PERCENT` of the bot's own spare gold, in the
  offline and the classic path both.
- **From 37% average or 15% skill a weapon never meets the plain anvil.**
  `IsPlayerBotScrollOnlyWeapon`; it goes under a scroll at every step, past
  `SCROLL_FROM` (under the floor it could never be refined at all), and
  `CanPlayerBotAttemptRefineItem` refuses it without a scroll the step can use,
  so the planner sends no bot to a blacksmith for it. A level-30 weapon under
  the line is ground at the anvil with no prize hold and no +6 hold, under a
  scroll only at steps of `PLAYERBOT_WORN_SCROLL_MAX_PROB` and below (the
  family runs 90/85/75/65/55/45/35/25/20) - and under
  `PLAYERBOT_LEVEL30_SCROLL_LOW_AVERAGE` (30) not before the step to +5, however
  low those odds: "do +4 u kowala, zwoje od +5" (Tieru, 15 September), after
  CiosZKarpia spent ten of twelve scrolls in twenty minutes on the +3 and +4
  steps of an Ostrze z Czerwonej Stali of one percent. A kept scroll logs
  `level-30 weapon to the anvil, scroll kept for +5`.
- **An mt2009 refine scroll is a kind, not a vnum.** `world.item_proto`,
  USE_TUNING: 25040 and 25041 plain (value0 0), 25042 NO_REDUCTION_WHEN_FAIL,
  25043/70039 plain +15 (value1), 25045/71032 plain +10, 25044/71021
  UP_TO_3TH_LEVEL (refused from +4, certain below), 25051-25054 REFINE_BONUS.
  `DoRefineWithScroll`: success is prob + value1, a failure hands the piece back
  a level down unless NO_REDUCTION, and REFINE_BONUS destroys. The vnum lists
  were r40250's (39xxx, 76009), so on mt2009 only 25040 and 71032 ever counted
  and bags held nothing else. `FindPlayerBotRefineScrollCell` ranks by the
  values there (War God under +4, then the Magic Stone at steps of
  `PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB` and under, then plain scrolls by
  value1) and never takes a Gwarancja.
- **A Biologist specimen from level thirty up goes to him whatever the bot has
  outgrown.** Measured the same day: collect_quest_lv30 978 bots in
  go_to_disciple, 3 in key_item, none complete, while 358 bots carried 1484
  teeth - the carrying pass asked an outgrown row for the whole count, and past
  forty every row below was outgrown. It takes any held specimen of a row from
  `PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL` now; `GetPlayerBotBiologistReserve`
  (the rest of the count over the accept roll, ten teeth at 60% being
  seventeen) is what `CanPlayerBotAttemptRefineItem` leaves in the bag, and the
  blacksmith pass asks that of every candidate at the attempt - it asked only of
  worn pieces, and `DoRefine` takes a material whoever is owed it. A row in
  key_item makes its specimens surplus. The AI's hand-in never kept the quest's
  twenty-two hours. The Demon Souvenir row is live on this world: 1001-1004 have
  homes on map 66 and 29 bots had finished it, so the comment in
  `PLAYERBOT_BIOLOGIST_MISSIONS` saying 1001 has none is out of date.
- **The medal errand was rolled by almost nobody.** 17 of 999 bots in a Monkey
  Dungeon and none in the medium one, one medal handed in an hour, 415 of 1177
  bots of 35 and up on no horse and 8 past level ten. A bot short of its battle
  horse rolls twice as often (cap 70), and past `PLAYERBOT_MONKEY_MEDAL_MAX_LEVEL`
  (64) nobody farms - the hard dungeon's rolls are a few percent there - and the
  medal comes off a counter as a strategic purchase.
- **A price memory is yang, so it belongs to one yang rate.**
  `ForgetPlayerBotPricesOnRateChange` clears the ask anchors and the sale
  medians when `GetMobGoldAmountRate` moves (`PLAYERBOT_MARKET: yang rate` once
  a start, `changed from` after), and an offline shop's stamp is
  `GetPlayerBotPriceGeneration` - table version and rate - so a moved rate
  reprices every counter on the service walk. Iwakura's sheet was right; "one
  zero too many" was stands priced under another rate, stepping five percent per
  ten minutes towards the new one.
- **A duel is fought with what is in the hand.** `AcceptPlayerBotPvpChallenge`
  agreed three seconds after a challenge and `ManagePlayerBotDuelCombat` fights
  at the top of the tick, above the fishing session, so a bot on the bank with
  its rod out took a duel from another bot and fought it with the rod (Tieru,
  15 September: "chyba ze chca robic zawody na lowienie ryb"). The bot duel's
  challenger checked `bFishingSession` for itself and nobody checked the bot it
  picked or the bot that agreed. `GetPlayerBotDuelUnreadiness` (a fishing
  session or a rod, mining or a pickaxe, no weapon) is asked now by the
  acceptance, which tells a player why, by both challengers - the bot duel and
  the kingdom quarrel - for themselves and for the bot they pick, and by the
  duel pass, which ends a duel under way rather than fight it.
- **The operator's medal droppers are a cohort on top of the population.**
  `PLAYERBOT_MEDAL_DROPPERS` (a kingdom, 0 by default) and
  `PLAYERBOT_MEDAL_DROPPER_LEVEL` (25, clamped to 18-120) come through the game
  service's environment like `PLAYERBOT_AUTOSPAWN_COUNT` into the bootstrap in
  `input_db.cpp` (playerbotify). `SpawnMedalDropperCohort` schedules that many
  identities from the far end of each kingdom's registry - the set is in pid
  order and the ordinary spawn takes it from the front - passing over any saved
  more than two levels above the lock, before the ordinary cohort, which steps
  over them, and `TopUpMissingBots` restores them like the rest. The state init
  makes them medal droppers with no party or stone role; `ManagePlayerBotExpLock`
  locks them at the cohort's level rather than the personality's 33 and lifts
  `AFFECT_EXP_BLOCK` from a bot that should not carry it, since the affect is
  otherwise for good. The registry query reads `p.level` for it. They had to be
  new characters: on the test world all 1000 live bots were 30 and up and the
  1500 benched ones level 1, and a bot's level is never set by hand. The
  bootstrap's playerbotify edit carries a marker now, because this one writes
  into it.
- **Seban's 1.48.0 calls `collector.init()` from app.py with a DictCursor.** Our
  2.0.47 check read `fetchone()[0]`, which is a KeyError on a dict row: the
  panel's workers failed to boot and item-grants with them, a minute after the
  first deploy. `SELECT 1 ... LIMIT 1` with `fetchone() is None` works with
  either cursor, the shape of his original SHOW COLUMNS. Test a merged panel by
  starting it, not by `ast.parse`.
- **A horse destroyed by anything but its own rider left the rider holding it.**
  `CHARACTER::Destroy` on mt2009 unlinks a horse from its rider only under
  `IsPC() && GetRider()`, and only a horse has a rider, so it never did: a horse
  destroyed any way but its rider's own `HorseSummon(false)` left `m_chHorse`
  dangling, and the rider's next `StartRiding()` called `HorseSummon(false)` on
  freed memory - game1 SIGSEGV twice in six hours at 2000 bots, both
  `CPlayerBotManager::Update -> CHARACTER::StartRiding+0x251 ->
  CHARACTER::HorseSummon+0x6e` (sizowski, 15 September). What destroys one is a
  splash skill: `battle_is_attackable` ends in `CPVPManager::CanAttack`, which
  refuses NPC/WARP/GOTO by type and lets every other NPC through, and
  `SetPlayerBotRidingForTravel` sends a dismounted rider's horse away only in a
  safe zone, so a hunting map is full of follower horses standing among area
  skills. `apply_horse_rider_links` (playerbotify.py) unlinks a horse from a
  rider that still holds it and makes a horse with a rider nobody's target;
  char.cpp and pvp.cpp already ship staged. A player's summoned horse had the
  same crash waiting, it was only rarer.
- **Shinsoo's and Jinno's easy Monkey Dungeons had no geometry slot.**
  `GetPlayerBotMonkeyGeometrySlot` named 25, 108 and 109, so maps 5 and 45
  answered NULL, no chamber or door was known there, and those kingdoms' bots
  hunted the entrance room alone ("Bots only farm in starting zone of Ape
  Dungeon", Dixdros, 14 September). The geometry was always read off each map's
  own NPCs; the slot was all that was missing. A per-dungeon array has to grow
  with `IsPlayerBotMonkeyMap`. The test world's logs from before the fix have
  bots entering 5 and 45 and no geometry line for either, ever; the first
  entry after it logged `geometry map=5 chambers=11/11 doors=24`, and a minute
  later the second bot in logged `spread crossed ... from=0 to=2`.
- **A bot spends only the bonus stones in its bag.** `BuyPlayerBotBonusStone`
  created 71084/71085 with `AutoGiveItem` for 25 000 yang whenever the bag had
  none, reasoning that the stones cannot be dropped or traded - true of r40250's
  item_proto, not of mt2009's, where both drop (`mob_drop_item.txt`) and come
  out of chests. With the chests switched off, the gear history showed stones
  spent that no bag had received and the economy charts had none of (seban
  latino, Drip, 15 September). The operator chose "only its own":
  `HasPlayerBotBonusStone`, and a bag with no stone and no marble ends the pass
  at once. No counter lists a stone, so drops and chests are the whole supply.
- **The F9 panel's login probe told every player "no such command".** The
  client sends `/gmpanel_check_gm` some 300 frames after every entry into the
  game, and F9/F10 send `/gmpanel_open` and `/botadmin`; with HIGH_WIZARD and
  IMPLEMENTOR rows in `cmd_info[]` a player got "Ta komenda nie istnieje." after
  every teleport (NerrVoVy, 15 September). The three rows are GM_PLAYER, each
  command checks its own threshold and answers nothing below it, and every
  action of the panel still checks its own row.
- **`/transfer` of a bot was a WarpSet.** It took the bot off its sectree and
  the rescue put it back at its own map's start ("robi tp, ale jakby na start
  mapy"). `do_transfer` hands a bot on this core to
  `CPlayerBotManager::TransferBot` - `TransitionPlayerBotMap` onto the GM's spot,
  the answer in the GM's chat - and refuses a registered bot on another core by
  name, because a bot cannot stand on a map its core does not host.
- **A dropper is drawn only inside its band.** The exp lock stops experience
  and gives none back, so a bot already past its band kept the name and farmed
  a table the engine fades to nothing - a level-45 dropper at a level-35 Metin
  in Bokjung (sizowski, 15 September). `IsPlayerBotPastDropperBand` (more than
  `PLAYERBOT_DROPPER_OUTGROWN_LEVELS` over the lock) draws such a bot as an
  adventurer at its next spawn, and `ManagePlayerBotExpLock` lifts its lock.
  The test world had no such bot to move - not one `exp locked` line above
  lock+2 in its log, and 0 of 250 droppers past their band after the restart -
  so the change shows on an older world like sizowski's, not on ours.
- **A mining session ends at a blow and comes back after a death.** A session
  owns the tick above the fight and the emergency recovery, so a miner could be
  killed at its vein without hitting back, and standing up afterwards counted as
  "busy" with the whole 15-45 minute rest (Mat, 14 September). Health below both
  its last look and its maximum ends the session as `attacked`, recovery ends it
  as `recovering`, and either comes back after
  `PLAYERBOT_MINING_RESUME_AFTER_FIGHT`. The maximum is in the test because a
  falling maximum pulls health down with it and is no blow. In the first four
  minutes on the test world three sessions on Mount Sohan ended `attacked`, and
  the first of those bots dismounted, buffed, cast and drank a potion within
  three seconds of it.
- **A bot in a player's party runs no errand, not only no map change.** After
  2.0.48 a bot with a Biologist or merchant errand still walked off from the
  player's side and the follow pass fetched it back, in turns ("[PT] Ide do
  handlarza bronia (cel: Biolog)"). Pabloo's fix: the stable, the Biologist, the
  start and the continuation of a town visit and the weaponless branch all
  stand down for `IsPlayerBotHumanLedParty`; the visit flags are kept, so
  `ManagePlayerBotBuffHumanLeader` no longer refuses a bot for carrying them.
- **The weapon in the hand is not burned for want of a scroll when nothing
  would replace it.** CiosZKarpia (75, Mental Warrior, 125 million yang) gifted
  her Halabarda +8 at 01:52 on 15 September, burned Zabojca Lwow at +5 -> +6 at
  a plain anvil with no scroll at 02:08, and fought on with a Gilotynowe Ostrze
  +7 of level ten while a Halabarda +6 and three swords of level 55 stood on her
  own offline counter. `IsPlayerBotWornWeaponAtRisk` (the hand weapon -
  `GetPlayerBotHandWeapon`, worn or the one a blacksmith session keeps in the
  bag - at a step of `PLAYERBOT_WORN_SCROLL_MAX_PROB` or under, no
  `FindPlayerBotBackupWeapon`, and above `GetPlayerBotMerchantWeaponCeiling`)
  goes under a scroll or waits; `CanPlayerBotAttemptRefineItem` asks it, so the
  planner agrees, and `PlayerBotNeedsScrollForWeapon` makes the bot a scroll
  buyer. The backup weapon is never a gift, scrap or counter goods. Level-30
  weapons are left out: grinding those at the anvil is the operator's rule.
  `BotOfflineReclaimLine` takes a piece back off the own offline counter when
  it beats what is worn and what is in the bag by
  `PLAYERBOT_OFFLINE_RECLAIM_MIN_GAIN_PERCENT` (probed once a minute while the
  hand is empty; one line is not taken back twice in six hours) - without the
  margin the first run took pieces back for half a point of blow. And the
  Mental Warrior's two-hander preference is a share of its blow
  (`PLAYERBOT_TWO_HANDED_PREFERENCE_PERCENT`), not a flat 200 000.
- **A weapon's damage lines are priced between Iwakura's bands.** As steps,
  19%/-5% and 1%/+3% were both x1.2: two Ostrza z Czerwonej Stali +0 at
  15 150 000 ("czy nie pracowalismy nad tym, aby premiowana bardziej byla z
  wyzszymi srednimi?"). `GetPlayerBotDamageTierPct` takes his number as a
  band's middle and runs straight between middles (x1.33 and x1.10 for those
  two); `PLAYERBOT_PRICE_TABLE_VERSION` 5 reprices every counter.
- **Keys, hoards and marbles reach a counter.** On 15 September 2598 gold and
  silver keys lay in 1057 bags with no chest they open, 158 bots held 19 577
  Nieznane Lekarstwo, and the ledger called 7182 listing decisions of an hour
  overstock. `IsPlayerBotSurplusTreasureKey` (two of a kind kept; the safebox
  under pressure; withdrawn and bought when a chest turns up),
  `IsPlayerBotHoardedMaterial` (50 over the anvil's reserve: packs of ten,
  three lines a counter, past the ledger; the offline service cuts the pack
  itself, `BotOfflinePrepareLine`) and `PLAYERBOT_SHOP_REASON_HOARD`, rolled
  against TRADE like the books. The shop pass asks for its reason only after
  the cheap gates: it read the whole bag on every tick of every bot without a
  counter. Helmets and shields are loot whatever their merchant price.
- **A scroll that is also a recipe material was priced as a material.**
  Recipe 501 consumes the Blessing Scroll, so the material branch of
  `ScorePlayerBotShopStock` and the ledger decided it, and the 2.0.31 rule
  (one trader in five keeps one scroll and sells the rest) never ran: 8
  scrolls on 978 counters, 312 in 177 safeboxes. Scrolls are scored before the
  materials, kept out of the material deposit and withdrawn. Nor did a trader
  ever hold "a stack of two" Moonlight chests - the chest pass opens one eight
  seconds after the drop - so a trader now keeps them for the counter, up to
  `PLAYERBOT_CHEST_TRADER_HOLD`.
- **An item AutoGiveItem put on the ground must never be equipped.** BROLID
  (15 September): 11:10:34 the emergency weapon was bought into a full bag
  (log.log SYSTEM_DROP) and equipped anyway, 11:15:11 its ground timer found
  "Owner exist", 11:16:13 a burn at the anvil ended in RemoveFromCharacter's
  "Invalid Item Position" and a destroyed item left in the weapon slot, and the
  equipment pass swapped a sword over it 21 times in a second
  (`old_vnum=1947153072`) until FAST_ITEM_SWAP threw the bot out.
  `BuyPlayerBotEmergencyWeapon` asks for room first and for the item's owner
  and window after; `IsPlayerBotWornItemSound` keeps the equipment pass and
  both refine passes off a slot the engine does not really wear; the engine's
  `UnequipItem` takes `GetEmptyInventoryEx` without checking it, so the rod
  refine and the pickaxe ask for room before they unequip.
- **The weapon atlas is the world's own tables, rendered.**
  `tools/generate_weapon_atlas.py` (dumps of item_proto, mob_proto and the
  shops, plus the locale directory) writes `playerbot_weapon_atlas.h`: 126
  families, who may carry them, and where one comes from - a merchant on a bot
  map, the common drop of a rank and level band standing there, a monster or a
  chest there, or only elsewhere; 93 are reachable. `playerbot_weapon_goal.h`
  gives each bot the best reachable family at its level: outclassed by 30% and
  able to pay, it walks to the market; a counter weapon 25% better than the
  hand is paid out of the strategic budget; `PLAYERBOT_WEAPON: census` counts
  the gap. Worth knowing before promising more: weapons of 65 and up drop only
  on maps 67/68 (bots walk them) and 70/73 (hosted, never walked), no family of
  80 or more has any source, and Weapon Shop Dealer 2 (9007, weapons to 60) is
  commented out in every village's npc.txt.
  A rod or a pickaxe in the hand is the session's tool, not a weapon with a
  blow of nothing: the first run's goals named anglers and miners
  (`hand=27430 blow=0`, `hand=29101 blow=0`), for whom every counter weapon was
  a strategic offer. `GetPlayerBotHandWeapon` reads the bag when the hand holds
  a tool, and `BotOfflineReclaimLine` measures a line against the weapon in the
  bag, not against the rod. The census works out at most
  `PLAYERBOT_WEAPON_CENSUS_REFRESHES` stale goals and carries on from the pid
  it stopped at: the goal refresh and the census share one ten-minute interval,
  so a census that always started from the first pid would read the same bots
  every time.
- **The 1.x updater and installer refuse a 2.x server.** seban latino (15
  September) ran `docker compose exec updater m2-updater`, which bypasses the
  2.x compose file's entrypoint (update.sh): it fetched main and tarred the
  repository's linux-port/docker over the stack, MariaDB 10.11 started on a
  database 11.8 made, and two log indexes were damaged in two minutes.
  `stack_engine` in m2-updater and the same test in `installer/install.sh`
  read the ENGINE file (or a compose file naming MariaDB 11) and stop.
- **A withdrawal must not fill the bag the deposit then empties.** The two
  safebox rules are each other's inverse only item by item: the deposit waits
  for bag pressure (`IsPlayerBotBagFull`, eighteen free cells at 80%, or
  `PLAYERBOT_BAG_PRESSURE_FREE_CELLS`), while the withdrawal took any material
  the ledger said somebody was short of into any free cell. Demand moves with
  every minute's ledger, so a bag the withdrawal had filled sent the same stack
  back down on the next visit. On 15 September 538 of 4060 withdrawals went
  back within fifteen minutes, 537 of them with no refine in between, and
  Soul1994 visited the storekeeper four times in eight minutes. The ledger's
  half now takes only what leaves the bag clear of that pressure. The anvil's
  half is unchanged, because the deposit never sends down what the anvil needs.
  `reason=` in `safebox withdraw` says which half moved an item.
- **A bot's status is a text tail on the 2.x line, not talking.**
  `SendPlayerBotOverheadChat` sent `CHAT_TYPE_TALKING` with the bot's VID,
  and the client's `RecvChatPacket` registers a talking packet's text tail
  *and* appends the whole line to the chat history
  (PythonNetworkStreamPhaseGame.cpp), so a town of bots filled the chat window
  with statuses. Under `PLAYERBOT_ENGINE_MT2009` it sends the server command
  `PlayerBotStatus <vid> <hex>`: `CHAT_TYPE_COMMAND` goes to `ServerCommand`
  before anything touches the chat, game.py hands it to
  `client-root/playerbot_status_tail.py`, and that calls
  `textTail.RegisterChatTail` and nothing else. Hex because the command parser
  splits its line on spaces; the bytes are the status's CP1250, at most
  `PLAYERBOT_STATUS_TAIL_MAX_BYTES`. A root without the handler returns 0 from
  `BINARY_ServerCommand_Run` and the C++ ends in `TraceError("Unknown Server
  Command")` in syserr.txt - no chat line and no bubble - so the server and the
  client root have to ship in one release. game.py's two lines are
  `clientrootify.py`'s, with anchors that take in the following line, so a
  second run changes nothing; `tests/playerbot_status_tail_test.py` runs the
  decoder on Python 2.7 (the client's) and 3. The refine announcement
  (`BroadcastPlayerBotRefineSuccess`, one shout in three minutes for the whole
  world) and the trade shouts stay shouts. The root's own `PlayerbotOverhead`
  handler is OskarPWA's GM-only bot-admin overlay; its server half
  (`SendPlayerBotOverheadTail`) was never merged, and nothing sends it.
- **How a refine was made lives in log.refinelog, and its SET column lost
  most of it.** The gear history's "Ulepszenie udane" said nothing of the way
  (Tieru, 15 September: "w nawiasie pisz (Kowal, Zwoj Blogoslawienstwa, ...)").
  `DoRefine` logged POWER for the plain blacksmith and the Demon Tower smith
  alike (`bMoneyOnly`, the `REFINE_TYPE_MONEY_ONLY` path of
  `CInputMain::Refine`), `DoRefineWithScroll` logged SCROLL for every scroll,
  and `setType` was a SET that silently dropped the three longer names this
  engine writes. `apply_refine_log_way` (playerbotify) writes DEVILTOWER and
  `SCROLL:<vnum>` (the vnum taken before `SetCount` can destroy the last
  scroll), logschemify makes the column varchar(40) with an index on
  (pid, time), and `match_refine_ways` in admin_panel.py pairs each refine row
  of log.log with its refinelog row - the same pid within two seconds, the
  same outcome, the grade the attempt started from - and puts the way in
  brackets, a scroll by its item_proto name. Two things the rows taught: a
  refinelog row names the *old* piece while a success's log.log row names the
  new one, a grade up; and a scroll's downgrade writes REFINE FAIL for the new
  piece *and* REMOVE (REFINE FAIL) for the old one, the reason a burn uses, so
  the history read "Spalone +3" over a sword that was now +2 (CiosZKarpia,
  12:56). A REMOVE with a REFINE FAIL one grade lower beside it is skipped.
  This world's "Magiczny Metal" (39016/71026) is a bonus item, not a refine
  scroll; the no-reduction stone is Magiczny Kamień (25042).
- **The Biologist ranking named the row at the position of the count.** "6/9 •
  Grzyb Tue" was the sixth row of the table beside a count of six, read as
  "done up to the Tue Mushroom" - but rows are not finished in order: an
  outgrown row is stepped over, so bots whose fifth finished row was the Demon
  Souvenir read "5/9 • Bez", and OddajKonto, six herbs done and the Orc Tooth
  at 1/10, read as a mushroom collector (Tieru, 15 September: "nie rozumiem
  statusu biologa"). The card and the ranking now ask one function,
  `biologist_progress` in admin_panel.py, which picks the row the way
  `GetActivePlayerBotBiologistMission` does - carrying, then the monster on the
  live map, then the first not outgrown, then the highest left - reads the key
  phase (`key_item` is -1726153001 in every quest, the state index being a hash
  of its name) and counts the outgrown rows it skipped. The ranking says "6/9
  ukończone • teraz: Ząb Orka 1/10"; the card adds "za niskie dla bota,
  pominięte: 7", which is how a bot of seventy reads 1/9 beside the Demon
  Souvenir.
- **300 packets in one second close a player's connection.**
  `CInputMain::Analyze` counts a PC's packets while they arrive within the same
  second and at 300 logs `FLOOD_HEADER_<header>` to `log.hack_log` and sets
  PHASE_CLOSE - the client is back at the login screen with nothing in its
  own syserr. The inventory's auto-stack button sent a move (header 13) for
  every pair of stacks of one item in a single frame, 300 for 25 stacks
  (l0st3k, 15 September). `client-root/autostackpump.py` sends the same moves
  six a tenth of a second. Anything a client script sends in a loop wants the
  same pacing; commands are separately limited to five in half a second
  (`ENABLE_ANTI_CMD_FLOOD`), and the ones over it are dropped silently.
- **Auto Lowy is the client's walk with the server's eyes.** The player's
  auto-hunt (Tieru, 15 September: free, no requirements, the official window's
  features) is `client-root/uiautohunt.py`, run as one of game.py's
  updateables. Python in this client has no list of the characters round the
  player - the public scripts scan a million VIDs a frame and teleport with
  `chr.SetPixelPosition` - so the hunt asks `/autohunt_target <range> <stones>
  <x> <y>` under a second apart and `do_autohunt_target` (`apply_auto_hunt`,
  playerbotify.py) answers `AutoHuntTarget <vid>` from `ForEachAround`:
  monsters (stones on request) that `battle_is_attackable` allows, within the
  range of the hunt's start, what hits the hunter first. The walk is
  `chr.MoveToDestPosition` on the main instance, the swing is the attack key
  and `chr.SetRotation`, a skill is `player.ClickSkillSlot`, a potion
  `net.SendItemUsePacket`, standing up `/restart_here` (refused for ten
  seconds after death). `tests/uiautohunt_test.py` drives the decisions against
  stub modules on Python 2.7 and 3; the client itself was not run by us.
- **A dropper is a drop character, and nothing else asks for its time.**
  The operator's medal droppers of twenty-five were found doing everything but
  their dungeon: none of 99 on a Monkey Dungeon map, thirty on the Biologist's
  goal, ten resting on Yongan's square, and 118 of the 127 medal droppers in a
  guild; one bot's log has the horse errand flip to the Biologist the second
  its bag filled ("dropki medali ... robia rozne rzeczy jak biolog", "niech nie
  dochodza do gildii, to tylko dropki", Tieru, 15 September; "latają po m2",
  sizowski). `GetActivePlayerBotBiologistMission` answers NULL for every
  `IsPlayerBotDropper` personality - the planner, the pass, the travel and the
  status all ask it; `RollPlayerBotMetinExpedition` gives a medal, M2 or M3
  dropper no expedition (the Metin dropper's table is the stones);
  `MayPlayerBotRestInTown` refuses it; and `ManagePlayerBotGuild` neither
  founds nor recruits one and takes one already inside out
  (`LeavePlayerBotGuildAsDropper`: `RequestRemoveMember`, or for a master
  `ChangeMasterTo` the strongest non-dropper bot of that guild in sight, or
  `RequestDisband` for a guild of one). That was not the half of it: in the
  first twenty-five minutes after that restart 116 of the medal droppers took
  350 market trips (75 of them walks from the second village back to Joan), 68
  service walks to their offline shops and 124 material errands, some went
  fishing, and three reached a dungeon. None of it is a dropper's business any
  more - `ManagePlayerBotShopping` returns for every `IsPlayerBotDropper`,
  `IsPlayerBotAngler`, `IsPlayerBotMiner` and `StartPlayerBotMaterialHunt`
  refuse one, its offline shop is served every
  `PLAYERBOT_DROPPER_SHOP_SERVICE_MIN_MS` to `_MAX_MS` rather than every ten to
  fifteen minutes, nobody's is served out of a Monkey Dungeon
  (`BotOfflineBusy`), a medal dropper never leaves for the frontier
  (`ShouldPlayerBotLeaveForFrontier`) and a medal in its bag holds it back
  nowhere (`holdsMedalToHandIn`). Two gates were opened for every bot on its
  way to a medal: the Joan-first market walk, whose gate is in the village the
  walk leaves, and the soft half of `NeedsPlayerBotCriticalTownServices`, which
  counts a bag at 45% as critical - and a keeper's bag holds fifty to seventy of
  its ninety cells, twenty-four of them materials and nine potions. The next
  fourteen minutes had 37 dungeon visits among the medal droppers, and 29 of
  them ended as "horse complete" (the six looked at after 36 seconds to three
  minutes): the medal dropper still left at `PLAYERBOT_BAG_FULL_PERCENT`, and
  one holding five medals walked in and out in nine seconds against a stock of
  five. It stays now while a medal has a cell
  (`ShouldPlayerBotPursueHorseExpedition`), its stock is a full stack
  (`PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK`), and `PLAYERBOT_MONKEY: exit` says
  which gate closed a visit (`free_cells`, `medals`, `stock`, `visit_s`). Its
  first run named the bag: 28 of 37 exits in fourteen minutes with no free
  cell, the visits averaging 169 seconds - the dungeon's floor filled what the
  counter's stock had left. So a medal dropper picks up only the medal, the
  goods a player crafts further, a skill book and what pours into a stack it
  already carries (`IsPlayerBotMedalDropperLoot`, in the loot collector), and
  a dropper's first service visit after a spawn is spread over its long round
  too, where the ten minutes had sent medal droppers to the first village 69
  times in fourteen minutes.
  Fourteen minutes after that: 93 of the 125 medal droppers stood in a Monkey
  Dungeon (26 with the bag rule alone, 6 with the errands cleared alone, and
  none of the 99 of level twenty-five before any of it), 111 with the horse
  as their goal and the other 14 saving their skins; 83 visits began, all 18
  that ended were restocks, and the fullest bag still had three cells free.
  No market trip, errand, angler or miner, and two service walks. The restock
  was not potions, which is what this note said first: `needsPotions` is
  `NeedsPlayerBotEmergencyPotions` - under ten red, or eight blue for a
  caster - and the bots that left carried hundreds of both. They were
  archers: `NeedsPlayerBotArrows` sends one out under
  `PLAYERBOT_ARROW_RESTOCK_THRESHOLD` (a hundred), and the merchant pass
  bought arrows only while that need stood. A dropper archer fills up to
  `PLAYERBOT_DROPPER_ARROW_STOCK` there now (`WantsPlayerBotArrowTopUp`, and
  never with an emergency sale). In the fourteen minutes after that restart
  109 of the 119 medal droppers stood in a dungeon, the 14 visits that ended
  were restocks of 14 archers, and 85 purchases put 17 000 arrows into their
  slots; twenty minutes on, the same fourteen held 650 to 915.
  `PLAYERBOT_MONKEY: exit` does not say which need a restock was, and arrows
  are worn: `EQUIPMENT` position 9 in `player.item`, so a query of `INVENTORY`
  alone reads every archer as empty.
- **The unique slots took whatever came to hand.** The equipment pass scores a
  unique by its lines and an empty slot beats everything, so a unique with no
  line on it was worn as readily as any other - Pierscien Niejawnosci (70007),
  which hides the level (`IsLevelViewable`), was on eleven bots ("Bot Toty nie
  ma widocznego lv, dlaczego?", Tieru, 15 September). `playerbot_unique_slots.h`
  owns the slots now. 70007, Plaszcz Uciekiniera (70048, the alignment title)
  and Maska Sabaha are taken off and never worn (`IsPlayerBotNeverWornUnique`,
  also refused by `IsPlayerBotEquipmentCandidate`). The rings of experience
  (group 10000 and 70005, half as much experience again) and the thief's gloves
  (group 10002: 70043, 72004, 72005) go on only while the bot hunts: their
  minutes run only while worn - value2 is 0 on all of them, so
  `unique_expire_event` takes a minute a minute and stops at the unequip - so
  the pass takes them off in a safe zone, on an errand, at the water or the
  vein, behind a counter, in a duel and after `PLAYERBOT_TIMED_UNIQUE_IDLE_MS`
  without a blow ("oby nie ubierali ich w miescie"). An exp-locked dropper
  takes the glove and leaves the ring, and the equipment pass never displaces a
  worn one. 72006 pays only against bosses and stones and 71016 is used rather
  than worn, so neither is on the lists. The test world had one glove and no
  ring in any bot's slot before this: the drops are rare.
- **What a player crafts further is not merchant fodder.**
  `IsPlayerBotPickupGoods` (gear.h) is the herbalist's Korzen Gango and Grzyb
  Tue (50724, 50726 - not the Biologist's 50704/50706, which are quest items),
  Krysztalowe Kolczyki (17160-17169), Zbroja Twarzy Ducha (11670-11679), every
  weapon of level 65 (fourteen families, 140 to 7140), Fasolka Zen (70102) and
  Pigulka Krwi (70014). The choosy looter walked past them - herbs at nine
  yang and gear outgrown under +4 are exactly `IsPlayerBotLootBeneathBot`'s
  fodder, and a bot of seventy-three left a Zbroja Twarzy Ducha+3 on a floor -
  and the junk rule vendored the herbs as a non-gear material ("warto
  podnosic, aby dalej przerabiac", Tieru, 15 September). The loot filter never
  leaves them now, the junk rule lets the merchant have them only from a bag
  under pressure with no counter, and the counter ranks them at
  `PLAYERBOT_SHOP_PICKUP_GOODS_SCORE`, beside the materials. The rings and
  gloves needed nothing here: they carry ANTI_SELL and were always picked up.
- **A duel is not a hunt.** `ManagePlayerBotDuelCombat` claims the tick above
  the buff pass, so a duellist never put its aura up; it swung from the hunt's
  280 units, a monster's size; and its rotation ran on the hunt's clock - one
  skill in a duel of twenty seconds (Tieru, 15 September: swords waved from
  afar, Trzystronne Ciecie under no aura, no Szarza, no Wir Miecza). Inside
  `PLAYERBOT_DUEL_BUFF_RANGE` it asks `ManagePlayerBotCombatBuffs(.., duel=true)`,
  which skips the town and errand gates; it swings from
  `PLAYERBOT_DUEL_MELEE_RANGE`, casts through `CastPlayerBotDuelSkill` on
  `PLAYERBOT_DUEL_SKILL_INTERVAL`, lets a Shaman and a black-magic Sura cast
  from `PLAYERBOT_DUEL_CASTER_RANGE`, and a warrior charges a foe 250-600 units
  off (`TryPlayerBotDuelGapCloser`: Szarza for the body, Uderzenie Miecza for
  the mind). "Boty w PvP uzywaja potki czerwonej" was the auto potion: the
  potion ban stopped the drinking, but `AutoRecoveryItemProcess` heals by
  itself whenever a switched-on auto potion runs. `SwitchOffPlayerBotAutoPotionsForDuel`
  uses the running one again - the engine's own switch, the item found by the
  id the affect carries in `dwFlag` - and `ManagePlayerBotAutoPotions` switches
  none on during a duel; it puts them back within a minute of the end.
- **Hwang has no curse, and the world no Maska Sabaha.** `CHARACTER::Damage`
  turned every blow at a monster on map 65 into a DODGE unless `number(1, 100)`
  beat 50 plus `POINT_BREAK_TEMPLE_CURSE`, which the mask's apply 146 lifts by
  100 - a player without the mask missed half his blows there (NerrVoVy;
  "bedziemy musieli usunac wymog i ten item", Tieru, 15 September).
  `apply_hwang_curse_removed` (playerbotify) removes the block from the staged
  `char_battle.cpp`. The share step `shareify.py` renders zeroes the mask's six
  drop lines in `mob_drop_item.txt` and its line in the Hwang loot box (20706),
  and takes it out of `reward_data.hwang_introduction` - a group stops reading
  at the first index it lacks, so a line is zeroed, never deleted - and
  `apply.sh` deletes it from `world.shop_item` (shop 16). The `sabaha` group
  (10026) in special_item_group.txt is a unique-group membership list, not a
  source, and stays. Nor does anybody keep one: `apply.sh` deletes every Maska
  Sabaha from `player.item` on every start - bags, worn slots, safeboxes and
  counters alike (Tieru, 15 September, "usun" to the masks players already
  held) - so a mask an old core still held while an update ran the migrator
  beside it goes on the next start. A bot sells one before that anyway
  (`IsPlayerBotRetiredItem` is junk and never counter goods). The test world
  held 138, 94 of them worn, and none were left for the delete by the time it
  first ran there. Built and checked in the image (the step echoes `share:
  Maska Sabaha removed`, the six lines read 0, the literal of the curse's debug
  line is gone from the binary).
- **A village's market stands on its guard.** `GetTownPitch` took the centroid
  of the eight service NPCs for the Shinsoo and Jinno villages, which in Jayang
  is the merchants' side of the square ("sklepy sa zle rozstawione, bardziej
  przy handlarzach niz przy kole, straznik ... na kordach 457, 630", Tieru, 15
  September) and ran Pyongmoo's ring half out of the safe zone. Each kingdom's
  guard - 11000 Shinsoo, 11002 Chunjo, 11004 Jinno - stands in the middle of
  its village's round square, and Chunjo's hand-made pitches were always on
  11002. The four others are the cell under their guard now; measured on each
  map's server_attr, the whole ring of 400 to 1700 round each is open ground
  inside the safe zone (Jayang's old ring: 220 samples of 504 in it, Pyongmoo's
  353). `tests/playerbot_empire_rules_test.cpp` pins the four. A moved pitch
  moves no shop by itself: `OpenOfflineShop` takes the keeper's position, a
  reopen included, and a keeper walks to its shop to serve it. `apply.sh`
  carries each bot's shop of the old ring across by the distance between the
  two pitches, which keeps the ring's shape and spacing, pulled in to 1650 of
  the guard where it stood further out; a shop already inside the new ring and
  outside the old one stays. Once, in one transaction with its marker
  (`player.playerbot_migrations`, `pitch_on_guard_2052`); on every later start
  only a bot's shop still within 2000 of an old pitch and more than 2000 from
  the new one moves - a keeper that reopened on the old spot while update.sh
  ran the migrator beside the old game. The db core writes a shop's position
  only when a shop is opened or moved (`IkarusShopCache.cpp`), so an old core
  cannot write the moved ones back. On the test world it carried 611 shops in
  seven seconds, and the shops standing outside the safe zone on those four
  maps went from 136 of 673 to 21 of 675, one of them put just past the edge by
  the move; ten pairs now stand closer than fifty units, where none did - the
  number server_attr gave before the run (`scratchpad/check_pitch_shops.py` is
  the shape of that check). Moving only the shops outside the new ring was the
  other candidate, and it put 37 pairs on top of each other.

- **Poison is a boss's bane, so its line is worth more where the bosses are.**
  `poison_event` (char_resist.cpp) takes `GetPoisonDamageRate` per mille of the
  victim's maximum health ten times, three seconds apart; the rate is 25 for
  `MOB_RANK_BOSS` and 1 for a king, and `IsImmune(IMMUNE_POISON)` is commented
  out. One proc is therefore a quarter of the Orc Chief's, Nine Tails', the
  Spider Queen's or the Yellow Tiger Spectre's health - all four rank 4, none
  immune - and next to nothing against the Spider Baroness or the Elite Queen.
  From `PLAYERBOT_POISON_BOSS_LEVEL` the reroll (`ScorePlayerBotBonusLine`) and
  the equipment score count the line double ("przyda im sie w ekwipunku tez
  bonus szansa na otrucie", Tieru, 15 September). The bots were hunting bosses
  already - fifteen hours of the test world's syslog before this held 447 raid
  departures (Yellow Tiger Spectre 218, Nine Tails 125, Bestial Captain 85, Orc
  Chief 19), 173 guild calls and 108 marbles used on a boss (591 57, 792 25,
  1901 12, 791 10, 691 4) - and the one boss hub nobody reaches is the Spider
  Queen's in V1: one raid line in those fifteen hours.
- **A bot gives nothing away; it trades.** `SharePlayerBotOldGearNearby`
  handed a spare at +6 or better to a weaker bot of the same class and group
  within 2200 units - after every upgrade and from the party-share pass, party
  or not - and no counter ever saw it: a bot raised Srebrne Kolczyki from +1 to
  +6 at 13:44 on 15 September and gave them to Igor94PL at 13:45 while it wore
  copper earrings itself ("dobry samarytanin", AkhiGubernator; "niech handluja
  ale nie daja za darmo", Tieru), and seven such gifts ran in the first three
  minutes after that evening's restart. The function is gone; what comes off
  stays in the bag for the junk rule and the counter. The party's share went
  the same way (`SharePlayerBotUsefulItemWithParty`: a book of another class
  and a material a member was short of, "usun", Tieru): a member buys either
  off a counter like anybody else. The old `PLAYERBOT_GIFT_OUT/IN` rows stay
  readable in the gear history.
- **Kamien Duchowy is read, not sold.** 50513 is an ITEM_QUEST with antiflag 0
  and nothing in the junk rule kept it, so the merchant bought every one for
  194 yang ("Boty sprzedaja kamienie zamiast z nich korzystac", mateuszp211, 15
  September) and the test world held none. It comes from boss chests (Nine
  Tails', the Yellow Tiger's, the Fire King's, the Reaper's), the Bestial
  Captain and the Orc mini-boss. `ManagePlayerBotGrandMasterTraining`
  (playerbot_manager.cpp, beside the books) does what
  `training_grandmaster_skill.quest` does behind its dialog: a skill of the
  build at G1..G10 (the primary first, then the highest grade), twelve hours
  between reads in the flag `training_grandmaster_skill.next_time` (waved away
  like the books' wait while the BOOKS switch is on), the stone spent before
  `LearnGrandMasterSkill` rolls (30%, 4% under the grade's minimum read count),
  and the rank it costs - `1000 + 500 * (level - 30)` real alignment on a
  success, a third to a half of that on a failure, doubled below zero. A bot
  reads only while the full price leaves `GetRealAlignment()` at zero or above,
  so it never carries a negative rank, which is what `ItemDropPenalty` makes a
  character pay when a player kills it ("boty powinny unikac biegania z
  negatywna ranga", Tieru). A monster within ten levels gives +2 a kill (+7
  below zero), and the bots of forty and up held 1000 to over 20000 with none
  below zero. The stone is never junk and never counter goods. Fasolka Zen
  (70102) lifts a negative rank by up to its value0 of 5000 and the engine takes
  it only then; `ManagePlayerBotZenBeans` eats one when the rank is below zero,
  and a counter keeps `PLAYERBOT_ZEN_BEAN_KEEP_MIN` to `_MAX` back - ten to
  fifteen by pid since 2.0.95, "pod robienie skilli na P", counted over the
  whole bag and listed in lines of `PLAYERBOT_ZEN_BEAN_LINE_UNITS`. The keep
  was two and counted only over the cells in front of a stack, so a bot's one
  stack was never goods: 18 000 beans in bags on blasty's world and none on a
  counter. A rank that is below zero all the same keeps
  its bot in the safe zone until a bean lifts it, and the bean is the only way
  back (Tieru, 15 September): `KeepPlayerBotNegativeRankInTown`, in the tick
  ahead of the loot, the errands, the travel and the fight, and never for a bot
  in a player's party, holds a bot inside the ring with `dwTownLingerUntil`
  (which the inactivity watchdog reads as a town linger), walks one on a
  village map to its pitch and carries one on any other map home. The market
  and bean passes both run above it, so a held bot still shops - a dropper
  too, which otherwise buys nothing (`ManagePlayerBotShopping`) - and
  `WantsPlayerBotStallItem` wants one bean while the rank is negative and the
  bag holds none. Compiled and deployed; with no stone in any bag and no
  negative rank on the test world, none of these passes has been watched
  firing yet.
- **Auto Lowy picks up by kind, and the server names the item.**
  `player.PickCloseItem` takes the nearest item whatever it is, and the window
  was asked for "nie podnos broni, zbroi" (Tieru, 15 September).
  `/autohunt_loot <range> <kinds> <x> <y>` (`do_autohunt_loot`,
  `apply_auto_hunt` in playerbotify.py) answers `AutoHuntLoot <vid> <x> <y>`:
  the nearest item on the ground that `IsOwnership` lets this character take,
  within the hunt's range, of a kind in the mask - `AutoHuntLootKind`: weapons
  but not arrows, armour (body, helmet, shield), jewellery (the other armour
  slots, rings, belts), potions (USE_POTION, _NODELAY, ABILITY_UP), books
  (skill and forgetting), stones (ITEM_METIN), and the rest; yang goes with any
  kind. The client walks there and sends `net.SendItemPickUpPacket`, which
  `PickupItem` judges as it judges anybody's (`DistanceValid` allows 600 since
  @fixme173, one pick-up every half second); an item it cannot reach in six
  seconds is left alone for ten. The same change gave the window what the
  official premium sells, for everybody: six skill slots, three items on a
  clock, a revive delay field, and the kinds as switches ("A switch says
  what it is set to", below). `tests/uiautohunt_test.py` covers the mask, the walk and the pick-up,
  the unreachable item, the sixth skill and the third item on Python 2.7 and 3;
  the operator ran the first version in the client, not this one.
- **A giftbox that hands out the next giftbox needs the next one's group, and
  nothing says so until somebody opens it.** The starter chain `shareify.py`
  renders for mt2009 stopped at lv60 (50193), and lv60's last line is 50194,
  Skrzynia Mistrza II of level seventy - so the chain went on in every bag and
  nowhere in `special_item_group.txt`. A missing group is no load error: the
  file loads, and `CHARACTER::DropSpecialItemGroup` writes "cannot find special
  item group 50194" and hands out nothing at every use, a player's use too.
  Only the chest pass in `playerbot_consumables.h` remembered a refusal;
  `ManagePlayerBotProgressionChests` comes back every ten to fifteen seconds and
  remembered nothing, so on m2zip on 15 September 195 bots of seventy asked for
  it some 560 times a minute - 258 175 syserr lines in thirty hours - with 368
  bots of sixty holding one they would start on at seventy. r40250's share
  goes on to lv70, lv80 and lv90 (50194 -> 50195 -> 50196) and every item those
  three hold is in this package's `item_proto` (the lv30 and lv60 rows were the
  ones with r40250 items missing), so they are copied as they stand; the
  Dockerfile's awk cuts whatever vnums the two appended files define, read out
  of them rather than typed; and `check_chain` refuses a starter file that hands
  out a giftbox with no group - against the old file it names 50194. Taking
  50194 out of lv60 was the other way, and it would have left 574 boxes in bags
  for good: the chain's antiflag is DROP, SELL, GIVE, STACK, MYSHOP and SAFEBOX.
  The weapon recovery in `PrepareWeapon` opens the same chests every second
  with the result ignored, so both passes ask `IsPlayerBotChestRefused` and tell
  `NotePlayerBotChestRefused` now, and the progression pass logs
  `PLAYERBOT_CHEST: progression chest refused ... group= room3=` - group=0 is
  this bug. Built and deployed on m2zip the same evening: from the new core's
  start at 20:58:33 not one "cannot find special item group" in syserr, 219
  bots used their Skrzynia Mistrza II once each in the first ten minutes and
  201 of those uses opened it, and at 21:12 the database had 197 bots of
  seventy holding a Skrzynia Mistrza III against 17 still holding the second.
  Most of those 17 were refused by the engine and not for want of a group:
  16 of the 23 bags looked at had free cells and no free column three cells
  high, which `UseItemEx` asks of any giftbox whatever its group holds
  (`GetEmptyInventory(3)`) and `PlayerBotBagTakesGroup` does not model on this
  line. Such a bot asks again ten minutes later to the second (Vyvanse at
  20:59:03 and 21:09:07); modelling that rule is a change of its own. The
  overlay compiles on r40250 with the same 89 warnings as before it. Since
  2.0.54 the progression pass asks `FreePlayerBotGiftboxColumn` before
  `UseItem`, as the Moonlight chest pass does, and both passes use the chest
  by `item->GetCell()`: the column the helper frees may be the one the chest
  itself stood in.
- **mt2009's `affect.add` takes a point, not an apply.** `ALUA(affect_add)`
  tests `applyOn >= POINT_MAX_NUM` and hands the number to `AddAffect` as it
  is, where r40250's converts an APPLY_* through `aApplyInfo`; the locale's
  `apply.MOV_SPEED` (questlib.lua) is still the APPLY_* number 8, and point 8
  is `POINT_MAX_SP`. So the server-wide movement bonus
  (`linux-port-mt2009/docker/game/quest/speed_boost.quest`) gave every
  character on the 2.x line twenty SP and no speed from the port on - 58
  characters carried affect 1000 on point 8 on the test world, found while
  answering a player's question about movement speed (15 September). It
  passes `POINT_MOV_SPEED` (libs/enum/enum_point.lua) now, and the flag holds
  a version, the percentage and the point so the old affect comes off at the
  next check - which runs from a timer, never from `login`: `QuestLoad` calls
  `Login` the moment the quest flags arrive in PHASE_GAME and the affects are
  a later packet (`AffectLoad`, and `LoadAffect` pushes without looking for a
  duplicate), so the first fix removed nothing at login, added the new affect,
  wrote the flag, and the old one loaded in beside it: 1 021 of 1 082
  characters on the test world kept point 8. Any quest that reads or edits
  affects at login has the same race; give it a timer of a few seconds.
  Ten minutes after the timer went live 1 082 characters carried the bonus on
  point 19, and two GM characters that had not logged in since still on 8.
  `affect.add_collect` takes a POINT_* as well (see "mt2009's
  affect.add_collect takes a POINT too" below), and the potions are
  `potion_system.lua` on affect types of their own. Any other quest of ours
  that calls `affect.add` on this line wants a POINT_* number. The engine's
  own ceiling for a walker is 200 (`GetLimitPoint`; sprint adds up to 40 but
  not past 150 on foot, 230 riding; Feather Walk 220).
- **A chest that is opened is a chest somebody buys, and a dropper is who
  sells it.** No rule wanted a Moonlight chest (50011) off a counter, what a
  trader listed stayed listed, and a refused chest was surplus for good:
  4 643 chests in 322 bags (a stack of 148) and 1 916 on the counters of the
  test world, none ever sold ("boty nigdy nie otwieraja ani nie kupuja
  Szkatulek Blasku Ksiezyca", AkhiGubernator). `WantsPlayerBotMoonlightChest`
  (market.h) is a bot of `PLAYERBOT_CHEST_BUY_MIN_LEVEL` that neither trades
  resources nor drops, holds fewer than `PLAYERBOT_CHEST_BUY_HOLD`, has
  `PLAYERBOT_CHEST_BUY_MIN_FREE_CELLS`, the engine's free column and room for
  the group's set (`PlayerBotBagTakesGroup`), and spare gold of
  `PLAYERBOT_CHEST_BUY_PRICE_MULTIPLE` times Iwakura's price;
  `PlayerBotWantsAnythingFromMarket` sends one to the market only while the
  ledger counts a chest on some counter. The first ten minutes of that: 162
  bought, 112 opened a minute against 39. What was left sat in two places. A
  dropper's bag: Metin droppers held 1 199 of the 3 132 chests still in bags,
  in stacks to 158, and a medal dropper never bent down for one (darkroom22).
  And any bag of seventy to ninety cells: a giftbox opens only into a free
  column of three (`UseItemEx` asks `GetEmptyInventory(3)`), so a full bag's
  chests stood unopened, while a non-trader's stack of five went up whole -
  22 counter lines of eleven to thirty chests that no buyer's cap reached. So
  everybody opens the chests but the two sellers: a resource trader
  (`PLAYERBOT_CHEST_TRADER_HOLD`) and a dropper (`PLAYERBOT_CHEST_DROPPER_HOLD`;
  the medal dropper's loot filter takes them now) keep them for the counter,
  in packs of `PLAYERBOT_CHEST_LINE_UNITS` and at most
  `PLAYERBOT_CHEST_COUNTER_LINES` lines (`BotOfflinePrepareLine`); a bigger
  line, or one on the counter of a bot that opens its chests, comes off at the
  next service visit (`BotOfflineUnwantedLine`); and
  `FreePlayerBotGiftboxColumn` moves up to two single-cell items out of the
  column nearest to free before a chest is given up on ("niech boty otwieraja
  duzo tych szkatulek", Tieru, 15 September). Ten minutes after that deploy
  the bags held 2 761 chests and the counters 851, with 35 lines of more than
  five still standing for the service visits to cut; in sixteen minutes 1 349
  were opened, 14 columns freed and two chests refused. What stays in the bags
  is in full ones: 26 bots with two free cells or fewer held 601 of them.
- **Gear for level one is worth a counter only at +7, because +7 is never
  scrap.** A weapon or body armour with a level limit of one or none (Miecz,
  the starter plates) is merchant scrap under
  `PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE`
  (`GetPlayerBotLowGearMinRefine`), the rest of the gear under thirty under
  +6 as before; the two-line cap counts only pieces under
  `PLAYERBOT_SHOP_LOW_GEAR_CAP_BELOW_REFINE` (+7), so a +7 is listed past
  it; and the backup weapon is never scrap (Tieru about PoteznyToporv4's bag,
  15 September). The offline service takes the rest off standing shops one
  line a visit: starter +6/+7 lines 280/127 to 224/110 in the first thirteen
  minutes. Iwakura's sheet 1.1 dropped its "do handlarki" bands and says the
  bots may list +0..+3; the operator's rules above are what the code keeps
  until he says otherwise.
- **No Biologist row is "too low", and the bot goes where the row's monster
  stands.** Until 2.0.60 `GetActivePlayerBotBiologistMission` stepped over a
  row more than `PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS` under the bot, and the
  panel counted them: "Zab Orka 4/10, za niskie dla bota, pominiete: 4" over
  a bot of seventy-eight that would finish neither ("nie ma czegos takiego
  jak za niskie dla bota", Tieru, 16 September). Rows are done in order at
  any level now - the row whose specimens the bag holds first, then the
  first open row - and the travel takes the bot to the monster:
  `GetPlayerBotBiologistHuntMob` names it (the key's monster in the key
  phase, nothing while the bag holds the hand-in), `GetPlayerBotHuntingMobHome`
  makes the valley or the tower the frontier draw for the three collect rows
  (`GetPlayerBotFrontierMapForLevelRaw`, ahead of the level draw and behind
  the horse trials), the six herb rows send a bot anywhere but a first
  village there through `NeedsPlayerBotM1OnlyServices`, and in the village
  the hubs are chosen for the row's level (`GetPlayerBotVillageHuntLevel`),
  because a bot of seventy-eight at the tigers' hub never meets the Gango
  Root's monster. The specimen is the quest's own kill hook, which asks
  nothing about the level gap, and ALLOW_QUEST in the value policy outranks
  the outgrown-prey rule.
- **The horse trial's monsters are quest targets, or the trial never
  happens.** 161 of the 178 bots of seventy and up with a horse at ten on
  the test world had never made one kill of the desert trial (the
  `playerbot.battle_horse_kills` flag absent): the frontier draw sent them
  to the desert, the value policy refused every scorpion as worthless
  experience for a bot that high, and the frontier visit expired with
  nothing killed. `IsPlayerBotHorseTrialTarget` (playerbot_targeting.h)
  marks the battle trial's two archers and the military trial's four demons
  as quest targets in `BuildPlayerBotCombatContext` and in the collector's
  score, for a bot on that trial. Everything else was already there: the
  draw, the `outOfBand` return from any other frontier map, the stable
  keeper's hand-over for 500 000 yang.
- **The three collect rows are one chain, started in order.**
  `EnsurePlayerBotBiologistMissionStarted` sets a row's state directly, and
  `GetActivePlayerBotBiologistMission` took any row the bot was old enough
  for, so 866 characters of the test world held the Curse Book without the
  Orc Tooth and 713 the Demon Souvenir without the Curse Book - where the
  quests start lv40 from lv30's last state and lv50 from lv40's ("Ksiegi
  Klatw sa po Zebach orka", Tieru). `IsPlayerBotBiologistMissionOpen` keeps
  lv40 and lv50 shut until the row before is `__complete`, in every pass,
  and the classic panel's `biologist_progress` asks the same
  (`BIOLOGIST_CHAIN_PREVIOUS`). Rows already started are left as they are.
- **A bot's personality is drawn where a player's alignment title stands.**
  `ManagePlayerBotPersonalityTitle` (status.h, mt2009 only) sends
  `PlayerBotTitle <vid> <personality>` for a bot a player is near, every
  `PLAYERBOT_TITLE_RESEND_MIN_MS` to `_MAX_MS`; game.py hands it to
  `playerbot_status_tail.py`, which calls `textTail.AttachTitle` with the
  classic panel's names in CP1250 and a colour per personality, and its
  `TitleKeeper` (an updateable) attaches it again every second for a minute,
  because the client writes the alignment title back on every alignment
  change - for a bot, every kill. A root without the handler logs "Unknown
  Server Command" to syserr.txt, so server 2.0.53 and client 2.0.9 ship
  together (Kenny's idea). Tested against stub modules, not in a client.
- **Only a refine that landed is news.** `BroadcastPlayerBotRefineSuccess`
  was called for both outcomes on one path and named the piece the refine was
  asked of, so a scroll's downgrade was shouted as luck ("+4 na +3", Tieru,
  15 September). It is called on a success only, takes the result's vnum and
  names it from the item table, and still speaks from +7. The table's name
  carries the grade ("Smoczy Noz+7"), so a line that said the grade too said it
  twice ("no i mam +7 na Pajecza Wlocznia+7", archonek, 19 September):
  `PlayerBotRefineBaseName` takes it off, every line reads name, "z +6 na +7",
  reaction - the name first, because it cannot be declined after "na" - and the
  verbs stay out of the past tense, whose gender the piece does not tell.
- **Plaszcz Uciekiniera and Symb. Krola Przepowiedni stay on the ground.**
  Both are uniques without a line (70048 hides the alignment title, 70050 is
  `UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_TIME`) that drop and come off the rod
  (`fishing.cpp`). `IsPlayerBotLeftOnGroundItem` keeps them out of the loot
  pass and makes them merchant scrap; a worn 70050 is left alone (206 bots
  wear one). A dead fish is kept only up to `PLAYERBOT_DEAD_FISH_KEEP`, with
  a campfire in the bag. First ten minutes: capes in bags 1 005 to 257,
  symbols 643 to 151, dead fish 4 356 to 1 981 (the fullest bag 167 to 66).
- **A hairstyle is not a costume the block refuses.**
  `apply_costume_hair_allowed` (playerbotify.py) lets `COSTUME_HAIR` past
  `apply_costume_block`'s refusal at the top of `CanEquipNow`; every other
  costume is still refused ("fryzury z itemshopu musza dzialac", hunmar,
  15 September). Compiled, not tried in a client.
- **The quests name this world.** The package's `translate.lua` calls it
  Metin2009 in sixteen quest and quiz lines, and
  `quest/libs/translate/translate.lua` in a test-server greeting; a share
  step `shareify.py` renders (`DOCKERFILE_BRAND_*`) runs
  `sed s/Metin2009/Metin2 SinglePlayer/` over both and fails the build if one
  is left. The result is byte for byte the file l0st3k sent (15 September).
- **Iwakura's sheet 1.1** replaced 1.0 in `data/iwakura_ceny.txt`: lower
  multipliers on several bonuses, new material prices, the chest at 85 000,
  the medal at 180 000, the gold and silver keys, Gourou and a [Lowienie]
  section of dyes. `generate_iwakura_prices.py` needed one alias ("Czerwona
  Farba Do Wlosow" is "Czerw. farba do wlosow" in item_proto) and
  `PLAYERBOT_PRICE_TABLE_VERSION` went to 6 so every counter reprices.
- **Auto Lowy fetches the drops between fights.** A melee fight leaves its
  drops a few hundred units off, the pick-up reached 250, and the next target
  was named within a second, so a player hunting a crowd walked from monster
  to monster and picked up nothing ("autolowy nie podnosza itemkow",
  NerrVoVy, 15 September). `LOOT_PICK_DISTANCE` is 450 (the server allows
  600), and between fights an item within `LOOT_FIRST_DISTANCE` (900) is
  fetched before a target out of reach is chased; a fight in reach still
  comes first. Tested against the stubs, not in a client.
- **A switch says what it is set to.** Auto Lowy's pick-up switches were
  `ui.ToggleButton`s, pressed meaning "takes", and a pressed button reads as
  one switched off: the operator's own `autohunt_Tieru.cfg` came back from the
  2.0.8 client with `pickup=0` and every kind at 0, and the character picked
  nothing up ("dalej postac nic nie podnosi", Tieru, 15 September) - with
  `LootMask` at 0 the server was never asked. One click on Podnos meant to
  switch it on switched the whole pick-up off. They are plain buttons reading
  `Podnos: tak` or `Bron: nie` now, like Metiny, Wstawaj and Wracaj, the start
  says in the chat when the pick-up is off, and a settings file with no
  `config_version` - what the toggle window wrote - gets the pick-up and every
  kind back once (`ConfigFromText`). Tested against the stubs on Python 2.7
  and 3; in the operator's client it did not help - the character still
  picked nothing up whatever the switches said (15 September, 23:10), and
  2.0.53 shipped saying so in its notes. The cause was the frame, not the
  switches: the client counts positions from its map's corner - its network
  stream takes the map's base off every position it receives, and the
  minimap's "334, 857" in Bokjung sits on a base of 102400, 204800 - while
  `AutoHuntLoot` carried the item's world `GetX`/`GetY`, so to `LootDistance`
  every item lay a map's base away and the walk went for a point off the map;
  the start point went the other way and the server threw it out for being
  more than 10 000 from the character, so the range was always counted round
  the character. Since 2.0.54 both go as offsets from the character
  (`apply_auto_hunt_offsets` in playerbotify.py, `AnchorOffset` in the
  client). Any client script that trades positions with the server has this
  frame to convert. Compiled and tested against the stubs; the operator's
  word the next morning (16 September) was "od ostatniej aktualizacji
  autolowy dzialaja pieknie".
- **Client 2.0.9's locale pack is the one ĹŌŞƬĒĶ sent.** That pack.zip (15
  September) replaces the character-select background,
  `locale/common/ui/select.jpg`, and nothing else: 762 files in the locale
  pack before and after, one of them different. The package ships that
  `pack/locale.{index,data}` unchanged, so the next locale repack
  (`rulesify.py`, `client-locale`) has to start from the published 2.0.9 pack
  or it puts the old background back.
- **The offline stand's service visit put a stack up whole.**
  `BotOfflinePrepareLine` cut a line only for a hoard, a key and the Moonlight
  chests; everything else - the twenty Blessing Scrolls of jaksiezabic's
  screenshot, 4.7 million for one line - went up as the stack it was, while
  the classic split (`SplitPlayerBotStallSingles`) only ever ran for the first
  opening. A safe refine scroll is a line of `PLAYERBOT_SHOP_SCROLL_LINE_UNITS`
  (five) now, `PLAYERBOT_SHOP_SCROLL_LINES` of them a counter, in
  `GetPlayerBotStallLineUnits` for both paths, and `BotOfflineUnwantedLine`
  takes a bigger scroll line home at the next visit to be cut. Any other kind
  a player buys a few of at a time and the bots hold in stacks wants the same
  two lines - the offline path is the one every mt2009 stand goes through.
- **A player's private shop opens at fifteen, kills or none.** mt2009's
  `CanOpenShop` asked `PLAYER_STATS_MONSTER_FLAG >= 800`, the public server's
  gate for a "tobolek"; 2.0.12 exempted bots and 2.0.45 GMs, and a new
  character still had eight hundred kills to make before trading (gregoszky,
  14 September). A third playerbotify edit on the same function takes the
  count out for everybody - it sits below the two early returns, so the
  markers of all three hold on a re-run.
- **The Biologist's wait is a runtime library, not a quest.** Every
  `collect_quest_lv*` asks `collect_data.is_wait` (`quest/libs/other/
  collect_data.lua`, `dofile`d by questlib at start), so the day between two
  hand-ins is one function in one file the image carries: a shareify step
  makes `is_research_in_progress` answer false (`do return false end` - Lua
  wants `return` last in a block), which also leaves the Researcher's Elixir
  unconsumed. The file is CRLF, so the sed anchors an optional carriage
  return. The bots' own hand-in never kept the wait; this is for players
  (namiot_, 15 September). No quest recompiles for it.
- **A bot's title is the player's choice, in the client.** The game options
  carry a "Tytuly botow" row (Osobowosc / Klasyczne) since client 2.0.11:
  `playerbot_status_tail.py` keeps `playerbot_titles.cfg` beside the client
  (systemSetting has no key to lend), `show_title` attaches nothing while it
  is off and the keeper forgets what it held. textTail has `AttachTitle` and
  no detach - checked in the binary - so a title already drawn stays until the
  bot's next alignment change, a kill away. `clientrootify.py` renders
  `uigameoption.py` and `uiscript/gameoptiondialog.py` from the stock root
  (its first file in a subdirectory; the strings are CP1250 escapes so the
  scripts stay ASCII, and the dialog grows by a row). Asked for by NerrVoVy
  (15 September).
- **Seban's 1.54.1 carries his backups and an unused icon set.**
  `static/bak-*`, `static/maps/bak-*`, `templates/bak-*`, `icons.bak-*`,
  `*.orig`, `*.before-*` and `static/icons_new` (1 915 files nothing reads)
  travel in his zip; `scratchpad/merge_seban1541.py` is the shape of the
  merge (three-way against his 1.48.0, our guards re-asserted). His
  `updater/` is a host-side systemd service for a VPS and nothing in our
  images runs it; the panel only shows its state. His `web_admin.quest` has
  a `NOTICE` command ours lacks, gated off in public builds.
- **A stack's ceiling is the item's, and two of the fragments' loops were
  measured against the wrong one.** mt2009 keeps `dwMaxStack` per proto -
  twenty for Medal Konny (50050) and the Blessing Scroll, a thousand for
  arrows, two hundred for most - and `CItem::SetCount` clamps to it silently.
  `MergePlayerBotStacks` compared with `PLAYERBOT_STACK_MAX` (200), so ten full
  stacks of twenty medals were "merged" four at a time every five seconds for
  ever: MoveItem into a full stack moves nothing and answers true, the pass
  counted four merges, used its budget and came straight back - 110 bots and
  14 321 `PLAYERBOT_BAG: merged` lines in ten minutes, every medal dropper
  among them. `PlayerBotMaxStack` (economy.h) is the item's own limit on both
  engines, a merge counts only when the destination grew, and the safebox
  top-up and merge measure with it too: a pour of twenty into a box stack of
  twenty measured against two hundred would have removed the bag stack and
  kept nothing of it. Medal Konny came only in stacks of twenty for the same
  reason: that is the engine's pickup, not a split.
- **A dropper with its stock in the bag was sent back for more.** The
  dungeon's exit rule sends a medal dropper out at
  `PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK` medals (`DecideMonkeyExit`,
  MEDAL_READY) and `ShouldPlayerBotPursueHorseExpedition` sent it straight
  back, because its dropper branch asked only for a free cell: 703 of 854
  dungeon visits under ten seconds in an hour on the test world, one bot
  every fifty seconds, `horse_to_monkey_medium` and `monkey_medal_found_direct`
  by turns. The pursue rule asks for the stock too; a full dropper hunts on
  its village's ground until a counter line sells (they sell slowly: 1 704
  medals stood on 42 counters in lines of twenty). Two rules that decide one
  errand from two sides have to read the same number.
- **The travel status named a frontier the traveller was refused.** The
  `BOT_ACTION_TRAVEL` fallback printed `GetPlayerBotFrontierMapForLevel`, so
  eleven medal droppers of thirty-three read "Ide na Pustynie Yongbi (cel:
  rozwoj konia)" in Bokjung while riding to the Monkey Dungeon - the same
  three bots the `ShouldPlayerBotLeaveForFrontier` comment describes, seen
  again from the panel. The status asks that rule first now.
- **The basic shot refused a bow whose quiver had just emptied.**
  `ExecutePlayerBotBasicAttack` asked `GetArrowAndBow` and gave up; only the
  skill path nocked arrows from the bag. Seven of 127 archers of the test
  world stood with a bow, nothing in the arrow slot and a thousand arrows in
  the bag (16 September). It nocks first now. No bot shoots without arrows -
  the engine's `GetArrowAndBow` is the rule for a bot as for a player, and
  the one archer with none anywhere was walking to the weapon merchant; a
  report of "shooting without arrows" is worth checking against the arrow
  slot (`EQUIPMENT` position 9), which a bag view does not show.
- **A keep is a count of scrolls, not of cells before this one.** The
  scroll rule in `ScorePlayerBotShopStock` held a stack back while fewer
  than `PLAYERBOT_REFINE_SCROLL_KEEP` scrolls lay in the cells *ahead* of
  it - written for the classic stall, which split singles off first so the
  base stack always had cut lines behind it. The offline stand's service
  visit splits nothing before it scores, so a bot's one stack was kept whole
  whatever it held: 294 bots with 1 405 scrolls, 289 of them in a single
  stack, 116 of those over the keep, and 5 on the counters of the whole
  world ("A bodzi jak nie bylo tak nie ma", 16 September - and the 2.0.55
  cut into fives could never run, because the scorer never handed it a
  stack). A stack is goods when it and the scrolls ahead of it exceed the
  keep (`GetPlayerBotRefineScrollKeep`, three, one for a resource trader,
  none without scroll work); `GetPlayerBotStallBaseKeep` leaves the keep in
  the base stack, and both cuts - `BotOfflinePrepareLine` and
  `SplitPlayerBotStallSingles` - take what is over it up to the line, so a
  stack of five with a keep of three is a line of two, never a line of five
  that leaves the anvil nothing. And the base keep has to ask the scroll rule
  *before* the material rule: the Blessing Scroll is what recipe 501 consumes,
  so `IsPlayerBotTradeableMaterial` is true of it and
  `GetPlayerBotStallBaseKeep` answered with the anvil's reserve - twice the
  recipe count of every piece under scroll work, larger than most stacks -
  and the first deploy of the count rule still cut nothing: a bot with
  twenty-six scrolls put a marble up instead. Any rule that keeps "the first
  N" of a kind has to be re-read for the path that never splits, and any
  item that is two things at once for the one that answers first.
- **A service visit could never cut a line, because the board was already
  open.** `MoveItem` asks `CanHandleItem`, which is false while `IsBusy` -
  looking at a shop, its safebox open, edit mode - and the visit set all of
  that before the add loop asked `BotOfflinePrepareLine` to cut. Every cut
  since 2.0.26 failed silently and the loop moved to the next item: the diag
  lines of 16 September read score 800, a slot, a keep of three and
  `lineCell=-1` for every scroll, and `cut a line` had never once been
  logged that morning for a hoard or a chest either - what the notes above
  call "packs cut here" were whole stacks going up. `BotOfflinePrepareVisitLine`
  collects, chooses and cuts before `SetLookingShopOwner(true)`, keeps the
  line by item id and cell in `playerbot_offline::State`, and the add loop
  takes it first; a line left behind by a visit that ended early is a split
  stack the merge pass pours back. The scroll cut measures its keep over the
  whole kind (`CountSpecifyItem`), so the cut line itself goes up whole.
  Anything the engine refuses while "busy" - a move, an equip, a use - has to
  happen before the board opens, never between its packets.
- **The stable keeper's waits live in four stock quests, not in ours.**
  `pony_levelup` (ours since 2.0.12) has no wait for levels 1-10, but the
  package's `quest/systems/horse/` still made a player wait: `pony_buy` and
  the two `horse_upgrade` quests set `make_time` twelve hours ahead in their
  `report` state and left a `wait` state for the next login, and
  `horse_levelup` kept twenty-one hours (`next_time`) between two trainings
  of levels 11-19 ("Zniesienie czasu oczekiwania na 1lv konia", greess;
  "niezaleznie od poziomu konia", Tieru, 16 September). The four ship as our
  copies in `linux-port-mt2009/docker/game/quest/` - report goes straight to
  `buy`, the training's clock is an `elseif false` like pony_levelup's - and
  compile in the Dockerfile loop beside pony_levelup, their objects landing
  over the stock ones. Two things the first build taught: the loop's
  `[ -f ... ] || continue` skips a quest the build context lacks without a
  word, so a deploy that copies the Dockerfile and not the new files builds
  the old objects (check `object/20349/chat/pony_buy.report.1.script` for
  `setstate ( "buy" )`); and the `gameforge.horse_levelup.*` names in qc's
  output for pony_levelup are translate keys, not a second quest of that name.
- **The chest switch is a side file of the panel, not a weights key.** The
  weights file is re-read every five seconds and an unknown key costs the
  core a log line each time, so "Wylacz drop Szkatulek Blasku Ksiezyca" on
  the AI page (`CHEST_OFF`, Tieru, 16 September) writes CHEST 0 and
  CHEST_STONE 0 into the weights file and keeps the sliders' own values in
  `playerbot_chest_switch.tsv` beside it; unticking writes them back. Tried
  on the test world through the page's own form: off wrote the zeros and the
  core reloaded within the five seconds, on put 10 and 300 back.
- **The world's difficulty is three event flags the migrator writes and one
  Lua file the quests ask.** `M2_DIFFICULTY` (easy, medium, hard, custom with
  `M2_BIOLOGIST_WAIT_HOURS` and `M2_HORSE_WAIT_HOURS`) goes to the migrate
  service; `apply.sh` turns it into `player.quest` rows with dwPID 0 in seconds
  (`m2_biologist_wait`, `m2_horse_buy_wait`, `_upgrade_`, `_train_` 1-10,
  `_train2_` 11-19), written before the seed because the seed may exit early
  on a foreign cohort. The db core loads those at boot (`LoadEventFlag`) and
  pushes them to every game core, which is the package's own idiom for a
  world-wide switch (`beta_server` in the same quests) - so a change is a
  restart, and the panel could read the numbers. `quest/m2_difficulty.lua`,
  copied into `libs/other/` and dofile'd from `_otherModuleLoader.lua` (CRLF,
  stripped first) after `collect_data.lua`, overrides
  `collect_data.is_research_in_progress` and `set_wait_time` - the 2.0.55 sed
  that made the first answer false is gone, that is what "easy" is now - and
  defines `m2_horse_wait(kind)`, a plain global because qc admits only the
  dotted calls it knows. The four horse quests and pony_levelup ask it where
  the package wrote 12, 18 and 21 hours; a wait of zero goes straight to
  `buy`, because the package's `wait` state hands the horse over only at the
  next talk or login. Presets scale the package's numbers: hard is what it
  shipped with, medium a third. The launcher has a button and a text-menu
  entry (`SetDifficulty`, `-Difficulty -BiologistHours -HorseHours`), and
  `Set-DotEnvValue` is the generic .env writer it needed. Tried on the test
  world in hard: the flags arrived at the core (`QUEST eventflag m2_difficulty
  2`), the compiled objects carry the calls, questlib loaded without a Lua
  error; no player dialog was driven. The bots' Biologist and stable keeper
  are the AI's own code and never waited (Drip's "z harda na easy nieeee",
  Tieru, 16 September).
- **A timed event is a file the panel writes and one core acts on.**
  `playerbot_event_rules.h` (pure, unit-tested) reads
  `/opt/m2spool/playerbot_events.tsv` - weekly windows by kind (chest, exp,
  drop, yang), days 1-7, HH:MM-HH:MM past midnight allowed, a percent over
  the world's rate, `#off` rows the panel keeps, and `now` lines the
  "Aktywuj teraz" button writes with an epoch - and `Evaluate` says per kind:
  active, until, next start. `playerbot_events.h` runs it once a second.
  Two things are per core and one is not: the chest gate is local (every
  core's `CreateDropItem` rolls on its own `g_iMoonlightChestPermille`), so
  every core holds the two figures at zero while a chest window is written
  and not open, keeping the sliders' values captured on each weights
  generation (`GetPlayerBotChestWantedPermille`, which the F9 report asks
  through a forward declaration in config.h); the rate flags and the notices
  are the leader's alone - the core hosting Joan, map 21, which is game1
  under both layouts - because `BroadcastNotice` goes to every core by P2P
  and three cores each adding fifty percent to `mob_exp` would compound it.
  The base rate lives in a flag of its own (`m2_event_exp_base`, and
  `_buyer`), so a core restarted inside an event computes the same boosted
  number again, and a rate the operator moved during the event is left where
  they put it. The core answers with `playerbot_events_status.tsv` beside
  `playerbot_status.tsv`; the panel's `/events` page reads the newest of the
  three cores' and shows "active until / next at" with the schedule editor
  (two empty rows after the saved ones, no script) and the activate-now
  forms. Log tag `PLAYERBOT_EVENT`, in the bundle's list.
- **The guild-mark connection logs in twice, and the second was an error
  line.** The client's mark downloader sends `HEADER_CG_MARK_LOGIN` (100)
  once the handshake has put its connection in PHASE_LOGIN;
  `CInputHandshake` answers that header only inside the handshake, so
  `CInputLogin::Analyze` fell to its default branch: "login phase does not
  handle this packet! header 100" on every mark download, 202 in two days on
  sizowski's world and 92 on ours, read by him (and by the Claude he asked)
  as the cause of a login problem it had nothing to do with - the branch
  already returned 0 with `SetPhase(PHASE_CLOSE)` commented out.
  `apply_mark_login_quiet` (playerbotify.py) gives the header a silent case;
  input_login.cpp already shipped staged.
- **A stone is broken together, not claimed.** `IsTargetClaimedByAnotherBot`
  kept every bot off a target another bot had, and that included a Metin: one
  bot on the only stone in sight and the rest walking past ("jak jest jeden
  metek to jeden bije a reszta sie nie dolacza", Kiciamol, 16 September;
  Tieru: "to bug"). For a stone the claim is a headcount now -
  `PLAYERBOT_STONE_MAX_ATTACKERS` on it - and `CCountPlayerBotStoneAttackers`
  tells bots from players. Every bot scores a stone in its band above the
  sweet-spot monster (`PLAYERBOT_STONE_BASE_SCORE`; the hunter's 1.5M is
  untouched) and a stone another bot is already on gets
  `PLAYERBOT_STONE_JOIN_BONUS` on top; `IsPlayerBotStoneJoinable` admits a
  stone up to `PLAYERBOT_STONE_JOIN_LEVEL_DELTA` over the bot while others
  break it, in the collector, in the party's target and in the manager's
  "obsolete stone" check, which would otherwise drop the joined stone on the
  next tick. A stone only a player is hitting is left to the player
  (`PLAYERBOT_STONE_JOIN_PLAYERS`), because the drop goes to whoever dealt the
  most damage. `PLAYERBOT_METIN: joined a stone` says who joined whom.
- **A boss's fall opens the loot window a broken stone gets.** The raid's next
  step after a kill is "boss down, going back to work", and the killer walked
  off with the Umarly Rozpruwacz's casket (50082, a giftbox of level-75
  weapons; nothing in the loot rules refuses it) lying on the snow (Ciapek,
  16 September). `bFightProgressBoss` rides with the fight-progress clock; when
  that monster is dead or gone, `dwStoneBrokenTime` is stamped and
  `HandleLoot` dashes for what lies within `PLAYERBOT_METIN_LOOT_DASH_RANGE`
  for `PLAYERBOT_METIN_LOOT_DASH_TIME`, ahead of the threat scan and the
  wander. The killer owns the drops for the engine's ten seconds; a party
  member's are picked up for the owner, as before.
- **Sztuka Combo and the Leadership books are not ITEM_SKILLBOOK.** 50301-50303
  (Sun Zi, Wu Zi, WeiLiao Zi: Leadership by twenty levels each, 35% a read
  through the class-book branch of `LearnSkillByBook`) and 50304-50306 (Combo
  at 20/70/100 percent, from level 30 for Combo 1 and 50 for Combo 2) are
  ITEM_USE, USE_SPECIAL, with cases of their own in `char_item.cpp`; the
  skill-book pass never saw them and the junk rule's default sold them for a
  thousand yang ("Mistrz. Sztuka Combo" sold to the merchant in a gear
  history, sizowski, 16 September). `IsPlayerBotGeneralSkillBook` and
  `CanPlayerBotReadGeneralSkillBookNow` (consumables.h, the engine's own
  tests) drive `ReadPlayerBotGeneralSkillBook`, asked from the class-book
  pass when no class book is due and waving the engine's day away like it;
  the junk rule never vendors one; the counter lists what the bot cannot
  read or holds beyond `PLAYERBOT_GENERAL_BOOK_KEEP`, at Iwakura's ordinary
  book times two (Leadership) or four (Combo). Combo is worth having: a
  swing's `GetShootMaxTargetCount` is 3 + the Combo level, so Combo 2 hits
  five monsters where Combo 0 hits three.
- **A mod in an engine file survives an update and breaks the next build.**
  The package replaces our files and leaves everything else, so a fork's
  edit in `guild.cpp` or `messenger_manager.cpp` calling a manager method we
  never had (`GetCompanionOwner`, maxziomeknw1, 15 September) fails the
  build after every update, one file at a time. The answer is a search over
  `game/src` for the symbol and the stock file from the full package for
  each hit; a launcher that named the files itself would be the real fix.
  It sprang again on 18 September, the same file and the same call:
  archonek's `messenger_manager.cpp` called `GetCompanionOwner` at line
  190 and every update from 2.0.74 failed there, while the launcher's
  diagnosis said "update channel not published" (the 404 rule of the
  time) and then nothing that named the file. Since 2.0.78
  that file ships in the update package, so the next click puts the stock
  copy back (`guild.cpp` has shipped since 2.0.65), and
  `Get-M2LauncherErrorGuidance` answers `ENGINE_FILE_FROM_MOD` with the file
  the compiler named for any "`CPlayerBotManager` has no member named" line.
  Read the launcher log's compiler lines before any other theory of a failed
  update: `grep -o "[a-z_0-9]*\.cpp:[0-9]*:[0-9]*: error: .*"`.
- **A rule written for bot parties was asked of a player's.** `ManagePlayerBotParty`
  put every party it checked back on `PARTY_EXP_DISTRIBUTION_PARITY`, the
  player's included, on every check ("boty dodane do PT zawsze same zmieniaja
  podzial na rowny nawet gdy to nie one sa liderem", Dearminder, 15 September).
  A human-led party's split is the leader's, so the pass returns before it for
  `IsPlayerBotHumanLedParty`; the three `SetParameter` calls left all stand on
  bot-led parties. Compiled and deployed, not watched: the test world has no
  person in a party. Grep every `SetParameter(PARTY_EXP_DISTRIBUTION` before
  writing a rule about a party's split, the way `->Quit(` is grepped for who
  stays.
- **A purchase every bot wants is a market emptied in an hour.**
  `WantsPlayerBotMoonlightChest` (2.0.53) made every non-trader short of chests
  a buyer, and a thousand buyers took every chest off every counter ("boty
  wykupuja doslownie WSZYSTKIE szkatulki bez opamietania", sizowski, 16
  September). Two brakes now: `PLAYERBOT_CHEST_MARKET_RESERVE` - no bot buys
  while the ledger counts thirty or fewer on the counters of the whole world -
  and `PLAYERBOT_CHEST_BUY_COOLDOWN` per bot (`s_mapPlayerBotChestBoughtAt`,
  stamped by the offline and the classic purchase path both). The ledger is
  refreshed once a minute, so a minute's purchases can still dip under the
  reserve; that is the size of the overshoot, not a hole. Anything else the
  whole population wants off the counters at once needs the same two.
- **A second village is entered at one end, and the bots stayed at that end.**
  "Boty z Shinsoo omijaja gorna czesc Jayang, z Jinno dolna czesc Bakra - moze
  mosty albo granice mapy sa zle okreslone?" (blasty, 16 September). Not the
  map: `tools/analyse_map_bridges.py` on each map's own server_attr (mt2009's,
  in docker with python-lzo) reaches 99.9% of the ground and every spawn group
  of a3 and c3 from the town point under the BLOCK|OBJECT rule. What kept the
  bots off was that Jayang's gate from Yongan opens in its south and Bakra's
  from Pyongmoo in its north, each onto the tigers of 18-20 and the Black Wind
  of 26-30, and a bot chain-kills outward from where it stands: the wander
  pass runs only on a tick nothing was worth attacking, the outgrown-prey rule
  stopped at the first villages, and `PLAYERBOT_GROUND_HUBS_3/43` were twelve
  hubs by pid with `mobLevel` 0 - so the 501-504 ground of 29-36 that fills
  the far half of each map had nobody on it. Measured on m2zip before the
  change: every bot south of y 280 000 on map 43 was passing through ("Ide do
  Biologa", "Ide do Doliny Orkow"), and the map's richest cell,
  (848000,291200) at 78 monsters a look, had 793 looks and not one fight.
  Every M2 hub carries its band now (`tools/generate_wander_hubs.py <map>
  --count 24 --band --spacing 4500`; its first twelve rows for 3 and 43 are
  the 2.0.8 tables to the unit, which says those came from it with `--count
  12`), the M2 branch of `ManagePlayerBotWandering` takes
  `CollectPlayerBotM1HubsForLevel` like the first villages, and `outgrownPrey`
  in `BuildPlayerBotCombatContext` is set on `IsPlayerBotM2Map` too. Bokjung's
  twelve hand-placed hubs went with it: measured against regen.txt
  (`scratchpad/measure_hub_bands.py` is the shape), three stood two to four
  kilometres from the nearest spawn rectangle and two beside fewer than
  fifteen points. The bands come out as 27, 29-30 and 35 - the tigers are
  nowhere a majority, so a bot under 26 takes the nearest band and kills them
  on the way - and Jayang has one hub of band 27, so
  `CollectPlayerBotM2HubsForLevel` fills the choice set up to
  `PLAYERBOT_M2_HUB_CHOICES_MIN` from the nearest bands. Measure it as bots
  in `BOT_ACTION_FIGHT` per 10 000-unit band of y in `playerbot_status.tsv`
  on maps 3 and 43 **by level**, never as the count of bots there. Fifteen
  minutes after the deploy on m2zip: every fighter under 26 on both maps
  stood on the low ground by its gate (Jayang 850-870k, Bakra 220-270k),
  the 33-35s on the middle ground, and the far halves held 66 and 102 bots
  of 36+ standing between errands and not fighting - on a unified world a
  bot past the M2 ceiling is SERVICE_ONLY, and the band choice now parks
  them on the band-35 hubs instead of anywhere. The far-half hunting
  itself needs a split world, where a Shinsoo or Jinno bot of 36+ has
  nowhere else to go; m2zip cannot show it.
- **The cohort's window is the operator's, and so is the second cohort.**
  `SpawnRegistered` drained its queue over `PLAYERBOT_SPAWN_WINDOW` (a minute)
  and that was the whole plan: a player who started two thousand at once had
  the square "jak w szpitalu" (Ciapek, 16 September; "nie trzeba ich pchac
  2k na serwer", seban). `PLAYERBOT_SPAWN_WINDOW_MINUTES`,
  `PLAYERBOT_LATE_JOINERS` and `PLAYERBOT_LATE_JOIN_HOURS` come from `.env`
  through both compose files into the bootstrap in `input_db.cpp`
  (playerbotify.py): `CPlayerBotManager::SetSpawnWindow` stretches the
  cohort's batches, `ScheduleLateJoiners` takes the next identities of each
  kingdom after the scheduled ones (`SplitPopulation` over what the cohort
  leaves each kingdom) and gives each its moment spread evenly over the
  hours, `SpawnLateJoiners` spawns them from the tick. A late joiner is in
  `m_setScheduledBots` only from its moment on, so `TopUpMissingBots` neither
  counts nor hurries it; a kingdom whose cohort share is zero gets no late
  joiners either. The launcher's LICZBA BOTOW asks for the three beside the
  count (text menu: Enter keeps the value; GUI: three boxes under the
  slider, passed as `-SpawnMinutes -LateJoiners -LateHours`).
- **"Boty graja jak zywi ludzie" is a switch, off, and a rest is a ban the
  top-up honours.** The `LIFE` key of the weights file (the classic panel's
  AI page, experimental, default 0; `IsPlayerBotLifeScheduleEnabled`) runs
  `CPlayerBotManager::ManageLifeSchedule` once a minute: every live bot gets a
  session end (`PLAYERBOT_LIFE_SESSION_MIN..MAX`, the first after a start from
  `PLAYERBOT_LIFE_FIRST_SESSION_MIN_MS` so the log-outs spread), at its end it
  is `Despawn`ed and put in `m_mapLifeRestEnd` for `PLAYERBOT_LIFE_REST_MIN..MAX`,
  and `IsRestingBot` keeps it out of `SpawnPendingBatch`, `TopUpMissingBots`
  and `SpawnLateJoiners` exactly as the ban set does. When the rest ends the
  pid goes to `m_setLifeReturning` (a whole session next time, not a first
  one) and the top-up is asked at once. A bot in a player's party is
  postponed `PLAYERBOT_LIFE_POSTPONE_MS`; its offline shop stands on, being
  an entity of its own. Switching off clears both maps and the top-up fills
  the world within its window. Measure it as `PLAYERBOT_LIFE: census
  online= resting=` every ten minutes, never as the panel's bot count, which
  is meant to fall. Tieru's brief (16 September): "graja np kilka godzin
  dziennie, wyloguja sie i graja znow po odpoczynku", experimental, off for
  everybody. The whole cycle was run on m2zip through the panel's own form:
  on at 14:40, the first log-outs at 15:13 (fourteen by 15:15, rests of 184
  to 500 minutes), not one "topping up" line while they rested, off at 15:15
  and "schedule off, 14 resting come back" with `topping up ... missing=14`
  on the next minute, all fourteen back. A restart forgets the rest map, so
  everybody returns at a start whatever the switch says. And the spawn plan
  the same afternoon: a window of three minutes took the 1099 in 180 s
  against 64, and sixty late joiners over an hour came one every minute
  (`late joiner pid= ... left=`), the top-up counting each from its moment.
- **A stone's band is sixteen levels either way, and a tower stone is a
  floor's objective, not a Metin.** `PLAYERBOT_STONE_JOIN_LEVEL_DELTA` was
  thirty from 2.0.57 (a bot joined a stone thirty levels over itself) and the
  outgrown side ten; both are sixteen now, in `IsPlayerBotMetinWorthFighting`
  and `IsPlayerBotStoneJoinable` ("przedzial postaci bijacych metina niech
  wynosi maksymalnie 16 poziomow", Tieru, 16 September) - the drop curve is
  1% at fifteen over, so past that nothing comes out of a stone; alone a bot
  still starts one only up to nine over itself. The Demon Tower's 8015-8019
  are refused everywhere for a bot on its own (breaking one warps every PC
  on the map), and `IsPlayerBotDungeonStoneObjective` - the bot in a
  player's party with the player on the same map, `IsPlayerBotClimbingWithPlayer`
  in `playerbot_movement.h` - lifts that refusal, the join rule, the sweep's
  and the splash's avoidance, with no band at all: "takie metiny sie zbija by
  zaliczyc kolejne pietra". Compiled and read, not watched: the test world
  has no player climbing with bots.
- **A quest's wait state remembers the wait it was entered under.** The
  difficulty flags (2.0.57) reach `m2_horse_wait` at once, but a character
  already in `pony_buy`'s `wait` carries the `make_time` set on entry - twelve
  hours under hard - and the state only tested `get_time() >= make_time`, so
  switching to easy changed nothing for anybody already waiting ("dalej
  trzeba czekac", Hiob, 16 September, with the wait state's own dialog in
  the screenshot), while 2.0.57's changelog had promised the opposite. The
  five horse quests cap the stored deadline at `now + m2_horse_wait(kind)`
  on login and on every talk (`make_time` in the three wait states,
  `next_time` at the top of the two training talk handlers), so the wait
  can never be longer than the setting says now and zero means at once.
  `reload q` recompiles nothing here and reads no flag: the flags are the
  migrator's, at a start. `horse_levelup.quest` is LF where the other four
  are CRLF - a patch has to take each file's own ending, and `grep -c
  $'\r$'` under the Bash tool's sh does not say which is which. Compiled
  in the image (the objects carry `m2_horse_wait`), not driven in a client.

- **The Biologist's herb row sent a bot of seventy-seven away from its horse
  trial.** The frontier draw put the desert first for a battle-horse candidate
  since 2.0.60, but `NeedsPlayerBotM1OnlyServices` runs before the frontier
  branch and the Gango Root's monster stands in the first village, so 35 bots
  on Jayang alone read "Zdobywam konia bojowego na pustyni (0/100)" with the
  Biologist as their goal, riding to the M1 gate and back between town visits
  (Tieru, 16 September, screenshot). `GetPlayerBotBiologistHuntMob` answers
  nothing while a horse trial is open - the row waits, the hand-in still
  walks - and every travel rule that asks it follows. Two rules that both
  claim "hunt here" want an order, and the status line must name the one the
  travel takes.
- **A rule that sends "every bot with X" somewhere sends them all at once.**
  2.0.60 made the herb rows a destination for every bot that had outgrown
  them - which was nearly every bot past forty, because those rows had
  been skipped for weeks - and half the world left for the first villages
  within the hour: M2 -> M1 crossings 300/h -> 2 766/h on the test world,
  580 of 1 099 bots in M1, and the players filmed the crowd riding into
  the gates ("masa botow na koniach wchodzacych do portalu"; "boty 40-50+
  expia w m1"). `PlayerBotMayTakeHerbErrand` (playerbot_missions.h) gives
  `PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE` of the live population a place
  on the errand at a time; a bot without one steps over the outgrown herb
  row in every pass of `GetActivePlayerBotBiologistMission` - in the first
  village too, or the 483 bots already drawn there stayed for all six
  rows (measured after the first build). Same shape as the M3 crowd share and the
  chest market reserve: an errand that names one map takes a share of the
  population, never the population.
- **Pierscien Teleportacji: the 2.0.62 diagnosis read the wrong flag.** 70058
  is an ITEM_QUEST whose proto carries flag 8192, and 2.0.62 took that for
  `ITEM_FLAG_APPLICABLE` (the `ENABLE_QUEST_DND_EVENT` branch of `UseItemEx`,
  "drop it onto another item", returns before the quest is asked). On this
  engine `ITEM_FLAG_APPLICABLE` is `1 << 14` (`common/item_length.h`) and 8192
  is `ITEM_FLAG_LOG`, so that branch never ran and `apply.sh` cleared a
  harmless flag; the ring was still "nic nie robi" on 2.0.64 (NerrVoVy, 17
  September) and his bundle had no line about it. What the use goes through:
  `CHARACTER::UseItem` (CanHandleItem, CanUsedBy, a suspended quest state -
  chat only), `UseItemEx` (the level limit, then `case ITEM_QUEST` ->
  `CQuestManager::UseItem` -> `m_mapNPC[70058].OnUseItem` ->
  `NPC::HandleEvent`, which refuses a `pc.IsRunning()` state silently off the
  test server), and the compiled `object/70058/use/teleport_ring.start` is
  registered at boot ("QUEST loading ..." on m2zip). Since 2.0.65 every plain
  use of an ITEM_QUEST logs `QUEST_ITEM: use ... flag= running= quest=`
  (playerbotify `apply_quest_item_use_log`), a suspended state is told to the
  player, the ring quest logs `QUEST_ITEM: teleport_ring runs`, and the bundle
  keeps the tag - the next report can be read instead of guessed. Read the
  enum before naming a flag by its number. And the use path above is not the
  ring's: the ring is in special item group 10031, so its use is
  `CQuestManager::SIGUse(10031)` and no handler under 70058 is ever asked -
  found by Mur4s in 2.0.95, after both diagnoses here.
- **A player's guild invitation reaches the bot on the same call.**
  `CGuild::Invite` sends the invitee a packet a bot's descriptor never answers,
  so the invitation event expired in silence. Unlike the party invitation, the
  acceptance is the guild's own method with the invitee as its argument
  (`InviteAccept`), so `apply_playerbot_guild_invites` (playerbotify.py) has the
  engine hand the invitation to `CPlayerBotManager::OnGuildInvite` right after
  the packet, and `AcceptPlayerBotGuildInvite` (playerbot_guild.h) accepts it
  while the event is alive - any bot with no guild, a dropper too. A bot in a
  player's guild offers its hour's share of experience at the ordinary rate
  and is otherwise left alone (`ManagePlayerBotGuild` returns before the
  dropper rule for a guild whose master is not a bot). `guild.cpp` ships
  staged in `server-update-files.mt2009.txt` for it.
- **Iwakura's tier list is a nudge on top of the measured scores, not a
  ranking of its own.** `data/iwakura_tiery.txt` (16 September) rates every
  family of bracelets, earrings, necklaces, boots and weapons and every bonus
  line 1..6 for PvE and PvP, with "+1 dla Wojownika" notes;
  `tools/generate_iwakura_tiers.py` renders it into `playerbot_item_tiers.h`
  (161 families, 45 bonuses; unbound names abort like the price generator,
  and it reuses the price generator's aliases). The PvE column is used:
  `GetPlayerBotEquipmentScore` moves the whole score
  `PLAYERBOT_TIER_SCORE_PERCENT` a step from the neutral 3 and scales every
  line by `PLAYERBOT_BONUS_TIER_PERCENT`, `ScorePlayerBotBonusLine` scales the
  reroll weights the same way, and `IsPlayerBotHigherTierSpare` counts a bag
  piece of a better tier as the spare the blacksmith works on - his own
  instruction: refine and bonus it first, do not swap Miedziane Kolczyki +9
  for Ebonitowe +1 because a table says so. The PvP column is rendered and
  unused until the second set exists. Body armour, helmets and shields are not
  in his list on purpose (judged by level and lines).
- **A soul stone is seated by Iwakura's KD tiers, to the letter.** His
  sheet of 19 September (`data/iwakura_tiery.txt`, the [TIERY KD] section)
  rates each kind of stone at +3 and +4 for PvE and PvP, bans +0, +1 and +2 in
  weapons and armour, and calls the class stones (Wojownika, Sury, Ninja,
  Szamana) PvP-only and useless for PvE. `generate_iwakura_tiers.py` binds each
  row to its vnum - 28000 + grade * 100 + kind, 30-37 the weapon kinds and 38-43
  the armour kinds, checked against item_proto's name and type - and renders
  `TPlayerBotSoulStoneTier` into `playerbot_item_tiers.h`. A stone is seated
  when its kind's PvE tier is at least `PLAYERBOT_SOUL_STONE_MIN_PVE_TIER` (3),
  into a piece at +6 or more, and a piece at +8 or more waits for a +4; the
  market buys only +3 and +4; the equipment score counts the stones a piece
  carries by their tier (`ScorePlayerBotSeatedSoulStones`), so a piece with
  good stones is not swapped for a bare one. The operator's one exception, the
  same evening ("te kamienie mozna wkladac jak sie dropnie do slabych itemow do
  21 levela jesli sa to itemy co najwyzej +6"): a dropped +0..+2 goes into a
  piece of level 21 or less at +6 or less (`IsPlayerBotWeakSoulStoneGear`) if
  its kind rates 3 or more at +3 or +4 - Witalnosci's item name carries no
  "Duszy", which is why the generator matches the stem. The rest of the sheet
  only lost its "(Lvl N)" labels and spelled Miecz Zadlowy (an alias).
- **Cennik 1.2 has two rows for one item, and the generator now says which it
  skips.** "Waleczna Dusza Zaprzys" (1.1) and "Waleczna dusza" (1.2) both bind
  to 30356, "Wyuszone Oczy" is a typo beside "Wysuszone Oczy"; `SKIPPED_ROWS`
  names them with the reason, because every other unbound name still aborts.
  1.2 also fixed his spellings (Mikstur, wachlarze) - both spellings are
  understood - and added [Szkatulki], [Ulepszanie], [Pasywne], [Kon] and
  [Lowienie], all goods sections now. `PLAYERBOT_PRICE_TABLE_VERSION` 7.
- **An offline stand's line that does not sell comes down on a clock.** The
  service visit recomputed a line's price from market policy and never from
  how long it had stood; the classic stall's per-stand markdown had no offline
  twin. The reprice step takes `PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT` off
  for every `PLAYERBOT_OFFLINE_UNSOLD_STEP_MS` the line has been listed
  (`o.listed`, clocked from the first visit that sees a line the core does not
  remember), to `PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL` and never under
  `GetPlayerBotRefineInvestment` (Tieru, 16 September). `PLAYERBOT_OFFLINE:
  marked down` is the line.
- **Three kingdoms' wars on one clock is ninety quiet minutes.** The first
  wars all began thirty minutes after the start and ended together, so the
  operator who came to watch one an hour later found none. The first war is
  `PLAYERBOT_GUILD_WAR_KINGDOM_STAGGER` later for each kingdom after Shinsoo
  and the interval after a war is ninety minutes (a two-hour period), so a war
  stands somewhere for ninety minutes of every two hours; a notice at the
  declaration names the pair and the map a minute before the blows, and the
  guild report carries `next_war_in_s` (0 = under way, -1 = none) which the
  panel's guilds page turns into "Nastepna wojna gildii botow" per kingdom.
- **The client's title switch shows on the next login, and the client says
  so.** textTail has no detach, so a switch in the options changes what is
  drawn only once a title is attached again; `__OnClickBotTitleButton`
  (clientrootify.py, client 2.0.12) writes one chat line saying it.

- **A bot's WarpSet is its own AI's move (2.0.62).** `CHARACTER::WarpSet`
  takes the character off its sectree and waits for the client to reconnect
  to the target map's core; a bot has no client, so a dungeon's `JumpAll`,
  `ExitAll`, a quest's `pc.warp` and a GM's `/warp` left it off the map
  until the sectree rescue put it back at its map's start (the nine bots at
  660000 on 14 September). `apply_bot_warpset` (playerbotify.py) sends a
  bot descriptor's WarpSet to `CPlayerBotManager::WarpBot`: the map index
  from `SECTREE_MANAGER::GetMapIndex(x, y)` (a private index keeps its own
  number once it is a child of that map), refused when this core does not
  host it, `TransitionPlayerBotMap` otherwise, and the dungeon membership
  `Entergame` would give a reconnecting player (`SetDungeon`, which is what
  `pc.in_dungeon()` and `d.*` read - `Show` never sets it). `SetDungeon`'s
  own rule stands: a character joining a dungeon past level 0 whose
  `dungeon_return.dungeon_level` does not match is warped home, which for a
  bot is now a real warp. The navigation grid is the base map's for an
  instance (`CPlayerBotNavigation::instance` and `Init` divide by ten
  thousand): same attributes, no grid per copy. Bots saved inside an
  instance are moved home by apply.sh at the next start as any map not on
  its list.
- **The Demon Tower is the package's quest, driven from outside it.**
  `deviltower_zone.quest` (pre_qc in the image, `/opt/metin2/share/locale/
  poland/quest/pre_qc/`) is the whole mechanism: the ground floor's Metin
  of Toughness (8015, regen cell 195,690, five to seven minutes to respawn)
  breaks -> six seconds -> `d.new_jump_all(66, ...)` takes every PC on the
  killer's map into a new instance at level 0; `d.get_level()` + 2 is the
  floor. Floor 2 clears by `set_warp_at_eliminate` (its first argument is a
  delay in seconds, not a count); 3 the Demon King 1091 then everything;
  4 the Metin of the Devil 8016, then seven Metins of the Fall 8017 of which
  six are purged at half health and one has to die, fifteen minutes; 5 the
  Opening Stone 50084 dropped every fifty kills and given (`CHARACTER::GiveItem`,
  the quest's `take`) to the five Ancient Seals 20073, twenty minutes; 6 the
  Elite Demon King 1092, then one of three smiths 20074-20076 whose "go on"
  is `select` in a dialog and needs level 75 - a bot cannot press it, so a
  bot of 75 does what `devil_jump_7` does (`Purge`, `ClearRegen`, four 8018 at
  the quest's cells, `AdvanceLevel`, `JumpAll`) and without one the run ends
  there, as it would for players; 7 the four Metins of Death 8018, then the
  Metin of Murder 8019 (one, nine seconds to respawn) dropping the Unknown
  Old Chest 30300, used for a one-in-ten Map of the Tower 30302, used for the
  jump; 8 the Zin-Bong-In Key 30304 (one kill in fifty of the Immortal
  Ghost 1040, four in five of those the fake 30303) given to Sa-Soe 20366;
  9 the Dead Reaper 1093, then `d.exit_all` a minute later. A jump inside
  the instance is a `Show` on the same map, so the AI's route and target
  are stale after it - the fragment drops them on a floor change. And the
  fifth floor was broken on this package: the quest counted `1062.kill`
  while `deviltower5_regen.txt` resolves (through group_group 1051-1053 and
  the global group.txt) to 1002-1004 and 1031-1034, never 1062. The shipped
  copy in `linux-port-mt2009/docker/game/quest/` counts those, compiled in
  the Dockerfile loop like the horse quests. `tools`-style check before
  trusting a regen: resolve `r`/`ra` lines through both group files
  (the fields are `<idx> "<name>" <mob>` in group.txt and `<idx> <group>
  <prob>` in group_group.txt).
- **Inside the tower the pass owns the tick, after the loot.** The hook
  sits after `HandleLoot` so the keys are picked up (`IsPlayerBotDemonTowerKey`
  passes the choosy and dropper filters; the fake key is left on the ground
  and is scrap), and claims everything below it: the target collector never
  runs there, so the fight is the war's duel-shaped one against the nearest
  objective of a whole-map scan (`ScanPlayerBotTowerMap`, once per
  PLAYERBOT_TOWER_SCAN_INTERVAL per map - the floors are far wider than
  `PLAYERBOT_SEARCH_RANGE`), stones ahead of monsters on the fourth and
  seventh floors, and what keeps a bot alive is called from the pass
  (`HandlePostDeathRecovery`, the potions, the recovery start), because the
  tick's own copies sit below the hook. The inactivity watchdog counts a bot
  in an instance, a raider and a summoned bot as legitimately still. The
  value policy's `activeQuestTarget` is also set for the tower
  (`IsPlayerBotDemonTowerTarget`) so the collector agrees where it does run
  - climbing with a player - and `IsPlayerBotDungeonStoneObjective` lifts the
  8015-8019 refusals for a raider as for a bot in a player's party.
- **On a floor the pack fights as one, or it dies one at a time.** The
  first runs on m2zip (16 September) took floors 2-6 in 208, 253, 107, 177
  and 104 seconds - sixteen bots, five Opening Stones to the seals, the
  smith passed by a bot of 75 three seconds after the Elite Demon King fell
  - and stalled on the seventh: 214 demons of 72-73, each bot on the
  nearest one to itself, 253 deaths and 244 revivals in eight minutes, not
  one kill in the last six. The revival is `restart_here` at twenty percent
  in the middle of the pack that killed the bot. So the objective is chosen
  from the pack's centroid (`TPlayerBotTowerScan::packX/Y`, the live bots
  on the map), everybody walks at the same demon, a straggler with nothing
  within PLAYERBOT_TOWER_PACK_FIGHT_RANGE of itself walks back to the pack
  first, and a floor's stones wait until PLAYERBOT_TOWER_STONE_CLEAR_LIMIT
  monsters or fewer stand - the Metin of Murder stood among the two
  hundred and the stone-first rule had walked the pack into them. The
  fourth floor is stones only and keeps stone-first. Also learned there:
  the census waits ten minutes after a start, so a raid called before it
  sorted its members by pid - the sixteen lowest, two of them of
  forty-two; the level stands in for a strength of zero. And three raiders
  of the first run left the instance inside two minutes for
  `offline_shop_service`: the passes above the tower's hook that can move
  a bot - the offline stand's service visit, the market trip, the
  negative-rank rule - ask `IsPlayerBotOnTowerBusiness` now.
- **One raid on a core at a time, because the ground floor is one map.**
  Two guilds gathering on the parter would be jumped into one instance by
  whichever broke the stone first, so `s_PlayerBotTowerRaid` is a single
  record, the pick rotates over the guilds with PLAYERBOT_TOWER_MIN_MEMBERS
  online of PLAYERBOT_TOWER_MIN_LEVEL (those with a bot of 75 first), the
  war picker skips a raiding guild and the raid picker a warring one. The
  members' `dwTowerRaidGuild` is set from the world pass and cleared when
  the raid ends or the bot leaves the instance; a bystander (any bot on the
  parter, or a player's guild bot summoned to its human master standing
  there) has none and is simply on the floor.

- **A reason to go somewhere is a reason to stay there.** The herb rows of
  the Biologist became a trip to the first village (`NeedsPlayerBotM1OnlyServices`,
  asked only from outside one), and the M1 branch of the world travel had no
  reason to keep a bot of forty for a level-15 monster: "level_to_m2" out,
  "m1_only_service" back, and since both gates' arrival points stand beside
  the return gate the round trip was four seconds (Greess, 16 September,
  TAKAMURU1's Logi.txt, "nie przechodza przez teleporty").
  `PlayerBotHuntsVillageHerbs` holds the bot in the M1 branch exactly as the
  errand that brought it. Measured the same evening: the Teleporter is used
  from every first and second village of all three kingdoms (M1 -> Orc
  Valley 361/377/252 in 45 minutes, M2 -> Orc Valley 103/54/67), so a report
  of "bots not passing a teleport" is a loop or a hold, never the warp.
- **A door and an exit that ask two questions make a revolving door.**
  `ShouldPlayerBotVisitM3` answered for the M3 dropper by level alone - the
  level-30 weapon it holds is what it farms for - and the M3 branch of the
  world travel sent any bot holding one home as "m3_weapon_found": four
  droppers of seban latino's split world (two a kingdom) crossed M2 <-> M3
  every five seconds, 62-65 round trips each in six minutes, and the
  Teleporter's arrival on the guild map stands beside the return gate.
  `IsPlayerBotM3DropperOnFarm` is the one answer both ask. The same shape as
  the Joan <-> Bokjung loop above, and the measurement is the same: pairs of
  `transitioned` lines for one pid under ten seconds apart, by map pair.
- **A trial is open only where its map is.** `IsPlayerBotOnBattleHorseTrial`
  asked the level, the horse and the kills and never the core, so on a split
  world 58 Shinsoo and 42 Jinno bots read "Zdobywam konia bojowego na pustyni
  (0/100)" in their second villages with the desert on game1 - the frontier
  draw answered the desert and `GetPlayerBotFrontierMapForLevel` filtered it to
  nothing, and since 2.0.61 `GetPlayerBotBiologistHuntMob` yielded to the
  trial, so those bots had neither a frontier nor a Biologist row.
  `IsPlayerBotHorseTrialOpenHere` (the desert for the battle horse, the Demon
  Tower for the military one; `IsPlayerBotMapHostedHere` forward-declared,
  the answer kept per map because the collector asks per candidate) gates
  both trial predicates, so the status, the draw, the targeting and the
  Biologist's yield agree. Anything that names a map a bot must reach has
  to ask whether this core hosts it before it becomes a status line.

- **A tower floor's stone is broken once nothing stands about it, not once
  the floor is clear.** The pack rule (above) kept sixteen bots alive on
  the seventh floor - 43 deaths in ten minutes against 253 in eight, 1200
  to 1400 attacks a minute against 20 - and killed fifteen demons a minute,
  but the floor's regen refilled faster: 140 to 170 alive for ten minutes,
  so "PLAYERBOT_TOWER_STONE_CLEAR_LIMIT or fewer" never came and the Metin
  of Murder, which drops the Unknown Old Chest the floor turns on, was never
  touched. A stone is a candidate when no monster stands within
  PLAYERBOT_TOWER_STONE_CLEAR_RADIUS of it (`CountPlayerBotTowerMonstersNear`),
  so the pack clears the ground round the stone and breaks it; the fourth
  floor, stones only, keeps stone-first. The second run with the pack rule
  measured floors 2-6 at 222, 256, 153, 206 and 156 seconds, the smith
  passed by a bot of 76 at 23:35:43 - the same shape as the first run, which
  says the pack costs no time on the floors it never needed it on. And the
  radius alone was not enough either: ranked from the pack's own centroid
  the objective drifted after whatever demon was nearest, and the Metin of
  Murder stood untouched for nine minutes among the respawns (17 September,
  00:05-00:14, 63 deaths, the pack alive and killing). On a stone floor the
  monsters are ranked from the stone (c532942), so the ground round it is
  what gets cleared and the stone becomes a candidate the moment nothing
  stands there. Compiled and committed, not watched: the test machine was
  shut down before the next raid. The seventh floor's chest and map, the
  eighth's key to Sa-Soe and the ninth's Reaper have never been reached by
  a bot; watch `key used`, `key handed ... npc=20366` and `floor 9` first.

- **A playerbotify edit whose marker is its whole text is inserted again at
  every rewording.** The 0006 group in `item_manager.cpp` (the stone's book
  top-up, the Moonlight chest roll, the voucher roll) had no `marker=`, so
  each time its comments were reworded the staged file no longer contained
  the exact new text, "not applied" was the verdict, and the group went in
  once more at the anchor: every shipped package from at least 2.0.50 to
  2.0.63 rolled the chest and the Dragon Coin voucher **twice** per kill and
  per stone (the book top-up counts what is there and was harmless), and the
  staging of 17 September held three copies. `scratchpad/dedupe_item_manager.py`
  is the shape of the repair, the edit carries a stable marker now, and
  `count_chest_blocks.py` over the release zips is the check. The mirror of
  "A playerbotify edit whose marker is its whole replacement fails the second
  run" above: one shape, two failures.
- **The chest window is a gate on a variable two other things write.**
  `g_iMoonlightChestPermille` was set by the weights parse on every save of the
  file and by `ResetPlayerBotWeights`, and the event gate zeroed it again only
  on its next second - a hole of up to a second per save while a chest window
  was shut ("dropia tez poza konkursem", NerrVoVy, 17 September). The parse
  keeps the sliders' figure (`GetPlayerBotChestConfigPermille`) and writes the
  engine's variable only while `IsPlayerBotChestGateClosed()` (forward-declared
  from `playerbot_events.h`) is false; the gate reads the parsed figure, never
  the variable it may itself have zeroed. And the package's own drop tables
  carry 50011 lines the permilles never touched: `CreateDropItem` (playerbotify)
  drops none of the tables' chests while both figures are zero - a shut window
  or the switch off means no Moonlight chest from anybody.
- **A unique the bot needs cannot be worn into a full pair of slots.**
  `FindEquipCell` answers WEAR_UNIQUE2 when UNIQUE1 is taken and `EquipItem`
  refuses the occupied cell, so `EnsurePlayerBotFishingPass` failed for every
  bot wearing two uniques (the Prophet King's symbol and a ring, mostly) and
  the next ask was an hour away: the whole FISHING output of seban latino's
  1013-bot world was "fishing pass in the bag but not worn yet" and nobody
  fished. It frees a slot the way the unique-slots pass does (what pays
  nothing first, a timed ring last, never IRREMOVABLE), and the cast step asks
  for the pass again on every cast - which also refreshes the hold that keeps
  the equipment pass off it - and ends the session `no_pass` when it cannot be
  worn, instead of 730 "You need to have a fishing pass" refusals in two
  minutes. The Rybak's tackle leg is the "snapped goal outside the arrival
  radius" shape once more: a 16-cell snap against an arrival of 100 left a bot
  119-177 units from the counter for the whole session on Yongan and Pyongmoo
  (`PLAYERBOT_FISHING_TACKLE_ARRIVE`, `_SNAP_CELLS`). And the dry-stand mark
  handed the same stand back: with more anglers than stands the share-a-stand
  fallback took slot `start` whatever it was, so a bot that had just marked
  its stand dry stood on it for the idle timeout ("dry stand ... moving to
  another" once a second on one key, then never_cast). The fallback shares
  the first wet stand, and a stand handed out dry twice ends the session as
  `bank_dry`.
- **A war is fought on foot, in the middle.** `CanPlayerBotEverFightOnHorse`
  kept a battle-horse rider in the saddle on the guild map and the two sides
  rallied 700 units apart (NerrVoVy's video, 17 September); the operator's
  rule is horses dismissed and both sides on the same open ground
  (`PLAYERBOT_GUILD_WAR_RALLY_SPREAD` 0).
- **The armour on the bot's back has the hand weapon's burn rule.**
  `IsPlayerBotWornArmourAtRisk`: worn body armour at a step that can burn
  (`PLAYERBOT_WORN_SCROLL_MAX_PROB`) with no other wearable body armour in the
  bag goes under a scroll or waits, in `CanPlayerBotAttemptRefineItem` and the
  refine pass both ("potrafia spalic jedyna zbroje ... ida farmic bez zbroi",
  THC, 16 September).
- **Respawn speed was already an event flag; it only lacked a world-wide
  name.** `regen_event` scales the next spawn by `fastBossSpawn<map>` /
  `fastMobSpawn<map>` (a percent of the line's delay, 0 = untouched), which is
  what Seban's per-map console writes. playerbotify adds the map-less names as
  the fallback, `web_admin.quest` has a `REGEN` command ("boss,mob") beside
  `RATES`, and the classic panel's /rates page carries the two figures as
  percent of the normal time (10-100), persisted as `player.quest` rows with
  dwPID 0 like the rates (Hiob, 17 September).
- **update.sh appends the .env keys a release adds, and only the safe ones.**
  `.env` is written once; only the Windows launcher's `Add-MissingDotEnvKeys`
  ever added new keys, so a Linux host had no `M2_DIFFICULTY` after 2.0.57
  (GoracyDelfin, 17 September). `add_missing_env_keys` copies from
  `.env.example` the keys named in `ENV_KEYS_FROM_EXAMPLE` - each one's example
  value is the compose default, so an absent key already meant that - and
  never a password, a port or an address, whose example value is not what an
  existing install runs on.

- **A tool in the hand is swung on foot, and the engine will not say so.**
  `CHARACTER::mining()` asks nothing about a horse and `EquipItem` lets a
  pickaxe on from the saddle, so a miner that rode to its vein dug from
  horseback (Remigiusz, 17 September: a bot on a white horse with the
  pickaxe at a Sterta Muszli). The session climbs down at the vein and sends
  the horse away, as the fishing session does at the water; a session that
  puts a tool in the hand wants the same line, and the rider note above
  ("a rider reads, dresses, opens chests ...") lists the engine's refusals,
  which is not the same list as what looks right.
- **One HTTP request is one failure away from a failed update.**
  `Get-M2Download` made a single `Invoke-WebRequest` for the release asset;
  GitHub answered "(500) Wewnetrzny blad serwera" and dropped a connection
  a second into the download, twice in two minutes, and served the same
  47 MB minutes later (Hiob, 17 September; the manifest read through the
  API had just succeeded). Three attempts with a pause since 2.0.66; the
  antivirus block is still raised at once, because it does not mend itself.
  The launcher module ships in the package, so a launcher fix reaches a
  player one update late - the update that fails is run by the old code.
- **A trial is a hundred kills on one map, and every errand that leaves the
  map restarts the wait.** With the herbs out of the way (2.0.65) the trial
  bots still finished nothing: 85 arrivals on the desert in an hour, 75
  stays of 344 s on average, 67 under ten minutes, the kill counter moving
  25 at a time half an hour apart - three completions at 08h and none in
  the next three hours, 102 of 120 trial bots at 0/100 in the villages. What
  took them home was the Biologist hand-in they carried
  (`NeedsPlayerBotM1OnlyServices`, "frontier_services_to_m1" 29/h) and the
  offline stand's service walk (`BotOfflineBusy`, 23/h). Both wait while
  `IsPlayerBotOnBattleHorseTrial` holds on the desert; a blocking need (no
  weapon, no potions) still wins. Measure a trial as desert stays per bot
  and the `battle trial ... kills=` lines' spacing, never as "bots on the
  desert now" - nine at a time was the shape of eighty leaving. And the
  other half, once the stays were fixed: three trial bots reached the
  desert in twenty-five minutes while 126 stood in the villages, one of
  seventy in Joan all day on town visit -> market -> party -> town visit.
  `NeedsPlayerBotCriticalTownServices`'s soft half (a bag at 45 percent,
  which a keeper with goods carries for good) held the village branches of
  the world travel between visits; a trial bot skips it, the hard needs
  stand. Same shape as the medal droppers' loop of 15 September: a soft
  need the town cannot meet is a loop, not an errand. And the hard need
  behind it, "no free column" (`BlocksPlayerBotTravel`), was permanent for a
  keeper: `CollectPlayerBotSafeboxMaterials` kept every material the ledger
  said somebody was short of for the counter, and a counter lists a few
  lines - a bot of forty with 200 million yang held 38 stacks of them in a
  bag of 94 cells and four items in the safebox, sold three pieces per town
  visit, and left the desert a minute after arriving, every time. A full
  bag (`IsPlayerBotBagFull`) deposits them whatever the ledger says; the
  withdrawal already brings a material back only while the bag stays clear
  of pressure. Measure a keeper by its safebox count beside its bag.
  With all that in place (2.0.66) two trials finished in fifty minutes and
  the shuttle went on for the rest: `BlocksPlayerBotTravel` counts a bag
  with no free three-cell column, which a keeper with sixteen loose free
  cells has for good (GumbASSx, five stays of 97-426 s in forty minutes),
  and the personality's frontier visit clock ended a trial two-thirds done
  ("frontier_visit_complete" after 41 minutes). On the trial a bot is
  blocked by what stops the fight alone, its visit does not expire, and a
  trial archer fills its quiver like a dropper (2.0.67). A trial is one
  errand with one end; every clock and every need that ends an ordinary
  frontier visit has to be asked whether it ends this one.
- **An open horse trial outranks the herb errand, not only the hunt row.**
  2.0.61 made `GetPlayerBotBiologistHuntMob` yield to a trial; the herb
  errand (`PlayerBotMayTakeHerbErrand`, the trickle to the first villages)
  did not ask, so a bot of seventy-six with its battle horse open walked to
  Joan for the fourth herb row under "Zdobywam konia bojowego na pustyni
  (0/100)": on m2zip 88 of 124 trial bots had the Biologist as their goal and
  8 stood on the desert (17 September, two hours after 2.0.64, with 18 trials
  completed in those two hours - the trial works, it queued behind herbs).
  The errand refuses a trial bot; a hand-in already carried still walks. A
  status line that names one errand while the planner runs another is the
  measurement to keep making: goal x action of the bots wearing the line.
  And the gate uncovered a churn the full map had hidden: the row loop
  *granted* a place for every outgrown herb row it passed, so a bot whose
  pick ended on a collect row (the Orc Teeth it carried) took a place and
  gave it back in the same call, once a tick - 525 "herb errand" and 514
  "over" lines in two minutes. A place is taken for the row picked
  (`PlayerBotTakeHerbErrand`), and the loop only asks whether one is held
  or free. A gate consulted inside a loop must not have a side effect.
- **A counter line is sized by what the goods are worth, and the offline
  stand has to cut it.** The service visit added the best-scored cell as the
  stack it was: a refine material that was not a hoard went up whole, the
  anvil's reserve included (1084 material lines of more than ten on m2zip on
  17 September, 25 Kawalek Lodu for 19.7 million on one line - "wystawia
  ulepy w stacku po 20-40 gdzie nikt tego nie kupi", uxietoszef), and a herb
  went up as whatever a cell held. The bags kept their roots in full stacks
  of 200 at the front and the one picked up since sat further back, and equal
  scores sort by the higher cell, so the single root went up: 3343 herb lines,
  1171 of them one root, one shop with 34 herb lines for 89 units, and 62 813
  roots in 517 bags ("korzenie gango ... sa stackowane w sklepach po 1",
  Tieru). `IsPlayerBotBulkGoods` is Iwakura's sheet at
  `PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE` or less before the yang rate (the
  herbs and the ores; the cheapest refine material on those counters asked
  240 thousand a unit and the dearest herb 67 thousand) and goes up in heaps
  of `PLAYERBOT_SHOP_BULK_PACK_UNITS`, never under `_MIN_UNITS` (a
  herbalist's recipe takes ten), two lines of a kind; a refine material in
  packs of `PLAYERBOT_SHOP_PACK_UNITS` (five now), a hoard in tens, three
  lines of a kind, cut from what is over the reserve counted over every
  stack (`BotOfflinePrepareLine`); a herb line under the minimum and a
  material line over a hoard's pack come home at the next service visit
  (`BotOfflineUnwantedLine`). The Blessing Scroll is a refine material too
  (recipe 501), so the new branch steps round it and the scroll keeps its own
  keep. Measure it as lines by count bucket per kind in `player.item` with
  window IKASHOP_OFFLINESHOP, and the price per unit out of `ikashop_data`.
- **A Biologist row's monster is a family, and the level gap is the row's
  real wall.** SIZOWSKI's world (17 September): 997 of 1621 bots in Orc
  Valley, levels 53-66; m2zip the same hour: 277 of 1098, 275 of them in
  collect_quest_lv30's go_to_disciple at 3.3 teeth of ten, 129 standing in
  parties reading "Szukam celu dla grupy", 96 teeth accepted across 60 bots
  in 80 minutes and no collect row finished. Read off this world's files: the
  quest's hook gives the tooth only for 601 at 5%, and 601 stands in the
  valley on two points, both inside boss groups; the tooth reaches the world
  through the etc table on the Black Orcs 636/656 at 1.17 (x10000 against a
  range of four million), which `GetDropPct` fades by `PERCENT_LVDELTA` - one
  percent at fifteen levels over the monster - so a bot of seventy at the
  world's rate with its premium has one tooth in about 17 000 kills. The
  Curse Book has no hook (etc 2.70 on 706/756), the Demon Souvenir neither
  (etc 1.26 on 1001); the keys are hooks on 631-637, 701-707 with 731-737,
  and 1001-1004. And the hunt named one vnum, so every other carrier was
  worthless experience to a bot past its level. Now `IsPlayerBotBiologistHuntRace`
  turns the row's hunt vnum into its family (the lv40 key names 701 and the
  lv50 key 1002, because a specimen and its key are different families on
  the same monster), `NotePlayerBotBiologistCarrierKill` - under the horse
  trial's kill note and its VID guard - rolls the part of the etc chance the
  level gap took (GetDropPct's own percent times (100 - fade) / fade, so the
  world's rate and the premium apply and a bot in level gets nothing extra),
  only while the Biologist is still owed the item, and an outgrown collect
  row takes a place in `PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE` of the
  live bots for `_ERRAND_MAX_MS`, the herb errand's shape. A party's members
  each see the corpse and each roll; the reserve bounds it. With those three
  deployed, 51 of 86 bots in the valley still read "Szukam celu dla grupy":
  the hub choice asks the level band and not where the Black Orcs stand, so
  bots of seventy chose a band hub every thirty seconds and found nothing
  within the party's cohesion radius. The material errand's map scan
  (`StartPlayerBotMaterialHunt`) now keeps the nearest of the row's family
  as well and walks there first (`PLAYERBOT_HUNT: biologist errand`), and
  the answer is kept for `PLAYERBOT_BIOLOGIST_WALK_STICK_MS`: a fight on the
  way parks the route and the hub choice after it walked the bot off again -
  329 such walks in 21 minutes and still 43 of 91 valley bots in parties with
  nothing to hit - so the frontier wander walks to the kept point before any
  hub, the way a known Metin comes first for a stone hunter. And the top of
  `ManagePlayerBotWandering` stamps `BOT_ACTION_PARTY_ASSEMBLE` on any party
  bot it walks, so that walk read "[PT] Szukam celu dla grupy" - 56 of 90
  valley bots, every one of twelve looked at a minute later five to fifteen
  thousand units nearer the Black Orcs or fighting one. The walk is
  `BOT_ACTION_BIOLOGIST` ("Zbieram dla Biologa: Zab Orka"), stamped at the
  top of the wander while the kept walk is live - stamped only where the walk
  is taken up, the route continuation above it returned first and the next
  measurement found the words nowhere - and TRAVEL under the Biologist goal
  says "Ide do Biologa" only for a bot carrying the hand-in: 27 of 68 valley
  bots said it with no tooth in the bag. A status is a
  measurement only once the pass that sets it is known. Before believing
  a quest's kill hook, count its monster's spawn points on the map it sends
  the bot to (`scratchpad/count_valley_mobs.py` is the shape).
- **A service visit to a shop on another map is two map changes.** On m2zip
  on 17 September 3405 of 7951 map changes in 95 minutes were
  "offline_shop_service" and most of the rest the way back
  ("m1_direct_to_orc_valley" 2428, "level_to_orc_valley" 700): the valley's
  bots warped to their stands in the first villages every ten to fifteen
  minutes and straight back, 250 round trips inside thirty seconds - the
  traffic players read at the gates as bots going round in circles ("kreca
  sie ciagle pomiedzy tp", gregoszky). A keeper elsewhere waits
  `PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS` since `State::lastServedAt`; on its
  own map it still serves every ten to fifteen minutes, and an empty hand
  with a weapon on its counter never waits. Measure round trips as A->B->A
  pairs of `PLAYERBOT_WORLD: transitioned` per pid within thirty seconds, by
  reason (`scratchpad/loop_pairs.py`).
- **A package shop nothing opens is a shop that does not exist.** Karta
  Wedkarska (27620), which `CHARACTER::fishing()` wants worn on mt2009, is
  sold in exactly one place: `world.shop_special` 9009, the Fisherman - 25 000
  yang and five Materialy Rzemieslnicze (30378, the storekeeper's material
  exchange), from level fifty, once in twenty-two hours. `special_shop.quest`
  opens 20406, 9006 and the three guards, and no quest names 9009, so no
  player could fish ("gdzie mozna zdobyc fishing pass?" - "Nie da sie, misja
  nie dziala poprawnie", Greess and SIZOWSKI, 17 September); the bots never
  noticed because `EnsurePlayerBotFishingPass` makes theirs.
  `fishing_pass_shop.quest` is the Fisherman's button for it, compiled in the
  Dockerfile loop with `pc.open_special_shop` added to qc's function list.
  The shop's own limit said level fifty while the rods here are thirty
  (`apply.sh`, the line above it), so a player of thirty to forty-nine wore
  a rod and could not buy the pass: `apply.sh` lowers the LEVEL limit of
  27620 in `world.shop_special_proto` to thirty - the db core reads that
  table at boot, so it is live on the next start - and the quest's own check
  says thirty too (Tieru, 17 September). Two gates that name one level have
  to name the same number.
  Before telling a player an item cannot be had, look for it in
  `world.shop_special_proto` and then for the quest that opens that shop.
  The same day's "Wzmocnienie Przedmiotu from the chests does not count for
  the marble" was the package's design, not a bug: `world.crafting_proto` 102
  wants 71285, the craftable copy (recipe 101), and chests drop 71085.

- **A deposit that takes the whole stack is a withdrawal on the next line.**
  `CollectPlayerBotSafeboxMaterials` skips a material the anvil is short of,
  but the deposit then moved the **entire** stack - the reserve included - so
  one line later `WithdrawPlayerBotSafebox` asked the same question of a bag
  holding none, found the bot short, and took all of it back. Every visit, for
  ever: on m2zip 3574 of 4698 withdrawals in an hour were kinds the same visit
  had just deposited, Maud doing it every four minutes with the same eleven
  Kawalek Klejnotu. The syserr pairs this produced -
  `CreateItem: ITEM_ID_DUP` and `LoadSafebox: cannot create item`, 650 a day
  across 26 bots since 15 September - are that round trip seen from the
  database: `QUERY_SAFEBOX_LOAD` reads `player.item` directly, the db core had
  not yet written "in the bag now" (`PLAYER_CACHE_FLUSH_SECONDS`, seven
  minutes), so the load hands back an item the bot is holding and
  `ITEM_MANAGER::CreateItem` refuses the duplicate id. Nothing is duplicated;
  what it costs is the item's grid cell, which stays free for the visit, so a
  later deposit can put another item where a row already claims a place and
  that row never loads again (one such pair in the safeboxes on 17 September,
  71 in the bags). The deposit cuts the stack now - only what is over
  `GetPlayerBotRefineMaterialReserve`, the way a counter line is cut - the
  withdrawal skips every vnum the same visit deposited, and its anvil branch
  honours the bag pressure its market branch always did. Measure it as
  `PLAYERBOT_STOCK: to safebox` and `safebox withdraw` of one vnum for one pid
  in the same second.
- **A catch-up belongs where the bot already stands.** The herb rows are a
  trip to a first village and that trip is rationed
  (`PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE`, the answer to 2.0.60's flood), so
  a bot of seventy-five with four rows left never got a place and never caught
  up. `PlayerBotMayWorkHerbRowHere` gives a bot that is in a first village
  anyway - services, the market, a hand-in - `PLAYERBOT_BIOLOGIST_HERB_VILLAGE_MS`
  of work on an outgrown herb row without taking a place, because that adds no
  map change to the world at all; the window is per arrival and opens again
  only after the bot has been somewhere else, or the bots an update draws into
  the villages stay for all six rows. Beside it the share went to 70 per mille
  for two hours, and a trip whose bag already holds specimens keeps its place
  to the hand-in (`_CARRY_MAX_MS`) instead of expiring with the row half done.
  The gate is asked once, above the row loop: it starts the window, and a gate
  consulted inside a loop must not have a side effect.
- **The panel is the database, and the database is seven minutes behind.**
  `g_iPlayerCacheFlushSeconds` (db core, `PLAYER_CACHE_FLUSH_SECONDS`) is
  `60*7` and `CItemCache` expires with it, so a bot that had just put a shield
  on showed an empty shield slot in the classic panel for minutes and read as
  a sync bug (Tieru, 17 September). `HEADER_GD_ITEM_FLUSH` is the engine's own
  "write this row now" - `CInputMain` sends it after a shop deal - and
  `FlushPlayerBotItemRow` (playerbot_gear.h) sends it for the piece worn and
  the piece taken off. One write per equip; the bag, which turns over every few
  seconds, is left to the cache. Anything else the panels show late is the same
  seven minutes, not a panel bug.
- **A bot's stall is in neither its bag nor its depot.** On this line it is a
  real IkarusShop offline shop: `player.ikashop_offlineshop` is the stand (map,
  position, banner, premium flag) and `player.item` with window
  `IKASHOP_OFFLINESHOP` is the counter, each line's asking price inside that
  item's own `ikashop_data` JSON (`{"yang":...}`). That is what seban's panel
  reads for /economy/shops and what `/api/bot_shop` reads for the classic
  panel's stall window. The banner is cp1250 like every other name column.
- **The client's personality row is l0st3k's, and his serverinfo is his own.**
  Client 2.0.13 (16 September) moves a bot's personality off the alignment
  title onto a `CPythonTextTail` row of its own
  (`AttachPersonality`/`DetachPersonality` in his exe), so the rank is visible
  again and the options switch takes effect without a relog. Two things to
  check in any client anybody sends: his `serverinfo.py` carried his LAN
  address (192.168.0.70) - the package ships `127.0.0.1` - and the root must
  still hold our own scripts (`playerbot_status_tail.py`, `uiautohunt.py`,
  `autostackpump.py`, the rendered `uigameoption.py` and
  `uiscript/gameoptiondialog.py`). Extract both packs with
  `tools/eterpack.py --profile mt2009 extract` and diff against the published
  one before shipping; a script that calls a new engine function tests for it
  with `hasattr` so an older exe draws nothing instead of failing.
- **The herbalism system was shipped, compiled and entirely unused.** Baek-Go
  (mob 20018, one in every first village and NOT the Biologist, who is 20084)
  carries `herbalism_onboarding` and `herbalism` hooked to his chat, a special
  shop (14) selling the Herbalist's Knife and the three empty bottles, and 77
  rows of `world.crafting_proto` behind eight levels of recipe knowledge -
  Iwakura's write-up of 17 September matches the shipped tables to the yang.
  Nothing in this world had ever touched it: the recipes (29 Metin stone groups
  at 12.5-18%) went to the merchant as an unknown ITEM_USE and the herbs went on
  the counters as bulk goods. `playerbot_herbalism.h` is the AI's half of it.
  Five things that decide its shape, all measured on the running server:
  **the board is a client window** - `crafting.open` sends `craft_open` down the
  chat channel and `crafting.create` refuses anything the window did not report
  open - so a bot can never press a button on it and the craft is re-implemented
  against `CCraftingManager`, on the quest's own rows, odds, price and progress
  flags (`crafting.progress_<recipe>`, so a bot and a player share one ledger);
  **a craft spends the materials before it rolls**, so a 60% row is a real loss;
  **a potion is ITEM_POTION (type 36)**, a type of its own here whose use goes
  through the compiled hook `object/36/use_type` and not through any case in
  `char_item.cpp` - `value0` is the duration, `value1` the group, and the engine
  allows 5 boost, 3 offensive and 2 defensive affects at once; **the plants are
  not the supply** - sixteen bushes exist (20602-20644, the knife in WEAR_WEAPON
  like a pickaxe, three seconds a pick, 30% plus the knife plus
  `pc.get_mining_skill_bonus()`) and this world spawns four of them, in
  `stone.txt` rather than `regen.txt`: the Alpine Rose on a3/b3/c3, the Thistle
  on Sohan, the Amber Petal and the Nettle on the two Trent maps, and **not** the
  Peach Blossom the onboarding asks ten of. The herbs come from the drop tables
  instead, where all sixteen are. And the fifth, which is what made the first
  deploy do nothing at all: **`PLAYERBOT_PICKUP_GOODS_VNUMS` named two herbs**,
  the Gango Root and the Tue Mushroom, because those are what the Biologist's
  rows want - so the bags held 86 496 roots and 14 515 mushrooms against ELEVEN
  Peach Blossoms in the whole world. A system fed by a drop table needs the loot
  rule to admit every item it consumes, or it starves with the bags full.
  Two more things the first hour on m2zip taught, both the shape of traps
  already in this file. **A herb and a specimen are different items with the
  same name**: the Biologist's are ITEM_QUEST 50701-50706, which drop only
  while his mission is open and go straight to the bag, and the herbalist's are
  ITEM_MATERIAL 50721-50736, which drop on the ground like anything else
  (Tieru, 17 September, before a single line of the AI could confuse them).
  Every crafting row consumes the second range; nothing the bots do for
  herbalism may touch the first. And **a pass hung on somebody else's early
  return never runs**: reading a recipe was put at the tail of
  `ManagePlayerBotSkillBooks`, in the branch that fires only when no class book
  is due, and MordercaBezSerca3 finished the onboarding with the recipe in its
  bag, 55 skill books beside it, and read nothing for half an hour. It is a
  pass of its own with its own clock now - the same lesson as "a silent
  `continue` in the tick is a bot that stands for good", from the other side.

- **A getter that reserves is a getter the panel must not call.**
  `GetActivePlayerBotBiologistMission` took and gave back places on the two
  errand queues on every call, and `BuildPlayerBotStatusText` is one of its
  callers - so reading the line over a bot's head could hand it a trip or end
  one, and the panel changed the world by being looked at (audit of
  17 September). It takes `mayReserve` now and only the three passes that
  actually decide to travel pass true: the trip to a first village
  (`NeedsPlayerBotM1OnlyServices`), the frontier draw that sends a bot at a
  collect row, and the Biologist visit that gives the place back. Everything
  else - the status, the planner, the target picker, the kill note, the
  village hunt level - reads without touching the queue. Two rules that came
  with it: **handing in is not travelling**, so a bot carrying a row's
  specimens keeps that row whatever the share says (the gates used to stand in
  front of the `carrying` test, so a bot without a place walked past the
  Biologist holding his specimens); and **a spent quantum goes to the back of
  the queue** (`PLAYERBOT_BIOLOGIST_ERRAND_COOLDOWN_MS`), because the map is
  keyed by pid with no waiting list and whoever asks most often would
  otherwise reclaim the place the sweep just freed. A finished row still
  releases the place with no wait - the cooldown is for a quantum that ran
  out, not for work that is done.
- **A refine tier is not a finished weapon.** `PlayerBotCouldUseLevel30Weapon`
  refused the whole market to any bot already wearing a special level-30 weapon
  at `PLAYERBOT_LEVEL30_PROJECT_PLUS`, whatever was rolled on it, so a Full
  Moon Sword +7 with nothing on its lines stopped its owner from ever looking
  for a better one. The comparison below that test is the real answer and is
  stricter where it matters: `toBeat` counts a worn level-30 weapon at ITS
  POTENTIAL, so a good +7 still refuses every offer and only a poor one lets
  the search continue.

- **A package built from the worktree is not the package that was released.**
  `server-update-files.mt2009.txt` names sources, and `PathMap` renames them on
  the way out - so the row `linux-port/docker/panel/app/admin_panel.py` is read
  **literally from the 1.x tree**, which is gitignored (`linux-port/docker/
  .gitignore`) and on this machine holds whatever `start-server.ps1` last staged
  there for the other line. Building 2.0.71 again from the worktree on
  18 September put a panel of 971 769 bytes into the package where the release
  carries 1 040 779 - the one `files/admin_panel.py` holds - because the release
  was packed from a clean HEAD export, where that path does not exist at all and
  the packager falls back to the `files/` copy. Git's CRLF checkout is the other
  half: 42 more text files (the five horse quests, seban-panel's css/js/py, the
  itemshop's php) came out a percent larger than the published ones. So a full
  package is assembled by unpacking the **published** update zip over the deploy
  tree, never by trusting a rebuild to reproduce it; compare the two file by
  file before shipping, the way `scratchpad/player_zip_2071.py` does.
- **A clean HEAD export is not a package, and the packager does not fall back.**
  A 2.x update package is built from `git archive HEAD` - that is what keeps the
  gitignored 1.x staging out of it - but the file list names 126 entries and 7
  of them git does not track: the staged engine tree
  (`linux-port-mt2009/docker/game/src/server/{common,db,game,libthecore}`) and
  the panel's build context (`linux-port/docker/panel/{app,schema,bin}`).
  `New-M2UpdatePackage.ps1` **throws** on the first one it cannot find
  ("Listed file does not exist") - an earlier note here said it falls back to
  the `files/` copy, and it does not. So the export is filled from the working
  tree afterwards, copying **only what the export lacks** (a file already there
  came from HEAD and is the one that must ship), the staged
  `playerbot_*` are overwritten with HEAD's overlay, and
  `linux-port/docker/panel/app/admin_panel.py` is `files/admin_panel.py`
  (1 072 542 bytes for 2.0.93 - the number to check, because the 1.x staging is
  smaller). `scratchpad/fill_export_2093.py` of session 82d3ab90 is the shape;
  verify the built zip by reading files out of it, never by its size alone.
- **`check-release-covers-changes.py` reads the 1.x list only.** Its `LIST` is
  `launcher/server-update-files.txt`, so on a 2.x release it reports
  `linux-port-mt2009/VERSION` as "NIE DOJADA DO GRACZA" although that path is
  line 61 of `server-update-files.mt2009.txt`. The manifest is the other
  standing false positive: the launcher fetches it from GitHub, so it never
  travels in a package. Check both by hand against the mt2009 list before
  believing the gate, or fix the gate to take the engine.
- **A development purchase is a quantity, and the trip to make it is a
  share.** `playerbot_progression_needs.h` (Codex, 18 September) counts what a
  bot is short of for its own progress - books of a skill at Master up to
  `GetPlayerBotBookKeepLimit`, Kamienie Duchowe up to
  `PLAYERBOT_GRAND_MASTER_STONE_KEEP` while a skill stands at G1..G10, and the
  open collect row's specimens up to `GetPlayerBotBiologistReserve` - and
  `WantsPlayerBotStallItem` takes a counter line only when its whole count fits
  that need (`IsPlayerBotProgressionOffer`). It is paid from at most 30% of the
  bot's own spare gold and never over twice `GetPlayerBotShopAskingPrice`
  (`CanPlayerBotPayForOffer`, asked at the browse and again on the native slot
  right before the buy). The trip to the first village's counters for it
  (`ShouldPlayerBotVisitProgressionMarket`, answered through
  `NeedsPlayerBotM1OnlyServices`, which also holds the bot in M1 for the trip)
  was written for every bot with a skill at Master and gold - which is nearly
  the whole population, because a bot reads every book it gets and is always
  short of twelve - so it ships as `PLAYERBOT_PROGRESSION_TRIP_PER_MILLE` of the
  live bots at a time, droppers and a player's party left out, with a
  `PLAYERBOT_MARKET: progression trip` line: the shape of 2.0.60's herb flood
  caught before it left. The ledger counts books by 50300 alone, so its supply
  says "some book", not "this skill's book"; measure empty trips before
  widening the share. `IsPlayerBotSinglyTradedGoods` has no caller, and
  `BotOfflinePrepareLine` cuts no book or stone, so the new line unit of one
  for them reaches the classic stall's split only.
- **The offline shops' mutation budget is one a second for every keeper
  together.** `BotOfflineBudget` gates every step of every service visit, and
  the night of 18 September spent 1 863 of its 3 600 an hour on m2zip - 775
  edits, 720 adds, 269 buys. Codex's reprice slice (four lines every other
  visit, so repricing could no longer starve restocking) would have asked for
  more than the whole budget, and a keeper refused a token stands at its stand
  until the visit's ninety seconds run out. The slice is
  `PLAYERBOT_OFFLINE_REPRICE_SLICE` lines an hour now, at the ten-minute pace
  only while a counter's stamp is behind the generation this core runs (a
  yang rate moved in the panel). A restart is not a change - the first visit
  stamps the counter with what the core runs: walking every counter again at
  the fast pace took 284 of the 468 mutations of the first fifteen minutes
  after one - when every keeper's first visit is queueing for the same
  budget - and adds fell from 169 to 117, the bots' own purchases from 76 to
  48. The price of that: the stamp lives in memory, and a new
  `PLAYERBOT_PRICE_TABLE_VERSION` arrives with a restart and nothing else,
  so a bumped table reaches the counters at the hourly pace. Persist the
  stamp (the core's directory is the `game-var` volume) in the release that
  next bumps it. Left at 0 until a rotation came round, as the first build
  of this had it, the stamp could not see a rate moved either: a counter of
  thirty lines at two an hour comes round in fifteen hours. `PLAYERBOT_OFFLINE: budget last_minute granted=
  refused=` is the measurement, and granted near sixty a minute is a queue -
  which the ten minutes after a restart are, every keeper's first visit
  falling inside them (46 to 56 a minute measured on 2.0.72; 2.0.71 had no
  counter to say). And **a step of a
  slice is a visit of its own.** The first version kept the visit open between
  steps with the board still in edit mode; the ACK was back before the next
  tick, that tick asked `RecvShopRequestEditClientPacket` again, ikashop
  refused it as `IsBusy(BUSY_SHOP_MANAGE)`, the visit ended there and the slice
  was finished a service interval later - so on m2zip not one line was added to
  any counter in the sixteen minutes after a restart, against 169 on 2.0.71.
  Each step now closes its visit and comes back two seconds on, and the first
  visit after a spawn restocks first (`nextReprice` 0), as it always did.
  Anything that mutates an ikashop board twice has to reopen it in between.
- **A kind a bot keeps by count is counted over the bag, and its line is one
  unit.** Codex's review of 2.0.72 found both halves. The soul stone's scorer
  asked only the stones in the cells *before* a stack - the scrolls' old trap -
  so a bot's single stack of ten against a keep of three was never goods; and
  the offline stand's cut (`BotOfflinePrepareLine`) cut no book or stone, while
  a buyer takes a line only when all of it fits what it is short of. And
  `CountPlayerBotSkillBooksAhead` counted rows: a book stacks to ten on mt2009
  (`world.item_proto` stack 10, like 50513), so "keep twelve" kept twelve
  stacks. `playerbot_stall_rules.h` is the arithmetic, pure and tested against
  a bag laid out the engine's way (`tests/playerbot_stall_rules_test.cpp`): a
  stack is goods once it holds a unit over the keep (`HoldsSpare`), the offline
  line is one unit of the spare (`LineTake`), and the classic stall - which
  lists stacks whole, and on mt2009 is what creates a stand - lists one only
  while what stays behind still holds the keep (`MayListWhole`). The keep is
  `GetPlayerBotCountedGoodsKeep`, the number the buyer's side asks
  (`GetPlayerBotProgressionNeed`), so no counter sells what its keeper would
  walk to the market to buy back and `BotOfflineReclaimLine` has nothing to
  ping-pong with. `PLAYERBOT_SHOP_COUNTED_SINGLE_LINES` lines of one kind stand
  at a time, and a stone line longer than the stone keep comes home to be cut.
  On m2zip before it: 10 363 books in bags, 2 196 of them stacks of two or
  more, against 98 on the counters (91 singles, 5 pairs); 30 stones in the
  whole world and none on a counter.
- **A read the engine refuses is not a read.** `LearnSkillByBook` wants
  `PLAYERBOT_BOOK_READ_EXP` in hand under the level cap, keeps the book when
  it is short, and the use still returns true - so `PLAYERBOT_AI: read skill
  book ... success=0` was mostly refusals: 7 095 such lines in twelve minutes
  on m2zip on 18 September against 23 reads the engine rolled, and 78 of the
  91 readers under the mark (droppers whose experience is locked at 25 and 33,
  bots of forty in a second village where they may not hunt), each asking
  again every eight seconds. The pass waits for the experience now
  (`PlayerBotHasBookReadExp`, `book read waits for experience`). Count book
  progress as the engine's own roll line (`LearnSkillByBook <name> table idx`)
  and as `player.skill_level` - six bytes a skill: master type, level, next
  read - never as the AI's read lines. And a class read costs those 20 000
  whatever it rolls: with the BOOKS switch waving the day's wait away, a bot
  that reads as fast as it earns spends its experience bar on books.
- **A trip to the market is only as good as the counters it can read.**
  `ShouldPlayerBotVisitProgressionMarket` asks the ledger, which counts books
  by 50300 alone, whatever the skill and wherever the counter; the browse at
  the end of the trip reads the counters of the map the bot stands on, for
  the skill it is short of. On m2zip on 18 September 96 trips in half an hour ended in two
  purchases: the bot reached its first village's ring, browsed for two
  seconds and logged `trip over ... reason=nothing_on_offer`, and asked again
  a few minutes later. The supply it needs is per skill and per map the trip
  can reach (Codex's point 3), and `scratchpad/progress_2073.py` is the shape
  of the measurement: a snapshot of `player.skill_level`, the Biologist's
  `__status` (557528158 complete, -1726153001 the key) and the goods by window,
  diffed over hours, beside the syslog of the same window.
- **The client's exe is built here now, from the package's source.** The
  package ships its client C++ (`Downloads/Metin2 Singleplayer/Source`: a
  VS 2022 solution, "Source Client" and 1.1 GB of "Extern"), and
  `linux-port-mt2009/tools/build-client.ps1` builds it with our edits,
  `port/clientify.py` - exact-string, idempotent, both line ends tried because
  the client's files mix CRLF and LF. Three things the first build taught:
  MSBuild cannot open `..\UserInterface\Locale_inc.h` once a path passes 260
  characters, so the copy goes to `%TEMP%\m2cb`, never to a scratchpad; the
  client compiles against the *server's* `common/*.h` (`../../Server/common`),
  which is where `INVENTORY_PAGE_COUNT` and `ENABLE_EXTEND_INVEN_SYSTEM` live -
  so the script links our staged server tree there, and a constant changed in
  `common` changes both ends of the wire; and the links are junctions, which
  Windows PowerShell 5.1's `Remove-Item -Recurse` follows and empties - delete
  one with `[System.IO.Directory]::Delete(link, $false)` or Explorer. Built
  unchanged, the source gives the package's own exe to a kilobyte; client 2.0.13
  (l0st3k's build) differed from it only by `textTail.AttachPersonality` and
  `DetachPersonality`, which clientify.py now adds on AttachTitle's model - a
  row between the name and the guild, the guild a row higher. Compare two
  client builds by the identifiers in them, not by size: a Python sweep of the
  printable runs names every module function one has and the other lacks.
- **Four inventory pages on the 2.x line, and the database moves with them.**
  `INVENTORY_DEFAULT_PAGE_COUNT` is 4 in `common/length.h`
  (`apply_four_inventory_pages` in playerbotify.py), and everything after the
  bag moves with it: the horse's page to 180-224, the worn slots to 225 (their
  EQUIPMENT rows are relative and stay), the dragon soul slots, and the belt's
  cells to 287-302, which this line keeps in the INVENTORY window
  (`ENABLE_BELT_INVENTORY_EX` is off). A world saved under two pages therefore
  has every INVENTORY row from 90 up in the wrong place, and its quickslot
  blob holds BYTE positions that cannot name a cell past 255. The db core
  migrates both before any game core connects (`__MigrateInventoryFourPages`
  in `db/src/ClientManager.cpp`: rows from 90 up move by 90 in `ORDER BY pos
  DESC`, so no row lands on one not moved yet; the 80-byte blob becomes 120
  with every position a WORD; one InnoDB transaction with the marker
  `playerbot_migrations('inventory_four_pages')`) and refuses to start when it
  fails. There is no way back: a downgrade across 2.0.74 needs the backup from
  before it. The client compiles against the same `length.h`, and the wire
  changes with it (a quickslot's position and a shop sale's cell are WORDs), so
  an old client on a new server is a desync, not an old window. The auth core
  refuses one by version - `server_version: 1010100` in the auth CONFIG only,
  because a game core that sees a version of a million or more takes itself
  for a production server and turns `/reload p/q` and `beta_server` off - with
  "UPDATE", which the locale words as "Wymagana aktualizacja klienta gry przez
  Patcher."; the root's `constinfo.py` says 1.1.0. The client needed one edit
  the compiler found and nothing else would have: `AbstractPlayer.h` declares
  `AddQuickSlot` pure virtual with a `char` position, so widening only the
  implementation made `CPythonPlayer` abstract. Bots have the four pages too,
  by the operator's choice: in two hours on m2zip 295 bots put 4 376 items on
  pages III and IV, a full bag (80%) now means 144 items rather than 72, and
  the tick rose by about a tenth. Both panels drew two pages - the classic one
  hid III and IV, Seban's drew them over page II with `pos % 45` - and draw
  four on mt2009 now (`INVENTORY_PAGES`, and his tab art cut to a quarter,
  `quad-*.png`).
- **A core with no bot never ran the bots' clock.** `CPlayerBotManager`'s
  Update event is created when the first bot loads (`OnPlayerLoaded`), so a
  core hosting none - first and game2 under `unified`, and every core before
  its first spawn - never re-read the weights file nor ran the timed events.
  The chest gate is per core (each core's `CreateDropItem` rolls on its own
  permille), so there it stayed open for good, and with no schedule written it
  was open everywhere: "dropia tez poza konkursem" (NerrVoVy).
  `StartWorldClock`, called from the bootstrap in `input_db.cpp`
  (`apply_world_clock`), runs the weights and the events once a second until
  the Update event exists and then stands down. The operator's rule since
  2.0.74: a Moonlight chest drops only while a chest event runs, so no
  schedule means no chests. On m2zip, 39 chests picked up in six minutes an
  hour before, none in the seventeen minutes after, beside 1 994 other pickups.
- **The bots' pass has a time budget.** The game core is one thread, so a
  pass over the bots is time in which no login packet is answered - 700 ms
  and more while a cohort spawns, which is SIZOWSKI's hanging login on a big
  world. `PLAYERBOT_TICK_BUDGET_MS_DEFAULT` (120 ms; weights key `TICK_MS`, 0
  for none) ends the pass when it runs out and the next one, a quarter of a
  second later, resumes at the next pid; a sweep counter replaced the tick
  counter in the heavy/light parity, so a bot that is often cut off does not
  always land on the same half. `sliced=` in `PLAYERBOT_LOAD` counts the cut
  passes. At 1 099 bots with a budget of 20 ms: 150-205 cut passes a minute,
  the longest pass 34 ms against 343.
- **Green bonus stones are what a bot under forty may use.** 71151/76023
  change and 71152/76024 add, only on a weapon or a body armour of level forty
  or less (the engine's own rule), and the reroll pass returned below
  `PLAYERBOT_BONUS_MIN_LEVEL` for everybody, so 628 of them lay in the bags of
  bots under forty (Sammy). Under that level the pass takes green stones only
  and no marble; above it a green stone goes first on a piece that takes one.
  Fifteen minutes after the deploy a bot of twenty-five had added lines to a
  Gilotynowe Ostrze+7 and a Tiger plate +6.
- **Hay, carrots and the mission books are pickup goods** (50054, 50055,
  50307-50310): a player uses them and no bot does, and the merchant paid five
  hundred yang for a book (Greess). The mission books were picked up three
  times as often in the first seventeen minutes.
- **Auto Lowy asks for stones first and names what it cannot reach.** With
  Metiny on, a stone outranks every monster in `/autohunt_target`
  (`apply_auto_hunt_stone_priority`), and a fifth argument names the VID the
  client gave up on after `STUCK_SECONDS`, which the server skips for
  `STUCK_SKIP_SECONDS` (a minute) - it used to name the same unreachable
  monster straight back (blasty).
- **A Linux update stages the panel's build context itself.** Only the
  Windows launcher's `Sync-M2PlayerbotOverlay` ever copied VERSION, the
  changelog, `admin_panel.py`, items.json, the favicon, the schema and
  `files/static` into `linux-port/docker/panel/`; a VPS updated with
  `update.sh` built from what the package held and stopped at "/schema: not
  found" (DUDU). `stage_panel_context` does it before compose, the package
  ships the schema, and `check-update-covers-build.py --context` checks the
  shared build contexts with no prefix assumed staged. listify.py skips an ELF
  in the staged tree - a local compile left the 78 MB game binary there, and
  it looked like a file the port had added.
- **The presence on Discord is ours.** `apply_discord_presence` in clientify:
  application 1548716643541065798 ("Metin2 SinglePlayer") and the button to
  the YouTube channel instead of mt2009.pl. Discord does not show a profile's
  owner the buttons of their own presence, so "Dolacz do gry" is checked from
  another account.
- **"Scal i uporzadkuj" is one server operation, for a player's button and for
  the bots alike.** The inventory's auto-stack button sent a move for every
  pair of stacks - three hundred in a frame, which the flood limit closed the
  connection on, and then a few at a time (autostackpump.py) - and a queue of
  moves could only pour stacks, never lay a page out. It sends
  `/inventory_arrange` once now (client-root/inventoryarrange.py), and the
  server answers `InventoryArrangeResult <code> <moved> <merged> <units>`.
  `playerbot_arrange.cpp` is a translation unit of its own - the mt2009
  Makefile compiles every `*.cpp` in game/src, r40250's every
  `playerbot_*.cpp`, where it is a stub - and reads the four pages into
  `playerbot_arrange_rules.h` (pure, tests/playerbot_arrange_rules_test.cpp).
  The plan pours stacks on a copy of the counts (the fullest stack keeps its
  id and an emptied stack's quickslot follows it), then lays the pages out by
  category, potions first as the operator asked for the bots' bags: first fit
  in reading order; the tallest first when that does not fit; an exact packing
  of the free runs when neither does (bin packing with heights of one to three
  is a table over the runs: best[a] is the most two-cell items beside `a`
  three-cell ones); and, never needed yet, the old layout, which is always
  legal because pouring only takes items away. Only a complete, checked plan
  is applied, the way MoveItem moves one item: every item that changes cell
  is RemoveFromCharacter'd first and SetItem'd at its new cell after, so a
  cycle needs no free cell, and the quickslots are rewritten from a snapshot
  taken before the first pour. An item `isLocked()` stays where it is - an
  active auto potion, whose affect holds its id and which MoveItem refuses.
  Refused when dead, when `CanHandleItem(false, false, 0)` says busy (every
  busy state, the item shop's included), while a quest runs, and within two
  seconds of the last click. Two stacks pour only when vnum, flag word,
  sockets, attributes and look all match - stricter than MoveItem, which asks
  the sockets alone. The units of every vnum are counted again after each run
  and a difference goes to syserr as `INVENTORY_ARRANGE:`. The bots on the
  2.x line arrange every half hour and a pid's spread (`ManagePlayerBotArrange`,
  in place of `SortPlayerBotConsumablesToFront` there): an item picked up
  since shifts everything after its place in the order, and every moved item
  is a save for the db core. The client refuses the click while an item hangs
  on the cursor or a private shop is being built - both name cells the server
  is about to change. The method the button used to call stays in
  uiinventory.py as `__OnAutoStackButtonByMoves`, never called: clientrootify
  anchors the edit on the method's first line alone, which is the only text
  the stock root, a root with the pump and a root with this all share.
- **`ChainQuickslotItem` took its old position as a BYTE**, and four pages put
  the belt on 287-302: a potion stack running out in the belt chained the
  quickslot that pointed at bag cell 34 (290 - 256) and left the belt's own on
  an empty cell. WORD since 2.0.75, like SyncQuickslot and TQuickslot.pos.
- **How many monsters a respawn line keeps standing is an event flag, and
  "boss or stone" was never true of any line.** `regen_spawn` topped each line
  up to its `max_count`; `regen_target_count` (playerbotify
  `apply_regen_spawn_count`) makes that `max_count` times `m2_mob_count` or
  `m2_boss_count` percent (100 or unset = as written, 400 at most), written by
  the classic panel's /rates card "Liczba potworow w respie" as `player.quest`
  rows with dwPID 0 and made live by web_admin.quest's `REGEN_COUNT`, like the
  respawn times (Kiciamol, 18 September - his own edit of regen.cpp was undone
  by every update). A dungeon's lines and a quest's one-off spawn are never
  multiplied, nor is any line one of whose possible members is not a monster
  or a stone (`regen_member_vnums`: the vnum, a group's members, every member
  of every group a group of groups may draw); and a count above the target
  spawns nothing: `max_count - count` used to wrap round to four billion the
  moment a lowered multiplier left more standing than the line asked for.
  The member test is what the first version got wrong: it asked the type of
  `m` lines only, and **stone.txt's `r` lines are not Metin stones** - on this
  world they are the ore veins (20047-20059) and herb bushes (206xx), NPCs of
  rank five, 380 lines against the 180 `m` lines that hold the actual stones -
  so "zwykle potwory x2" would have doubled every vein and bush, and "Metiny i
  bossowie x2" the horse and pony groups of npc.txt. `read_line` also
  classified a line as boss or stone before it had parsed the vnum - the zero
  of a fresh REGEN - so `is_boss_or_stone` was false for every line and the
  /rates page's "Metiny i bossowie" respawn time reached nothing since
  2.0.64; the vnum is read first now and a group of groups is asked the way
  the engine asks a group (a boss, a mini-boss or a stone among its members),
  which takes in four `r` lines of stones 8031-8034 on the other cores' maps
  and puts the veins and bushes on the boss field's respawn time. The groups
  of a group of groups are private to `CMobManager`, whose only answer was one
  at random, so playerbotify gives it `GetGroupGroupMembers` and
  `mob_manager.h` ships staged. `scratchpad/regen_class/classify.py` of
  session 82d3ab90 is the shape of the measurement: it reads every map's four
  regen files, group.txt, group_group.txt and `world.mob_proto`'s rank and type.
  The maps are built three seconds before the `m2_*` flags reach the core, so
  after a restart the boot spawn is x1 and each line reaches its multiple at
  its own next respawn - minutes for monsters, 15-25 minutes for stones and
  bosses. `PLAYERBOT_LOAD` carries `mobs=`, `stones=` and `npcs=`, what is
  standing on the core, which is how the multiplier is measured; `npcs=` leaves
  out a horse with a rider, which comes and goes with the bots. A vein kills
  itself 7-15 minutes after it stands and its line brings it back only at the
  line's own time (18-22 minutes, most an hour), so an NPC count watched for
  three minutes after a switch proves nothing - the lines have not come round.
  Measured on m2zip at 1099 bots: 41 436 monsters at x1, 80 344 two minutes
  after x2; with stones at x2 and monsters at x1 the stones went 113 -> 231 as
  their lines came round while the veins' lines came back one apiece (+27,
  where two a line would have been at least +62); and the surplus after going
  back to x1 fell 81.4k -> 72k in twenty minutes, by killing alone. The bots'
  pass went from 13-14 s to 16-18 s of every 60 at x2.
- **A client update refused for a running game is asked about before the
  download.** Windows will not replace the exe of a running program and says
  so only at the copy - after the 65 MB download, every time: Ratorex (18
  September) tried five times in a quarter of an hour. `Assert-ClientNotRunning`
  (Metin2-Launcher.ps1) asks `Get-M2FolderProcesses` for anything running from
  the client folder before `Update-Client` downloads, and "update everything"
  asks it before the server, so a new server is never left beside a client
  that cannot log in to it; a sharing violation at the copy
  (`Test-M2FileInUse`, HRESULT 0x80070020/21) gets `New-M2FileInUseError`
  instead of the raw Windows sentence. Like every launcher fix, it reaches a
  player one update late.
- **Compress-Archive stops at 2 GB, and the world's backup went through it.**
  Windows PowerShell 5.1's Compress-Archive holds every entry in a
  MemoryStream (a documented limit), so a world whose `log.sql` dump passed
  2 GB failed `New-M2DatabaseBackup` with `Exception calling "Write" with "3"
  argument(s): "Stream was too long."` - "Strumien jest za dlugi" on a Polish
  Windows. The reset backs the world up before it deletes anything, so such a
  world could not be reset at all, and the world stayed whole every time
  (uxietoszef, 18 September: three tries in a day). The backup's zip is
  `ZipFile.CreateFromDirectory` now, which writes each file straight into the
  archive; tested on a 2.4 GB file, and `Expand-Archive` - the restore's way
  back - reads it whole. Unlike an update fix this one works on the first try
  after the update: the GUI runs every action as a new `Metin2-Launcher.ps1
  -Action` process, which imports the module the update wrote. Anything else
  that zips a world wants the same call.
- **A pass the budget cuts must still move every bot.** 2.0.74's budget
  resumed at a pid and alternated the heavy and light tick by sweep, so once
  a sweep took several passes a bot got nothing between two visits - no next
  waypoint, no blow - and stood at the end of its leg: "dwa kroki i staja" at
  1500 bots on one core (SIZOWSKI, 18 September), invisible on m2zip, whose
  1099 bots never sliced (`sliced=0`, `tick_max_ms` 91). The bots a pass does
  not reach take `RunPlayerBotLightTick` (route continuation and the blow at
  the target in hand, nothing planned) after it, every pass; the budgeted
  pass leaves room for that by the last light pass's cost, never under a
  quarter of the budget. `light_ms=` in `PLAYERBOT_LOAD`.
- **A second channel is a partition, and every core must compute the same
  one.** `M2_PLAYERBOT_CH2` (default 0) and `PLAYERBOT_CH2_SHARE` (10-90,
  default 40) come to every core from the container's environment; the
  entrypoint takes them from `.env` or from the web panel's
  `/opt/m2spool/channels.wanted`, whichever `SET_AT` is newer, raises
  `M2_CHANNELS` to 2 and writes `/opt/metin2/var/channels.effective` for the
  panel. `playerbot_channel_rules.h` (pure, tested) puts a pid on channel 2
  by a mixed hash under the share, unless it is pinned: every bot that has
  ever owned an offline shop (`player.playerbot_channel_pin`, append-only,
  filled by apply.sh and by each core before it reads) lives on channel 1 for
  good, because shops are channel 1's alone - `SubmitPlayerBotOfflineShop`,
  the stall pass and, for players too, `OpenOfflineShop` (playerbotify) refuse
  anywhere else. Append-only is what keeps a core restarted mid-session
  agreeing with its neighbours: no channel-2 bot can open a shop, so none can
  become pinned while the others run. `LoadRegisteredBots` registers only
  its own channel's identities, so the split, the queues, the top-up and a
  GM's spawn cannot start another channel's bot; `IsRegisteredBotPID` asks
  every channel's set, because "is this a bot" is a different question.
  Pins unreadable: channel 1 takes only the spread's bots, channel 2 none - a
  bot may start nowhere, never twice. `Spawn` also refuses a pid the P2P
  table knows (the belt: it cannot see a start's first batch). A pid on two
  cores is not a curiosity - the db core serves a bot's load to anyone,
  P2P_MANAGER overwrites the entry, and two copies' saves duplicate items.
  The operator's number is the world's, and it is split between the
  kingdoms before it is split between the channels: `SplitForThisChannel`
  runs `SplitPopulation` over every channel's identities (the same answer on
  every core), then gives channel 2 its share of each kingdom
  (`ShareOfTotal`, never more than its identities there) and channel 1 the
  rest. Split per channel first, a thousand came out Shinsoo 252, Chunjo 491,
  because the 1100 pinned shop owners are channel 1's and mostly Chunjo's.
  Measured on m2zip at a thousand and 40: 282/200/276 on channel 1, 52/133/57
  on channel 2, 334/333/333 in the world, no pid on both channels, no shop
  opened on channel 2. Medal
  droppers, the events leader, the declaration of guild wars, tower raids,
  the strength census, founding and the guild report are channel 1's. A war
  is fought on both channels, and channel 2 has no copy of the pair channel 1
  keeps in `s_mapPlayerBotGuildWars`, so there `GetPlayerBotWarEnemy` takes
  any field war between two guilds whose masters are both bots - the first
  build read only that map, and channel 2's bots sat their guild's wars out. Ports: compose publishes
  `M2_GAME_PORT_RANGE` onto `M2_GAME_CONTAINER_PORT_RANGE`, and the launcher
  widens both to 13000-13012 only while the channel is on, so a world that
  never asked publishes nothing new. The client lists 2 channels and
  intrologin hides one past the first that does not answer. Until 2.0.84 a
  bot on channel 2 did not trade: the offline market read only its own
  channel's shops (as ikashop's `IsNearShop` does), and `PlayerBotCanOpenShop`
  answered no there, so its goods took the no-counter path - the merchant under
  bag pressure, the safebox. Since then it asks to be moved to channel 1 for
  anything at a stand (the next note), and the pins above are not read.
- **The pins emptied the second channel, so since 2.0.84 a bot's channel is
  a row that moves.** On a world that has played nearly every bot keeps an
  offline shop - 2 404 shops for 2 500 bots on SIZOWSKI's - so nearly every bot
  was pinned to channel 1 and channel 2 carried 42 ("% botow na channelach nie
  dziala poprawnie", Xewi and Mkls, 19 September). SIZOWSKI sent a design and a
  patch the same day; what shipped is his design with the ready time his patch
  lacked. With the channel on (mt2009 with ikashop, `m_bChannelTable`) a bot's
  channel is its row of `common.playerbot_channel_assignment` - one row a pid,
  read by `LoadRegisteredBots`, a missing row filled from the spread by the
  coordinator; the pins are not read. A bot on channel 2 with business at a
  stand asks to be moved (`RequestShopChannel`): a stand to open
  (`EnsurePlayerBotPrivateShopChannel`, which also holds the bot in town up to
  75 s), another bot's counter to buy from, and its own stand 45 to 75 minutes
  after the bot arrived on channel 2 (`PLAYERBOT_SHOP_CHANNEL_SERVICE_*`). The
  first build asked at every keeper's first service there, and 235 of 397 bots
  of channel 2 were waiting twelve minutes after the start, against 33 places
  a gate; the second asked for an expired stand at once, and a stand its owner
  will not renew (the TRADE slider, no yang for the fee) stays expired with its
  goods for good - 253 of them among channel 2's owners on m2zip - so those
  owners went back and forth for nothing. The
  coordinator is the channel-1 core that hosts Joan: a census every five
  seconds of the bots seen in the last half minute (both channels publish every
  ten) and one step a gate of two minutes (`common.playerbot_channel_control`)
  - `PlanChannelMoves`: straight in under the cap, one for one at it (3% a
  gate), a drain's worth more out than in over it, and with nobody waiting a
  drain (2% a gate): over the cap back to the cap, with anybody not pinned,
  the cheapest first, and between the cap and the target only bots with no
  live stand - with most bots behind a stand, that gentle drain alone found
  two of 686, and a first build that drained towards the target at any cost
  took twenty-one bots off a channel one over its cap. The cap
  and the target come from the slider: 100 minus the share, and ten under that
  (60 and 50 at 40). Who steps out is chosen by `MoveCost`: a village +1, an
  errand +1, a live stand +2; a service visit, a shop operation in flight, a
  player's party, a war, a dungeon, a raid, a duel and the medal droppers'
  cohort (channel 1's alone, `SpawnMedalDropperCohort`) are pinned. SIZOWSKI's
  swap kept every bot in a village out altogether; on m2zip 654 of 693 bots of
  channel 1 stood in a village and the swaps ran at six a gate while eighty
  waited. A move is a row changed: the old core reads it within a refresh and
  despawns the bot, the new one spawns it once `ready_at` has passed and the
  P2P table no longer knows it, so no pid is ever on two cores; a bot moved out
  of channel 1 stays out of the next swap for twenty minutes (`moved_at`). A
  bot moved in for its stand is served five seconds after it loads
  (`m_setChannelMovedIn`) - 12 s from arrival to the first counter line
  measured. Everything runs on a connection and a thread of the manager's own
  (`m_pChannelSql`; the credentials come from config.cpp through
  `apply_channel_connection` in playerbotify.py), so the game thread never
  waits for the database. The coordinator moves nobody until the spawn window
  plus two minutes have passed, never under five (the first test drained the
  shop channel thirteen seconds in), and a start drops the last run's requests.
  Measured on m2zip at 1 000 bots, the 99 medal droppers and a share of 40:
  channel 1 went from 693 of 1 099 to its cap of 659 in two gates (29 out and
  8 in, 27 and 7) and then swapped one for one with a dozen waiting; ticks 6-7 s
  of 60 on channel 1 and 4.7 s on channel 2; no pid on both channels, no login
  refused. A player never sees any of this but the numbers on the channel list.
- **An event that cancels itself wrote into freed memory.** `event_process`
  deletes the queue element before calling the event and left `q_el` on it,
  and `event_cancel` of a processing event writes `q_el->bCancel`. A quest's
  `target.delete` of its own arrow is that path; the chunk often belonged to
  the script compiled a moment before, and game2 died in `luaV_execute+0xac7`
  (OP_GETGLOBAL through a Proto's `k` that was no longer one) at the third
  point of the horse training on the fire land, and at every login beside it
  (Dearminder, 18 September). `apply_event_cancel_in_flight` nulls `q_el`
  after the delete; every reader handles NULL. Read the faulting instruction
  before blaming a quest: `objdump -d --start-address` on the shipped binary
  at the symbol's offset (luaV_execute is in the dynamic symbols).
- **mt2009's affect.add_collect takes a POINT too, sums, and never expires.**
  The panel's "Szybkosc biegu" passed `apply.MOV_SPEED` (APPLY 8 =
  POINT_MAX_SP) and gave a hundred SP (archonek, 18 September); and because
  a point has one AFFECT_COLLECT, taking a panel speed off there would take
  the Biologist's movement reward with it. The panel's speed is
  `affect.add_new(9910, POINT_MOV_SPEED, bonus, secs)` now - a type of its
  own, beside speed_boost's quest affect - with `affect.remove_new`; both
  had to join qc's function list in the Dockerfile.
- **A requirement window is the client's count, not the server's.**
  `utils.CountItemCountInInventory` counted `INVENTORY_PAGE_SIZE * 2` (+1
  with the horse out), so after the four pages a horse-bag unlock read "0 na
  60" for goods on pages III, IV or in the bag (blasty, 18 September) while
  `CountSpecifyItem` on the server was right. utils.py is rendered by
  clientrootify now. And intrologin.py is one of clientrootify's renders:
  an edit made to `client-root/` directly is undone by the next render - put
  it in `EDITS`.
- **A village a bot has outgrown gives it its top bands, not its top band.**
  `CollectPlayerBotM1HubsForLevel` took the nearest band when none held the
  level, and for a bot above every band that was Joan's two band-21 hubs:
  thirty bots of 28-35 and their horses on one meadow (Remigiusz, 18
  September). Above the top band + 3 it takes whole bands downward until
  `PLAYERBOT_M1_OUTGROWN_HUB_CHOICES_MIN` (6) hubs.
- **A merge of Seban's panel can drop our links.** The 1.54.1 merge
  (5f06d3d) took the "Masowe nadawanie przedmiotow" section out of
  manage.html while `/manage/items` stayed - a page nothing linked to
  (archonek, DUDU). And his spawn-plan form refuses to save without his
  integration; it now says where the plan is set instead.
- **The whole drop under `.** `/pickup_nearby` (`CHARACTER::PickupNearbyItems`,
  playerbotify) hands every item in the pickup range that is the
  character's or may be its party's to `PickupItem`, nearest first, 40 at
  most, once per half second, lifting and restoring `m_lastPickupTime` around
  its own calls; a bag without room for the next stops it.
  `client-root/pickupnearby.py` sends it from the ` key; Z stays single.
- **Per-kingdom bot counts.** `PLAYERBOT_AUTOSPAWN_PER_KINGDOM=1` and
  `PLAYERBOT_AUTOSPAWN_{SHINSOO,CHUNJO,JINNO}` (launcher: "Indywidualne
  wartosci dla krolestw", Greess) replace the split of the one number with
  `playerbot_empire_rules::TakeKingdomCounts`: each kingdom its own, cut to
  the identities it has, nothing handed on. With the second channel on each
  kingdom's number is the world's too (`ScaleToThisChannel` per kingdom):
  100/400/100 at 40 started 60/240/60 on channel 1 and 40/160/40 on
  channel 2.
- **A player is a buyer the ledger cannot see, so every village keeps a
  floor.** `DecidePlayerBotMaterialListing` listed a recipe material only
  while the core's counters held less than five units per bot short of it,
  with one probe stack when nobody was, and a player's purchases never reach
  that count: on m2zip on 18 September 3 366 of ten minutes' decisions said
  overstock against 255 that listed, the bags and safeboxes held about 1.27
  million units of material against 105 thousand on the counters, and 167 of
  the 564 pairs of a recipe material the bots held two hundred of and a
  village had nothing on sale - Czarny Uniform 62 065 held and none in
  Pyongmoo or Bakra, Ksiega Klatw and Zab Orka nowhere ("chomikuja", Hiob;
  his item finder found no counter with the Orc Valley's or the desert's
  materials in any kingdom). The ledger keeps the units by the map their
  counter stands on as well (`s_mapMarketLocalSupply`, and
  `AddPlayerBotMarketSupply` takes the map from all three writers: the classic
  stall, the offline shop at the refresh, and each offline add at once), and
  in a village the decision asks first whether that village's counters hold
  `PLAYERBOT_MARKET_LOCAL_FLOOR_UNITS` of it (FLOOR, scored 480, counted in
  the ledger line). Only what is over the bot's anvil reserve is ever goods,
  so the floor sells nothing a bot needs. The floor is per village because
  ikashop's item finder searches the searcher's own map
  (`RecvShopSearchItemClientPacket` skips a shop on another map).
  What it did to the counters was not measured before 2.0.77 shipped: the
  release went out at once because 2.0.76 had broken the support bundle,
  and a keeper adds one line per service visit, so a village fills over
  hours - measure it as `scratchpad/market_coverage.py` of session 82d3ab90
  does (pairs of a recipe material and a village with nothing on sale).
- **The offline mutation budget is a bucket.** One token every
  `PLAYERBOT_OFFLINE_MUTATION_MS` (500), `PLAYERBOT_OFFLINE_MUTATION_BURST`
  (five) saved. It was one a second with nothing saved, so two service visits
  in the same second had one refused while the seconds before had gone
  unused: 35 to 47 granted a minute against 25 to 434 refused on m2zip. A
  visit costs one token and adds at most one line, so the visits are the
  ceiling now, not the budget: 59 to 71 granted a minute and 0 to 2 refused
  in the first minutes after the change. Read the db core's queue before
  raising it again.
- **The Red Forest's arrival and exit stood on blocked cells.** Decoded in
  2.0.77 with the lzo that `m2-eterpack:dev` carries: the arrival is rescued
  a cell away by the engine, but the exit's nearest open cell was 625 units
  off, beyond every snap, so each bot that wanted to leave map 68 planned the
  same unreachable walk every twenty seconds - 657 of the core's 1 018
  unreachable lines in half an hour, 22 of the 25 bots on the map - and left
  only by a direct transfer. None in the minutes after the move, and all
  unreachable lines from 34 a minute to 5. One hub of each forest stood on a
  blocked cell too. Decode a map before trusting a point taken from its regen
  file: `scratchpad/pick_points_2077.py` of session 82d3ab90 is the shape
  (the area the regen stands on, 4-connected, BLOCK|OBJECT at the cell centre
  like the navigation grid, three open cells all round).
- **A war foe in the safe zone is no foe, and a pair is not a kingdom's war
  for ever.** `FindPlayerBotGuildWarFoe` took the nearest enemy wherever it
  stood, and `battle_is_attackable` refuses anybody on ATTR_BANPK, struck or
  striker, so an enemy inside the guild map's safe zone drew its foes in to
  swing at nothing ("sporo stalo w bezpiecznej czesci", gregory_955).
  `IsPlayerBotWarTargetable` filters the search and the held foe, and a bot
  standing in the zone walks to the rally, which is open ground by
  construction. And the pick put the tier gap first and let the rotation by
  the minute only break ties, so a kingdom with exactly two top-tier guilds
  fought one war every two hours: Tuskaffki against Przelew24 for a day on
  m2zip with seven more guilds ready. The kingdom's last pair sits a war out
  while a third guild is ready, and within one gap the pair whose latest war
  is oldest goes first (`s_mapPlayerBotLastWarPair`,
  `s_mapPlayerBotGuildLastWarAt`, kept for the process). The panel already
  showed a running war's score; gregory asked for one not knowing it.
- **A Compose that pulls what the project builds, and a diagnosis that read
  any 404.** seban-collector and seban-item-grants run metin2/seban-panel,
  which the seban-panel service builds; an older Compose pulls it from Docker
  Hub first and ends the start in code 1 ("pull access denied", DUDU on a
  VPS, archonek updating to 2.0.76 on Windows) - Compose 5.5 on this machine
  never tries. `pull_policy: never` on both (the source is
  linux-port/docker/docker-compose.yml; composify renders it). And
  `Get-M2LauncherErrorGuidance` took "404" anywhere in a failed action's
  output for an unpublished update channel - a panel request for a missing
  icon is enough - so archonek was told the manifest was missing while it
  answered 200. The 404 must share a line with update-manifest now, and
  "pull access denied for metin2/" has its own remedy (LOCAL_IMAGE_PULLED).
  The gate that checks a release's changes reach a player
  (`check_release_covers_changes_mt2009.py`) also had to learn that
  linux-port/docker/docker-compose.yml reaches the 2.x line only through
  composify.
- **`[string]` of a command that printed nothing is `$null` in Windows
  PowerShell 5.1.** 2.0.76 probed for the second channel's files with
  `$probe = [string](docker compose ... exec ... ls ...)` and asked
  `$probe.Trim()`, so on every server without the channel - nearly all of
  them - the support bundle ended in "You cannot call a method on a
  null-valued expression" (archonek and Urtopy within the hour, both as
  "Logs (kod 1)"). Join and ask instead:
  `[string]::IsNullOrWhiteSpace([string](@($probe) -join ''))`; `"$x".Trim()`
  is safe too, a bare `.Trim()` on a cast never is. Tested under
  StrictMode 2.0 both ways, and the whole bundle end to end on m2zip.
- **A floor is only as good as the room on the counter.** 2.0.77's village
  floor did what it said - 272 of the 409 lines the keepers added in the
  first twenty minutes were recipe materials - and the missing pairs fell
  only from 167 to 155 in forty minutes, all of it in the first villages:
  Bokjung, Jayang and Bakra did not move, because 65 of Bokjung's 85 offline
  counters had all sixty cells taken, and the keepers of Bakra held 48 of
  the 49 materials Bakra lacked in their own bags. What filled the counters
  was measured against two days of sales (`scratchpad/sell_through.py` of
  session 82d3ab90 is the shape): 6 581 polymorph marble lines on 1 033
  counters, up to thirty-one on one, and not one marble sold, against 792
  sales of 11 416 material lines and 501 of 99 book lines; weapons were
  7 071 lines for 30 sales. A marble scored 600, above every material, so a
  keeper with a marble and a floor material put the marble up first. The
  counter shows `PLAYERBOT_SHOP_MARBLE_LINES` now, never two of one monster
  (socket 0), the rest come home one a service visit (`BotOfflineUnwantedLine`),
  the score is under the books and the materials, and a marble past the
  counter's share is merchant scrap under bag pressure even with a counter.
  The caps of the two offline add loops are one function now
  (`BotOfflineCounterRefuses`): the line chosen before the board opens and
  the add must refuse the same thing, or a cut line stays in the bag.
  Measure it as marble lines per counter, full counters per village and the
  missing pairs of `market_coverage.py`, by village. Everything in these
  2.0.78 notes was compiled on both engines and ran on m2zip from 22:48 on
  18 September; the release went out before the first measurement of it
  came back, because 2.0.77's update was failing for a player.
- **A pick's memory that lives in the process is gone at every update.**
  2.0.77 kept the kingdom's last war pair in the process, and every update
  is a restart, so the first war after each start went back to the pair of
  the smallest gap: Tuskaffki and Przelew24 again at 21:36 on 18 September,
  the first war after 2.0.77 went in. `player.playerbot_guild.last_war_at`
  (unix seconds, added by apply.sh) is written at each declaration and read
  once before the first pick (`LoadPlayerBotGuildWarMemory`); a table
  without the column answers with an error and the memory starts empty, as
  before. The first start with it read `war memory loaded guilds=4
  kingdoms=2`: two pairs, seeded by hand from that evening's wars.
- **A counter is three lines of any one thing.** The per-kind caps (a
  material's lines, a heap's, the chests', the scrolls', the counted singles')
  left everything else uncapped, and the lines from before a cap existed
  never came down: on 18 September a counter of m2zip held 46 lines of
  Kawalek Lodu, others ten to fourteen of one hair dye or seventeen horse
  medals, and 10 865 lines stood over three of one vnum ("caly sklep jest w
  matowych lodach", Tieru). `PLAYERBOT_SHOP_SAME_VNUM_LINES` caps every item
  by vnum but the goods counted by kind, a Forgetting Scroll and a marble
  (`IsPlayerBotSameVnumCapped`), in the classic collector, in
  `BotOfflineCounterRefuses` and, one line a visit, in `BotOfflineUnwantedLine`.
- **A level-30 weapon of another class is ground for sale by half its
  keepers.** `PlayerBotRefinesLevel30ForSale` (by the pair, like the anvil
  keep) sends it to the plain anvil as far as the operator's ceiling for its
  line and never under a scroll, and it is listed the moment the next step
  cannot be paid; a counter line of one comes home while it can
  (`CanPlayerBotPayRefineStep`, the purse-and-bag half of
  `CanPlayerBotAttemptRefineItem`, asked of a preview). Worth knowing before
  promising +9 from it: the family runs 90/85/75/65/55/45/35/25/20, so from +5
  under a Blessing Scroll a +9 costs some 207 scrolls on average (59 with the
  +10% scroll, 14 with the no-reduction stone, 0.79% at the plain anvil) -
  the world held 2 453 Blessing Scrolls and one level-30 weapon at +9.
- **A dye from the water is thrown away.** 5 157 counter lines and 3 516 bag
  items of 70201-70206 on 18 September, merchant price three hundred, and
  "wiekszosc ludzi je wyrzuca" (Tieru). `DiscardPlayerBotFishedDyes` (at the
  end of a fishing session and at every merchant) keeps one colour for a bot
  whose hair has none yet (`ManagePlayerBotHairDye` uses it) and
  `PLAYERBOT_HAIR_DYE_KEEP_PERMILLE` by item id for a counter; the rest come
  home from the counters to be thrown. The item shop's dyes are goods.
- **An item-shop hairstyle is a keeper's stock.** One keeper in
  `PLAYERBOT_ISHOP_HAIR_TRADE_SHARE` with nothing else to spend coins on buys
  a head it cannot wear (`PickPlayerBotHairstyleForCounter`) and lists it at
  `PLAYERBOT_PRIOR_ISHOP_HAIRSTYLE`; `WearPlayerBotBoughtHairstyle` and the
  hairstyle wish ask `CanUsedBy`, so the pass that dresses a bot never takes
  the one for sale. None of the 96 heads in this package's shop carries a
  bonus (every applytype is zero), whatever a player remembers of another
  server.
- **The ground nearest a point is the edge of whatever surrounds it.**
  `FindPlayerBotWarGround` took the first open cell in rings from the
  Town.txt point, and on the guild maps whose point is inside the safe zone
  that cell is the zone's own border: fifty units from ATTR_BANPK on
  metin2_map_guild_02 and a hundred on _03 (guild_01's point is open ground,
  450 from it). A bot's spot is the ground and up to 400 units of pid, so the
  war straddled the border - four minutes into the Chunjo war of 18 September
  eleven of sixty-seven fighters stood where no blow lands, with the 2.0.77
  foe filter already in place. The ground now keeps
  `PLAYERBOT_GUILD_WAR_SAFE_MARGIN` (800) from the zone, sampled on rings of
  200 in sixteen directions (`IsPlayerBotWarGroundClearOfSafeZone`), and falls
  back to the old nearest cell only where nothing in reach qualifies; the
  battlefield line carries `safe_margin=`. Measured with
  `scratchpad/war_ground_margin.py` (a BANPK distance transform on the map's
  server_attr): the new ground is about a kilometre from the old on _02 and
  800 units on _03.
- **The monkey curse is a stock quest, and the image no longer carries it.**
  `monkey_curse.quest` (the package's `quest/map_entrance/`) sets a timer at
  every login inside a Monkey Dungeon - 55 minutes on 5, 25 and 45, 35 on 108,
  25 on 109; 107 is on its list with no delay at all - and when it runs out
  turns the character into a monkey (`pc.polymorph(5003, 5*60)`) and warps it
  to its village, unless the herb of that dungeon's monkeys (50057-50059, an
  affect for two hours) is running. No bot ever carried the herb, so the medal
  droppers who live in those dungeons came out as monkeys ("klatwa malp, z
  ktora boty nie potrafia sobie poradzic", SIZOWSKI, 12 September; "usuniemy
  to", Tieru, 18 September). The cores load a quest's handlers from
  `quest/object/<vnum or name>/<event>/` and its state table from
  `object/state/`, so a shareify step deletes the six handler files (login,
  logout, the timer and the three herbs' use) and the timer's directory from
  the image, and fails the build if one is left; the state table stays, so the
  `monkey_curse.time` flags characters already carry still name a quest the
  engine knows, and an empty `use` directory is read as no handler at all
  (`NPC::Set`). The herbs stay in the drop tables: `subquest_39` (levels 55-57)
  asks for the hard one. It is not the Hwang curse, which 2.0.52 took out of
  `char_battle.cpp`. Checked by running the step against the running image's
  own tree - six handlers gone, the state table and the other 211 login
  handlers untouched; not yet built into an image or watched in a world.
- **"Stall" is the counter instead of the merchant, not a counter of nothing
  else.** 2.0.78's caps - three lines of an item, three marbles of three
  monsters - stepped round every item the operator had put on "stall", and the
  test world's own policy file had marbles and Kawalek Lodu on it: the panel's
  example, written by a test on 13 September and never taken out. So those two
  stood 6 735 lines over three of a kind there on 19 September (3 877 marbles;
  2 858 of Kawalek Lodu, 46 on one counter) against 3 343 for everything else,
  and read as caps that did nothing - which is also where "caly sklep jest w
  matowych lodach" came from. Since 2.0.79 the caps hold for "stall" too, in
  the classic collector, `BotOfflineCounterRefuses` and
  `BotOfflineUnwantedLine`: the item still goes up ahead of everything and is
  never junk, so what is over the cap waits in the bag. The test world's file
  was emptied at 00:18 that night (the three lines are in
  `scratchpad/item_policy_m2zip_backup_20260919.tsv` of session 82d3ab90), and
  with 2.0.78's caps then reaching both the marble lines began to fall within
  minutes (6 628 to 6 608 by 00:23). A test that writes the spool writes the
  operator's world: put it back when the test ends, and read the policy file
  before measuring anything a policy can steer.
- **A build runs on the Windows clock.** Docker Desktop's machine takes its
  time from Windows, and apt refuses a Release file dated after that clock as
  "not valid yet". Xewi's Windows ran three hours behind (19 September - the
  launcher's log said 09:09 in a Bucharest zone while Discord stamped the same
  minute 09:10 UTC), so every GRAJ stopped at the panel's apt-get while the
  containers built before still started from Docker Desktop ("z dockera
  dziala"). The panel and the game's runtime stage run apt with
  `Acquire::Check-Date=false` - the signatures are still checked - and the
  game's deps stage does not, because a changed line there rebuilds every
  library on every install that has built once; a fresh install on a wrong
  clock still stops there. The preflight says why:
  `Get-M2InternetClockSkew` (an HTTPS Date header against Windows) and
  `Get-M2DockerClockSkew` (`docker info` SystemTime against Windows) warn past
  five minutes, and `CLOCK_BEHIND` is the error guidance. Read a support
  bundle's clock the same way: the launcher log's local time against the
  Discord timestamp of the message that carried the bundle.
- **Auto Lowy is Colide's window since client 2.0.17.** A player rebuilt it
  for himself and sent it in (19 September): twelve skills in two rows, six
  potion slots each under its own share - of mana when `IsManaItem` says what
  lies there restores mana, of health otherwise, where the old window took the
  first slot for health and the second for mana whatever was in them - six
  items on a clock in seconds, a switch for the attack, the skills, the
  revive, the potions, the items, the stones and the walk back, a share of
  health to wait for after standing up, and the skills cast on their own
  clocks fight or no fight. Settings go to `autohunt/<name>.cfg`; the old
  `autohunt_<name>.cfg` is still read. On top of his file: a file of version 2
  or none moves into the new slots (`ConfigFromOldValues`) where his reset it
  to the defaults, which would have emptied every player's skill slots at the
  update; `IsManaItem` reads the item's table (USE_POTION and
  USE_POTION_NODELAY: value1 without value0, or a blessing's value4 without
  value3) before his list of eleven, because the table knows 27052, the sushi
  and the juice he had not found; the share after standing up is capped at a
  hundred; and a missing item is looked for once a second, not on every frame
  - the search walks every cell of four pages. His layout has no field for the
  delay before standing up; the value stays in the file (15 s by default). He
  is in the README's credits and in the release notes. Client 2.0.19 carries
  his second version: twelve items on a clock in two rows (`USE_ITEM_SLOTS` 18,
  the first six the potions), and the pick-up in a window of its own, "Auto
  Lowy - Lupy", movable, so each of the two fits 800x600. `CONFIG_VERSION`
  stayed 4: the new keys only take their defaults, and a file of 2.0.18 reads
  as it was (`tests/uiautohunt_test.py` checks both, and the two windows' places
  on an 800x600 screen).
- **A crash can zero-fill the .env, and the database's passwords were only
  there.** Greess, 19 September: the machine went down during a client update
  four minutes after the launcher had rewritten .env (it appended new keys),
  and came back with the file's 21 395 bytes all zero - NTFS had kept the
  length and not the data - and the launcher's own log with the same hole at
  the same minute. Compose refused line 1 ("unexpected character \x00"), the
  old launcher appended the example's defaults to a file it could no longer
  read (a fresh panel password among them), and every GRAJ failed on the same
  line. start-server.ps1 now writes .env and .m2install.json durably
  (`Write-FileDurable`: a file beside it, `Flush($true)`, `File.Replace`),
  leaves `.env.last-good` after every identity step that holds both database
  passwords, and `Repair-DotEnvAfterCrash` - asked before .env is read - keeps
  the damaged file as `.env.damaged-<stamp>`, puts the last-good copy back,
  and otherwise reads the values back from the installation's containers:
  Compose put the whole .env into them at their last start, and a start that
  could not read the file recreated none of them (`MARIADB_ROOT_PASSWORD` is
  the root password, `TZ` the zone, the two bind addresses come from the
  published ports; container values win over the appended defaults). It never
  invents a password while the database volume exists. On m2zip, on a
  zero-filled copy of its .env: both database passwords, both panel passwords,
  the bot count, the second channel's settings and the bind addresses came
  back equal (`tests/start_server_env_repair_test.ps1` covers every branch
  with Docker stubbed). An update reaches a broken install: `Update-Server`
  applies the package and then runs the new start-server.ps1 (`-IdentityOnly`)
  before any compose call. And a .NET trap met on the way: `StartsWith` with
  U+FEFF compares culture-sensitively, the character is ignorable, and so
  every string "starts with" a byte order mark - test the first character.
- **The package's player dump brought another server's guild lands.**
  `initdb.d/dumps/player.sql` holds 28 `player.guild_land` rows and 62
  `player.object` buildings from the server the package was taken from, and
  not one `player.guild` row. `building::CManager::FinalizeBoot` stands the land
  agent (NPC 20040) only on a land whose owner is zero, so those lands were
  never for sale, their buildings stood on ground nobody held, and a bot guild
  founded later under one of those ids (2, 3, 5, ...) owned a land and
  buildings it never paid for ("stoja juz budynki, pomimo ze teren nie jest
  zajety", Mat, 19 September; NerrVoVy cleared his by hand in Navicat). The
  migrator takes the dump's exact rows off once (`package_guild_lands_2081`,
  `PACKAGE_GUILD_LANDS`/`_OBJECTS` in migratorify.py) - what the engine's own
  `ClearLand` does - and leaves a land a player bought and the buildings put
  up since (ids past 62). Run in a transaction rolled back on m2coop's
  database: 28 lands and 62 buildings, nothing else.
- **A rendered file edited by hand is reverted by the next render, without a
  word.** mt2009's apply.sh says DO NOT EDIT and is rendered by
  `port/migratorify.py`, yet 6629944 (the guild tiers, the channel pins),
  43ca602 (the difficulty) and the fishing pass and teleport ring lines were
  written into the rendered file, and rendering it for 2.0.81 dropped all of
  them. They live in migratorify.py now. Before committing a render, diff it
  against the file it replaces: the only difference should be the change meant.
- **A dropper takes no trial.** `IsPlayerBotTrialExempt` sits in both "on
  trial" predicates (playerbot_battle_horse.h): GG1249125 and MORDEGAPOTEGA,
  Metin droppers of thirty-six with a horse at ten, read "Zdobywam konia
  bojowego na pustyni (0/100)" on the guild map they farm, and the frontier
  draw pointed them at the desert (Urtopy, 18 September). A report of "bots in
  M3 at 32-39 doing no Biologist" is the droppers' own band - the locks are 40
  (Metin), 36 (M2), 33 (medal) and 30 (M3), plus
  `PLAYERBOT_DROPPER_OUTGROWN_LEVELS` - beside the level-30 weapon hunt, which
  runs to forty; a world of forty bots shows them plainly. The classic panel
  reads "nie dotyczy" for a dropper's Biologist (`BOT_DROPPER_PERSONALITIES`)
  instead of a 0/7 that looks like a bot stuck for good.
- **The tower's pack spreads its blows, not itself.** One demon for sixteen
  bots stopped them dying one by one on the seventh floor and made every floor
  a queue ("atakuja po jednym przeciwniku", Nagash, 19 September).
  `PickPlayerBotTowerObjective` gives each bot one of the ordinary monsters
  nearest the pack by a slot drawn from its pid - about
  `PLAYERBOT_TOWER_BOTS_PER_MONSTER` to each, and only within
  `PLAYERBOT_TOWER_SPREAD_RANGE` beyond the nearest - and keeps it while it
  stands; a stone, or a boss once he is the nearest, stays everybody's.
  Compiled on both engines, not yet watched in a raid.
- **"Cofki" is a character pulled back while it runs, not a rollback in the
  database.** Kiciamol's bundle of 19 September: his character's gold grew
  across every login and no core died; on the chat seban asked "czy cofa cie,
  jak biegniesz" and it did. His world was `unified`, 838 bots and 83 thousand
  monsters (the respawn count at about x2) on game1 alone; the bots' pass took
  16 s of every 60 there with passes up to 127 ms, and the monsters' own AI
  comes on top of that and is in no log line. Ask which one a player means
  before reading a bundle for lost data.
- **A restart that starts a console program and exits in the same breath can
  kill it.** The launcher's restart after an update ran the .bat and closed its
  window at once, and on Windows 11 the new cmd.exe died with 0xc0000142
  (Urtopy, 19 September, the launcher started from its desktop shortcut).
  `Restart-Launcher` keeps the old window up to four seconds, logs the exit
  code of a start that died and tries powershell.exe once more. That fallback
  had always passed `$PSCommandPath` unquoted, which Windows PowerShell 5.1's
  Start-Process joins with spaces - and the install folder is "Metin2
  Singleplayer" - so it could never have started anything. Checked with a
  harness (a .bat exiting 0xC0000142, then a script in a folder with a space);
  like every launcher fix it reaches a player one update late.
- **A pass that claims the tick above the potions has to keep the bot alive
  itself.** `ManagePlayerBotGuildWar` runs above the potions, the emergency
  recovery and `HandlePostDeathRecovery` and `continue`s, so a bot that fell at
  war stood up where it fell (`restart_here`) at a fifth of its health and went
  straight back at its killer, while the enemies swung at a foe that was still
  invisible - which mt2009's `battle_is_attackable` refuses, so the field "stood
  still". Hiob, 19 September: "boty w nieskonczonosc sie bija ... nie wychodza z
  m3 tylko sie bija w miejscu"; his bundle has 141 bots on game1 standing up
  1662 times in three and a half minutes, seventeen the most, after he had
  switched the wars off. `KeepPlayerBotAliveAtWar` runs the recovery and the
  potions from inside the pass, as `KeepPlayerBotTowerAlive` does in the tower,
  and a foe that is recovering (`bRecoveringAfterDeath` or
  `AFF_REVIVE_INVISIBLE`) is no foe for the search or the held target. A bot at
  war still does not break off at `PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT`: a
  kill is the war's score. And the panel's WARS switch ends the bots' part in a
  war under way (`GetPlayerBotWarEnemy` answers nothing, the bots on the ground
  go home by `guild_war_over`); it used to stop the next declaration only, and
  the engine's war ran its half hour with the bots in it. Measured on m2zip,
  with `scratchpad/war_deaths2.py` of session 82d3ab90 (deaths within a radius
  of the battlefield, and how many came within fifteen seconds of standing up):
  the Chunjo war before the change, 2065 deaths of 40 bots in 29 minutes, 80%
  of them within fifteen seconds of standing up, the worst bots 101-116 each;
  the Shinsoo war after it, 721 deaths of 62 bots in 10 minutes, 26%, the worst
  16-18 - a third fewer deaths a bot and half as many for the worst. What is
  left is the fight itself: of those 721, 135 came within three seconds of a
  recovery ending, because every bot takes the nearest foe and a side of 39
  focuses a side of 23. Spreading the blows (the tower's pid slot,
  `PickPlayerBotTowerObjective`) is the next step if a war should look less like
  an execution; the escape walk in `HandlePostDeathRecovery` also left some bots
  where they fell (43 deaths on the spot of the previous one).
- **A bag laid out in one operation is dozens of packets, and the client only
  has to miss one.** `CHARACTER::SetItem` sends ITEM_DEL for the cell an item
  leaves and ITEM_SET for the cell it takes, so `ArrangeInventory` is already
  correct packet for packet - and "w te ktore staly sie puste w wyniku
  sortowania juz nie [moge przeniesc] ... wystarczy przelogowac postac"
  (Dearminder, 19 September) is what it looks like when one of them does not
  land: a relog is the server saying the whole bag again. So the operation ends
  by restating every cell it touched (`RestateCell`, a copy of what the server
  holds - ITEM_SET or ITEM_DEL, never a state of its own) and counts the empty
  cells the engine's own `bItemGrid` still calls taken, which is what
  `GetEmptyInventory` reads and therefore what a safebox checkout, a purchase
  and a pickup all ask - measured before the plan as well as after it, because
  a cell the bag already carried that way is not the operation's doing. And
  measured against `GetInventoryMaxCount()`, which is what `IsEmptyItemGrid`
  itself measures by (`bCell >= MAX_INVENTORY` answers "not empty"): read
  against `INVENTORY_DEFAULT_MAX_NUM` instead, every bot without the full four
  pages reads as having half a page of holes, and the first run of this said
  314 of 566 bags were broken when none of them was. The point you verify must
  be the point the engine samples, for a count as much as for a coordinate.
  Measured right: 601 sorts on the test world and not one cell out of step, so
  the server's grid is sound and the client is what had to be told again. Note the numbering while reading either side: header 20
  is `HEADER_GC_ITEM_DEL` to the server and `HEADER_GC_ITEM_SET` (the short
  struct, no flags) to the client, and 21 is `HEADER_GC_ITEM_SET` to the server
  and `HEADER_GC_ITEM_SET2` to the client - the structures match pairwise, so
  the wire is sound and only the names disagree.
- **A medal nobody may spend is a medal nobody may sell, unless something says
  so.** `CanPlayerBotSellHorseMedals` wanted a level *under* the next horse
  milestone, and a battle-horse candidate - a horse at exactly ten, level
  thirty-five or more - is past it by definition while `CanPlayerBotAdvanceHorse`
  forbids it to spend one (an eleventh level can never be undone). Both halves
  refused, so the bag filled for ever: "10 lv konia, ponad 40 medali w plecaku"
  (Greess, 19 September), and on the test world 37 627 medals in 2 104 bag
  stacks against 5 346 on the counters, 478 bots holding more than two.
  `PLAYERBOT_HORSE_MEDAL_KEEP` (two, for the ladder that starts again after the
  trial) is what stays; everything over it is goods, and
  `GetPlayerBotStallBaseKeep` leaves that much in the base stack so the cut
  lines agree - the medal dropper keeps one, being the medal shop. Any rule of
  the shape "may spend" beside one of the shape "may sell" wants reading
  together: the pair can refuse both ways at once.
- **Exempt from the junk rule is not the same as listed anywhere.** The change
  stone, the add stone and the blessing marble have been kept out of
  `IsPlayerBotJunkItem` since the marble went in - no bot may vendor one - and
  `ScorePlayerBotShopStock` had no branch for them at all, so a bot that found
  more than its own rerolling could spend kept them for ever: 20 387 stones in
  1 475 bag stacks on the test world, **none** on any counter, and 374 bots
  holding more than ten. One player's screenshot had a hundred and ninety in a
  single bag (Nagash, 19 September, "mozna by im chociaz pozwolic wystawiac te
  dodania i zmianki na sklep"). `IsPlayerBotBonusStoneItem` asks the subtype,
  not the vnum - each stone has an ItemShop copy and the green pair for gear of
  forty and under - and `PLAYERBOT_BONUS_STONE_KEEP` is what stays; the rest
  are goods at `PLAYERBOT_SHOP_BONUS_STONE_SCORE`, cut the way every counted
  kind is. When a rule exempts an item from the merchant, read the counter's
  scorer for it in the same breath: between the two there is a bag with no
  bottom, and the medals above are the same shape found the same night.
  **And then the measurement said no counter ever listed one anyway, because
  the engine forbids it:** 71084, 71085 and the ItemShop copies 76023/76024
  carry `ITEM_ANTIFLAG_MYSHOP` (and GIVE), which
  `CollectPlayerBotShopItems` refuses on the first line of its loop, before
  any score is asked - a player cannot stand one on their own counter either.
  The branch stays, written by subtype, so a stone without the flag (a green
  71151/71152) is goods the day it appears. What is left is not a full bag:
  the stones stack, so they are 1.61 cells a bot on the test world and four
  at the worst, and the bots do spend them - 2 285 adds and 1 667 changes in
  two days. Nagash's wish needs the flag cleared in `item_proto`, which is
  the operator's call about the world's economy, not a bug to fix. Before
  concluding that a counter rule does nothing, read the item's antiflags:
  `dwAntiFlags` answers in one query what a day of scoring cannot.
- **The grid audit counted the bottom half of every sword.** `SetItem` writes
  the item's pointer into its top cell alone while `bItemGrid` is marked for
  every cell the piece covers, so "no pointer here and the grid says taken"
  is the ordinary state of a three-cell weapon and a two-cell armour.
  `ArrangeInventory`'s own check counted those as damage and wrote 2 188
  syserr lines about healthy bags. What said the measurement was wrong rather
  than the world: **not one of them differed before and after** - the same
  number of "holes" going in as coming out, on an operation that only moves
  items. `CountPlayerBotGridHoles` marks each item's footprint first and
  counts only what no item above explains. This is the third shape of one
  mistake in this file (the cell centre the navigation samples, the bag size
  `IsEmptyItemGrid` measures by, and now the cells an item covers): measure
  what the engine holds, not what one accessor returns.
- **A render nobody runs is a render that will delete something.**
  `linux-port-mt2009/port/envify.py` writes that line's `.env.example` from
  the r40250 one, and had not been run since 2.0.1 while 18 keys were added
  to the rendered file by hand - the difficulty, the second channel, the
  medal droppers, the world layout, the per-kingdom counts. One run would
  have dropped every one of them, and that file is what a new install's
  `.env` is written from and what `Add-MissingDotEnvKeys` tops an older one
  up from, so those settings would have quietly stopped reaching anybody.
  It refuses to write while the render would lose a key and names each one.
  A generator that has fallen behind its output is more dangerous than no
  generator: check what it would produce before running it.
- **A channel nobody can reach is a channel that is running.** Channel N
  listens on 13000+10*(N-1)..+2 inside the container and compose publishes
  `M2_GAME_PORT_RANGE` onto `M2_GAME_CONTAINER_PORT_RANGE`, so with the second
  channel on and the range left at 13000-13002 the cores are up, the bots play
  on CH2 and nothing outside the machine can log in to it: "Boty graly na ch2
  lecz ja nie moglem sie logowac" (GoracyDelfin, 19 September), fixed by hand
  in `.env`. Only the Windows launcher ever widened the range, so a Linux host
  - and anyone who used the panel's own switch, which writes a wish the game
  container reads at its next start - had CH2 unreachable. `sync_channel_ports`
  (update.sh) sets both ranges from the channel's state before compose runs,
  reading the panel's wish out of the running container when there is one; a
  published port only changes at a recreate, so it has to be before.
- **Split is the level wall, and it was a switch almost nobody knew of.**
  `M2_PLAYERBOT_WORLD_LAYOUT` has offered `unified` since 2.0.30, and on
  19 September players were passing each other screenshots of the line to paste
  into `.env` by hand while Shinsoo and Jinno stopped at thirty-six
  (Hiob to LIGI VAN ASTREA; Iwakura: "unified powinno byc domyslnie tbh"). It is
  the default since 2.0.85, for a new install and - once, the way the three
  kingdoms were - for one that already stands: `Assert-WorldLayoutDefault`
  (start-server.ps1) and `migrate_world_layout` (update.sh) write it with
  `M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=1`, so a later `split` is the operator's
  and is kept. A world asking for more than 1500 bots is left on `split`: one
  core carrying everything was measured at 9.4 s of every 60 at that size.
- **An open safebox blocked every move in the bag, and its packets carry no
  count.** blasty's proposal of 19 September (Tieru: "Jasne"): the bag's
  "Scal i uporzadkuj" for the safebox, and a stack split or poured across the
  two windows. Three things stood in the way. `CHARACTER::MoveItem` asks
  `CanHandleItem()` with the default exclusion, and an open safebox is busy to
  `IsBusy`, so with the safebox open the server silently refused every move,
  split and merge inside the bag - `apply_safebox_hands` (playerbotify)
  excludes the safebox there, and the bag's own "Scal i uporzadkuj"
  (`ArrangeInventory`) lets it through the same way; every other busy state,
  the item shop included, still refuses. The client's safebox packets
  (checkin, checkout, item move) name cells and no count, and a drop from the
  bag onto a taken safebox cell was a commented-out packet; so a part of a
  stack, and a stack dropped on the same item, go as `/safebox_put`,
  `/safebox_take` and `/safebox_move` with a count (one `do_safebox_transfer`,
  the subcommand is `playerbot_arrange::ETransferOp`), a whole stack onto a free
  place still goes by the packet, and `/safebox_arrange` is the button in the
  safebox's title bar (client-root/safeboxtransfer.py, answered
  `SafeboxArrangeResult` and `SafeboxTransferResult`). The work is in
  `playerbot_arrange.cpp` beside the bag's own arrange, and the decision for a
  transfer is `playerbot_arrange_rules::PlanTransfer` (unit-tested). Third, the
  package's `ENABLE_MT2009_DISABLE_SAFEBOX_STACK` stays on, and for a reason
  worth knowing: `CSafebox::Remove` and `Add` refuse while the owner is
  `IsBusy(BUSY_SAFEBOX)` - which includes browsing the item shop, a state
  `CanHandleItem`'s default lets through - and the old stacking path destroyed
  the item `Remove` had just refused to take out, leaving a freed item in the
  box. `SafeboxHands` asks `CanHandleItem(false, false, BUSY_SAFEBOX)` before
  the first change. Two engine facts the code is shaped round: the db core
  writes the SAFEBOX window straight to the table (`QUERY_ITEM_SAVE` caches
  every other window) and `QUERY_SAFEBOX_LOAD` reads the table, so a count
  changed in the box goes out with `FlushDelayedSave` and whatever changed in
  the bag with `FlushRow` (checkout's own HEADER_GD_ITEM_FLUSH); and
  `CItem::SetCount(0)` on a safebox item only clears its owner, because
  `RemoveFromCharacter` leaves the box's pointer and grid alone for that
  window - an emptied safebox stack is `CSafebox::Remove`d and then destroyed.
  The ITEM_UPDATE a safebox item's `SetCount` sends is dropped by the client
  (`IsValidItemPosition` says no to SAFEBOX), so every count change is
  followed by `CSafebox::Refresh`. And the safebox's grid is one `CGrid` of
  five columns and nine rows a page with no page edge in it, so a two-cell
  item may stand across two pages: `MakePlan(items, pages, false)` reads the
  box by that rule (`ValidGrid`) and lays it out inside the pages. The bots on
  mt2009 run `ArrangeSafebox` at the end of every safebox visit in place of the
  sixteen merges a visit, which is what puts that code through hundreds of
  boxes on the test world. The transfers no bot makes were checked on m2zip by
  a self-test built into the deploy copy only (`make_selftest_town.py` of
  session 82d3ab90): on six bots' real boxes, twelve steps each - a part put
  in, poured onto, split and poured back inside the box, taken out onto the
  bag's stack and onto a free cell, whole stacks moved every way, a move onto
  itself and a take from nothing refused - with the units of the kind in bag
  and box counted after every step: 72 of 72, every bag stack back at its
  count, and the same six boxes then arranged (10-43 moves each) with nothing
  in syserr. The engine's own pulses (`SafeboxCheckInOut`, 250 ms) refuse a
  second transfer in the same tick, so a test like that has to
  `PulseManager::ClearClock` between steps. When adding a clientrootify edit, anchor it where
  no other edit's new text runs: the first version put the safebox handlers
  in game.py right after the bag's handler, split the text by which the older
  edit knows it has been applied, and a second run added the bag's handler
  again - the idempotency check (copy client-root, render it with `--root` on
  the copy, `diff -r`) is what caught it, and it overwrites client-root, so
  render from the published root again afterwards.
- **An offline shop runs on two clocks, and only the db core's may end it.**
  This core counts each shop's minutes down on its own pulse event
  (`func_offlineshop_update_duration`) and the db core counts the same minutes
  on its own; the db core's count expires the shop and refuses a create while
  its own copy has time left ("it already exists", with no answer at all). A
  loaded core's pulse clock runs fast - `heart_idle` counts
  `floor(elapsed / pass) + 1` pulses whenever a loop overruns, and game1 at a
  thousand bots measured 26.6 pulses a second against 25 - so a shop reached
  zero here long before it did there. In that stretch the entity stood on the
  map and a click did nothing ("sklepy WIDMO w ktore nie da sie kliknac",
  Iwakura), and a renewal asked for in it took the fee and was refused without
  a word ("sklep nie daje sie ponownie otworzyc", ElGrande): 368 bot renewals
  on m2zip from 17 to 20 September. `apply_shop_clock` (playerbotify) keeps
  this core's count at one minute or more - zero is the db core's word alone,
  and it arrives with the expiry packet - and a renewal still unanswered after
  `PLAYERBOT_OFFLINE_RENEW_ABANDON_MS` is dropped and asked again
  (`PLAYERBOT_OFFLINE: renewal went unanswered`): it carries no goods, so
  nothing can be doubled, where a create is still never retried.
  Any duration the two cores both count wants one owner.
- **`AFFECT_EXP_BLOCK` takes an attacker out of the whole kill, not just its
  own share.** `CHARACTER::DistributeExp` skips a blocked attacker in the
  damage map altogether, so its blows count for nobody - and the Grinder's
  locks hold their bots under that affect. A player in a party of such bots
  got nothing from what they killed until the player's own blow put him on the
  map ("W PT - NA ROWNYM nie leci exp", SIZOWSKI).
  `apply_party_exp_of_blocked_members` counts a blocked member's blows for its
  party while somebody in it can take experience; `PointChange` still refuses
  the blocked member's own share, and a party of nothing but blocked members,
  or a blocked attacker alone, is left out as before. char_battle.cpp ships
  staged.
- **A switch read in a loop of sliders has to `continue`.** The classic
  panel's `read_ai_weights` handled WARS in its own branch and then fell
  through to the slider branch below, which clamped it to 25: the page showed
  the wars on after every reload and the next save of anything wrote WARS 1 -
  so switching the wars off from the panel never held, and "bots still fight
  with the wars off" was the panel. Every non-slider key in that loop ends its
  branch with `continue`; check a new one the same way.
- **The stock level-85 Biologist row counted the level-80 row's item.**
  `collect_quest_lv85` (Czerw. Konar Duchodrzewa, 30167) asked the bag for
  30166 at the hand-in, so a player holding a hundred and sixty red branches
  was sent away at every visit with nothing taken (! AmaZing !, 21 September).
  Our copy in `linux-port-mt2009/docker/game/quest/` changes that one line and
  compiles in the Dockerfile loop over the stock objects. The other rows'
  hand-ins were read beside it and ask for their own items.
- **A quest item in a special item group is used under the group's number.**
  `special_item_group.txt` puts the Teleport Ring (70058) in the quest group
  10031 ("Type Quest"), so `ITEM_MANAGER` gives every ring SIGVnum 10031 and
  the use goes to `CQuestManager::SIGUse(10031)`, which looks for a `sig_use`
  handler under that number and never under the item's own. `when 70058.use`
  compiled, sat under `object/70058/use` where nothing looked, and the ring did
  nothing from 2.0.48 to 2.0.94 - through two diagnoses in this file that read
  the flags and the use path and never the group. Mur4s found it ("FIX
  PIERSCIONKA TELEPORTACJI", 22 September) and NerrVoVy confirmed it:
  `when 10031.sig_use`. Before writing a `use` handler for an item, grep
  `special_item_group.txt` for its vnum.
- **A schedule that sends a cohort to rest sends it all at once.** With the
  LIFE switch on every bot starts with the core, every first session ends
  inside the same six hours, and a rest is three to nine: at six hours nine
  bots in ten were resting and one in ten was back - Kuszaa's chart of 21
  September, 500 a kingdom at 14:30 and 40 at 20:40 ("boty poszly na odpoczynek
  ale z niego nie wracaja"; they were coming back, too few at a time).
  `PLAYERBOT_LIFE_MAX_RESTING_PERCENT` of the scheduled bots rest at most, and
  a session that ends past it plays on `PLAYERBOT_LIFE_HOLD_MIN_MS` to `_MAX_MS`
  more, by pid; the census carries `held_on=` and `cap=`. Worked out, not yet
  watched: m2zip runs with the switch off.
- **A war fought with skills alone is a war of bots standing still.** The
  basic swing knew only the duel's opponent, so a guild war and every Anti-PK
  fight back were fought with skills and nothing between them ("boty jedynie
  uzywaja umiejetnosci, nie autoatakuja, nie biegaja", prodnathin; Dixdros'
  "PVP Bots"). It takes the war's foe and the Anti-PK foe now, and
  `battle_is_attackable` still has the last word on every blow. And the war
  chased the nearest enemy anywhere on the map, so a foe who walked off after a
  death drew its enemies up the slopes and onto the Shinsoo map's bridge and
  cliffs, four kilometres of fight with bots in the rock faces:
  `PLAYERBOT_GUILD_WAR_FIELD_RADIUS` round the ground the sides were found on
  is the field, a bot off it walks back before it looks for anybody, and a foe
  off it is nobody's target.
- **Encode what a tool writes before the file is opened for writing.** An edit
  script opened `playerbot_lpp.h` for writing and then failed to encode a
  Polish letter in its replacement text: the file was left at zero bytes, and
  only `git checkout` brought it back. The session's helper encodes first,
  writes a temporary file and `os.replace`s it, and refuses non-ASCII text for
  an overlay file (the convention is ASCII there anyway). Any script that
  rewrites a source in place wants the same order.
- **A wait must be one the pass that ends it will end.** Community Patch 2's
  swap rule holds a new piece in the bag while a stone could lift its lines
  past the worn one's, and the first version asked only whether the bag held a
  stone. The bonus pass's loop for the held piece asks
  `CanPlayerBotRerollItemFor`, which refuses an armour under
  `PLAYERBOT_BONUS_MIN_REFINE` - so a bot that bought a +0 armour from the
  merchant with a stone in its bag would have kept it there for good.
  `PlayerBotHoldsBonusStoneFor` asks the loop's question first. Found by
  reading the throttled `new piece waits in the bag` line on m2zip before
  anything shipped; the same shape as "the planner and the pass that acts must
  ask one function".
- **A medal dropper's lock is its dungeon's, and the status file has to say
  so.** The exp lock held Community Patch 2's medal droppers at
  `PLAYERBOT_EXP_LOCK_MEDAL_DROPPER`, while `ManagePlayerBotAdvancement` went on
  treating them as Grinders: the status file and both panels showed tier locks
  of 15, 18 and 25 for bots levelling to 33, and the Law of Advancement could
  have made one a Conqueror in the Monkey Dungeon. `GetPlayerBotPersonaLockLevel`
  answers for them (`GetPlayerBotMedalDropperLock`, the operator's cohort at its
  own level), writes the lock down only once it holds - as a Grinder's is - and
  the law leaves them alone.
- **A bot's counter has no padlocks.** A shop's right half opens row by row as
  a player unlocks it (`SHOP_SLOT_UNLOCK_PROGRESS_FLAG`, rows 0-3) and with the
  premium (rows 4-7, which every bot holds); a bot never unlocked anything, so
  its counters showed the padlocks and used half the grid.
  `apply_bot_shop_slots_unlocked` answers `GetShopUnlockedProgress` for a bot
  descriptor with every lockable row open - the offline shop takes it as its
  `unlockCount` at the next add, the classic stall through
  `CanPlaceOnShopSlot` - and a player's progress is his flag as ever
  (Iwakura's idea, on SIZOWSKI's condition that it is the bots' alone).
  char_shop.cpp ships staged.
- **A bot's stand stands in a first village.** Kuszaa's reasoning, Iwakura's
  idea: the players shop in the first villages, so the stands in the second
  were counters nobody visits. The `SHOP_M2` key (off by default, a tick box
  on the classic panel's AI page) is what lets them stand there again;
  `IsPlayerBotShopMapAllowed` refuses the open pass anywhere else, and an
  expired stand in a second village is renewed on its owner's first-village
  ring at the next service visit (`moved_from=` in `PLAYERBOT_OFFLINE: reopen`) -
  a renewal stands where its owner stands (`OpenOfflineShop`), so the owner is
  served there instead of being walked back. Nothing moves a running stand, and
  nothing touches a player's.
- **The apprentice chest is the world's choice, asked with the rates.**
  seban latino's idea: `M2_STARTER_CHEST` (launcher: a question in the text
  menu and a tick box in the new-world dialog, `-StarterChest`) reaches the
  migrator, which writes the event flag `m2_starter_chest_off`, and
  `starter_chest.quest` gives nothing while it is raised. A character that
  logs in meanwhile is not marked, so it gets its chest on a later login if
  the chest is switched back on before it passes level five. A change is live
  from the next start, like the difficulty.
- **During a rate event the base flag is the operator's number.** The event
  keeps the live flag at the boost of the base; a rate saved from a panel
  during an event wrote the live flag, the core put the old boost straight
  back over it, and the page read the base - so the save looked lost and did
  not survive the event either ("zmiana rat nie dziala ... teraz nie mozna ich
  zmienic mimo ze zaden event nie jest wlaczony", Monek, CarloMontana). Both
  panels write the base while an event runs, the event's end returns the live
  flag to the base whatever it holds, and a base left by an event that ended
  while the core was down is cleared half a minute after the leader's start
  (`PLAYERBOT_EVENT: stale base cleared`).
- **A backup must survive the table it is backing up.** `log` is history
  alone and is where a crashed MyISAM table lives, and its dump failing failed
  the whole backup and the world reset behind it ("Zrzut bazy 'log' nie
  powiodl sie", uxietoszef). It is dumped again with `--force` past what cannot
  be read and, if even that fails, left out; the backup's README names what
  was skipped. Any other database is the world and still stops the backup.
- **The price pump waits for the answers before it calls anything a miss.**
  Client 2.0.24's `shoppricepump.py`: an edit is a round trip through the db
  core, which on a world of a thousand bots took longer than the second and a
  half it was given, so every line was sent three times and the chat reported
  failures for prices that had all changed on the first click (blastyw). A
  line is looked at again every `CHECK_EVERY` for up to `PATIENCE` before it
  is sent again (`tests/shoppricepump_test.py`).
- **A visit resumed after a party can start on the wrong side of Joan's
  wall.** A town visit is paused, not ended, while the bot is in a person's
  party, and the bot goes where the person goes - so a visit begun inside the
  wall resumed at the weapon merchant with the bot out by the fields: the leg's
  goal was moved onto the bot's own side, "arrived" (`nav_out=2`) and the phase
  never moved until the watchdog (Sammy Suricate's world). A bot outside the
  wall in an inside phase crosses the gate again
  (`PLAYERBOT_TOWN: outside the wall in phase`).
- **Jinno's guild map is two pieces.** Its south-east fifth - 111 536 of
  570 456 open cells, fifteen spawn groups - has no way in from the Town.txt
  point, and the hub there was planned "unreachable" by every bot that drew it
  (Champion, urtopy's world). It is replaced by the main piece's spawn point
  furthest from the other nine. Check a hub's component, not only its cell.
- **The FISHING slider reached the villages only.** The Rybak's roll was
  read in the two villages and every bot old enough for the rod lives on the
  frontier, so 200% moved a handful of bots of thirty to thirty-five and
  nobody else ("ustawilem 200%, a liczba rybakow nie rosnie", blasty). A
  frontier bot whose roll says fish (`IsPlayerBotRybakNow`) goes home to the
  bank now; a trial and a party are not interrupted for it.
- **The client's clock starts again at every warp.** `app.GetTime()` is
  `CTimer`'s, and `CTimer::SetBaseTime()` runs in the handshake of every
  connection (`PythonNetworkStreamPhaseHandShake.cpp`), which is every
  teleport. A "next allowed moment" taken on the map before is therefore far
  in the future on the next one: the quick pickup (`) waited as long as the
  character had played before the warp, so it "worked only in M1"
  (GorącyDelfin, 22 September). `client-root/clientclock.py` is a clock that
  only goes forward (an offset added whenever `app.GetTime()` jumps back), and
  every hand-written module that keeps a time asks it: pickupnearby,
  inventoryarrange, safeboxtransfer, autostackpump, shoppricepump,
  playerbot_status_tail and uiautohunt. `app` is imported inside `Now()`, so a
  test's stub module can be put in place after the import. A new module that
  waits for anything must use it too.
- **A bot in a person's party hunts round the person.** The target search
  reaches `PLAYERBOT_SEARCH_RANGE` (6000) from the bot and
  `ManagePlayerBotFollowHumanLeader` walks only a bot with nothing to do, so a
  bot that always found its next pack was never idle and drifted off for
  good - SIZOWSKI's Shamans that buffed nobody. `IsPlayerBotTargetOffHumanLeader`
  refuses a monster further than `PLAYERBOT_PARTY_HUMAN_HUNT_RANGE` (2500)
  from the leader, both for the held target and for a new one, unless it is
  hitting the bot. Not watched with a person in a party; the test world has
  none.
- **An M3 visit that runs out without the weapon closes M3's door for 45-90
  minutes.** The exit sent a bot to M2 and the M2 branch's `wantsM3` sent it
  straight back ("m3_visit_complete" and "level30_weapon_to_m3" two seconds
  apart, Champion on urtopy's world), so its other errands never ran and its
  status read the planner's goal. `TPlayerBotAIState::dwM3RevisitAfter`
  (`PLAYERBOT_M3_REVISIT_WAIT_*`, drawn by pid) is set on such an exit and
  read by `wantsM3`; the M3 dropper is exempt, because M3 is where it lives.
  `PLAYERBOT_WORLD: m3 visit ran out without the weapon ... back_in_min=`.
- **Auto Lowy 2.0 is Colide's (22 September), with our fixes put back.** His
  window replaced ours; `scratchpad` merge of session 82d3ab90 re-applied what
  his copy of an older file had lost: the config migration from every older
  format, mana potions by the item table, the revive share capped at 100, the
  clock above, and a missing potion or item looked for once an interval
  instead of on every frame (four pages of cells). New in his protocol:
  `/autohunt_target <range> <stones> <dx> <dy> <mobs> <bosses> [<skip>]` -
  the server takes the categories only when a sixth field is there, so an old
  client's skip VID in the fifth is never read as a switch
  (`apply_auto_hunt_categories`); a boss is `MOB_RANK_BOSS` and up and goes
  before a stone only when the window asked for bosses. Three client
  functions (`clientify apply_auto_hunt_circle`): `player.IsBowEquipped`
  (`IsBowMode` of the main instance - reach 2400 against 200),
  `player.IsTargetDead` (the corpse is dropped and skipped for five seconds),
  `player.SetAutoHuntRangeCircle(range[, x, y, anchored])` (a 120-segment
  circle on the terrain, drawn after the characters, render states saved and
  restored). The script asks for each with `hasattr` or under
  `AttributeError`, and without `IsBowEquipped` falls back to the race and
  skill group, so a root on an older exe still hunts. Two of his decisions
  that look like bugs and are not: an "AutoHuntTarget 0" answer keeps the
  target in hand, and a new target is refused while the one in hand is being
  walked to beyond reach+200 - switching on every answer is what turned the
  hunter back and forth. And the target is marked on one pass and the attack
  key pressed on the next (`GetTargetVID` equal to the target), which the
  tests step twice for. Settings: `autohunt/config.cfg` (window positions,
  saved on "Zapisz"), `autohunt/postacie/<name>.cfg` per character, and
  `autohunt/<name>.cfg` (2.0.17-2.0.24) or `autohunt_<name>.cfg` read until
  the first save. `tests/uiautohunt_test.py`, 59 tests on 2.7 and 3.
- **The launcher's window is a layout file over the old controls.**
  `Metin2-Launcher-GUI.Layout.ps1` (22 September, prepared outside this
  repository as a UI test and brought in here) is dot-sourced by the GUI
  after every Click handler and before the timers. It builds no action of its
  own - it moves the existing controls into a menu of five pages - so the
  handlers and `Start-LauncherAction`'s guard are untouched; the one new
  button opens the classic panel's `/rates`. The background
  (`Metin2-Launcher-GUI.Background.png`) is drawn by a panel compiled with
  `Add-Type -TypeDefinition`, the first C# the launcher ever compiled: an
  antivirus or a locked `%TEMP%` can refuse csc, so a refusal falls back to a
  plain panel with the picture zoomed. Anything that fails after the form
  was cleared (`$script:ui.Cleared`) writes `.m2launcher-classic-layout`
  beside the launcher and stops with a message, and the next start opens the
  plain window - a launcher that cannot open is a launcher that cannot
  update. Without the layout file the plain window opens too, so a 1.x
  package carrying the new GUI alone is harmless. Test it with
  `powershell -STA -File Metin2-Launcher-GUI.ps1 -UiSelfTest -UiLanguage pl
  -UiTestOutput <dir> -ServerRoot <an installation>`: the window is built off
  screen, every page is drawn at three sizes into `<dir>`, and the process
  ends before any timer, Docker question or action. The window is 1280 x 820
  clamped to the primary screen's working area, and the pages scroll.
- **"Bots never go back to town" is a question of the world's rates.**
  SIZOWSKI (22 September) saw it on a world at ordinary rates with the four
  inventory pages; on m2zip (yang 3000%) the same hour had 888-921 distinct
  bots a core visiting a town merchant and some 4 000 market trips, almost
  all of them ending `trip over reason=nothing_on_offer`. What sends a bot to
  town is mostly the bag (`NeedsPlayerBotCriticalTownServices`' soft half at
  45%, `IsPlayerBotBagFull` at 80%), both measured against the whole bag, so
  the fourth page doubled the time between visits wherever drops are slow.
  Not changed yet: measure on an ordinary-rate world before moving either
  threshold, because on a fast one the visits are already constant.
- **A whisper to a bot is a conversation now, and it is ĹŌŞƬĒĶ's (l0st3k).** Their
  "chat bots v1.1a" (23 September) came as a whole game/src against 2.0.94;
  against the package only fifteen files differed, none of them an engine
  file, so it went in as ten headers, their test and three small hunks in the
  manager and the trade layer. The shape to keep: `playerbot_conv_*.h` is
  pure (no engine types) and bounded - 4096 pairs of person and bot, six
  queued lines a pair, four remembered turns, 256 bytes of input, a pair
  forgotten three hours after its last line - and `tests/playerbot_conversation_test.cpp`
  runs it (505 checks since 1.1b). `playerbot_chat_conversation.h` is the only file that
  knows the engine: it looks the bot and the person up by pid every time it
  answers and never holds a character across ticks, sends the raw whisper
  packet, and runs its own event only while a reply is waiting (it returns 0
  and forgets the pointer when the queue empties). The trade layer answers a
  "Kupie"/"Sprzedam" line as a shout, as before, and hands everything else -
  and a trade line inside the eight-second trade clock, which used to vanish -
  to `HandlePlayerBotConversation`. The layer reads the AI and writes nothing
  to it. **On the 2.x line a bot's counter is an offline shop, so
  `GetMyShop()` answered "no stall" for every bot**; 1.1b (the same day) has
  one reader for both kinds, `GetPlayerBotStall` in `playerbot_chat_trade.h`
  (the classic stall's `vecShopOffers`, or `ikashop::GetShopByOwnerID` - its
  lines, prices and spawn), which the shout answer, the whisper fallback, the
  snapshot and the price question all ask. In the snapshot `shopOpen` means a
  stall of either kind and `shopStanding` the classic one the bot stands
  behind - a line that says "I am standing here" must ask the second. Item
  names go through `playerbot_conv::ExpandItemQuery` everywhere, so an alias
  means the same thing in a whisper and on the shout channel. What we added
  to 1.1b: the free cells are the item grid's, not the empty pointers (their
  build still counts pointers - put it back at every merge); a two-letter
  shout query is refused unless it is a dictionary word ("Kupie KK"); the
  "Sprzedam" path asks the dictionary too; a named sum is capped before it can
  overflow; and three things the live test found. **An alias word inside a
  real name**: 1.1b dropped the query as typed whenever one of its words was
  an alias (so "morelek" would not stem into "Moreli"), and "szpon" is an
  alias, so "Kupie szpon wilka" went unanswered beside a counter selling
  Szpon Wilka - the query as typed is kept now with its alias words marked
  '=', which `ItemWordsMatch` wants whole (`HasWholeWord`). **"Kupie ksiega
  X"** where X is no skill is searched as a name ("ksiega misji"), where it
  was dropped without a log line. And "gdzie" is a skip word of
  `ExtractTradeObject`, or "gdzie masz stragan?" searched the stall for an
  item called "gdzie masz stragan". Left to the author, measured with the
  self-test: "ksiega misji", "marmur polimorfii", "kamien duchowy" and
  "ku strach" land in the skills, Metin and fear topics, and "dam 500k za X"
  has no rule. Runtime switches are files in the core's working
  directory, read every thirty seconds: `playerbot_conv_debug` (the
  `PLAYERBOT_CONV`, `_QUEUE`, `_REPLY`, `_INITIATIVE` lines) and
  `playerbot_conv_noinit` (no bot starts a conversation). `PLAYERBOT_CONV_STATS`
  every ten minutes is the census. A bot descriptor drops every packet
  (`DESC::Packet` returns for `m_bBot`), which is what makes a server-side
  test possible: a variant with a self-test that has one bot whisper another
  (`make_selftest_conv.py` of session 82d3ab90) runs the whole engine side on
  live states with nobody logged in. Measured with it on m2zip on 23
  September at about 850 bots: 885 lines in twelve minutes, bursts of six
  among them, 542 replies (130 of them merging several lines), one line a
  bot began itself, no core died, and the tick at 7.0 s of 60 against 6.5 to
  6.9 before. Never watched with a person whispering: the test world has
  none, and the author runs it on their own.
- **A full package is built from an installation, and an installation keeps
  secrets beside its files.** The 2.0.71, 2.0.85 and 2.0.86 full packages
  carried the test client's saved logins (`Klient/cache/credentials.json`,
  the login window's "save account": passwords XORed with a key made from the
  machine's MAC address and base64'd, so readable by anybody who guesses the
  MAC), and 2.0.85/86 two copies of the test world's `.env`
  (`.env.last-good`, `.env.bak-2.0.39`: the database, panel and session
  passwords). The builders left out `.env` by its exact name only, and the
  client's `credentials.json` only in the repository's script - not in the
  session script the packages were actually made with. Both now leave out
  every `.env*` but `.env.example`, the client's saved logins, its COOP entry
  (`coop.cfg`), Auto Lowy's settings and the title switch, and both refuse a
  package whose finished staging or zip still holds a file of that shape
  (the scan in `linux-port-mt2009/tools/New-M2FullPackage.ps1`). Check a
  full package's names against that shape before it goes anywhere. Found
  answering Kordyl13 (23 September), whose VPS had followed the 1.x
  `installer/install.sh` into `/opt/metin2/stack` and stopped at
  `COPY schema/`: `docs/INSTALL.md` now opens with the 2.x line (the package
  and `sh linux-port/tools/update.sh`), and the 2.x `.env.example` says what
  the code does - on this line the classic panel opens without a passphrase
  on any address while `M2_PANEL_LOCAL_ONLY` is empty, and the advanced one
  starts with its own switch off, so a VPS keeps both on 127.0.0.1.
- **A language pack is code the client runs: every text is a format, and the
  loader cuts the last character of every line.** "Nothing happens when I put
  the item on the blacksmith, and then the whole inventory is bugged" (JFK and
  zhask9431, 23 September) was REFINE_COST "Kosten: %d Yang" in DE, ES, IT,
  PT, RO and TR against `NumberToMoneyString`'s "1.000 Yang":
  `RefineDialogNew.Open` raised before `Show()`, while the server had entered
  refine mode (`SetRefineMode`) the moment it sent the dialog - and
  `CanHandleItem` refuses every move, drop, use and gift under it until the
  client answers with a refine or the cancel (255, 255), which a window that
  never opened cannot send. Relog was the only way out. Three layers now:
  `port/localeify.py` renders the fixed texts into `client-locale/locale/<lang>/`
  and refuses to write while any text of any language fails to format with the
  arguments its Polish counterpart takes (89 did, in seven languages - the
  yang pickup, the screenshot line, the item shop's buy button, the stat minus
  tooltips, the guild's dragon ghost, a fish's length, the party skills, and
  "%,0f", which is no conversion at all); clientrootify's `uirefine.py` sends
  the cancel when `Open` raises, so the next bad text costs a window and not a
  bag; and `apply_refine_abandoned_session` (playerbotify) ends a session whose
  refine NPC is gone, on another map or beyond the 2000 units
  `CInputMain::Refine` allows, the first time `CanHandleItem` is asked - so a
  player with an old client walks away from the blacksmith and has the bag
  back. A scroll's session has no NPC and is untouched. Two more things the
  same check found. `localeinfo.LoadLocaleFile` takes `line[:-1]`, and the
  pack's `readline` hands the last line over without a newline when the file
  does not end in one: es/locale_game.txt did not, its last type "SNA" came out
  "SN", the loader raised and a Spanish client stopped with a message box
  before the login. And `english_gui.GAME` went on with `globals().update`,
  which put plain strings over typed lines (SNA/SA are functions the scripts
  call), so `WHISPER_ERROR[mode](name)` was "'str' object is not callable" in
  English; the overlay keeps a typed line a function now. The locale pack is
  rendered from the last published one: extract it, run `localeify.py
  --locale <dir>`, repack with `client-locale` as the overlay, and run
  `tests/client_locale_loader_test.py <client-root> <extraction with
  client-locale over it>` under Python 2.7 - it loads every language through
  the client's own loader and formats the call sites the root formats.
  NPC names over the heads are not the client's to translate: the server sends
  an NPC's name (the Polish `mob_proto`) in the character packet, and only
  monsters are named from `locale/<lang>/mob_names.txt`, which is why an
  English player sees "Kowal" and a "Blacksmith" nobody reads sits in the EN
  pack. Naming NPCs from it would be an exe change.


## Engine facts worth not re-deriving

- Item types/subtypes live in `common/item_length.h`; map attributes and
  `SECTREE_SIZE`/`CELL_SIZE` in `game/src/sectree.h`.
- **Mining is entirely in the engine and entirely absent from this world.**
  `mining.cpp` holds the eighteen-row ore table (vein 20047-20059 and
  30301-30305, raw 50601-50618, smelted 50621-50638 - ebonite is vein 20054,
  raw 50608, smelted 50628), `CHARACTER::mining(vein)` is the only entry point,
  the pickaxe (`ITEM_PICK`, Kilof 29101-29110, LIMIT_LEVEL 30) must be in
  `WEAR_WEAPON`, and a swing is one event of `2 * number(5,15)` seconds rolling
  20% plus the pick's grade. But **no map here spawns a vein or an alchemist** -
  checked across all 109 - so `playerbot_mining.h` places and maintains them
  itself. Two engine rules govern that: `SpawnMob` refuses a vein on
  `ATTR_BLOCK` (and only there - an ordinary NPC is also refused on
  `ATTR_OBJECT`), and a vein kills itself after 7-15 minutes
  (`kill_ore_load_event`), so the sites are swept once a minute. The ore drops
  on the ground with fifteen seconds of ownership; the ordinary loot pass takes
  it. Smelting is `mining::OreRefine` - a hundred raw for one piece, from an
  alchemist's quest - and is reimplemented rather than called, the way
  `CollectPlayerBotBattleHorse` reimplements the stable keeper's.
- **The Cube is a GM command, and a quest cannot open it for a player.**
  `cube.cpp` reads `share/locale/<lang>/cube.txt` at boot
  (`LocaleService_GetBasePath()`, `Cube_init`, also re-read by `/reload`) and its
  format is `section / npc / item / reward / percent / gold / end`, with `percent`
  a flat 1..100 roll and the materials spent before it. But the only way in is
  `ACMD(do_cube)`, whose row in `cmd.cpp` is **GM_IMPLEMENTOR**, and a quest's
  `command("cube open")` goes through the same `interpret_command`, which answers
  "This command does not exist." below that level - so a quest that works for the
  operator works for nobody else. There is no lua entry point at all. This world
  ships 106 sections on four NPCs (20017, 20018, 20022, 20383); every other cube
  NPC, 20091 included, is either absent from `npc.txt` or commented out there, and
  `Cube_open` refuses an NPC no section names. Two things to check before promising
  a recipe works: `Cube_make` calls `new_item->GetID()` with no NULL test, so a
  reward vnum `item_proto` does not know is a core crash on the first success; and
  `AutoGiveItem` drops the reward on the ground when the bag is full, after the
  materials are gone. `share/` is baked into the game image, so both `cube.txt` and
  `npc.txt` come back at every update - the same shape as the language switch.
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

## COOP (the Patreon testers' since 2.0.80, behind a password)

Playing the host's world with friends over the Internet: the host runs the
stack, a friend runs only the client. Built on 19 September and shipped the
same day in an ordinary release, at the operator's choice: every install
carries it, the hosting half asks for a password that only the patrons get,
and the changelog and the devlog say one sentence about it - "nie
upublicznimy innym dopoki to nie bedzie dzialac". Say no more than that
anywhere public until the operator does.

- **The server needs no change; the client does.** The server names one
  address for the game cores - PROXY_IP, which is M2_PUBLIC_ADDRESS and
  127.0.0.1 on every install - in the LOGIN_SUCCESS character list and in
  HEADER_GC_WARP. `apply_coop_game_host` (clientify.py) makes the client keep
  the host it logged in through (AccountConnector's `m_strAddr`, the channel
  address serverinfo gave it) and connect every core there with the packet's
  port, so the host's own client and a friend's both work against one server
  and nothing needs NAT loopback.
- **The second server on the list is a file.** serverinfo.py reads
  `coop.cfg` beside the client (name, host, auth, channel, channels) and adds
  "Online: <name>"; a bad or missing file leaves one server. The guild-mark
  name is "20" so two worlds' marks do not share a cache.
  `tests/coop_serverinfo_test.py` runs it on Python 2.7 and 3.
- **The launcher's half is `launcher/Metin2Launcher.Coop.psm1`**: the network
  report (the LAN adapter with the default route - SSDP has to be bound to it,
  because the WSL and Hyper-V adapters swallow the multicast - the public
  address, the UPnP gateway, the CGNAT and double-NAT verdicts), our own UPnP
  mappings (described "Metin2 SinglePlayer COOP"; somebody else's mapping is
  never touched), one firewall rule added through UAC, friend accounts (the
  engine's hash, `CONCAT('*', UPPER(SHA1(UNHEX(SHA1(pw)))))`), the invite code
  (`M2COOP1:` + base64url JSON, password included) and coop.cfg. The console
  actions are `Coop*` in Metin2-Launcher.ps1 (menu 23-29); the window's COOP
  button goes through `Open-CoopWindow` to `Show-CoopDialog`. A friend with no
  server uses `Dolacz.bat` in the client folder
  (`linux-port-mt2009/client-coop/`, shipped in the client package since
  2.0.17, with a page of instructions beside it).
- **The testers' password is a digest in the module, and it gates hosting
  alone.** `Grant-M2CoopAccess` compares SHA-256 of a salt and the password
  (spaces and dashes dropped, case folded) with `$script:CoopAccessDigest` and
  keeps that digest in `.m2coop.json` as `access`; `Test-M2CoopAccess` is the
  gate. The window asks once, before its COOP dialog (`Open-CoopWindow`,
  `Show-CoopUnlockDialog`); the text menu asks inline (`Assert-CoopHostAccess`,
  `Read-Host -AsSecureString`, which reads the console and cannot be fed from
  a pipe); an action the window starts has no console to answer from and
  refuses instead. Securing the accounts, friends, invites and hosting ask;
  ending hosting, the lease, the network check and joining never do - an
  invite code is the key to one world and only somebody who can host can make
  one, so the unlock dialog's "Mam kod zaproszenia" opens the joining tab
  alone. It is a gate, not a lock: the module is plain text. A new digest
  re-locks every install; going public is deleting the gate. The password is
  not in the repository - ask the operator.
- **No password reaches a log.** The window's actions write their output under
  launcher-logs, which support bundles carry, so CoopHost, CoopStop and
  CoopCheck print none, and whatever shows a password runs in-process in the
  dialog. `.m2coop.json` keeps the passwords and is a protected path for
  updates.
- **Hosting is `M2_HOST_BIND_ADDRESS=0.0.0.0` in .env and a recreate of the
  game container** (every core restarts). It survives GRAJ, which reads .env,
  and GRAJ renews the router's four-hour lease; the panels stay on
  M2_PANEL_BIND_ADDRESS, written out as 127.0.0.1 first. MariaDB is bound to
  127.0.0.1 by compose whatever this says.
- **A connection to a published port proves nothing on Docker Desktop.** Its
  proxy accepts one before anything in the container listens and then closes
  it; `Wait-CoopGameReady` waits for every core's handshake (`FD 01 FF ...`).
  The first version called the world ready eleven seconds into a boot the
  cores needed forty for.
- **The shipped accounts are refused.** admin/admin (IMPLEMENTOR; gmlist
  carries no IP) and test/test are created at initdb only, so the passwords
  `Protect-M2CoopAccounts` sets stick, and hosting refuses while either account
  still has the shipped one.
- **Measured on m2coop** (a copy of m2zip under `Downloads\m2coop-test`, its
  own project and volumes): CoopCheck on the Funbox 2.6 (UPnP answers, public
  IPv4); CoopHost with the firewall and UPnP steps stubbed - bindings on
  0.0.0.0 and all four cores answering on 192.168.1.16; CoopStop back to
  127.0.0.1 and the LAN refused; the real window's COOP button opening the
  dialog. Then the operator's own test on 19 September, a laptop on a phone's
  hotspot against the PC at home: Hostuj's four UPnP mappings held on the
  Funbox, the laptop logged in (once the handshake below was fixed), both
  characters saw each other and the Teleporter worked on the laptop. A trade
  and a pickup that failed once, with the laptop thrown back to the menu, were
  the hotspot stalling - the server removed the dead session a minute later -
  and worked after. Not run yet: a map on another core (`/transfer` to map 62,
  game2) with a friend in it.
- **The traffic is not private.** The client's XTEA key is fixed and key
  agreement is compiled out, so a password in LOGIN3 can be read on the way.
  That is why friends' passwords are random and per world; say so before any
  of this goes public. Hosting through a VPN (below) is the one way it is
  encrypted.
- **A host behind CGNAT is offered through a VPN, not refused.** The patrons'
  first question after 2.0.80 was CGNAT (mobile Internet, part of the fibre),
  and there nothing in the host's router can help. The module finds Radmin
  VPN, Tailscale, ZeroTier and Hamachi by their adapters
  (`Select-M2CoopVpnAdapters`, pure, `tests/coop_vpn_test.ps1`); CoopHost
  takes `-CoopVia auto|internet|vpn|radmin|tailscale|zerotier|hamachi`, and
  `Resolve-M2CoopHostingVia` keeps the Internet wherever it can work and takes
  a VPN only on `cgnat`/`double-nat` (auto), so nobody who hosted before sees
  a change. Through a VPN nothing is opened in the router - the mappings an
  earlier Internet hosting left are closed - the binding is 0.0.0.0 as before,
  and the state's `hosting` carries `mode`, `vpn`, `vpnName` and
  `friendAddress`, which is what the invites (`Get-M2CoopInviteTarget`) and the
  lease renewal (skipped) read. An invite gets a `vpn` field only when it has
  one, so an Internet code is byte for byte 2.0.80's, and the friend's side
  (`Get-M2CoopJoinAdvice`, the join tab, Dolacz.ps1 from the next client)
  says which VPN is missing and probes the world's auth (`Test-M2CoopHostAnswers`,
  the handshake bytes). Not bound to the VPN address alone on purpose: a
  container published on an adapter that is not up yet does not start, and
  the VPN comes up after Docker Desktop at boot. Tested on stubs and on this
  machine's own adapters, which hold no VPN; no world has been hosted through
  a real one yet.
- **`@($list)` of a `List[object]` is an error in Windows PowerShell 5.1.**
  "Argument types do not match" ("Niezgodne typy argumentów"), empty or not,
  at the `return @($found)` that wrote it; `List[int]` is fine, which is why
  `Get-M2CoopGamePorts` never showed it. Return `$found.ToArray()` and wrap
  at the call site as everywhere else.
- **A handshake over a hotspot needs time and slack, and gets neither from
  the package.** The login handshake is accepted only when one exchange's
  round trip is within 50 ms of the previous one, and
  `DESC_MANAGER::ConnectionCollector` (martysama's anti-flood pass) destroys
  every connection still handshaking five seconds after it opened, with no
  line in any log. The first test from a laptop on a phone's hotspot sat on
  "Zostaniesz polaczony z serwerem" for good: two auth connections, each
  closed after five to six seconds, the round trip swinging by 600 ms between
  exchanges - and the client never noticed, because
  `CAccountConnector::OnRemoteDisconnect` only goes offline and tells Python
  nothing. From the host's own network the same handshake takes 0.24 s, which
  is why no local test could have shown it. `apply_coop_handshake_window`
  (playerbotify.py) widens the window by 100 ms a retry up to a second and
  gives the collector thirty seconds; the lower bound stays at zero, because a
  client clock ahead of the server's is what the speed hack check in
  `CInputMain::Move` kicks. Through a local relay adding 50-700 ms each way
  (`scratchpad/jitter_proxy.py` of session 82d3ab90 is the shape) ten
  handshakes of ten completed in 2.9-7.3 s. `desc.cpp` and `desc_manager.cpp`
  already ship staged in `server-update-files.mt2009.txt`.
- **A test install's `overlays` is the source its next build compiles.** The
  launcher's start copies `linux-port/overlays/playerbot/src/game/src` over the
  staged tree (`Sync-M2PlayerbotOverlay`), and the copy of m2zip carried the
  2.0.74 package's overlays under a 2.0.79 engine staged by hand: the first
  rebuild of m2coop stopped at `input_db.cpp` ("no member named
  SplitForThisChannel") while the running image was fine. Put the repository's
  overlay into both before building a copy.
- UIAutomation sees the launcher's flat buttons as Pane with no Invoke
  pattern; `PostMessage(BM_CLICK)` to the NativeWindowHandle clicks them. A
  form started by `Start-Process -WindowStyle Hidden` stays hidden, because
  its first ShowWindow takes the start's SW_HIDE.

## The mt2009 tree (second engine)

`linux-port-mt2009/` is the same suite on the mt2009 / Martysama r41023 server
files: the launcher, the panels and the playerbot overlay are shared, the engine
port, the container scripts and the database bootstrap are its own. Read
`linux-port-mt2009/README.md` before touching it - it says which port script
renders which file, why the player's tree is still called `linux-port` (the
`ENGINE` marker), and what the overlay does differently under
`PLAYERBOT_ENGINE_MT2009` (`playerbot_engine_compat.h`).
