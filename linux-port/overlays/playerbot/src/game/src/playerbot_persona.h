#ifndef __INC_METIN2_PLAYERBOT_PERSONA_H__
#define __INC_METIN2_PLAYERBOT_PERSONA_H__

// Iwakura's personalities, the engine's half: which one claims a bot, the
// Grinder's experience lock and the Law of Advancement, the Conqueror who
// outgrows its gear, and the two habits of a bad mood - the pause between two
// packs and the stop from the keyboard.
//
// Most of what the document asks of a personality this AI already did, under
// other names and at other thresholds: the town visit is the Trader, the
// blacksmith and the market the Perfectionist, the stone fight the Metin
// Slayer, the fishing and mining sessions the Fisherman and the Miner, a party
// the Companion. What is new is that one of them is the bot's answer to its
// situation, computed again every planning pass, shown in both panels and over
// the bot's head - and the rules that make a bot change between them: the
// Grinder that holds its level until its gear is ready for the next tier, the
// Conqueror that levels once it is. The rest of the document lands on the
// subsystems it concerns (see the PERSONA switch).
//
// The old personality drawn by pid at login is the bot's character now
// (GetPlayerBotCharakter): it tilts the odds and nothing else, and the Metin,
// M2 and M3 droppers give way to the Grinder's tiers (the operator's choice,
// 19 September). The launcher's medal-dropper cohort is not a draw and stays
// exactly what it was.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_planner.h - it reads the bag, the gear and
// the sessions - and before playerbot_town.h and the manager's tick.

namespace
{
	const char* GetPlayerBotPersonaName(BYTE persona)
	{
		switch (persona)
		{
			case playerbot_persona::PERSONA_GRINDER: return "Grinder";
			case playerbot_persona::PERSONA_ZDOBYWCA: return "Zdobywca";
			case playerbot_persona::PERSONA_HANDLARZ: return "Handlarz";
			case playerbot_persona::PERSONA_HAZARDZISTA: return "Hazardzista";
			case playerbot_persona::PERSONA_PERFEKCJONISTA: return "Perfekcjonista";
			case playerbot_persona::PERSONA_POGROMCA: return "Pogromca metinow";
			case playerbot_persona::PERSONA_GORNIK: return "Gornik";
			case playerbot_persona::PERSONA_RYBAK: return "Rybak";
			case playerbot_persona::PERSONA_NAJEMNIK: return "Najemnik";
			case playerbot_persona::PERSONA_TOWARZYSZ: return "Towarzysz";
			case playerbot_persona::PERSONA_METINOLOG: return "Metinolog";
			case playerbot_persona::PERSONA_NALOGOWIEC: return "Nalogowiec";
			case playerbot_persona::PERSONA_NAUKOWIEC: return "Szalony Naukowiec";
			case playerbot_persona::PERSONA_EGZEKUTOR: return "Egzekutor";
			case playerbot_persona::PERSONA_WEDKARZ: return "Szalony Wedkarz";
			default: return "?";
		}
	}

	// The character a drawn personality becomes under the switch. A dropper's
	// farm is a Grinder's tier now, so the draw keeps what the dropper was
	// for - the stones, the level-30 weapons, the horse - as a character that
	// leans that way, and loses the fixed lock and the fixed ground.
	BYTE GetPlayerBotCharakter(BYTE drawn)
	{
		switch (drawn)
		{
			case BOT_PERSONALITY_METIN_DROPPER: return BOT_PERSONALITY_METIN_BREAKER;
			case BOT_PERSONALITY_M3_DROPPER:
			case BOT_PERSONALITY_M2_DROPPER: return BOT_PERSONALITY_GEAR_SPECIALIST;
			// The medal dropper stays one: community patch 2, point 4 asks for
			// four and a half times as many, where this line made none.
			case BOT_PERSONALITY_MEDAL_DROPPER: return BOT_PERSONALITY_MEDAL_DROPPER;
			default: return drawn;
		}
	}

	// The chance in percent that a Grinder which meets the law moves on when
	// asked, by its character: an equipment specialist stays longer for its +8
	// and +9, a wanderer does not wait at all.
	int GetPlayerBotAdvanceChance(BYTE charakter)
	{
		switch (charakter)
		{
			case BOT_PERSONALITY_GEAR_SPECIALIST: return PLAYERBOT_PERSONA_ADVANCE_CHANCE / 2;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR: return PLAYERBOT_PERSONA_ADVANCE_CHANCE * 2 / 3;
			case BOT_PERSONALITY_WANDERER: return 100;
			case BOT_PERSONALITY_METIN_BREAKER:
			case BOT_PERSONALITY_TEAM_COMPANION: return PLAYERBOT_PERSONA_ADVANCE_CHANCE + 15;
			default: return PLAYERBOT_PERSONA_ADVANCE_CHANCE;
		}
	}

	BYTE GetPlayerBotPersonaLevelLimit(LPITEM item)
	{
		const TItemTable* proto = item ? item->GetProto() : NULL;
		if (!proto)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return (BYTE)std::max<long>(0, std::min<long>(255, proto->aLimits[i].lValue));
		return 0;
	}

	playerbot_persona::TGearPiece GetPlayerBotPersonaPiece(LPITEM item, bool premium)
	{
		if (!item)
			return playerbot_persona::TGearPiece();
		return playerbot_persona::TGearPiece((uint8_t)item->GetRefineLevel(),
				GetPlayerBotPersonaLevelLimit(item), premium);
	}

	playerbot_persona::TAdvanceGear GetPlayerBotAdvanceGear(LPCHARACTER ch)
	{
		playerbot_persona::TAdvanceGear gear;
		if (!ch)
			return gear;
		gear.level = (uint8_t)std::min<int>(255, ch->GetLevel());
		// The hand's weapon, not a rod or a pickaxe the session put there.
		LPITEM weapon = GetPlayerBotHandWeapon(ch);
		gear.weapon = GetPlayerBotPersonaPiece(weapon,
				weapon && IsPlayerBotSpecialLevel30Weapon(weapon));
		gear.armour = GetPlayerBotPersonaPiece(ch->GetWear(WEAR_BODY), false);
		gear.shield = GetPlayerBotPersonaPiece(ch->GetWear(WEAR_SHIELD), false);
		// The mask or the helmet, which the law asks for from level 35 on.
		gear.helmet = GetPlayerBotPersonaPiece(ch->GetWear(WEAR_HEAD), false);
		gear.wantsShield = PlayerBotWantsShield(ch);
		return gear;
	}

	bool IsPlayerBotMentalWarrior(LPCHARACTER ch)
	{
		return ch && ch->GetJob() == JOB_WARRIOR && ch->GetSkillGroup() == 2;
	}

	// The lock a medal dropper holds at instead of a Grinder's, or zero for
	// any other bot: the operator's cohort at the cohort's level, one drawn
	// under the personalities at its dungeon's (community patch 2, point 4) -
	// the answer ManagePlayerBotExpLock holds it at.
	BYTE GetPlayerBotMedalDropperLock(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch)
			return 0;
		if (CPlayerBotManager::instance().IsMedalDropperCohortPID(ch->GetPlayerID()))
			return CPlayerBotManager::instance().GetMedalDropperCohortLevel();
		return state.bPersonality == BOT_PERSONALITY_MEDAL_DROPPER ? PLAYERBOT_EXP_LOCK_MEDAL_DROPPER : 0;
	}

	// The level this bot holds at under the switch, or zero for none: a
	// Conqueror holds nowhere, a Grinder at the lock it reached or, until it
	// has reached one, at its tier's lock once it gets there. The lock reached
	// is kept, so the bot does not slide into the next tier's lock by the level
	// it has just been held at.
	BYTE GetPlayerBotPersonaLockLevel(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return 0;
		TPlayerBotPersona& p = state.persona;
		// A medal dropper is held at its dungeon's lock whatever tier its level
		// falls in, and never advances out of it (the goal dropper leaves by
		// its gear, every other by the dropper band). Written here as well, so
		// the status file, the census and the Law of Advancement read the lock
		// the bot is actually held at: they read a Grinder's tier lock before,
		// and a dropper of eighteen in the Monkey Dungeon showed a lock of
		// eighteen and could "advance" to Conqueror while it levelled to 33.
		const BYTE dropperLock = GetPlayerBotMedalDropperLock(ch, state);
		if (dropperLock != 0)
		{
			const BYTE held = ch->GetLevel() >= dropperLock ? dropperLock : 0;
			if (p.bLockLevel != held || p.bAdvanced)
			{
				p.bLockLevel = held;
				p.bAdvanced = false;
				p.dwNextAdvanceRoll = 0;
				p.bDirty = true;
			}
			return dropperLock;
		}
		if (p.bAdvanced)
			return 0;
		const BYTE level = (BYTE)std::min<int>(255, ch->GetLevel());
		// The Grinders who never hold, and the ones who gave grinding up
		// (community patch 2, point 2): no lock, and one written before goes.
		if (playerbot_persona::NeverHoldsAtLocks(ch->GetPlayerID()) || p.bQuitGrinding)
		{
			if (p.bLockLevel != 0)
			{
				sys_log(0, "PLAYERBOT_PERSONA: grinder lock lifted pid=%u name=%s level=%u was=%u why=%s",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)level, (unsigned int)p.bLockLevel,
						p.bQuitGrinding ? "quit" : "never_holds");
				p.bLockLevel = 0;
				p.bDirty = true;
			}
			return 0;
		}
		const BYTE lock = playerbot_persona::GrinderLockFor(level, ch->GetPlayerID());
		if (p.bLockLevel != 0 && lock == 0)
		{
			// Today's draw says this bot is held nowhere - it is past the last
			// tier, or it is one of the quarter that walks through the first
			// village - so a lock written under an older rule goes. Without
			// this the fix above reaches only bots that have not been locked
			// yet, and m2zip's 112 bots frozen at 71 would stay there.
			sys_log(0, "PLAYERBOT_PERSONA: grinder lock lifted pid=%u name=%s level=%u was=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)level,
					(unsigned int)p.bLockLevel);
			p.bLockLevel = 0;
			p.bDirty = true;
			return 0;
		}
		if (p.bLockLevel != 0)
		{
			// A lock written before Community Patch 1 is the old fixed number
			// - fifteen for every bot of the first village, twenty-three for
			// every bot of M3 - and a world that has been played holds
			// thousands of them. Keeping them would leave the patch reaching
			// no bot that already stands on one, which is the whole village.
			// So a lock is raised to what this bot's pid draws today, and
			// only raised: lowering it would hand a bot a level it has already
			// passed, and only inside the tier it is already in, so nothing
			// slides into the next tier's band.
			if (lock > p.bLockLevel &&
					playerbot_persona::GrinderTierFor(p.bLockLevel) ==
						playerbot_persona::GrinderTierFor(level))
			{
				sys_log(0, "PLAYERBOT_PERSONA: grinder lock redrawn pid=%u name=%s was=%u now=%u",
						ch->GetPlayerID(), ch->GetName(),
						(unsigned int)p.bLockLevel, (unsigned int)lock);
				p.bLockLevel = lock;
				p.bDirty = true;
			}
			return p.bLockLevel;
		}
		if (lock != 0 && level >= lock)
		{
			p.bLockLevel = lock;
			p.bDirty = true;
			sys_log(0, "PLAYERBOT_PERSONA: grinder holds pid=%u name=%s level=%u tier=%u lock=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)level,
					(unsigned int)playerbot_persona::GrinderTierFor(level), (unsigned int)lock);
		}
		return lock;
	}

	// Community patch 2, point 14: one in a hundred of the first village's
	// Grinders farms materials there for sale and does not advance until the
	// purse pays for its class's level-30 weapon at +8 and an armour of level
	// 18 or 26 at +9 - or it holds them. Priced on Iwakura's sheet.
	bool IsPlayerBotM1Farmer(DWORD pid)
	{
		return (int)(PlayerBotNavHash(pid ^ 0x4d314641U) % 100U) < PLAYERBOT_M1_FARMER_PERCENT;
	}

	long long GetPlayerBotGearFamilyPrice(DWORD familyBase, int plus);

	bool PlayerBotM1FarmerGoalMet(LPCHARACTER ch)
	{
		if (!ch)
			return true;
		LPITEM weapon = FindPlayerBotClassLevel30Weapon(ch);
		LPITEM armour = ch->GetWear(WEAR_BODY);
		const DWORD armourBase = GetPlayerBotArmorClassBase(ch);
		const bool weaponDone = weapon && (int)weapon->GetRefineLevel() >= PLAYERBOT_M1_FARMER_WEAPON_PLUS;
		const DWORD armourFamily = armour ? armour->GetVnum() - (DWORD)std::max(0, armour->GetRefineLevel()) : 0;
		const bool armourDone = armour && (int)armour->GetRefineLevel() >= PLAYERBOT_M1_FARMER_ARMOUR_PLUS &&
				(armourFamily == armourBase + 20 || armourFamily == armourBase + 30);
		if (weaponDone && armourDone)
			return true;
		long long cost = 0;
		if (!weaponDone)
		{
			// The cheapest of the class's level-30 families it could wield.
			static const DWORD families[] = { 290, 1170, 2150, 3210, 5110, 7160 };
			long long cheapest = 0;
			for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); ++i)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(families[i]);
				if (!proto || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType) || !IsPlayerBotProtoForCharacter(ch, proto))
					continue;
				const long long price = GetPlayerBotGearFamilyPrice(families[i], PLAYERBOT_M1_FARMER_WEAPON_PLUS);
				if (price > 0 && (cheapest == 0 || price < cheapest))
					cheapest = price;
			}
			cost += cheapest;
		}
		if (!armourDone)
			cost += GetPlayerBotGearFamilyPrice(armourBase + 20, PLAYERBOT_M1_FARMER_ARMOUR_PLUS);
		return cost > 0 && (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) >= cost;
	}

	// The Law of Advancement, asked of a Grinder that has reached its lock.
	// Meeting it does not move the bot on: it is asked when the law is first
	// met and then once an hour, and its character says how likely it is to
	// go - "moze podjac decyzje o przedluzeniu pobytu na obecnym spocie", to
	// push its gear to +8 and +9 first.
	void ManagePlayerBotAdvancement(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return;
		TPlayerBotPersona& p = state.persona;
		// A medal dropper's lock is its dungeon's and the law does not move it
		// on: the lock is only written down (GetPlayerBotPersonaLockLevel).
		if (GetPlayerBotMedalDropperLock(ch, state) != 0)
		{
			GetPlayerBotPersonaLockLevel(ch, state);
			return;
		}
		if (p.bAdvanced)
			return;
		const BYTE lock = GetPlayerBotPersonaLockLevel(ch, state);
		if (lock == 0 || ch->GetLevel() < lock)
			return;
		const int gaps = playerbot_persona::AwansGaps(GetPlayerBotAdvanceGear(ch));
		if (gaps != 0)
		{
			p.dwNextAdvanceRoll = 0;
			return;
		}
		// The first village's farmer stays until its goal is paid for.
		if (playerbot_persona::GrinderTierFor((uint8_t)std::min<int>(255, ch->GetLevel())) == 1 &&
				IsPlayerBotM1Farmer(ch->GetPlayerID()) && !PlayerBotM1FarmerGoalMet(ch))
		{
			PlayerBotLogThrottled("m1_farmer_stays", dwNow,
					"PLAYERBOT_PERSONA: first village farmer stays for its goal pid=%u name=%s level=%u gold=%lld",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (long long)ch->GetGold());
			p.dwNextAdvanceRoll = 0;
			return;
		}
		// The tenth that may give grinding up rolls its 33% once a tier, the
		// moment the tier's gear stands (community patch 2, point 2).
		if (playerbot_persona::MayQuitGrinding(ch->GetPlayerID()) && !p.bQuitGrinding)
		{
			const BYTE tier = playerbot_persona::GrinderTierFor((uint8_t)std::min<int>(255, ch->GetLevel()));
			if (tier != 0 && tier != p.bQuitRolledTier)
			{
				p.bQuitRolledTier = tier;
				p.bDirty = true;
				if (playerbot_persona::RollQuitGrinding((uint32_t)number(0, 99)))
				{
					p.bQuitGrinding = true;
					p.bLockLevel = 0;
					p.dwNextAdvanceRoll = 0;
					sys_log(0, "PLAYERBOT_PERSONA: grinder gives grinding up pid=%u name=%s level=%u tier=%u lock=%u",
							ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(),
							(unsigned int)tier, (unsigned int)lock);
					return;
				}
			}
		}
		if (p.dwNextAdvanceRoll == 0)
		{
			p.dwNextAdvanceRoll = dwNow + PLAYERBOT_PERSONA_ADVANCE_FIRST_ROLL;
			sys_log(0, "PLAYERBOT_PERSONA: law met pid=%u name=%s level=%u lock=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)lock);
			return;
		}
		if (dwNow < p.dwNextAdvanceRoll)
			return;
		const int chance = GetPlayerBotAdvanceChance(state.bPersonality);
		if (number(1, 100) > chance)
		{
			p.dwNextAdvanceRoll = dwNow + PLAYERBOT_PERSONA_ADVANCE_ROLL_INTERVAL;
			sys_log(0, "PLAYERBOT_PERSONA: grinder stays to push its gear pid=%u name=%s level=%u chance=%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), chance);
			return;
		}
		p.bAdvanced = true;
		p.bLockLevel = 0;
		p.dwNextAdvanceRoll = 0;
		p.bDirty = true;
		const LPITEM weapon = GetPlayerBotHandWeapon(ch);
		const LPITEM armour = ch->GetWear(WEAR_BODY);
		const LPITEM shield = ch->GetWear(WEAR_SHIELD);
		sys_log(0, "PLAYERBOT_PERSONA: advances pid=%u name=%s level=%u weapon=%u+%u armour=%u+%u shield=%u+%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(),
				weapon ? weapon->GetVnum() : 0, weapon ? (unsigned int)weapon->GetRefineLevel() : 0,
				armour ? armour->GetVnum() : 0, armour ? (unsigned int)armour->GetRefineLevel() : 0,
				shield ? shield->GetVnum() : 0, shield ? (unsigned int)shield->GetRefineLevel() : 0);
	}

	// Whether the death just detected was at a player's hands. The engine
	// counts those itself on mt2009 (PLAYER_STATS_DEATH_FROM_PLAYER_FLAG,
	// ENABLE_QUEST_DIE_EVENT in CHARACTER::Dead); r40250 counts nothing and
	// every death reads as a monster's there.
	bool WasPlayerBotKilledByPlayer(LPCHARACTER ch, TPlayerBotPersona& p)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		const long long now = ch ? ch->GetSpecialFlag((DWORD)PLAYER_STATS_DEATH_FROM_PLAYER_FLAG) : 0;
		const bool byPlayer = p.llPlayerDeaths >= 0 && now > p.llPlayerDeaths;
		p.llPlayerDeaths = now;
		return byPlayer;
#else
		(void)ch;
		(void)p;
		return false;
#endif
	}

	// Whether this point is on the ground a capitulation gave up (the Anti-PK
	// protocol, playerbot_anti_pk.h): the target search and the hunting-ground
	// choice leave it alone for as long as it is given up.
	bool IsPlayerBotAvoidedSpot(const TPlayerBotAIState& state, long mapIndex, long x, long y, DWORD dwNow)
	{
		const TPlayerBotPersona& p = state.persona;
		return p.dwAvoidSpotUntil != 0 && dwNow < p.dwAvoidSpotUntil &&
				playerbot_persona::IsPkSameSpot(p.lAvoidSpotMap, p.lAvoidSpotX, p.lAvoidSpotY, mapIndex, x, y);
	}

	// A village's hub choices less the ones on given-up ground - unless that
	// is all of them, and then all of them: somewhere to hunt beats nowhere.
	int FilterPlayerBotAvoidedHubs(const TPlayerBotAIState& state, long mapIndex,
			const TPlayerBotVillageHub* hubs, int* choices, int count, DWORD dwNow)
	{
		if (!hubs || !choices || count <= 0 || state.persona.dwAvoidSpotUntil == 0 ||
				dwNow >= state.persona.dwAvoidSpotUntil)
			return count;
		int kept = 0;
		for (int i = 0; i < count; ++i)
			if (!IsPlayerBotAvoidedSpot(state, mapIndex, hubs[choices[i]].x, hubs[choices[i]].y, dwNow))
				choices[kept++] = choices[i];
		return kept > 0 ? kept : count;
	}

	// The Anti-PK protocol's and the stone hunter's half of a death
	// (playerbot_anti_pk.h, after the fight it is about).
	void NotePlayerBotAntiPkDeath(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, bool byPlayer);
	// The mercenary's side of the same count, and the two sides of a contract
	// the personality is decided by (playerbot_companions.h).
	void NotePlayerBotDistress(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);
	bool IsPlayerBotMercenaryOnContract(DWORD pid);
	bool IsPlayerBotHiredClient(DWORD pid);

	// A death, noticed on the tick it happened. A Conqueror dying to monsters
	// three times in half an hour has outgrown its gear: it becomes a Grinder
	// again, held where it stands, until its gear meets the law for this level.
	void NotePlayerBotPersonaDeath(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return;
		TPlayerBotPersona& p = state.persona;
		const bool byPlayer = WasPlayerBotKilledByPlayer(ch, p);
		NotePlayerBotAntiPkDeath(ch, state, dwNow, byPlayer);
		if (byPlayer)
			return;
		const bool weak = playerbot_persona::NoteDeath(p.deaths);
		// Three deaths to monsters in half an hour is a bot that visibly cannot
		// cope, which a stronger one of its kingdom may offer to carry.
		if (weak)
			NotePlayerBotDistress(ch, state, dwNow);
		if (!weak || !p.bAdvanced)
			return;
		p.bAdvanced = false;
		p.bLockLevel = (BYTE)std::min<int>(255, ch->GetLevel());
		p.dwNextAdvanceRoll = 0;
		p.deaths = playerbot_persona::TDeathWindow();
		p.bDirty = true;
		sys_log(0, "PLAYERBOT_PERSONA: outgrew its gear, back to grinding pid=%u name=%s level=%u map=%ld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex());
	}

	// M3's door for a Grinder of the second tier, the document's own: a weapon
	// at +6 and an armour at +5, the Mental Warrior with the weapon alone.
	bool MeetsPlayerBotM3Survival(LPCHARACTER ch)
	{
		return ch && playerbot_persona::MeetsM3Survival(GetPlayerBotAdvanceGear(ch),
				IsPlayerBotMentalWarrior(ch));
	}

	// Which personality claims the bot now. Recomputed every planning pass, so
	// it is the bot's answer to its situation and never a label it carries.
	void DecidePlayerBotPersona(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return;
		TPlayerBotPersona& p = state.persona;
		playerbot_persona::TPersonaSignals s;
		s.inParty = ch->GetParty() != NULL;
		s.mercenary = IsPlayerBotMercenaryOnContract(ch->GetPlayerID());
		s.hired = IsPlayerBotHiredClient(ch->GetPlayerID());
		s.fishing = state.bFishingSession;
		s.mining = IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow);
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		s.stoneFight = (target && target->IsStone() && !target->IsDead()) ||
				state.dwStoneProgressVID != 0;
		// The gambler's session (playerbot_gambler.h) outranks the anvil's own
		// work; the anvil and the counters for the bot itself are the
		// Perfectionist; everything else in town is the Trader emptying its bag.
		s.gambling = IsPlayerBotGambling(state, dwNow);
		s.perfecting = (state.bVisitingShop && state.bTownNeedBlacksmith) || state.bMarketTrip;
		p.bBagFull = IsPlayerBotBagFull(ch);
		s.trading = p.bBagFull || state.bVisitingShop ||
				state.bCurrentAction == BOT_ACTION_STALL || ch->GetMyShop() != NULL;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		s.trading = s.trading || state.offlineShop.visiting;
#endif
		s.advanced = p.bAdvanced;
		// A rare personality is the bot's for its whole length (Iwakura's
		// Patch 3, point 7).
		const BYTE rare = GetPlayerBotRareNow(p, dwNow);
		if (rare != 0)
			s.rare = playerbot_persona::GetRareRule(rare).persona;
		const BYTE persona = playerbot_persona::DecidePersona(s);
		if (persona == p.bPersona && p.dwPersonaSince != 0)
			return;
		const BYTE before = p.bPersona;
		p.bPersona = persona;
		p.dwPersonaSince = dwNow;
		// A stone fight, a market trip or a party flips a bot's personality
		// many times an hour, and at a thousand bots a line for every one would
		// be ten thousand lines an hour: the changes that tell a bot's story
		// (the law met, the advance, the gear outgrown) have lines of their own
		// at level 0, and the census counts the rest every ten minutes.
		sys_log(1, "PLAYERBOT_PERSONA: pid=%u name=%s %s -> %s mood=%s level=%u map=%ld",
				ch->GetPlayerID(), ch->GetName(), GetPlayerBotPersonaName(before),
				GetPlayerBotPersonaName(persona), GetPlayerBotMoodName(p.mood.mood),
				(unsigned int)ch->GetLevel(), ch->GetMapIndex());
	}

	// Both of the above, every two seconds rather than every tick: the bag's
	// eighty percent alone is a walk over every cell of it.
	const DWORD PLAYERBOT_PERSONA_DECIDE_INTERVAL = 2000;

	// The goal dropper's look at its purse, further down beside the AFK.
	void ManagePlayerBotMedalGoal(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);
	// A rare personality's clock and ends (playerbot_rare_persona.h, later).
	void ManagePlayerBotRareState(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

	void ManagePlayerBotPersona(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return;
		TPlayerBotPersona& p = state.persona;
		if (p.dwNextDecide != 0 && dwNow < p.dwNextDecide)
			return;
		p.dwNextDecide = dwNow + PLAYERBOT_PERSONA_DECIDE_INTERVAL;
		ManagePlayerBotMedalGoal(ch, state, dwNow);
		ManagePlayerBotAdvancement(ch, state, dwNow);
		ManagePlayerBotRareState(ch, state, dwNow);
		DecidePlayerBotPersona(ch, state, dwNow);
	}

	// The drops the loot pass would walk to and take (playerbot_loot.h, which
	// comes later in the include order).
	size_t CountPlayerBotLootToTake(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

	// Iwakura's price of a family at a plus, on his curve, or zero.
	long long GetPlayerBotGearFamilyPrice(DWORD familyBase, int plus)
	{
		if (familyBase == 0 || plus < 0 || plus > 9)
			return 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == familyBase)
				return (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_GEAR_PRICES[i].adwPrice[plus]);
		return 0;
	}

	// The goal droppers: PLAYERBOT_MEDAL_GOAL_PERCENT of the medal droppers.
	bool IsPlayerBotMedalGoalDropper(DWORD pid)
	{
		return (int)(PlayerBotNavHash(pid ^ 0x474f414cU) % 100U) < PLAYERBOT_MEDAL_GOAL_PERCENT;
	}

	// Whether one worn piece meets its part of the goal: its slot filled at
	// the plus, by a piece no more than the law's window under the bot.
	bool PlayerBotWearsGoalPiece(LPCHARACTER ch, BYTE wearCell, int plus)
	{
		LPITEM worn = wearCell == WEAR_WEAPON ? GetPlayerBotHandWeapon(ch) : ch->GetWear(wearCell);
		return worn && (int)worn->GetRefineLevel() >= plus &&
				(int)GetPlayerBotPersonaLevelLimit(worn) + playerbot_persona::AWANS_LEVEL_WINDOW >= (int)ch->GetLevel();
	}

	// A goal dropper's look at its purse (community patch 2, point 4): the
	// medals it sold pay for its level's weapon and armour at +9 and helmet
	// and shield at +7, or it wears them - and then it is a medal dropper no
	// more. The Wanderer its draw would have been takes over, the exp lock
	// comes off with the personality (ManagePlayerBotExpLock), and the market
	// Perfectionist's rule spends the purse (IsPlayerBotMarketPerfectionist).
	void ManagePlayerBotMedalGoal(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (!ch || state.bPersonality != BOT_PERSONALITY_MEDAL_DROPPER || p.bMedalGoalDone ||
				!IsPlayerBotMedalGoalDropper(ch->GetPlayerID()) ||
				CPlayerBotManager::instance().IsMedalDropperCohortPID(ch->GetPlayerID()) ||
				(p.dwNextMedalGoalCheck != 0 && dwNow < p.dwNextMedalGoalCheck))
			return;
		p.dwNextMedalGoalCheck = dwNow + PLAYERBOT_MEDAL_GOAL_CHECK_MS;
		long long cost = 0;
		const bool wantsShield = PlayerBotWantsShield(ch);
		if (!PlayerBotWearsGoalPiece(ch, WEAR_WEAPON, PLAYERBOT_MEDAL_GOAL_MAIN_PLUS))
			cost += GetPlayerBotGearFamilyPrice(GetPlayerBotProgressionWeaponVnum(ch), PLAYERBOT_MEDAL_GOAL_MAIN_PLUS);
		if (!PlayerBotWearsGoalPiece(ch, WEAR_BODY, PLAYERBOT_MEDAL_GOAL_MAIN_PLUS))
			cost += GetPlayerBotGearFamilyPrice(GetPlayerBotProgressionArmorVnum(ch), PLAYERBOT_MEDAL_GOAL_MAIN_PLUS);
		if (!PlayerBotWearsGoalPiece(ch, WEAR_HEAD, PLAYERBOT_MEDAL_GOAL_SIDE_PLUS))
			cost += GetPlayerBotGearFamilyPrice(GetPlayerBotProgressionHelmetVnum(ch), PLAYERBOT_MEDAL_GOAL_SIDE_PLUS);
		if (wantsShield && !PlayerBotWearsGoalPiece(ch, WEAR_SHIELD, PLAYERBOT_MEDAL_GOAL_SIDE_PLUS))
			cost += GetPlayerBotGearFamilyPrice(GetPlayerBotProgressionShieldVnum(ch), PLAYERBOT_MEDAL_GOAL_SIDE_PLUS);
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch);
		if (cost > 0 && spare < cost)
			return;
		p.bMedalGoalDone = true;
		p.bDirty = true;
		state.bPersonality = BOT_PERSONALITY_WANDERER;
		sys_log(0, "PLAYERBOT_PERSONA: medal dropper met its goal pid=%u name=%s level=%u gold=%lld cost=%lld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(),
				(long long)ch->GetGold(), cost);
	}

	// SLABY's stop from the keyboard: every 10-30 minutes, for 2-5, somewhere
	// it will not simply die for it. It ends early when the bot is struck
	// (its health falls) or its mood lifts. Claims the tick while it lasts.
	bool ManagePlayerBotMoodAfk(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return false;
		TPlayerBotPersona& p = state.persona;
		const BYTE mood = GetPlayerBotPlayMood(ch, state, dwNow);
		if (p.dwAfkUntil != 0)
		{
			static std::map<DWORD, int> s_mapAfkHp;
			int& hpAtStart = s_mapAfkHp[ch->GetPlayerID()];
			if (hpAtStart <= 0)
				hpAtStart = ch->GetHP();
			const bool struck = ch->GetHP() < hpAtStart || ch->GetVictim() != NULL;
			if (dwNow >= p.dwAfkUntil || mood != playerbot_persona::MOOD_SLABY || struck || ch->IsDead())
			{
				sys_log(0, "PLAYERBOT_MOOD: back at the keyboard pid=%u name=%s reason=%s",
						ch->GetPlayerID(), ch->GetName(),
						struck ? "struck" : (mood != playerbot_persona::MOOD_SLABY ? "mood" : "time"));
				p.dwAfkUntil = 0;
				p.dwNextAfkAt = dwNow + (struck ? PLAYERBOT_MOOD_AFK_INTERRUPTED_RETRY
						: playerbot_persona::AfkInterval((uint32_t)number(0, 0x7fffffff)));
				s_mapAfkHp.erase(ch->GetPlayerID());
				return false;
			}
			if (ch->IsStateMove())
				ch->Stop();
			state.dwLastMeaningfulActivityTime = dwNow;
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}
		if (p.dwNextAfkAt == 0)
			p.dwNextAfkAt = dwNow + playerbot_persona::AfkInterval((uint32_t)number(0, 0x7fffffff));
		if (mood != playerbot_persona::MOOD_SLABY || dwNow < p.dwNextAfkAt)
			return false;
		// Only between two things, never in the middle of one.
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		const bool busy = (target && !target->IsDead()) || ch->GetVictim() != NULL ||
				state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bVisitingHerbalist || state.bMarketTrip || state.bFishingSession ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat || ch->GetMyShop() != NULL ||
				IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) ||
				(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * PLAYERBOT_MOOD_AFK_MIN_HP_PERCENT);
		if (busy)
		{
			p.dwNextAfkAt = dwNow + 60000;
			p.dwAfkLootWaitSince = 0;
			return false;
		}
		// What the pack left is picked up first (community patch 2, point 7):
		// the stop claims the tick above the loot pass, so a bot that went AFK
		// beside its drop left it to whoever came by. The loot pass's own
		// collector decides what counts - the drops it would walk to and take,
		// not the merchant fodder a choosy looter leaves on purpose.
		const size_t drops = CountPlayerBotLootToTake(ch, state, dwNow);
		if (drops > 0)
		{
			if (p.dwAfkLootWaitSince == 0)
			{
				p.dwAfkLootWaitSince = dwNow;
				sys_log(0, "PLAYERBOT_MOOD: picks up its drop before the stop pid=%u name=%s drops=%u",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)drops);
			}
			if (dwNow - p.dwAfkLootWaitSince < PLAYERBOT_MOOD_AFK_LOOT_WAIT_MAX_MS)
			{
				p.dwNextAfkAt = dwNow + PLAYERBOT_MOOD_AFK_LOOT_RETRY_MS;
				return false;
			}
		}
		p.dwAfkLootWaitSince = 0;
		p.dwAfkUntil = dwNow + playerbot_persona::AfkDuration((uint32_t)number(0, 0x7fffffff));
		ClearPlayerBotRoute(state, true);
		if (ch->IsStateMove())
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		sys_log(0, "PLAYERBOT_MOOD: away from the keyboard pid=%u name=%s minutes=%u map=%ld",
				ch->GetPlayerID(), ch->GetName(), (p.dwAfkUntil - dwNow) / 60000u, ch->GetMapIndex());
		return true;
	}

	// SLABY's other habit: a pause of 2-8 seconds when a pack is dead and
	// before the next one is looked for. Asked where the tick has found
	// nothing to fight, once for every fight.
	bool TakePlayerBotMoodPause(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return false;
		TPlayerBotPersona& p = state.persona;
		if (GetPlayerBotPlayMood(ch, state, dwNow) != playerbot_persona::MOOD_SLABY)
		{
			p.dwPauseUntil = 0;
			return false;
		}
		if (p.dwPauseUntil != 0 && dwNow < p.dwPauseUntil)
		{
			if (ch->IsStateMove())
				ch->Stop();
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}
		p.dwPauseUntil = 0;
		if (state.dwLastCombatActionTime == 0 ||
				dwNow - state.dwLastCombatActionTime > PLAYERBOT_MOOD_PAUSE_FIGHT_MS ||
				p.dwPausedAfterFight == state.dwLastCombatActionTime)
			return false;
		p.dwPausedAfterFight = state.dwLastCombatActionTime;
		p.dwPauseUntil = dwNow + playerbot_persona::PauseDuration((uint32_t)number(0, 0x7fffffff));
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// The census: how many bots of each personality and mood, every ten
	// minutes, counted over the pass that has just finished.
	unsigned int s_auPlayerBotPersonaCensus[playerbot_persona::PERSONA_COUNT];
	unsigned int s_auPlayerBotMoodCensus[playerbot_persona::MOOD_COUNT];
	unsigned int s_uPlayerBotPersonaLocked = 0;
	unsigned int s_uPlayerBotPersonaAfk = 0;
	// The Anti-PK protocol's share of it: bots fighting a player now, and bots
	// under a capitulation.
	unsigned int s_uPlayerBotPersonaPvp = 0;
	unsigned int s_uPlayerBotPersonaCapitulated = 0;
	DWORD s_dwPlayerBotPersonaCensusTime = 0;
	bool s_bPlayerBotPersonaCensusPass = false;

	void BeginPlayerBotPersonaCensus(DWORD dwNow)
	{
		s_bPlayerBotPersonaCensusPass = IsPlayerBotPersonaEnabled() &&
				(s_dwPlayerBotPersonaCensusTime == 0 ||
				 dwNow - s_dwPlayerBotPersonaCensusTime >= PLAYERBOT_PERSONA_CENSUS_INTERVAL);
		if (!s_bPlayerBotPersonaCensusPass)
			return;
		s_dwPlayerBotPersonaCensusTime = dwNow;
		for (int i = 0; i < playerbot_persona::PERSONA_COUNT; ++i)
			s_auPlayerBotPersonaCensus[i] = 0;
		for (int i = 0; i < playerbot_persona::MOOD_COUNT; ++i)
			s_auPlayerBotMoodCensus[i] = 0;
		s_uPlayerBotPersonaLocked = s_uPlayerBotPersonaAfk = 0;
		s_uPlayerBotPersonaPvp = s_uPlayerBotPersonaCapitulated = 0;
	}

	void NotePlayerBotPersonaCensus(const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!s_bPlayerBotPersonaCensusPass || !state.persona.bRestored)
			return;
		const TPlayerBotPersona& p = state.persona;
		if (p.bPersona < playerbot_persona::PERSONA_COUNT)
			++s_auPlayerBotPersonaCensus[p.bPersona];
		if (p.mood.mood < playerbot_persona::MOOD_COUNT)
			++s_auPlayerBotMoodCensus[p.mood.mood];
		if (!p.bAdvanced && p.bLockLevel != 0)
			++s_uPlayerBotPersonaLocked;
		if (p.dwAfkUntil != 0 && dwNow < p.dwAfkUntil)
			++s_uPlayerBotPersonaAfk;
		if (p.dwFoeVID != 0)
			++s_uPlayerBotPersonaPvp;
		if (p.dwCapitulatedUntil != 0 && dwNow < p.dwCapitulatedUntil)
			++s_uPlayerBotPersonaCapitulated;
	}

	void ReportPlayerBotPersonaCensus()
	{
		if (!s_bPlayerBotPersonaCensusPass)
			return;
		s_bPlayerBotPersonaCensusPass = false;
		const unsigned int* c = s_auPlayerBotPersonaCensus;
		sys_log(0, "PLAYERBOT_PERSONA: census grinder=%u zdobywca=%u handlarz=%u hazardzista=%u perfekcjonista=%u pogromca=%u gornik=%u rybak=%u najemnik=%u towarzysz=%u metinolog=%u nalogowiec=%u naukowiec=%u egzekutor=%u wedkarz=%u held=%u | mood slaby=%u normalny=%u bardzo_dobry=%u afk=%u | pvp=%u capitulated=%u",
				c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10], c[11], c[12], c[13], c[14],
				s_uPlayerBotPersonaLocked,
				s_auPlayerBotMoodCensus[0], s_auPlayerBotMoodCensus[1], s_auPlayerBotMoodCensus[2],
				s_uPlayerBotPersonaAfk, s_uPlayerBotPersonaPvp, s_uPlayerBotPersonaCapitulated);
	}
}

#endif
