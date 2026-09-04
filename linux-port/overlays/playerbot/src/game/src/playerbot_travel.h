#ifndef __INC_METIN2_PLAYERBOT_TRAVEL_H__
#define __INC_METIN2_PLAYERBOT_TRAVEL_H__

// Where a bot ought to be, and how it gets there across maps.
//
// Two things live here that look separate and are not. The first is what a bot
// still needs - potions, a weapon, a repair - because that is what decides
// whether it may leave for a hunting ground at all. The second is the crossing
// itself: which frontier suits its level, when a trip is worth making, and the
// difference between a warp and a walk to a portal.
//
// A server-side bot has no client to reconnect, so it cannot cross between game
// cores. Every route planned here has to stay on the cores that host its maps.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_movement.h - it decides the destination, that file walks to
// it - and after playerbot_economy.h, whose refine opportunities it consults.

namespace
{
	void CountPlayerBotPotions(LPCHARACTER ch, size_t& redCount, size_t& blueCount)
	{
		redCount = 0;
		blueCount = 0;
		if (!ch || !ch->IsItemLoaded())
			return;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051)
				redCount += item->GetCount();
			else if (vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				blueCount += item->GetCount();
		}
	}

	bool NeedsPlayerBotPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		if (ch->GetLevel() <= 10)
			return (redCount < 30 && ch->GetGold() >= 300) ||
					(isMage && blueCount < 20 && ch->GetGold() >= 400);
		// Only a belt that is nearly out is worth crossing a map for. A bot with
		// half its potions left has no business walking away from a good spot -
		// it will fill up anyway the next time something else brings it to town.
		return (redCount < 150 && ch->GetGold() >= 1200) ||
				(blueCount < 100 && ch->GetGold() >= 1200);
	}

	bool NeedsPlayerBotEmergencyPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		// Normal restocking happens at 50/30 (or 10) units. Cross-map travel is
		// justified only by a genuinely short combat reserve, not by one consumed pot.
		//
		// Blue potions restore SP, so an empty belt is an emergency for a caster
		// and nothing at all for a warrior. Treating "no blue potions" as critical
		// for everybody put 210 of 442 warriors and ninjas into a permanent fake
		// emergency: they were always considered one step from being unable to
		// fight, so they abandoned every trip the moment their items finished
		// loading and shuttled straight back to town.
		return redCount < 10 || (isMage && blueCount < 8);
	}

	bool NeedsPlayerBotCriticalTownServices(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// These problems can make continued combat impossible or waste most future
		// drops, so they justify an immediate cross-map return.
		if (ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				ch->GetWear(WEAR_SHIELD) == NULL || ch->GetWear(WEAR_HEAD) == NULL ||
				ch->GetWear(WEAR_FOOTS) == NULL || NeedsPlayerBotEmergencyPotions(ch) ||
				NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0)
			return true;

		size_t occupiedGridCells = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item)
				occupiedGridCells += std::max(1, (int)item->GetSize());
		}
		return occupiedGridCells * 100 >= INVENTORY_MAX_NUM * 45;
	}

	// A missing weapon or body armour, an empty potion belt, no arrows or a full
	// inventory really do stop a bot from playing, and must outrank travelling.
	// A missing helmet, boots or shield only make it a little weaker. Treating
	// those as equally critical trapped a bot for good whenever the town could
	// not sell it the missing piece: it always wanted to shop, so it was never
	// allowed to travel, and it kept farming level-3 wolves in Joan at level 27.
	bool BlocksPlayerBotTravel(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		return ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				NeedsPlayerBotEmergencyPotions(ch) || NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0;
	}

	bool NeedsPlayerBotM1OnlyServices(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		// Bokjung has no profession trainers or Biologist. Everything else can be
		// handled locally in M2, so only these two real activities justify M2 -> M1.
		if (ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN)
			return true;

		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch);
		if (!mission)
			return false;
		const int accepted = std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, (int)mission->requiredCount - accepted);
		return remaining > 0 && ch->CountSpecifyItem(mission->itemVnum) >= remaining;
	}

	// Above this level Bokjung has nothing left to offer, so nothing there is
	// worth keeping a bot for either.
	const BYTE PLAYERBOT_M2_COHORT_MAX_LEVEL = 35;

	bool IsPlayerBotPastM2Ceiling(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL;
	}

	bool IsPlayerBotM2LevelingCohort(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 20 ||
				ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL)
			return false;
		// Levels 20-21 still have a little useful M1 progression, so retain a small
		// stable minority there. At level 22 every ordinary leveler graduates to M2.
		return ch->GetLevel() >= 22 ||
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d325850U) % 10U) != 0;
	}

	bool ShouldPlayerBotLeaveRemoteMapForRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!HasPlayerBotPriorityRefineOpportunity(ch))
			return false;
		if (state.dwNextRemoteRefineReturnTime == 0)
		{
			const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^
					(dwNow / 60000U) ^ 0x52455455U) %
					(PLAYERBOT_REMOTE_REFINE_RETURN_MAX_DELAY -
					 PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + 1);
			state.dwNextRemoteRefineReturnTime = dwNow +
					PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + spread;
			return false;
		}
		return dwNow >= state.dwNextRemoteRefineReturnTime;
	}

	bool IsPlayerBotFrontierMap(long mapIndex)
	{
		return mapIndex == PLAYERBOT_MAP_ORC_VALLEY || mapIndex == PLAYERBOT_MAP_DESERT;
	}

	// The map whose ordinary spawns still sit inside this bot's useful level
	// window, or 0 when Bokjung is still the right place for it.
	long GetPlayerBotFrontierMapForLevel(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const BYTE level = ch->GetLevel();
		if (level >= PLAYERBOT_ORC_VALLEY_MIN_LEVEL && level <= PLAYERBOT_ORC_VALLEY_MAX_LEVEL)
			return PLAYERBOT_MAP_ORC_VALLEY;
		if (level >= PLAYERBOT_DESERT_MIN_LEVEL && level <= PLAYERBOT_DESERT_MAX_LEVEL)
			return PLAYERBOT_MAP_DESERT;
		return 0;
	}

	// How far from town a personality is willing to play, in eighths. Personality
	// used to decide only how far a bot would push a refine, so every character
	// hunted in the same places; this is what makes the trait visible in-world.
	BYTE GetPlayerBotFrontierAppetite(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return 7; // explorer: almost always out on the far maps
			case BOT_PERSONALITY_METIN_BREAKER:
				return 6; // both frontier maps carry their own Metin spawns
			case BOT_PERSONALITY_GEAR_SPECIALIST:
				return 6; // Orc Valley is where the level-30 weapons drop
			case BOT_PERSONALITY_TEAM_COMPANION:
				return 4; // stays near the party pool at least half the time
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return 2; // protects what it has earned, keeps a town run short
			default:
				return 5; // steady adventurer
		}
	}

	// Due for a fishing trip: old enough for the rod, and its cooldown is up.
	bool WantsPlayerBotFishingTrip(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!IsPlayerBotAngler(ch, state))
			return false;
		return state.dwNextFishingCheckTime == 0 || dwNow >= state.dwNextFishingCheckTime;
	}

	bool ShouldPlayerBotLeaveForFrontier(LPCHARACTER ch)
	{
		if (!ch || GetPlayerBotFrontierMapForLevel(ch) == 0)
			return false;
		TPlayerBotAIStateMap::const_iterator it =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = it != s_mapPlayerBotAIStates.end()
				? it->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// Above the Bokjung ceiling there is nothing there left to stay behind
		// for, so the reserve rule has nothing to protect and only strands bots.
		if (IsPlayerBotPastM2Ceiling(ch))
			return true;
		// Below it Bokjung must never empty out completely: even the keenest
		// explorer leaves one bot in eight behind for the town, its Bestials
		// and the local party pool.
		const DWORD appetite = GetPlayerBotFrontierAppetite(personality);
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x46524f4eU) % 8U) < appetite;
	}

	// A wanderer settles in for a long session; a careful collector treats the
	// trip as an errand and heads back while it still has potions left.
	DWORD GetPlayerBotFrontierVisitTime(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME * 2;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME / 2;
			default:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME;
		}
	}

	bool ShouldPlayerBotVisitM3(LPCHARACTER ch)
	{
		if (!HasPlayerBotM3ReadyEquipment(ch) || ch->GetLevel() > 24 ||
				HasPlayerBotSpecialLevel30Weapon(ch, true))
			return false;
		// A stable third of the eligible population farms infected animals for
		// class-specific level-30 weapons. Other bots remain in M2 for Bestials.
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d335850U) % 3U) == 0;
	}

	bool ShouldPlayerBotHuntM2Bestials(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 25 || ch->GetLevel() > 35 ||
				!HasPlayerBotM3ReadyEquipment(ch) ||
				HasPlayerBotSpecialLevel30Weapon(ch, true) ||
				ShouldPlayerBotVisitM3(ch))
			return false;
		// M3 already owns one third of the eligible weapon hunters. Half of the
		// remaining cohort searches the two rare Bestial spawns in M2, while the
		// rest keeps levelling normally instead of camping one pair of enemies.
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x42455354U) % 2U) == 0;
	}

	bool ShouldPlayerBotPursueHorseExpedition(LPCHARACTER ch, DWORD dwNow)
	{
		if (!CanPlayerBotAdvanceHorse(ch))
			return false;

		// The dungeon is only worth a trip inside its own level band. This gate
		// also empties it: a bot already inside re-evaluates the same call every
		// tick, so the one that levels past the band finishes what it is doing
		// and walks out rather than waiting for the thirty-minute timeout.
		if (ch->GetLevel() < PLAYERBOT_MONKEY_MIN_LEVEL ||
				ch->GetLevel() > PLAYERBOT_MONKEY_MAX_LEVEL)
			return false;

		// A combat horse matters most to Warriors and weapon Suras, but it must be
		// one goal among several rather than a compulsory conveyor belt through the
		// dungeon.  The cohort rotates every 30 minutes and again after every earned
		// horse level.  Eventually every build gets opportunities while most bots
		// continue levelling in M2 at any given time.
		const bool hasCombatHorse = ch->GetHorseLevel() >= 11;
		BYTE chance = 10;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
				chance = hasCombatHorse ? 4 : (ch->GetHorseLevel() == 0 ? 34 : 26);
				break;
			case JOB_SURA:
				// Skill group 1 is Weaponry (WP); group 2 is Black Magic.
				chance = ch->GetSkillGroup() == 1
						? (hasCombatHorse ? 4 : (ch->GetHorseLevel() == 0 ? 32 : 25))
						: (hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 14 : 9));
				break;
			case JOB_ASSASSIN:
				chance = ch->GetSkillGroup() == 2
						? (hasCombatHorse ? 1 : (ch->GetHorseLevel() == 0 ? 6 : 4))
						: (hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 18 : 14));
				break;
			case JOB_SHAMAN:
				chance = hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 15 : 10);
				break;
		}
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (stateIt != s_mapPlayerBotAIStates.end() &&
				stateIt->second.bAmbition == BOT_AMBITION_HORSE && !hasCombatHorse)
			chance = std::min<BYTE>(55, chance + 15);

		const DWORD window = dwNow / (30U * 60U * 1000U);
		const DWORD seed = ch->GetPlayerID() ^ (window * 0x9e3779b9U) ^
				((DWORD)(ch->GetHorseLevel() + 1) * 0x85ebca6bU);
		return (PlayerBotNavHash(seed ^ 0x484f5253U) % 100U) < chance;
	}

	int GetPlayerBotDesiredHorseMedalStock(LPCHARACTER ch)
	{
		if (!ch)
			return 1;
		// The high-priority builds occasionally prepare the next horse level in the
		// same visit. Other classes leave after one medal, freeing dungeon capacity
		// and returning to ordinary experience progression much sooner.
		const bool highPriority = ch->GetJob() == JOB_WARRIOR ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 1);
		return highPriority
				? 1 + (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d454441U) % 2U)
				: 1;
	}

	bool TransitionPlayerBotMap(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(targetMap);
		if (!navigation.Init(targetMap))
		{
			sys_err("PLAYERBOT_WORLD: target navigation unavailable pid=%u name=%s map=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), targetMap, reason ? reason : "?");
			return false;
		}

		const long oldMap = ch->GetMapIndex();
		const bool wasRiding = ch->IsRiding();
		if (ch->GetParty())
			ch->GetParty()->Quit(ch->GetPlayerID());
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		// A PC mount and the separately summoned horse are two different server
		// entities. StopRiding() summons the latter on the old map, so explicitly
		// remove it before Show(). Otherwise a rider can leave behind an orphaned
		// horse at a dungeon portal (issue #4).
		if (wasRiding)
			ch->StopRiding();
		ch->HorseSummon(false);
		ClearPlayerBotRoute(state, true);
		state.bVisitingShop = false;
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		if (!ch->Show(targetMap, targetX, targetY, 0))
		{
			if (wasRiding && !ch->IsRiding())
				ch->StartRiding();
			sys_err("PLAYERBOT_WORLD: transition failed pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason ? reason : "?");
			return false;
		}
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, targetX, targetY, 0, dwNow);
		if (wasRiding && ch->GetHorseHealth() > 0 && ch->GetHorseStamina() > 0)
			ch->StartRiding();
		ch->Save();
		state.dwNextWanderTime = dwNow + number(1500, 4500);
		state.dwNextHorseRideCheckTime = dwNow + 1000;
		state.dwDungeonEnteredTime = targetMap == PLAYERBOT_MAP_MONKEY_EASY ? dwNow : 0;
		state.dwM3EnteredTime = targetMap == PLAYERBOT_MAP_CHUNJO_M3 ? dwNow : 0;
		state.dwFrontierEnteredTime = IsPlayerBotFrontierMap(targetMap) ? dwNow : 0;
		if (targetMap == PLAYERBOT_MAP_CHUNJO_M3)
			state.dwNextRemoteRefineReturnTime = 0;
		sys_log(0, "PLAYERBOT_WORLD: transitioned pid=%u name=%s from=%ld to=%ld pos=(%ld,%ld) reason=%s",
				ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, targetX, targetY,
				reason ? reason : "?");
		return true;
	}

	bool MovePlayerBotToWorldPortal(LPCHARACTER ch, TPlayerBotAIState& state,
			long portalX, long portalY, long targetMap, long targetX, long targetY,
			DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Warp NPCs trigger at 300 units and expect a real client reconnect. Switch
		// server-side at 900 units so a bot descriptor never enters that code path,
		// while the character still visibly walks all the way to the portal area.
		if (DISTANCE_APPROX(ch->GetX() - portalX, ch->GetY() - portalY) <= 900)
			return TransitionPlayerBotMap(ch, state, targetMap, targetX, targetY, dwNow, reason);

		MovePlayerBot(ch, portalX, portalY, dwNow, 24, true, true);
		return true;
	}

	// Being in a fight is not the same as having something selected. A bot is in
	// the fight when blows are being exchanged, when the monster is coming for
	// it, or when it already stands within reach. A mob picked out across the
	// field is none of those and must not postpone a decision to leave the map.
	bool IsPlayerBotEngagedWith(LPCHARACTER ch, LPCHARACTER victim,
			const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !victim)
			return false;
		if (victim->GetVictim() == ch)
			return true;
		if (state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime <= PLAYERBOT_TRAVEL_ENGAGED_WINDOW)
			return true;
		return DISTANCE_APPROX(ch->GetX() - victim->GetX(),
				ch->GetY() - victim->GetY()) <= PLAYERBOT_TRAVEL_ENGAGED_RANGE;
	}

	bool ManagePlayerBotWorldTravel(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || state.bVisitingBiologist ||
				state.bVisitingStable || state.bRecoveringAfterDeath || state.bTacticalRetreat)
			return false;

		const long mapIndex = ch->GetMapIndex();
		const bool hasMedal = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0;
		// A trader does not down tools to go and farm horse medals in the Monkey
		// Dungeon. That errand takes a bot right across the world for the better
		// part of an hour, and it is exactly the striving this personality exists
		// not to do.
		const bool pursuesHorseExpedition =
				state.bPersonality != BOT_PERSONALITY_MERCHANT &&
				ShouldPlayerBotPursueHorseExpedition(ch, dwNow);
		const bool needsHorseExpedition = pursuesHorseExpedition && !hasMedal;
		// GetWear answers NULL for every slot until the item cache has loaded,
		// which is exactly the state a character is in for the first seconds after
		// a map change. Trusting it there made a bot believe it had lost its
		// weapon the moment it arrived somewhere.
		const bool needsEssentialWeaponSupply = ch->IsItemLoaded() &&
				(ch->GetWear(WEAR_WEAPON) == NULL || NeedsPlayerBotArrows(ch));
		const bool m2LevelingCohort = IsPlayerBotM2LevelingCohort(ch);
		const bool wantsM3 = ShouldPlayerBotVisitM3(ch);
		const bool needsCriticalTownServices = NeedsPlayerBotCriticalTownServices(ch);
		const bool needsM1OnlyServices = NeedsPlayerBotM1OnlyServices(ch);
		// M2 has its own blacksmith. Only the remote M3 farm needs to schedule a
		// return to town for equipment progression.
		const bool scheduledRemoteRefine = mapIndex == PLAYERBOT_MAP_CHUNJO_M3 &&
				ShouldPlayerBotLeaveRemoteMapForRefining(ch, state, dwNow);

		// Leaving the Monkey Dungeon is a decision, not a pathfinding exercise.
		// Evaluate it before yielding to an existing victim: a monster near the
		// portal must not keep a finished, timed-out or unequipped bot here forever.
		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY)
		{
			if (state.dwDungeonEnteredTime == 0)
				state.dwDungeonEnteredTime = dwNow;
			const bool visitExpired = dwNow - state.dwDungeonEnteredTime >=
					PLAYERBOT_MONKEY_MAX_VISIT_TIME;
			const int medalCount = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM);
			playerbot_world_rules::TMonkeyVisitContext context;
			context.needsEssentialSupply = needsEssentialWeaponSupply;
			context.needsPotions = NeedsPlayerBotEmergencyPotions(ch);
			context.medalCount = medalCount;
			context.desiredMedalCount = GetPlayerBotDesiredHorseMedalStock(ch);
			context.visitExpired = visitExpired;
			// Re-evaluate the rotating cohort even inside the dungeon. Bots which are
			// no longer selected finish their current medal (if any) and leave instead
			// of occupying the dungeon until its absolute 30-minute timeout.
			context.canAdvanceHorse = CanPlayerBotAdvanceHorse(ch) &&
					pursuesHorseExpedition;
			const playerbot_world_rules::EMonkeyExitDecision exitDecision =
					playerbot_world_rules::DecideMonkeyExit(context);
			if (exitDecision != playerbot_world_rules::MONKEY_STAY)
			{
				SetPlayerBotGoal(ch, state,
						exitDecision == playerbot_world_rules::MONKEY_EXIT_RESTOCK
						? BOT_GOAL_RESTOCK : BOT_GOAL_HORSE, dwNow);
				const char* reason = "monkey_horse_complete_direct";
				if (exitDecision == playerbot_world_rules::MONKEY_EXIT_RESTOCK)
					reason = "monkey_restock_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_MEDAL_READY)
					reason = "monkey_medal_found_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_TIMEOUT)
					reason = "monkey_timeout_direct";
				const bool transitioned = TransitionPlayerBotMap(ch, state,
						PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_MONKEY_RETURN_X,
						PLAYERBOT_M2_MONKEY_RETURN_Y, dwNow, reason);
				if (transitioned && medalCount == 0)
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return transitioned;
			}
		}

		// M3 is a focused level-30 weapon farm, not a levelling map. A bot which
		// reaches level 25 graduates immediately, even if an old victim is still
		// alive, and resumes normal progression in M2.
		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M3 && ch->GetLevel() > 24)
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M3_RETURN_PORTAL_X, PLAYERBOT_M3_RETURN_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X,
					PLAYERBOT_M2_FROM_M3_Y, dwNow, "m3_level_graduated");
		}

		// A bot should not walk away from a fight -- but "holds a target" was
		// standing in for "is fighting", and in a dense respawn those are not the
		// same thing at all. Something is always in range, so this check used to
		// return before the routing below had ever been consulted, and the
		// arrival areas of frontier maps quietly became one-way (issue #10).
		//
		// Three cases now. A Metin already under the hammer is always finished
		// first; ShouldPlayerBotAbandonStone releases a stalled one. A live fight
		// holds travel back, but only for a bounded grace period, so an endless
		// chain of packs can no longer outrank the decision to leave. A target
		// merely selected across the field does not delay anything.
		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!victim || victim->IsDead())
			state.dwTravelBlockedSince = 0;
		else
		{
			if (state.dwTravelBlockedSince == 0)
				state.dwTravelBlockedSince = dwNow;
			const bool graceLeft = dwNow - state.dwTravelBlockedSince <
					PLAYERBOT_TRAVEL_FIGHT_GRACE;
			if (victim->IsStone() ||
					(graceLeft && IsPlayerBotEngagedWith(ch, victim, state, dwNow)))
				return false;
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M1)
		{
			// Compact and sell an oversized potion reserve before the first trip to
			// M2. Once the bot is already outside M1, excess potions alone must not
			// drag it back across maps; it can keep levelling until a real restock or
			// inventory visit is needed.
			const bool townVisitRecentlyCompleted = state.dwNextShopCheckTime != 0 &&
					dwNow < state.dwNextShopCheckTime;
			const bool needsAnyRefine = HasPlayerBotRefineOpportunity(ch);
			const bool needsTownPreparation = NeedsPlayerBotPotions(ch) ||
					CountPlayerBotJunkItems(ch) >= 12 || needsAnyRefine;
			// The soft needs get one town visit to be met. If the bot has just
			// been shopping and still wants something, the town cannot supply it,
			// and standing here is worse than moving on.
			if (hasMedal || BlocksPlayerBotTravel(ch) || needsM1OnlyServices ||
					HasPlayerBotExcessPotions(ch) ||
					((needsTownPreparation || needsCriticalTownServices) &&
					 !townVisitRecentlyCompleted))
				return false;
			// The M2 cohort ends at 35, but the frontier maps begin at 30 and run
			// far past it. Testing only the cohort here meant a level 36+ bot fell
			// out of this gate on every tick and stayed in Joan for good, hunting
			// level-3 wolves - which is the "boty bija psy" the Discord keeps
			// reporting. Anything with somewhere better to be gets through.
			if (!needsHorseExpedition && !m2LevelingCohort && !wantsM3 &&
					GetPlayerBotFrontierMapForLevel(ch) == 0 &&
					!IsPlayerBotPastM2Ceiling(ch))
				return false;
			if (state.dwNextWorldTravelTime == 0)
			{
				const bool graduatedFromM1 = ch->GetLevel() >= 22 && !needsHorseExpedition;
				const DWORD minDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MIN_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MIN_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MIN_DELAY);
				const DWORD maxDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MAX_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MAX_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MAX_DELAY);
				const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x54524156U) %
						(maxDelay - minDelay + 1);
				state.dwNextWorldTravelTime = dwNow + minDelay + spread;
				return false;
			}
			if (dwNow < state.dwNextWorldTravelTime)
				return false;

			// Joan has a Teleporter of its own, so a bot which has already outgrown
			// Bokjung can leave for the frontier directly. Routing it through M2
			// first would queue it behind every town errand in the village, which
			// is what left Orc Valley empty while the Desert filled up from the
			// bots that happened to already be in Bokjung.
			// Let it fish before it is handed the next hunting destination.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
				return false;

			if (!needsHorseExpedition && !wantsM3)
			{
				const long directMap = ShouldPlayerBotLeaveForFrontier(ch)
						? GetPlayerBotFrontierMapForLevel(ch) : 0;
				if (directMap != 0)
				{
					const bool toDesert = directMap == PLAYERBOT_MAP_DESERT;
					SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
					return MovePlayerBotToWorldPortal(ch, state,
							PLAYERBOT_M1_TELEPORTER_X, PLAYERBOT_M1_TELEPORTER_Y,
							directMap,
							toDesert ? PLAYERBOT_DESERT_ARRIVAL_X : PLAYERBOT_ORC_VALLEY_ARRIVAL_X,
							toDesert ? PLAYERBOT_DESERT_ARRIVAL_Y : PLAYERBOT_ORC_VALLEY_ARRIVAL_Y,
							dwNow,
							toDesert ? "m1_direct_to_desert" : "m1_direct_to_orc_valley");
				}
			}

			SetPlayerBotGoal(ch, state, needsHorseExpedition ? BOT_GOAL_HORSE :
					(wantsM3 ? BOT_GOAL_GET_EQUIPMENT : BOT_GOAL_LEVEL_UP), dwNow);
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M1_TO_M2_PORTAL_X, PLAYERBOT_M1_TO_M2_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_ARRIVAL_X, PLAYERBOT_M2_ARRIVAL_Y,
					dwNow, needsHorseExpedition ? "horse_to_m2" : "level_to_m2");
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M2)
		{
			// ManagePlayerBotHorse owns the medal on M2 and walks to the local Stable
			// Boy. Returning to M1 here was the source of the needless three-map trip.
			if (hasMedal)
				return false;

			// Profession trainers and the Biologist only exist in Joan. Routine gear,
			// potion, inventory and refine needs are served by the real Bokjung NPCs.
			if (needsM1OnlyServices)
			{
				SetPlayerBotGoal(ch, state, ch->GetSkillGroup() == 0
						? BOT_GOAL_CHOOSE_PROFESSION : BOT_GOAL_BIOLOGIST, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X,
						PLAYERBOT_M1_RETURN_Y, dwNow, "m1_only_service");
			}
			// Same rule in Bokjung: its own shops own this need, but only until a
			// visit has actually happened. Otherwise a bot the town cannot equip
			// would never reach the frontier maps either.
			if (BlocksPlayerBotTravel(ch))
				return false;
			if (needsCriticalTownServices && (state.dwNextShopCheckTime == 0 ||
					dwNow < state.dwNextShopCheckTime))
				return false; // local M2 town visit owns this need

			if (needsHorseExpedition)
			{
				// Honour the rest period set by a failed/timed-out expedition and
				// stagger fresh M2 populations after a restart. Without this guard a
				// direct exit was followed by an immediate direct re-entry.
				if (state.dwNextWorldTravelTime == 0)
				{
					const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d4f4e4bU) %
							(PLAYERBOT_HORSE_TRAVEL_MAX_DELAY -
							 PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + 1);
					state.dwNextWorldTravelTime = dwNow +
							PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + spread;
					return false;
				}
				if (playerbot_world_rules::IsTravelCooldownActive(
						dwNow, state.dwNextWorldTravelTime))
					return false;
				SetPlayerBotGoal(ch, state, BOT_GOAL_HORSE, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_MONKEY_PORTAL_X, PLAYERBOT_M2_MONKEY_PORTAL_Y,
						PLAYERBOT_MAP_MONKEY_EASY, PLAYERBOT_MONKEY_EASY_ARRIVAL_X,
						PLAYERBOT_MONKEY_EASY_ARRIVAL_Y, dwNow, "horse_to_monkey");
			}

			if (wantsM3 && !needsCriticalTownServices)
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M3_TELEPORTER_X, PLAYERBOT_M2_TO_M3_TELEPORTER_Y,
						PLAYERBOT_MAP_CHUNJO_M3, PLAYERBOT_M3_ARRIVAL_X,
						PLAYERBOT_M3_ARRIVAL_Y, dwNow, "level30_weapon_to_m3");
			}

			// The river is in Joan, and every bot old enough to hold a rod has long
			// since left it - so fishing only ever happens if the trip is a real
			// destination. Ranked above the frontier maps: a session is short, and
			// the pearls it brings back are worth more than the hunting it skips.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_HUNTING, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X,
						PLAYERBOT_M1_RETURN_Y, dwNow, "fishing_to_m1");
			}

			// Bokjung's own spawns stop paying long before the M2 band ends. Bots
			// which have outgrown them move on to Orc Valley and then the Desert
			// instead of grinding monsters they would rather walk past.
			const long frontierMap = ShouldPlayerBotLeaveForFrontier(ch)
					? GetPlayerBotFrontierMapForLevel(ch) : 0;
			if (frontierMap != 0)
			{
				const bool toDesert = frontierMap == PLAYERBOT_MAP_DESERT;
				SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M3_TELEPORTER_X, PLAYERBOT_M2_TO_M3_TELEPORTER_Y,
						frontierMap,
						toDesert ? PLAYERBOT_DESERT_ARRIVAL_X : PLAYERBOT_ORC_VALLEY_ARRIVAL_X,
						toDesert ? PLAYERBOT_DESERT_ARRIVAL_Y : PLAYERBOT_ORC_VALLEY_ARRIVAL_Y,
						dwNow, toDesert ? "level_to_desert" : "level_to_orc_valley");
			}

			// Sending a bot back to Joan is only right when it has outgrown
			// nothing yet. Doing it above the ceiling produced an M1 <-> M2 loop:
			// Joan let it leave, Bokjung refused to keep it, and neither ever
			// hunted anything.
			if (!m2LevelingCohort && !IsPlayerBotPastM2Ceiling(ch))
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
				const bool moving = MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X, PLAYERBOT_M1_RETURN_Y,
						dwNow, "m2_level_range_complete");
				if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1)
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return moving;
			}

			return false; // designated M2 leveler: hunt normally
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M3)
		{
			if (state.dwM3EnteredTime == 0)
				state.dwM3EnteredTime = dwNow;
			const bool visitExpired = dwNow - state.dwM3EnteredTime >= PLAYERBOT_M3_MAX_VISIT_TIME;
			if (!visitExpired && !needsCriticalTownServices && !needsM1OnlyServices &&
					!scheduledRemoteRefine &&
					!HasPlayerBotSpecialLevel30Weapon(ch, true))
				return false;

			SetPlayerBotGoal(ch, state,
					(needsCriticalTownServices || needsM1OnlyServices) ? BOT_GOAL_RESTOCK :
					(scheduledRemoteRefine ? BOT_GOAL_GET_EQUIPMENT : BOT_GOAL_LEVEL_UP), dwNow);
			const char* reason = "m3_weapon_found";
			if (needsCriticalTownServices || needsM1OnlyServices)
				reason = "m3_services_to_m2";
			else if (scheduledRemoteRefine)
				reason = "m3_scheduled_refine_to_m2";
			else if (visitExpired)
				reason = "m3_visit_complete";
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M3_RETURN_PORTAL_X, PLAYERBOT_M3_RETURN_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X,
					PLAYERBOT_M2_FROM_M3_Y, dwNow, reason);
		}

		if (IsPlayerBotFrontierMap(mapIndex))
		{
			if (state.dwFrontierEnteredTime == 0)
				state.dwFrontierEnteredTime = dwNow;
			const DWORD stayed = dwNow - state.dwFrontierEnteredTime;
			const bool visitExpired = stayed >=
					GetPlayerBotFrontierVisitTime(state.bPersonality);
			// Two minutes of actually playing here before anything but a real
			// emergency may send the bot home again.
			const bool settledIn = stayed >= PLAYERBOT_FRONTIER_MIN_VISIT_TIME;
			// Outgrowing the map matters as much as running out of potions: neither
			// Orc Valley nor the Desert has a merchant, a blacksmith or a trainer.
			const bool outOfBand = GetPlayerBotFrontierMapForLevel(ch) != mapIndex;
			// Only a need that actually stops the bot playing is worth the trip
			// home. Sending it back for a helmet the town cannot sell turned the
			// journey into a shuttle: it arrived, saw the same unmet need, and
			// left again without ever fighting anything here.
			// A blocking need still wins immediately - a bot with no weapon left
			// cannot wait out a timer. Everything else waits until the bot has
			// been here long enough for the trip to have been worth making.
			const bool blocked = BlocksPlayerBotTravel(ch);
			const bool needsTown = blocked ||
					(settledIn && (needsM1OnlyServices || needsEssentialWeaponSupply));
			if (!visitExpired && !outOfBand && !needsTown)
				return false;

			SetPlayerBotGoal(ch, state, needsTown ? BOT_GOAL_RESTOCK : BOT_GOAL_LEVEL_UP, dwNow);
			const char* reason = "frontier_visit_complete";
			if (needsTown)
				reason = "frontier_services_to_m2";
			else if (outOfBand)
				reason = "frontier_level_graduated";
			const bool inDesert = mapIndex == PLAYERBOT_MAP_DESERT;
			return MovePlayerBotToWorldPortal(ch, state,
					inDesert ? PLAYERBOT_DESERT_EXIT_X : PLAYERBOT_ORC_VALLEY_EXIT_X,
					inDesert ? PLAYERBOT_DESERT_EXIT_Y : PLAYERBOT_ORC_VALLEY_EXIT_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X, PLAYERBOT_M2_FROM_M3_Y,
					dwNow, reason);
		}

		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY)
			return false; // stay and fight; departure was handled before victim yielding

		// Anything else means the bot was moved somewhere no branch above owns:
		// the frontier maps carry real warp NPCs of their own, and walking inside
		// one's trigger radius hands the character to a map the AI has no plan
		// for. Nothing would ever bring it back, so it would sit there for good.
		SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
		if (TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_CHUNJO_M2,
				PLAYERBOT_M2_FROM_M3_X, PLAYERBOT_M2_FROM_M3_Y, dwNow, "stranded_recovery"))
		{
			sys_log(0, "PLAYERBOT_WORLD: recovered pid=%u name=%s from unmanaged map=%ld",
					ch->GetPlayerID(), ch->GetName(), mapIndex);
			return true;
		}
		return false;
	}
}

#endif
