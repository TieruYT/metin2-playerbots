#ifndef __INC_METIN2_PLAYERBOT_TOWN_H__
#define __INC_METIN2_PLAYERBOT_TOWN_H__

// A visit to town, from the gate to the last errand: which NPCs this trip is
// for and in what order, walking each leg, the Biologist, and setting up a
// market stall.
//
// A town visit is a small state machine because it has to survive being
// interrupted - a bot that is teleported, killed, or simply loses its route
// mid-errand must be able to pick the trip up rather than start it again. The
// phase in TPlayerBotAIState is that memory, and every branch here either
// advances it or ends the visit.
//
// The stall is engine state with an AI deadline, so releasing one runs at the
// very top of the tick, ahead of everything that could claim it - see
// ManagePlayerBotShopLifetime and the comment in CPlayerBotManager::Update.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_travel.h - that file decides a town trip is due, this one
// carries it out.

namespace
{
	BYTE GetPlayerBotFirstInteriorTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedMisc)
			return BOT_TOWN_PHASE_MISC_MERCHANT;
		if (state.bTownNeedBlacksmith)
			return BOT_TOWN_PHASE_BLACKSMITH;
		return BOT_TOWN_PHASE_NONE;
	}

	bool IsPlayerBotSurplusSkillBook(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_SKILLBOOK)
			return false;
		const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
		if (skillVnum == 0 || ch->GetSkillGroup() == 0 || !IsPlayerBotOwnSkill(ch, skillVnum))
			return true;
		return CountPlayerBotSkillBooksAhead(ch, item, skillVnum) >=
				GetPlayerBotBookKeepLimit(ch, skillVnum);
	}

	// The surplus books beyond what the bag keeps as counter goods, oldest
	// cells first. Empty unless the bag is under pressure: a bag with room is
	// a counter with stock.
	void CollectPlayerBotSafeboxBooks(LPCHARACTER ch, std::vector<WORD>& cells)
	{
		cells.clear();
		if (!ch || (!IsPlayerBotBagFull(ch) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_BAG_PRESSURE_FREE_CELLS))
			return;
		int keep = PLAYERBOT_SAFEBOX_BOOK_KEEP;
		if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_METIN_DROPPER)
			keep = PLAYERBOT_DROPPER_BOOK_KEEP;
		int surplus = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotSurplusSkillBook(ch, item))
				continue;
			surplus += item->GetCount();
			if (surplus > keep)
				cells.push_back(cell);
		}
	}

	int CountPlayerBotSurplusSkillBooks(LPCHARACTER ch)
	{
		int surplus = 0;
		for (WORD cell = 0; ch && cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotSurplusSkillBook(ch, item))
				surplus += item->GetCount();
		}
		return surplus;
	}

	bool HasPlayerBotSafeboxDeposit(LPCHARACTER ch)
	{
		std::vector<WORD> cells;
		CollectPlayerBotSafeboxBooks(ch, cells);
		return !cells.empty();
	}

	// Into the open safebox, the way CInputMain::SafeboxCheckin does it: off the
	// character, onto the first empty slot of the grid. Returns how many books
	// went in; the rest stay in the bag as goods when the page is full.
	int DepositPlayerBotSafeboxBooks(LPCHARACTER ch, CSafebox* box)
	{
		std::vector<WORD> cells;
		CollectPlayerBotSafeboxBooks(ch, cells);
		int deposited = 0;
		for (size_t i = 0; i < cells.size(); ++i)
		{
			LPITEM item = ch->GetInventoryItem(cells[i]);
			if (!item)
				continue;
			bool placed = false;
			for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM && !placed; ++pos)
			{
				if (!box->IsValidPosition(pos) || !box->IsEmpty(pos, item->GetSize()))
					continue;
				char szHint[128];
				snprintf(szHint, sizeof(szHint), "%s %u", item->GetName(), (unsigned int)item->GetCount());
				LogManager::instance().ItemLog(ch, item, "SAFEBOX PUT", szHint);
				item->RemoveFromCharacter();
				box->Add(pos, item);
				placed = true;
				deposited += item->GetCount();
			}
			if (!placed)
			{
				sys_log(0, "PLAYERBOT_TOWN: safebox full pid=%u name=%s deposited=%d left=%u",
						ch->GetPlayerID(), ch->GetName(), deposited, (unsigned int)(cells.size() - i));
				break;
			}
		}
		return deposited;
	}

	BYTE GetPlayerBotFirstExteriorTownPhase(const TPlayerBotAIState& state)
	{
		// Before the trainer, because it is what creates the need for one: the
		// reset leaves the skill group at zero and the trainer is where a group
		// is chosen again.
		if (state.bTownNeedSkillReset)
			return BOT_TOWN_PHASE_SKILL_RESET;
		if (state.bTownNeedTrainer)
			return BOT_TOWN_PHASE_TRAINER;
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
		if (state.bTownNeedSafebox)
			return BOT_TOWN_PHASE_SAFEBOX;
		return BOT_TOWN_PHASE_NONE;
	}

	BYTE GetPlayerBotFirstDirectTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
		if (state.bTownNeedSafebox)
			return BOT_TOWN_PHASE_SAFEBOX;
		if (state.bTownNeedMisc)
			return BOT_TOWN_PHASE_MISC_MERCHANT;
		if (state.bTownNeedBlacksmith)
			return BOT_TOWN_PHASE_BLACKSMITH;
		return BOT_TOWN_PHASE_NONE;
	}

	void StartPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return;
		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;

		state.bTownNeedTrainer = !inM2 && ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN;
		state.bTownNeedSkillReset = !inM2 && ShouldPlayerBotResetSkills(ch, state, dwNow);
		state.bTownNeedMisc = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_MISC) ||
				NeedsPlayerBotPotions(ch) || HasPlayerBotExcessPotions(ch) ||
				NeedsPlayerBotAccessoryShopGoods(ch);
		state.bTownNeedWeaponMerchant = HasPlayerBotJunkForMerchant(
				ch, BOT_MERCHANT_WEAPON) || ch->GetWear(WEAR_WEAPON) == NULL ||
				NeedsPlayerBotProgressionWeapon(ch) || NeedsPlayerBotArrows(ch);
		state.bTownNeedArmorMerchant = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_ARMOR) ||
				NeedsPlayerBotArmorShopGoods(ch);
		state.bTownNeedBlacksmith = HasPlayerBotRefineOpportunity(ch);
		state.bTownNeedSafebox = HasPlayerBotSafeboxDeposit(ch);
		if (!state.bTownNeedTrainer && !state.bTownNeedSkillReset && !state.bTownNeedMisc &&
				!state.bTownNeedWeaponMerchant && !state.bTownNeedSafebox &&
				!state.bTownNeedArmorMerchant && !state.bTownNeedBlacksmith)
		{
			state.dwNextShopCheckTime = dwNow + number(60000, 120000);
			return;
		}

		state.bVisitingShop = true;
		if (inM2)
		{
			// Bokjung has no decorative gate split: visit only the specialists which
			// are needed and then walk straight back to the local hunting fields.
			state.bTownVisitPhase = GetPlayerBotFirstDirectTownPhase(state);
		}
		else
		{
			const bool alreadyInsideTown = ch->GetX() >= 57000 && ch->GetX() <= 63000 &&
					ch->GetY() >= 170000 && ch->GetY() <= 174000;
			if (alreadyInsideTown)
			{
				state.bTownVisitPhase = GetPlayerBotFirstInteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_OUT;
			}
			else
			{
				state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			}
		}
		state.dwTownWaitUntil = 0;
		state.dwNextShopCheckTime = dwNow + 60000;
		state.dwTargetVID = 0;
		state.bStuckCounter = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
	}

	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ salt);
		const int lane = (int)(hash % 11U) - 5;
		const int row = (int)((hash / 11U) % 6U);
		approachX = npcX + lane * 90;
		approachY = npcY - 240 - row * 80;
	}

	void GivePlayerBotBiologistReward(LPCHARACTER ch,
			const TPlayerBotBiologistMission& mission)
	{
		if (!ch)
			return;

		DWORD rewardItem = 0;
		switch (mission.requiredLevel)
		{
			case 4:
				rewardItem = ch->GetJob() == JOB_SHAMAN ? 7003 : 13;
				break;
			case 7:
			{
				const DWORD armorRewards[4] = { 11203, 11403, 11603, 11803 };
				if (ch->GetJob() <= JOB_SHAMAN)
					rewardItem = armorRewards[ch->GetJob()];
				break;
			}
			case 10: rewardItem = 16023; break;
			case 15: rewardItem = 17023; break;
			case 20: rewardItem = 14023; break;
			case 25:
			{
				const DWORD helmetRewards[4] = { 12222, 12362, 12502, 12642 };
				if (ch->GetJob() <= JOB_SHAMAN)
					rewardItem = helmetRewards[ch->GetJob()];
				break;
			}
		}

		if (rewardItem != 0)
			ch->AutoGiveItem(rewardItem, 1, -1, false);
		if (mission.rewardGold > 0)
			ch->PointChange(POINT_GOLD, mission.rewardGold);
		if (mission.rewardExp > 0)
			ch->PointChange(POINT_EXP, mission.rewardExp, true);
	}

	bool CompletePlayerBotBiologistMission(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int completeState = GetPlayerBotBiologistStateIndex(missionIndex, "__complete");
		quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		if (!pc || completeState < 0)
			return false;

		GivePlayerBotBiologistReward(ch, mission);
		ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "collect_count"), 0);
		ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "drink_drug"), 0);
		pc->SetQuestState(mission.questName, completeState);
		sys_log(0, "PLAYERBOT_BIOLOGIST: mission complete pid=%u name=%s quest=%s level=%u gold=%u exp=%u",
				ch->GetPlayerID(), ch->GetName(), mission.questName, mission.requiredLevel,
				mission.rewardGold, mission.rewardExp);
		return true;
	}

	bool ManagePlayerBotBiologist(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop)
			return false;
		if (!state.bVisitingBiologist && dwNow < state.dwNextBiologistCheckTime)
			return false;
		if (!state.bVisitingBiologist)
			state.dwNextBiologistCheckTime = dwNow + 2000;

		size_t missionIndex = 0;
		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch, &missionIndex);
		if (!mission)
		{
			state.bVisitingBiologist = false;
			return false;
		}
		// The mission is taken wherever the bot stands: the quest's kill hook
		// only drops the specimen once the state says so, and a bot that never
		// passed through Joan at the right level would otherwise never collect
		// anything. Only the hand-in needs the Biologist, who is in Joan.
		if (!EnsurePlayerBotBiologistMissionStarted(ch, missionIndex))
			return false;
		if (ch->GetMapIndex() != 21)
		{
			state.bVisitingBiologist = false;
			return false;
		}

		const bool keyPhase = IsPlayerBotBiologistKeyPhase(ch, missionIndex);
		int required = mission->requiredCount;
		const DWORD wantedVnum = GetPlayerBotBiologistWantedItem(ch, missionIndex, &required);
		const int accepted = keyPhase ? 0 : std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, required - accepted);
		const int carried = ch->CountSpecifyItem(wantedVnum);
		// A handful is worth the walk. See PLAYERBOT_BIOLOGIST_MIN_HANDIN: the
		// quest takes them one at a time, so there is nothing to wait for.
		const int worthTheWalk = std::min(remaining, PLAYERBOT_BIOLOGIST_MIN_HANDIN);
		if (!state.bVisitingBiologist && carried < worthTheWalk)
			return false;

		if (!state.bVisitingBiologist)
		{
			state.bVisitingBiologist = true;
			state.dwNextBiologistActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_BIOLOGIST: going to NPC pid=%u name=%s quest=%s carried=%d accepted=%d/%u",
					ch->GetPlayerID(), ch->GetName(), mission->questName,
					carried, accepted, mission->requiredCount);
		}

		SetPlayerBotAction(state, BOT_ACTION_BIOLOGIST, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), PLAYERBOT_BIOLOGIST_X,
				PLAYERBOT_BIOLOGIST_Y, 0x42494f4cU, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true, false, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingBiologist = false;
				state.dwNextBiologistCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_BIOLOGIST: route failed pid=%u name=%s from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextBiologistActionTime == 0)
		{
			state.dwNextBiologistActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextBiologistActionTime)
			return true;

		if (ch->CountSpecifyItem(wantedVnum) <= 0)
		{
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(5000, 12000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		// The second half of the Orc Tooth quest: the stone is handed in, and the
		// reward is what the quest's own last state gives - ten movement speed
		// for sixty years, and the box.
		if (keyPhase)
		{
			ch->RemoveSpecifyItem(wantedVnum, 1);
			ch->AddAffect(AFFECT_COLLECT, POINT_MOV_SPEED, PLAYERBOT_ORC_TOOTH_REWARD_MOV_SPEED,
					0, 60L * 60L * 24L * 365L * 60L, 0, false);
			ch->AutoGiveItem(PLAYERBOT_ORC_TOOTH_REWARD_BOX_VNUM, 1, -1, false);
			sys_log(0, "PLAYERBOT_BIOLOGIST: soul stone handed in pid=%u name=%s quest=%s mov_speed=+%d",
					ch->GetPlayerID(), ch->GetName(), mission->questName, PLAYERBOT_ORC_TOOTH_REWARD_MOV_SPEED);
			CompletePlayerBotBiologistMission(ch, missionIndex);
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(10000, 25000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		ch->RemoveSpecifyItem(mission->itemVnum, 1);
		const bool acceptedNow = number(1, 100) <= mission->acceptPercent;
		int newAccepted = accepted;
		if (acceptedNow)
		{
			newAccepted = accepted + 1;
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(*mission, "collect_count"), newAccepted);
		}
		sys_log(0, "PLAYERBOT_BIOLOGIST: submitted pid=%u name=%s quest=%s accepted_now=%d progress=%d/%u carried_left=%d",
				ch->GetPlayerID(), ch->GetName(), mission->questName, acceptedNow ? 1 : 0,
				newAccepted, mission->requiredCount, ch->CountSpecifyItem(mission->itemVnum));

		// Ten teeth in: the Orc Tooth quest does not end here, it waits in
		// key_item for the stone. The quest's own kill hook drops it, one in
		// five hundred Elite Orcs, once the state says so.
		if (newAccepted >= mission->requiredCount && missionIndex == PLAYERBOT_BIOLOGIST_ORC_TOOTH_INDEX)
		{
			const int keyState = GetPlayerBotBiologistStateIndex(missionIndex, "key_item");
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			if (pc && keyState >= 0)
			{
				pc->SetQuestState(mission->questName, keyState);
				sys_log(0, "PLAYERBOT_BIOLOGIST: teeth accepted, waiting for the soul stone pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(10000, 25000);
			ClearPlayerBotRoute(state, true);
			return false;
		}
		if (newAccepted >= mission->requiredCount &&
				CompletePlayerBotBiologistMission(ch, missionIndex))
		{
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(10000, 25000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		state.dwNextBiologistActionTime = dwNow + number(2500, 5000);
		return true;
	}

	void FinishPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			bool completed)
	{
		state.bVisitingShop = false;
		state.bTownNeedMisc = false;
		state.bTownNeedWeaponMerchant = false;
		state.bTownNeedArmorMerchant = false;
		state.bTownNeedBlacksmith = false;
		state.bTownNeedTrainer = false;
		state.bTownNeedSkillReset = false;
		state.bTownNeedSafebox = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.dwTownWaitUntil = 0;
		state.dwNextShopCheckTime = dwNow +
			(completed ? number(300000, 600000) : number(60000, 120000));
		if (completed)
		{
			state.dwErrandDoneTime = dwNow;
			// The errand is done, so the recovery that was carrying it is over
			// and the departure the audit asked to keep alive can go ahead.
			if (state.bServicePending)
				sys_log(0, "PLAYERBOT_SERVICE: settled pid=%u name=%s map=%ld age_ms=%u",
						ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?",
						ch ? ch->GetMapIndex() : 0,
						state.dwServiceSince != 0 ? dwNow - state.dwServiceSince : 0);
			state.bServicePending = false;
			state.dwServiceRetryAt = 0;
			state.dwServiceSince = 0;
		}
		// Half the bots that finish an errand in Joan stay a while instead of
		// walking straight back out. See PLAYERBOT_TOWN_LINGER_PERCENT: a town
		// with four hundred bots on its map and two dozen in its square does not
		// look like a town, and the stalls that now open there have nobody to
		// stand among. Bokjung is left out on purpose - it is crowded already.
		if (completed && ch && ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 &&
				number(1, 100) <= PLAYERBOT_TOWN_LINGER_PERCENT)
			state.dwTownLingerUntil = dwNow + number(
					(int)PLAYERBOT_TOWN_LINGER_MIN, (int)PLAYERBOT_TOWN_LINGER_MAX);
		// Free, standing in town, errands done: the one moment this bot is the
		// customer the market needs. The shopping timer is cleared rather than
		// left where the visit pushed it - every check that ran during the visit
		// advanced it by two to five minutes and then refused the trip, because a
		// bot on an errand may buy from a counter beside it but may not walk off
		// across town. Joan spent nine hundred bots that way and sent one
		// shopper in fourteen minutes.
		state.dwNextShoppingTime = dwNow;
		state.dwTargetVID = 0;
		state.bStuckCounter = 0;
		if (ch)
		{
			ch->SetVictim(NULL);
			ch->Stop();
			ch->SetPosition(POS_STANDING);
		}
		ClearPlayerBotRoute(state, true);
	}

	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance);

	// A stable tenth of the population runs a market stall - always the same
	// bots, so the market does not move around between restarts. Keeping a shop
	// means not hunting, which is why it stays a minority; and since a keeper
	// only opens when it happens to be in Bokjung with no errand outstanding,
	// the share actually standing at any moment is smaller again.
	// The centre of a town's stall ring, or false for a map that has none.
	// Defined with the chat trade, which comes after the market: the keeper
	// says on the world channel what it has just put up.
	void AnnouncePlayerBotStall(LPCHARACTER ch, const char* pszItemName);

	bool GetPlayerBotShopCentre(long mapIndex, long& pitchX, long& pitchY)
	{
		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M1)
		{
			pitchX = PLAYERBOT_M1_GUARD_X;
			pitchY = PLAYERBOT_M1_GUARD_Y;
			return true;
		}
		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M2)
		{
			pitchX = PLAYERBOT_M2_MARKET_X;
			pitchY = PLAYERBOT_M2_MARKET_Y;
			return true;
		}
		return false;
	}

	bool IsPlayerBotMerchant(const TPlayerBotAIState& state)
	{
		return state.bPersonality == BOT_PERSONALITY_MERCHANT;
	}

	// Who gets a merchant's counter - the bigger table and the medals for sale
	// - without a merchant's day: the droppers hunt and sell, the merchant sells.
	bool IsPlayerBotStallKeeper(const TPlayerBotAIState& state)
	{
		return IsPlayerBotMerchant(state) || IsPlayerBotDropper(state.bPersonality);
	}

	// Too poor for its own potions: see PLAYERBOT_SHOP_POOR_MIN_LEVEL.
	bool IsPlayerBotPoorKeeper(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_SHOP_POOR_MIN_LEVEL)
			return false;
		const bool bBig = ch->GetLevel() >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
		const int tripCost = PLAYERBOT_POTION_TRIP_RED * (bBig ? 40 : 20) +
				PLAYERBOT_POTION_TRIP_BLUE * (bBig ? 64 : 32);
		if (ch->GetGold() >= tripCost)
			return false;
		return PlayerBotNavHash(ch->GetPlayerID() ^
				(DWORD)(get_dword_time() / PLAYERBOT_SHOP_POOR_ROTATION_MS) ^ 0x504f4f52U) %
				PLAYERBOT_SHOP_POOR_ROTATION_SHARE == 0;
	}

	bool ShouldPlayerBotKeepShop(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_SHOP_MIN_LEVEL)
			return false;
		// A bot that cannot afford its potions sells what it has, whatever its
		// personality rolled. So does one whose bag is full: the counter is
		// where the spares a collector will not scrap can go.
		if (IsPlayerBotPoorKeeper(ch) || IsPlayerBotBagFull(ch))
			return true;
		// A trader always has the stall open when it can. For everyone else it
		// stays what it was: an occasional thing one bot in ten does with a spare.
		if (IsPlayerBotMerchant(state))
			return true;
		// A stock of surplus books is a counter, whatever the personality
		// rolled: a book never goes to the merchant, so the counter is the
		// only way it leaves the bag - and the roll left nine bots in ten
		// flying round the stones with bags of books. Not gated on bag
		// pressure: on a world of full bags every keeper would qualify, and
		// on one of half-empty bags none would, while the books sat either
		// way.
		if (CountPlayerBotSurplusSkillBooks(ch) >= PLAYERBOT_SHOP_BOOK_PRESSURE_MIN)
			return true;
		if (IsPlayerBotDropper(state.bPersonality))
		{
			// A dropper whose bag is under pressure sells whatever the roll said:
			// the goods are the point of the personality, and a dropper that lost
			// the roll carried eighty books and picked up nothing.
			if (CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS)
				return true;
			return PlayerBotWeightedRoll(
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x44524f50U) % 1000U,
					PLAYERBOT_DROPPER_SHOP_ROLL, PLAYERBOT_WEIGHT_TRADE);
		}
		// One bot in ten, stretched or shrunk by the TRADE weight. Drawn against a
		// thousand rather than ten so that the weight has somewhere to move: the
		// odds at the neutral 100 are the same one in ten as before, over a
		// different tenth of the population. Three in ten once the bot has
		// nothing left to buy - the same draw, so the one in ten are among them.
		return PlayerBotWeightedRoll(
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x53484f50U) % 1000U,
				IsPlayerBotFullyEquipped(ch) ? PLAYERBOT_FULL_GEAR_SHOP_ROLL : 100,
				PLAYERBOT_WEIGHT_TRADE);
	}

	// What a bot asks for what it puts up. A refined item has no price in the
	// tables - the merchant value is that of the unrefined base - so above +6
	// the number is ours. Deliberately modest: the point is that another bot can
	// actually buy it after an hour of hunting.
	DWORD ApplyPlayerBotBonusPremium(DWORD price, int bonusPercent)
	{
		if (bonusPercent <= 0)
			return price;
		return std::max<DWORD>(1, (DWORD)((unsigned long long)price *
				(unsigned long long)(100 + bonusPercent) / 100ULL));
	}

	DWORD GetPlayerBotShopAskingPrice(LPITEM item)
	{
		if (!item)
			return 1;
		// What is rolled on this particular piece, worked out once and applied to
		// every way out of this function. The three refine prices below are flat
		// by design - a +7 has no merchant price to scale - and returning them
		// unscaled is what left boots +7 with five bonus lines on a counter at
		// the same 150 000 as boots +7 with none.
		const int bonusPercent = GetPlayerBotBonusPricePercent(item);
		const BYTE refine = item->GetRefineLevel();
		if (refine >= 9)
			return ApplyPlayerBotBonusPremium(PLAYERBOT_SHOP_PRICE_PLUS9, bonusPercent);
		if (refine == 8)
			return ApplyPlayerBotBonusPremium(PLAYERBOT_SHOP_PRICE_PLUS8, bonusPercent);
		if (refine == 7)
			return ApplyPlayerBotBonusPremium(PLAYERBOT_SHOP_PRICE_PLUS7, bonusPercent);
		// A skill book's own market, and the goods whose merchant price says
		// nothing about what they are worth here. Both come from the audit of
		// 8 September: the merchant charges a thousand yang for every book
		// whatever skill is in its socket, and the pearls' proto prices were set
		// for wallets a hundred times smaller than these.
		const DWORD bookSkill = item->GetType() == ITEM_SKILLBOOK
				? GetPlayerBotSkillBookSkillVnum(item) : 0;
		const DWORD npcUnit = GetPlayerBotNpcSellUnitPrice(item);
		// Scrap gear is priced as scrap: twice what the merchant pays, so the
		// player burning it at the blacksmith is not paying market money for it.
		// Never scrap, whatever the refine: see PLAYERBOT_PRIOR_LEVEL30_WEAPON.
		const bool bLevel30 = IsPlayerBotSpecialLevel30Weapon(item);
		if (!bLevel30 &&
				(item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				refine < PLAYERBOT_SHOP_MIN_GEAR_REFINE)
			return ApplyPlayerBotBonusPremium(
					std::max<DWORD>(1, npcUnit * PLAYERBOT_SCRAP_PRICE_MULT), bonusPercent);
		DWORD unit = npcUnit * PLAYERBOT_SHOP_MATERIAL_MARKUP;
		if (bLevel30)
			unit = std::max(unit, PLAYERBOT_PRIOR_LEVEL30_WEAPON);
		// The opening prices. Blended away by the sale memory below as real
		// transactions accumulate - a prior is where a price starts, not where
		// it stays.
		if (bookSkill != 0)
		{
			if (bookSkill == 4)
				unit = PLAYERBOT_PRIOR_BOOK_AURA;
			else if (bookSkill == 63)
				unit = PLAYERBOT_PRIOR_BOOK_ENCHANTED_BLADE;
			else if (bookSkill == 19)
				unit = PLAYERBOT_PRIOR_BOOK_STRONG_BODY;
			else
				unit = std::max(unit, PLAYERBOT_PRIOR_BOOK_ORDINARY);
		}
		else if (item->GetVnum() == PLAYERBOT_PEARL_FIRST_VNUM)
			unit = PLAYERBOT_PRIOR_PEARL_WHITE;
		else if (item->GetVnum() == PLAYERBOT_PEARL_FIRST_VNUM + 1)
			unit = PLAYERBOT_PRIOR_PEARL_BLUE;
		else if (item->GetVnum() == PLAYERBOT_PEARL_LAST_VNUM)
			unit = PLAYERBOT_PRIOR_PEARL_RED;
		else if (item->GetVnum() == PLAYERBOT_SHELLFISH_VNUM)
			unit = PLAYERBOT_PRIOR_SHELLFISH;
		else if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			unit = PLAYERBOT_PRIOR_HORSE_MEDAL;
		// A soul stone has no merchant price: the counter asks by grade.
		if (item->GetType() == ITEM_METIN)
			unit = PLAYERBOT_SHOP_PRICE_SOUL_STONE[std::min(4, GetPlayerBotSoulStoneGrade(item->GetVnum()))];

		// Anything the merchant refuses to buy has no prior at all, and the
		// wallet block below is skipped whole while the ledger has not run yet -
		// the first minute after every start. That pair is what put horse medals
		// and unopened chests on the counters at one yang each.
		if (unit == 0)
			unit = PLAYERBOT_PRIOR_NO_MERCHANT_PRICE;

		// And scaled to the buyers' wallets, where that is more - see the
		// PLAYERBOT_MARKET_*_WALLET_* constants for why the merchant's markup
		// alone was a giveaway. A soul stone keeps its grade table.
		const DWORD wallet = GetPlayerBotMarketMedianWallet();
		if (wallet > 0 && item->GetType() != ITEM_METIN)
		{
			DWORD permille = PLAYERBOT_MARKET_OTHER_WALLET_PERMILLE;
			if (IsPlayerBotTradeableMaterial(item))
				permille = PLAYERBOT_MARKET_MATERIAL_WALLET_PERMILLE;
			else if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) && refine > 2)
				permille = PLAYERBOT_MARKET_GEAR_WALLET_PERMILLE_PER_REFINE * (refine - 2);
			const unsigned long long count = std::max<DWORD>(1, item->GetCount());
			// The wallet says what the market can afford, the merchant's own
			// price says how two materials rank against each other, and taking
			// the wallet number flat threw the ranking away - see
			// PLAYERBOT_MARKET_WALLET_REFERENCE_PRICE.
			//
			// Materials only. Gear already has its own scale in this block - a
			// permille per refine level - and a piece of level-50 armour the
			// merchant values at twenty thousand would come out eight times
			// dearer for no reason anybody asked for.
			DWORD worthPercent = PLAYERBOT_MARKET_WALLET_WORTH_MIN_PERCENT;
			if (IsPlayerBotTradeableMaterial(item))
			{
				worthPercent = (DWORD)((unsigned long long)npcUnit * 100 /
						PLAYERBOT_MARKET_WALLET_REFERENCE_PRICE);
				worthPercent = std::max(PLAYERBOT_MARKET_WALLET_WORTH_MIN_PERCENT,
						std::min(PLAYERBOT_MARKET_WALLET_WORTH_MAX_PERCENT, worthPercent));
			}
			const DWORD walletUnit = (DWORD)((unsigned long long)wallet * permille /
					1000 * worthPercent / 100);
			const DWORD stackCap = (DWORD)((unsigned long long)wallet *
					PLAYERBOT_MARKET_STACK_WALLET_PERCENT / 100 / count);
			unit = std::max(unit, std::max<DWORD>(1, std::min(walletUnit, stackCap)));
		}
		// That is the prior: what the counter asks before the market has said
		// anything.
		const DWORD prior = unit;
		const DWORD dwNow = get_dword_time();

		// What the market has paid, blended with the prior by how much of it
		// there is - w = n / (n + n0), in log space so a tenfold gap is halved
		// rather than averaged. Two sales move the number a third of the way;
		// the full memory of eight, two thirds. It used to replace the prior
		// outright at two sales, so one bot buying twice at a bad price set
		// the price of the thing for everybody. Kept within a band round the
		// prior - a quarter of it to four times it, and never under the
		// merchant's own price, below which the stall is a worse deal than the
		// NPC for the seller - so one overpayment cannot price a material out
		// of every other bot's reach, and one giveaway cannot drag it back to
		// the merchant's pennies.
		size_t samples = 0;
		const DWORD paid = GetPlayerBotSaleUnitPrice(item->GetVnum(), refine, dwNow,
				&samples, bookSkill);
		if (paid != 0)
		{
			const DWORD floor = std::max<DWORD>(std::max<DWORD>(1, npcUnit),
					prior / PLAYERBOT_SALE_PRICE_CAP_MULT);
			const DWORD cap = prior == 0 ? PLAYERBOT_SALE_PRICE_CAP_FLAT
					: prior * PLAYERBOT_SALE_PRICE_CAP_MULT;
			const DWORD market = std::min(std::max(paid, floor), std::max(floor, cap));
			if (prior == 0)
				unit = market;
			else
			{
				const double w = (double)samples /
						(double)(samples + PLAYERBOT_MARKET_ANCHOR_N0);
				unit = (DWORD)(exp((1.0 - w) * log((double)prior) +
						w * log((double)market)) + 0.5);
			}
		}

		// Then the ledger, for a material: (D + q0) / (S + q0) to the fifth
		// root, within [0.75, 1.35] - so twenty bots short of a thing against
		// five units on the counters asks a fifth more, not four times. Nothing
		// when the ledger has seen neither a counter nor a buyer: that is no
		// information, not a shortage.
		if (unit > 0 && IsPlayerBotTradeableMaterial(item))
		{
			const TPlayerBotMarketLedgerEntry* entry = GetPlayerBotMarketLedgerEntry(item->GetVnum());
			if (entry && (entry->dwDemandBots > 0 || entry->dwSupplyUnits > 0))
			{
				const double ratio =
						(double)(entry->dwDemandBots + PLAYERBOT_MARKET_REGULATOR_Q0) /
						(double)(entry->dwSupplyUnits + PLAYERBOT_MARKET_REGULATOR_Q0);
				const double mult = std::max(PLAYERBOT_MARKET_REGULATOR_MIN,
						std::min(PLAYERBOT_MARKET_REGULATOR_MAX,
							pow(ratio, PLAYERBOT_MARKET_REGULATOR_EXPONENT)));
				unit = std::max<DWORD>(1, (DWORD)(unit * mult + 0.5));
			}
		}

		// And a bounded step from wherever the last counter had it, so the
		// market's price of a thing drifts rather than jumps.
		unit = LimitPlayerBotAskStep(item->GetVnum(), refine, std::max<DWORD>(1, unit),
				dwNow, bookSkill);
		// And last, the lines rolled on it - on top of the step limiter rather
		// than under it, because the limiter and the sale memory are both keyed
		// by vnum and refine, the one pair that cannot tell two otherwise
		// identical pieces apart.
		unit = ApplyPlayerBotBonusPremium(unit, bonusPercent);
		const DWORD price = unit * (DWORD)item->GetCount();
		return price == 0 ? 1U : price;
	}

	// How much a stall wants this on its counter rather than in the bag. Higher
	// wins. Nothing scores what the bot still needs itself: that is filtered out
	// before scoring, not scored badly.
	// Does this item carry a bonus line worth more than the item itself? A large
	// health roll or a shield rolled with block or reflect is what a player looks
	// for, and it is worth a counter slot at any refine.
	bool HasPlayerBotValuableBonus(LPITEM item)
	{
		if (!item)
			return false;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			if (value <= 0)
				continue;
			if (type == APPLY_MAX_HP && value >= PLAYERBOT_VALUABLE_HP_BONUS)
				return true;
			// A shield is bought for what it stops, not for its defence number,
			// and what it is bought for is immunity to stun - "NNO". This used to
			// say block or reflect, which was a guess.
			if (item->GetType() == ITEM_ARMOR &&
					item->GetSubType() == ARMOR_SHIELD &&
					type == APPLY_IMMUNE_STUN)
				return true;
		}
		return false;
	}

	// The two slots where a piece below level 30 is still the best a bot can get.
	// Shields and helmets go straight from the starter tier to level 41, so the
	// level-21 one is what everybody between 21 and 40 wears - which is why it
	// sells, and why it is the exception to the rule below it. Every other slot
	// has a tier in the twenties that is merely one step behind the thirties.
	bool IsPlayerBotTopSlotLowLevelGear(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_ARMOR)
			return false;
		const BYTE sub = item->GetSubType();
		if (sub != ARMOR_SHIELD && sub != ARMOR_HEAD)
			return false;
		return item->GetLevelLimit() >= PLAYERBOT_SHOP_TOP_SLOT_GEAR_LEVEL;
	}

	// Horse medals are the one thing a bot farms for itself for hours. A trader
	// has no such errand - it does not go to the Monkey Dungeon at all - and a
	// bot whose horse is already at the level cap has nothing left to spend them
	// on, so for those two the medals are stock like anything else.
	bool CanPlayerBotSellHorseMedals(LPCHARACTER ch, bool merchant)
	{
		if (merchant)
			return true;
		// The medal dropper is the medal shop: it farms them to put them up. It
		// used to hold them while its own horse could use one, and a dropper of
		// forty on a horse of ten - a battle horse candidate, forbidden to spend
		// one - could neither use nor sell what it had.
		if (ch && GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_MEDAL_DROPPER)
			return true;
		return ch && ch->GetHorseLevel() >= 10 &&
				ch->GetLevel() < GetPlayerBotNextHorseRequiredLevel(ch->GetHorseLevel());
	}

	// Whether the market wants another stack of this material on a counter,
	// and the reason when it does not. LIST while the counters hold fewer
	// units than the bots short of it would buy; PROBE when nobody is short of
	// it and nothing of it is on sale, so one stack finds out; NO_DEMAND when
	// a stack is already finding out; OVERSTOCK when the buyers are covered.
	// The two refusals are logged with the numbers, once a minute, because a
	// material held back looks exactly like a material never dropped.
	int DecidePlayerBotMaterialListing(LPCHARACTER ch, LPITEM item, bool report)
	{
		const TPlayerBotMarketLedgerEntry* entry = GetPlayerBotMarketLedgerEntry(item->GetVnum());
		const DWORD supply = entry ? entry->dwSupplyUnits : 0;
		const DWORD demand = entry ? entry->dwDemandBots : 0;
		int decision;
		if (demand == 0)
			decision = supply == 0 ? PLAYERBOT_LIST_PROBE : PLAYERBOT_LIST_NO_DEMAND;
		else
		{
			const DWORD target = demand * PLAYERBOT_MARKET_SUPPLY_PER_BUYER *
					PLAYERBOT_MARKET_SUPPLY_MARGIN_PERCENT / 100;
			decision = supply < target ? PLAYERBOT_LIST_LIST : PLAYERBOT_LIST_OVERSTOCK;
		}
		if (!report)
			return decision;
		++s_auMarketDecisions[decision];
		if (decision == PLAYERBOT_LIST_NO_DEMAND || decision == PLAYERBOT_LIST_OVERSTOCK)
			PlayerBotLogThrottled("PLAYERBOT_MARKET_HELD", get_dword_time(),
					"PLAYERBOT_MARKET: held pid=%u name=%s vnum=%u count=%u reason=%s supply=%u demand=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
					(unsigned int)item->GetCount(), s_apszMarketDecisionNames[decision],
					supply, demand);
		return decision;
	}

	int ScorePlayerBotShopStock(LPCHARACTER ch, LPITEM item, bool merchant, bool report)
	{
		if (!item)
			return -1;
		// A weapon from the level-30 set is the prize of this whole market. It is
		// worth a counter slot at any refine at all, unrefined included.
		if (IsPlayerBotSpecialLevel30Weapon(item))
			return 2000;
		// Then anything rolled with a bonus a player would go looking for.
		if (HasPlayerBotValuableBonus(item))
			return 1500;
		// A spare at +6 or better is worth walking across town for, and is the one
		// thing that must never reach an NPC merchant for a fifth of its worth.
		if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			return 1000 + item->GetRefineLevel();
		// A material this bot is short of stays in its own bag.
		if (PlayerBotNeedsRefineMaterial(ch, item->GetVnum()))
			return -1;
		if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return CanPlayerBotSellHorseMedals(ch, merchant) ? 900 : -1;
		// Refine materials: what every other bot is short of and would otherwise
		// have to farm for an hour. Only the ones some recipe actually consumes
		// rank this high - the rest of ITEM_MATERIAL is scenery to an anvil.
		if (IsPlayerBotTradeableMaterial(item))
		{
			// ...and only as many of them as the market is short of. A probe
			// ranks just below a wanted material, so a counter with both shows
			// the wanted one first.
			const int decision = DecidePlayerBotMaterialListing(ch, item, report);
			if (decision == PLAYERBOT_LIST_LIST)
				return 500;
			if (decision == PLAYERBOT_LIST_PROBE)
				return 450;
			return -1;
		}
		// Hair dye: the one the bot is wearing is spent, the rest are stock.
		// Ranked above ordinary spare gear because there is nowhere else in this
		// world to buy one.
		if (IsPlayerBotHairDye(item->GetVnum()))
			return 900;
		// A Forgetting Scroll sells well; the keeper keeps it only while one of
		// its own skills is waiting for it.
		if (item->GetVnum() == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return GetPlayerBotStuckSkill(ch) != 0 ? -1 : 800;
		// An ITEM_MATERIAL no recipe consumes is scenery, not goods: it was put
		// up for its type, and its type is not a reason anybody would buy it.
		if (item->GetRefinedVnum() == 0 && item->GetType() == ITEM_MATERIAL)
			return -1;
		// A soul stone the bot cannot seat - the wrong school's, no socket open,
		// the wrong grade for the piece it keeps - is somebody else's set.
		if (item->GetType() == ITEM_METIN)
			return WantsPlayerBotSoulStone(ch, item->GetVnum(), (DWORD)item->GetValue(5))
					? -1 : 700 + GetPlayerBotSoulStoneGrade(item->GetVnum()) * 100;
		// Skill books. Stock for everyone; the Metin dropper's whole trade, so
		// on its counter they go up beside the level-30 weapons.
		if (item->GetType() == ITEM_SKILLBOOK)
		{
			// Its own, within what it keeps to read, stays in the bag: a counter
			// used to carry the very book its keeper was waiting to read.
			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			if (ch->GetSkillGroup() != 0 && IsPlayerBotOwnSkill(ch, skillVnum) &&
					CountPlayerBotSkillBooksAhead(ch, item, skillVnum) <
						GetPlayerBotBookKeepLimit(ch, skillVnum))
				return -1;
			return GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_METIN_DROPPER
					? 1800 : 400;
		}

		// Ordinary spare gear, and only if somebody could want it. This used to
		// be "return 1" for absolutely everything else, which is how counters
		// filled up with +1, +2 and +3 spares: a bot with eight of those and
		// nothing better put all eight out. There are nineteen hundred such
		// pieces in this world's bags against two hundred at +4 or better, so
		// that one line decided what the whole market looked like.
		const BYTE type = item->GetType();
		if (type == ITEM_WEAPON || type == ITEM_ARMOR)
		{
			// A scrap keeper puts the low refines out too, last in line after
			// everything worth more: fodder for a player's blacksmith runs.
			if (IsPlayerBotScrapKeeper(ch->GetPlayerID()) &&
					item->GetRefineLevel() < PLAYERBOT_SHOP_MIN_GEAR_REFINE)
				return 100 + item->GetRefineLevel();
			if (item->GetRefineLevel() < PLAYERBOT_SHOP_MIN_GEAR_REFINE)
				return -1;
			// The refine floor alone let the whole of the twenties through, and
			// the twenties are what a bot has just stopped wearing: 272 of the
			// 487 spares at +4 or +5 in this world are for level 29 or below.
			// A +4 body armour for level 26, offered to a market whose customers
			// are level 30 and up, is junk at any refine.
			if (item->GetLevelLimit() < PLAYERBOT_SHOP_MIN_GEAR_LEVEL &&
					!IsPlayerBotTopSlotLowLevelGear(item))
				return -1;
			return 100;
		}

		// An unopened box. Ranked between the materials and the spare gear: it
		// is a gamble somebody might want, not a thing anybody came for.
		if (IsPlayerBotSurplusChest(item))
			return 350;
		// A specimen of a mission already handed in. The Orc Tooth never gets
		// here: it is a refine material and the material branch above priced
		// it, ledger and all.
		if (IsPlayerBotBiologistSpecimenSurplus(ch, item->GetVnum()))
			return PLAYERBOT_SHOP_SPECIMEN_SCORE;

		// Whatever is left is the bot's own business, not goods. A stall with two
		// things worth buying beats one padded out to eight.
		return -1;
	}

	// Standing about in town, because that is what a town is for.
	//
	// Claims the tick while it runs, so the wandering does not walk the bot back
	// out to the fields - and the inactivity watchdog is told about it in the
	// manager, since a bot resting on purpose is still and that is the point.
	// Anything with a claim on the bot ends it: an errand, a stall of its own, a
	// shopping trip, a retreat, or simply leaving the map.
	bool ManagePlayerBotTownLinger(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.dwTownLingerUntil == 0)
			return false;
		// Over for good: the clock ran out, the bot left Joan, it opened a stall
		// of its own, or it is busy staying alive.
		if (dwNow >= state.dwTownLingerUntil ||
				ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 ||
				ch->GetMyShop() || state.bTacticalRetreat || state.bRecoveringAfterDeath)
		{
			state.dwTownLingerUntil = 0;
			state.dwTownBrowseUntil = 0;
			return false;
		}
		// Merely interrupted: an errand, a shopping trip, a cast or a fight has
		// the tick for now and the rest resumes when it is done. Clearing the
		// clock here instead is what made this never happen at all - an angler
		// that finishes a session is usually out of bait, so a town visit starts
		// on the very next tick and cancelled the rest before it began.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bMarketTrip || state.bFishingSession || ch->GetVictim() != NULL)
			return false;

		long pitchX = 0, pitchY = 0;
		if (!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY))
		{
			state.dwTownLingerUntil = 0;
			return false;
		}
		// A new place on the market ring every few seconds, rather than one spot
		// held for the whole visit. The salt is the clock, so each stop is
		// somewhere else and the square keeps moving; what a passer-by sees is
		// people walking between the counters, which is what a market looks
		// like. This is the whole of the change asked for on the Discord: bots
		// were standing still in town for up to ten minutes at a time.
		SetPlayerBotAction(state, BOT_ACTION_TOWN_REST, dwNow);
		const bool arrived = state.dwTownBrowseUntil != 0 &&
				DISTANCE_APPROX(ch->GetX() - state.lTownBrowseX,
						ch->GetY() - state.lTownBrowseY) <= PLAYERBOT_MARKET_ARRIVE;
		// A counter it cannot reach is abandoned rather than walked at for ever:
		// without the second clause a bot whose route keeps failing would stand
		// facing an unreachable point for the whole visit, which is exactly the
		// standing still this replaced.
		if (state.dwTownBrowseUntil == 0 || (arrived && dwNow >= state.dwTownBrowseUntil) ||
				dwNow >= state.dwTownBrowseUntil + PLAYERBOT_TOWN_BROWSE_GIVE_UP)
		{
			long offsetX = 0, offsetY = 0;
			GetPlayerBotStableOffset(ch->GetPlayerID() ^ (dwNow >> 13), 0x52455354U,
					PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS + 900,
					offsetX, offsetY);
			state.lTownBrowseX = pitchX + offsetX;
			state.lTownBrowseY = pitchY + offsetY;
			state.dwTownBrowseUntil = dwNow + number(
					(int)PLAYERBOT_TOWN_BROWSE_MIN, (int)PLAYERBOT_TOWN_BROWSE_MAX);
		}
		if (DISTANCE_APPROX(ch->GetX() - state.lTownBrowseX,
				ch->GetY() - state.lTownBrowseY) > PLAYERBOT_MARKET_ARRIVE)
		{
			MovePlayerBot(ch, state.lTownBrowseX, state.lTownBrowseY, dwNow, 6, true);
			return true;
		}
		// Standing at a counter for a moment is the looking; the clock above
		// sends it to the next one.
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_STANDING);
		return true;
	}

	// Is this worth putting a sign up for? Three lines make a stall; fewer than
	// that only if one of them is the reason somebody would cross the market for
	// it - a level-30 weapon, a big bonus roll, anything at +6, a horse medal.
	// The caller passes the best score it has, because that is exactly what
	// ScorePlayerBotShopStock spent its time working out.
	bool IsPlayerBotStallWorthOpening(size_t lines, int bestScore, bool poor = false)
	{
		if (lines == 0)
			return false;
		// A clearance sale is worth a sign with one line on it.
		return poor || lines >= PLAYERBOT_SHOP_MIN_ITEMS ||
				bestScore >= PLAYERBOT_SHOP_PRIZE_SCORE;
	}

	// Everything this bot can legitimately part with, best first. OpenMyShop
	// refuses equipped, locked and ANTI_GIVE/ANTI_MYSHOP items outright - and it
	// refuses the *whole* shop over one bad line, not just that line - so the
	// same rules are applied here rather than letting the call fail silently.
	void CollectPlayerBotShopItems(LPCHARACTER ch,
			std::vector<std::pair<int, WORD> >& outScored, bool merchant)
	{
		outScored.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		// Once a minute per bot the ledger decisions are counted and the
		// refusals logged; the other scans of the same bag say nothing.
		const bool report = ShouldReportPlayerBotMarketDecisions(
				ch->GetPlayerID(), get_dword_time());
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const TItemTable* proto = item->GetProto();
			if (!proto || IS_SET(proto->dwAntiFlags,
					ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
				continue;
			// Not the vendor-trash rule: a stall should carry something a player
			// might actually want. Materials and spare loot qualify; the bot's own
			// supplies, weapons and armour do not, so it can never sell the gear
			// or the potions it needs to keep playing.
			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051 ||
					vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				continue;
			// Biologist specimens stay: they are quest progress, not goods. Horse
			// medals used to be excluded here as well, which meant nobody could
			// ever buy one; whether they are for sale is now the scoring's call.
			if (((vnum >= 50701 && vnum <= 50706) || vnum == PLAYERBOT_ORC_TOOTH_VNUM ||
					vnum == PLAYERBOT_JINUNGGYI_STONE_VNUM) &&
					!IsPlayerBotBiologistSpecimenSurplus(ch, vnum))
				continue;
			// Spare gear is the most interesting thing a stall can offer, but the
			// bot must never put up the only weapon or armour it owns for a slot
			// it is still walking around empty. Something already worn there means
			// what it carries is genuinely a spare.
			const BYTE type = item->GetType();
			if (type == ITEM_WEAPON || type == ITEM_ARMOR)
			{
				// Something the bot ought to be wearing is not a spare, whatever
				// the slot says. This pass runs near the top of the tick and the
				// equipment pass near the bottom, so without this line the
				// counter always won the race for a gift.
				if (IsPlayerBotWearableUpgrade(ch, item, cell))
					continue;
				// The Archer's stone weapon (playerbot_gear.h) is not goods.
				if (IsPlayerBotArcherBuild(ch) && IsPlayerBotStoneMeleeWeapon(ch, item) &&
						FindPlayerBotStoneWeapon(ch, false) == item)
					continue;
				const int wearCell = item->FindEquipCell(ch);
				if (wearCell < 0 || ch->GetWear((BYTE)wearCell) == NULL)
					continue;
			}
			const int score = ScorePlayerBotShopStock(ch, item, merchant, report);
			if (score > 0)
				outScored.push_back(std::make_pair(score, cell));
		}
		// Best first, so a counter that cannot hold everything holds the part
		// worth walking across town for.
		std::sort(outScored.begin(), outScored.end(),
				std::greater<std::pair<int, WORD> >());
		const size_t limit = merchant
				? (size_t)PLAYERBOT_SHOP_MERCHANT_ITEMS
				: (size_t)PLAYERBOT_SHOP_MAX_ITEMS;

		// Half the counter is kept for refine materials. Ranking on worth alone
		// buried them: a level-30 weapon, a good bonus roll and every spare at +6
		// all outrank a material, and a bot with a few of those filled all eight
		// slots with gear. Materials are what another bot actually walks the
		// market for - the alternative is farming the same one for an hour - so a
		// counter that has them always shows some.
		const size_t reserved = limit / 2;
		std::vector<std::pair<int, WORD> > materials;
		std::vector<std::pair<int, WORD> > rest;
		for (size_t i = 0; i < outScored.size(); ++i)
		{
			LPITEM item = ch->GetInventoryItem(outScored[i].second);
			const bool isMaterial = IsPlayerBotTradeableMaterial(item);
			if (isMaterial && materials.size() < reserved)
				materials.push_back(outScored[i]);
			else
				rest.push_back(outScored[i]);
		}
		outScored = materials;
		for (size_t i = 0; i < rest.size() && outScored.size() < limit; ++i)
			outScored.push_back(rest[i]);
		// Worth order again, so the best of whatever made the cut leads.
		std::sort(outScored.begin(), outScored.end(),
				std::greater<std::pair<int, WORD> >());
	}

	// A good refine is the one moment worth breaking the bots' silence for. They
	// say nothing when attacked, nothing during PvP, and nothing on a kill -
	// only the blacksmith gets a reaction, and even then rarely.
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, LPITEM item, int newPlus)
	{
		if (!ch || !item || newPlus < 7)
			return;
		// Same rule as the overhead line: a bot minding a stall says nothing.
		if (ch->GetMyShop())
			return;

		static DWORD s_dwLastShoutTime = 0;
		const DWORD dwNow = get_dword_time();
		// One announcement every few minutes for the whole world: the chat should
		// feel inhabited, not flooded.
		if (s_dwLastShoutTime != 0 && dwNow < s_dwLastShoutTime + 180000)
			return;
		if (number(1, 100) > 45)
			return;

		static const char* kPlus7[] = {
			"%s poszedl na +7, kowal dzis laskawy",
			"no i mam +7 na %s, moglo byc gorzej",
			"+7 na %s siadlo za pierwszym razem",
			"udalo sie, %s na +7"
		};
		static const char* kPlus8[] = {
			"%s na +8! rece mi sie trzesly",
			"jest +8 na %s, teraz sie zastanawiam czy pchac dalej",
			"+8 na %s, chyba mam dzis szczescie",
			"weszlo na +8, %s gotowy do roboty"
		};
		static const char* kPlus9[] = {
			"%s NA +9!!! nie wierze",
			"+9 na %s, kto by pomyslal",
			"dziewiatka na %s, dzis stawiam :D",
			"%s +9, chyba wystarczy tych probek na dzis"
		};

		const char** pool = kPlus7;
		if (newPlus >= 9)
			pool = kPlus9;
		else if (newPlus == 8)
			pool = kPlus8;

		char msg[CHAT_MAX_LEN + 1];
		char body[CHAT_MAX_LEN + 1];
		snprintf(body, sizeof(body), pool[number(0, 3)], item->GetName());
		snprintf(msg, sizeof(msg), "%s : %s", ch->GetName(), body);

		s_dwLastShoutTime = dwNow;
		SendShout(msg, ch->GetEmpire());
		sys_log(0, "PLAYERBOT_SHOUT: pid=%u plus=%d text=%s",
				ch->GetPlayerID(), newPlus, msg);
	}

	// Where a bot belongs on each map we manage: the point that map is entered
	// by, and Bokjung for anything else.
	void GetPlayerBotHomePoint(long mapIndex, long& outMap, long& outX, long& outY)
	{
		outMap = PLAYERBOT_MAP_CHUNJO_M2;
		outX = PLAYERBOT_M2_FROM_M3_X;
		outY = PLAYERBOT_M2_FROM_M3_Y;
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_CHUNJO_M1:
				outMap = mapIndex; outX = PLAYERBOT_M1_RETURN_X; outY = PLAYERBOT_M1_RETURN_Y; break;
			case PLAYERBOT_MAP_CHUNJO_M2:
				outMap = mapIndex; outX = PLAYERBOT_M2_ARRIVAL_X; outY = PLAYERBOT_M2_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_CHUNJO_M3:
				outMap = mapIndex; outX = PLAYERBOT_M3_ARRIVAL_X; outY = PLAYERBOT_M3_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_MONKEY_EASY:
			case PLAYERBOT_MAP_MONKEY_MEDIUM:
			case PLAYERBOT_MAP_MONKEY_HARD:
				if (GetPlayerBotMonkeyArrival(mapIndex, outX, outY))
					outMap = mapIndex;
				break;
			default:
				if (GetPlayerBotFrontierArrival(mapIndex, outX, outY))
					outMap = mapIndex;
				break;
		}
	}

	// A character with no sector, or one standing on a map this core does not
	// host, cannot move at all - and nothing else in the tick can put it back.
	// It is asked again on the next tick and answers the same way, for as long as
	// the server runs: a day of logs held 35k such lines from 45 bots that never
	// took another step, plus the watchdog resetting them 8k times to no effect.
	bool RescuePlayerBotWithoutSectree(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		const long mapIndex = ch->GetMapIndex();
		if (ch->GetSectree() && SECTREE_MANAGER::instance().GetMap(mapIndex) != NULL)
			return false;
		if (state.dwNextSectreeRescueTime != 0 && dwNow < state.dwNextSectreeRescueTime)
			return true; // already tried recently; do not spin
		state.dwNextSectreeRescueTime = dwNow + 30000;

		long homeMap = 0, homeX = 0, homeY = 0;
		GetPlayerBotHomePoint(mapIndex, homeMap, homeX, homeY);
		if (TransitionPlayerBotMap(ch, state, homeMap, homeX, homeY, dwNow, "sectree_rescue"))
		{
			sys_log(0, "PLAYERBOT_RESCUE: pid=%u name=%s had no sectree on map=%ld, moved to map=%ld (%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), mapIndex, homeMap, homeX, homeY);
		}
		return true;
	}

	// A stall is engine state; the deadline that ends it is AI state. Keeping the
	// two in step is the whole job of this pair of helpers, and doing it from a
	// single place is what makes it possible to run the release *before* the
	// subsystems that can claim the tick.
	// Take back a shop sign the engine has broadcast for a shop that does not
	// exist. CloseMyShop does this itself, but only for a shop it can find -
	// which is exactly the case this is for.
	void ClearPlayerBotShopSign(LPCHARACTER ch)
	{
		if (!ch)
			return;
		TPacketGCShopSign p;
		p.bHeader = HEADER_GC_SHOP_SIGN;
		p.dwVID = ch->GetVID();
		p.szSign[0] = '\0';
		ch->PacketAround(&p, sizeof(TPacketGCShopSign));
	}

	// Defined below, with the counter's line bookkeeping.
	LPITEM FindPlayerBotOfferItem(LPCHARACTER keeper, const TPlayerBotShopOffer& offer);

	void ClosePlayerBotShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			const char* reason)
	{
		if (!ch)
			return;
		const bool bHadShop = ch->GetMyShop() != NULL;
		if (bHadShop)
			ch->CloseMyShop();
		// Did this stand sell anything? The lifetime pass writes bSoldLogged
		// once per line that left the bag.
		bool bSoldSomething = false;
		for (size_t i = 0; i < state.vecShopOffers.size(); ++i)
		{
			const TPlayerBotShopOffer& offer = state.vecShopOffers[i];
			if (offer.bSoldLogged || !FindPlayerBotOfferItem(ch, offer))
			{
				bSoldSomething = bSoldSomething || offer.bSoldLogged;
				state.mapStallUnsold.erase(offer.dwItemID);
				continue;
			}
			BYTE& unsold = state.mapStallUnsold[offer.dwItemID];
			if (unsold < 255)
				++unsold;
		}
		// The map outlives the goods: a line vendored or burnt leaves its id
		// behind, so it is pruned against the bag now and then.
		if (state.mapStallUnsold.size() > 64)
		{
			for (std::map<DWORD, BYTE>::iterator it = state.mapStallUnsold.begin();
					it != state.mapStallUnsold.end(); )
			{
				LPITEM held = ITEM_MANAGER::instance().Find(it->first);
				if (!held || held->GetOwner() != ch)
					state.mapStallUnsold.erase(it++);
				else
					++it;
			}
		}
		state.dwShopOpenedTime = 0;
		state.dwShopCloseTime = 0;
		state.vecShopOffers.clear();
		// A stand that merely ran out of time is followed by another one on
		// the same pitch - the keeper is standing there, the goods are in the
		// bag, and the open pass takes "already at the pitch" - up to
		// PLAYERBOT_SHOP_STANDS_IN_ROW of them, and not after two dry stands in
		// a row. Anything else (sold out, walked off, refused) rests.
		const bool bExpired = reason && strcmp(reason, "expired") == 0;
		const bool bBarren = !bSoldSomething && state.bShopStandsInRow > 0 &&
				!state.bShopLastStandSold;
		const bool bAgain = bExpired && !bBarren &&
				state.bShopStandsInRow + 1 < PLAYERBOT_SHOP_STANDS_IN_ROW &&
				ShouldPlayerBotKeepShop(ch, state);
		if (bAgain)
		{
			++state.bShopStandsInRow;
			state.bShopLastStandSold = bSoldSomething;
			state.dwNextShopKeepTime = dwNow + PLAYERBOT_SHOP_REOPEN_MS;
			sys_log(0, "PLAYERBOT_SHOP: another stand pid=%u name=%s stand=%d/%d sold=%d",
					ch->GetPlayerID(), ch->GetName(), (int)state.bShopStandsInRow + 1,
					PLAYERBOT_SHOP_STANDS_IN_ROW, bSoldSomething ? 1 : 0);
		}
		else
		{
			state.bShopStandsInRow = 0;
			state.bShopLastStandSold = false;
			state.dwNextShopKeepTime = dwNow +
					number(PLAYERBOT_SHOP_REST_MIN, PLAYERBOT_SHOP_REST_MAX);
		}
		if (bHadShop)
		{
			// CloseMyShop takes the sign back from whoever is in view at this
			// instant. Somebody arriving a moment later is not, which is how a bot
			// comes to be seen running about wearing a stall nobody can open.
			state.dwShopSignClearUntil = dwNow + PLAYERBOT_SHOP_SIGN_CLEAR_WINDOW;
			state.dwNextShopSignClearTime = dwNow;
			sys_log(0, "PLAYERBOT_SHOP: closed pid=%u name=%s reason=%s",
					ch->GetPlayerID(), ch->GetName(), reason);
			// The packs and singles were for the counter; pour them back.
			state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_AFTER_SHOP_MS;
		}
	}

	// The first cell of the engine's shop grid where a line of this height fits,
	// searched the way CGrid::FindBlank does - row by row, left to right - or
	// -1 when the counter is full. See TPlayerBotShopOffer::bSlot for why the
	// line's index in the table is not its slot.
	// Split lines off each stack that is going on the counter, into free
	// cells: singles for what is bought one at a time, packs of
	// PLAYERBOT_SHOP_PACK_UNITS for a material (GetPlayerBotStallLineUnits).
	// Returns true when the bag changed and the scan has to run again.
	bool SplitPlayerBotStallSingles(LPCHARACTER ch,
			const std::vector<std::pair<int, WORD> >& scored, DWORD dwNow)
	{
		bool changed = false;
		for (size_t i = 0; i < scored.size(); ++i)
		{
			LPITEM item = ch->GetInventoryItem(scored[i].second);
			const int units = item ? GetPlayerBotStallLineUnits(item) : 0;
			if (units <= 0 || (int)item->GetCount() <= units || item->isLocked())
				continue;
			const int wantLines = units == 1 ? PLAYERBOT_SHOP_SINGLE_UNITS : PLAYERBOT_SHOP_PACK_LINES;
			int lines = 0;
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (other && other != item && (int)other->GetCount() == units &&
						PlayerBotStacksTogether(item, other))
					++lines;
			}
			int split = 0;
			while (lines < wantLines && (int)item->GetCount() > units &&
					CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS)
			{
				const int to = ch->GetEmptyInventory(item->GetSize());
				if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, item->GetCell()),
						TItemPos(INVENTORY, (WORD)to), (BYTE)units))
					break;
				++lines;
				++split;
			}
			if (split > 0)
			{
				changed = true;
				sys_log(0, "PLAYERBOT_SHOP: split for the counter pid=%u name=%s vnum=%u units=%d lines=%d left=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), units, lines,
						(unsigned int)item->GetCount());
			}
		}
		return changed;
	}

	int FindPlayerBotShopSlot(const bool* grid, int height)
	{
		for (int row = 0; row + height <= PLAYERBOT_SHOP_GRID_ROWS; ++row)
			for (int col = 0; col < PLAYERBOT_SHOP_GRID_COLUMNS; ++col)
			{
				bool empty = true;
				for (int h = 0; h < height && empty; ++h)
					empty = !grid[(row + h) * PLAYERBOT_SHOP_GRID_COLUMNS + col];
				if (empty)
					return row * PLAYERBOT_SHOP_GRID_COLUMNS + col;
			}
		return -1;
	}

	void PutPlayerBotShopSlot(bool* grid, int slot, int height)
	{
		for (int h = 0; h < height; ++h)
			grid[slot + h * PLAYERBOT_SHOP_GRID_COLUMNS] = true;
	}

	// The item behind a counter line, while it is still the keeper's to sell.
	// The engine's own test, made before the walk instead of after it: the item
	// by id, and its owner the keeper. Sold, and the id belongs to the buyer;
	// dropped or vendored, and it belongs to nobody.
	LPITEM FindPlayerBotOfferItem(LPCHARACTER keeper, const TPlayerBotShopOffer& offer)
	{
		if (!keeper || offer.dwItemID == 0)
			return NULL;
		LPITEM item = ITEM_MANAGER::instance().Find(offer.dwItemID);
		if (!item || item->GetOwner() != keeper)
			return NULL;
		return item;
	}

	// Runs at the very top of the tick, ahead of the inactivity watchdog and the
	// navigation rescues. Those both "continue", and a keeper that never reached
	// the shop hook could not close its stall: the sign stayed over its head and
	// a rescue was free to teleport it out of the market still wearing it. The
	// stall now outranks them - releasing engine state is not something a bot may
	// be interrupted out of.
	bool ManagePlayerBotShopLifetime(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow)
	{
		if (!ch || !ch->GetMyShop())
		{
			// The engine closes a stall the moment its last item is sold, so a
			// recorded offer can outlive the shop it described. Nobody reads it
			// without checking GetMyShop() first, but leaving it set would make
			// the state say something untrue.
			if (ch && !state.vecShopOffers.empty())
			{
				// The shop went away without this manager closing it - the engine
				// drops one on stun, on death and when the character is destroyed.
				// Whatever did it, the sign may still be over the bot's head.
				state.vecShopOffers.clear();
				state.dwShopSignClearUntil = dwNow + PLAYERBOT_SHOP_SIGN_CLEAR_WINDOW;
				state.dwNextShopSignClearTime = dwNow;
			}
			// And keep taking it back for a few seconds, because one broadcast only
			// reaches the clients that happened to be watching when it went out.
			if (state.dwShopSignClearUntil != 0)
			{
				if (dwNow >= state.dwShopSignClearUntil)
				{
					state.dwShopSignClearUntil = 0;
					state.dwNextShopSignClearTime = 0;
				}
				else if (dwNow >= state.dwNextShopSignClearTime)
				{
					state.dwNextShopSignClearTime =
							dwNow + PLAYERBOT_SHOP_SIGN_CLEAR_INTERVAL;
					ClearPlayerBotShopSign(ch);
				}
			}
			return false;
		}

		// A stall with no deadline can never expire. The shop lives in the engine
		// and the deadline in the AI state, so any path that loses one without the
		// other used to strand the keeper trading forever; give it one instead.
		if (state.dwShopCloseTime == 0)
			state.dwShopCloseTime =
					(state.dwShopOpenedTime != 0 ? state.dwShopOpenedTime : dwNow) +
					PLAYERBOT_SHOP_MIN_DURATION;

		// A counter with nothing left on it is just a bot standing still. Private
		// shop items stay in the owner's inventory, so what is still for sale is
		// simply what is still there. One pass over the bag, returning the moment
		// anything matches - which is the ordinary case.
		bool bSoldOut = !state.vecShopOffers.empty() && ch->IsItemLoaded();
		if (bSoldOut)
		{
			// Line by line, by item id - see TPlayerBotShopOffer::dwItemID. By
			// vnum a counter whose every line had sold stayed open as long as
			// the bag held a second stack of any of them. A line that is gone
			// is written to log.log once, as the keeper's sale: the panel's
			// equipment history reads it there beside the engine's own rows.
			for (size_t i = 0; i < state.vecShopOffers.size(); ++i)
			{
				TPlayerBotShopOffer& offer = state.vecShopOffers[i];
				if (FindPlayerBotOfferItem(ch, offer))
				{
					bSoldOut = false;
					continue;
				}
				if (!offer.bSoldLogged)
				{
					offer.bSoldLogged = true;
					char szHint[64];
					snprintf(szHint, sizeof(szHint), "%u x%u za %u", offer.dwVnum,
							(unsigned int)offer.wCount, offer.dwPrice);
					LogManager::instance().ItemLog(ch, (int)offer.dwItemID, (int)offer.dwVnum,
							"PLAYERBOT_STALL_SOLD", szHint);
				}
			}
		}

		// Whatever else happens, a corpse or a bot that is no longer standing on
		// the market strip has no business still holding a stall.
		long pitchX = 0, pitchY = 0;
		const bool onShopMap = GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY);
		const bool bOffPitch = ch->IsDead() || !onShopMap ||
				DISTANCE_APPROX(ch->GetX() - pitchX, ch->GetY() - pitchY) >
					PLAYERBOT_SHOP_RING_RADIUS + PLAYERBOT_MARKET_ARRIVE * 2;

		if (bSoldOut)
		{
			// Sold out is a reason to go back to playing, not to stand at an empty
			// counter until the clock runs out.
			ClosePlayerBotShop(ch, state, dwNow, "sold_out");
			return false;
		}

		if (dwNow < state.dwShopCloseTime && !bOffPitch)
		{
			// Standing at a stall is the activity, not the absence of one. Without
			// this the 90-second watchdog fired on every keeper, once per stall.
			state.dwLastMeaningfulActivityTime = dwNow;
			state.lLastX = ch->GetX();
			state.lLastY = ch->GetY();
			SetPlayerBotAction(state, BOT_ACTION_STALL, dwNow);
			return true;
		}

		ClosePlayerBotShop(ch, state, dwNow, bOffPitch ? "off_pitch" : "expired");
		return false;
	}

	bool ManagePlayerBotPrivateShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		// The stall's own lifetime is settled earlier in the tick; by the time
		// this runs a keeper either has no shop or has already been held there.
		if (ch->GetMyShop())
			return true;

		if (!ShouldPlayerBotKeepShop(ch, state))
			return false;
		// No map test here. There used to be one pinning stalls to Bokjung, left
		// over from when that was the only market, and it sat in front of the
		// choice below - so a bot in Joan returned before it ever got to roll, and
		// every stall in the world was still opening in Bokjung. Which towns are
		// allowed is decided by the roll and by GetPlayerBotShopCentre.

		// Errands still come first - a stall opened mid-visit would be abandoned
		// on the next tick.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable)
			return false;
		if (state.dwNextShopKeepTime != 0 && dwNow < state.dwNextShopKeepTime)
			return false;
		// ...but "in town with nothing to do" is a state that barely exists: a bot
		// comes to Bokjung *because* it has an errand, and leaves the moment the
		// errand is done. The stall therefore opens right after a completed town
		// visit, while the bot is still standing in the village, instead of waiting
		// for an idle moment that never arrives.
		const bool justFinishedInTown = state.dwNextShopCheckTime != 0 &&
				dwNow < state.dwNextShopCheckTime;
		// A keeper trades in the town it is standing in.
		//
		// This used to roll a town - nine openings in ten choosing Joan - and
		// then refuse to open unless the bot already happened to be there. The
		// bots with anything to sell are in Bokjung, so nine rolls in ten were
		// thrown away and Joan got no stalls at all, which is the opposite of
		// what the roll was for. The market browse already reads the ring of
		// whatever map its own bot is on, so a stall in Joan has the four
		// hundred bots of map 21 for customers.
		long pitchX = 0, pitchY = 0;
		if (!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY))
			return false;
		// Bokjung's ring used to be capped, and a keeper that found it full
		// carried its goods to Joan. Both are gone: a town is meant to fill up,
		// and a counter refused is a bot with nothing to do (the operator's
		// call - "jak chca to niech chodza i zaludniaja miasto"). Joan gets its
		// stalls from the bots standing in Joan, which is where they came from
		// before the cap existed. s_iPlayerBotStallsInM2 is still counted for
		// the ledger's report.
		// A keeper already standing on the ring counts as in town too. A server
		// restart drops every shop - they live only in memory - and leaves its
		// keeper parked exactly where the stall was, with no errand to bring it
		// back to town and therefore no way to ever reopen.
		const bool alreadyAtPitch =
				DISTANCE_APPROX(ch->GetX() - pitchX, ch->GetY() - pitchY) <=
					PLAYERBOT_SHOP_RING_RADIUS + PLAYERBOT_MARKET_ARRIVE;
		if (!justFinishedInTown && !alreadyAtPitch)
			return false;

		// Sorted best first, so the head of the list is the best score there is.
		std::vector<std::pair<int, WORD> > scored;
		CollectPlayerBotShopItems(ch, scored, IsPlayerBotStallKeeper(state));
		// Single units of the goods a player buys singly, split off before the
		// lines are chosen. Idempotent - the scan runs again on every tick of
		// the walk to the pitch - and bounded by the cells the shop bundle and
		// the loot still need.
		if (SplitPlayerBotStallSingles(ch, scored, dwNow))
		{
			scored.clear();
			CollectPlayerBotShopItems(ch, scored, IsPlayerBotStallKeeper(state));
		}
		if (!IsPlayerBotStallWorthOpening(scored.size(),
				scored.empty() ? 0 : scored[0].first,
				IsPlayerBotPoorKeeper(ch) || IsPlayerBotBagFull(ch)))
		{
			// Nothing worth a stall right now; look again after a hunt rather than
			// re-scanning the whole inventory every tick. A bot that is merely a
			// line or two short is asked again sooner: it needs one more drop,
			// not an evening, and it can only open while it happens to be in town.
			state.dwNextShopKeepTime = dwNow + (scored.empty()
					? number(300000, 600000) : number(120000, 240000));
			sys_log(0, "PLAYERBOT_SHOP: nothing to sell pid=%u name=%s lines=%u best=%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)scored.size(),
					scored.empty() ? 0 : scored[0].first);
			return false;
		}

		// Distance and angle are both stable per bot, so a keeper returns to its
		// own pitch every time instead of the market rearranging itself.
		long offsetX = 0, offsetY = 0;
		GetPlayerBotStableOffset(ch->GetPlayerID(), 0x4d4b5450U,
				PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS,
				offsetX, offsetY);
		const long stallX = pitchX + offsetX;
		const long stallY = pitchY + offsetY;

		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (!MovePlayerBotTownLeg(ch, state, dwNow, stallX, stallY,
				PLAYERBOT_MARKET_ARRIVE))
			return true; // still walking to the pitch
		// Counted the moment it opens rather than at the next ledger sweep, or
		// eight keepers arriving in the same minute would all read six.
		if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2)
			++s_iPlayerBotStallsInM2;

		// OpenMyShop refuses a character whose main part is not its own body, so
		// the horse has to go before the stall can be set up.
		if (ch->IsRiding())
			ch->StopRiding();
		ch->HorseSummon(false);
		ch->SetVictim(NULL);
		ch->Stop();

		// One table entry per item, in the order they will sit on the counter.
		// OpenMyShop rejects the entire shop if any single line is unsellable, so
		// each item is re-checked here: the inventory may have moved between the
		// scan above and this point - a town errand happens in between.
		TShopItemTable table[PLAYERBOT_SHOP_MERCHANT_ITEMS];
		memset(table, 0, sizeof(table));
		// Decided once, here: the prices below and the sign both read it.
		const bool bPoor = IsPlayerBotPoorKeeper(ch);
		std::vector<TPlayerBotShopOffer> offers;
		const BYTE tableLimit = IsPlayerBotStallKeeper(state)
				? PLAYERBOT_SHOP_MERCHANT_ITEMS : PLAYERBOT_SHOP_MAX_ITEMS;
		BYTE tableCount = 0;
		int bestScore = 0;
		// What the sign will say. The counter is sorted best first, so the first
		// line is the headline; the rest decides the wording.
		const char* pszBestName = NULL;
		const char* pszWeapon30 = NULL;
		const char* pszPrecious = NULL;
		BYTE bPreciousRefine = 0;
		const char* apszMaterials[2] = { NULL, NULL };
		int iMaterials = 0;
		const char* pszBook = NULL;
		int iBooks = 0;
		int iScrap = 0;
		bool grid[PLAYERBOT_SHOP_GRID_CELLS];
		memset(grid, 0, sizeof(grid));
		for (size_t i = 0; i < scored.size() && tableCount < tableLimit; ++i)
		{
			const WORD cell = scored[i].second;
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const TItemTable* proto = item->GetProto();
			if (!proto || IS_SET(proto->dwAntiFlags,
					ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
				continue;
			// Placed on the engine's grid by height, or the engine drops the
			// line and every buyer who comes for it is refused at an empty slot.
			const int height = std::max<int>(1, std::min<int>(PLAYERBOT_SHOP_GRID_ROWS, item->GetSize()));
			const int slot = FindPlayerBotShopSlot(grid, height);
			if (slot < 0)
				continue;
			PutPlayerBotShopSlot(grid, slot, height);
			DWORD price = GetPlayerBotShopAskingPrice(item);
			// The clearance discount. Applied to the price as asked, so the sale
			// memory still learns the real price the market would have paid.
			if (bPoor)
				price = std::max<DWORD>(1, price * PLAYERBOT_SHOP_POOR_DISCOUNT_PERCENT / 100);
			// Carried home unsold before: cheaper by the stand, after the price
			// is asked so the sale memory learns the market and not the markdown.
			{
				std::map<DWORD, BYTE>::const_iterator unsold = state.mapStallUnsold.find(item->GetID());
				if (unsold != state.mapStallUnsold.end() && unsold->second > 0)
				{
					const int stands = std::min<int>(unsold->second, PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_STANDS);
					price = std::max<DWORD>(1, price * (100 - stands * PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT) / 100);
				}
			}
			table[tableCount].vnum = item->GetVnum();
			table[tableCount].count = item->GetCount();
			table[tableCount].pos = TItemPos(INVENTORY, cell);
			table[tableCount].price = price;
			table[tableCount].display_pos = (BYTE)slot;

			TPlayerBotShopOffer offer;
			offer.dwVnum = item->GetVnum();
			offer.dwPrice = price;
			offer.bRefine = item->GetRefineLevel();
			offer.wCount = item->GetCount();
			offer.dwItemID = item->GetID();
			offer.bSoldLogged = false;
			offer.bSlot = (BYTE)slot;
			offers.push_back(offer);
			if (scored[i].first > bestScore)
				bestScore = scored[i].first;
			++tableCount;

			const char* pszName = proto->szLocaleName;
			if (!pszBestName)
				pszBestName = pszName;
			if (IsPlayerBotSpecialLevel30Weapon(item))
				pszWeapon30 = pszWeapon30 ? pszWeapon30 : pszName;
			else if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			{
				if (!pszPrecious || item->GetRefineLevel() > bPreciousRefine)
				{
					pszPrecious = pszName;
					bPreciousRefine = item->GetRefineLevel();
				}
			}
			else if (IsPlayerBotTradeableMaterial(item))
			{
				if (iMaterials < 2)
					apszMaterials[iMaterials] = pszName;
				++iMaterials;
			}
			else if (item->GetType() == ITEM_SKILLBOOK)
			{
				pszBook = pszBook ? pszBook : pszName;
				++iBooks;
			}
			else if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
					item->GetRefineLevel() < PLAYERBOT_SHOP_MIN_GEAR_REFINE)
				++iScrap;
		}
		// Asked again here rather than trusting the scan above: the inventory
		// moves between the two - a town errand happens in between - and a stall
		// that loses two of its three lines on the way to the pitch should stay
		// packed up rather than open with what is left.
		if (!IsPlayerBotStallWorthOpening(tableCount, bestScore, bPoor || IsPlayerBotBagFull(ch)))
		{
			state.dwNextShopKeepTime = dwNow + (tableCount == 0
					? number(300000, 600000) : number(120000, 240000));
			return false;
		}

		// The sign says what is on the counter - a market of forty stalls all
		// signed with account names read as a wall of nothing, and a themed
		// phrase drawn at random read as the same wall in fancy dress. The
		// headline is the best line; the wording says what kind of counter it
		// is; a short prefix by pid keeps neighbours from matching word for word.
		char sign[SHOP_SIGN_MAX_LEN + 1];
		{
			// Named for what is on the counter, in words a player reads at a
			// glance - "przejrzyste nazwy sklepow", the Discord's request. A
			// counter that is mostly books is a bookshop and says so; mostly
			// materials, a smith's supplier; a mixed one takes a market cry
			// drawn by pid ("zobacz kotku co mam w srodku, aka 2009 gameplay").
			// The item-name signs stay for the goods people cross a market for:
			// a level-30 weapon, a big refine. A poor keeper's counter is a
			// clearance sale and the sign says that first.
			const DWORD draw = PlayerBotNavHash(ch->GetPlayerID() ^ 0x5349474eU);
			static const char* const s_apszPrefixes[] = { "", "Tanio: ", "Okazja: ", "Sprzedam " };
			static const char* const s_apszBookShops[] = {
				"Ksiegi umiejetnosci", "KU dla kazdej klasy", "Biblioteka - ksiegi", "Ksiegi: %s i inne" };
			static const char* const s_apszMaterialShops[] = {
				"Ulepki z %s", "Materialy do kowala", "Skory, zeby i kly", "Ulepszacze, tanio" };
			static const char* const s_apszMarketCries[] = {
				"Zobacz kotku co mam w srodku", "Zaczynam gre, kup cos", "%s - najnizsze ceny",
				"Wszystko za grosze", "Tanio jak barszcz", "Rozne rozczne, zapraszam",
				"Czego szukasz, to mam", "Sprzedam, bez targow" };
			const char* pszPrefix = bPoor ? "Wyprzedaz: "
					: s_apszPrefixes[(ch->GetPlayerID() * 2654435761U >> 8) % 4U];
			char body[SHOP_SIGN_MAX_LEN * 2 + 1];
			const char* pszTemplate = NULL;
			const char* pszArg = "";
			if (pszWeapon30)
				snprintf(body, sizeof(body), "Bron 30: %s", pszWeapon30);
			else if (pszPrecious)
				snprintf(body, sizeof(body), "%s", pszPrecious); // the name carries its +N
			else if (iBooks > 0 && iBooks * 2 >= (int)tableCount)
			{
				pszTemplate = s_apszBookShops[draw % 4U];
				pszArg = pszBook ? pszBook : "";
			}
			else if (iMaterials > 0 && iMaterials * 2 >= (int)tableCount)
			{
				pszTemplate = s_apszMaterialShops[draw % 4U];
				pszArg = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 ? "M1" : "M2";
			}
			else if (iScrap > 0 && iScrap >= (int)tableCount / 2)
				snprintf(body, sizeof(body), "Zlom do palenia +0..+3");
			else if (pszBestName && tableCount > 1 && (draw & 8U) != 0)
				snprintf(body, sizeof(body), "%s i inne", pszBestName);
			else if (tableCount > 1)
			{
				pszTemplate = s_apszMarketCries[draw % 8U];
				pszArg = ch->GetName();
			}
			else
				snprintf(body, sizeof(body), "%s", pszBestName ? pszBestName : ch->GetName());
			if (pszTemplate)
			{
				if (strstr(pszTemplate, "%s"))
					snprintf(body, sizeof(body), pszTemplate, pszArg);
				else
					snprintf(body, sizeof(body), "%s", pszTemplate);
			}
			// The prefix goes only where the whole line still fits: the goods are
			// the point, the flourish is not.
			if (strlen(pszPrefix) + strlen(body) <= SHOP_SIGN_MAX_LEN)
				snprintf(sign, sizeof(sign), "%s%s", pszPrefix, body);
			else
				snprintf(sign, sizeof(sign), "%s", body);
		}

		// Opening a stall costs a shop bundle, exactly as it does for a player:
		// OpenMyShop consumes one 50200 and refuses outright without it. The other
		// accepted item, the permanent 71049, takes a branch that writes through
		// GetDesc() - a bot has no client descriptor, so that path must be avoided.
		if (ch->CountSpecifyItem(71049) > 0)
			return false;
		if (ch->CountSpecifyItem(50200) == 0)
		{
			// AutoGiveItem puts the bundle on the ground when the bag has no free
			// cell - and a bag about to be sold from is often exactly that full.
			// The shop then refused for want of a bundle, the next attempt bought
			// another, and the market square filled with "botjade2's Tobol" lying
			// in the drop-protection window while the bots ran about between them.
			if (ch->GetEmptyInventory(1) < 0)
			{
				// First pour the split singles back together: the stall scan cut
				// shellfish and bait into one-unit lines for a counter that then
				// had no cell for its own bundle, and a bag of ninety carried
				// twelve shells in twelve cells for good. A merge that frees a
				// cell is retried at once.
				const int merged = MergePlayerBotStacks(ch, PLAYERBOT_STACK_MERGES_PER_PASS * 2);
				if (merged > 0 && ch->GetEmptyInventory(1) >= 0)
				{
					sys_log(0, "PLAYERBOT_SHOP: merged %d stacks to make room for the bundle pid=%u name=%s",
							merged, ch->GetPlayerID(), ch->GetName());
					state.dwNextShopKeepTime = dwNow + 2000;
					return false;
				}
				// And back off. Returning without a clock left the keeper asking
				// again on the very next tick: one log line a second per bot, and
				// the bot itself pacing its pitch with a stall it could never
				// open - reported from the Discord with eight identical lines a
				// second for one pid. A full bag is relieved by the merchant leg
				// of an ordinary town visit, which cannot have the tick while
				// this pass keeps claiming it.
				state.dwNextShopKeepTime = dwNow + number(60000, 180000);
				PlayerBotLogThrottled("shop_no_bundle_room", dwNow,
						"PLAYERBOT_SHOP: no room for bundle pid=%u name=%s lines=%u",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)tableCount);
				return false;
			}
			// The bot buys its stall like anything else it carries.
			if (ch->GetGold() >= PLAYERBOT_SHOP_BUNDLE_PRICE)
				ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_SHOP_BUNDLE_PRICE);
			ch->AutoGiveItem(50200, 1);
		}

		ch->OpenMyShop(sign, table, tableCount);
		if (!ch->GetMyShop())
		{
			// OpenMyShop refuses silently, and it refuses the whole shop over one
			// bad line - so report the first item as the representative sample
			// along with how many were offered.
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			LPITEM first = ch->GetItem(table[0].pos);
			const TItemTable* rp = first ? first->GetProto() : NULL;
			sys_log(0, "PLAYERBOT_SHOP: refused pid=%u items=%u poly=%d quest=%d vnum=%u anti=%u equipped=%d locked=%d viaPos=%d sign=%d gold=%d",
					ch->GetPlayerID(), (unsigned int)tableCount,
					ch->IsPolymorphed() ? 1 : 0,
					(pc && pc->IsRunning()) ? 1 : 0, table[0].vnum,
					rp ? rp->dwAntiFlags : 0, first && first->IsEquipped() ? 1 : 0,
					first && first->isLocked() ? 1 : 0, first ? 1 : 0,
					(int)strlen(sign), (int)(ch->GetGold() / 1000));
			// The sign is already on every client in view - OpenMyShop sends it
			// before it creates the shop, and nothing takes it back when the
			// creation fails. Left alone, this bot walks off wearing a stall
			// nobody can open.
			ClearPlayerBotShopSign(ch);
			state.dwNextShopKeepTime = dwNow + number(60000, 180000);
			return false;
		}

		// A keeper is not hunting. The tick has held it at its counter since the
		// stall opened - the shop hook runs before skills and claims the tick -
		// so nothing casts while a shop is up. What is seen glowing over a shaman
		// minding a stall is an aura from before it sat down, running its minutes
		// out. Drop them: it reads as a bot playing the game while it keeps shop,
		// and it is upkeep spent on standing still.
		ch->RemoveGoodAffect();

		state.dwShopOpenedTime = dwNow;
		state.dwShopCloseTime = dwNow + (IsPlayerBotMerchant(state)
				? number(PLAYERBOT_SHOP_MERCHANT_MIN_DURATION,
						PLAYERBOT_SHOP_MERCHANT_MAX_DURATION)
				: number(PLAYERBOT_SHOP_MIN_DURATION, PLAYERBOT_SHOP_MAX_DURATION));
		// The shop's own item list is private to CShop, so what is on the counter
		// is recorded here instead - in the same order, because that order is what
		// CShopManager::Buy indexes by. A bot browsing the market reads this
		// rather than the engine's structure.
		state.vecShopOffers = offers;
		// On the ledger now rather than at its next refresh - see
		// AddPlayerBotMarketSupply for the three keepers this is about.
		for (size_t i = 0; i < offers.size(); ++i)
			AddPlayerBotMarketSupply(offers[i].dwVnum, offers[i].wCount);
		sys_log(0, "PLAYERBOT_SHOP: opened pid=%u name=%s items=%u first_vnum=%u first_price=%u pos=(%ld,%ld) sign=\"%s\"",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)tableCount,
				offers[0].dwVnum, offers[0].dwPrice, ch->GetX(), ch->GetY(), sign);
		// Worth crossing town for is worth a line on the world channel - the
		// same bar the sign uses for a stall that carries one thing.
		if (bestScore >= PLAYERBOT_SHOP_PRIZE_SCORE && pszBestName)
			AnnouncePlayerBotStall(ch, pszBestName);
		return true;
	}

	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance)
	{
		if (DISTANCE_APPROX(ch->GetX() - goalX, ch->GetY() - goalY) <= arrivalDistance)
		{
			// No dismount: the engine serves a rider at every counter - the
			// shop, the blacksmith, the storekeeper, a quest NPC - and refuses
			// one only a skill book, a costume and a second mount. Climbing
			// down at each NPC and up again after was the visible half of a
			// town visit, and what a player does not do.
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			ClearPlayerBotRoute(state, true);
			return true;
		}

		// NPCs and gateposts occupy ATTR_OBJECT cells.  A strict four-cell snap can
		// therefore reject a perfectly valid visit when the chosen waiting spot is
		// on the other side of a counter, pillar or another dynamic object.  Town
		// legs may finish at the nearest point of the bot's own walkable component;
		// the larger arrival radii below represent the normal interaction area.
		//
		// But the snap has to stay inside the radius this leg then tests, or
		// arriving is not arriving. At a fixed sixteen cells the planner could
		// place the goal eight hundred units from where it was asked for - past
		// the 350 to 850 the phases accept - and the bot would walk its route,
		// reach the snapped cell, find itself still outside the radius, clear the
		// route and plan the identical one again. Nothing counts that as a
		// failure: consuming a waypoint resets the stuck counter, so the service
		// rescue below never fired. One bot stood at the skill trainer in Joan
		// for three days that way, reset by the inactivity watchdog every ninety
		// seconds without ever taking a step. A ring of n cells reaches
		// n * 50 * sqrt(2) at the corners, so a tenth of the arrival distance in
		// cells keeps the snapped goal comfortably inside it.
		const int snapCells = std::max(2, std::min(16, arrivalDistance / 100));
		// A goal on ground the bot's terrain does not join is not walked to at
		// all: the planner answers "unreachable" three times, the service
		// rescue relocates the bot on the sixth, and the misc merchant of
		// Bokjung - whose approach point sits on a strip cut off from the
		// square - cost 260 such rescues in a morning, six failed plans each.
		// The nearest cell of the bot's own component inside the arrival
		// radius is where the rescue would have put it; ask for it first.
		long walkX = goalX, walkY = goalY;
		{
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
			if (navigation.Init(ch->GetMapIndex()) &&
					!navigation.CanReach(ch->GetX(), ch->GetY(), goalX, goalY))
			{
				const DWORD own = navigation.GetComponentAtWorld(ch->GetX(), ch->GetY());
				const int radius = std::max(2, arrivalDistance / 50 - 2);
				long bestDistance = -1;
				for (int dy = -radius; dy <= radius && own != 0; ++dy)
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
				if (bestDistance >= 0)
					PlayerBotLogThrottled("town_goal_reachable", dwNow,
							"PLAYERBOT_TOWN: goal moved onto reachable ground pid=%u name=%s phase=%u goal=(%ld,%ld) walk=(%ld,%ld)",
							ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
							goalX, goalY, walkX, walkY);
			}
		}
		const bool moveAccepted = MovePlayerBot(ch, walkX, walkY, dwNow,
				snapCells, true, true, false, true);
		if (!moveAccepted && state.bStuckCounter >= 6)
		{
			sys_err("PLAYERBOT_TOWN: route failed pid=%u name=%s phase=%u from=(%ld,%ld) to=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
					ch->GetX(), ch->GetY(), goalX, goalY);

			// A handful of decorative town objects are disconnected in server_attr
			// even though the native client can run around them.  Retrying the same
			// component forever left a bot permanently unable to sell.  After six
			// independently planned failures, relocate once to the nearest verified
			// walkable service cell and let the normal arrival/wait phase continue.
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
			PIXEL_POSITION safe;
			if (navigation.Init(ch->GetMapIndex()) &&
					navigation.FindNearestWalkableWorld(goalX, goalY, 16, safe,
							ch->GetPlayerID() ^ (DWORD)state.bTownVisitPhase))
			{
				ClearPlayerBotRoute(state, true);
				state.bStuckCounter = 0;
				ch->Show(ch->GetMapIndex(), safe.x, safe.y, 0);
				ch->Stop();
				ch->SendMovePacket(FUNC_MOVE, 0, safe.x, safe.y, 0, dwNow);
				sys_err("PLAYERBOT_TOWN: service rescue pid=%u name=%s phase=%u to=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
						safe.x, safe.y);
			}
			else
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
		}
		return false;
	}

	bool MovePlayerBotAcrossTownGate(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, long goalY)
	{
		if (!ch)
			return false;
		const int distance = DISTANCE_APPROX(
				ch->GetX() - PLAYERBOT_TOWN_GATE_X, ch->GetY() - goalY);
		if (distance <= 450)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			ClearPlayerBotRoute(state, true);
			return true;
		}

		// server_attr separates the two sides of Joan's decorative gate into
		// different components even though players can run through its opening.
		// This one verified 5.75 m segment is therefore issued directly, while all
		// ordinary navigation remains collision-aware. It replaces endless A*
		// retries at (603,675) with the same straight run a real player performs.
		if (distance <= 1200)
		{
			ClearPlayerBotRoute(state, true);
			ch->SetRotationToXY(PLAYERBOT_TOWN_GATE_X, goalY);
			if (ch->Goto(PLAYERBOT_TOWN_GATE_X, goalY))
			{
				ch->SendMovePacket(FUNC_MOVE, 0, PLAYERBOT_TOWN_GATE_X, goalY,
						ch->GetCurrentMoveDuration(), dwNow);
				return false;
			}
		}

		return MovePlayerBotTownLeg(ch, state, dwNow,
				PLAYERBOT_TOWN_GATE_X, goalY, 450);
	}

	bool HandlePlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !state.bVisitingShop ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return false;
		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;
		const long weaponNpcX = inM2 ? PLAYERBOT_M2_WEAPON_MERCHANT_X : PLAYERBOT_WEAPON_MERCHANT_X;
		const long weaponNpcY = inM2 ? PLAYERBOT_M2_WEAPON_MERCHANT_Y : PLAYERBOT_WEAPON_MERCHANT_Y;
		const long armorNpcX = inM2 ? PLAYERBOT_M2_ARMOR_MERCHANT_X : PLAYERBOT_ARMOR_MERCHANT_X;
		const long armorNpcY = inM2 ? PLAYERBOT_M2_ARMOR_MERCHANT_Y : PLAYERBOT_ARMOR_MERCHANT_Y;
		const long miscNpcX = inM2 ? PLAYERBOT_M2_MISC_MERCHANT_X : PLAYERBOT_MISC_MERCHANT_X;
		const long miscNpcY = inM2 ? PLAYERBOT_M2_MISC_MERCHANT_Y : PLAYERBOT_MISC_MERCHANT_Y;
		const long blacksmithNpcX = inM2 ? PLAYERBOT_M2_BLACKSMITH_X : PLAYERBOT_BLACKSMITH_X;
		const long blacksmithNpcY = inM2 ? PLAYERBOT_M2_BLACKSMITH_Y : PLAYERBOT_BLACKSMITH_Y;

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_REFINE, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_SAFEBOX_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		else
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
		{
			state.bTownVisitPhase = inM2
					? GetPlayerBotFirstDirectTownPhase(state)
					: GetPlayerBotFirstExteriorTownPhase(state);
			if (!inM2 && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			if (inM2 && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, true);
				return true;
			}
		}

		// Eight profession trainers stand south of Joan.  Their npc.txt cells are
		// 623/627 (Warrior), 631/635 (Ninja), 645/649 (Sura), 653/657
		// (Shaman); the second coordinate includes map 21's 102400 Y base.
		const BYTE wantedGroup = (ch->GetPlayerID() % 2 == 0) ? 1 : 2;
		const long trainerGroup1X[4] = { 62300, 63100, 64500, 65300 };
		const long trainerGroup2X[4] = { 62700, 63500, 64900, 65700 };
		const BYTE trainerJob = std::min<BYTE>(ch->GetJob(), JOB_SHAMAN);
		const long trainerNpcX = wantedGroup == 1
				? trainerGroup1X[trainerJob] : trainerGroup2X[trainerJob];
		const long trainerNpcY = ch->GetJob() <= JOB_WARRIOR ? 161800 : 161900;
		long trainerX = 0, trainerY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), trainerNpcX, trainerNpcY,
				0x54524149U, trainerX, trainerY);

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			long resetX = 0, resetY = 0;
			GetPlayerBotNpcApproach(ch->GetPlayerID(), PLAYERBOT_SKILL_RESET_NPC_X,
					PLAYERBOT_SKILL_RESET_NPC_Y, 0x52534554U, resetX, resetY);
			if (MovePlayerBotTownLeg(ch, state, dwNow, resetX, resetY, 550))
			{
				// Asked again on arrival rather than trusted from the walk: a
				// level gained on the way, a purse spent at a merchant, or a
				// Master that finally rolled all end the errand here instead of
				// paying for nothing.
				if (ShouldPlayerBotResetSkills(ch, state, dwNow) &&
						PayPlayerBotSkillReset(ch, state, dwNow))
					state.bTownNeedTrainer = true;
				state.bTownNeedSkillReset = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_TRAINER_WAIT_MIN, PLAYERBOT_TRAINER_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_SKILL_RESET_WAIT;
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET_WAIT)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE &&
						(state.bTownNeedMisc || state.bTownNeedBlacksmith))
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			if (MovePlayerBotTownLeg(ch, state, dwNow, trainerX, trainerY, 550))
			{
				if (ChoosePlayerBotSkillGroup(ch))
					state.bTownNeedTrainer = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_TRAINER_WAIT_MIN, PLAYERBOT_TRAINER_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_TRAINER_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: trainer visit pid=%u name=%s job=%u group=%u wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetJob(), ch->GetSkillGroup(),
						state.dwTownWaitUntil - dwNow, ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER_WAIT)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedWeaponMerchant
						? BOT_TOWN_PHASE_WEAPON_MERCHANT
						: (state.bTownNeedArmorMerchant ? BOT_TOWN_PHASE_ARMOR_MERCHANT
							: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
								? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE));
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long weaponMerchantX = 0, weaponMerchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), weaponNpcX,
				weaponNpcY, 0x57454150U, weaponMerchantX, weaponMerchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					weaponMerchantX, weaponMerchantY, 850))
			{
				ManagePlayerBotWeaponMerchant(ch);
				ManagePlayerBotEquipment(ch, state, dwNow);
				if (!ch->GetWear(WEAR_WEAPON))
				{
					// Nothing sellable was sufficient. Leave the counter after this
					// visit and search nearby hunting fields for ownerless Yang/gear.
					state.dwEmergencyScavengeUntil = dwNow + 120000;
					sys_log(0, "PLAYERBOT_GEAR: emergency scavenging armed pid=%u name=%s until=%u gold=%lld",
							ch->GetPlayerID(), ch->GetName(), state.dwEmergencyScavengeUntil,
							(long long)ch->GetGold());
				}
				state.bTownNeedBlacksmith = state.bTownNeedBlacksmith ||
						HasPlayerBotRefineOpportunity(ch);
				state.bTownNeedWeaponMerchant = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_WEAPON_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: weapon merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedArmorMerchant
						? BOT_TOWN_PHASE_ARMOR_MERCHANT
						: (inM2 ? GetPlayerBotFirstDirectTownPhase(state)
							: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
								? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE));
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long armorMerchantX = 0, armorMerchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), armorNpcX,
				armorNpcY, 0x41524d52U, armorMerchantX, armorMerchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					armorMerchantX, armorMerchantY, 850))
			{
				ManagePlayerBotArmorMerchant(ch);
				ManagePlayerBotEquipment(ch, state, dwNow);
				state.bTownNeedBlacksmith = state.bTownNeedBlacksmith ||
						HasPlayerBotRefineOpportunity(ch);
				state.bTownNeedArmorMerchant = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_ARMOR_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: armor merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = inM2
						? GetPlayerBotFirstDirectTownPhase(state)
						: (state.bTownNeedSafebox ? BOT_TOWN_PHASE_SAFEBOX
							: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
								? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE));
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		// The storekeeper. The safebox is opened the way the Dozorca's quest
		// opens it - SetSafeboxOpenPosition and a load request with the
		// default password - and comes back from the DB core a moment later;
		// the wait phase deposits into it when it does and closes it. An
		// answer that never comes leaves m_bOpeningSafebox set, which would
		// refuse the next request as "overlapped", so the wait cancels it.
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_SAFEBOX)
		{
			const long keeperX = inM2 ? PLAYERBOT_M2_STOREKEEPER_X : PLAYERBOT_STOREKEEPER_X;
			const long keeperY = inM2 ? PLAYERBOT_M2_STOREKEEPER_Y : PLAYERBOT_STOREKEEPER_Y;
			long goalX = 0, goalY = 0;
			GetPlayerBotNpcApproach(ch->GetPlayerID(), keeperX, keeperY, 0x53414645U, goalX, goalY);
			if (MovePlayerBotTownLeg(ch, state, dwNow, goalX, goalY, 650))
			{
				// The first visit pays for the page the way a player does at
				// the keeper's dialog: 500 yang, and the DB core opens a
				// safebox row of one page for the account.
				if (ch->GetQuestFlag(PLAYERBOT_SAFEBOX_PAID_FLAG) <= 0 && ch->GetDesc())
				{
					if (ch->GetGold() < PLAYERBOT_SAFEBOX_FEE)
					{
						sys_log(0, "PLAYERBOT_TOWN: safebox unaffordable pid=%u name=%s gold=%d",
								ch->GetPlayerID(), ch->GetName(), ch->GetGold());
						state.bTownNeedSafebox = false;
						state.bTownVisitPhase = inM2
								? GetPlayerBotFirstDirectTownPhase(state)
								: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
									? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE);
						ClearPlayerBotRoute(state, true);
						if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
							FinishPlayerBotTownVisit(ch, state, dwNow, true);
						return true;
					}
					ch->PointChange(POINT_GOLD, -PLAYERBOT_SAFEBOX_FEE);
					TSafeboxChangeSizePacket page;
					page.dwID = ch->GetDesc()->GetAccountTable().id;
					page.bSize = 1;
					db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_SIZE, ch->GetDesc()->GetHandle(),
							&page, sizeof(page));
					ch->SetSafeboxSize(SAFEBOX_PAGE_SIZE);
					ch->SetQuestFlag(PLAYERBOT_SAFEBOX_PAID_FLAG, 1);
					sys_log(0, "PLAYERBOT_TOWN: safebox paid pid=%u name=%s account=%u fee=%d gold_left=%d",
							ch->GetPlayerID(), ch->GetName(), page.dwID, PLAYERBOT_SAFEBOX_FEE, ch->GetGold());
				}
				ch->CancelSafeboxLoad();
				ch->SetSafeboxOpenPosition();
				ch->ReqSafeboxLoad(PLAYERBOT_SAFEBOX_PASSWORD);
				state.dwTownWaitUntil = dwNow + PLAYERBOT_SAFEBOX_LOAD_WAIT_MS;
				state.bTownVisitPhase = BOT_TOWN_PHASE_SAFEBOX_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: safebox requested pid=%u name=%s map=%ld pos=(%ld,%ld) books=%d free_cells=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
						CountPlayerBotSkillBooks(ch), CountPlayerBotFreeInventoryCells(ch));
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_SAFEBOX_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			bool done = false;
			CSafebox* box = ch->GetSafebox();
			if (box)
			{
				const int deposited = DepositPlayerBotSafeboxBooks(ch, box);
				ch->CloseSafebox();
				sys_log(0, "PLAYERBOT_TOWN: safebox deposit pid=%u name=%s deposited=%d books_left=%d free_cells=%d",
						ch->GetPlayerID(), ch->GetName(), deposited, CountPlayerBotSkillBooks(ch),
						CountPlayerBotFreeInventoryCells(ch));
				done = true;
			}
			else if (dwNow >= state.dwTownWaitUntil)
			{
				ch->CancelSafeboxLoad();
				sys_err("PLAYERBOT_TOWN: safebox never opened pid=%u name=%s map=%ld pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				done = true;
			}
			if (done)
			{
				state.bTownNeedSafebox = false;
				state.bTownVisitPhase = inM2
						? GetPlayerBotFirstDirectTownPhase(state)
						: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
							? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE);
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_IN)
		{
			if (inM2)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
				return true;
			}
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_X, PLAYERBOT_TOWN_GATE_OUTSIDE_Y, 1000))
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_CROSS_IN;
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_CROSS_IN)
		{
			if (MovePlayerBotAcrossTownGate(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_INSIDE_Y))
			{
				state.bTownVisitPhase = GetPlayerBotFirstInteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_OUT;
			}
			return true;
		}

		long merchantX = 0, merchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), miscNpcX,
				miscNpcY, 0x4d495343U, merchantX, merchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow, merchantX, merchantY, 650))
			{
				ManagePlayerBotMiscMerchant(ch);
				state.bTownNeedMisc = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_MISC_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: misc merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedBlacksmith
						? BOT_TOWN_PHASE_BLACKSMITH
						: (inM2 ? BOT_TOWN_PHASE_NONE : BOT_TOWN_PHASE_GATE_OUT);
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long blacksmithX = 0, blacksmithY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), blacksmithNpcX,
				blacksmithNpcY, 0x4b4f574cU, blacksmithX, blacksmithY);
		// Keep the per-PID spread, but halve it specifically at the blacksmith.
		// Together with the tighter arrival radius this keeps every refiner close
		// enough to look like it is actually interacting with the NPC.
		blacksmithX = blacksmithNpcX + (blacksmithX - blacksmithNpcX) / 2;
		blacksmithY = blacksmithNpcY + (blacksmithY - blacksmithNpcY) / 2;
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow, blacksmithX, blacksmithY, 500))
			{
				ManagePlayerBotRefining(ch, state, dwNow);
				// The blacksmith is where a player rerolls bonus lines too, and the
				// bot is already standing still there for six to twenty-four seconds.
				ManagePlayerBotBonusReroll(ch, state, dwNow);
				if (!HasPlayerBotRefineOpportunity(ch))
					RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
				state.bTownNeedBlacksmith = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_BLACKSMITH_WAIT_MIN, PLAYERBOT_BLACKSMITH_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_BLACKSMITH_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: blacksmith visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			// Use the time spent at the NPC like a real player: make further regular
			// refine attempts instead of clicking only once and idling.  This cadence
			// never extends dwTownWaitUntil; the visit has one absolute 6-24 s limit.
			ManagePlayerBotRefining(ch, state, dwNow);
			ManagePlayerBotBonusReroll(ch, state, dwNow);
			if (!HasPlayerBotRefineOpportunity(ch))
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
			if (dwNow >= state.dwTownWaitUntil)
			{
				// Always leave the NPC wearing the best surviving/refined equipment,
				// even if materials, Yang or a failed roll ended the session early.
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
				state.bTownVisitPhase = inM2
						? BOT_TOWN_PHASE_NONE : BOT_TOWN_PHASE_GATE_OUT;
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_OUT)
		{
			if (inM2)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
				return true;
			}
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_X, PLAYERBOT_TOWN_GATE_INSIDE_Y, 1000))
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_CROSS_OUT;
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_CROSS_OUT)
		{
			if (MovePlayerBotAcrossTownGate(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_OUTSIDE_Y))
			{
				state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		FinishPlayerBotTownVisit(ch, state, dwNow, false);
		return true;
	}
}

#endif
