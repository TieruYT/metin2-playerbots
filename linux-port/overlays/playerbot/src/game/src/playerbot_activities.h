#ifndef __INC_METIN2_PLAYERBOT_ACTIVITIES_H__
#define __INC_METIN2_PLAYERBOT_ACTIVITIES_H__

// The things a bot does that are not fighting: raising a horse, and fishing.
//
// Both own the whole tick while they run - the rod sits in the weapon slot, and
// a bot walking to the stable is not hunting - which is why they read as
// separate activities rather than as steps inside the combat loop.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_movement.h (it walks) and playerbot_gear.h (it buys).

namespace
{
	// Defined in playerbot_town.h, which is included after this file - the same
	// forward-declaration shape town.h itself uses for AnnouncePlayerBotStall.
	// A town leg is not a walk: it asks CanReach first and finishes at the
	// nearest cell of the bot's own walkable component, which is the only thing
	// that gets a bot to an NPC standing on ground server_attr cuts off from
	// the square. Its contract is "have I arrived", so it answers false for
	// every tick of the walk itself.
	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance);

	BYTE GetPlayerBotNextHorseRequiredLevel(BYTE horseLevel)
	{
		if (horseLevel >= 21)
			return 255;
		if (horseLevel >= 20)
			return 50; // military horse milestone
		if (horseLevel >= 10)
			return 35; // combat horse milestone
		return PLAYERBOT_HORSE_REQUIRED_LEVEL;
	}

	// Iwakura's Jezdziec: a Grinder wants a horse for its speed and no more
	// ("odebrac konia na 1. poziomie - zalezy mu tylko na szybkosci
	// przemieszczania sie"), and the medals after the first are stock for a
	// counter; a Conqueror raises it to eleven and twenty-one, the horse it
	// fights from. A dropper keeps its own rules.
	bool IsPlayerBotGrinderRider(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		return it != s_mapPlayerBotAIStates.end() && it->second.persona.bRestored &&
				!it->second.persona.bAdvanced && !IsPlayerBotDropper(it->second.bPersonality);
	}

	bool CanPlayerBotAdvanceHorse(LPCHARACTER ch)
	{
		if (!ch || ch->GetHorseLevel() >= 21)
			return false;
		if (ch->GetHorseLevel() >= 1 && IsPlayerBotGrinderRider(ch))
			return false;
		// A horse at exactly ten is what the battle horse trial asks for, and one
		// more medal makes it eleven - after which no medal, quest or NPC in this
		// world will ever put it back. So a bot that could still win the battle
		// horse keeps its medals until the stable keeper has handed the scroll
		// over, which sets the horse to eleven itself and starts the ladder again.
		//
		// This also stops it farming medals it must not spend: every other caller
		// of this function - the Monkey Dungeon expedition, buying a medal off a
		// stall, the goal that walks it to the stable - reads the same answer and
		// leaves it free to be out in the desert earning the thing instead.
		if (IsPlayerBotBattleHorseCandidate(ch))
			return false;
		// The same shape one level up: a horse at exactly twenty is waiting on
		// the Demon Tower trial, not on another medal, so it does not go
		// collecting them - but once the trial is done it walks to the stable
		// like anybody with something to hand in.
		if (ch->GetHorseLevel() == PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL)
			return IsPlayerBotMilitaryHorseEarned(ch);
		return ch->GetLevel() >= GetPlayerBotNextHorseRequiredLevel(ch->GetHorseLevel());
	}

	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	// horse.advance() climbs down and back round the new level, so the rider sits
	// on the animal it has just become. The stable pass does the same on the one
	// tick a level changes and keeps the saddle for the rest of the visit, the
	// way CollectPlayerBotBattleHorse does.
	void SetPlayerBotHorseLevelInSaddle(LPCHARACTER ch, int level)
	{
		if (!ch || (int)ch->GetHorseLevel() >= level)
			return;
		const bool wasRiding = ch->IsRiding();
		if (wasRiding)
			ch->StopRiding();
		ch->SetHorseLevel(level);
		ch->ComputePoints();
		ch->SkillLevelPacket();
		if (wasRiding)
			ch->StartRiding();
	}

	bool ManagePlayerBotHorse(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// The stable keeper stands in all six villages, so the horse errand is a
		// local one wherever the bot lives.
		playerbot_empire_rules::TTownServices svc;
		if (!ch || !playerbot_empire_rules::GetTownServices(ch->GetMapIndex(), svc) ||
				state.bVisitingShop || state.bVisitingBiologist)
			return false;
		if (!state.bVisitingStable && dwNow < state.dwNextHorseCheckTime)
			return false;
		if (!state.bVisitingStable)
			state.dwNextHorseCheckTime = dwNow + 3000;

		const BYTE horseLevel = ch->GetHorseLevel();
		const bool bBattleHorseWaiting = IsPlayerBotBattleHorseEarned(ch) &&
				ch->GetGold() >= (int)PLAYERBOT_BATTLE_HORSE_FEE;
		if (!bBattleHorseWaiting && (!CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0))
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			return false;
		}

		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!state.bVisitingStable && victim && !victim->IsDead())
			return false;

		if (!state.bVisitingStable)
		{
			int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
			if (delivered < horseLevel)
			{
				delivered = horseLevel;
				ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
			}
			state.bVisitingStable = true;
			state.dwNextHorseActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_HORSE: going to stable pid=%u name=%s medals=%d horse_level=%u delivered=%d",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM), horseLevel, delivered);
		}

		SetPlayerBotGoal(ch, state, BOT_GOAL_HORSE, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_STABLE, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		const bool inM2 = IsPlayerBotM2Map(ch->GetMapIndex());
		const long stableX = svc.stableKeeper.x;
		const long stableY = svc.stableKeeper.y;
		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), stableX, stableY,
				inM2 ? 0x4d324853U : 0x484f5253U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > PLAYERBOT_STABLE_ARRIVE_DISTANCE)
		{
			// A town leg, not a bare walk. The stable keeper of Jinno's second
			// village stands on ground the square does not join, and a raw
			// MovePlayerBot has no answer to that: it plans, is told
			// "unreachable", and plans the identical route again. Measured on
			// this world: 582 refusals, every one of them map=43, from six bots
			// - two of which produced 554 between them while the other
			// kingdoms' stables were served 185 times and handed over 82
			// medals. The town leg moves the goal onto the bot's own component
			// and keeps the rescue for the cases a component lookup cannot see.
			if (!MovePlayerBotTownLeg(ch, state, dwNow, approachX, approachY,
						PLAYERBOT_STABLE_ARRIVE_DISTANCE) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingStable = false;
				state.dwNextHorseCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_HORSE: route failed pid=%u name=%s map=%ld from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		// The keeper talks to a rider; only the level itself is changed on foot
		// (SetPlayerBotHorseLevelInSaddle).
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextHorseActionTime == 0)
		{
			state.dwNextHorseActionTime = dwNow + number(5000, 15000);
			return true;
		}
		if (dwNow < state.dwNextHorseActionTime)
			return true;

		// The trial first: a bot that has earned the battle horse is here to
		// collect it, not to hand in a medal it does not have.
		if (CollectPlayerBotBattleHorse(ch))
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		// The military horse is collected here, before any medal is looked at: a
		// bot that finished the Demon Tower trial has nothing to hand in and
		// would otherwise be turned away by the medal check below and never get
		// its twenty-first level.
		if (IsPlayerBotMilitaryHorseEarned(ch))
		{
			SetPlayerBotHorseLevelInSaddle(ch, PLAYERBOT_MILITARY_HORSE_LEVEL);
			ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, PLAYERBOT_MILITARY_HORSE_LEVEL);
			ch->SetSkillLevel(131, 10);
			sys_log(0, "PLAYERBOT_HORSE: military horse granted pid=%u name=%s horse_level=%u kills=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHorseLevel(),
					GetPlayerBotMilitaryHorseKills(ch));
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(15000, 30000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		ch->RemoveSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM, 1);
		int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
		delivered = std::max(delivered, (int)ch->GetHorseLevel()) + 1;
		// Medals stop at twenty. The twenty-first level is the Demon Tower
		// trial's to give, not a medal's.
		delivered = std::min(delivered, (int)PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL);
		ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
		ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_DELIVERY_TIME_FLAG, get_global_time());
		SetPlayerBotHorseLevelInSaddle(ch, delivered);
		ch->SetSkillLevel(131, 10);

		const char* stage = delivered >= PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL
				? "military_trial_next" : (delivered >= 11 ? "combat" : "normal");
		sys_log(0, "PLAYERBOT_HORSE: medal delivered pid=%u name=%s delivered=%d horse_level=%u stage=%s medals_left=%d",
				ch->GetPlayerID(), ch->GetName(), delivered, ch->GetHorseLevel(), stage,
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM));

		if (delivered >= 21 || !CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		state.dwNextHorseActionTime = dwNow + number(5000, 10000);
		return true;
	}

	// A small, stable slice of the M1 population fishes. Careful collectors are the
	// natural anglers -- pearls are a collector's prize -- but a few other
	// personalities join them so the bank is never one archetype deep. The roll is
	// derived from the player id, so a bot keeps the same hobby across restarts.
	// The rod is a weapon and carries a level limit like any other. Asking the
	// item what it needs keeps this honest: the hand-picked level 10 was below
	// the rod's real requirement of 30, so a level-13 bot bought tackle it could
	// never equip and then retried the same failing step for good.
	bool CanPlayerBotUseFishingRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_ROD_VNUM);
		if (!proto)
			return false;
		return GetPlayerBotProtoLevelLimit(proto) <= (int)ch->GetLevel();
	}

	// Iwakura's Rybak, while the PERSONA switch is on: from level thirty, never
	// in a party ("jesli bot jest w PT nie powinien lowic"), for the hour a
	// capitulation sent it to the water, and otherwise by its mood - a SLABY
	// bot very likely gives up the grind for the bank, a NORMALNY one now and
	// then, and a BARDZO DOBRY one does not. Rolled once per window per bot.
	bool IsPlayerBotRybakNow(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetParty() || (int)ch->GetLevel() < playerbot_persona::PK_FISHING_MIN_LEVEL)
			return false;
		const TPlayerBotPersona& p = state.persona;
		if (p.dwFishingSpellUntil != 0 && dwNow < p.dwFishingSpellUntil)
			return true;
		const DWORD roll = PlayerBotNavHash(ch->GetPlayerID() ^ 0x5259424BU ^
				(dwNow / PLAYERBOT_RYBAK_ROLL_WINDOW_MS));
		switch (p.mood.mood)
		{
			case playerbot_persona::MOOD_SLABY:
				return PlayerBotWeightedRoll(roll % 100U, PLAYERBOT_RYBAK_SLABY_PERCENT,
						PLAYERBOT_WEIGHT_FISHING);
			case playerbot_persona::MOOD_NORMALNY:
				return PlayerBotWeightedRoll(roll % 1000U, PLAYERBOT_RYBAK_NORMALNY_PERMILLE,
						PLAYERBOT_WEIGHT_FISHING);
			default:
				return false;
		}
	}

	// How long this Rybak stays at the water: the capitulation's hour to its
	// end, a bad mood half an hour to an hour, a good one a short episode -
	// never more than the hour the document allows.
	DWORD GetPlayerBotRybakSessionMs(const TPlayerBotAIState& state, DWORD dwNow)
	{
		const TPlayerBotPersona& p = state.persona;
		DWORD length;
		if (p.dwFishingSpellUntil != 0 && dwNow < p.dwFishingSpellUntil)
			length = p.dwFishingSpellUntil - dwNow;
		else if (p.mood.mood == playerbot_persona::MOOD_SLABY)
			length = (DWORD)number(PLAYERBOT_RYBAK_SLABY_SESSION_MIN, PLAYERBOT_RYBAK_SLABY_SESSION_MAX);
		else
			length = (DWORD)number(PLAYERBOT_RYBAK_EPISODE_MIN, PLAYERBOT_RYBAK_EPISODE_MAX);
		return std::min<DWORD>(length, PLAYERBOT_RYBAK_MAX_SESSION);
	}

	bool IsPlayerBotAngler(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		// A dropper is no angler: a session is a stay on a bank in the first
		// village, away from the one thing it farms.
		if (IsPlayerBotDropper(state.bPersonality) || !CanPlayerBotUseFishingRod(ch))
			return false;
		if (IsPlayerBotPersonaEnabled() && state.persona.bRestored)
			return IsPlayerBotRybakNow(ch, state, get_dword_time());
		const DWORD roll = PlayerBotNavHash(ch->GetPlayerID() ^ 0x46495348U) % 100U;
		// Thirty collectors in a hundred and eight of everyone else, stretched or
		// shrunk by the FISHING weight.
		//
		// Raised from 20/2 because fishing is the one errand that takes a bot to
		// Joan and keeps it there: the bank, the Fisherman who sells the bait and
		// the market ring are all on map 21, so an angler is a customer, a
		// passer-by and a stall in one. Joan looked deserted with nineteen of
		// eight hundred live bots standing on its map, and half of those nineteen
		// were the anglers.
		const int chance = state.bPersonality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 30 : 8;
		return PlayerBotWeightedRoll(roll, chance, PLAYERBOT_WEIGHT_FISHING);
	}

	// Which angler stands where. A stand is claimed for the session and released
	// with it, because a hash cannot promise what the Discord asked for: fifty
	// anglers drawing from fifty slots collide by the birthday problem long
	// before they fill them, and what that looks like in game is a heap.
	//
	// The engine constrains none of this. CHARACTER::fishing() tests only the
	// cell the angler is standing on; it computes a point four hundred units in
	// front of the character and then never reads it. So the bank is chosen to
	// look right - standable ground with the river in front - and the spacing is
	// a metre because that is what was asked for.
	struct TPlayerBotFishingStand
	{
		DWORD dwPid;
		DWORD dwTouched;
	};
	std::map<int, TPlayerBotFishingStand> s_mapPlayerBotFishingStands;
	// Stands the engine refused as dry. The tables were measured on one
	// engine's server_attr and the other engine's map of the same village
	// differs by a cell here and there: Joan's stand (67175,158125) has no
	// water beside it on mt2009, and the bot that drew it stood there session
	// after session to "never_cast" while its neighbour caught fish. A stand
	// found dry is given up for good on this core, and the claim search skips
	// it.
	std::set<int> s_setPlayerBotDryFishingStands;
#if defined(PLAYERBOT_ENGINE_MT2009)
	// again: the stand was in the dry set already - the bot was handed a dry
	// stand a second time, which only happens once every stand of the bank is.
	int MarkPlayerBotFishingStandDry(DWORD playerID, bool& again)
	{
		again = false;
		for (std::map<int, TPlayerBotFishingStand>::iterator it =
				s_mapPlayerBotFishingStands.begin();
				it != s_mapPlayerBotFishingStands.end(); ++it)
		{
			if (it->second.dwPid == playerID)
			{
				const int key = it->first;
				again = !s_setPlayerBotDryFishingStands.insert(key).second;
				s_mapPlayerBotFishingStands.erase(it);
				return key;
			}
		}
		return -1;
	}
#endif

	void ReleasePlayerBotFishingStand(DWORD playerID)
	{
		for (std::map<int, TPlayerBotFishingStand>::iterator it =
				s_mapPlayerBotFishingStands.begin();
				it != s_mapPlayerBotFishingStands.end(); ++it)
		{
			if (it->second.dwPid == playerID)
			{
				s_mapPlayerBotFishingStands.erase(it);
				return;
			}
		}
	}

	// Slot ids are per map: three banks numbering their stands from zero would
	// have an angler in Yongan holding Joan's stand seven.
	int PlayerBotFishingClaimKey(long mapIndex, int slot)
	{
		return (int)mapIndex * 1000 + slot;
	}

	void GetPlayerBotFishingStand(DWORD playerID, DWORD dwNow, long mapIndex,
			long& standX, long& standY)
	{
		const TPlayerBotFishingBank* bank = GetPlayerBotFishingBank(mapIndex);
		if (!bank)
			return;
		const int slots = (int)bank->standCount;
		int mine = -1;
		for (std::map<int, TPlayerBotFishingStand>::iterator it =
				s_mapPlayerBotFishingStands.begin();
				it != s_mapPlayerBotFishingStands.end(); ++it)
		{
			if (it->second.dwPid == playerID)
			{
				mine = it->first;
				it->second.dwTouched = dwNow;
				break;
			}
		}
		if (mine < 0)
		{
			// From its own place in the row, then along it: the same bot comes
			// back to the same stand session after session while the bank is
			// empty, and takes the next free one when it is not.
			const int start = (int)(PlayerBotNavHash(playerID ^ 0x42414e4bU) % (DWORD)slots);
			for (int step = 0; step < slots && mine < 0; ++step)
			{
				const int slot = PlayerBotFishingClaimKey(mapIndex, (start + step) % slots);
				if (s_setPlayerBotDryFishingStands.count(slot))
					continue;
				std::map<int, TPlayerBotFishingStand>::const_iterator it =
						s_mapPlayerBotFishingStands.find(slot);
				if (it == s_mapPlayerBotFishingStands.end() ||
						dwNow - it->second.dwTouched >= PLAYERBOT_FISHING_STAND_CLAIM)
					mine = slot;
			}
			// More anglers than stands one day: share a stand rather than refuse
			// to fish - a wet one, though. Sharing slot `start` whatever it was
			// handed a bot the very stand it had just marked dry, once a second
			// for the whole idle timeout (xXxMotykaxXx on Yongan, 17 September:
			// 120 s of "dry stand ... moving to another" on one key, then
			// never_cast).
			for (int step = 0; step < slots && mine < 0; ++step)
			{
				const int slot = PlayerBotFishingClaimKey(mapIndex, (start + step) % slots);
				if (!s_setPlayerBotDryFishingStands.count(slot))
					mine = slot;
			}
			if (mine < 0)
				mine = PlayerBotFishingClaimKey(mapIndex, start);
			TPlayerBotFishingStand& claim = s_mapPlayerBotFishingStands[mine];
			claim.dwPid = playerID;
			claim.dwTouched = dwNow;
		}
		const int index = mine - PlayerBotFishingClaimKey(mapIndex, 0);
		if (index < 0 || index >= slots)
			return;
		standX = bank->stands[index].x;
		standY = bank->stands[index].y;
	}

	// The water this stand looks at. Due east was right for the one straight
	// stretch the first version knew about and wrong for every bend.
	void GetPlayerBotFishingFacing(DWORD playerID, long mapIndex,
			long& waterX, long& waterY)
	{
		const TPlayerBotFishingBank* bank = GetPlayerBotFishingBank(mapIndex);
		waterX = bank ? bank->centre.x : PLAYERBOT_FISHING_WATER_X;
		waterY = 0;
		if (!bank)
			return;
		for (std::map<int, TPlayerBotFishingStand>::const_iterator it =
				s_mapPlayerBotFishingStands.begin();
				it != s_mapPlayerBotFishingStands.end(); ++it)
		{
			if (it->second.dwPid != playerID)
				continue;
			const int index = it->first - PlayerBotFishingClaimKey(mapIndex, 0);
			if (index < 0 || index >= (int)bank->standCount)
				return;
			waterX = bank->stands[index].waterX;
			waterY = bank->stands[index].waterY;
			return;
		}
	}

	bool IsPlayerBotHoldingRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return rod && rod->GetType() == ITEM_ROD;
	}

	// A bot stops walking at PLAYERBOT_NAV_ARRIVAL_DISTANCE from its goal, so an
	// arrival test tighter than that can never pass: the walk reports success,
	// the caller asks for another step, nothing moves, and the bot stands in the
	// gap with no failure recorded anywhere. Both files are included here, so
	// the rule can be checked rather than remembered.
	static_assert(PLAYERBOT_FISHING_ARRIVE >= PLAYERBOT_NAV_ARRIVAL_DISTANCE,
			"an arrival radius below the navigation's own strands the bot short of it");

	// Rods in the bag, whatever their grade. The engine refines a rod as it
	// is fished with (fishing.cpp: a roll per catch, and the rod becomes its
	// GetRefinedVnum, a new item), so a bot's Wedka+1 is a Wedka+2 after a
	// session and CountSpecifyItem(27400) says none: three of five anglers in
	// Joan had bought rods until the bag was full of them.
	int CountPlayerBotRods(LPCHARACTER ch)
	{
		int rods = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_ROD)
				++rods;
		}
		return rods;
	}

	bool EquipPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotHoldingRod(ch))
			return true;

		// The best rod in the bag: the grades are consecutive vnums, so the
		// highest vnum is the most refined one.
		LPITEM best = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_ROD && (!best || item->GetVnum() > best->GetVnum()))
				best = item;
		}
		if (!best)
			return false;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && !ch->UnequipItem(worn))
			return false;
		if (PlayerBotEquipItem(ch, best))
		{
			sys_log(0, "PLAYERBOT_FISHING: rod equipped pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), best->GetVnum());
			return true;
		}
		return false;
	}

	// The rod is worthless in a fight, so a finished session always puts the real
	// weapon back before the bot rejoins the grind.
	void StowPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return;
		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!rod || rod->GetType() != ITEM_ROD)
			return;
		if (!ch->UnequipItem(rod))
			return;
		EquipFirstAvailablePlayerBotWeapon(ch);
	}

	// Bait does not sit in the pouch while fishing: using it moves its value into
	// the rod's socket 2, which is what the engine actually checks before a cast.
	bool BaitPlayerBotRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		if (!rod || rod->GetType() != ITEM_ROD)
			return false;
		if (rod->GetSocket(2) != 0)
			return true;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_FISHING_BAIT_VNUM)
				continue;
			ch->UseItem(TItemPos(INVENTORY, cell));
			return rod->GetSocket(2) != 0;
		}
		return false;
	}

	// One item per pass. Gutting a fish and prying a shell open both run through
	// UseItem, which frees the very inventory slot being iterated over.
	// Defined in playerbot_economy.h, which this file precedes.
	bool PlayerBotNeedsRefineMaterial(LPCHARACTER ch, DWORD materialVnum);
	int GetPlayerBotRefineMaterialReserve(LPCHARACTER ch, DWORD materialVnum);

	// The Rybak's batch of shells being opened, by pid: five at a time.
	std::map<DWORD, int> s_mapPlayerBotShellBatch;

	// What a thing is worth to sell: what the market has paid for it, else a
	// fifth of the shop price, which is what the merchant pays.
	long long GetPlayerBotVnumSaleValue(DWORD vnum, DWORD dwNow)
	{
		size_t samples = 0;
		const DWORD paid = GetPlayerBotSaleUnitPrice(vnum, 0, dwNow, &samples);
		if (paid != 0)
			return (long long)paid;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		return proto ? (long long)(proto->dwShopBuyPrice / 5) : 0;
	}

	// Prying a shell open is a bet, not a step: half the time a Stone Piece,
	// a third of the time nothing, a pearl the rest. Open it when what the bet
	// pays on average beats what the shell sells for whole; the careful
	// collector wants the bet to pay half again as much, the gear specialist
	// takes a slightly worse one for the pearls it is after.
	bool ShouldPlayerBotOpenShellfish(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		const long long expected =
				(long long)GetPlayerBotShellfishPermille(PLAYERBOT_SHELL_STONE, PLAYERBOT_SHELLFISH_STONE_PERMILLE) *
						GetPlayerBotVnumSaleValue(PLAYERBOT_STONE_PIECE_VNUM, dwNow) +
				(long long)GetPlayerBotShellfishPermille(PLAYERBOT_SHELL_WHITE, PLAYERBOT_SHELLFISH_WHITE_PERMILLE) *
						GetPlayerBotVnumSaleValue(PLAYERBOT_PEARL_FIRST_VNUM, dwNow) +
				(long long)GetPlayerBotShellfishPermille(PLAYERBOT_SHELL_BLUE, PLAYERBOT_SHELLFISH_BLUE_PERMILLE) *
						GetPlayerBotVnumSaleValue(PLAYERBOT_PEARL_FIRST_VNUM + 1, dwNow) +
				(long long)GetPlayerBotShellfishPermille(PLAYERBOT_SHELL_RED, PLAYERBOT_SHELLFISH_RED_PERMILLE) *
						GetPlayerBotVnumSaleValue(PLAYERBOT_PEARL_LAST_VNUM, dwNow);
		const long long whole = GetPlayerBotVnumSaleValue(PLAYERBOT_SHELLFISH_VNUM, dwNow) * 1000;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = it != s_mapPlayerBotAIStates.end()
				? it->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		const long long needed = personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? whole * 3 / 2
				: (personality == BOT_PERSONALITY_GEAR_SPECIALIST ? whole * 4 / 5 : whole);
		return expected >= needed;
	}

	// The campfire mob this bot lit, if it is still burning within reach.
	struct FPlayerBotFindCampfire
	{
		LPCHARACTER m_owner;
		LPCHARACTER m_found;
		int m_bestDistance;
		FPlayerBotFindCampfire(LPCHARACTER owner) : m_owner(owner), m_found(NULL), m_bestDistance(PLAYERBOT_BAKE_RANGE) {}
		void operator()(LPENTITY entity)
		{
			if (!entity || !entity->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER fire = static_cast<LPCHARACTER>(entity);
			if (fire->GetRaceNum() != PLAYERBOT_CAMPFIRE_MOB_VNUM)
				return;
			const int distance = DISTANCE_APPROX(fire->GetX() - m_owner->GetX(), fire->GetY() - m_owner->GetY());
			if (distance < m_bestDistance)
			{
				m_bestDistance = distance;
				m_found = fire;
			}
		}
	};

	// Upgrading the rod, which on a human's server is the Fisherman's service.
	//
	// fishing.cpp has both halves and the bots only ever ran one: every catch
	// rolls 1-in-`GetValue(1)` for a point in socket 0, up to `GetValue(2)`,
	// and at that ceiling `fishing::RefinableRod` says the rod may be upgraded.
	// Nothing ever asked. Measured before this: 62 of the world's 64 rods sat
	// at exactly ten points - full, for ever - because `RealRefineRod` is
	// reached only from a GM command and from a quest dialog a bot cannot open.
	//
	// Reimplemented rather than called, the way `CollectPlayerBotBattleHorse`
	// reimplements the stable keeper's quest. The numbers are the item's own:
	// `GetValue(3)` is the percentage and `GetValue(4)` is what a failure
	// leaves behind. +0 is free - a hundred percent, and its failure row points
	// back at itself - while +1 upwards is a real gamble (88, 77, 66, 55) that
	// drops the rod a grade. That is the Fisherman's arithmetic and not ours to
	// soften.
	bool UpgradePlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM rod = NULL;
		// RefinableRod refuses a rod in the hand, so a worn one comes off first
		// - and only when it is actually ready, or a session would be
		// interrupted for nothing.
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetType() == ITEM_ROD && worn->GetRefinedVnum() > 0 &&
				worn->GetSocket(0) >= worn->GetValue(2))
		{
			// The engine's UnequipItem does not ask for room itself, and the new
			// rod is put in the old one's cell: a rod that stayed in the hand
			// would hand the slot's cell to an item nobody equipped.
			if (ch->GetEmptyInventory(worn->GetSize()) < 0 || !ch->UnequipItem(worn) ||
					worn->IsEquipped())
				return false;
			rod = worn;
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !rod; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_ROD && !item->IsEquipped() &&
					item->GetRefinedVnum() > 0 &&
					item->GetSocket(0) >= item->GetValue(2))
				rod = item;
		}
		if (!rod)
			return false;

		if (rod->GetWindow() != INVENTORY || rod->GetCell() >= PLAYERBOT_BAG_CELLS)
			return false;
		const DWORD oldVnum = rod->GetVnum();
		const BYTE bCell = rod->GetCell();
		const int chance = rod->GetValue(3);
		const DWORD nextVnum = number(1, 100) <= chance
				? rod->GetRefinedVnum() : (DWORD)rod->GetValue(4);
		if (nextVnum == 0)
			return false;
		LPITEM fresh = ITEM_MANAGER::instance().CreateItem(nextVnum, 1);
		if (!fresh)
			return false;
		const bool won = nextVnum > oldVnum;
		ITEM_MANAGER::instance().RemoveItem(rod, "REMOVE (REFINE FISH_ROD)");
		fresh->AddToCharacter(ch, TItemPos(INVENTORY, bCell));
		LogManager::instance().ItemLog(ch, fresh,
				won ? "REFINE FISH_ROD SUCCESS" : "REFINE FISH_ROD FAIL", fresh->GetName());
		sys_log(0, "PLAYERBOT_FISHING: rod upgrade pid=%u name=%s %u -> %u chance=%d %s",
				ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, chance,
				won ? "SUCCESS" : "FAIL");
		return true;
	}

	int CountPlayerBotDeadFish(LPCHARACTER ch)
	{
		int count = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_FISH && item->GetSubType() == FISH_DEAD)
				count += item->GetCount();
		}
		return count;
	}

	// Light the fire at the end of a session with dead fish in the bag; the
	// engine does the rest once the fish are handed over. Returns whether a
	// fire was lit.
	bool LightPlayerBotCampfire(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// The threshold moved here from the purchase, where it could never be
		// met. One log grills any number of fish, so it is worth lighting for a
		// session's catch and not for a single fish.
		if (!ch || CountPlayerBotDeadFish(ch) < PLAYERBOT_BAKE_MIN_FISH)
			return false;

		// Turn your back on the river before striking a light.
		//
		// char_item.cpp's ITEM_CAMPFIRE measures the tile a hundred units ahead
		// of the character's own rotation - GetDeltaByDegree(GetRotation(), 100)
		// - and refuses ATTR_WATER outright ("You cannot build a campfire under
		// water"). An angler is pointed straight at the water by the session
		// that has just ended, so every log went into the river: 23 bought on
		// this world and not one fire lit, with the refusal invisible because
		// the engine explains it to a client the bot does not have.
		//
		// The mirror of the water point across the bot is the bank it is
		// standing on, which is dry by construction. waterY of zero means the
		// bank table had no row for this stand, and the session reads that as
		// "keep your own Y"; mirroring has to read it the same way.
		long waterX = 0, waterY = 0;
		GetPlayerBotFishingFacing(ch->GetPlayerID(), ch->GetMapIndex(), waterX, waterY);
		if (waterX != 0 || waterY != 0)
			ch->SetRotationToXY(2 * ch->GetX() - waterX,
					waterY != 0 ? 2 * ch->GetY() - waterY : ch->GetY());

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_CAMPFIRE_VNUM)
				continue;
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
			{
				// This function used to speak only on success, so a refusal was
				// indistinguishable from never having been called: nine logs in
				// the world, a bot holding one beside eight dead fish, and no
				// fire. Name the refusal - it is what found the bait purchase in
				// two minutes.
				PlayerBotLogThrottled("campfire_refused", dwNow,
						"PLAYERBOT_FISHING: campfire refused pid=%u name=%s cell=%u riding=%d fish=%d",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)cell,
						ch->IsRiding() ? 1 : 0, CountPlayerBotDeadFish(ch));
				return false;
			}
			state.dwBakeUntil = dwNow + PLAYERBOT_BAKE_WINDOW;
			sys_log(0, "PLAYERBOT_FISHING: campfire lit pid=%u name=%s dead_fish=%d",
					ch->GetPlayerID(), ch->GetName(), CountPlayerBotDeadFish(ch));
			return true;
		}
		// Enough fish to be worth a fire and no log in the bag. Said once a
		// minute for the whole population, because the answer is a purchase the
		// restock makes on the next trip to the Fisherman, not a fault.
		PlayerBotLogThrottled("campfire_no_wood", dwNow,
				"PLAYERBOT_FISHING: no campfire wood pid=%u name=%s fish=%d",
				ch->GetPlayerID(), ch->GetName(), CountPlayerBotDeadFish(ch));
		return false;
	}

	// Hand the dead fish to the fire, one pass a tick, while it burns. Owns
	// the tick so the bot stands by its fire instead of walking off.
	bool BakePlayerBotFish(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.dwBakeUntil == 0)
			return false;
		if (dwNow >= state.dwBakeUntil || !ch->GetSectree())
		{
			state.dwBakeUntil = 0;
			return false;
		}
		FPlayerBotFindCampfire finder(ch);
		ch->GetSectree()->ForEachAround(finder);
		if (!finder.m_found)
			return true; // lit a moment ago, not in the sectree yet
		int baked = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_FISH || item->GetSubType() != FISH_DEAD)
				continue;
			const int count = item->GetCount();
			if (ch->GiveItem(finder.m_found, TItemPos(INVENTORY, cell)))
				baked += count;
		}
		if (CountPlayerBotDeadFish(ch) == 0)
		{
			sys_log(0, "PLAYERBOT_FISHING: baked pid=%u name=%s fish=%d", ch->GetPlayerID(), ch->GetName(), baked);
			state.dwBakeUntil = 0;
			return false;
		}
		return true;
	}

	bool ProcessPlayerBotCatch(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD vnum = item->GetVnum();
			const bool aliveFish = item->GetType() == ITEM_FISH &&
					item->GetSubType() == FISH_ALIVE;
			if (!aliveFish && vnum != PLAYERBOT_SHELLFISH_VNUM)
				continue;
			// A shellfish is two things: a shot at a pearl, and a refine material
			// in its own right - twenty-six recipes on this proto consume one as
			// it is. Prying open the one the bot's own anvil is about to ask for
			// trades a certain material for a chance at a different one.
			// A shell is worth something whole, so the first few are never
			// gambled with: they go to the anvil or onto the counter, and only
			// the surplus is pried open.
			// Iwakura's Rybak opens them in fives for the pearls ("bot otwiera
			// co 5 Malz w celu zdobycia perly"), whatever the market says a
			// shell is worth - but never the ones its own anvil keeps back.
			if (vnum == PLAYERBOT_SHELLFISH_VNUM && IsPlayerBotPersonaEnabled())
			{
				int& batch = s_mapPlayerBotShellBatch[ch->GetPlayerID()];
				const int spare = (int)ch->CountSpecifyItem(PLAYERBOT_SHELLFISH_VNUM) -
						GetPlayerBotRefineMaterialReserve(ch, PLAYERBOT_SHELLFISH_VNUM);
				if (batch <= 0 && spare >= PLAYERBOT_RYBAK_SHELL_BATCH)
					batch = PLAYERBOT_RYBAK_SHELL_BATCH;
				if (batch <= 0 || spare <= 0)
					continue;
				--batch;
			}
			else if (vnum == PLAYERBOT_SHELLFISH_VNUM &&
					(ch->CountSpecifyItem(PLAYERBOT_SHELLFISH_VNUM) <= PLAYERBOT_SHELLFISH_KEEP ||
					 PlayerBotNeedsRefineMaterial(ch, vnum) ||
					 !ShouldPlayerBotOpenShellfish(ch, get_dword_time())))
				continue;
			const int stoneBefore = ch->CountSpecifyItem(PLAYERBOT_STONE_PIECE_VNUM);
			const int whiteBefore = ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM);
			const int blueBefore = ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM + 1);
			const int redBefore = ch->CountSpecifyItem(PLAYERBOT_PEARL_LAST_VNUM);
			const int shellBefore = ch->CountSpecifyItem(PLAYERBOT_SHELLFISH_VNUM);
			const int boneBefore = ch->CountSpecifyItem(PLAYERBOT_FISH_BONE_VNUM);
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;
			// A shell or a bone out of a fish, a pearl out of a shell: the
			// document's valuables, and an angler's cure for a SLABY mood
			// ("nastroj natychmiast poprawia sie", playerbot_mood.h).
			if (aliveFish)
			{
				if (ch->CountSpecifyItem(PLAYERBOT_SHELLFISH_VNUM) > shellBefore)
					NotePlayerBotMoodValuable(ch, PLAYERBOT_SHELLFISH_VNUM, 0, ITEM_USE, "fish");
				else if (ch->CountSpecifyItem(PLAYERBOT_FISH_BONE_VNUM) > boneBefore)
					NotePlayerBotMoodValuable(ch, PLAYERBOT_FISH_BONE_VNUM, 0, ITEM_MATERIAL, "fish");
			}
			else
			{
				for (DWORD pearl = PLAYERBOT_PEARL_FIRST_VNUM; pearl <= PLAYERBOT_PEARL_LAST_VNUM; ++pearl)
				{
					const int before = pearl == PLAYERBOT_PEARL_FIRST_VNUM ? whiteBefore
							: (pearl == PLAYERBOT_PEARL_LAST_VNUM ? redBefore : blueBefore);
					if (ch->CountSpecifyItem(pearl) > before)
					{
						NotePlayerBotMoodValuable(ch, pearl, 0, ITEM_MATERIAL, "shell");
						break;
					}
				}
			}
			if (!aliveFish)
			{
				// What the shell held, counted whatever it was - the empty ones
				// are what keeps the population's estimate honest.
				int outcome = PLAYERBOT_SHELL_NOTHING;
				if (ch->CountSpecifyItem(PLAYERBOT_PEARL_LAST_VNUM) > redBefore)
					outcome = PLAYERBOT_SHELL_RED;
				else if (ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM + 1) > blueBefore)
					outcome = PLAYERBOT_SHELL_BLUE;
				else if (ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM) > whiteBefore)
					outcome = PLAYERBOT_SHELL_WHITE;
				else if (ch->CountSpecifyItem(PLAYERBOT_STONE_PIECE_VNUM) > stoneBefore)
					outcome = PLAYERBOT_SHELL_STONE;
				RememberPlayerBotShellfishOutcome(outcome);
			}

			sys_log(0, "PLAYERBOT_FISHING: processed catch pid=%u name=%s vnum=%u kind=%s",
					ch->GetPlayerID(), ch->GetName(), vnum,
					aliveFish ? "fish" : "shellfish");
			return true;
		}
		return false;
	}

	// One bot, one colour, for good.
	//
	// The dye is fished up and dropped often enough that bots were carrying it
	// about as scrap. The engine takes it straight from UseItem - SetPart on
	// PART_HAIR, no client involved - and the colour is permanent, which is
	// exactly why it is worth using: eight hundred characters that all look
	// alike stop looking like one character copied eight hundred times. Used
	// once and once only; everything after the first is goods, and the engine
	// would refuse a second one for three levels anyway.
	bool ManagePlayerBotHairDye(LPCHARACTER ch)
	{
		if (!ch || ch->GetPart(PART_HAIR) != 0)
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			// Only the range char_item.cpp answers for, and not the remover:
			// washing out a colour that was never applied consumes the item and
			// changes nothing.
			if (vnum <= PLAYERBOT_HAIR_DYE_FIRST_VNUM ||
					vnum > PLAYERBOT_HAIR_DYE_LAST_VNUM)
				continue;
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;
			sys_log(0, "PLAYERBOT_LOOK: hair dyed pid=%u name=%s vnum=%u part=%d",
					ch->GetPlayerID(), ch->GetName(), vnum, ch->GetPart(PART_HAIR));
			return true;
		}
		return false;
	}

	// Whether this dye from the water is one of the few kept for a counter
	// (PLAYERBOT_HAIR_DYE_KEEP_PERMILLE). The id is the item's own, or an
	// offline counter line's, which is the same item.
	bool IsPlayerBotHairDyeKeptForSaleId(DWORD itemId)
	{
		return (int)(PlayerBotNavHash(itemId ^ 0x44594553U) % 1000U) < PLAYERBOT_HAIR_DYE_KEEP_PERMILLE;
	}

	bool IsPlayerBotHairDyeKeptForSale(LPITEM item)
	{
		return item && IsPlayerBotHairDyeKeptForSaleId(item->GetID());
	}

	// Iwakura's Rybak sells the water's rubbish to the Fisherman instead,
	// once the bag is PLAYERBOT_RYBAK_JUNK_SELL_PERCENT full ("sprzedawane u
	// rybaka jesli ekwipunek bedzie zapelniony przynajmniej w 70%").
	bool IsPlayerBotBagFullForFishingJunk(LPCHARACTER ch)
	{
		return ch && (PLAYERBOT_BAG_CELLS - CountPlayerBotFreeInventoryCells(ch)) * 100 >=
				PLAYERBOT_BAG_CELLS * PLAYERBOT_RYBAK_JUNK_SELL_PERCENT;
	}

	// Thrown away, as most players throw theirs: every dye from the water but
	// one colour for a bot whose hair has none yet (ManagePlayerBotHairDye
	// uses it) and the few kept for a counter. The remover is never used, so
	// it is never the one kept. An item the operator gave a word to is his.
	// With `sell`, the same dyes go to the NPC for a fifth of their price, the
	// merchant's rate - the Rybak's way.
	int DiscardPlayerBotFishedDyes(LPCHARACTER ch, bool sell = false)
	{
		if (!ch || !ch->IsItemLoaded())
			return 0;
		bool keepOneColour = ch->GetPart(PART_HAIR) == 0;
		int thrown = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->isLocked() ||
					!IsPlayerBotFishedHairDye(item->GetVnum()) ||
					GetPlayerBotItemPolicy(item) != PLAYERBOT_ITEM_POLICY_NONE)
				continue;
			if (keepOneColour && item->GetVnum() > PLAYERBOT_HAIR_DYE_FIRST_VNUM)
			{
				keepOneColour = false;
				continue;
			}
			if (IsPlayerBotHairDyeKeptForSale(item))
				continue;
			const int count = std::max<int>(1, item->GetCount());
			thrown += count;
			if (sell)
			{
				DWORD price = item->GetShopBuyPrice();
				if (price == 0)
					price = item->GetProto() ? item->GetProto()->dwGold : 100;
				PlayerBotChangeGold(ch, (long long)std::max<DWORD>(10, price / 5) * count);
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_SHOP_SELL");
			}
			else
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DISCARD");
		}
		if (thrown > 0)
			PlayerBotLogThrottled("dye_discard", get_dword_time(),
					"PLAYERBOT_LOOK: %s hair dye pid=%u name=%s count=%d",
					sell ? "sold" : "threw away", ch->GetPlayerID(), ch->GetName(), thrown);
		return thrown;
	}

	bool EndPlayerBotFishingSession(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		if (ch && ch->m_pkFishingEvent)
			ch->fishing_take();

		if (ch)
			ReleasePlayerBotFishingStand(ch->GetPlayerID());
		state.bFishingSession = false;
		state.bIsFishing = false;
		state.dwFishingCastTime = 0;
		state.dwFishingIdleSince = 0;
		state.dwFishingSessionEndTime = 0;
		state.dwNextFishingActionTime = 0;
		state.dwNextFishingCheckTime = dwNow +
				number(PLAYERBOT_FISHING_REST_MIN, PLAYERBOT_FISHING_REST_MAX);
		StowPlayerBotRod(ch);
		// The Rybak sells them to the Fisherman beside the bank, and only from
		// a bag seventy percent full; without the switch they are thrown away.
		if (!IsPlayerBotPersonaEnabled())
			DiscardPlayerBotFishedDyes(ch);
		else if (IsPlayerBotBagFullForFishingJunk(ch))
			DiscardPlayerBotFishedDyes(ch, true);
		ClearPlayerBotRoute(state, true);
		// An angler that has just packed the rod away is the one bot reliably
		// standing in Joan with nothing left to do. Half of them wander over to
		// the market ring for a while instead of walking straight back out -
		// which is the whole of what makes that square look inhabited, since the
		// bank, the bait merchant and the stalls are all on this one map.
		if (RollPlayerBotTownRest(ch))
			state.dwTownLingerUntil = dwNow + number(
					(int)PLAYERBOT_TOWN_LINGER_MIN, (int)PLAYERBOT_TOWN_LINGER_MAX);
		if (ch)
			sys_log(0, "PLAYERBOT_FISHING: session over pid=%u name=%s pearls=%d/%d/%d reason=%s",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM + 1),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_LAST_VNUM),
					reason ? reason : "?");
		return false;
	}

	// Rod and bait both come from the Rybak, who stands on the bank the bots fish
	// from, so restocking and fishing share one walk.
	// Buying one thing from the Rybak, and saying out loud when it will not
	// happen. Three quite different failures used to leave by the same door and
	// arrive as "cannot_afford_tackle": an item this world does not price, a bot
	// that genuinely has no money, and a bot whose bag has no free cell. The
	// first is a serverfile question, the second fixes itself, the third is a
	// bag the merchant pass should have emptied - and no log told them apart.
	bool BuyPlayerBotTackleItem(LPCHARACTER ch, DWORD vnum, int count,
			const char* what, DWORD dwNow)
	{
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto)
		{
			PlayerBotLogThrottled("tackle_no_proto", dwNow,
					"PLAYERBOT_FISHING: %s has no item table pid=%u name=%s vnum=%u",
					what, ch->GetPlayerID(), ch->GetName(), vnum);
			return false;
		}
		long long price = GetPlayerBotNpcPurchasePrice(proto, count);
		if (price <= 0)
		{
			PlayerBotLogThrottled("tackle_no_price", dwNow,
					"PLAYERBOT_FISHING: %s has no price on this world pid=%u name=%s vnum=%u count=%d",
					what, ch->GetPlayerID(), ch->GetName(), vnum, count);
			return false;
		}
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, what);
		// Buy what the purse reaches rather than nothing at all. A bundle is
		// twenty worms for eight hundred yang and the bot was refusing the whole
		// purchase over the last few: caught on our own world with 556 yang in
		// hand, which is thirteen worms and a session's fishing, standing at the
		// Rybak buying none of them. Only for a stack - a rod is one item and
		// either affordable or not.
		if (ch->GetGold() < price && count > 1)
		{
			const long long unit = GetPlayerBotNpcPurchasePrice(proto, 1);
			const long long spendable = (long long)ch->GetGold() *
					PLAYERBOT_FISHING_TACKLE_SPEND_PERCENT / 100;
			const int affordable = unit > 0
					? (int)std::min<long long>(count, spendable / unit) : 0;
			if (affordable > 0)
			{
				count = affordable;
				price = GetPlayerBotNpcPurchasePrice(proto, count);
				PlayerBotLogThrottled("tackle_part_buy", dwNow,
						"PLAYERBOT_FISHING: buying what it can afford of %s pid=%u name=%s vnum=%u count=%d price=%lld gold=%lld",
						what, ch->GetPlayerID(), ch->GetName(), vnum, count,
						price, (long long)ch->GetGold());
			}
		}
		if (price <= 0 || ch->GetGold() < price)
		{
			PlayerBotLogThrottled("tackle_no_gold", dwNow,
					"PLAYERBOT_FISHING: cannot afford %s pid=%u name=%s vnum=%u count=%d price=%lld gold=%lld",
					what, ch->GetPlayerID(), ch->GetName(), vnum, count,
					price, (long long)ch->GetGold());
			return false;
		}
		// The bag has to have room BEFORE the purchase, because AutoGiveItem
		// does not refuse a full bag: char_item.cpp puts the item on the ground
		// at the character's feet (AddToGround + StartDestroyEvent) and hands
		// it back as a success. So the old "if (!AutoGiveItem)" below never
		// fired, the bot paid, the rod lay on the grass, the bot still had no
		// rod and bought another on the next pass - which is the photograph
		// from the Discord: a herd of summoned horses round the Rybak standing
		// in a carpet of "Wedka+1". The arrow purchase learned this first
		// (playerbot_gear.h) and says so in the same words; the tackle purchase
		// was written a day later without it. A stackable that already has a
		// stack merges into it and needs no cell - that is the bait case.
		// Room the size of the item, not of one cell: a rod is three cells
		// high, and a bag with single holes and no free column passed the
		// one-cell test, paid, and had the rod put on the ground - then paid
		// again on the next tick, eight times in eight seconds, and picked the
		// rods up later ("bot nie ogarnal ze 1 wedka wystarczy", sizowski,
		// with a bag of fifteen).
		const bool bMergesIntoStack = count > 1 && ch->CountSpecifyItem(vnum) > 0;
		const int size = std::max<int>(1, proto->bSize);
		if (!bMergesIntoStack && ch->GetEmptyInventory(size) < 0)
		{
			PlayerBotLogThrottled("tackle_no_room", dwNow,
					"PLAYERBOT_FISHING: no bag room for %s pid=%u name=%s vnum=%u count=%d size=%d gold=%lld",
					what, ch->GetPlayerID(), ch->GetName(), vnum, count, size, (long long)ch->GetGold());
			return false;
		}
		LPITEM bought = ch->AutoGiveItem(vnum, count, -1, false);
		if (!bought)
			return false;
		// And the proof: AutoGiveItem hands back an item it dropped on the
		// ground as readily as one it put in the bag. A purchase that did not
		// reach the bag is not paid for and is not tried again this pass.
		if (bought->GetWindow() != INVENTORY)
		{
			PlayerBotLogThrottled("tackle_on_ground", dwNow,
					"PLAYERBOT_FISHING: %s landed outside the bag pid=%u name=%s vnum=%u window=%d",
					what, ch->GetPlayerID(), ch->GetName(), vnum, (int)bought->GetWindow());
			return false;
		}
		PlayerBotChangeGold(ch, -price);
		sys_log(0, "PLAYERBOT_FISHING: bought %s pid=%u name=%s vnum=%u count=%d price=%lld",
				what, ch->GetPlayerID(), ch->GetName(), vnum, count, price);
		return true;
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// CHARACTER::fishing() on this engine also insists on the fishing pass
	// (unique item 27620, a day of real time) being worn, and nothing sells
	// one: it comes out of the package's fishing quest, which a bot cannot
	// talk through. Until 2.0.16 the trip rule simply refused a bot without
	// it, so no bot on an mt2009 world ever fished ("Boty nie lowia ryb",
	// sizowski, 12 September). A bot of fifty that is due for a trip pays for
	// one the way it pays for the Forgetting Scroll - created on the spot and
	// worn at once - and buys the next when this one has run out. A pass that
	// could not be worn (EquipItem refuses within 1.5 s of an attack) waits
	// in the bag and is worn on the next ask rather than bought again.
	bool EnsurePlayerBotFishingPass(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		// Asked for now: the equipment pass leaves a worn pass alone for a while
		// (IsPlayerBotFishingPassHeld).
		s_mapPlayerBotFishingPassAskedAt[ch->GetPlayerID()] = dwNow;
		if (ch->IsEquipUniqueItem(UNIQUE_ITEM_FISHING_PASS))
			return true;
		LPITEM pass = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !pass; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetVnum() == UNIQUE_ITEM_FISHING_PASS)
				pass = item;
		}
		if (!pass)
		{
			if (ch->GetGold() < (int)(PLAYERBOT_FISHING_PASS_PRICE + GetPlayerBotReservedGold(ch)))
				return false;
			// AutoGiveItem drops what the bag cannot take at the bot's feet.
			if (ch->GetEmptyInventory(1) < 0)
				return false;
			pass = ch->AutoGiveItem(UNIQUE_ITEM_FISHING_PASS, 1, -1, false);
			// On the ground is not in the bag (IsPlayerBotWornItemSound).
			if (!pass || pass->GetOwner() != ch || pass->GetWindow() != INVENTORY)
				return false;
			PlayerBotChangeGold(ch, -(int)PLAYERBOT_FISHING_PASS_PRICE);
			sys_log(0, "PLAYERBOT_FISHING: fishing pass bought pid=%u name=%s price=%u gold=%lld",
					ch->GetPlayerID(), ch->GetName(), PLAYERBOT_FISHING_PASS_PRICE, (long long)ch->GetGold());
		}
		// Both unique slots taken: FindEquipCell answers WEAR_UNIQUE2 and
		// EquipItem refuses the occupied cell, so the pass stayed in the bag
		// for good and the next ask was an hour away - every FISHING line of
		// seban latino's 1013-bot world was "not worn yet" and nobody fished.
		// One slot is freed the way the unique-slots pass frees one for a
		// ring: what pays the bot nothing first, a ring or glove on its clock
		// last (it comes off at the water anyway), never what the engine
		// will not let go of.
		if (ch->GetWear(WEAR_UNIQUE1) && ch->GetWear(WEAR_UNIQUE2))
		{
			LPITEM displaced = NULL;
			for (int pass_ = 0; pass_ < 2 && !displaced; ++pass_)
				for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2 && !displaced; ++wear)
				{
					LPITEM worn = ch->GetWear(wear);
					if (!worn || !IsPlayerBotWornItemSound(ch, worn, wear) ||
							IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE))
						continue;
					if (pass_ == 0 && IsPlayerBotTimedUnique(worn->GetVnum()))
						continue;
					displaced = worn;
				}
			if (displaced && ch->GetEmptyInventory(displaced->GetSize()) >= 0 &&
					ch->UnequipItem(displaced))
				sys_log(0, "PLAYERBOT_FISHING: unique taken off for the pass pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), displaced->GetVnum());
		}
		if (!ch->EquipItem(pass))
		{
			PlayerBotLogThrottled("fishing_pass_wear", dwNow,
					"PLAYERBOT_FISHING: fishing pass in the bag but not worn yet pid=%u name=%s unique1=%u unique2=%u",
					ch->GetPlayerID(), ch->GetName(),
					ch->GetWear(WEAR_UNIQUE1) ? ch->GetWear(WEAR_UNIQUE1)->GetVnum() : 0,
					ch->GetWear(WEAR_UNIQUE2) ? ch->GetWear(WEAR_UNIQUE2)->GetVnum() : 0);
			return false;
		}
		return true;
	}
#endif

	bool RestockPlayerBotTackle(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;

		// "Nothing needed buying" and "buying failed" are not the same answer,
		// and returning the same false for both ended the session of every bot
		// that was already carrying what it came for.
		bool refused = false;
		if (!IsPlayerBotHoldingRod(ch) && CountPlayerBotRods(ch) <= 0)
		{
			if (!BuyPlayerBotTackleItem(ch, PLAYERBOT_FISHING_ROD_VNUM, 1, "fishing_rod", dwNow))
				refused = true;
		}

		if (!refused &&
				ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) < PLAYERBOT_FISHING_BAIT_RESTOCK)
		{
			if (!BuyPlayerBotTackleItem(ch, PLAYERBOT_FISHING_BAIT_VNUM,
					PLAYERBOT_FISHING_BAIT_BUNDLE, "fishing_bait", dwNow))
				refused = true;
		}
		// And one piece of Dried Wood for the end of the session, from the same
		// counter: the dead fish get grilled instead of vendored. The wood costs
		// twenty thousand and one fire takes any number of fish, so it is bought
		// for a batch, not for the three fish of a short session.
		// Bought before the fish exist, not after. The restock happens at the
		// Fisherman on the way to the bank and the dead fish only appear during
		// the session that follows, so asking the bot to be carrying them here
		// was a circle it could never close: measured on this world, 27 sessions
		// ended on the very path that lights a fire, two bots ever owned a log
		// between them, and not one fire was ever lit. An angler on its way out
		// buys one; thirty thousand yang against an angler's wallet is not a
		// decision worth making twice.
		if (ch->CountSpecifyItem(PLAYERBOT_CAMPFIRE_VNUM) <= 0)
		{
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_CAMPFIRE_VNUM);
			if (proto)
			{
				const long long price = GetPlayerBotNpcPurchasePrice(proto, 1);
				// Same trap as the rod: no cell means the wood lands on the grass.
				if (price > 0 && ch->GetGold() >= price &&
						ch->GetEmptyInventory(1) >= 0 &&
						ch->AutoGiveItem(PLAYERBOT_CAMPFIRE_VNUM, 1, -1, false))
				{
					PlayerBotChangeGold(ch, -price);
					sys_log(0, "PLAYERBOT_FISHING: bought campfire pid=%u name=%s price=%lld",
							ch->GetPlayerID(), ch->GetName(), price);
				}
			}
		}
		// The wood is a nicety and never a reason to end a session, so it does
		// not speak here. What matters is whether the two things the bot cannot
		// fish without were refused.
		return !refused;
	}

	bool ManagePlayerBotFishing(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (state.dwBakeUntil != 0 && BakePlayerBotFish(ch, state, dwNow))
			return true;
		if (!ch || ch->IsDead())
			return false;
		// Between sessions, never during one: the upgrade needs the rod out of
		// the hand, and EquipPlayerBotRod puts the new grade back on the next
		// cast by itself - it always takes the highest vnum in the bag.
		if (!state.bFishingSession)
			UpgradePlayerBotRod(ch);
		const TPlayerBotFishingBank* bank = GetPlayerBotFishingBank(ch->GetMapIndex());
		if (bank == NULL)
		{
			// The rod must not travel to a hunting map in the weapon slot.
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "left_m1");
			return false;
		}
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive)
		{
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "town_errand");
			return false;
		}

		if (!state.bFishingSession)
		{
			if (dwNow < state.dwNextFishingCheckTime || !IsPlayerBotAngler(ch, state))
				return false;
			// Never walk off mid-fight; finish the pack first.
			LPCHARACTER victim = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (victim && !victim->IsDead())
				return false;
#if defined(PLAYERBOT_ENGINE_MT2009)
			// This engine's fishing() wants a level and the pass worn; a session
			// begun without them walked to the stand and stood there two minutes
			// to "never_cast" (measured on the test world the day the pass was
			// added). Under the floor there is nothing to wait for; without the
			// gold for a pass the next ask is in an hour. The floor is the
			// engine's own - playerbotify.py moves fishing() to the same number,
			// so this gate and the engine can never disagree.
			if (ch->GetLevel() < PLAYERBOT_FISHING_MIN_LEVEL)
				return false;
			if (!EnsurePlayerBotFishingPass(ch, dwNow))
			{
				state.dwNextFishingCheckTime = dwNow + 60 * 60 * 1000;
				return false;
			}
#endif

			state.bFishingSession = true;
			state.bIsFishing = false;
			state.dwFishingCastTime = 0;
			state.dwNextFishingActionTime = 0;
			state.dwNextFishingProgressLogTime = 0;
			// Iwakura's Rybak is at the water for its mood's length, an hour at
			// most (GetPlayerBotRybakSessionMs).
			state.dwFishingSessionEndTime = dwNow +
					(IsPlayerBotPersonaEnabled() && state.persona.bRestored
						? GetPlayerBotRybakSessionMs(state, dwNow)
						: (DWORD)number(PLAYERBOT_FISHING_SESSION_MIN, PLAYERBOT_FISHING_SESSION_MAX));
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_FISHING: heading for the bank pid=%u name=%s level=%u personality=%u mood=%u spell=%d minutes=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bPersonality, (unsigned int)state.persona.mood.mood,
					state.persona.dwFishingSpellUntil != 0 && dwNow < state.persona.dwFishingSpellUntil ? 1 : 0,
					(unsigned int)((state.dwFishingSessionEndTime - dwNow) / 60000));
		}

		// A session only ends between casts, so a fish already on the hook is
		// still landed.
		if (dwNow >= state.dwFishingSessionEndTime && !state.bIsFishing)
		{
			LightPlayerBotCampfire(ch, state, dwNow);
			return EndPlayerBotFishingSession(ch, state, dwNow, "session_finished");
		}

		SetPlayerBotGoal(ch, state, BOT_GOAL_FISHING, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_FISHING, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Rod first, then worms: both come from the Rybak, who stands a short walk
		// upstream of the bank. Running out of bait sends the bot back to him.
		const bool needsTackle =
				(!IsPlayerBotHoldingRod(ch) && CountPlayerBotRods(ch) <= 0) ||
				ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
					PLAYERBOT_FISHING_BAIT_RESTOCK;

		long destX = 0, destY = 0;
		if (needsTackle)
		{
			GetPlayerBotNpcApproach(ch->GetPlayerID(), bank->fisherman.x,
					bank->fisherman.y, 0x46495348U, destX, destY);
		}
		else
		{
			GetPlayerBotFishingStand(ch->GetPlayerID(), dwNow, ch->GetMapIndex(),
					destX, destY);
			// A last check against the navigation's own grid, in case a stand
			// falls in a cell it refuses - but within two cells, not twelve.
			// Twelve is six hundred world units against an arrival radius of
			// twenty-five, which is the same mistake the portal walk made: two
			// stands a hundred and fifty apart could both be dragged onto one
			// cell, and two anglers were found eight units apart because of it.
			CPlayerBotNavigation& navigation =
					CPlayerBotNavigation::instance(ch->GetMapIndex());
			PIXEL_POSITION bank;
			if (navigation.Init(ch->GetMapIndex()) &&
					navigation.FindNearestWalkableWorld(destX, destY, 2, bank,
							ch->GetPlayerID()))
			{
				destX = bank.x;
				destY = bank.y;
			}
		}

		// bFishingSession exempts a bot from the inactivity watchdog - standing
		// still at the bank is the activity - which also means a session that goes
		// wrong is completely silent. One throttled line says where it actually is.
		if (dwNow >= state.dwNextFishingProgressLogTime)
		{
			state.dwNextFishingProgressLogTime = dwNow + PLAYERBOT_FISHING_PROGRESS_LOG;
			sys_log(0, "PLAYERBOT_FISHING: progress pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) dist=%ld tackle=%d rod=%d bait=%d casting=%d stuck=%u riding=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY,
					(long)DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY),
					needsTackle ? 1 : 0, IsPlayerBotHoldingRod(ch) ? 1 : 0,
					ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM),
					state.bIsFishing ? 1 : 0, (unsigned int)state.bStuckCounter,
					ch->IsRiding() ? 1 : 0);
		}

		// A session that never reaches the water is the worst of both worlds: the
		// bot has paid for tackle, stopped hunting, and walks the same failing
		// approach for as long as the server runs. Observed on a live world - a
		// level-34 bot stood at the Rybak with a rod in its bag and never cast.
		// Give up out loud instead, so the log says which leg failed.
		if (state.dwFishingSessionEndTime != 0 && !state.bIsFishing &&
				dwNow >= state.dwFishingSessionEndTime)
		{
			sys_err("PLAYERBOT_FISHING: never reached the water pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) tackle=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY, needsTackle ? 1 : 0);
			return EndPlayerBotFishingSession(ch, state, dwNow, "never_reached_water");
		}

		// The Rybak is a counter: its own radius and a snap inside it (see
		// PLAYERBOT_FISHING_TACKLE_ARRIVE); the stand keeps the cast point's.
		const int arrive = needsTackle ? PLAYERBOT_FISHING_TACKLE_ARRIVE : PLAYERBOT_FISHING_ARRIVE;
		const int snapCells = needsTackle ? PLAYERBOT_FISHING_TACKLE_SNAP_CELLS : 16;
		if (DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) > arrive)
		{
			// The Rybak's approach point is drawn by pid round him, and for
			// some pids it falls on ground the square does not join: the plan
			// was "unreachable" three times and the session over in ten
			// seconds, every session, for the same bots (OptimusPrime001 four
			// times on 23 September). The purchase asks no distance of him, so
			// the nearest cell of the bot's own ground inside his radius does.
			long walkX = destX, walkY = destY;
			if (needsTackle && FindPlayerBotReachableGoal(ch, destX, destY, arrive, walkX, walkY))
				PlayerBotLogThrottled("fishing_tackle_reachable", dwNow,
						"PLAYERBOT_FISHING: tackle approach moved onto reachable ground pid=%u name=%s goal=(%ld,%ld) walk=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), destX, destY, walkX, walkY);
			// Riding there is fine; the line simply cannot go in from a saddle.
			if (MovePlayerBot(ch, walkX, walkY, dwNow, snapCells, true, true) ||
					state.bStuckCounter < PLAYERBOT_FISHING_STUCK_LIMIT)
				return true;

			// Out of route. The tackle leg cannot be skipped - only the Rybak sells
			// rods - but the bank can be: fishing() in r40250 asks for a
			// non-blocking tile, a rod of type ITEM_ROD and bait in socket 2, and
			// never looks for water at all (it computes a facing offset and then
			// discards it). Casting where the bot already stands is therefore a
			// real cast, and it beats spending the entire session walking at a
			// bank the navigation cannot reach.
			if (needsTackle ||
					IsPlayerBotPositionBlocked(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			{
				sys_err("PLAYERBOT_FISHING: route failed pid=%u name=%s from=(%ld,%ld) to=(%ld,%ld) tackle=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
						destX, destY, needsTackle ? 1 : 0);
				return EndPlayerBotFishingSession(ch, state, dwNow, "route_failed");
			}

			sys_log(0, "PLAYERBOT_FISHING: bank unreachable, casting in place pid=%u name=%s pos=(%ld,%ld) bank=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY);
			state.bStuckCounter = 0;
			ClearPlayerBotRoute(state, true);
		}

		if (SetPlayerBotRidingForTravel(ch, state, false, dwNow, "fishing"))
			// StopRiding leaves the horse standing behind the angler for the
			// whole session ("wszystkie moje boty lowia z konmi obok"); it is
			// sent away like a player would, and summoned again for the ride.
			ch->HorseSummon(false);
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_STANDING);

		if (needsTackle)
		{
			// At the Fisherman's counter: the Rybak's rubbish goes here, from a
			// bag seventy percent full.
			if (IsPlayerBotPersonaEnabled() && IsPlayerBotBagFullForFishingJunk(ch))
				DiscardPlayerBotFishedDyes(ch, true);
			if (!RestockPlayerBotTackle(ch, state, dwNow))
				return EndPlayerBotFishingSession(ch, state, dwNow, "cannot_afford_tackle");
			return true;
		}
		if (!EquipPlayerBotRod(ch))
			return EndPlayerBotFishingSession(ch, state, dwNow, "rod_not_equippable");

		if (dwNow < state.dwNextFishingActionTime)
			return true;

		// Ready to fish and not fishing. If that goes on long enough the session
		// is over: a rod that will not go on, a bait that will not seat, or
		// anything else nobody has thought of yet, all end the same way instead
		// of standing at the water for hours.
		if (state.bIsFishing)
			state.dwFishingIdleSince = 0;
		else
		{
			if (state.dwFishingIdleSince == 0)
				state.dwFishingIdleSince = dwNow;
			else if (dwNow - state.dwFishingIdleSince >= PLAYERBOT_FISHING_NO_CAST_GIVE_UP)
			{
				sys_log(0, "PLAYERBOT_FISHING: no cast pid=%u name=%s rod=%d bait=%d idle_ms=%u",
						ch->GetPlayerID(), ch->GetName(),
						IsPlayerBotHoldingRod(ch) ? 1 : 0,
						ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM),
						(unsigned int)(dwNow - state.dwFishingIdleSince));
				return EndPlayerBotFishingSession(ch, state, dwNow, "never_cast");
			}
		}

#if defined(PLAYERBOT_ENGINE_MT2009)
		// The one gate of fishing() the bank tables cannot promise: water beside
		// the cell the bot stands on (IsNearAttr - r40250's fishing() has no
		// such test and its SECTREE no such method). A stand that has none on
		// this engine's map is marked dry and the next pass walks to another
		// (see s_setPlayerBotDryFishingStands); before this the bot stood there
		// the whole idle timeout and the session ended as "never_cast".
		if (!state.bIsFishing)
		{
			LPSECTREE dryTree = ch->GetSectree();
			if (dryTree && !dryTree->IsNearAttr(ch->GetX(), ch->GetY(), ATTR_WATER))
			{
				bool again = false;
				const int dry = MarkPlayerBotFishingStandDry(ch->GetPlayerID(), again);
				if (again)
				{
					// Handed a stand already marked dry: every stand of this bank is,
					// so there is nowhere to move to and the session ends here rather
					// than after the idle timeout as never_cast.
					sys_log(0, "PLAYERBOT_FISHING: bank dry pid=%u name=%s map=%ld key=%d pos=(%ld,%ld)",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), dry, ch->GetX(), ch->GetY());
					return EndPlayerBotFishingSession(ch, state, dwNow, "bank_dry");
				}
				sys_log(0, "PLAYERBOT_FISHING: dry stand pid=%u name=%s map=%ld key=%d pos=(%ld,%ld), moving to another",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), dry, ch->GetX(), ch->GetY());
				state.dwNextFishingActionTime = dwNow + 1000;
				return true;
			}
		}
#endif

		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!state.bIsFishing && rod && rod->GetSocket(2) == 0 && !BaitPlayerBotRod(ch))
		{
			// The pouch ran dry between passes; the walk back to the Rybak is
			// picked up by the tackle check at the top of the next pass.
			state.dwNextFishingActionTime = dwNow + number(1000, 2000);
			return true;
		}

#if defined(PLAYERBOT_ENGINE_MT2009)
		// mt2009 fishing is a reaction test and then a minigame, both driven
		// from the client. The pre-event bites 17 s after the cast: the engine
		// stamps m_bPlayerFishReactTime and the take has to come 1.7-3.5 s later
		// (fishing::Take), or four seconds after the bite the cast is failed. A
		// take in the window rolls the rod's chance; on a hit the minigame
		// starts: a bar climbing 0..100 at 5-9 per half second, a fish sinking 2
		// per half second and rising 8 per take, and the fish has to stay inside
		// the bar until the bar's top reaches 100. The catch itself comes from
		// the fishing quest, so it is read off the bag exactly as before.
		const bool bPreCast = ch->m_pkPreFishingEvent != NULL;
		const bool bCastLive = bPreCast || ch->IsPlayingFishGame();
#else
		// The engine holds the whole cast in one event: step 0 is the line in the
		// water, step 1 means a fish is on and starts the 6 s window to pull.
		fishing::fishing_event_info* info = ch->m_pkFishingEvent
				? dynamic_cast<fishing::fishing_event_info*>(ch->m_pkFishingEvent->info)
				: NULL;
		const bool bCastLive = info != NULL;
#endif

		if (!state.bIsFishing || !bCastLive)
		{
			if (bCastLive)
			{
				// A cast survived from an earlier pass; adopt it rather than
				// stacking a second one.
				state.bIsFishing = true;
				state.dwFishingCastTime = dwNow;
				return true;
			}
			if (state.bIsFishing)
			{
				// The event ended on its own -- the bite window elapsed. The engine
				// already cleared the bait, so the next pass re-baits and recasts.
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				ProcessPlayerBotCatch(ch);
				return true;
			}

			// A catch goes through AutoGiveItem, and AutoGiveItem never refuses a
			// full bag: it puts the fish on the grass and reports success. That is
			// what "the anglers drop their catch and every bot runs for it" was
			// (bierzyn, 10 September, with the photograph). A session with no
			// cell left ends here; the planner sends the bot to empty the bag.
			if (ch->GetEmptyInventory(1) < 0)
				return EndPlayerBotFishingSession(ch, state, dwNow, "bag_full");

			// CHARACTER::fishing() dereferences the sectree map and the tile under
			// the bot without checking either, so never call it blind.
			if (!ch->GetSectree() ||
					!SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex()))
			{
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}

			// Face straight across at the river rather than along the bank: the
			// water lies due east of this stretch.
			long waterX = 0, waterY = 0;
			GetPlayerBotFishingFacing(ch->GetPlayerID(), ch->GetMapIndex(),
					waterX, waterY);
			ch->SetRotationToXY(waterX, waterY != 0 ? waterY : ch->GetY());
#if defined(PLAYERBOT_ENGINE_MT2009)
			// The onboarding quest's flag is what fishing() checks; a bot never
			// talks to the fisherman, so it is set here once.
			if (ch->GetQuestFlag("fishing_onboarding.completed") < 1)
				ch->SetQuestFlag("fishing_onboarding.completed", 1);
			// The pass again on every cast: the ask refreshes the hold that keeps
			// the equipment and unique-slot passes off it for the session, and a
			// pass that came off is put back rather than cast without - fishing()
			// answered "You need to have a fishing pass" 730 times in two minutes
			// and 24 sessions ended never_cast with a rod and bait (17 September).
			if (!EnsurePlayerBotFishingPass(ch, dwNow))
				return EndPlayerBotFishingSession(ch, state, dwNow, "no_pass");
			ch->fishing();
			if (!ch->m_pkPreFishingEvent)
			{
				// fishing() explains a refusal to the client only, and a bot has
				// none: every gate it has is asked again here so the log says
				// which one it was.
				LPITEM rod = ch->GetWear(WEAR_WEAPON);
				LPSECTREE tree = ch->GetSectree();
				PlayerBotLogThrottled("fishing_refused", dwNow,
						"PLAYERBOT_FISHING: fishing() refused pid=%u name=%s level=%d map=%ld pass=%d onboarding=%d rod=%d bait_socket=%ld water=%d blocked=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetMapIndex(),
						ch->IsEquipUniqueItem(UNIQUE_ITEM_FISHING_PASS) ? 1 : 0,
						ch->GetQuestFlag("fishing_onboarding.completed"),
						(rod && rod->GetType() == ITEM_ROD) ? 1 : 0,
						rod ? rod->GetSocket(2) : -1L,
						(tree && tree->IsNearAttr(ch->GetX(), ch->GetY(), ATTR_WATER)) ? 1 : 0,
						(tree && tree->IsAttr(ch->GetX(), ch->GetY(), ATTR_BLOCK)) ? 1 : 0);
			}
			if (!ch->m_pkPreFishingEvent)
#else
			ch->fishing();
			if (!ch->m_pkFishingEvent)
#endif
			{
				// Blocked tile or missing bait; step away and try again shortly.
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}
			state.bIsFishing = true;
			state.dwFishingCastTime = dwNow;
			return true;
		}

#if defined(PLAYERBOT_ENGINE_MT2009)
		if (bPreCast)
		{
			const DWORD react = ch->m_bPlayerFishReactTime;
			if (react >= state.dwFishingCastTime && react <= dwNow &&
					dwNow - react >= PLAYERBOT_MT2009_FISHING_REACT_MIN &&
					dwNow - react <= PLAYERBOT_MT2009_FISHING_REACT_MAX)
			{
				ch->fishing_take();
				state.dwLastMeaningfulActivityTime = dwNow;
				return true;
			}
			if (dwNow - state.dwFishingCastTime > PLAYERBOT_FISHING_CAST_TIMEOUT)
			{
				// A take outside the window cancels both events.
				ch->fishing_take();
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				sys_log(0, "PLAYERBOT_FISHING: cast timed out pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			return true;
		}

		// The minigame. This pass runs every quarter second and the bar moves at
		// most 13 per half second, so a fish put just under the top of the bar
		// on each pass is still inside it on the next.
		fishing::fishing_event_info* game = ch->m_pkFishingEvent
				? dynamic_cast<fishing::fishing_event_info*>(ch->m_pkFishingEvent->info)
				: NULL;
		if (game && ch->m_biFishGameState >= PLAYERBOT_MT2009_FISHING_GAME_IN_PROGRESS)
		{
			const int top = (int)game->bar_position + (int)game->bar_height;
			for (int presses = 0; presses < 16 && ch->m_iFish_position + 8 <= top + 2; ++presses)
				fishing::Take(game, ch);
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		return true;
#else
		if (info->step < 1)
		{
			// Still waiting for a bite. The engine takes 10-40 s; anything past a
			// minute means the event is wedged.
			if (dwNow - state.dwFishingCastTime > PLAYERBOT_FISHING_CAST_TIMEOUT)
			{
				ch->fishing_take();
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				sys_log(0, "PLAYERBOT_FISHING: cast timed out pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			return true;
		}

		// A fish is on. fishing::Compute() peaks around 3 s after the bite, so wait
		// out that band before pulling instead of yanking the rod instantly.
		const DWORD hooked = get_dword_time() - info->hang_time;
		const DWORD pullAt = PLAYERBOT_FISHING_PULL_MIN_DELAY +
				PlayerBotNavHash(ch->GetPlayerID() ^ info->hang_time) %
				(PLAYERBOT_FISHING_PULL_MAX_DELAY - PLAYERBOT_FISHING_PULL_MIN_DELAY + 1U);
		if (hooked < pullAt)
			return true;

		ch->fishing_take();
		state.bIsFishing = false;
		state.dwNextFishingActionTime = dwNow + number(2000, 4000);
		state.dwLastMeaningfulActivityTime = dwNow;
		ProcessPlayerBotCatch(ch);
		sys_log(0, "PLAYERBOT_FISHING: pulled pid=%u name=%s hooked_ms=%u fish=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)hooked, info->fish_id);
		return true;
#endif
	}
}

#endif
