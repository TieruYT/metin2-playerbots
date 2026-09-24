#ifndef __INC_METIN2_PLAYERBOT_HERBALISM_H__
#define __INC_METIN2_PLAYERBOT_HERBALISM_H__

// Herbalism: Baek-Go's crafting board, worked without his window.
//
// The 2.x package ships the whole system and this world has never used a line
// of it. Read off the running server on 17 September: the onboarding quest is
// compiled and hooked to mob 20018 in all three first villages, his special
// shop 14 sells the Herbalist's Knife and the three empty bottles, and
// world.crafting_proto carries 77 rows behind eight levels of recipe knowledge.
// What it produced in game was nothing at all - the recipes that drop from
// Metin stones (29 groups at 12.5-18%) went to the merchant as an unknown
// item, and the herbs went on the counters as bulk goods.
//
// Three engine facts decide the shape of what follows:
//
//   * The board is a CLIENT WINDOW. crafting.open sends `craft_open` down the
//     chat channel and crafting.create refuses anything unless that window
//     reported itself open, so a bot - which has no client - can never press a
//     single button on it. The craft is therefore re-implemented here against
//     CCraftingManager, the way the stable keeper's hand-over and the ore
//     smelting already are. Everything it touches is the quest's own: the same
//     rows, the same odds, the same price, and the same progress flags
//     (`crafting.progress_<recipe>`), so a bot and a player are counted by one
//     ledger and the panel reads both the same way.
//   * A craft SPENDS THE MATERIALS WHETHER IT SUCCEEDS OR NOT - crafting.lua
//     removes them before it rolls - so a 60% row is a real loss and a bot
//     keeps PLAYERBOT_HERBALISM_GOLD_RESERVE rather than grinding its purse.
//   * A potion is ITEM_POTION (type 36), a type of its own on this line, and
//     its use goes through the compiled quest hook `object/36/use_type`, not
//     through any case in char_item.cpp. So a bot drinks one with an ordinary
//     UseItem and the quest does the rest: value0 is the duration, value1 the
//     group (boost 1, offensive 2, defensive 3) and the engine allows 5, 3 and
//     2 affects of those groups at once.
//
// What is deliberately NOT here: picking the plants. The knife wants a growing
// bush to click and this world spawns four of the sixteen - the Alpine Rose in
// the three second villages, the Thistle on Mount Sohan, the Amber Petal and
// the Nettle on the two Trent maps - none of them the Peach Blossom the
// onboarding asks for. That is the mining problem again (no map spawns a vein
// either) and placing plants is a change of its own. The herbs a bot works
// with come from the drop tables, where all of them are: the bags and counters
// of the test world held 2788 of them before a single line of this ran.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_mining.h - it uses the free-cell count and
// the gold reserve - and before playerbot_economy.h, whose junk rule asks it
// what a potion and a recipe are worth keeping.

#if defined(PLAYERBOT_ENGINE_MT2009)
#include "crafting_manager.h"
#endif

namespace
{
#if defined(PLAYERBOT_ENGINE_MT2009)

	// Baek-Go's own rows, read out of the package's crafting_data.lua
	// (CRAFTING_BAEKGO = 102) rather than guessed from the numbering: 58-66 are
	// commented out there as "wszystkie rosy pvp" and are not on his board, so
	// a bot that walked the range would ask for rows the quest refuses.
	const DWORD PLAYERBOT_HERBALISM_ROWS[] = {
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
		28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
		45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
		67, 68, 71, 72, 73, 74, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
		92, 93, 94, 95
	};
	const size_t PLAYERBOT_HERBALISM_ROW_COUNT =
			sizeof(PLAYERBOT_HERBALISM_ROWS) / sizeof(PLAYERBOT_HERBALISM_ROWS[0]);

	// The herbalist's sixteen, by vnum: the materials of his board.
	bool IsPlayerBotHerbalismHerb(DWORD vnum)
	{
		return vnum >= PLAYERBOT_HERB_VNUM_FIRST && vnum <= PLAYERBOT_HERB_VNUM_LAST;
	}

	bool IsPlayerBotCraftedPotion(LPITEM item)
	{
		return item && item->GetType() == ITEM_POTION;
	}

	bool IsPlayerBotCraftRecipeItem(LPITEM item)
	{
		return item && item->GetType() == ITEM_USE &&
				item->GetSubType() == USE_CRAFT_RECIPE;
	}

	// The green and purple potions (Zielona/Fioletowa Mikstura M/S/D, both
	// runs of them) that a counter sells in packs (PLAYERBOT_SHOP_POTION_PACKS).
	bool IsPlayerBotPackedPotion(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_POTION)
			return false;
		const DWORD vnum = item->GetVnum();
		return (vnum >= 27100 && vnum <= 27105) || (vnum >= 27110 && vnum <= 27115);
	}

	// The quest's own ledger: pc.setf("crafting", "progress_<vnum>") in
	// crafting.lua, which is `crafting.progress_<vnum>` to GetQuestFlag.
	int GetPlayerBotCraftProgress(LPCHARACTER ch, DWORD recipeVnum)
	{
		if (!ch || recipeVnum == 0)
			return 0;
		char flag[64];
		snprintf(flag, sizeof(flag), "crafting.progress_%u", recipeVnum);
		return std::max(0, ch->GetQuestFlag(flag));
	}

	void SetPlayerBotCraftProgress(LPCHARACTER ch, DWORD recipeVnum, int value)
	{
		if (!ch || recipeVnum == 0)
			return;
		char flag[64];
		snprintf(flag, sizeof(flag), "crafting.progress_%u", recipeVnum);
		ch->SetQuestFlag(flag, value);
	}

	// herbalism.lua asks for this before it opens the board at all, and before
	// a recipe may be read (herbalism.can_craft). The quest sets it by walking a
	// player through Baek-Go's dialog; a bot has no dialog, so it is given the
	// same thing on the same terms wherever it stands: level fifteen and ten
	// Peach Blossoms in the bag - which drop from 29 monsters on this world -
	// and in return the first recipe and five bottles. The quest only looks at
	// the flowers and never takes them (herbalism_onboarding.quest, the
	// state_progress handler), and neither does this: the first build took
	// them, which was a price no player paid.
	bool IsPlayerBotHerbalismUnlocked(LPCHARACTER ch)
	{
		return ch && ch->GetQuestFlag("herbalism_onboarding.completed") > 0;
	}

	bool EnsurePlayerBotHerbalismStarted(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotHerbalismUnlocked(ch))
			return true;
		if (ch->GetLevel() < PLAYERBOT_HERBALISM_MIN_LEVEL)
			return false;
		const int flowers = (int) ch->CountSpecifyItem(PLAYERBOT_HERBALISM_ONBOARD_FLOWER);
		if (flowers < PLAYERBOT_HERBALISM_ONBOARD_COUNT)
			return false;
		if (CountPlayerBotFreeInventoryCells(ch) < 2)
			return false;
		ch->AutoGiveItem(PLAYERBOT_HERBALISM_FIRST_RECIPE, 1);
		ch->AutoGiveItem(PLAYERBOT_HERBALISM_BOTTLE_M, PLAYERBOT_HERBALISM_ONBOARD_BOTTLES);
		ch->SetQuestFlag("herbalism_onboarding.completed", 1);
		sys_log(0, "PLAYERBOT_HERB: onboarding done pid=%u name=%s level=%u flowers=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int) ch->GetLevel(), flowers);
		return true;
	}

	// The quest's own wait flag, pc.setf("crafting", "learn_delay"..vnum).
	int GetPlayerBotRecipeLearnDelay(LPCHARACTER ch, DWORD recipeVnum)
	{
		char flag[64];
		snprintf(flag, sizeof(flag), "crafting.learn_delay%u", recipeVnum);
		return ch->GetQuestFlag(flag);
	}

	void SetPlayerBotRecipeLearnDelay(LPCHARACTER ch, DWORD recipeVnum, int value)
	{
		char flag[64];
		snprintf(flag, sizeof(flag), "crafting.learn_delay%u", recipeVnum);
		ch->SetQuestFlag(flag, value);
	}

	// Whether a recipe could be read at all, by anybody: crafting.learn_recipe
	// answers false for a row crafting_data does not know, and keeps the item.
	const TCraftingItem* GetPlayerBotRecipeRow(LPITEM item)
	{
		if (!IsPlayerBotCraftRecipeItem(item) || item->GetValue(0) <= 0)
			return NULL;
		const TCraftingItem* row = CCraftingManager::instance().GetCraftingRecipe(item->GetValue(0));
		return row && row->itemVnum != 0 ? row : NULL;
	}

	// One recipe read per call, the way crafting.learn_recipe reads one: the
	// odds and the ceiling are the item's own (value1, value2), the progress is
	// the quest's flag, so a bot's knowledge is a player's knowledge, and the
	// quest's refusals are this one's - no row, the ceiling, the row's level, a
	// wait still running. A read that gets past them costs ONE recipe whether it
	// succeeds or not (the quest's item.remove(1)); the first build removed the
	// whole stack, ten recipes for one roll.
	//
	// The wait is the bots' book wait (GetPlayerBotBookWaitSeconds), not the
	// quest's twenty-one hours: it is the same number the operator set for the
	// bots' skill books, and on the default world it is none. And a Hermit's
	// Advice waits: the quest takes it off at any recipe read without using it,
	// and the book pass puts it on for the class book it is about to read.
	bool ReadPlayerBotCraftRecipe(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotHerbalismUnlocked(ch))
			return false;
		if (ch->FindAffect(AFFECT_SKILL_BOOK_BONUS))
			return false;
		const int wait = GetPlayerBotBookWaitSeconds();
		const int now = (int) get_global_time();
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			const TCraftingItem* row = GetPlayerBotRecipeRow(item);
			if (!row)
				continue;
			const DWORD recipeVnum = item->GetValue(0);
			const int chance = item->GetValue(1);
			const int maxRead = item->GetValue(2);
			if (maxRead <= 0)
				continue;
			const int progress = GetPlayerBotCraftProgress(ch, recipeVnum);
			if (progress >= maxRead)
				continue;   // known to the ceiling: the stack is goods now
			if (ch->GetLevel() < row->reqLevel)
				continue;
			if (wait > 0)
			{
				// A wait written under a longer setting ends where the current
				// one would have ended it, as the book wait does.
				int until = GetPlayerBotRecipeLearnDelay(ch, recipeVnum);
				if (until > now + wait)
				{
					until = now + wait;
					SetPlayerBotRecipeLearnDelay(ch, recipeVnum, until);
				}
				if (until > now)
					continue;
			}
			const int bonus = ch->GetPoint(POINT_LEARN_CHANCE);
			const int rolled = chance * (100 + bonus) / 100;
			// The quest spends a learning potion's affect on any read.
			CAffect* learnPotion = ch->FindAffect(AFFECT_POTION_GENERAL_USE, POINT_LEARN_CHANCE);
			if (learnPotion)
				ch->RemoveAffect(learnPotion);
			const bool learnt = number(1, 100) <= rolled;
			if (learnt)
			{
				SetPlayerBotCraftProgress(ch, recipeVnum, progress + 1);
				if (wait > 0)
					SetPlayerBotRecipeLearnDelay(ch, recipeVnum, now + wait);
			}
			const DWORD vnum = item->GetVnum();
			const int left = (int) item->GetCount() - 1;
			if (left > 0)
				item->SetCount(left);
			else
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_RECIPE");
			sys_log(0, "PLAYERBOT_HERB: recipe read pid=%u name=%s item=%u recipe=%u progress=%d/%d chance=%d left=%d %s",
					ch->GetPlayerID(), ch->GetName(), vnum, recipeVnum,
					learnt ? progress + 1 : progress, maxRead, rolled, left,
					learnt ? "LEARNT" : "FAILED");
			return true;
		}
		return false;
	}

	bool PlayerBotHoldsCraftRecipe(LPCHARACTER ch)
	{
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			if (IsPlayerBotCraftRecipeItem(ch->GetInventoryItem(cell)))
				return true;
		return false;
	}

	// A pass of its own, with its own clock. The first build hung this on the
	// tail of ManagePlayerBotSkillBooks - the branch that runs when no class
	// book is due - and that branch is unreachable for the bots that matter:
	// MordercaBezSerca3 finished the onboarding at 22:28 with the recipe in its
	// bag and 55 skill books beside it, so the book pass always had something
	// better to do and the recipe sat unread for half an hour. A feature wired
	// into somebody else's early return is a feature that never runs.
	//
	// And the second build waited for an onboarding only Baek-Go's visit gave,
	// which only the Zielarz makes - a Conqueror of forty-five under the
	// PERSONA switch. On 24 September m2zip had 1597 recipes in the bags of 822
	// bots and not one bot onboarded ("maja ich pelno w eq a powinny czytac od
	// razu po dropnieciu", Iwakura). A bot holding a recipe is onboarded here,
	// wherever it stands, and then reads one every few seconds until nothing in
	// its bag can teach it more.
	void ManagePlayerBotCraftRecipes(LPCHARACTER ch, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotRecipeNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || ch->GetExchange() ||
				ch->GetMyShop())
			return;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotRecipeNext.find(pid);
		if (it != s_mapPlayerBotRecipeNext.end() && dwNow < it->second)
			return;
		if (!IsPlayerBotHerbalismUnlocked(ch))
		{
			if (!PlayerBotHoldsCraftRecipe(ch) || !EnsurePlayerBotHerbalismStarted(ch))
			{
				s_mapPlayerBotRecipeNext[pid] = dwNow + number(
						PLAYERBOT_HERBALISM_ONBOARD_RETRY_MIN_MS, PLAYERBOT_HERBALISM_ONBOARD_RETRY_MAX_MS);
				return;
			}
		}
		const bool read = ReadPlayerBotCraftRecipe(ch);
		s_mapPlayerBotRecipeNext[pid] = dwNow + (read ?
				number(PLAYERBOT_HERBALISM_RECIPE_READ_GAP_MIN_MS, PLAYERBOT_HERBALISM_RECIPE_READ_GAP_MAX_MS) :
				number(PLAYERBOT_HERBALISM_RECIPE_IDLE_MIN_MS, PLAYERBOT_HERBALISM_RECIPE_IDLE_MAX_MS));
	}

	// The bottles are Baek-Go's shop, which is a quest window like the board.
	// Bought the way EnsurePlayerBotFishingPass buys the fishing pass: the item
	// is created for the price the shop asks, so nothing is had for free.
	bool BuyPlayerBotCraftBottles(LPCHARACTER ch, DWORD vnum, int needed)
	{
		if (!ch || needed <= 0)
			return false;
		long long price = 0;
		if (vnum == PLAYERBOT_HERBALISM_BOTTLE_M)
			price = PLAYERBOT_HERBALISM_BOTTLE_M_PRICE;
		else if (vnum == PLAYERBOT_HERBALISM_BOTTLE_S)
			price = PLAYERBOT_HERBALISM_BOTTLE_S_PRICE;
		else if (vnum == PLAYERBOT_HERBALISM_BOTTLE_D)
			price = PLAYERBOT_HERBALISM_BOTTLE_D_PRICE;
		else
			return false;
		// The shop sells them ten at a time and so does this: a pack at a time
		// until the row is covered, each one paid for.
		int packs = (needed + PLAYERBOT_HERBALISM_BOTTLE_PACK - 1) /
				PLAYERBOT_HERBALISM_BOTTLE_PACK;
		if (packs <= 0)
			return false;
		const long long total = price * packs;
		if ((long long) ch->GetGold() < total + PLAYERBOT_HERBALISM_GOLD_RESERVE)
			return false;
		if (CountPlayerBotFreeInventoryCells(ch) < 1)
			return false;
		ch->PointChange(POINT_GOLD, -total);
		ch->AutoGiveItem(vnum, packs * PLAYERBOT_HERBALISM_BOTTLE_PACK);
		sys_log(0, "PLAYERBOT_HERB: bottles bought pid=%u name=%s vnum=%u packs=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), vnum, packs, total);
		return true;
	}

	bool IsPlayerBotCraftBottle(DWORD vnum)
	{
		return vnum == PLAYERBOT_HERBALISM_BOTTLE_M ||
				vnum == PLAYERBOT_HERBALISM_BOTTLE_S ||
				vnum == PLAYERBOT_HERBALISM_BOTTLE_D;
	}

	// Everything is_crafting_item_available asks, minus the window: the row is
	// on this board, the bot is old enough for it and its knowledge reaches the
	// row's threshold.
	bool PlayerBotKnowsCraftRow(LPCHARACTER ch, const TCraftingItem* row)
	{
		if (!ch || !row)
			return false;
		if (ch->GetLevel() < row->reqLevel)
			return false;
		if (row->reqProgress == 0)
			return true;
		return (DWORD) GetPlayerBotCraftProgress(ch, row->recipeVnum) >= row->reqProgress;
	}

	// Whether the bag covers the row, buying the bottles it is short of. The
	// bottles are the one material a bot can always get, which is why they are
	// the only thing bought here; every herb comes from the world.
	bool PreparePlayerBotCraftMaterials(LPCHARACTER ch, const TCraftingItem* row, bool buy)
	{
		if (!ch || !row)
			return false;
		for (int i = 0; i < CRAFTING_MATERIAL_MAX_NUM; ++i)
		{
			const DWORD vnum = row->materials[i].vnum;
			const int need = (int) row->materials[i].count;
			if (vnum == 0 || need <= 0)
				continue;
			const int have = (int) ch->CountSpecifyItem(vnum);
			if (have >= need)
				continue;
			if (!IsPlayerBotCraftBottle(vnum))
				return false;
			// A bottle is the one material that is always available: Baek-Go
			// sells it at the same counter. When only asked whether the row is
			// possible (buy == false), an empty bottle shelf is not a refusal -
			// otherwise no bot would ever set out for its first craft, because
			// the bottles can only be bought once it has arrived.
			if (!buy)
				continue;
			if (!BuyPlayerBotCraftBottles(ch, vnum, need - have))
				return false;
			if ((int) ch->CountSpecifyItem(vnum) < need)
				return false;
		}
		return true;
	}

	// What a bot makes when it is at the board. The highest row it can afford
	// comes first, because the chain feeds itself - a Water eats ten Juices -
	// so working the top of what a bot knows is what turns a heap of herbs into
	// something worth carrying, and it is also what Iwakura asked for: keep the
	// lower tier for the stronger one rather than drinking it.
	const TCraftingItem* ChoosePlayerBotCraftRow(LPCHARACTER ch)
	{
		if (!ch)
			return NULL;
		const TCraftingItem* best = NULL;
		for (size_t i = 0; i < PLAYERBOT_HERBALISM_ROW_COUNT; ++i)
		{
			const TCraftingItem* row =
					CCraftingManager::instance().GetCraftingRecipe(PLAYERBOT_HERBALISM_ROWS[i]);
			if (!row || row->itemVnum == 0)
				continue;
			if (!PlayerBotKnowsCraftRow(ch, row))
				continue;
			if ((long long) ch->GetGold() < (long long) row->price + PLAYERBOT_HERBALISM_GOLD_RESERVE)
				continue;
			if (!PreparePlayerBotCraftMaterials(ch, row, false))
				continue;
			if (!best || row->reqProgress > best->reqProgress ||
					(row->reqProgress == best->reqProgress && row->price > best->price))
				best = row;
		}
		return best;
	}

	// Is there a row this bot could make right now, if only it were standing at
	// the board? The travel asks this before it spends a trip to a first
	// village, so it is the full list - knowledge, herbs, purse - and not a
	// hope. The bottles are left out on purpose: they are bought at the counter
	// itself, so a row short of only those is still a reason to go.
	// Iwakura's Zielarz: under the PERSONA switch Baek-Go's board is a
	// Conqueror's errand, from level forty-five ("Reakcja Zdobywcy ... odpala
	// mikro-faze Alchemika"). Every bot still picks the herbs up on the way.
	bool IsPlayerBotZielarz(LPCHARACTER ch)
	{
		if (!IsPlayerBotPersonaEnabled())
			return true;
		if (!ch || (int)ch->GetLevel() < PLAYERBOT_ZIELARZ_MIN_LEVEL)
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		return it != s_mapPlayerBotAIStates.end() && it->second.persona.bRestored &&
				it->second.persona.bAdvanced;
	}

	bool PlayerBotHasReadyCraftRow(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || !IsPlayerBotHerbalismUnlocked(ch))
			return false;
		if (ch->GetLevel() < PLAYERBOT_HERBALISM_MIN_LEVEL || !IsPlayerBotZielarz(ch))
			return false;
		if (CountPlayerBotFreeInventoryCells(ch) < PLAYERBOT_HERBALISM_FREE_CELLS)
			return false;
		return ChoosePlayerBotCraftRow(ch) != NULL;
	}

	// The craft itself, in crafting.create's order: materials out, gold out,
	// one roll per unit asked for, the product in. One unit at a time here -
	// the window lets a player ask for several and a bot has no reason to.
	bool CraftPlayerBotPotion(LPCHARACTER ch, const TCraftingItem* row)
	{
		if (!ch || !row || row->itemVnum == 0)
			return false;
		if (CountPlayerBotFreeInventoryCells(ch) < 1)
			return false;
		if (!PreparePlayerBotCraftMaterials(ch, row, true))
			return false;
		for (int i = 0; i < CRAFTING_MATERIAL_MAX_NUM; ++i)
		{
			if (row->materials[i].vnum == 0 || row->materials[i].count <= 0)
				continue;
			ch->RemoveSpecifyItem(row->materials[i].vnum, row->materials[i].count);
		}
		if (row->price > 0)
			ch->PointChange(POINT_GOLD, -(long long) row->price);
		const bool made = number(1, 100) <= row->chance;
		if (made)
			ch->AutoGiveItem(row->itemVnum, row->count);
		sys_log(0, "PLAYERBOT_HERB: craft pid=%u name=%s row=%u item=%u count=%d chance=%d price=%lld %s",
				ch->GetPlayerID(), ch->GetName(), row->vnum, row->itemVnum,
				made ? (int) row->count : 0, row->chance, (long long) row->price,
				made ? "OK" : "FAILED");
		return true;
	}

	// How many of a potion a bot keeps for itself; the rest is what a counter
	// can carry, because until now no player could buy one anywhere.
	bool IsPlayerBotSurplusPotion(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !IsPlayerBotCraftedPotion(item))
			return false;
		const int spare = (int) ch->CountSpecifyItem(item->GetVnum()) - PLAYERBOT_HERBALISM_POTION_KEEP;
		// A packed potion is goods once the spare fills the smallest pack.
		if (IsPlayerBotPackedPotion(item))
			return GetPlayerBotPotionPackUnits(spare) > 0;
		return spare > 0;
	}

	// Drinking. A crafted potion is ten minutes of something a bot cannot get
	// any other way - 60 attack value, 12% critical, 90 defence - and the bag
	// holds a handful, so it is spent where ten minutes are worth spending: a
	// boss, a Metin stone, a Demon Tower floor. Never on the ordinary monster a
	// bot kills every four seconds, which is what would drain a bag in an hour.
	//
	// The engine counts the affects per group (potion_system.lua: 5 boosts, 3
	// offensive, 2 defensive) and the quest refuses a sixth, so this asks
	// whether the group's affect is already running rather than counting: one
	// of each kind up is what a bot can reliably keep, and a refused use would
	// be a wasted potion.
	//
	// The target is passed in rather than read from GetVictim(): the tick knows
	// what the bot is fighting (`curTarget`) long before the engine's victim is
	// set, and the first build asked the engine - 87 Metin lines and not one
	// potion in the two minutes after it went live.
	// A fight with a player is one too, when the Anti-PK protocol is the one
	// asking (playerbot_anti_pk.h): "jesli posiada w ekwipunku Rosy/Wody ...
	// odpala je, by zwiekszyc swoje szanse".
	bool DrinkPlayerBotCraftedPotion(LPCHARACTER ch, LPCHARACTER target, DWORD dwNow, bool pvp = false)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotPotionNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return false;
		if (!target || target->IsDead())
			return false;
		const bool worthIt = target->IsStone() ||
				(target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS) ||
				(pvp && target->IsPC());
		if (!worthIt)
			return false;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotPotionNext.find(pid);
		if (it != s_mapPlayerBotPotionNext.end() && dwNow < it->second)
			return false;
		s_mapPlayerBotPotionNext[pid] = dwNow + PLAYERBOT_HERBALISM_DRINK_RETRY_MS;

		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotCraftedPotion(item))
				continue;
			if (ch->GetLevel() < item->GetLevelLimit())
				continue;
			// value1 is the group; the affect the quest adds for it is
			// AFFECT_POTION_START + group. Only the three that are a fight's
			// worth of buff: 1 boost, 2 offensive, 3 defensive. Group 0 is the
			// general-use shelf and group 4 is FOOD - the grilled fish an angler
			// brings home, which the first run drank at a Metin stone
			// (xTurbina, 22:23, vnum 27866) because the type is shared. Food is
			// for a bot that is hurt, not for a stone.
			const int group = item->GetValue(1);
			if (group < 1 || group > 3)
				continue;
			if (ch->FindAffect(AFFECT_POTION_START + group))
				continue;   // that kind is already running
			const DWORD vnum = item->GetVnum();
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;
			sys_log(0, "PLAYERBOT_HERB: potion drunk pid=%u name=%s vnum=%u group=%d against=%s",
					pid, ch->GetName(), vnum, group,
					target->IsStone() ? "stone" : (target->IsPC() ? "player" : "boss"));
			return true;
		}
		return false;
	}

	// A recipe is goods only once the bot can learn nothing more from it: the
	// ceiling is the item's own value2, and up to it every copy is knowledge
	// the bot still wants. Listing one it could read would be the bookshelf
	// mistake again - selling the thing that makes the trade possible.
	bool IsPlayerBotSurplusRecipe(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !IsPlayerBotCraftRecipeItem(item))
			return false;
		const DWORD recipeVnum = item->GetValue(0);
		const int maxRead = item->GetValue(2);
		// A recipe nothing can be learnt from: no reads, or a row the board
		// does not have (Mikstura Nietykalnosci, 50931, names row 70).
		if (recipeVnum == 0 || maxRead <= 0 || !GetPlayerBotRecipeRow(item))
			return true;
		return GetPlayerBotCraftProgress(ch, recipeVnum) >= maxRead;
	}

#else   // r40250 has no crafting board, so none of this exists there.

	bool IsPlayerBotHerbalismHerb(DWORD) { return false; }
	bool IsPlayerBotCraftedPotion(LPITEM) { return false; }
	bool IsPlayerBotCraftRecipeItem(LPITEM) { return false; }
	bool IsPlayerBotPackedPotion(LPITEM) { return false; }
	bool IsPlayerBotHerbalismUnlocked(LPCHARACTER) { return false; }
	bool IsPlayerBotSurplusPotion(LPCHARACTER, LPITEM) { return false; }
	bool IsPlayerBotSurplusRecipe(LPCHARACTER, LPITEM) { return false; }
	bool ReadPlayerBotCraftRecipe(LPCHARACTER) { return false; }
	bool PlayerBotHasReadyCraftRow(LPCHARACTER) { return false; }
	void ManagePlayerBotCraftRecipes(LPCHARACTER, DWORD) { }
	bool DrinkPlayerBotCraftedPotion(LPCHARACTER, LPCHARACTER, DWORD, bool = false) { return false; }

#endif
}

#endif
