#ifndef __INC_METIN2_PLAYERBOT_GEAR_H__
#define __INC_METIN2_PLAYERBOT_GEAR_H__

// What a bot wears and what it carries: equipment scoring, the progression
// ladder of weapons and armour, arrows, and the potion supply.
//
// Same kind of file as playerbot_types.h -- an implementation fragment, not a
// normal header. It defines objects, it relies on the engine headers
// playerbot_manager.cpp includes above it, and its anonymous namespace is the
// same one the manager reopens. Include it exactly once, from
// playerbot_manager.cpp, after playerbot_navigation.h.
//
// The order of these fragments is the dependency order: everything here is
// written against the world (navigation) and the bot's own state (types), and
// against nothing that comes after it. The few things it needs from later
// subsystems are forward-declared below rather than pulled in.

namespace
{
	// Defined with the town code. Buying anything means standing at an NPC
	// first, and where exactly is a town concern, not a gear one.
	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	bool IsPlayerBotWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;

		if (ch)
		{
			const BYTE subType = item->GetSubType();
			switch (ch->GetJob())
			{
				case JOB_ASSASSIN:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_BOW;
					// Before selecting a profession and on Dagger training, never equip
					// a bow: melee Ninja skills ask CalcMeleeDamage and reject bows.
					return subType == WEAPON_DAGGER || subType == WEAPON_SWORD;
				case JOB_WARRIOR:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_TWO_HANDED || subType == WEAPON_SWORD;
					return subType == WEAPON_SWORD;
				case JOB_SURA:
					return subType == WEAPON_SWORD;
				case JOB_SHAMAN:
					return subType == WEAPON_BELL || subType == WEAPON_FAN;
			}
		}

		switch (item->GetSubType())
		{
			case WEAPON_SWORD:
			case WEAPON_DAGGER:
			case WEAPON_TWO_HANDED:
			case WEAPON_BELL:
			case WEAPON_FAN:
			case WEAPON_MOUNT_SPEAR:
				return true;
			case WEAPON_BOW:
				return true;
		}

		return false;
	}

	bool IsPlayerBotEquipmentCandidate(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsExchanging() || !item->IsEquipable())
			return false;

		// IsEquipable only describes the item type.  The class restrictions live
		// in the anti flags and were previously checked only for sex, so a Warrior
		// could keep (and repeatedly try to equip/refine) entire inventories of
		// Ninja, Sura and Shaman armour.  CanUsedBy is the engine's canonical job
		// anti-flag check and deliberately does not depend on combat state.
		if (!item->CanUsedBy(ch))
			return false;

		if (item->GetType() == ITEM_WEAPON && !IsPlayerBotWeapon(ch, item))
			return false;

		switch (item->GetType())
		{
			case ITEM_WEAPON:
			case ITEM_ARMOR:
			case ITEM_UNIQUE:
			case ITEM_RING:
			case ITEM_BELT:
				break;
			default:
				return false;
		}

		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_MALE) && GET_SEX(ch) == SEX_MALE)
			return false;
		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_FEMALE) && GET_SEX(ch) == SEX_FEMALE)
			return false;

		return true;
	}

	// The stat this character fights with. A warrior swings with strength and a
	// shaman casts with intelligence, so the same earring is a good piece for one
	// and jewellery for the other. Sura splits: the weaponry build hits with
	// strength, the black-magic one with intelligence, and the skill group is
	// what says which - the same question the weapon ladder already asks it.
	BYTE GetPlayerBotPrimaryStatApply(LPCHARACTER ch)
	{
		if (!ch)
			return APPLY_NONE;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  return APPLY_STR;
			case JOB_ASSASSIN: return APPLY_DEX;
			case JOB_SURA:     return ch->GetSkillGroup() == 2 ? APPLY_INT : APPLY_STR;
			case JOB_SHAMAN:   return APPLY_INT;
			default:           return APPLY_NONE;
		}
	}

	// Which of the two ways of doing damage a school lives by. Warrior Body,
	// Sura Weaponry and Ninja Dagger earn with ordinary blows; Warrior Mental,
	// Sura Black Magic, Ninja Archer and both Shaman schools with skills. +1 for
	// a skill school, -1 for a blow school, 0 before the profession is chosen -
	// and 0 is a real answer, not a default: it means weight both the same.
	int GetPlayerBotSchoolStyle(LPCHARACTER ch)
	{
		if (!ch || ch->GetSkillGroup() == 0)
			return 0;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_SURA:     return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_ASSASSIN: return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_SHAMAN:   return 1;
			default:           return 0;
		}
	}

	// And of the skill schools, which ones roll the weapon's *magic* values.
	// SetPolyVarForAttack (char_skill.cpp) hands every skill both numbers - wep
	// from VALUE3/4 and mwep from VALUE1/2 - and the skill's own formula picks.
	// Black Magic and both Shaman schools pick mwep; Mental and Archer are skill
	// schools that still hit with wep.
	bool IsPlayerBotMagicSchool(LPCHARACTER ch)
	{
		if (!ch || ch->GetSkillGroup() == 0)
			return false;
		if (ch->GetJob() == JOB_SHAMAN)
			return true;
		return ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2;
	}

	long long ScorePlayerBotApply(BYTE bType, long lValue, LPCHARACTER ch = NULL)
	{
		switch (bType)
		{
			case APPLY_NONE:
			case APPLY_SKILL:
				return 0;
			case APPLY_MAX_HP:
				return (long long)lValue * 10;
			case APPLY_MAX_SP:
			// The sprint bar, and the reason a bot would otherwise wear the
			// level-0 bracelet for ever: ten points of it through the catch-all
			// of fifty scored five hundred, more than anything the line offers
			// below level 46. It is not a combat stat for a player either.
			case APPLY_MAX_STAMINA:
				return (long long)lValue * 3;
			case APPLY_CON:
			case APPLY_STR:
			case APPLY_DEX:
			case APPLY_INT:
			{
				// 250 for all four was the class-blind figure, and it is still
				// what an unknown character gets. Knowing the class, the stat it
				// fights with is worth twice that and the two it does not use
				// half. Constitution stays in between for everybody: nobody
				// builds around it and nobody is sorry to have it.
				const BYTE primary = GetPlayerBotPrimaryStatApply(ch);
				long long weight = 250;
				if (primary != APPLY_NONE)
					weight = (bType == primary) ? 500
							: (bType == APPLY_CON ? 300 : 120);
				return (long long)lValue * weight;
			}
			case APPLY_ATT_SPEED:
				return (long long)lValue * 200;
			case APPLY_MOV_SPEED:
				return (long long)lValue * 100;
			case APPLY_HP_REGEN:
			case APPLY_POTION_BONUS:
				return (long long)lValue * 100;
			case APPLY_POISON_PCT:
			case APPLY_STUN_PCT:
			case APPLY_SLOW_PCT:
				return (long long)lValue * 200;
			case APPLY_CRITICAL_PCT:
			case APPLY_PENETRATE_PCT:
			case APPLY_BLOCK:
			case APPLY_DODGE:
				return (long long)lValue * 400;
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:
			case APPLY_ATTBONUS_MONSTER:
				return (long long)lValue * 300;
			case APPLY_STEAL_HP:
			case APPLY_KILL_HP_RECOVER:
				return (long long)lValue * 250;
			case APPLY_ATT_GRADE_BONUS:
			case APPLY_DEF_GRADE_BONUS:
			case APPLY_DEF_GRADE:
				return (long long)lValue * 300;
			// Average damage is for ordinary blows and skill damage is for skills,
			// and which of the two a bot wants depends on how its school earns.
			// These used to be 500 and 50 - the skill bonus fell into the default
			// bucket - so a Black Magic sura or a shaman rated a 10% skill roll
			// below a single point of strength. Nothing is worth nothing to
			// anybody: a Body warrior still casts, a shaman still swings. Undecided
			// weights both equally.
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
			{
				const int style = GetPlayerBotSchoolStyle(ch);
				return (long long)lValue * (style < 0 ? 500 : style > 0 ? 250 : 400);
			}
			case APPLY_SKILL_DAMAGE_BONUS:
			{
				const int style = GetPlayerBotSchoolStyle(ch);
				return (long long)lValue * (style > 0 ? 500 : style < 0 ? 250 : 400);
			}
			// Defending against ordinary blows is worth the same to everybody:
			// every monster in this world swings.
			case APPLY_NORMAL_HIT_DEFEND_BONUS:
				return (long long)lValue * 500;
			// A flat magic attack bonus is the magic schools' strength line.
			case APPLY_MAGIC_ATT_GRADE:
				return (long long)lValue * (IsPlayerBotMagicSchool(ch) ? 300 : 60);
			case APPLY_IMMUNE_STUN:
			case APPLY_IMMUNE_SLOW:
			case APPLY_IMMUNE_FALL:
				return (long long)lValue * 1000;
			default:
				return (long long)lValue * 50;
		}
	}

	bool IsPlayerBotSpecialLevel30WeaponVnum(DWORD vnum)
	{
		return (vnum >= 290 && vnum <= 299) || (vnum >= 1170 && vnum <= 1179) ||
				(vnum >= 2150 && vnum <= 2159) || (vnum >= 3210 && vnum <= 3219) ||
				(vnum >= 5110 && vnum <= 5119) || (vnum >= 7160 && vnum <= 7169);
	}

	long long GetPlayerBotEquipmentScore(LPITEM item, LPCHARACTER ch = NULL)
	{
		if (!item || !item->GetProto())
			return 0;

		long long score = 1;
		if (item->GetType() == ITEM_WEAPON)
		{
			// What the engine's own damage code reads: value 3 and 4 are the
			// damage roll and value 5 is an attack bonus counted twice
			// (battle.cpp). Refining raises these, so a refined weapon is already
			// worth more here without anything being said about the refine.
			const long long physical = (long long)(item->GetValue(3) + item->GetValue(4) +
					2 * item->GetValue(5));

			// Value 1 and 2 are the other roll: what SetPolyVarForAttack hands a
			// skill as mwep, with the same value-5 bonus on top. Every bell and
			// nearly every fan carries it, and so do 261 of the 311 swords - the
			// sura ones. Scoring physical damage alone rated a Black Magic sura's
			// sword by the half of it that sura never uses, and a shaman's bell by
			// the half it uses only when it stops casting. The schools that cast
			// take the magic roll with half the physical behind it; everybody else
			// takes the physical with a quarter of the magic, because a Weaponry
			// sura does still cast now and then.
			const long long magical = (long long)(item->GetValue(1) + item->GetValue(2) +
					2 * item->GetValue(5));
			long long attack = IsPlayerBotMagicSchool(ch)
					? magical + physical / 2
					: physical + magical / 4;

			// And then how often it lands, because a swing is not a swing.
			// GET_ATTACK_SPEED halves the interval for a dagger, and a bow's roll
			// is doubled before anything else touches it - so on both, the same
			// numbers are worth twice what the tooltip suggests. A level-30
			// dagger showing 40-44 outdamages a level-30 sword showing 57-73.
			//
			// The per-race animation speeds would sharpen this further, but they
			// are in the client's animation data, not in anything the server
			// reads, so only the two rules the engine states outright are used.
			const BYTE weaponSubType = item->GetSubType();
			if (weaponSubType == WEAPON_DAGGER || weaponSubType == WEAPON_BOW)
				attack *= 2;
			score += attack * 1000;

			// A level-30 average-damage weapon used to be handed a flat 350000
			// here. Damage is scored at a thousand a point, so that was more than
			// any weapon in the game is worth and no bot ever replaced one: an
			// FMS at +4 scored 524000 against 208000 for a level-36 sword at +7,
			// which is the better weapon by the numbers above. These weapons are
			// still protected from a merchant - IsPlayerBotJunkItem says a
			// level-30 weapon is never junk - but protecting them is not the same
			// as pretending nothing can beat them.

			if (ch)
			{
				if (ch->GetJob() == JOB_ASSASSIN)
				{
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_BOW)
						score += 500000; // Prefer bows for Archer Ninja
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_DAGGER)
						score += 300000; // Prefer daggers for Dagger Ninja
				}
				else if (ch->GetJob() == JOB_WARRIOR)
				{
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_TWO_HANDED)
						score += 200000; // Prefer two-handed for Mental Warrior
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_SWORD)
						score += 200000; // Prefer sword for Body Warrior
				}
			}
		}
		else if (item->GetType() == ITEM_ARMOR &&
				(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_HEAD ||
				 item->GetSubType() == ARMOR_FOOTS || item->GetSubType() == ARMOR_SHIELD))
		{
			score += (long long)(item->GetValue(1) + 2 * item->GetValue(5)) * 1000;
		}

		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			score += ScorePlayerBotApply(item->GetProto()->aApplies[i].bType,
					item->GetProto()->aApplies[i].lValue, ch);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			score += ScorePlayerBotApply(item->GetAttributeType(i),
					item->GetAttributeValue(i), ch);

		if (item->GetImmuneFlag() != 0)
			score += 1000;

		// A race-attack bonus is only worth carrying where that race is what you
		// actually fight. The population learns which monsters live on each map,
		// so "strong against orcs" counts for far more in Orc Valley than in a
		// place where nothing orcish ever spawns.
		if (ch)
		{
			const int dominant = GetPlayerBotDominantRace(ch->GetMapIndex());
			if (dominant != PLAYERBOT_RACE_NONE)
			{
				const BYTE wanted = GetPlayerBotRaceApplyType(dominant);
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					if (item->GetAttributeType(i) == wanted)
						score += (long long)item->GetAttributeValue(i) * 600;
				}
				for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
				{
					if (item->GetProto()->aApplies[i].bType == wanted)
						score += (long long)item->GetProto()->aApplies[i].lValue * 600;
				}
			}
		}

		return score;
	}

	bool SharePlayerBotOldGearNearby(LPCHARACTER ch, LPITEM oldItem)
	{
		if (!ch || !oldItem || oldItem->IsEquipped() || oldItem->isLocked() ||
				oldItem->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
				!ch->GetSectree())
			return false;

		const int wearCell = oldItem->FindEquipCell(ch);
		if (wearCell < 0)
			return false;

		struct FGearSharer
		{
			LPCHARACTER m_giver;
			LPITEM m_item;
			int m_wearCell;
			LPCHARACTER m_receiver;
			long long m_bestImprovement;
			int m_bestDistance;

			FGearSharer(LPCHARACTER giver, LPITEM item, int wearCell) :
				m_giver(giver), m_item(item), m_wearCell(wearCell), m_receiver(NULL),
				m_bestImprovement(0), m_bestDistance(INT_MAX) {}

			bool operator()(LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;

				LPCHARACTER member = static_cast<LPCHARACTER>(entity);
				if (!member || member == m_giver || member->IsDead() || !member->IsPC() ||
						!member->GetDesc() || !member->GetDesc()->IsBot() ||
						member->GetJob() != m_giver->GetJob() ||
						member->GetSkillGroup() != m_giver->GetSkillGroup() ||
						member->GetLevel() >= m_giver->GetLevel() ||
						m_item->GetLevelLimit() > member->GetLevel())
					return true;

				const int distance = DISTANCE_APPROX(
						m_giver->GetX() - member->GetX(), m_giver->GetY() - member->GetY());
				if (distance > PLAYERBOT_GEAR_SHARE_RANGE ||
						member->GetEmptyInventory(m_item->GetSize()) < 0)
					return true;

				if (!IsPlayerBotEquipmentCandidate(member, m_item))
					return true;

				LPITEM memberOldItem = member->GetWear(m_wearCell);
				const long long newItemScore = GetPlayerBotEquipmentScore(m_item, member);
				long long memberScore = memberOldItem ? GetPlayerBotEquipmentScore(memberOldItem, member) : 0;
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM candidate = member->GetInventoryItem(cell);
					if (!candidate || !IsPlayerBotEquipmentCandidate(member, candidate) ||
							candidate->GetLevelLimit() > member->GetLevel() ||
							candidate->FindEquipCell(member) != m_wearCell)
						continue;
					memberScore = std::max(memberScore,
							GetPlayerBotEquipmentScore(candidate, member));
				}

				const long long improvement = newItemScore - memberScore;
				if (improvement > 0 &&
						(!m_receiver || improvement > m_bestImprovement ||
						 (improvement == m_bestImprovement && distance < m_bestDistance)))
				{
					m_receiver = member;
					m_bestImprovement = improvement;
					m_bestDistance = distance;
				}
				return true;
			}
		};

		FGearSharer sharer(ch, oldItem, wearCell);
		ch->GetSectree()->ForEachAround(sharer);
		if (!sharer.m_receiver)
			return false;

		const int receiverCell = sharer.m_receiver->GetEmptyInventory(oldItem->GetSize());
		if (receiverCell < 0)
			return false;

		const WORD oldCell = oldItem->GetCell();
		const DWORD vnum = oldItem->GetVnum();
		const BYTE refine = oldItem->GetRefineLevel();
		oldItem->RemoveFromCharacter();
		if (oldItem->AddToCharacter(sharer.m_receiver, TItemPos(INVENTORY, receiverCell)))
		{
			sys_log(0, "PLAYERBOT_AI: gifted reserve gear pid=%u name=%s -> target_pid=%u target_name=%s vnum=%u refine=%u improvement=%lld",
					ch->GetPlayerID(), ch->GetName(), sharer.m_receiver->GetPlayerID(),
					sharer.m_receiver->GetName(), vnum, refine, sharer.m_bestImprovement);
			return true;
		}

		oldItem->AddToCharacter(ch, TItemPos(INVENTORY, oldCell));
		return false;
	}

	bool ManagePlayerBotEquipment(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		if (dwNow < state.dwNextEquipmentCheckTime && !state.bEquipPending)
			return false;

		LPITEM bestItem = NULL;
		LPITEM bestOldItem = NULL;
		int bestWearCell = -1;
		long long bestImprovement = 0;
		long long bestScore = 0;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotEquipmentCandidate(ch, item))
				continue;

			const int wearCell = item->FindEquipCell(ch);
			if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
				continue;

			LPITEM oldItem = ch->GetWear(wearCell);
			if (oldItem && IS_SET(oldItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;

			if (!ch->CanEquipNow(item, TItemPos(INVENTORY, cell)))
				continue;

			const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
			const long long oldScore = oldItem ? GetPlayerBotEquipmentScore(oldItem, ch) : 0;
			if (oldItem && itemScore <= oldScore)
				continue;

			const long long improvement = oldItem ? itemScore - oldScore : 1000000000000LL + itemScore;
			if (!bestItem || improvement > bestImprovement)
			{
				bestItem = item;
				bestOldItem = oldItem;
				bestWearCell = wearCell;
				bestImprovement = improvement;
				bestScore = itemScore;
			}
		}

		if (!bestItem)
		{
			state.bEquipPending = false;
			state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;
			return false;
		}

		// Equipping is forbidden for 1.5 seconds after an attack or skill, or right after spawn.
		// Hold bEquipPending and do not disrupt active combat.
		if (dwNow - ch->GetLastAttackTime() <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY ||
			dwNow - state.dwLastBotSkillTime <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY ||
			(state.dwSpawnTime != 0 && dwNow - state.dwSpawnTime <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY))
		{
			state.bEquipPending = true;
			return false;
		}

		state.bEquipPending = false;
		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;

		const DWORD newVnum = bestItem->GetVnum();
		const DWORD oldVnum = bestOldItem ? bestOldItem->GetVnum() : 0;
		const long long oldScore = bestOldItem ? GetPlayerBotEquipmentScore(bestOldItem, ch) : 0;
		// EquipItem's optional integer is a candidate slot for rings/uniques, not a
		// wear slot.  Passing WEAR_WEAPON/WEAR_BODY here made swaps of differently
		// sized items fail.  Put the old item into a genuinely free inventory area,
		// then let FindEquipCell choose the normal destination.
		if (bestOldItem)
		{
			if (ch->GetEmptyInventory(bestOldItem->GetSize()) < 0 ||
					!ch->UnequipItem(bestOldItem))
			{
				state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
				return false;
			}
		}

		if (ch->EquipItem(bestItem))
		{
			sys_log(0, "PLAYERBOT_AI: equipped upgrade pid=%u name=%s wear=%d old_vnum=%u new_vnum=%u old_score=%lld new_score=%lld",
					ch->GetPlayerID(), ch->GetName(), bestWearCell, oldVnum, newVnum, oldScore, bestScore);

			if (bestOldItem)
				SharePlayerBotOldGearNearby(ch, bestOldItem);

			return true;
		}

		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
		sys_err("PLAYERBOT_AI: failed to equip upgrade pid=%u name=%s wear=%d vnum=%u",
				ch->GetPlayerID(), ch->GetName(), bestWearCell, newVnum);
		return false;
	}

	void RestorePlayerBotEquipmentAfterRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		// A blacksmith session can temporarily remove more than one worn item.
		// ManagePlayerBotEquipment intentionally equips only one upgrade per call,
		// so force a short bounded pass before the bot leaves the NPC.  This makes
		// the visible sequence match a real player: remove, refine through the
		// desired + level, then put the best surviving result back on.
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			if (!ManagePlayerBotEquipment(ch, state, dwNow))
				break;
		}
	}

	DWORD GetStarterChestVnum(BYTE bJob)
	{
		switch (bJob)
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 50187;
			case JOB_ASSASSIN:
				return 50212;
			case JOB_SHAMAN:
				return 50213;
		}

		return 0;
	}

	DWORD GetPlayerBotEmergencyWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 10;

		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 10;   // Sword +0
			case JOB_ASSASSIN:
				if (ch->GetSkillGroup() == 2)
					return 2000; // Bow +0
				return 1000; // Dagger +0
			case JOB_SHAMAN:
				return 7000; // Fan +0
		}

		return 10;
	}

	long long GetPlayerBotEmergencyWeaponPrice(LPCHARACTER ch)
	{
		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		return vnum == 7000 ? 600 : 100;
	}

	int GetPlayerBotProtoLevelLimit(const TItemTable* proto)
	{
		if (!proto)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return proto->aLimits[i].lValue;
		return 0;
	}

	DWORD GetPlayerBotProgressionWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD familyBase = 10;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
				familyBase = ch->GetSkillGroup() == 2 ? 3000 : 10;
				break;
			case JOB_ASSASSIN:
				familyBase = ch->GetSkillGroup() == 2 ? 2000 : 1000;
				break;
			case JOB_SURA:
				familyBase = 10;
				break;
			case JOB_SHAMAN:
				familyBase = 7000;
				break;
		}

		DWORD bestVnum = familyBase;
		int bestLevel = -1;
		// Twenty tiers, not eight. Eight stopped at familyBase+70, which is the
		// level-36 weapon in four of the five families - so every bot past 36
		// wanted nothing better than what it was already holding and stopped
		// upgrading for good. One was reported still swinging a +0 at level 49.
		//
		// Twenty is safe as well as sufficient: checked against item_proto, all
		// five families keep the same weapon subtype for twenty tiers, and only a
		// strictly higher requirement wins - so a family that runs out early stops
		// contributing, and the special level-30 weapons sitting at the far end of
		// three of these ranges can never displace a higher-level piece.
		for (int tier = 0; tier < 20; ++tier)
		{
			const DWORD candidateVnum = familyBase + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionArmorVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD baseVnum = 11200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 11400; break;
			case JOB_SURA:     baseVnum = 11600; break;
			case JOB_SHAMAN:   baseVnum = 11800; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		// Ten tiers, not eight. Each class's body armour runs base+0 to base+90 -
		// levels 0, 9, 18, 26, 34, 42, 48, 54, 61, 66 - and eight of them stopped
		// at 54, so the last two pieces were unreachable however high a bot got.
		// Ten is also the ceiling: base+100 begins a different series with its own
		// numbering, and the classes are 200 apart, so this cannot reach one.
		for (int tier = 0; tier < 10; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionShieldVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 13000;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = 13000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionHelmetVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD baseVnum = 12200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 12340; break;
			case JOB_SURA:     baseVnum = 12480; break;
			case JOB_SHAMAN:   baseVnum = 12620; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		// Seven, and seven is the whole family: helmet ranges are 140 vnums apart
		// and the stride is 20, so the eighth tier was the next class's first
		// helmet. Nothing came of that - those entries are level-0 items and a
		// level-0 item cannot outrank a real one - but a ladder has no business
		// reading another class's gear to decide what this one should wear.
		// Nothing is lost either: every class reaches its best helmet by the
		// fourth tier (level 41 for a warrior, level 80 for the other three).
		for (int tier = 0; tier < 7; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionBootsVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 15000;
		int bestLevel = -1;
		for (int tier = 0; tier < 12; ++tier)
		{
			const DWORD candidateVnum = 15000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	bool HasPlayerBotProgressionGear(LPCHARACTER ch, DWORD desiredVnum, int wearCell)
	{
		if (!ch || desiredVnum == 0)
			return false;
		TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		if (!desiredProto)
			return false;

		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : INVENTORY_MAX_NUM;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(wearCell) : ch->GetInventoryItem(index);
				if (!item || !IsPlayerBotEquipmentCandidate(ch, item) ||
						item->FindEquipCell(ch) != wearCell)
					continue;
				if (wearCell == WEAR_WEAPON && !IsPlayerBotWeapon(ch, item))
					continue;
				if (item->GetLevelLimit() >= desiredLevel && item->GetLevelLimit() <= ch->GetLevel())
					return true;
			}
		}
		return false;
	}

	// What a proto is worth to this character, before anything has been rolled
	// on it. The ladders below compare candidates they cannot hold yet.
	long long ScorePlayerBotProtoApplies(const TItemTable* proto, LPCHARACTER ch)
	{
		if (!proto)
			return 0;
		long long score = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			score += ScorePlayerBotApply(proto->aApplies[i].bType,
					proto->aApplies[i].lValue, ch);
		return score;
	}

	// Bracelets, necklaces and earrings. Not a ladder in the sense the other
	// slots are: the earring line rotates dexterity, strength, constitution and
	// intelligence as it climbs, so the newest tier a bot qualifies for is the
	// right one only for the class that tier favours. Worth decides, and the
	// required level only breaks a tie - which for the bracelets and necklaces,
	// whose lines climb straight, comes to the same answer as before.
	DWORD GetPlayerBotProgressionAccessoryVnum(LPCHARACTER ch, DWORD baseVnum,
			DWORD stride, int tiers)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 0;
		long long bestScore = -1;
		int bestLevel = -1;
		for (int tier = 0; tier < tiers; ++tier)
		{
			const DWORD candidateVnum = baseVnum + (DWORD)tier * stride;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			if (reqLevel > (int)ch->GetLevel())
				continue;
			const long long score = ScorePlayerBotProtoApplies(proto, ch);
			if (score > bestScore || (score == bestScore && reqLevel > bestLevel))
			{
				bestVnum = candidateVnum;
				bestScore = score;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	// Twelve tiers each, which is the whole of every one of the three families
	// below level 80: the next entries after them sit at 85 and above, past
	// anything this world levels to.
	DWORD GetPlayerBotProgressionWristVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 14000, 20, 12);
	}

	DWORD GetPlayerBotProgressionNecklaceVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 16000, 20, 12);
	}

	DWORD GetPlayerBotProgressionEarringVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 17000, 20, 12);
	}

	// The other slots ask "is what I am wearing for a lower level than what I
	// could buy". That question is wrong here, because a better earring can be
	// an older one. This asks what the slot is actually worth instead.
	bool NeedsPlayerBotProgressionAccessory(LPCHARACTER ch, BYTE wearCell,
			DWORD desiredVnum)
	{
		if (!ch || desiredVnum == 0)
			return false;
		const TItemTable* desired = ITEM_MANAGER::instance().GetTable(desiredVnum);
		if (!desired)
			return false;
		const long long wanted = ScorePlayerBotProtoApplies(desired, ch);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : INVENTORY_MAX_NUM;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(wearCell)
						: ch->GetInventoryItem(index);
				if (!item || !IsPlayerBotEquipmentCandidate(ch, item) ||
						item->FindEquipCell(ch) != wearCell)
					continue;
				if (item->GetLevelLimit() > ch->GetLevel())
					continue;
				if (ScorePlayerBotProtoApplies(item->GetProto(), ch) >= wanted)
					return false;
			}
		}
		return true;
	}

	bool NeedsPlayerBotProgressionWrist(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_WRIST,
				GetPlayerBotProgressionWristVnum(ch));
	}

	bool NeedsPlayerBotProgressionNecklace(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_NECK,
				GetPlayerBotProgressionNecklaceVnum(ch));
	}

	bool NeedsPlayerBotProgressionEarring(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_EAR,
				GetPlayerBotProgressionEarringVnum(ch));
	}

	bool NeedsPlayerBotProgressionWeapon(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionWeaponVnum(ch), WEAR_WEAPON);
	}

	bool NeedsPlayerBotProgressionArmor(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionArmorVnum(ch), WEAR_BODY);
	}

	bool NeedsPlayerBotProgressionShield(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionShieldVnum(ch), WEAR_SHIELD);
	}

	bool NeedsPlayerBotProgressionHelmet(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionHelmetVnum(ch), WEAR_HEAD);
	}

	bool NeedsPlayerBotProgressionBoots(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionBootsVnum(ch), WEAR_FOOTS);
	}

	bool IsPlayerBotSpecialLevel30Weapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		return IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum());
	}

	bool HasPlayerBotSpecialLevel30Weapon(LPCHARACTER ch, bool requireAverageDamage)
	{
		if (!ch)
			return false;
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : INVENTORY_MAX_NUM;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(WEAR_WEAPON) : ch->GetInventoryItem(index);
				if (!IsPlayerBotSpecialLevel30Weapon(item) || !IsPlayerBotWeapon(ch, item))
					continue;
				if (!requireAverageDamage)
					return true;
				for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
					if (item->GetAttributeType(attr) == APPLY_NORMAL_HIT_DAMAGE_BONUS &&
							item->GetAttributeValue(attr) > 0)
						return true;
			}
		}
		return false;
	}

	bool HasPlayerBotM3ReadyEquipment(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
		LPITEM boots = ch->GetWear(WEAR_FOOTS);
		if (!weapon || !armor || !shield || !helmet || !boots)
			return false;
		if (ch->GetLevel() >= 20)
			return true;
		return ch->GetLevel() >= 15 && weapon->GetRefineLevel() >= 4 &&
				armor->GetRefineLevel() >= 4 && shield->GetRefineLevel() >= 4;
	}

	bool IsPlayerBotCoreProgressionItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item))
			return false;

		const int wearCell = item->FindEquipCell(ch);
		DWORD desiredVnum = 0;
		if (wearCell == WEAR_WEAPON)
		{
			if (!IsPlayerBotWeapon(ch, item))
				return false;
			desiredVnum = GetPlayerBotProgressionWeaponVnum(ch);
		}
		else if (wearCell == WEAR_BODY)
		{
			desiredVnum = GetPlayerBotProgressionArmorVnum(ch);
		}
		else if (wearCell == WEAR_SHIELD)
		{
			desiredVnum = GetPlayerBotProgressionShieldVnum(ch);
		}
		else if (wearCell == WEAR_HEAD)
		{
			desiredVnum = GetPlayerBotProgressionHelmetVnum(ch);
		}
		else if (wearCell == WEAR_FOOTS)
		{
			desiredVnum = GetPlayerBotProgressionBootsVnum(ch);
		}
		else
		{
			return false;
		}

		const TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		return item->GetLevelLimit() >= desiredLevel &&
				item->GetLevelLimit() <= ch->GetLevel();
	}

	// Could this character put the item this vnum names on? Asked of a refine
	// *result* before the anvil is used, because the engine will not ask.
	// DoRefine checks the result's level limit only under !g_iUseLocale - "in
	// korea only", says the comment - and this server runs with a locale, so a
	// level-40 bot could turn its Upiorna Kusza +1 (level 40) into a +2 that
	// needs 42, and then stand there unable to equip the weapon it just paid
	// for. Twenty-four families on this proto raise their level with the plus;
	// five of them - that crossbow, the Three Lords shield and the three
	// level-52 bells - sit inside the levels bots reach.
	bool IsPlayerBotWearableAtLevel(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || vnum == 0)
			return false;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto)
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL &&
					proto->aLimits[i].lValue > ch->GetLevel())
				return false;
		return true;
	}

	BYTE GetPlayerBotRefineTarget(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;

		// Equipment is a primary progression system, not a side activity. Every bot
		// aims for at least +6, while a stable per-character/per-family personality
		// decides who risks +7, +8 or +9. The actual attempt still goes through
		// DoRefine(false), so every result pays the real fee, consumes real materials
		// and can burn at the normal server success rate.
		const DWORD familyVnum = item->GetVnum() >= item->GetRefineLevel()
				? item->GetVnum() - item->GetRefineLevel() : item->GetVnum();
		const int wearCell = item->FindEquipCell(ch);
		const DWORD seed = ch->GetPlayerID() ^ (familyVnum * 0x9e3779b9U) ^
				((DWORD)(wearCell + 2) * 0x85ebca6bU);
		const DWORD ambition = PlayerBotNavHash(seed ^ 0x52454649U) % 1000U;
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = stateIt != s_mapPlayerBotAIStates.end()
				? stateIt->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// Gear specialists deliberately accept more upgrade risk. Careful collectors
		// still has a small chance to become the lucky +8/+9 outlier, but normally
		// protects the equipment already earned.
		const DWORD plusNineChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 120U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 20U : 50U);
		const DWORD plusEightChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 320U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 90U : 150U);
		const DWORD plusSevenChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 650U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 290U : 400U);
		if (ambition < plusNineChance)
			return 9; // exceptional 5% cohort
		if (ambition < plusEightChance)
			return 8; // another 10%
		if (ambition < plusSevenChance)
			return 7; // another 25%
		return 6;
	}

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category)
	{
		if (!ch || vnum == 0)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) < 0)
			return false;

		long long price = proto->dwShopBuyPrice > 0 ? proto->dwShopBuyPrice : proto->dwGold;
		price = std::max<long long>(100, price);
		if (ch->GetGold() < price)
			return false;

		LPITEM item = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!item)
			return false;
		ch->PointChange(POINT_GOLD, -price);
		sys_log(0, "PLAYERBOT_GEAR: bought progression %s pid=%u name=%s vnum=%u required_level=%d price=%lld",
				category ? category : "gear", ch->GetPlayerID(), ch->GetName(), vnum,
				item->GetLevelLimit(), price);
		return true;
	}

	int CountPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int count = 0;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (worn && worn->GetType() == ITEM_WEAPON && worn->GetSubType() == WEAPON_ARROW)
			count += worn->GetCount();
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW)
				count += item->GetCount();
		}
		return count;
	}

	bool EnsurePlayerBotArrowsEquipped(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (worn && worn->GetType() == ITEM_WEAPON && worn->GetSubType() == WEAPON_ARROW &&
				worn->GetCount() > 0)
			return true;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW &&
					item->GetCount() > 0 && ch->EquipItem(item, WEAR_ARROW))
				return true;
		}
		return false;
	}

	bool NeedsPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2)
			return false;
		return CountPlayerBotArrows(ch) < PLAYERBOT_ARROW_RESTOCK_THRESHOLD;
	}

	long long GetPlayerBotNpcPurchasePrice(const TItemTable* proto, int count)
	{
		if (!proto || count <= 0)
			return 0;
		if (IS_SET(proto->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			return proto->dwGold == 0 ? count : count / proto->dwGold;
		return (long long)proto->dwGold * count;
	}

	DWORD GetPlayerBotNpcSellUnitPrice(LPITEM item)
	{
		if (!item || !item->GetProto() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return 0;

		DWORD price = item->GetShopBuyPrice();
		if (IS_SET(item->GetFlag(), ITEM_FLAG_COUNT_PER_1GOLD))
			price = price == 0 ? 1 : 1 / price;
		price /= 5;
		price -= price * 3 / 100;
		return price;
	}

	enum EPlayerBotPotionSupply
	{
		PLAYERBOT_POTION_SUPPLY_HP = 0,
		PLAYERBOT_POTION_SUPPLY_SP,
		PLAYERBOT_POTION_SUPPLY_GREEN,
		PLAYERBOT_POTION_SUPPLY_PURPLE,
		PLAYERBOT_POTION_SUPPLY_NONE
	};

	EPlayerBotPotionSupply GetPlayerBotPotionSupply(DWORD vnum)
	{
		if (vnum == 27051 || (vnum >= 27001 && vnum <= 27003))
			return PLAYERBOT_POTION_SUPPLY_HP;
		if (vnum == 27052 || (vnum >= 27004 && vnum <= 27006))
			return PLAYERBOT_POTION_SUPPLY_SP;
		if (vnum == 27053 || (vnum >= 27100 && vnum <= 27102))
			return PLAYERBOT_POTION_SUPPLY_GREEN;
		if (vnum == 27054 || (vnum >= 27103 && vnum <= 27105))
			return PLAYERBOT_POTION_SUPPLY_PURPLE;
		return PLAYERBOT_POTION_SUPPLY_NONE;
	}

	DWORD GetPlayerBotPotionSupplyLimit(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		const bool lowLevel = !ch || ch->GetLevel() <= 10;
		const bool mage = ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		switch (supply)
		{
			// A bot with yang in the bank should carry a real belt, not a token
			// one: potions are cheap next to what it earns, and running dry is
			// what sends it home in the middle of a good spot. These are also the
			// limits the excess-potion sale trims down to, so they have to move
			// together with the purchase below.
			case PLAYERBOT_POTION_SUPPLY_HP:     return lowLevel ? 160 : 800;
			case PLAYERBOT_POTION_SUPPLY_SP:     return lowLevel ? (mage ? 100 : 50) : 600;
			case PLAYERBOT_POTION_SUPPLY_GREEN:  return 30;
			case PLAYERBOT_POTION_SUPPLY_PURPLE: return 30;
			default: return 0;
		}
	}

	DWORD CountPlayerBotPotionSupply(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		if (!ch || supply == PLAYERBOT_POTION_SUPPLY_NONE)
			return 0;
		DWORD count = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && GetPlayerBotPotionSupply(item->GetVnum()) == supply)
				count += item->GetCount();
		}
		return count;
	}

	bool HasPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			if (CountPlayerBotPotionSupply(ch, kind) >
					GetPlayerBotPotionSupplyLimit(ch, kind))
				return true;
		}
		return false;
	}

	bool CanMergePlayerBotPotionStacks(LPITEM destination, LPITEM source)
	{
		if (!destination || !source || destination == source ||
				destination->GetVnum() != source->GetVnum() ||
				GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE ||
				!destination->IsStackable() || !source->IsStackable() ||
				IS_SET(destination->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
				IS_SET(source->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			if (destination->GetSocket(socket) != source->GetSocket(socket))
				return false;
		for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			if (destination->GetAttributeType(attr) != source->GetAttributeType(attr) ||
					destination->GetAttributeValue(attr) != source->GetAttributeValue(attr))
				return false;
		return true;
	}

	bool CompactPlayerBotPotionStacks(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		DWORD movedUnits = 0;
		DWORD removedStacks = 0;
		for (WORD destinationCell = 0; destinationCell < INVENTORY_MAX_NUM; ++destinationCell)
		{
			LPITEM destination = ch->GetInventoryItem(destinationCell);
			if (!destination || destination->GetCount() >= 200 ||
					GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE)
				continue;
			for (WORD sourceCell = destinationCell + 1;
					sourceCell < INVENTORY_MAX_NUM && destination->GetCount() < 200;
					++sourceCell)
			{
				LPITEM source = ch->GetInventoryItem(sourceCell);
				if (!CanMergePlayerBotPotionStacks(destination, source))
					continue;
				const DWORD sourceCount = source->GetCount();
				const DWORD transfer = std::min<DWORD>(200 - destination->GetCount(), sourceCount);
				if (transfer == 0)
					continue;
				destination->SetCount(destination->GetCount() + transfer);
				source->SetCount(sourceCount - transfer);
				movedUnits += transfer;
				if (transfer == sourceCount)
					++removedStacks;
			}
		}
		if (movedUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: compacted potions pid=%u name=%s moved=%u freed_stacks=%u",
					ch->GetPlayerID(), ch->GetName(), movedUnits, removedStacks);
		return movedUnits > 0;
	}

	bool SellPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// Sell weaker variants first, while retaining a bounded combat/travel reserve.
		const DWORD saleOrder[] = {
			27051, 27001, 27002, 27003,
			27052, 27004, 27005, 27006,
			27053, 27100, 27101, 27102,
			27054, 27103, 27104, 27105
		};
		DWORD soldUnits = 0;
		long long earnedGold = 0;
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			DWORD total = CountPlayerBotPotionSupply(ch, kind);
			const DWORD keep = GetPlayerBotPotionSupplyLimit(ch, kind);
			if (total <= keep)
				continue;
			DWORD excess = total - keep;
			for (size_t order = 0;
					order < sizeof(saleOrder) / sizeof(saleOrder[0]) && excess > 0; ++order)
			{
				if (GetPlayerBotPotionSupply(saleOrder[order]) != kind)
					continue;
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM && excess > 0; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != saleOrder[order])
						continue;
					const DWORD unitPrice = GetPlayerBotNpcSellUnitPrice(item);
					if (unitPrice == 0)
						continue;
					const DWORD count = std::min<DWORD>(excess, item->GetCount());
					item->SetCount(item->GetCount() - count);
					ch->PointChange(POINT_GOLD, (long long)unitPrice * count);
					excess -= count;
					soldUnits += count;
					earnedGold += (long long)unitPrice * count;
				}
			}
		}
		if (soldUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: sold excess potions pid=%u name=%s units=%u earned=%lld gold=%lld",
					ch->GetPlayerID(), ch->GetName(), soldUnits, earnedGold,
					(long long)ch->GetGold());
		return soldUnits > 0;
	}

	bool RaisePlayerBotEmergencyGold(LPCHARACTER ch, long long requiredGold,
			const char* reason)
	{
		if (!ch || ch->GetGold() >= requiredGold)
			return false;

		// The native NPC shop accepts potions too. Sell only as many surplus units
		// as are required to restore an essential weapon/ammunition purchase. Blue
		// potions go first and both HP/SP reserves remain protected.
		const DWORD potionVnums[] = {
			27004, 27005, 27006, 27052,
			27001, 27002, 27003, 27051
		};
		for (size_t v = 0; v < sizeof(potionVnums) / sizeof(potionVnums[0]); ++v)
		{
			const bool bluePotion = v < 4;
			const DWORD reserve = bluePotion ? 10 : 30;
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != potionVnums[v] || item->GetCount() <= reserve)
					continue;

				const DWORD price = GetPlayerBotNpcSellUnitPrice(item);
				if (price == 0)
					continue;

				const long long deficit = requiredGold - ch->GetGold();
				const DWORD available = item->GetCount() - reserve;
				DWORD count = (DWORD)((deficit + price - 1) / price);
				count = std::max<DWORD>(1, std::min<DWORD>(count, available));
				item->SetCount(item->GetCount() - count);
				ch->PointChange(POINT_GOLD, (long long)price * count);
				sys_log(0, "PLAYERBOT_GEAR: emergency sale pid=%u name=%s reason=%s vnum=%u count=%u earned=%lld total_gold=%lld required=%lld",
						ch->GetPlayerID(), ch->GetName(), reason ? reason : "supply",
						potionVnums[v], count, (long long)price * count,
						(long long)ch->GetGold(), requiredGold);
				if (ch->GetGold() >= requiredGold)
					return true;
			}
		}
		return ch->GetGold() >= requiredGold;
	}

	bool BuyPlayerBotArrowsAtMerchant(LPCHARACTER ch)
	{
		if (!NeedsPlayerBotArrows(ch))
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_WOODEN_ARROW_VNUM);
		if (!proto)
			return false;

		const long long smallPrice = GetPlayerBotNpcPurchasePrice(
				proto, PLAYERBOT_ARROW_SMALL_BUNDLE);
		if (ch->GetGold() < smallPrice)
			RaisePlayerBotEmergencyGold(ch, smallPrice, "arrows");

		int bundle = 0;
		long long price = GetPlayerBotNpcPurchasePrice(
				proto, PLAYERBOT_ARROW_LARGE_BUNDLE);
		if (price > 0 && ch->GetGold() >= price)
			bundle = PLAYERBOT_ARROW_LARGE_BUNDLE;
		else
		{
			price = smallPrice;
			if (price > 0 && ch->GetGold() >= price)
				bundle = PLAYERBOT_ARROW_SMALL_BUNDLE;
		}
		if (bundle == 0)
			return false;

		LPITEM arrows = ch->AutoGiveItem(
				PLAYERBOT_WOODEN_ARROW_VNUM, bundle, -1, false);
		if (!arrows)
			return false;
		ch->PointChange(POINT_GOLD, -price);
		const bool equipped = EnsurePlayerBotArrowsEquipped(ch);
		sys_log(0, "PLAYERBOT_GEAR: bought wooden arrows pid=%u name=%s vnum=%u count=%d price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_WOODEN_ARROW_VNUM,
				bundle, price, equipped ? 1 : 0);
		return true;
	}

	bool EquipFirstAvailablePlayerBotWeapon(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotWeapon(ch, item))
				continue;

			const DWORD vnum = item->GetVnum();
			if (ch->CanEquipNow(item, TItemPos(INVENTORY, cell)) && ch->EquipItem(item))
			{
				sys_log(0, "PLAYERBOT_AI: equipped weapon pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), vnum);
				return ch->GetWear(WEAR_WEAPON) != NULL;
			}
		}

		return false;
	}

	bool BuyPlayerBotEmergencyWeapon(LPCHARACTER ch)
	{
		if (!ch || ch->GetWear(WEAR_WEAPON))
			return ch && ch->GetWear(WEAR_WEAPON);

		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		const long long price = GetPlayerBotEmergencyWeaponPrice(ch);
		if (vnum == 0)
			return false;
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, "weapon");
		if (ch->GetGold() < price)
			return false;

		LPITEM weapon = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!weapon)
			return false;

		ch->PointChange(POINT_GOLD, -price);
		const bool equipped = ch->EquipItem(weapon);

		sys_log(0, "PLAYERBOT_AI: bought emergency weapon pid=%u name=%s vnum=%u price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), vnum, price, equipped ? 1 : 0);
		return equipped;
	}

	bool PrepareWeapon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		LPITEM equippedWeapon = ch->GetWear(WEAR_WEAPON);
		if (equippedWeapon && IsPlayerBotWeapon(ch, equippedWeapon))
		{
			state.dwEmergencyScavengeUntil = 0;
			if (equippedWeapon->GetSubType() == WEAPON_BOW)
				return EnsurePlayerBotArrowsEquipped(ch);
			return true;
		}
		if (equippedWeapon)
		{
			const DWORD wrongVnum = equippedWeapon->GetVnum();
			if (ch->GetEmptyInventory(equippedWeapon->GetSize()) >= 0)
			{
				ch->UnequipItem(equippedWeapon);
				sys_log(0, "PLAYERBOT_AI: unequipped profession-incompatible weapon pid=%u name=%s vnum=%u group=%u",
						ch->GetPlayerID(), ch->GetName(), wrongVnum, ch->GetSkillGroup());
			}
			if (ch->GetWear(WEAR_WEAPON))
				return false;
		}

		if (!ch->IsItemLoaded() || dwNow < state.dwNextGearAttemptTime)
			return false;

		state.dwNextGearAttemptTime = dwNow + PLAYERBOT_GEAR_RETRY_INTERVAL;

		if (EquipFirstAvailablePlayerBotWeapon(ch))
		{
			state.dwEmergencyScavengeUntil = 0;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		const DWORD dwStarterChestVnum = GetStarterChestVnum(ch->GetJob());
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD chestVnum = item->GetVnum();
			const bool classStarterChest = (dwStarterChestVnum != 0 && chestVnum == dwStarterChestVnum);
			const bool progressionChest = (chestVnum >= 50187 && chestVnum <= 50196);
			const int progressionLevel = (chestVnum == 50187) ? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarterChest && (!progressionChest || ch->GetLevel() < progressionLevel))
				continue;

			sys_log(0, "PLAYERBOT_AI: opening weapon recovery chest pid=%u name=%s vnum=%u cell=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, cell);
			ch->UseItem(TItemPos(INVENTORY, cell));
			if (!EquipFirstAvailablePlayerBotWeapon(ch))
				return false;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		// No remote purchase or free fallback.  The update loop first gives the
		// bot a chance to collect an owned weapon drop and then starts a visible
		// trip to the real Weapon Merchant.  Archer arrows follow the same rule.
		if (dwNow >= state.dwNextGearLogTime)
		{
			state.dwNextGearLogTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
			sys_err("PLAYERBOT_AI: idle without weapon pid=%u name=%s expected_chest=%u",
					ch->GetPlayerID(), ch->GetName(), dwStarterChestVnum);
		}

		return false;
	}

	bool ManagePlayerBotProgressionChests(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() ||
			dwNow < state.dwNextProgressionChestCheckTime)
			return false;
		state.dwNextProgressionChestCheckTime = dwNow + 10000 +
				(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);

		// The seed historically supplied one starter chest and the stock
		// give_basic_weapon quest supplied another on first login. Since every
		// apprentice chest contains the next tier, that duplicated the entire
		// progression chain. These boxes are one-per-character rewards: retain one
		// copy of each tier and remove only the artificial duplicates.
		std::map<DWORD, bool> seenProgressionChests;
		DWORD removedChestUnits = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			const bool progression = (vnum >= 50187 && vnum <= 50196) ||
					vnum == 50212 || vnum == 50213;
			if (!progression)
				continue;

			const DWORD count = std::max<DWORD>(1, item->GetCount());
			if (seenProgressionChests.find(vnum) != seenProgressionChests.end())
			{
				removedChestUnits += count;
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DUPLICATE_CHEST");
				continue;
			}

			seenProgressionChests[vnum] = true;
			if (count > 1)
			{
				removedChestUnits += count - 1;
				item->SetCount(1);
			}
		}
		if (removedChestUnits > 0)
			sys_log(0, "PLAYERBOT_GEAR: removed duplicate progression chests pid=%u name=%s units=%u",
					ch->GetPlayerID(), ch->GetName(), removedChestUnits);

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if ((target && !target->IsDead()) ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < 3000))
			return false;

		const DWORD starterVnum = GetStarterChestVnum(ch->GetJob());
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD chestVnum = item->GetVnum();
			const bool classStarter = starterVnum != 0 && chestVnum == starterVnum;
			const bool progression = chestVnum >= 50187 && chestVnum <= 50196;
			const int requiredLevel = chestVnum == 50187
					? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarter && (!progression || ch->GetLevel() < requiredLevel))
				continue;

			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			state.dwNextGearAttemptTime = 0;
			sys_log(0, "PLAYERBOT_GEAR: opened progression chest pid=%u name=%s vnum=%u level=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel());
			return true;
		}
		return false;
	}

	bool UseHealthPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ch->GetMaxHP() <= 0 || ch->GetHP() * 100 > ch->GetMaxHP() * PLAYERBOT_POTION_HP_PERCENT)
			return false;

		if (dwNow < state.dwNextPotionTime)
			return false;

		state.dwNextPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27051 is the beginner red potion supplied by the level-one chest.
		const DWORD redPotionVnums[] = { 27051, 27001, 27002, 27003 };
		for (size_t potionIndex = 0; potionIndex < sizeof(redPotionVnums) / sizeof(redPotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != redPotionVnums[potionIndex])
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used health potion pid=%u name=%s vnum=%u hp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetHP(), ch->GetMaxHP());
					return true;
				}
			}
		}

		if (dwNow >= state.dwNextPotionLogTime)
		{
			state.dwNextPotionLogTime = dwNow + PLAYERBOT_POTION_LOG_INTERVAL;
			sys_log(0, "PLAYERBOT_AI: no usable health potion pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
		}

		return false;
	}

	bool UseManaPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ch->GetMaxSP() <= 0 || ch->GetSP() * 100 > ch->GetMaxSP() * PLAYERBOT_POTION_SP_PERCENT)
			return false;

		if (dwNow < state.dwNextManaPotionTime)
			return false;

		state.dwNextManaPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27052 is the beginner blue potion, followed by standard small, medium, large blue potions.
		const DWORD bluePotionVnums[] = { 27052, 27004, 27005, 27006 };
		for (size_t potionIndex = 0; potionIndex < sizeof(bluePotionVnums) / sizeof(bluePotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != bluePotionVnums[potionIndex])
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used mana potion pid=%u name=%s vnum=%u sp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetSP(), ch->GetMaxSP());
					return true;
				}
			}
		}

		return false;
	}

	bool UseUtilityPotions(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		const bool activeCombat = target && !target->IsDead() &&
				(target->IsMonster() || target->IsStone());
		const bool importantFight = activeCombat && (target->IsStone() ||
				(target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS));
		// One third of ordinary grinders plans a longer session and uses attack-speed
		// potions as well. Every bot uses them for Metins/bosses, but nobody drinks
		// one merely while waiting at an NPC or recovering from death.
		const bool longGrindingSession = activeCombat && target->IsMonster() &&
				state.bLongTermGoal == BOT_GOAL_LEVEL_UP &&
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x47524545U) % 3U) == 0;
		const bool shouldUseGreen = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(importantFight || longGrindingSession);

		// 1. Green Potion (Zielona Mikstura - Attack Speed)
		if (shouldUseGreen && ch->FindAffect(AFFECT_ATT_SPEED) == NULL &&
				state.mapBuffActiveUntil[27102] <= dwNow)
		{
			const DWORD greenPotionVnums[] = { 27102, 27101, 27100, 27053 };
			for (size_t i = 0; i < sizeof(greenPotionVnums) / sizeof(greenPotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != greenPotionVnums[i])
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						// FindAffect is authoritative for the real item duration. This short
						// guard only prevents a broken proto from being consumed every tick.
						state.mapBuffActiveUntil[27102] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used green potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		// 2. Purple Potion (Fioletowa Mikstura - Movement Speed). Use it for travel,
		// loot runs and the approach to a distant target, not while standing at NPCs.
		const bool shouldUsePurple = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(!activeCombat || DISTANCE_APPROX(ch->GetX() - target->GetX(),
					target->GetY() - ch->GetY()) > 500);
		if (shouldUsePurple && ch->FindAffect(AFFECT_MOV_SPEED) == NULL &&
				state.mapBuffActiveUntil[27105] <= dwNow)
		{
			const DWORD purplePotionVnums[] = { 27105, 27104, 27103, 27054 };
			for (size_t i = 0; i < sizeof(purplePotionVnums) / sizeof(purplePotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != purplePotionVnums[i])
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						state.mapBuffActiveUntil[27105] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used purple potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		return false;
	}
}

#endif
