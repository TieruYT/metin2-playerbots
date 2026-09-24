#ifndef __INC_METIN2_PLAYERBOT_WANDERING_H__
#define __INC_METIN2_PLAYERBOT_WANDERING_H__

// What a bot does on a hunting map when nothing is asking for its attention.
//
// This is the difference between a populated world and a car park full of
// idling characters, so it is deliberately not "walk to a random point": bots
// work a rotation of hotspots, spread out rather than stack, and keep moving
// through ground they have already cleared.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	// The hub for this bot, from a banded table, by what the population has
	// seen there. Level rules the band; a party hub needs a party of the
	// challenge size with this bot leading it; among what is left the richest
	// ground wins, its worth shared out among the bots already on it - except
	// that a leader's own party and guild are not a crowd, so a guild converges
	// on one camp instead of fleeing each other. Unknown ground is scored as an
	// average spot, which is optimistic on purpose: it has to be looked at to
	// be known. A small hash keeps equal scores from all resolving the same way.
	struct FPlayerBotFindBoss
	{
		WORD m_wRace;
		LPCHARACTER m_found;
		FPlayerBotFindBoss(WORD wRace) : m_wRace(wRace), m_found(NULL) {}
		void operator()(LPENTITY entity)
		{
			if (m_found || !entity || !entity->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER mob = static_cast<LPCHARACTER>(entity);
			if (mob->IsMonster() && mob->GetRaceNum() == m_wRace && !mob->IsDead())
				m_found = mob;
		}
	};

	// Is the boss standing on this map right now, and where? The Orc Chief's
	// group (621) is placed anywhere within a hundred and fifty cells of its
	// point - fifteen thousand units - so one sector's neighbourhood missed
	// him: "down" was logged while he was casting a mile away. Nine sectors
	// a sector apart were the next answer, and the Spider Queen walked out of
	// those too: logged standing five kilometres from her hub, "down" three
	// minutes later with no BOSS_KILL in the log, standing again eleven
	// kilometres from it - she chases what attacks her, and every raid was
	// sent home while she was still on her feet. The whole map is asked now;
	// the map's entities are one snapshot copy, and the answer is kept for
	// PLAYERBOT_RAID_BOSS_CHECK_INTERVAL, so a hundred bots choosing hubs in
	// the same half minute cost one pass over the Spider Dungeon's monsters.
	bool IsPlayerBotBossAlive(long mapIndex, long x, long y, WORD wRace, DWORD dwNow,
			long* pBossX, long* pBossY, char* pName = NULL, size_t nameSize = 0)
	{
		struct TBossAnswer { DWORD dwStamp; bool bAlive; long lX; long lY; char szName[32]; };
		// By map as well as race. The Bestial Captain (591) stands in all three
		// second villages, and an answer kept by race alone gave a bot in
		// Bokjung the Captain of Jayang for the thirty seconds it was trusted:
		// a walk to another map's coordinates, which the planner clamped onto
		// Bokjung's far corner (204750,307150) and called unreachable - 1615
		// far plans a day on the test world, and close to four thousand
		// refusals a minute once MovePlayerBot refused such a point. The raid
		// roster and the guild call below are still kept by race, which holds
		// while every boss hub is the boss of one map.
		typedef std::pair<long, WORD> TBossKey;
		static std::map<TBossKey, TBossAnswer> s_mapAnswers;
		const TBossKey key(mapIndex, wRace);
		std::map<TBossKey, TBossAnswer>::iterator it = s_mapAnswers.find(key);
		if (it != s_mapAnswers.end() && dwNow - it->second.dwStamp < PLAYERBOT_RAID_BOSS_CHECK_INTERVAL)
		{
			if (pBossX) *pBossX = it->second.lX;
			if (pBossY) *pBossY = it->second.lY;
			if (pName && nameSize > 0)
				strlcpy(pName, it->second.szName, nameSize);
			return it->second.bAlive;
		}
		LPCHARACTER boss = NULL;
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
		if (pMap)
		{
			FPlayerBotFindBoss finder(wRace);
			pMap->for_each(finder);
			boss = finder.m_found;
		}
		TBossAnswer& answer = s_mapAnswers[key];
		const bool bAlive = boss != NULL;
		if (it == s_mapAnswers.end() || answer.bAlive != bAlive)
			sys_log(0, "PLAYERBOT_RAID: boss race=%u map=%ld %s pos=(%ld,%ld)", (unsigned int)wRace, mapIndex,
					bAlive ? "standing" : "down", boss ? boss->GetX() : 0L, boss ? boss->GetY() : 0L);
		answer.dwStamp = dwNow;
		answer.bAlive = bAlive;
		answer.lX = boss ? boss->GetX() : x;
		answer.lY = boss ? boss->GetY() : y;
		strlcpy(answer.szName, boss ? boss->GetName() : "Boss", sizeof(answer.szName));
		if (pBossX) *pBossX = answer.lX;
		if (pBossY) *pBossY = answer.lY;
		if (pName && nameSize > 0)
			strlcpy(pName, answer.szName, nameSize);
		return bAlive;
	}

	// A boss is news, and news travels through the guild of whoever saw him.
	//
	// Before this every bot of the band walked to a boss hub the moment the
	// sector said he was standing - a raid worth a hundred thousand outscored
	// every hunting ground by two orders of magnitude - and the ones that
	// arrived after he was down stood about at the table point waiting for a
	// monster that was not there. Now the first bot to find him standing calls
	// its own guild, and it is that guild's business until there are enough
	// bodies on him; everybody else goes on hunting.
	struct TPlayerBotRaidCall
	{
		DWORD dwGuild;
		DWORD dwStamp;
		TPlayerBotRaidCall() : dwGuild(0), dwStamp(0) {}
	};
	std::map<WORD, TPlayerBotRaidCall> s_mapPlayerBotRaidCalls;

	void NotePlayerBotRaidSighting(LPCHARACTER ch, WORD wRace, const char* bossName,
			DWORD dwNow)
	{
		if (!ch || !ch->GetGuild())
			return;
		TPlayerBotRaidCall& call = s_mapPlayerBotRaidCalls[wRace];
		if (call.dwStamp != 0 && dwNow - call.dwStamp < PLAYERBOT_RAID_CALL_TIME)
			return;
		call.dwGuild = ch->GetGuild()->GetID();
		call.dwStamp = dwNow;
		char msg[128];
		snprintf(msg, sizeof(msg), "%s stoi! Zbieramy sie na niego.",
				bossName && *bossName ? bossName : "Boss");
		ch->GetGuild()->Chat(msg);
		sys_log(0, "PLAYERBOT_RAID: called pid=%u name=%s guild=%u race=%u boss=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)call.dwGuild,
				(unsigned int)wRace, bossName ? bossName : "?");
	}

	// Who has already set off. Counting the bots standing on the hub instead
	// answers "nobody yet" to every one of a hundred bots deciding in the same
	// second, because none of them has arrived - which is how a throttle of
	// twelve let a hundred and forty-five head for one monster.
	std::map<WORD, std::map<DWORD, DWORD> > s_mapPlayerBotRaidRoster;

	int CountPlayerBotRaiders(WORD wRace, DWORD dwNow)
	{
		std::map<DWORD, DWORD>& roster = s_mapPlayerBotRaidRoster[wRace];
		std::map<DWORD, DWORD>::iterator it = roster.begin();
		while (it != roster.end())
		{
			if (dwNow - it->second > PLAYERBOT_RAID_CALL_TIME)
				roster.erase(it++);
			else
				++it;
		}
		return (int)roster.size();
	}

	void NotePlayerBotRaider(WORD wRace, DWORD pid, DWORD dwNow)
	{
		s_mapPlayerBotRaidRoster[wRace][pid] = dwNow;
	}

	bool IsPlayerBotRaidCalled(LPCHARACTER ch, WORD wRace, DWORD dwNow)
	{
		if (!ch || !ch->GetGuild())
			return false;
		std::map<WORD, TPlayerBotRaidCall>::const_iterator it =
				s_mapPlayerBotRaidCalls.find(wRace);
		return it != s_mapPlayerBotRaidCalls.end() &&
				it->second.dwStamp != 0 &&
				dwNow - it->second.dwStamp < PLAYERBOT_RAID_CALL_TIME &&
				it->second.dwGuild == ch->GetGuild()->GetID();
	}

	bool ChoosePlayerBotHuntingHub(LPCHARACTER ch, const TPlayerBotHuntingHub* hubs,
			size_t hubCount, DWORD dwNow, size_t excludeIndex, size_t& indexOut, int& scoreOut)
	{
		indexOut = 0;
		scoreOut = 0;
		if (!ch || !hubs || hubCount == 0)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
		if (!navigation.Init(ch->GetMapIndex()))
			return false;

		const BYTE level = ch->GetLevel();
		LPPARTY party = ch->GetParty();
		const bool bLeadsParty = party && party->GetLeaderCharacter() == ch &&
				(int)party->GetMemberCount() >= PLAYERBOT_PARTY_CHALLENGE_MIN_MEMBERS;
		CGuild* guild = ch->GetGuild();
		const DWORD dwSeed = guild ? (0x47494c44U ^ guild->GetID()) : ch->GetPlayerID();

		std::vector<TPlayerBotCrowdEntry> crowd;
		CollectPlayerBotCrowd(ch, ch->GetMapIndex(), crowd);
		// What this bot is short of at the anvil. A hub where one of those has
		// been picked up is worth more to it; one with a long record of fights
		// and none of it is not.
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);

		// Ground a capitulation gave up is not chosen for as long as it is
		// given up (the Anti-PK protocol, playerbot_anti_pk.h).
		TPlayerBotAIStateMap::const_iterator ownState = s_mapPlayerBotAIStates.find(ch->GetPlayerID());

		int bestScore = INT_MIN;
		size_t best = 0;
		bool bFound = false;
		for (size_t i = 0; i < hubCount; ++i)
		{
			const TPlayerBotHuntingHub& hub = hubs[i];
			if (i == excludeIndex || level < hub.bMinLevel || level > hub.bMaxLevel)
				continue;
			if (hub.bNeedsParty && !bLeadsParty)
				continue;
			if (ownState != s_mapPlayerBotAIStates.end() &&
					IsPlayerBotAvoidedSpot(ownState->second, ch->GetMapIndex(), hub.x, hub.y, dwNow))
				continue;
			// A boss hub is worth going to while the boss stands, and nothing when
			// he is down; the crowd already on him is not a reason to stay away.
			if (hub.wBossRace != 0)
			{
				long bossX = hub.x, bossY = hub.y;
				char bossName[32] = "";
				if (!IsPlayerBotBossAlive(ch->GetMapIndex(), hub.x, hub.y, hub.wBossRace,
						dwNow, &bossX, &bossY, bossName, sizeof(bossName)))
					continue;
				NotePlayerBotRaidSighting(ch, hub.wBossRace, bossName, dwNow);
				// Where the boss actually stands is walkable by definition; the
				// question is whether it is this bot's terrain.
				const DWORD bossGround = navigation.GetComponentAtWorld(bossX, bossY, 12);
				const DWORD ownGround = navigation.GetComponentAtWorld(ch->GetX(), ch->GetY());
				if (bossGround == 0 || bossGround != ownGround)
				{
					PlayerBotLogThrottled("raid_unreachable", dwNow,
							"PLAYERBOT_RAID: boss hub unreachable race=%u pid=%u name=%s pos=(%ld,%ld) boss_ground=%u own_ground=%u",
							(unsigned int)hub.wBossRace, ch->GetPlayerID(), ch->GetName(),
							ch->GetX(), ch->GetY(), (unsigned int)bossGround, (unsigned int)ownGround);
					continue;
				}
			}
			// A hub is only worth walking to if the navigation can get there.
			// Orc Valley taught this: its entrance opens onto one island of a
			// river delta, and while the navigation refused water - which is to
			// say, refused the bridges - all twelve of its hand-picked hubs sat
			// on the far side of a crossing. A bot planned an impossible route,
			// gave up after three tries, advanced to the next hub and planned
			// another impossible route, twelve times, then round again: twelve
			// bots on that one map produced 7812 of the 8259 "unreachable" lines
			// in a session and never reached a hunting ground. Asking costs a
			// component lookup; the alternative costs an A* guaranteed to fail.
			//
			// It is not the answer everywhere. A map may be built in chambers
			// the terrain does not join at all - the Monkey Dungeon is ten of
			// them - and there this question has only ever one answer, "the room
			// you are already in". That map is walked by its own portal graph
			// instead; see the chamber table in playerbot_movement.h.
			else if (!navigation.CanReach(ch->GetX(), ch->GetY(), hub.x, hub.y))
				continue;
			DWORD samples = 0;
			int averageLevel = 0;
			const int density = GetPlayerBotSpotDensityPermille(ch->GetMapIndex(), hub.x, hub.y, dwNow,
					&samples, &averageLevel);
			int worth = samples >= PLAYERBOT_SPOT_MIN_SAMPLES ? density : PLAYERBOT_SPOT_UNKNOWN_PERMILLE;
			// Full of monsters the bot cannot touch is empty for the bot. A camp
			// of knights ten levels up is remembered as rich by everyone who
			// looked at it and is no place for a bot on its own; a leader with a
			// party judges by the party's reach, which the challenge rules apply.
			if (samples >= PLAYERBOT_SPOT_MIN_SAMPLES && !bLeadsParty &&
					averageLevel > (int)level + PLAYERBOT_MAX_TARGET_LEVEL_DELTA)
				worth = 0;
			const int others = CountPlayerBotsNear(ch, crowd, hub.x, hub.y,
					PLAYERBOT_SPOT_CROWD_RADIUS, hub.bNeedsParty);
			bool dropsWanted = false;
			for (std::set<DWORD>::const_iterator w = wanted.begin(); w != wanted.end() && !dropsWanted; ++w)
			{
				DWORD fights = 0;
				const DWORD drops = GetPlayerBotSpotDropCount(ch->GetMapIndex(), hub.x, hub.y, *w, &fights);
				if (drops > 0 && !(fights >= PLAYERBOT_SPOT_MATERIAL_BARREN_FIGHTS && drops == 0))
					dropsWanted = true;
			}
			if (dropsWanted)
				worth += worth * PLAYERBOT_SPOT_MATERIAL_BONUS_PERCENT / 100;
			// The bot's share of what is there: the monsters in reach divided among
			// the bots already in reach of them, plus this one.
			int score;
			if (hub.wBossRace != 0)
			{
				// A raid is twelve, not a province. The guild that was called
				// may fill it; anybody else takes only the first half, so a
				// boss nobody called still gets killed and the rest of the band
				// carries on hunting instead of queueing on a snowfield.
				const int raiders = CountPlayerBotRaiders(hub.wBossRace, dwNow);
				const bool called = IsPlayerBotRaidCalled(ch, hub.wBossRace, dwNow);
				// Twelve, or a share of everyone on the map - see
				// PLAYERBOT_RAID_MAP_SHARE_CALLED_PERCENT for the forty bots that
				// hunted soldiers within sight of the Spider Queen.
				const int onMap = GetPlayerBotsOnMap(ch->GetMapIndex());
				const int room = std::max(
						called ? PLAYERBOT_RAID_CROWD : PLAYERBOT_RAID_CROWD / 2,
						onMap * (called ? PLAYERBOT_RAID_MAP_SHARE_CALLED_PERCENT
								: PLAYERBOT_RAID_MAP_SHARE_UNCALLED_PERCENT) / 100);
				if (raiders >= room)
					continue;
				score = PLAYERBOT_RAID_WORTH;
			}
			else
				score = worth / (1 + others);
			// Nearer is better, all else equal: a camp across the delta costs a
			// route of two hundred milliseconds to plan and three minutes to walk.
			const int distance = DISTANCE_APPROX(ch->GetX() - hub.x, ch->GetY() - hub.y);
			score = (int)((long long)score * PLAYERBOT_HUB_HALF_WORTH_DISTANCE /
					(PLAYERBOT_HUB_HALF_WORTH_DISTANCE + distance));
			score += (int)(PlayerBotNavHash(dwSeed ^ (DWORD)(i * 0x9e3779b9U)) % 150U);
			if (score > bestScore)
			{
				bestScore = score;
				best = i;
				bFound = true;
			}
		}
		indexOut = best;
		scoreOut = bestScore;
		return bFound;
	}

	// Whether a village hub whose monsters sit at mobLevel is ground for a
	// bot of this level: the target scorer's sweet spot is a monster within
	// -2..+5 of the bot, so a hub is taken from two levels under its median
	// to three over it. It used to be seven over, and what the seven bought
	// was dogs: a bot of nine qualified for the band-three hubs beside the
	// band-nine ones and was sent to them by pid, where a Wild Dog pays 15
	// experience against a Blue Alpha Wolf's 111 - a third of every fight
	// measured on the test world was six or more levels under the bot, and a
	// level in the teens took two hours. (PERCENT_LVDELTA is not what limits
	// it: this engine's table still pays 90% at six under. The base is.)
	bool IsPlayerBotM1HubForLevel(int botLevel, int mobLevel)
	{
		return botLevel >= mobLevel - 2 && botLevel <= mobLevel + 3;
	}

	// The hubs of a table whose band holds the bot's level, or - when none
	// does - the hubs of the band nearest to it. Falling back on the whole
	// table put a bot of sixteen on Yongan, between the thirteens and the
	// eighteens, anywhere at all, including the twenty-fives it cannot fight
	// and the threes not worth fighting. Returns how many were written.
	int CollectPlayerBotM1HubsForLevel(int botLevel, const TPlayerBotVillageHub* hubs,
			int hubTotal, int* out, int cap)
	{
		int count = 0;
		for (int h = 0; h < hubTotal && count < cap; ++h)
			if (IsPlayerBotM1HubForLevel(botLevel, hubs[h].mobLevel))
				out[count++] = h;
		if (count > 0)
			return count;
		// Above every band - a bot the village has outgrown, here for an
		// errand - the top bands together, whole bands until there are at
		// least PLAYERBOT_M1_OUTGROWN_HUB_CHOICES_MIN hubs. The nearest band
		// alone was Joan's two band-21 hubs for every bot of twenty-five and
		// over in the village: thirty of them and their horses on one meadow
		// (Remigiusz's screenshot, 18 September).
		int top = 0;
		for (int h = 0; h < hubTotal; ++h)
			top = std::max(top, (int)hubs[h].mobLevel);
		if (hubTotal > 0 && botLevel > top + 3)
		{
			for (int distance = 0; distance < 64 && count < cap &&
					count < PLAYERBOT_M1_OUTGROWN_HUB_CHOICES_MIN; ++distance)
				for (int h = 0; h < hubTotal && count < cap; ++h)
					if (top - hubs[h].mobLevel == distance)
						out[count++] = h;
			return count;
		}
		int best = 1000;
		for (int h = 0; h < hubTotal; ++h)
			best = std::min(best, abs(hubs[h].mobLevel - botLevel));
		for (int h = 0; h < hubTotal && count < cap; ++h)
			if (abs(hubs[h].mobLevel - botLevel) == best)
				out[count++] = h;
		return count;
	}

	// Whether a first village's hub is one of the valuable ones
	// (PLAYERBOT_M1_VALUE_HUBS).
	bool IsPlayerBotM1ValueHub(long mapIndex, long x, long y)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_M1_VALUE_HUBS) / sizeof(PLAYERBOT_M1_VALUE_HUBS[0]); ++i)
			if (PLAYERBOT_M1_VALUE_HUBS[i].mapIndex == mapIndex &&
					PLAYERBOT_M1_VALUE_HUBS[i].x == x && PLAYERBOT_M1_VALUE_HUBS[i].y == y)
				return true;
		return false;
	}

	// The soldiers and the bears first (community patch 2, point 14): of the
	// hubs the band admitted, the valuable ones when there are any; when there
	// are none, the valuable hubs of a band up to PLAYERBOT_M1_VALUE_HUB_UNDER
	// under the bot. A band with neither keeps its own choice - under level
	// twelve there is nothing but the dogs' kind to hunt.
	int PreferPlayerBotM1ValueHubs(long mapIndex, int botLevel, const TPlayerBotVillageHub* hubs,
			int hubTotal, int* choices, int count, int cap)
	{
		int valuable = 0;
		for (int i = 0; i < count; ++i)
			if (IsPlayerBotM1ValueHub(mapIndex, hubs[choices[i]].x, hubs[choices[i]].y))
				choices[valuable++] = choices[i];
		if (valuable > 0)
			return valuable;
		int widened = 0;
		for (int h = 0; h < hubTotal && widened < cap; ++h)
			if (hubs[h].mobLevel <= botLevel + 2 && hubs[h].mobLevel >= botLevel - PLAYERBOT_M1_VALUE_HUB_UNDER &&
					IsPlayerBotM1ValueHub(mapIndex, hubs[h].x, hubs[h].y))
				choices[widened++] = h;
		return widened > 0 ? widened : count;
	}

	// The second villages' choice: the band rule above, and when it admits
	// fewer than PLAYERBOT_M2_HUB_CHOICES_MIN hubs, the nearest bands by
	// distance fill the set. The first villages have thirty-two hubs over a
	// spread of thirty levels and never needed this; a second village has
	// three bands, and the one under twenty-five matches none of them.
	int CollectPlayerBotM2HubsForLevel(int botLevel, const TPlayerBotVillageHub* hubs,
			int hubTotal, int* out, int cap)
	{
		int count = CollectPlayerBotM1HubsForLevel(botLevel, hubs, hubTotal, out, cap);
		for (int distance = 0;
				count < PLAYERBOT_M2_HUB_CHOICES_MIN && count < hubTotal && count < cap && distance < 64;
				++distance)
		{
			for (int h = 0; h < hubTotal && count < PLAYERBOT_M2_HUB_CHOICES_MIN && count < cap; ++h)
			{
				if (abs(hubs[h].mobLevel - botLevel) != distance)
					continue;
				bool have = false;
				for (int i = 0; i < count && !have; ++i)
					have = out[i] == h;
				if (!have)
					out[count++] = h;
			}
		}
		return count;
	}

	void ManagePlayerBotWandering(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

	// A frontier map is worked, not squatted on.
	//
	// Wandering is what rotates a bot between hunting hubs, and in the tick it
	// runs only on an update where nothing was worth attacking. Orc Valley has
	// 4041 spawn points, so on that map there is always something in reach and
	// the rotation never came round: bots warped to the arrival point, found a
	// monster, and stayed. Twenty-four bots on the map, twenty-one of them at
	// the two hubs beside the entrance, inside a box thirty kilometres across -
	// while the Elite Orcs that carry the Orc Amulet, two hundred and sixty-five
	// spawns of them, stood on islands nobody visited. The market cannot trade
	// what the world never drops, and the world does not drop what nobody kills.
	//
	// So a bot that has not moved a hub's width in five minutes is walked to the
	// next hub even though there is something here to kill. It has to claim
	// the tick while it walks: otherwise target acquisition picks the monster it
	// has been standing next to and the walk never takes a step.
	bool ManagePlayerBotRelocation(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			state.dwCampSince = 0;
			state.dwRelocateSince = 0;
			return false;
		}
		// A follower goes where its leader goes. Two members deciding separately
		// to walk off is how a party comes apart.
		if (ch->GetParty() && ch->GetParty()->GetLeaderCharacter() != ch)
			return false;

		const int fromCamp = DISTANCE_APPROX(ch->GetX() - state.lCampX,
				ch->GetY() - state.lCampY);

		if (state.dwRelocateSince != 0)
		{
			// Arrived, or long enough trying. Either way this is home now.
			//
			// "Arrived" has to mean the hub, not a fixed number of paces. An
			// earlier draft ended the leg after one hub's width, which on a map
			// this size is halfway to nowhere: the far side of Orc Valley is
			// sixty-four thousand units from the entrance, so a bot needed five
			// separate legs with five minutes of standing still between them.
			// Measured after forty minutes of that: eight of the sixteen hubs
			// had somebody on them and every one of the eight was in the middle.
			const bool bHaveDestination = state.lRouteMapIndex == ch->GetMapIndex() &&
					(state.lRouteDestX != 0 || state.lRouteDestY != 0);
			const bool bArrived = bHaveDestination
					? DISTANCE_APPROX(ch->GetX() - state.lRouteDestX,
							ch->GetY() - state.lRouteDestY) <= PLAYERBOT_RELOCATE_ARRIVED
					: fromCamp >= PLAYERBOT_RELOCATE_DISTANCE;
			if (bArrived ||
					dwNow - state.dwRelocateSince >= PLAYERBOT_RELOCATE_TIMEOUT)
			{
				state.lCampX = ch->GetX();
				state.lCampY = ch->GetY();
				state.dwCampSince = dwNow;
				state.dwRelocateSince = 0;
				return false;
			}
			state.dwNextWanderTime = dwNow;
			ManagePlayerBotWandering(ch, state, dwNow);
			return true;
		}

		if (state.dwCampSince == 0 || fromCamp > PLAYERBOT_RELOCATE_DISTANCE)
		{
			// Already a hub away under its own steam, which is the ordinary case
			// and the whole point. Note where it is and let it hunt.
			state.lCampX = ch->GetX();
			state.lCampY = ch->GetY();
			state.dwCampSince = dwNow;
			return false;
		}
		if (dwNow - state.dwCampSince < PLAYERBOT_CAMP_TIMEOUT)
			return false;

		sys_log(0, "PLAYERBOT_TRAVEL: moving on pid=%u name=%s map=%ld camped=%us pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
				(unsigned int)((dwNow - state.dwCampSince) / 1000),
				ch->GetX(), ch->GetY());
		state.dwRelocateSince = dwNow;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		state.dwNextWanderTime = dwNow;
		ManagePlayerBotWandering(ch, state, dwNow);
		return true;
	}

	void ManagePlayerBotWandering(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		// A kept walk to the collect row's monsters is the Biologist's errand for
		// its whole length, the route continuation below included - stamped only
		// where the walk is taken up, it read "Szukam celu dla grupy" or "Ide do
		// Biologa" on most of the way. The same walk to the battle trial's
		// archers is travel, whose status is the trial's.
		const bool bHuntWalk = state.dwBiologistWalkUntil != 0 && dwNow < state.dwBiologistWalkUntil &&
				state.lBiologistWalkMap == ch->GetMapIndex();
		SetPlayerBotAction(state, bHuntWalk
				? (GetPlayerBotHorseTrialHuntMob(ch) != 0 ? BOT_ACTION_TRAVEL : BOT_ACTION_BIOLOGIST)
				: (ch->GetParty() ? BOT_ACTION_PARTY_ASSEMBLE : BOT_ACTION_TRAVEL), dwNow);

		// Party following is an active movement intent, not a new wander decision.
		// Refresh it on every AI update so followers do not stop for 8-12 seconds
		// between short route segments.
		if (ch->GetParty())
		{
			LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
			if (leader && leader != ch && leader->GetMapIndex() == ch->GetMapIndex())
			{
				int distToLeader = DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY());
				if (distToLeader > 500)
				{
					const int formAngle = (int)((ch->GetPlayerID() * 73) % 360);
					float fx = 0.0f, fy = 0.0f;
					const int formRadius = 250 + (int)(PlayerBotNavHash(ch->GetPlayerID()) % 201U);
					GetDeltaByDegree((float)formAngle, (float)formRadius, &fx, &fy);
					long targetX = leader->GetX() + (long)fx;
					long targetY = leader->GetY() + (long)fy;
					MovePlayerBot(ch, targetX, targetY, dwNow, 16, true);
					state.dwNextWanderTime = dwNow + 1000;
					return;
				}
			}
		}

		// Goto only carries the character to the currently issued waypoint.  An
		// existing multi-segment route must therefore be advanced every AI tick;
		// the wander timer controls choosing a new destination, not following an
		// already chosen one.
		if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size() &&
				state.lRouteMapIndex == ch->GetMapIndex())
		{
			MovePlayerBot(ch, state.lRouteDestX, state.lRouteDestY, dwNow, 32, true);
			return;
		}

		if (dwNow < state.dwNextWanderTime)
			return;

		state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 4000);

		// Define known hunting sectors by map
		long targetX = ch->GetX();
		long targetY = ch->GetY();

		// The ground belongs to the map, not to Chunjo. Every branch below used
		// to name 21, 23 or 24, so a Shinsoo or Jinno bot reached none of them
		// and fell through to a random walk on a map full of monsters it could
		// not find. What each village holds is the same; where it holds it is
		// not, so the tables are per map and were measured per map.
		const TPlayerBotVillageGround* ground =
				GetPlayerBotVillageGround(ch->GetMapIndex());

		if (ground != NULL && IsPlayerBotM1Map(ch->GetMapIndex()))
		{
			const DWORD pid = ch->GetPlayerID();

			// 1. Role: Metin breaker (25% of bots). These are the in-bounds
			// centres from metin2_map_b1/stone.txt, converted to world coordinates.
			// Four legacy Gemini entries near the southern map edge were manually
			// shifted from stone rows whose centres lie beyond this map's Y limit;
			// runtime attr checks confirmed that the shifted points were blocked.
			if (IsPlayerBotMetinHunting(state, dwNow))
			{
				LPCHARACTER knownMetin = FindKnownPlayerBotMetin(ch, dwNow);
				if (knownMetin &&
						DISTANCE_APPROX(ch->GetX() - knownMetin->GetX(), ch->GetY() - knownMetin->GetY()) >
						800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = knownMetin->GetX();
					targetY = knownMetin->GetY();
					if (!MovePlayerBot(ch, targetX, targetY, dwNow, 32, true, true) &&
							state.bStuckCounter >= 3)
					{
						s_mapKnownPlayerBotMetins.erase(knownMetin->GetVID());
						ClearPlayerBotRoute(state, true);
					}
					return;
				}

				BYTE hIdx = ChoosePlayerBotMetinHotspot(pid, state.uMetinHotspotIndex,
						dwNow, ch->GetMapIndex());
				if (ground->metinCount == 0)
					return;
				if (hIdx >= (BYTE)ground->metinCount)
					hIdx = (BYTE)(hIdx % ground->metinCount);
				long hx = ground->metins[hIdx].x;
				long hy = ground->metins[hIdx].y;
				long hotspotOffsetX = 0, hotspotOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x4d455449U + hIdx, 100, 650,
						hotspotOffsetX, hotspotOffsetY);
				hx += hotspotOffsetX;
				hy += hotspotOffsetY;
				int distToMetinHotspot = DISTANCE_APPROX(ch->GetX() - hx, ch->GetY() - hy);

				if (distToMetinHotspot < 1200)
				{
					// Reached current hotspot: wander in search of stones, then advance to next
					const int statSlot = GetPlayerBotMetinHotspotSlot(ch->GetMapIndex());
					if (statSlot >= 0)
						++s_adwPlayerBotMetinHotspotVisits[statSlot][hIdx];
					state.uMetinHotspotIndex =
							(state.uMetinHotspotIndex + 1) % (BYTE)ground->metinCount;
					state.dwNextWanderTime = dwNow + 2000;
					targetX = ch->GetX() + number(-600, 600);
					targetY = ch->GetY() + number(-600, 600);
				}
				else
				{
					// Rove toward next Metin hotspot
					state.dwNextWanderTime = dwNow + 1200;
					targetX = hx;
					targetY = hy;
				}
			}
			// 2. Role: Party Fighter (25% of bots - dense monster camps)
			else if (state.bBotRole == BOT_ROLE_PARTY_FIGHTER)
			{
				// Centres of group-spawn rectangles from metin2_map_b1/regen.txt.
				// The final point is still validated and snapped through server_attr.
				// The third number is the median monster level within 2500
				// units, measured from regen.txt through group.txt: a camp is
				// picked among those whose band holds the bot's level, so a
				// level-twenty party is not sent to the East beasts of three.
				const TPlayerBotVillageHub* partyCamps = ground->camps;
				const int campTotal = (int)ground->campCount;
				if (partyCamps == NULL || campTotal <= 0)
					return;
				int campChoices[16];
				int campCount = CollectPlayerBotM1HubsForLevel(GetPlayerBotVillageHuntLevel(ch),
						partyCamps, campTotal, campChoices, 16);
				// Not the ground a capitulation gave up (playerbot_anti_pk.h).
				campCount = FilterPlayerBotAvoidedHubs(state, ch->GetMapIndex(), partyCamps,
						campChoices, campCount, dwNow);
				if (campCount <= 0)
					return;

				int campIdx = campChoices[((pid / 4) + state.uMetinHotspotIndex) % campCount];
				long cx = partyCamps[campIdx].x;
				long cy = partyCamps[campIdx].y;
				long campOffsetX = 0, campOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x43414d50U + campIdx, 350, 1350,
						campOffsetX, campOffsetY);
				cx += campOffsetX;
				cy += campOffsetY;
				int distToCamp = DISTANCE_APPROX(ch->GetX() - cx, ch->GetY() - cy);

				if (distToCamp > 1800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = cx;
					targetY = cy;
				}
				else
				{
					state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 2000);
					targetX = ch->GetX() + number(-800, 800);
					targetY = ch->GetY() + number(-800, 800);
				}
			}
			// 3. Role: Area Mob Grinder (50% of bots - spread across 32 discrete hubs in 8 quadrants)
			else
			{
				// Each hub is the centre of a real group-spawn rectangle from
				// regen.txt, rather than a guessed coordinate.  Rectangle centres
				// still pass through the live attr/same-component validation.
				// Each hub is the centre of a real group-spawn rectangle from
				// regen.txt, rather than a guessed coordinate, and the third
				// number is the median monster level within 2500 units of it
				// (regen.txt through group.txt and group_group.txt, mob_proto
				// for the levels). The choice used to be by pid alone, so a bot
				// of ten hunted the tigers of the South-East and a bot of twenty
				// the dogs of the East - "boty bija na 9/10 lvlach nadal psy,
				// kolo 19/20 wciaz bija wilki". A bot picks among the hubs whose
				// band holds its level; the pid still spreads the population
				// over them. Rectangle centres still pass through the live
				// attr/same-component validation.
				const TPlayerBotVillageHub* hubs = ground->hubs;
				const int hubTotal = (int)ground->hubCount;
				if (hubs == NULL || hubTotal <= 0)
					return;
				int hubChoices[64];
				// The active herb row's level while its monster is wanted, the
				// bot's own otherwise (GetPlayerBotVillageHuntLevel).
				const int huntLevel = GetPlayerBotVillageHuntLevel(ch);
				int hubCount = CollectPlayerBotM1HubsForLevel(huntLevel,
						hubs, hubTotal, hubChoices, 64);
				// The soldiers and the bears ahead of the dogs (community patch
				// 2, point 14) - unless a herb row's monster is what the bot is
				// hunting here, which has its own level and its own ground.
				// Every row of the list is back on the table the first time
				// the bands are filled again: the valuable set is re-read each
				// decision.
				if (huntLevel == (int)ch->GetLevel())
					hubCount = PreferPlayerBotM1ValueHubs(ch->GetMapIndex(), huntLevel,
							hubs, hubTotal, hubChoices, hubCount, 64);
				hubCount = FilterPlayerBotAvoidedHubs(state, ch->GetMapIndex(), hubs,
						hubChoices, hubCount, dwNow);
				if (hubCount <= 0)
					return;
				int hubIdx = hubChoices[((pid / 2) + state.uMetinHotspotIndex) % hubCount];
				long hubX = hubs[hubIdx].x;
				long hubY = hubs[hubIdx].y;
				long hubOffsetX = 0, hubOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x48554200U + hubIdx, 250, 1100,
						hubOffsetX, hubOffsetY);
				hubX += hubOffsetX;
				hubY += hubOffsetY;
				int distToHub = DISTANCE_APPROX(ch->GetX() - hubX, ch->GetY() - hubY);

				if (distToHub > 1800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = hubX;
					targetY = hubY;
				}
				else
				{
					state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 2000);
					targetX = ch->GetX() + number(-800, 800);
					targetY = ch->GetY() + number(-800, 800);
				}
			}
		}
		else if (ground != NULL && IsPlayerBotM2Map(ch->GetMapIndex()))
		{
			const DWORD pid = ch->GetPlayerID();
			// The Bestial Captain, while he stands: anybody of the band goes,
			// the way the valley goes for the Orc Chief. Nine sectors round his
			// point are asked, once every thirty seconds for everybody.
			if (ch->GetLevel() >= PLAYERBOT_M2_CAPTAIN_MIN_LEVEL &&
					ground->captain.x != 0)
			{
				long bossX = 0, bossY = 0;
				if (IsPlayerBotBossAlive(ch->GetMapIndex(), ground->captain.x, ground->captain.y,
						591, dwNow, &bossX, &bossY) &&
						DISTANCE_APPROX(ch->GetX() - bossX, ch->GetY() - bossY) > 600)
				{
					PlayerBotLogThrottled("raid_captain", dwNow,
							"PLAYERBOT_RAID: heading for boss race=591 pid=%u name=%s level=%u map=%ld party=%u guild=%u",
							ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetMapIndex(),
							ch->GetParty() ? (unsigned int)ch->GetParty()->GetMemberCount() : 0U,
							ch->GetGuild() ? (unsigned int)ch->GetGuild()->GetID() : 0U);
					long offsetX = 0, offsetY = 0;
					GetPlayerBotStableOffset(pid, 0x43415054U, 100, 350, offsetX, offsetY);
					state.dwNextWanderTime = dwNow + 1500;
					MovePlayerBot(ch, bossX + offsetX, bossY + offsetY, dwNow, 32, true, true);
					return;
				}
			}
			if (ShouldPlayerBotHuntM2Bestials(ch) && ground->bestials != NULL)
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				const size_t bestialIndex = (pid + state.uMetinHotspotIndex) % 2;
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid, 0x42455354U + (DWORD)bestialIndex,
						100, 450, offsetX, offsetY);
				targetX = ground->bestials[bestialIndex].x + offsetX;
				targetY = ground->bestials[bestialIndex].y + offsetY;
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1000)
				{
					++state.uMetinHotspotIndex;
					state.dwNextWanderTime = dwNow + number(5000, 9000);
					targetX = ch->GetX() + number(-500, 500);
					targetY = ch->GetY() + number(-500, 500);
				}
			}
			else
			{
				// Real spawn clusters from this village's own regen.txt, each with
				// the median monster level round it, and a bot goes only to the
				// hubs of its own band, the way the first villages do it. Persistent
				// hub assignment by pid stops the cohort from tracing one route.
				//
				// The table used to be twelve hubs taken by pid with no band, and
				// the wander pass only runs on a tick nothing was worth attacking:
				// a bot came in at the gate, found monsters, and chain-killed its
				// way outward from there for the rest of its life. Jayang's gate is
				// in its south and Bakra's in its north, and the far half of each -
				// the 501-504 ground of 29-36 - had nobody on it ("boty z Shinsoo
				// omijaja gorna czesc Jayang, z Jinno dolna czesc Bakra", blasty,
				// 16 September). The band choice sends the 33+ there, and the
				// outgrown-prey rule in the combat policy is what lets them leave.
				const TPlayerBotVillageHub* hubs = ground->hubs;
				int hubChoices[32];
				int hubCount = CollectPlayerBotM2HubsForLevel(ch->GetLevel(),
						hubs, (int)ground->hubCount, hubChoices, 32);
				hubCount = FilterPlayerBotAvoidedHubs(state, ch->GetMapIndex(), hubs,
						hubChoices, hubCount, dwNow);
				if (hubCount <= 0)
					return;
				const size_t hubIndex =
						(size_t)hubChoices[(pid + state.uMetinHotspotIndex) % (DWORD)hubCount];
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid, 0x4d324855U + (DWORD)hubIndex,
						150, 700, offsetX, offsetY);
				targetX = hubs[hubIndex].x + offsetX;
				targetY = hubs[hubIndex].y + offsetY;
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1400)
				{
					++state.uMetinHotspotIndex;
					targetX = ch->GetX() + number(-700, 700);
					targetY = ch->GetY() + number(-700, 700);
				}
			}
		}
		else if (ground != NULL && IsPlayerBotM3Map(ch->GetMapIndex()))
		{
			// Centres of that guild map's own infected-animal regen rectangles.
			// Keeping the arrival/return strip out of the set also prevents
			// farming inside the teleporter's BANPK area.
			const TPlayerBotVillageHub* hubs = ground->hubs;
			const DWORD pid = ch->GetPlayerID();
			const size_t hubIndex = (pid + state.uMetinHotspotIndex) % ground->hubCount;
			long offsetX = 0, offsetY = 0;
			GetPlayerBotStableOffset(pid, 0x4d334855U + (DWORD)hubIndex,
					100, 550, offsetX, offsetY);
			targetX = hubs[hubIndex].x + offsetX;
			targetY = hubs[hubIndex].y + offsetY;
			if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1100)
			{
				++state.uMetinHotspotIndex;
				targetX = ch->GetX() + number(-600, 600);
				targetY = ch->GetY() + number(-600, 600);
			}
		}
		else if (IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			// Hubs are hand-placed on spawn points from regen.txt - a hub can never
			// be planted inside an obstacle - and carry the level band they are
			// for. Which of them a bot walks to is decided by what the population
			// has seen there, see ChoosePlayerBotHuntingHub.
			//
			// Orc Valley by level: the five Fanatic (35) / Arahan (38) islands
			// from the wiki's map of them, for thirty to thirty-nine; the sixteen
			// density hubs that cover the rest of the map, for thirty-six and up
			// on their own; the three Black Orc (46) camps for a party of forty
			// and up; the central island's Tormentors (49), who carry the Curse
			// Book, for a party of forty-five and up. Client-map cells for the
			// player's eye: camps (601,625), (774,923), (933,639); centre (767,792).
			// The Forest (67). Its own regen, densest 6400-unit cells first, each
			// hub on the real spawn point nearest that cell's centre; the band is
			// the cell's median monster level less three, the same rule the
			// village hubs use. Trent carries 2301-2305 of 65-71 over 527 spawn
			// points and no stones at all.
			const TPlayerBotHuntingHub forestHubs[] = {
				{  316300,   16500, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 },
				// Moved 325 units off a blocked cell (2.0.77, server_attr).
				{  316325,   40575, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 },
				{  310300,   27200, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 },
				{  324400,   36200, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 },
				{  284800,   29700, 64, 255, false, 0 },
				{  310500,   21500, 64, 255, false, 0 },
				{  296600,   36500, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 },
				{  299000,   28100, PLAYERBOT_FOREST_MIN_LEVEL, 255, false, 0 }
			};
			// The Red Forest (68): 2311-2315 of 74-82 over 693 spawn points, and
			// the two hardest of them (80 and 82) are what makes the upper hubs
			// a party's ground rather than anybody's.
			const TPlayerBotHuntingHub redForestHubs[] = {
				// The first and third stood on blocked cells; both moved onto
				// the nearest open ground (2.0.77, server_attr), the first to
				// the new arrival point.
				{ PLAYERBOT_RED_FOREST_ARRIVAL_X, PLAYERBOT_RED_FOREST_ARRIVAL_Y, PLAYERBOT_RED_FOREST_MIN_LEVEL, 255, false, 0 },
				{ 1070400,   67400, PLAYERBOT_RED_FOREST_MIN_LEVEL, 255, false, 0 },
				{ 1122625,   15675, PLAYERBOT_RED_FOREST_MIN_LEVEL, 255, false, 0 },
				{ 1078200,   40800, 73, 255, false, 0 },
				{ 1053300,   43700, PLAYERBOT_RED_FOREST_MIN_LEVEL, 255, false, 0 },
				{ 1080600,   16200, 73, 255, false, 0 },
				{ 1092600,   15100, 73, 255, false, 0 },
				{ 1092600,   42500, PLAYERBOT_RED_FOREST_MIN_LEVEL, 255, false, 0 }
			};
			// Doyyumhwaji (62). Its own regen and server_attr (23 September), the
			// Forest's rule: the densest 6400-unit cells first, each hub on the
			// real spawn point nearest the cell's centre that stands on the map's
			// one walkable area with open ground round it and outside the safe
			// zone, the band the cell's median monster level less three. The
			// east third, where Jinno's gate opens, is sparser and was measured
			// on its own, or every hub would be a half-map walk from it. The
			// last row is the Flame King (2206, level 73, a boss of a hundred
			// thousand), whom special_spawns.txt puts at one of three points
			// every two hours or so with an escort: a party's raid, walked to
			// wherever he stands.
			const TPlayerBotHuntingHub fireLandHubs[] = {
				{  631200,  719000, 67, 255, false, 0 },
				{  610900,  681800, 68, 255, false, 0 },
				{  656000,  732300, 67, 255, false, 0 },
				{  603100,  688200, 68, 255, false, 0 },
				{  630600,  660000, 67, 255, false, 0 },
				{  605200,  720700, 67, 255, false, 0 },
				{  681900,  752900, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  661700,  753400, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  597300,  702400, 68, 255, false, 0 },
				{  611400,  701100, 68, 255, false, 0 },
				{  603900,  662900, 67, 255, false, 0 },
				{  637300,  681500, 67, 255, false, 0 },
				{  669600,  726300, 67, 255, false, 0 },
				{  676500,  630900, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  721400,  636900, 67, 255, false, 0 },
				{  600100,  681900, 68, 255, false, 0 },
				{  631500,  687200, 67, 255, false, 0 },
				{  656000,  714100, 67, 255, false, 0 },
				{  623100,  725200, 67, 255, false, 0 },
				{  603600,  732600, 67, 255, false, 0 },
				{  706300,  679700, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  687400,  701100, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  700100,  701100, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  727200,  643300, 67, 255, false, 0 },
				{  718800,  668000, 67, 255, false, 0 },
				{  714100,  714000, 67, 255, false, 0 },
				{  719300,  683000, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, false, 0 },
				{  598000,  682200, PLAYERBOT_FIRE_LAND_MIN_LEVEL, 255, true, 2206 }
			};
			// The Demon Tower (66). Only two clusters carry 1001-1004 at all, and
			// this is a map a bot visits for one specimen rather than lives on,
			// so two hubs is the whole table.
			const TPlayerBotHuntingHub demonTowerHubs[] = {
				{  143400,  860100, PLAYERBOT_DEMON_TOWER_MIN_LEVEL, 255, false, 0 },
				{  143900,  857000, PLAYERBOT_DEMON_TOWER_MIN_LEVEL, 255, false, 0 }
			};
			const TPlayerBotHuntingHub orcValleyHubs[] = {
				{ 276600, 684600, PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL, PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL, false },
				{ 281700, 795300, PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL, PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL, false },
				{ 290300, 799400, PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL, PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL, false },
				{ 348300, 705800, PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL, PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL, false },
				{ 391100, 738100, PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL, PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL, false },
				{ 315800, 732600, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 342600, 729800, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 335500, 758000, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 328000, 743600, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 277800, 793500, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 347700, 797500, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 334000, 800200, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 343200, 743100, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 391700, 696600, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 330500, 727300, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 292200, 751000, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 365100, 777800, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 271500, 683700, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 297600, 716400, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 302500, 777000, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false }, { 336500, 703600, PLAYERBOT_ORC_VALLEY_MIN_LEVEL, 255, false },
				{ 316600, 728500, PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL, 255, true },
				{ 333200, 758600, PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL, 255, true },
				{ 350300, 726900, PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL, 255, true },
				{ 332900, 747200, PLAYERBOT_ORC_VALLEY_CENTRE_MIN_LEVEL, 255, true },
				// The Orc Chief (691, level 50, boss) from boss.txt, cell (770,757),
				// back every thirty minutes: a raid for a party, guild mates first.
				// Anybody of the band, not only a party: the Chief has twenty-five
				// thousand health and comes back every half hour, and a valley
				// full of bots piling onto him is how a valley full of players does it.
				{ 333000, 741300, PLAYERBOT_ORC_VALLEY_CENTRE_MIN_LEVEL, 255, false, 691 }
			};
			const TPlayerBotHuntingHub desertHubs[] = {
				{ 291300, 515700, 0, 255, false }, { 237500, 525900, 0, 255, false }, { 264600, 526100, 0, 255, false },
				{ 317900, 526100, 0, 255, false }, { 336900, 534300, 0, 255, false }, { 245100, 542500, 0, 255, false },
				{ 264500, 552300, 0, 255, false }, { 327700, 552900, 0, 255, false }, { 253900, 570100, 0, 255, false },
				{ 327800, 579500, 0, 255, false }, { 321600, 582700, 0, 255, false }, { 273800, 614900, 0, 255, false }
			};
			// Mount Sohan (61), from map_n_snowm_01/regen.txt: the Infected of 49-58
			// in the south for forty-eight and up, the ice creatures of 62-66 in the
			// north for fifty-eight and up. The Spider Dungeon's eight, for
			// forty-eight and up - knights of 50 to 58, and the level filter keeps a
			// bot on its own off the ones it cannot touch.
			const TPlayerBotHuntingHub sohanHubs[] = {
				{ 432000, 272000, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 393600, 265600, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 470400, 291200, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 438400, 272000, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 412800, 278400, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 483200, 208000, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 380800, 220800, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 470400, 284800, PLAYERBOT_SOHAN_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 489600, 284800, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 464000, 240000, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 432000, 176000, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 432000, 220800, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 489600, 227200, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				{ 387200, 240000, PLAYERBOT_SOHAN_ICE_MIN_LEVEL, PLAYERBOT_SOHAN_MAX_LEVEL, false },
				// Nine Tails (1901, level 72, a hundred and sixty-six thousand
				// health, two ice golems and a yeti beside him) from boss.txt cell
				// (749,629) with a spread of 150x200 cells, back every two hours:
				// a party's raid, like the Queen's.
				{ PLAYERBOT_SOHAN_NINE_TAILS_X, PLAYERBOT_SOHAN_NINE_TAILS_Y, PLAYERBOT_SOHAN_MIN_LEVEL, 255, true, 1901 }
			};
			const TPlayerBotHuntingHub spiderHubs[] = {
				{ 70000, 505300, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false }, { 80400, 519800, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false },
				{ 69800, 517300, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false }, { 70300, 527500, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false },
				{ 82100, 527400, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false }, { 59500, 517700, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false },
				{ 58600, 504300, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false }, { 59800, 527600, PLAYERBOT_SPIDER_MIN_LEVEL, 255, false },
				// The Spider Queen (2091, level 60, boss) at the end of the dungeon,
				// boss.txt cell (385,387), back every four hours: a party's raid.
				// A party's work and nobody else's: two hundred thousand health at
				// level sixty.
				{ 89700, 525100, PLAYERBOT_SPIDER_MIN_LEVEL, 255, true, 2091 }
			};
			// The second Spider Dungeon, measured the way the temple below was:
			// every spawn point of regen.txt (668 of them, through group.txt
			// and group_group.txt) binned into 6400-unit cells, the eleven
			// richest taken, and each hub put on the actual spawn point nearest
			// its cell's centre, checked free on server_attr. The east and the
			// south run to 66; the middle band is 62-63. None of them attacks
			// first, so no party is needed anywhere; the Elite Queen by the V3
			// warp is level 97 and gets no row.
			const TPlayerBotHuntingHub spiderV2Hubs[] = {
				{ 694400, 483300, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false }, { 725300, 483600, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false },
				{ 713700, 482500, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false }, { 714700, 470700, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false },
				{ 700600, 482600, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false }, { 688200, 483200, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false },
				{ 682200, 484100, PLAYERBOT_SPIDER_V2_MIN_LEVEL, 255, false },
				{ 725500, 501800, 58, 255, false }, { 695700, 503200, 58, 255, false },
				{ 687400, 502800, 58, 255, false }, { 713900, 501300, 58, 255, false }
			};
			// The Hwang Temple, from the density of its own regen.txt rather than
			// from the map: every spawn point binned into 6400-unit cells and the
			// richest taken, which is the same unit the population's own memory
			// scores a place by. It falls into two bands, which is how the map is
			// built - the Elite Esoterics of 52-55 in the west where the temple
			// is entered, the Tree Turtle Soldier and the Bogey of 55-58 in the
			// east. Every one of these was then checked against milgyo's
			// server_attr for standing room; two of the density centres came out
			// inside a wall and a river and were moved to the nearest free cell,
			// which is what the odd numbers are.
			//
			//
			// Measured again on 9 September against the whole regen: three
			// cells of 800-1200 spawn points had no hub within nineteen
			// kilometres - the south-east corner (627200,57600), the ground
			// east of the middle (620800,64000) and the frog field north of the
			// entrance (582400,128000) - so the population walked the west and
			// the east bands and never the middle or the corner. The rows below
			// cover them, each probed standable in milgyo's server_attr.
			//
			// The boss: boss.txt puts group 2110 at cell (374,420) every two
			// hours - the Yellow Tiger Spectre (1304, level 75, 178 040 hit
			// points, boss rank) with two Frog Generals and two Tree Frog
			// Chiefs beside him. Twenty levels over the band's bots, so the
			// hub is a party's raid like the Queen's and Nine Tails': a full
			// party of fifty-fives may challenge seventy-five
			// (PLAYERBOT_PARTY_LEVEL_BONUS_PER_MEMBER), and the raid swarm
			// bonus puts him above the frogs round him once three have set
			// out. The other boss point, group 727 at (910,847), is an Elite
			// Esoteric Summoner pack of 57 and needs no hub of its own - the
			// east band hunts through it. The Demon Tower entrance (the
			// Guardian, 20348, at (590800,110800)) stands on ground with no
			// spawn within 2500 units; a hub there would be a hub for nothing.
			const TPlayerBotHuntingHub hwangHubs[] = {
				{ 553600, 118400, PLAYERBOT_HWANG_MIN_LEVEL, 255, false, 0 },
				{ 553600,  92800, PLAYERBOT_HWANG_MIN_LEVEL, 255, false, 0 },
				{ 553600,  67200, PLAYERBOT_HWANG_MIN_LEVEL, 255, false, 0 },
				{ 585600,  66950, PLAYERBOT_HWANG_MIN_LEVEL, 255, false, 0 },
				{ 585600, 131200, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 588800,  96000, PLAYERBOT_HWANG_MIN_LEVEL, 255, false, 0 },
				{ 630500, 137600, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 630400, 118400, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 624000, 112000, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 630400,  86400, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 624000,  67200, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 630400,  60800, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 604800,  67200, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, false, 0 },
				{ 575000,  93200, PLAYERBOT_HWANG_EAST_MIN_LEVEL, 255, true, 1304 }
			};
			const bool inDesert = ch->GetMapIndex() == PLAYERBOT_MAP_DESERT;
			const TPlayerBotHuntingHub* hubs = orcValleyHubs;
			size_t hubCount = sizeof(orcValleyHubs) / sizeof(orcValleyHubs[0]);
			if (inDesert)
			{
				hubs = desertHubs;
				hubCount = sizeof(desertHubs) / sizeof(desertHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_SOHAN)
			{
				hubs = sohanHubs;
				hubCount = sizeof(sohanHubs) / sizeof(sohanHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_SPIDER_V1)
			{
				hubs = spiderHubs;
				hubCount = sizeof(spiderHubs) / sizeof(spiderHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_SPIDER_V2)
			{
				hubs = spiderV2Hubs;
				hubCount = sizeof(spiderV2Hubs) / sizeof(spiderV2Hubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER)
			{
				hubs = demonTowerHubs;
				hubCount = sizeof(demonTowerHubs) / sizeof(demonTowerHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_FOREST)
			{
				hubs = forestHubs;
				hubCount = sizeof(forestHubs) / sizeof(forestHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_RED_FOREST)
			{
				hubs = redForestHubs;
				hubCount = sizeof(redForestHubs) / sizeof(redForestHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_HWANG)
			{
				hubs = hwangHubs;
				hubCount = sizeof(hwangHubs) / sizeof(hwangHubs[0]);
			}
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_FIRE_LAND)
			{
				hubs = fireLandHubs;
				hubCount = sizeof(fireLandHubs) / sizeof(fireLandHubs[0]);
			}
			const DWORD pid = ch->GetPlayerID();
			// A stone anybody has seen on this map comes before any hub while the
			// bot hunts stones - by role, or on an expedition. Off the town map
			// this used to be the one thing a hunter did not do.
			if (IsPlayerBotMetinHunting(state, dwNow))
			{
				LPCHARACTER knownMetin = FindKnownPlayerBotMetin(ch, dwNow);
				if (knownMetin &&
						DISTANCE_APPROX(ch->GetX() - knownMetin->GetX(), ch->GetY() - knownMetin->GetY()) > 800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					if (!MovePlayerBot(ch, knownMetin->GetX(), knownMetin->GetY(), dwNow, 32, true, true) &&
							state.bStuckCounter >= 3)
					{
						s_mapKnownPlayerBotMetins.erase(knownMetin->GetVID());
						ClearPlayerBotRoute(state, true);
					}
					return;
				}
			}
			// The collect row's monsters the last scan found come before any hub
			// too (StartPlayerBotMaterialHunt). A fight on the way parked the
			// route and the hub choice after it walked the bot off by level: on
			// m2zip 43 of 91 bots in Orc Valley stood in parties reading "Szukam
			// celu dla grupy" with a collect place each, a walk to the Black Orcs
			// and a band hub by turns (17 September).
			if (state.dwBiologistWalkUntil != 0)
			{
				// The battle trial's archers keep the walk too: the Biologist's
				// hunt answers nothing while a horse trial is open.
				const DWORD trialMob = GetPlayerBotHorseTrialHuntMob(ch);
				const DWORD huntMob = trialMob != 0 ? trialMob : GetPlayerBotBiologistHuntMob(ch);
				const bool arrived = DISTANCE_APPROX(ch->GetX() - state.lBiologistWalkX,
						ch->GetY() - state.lBiologistWalkY) <= PLAYERBOT_BIOLOGIST_WALK_ARRIVED;
				if (dwNow >= state.dwBiologistWalkUntil || arrived || huntMob < 500 ||
						state.lBiologistWalkMap != ch->GetMapIndex())
					state.dwBiologistWalkUntil = 0;
				else
				{
					// The top of this function stamped PARTY_ASSEMBLE on a party
					// bot: 56 of 90 bots in the valley read "Szukam celu dla grupy"
					// while walking here.
					SetPlayerBotAction(state, trialMob != 0 ? BOT_ACTION_TRAVEL : BOT_ACTION_BIOLOGIST, dwNow);
					state.dwNextWanderTime = dwNow + 1200;
					if (!MovePlayerBot(ch, state.lBiologistWalkX, state.lBiologistWalkY, dwNow, 24, true, true) &&
							state.bStuckCounter >= 3)
					{
						state.dwBiologistWalkUntil = 0;
						ClearPlayerBotRoute(state, true);
					}
					return;
				}
			}
			size_t hubIndex = 0;
			int hubScore = 0;
			bool bHubReachable = false;
			// The hub chosen a moment ago is still the hub, unless the bot has
			// outgrown its band or the choice is old enough to revisit - sooner
			// on a stone hunt, which is done by covering ground.
			const DWORD hubStick = IsPlayerBotMetinHunting(state, dwNow)
					? PLAYERBOT_METIN_EXPEDITION_HUB_STICK : PLAYERBOT_HUB_STICK_TIME;
			// A hub is kept for a few minutes so a bot does not cross the map
			// twice for a slightly better camp - but a boss hub is only a place
			// while the boss is standing on it. Keeping one for four minutes
			// after he went down is what put a column of bots on an empty
			// snowfield with nothing to fight: the stick has to ask again.
			const bool stickIsBoss = state.wHuntingHub < hubCount &&
					hubs[state.wHuntingHub].wBossRace != 0;
			bool stickBossStanding = true;
			if (stickIsBoss)
			{
				long bossX = 0, bossY = 0;
				stickBossStanding = IsPlayerBotBossAlive(ch->GetMapIndex(),
						hubs[state.wHuntingHub].x, hubs[state.wHuntingHub].y,
						hubs[state.wHuntingHub].wBossRace, dwNow, &bossX, &bossY);
				if (!stickBossStanding)
					PlayerBotLogThrottled("raid_over", dwNow,
							"PLAYERBOT_RAID: boss down, going back to work race=%u pid=%u name=%s map=%ld",
							(unsigned int)hubs[state.wHuntingHub].wBossRace,
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex());
			}
			if (state.wHuntingHub < hubCount && state.dwHubChosenTime != 0 &&
					dwNow - state.dwHubChosenTime < hubStick && stickBossStanding &&
					ch->GetLevel() >= hubs[state.wHuntingHub].bMinLevel &&
					ch->GetLevel() <= hubs[state.wHuntingHub].bMaxLevel)
			{
				hubIndex = state.wHuntingHub;
				bHubReachable = true;
			}
			else
				bHubReachable = ChoosePlayerBotHuntingHub(ch, hubs, hubCount, dwNow,
						(size_t)-1, hubIndex, hubScore);
			if (bHubReachable)
			{
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid,
						(inDesert ? 0x44455348U : 0x4f524348U) + (DWORD)hubIndex,
						150, 700, offsetX, offsetY);
				targetX = hubs[hubIndex].x + offsetX;
				targetY = hubs[hubIndex].y + offsetY;
				// A boss hub is wherever the boss is, not the point on the table.
				if (hubs[hubIndex].wBossRace != 0)
				{
					long bossX = 0, bossY = 0;
					if (IsPlayerBotBossAlive(ch->GetMapIndex(), hubs[hubIndex].x, hubs[hubIndex].y,
							hubs[hubIndex].wBossRace, dwNow, &bossX, &bossY))
					{
						targetX = bossX + offsetX / 2;
						targetY = bossY + offsetY / 2;
					}
				}
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1400)
				{
					// Standing on the hub with nothing left to fight here. Choose
					// again with this one left out - a seven-hundred-unit nudge and
					// another ten seconds of waiting is how a bot ends up guarding
					// one respawn for an hour.
					size_t nextIndex = 0;
					int nextScore = 0;
					if (ChoosePlayerBotHuntingHub(ch, hubs, hubCount, dwNow, hubIndex, nextIndex, nextScore))
					{
						hubIndex = nextIndex;
						hubScore = nextScore;
						state.dwHubChosenTime = dwNow;
						GetPlayerBotStableOffset(pid,
								(inDesert ? 0x44455348U : 0x4f524348U) + (DWORD)hubIndex,
								150, 700, offsetX, offsetY);
						targetX = hubs[hubIndex].x + offsetX;
						targetY = hubs[hubIndex].y + offsetY;
					}
					else
						bHubReachable = false;
				}
			}
			if (!bHubReachable)
			{
				// Nothing on the list is for this bot from where it stands - walled
				// into a pocket, or a party hub without the party. Work the ground
				// here rather than plan a route that cannot exist.
				targetX = ch->GetX() + number(-1200, 1200);
				targetY = ch->GetY() + number(-1200, 1200);
			}
			else if (state.wHuntingHub != (WORD)hubIndex)
			{
				state.wHuntingHub = (WORD)hubIndex;
				state.dwHubChosenTime = dwNow;
				if (hubs[hubIndex].wBossRace != 0)
				{
					NotePlayerBotRaider(hubs[hubIndex].wBossRace, pid, dwNow);
					sys_log(0, "PLAYERBOT_RAID: heading for boss race=%u pid=%u name=%s level=%u map=%ld party=%u guild=%u raiders=%d",
							(unsigned int)hubs[hubIndex].wBossRace, ch->GetPlayerID(), ch->GetName(),
							ch->GetLevel(), ch->GetMapIndex(),
							ch->GetParty() ? (unsigned int)ch->GetParty()->GetMemberCount() : 0U,
							ch->GetGuild() ? (unsigned int)ch->GetGuild()->GetID() : 0U,
							CountPlayerBotRaiders(hubs[hubIndex].wBossRace, dwNow));
				}
				sys_log(0, "PLAYERBOT_SPOT: hub chosen pid=%u name=%s level=%u map=%ld hub=%u pos=(%ld,%ld) band=%u-%u party_hub=%d party=%u guild=%u score=%d",
						pid, ch->GetName(), ch->GetLevel(), ch->GetMapIndex(), (unsigned int)hubIndex,
						hubs[hubIndex].x, hubs[hubIndex].y, hubs[hubIndex].bMinLevel, hubs[hubIndex].bMaxLevel,
						hubs[hubIndex].bNeedsParty ? 1 : 0,
						ch->GetParty() ? (unsigned int)ch->GetParty()->GetMemberCount() : 0U,
						ch->GetGuild() ? ch->GetGuild()->GetID() : 0U, hubScore);
			}
		}
		else if (IsPlayerBotMonkeyMap(ch->GetMapIndex()))
		{
			// Chambers joined only by the GOTO doors - the table and the
			// reasoning are in playerbot_movement.h. A bot patrols the spawn
			// points of the chamber it is standing in, and once it has worked
			// that chamber over it walks to the portal for the next one. Every
			// route it plans therefore lies inside one chamber, which is the
			// only kind of route this maze has: asking for a room across a
			// portal is what left the whole population in the entrance corridor.
			const long mapIndex = ch->GetMapIndex();
			long baseX = 0, baseY = 0;
			GetPlayerBotMonkeyBase(mapIndex, baseX, baseY);
			const DWORD pid = ch->GetPlayerID();
			// UpdatePlayerBotMonkeyChamber answered this at the top of the tick.
			const int chamber = state.bMonkeyChamber == 255
					? -1 : (int)state.bMonkeyChamber;
			if (chamber < 0)
			{
				// Between the rooms is not a place. Work the ground here rather
				// than plan a route to somewhere that cannot be walked to.
				targetX = ch->GetX() + number(-450, 450);
				targetY = ch->GetY() + number(-450, 450);
			}
			else
			{
				const TPlayerBotMonkeyChamber& room = PLAYERBOT_MONKEY_CHAMBERS[chamber];
				int exitChambers[8];
				int exitDoors[8];
				const int exits =
						dwNow - state.dwMonkeyChamberTime >= PLAYERBOT_MONKEY_CHAMBER_DWELL
						? GetPlayerBotMonkeyChamberExits(mapIndex, chamber,
								exitChambers, exitDoors, 8)
						: 0;
				int chosenExit = -1;
				for (int step = 0; step < exits && chosenExit < 0; ++step)
				{
					// Its own order over the doors, and never straight back the
					// way it came unless that is the only one: a population that
					// walks through the dungeon instead of bouncing off its
					// first wall. uMetinHotspotIndex is in the hash because the
					// stuck handling below advances it, so a door that cannot be
					// reached is not chosen twice.
					const int candidate = (int)((PlayerBotNavHash(pid ^
							(0x4d4b4559U + (DWORD)chamber * 31U +
							(DWORD)state.uMetinHotspotIndex)) + (DWORD)step) % (DWORD)exits);
					if (exits > 1 && exitChambers[candidate] == (int)state.bMonkeyPrevChamber)
						continue;
					chosenExit = candidate;
				}

				long doorX = 0, doorY = 0;
				if (chosenExit >= 0 && GetPlayerBotMonkeyDoorPosition(mapIndex,
						exitDoors[chosenExit], doorX, doorY))
				{
					// The crossing is the walk: warp_npc_event teleports anyone
					// within 300 units of the GOTO NPC, so the door's own cell is
					// the destination and arriving at it is the whole move.
					targetX = doorX;
					targetY = doorY;
				}
				else
				{
					const int spot = state.bMonkeySpot % room.bSpotCount;
					long offsetX = 0, offsetY = 0;
					GetPlayerBotStableOffset(pid, 0x4d4f4e4bU + (DWORD)spot,
							50, 250, offsetX, offsetY);
					targetX = baseX + room.spots[spot].x * 100L + offsetX;
					targetY = baseY + room.spots[spot].y * 100L + offsetY;
					if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 900)
					{
						++state.bMonkeySpot;
						targetX = ch->GetX() + number(-450, 450);
						targetY = ch->GetY() + number(-450, 450);
					}
				}
			}
		}
		else
		{
			// Wander in random nearby direction
			targetX += number(-1500, 1500);
			targetY += number(-1500, 1500);
		}

		// On the horse, if there is one and the hub is far. Reported from the
		// Discord: "bots travelling a long way go on foot and the horse runs
		// along behind them" - which is exactly what it looks like, because
		// StopRiding summons the horse as a follower and nothing put the rider
		// back on it. A hunting hub is chosen up to twenty kilometres away and
		// every wander leg asked for allowHorse=false, so the whole crossing was
		// walked. UpdatePlayerBotTravelMount still refuses to mount inside
		// PLAYERBOT_HORSE_MOUNT_DISTANCE, so a step across a clearing is
		// unaffected.
		if (!MovePlayerBot(ch, targetX, targetY, dwNow, 32, true, true))
		{
			state.dwNextWanderTime = dwNow + 1500;
			if (state.bStuckCounter >= 3)
			{
				// Abandon a genuinely unreachable region instead of recomputing the
				// identical path forever.  The shared index selects another hotspot,
				// camp or hub depending on the bot's role.
				++state.uMetinHotspotIndex;
				ClearPlayerBotRoute(state, true);
			}
		}
	}
}

#endif
