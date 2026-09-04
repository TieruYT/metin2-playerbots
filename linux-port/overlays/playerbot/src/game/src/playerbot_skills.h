#ifndef __INC_METIN2_PLAYERBOT_SKILLS_H__
#define __INC_METIN2_PLAYERBOT_SKILLS_H__

// What a bot trains: where its stat points go, which skills its job levels and
// in what order, and keeping its own buffs up.
//
// This is the character sheet, not the fight. Nothing here sends a packet or
// touches a target - that is playerbot_combat.h, which reads the build decided
// here and never the other way round.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	bool AllocatePlayerBotStat(LPCHARACTER ch, BYTE statType)
	{
		if (!ch || ch->GetRealPoint(statType) >= 90 || ch->GetPoint(POINT_STAT) <= 0)
			return false;

		ch->SetRealPoint(statType, ch->GetRealPoint(statType) + 1);
		ch->SetPoint(statType, ch->GetPoint(statType) + 1);
		ch->ComputePoints();
		ch->PointChange(statType, 0);

		if (statType == POINT_IQ)
			ch->PointChange(POINT_MAX_HP, 0);
		else if (statType == POINT_HT)
			ch->PointChange(POINT_MAX_SP, 0);

		ch->PointChange(POINT_STAT, -1);
		ch->ComputePoints();
		return true;
	}

	void ManagePlayerBotStats(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetPoint(POINT_STAT) <= 0 || dwNow < state.dwNextStatCheckTime)
			return;

		state.dwNextStatCheckTime = dwNow + PLAYERBOT_STAT_CHECK_INTERVAL;

		while (ch->GetPoint(POINT_STAT) > 0)
		{
			const int ht = ch->GetRealPoint(POINT_HT);
			const int st = ch->GetRealPoint(POINT_ST);
			const int dx = ch->GetRealPoint(POINT_DX);
			const int iq = ch->GetRealPoint(POINT_IQ);

			BYTE targetStat = 0;

			switch (ch->GetJob())
			{
				case JOB_WARRIOR:
					// STR : VIT = 2 : 1 until 90, then DEX
					if (st < 90 && (st < ht * 2 || ht >= 90))
						targetStat = POINT_ST;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (iq < 90)
						targetStat = POINT_IQ;
					break;

				case JOB_ASSASSIN:
					// DEX : VIT = 2 : 1 until 90, then STR
					if (dx < 90 && (dx < ht * 2 || ht >= 90))
						targetStat = POINT_DX;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (st < 90)
						targetStat = POINT_ST;
					else if (iq < 90)
						targetStat = POINT_IQ;
					break;

				case JOB_SURA:
					// INT : VIT = 2 : 1 until 90, then DEX for skill dmg/reduction (or STR)
					if (iq < 90 && (iq < ht * 2 || ht >= 90))
						targetStat = POINT_IQ;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (st < 90)
						targetStat = POINT_ST;
					break;

				case JOB_SHAMAN:
					// INT : VIT = 2 : 1 until 90, then DEX
					if (iq < 90 && (iq < ht * 2 || ht >= 90))
						targetStat = POINT_IQ;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (st < 90)
						targetStat = POINT_ST;
					break;

				default:
					if (ht < 90)
						targetStat = POINT_HT;
					else if (st < 90)
						targetStat = POINT_ST;
					break;
			}

			if (targetStat == 0 || !AllocatePlayerBotStat(ch, targetStat))
				break;

			sys_log(0, "PLAYERBOT_AI: allocated stat pid=%u name=%s stat=%u new_val=%d points_left=%d",
					ch->GetPlayerID(), ch->GetName(), targetStat, ch->GetRealPoint(targetStat), ch->GetPoint(POINT_STAT));
		}
	}

	struct TJobSkillBuild
	{
		BYTE bSkillCount;
		DWORD dwSkills[6];
		DWORD dwBuffSkills[3];
		DWORD dwOffensiveSkills[4];
		DWORD dwPrimaryMaxSkill;
	};

	TJobSkillBuild GetPlayerBotSkillBuild(BYTE bJob, BYTE bGroup, DWORD playerID = 0)
	{
		TJobSkillBuild build;
		memset(&build, 0, sizeof(build));

		if (bGroup < 1 || bGroup > 2)
			return build;

		if (bJob == JOB_WARRIOR)
		{
			if (bGroup == 1) // Body
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 1; build.dwSkills[1] = 2; build.dwSkills[2] = 3; build.dwSkills[3] = 4; build.dwSkills[4] = 5;
				build.dwBuffSkills[0] = 4; // Aura of Sword
				build.dwBuffSkills[1] = 3; // Berserk
				build.dwOffensiveSkills[0] = 2; // Sword Spin
				build.dwOffensiveSkills[1] = 1; // Three-Way Cut
				build.dwOffensiveSkills[2] = 5; // Dash
				build.dwPrimaryMaxSkill = 4; // Max Aura first
			}
			else // Mental
			{
				build.bSkillCount = 5;
				// Mental warriors do not all follow one copied guide.  One cohort
				// develops Strong Body -> Bash, another prefers Spirit Strike, and a
				// third brings Stump forward for pack control.
				if (playerID % 3 == 0)
				{
					build.dwSkills[0] = 19; build.dwSkills[1] = 17; build.dwSkills[2] = 18; build.dwSkills[3] = 16; build.dwSkills[4] = 20;
				}
				else if (playerID % 3 == 1)
				{
					build.dwSkills[0] = 16; build.dwSkills[1] = 19; build.dwSkills[2] = 17; build.dwSkills[3] = 18; build.dwSkills[4] = 20;
				}
				else
				{
					build.dwSkills[0] = 19; build.dwSkills[1] = 18; build.dwSkills[2] = 17; build.dwSkills[3] = 16; build.dwSkills[4] = 20;
				}
				build.dwBuffSkills[0] = 19; // Strong Body
				build.dwOffensiveSkills[0] = playerID % 3 == 0 ? 17 : 16;
				build.dwOffensiveSkills[1] = playerID % 3 == 0 ? 16 : 17;
				build.dwOffensiveSkills[2] = 18; // Stump
				build.dwOffensiveSkills[3] = 20; // Sword Strike
				build.dwPrimaryMaxSkill = 19; // Max Strong Body first
			}
		}
		else if (bJob == JOB_ASSASSIN)
		{
			if (bGroup == 1) // Dagger
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 31; build.dwSkills[1] = 32; build.dwSkills[2] = 33; build.dwSkills[3] = 34; build.dwSkills[4] = 35;
				// Stealth is valuable in PvP, but wastes an action and contributes no
				// meaningful PvE damage.  Keep it learned, never auto-cast it on mobs.
				build.dwBuffSkills[0] = 0;
				build.dwOffensiveSkills[0] = 31; // Ambush
				build.dwOffensiveSkills[1] = 33; // Rolling Dagger
				build.dwOffensiveSkills[2] = 32; // Fast Attack
				build.dwOffensiveSkills[3] = 35; // Poison Cloud
				build.dwPrimaryMaxSkill = 31; // Ambush
			}
			else // Archer
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 46; build.dwSkills[1] = 47; build.dwSkills[2] = 48; build.dwSkills[3] = 49; build.dwSkills[4] = 50;
				build.dwBuffSkills[0] = 49; // Feather Walk
				build.dwOffensiveSkills[0] = 48; // Fire Arrow
				build.dwOffensiveSkills[1] = 50; // Poison Arrow
				build.dwOffensiveSkills[2] = 46; // Repetitive Shot
				build.dwOffensiveSkills[3] = 47; // Arrow Shower
				build.dwPrimaryMaxSkill = 48;
			}
		}
		else if (bJob == JOB_SURA)
		{
			if (bGroup == 1) // Weaponary (WP)
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 61; build.dwSkills[1] = 62; build.dwSkills[2] = 63; build.dwSkills[3] = 64; build.dwSkills[4] = 65; build.dwSkills[5] = 66;
				// 63 Enchanted Blade, 65 Enchanted Armor, 64 Fear - the numbers
				// from skill.h. What stood here was 63/64/66 under the labels
				// Blade/Armor/Fear, so Enchanted Armor was never buffed at all
				// and 66 was cast in its place every cycle. 66 is not a buff: it
				// is Dispel, which strips good affects from its target, and the
				// target here is the caster - so the one buff a weapon Sura is
				// built around kept taking its own Dispel off itself.
				build.dwBuffSkills[0] = 63; // Enchanted Blade
				build.dwBuffSkills[1] = 65; // Enchanted Armor
				build.dwBuffSkills[2] = 64; // Fear
				build.dwOffensiveSkills[0] = 62; // Dragon Swirl
				build.dwOffensiveSkills[1] = 61; // Finger Strike
				build.dwPrimaryMaxSkill = 63; // Enchanted Blade
			}
			else // Black Magic (BM)
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 76; build.dwSkills[1] = 77; build.dwSkills[2] = 78; build.dwSkills[3] = 79; build.dwSkills[4] = 80; build.dwSkills[5] = 81;
				build.dwBuffSkills[0] = 78; // Flame Spirit (Ognisty Duch / SKILL_MUYEONG)
				build.dwBuffSkills[1] = 79; // Dark Protection
				build.dwOffensiveSkills[0] = 77; // Flame Strike / Explosion
				build.dwOffensiveSkills[1] = 76; // Dark Strike
				build.dwOffensiveSkills[2] = 80; // Spirit Strike
				build.dwOffensiveSkills[3] = 81; // Dark Orb
				build.dwPrimaryMaxSkill = 78; // Keep Flame Spirit active and develop it first
			}
		}
		else if (bJob == JOB_SHAMAN)
		{
			if (bGroup == 1) // Dragon
			{
				build.bSkillCount = 6;
				const bool roarFirst = (playerID % 2) != 0;
				if (roarFirst)
				{
					build.dwSkills[0] = 93; build.dwSkills[1] = 96; build.dwSkills[2] = 92; build.dwSkills[3] = 91; build.dwSkills[4] = 94; build.dwSkills[5] = 95;
				}
				else
				{
					build.dwSkills[0] = 96; build.dwSkills[1] = 93; build.dwSkills[2] = 92; build.dwSkills[3] = 91; build.dwSkills[4] = 94; build.dwSkills[5] = 95;
				}
				build.dwBuffSkills[0] = 96; // Dragon's Aid (Crit)
				build.dwBuffSkills[1] = 94; // Blessing
				build.dwBuffSkills[2] = 95; // Reflect
				build.dwOffensiveSkills[0] = 92; // Shooting Dragon
				build.dwOffensiveSkills[1] = 93; // Dragon Roar
				build.dwOffensiveSkills[2] = 91; // Flying Talisman
				build.dwPrimaryMaxSkill = roarFirst ? 93 : 96;
			}
			else // Healer
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 106; build.dwSkills[1] = 107; build.dwSkills[2] = 108; build.dwSkills[3] = 109; build.dwSkills[4] = 110; build.dwSkills[5] = 111;
				build.dwBuffSkills[0] = 110; // Swiftness
				build.dwBuffSkills[1] = 111; // Attack Up
				build.dwBuffSkills[2] = 109; // Cure
				build.dwOffensiveSkills[0] = 106; // Lightning Throw
				build.dwOffensiveSkills[1] = 108; // Summon Lightning
				build.dwOffensiveSkills[2] = 107; // Lightning Claw
				build.dwPrimaryMaxSkill = 110; // Swiftness
			}
		}

		return build;
	}

	bool IsPlayerBotBuffActive(LPCHARACTER ch, DWORD buffVnum, DWORD dwNow, const TPlayerBotAIState& state)
	{
		if (!ch) return true;

		// 1. Affect flags (standard Metin2 toggle and buff flags)
		switch (buffVnum)
		{
			// skill.h names these after the affects they grant, so the pairing is
			// not a guess: SKILL_JEONGWI is 3 and AFF_JEONGWIHON carries the same
			// Korean name, SKILL_GEOMKYUNG is 4 and pairs with AFF_GEOMGYEONG.
			// They were crossed here, and the damage was the false positive
			// rather than the false negative: a warrior with its aura up made
			// this return true for Berserk as well, so Berserk was never cast at
			// all. (The reverse case fell through to FindAffect below and still
			// worked, which is why only half the problem was ever visible.)
			case 3:   if (ch->IsAffectFlag(AFF_JEONGWIHON)) return true; break; // Berserk
			case 4:   if (ch->IsAffectFlag(AFF_GEOMGYEONG)) return true; break; // Aura of Sword
			case 19:  if (ch->IsAffectFlag(AFF_CHEONGEUN))  return true; break; // Strong Body
			case 34:  if (ch->IsAffectFlag(AFF_EUNHYUNG))   return true; break; // Stealth
			case 49:  if (ch->IsAffectFlag(AFF_GYEONGGONG)) return true; break; // Feather Walk
			case 63:  if (ch->IsAffectFlag(AFF_GWIGUM))     return true; break; // Enchanted Blade toggle
			// Crossed the same way: SKILL_TERROR is 64, SKILL_JUMAGAP is 65.
			case 64:  if (ch->IsAffectFlag(AFF_TERROR))     return true; break; // Fear
			case 65:  if (ch->IsAffectFlag(AFF_JUMAGAP))    return true; break; // Enchanted Armor
			case 78:  if (ch->IsAffectFlag(AFF_MUYEONG))    return true; break; // Flame Spirit toggle
			case 79:  if (ch->IsAffectFlag(AFF_MANASHIELD)) return true; break; // Dark Protection toggle
			// Crossed as well; skill_proto has 94 setAffectFlag=HOSIN and 95=BOHO.
			// The shaman buffs all three of 96/94/95, so this pair suppressed
			// each other: whichever went up first made the other read as active
			// and it was never cast for the rest of the bot's life.
			case 94:  if (ch->IsAffectFlag(AFF_HOSIN))      return true; break; // Blessing
			case 95:  if (ch->IsAffectFlag(AFF_BOHO))       return true; break; // Reflect
			case 96:  if (ch->IsAffectFlag(AFF_GICHEON))    return true; break; // Dragon Aid
			case 110: if (ch->IsAffectFlag(AFF_KWAESOK))    return true; break; // Swiftness
			case 111: if (ch->IsAffectFlag(AFF_JEUNGRYEOK)) return true; break; // Attack Up
		}

		// 2. Generic FindAffect check
		if (ch->FindAffect(buffVnum) != NULL)
			return true;

		// 3. Fallback timestamp map
		std::map<DWORD, DWORD>::const_iterator it = state.mapBuffActiveUntil.find(buffVnum);
		if (it != state.mapBuffActiveUntil.end() && dwNow < it->second)
			return true;

		return false;
	}

	bool ChoosePlayerBotSkillGroup(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 5)
			return false;
		if (ch->GetSkillGroup() != 0)
			return true;

		const BYTE bGroup = (ch->GetPlayerID() % 2 == 0) ? 1 : 2;
		ch->SetSkillGroup(bGroup);
		ch->ClearSkill();
		sys_log(0, "PLAYERBOT_AI: chosen skill group at trainer pid=%u name=%s job=%u group=%u points=%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetJob(), bGroup, ch->GetPoint(POINT_SKILL));
		return ch->GetSkillGroup() == bGroup;
	}

	void ManagePlayerBotSkills(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetLevel() < 5 || dwNow < state.dwNextSkillCheckTime)
			return;

		state.dwNextSkillCheckTime = dwNow + PLAYERBOT_SKILL_CHECK_INTERVAL;

		// Profession is chosen only after the bot physically visits the matching
		// Chunjo trainer.  Until then the long-term planner owns this goal.
		if (ch->GetSkillGroup() == 0)
			return;

		const BYTE bGroup = ch->GetSkillGroup();
		if (bGroup == 0)
			return;

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), bGroup, ch->GetPlayerID());
		if (build.bSkillCount == 0)
			return;

		// Purge any skill levels that do not belong to this bot's chosen profession
		for (DWORD s = 1; s <= 120; ++s)
		{
			if (ch->GetSkillLevel(s) > 0)
			{
				bool bValid = false;
				for (BYTE k = 0; k < build.bSkillCount; ++k)
				{
					if (build.dwSkills[k] == s)
					{
						bValid = true;
						break;
					}
				}
				if (!bValid)
				{
					ch->SetSkillLevel(s, 0);
				}
			}
		}

		if (ch->GetPoint(POINT_SKILL) <= 0)
			return;

		// First pass: ensure each group skill has at least 1 point to unlock it
		for (BYTE i = 0; i < build.bSkillCount && ch->GetPoint(POINT_SKILL) > 0; ++i)
		{
			const DWORD dwSkillVnum = build.dwSkills[i];
			if (dwSkillVnum != 0 && ch->GetSkillLevel(dwSkillVnum) == 0)
			{
				ch->SkillLevelUp(dwSkillVnum);
				sys_log(0, "PLAYERBOT_AI: unlocked skill pid=%u name=%s vnum=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), dwSkillVnum, ch->GetPoint(POINT_SKILL));
			}
		}

		// Second pass: level primary max skill up to Master (17-20)
		if (build.dwPrimaryMaxSkill != 0 && ch->GetPoint(POINT_SKILL) > 0)
		{
			while (ch->GetPoint(POINT_SKILL) > 0 &&
					ch->GetSkillMasterType(build.dwPrimaryMaxSkill) == SKILL_NORMAL &&
					ch->GetSkillLevel(build.dwPrimaryMaxSkill) < 20)
			{
				const BYTE bOldLevel = ch->GetSkillLevel(build.dwPrimaryMaxSkill);
				ch->SkillLevelUp(build.dwPrimaryMaxSkill);
				if (ch->GetSkillLevel(build.dwPrimaryMaxSkill) == bOldLevel)
					break;

				sys_log(0, "PLAYERBOT_AI: leveled primary skill pid=%u name=%s vnum=%u level=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), build.dwPrimaryMaxSkill, ch->GetSkillLevel(build.dwPrimaryMaxSkill), ch->GetPoint(POINT_SKILL));
			}
		}

		// Third pass: distribute remaining points into secondary skills
		for (BYTE i = 0; i < build.bSkillCount && ch->GetPoint(POINT_SKILL) > 0; ++i)
		{
			const DWORD dwSkillVnum = build.dwSkills[i];
			if (dwSkillVnum == 0 || dwSkillVnum == build.dwPrimaryMaxSkill)
				continue;

			while (ch->GetPoint(POINT_SKILL) > 0 &&
					ch->GetSkillMasterType(dwSkillVnum) == SKILL_NORMAL &&
					ch->GetSkillLevel(dwSkillVnum) < 20)
			{
				const BYTE bOldLevel = ch->GetSkillLevel(dwSkillVnum);
				ch->SkillLevelUp(dwSkillVnum);
				if (ch->GetSkillLevel(dwSkillVnum) == bOldLevel)
					break;

				sys_log(0, "PLAYERBOT_AI: leveled secondary skill pid=%u name=%s vnum=%u level=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), dwSkillVnum, ch->GetSkillLevel(dwSkillVnum), ch->GetPoint(POINT_SKILL));
			}
		}
	}
}

#endif
