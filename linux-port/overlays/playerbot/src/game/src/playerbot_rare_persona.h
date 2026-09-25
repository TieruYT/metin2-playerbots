#ifndef __INC_METIN2_PLAYERBOT_RARE_PERSONA_H__
#define __INC_METIN2_PLAYERBOT_RARE_PERSONA_H__

// Iwakura's Patch 3, point 7: the rare personalities, shown in red over a
// bot's head. Each is a state of its own length that outranks the bot's
// situation (DecidePersona puts it after a contract and a party only), drawn
// on each core every PLAYERBOT_RARE_DRAW_MS over the bots it runs: every bot
// that qualifies wins a kind one time in its number, no more than its cap run
// at once, and after one begins none of its kind begins for its world pause
// (playerbot_persona_rules.h, GetRareRule - unit-tested).
//
// None of them is new machinery; each is an old errand held open for longer
// or with a bigger purse:
//   Metinolog          a Metin expedition (IsPlayerBotMetinHunting) of 120 to
//                      250 minutes - the hunter's patrol, loot and town trips.
//   Nalogowiec         the gambler's session (playerbot_gambler.h) past every
//                      gate but the village, on 85 percent of the purse, with
//                      bases bought off the counters up to +6; what comes off
//                      the anvil better than its own gear it wears.
//   Szalony Naukowiec  a market trip for its Master skills' books
//                      (playerbot_progression_needs.h) on 70 percent of it.
//   Egzekutor          two hours of falling on the other kingdoms' characters
//                      on the shared maps (playerbot_anti_pk.h): the victim's
//                      kingdom near him defends, and a death at a bot's hands
//                      sends him to other ground.
//   Szalony Wedkarz    six hours at the water (the fishing spell), sessions
//                      a minute or two apart, the bag emptied in town between.
//
// The states and the world's pauses live in the core's memory: a restart ends
// them and starts the pauses afresh. An implementation fragment in the sense
// playerbot_types.h describes: include it exactly once, after
// playerbot_anti_pk.h, whose foe pass is the Executioner's fight.

namespace
{
	const char* GetPlayerBotRareName(BYTE rare)
	{
		switch (rare)
		{
			case playerbot_persona::RARE_METINOLOG: return "metinolog";
			case playerbot_persona::RARE_NALOGOWIEC: return "nalogowiec";
			case playerbot_persona::RARE_NAUKOWIEC: return "naukowiec";
			case playerbot_persona::RARE_EGZEKUTOR: return "egzekutor";
			case playerbot_persona::RARE_WEDKARZ: return "wedkarz";
			default: return "none";
		}
	}

	// Who may be drawn for anything: under the switch, in the world, its own
	// master - no party, no contract - no dropper (a drop character's time is
	// its table's) and nothing rare running already.
	bool IsPlayerBotRareCandidate(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !ch->IsItemLoaded() || !state.persona.bRestored ||
				ch->GetParty() || IsPlayerBotDropper(state.bPersonality) || IsPlayerBotSidekickPID(ch->GetPlayerID()) ||
				IsPlayerBotMercenaryOnContract(ch->GetPlayerID()) || IsPlayerBotHiredClient(ch->GetPlayerID()))
			return false;
		return GetPlayerBotRareNow(state.persona, dwNow) == 0;
	}

	int GetPlayerBotWornWeaponPlus(LPCHARACTER ch)
	{
		LPITEM weapon = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return weapon && weapon->GetType() == ITEM_WEAPON ? weapon->GetRefineLevel() : -1;
	}

	// "Dowolny przedmiot z bonusem 10% na ludzi": any worn piece.
	bool PlayerBotWearsHumanBonus(LPCHARACTER ch)
	{
		for (int wear = 0; ch && wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM item = ch->GetWear(wear);
			if (!item)
				continue;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (item->GetAttributeType(i) == APPLY_ATTBONUS_HUMAN &&
						item->GetAttributeValue(i) >= PLAYERBOT_EGZEKUTOR_HUMAN_BONUS)
					return true;
		}
		return false;
	}

	// "Wczesniejsze doswiadczenie w lowieniu ryb": the fishing onboarding the
	// session completes on its first cast on the 2.x line, or a rod the
	// catches have refined past the first grade.
	bool PlayerBotHasFishedBefore(LPCHARACTER ch)
	{
		if (!ch)
			return false;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (ch->GetQuestFlag("fishing_onboarding.completed") > 0)
			return true;
#endif
		LPITEM held = ch->GetWear(WEAR_WEAPON);
		if (held && held->GetType() == ITEM_ROD && held->GetVnum() > PLAYERBOT_FISHING_ROD_VNUM)
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_ROD && item->GetVnum() > PLAYERBOT_FISHING_ROD_VNUM)
				return true;
		}
		return false;
	}

	// His conditions, kind by kind.
	bool QualifiesPlayerBotRare(LPCHARACTER ch, const TPlayerBotAIState& state, BYTE rare)
	{
		switch (rare)
		{
			case playerbot_persona::RARE_METINOLOG:
				// "Co najmniej 11. poziom konia oraz bron na minimum +7."
				return ch->GetHorseLevel() >= PLAYERBOT_METINOLOG_MIN_HORSE_LEVEL &&
						GetPlayerBotWornWeaponPlus(ch) >= PLAYERBOT_METINOLOG_MIN_WEAPON_PLUS &&
						(int)ch->GetLevel() >= PLAYERBOT_METIN_EXPEDITION_MIN_LEVEL;
			case playerbot_persona::RARE_NALOGOWIEC:
				// No condition in the document; a purse the gambler would look
				// at is what gives the anvil anything to take.
				return (long long)ch->GetGold() >= (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_GAMBLE_MIN_PURSE_BASE);
			case playerbot_persona::RARE_NAUKOWIEC:
				// A skill at Master that still wants books, and money for them.
				return ch->GetSkillGroup() != 0 &&
						(long long)ch->GetGold() > GetPlayerBotReservedGold(ch) + PLAYERBOT_SHOPPING_GOLD_FLOOR &&
						PlayerBotNeedsMasterBooks(ch);
			case playerbot_persona::RARE_EGZEKUTOR:
				// "Minimum 39. poziom, bron na co najmniej +6 lub dowolny
				// przedmiot z bonusem 10% na ludzi."
				return (int)ch->GetLevel() >= PLAYERBOT_EGZEKUTOR_MIN_LEVEL &&
						(GetPlayerBotWornWeaponPlus(ch) >= PLAYERBOT_EGZEKUTOR_MIN_WEAPON_PLUS ||
						 PlayerBotWearsHumanBonus(ch));
			case playerbot_persona::RARE_WEDKARZ:
				// "Minimum 30. poziom, wedka oraz wczesniejsze doswiadczenie."
				return (int)ch->GetLevel() >= PLAYERBOT_WEDKARZ_MIN_LEVEL &&
						(CountPlayerBotRods(ch) > 0 || IsPlayerBotHoldingRod(ch)) &&
						PlayerBotHasFishedBefore(ch) && CanPlayerBotUseFishingRod(ch) &&
						!IsPlayerBotOnBattleHorseTrial(ch) && !IsPlayerBotOnMilitaryHorseTrial(ch);
			default:
				return false;
		}
	}

	void StartPlayerBotRare(LPCHARACTER ch, TPlayerBotAIState& state, BYTE rare, DWORD dwNow,
			uint32_t eligible, uint32_t running, uint32_t cap)
	{
		TPlayerBotPersona& p = state.persona;
		const playerbot_persona::TRareRule rule = playerbot_persona::GetRareRule(rare);
		const uint32_t minutes = playerbot_persona::RareMinutes(rule, (uint32_t)number(0, 100000));
		p.bRare = rare;
		p.bRareStage = 0;
		p.dwRareSince = dwNow;
		p.dwRareUntil = dwNow + minutes * 60000U;
		p.llRareGoldStart = (long long)ch->GetGold();
		p.llRareSpent = 0;
		p.dwNextDecide = 0;
		switch (rare)
		{
			case playerbot_persona::RARE_METINOLOG:
				state.dwMetinExpeditionUntil = p.dwRareUntil;
				state.dwHubChosenTime = 0;
				break;
			case playerbot_persona::RARE_NALOGOWIEC:
				p.dwNextGambleAt = 0;
				break;
			case playerbot_persona::RARE_NAUKOWIEC:
				state.dwProgressionTripNext = 0;
				break;
			case playerbot_persona::RARE_WEDKARZ:
				p.dwFishingSpellUntil = p.dwRareUntil;
				state.dwNextFishingCheckTime = 0;
				break;
			default:
				break;
		}
		sys_log(0, "PLAYERBOT_PERSONA: rare begins pid=%u name=%s kind=%s level=%u minutes=%u gold=%lld map=%ld eligible=%u running=%u cap=%u",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotRareName(rare), (unsigned int)ch->GetLevel(),
				minutes, (long long)ch->GetGold(), ch->GetMapIndex(), eligible, running + 1, cap);
	}

	void EndPlayerBotRare(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, const char* reason)
	{
		TPlayerBotPersona& p = state.persona;
		const BYTE rare = p.bRare;
		if (rare == 0)
			return;
		switch (rare)
		{
			case playerbot_persona::RARE_METINOLOG:
				// The expedition the patrol was; the planner's clock clears it.
				if (state.dwMetinExpeditionUntil != 0 && (int)(state.dwMetinExpeditionUntil - dwNow) > 0)
					state.dwMetinExpeditionUntil = dwNow;
				break;
			case playerbot_persona::RARE_WEDKARZ:
				if (p.dwFishingSpellUntil != 0 && (int)(p.dwFishingSpellUntil - dwNow) > 0)
					p.dwFishingSpellUntil = dwNow;
				break;
			case playerbot_persona::RARE_EGZEKUTOR:
				if (p.bFoeReason == BOT_FOE_EXECUTOR)
				{
					p.dwFoeVID = 0;
					p.bFoeReason = BOT_FOE_NONE;
				}
				break;
			default:
				break;
		}
		sys_log(0, "PLAYERBOT_PERSONA: rare over pid=%u name=%s kind=%s reason=%s minutes=%u spent=%lld gold=%lld",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?", GetPlayerBotRareName(rare),
				reason ? reason : "?", (unsigned int)((dwNow - p.dwRareSince) / 60000U), p.llRareSpent,
				ch ? (long long)ch->GetGold() : 0LL);
		p.bRare = 0;
		p.bRareStage = 0;
		p.dwRareUntil = 0;
		p.dwNextDecide = 0;
	}

	// Every persona pass, for a bot with a rare one: its time, and the ends
	// the addict's session and the scientist's trip bring themselves.
	void ManagePlayerBotRareState(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (!ch || p.bRare == 0)
			return;
		if (p.dwRareUntil == 0 || (int)(dwNow - p.dwRareUntil) >= 0)
		{
			EndPlayerBotRare(ch, state, dwNow, "time");
			return;
		}
		switch (p.bRare)
		{
			case playerbot_persona::RARE_NALOGOWIEC:
				if (p.bRareStage != 0 && !IsPlayerBotGambling(state, dwNow))
					EndPlayerBotRare(ch, state, dwNow, "session_over");
				break;
			case playerbot_persona::RARE_NAUKOWIEC:
				if (p.bRareStage != 0 && (state.dwProgressionTripUntil == 0 ||
						(int)(dwNow - state.dwProgressionTripUntil) >= 0))
					EndPlayerBotRare(ch, state, dwNow, "trip_over");
				else if (!PlayerBotNeedsMasterBooks(ch))
					EndPlayerBotRare(ch, state, dwNow, "books_enough");
				else if (GetPlayerBotBookBudgetLeft(ch) <= 0)
					EndPlayerBotRare(ch, state, dwNow, "budget");
				break;
			case playerbot_persona::RARE_WEDKARZ:
				if (!CanPlayerBotUseFishingRod(ch))
					EndPlayerBotRare(ch, state, dwNow, "no_rod");
				break;
			default:
				break;
		}
	}

	// The draw, once every PLAYERBOT_RARE_DRAW_MS on this core. The world's
	// pause is by kind, in minutes of the core's clock.
	DWORD s_adwPlayerBotRareLastStartMin[playerbot_persona::RARE_COUNT];
	bool s_abPlayerBotRareStarted[playerbot_persona::RARE_COUNT];

	void ManagePlayerBotRarePersonas(DWORD dwNow)
	{
		static DWORD s_dwNextDraw = 0;
		if (!IsPlayerBotPersonaEnabled())
			return;
		if (s_dwNextDraw != 0 && (int)(dwNow - s_dwNextDraw) < 0)
			return;
		s_dwNextDraw = dwNow + PLAYERBOT_RARE_DRAW_MS;

		uint32_t running[playerbot_persona::RARE_COUNT] = { 0 };
		uint32_t eligible[playerbot_persona::RARE_COUNT] = { 0 };
		std::vector<std::pair<LPCHARACTER, TPlayerBotAIState*> > bots;
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch)
				continue;
			const BYTE now = GetPlayerBotRareNow(it->second.persona, dwNow);
			if (now != 0)
			{
				++running[now];
				continue;
			}
			if (!IsPlayerBotRareCandidate(ch, it->second, dwNow))
				continue;
			bool any = false;
			for (BYTE rare = 1; rare < playerbot_persona::RARE_COUNT; ++rare)
				if (QualifiesPlayerBotRare(ch, it->second, rare))
				{
					++eligible[rare];
					any = true;
				}
			if (any)
				bots.push_back(std::make_pair(ch, &it->second));
		}
		// A fresh order every draw, so the first pids are not the only ones
		// that ever win.
		for (size_t i = bots.size(); i > 1; --i)
			std::swap(bots[i - 1], bots[(size_t)number(0, (int)i - 1)]);

		const uint32_t nowMin = dwNow / 60000U;
		std::set<DWORD> drawn;
		for (BYTE rare = 1; rare < playerbot_persona::RARE_COUNT; ++rare)
		{
			const playerbot_persona::TRareRule rule = playerbot_persona::GetRareRule(rare);
			if (!playerbot_persona::RareMayStart(rule, running[rare], eligible[rare], nowMin,
					s_adwPlayerBotRareLastStartMin[rare], s_abPlayerBotRareStarted[rare]))
				continue;
			const uint32_t cap = playerbot_persona::RareCap(rule, eligible[rare]);
			for (size_t i = 0; i < bots.size() && running[rare] < cap; ++i)
			{
				LPCHARACTER ch = bots[i].first;
				TPlayerBotAIState& state = *bots[i].second;
				if (drawn.count(ch->GetPlayerID()) || !QualifiesPlayerBotRare(ch, state, rare))
					continue;
				if (!playerbot_persona::RareDrawWins((uint32_t)number(0, 0x3fffffff), rule))
					continue;
				StartPlayerBotRare(ch, state, rare, dwNow, eligible[rare], running[rare], cap);
				drawn.insert(ch->GetPlayerID());
				++running[rare];
				s_adwPlayerBotRareLastStartMin[rare] = nowMin;
				s_abPlayerBotRareStarted[rare] = true;
				// A kind with a world pause begins once per pause.
				if (rule.worldPauseMin != 0)
					break;
			}
		}
		sys_log(0, "PLAYERBOT_PERSONA: rare census running metinolog=%u nalogowiec=%u naukowiec=%u egzekutor=%u wedkarz=%u eligible metinolog=%u nalogowiec=%u naukowiec=%u egzekutor=%u wedkarz=%u",
				running[1], running[2], running[3], running[4], running[5],
				eligible[1], eligible[2], eligible[3], eligible[4], eligible[5]);
	}
}

#endif
