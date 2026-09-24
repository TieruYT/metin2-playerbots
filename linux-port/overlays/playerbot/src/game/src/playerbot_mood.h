#ifndef __INC_METIN2_PLAYERBOT_MOOD_H__
#define __INC_METIN2_PLAYERBOT_MOOD_H__

// The Bot Mood System of Iwakura's personality document, the engine's half.
//
// playerbot_persona_rules.h holds what a mood is and how it moves; this file
// holds where the news comes from and where the mood is kept. It comes early
// in the include order on purpose: the drops that lift a mood are noticed by
// the loot pass, the chests and the fishing, and the euphoria of a refine by
// the blacksmith pass, and every one of those only has to call a note here -
// none of them needs to know what a personality is.
//
// A mood is kept in the bot's quest flags (PLAYERBOT_PERSONA_FLAG_*), because
// a bot is logged out and in far more often than a mood is meant to change: a
// restart, a move between the two channels, the life schedule's rest. The
// flags arrive from the db core a moment after the bot enters the game, so the
// first pass waits for quest::PC::IsLoaded before reading them, and nothing is
// written back before they have been read.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_config.h (the PERSONA switch) and the
// generated playerbot_persona_tables.h.

namespace
{
	// Defined in playerbot_mining.h, further down the include order.
	bool IsPlayerBotMiningNow(DWORD pid, DWORD dwNow);

	bool IsPlayerBotSkillInList(const BYTE* list, size_t count, long skill)
	{
		for (size_t i = 0; i < count; ++i)
			if ((long)list[i] == skill)
				return true;
		return false;
	}

	// A boss's casket lifts the mood by one as well (Iwakura, Community Patch
	// 2, point 6): the Orc Chief's, the Esoteric and the Reborn Lord's, the
	// Spider Queen's and the Giant Spider's, the Plague Bearer's, the Desert
	// Turtle's, Nine Tails', the Yellow Tiger's, the Fire King's, the Red
	// Dragon's, the Demon King's, the Reaper's, the Nine-Tailed Fox's, the
	// Giant Tree's and Chegal's - every one of them a giftbox in item_proto.
	// Kept beside the rendered table rather than in it: the table is his
	// personality document's list, and this one is the patch's.
	const DWORD PLAYERBOT_MOOD_BOSS_CASKET_VNUMS[] = {
		50070, 50071, 50072, 50073, 50074, 50075, 50076, 50077,
		50078, 50079, 50080, 50081, 50082, 50090, 50097, 50098,
	};

	// Is this item on Iwakura's list of valuable drops? A skill book and a
	// Forgetting book are one vnum each, told apart by the skill in socket 0;
	// a family of gear counts at any refine.
	bool IsPlayerBotMoodValuable(DWORD vnum, long socket0, BYTE type)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_MOOD_BOSS_CASKET_VNUMS) / sizeof(PLAYERBOT_MOOD_BOSS_CASKET_VNUMS[0]); ++i)
			if (PLAYERBOT_MOOD_BOSS_CASKET_VNUMS[i] == vnum)
				return true;
		if (vnum == PLAYERBOT_MOOD_SKILL_BOOK_VNUM)
			return IsPlayerBotSkillInList(PLAYERBOT_MOOD_VALUABLE_BOOK_SKILLS,
					sizeof(PLAYERBOT_MOOD_VALUABLE_BOOK_SKILLS), socket0);
		if (vnum == PLAYERBOT_MOOD_FORGET_BOOK_VNUM)
			return IsPlayerBotSkillInList(PLAYERBOT_MOOD_VALUABLE_FORGET_SKILLS,
					sizeof(PLAYERBOT_MOOD_VALUABLE_FORGET_SKILLS), socket0);
		if (PLAYERBOT_MOOD_ANY_POLYMORPH && type == ITEM_POLYMORPH)
			return true;
		const size_t singles = sizeof(PLAYERBOT_MOOD_VALUABLE_VNUMS) / sizeof(PLAYERBOT_MOOD_VALUABLE_VNUMS[0]);
		if (std::binary_search(PLAYERBOT_MOOD_VALUABLE_VNUMS, PLAYERBOT_MOOD_VALUABLE_VNUMS + singles, vnum))
			return true;
		const DWORD base = vnum / 10 * 10;
		const size_t families = sizeof(PLAYERBOT_MOOD_VALUABLE_FAMILIES) / sizeof(PLAYERBOT_MOOD_VALUABLE_FAMILIES[0]);
		for (size_t i = 0; i < families; ++i)
			if (PLAYERBOT_MOOD_VALUABLE_FAMILIES[i] == base)
				return true;
		return false;
	}

	bool IsPlayerBotMoodValuableItem(LPITEM item)
	{
		return item && IsPlayerBotMoodValuable(item->GetVnum(), item->GetSocket(0), item->GetType());
	}

	// How many valuable units the bag holds: the chest pass counts before and
	// after an opening, because what a giftbox hands out goes straight into the
	// bag and nothing else says what it was.
	int CountPlayerBotMoodValuables(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return 0;
		int count = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotMoodValuableItem(item))
				count += (int)item->GetCount();
		}
		return count;
	}

	const char* GetPlayerBotMoodName(BYTE mood)
	{
		switch (mood)
		{
			case playerbot_persona::MOOD_SLABY: return "slaby";
			case playerbot_persona::MOOD_BARDZO_DOBRY: return "bardzo dobry";
			default: return "normalny";
		}
	}

	TPlayerBotPersona* FindPlayerBotPersona(LPCHARACTER ch)
	{
		if (!ch)
			return NULL;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		return it == s_mapPlayerBotAIStates.end() ? NULL : &it->second.persona;
	}

	// A mercenary's contract, either side (playerbot_companions.h).
	bool IsPlayerBotOnMercContract(DWORD pid);
	// Called over by a person's whisper (playerbot_chat_conversation.h).
	inline bool IsPlayerBotSummoned(DWORD botPID);

	// Playing alone, in the document's sense: no party, no dungeon, no raid,
	// no war, no duel. Everywhere else the bot plays NORMALNY whatever it
	// feels ("zachowanie zostaje sztywno zablokowane na poziomie NORMALNY").
	// A mercenary's contract counts as company for its whole length, the
	// pause included ("blokuje nastroj obu botow ... na czas trwania
	// kontraktu"), though the party is apart while the mercenary is in town.
	// A bot a person called over is in that person's company too: a SLABY
	// bot's pause and AFK stop sit above the summon in the tick and would
	// leave the person waiting for a bot that stopped halfway.
	bool IsPlayerBotPlayingAlone(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetParty() != NULL || IsPlayerBotOnMercContract(ch->GetPlayerID()) ||
				IsPlayerBotSummoned(ch->GetPlayerID()))
			return false;
		if (ch->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			return false;
		if (IsPlayerBotOnTowerBusiness(ch, state) || state.dwGuildWarEnemyGID != 0)
			return false;
		return playerbot_pvp::GetDuelOpponent(ch->GetPlayerID(), dwNow) == 0;
	}

	// The mood the bot plays by right now: its own when alone, NORMALNY in
	// company, and NORMALNY for everybody while the switch is off.
	BYTE GetPlayerBotPlayMood(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return playerbot_persona::MOOD_NORMALNY;
		return playerbot_persona::EffectiveMood(state.persona.mood,
				!IsPlayerBotPlayingAlone(ch, state, dwNow));
	}

	void SavePlayerBotPersonaState(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (!ch || !p.bRestored)
			return;
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_MOOD, (int)p.mood.mood + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK, (int)p.mood.lockKind + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK_LEFT, (int)(p.mood.lockLeftMs / 1000) + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_PLAYED, (int)(p.mood.playedMs / 1000) + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_DROUGHT, (int)(p.mood.droughtMs / 1000) + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_ADVANCED, (p.bAdvanced ? 1 : 0) + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK_LEVEL, (int)p.bLockLevel + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_QUIT, (p.bQuitGrinding ? 1 : 0) + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_QUIT_TIER, (int)p.bQuitRolledTier + 1);
		ch->SetQuestFlag(PLAYERBOT_PERSONA_FLAG_MEDAL_GOAL, (p.bMedalGoalDone ? 1 : 0) + 1);
		p.bDirty = false;
		p.dwNextSave = dwNow + PLAYERBOT_PERSONA_SAVE_INTERVAL;
	}

	// Once, as soon as the flags have arrived. A bot that has never had a mood
	// draws one: the document's rotation is "calkowicie losowy", and a world
	// that switched the system on should not wake up with every bot the same.
	bool RestorePlayerBotPersonaState(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (p.bRestored)
			return true;
		if (!ch)
			return false;
		quest::PC* pc = quest::CQuestManager::instance().GetPC(ch->GetPlayerID());
		if (!pc || !pc->IsLoaded())
			return false;
		const int mood = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_MOOD);
		if (mood > 0)
		{
			p.mood.mood = playerbot_persona::ClampMood((unsigned int)(mood - 1));
			const int lock = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK) - 1;
			const int lockLeft = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK_LEFT) - 1;
			if (lock > playerbot_persona::MOOD_LOCK_NONE && lock <= playerbot_persona::MOOD_LOCK_CAPITULATION &&
					lockLeft > 0)
			{
				p.mood.lockKind = (uint8_t)lock;
				p.mood.lockLeftMs = (uint32_t)lockLeft * 1000u;
			}
			const int played = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_PLAYED) - 1;
			const int drought = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_DROUGHT) - 1;
			p.mood.playedMs = played > 0 ? (uint32_t)played * 1000u : 0;
			p.mood.droughtMs = drought > 0 ? (uint32_t)drought * 1000u : 0;
		}
		else
		{
			p.mood.mood = playerbot_persona::RollMood((uint32_t)number(0, 299));
			p.bDirty = true;
		}
		p.bAdvanced = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_ADVANCED) - 1 == 1;
		const int lockLevel = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_LOCK_LEVEL) - 1;
		p.bLockLevel = lockLevel > 0 && lockLevel <= PLAYER_MAX_LEVEL_CONST ? (BYTE)lockLevel : 0;
		p.bQuitGrinding = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_QUIT) - 1 == 1;
		const int quitTier = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_QUIT_TIER) - 1;
		p.bQuitRolledTier = quitTier > 0 && quitTier < 256 ? (BYTE)quitTier : 0;
		p.bMedalGoalDone = ch->GetQuestFlag(PLAYERBOT_PERSONA_FLAG_MEDAL_GOAL) - 1 == 1;
		// A goal dropper that graduated before this login plays as the
		// Wanderer its draw would otherwise have made it (community patch 2,
		// point 4); the operator's cohort is the operator's.
		if (p.bMedalGoalDone && state.bPersonality == BOT_PERSONALITY_MEDAL_DROPPER &&
				!CPlayerBotManager::instance().IsMedalDropperCohortPID(ch->GetPlayerID()))
			state.bPersonality = BOT_PERSONALITY_WANDERER;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// Where the engine's count of deaths at a player's hands stands, so the
		// next death can be told apart (WasPlayerBotKilledByPlayer).
		p.llPlayerDeaths = ch->GetSpecialFlag((DWORD)PLAYER_STATS_DEATH_FROM_PLAYER_FLAG);
#endif
		p.bRestored = true;
		p.dwLastTick = dwNow;
		p.dwNextSave = dwNow + PLAYERBOT_PERSONA_SAVE_INTERVAL;
		// The first stop from the keyboard is never on the first minute.
		p.dwNextAfkAt = dwNow + playerbot_persona::AfkInterval((uint32_t)number(0, 0x7fffffff));
		sys_log(0, "PLAYERBOT_MOOD: restored pid=%u name=%s mood=%s lock=%u lock_min=%u played_min=%u drought_min=%u advanced=%d lock_level=%u fresh=%d",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(p.mood.mood),
				(unsigned int)p.mood.lockKind, p.mood.lockLeftMs / 60000u, p.mood.playedMs / 60000u,
				p.mood.droughtMs / 60000u, p.bAdvanced ? 1 : 0, (unsigned int)p.bLockLevel, mood > 0 ? 0 : 1);
		return true;
	}

	// The mood's clocks, once a full tick. The drought clock runs while the
	// bot hunts - off the village maps, or in a fight anywhere - and an angler
	// or a miner is not hunting.
	void AdvancePlayerBotMood(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (!ch || !IsPlayerBotPersonaEnabled())
		{
			p.dwLastTick = dwNow;
			return;
		}
		if (!RestorePlayerBotPersonaState(ch, state, dwNow))
			return;
		DWORD dt = p.dwLastTick != 0 ? dwNow - p.dwLastTick : 0;
		p.dwLastTick = dwNow;
		if (dt > PLAYERBOT_PERSONA_TICK_MAX_DT)
			dt = 0;
		// The Conqueror's deaths age on the same clock (NotePlayerBotPersonaDeath).
		playerbot_persona::AdvanceDeathWindow(p.deaths, dt);
		const bool fighting = state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime < PLAYERBOT_MOOD_HUNTING_COMBAT_MS;
		const bool hunting = (fighting || !IsPlayerBotVillageMap(ch->GetMapIndex())) &&
				!state.bFishingSession && !IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow);
		const uint8_t before = p.mood.mood;
		const int events = playerbot_persona::AdvanceMood(p.mood, dt, hunting,
				(uint32_t)number(0, 0x7fffffff));
		if (events & playerbot_persona::MOOD_EVENT_UNLOCKED)
		{
			sys_log(0, "PLAYERBOT_MOOD: lock over pid=%u name=%s mood=%s",
					ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(p.mood.mood));
			p.bDirty = true;
		}
		if (events & (playerbot_persona::MOOD_EVENT_ROTATED | playerbot_persona::MOOD_EVENT_DROUGHT))
		{
			sys_log(0, "PLAYERBOT_MOOD: %s pid=%u name=%s from=%s to=%s",
					(events & playerbot_persona::MOOD_EVENT_ROTATED) ? "rotated" : "drought",
					ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(before),
					GetPlayerBotMoodName(p.mood.mood));
			p.bDirty = true;
		}
		if (p.bDirty || dwNow >= p.dwNextSave)
			SavePlayerBotPersonaState(ch, state, dwNow);
	}

	// Something from the list came to hand: a pickup, a chest, the water.
	void NotePlayerBotMoodValuable(LPCHARACTER ch, DWORD vnum, long socket0, BYTE type, const char* szHow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !IsPlayerBotMoodValuable(vnum, socket0, type))
			return;
		TPlayerBotPersona* p = FindPlayerBotPersona(ch);
		if (!p || !p->bRestored)
			return;
		const uint8_t before = p->mood.mood;
		if (playerbot_persona::OnValuableDrop(p->mood))
		{
			sys_log(0, "PLAYERBOT_MOOD: better pid=%u name=%s from=%s to=%s how=%s vnum=%u socket=%ld",
					ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(before),
					GetPlayerBotMoodName(p->mood.mood), szHow, vnum, socket0);
			p->bDirty = true;
		}
	}

	// The same for a count of units that came into the bag by a way nothing
	// names - a giftbox hands its set out one by one.
	void NotePlayerBotMoodValuableCount(LPCHARACTER ch, int gained, const char* szHow)
	{
		if (!ch || gained <= 0 || !IsPlayerBotPersonaEnabled())
			return;
		TPlayerBotPersona* p = FindPlayerBotPersona(ch);
		if (!p || !p->bRestored)
			return;
		const uint8_t before = p->mood.mood;
		if (playerbot_persona::OnValuableDrop(p->mood))
		{
			sys_log(0, "PLAYERBOT_MOOD: better pid=%u name=%s from=%s to=%s how=%s units=%d",
					ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(before),
					GetPlayerBotMoodName(p->mood.mood), szHow, gained);
			p->bDirty = true;
		}
	}

	// A refine that landed. +8 and +9 are the document's euphoria: BARDZO
	// DOBRY for three hours, and nothing changes it.
	void NotePlayerBotMoodRefine(LPCHARACTER ch, int newPlus)
	{
		if (!ch || newPlus < 8 || !IsPlayerBotPersonaEnabled())
			return;
		TPlayerBotPersona* p = FindPlayerBotPersona(ch);
		if (!p || !p->bRestored)
			return;
		const uint8_t before = p->mood.mood;
		playerbot_persona::OnEuphoria(p->mood);
		sys_log(0, "PLAYERBOT_MOOD: euphoria pid=%u name=%s from=%s plus=%d hours=3",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(before), newPlus);
		p->bDirty = true;
	}

	// And one that failed on its way to +8 or +9: burned at the anvil, or a
	// grade down under a scroll - one level down (OnBigRefineFailure, which
	// holds under a lock). targetPlus is the grade the attempt was for.
	void NotePlayerBotMoodRefineFailure(LPCHARACTER ch, int targetPlus, const char* szHow)
	{
		if (!ch || targetPlus < playerbot_persona::MOOD_BIG_REFINE_PLUS || !IsPlayerBotPersonaEnabled())
			return;
		TPlayerBotPersona* p = FindPlayerBotPersona(ch);
		if (!p || !p->bRestored)
			return;
		const uint8_t before = p->mood.mood;
		if (!playerbot_persona::OnBigRefineFailure(p->mood, (uint8_t)targetPlus))
			return;
		sys_log(0, "PLAYERBOT_MOOD: worse pid=%u name=%s from=%s to=%s plus=%d how=%s",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotMoodName(before),
				GetPlayerBotMoodName(p->mood.mood), targetPlus, szHow ? szHow : "?");
		p->bDirty = true;
	}

	// The REST slider's town rest belongs to SLABY alone while the system is on
	// (the operator's choice, 19 September): NORMALNY and BARDZO DOBRY take a
	// break only for what the game makes them do.
	bool PlayerBotMoodAllowsTownRest(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return true;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		return GetPlayerBotPlayMood(ch, it->second, get_dword_time()) == playerbot_persona::MOOD_SLABY;
	}
}

#endif
