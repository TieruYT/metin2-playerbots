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

	// A weapon with a line a player stops rerolling at: average damage from
	// PLAYERBOT_BONUS_KEEP_AVERAGE, or skill damage from
	// PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT. In this engine every failed
	// refine destroys the item - DoRefine has no grade that only drops a
	// level - so a +8 bow with 51% average burned at the blacksmith on a
	// forty-percent roll ("i spalil u kowala"). Such a weapon is refined only
	// under a Blessing Scroll, which hands it back a level down instead.
	bool IsPlayerBotPrizeWeapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		long avg = 0, skill = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (t == APPLY_NORMAL_HIT_DAMAGE_BONUS) avg += item->GetAttributeValue(i);
			else if (t == APPLY_SKILL_DAMAGE_BONUS) skill += item->GetAttributeValue(i);
		}
		return avg >= PLAYERBOT_BONUS_KEEP_AVERAGE || skill >= PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT;
	}

	// Anything a player would not put on the anvil without a scroll: a prize
	// weapon, or a piece already carrying PLAYERBOT_PRIZE_LINES lines.
	bool IsPlayerBotPrizeItem(LPITEM item)
	{
		if (!item)
			return false;
		if (IsPlayerBotPrizeWeapon(item))
			return true;
		int lines = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0 && item->GetAttributeValue(i) != 0)
				++lines;
		return lines >= PLAYERBOT_PRIZE_LINES;
	}

	// Every skill book in the bag, whatever the skill.
	int CountPlayerBotSkillBooks(LPCHARACTER ch)
	{
		int books = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_SKILLBOOK)
				books += item->GetCount();
		}
		return books;
	}

	// Whether a book is beyond the bag's working stock: somebody else's skill,
	// or one of its own past GetPlayerBotBookKeepLimit. What the counter
	// sells, and what the safebox takes when the counter has not.
	bool IsPlayerBotSurplusSkillBook(LPCHARACTER ch, LPITEM item);

	// How many books of one of its own skills a bot keeps in the bag: the
	// working stock while the skill is readable, a few before that, none once
	// a book can do nothing more for it. Every rule that keeps, lists or buys
	// a book asks this, so the bag, the counter and the market agree.
	int GetPlayerBotBookKeepLimit(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || skillVnum == 0)
			return 0;
		if (ch->GetSkillMasterType(skillVnum) >= SKILL_GRAND_MASTER)
			return 0;
		const BYTE level = ch->GetSkillLevel(skillVnum);
		if (ch->GetSkillMasterType(skillVnum) == SKILL_MASTER && level >= 20 && level < 30)
			return PLAYERBOT_BOOK_KEEP_PER_SKILL;
		return PLAYERBOT_BOOK_KEEP_UNREADABLE;
	}

	// The engine's stacking rule, asked of two bag items: MoveItem pours one
	// into the other only for the same vnum with every socket equal.
	bool PlayerBotStacksTogether(LPITEM item, LPITEM other)
	{
		if (!item || !other || item == other || item->GetVnum() != other->GetVnum())
			return false;
		if (!item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (int s = 0; s < ITEM_SOCKET_MAX_NUM; ++s)
			if (item->GetSocket(s) != other->GetSocket(s))
				return false;
		return true;
	}

	// Pour split stacks together, a few at a time. MoveItem with a count of
	// zero moves as much of the source as the destination has room for and
	// removes the source when it is emptied - the same thing a player's drag
	// does, packets and item log included.
	int MergePlayerBotStacks(LPCHARACTER ch, int maxMerges)
	{
		int merged = 0;
		for (WORD i = 0; i < INVENTORY_MAX_NUM && merged < maxMerges; ++i)
		{
			LPITEM item = ch->GetInventoryItem(i);
			if (!item || item->IsEquipped() || item->isLocked() ||
					item->GetCount() >= PLAYERBOT_STACK_MAX)
				continue;
			for (WORD j = i + 1; j < INVENTORY_MAX_NUM && merged < maxMerges; ++j)
			{
				LPITEM other = ch->GetInventoryItem(j);
				if (!other || other->IsEquipped() || other->isLocked() ||
						!PlayerBotStacksTogether(item, other))
					continue;
				if (ch->MoveItem(TItemPos(INVENTORY, j), TItemPos(INVENTORY, i), 0))
					++merged;
				if (item->GetCount() >= PLAYERBOT_STACK_MAX)
					break;
			}
		}
		return merged;
	}

	void ManagePlayerBotStackMerge(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextStackMergeTime)
			return;
		state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x53544b4dU) % 60000U;
		// Not behind a counter: the singles there were split on purpose, and
		// the shop table points at the cells they are in.
		if (!ch->IsItemLoaded() || ch->GetMyShop())
			return;
		const int merged = MergePlayerBotStacks(ch, PLAYERBOT_STACK_MERGES_PER_PASS);
		if (merged > 0)
			sys_log(0, "PLAYERBOT_BAG: merged stacks pid=%u name=%s merges=%d",
					ch->GetPlayerID(), ch->GetName(), merged);
		// A pass that used its whole budget has more to do: back soon, not in
		// five minutes - a closed counter leaves eight packs of one material.
		if (merged >= PLAYERBOT_STACK_MERGES_PER_PASS)
			state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_AFTER_SHOP_MS;
	}

	// How many units of a stackable go on one counter line. A private shop
	// sells a line whole, so a stack of twenty scrolls on one line is twenty
	// scrolls or nothing: what a player buys one at a time - potions,
	// scrolls, stones, the shell and the pearls - is a single; a material is
	// a pack of PLAYERBOT_SHOP_PACK_UNITS, small enough to buy for one refine
	// and few enough lines to leave room on the counter. Zero for anything
	// that does not stack.
	int GetPlayerBotStallLineUnits(LPITEM item)
	{
		if (!item || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return 0;
		if (item->GetType() == ITEM_USE || item->GetType() == ITEM_METIN ||
				(item->GetVnum() >= 27992 && item->GetVnum() <= 27994))
			return 1;
		return PLAYERBOT_SHOP_PACK_UNITS;
	}

	bool IsPlayerBotSinglyTradedGoods(LPITEM item)
	{
		return GetPlayerBotStallLineUnits(item) == 1;
	}

	// Everything this bot wears or carries that is still below its refine target,
	// against what those refines actually consume. A materialVnum of zero asks
	// the looser question - short of anything at all - which is what decides
	// whether walking to the market is worth the trip; a real vnum asks about the
	// one thing on the counter in front of it.
	bool PlayerBotIsShortOfRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch)
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
				const DWORD vnum = recipe->materials[m].vnum;
				if (vnum == 0 || recipe->materials[m].count == 0)
					continue;
				if (materialVnum != 0 && vnum != materialVnum)
					continue;
				if (ch->CountSpecifyItem(vnum) < recipe->materials[m].count * 2)
					return true;
			}
		}
		return false;
	}

	bool PlayerBotNeedsRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		return materialVnum != 0 &&
				PlayerBotIsShortOfRefineMaterial(ch, materialVnum);
	}

	bool PlayerBotNeedsAnyRefineMaterial(LPCHARACTER ch)
	{
		return PlayerBotIsShortOfRefineMaterial(ch, 0);
	}

	// Every material this bot is short of, in one set. The same walk as the
	// question above, asked once per target scan instead of once per monster:
	// a scan looks at dozens of candidates a second and each answer costs a
	// pass over the bag.
	void CollectPlayerBotWantedMaterials(LPCHARACTER ch, std::set<DWORD>& out)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
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
			const TRefineTable* recipe =
					CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
			{
				const DWORD vnum = recipe->materials[m].vnum;
				if (vnum == 0 || recipe->materials[m].count == 0)
					continue;
				if (ch->CountSpecifyItem(vnum) < recipe->materials[m].count * 2)
					out.insert(vnum);
			}
		}
	}

	// Every material any recipe in the game consumes, collected once. There is
	// no iterator over the recipe table, so the ids are walked; what comes back
	// is the only list worth trading, because a counter slot spent on something
	// no anvil asks for is a slot the market cannot use. Eighty-four of them in
	// this world, against two hundred and forty items of type ITEM_MATERIAL.
	const std::set<DWORD>& GetPlayerBotRefineMaterialVnums()
	{
		static std::set<DWORD> s_materials;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			for (DWORD id = 1; id <= PLAYERBOT_REFINE_RECIPE_MAX_ID; ++id)
			{
				const TRefineTable* recipe =
						CRefineManager::instance().GetRefineRecipe(id);
				if (!recipe)
					continue;
				for (int m = 0; m < recipe->material_count; ++m)
					if (recipe->materials[m].vnum != 0)
						s_materials.insert(recipe->materials[m].vnum);
			}
			sys_log(0, "PLAYERBOT_ECONOMY: %u refine materials are worth a counter slot",
					(unsigned int)s_materials.size());
		}
		return s_materials;
	}

	// The fisherman's keepsakes: kept whatever else is true, because they are
	// the entire point of a fishing trip and the road to +7 and beyond. They are
	// counted here so the stock cap below does not spend its eight cells on them.
	bool IsPlayerBotFishingKeepsake(DWORD vnum)
	{
		return vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM);
	}

	// A refine material is whatever a recipe consumes, whatever type the proto
	// gives it. The first version asked for ITEM_MATERIAL as well, and eight of
	// the eighty-four are not: the fishbone is ITEM_RESOURCE, the shellfish and
	// the blessing scroll are ITEM_USE, the three pearls are ITEM_RESOURCE. The
	// fishbone alone is in thirteen recipes and was being sold as "the angler's
	// pocket money".
	bool IsPlayerBotTradeableMaterial(LPITEM item)
	{
		if (!item)
			return false;
		const std::set<DWORD>& materials = GetPlayerBotRefineMaterialVnums();
		return materials.find(item->GetVnum()) != materials.end();
	}

	// Junk goes to the general-goods merchant on the next town visit, and for a
	// refine material that was the end of it. Five hundred and twenty-six bots
	// on this world are short of one; the top of that list is three hundred and
	// seventeen bots wanting fourteen hundred Orc Amulets between them, against
	// five in existence. A material its finder did not personally need was being
	// destroyed at the rate it dropped, which is why a counter carried one only
	// when somebody happened to have a spare, and why an evening's whole market
	// saw four material purchases.
	//
	// So a spare is kept and put on the counter instead, where the bot that
	// needs it walks up and buys it. Nothing is added to any NPC: the supply is
	// what the world already drops, and the trade is between bots.
	//
	// Bounded, or a bag would fill - there are ninety cells and no shortage of
	// materials to find. The cap counts every material cell, the ones held for
	// this bot's own anvil included, because those are already spoken for above:
	// a material on the wishlist never reaches this test.
	bool IsPlayerBotSurplusMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotTradeableMaterial(item))
			return true;

		// Cell order decides, so the same spares stay put from one town visit
		// to the next rather than the bag reshuffling itself every trip.
		size_t ahead = 0;
		const WORD ownCell = item->GetCell();
		for (WORD cell = 0; cell < ownCell && cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && IsPlayerBotTradeableMaterial(other) &&
					!IsPlayerBotFishingKeepsake(other->GetVnum()) &&
					++ahead >= PLAYERBOT_MATERIAL_STOCK_SLOTS)
				return true;
		}
		return false;
	}

	int CountPlayerBotFreeInventoryCells(LPCHARACTER ch)
	{
		int free = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			if (!ch->GetInventoryItem(cell))
				++free;
		return free;
	}

	// Occupied cells against PLAYERBOT_BAG_FULL_PERCENT of the bag. Counted
	// by cell rather than by item, so a weapon's three cells count as three.
	bool IsPlayerBotBagFull(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const int occupied = INVENTORY_MAX_NUM - CountPlayerBotFreeInventoryCells(ch);
		return occupied * 100 >= INVENTORY_MAX_NUM * PLAYERBOT_BAG_FULL_PERCENT;
	}

	// Books of one skill in the cells before this one. Cell order decides, so
	// the same books stay put from one town visit to the next.
	int CountPlayerBotSkillBooksAhead(LPCHARACTER ch, LPITEM item, DWORD skillVnum)
	{
		int ahead = 0;
		const WORD ownCell = item->GetCell();
		for (WORD cell = 0; cell < ownCell && cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetType() == ITEM_SKILLBOOK &&
					GetPlayerBotSkillBookSkillVnum(other) == skillVnum)
				++ahead;
		}
		return ahead;
	}

	// A key whose lock matches this chest, anywhere in the bag.
	bool PlayerBotHasTreasureKeyFor(LPCHARACTER ch, LPITEM box)
	{
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM key = ch->GetInventoryItem(cell);
			if (key && key->GetType() == ITEM_TREASURE_KEY && key->GetValue(0) == box->GetValue(0))
				return true;
		}
		return false;
	}

	// How many bots are short of a material and can pay for it, from the
	// market ledger (playerbot_market.h, which comes after this fragment).
	DWORD GetPlayerBotLedgerDemand(DWORD vnum);

	// A spare of a higher tier than the piece worn in its slot: not an
	// upgrade yet, but one the blacksmith can make into one, so neither the
	// merchant nor the refine pass treats it as scrap. Only the best such
	// spare per slot counts; the rest are still scrap.
	bool IsPlayerBotHigherTierSpare(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item))
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		if (item->GetLevelLimit() > ch->GetLevel())
			return false;
		LPITEM worn = ch->GetWear(wearCell);
		if (!worn || item->GetLevelLimit() <= worn->GetLevelLimit())
			return false;
		const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
		for (WORD otherCell = 0; otherCell < INVENTORY_MAX_NUM; ++otherCell)
		{
			LPITEM other = ch->GetInventoryItem(otherCell);
			if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
					other->GetLevelLimit() > ch->GetLevel() ||
					other->GetLevelLimit() <= worn->GetLevelLimit() ||
					other->FindEquipCell(ch) != wearCell)
				continue;
			const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
			if (otherScore > itemScore ||
					(otherScore == itemScore && other->GetID() < item->GetID()))
				return false;
		}
		return true;
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

		// Whatever else it is, a +7 or better is not something to hand an NPC for
		// a fifth of the shop price. The reserve rule below keeps one spare per
		// slot and sold the rest; that is how a Riba +9 went to a merchant
		// because the same bot was carrying an axe +9. These go on a stall.
		if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			return false;

		// The Archer's one stone weapon (playerbot_gear.h) is kept.
		if (IsPlayerBotArcherBuild(ch) && IsPlayerBotStoneMeleeWeapon(ch, item) &&
				FindPlayerBotStoneWeapon(ch, false) == item)
			return false;

		// A scrap keeper's low refines are its stock, not its junk - until the
		// bag runs short, and then the merchant gets them like anyone else's.
		// Gear the counter could not sell in six stands is scrap, whatever the
		// keeper rules below would keep it for. Above the precious refine it
		// never gets here.
		if (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR)
		{
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
			if (st != s_mapPlayerBotAIStates.end())
			{
				std::map<DWORD, BYTE>::const_iterator unsold = st->second.mapStallUnsold.find(item->GetID());
				if (unsold != st->second.mapStallUnsold.end() &&
						unsold->second >= PLAYERBOT_SHOP_UNSOLD_SCRAP_STANDS)
					return true;
			}
		}

		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				IsPlayerBotScrapKeeper(ch->GetPlayerID()) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_SCRAP_KEEP_FREE_CELLS)
			return false;

		// Quest progress must survive every merchant visit. In particular, Horse
		// Medals used to look like ordinary miscellaneous loot and could be sold
		// before the world-travel state machine returned the bot to the Stable Boy.
		//
		// And so must what the progress was for. The battle horse scroll is the
		// end of that whole chain - twenty-one medals, a hundred kills in the
		// desert and 500 000 yang at the stable keeper - and it is a miscellaneous
		// item like any other to the junk rule, which says "junk" unless told
		// otherwise. The first bot ever to finish the trial sold it three minutes
		// later, for its share of 1020 gold, and left the world with no battle
		// horse in it at all.
		if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM ||
				vnum == PLAYERBOT_BATTLE_HORSE_BOOK_VNUM ||
				(vnum >= 50701 && vnum <= 50706) ||
				vnum == PLAYERBOT_ORC_TOOTH_VNUM || vnum == PLAYERBOT_JINUNGGYI_STONE_VNUM)
			return false;
		// A chest is opened on the next pass, not sold; the bonus scrolls, boosters
		// and big potions it holds carry ANTI_SELL and never reach this rule.
		if (vnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return false;
		// A treasure chest waits for its key, a key for its chest, and a
		// Forgetting Scroll for a skill stuck at seventeen - this bot's or, across
		// a counter, another's.
		// A chest without its key is kept while there is room for it. Keys are
		// rare and chests are not: a bag under pressure lets the merchant have
		// the chests, so the loot and the Moonlight chests that open by
		// themselves still have somewhere to land.
		if (item->GetType() == ITEM_TREASURE_BOX)
			return CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS &&
					!PlayerBotHasTreasureKeyFor(ch, item);
		if (item->GetType() == ITEM_TREASURE_KEY ||
				item->GetType() == ITEM_GIFTBOX || vnum == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return false;
		// A refine scroll stays in the bag or goes on a counter, never to the
		// merchant. It is still stall goods: another bot needs one too.
		if (IsPlayerBotRefineScroll(vnum))
			return false;
		// A soul stone is somebody's socket: this bot's, or across a counter
		// another's. The merchant paid one yang for a Potwora +4.
		if (item->GetType() == ITEM_METIN)
			return false;

		// Fishing tackle and the catch worth keeping. Pearls are the entire point
		// of a fishing trip -- they are what carries equipment to +7/+8/+9 -- and a
		// vendored rod would simply have to be bought again for the next session.
		// Ordinary fish and bones stay sellable: that is the angler's pocket money.
		// One rod is tackle; a second one is scrap. The bots that bought a rod
		// per session (see CountPlayerBotRods) are carrying fifteen, and the
		// worst of them go to the merchant: a rod that another rod - worn or in
		// the bag - matches or beats in grade.
		if (item->GetType() == ITEM_ROD)
		{
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && worn != item && worn->GetType() == ITEM_ROD && worn->GetVnum() >= vnum)
				return true;
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (other && other != item && other->GetType() == ITEM_ROD &&
						(other->GetVnum() > vnum || (other->GetVnum() == vnum && other->GetID() < item->GetID())))
					return true;
			}
			return false;
		}
		if (vnum == PLAYERBOT_FISHING_BAIT_VNUM ||
				vnum == PLAYERBOT_SHELLFISH_VNUM || vnum == PLAYERBOT_CAMPFIRE_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;
		// Hair dye. The merchant pays nothing for it and a player will: it is
		// the only way to change a character's colour for good, and the anglers
		// pull it out of the water by the handful.
		if (IsPlayerBotHairDye(vnum))
			return false;
		// A dead fish waits for the campfire at the end of the next session, as
		// long as the bot has the wood for one; a grilled fish is a potion.
		if (item->GetType() == ITEM_FISH && item->GetSubType() == FISH_DEAD &&
				ch->CountSpecifyItem(PLAYERBOT_CAMPFIRE_VNUM) > 0)
			return false;
		if (vnum >= PLAYERBOT_GRILLED_FISH_FIRST_VNUM && vnum <= PLAYERBOT_GRILLED_FISH_LAST_VNUM)
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

		// A skill book never goes to the merchant. Its own working stock stays
		// in the bag (GetPlayerBotBookKeepLimit), the surplus - somebody else's
		// skill, or more of its own than it can read - is goods for the counter,
		// and what the bag cannot hold beyond PLAYERBOT_SAFEBOX_BOOK_KEEP of
		// those goes to the storekeeper's safebox on the next town visit
		// (IsPlayerBotSafeboxBook). The merchant paid pennies for Aura Miecza
		// while the warrior three stalls away would have paid a fortune, and
		// "under bag pressure" turned out to be most of a dropper's life.
		if (item->GetType() == ITEM_SKILLBOOK)
			return false;

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

		// What this bot is about to refine with stays in its bag. A spare that
		// some recipe wants stays too, as stock for its own counter - see
		// IsPlayerBotSurplusMaterial for why that is worth eight cells. Judged by
		// the recipe table, not by item type: that is what brings the fishbone
		// and the blessing scroll in.
		// A material somebody on this world is short of is counter goods,
		// not merchant scrap: a Scorpion Tail went to the merchant for a
		// few hundred yang while the next stall along sold one for 58 894.
		// Only a material nobody wants, and only under bag pressure.
		if (IsPlayerBotTradeableMaterial(item))
			return !PlayerBotNeedsRefineMaterial(ch, vnum) &&
					IsPlayerBotSurplusMaterial(ch, item) &&
					GetPlayerBotLedgerDemand(vnum) == 0;
		// The rest of the 30000 block is eight gift boxes and two quest items.
		// No counter would carry those, so there junk still means junk.
		if (vnum >= 30000 && vnum <= 30200)
			return !PlayerBotNeedsRefineMaterial(ch, vnum) &&
					GetPlayerBotLedgerDemand(vnum) == 0 &&
					CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
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
				if (IsPlayerBotHigherTierSpare(ch, item))
					return false;

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

	// The scroll a refine goes under: from PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS
	// the Zwoj Boga Smokow when the bag has one, otherwise the Blessing
	// Scroll. Both are read by DoRefineWithScroll from the cell SetRefineMode
	// names, no blacksmith needed - which is also why the scroll pass runs
	// wherever the bot stands. plusLevel 0 means "any scroll that is here".
	int FindPlayerBotRefineScrollCell(LPCHARACTER ch, BYTE plusLevel)
	{
		if (!ch)
			return -1;
		int blessing = -1, dragonGod = -1;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (!scroll)
				continue;
			const DWORD vnum = scroll->GetVnum();
			if (vnum == PLAYERBOT_BLESSING_SCROLL_VNUM && blessing < 0)
				blessing = cell;
			for (size_t i = 0; i < sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS) / sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[0]); ++i)
				if (vnum == PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[i] && dragonGod < 0)
					dragonGod = cell;
		}
		if (dragonGod >= 0 && (plusLevel >= PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS || blessing < 0))
			return dragonGod;
		return blessing;
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
			// What the merchant would take on the next town visit is not
			// worth a refine now: Ametystowy Naszyjnik+0 was raised to +1 at
			// 17:58 and sold for scrap at 18:19. A spare that is kept - an
			// upgrade, a higher tier than the worn piece, a reserve at +6 -
			// is worth raising; the rest is scrap and stays at what it is.
			if (IsPlayerBotJunkItem(ch, item))
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

			// What comes off the anvil must still fit. See IsPlayerBotWearableAtLevel
			// for why the engine will not stop this on its own.
			if (!IsPlayerBotWearableAtLevel(ch, nextVnum))
			{
				PlayerBotLogThrottled("refine_outgrows", dwNow,
						"PLAYERBOT_AI: refine would outgrow the bot pid=%u name=%s level=%u vnum=%u next=%u plus=%u",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
						oldVnum, nextVnum, (unsigned int)plusLevel + 1);
				continue;
			}
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
			// With a Blessing Scroll in the bag and a level worth protecting, go
			// the scroll's way: the engine reads the scroll from the cell set by
			// SetRefineMode, spends it, and on failure hands back the item one
			// level down rather than nothing.
			int scrollCell = -1;
			if (plusLevel >= PLAYERBOT_SCROLL_REFINE_MIN_PLUS)
				scrollCell = FindPlayerBotRefineScrollCell(ch, plusLevel);
			// No scroll, a roll that can fail, and a weapon worth more than the
			// next plus: leave it. The blacksmith burns what he fails.
			// A level-30 weapon from +6 on goes only under a scroll, prize lines
			// or not - a burnt Full Moon Sword +7 is a week of somebody's
			// hunting, and the Moonlight chests keep the scrolls coming.
			if (scrollCell < 0 && (IsPlayerBotPrizeItem(item) ||
					(IsPlayerBotSpecialLevel30Weapon(item) && plusLevel >= PLAYERBOT_SCROLL_REFINE_MIN_PLUS)))
			{
				const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
				if (prt && prt->prob < 100)
				{
					PlayerBotLogThrottled("refine_prize_no_scroll", dwNow,
							"PLAYERBOT_AI: refine held, prize line and no blessing scroll pid=%u name=%s vnum=%u plus=%u prob=%d",
							ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, prt->prob);
					continue;
				}
			}
			bool attempted = false;
			if (scrollCell >= 0)
			{
				ch->SetRefineMode(scrollCell);
				attempted = ch->DoRefineWithScroll(item);
				ch->ClearRefineMode();
			}
			else
				attempted = ch->DoRefine(item, false);
			if (attempted)
			{
				const bool success = ch->CountSpecifyItem(nextVnum) > resultCountBefore;
				BroadcastPlayerBotRefineSuccess(ch, item, (int)plusLevel + 1);
				sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=%d",
						success ? "SUCCESS" : (scrollCell >= 0 ? "FAILED_DOWNGRADED" : "FAILED_BURNED"),
						ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, plusLevel + 1, scrollCell >= 0 ? 1 : 0);
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

	// The Blessing Scroll works from the bag, wherever the bot stands - a
	// player uses one in the field, not at the anvil - so a bot carrying one
	// does not wait for its next town visit to put it to use. In a quiet
	// moment it takes the lowest worn piece at +6 or better, pays the table's
	// fee and materials, and refines it under the scroll: on failure the piece
	// comes back one level down instead of not at all.
	bool ManagePlayerBotScrollRefine(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextScrollRefineTime)
			return false;
		if (state.bCurrentAction == BOT_ACTION_FIGHT || state.bVisitingShop ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat || ch->IsDead())
			return false;
		state.dwNextScrollRefineTime = dwNow + PLAYERBOT_SCROLL_REFINE_INTERVAL;

		int scrollCell = FindPlayerBotRefineScrollCell(ch, 0);
		if (scrollCell < 0)
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		LPITEM best = NULL;
		BYTE bestWear = 0;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0 || item->isLocked() || item->IsExchanging())
				continue;
			const BYTE plus = item->GetRefineLevel();
			if (plus < PLAYERBOT_SCROLL_REFINE_MIN_PLUS || plus >= GetPlayerBotRefineTarget(ch, item))
				continue;
			if (!IsPlayerBotWearableAtLevel(ch, item->GetRefinedVnum()))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			if (ch->GetGold() - GetPlayerBotReservedGold(ch) < (int)recipe->cost)
				continue;
			bool materials = true;
			for (int m = 0; m < recipe->material_count && materials; ++m)
				if (recipe->materials[m].vnum != 0 &&
						ch->CountSpecifyItem(recipe->materials[m].vnum) < recipe->materials[m].count)
					materials = false;
			if (!materials)
				continue;
			if (!best || plus < best->GetRefineLevel())
			{
				best = item;
				bestWear = wearSlots[i];
			}
		}
		if (!best)
			return false;

		// Off, refined, and back on: the engine will not touch a worn piece, and
		// the result is a new item in the same cell whatever the outcome.
		if (ch->GetEmptyInventory(best->GetSize()) < 0)
			return false;
		const DWORD oldVnum = best->GetVnum();
		const DWORD nextVnum = best->GetRefinedVnum();
		const BYTE plus = best->GetRefineLevel();
		if (!ch->UnequipItem(best) || best->IsEquipped())
			return false;
		const WORD cell = best->GetCell();
		const int before = ch->CountSpecifyItem(nextVnum);
		ch->SetRefineMode(scrollCell);
		const bool attempted = ch->DoRefineWithScroll(best);
		ch->ClearRefineMode();
		LPITEM after = ch->GetInventoryItem(cell);
		if (after)
			ch->EquipItem(after);
		if (attempted)
		{
			const bool success = ch->CountSpecifyItem(nextVnum) > before;
			if (success)
				BroadcastPlayerBotRefineSuccess(ch, after ? after : best, (int)plus + 1);
			sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=1 place=field wear=%u",
					success ? "SUCCESS" : "FAILED_DOWNGRADED", ch->GetPlayerID(), ch->GetName(),
					oldVnum, nextVnum, (unsigned int)plus + 1, (unsigned int)bestWear);
		}
		return attempted;
	}

	bool ManagePlayerBotMiscMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		CompactPlayerBotPotionStacks(ch);
		SellPlayerBotExcessPotions(ch);

		DWORD redCount = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_HP);
		DWORD blueCount = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_SP);

		// Miscellaneous loot belongs to Handlarka. Weapons and wearable equipment
		// are deliberately left for their own specialist merchants.
		SellPlayerBotJunkAtMerchant(ch, BOT_MERCHANT_MISC, "misc_merchant");

		// Economical potion purchase at Handlarka.  Refining is intentionally
		// performed in the separate blacksmith phase after the bot walks there.
		const bool isMage = (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		const BYTE botLvl = ch->GetLevel();
		// What this visit actually bought. The purchase used to be silent - the
		// only line was "misc merchant visit" - so "do the bots really buy the
		// big stock?" could only be answered by querying the database, which is
		// not a question an operator should have to take that far.
		DWORD boughtRed = 0, boughtBlue = 0;

		if (botLvl <= 10)
		{
			if (redCount < 30 && BuyPlayerBotFromNearbyNpcShop(ch, 27001, "potion"))
			{
				const DWORD now = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_HP);
				boughtRed += now - redCount;
				redCount = now;
			}
			if (isMage && blueCount < 20 && BuyPlayerBotFromNearbyNpcShop(ch, 27004, "potion"))
			{
				const DWORD now = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_SP);
				boughtBlue += now - blueCount;
				blueCount = now;
			}
		}
		else
		{
			const DWORD RED_TARGET = 800;
			const DWORD BLUE_TARGET = 600;
			// From forty the bot buys the big (D) potions, not the medium (S).
			// One shop row at a time, the largest stack the counter holds and
			// the purse can pay. AutoGiveItem used to mint hundreds onto the
			// floor of a full bag; CShop::Buy refuses that.
			const bool bBig = botLvl >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
			const DWORD RED_VNUM = bBig ? 27003 : 27002;
			const DWORD BLUE_VNUM = bBig ? 27006 : 27005;
			for (int n = 0; n < 8 && redCount < RED_TARGET && ch->GetGold() >= 1200; ++n)
			{
				if (!BuyPlayerBotFromNearbyNpcShop(ch, RED_VNUM, "potion"))
					break;
				const DWORD now = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_HP);
				if (now <= redCount)
					break;
				boughtRed += now - redCount;
				redCount = now;
			}
			for (int n = 0; n < 8 && blueCount < BLUE_TARGET && ch->GetGold() >= 1200; ++n)
			{
				if (!BuyPlayerBotFromNearbyNpcShop(ch, BLUE_VNUM, "potion"))
					break;
				const DWORD now = CountPlayerBotPotionSupply(ch, PLAYERBOT_POTION_SUPPLY_SP);
				if (now <= blueCount)
					break;
				boughtBlue += now - blueCount;
				blueCount = now;
			}
		}

		if (boughtRed != 0 || boughtBlue != 0)
			sys_log(0, "PLAYERBOT_RESTOCK: bought pid=%u name=%s level=%u red=%u blue=%u "
					"had_red=%u had_blue=%u gold_left=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)boughtRed, (unsigned int)boughtBlue,
					(unsigned int)redCount, (unsigned int)blueCount,
					(int)(ch->GetGold() / 1000));

		// Wooden bracelet, leather shoes, wooden necklace, wooden earrings.
		// That is the whole of Handlarka's accessory counter (shop 3). Asking
		// for the next tier was a miss: those vnums belong to NPC 9008, who
		// is not on the map.
		if (NeedsPlayerBotProgressionWrist(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWristVnum(ch), "wrist");
		if (NeedsPlayerBotProgressionBoots(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionBootsVnum(ch), "boots");
		if (NeedsPlayerBotProgressionNecklace(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionNecklaceVnum(ch), "necklace");
		if (NeedsPlayerBotProgressionEarring(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionEarringVnum(ch), "earring");

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
		// ...and a stone weapon, the dagger of its level, when the bag has none.
		if (isArcher && !FindPlayerBotStoneWeapon(ch, true))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionStoneWeaponVnum(ch), "stone dagger") || bought;
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
		if (!recipe || ch->GetGold() - GetPlayerBotReservedGold(ch) <
				ch->ComputeRefineFee(recipe->cost))
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
