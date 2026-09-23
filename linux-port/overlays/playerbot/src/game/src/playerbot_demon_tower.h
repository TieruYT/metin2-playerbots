#ifndef __INC_METIN2_PLAYERBOT_DEMON_TOWER_H__
#define __INC_METIN2_PLAYERBOT_DEMON_TOWER_H__

// The Demon Tower, climbed by a guild.
//
// The tower (metin2_map_deviltower1, map 66) is the package's own
// deviltower_zone quest: a public ground floor with the Metin of Toughness
// (8015), and eight floors inside a private instance that the quest drives -
// every PC on the killer's map is jumped into a new instance when the stone
// breaks, the floors advance on their own triggers (everything dead, a boss
// down, the right stone broken, a key handed to a seal, a map used, a key
// given to Sa-Soe), and d.exit_all sends everybody back to the ground floor.
// A bot could not follow any of that: CHARACTER::WarpSet takes a character
// off its sectree and waits for its client to reconnect, and a bot has no
// client - nine of them were jumped to 660000 on 14 September and put back on
// the ground floor by the sectree rescue. Since 2.0.61 WarpSet moves a bot
// server-side, through CPlayerBotManager::WarpBot (playerbotify.py), so a
// jump, an exit and a quest's pc.warp all reach it; this fragment is what the
// bots do once they are there.
//
// A raid is one bot guild at a time on this core: the master calls the guild
// (CGuild::Chat, so a player in that guild reads it too), the strongest
// members of PLAYERBOT_TOWER_MIN_LEVEL and up gather on the ground floor by
// the stone, and after PLAYERBOT_TOWER_GATHER_MS they break it together.
// Whoever else stands on the ground floor at that moment - a bot on its
// Biologist errand, a player who came to watch - is jumped along by the
// engine, exactly as the operator asked ("osoby postronne ktore sa akurat w
// wiezy demonow tez dolaczyli"), and inside the tower every bot fights by
// the floor's rules whether it was called or not. A player's guild works the
// other way round: its bots come to the ground floor when their human master
// stands there, and go in when the player breaks the stone.
//
// Inside, the pass owns the tick the way the guild war does: the fight is
// the duel's shape against the nearest thing that has to die on the floor,
// the keys are picked up by the ordinary loot pass and used or handed in
// here, and the smith on the sixth floor - whose "go on" is a dialog with a
// select a bot cannot press - is passed the way devil_jump_7 in the quest
// passes him, by a bot of PLAYERBOT_TOWER_UPPER_LEVEL, because that is the
// game's own rule for the last three floors. A run that makes no progress
// for PLAYERBOT_TOWER_STALL_MS leaves.
//
// Read before believing the quest: on this package the fifth floor counts
// kills of 1062 (Brutalny Demon Lucznik) and its regen spawns 1002-1004 and
// 1031-1034, never 1062 - so nobody, player or bot, could pass it. The
// shipped copy of deviltower_zone.quest counts the floor's own demons.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it exactly once, after playerbot_guild_war.h (the fight is the
// duel's and the kingdom names are that file's) and before playerbot_lure.h.

namespace
{
	// ------------------------------------------------------------ the raid

	enum EPlayerBotTowerPhase
	{
		TOWER_PHASE_NONE = 0,
		TOWER_PHASE_GATHER,	// members on their way to the ground floor
		TOWER_PHASE_STONE,	// breaking the Metin of Toughness
		TOWER_PHASE_INSIDE	// the instance exists; the floors are the run's
	};

	struct TPlayerBotTowerRaid
	{
		DWORD dwGuildID;
		BYTE bEmpire;
		BYTE bPhase;
		DWORD dwCalledAt;
		DWORD dwPhaseSince;
		long lInstance;
		int iUpper;
		std::set<DWORD> members;

		TPlayerBotTowerRaid() :
			dwGuildID(0), bEmpire(0), bPhase(TOWER_PHASE_NONE), dwCalledAt(0),
			dwPhaseSince(0), lInstance(0), iUpper(0) {}
	};

	TPlayerBotTowerRaid s_PlayerBotTowerRaid;
	DWORD s_dwNextPlayerBotTowerCheck = 0;
	DWORD s_dwNextPlayerBotTowerRaidTime = 0;
	DWORD s_dwNextPlayerBotTowerCensus = 0;
	unsigned int s_uPlayerBotTowerRaids = 0;
	unsigned int s_uPlayerBotTowerRunsFinished = 0;
	int s_iPlayerBotTowerBestFloor = 0;
	time_t s_tPlayerBotTowerNowSeen = (time_t)-1;

	// One run of the tower: the instance, its floor, the progress clock.
	struct TPlayerBotTowerRun
	{
		int iLevel;
		int iAlive;
		DWORD dwEnteredAt;
		DWORD dwLevelSince;
		DWORD dwLastProgress;
		DWORD dwSmithSince;
		bool bSmithDone;
		bool bEnding;
		const char* pszEnd;
		DWORD dwNextReport;
		// The bots that have had their turn at the sixth floor's smith.
		std::set<DWORD> smithServed;

		TPlayerBotTowerRun() :
			iLevel(-1), iAlive(-1), dwEnteredAt(0), dwLevelSince(0), dwLastProgress(0),
			dwSmithSince(0), bSmithDone(false), bEnding(false), pszEnd(""), dwNextReport(0) {}
	};
	std::map<long, TPlayerBotTowerRun> s_mapPlayerBotTowerRuns;

	// Where each floor's jump lands, in cells off the map's base
	// (deviltower_zone.quest, level_coords): the floor a bot is on is the
	// room round that point, and what stands in another room - a floor's
	// leftovers, which clear_regen does not kill - is not this floor's.
	const long PLAYERBOT_TOWER_FLOOR_CELLS[8][2] = {
		{126, 384}, {134, 147}, {369, 629}, {369, 401}, {374, 167}, {621, 631}, {616, 399}, {591, 159},
	};
	const int PLAYERBOT_TOWER_FLOOR_RADIUS = 16000;

	bool GetPlayerBotTowerFloorCentre(int level, long& outX, long& outY)
	{
		if (level < 0 || level >= 8)
			return false;
		outX = (PLAYERBOT_TOWER_BASE_CELL_X + PLAYERBOT_TOWER_FLOOR_CELLS[level][0]) * 100;
		outY = (PLAYERBOT_TOWER_BASE_CELL_Y + PLAYERBOT_TOWER_FLOOR_CELLS[level][1]) * 100;
		return true;
	}

	bool IsPlayerBotGuildRaidingTower(DWORD dwGuildID)
	{
		return dwGuildID != 0 && s_PlayerBotTowerRaid.bPhase != TOWER_PHASE_NONE &&
				s_PlayerBotTowerRaid.dwGuildID == dwGuildID;
	}

	// ------------------------------------------------------------ the scan
	//
	// What stands on a tower map, read once per PLAYERBOT_TOWER_SCAN_INTERVAL
	// for everybody on it: the floors are small and the search range of the
	// ordinary target collector is not the floor.

	struct TPlayerBotTowerEntity
	{
		DWORD vid;
		long x;
		long y;
		DWORD race;
		bool stone;
		bool npc;
		bool boss;	// a monster of MOB_RANK_BOSS and up: the pack's, like a stone
	};

	struct TPlayerBotTowerScan
	{
		DWORD dwScannedAt;
		std::vector<TPlayerBotTowerEntity> entities;
		int alive;	// monsters and stones
		int monsters;	// the monsters alone
		int upperBots;	// bots of PLAYERBOT_TOWER_UPPER_LEVEL and up
		// The pack: where the live bots on the map stand, on average. The
		// objective is chosen from here, not from each bot, so the sixteen
		// fight the same few demons in one place instead of sixteen different
		// ones across the floor - spread over the seventh floor they died 253
		// times in eight minutes.
		long packX;
		long packY;
		int packN;

		TPlayerBotTowerScan() : dwScannedAt(0), alive(0), monsters(0), upperBots(0), packX(0), packY(0), packN(0) {}
	};
	std::map<long, TPlayerBotTowerScan> s_mapPlayerBotTowerScans;

	struct FPlayerBotTowerCollect
	{
		TPlayerBotTowerScan& m_scan;
		FPlayerBotTowerCollect(TPlayerBotTowerScan& scan) : m_scan(scan) {}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsPC())
			{
				if (!c->IsDead() && c->GetDesc() && c->GetDesc()->IsBot())
				{
					if (c->GetLevel() >= PLAYERBOT_TOWER_UPPER_LEVEL)
						++m_scan.upperBots;
					m_scan.packX += c->GetX();
					m_scan.packY += c->GetY();
					++m_scan.packN;
				}
				return;
			}
			if (c->IsDead())
				return;
			TPlayerBotTowerEntity e;
			e.vid = (DWORD)c->GetVID();
			e.x = c->GetX();
			e.y = c->GetY();
			e.race = c->GetRaceNum();
			e.stone = c->IsStone();
			e.npc = !e.stone && !c->IsMonster();
			e.boss = !e.stone && !e.npc && c->GetMobRank() >= MOB_RANK_BOSS;
			if (!e.npc)
			{
				++m_scan.alive;
				if (!e.stone)
					++m_scan.monsters;
			}
			m_scan.entities.push_back(e);
		}
	};

	const TPlayerBotTowerScan* ScanPlayerBotTowerMap(long lMapIndex, DWORD dwNow)
	{
		TPlayerBotTowerScan& scan = s_mapPlayerBotTowerScans[lMapIndex];
		if (scan.dwScannedAt != 0 && dwNow - scan.dwScannedAt < PLAYERBOT_TOWER_SCAN_INTERVAL)
			return &scan;
		scan.dwScannedAt = dwNow;
		scan.entities.clear();
		scan.alive = 0;
		scan.monsters = 0;
		scan.upperBots = 0;
		scan.packX = 0;
		scan.packY = 0;
		scan.packN = 0;
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		if (!pMap)
			return &scan;
		FPlayerBotTowerCollect f(scan);
		pMap->for_each(f);
		if (scan.packN > 0)
		{
			scan.packX /= scan.packN;
			scan.packY /= scan.packN;
		}
		return &scan;
	}

	LPCHARACTER FindPlayerBotTowerNpc(const TPlayerBotTowerScan* scan, DWORD raceFirst, DWORD raceLast,
			long x, long y)
	{
		DWORD bestVid = 0;
		int bestDistance = INT_MAX;
		for (size_t i = 0; scan && i < scan->entities.size(); ++i)
		{
			const TPlayerBotTowerEntity& e = scan->entities[i];
			if (!e.npc || e.race < raceFirst || e.race > raceLast)
				continue;
			const int distance = DISTANCE_APPROX(x - e.x, y - e.y);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestVid = e.vid;
			}
		}
		return bestVid ? CHARACTER_MANAGER::instance().Find(bestVid) : NULL;
	}

	// Monsters standing within radius of a point: a stone with none about it
	// is broken without being surrounded.
	int CountPlayerBotTowerMonstersNear(const TPlayerBotTowerScan* scan, long x, long y, int radius)
	{
		int n = 0;
		for (size_t i = 0; scan && i < scan->entities.size(); ++i)
		{
			const TPlayerBotTowerEntity& e = scan->entities[i];
			if (!e.npc && !e.stone && DISTANCE_APPROX(x - e.x, y - e.y) <= radius)
				++n;
		}
		return n;
	}

	// The floor's objective for this bot: the nearest thing that has to die
	// on it. On the ground floor only the Metin of Toughness; inside, that
	// stone never (breaking it there does nothing), the fourth and seventh
	// floors' stones ahead of their monsters because the floor turns on them,
	// the Metin of the Devil (8016) ahead of the seven it spawns. A stone, or a
	// boss once he is the nearest, is the whole pack's; an ordinary monster
	// is spread over the pack instead (PLAYERBOT_TOWER_BOTS_PER_MONSTER):
	// each bot takes one of the few standing nearest the pack by a slot drawn
	// from its pid, and keeps it while it stands, so the pack stays in one
	// place and fights several monsters at once rather than queueing on one.
	LPCHARACTER PickPlayerBotTowerObjective(LPCHARACTER ch, const TPlayerBotTowerScan* scan, int level,
			bool parterStone, int maxDistance)
	{
		DWORD bestVid = 0;
		int bestScore = INT_MIN;
		bool bestShared = false;
		// The ordinary monsters that passed every test below, with their
		// distance from the point the pack ranks from.
		std::vector<std::pair<int, DWORD> > spread;
		long floorX = 0, floorY = 0;
		const bool onFloor = !parterStone && GetPlayerBotTowerFloorCentre(level, floorX, floorY);
		// Measured from the pack when there is one, so everybody picks among the
		// same few; from the bot itself on the ground floor and when alone.
		const bool fromPack = onFloor && scan && scan->packN >= 2;
		long fromX = fromPack ? scan->packX : ch->GetX();
		long fromY = fromPack ? scan->packY : ch->GetY();
		// On a floor the stone turns (the seventh: the Metin of Murder drops
		// the chest) the pack fights its way to the stone: the monsters are
		// ranked from the stone, so the ground round it is what gets cleared,
		// and the stone becomes a candidate the moment nothing stands there.
		// Ranked from the pack alone it drifted after whatever was nearest and
		// the stone stood untouched for nine minutes (17 September, 00:05).
		if (fromPack && level == 5)
		{
			for (size_t i = 0; i < scan->entities.size(); ++i)
				if (scan->entities[i].stone && scan->entities[i].race != PLAYERBOT_DEVIL_TOWER_STONE_FIRST)
				{
					fromX = scan->entities[i].x;
					fromY = scan->entities[i].y;
					break;
				}
		}
		// A stone cannot hit back: on a floor still full of monsters it waits,
		// or the pack walks into two hundred demons to reach it (the fourth
		// floor has no monsters, only its stones).
		const bool stonesNow = level == 2 || !scan || scan->monsters <= PLAYERBOT_TOWER_STONE_CLEAR_LIMIT;
		for (size_t i = 0; scan && i < scan->entities.size(); ++i)
		{
			const TPlayerBotTowerEntity& e = scan->entities[i];
			if (e.npc)
				continue;
			const bool first = e.stone && e.race == PLAYERBOT_DEVIL_TOWER_STONE_FIRST;
			if (parterStone ? !first : first)
				continue;
			if (onFloor && DISTANCE_APPROX(floorX - e.x, floorY - e.y) > PLAYERBOT_TOWER_FLOOR_RADIUS)
				continue;
			const int distance = DISTANCE_APPROX(ch->GetX() - e.x, ch->GetY() - e.y);
			if (maxDistance > 0 && distance > maxDistance)
				continue;
			// A stone on a floor still full of monsters is broken only once nothing
			// stands about it: the seventh floor's regen refills faster than a pack
			// kills (140-170 alive for ten minutes), so "the floor is clear" would
			// never come, while the ground round the stone does clear.
			if (!parterStone && e.stone && !stonesNow &&
					CountPlayerBotTowerMonstersNear(scan, e.x, e.y, PLAYERBOT_TOWER_STONE_CLEAR_RADIUS) > 0)
				continue;
			const int fromDistance = DISTANCE_APPROX(fromX - e.x, fromY - e.y);
			const bool shared = parterStone || e.stone || e.boss;
			int score = -fromDistance;
			if (!parterStone && e.stone)
				score += 100000;
			if (e.race == PLAYERBOT_TOWER_STONE_FLOOR4)
				score += 50000;
			if (fromPack && !shared)
				spread.push_back(std::make_pair(fromDistance, e.vid));
			if (score > bestScore)
			{
				bestScore = score;
				bestVid = e.vid;
				bestShared = shared;
			}
		}
		// No stone or boss outranks the monsters: this bot's share of them.
		// Only the ones within SPREAD_RANGE beyond the nearest count, and no
		// more of them than the pack has bots for.
		if (!bestShared && spread.size() > 1 && scan->packN > PLAYERBOT_TOWER_BOTS_PER_MONSTER)
		{
			std::sort(spread.begin(), spread.end());
			const int limit = spread.front().first + PLAYERBOT_TOWER_SPREAD_RANGE;
			size_t nearby = 0;
			while (nearby < spread.size() && spread[nearby].first <= limit)
				++nearby;
			const size_t wanted = (size_t)((scan->packN + PLAYERBOT_TOWER_BOTS_PER_MONSTER - 1) /
					PLAYERBOT_TOWER_BOTS_PER_MONSTER);
			const size_t choices = std::min(nearby, wanted);
			if (choices > 1)
				bestVid = spread[PlayerBotNavHash(ch->GetPlayerID() ^ 0x53505244U) % choices].second;
		}
		return bestVid ? CHARACTER_MANAGER::instance().Find(bestVid) : NULL;
	}

	// ------------------------------------------------------------ the rules
	// asked from earlier fragments (forward-declared in playerbot_movement.h
	// and playerbot_targeting.h)

	// A bot the tower's stones are an objective for: inside an instance, or a
	// raider on the ground floor while the raid is breaking the stone.
	bool IsPlayerBotTowerRaider(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
			return true;
		if (ch->GetMapIndex() != PLAYERBOT_MAP_DEMON_TOWER ||
				s_PlayerBotTowerRaid.bPhase != TOWER_PHASE_STONE)
			return false;
		return s_PlayerBotTowerRaid.members.find(ch->GetPlayerID()) != s_PlayerBotTowerRaid.members.end();
	}

	bool IsPlayerBotDemonTowerTarget(LPCHARACTER ch, LPCHARACTER candidate)
	{
		if (!ch || !candidate || candidate->IsDead())
			return false;
		if (IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
			return (candidate->IsMonster() || candidate->IsStone()) &&
					candidate->GetMapIndex() == ch->GetMapIndex() &&
					candidate->GetRaceNum() != PLAYERBOT_DEVIL_TOWER_STONE_FIRST;
		return candidate->IsStone() && candidate->GetRaceNum() == PLAYERBOT_DEVIL_TOWER_STONE_FIRST &&
				IsPlayerBotTowerRaider(ch);
	}

	// ------------------------------------------------------------ the fight
	//
	// The duel's shape, as in the guild war: the aura first, a caster from its
	// range, a warrior across the gap, a blade from where it reaches. The
	// equipment pass in the upkeep group sees the stone in dwTargetVID and puts
	// the archer's dagger in its hand as it does everywhere else.
	bool FightPlayerBotTowerObjective(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER foe, DWORD dwNow)
	{
		const int distance = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		state.dwTargetVID = (DWORD)foe->GetVID();
		ch->SetVictim(foe);
		ch->SetRotationToXY(foe->GetX(), foe->GetY());
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);

		if (distance <= PLAYERBOT_DUEL_BUFF_RANGE && ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
			return true;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW);
		const int combatRange = isBow ? 800 : PLAYERBOT_DUEL_MELEE_RANGE;
		const bool caster = ch->GetJob() == JOB_SHAMAN ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2);
		if (distance > combatRange)
		{
			if (!isBow && caster && distance <= PLAYERBOT_DUEL_CASTER_RANGE &&
					dwNow >= state.dwNextSkillCastTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				if (CastPlayerBotDuelSkill(ch, foe, state, dwNow))
					return true;
			}
			if (!isBow && !foe->IsStone() && distance <= PLAYERBOT_SEARCH_RANGE &&
					TryPlayerBotDuelGapCloser(ch, foe, state, dwNow, distance))
				return true;
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 1000;
				MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 4, distance > PLAYERBOT_SEARCH_RANGE, false);
			}
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_FIGHTING);
		if (!CastPlayerBotDuelSkill(ch, foe, state, dwNow))
			ExecutePlayerBotBasicAttack(ch, foe, state, dwNow);
		return true;
	}

	// What the tick does for a bot's life before the fight, and this pass
	// claims the tick above all of it: standing up after a death, the potions,
	// and breaking off at PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT.
	bool KeepPlayerBotTowerAlive(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (HandlePostDeathRecovery(ch, state, dwNow))
			return true;
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		if (!state.bRecoveringAfterDeath && ch->GetMaxHP() > 0 &&
				ch->GetHP() * 100 <= ch->GetMaxHP() * PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT)
		{
			state.bRecoveringAfterDeath = true;
			state.dwLastDeathTime = dwNow;
			state.lDeathX = ch->GetX();
			state.lDeathY = ch->GetY();
			state.dwNextRecoveryProtectionTime = 0;
			state.dwNextRecoveryHealTime = dwNow;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_TOWER: recovery started pid=%u name=%s hp=%d/%d map=%ld",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP(), ch->GetMapIndex());
			return true;
		}
		return false;
	}

	// A raid is a guild and not a party, so neither the party's buffs
	// (ManagePlayerBotBuffCompanions) nor the party branch of the self-buff
	// pass ever reached it: a Shaman in the tower buffed itself and nobody
	// else ("Szamani nie wspieraja druzyny", prodnathin, 23 September). The
	// same cast, pointed at whoever stands with it in the tower - the people
	// first, then the nearest. UseSkill refuses a buff on a character of
	// another kingdom, so they are not asked.
	struct FPlayerBotTowerFellows
	{
		LPCHARACTER m_me;
		std::vector<LPCHARACTER> m_fellows;
		FPlayerBotTowerFellows(LPCHARACTER me) : m_me(me) {}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c == m_me || !c->IsPC() || c->IsDead() || c->GetEmpire() != m_me->GetEmpire() ||
					c->GetMapIndex() != m_me->GetMapIndex())
				return;
			m_fellows.push_back(c);
		}
	};

	struct FPlayerBotTowerFellowOrder
	{
		LPCHARACTER me;
		bool operator()(LPCHARACTER a, LPCHARACTER b) const
		{
			const bool personA = !a->GetDesc() || !a->GetDesc()->IsBot();
			const bool personB = !b->GetDesc() || !b->GetDesc()->IsBot();
			if (personA != personB)
				return personA;
			return DISTANCE_APPROX(me->GetX() - a->GetX(), me->GetY() - a->GetY()) <
					DISTANCE_APPROX(me->GetX() - b->GetX(), me->GetY() - b->GetY());
		}
	};

	bool BuffPlayerBotTowerFellows(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotTowerBuffNext;
		if (!ch || ch->IsDead() || ch->GetJob() != JOB_SHAMAN || ch->GetSkillGroup() == 0 ||
				state.bRecoveringAfterDeath || !ch->GetSectree())
			return false;
		DWORD& next = s_mapPlayerBotTowerBuffNext[ch->GetPlayerID()];
		if (dwNow < next)
			return false;
		next = dwNow + PLAYERBOT_TOWER_ALLY_BUFF_INTERVAL;
		FPlayerBotTowerFellows fellows(ch);
		ch->GetSectree()->ForEachAround(fellows);
		if (fellows.m_fellows.empty())
			return false;
		FPlayerBotTowerFellowOrder order;
		order.me = ch;
		std::sort(fellows.m_fellows.begin(), fellows.m_fellows.end(), order);
		LPCHARACTER target = NULL;
		DWORD vnum = 0;
		const int done = CastPlayerBotSupportBuff(ch, state, dwNow, fellows.m_fellows, true,
				"tower_buff", target, vnum);
		if (done == 0)
			return false;
		next = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
		if (done == 2)
			PlayerBotLogThrottled("tower_buff", dwNow,
					"PLAYERBOT_TOWER: buffed a fellow pid=%u name=%s fellow=%s person=%d vnum=%u map=%ld",
					ch->GetPlayerID(), ch->GetName(), target->GetName(),
					(!target->GetDesc() || !target->GetDesc()->IsBot()) ? 1 : 0, vnum, ch->GetMapIndex());
		return true;
	}

	// ------------------------------------------------------------- the keys

	int FindPlayerBotTowerItemCell(LPCHARACTER ch, DWORD vnum)
	{
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetVnum() == vnum)
				return cell;
		}
		return -1;
	}

	// A key that is used where the bot stands: the Unknown Old Chest (30300)
	// and the Map of the Tower (30302) on the seventh floor.
	bool UsePlayerBotTowerKey(LPCHARACTER ch, TPlayerBotAIState& state, DWORD vnum, DWORD dwNow)
	{
		const int cell = FindPlayerBotTowerItemCell(ch, vnum);
		if (cell < 0)
			return false;
		if (dwNow < state.dwNextTowerMoveTime)
			return true;
		state.dwNextTowerMoveTime = dwNow + 3000;
		if (ch->IsStateMove())
			ch->Stop();
		const bool ok = ch->UseItem(TItemPos(INVENTORY, (WORD)cell));
		sys_log(0, "PLAYERBOT_TOWER: key used pid=%u name=%s vnum=%u ok=%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), vnum, ok ? 1 : 0, ch->GetMapIndex());
		return true;
	}

	// A key that is handed to somebody: the Opening Stone (50084) to one of
	// the five Ancient Seals on the fifth floor, the Zin-Bong-In Key (30304)
	// to Sa-Soe on the eighth. CHARACTER::GiveItem is what a player's drag
	// onto the NPC does, and the quest's take handler answers it.
	bool CarryPlayerBotTowerKey(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotTowerScan* scan,
			DWORD vnum, DWORD npcFirst, DWORD npcLast, DWORD dwNow)
	{
		const int cell = FindPlayerBotTowerItemCell(ch, vnum);
		if (cell < 0)
			return false;
		LPCHARACTER npc = FindPlayerBotTowerNpc(scan, npcFirst, npcLast, ch->GetX(), ch->GetY());
		if (!npc)
			return false;
		const int distance = DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY());
		if (distance > PLAYERBOT_TOWER_HANDIN_RANGE)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 1000;
				MovePlayerBot(ch, npc->GetX(), npc->GetY(), dwNow, 4, true, false);
			}
			return true;
		}
		if (dwNow < state.dwNextTowerMoveTime)
			return true;
		state.dwNextTowerMoveTime = dwNow + 2000;
		if (ch->IsStateMove())
			ch->Stop();
		const bool ok = ch->GiveItem(npc, TItemPos(INVENTORY, (WORD)cell));
		sys_log(0, "PLAYERBOT_TOWER: key handed pid=%u name=%s vnum=%u npc=%u ok=%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), vnum, npc->GetRaceNum(), ok ? 1 : 0, ch->GetMapIndex());
		return true;
	}

	// The tower's keys left in a bag on the way out, as the quest removes them
	// on a player's logout.
	void DropPlayerBotTowerKeys(LPCHARACTER ch)
	{
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && (IsPlayerBotDemonTowerKey(item->GetVnum()) ||
					item->GetVnum() == PLAYERBOT_TOWER_FAKE_KEY))
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_TOWER");
		}
	}

	// ------------------------------------------------------------ the smith
	//
	// What devil_jump_7 in deviltower_zone.quest does once a player of
	// seventy-five has told the smith to go on: the floor purged, the four
	// Metins of Death set, the level advanced and everybody jumped to the
	// seventh floor. A bot cannot press the select, so the first bot of that
	// level does it from here, and the smith's lock on his dialog is not in
	// the way.
	void AdvancePlayerBotTowerPastSmith(LPDUNGEON d, LPCHARACTER ch)
	{
		d->Purge();
		d->ClearRegen();
		d->SpawnMob(PLAYERBOT_TOWER_STONE_FLOOR7, 639, 658);
		d->SpawnMob(PLAYERBOT_TOWER_STONE_FLOOR7, 611, 637);
		d->SpawnMob(PLAYERBOT_TOWER_STONE_FLOOR7, 596, 674);
		d->SpawnMob(PLAYERBOT_TOWER_STONE_FLOOR7, 629, 670);
		AdvancePlayerBotDungeonLevel(d);
		d->Notice("Kamienie Smierci strzega siodmego pietra. Rozbijcie wszystkie cztery!");
		d->JumpAll(d->GetMapIndex(), PLAYERBOT_TOWER_BASE_CELL_X + 621, PLAYERBOT_TOWER_BASE_CELL_Y + 631);
		sys_log(0, "PLAYERBOT_TOWER: smith passed pid=%u name=%s level=%u map=%ld",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), d->GetMapIndex());
	}

	// What a player does with the smith before anybody takes the run past him:
	// one piece dragged onto him and the fee paid (CanReceiveItem, then
	// RefineInformation with REFINE_TYPE_MONEY_ONLY, then CInputMain::Refine
	// calls DoRefine(item, true) and takes one off the quest's can_refine
	// flag). Which smith stands is the quest's roll, and what each takes is
	// CHARACTER::CanReceiveItem's: 20074 a weapon - by its item type, so a rod
	// in the hand is no weapon to him, which the first read of this on live
	// bots got wrong - 20075 a body armour, a shield or a helmet, 20076 the
	// rest of the armour. The refine sets he refuses are the engine's own too:
	// the two elite sets on mt2009 (IsEliteRefine), everything from 500 on
	// r40250.
	bool IsPlayerBotTowerSmithPiece(DWORD smithRace, LPITEM item)
	{
		if (!item || item->GetRefinedVnum() == 0)
			return false;
		const bool bodyShieldHead = item->GetType() == ITEM_ARMOR &&
				(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD ||
				 item->GetSubType() == ARMOR_HEAD);
		switch (smithRace)
		{
			case 20074:
				return item->GetType() == ITEM_WEAPON;
			case 20075:
				return bodyShieldHead;
			case 20076:
				return item->GetType() == ITEM_ARMOR && !bodyShieldHead;
		}
		return false;
	}

	bool IsPlayerBotTowerSmithRefineSet(DWORD refineSet)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return refineSet != 0 && !IsEliteRefine(refineSet);
#else
		return refineSet != 0 && refineSet < 500;
#endif
	}

	// The piece this bot gives him: the first worn one of his slots, in the
	// blacksmith pass's order, that the bot's own rules would raise at a plain
	// anvil - below its refine target, a result it may wear, the fee in hand.
	// That is what this is with the materials waived, so the anvil's risk
	// stands and so does everything the bot keeps a piece back from it for:
	// a scroll-only weapon, a prize at odds under PLAYERBOT_PRIZE_SAFE_REFINE_PROB,
	// a level-30 weapon above its ceiling, the only weapon or armour at a step
	// that can burn it. And a weapon or a body armour with nothing in the bag
	// to put on instead is not risked at all in here: there is no merchant
	// between the sixth floor and the ninth.
	LPITEM PickPlayerBotTowerSmithPiece(LPCHARACTER ch, DWORD smithRace)
	{
		static const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			const BYTE wear = wearSlots[i];
			LPITEM item = ch->GetWear(wear);
			if (!IsPlayerBotTowerSmithPiece(smithRace, item) || item->isLocked() || item->IsExchanging() ||
					!IsPlayerBotWornItemSound(ch, item, wear) ||
					!IsPlayerBotTowerSmithRefineSet(item->GetRefineSet()))
				continue;
			const BYTE plus = item->GetRefineLevel();
			if (plus >= GetPlayerBotRefineTarget(ch, item) ||
					!IsPlayerBotWearableAtLevel(ch, item->GetRefinedVnum()))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe || (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) <
					(long long)ch->ComputeRefineFee(recipe->cost))
				continue;
			if (IsPlayerBotScrollOnlyWeapon(item))
				continue;
			if ((wear == WEAR_WEAPON || wear == WEAR_BODY) && !HasPlayerBotBackupGear(ch, wear))
				continue;
			const bool scrollStepAllowed = IsPlayerBotScrollStepAllowed(plus);
			if (scrollStepAllowed &&
					(IsPlayerBotWornWeaponAtRisk(ch, item) || IsPlayerBotWornArmourAtRisk(ch, item)))
				continue;
			if (IsPlayerBotSpecialLevel30Weapon(item))
			{
				if ((int)plus >= GetPlayerBotLevel30AnvilCeiling(
						SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS)))
					continue;
			}
			else if (scrollStepAllowed && IsPlayerBotPrizeItem(item) &&
					recipe->prob < PLAYERBOT_PRIZE_SAFE_REFINE_PROB)
				continue;
			return item;
		}
		return NULL;
	}

	// This bot's turn at the smith: the walk to him, the piece off, the
	// refine, the flag down. True while it claims the tick; false once the
	// turn is over, with the bot entered in run.smithServed either way - a bot
	// with nothing to give him has had its turn too.
	bool UsePlayerBotTowerSmith(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER smith,
			TPlayerBotTowerRun& run, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		if (run.smithServed.find(pid) != run.smithServed.end())
			return false;
		const int canRefine = ch->GetQuestFlag("deviltower_zone.can_refine");
		LPITEM item = canRefine > 0 ? PickPlayerBotTowerSmithPiece(ch, smith->GetRaceNum()) : NULL;
		if (!item)
		{
			run.smithServed.insert(pid);
			sys_log(0, "PLAYERBOT_TOWER: smith has nothing for pid=%u name=%s smith=%u can_refine=%d",
					pid, ch->GetName(), smith->GetRaceNum(), canRefine);
			return false;
		}
		const int distance = DISTANCE_APPROX(ch->GetX() - smith->GetX(), ch->GetY() - smith->GetY());
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (distance > PLAYERBOT_TOWER_HANDIN_RANGE)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 1000;
				MovePlayerBot(ch, smith->GetX(), smith->GetY(), dwNow, 4, true, false);
			}
			return true;
		}
		if (dwNow < state.dwNextTowerMoveTime)
			return true;
		state.dwNextTowerMoveTime = dwNow + 2000;
		if (ch->IsStateMove())
			ch->Stop();
		const BYTE plus = item->GetRefineLevel();
		const DWORD oldVnum = item->GetVnum();
		const DWORD nextVnum = item->GetRefinedVnum();
		// DoRefine puts the result where the piece lay in the bag: a worn one
		// comes off first, into a cell of its size, and the equipment pass
		// puts the result back on. The engine refuses the swap for a second
		// and a half after a blow; the next pass asks again.
		if (item->IsEquipped())
		{
			if (ch->GetEmptyInventory(item->GetSize()) < 0)
			{
				run.smithServed.insert(pid);
				sys_log(0, "PLAYERBOT_TOWER: smith refine skipped pid=%u name=%s vnum=%u reason=no_bag_cell",
						pid, ch->GetName(), oldVnum);
				return false;
			}
			if (!ch->UnequipItem(item) || item->IsEquipped())
				return true;
		}
		if (item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
				item->GetCell() >= PLAYERBOT_BAG_CELLS || ch->GetInventoryItem(item->GetCell()) != item)
			return true;
		const int resultCountBefore = ch->CountSpecifyItem(nextVnum);
		const bool attempted = ch->DoRefine(item, true);
		run.smithServed.insert(pid);
		if (!attempted)
		{
			sys_log(0, "PLAYERBOT_TOWER: smith refine refused pid=%u name=%s vnum=%u plus=%u",
					pid, ch->GetName(), oldVnum, (unsigned int)plus);
			return true;
		}
		ch->SetQuestFlag("deviltower_zone.can_refine", std::max(0, canRefine - 1));
		const bool success = ch->CountSpecifyItem(nextVnum) > resultCountBefore;
		if (success)
		{
			BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plus + 1);
			NotePlayerBotMoodRefine(ch, (int)plus + 1);
		}
		else
			NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "burned");
		sys_log(0, "PLAYERBOT_TOWER: smith refine %s pid=%u name=%s smith=%u old_vnum=%u new_vnum=%u plus=%u",
				success ? "SUCCESS" : "FAILED_BURNED", pid, ch->GetName(), smith->GetRaceNum(),
				oldVnum, nextVnum, (unsigned int)plus + 1);
		return true;
	}

	// Whether everybody in the instance has had the turn: every bot entered in
	// run.smithServed, every person's flag spent. A person may not want the
	// smith at all, which nothing here can tell; the wait's own cap
	// (PLAYERBOT_TOWER_SMITH_REFINE_WAIT_MS) is for them.
	struct FPlayerBotTowerSmithWaiting
	{
		const TPlayerBotTowerRun& m_run;
		int m_waiting;
		FPlayerBotTowerSmithWaiting(const TPlayerBotTowerRun& run) : m_run(run), m_waiting(0) {}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (!c->IsPC() || c->IsDead())
				return;
			if (c->GetDesc() && c->GetDesc()->IsBot())
			{
				if (m_run.smithServed.find(c->GetPlayerID()) == m_run.smithServed.end())
					++m_waiting;
			}
			else if (c->GetQuestFlag("deviltower_zone.can_refine") > 0)
				++m_waiting;
		}
	};

	int CountPlayerBotTowerSmithWaiting(long lMapIndex, const TPlayerBotTowerRun& run)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		if (!pMap)
			return 0;
		FPlayerBotTowerSmithWaiting f(run);
		pMap->for_each(f);
		return f.m_waiting;
	}

	// ------------------------------------------------------------ the floors

	bool ManagePlayerBotTowerFloor(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const long map = ch->GetMapIndex();
		LPDUNGEON d = ch->GetDungeon();
		if (!d)
		{
			d = CDungeonManager::instance().FindByMapIndex(map);
			if (d)
				ch->SetDungeon(d);
		}
		if (!d)
		{
			// The instance is gone from under the bot.
			long destMap = 0, destX = 0, destY = 0;
			sys_log(0, "PLAYERBOT_TOWER: instance gone pid=%u name=%s map=%ld", ch->GetPlayerID(), ch->GetName(), map);
			state.lTowerInstance = 0;
			if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
				TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "tower_instance_gone");
			return true;
		}

		TPlayerBotTowerRun& run = s_mapPlayerBotTowerRuns[map];
		if (run.dwEnteredAt == 0)
		{
			run.dwEnteredAt = dwNow;
			run.dwLastProgress = dwNow;
			run.dwLevelSince = dwNow;
			sys_log(0, "PLAYERBOT_TOWER: run begins map=%ld first_pid=%u name=%s", map, ch->GetPlayerID(), ch->GetName());
		}
		if (state.lTowerInstance != map)
		{
			state.lTowerInstance = map;
			state.bTowerTalkStep = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			// Where d.exit_all sends a bot: the ground floor's entrance, as the
			// quest's login handler sets it for a player.
			ch->SetWarpLocation(PLAYERBOT_MAP_DEMON_TOWER, PLAYERBOT_TOWER_PARTER_CELL_X, PLAYERBOT_TOWER_PARTER_CELL_Y);
			if (s_PlayerBotTowerRaid.bPhase == TOWER_PHASE_STONE &&
					s_PlayerBotTowerRaid.members.find(ch->GetPlayerID()) != s_PlayerBotTowerRaid.members.end())
			{
				s_PlayerBotTowerRaid.bPhase = TOWER_PHASE_INSIDE;
				s_PlayerBotTowerRaid.lInstance = map;
				s_PlayerBotTowerRaid.dwPhaseSince = dwNow;
				sys_log(0, "PLAYERBOT_TOWER: raid inside guild=%u instance=%ld", s_PlayerBotTowerRaid.dwGuildID, map);
			}
			sys_log(0, "PLAYERBOT_TOWER: entered pid=%u name=%s level=%u map=%ld raid=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), map,
					state.dwTowerRaidGuild != 0 ? 1 : 0);
		}

		const int level = GetPlayerBotDungeonLevel(d);
		// A jump to the next floor is a Show on the same map, so nothing reset
		// this bot's route or target: the floor last seen is kept in
		// bTowerTalkStep (level + 1) and both are dropped when it moves.
		if (state.bTowerTalkStep != (BYTE)(level + 1))
		{
			state.bTowerTalkStep = (BYTE)(level + 1);
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			if (ch->IsStateMove())
				ch->Stop();
		}
		const TPlayerBotTowerScan* scan = ScanPlayerBotTowerMap(map, dwNow);
		if (level != run.iLevel)
		{
			run.iLevel = level;
			run.dwLevelSince = dwNow;
			run.dwLastProgress = dwNow;
			run.dwSmithSince = 0;
			run.bSmithDone = false;
			run.smithServed.clear();
			if (level + 2 > s_iPlayerBotTowerBestFloor)
				s_iPlayerBotTowerBestFloor = level + 2;
			sys_log(0, "PLAYERBOT_TOWER: floor %d map=%ld alive=%d after_s=%u",
					level + 2, map, scan->alive, (dwNow - run.dwEnteredAt) / 1000U);
		}
		if (scan->alive != run.iAlive)
		{
			run.iAlive = scan->alive;
			run.dwLastProgress = dwNow;
		}
		if (dwNow >= run.dwNextReport)
		{
			run.dwNextReport = dwNow + 60000;
			int inside = 0, dead = 0;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
				if (it->second.lTowerInstance == map)
				{
					++inside;
					if (it->second.dwDeathDetectedTime != 0)
						++dead;
				}
			sys_log(0, "PLAYERBOT_TOWER: progress map=%ld floor=%d alive=%d bots=%d dead=%d upper=%d floor_s=%u run_s=%u",
					map, level + 2, scan->alive, inside, dead, scan->upperBots,
					(dwNow - run.dwLevelSince) / 1000U, (dwNow - run.dwEnteredAt) / 1000U);
		}
		if (!run.bEnding)
		{
			if (dwNow - run.dwLastProgress > PLAYERBOT_TOWER_STALL_MS)
			{
				run.bEnding = true;
				run.pszEnd = "stalled";
			}
			else if (dwNow - run.dwLevelSince > PLAYERBOT_TOWER_FLOOR_MAX_MS)
			{
				run.bEnding = true;
				run.pszEnd = "floor_timeout";
			}
			else if (dwNow - run.dwEnteredAt > PLAYERBOT_TOWER_MAX_MS)
			{
				run.bEnding = true;
				run.pszEnd = "run_timeout";
			}
			if (run.bEnding)
			{
				sys_log(0, "PLAYERBOT_TOWER: run ends map=%ld floor=%d reason=%s after_s=%u",
						map, level + 2, run.pszEnd, (dwNow - run.dwEnteredAt) / 1000U);
				d->Notice("Boty opuszczaja Wieze Demonow.");
			}
		}
		if (run.bEnding)
		{
			// One WarpSet a few seconds, not one a tick, should the exit be refused.
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 5000;
				ch->ExitToSavedLocation();
			}
			return true;
		}

		// The tower is from PLAYERBOT_TOWER_MIN_LEVEL, as its keeper tells a
		// player at the door (deviltower_zone.quest), and a raid only ever
		// calls bots of that level - but the stone's jump takes everybody on
		// the ground floor, and a bot below it that came along only dies on
		// the floors ("przydalby sie okreslony minimalny poziom", prodnathin,
		// 23 September; "40 poziom minimum", Tieru). A bot in a person's party
		// stays with the person, who decided.
		if (ch->GetLevel() < PLAYERBOT_TOWER_MIN_LEVEL &&
				!(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())))
		{
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 5000;
				sys_log(0, "PLAYERBOT_TOWER: under the tower's level, leaving pid=%u name=%s level=%u map=%ld",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), map);
				ch->ExitToSavedLocation();
			}
			return true;
		}

		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "tower");
			return true;
		}
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);

		// The floor's keys before the floor's fight.
		if (level == 5)
		{
			if (UsePlayerBotTowerKey(ch, state, PLAYERBOT_TOWER_MAP_ITEM, dwNow))
				return true;
			if (UsePlayerBotTowerKey(ch, state, PLAYERBOT_TOWER_CHEST_ITEM, dwNow))
				return true;
		}
		if (level == 3 && CarryPlayerBotTowerKey(ch, state, scan, PLAYERBOT_TOWER_OPENING_STONE,
				PLAYERBOT_TOWER_NPC_SEAL, PLAYERBOT_TOWER_NPC_SEAL, dwNow))
			return true;
		if (level == 6 && CarryPlayerBotTowerKey(ch, state, scan, PLAYERBOT_TOWER_KEY_ITEM,
				PLAYERBOT_TOWER_NPC_SASOE, PLAYERBOT_TOWER_NPC_SASOE, dwNow))
			return true;

		// The sixth floor's smith stands once the Elite Demon King is down.
		// Everybody has a turn at him first (UsePlayerBotTowerSmith), and only
		// then does a bot of seventy-five take the run on - or when the turns
		// have taken PLAYERBOT_TOWER_SMITH_REFINE_WAIT_MS.
		if (level == 4 && !run.bSmithDone)
		{
			LPCHARACTER smith = FindPlayerBotTowerNpc(scan, PLAYERBOT_TOWER_NPC_SMITH_FIRST,
					PLAYERBOT_TOWER_NPC_SMITH_LAST, ch->GetX(), ch->GetY());
			if (smith)
			{
				if (run.dwSmithSince == 0)
				{
					run.dwSmithSince = dwNow;
					sys_log(0, "PLAYERBOT_TOWER: smith stands map=%ld smith=%u upper_bots=%d",
							map, smith->GetRaceNum(), scan->upperBots);
				}
				if (UsePlayerBotTowerSmith(ch, state, smith, run, dwNow))
					return true;
				// Only the bots that could act on it ask whether the turns are
				// over: the answer walks the whole instance.
				const bool upper = ch->GetLevel() >= PLAYERBOT_TOWER_UPPER_LEVEL;
				const bool mayEnd = !upper && scan->upperBots == 0 &&
						dwNow - run.dwSmithSince > PLAYERBOT_TOWER_SMITH_WAIT_MS;
				const bool turnsTaken = (upper || mayEnd) &&
						(dwNow - run.dwSmithSince > PLAYERBOT_TOWER_SMITH_REFINE_WAIT_MS ||
						 CountPlayerBotTowerSmithWaiting(map, run) == 0);
				if (turnsTaken && upper)
				{
					run.bSmithDone = true;
					sys_log(0, "PLAYERBOT_TOWER: smith turns over map=%ld served=%u after_s=%u",
							map, (unsigned int)run.smithServed.size(), (dwNow - run.dwSmithSince) / 1000U);
					AdvancePlayerBotTowerPastSmith(d, ch);
					return true;
				}
				if (turnsTaken && mayEnd)
				{
					run.bEnding = true;
					run.pszEnd = "nobody_of_75";
					sys_log(0, "PLAYERBOT_TOWER: run ends map=%ld floor=6 reason=%s after_s=%u",
							map, run.pszEnd, (dwNow - run.dwEnteredAt) / 1000U);
					d->Notice("Nikt z botow nie ma 75. poziomu - Wieza konczy sie na szostym pietrze.");
					return true;
				}
			}
		}

		// The Shaman's fellows before its own next blow.
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;

		// A straggler with nothing at its feet walks back to the pack first.
		if (scan->packN >= 2 &&
				DISTANCE_APPROX(ch->GetX() - scan->packX, ch->GetY() - scan->packY) > PLAYERBOT_TOWER_PACK_RADIUS &&
				!PickPlayerBotTowerObjective(ch, scan, level, false, PLAYERBOT_TOWER_PACK_FIGHT_RANGE))
		{
			state.dwTargetVID = 0;
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 1500;
				MovePlayerBot(ch, scan->packX, scan->packY, dwNow, 8, true, false);
			}
			return true;
		}

		// The fight: the foe in hand while it stands, the nearest objective
		// when it is lost.
		LPCHARACTER foe = NULL;
		if (state.dwTargetVID != 0)
		{
			LPCHARACTER held = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (held && !held->IsDead() && held->GetMapIndex() == map &&
					(held->IsMonster() || held->IsStone()) &&
					held->GetRaceNum() != PLAYERBOT_DEVIL_TOWER_STONE_FIRST)
				foe = held;
		}
		if (!foe)
			foe = PickPlayerBotTowerObjective(ch, scan, level, false, 0);
		if (!foe)
		{
			// Nothing to fight: the floor's own script is at work (a jump in
			// six seconds, a seal, a spawn), or a key still has to drop.
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (ch->IsStateMove())
				ch->Stop();
			return true;
		}
		return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
	}

	// ------------------------------------------------------------ the way out

	void LeavePlayerBotTower(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const long instance = state.lTowerInstance;
		const bool raider = state.dwTowerRaidGuild != 0;
		state.lTowerInstance = 0;
		state.bTowerTalkStep = 0;
		state.dwTowerRaidGuild = 0;
		state.bTowerSummoned = false;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		DropPlayerBotTowerKeys(ch);
		sys_log(0, "PLAYERBOT_TOWER: left pid=%u name=%s instance=%ld map=%ld raider=%d",
				ch->GetPlayerID(), ch->GetName(), instance, ch->GetMapIndex(), raider ? 1 : 0);
		// A raider goes home; a bystander is back on the ground it came for,
		// unless it is under the tower's level, when the next raid's jump would
		// only take it in again.
		const bool underLevel = ch->GetLevel() < PLAYERBOT_TOWER_MIN_LEVEL &&
				!(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()));
		if ((raider || underLevel) && ch->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER)
		{
			long destMap = 0, destX = 0, destY = 0;
			if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
				TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "tower_over");
		}
	}

	// ------------------------------------------------------------ the ground floor

	// The bot's own spot near the stone: the stone's point with a few hundred
	// units of pid, on open ground.
	void GetPlayerBotTowerRally(DWORD pid, long& outX, long& outY)
	{
		const long angle = (long)(PlayerBotNavHash(pid ^ 0x544F5745U) % 360U);
		const long radius = 400 + (long)(PlayerBotNavHash(pid ^ 0x544F5752U) % 400U);
		const double rad = angle * 3.14159265 / 180.0;
		outX = PLAYERBOT_TOWER_STONE_X + (long)(cos(rad) * radius);
		outY = PLAYERBOT_TOWER_STONE_Y + (long)(sin(rad) * radius);
		long openX = 0, openY = 0;
		if (FindPlayerBotWarGround(PLAYERBOT_MAP_DEMON_TOWER, outX, outY, 600, openX, openY))
		{
			outX = openX;
			outY = openY;
		}
	}

	// A raider on its way to, or standing on, the ground floor.
	bool ManagePlayerBotTowerCall(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;
		if (raid.bPhase == TOWER_PHASE_NONE || raid.dwGuildID != state.dwTowerRaidGuild ||
				raid.members.find(pid) == raid.members.end() || raid.bPhase == TOWER_PHASE_INSIDE)
		{
			// The raid is over, or went in without this bot.
			state.dwTowerRaidGuild = 0;
			if (ch->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER)
			{
				long destMap = 0, destX = 0, destY = 0;
				if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
					TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "tower_missed");
				return true;
			}
			return false;
		}
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;

		if (ch->GetMapIndex() != PLAYERBOT_MAP_DEMON_TOWER)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextTowerMoveTime)
				return true;
			state.dwNextTowerMoveTime = dwNow + 5000;
			if (!IsPlayerBotMapHostedHere(PLAYERBOT_MAP_DEMON_TOWER))
			{
				state.dwTowerRaidGuild = 0;
				return false;
			}
			long rallyX = 0, rallyY = 0;
			GetPlayerBotTowerRally(pid, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_DEMON_TOWER, rallyX, rallyY, dwNow, "tower_raid");
			return true;
		}
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "tower");
			return true;
		}
		const TPlayerBotTowerScan* scan = ScanPlayerBotTowerMap(PLAYERBOT_MAP_DEMON_TOWER, dwNow);
		if (raid.bPhase == TOWER_PHASE_STONE)
		{
			// The first floor comes six seconds after the stone breaks.
			if (BuffPlayerBotTowerFellows(ch, state, dwNow))
				return true;
			LPCHARACTER stone = PickPlayerBotTowerObjective(ch, scan, -1, true, 0);
			if (stone)
				return FightPlayerBotTowerObjective(ch, state, stone, dwNow);
		}
		// Gathering: the ground floor's own demons are fought where they come,
		// and the bot keeps to its spot by the stone otherwise.
		LPCHARACTER foe = PickPlayerBotTowerObjective(ch, scan, -1, false, PLAYERBOT_TOWER_GATHER_FIGHT_RANGE);
		if (foe)
			return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		long rallyX = 0, rallyY = 0;
		GetPlayerBotTowerRally(pid, rallyX, rallyY);
		if (DISTANCE_APPROX(ch->GetX() - rallyX, ch->GetY() - rallyY) > 500 &&
				dwNow >= state.dwNextTowerMoveTime)
		{
			state.dwNextTowerMoveTime = dwNow + 3000;
			MovePlayerBot(ch, rallyX, rallyY, dwNow, 8, true, false);
		}
		return true;
	}

	// A player's guild: its bots come to the ground floor while their human
	// master stands there, and stand by him until he breaks the stone.
	bool ManagePlayerBotTowerWithMaster(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		CGuild* guild = ch->GetGuild();
		LPCHARACTER master = NULL;
		if (guild && !CPlayerBotManager::instance().IsRegisteredBotPID(guild->GetMasterPID()))
			master = guild->GetMasterCharacter();
		const bool summoned = master && master != ch && !master->IsDead() &&
				master->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER &&
				ch->GetLevel() >= PLAYERBOT_TOWER_MIN_LEVEL &&
				!(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()));
		if (!summoned)
		{
			if (state.bTowerSummoned)
			{
				state.bTowerSummoned = false;
				sys_log(0, "PLAYERBOT_TOWER: master left the ground floor pid=%u name=%s", ch->GetPlayerID(), ch->GetName());
				if (ch->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER)
				{
					long destMap = 0, destX = 0, destY = 0;
					if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
						TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "tower_master_gone");
					return true;
				}
			}
			return false;
		}
		if (!state.bTowerSummoned)
		{
			state.bTowerSummoned = true;
			sys_log(0, "PLAYERBOT_TOWER: called by master pid=%u name=%s guild=%s master=%s",
					ch->GetPlayerID(), ch->GetName(), guild->GetName(), master->GetName());
		}
		if (ch->GetMapIndex() != PLAYERBOT_MAP_DEMON_TOWER)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextTowerMoveTime)
				return true;
			state.dwNextTowerMoveTime = dwNow + 5000;
			if (!IsPlayerBotMapHostedHere(PLAYERBOT_MAP_DEMON_TOWER))
				return false;
			TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_DEMON_TOWER,
					master->GetX() + (long)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4D535452U) % 601U) - 300,
					master->GetY() + (long)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4D535453U) % 601U) - 300,
					dwNow, "tower_master");
			return true;
		}
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "tower");
			return true;
		}
		const TPlayerBotTowerScan* scan = ScanPlayerBotTowerMap(PLAYERBOT_MAP_DEMON_TOWER, dwNow);
		LPCHARACTER foe = PickPlayerBotTowerObjective(ch, scan, -1, false, PLAYERBOT_TOWER_GATHER_FIGHT_RANGE);
		if (foe)
			return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (DISTANCE_APPROX(ch->GetX() - master->GetX(), ch->GetY() - master->GetY()) > 700 &&
				dwNow >= state.dwNextTowerMoveTime)
		{
			state.dwNextTowerMoveTime = dwNow + 3000;
			MovePlayerBot(ch, master->GetX(), master->GetY(), dwNow, 8, true, false);
		}
		return true;
	}

	// The per-bot pass. Claims the tick inside the tower and on the way to it.
	bool ManagePlayerBotDemonTower(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		if (IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
			return ManagePlayerBotTowerFloor(ch, state, dwNow);
		if (state.lTowerInstance != 0)
		{
			LeavePlayerBotTower(ch, state, dwNow);
			return true;
		}
		if (state.dwTowerRaidGuild != 0)
			return ManagePlayerBotTowerCall(ch, state, dwNow);
		if (dwNow < state.dwNextTowerMasterCheckTime)
			return state.bTowerSummoned ? ManagePlayerBotTowerWithMaster(ch, state, dwNow) : false;
		state.dwNextTowerMasterCheckTime = dwNow + (state.bTowerSummoned ? 1000 : 10000);
		return ManagePlayerBotTowerWithMaster(ch, state, dwNow);
	}

	// ------------------------------------------------------------ the call

	struct TPlayerBotTowerCandidate
	{
		DWORD pid;
		int strength;
		int level;
	};

	bool PlayerBotTowerCandidateOrder(const TPlayerBotTowerCandidate& a, const TPlayerBotTowerCandidate& b)
	{
		if (a.strength != b.strength)
			return a.strength > b.strength;
		return a.pid < b.pid;
	}

	// The guild's members this core could send: of the level, alive, not
	// with a player, strongest first, at most PLAYERBOT_TOWER_MAX_MEMBERS.
	void GatherPlayerBotTowerMembers(CGuild* g, std::vector<TPlayerBotTowerCandidate>& out, int& upper)
	{
		out.clear();
		upper = 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || ch->GetGuild() != g || ch->IsDead() || ch->GetLevel() < PLAYERBOT_TOWER_MIN_LEVEL)
				continue;
			if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
				continue;
			if (IsPlayerBotDropper(it->second.bPersonality))
				continue;
			// A rod or a pickaxe is no weapon (the duel learned this first).
			if (IsPlayerBotAngler(ch, it->second) || IsPlayerBotMiner(ch, it->second) || it->second.bFishingSession)
				continue;
			TPlayerBotTowerCandidate c;
			c.pid = it->first;
			// The census waits ten minutes after a start; until it has run the
			// level stands in, or the pick is by pid (the first raid on the test
			// world took the sixteen lowest pids, two of them of forty-two).
			c.strength = GetPlayerBotStrengthCached(it->first);
			if (c.strength <= 0)
				c.strength = ch->GetLevel() * 1000;
			c.level = ch->GetLevel();
			out.push_back(c);
		}
		std::sort(out.begin(), out.end(), PlayerBotTowerCandidateOrder);
		if (out.size() > (size_t)PLAYERBOT_TOWER_MAX_MEMBERS)
			out.resize((size_t)PLAYERBOT_TOWER_MAX_MEMBERS);
		for (size_t i = 0; i < out.size(); ++i)
			if (out[i].level >= PLAYERBOT_TOWER_UPPER_LEVEL)
				++upper;
	}

	struct TPlayerBotTowerGuildEntry
	{
		CGuild* guild;
		BYTE empire;
		int upper;
		std::vector<TPlayerBotTowerCandidate> members;
	};

	// The guild that goes: enough members of the level in this core's world,
	// not at war, one with a bot of seventy-five ahead of one without, and the
	// pick rotated by the raids fought so the same guild does not go every time.
	bool PickPlayerBotTowerGuild(DWORD dwNow, TPlayerBotTowerGuildEntry& out)
	{
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();
		std::vector<TPlayerBotTowerGuildEntry> ready;
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			CGuild* g = CGuildManager::instance().FindGuild(it->first);
			if (!g || g->UnderAnyWar() != 0)
				continue;
			// Not with the kingdom's war about to be declared: the war picker
			// skips a raiding guild, and this is the other half of that.
			const int nextWar = GetPlayerBotNextGuildWarInSeconds(it->second.bEmpire, dwNow);
			if (nextWar > 0 && nextWar < 600)
				continue;
			TPlayerBotTowerGuildEntry e;
			e.guild = g;
			e.empire = it->second.bEmpire;
			GatherPlayerBotTowerMembers(g, e.members, e.upper);
			if ((int)e.members.size() < PLAYERBOT_TOWER_MIN_MEMBERS)
				continue;
			ready.push_back(e);
		}
		if (ready.empty())
			return false;
		std::vector<size_t> preferred;
		for (size_t i = 0; i < ready.size(); ++i)
			if (ready[i].upper > 0)
				preferred.push_back(i);
		const size_t n = preferred.empty() ? ready.size() : preferred.size();
		const size_t pick = (size_t)((s_uPlayerBotTowerRaids + PlayerBotNavHash(dwNow / 3600000U)) % n);
		out = ready[preferred.empty() ? pick : preferred[pick]];
		return true;
	}

	void CallPlayerBotTowerRaid(const TPlayerBotTowerGuildEntry& e, DWORD dwNow, const char* why)
	{
		TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;
		raid = TPlayerBotTowerRaid();
		raid.dwGuildID = e.guild->GetID();
		raid.bEmpire = e.empire;
		raid.bPhase = TOWER_PHASE_GATHER;
		raid.dwCalledAt = dwNow;
		raid.dwPhaseSince = dwNow;
		raid.iUpper = e.upper;
		for (size_t i = 0; i < e.members.size(); ++i)
		{
			raid.members.insert(e.members[i].pid);
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(e.members[i].pid);
			if (st != s_mapPlayerBotAIStates.end())
			{
				st->second.dwTowerRaidGuild = raid.dwGuildID;
				// Spread the departures over the first minute by pid.
				st->second.dwNextTowerMoveTime = dwNow + (PlayerBotNavHash(e.members[i].pid ^ 0x544F5731U) % 60000U);
			}
		}
		++s_uPlayerBotTowerRaids;
		char msg[220];
		snprintf(msg, sizeof(msg), "Wieza Demonow! Zbiorka na parterze wiezy przy Metinie Twardosci - za %u minut rozbijamy go razem.",
				(unsigned int)(PLAYERBOT_TOWER_GATHER_MS / 60000U));
		e.guild->Chat(msg);
		snprintf(msg, sizeof(msg), "Gildia %s (%s) rusza na Wieze Demonow: zbiorka na parterze wiezy, start za %u minut. Kto stoi na parterze, wchodzi razem z nimi.",
				e.guild->GetName(), GetPlayerBotKingdomName(e.empire), (unsigned int)(PLAYERBOT_TOWER_GATHER_MS / 60000U));
		BroadcastNotice(msg);
		sys_log(0, "PLAYERBOT_TOWER: raid called guild=%s id=%u empire=%u members=%u upper=%d why=%s",
				e.guild->GetName(), raid.dwGuildID, (unsigned int)e.empire,
				(unsigned int)e.members.size(), e.upper, why);
	}

	void EndPlayerBotTowerRaid(DWORD dwNow, const char* why)
	{
		TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;
		CGuild* g = CGuildManager::instance().FindGuild(raid.dwGuildID);
		int floor = 0;
		std::map<long, TPlayerBotTowerRun>::iterator run = s_mapPlayerBotTowerRuns.find(raid.lInstance);
		if (run != s_mapPlayerBotTowerRuns.end())
			floor = run->second.iLevel + 2;
		sys_log(0, "PLAYERBOT_TOWER: raid over guild=%s id=%u phase=%d floor=%d after_min=%u why=%s",
				g ? g->GetName() : "?", raid.dwGuildID, (int)raid.bPhase, floor,
				(dwNow - raid.dwCalledAt) / 60000U, why);
		for (std::set<DWORD>::const_iterator it = raid.members.begin(); it != raid.members.end(); ++it)
		{
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(*it);
			if (st != s_mapPlayerBotAIStates.end() && st->second.lTowerInstance == 0)
				st->second.dwTowerRaidGuild = 0;
		}
		raid = TPlayerBotTowerRaid();
		s_dwNextPlayerBotTowerRaidTime = dwNow + PLAYERBOT_TOWER_INTERVAL;
	}

	// "Aktywuj teraz": the panel touches this file; a raid is called on the
	// next check if none is under way.
	bool PlayerBotTowerNowRequested()
	{
		struct stat st;
		if (stat(PLAYERBOT_TOWER_NOW_PATH, &st) != 0)
			return false;
		if (s_tPlayerBotTowerNowSeen == (time_t)-1)
		{
			// Whatever was there before this core started is not a request.
			s_tPlayerBotTowerNowSeen = st.st_mtime;
			return false;
		}
		if (st.st_mtime <= s_tPlayerBotTowerNowSeen)
			return false;
		s_tPlayerBotTowerNowSeen = st.st_mtime;
		return true;
	}

	int CountPlayerBotTowerMembersOnMap(long lMapIndex, int* pInstances = NULL)
	{
		int n = 0;
		if (pInstances)
			*pInstances = 0;
		for (std::set<DWORD>::const_iterator it = s_PlayerBotTowerRaid.members.begin();
				it != s_PlayerBotTowerRaid.members.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
			if (!ch)
				continue;
			if (ch->GetMapIndex() == lMapIndex)
				++n;
			if (pInstances && IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
				++*pInstances;
		}
		return n;
	}

	// The world's pass: the raid under way moved along, or the next one
	// called when its time has come.
	void ManagePlayerBotTowerRaids(DWORD dwNow)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		// One raid for the world, called on the first channel.
		if (g_bChannel != 1)
			return;
		if (s_dwNextPlayerBotTowerCheck != 0 && dwNow < s_dwNextPlayerBotTowerCheck)
			return;
		s_dwNextPlayerBotTowerCheck = dwNow + PLAYERBOT_TOWER_CHECK_INTERVAL;
		if (!IsPlayerBotMapHostedHere(PLAYERBOT_MAP_DEMON_TOWER))
			return;
		// A request is a touch after this core's first check, whether or not
		// the file already existed.
		if (s_tPlayerBotTowerNowSeen == (time_t)-1)
			s_tPlayerBotTowerNowSeen = time(NULL);
		const bool now = PlayerBotTowerNowRequested();

		// Runs whose instance is gone.
		for (std::map<long, TPlayerBotTowerRun>::iterator it = s_mapPlayerBotTowerRuns.begin();
				it != s_mapPlayerBotTowerRuns.end();)
		{
			if (CDungeonManager::instance().FindByMapIndex(it->first) == NULL)
			{
				++s_uPlayerBotTowerRunsFinished;
				sys_log(0, "PLAYERBOT_TOWER: run gone map=%ld floor=%d after_s=%u end=%s",
						it->first, it->second.iLevel + 2, (dwNow - it->second.dwEnteredAt) / 1000U, it->second.pszEnd);
				s_mapPlayerBotTowerScans.erase(it->first);
				s_mapPlayerBotTowerRuns.erase(it++);
			}
			else
				++it;
		}

		TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;
		if (raid.bPhase != TOWER_PHASE_NONE)
		{
			CGuild* g = CGuildManager::instance().FindGuild(raid.dwGuildID);
			if (!g)
			{
				EndPlayerBotTowerRaid(dwNow, "guild_gone");
				return;
			}
			int inside = 0;
			const int onGround = CountPlayerBotTowerMembersOnMap(PLAYERBOT_MAP_DEMON_TOWER, &inside);
			switch (raid.bPhase)
			{
				case TOWER_PHASE_GATHER:
					if (inside > 0)
					{
						// Somebody else broke the stone with our members beside it.
						raid.bPhase = TOWER_PHASE_INSIDE;
						raid.dwPhaseSince = dwNow;
					}
					else if (dwNow - raid.dwPhaseSince >= PLAYERBOT_TOWER_GATHER_MS ||
							onGround >= (int)raid.members.size())
					{
						raid.bPhase = TOWER_PHASE_STONE;
						raid.dwPhaseSince = dwNow;
						g->Chat("Rozbijamy Metin Twardosci!");
						sys_log(0, "PLAYERBOT_TOWER: raid breaking the stone guild=%s on_ground=%d of %u",
								g->GetName(), onGround, (unsigned int)raid.members.size());
					}
					break;
				case TOWER_PHASE_STONE:
					if (inside > 0)
					{
						raid.bPhase = TOWER_PHASE_INSIDE;
						raid.dwPhaseSince = dwNow;
					}
					else if (dwNow - raid.dwPhaseSince > PLAYERBOT_TOWER_STONE_TIMEOUT_MS)
						EndPlayerBotTowerRaid(dwNow, "stone_timeout");
					break;
				case TOWER_PHASE_INSIDE:
					if (inside == 0 && dwNow - raid.dwPhaseSince > PLAYERBOT_TOWER_CHECK_INTERVAL)
						EndPlayerBotTowerRaid(dwNow, "run_over");
					break;
				default:
					break;
			}
		}
		else
		{
			const bool enabled = IsPlayerBotTowerRaidsEnabled();
			if (s_dwNextPlayerBotTowerRaidTime == 0)
				s_dwNextPlayerBotTowerRaidTime = dwNow + PLAYERBOT_TOWER_FIRST_DELAY;
			if (now || (enabled && dwNow >= s_dwNextPlayerBotTowerRaidTime))
			{
				TPlayerBotTowerGuildEntry e;
				if (PickPlayerBotTowerGuild(dwNow, e))
					CallPlayerBotTowerRaid(e, dwNow, now ? "panel" : "clock");
				else
				{
					s_dwNextPlayerBotTowerRaidTime = dwNow + PLAYERBOT_TOWER_RETRY_MS;
					PlayerBotLogThrottled("tower_no_guild", dwNow,
							"PLAYERBOT_TOWER: no guild with %d bots of %d online, next try in %u min",
							PLAYERBOT_TOWER_MIN_MEMBERS, PLAYERBOT_TOWER_MIN_LEVEL,
							(unsigned int)(PLAYERBOT_TOWER_RETRY_MS / 60000U));
				}
			}
		}

		if (s_dwNextPlayerBotTowerCensus == 0 || dwNow >= s_dwNextPlayerBotTowerCensus)
		{
			s_dwNextPlayerBotTowerCensus = dwNow + PLAYERBOT_TOWER_CENSUS_INTERVAL;
			int inside = 0;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
				if (it->second.lTowerInstance != 0)
					++inside;
			sys_log(0, "PLAYERBOT_TOWER: census raids=%u runs_finished=%u best_floor=%d inside=%d phase=%d next_in_min=%d",
					s_uPlayerBotTowerRaids, s_uPlayerBotTowerRunsFinished, s_iPlayerBotTowerBestFloor, inside,
					(int)s_PlayerBotTowerRaid.bPhase,
					s_PlayerBotTowerRaid.bPhase != TOWER_PHASE_NONE || s_dwNextPlayerBotTowerRaidTime <= dwNow
						? 0 : (int)((s_dwNextPlayerBotTowerRaidTime - dwNow) / 60000U));
		}
#else
		(void)dwNow;
#endif
	}
}

#endif
