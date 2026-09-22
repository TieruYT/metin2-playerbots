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
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, DWORD resultVnum, int newPlus);
	// Defined beside HasPlayerBotRefineOpportunity; the blacksmith pass asks it
	// before taking a worn piece off for the anvil.
	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item);

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

	// The trader's book purse (community patch 2, point 5): what is left of
	// PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT of the yang its window began with.
	// A window the town visit did not open (HandlePlayerBotTownVisit) opens on
	// the first question and lasts PLAYERBOT_BOOK_BUDGET_WINDOW_MS.
	long long GetPlayerBotBookBudgetLeft(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return 0;
		TPlayerBotPersona& p = st->second.persona;
		const DWORD now = get_dword_time();
		if (p.dwBookBudgetSince == 0 || now - p.dwBookBudgetSince >= PLAYERBOT_BOOK_BUDGET_WINDOW_MS)
		{
			p.llBookBudgetBase = (long long)ch->GetGold();
			p.llBookBudgetSpent = 0;
			p.dwBookBudgetSince = now;
		}
		return std::max(0LL, p.llBookBudgetBase * PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT / 100 -
				p.llBookBudgetSpent);
	}

	void NotePlayerBotBookBought(LPCHARACTER ch, long long price)
	{
		if (!ch || price <= 0)
			return;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st != s_mapPlayerBotAIStates.end())
			st->second.persona.llBookBudgetSpent += price;
	}

	// A bot in town as the Trader: the personality says so, or the town visit
	// does (the blacksmith's part of a visit is the Perfectionist's).
	bool PlayerBotBuysBooksAsTrader(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return false;
		return (st->second.persona.bPersona == playerbot_persona::PERSONA_HANDLARZ ||
				st->second.bVisitingShop) && GetPlayerBotBookBudgetLeft(ch) > 0;
	}

	// Iwakura's list (playerbot_lpp.h, later): a piece on it the list does not
	// keep, which is counter goods.
	bool IsPlayerBotLppSurplusGoods(LPCHARACTER ch, LPITEM item);

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

	// A weapon PLAYERBOT_JUNK_WEAPON_MARKET_CAP caps (community patch 2,
	// point 13). One rolled with prize lines is not low quality: a Krwawy
	// Miecz +0 with a forty percent average is what players cross a market
	// for, and the cap was written against the ones nobody buys.
	bool IsPlayerBotCappedJunkWeapon(LPITEM item)
	{
		return item && item->GetType() == ITEM_WEAPON && IsPlayerBotJunkWeaponVnum(item->GetVnum()) &&
				!IsPlayerBotPrizeItem(item);
	}

	// Every skill book in the bag, whatever the skill.
	int CountPlayerBotSkillBooks(LPCHARACTER ch)
	{
		int books = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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

	// The stack an item can grow to. mt2009 keeps one per proto (dwMaxStack:
	// twenty for a Medal Konny or a Blessing Scroll, a thousand for arrows,
	// two hundred for most things); r40250 has the one ITEM_MAX_COUNT. Every
	// count the fragments compared with PLAYERBOT_STACK_MAX read a full stack
	// of twenty medals as room for a hundred and eighty more: MoveItem into a
	// full stack moves nothing and still answers true, so ten stacks of twenty
	// were "merged" four at a time every five seconds for ever - 110 bots and
	// 14 321 lines in ten minutes on the test world, every medal dropper among
	// them (16 September). SetCount clamps to the same limit, silently, which
	// is what the safebox top-up below has to know before it removes the bag
	// stack it thinks it poured in.
	int PlayerBotMaxStack(LPITEM item)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return item ? (int)item->GetMaxStack() : PLAYERBOT_STACK_MAX;
#else
		(void)item;
		return PLAYERBOT_STACK_MAX;
#endif
	}

	// Pour split stacks together, a few at a time. MoveItem with a count of
	// zero moves as much of the source as the destination has room for and
	// removes the source when it is emptied - the same thing a player's drag
	// does, packets and item log included. A merge is counted only when the
	// destination grew: a MoveItem that moved nothing is not work done.
	int MergePlayerBotStacks(LPCHARACTER ch, int maxMerges)
	{
		int merged = 0;
		for (WORD i = 0; i < PLAYERBOT_BAG_CELLS && merged < maxMerges; ++i)
		{
			LPITEM item = ch->GetInventoryItem(i);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const int maxStack = PlayerBotMaxStack(item);
			if ((int)item->GetCount() >= maxStack)
				continue;
			for (WORD j = i + 1; j < PLAYERBOT_BAG_CELLS && merged < maxMerges; ++j)
			{
				LPITEM other = ch->GetInventoryItem(j);
				if (!other || other->IsEquipped() || other->isLocked() ||
						!PlayerBotStacksTogether(item, other))
					continue;
				const DWORD before = item->GetCount();
				if (ch->MoveItem(TItemPos(INVENTORY, j), TItemPos(INVENTORY, i), 0) &&
						item->GetCount() > before)
					++merged;
				if ((int)item->GetCount() >= maxStack)
					break;
			}
		}
		return merged;
	}

#if !defined(PLAYERBOT_ENGINE_MT2009)
	// The order the bag is tidied into: the red and blue potions, then every
	// other potion - green and purple, the timed boosters, the auto potions -
	// then chests and keys ("wszelakie potki pierwsze a potem reszte", Tieru,
	// 15 September). Everything else is 99 and stays behind them.
	int GetPlayerBotSortPriority(LPITEM item)
	{
		if (!item)
			return 99;
		const DWORD vnum = item->GetVnum();
		const BYTE type = item->GetType();
		if (type == ITEM_USE)
		{
			const BYTE sub = item->GetSubType();
			if (sub == USE_POTION || sub == USE_POTION_NODELAY)
				return 0;   // red/blue/big HP-SP potions
			if (sub == USE_POTION_CONTINUE || IsPlayerBotBoosterItem(item) ||
					GetPlayerBotAutoPotionAffect(vnum) != 0)
				return 1;   // green/purple, Hand of Critic/Penetration, Swiftness, the elixirs
		}
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (type == ITEM_POTION)
			return 1;   // the green and purple potions are a type of their own here
#endif
		if ((vnum >= 27100 && vnum <= 27105) || vnum == 27053 || vnum == 27054)
			return 1;
		if (vnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM || type == ITEM_TREASURE_BOX ||
				type == ITEM_TREASURE_KEY || type == ITEM_GIFTBOX)
			return 2;   // Moonlight chests, silver/gold chests and keys, boss caskets
		return 99;
	}

	// Potions to the front, chests and keys behind them, the rest after
	// (GetPlayerBotSortPriority). A selection sort over the cells a single-cell
	// item can stand in: the best item not yet in place is swapped with the one
	// in its way through a free cell - three MoveItems, each into an empty cell,
	// so nothing is merged, overwritten or lost, and a move the engine refuses
	// leaves an item where it was or in a free cell of the same bag. Gear of two
	// and three cells never moves, and nothing moves into the cells it covers.
	// The first version only moved a potion into an empty cell before it, and a
	// bag full from the front has none: the operator's screenshots showed
	// potions scattered through two pages of full bags.
	void SortPlayerBotConsumablesToFront(LPCHARACTER ch)
	{
		if (!ch)
			return;
		const long kRest = 99L * 1000000L;
		long key[PLAYERBOT_BAG_CELLS];
		bool empty[PLAYERBOT_BAG_CELLS];
		bool anyFront = false;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			key[cell] = -1;   // a cell the sort does not touch
			empty[cell] = false;
			LPITEM item = ch->GetInventoryItem(cell);
			if (item)
			{
				if (item->GetCell() != cell || item->GetSize() != 1 || item->IsEquipped() ||
						item->isLocked() || item->IsExchanging())
					continue;
				const int prio = GetPlayerBotSortPriority(item);
				key[cell] = prio >= 99 ? kRest : (long)prio * 1000000L + (long)(item->GetVnum() % 1000000U);
				anyFront = anyFront || prio < 99;
			}
			else if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, (WORD)cell), 1))
			{
				key[cell] = kRest;
				empty[cell] = true;
			}
		}
		if (!anyFront)
			return;
		int moves = 0;
		for (int i = 0; i < PLAYERBOT_BAG_CELLS && moves + 3 <= PLAYERBOT_SORT_MAX_MOVES; ++i)
		{
			if (key[i] < 0)
				continue;
			int best = -1;
			for (int j = i + 1; j < PLAYERBOT_BAG_CELLS; ++j)
				if (key[j] >= 0 && !empty[j] && key[j] < key[i] && (best < 0 || key[j] < key[best]))
					best = j;
			if (best < 0)
				continue;
			if (empty[i])
			{
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)best), TItemPos(INVENTORY, (WORD)i), 0))
					break;
				++moves;
			}
			else
			{
				int buffer = -1;
				for (int k = PLAYERBOT_BAG_CELLS - 1; k >= 0; --k)
					if (empty[k] && k != i && k != best)
					{
						buffer = k;
						break;
					}
				if (buffer < 0)
					break;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)best), TItemPos(INVENTORY, (WORD)buffer), 0))
					break;
				++moves;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)i), TItemPos(INVENTORY, (WORD)best), 0))
					break;
				++moves;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)buffer), TItemPos(INVENTORY, (WORD)i), 0))
					break;
				++moves;
			}
			std::swap(key[i], key[best]);
			std::swap(empty[i], empty[best]);
		}
		if (moves > 0)
			sys_log(0, "PLAYERBOT_BAG: sorted pid=%u name=%s moves=%d",
					ch->GetPlayerID(), ch->GetName(), moves);
	}
#else
	// The whole bag, as a player's "Scal i uporzadkuj" lays it out
	// (playerbot_arrange.cpp) - which keeps the potions-first order the
	// sort above gave the r40250 bags - on its own clock, by pid, so a
	// restart does not arrange the whole population in one minute.
	std::map<DWORD, DWORD> s_mapPlayerBotArrangeNext;

	void ManagePlayerBotArrange(LPCHARACTER ch, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		const DWORD spread = PlayerBotNavHash(pid ^ 0x41524e47U);
		std::map<DWORD, DWORD>::iterator next = s_mapPlayerBotArrangeNext.find(pid);
		if (next == s_mapPlayerBotArrangeNext.end())
		{
			s_mapPlayerBotArrangeNext[pid] = dwNow + spread % PLAYERBOT_ARRANGE_INTERVAL;
			return;
		}
		if (dwNow < next->second)
			return;
		next->second = dwNow + PLAYERBOT_ARRANGE_INTERVAL + spread % PLAYERBOT_ARRANGE_SPREAD;
		const playerbot_arrange::TResult result = playerbot_arrange::ArrangeInventory(ch, false);
		if (result.code == playerbot_arrange::RESULT_DONE)
			sys_log(0, "PLAYERBOT_BAG: arranged pid=%u name=%s items=%d moved=%d merged=%d units=%u pinned=%d strategy=%d us=%u",
					pid, ch->GetName(), result.items, result.moved, result.merged, result.units,
					result.pinned, result.strategy, result.micros);
		else if (result.code == playerbot_arrange::RESULT_BUSY)
			next->second = dwNow + PLAYERBOT_ARRANGE_BUSY_RETRY;
	}
#endif

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
		// Then tidy: potions, boosters and chests to the front.
#if defined(PLAYERBOT_ENGINE_MT2009)
		ManagePlayerBotArrange(ch, dwNow);
#else
		SortPlayerBotConsumablesToFront(ch);
#endif
		// A pass that used its whole budget has more to do: back soon, not in
		// five minutes - a closed counter leaves eight packs of one material.
		if (merged >= PLAYERBOT_STACK_MERGES_PER_PASS)
			state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_AFTER_SHOP_MS;
	}

	// Goods priced by the heap: Iwakura's sheet at
	// PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE or less before the yang rate, and of
	// a kind nobody buys one of - a material, a resource, an ore. The sheet's
	// own number rather than the asking price, which moves with the market,
	// the jitter and the markdown, so a line's size does not change between
	// two visits. No refine material is priced this low; a potion, a stone
	// or a key has a line of its own below whatever it costs.
	bool IsPlayerBotBulkGoods(LPITEM item)
	{
		if (!item || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		const BYTE type = item->GetType();
		if (type != ITEM_MATERIAL && type != ITEM_RESOURCE && type != ITEM_SPECIAL)
			return false;
		const DWORD vnum = item->GetVnum();
		for (size_t i = 0; i < sizeof(PLAYERBOT_MATERIAL_PRICES) / sizeof(PLAYERBOT_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_MATERIAL_PRICES[i].dwVnum == vnum)
				return false;
		for (size_t i = 0; i < sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES) / sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwVnum == vnum)
				return PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice != 0 &&
						PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice <= PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE;
		return false;
	}

	// The part of Iwakura's sheet no rule of its own looks after: items he
	// prices by name above the heap line that are ITEM_USE of the special or
	// the detachment kind (the horse and polymorph books, the stone scroll,
	// the Cape of Courage, the rose) or a material or special no recipe
	// consumes. They reached the junk rule's default and the merchant paid a
	// few hundred yang for what the sheet prices at forty to a hundred and
	// thirty-five thousand. The rest of the sheet keeps the rules it has -
	// rings, gloves and uniques (the unique slots), bonus stones (never on a
	// counter), refine scrolls, recipes, chests and keys, the shell and the
	// pearls - because a price is no reason to overrule them. Built once
	// from the two tables: the junk rule runs for every cell of every scan.
	bool IsPlayerBotSheetGoods(LPITEM item)
	{
		static std::set<DWORD> s_goods;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			for (size_t i = 0; i < sizeof(PLAYERBOT_MATERIAL_PRICES) / sizeof(PLAYERBOT_MATERIAL_PRICES[0]); ++i)
				if (PLAYERBOT_MATERIAL_PRICES[i].dwPrice > PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE)
					s_goods.insert(PLAYERBOT_MATERIAL_PRICES[i].dwVnum);
			for (size_t i = 0; i < sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES) / sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES[0]); ++i)
				if (PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice > PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE)
					s_goods.insert(PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwVnum);
		}
		if (!item || s_goods.find(item->GetVnum()) == s_goods.end())
			return false;
		const DWORD vnum = item->GetVnum();
		if (vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;
		const BYTE type = item->GetType();
		if (type == ITEM_USE)
			return item->GetSubType() == USE_SPECIAL || item->GetSubType() == USE_DETACHMENT;
		return type == ITEM_MATERIAL || type == ITEM_SPECIAL;
	}

	// How many units of a stackable go on one counter line. A private shop
	// sells a line whole, so a stack of twenty scrolls on one line is twenty
	// scrolls or nothing: what a player buys one at a time - potions,
	// stones, the shell and the pearls - is a single; a safe refine scroll
	// is a line of PLAYERBOT_SHOP_SCROLL_LINE_UNITS (one to five, the
	// Discord's number); a material is a pack of PLAYERBOT_SHOP_PACK_UNITS,
	// small enough to buy for one refine and few enough lines to leave room
	// on the counter. Zero for anything that does not stack.
	// The three stones a bot rerolls gear with: the change stone, the add
	// stone and the blessing marble, by their subtype rather than by vnum -
	// each comes in an ordinary and an ItemShop flavour (the 76xxx copies),
	// and the green pair for gear of forty and under is a fourth and fifth.
	bool IsPlayerBotBonusStoneItem(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_USE)
			return false;
		const BYTE sub = item->GetSubType();
		return sub == USE_ADD_ATTRIBUTE || sub == USE_CHANGE_ATTRIBUTE ||
				sub == USE_ADD_ATTRIBUTE2;
	}

	int GetPlayerBotStallLineUnits(LPITEM item)
	{
		if (!item || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return 0;
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return PLAYERBOT_SHOP_SCROLL_LINE_UNITS;
		if (item->GetType() == ITEM_SKILLBOOK || item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return 1;
		// A bean is bought a handful at a time (PLAYERBOT_ZEN_BEAN_LINE_UNITS).
		if (item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM)
			return PLAYERBOT_ZEN_BEAN_LINE_UNITS;
		// A bonus stone is bought a few at a time, and a bot that found more
		// than it can spend has hundreds: one a line would take a day and a
		// half to shift a single bag of them.
		if (IsPlayerBotBonusStoneItem(item))
			return PLAYERBOT_SHOP_PACK_UNITS;
		if (item->GetType() == ITEM_USE || item->GetType() == ITEM_METIN ||
				item->GetType() == ITEM_TREASURE_KEY ||
				(item->GetVnum() >= 27992 && item->GetVnum() <= 27994))
			return 1;
		if (item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return PLAYERBOT_CHEST_LINE_UNITS;
		// A root worth pennies is sold by the heap (IsPlayerBotBulkGoods).
		if (IsPlayerBotBulkGoods(item))
			return PLAYERBOT_SHOP_BULK_PACK_UNITS;
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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

	// How many units of a material this bot keeps back for its own anvil:
	// twice the largest recipe count among the pieces it would raise - the
	// same measure "short" uses above. The counter lists only what is over
	// it. Without it a bot short by one bought a pack of two, was no longer
	// short, and put both on its own counter at the price it had just paid
	// (Zolc Niedzwiedzia x2, sizowski) - then was short again.
	int GetPlayerBotRefineMaterialReserve(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch || materialVnum == 0)
			return 0;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}
		int reserve = 0;
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
				if (recipe->materials[m].vnum == materialVnum)
					reserve = std::max(reserve, (int)recipe->materials[m].count * 2);
		}
		return reserve;
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
			// Every recipe an item names (wRefineSet), whatever its id, besides
			// the fixed range. The walk used to stop at PLAYERBOT_REFINE_RECIPE_MAX_ID
			// while mt2009's refine_proto runs to 1362, so the thirteen materials
			// only its upper recipes consume were junk to every bot - the Red
			// Seed (30354, recipes 1089-1338) among them, 496 sold to the
			// merchant in one day on the test world (18 September).
			std::set<DWORD> recipeIds;
			for (DWORD id = 1; id <= PLAYERBOT_REFINE_RECIPE_MAX_ID; ++id)
				recipeIds.insert(id);
			const std::vector<TItemTable>& protos = ITEM_MANAGER::instance().GetTable();
			for (size_t i = 0; i < protos.size(); ++i)
				if (protos[i].wRefineSet != 0)
					recipeIds.insert(protos[i].wRefineSet);
			for (std::set<DWORD>::const_iterator id = recipeIds.begin(); id != recipeIds.end(); ++id)
			{
				const TRefineTable* recipe =
						CRefineManager::instance().GetRefineRecipe(*id);
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

	// The materials no weapon or armour recipe asks for. Every one of them in
	// this world feeds only the Herbalist's Knife (item type 35): the herbs of
	// 50721-50736 and the two brews 30341 and 30342, measured on refine_proto
	// against item_proto. No bot carries that knife, so to a bot they are not
	// materials at all - and the rules that keep a material for the anvil, the
	// counter and the storekeeper kept these in 850 bags and 400 safeboxes of
	// our own world ("skladniki na mikstury, ktorych boty nie craftuja,
	// blokuja eq", uxietoszef). They are merchant scrap. Asked of the tables
	// rather than listed, so a world whose recipes differ gets its own answer.
	bool IsPlayerBotNonGearMaterial(DWORD vnum)
	{
		static std::set<DWORD> s_nonGear;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			std::set<DWORD> gear, other;
			const std::vector<TItemTable>& protos = ITEM_MANAGER::instance().GetTable();
			for (size_t i = 0; i < protos.size(); ++i)
			{
				if (protos[i].wRefineSet == 0)
					continue;
				const TRefineTable* recipe =
						CRefineManager::instance().GetRefineRecipe(protos[i].wRefineSet);
				if (!recipe)
					continue;
				const bool forGear = protos[i].bType == ITEM_WEAPON || protos[i].bType == ITEM_ARMOR;
				for (int m = 0; m < recipe->material_count; ++m)
					if (recipe->materials[m].vnum != 0)
						(forGear ? gear : other).insert(recipe->materials[m].vnum);
			}
			for (std::set<DWORD>::const_iterator it = other.begin(); it != other.end(); ++it)
			{
				// ITEM_MATERIAL only. The walk also finds items of other types that
				// feed nothing but such recipes - 29 of the 47 it found on the first
				// live run: the eleven ores of the pickaxe, the seventeen fish and the
				// shrimp of the rod. Those have rules of their own (the ore trade, the
				// grill) that this one must not overrule.
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
				if (gear.find(*it) == gear.end() && proto && proto->bType == ITEM_MATERIAL)
					s_nonGear.insert(*it);
			}
			sys_log(0, "PLAYERBOT_ECONOMY: %u ITEM_MATERIAL items feed no weapon or armour recipe",
					(unsigned int)s_nonGear.size());
		}
		return s_nonGear.find(vnum) != s_nonGear.end();
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
		return materials.find(item->GetVnum()) != materials.end() &&
				!IsPlayerBotNonGearMaterial(item->GetVnum());
	}

	// A hoard of a refine material: PLAYERBOT_SHOP_HOARD_MIN_UNITS or more over
	// what the bot's own anvil keeps back. It goes on a counter in packs of
	// PLAYERBOT_SHOP_HOARD_PACK_UNITS whatever the ledger says - the ledger lets
	// a stack out only while the market is short, and two hundred of one
	// material in one bag is not a market, it is a bag.
	bool IsPlayerBotHoardedMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotTradeableMaterial(item))
			return false;
		const int total = (int)ch->CountSpecifyItem(item->GetVnum());
		if (total < PLAYERBOT_SHOP_HOARD_MIN_UNITS)
			return false;
		return total - GetPlayerBotRefineMaterialReserve(ch, item->GetVnum()) >=
				PLAYERBOT_SHOP_HOARD_MIN_UNITS;
	}

	// GetPlayerBotStallLineUnits for this bot's bag: a hoard sells in tens.
	int GetPlayerBotStallLineUnitsFor(LPCHARACTER ch, LPITEM item)
	{
		const int units = GetPlayerBotStallLineUnits(item);
		return units > 1 && IsPlayerBotHoardedMaterial(ch, item)
				? PLAYERBOT_SHOP_HOARD_PACK_UNITS : units;
	}

	// The refine scrolls a bot keeps for its own anvil (defined in
	// playerbot_town.h beside the rule that asks it).
	int GetPlayerBotRefineScrollKeep(LPCHARACTER ch);

	// What the stack a counter's lines are cut from keeps back: the anvil's
	// reserve of a material, the keys the bot holds on to, the scrolls of its
	// own scroll work, one of anything else.
	int GetPlayerBotStallBaseKeep(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return 1;
		// Asked before the materials: the Blessing Scroll is what recipe 501
		// consumes, so it is a tradeable material too, and the anvil's reserve
		// of it - twice the recipe count of every piece under scroll work - is
		// larger than most stacks. Measured against that, no cut ever had a
		// scroll to spare: a bot with twenty-six put a marble up instead
		// (16 September). The scroll's keep is the scroll rule's.
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return GetPlayerBotRefineScrollKeep(ch);
		if (IsPlayerBotTradeableMaterial(item))
			return std::max(1, GetPlayerBotRefineMaterialReserve(ch, item->GetVnum()));
		if (item->GetType() == ITEM_TREASURE_KEY)
			return PLAYERBOT_TREASURE_KEY_KEEP;
		if (IsPlayerBotBonusStoneItem(item))
			return PLAYERBOT_BONUS_STONE_KEEP;
		// The medal dropper is the medal shop and keeps one back; everybody else
		// keeps the ladder's two (PLAYERBOT_HORSE_MEDAL_KEEP) and lists the rest.
		if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return ch && GetPlayerBotPersonalityByPID(ch->GetPlayerID()) ==
					BOT_PERSONALITY_MEDAL_DROPPER ? 1 : PLAYERBOT_HORSE_MEDAL_KEEP;
		// Nobody keeps a root back: the heap is the whole of what it is for.
		if (IsPlayerBotBulkGoods(item))
			return 0;
		return 1;
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
		for (WORD cell = 0; cell < ownCell && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && IsPlayerBotTradeableMaterial(other) &&
					!IsPlayerBotFishingKeepsake(other->GetVnum()) &&
					++ahead >= PLAYERBOT_MATERIAL_STOCK_SLOTS)
				return true;
		}
		return false;
	}

	// CountPlayerBotFreeInventoryCells jest w playerbot_consumables.h, ktory
	// jest wlaczany wczesniej: skrzynie musza pytac o to samo, a w jednej
	// jednostce kompilacji definicja moze byc tylko jedna.

	// Occupied cells against PLAYERBOT_BAG_FULL_PERCENT of the bag. Counted
	// by cell rather than by item, so a weapon's three cells count as three.
	bool IsPlayerBotBagFull(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const int occupied = PLAYERBOT_BAG_CELLS - CountPlayerBotFreeInventoryCells(ch);
		return occupied * 100 >= PLAYERBOT_BAG_CELLS * PLAYERBOT_BAG_FULL_PERCENT;
	}

	// Defined with the stall's keeps in playerbot_town.h.
	int CountPlayerBotVnumUnitsAhead(LPCHARACTER ch, LPITEM item);

	// Fewer free cells than the loot and the chests need to land in. The
	// junk rule below reads this together with PlayerBotCanOpenShop: a bag
	// under pressure with a counter to sell from keeps its goods, a bag
	// under pressure with no counter (mt2009 before level 15 and 800 kills)
	// has nowhere but the merchant, and a bot that kept hunting with a full
	// bag "mowi ze podnosi lup ale nie robi nic" (JaroszV2, 11 September).
	bool IsPlayerBotBagUnderPressure(LPCHARACTER ch)
	{
		return ch && ch->IsItemLoaded() &&
				CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
	}

	// A bag piece this bot would put on: its slot is empty or it outscores
	// what is worn there. Everything else the junk rule may let go under
	// pressure.
	bool IsPlayerBotUpgradeForSelf(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item) ||
				item->GetLevelLimit() > ch->GetLevel())
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		LPITEM worn = ch->GetWear(wearCell);
		return !worn || GetPlayerBotEquipmentScore(item, ch) > GetPlayerBotEquipmentScore(worn, ch);
	}

	// Books of one skill in the cells before this one, in books. Cell order
	// decides, so the same books stay put from one town visit to the next. It
	// used to count rows, and a book stacks to ten on mt2009, so "keep twelve"
	// kept twelve stacks - up to a hundred and twenty books of a skill nothing
	// was selling. An item outside the bag (the safebox's, asked
	// whether to come out) has no cells before it: the whole bag is in front.
	int CountPlayerBotSkillBooksAhead(LPCHARACTER ch, LPITEM item, DWORD skillVnum)
	{
		int ahead = 0;
		const WORD ownCell = item->GetWindow() == INVENTORY
				? item->GetCell() : (WORD)PLAYERBOT_BAG_CELLS;
		for (WORD cell = 0; cell < ownCell && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetCell() == cell &&
					other->GetType() == ITEM_SKILLBOOK &&
					GetPlayerBotSkillBookSkillVnum(other) == skillVnum)
				ahead += std::max<int>(1, other->GetCount());
		}
		return ahead;
	}

	// Defined further down (playerbot_progression_needs.h): a bot's books of
	// one skill over the whole bag, and whether a skill of its build is at a
	// Grand Master grade the soul stone trains.
	int CountPlayerBotOwnedSkillBooks(LPCHARACTER ch, DWORD skill);
	bool PlayerBotHasGrandMasterToTrain(LPCHARACTER ch);

	// The goods a bot keeps a number of and sells the rest of one at a time: a
	// skill book (its kind is its skill) and the soul stone. A counter line of
	// either is a single (GetPlayerBotStallLineUnits), because a buyer takes a
	// line only when all of it fits what it is short of.
	// The goods a bot keeps by count over the whole bag and cuts lines of from
	// what is over the keep: its own skill books, the soul stone and the Zen
	// bean (GetPlayerBotCountedGoodsKeep).
	bool IsPlayerBotCountedSingleGoods(LPITEM item)
	{
		return item && (item->GetType() == ITEM_SKILLBOOK ||
				item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM ||
				item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM);
	}

	// What PLAYERBOT_SHOP_SAME_VNUM_LINES caps by vnum: everything but the
	// goods counted by kind, a Forgetting Scroll (its skill is its kind) and a
	// marble (PLAYERBOT_SHOP_MARBLE_LINES, one a monster).
	bool IsPlayerBotSameVnumCapped(LPITEM item)
	{
		return item && !IsPlayerBotCountedSingleGoods(item) &&
				item->GetType() != ITEM_SKILLFORGET && item->GetType() != ITEM_POLYMORPH;
	}

	// One key per kind of those goods, zero for anything else: a book's skill
	// with the top bit set (socket 0 of the engine's 50300, the first value of
	// a named book - GetPlayerBotSkillBookSkillVnum), the stone's vnum. Asked of
	// the raw fields too, because an offline counter's line is not an item.
	DWORD GetPlayerBotStallKindKeyOf(DWORD vnum, BYTE type, long socket0, long value0)
	{
		if (type == ITEM_SKILLBOOK)
			return 0x80000000U | (DWORD)(vnum == 50300 ? socket0 : value0);
		return vnum == PLAYERBOT_GRAND_MASTER_STONE_VNUM || vnum == PLAYERBOT_ZEN_BEAN_VNUM ? vnum : 0;
	}

	// The beans this bot keeps (PLAYERBOT_ZEN_BEAN_KEEP_MIN..MAX), the same
	// number every time it is asked.
	int GetPlayerBotZenBeanKeep(LPCHARACTER ch)
	{
		if (!ch)
			return PLAYERBOT_ZEN_BEAN_KEEP_MAX;
		return PLAYERBOT_ZEN_BEAN_KEEP_MIN + (int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x5a454e42U) %
				(DWORD)(PLAYERBOT_ZEN_BEAN_KEEP_MAX - PLAYERBOT_ZEN_BEAN_KEEP_MIN + 1));
	}

	DWORD GetPlayerBotStallKindKey(LPITEM item)
	{
		return item ? GetPlayerBotStallKindKeyOf(item->GetVnum(), item->GetType(),
				item->GetSocket(0), item->GetValue(0)) : 0;
	}

	// What the bag keeps of that kind: the books of its own skill it will read
	// (GetPlayerBotBookKeepLimit) and none of anybody else's; three stones while
	// a skill stands at a grade they train, one against the day one will. The
	// same numbers the buyer's side asks for (GetPlayerBotProgressionNeed), so a
	// counter never sells what its keeper would walk to the market to buy back.
	int GetPlayerBotCountedGoodsKeep(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		if (item->GetType() == ITEM_SKILLBOOK)
		{
			const DWORD skill = GetPlayerBotSkillBookSkillVnum(item);
			return ch->GetSkillGroup() != 0 && IsPlayerBotOwnSkill(ch, skill)
					? GetPlayerBotBookKeepLimit(ch, skill) : 0;
		}
		if (item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return PlayerBotHasGrandMasterToTrain(ch) ? PLAYERBOT_GRAND_MASTER_STONE_KEEP : 1;
		if (item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM)
			return GetPlayerBotZenBeanKeep(ch);
		if (IsPlayerBotBonusStoneItem(item))
			return PLAYERBOT_BONUS_STONE_KEEP;
		return 0;
	}

	// Units of the item's kind over the whole bag.
	int CountPlayerBotStallKindUnits(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		if (item->GetType() == ITEM_SKILLBOOK)
			return CountPlayerBotOwnedSkillBooks(ch, GetPlayerBotSkillBookSkillVnum(item));
		return (int)ch->CountSpecifyItem(item->GetVnum());
	}

	// A key whose lock matches this chest, anywhere in the bag.
	bool PlayerBotHasTreasureKeyFor(LPCHARACTER ch, LPITEM box)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM key = ch->GetInventoryItem(cell);
			if (key && key->GetType() == ITEM_TREASURE_KEY && key->GetValue(0) == box->GetValue(0))
				return true;
		}
		return false;
	}

	// A chest this key opens, anywhere in the bag.
	bool PlayerBotHasTreasureBoxFor(LPCHARACTER ch, LPITEM key)
	{
		for (WORD cell = 0; ch && key && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM box = ch->GetInventoryItem(cell);
			if (box && box->GetType() == ITEM_TREASURE_BOX && box->GetValue(0) == key->GetValue(0))
				return true;
		}
		return false;
	}

	// A key for a chest the bot holds and has no key for: what a counter or the
	// storekeeper should hand back to it.
	bool PlayerBotWantsTreasureKey(LPCHARACTER ch, LPITEM key)
	{
		if (!ch || !key || key->GetType() != ITEM_TREASURE_KEY || !PlayerBotHasTreasureBoxFor(ch, key))
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held != key && held->GetType() == ITEM_TREASURE_KEY &&
					held->GetValue(0) == key->GetValue(0))
				return false;
		}
		return true;
	}

	// A key past the PLAYERBOT_TREASURE_KEY_KEEP of its kind the bot holds on
	// to, with no chest in the bag it opens: goods for a counter, and the
	// storekeeper's when the bag is short of room. A key was never the
	// merchant's and never a counter's, while the chests it opens go to the
	// merchant under bag pressure - so on 15 September 2598 gold and silver keys
	// lay in 1057 bags and not one of those bags held a chest ("srebrne i zlote
	// klucze ... chomikuja to bez konca", Tieru). The last units in cell order
	// are the ones kept.
	bool IsPlayerBotSurplusTreasureKey(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_TREASURE_KEY || PlayerBotHasTreasureBoxFor(ch, item))
			return false;
		const int total = (int)ch->CountSpecifyItem(item->GetVnum());
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetVnum() == item->GetVnum())
				ahead += std::max<int>(1, other->GetCount());
		}
		return ahead < total - PLAYERBOT_TREASURE_KEY_KEEP;
	}

	// How many bots are short of a material and can pay for it, from the
	// market ledger (playerbot_market.h, which comes after this fragment).
	DWORD GetPlayerBotLedgerDemand(DWORD vnum);

	// A spare of a higher tier than the piece worn in its slot: not an
	// upgrade yet, but one the blacksmith can make into one, so neither the
	// merchant nor the refine pass treats it as scrap. Only the best such
	// spare per slot counts; the rest are still scrap.
	// A candidate that outranks the worn piece: by the engine's level limit,
	// or by Iwakura's PvE tier where his list rates both families - a bot
	// wearing Miedziane Kolczyki with Ebonitowe in the bag has an upgrade to
	// make whatever the level limits say, and his instruction is to refine
	// and bonus it before wearing it, not to swap blindly (16 September).
	bool PlayerBotOutranksWornTier(LPCHARACTER ch, LPITEM cand, LPITEM worn)
	{
		if (cand->GetLevelLimit() > worn->GetLevelLimit())
			return true;
		const int candTier = GetPlayerBotItemTierOf(cand, ch);
		const int wornTier = GetPlayerBotItemTierOf(worn, ch);
		return candTier > 0 && wornTier > 0 && candTier > wornTier;
	}

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
		if (!worn || !PlayerBotOutranksWornTier(ch, item, worn))
			return false;
		const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
		for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
		{
			LPITEM other = ch->GetInventoryItem(otherCell);
			if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
					other->GetLevelLimit() > ch->GetLevel() ||
					!PlayerBotOutranksWornTier(ch, other, worn) ||
					other->FindEquipCell(ch) != wearCell)
				continue;
			const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
			if (otherScore > itemScore ||
					(otherScore == itemScore && other->GetID() < item->GetID()))
				return false;
		}
		return true;
	}

	// Gear under PLAYERBOT_SHOP_MIN_GEAR_LEVEL: what a counter carries only at
	// PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE and only a little of, and what the
	// merchant takes below that.
	bool IsPlayerBotLowLevelGear(LPITEM item)
	{
		return item && (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetLevelLimit() < PLAYERBOT_SHOP_MIN_GEAR_LEVEL;
	}

	// The refine that gear needs for a counter: +6, and +7 for a weapon or a body
	// armour of level one (PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE). It is one
	// number for two decisions - what a counter takes and what the junk rule
	// scraps under - so raising it past seven is what sent a starter +7 to a
	// merchant, against "+7 to nigdy nie jest zlom".
	BYTE GetPlayerBotLowGearMinRefine(LPITEM item)
	{
		if (item && (int)item->GetLevelLimit() <= PLAYERBOT_SHOP_STARTER_GEAR_MAX_LEVEL &&
				(item->GetType() == ITEM_WEAPON ||
				 (item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY)))
			return PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE;
		return PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE;
	}

	// Whether a counter line of that gear takes one of the
	// PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES places: +7 and better does not.
	bool CountsAgainstPlayerBotLowGearCap(LPITEM item)
	{
		return IsPlayerBotLowLevelGear(item) &&
				item->GetRefineLevel() < PLAYERBOT_SHOP_LOW_GEAR_CAP_BELOW_REFINE;
	}

	// Iwakura's Useful Items List (playerbot_lpp.h).
	bool IsPlayerBotLppKeptItem(LPCHARACTER ch, LPITEM item);

	bool IsPlayerBotJunkItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->isLocked())
			return false;

		// The operator's word first: merchant is scrap whatever the rules
		// below would keep it for; keep, stall and drop are never scrap (drop
		// is thrown away by the merchant leg, not sold).
		{
			const BYTE policy = GetPlayerBotItemPolicy(item);
			if (policy == PLAYERBOT_ITEM_POLICY_MERCHANT)
				return true;
			if (policy != PLAYERBOT_ITEM_POLICY_NONE)
				return false;
		}

		// A piece Iwakura's list keeps for the storekeeper is never the
		// merchant's, whatever the rules below would make of it.
		if (IsPlayerBotLppKeptItem(ch, item))
			return false;

		const DWORD vnum = item->GetVnum();

		// Maska Sabaha left the world with the Hwang curse (IsPlayerBotRetiredItem):
		// the merchant takes the ones still in bags.
		if (IsPlayerBotRetiredItem(vnum))
			return true;
		// So are the uniques a bot leaves on the ground (IsPlayerBotLeftOnGroundItem).
		if (IsPlayerBotLeftOnGroundItem(vnum))
			return true;
		// The Demon Tower's keys are the floor's while the bot is in the tower
		// and nothing anywhere else (the quest takes a player's on logout).
		if (IsPlayerBotDemonTowerKey(vnum))
			return !IsPlayerBotDemonTowerInstance(ch->GetMapIndex());
		// One of Iwakura's fifty-four weapons nobody buys at +0..+3 goes to the
		// merchant once the bots' counters carry PLAYERBOT_JUNK_WEAPON_MARKET_CAP
		// of them - unless this bot will wear it, refine it or keep it as the
		// weapon for the day its own burns. Asked before the pickup goods below,
		// which would keep the level-65 ones for a counter that has no room.
		if (IsPlayerBotCappedJunkWeapon(item) && IsPlayerBotJunkWeaponMarketFull() &&
				!IsPlayerBotUpgradeForSelf(ch, item) && !IsPlayerBotHigherTierSpare(ch, item) &&
				!IsPlayerBotKeptBackupWeapon(ch, item))
			return true;
		// The goods a player crafts further (IsPlayerBotPickupGoods) wait for a
		// counter, and reach the merchant only from a bag under pressure that
		// has no counter to sell from - the rule a polymorph marble keeps.
		if (IsPlayerBotPickupGoods(item))
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotCanOpenShop(ch);
		// Kamien Duchowy is its owner's training (ManagePlayerBotGrandMasterTraining),
		// never the merchant's: he paid 194 yang for one.
		if (vnum == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return false;
		// A hairstyle from the ItemShop (playerbot_itemshop.h) is worn, not sold:
		// the rule's default would vendor it on the next town trip.
		if (item->GetType() == ITEM_COSTUME)
			return false;

		// Baek-Go's board (playerbot_herbalism.h) gave two kinds of item a
		// worth this rule had no branch for, so its default sold both. A recipe
		// is knowledge - and the only source of it in this world is the Metin
		// stones the bots break all day, so the whole system was being fed to
		// the merchant a few hundred yang at a time - and a potion is what the
		// herbs finally turn into. Neither is ever merchant scrap; the surplus
		// of both goes on a counter, where a player can reach it at last.
		if (IsPlayerBotCraftRecipeItem(item) || IsPlayerBotCraftedPotion(item))
			return false;

		// A specimen of a Biologist row already handed in is scrap, not goods:
		// "niech ich nie wystawiaja, sprzedaja u handlarza albo wyrzucaja".
		// Before the anti-sell test on purpose - the quest items carry it, and
		// the merchant leg pays the merchant's pennies for them anyway. The Orc
		// Tooth is a refine material and takes the material branch below.
		if (vnum >= 50701 && vnum <= 50706 && !IsPlayerBotTradeableMaterial(item) &&
				IsPlayerBotBiologistSpecimenSurplus(ch, vnum))
			return true;

		// A material only the Herbalist's Knife consumes is nothing to a bot:
		// see IsPlayerBotNonGearMaterial.
		if (item->GetType() == ITEM_MATERIAL && IsPlayerBotNonGearMaterial(vnum))
			return true;

		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return false;
		// What the bot uses is never scrap, whatever the vnum: a timed buff or an
		// ability potion is drunk in the next fight, a bonus stone or marble goes
		// on its gear, an auto potion with anything left in it keeps its recovery
		// running (an empty one is scrap), and the Metin detector is a player
		// thing for the counter. Only the ItemShop copies
		// (76xxx) lack ANTI_SELL, which is how a bot handed them from the panel
		// vendored the lot (Pasywny, 13 September).
		if (item->GetType() == ITEM_USE)
		{
			const BYTE sub = item->GetSubType();
			// A Combo or Leadership book is read or sold on a counter, never
			// vendored, the way a skill book is.
			if (IsPlayerBotGeneralSkillBook(vnum))
				return false;
			if (IsPlayerBotBoosterItem(item) || IsPlayerBotMetinDetector(vnum) ||
					(GetPlayerBotAutoPotionAffect(vnum) != 0 && !IsPlayerBotAutoPotionEmpty(item)) ||
					sub == USE_ADD_ATTRIBUTE || sub == USE_CHANGE_ATTRIBUTE || sub == USE_ADD_ATTRIBUTE2)
				return false;
		}

		// Level-30 weapons with average/skill damage are strategic market assets.
		// Never vendor them: this also applies when the current owner is below level
		// 30 or belongs to another class. They remain available for future playerbot
		// trading/private shops instead of disappearing for a trivial NPC price.
		if (IsPlayerBotSpecialLevel30Weapon(item))
			return false;

		// The Archer's one stone weapon (playerbot_gear.h) is kept.
		if (IsPlayerBotArcherBuild(ch) && IsPlayerBotStoneMeleeWeapon(ch, item) &&
				FindPlayerBotStoneWeapon(ch, false) == item)
			return false;
		// So is the weapon kept for the day the one in the hand burns
		// (FindPlayerBotBackupWeapon).
		if (IsPlayerBotKeptBackupWeapon(ch, item))
			return false;

		// Gear the counter could not sell in six stands is scrap, whatever the
		// rules below would keep it for - up to PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE.
		// This has to come before the precious-refine keep below, or it never
		// applies to the +4 and +5 the counter actually keeps, which is what it
		// was written for: with it underneath, a bag of unsold +5 was for life.
		// Only when the bag is under pressure, though: a keeper that simply has
		// extra and does not need the yang can stand as long as the loop needs,
		// re-listing the piece; the merchant is for a cornered bot, not a bored
		// one ("jesli maja extra a nie potrzebuja yang, moga stac", akhigubernator).
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetRefineLevel() <= PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE &&
				IsPlayerBotBagUnderPressure(ch))
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

		// A full bag with no counter to sell from. On mt2009 a bot under level
		// 15 or 800 kills cannot open a stall, so "goods for the counter" was
		// a bag for life: full, every drop refused, the bot hunting on with
		// "Podnosze lup" over its head. Gear the merchant may have (up to +4,
		// the operator's line) goes to him now when the bag is under pressure
		// and no counter is possible - unless this bot would wear it.
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetRefineLevel() <= PLAYERBOT_MERCHANT_MAX_REFINE &&
				IsPlayerBotBagUnderPressure(ch) && !PlayerBotCanOpenShop(ch) &&
				!IsPlayerBotUpgradeForSelf(ch, item))
			return true;

		// Gear under level thirty below +6 is not counter goods any more (see
		// PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE), so once the bot has no use for it
		// the merchant takes it, +4 and +5 included. Kept, it would ride in the
		// bag for good: the counter it used to be kept for no longer takes it,
		// and the unsold-stands rule above only counts what went up.
		if (IsPlayerBotLowLevelGear(item) &&
				item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE &&
				item->GetRefineLevel() < GetPlayerBotLowGearMinRefine(item) &&
				!IsPlayerBotUpgradeForSelf(ch, item) && !IsPlayerBotHigherTierSpare(ch, item) &&
				item->GetID() != GetPlayerBotBackupWeaponID(ch, false))
			return true;

		// A piece of Iwakura's list past what the list keeps goes on a counter
		// for the gamblers (community patch 2, point 9), and to the merchant
		// only from a bag under pressure with no counter to sell from. Gear
		// under level thirty keeps the operator's rule above.
		if (IsPlayerBotLppSurplusGoods(ch, item) && !IsPlayerBotLowLevelGear(item) &&
				!IsPlayerBotUpgradeForSelf(ch, item))
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotCanOpenShop(ch);

		// Whatever else it is, a +5 or better is not something to hand an NPC for
		// a fifth of the shop price. The reserve rule below keeps one spare per
		// slot and sold the rest; that is how a Riba +9 went to a merchant
		// because the same bot was carrying an axe +9. These go on a stall -
		// or to the blacksmith first and then on a stall - and never to him.
		if (item->GetRefineLevel() > PLAYERBOT_MERCHANT_MAX_REFINE)
			return false;
		// +4 exactly is the counter's own threshold: kept as goods while the
		// counter can sell it, scrap after six unsold stands (above).
		if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			return false;

		// A scrap keeper's low refines are its stock, not its junk - until the
		// bag runs short, and then the merchant gets them like anyone else's.

		// Not gear under level thirty, which no counter takes below +6 now.
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				!IsPlayerBotLowLevelGear(item) &&
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
				IsPlayerBotBiologistSpecimen(vnum) || IsPlayerBotBiologistKeyItem(vnum))
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
		// A polymorph marble is counter goods; the merchant takes it only
		// under bag pressure with no counter to sell from, like a material -
		// or past the PLAYERBOT_SHOP_MARBLE_LINES a counter shows, which no
		// counter will take and which would otherwise ride in the bag for good.
		if (item->GetType() == ITEM_POLYMORPH)
			return IsPlayerBotBagUnderPressure(ch) && (!PlayerBotCanOpenShop(ch) ||
					CountPlayerBotVnumUnitsAhead(ch, item) >= PLAYERBOT_SHOP_MARBLE_LINES);
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
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		// A pickaxe is tackle, exactly as a rod is, and the same rule applies:
		// one is the tool, a second one is scrap. It costs eighty thousand yang
		// and the junk rule's default is to sell, so without this a miner would
		// vendor its pickaxe on the town trip after every session.
		if (item->GetType() == ITEM_PICK)
		{
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && worn != item && worn->GetType() == ITEM_PICK && worn->GetVnum() >= vnum)
				return true;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (other && other != item && other->GetType() == ITEM_PICK &&
						(other->GetVnum() > vnum || (other->GetVnum() == vnum && other->GetID() < item->GetID())))
					return true;
			}
			return false;
		}
		// Ore, raw and smelted. A hundred raw make one smelted piece and the
		// smelted ones are what a player crosses a market for, so neither is
		// ever the merchant's - they are the whole point of the digging, and
		// the counter is where the operator asked the trade to happen.
		if (IsPlayerBotRawOre(vnum) || IsPlayerBotSmeltedOre(vnum))
			return false;
		// Hair dye is never the merchant's: the item shop's is goods, and one
		// from the water is thrown away (DiscardPlayerBotFishedDyes) but for
		// the colour a bot has yet to use and the few kept for a counter.
		if (IsPlayerBotHairDye(vnum))
			return false;
		// A dead fish waits for the campfire at the end of the next session, as
		// long as the bot has the wood for one; a grilled fish is a potion.
		if (item->GetType() == ITEM_FISH && item->GetSubType() == FISH_DEAD &&
				ch->CountSpecifyItem(PLAYERBOT_CAMPFIRE_VNUM) > 0)
		{
			// As many as a fire is worth: the first PLAYERBOT_DEAD_FISH_KEEP in
			// bag order, and the rest go to the merchant.
			int ahead = 0;
			for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM held = ch->GetInventoryItem(cell);
				if (held && held->GetCell() == cell && held->GetType() == ITEM_FISH &&
						held->GetSubType() == FISH_DEAD)
					ahead += std::max<int>(1, held->GetCount());
			}
			if (ahead < PLAYERBOT_DEAD_FISH_KEEP)
				return false;
		}
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
		// Only a material nobody wants, and only under bag pressure - or a
		// material this bot has no counter to sell from, whoever wants it.
		// And only under bag pressure at all: "Stalowy Grot, Futro Yeti, Kawalek
		// Lodu ... sprzedane handlarzowi" non stop from bags with room to spare.
		// A refine material is never merchant scrap. What it does not sell on a
		// counter and the bag has no room for goes to the storekeeper
		// (CollectPlayerBotSafeboxMaterials), not to the merchant for pennies:
		// "jak nie ma miejsca to materialy niech traf ia do magazynu u Dozorcy"
		// (Tieru, 13 September).
		if (IsPlayerBotTradeableMaterial(item))
			return false;
		// The rest of the 30000 block is eight gift boxes and two quest items.
		// No counter would carry those, so there junk still means junk.
		if (vnum >= 30000 && vnum <= 30200)
			return !PlayerBotNeedsRefineMaterial(ch, vnum) &&
					GetPlayerBotLedgerDemand(vnum) == 0 &&
					CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
		if (vnum >= 70038 && vnum <= 70060)
			return false;

		// What Iwakura's sheet prices by name and no rule above placed - the
		// horse books (50060-50062), the polymorph books (50314-50316), the
		// stone detachment scroll (25100) - is goods, the way a polymorph
		// marble is: the counter's (ScorePlayerBotShopStock), and the
		// merchant's only from a bag under pressure with no counter to sell
		// from. The default below sold them all - on the test world some
		// thousand of each in a day, for a few hundred yang against 40 000 to
		// 135 000 on the sheet (Tieru, 18 September).
		if (IsPlayerBotSheetGoods(item))
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotCanOpenShop(ch);

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
					for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
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
					for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
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

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			if (IsPlayerBotJunkItem(ch, ch->GetInventoryItem(cell)))
				++count;
		return count;
	}

	// Boosters nobody but their holder can use, past PLAYERBOT_BOOSTER_KEEP_PER_VNUM,
	// thrown away at the merchant visit. Only the ones that may neither be sold
	// nor put on a counter: an ItemShop copy without ANTI_SELL is the operator's
	// gift and stays. The first stacks are kept and whole stacks go after them,
	// so a bot is left with at most one stack over the keep.
	void DiscardPlayerBotSurplusBoosters(LPCHARACTER ch)
	{
		std::map<DWORD, int> kept;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->isLocked() ||
					!IsPlayerBotBoosterItem(item) ||
					!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL) ||
					!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MYSHOP))
				continue;
			int& held = kept[item->GetVnum()];
			if (held < PLAYERBOT_BOOSTER_KEEP_PER_VNUM)
			{
				held += (int)item->GetCount();
				continue;
			}
			sys_log(0, "PLAYERBOT_CHEST: discarded surplus booster pid=%u name=%s vnum=%u count=%u kept=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetCount(), held);
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DISCARD_BOOSTER");
		}
	}

	bool SellPlayerBotJunkAtMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category,
			const char* merchantName)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		DiscardPlayerBotSurplusBoosters(ch);
		// Sold rather than thrown away under Iwakura's system (the Rybak's way).
		DiscardPlayerBotFishedDyes(ch, IsPlayerBotPersonaEnabled());

		size_t soldCount = 0;
		long long totalSoldGold = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			// The operator said "drop": thrown away here, at the merchant,
			// without a sale - the one place a bag is emptied on purpose.
			if (item && !item->IsEquipped() && !item->isLocked() &&
					GetPlayerBotItemPolicy(item) == PLAYERBOT_ITEM_POLICY_DROP)
			{
				sys_log(0, "PLAYERBOT_AI: discarded by policy pid=%u name=%s vnum=%u count=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetCount());
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DISCARD");
				continue;
			}
			if (!item || !IsPlayerBotJunkItem(ch, item) ||
					GetPlayerBotJunkMerchant(item) != category)
				continue;

			DWORD price = item->GetShopBuyPrice();
			if (price == 0)
				price = item->GetProto() ? item->GetProto()->dwGold : 100;
			price = std::max<DWORD>(10, price / 5);
			totalSoldGold += price;
			PlayerBotChangeGold(ch, price);
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

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->FindEquipCell(ch) == wearCell)
				return true;
		}

		return false;
	}

	// The highest level a village merchant sells this character a weapon at:
	// what a burned weapon is replaced with for yang, on the spot.
	int GetPlayerBotMerchantWeaponCeiling(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		int ceiling = 0;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(offers[k].vnum);
				if (!proto || proto->bType != ITEM_WEAPON || !IsPlayerBotProtoForCharacter(ch, proto) ||
						!IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType))
					continue;
				const int level = GetPlayerBotProtoLevelLimit(proto);
				if (level <= (int)ch->GetLevel())
					ceiling = std::max(ceiling, level);
			}
		}
		return ceiling;
	}

	// The weapon in the hand (GetPlayerBotHandWeapon) at a step the plain anvil
	// can burn it, with nothing to fall back on: no backup in the bag
	// (FindPlayerBotBackupWeapon) and nothing a merchant sells at its level.
	// Such a step goes under a scroll or waits for one. A level-30 weapon is
	// left out - grinding one at the anvil and buying the next is the
	// operator's rule - and so is a scroll-only weapon, held on its own.
	bool IsPlayerBotWornWeaponAtRisk(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		if (!ch || !item || item->GetType() != ITEM_WEAPON || item->GetRefinedVnum() == 0 ||
				IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotScrollOnlyWeapon(item))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe || recipe->prob > PLAYERBOT_WORN_SCROLL_MAX_PROB)
			return false;
		if (item != GetPlayerBotHandWeapon(ch) ||
				item->GetLevelLimit() <= GetPlayerBotMerchantWeaponCeiling(ch))
			return false;
		return GetPlayerBotBackupWeaponID(ch, fresh) == 0;
	}

	// The armour on the bot's back, at a step that can burn it, with no
	// other body armour in the bag it could put on: the weapon's rule for
	// the other slot a bot cannot do without. Every failed refine destroys
	// the piece on this engine, so a bot of twenty raised its only plate at
	// the anvil, lost it, and went on farming bare with the upgrade
	// materials it had kept for it (THC, 16 September). Under a scroll or
	// not at all; the merchant's re-stock is a town visit away, and that is
	// exactly the bare walk the report was about.
	bool IsPlayerBotWornArmourAtRisk(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_ARMOR || item->GetSubType() != ARMOR_BODY ||
				item->GetRefinedVnum() == 0 || ch->GetWear(WEAR_BODY) != item)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe || recipe->prob > PLAYERBOT_WORN_SCROLL_MAX_PROB)
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM spare = ch->GetInventoryItem(cell);
			if (!spare || spare->GetCell() != cell || spare == item ||
					spare->GetType() != ITEM_ARMOR || spare->GetSubType() != ARMOR_BODY ||
					spare->GetLevelLimit() > (int)ch->GetLevel() ||
					!IsPlayerBotProtoForCharacter(ch, spare->GetProto()))
				continue;
			return false;
		}
		return true;
	}

	// The scroll a refine goes under: from PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS
	// the Zwoj Boga Smokow when the bag has one, otherwise the Blessing
	// Scroll. Both are read by DoRefineWithScroll from the cell SetRefineMode
	// names, no blacksmith needed - which is also why the scroll pass runs
	// wherever the bot stands. plusLevel 0 means "any scroll that is here".
	int FindPlayerBotRefineScrollCell(LPCHARACTER ch, BYTE plusLevel, int stepProb = 100)
	{
		if (!ch)
			return -1;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// mt2009 knows a scroll by what DoRefineWithScroll reads off it, not by
		// vnum: value0 the kind, value1 the percent added to the step. The War
		// God scroll (UP_TO_3TH_LEVEL) makes a step under +4 certain; a plain
		// scroll hands the piece back a level down on failure, the more value1
		// the better; the Magic Stone (NO_REDUCTION_WHEN_FAIL) keeps the level
		// and is saved for steps at PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB and
		// under; the Gwarancja (REFINE_BONUS) burns what it fails and is never
		// taken. By vnum only 25040 and 71032 were ever used, and every other
		// kind a bot found was goods to it. plusLevel 0 is "any scroll here".
		int bestCell = -1, bestRank = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (!scroll || scroll->GetCell() != cell || scroll->GetType() != ITEM_USE ||
					scroll->GetSubType() != USE_TUNING)
				continue;
			int rank = 0;
			switch (scroll->GetValue(0))
			{
				case UP_TO_3TH_LEVEL_SCROLL:
					rank = plusLevel < 4 ? 1000 : 0;
					break;
				case NO_REDUCTION_WHEN_FAIL_SCROLL:
					rank = stepProb <= PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB ? 900 : 100;
					break;
				case NORMAL_REFINE_SCROLL:
					rank = 200 + std::max<int>(0, (int)scroll->GetValue(1));
					break;
				default:
					break;
			}
			if (rank > bestRank)
			{
				bestRank = rank;
				bestCell = cell;
			}
		}
		return bestCell;
#else
		(void)stepProb;
		int blessing = -1, dragonGod = -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
#endif
	}

	// Whether a piece lying in the bag is one this bot would actually raise.
	//
	// It exists because the planner and the pass that does the refining used to
	// answer that question differently: the planner accepted anything the
	// general equipment selector liked, the executor then also rejected
	// whatever the junk rule had marked for the merchant. A bot could therefore
	// see an opportunity, commit to a blacksmith visit - and BOT_TOWN_PHASE
	// treats a started visit as a commitment that outranks the ordinary goal
	// choice - walk there, find nothing to do, and come back. Reported as "mam
	// wszystko +9 zalozone, a bot dalej lezie do kowala".
	//
	// Worn pieces do not go through here: the junk rule does not apply to
	// something the bot is wearing, and a full +9 set is not a reason to refuse
	// a legitimate upgrade waiting in the bag.
	//
	// And "not scrap" is not "worth the bot's yang". The junk rule keeps a
	// great deal on purpose - a collector's stock, anything at +4 for the
	// counter, a piece with prize lines - and the refine pass took all of it
	// to the anvil: a bot under a Guillotine Blade +4 with 14 000 yang left
	// raised level-one swords, glaives, wooden earrings and copper necklaces
	// from +1 to +3 one by one, and burned one (elgrandebgc, 12 September,
	// "ulepszaja wszystko co popadnie, zeby potem wystawic za bezcen"). Only
	// what the bot will wear is worth refining in the bag: an upgrade the
	// equipment pass is about to put on, or the one higher-tier spare per slot
	// the blacksmith can make into one. Goods are sold at what they are.
	bool IsPlayerBotRefineBagCandidate(LPCHARACTER ch, LPITEM item)
	{
		if (!item || item->GetRefinedVnum() == 0)
			return false;
		// A level-30 weapon of a class this bot cannot wear, ground for sale
		// (PlayerBotRefinesLevel30ForSale): no equipment candidate of its own,
		// and never junk.
		if (PlayerBotRefinesLevel30ForSale(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// The class's own level-30 weapon, whatever the damage model makes of
		// it today (community patch 2, point 1).
		if (IsPlayerBotPersonaEnabled() && item == FindPlayerBotClassLevel30Weapon(ch))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		if (!IsPlayerBotEquipmentCandidate(ch, item) ||
				IsPlayerBotJunkItem(ch, item))
			return false;
		// An Archer's stone dagger is worn only on a stone, so it is neither a
		// wearable upgrade nor a higher-tier spare - yet it must reach +4 to break
		// stones at all (Tieru). Refine it in the bag like a worn piece.
		if (IsPlayerBotArcherStoneWeapon(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// And the level-30 weapon it is grinding: not worn yet because it is not
		// yet better, and not better until it is refined.
		// The project, and every other level-30 weapon this bot keeps for the
		// anvil: it is not worn yet because it is not better yet, and it will
		// not be better until it is refined.
		if (PlayerBotKeepsLevel30ForAnvil(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		return IsPlayerBotHigherTierSpare(ch, item) ||
				IsPlayerBotWearableUpgrade(ch, item, item->GetCell());
	}

	// Iwakura's community patch 2, point 11: the Perfectionists who buy a
	// finished piece rather than make one, fifteen in a hundred by pid.
	bool IsPlayerBotMarketPerfectionist(DWORD pid)
	{
		if ((int)(PlayerBotNavHash(pid ^ 0x52454459U) % 100U) < PLAYERBOT_READY_GEAR_PERCENT)
			return true;
		// So are the Grinders who never hold and the ones who gave grinding
		// up: "maja glownie kupowac ulepszacze i przedmioty z rynku"
		// (community patch 2, point 2).
		if (playerbot_persona::NeverHoldsAtLocks(pid))
			return true;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
		// And a medal dropper that met its goal (community patch 2, point 4).
		return st != s_mapPlayerBotAIStates.end() &&
				(st->second.persona.bQuitGrinding || st->second.persona.bMedalGoalDone);
	}

	// Whether a wear slot is one the finished piece is looked for in: the
	// weapon, the armour, the shield and the helmet.
	bool IsPlayerBotReadyGearSlot(int wearCell)
	{
		return wearCell == WEAR_WEAPON || wearCell == WEAR_BODY || wearCell == WEAR_SHIELD ||
				wearCell == WEAR_HEAD;
	}

	// A finished piece for this bot: its class's, of the slot, at +8 or +9,
	// no higher than its level and at most PLAYERBOT_READY_GEAR_LEVEL_WINDOW
	// under it ("bot majacy 34. poziom zauwazy Smiertelna Zbroje Plytowa +8").
	bool IsPlayerBotReadyGearProto(LPCHARACTER ch, const TItemTable* proto, DWORD vnum, int wearCell)
	{
		if (!ch || !proto || (proto->bType != ITEM_WEAPON && proto->bType != ITEM_ARMOR) ||
				(int)(vnum % 10) < PLAYERBOT_READY_GEAR_MIN_PLUS || !IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		if (proto->bType == ITEM_WEAPON)
		{
			if (wearCell != WEAR_WEAPON || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType))
				return false;
		}
		else if ((wearCell == WEAR_BODY && proto->bSubType != ARMOR_BODY) ||
				(wearCell == WEAR_SHIELD && proto->bSubType != ARMOR_SHIELD) ||
				(wearCell == WEAR_HEAD && proto->bSubType != ARMOR_HEAD) || wearCell == WEAR_WEAPON)
			return false;
		const int level = GetPlayerBotProtoLevelLimit(proto);
		return level <= (int)ch->GetLevel() && level + PLAYERBOT_READY_GEAR_LEVEL_WINDOW >= (int)ch->GetLevel();
	}

	// Whether some counter holds one for this slot, better than the piece worn
	// there: none worn, one worn under +8, or a lower level. Read off the
	// ledger, without walking to a counter.
	bool PlayerBotMarketHasReadyGear(LPCHARACTER ch, int wearCell)
	{
		if (!ch || !IsPlayerBotReadyGearSlot(wearCell))
			return false;
		LPITEM worn = ch->GetWear((WORD)wearCell);
		const int wornLevel = worn && worn->GetProto() ? GetPlayerBotProtoLevelLimit(worn->GetProto()) : -1;
		for (TPlayerBotMarketLedger::const_iterator it = s_mapMarketLedger.begin(); it != s_mapMarketLedger.end(); ++it)
		{
			if (it->second.dwSupplyUnits == 0)
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(it->first);
			if (!IsPlayerBotReadyGearProto(ch, proto, it->first, wearCell))
				continue;
			if (!worn || worn->GetRefineLevel() < PLAYERBOT_READY_GEAR_MIN_PLUS ||
					GetPlayerBotProtoLevelLimit(proto) > wornLevel)
				return true;
		}
		return false;
	}

	// A counter's piece the market Perfectionist buys: a finished one by the
	// rule above, and an upgrade on what it wears (WantsPlayerBotStallItem's
	// gear test still decides that). Paid from the Perfectionist's share.
	bool IsPlayerBotReadyGearOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotPersonaEnabled() ||
				!IsPlayerBotMarketPerfectionist(ch->GetPlayerID()) || !IsPlayerBotEquipmentCandidate(ch, offer))
			return false;
		const int wearCell = offer->FindEquipCell(ch);
		return IsPlayerBotReadyGearSlot(wearCell) &&
				IsPlayerBotReadyGearProto(ch, offer->GetProto(), offer->GetVnum(), wearCell) &&
				offer->GetRefineLevel() >= PLAYERBOT_READY_GEAR_MIN_PLUS;
	}

	bool ManagePlayerBotRefining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextRefineCheckTime)
			return false;

		state.dwNextRefineCheckTime = dwNow + PLAYERBOT_REFINE_INTERVAL;

		// Iwakura's Perfectionist spends at most PERFECT_BUDGET_PERCENT of what
		// it walked into town with ("max 80% yang"), and keeps the rest.
		const bool personaOn = IsPlayerBotPersonaEnabled();
		if (personaOn && state.persona.llVisitGoldStart > 0 &&
				(long long)ch->GetGold() * 100 <
					state.persona.llVisitGoldStart * (100 - playerbot_persona::PERFECT_BUDGET_PERCENT))
		{
			PlayerBotLogThrottled("perfectionist_budget", dwNow,
					"PLAYERBOT_PERSONA: perfectionist budget spent pid=%u name=%s gold=%lld start=%lld",
					ch->GetPlayerID(), ch->GetName(), (long long)ch->GetGold(),
					state.persona.llVisitGoldStart);
			return false;
		}

		// Collect all upgradable worn items and inventory candidates
		struct TRefineCandidate
		{
			BYTE wearCell;
			LPITEM item;
			BYTE plusLevel;
			BYTE priority;
		};

		std::vector<TRefineCandidate> candidates;
		// The class's own level-30 weapon goes to the anvil first, ahead of
		// everything the Perfectionist's order ranks (community patch 2,
		// point 1: "ABSOLUTNY PRIORYTET").
		LPITEM classLevel30 = personaOn ? FindPlayerBotClassLevel30Weapon(ch) : NULL;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0 ||
					!IsPlayerBotWornItemSound(ch, item, wearSlots[i]))
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;
			// A worn piece is taken off for the anvil below, so a step the bag
			// cannot pay for - a material short, or the fee - is not queued at
			// all. It was: the engine refused the attempt after the unequip, the
			// equipment pass put the piece back, and the next blacksmith tick
			// took it off again, every three seconds for the whole visit
			// ("refine SKIPPED ... materials=30057:2/21,27799:1/0" beside
			// "equipped upgrade wear=0 old_vnum=0", bandyciaras, 14 September).
			if (!CanPlayerBotAttemptRefineItem(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = wearSlots[i];
			cand.item = item;
			cand.plusLevel = plusLevel;
			// The Perfectionist's order: the weapon, the armour, the shield,
			// then the rest (GetPlayerBotPerfectionistRank).
			cand.priority = item == classLevel30 ? 0 : 1 + (personaOn ? GetPlayerBotPerfectionistRank(ch, item)
					: (coreProgression ? 0 : 2));
			// A medal dropper that met its goal: the weapon first from level
			// thirty, the armour first under it (community patch 2, point 4).
			if (personaOn && state.persona.bMedalGoalDone && cand.priority != 0 &&
					wearSlots[i] == (ch->GetLevel() >= PLAYERBOT_MEDAL_GOAL_WEAPON_FIRST_LEVEL ? WEAR_WEAPON : WEAR_BODY))
				cand.priority = 1;
			candidates.push_back(cand);
		}

		// Also collect candidate gear in inventory
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			// What the merchant would take on the next town visit is not
			// worth a refine now: Ametystowy Naszyjnik+0 was raised to +1 at
			// 17:58 and sold for scrap at 18:19. A spare that is kept - an
			// upgrade, a higher tier than the worn piece, a reserve at +6 -
			// is worth raising; the rest is scrap and stays at what it is.
			// The planner asks the same function, above.
			LPITEM item = ch->GetInventoryItem(cell);
			// "Na 341 broni na serwerze praktycznie wszystkie sa +0 (max +2),
			// boty ich wcale nie ulepszaja" (Iwakura, 13 September). Four
			// different rules can pass a bag piece over here and from outside
			// they look identical - which is why that report could be neither
			// confirmed nor explained from any log. A level-30 weapon is the
			// one the market watches, so when one is passed over it says which
			// rule did it, once a minute for the whole population.
			if (!IsPlayerBotRefineBagCandidate(ch, item))
			{
				if (IsPlayerBotSpecialLevel30Weapon(item))
					PlayerBotLogThrottled("refine_l30_skipped", dwNow,
							"PLAYERBOT_AI: level-30 weapon not refined pid=%u name=%s vnum=%u plus=%u level=%u reason=not_a_candidate junk=%d spare=%d upgrade=%d",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
							(unsigned int)item->GetRefineLevel(), (unsigned int)ch->GetLevel(),
							IsPlayerBotJunkItem(ch, item) ? 1 : 0,
							IsPlayerBotHigherTierSpare(ch, item) ? 1 : 0,
							IsPlayerBotWearableUpgrade(ch, item, item->GetCell()) ? 1 : 0);
				continue;
			}

			const BYTE plusLevel = item->GetRefineLevel();
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
			{
				if (IsPlayerBotSpecialLevel30Weapon(item))
					PlayerBotLogThrottled("refine_l30_target", dwNow,
							"PLAYERBOT_AI: level-30 weapon not refined pid=%u name=%s vnum=%u plus=%u level=%u reason=target_reached target=%u",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
							(unsigned int)plusLevel, (unsigned int)ch->GetLevel(),
							(unsigned int)GetPlayerBotRefineTarget(ch, item));
				continue;
			}

			TRefineCandidate cand;
			cand.wearCell = 255;
			cand.item = item;
			cand.plusLevel = plusLevel;
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			cand.priority = item == classLevel30 ? 0 : 1 + (personaOn ? GetPlayerBotPerfectionistRank(ch, item)
					: (coreProgression ? 0 : 2));
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
			// The market Perfectionist looks at the counters first (community
			// patch 2, point 11): a finished +8 or +9 for this slot there, and
			// the anvil waits PLAYERBOT_READY_GEAR_WAIT_MS for the purchase;
			// nothing there, or the wait over, and it refines as ever. The
			// class's level-30 weapon is its own rule's.
			if (personaOn && item != classLevel30 && IsPlayerBotMarketPerfectionist(ch->GetPlayerID()))
			{
				const int slot = wearCell != 255 ? (int)wearCell : item->FindEquipCell(ch);
				if (IsPlayerBotReadyGearSlot(slot))
				{
					TPlayerBotPersona& p = state.persona;
					if ((p.dwReadyGearCheckedAt == 0 || dwNow - p.dwReadyGearCheckedAt >= PLAYERBOT_READY_GEAR_RECHECK_MS) &&
							PlayerBotMarketHasReadyGear(ch, slot))
					{
						p.dwReadyGearCheckedAt = dwNow;
						p.dwReadyGearWaitUntil = dwNow + PLAYERBOT_READY_GEAR_WAIT_MS;
						sys_log(0, "PLAYERBOT_MARKET: perfectionist looks for a finished piece pid=%u name=%s level=%u slot=%d worn_plus=%d",
								ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), slot,
								ch->GetWear((WORD)slot) ? (int)ch->GetWear((WORD)slot)->GetRefineLevel() : -1);
					}
					if (p.dwReadyGearWaitUntil != 0 && dwNow < p.dwReadyGearWaitUntil)
						continue;
				}
			}
			// The level-30 weapon's purchase and anvil share one budget:
			// PLAYERBOT_LEVEL30_BUDGET_PERCENT of the purse the visit began with.
			if (item == classLevel30 && state.persona.llVisitGoldStart > 0 &&
					(long long)ch->GetGold() * 100 <
						state.persona.llVisitGoldStart * (100 - PLAYERBOT_LEVEL30_BUDGET_PERCENT))
			{
				PlayerBotLogThrottled("refine_l30_budget", dwNow,
						"PLAYERBOT_AI: level-30 weapon budget spent pid=%u name=%s vnum=%u plus=%u gold=%lld start=%lld",
						ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel,
						(long long)ch->GetGold(), state.persona.llVisitGoldStart);
				continue;
			}
			const bool hasBackup = (wearCell != 255) ? HasPlayerBotBackupGear(ch, wearCell) : true;

			if (wearCell == WEAR_WEAPON && !hasBackup && plusLevel >= 4 && ch->GetGold() < 5000)
				continue;

			if (plusLevel >= 5 && !hasBackup && ch->GetGold() < 15000)
				continue;

			if (plusLevel == 4 && !hasBackup && number(1, 100) > 75)
				continue;

			// Asked of every piece at the attempt, worn or not: the step before
			// this one may have spent the fee or the material this one counted
			// on, and a specimen the Biologist is still owed is no material at
			// all (GetPlayerBotBiologistReserve) - DoRefine would take it anyway.
			if (!CanPlayerBotAttemptRefineItem(ch, item))
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
			// Only a piece in this bot's own bag goes to the anvil. DoRefine
			// removes what it burns by its cell, and a piece in a slot the engine
			// does not count as worn is removed from nowhere - the slot keeps
			// pointing at a destroyed item (IsPlayerBotWornItemSound).
			if (item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
					item->GetCell() >= PLAYERBOT_BAG_CELLS || ch->GetInventoryItem(item->GetCell()) != item)
				continue;

			// DoRefine(false) is the regular blacksmith path: it reads refine_proto,
			// charges the exact fee, consumes every required material and applies the
			// normal success/failure roll.  The return value only says that an attempt
			// was performed, so compare the result item count to log its real outcome.
			const int resultCountBefore = ch->CountSpecifyItem(nextVnum);
			// What the recipe asks and what the bag holds, taken before the
			// attempt takes it. "The bot refined to +8 without Orkowe Jadra" was
			// read off a bag after the refine had consumed them (jaksiezabic,
			// 14 September), and nothing in the log could say otherwise: need/have
			// by vnum, "none" for a step whose recipe names no material.
			char materials[128] = "none";
			if (const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet()))
			{
				size_t used = 0;
				for (int m = 0; m < recipe->material_count && used + 24 < sizeof(materials); ++m)
					used += snprintf(materials + used, sizeof(materials) - used, "%s%u:%d/%d",
							m ? "," : "", (unsigned int)recipe->materials[m].vnum, (int)recipe->materials[m].count,
							(int)ch->CountSpecifyItem(recipe->materials[m].vnum));
			}
			// With a Blessing Scroll in the bag and a level worth protecting, go
			// the scroll's way: the engine reads the scroll from the cell set by
			// SetRefineMode, spends it, and on failure hands back the item one
			// level down rather than nothing.
			int scrollCell = -1;
			// A prize piece may ask for a scroll at any plus, not only from +6.
			// The +6 rule is about not spending a scarce scroll on an ordinary
			// item; a piece the bot refuses to risk needs one wherever it
			// stands, and without this it was refused a scroll below +6 and
			// then held for want of one - the deadlock that parked 451 weapons
			// on +4.
			// And the piece in the bot's hands, at a step that can burn it, goes
			// under a scroll whatever its plus. GetPlayerBotRefineTarget reads a
			// scroll in the bag as a ladder to +9, and then the +4 and +5 steps -
			// eighty and sixty percent - went to the plain anvil and burned the
			// worn weapon: "biegaja do kowala, pala swoj glowny item" with 55 000
			// Blessing Scrolls in the bags of 1100 bots (uxietoszef). A spare in
			// the bag keeps the +6 rule: its burn is the price of not spending a
			// scarce scroll on it.
			const TRefineTable* stepRecipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			const bool wornStepCanBurn = wearCell != 255 && stepRecipe &&
					stepRecipe->prob <= PLAYERBOT_WORN_SCROLL_MAX_PROB;
			// Every rule above gives way to the operator's floor: under
			// SCROLL_FROM no scroll goes on the step, whatever the piece.
			const bool scrollStepAllowed = IsPlayerBotScrollStepAllowed(plusLevel);
			const int stepProb = stepRecipe ? (int)stepRecipe->prob : 100;
			// The level-30 weapons (Tieru, 15 September). From
			// PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE a weapon goes under a scroll at
			// every step and never to the plain anvil - past the operator's floor
			// too, or under SCROLL_FROM it could never be refined at all. A
			// level-30 weapon under that line is ground towards +9 at the anvil,
			// under a scroll only where the step is a real risk - and under
			// PLAYERBOT_LEVEL30_SCROLL_LOW_AVERAGE not before the step to +5,
			// however low the family's odds run below it.
			const bool scrollOnly = IsPlayerBotScrollOnlyWeapon(item);
			const bool level30Grind = !scrollOnly && IsPlayerBotSpecialLevel30Weapon(item);
			// The weapon in the hand - or the one going back into it, since the
			// session keeps it in the bag - at a step that can burn it, with no
			// backup and nothing a merchant sells at its level: under a scroll or
			// not at all (IsPlayerBotWornWeaponAtRisk). CanPlayerBotAttemptRefineItem
			// above already refused it without a scroll; this is the scroll's half.
			const bool handAtRisk = IsPlayerBotWornWeaponAtRisk(ch, item, true) ||
					IsPlayerBotWornArmourAtRisk(ch, item);
			if (scrollOnly)
				scrollCell = FindPlayerBotRefineScrollCell(ch, plusLevel, stepProb);
			else if (level30Grind)
			{
				// The operator's table (GetPlayerBotLevel30AnvilCeiling): below
				// the ceiling the bot grinds at the anvil and takes the burn
				// risk, at or above it the step is a scroll's. The better the
				// average, the lower the ceiling - what is being protected is
				// the roll, not the plus.
				const long average = SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS);
				const int anvilCeiling = GetPlayerBotLevel30AnvilCeiling(average);
				const bool aboveCeiling = (int)plusLevel >= anvilCeiling;
				// A common roll is worth a gamble even above its ceiling: the
				// weapon is everywhere and the scroll is not.
				const bool cheapGamble = aboveCeiling &&
						average <= PLAYERBOT_LEVEL30_ANVIL_AVG_CHEAP &&
						number(1, 100) <= PLAYERBOT_LEVEL30_CHEAP_ANVIL_PERCENT;
				if (aboveCeiling && !cheapGamble && scrollStepAllowed)
					scrollCell = FindPlayerBotRefineScrollCell(ch, plusLevel, stepProb);
				if (aboveCeiling && !cheapGamble && scrollCell < 0)
				{
					// Waiting for a scroll is the point of the ceiling: the
					// anvil here is how a good roll is lost.
					PlayerBotLogThrottled("refine_l30_ceiling", dwNow,
							"PLAYERBOT_AI: level-30 weapon waits for a scroll pid=%u name=%s vnum=%u plus=%u avg=%ld ceiling=%d prob=%d",
							ch->GetPlayerID(), ch->GetName(), oldVnum,
							(unsigned int)plusLevel, average, anvilCeiling, stepProb);
					continue;
				}
			}
			else if (scrollStepAllowed &&
					(plusLevel >= PLAYERBOT_SCROLL_REFINE_MIN_PLUS || IsPlayerBotPrizeItem(item) ||
						wornStepCanBurn || handAtRisk))
				scrollCell = FindPlayerBotRefineScrollCell(ch, plusLevel, stepProb);
			if (scrollCell < 0 && scrollOnly)
			{
				PlayerBotLogThrottled("refine_scroll_only", dwNow,
						"PLAYERBOT_AI: refine held, scroll-only weapon and no scroll pid=%u name=%s vnum=%u plus=%u prob=%d",
						ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, stepProb);
				continue;
			}
			if (scrollCell < 0 && scrollStepAllowed && handAtRisk)
				continue;
			// No scroll, a roll that can fail, and a weapon worth more than the
			// next plus: leave it. The blacksmith burns what he fails. A level-30
			// weapon under the scroll-only line is not held any more: grinding it
			// at the anvil and buying the next one is what the operator asked for.
			// Only where a scroll may go at all: a piece held for a scroll the
			// floor forbids is held for good, the shape of the deadlock that
			// once parked 451 weapons on +4. Under SCROLL_FROM it takes the
			// plain anvil's odds like everything else, which is the setting.
			if (scrollCell < 0 && scrollStepAllowed && !level30Grind && IsPlayerBotPrizeItem(item))
			{
				const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
				// Hold only where a failure really costs something. Ninety and
				// eighty percent are not odds worth freezing a weapon over, and
				// freezing it is what happened: every step of refine_proto is
				// under a hundred, so "prob < 100" held every prize item at
				// whatever plus it happened to have.
				if (prt && prt->prob < PLAYERBOT_PRIZE_SAFE_REFINE_PROB)
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
				// Only a refine that landed is news. A scroll's failure hands the
				// piece back a grade down and a plain one burns it, and both were
				// shouted as luck ("ulepszylem zbroje +4 na +3", Tieru, 15 September).
				if (success)
				{
					BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plusLevel + 1);
					// +8 and +9 are Iwakura's euphoria (playerbot_mood.h).
					NotePlayerBotMoodRefine(ch, (int)plusLevel + 1);
				}
				else
					// And a failure on the way to them costs a level of mood.
					NotePlayerBotMoodRefineFailure(ch, (int)plusLevel + 1,
							scrollCell >= 0 ? "downgraded" : "burned");
				sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=%d materials=%s",
						success ? "SUCCESS" : (scrollCell >= 0 ? "FAILED_DOWNGRADED" : "FAILED_BURNED"),
						ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, plusLevel + 1, scrollCell >= 0 ? 1 : 0,
						materials);
				++refinedCount;
			}
			else
			{
				sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u materials=%s (requirements/state)",
						ch->GetPlayerID(), ch->GetName(), oldVnum, plusLevel, materials);
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
	// comes back one level down instead of not at all. Never on a step under
	// the operator's SCROLL_FROM (playerbot_config.h).
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

		// "1. Bronie, 2. Zbroje, 3. Tarcze, 4. Helmy" (Community Patch 1) -
		// the order every other refine pass already used, and this one did not.
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		LPITEM best = NULL;
		BYTE bestWear = 0;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0 || item->isLocked() || item->IsExchanging() ||
					!IsPlayerBotWornItemSound(ch, item, wearSlots[i]))
				continue;
			const BYTE plus = item->GetRefineLevel();
			// The target is PLAYERBOT_SCROLL_REFINE_MAX_PLUS here by construction:
			// this pass only runs with a scroll in the bag. A scroll-only weapon
			// (IsPlayerBotScrollOnlyWeapon) is taken at any plus and past the
			// floor, since it is never raised any other way.
			const bool scrollOnly = IsPlayerBotScrollOnlyWeapon(item);
			if ((!scrollOnly && (plus < PLAYERBOT_SCROLL_REFINE_MIN_PLUS || !IsPlayerBotScrollStepAllowed(plus))) ||
					plus >= GetPlayerBotRefineTarget(ch, item))
				continue;
			if (!IsPlayerBotWearableAtLevel(ch, item->GetRefinedVnum()))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			// A scroll this step can go under, not merely one in the bag: the War
			// God scroll stops at +4.
			if (FindPlayerBotRefineScrollCell(ch, plus, (int)recipe->prob) < 0)
				continue;
			// The fee, the materials and the Biologist's reserve, as the blacksmith
			// pass asks them.
			if (!CanPlayerBotAttemptRefineItem(ch, item))
				continue;
			if (!best || plus < best->GetRefineLevel())
			{
				best = item;
				bestWear = wearSlots[i];
			}
		}
		if (!best)
			return false;
		// The scroll the blacksmith pass would put on the same step - the
		// Dragon God from PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS - rather than
		// whichever scroll happened to lie first in the bag.
		{
			const TRefineTable* bestRecipe = CRefineManager::instance().GetRefineRecipe(best->GetRefineSet());
			scrollCell = FindPlayerBotRefineScrollCell(ch, best->GetRefineLevel(),
					bestRecipe ? (int)bestRecipe->prob : 100);
		}
		if (scrollCell < 0)
			return false;

		// Off, refined, and back on: the engine will not touch a worn piece, and
		// the result is a new item in the same cell whatever the outcome.
		if (ch->GetEmptyInventory(best->GetSize()) < 0)
			return false;
		const DWORD oldVnum = best->GetVnum();
		const DWORD nextVnum = best->GetRefinedVnum();
		const BYTE plus = best->GetRefineLevel();
		if (!ch->UnequipItem(best) || best->IsEquipped() || best->GetWindow() != INVENTORY ||
				best->GetCell() >= PLAYERBOT_BAG_CELLS)
			return false;
		const WORD cell = best->GetCell();
		const int before = ch->CountSpecifyItem(nextVnum);
		ch->SetRefineMode(scrollCell);
		const bool attempted = ch->DoRefineWithScroll(best);
		ch->ClearRefineMode();
		LPITEM after = ch->GetInventoryItem(cell);
		if (after)
			PlayerBotEquipItem(ch, after);
		if (attempted)
		{
			const bool success = ch->CountSpecifyItem(nextVnum) > before;
			if (success)
			{
				BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plus + 1);
				NotePlayerBotMoodRefine(ch, (int)plus + 1);
			}
			else
				NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "downgraded");
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

		// Count red and blue potions
		size_t redCount = 0;
		size_t blueCount = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		// What this visit actually bought. The purchase used to be silent - the
		// only line was "misc merchant visit" - so "do the bots really buy the
		// big stock?" could only be answered by querying the database, which is
		// not a question an operator should have to take that far.
		DWORD boughtRed = 0, boughtBlue = 0;

		if (botLvl <= 10)
		{
			if (redCount < 30 && ch->GetGold() >= 300)
			{
				PlayerBotChangeGold(ch, -240);
				ch->AutoGiveItem(27001, 30); // Red Potion (S) 30x
				boughtRed += 30;
			}
			if (isMage && blueCount < 20 && ch->GetGold() >= 400)
			{
				PlayerBotChangeGold(ch, -360);
				ch->AutoGiveItem(27004, 15); // Blue Potion (S) 15x
				boughtBlue += 15;
			}
		}
		else
		{
			// Unit prices are the ones the old fixed purchases implied: 20 yang for
			// a Red Potion (M), 32 for a Blue Potion (M).
			const DWORD RED_TARGET = 800;
			const DWORD BLUE_TARGET = 600;
			// From forty the bot buys the big (D) potions, not the medium (S).
			// A level-47 bot heals in the hundreds per hit and a medium potion
			// is a sip; "na tych poziomach to juz duze potki u handlarki", as
			// the Discord put it. The unit prices follow the same rule as the
			// medium ones - what the old fixed purchases implied - scaled by the
			// proto's sell-price ratio (160/96, 480/288) and rounded up.
			const bool bBig = botLvl >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
			const DWORD RED_VNUM = bBig ? 27003 : 27002;
			const DWORD BLUE_VNUM = bBig ? 27006 : 27005;
			const DWORD RED_UNIT = bBig ? 40 : 20;
			const DWORD BLUE_UNIT = bBig ? 64 : 32;
			// And never more than the bag can hold, because AutoGiveItem does not
			// refuse a full one - it fills whatever stack has room and puts the
			// rest on the ground at the bot's feet, paid for. A stack is 200. The
			// estimate below counts the headroom of one partial stack plus every
			// free cell, which is at most what the engine will find, never more.
			const int freeCells = std::max(0, ch->GetEmptyInventory(1) < 0 ? 0 :
					CountPlayerBotFreeInventoryCells(ch));
			const DWORD redRoom = (DWORD)freeCells * 200 + (200 - redCount % 200) % 200;
			const DWORD blueRoom = (DWORD)freeCells * 200 + (200 - blueCount % 200) % 200;
			// Standing at the merchant already: fill the belt right up whatever the
			// level, because this costs nothing extra. The decision to make the
			// trip at all lives in NeedsPlayerBotPotions and is far stricter.
			// Never spend more than half the purse, so shopping can't leave the
			// bot unable to afford a refine.
			if (redCount < RED_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(RED_TARGET - redCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / RED_UNIT);
				DWORD buy = want < affordable ? want : affordable;
				if (buy > redRoom)
					buy = redRoom;
				if (buy > 0)
				{
					PlayerBotChangeGold(ch, -(int)(buy * RED_UNIT));
					ch->AutoGiveItem(RED_VNUM, buy);
					boughtRed += buy;
				}
			}
			// Skills spend SP continuously, so a warrior wants a reserve too. It
			// simply must never be the thing that forbids travelling.
			if (blueCount < BLUE_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(BLUE_TARGET - blueCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / BLUE_UNIT);
				DWORD buy = want < affordable ? want : affordable;
				if (buy > blueRoom)
					buy = blueRoom;
				if (buy > 0)
				{
					PlayerBotChangeGold(ch, -(int)(buy * BLUE_UNIT));
					ch->AutoGiveItem(BLUE_VNUM, buy);
					boughtBlue += buy;
				}
			}
		}

		if (boughtRed != 0 || boughtBlue != 0)
			sys_log(0, "PLAYERBOT_RESTOCK: bought pid=%u name=%s level=%u red=%u blue=%u "
					"had_red=%u had_blue=%u gold_left=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)boughtRed, (unsigned int)boughtBlue,
					(unsigned int)redCount, (unsigned int)blueCount,
					(int)(ch->GetGold() / 1000));

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
		// The exact ladder tier first; when the merchant does not stock it -
		// which is every tier below the bot except the three the shop carries,
		// and every tier above level 26 - the best piece it does stock, so a
		// naked slot is filled and the blacksmith can raise it to +6.
		bool bought = false;
		if (NeedsPlayerBotProgressionArmor(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionArmorVnum(ch), "armor") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_BODY, "armor") || bought;
		if (NeedsPlayerBotProgressionShield(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionShieldVnum(ch), "shield") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_SHIELD, "shield") || bought;
		if (NeedsPlayerBotProgressionHelmet(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionHelmetVnum(ch), "helmet") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_HEAD, "helmet") || bought;
		// The three slots nothing ever filled. A bot wore a bracelet, a necklace
		// or an earring only when one happened to drop for it, because no ladder
		// asked for them - so most of them went their whole lives with three
		// empty slots on the character sheet.
		if (NeedsPlayerBotProgressionWrist(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWristVnum(ch), "wrist") || bought;
		if (NeedsPlayerBotProgressionNecklace(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionNecklaceVnum(ch), "necklace") || bought;
		if (NeedsPlayerBotProgressionEarring(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionEarringVnum(ch), "earring") || bought;
		return sold || bought;
	}

	// The purse and the bag against the next step of this piece: the fee over
	// the reserve, and every material with the Biologist's share left alone.
	// Asked of an offline counter's line too (BotOfflineUnwantedLine), which
	// is why it takes the item and not its place.
	bool CanPlayerBotPayRefineStep(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe || ch->GetGold() - GetPlayerBotReservedGold(ch) <
				ch->ComputeRefineFee(recipe->cost))
			return false;

		for (int i = 0; i < recipe->material_count; ++i)
		{
			// What the Biologist is still owed is not the anvil's: an Orc Tooth
			// goes to him first and into a recipe after (Tieru, 15 September).
			if (ch->CountSpecifyItem(recipe->materials[i].vnum) -
					GetPlayerBotBiologistReserve(ch, recipe->materials[i].vnum) < recipe->materials[i].count)
				return false;
		}
		return true;
	}

	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetRefinedVnum() == 0 ||
				item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
			return false;
		if (!CanPlayerBotPayRefineStep(ch, item))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe)
			return false;
		// A weapon refined only under a scroll is no errand without one: the
		// planner asks this before it sends a bot to the blacksmith.
		if (IsPlayerBotScrollOnlyWeapon(item) &&
				FindPlayerBotRefineScrollCell(ch, item->GetRefineLevel(), (int)recipe->prob) < 0)
			return false;
		// Nor is the weapon in the hand at a step that can burn it with nothing
		// to fall back on (IsPlayerBotWornWeaponAtRisk). Under the operator's
		// SCROLL_FROM no scroll may go on the step, and the anvil's odds stand.
		if (IsPlayerBotScrollStepAllowed(item->GetRefineLevel()) &&
				(IsPlayerBotWornWeaponAtRisk(ch, item) || IsPlayerBotWornArmourAtRisk(ch, item)) &&
				FindPlayerBotRefineScrollCell(ch, item->GetRefineLevel(), (int)recipe->prob) < 0)
		{
			PlayerBotLogThrottled("refine_hand_weapon", get_dword_time(),
					"PLAYERBOT_AI: refine held, the only weapon or armour and no scroll pid=%u name=%s vnum=%u plus=%u prob=%d level=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetRefineLevel(),
					(int)recipe->prob, (unsigned int)ch->GetLevel());
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

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			// The same test the refining pass applies, not a looser one: a
			// promise the executor will refuse is a walk to town for nothing.
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotRefineBagCandidate(ch, item) &&
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
