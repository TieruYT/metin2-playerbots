#ifndef __INC_METIN2_PLAYERBOT_EMPIRE_RULES_H__
#define __INC_METIN2_PLAYERBOT_EMPIRE_RULES_H__

// The three kingdoms as pure policy: which maps belong to whom, what a map is
// for, where its services and gates are, and how two characters stand to one
// another. No CHARACTER, no singletons, no engine headers - so all of it can
// be tested without booting a core, the way playerbot_world_rules.h is.
//
// Every number in the tables below was read out of the server's own files by
// tools/dump_world_catalog.py (map/index, Setting.txt, Town.txt, npc.txt with
// the warp destination taken from the NPC's own name, quest/map_warp.quest for
// the Teleporter, and server_attr for the ground). Four of them are already in
// use as hand-written Chunjo constants and match to the unit:
//
//   Teleporter on M1  (51900,153600)   PLAYERBOT_M1_TELEPORTER
//   Teleporter on M2  (136900,240300)  PLAYERBOT_M2_TO_M3_TELEPORTER
//   Desert arrival    (221900,502700)  PLAYERBOT_DESERT_ARRIVAL
//   Orc Valley        (270400,739900)  PLAYERBOT_ORC_VALLEY_ARRIVAL
//
// which is the evidence that the extraction reads these files the way the
// engine does. Do not "derive" Shinsoo or Jinno from Chunjo by adding an
// offset: the three towns are laid out differently, and the shared maps give
// each kingdom its own arrival point.
namespace playerbot_empire_rules
{
	enum EEmpire
	{
		EMPIRE_NONE = 0,
		EMPIRE_SHINSOO = 1,
		EMPIRE_CHUNJO = 2,
		EMPIRE_JINNO = 3,
		EMPIRE_COUNT = 4
	};

	enum EMapRole
	{
		MAP_ROLE_NONE = 0,
		MAP_ROLE_M1,          // the village map a character starts on
		MAP_ROLE_M2,          // the second map: services, mid levels, the gates out
		MAP_ROLE_M3,          // the kingdom's guild map (metin2_map_guild_0N)
		MAP_ROLE_MONKEY_EASY, // that kingdom's own easy Monkey Dungeon
		MAP_ROLE_SHARED       // no owner: the valley, the desert, Sohan, the dungeons
	};

	enum ERelation
	{
		RELATION_SELF = 0,
		RELATION_ALLY,        // same kingdom
		RELATION_ENEMY,       // another kingdom
		RELATION_NEUTRAL      // no kingdom of its own: a monster, an NPC, a stone
	};

	struct TPoint
	{
		long x;
		long y;
	};

	struct TKingdomMaps
	{
		long m1;
		long m2;
		long m3;
		long monkeyEasy;
	};

	// -------------------------------------------------------------------
	//  Which maps belong to which kingdom
	// -------------------------------------------------------------------
	inline TKingdomMaps GetKingdomMaps(int empire)
	{
		TKingdomMaps maps = { 0, 0, 0, 0 };
		switch (empire)
		{
			case EMPIRE_SHINSOO: maps.m1 = 1;  maps.m2 = 3;  maps.m3 = 4;  maps.monkeyEasy = 5;  break;
			case EMPIRE_CHUNJO:  maps.m1 = 21; maps.m2 = 23; maps.m3 = 24; maps.monkeyEasy = 25; break;
			case EMPIRE_JINNO:   maps.m1 = 41; maps.m2 = 43; maps.m3 = 44; maps.monkeyEasy = 45; break;
			default: break;
		}
		return maps;
	}

	// The map a bot of this kingdom means when it says "my M1". Zero for an
	// unknown kingdom or role - never Chunjo's, or a Jinno bot sent home would
	// arrive in somebody else's village.
	inline long GetHomeMap(int empire, EMapRole role)
	{
		const TKingdomMaps maps = GetKingdomMaps(empire);
		switch (role)
		{
			case MAP_ROLE_M1: return maps.m1;
			case MAP_ROLE_M2: return maps.m2;
			case MAP_ROLE_M3: return maps.m3;
			case MAP_ROLE_MONKEY_EASY: return maps.monkeyEasy;
			default: return 0;
		}
	}

	// What a map is, whoever is standing on it. A shared map has no owner;
	// EMPIRE_NONE here means "belongs to nobody", not "hostile to everybody".
	inline int GetMapOwnerEmpire(long mapIndex)
	{
		switch (mapIndex)
		{
			case 1: case 3: case 4: case 5: return EMPIRE_SHINSOO;
			case 21: case 23: case 24: case 25: return EMPIRE_CHUNJO;
			case 41: case 43: case 44: case 45: return EMPIRE_JINNO;
			default: return EMPIRE_NONE;
		}
	}

	inline EMapRole GetMapRole(long mapIndex)
	{
		switch (mapIndex)
		{
			case 1: case 21: case 41: return MAP_ROLE_M1;
			case 3: case 23: case 43: return MAP_ROLE_M2;
			case 4: case 24: case 44: return MAP_ROLE_M3;
			case 5: case 25: case 45: return MAP_ROLE_MONKEY_EASY;
			default: return MAP_ROLE_SHARED;
		}
	}

	// "Is this an M2?" and "is this MY M2?" are two questions, and the second
	// is the one travel has to ask. A Jinno bot standing on Shinsoo's map 3 is
	// on an M2 and is not at home.
	inline bool IsHomeMap(int empire, long mapIndex)
	{
		return GetMapOwnerEmpire(mapIndex) == empire && empire != EMPIRE_NONE;
	}

	inline bool IsHomeMapOfRole(int empire, long mapIndex, EMapRole role)
	{
		return mapIndex != 0 && GetHomeMap(empire, role) == mapIndex;
	}

	inline bool IsKingdomMap(long mapIndex)
	{
		return GetMapOwnerEmpire(mapIndex) != EMPIRE_NONE;
	}

	// Every kingdom has its own easy dungeon and they are three different maps
	// with the same geometry. Which one a bot may use follows the bot, never
	// the map it happens to be standing on.
	inline long GetMonkeyEasyMap(int empire)
	{
		return GetKingdomMaps(empire).monkeyEasy;
	}

	inline bool IsMonkeyEasyMap(long mapIndex)
	{
		return mapIndex == 5 || mapIndex == 25 || mapIndex == 45;
	}

	// -------------------------------------------------------------------
	//  Relations
	// -------------------------------------------------------------------
	// A relation is what a bot knows, not a decision to attack: whether it may
	// strike is the engine's question (BANPK, PK protection, High Risk) and the
	// AI's policy question, both asked later and separately.
	inline ERelation GetRelation(int myEmpire, int otherEmpire)
	{
		if (otherEmpire == EMPIRE_NONE || myEmpire == EMPIRE_NONE)
			return RELATION_NEUTRAL;
		return myEmpire == otherEmpire ? RELATION_ALLY : RELATION_ENEMY;
	}

	inline bool IsEnemyEmpire(int myEmpire, int otherEmpire)
	{
		return GetRelation(myEmpire, otherEmpire) == RELATION_ENEMY;
	}

	inline bool IsAllyEmpire(int myEmpire, int otherEmpire)
	{
		return GetRelation(myEmpire, otherEmpire) == RELATION_ALLY;
	}

	// -------------------------------------------------------------------
	//  How many bots each kingdom runs
	// -------------------------------------------------------------------
	// One budget, three kingdoms, and every core works out the same split from
	// the same registry - which is what lets three processes each start their
	// own kingdom without asking one another anything.
	//
	// Equal shares, capped by the identities that actually exist, and whatever
	// that leaves over goes to the kingdoms with spare identities. The degenerate
	// case is the one that runs today: with Chunjo the only seeded kingdom it
	// gets the whole budget, so an operator who has not seeded Shinsoo or Jinno
	// sees exactly the population the slider has always given.
	//
	// registered[e] and out[e] are indexed by EEmpire; index 0 is unused.
	inline void SplitPopulation(int total, const int* registered, int* out)
	{
		for (int e = 0; e < EMPIRE_COUNT; ++e)
			out[e] = 0;
		if (total <= 0 || !registered || !out)
			return;
		int kingdoms = 0;
		for (int e = EMPIRE_SHINSOO; e <= EMPIRE_JINNO; ++e)
			if (registered[e] > 0)
				++kingdoms;
		if (kingdoms == 0)
			return;
		const int share = total / kingdoms;
		int given = 0;
		for (int e = EMPIRE_SHINSOO; e <= EMPIRE_JINNO; ++e)
		{
			if (registered[e] <= 0)
				continue;
			out[e] = share < registered[e] ? share : registered[e];
			given += out[e];
		}
		// The remainder - both the division's and whatever a kingdom short of
		// identities could not take - goes round the kingdoms that still have
		// spare ones, one at a time, so the order cannot give anybody two.
		bool progress = true;
		while (given < total && progress)
		{
			progress = false;
			for (int e = EMPIRE_SHINSOO; e <= EMPIRE_JINNO && given < total; ++e)
			{
				if (out[e] >= registered[e])
					continue;
				++out[e];
				++given;
				progress = true;
			}
		}
	}

	// -------------------------------------------------------------------
	//  The points a bot walks to, per kingdom
	// -------------------------------------------------------------------
	// npc.txt, in world coordinates. Only the services the AI has code for.
	struct TTownServices
	{
		TPoint miscMerchant;
		TPoint weaponMerchant;   // 9004: absent in these villages, filled with 9003
		TPoint armourMerchant;
		TPoint storekeeper;
		TPoint blacksmith;
		TPoint stableKeeper;
		TPoint skillReset;       // 9006, the old woman
		TPoint guard;
		TPoint teleporter;       // 9012, the quest NPC every long trip goes through
	};

	struct TTownServiceRow
	{
		long mapIndex;
		TTownServices services;
	};

	inline bool GetTownServices(long mapIndex, TTownServices& out)
	{
		// misc, weapon, armour, storekeeper, blacksmith, stable, skill reset,
		// guard, teleporter. These villages have no separate 9004 weapon
		// merchant, so that slot repeats the 9003 the AI actually buys from.
		static const TTownServiceRow rows[] = {
			// Shinsoo M1, metin2_map_a1
			{ 1, { { 477400, 952500 }, { 477400, 952500 }, { 469200, 956500 }, { 477000, 956700 }, { 476800, 951600 }, { 481400, 953700 }, { 472900, 958200 }, { 469200, 951700 }, { 465100, 958500 } } },
			// Shinsoo M2, metin2_map_a3
			{ 3, { { 349400, 880000 }, { 349400, 880000 }, { 354300, 880000 }, { 350200, 876600 }, { 349500, 881000 }, { 362000, 878000 }, { 355900, 885800 }, { 352200, 880000 }, { 357200, 876700 } } },
			// Chunjo M1, metin2_map_b1 (Joan)
			{ 21, { { 59000, 171300 }, { 59000, 171300 }, { 67600, 164100 }, { 60900, 162000 }, { 59400, 171600 }, { 54900, 163400 }, { 58800, 165700 }, { 67600, 168600 }, { 51900, 153600 } } },
			// Chunjo M2, metin2_map_b3 (Bokjung)
			{ 23, { { 141300, 240400 }, { 141300, 240400 }, { 148500, 242200 }, { 149500, 239500 }, { 142000, 239200 }, { 146900, 232400 }, { 144300, 235700 }, { 147200, 243500 }, { 136900, 240300 } } },
			// Jinno M1, metin2_map_c1
			{ 41, { { 959900, 274100 }, { 959900, 274100 }, { 961900, 263400 }, { 953100, 260800 }, { 960900, 274000 }, { 961200, 278300 }, { 963300, 271900 }, { 964600, 265500 }, { 964800, 273300 } } },
			// Jinno M2, metin2_map_c3
			{ 43, { { 867600, 243500 }, { 867600, 243500 }, { 862700, 251500 }, { 858600, 243800 }, { 868300, 244700 }, { 871900, 242100 }, { 862500, 242400 }, { 865200, 251500 }, { 867200, 240300 } } },
		};
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].services;
				return true;
			}
		}
		return false;
	}

	// The warp NPCs that join a kingdom's own four maps, both ends. `gate` is
	// where the NPC stands (the bot walks to it), `arrival` is where the engine
	// puts the character down, read from the NPC's own name.
	struct TKingdomGate
	{
		long fromMap;
		long toMap;
		TPoint gate;
		TPoint arrival;
	};

	// Ordered per kingdom: M1->M2, M2->M1, M2->M3, M3->M2, M2->easy, easy->M2.
	inline int GetKingdomGates(int empire, TKingdomGate* out, int max)
	{
		static const TKingdomGate shinsoo[6] = {
			{ 1, 3, { 450100, 903300 }, { 400200, 899500 } },
			{ 3, 1, { 402000, 899600 }, { 450300, 905000 } },
			{ 3, 4, { 321700, 901500 }, { 135600, 4300 } },
			{ 4, 3, { 135400, 3800 },   { 321100, 900900 } },
			{ 3, 5, { 407400, 875700 }, { 775200, 447700 } },
			{ 5, 3, { 775200, 447100 }, { 406200, 875600 } },
		};
		static const TKingdomGate chunjo[6] = {
			{ 21, 23, { 87600, 215100 },  { 111800, 216100 } },
			{ 23, 21, { 113000, 213600 }, { 87600, 213100 } },
			{ 23, 24, { 116000, 292800 }, { 221900, 9300 } },
			{ 24, 23, { 222000, 8800 },   { 116800, 292000 } },
			{ 23, 25, { 161700, 211900 }, { 852000, 447700 } },
			{ 25, 23, { 852000, 447100 }, { 161100, 213000 } },
		};
		static const TKingdomGate jinno[6] = {
			{ 41, 43, { 935300, 217400 }, { 906400, 221400 } },
			{ 43, 41, { 909300, 219900 }, { 937200, 218800 } },
			{ 43, 44, { 910600, 295500 }, { 271800, 13000 } },
			{ 44, 43, { 272400, 13300 },  { 909800, 294800 } },
			{ 43, 45, { 872700, 208900 }, { 928800, 447700 } },
			{ 45, 43, { 928800, 447100 }, { 872700, 210000 } },
		};
		const TKingdomGate* table = 0;
		switch (empire)
		{
			case EMPIRE_SHINSOO: table = shinsoo; break;
			case EMPIRE_CHUNJO: table = chunjo; break;
			case EMPIRE_JINNO: table = jinno; break;
			default: return 0;
		}
		const int count = max < 6 ? max : 6;
		for (int i = 0; i < count; ++i)
			out[i] = table[i];
		return count;
	}

	inline bool FindKingdomGate(int empire, long fromMap, long toMap, TKingdomGate& out)
	{
		TKingdomGate gates[6];
		const int count = GetKingdomGates(empire, gates, 6);
		for (int i = 0; i < count; ++i)
		{
			if (gates[i].fromMap == fromMap && gates[i].toMap == toMap)
			{
				out = gates[i];
				return true;
			}
		}
		return false;
	}

	// quest/map_warp.quest, NPC 9012: the fare is floor(level/5)*1000 with a
	// floor of 1000 and nothing below level 11, and every destination is per
	// empire. The Teleporter stands on all six village maps.
	enum ETeleportDestination
	{
		TELEPORT_GUILD_MAP = 0,   // the kingdom's own M3
		TELEPORT_ORC_VALLEY,      // 64
		TELEPORT_DESERT,          // 63
		TELEPORT_SOHAN,           // 61
		TELEPORT_DESTINATIONS
	};

	inline bool GetTeleportArrival(int empire, ETeleportDestination where, TPoint& out)
	{
		// [destination][empire - 1]
		static const TPoint table[TELEPORT_DESTINATIONS][3] = {
			{ { 135600, 4300 },   { 179500, 1000 },   { 271800, 13000 } },
			{ { 402100, 673900 }, { 270400, 739900 }, { 321300, 808000 } },
			{ { 217800, 627200 }, { 221900, 502700 }, { 344000, 502500 } },
			{ { 434200, 290600 }, { 375200, 174900 }, { 491800, 173600 } },
		};
		if (empire < EMPIRE_SHINSOO || empire > EMPIRE_JINNO)
			return false;
		if (where < 0 || where >= TELEPORT_DESTINATIONS)
			return false;
		out = table[where][empire - 1];
		return true;
	}

	inline long GetTeleportDestinationMap(ETeleportDestination where, int empire)
	{
		switch (where)
		{
			case TELEPORT_GUILD_MAP: return GetHomeMap(empire, MAP_ROLE_M3);
			case TELEPORT_ORC_VALLEY: return 64;
			case TELEPORT_DESERT: return 63;
			case TELEPORT_SOHAN: return 61;
			default: return 0;
		}
	}

	// map_warp.quest: "if pc.get_level() <= 10 then" refuses, and the fare is
	// the same arithmetic for everybody.
	inline int GetTeleportFee(int level)
	{
		const int fee = (level / 5) * 1000;
		return fee < 1000 ? 1000 : fee;
	}

	inline bool CanUseTeleporter(int level)
	{
		return level > 10;
	}
}

#endif
