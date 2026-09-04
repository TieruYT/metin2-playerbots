#ifndef __INC_METIN2_PLAYERBOT_ECONOMY_H__
#define __INC_METIN2_PLAYERBOT_ECONOMY_H__

// What a bot does with money and with the contents of its bag: deciding what is
// junk, selling it to the right merchant, upgrading gear at the blacksmith,
// rerolling bonus lines, and running a market stall of its own.
//
// The one rule worth knowing before changing anything here: IsPlayerBotJunkItem
// **defaults to true**. Anything worth keeping needs an explicit exemption, or
// bots vendor it on their next trip to town.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_gear.h - it prices and sells what that file decides to wear.

namespace
{
	// Defined with the market-stall code, which comes later because it needs
	// the town. Refining announces a good result the moment it happens, so it
	// cannot wait for that file.
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, LPITEM item, int newPlus);

	PIXEL_POSITION GetPlayerBotGeneralStorePos(long mapIndex)
	{
		PIXEL_POSITION pos;
		pos.x = 0;
		pos.y = 0;
		pos.z = 0;

		if (mapIndex == 21 || mapIndex == 23) // Chunjo M1 / M3
		{
			pos.x = 59000;
			pos.y = 68900;
		}
		else if (mapIndex == 1 || mapIndex == 3) // Shinsoo M1 / M3
		{
			pos.x = 67800;
			pos.y = 56500;
		}
		else if (mapIndex == 41 || mapIndex == 43) // Jinno M1 / M3
		{
			pos.x = 38300;
			pos.y = 69300;
		}

		return pos;
	}

	DWORD GetPlayerBotSkillBookSkillVnum(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_SKILLBOOK)
			return 0;
		return item->GetVnum() == 50300 ? (DWORD)item->GetSocket(0) : (DWORD)item->GetValue(0);
	}

	bool IsPlayerBotOwnSkill(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || skillVnum == 0 || ch->GetSkillGroup() == 0)
			return false;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (BYTE i = 0; i < build.bSkillCount; ++i)
			if (build.dwSkills[i] == skillVnum)
				return true;
		return false;
	}

	bool PlayerBotNeedsRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch || materialVnum == 0)
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}

		for (size_t i = 0; i < gear.size(); ++i)
		{
			LPITEM item = gear[i];
			if (!item || item->GetRefinedVnum() == 0 ||
					item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
			{
				if (recipe->materials[m].vnum == materialVnum &&
						ch->CountSpecifyItem(materialVnum) < recipe->materials[m].count * 2)
					return true;
			}
		}
		return false;
	}

	bool IsPlayerBotJunkItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->isLocked())
			return false;

		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return false;

		const DWORD vnum = item->GetVnum();

		// Level-30 weapons with average/skill damage are strategic market assets.
		// Never vendor them: this also applies when the current owner is below level
		// 30 or belongs to another class. They remain available for future playerbot
		// trading/private shops instead of disappearing for a trivial NPC price.
		if (IsPlayerBotSpecialLevel30Weapon(item))
			return false;

		// Quest progress must survive every merchant visit. In particular, Horse
		// Medals used to look like ordinary miscellaneous loot and could be sold
		// before the world-travel state machine returned the bot to the Stable Boy.
		if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM || (vnum >= 50701 && vnum <= 50706))
			return false;

		// Fishing tackle and the catch worth keeping. Pearls are the entire point
		// of a fishing trip -- they are what carries equipment to +7/+8/+9 -- and a
		// vendored rod would simply have to be bought again for the next session.
		// Ordinary fish and bones stay sellable: that is the angler's pocket money.
		if (item->GetType() == ITEM_ROD || vnum == PLAYERBOT_FISHING_BAIT_VNUM ||
				vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;

		// Arrows are ammunition, not a primary weapon/equipment candidate. Keep all
		// spare stacks for an Archer (including a Ninja which is about to choose the
		// deterministic Bow profession), while other classes may sell accidental
		// arrow drops at the Weapon Merchant.
		if (item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW)
		{
			const bool isOrWillBeArcher = ch->GetJob() == JOB_ASSASSIN &&
					(ch->GetSkillGroup() == 2 ||
					 (ch->GetSkillGroup() == 0 && (ch->GetPlayerID() % 2) != 0));
			return !isOrWillBeArcher;
		}

		if (item->GetType() == ITEM_SKILLBOOK)
		{
			// Keep books for the selected build (also before profession selection).
			// Books for another class/build may first be handed to a party member;
			// if nobody needs them they become normal miscellaneous loot.
			return ch->GetSkillGroup() != 0 &&
					!IsPlayerBotOwnSkill(ch, GetPlayerBotSkillBookSkillVnum(item));
		}

		// Preserve health, mana, green and purple speed potions
		if (vnum == 27051 || vnum == 27001 || vnum == 27002 || vnum == 27003 ||
			vnum == 27052 || vnum == 27004 || vnum == 27005 || vnum == 27006 ||
			(vnum >= 27100 && vnum <= 27105) || vnum == 27053 || vnum == 27054)
			return false;

		// Preserve every Apprentice Chest until the bot can open it. Class-specific
		// first chests use 50212/50213, while later progression boxes use 50187-50196.
		if (vnum == GetStarterChestVnum(ch->GetJob()) ||
				(vnum >= 50187 && vnum <= 50196))
			return false;

		// Preserve only materials on this bot's current two-attempt refine wishlist.
		// Unneeded materials no longer fill the inventory forever.
		if (item->GetType() == ITEM_MATERIAL)
			return !PlayerBotNeedsRefineMaterial(ch, vnum);
		if (vnum >= 30000 && vnum <= 30200)
			return !PlayerBotNeedsRefineMaterial(ch, vnum);
		if (vnum >= 70038 && vnum <= 70060)
			return false;

		// Keep at most one immediately usable upgrade for each wear slot.  The old
		// test kept every item that scored above the currently worn one; at high
		// drop rates that meant dozens of near-identical weapons and armours could
		// never become junk even though only the best one would ever be equipped.
		if (IsPlayerBotEquipmentCandidate(ch, item))
		{
			const int wearCell = item->FindEquipCell(ch);
			if (wearCell >= 0 && wearCell < WEAR_MAX_NUM)
			{
				if (item->GetLevelLimit() > ch->GetLevel())
					return true;

				LPITEM oldItem = ch->GetWear(wearCell);
				const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
				const long long oldScore = oldItem ? GetPlayerBotEquipmentScore(oldItem, ch) : 0;
				if (!oldItem || itemScore > oldScore)
				{
					for (WORD otherCell = 0; otherCell < INVENTORY_MAX_NUM; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}

				// A well-refined item replaced by genuinely stronger progression gear is
				// still valuable to another bot.  Keep only the single best +6-or-higher
				// reserve for this wear slot; the nearby sharing pass will hand the real
				// item (including sockets/attributes) to a lower-level compatible build.
				if (item->GetRefineLevel() >= PLAYERBOT_RESERVE_GEAR_MIN_REFINE)
				{
					for (WORD otherCell = 0; otherCell < INVENTORY_MAX_NUM; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item ||
								other->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
								!IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}
			}
		}

		return true;
	}

	EPlayerBotMerchantCategory GetPlayerBotJunkMerchant(LPITEM item)
	{
		if (!item)
			return BOT_MERCHANT_MISC;

		if (item->GetType() == ITEM_WEAPON)
			return BOT_MERCHANT_WEAPON;
		if (item->GetType() == ITEM_ARMOR || item->GetType() == ITEM_UNIQUE ||
				item->GetType() == ITEM_RING || item->GetType() == ITEM_BELT)
			return BOT_MERCHANT_ARMOR;
		return BOT_MERCHANT_MISC;
	}

	bool HasPlayerBotJunkForMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotJunkItem(ch, item) &&
					GetPlayerBotJunkMerchant(item) == category)
				return true;
		}
		return false;
	}

	size_t CountPlayerBotJunkItems(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return 0;
		size_t count = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			if (IsPlayerBotJunkItem(ch, ch->GetInventoryItem(cell)))
				++count;
		return count;
	}

	bool SellPlayerBotJunkAtMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category,
			const char* merchantName)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		size_t soldCount = 0;
		long long totalSoldGold = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotJunkItem(ch, item) ||
					GetPlayerBotJunkMerchant(item) != category)
				continue;

			DWORD price = item->GetShopBuyPrice();
			if (price == 0)
				price = item->GetProto() ? item->GetProto()->dwGold : 100;
			price = std::max<DWORD>(10, price / 5);
			totalSoldGold += price;
			ch->PointChange(POINT_GOLD, price);
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_SHOP_SELL");
			++soldCount;
		}

		if (soldCount > 0)
		{
			sys_log(0, "PLAYERBOT_AI: sold %u items at %s pid=%u name=%s gold_gained=%lld total_gold=%lld",
					(unsigned int)soldCount, merchantName ? merchantName : "merchant",
					ch->GetPlayerID(), ch->GetName(), totalSoldGold, (long long)ch->GetGold());
		}
		return soldCount > 0;
	}

	bool HasPlayerBotBackupGear(LPCHARACTER ch, BYTE wearCell)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->FindEquipCell(ch) == wearCell)
				return true;
		}

		return false;
	}

	// --- Bonus lines ---------------------------------------------------------
	// Gear is only half a bot's power; the four bonus lines are the other half. A
	// level-appropriate weapon rolled into four resistances is genuinely worse
	// than the one it replaced, and until now nothing ever looked at them.
	//
	// The scoring below is deliberately coarse. It exists to tell "worth keeping"
	// from "roll it again", not to model the damage formula: every line is scored
	// as points-per-typical-roll so that a +2000 HP line and a +15 attack line
	// can be compared at all.
	bool IsPlayerBotCaster(LPCHARACTER ch)
	{
		return ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
	}

	int ScorePlayerBotBonusLine(LPCHARACTER ch, bool bOffensiveSlot, BYTE type, short value)
	{
		// A negative roll exists (movement speed on some sets) and is worth less
		// than nothing, so it must not be able to prop up a bad item's total.
		if (value <= 0)
			return 0;

		switch (type)
		{
			case APPLY_SKILL_DAMAGE_BONUS:      return value * 12;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS: return value * 10;
			case APPLY_CRITICAL_PCT:            return value * 10;
			case APPLY_PENETRATE_PCT:           return value * 10;
			case APPLY_ATTBONUS_MONSTER:        return value * 8;
			case APPLY_ATT_SPEED:               return value * 8;
			case APPLY_STEAL_HP:                return value * 6;
			case APPLY_ATT_GRADE_BONUS:         return bOffensiveSlot ? value * 5 : value * 3;
			case APPLY_CAST_SPEED:              return IsPlayerBotCaster(ch) ? value * 8 : value;
			case APPLY_MAX_HP_PCT:              return value * 15;
			case APPLY_DEF_GRADE_BONUS:         return bOffensiveSlot ? value * 2 : value * 6;
			case APPLY_MOV_SPEED:               return value * 3;
			// Big absolute numbers that have to be scaled down to compare with the
			// percentage lines above.
			case APPLY_MAX_HP:                  return value / 4;
			case APPLY_MAX_SP:                  return IsPlayerBotCaster(ch) ? value / 4 : value / 12;
			// Everything else - resistances, stamina, experience bonus - is real but
			// minor for a bot that only grinds. Never zero: a line is still a line.
			default:                            return value;
		}
	}

	bool IsPlayerBotOffensiveSlot(BYTE wearCell)
	{
		return wearCell == WEAR_WEAPON;
	}

	int ScorePlayerBotItemBonuses(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!ch || !item)
			return 0;
		const bool bOffensive = IsPlayerBotOffensiveSlot(wearCell);
		int score = 0;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			score += ScorePlayerBotBonusLine(ch, bOffensive,
					item->GetAttributeType(i), item->GetAttributeValue(i));
		}
		return score;
	}

	// An item the engine will actually accept a stone on. UseItemEx refuses an
	// equipped item outright ("if (item2->IsEquipped()) return false"), costumes,
	// and anything without an attribute set, so a bot has to take the piece off
	// first - exactly as a player does.
	bool CanPlayerBotRerollItem(LPITEM item)
	{
		return item && item->GetType() != ITEM_COSTUME && !item->isLocked() &&
				!item->IsExchanging() && item->GetAttributeSetIndex() != -1;
	}

	// The stones cannot be dropped, sold, traded or shopped, so there is no market
	// to walk to: the bot pays for one the same way it pays for its stall.
	bool BuyPlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		if (ch->CountSpecifyItem(vnum) > 0)
			return true;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;
		if (ch->GetEmptyInventory(1) < 0)
			return false;
		if (!ch->AutoGiveItem(vnum, 1, -1, false))
			return false;
		ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_BONUS_STONE_PRICE);
		return true;
	}

	bool ConsumePlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetVnum() != vnum)
				continue;
			if (stone->GetCount() > 1)
				stone->SetCount(stone->GetCount() - 1);
			else
				ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_BONUS");
			return true;
		}
		return false;
	}

	// Worn gear only. Spares in the bag are sold or put in a stall long before
	// they are worth polishing, and rerolling them would spend the gold the bot
	// needs for its next real upgrade.
	bool ManagePlayerBotBonusReroll(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextBonusCheckTime)
			return false;
		state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_INTERVAL;
		if (ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL)
			return false;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		int stonesUsed = 0;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]) &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++i)
		{
			const BYTE wearCell = wearSlots[i];
			LPITEM item = ch->GetWear(wearCell);
			if (!CanPlayerBotRerollItem(item))
				continue;

			const int count = item->GetAttributeCount();
			const int score = ScorePlayerBotItemBonuses(ch, item, wearCell);

			// An empty line is free power: add before rerolling, always. Only once
			// the item is full does the quality of what it rolled start to matter,
			// and USE_CHANGE_ATTRIBUTE needs at least one line to work on anyway.
			const bool bWantAdd = count < 4;
			const bool bWantChange = !bWantAdd && score < PLAYERBOT_BONUS_KEEP_SCORE;
			if (!bWantAdd && !bWantChange)
				continue;

			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM
					: PLAYERBOT_BONUS_CHANGE_VNUM;
			if (!BuyPlayerBotBonusStone(ch, stoneVnum))
				continue;

			// The piece has to come off for the engine to touch it, and it has to go
			// back on afterwards - a bot walking around with its weapon in the bag
			// would be worse than any bonus line it could win.
			if (!ch->UnequipItem(item))
				continue;

			if (bWantAdd)
				item->AddAttribute();
			else
				item->ChangeAttribute();

			ConsumePlayerBotBonusStone(ch, stoneVnum);
			++stonesUsed;

			const int newScore = ScorePlayerBotItemBonuses(ch, item, wearCell);
			if (!ch->EquipItem(item))
			{
				sys_err("PLAYERBOT_BONUS: could not re-equip pid=%u name=%s vnum=%u slot=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
						(unsigned int)wearCell);
				continue;
			}

			sys_log(0, "PLAYERBOT_BONUS: %s pid=%u name=%s vnum=%u slot=%u lines=%d->%d score=%d->%d gold=%d",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(),
					item->GetVnum(), (unsigned int)wearCell, count,
					item->GetAttributeCount(), score, newScore,
					(int)(ch->GetGold() / 1000));
		}

		return stonesUsed > 0;
	}

	bool ManagePlayerBotRefining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextRefineCheckTime)
			return false;

		state.dwNextRefineCheckTime = dwNow + PLAYERBOT_REFINE_INTERVAL;

		// Collect all upgradable worn items and inventory candidates
		struct TRefineCandidate
		{
			BYTE wearCell;
			LPITEM item;
			BYTE plusLevel;
			BYTE priority;
		};

		std::vector<TRefineCandidate> candidates;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0)
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = wearSlots[i];
			cand.item = item;
			cand.plusLevel = plusLevel;
			cand.priority = coreProgression ? 0 : 2;
			candidates.push_back(cand);
		}

		// Also collect candidate gear in inventory
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetRefinedVnum() == 0 || !IsPlayerBotEquipmentCandidate(ch, item))
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = 255;
			cand.item = item;
			cand.plusLevel = plusLevel;
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			cand.priority = coreProgression ? 0 : 2;
			candidates.push_back(cand);
		}

		if (candidates.empty())
			return false;

		// Core level-appropriate weapon/body gear comes first. Within the same
		// priority, raise the lowest plus level so both essentials progress evenly.
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			for (size_t j = i + 1; j < candidates.size(); ++j)
			{
				if (candidates[j].priority < candidates[i].priority ||
						(candidates[j].priority == candidates[i].priority &&
						 candidates[j].plusLevel < candidates[i].plusLevel))
				{
					TRefineCandidate tmp = candidates[i];
					candidates[i] = candidates[j];
					candidates[j] = tmp;
				}
			}
		}

		int refinedCount = 0;
		for (size_t i = 0; i < candidates.size() && refinedCount < 2; ++i)
		{
			LPITEM item = candidates[i].item;
			if (!item || item->GetRefinedVnum() == 0)
				continue;

			const DWORD oldVnum = item->GetVnum();
			const DWORD nextVnum = item->GetRefinedVnum();
			const BYTE plusLevel = candidates[i].plusLevel;
			const BYTE wearCell = candidates[i].wearCell;
			const bool hasBackup = (wearCell != 255) ? HasPlayerBotBackupGear(ch, wearCell) : true;

			if (wearCell == WEAR_WEAPON && !hasBackup && plusLevel >= 4 && ch->GetGold() < 5000)
				continue;

			if (plusLevel >= 5 && !hasBackup && ch->GetGold() < 15000)
				continue;

			if (plusLevel == 4 && !hasBackup && number(1, 100) > 75)
				continue;

			// Equipment management after an earlier attempt may have equipped another
			// queued candidate, so inspect its live position instead of trusting the
			// location captured when the list was built.
			if (item->IsEquipped())
			{
				int emptyCell = ch->GetEmptyInventory(item->GetSize());
				if (emptyCell < 0)
					continue;
				if (!ch->UnequipItem(item) || item->IsEquipped())
					continue;
			}

			// DoRefine(false) is the regular blacksmith path: it reads refine_proto,
			// charges the exact fee, consumes every required material and applies the
			// normal success/failure roll.  The return value only says that an attempt
			// was performed, so compare the result item count to log its real outcome.
			const int resultCountBefore = ch->CountSpecifyItem(nextVnum);
			if (ch->DoRefine(item, false))
			{
				const bool success = ch->CountSpecifyItem(nextVnum) > resultCountBefore;
				BroadcastPlayerBotRefineSuccess(ch, item, (int)plusLevel + 1);
				sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u",
						success ? "SUCCESS" : "FAILED_BURNED", ch->GetPlayerID(), ch->GetName(),
						oldVnum, nextVnum, plusLevel + 1);
				++refinedCount;
			}
			else
			{
				sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u (requirements/state)",
						ch->GetPlayerID(), ch->GetName(), oldVnum, plusLevel);
			}

			// Do not equip the result again between consecutive + levels.  Keep it
			// visibly in the inventory for the complete blacksmith session and let
			// the town state equip the final/best result once refining is finished.
		}

		return refinedCount > 0;
	}

	bool ManagePlayerBotMiscMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		CompactPlayerBotPotionStacks(ch);
		SellPlayerBotExcessPotions(ch);

		// Count red and blue potions
		size_t redCount = 0;
		size_t blueCount = 0;
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

		// Miscellaneous loot belongs to Handlarka. Weapons and wearable equipment
		// are deliberately left for their own specialist merchants.
		SellPlayerBotJunkAtMerchant(ch, BOT_MERCHANT_MISC, "misc_merchant");

		// Economical potion purchase at Handlarka.  Refining is intentionally
		// performed in the separate blacksmith phase after the bot walks there.
		const bool isMage = (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		const BYTE botLvl = ch->GetLevel();

		if (botLvl <= 10)
		{
			if (redCount < 30 && ch->GetGold() >= 300)
			{
				ch->PointChange(POINT_GOLD, -240);
				ch->AutoGiveItem(27001, 30); // Red Potion (S) 30x
			}
			if (isMage && blueCount < 20 && ch->GetGold() >= 400)
			{
				ch->PointChange(POINT_GOLD, -360);
				ch->AutoGiveItem(27004, 15); // Blue Potion (S) 15x
			}
		}
		else
		{
			// Unit prices are the ones the old fixed purchases implied: 20 yang for
			// a Red Potion (M), 32 for a Blue Potion (M).
			const DWORD RED_TARGET = 800;
			const DWORD BLUE_TARGET = 600;
			const DWORD RED_UNIT = 20;
			const DWORD BLUE_UNIT = 32;
			// Standing at the merchant already: fill the belt right up whatever the
			// level, because this costs nothing extra. The decision to make the
			// trip at all lives in NeedsPlayerBotPotions and is far stricter.
			// Never spend more than half the purse, so shopping can't leave the
			// bot unable to afford a refine.
			if (redCount < RED_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(RED_TARGET - redCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / RED_UNIT);
				const DWORD buy = want < affordable ? want : affordable;
				if (buy > 0)
				{
					ch->PointChange(POINT_GOLD, -(int)(buy * RED_UNIT));
					ch->AutoGiveItem(27002, buy);
				}
			}
			// Skills spend SP continuously, so a warrior wants a reserve too. It
			// simply must never be the thing that forbids travelling.
			if (blueCount < BLUE_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(BLUE_TARGET - blueCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / BLUE_UNIT);
				const DWORD buy = want < affordable ? want : affordable;
				if (buy > 0)
				{
					ch->PointChange(POINT_GOLD, -(int)(buy * BLUE_UNIT));
					ch->AutoGiveItem(27005, buy);
				}
			}
		}

		// Even the level-one shoes add movement speed. Missing footwear is therefore
		// a progression problem, not cosmetic equipment.
		if (NeedsPlayerBotProgressionBoots(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionBootsVnum(ch), "boots");

		return true;
	}

	bool ManagePlayerBotWeaponMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_WEAPON, "weapon_merchant");
		bool bought = false;
		const bool isArcher = ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2;
		// A missing weapon is essential, so restore the cheap functional weapon
		// first. With a bow already equipped, ammunition takes priority over a
		// level-tier upgrade: buying a better bow and leaving zero Yang for arrows
		// merely creates a better-equipped idle bot.
		if (!ch->GetWear(WEAR_WEAPON))
			bought = BuyPlayerBotEmergencyWeapon(ch) || bought;
		if (isArcher)
			bought = BuyPlayerBotArrowsAtMerchant(ch) || bought;
		if (NeedsPlayerBotProgressionWeapon(ch) &&
				(!isArcher || CountPlayerBotArrows(ch) >= PLAYERBOT_ARROW_RESTOCK_THRESHOLD))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWeaponVnum(ch), "weapon") || bought;
		return sold || bought;
	}

	bool ManagePlayerBotArmorMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_ARMOR, "armor_merchant");
		bool bought = NeedsPlayerBotProgressionArmor(ch) &&
				BuyPlayerBotProgressionGear(ch,
						GetPlayerBotProgressionArmorVnum(ch), "armor");
		if (NeedsPlayerBotProgressionShield(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionShieldVnum(ch), "shield") || bought;
		if (NeedsPlayerBotProgressionHelmet(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionHelmetVnum(ch), "helmet") || bought;
		return sold || bought;
	}

	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetRefinedVnum() == 0 ||
				item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
			return false;

		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe || ch->GetGold() < ch->ComputeRefineFee(recipe->cost))
			return false;

		for (int i = 0; i < recipe->material_count; ++i)
		{
			if (ch->CountSpecifyItem(recipe->materials[i].vnum) < recipe->materials[i].count)
				return false;
		}
		return true;
	}

	bool HasPlayerBotRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, item) &&
					CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}
		return false;
	}

	bool HasPlayerBotPriorityRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		// Cross-map blacksmith trips are reserved for currently worn essentials.
		// A routine accessory or spare can wait until the next normal M1 visit, but
		// a weapon/body/shield/helmet/boots upgrade should not sit unused in M2/M3.
		const BYTE coreWearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD, WEAR_FOOTS
		};
		for (size_t i = 0; i < sizeof(coreWearSlots) / sizeof(coreWearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(coreWearSlots[i]);
			if (item && CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}
		return false;
	}
}

#endif
