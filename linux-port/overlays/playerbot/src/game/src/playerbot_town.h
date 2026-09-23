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
	// Iwakura's gambler, defined in playerbot_gambler.h after this file: a
	// session that takes pieces from the storekeeper and the bag to the anvil
	// and refines them for the counter. It runs inside a town visit - the
	// visit is the commitment every other pass already stands back for - so
	// the visit's phases ask it what they should do.
	bool IsPlayerBotGambling(const TPlayerBotAIState& state, DWORD dwNow);
	bool IsPlayerBotGambleStock(LPCHARACTER ch, LPITEM item);
	void CollectPlayerBotGambleMaterials(LPCHARACTER ch, std::set<DWORD>& out);
	bool ManagePlayerBotGamble(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);
	bool ContinuePlayerBotVisitAsGambler(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	bool HasPlayerBotOfflineShop(LPCHARACTER ch);
	bool SubmitPlayerBotOfflineShop(LPCHARACTER, TPlayerBotAIState&, DWORD, const char*, TShopItemTable*, BYTE);

	// The stands are the shop channel's alone. With the two channels with
	// moves a bot elsewhere that has business with one - a stand to open, its
	// own to renew or serve - asks to be moved there, waits in town while the
	// coordinator does it (the hold in CPlayerBotManager::Update), and is
	// refused here until it has arrived. Without the assignment table the
	// refusal is all there is, as it always was.
	bool EnsurePlayerBotPrivateShopChannel(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD now, const char* phase)
	{
		if (!ch)
			return false;
		if (g_bChannel == playerbot_channel_rules::SHOP_CHANNEL)
			return true;
		if (!CPlayerBotManager::instance().IsChannelTableMode())
			return false;
		const bool requested = CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID());
		if (!state.bWaitingForShopChannel)
			state.dwShopChannelWaitStarted = now;
		state.bWaitingForShopChannel = true;
		state.dwNextShopChannelRequestTime = now + PLAYERBOT_SHOP_CHANNEL_REQUEST_REFRESH_MS;
		state.dwNextShopKeepTime = now + PLAYERBOT_SHOP_CHANNEL_REQUEST_RETRY_MS;
		state.offlineShop.nextService = now + PLAYERBOT_SHOP_CHANNEL_REQUEST_RETRY_MS;
		ClearPlayerBotRoute(state, true);
		PlayerBotLogThrottled("shop_channel_request", now,
				"PLAYERBOT_CHANNEL: pid=%u name=%s asks for the shop channel (here %u) phase=%s requested=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)g_bChannel,
				phase ? phase : "unknown", requested ? 1 : 0);
		return false;
	}
#endif
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

	// Iwakura's Useful Items List (playerbot_lpp.h, after the gambler): what
	// the bot keeps rather than sells, what goes to the box, what comes out.
	bool IsPlayerBotLppKeptItem(LPCHARACTER ch, LPITEM item);
	bool IsPlayerBotLppStoredSurplus(LPCHARACTER ch, LPITEM item);
	bool IsPlayerBotLppHerb(LPITEM item);
	void CollectPlayerBotSafeboxLpp(LPCHARACTER ch, const TPlayerBotAIState& state, std::vector<WORD>& cells);
	void RefreshPlayerBotLppStored(LPCHARACTER ch, TPlayerBotPersona& p, CSafebox* box);
	void NotePlayerBotLppReleased(TPlayerBotPersona& p, DWORD itemId);
	bool IsPlayerBotLppReleased(LPCHARACTER ch, DWORD itemId);
	void NotePlayerBotLppDeposit();
	bool IsPlayerBotZielarz(LPCHARACTER ch);

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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotSurplusSkillBook(ch, item))
				continue;
			surplus += item->GetCount();
			if (surplus > keep)
				cells.push_back(cell);
		}
	}

	// The books of a build that is not this bot's, in books.
	int CountPlayerBotOtherClassBooks(LPCHARACTER ch)
	{
		int books = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetType() == ITEM_SKILLBOOK &&
					!IsPlayerBotOwnSkill(ch, GetPlayerBotSkillBookSkillVnum(item)))
				books += item->GetCount();
		}
		return books;
	}

	int CountPlayerBotSurplusSkillBooks(LPCHARACTER ch)
	{
		int surplus = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotSurplusSkillBook(ch, item))
				surplus += item->GetCount();
		}
		return surplus;
	}

	// Gear nobody bought in PLAYERBOT_SHOP_UNSOLD_SAFEBOX_STANDS stands and the
	// merchant may not have: the piece a player photographs in a bag of the
	// wrong class. Only under bag pressure, like the books - with room in the
	// bag it is still counter goods - and never a piece the bot ought to wear.
	void CollectPlayerBotSafeboxDeadStock(LPCHARACTER ch, const TPlayerBotAIState& state,
			std::vector<WORD>& cells)
	{
		cells.clear();
		if (!ch || state.mapStallUnsold.empty() || (!IsPlayerBotBagFull(ch) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_BAG_PRESSURE_FREE_CELLS))
			return;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			if (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR)
				continue;
			if (item->GetRefineLevel() <= PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE)
				continue;   // the merchant's rule has that one
			// A piece Iwakura's list let go from the box is for the market,
			// whatever it takes; sending it back down would make the two rules
			// a loop.
			if (IsPlayerBotLppReleased(ch, item->GetID()))
				continue;
			std::map<DWORD, BYTE>::const_iterator unsold = state.mapStallUnsold.find(item->GetID());
			if (unsold == state.mapStallUnsold.end() || unsold->second < PLAYERBOT_SHOP_UNSOLD_SAFEBOX_STANDS)
				continue;
			if (IsPlayerBotWearableUpgrade(ch, item, cell))
				continue;
			cells.push_back(cell);
		}
	}

	// Surplus refine materials, under bag pressure, that the bot cannot sell
	// on its own counter - no demand for them, or it cannot keep a shop.
	// What it can sell (a material somebody is short of, and the bot can open
	// a stall) stays for the counter; the anvil's reserve is never touched.
	void CollectPlayerBotSafeboxMaterials(LPCHARACTER ch, std::vector<WORD>& cells)
	{
		cells.clear();
		if (!ch || (!IsPlayerBotBagFull(ch) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_BAG_PRESSURE_FREE_CELLS))
			return;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			if (!IsPlayerBotTradeableMaterial(item))
				continue;
			// Not a refine scroll, which recipe 501 makes a material too: it is the
			// bot's own ladder or counter goods, and down there it was neither -
			// 312 of them in 177 safeboxes on the test world, 8 on the counters.
			if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
				continue;
			if (PlayerBotNeedsRefineMaterial(ch, item->GetVnum()) ||
					!IsPlayerBotSurplusMaterial(ch, item))
				continue;
			// A material somebody is short of is the counter's - while the bag
			// can hold it. A counter lists a few lines, and a keeper of forty
			// held 38 stacks of them in a bag of 94 cells with four items in the
			// safebox, so every trip out ended a minute later as "no free
			// column" (BlocksPlayerBotTravel) and its horse trial never began
			// (xXxKacperxXx, m2zip, 17 September). A full bag deposits them; the
			// withdrawal brings them back for the counter only while the bag
			// stays clear of pressure.
			if (GetPlayerBotLedgerDemand(item->GetVnum()) > 0 && PlayerBotCanOpenShop(ch) &&
					!IsPlayerBotBagFull(ch))
				continue;
			cells.push_back(cell);
		}
	}

	// Keys past what the bag keeps (IsPlayerBotSurplusTreasureKey), under bag
	// pressure. A key is small and the chest it opens may still drop, so it
	// waits in the safebox rather than going to the merchant ("chyba ze chca
	// chomikowac to warto do magazynu schowac", Tieru, 15 September); the
	// withdrawal below hands one back when a chest turns up.
	void CollectPlayerBotSafeboxKeys(LPCHARACTER ch, std::vector<WORD>& cells)
	{
		cells.clear();
		if (!ch || (!IsPlayerBotBagFull(ch) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_BAG_PRESSURE_FREE_CELLS))
			return;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			if (IsPlayerBotSurplusTreasureKey(ch, item))
				cells.push_back(cell);
		}
	}

	bool HasPlayerBotSafeboxDeposit(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		std::vector<WORD> cells;
		CollectPlayerBotSafeboxBooks(ch, cells);
		if (!cells.empty())
			return true;
		CollectPlayerBotSafeboxDeadStock(ch, state, cells);
		if (!cells.empty())
			return true;
		CollectPlayerBotSafeboxMaterials(ch, cells);
		if (!cells.empty())
			return true;
		CollectPlayerBotSafeboxKeys(ch, cells);
		if (!cells.empty())
			return true;
		CollectPlayerBotSafeboxLpp(ch, state, cells);
		return !cells.empty();
	}

	// And out of it again, the way CInputMain::SafeboxCheckout does it.
	//
	// The store was one-way for as long as it has existed, and the note above
	// said so outright: "books put in are never taken out". akhigubernator made
	// the case that a cold store with no door is a leak rather than a design -
	// the page fills and is never freed, what was junk an hour ago can be
	// wanted now, and a bot can be sitting on books it has since grown into.
	// He is right, and the rule then writes itself: take back exactly what the
	// three deposit rules above would no longer send down. This is their
	// inverse, asked item by item.
	//
	// Gear is deliberately not in it. IsPlayerBotWearableUpgrade judges a piece
	// by the bag cell it sits in, which a stored item does not have - and
	// measured on this world the safebox holds 2737 materials, 29 usables and
	// not one weapon, so the case does not arise. Materials are the whole of
	// what is down there, which is also why "it fills up and is never freed"
	// was the right complaint.
	//
	// The one exception is Iwakura's gambler (pGambler, set while a session
	// is on): it comes to the storekeeper first "by wyciagnac z magazynu
	// odlozone wczesniej przedmioty ... i ulepszacze", so it takes the pieces
	// it could work - judged by the item alone (IsPlayerBotGambleStock), the
	// bag's own tests run at the anvil - and the materials its bag pieces'
	// next steps consume.
	int WithdrawPlayerBotSafebox(LPCHARACTER ch, CSafebox* box,
			const std::set<DWORD>* pJustDeposited = NULL, TPlayerBotPersona* pGambler = NULL,
			TPlayerBotPersona* pPersona = NULL)
	{
		if (!ch || !box)
			return 0;
		std::set<DWORD> gambleMaterials;
		if (pGambler)
			CollectPlayerBotGambleMaterials(ch, gambleMaterials);
		int taken = 0;
		for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM &&
				taken < PLAYERBOT_SAFEBOX_WITHDRAW_MAX; ++pos)
		{
			if (!box->IsValidPosition(pos))
				continue;
			LPITEM item = box->Get(pos);
			if (!item)
				continue;
			// Never the kind this visit's deposit just put down. The deposit
			// runs first and the withdrawal asked its questions of the bag as
			// the deposit had left it, so a material the anvil was owed went
			// down with the whole stack and came straight back up: on m2zip on
			// 17 September 3574 of 4698 withdrawals in an hour were items the
			// same visit had deposited, one bot doing it every four minutes,
			// and the pairs of CreateItem: ITEM_ID_DUP / LoadSafebox lines in
			// syserr are that round trip seen from the database (Tieru).
			if (pJustDeposited && pJustDeposited->find(item->GetVnum()) != pJustDeposited->end())
				continue;

			bool wanted = false;
			const char* why = "";
			if (item->GetType() == ITEM_SKILLBOOK)
			{
				// No longer surplus: the skill reached Master and the keep
				// limit rose with it, or the bot finally has a skill group.
				wanted = !IsPlayerBotSurplusSkillBook(ch, item);
				why = "book";
			}
			else if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			{
				// A refine scroll the deposit took for a material before it knew
				// better: the refine pass or a counter wants it back.
				wanted = true;
				why = "scroll";
			}
			else if (IsPlayerBotTradeableMaterial(item))
			{
				// Short of it at the anvil, or the ledger says somebody is and
				// this bot can put up a counter - the exact two tests the
				// deposit uses to decide a material may go down. The ledger's
				// half only into a bag it leaves clear of the pressure the
				// deposit waits for: demand moves with every minute's ledger, and
				// a bag the withdrawal had filled sent the same stack back down
				// on the next visit - 538 of 4060 withdrawals went back inside
				// fifteen minutes on the test world, 15 September.
				if (PlayerBotNeedsRefineMaterial(ch, item->GetVnum()))
				{
					// Into a bag that stays clear of the pressure the deposit
					// waits for, exactly as the ledger's half below: a material
					// taken back into a full bag is sent down again on the next
					// visit, and that is the other half of the round trip.
					const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
					wanted = freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
					why = "anvil";
				}
				else if (GetPlayerBotLedgerDemand(item->GetVnum()) > 0 && PlayerBotCanOpenShop(ch))
				{
					const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
					wanted = freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS &&
							(PLAYERBOT_BAG_CELLS - freeAfter) * 100 < PLAYERBOT_BAG_CELLS * PLAYERBOT_BAG_FULL_PERCENT;
					why = "market";
				}
				else if (pGambler && gambleMaterials.find(item->GetVnum()) != gambleMaterials.end())
				{
					// What the gambler's pieces consume next, on the same terms.
					const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
					wanted = freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
					why = "gamble";
				}
			}
			else if (pGambler && (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR))
			{
				// A few pieces a session, into a bag that stays clear of the
				// pressure the deposit waits for, like the market's half above.
				const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
				wanted = pGambler->bGambleSafeboxTaken < PLAYERBOT_GAMBLE_SAFEBOX_TAKE &&
						IsPlayerBotGambleStock(ch, item) &&
						freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS &&
						(PLAYERBOT_BAG_CELLS - freeAfter) * 100 < PLAYERBOT_BAG_CELLS * PLAYERBOT_BAG_FULL_PERCENT;
				why = "gamble";
			}
			else if (!pGambler && IsPlayerBotPersonaEnabled() &&
					(item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR))
			{
				// Iwakura's list lets a stored piece go - outgrown, or a plain
				// copy of a family now worn at +9 - and it goes to the market
				// ("zwalnia miejsce w magazynie, wystawiajac stare zapasy na
				// rynek"), into a bag that stays clear of pressure.
				const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
				wanted = IsPlayerBotLppStoredSurplus(ch, item) &&
						freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS &&
						(PLAYERBOT_BAG_CELLS - freeAfter) * 100 < PLAYERBOT_BAG_CELLS * PLAYERBOT_BAG_FULL_PERCENT;
				why = "lpp_released";
			}
			else if (item->GetType() == ITEM_METIN && IsPlayerBotPersonaEnabled())
			{
				// A soul stone the list kept, now that the bot has a socket for it.
				wanted = CanPlayerBotSeatSoulStone(ch, item->GetVnum(), (DWORD)item->GetValue(5));
				why = "lpp_stone";
			}
			else if (item->GetType() == ITEM_MATERIAL && IsPlayerBotNonGearMaterial(item->GetVnum()))
			{
				// The herbs an older version put down as materials: out, and to
				// the merchant on the next visit (IsPlayerBotNonGearMaterial).
				// Under Iwakura's system the herbs are kept ("zachowujac je w
				// ekwipunku, a nastepnie w magazynie") for the brews of Baek-Go's
				// board, and come out for the bot that brews them, into a bag
				// that stays clear of pressure.
				if (IsPlayerBotPersonaEnabled())
				{
					const int freeAfter = CountPlayerBotFreeInventoryCells(ch) - (int)item->GetSize();
					wanted = IsPlayerBotZielarz(ch) && freeAfter > PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
					why = "zielarz";
				}
				else
				{
					wanted = true;
					why = "herb";
				}
			}
			else if (item->GetType() == ITEM_TREASURE_KEY)
			{
				// A key for a chest the bag now holds and has no key for.
				wanted = PlayerBotWantsTreasureKey(ch, item);
				why = "key";
			}
			if (!wanted)
				continue;

			// Room for this exact piece or it stays where it is; a bag filled
			// by the withdrawal would be emptied into the box on the next trip.
			const int cell = ch->GetEmptyInventory(item->GetSize());
			if (cell < 0)
				break;
			char szHint[128];
			snprintf(szHint, sizeof(szHint), "%s %u", item->GetName(),
					(unsigned int)item->GetCount());
			box->Remove(pos);
			item->AddToCharacter(ch, TItemPos(INVENTORY, (WORD)cell));
			ITEM_MANAGER::instance().FlushDelayedSave(item);
			LogManager::instance().ItemLog(ch, item, "SAFEBOX GET", szHint);
			sys_log(0, "PLAYERBOT_TOWN: safebox withdraw pid=%u name=%s vnum=%u count=%u reason=%s",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
					(unsigned int)item->GetCount(), why);
			if (pPersona && !strcmp(why, "lpp_released"))
				NotePlayerBotLppReleased(*pPersona, item->GetID());
			if (pGambler && (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR))
				++pGambler->bGambleSafeboxTaken;
			++taken;
		}
		return taken;
	}

	// The box's own stacks take what they have room for before anything takes
	// a slot of its own.
	//
	// This line's engine keeps the safebox from stacking at all
	// (ENABLE_MT2009_DISABLE_SAFEBOX_STACK in CommonDefines.h), so a player
	// merges by taking a stack out, dropping it on the other in the bag and
	// putting it back - and a bot, which only ever took the first empty slot,
	// left every visit's materials in a stack of their own: 357 split groups
	// and 683 slots wasted in 344 boxes on the test world ("boty nie lacza
	// przedmiotow w magazynie", jaksiezabic). The result here is the player's,
	// reached without the round trip: counts move, nothing is created, the
	// destination is saved before the source goes, and an emptied item is
	// destroyed the way CSafebox::MoveItem destroys one. Returns true when the
	// whole bag stack went in and the bag item is gone.
	bool TopUpPlayerBotSafeboxStacks(LPCHARACTER ch, CSafebox* box, LPITEM item)
	{
		if (!ch || !box || !item || !item->IsStackable() ||
				IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM; ++pos)
		{
			if (!box->IsValidPosition(pos))
				continue;
			LPITEM held = box->Get(pos);
			// The item's own limit (PlayerBotMaxStack): SetCount clamps to it
			// silently, so a pour measured against two hundred into a stack
			// of twenty would remove the bag stack and keep nothing of it.
			if (!held || !PlayerBotStacksTogether(held, item) ||
					(int)held->GetCount() >= PlayerBotMaxStack(held))
				continue;
			const int moved = std::min(PlayerBotMaxStack(held) - (int)held->GetCount(),
					(int)item->GetCount());
			if (moved <= 0)
				continue;
			char szHint[128];
			snprintf(szHint, sizeof(szHint), "%s %d", item->GetName(), moved);
			LogManager::instance().ItemLog(ch, item, "SAFEBOX PUT", szHint);
			held->SetCount(held->GetCount() + moved);
			ITEM_MANAGER::instance().FlushDelayedSave(held);
			if ((int)item->GetCount() <= moved)
			{
				M2_DESTROY_ITEM(item->RemoveFromCharacter());
				return true;
			}
			item->SetCount(item->GetCount() - moved);
		}
		return false;
	}

#if !defined(PLAYERBOT_ENGINE_MT2009)
	// And the stacks older visits left split in the box, poured together a
	// few at a time on the same rule. The 2.x line arranges the whole box
	// instead (ArrangeSafebox, at the end of the visit).
	int MergePlayerBotSafeboxStacks(CSafebox* box, int maxMerges)
	{
		int merged = 0;
		for (DWORD i = 0; box && i < SAFEBOX_MAX_NUM && merged < maxMerges; ++i)
		{
			if (!box->IsValidPosition(i))
				continue;
			LPITEM item = box->Get(i);
			if (!item || (int)item->GetCount() >= PlayerBotMaxStack(item))
				continue;
			for (DWORD j = i + 1; j < SAFEBOX_MAX_NUM && merged < maxMerges; ++j)
			{
				if (!box->IsValidPosition(j))
					continue;
				LPITEM other = box->Get(j);
				if (!other || !PlayerBotStacksTogether(item, other))
					continue;
				const int moved = std::min(PlayerBotMaxStack(item) - (int)item->GetCount(),
						(int)other->GetCount());
				if (moved <= 0)
					break;
				item->SetCount(item->GetCount() + moved);
				ITEM_MANAGER::instance().FlushDelayedSave(item);
				if ((int)other->GetCount() <= moved)
					M2_DESTROY_ITEM(box->Remove(j));
				else
					other->SetCount(other->GetCount() - moved);
				++merged;
				if ((int)item->GetCount() >= PlayerBotMaxStack(item))
					break;
			}
		}
		return merged;
	}
#endif

	// Into the open safebox, the way CInputMain::SafeboxCheckin does it: off the
	// character, onto the first empty slot of the grid. Returns how many books
	// went in; the rest stay in the bag as goods when the page is full.
	int DepositPlayerBotSafeboxBooks(LPCHARACTER ch, TPlayerBotAIState& state, CSafebox* box,
			int* pToppedUp = NULL, std::set<DWORD>* pDeposited = NULL)
	{
		std::vector<WORD> cells;
		CollectPlayerBotSafeboxBooks(ch, cells);
		const size_t books = cells.size();
		std::vector<WORD> dead;
		CollectPlayerBotSafeboxDeadStock(ch, state, dead);
		cells.insert(cells.end(), dead.begin(), dead.end());
		const DWORD dwNow = get_dword_time();
		// Not while gambling: the gambler came for the storekeeper's materials,
		// and whatever went down here the withdrawal below would not bring
		// back up in the same visit (pDeposited).
		std::vector<WORD> mats;
		if (!IsPlayerBotGambling(state, dwNow))
			CollectPlayerBotSafeboxMaterials(ch, mats);
		cells.insert(cells.end(), mats.begin(), mats.end());
		std::vector<WORD> keys;
		CollectPlayerBotSafeboxKeys(ch, keys);
		cells.insert(cells.end(), keys.begin(), keys.end());
		// And what Iwakura's list keeps for later (playerbot_lpp.h).
		const size_t lppFrom = cells.size();
		std::vector<WORD> lpp;
		CollectPlayerBotSafeboxLpp(ch, state, lpp);
		cells.insert(cells.end(), lpp.begin(), lpp.end());
		int deposited = 0;
		for (size_t i = 0; i < cells.size(); ++i)
		{
			LPITEM item = ch->GetInventoryItem(cells[i]);
			if (!item)
				continue;
			// The anvil's reserve stays in the bag, and only what is over it
			// goes down - the stack is cut here the way a counter line is cut
			// (BotOfflinePrepareLine). The whole stack used to go, reserve and
			// all, so the withdrawal below found the bot short of the very
			// material the deposit had just stored and took it straight back:
			// that round trip was three quarters of all safebox traffic on
			// m2zip and the source of the ITEM_ID_DUP lines in syserr.
			if (IsPlayerBotTradeableMaterial(item) && !IsPlayerBotSafeRefineScroll(item->GetVnum()))
			{
				const int keep = GetPlayerBotRefineMaterialReserve(ch, item->GetVnum());
				const int spare = (int)ch->CountSpecifyItem(item->GetVnum()) - keep;
				if (spare <= 0)
					continue;
				if (spare < (int)item->GetCount())
				{
					const int to = ch->GetEmptyInventory(item->GetSize());
					if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cells[i]),
							TItemPos(INVENTORY, (WORD)to), spare))
						continue;
					item = ch->GetInventoryItem(to);
					if (!item)
						continue;
				}
			}
			if (pDeposited)
				pDeposited->insert(item->GetVnum());
			const int before = (int)item->GetCount();
			if (TopUpPlayerBotSafeboxStacks(ch, box, item))
			{
				deposited += before;
				if (pToppedUp)
					++*pToppedUp;
				continue;
			}
			if ((int)item->GetCount() < before)
			{
				deposited += before - (int)item->GetCount();
				if (pToppedUp)
					++*pToppedUp;
			}
			bool placed = false;
			for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM && !placed; ++pos)
			{
				if (!box->IsValidPosition(pos) || !box->IsEmpty(pos, item->GetSize()))
					continue;
				char szHint[128];
				snprintf(szHint, sizeof(szHint), "%s %u", item->GetName(), (unsigned int)item->GetCount());
				LogManager::instance().ItemLog(ch, item, "SAFEBOX PUT", szHint);
				if (i >= lppFrom)
				{
					sys_log(0, "PLAYERBOT_LPP: to safebox pid=%u name=%s vnum=%u+%u count=%u level=%u",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
							(unsigned int)std::max(0, item->GetRefineLevel()), (unsigned int)item->GetCount(),
							(unsigned int)ch->GetLevel());
					state.mapStallUnsold.erase(item->GetID());
					state.mapStockFirstListed.erase(item->GetID());
					NotePlayerBotLppDeposit();
				}
				else if (i >= books)
				{
					// The registry says how long this piece was for sale.
					std::map<DWORD, BYTE>::const_iterator unsold = state.mapStallUnsold.find(item->GetID());
					std::map<DWORD, DWORD>::const_iterator first = state.mapStockFirstListed.find(item->GetID());
					sys_log(0, "PLAYERBOT_STOCK: to safebox pid=%u name=%s vnum=%u+%u stands_unsold=%u listed_for=%u min",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetRefineLevel(),
							unsold != state.mapStallUnsold.end() ? (unsigned int)unsold->second : 0U,
							first != state.mapStockFirstListed.end() ? (unsigned int)((dwNow - first->second) / 60000) : 0U);
					state.mapStallUnsold.erase(item->GetID());
					state.mapStockFirstListed.erase(item->GetID());
				}
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
		// The trainer and the old woman first, exactly as on the exterior list.
		// This list was Bokjung's, where neither stands, and 2.0.8 handed it to
		// Yongan and Pyongmoo as well - so a Shinsoo or Jinno bot at level
		// five, whose only errand was the trainer, began a visit with nothing
		// on the list, finished it on the same tick, and began it again on the
		// next: five hundred bots a core standing on the market pitch with
		// goal=CHOOSE_PROFESSION, reset by the watchdog every ninety seconds,
		// never past level five ("boty na 5 lv sie buguja", four reports in a
		// night). In a second village the trainer flag is never set, so this
		// costs Bokjung nothing.
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
		if (state.bTownNeedMisc)
			return BOT_TOWN_PHASE_MISC_MERCHANT;
		if (state.bTownNeedBlacksmith)
			return BOT_TOWN_PHASE_BLACKSMITH;
		return BOT_TOWN_PHASE_NONE;
	}

	void StartPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || !IsPlayerBotVillageMap(ch->GetMapIndex()))
			return;
		// Not "am I in Bokjung" but "is this a second village": what the trainer
		// and the old woman are missing from is the role, and every kingdom's
		// second village is missing them in the same way.
		const bool inM2 = IsPlayerBotM2Map(ch->GetMapIndex());
		// Which phase list: every village but Joan walks straight to each NPC.
		const bool bDirect = inM2 || !IsPlayerBotGatedVillage(ch->GetMapIndex());

		// Only where a trainer stands: a need no phase can serve would start a
		// visit that ends on the tick it began, for ever (see below).
		state.bTownNeedTrainer = !inM2 && ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN &&
				playerbot_empire_rules::HasSkillTrainers(ch->GetMapIndex());
		state.bTownNeedSkillReset = !inM2 && ShouldPlayerBotResetSkills(ch, state, dwNow);
		state.bTownNeedMisc = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_MISC) ||
				NeedsPlayerBotPotions(ch) || HasPlayerBotExcessPotions(ch) ||
				NeedsPlayerBotProgressionBoots(ch);
		state.bTownNeedWeaponMerchant = HasPlayerBotJunkForMerchant(
				ch, BOT_MERCHANT_WEAPON) || ch->GetWear(WEAR_WEAPON) == NULL ||
				NeedsPlayerBotProgressionWeapon(ch) || NeedsPlayerBotArrows(ch);
		state.bTownNeedArmorMerchant = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_ARMOR) ||
				NeedsPlayerBotProgressionArmor(ch) || NeedsPlayerBotProgressionShield(ch) ||
				NeedsPlayerBotProgressionHelmet(ch);
		state.bTownNeedBlacksmith = HasPlayerBotRefineOpportunity(ch) ||
				IsPlayerBotGambling(state, dwNow);
		// The gambler's first stop is the storekeeper, once a session.
		state.bTownNeedSafebox = HasPlayerBotSafeboxDeposit(ch, state) ||
				(IsPlayerBotGambling(state, dwNow) && !state.persona.bGambleSafeboxChecked);
		if (!state.bTownNeedTrainer && !state.bTownNeedSkillReset && !state.bTownNeedMisc &&
				!state.bTownNeedWeaponMerchant && !state.bTownNeedSafebox &&
				!state.bTownNeedArmorMerchant && !state.bTownNeedBlacksmith)
		{
			state.dwNextShopCheckTime = dwNow + number(60000, 120000);
			return;
		}

		state.bVisitingShop = true;
		// The purse the Perfectionist's share is measured against
		// (ManagePlayerBotRefining).
		state.persona.llVisitGoldStart = (long long)ch->GetGold();
		// The trader's book purse opens with the visit (community patch 2,
		// point 5).
		state.persona.llBookBudgetBase = (long long)ch->GetGold();
		state.persona.llBookBudgetSpent = 0;
		state.persona.dwBookBudgetSince = dwNow;
		// And where the level-30 weapon started it (GetPlayerBotLevel30Aim).
		{
			LPITEM classLevel30 = FindPlayerBotClassLevel30Weapon(ch);
			state.persona.bLevel30VisitStartPlus = classLevel30 ? classLevel30->GetRefineLevel() : 0xFF;
		}
		if (bDirect)
		{
			// Bokjung has no decorative gate split, and neither have Yongan and
			// Pyongmoo: visit only the specialists which are needed and then walk
			// straight back to the local hunting fields.
			state.bTownVisitPhase = GetPlayerBotFirstDirectTownPhase(state);
			// A need the list cannot serve is not a visit. Without this a visit
			// began and finished on the same tick, and the manager began it
			// again on the next - the bot stood on the spot for good, claiming
			// every tick. Back off the way "nothing needed" does, and say so.
			if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
			{
				PlayerBotLogThrottled("town_direct_empty", dwNow,
						"PLAYERBOT_TOWN: nothing on the direct list pid=%u name=%s map=%ld trainer=%d misc=%d weapon=%d armor=%d smith=%d safebox=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
						state.bTownNeedTrainer ? 1 : 0, state.bTownNeedMisc ? 1 : 0,
						state.bTownNeedWeaponMerchant ? 1 : 0, state.bTownNeedArmorMerchant ? 1 : 0,
						state.bTownNeedBlacksmith ? 1 : 0, state.bTownNeedSafebox ? 1 : 0);
				state.bVisitingShop = false;
				state.dwNextShopCheckTime = dwNow + number(60000, 120000);
				return;
			}
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

		// What the quest actually gives on this server, and nothing besides.
		//
		// Read row by row out of the server's own files: only the first herb
		// pays anything. collect_herb_lv4 ends with select_weapon_reward() - a
		// weapon per class, and the first of each list is what a player who
		// makes no choice gets. The other five rows call give_reward("herb_lvN")
		// and reward_data.lua has no such key: seventy-nine entries and not one
		// of them a biologist quest, so give_reward logs "ERROR NO QUEST REWARD
		// DATA" and hands out nothing at all.
		//
		// Until now this function invented a reward for every row - armour at
		// seven, a bracelet at ten, an earring at fifteen, a necklace at twenty,
		// a helmet at twenty-five - and gold and experience on top of all of
		// them. No player has ever been paid any of it ("boty maja miec te same
		// nagrody co gracz 1:1", Tieru). The Orc Tooth's own reward is not here
		// either: it is paid where the quest pays it, at the key-item hand-in.
		DWORD rewardItem = 0;
		if (mission.requiredLevel == 4)
		{
			// JOB_WARRIOR, JOB_ASSASSIN, JOB_SURA, JOB_SHAMAN - the quest's
			// weapon_reward_by_job: warrior {13, 3003}, ninja {1003, 2003},
			// sura {13}, shaman {7003}.
			const DWORD herbWeapons[4] = { 13, 1003, 13, 7003 };
			if (ch->GetJob() <= JOB_SHAMAN)
				rewardItem = herbWeapons[ch->GetJob()];
		}

		if (rewardItem != 0)
			ch->AutoGiveItem(rewardItem, 1, -1, false);
		// Kept as fields rather than deleted: a row whose quest does carry a
		// reward_data entry can fill them in without this function changing.
		if (mission.rewardGold > 0)
			PlayerBotChangeGold(ch, mission.rewardGold);
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
		if (!pc || completeState == PLAYERBOT_QUEST_STATE_UNKNOWN)
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
		// The other owner: the visit that finishes an errand gives the place
		// back, so it asks with the queue unlocked.
		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch, &missionIndex, true);
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
		// 20084 stands in all three first villages, so the hand-in is a local
		// errand in every kingdom. This test named map 21 outright and survived
		// the move to the catalog below it, which meant a Shinsoo or Jinno bot
		// could stand beside its own Biologist and still turn round.
		playerbot_empire_rules::TPoint biologistPos;
		if (!playerbot_empire_rules::GetBiologist(ch->GetMapIndex(), biologistPos))
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
		// One specimen is worth handing in, because the bot is already here.
		//
		// PLAYERBOT_BIOLOGIST_MIN_HANDIN is the threshold for the JOURNEY - it
		// is what NeedsPlayerBotM1OnlyServices asks before spending a trip from
		// the frontier - and applying it here as well meant a bot standing in
		// its own village with two Orc Teeth walked past the Biologist and kept
		// them. Measured on this world: 656 bots of thirty and up had handed in
		// nothing at all while carrying 577 teeth between them, and the counts
		// fell away sharply from three, which is where four-at-once stops
		// happening for a row the bot has outgrown and no longer hunts on
		// purpose. The quest accepts one specimen per interaction anyway, so
		// there was never anything to save them up for.
		if (!state.bVisitingBiologist && carried < 1)
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

		// biologistPos came from the guard at the top of this pass: reaching
		// here means this village has one.
		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), biologistPos.x,
				biologistPos.y, 0x42494f4cU, approachX, approachY);
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

		// The second half of a row: the key item is handed in, and the reward is
		// what the quest's own last state gives - a permanent collect affect and
		// a casket, both read from the row rather than named after the tooth.
		//
		// Paid the way affect.add_collect pays a player, which is not what a
		// bare AddAffect does: the engine's binding finds the existing affect of
		// that point, adds the new value to it and writes it back with bOverride
		// and IsCube both true. IsCube is the load-bearing one - with it false
		// AddAffect looks an affect up by TYPE alone, so paying the Curse Book's
		// attack speed would have overwritten the Orc Tooth's movement speed
		// instead of standing beside it.
		if (keyPhase)
		{
			ch->RemoveSpecifyItem(wantedVnum, 1);
			if (mission->rewardPoint != 0)
			{
				long lValue = mission->rewardPointValue;
				const CAffect* pkAffect = ch->FindAffect(AFFECT_COLLECT, mission->rewardPoint);
				if (pkAffect)
					lValue += pkAffect->lApplyValue;
				ch->AddAffect(AFFECT_COLLECT, mission->rewardPoint, lValue, 0,
						INFINITE_AFFECT_DURATION, 0, true, true);
			}
			if (mission->rewardBoxVnum != 0)
				ch->AutoGiveItem(mission->rewardBoxVnum, 1, -1, false);
			sys_log(0, "PLAYERBOT_BIOLOGIST: key item handed in pid=%u name=%s quest=%s point=%u value=+%d box=%u",
					ch->GetPlayerID(), ch->GetName(), mission->questName,
					(unsigned)mission->rewardPoint, mission->rewardPointValue,
					mission->rewardBoxVnum);
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

		// The specimens are in, but a row with a key item does not end here: it
		// waits in key_item for the key, which the quest's own kill hook drops
		// one time in five hundred once the state says so.
		if (newAccepted >= mission->requiredCount && mission->keyItemVnum != 0)
		{
			const int keyState = GetPlayerBotBiologistStateIndex(missionIndex, "key_item");
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			if (pc && keyState != PLAYERBOT_QUEST_STATE_UNKNOWN)
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

#if defined(PLAYERBOT_ENGINE_MT2009)
	// Baek-Go's crafting board (playerbot_herbalism.h), worked the way the
	// Biologist's hand-in above is: the bot walks to the NPC, stands there and
	// spends one visit on it. The walk is not decoration - crafting.create asks
	// npc.is_near(10) before it does anything, so a craft made across the town
	// would be a craft no player could have made, and the whole point of doing
	// this server-side is that a bot and a player pay the same price for the
	// same row.
	bool ManagePlayerBotHerbalist(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || state.bVisitingBiologist)
			return false;
		if (ch->GetLevel() < PLAYERBOT_HERBALISM_MIN_LEVEL)
			return false;
		// A Conqueror's errand from forty-five under Iwakura's system
		// (IsPlayerBotZielarz); a visit already walking finishes.
		if (!state.bVisitingHerbalist && !IsPlayerBotZielarz(ch))
			return false;
		// A bot in somebody's party is theirs, and the board is an errand.
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		if (!state.bVisitingHerbalist && dwNow < state.dwNextHerbalistCheckTime)
			return false;

		playerbot_empire_rules::TPoint herbalistPos;
		if (!playerbot_empire_rules::GetHerbalist(ch->GetMapIndex(), herbalistPos))
		{
			state.bVisitingHerbalist = false;
			return false;
		}

		// Is there anything to go there for? Asked before the walk, so nobody
		// crosses a village for an empty board: either the onboarding the quest
		// wants (ten Peach Blossoms) or a row the bag and the purse already
		// cover. The bottles are not counted here - they are bought at the
		// counter itself - so a row short of only those still brings the bot.
		const bool unlocked = IsPlayerBotHerbalismUnlocked(ch);
		const bool wantsOnboarding = !unlocked &&
				(int) ch->CountSpecifyItem(PLAYERBOT_HERBALISM_ONBOARD_FLOWER) >=
						PLAYERBOT_HERBALISM_ONBOARD_COUNT;
		const TCraftingItem* row = unlocked ? ChoosePlayerBotCraftRow(ch) : NULL;
		if (!wantsOnboarding && !row)
		{
			state.bVisitingHerbalist = false;
			state.dwNextHerbalistCheckTime = dwNow + number(
					PLAYERBOT_HERBALISM_VISIT_MIN_MS, PLAYERBOT_HERBALISM_VISIT_MAX_MS);
			return false;
		}
		if (CountPlayerBotFreeInventoryCells(ch) < PLAYERBOT_HERBALISM_FREE_CELLS)
		{
			state.bVisitingHerbalist = false;
			state.dwNextHerbalistCheckTime = dwNow + number(
					PLAYERBOT_HERBALISM_VISIT_MIN_MS, PLAYERBOT_HERBALISM_VISIT_MAX_MS);
			return false;
		}

		if (!state.bVisitingHerbalist)
		{
			state.bVisitingHerbalist = true;
			// The Zielarz's tenth of the purse is measured against this.
			state.persona.llHerbGoldStart = (long long)ch->GetGold();
			state.dwNextHerbalistActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_HERB: going to Baek-Go pid=%u name=%s onboarding=%d row=%u",
					ch->GetPlayerID(), ch->GetName(), wantsOnboarding ? 1 : 0,
					row ? row->vnum : 0);
		}

		SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), herbalistPos.x, herbalistPos.y,
				0x48455242U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true, false, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingHerbalist = false;
				state.dwNextHerbalistCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_HERB: route failed pid=%u name=%s from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextHerbalistActionTime == 0)
		{
			state.dwNextHerbalistActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextHerbalistActionTime)
			return true;

		// At the board at last. The onboarding first - it is what opens it -
		// and then a few crafts, because walking here for one is a walk wasted.
		int made = 0;
		if (!IsPlayerBotHerbalismUnlocked(ch))
			EnsurePlayerBotHerbalismStarted(ch);
		if (IsPlayerBotHerbalismUnlocked(ch))
		{
			for (DWORD i = 0; i < PLAYERBOT_HERBALISM_CRAFTS_PER_VISIT; ++i)
			{
				const TCraftingItem* next = ChoosePlayerBotCraftRow(ch);
				// "Nie wykorzystuje w tym celu wiecej niz 10% swoich Yang": the
				// craft's fee may not take the purse under nine tenths of what
				// the visit came with.
				if (next && IsPlayerBotPersonaEnabled() && state.persona.llHerbGoldStart > 0 &&
						(long long)ch->GetGold() - (long long)next->price <
							state.persona.llHerbGoldStart * (100 - PLAYERBOT_ZIELARZ_SPEND_PERCENT) / 100)
					break;
				if (!next || !CraftPlayerBotPotion(ch, next))
					break;
				++made;
			}
		}

		state.bVisitingHerbalist = false;
		state.dwNextHerbalistActionTime = 0;
		state.dwNextHerbalistCheckTime = dwNow + number(
				PLAYERBOT_HERBALISM_VISIT_MIN_MS, PLAYERBOT_HERBALISM_VISIT_MAX_MS);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_HERB: visit over pid=%u name=%s crafted=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), made, (long long) ch->GetGold());
		return made > 0;
	}
#else
	// r40250 has no crafting board and no Baek-Go to walk to.
	bool ManagePlayerBotHerbalist(LPCHARACTER, TPlayerBotAIState&, DWORD) { return false; }
#endif

	void FinishPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			bool completed)
	{
		// The Trader's visit is over: with a heavy purse and its own gear done
		// it may turn gambler ("moze plynnie zmienic sie w Hazardziste"). The
		// visit goes on to the storekeeper and the anvil instead of ending -
		// ended, it hands the bot to the stall, the market and the road, and
		// any of them would walk it away from the anvil before it got there.
		if (completed && ch && ContinuePlayerBotVisitAsGambler(ch, state, dwNow))
			return;
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
		// walking straight back out. See RollPlayerBotTownRest: a town
		// with four hundred bots on its map and two dozen in its square does not
		// look like a town, and the stalls that now open there have nobody to
		// stand among. Bokjung is left out on purpose - it is crowded already.
		if (completed && RollPlayerBotTownRest(ch))
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
		playerbot_empire_rules::TPoint pitch;
		if (!playerbot_empire_rules::GetTownPitch(mapIndex, pitch))
			return false;
		pitchX = pitch.x;
		pitchY = pitch.y;
		return true;
	}

	// Whether a bot's stand may stand on this map: a first village always, a
	// second village only while the operator's SHOP_M2 switch says so.
	bool IsPlayerBotShopMapAllowed(long mapIndex)
	{
		long x = 0, y = 0;
		if (!GetPlayerBotShopCentre(mapIndex, x, y))
			return false;
		return !IsPlayerBotM2Map(mapIndex) || IsPlayerBotShopsInM2Enabled();
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

	// Why this bot would keep a counter right now - PLAYERBOT_SHOP_REASON_NONE
	// when it would not. The four exceptions come first and ignore the TRADE
	// weight on purpose (see EPlayerBotShopReason); the three rolls after them
	// are what the slider moves.
	// A weapon or armour the bot holds a worse duplicate of: the slot is
	// filled by an equal-or-better worn piece, the spare is refined enough
	// to be worth a counter, and it is not itself an upgrade waiting to be
	// worn. The collector already lists such a spare; nothing opened a stall
	// for it, so a bot on two FMS +9 sat on the second for good (Ciapek,
	// 13 September).
	bool HasPlayerBotSellableSpare(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const BYTE type = item->GetType();
			if (type != ITEM_WEAPON && type != ITEM_ARMOR)
				continue;
			if (item->GetRefineLevel() < PLAYERBOT_SHOP_SPARE_MIN_REFINE)
				continue;
			// Nor the weapon kept for the day the one in the hand burns.
			if (IsPlayerBotKeptBackupWeapon(ch, item))
				continue;
			// Gear under level thirty ranks under the prize score and is capped
			// on a counter, so it cannot carry a stall on its own - a reason to
			// open for it would walk the bot to town for a stand that refuses.
			if (IsPlayerBotLowLevelGear(item))
				continue;
			if (IsPlayerBotWearableUpgrade(ch, item, cell))
				continue;
			// Nor the level-30 weapon it is grinding: no counter takes that.
			if (IsPlayerBotLevel30Project(ch, item))
				continue;
			const int wearCell = item->FindEquipCell(ch);
			if (wearCell < 0 || ch->GetWear((BYTE)wearCell) == NULL)
				continue;
			return true;
		}
		return false;
	}

	// Goods that pile up in a bag with nothing else to put them on a counter: a
	// hoard of a refine material (IsPlayerBotHoardedMaterial), keys with no
	// chest (IsPlayerBotSurplusTreasureKey), polymorph marbles. A bot on the
	// operator's screenshots carried nearly two hundred of one material, a
	// stack of keys and four marbles, lost the trade roll and put none of it up.
	bool PlayerBotWearsScrollWork(LPCHARACTER ch);

	bool HasPlayerBotHoardedGoods(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		int keys = 0, marbles = 0, chests = 0, scrolls = 0;
		const bool trader = IsPlayerBotResourceTrader(ch->GetPlayerID());
		const bool dropper = IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID()));
		std::map<DWORD, int> materials;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const int count = std::max<int>(1, item->GetCount());
			// A resource trader's Moonlight chests and refine scrolls are its
			// trade (IsPlayerBotResourceTrader): it keeps one scroll, and the
			// chests it does not open.
			if ((trader || dropper) && item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			{
				if ((chests += count) >= PLAYERBOT_SHOP_HOARD_MARBLES)
					return true;
				continue;
			}
			if (trader && IsPlayerBotSafeRefineScroll(item->GetVnum()))
			{
				scrolls += count;
				continue;
			}
			if (item->GetType() == ITEM_POLYMORPH)
			{
				if ((marbles += count) >= PLAYERBOT_SHOP_HOARD_MARBLES)
					return true;
			}
			else if (item->GetType() == ITEM_TREASURE_KEY)
			{
				if (IsPlayerBotSurplusTreasureKey(ch, item) && (keys += count) >= PLAYERBOT_SHOP_HOARD_KEYS)
					return true;
			}
			else if (IsPlayerBotTradeableMaterial(item))
				materials[item->GetVnum()] += count;
		}
		if (scrolls - (PlayerBotWearsScrollWork(ch) ? PLAYERBOT_REFINE_SCROLL_TRADER_KEEP : 0) >=
				PLAYERBOT_SHOP_HOARD_MARBLES)
			return true;
		// The reserve walks the gear, so it is asked only of what could be a
		// hoard at all.
		for (std::map<DWORD, int>::const_iterator it = materials.begin(); it != materials.end(); ++it)
			if (it->second >= PLAYERBOT_SHOP_HOARD_MIN_UNITS &&
					it->second - GetPlayerBotRefineMaterialReserve(ch, it->first) >= PLAYERBOT_SHOP_HOARD_MIN_UNITS)
				return true;
		return false;
	}

	BYTE GetPlayerBotShopReason(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_SHOP_MIN_LEVEL)
			return PLAYERBOT_SHOP_REASON_NONE;
		// A bot that cannot afford its potions sells what it has, whatever its
		// personality rolled. So does one whose bag is full: the counter is
		// where the spares a collector will not scrap can go.
		if (IsPlayerBotPoorKeeper(ch))
			return PLAYERBOT_SHOP_REASON_POOR;
		if (IsPlayerBotBagFull(ch))
			return PLAYERBOT_SHOP_REASON_BAG_FULL;
		// A valuable spare of a slot the bot already has filled is goods it
		// should put up, whatever the trade roll or bag pressure said.
		if (HasPlayerBotSellableSpare(ch))
			return PLAYERBOT_SHOP_REASON_SPARE;
		// A trader always has the stall open when it can. For everyone else it
		// stays what it was: an occasional thing one bot in ten does with a spare.
		if (IsPlayerBotMerchant(state))
			return PLAYERBOT_SHOP_REASON_MERCHANT;
		// A stock of surplus books is a counter, whatever the personality
		// rolled: a book never goes to the merchant, so the counter is the
		// only way it leaves the bag - and the roll left nine bots in ten
		// flying round the stones with bags of books. Not gated on bag
		// pressure: on a world of full bags every keeper would qualify, and
		// on one of half-empty bags none would, while the books sat either
		// way.
		// ...but the operator's slider still decides how many of them do it. This
		// clause used to return true outright, so a world whose bots had books -
		// which is every world after a few hours of Metin stones - ran whatever
		// counter share the books dictated and the TRADE weight moved nothing.
		// At the neutral weight the behaviour is what it was; at the minimum the
		// stalls actually stop.
		if ((CountPlayerBotSurplusSkillBooks(ch) >= PLAYERBOT_SHOP_BOOK_PRESSURE_MIN ||
					CountPlayerBotOtherClassBooks(ch) >= PLAYERBOT_SHOP_OTHER_CLASS_BOOK_MIN) &&
				PlayerBotWeightedRoll(
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x424f4f4bU) % 1000U,
					PLAYERBOT_SHOP_BOOK_ROLL, PLAYERBOT_WEIGHT_TRADE))
			return PLAYERBOT_SHOP_REASON_BOOKS;
		// Goods piling up with no other reason to put them out: a hoard of one
		// material, keys with no chest, polymorph marbles. The TRADE weight moves
		// this the way it moves the books.
		if (HasPlayerBotHoardedGoods(ch) &&
				PlayerBotWeightedRoll(
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x484f4152U) % 1000U,
					PLAYERBOT_SHOP_HOARD_ROLL, PLAYERBOT_WEIGHT_TRADE))
			return PLAYERBOT_SHOP_REASON_HOARD;
		if (IsPlayerBotDropper(state.bPersonality))
		{
			// A dropper whose bag is under pressure sells whatever the roll said:
			// the goods are the point of the personality, and a dropper that lost
			// the roll carried eighty books and picked up nothing.
			if (CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS)
				return PLAYERBOT_SHOP_REASON_DROPPER_PRESSURE;
			return PlayerBotWeightedRoll(
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x44524f50U) % 1000U,
					PLAYERBOT_DROPPER_SHOP_ROLL, PLAYERBOT_WEIGHT_TRADE)
					? PLAYERBOT_SHOP_REASON_DROPPER_ROLL : PLAYERBOT_SHOP_REASON_NONE;
		}
		// One bot in ten, stretched or shrunk by the TRADE weight. Drawn against a
		// thousand rather than ten so that the weight has somewhere to move: the
		// odds at the neutral 100 are the same one in ten as before, over a
		// different tenth of the population. Three in ten once the bot has
		// nothing left to buy - the same draw, so the one in ten are among them.
		return PlayerBotWeightedRoll(
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x53484f50U) % 1000U,
				IsPlayerBotFullyEquipped(ch) ? PLAYERBOT_FULL_GEAR_SHOP_ROLL : 100,
				PLAYERBOT_WEIGHT_TRADE)
				? PLAYERBOT_SHOP_REASON_ROLL : PLAYERBOT_SHOP_REASON_NONE;
	}

	bool ShouldPlayerBotKeepShop(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		return GetPlayerBotShopReason(ch, state) != PLAYERBOT_SHOP_REASON_NONE;
	}

	// What the blacksmith was paid to make this piece what it is: the fee of
	// every step from the base item to this refine, read off the engine's own
	// tables - the base proto is this vnum less the refine, each proto names
	// the next (dwRefinedVnum) and its recipe (wRefineSet) - and each step
	// charged at what it costs on average to get through it, cost * 100 /
	// prob, because a step that fails six times in ten at +7 is paid for more
	// than once, and the last failure takes the piece. That quotient is the
	// risk premium: eleven percent at +1, a hundred at +6, over two hundred at
	// +9. The walk has to land back on this vnum, or the piece is not on a
	// plain ladder and gets no floor. Materials are not counted; they have a
	// market of their own.
	//
	// Why a floor at all: the asking price scales the merchant's price of the
	// base item by the median wallet, and on a fresh world both are pennies -
	// Miecz+4 at 90 yang, a Sejmitar+4 at 582 (djariczek, 12 September), where
	// the four fees alone come to 6100. Nobody sells for less than they paid
	// the blacksmith.
	DWORD GetPlayerBotRefineInvestment(LPITEM item)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const int refine = item->GetRefineLevel();
		if (refine <= 0 || (DWORD)refine > item->GetVnum())
			return 0;
		DWORD vnum = item->GetVnum() - (DWORD)refine;
		unsigned long long total = 0;
		for (int step = 0; step < refine; ++step)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (!proto || proto->dwRefinedVnum == 0)
				return 0;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(proto->wRefineSet);
			if (!recipe || recipe->cost < 0)
				return 0;
			const int prob = std::max(1, std::min(100, (int)recipe->prob));
			total += (unsigned long long)recipe->cost * 100ULL / (unsigned long long)prob;
			vnum = proto->dwRefinedVnum;
		}
		if (vnum != item->GetVnum())
			return 0;
		return total > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (DWORD)total;
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

	// Iwakura's price competition (see PLAYERBOT_SHOP_PRICE_JITTER_PCT): a stable
	// per-keeper, per-item swing so two stalls with the same +7 do not both ask
	// the flat 150 000. Hashed on the owner and the item, so it does not flicker
	// between stands; keyed off the item's owner, so a preview with no owner (the
	// offline reprice path) is left exactly as priced.
	DWORD ApplyPlayerBotPriceCompetition(LPITEM item, DWORD price)
	{
		if (!item || price <= 1)
			return price;
		LPCHARACTER owner = item->GetOwner();
		if (!owner)
			return price;
		const DWORD span = 2U * PLAYERBOT_SHOP_PRICE_JITTER_PCT + 1U;
		const int delta = (int)(PlayerBotNavHash(owner->GetPlayerID() ^
				(item->GetVnum() * 2654435761U) ^
				((DWORD)item->GetRefineLevel() * 0x9E3779B9U)) % span) -
				(int)PLAYERBOT_SHOP_PRICE_JITTER_PCT;
		return std::max<DWORD>(1, (DWORD)((unsigned long long)price *
				(unsigned long long)(100 + delta) / 100ULL));
	}

	// The world's yang, for the inflation (PLAYERBOT_INFLATION_STEP_YANG). A
	// character not in the game holds yang too, so the sum is the database's,
	// asked on the engine's own queue (DBManager::FuncQuery answers on a later
	// tick of this thread); what the live characters hold there is as old as
	// the db core's last flush, which for a step of 2.5 billion is nothing.
	long long s_llPlayerBotWorldYang = 0;
	DWORD s_dwPlayerBotWorldYangAt = 0;

	int GetPlayerBotInflationPercent()
	{
		if (s_llPlayerBotWorldYang < PLAYERBOT_INFLATION_STEP_YANG)
			return 0;
		const long long steps = s_llPlayerBotWorldYang / PLAYERBOT_INFLATION_STEP_YANG;
		return (int)std::min<long long>(steps * PLAYERBOT_INFLATION_STEP_PERCENT,
				PLAYERBOT_INFLATION_MAX_PERCENT);
	}

	void RefreshPlayerBotWorldYang(DWORD dwNow)
	{
		if (s_dwPlayerBotWorldYangAt != 0 && dwNow - s_dwPlayerBotWorldYangAt < PLAYERBOT_INFLATION_REFRESH_MS)
			return;
		s_dwPlayerBotWorldYangAt = dwNow;
		DBManager::instance().FuncQuery([](SQLMsg* msg)
		{
			if (!msg || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
				return;
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			if (!row || !row[0])
				return;
			const int before = GetPlayerBotInflationPercent();
			const bool first = s_llPlayerBotWorldYang == 0;
			s_llPlayerBotWorldYang = std::max(0LL, strtoll(row[0], NULL, 10));
			const int after = GetPlayerBotInflationPercent();
			if (first || after != before)
				sys_log(0, "PLAYERBOT_MARKET: world yang=%lld inflation=%d%% (was %d%%)",
						s_llPlayerBotWorldYang, after, before);
		}, "SELECT COALESCE(SUM(gold),0) FROM player.player");
	}

	// Iwakura's scaling rule, from the top of his sheet: the yang drop rate
	// (the mob_gold multiplier in percent, 100 when nothing set it, the same
	// number the panel's rates page writes) against the multiplier it pays -
	// 100% x1.0, 200% x2.2, 500% x5.0 and so up to 10000% x100 - read straight
	// through between his points and proportionally outside them. Every price
	// his sheet sets goes through it: books, materials, gear, soul stones,
	// marbles and scrolls. Until v1.0 the books had a line of their own
	// (x1.1 at 100%) and the materials the bare rate.
	DWORD ScalePlayerBotIwakuraPrice(DWORD base)
	{
		if (base == 0)
			return 0;
		// The world's rate, not a yang event's boost of it (playerbot_events.h).
		const long long rate = std::max(1, GetPlayerBotPriceYangRate());
		const size_t count = sizeof(PLAYERBOT_PRICE_RATE_POINTS) / sizeof(PLAYERBOT_PRICE_RATE_POINTS[0]);
		const TPlayerBotPriceRatePoint& first = PLAYERBOT_PRICE_RATE_POINTS[0];
		const TPlayerBotPriceRatePoint& last = PLAYERBOT_PRICE_RATE_POINTS[count - 1];
		long long pct = (long long)last.iPct * rate / last.iRate;
		if (rate <= first.iRate)
			pct = (long long)first.iPct * rate / first.iRate;
		else
			for (size_t i = 1; i < count; ++i)
				if (rate <= PLAYERBOT_PRICE_RATE_POINTS[i].iRate)
				{
					const TPlayerBotPriceRatePoint& lo = PLAYERBOT_PRICE_RATE_POINTS[i - 1];
					const TPlayerBotPriceRatePoint& hi = PLAYERBOT_PRICE_RATE_POINTS[i];
					pct = lo.iPct + (long long)(hi.iPct - lo.iPct) * (rate - lo.iRate) / (hi.iRate - lo.iRate);
					break;
				}
		// And the inflation over the curve (community patch 2, point 8).
		const unsigned long long scaled = (unsigned long long)base *
				(unsigned long long)std::max(1LL, pct) / 100ULL *
				(unsigned long long)(100 + GetPlayerBotInflationPercent()) / 100ULL;
		return scaled > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : (DWORD)scaled;
	}

	// What a counter's prices were set under: the table version, the yang
	// rate and the inflation. The offline service reprices a stand whose stamp
	// differs at the catch-up pace (PLAYERBOT_OFFLINE_REPRICE_CATCHUP_MS a
	// slice), so a rate moved while the core runs - or a world that has just
	// crossed another 2.5 billion - reaches every counter in hours, not days.
	// Compared for equality only, so the inflation is mixed in above the bits
	// the version and the rate use.
	DWORD GetPlayerBotPriceGeneration()
	{
		const int rate = std::max(1, GetPlayerBotPriceYangRate());
		return (PLAYERBOT_PRICE_TABLE_VERSION * 1000000UL + (DWORD)std::min(rate, 999999)) ^
				((DWORD)(GetPlayerBotInflationPercent() / PLAYERBOT_INFLATION_STEP_PERCENT) << 24);
	}

	// Iwakura's base for a book, at this world's yang rate. The rate is the
	// mob_gold multiplier in percent (100 when nothing set it), the same
	// number the panel's rates page writes.
	DWORD GetPlayerBotBookAskingBase(DWORD dwSkill)
	{
		DWORD base = PLAYERBOT_PRIOR_BOOK_ORDINARY;
		for (size_t i = 0; i < sizeof(PLAYERBOT_BOOK_PRICES) / sizeof(PLAYERBOT_BOOK_PRICES[0]); ++i)
			if (PLAYERBOT_BOOK_PRICES[i].dwSkill == dwSkill)
			{
				base = PLAYERBOT_BOOK_PRICES[i].dwPrice;
				break;
			}
		return ScalePlayerBotIwakuraPrice(base);
	}

	// Iwakura's price for an upgrade material or anything else his sheet prices
	// by name, or zero when he has not priced this one.
	DWORD GetPlayerBotMaterialAskingBase(DWORD dwVnum)
	{
		DWORD base = 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_MATERIAL_PRICES) / sizeof(PLAYERBOT_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_MATERIAL_PRICES[i].dwVnum == dwVnum)
			{
				base = PLAYERBOT_MATERIAL_PRICES[i].dwPrice;
				break;
			}
		// Then the rest of what he prices by name: the Moonlight chest, the
		// Blessing Scroll, the horse medal, herbs, guild materials and ores.
		for (size_t i = 0; base == 0 &&
				i < sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES) / sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwVnum == dwVnum)
			{
				base = PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice;
				break;
			}
		return ScalePlayerBotIwakuraPrice(base);
	}

	// A round number, the way a person writes one. The step is the price's own
	// order of magnitude over PLAYERBOT_PRICE_ROUND_DIVISOR and the price goes
	// up to the next step, so the shape of the number survives and only the
	// tail goes: 1 591 511 becomes 1 595 000 rather than a machine's exact sum.
	// Applied to every price this file hands out - see the wrapper below.
	DWORD RoundPlayerBotPrice(DWORD price)
	{
		if (price < PLAYERBOT_PRICE_ROUND_MIN)
			return price;
		unsigned long long magnitude = 1;
		while (magnitude * 10ULL <= (unsigned long long)price)
			magnitude *= 10ULL;
		const unsigned long long step = magnitude / PLAYERBOT_PRICE_ROUND_DIVISOR;
		if (step < 2)
			return price;
		const unsigned long long rounded = ((unsigned long long)price + step - 1ULL) / step * step;
		// A price that would overflow the type it came in keeps its old value:
		// nothing on a counter is worth an arithmetic surprise.
		return rounded > 0xFFFFFFFFULL ? price : (DWORD)rounded;
	}

	// What the stones seated in a weapon or armour add to its price, in
	// percent. Two steps, both his: how many are in it, then which ones. A
	// socket holding 1 is open and one holding the broken vnum is a failed
	// insertion - his "Peknięte KD - 1.0" - so neither counts.
	int GetPlayerBotSocketStonePercent(LPITEM item)
	{
		if (!item)
			return 100;
		int seated = 0;
		int percent = 100;
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
		{
			const DWORD inSocket = (DWORD)item->GetSocket(socket);
			if (inSocket <= 2 || inSocket == PLAYERBOT_BROKEN_SOUL_STONE_VNUM)
				continue;
			++seated;
			const int kind = GetPlayerBotSoulStoneKind(inSocket);
			const int grade = GetPlayerBotSoulStoneGrade(inSocket);
			for (size_t i = 0; i < sizeof(PLAYERBOT_SOCKET_STONE_PERCENT) /
					sizeof(PLAYERBOT_SOCKET_STONE_PERCENT[0]); ++i)
				if (PLAYERBOT_SOCKET_STONE_PERCENT[i].iKind == kind &&
						PLAYERBOT_SOCKET_STONE_PERCENT[i].iGrade == grade)
				{
					percent = percent * PLAYERBOT_SOCKET_STONE_PERCENT[i].iPercent / 100;
					break;
				}
		}
		if (seated <= 0)
			return 100;
		const int counted = seated > 3 ? 3 : seated;
		return percent * PLAYERBOT_SOCKET_COUNT_PERCENT[counted] / 100;
	}

	// Iwakura's price for this weapon or armour, or zero when his sheets do
	// not carry the family. The base proto is this vnum less the refine - the
	// same arithmetic GetPlayerBotRefineInvestment walks the ladder with.
	DWORD GetPlayerBotGearAskingBase(LPITEM item)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9)
			return 0;
		const DWORD baseVnum = item->GetVnum() - refine;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == baseVnum)
			{
				const DWORD price = PLAYERBOT_GEAR_PRICES[i].adwPrice[refine];
				if (price == 0)
					return 0;
				return ScalePlayerBotIwakuraPrice(
						(DWORD)((unsigned long long)price *
							(unsigned long long)GetPlayerBotSocketStonePercent(item) / 100ULL));
			}
		return 0;
	}

	// Is this piece at a refine his sheet marks "do handlarki" - the jewellery,
	// the boots and the plain shield at +0 to +3? Such a piece keeps its
	// merchant price and never takes a counter slot, like the seven Forgetting
	// Scrolls he sends the same way.
	bool IsPlayerBotMerchantOnlyGear(LPITEM item)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return false;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9)
			return false;
		const DWORD baseVnum = item->GetVnum() - refine;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == baseVnum)
				return (PLAYERBOT_GEAR_PRICES[i].wMerchantMask & (1U << refine)) != 0;
		return false;
	}

	// A soul stone by kind and grade. His table names every +4 one by one and
	// gives the lower grades one price each, with three exceptions.
	DWORD GetPlayerBotSoulStoneAskingBase(DWORD dwVnum)
	{
		const int kind = GetPlayerBotSoulStoneKind(dwVnum);
		const int grade = GetPlayerBotSoulStoneGrade(dwVnum);
		for (size_t i = 0; i < sizeof(PLAYERBOT_SOUL_STONE_PRICES) /
				sizeof(PLAYERBOT_SOUL_STONE_PRICES[0]); ++i)
			if (PLAYERBOT_SOUL_STONE_PRICES[i].iKind == kind &&
					PLAYERBOT_SOUL_STONE_PRICES[i].iGrade == grade)
				return ScalePlayerBotIwakuraPrice(PLAYERBOT_SOUL_STONE_PRICES[i].dwPrice);
		if (grade >= 0 && grade < 5 && PLAYERBOT_SOUL_STONE_GRADE_PRICES[grade] != 0)
			return ScalePlayerBotIwakuraPrice(PLAYERBOT_SOUL_STONE_GRADE_PRICES[grade]);
		return 0;
	}

	// A polymorph marble, by the monster in socket 0. The named ones have
	// their own price; everything else is drawn from his band, once and for
	// good per marble - two counters showing the same marble at the same
	// number is exactly what the band is there to avoid.
	DWORD GetPlayerBotMarbleAskingBase(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_POLYMORPH)
			return 0;
		const DWORD mob = (DWORD)item->GetSocket(0);
		for (size_t i = 0; i < sizeof(PLAYERBOT_MARBLE_PRICES) / sizeof(PLAYERBOT_MARBLE_PRICES[0]); ++i)
			if (PLAYERBOT_MARBLE_PRICES[i].dwMob == mob)
				return ScalePlayerBotIwakuraPrice(PLAYERBOT_MARBLE_PRICES[i].dwPrice);
		const DWORD span = PLAYERBOT_MARBLE_PRICE_MAX - PLAYERBOT_MARBLE_PRICE_MIN + 1;
		return ScalePlayerBotIwakuraPrice(PLAYERBOT_MARBLE_PRICE_MIN +
				PlayerBotNavHash(item->GetID() ^ 0x4d41524cU) % span);
	}

	// A Forgetting Scroll, by the skill in socket 0. Zero means two different
	// things and both are handled by the caller: a scroll whose socket nobody
	// has set yet (a drop nobody has aimed at a skill), and the seven skills
	// his sheet marks "do sprzedazy u handlarki" - those are the merchant's.
	DWORD GetPlayerBotForgetScrollAskingBase(LPITEM item)
	{
		if (!item || item->GetVnum() != PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return 0;
		const DWORD skill = (DWORD)item->GetSocket(0);
		for (size_t i = 0; i < sizeof(PLAYERBOT_FORGET_SCROLL_PRICES) /
				sizeof(PLAYERBOT_FORGET_SCROLL_PRICES[0]); ++i)
			if (PLAYERBOT_FORGET_SCROLL_PRICES[i].dwSkill == skill)
				return ScalePlayerBotIwakuraPrice(PLAYERBOT_FORGET_SCROLL_PRICES[i].dwPrice);
		return 0;
	}

	// Is this one of the seven scrolls his sheet sends to the merchant rather
	// than to a counter? Only ever true for a scroll whose skill is known.
	bool IsPlayerBotMerchantOnlyForgetScroll(LPITEM item)
	{
		if (!item || item->GetVnum() != PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return false;
		const DWORD skill = (DWORD)item->GetSocket(0);
		if (skill == 0)
			return false;
		for (size_t i = 0; i < sizeof(PLAYERBOT_FORGET_SCROLL_PRICES) /
				sizeof(PLAYERBOT_FORGET_SCROLL_PRICES[0]); ++i)
			if (PLAYERBOT_FORGET_SCROLL_PRICES[i].dwSkill == skill)
				return PLAYERBOT_FORGET_SCROLL_PRICES[i].dwPrice == 0;
		return false;
	}

	DWORD GetPlayerBotShopAskingPriceRaw(LPITEM item)
	{
		if (!item)
			return 1;
		// The sale memory is read below before the step limiter would notice a
		// new yang rate, so the rate is checked here first as well.
		ForgetPlayerBotPricesOnRateChange();
		// What is rolled on this particular piece, worked out once and applied to
		// every way out of this function. The three refine prices below are flat
		// by design - a +7 has no merchant price to scale - and returning them
		// unscaled is what left boots +7 with five bonus lines on a counter at
		// the same 150 000 as boots +7 with none.
		const int bonusPercent = GetPlayerBotBonusPricePercent(item);
		const BYTE refine = item->GetRefineLevel();
		// And what it cost to make, under every way out of here - the flat
		// prices, the scrap price, the prior, and the number the memory and
		// the step limiter finally settle on.
		const DWORD investment = GetPlayerBotRefineInvestment(item);
		// Iwakura's own sheet for this family and this refine, where he has
		// priced it - 147 families, every one of them resolved to a vnum by
		// the generator rather than by hand. It wins over the three flat
		// prices below, which were invented ("napisane z palca") precisely
		// because the item tables carry no price for a refined weapon: a
		// Zatruty Miecz +9 and a Miecz +9 both asked 900 000 before this.
		// The stones seated in it are already in the number.
		const DWORD gearBase = GetPlayerBotGearAskingBase(item);
		if (gearBase != 0)
			return ApplyPlayerBotPriceCompetition(item,
					ApplyPlayerBotBonusPremium(std::max(gearBase, investment), bonusPercent));
		if (refine >= 9)
			return ApplyPlayerBotPriceCompetition(item, ApplyPlayerBotBonusPremium(std::max(PLAYERBOT_SHOP_PRICE_PLUS9, investment), bonusPercent));
		if (refine == 8)
			return ApplyPlayerBotPriceCompetition(item, ApplyPlayerBotBonusPremium(std::max(PLAYERBOT_SHOP_PRICE_PLUS8, investment), bonusPercent));
		if (refine == 7)
			return ApplyPlayerBotPriceCompetition(item, ApplyPlayerBotBonusPremium(std::max(PLAYERBOT_SHOP_PRICE_PLUS7, investment), bonusPercent));
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
					std::max(std::max<DWORD>(1, npcUnit * PLAYERBOT_SCRAP_PRICE_MULT), investment),
					bonusPercent);
		DWORD unit = npcUnit * PLAYERBOT_SHOP_MATERIAL_MARKUP;
		if (bLevel30)
			unit = std::max(unit, PLAYERBOT_PRIOR_LEVEL30_WEAPON);
		// The opening prices. Blended away by the sale memory below as real
		// transactions accumulate - a prior is where a price starts, not where
		// it stays.
		// A book asks Iwakura's price for its skill (PLAYERBOT_BOOK_PRICES),
		// at the world's yang rate; the wallet block below is not for it.
		// A material Iwakura has priced by hand wins over every prior below it,
		// pearls and the shell included - his table covers those three too.
		const DWORD materialBase = GetPlayerBotMaterialAskingBase(item->GetVnum());
		// The two other things his sheets price by hand: a polymorph marble by
		// the monster in its socket, and a Forgetting Scroll by the skill in
		// its own. Both are worth what he says whatever the merchant thinks -
		// a marble sold for three hundred yang before there was a table.
		const DWORD iwakuraBase = GetPlayerBotMarbleAskingBase(item) +
				GetPlayerBotForgetScrollAskingBase(item);
		if (bookSkill != 0)
			unit = GetPlayerBotBookAskingBase(bookSkill);
		else if (IsPlayerBotGeneralSkillBook(item->GetVnum()))
			unit = ScalePlayerBotIwakuraPrice(PLAYERBOT_PRIOR_BOOK_ORDINARY *
					(item->GetVnum() >= 50304 ? PLAYERBOT_GENERAL_BOOK_PRICE_MULT_COMBO
						: PLAYERBOT_GENERAL_BOOK_PRICE_MULT_LEADERSHIP));
		else if (materialBase != 0)
			unit = materialBase;
		else if (iwakuraBase != 0)
			unit = iwakuraBase;
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
		// An item-shop head, at the price of the coins it cost
		// (PLAYERBOT_PRIOR_ISHOP_HAIRSTYLE), whatever the wallets say.
		const bool hairstyle = item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR;
		if (hairstyle)
			unit = ScalePlayerBotIwakuraPrice(PLAYERBOT_PRIOR_ISHOP_HAIRSTYLE);
		// A soul stone has no merchant price: the counter asks by grade. His
		// table names every +4 by kind and three of the lower ones; the old
		// per-grade array stays for a stone he has not priced.
		if (item->GetType() == ITEM_METIN)
		{
			const DWORD stoneBase = GetPlayerBotSoulStoneAskingBase(item->GetVnum());
			unit = stoneBase != 0 ? stoneBase
					: PLAYERBOT_SHOP_PRICE_SOUL_STONE[std::min(4, GetPlayerBotSoulStoneGrade(item->GetVnum()))];
		}

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
		if (wallet > 0 && item->GetType() != ITEM_METIN && bookSkill == 0 &&
				materialBase == 0 && iwakuraBase == 0 && !hairstyle)
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
		// And never under what the blacksmith was paid.
		unit = std::max(unit, investment);
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
		// The memory may pull the number to a quarter of the prior and the
		// limiter drifts from wherever the last counter had it; neither goes
		// under the fees.
		unit = std::max(unit, investment);
		// A book's market has some noise in it: a fifth under to a quarter
		// over, drawn per listing. After the limiter and the memory, so the
		// anchor they keep is the table's number and not one draw of it.
		// The same draw for a hand-priced material: Iwakura asks for it on both
		// tables, so two counters never show the same number for a Zab Orka
		// either.
		if (bookSkill != 0 || materialBase != 0 || iwakuraBase != 0)
			unit = std::max<DWORD>(1, (DWORD)((unsigned long long)unit *
					(unsigned long long)number(PLAYERBOT_BOOK_PRICE_JITTER_MIN, PLAYERBOT_BOOK_PRICE_JITTER_MAX) / 100ULL));
		unit = ApplyPlayerBotBonusPremium(unit, bonusPercent);
		const DWORD price = unit * (DWORD)item->GetCount();
		return price == 0 ? 1U : price;
	}

	// The one door out. The function above has five ways to return - the three
	// flat refine prices, the scrap price and the settled one - and rounding
	// only the last of them would have left exactly the prices players look at
	// (+7, +8, +9) as raw sums. Every caller goes through here.
	DWORD GetPlayerBotShopAskingPrice(LPITEM item)
	{
		return RoundPlayerBotPrice(GetPlayerBotShopAskingPriceRaw(item));
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
		// Iwakura's Grinder on its first horse: "a nadmiar medali sprzedaje".
		if (ch && ch->GetHorseLevel() >= 1 && IsPlayerBotGrinderRider(ch))
			return true;
		// Anything over the keep is goods for everybody. Without this a bot on a
		// horse of exactly ten past level thirty-five - a battle-horse candidate,
		// which may spend no medal at all - was refused by both halves of the
		// rule and carried whatever it found: forty medals in one player's bag.
		if (ch && (int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) >
				PLAYERBOT_HORSE_MEDAL_KEEP)
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
		// The player's floor first (PLAYERBOT_MARKET_LOCAL_FLOOR_UNITS): this
		// village's own counters, not the core's.
		if (IsPlayerBotVillageMap(ch->GetMapIndex()) &&
				GetPlayerBotMarketLocalSupply(ch->GetMapIndex(), item->GetVnum()) < PLAYERBOT_MARKET_LOCAL_FLOOR_UNITS)
			decision = PLAYERBOT_LIST_FLOOR;
		else if (demand == 0)
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

	// Is any worn piece still short of what a scroll can take it to?
	bool PlayerBotWearsScrollWork(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		static const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM worn = ch->GetWear(wearSlots[i]);
			if (worn && worn->GetRefinedVnum() != 0 &&
					worn->GetRefineLevel() < PLAYERBOT_SCROLL_REFINE_MAX_PLUS)
				return true;
		}
		return false;
	}

	// The scrolls a bot keeps for its own anvil: PLAYERBOT_REFINE_SCROLL_KEEP
	// while a worn piece can still use one (one bot in five, the resource
	// trader, keeps a single), none once nothing worn wants a scroll.
	int GetPlayerBotRefineScrollKeep(LPCHARACTER ch)
	{
		if (!ch || !PlayerBotWearsScrollWork(ch))
			return 0;
		return IsPlayerBotResourceTrader(ch->GetPlayerID())
				? PLAYERBOT_REFINE_SCROLL_TRADER_KEEP : PLAYERBOT_REFINE_SCROLL_KEEP;
	}

	// Scrolls lying in cells before this one: the ones the keep counts first.
	int CountPlayerBotSafeRefineScrollsAhead(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held->GetCell() == cell && IsPlayerBotSafeRefineScroll(held->GetVnum()))
				ahead += std::max<int>(1, held->GetCount());
		}
		return ahead;
	}

	// Units of this item's own vnum in the cells before it: a keep counts those
	// first, whatever the stall splits the stacks into.
	int CountPlayerBotVnumUnitsAhead(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held->GetCell() == cell && held->GetVnum() == item->GetVnum())
				ahead += std::max<int>(1, held->GetCount());
		}
		return ahead;
	}

	int ScorePlayerBotShopStock(LPCHARACTER ch, LPITEM item, bool merchant, bool report)
	{
		if (!item)
			return -1;
		// The operator's word first: stall goes up ahead of everything, the
		// other three never do.
		{
			const BYTE policy = GetPlayerBotItemPolicy(item);
			if (policy == PLAYERBOT_ITEM_POLICY_STALL)
				return PLAYERBOT_SHOP_POLICY_STALL_SCORE;
			if (policy != PLAYERBOT_ITEM_POLICY_NONE)
				return -1;
		}
		// A retired item is nobody's goods (IsPlayerBotRetiredItem).
		if (IsPlayerBotRetiredItem(item->GetVnum()))
			return -1;
		// Nor is a piece Iwakura's list keeps for the storekeeper
		// (playerbot_lpp.h): only its copies past the keep are for sale.
		if (ch && IsPlayerBotLppKeptItem(ch, item))
			return -1;
		// A Kamien Duchowy is every bot's own to train with, up to its keep
		// (GetPlayerBotCountedGoodsKeep); a stack holding a stone over the keep
		// is goods, and the cut takes only what is over it (BotOfflinePrepareLine,
		// the classic stall's MayListWhole). Counting only the stones in the cells
		// before a stack, as this did, never let a bot's one stack go: ten in a
		// stack against a keep of three were ten kept.
		if (item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return playerbot_stall_rules::HoldsSpare(CountPlayerBotVnumUnitsAhead(ch, item),
					(int)item->GetCount(), GetPlayerBotCountedGoodsKeep(ch, item)) ? 800 : -1;
		if (item->GetType() == ITEM_POLYMORPH || IsPlayerBotMetinDetector(item->GetVnum()))
			return PLAYERBOT_SHOP_POLYMORPH_SCORE;
		// A bonus stone over what the bot keeps for its own rerolling, above the
		// books and below the materials, the way a marble sits. On this world it
		// reaches no counter and that is the engine's word, not this branch's:
		// 71084, 71085 and the ItemShop copies 76023/76024 all carry
		// ITEM_ANTIFLAG_MYSHOP, so CollectPlayerBotShopItems drops them at its
		// first line and a player cannot stand one on their own counter either.
		// The rule is written by subtype rather than by vnum so a stone without
		// the flag - a green 71151/71152, or any of them if an operator ever
		// clears it in item_proto - is goods the day it appears.
		if (IsPlayerBotBonusStoneItem(item))
			return playerbot_stall_rules::HoldsSpare(CountPlayerBotVnumUnitsAhead(ch, item),
					(int)item->GetCount(), GetPlayerBotCountedGoodsKeep(ch, item))
					? PLAYERBOT_SHOP_BONUS_STONE_SCORE : -1;
		// Seven of the Forgetting Scrolls are marked "do sprzedazy u
		// handlarki" on Iwakura's sheet - the ones whose skill nobody buys a
		// scroll for. They keep their merchant price and never take a counter
		// slot from something that would sell.
		if (IsPlayerBotMerchantOnlyForgetScroll(item))
			return merchant ? 400 : -1;
		// A weapon from the level-30 set is the prize of this whole market. It is
		// worth a counter slot at any refine at all, unrefined included - except
		// the one its keeper is grinding towards +9 itself.
		// A level-30 weapon the bot means to grind is not goods: the operator's
		// share (PLAYERBOT_LEVEL30_KEEP_PERCENT) keeps most of them for the
		// anvil and lists the rest, which is why 2603 of them stood on the
		// counters at +0 on 17 September while 28 bots wore one.
		if (IsPlayerBotSpecialLevel30Weapon(item))
		{
			// One of another class stays in the bag while the anvil can take
			// it towards its ceiling (PlayerBotRefinesLevel30ForSale), and is
			// goods the moment it cannot.
			if (PlayerBotRefinesLevel30ForSale(ch, item) && CanPlayerBotAttemptRefineItem(ch, item))
				return -1;
			return PlayerBotKeepsLevel30ForAnvil(ch, item) ? -1 : 2000;
		}
		// Iwakura's fifty-four weapons at +0..+3 stand on the bots' counters
		// PLAYERBOT_JUNK_WEAPON_MARKET_CAP at a time, world-wide.
		if (IsPlayerBotCappedJunkWeapon(item) && IsPlayerBotJunkWeaponMarketFull())
			return -1;
		// Gear under level thirty goes up at +6 or better and ranks under the
		// materials whatever is rolled on it, and one counter carries only
		// PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES of it (CollectPlayerBotShopItems).
		// Asked before the bonus and the precious refine below, both of which
		// used to wave a +4 armour for level 26 through to the top of the list.
		if (IsPlayerBotLowLevelGear(item))
			return item->GetRefineLevel() >= GetPlayerBotLowGearMinRefine(item)
					? PLAYERBOT_SHOP_LOW_GEAR_SCORE + item->GetRefineLevel() : -1;
		// A piece of Iwakura's list past what the list keeps, at any refine:
		// the gamblers' stock (community patch 2, point 9).
		if (ch && IsPlayerBotLppSurplusGoods(ch, item))
			return std::max<int>(PLAYERBOT_SHOP_LPP_SURPLUS_SCORE,
					HasPlayerBotValuableBonus(item) ? 1500 : 0) + item->GetRefineLevel();
		// Then anything rolled with a bonus a player would go looking for.
		if (HasPlayerBotValuableBonus(item))
			return 1500;
		// Below that, a piece his sheet sends to the merchant stays off the
		// counter: a bracelet +2 with no line worth having is not goods.
		if (IsPlayerBotMerchantOnlyGear(item))
			return merchant ? 400 : -1;
		// A spare at +6 or better is worth walking across town for, and is the one
		// thing that must never reach an NPC merchant for a fifth of its worth.
		if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			return 1000 + item->GetRefineLevel();
		// A Blessing or Dragon God scroll is the bot's own ladder to +9 (it
		// lifts GetPlayerBotRefineTarget while it is in the bag), so the first
		// PLAYERBOT_REFINE_SCROLL_KEEP stay while a worn piece can still use
		// one; the rest are goods - another bot needs them too. Counted by cell
		// order, because the stall splits a stack into singles first.
		// Asked before the materials: the Blessing Scroll is also what recipe 501
		// consumes, so it used to take the material branch below and the ledger
		// decided it - no demand, no line - and never reached this rule, the one
		// 2.0.31 wrote for the resource traders. Measured on the test world on
		// 15 September: 8 scrolls on 978 counters, 312 in 177 safeboxes, and
		// "nadal po update stan sklepow z bodziami: 0" (sizowski).
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
		{
			// One bot in five keeps a single scroll rather than three, so the
			// scrolls reach the market instead of sitting in bags until every
			// worn piece is at +9 - which for a bot that keeps re-gearing is
			// never ("zaden bot nie sprzedaje zwojow blogoslawienstwa").
			// The keep is a count of scrolls, not of cells before this one: a
			// stack is goods when it and the scrolls ahead of it hold more than
			// the keep, and the cut (GetPlayerBotStallBaseKeep) leaves the keep
			// in it. Asking only whether enough lay *ahead* kept a bot's one
			// stack whole whatever its size, and the service visit never
			// splits: 294 bots held 1 405 scrolls, 289 of them in one stack,
			// 116 of those over the keep, and 5 stood on the counters of the
			// whole world ("A bodzi jak nie bylo tak nie ma", 16 September).
			const int keep = GetPlayerBotRefineScrollKeep(ch);
			if (CountPlayerBotSafeRefineScrollsAhead(ch, item) + (int)item->GetCount() <= keep)
				return -1;
			return 800;
		}
		// A material this bot is short of stays in its own bag.
		// And nothing out of the reserve its own anvil wants: only what is
		// over it, by at least one pack, is goods.
		if (IsPlayerBotTradeableMaterial(item) &&
				(int)ch->CountSpecifyItem(item->GetVnum()) -
					GetPlayerBotRefineMaterialReserve(ch, item->GetVnum()) < PLAYERBOT_SHOP_PACK_UNITS)
			return -1;
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
			// A hoard goes up whatever the ledger says, in packs of ten
			// (IsPlayerBotHoardedMaterial): held back, it was held for good.
			const bool hoard = IsPlayerBotHoardedMaterial(ch, item);
			const int decision = DecidePlayerBotMaterialListing(ch, item, report && !hoard);
			if (decision == PLAYERBOT_LIST_LIST)
				return 500;
			// Under the player's floor: ahead of spare gear
			// (PLAYERBOT_SHOP_FLOOR_SCORE says why).
			if (decision == PLAYERBOT_LIST_FLOOR)
				return PLAYERBOT_SHOP_FLOOR_SCORE;
			if (decision == PLAYERBOT_LIST_PROBE)
				return 450;
			return hoard ? PLAYERBOT_SHOP_HOARD_SCORE : -1;
		}
		// What a player crafts or refines further: the herbalist's herbs, the
		// Crystal Earrings, the Ghost Face Armour, the level-65 weapons under +4
		// (from +4 they ranked above already), the Zen Bean and the Blood Pill.
		// The first beans stay for a rank that ever falls below zero - ten to
		// fifteen of them, counted over the whole bag (GetPlayerBotZenBeanKeep);
		// a stack holding a bean over the keep is goods, and the cut takes
		// only what is over it.
		if (item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM &&
				!playerbot_stall_rules::HoldsSpare(CountPlayerBotVnumUnitsAhead(ch, item),
					(int)item->GetCount(), GetPlayerBotCountedGoodsKeep(ch, item)))
			return -1;
		// A heap is PLAYERBOT_SHOP_BULK_MIN_UNITS at least: the service visit
		// put up whatever a cell held, and one root picked up since was a line
		// of its own - 1171 single herbs on the counters of m2zip.
		if (IsPlayerBotBulkGoods(item) &&
				(int)ch->CountSpecifyItem(item->GetVnum()) < PLAYERBOT_SHOP_BULK_MIN_UNITS)
			return -1;
		// A herb is Baek-Go's material now, so a few stay home. Without this a
		// keeper listed the lot and then stood at the board with nothing to
		// craft - the counters already held 12 506 Tue Mushrooms on 17 September.
		if (IsPlayerBotHerbalismHerb(item->GetVnum()) &&
				(int) ch->CountSpecifyItem(item->GetVnum()) <= PLAYERBOT_HERBALISM_HERB_KEEP)
			return -1;
		if (IsPlayerBotPickupGoods(item))
			return PLAYERBOT_SHOP_PICKUP_GOODS_SCORE + item->GetRefineLevel();
		// What Baek-Go's board made. A bot keeps a few for the fights that are
		// worth a ten-minute buff and the rest is stock - and it is stock worth
		// listing high, because this is the only place in the world a player can
		// buy one: nothing else crafts, and the potions have no drop table.
		// A recipe the bot has read to its ceiling is goods for the same reason.
		if (IsPlayerBotSurplusPotion(ch, item))
			return 950;
		if (IsPlayerBotSurplusRecipe(ch, item))
			return 900;
		// Hair dye. The item shop's is stock worth a slot; one from the water
		// is thrown away but for the few a bot keeps
		// (PLAYERBOT_HAIR_DYE_KEEP_PERMILLE), and those go up last.
		if (IsPlayerBotHairDye(item->GetVnum()))
		{
			if (!IsPlayerBotFishedHairDye(item->GetVnum()))
				return 900;
			return IsPlayerBotHairDyeKeptForSale(item) ? 200 : -1;
		}
		// A hairstyle the bot cannot wear: an item-shop head a keeper bought
		// for its counter (playerbot_itemshop.h). One it can wear is its own,
		// on its way to its head.
		if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR)
			return item->CanUsedBy(ch) ? -1 : PLAYERBOT_SHOP_ISHOP_HAIR_SCORE;
		// A Forgetting Scroll sells well; the keeper keeps it only while one of
		// its own skills is waiting for it.
		if (item->GetVnum() == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return GetPlayerBotStuckSkill(ch) != 0 ? -1 : 800;
		// (A Blessing or Dragon God scroll was judged here, under the materials
		// that took it first - see above the material reserve.)
		// An ITEM_MATERIAL no recipe consumes is scenery, not goods: it was put
		// up for its type, and its type is not a reason anybody would buy it -
		// unless Iwakura's sheet prices it, which is exactly that reason (the
		// branch after the books below lists it).
		if (item->GetRefinedVnum() == 0 && item->GetType() == ITEM_MATERIAL &&
				!IsPlayerBotSheetGoods(item))
			return -1;
		// A soul stone the bot cannot seat - one Iwakura's list keeps out of the
		// hunting set, no socket open, the wrong grade for the piece it keeps -
		// is somebody else's set.
		if (item->GetType() == ITEM_METIN)
			return CanPlayerBotSeatSoulStone(ch, item->GetVnum(), (DWORD)item->GetValue(5))
					? -1 : 700 + GetPlayerBotSoulStoneGrade(item->GetVnum()) * 100;
		// Sztuka Combo and the Leadership books: kept while the bot can read
		// them (a few of each), the rest goods like any other book.
		if (IsPlayerBotGeneralSkillBook(item->GetVnum()))
		{
			if (IsPlayerBotGeneralSkillBookUseful(ch, item->GetVnum()) &&
					CountPlayerBotVnumUnitsAhead(ch, item) < PLAYERBOT_GENERAL_BOOK_KEEP)
				return -1;
			return 400;
		}
		// What Iwakura's sheet prices by name and nothing above placed: the
		// horse and polymorph books and the stone detachment scroll the junk
		// rule used to sell (IsPlayerBotSheetGoods, which keeps them from the
		// merchant now - so they have to be goods here, or they would ride in
		// the bag for good). After the Combo and Leadership books, which are
		// on the sheet too and keep a few to read.
		if (IsPlayerBotSheetGoods(item))
			return PLAYERBOT_SHOP_SHEET_GOODS_SCORE;
		// Skill books. Stock for everyone; the Metin dropper's whole trade, so
		// on its counter they go up beside the level-30 weapons.
		if (item->GetType() == ITEM_SKILLBOOK)
		{
			// Its own, within what it keeps to read, stays in the bag: a counter
			// used to carry the very book its keeper was waiting to read. A stack
			// is goods once it holds a book over the keep, counted in books over
			// the skill; the line is cut from what is over it.
			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			if (!playerbot_stall_rules::HoldsSpare(CountPlayerBotSkillBooksAhead(ch, item, skillVnum),
					(int)item->GetCount(), GetPlayerBotCountedGoodsKeep(ch, item)))
				return -1;
			if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_METIN_DROPPER)
				return 1800;
			// Another build's book is somebody else's progress and nothing of
			// this bot's: ahead of the ordinary goods (community patch 2, point 5).
			return IsPlayerBotOwnSkill(ch, skillVnum) ? 400 : PLAYERBOT_SHOP_OTHER_CLASS_BOOK_SCORE;
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
			// everything worth more: fodder for a player's blacksmith runs. From
			// level thirty only - the gear under it never gets this far (see the
			// top of this function), and nothing at +4 does either.
			if (IsPlayerBotScrapKeeper(ch->GetPlayerID()) &&
					item->GetRefineLevel() < PLAYERBOT_SHOP_MIN_GEAR_REFINE)
				return 100 + item->GetRefineLevel();
			return -1;
		}

		// An unopened box. Ranked between the materials and the spare gear: it
		// is a gamble somebody might want, not a thing anybody came for.
		if (IsPlayerBotSurplusChest(ch, item))
			return 350;
		// A key with no chest for it, past the ones the bot holds on to.
		if (item->GetType() == ITEM_TREASURE_KEY)
			return IsPlayerBotSurplusTreasureKey(ch, item) ? PLAYERBOT_SHOP_KEY_SCORE : -1;
		// A specimen of a mission already handed in. The Orc Tooth never gets
		// here: it is a refine material and the material branch above priced
		// it, ledger and all.
		// (a specimen of a handed-in row is the merchant's now - see the junk
		// rule - never the counter's: "boty wystawiaja przedmioty do badan".)

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
		// Over for good: the clock ran out, the bot left Joan or may no longer
		// rest there (the REST key moved to zero, the last counter packed up),
		// it opened a stall of its own, or it is busy staying alive.
		if (dwNow >= state.dwTownLingerUntil ||
				!MayPlayerBotRestInTown(ch) ||
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

	// Iwakura's name for a counter of these goods (playerbot_shop_signs.h, which
	// comes after this file because what heads a +7..+9 piece is its price).
	bool ChoosePlayerBotShopName(LPCHARACTER ch, const std::vector<LPITEM>& goods,
			char* out, size_t outSize, const char** how);

	// Everything this bot can legitimately part with, best first. OpenMyShop
	// refuses equipped, locked and ANTI_GIVE/ANTI_MYSHOP items outright - and it
	// refuses the *whole* shop over one bad line, not just that line - so the
	// same rules are applied here rather than letting the call fail silently.
	// lowGearOnCounter is how many lines of gear under level thirty the counter
	// already holds - an offline shop's own - so the cap counts both.
	void CollectPlayerBotShopItems(LPCHARACTER ch,
			std::vector<std::pair<int, WORD> >& outScored, bool merchant,
			int lowGearOnCounter = 0)
	{
		outScored.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		// Once a minute per bot the ledger decisions are counted and the
		// refusals logged; the other scans of the same bag say nothing.
		const bool report = ShouldReportPlayerBotMarketDecisions(
				ch->GetPlayerID(), get_dword_time());
		const DWORD backupWeaponID = GetPlayerBotBackupWeaponID(ch, false);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
			if ((IsPlayerBotBiologistSpecimen(vnum) || IsPlayerBotBiologistKeyItem(vnum)) &&
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
				// Nor the weapon kept for the day the one in the hand burns.
				if (type == ITEM_WEAPON && backupWeaponID != 0 && item->GetID() == backupWeaponID)
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
		// No counter full of gear under level thirty: the best
		// PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES of it, less what the counter already
		// holds, and the rest stays in the bag for a later stand. What the
		// operator put on "stall" does not count against it.
		{
			int lowRoom = std::max(0, PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES - lowGearOnCounter);
			std::vector<std::pair<int, WORD> > kept;
			kept.reserve(outScored.size());
			for (size_t i = 0; i < outScored.size(); ++i)
			{
				LPITEM item = ch->GetInventoryItem(outScored[i].second);
				if (CountsAgainstPlayerBotLowGearCap(item) &&
						GetPlayerBotItemPolicy(item) != PLAYERBOT_ITEM_POLICY_STALL)
				{
					if (lowRoom <= 0)
						continue;
					--lowRoom;
				}
				kept.push_back(outScored[i]);
			}
			outScored.swap(kept);
		}
		// Nor a counter of one thing: PLAYERBOT_SHOP_SAME_VNUM_LINES lines of an
		// item, and PLAYERBOT_SHOP_MARBLE_LINES marbles, one a monster
		// (PLAYERBOT_SHOP_POLYMORPH_SCORE says why). An offline stand's add
		// asks its own counter too (BotOfflineCounterRefuses). An item on
		// "stall" goes up ahead of everything and is held to the same lines:
		// the operator's word is the counter instead of the merchant, not a
		// counter of nothing else - the rest waits in the bag.
		{
			int marbles = 0;
			std::set<long> mobs;
			std::map<DWORD, int> lines;
			std::vector<std::pair<int, WORD> > kept;
			kept.reserve(outScored.size());
			for (size_t i = 0; i < outScored.size(); ++i)
			{
				LPITEM item = ch->GetInventoryItem(outScored[i].second);
				if (item)
				{
					if (item->GetType() == ITEM_POLYMORPH)
					{
						if (marbles >= PLAYERBOT_SHOP_MARBLE_LINES || !mobs.insert(item->GetSocket(0)).second)
							continue;
						++marbles;
					}
					else if (IsPlayerBotSameVnumCapped(item) &&
							++lines[item->GetVnum()] > PLAYERBOT_SHOP_SAME_VNUM_LINES)
						continue;
				}
				kept.push_back(outScored[i]);
			}
			outScored.swap(kept);
		}
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

	// The piece's name without the grade it has just reached. The table names
	// every grade ("Smoczy Noz+7"), so a line that also said the grade said it
	// twice: "no i mam +7 na Pajecza Wlocznia+7" (archonek, 19 September).
	std::string PlayerBotRefineBaseName(const char* name)
	{
		std::string base = name ? name : "";
		const std::string::size_type plus = base.find_last_of('+');
		if (plus != std::string::npos && plus + 1 < base.size() &&
				base.find_first_not_of("0123456789", plus + 1) == std::string::npos)
		{
			base.erase(plus);
			while (!base.empty() && base[base.size() - 1] == ' ')
				base.erase(base.size() - 1);
		}
		return base;
	}

	// A good refine is the one moment worth breaking the bots' silence for. They
	// say nothing when attacked, nothing during PvP, and nothing on a kill -
	// only the blacksmith gets a reaction, and even then rarely.
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, DWORD resultVnum, int newPlus)
	{
		// Named from the item table: the piece the refine was asked of is gone
		// by now, and the one in its place is a new object.
		const TItemTable* resultProto = ITEM_MANAGER::instance().GetTable(resultVnum);
		if (!ch || !resultProto || newPlus < 7)
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

		// Every line is the name, then "z +6 na +7", then the reaction: the name
		// cannot be declined here, and after "na" it had to be ("no i mam +7 na
		// Smoczy Noz" is not Polish), and a verb in the past would have to agree
		// with a gender the piece does not tell us.
		static const char* kPlus7[] = {
			"%s %s, kowal dzis laskawy",
			"udalo sie! %s %s",
			"%s %s, moglo byc gorzej",
			"wbite: %s %s!!!"
		};
		static const char* kPlus8[] = {
			"%s %s! rece mi sie trzesly",
			"jest! %s %s, pchac dalej?",
			"%s %s, chyba mam dzis szczescie",
			"wbite: %s %s, idzie do roboty"
		};
		static const char* kPlus9[] = {
			"%s %s!!! nie wierze",
			"%s %s, kto by pomyslal",
			"dziewiatka! %s %s, dzis stawiam :D",
			"%s %s, chyba wystarczy tych prob na dzis"
		};

		const char** pool = kPlus7;
		if (newPlus >= 9)
			pool = kPlus9;
		else if (newPlus == 8)
			pool = kPlus8;

		char msg[CHAT_MAX_LEN + 1];
		char body[CHAT_MAX_LEN + 1];
		char step[32];
		snprintf(step, sizeof(step), "z +%d na +%d", newPlus - 1, newPlus);
		const std::string name = PlayerBotRefineBaseName(resultProto->szLocaleName);
		snprintf(body, sizeof(body), pool[number(0, 3)], name.c_str(), step);
		snprintf(msg, sizeof(msg), "%s : %s", ch->GetName(), body);

		s_dwLastShoutTime = dwNow;
		SendShout(msg, ch->GetEmpire());
		sys_log(0, "PLAYERBOT_SHOUT: pid=%u plus=%d text=%s",
				ch->GetPlayerID(), newPlus, msg);
	}

	// Where a bot belongs on each map we manage: the point that map is entered
	// by, and its own kingdom's second village for anything else.
	//
	// The fallback has to be the bot's own kingdom and not a fixed map. This is
	// the rescue for a character with no sector, and dropping a Jinno bot into
	// Bokjung because 23 was written here would make every such rescue an
	// emigration - a bot that then walks Chunjo's roads for the rest of its life
	// because nothing ever tells it to go home.
	void GetPlayerBotHomePoint(LPCHARACTER ch, long mapIndex, long& outMap, long& outX, long& outY)
	{
		const int empire = ch ? (int)ch->GetEmpire() : playerbot_empire_rules::EMPIRE_CHUNJO;
		playerbot_empire_rules::TPoint pitch;
		outMap = playerbot_empire_rules::GetHomeMap(empire, playerbot_empire_rules::MAP_ROLE_M2);
		outX = PLAYERBOT_M2_FROM_M3_X;
		outY = PLAYERBOT_M2_FROM_M3_Y;
		if (playerbot_empire_rules::GetTownPitch(outMap, pitch))
		{
			outX = pitch.x;
			outY = pitch.y;
		}
		if (IsPlayerBotVillageMap(mapIndex) || IsPlayerBotM3Map(mapIndex))
		{
			// A village or a guild map: stand where its own gate puts a traveller
			// down. Which gate depends on the role, and the owner of the map
			// answers for it - a visitor arrives by the road a local uses.
			const int owner = playerbot_empire_rules::GetMapOwnerEmpire(mapIndex);
			const playerbot_empire_rules::EMapRole role = playerbot_empire_rules::GetMapRole(mapIndex);
			const long from = playerbot_empire_rules::GetHomeMap(owner,
					role == playerbot_empire_rules::MAP_ROLE_M1 ? playerbot_empire_rules::MAP_ROLE_M2 : playerbot_empire_rules::MAP_ROLE_M1);
			playerbot_empire_rules::TKingdomGate gate;
			if (playerbot_empire_rules::FindKingdomGate(owner, from, mapIndex, gate))
			{
				outMap = mapIndex;
				outX = gate.arrival.x;
				outY = gate.arrival.y;
			}
			else if (playerbot_empire_rules::GetTownPitch(mapIndex, pitch))
			{
				outMap = mapIndex;
				outX = pitch.x;
				outY = pitch.y;
			}
			return;
		}
		// Every dungeon, not the three that were named here: Shinsoo's and
		// Jinno's fell through to the frontier branch, which knows nothing
		// about them, so a bot stranded in one was sent to a point on another
		// map entirely.
		if (IsPlayerBotMonkeyMap(mapIndex))
		{
			if (GetPlayerBotMonkeyArrival(mapIndex, outX, outY))
				outMap = mapIndex;
			return;
		}
		if (GetPlayerBotFrontierArrivalFor(ch, mapIndex, outX, outY))
			outMap = mapIndex;
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
		GetPlayerBotHomePoint(ch, mapIndex, homeMap, homeX, homeY);
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
			for (std::map<DWORD, DWORD>::iterator it = state.mapStockFirstListed.begin();
					it != state.mapStockFirstListed.end(); )
			{
				if (state.mapStallUnsold.find(it->first) == state.mapStallUnsold.end())
					state.mapStockFirstListed.erase(it++);
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
			const int units = item ? GetPlayerBotStallLineUnitsFor(ch, item) : 0;
			if (units <= 0 || item->isLocked())
				continue;
			// A scroll stack is cut down to the bot's own keep whatever its
			// size - a stack of five with a keep of three is a line of two,
			// not a line of five that leaves the anvil nothing.
			const bool scroll = IsPlayerBotSafeRefineScroll(item->GetVnum());
			if (!scroll && (int)item->GetCount() <= units)
				continue;
			const int wantLines = units == 1 ? PLAYERBOT_SHOP_SINGLE_UNITS
					: IsPlayerBotBulkGoods(item) ? PLAYERBOT_SHOP_BULK_LINES
					: scroll ? PLAYERBOT_SHOP_SCROLL_LINES
					: units == PLAYERBOT_SHOP_HOARD_PACK_UNITS ? PLAYERBOT_SHOP_HOARD_LINES
					: PLAYERBOT_SHOP_PACK_LINES;
			int lines = 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (other && other != item && (int)other->GetCount() == units &&
						PlayerBotStacksTogether(item, other))
					++lines;
			}
			// The base stack keeps the anvil's reserve (the keys the bot holds
			// on to, one unit of anything else), so a line is never cut out of
			// what the bot came to the counter to buy.
			const int keep = GetPlayerBotStallBaseKeep(ch, item);
			int split = 0;
			while (lines < wantLines &&
					(scroll ? (int)item->GetCount() - keep >= 1 : (int)item->GetCount() - units >= keep) &&
					CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS)
			{
				const int take = scroll ? std::min(units, (int)item->GetCount() - keep) : units;
				const int to = ch->GetEmptyInventory(item->GetSize());
				if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, item->GetCell()),
						TItemPos(INVENTORY, (WORD)to), (BYTE)take))
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

	// The bots' own grid is five wide; what the engine indexes by is
	// PLAYERBOT_SHOP_ENGINE_COLUMNS wide (ten on mt2009, whose right half is
	// locked or premium). Same row, same column, the engine's stride.
	int PlayerBotShopSlotToEngine(int slot)
	{
		return (slot / PLAYERBOT_SHOP_GRID_COLUMNS) * PLAYERBOT_SHOP_ENGINE_COLUMNS +
				slot % PLAYERBOT_SHOP_GRID_COLUMNS;
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
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (ch && ch->GetMyShop()) { ClosePlayerBotShop(ch, state, dwNow, "migrate_offline"); return false; }
#endif
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
					// Gone, and how fast. A line that left within
					// PLAYERBOT_MARKET_FAST_SALE_MS of going up is Iwakura's
					// "wysoki popyt": the next counter carrying this thing asks
					// more. Read here because this is the one place that knows a
					// line has sold - the item is already out of the bag, which
					// is why the offer carries the book's skill.
					std::map<DWORD, DWORD>::const_iterator listed =
							state.mapStockFirstListed.find(offer.dwItemID);
					if (listed != state.mapStockFirstListed.end() &&
							dwNow - listed->second < PLAYERBOT_MARKET_FAST_SALE_MS)
					{
						NotePlayerBotFastSale(offer.dwVnum, offer.bRefine, dwNow, offer.dwSkillVnum);
						sys_log(0, "PLAYERBOT_MARKET: fast sale pid=%u name=%s vnum=%u+%u skill=%u in=%u s",
								ch->GetPlayerID(), ch->GetName(), offer.dwVnum,
								(unsigned int)offer.bRefine, offer.dwSkillVnum,
								(unsigned int)((dwNow - listed->second) / 1000));
					}
					state.mapStockFirstListed.erase(offer.dwItemID);
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

		// The operator moved the TRADE slider while this stand was up. A stand
		// that stood on a roll is judged again under the new weight - the roll
		// is by pid, so the answer is the one a fresh open would get - and one
		// that lost packs up within PLAYERBOT_SHOP_REEVALUATE_SPREAD_MS, spread
		// by pid, rather than all of them in the same tick. The four exceptions
		// are not asked: the slider never applied to them and the UI says so.
		const DWORD dwWeightsGeneration = GetPlayerBotWeightsGeneration();
		if (state.dwShopWeightsGeneration != dwWeightsGeneration)
		{
			state.dwShopWeightsGeneration = dwWeightsGeneration;
			if (IsPlayerBotShopReasonRolled(state.bShopOpenReason) &&
					!ShouldPlayerBotKeepShop(ch, state))
			{
				const DWORD dwEndBy = dwNow + PlayerBotNavHash(ch->GetPlayerID() ^ 0x57454947U) %
						PLAYERBOT_SHOP_REEVALUATE_SPREAD_MS;
				if (dwEndBy < state.dwShopCloseTime)
				{
					state.dwShopCloseTime = dwEndBy;
					sys_log(0, "PLAYERBOT_SHOP: weights changed pid=%u name=%s reason=%s stand ends in %u s",
							ch->GetPlayerID(), ch->GetName(),
							GetPlayerBotShopReasonName(state.bShopOpenReason),
							(unsigned int)((dwEndBy - dwNow) / 1000));
				}
			}
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
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (HasPlayerBotOfflineShop(ch)) return false;
#endif
		if (!ch || !ch->IsItemLoaded())
			return false;
		// Every shop in the world stands on the first channel (the operator's
		// rule for the second one, playerbot_channel_rules.h). Without the
		// assignment table a bot on another channel never opens one; with it,
		// the bot asks to be moved once it has a reason and the goods for a
		// stand (EnsurePlayerBotPrivateShopChannel, below) - asking earlier
		// would move bots that only pass through this pass once a tick.
		if (g_bChannel != playerbot_channel_rules::SHOP_CHANNEL)
		{
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
			if (!CPlayerBotManager::instance().IsChannelTableMode())
				return false;
#else
			return false;
#endif
		}

		// The stall's own lifetime is settled earlier in the tick; by the time
		// this runs a keeper either has no shop or has already been held there.
		if (ch->GetMyShop())
			return true;

		// No map test here. There used to be one pinning stalls to Bokjung, left
		// over from when that was the only market, and it sat in front of the
		// choice below - so a bot in Joan returned before it ever got to roll, and
		// every stall in the world was still opening in Bokjung. Which towns are
		// allowed is decided by the roll and by GetPlayerBotShopCentre.

		// Errands still come first - a stall opened mid-visit would be abandoned
		// on the next tick. They, the clock and the town are asked before the
		// reason, which reads the whole bag: this pass runs on every tick of
		// every bot without a counter.
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
		// A second village takes no new stand unless the operator allows it
		// (IsPlayerBotShopMapAllowed): the stands belong in the first villages.
		if (!IsPlayerBotShopMapAllowed(ch->GetMapIndex()))
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

		const BYTE bShopReason = GetPlayerBotShopReason(ch, state);
		if (bShopReason == PLAYERBOT_SHOP_REASON_NONE)
			return false;
		// Kept from here rather than from the open itself: the walk to the pitch
		// runs this pass every tick, and the reason it acts on is this one.
		state.bShopOpenReason = bShopReason;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// This engine grants the counter at level 15 and 800 kills (CanOpenShop);
		// before that OpenMyShop refuses with a chat line nobody reads, and a
		// young world logged thirty-nine refusals in a row for no reason a
		// keeper could mend. Asked here, before the walk to the pitch.
		if (!ch->CanOpenShop())
		{
			state.dwNextShopKeepTime = dwNow + PLAYERBOT_MT2009_SHOP_NOT_YET_RETRY;
			return false;
		}
#endif

		// The cheap refusals come before the scan, the split and the walk, and
		// every one of them sets the clock. Two exits at the far end of this
		// pass - the permanent bundle, and no yang for the bundle - returned
		// with no clock at all, after the stacks had been split for the
		// counter; the stack-merge pass then poured the singles back (it comes
		// straight back after a full budget, five seconds), the wander pass
		// took a step away, and the next tick split, walked and refused again:
		// 8250 split lines in thirteen minutes from one world, ~430 bots a core
		// in Bokjung "biegaja w jedna i druga strone bez celu" (FanFar,
		// 13 September, the first run of 2.0.26). Same shape as "a pass that
		// refuses must also back off".
		if (ch->CountSpecifyItem(71049) > 0)
		{
			state.dwNextShopKeepTime = dwNow + number(600000, 900000);
			PlayerBotLogThrottled("shop_permanent_bundle", dwNow,
					"PLAYERBOT_SHOP: refused pid=%u name=%s reason=permanent_bundle",
					ch->GetPlayerID(), ch->GetName());
			return false;
		}
		{
			long long need = ch->CountSpecifyItem(50200) > 0 ? 0 : (long long)PLAYERBOT_SHOP_BUNDLE_PRICE;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
			// The offline shop's own fee and the fares the bot keeps back, the
			// same sum SubmitPlayerBotOfflineShop refuses on - asked here so a
			// keeper that cannot pay does not split and walk first. And not in
			// the first two minutes after a spawn (see the submit), for the
			// same reason.
			need += (long long)aOfflineShopTime[1].price + (long long)GetPlayerBotReservedGold(ch);
			if (dwNow - state.dwSpawnTime < 120000)
				return false;
#endif
			if ((long long)ch->GetGold() < need)
			{
				state.dwNextShopKeepTime = dwNow + number(120000, 240000);
				PlayerBotLogThrottled("shop_cannot_pay", dwNow,
						"PLAYERBOT_SHOP: refused pid=%u name=%s reason=cannot_pay gold=%lld need=%lld",
						ch->GetPlayerID(), ch->GetName(), (long long)ch->GetGold(), need);
				return false;
			}
		}

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
				IsPlayerBotPoorKeeper(ch) || IsPlayerBotBagFull(ch) ||
					bShopReason == PLAYERBOT_SHOP_REASON_HOARD))
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

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		// A reason and the goods for a stand: only now ask for the shop channel.
		if (!EnsurePlayerBotPrivateShopChannel(ch, state, dwNow, "open"))
			return false;
#endif

		// Distance and angle are both stable per bot, so a keeper returns to its
		// own pitch every time instead of the market rearranging itself.
		long offsetX = 0, offsetY = 0;
		GetPlayerBotStableOffset(ch->GetPlayerID(), 0x4d4b5450U,
				PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS,
				offsetX, offsetY);
		long stallX = pitchX + offsetX;
		long stallY = pitchY + offsetY;
		// A pitch the bot's ground does not join is not walked to. The town
		// leg moved such a goal onto the bot's own component, the walk ended
		// there, the arrival test - against the pitch - failed, and the same
		// leg was planned again for as long as the visit lasted: one keeper on
		// map 3 spent a night at it (AkhiGubernator, 12 September). Salted
		// offsets are tried first; when none joins, the stand is put off and
		// the bot goes about its business.
		{
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
			if (navigation.Init(ch->GetMapIndex()))
			{
				int tries = 0;
				while (tries < PLAYERBOT_SHOP_PITCH_TRIES &&
						!navigation.CanReach(ch->GetX(), ch->GetY(), stallX, stallY))
				{
					++tries;
					GetPlayerBotStableOffset(ch->GetPlayerID() + (DWORD)tries * 7919U,
							0x4d4b5450U ^ (DWORD)tries,
							PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS,
							offsetX, offsetY);
					stallX = pitchX + offsetX;
					stallY = pitchY + offsetY;
				}
				if (!navigation.CanReach(ch->GetX(), ch->GetY(), stallX, stallY))
				{
					state.dwNextShopKeepTime = dwNow + number(300000, 600000);
					ClearPlayerBotRoute(state, true);
					sys_log(0, "PLAYERBOT_SHOP: pitch unreachable pid=%u name=%s map=%ld from=(%ld,%ld) pitch=(%ld,%ld) tries=%d",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
							ch->GetX(), ch->GetY(), pitchX, pitchY, tries);
					return false;
				}
				if (tries > 0)
					PlayerBotLogThrottled("shop_pitch_moved", dwNow,
							"PLAYERBOT_SHOP: pitch moved pid=%u name=%s map=%ld tries=%d to=(%ld,%ld)",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), tries, stallX, stallY);
			}
		}

		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (!MovePlayerBotTownLeg(ch, state, dwNow, stallX, stallY,
				PLAYERBOT_MARKET_ARRIVE))
			return true; // still walking to the pitch
		// Counted the moment it opens rather than at the next ledger sweep, or
		// eight keepers arriving in the same minute would all read six.
#if !defined(PLAYERBOT_ENGINE_MT2009)
		if (IsPlayerBotM2Map(ch->GetMapIndex()))
			++s_iPlayerBotStallsInM2;
		++s_mapPlayerBotStallsByMap[ch->GetMapIndex()];
#endif

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
		// What the sign will be about: every line that makes the counter, for
		// Iwakura's rules (playerbot_shop_signs.h), and the best line's name for
		// the world channel. The counter is sorted best first.
		const char* pszBestName = NULL;
		std::vector<LPITEM> signGoods;
		bool grid[PLAYERBOT_SHOP_GRID_CELLS];
		memset(grid, 0, sizeof(grid));
		// What qualified and still stayed in the bag, by reason - the audit's
		// "why was it not put up": no line left on the counter, no cell of the
		// right height on the grid, an anti-flag. Said on the open line.
		unsigned int uNoLine = 0, uNoSlot = 0, uAntiFlag = 0;
		// Lines of each hoarded material on this counter.
		std::map<DWORD, int> hoardLines;
		// Units of each kind a bot keeps by count (a book's skill, the soul
		// stone) this counter already carries.
		std::map<DWORD, int> countedListed;
		for (size_t i = 0; i < scored.size(); ++i)
		{
			if (tableCount >= tableLimit)
			{
				++uNoLine;
				continue;
			}
			const WORD cell = scored[i].second;
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			// A hoard sells in packs (IsPlayerBotHoardedMaterial): the stack they
			// were cut from stays in the bag, and a counter carries
			// PLAYERBOT_SHOP_HOARD_LINES of one kind.
			if (GetPlayerBotStallLineUnitsFor(ch, item) == PLAYERBOT_SHOP_HOARD_PACK_UNITS &&
					((int)item->GetCount() > PLAYERBOT_SHOP_HOARD_PACK_UNITS ||
						++hoardLines[item->GetVnum()] > PLAYERBOT_SHOP_HOARD_LINES))
				continue;
			// A stack of a kind kept by count goes up whole here, so it goes up
			// only while what stays in the bag still holds the keep: the scorer
			// calls a stack goods once it holds one unit over it, and the split
			// above may not have cut it down that far.
			const DWORD countedKind = GetPlayerBotStallKindKey(item);
			if (countedKind && !playerbot_stall_rules::MayListWhole(
					CountPlayerBotStallKindUnits(ch, item), countedListed[countedKind],
					(int)item->GetCount(), GetPlayerBotCountedGoodsKeep(ch, item)))
				continue;
			const TItemTable* proto = item->GetProto();
			if (!proto || IS_SET(proto->dwAntiFlags,
					ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
			{
				++uAntiFlag;
				continue;
			}
			// Placed on the engine's grid by height, or the engine drops the
			// line and every buyer who comes for it is refused at an empty slot.
			const int height = std::max<int>(1, std::min<int>(PLAYERBOT_SHOP_GRID_ROWS, item->GetSize()));
			const int slot = FindPlayerBotShopSlot(grid, height);
			if (slot < 0)
			{
				++uNoSlot;
				continue;
			}
			PutPlayerBotShopSlot(grid, slot, height);
			if (state.mapStockFirstListed.find(item->GetID()) == state.mapStockFirstListed.end())
				state.mapStockFirstListed[item->GetID()] = dwNow;
			DWORD price = GetPlayerBotShopAskingPrice(item);
			// The clearance discount. Applied to the price as asked, so the sale
			// memory still learns the real price the market would have paid.
			if (bPoor)
				price = std::max<DWORD>(1, price * PLAYERBOT_SHOP_POOR_DISCOUNT_PERCENT / 100);
			// Carried home unsold before: cheaper by the stand, after the price
			// is asked so the sale memory learns the market and not the markdown.
			// Iwakura's band, drawn per listing, and capped in total - four
			// stands at his upper end would otherwise leave nothing to ask for.
			{
				std::map<DWORD, BYTE>::const_iterator unsold = state.mapStallUnsold.find(item->GetID());
				if (unsold != state.mapStallUnsold.end() && unsold->second > 0)
				{
					const int stands = std::min<int>(unsold->second, PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_STANDS);
					const int cut = std::min(PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL,
							stands * number(PLAYERBOT_MARKET_DEMAND_MIN_PERCENT,
									PLAYERBOT_MARKET_DEMAND_MAX_PERCENT));
					price = std::max<DWORD>(1, price * (100 - cut) / 100);
				}
			}
			// And the other way: a thing buyers have been taking off the counter
			// at once goes up. Applied here rather than inside the asking price
			// so the market's own anchor is not dragged along with one keeper's
			// luck - see PLAYERBOT_MARKET_DEMAND_MIN_PERCENT.
			{
				const int hot = GetPlayerBotDemandPercent(item->GetVnum(),
						item->GetRefineLevel(), dwNow, GetPlayerBotSkillBookSkillVnum(item));
				if (hot > 0)
					price = std::max<DWORD>(1, (DWORD)((unsigned long long)price *
							(unsigned long long)(100 + hot) / 100ULL));
			}
			// Neither markdown goes under what the blacksmith was paid: a
			// discount is off the margin, not off what the piece cost to make.
			price = std::max(price, GetPlayerBotRefineInvestment(item));
			table[tableCount].vnum = item->GetVnum();
			table[tableCount].count = item->GetCount();
			table[tableCount].pos = TItemPos(INVENTORY, cell);
			table[tableCount].price = price;
			table[tableCount].display_pos = (BYTE)PlayerBotShopSlotToEngine(slot);

			TPlayerBotShopOffer offer;
			offer.dwVnum = item->GetVnum();
			offer.dwPrice = price;
			offer.bRefine = item->GetRefineLevel();
			offer.wCount = item->GetCount();
			offer.dwItemID = item->GetID();
			offer.bSoldLogged = false;
			// Kept with the line because the sale is noticed when the item has
			// already left the bag, and a book's market is its skill's.
			offer.dwSkillVnum = GetPlayerBotSkillBookSkillVnum(item);
			offer.bSlot = (BYTE)PlayerBotShopSlotToEngine(slot);
			offers.push_back(offer);
			if (scored[i].first > bestScore)
				bestScore = scored[i].first;
			++tableCount;
			if (countedKind)
				countedListed[countedKind] += (int)item->GetCount();

			if (!pszBestName)
				pszBestName = proto->szLocaleName;
			signGoods.push_back(item);
		}
		// Asked again here rather than trusting the scan above: the inventory
		// moves between the two - a town errand happens in between - and a stall
		// that loses two of its three lines on the way to the pitch should stay
		// packed up rather than open with what is left.
		if (!IsPlayerBotStallWorthOpening(tableCount, bestScore, bPoor || IsPlayerBotBagFull(ch) ||
				bShopReason == PLAYERBOT_SHOP_REASON_HOARD))
		{
			state.dwNextShopKeepTime = dwNow + (tableCount == 0
					? number(300000, 600000) : number(120000, 240000));
			return false;
		}

		// The sign says what is on the counter, by Iwakura's rules over his
		// names (playerbot_shop_signs.h): a +7..+9 piece names it, otherwise the
		// kind most of its lines are, and a third of the time a neutral name
		// whatever the goods. Nothing else goes over a counter any more - no
		// "Tanio: " before an item's name, no "Wyprzedaz: " over a poor keeper's
		// counter - because "boty powinny uzywac nazw sklepow TYLKO z tej listy".
		// The clearance discount itself stays; only its word is gone.
		char sign[SHOP_SIGN_MAX_LEN + 1];
		const char* pszSignHow = "none";
		if (!ChoosePlayerBotShopName(ch, signGoods, sign, sizeof(sign), &pszSignHow))
			strlcpy(sign, playerbot_shop_names::SIGN_NAMES[0].szName, sizeof(sign));
		sys_log(0, "PLAYERBOT_SHOP: sign pid=%u name=%s lines=%u how=%s sign=\"%s\"",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)tableCount, pszSignHow, sign);

		// Opening a stall costs a shop bundle, exactly as it does for a player:
		// OpenMyShop consumes one 50200 and refuses outright without it. The other
		// accepted item, the permanent 71049, takes a branch that writes through
		// GetDesc() - a bot has no client descriptor, so that path must be avoided.
		// Both asked again at the top of the pass, before the split; kept here
		// with a clock because the bag can change on the walk to the pitch.
		if (ch->CountSpecifyItem(71049) > 0)
		{
			state.dwNextShopKeepTime = dwNow + number(600000, 900000);
			return false;
		}
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
			if (ch->GetGold() < PLAYERBOT_SHOP_BUNDLE_PRICE)
			{
				state.dwNextShopKeepTime = dwNow + number(120000, 240000);
				return false;
			}
			PlayerBotChangeGold(ch, -(int)PLAYERBOT_SHOP_BUNDLE_PRICE);
			ch->AutoGiveItem(50200, 1);
		}

#if defined(PLAYERBOT_ENGINE_MT2009)
		return SubmitPlayerBotOfflineShop(ch, state, dwNow, sign, table, tableCount);
#else
		ch->OpenMyShop(sign, table, tableCount);
#endif
		if (!ch->GetMyShop())
		{
			// OpenMyShop refuses silently, and it refuses the whole shop over one
			// bad line - so report the first item as the representative sample
			// along with how many were offered.
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			LPITEM first = ch->GetItem(table[0].pos);
			const TItemTable* rp = first ? first->GetProto() : NULL;
			sys_log(0, "PLAYERBOT_SHOP: refused pid=%u items=%u poly=%d quest=%d vnum=%u anti=%u equipped=%d locked=%d viaPos=%d sign=%d gold=%d level=%d riding=%d",
					ch->GetPlayerID(), (unsigned int)tableCount,
					ch->IsPolymorphed() ? 1 : 0,
					(pc && pc->IsRunning()) ? 1 : 0, table[0].vnum,
					rp ? rp->dwAntiFlags : 0, first && first->IsEquipped() ? 1 : 0,
					first && first->isLocked() ? 1 : 0, first ? 1 : 0,
					(int)strlen(sign), (int)(ch->GetGold() / 1000), (int)ch->GetLevel(), ch->IsHorseRiding() ? 1 : 0);
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
		state.dwShopWeightsGeneration = GetPlayerBotWeightsGeneration();
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
		{
			AddPlayerBotMarketSupply(offers[i].dwVnum, offers[i].wCount, ch->GetMapIndex());
			NotePlayerBotJunkWeaponOnCounter(offers[i].dwVnum, offers[i].wCount);
		}
		sys_log(0, "PLAYERBOT_SHOP: opened pid=%u name=%s reason=%s items=%u left_behind no_line=%u no_slot=%u antiflag=%u first_vnum=%u first_price=%u pos=(%ld,%ld) sign=\"%s\"",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotShopReasonName(state.bShopOpenReason),
				(unsigned int)tableCount, uNoLine, uNoSlot, uAntiFlag,
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
		// A goal on ground the bot's terrain does not join is walked to the
		// nearest cell of the bot's own component inside the arrival radius
		// instead (FindPlayerBotReachableGoal, which the walk to the Rybak
		// shares) - the misc merchant of Bokjung cost 260 service rescues in a
		// morning before this.
		long walkX = goalX, walkY = goalY;
		if (FindPlayerBotReachableGoal(ch, goalX, goalY, arrivalDistance, walkX, walkY))
			PlayerBotLogThrottled("town_goal_reachable", dwNow,
					"PLAYERBOT_TOWN: goal moved onto reachable ground pid=%u name=%s phase=%u goal=(%ld,%ld) walk=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
					goalX, goalY, walkX, walkY);
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

	// Joan's decorative gate, and nothing else in the world. The two sides of it
	// are separate components of server_attr even though a player runs straight
	// through the opening, which is why the walk across it is issued by hand.
	// No other village has this shape, so every other village skips the phase
	// rather than being walked to Joan's coordinates - which is exactly what a
	// Jinno bot did when this was a bare constant.
	bool HasPlayerBotTownGate(long mapIndex)
	{
		return mapIndex == PLAYERBOT_MAP_CHUNJO_M1;
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
		// Six villages, one visit. The counters, the anvil and the keeper come
		// from the map the bot is standing on - asking the table is what stops a
		// Shinsoo bot announcing "Ide do kowala" and then walking towards
		// Chunjo's anvil eighty kilometres away, which is what a pair of
		// constants named M1 and M2 made every foreign bot do.
		playerbot_empire_rules::TTownServices svc;
		if (!ch || !state.bVisitingShop ||
				!playerbot_empire_rules::GetTownServices(ch->GetMapIndex(), svc))
			return false;
		const bool inM2 = IsPlayerBotM2Map(ch->GetMapIndex());
		const bool bDirect = inM2 || !IsPlayerBotGatedVillage(ch->GetMapIndex());
		const long weaponNpcX = svc.weaponMerchant.x;
		const long weaponNpcY = svc.weaponMerchant.y;
		const long armorNpcX = svc.armourMerchant.x;
		const long armorNpcY = svc.armourMerchant.y;
		const long miscNpcX = svc.miscMerchant.x;
		const long miscNpcY = svc.miscMerchant.y;
		const long blacksmithNpcX = svc.blacksmith.x;
		const long blacksmithNpcY = svc.blacksmith.y;

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

		// A phase inside Joan's wall taken up by a bot that stands outside it.
		// A visit is paused, not ended, while the bot is in a person's party,
		// and the bot goes where the person goes - so a visit begun inside the
		// wall resumed after the party at the weapon merchant with the bot out
		// by the fields: the leg's goal was moved onto the bot's own side of the
		// wall, "arrived" there (nav_out=2) and the phase never moved, until the
		// inactivity watchdog took it ninety seconds later (MegaDzikDuch22 of
		// Sammy Suricate's world, 22 September). It crosses the gate again.
		if (!bDirect && state.bTownVisitPhase >= BOT_TOWN_PHASE_WEAPON_MERCHANT &&
				state.bTownVisitPhase <= BOT_TOWN_PHASE_BLACKSMITH_WAIT)
		{
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
			if (navigation.Init(ch->GetMapIndex()) &&
					!navigation.CanReach(ch->GetX(), ch->GetY(), weaponNpcX, weaponNpcY))
			{
				PlayerBotLogThrottled("town_outside_wall", dwNow,
						"PLAYERBOT_TOWN: outside the wall in phase %u pid=%u name=%s, crossing the gate again",
						(unsigned int)state.bTownVisitPhase, ch->GetPlayerID(), ch->GetName());
				ClearPlayerBotRoute(state, true);
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			}
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
		{
			state.bTownVisitPhase = bDirect
					? GetPlayerBotFirstDirectTownPhase(state)
					: GetPlayerBotFirstExteriorTownPhase(state);
			if (!bDirect && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			if (bDirect && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, true);
				return true;
			}
		}

		// Eight profession trainers stand south of Joan.  Their npc.txt cells are
		// 623/627 (Warrior), 631/635 (Ninja), 645/649 (Sura), 653/657
		// (Shaman); the second coordinate includes map 21's 102400 Y base.
		const BYTE wantedGroup = (ch->GetPlayerID() % 2 == 0) ? 1 : 2;
		const BYTE trainerJob = std::min<BYTE>(ch->GetJob(), JOB_SHAMAN);
		playerbot_empire_rules::TPoint trainerNpc;
		const bool haveTrainer = playerbot_empire_rules::GetSkillTrainer(
				ch->GetMapIndex(), (int)trainerJob, (int)wantedGroup, trainerNpc);
		long trainerX = 0, trainerY = 0;
		if (haveTrainer)
			GetPlayerBotNpcApproach(ch->GetPlayerID(), trainerNpc.x, trainerNpc.y,
					0x54524149U, trainerX, trainerY);

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			long resetX = 0, resetY = 0;
			GetPlayerBotNpcApproach(ch->GetPlayerID(), svc.skillReset.x,
					svc.skillReset.y, 0x52534554U, resetX, resetY);
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
				state.bTownVisitPhase = bDirect
						? GetPlayerBotFirstDirectTownPhase(state)
						: GetPlayerBotFirstExteriorTownPhase(state);
				if (!bDirect && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE &&
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
			// No trainer in this village: the errand is not this map's to do,
			// and walking to (0,0) is what an unguarded table lookup would ask
			// for. The M2 -> M1 travel rule is what carries this need home.
			if (!haveTrainer)
			{
				state.bTownNeedTrainer = false;
				state.bTownVisitPhase = BOT_TOWN_PHASE_TRAINER_WAIT;
				state.dwTownWaitUntil = dwNow;
				return true;
			}
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
							: (bDirect ? GetPlayerBotFirstDirectTownPhase(state)
								: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
									? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE)));
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
						: (bDirect ? GetPlayerBotFirstDirectTownPhase(state)
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
				state.bTownVisitPhase = bDirect
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
			const long keeperX = svc.storekeeper.x;
			const long keeperY = svc.storekeeper.y;
			long goalX = 0, goalY = 0;
			GetPlayerBotNpcApproach(ch->GetPlayerID(), keeperX, keeperY, 0x53414645U, goalX, goalY);
			if (MovePlayerBotTownLeg(ch, state, dwNow, goalX, goalY, 650))
			{
				// One try a session for the gambler, whatever the box says: a
				// page not ready or a purse short of the fee is not a reason to
				// walk back here on every visit it restarts.
				if (IsPlayerBotGambling(state, dwNow))
					state.persona.bGambleSafeboxChecked = true;
				// The first visit pays for the page the way a player does at
				// the keeper's dialog: 500 yang, and the DB core opens a
				// safebox row of one page for the account.
				if (ch->GetQuestFlag(PLAYERBOT_SAFEBOX_PAID_FLAG) <= 0 && ch->GetDesc())
				{
					if (ch->GetGold() < PLAYERBOT_SAFEBOX_FEE)
					{
						sys_log(0, "PLAYERBOT_TOWN: safebox unaffordable pid=%u name=%s gold=%lld",
								ch->GetPlayerID(), ch->GetName(), (long long)ch->GetGold());
						state.bTownNeedSafebox = false;
						state.bTownVisitPhase = bDirect
								? GetPlayerBotFirstDirectTownPhase(state)
								: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
									? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE);
						ClearPlayerBotRoute(state, true);
						if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
							FinishPlayerBotTownVisit(ch, state, dwNow, true);
						return true;
					}
					PlayerBotChangeGold(ch, -PLAYERBOT_SAFEBOX_FEE);
					TSafeboxChangeSizePacket page;
					page.dwID = ch->GetDesc()->GetAccountTable().id;
					// Two pages, matching the window the storekeeper opens and
					// the SetSafeboxSize call below; one was half the store the
					// account is entitled to.
					page.bSize = PLAYERBOT_SAFEBOX_PAGES;
					db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_SIZE, ch->GetDesc()->GetHandle(),
							&page, sizeof(page));
					// A page count. SAFEBOX_PAGE_SIZE is forty-five cells and
					// SetSafeboxSize refuses anything at or above three, so this
					// call did nothing at all for as long as it has existed -
					// which is the real cause of the "page not ready" branch
					// below, not a slow DB round trip.
					ch->SetSafeboxSize(PLAYERBOT_SAFEBOX_PAGES);
					ch->SetQuestFlag(PLAYERBOT_SAFEBOX_PAID_FLAG, 1);
					sys_log(0, "PLAYERBOT_TOWN: safebox paid pid=%u name=%s account=%u fee=%d gold_left=%lld",
							ch->GetPlayerID(), ch->GetName(), page.dwID, PLAYERBOT_SAFEBOX_FEE, (long long)ch->GetGold());
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
			// The page the fee just bought is created by the DB core one round
			// trip late, so the box that loads on the first paid visit can have
			// no valid slot at all: IsValidPosition(0) is false and every
			// deposit lands nowhere (deposited=0, books_left unchanged - what
			// uxietoszef's bots showed, 74 books for good). Treat that as "not
			// ready" and keep the errand rather than reporting a phantom
			// deposit; the next visit finds the page and fills it, no fee again.
			if (box && !box->IsValidPosition(0))
			{
				ch->CloseSafebox();
				PlayerBotLogThrottled("safebox_not_ready", dwNow,
						"PLAYERBOT_TOWN: safebox page not ready pid=%u name=%s books=%d",
						ch->GetPlayerID(), ch->GetName(), CountPlayerBotSkillBooks(ch));
				// leave bTownNeedSafebox set; back off a little and try next visit.
				state.bTownVisitPhase = bDirect
						? GetPlayerBotFirstDirectTownPhase(state)
						: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
							? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE);
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
				return true;
			}
			if (box)
			{
				int toppedUp = 0;
				// What went down in this visit, so the withdrawal below cannot
				// ask for it back in the same breath.
				std::set<DWORD> justDeposited;
				const int deposited = DepositPlayerBotSafeboxBooks(ch, state, box, &toppedUp, &justDeposited);
				// And then back the other way, on the same open box. The deposit
				// runs first on purpose: it is what frees the bag cells the
				// withdrawal then needs, so a bot under pressure can still take
				// back the one material it came for. A gambler also takes its
				// pieces and their materials, once a session.
				const int taken = WithdrawPlayerBotSafebox(ch, box, &justDeposited,
						IsPlayerBotGambling(state, dwNow) ? &state.persona : NULL, &state.persona);
#if defined(PLAYERBOT_ENGINE_MT2009)
				// The box poured together and laid out the way a player's
				// "Scal i uporzadkuj" does it (ArrangeSafebox, playerbot_arrange.cpp):
				// every stack, not sixteen a visit, and the same code a player's
				// button runs, on every bot's box.
				const playerbot_arrange::TResult arranged = playerbot_arrange::ArrangeSafebox(ch, false);
				const int stacked = arranged.merged;
				const int rearranged = arranged.moved;
				const int arrangeCode = arranged.code;
#else
				const int stacked = MergePlayerBotSafeboxStacks(box, PLAYERBOT_SAFEBOX_STACK_MERGES_PER_VISIT);
				const int rearranged = 0;
				const int arrangeCode = -1;
#endif
				// What the box now holds of the list's families, for the choices
				// made away from it (playerbot_lpp.h).
				RefreshPlayerBotLppStored(ch, state.persona, box);
				ch->CloseSafebox();
				sys_log(0, "PLAYERBOT_TOWN: safebox deposit pid=%u name=%s deposited=%d taken=%d books_left=%d free_cells=%d topped_up=%d stacked=%d arranged=%d arrange_code=%d",
						ch->GetPlayerID(), ch->GetName(), deposited, taken, CountPlayerBotSkillBooks(ch),
						CountPlayerBotFreeInventoryCells(ch), toppedUp, stacked, rearranged, arrangeCode);
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
				state.bTownVisitPhase = bDirect
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
			if (!HasPlayerBotTownGate(ch->GetMapIndex()))
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
						: (bDirect ? BOT_TOWN_PHASE_NONE : BOT_TOWN_PHASE_GATE_OUT);
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
				// The gambler's pieces, after the bot's own (playerbot_gambler.h).
				ManagePlayerBotGamble(ch, state, dwNow);
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
			// The gambler stays at the anvil for as long as its session has a
			// piece and a purse left - the visit's six to twenty-four seconds
			// are a player's quick refine, not a gambler's evening
			// (IsPlayerBotGambling bounds it by its own clock).
			if (ManagePlayerBotGamble(ch, state, dwNow) || IsPlayerBotGambling(state, dwNow))
				state.dwTownWaitUntil = std::max<DWORD>(state.dwTownWaitUntil, dwNow + 2500);
			ManagePlayerBotBonusReroll(ch, state, dwNow);
			if (!HasPlayerBotRefineOpportunity(ch))
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
			if (dwNow >= state.dwTownWaitUntil)
			{
				// Always leave the NPC wearing the best surviving/refined equipment,
				// even if materials, Yang or a failed roll ended the session early.
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
				state.bTownVisitPhase = bDirect
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
			if (!HasPlayerBotTownGate(ch->GetMapIndex()))
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
