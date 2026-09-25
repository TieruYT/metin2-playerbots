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
		EMPIRE_COUNT = 4,      // array size: the ids are 1..3, so index 0 is unused
		EMPIRE_COUNT_REAL = 3  // how many kingdoms there actually are
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

	// The operator's own number for each kingdom instead of a share of one
	// budget (the launcher's "Indywidualne wartosci dla krolestw", Greess).
	// Each kingdom takes what it was asked for, cut to the identities it has,
	// and nothing it cannot take is handed to another: the operator named
	// every number, so a kingdom short of identities is short, not generous.
	inline void TakeKingdomCounts(const int* asked, const int* registered, int* out)
	{
		if (!out)
			return;
		for (int e = 0; e < EMPIRE_COUNT; ++e)
			out[e] = 0;
		if (!asked || !registered)
			return;
		for (int e = EMPIRE_SHINSOO; e <= EMPIRE_JINNO; ++e)
		{
			const int want = asked[e] > 0 ? asked[e] : 0;
			const int have = registered[e] > 0 ? registered[e] : 0;
			out[e] = want < have ? want : have;
		}
	}

	// -------------------------------------------------------------------
	//  The points a bot walks to, per kingdom
	// -------------------------------------------------------------------
	// npc.txt, in world coordinates. Only the services the AI has code for.
	// By vnum, with the names mob_proto actually gives them: 9001 Handlarz
	// Bronia, 9002 Handlarz Zbrojami, 9003 Handlarka Roznosci, 9005 Dozorca,
	// 9006 Starsza Pani, 20016 Kowal, 20349 Stajenny, 9012 Teleporter. Worth
	// stating because 9001 reads like a guard and is not one - the AI has
	// walked to it for weapons since long before this table existed.
	struct TTownServices
	{
		TPoint miscMerchant;     // 9003
		TPoint weaponMerchant;   // 9001
		TPoint armourMerchant;   // 9002
		TPoint storekeeper;      // 9005
		TPoint blacksmith;       // 20016
		TPoint stableKeeper;     // 20349
		TPoint skillReset;       // 9006, the old woman
		TPoint teleporter;       // 9012, the quest NPC every long trip goes through
	};

	struct TTownServiceRow
	{
		long mapIndex;
		TTownServices services;
	};

	inline bool GetTownServices(long mapIndex, TTownServices& out)
	{
		// The rows are in the struct's own order: misc, weapon, armour,
		// storekeeper, blacksmith, stable, skill reset, teleporter. Chunjo's
		// two rows reproduce, to the unit, the constants this AI has walked to
		// since long before the table existed - which is what says the reading
		// of npc.txt is right and the other four rows can be trusted.
		static const TTownServiceRow rows[] = {
			// Shinsoo M1, metin2_map_a1
			{ 1, { { 477400, 952500 }, { 469200, 951700 }, { 469200, 956500 }, { 477000, 956700 }, { 476800, 951600 }, { 481400, 953700 }, { 472900, 958200 }, { 465100, 958500 } } },
			// Shinsoo M2, metin2_map_a3
			{ 3, { { 349400, 880000 }, { 352200, 880000 }, { 354300, 880000 }, { 350200, 876600 }, { 349500, 881000 }, { 362000, 878000 }, { 355900, 885800 }, { 357200, 876700 } } },
			// Chunjo M1, metin2_map_b1 (Joan)
			{ 21, { { 59000, 171300 }, { 67600, 168600 }, { 67600, 164100 }, { 60900, 162000 }, { 59400, 171600 }, { 54900, 163400 }, { 58800, 165700 }, { 51900, 153600 } } },
			// Chunjo M2, metin2_map_b3 (Bokjung)
			{ 23, { { 141300, 240400 }, { 147200, 243500 }, { 148500, 242200 }, { 149500, 239500 }, { 142000, 239200 }, { 146900, 232400 }, { 144300, 235700 }, { 136900, 240300 } } },
			// Jinno M1, metin2_map_c1
			{ 41, { { 959900, 274100 }, { 964600, 265500 }, { 961900, 263400 }, { 953100, 260800 }, { 960900, 274000 }, { 961200, 278300 }, { 963300, 271900 }, { 964800, 273300 } } },
			// Jinno M2, metin2_map_c3
			{ 43, { { 867600, 243500 }, { 865200, 251500 }, { 862700, 251500 }, { 858600, 243800 }, { 868300, 244700 }, { 871900, 242100 }, { 862500, 242400 }, { 867200, 240300 } } },
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

	// The centre of a village's stall ring: where a keeper opens its counter and
	// where a shopper looks for one. Chunjo's two are the points the AI has
	// always used, and both stand beside that kingdom's guard (11002) in the
	// middle of the village's round square. The other four used to be the
	// centroid of the village's eight service NPCs, which put Jayang's market
	// among the merchants on the square's edge ("sklepy sa zle rozstawione,
	// bardziej przy handlarzach niz przy kole, straznik ... na kordach 457,
	// 630", Tieru, 15 September) and ran Pyongmoo's ring half out of the safe
	// zone. They are the cell under the kingdom's own guard now - 11000 in
	// Shinsoo, 11004 in Jinno, read from each map's npc.txt - and server_attr
	// says the whole ring of 400..1700 round every one is open safe ground.
	struct TTownPitchRow
	{
		long mapIndex;
		TPoint pitch;
	};

	inline bool GetTownPitch(long mapIndex, TPoint& out)
	{
		static const TTownPitchRow rows[] = {
			{ 1,  { 474325, 954225 } },
			{ 3,  { 353025, 882325 } },
			{ 21, { 63400, 166300 } },
			{ 23, { 145500, 240000 } },
			{ 41, { 959925, 268825 } },
			{ 43, { 863425, 246025 } },
		};
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].pitch;
				return true;
			}
		}
		return false;
	}

	// The eight profession trainers of a first village: [job][group], with job
	// as CHARACTER::GetJob returns it (0..3) and group 1 or 2. Shinsoo's are
	// 20300..20307, Chunjo's 20320..20327, Jinno's 20340..20347, and in every
	// kingdom the even vnum of a pair teaches the first group.
	//
	// Only the first villages have them. That is not an omission in this table:
	// npc.txt puts no trainer on any second village, which is exactly why a bot
	// with no skill group has to be sent back to M1 to choose one.
	struct TSkillTrainerRow
	{
		long mapIndex;
		TPoint trainers[4][2];
	};

	inline bool GetSkillTrainer(long mapIndex, int job, int group, TPoint& out)
	{
		static const TSkillTrainerRow rows[] = {
			{ 1, { { { 471800, 951600 }, { 472100, 951500 } }, { { 472500, 951400 }, { 472900, 951300 } }, { { 474000, 951100 }, { 474300, 951000 } }, { { 474700, 950900 }, { 475000, 950800 } } } },
			{ 21, { { { 62300, 161800 }, { 62700, 161800 } }, { { 63100, 161900 }, { 63500, 161900 } }, { { 64500, 161900 }, { 64900, 161900 } }, { { 65300, 161900 }, { 65700, 161900 } } } },
			{ 41, { { { 966000, 267100 }, { 966000, 267500 } }, { { 966000, 267900 }, { 966000, 268300 } }, { { 965900, 269200 }, { 965900, 269600 } }, { { 965900, 270000 }, { 965900, 270400 } } } },
		};
		if (job < 0 || job > 3 || (group != 1 && group != 2))
			return false;
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].trainers[job][group - 1];
				return true;
			}
		}
		return false;
	}

	inline bool HasSkillTrainers(long mapIndex)
	{
		TPoint unused;
		return GetSkillTrainer(mapIndex, 0, 1, unused);
	}

	// The Biologist, mob 20084. One per first village and none anywhere else,
	// so his collection is an M1 errand in every kingdom just as it is in Joan.
	inline bool GetBiologist(long mapIndex, TPoint& out)
	{
		static const TTownPitchRow rows[] = {
			{ 1,  { 499200, 957000 } },
			{ 21, { 89800, 182100 } },
			{ 41, { 950100, 233300 } },
		};
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].pitch;
				return true;
			}
		}
		return false;
	}

	// Baek-Go, mob 20018 - the herbalist, who is NOT the Biologist above: two
	// NPCs, both in every first village and nowhere else. His crafting board is
	// what turns the herbs a bot picks up into potions, so herbalism is an M1
	// errand exactly like the Biologist's hand-in, and a bot that walks to one
	// is already standing beside the other. Read off each map's npc.txt through
	// its own BasePosition (cell * 100 + base), the way the Biologist's row was.
	inline bool GetHerbalist(long mapIndex, TPoint& out)
	{
		static const TTownPitchRow rows[] = {
			{ 1,  { 479100, 961700 } },
			{ 21, { 67400, 161400 } },
			{ 41, { 968100, 266000 } },
		};
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].pitch;
				return true;
			}
		}
		return false;
	}

	// The Alchemist, mob 20001: one in each first village and nowhere else,
	// and the NPC whose item exchange takes a soul stone of +0 to +3 for
	// Magiczny Pyl (item_exchange.lua). Read off npc.txt the same way - cells
	// (622,511), (660,734) and (292,812) - and checked on each map's
	// server_attr to stand on the ground the herbalist and the market pitch
	// stand on.
	inline bool GetAlchemist(long mapIndex, TPoint& out)
	{
		static const TTownPitchRow rows[] = {
			{ 1,  { 471800, 947100 } },
			{ 21, { 66000, 175800 } },
			{ 41, { 950800, 286000 } },
		};
		for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
		{
			if (rows[i].mapIndex == mapIndex)
			{
				out = rows[i].pitch;
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
		TELEPORT_FIRE_LAND,       // 62, Doyyumhwaji: the Teleporter's second page
		TELEPORT_DESTINATIONS
	};

	inline bool GetTeleportArrival(int empire, ETeleportDestination where, TPoint& out)
	{
		// [destination][empire - 1]
		//
		// The guild-map row is each map's own Town.txt, not the Teleporter
		// quest's empire table: for Chunjo that table said (179500, 1000),
		// which is cell (3, 10) of metin2_map_guild_02 - the unwalkable
		// north-west corner. A bot warped there had no route to anything,
		// the watchdog reset it every ninety seconds at the same spot, and
		// nothing could move it (greess, 11 September: pid 16 at
		// (179500, 1000), nav_out=1, confirmed twice more). playerbot_types.h
		// had already learned this for Chunjo; the kingdom table reintroduced
		// the quest's number when the three-kingdom travel replaced the
		// constant. Town.txt: guild_01 base (128000,0) + (74,55),
		// guild_02 base (179200,0) + (427,92), guild_03 base (230400,0) +
		// (405,127).
		//
		// Doyyumhwaji is metin2_map_n_flame_01's Town.txt the same way, base
		// (588800,614400): Shinsoo (106,1419) in the north-west corner, Chunjo
		// (90,78) in the south-west, Jinno (1419,754) on the east edge. All
		// three stand on open ground of the map's one walkable area and none in
		// its safe zone (server_attr, 23 September).
		static const TPoint table[TELEPORT_DESTINATIONS][3] = {
			{ { 135400, 5500 },   { 221900, 9200 },   { 270900, 12700 } },
			{ { 402100, 673900 }, { 270400, 739900 }, { 321300, 808000 } },
			{ { 217800, 627200 }, { 221900, 502700 }, { 344000, 502500 } },
			{ { 434200, 290600 }, { 375200, 174900 }, { 491800, 173600 } },
			{ { 599400, 756300 }, { 597800, 622200 }, { 730700, 689800 } },
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
			case TELEPORT_FIRE_LAND: return 62;
			default: return 0;
		}
	}

	// The same four maps from the other side: which destination a shared map
	// is, so a caller holding a map index can ask the table above for the
	// arrival of the kingdom it is carrying. Everything else - the two Spider
	// Dungeons and Hwang - has one entry point for all three kingdoms and is
	// not in here.
	inline bool GetFrontierTeleportDestination(long mapIndex, ETeleportDestination& out)
	{
		switch (mapIndex)
		{
			case 64: out = TELEPORT_ORC_VALLEY; return true;
			case 63: out = TELEPORT_DESERT; return true;
			case 61: out = TELEPORT_SOHAN; return true;
			case 62: out = TELEPORT_FIRE_LAND; return true;
			default: return false;
		}
	}

	// Where a kingdom leaves a shared map: its own warp NPC, a few steps from
	// where that kingdom's characters are put down. npc.txt gives three per
	// map and they pair with the three Town.txt entries above - the valley and
	// Sohan number them 10007/10009/10011 for Shinsoo/Chunjo/Jinno, the desert
	// and Doyyumhwaji 10008/10010/10012. Doyyumhwaji's three name the second
	// villages (Jayang, Bokjung, Bakra), as the desert's do.
	//
	// This is the other half of the arrival table. Until it existed every bot
	// of every kingdom walked to Chunjo's gate to go home, which on the far
	// side of the valley is a seventy-kilometre crossing of hostile ground -
	// and arrived through Chunjo's entrance in the first place ("wszystkie boty
	// po wejsciu do doliny, nie zaleznie od krolestwa, wchodza w miejscu
	// wejscia zoltych", SIZOWSKI, 13 September).
	inline bool GetFrontierGate(int empire, long mapIndex, TPoint& out)
	{
		if (empire < EMPIRE_SHINSOO || empire > EMPIRE_JINNO)
			return false;
		// [map][empire - 1]
		static const TPoint valley[3] = { { 403200, 672900 }, { 269100, 740200 }, { 320000, 809200 } };
		static const TPoint desert[3] = { { 215700, 629000 }, { 219700, 499900 }, { 344000, 500000 } };
		static const TPoint sohan[3]  = { { 433600, 295100 }, { 374000, 181900 }, { 497600, 168800 } };
		static const TPoint fireLand[3] = { { 599100, 758800 }, { 596400, 620400 }, { 734600, 689500 } };
		switch (mapIndex)
		{
			case 64: out = valley[empire - 1]; return true;
			case 63: out = desert[empire - 1]; return true;
			case 61: out = sohan[empire - 1]; return true;
			case 62: out = fireLand[empire - 1]; return true;
			default: return false;
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
