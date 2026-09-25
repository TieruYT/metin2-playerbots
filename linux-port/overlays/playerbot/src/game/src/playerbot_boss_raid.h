#ifndef __INC_METIN2_PLAYERBOT_BOSS_RAID_H__
#define __INC_METIN2_PLAYERBOT_BOSS_RAID_H__

// Boss raids: a world boss broken by a crowd of one kingdom.
//
// A boss used to be a place to walk to (the boss hubs of
// playerbot_wandering.h), and three things kept him standing. A monster one
// bot had picked was nobody else's target (IsTargetClaimedByAnotherBot), so
// whoever came second chained onto his escort instead. The rule that gives
// up a fight going nowhere gave up a boss that heals faster than one bot
// hurts him - the Spider Queen takes back a tenth of her health every ten
// seconds, the Yellow Tiger Spectre an eighth every seven - and put him on
// the failed list for two minutes. And the walk to a boss hub was chosen only
// on a tick with nothing else to hit, and dropped for the first monster on
// the way. Measured on the test world over two and a half days (25
// September): the Orc Chief killed once, in fifty-three minutes, by
// twenty-three bots of whom exactly one was on him for eighteen of the
// thirty-four minutes he was fought, and not one casket in any bag
// ("sprawdzilem stan szkatulek - nie ma u mnie ani jednej po paru dniach
// gry", prodnathin; Tieru: the Giant Turtle, the Flame King, the Spider
// Queen, Nine Tails, the Tiger Spectre - every boss has to be killed).
//
// A raid is the Demon Tower's shape on open ground. This core's world pass
// looks for every boss of PLAYERBOT_WORLD_BOSSES on the maps it hosts and
// calls a raid to one standing with none: up to bSize bots of one kingdom in
// his level window, the ones already on his map first and then the
// strongest, a Shaman among them when the kingdom has one free. Each called
// bot's tick is this pass's: it comes to its own spot outside his sight,
// buffs, and when enough have come - or he has started on one of them - they
// all go at him with the tower's fight and the tower's keeping alive. A boss
// who has not lost half a percent of his health in
// PLAYERBOT_BOSS_RAID_STALL_MS gets a few more bots once, and after the
// second such minute is given up for PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS.
// After his fall the loot pass has its window and the raid disbands.
//
// Not a party: a party's rules (the cohort, the straggler radius, the level
// gap of six) are about camps, and a raid of eight from four maps would be
// broken up by them before it met. The engine pays experience by damage and
// the drop to the best damager, which is what a crowd wants anyway.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_demon_tower.h, whose fight it borrows.

namespace
{
	struct TPlayerBotWorldBoss
	{
		WORD wRace;
		long lMap;
		BYTE bMinLevel;
		BYTE bMaxLevel;
		BYTE bSize;
		// Whether his fall is news for the whole world (a notice). The Bestial
		// Captain falls three times an hour.
		bool bAnnounce;
	};

	// The bosses, read off the world's own tables (mob_proto, boss.txt and
	// special_spawns.txt, 25 September). The window runs from eight levels
	// under the boss to nine over him: past that the casket is no longer
	// certain (item_manager.cpp) and a bot refuses him for the experience.
	// The size aims at two and a half times what he heals, at the damage the
	// test world's bots of forty were measured dealing, scaled by level - an
	// estimate the stall rule corrects by calling more.
	const TPlayerBotWorldBoss PLAYERBOT_WORLD_BOSSES[] =
	{
		{  591,   3, 34, 51, 2, false },	// Bestial Captain (42), Jayang
		{  591,  23, 34, 51, 2, false },	// Bokjung
		{  591,  43, 34, 51, 2, false },	// Bakra
		{  691,  64, 46, 63, 4, true },	// Orc Chief (54), Orc Valley
		{ 2091, 104, 52, 69, 6, true },	// Spider Queen (60), Spider Dungeon
		{  791,  65, 52, 69, 6, true },	// Esoteric Leader (60), Hwang
		{  792,  65, 60, 77, 6, true },	// what his fall leaves standing (68)
		{ 2191,  63, 59, 76, 6, true },	// Giant Turtle (67), Yongbi Desert
		{ 1901,  61, 64, 81, 6, true },	// Nine Tails (72), Mount Sohan
		{ 2206,  62, 65, 82, 5, true },	// Flame King (73), Doyyumhwaji
		{ 1304,  65, 67, 84, 8, true },	// Yellow Tiger Spectre (75), Hwang
	};
	const size_t PLAYERBOT_WORLD_BOSS_COUNT = sizeof(PLAYERBOT_WORLD_BOSSES) / sizeof(PLAYERBOT_WORLD_BOSSES[0]);

	enum EPlayerBotBossRaidPhase
	{
		BOSS_RAID_PHASE_GATHER = 0,	// the members on their way to their spots
		BOSS_RAID_PHASE_FIGHT,		// everybody on him
		BOSS_RAID_PHASE_LOOT		// he is down; the loot pass has its window
	};

	struct TPlayerBotBossRaid
	{
		size_t row;
		BYTE bEmpire;
		BYTE bPhase;
		DWORD dwCalledAt;
		DWORD dwPhaseSince;
		DWORD dwBossVID;
		long lBossX;
		long lBossY;
		int iBestHP;
		DWORD dwLastProgress;
		bool bReinforced;
		std::set<DWORD> members;

		TPlayerBotBossRaid() :
			row(0), bEmpire(0), bPhase(BOSS_RAID_PHASE_GATHER), dwCalledAt(0), dwPhaseSince(0),
			dwBossVID(0), lBossX(0), lBossY(0), iBestHP(0), dwLastProgress(0), bReinforced(false) {}
	};

	typedef std::pair<long, WORD> TPlayerBotBossKey;
	typedef std::map<TPlayerBotBossKey, TPlayerBotBossRaid> TPlayerBotBossRaidMap;
	TPlayerBotBossRaidMap s_mapPlayerBotBossRaids;
	// When each boss may next be looked at: a raid over, a boss down, or
	// nobody free to call.
	std::map<TPlayerBotBossKey, DWORD> s_mapPlayerBotBossRaidNext;
	DWORD s_dwNextPlayerBotBossRaidCheck = 0;
	DWORD s_dwPlayerBotBossRaidsStartedAt = 0;
	DWORD s_dwNextPlayerBotBossRaidCensus = 0;
	unsigned int s_uPlayerBotBossRaidsFormed = 0;
	unsigned int s_uPlayerBotBossRaidsKilled = 0;
	unsigned int s_uPlayerBotBossRaidsOutpaced = 0;
	unsigned int s_uPlayerBotBossRaidsTooFew = 0;

	const char* GetPlayerBotWorldBossName(WORD wRace)
	{
		const CMob* mob = CMobManager::instance().Get(wRace);
		return mob ? mob->m_table.szLocaleName : "Boss";
	}

	// The boss on this map, or NULL. The whole map is asked, as
	// IsPlayerBotBossAlive asks it: a boss chases what hits him a long way
	// from where he stood.
	LPCHARACTER FindPlayerBotWorldBoss(long lMap, WORD wRace)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMap);
		if (!pMap)
			return NULL;
		FPlayerBotFindBoss finder(wRace);
		pMap->for_each(finder);
		return finder.m_found;
	}

	// The raid's boss by the VID it was called to, or NULL once he is dead or
	// gone - or the VID is somebody else's by now.
	LPCHARACTER GetPlayerBotBossRaidBoss(const TPlayerBotBossRaid& raid)
	{
		if (raid.dwBossVID == 0)
			return NULL;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(raid.dwBossVID);
		if (!boss || boss->IsDead() || !boss->IsMonster() || boss->GetRaceNum() != row.wRace ||
				boss->GetMapIndex() != row.lMap)
			return NULL;
		return boss;
	}

	TPlayerBotBossRaid* FindPlayerBotBossRaid(long lMap, WORD wRace)
	{
		TPlayerBotBossRaidMap::iterator it = s_mapPlayerBotBossRaids.find(TPlayerBotBossKey(lMap, wRace));
		return it == s_mapPlayerBotBossRaids.end() ? NULL : &it->second;
	}

	void ClearPlayerBotBossRaidState(TPlayerBotAIState& state, LPCHARACTER ch, DWORD dwBossVID)
	{
		state.wBossRaidRace = 0;
		state.lBossRaidMap = 0;
		state.dwNextBossRaidMoveTime = 0;
		if (dwBossVID != 0 && state.dwTargetVID == dwBossVID)
			state.dwTargetVID = 0;
		if (ch && dwBossVID != 0 && ch->GetVictim() && (DWORD)ch->GetVictim()->GetVID() == dwBossVID)
			ch->SetVictim(NULL);
	}

	// A bot's own spot by the boss: outside his sight (2000 for every boss of
	// the table), by pid round him, on open ground his own ground joins - a
	// spot across a river from him is a spot the bot stands on for good.
	void GetPlayerBotBossRaidRally(DWORD pid, const TPlayerBotBossRaid& raid, long& outX, long& outY)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(row.lMap);
		const bool nav = navigation.Init(row.lMap);
		const DWORD bossGround = nav ? navigation.GetComponentAtWorld(raid.lBossX, raid.lBossY, 12) : 0;
		const long radius = PLAYERBOT_BOSS_RAID_RALLY_MIN +
				(long)(PlayerBotNavHash(pid ^ 0x42524C5AU) % (DWORD)PLAYERBOT_BOSS_RAID_RALLY_SPREAD);
		const DWORD firstAngle = PlayerBotNavHash(pid ^ 0x42524C59U) % 360U;
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			const double rad = (double)((firstAngle + (DWORD)attempt * 45U) % 360U) * 3.14159265 / 180.0;
			long x = raid.lBossX + (long)(cos(rad) * radius);
			long y = raid.lBossY + (long)(sin(rad) * radius);
			long openX = 0, openY = 0;
			if (FindPlayerBotWarGround(row.lMap, x, y, 800, openX, openY))
			{
				x = openX;
				y = openY;
			}
			if (!nav || bossGround == 0 || navigation.GetComponentAtWorld(x, y, 4) == bossGround)
			{
				outX = x;
				outY = y;
				return;
			}
		}
		outX = raid.lBossX;
		outY = raid.lBossY;
	}

	// Why this bot cannot be called to this boss now, or NULL when it can.
	const char* GetPlayerBotBossRaidRefusal(LPCHARACTER c, const TPlayerBotAIState& st,
			const TPlayerBotWorldBoss& row, DWORD dwNow)
	{
		if (!c || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot())
			return "gone";
		const int level = (int)c->GetLevel();
		if (level < (int)row.bMinLevel || level > (int)row.bMaxLevel)
			return "level";
		if (st.wBossRaidRace != 0)
			return "raiding";
		const DWORD pid = c->GetPlayerID();
		if (IsPlayerBotSidekickPID(pid) || IsPlayerBotSummoned(pid) || IsPlayerBotOnMercContract(pid) ||
				IsPlayerBotHeldForCompany(c))
			return "company";
		if (c->GetParty() && IsPlayerBotHumanLedParty(c->GetParty()))
			return "person";
		if (IsPlayerBotDropper(st.bPersonality))
			return "dropper";
		if (st.bFishingSession || IsPlayerBotAngler(c, st) || IsPlayerBotMiner(c, st) ||
				IsPlayerBotMiningNow(pid, dwNow))
			return "tool";
		if (IsPlayerBotOnTowerBusiness(c, st) || st.dwGuildWarEnemyGID != 0 ||
				playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0)
			return "busy";
		if (IsPlayerBotOnBattleHorseTrial(c) || IsPlayerBotOnMilitaryHorseTrial(c))
			return "trial";
		if (st.bTownVisitPhase != BOT_TOWN_PHASE_NONE || c->GetMyShop())
			return "town";
		if (c->GetSkillGroup() == 0)
			return "no_skills";
		const long map = c->GetMapIndex();
		if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || map == PLAYERBOT_MAP_DEMON_TOWER ||
				IsPlayerBotDemonTowerInstance(map))
			return "dungeon";
		// The Spider Dungeon is reached across the desert on foot
		// (TransitionPlayerBotMap), which takes longer than a gathering lasts:
		// its queen is for the bots already down there.
		if (row.lMap == PLAYERBOT_MAP_SPIDER_V1 && map != row.lMap)
			return "far";
		if (st.bRecoveringAfterDeath || c->GetMaxHP() <= 0 ||
				(long long)c->GetHP() * 100 < (long long)c->GetMaxHP() * PLAYERBOT_BOSS_RAID_MIN_HP_PERCENT)
			return "health";
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(c, red, blue);
		const bool caster = c->GetJob() == JOB_SHAMAN || (c->GetJob() == JOB_SURA && c->GetSkillGroup() == 2);
		if (red < PLAYERBOT_BOSS_RAID_MIN_RED_POTIONS || (caster && blue < PLAYERBOT_BOSS_RAID_MIN_BLUE_POTIONS))
			return "potions";
		return NULL;
	}

	struct TPlayerBotBossRecruit
	{
		DWORD pid;
		BYTE empire;
		bool onMap;
		bool shaman;
		int strength;
	};

	bool PlayerBotBossRecruitOrder(const TPlayerBotBossRecruit& a, const TPlayerBotBossRecruit& b)
	{
		if (a.onMap != b.onMap)
			return a.onMap;
		if (a.strength != b.strength)
			return a.strength > b.strength;
		return a.pid < b.pid;
	}

	// Every bot of this core that could be called to this boss now - of one
	// kingdom, or of any when empire is zero. inBand counts the bots of his
	// level window before the rest of the refusals, for the log.
	void CollectPlayerBotBossRecruits(const TPlayerBotWorldBoss& row, LPCHARACTER boss, BYTE empire,
			DWORD dwNow, std::vector<TPlayerBotBossRecruit>& out, int& inBand)
	{
		out.clear();
		inBand = 0;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(row.lMap);
		const bool nav = navigation.Init(row.lMap);
		const DWORD bossGround = (nav && boss) ? navigation.GetComponentAtWorld(boss->GetX(), boss->GetY(), 12) : 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || (empire != 0 && c->GetEmpire() != empire))
				continue;
			if ((int)c->GetLevel() >= (int)row.bMinLevel && (int)c->GetLevel() <= (int)row.bMaxLevel)
				++inBand;
			if (GetPlayerBotBossRaidRefusal(c, it->second, row, dwNow))
				continue;
			TPlayerBotBossRecruit r;
			r.pid = it->first;
			r.empire = c->GetEmpire();
			r.onMap = c->GetMapIndex() == row.lMap;
			// On his map, but on ground his own ground does not join: an island
			// the bot cannot leave on foot.
			if (r.onMap && bossGround != 0 && navigation.GetComponentAtWorld(c->GetX(), c->GetY()) != bossGround)
				continue;
			r.shaman = c->GetJob() == JOB_SHAMAN;
			r.strength = GetPlayerBotStrengthCached(it->first);
			if (r.strength <= 0)
				r.strength = (int)c->GetLevel() * 1000;
			out.push_back(r);
		}
		std::sort(out.begin(), out.end(), PlayerBotBossRecruitOrder);
	}

	void EnlistPlayerBotBossRaider(TPlayerBotBossRaid& raid, DWORD pid, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		raid.members.insert(pid);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
		if (st == s_mapPlayerBotAIStates.end())
			return;
		st->second.wBossRaidRace = row.wRace;
		st->second.lBossRaidMap = row.lMap;
		// The departures spread over the first twenty seconds by pid.
		st->second.dwNextBossRaidMoveTime = dwNow + PlayerBotNavHash(pid ^ 0x42535452U) % 20000U;
	}

	// A boss standing with no raid on him: the kingdom with the most bots free
	// for him (his own map's, for the Captain of a second village), its bots
	// on his map first and then the strongest, a Shaman in when there is one.
	bool CallPlayerBotBossRaid(size_t rowIndex, LPCHARACTER boss, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[rowIndex];
		const int owner = playerbot_empire_rules::GetMapOwnerEmpire(row.lMap);
		std::vector<TPlayerBotBossRecruit> pool;
		int inBand = 0;
		CollectPlayerBotBossRecruits(row, boss, (BYTE)std::max(0, owner), dwNow, pool, inBand);
		int perEmpire[4] = { 0, 0, 0, 0 };
		for (size_t i = 0; i < pool.size(); ++i)
			if (pool[i].empire >= 1 && pool[i].empire <= 3)
				++perEmpire[pool[i].empire];
		BYTE empire = (BYTE)std::max(0, owner);
		if (empire == 0)
		{
			// Ties go round by the hour, so one kingdom does not take every
			// boss it ties for.
			int best = 0;
			const DWORD turn = (DWORD)row.wRace + dwNow / 3600000U;
			for (DWORD k = 0; k < 3; ++k)
			{
				const int e = 1 + (int)((turn + k) % 3U);
				if (perEmpire[e] > best)
				{
					best = perEmpire[e];
					empire = (BYTE)e;
				}
			}
		}
		std::vector<TPlayerBotBossRecruit> chosen;
		const TPlayerBotBossRecruit* shaman = NULL;
		for (size_t i = 0; i < pool.size(); ++i)
		{
			if (pool[i].empire != empire)
				continue;
			if (chosen.size() < (size_t)row.bSize)
				chosen.push_back(pool[i]);
			else if (!shaman && pool[i].shaman)
				shaman = &pool[i];
		}
		// A crowd with no Shaman takes the first one left over in place of its
		// last member: its buffs are worth more than one more blade.
		if (shaman && chosen.size() > 1)
		{
			bool has = false;
			for (size_t i = 0; i < chosen.size(); ++i)
				if (chosen[i].shaman)
					has = true;
			if (!has)
				chosen.back() = *shaman;
		}
		const int need = std::max(2, ((int)row.bSize + 1) / 2);
		if ((int)chosen.size() < need)
		{
			// Once in ten minutes a boss, each boss on its own: one throttle for
			// all of them showed the first boss of the minute and hid the rest.
			static std::map<size_t, DWORD> s_mapLogged;
			std::map<size_t, DWORD>::const_iterator logged = s_mapLogged.find(rowIndex);
			if (logged == s_mapLogged.end() || dwNow - logged->second >= PLAYERBOT_BOSS_RAID_CENSUS_MS)
			{
				s_mapLogged[rowIndex] = dwNow;
				sys_log(0, "PLAYERBOT_RAID: nobody to call boss=%s race=%u map=%ld in_band=%d free=%d/%d/%d need=%d",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, inBand,
						perEmpire[1], perEmpire[2], perEmpire[3], need);
			}
			return false;
		}
		TPlayerBotBossRaid& raid = s_mapPlayerBotBossRaids[TPlayerBotBossKey(row.lMap, row.wRace)];
		raid = TPlayerBotBossRaid();
		raid.row = rowIndex;
		raid.bEmpire = empire;
		raid.bPhase = BOSS_RAID_PHASE_GATHER;
		raid.dwCalledAt = dwNow;
		raid.dwPhaseSince = dwNow;
		raid.dwBossVID = (DWORD)boss->GetVID();
		raid.lBossX = boss->GetX();
		raid.lBossY = boss->GetY();
		raid.iBestHP = boss->GetHP();
		raid.dwLastProgress = dwNow;
		int onMap = 0;
		bool withShaman = false;
		for (size_t i = 0; i < chosen.size(); ++i)
		{
			EnlistPlayerBotBossRaider(raid, chosen[i].pid, dwNow);
			if (chosen[i].onMap)
				++onMap;
			if (chosen[i].shaman)
				withShaman = true;
		}
		++s_uPlayerBotBossRaidsFormed;
		sys_log(0, "PLAYERBOT_RAID: formed boss=%s race=%u map=%ld pos=(%ld,%ld) empire=%u members=%u on_map=%d shaman=%d need=%d in_band=%d",
				GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, raid.lBossX, raid.lBossY,
				(unsigned int)empire, (unsigned int)raid.members.size(), onMap, withShaman ? 1 : 0, need, inBand);
		return true;
	}

	// A raid the boss is outpacing takes a few more of its kingdom once.
	int ReinforcePlayerBotBossRaid(TPlayerBotBossRaid& raid, LPCHARACTER boss, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		std::vector<TPlayerBotBossRecruit> pool;
		int inBand = 0;
		CollectPlayerBotBossRecruits(row, boss, raid.bEmpire, dwNow, pool, inBand);
		int added = 0;
		for (size_t i = 0; i < pool.size() && added < PLAYERBOT_BOSS_RAID_REINFORCEMENTS; ++i)
		{
			EnlistPlayerBotBossRaider(raid, pool[i].pid, dwNow);
			++added;
		}
		return added;
	}

	void EndPlayerBotBossRaid(TPlayerBotBossRaidMap::iterator it, DWORD dwNow, const char* why, DWORD cooldown)
	{
		TPlayerBotBossRaid& raid = it->second;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
		{
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(*m);
			if (st == s_mapPlayerBotAIStates.end() || st->second.wBossRaidRace != row.wRace ||
					st->second.lBossRaidMap != row.lMap)
				continue;
			ClearPlayerBotBossRaidState(st->second, CHARACTER_MANAGER::instance().FindByPID(*m), raid.dwBossVID);
		}
		s_mapPlayerBotBossRaidNext[it->first] = dwNow + cooldown;
		sys_log(0, "PLAYERBOT_RAID: over boss=%s race=%u map=%ld empire=%u phase=%d members=%u after_s=%u why=%s",
				GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, (unsigned int)raid.bEmpire,
				(int)raid.bPhase, (unsigned int)raid.members.size(), (dwNow - raid.dwCalledAt) / 1000U, why);
		s_mapPlayerBotBossRaids.erase(it);
	}

	// One raid moved along; false when it has ended (and been erased).
	bool AdvancePlayerBotBossRaid(TPlayerBotBossRaidMap::iterator it, DWORD dwNow)
	{
		TPlayerBotBossRaid& raid = it->second;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		// Members gone from the world, or taken off the raid by their own pass.
		for (std::set<DWORD>::iterator m = raid.members.begin(); m != raid.members.end();)
		{
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(*m);
			if (!CHARACTER_MANAGER::instance().FindByPID(*m) || st == s_mapPlayerBotAIStates.end() ||
					st->second.wBossRaidRace != row.wRace || st->second.lBossRaidMap != row.lMap)
				raid.members.erase(m++);
			else
				++m;
		}
		LPCHARACTER boss = GetPlayerBotBossRaidBoss(raid);
		if (!boss)
		{
			if (raid.bPhase == BOSS_RAID_PHASE_GATHER)
			{
				// Down before the raid got to him: somebody else's kill.
				EndPlayerBotBossRaid(it, dwNow, "boss_gone", PLAYERBOT_BOSS_RAID_KILLED_COOLDOWN_MS);
				return false;
			}
			if (raid.bPhase == BOSS_RAID_PHASE_FIGHT)
			{
				raid.bPhase = BOSS_RAID_PHASE_LOOT;
				raid.dwPhaseSince = dwNow;
				++s_uPlayerBotBossRaidsKilled;
				const unsigned int minutes = (dwNow - raid.dwCalledAt) / 60000U;
				sys_log(0, "PLAYERBOT_RAID: killed boss=%s race=%u map=%ld empire=%u members=%u after_s=%u reinforced=%d",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
						(unsigned int)raid.bEmpire, (unsigned int)raid.members.size(),
						(dwNow - raid.dwCalledAt) / 1000U, raid.bReinforced ? 1 : 0);
				if (row.bAnnounce)
				{
					char notice[200];
					snprintf(notice, sizeof(notice), "Boty z krolestwa %s pokonaly: %s (%u min).",
							GetPlayerBotKingdomName(raid.bEmpire), GetPlayerBotWorldBossName(row.wRace),
							std::max(1U, minutes));
					BroadcastNotice(notice);
				}
				return true;
			}
			if (dwNow - raid.dwPhaseSince >= PLAYERBOT_BOSS_RAID_LOOT_MS)
			{
				EndPlayerBotBossRaid(it, dwNow, "killed", PLAYERBOT_BOSS_RAID_KILLED_COOLDOWN_MS);
				return false;
			}
			return true;
		}
		raid.lBossX = boss->GetX();
		raid.lBossY = boss->GetY();
		if (raid.members.empty())
		{
			EndPlayerBotBossRaid(it, dwNow, "no_members", PLAYERBOT_BOSS_RAID_TOO_FEW_COOLDOWN_MS);
			return false;
		}
		if (raid.bPhase == BOSS_RAID_PHASE_GATHER)
		{
			int arrived = 0;
			bool started = false;
			LPCHARACTER victim = boss->GetVictim();
			for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
			{
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
				if (!c || c->IsDead() || c->GetMapIndex() != row.lMap)
					continue;
				if (c == victim)
					started = true;
				if (DISTANCE_APPROX(c->GetX() - raid.lBossX, c->GetY() - raid.lBossY) <= PLAYERBOT_BOSS_RAID_ARRIVED_RANGE)
					++arrived;
			}
			const int need = std::max(2, ((int)row.bSize + 1) / 2);
			const bool timeUp = dwNow - raid.dwPhaseSince >= PLAYERBOT_BOSS_RAID_GATHER_MS;
			if (arrived >= (int)raid.members.size() || arrived >= (int)row.bSize || started ||
					(timeUp && arrived >= need))
			{
				raid.bPhase = BOSS_RAID_PHASE_FIGHT;
				raid.dwPhaseSince = dwNow;
				raid.iBestHP = boss->GetHP();
				raid.dwLastProgress = dwNow;
				sys_log(0, "PLAYERBOT_RAID: engaged boss=%s race=%u map=%ld arrived=%d of %u started_by_him=%d after_s=%u",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, arrived,
						(unsigned int)raid.members.size(), started ? 1 : 0, (dwNow - raid.dwCalledAt) / 1000U);
			}
			else if (timeUp)
			{
				++s_uPlayerBotBossRaidsTooFew;
				EndPlayerBotBossRaid(it, dwNow, "too_few_came", PLAYERBOT_BOSS_RAID_TOO_FEW_COOLDOWN_MS);
				return false;
			}
			return true;
		}
		if (raid.bPhase == BOSS_RAID_PHASE_FIGHT)
		{
			const int meaningful = std::max(1, boss->GetMaxHP() / 200);
			if (boss->GetHP() + meaningful <= raid.iBestHP)
			{
				raid.iBestHP = boss->GetHP();
				raid.dwLastProgress = dwNow;
			}
			if (dwNow - raid.dwLastProgress >= PLAYERBOT_BOSS_RAID_STALL_MS)
			{
				if (!raid.bReinforced)
				{
					raid.bReinforced = true;
					raid.dwLastProgress = dwNow;
					const int added = ReinforcePlayerBotBossRaid(raid, boss, dwNow);
					sys_log(0, "PLAYERBOT_RAID: outpaced, calling more boss=%s race=%u map=%ld hp=%d/%d added=%d members=%u",
							GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
							boss->GetHP(), boss->GetMaxHP(), added, (unsigned int)raid.members.size());
				}
				else
				{
					++s_uPlayerBotBossRaidsOutpaced;
					sys_log(0, "PLAYERBOT_RAID: outpaced, giving up boss=%s race=%u map=%ld hp=%d/%d best_hp=%d members=%u",
							GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
							boss->GetHP(), boss->GetMaxHP(), raid.iBestHP, (unsigned int)raid.members.size());
					EndPlayerBotBossRaid(it, dwNow, "outpaced", PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS);
					return false;
				}
			}
			else if (dwNow - raid.dwPhaseSince >= PLAYERBOT_BOSS_RAID_FIGHT_MAX_MS)
			{
				EndPlayerBotBossRaid(it, dwNow, "fight_too_long", PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS);
				return false;
			}
		}
		return true;
	}

	// The world's pass: the raids under way moved along, and a raid called to
	// every boss standing with none.
	void ManagePlayerBotBossRaids(DWORD dwNow)
	{
		if (s_dwPlayerBotBossRaidsStartedAt == 0)
			s_dwPlayerBotBossRaidsStartedAt = dwNow;
		if (dwNow < s_dwNextPlayerBotBossRaidCheck)
			return;
		s_dwNextPlayerBotBossRaidCheck = dwNow + PLAYERBOT_BOSS_RAID_CHECK_MS;
		// The cohort spawns over the first minutes, and a raid called then
		// takes whoever happened to be first.
		if (dwNow - s_dwPlayerBotBossRaidsStartedAt < PLAYERBOT_BOSS_RAID_FIRST_DELAY_MS)
			return;

		for (TPlayerBotBossRaidMap::iterator it = s_mapPlayerBotBossRaids.begin(); it != s_mapPlayerBotBossRaids.end();)
		{
			TPlayerBotBossRaidMap::iterator current = it++;
			AdvancePlayerBotBossRaid(current, dwNow);
		}

		for (size_t i = 0; i < PLAYERBOT_WORLD_BOSS_COUNT; ++i)
		{
			const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[i];
			const TPlayerBotBossKey key(row.lMap, row.wRace);
			if (s_mapPlayerBotBossRaids.find(key) != s_mapPlayerBotBossRaids.end())
				continue;
			std::map<TPlayerBotBossKey, DWORD>::const_iterator next = s_mapPlayerBotBossRaidNext.find(key);
			if (next != s_mapPlayerBotBossRaidNext.end() && dwNow < next->second)
				continue;
			if (!IsPlayerBotMapHostedHere(row.lMap))
			{
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_CENSUS_MS;
				continue;
			}
			LPCHARACTER boss = FindPlayerBotWorldBoss(row.lMap, row.wRace);
			if (!boss)
			{
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_DOWN_RECHECK_MS;
				continue;
			}
			if (!CallPlayerBotBossRaid(i, boss, dwNow))
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_CALL_RETRY_MS;
		}

		if (s_dwNextPlayerBotBossRaidCensus == 0 || dwNow >= s_dwNextPlayerBotBossRaidCensus)
		{
			s_dwNextPlayerBotBossRaidCensus = dwNow + PLAYERBOT_BOSS_RAID_CENSUS_MS;
			sys_log(0, "PLAYERBOT_RAID: census formed=%u killed=%u outpaced=%u too_few=%u active=%u",
					s_uPlayerBotBossRaidsFormed, s_uPlayerBotBossRaidsKilled, s_uPlayerBotBossRaidsOutpaced,
					s_uPlayerBotBossRaidsTooFew, (unsigned int)s_mapPlayerBotBossRaids.size());
		}
	}

	// The per-bot pass: a bot called to a boss owns its tick until the raid
	// is over. After the loot pass, so the fall's drops are picked up, and
	// beside the tower's hook, whose fight and keeping alive it borrows.
	bool ManagePlayerBotBossRaid(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (state.wBossRaidRace == 0 || !ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotBossRaid* raid = FindPlayerBotBossRaid(state.lBossRaidMap, state.wBossRaidRace);
		if (!raid || raid->members.find(pid) == raid->members.end())
		{
			ClearPlayerBotBossRaidState(state, ch, 0);
			return false;
		}
		// A person's party is the person's: the bot leaves the raid for it.
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
		{
			raid->members.erase(pid);
			ClearPlayerBotBossRaidState(state, ch, raid->dwBossVID);
			sys_log(0, "PLAYERBOT_RAID: member left for a person's party pid=%u name=%s", pid, ch->GetName());
			return false;
		}
		// His fall: the loot pass above this hook has its window, and the
		// rest of the tick is the bot's own again until the raid disbands.
		if (raid->bPhase == BOSS_RAID_PHASE_LOOT)
			return false;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid->row];
		if (ch->GetMapIndex() != row.lMap)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextBossRaidMoveTime)
				return true;
			state.dwNextBossRaidMoveTime = dwNow + 5000;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, row.lMap, rallyX, rallyY, dwNow, "boss_raid");
			return true;
		}
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		LPCHARACTER boss = GetPlayerBotBossRaidBoss(*raid);
		if (!boss)
			return false;
		const int distance = DISTANCE_APPROX(ch->GetX() - boss->GetX(), ch->GetY() - boss->GetY());
		// From the far end of his own map the walk would outlast the gathering
		// and come after the fight: brought to its spot, as a member from
		// another map is.
		if (distance > PLAYERBOT_BOSS_RAID_WALK_MAX)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextBossRaidMoveTime)
				return true;
			state.dwNextBossRaidMoveTime = dwNow + 5000;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, row.lMap, rallyX, rallyY, dwNow, "boss_raid_far");
			return true;
		}
		LPCHARACTER bossVictim = boss->GetVictim();
		const bool bossStarted = bossVictim && bossVictim->IsPC() &&
				raid->members.find(bossVictim->GetPlayerID()) != raid->members.end();
		if (raid->bPhase == BOSS_RAID_PHASE_FIGHT || bossStarted)
		{
			if (BuffPlayerBotTowerFellows(ch, state, dwNow))
				return true;
			// What hits the bot on its way to him is answered on the way; at
			// him, he is the fight.
			if (distance > PLAYERBOT_BOSS_RAID_RALLY_MIN)
			{
				LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
				if (engaged && engaged != boss)
					return FightPlayerBotTowerObjective(ch, state, engaged, dwNow);
			}
			return FightPlayerBotTowerObjective(ch, state, boss, dwNow);
		}
		// Gathering: what attacks the bot is fought where it comes, the
		// Shaman buffs whoever has come, and the rest is the walk to its spot
		// and its own buffs there.
		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
		if (engaged && engaged != boss)
			return FightPlayerBotTowerObjective(ch, state, engaged, dwNow);
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		long rallyX = 0, rallyY = 0;
		GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
		const int toRally = DISTANCE_APPROX(ch->GetX() - rallyX, ch->GetY() - rallyY);
		if (toRally <= 600)
		{
			// Buffs are cast on foot; a battle horse's rider is taken down by
			// the buff pass itself when one is missing.
			if (ch->IsRiding() && !HasPlayerBotBattleHorse(ch))
			{
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "boss_raid");
				return true;
			}
			if (ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
				return true;
		}
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (toRally > 400 && dwNow >= state.dwNextBossRaidMoveTime)
		{
			state.dwNextBossRaidMoveTime = dwNow + 2000;
			MovePlayerBot(ch, rallyX, rallyY, dwNow, 8, true, toRally > 4000);
		}
		return true;
	}
}

#endif
