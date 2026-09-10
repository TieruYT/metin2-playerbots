#include <cassert>
#include "../linux-port/overlays/playerbot/src/game/src/playerbot_empire_rules.h"

using namespace playerbot_empire_rules;

int main()
{
	// --- every kingdom resolves every role, and nobody falls back to Chunjo
	assert(GetHomeMap(EMPIRE_SHINSOO, MAP_ROLE_M1) == 1);
	assert(GetHomeMap(EMPIRE_SHINSOO, MAP_ROLE_M2) == 3);
	assert(GetHomeMap(EMPIRE_SHINSOO, MAP_ROLE_M3) == 4);
	assert(GetHomeMap(EMPIRE_SHINSOO, MAP_ROLE_MONKEY_EASY) == 5);
	assert(GetHomeMap(EMPIRE_CHUNJO, MAP_ROLE_M1) == 21);
	assert(GetHomeMap(EMPIRE_CHUNJO, MAP_ROLE_M2) == 23);
	assert(GetHomeMap(EMPIRE_CHUNJO, MAP_ROLE_M3) == 24);
	assert(GetHomeMap(EMPIRE_CHUNJO, MAP_ROLE_MONKEY_EASY) == 25);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_M1) == 41);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_M2) == 43);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_M3) == 44);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_MONKEY_EASY) == 45);

	// An unknown kingdom or role answers "no map", never somebody else's. A
	// bot sent to map 0 stays where it is; a bot sent to 23 by accident walks
	// into another kingdom's town.
	assert(GetHomeMap(EMPIRE_NONE, MAP_ROLE_M1) == 0);
	assert(GetHomeMap(7, MAP_ROLE_M2) == 0);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_SHARED) == 0);
	assert(GetHomeMap(EMPIRE_JINNO, MAP_ROLE_NONE) == 0);

	// --- what a map is, and whose it is
	assert(GetMapRole(3) == MAP_ROLE_M2 && GetMapOwnerEmpire(3) == EMPIRE_SHINSOO);
	assert(GetMapRole(44) == MAP_ROLE_M3 && GetMapOwnerEmpire(44) == EMPIRE_JINNO);
	assert(GetMapRole(64) == MAP_ROLE_SHARED);
	assert(GetMapOwnerEmpire(64) == EMPIRE_NONE);   // shared, not hostile
	assert(GetMapOwnerEmpire(104) == EMPIRE_NONE);  // the Spider Dungeon
	assert(!IsKingdomMap(61) && IsKingdomMap(45));

	// The whole point of the split: a Jinno bot standing on Shinsoo's M2 is on
	// an M2 and is not at home, and its own M2 is still 43.
	assert(GetMapRole(3) == MAP_ROLE_M2);
	assert(!IsHomeMap(EMPIRE_JINNO, 3));
	assert(!IsHomeMapOfRole(EMPIRE_JINNO, 3, MAP_ROLE_M2));
	assert(IsHomeMapOfRole(EMPIRE_JINNO, 43, MAP_ROLE_M2));
	assert(IsHomeMap(EMPIRE_SHINSOO, 3));

	// --- the easy dungeon follows the bot, not the ground it stands on
	assert(GetMonkeyEasyMap(EMPIRE_SHINSOO) == 5);
	assert(GetMonkeyEasyMap(EMPIRE_CHUNJO) == 25);
	assert(GetMonkeyEasyMap(EMPIRE_JINNO) == 45);
	assert(IsMonkeyEasyMap(5) && IsMonkeyEasyMap(25) && IsMonkeyEasyMap(45));
	assert(!IsMonkeyEasyMap(108) && !IsMonkeyEasyMap(109));

	// --- relations: all nine pairs, plus what has no kingdom at all
	for (int a = EMPIRE_SHINSOO; a <= EMPIRE_JINNO; ++a)
	{
		for (int b = EMPIRE_SHINSOO; b <= EMPIRE_JINNO; ++b)
		{
			if (a == b)
			{
				assert(GetRelation(a, b) == RELATION_ALLY);
				assert(IsAllyEmpire(a, b) && !IsEnemyEmpire(a, b));
			}
			else
			{
				assert(GetRelation(a, b) == RELATION_ENEMY);
				assert(IsEnemyEmpire(a, b) && !IsAllyEmpire(a, b));
			}
		}
	}
	// A monster, an NPC and a stone carry no empire: neutral, and never an
	// enemy - a fourth kingdom is exactly the mistake this guards against.
	assert(GetRelation(EMPIRE_JINNO, EMPIRE_NONE) == RELATION_NEUTRAL);
	assert(GetRelation(EMPIRE_NONE, EMPIRE_JINNO) == RELATION_NEUTRAL);
	assert(!IsEnemyEmpire(EMPIRE_JINNO, EMPIRE_NONE));

	// --- services: every village answers, and no two kingdoms share a point
	TTownServices a1, b1, c1, a3, b3, c3, none;
	assert(GetTownServices(1, a1) && GetTownServices(21, b1) && GetTownServices(41, c1));
	assert(GetTownServices(3, a3) && GetTownServices(23, b3) && GetTownServices(43, c3));
	assert(!GetTownServices(64, none));   // a shared map has no village of ours
	assert(!GetTownServices(4, none));    // nor does a guild map
	assert(a1.blacksmith.x != b1.blacksmith.x && b1.blacksmith.x != c1.blacksmith.x);
	assert(a3.teleporter.x != b3.teleporter.x && b3.teleporter.x != c3.teleporter.x);

	// The four points that were already in the AI as hand-written Chunjo
	// constants, and which the extraction reproduces to the unit. If one of
	// these ever fails, the reader of the map files has drifted, not the map.
	assert(b1.teleporter.x == 51900 && b1.teleporter.y == 153600);
	assert(b3.teleporter.x == 136900 && b3.teleporter.y == 240300);
	assert(b1.skillReset.x == 58800 && b1.skillReset.y == 165700);  // the old woman

	// --- gates: six per kingdom, both ends, and they lead where they say
	TKingdomGate gates[6];
	for (int empire = EMPIRE_SHINSOO; empire <= EMPIRE_JINNO; ++empire)
	{
		assert(GetKingdomGates(empire, gates, 6) == 6);
		const TKingdomMaps maps = GetKingdomMaps(empire);
		for (int i = 0; i < 6; ++i)
		{
			// Every gate joins two maps of this kingdom, in both directions.
			assert(GetMapOwnerEmpire(gates[i].fromMap) == empire);
			assert(GetMapOwnerEmpire(gates[i].toMap) == empire);
			TKingdomGate back;
			assert(FindKingdomGate(empire, gates[i].toMap, gates[i].fromMap, back));
			assert(back.fromMap == gates[i].toMap);
		}
		TKingdomGate gate;
		assert(FindKingdomGate(empire, maps.m1, maps.m2, gate));
		assert(FindKingdomGate(empire, maps.m2, maps.monkeyEasy, gate));
		assert(FindKingdomGate(empire, maps.m2, maps.m3, gate));
		// There is no gate from M1 straight to the dungeon: the trip goes
		// through M2, which is where the AI's route has to send it.
		assert(!FindKingdomGate(empire, maps.m1, maps.monkeyEasy, gate));
		assert(!FindKingdomGate(empire, maps.m1, maps.m3, gate));
	}
	// The Chunjo gate the AI has walked to for months.
	TKingdomGate joanGate;
	assert(FindKingdomGate(EMPIRE_CHUNJO, 23, 21, joanGate));
	assert(joanGate.gate.x == 113000 && joanGate.gate.y == 213600);
	assert(FindKingdomGate(EMPIRE_CHUNJO, 21, 23, joanGate));
	assert(joanGate.arrival.x == 111800 && joanGate.arrival.y == 216100);

	// --- the Teleporter: a destination per kingdom, and the engine's fare
	TPoint arrival, other;
	assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_DESERT, arrival));
	assert(arrival.x == 221900 && arrival.y == 502700);
	assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_ORC_VALLEY, arrival));
	assert(arrival.x == 270400 && arrival.y == 739900);
	// Each kingdom lands somewhere else on the same shared map.
	assert(GetTeleportArrival(EMPIRE_SHINSOO, TELEPORT_ORC_VALLEY, arrival));
	assert(GetTeleportArrival(EMPIRE_JINNO, TELEPORT_ORC_VALLEY, other));
	assert(arrival.x != other.x || arrival.y != other.y);
	assert(!GetTeleportArrival(EMPIRE_NONE, TELEPORT_DESERT, arrival));
	assert(!GetTeleportArrival(9, TELEPORT_DESERT, arrival));

	assert(GetTeleportDestinationMap(TELEPORT_ORC_VALLEY, EMPIRE_JINNO) == 64);
	assert(GetTeleportDestinationMap(TELEPORT_DESERT, EMPIRE_JINNO) == 63);
	assert(GetTeleportDestinationMap(TELEPORT_SOHAN, EMPIRE_JINNO) == 61);
	// The guild-map destination is the caller's own M3, not a fixed map.
	assert(GetTeleportDestinationMap(TELEPORT_GUILD_MAP, EMPIRE_SHINSOO) == 4);
	assert(GetTeleportDestinationMap(TELEPORT_GUILD_MAP, EMPIRE_JINNO) == 44);

	// floor(level/5)*1000 with a floor of 1000, and nothing below level 11 -
	// map_warp.quest, and what GetPlayerBotTeleporterFee already charges.
	assert(GetTeleportFee(1) == 1000);
	assert(GetTeleportFee(10) == 2000);
	assert(GetTeleportFee(49) == 9000);
	assert(GetTeleportFee(55) == 11000);
	assert(!CanUseTeleporter(10) && CanUseTeleporter(11));

	// --- one budget split between the kingdoms that have identities
	{
		int registered[EMPIRE_COUNT] = { 0, 0, 0, 0 };
		int want[EMPIRE_COUNT] = { 0, 0, 0, 0 };

		// What runs today: only Chunjo is seeded, so it gets the whole budget
		// and the slider means exactly what it has always meant.
		registered[EMPIRE_CHUNJO] = 1012;
		SplitPopulation(970, registered, want);
		assert(want[EMPIRE_SHINSOO] == 0 && want[EMPIRE_JINNO] == 0);
		assert(want[EMPIRE_CHUNJO] == 970);

		// Three kingdoms, plenty of identities: equal thirds, nothing lost to
		// the division.
		registered[EMPIRE_SHINSOO] = registered[EMPIRE_JINNO] = 1000;
		SplitPopulation(970, registered, want);
		assert(want[EMPIRE_SHINSOO] + want[EMPIRE_CHUNJO] + want[EMPIRE_JINNO] == 970);
		assert(want[EMPIRE_SHINSOO] >= 323 && want[EMPIRE_SHINSOO] <= 324);

		// A kingdom short of identities takes what it has and the rest is
		// handed to the kingdoms that can carry it - never left unspawned.
		registered[EMPIRE_SHINSOO] = 30;
		registered[EMPIRE_JINNO] = 30;
		SplitPopulation(970, registered, want);
		assert(want[EMPIRE_SHINSOO] == 30 && want[EMPIRE_JINNO] == 30);
		assert(want[EMPIRE_CHUNJO] == 910);
		assert(want[EMPIRE_SHINSOO] + want[EMPIRE_CHUNJO] + want[EMPIRE_JINNO] == 970);

		// Never more than exists, whatever the budget says.
		registered[EMPIRE_CHUNJO] = 40;
		SplitPopulation(970, registered, want);
		assert(want[EMPIRE_SHINSOO] == 30 && want[EMPIRE_CHUNJO] == 40 && want[EMPIRE_JINNO] == 30);

		// Nothing seeded at all, and a budget of nothing: no bots, no crash.
		registered[EMPIRE_SHINSOO] = registered[EMPIRE_CHUNJO] = registered[EMPIRE_JINNO] = 0;
		SplitPopulation(970, registered, want);
		assert(want[EMPIRE_SHINSOO] == 0 && want[EMPIRE_CHUNJO] == 0 && want[EMPIRE_JINNO] == 0);
		registered[EMPIRE_CHUNJO] = 500;
		SplitPopulation(0, registered, want);
		assert(want[EMPIRE_CHUNJO] == 0);
	}

	return 0;
}
