#ifndef __INC_METIN2_PLAYERBOT_GAMBLER_H__
#define __INC_METIN2_PLAYERBOT_GAMBLER_H__

// Iwakura's gambler (Hazardzista, "SYSTEM OSOBOWOSCI v2.0", 19 September): the
// bot's investment mode. With a heavy purse, its own gear done and no party, a
// bot may turn the end of a town visit into an evening at the anvil: pieces
// from the storekeeper and the bag, each with a risk of its own - +7 six times
// in ten, +8 three times, +9 once - worked at the plain blacksmith up to +7 and
// under nothing but the Blessing Scroll past it, until forty percent of the
// purse it came in with is spent or a piece reaches +9. What it makes is goods
// for the counter, never gear to wear: its own gear is the Perfectionist's.
//
// The rules are playerbot_persona_rules.h (RollGambleTarget, NextGambleStep,
// BudgetLeft), unit-tested; this file is the engine's half. It runs inside the
// town visit - ContinuePlayerBotVisitAsGambler turns a visit that has finished
// its errands into the walk to the storekeeper and the anvil, and the
// blacksmith's two phases call ManagePlayerBotGamble - because a visit is what
// every other pass (the stall, the market, the road) already stands back for;
// a session begun after the visit had ended would have been walked away from
// the anvil before it reached it.
//
// What the document asks and this does not do yet: buying a base or a
// material off the counters when the bag and the safebox have none.
//
// An implementation fragment in the sense playerbot_types.h describes. Include
// it exactly once, after playerbot_town.h, which declares what it asks of it.

namespace
{
	bool IsPlayerBotGambling(const TPlayerBotAIState& state, DWORD dwNow)
	{
		return state.persona.bGambling && dwNow < state.persona.dwGambleUntil;
	}

	// Iwakura's price of this piece at another refine, on his curve, or zero
	// when his sheet does not carry the family - GetPlayerBotGearAskingBase at
	// a refine of the caller's choosing. What the session stakes and loses is
	// counted in it: a burn costs the piece, a scroll's failure the grade.
	DWORD GetPlayerBotGambleValueAt(LPITEM item, int refine)
	{
		if (!item || refine < 0 || refine > 9 ||
				(item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const BYTE now = item->GetRefineLevel();
		if (now > 9 || (DWORD)now > item->GetVnum())
			return 0;
		const DWORD baseVnum = item->GetVnum() - now;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == baseVnum)
			{
				const DWORD price = PLAYERBOT_GEAR_PRICES[i].adwPrice[refine];
				if (price == 0)
					return 0;
				return ScalePlayerBotIwakuraPrice((DWORD)((unsigned long long)price *
						(unsigned long long)GetPlayerBotSocketStonePercent(item) / 100ULL));
			}
		return 0;
	}

	// A piece the gambler could work, judged by the item alone - which is all
	// a piece in the safebox has. The LPP's "cenny ekwipunek": a family his tier
	// list rates PLAYERBOT_GAMBLE_MIN_TIER or better in PvE or PvP, or the body
	// armour, helmets and shields he judges by level; and his sheet must price
	// it higher at +9 than as it is. Not what the operator already rules on: a
	// level-30 weapon has a ladder of its own, a scroll-only weapon never meets
	// the plain anvil, a prize line is not staked, and level-one gear is
	// merchant scrap under +8 whatever the anvil makes of it.
	bool IsPlayerBotGambleStock(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const BYTE type = item->GetType();
		if (type != ITEM_WEAPON && type != ITEM_ARMOR)
			return false;
		if (item->GetRefinedVnum() == 0 || item->GetRefineLevel() >= 9)
			return false;
		if (IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotScrollOnlyWeapon(item) ||
				IsPlayerBotPrizeItem(item) || IsPlayerBotMerchantOnlyGear(item))
			return false;
		if (IsPlayerBotLowLevelGear(item) &&
				GetPlayerBotLowGearMinRefine(item) > playerbot_persona::GAMBLE_SAFE_PLUS)
			return false;
		const DWORD valueNow = GetPlayerBotGambleValueAt(item, item->GetRefineLevel());
		const DWORD valueTop = GetPlayerBotGambleValueAt(item, 9);
		if (valueTop == 0 || valueTop <= valueNow)
			return false;
		const bool judgedByLevel = type == ITEM_ARMOR && (item->GetSubType() == ARMOR_BODY ||
				item->GetSubType() == ARMOR_HEAD || item->GetSubType() == ARMOR_SHIELD);
		if (!judgedByLevel)
		{
			const DWORD baseVnum = item->GetVnum() - item->GetRefineLevel();
			const int tier = std::max(GetPlayerBotItemTier(baseVnum, -1, false),
					GetPlayerBotItemTier(baseVnum, -1, true));
			if (tier < PLAYERBOT_GAMBLE_MIN_TIER)
				return false;
		}
		return true;
	}

	// And one in this bot's bag it may work now. Its own gear stays its own:
	// what the equipment pass is about to put on, the spare the blacksmith is
	// making into one, the backup weapon, the level-30 weapon it is grinding,
	// the dagger it breaks stones with, anything the ordinary refine pass is
	// raising. What the junk rule sends to the merchant is not worth a fee.
	bool IsPlayerBotGambleBase(LPCHARACTER ch, LPITEM item, LPITEM backup)
	{
		if (!IsPlayerBotGambleStock(ch, item))
			return false;
		if (item->IsEquipped() || item->isLocked() || item->IsExchanging() ||
				item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
				item->GetCell() >= PLAYERBOT_BAG_CELLS || ch->GetInventoryItem(item->GetCell()) != item)
			return false;
		if (item == backup || IsPlayerBotWearableUpgrade(ch, item, item->GetCell()) ||
				IsPlayerBotHigherTierSpare(ch, item) || IsPlayerBotLevel30Project(ch, item) ||
				IsPlayerBotArcherStoneWeapon(ch, item) || IsPlayerBotRefineBagCandidate(ch, item))
			return false;
		return !IsPlayerBotJunkItem(ch, item);
	}

	void CollectPlayerBotGambleBases(LPCHARACTER ch, std::vector<LPITEM>& out)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		LPITEM backup = FindPlayerBotBackupWeapon(ch);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotGambleBase(ch, item, backup))
				out.push_back(item);
		}
	}

	// What the next plain step of every base in the bag consumes: the
	// storekeeper's half of "ulepszacze" (WithdrawPlayerBotSafebox).
	void CollectPlayerBotGambleMaterials(LPCHARACTER ch, std::set<DWORD>& out)
	{
		out.clear();
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		for (size_t i = 0; i < bases.size(); ++i)
		{
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(bases[i]->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
				if (recipe->materials[m].vnum != 0 && recipe->materials[m].count > 0)
					out.insert(recipe->materials[m].vnum);
		}
	}

	// The materials a step consumes, if the bag holds them over what the
	// Biologist is still owed and what the bot's own anvil keeps back - the
	// gambler works with what is spare, never with the Perfectionist's stock.
	// Their worth on Iwakura's sheet goes to the budget like the fee.
	bool PlayerBotGambleHasMaterials(LPCHARACTER ch, const TRefineTable* recipe, long long& value)
	{
		value = 0;
		if (!ch || !recipe)
			return false;
		for (int i = 0; i < recipe->material_count; ++i)
		{
			const DWORD vnum = recipe->materials[i].vnum;
			const int need = recipe->materials[i].count;
			if (vnum == 0 || need <= 0)
				continue;
			const int spare = (int)ch->CountSpecifyItem(vnum) - GetPlayerBotBiologistReserve(ch, vnum) -
					GetPlayerBotRefineMaterialReserve(ch, vnum);
			if (spare < need)
				return false;
			value += (long long)need * (long long)GetPlayerBotMaterialAskingBase(vnum);
		}
		return true;
	}

	// "Wylacznie ze Zwojow Blogoslawienstwa": 25040 and nothing else, and only
	// what the bot's own scroll work does not keep back (GetPlayerBotRefineScrollKeep).
	int FindPlayerBotGambleScrollCell(LPCHARACTER ch)
	{
		if (!ch || CountPlayerBotSafeRefineScrolls(ch) - GetPlayerBotRefineScrollKeep(ch) <= 0)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (scroll && scroll->GetCell() == cell && !scroll->isLocked() &&
					scroll->GetVnum() == PLAYERBOT_BLESSING_SCROLL_VNUM)
				return cell;
		}
		return -1;
	}

	// Whether the anvil could do anything with this base right now: the next
	// step's recipe, its materials spare, and for a step past +7 a Blessing
	// Scroll to go under. The session is only begun for a bag that holds one:
	// the first version began it for any base and a third of the sessions
	// were a walk to the storekeeper and the anvil for nothing - every piece
	// stood at +4, where this world's recipes start asking for materials.
	bool IsPlayerBotGambleWorkable(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe)
			return false;
		long long materials = 0;
		if (!PlayerBotGambleHasMaterials(ch, recipe, materials))
			return false;
		if ((long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) <
				(long long)ch->ComputeRefineFee(recipe->cost))
			return false;
		return item->GetRefineLevel() < playerbot_persona::GAMBLE_SAFE_PLUS ||
				FindPlayerBotGambleScrollCell(ch) >= 0;
	}

	TPlayerBotGamblePlan* FindPlayerBotGamblePlan(TPlayerBotPersona& p, DWORD itemId)
	{
		for (size_t i = 0; i < p.vecGamblePlans.size(); ++i)
			if (p.vecGamblePlans[i].dwItemId == itemId)
				return &p.vecGamblePlans[i];
		return NULL;
	}

	// How likely a bot that qualifies is to take it, by the character it
	// plays: the trader most, the careful collector least. Iwakura gives no
	// number; what he gives is that the Perfectionist comes first and the
	// gambler only "moze plynnie przejsc" from the Trader.
	int GetPlayerBotGambleChance(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_MERCHANT:
				return 50;
			case BOT_PERSONALITY_WANDERER:
				return 30;
			case BOT_PERSONALITY_METIN_BREAKER:
				return 25;
			case BOT_PERSONALITY_GEAR_SPECIALIST:
				return 10;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return 5;
			default:
				return 20;
		}
	}

	void EndPlayerBotGamble(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, const char* reason)
	{
		TPlayerBotPersona& p = state.persona;
		if (!p.bGambling)
			return;
		p.bGambling = false;
		p.dwNextGambleAt = dwNow + number(PLAYERBOT_GAMBLE_REST_MIN_MS, PLAYERBOT_GAMBLE_REST_MAX_MS);
		p.dwNextDecide = 0;
		const long long budget = p.llGambleGoldStart * playerbot_persona::GAMBLE_BUDGET_PERCENT / 100;
		sys_log(0, "PLAYERBOT_PERSONA: gambler done pid=%u name=%s reason=%s spent=%lld budget=%lld attempts=%u set_aside=%u burned=%u downgraded=%u nines=%u gold=%lld",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?", reason ? reason : "?",
				p.llGambleSpent, budget, (unsigned int)p.wGambleAttempts,
				(unsigned int)p.bGambleFinished, (unsigned int)p.bGambleBurned,
				(unsigned int)p.bGambleDowngraded, (unsigned int)p.bGambleNines,
				ch ? (long long)ch->GetGold() : 0LL);
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		// "Bot aktualizuje swoj sklep offline": the stand is served as soon as
		// the visit lets the bot go, so what came off the anvil goes up.
		if (p.bGambleFinished > 0 && state.offlineShop.nextService > dwNow + 5000)
			state.offlineShop.nextService = dwNow + 5000;
#endif
		p.vecGamblePlans.clear();
	}

	// The end of a visit that did its errands: the moment a Trader with a
	// heavy purse may turn gambler. Every cheap gate first - most visits end
	// here with a bot that is resting from its last session, in a party, or
	// short of the purse - then the bag. The visit goes on at the storekeeper
	// and then the anvil; true when it does.
	bool ContinuePlayerBotVisitAsGambler(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotPersona& p = state.persona;
		if (!p.bRestored || p.bGambling || dwNow < p.dwNextGambleAt)
			return false;
		const long mapIndex = ch->GetMapIndex();
		// "Nie jest aktualnie przypisany do zadnej aktywnej grupy (PT)"; and a
		// dropper is a drop character, whose purse is its counter's.
		if (!IsPlayerBotVillageMap(mapIndex) || ch->GetParty() || IsPlayerBotDropper(state.bPersonality))
			return false;
		// Not straight after the Perfectionist, whose visit this may be.
		if (p.bPersona == playerbot_persona::PERSONA_PERFEKCJONISTA ||
				(p.dwPerfectEndedAt != 0 && dwNow - p.dwPerfectEndedAt < PLAYERBOT_GAMBLE_AFTER_PERFECT_MS))
			return false;
		const long long gold = (long long)ch->GetGold();
		if (gold < (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_GAMBLE_MIN_PURSE_BASE))
			return false;
		// Iwakura's community patch 2, point 10: back in its first village as
		// the Trader, with a weapon at +7, an armour at +6 and three million on
		// his scale, a bot turns gambler whatever the draw says - it picks a
		// piece, buys what the anvil wants and works it by the gambler's rules.
		// Such a bot is past the gear the rule below waits for, and its pieces
		// may all be at the storekeeper now (the Useful Items List puts them
		// there at every visit): the session's first stop is the safebox.
		LPITEM wornWeapon = ch->GetWear(WEAR_WEAPON);
		LPITEM wornArmour = ch->GetWear(WEAR_BODY);
		const bool townTrigger = IsPlayerBotM1Map(mapIndex) &&
				wornWeapon && wornWeapon->GetRefineLevel() >= PLAYERBOT_GAMBLE_TOWN_WEAPON_PLUS &&
				wornArmour && wornArmour->GetRefineLevel() >= PLAYERBOT_GAMBLE_TOWN_ARMOUR_PLUS &&
				gold >= (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_GAMBLE_TOWN_PURSE_BASE);
		// The bot's own gear comes first - the Perfectionist is "absolutny
		// fundament", the gambler what a bot does with what is left over.
		if (!townTrigger && HasPlayerBotRefineOpportunity(ch))
			return false;
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		size_t workable = 0;
		for (size_t i = 0; i < bases.size(); ++i)
			if (IsPlayerBotGambleWorkable(ch, bases[i]))
				++workable;
		if (workable == 0 && !(townTrigger && !p.mapLppStored.empty()))
			return false;
		if (!townTrigger && number(1, 100) > GetPlayerBotGambleChance(state.bPersonality))
		{
			p.dwNextGambleAt = dwNow + number(PLAYERBOT_GAMBLE_RETRY_MIN_MS, PLAYERBOT_GAMBLE_RETRY_MAX_MS);
			return false;
		}

		p.bGambling = true;
		p.llGambleGoldStart = gold;
		p.llGambleSpent = 0;
		p.dwGambleUntil = dwNow + PLAYERBOT_GAMBLE_MAX_MS;
		p.dwNextGambleStep = 0;
		p.bGambleNines = 0;
		p.bGambleBurned = 0;
		p.bGambleFinished = 0;
		p.bGambleDowngraded = 0;
		p.wGambleAttempts = 0;
		p.bGambleSafeboxChecked = false;
		p.bGambleSafeboxTaken = 0;
		p.vecGamblePlans.clear();
		p.dwNextDecide = 0;
		sys_log(0, "PLAYERBOT_PERSONA: gambler begins pid=%u name=%s gold=%lld budget=%lld pieces=%u workable=%u character=%u map=%ld town_trigger=%d",
				ch->GetPlayerID(), ch->GetName(), gold,
				gold * playerbot_persona::GAMBLE_BUDGET_PERCENT / 100, (unsigned int)bases.size(),
				(unsigned int)workable, (unsigned int)state.bPersonality, mapIndex, townTrigger ? 1 : 0);

		// The errands the visit came for are done; what is left is the
		// storekeeper and the anvil, walked by the same phases as any visit.
		state.bTownNeedMisc = false;
		state.bTownNeedWeaponMerchant = false;
		state.bTownNeedArmorMerchant = false;
		state.bTownNeedTrainer = false;
		state.bTownNeedSkillReset = false;
		state.bTownNeedSafebox = true;
		state.bTownNeedBlacksmith = true;
		const bool bDirect = IsPlayerBotM2Map(mapIndex) || !IsPlayerBotGatedVillage(mapIndex);
		if (bDirect)
			state.bTownVisitPhase = GetPlayerBotFirstDirectTownPhase(state);
		else
		{
			// A walled village's visit ends outside the gate, where its
			// storekeeper stands; the anvil is inside.
			state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
			if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
		}
		state.dwTownWaitUntil = 0;
		ClearPlayerBotRoute(state, true);
		return true;
	}

	// One step at the anvil, a player's click every PLAYERBOT_GAMBLE_STEP_*_MS,
	// from the blacksmith's two phases. True when a refine was attempted.
	bool ManagePlayerBotGamble(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		TPlayerBotPersona& p = state.persona;
		if (!p.bGambling)
			return false;
		if (!IsPlayerBotPersonaEnabled())
		{
			EndPlayerBotGamble(ch, state, dwNow, "switch_off");
			return false;
		}
		if (dwNow >= p.dwGambleUntil)
		{
			EndPlayerBotGamble(ch, state, dwNow, "time");
			return false;
		}
		if (state.bTownVisitPhase != BOT_TOWN_PHASE_BLACKSMITH &&
				state.bTownVisitPhase != BOT_TOWN_PHASE_BLACKSMITH_WAIT)
			return false;
		if (!ch->IsItemLoaded() || ch->IsDead() || ch->GetMyShop() || ch->GetExchange())
			return false;
		if (dwNow < p.dwNextGambleStep)
			return false;
		p.dwNextGambleStep = dwNow + number(PLAYERBOT_GAMBLE_STEP_MIN_MS, PLAYERBOT_GAMBLE_STEP_MAX_MS);

		const long long left = playerbot_persona::BudgetLeft(p.llGambleGoldStart,
				playerbot_persona::GAMBLE_BUDGET_PERCENT, p.llGambleSpent);
		if (left <= 0)
		{
			EndPlayerBotGamble(ch, state, dwNow, "budget");
			return false;
		}

		// The piece on the anvil: the one under way, else the most valuable
		// base not yet worked this session - its plan is rolled as it is taken.
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		LPITEM item = NULL;
		TPlayerBotGamblePlan* plan = NULL;
		for (size_t i = 0; i < bases.size() && !item; ++i)
		{
			TPlayerBotGamblePlan* known = FindPlayerBotGamblePlan(p, bases[i]->GetID());
			if (known && !known->bDone)
			{
				item = bases[i];
				plan = known;
			}
		}
		if (!item)
		{
			DWORD bestValue = 0;
			for (size_t i = 0; i < bases.size(); ++i)
			{
				if (FindPlayerBotGamblePlan(p, bases[i]->GetID()))
					continue;
				const DWORD value = GetPlayerBotGambleValueAt(bases[i], playerbot_persona::GAMBLE_SAFE_PLUS);
				if (!item || value > bestValue)
				{
					item = bases[i];
					bestValue = value;
				}
			}
			if (item)
			{
				TPlayerBotGamblePlan fresh;
				fresh.dwItemId = item->GetID();
				fresh.bTarget = playerbot_persona::RollGambleTarget((uint32_t)number(0, 99));
				fresh.bScrollFailed = false;
				fresh.bDone = false;
				p.vecGamblePlans.push_back(fresh);
				plan = &p.vecGamblePlans.back();
				sys_log(0, "PLAYERBOT_PERSONA: gambler takes pid=%u name=%s vnum=%u plus=%u target=%u value_at_7=%u left=%lld",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetRefineLevel(),
						(unsigned int)plan->bTarget, bestValue, left);
			}
		}
		if (!item || !plan)
		{
			EndPlayerBotGamble(ch, state, dwNow, "no_pieces");
			return false;
		}

		const BYTE plus = item->GetRefineLevel();
		const playerbot_persona::EGambleStep step =
				playerbot_persona::NextGambleStep(plus, plan->bTarget, plan->bScrollFailed);
		if (step == playerbot_persona::GAMBLE_STEP_DONE)
		{
			// For sale as it stands: at its target, or back at +7 after a
			// scroll failed on it.
			plan->bDone = true;
			++p.bGambleFinished;
			sys_log(0, "PLAYERBOT_PERSONA: gambler sets aside pid=%u name=%s vnum=%u plus=%u target=%u scroll_failed=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
					(unsigned int)plan->bTarget, plan->bScrollFailed ? 1 : 0);
			p.dwNextGambleStep = dwNow + 500;
			return false;
		}

		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe)
		{
			plan->bDone = true;
			return false;
		}
		const long long fee = (long long)ch->ComputeRefineFee(recipe->cost);
		long long materialsValue = 0;
		if (!PlayerBotGambleHasMaterials(ch, recipe, materialsValue))
		{
			// Not with the Perfectionist's stock: the piece is sold as it is.
			plan->bDone = true;
			PlayerBotLogThrottled("gamble_no_materials", dwNow,
					"PLAYERBOT_PERSONA: gambler lacks spare materials pid=%u name=%s vnum=%u plus=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus);
			return false;
		}
		if ((long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) < fee)
		{
			EndPlayerBotGamble(ch, state, dwNow, "gold");
			return false;
		}
		int scrollCell = -1;
		long long scrollValue = 0;
		if (step == playerbot_persona::GAMBLE_STEP_SCROLL)
		{
			scrollCell = FindPlayerBotGambleScrollCell(ch);
			if (scrollCell < 0)
			{
				// No scroll, no +8: "wylacznie ze Zwojow Blogoslawienstwa".
				plan->bDone = true;
				++p.bGambleFinished;
				sys_log(0, "PLAYERBOT_PERSONA: gambler has no blessing scroll pid=%u name=%s vnum=%u plus=%u target=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
						(unsigned int)plan->bTarget);
				return false;
			}
			scrollValue = (long long)GetPlayerBotMaterialAskingBase(PLAYERBOT_BLESSING_SCROLL_VNUM);
		}

		// The stake: the fee, the materials, the scroll, and what a failure
		// takes - the whole piece at the plain anvil, a grade under a scroll.
		// It has to fit in what is left of the forty percent ("nigdy nie
		// stawia wszystkiego na jedna karte"); a piece whose step would not
		// is sold as it is and the next one is tried.
		const long long valueNow = std::max<long long>(GetPlayerBotGambleValueAt(item, plus),
				(long long)GetPlayerBotRefineInvestment(item));
		const long long valueDown = plus > 0 ? (long long)GetPlayerBotGambleValueAt(item, plus - 1) : 0;
		const long long failLoss = scrollCell >= 0 ? std::max<long long>(0, valueNow - valueDown) : valueNow;
		if (fee + materialsValue + scrollValue + failLoss > left)
		{
			plan->bDone = true;
			if (plus >= playerbot_persona::GAMBLE_SAFE_PLUS)
				++p.bGambleFinished;
			sys_log(0, "PLAYERBOT_PERSONA: gambler will not stake pid=%u name=%s vnum=%u plus=%u stake=%lld left=%lld",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
					fee + materialsValue + scrollValue + failLoss, left);
			return false;
		}

		// What the recipe asks and what the bag holds, before the attempt
		// takes it - the same field the refine pass writes.
		char materials[128] = "none";
		{
			size_t used = 0;
			for (int m = 0; m < recipe->material_count && used + 24 < sizeof(materials); ++m)
				used += snprintf(materials + used, sizeof(materials) - used, "%s%u:%d/%d",
						m ? "," : "", (unsigned int)recipe->materials[m].vnum, (int)recipe->materials[m].count,
						(int)ch->CountSpecifyItem(recipe->materials[m].vnum));
		}

		// The engine puts what comes off the anvil in the cell the piece left:
		// the next grade, the grade under it after a scroll, or nothing after a
		// burn. The piece itself is gone after the call either way.
		const WORD cell = item->GetCell();
		const DWORD itemId = item->GetID();
		const DWORD oldVnum = item->GetVnum();
		const DWORD nextVnum = item->GetRefinedVnum();
		const long long goldBefore = (long long)ch->GetGold();
		bool attempted = false;
		if (scrollCell >= 0)
		{
			ch->SetRefineMode(scrollCell);
			attempted = ch->DoRefineWithScroll(item);
			ch->ClearRefineMode();
		}
		else
			attempted = ch->DoRefine(item, false);
		item = NULL;
		if (!attempted)
		{
			plan->bDone = true;
			sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u materials=%s gamble=1 (requirements/state)",
					ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plus, materials);
			return false;
		}
		++p.wGambleAttempts;

		LPITEM after = ch->GetInventoryItem(cell);
		const long long paid = std::max<long long>(0, goldBefore - (long long)ch->GetGold());
		long long lost = 0;
		const char* outcome = "FAILED_KEPT";
		const bool success = after && after->GetVnum() == nextVnum;
		if (success)
		{
			outcome = "SUCCESS";
			plan->dwItemId = after->GetID();
		}
		else if (!after)
		{
			outcome = "FAILED_BURNED";
			lost = valueNow;
			plan->bDone = true;
			++p.bGambleBurned;
			NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "burned");
		}
		else if (after->GetID() != itemId)
		{
			// A scroll's failure: the grade under it, in the same cell.
			outcome = "FAILED_DOWNGRADED";
			lost = failLoss;
			plan->dwItemId = after->GetID();
			plan->bScrollFailed = true;
			++p.bGambleDowngraded;
			NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "downgraded");
		}
		// A refined, downgraded or burned piece is not the item the stall
		// registries remember by id.
		if (!after || after->GetID() != itemId)
		{
			state.mapStallUnsold.erase(itemId);
			state.mapStockFirstListed.erase(itemId);
		}
		const long long spent = paid + materialsValue + (scrollCell >= 0 ? scrollValue : 0) + lost;
		p.llGambleSpent += spent;
		sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=%d materials=%s gamble=1 target=%u spent=%lld left=%lld",
				outcome, ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, (unsigned int)plus + 1,
				scrollCell >= 0 ? 1 : 0, materials, (unsigned int)plan->bTarget, spent,
				std::max<long long>(0, left - spent));

		if (success)
		{
			BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plus + 1);
			// +8 and +9 are Iwakura's euphoria (playerbot_mood.h).
			NotePlayerBotMoodRefine(ch, (int)plus + 1);
			// "Wielki sukces (+9)": the appetite is sated, whatever is left of
			// the budget, and the masterpiece goes to the counter.
			if (plus + 1 >= 9)
			{
				plan->bDone = true;
				++p.bGambleNines;
				++p.bGambleFinished;
				EndPlayerBotGamble(ch, state, dwNow, "nine");
			}
		}
		return true;
	}

	// The gambler's second source, after the storekeeper: the counters. "Jesli
	// brakuje mu bazy lub ulepszaczy, przeszukuje sklepy offline na rynku i
	// skupuje je po najnizszych cenach" - what it wants of an offer is a base
	// it could work (the item's own test; the bag's run at the anvil) while it
	// has fewer than PLAYERBOT_GAMBLE_MARKET_BASES to hand, or a material one
	// of its pieces' next steps consumes. The market's own rules price it.
	bool WantsPlayerBotGambleOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const DWORD dwNow = get_dword_time();
		if (it == s_mapPlayerBotAIStates.end() || !IsPlayerBotGambling(it->second, dwNow))
			return false;
		const TPlayerBotPersona& p = it->second.persona;
		if (playerbot_persona::BudgetLeft(p.llGambleGoldStart,
				playerbot_persona::GAMBLE_BUDGET_PERCENT, p.llGambleSpent) <= 0)
			return false;
		if (offer->GetType() == ITEM_WEAPON || offer->GetType() == ITEM_ARMOR)
		{
			std::vector<LPITEM> bases;
			CollectPlayerBotGambleBases(ch, bases);
			return (int)bases.size() < PLAYERBOT_GAMBLE_MARKET_BASES && IsPlayerBotGambleStock(ch, offer);
		}
		std::set<DWORD> materials;
		CollectPlayerBotGambleMaterials(ch, materials);
		return materials.find(offer->GetVnum()) != materials.end();
	}

	// Anything a gambler buys comes out of the session's budget, the fees and
	// the burned pieces beside it ("Kazdy zakup ulepszacza, oplata u Kowala,
	// zuzycie Zwoju Blogoslawienstwa czy spalenie przedmiotu bezposrednio
	// obciaza wydzielony budzet").
	void NotePlayerBotGamblePurchase(LPCHARACTER ch, long long paid)
	{
		if (!ch || paid <= 0)
			return;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end() || !IsPlayerBotGambling(it->second, get_dword_time()))
			return;
		it->second.persona.llGambleSpent += paid;
	}
}

#endif
