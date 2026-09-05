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
	DWORD s_adwPlayerBotMetinHotspotVisits[12] = { 0 };
	DWORD s_adwPlayerBotMetinHotspotFinds[12] = { 0 };
	DWORD s_adwPlayerBotMetinHotspotLastFind[12] = { 0 };

	struct TPlayerBotMonkeyPortal
	{
		int fromLocalX;
		int fromLocalY;
		int toLocalX;
		int toLocalY;
	};

	// The easy Monkey Dungeon is not one continuous walkable maze. Its rooms are
	// joined by native GOTO NPCs (10501..10524), which teleport a nearby character
	// locally without a loading screen. Mirror that directed graph in the planner;
	// the actual teleport is still performed by the normal server NPC event.
	const TPlayerBotMonkeyPortal PLAYERBOT_MONKEY_PORTALS[] = {
		{145, 315, 345, 361}, {80, 308, 106, 547}, {206, 109, 75, 368},
		{320, 238, 89, 746},  {421, 272, 520, 352}, {487, 279, 541, 45},
		{70, 368, 211, 109},  {65, 434, 615, 49},   {67, 498, 284, 705},
		{350, 361, 145, 310}, {210, 495, 553, 285}, {526, 352, 487, 274},
		{528, 415, 626, 291}, {523, 480, 285, 569}, {101, 547, 80, 303},
		{82, 746, 315, 238},  {541, 40, 416, 272},  {615, 44, 72, 498},
		{553, 291, 215, 495}, {626, 296, 523, 415}, {278, 705, 72, 498},
		{280, 569, 518, 480}, {470, 560, 583, 379}, {579, 390, 459, 558}
	};

	bool IsPlayerBotMonkeyReversePortal(int candidateIndex, int previousIndex)
	{
		const size_t portalCount = sizeof(PLAYERBOT_MONKEY_PORTALS) /
				sizeof(PLAYERBOT_MONKEY_PORTALS[0]);
		if (candidateIndex < 0 || previousIndex < 0 ||
				(size_t)candidateIndex >= portalCount || (size_t)previousIndex >= portalCount)
			return false;

		const TPlayerBotMonkeyPortal& candidate = PLAYERBOT_MONKEY_PORTALS[candidateIndex];
		const TPlayerBotMonkeyPortal& previous = PLAYERBOT_MONKEY_PORTALS[previousIndex];
		// Reciprocal GOTO entries deliberately land about five map cells beside
		// one another. Treat that pair as the same doorway for ten seconds: if a
		// target dies while the bot is crossing, the next target scan must not
		// send it straight back through the arrival portal.
		return abs(candidate.fromLocalX - previous.toLocalX) <= 8 &&
				abs(candidate.fromLocalY - previous.toLocalY) <= 8 &&
				abs(candidate.toLocalX - previous.fromLocalX) <= 8 &&
				abs(candidate.toLocalY - previous.fromLocalY) <= 8;
	}

	bool FindPlayerBotMonkeyPortalStep(CPlayerBotNavigation& navigation,
			long startX, long startY, long targetX, long targetY,
			int blockedReverseOfPortal, long& portalX, long& portalY,
			int& selectedPortalIndex)
	{
		const DWORD startComponent = navigation.GetComponentAtWorld(startX, startY, 6);
		const DWORD targetComponent = navigation.GetComponentAtWorld(targetX, targetY, 6);
		if (startComponent == 0 || targetComponent == 0 || startComponent == targetComponent)
			return false;

		std::queue<DWORD> open;
		std::map<DWORD, DWORD> parentComponent;
		std::map<DWORD, int> parentPortal;
		parentComponent[startComponent] = startComponent;
		open.push(startComponent);

		while (!open.empty() && parentComponent.find(targetComponent) == parentComponent.end())
		{
			const DWORD current = open.front();
			open.pop();
			for (size_t i = 0; i < sizeof(PLAYERBOT_MONKEY_PORTALS) /
					sizeof(PLAYERBOT_MONKEY_PORTALS[0]); ++i)
			{
				const TPlayerBotMonkeyPortal& portal = PLAYERBOT_MONKEY_PORTALS[i];
				if (IsPlayerBotMonkeyReversePortal((int)i, blockedReverseOfPortal))
					continue;
				const long fromX = PLAYERBOT_MONKEY_EASY_BASE_X + portal.fromLocalX * 100L;
				const long fromY = PLAYERBOT_MONKEY_EASY_BASE_Y + portal.fromLocalY * 100L;
				const DWORD fromComponent = navigation.GetComponentAtWorld(fromX, fromY, 12);
				if (fromComponent != current)
					continue;

				const long toX = PLAYERBOT_MONKEY_EASY_BASE_X + portal.toLocalX * 100L;
				const long toY = PLAYERBOT_MONKEY_EASY_BASE_Y + portal.toLocalY * 100L;
				const DWORD toComponent = navigation.GetComponentAtWorld(toX, toY, 12);
				if (toComponent == 0 || parentComponent.find(toComponent) != parentComponent.end())
					continue;

				parentComponent[toComponent] = current;
				parentPortal[toComponent] = (int)i;
				open.push(toComponent);
			}
		}

		if (parentComponent.find(targetComponent) == parentComponent.end())
			return false;

		DWORD cursor = targetComponent;
		int firstPortal = -1;
		while (cursor != startComponent)
		{
			std::map<DWORD, int>::const_iterator portalIt = parentPortal.find(cursor);
			std::map<DWORD, DWORD>::const_iterator parentIt = parentComponent.find(cursor);
			if (portalIt == parentPortal.end() || parentIt == parentComponent.end())
				return false;
			firstPortal = portalIt->second;
			cursor = parentIt->second;
		}
		if (firstPortal < 0)
			return false;

		portalX = PLAYERBOT_MONKEY_EASY_BASE_X +
				PLAYERBOT_MONKEY_PORTALS[firstPortal].fromLocalX * 100L;
		portalY = PLAYERBOT_MONKEY_EASY_BASE_Y +
				PLAYERBOT_MONKEY_PORTALS[firstPortal].fromLocalY * 100L;
		selectedPortalIndex = firstPortal;
		return true;
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

	void RememberPlayerBotMetin(LPCHARACTER stone, DWORD dwNow)
	{
		if (!stone || !stone->IsStone() || stone->IsDead())
			return;
		const bool bNewDiscovery = s_mapKnownPlayerBotMetins.find(stone->GetVID()) ==
				s_mapKnownPlayerBotMetins.end();
		TKnownPlayerBotMetin& known = s_mapKnownPlayerBotMetins[stone->GetVID()];
		known.lMapIndex = stone->GetMapIndex();
		known.lX = stone->GetX();
		known.lY = stone->GetY();
		known.bLevel = stone->GetLevel();
		known.dwLastSeenTime = dwNow;

		if (bNewDiscovery && stone->GetMapIndex() == 21)
		{
			int nearest = 0;
			int nearestDistance = INT_MAX;
			for (int i = 0; i < 12; ++i)
			{
				const int distance = DISTANCE_APPROX(stone->GetX() - PLAYERBOT_METIN_HOTSPOTS[i].x,
						stone->GetY() - PLAYERBOT_METIN_HOTSPOTS[i].y);
				if (distance < nearestDistance)
				{
					nearest = i;
					nearestDistance = distance;
				}
			}
			++s_adwPlayerBotMetinHotspotFinds[nearest];
			s_adwPlayerBotMetinHotspotLastFind[nearest] = dwNow;
		}
	}

	bool IsPlayerBotMetinWorthFighting(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
			return false;
		// The server drop multiplier still has useful value at a ten-level
		// advantage. Below that it collapses sharply (15% at -11 and 1% at -15),
		// so a level-25 bot should pass level-5/10 stones and keep level-15+.
		return stone->GetLevel() <= ch->GetLevel() + 9 &&
				ch->GetLevel() <= stone->GetLevel() + 10;
	}

	BYTE ChoosePlayerBotMetinHotspot(DWORD playerID, BYTE currentIndex, DWORD dwNow)
	{
		BYTE best = currentIndex % 12;
		int bestScore = INT_MIN;
		// Compare four PID-specific candidates. This learns productive areas while
		// keeping different hunters on different routes instead of one global line.
		for (int option = 0; option < 4; ++option)
		{
			const BYTE index = (BYTE)((currentIndex + option * 3 +
					(PlayerBotNavHash(playerID + option * 101U) % 5U)) % 12);
			const int successRate = (int)((s_adwPlayerBotMetinHotspotFinds[index] + 1) * 1000 /
					(s_adwPlayerBotMetinHotspotVisits[index] + 3));
			const int freshness = s_adwPlayerBotMetinHotspotLastFind[index] != 0 &&
					dwNow - s_adwPlayerBotMetinHotspotLastFind[index] < 300000 ? 250 : 0;
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

			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = 0;
			state.dwNextHorseRideCheckTime = dwNow + 1000;
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

		ch->Stop();
		if (!ch->StartRiding())
		{
			state.dwNextHorseRideCheckTime = dwNow + PLAYERBOT_HORSE_RIDE_RETRY_INTERVAL;
			sys_err("PLAYERBOT_HORSE: mount failed pid=%u name=%s horse_level=%u health=%d stamina=%d reason=%s",
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

	// A battle horse (level 11+) lets its rider strike from the saddle. Bots that
	// own one should ride into a fight instead of dismounting on the approach, but
	// only when the weapon and target actually make mounted combat sensible.
	// Could this bot fight from the saddle at all, whatever it ends up facing?
	// The horse and the weapon decide that much on their own, and the tick has
	// to know it before a target exists - that is the moment it decides whether
	// to climb down.
	bool CanPlayerBotEverFightOnHorse(LPCHARACTER ch)
	{
		if (!ch || ch->GetHorseLevel() < PLAYERBOT_BATTLE_HORSE_LEVEL)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		return weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() != WEAPON_BOW;
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
			bool fightOnHorse = false)
	{
		if (!ch)
			return;

		// Mounted combat overrides the travel dismount: the bot is closing on a
		// target it may legitimately hit from the saddle, so keep (or take) the
		// horse regardless of how near the destination is. SetPlayerBotRidingForTravel
		// still refuses gracefully when the horse is spent, leaving the bot on foot.
		if (fightOnHorse)
		{
			SetPlayerBotRidingForTravel(ch, state, true, dwNow, "mounted_combat");
			return;
		}

		const int distance = DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY);
		if (!allowHorse || distance <= PLAYERBOT_HORSE_DISMOUNT_DISTANCE)
			SetPlayerBotRidingForTravel(ch, state, false, dwNow,
					allowHorse ? "near_destination" : "on_foot_action");
		else if (distance >= PLAYERBOT_HORSE_MOUNT_DISTANCE)
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

	bool MovePlayerBot(LPCHARACTER ch, long destX, long destY, DWORD dwNow,
			int targetSnapRadius = 4, bool flexibleTargetSnap = false,
			bool allowHorse = false, bool fightOnHorse = false)
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
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		if (!navigation.Init(mapIndex))
			return false;
		navigation.ClampWorld(destX, destY);

		bool redirectedToMonkeyPortal = false;
		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY &&
				!navigation.CanReach(ch->GetX(), ch->GetY(), destX, destY))
		{
			long portalX = 0, portalY = 0;
			int portalIndex = -1;
			const int blockedReverseOfPortal =
					dwNow < state.dwMonkeyReversePortalBlockUntil
					? state.iLastMonkeyPortalIndex : -1;
			if (FindPlayerBotMonkeyPortalStep(navigation, ch->GetX(), ch->GetY(),
					destX, destY, blockedReverseOfPortal, portalX, portalY, portalIndex))
			{
				destX = portalX;
				destY = portalY;
				state.iLastMonkeyPortalIndex = portalIndex;
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
			state.bRouteAllowsHorse = allowHorse;
		UpdatePlayerBotTravelMount(ch, state, destX, destY,
				state.bRouteAllowsHorse, dwNow, fightOnHorse);
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
				return true;
			}

			if (navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), destX, destY))
			{
				BuildPlayerBotStraightRoute(ch->GetX(), ch->GetY(), destX, destY, state.vecRoute);
			}
			else
			{
				const DWORD routeSeed = ch->GetPlayerID() ^
						((DWORD)state.bStuckCounter * 0x9e3779b9U);
				const EPlayerBotNavPlanResult planResult = navigation.FindRoute(
						ch->GetX(), ch->GetY(), destX, destY, routeSeed, dwNow,
						targetSnapRadius, flexibleTargetSnap, state.vecRoute);
				if (planResult == PLAYERBOT_NAV_PLAN_DEFERRED)
				{
					if (state.bNavDeferredCount < 255)
						++state.bNavDeferredCount;
					if (state.bNavDeferredCount == 20)
						sys_err("PLAYERBOT_NAV: repeatedly deferred pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld)",
								ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY);
					// Desynchronise retries so the same low PIDs do not consume every
					// planning slot on each pass through the ordered bot map.
					state.dwNextNavPlanTime = dwNow + 750 +
							(PlayerBotNavHash(ch->GetPlayerID()) % 1251U);
					if (ch->IsStateMove())
						ch->Stop();
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
					return false;
				}
			}
			state.bNavDeferredCount = 0;

			state.uRouteIndex = 0;
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
			if (state.uRouteIndex + 1 < state.vecRoute.size())
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
			ch->Stop();
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
				return false;
			}
		}

		// Dynamic objects may have appeared since the path was planned.
		if (!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), waypoint.x, waypoint.y))
		{
			if (state.bStuckCounter < 255)
				++state.bStuckCounter;
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = dwNow + 200;
			ch->Stop();
			return false;
		}

		ch->SetRotationToXY(waypoint.x, waypoint.y);
		if (state.lIssuedWaypointX == waypoint.x && state.lIssuedWaypointY == waypoint.y && ch->IsStateMove())
			return true;

		const bool wasMoving = ch->IsStateMove();
		const bool commandAccepted = ch->Goto(waypoint.x, waypoint.y);
		state.lIssuedWaypointX = waypoint.x;
		state.lIssuedWaypointY = waypoint.y;
		if (commandAccepted || (!wasMoving && ch->IsStateMove()))
		{
			ch->SendMovePacket(FUNC_MOVE, 0, waypoint.x, waypoint.y, ch->GetCurrentMoveDuration(), dwNow);
			return true;
		}

		// Goto(false) also means that this exact destination is already active.
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
