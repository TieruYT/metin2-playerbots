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
	// The three merchants the town visit has always walked to on map 21.
	assert(b1.weaponMerchant.x == 67600 && b1.weaponMerchant.y == 168600);
	assert(b1.armourMerchant.x == 67600 && b1.armourMerchant.y == 164100);
	assert(b1.miscMerchant.x == 59000 && b1.miscMerchant.y == 171300);
	assert(b1.blacksmith.x == 59400 && b1.blacksmith.y == 171600);
	assert(b1.storekeeper.x == 60900 && b1.storekeeper.y == 162000);
	assert(b1.stableKeeper.x == 54900 && b1.stableKeeper.y == 163400);
	// ...and on map 23.
	assert(b3.weaponMerchant.x == 147200 && b3.armourMerchant.x == 148500);
	assert(b3.miscMerchant.x == 141300 && b3.blacksmith.x == 142000);
	assert(b3.storekeeper.x == 149500 && b3.stableKeeper.x == 146900);

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
	// The guild map lands on each map's own Town.txt cell, never on the
	// Teleporter quest's (179500, 1000) - that is the unwalkable corner of
	// metin2_map_guild_02 and stranded every bot sent there.
	assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_GUILD_MAP, arrival));
	assert(arrival.x == 221900 && arrival.y == 9200);
	assert(GetTeleportArrival(EMPIRE_SHINSOO, TELEPORT_GUILD_MAP, arrival));
	assert(arrival.x == 135400 && arrival.y == 5500);
	assert(GetTeleportArrival(EMPIRE_JINNO, TELEPORT_GUILD_MAP, arrival));
	assert(arrival.x == 270900 && arrival.y == 12700);
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

		// The operator's own number per kingdom: each takes its own, cut to
		// what it has, and nothing it cannot take goes to the others.
		int asked[EMPIRE_COUNT] = { 0, 60, 900, 60 };
		registered[EMPIRE_SHINSOO] = 500;
		registered[EMPIRE_CHUNJO] = 1012;
		registered[EMPIRE_JINNO] = 40;
		TakeKingdomCounts(asked, registered, want);
		assert(want[EMPIRE_SHINSOO] == 60 && want[EMPIRE_CHUNJO] == 900 && want[EMPIRE_JINNO] == 40);
		asked[EMPIRE_CHUNJO] = -5;
		TakeKingdomCounts(asked, registered, want);
		assert(want[0] == 0 && want[EMPIRE_CHUNJO] == 0);
		// A kingdom switched off (no identities) takes nothing.
		registered[EMPIRE_JINNO] = 0;
		TakeKingdomCounts(asked, registered, want);
		assert(want[EMPIRE_JINNO] == 0 && want[EMPIRE_SHINSOO] == 60);

		// kavvaski's world (23 September): 729/250/773 asked for, and the
		// 500 Shinsoo and Jinno identities of 2.0.8 cut it to 500/250/500.
		// With the 1500 a kingdom of 2.2.1 every number is taken whole, and
		// the one number 2500 is an equal world instead of 500/1500/500.
		int kavvaski[EMPIRE_COUNT] = { 0, 729, 250, 773 };
		registered[EMPIRE_SHINSOO] = registered[EMPIRE_JINNO] = 500;
		registered[EMPIRE_CHUNJO] = 1500;
		TakeKingdomCounts(kavvaski, registered, want);
		assert(want[EMPIRE_SHINSOO] == 500 && want[EMPIRE_CHUNJO] == 250 && want[EMPIRE_JINNO] == 500);
		SplitPopulation(2500, registered, want);
		assert(want[EMPIRE_SHINSOO] == 500 && want[EMPIRE_CHUNJO] == 1500 && want[EMPIRE_JINNO] == 500);
		registered[EMPIRE_SHINSOO] = registered[EMPIRE_CHUNJO] = registered[EMPIRE_JINNO] = 1500;
		TakeKingdomCounts(kavvaski, registered, want);
		assert(want[EMPIRE_SHINSOO] == 729 && want[EMPIRE_CHUNJO] == 250 && want[EMPIRE_JINNO] == 773);
		SplitPopulation(2500, registered, want);
		assert(want[EMPIRE_SHINSOO] == 834 && want[EMPIRE_CHUNJO] == 833 && want[EMPIRE_JINNO] == 833);
	}

	// -----------------------------------------------------------------
	//  The market pitch, the trainers and the Biologist
	// -----------------------------------------------------------------
	{
		TPoint p;
		// Chunjo's two are the points the AI has always used. If either of
		// these ever changes, a live market has moved and it was not on
		// purpose.
		assert(GetTownPitch(21, p) && p.x == 63400 && p.y == 166300);
		assert(GetTownPitch(23, p) && p.x == 145500 && p.y == 240000);
		// The other four stand on their kingdom's own guard, 11000 in Shinsoo
		// and 11004 in Jinno, in the middle of each village's round square -
		// the place Chunjo's two share with 11002. If one of these moves, a
		// market has left its square.
		assert(GetTownPitch(1, p) && p.x == 474325 && p.y == 954225);
		assert(GetTownPitch(3, p) && p.x == 353025 && p.y == 882325);
		assert(GetTownPitch(41, p) && p.x == 959925 && p.y == 268825);
		assert(GetTownPitch(43, p) && p.x == 863425 && p.y == 246025);
		// Every village has one, and only villages have one.
		const long villages[6] = { 1, 3, 21, 23, 41, 43 };
		for (int i = 0; i < 6; ++i)
			assert(GetTownPitch(villages[i], p));
		assert(!GetTownPitch(24, p));    // a guild map has no market
		assert(!GetTownPitch(64, p));    // nor has Orc Valley

		// The trainers reproduce, to the unit, the eight coordinates the town
		// visit carried as literals before this table existed: group one at
		// x = 62300/63100/64500/65300 and group two four hundred east of each,
		// with the Warrior a hundred south of the other three jobs.
		assert(GetSkillTrainer(21, 0, 1, p) && p.x == 62300 && p.y == 161800);
		assert(GetSkillTrainer(21, 0, 2, p) && p.x == 62700 && p.y == 161800);
		assert(GetSkillTrainer(21, 1, 1, p) && p.x == 63100 && p.y == 161900);
		assert(GetSkillTrainer(21, 1, 2, p) && p.x == 63500 && p.y == 161900);
		assert(GetSkillTrainer(21, 2, 1, p) && p.x == 64500 && p.y == 161900);
		assert(GetSkillTrainer(21, 2, 2, p) && p.x == 64900 && p.y == 161900);
		assert(GetSkillTrainer(21, 3, 1, p) && p.x == 65300 && p.y == 161900);
		assert(GetSkillTrainer(21, 3, 2, p) && p.x == 65700 && p.y == 161900);
		// All four jobs and both groups answer in every first village, and no
		// second village has a trainer at all - which is what sends a bot with
		// no skill group back to M1.
		const long firstVillages[3] = { 1, 21, 41 };
		for (int i = 0; i < 3; ++i)
		{
			assert(HasSkillTrainers(firstVillages[i]));
			for (int job = 0; job < 4; ++job)
				for (int group = 1; group <= 2; ++group)
					assert(GetSkillTrainer(firstVillages[i], job, group, p) &&
							p.x != 0 && p.y != 0);
		}
		assert(!HasSkillTrainers(3) && !HasSkillTrainers(23) && !HasSkillTrainers(43));
		// Out-of-range arguments answer no rather than reading past the table.
		assert(!GetSkillTrainer(21, -1, 1, p));
		assert(!GetSkillTrainer(21, 4, 1, p));
		assert(!GetSkillTrainer(21, 0, 0, p));
		assert(!GetSkillTrainer(21, 0, 3, p));

		// The Biologist stands in the three first villages and nowhere else.
		assert(GetBiologist(21, p) && p.x == 89800 && p.y == 182100);
		assert(GetBiologist(1, p) && GetBiologist(41, p));
		assert(!GetBiologist(3, p) && !GetBiologist(23, p) && !GetBiologist(43, p));

		// Baek-Go keeps the same shape and is a different NPC in the same
		// villages: a row copied from the Biologist would send every herbalist
		// errand to the wrong corner of the town.
		assert(GetHerbalist(21, p) && p.x == 67400 && p.y == 161400);
		assert(GetHerbalist(1, p) && GetHerbalist(41, p));
		assert(!GetHerbalist(3, p) && !GetHerbalist(23, p) && !GetHerbalist(43, p));
		{
			TPoint herb, bio;
			for (int k = 0; k < 3; ++k)
			{
				const long map = k == 0 ? 1 : (k == 1 ? 21 : 41);
				assert(GetHerbalist(map, herb) && GetBiologist(map, bio));
				assert(herb.x != bio.x || herb.y != bio.y);
			}
		}

		// The Alchemist (20001), who turns the banned soul stones into dust:
		// each first village's own npc.txt row, BasePosition + cell * 100, and
		// nowhere else. Checked the same way as the two above - a row taken
		// from another village sends the exchange across the world.
		assert(GetAlchemist(1, p) && p.x == 409600 + 62200 && p.y == 896000 + 51100);
		assert(GetAlchemist(21, p) && p.x == 0 + 66000 && p.y == 102400 + 73400);
		assert(GetAlchemist(41, p) && p.x == 921600 + 29200 && p.y == 204800 + 81200);
		assert(!GetAlchemist(3, p) && !GetAlchemist(23, p) && !GetAlchemist(43, p));
		{
			TPoint alch, herb;
			for (int k = 0; k < 3; ++k)
			{
				const long map = k == 0 ? 1 : (k == 1 ? 21 : 41);
				assert(GetAlchemist(map, alch) && GetHerbalist(map, herb));
				assert(alch.x != herb.x || alch.y != herb.y);
			}
		}

		// Every service point of every village is on that village's own ground:
		// a table row copied from the wrong kingdom is caught here rather than
		// by a bot walking eighty kilometres to the wrong anvil.
		for (int i = 0; i < 6; ++i)
		{
			const long map = villages[i];
			TTownServices svc;
			assert(GetTownServices(map, svc));
			assert(GetTownPitch(map, p));
			const TPoint* points[8] = {
				&svc.miscMerchant, &svc.weaponMerchant, &svc.armourMerchant,
				&svc.storekeeper, &svc.blacksmith, &svc.stableKeeper,
				&svc.skillReset, &svc.teleporter
			};
			for (int k = 0; k < 8; ++k)
			{
				const long dx = points[k]->x > p.x ? points[k]->x - p.x : p.x - points[k]->x;
				const long dy = points[k]->y > p.y ? points[k]->y - p.y : p.y - points[k]->y;
				assert(dx + dy < 30000);
			}
		}
	}

	// The frontier: three entrances and three gates per shared map, and no two
	// kingdoms sharing one. Read out of the maps' own files - Town.txt carries
	// a general spawn point followed by one pair per empire, and npc.txt puts
	// that kingdom's warp NPC beside each. Chunjo's rows reproduce the
	// constants the AI walked to before this table existed, to the unit, which
	// is what says the other six can be trusted.
	{
		TPoint fp;
		assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_ORC_VALLEY, fp) &&
				fp.x == 270400 && fp.y == 739900);
		assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_DESERT, fp) &&
				fp.x == 221900 && fp.y == 502700);
		assert(GetFrontierGate(EMPIRE_CHUNJO, 64, fp) && fp.x == 269100 && fp.y == 740200);
		assert(GetFrontierGate(EMPIRE_CHUNJO, 63, fp) && fp.x == 219700 && fp.y == 499900);
		// Doyyumhwaji, base (588800,614400): Town.txt (106,1419), (90,78),
		// (1419,754) and the warp NPCs 10008 (103,1444), 10010 (76,60) and
		// 10012 (1458,751) of its npc.txt.
		assert(GetTeleportArrival(EMPIRE_SHINSOO, TELEPORT_FIRE_LAND, fp) &&
				fp.x == 599400 && fp.y == 756300);
		assert(GetTeleportArrival(EMPIRE_CHUNJO, TELEPORT_FIRE_LAND, fp) &&
				fp.x == 597800 && fp.y == 622200);
		assert(GetTeleportArrival(EMPIRE_JINNO, TELEPORT_FIRE_LAND, fp) &&
				fp.x == 730700 && fp.y == 689800);
		assert(GetFrontierGate(EMPIRE_SHINSOO, 62, fp) && fp.x == 599100 && fp.y == 758800);
		assert(GetFrontierGate(EMPIRE_CHUNJO, 62, fp) && fp.x == 596400 && fp.y == 620400);
		assert(GetFrontierGate(EMPIRE_JINNO, 62, fp) && fp.x == 734600 && fp.y == 689500);
		assert(GetTeleportDestinationMap(TELEPORT_FIRE_LAND, EMPIRE_SHINSOO) == 62);

		const long frontiers[4] = { 64, 63, 61, 62 };
		for (int i = 0; i < 4; ++i)
		{
			ETeleportDestination where;
			assert(GetFrontierTeleportDestination(frontiers[i], where));
			TPoint seen[3];
			for (int e = EMPIRE_SHINSOO; e <= EMPIRE_JINNO; ++e)
			{
				TPoint arrival, gate;
				assert(GetTeleportArrival(e, where, arrival));
				assert(GetFrontierGate(e, frontiers[i], gate));
				seen[e - 1] = arrival;
				// A kingdom leaves by the NPC standing beside its own
				// entrance. Until this table existed every bot of every
				// kingdom walked to Chunjo's, which from the far side of the
				// valley is a crossing of the whole map. Twenty thousand is
				// loose on purpose: Jinno's Sohan gate is the furthest of the
				// nine at 10600, and the nearest row of another kingdom on any
				// of these maps is over a hundred thousand away - which is the
				// mistake this is here to catch.
				const long dx = gate.x > arrival.x ? gate.x - arrival.x : arrival.x - gate.x;
				const long dy = gate.y > arrival.y ? gate.y - arrival.y : arrival.y - gate.y;
				assert(dx + dy < 20000);
			}
			// Three kingdoms, three different entrances - a row copied from
			// the wrong one is caught here.
			for (int a = 0; a < 3; ++a)
				for (int b = a + 1; b < 3; ++b)
					assert(seen[a].x != seen[b].x || seen[a].y != seen[b].y);
		}
		// A map with one entrance for everybody is not in the table, and an
		// unknown kingdom answers no rather than reading past it.
		ETeleportDestination unused;
		assert(!GetFrontierTeleportDestination(104, unused));
		assert(!GetFrontierTeleportDestination(71, unused));
		assert(!GetFrontierTeleportDestination(65, unused));
		assert(!GetFrontierGate(EMPIRE_CHUNJO, 104, fp));
		assert(!GetFrontierGate(EMPIRE_NONE, 64, fp));

		// Every kingdom has an easy Monkey Dungeon of its own, and they are
		// three different maps: naming Chunjo's for all three is what left
		// Shinsoo and Jinno without a single horse.
		assert(GetMonkeyEasyMap(EMPIRE_SHINSOO) == 5);
		assert(GetMonkeyEasyMap(EMPIRE_CHUNJO) == 25);
		assert(GetMonkeyEasyMap(EMPIRE_JINNO) == 45);
		assert(IsMonkeyEasyMap(5) && IsMonkeyEasyMap(25) && IsMonkeyEasyMap(45));
		assert(!IsMonkeyEasyMap(108) && !IsMonkeyEasyMap(109));
		// And each is entered by its own kingdom's gate out of its own M2.
		TKingdomGate leg;
		assert(FindKingdomGate(EMPIRE_SHINSOO, 3, 5, leg) &&
				leg.arrival.x == 775200 && leg.arrival.y == 447700);
		assert(FindKingdomGate(EMPIRE_CHUNJO, 23, 25, leg) &&
				leg.arrival.x == 852000 && leg.arrival.y == 447700);
		assert(FindKingdomGate(EMPIRE_JINNO, 43, 45, leg) &&
				leg.arrival.x == 928800 && leg.arrival.y == 447700);
	}

	return 0;
}
