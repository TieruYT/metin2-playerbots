#ifndef __INC_METIN2_PLAYERBOT_PLANNER_H__
#define __INC_METIN2_PLAYERBOT_PLANNER_H__

// What a bot decides to do with the next few minutes of its life.
//
// This used to be one chain of else-ifs at the top of the manager, which read
// well but could only ever be changed by rebuilding the image: the order was
// the policy. It is now a list of candidates, each with a base priority and a
// weight the operator can move in the panel while the world runs.
//
// The base priorities are spaced ten apart and listed in exactly the order the
// old chain tested, so with every weight at its neutral 100 this picks the same
// goal the chain did, every time. That property is the whole point - the file
// is a tuning surface, not a rewrite - and it is what makes it safe to ship the
// weights to a world with hundreds of bots already living in it.
//
// Three things stay outside the vote, because they are not preferences:
// surviving a fight that is going badly, choosing a profession at level 5, and
// finding a weapon when holding none. A weight that could suppress those would
// not be a slider, it would be a way to break the server from a web form.

namespace
{
	// A bot that is standing in the stable or at the blacksmith is not choosing
	// between errands any more; it is halfway through one. Interrupting that on
	// a weight change would leave it walking away from an NPC it just paid to
	// reach, so these two remain hard.
	bool HasPlayerBotCommittedTownErrand(const TPlayerBotAIState& state)
	{
		return state.bVisitingStable || (state.bVisitingShop && state.bTownNeedBlacksmith);
	}

	bool HasPlayerBotUsableSkillBook(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || ch->GetSkillGroup() == 0)
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			if (IsPlayerBotOwnSkill(ch, skillVnum) &&
					ch->GetSkillMasterType(skillVnum) == SKILL_MASTER &&
					ch->GetSkillLevel(skillVnum) >= 20 && ch->GetSkillLevel(skillVnum) < 30)
				return true;
		}
		return false;
	}

	struct TPlayerBotGoalCandidate
	{
		BYTE bGoal;
		BYTE bWeight;
		int iBase;
	};

	// The base priorities, in the order the old chain tested them. Ten apart so
	// that a neighbouring pair cannot swap by accident, and far enough from zero
	// that the smallest weight still leaves every candidate above nothing.
	const int PLAYERBOT_GOAL_BASE_TOP = 1000;
	const int PLAYERBOT_GOAL_BASE_STEP = 10;

	void OfferPlayerBotGoal(TPlayerBotGoalCandidate* pkList, int& rank, int& count,
			bool bAvailable, BYTE bGoal, BYTE bWeight)
	{
		const int iBase = PLAYERBOT_GOAL_BASE_TOP - rank * PLAYERBOT_GOAL_BASE_STEP;
		++rank;
		if (!bAvailable)
			return;
		pkList[count].bGoal = bGoal;
		pkList[count].bWeight = bWeight;
		pkList[count].iBase = iBase;
		++count;
	}

	// Once an hour, whether the next half hour is an evening of stones. The
	// hunters by role never roll - they are already out there - and a bot too
	// low for any stone worth the walk does not either.
	void RollPlayerBotMetinExpedition(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bBotRole == BOT_ROLE_METIN_HUNTER)
			return;
		// A medal, M2 or M3 dropper farms its own table and goes on no Metin
		// expedition - the half hour that kept a medal dropper of twenty-five
		// out of its dungeon. The Metin dropper's table is the stones.
		if (IsPlayerBotDropper(state.bPersonality) &&
				state.bPersonality != BOT_PERSONALITY_METIN_DROPPER)
		{
			state.dwMetinExpeditionUntil = 0;
			return;
		}
		if (state.dwMetinExpeditionUntil != 0 && dwNow >= state.dwMetinExpeditionUntil)
		{
			state.dwMetinExpeditionUntil = 0;
			state.dwHubChosenTime = 0;
			sys_log(0, "PLAYERBOT_METIN: expedition over pid=%u name=%s level=%u map=%ld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetMapIndex());
		}
		if (state.dwNextMetinExpeditionRoll != 0 && dwNow < state.dwNextMetinExpeditionRoll)
			return;
		// Spread by pid, so the hour's rolls do not all land on the same tick.
		state.dwNextMetinExpeditionRoll = dwNow + PLAYERBOT_METIN_EXPEDITION_ROLL_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d455850U) % 600000U;
		if (state.dwMetinExpeditionUntil != 0 || ch->GetLevel() < PLAYERBOT_METIN_EXPEDITION_MIN_LEVEL)
			return;
		// No expedition from, or towards, a map without stones: the frontier
		// draw sends a third of the bots past fifty-two to the Spider Dungeon,
		// and an expedition there had them travelling for half an hour to
		// find nothing. They roll again next hour, from wherever they stand.
		const long frontier = GetPlayerBotFrontierMapForLevel(ch);
		if (!PlayerBotMapHasMetinStones(ch->GetMapIndex()) ||
				(frontier != 0 && !PlayerBotMapHasMetinStones(frontier)))
			return;
		// A battle horse is what a player raises for the stones, and its rider
		// breaks them from the saddle (CanPlayerBotFightOnHorse), so it goes out
		// for them twice as often. 2.2.6 took the doubling away on a misreading:
		// "do zbijania metinow nikt nie uzywa bojowca" (prodnathin, 23 September)
		// was the complaint that no bot did, not the rule that none should - "tu
		// chodzilo o to, ze nie bili wcale z konia ... akurat na metinie mogliby
		// z bojowca bic", and the operator the next morning: "boty niestety nie
		// uzywaja koni bojowych do bicia metinow".
		const int chance = PLAYERBOT_METIN_EXPEDITION_CHANCE_PERCENT *
				GetPlayerBotWeight(PLAYERBOT_WEIGHT_METIN) / PLAYERBOT_WEIGHT_NEUTRAL *
				(HasPlayerBotBattleHorse(ch) ? 2 : 1);
		if (number(1, 100) > chance)
			return;
		state.dwMetinExpeditionUntil = dwNow + PLAYERBOT_METIN_EXPEDITION_DURATION;
		state.dwHubChosenTime = 0;
		sys_log(0, "PLAYERBOT_METIN: expedition start pid=%u name=%s level=%u map=%ld minutes=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetMapIndex(),
				PLAYERBOT_METIN_EXPEDITION_DURATION / 60000);
	}

	void PlanPlayerBotLongTermGoal(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextGoalPlanTime)
			return;
		state.dwNextGoalPlanTime = dwNow + PLAYERBOT_GOAL_PLAN_INTERVAL + number(0, 1500);

		// --- the three that are never a matter of taste -----------------------
		if (state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * 35))
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_SURVIVE, dwNow);
			return;
		}
		if (ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0)
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_CHOOSE_PROFESSION, dwNow);
			return;
		}
		if (ch->GetWear(WEAR_WEAPON) == NULL)
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
			return;
		}
		if (HasPlayerBotCommittedTownErrand(state))
		{
			SetPlayerBotGoal(ch, state,
					state.bVisitingStable ? BOT_GOAL_HORSE : BOT_GOAL_REFINE, dwNow);
			return;
		}
		// A fishing session owns the tick, and so it owns the goal: the session
		// stamped FISHING every tick and this planner answered RESTOCK every
		// five seconds - forty-three anglers, six thousand goal lines in twelve
		// minutes, and "zapasy" over the head of a bot that was fishing.
		if (state.bFishingSession)
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_FISHING, dwNow);
			return;
		}

		// --- and everything that is ------------------------------------------
		const bool canAdvanceHorse = ShouldPlayerBotPursueHorseExpedition(ch, dwNow);
		const bool hasBiologistMission = GetActivePlayerBotBiologistMission(ch) != NULL;
		const bool hasHuntingMission = GetActivePlayerBotHuntingMission(ch) != NULL;
		const bool canRefine = HasPlayerBotRefineOpportunity(ch);
		const bool canReadBook = HasPlayerBotUsableSkillBook(ch);

		TPlayerBotGoalCandidate candidates[16];
		int rank = 0;
		int count = 0;

		OfferPlayerBotGoal(candidates, rank, count, NeedsPlayerBotPotions(ch),
				BOT_GOAL_RESTOCK, PLAYERBOT_WEIGHT_RESTOCK);
		// Training blocked on alignment needs ordinary hunting, not another stone.
		OfferPlayerBotGoal(candidates, rank, count, PlayerBotNeedsTrainingRank(ch),
				BOT_GOAL_LEVEL_UP, PLAYERBOT_WEIGHT_SKILL);
		OfferPlayerBotGoal(candidates, rank, count,
				state.bAmbition == BOT_AMBITION_EQUIPMENT && canRefine,
				BOT_GOAL_REFINE, PLAYERBOT_WEIGHT_REFINE);
		OfferPlayerBotGoal(candidates, rank, count,
				state.bAmbition == BOT_AMBITION_SKILLS && canReadBook,
				BOT_GOAL_MASTER_SKILL, PLAYERBOT_WEIGHT_SKILL);
		// The medal dropper does not wait for the horse ambition to come round.
		//
		// Farming medals is the whole of what that personality is for, and the
		// ambition rotates: seventy-three bots of eight hundred and thirty-eight
		// hold it at any moment, so thirteen droppers were idle nine times out of
		// ten and all three Monkey Dungeons stood nearly empty - seven bots
		// between them, none at all in the easy one. Everyone else still needs
		// the ambition, so this does not turn the dungeon into a conveyor belt.
		OfferPlayerBotGoal(candidates, rank, count,
				canAdvanceHorse && (state.bAmbition == BOT_AMBITION_HORSE ||
					GetPlayerBotPersonalityByPID(ch->GetPlayerID()) ==
						BOT_PERSONALITY_MEDAL_DROPPER),
				BOT_GOAL_HORSE, PLAYERBOT_WEIGHT_HORSE);
		OfferPlayerBotGoal(candidates, rank, count,
				state.bAmbition == BOT_AMBITION_BIOLOGIST && hasBiologistMission,
				BOT_GOAL_BIOLOGIST, PLAYERBOT_WEIGHT_BIOLOG);
		OfferPlayerBotGoal(candidates, rank, count,
				state.bAmbition == BOT_AMBITION_METINS &&
				IsPlayerBotMetinHunting(state, dwNow),
				BOT_GOAL_HUNT_METIN, PLAYERBOT_WEIGHT_METIN);
		OfferPlayerBotGoal(candidates, rank, count,
				state.bBotRole == BOT_ROLE_PARTY_FIGHTER && ch->GetParty() != NULL,
				BOT_GOAL_PARTY_CHALLENGE, PLAYERBOT_WEIGHT_PARTY);
		OfferPlayerBotGoal(candidates, rank, count, canAdvanceHorse,
				BOT_GOAL_HORSE, PLAYERBOT_WEIGHT_HORSE);
		// Two thirds of the population takes the Biologist ahead of the level-up
		// hunt and a third the other way round, so that neither errand ever
		// empties the maps of the other.
		OfferPlayerBotGoal(candidates, rank, count,
				hasBiologistMission && ch->GetPlayerID() % 3 != 0,
				BOT_GOAL_BIOLOGIST, PLAYERBOT_WEIGHT_BIOLOG);
		OfferPlayerBotGoal(candidates, rank, count, hasHuntingMission,
				BOT_GOAL_HUNTING, PLAYERBOT_WEIGHT_HUNTING);
		OfferPlayerBotGoal(candidates, rank, count, hasBiologistMission,
				BOT_GOAL_BIOLOGIST, PLAYERBOT_WEIGHT_BIOLOG);
		OfferPlayerBotGoal(candidates, rank, count, canRefine,
				BOT_GOAL_REFINE, PLAYERBOT_WEIGHT_REFINE);
		OfferPlayerBotGoal(candidates, rank, count, canReadBook,
				BOT_GOAL_MASTER_SKILL, PLAYERBOT_WEIGHT_SKILL);
		// An expedition outranks the errands below it: the point of the half
		// hour is the stones, and a refine can wait thirty minutes.
		OfferPlayerBotGoal(candidates, rank, count,
				IsPlayerBotMetinHunting(state, dwNow),
				BOT_GOAL_HUNT_METIN, PLAYERBOT_WEIGHT_METIN);
		// Grinding is always available: it is what a bot does when nothing else
		// is asking for it, and it must stay in the vote so that its own weight
		// can pull it above the errands.
		OfferPlayerBotGoal(candidates, rank, count, true,
				BOT_GOAL_LEVEL_UP, PLAYERBOT_WEIGHT_LEVEL);

		BYTE goal = BOT_GOAL_LEVEL_UP;
		int best = -1;
		for (int i = 0; i < count; ++i)
		{
			const int score = WeighPlayerBotPriority(candidates[i].iBase,
					candidates[i].bWeight);
			// Strictly greater, so that equal scores keep the earlier - that is,
			// the historical - candidate.
			if (score > best)
			{
				best = score;
				goal = candidates[i].bGoal;
			}
		}

		SetPlayerBotGoal(ch, state, goal, dwNow);
	}
}

#endif
