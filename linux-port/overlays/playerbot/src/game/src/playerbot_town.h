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

	BYTE GetPlayerBotFirstExteriorTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedTrainer)
			return BOT_TOWN_PHASE_TRAINER;
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
		return BOT_TOWN_PHASE_NONE;
	}

	BYTE GetPlayerBotFirstDirectTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
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
		state.bTownNeedMisc = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_MISC) ||
				NeedsPlayerBotPotions(ch) || HasPlayerBotExcessPotions(ch) ||
				NeedsPlayerBotProgressionBoots(ch);
		state.bTownNeedWeaponMerchant = HasPlayerBotJunkForMerchant(
				ch, BOT_MERCHANT_WEAPON) || ch->GetWear(WEAR_WEAPON) == NULL ||
				NeedsPlayerBotProgressionWeapon(ch) || NeedsPlayerBotArrows(ch);
		state.bTownNeedArmorMerchant = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_ARMOR) ||
				NeedsPlayerBotProgressionArmor(ch) || NeedsPlayerBotProgressionShield(ch) ||
				NeedsPlayerBotProgressionHelmet(ch);
		state.bTownNeedBlacksmith = HasPlayerBotRefineOpportunity(ch);
		if (!state.bTownNeedTrainer && !state.bTownNeedMisc && !state.bTownNeedWeaponMerchant &&
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
		if (!ch || ch->GetMapIndex() != 21 || state.bVisitingShop)
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
		if (!EnsurePlayerBotBiologistMissionStarted(ch, missionIndex))
			return false;

		const int accepted = std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, (int)mission->requiredCount - accepted);
		const int carried = ch->CountSpecifyItem(mission->itemVnum);
		if (!state.bVisitingBiologist && carried < remaining)
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
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true) &&
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

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "biologist_interaction");
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextBiologistActionTime == 0)
		{
			state.dwNextBiologistActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextBiologistActionTime)
			return true;

		if (ch->CountSpecifyItem(mission->itemVnum) <= 0)
		{
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(5000, 12000);
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
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.dwTownWaitUntil = 0;
		state.dwNextShopCheckTime = dwNow +
			(completed ? number(300000, 600000) : number(60000, 120000));
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

	bool ShouldPlayerBotKeepShop(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_SHOP_MIN_LEVEL)
			return false;
		// A trader always has the stall open when it can. For everyone else it
		// stays what it was: an occasional thing one bot in ten does with a spare.
		if (IsPlayerBotMerchant(state))
			return true;
		// One bot in ten, stretched or shrunk by the TRADE weight. Drawn against a
		// thousand rather than ten so that the weight has somewhere to move: the
		// odds at the neutral 100 are the same one in ten as before, over a
		// different tenth of the population.
		return PlayerBotWeightedRoll(
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x53484f50U) % 1000U,
				100, PLAYERBOT_WEIGHT_TRADE);
	}

	// What a bot asks for what it puts up. A refined item has no price in the
	// tables - the merchant value is that of the unrefined base - so above +6
	// the number is ours. Deliberately modest: the point is that another bot can
	// actually buy it after an hour of hunting.
	DWORD GetPlayerBotShopAskingPrice(LPITEM item)
	{
		if (!item)
			return 1;
		const BYTE refine = item->GetRefineLevel();
		if (refine >= 9)
			return PLAYERBOT_SHOP_PRICE_PLUS9;
		if (refine == 8)
			return PLAYERBOT_SHOP_PRICE_PLUS8;
		if (refine == 7)
			return PLAYERBOT_SHOP_PRICE_PLUS7;
		const DWORD unit = GetPlayerBotNpcSellUnitPrice(item);
		const DWORD price = unit * (DWORD)item->GetCount() *
				PLAYERBOT_SHOP_MATERIAL_MARKUP;
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
		return ch && ch->GetHorseLevel() >= 10 &&
				ch->GetLevel() < GetPlayerBotNextHorseRequiredLevel(ch->GetHorseLevel());
	}

	int ScorePlayerBotShopStock(LPCHARACTER ch, LPITEM item, bool merchant)
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
			return 500;
		if (item->GetRefinedVnum() == 0 && item->GetType() == ITEM_MATERIAL)
			return 200;
		// Skill books. No bot reads one - that is still unwritten - so they are
		// pure stock, and a player will want them.
		if (item->GetType() == ITEM_SKILLBOOK)
			return 400;

		// Ordinary spare gear, and only if somebody could want it. This used to
		// be "return 1" for absolutely everything else, which is how counters
		// filled up with +1, +2 and +3 spares: a bot with eight of those and
		// nothing better put all eight out. There are nineteen hundred such
		// pieces in this world's bags against two hundred at +4 or better, so
		// that one line decided what the whole market looked like.
		const BYTE type = item->GetType();
		if (type == ITEM_WEAPON || type == ITEM_ARMOR)
		{
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

		// Whatever is left is the bot's own business, not goods. A stall with two
		// things worth buying beats one padded out to eight.
		return -1;
	}

	// Is this worth putting a sign up for? Three lines make a stall; fewer than
	// that only if one of them is the reason somebody would cross the market for
	// it - a level-30 weapon, a big bonus roll, anything at +6, a horse medal.
	// The caller passes the best score it has, because that is exactly what
	// ScorePlayerBotShopStock spent its time working out.
	bool IsPlayerBotStallWorthOpening(size_t lines, int bestScore)
	{
		if (lines == 0)
			return false;
		return lines >= PLAYERBOT_SHOP_MIN_ITEMS ||
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
			if (vnum >= 50701 && vnum <= 50706)
				continue;
			// Spare gear is the most interesting thing a stall can offer, but the
			// bot must never put up the only weapon or armour it owns for a slot
			// it is still walking around empty. Something already worn there means
			// what it carries is genuinely a spare.
			const BYTE type = item->GetType();
			if (type == ITEM_WEAPON || type == ITEM_ARMOR)
			{
				const int wearCell = item->FindEquipCell(ch);
				if (wearCell < 0 || ch->GetWear((BYTE)wearCell) == NULL)
					continue;
			}
			const int score = ScorePlayerBotShopStock(ch, item, merchant);
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
				outMap = mapIndex; outX = PLAYERBOT_MONKEY_EASY_ARRIVAL_X; outY = PLAYERBOT_MONKEY_EASY_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_ORC_VALLEY:
				outMap = mapIndex; outX = PLAYERBOT_ORC_VALLEY_ARRIVAL_X; outY = PLAYERBOT_ORC_VALLEY_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_DESERT:
				outMap = mapIndex; outX = PLAYERBOT_DESERT_ARRIVAL_X; outY = PLAYERBOT_DESERT_ARRIVAL_Y; break;
			default: break;
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

	void ClosePlayerBotShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			const char* reason)
	{
		if (!ch)
			return;
		const bool bHadShop = ch->GetMyShop() != NULL;
		if (bHadShop)
			ch->CloseMyShop();
		state.dwShopOpenedTime = 0;
		state.dwShopCloseTime = 0;
		state.vecShopOffers.clear();
		state.dwNextShopKeepTime = dwNow +
				number(PLAYERBOT_SHOP_REST_MIN, PLAYERBOT_SHOP_REST_MAX);
		if (bHadShop)
		{
			// CloseMyShop takes the sign back from whoever is in view at this
			// instant. Somebody arriving a moment later is not, which is how a bot
			// comes to be seen running about wearing a stall nobody can open.
			state.dwShopSignClearUntil = dwNow + PLAYERBOT_SHOP_SIGN_CLEAR_WINDOW;
			state.dwNextShopSignClearTime = dwNow;
			sys_log(0, "PLAYERBOT_SHOP: closed pid=%u name=%s reason=%s",
					ch->GetPlayerID(), ch->GetName(), reason);
		}
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
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM && bSoldOut; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item)
					continue;
				for (size_t i = 0; i < state.vecShopOffers.size(); ++i)
				{
					if (item->GetVnum() == state.vecShopOffers[i].dwVnum &&
							item->GetRefineLevel() == state.vecShopOffers[i].bRefine)
					{
						bSoldOut = false;
						break;
					}
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
		// Where to trade is rolled afresh for every stall, not fixed per bot: nine
		// openings in ten choose Joan. Nothing is remembered - a bot that rolls the
		// town it is not standing in simply does not open this time and rolls again
		// on its next attempt, so the choice can never strand a keeper waiting for
		// a town it rarely visits.
		long pitchX = 0, pitchY = 0;
		const long wantedMap = (number(1, 100) <= (int)PLAYERBOT_SHOP_M1_SHARE)
				? PLAYERBOT_MAP_CHUNJO_M1 : PLAYERBOT_MAP_CHUNJO_M2;
		if (ch->GetMapIndex() != wantedMap ||
				!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY))
			return false;
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
		CollectPlayerBotShopItems(ch, scored, IsPlayerBotMerchant(state));
		if (!IsPlayerBotStallWorthOpening(scored.size(),
				scored.empty() ? 0 : scored[0].first))
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
		std::vector<TPlayerBotShopOffer> offers;
		const BYTE tableLimit = IsPlayerBotMerchant(state)
				? PLAYERBOT_SHOP_MERCHANT_ITEMS : PLAYERBOT_SHOP_MAX_ITEMS;
		BYTE tableCount = 0;
		int bestScore = 0;
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
			const DWORD price = GetPlayerBotShopAskingPrice(item);
			table[tableCount].vnum = item->GetVnum();
			table[tableCount].count = item->GetCount();
			table[tableCount].pos = TItemPos(INVENTORY, cell);
			table[tableCount].price = price;
			table[tableCount].display_pos = tableCount;

			TPlayerBotShopOffer offer;
			offer.dwVnum = item->GetVnum();
			offer.dwPrice = price;
			offer.bRefine = item->GetRefineLevel();
			offers.push_back(offer);
			if (scored[i].first > bestScore)
				bestScore = scored[i].first;
			++tableCount;
		}
		// Asked again here rather than trusting the scan above: the inventory
		// moves between the two - a town errand happens in between - and a stall
		// that loses two of its three lines on the way to the pitch should stay
		// packed up rather than open with what is left.
		if (!IsPlayerBotStallWorthOpening(tableCount, bestScore))
		{
			state.dwNextShopKeepTime = dwNow + (tableCount == 0
					? number(300000, 600000) : number(120000, 240000));
			return false;
		}

		char sign[SHOP_SIGN_MAX_LEN + 1];
		snprintf(sign, sizeof(sign), "%s", ch->GetName());

		// Opening a stall costs a shop bundle, exactly as it does for a player:
		// OpenMyShop consumes one 50200 and refuses outright without it. The other
		// accepted item, the permanent 71049, takes a branch that writes through
		// GetDesc() - a bot has no client descriptor, so that path must be avoided.
		if (ch->CountSpecifyItem(71049) > 0)
			return false;
		if (ch->CountSpecifyItem(50200) == 0)
		{
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
		sys_log(0, "PLAYERBOT_SHOP: opened pid=%u name=%s items=%u first_vnum=%u first_price=%u pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)tableCount,
				offers[0].dwVnum, offers[0].dwPrice, ch->GetX(), ch->GetY());
		return true;
	}

	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance)
	{
		if (DISTANCE_APPROX(ch->GetX() - goalX, ch->GetY() - goalY) <= arrivalDistance)
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "town_interaction");
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
		const bool moveAccepted = MovePlayerBot(ch, goalX, goalY, dwNow, 16, true, true);
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
				state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_REFINE, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_WAIT)
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
