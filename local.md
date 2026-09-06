# local.md — jak to działa (robocza mapa AI)

Dokument na nasze sesje: edycja istniejących zachowań i dodawanie nowych.
Nie zastępuje `AGENTS.md` / `CLAUDE.md` (fakty silnika, pułapki, kompilacja).
Tu jest **przepływ decyzji** i **gdzie coś zmienić**.

Ostatnia aktualizacja: 2026-09-06, na podstawie kodu overlay, nie z live world.

---

## 1. Czym to jest

Suite singleplayer: Linuxowy port serwera r40250 + launcher + panel Flask + **playerbot AI w core `game`**.

Bot **nie jest klientem**. To zwykła postać PC w silniku, z fałszywym deskryptorem. Ruch, ataki i ekwipunek idą tym samym protokołem co do gracza. Panel (`files/admin_panel.py`, ~http://127.0.0.1:7788) pokazuje mapę i suwaki wag.

Źródło silnika **nie jest w tym repo**. Overlay jest nasz:

| Ścieżka | Rola |
|---|---|
| `linux-port/overlays/playerbot/src/game/src/` | Całe AI (`playerbot_*.h` + `playerbot_manager.cpp`) |
| `linux-port/overlays/playerbot/patches/` | Patche na staged engine (Windows: to, co jest staged, to co się kompiluje) |
| `linux-port/patches/` | Port Linuxa, nie AI |
| `files/admin_panel.py` | Panel. Kopia w `linux-port/docker/panel/app/` jest staged i gitignored |
| `tests/playerbot_world_rules_test.cpp` | Jedyny test C++ (czyste reguły podróży) |

Świat: **Chunjo** — Joan (21), Bokjung (23), Waryong/M3 (24), Easy Monkey (25), Orc Valley (64), pustynia (63), plus frontier (Sohan 43, Spider V1 104). Bot chodzi tylko po mapach, które **jego core** hostuje (`MAP_ALLOW`).

---

## 2. Infrastruktura, która ogranicza AI

- **32-bit C++23 w Dockerze.** Lokalnie nie ma kompilatora. Syntax-check i build: `docs/DEVELOPMENT.md`, `tools/fast-game-build/`. `-m32` jest obowiązkowe (`time_t` w packetach).
- **Jeden translation unit.** Wszystkie `playerbot_*.h` to *fragmenty implementacji*: ten sam anonymous namespace, include **dokładnie raz** z `playerbot_manager.cpp`, w kolejności zależności. Kolejność definicji = kolejność wywołań. Nowy plik: wrzuć `playerbot_foo.h` do katalogu — Makefile i packager łapią `playerbot_*`. Nie dopisuj list ręcznie.
- **Spawn ≠ suwak.** `PLAYERBOT_AUTOSPAWN_COUNT` prosi o liczbę. `LoadRegisteredBots` puszcza PID tylko gdy ledger = `complete`/`adopted`, login = `playerbot_NNN`, social id zgadza się, jeden char w empire 2. Inaczej postać zostaje w DB i nigdy nie wstaje. Sufit kohorty: `BOT_COUNT` w `generate_seed.py`.
- **Logowanie partiami.** `PLAYERBOT_SPAWN_WINDOW` (60 s) + batch co 1 s. Cała kohorta na raz przypinała core (A* + watchdog).
- **Wagi live.** Panel pisze TSV `/opt/m2spool/playerbot_weights.tsv`. Gra czyta co 5 s. 100 = jak stary łańcuch ifów. Zakres 25–250. Brak pliku = neutral.

Start gry (czytać zanim uwierzyć w liczbę botów):

```
PLAYERBOT_AUTH: loaded N registered bot identities
PLAYERBOT: autospawn requested=... registered_started=... in Chunjo
```

---

## 3. Model zachowania (cztery warstwy)

Nie mylić. Każda warstwa odpowiada na inne pytanie.

### Rola (`EPlayerBotRole`) — jak walczy

Ustalana przy logowaniu (nie „nastrój godziny”):

- `BOT_ROLE_MOB_GRINDER` — zwykły grind
- `BOT_ROLE_METIN_HUNTER` — kamienie
- `BOT_ROLE_PARTY_FIGHTER` — party (w Dolinie Orków też obozy Black Orc)

### Osobowość (`EPlayerBotPersonality`) — kim jest

Deterministycznie z PID (`GetPlayerBotStablePersonality`). **Doklejaj na końcu enumu.** Wstawianie w środek psuje id w statusie panelu.

| Osobowość | Sens |
|---|---|
| Steady adventurer | Domyślny grind |
| Metin breaker / dropper | Kamienie; dropper nie vendruje książek |
| Team companion | Party |
| Gear specialist | Refine, sprzęt |
| Careful collector | Biolog |
| Merchant | Stragan jako sposób życia |
| Wanderer | Koń / wędrowanie |
| M3 / M2 / medal dropper | Farma pod rynek, nie pod siebie |

`IsPlayerBotDropper` + `GetPlayerBotPersonalityByPID` — gdy masz tylko `CHARACTER`, bez pełnego state.

### Ambicja (`EPlayerBotAmbition`) — co faworyzuje planner

Z osobowości: LEVEL, EQUIPMENT, METINS, HORSE, BIOLOGIST, SKILLS, TRADE.

### Cel (`EPlayerBotLongTermGoal`) vs akcja (`EPlayerBotCurrentAction`)

- **Cel** = co robi w najbliższych minutach. Planner co ~5 s (`PLAYERBOT_GOAL_PLAN_INTERVAL`). Log: `PLAYERBOT_GOAL:`.
- **Akcja** = co robi w tej sekundzie (panel nad głową). Ustawia podsystem, który wygrał tick.

Cele: LEVEL_UP, SURVIVE, CHOOSE_PROFESSION, GET_EQUIPMENT, RESTOCK, REFINE, MASTER_SKILL, HUNT_METIN, PARTY_CHALLENGE, BIOLOGIST, HUNTING, HORSE, FISHING.

Akcje: IDLE, TRAVEL, FIGHT, LOOT, RECOVER, TRAIN, SHOP, REFINE, READ_BOOK, SOCKET_STONE, PARTY_ASSEMBLE, BIOLOGIST, STABLE, STALL, FISHING, MARKET.

`SHOP` = NPC, `STALL` = własny stragan, `MARKET` = kupowanie u innego bota. Nie scalać.

Stan jednego bota: `TPlayerBotAIState` w `s_mapPlayerBotAIStates`. Timery, flaga wizyty w mieście, trasa A*, VID celu, fazy town visit, fishing, stall, itd. **Kolejność memberów konstruktora = kolejność pól** (`-Wreorder`).

Przejścia:

```text
SetPlayerBotGoal(ch, state, BOT_GOAL_..., dwNow)
SetPlayerBotAction(state, BOT_ACTION_..., dwNow)
```

Oba w `playerbot_types.h`. Każdy podsystem ich używa.

---

## 4. Tick — to jest serce

`CPlayerBotManager::Update` w `playerbot_manager.cpp`. Kolejność jest **ładująca**. Podsystem, który `continue`, **zabiera tick**: nie ma walki, nie ma gear passa.

Pełny tick jest **co drugi** dla danego PID (`(pid + s_dwTick) % 2`). Na ticku „lekkim”: tylko dokończenie trasy (`MovePlayerBot`) + `ExecutePlayerBotBasicAttack` + kill note dla konia bojowego. Bez tego szybkie postacie stają na waypoincie.

### Kolejność pełnego ticka (uproszczona)

1. `SpawnPendingBatch` / `RefreshPlayerBotWeights` — raz na populację
2. Status nad głową
3. Persist, śmierć
4. **`ManagePlayerBotShopLifetime` — zawsze pierwsze.** Otwarty stragan to stan silnika z deadline. Nie może zależeć od tego, kto wygra tick.
5. Zakupy na rynku (`ManagePlayerBotShopping`) — `continue` gdy kupuje
6. Watchdog bezczynności (~90 s bez ruchu/ataku/castu) — reset stanu. **Każda nowa aktywność, która stoi w miejscu, musi tu dostać wyjątek.**
7. Rescue poza mapą / w ścianie
8. Staty, skille, gildia, party, hunting quest (bez dialogu)
9. Skrzynie progresji — `continue`
10. **`PlanPlayerBotLongTermGoal`** — ustawia cel; **nie wykonuje** go
11. Loot — w walce nie blokuje ticka (throttled pickup); poza walką może zabrać tick
12. Koń, ryby, world travel, biolog — każdy może `continue`
13. Start wizyty w mieście (pełny ekwipunek, brak mikstur, brak broni, refine, strzały, junk…)
14. **`HandlePlayerBotTownVisit`** — maszyna stanów faz; `continue` przez całą wizytę
15. Dismount jeśli koń nie jest bojowy
16. `PrepareWeapon` — bez broni: scavenger albo town
17. Mikstury, skrzynie, boostery, recovery, retreat
18. `ManagePlayerBotEquipment` — `continue` gdy ubiera
19. Buffy, multi-pull — `continue`
20. Relokacja (stoi 5 min → następny hub)
21. **Na końcu:** target (`FindPlayerBotEngagedTarget` potem `FindDistributedTarget`) i atak

Wniosek do nowych zachowań: jeśli aktywność **nie może** iść w parze z walką (wędka w slocie broni, stragan, wizyta u NPC), musi `continue` **przed** targetingiem. Jeśli ma iść „przy okazji walki” (mikstura, lekki loot), nie zabierać ticka.

---

## 5. Planner — jak wygrywa cel

`playerbot_planner.h` → `PlanPlayerBotLongTermGoal`.

**Trzy bramki poza suwakami** (waga ich nie wyłączy):

1. SURVIVE — death recovery, retreat, HP < 35%
2. CHOOSE_PROFESSION — lvl ≥ 5, brak skill group
3. GET_EQUIPMENT — brak broni w `WEAR_WEAPON`

Plus: jeśli bot jest **w środku** stajni / kowala (`HasPlayerBotCommittedTownErrand`), nie przerywaj wizyty.

Reszta: lista kandydatów z `iBase` (stary łańcuch ifów, co 10) × waga z panelu. Przy wszystkich wagach = 100 wynik = historyczny łańcuch. Remis zostawia **wcześniejszego** kandydata.

`BOT_GOAL_LEVEL_UP` jest zawsze w głosowaniu — grind to fallback, nie „brak celu”.

Waga `FISHING` / `TRADE` **nie są** w tym głosowaniu. Sterują tym, *ilu* botów w ogóle wchodzi w sesję (osobowość + udział). Cel FISHING ustawia `ManagePlayerBotFishing`, gdy sesja żyje.

Nowy cel długoterminowy:

1. Dokleić wartość w `EPlayerBotLongTermGoal`
2. Dodać `OfferPlayerBotGoal(...)` we właściwym miejscu listy (kolejność = historyczny priorytet)
3. Opcjonalnie nowa waga w `playerbot_config.h` + nazwa TSV + slider w panelu
4. Podsystem, który ten cel **realizuje**, musi i tak wygrać tick (planner nic nie rusza)

---

## 6. Mapa plików (gdzie edytować)

Include order w `playerbot_manager.cpp` = graf zależności.

| Plik | Wchodzić przez | Gdy zmieniasz |
|---|---|---|
| `playerbot_types.h` | stałe, enumy, `TPlayerBotAIState` | nowy timer, cel, akcja, osobowość |
| `playerbot_log.h` | log raz na N botów | nowy tag `PLAYERBOT_FOO:` |
| `playerbot_config.h` | wagi live | nowy slider |
| `playerbot_world_rules.h` | czysta polityka podróży | **tu da się test jednostkowy** |
| `playerbot_navigation.h` | walkable, A*, woda jako kara nie ściana | grid, mosty |
| `playerbot_world_memory.h` | gęstość mobów w komórkach 6400 | wybór huba |
| `playerbot_movement.h` | `MovePlayerBot`, portale, metin registry | chodzenie |
| `playerbot_gear.h` | scoring ekwipunku, strzały | drabinka progresji |
| `playerbot_consumables.h` | skrzynie moonlight, boostery | zużywanie itemów |
| `playerbot_activities.h` | `ManagePlayerBotHorse`, `ManagePlayerBotFishing` | koń, ryby — **własny tick** |
| `playerbot_missions.h` | biolog, hunting mission | zbiórki bez dialogu |
| `playerbot_skills.h` | staty, kolejność skilli, buffy | karta postaci |
| `playerbot_combat.h` | pakiety ataku (bot nie ma klienta) | swing / skill |
| `playerbot_economy.h` | junk, NPC, kowal, stall, refine | **junk default = true** |
| `playerbot_bonus.h` | linie bonusów, reroll | płacenie za zmianę linii |
| `playerbot_travel.h` | `ManagePlayerBotWorldTravel`, `TransitionPlayerBotMap` | która mapa |
| `playerbot_planner.h` | `PlanPlayerBotLongTermGoal` | co wygrywa następną chwilę |
| `playerbot_guild.h` | gildia, affinity | Module 4 |
| `playerbot_town.h` | `HandlePlayerBotTownVisit` | wizyta w mieście |
| `playerbot_market.h` | kupno u innego bota | stragany od strony kupującego |
| `playerbot_loot.h` | pickup | nie zamiatać podłogi |
| `playerbot_survival.h` | save, retreat, powrót po śmierci | watchdog exemptions tu / w managerze |
| `playerbot_wandering.h` | huby, `ChoosePlayerBotHuntingHub` | „co robi gdy nic nie woła” |
| `playerbot_status.h` | tekst nad głową | polski ASCII |
| `playerbot_targeting.h` | `FindDistributedTarget`, claim moba | kto bije kogo |
| `playerbot_manager.cpp` | osobowość, party, `Update` | kolejność hooków, spawn |

Entry pointy (szukaj po nazwie, nie po numerze linii):

- Plan: `PlanPlayerBotLongTermGoal`, `GetPlayerBotWeight`, `RefreshPlayerBotWeights`
- Nav: `MovePlayerBot`, `TransitionPlayerBotMap`, `MovePlayerBotToWorldPortal`, `ManagePlayerBotWorldTravel`
- Walka: `FindDistributedTarget`, `ExecutePlayerBotBasicAttack`, `ExecutePlayerBotAttackSkill`, `HandlePlayerBotMultiPull`
- Ekonomia: `ManagePlayerBotEquipment`, `IsPlayerBotJunkItem`, `ManagePlayerBot*Merchant`, `ManagePlayerBotRefining`, `ManagePlayerBotPrivateShop`, `ManagePlayerBotBonusReroll`
- Aktywności: `ManagePlayerBotHorse`, `ManagePlayerBotFishing`, `ManagePlayerBotBiologist`, `HandlePlayerBotTownVisit`

---

## 7. Nawigacja i „gdzie bot ma stać”

- Planner trasy czyta **własny grid** (`m_blocked` = `ATTR_BLOCK|ATTR_OBJECT` w środku komórki). Live sectree tylko przy chodzeniu (`SegmentClearWorld`). Pytanie live w A* = 31 s / 60 przy ~850 botach.
- `ATTR_WATER` **nie jest ścianą**. Most = WATER bez BLOCK. Kara: `PLAYERBOT_NAV_WATER_PENALTY`.
- Huby są **ręczne** (`TPlayerBotHuntingHub` w wandering). Pamięć (`playerbot_world_memory.h`) mówi jak gęsto jest w komórce; tabela mówi gdzie jest miejsce. Bot stojący w respawnie ≠ miejsce życia packa.
- Wybór huba: share mobów / botów tam, kara odległości (pół przy 20 km), trzymanie wyboru ~4 min. Bez holda: 160–334 far plans/min.

Nowa mapa frontier: rząd w `GetPlayerBotFrontier*`, huby, whitelist nav, panel, `MAP_ALLOW` na core.

---

## 8. Miasto, rynek, junk

Wizyta: maszyna stanów `EPlayerBotTownVisitPhase` w `playerbot_town.h`. Przeżywa teleport/śmierć — faza jest pamięcią. Nie restartuj wizyty w środku.

`IsPlayerBotJunkItem` **domyślnie `return true`**. Każdy item warty zachowania: jawny wyjątek, albo zniknie przy następnym mieście. Dropperzy: książki Metin, medal, farmiony loot.

Stragan: min. 2 pozycje albo jedna z score ≥ `PLAYERBOT_SHOP_PRIZE_SCORE`. +7+ nigdy do NPC. Merchant personality trzyma counter jako pracę.

---

## 9. Przepis: nowe zachowanie

Minimalna ścieżka (kolejność ważna):

1. **Stan** — pole / timer w `TPlayerBotAIState` (kolejność init!). Jeśli cel lub akcja: dokleić enum.
2. **Fragment** — nowy `playerbot_xyz.h` albo istniejący podsystem. Funkcja `bool ManagePlayerBotX(...)`: `true` = zabrałem tick.
3. **Include** w `playerbot_manager.cpp` **przed** callerami.
4. **Hook w `Update`** — miejsce: przed combat jeśli mutually exclusive; obok loot/potion jeśli tło. Po `PlanPlayerBotLongTermGoal` jeśli ma reagować na cel.
5. **Planner** — jeśli to wielominutowy cel, nie tylko chwilowa akcja.
6. **Watchdog** — jeśli bot stoi (ryby, stall, czekanie na NPC).
7. **Junk / ekwipunek** — jeśli nowe itemy.
8. **Log** — `PLAYERBOT_<AREA>: pid= name=`
9. **Status** — polski ASCII w `playerbot_status.h`
10. **Stałe nazwane** na górze namespace, nie literały w środku ticka

Nie: pełny skan mapy / inventory co tick. Budżet widać w `PLAYERBOT_LOAD:` raz na minutę.

Wzorzec czystej polityki (test bez Dockera): `playerbot_world_rules.h` + `tests/playerbot_world_rules_test.cpp`. Decyzja na zwykłych structach; pakiety i `CHARACTER` zostają w managerze / fragmencie.

---

## 10. Przepis: edycja istniejącego

1. Znajdź **entry point** z tabeli, nie „miejsce w 12k linii”.
2. Sprawdź, czy zmiana to **polityka** (kiedy / czy) czy **wykonanie** (pakiet, timer, warp). Politykę da się często wyciągnąć; pakietów nie ruszać w tym samym commicie bez potrzeby.
3. Sprawdź **kto zabiera tick** — objaw „bot nie walczy” prawie zawsze = `continue` za wcześnie albo brak zwolnienia flagi (`bVisitingShop`, `bFishingSession`, stall).
4. Osobowość / dropper: nie polegaj na roli; pytaj `GetPlayerBotPersonalityByPID`.
5. Po hot-loop: czytaj `PLAYERBOT_LOAD:` (plany wg odległości, deferrals, target searches, watchdog). CPU samo kłamie.

---

## 11. Weryfikacja (tego nie oszukamy lokalnie)

- Syntax-check overlay vs staged headers — ~1 min, Docker `gcc:13 -m32` (przepis w AGENTS.md). Potem **przywróć** staged pliki w cache.
- Unit test: tylko world_rules.
- Runtime: operatorzy, ~350+ botów. Mówić jasno: *skomplikowane* vs *zaobserwowane*.
- Logi: `docker exec … syslog | grep PLAYERBOT_` — `AI`, `GOAL`, `NAV`, `LOAD`, `SPOT`, `METIN`, `PARTY`.
- Panel: cele/akcje nad głową; jeśli nowa akcja, id musi być doklejone, nie wstawione.

---

## 12. Pułapki przy zachowaniach (skrót)

Pełna lista w AGENTS.md. To, co gryzie przy AI:

- Watchdog 90 s vs stanie w miejscu
- Junk default true
- `TMonkeyVisitContext` pozycyjny w teście — nowe pole przesuwa resztę
- `CHARACTER::fishing()` bez null-check na sectree
- Hubów nie wymyślać z pamięci komórek
- Patch engine na Windowsie = staged file na liście `launcher/server-update-files.txt`, nie sam `.patch`
- Stringi gracza: polski, ASCII, bez ogonków

---

## 13. Następne sesje (placeholder)

Tu dopisujemy konkretne zachowania, które ruszamy:

- [ ] …
