# Playerbot architecture and refactoring path

The live AI grew experimentally while behaviour was being discovered in-game.
That produced useful results quickly, but `playerbot_manager.cpp` now owns too
many responsibilities. The project will improve it incrementally: one bounded,
testable policy at a time, with the existing world used as a regression test.
A big-bang rewrite would hide behavioural changes and is deliberately avoided.

## Target module boundaries

| Module | Owns | Must not own |
|---|---|---|
| lifecycle/state | spawn, load, save, death and recovery | pathfinding or combat policy |
| navigation | walkability, routes, stuck recovery | equipment and progression |
| world travel | map choice, portal/teleport policy, travel cooldowns | combat execution |
| combat | target selection, combos, skills, party engagement | shops and database persistence |
| loot/economy | pickup, inventory pressure, merchants, prices | navigation internals |
| equipment | comparison, equipping, upgrading and hand-me-downs | map transitions |
| progression | stats, skill paths, Biologist, horse and hunting goals | packet animation details |
| presentation | status/action text and live-map DTOs | AI decisions |

Dependencies should point from the manager toward small policy modules. Pure
rules accept plain context structs and return decisions; server actions stay in
the manager or a narrow service that can validate `CHARACTER` state.

## First extraction

`playerbot_world_rules.h` contains the pure decision for leaving the Easy
Monkey Dungeon and travel-cooldown evaluation. Its regression test is
`tests/playerbot_world_rules_test.cpp`. This fixes a real bug while establishing
the pattern for later extractions.

## Change discipline

1. Capture the current behaviour with a pure test or a runtime invariant.
2. Extract only the decision, without changing packets, timings and persistence
   in the same commit unless the issue requires it.
3. Compile the full game core, deploy to the persistent test world, then inspect
   transition/error logs and bot distribution by map.
4. Keep database migrations additive and never make a fresh seed look like a
   valid established world.
5. Measure hot-loop changes with hundreds of bots; avoid per-tick full-world or
   full-inventory scans.

Next candidates are target-scoring, inventory disposition and status text.
They have clear inputs/outputs and can be extracted without replacing stable
navigation or animation code.
