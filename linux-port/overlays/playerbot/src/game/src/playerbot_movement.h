#ifndef __INC_METIN2_PLAYERBOT_MOVEMENT_H__
#define __INC_METIN2_PLAYERBOT_MOVEMENT_H__

// Getting a bot from where it is to where it wants to be, and remembering what
// is worth going to.
//
// Navigation answers "is this cell standable, is there a route". This is the
// layer above: it holds the route a bot is following, decides when to mount,
// walks the waypoints, and crosses the Monkey Dungeon portals. The known-metin
// registry lives here too rather than with the other world memory, because
// whether a stone is worth remembering is decided by whether anyone can reach
// it - the two cannot be separated without passing reachability back in.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// from playerbot_manager.cpp, after playerbot_navigation.h.

// CDungeon is only forward-declared by the engine headers the manager
// includes; the floor helpers below need the class.
#include "dungeon.h"

namespace
{
	struct TKnownPlayerBotMetin
	{
		TKnownPlayerBotMetin() : lMapIndex(0), lX(0), lY(0), bLevel(0),
			dwLastSeenTime(0), dwReservedByPID(0), dwReserveUntil(0) {}
		long lMapIndex;
		long lX;
		long lY;
		BYTE bLevel;
		DWORD dwLastSeenTime;
		DWORD dwReservedByPID;
		DWORD dwReserveUntil;
	};

	typedef std::map<DWORD, TKnownPlayerBotMetin> TKnownPlayerBotMetinMap;
	TKnownPlayerBotMetinMap s_mapKnownPlayerBotMetins;
	// Twelve stone hotspots per first village, and a village per kingdom: what
	// is learned about Yongan's north field says nothing about Joan's. Indexing
	// one array of twelve by hotspot alone blended three maps into one score.
	DWORD s_adwPlayerBotMetinHotspotVisits[3][12] = { { 0 } };
	DWORD s_adwPlayerBotMetinHotspotFinds[3][12] = { { 0 } };
	DWORD s_adwPlayerBotMetinHotspotLastFind[3][12] = { { 0 } };

	// The kingdom whose statistics a map's stones belong to, 0..2, or -1 for a
	// map that keeps none.
	int GetPlayerBotMetinHotspotSlot(long mapIndex)
	{
		const int empire = playerbot_empire_rules::GetMapOwnerEmpire(mapIndex);
		if (empire < playerbot_empire_rules::EMPIRE_SHINSOO ||
				empire > playerbot_empire_rules::EMPIRE_JINNO)
			return -1;
		return empire - 1;
	}

	// The Monkey Dungeon is ten or eleven chambers, and the GOTO NPCs are the
	// only way between them.
	//
	// server_attr says the maze is not one walkable space: the rooms of
	// regen.txt and the two boss rooms are separate connected components, and
	// warp_npc_event teleports any player within 300 units of a GOTO NPC, twice
	// a second, so walking up to one is the whole crossing and nothing is
	// clicked. Room choice used to ask CPlayerBotNavigation::CanReach, which
	// can only answer for the component the bot is standing in, so every bot
	// found exactly one room - its own - and walked to the same spawn point as
	// everybody else who had come in by the entrance. An hour of logs held 130
	// trips into the dungeon and not one planned crossing, and every
	// PLAYERBOT_SPOT density cell of all three dungeons sat in the entrance
	// chamber.
	//
	// The spots are the distinct spawn cells of each room, as cells off the
	// dungeon's base. Those are the same in all three dungeons - one
	// server_attr, one set of regen cells - and only the hard one populates the
	// eleventh chamber, which the other two still reach.
	struct TPlayerBotMonkeySpot
	{
		short x;
		short y;
	};

	struct TPlayerBotMonkeyChamber
	{
		const TPlayerBotMonkeySpot* spots;
		BYTE bSpotCount;
	};

	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_0[] = {   // NO1, the entrance
		{75, 162}, {76, 197}, {78, 234}, {81, 272}, {100, 213}, {142, 216},
		{144, 277}, {145, 244}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_1[] = {   // NO5
		{243, 496}, {251, 362}, {251, 393}, {252, 427}, {279, 494}, {281, 428},
		{283, 362}, {310, 431}, {310, 459}, {310, 494}, {319, 359}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_2[] = {   // NO7
		{115, 747}, {123, 647}, {123, 677}, {125, 614}, {128, 549}, {148, 747},
		{151, 681}, {160, 614}, {166, 549}, {182, 713}, {183, 683}, {183, 746},
		{194, 551}, {195, 579}, {195, 615}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_3[] = {   // NO8
		{538, 57}, {539, 79}, {545, 136}, {546, 167}, {549, 200}, {550, 267},
		{551, 229}, {582, 137}, {582, 201}, {586, 201}, {613, 60}, {617, 98},
		{619, 138}, {620, 168}, {623, 202}, {624, 271}, {627, 233}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_4[] = {   // NO2
		{228, 192}, {228, 230}, {237, 172}, {239, 108}, {252, 240}, {278, 107},
		{281, 175}, {289, 239}, {300, 156}, {301, 118}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_5[] = {   // NO3
		{417, 116}, {418, 178}, {423, 239}, {450, 117}, {484, 121}, {484, 163},
		{486, 210}, {487, 253}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_6[] = {   // NO4
		{105, 370}, {107, 435}, {109, 499}, {136, 367}, {138, 428}, {139, 496},
		{164, 372}, {164, 400}, {164, 434}, {165, 496}, {168, 462}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_7[] = {   // NO6
		{425, 383}, {427, 355}, {428, 416}, {429, 449}, {431, 482}, {460, 352},
		{460, 416}, {461, 482}, {493, 415}, {494, 482}, {495, 349}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_8[] = {   // NO9
		{310, 568}, {310, 596}, {310, 705}, {313, 674}, {314, 637}, {341, 704},
		{342, 566}, {374, 567}, {374, 674}, {374, 701}, {375, 595}, {377, 632},
		{404, 702}, {406, 563}, {437, 591}, {438, 563}, {440, 631}, {440, 704},
		{441, 674}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_9[] = {   // boss1
		{573, 355}, {573, 367}, {579, 343}, {579, 376}, {580, 356}, {584, 365},
		{585, 348}, {588, 358}, {593, 344}, {596, 332}, {596, 353}, {598, 381},
		{604, 355}, {611, 368}
	};
	const TPlayerBotMonkeySpot PLAYERBOT_MONKEY_SPOTS_10[] = {   // boss2
		{608, 516}, {608, 536}, {608, 546}, {611, 556}, {612, 558}, {614, 535},
		{619, 494}, {619, 515}, {619, 548}, {627, 557}, {630, 489}, {630, 495},
		{630, 506}, {630, 518}, {631, 540}, {635, 549}, {640, 493}, {642, 516},
		{644, 537}, {644, 543}, {649, 552}, {653, 514}, {659, 538}, {661, 549}
	};

	const TPlayerBotMonkeyChamber PLAYERBOT_MONKEY_CHAMBERS[] = {
		{ PLAYERBOT_MONKEY_SPOTS_0, 8 },
		{ PLAYERBOT_MONKEY_SPOTS_1, 11 },
		{ PLAYERBOT_MONKEY_SPOTS_2, 15 },
		{ PLAYERBOT_MONKEY_SPOTS_3, 17 },
		{ PLAYERBOT_MONKEY_SPOTS_4, 10 },
		{ PLAYERBOT_MONKEY_SPOTS_5, 8 },
		{ PLAYERBOT_MONKEY_SPOTS_6, 11 },
		{ PLAYERBOT_MONKEY_SPOTS_7, 11 },
		{ PLAYERBOT_MONKEY_SPOTS_8, 19 },
		{ PLAYERBOT_MONKEY_SPOTS_9, 14 },
		{ PLAYERBOT_MONKEY_SPOTS_10, 24 }
	};

	const int PLAYERBOT_MONKEY_CHAMBER_COUNT =
			(int)(sizeof(PLAYERBOT_MONKEY_CHAMBERS) / sizeof(PLAYERBOT_MONKEY_CHAMBERS[0]));
	const int PLAYERBOT_MONKEY_MAX_SPOTS = 32;
	const int PLAYERBOT_MONKEY_MAX_DOORS = 40;
	const DWORD PLAYERBOT_MONKEY_GEOMETRY_RETRY_TIME = 5000;
	// Two reciprocal GOTO NPCs deliberately land about five map cells beside one
	// another, so a door and the door back are recognised by their ends nearly
	// swapping over.
	const int PLAYERBOT_MONKEY_DOOR_PAIR_RANGE = 800;

	// Where a door stands and where it puts you - read off the NPC itself, never
	// tabulated. The three dungeons stand on one server_attr with one set of NPC
	// cells and three different wirings: on the easy map the door at cell
	// (80,308) leads to (106,547), on the medium one to (520,352), and the
	// medium and hard maps carry two doors the easy one does not have at all.
	// A table copied from one of them and used for the other two sent bots to
	// chambers nobody had planned. CHARACTER::StartWarpNPCEvent reads the
	// destination out of the NPC's own name; so does this.
	struct TPlayerBotMonkeyDoor
	{
		long lFromX;
		long lFromY;
		long lToX;
		long lToY;
		DWORD dwFromComponent;
		DWORD dwToComponent;
	};

	struct TPlayerBotMonkeyGeometry
	{
		TPlayerBotMonkeyGeometry() : bReady(false), dwNextAttemptTime(0), nDoorCount(0)
		{
			memset(adwChamber, 0, sizeof(adwChamber));
			memset(aDoors, 0, sizeof(aDoors));
		}
		bool bReady;
		// Walking every sectree of the map is a one-off, but a dungeon whose
		// NPCs are not up yet would have it walked again for every bot on it,
		// four times a second. Try again in a moment instead.
		DWORD dwNextAttemptTime;
		int nDoorCount;
		DWORD adwChamber[PLAYERBOT_MONKEY_CHAMBER_COUNT];
		TPlayerBotMonkeyDoor aDoors[PLAYERBOT_MONKEY_MAX_DOORS];
	};

	TPlayerBotMonkeyGeometry s_aPlayerBotMonkeyGeometry[5];

	struct FPlayerBotCollectMonkeyDoors
	{
		TPlayerBotMonkeyGeometry* geometry;
		long lBaseX;
		long lBaseY;

		FPlayerBotCollectMonkeyDoors(TPlayerBotMonkeyGeometry* g, long baseX, long baseY)
			: geometry(g), lBaseX(baseX), lBaseY(baseY) {}

		void operator()(LPENTITY entity)
		{
			if (!entity || !entity->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER npc = (LPCHARACTER)entity;
			if (!npc->IsGoto() || geometry->nDoorCount >= PLAYERBOT_MONKEY_MAX_DOORS)
				return;

			char szTmp[64];
			long lToX = 0, lToY = 0;
			if (3 != sscanf(npc->GetName(), " %63s %ld %ld ", szTmp, &lToX, &lToY))
				return;

			// The sector neighbourhoods overlap, so the same NPC arrives many times.
			for (int i = 0; i < geometry->nDoorCount; ++i)
				if (geometry->aDoors[i].lFromX == npc->GetX() &&
						geometry->aDoors[i].lFromY == npc->GetY())
					return;

			TPlayerBotMonkeyDoor& door = geometry->aDoors[geometry->nDoorCount++];
			door.lFromX = npc->GetX();
			door.lFromY = npc->GetY();
			door.lToX = lBaseX + lToX * 100L;
			door.lToY = lBaseY + lToY * 100L;
		}
	};

	// One cache per dungeon map. The three kingdoms' easy dungeons are the same
	// maze on three bases and the geometry is read off each map's own NPCs, so
	// Shinsoo's and Jinno's rooms were never the problem - having no slot was:
	// this answered -1 for maps 5 and 45, GetPlayerBotMonkeyGeometry answered
	// NULL, no chamber or door was known there, and every bot in those two
	// dungeons hunted the entrance room while Chunjo's walked all eleven ("in
	// both Kingdoms Bots only farm in starting zone of Ape Dungeon", Dixdros,
	// 14 September).
	int GetPlayerBotMonkeyGeometrySlot(long mapIndex)
	{
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_MONKEY_EASY: return 0;
			case PLAYERBOT_MAP_MONKEY_MEDIUM: return 1;
			case PLAYERBOT_MAP_MONKEY_HARD: return 2;
			case PLAYERBOT_MAP_MONKEY_SHINSOO: return 3;
			case PLAYERBOT_MAP_MONKEY_JINNO: return 4;
			default: return -1;
		}
	}

	// Which component every chamber and every door end falls in, answered once
	// per dungeon: the grid is built from server_attr and never rebuilt, and the
	// NPCs stand where the map put them. Read, not recomputed, on the tick.
	const TPlayerBotMonkeyGeometry* GetPlayerBotMonkeyGeometry(long mapIndex)
	{
		const int slot = GetPlayerBotMonkeyGeometrySlot(mapIndex);
		if (slot < 0)
			return NULL;
		TPlayerBotMonkeyGeometry& geometry = s_aPlayerBotMonkeyGeometry[slot];
		if (geometry.bReady)
			return &geometry;
		const DWORD dwNow = get_dword_time();
		if (geometry.dwNextAttemptTime != 0 && dwNow < geometry.dwNextAttemptTime)
			return NULL;
		geometry.dwNextAttemptTime = dwNow + PLAYERBOT_MONKEY_GEOMETRY_RETRY_TIME;

		long baseX = 0, baseY = 0;
		if (!GetPlayerBotMonkeyBase(mapIndex, baseX, baseY))
			return NULL;
		LPSECTREE_MAP sectreeMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
		if (!sectreeMap)
			return NULL;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		if (!navigation.Init(mapIndex))
			return NULL;

		geometry.nDoorCount = 0;
		FPlayerBotCollectMonkeyDoors collector(&geometry, baseX, baseY);
		const int sectorsX = std::max(1, sectreeMap->m_setting.iWidth / SECTREE_SIZE);
		const int sectorsY = std::max(1, sectreeMap->m_setting.iHeight / SECTREE_SIZE);
		for (int sy = 0; sy < sectorsY; ++sy)
		{
			for (int sx = 0; sx < sectorsX; ++sx)
			{
				const long x = baseX + sx * SECTREE_SIZE + SECTREE_SIZE / 2;
				const long y = baseY + sy * SECTREE_SIZE + SECTREE_SIZE / 2;
				LPSECTREE tree = SECTREE_MANAGER::instance().Get(mapIndex, x, y);
				if (tree)
					tree->ForEachAround(collector);
			}
		}
		if (geometry.nDoorCount == 0)
		{
			// The core has the map but has not spawned its NPCs yet. Answer again
			// on the next tick rather than remember a dungeon with no doors.
			PlayerBotErrThrottled("monkey_no_doors", dwNow,
					"PLAYERBOT_MONKEY: no doors found map=%ld", mapIndex);
			return NULL;
		}

		int resolved = 0;
		for (int i = 0; i < PLAYERBOT_MONKEY_CHAMBER_COUNT; ++i)
		{
			const TPlayerBotMonkeyChamber& chamber = PLAYERBOT_MONKEY_CHAMBERS[i];
			// The component the room agrees on, not the one its first spawn cell
			// happens to fall in: a single cell buried under an object would
			// otherwise name the whole chamber wrong.
			DWORD components[PLAYERBOT_MONKEY_MAX_SPOTS];
			const int count = std::min((int)chamber.bSpotCount, PLAYERBOT_MONKEY_MAX_SPOTS);
			for (int s = 0; s < count; ++s)
				components[s] = navigation.GetComponentAtWorld(
						baseX + chamber.spots[s].x * 100L,
						baseY + chamber.spots[s].y * 100L, 12);
			DWORD best = 0;
			int bestCount = 0;
			for (int s = 0; s < count; ++s)
			{
				if (components[s] == 0)
					continue;
				int agree = 0;
				for (int t = 0; t < count; ++t)
					if (components[t] == components[s])
						++agree;
				if (agree > bestCount)
				{
					bestCount = agree;
					best = components[s];
				}
			}
			geometry.adwChamber[i] = best;
			if (best != 0)
				++resolved;
		}
		for (int i = 0; i < geometry.nDoorCount; ++i)
		{
			TPlayerBotMonkeyDoor& door = geometry.aDoors[i];
			door.dwFromComponent = navigation.GetComponentAtWorld(door.lFromX, door.lFromY, 12);
			door.dwToComponent = navigation.GetComponentAtWorld(door.lToX, door.lToY, 12);
		}
		geometry.bReady = true;
		sys_log(0, "PLAYERBOT_MONKEY: geometry map=%ld chambers=%d/%d doors=%d",
				mapIndex, resolved, PLAYERBOT_MONKEY_CHAMBER_COUNT, geometry.nDoorCount);
		return &geometry;
	}

	int GetPlayerBotMonkeyChamberAt(long mapIndex, long x, long y)
	{
		const TPlayerBotMonkeyGeometry* geometry = GetPlayerBotMonkeyGeometry(mapIndex);
		if (!geometry)
			return -1;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		// Two chambers can run side by side with one blocked cell between them,
		// so the question is which component this bot is standing on, not which
		// one is nearest: a wide search answers with the corridor over the wall.
		const DWORD component = navigation.GetComponentAtWorld(x, y, 1);
		if (component == 0)
			return -1;
		for (int i = 0; i < PLAYERBOT_MONKEY_CHAMBER_COUNT; ++i)
			if (geometry->adwChamber[i] == component)
				return i;
		return -1;
	}

	// The chambers one door away, and the door that reaches each of them.
	int GetPlayerBotMonkeyChamberExits(long mapIndex, int chamber,
			int* outChambers, int* outDoors, int maxExits)
	{
		const TPlayerBotMonkeyGeometry* geometry = GetPlayerBotMonkeyGeometry(mapIndex);
		if (!geometry || chamber < 0 || chamber >= PLAYERBOT_MONKEY_CHAMBER_COUNT)
			return 0;
		const DWORD from = geometry->adwChamber[chamber];
		if (from == 0)
			return 0;

		int found = 0;
		for (int i = 0; i < geometry->nDoorCount && found < maxExits; ++i)
		{
			if (geometry->aDoors[i].dwFromComponent != from)
				continue;
			for (int c = 0; c < PLAYERBOT_MONKEY_CHAMBER_COUNT; ++c)
			{
				if (geometry->adwChamber[c] != geometry->aDoors[i].dwToComponent ||
						c == chamber)
					continue;
				bool already = false;
				for (int k = 0; k < found; ++k)
					if (outChambers[k] == c)
						already = true;
				if (!already)
				{
					outChambers[found] = c;
					outDoors[found] = i;
					++found;
				}
				break;
			}
		}
		return found;
	}

	bool GetPlayerBotMonkeyDoorPosition(long mapIndex, int door, long& outX, long& outY)
	{
		const TPlayerBotMonkeyGeometry* geometry = GetPlayerBotMonkeyGeometry(mapIndex);
		if (!geometry || door < 0 || door >= geometry->nDoorCount)
			return false;
		outX = geometry->aDoors[door].lFromX;
		outY = geometry->aDoors[door].lFromY;
		return true;
	}

	bool IsPlayerBotMonkeyReverseDoor(const TPlayerBotMonkeyGeometry* geometry,
			int candidate, int previous)
	{
		if (!geometry || candidate < 0 || previous < 0 ||
				candidate >= geometry->nDoorCount || previous >= geometry->nDoorCount)
			return false;
		const TPlayerBotMonkeyDoor& a = geometry->aDoors[candidate];
		const TPlayerBotMonkeyDoor& b = geometry->aDoors[previous];
		// Treat a door and the door back as one doorway for ten seconds: if a
		// target dies while the bot is crossing, the next scan must not send it
		// straight back through the one it arrived by.
		return labs(a.lFromX - b.lToX) <= PLAYERBOT_MONKEY_DOOR_PAIR_RANGE &&
				labs(a.lFromY - b.lToY) <= PLAYERBOT_MONKEY_DOOR_PAIR_RANGE &&
				labs(a.lToX - b.lFromX) <= PLAYERBOT_MONKEY_DOOR_PAIR_RANGE &&
				labs(a.lToY - b.lFromY) <= PLAYERBOT_MONKEY_DOOR_PAIR_RANGE;
	}

	// The first door on a way from here to there, for anything that asks to walk
	// across a chamber wall - the wandering never does, because it only ever
	// aims at its own chamber or at one of its doors.
	bool FindPlayerBotMonkeyDoorStep(long mapIndex, long startX, long startY,
			long targetX, long targetY, int blockedReverseOfDoor,
			long& doorX, long& doorY, int& selectedDoor)
	{
		const TPlayerBotMonkeyGeometry* geometry = GetPlayerBotMonkeyGeometry(mapIndex);
		if (!geometry)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		const DWORD startComponent = navigation.GetComponentAtWorld(startX, startY, 6);
		const DWORD targetComponent = navigation.GetComponentAtWorld(targetX, targetY, 6);
		if (startComponent == 0 || targetComponent == 0 || startComponent == targetComponent)
			return false;

		std::queue<DWORD> open;
		std::map<DWORD, DWORD> parentComponent;
		std::map<DWORD, int> parentDoor;
		parentComponent[startComponent] = startComponent;
		open.push(startComponent);

		while (!open.empty() && parentComponent.find(targetComponent) == parentComponent.end())
		{
			const DWORD current = open.front();
			open.pop();
			for (int i = 0; i < geometry->nDoorCount; ++i)
			{
				const TPlayerBotMonkeyDoor& door = geometry->aDoors[i];
				if (door.dwFromComponent != current || door.dwToComponent == 0)
					continue;
				if (IsPlayerBotMonkeyReverseDoor(geometry, i, blockedReverseOfDoor))
					continue;
				if (parentComponent.find(door.dwToComponent) != parentComponent.end())
					continue;
				parentComponent[door.dwToComponent] = current;
				parentDoor[door.dwToComponent] = i;
				open.push(door.dwToComponent);
			}
		}

		if (parentComponent.find(targetComponent) == parentComponent.end())
			return false;

		DWORD cursor = targetComponent;
		int firstDoor = -1;
		while (cursor != startComponent)
		{
			std::map<DWORD, int>::const_iterator doorIt = parentDoor.find(cursor);
			std::map<DWORD, DWORD>::const_iterator parentIt = parentComponent.find(cursor);
			if (doorIt == parentDoor.end() || parentIt == parentComponent.end())
				return false;
			firstDoor = doorIt->second;
			cursor = parentIt->second;
		}
		if (firstDoor < 0)
			return false;

		doorX = geometry->aDoors[firstDoor].lFromX;
		doorY = geometry->aDoors[firstDoor].lFromY;
		selectedDoor = firstDoor;
		return true;
	}

	// Where a bot dropped by a door starts working: the nearest spawn point of
	// the chamber it has landed in. That is also the shortest way out of the
	// door's own three hundred units, which is what stops an arrival being sent
	// straight back through the one it came in by.
	int FindNearestPlayerBotMonkeySpot(long baseX, long baseY,
			const TPlayerBotMonkeyChamber& chamber, long x, long y)
	{
		int best = 0;
		int bestDistance = -1;
		for (int i = 0; i < (int)chamber.bSpotCount; ++i)
		{
			const int distance = DISTANCE_APPROX(
					baseX + chamber.spots[i].x * 100L - x,
					baseY + chamber.spots[i].y * 100L - y);
			if (bestDistance < 0 || distance < bestDistance)
			{
				bestDistance = distance;
				best = i;
			}
		}
		return best;
	}

	bool IsPlayerBotPathClear(long lMapIndex, long startX, long startY, long endX, long endY)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMapIndex);
		if (navigation.Init(lMapIndex))
			return navigation.SegmentClearWorld(startX, startY, endX, endY);

		const int distance = DISTANCE_APPROX(endX - startX, endY - startY);
		const int steps = std::max(1, (distance + PLAYERBOT_NAV_NATIVE_SAMPLE - 1) /
				PLAYERBOT_NAV_NATIVE_SAMPLE);
		for (int i = 0; i <= steps; ++i)
		{
			const long x = startX + ((endX - startX) * i) / steps;
			const long y = startY + ((endY - startY) * i) / steps;
			if (IsPlayerBotPositionBlocked(lMapIndex, x, y))
				return false;
		}
		return true;
	}

	bool IsPlayerBotReachable(long lMapIndex, long startX, long startY, long endX, long endY)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMapIndex);
		if (navigation.Init(lMapIndex))
			return navigation.CanReach(startX, startY, endX, endY);
		return IsPlayerBotPathClear(lMapIndex, startX, startY, endX, endY);
	}

	DWORD GetPlayerBotPartyReservationPID(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		LPCHARACTER leader = ch->GetParty() ? ch->GetParty()->GetLeaderCharacter() : NULL;
		return leader ? leader->GetPlayerID() : ch->GetPlayerID();
	}

	// Defined with the party code in playerbot_manager.cpp; declared in
	// playerbot_travel.h too, which comes later in the include order.
	bool IsPlayerBotHumanLedParty(LPPARTY party);

	// A bot climbing the Demon Tower with a player: in the player's party, the
	// player on the same map. For such a bot the tower's stones are the floor's
	// objective - "takie metiny sie zbija, by zaliczyc kolejne pietra" (Tieru,
	// 16 September) - and no level band applies; for a bot on its own they stay
	// what IsPlayerBotDungeonTriggerStone says, a warp sprung on strangers.
	// The dungeon's floor counter: mt2009's CDungeon carries one (the quests'
	// d.get_level and d.advance_level), r40250's does not.
	int GetPlayerBotDungeonLevel(LPDUNGEON d)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return d ? d->GetLevel() : 0;
#else
		(void)d;
		return 0;
#endif
	}

	void AdvancePlayerBotDungeonLevel(LPDUNGEON d)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (d)
			d->AdvanceLevel();
#else
		(void)d;
#endif
	}

	// A raider of the Demon Tower, or any bot inside its instance
	// (playerbot_demon_tower.h, which comes later in the include order).
	bool IsPlayerBotTowerRaider(LPCHARACTER ch);

	bool IsPlayerBotClimbingWithPlayer(LPCHARACTER ch)
	{
		if (!ch || !ch->GetParty() || !IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		return leader && leader != ch && leader->GetMapIndex() == ch->GetMapIndex();
	}

	bool IsPlayerBotDungeonStoneObjective(LPCHARACTER ch, LPCHARACTER stone)
	{
		return ch && stone && stone->IsStone() && !stone->IsDead() &&
				IsPlayerBotDungeonTriggerStone(stone->GetRaceNum()) &&
				(IsPlayerBotClimbingWithPlayer(ch) || IsPlayerBotTowerRaider(ch));
	}

	void RememberPlayerBotMetin(LPCHARACTER stone, DWORD dwNow)
	{
		// The Demon Tower's quest stones are nobody's hunting ground: see
		// PLAYERBOT_DEVIL_TOWER_STONE_FIRST.
		if (!stone || !stone->IsStone() || stone->IsDead() ||
				IsPlayerBotDungeonTriggerStone(stone->GetRaceNum()))
			return;
		const bool bNewDiscovery = s_mapKnownPlayerBotMetins.find(stone->GetVID()) ==
				s_mapKnownPlayerBotMetins.end();
		TKnownPlayerBotMetin& known = s_mapKnownPlayerBotMetins[stone->GetVID()];
		known.lMapIndex = stone->GetMapIndex();
		known.lX = stone->GetX();
		known.lY = stone->GetY();
		known.bLevel = stone->GetLevel();
		known.dwLastSeenTime = dwNow;

		const TPlayerBotVillageGround* ground = bNewDiscovery &&
				IsPlayerBotM1Map(stone->GetMapIndex())
				? GetPlayerBotVillageGround(stone->GetMapIndex()) : NULL;
		const int slot = ground ? GetPlayerBotMetinHotspotSlot(stone->GetMapIndex()) : -1;
		if (ground != NULL && ground->metinCount > 0 && slot >= 0)
		{
			int nearest = 0;
			int nearestDistance = INT_MAX;
			for (size_t i = 0; i < ground->metinCount && i < 12; ++i)
			{
				const int distance = DISTANCE_APPROX(stone->GetX() - ground->metins[i].x,
						stone->GetY() - ground->metins[i].y);
				if (distance < nearestDistance)
				{
					nearest = (int)i;
					nearestDistance = distance;
				}
			}
			++s_adwPlayerBotMetinHotspotFinds[slot][nearest];
			s_adwPlayerBotMetinHotspotLastFind[slot][nearest] = dwNow;
		}
	}

	bool IsPlayerBotMetinWorthFighting(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
			return false;
		// A floor's objective for a bot climbing with a player: no band at all.
		if (IsPlayerBotDungeonStoneObjective(ch, stone))
			return true;
		// Breaking one warps every PC on the killer's map into a new tower.
		if (IsPlayerBotDungeonTriggerStone(stone->GetRaceNum()))
			return false;
		// Iwakura's stone hunter: ten levels either way, "aby zagwarantowac
		// szanse na drop oraz upewnic sie, ze bot fizycznie da rade go zniszczyc".
		if (IsPlayerBotPersonaEnabled())
			return playerbot_persona::InPogromcaBand((int)ch->GetLevel(), (int)stone->GetLevel());
		// Alone, a stone up to nine over the bot (a stronger one it cannot break
		// by itself - it joins those, IsPlayerBotStoneJoinable), and one it has
		// outgrown by PLAYERBOT_STONE_OUTGROWN_LEVELS is passed: the drop curve
		// is 1% at fifteen over, and nothing comes out of it past that.
		return stone->GetLevel() <= ch->GetLevel() + 9 &&
				(int)ch->GetLevel() <= (int)stone->GetLevel() + PLAYERBOT_STONE_OUTGROWN_LEVELS;
	}

	BYTE ChoosePlayerBotMetinHotspot(DWORD playerID, BYTE currentIndex, DWORD dwNow,
			long mapIndex)
	{
		const int slot = GetPlayerBotMetinHotspotSlot(mapIndex);
		if (slot < 0)
			return (BYTE)(currentIndex % 12);
		BYTE best = currentIndex % 12;
		int bestScore = INT_MIN;
		// Compare four PID-specific candidates. This learns productive areas while
		// keeping different hunters on different routes instead of one global line.
		for (int option = 0; option < 4; ++option)
		{
			const BYTE index = (BYTE)((currentIndex + option * 3 +
					(PlayerBotNavHash(playerID + option * 101U) % 5U)) % 12);
			const int successRate = (int)((s_adwPlayerBotMetinHotspotFinds[slot][index] + 1) * 1000 /
					(s_adwPlayerBotMetinHotspotVisits[slot][index] + 3));
			const int freshness = s_adwPlayerBotMetinHotspotLastFind[slot][index] != 0 &&
					dwNow - s_adwPlayerBotMetinHotspotLastFind[slot][index] < 300000 ? 250 : 0;
			const int personalJitter = (int)(PlayerBotNavHash(playerID ^ (index * 7919U)) % 350U);
			const int score = successRate + freshness + personalJitter;
			if (score > bestScore)
			{
				bestScore = score;
				best = index;
			}
		}
		return best;
	}

	void ReservePlayerBotMetin(LPCHARACTER ch, LPCHARACTER stone, DWORD dwNow)
	{
		if (!IsPlayerBotMetinWorthFighting(ch, stone))
			return;
		RememberPlayerBotMetin(stone, dwNow);
		TKnownPlayerBotMetin& known = s_mapKnownPlayerBotMetins[stone->GetVID()];
		known.dwReservedByPID = GetPlayerBotPartyReservationPID(ch);
		known.dwReserveUntil = dwNow + 45000;
	}

	void ReleasePlayerBotMetinReservation(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || ch->GetParty())
			return;
		TKnownPlayerBotMetinMap::iterator it =
				s_mapKnownPlayerBotMetins.find(stone->GetVID());
		if (it == s_mapKnownPlayerBotMetins.end() ||
				it->second.dwReservedByPID != ch->GetPlayerID())
			return;
		it->second.dwReservedByPID = 0;
		it->second.dwReserveUntil = 0;
	}

	LPCHARACTER FindKnownPlayerBotMetin(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return NULL;

		LPCHARACTER best = NULL;
		int bestScore = INT_MIN;
		const DWORD myReservationPID = GetPlayerBotPartyReservationPID(ch);
		for (TKnownPlayerBotMetinMap::iterator it = s_mapKnownPlayerBotMetins.begin();
				it != s_mapKnownPlayerBotMetins.end(); )
		{
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(it->first);
			if (!stone || !stone->IsStone() || stone->IsDead() ||
					dwNow - it->second.dwLastSeenTime > 300000)
			{
				s_mapKnownPlayerBotMetins.erase(it++);
				continue;
			}

			RememberPlayerBotMetin(stone, dwNow);
			TKnownPlayerBotMetin& known = it->second;
			++it;
			if (known.lMapIndex != ch->GetMapIndex() ||
					!IsPlayerBotMetinWorthFighting(ch, stone))
				continue;
			if (known.dwReserveUntil > dwNow && known.dwReservedByPID != 0 &&
					known.dwReservedByPID != myReservationPID)
				continue;
			if (!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), known.lX, known.lY))
				continue;

			const int distance = DISTANCE_APPROX(ch->GetX() - known.lX, ch->GetY() - known.lY);
			const int levelDelta = abs((int)ch->GetLevel() - (int)known.bLevel);
			const int score = 500000 - distance * 3 - levelDelta * 10000;
			if (!best || score > bestScore)
			{
				best = stone;
				bestScore = score;
			}
		}

		if (best)
			ReservePlayerBotMetin(ch, best, dwNow);
		return best;
	}

	void ClearPlayerBotRoute(TPlayerBotAIState& state, bool clearGoal)
	{
		// A long route that still has somewhere to go is parked, not dropped:
		// whoever clears it - a fight, a pickup - will ask for the same
		// destination again in a moment.
		if (state.uRouteIndex + PLAYERBOT_NAV_PARK_MIN_WAYPOINTS <= state.vecRoute.size() &&
				state.lRouteMapIndex != 0)
		{
			state.vecParkedRoute.swap(state.vecRoute);
			state.lParkedDestX = state.lRouteDestX;
			state.lParkedDestY = state.lRouteDestY;
			state.lParkedMapIndex = state.lRouteMapIndex;
		}
		state.vecRoute.clear();
		state.uRouteIndex = 0;
		state.lIssuedWaypointX = 0;
		state.lIssuedWaypointY = 0;
		state.iNavLastWaypointDistance = -1;
		state.dwNextNavProgressTime = 0;
		state.bNavNoProgressCount = 0;
		state.bNavDeferredCount = 0;
		if (clearGoal)
		{
			state.lRouteDestX = 0;
			state.lRouteDestY = 0;
			state.lRouteMapIndex = 0;
			state.bRouteAllowsHorse = false;
		}
	}

	bool SetPlayerBotRidingForTravel(LPCHARACTER ch, TPlayerBotAIState& state,
			bool shouldRide, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;

		if (!shouldRide)
		{
			if (!ch->IsRiding())
				return false;

			ch->Stop();
			if (!ch->StopRiding())
				return false;

			// StopRiding summons the horse as a follower, so a bot that climbs
			// down in a town square leaves it standing there - which is the herd
			// of horses in every screenshot of Joan and Bokjung. Inside a safe
			// zone the horse is sent away like a player would send it; on a
			// hunting map it stays, because the bot is about to want it again.
			if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
				ch->HorseSummon(false);
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = 0;
			// The pass that climbed down wants the ground for a moment, and the
			// travel used to put the bot back on the horse a second later
			// (PLAYERBOT_HORSE_TRAVEL_FLIP_HOLD_MS).
			state.dwNextHorseRideCheckTime = dwNow + PLAYERBOT_HORSE_TRAVEL_FLIP_HOLD_MS;
			state.dwLastMeaningfulActivityTime = dwNow;
			sys_log(0, "PLAYERBOT_HORSE: dismounted pid=%u name=%s map=%ld pos=(%ld,%ld) reason=%s",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
					reason ? reason : "?");
			return true;
		}

		if (ch->IsRiding() || ch->GetHorseLevel() == 0 ||
				ch->GetHorseHealth() <= 0 || ch->GetHorseStamina() <= 0 ||
				dwNow < state.dwNextHorseRideCheckTime)
			return false;
		// What CHARACTER::StartRiding refuses that this pass can see coming.
		// IsBusy is the whole of it in practice: a counter open, another bot's
		// counter being read, the safebox page, the item shop, a herbalist's
		// craft - all ordinary things a bot does, and every one of them made
		// the mount below fail and write a line to syserr. Measured on the
		// test world on 20 September: 1060 + 775 + 566 ... some thousands of
		// "mount failed" lines in a day, every one of them a bot that was
		// simply doing something else at the time. A refusal the AI could
		// have predicted is not an error; it is a reason to wait.
		// IsBusy is mt2009's; r40250's StartRiding asks nothing of the kind, so
		// there is nothing to predict there beyond these two.
		if (ch->IsDead() || ch->IsPolymorphed()
#if defined(PLAYERBOT_ENGINE_MT2009)
				|| ch->IsBusy()
#endif
				)
		{
			state.dwNextHorseRideCheckTime = dwNow + PLAYERBOT_HORSE_RIDE_RETRY_INTERVAL;
			return false;
		}

		ch->Stop();
		if (!ch->StartRiding())
		{
			state.dwNextHorseRideCheckTime = dwNow + PLAYERBOT_HORSE_RIDE_RETRY_INTERVAL;
			// Throttled, because what is left is whatever the engine refuses
			// for a reason nothing above could ask about - and if that ever
			// becomes common again, one line a minute is enough to see it
			// without burying every other error in the file.
			PlayerBotLogThrottled("horse_mount_failed", dwNow,
					"PLAYERBOT_HORSE: mount failed pid=%u name=%s horse_level=%u health=%d stamina=%d reason=%s",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetHorseLevel(),
					ch->GetHorseHealth(), ch->GetHorseStamina(), reason ? reason : "?");
			return false;
		}

		ClearPlayerBotRoute(state, false);
		state.dwNextNavPlanTime = 0;
		state.dwNextHorseRideCheckTime = dwNow + 1000;
		state.dwLastMeaningfulActivityTime = dwNow;
		sys_log(0, "PLAYERBOT_HORSE: mounted pid=%u name=%s horse_level=%u map=%ld pos=(%ld,%ld) reason=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetHorseLevel(),
				ch->GetMapIndex(), ch->GetX(), ch->GetY(), reason ? reason : "?");
		return true;
	}

	// Defined with the builds (playerbot_skills.h), which come later.
	bool PlayerBotSkillsBeatTheSaddle(LPCHARACTER ch);

	// A battle horse (level 11+) and a weapon that can be swung from it: what a
	// player raises for the stones, whatever the bot then does with it.
	bool HasPlayerBotBattleHorse(LPCHARACTER ch)
	{
		if (!ch || ch->GetHorseLevel() < PLAYERBOT_BATTLE_HORSE_LEVEL)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		return weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() != WEAPON_BOW;
	}

	// A battle horse (level 11+) lets its rider strike from the saddle. Bots that
	// own one should ride into a fight instead of dismounting on the approach, but
	// only when the weapon and target actually make mounted combat sensible.
	// Could this bot fight from the saddle at all, whatever it ends up facing?
	// The horse and the weapon decide that much on their own, and the tick has
	// to know it before a target exists - that is the moment it decides whether
	// to climb down.
	//
	// And the skills decide the rest. No skill of a class can be cast from a
	// saddle (PLAYERBOT_SADDLE_SKILL_LEVEL), so a bot whose attack skills are
	// trained fights on foot like any rider of a transport horse: the target
	// section climbs down when it picks a foe, and so do the duel, the Anti-PK
	// fight and the tower.
	bool CanPlayerBotEverFightOnHorse(LPCHARACTER ch)
	{
		return HasPlayerBotBattleHorse(ch) && !PlayerBotSkillsBeatTheSaddle(ch);
	}

	bool CanPlayerBotFightOnHorse(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!CanPlayerBotEverFightOnHorse(ch))
			return false;

		// Against Metins a battle horse is priority #1: the rider keeps hacking the
		// stone from the saddle rather than climbing down for every spot.
		if (target && target->IsStone())
			return true;

		// Warriors and Suras clear mob spots (multi-pull / valour cloak packs) from
		// horseback; ranged and caster jobs still fight on foot.
		if (ch->GetJob() == JOB_WARRIOR || ch->GetJob() == JOB_SURA)
			return true;

		return false;
	}

	void UpdatePlayerBotTravelMount(LPCHARACTER ch, TPlayerBotAIState& state,
			long destX, long destY, bool allowHorse, DWORD dwNow,
			bool fightOnHorse = false, bool keepHorseAtDestination = false)
	{
		if (!ch)
			return;

		// Mounted combat overrides the travel dismount: the bot is closing on a
		// target it may legitimately hit from the saddle, so keep (or take) the
		// horse regardless of how near the destination is. SetPlayerBotRidingForTravel
		// still refuses gracefully when the horse is spent, leaving the bot on foot.
		//
		// A portal wants the saddle for any distance; the leg's own mount below
		// takes the horse only for a long way.
		if (fightOnHorse || keepHorseAtDestination)
		{
			SetPlayerBotRidingForTravel(ch, state, true, dwNow,
					fightOnHorse ? "mounted_combat" : "riding_to_portal");
			return;
		}

		// The target pass owns the saddle while there is a target it may hit
		// from it: it mounts ("mounted_combat") on its approach, and the leg
		// that brought the bot here - a known-stone walk, a wander leg - used
		// to climb down again the moment the route's end was close, so a
		// warrior with a battle horse next to a Metin mounted and dismounted
		// once a second for as long as the stone stood (botgrom2, V1, a
		// hundred pairs a minute in one operator's bundle).
		if (!fightOnHorse && !keepHorseAtDestination && ch->IsRiding() && state.dwTargetVID != 0)
		{
			LPCHARACTER target = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (target && CanPlayerBotFightOnHorse(ch, target))
				return;
		}
		// The opposite case, and the one the desert bots fell into: a transport
		// horse and a live combat target that must be fought on foot. Mounting for
		// the leg here only to have the combat pass (combat_ready /
		// dismount_for_target) climb down again next tick was the
		// long_travel<->combat_ready thrash - KimJestes2 mounted and dismounted
		// once a second on the desert for minutes, never landing a blow
		// (sizowski). While a fight is pending the saddle is the combat pass's to
		// give up, not this pass's to take; once the foe is gone the next leg
		// mounts as before. Only for a real, live foe, so a stale VID cannot
		// strand the bot on foot.
		if (!fightOnHorse && !keepHorseAtDestination && !CanPlayerBotEverFightOnHorse(ch))
		{
			LPCHARACTER foe = ch->GetVictim();
			if (!foe && state.dwTargetVID != 0)
				foe = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (foe && !foe->IsDead())
				return;
		}
		// A rider keeps the saddle to the end of the leg, and on a leg that does
		// not ask for the horse. Nothing a bot does at the end of one wants the
		// ground on either engine: an NPC, a counter, the anvil, a chest, a book,
		// the gear and a portal all answer a rider (Tieru, 15 September: "Nie
		// trzeba schodzic z konia by przeczytac ksiazke, sciagnac eq, ubrac eq,
		// otworzyc jakies skrzynki, porozmawiac z npc, przejsc przez portal"). The
		// two climb-downs that stood here, near_destination and on_foot_action,
		// were 13 011 of 24 389 in 36 minutes on the test world. What does want
		// the ground gets off by itself: a fight on a transport horse, a duel, a
		// skill, the rod, a polymorph marble, a counter going up.
		if (allowHorse && !ch->IsRiding() &&
				DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) >= PLAYERBOT_HORSE_MOUNT_DISTANCE)
			SetPlayerBotRidingForTravel(ch, state, true, dwNow, "long_travel");
	}

	void BuildPlayerBotStraightRoute(long startX, long startY, long targetX, long targetY,
			std::vector<PIXEL_POSITION>& route)
	{
		route.clear();
		const int distance = DISTANCE_APPROX(targetX - startX, targetY - startY);
		const int segmentCount = std::max(1, (distance + PLAYERBOT_NAV_MAX_SEGMENT - 1) /
				PLAYERBOT_NAV_MAX_SEGMENT);
		for (int segment = 1; segment <= segmentCount; ++segment)
		{
			PIXEL_POSITION point;
			point.x = startX + ((targetX - startX) * segment) / segmentCount;
			point.y = startY + ((targetY - startY) * segment) / segmentCount;
			point.z = 0;
			route.push_back(point);
		}
	}

	// The parked route is the one being asked for again if it is on this map
	// and to the same place; it is taken up at the nearest waypoint the bot can
	// walk straight to. Everything else - a new goal, a different map, a fight
	// that carried the bot off the line - falls through to a fresh plan.
	bool ResumePlayerBotParkedRoute(LPCHARACTER ch, TPlayerBotAIState& state,
			CPlayerBotNavigation& navigation, long mapIndex, long destX, long destY)
	{
		if (state.vecParkedRoute.empty())
			return false;
		if (state.lParkedMapIndex != mapIndex)
		{
			state.vecParkedRoute.clear();
			return false;
		}
		// A different destination is the fight itself - the step towards the
		// monster, the walk to the drop - not a change of mind. The parked
		// route waits for the hub to be asked for again; dropping it here is
		// what left seven resumes a minute against a hundred far plans.
		if (DISTANCE_APPROX(destX - state.lParkedDestX, destY - state.lParkedDestY) >
				PLAYERBOT_NAV_GOAL_REPLAN_DISTANCE)
			return false;
		size_t bestIndex = state.vecParkedRoute.size();
		int bestDistance = PLAYERBOT_NAV_RESUME_DISTANCE;
		for (size_t i = 0; i < state.vecParkedRoute.size(); ++i)
		{
			const PIXEL_POSITION& waypoint = state.vecParkedRoute[i];
			const int distance = DISTANCE_APPROX(ch->GetX() - waypoint.x, ch->GetY() - waypoint.y);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestIndex = i;
			}
		}
		// The waypoints after the nearest one are the walk that is left; the
		// bot must be able to step onto the line, not merely be near it.
		if (bestIndex >= state.vecParkedRoute.size() ||
				!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(),
						state.vecParkedRoute[bestIndex].x, state.vecParkedRoute[bestIndex].y))
		{
			state.vecParkedRoute.clear();
			return false;
		}
		state.vecRoute.swap(state.vecParkedRoute);
		state.vecParkedRoute.clear();
		state.uRouteIndex = bestIndex;
		return true;
	}

	// Which chamber a bot is standing in, kept up to date on the tick.
	//
	// A GOTO portal moves the bot without touching its route, so both the route
	// it was following and the destination anything would re-plan towards
	// belong to the chamber it has just left. Asked for that destination again
	// the navigation finds it unreachable, looks for a portal to it, and finds
	// the door the bot came in by - and since every arrival stands five hundred
	// units from that door while the trigger is three hundred, a bot that keeps
	// walking is a bot that keeps crossing. The logs showed bots three chambers
	// away from the one they believed they were in.
	//
	// This cannot live in the wander pass. That pass runs only on a tick no
	// subsystem claimed, and a bot dropped among aggressive monkeys is fighting,
	// not wandering: two or three doors would go by before it looked. So the
	// crossing is noticed here, once a tick, for every bot in a dungeon.
	void UpdatePlayerBotMonkeyChamber(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow)
	{
		if (!ch || !IsPlayerBotMonkeyMap(ch->GetMapIndex()))
		{
			state.bMonkeyChamber = 255;
			state.bMonkeyPrevChamber = 255;
			return;
		}
		const long mapIndex = ch->GetMapIndex();
		const int chamber = GetPlayerBotMonkeyChamberAt(mapIndex, ch->GetX(), ch->GetY());
		if (chamber < 0 || state.bMonkeyChamber == (BYTE)chamber)
			return;

		long baseX = 0, baseY = 0;
		GetPlayerBotMonkeyBase(mapIndex, baseX, baseY);
		const TPlayerBotMonkeyChamber& room = PLAYERBOT_MONKEY_CHAMBERS[chamber];
		const int previous = state.bMonkeyChamber == 255 ? -1 : (int)state.bMonkeyChamber;
		state.bMonkeyPrevChamber = state.bMonkeyChamber;
		state.bMonkeyChamber = (BYTE)chamber;
		state.dwMonkeyChamberTime = dwNow;
		// The nearest spawn point is both where the monsters are and the
		// shortest way out of the door's own three hundred units.
		state.bMonkeySpot = (BYTE)FindNearestPlayerBotMonkeySpot(
				baseX, baseY, room, ch->GetX(), ch->GetY());
		// Everything the bot was doing belongs to the room it has left. The
		// route used to be cleared with its goal kept - and parked, when long -
		// so the next move towards that goal, or towards the monster it had been
		// fighting, found it unreachable from here and was routed back through
		// the door the bot had just come by. Of 87 returns to the entrance
		// chamber within fifteen seconds, 35 had exactly such a portal route
		// planned in between. The goal, the parked route and the target go with
		// the room; nothing that fights there can follow, because a GOTO door
		// moves only player characters.
		ClearPlayerBotRoute(state, true);
		state.vecParkedRoute.clear();
		state.lParkedMapIndex = 0;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		state.dwNextWanderTime = dwNow;
		sys_log(0, "PLAYERBOT_MONKEY: chamber pid=%u name=%s map=%ld chamber=%d from=%d spots=%u",
				ch->GetPlayerID(), ch->GetName(), mapIndex, chamber, previous,
				(unsigned int)room.bSpotCount);
	}

	// A goal on ground the bot's terrain does not join is not walked to at
	// all: the planner answers "unreachable" three times, the service rescue
	// relocates the bot on the sixth, and the misc merchant of Bokjung - whose
	// approach point sits on a strip cut off from the square - cost 260 such
	// rescues in a morning, six failed plans each. The nearest cell of the
	// bot's own component inside the arrival radius is where the rescue would
	// have put it; ask for it first. The ring's corners reach
	// radius * 50 * sqrt(2), and the arrival test is against the goal asked
	// for, not the moved one: a corner cell past the radius was walked to and
	// never "arrived". True when the goal was moved; walkX/walkY are the goal
	// itself otherwise. The town legs ask it, and so does the walk to the
	// Rybak, whose approach point is drawn by pid round him and fell for some
	// bots on ground the square does not join - every fishing session of
	// theirs ended "route_failed" before a step (23 September).
	bool FindPlayerBotReachableGoal(LPCHARACTER ch, long goalX, long goalY,
			int arrivalDistance, long& walkX, long& walkY)
	{
		walkX = goalX;
		walkY = goalY;
		if (!ch)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
		if (!navigation.Init(ch->GetMapIndex()) ||
				navigation.CanReach(ch->GetX(), ch->GetY(), goalX, goalY))
			return false;
		const DWORD own = navigation.GetComponentAtWorld(ch->GetX(), ch->GetY());
		if (own == 0)
			return false;
		const int radius = std::max(2, arrivalDistance / 71);
		long bestDistance = -1;
		for (int dy = -radius; dy <= radius; ++dy)
			for (int dx = -radius; dx <= radius; ++dx)
			{
				const long cx = goalX + dx * 50, cy = goalY + dy * 50;
				const long distance = dx * dx + dy * dy;
				if (bestDistance >= 0 && distance >= bestDistance)
					continue;
				if (navigation.GetComponentAtWorld(cx, cy, 0) != own)
					continue;
				bestDistance = distance;
				walkX = cx;
				walkY = cy;
			}
		return bestDistance >= 0;
	}

	bool MovePlayerBot(LPCHARACTER ch, long destX, long destY, DWORD dwNow,
			int targetSnapRadius = 4, bool flexibleTargetSnap = false,
			bool allowHorse = false, bool fightOnHorse = false,
			bool keepHorseAtDestination = false)
	{
		if (!ch)
			return false;
		TPlayerBotAIState& state = s_mapPlayerBotAIStates[ch->GetPlayerID()];
		if (!ch->GetSectree())
		{
			// Still an error - a character standing on no sector is wrong - but
			// the old limiter was per bot, so three hundred bots kept it at
			// thirty lines a second between them. One a minute for the whole
			// population, with the count.
			PlayerBotErrThrottled("nav_missing_sectree", dwNow,
					"PLAYERBOT_NAV: missing sectree pid=%u name=%s map=%ld pos=(%ld,%ld) dest=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
					destX, destY);
			return false;
		}

		const long mapIndex = ch->GetMapIndex();
		// A point on another map's coordinates is not a walk. ClampWorld below
		// pulls any goal onto this map's last cell, the planner calls that
		// corner unreachable, and the caller asks again: bots on Bokjung's town
		// square planned (204750,307150) - that map's far corner - 1615 times in
		// a day on the test world, every one a far plan for nothing. The hub
		// tables, all 222 village ground points, the known-Metin registry and
		// the walk back after a death each check the map, so the source is
		// somewhere else; refused here with the point as it was asked for, the
		// bot's errands and the caller's address, one line a minute per caller,
		// so the log names it. A step a little past the edge still goes through.
		if (LPSECTREE_MAP offMap = SECTREE_MANAGER::instance().GetMap(mapIndex))
		{
			const TMapSetting& setting = offMap->m_setting;
			if (destX < setting.iBaseX - PLAYERBOT_NAV_OFF_MAP_MARGIN ||
					destY < setting.iBaseY - PLAYERBOT_NAV_OFF_MAP_MARGIN ||
					destX >= setting.iBaseX + setting.iWidth + PLAYERBOT_NAV_OFF_MAP_MARGIN ||
					destY >= setting.iBaseY + setting.iHeight + PLAYERBOT_NAV_OFF_MAP_MARGIN)
			{
				const void* caller = __builtin_return_address(0);
				char szOffMapTag[48];
				snprintf(szOffMapTag, sizeof(szOffMapTag), "nav_off_map:%p", caller);
				PlayerBotErrThrottled(szOffMapTag, dwNow,
						"PLAYERBOT_NAV: destination off the map pid=%u name=%s map=%ld pos=(%ld,%ld) dest=(%ld,%ld) "
						"action=%u goal=%u shop=%d phase=%u market=%d bio=%d stable=%d fishing=%d crossing=%ld "
						"departure=%ld hub=%u metin_hunt=%d caller=%p",
						ch->GetPlayerID(), ch->GetName(), mapIndex, ch->GetX(), ch->GetY(), destX, destY,
						(unsigned int)state.bCurrentAction, (unsigned int)state.bLongTermGoal,
						state.bVisitingShop ? 1 : 0, (unsigned int)state.bTownVisitPhase,
						state.bMarketTrip ? 1 : 0, state.bVisitingBiologist ? 1 : 0,
						state.bVisitingStable ? 1 : 0, state.bFishingSession ? 1 : 0,
						state.lDesertCrossingTo, state.lDepartureMap, (unsigned int)state.wHuntingHub,
						IsPlayerBotMetinHunting(state, dwNow) ? 1 : 0, caller);
				if (ch->IsStateMove())
					ch->Stop();
				if (state.bStuckCounter < 255)
					++state.bStuckCounter;
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_UNREACHABLE;
				return false;
			}
		}
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		if (!navigation.Init(mapIndex))
			return false;
		navigation.ClampWorld(destX, destY);

		// Step off first, if the ground underfoot is ground nothing can leave.
		// Every route out of such a cell is refused at its first segment, so
		// this has to happen before the route is even considered - and it is a
		// direct Goto rather than a planned one, because planning is exactly
		// what does not work from here.
		long escapeX = 0, escapeY = 0;
		if (navigation.FindEscapeFromBlockedCell(ch->GetX(), ch->GetY(), escapeX, escapeY))
		{
			PlayerBotLogThrottled("nav_escape_blocked", dwNow,
					"PLAYERBOT_NAV: standing on blocked ground pid=%u name=%s map=%ld pos=(%ld,%ld) step_to=(%ld,%ld) dest=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), mapIndex,
					ch->GetX(), ch->GetY(), escapeX, escapeY, destX, destY);
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = 0;
			state.bStuckCounter = 0;
			ch->Goto(escapeX, escapeY);
			ch->SendMovePacket(FUNC_MOVE, 0, escapeX, escapeY,
					ch->GetCurrentMoveDuration(), dwNow);
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_ESCAPED;
			return true;
		}

		bool redirectedToMonkeyPortal = false;
		// Not through a door before the room has been worked. A destination in
		// another chamber, asked for inside the dwell, is simply unreachable from
		// here. Routing it through a door was the AI's half of the returns to
		// the entrance chamber, and with the engine now refusing to move a bot
		// through a door for the same time, every such route would end with a
		// bot waiting at a door that will not open - 86% of the routes through a
		// door were planned inside the dwell. A chosen exit is unaffected: the
		// wander pass walks to a door that stands in this chamber, which needs
		// no redirect.
		if (IsPlayerBotMonkeyMap(mapIndex) &&
				dwNow - state.dwMonkeyChamberTime >= PLAYERBOT_MONKEY_CHAMBER_DWELL &&
				!navigation.CanReach(ch->GetX(), ch->GetY(), destX, destY))
		{
			long doorX = 0, doorY = 0;
			int doorIndex = -1;
			const int blockedReverseOfDoor =
					dwNow < state.dwMonkeyReversePortalBlockUntil
					? state.iLastMonkeyPortalIndex : -1;
			if (FindPlayerBotMonkeyDoorStep(mapIndex, ch->GetX(), ch->GetY(),
					destX, destY, blockedReverseOfDoor, doorX, doorY, doorIndex))
			{
				destX = doorX;
				destY = doorY;
				state.iLastMonkeyPortalIndex = doorIndex;
				state.dwMonkeyReversePortalBlockUntil =
						dwNow + PLAYERBOT_MONKEY_REVERSE_PORTAL_BLOCK_TIME;
				targetSnapRadius = std::max(targetSnapRadius, 16);
				flexibleTargetSnap = true;
				redirectedToMonkeyPortal = true;
			}
		}

		const int goalDrift = DISTANCE_APPROX(destX - state.lRouteDestX, destY - state.lRouteDestY);
		const bool newGoal = state.lRouteMapIndex != mapIndex ||
				goalDrift > PLAYERBOT_NAV_GOAL_REPLAN_DISTANCE;
		// Keep the travel mode attached to the route itself.  Several lightweight
		// AI passes merely continue the already planned destination and call this
		// function without explicitly requesting a horse.  Treating that default
		// value as a new decision made mounted bots dismount and remount every tick.
		if (newGoal)
		{
			state.bRouteAllowsHorse = allowHorse;
			state.bRouteKeepsHorse = keepHorseAtDestination;
		}
		UpdatePlayerBotTravelMount(ch, state, destX, destY,
				state.bRouteAllowsHorse, dwNow, fightOnHorse,
				keepHorseAtDestination || state.bRouteKeepsHorse);
		if (newGoal)
		{
			ClearPlayerBotRoute(state, false);
			// Goto() safely redirects an active move from the current authoritative
			// position. Stopping first reset the movement state for a single frame
			// and amplified the client's backwards correction.
			state.lRouteMapIndex = mapIndex;
			state.lRouteDestX = destX;
			state.lRouteDestY = destY;
			state.dwNextNavPlanTime = 0;
			state.bStuckCounter = 0;
			if (redirectedToMonkeyPortal)
				sys_log(0, "PLAYERBOT_MONKEY: portal route pid=%u name=%s from=(%ld,%ld) portal=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY);
		}

		// If an old route ended near a moving target, do not keep reporting
		// success while the current requested destination is still far away.
		if (!state.vecRoute.empty() && state.uRouteIndex >= state.vecRoute.size() &&
				DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) > PLAYERBOT_NAV_ARRIVAL_DISTANCE)
			ClearPlayerBotRoute(state, false);

		if (state.vecRoute.empty())
		{
			if (dwNow < state.dwNextNavPlanTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_BACKOFF;
				return true;
			}

			size_t resumedIndex = 0;
			if (ResumePlayerBotParkedRoute(ch, state, navigation, mapIndex, destX, destY))
			{
				++s_uPlayerBotLoadPlanResumed;
				resumedIndex = state.uRouteIndex;
			}
			else if (navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), destX, destY))
			{
				BuildPlayerBotStraightRoute(ch->GetX(), ch->GetY(), destX, destY, state.vecRoute);
			}
			else
			{
				const DWORD routeSeed = ch->GetPlayerID() ^
						((DWORD)state.bStuckCounter * 0x9e3779b9U);
				// A bot that has been turned away this many times stops queueing
				// behind the per-tick count. Nothing else about the request
				// changes, and the tick's microsecond budget still applies.
				const EPlayerBotNavPlanResult planResult = navigation.FindRoute(
						ch->GetX(), ch->GetY(), destX, destY, routeSeed, dwNow,
						targetSnapRadius, flexibleTargetSnap, state.vecRoute,
						state.bNavDeferredCount >= PLAYERBOT_NAV_STARVED_ATTEMPTS);
				state.bRoutePartial = planResult == PLAYERBOT_NAV_PLAN_FOUND &&
						navigation.LastPlanWasPartial();
				if (planResult == PLAYERBOT_NAV_PLAN_DEFERRED)
				{
					if (state.bNavDeferredCount < 255)
						++state.bNavDeferredCount;
					if (state.dwFirstNavDeferTime == 0)
						state.dwFirstNavDeferTime = dwNow;
					if (state.bNavDeferredCount == 20)
						sys_err("PLAYERBOT_NAV: repeatedly deferred pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) reason=%s waited_ms=%u",
								ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
								destX, destY, s_szPlayerBotNavDeferReason,
								state.dwFirstNavDeferTime != 0
									? dwNow - state.dwFirstNavDeferTime : 0);
					// Desynchronise retries so the same low PIDs do not consume every
					// planning slot on each pass through the ordered bot map.
					state.dwNextNavPlanTime = dwNow + 750 +
							(PlayerBotNavHash(ch->GetPlayerID()) % 1251U);
					if (ch->IsStateMove())
						ch->Stop();
					state.bLastNavOutcome = PLAYERBOT_NAV_OUT_DEFERRED;
					return true;
				}
				if (planResult == PLAYERBOT_NAV_PLAN_UNREACHABLE)
				{
					state.bNavDeferredCount = 0;
					state.dwNextNavPlanTime = dwNow + 1500 + (ch->GetPlayerID() % 700);
					if (state.bStuckCounter < 255)
						++state.bStuckCounter;
					if (state.bStuckCounter == 1 || state.bStuckCounter == 3)
						sys_err("PLAYERBOT_NAV: unreachable pid=%u name=%s from=(%ld,%ld) to=(%ld,%ld) failures=%u",
								ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY,
								state.bStuckCounter);
					if (ch->IsStateMove())
						ch->Stop();
					state.bLastNavOutcome = PLAYERBOT_NAV_OUT_UNREACHABLE;
					return false;
				}
			}
			state.bNavDeferredCount = 0;
			// And the clock with it. Nothing ever cleared this, so `waited_ms`
			// in the deferral line was the time since the bot's *first* refusal
			// ever, not the wait of the request being reported: the live server
			// printed waits of four to six hours, which is simply how long the
			// bot had been alive. A number that cannot be wrong is worth more
			// than a number that is usually enormous.
			state.dwFirstNavDeferTime = 0;

			state.uRouteIndex = resumedIndex;
			state.lIssuedWaypointX = 0;
			state.lIssuedWaypointY = 0;
			state.lNavProgressX = ch->GetX();
			state.lNavProgressY = ch->GetY();
			state.iNavLastWaypointDistance = -1;
			state.dwNextNavProgressTime = dwNow + 2000;
			state.bNavNoProgressCount = 0;
		}

		while (state.uRouteIndex < state.vecRoute.size())
		{
			const PIXEL_POSITION& waypoint = state.vecRoute[state.uRouteIndex];
			const int waypointDistance = DISTANCE_APPROX(
					ch->GetX() - waypoint.x, ch->GetY() - waypoint.y);
			if (waypointDistance > PLAYERBOT_NAV_ARRIVAL_DISTANCE)
				break;

			// Do not skip a short alignment waypoint when the following segment
			// is still obstructed from the character's exact interpolated point.
			// Moving those few centimetres to the cell centre is what makes the
			// next corner safe; skipping it caused route=0/0 retry loops.
			//
			// Unless the character is already standing on it. Then there is
			// nothing left to move by: Goto refuses a destination equal to the
			// position, the walk reported that as "moved" because the waypoint
			// was within arrival distance, and the bot stood on its own first
			// waypoint for good - sixteen of eighteen watchdog resets in an
			// afternoon were nav_out=11 at route=0/2 on a cell centre. Consumed,
			// the obstructed segment goes through the blocked-segment branch
			// below, which has the rescues and counts the failure.
			if (state.uRouteIndex + 1 < state.vecRoute.size() &&
					(ch->GetX() != waypoint.x || ch->GetY() != waypoint.y))
			{
				const PIXEL_POSITION& nextWaypoint = state.vecRoute[state.uRouteIndex + 1];
				if (!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(),
						nextWaypoint.x, nextWaypoint.y))
					break;
			}
			++state.uRouteIndex;
			state.lIssuedWaypointX = 0;
			state.lIssuedWaypointY = 0;
			state.iNavLastWaypointDistance = -1;
			state.bNavNoProgressCount = 0;
			state.bStuckCounter = 0;
		}

		if (state.uRouteIndex >= state.vecRoute.size())
		{
			// A partial route ran out where the cap fell, not at the goal: plan
			// the rest from here. Reporting an arrival even once would hand a
			// caller a destination the bot is nowhere near.
			if (state.bRoutePartial &&
					DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) > PLAYERBOT_NAV_ARRIVAL_DISTANCE)
			{
				state.bRoutePartial = false;
				ClearPlayerBotRoute(state, false);
				state.dwNextNavPlanTime = 0;
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NO_PROGRESS;
				return false;
			}
			ch->Stop();
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_ARRIVED;
			return true;
		}

		const PIXEL_POSITION& waypoint = state.vecRoute[state.uRouteIndex];
		const int waypointDistance = DISTANCE_APPROX(
				ch->GetX() - waypoint.x, ch->GetY() - waypoint.y);

		if (dwNow >= state.dwNextNavProgressTime)
		{
			const bool hadProgressBaseline = state.iNavLastWaypointDistance >= 0;
			const bool gotCloser = hadProgressBaseline &&
					waypointDistance + 75 < state.iNavLastWaypointDistance;
			if (hadProgressBaseline &&
					waypointDistance > PLAYERBOT_NAV_ARRIVAL_DISTANCE && !gotCloser)
			{
				if (state.bNavNoProgressCount < 255)
					++state.bNavNoProgressCount;
			}
			else if (hadProgressBaseline)
			{
				state.bNavNoProgressCount = 0;
				state.bStuckCounter = 0;
			}

			state.lNavProgressX = ch->GetX();
			state.lNavProgressY = ch->GetY();
			state.iNavLastWaypointDistance = waypointDistance;
			state.dwNextNavProgressTime = dwNow + 2000;

			if (state.bNavNoProgressCount >= 3)
			{
				sys_err("PLAYERBOT_NAV: no progress, replanning pid=%u name=%s pos=(%ld,%ld) waypoint=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), waypoint.x, waypoint.y);
				if (state.bStuckCounter < 255)
					++state.bStuckCounter;
				ClearPlayerBotRoute(state, false);
				state.dwNextNavPlanTime = dwNow + 200;
				ch->Stop();
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NO_PROGRESS;
				return false;
			}
		}

		// Dynamic objects may have appeared since the path was planned.
		if (!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), waypoint.x, waypoint.y))
		{
			if (state.bStuckCounter < 255)
				++state.bStuckCounter;
			// And say so. This branch threw a route away and replanned two
			// hundred milliseconds later without a word, so a bot whose
			// planner keeps producing a first step the live world refuses
			// looped here for ever in complete silence: no route, no
			// movement, nothing in any log, and a stuck counter of forty-two
			// that only the portal diagnostic ever showed. The unreachable
			// branch beside it speaks at one and three and then goes quiet,
			// which is the same mistake in a milder form.
			// Two rescues before the route is thrown away, because throwing it
			// away and planning the identical one again is exactly what these
			// bots did: forty-two refusals in twenty seconds at four different
			// portals on three maps, no movement and no log line.
			//
			// The planner strings its corners straight and works cell centre to
			// cell centre on the static grid. The walk tests the real segment
			// from wherever the character is standing, with a supercover
			// traversal that counts a cell grazed by a millimetre of corner. The
			// two disagree precisely at a pulled corner, and the disagreement is
			// permanent - the same plan comes back every time it is asked for.
			//
			// First rescue: step to the middle of the cell the character is in,
			// which is the point the route was planned from. The waypoint
			// consumption loop above already relies on this - "moving those few
			// centimetres to the cell centre is what makes the next corner
			// safe" - it simply was not applied to the blocked case.
			// Both ends, because either can be the one that grazes: the
			// character stands wherever movement left it, and a waypoint that
			// came straight from a caller's constant - a portal, an NPC - is a
			// raw world point and not the middle of anything.
			long alignX = 0, alignY = 0;
			long targetX = 0, targetY = 0;
			navigation.CellCentreWorld(ch->GetX(), ch->GetY(), alignX, alignY);
			navigation.CellCentreWorld(waypoint.x, waypoint.y, targetX, targetY);
			const bool movedTarget = targetX != waypoint.x || targetY != waypoint.y;
			const bool movedSelf = alignX != ch->GetX() || alignY != ch->GetY();
			if (movedTarget &&
					navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), targetX, targetY))
			{
				ch->SetRotationToXY(targetX, targetY);
				if (ch->Goto(targetX, targetY))
					ch->SendMovePacket(FUNC_MOVE, 0, targetX, targetY,
							ch->GetCurrentMoveDuration(), dwNow);
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_ALIGNED;
				return true;
			}
			if (movedSelf &&
					(navigation.SegmentClearWorld(alignX, alignY, waypoint.x, waypoint.y) ||
					 (movedTarget && navigation.SegmentClearWorld(alignX, alignY, targetX, targetY))))
			{
				ch->SetRotationToXY(alignX, alignY);
				if (ch->Goto(alignX, alignY))
					ch->SendMovePacket(FUNC_MOVE, 0, alignX, alignY,
							ch->GetCurrentMoveDuration(), dwNow);
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_ALIGNED;
				return true;
			}
			// Second rescue: take the corner as a corner. If the waypoint after
			// this one is reachable in a straight line, the grazed corner was
			// the only thing in the way and the route itself is sound.
			if (state.uRouteIndex + 1 < state.vecRoute.size())
			{
				const PIXEL_POSITION& afterWaypoint = state.vecRoute[state.uRouteIndex + 1];
				if (navigation.SegmentClearWorld(ch->GetX(), ch->GetY(),
						afterWaypoint.x, afterWaypoint.y))
				{
					++state.uRouteIndex;
					state.lIssuedWaypointX = 0;
					state.lIssuedWaypointY = 0;
					state.iNavLastWaypointDistance = -1;
					state.bLastNavOutcome = PLAYERBOT_NAV_OUT_CORNERED;
					return true;
				}
			}
			// Third rescue: both ends are cell centres already, so neither
			// alignment can change the answer, and the corner after this one is
			// not in reach either. The static grid planned this segment; the
			// live supercover test disagrees at a grazed corner, and it will
			// disagree identically on every replan. Walk it: the server moves a
			// bot along a straight line with no collision, so the worst case is
			// a shoulder through a decorative corner - the alternative, measured
			// at the Monkey Dungeon exit, was a bot stopping short of the portal
			// and turning back for good.
			if (!movedSelf && !movedTarget)
			{
				ch->SetRotationToXY(waypoint.x, waypoint.y);
				if (ch->Goto(waypoint.x, waypoint.y))
				{
					ch->SendMovePacket(FUNC_MOVE, 0, waypoint.x, waypoint.y,
							ch->GetCurrentMoveDuration(), dwNow);
					state.lIssuedWaypointX = waypoint.x;
					state.lIssuedWaypointY = waypoint.y;
					state.bLastNavOutcome = PLAYERBOT_NAV_OUT_FORCED;
					PlayerBotLogThrottled("nav_forced_corner", dwNow,
							"PLAYERBOT_NAV: forced through a grazed corner pid=%u name=%s map=%ld pos=(%ld,%ld) waypoint=(%ld,%ld) dest=(%ld,%ld)",
							ch->GetPlayerID(), ch->GetName(), mapIndex,
							ch->GetX(), ch->GetY(), waypoint.x, waypoint.y, destX, destY);
					return true;
				}
			}
			if (state.bStuckCounter == 4 || (state.bStuckCounter % 32) == 0)
				PlayerBotLogThrottled("nav_segment_blocked", dwNow,
						"PLAYERBOT_NAV: waypoint blocked by the live world pid=%u name=%s map=%ld pos=(%ld,%ld) waypoint=(%ld,%ld) dest=(%ld,%ld) failures=%u",
						ch->GetPlayerID(), ch->GetName(), mapIndex,
						ch->GetX(), ch->GetY(), waypoint.x, waypoint.y,
						destX, destY, (unsigned int)state.bStuckCounter);
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = dwNow + 200;
			ch->Stop();
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_SEGMENT;
			return false;
		}

		ch->SetRotationToXY(waypoint.x, waypoint.y);
		if (state.lIssuedWaypointX == waypoint.x && state.lIssuedWaypointY == waypoint.y && ch->IsStateMove())
		{
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_MOVED;
			return true;
		}

		const bool wasMoving = ch->IsStateMove();
		const bool commandAccepted = ch->Goto(waypoint.x, waypoint.y);
		state.lIssuedWaypointX = waypoint.x;
		state.lIssuedWaypointY = waypoint.y;
		if (commandAccepted || (!wasMoving && ch->IsStateMove()))
		{
			ch->SendMovePacket(FUNC_MOVE, 0, waypoint.x, waypoint.y, ch->GetCurrentMoveDuration(), dwNow);
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_MOVED;
			return true;
		}

		// Goto(false) also means that this exact destination is already active.
		state.bLastNavOutcome = PLAYERBOT_NAV_OUT_REFUSED;
		return ch->IsStateMove() || waypointDistance <= PLAYERBOT_NAV_ARRIVAL_DISTANCE;
	}

	void GetPlayerBotStableOffset(DWORD playerID, DWORD salt, int minRadius, int maxRadius,
			long& offsetX, long& offsetY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ salt);
		const int radiusRange = std::max(0, maxRadius - minRadius);
		const int radius = minRadius + (radiusRange > 0 ? (int)(hash % (DWORD)(radiusRange + 1)) : 0);
		const float angle = (float)(PlayerBotNavHash(hash ^ 0xa511e9b3U) % 360U);
		float dx = 0.0f;
		float dy = 0.0f;
		GetDeltaByDegree(angle, (float)radius, &dx, &dy);
		offsetX = (long)dx;
		offsetY = (long)dy;
	}
}

#endif
