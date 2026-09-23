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
	// "Write this item's row now." The db core keeps a changed item in its
	// cache for PLAYER_CACHE_FLUSH_SECONDS - seven minutes by default, which
	// is what this world runs - before MariaDB sees it, so anything reading
	// player.item (both panels) is that far behind a bot's bag. The engine's
	// own answer is HEADER_GD_ITEM_FLUSH, which CInputMain sends after a shop
	// deal; it costs one write, so it is for the rare, visible changes - what
	// a bot wears - and never for a bag that turns over every few seconds.
	void FlushPlayerBotItemRow(LPITEM item)
	{
		if (!item || item->GetID() == 0 || !db_clientdesc)
			return;
		ITEM_MANAGER::instance().FlushDelayedSave(item);
		const DWORD dwID = item->GetID();
		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(DWORD));
		db_clientdesc->Packet(&dwID, sizeof(DWORD));
	}

	// Defined with the town code. Buying anything means standing at an NPC
	// first, and where exactly is a town concern, not a gear one.
	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	// Defined with the bag rules (playerbot_economy.h): an item's own stack
	// limit, which on mt2009 is the proto's and not always two hundred.
	int PlayerBotMaxStack(LPITEM item);

	// Defined with the chest pass (playerbot_consumables.h): a box the engine
	// refused this bot, remembered by bot and vnum. The two passes here that
	// open a starter-chain chest by themselves ask it before they try and tell
	// it when UseItem says no, or a box that cannot open is asked for again on
	// every one of their passes.
	bool IsPlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow);
	void NotePlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow);
	// And the free column of three a giftbox opens into, made the same way
	// (FreePlayerBotGiftboxColumn, playerbot_consumables.h).
	bool FreePlayerBotGiftboxColumn(LPCHARACTER ch);

	// Whether this character fights with a weapon of this kind. Asked of an
	// item by IsPlayerBotWeapon and of a merchant's proto by
	// GetPlayerBotMerchantWeaponCeiling.
	bool IsPlayerBotWeaponSubTypeFor(LPCHARACTER ch, BYTE subType)
	{
		if (ch)
		{
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

		switch (subType)
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

	bool IsPlayerBotWeapon(LPCHARACTER ch, LPITEM item)
	{
		return item && item->GetType() == ITEM_WEAPON &&
				IsPlayerBotWeaponSubTypeFor(ch, item->GetSubType());
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
		// Two uniques are never worn, and the rings and gloves on a clock are
		// the unique-slot pass's to put on and take off (playerbot_unique_slots.h):
		// an empty unique slot took any unique here, which is how a ring that
		// hides the level ended up on eleven bots.
		if (item->GetType() == ITEM_UNIQUE &&
				(IsPlayerBotNeverWornUnique(item->GetVnum()) || IsPlayerBotTimedUnique(item->GetVnum())))
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

	// An item a wear slot points at that the engine really wears: owned by this
	// character, flagged equipped, and in that slot's own cell. The first
	// version of the emergency weapon purchase equipped whatever AutoGiveItem
	// handed back, and with a full bag that is an item lying on the ground: a
	// Miecz+0 then sat in the slot and on the ground at once (BROLID, 15
	// September, 11:10:34), its ground timer fired five minutes later ("Owner
	// exist"), and the next blacksmith visit ended with RemoveFromCharacter's
	// "Invalid Item Position", a destroyed item still in the weapon slot, and
	// twenty-one equips of a sword over it in one second - FAST_ITEM_SWAP, and
	// a bot thrown out of the game. Whatever put an item there, nothing is
	// swapped, refined or taken off through a slot that fails this.
	bool IsPlayerBotWornItemSound(LPCHARACTER ch, LPITEM item, int wearCell)
	{
		return ch && item && wearCell >= 0 && item->GetOwner() == ch && item->IsEquipped() &&
				(int)item->GetCell() == (int)INVENTORY_MAX_NUM + wearCell;
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
			// Fifteen a point: fifteen hundred health - half again what a
			// character of forty has - is worth more than twenty-two points of
			// defence, and at ten it was worth fifteen, less than the seven
			// points that separated two armours a bot chose between.
			case APPLY_MAX_HP:
				return (long long)lValue * 15;
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
				// A quarter of a boss's health a proc: PLAYERBOT_POISON_BOSS_LEVEL.
				return (long long)lValue *
						(ch && (int)ch->GetLevel() >= PLAYERBOT_POISON_BOSS_LEVEL ? 400 : 200);
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

	// Every line of one apply type an item carries: the proto's fixed applies and
	// the rolled attributes together.
	long SumPlayerBotItemLines(LPITEM item, BYTE applyType)
	{
		if (!item || !item->GetProto() || applyType == 0)
			return 0;
		long total = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			if (item->GetProto()->aApplies[i].bType == applyType)
				total += item->GetProto()->aApplies[i].lValue;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) == applyType)
				total += item->GetAttributeValue(i);
		return total;
	}

	// The lines a hit is made of, and which the hit model below reads itself -
	// the flat scorer must not count them a second time on a weapon.
	bool IsPlayerBotHitModelApply(BYTE applyType, LPCHARACTER ch)
	{
		switch (applyType)
		{
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
			case APPLY_SKILL_DAMAGE_BONUS:
			case APPLY_ATT_GRADE_BONUS:
			case APPLY_STR:
			case APPLY_CRITICAL_PCT:
			case APPLY_PENETRATE_PCT:
			case APPLY_ATTBONUS_MONSTER:
				return true;
			default:
				break;
		}
		int racePercent = 0;
		const int dominant = ch ? GetPlayerBotFightingRace(ch, &racePercent) : PLAYERBOT_RACE_NONE;
		return dominant != PLAYERBOT_RACE_NONE && applyType == GetPlayerBotRaceApplyType(dominant);
	}

	// What a character brings to a hit on one point, with the weapon it wears
	// taken back out: the candidate's own lines are then added, so the weapon in
	// the hand and two in the bag are all read against the same body.
	long PlayerBotPointWithoutWornWeapon(LPCHARACTER ch, BYTE point, BYTE applyType, LPITEM candidate)
	{
		if (!ch)
			return 0;
		long value = ch->GetPoint(point);
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn != candidate)
			value -= SumPlayerBotItemLines(worn, applyType);
		return value;
	}

	// The lines of one apply type a weapon carries at another plus: the fixed
	// applies of that plus's proto and the attributes rolled on this piece,
	// which every refine copies across (ITEM_MANAGER::CopyAllAttrTo). A NULL
	// piece is a weapon nobody holds yet: the proto's lines alone.
	long SumPlayerBotLinesAt(LPITEM item, const TItemTable* proto, BYTE applyType)
	{
		if (!proto || applyType == 0)
			return 0;
		long total = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			if (proto->aApplies[i].bType == applyType)
				total += proto->aApplies[i].lValue;
		if (item)
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (item->GetAttributeType(i) == applyType)
					total += item->GetAttributeValue(i);
		return total;
	}

	// The share mt2009 adds to a normal hit on a monster by the weapon's own
	// level (CHARACTER::Damage, DAMAGE_TYPE_NORMAL): a level limit of 32 to 65
	// adds limit * 30 / 100 - 3 percent, the level-65 elite families excepted,
	// and 70 or 75 adds ten. No tooltip shows it, and it is why a Krwawy Miecz
	// (level 45) hits ten percent harder than its numbers say while a level-30
	// weapon gets nothing. r40250 has no such rule.
	int GetPlayerBotWeaponLevelBonusPercent(const TItemTable* proto)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (!proto || proto->bType != ITEM_WEAPON)
			return 0;
		long limit = 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				limit = proto->aLimits[i].lValue;
		const DWORD vnum = proto->dwVnum;
		// The engine's list; its last range (7140..5149) is empty and excepts
		// no fan, so neither does this one.
		const bool elite65 = (vnum >= 140 && vnum <= 159) || (vnum >= 1100 && vnum <= 1109) ||
				(vnum >= 2140 && vnum <= 2149) || (vnum >= 3130 && vnum <= 3139) ||
				(vnum >= 5100 && vnum <= 5109);
		if (limit >= 32 && limit <= 65 && !elite65)
			return (int)(limit * 30 / 100 - 3);
		if (limit == 70 || limit == 75)
			return 10;
		return 0;
#else
		(void)proto;
		return 0;
#endif
	}

	// One blow with this weapon at the plus whose proto is given, expected
	// over the dice, against a monster of the bot's own level - the way
	// battle.cpp and char_battle.cpp deal it ("moze warto aby postacie znaly
	// algorytm obrazen danej broni", Tieru, 15 September):
	//
	//   AR     = (min(90, (DX*4 + level*2) / 6) + 210) / 300
	//            - (2*ER + 5) / (ER + 95) * 0.3, ER the monster's own rating
	//   atk    = (ATT_GRADE + roll*2 - level*2) * AR + level*2 + value5*2
	//   atk   *= 100 + attack percent, then the race line for its share
	//   hit    = atk - defence (about level + 15 on this proto)
	//   hit   *= 100 + average line, then the weapon-level bonus (mt2009)
	//   skill  = atk before the defence (a magic school: grade + magic roll),
	//            under 100 + skill-damage line
	//
	// A critical is a second hit one time in a hundred per percent; piercing
	// hands the defence back, counted at half. What the candidate would change
	// on the character - STR, grade, the percent lines - is measured against
	// the character without the weapon it wears, so the weapon in the hand and
	// two in the bag are read against the same body. The build decides the mix
	// (PLAYERBOT_WEAPON_OWN_LINE_PERCENT / _OTHER_LINE_PERCENT): a skill school
	// earns with skills and still swings, a blow school the other way round.
	// Skill damage used to be left out as "a PvP line", while char_battle.cpp
	// multiplies every skill on a monster by it, so a shaman's skill line was
	// worth nothing. And the defence is what the old model lacked most: a
	// percent line multiplies what is left after it, so a big line on a weak
	// base is worth less than it reads. assumedAverage stands in for the lines
	// of a weapon nobody holds (item NULL).
	long long GetPlayerBotWeaponHitDamageAt(LPITEM item, const TItemTable* proto, LPCHARACTER ch,
			long assumedAverage = 0)
	{
		if (!proto || proto->bType != ITEM_WEAPON)
			return 0;
		long long roll = (long long)proto->alValues[3] + proto->alValues[4];
		const long long magicRoll = (long long)proto->alValues[1] + proto->alValues[2];
		const long long plusAttack = 2LL * proto->alValues[5];
		// As before: a dagger's interval is half a sword's and a bow's roll is
		// doubled by CalcArrowDamage, both counted as a doubled roll.
		if (proto->bSubType == WEAPON_DAGGER || proto->bSubType == WEAPON_BOW)
			roll *= 2;
		long level = ch ? (long)ch->GetLevel() : 0;
		if (!ch)
			for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
				if (proto->aLimits[i].bType == LIMIT_LEVEL)
					level = proto->aLimits[i].lValue;

		LPITEM worn = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		// The weapon in the hand at its own plus is already in every point; any
		// other reading takes the worn weapon's lines out and puts these in.
		const bool inHand = item != NULL && item == worn && proto == item->GetProto();
		LPITEM kept = inHand ? item : NULL;
		const long assumed = item ? 0 : assumedAverage;
		long long grade = 0, attPct = 0, avgPct = 0, skillPct = 0, critPct = 0, penPct = 0;
		if (ch)
		{
			grade = PlayerBotPointWithoutWornWeapon(ch, POINT_ATT_GRADE, APPLY_ATT_GRADE_BONUS, kept);
			// No item carries an attack-percent line; POINT_ATT_BONUS comes from
			// affects and skills alone, the same for every candidate.
			attPct = ch->GetPoint(POINT_ATT_BONUS);
			avgPct = PlayerBotPointWithoutWornWeapon(ch, POINT_NORMAL_HIT_DAMAGE_BONUS, APPLY_NORMAL_HIT_DAMAGE_BONUS, kept);
			skillPct = PlayerBotPointWithoutWornWeapon(ch, POINT_SKILL_DAMAGE_BONUS, APPLY_SKILL_DAMAGE_BONUS, kept);
			critPct = PlayerBotPointWithoutWornWeapon(ch, POINT_CRITICAL_PCT, APPLY_CRITICAL_PCT, kept);
			penPct = PlayerBotPointWithoutWornWeapon(ch, POINT_PENETRATE_PCT, APPLY_PENETRATE_PCT, kept);
			if (!inHand)
			{
				if (worn)
					grade -= 2 * SumPlayerBotItemLines(worn, APPLY_STR);
				grade += SumPlayerBotLinesAt(item, proto, APPLY_ATT_GRADE_BONUS) +
						2 * SumPlayerBotLinesAt(item, proto, APPLY_STR);
				avgPct += SumPlayerBotLinesAt(item, proto, APPLY_NORMAL_HIT_DAMAGE_BONUS) + assumed;
				skillPct += SumPlayerBotLinesAt(item, proto, APPLY_SKILL_DAMAGE_BONUS);
				critPct += SumPlayerBotLinesAt(item, proto, APPLY_CRITICAL_PCT);
				penPct += SumPlayerBotLinesAt(item, proto, APPLY_PENETRATE_PCT);
			}
		}
		else
		{
			grade = SumPlayerBotLinesAt(item, proto, APPLY_ATT_GRADE_BONUS) +
					2 * SumPlayerBotLinesAt(item, proto, APPLY_STR);
			avgPct = SumPlayerBotLinesAt(item, proto, APPLY_NORMAL_HIT_DAMAGE_BONUS) + assumed;
			skillPct = SumPlayerBotLinesAt(item, proto, APPLY_SKILL_DAMAGE_BONUS);
			critPct = SumPlayerBotLinesAt(item, proto, APPLY_CRITICAL_PCT);
			penPct = SumPlayerBotLinesAt(item, proto, APPLY_PENETRATE_PCT);
		}
		long long racePct = SumPlayerBotLinesAt(item, proto, APPLY_ATTBONUS_MONSTER);
		int racePercent = 0;
		const int dominant = ch ? GetPlayerBotFightingRace(ch, &racePercent) : PLAYERBOT_RACE_NONE;
		if (dominant != PLAYERBOT_RACE_NONE && racePercent > 0)
			racePct += SumPlayerBotLinesAt(item, proto, GetPlayerBotRaceApplyType(dominant)) * racePercent / 100;

		// The attack rating against a monster of the bot's own level, whose DX
		// runs with its level: CalcAttackRating in thousandths.
		const long dx = ch ? (long)ch->GetPoint(POINT_DX) : level;
		const long arSrc = std::min<long>(90, (dx * 4 + level * 2) / 6);
		const long erSrc = std::min<long>(90, level);
		const long long ar = std::max<long long>(100,
				(long long)(arSrc + 210) * 1000 / 300 - (long long)(2 * erSrc + 5) * 300 / (erSrc + 95));
		const long long levelPart = 2LL * level;
		long long attack = (grade + roll - levelPart) * ar / 1000 + levelPart + plusAttack;
		if (attack < 1)
			attack = 1;
		attack = attack * std::max<long long>(20, 100 + attPct) / 100;
		attack = attack * std::max<long long>(20, 100 + racePct) / 100;

		const long long defence = (long long)level + PLAYERBOT_MONSTER_DEFENCE_OVER_LEVEL;
		long long hit = std::max<long long>(1, attack - defence);
		hit += defence * std::max<long long>(0, std::min<long long>(100, penPct)) / 200;
		hit = hit * std::max<long long>(20, 100 + avgPct) / 100;
		hit = hit * (100 + GetPlayerBotWeaponLevelBonusPercent(proto)) / 100;
		hit = hit * std::max<long long>(100, 100 + critPct) / 100;

		long long skill = IsPlayerBotMagicSchool(ch)
				? std::max<long long>(1, grade + magicRoll + plusAttack) : attack;
		skill = skill * std::max<long long>(20, 100 + skillPct) / 100;

		const int style = GetPlayerBotSchoolStyle(ch);
		const long long hitShare = style > 0 ? PLAYERBOT_WEAPON_OTHER_LINE_PERCENT : PLAYERBOT_WEAPON_OWN_LINE_PERCENT;
		const long long skillShare = style < 0 ? PLAYERBOT_WEAPON_OTHER_LINE_PERCENT : PLAYERBOT_WEAPON_OWN_LINE_PERCENT;
		const long long total = (hit * hitShare + skill * skillShare) / (hitShare + skillShare);
		return total < 1 ? 1 : total;
	}

	long long GetPlayerBotWeaponHitDamage(LPITEM item, LPCHARACTER ch)
	{
		if (!item || !item->GetProto() || item->GetType() != ITEM_WEAPON)
			return 0;
		return GetPlayerBotWeaponHitDamageAt(item, item->GetProto(), ch);
	}

	// Iwakura's PvE tier of this piece's family for this character, or 0
	// when his list does not rate the family (body armour, helmets, shields,
	// everything he judges by level and lines). The family is the +0 vnum,
	// the same arithmetic the price table uses.
	int GetPlayerBotItemTierOf(LPITEM item, LPCHARACTER ch)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9)
			return 0;
		return GetPlayerBotItemTier(item->GetVnum() - refine, ch ? (int)ch->GetJob() : -1, false);
	}

	// A line's worth, scaled by Iwakura's PvE tier of that kind of line
	// (PLAYERBOT_BONUS_TIER_PERCENT) - the same scale the reroll pass applies
	// in ScorePlayerBotBonusLine, so the pass that buys a piece and the pass
	// that rerolls it agree about what a line is worth.
	long long ScorePlayerBotApplyTiered(BYTE bType, long lValue, LPCHARACTER ch)
	{
		const long long raw = ScorePlayerBotApply(bType, lValue, ch);
		const int tier = ch ? GetPlayerBotBonusTier(bType, (int)ch->GetJob(), false) : 0;
		return tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
	}

	// A stone already in a socket, as the equipment score counts it: its own
	// lines, by his tier of the stone the way a bonus line goes by his tier of
	// the line. A stone seated before his list (a +0 to +2, or one he rates
	// low) still does what it does and counts at face value - a socket cannot
	// be emptied, so the piece is worth exactly what it holds.
	long long ScorePlayerBotSeatedSoulStones(LPITEM item, LPCHARACTER ch)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		long long score = 0;
		for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
		{
			const DWORD inSocket = (DWORD)item->GetSocket(socketIdx);
			if (inSocket <= 2 || inSocket == PLAYERBOT_BROKEN_SOUL_STONE_VNUM)
				continue;
			const TItemTable* stone = ITEM_MANAGER::instance().GetTable(inSocket);
			if (!stone || stone->bType != ITEM_METIN)
				continue;
			const int tier = GetPlayerBotSoulStoneTier(inSocket, false);
			for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			{
				const long long raw = ScorePlayerBotApply(stone->aApplies[i].bType, stone->aApplies[i].lValue, ch);
				score += tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
			}
		}
		return score;
	}

	long long GetPlayerBotEquipmentScore(LPITEM item, LPCHARACTER ch = NULL)
	{
		if (!item || !item->GetProto())
			return 0;

		long long score = 1;
		if (item->GetType() == ITEM_WEAPON)
		{
			// One expected hit, a thousand a point, so the flat lines and the
			// class preferences below keep the proportions they always had.
			score += GetPlayerBotWeaponHitDamage(item, ch) * 1000;

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
					// The two-handed weapon is worth the preference once the bot
					// fights from the battle horse (level 11): that is where a
					// Mental Warrior breaks Metin stones with it. Before that the
					// flat bonus made a +4 spike win over a +6 sword with a
					// thirty-percent bonus against monsters.
					// A share of its own blow, not a flat two hundred points of it:
					// at 75 two hundred is most of what a level-ten two-hander hits
					// for, so the flat bonus let one outscore a far better sword
					// (a Gilotynowe Ostrze +7 in the hand of a warrior of 75,
					// Tieru, 15 September). score is one plus the blow here.
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_TWO_HANDED &&
							ch->GetHorseLevel() >= PLAYERBOT_BATTLE_HORSE_LEVEL)
						score += (score - 1) * PLAYERBOT_TWO_HANDED_PREFERENCE_PERCENT / 100;
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
			// A piece the bot has outgrown by twenty levels or more loses ground
			// for every level past that. By the defence figure alone a Battle
			// Shield +6 (3 + 2*18) beats a level-41 shield at +3 (5 + 2*16), and
			// 61 of the bots past forty wore exactly that: the level-1 shield
			// bought at level 1, refined once and never put down, while the
			// level-41 shield sat in the bag. Each refine on the higher tier is
			// worth more than one on the lower, so the bot switches tiers and
			// refines that - a level-41 shield at +4 beats a level-21 one at +6.
			// The penalty is a share of the defence figure alone: the bonus lines
			// are added below and are worth what they are worth at any level.
			//
			// Compounded, not subtracted to nothing. At five percent a level up
			// to all of it, every armour twenty levels outgrown scored the same
			// single point, and at level 74 that is every body armour a merchant
			// sells: a sura wore a level-1 plate +6 with the level-34 one +4 in
			// its bag, then put the level-34 one on its counter (NaCoPaczysz,
			// 14 September). Each level past the threshold keeps ninety-five
			// percent of what the level before kept - at 74 a level-34 +4 keeps
			// 36%, a level-26 +6 24%, a level-1 +6 6% - so the higher tier wins.
			if (ch && (int)ch->GetLevel() - item->GetLevelLimit() > PLAYERBOT_ARMOR_OUTGROWN_LEVELS)
			{
				int outgrown = (int)ch->GetLevel() - item->GetLevelLimit() - PLAYERBOT_ARMOR_OUTGROWN_LEVELS;
				long long defence = score - 1;
				for (; outgrown > 0 && defence > 0; --outgrown)
					defence = defence * (100 - PLAYERBOT_ARMOR_OUTGROWN_PERCENT_PER_LEVEL) / 100;
				score = 1 + defence;
			}
		}

		// A weapon's two damage-percent lines were folded into its attack
		// above; everything else is a flat line.
		const bool bWeaponHitDone = item->GetType() == ITEM_WEAPON;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		{
			const BYTE t = item->GetProto()->aApplies[i].bType;
			if (bWeaponHitDone && IsPlayerBotHitModelApply(t, ch))
				continue;
			score += ScorePlayerBotApplyTiered(t, item->GetProto()->aApplies[i].lValue, ch);
		}
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (bWeaponHitDone && IsPlayerBotHitModelApply(t, ch))
				continue;
			score += ScorePlayerBotApplyTiered(t, item->GetAttributeValue(i), ch);
		}
		// The soul stones in its sockets are lines of the piece too: a weapon
		// holding Potwora and Smierci +4 is a different weapon from the same
		// one with the sockets open, and a swap for a bare one of a point
		// more would throw both stones away.
		score += ScorePlayerBotSeatedSoulStones(item, ch);

		if (item->GetImmuneFlag() != 0)
			score += 1000;

		// A race-attack bonus is only worth carrying where that race is what you
		// actually fight, and it is worth what share of the map that race is:
		// "strong against orcs" covers 63% of Orc Valley, "strong against
		// animals" the whole of a Monkey Dungeon, and nothing at all on the
		// desert, which is made of a race no item can reach. The share comes
		// from the same call the reroll scorer uses, so the pass that buys an
		// item and the pass that rerolls it can no longer disagree about the
		// line that made the bot pick it up.
		if (ch && item->GetType() != ITEM_WEAPON)
		{
			int racePercent = 0;
			const int dominant = GetPlayerBotFightingRace(ch, &racePercent);
			if (dominant != PLAYERBOT_RACE_NONE && racePercent > 0)
			{
				const BYTE wanted = GetPlayerBotRaceApplyType(dominant);
				const long long perPoint = (long long)PLAYERBOT_GEAR_RACE_LINE_VALUE *
						racePercent / 100;
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					if (item->GetAttributeType(i) == wanted)
						score += (long long)item->GetAttributeValue(i) * perPoint;
				}
				for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
				{
					if (item->GetProto()->aApplies[i].bType == wanted)
						score += (long long)item->GetProto()->aApplies[i].lValue * perPoint;
				}
			}
		}

		// Iwakura's tier of the family, as a nudge on the whole: the family's
		// own lines are already in the score, so this is his verdict on what
		// they are worth together, not a second count of them. Bounded by
		// PLAYERBOT_TIER_SCORE_PERCENT a step so that a +9 with lines still
		// beats a +1 of a better family with none.
		if (ch)
		{
			const int tier = GetPlayerBotItemTierOf(item, ch);
			if (tier > 0)
				score = score * (100 + (tier - 3) * PLAYERBOT_TIER_SCORE_PERCENT) / 100;
		}

		return score;
	}

	// The best weapon in the bag this character can wear now, other than
	// `except`, and its score.
	LPITEM FindPlayerBotBestBagWeapon(LPCHARACTER ch, LPITEM except, long long* scoreOut)
	{
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item == except || item->GetCell() != cell || item->IsEquipped() ||
					item->GetType() != ITEM_WEAPON || !IsPlayerBotEquipmentCandidate(ch, item) ||
					item->GetLevelLimit() > ch->GetLevel() || item->FindEquipCell(ch) != WEAR_WEAPON)
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!best || score > bestScore || (score == bestScore && item->GetID() < best->GetID()))
			{
				best = item;
				bestScore = score;
			}
		}
		if (scoreOut)
			*scoreOut = bestScore;
		return best;
	}

	// The weapon in the hand, or - with the hand empty, as it is for a whole
	// blacksmith session, or holding a rod or a pickaxe - the one that goes
	// back into it. A tool is not a weapon with a blow of nothing: read as one,
	// an angler's hand was outclassed by the whole atlas and every weapon on a
	// counter was worth its savings.
	LPITEM GetPlayerBotHandWeapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		return worn && worn->GetType() == ITEM_WEAPON ? worn : FindPlayerBotBestBagWeapon(ch, NULL, NULL);
	}

	// The weapon a bot keeps for the day the one in its hand burns: the best
	// other weapon in the bag it can wear, scoring at least
	// PLAYERBOT_REFINE_BACKUP_SCORE_PERCENT of the hand's. It is not a gift,
	// not merchant scrap and not counter goods; without one the refine pass
	// holds the hand's weapon off the plain anvil (IsPlayerBotWornWeaponAtRisk).
	LPITEM FindPlayerBotBackupWeapon(LPCHARACTER ch)
	{
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		if (!hand)
			return NULL;
		long long backupScore = 0;
		LPITEM backup = FindPlayerBotBestBagWeapon(ch, hand, &backupScore);
		if (!backup || backupScore * 100 <
				GetPlayerBotEquipmentScore(hand, ch) * PLAYERBOT_REFINE_BACKUP_SCORE_PERCENT)
			return NULL;
		return backup;
	}

	// FindPlayerBotBackupWeapon scores every weapon in the bag, and the junk
	// rule and the planner's refine question ask it once for every weapon in
	// the bag - the square of the bag's weapons, on every pass of 1100 bots
	// (the tick went from 8-9 s of 60 to 11 on its first deploy). The answer is
	// kept by item id for PLAYERBOT_BACKUP_WEAPON_CACHE_MS; what acts on it at
	// once - a gift, the refine at the anvil - asks afresh.
	struct TPlayerBotBackupWeaponAnswer
	{
		DWORD dwTime;
		DWORD dwItemID;
	};
	std::map<DWORD, TPlayerBotBackupWeaponAnswer> s_mapPlayerBotBackupWeapon;

	DWORD GetPlayerBotBackupWeaponID(LPCHARACTER ch, bool fresh)
	{
		if (!ch)
			return 0;
		const DWORD now = get_dword_time();
		TPlayerBotBackupWeaponAnswer& answer = s_mapPlayerBotBackupWeapon[ch->GetPlayerID()];
		if (fresh || answer.dwTime == 0 || now - answer.dwTime >= PLAYERBOT_BACKUP_WEAPON_CACHE_MS)
		{
			LPITEM backup = FindPlayerBotBackupWeapon(ch);
			answer.dwTime = now != 0 ? now : 1;
			answer.dwItemID = backup ? backup->GetID() : 0;
		}
		return answer.dwItemID;
	}

	bool IsPlayerBotKeptBackupWeapon(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		return ch && item && item->GetType() == ITEM_WEAPON && item->GetID() != 0 &&
				GetPlayerBotBackupWeaponID(ch, fresh) == item->GetID();
	}

	// A bot gives nothing away. It used to hand a spare it had outgrown to a
	// weaker bot of its class standing nearby, and the counter never saw it: a
	// bot raised Srebrne Kolczyki from +1 to +6 in a minute and gave them to
	// another while it wore copper ones itself (AkhiGubernator, 15 September:
	// "dobry samarytanin"). "Niech handluja ale nie daja za darmo" (Tieru):
	// what comes off stays in the bag, and the junk rule and the counter
	// decide what becomes of it. Nor does a party pass anything on any more:
	// the book of another class and the material a member was short of went
	// the same way ("usun", Tieru, 15 September).

	// The Archer's stone weapon (by build, whatever is in the hand - the
	// IsPlayerBotArcher of playerbot_targeting.h asks for the bow). A bow cannot break a Metin: the stone does
	// not move, the arrows run out, the shot's rhythm is a fraction of a
	// swing's, and the bot "fell over x times and gave up" (Kuszaa). A dagger
	// or a sword the ninja can wear, kept in the bag, goes into the hand for
	// the stone and comes out afterwards. Both are one item per bot - the
	// best by the equipment score - and the junk rule and the counter leave
	// that one alone.
	bool IsPlayerBotArcherBuild(LPCHARACTER ch)
	{
		return ch && ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2;
	}

	bool IsPlayerBotStoneMeleeWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_WEAPON)
			return false;
		const BYTE sub = item->GetSubType();
		return (sub == WEAPON_DAGGER || sub == WEAPON_SWORD) && item->CanUsedBy(ch) &&
				item->GetLevelLimit() <= ch->GetLevel();
	}

	// The best stone weapon the bot holds: in the bag, or - with includeWorn -
	// in the hand as well. NULL when there is none. A dagger beats a sword
	// whatever the score: it swings faster and costs less, and a stone has no
	// armour worth a heavier blow.
	LPITEM FindPlayerBotStoneWeapon(LPCHARACTER ch, bool includeWorn)
	{
		if (!ch)
			return NULL;
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotStoneMeleeWeapon(ch, item))
				continue;
			const bool dagger = item->GetSubType() == WEAPON_DAGGER;
			const bool bestDagger = best && best->GetSubType() == WEAPON_DAGGER;
			if (best && bestDagger && !dagger)
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!best || (dagger && !bestDagger) || score > bestScore)
			{
				best = item;
				bestScore = score;
			}
		}
		if (includeWorn && !best)
		{
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && IsPlayerBotStoneMeleeWeapon(ch, worn))
				best = worn;
		}
		return best;
	}

	// The one blade an Archer keeps for Metin stones - the chosen bag weapon, or
	// the one worn while it is on a stone. It is worth refining even though it is
	// never a wearable upgrade or a higher-tier spare, because a bow cannot break
	// a stone and a +0 dagger barely can.
	bool IsPlayerBotArcherStoneWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!item || !IsPlayerBotArcherBuild(ch) || !IsPlayerBotStoneMeleeWeapon(ch, item))
			return false;
		return FindPlayerBotStoneWeapon(ch, false) == item ||
				ch->GetWear(WEAR_WEAPON) == item;
	}

	// Whether the Archer holds a dagger good enough to break a stone: at least
	// +4, worn or in the bag. Below that a stone is not worth taking on alone -
	// the dagger is refined at the blacksmith towards this first.
	bool HasPlayerBotUsableStoneDagger(LPCHARACTER ch)
	{
		LPITEM w = FindPlayerBotStoneWeapon(ch, true);
		return w && w->GetRefineLevel() >= PLAYERBOT_ARCHER_STONE_MIN_REFINE;
	}

	// What the hand should hold right now: the job's weapon, or the stone
	// weapon while an Archer is on a stone.
	bool PlayerBotWeaponFitsNow(LPCHARACTER ch, const TPlayerBotAIState& state, LPITEM item)
	{
		if (IsPlayerBotArcherBuild(ch) && state.bMeleeForStone)
			return IsPlayerBotStoneMeleeWeapon(ch, item);
		return IsPlayerBotWeapon(ch, item);
	}

	// What the rolled lines of a piece are worth, by Iwakura's tiers - the
	// equipment score's own count of them (ScorePlayerBotApplyTiered), without
	// the base the piece is made of.
	long long GetPlayerBotItemLineScore(LPITEM item, LPCHARACTER ch)
	{
		long long lines = 0;
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0 && item->GetAttributeValue(i) > 0)
				lines += ScorePlayerBotApplyTiered(item->GetAttributeType(i), item->GetAttributeValue(i), ch);
		return lines;
	}

	// Defined in playerbot_bonus.h, later: a stone in the bag for this piece.
	bool PlayerBotHoldsBonusStoneFor(LPCHARACTER ch, LPITEM item);

	// Iwakura's community patch 2, point 3 ("zasada wymiany ekwipunku"): a
	// bot wearing a piece with, say, fifteen hundred health keeps a new one in
	// the bag until it has bonused it and the new lines outweigh the old -
	// whatever the new base is worth. The weapon is outside it: its average
	// line is in its blow already (GetPlayerBotWeaponHitDamage). And the wait
	// lasts only while it can end: with no stone in the bag for the new piece
	// the swap goes by the score as ever, or a bot without stones would keep
	// its level-one boots for life.
	bool IsPlayerBotSwapHeldForBonus(LPCHARACTER ch, LPITEM candidate, LPITEM worn)
	{
		if (!ch || !candidate || !worn || !IsPlayerBotPersonaEnabled() ||
				candidate->GetType() == ITEM_WEAPON || worn->GetType() == ITEM_WEAPON)
			return false;
		const long long wornLines = GetPlayerBotItemLineScore(worn, ch);
		if (wornLines <= 0 || GetPlayerBotItemLineScore(candidate, ch) >= wornLines)
			return false;
		return PlayerBotHoldsBonusStoneFor(ch, candidate);
	}

	bool ManagePlayerBotEquipment(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		// The Archer's stone mode, decided here because this pass is what
		// puts a weapon in the hand: on while the target is a standing stone
		// and a stone weapon is at hand, off the moment it is not - either
		// flip is looked at on this very tick.
		if (IsPlayerBotArcherBuild(ch))
		{
			LPCHARACTER target = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			// Only a dagger at +4 or better puts the Archer into melee; below that
			// it stays on the bow and reaches a stone only when others are already
			// breaking it (CanPlayerBotEngageStone). And never while a person has
			// asked this bot to lure: IsPlayerBotArcher wants the bow in the hand,
			// so a dagger drawn for one stone makes the whole course "ineligible"
			// until the stone is gone - with nothing anywhere saying why.
			const bool wantMelee = target && target->IsStone() && !target->IsDead() &&
					state.dwLurePlayerPID == 0 && HasPlayerBotUsableStoneDagger(ch);
			if (wantMelee != state.bMeleeForStone)
			{
				state.bMeleeForStone = wantMelee;
				state.dwNextEquipmentCheckTime = dwNow;
				sys_log(0, "PLAYERBOT_GEAR: archer %s pid=%u name=%s target_vid=%u",
						wantMelee ? "draws the stone weapon" : "takes the bow back",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)state.dwTargetVID);
			}
		}

		if (dwNow < state.dwNextEquipmentCheckTime && !state.bEquipPending)
			return false;

		LPITEM bestItem = NULL;
		LPITEM bestOldItem = NULL;
		int bestWearCell = -1;
		long long bestImprovement = 0;
		long long bestScore = 0;

		const bool stoneMode = IsPlayerBotArcherBuild(ch) && state.bMeleeForStone;
		// The one stone weapon the bot has chosen (dagger first), not any
		// blade in the bag: the score alone would put a heavier sword ahead.
		LPITEM chosenStoneWeapon = stoneMode ? FindPlayerBotStoneWeapon(ch, false) : NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const bool stoneWeapon = stoneMode && item == chosenStoneWeapon &&
					!item->IsExchanging();
			if (!stoneWeapon && !IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->GetType() == ITEM_WEAPON && !PlayerBotWeaponFitsNow(ch, state, item))
				continue;

			const int wearCell = item->FindEquipCell(ch);
			if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
				continue;

			LPITEM oldItem = ch->GetWear(wearCell);
			if (oldItem && !IsPlayerBotWornItemSound(ch, oldItem, wearCell))
			{
				PlayerBotLogThrottled("wear_slot_unsound", dwNow,
						"PLAYERBOT_AI: wear slot holds an item the engine does not wear pid=%u name=%s wear=%d",
						ch->GetPlayerID(), ch->GetName(), wearCell);
				continue;
			}
			if (oldItem && IS_SET(oldItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;
#if defined(PLAYERBOT_ENGINE_MT2009)
			// A fishing pass the fishing asked for a moment ago stays on; see
			// IsPlayerBotFishingPassHeld for the loop that taking it off made.
			if (oldItem && oldItem->GetVnum() == UNIQUE_ITEM_FISHING_PASS &&
					IsPlayerBotFishingPassHeld(ch->GetPlayerID(), dwNow))
				continue;
#endif
			// A ring or a glove on its clock comes off only through the
			// unique-slot pass, which knows whether the bot is hunting.
			if (oldItem && IsPlayerBotTimedUnique(oldItem->GetVnum()))
				continue;

			if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
				continue;

			const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
			// A weapon in the hand that does not fit the moment - the bow while
			// the Archer is on a stone, the dagger once the stone is gone - is
			// worth nothing against the one that does.
			const long long oldScore = oldItem
					? ((wearCell == WEAR_WEAPON && !PlayerBotWeaponFitsNow(ch, state, oldItem))
						? 0 : GetPlayerBotEquipmentScore(oldItem, ch))
					: 0;
			if (oldItem && itemScore <= oldScore)
				continue;
			// A new piece waits in the bag while its lines are worth less than
			// the worn one's and a stone can change that (community patch 2,
			// point 3); the bonus pass works on it there.
			if (oldItem && IsPlayerBotSwapHeldForBonus(ch, item, oldItem))
			{
				PlayerBotLogThrottled("swap_held_for_bonus", dwNow,
						"PLAYERBOT_BONUS: new piece waits in the bag for its lines pid=%u name=%s wear=%d new_vnum=%u old_vnum=%u new_lines=%lld old_lines=%lld",
						ch->GetPlayerID(), ch->GetName(), wearCell, item->GetVnum(), oldItem->GetVnum(),
						GetPlayerBotItemLineScore(item, ch), GetPlayerBotItemLineScore(oldItem, ch));
				continue;
			}

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

		if (PlayerBotEquipItem(ch, bestItem))
		{
			sys_log(0, "PLAYERBOT_AI: equipped upgrade pid=%u name=%s wear=%d old_vnum=%u new_vnum=%u old_score=%lld new_score=%lld",
					ch->GetPlayerID(), ch->GetName(), bestWearCell, oldVnum, newVnum, oldScore, bestScore);
			// The one line a player asks about first - "why is my top Sura
			// suddenly without her +8" - is the swap, so it goes to log.log
			// with what came off.
			char szHint[64];
			snprintf(szHint, sizeof(szHint), "slot %d zamiast %u", bestWearCell, oldVnum);
			LogManager::instance().ItemLog(ch, bestItem, "PLAYERBOT_EQUIP", szHint);
			// And the row is written now, not in seven minutes. The panel reads
			// player.item, while the db core keeps a changed item in its cache
			// for PLAYER_CACHE_FLUSH_SECONDS - so a bot that had just put a
			// shield on showed an empty shield slot in the panel for minutes
			// ("chyba na www klasycznym jest bug synchronizacji eq", Tieru,
			// 17 September; the row was there five minutes later). Both pieces:
			// the one worn and the one taken off.
			FlushPlayerBotItemRow(bestItem);
			FlushPlayerBotItemRow(bestOldItem);
			// What came off stays in the bag: a bot trades its spares, it does
			// not give them away.
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

	// The dagger ladder, for the Archer's stone weapon.
	DWORD GetPlayerBotProgressionStoneWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const DWORD familyBase = 1000;
		DWORD bestVnum = familyBase;
		int bestLevel = -1;
		for (int tier = 0; tier < 20; ++tier)
		{
			const DWORD candidateVnum = familyBase + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
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

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category);
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
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
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
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
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

	// "Full eq" as a player says it: every slot filled and nothing on the
	// progression ladder left to buy.
	bool IsPlayerBotFullyEquipped(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		static const BYTE slots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_WRIST, WEAR_FOOTS, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i)
			if (!ch->GetWear(slots[i]))
				return false;
		return !NeedsPlayerBotProgressionWeapon(ch) &&
				!NeedsPlayerBotProgressionArmor(ch) &&
				!NeedsPlayerBotProgressionShield(ch) &&
				!NeedsPlayerBotProgressionHelmet(ch) &&
				!NeedsPlayerBotProgressionBoots(ch) &&
				!NeedsPlayerBotProgressionWrist(ch) &&
				!NeedsPlayerBotProgressionNecklace(ch) &&
				!NeedsPlayerBotProgressionEarring(ch);
	}

	bool IsPlayerBotSpecialLevel30Weapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		return IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum());
	}

	// What a bot picks up and keeps for a player's crafting whatever the
	// merchant pays for it (PLAYERBOT_PICKUP_GOODS_VNUMS): the herbalist's
	// Gango Root and Tue Mushroom, the Crystal Earrings and the Ghost Face
	// Armour a smith takes further, every weapon of level sixty-five, the Zen
	// Bean and the Blood Pill. The loot filter never leaves these on a floor,
	// the junk rule never hands them to the merchant while a counter can sell
	// them, and the counter ranks them beside the materials.
	bool IsPlayerBotPickupGoods(LPITEM item)
	{
		if (!item || !item->GetProto())
			return false;
		const DWORD vnum = item->GetVnum();
		for (size_t i = 0; i < sizeof(PLAYERBOT_PICKUP_GOODS_VNUMS) / sizeof(PLAYERBOT_PICKUP_GOODS_VNUMS[0]); ++i)
			if (PLAYERBOT_PICKUP_GOODS_VNUMS[i] == vnum)
				return true;
		// Every herb of the herbalist's range, not the two the Biologist's rows
		// happen to want: they are the materials Baek-Go's board runs on.
		if (vnum >= PLAYERBOT_HERB_VNUM_FIRST && vnum <= PLAYERBOT_HERB_VNUM_LAST)
			return true;
		if ((vnum >= PLAYERBOT_PICKUP_EARRING_FIRST && vnum <= PLAYERBOT_PICKUP_EARRING_FIRST + 9) ||
				(vnum >= PLAYERBOT_PICKUP_ARMOUR_FIRST && vnum <= PLAYERBOT_PICKUP_ARMOUR_FIRST + 9))
			return true;
		return item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW &&
				(int)item->GetLevelLimit() == PLAYERBOT_PICKUP_WEAPON_LEVEL;
	}

	bool HasPlayerBotSpecialLevel30Weapon(LPCHARACTER ch, bool requireAverageDamage)
	{
		if (!ch)
			return false;
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
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

	// The proto of this piece's family at a higher plus, walking the refine
	// chain the blacksmith walks; the piece's own proto at or below its plus,
	// NULL past the top of the chain.
	const TItemTable* GetPlayerBotWeaponProtoAtPlus(LPITEM item, BYTE plus)
	{
		if (!item || !item->GetProto())
			return NULL;
		const TItemTable* proto = item->GetProto();
		BYTE at = item->GetRefineLevel();
		while (at < plus && proto && proto->dwRefinedVnum != 0)
		{
			proto = ITEM_MANAGER::instance().GetTable(proto->dwRefinedVnum);
			++at;
		}
		return at >= plus ? proto : NULL;
	}

	// A weapon whose lines are worth more than any blacksmith's odds: refined
	// under a scroll at every step or not at all (PLAYERBOT_WEAPON_SCROLL_ONLY_*).
	bool IsPlayerBotScrollOnlyWeapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		long avg = 0, skill = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (t == APPLY_NORMAL_HIT_DAMAGE_BONUS)
				avg += item->GetAttributeValue(i);
			else if (t == APPLY_SKILL_DAMAGE_BONUS)
				skill += item->GetAttributeValue(i);
		}
		return avg >= PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE || skill >= PLAYERBOT_WEAPON_SCROLL_ONLY_SKILL;
	}

	// The operator's anvil ceiling for a level-30 weapon, by its average line.
	// Below it the bot grinds at the blacksmith and takes the risk; at or above
	// it the step belongs to a scroll. A weapon over the scroll-only line never
	// reaches this at all - IsPlayerBotScrollOnlyWeapon answers first.
	int GetPlayerBotLevel30AnvilCeiling(long average)
	{
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_CHEAP)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_CHEAP;
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_GOOD)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_GOOD;
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_BETTER)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_BETTER;
		// From 37% the operator's scroll-only rule answers first. Community
		// patch 2's "from 34% under a Blessing Scroll from +3" is gone: Iwakura
		// took it back on 23 September (PLAYERBOT_LEVEL30_MIN_PLUS).
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_HIGH)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_HIGH;
		return 0;
	}

	// Whether this bot grinds THIS level-30 weapon of another class for sale
	// (PLAYERBOT_LEVEL30_SALE_REFINE_PERCENT). The id is the item's own, or an
	// offline counter line's, which is the same item. A weapon over the
	// scroll-only line never meets the plain anvil, so it is sold as it is.
	bool IsPlayerBotLevel30SaleDraw(LPCHARACTER ch, DWORD itemId)
	{
		const DWORD salt = ch->GetPlayerID() ^ (itemId * 2246822519U) ^ 0x53414c45U;
		return (int)(PlayerBotNavHash(salt) % 100U) < PLAYERBOT_LEVEL30_SALE_REFINE_PERCENT;
	}

	bool PlayerBotRefinesLevel30ForSale(LPCHARACTER ch, LPITEM item, DWORD itemId)
	{
		if (!ch || !item || !IsPlayerBotSpecialLevel30Weapon(item) || item->CanUsedBy(ch) ||
				IsPlayerBotScrollOnlyWeapon(item) || !IsPlayerBotLevel30SaleDraw(ch, itemId))
			return false;
		// Two of a family in the bag at most, and the rest are goods at once
		// (PLAYERBOT_HELD_FAMILY_LIMIT): what a bot grinds for sale is still
		// what it holds. A copy counts when it lies ahead of this one - or
		// always, for a line on a counter, which has no cell in the bag.
		const DWORD family = item->GetVnum() - (DWORD)std::max(0, item->GetRefineLevel());
		const bool inBag = item->GetWindow() == INVENTORY && ch->GetInventoryItem(item->GetCell()) == item;
		int ahead = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->GetID() == itemId || other->GetCell() != cell ||
					!IsPlayerBotSpecialLevel30Weapon(other) || other->CanUsedBy(ch) ||
					IsPlayerBotScrollOnlyWeapon(other) ||
					other->GetVnum() - (DWORD)std::max(0, other->GetRefineLevel()) != family ||
					!IsPlayerBotLevel30SaleDraw(ch, other->GetID()))
				continue;
			if (!inBag || cell < item->GetCell())
				++ahead;
		}
		return ahead < PLAYERBOT_HELD_FAMILY_LIMIT;
	}

	bool PlayerBotRefinesLevel30ForSale(LPCHARACTER ch, LPITEM item)
	{
		return item && PlayerBotRefinesLevel30ForSale(ch, item, item->GetID());
	}

	// How far: the operator's anvil ceiling for its average line.
	BYTE GetPlayerBotLevel30SaleTarget(LPITEM item)
	{
		return (BYTE)GetPlayerBotLevel30AnvilCeiling(SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS));
	}

	// What a level-30 weapon will hit for once ground to
	// PLAYERBOT_LEVEL30_PROJECT_PLUS, or as it is above that.
	long long GetPlayerBotLevel30Potential(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return 0;
		const BYTE plus = std::max<BYTE>(item->GetRefineLevel(), PLAYERBOT_LEVEL30_PROJECT_PLUS);
		const TItemTable* proto = GetPlayerBotWeaponProtoAtPlus(item, plus);
		return proto ? GetPlayerBotWeaponHitDamageAt(item, proto, ch) : 0;
	}

	// Iwakura's community patch 2, point 1: the level-30 weapon of this bot's
	// class - the Full Moon Sword, the Ostrze z Czerwonej Stali, the bow, the
	// dagger, the bell or the fan, whichever its build wields - is every bot's
	// from level thirty, whatever its level now: a bot of forty-two on an old
	// world without one is sent for it as surely as a bot of thirty.
	bool IsPlayerBotClassLevel30Weapon(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotSpecialLevel30Weapon(item) && IsPlayerBotWeapon(ch, item) &&
				item->CanUsedBy(ch);
	}

	// The class's own level-30 weapon, wearable now and under
	// PLAYERBOT_LEVEL30_MIN_PLUS: refined whatever its average, under a scroll
	// when there is one for the step and at the plain anvil when there is not
	// - no scroll-only hold and no ceiling below the floor.
	bool IsPlayerBotLevel30UnderFloor(LPCHARACTER ch, LPITEM item)
	{
		return IsPlayerBotClassLevel30Weapon(ch, item) && item->GetLevelLimit() <= ch->GetLevel() &&
				item->GetRefineLevel() < PLAYERBOT_LEVEL30_MIN_PLUS;
	}

	// The one it works on: the best of them by what it will hit for, the
	// weapon in the hand winning a tie.
	LPITEM FindPlayerBotClassLevel30Weapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM best = NULL;
		long long bestPotential = -1;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (IsPlayerBotClassLevel30Weapon(ch, worn))
		{
			best = worn;
			bestPotential = GetPlayerBotLevel30Potential(ch, worn);
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotClassLevel30Weapon(ch, item))
				continue;
			const long long potential = GetPlayerBotLevel30Potential(ch, item);
			if (potential > bestPotential)
			{
				best = item;
				bestPotential = potential;
			}
		}
		return best;
	}

	// Who has to go and get one: thirty or more, not a dropper (its time is
	// its farm's, IsPlayerBotDropper), and none in the hand or the bag.
	bool PlayerBotLacksClassLevel30Weapon(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() >= 30 && ch->IsItemLoaded() &&
				!IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) &&
				FindPlayerBotClassLevel30Weapon(ch) == NULL;
	}

	// What the purchase may cost: PLAYERBOT_LEVEL30_BUDGET_PERCENT of what the
	// bot holds over its reserve and the shopping floor. The anvil's share of
	// the same budget is kept by ManagePlayerBotRefining against the purse the
	// visit began with, so a purchase made on the way leaves it less.
	long long GetPlayerBotLevel30PurchaseCap(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		return spare > 0 ? spare * PLAYERBOT_LEVEL30_BUDGET_PERCENT / 100 : 0;
	}

	// The first plus, drawn by pid so a bot keeps the same one for life.
	BYTE GetPlayerBotLevel30FirstPlus(DWORD pid)
	{
		const int roll = (int)(PlayerBotNavHash(pid ^ 0x4c333046U) % 100U);
		int edge = PLAYERBOT_LEVEL30_FIRST_PLUS6_PERCENT;
		if (roll < edge)
			return 6;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS7_PERCENT;
		if (roll < edge)
			return 7;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS8_PERCENT;
		if (roll < edge)
			return 8;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS9_PERCENT;
		if (roll < edge)
			return 9;
		return 6;
	}

	// And the one it keeps trying for at every later visit.
	BYTE GetPlayerBotLevel30LongPlus(DWORD pid)
	{
		return std::max<BYTE>(GetPlayerBotLevel30FirstPlus(pid), PLAYERBOT_LEVEL30_LONG_TERM_PLUS);
	}

	// This visit's aim: the first plus while the weapon walked into town
	// under it - one visit's push, as far as the purse allows - and the
	// long-term plus from the visit after that.
	BYTE GetPlayerBotLevel30Aim(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		const DWORD pid = ch->GetPlayerID();
		const BYTE first = GetPlayerBotLevel30FirstPlus(pid);
		BYTE startPlus = item->GetRefineLevel();
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
		if (st != s_mapPlayerBotAIStates.end() && st->second.persona.bLevel30VisitStartPlus != 0xFF)
			startPlus = st->second.persona.bLevel30VisitStartPlus;
		return startPlus < first ? first : GetPlayerBotLevel30LongPlus(pid);
	}

	// The best average line of the class's level-30 weapons the bot holds.
	long GetPlayerBotBestClassLevel30Average(LPCHARACTER ch)
	{
		long best = -1;
		if (!ch || !ch->IsItemLoaded())
			return best;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (IsPlayerBotClassLevel30Weapon(ch, worn))
			best = SumPlayerBotItemLines(worn, APPLY_NORMAL_HIT_DAMAGE_BONUS);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotClassLevel30Weapon(ch, item))
				best = std::max(best, SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS));
		}
		return best;
	}

	// A counter's level-30 weapon of the bot's own class it has to buy: any,
	// when it has none - after a burn too ("w przypadku zniszczenia ... bot
	// ma obowiazek zakupic kolejna sztuke ... jesli pozwala na to jego
	// budzet"; the budget is CanPlayerBotPayForOffer's). Community patch 2
	// also made it buy every one of 34% or more that beat its own; Iwakura
	// took that back on 23 September, and a better one is bought only when
	// it would hit harder (IsPlayerBotBetterLevel30Offer).
	bool IsPlayerBotMandatedLevel30Offer(LPCHARACTER ch, LPITEM offer)
	{
		if (!IsPlayerBotClassLevel30Weapon(ch, offer) || offer->GetLevelLimit() > ch->GetLevel() ||
				ch->GetLevel() < 30 || IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())))
			return false;
		return GetPlayerBotBestClassLevel30Average(ch) < 0;
	}

	// What a level-30 weapon has to beat, and the one worth grinding for it.
	// toBeat is the best blow the bot has today - the weapon in its hand and
	// every weapon of its class in the bag it could put on - with a level-30
	// weapon it already wears counted at its potential, so a bot on a Full
	// Moon Sword +3 does not start a second one. The project is the bag's
	// level-30 weapon with the best potential, when that beats toBeat by
	// PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT. Nothing is stored: the bag is
	// read each time, so a sale, a burn or a better find changes the answer.
	struct TPlayerBotLevel30View
	{
		long long toBeat;
		LPITEM project;
		long long projectPotential;
		TPlayerBotLevel30View() : toBeat(0), project(NULL), projectPotential(0)
		{
		}
	};

	void ReadPlayerBotLevel30View(LPCHARACTER ch, TPlayerBotLevel30View& view)
	{
		view = TPlayerBotLevel30View();
		if (!ch || !ch->IsItemLoaded())
			return;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetType() == ITEM_WEAPON)
		{
			view.toBeat = GetPlayerBotWeaponHitDamage(worn, ch);
			if (IsPlayerBotSpecialLevel30Weapon(worn) && IsPlayerBotWeapon(ch, worn))
				view.toBeat = std::max(view.toBeat, GetPlayerBotLevel30Potential(ch, worn));
		}
		LPITEM best = NULL;
		long long bestPotential = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					!IsPlayerBotWeapon(ch, item) || item->GetLevelLimit() > ch->GetLevel())
				continue;
			if (!IsPlayerBotSpecialLevel30Weapon(item))
			{
				view.toBeat = std::max(view.toBeat, GetPlayerBotWeaponHitDamage(item, ch));
				continue;
			}
			const long long potential = GetPlayerBotLevel30Potential(ch, item);
			if (potential > bestPotential)
			{
				best = item;
				bestPotential = potential;
			}
		}
		if (best && bestPotential * 100 > view.toBeat * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT))
		{
			view.project = best;
			view.projectPotential = bestPotential;
		}
	}

	bool IsPlayerBotLevel30Project(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !IsPlayerBotSpecialLevel30Weapon(item) || item->IsEquipped())
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		return view.project == item;
	}

	// Whether this bot works THIS level-30 weapon at the anvil rather than
	// listing it. "Niech botom zalezy na takich broniach ... 65% do kowala,
	// reszta na rynek" - so the draw is per weapon, not per bot, and it is a
	// hash of the pair rather than a roll: a keeper that changed its mind would
	// put the same weapon up and take it back every service visit. The bag
	// still has a ceiling, because the bots hold 276 of these at +0 between
	// them and grinding all of them would be a purse emptied for nothing.
	bool PlayerBotKeepsLevel30ForAnvil(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotSpecialLevel30Weapon(item))
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		if (!item->IsEquipped() && item == view.project)
			return true;   // the project is kept whatever the draw says
		// So is the class's own (community patch 2, point 1).
		const LPITEM classOwn = FindPlayerBotClassLevel30Weapon(ch);
		if (item == classOwn)
			return true;
		// Another class's is never kept for this bot's anvil: it cannot wear
		// it, so it was kept to be kept - the grind for sale is its own rule
		// (PlayerBotRefinesLevel30ForSale), and the rest is counter goods at
		// once (community patch 2, point 9).
		if (!IsPlayerBotClassLevel30Weapon(ch, item))
			return false;
		const DWORD salt = ch->GetPlayerID() ^ (item->GetID() * 2654435761U);
		if ((int)(PlayerBotNavHash(salt) % 100U) >= PLAYERBOT_LEVEL30_KEEP_PERCENT)
			return false;
		// Count what the bag already works on, so a bot keeps a few and lists
		// the rest instead of hoarding every one it picks up: four at most, the
		// project and the class's own among them - Iwakura's four are pieces,
		// not reasons ("w ekwipunku i magazynie moga znajdowac sie
		// maksymalnie ... wyjatek: bron na 30. poziom dla klasy bota, ktorej
		// limit wynosi 4 sztuki", 23 September). A drawn copy counts only when
		// it lies ahead of this one in the bag; counting every other copy, as
		// this did, let none of five be kept and all five back the next time.
		const bool inBag = item->GetWindow() == INVENTORY && !item->IsEquipped();
		int kept = 0;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->IsEquipped() || !IsPlayerBotClassLevel30Weapon(ch, other))
				continue;
			if (other == view.project || other == classOwn)
			{
				++kept;
				continue;
			}
			const DWORD otherSalt = ch->GetPlayerID() ^ (other->GetID() * 2654435761U);
			if ((int)(PlayerBotNavHash(otherSalt) % 100U) < PLAYERBOT_LEVEL30_KEEP_PERCENT &&
					(!inBag || other->GetCell() < item->GetCell()))
				++kept;
		}
		return kept < PLAYERBOT_LEVEL30_KEEP_MAX;
	}


	// A level-30 weapon on somebody's counter is worth buying when its
	// potential beats both the best blow the bot has and its own project.
	bool IsPlayerBotBetterLevel30Offer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !IsPlayerBotSpecialLevel30Weapon(offer) || !IsPlayerBotWeapon(ch, offer) ||
				offer->GetLevelLimit() > ch->GetLevel())
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		const long long bar = std::max(view.toBeat, view.projectPotential);
		return GetPlayerBotLevel30Potential(ch, offer) * 100 >
				bar * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT);
	}

	// Before a walk to a market for one: level thirty, no project in the bag,
	// no finished one in the hand, and the level-30 weapon of the kind it
	// wields - at PLAYERBOT_LEVEL30_PROJECT_PLUS with a
	// PLAYERBOT_LEVEL30_HOPED_AVERAGE line - would beat what it has. A bot of
	// seventy on a level-65 weapon +9 has nothing to look for there.
	bool PlayerBotCouldUseLevel30Weapon(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 30 || !ch->IsItemLoaded())
			return false;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && !IsPlayerBotWeapon(ch, worn))
			return false;
		// A finished refine is not a finished weapon. This used to answer "no
		// thank you" to the whole market for any bot already wearing a special
		// level-30 weapon at +7, whatever was rolled on it - so a Full Moon
		// Sword +7 with nothing on it stopped its owner from ever looking at a
		// better one (audit of 17 September). The comparison below is the real
		// test and it is the stricter one where it should be: toBeat counts a
		// worn level-30 weapon AT ITS POTENTIAL, so a good +7 still refuses
		// every offer, and only a poor one lets the search go on.
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		if (view.project)
			return false;
		if (!worn)
			return true;
		static const DWORD families[] = { 290, 1170, 2150, 3210, 5110, 7160 };
		for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(families[i] + PLAYERBOT_LEVEL30_PROJECT_PLUS);
			if (!proto || proto->bType != ITEM_WEAPON || proto->bSubType != worn->GetSubType())
				continue;
			return GetPlayerBotWeaponHitDamageAt(NULL, proto, ch, PLAYERBOT_LEVEL30_HOPED_AVERAGE) * 100 >
					view.toBeat * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT);
		}
		return false;
	}

	// Defined in playerbot_persona.h, further down: M3's door by Iwakura's
	// document - a weapon at +6 and an armour at +5, the Mental Warrior with the
	// weapon alone.
	bool MeetsPlayerBotM3Survival(LPCHARACTER ch);

	bool HasPlayerBotM3ReadyEquipment(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		// The document's second tier starts at fifteen, as the old rule did.
		if (IsPlayerBotPersonaEnabled())
			return ch->GetLevel() >= 15 && MeetsPlayerBotM3Survival(ch);
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

	// How many Blessing and Dragon God scrolls the bag holds.
	int CountPlayerBotSafeRefineScrolls(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int scrolls = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held->GetCell() == cell && IsPlayerBotSafeRefineScroll(held->GetVnum()))
				scrolls += std::max<int>(1, held->GetCount());
		}
		return scrolls;
	}

	bool PlayerBotWantsShield(LPCHARACTER ch);

	// Iwakura's Perfectionist ranks the gear: the weapon first, the armour,
	// the shield, and the rest - helmet, boots, jewellery - only once those
	// three stand at +7 ("wielka trojca"). A bow or a two-hander has no shield
	// to wait for.
	bool IsPlayerBotBigThreeSlot(int wearCell)
	{
		return wearCell == WEAR_WEAPON || wearCell == WEAR_BODY || wearCell == WEAR_SHIELD;
	}

	bool IsPlayerBotBigThreeAtPlus(LPCHARACTER ch, BYTE plus)
	{
		if (!ch)
			return false;
		LPITEM weapon = GetPlayerBotHandWeapon(ch);
		LPITEM body = ch->GetWear(WEAR_BODY);
		if (!weapon || weapon->GetRefineLevel() < plus || !body || body->GetRefineLevel() < plus)
			return false;
		if (!PlayerBotWantsShield(ch))
			return true;
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		return shield && shield->GetRefineLevel() >= plus;
	}

	// The Perfectionist's rank of a piece for the anvil: 0 the weapon, 1 the
	// armour, 2 the shield, 3 the rest.
	BYTE GetPlayerBotPerfectionistRank(LPCHARACTER ch, LPITEM item)
	{
		const int cell = item ? item->FindEquipCell(ch) : -1;
		return cell == WEAR_WEAPON ? 0 : (cell == WEAR_BODY ? 1 : (cell == WEAR_SHIELD ? 2 : 3));
	}

	// How far a bot means to take a piece on the plain anvil, where a failed
	// step burns it. What a scroll in the bag changes is GetPlayerBotRefineTarget.
	BYTE GetPlayerBotRefineAmbitionDrawn(LPCHARACTER ch, LPITEM item);

	BYTE GetPlayerBotRefineAmbition(LPCHARACTER ch, LPITEM item)
	{
		const BYTE drawn = GetPlayerBotRefineAmbitionDrawn(ch, item);
		if (!ch || !item || drawn == 0 || !IsPlayerBotPersonaEnabled() || IsPlayerBotArcherStoneWeapon(ch, item))
			return drawn;
		// The Perfectionist aims the three at +7 whatever the draw - the Law of
		// Advancement asks the weapon for +7 - and lets the draw carry them to
		// +8 and +9 ("dazac do progu +7, a docelowo +8 i +9"). The rest waits
		// for the three.
		if (IsPlayerBotBigThreeSlot(item->FindEquipCell(ch)))
			return std::max<BYTE>(drawn, 7);
		return IsPlayerBotBigThreeAtPlus(ch, 7) ? drawn : 0;
	}

	BYTE GetPlayerBotRefineAmbitionDrawn(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		// The Archer's stone dagger is a tool, not a prize: carry it to +4, where
		// the steps are still 90% and a burn is rare, and stop - going for +6
		// without a scroll would burn it and leave the bot breaking stones with a
		// bow again. (A scroll still takes it higher: GetPlayerBotRefineTarget.)
		if (IsPlayerBotArcherStoneWeapon(ch, item))
			return PLAYERBOT_ARCHER_STONE_MIN_REFINE;

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

	BYTE GetPlayerBotRefineTarget(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		// A level-30 weapon of its own class in the hand, or the one it is
		// grinding, goes to +9 whatever the personality: that is what the
		// weapon is for. A scroll-only one gets there under scrolls or not at
		// all (CanPlayerBotAttemptRefineItem).
		// Under the personalities the class's own goes to this visit's aim
		// instead (community patch 2, point 1: GetPlayerBotLevel30Aim).
		if (IsPlayerBotSpecialLevel30Weapon(item) && IsPlayerBotWeapon(ch, item) &&
				(item->IsEquipped() || IsPlayerBotLevel30Project(ch, item) ||
					(IsPlayerBotPersonaEnabled() && item == FindPlayerBotClassLevel30Weapon(ch))))
			return IsPlayerBotPersonaEnabled() ? GetPlayerBotLevel30Aim(ch, item)
					: PLAYERBOT_SCROLL_REFINE_MAX_PLUS;
		// One of another class, ground for sale, as far as its ceiling.
		if (PlayerBotRefinesLevel30ForSale(ch, item))
			return GetPlayerBotLevel30SaleTarget(item);
		// Under Iwakura's personalities the helmet, the boots and the jewellery
		// wait for the weapon, the armour and the shield to stand at +7
		// (GetPlayerBotRefineAmbition) - and a scroll in the bag, which would
		// otherwise make a ladder of any piece, does not change that.
		if (IsPlayerBotPersonaEnabled() && !IsPlayerBotArcherStoneWeapon(ch, item) &&
				!IsPlayerBotBigThreeSlot(item->FindEquipCell(ch)) && !IsPlayerBotBigThreeAtPlus(ch, 7))
			return 0;
		// A scroll in the bag is a ladder to +9 for everybody: under it a
		// failure costs a level or nothing, never the piece, so the ambition -
		// which is about not burning what was earned - does not apply while
		// one is there. See PLAYERBOT_SCROLL_REFINE_MAX_PLUS.
		if (CountPlayerBotSafeRefineScrolls(ch) == 0)
			return GetPlayerBotRefineAmbition(ch, item);
		// The operator's SCROLL_FROM can put the ladder's first rung above
		// where a piece would climb by itself, and the steps under that rung
		// go to the plain anvil. So the ladder is the answer only for a piece
		// already on the rung or one whose own ambition carries it there; the
		// rest keep their ambition, or a bot content with +6 and scrolls kept
		// for +8 would try +7 unprotected for the sake of a scroll it may not
		// use. With no floor the first rung is +0 and every piece climbs.
		const int firstRung = GetPlayerBotScrollFromPlus() - 1;
		if ((int)item->GetRefineLevel() >= firstRung)
			return PLAYERBOT_SCROLL_REFINE_MAX_PLUS;
		const BYTE ambition = GetPlayerBotRefineAmbition(ch, item);
		return (int)ambition >= firstRung ? PLAYERBOT_SCROLL_REFINE_MAX_PLUS : ambition;
	}

	// What the village merchants actually stock, and what they charge for it.
	//
	// The three shops hold sixty-four rows between them in this world, and
	// nothing outside them can be bought by a player at any counter. The
	// progression ladder walks item_proto by vnum stride instead, so it named
	// pieces no shop has ever sold - and the purchase below simply created
	// them. What a player saw was a bot in a level-60 Mask of Fear bought for
	// twenty thousand yang at the armour merchant, with the log line to prove
	// it (jaksiezabic). A bot buys what a player could buy at the same counter,
	// at the same price; everything above that comes from drops, the counters
	// and the blacksmith, exactly as it does for a player.
	bool FindPlayerBotMerchantOffer(DWORD vnum, long long* priceOut)
	{
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				if (offers[k].vnum != vnum)
					continue;
				if (priceOut)
					*priceOut = offers[k].price > 0 ? offers[k].price : 0;
				return true;
			}
		}
		return false;
	}

	// The class's body-armour family base (11200/11400/11600/11800), so a
	// warrior never buys a shaman's robe. Shields and helmets are shared.
	DWORD GetPlayerBotArmorClassBase(LPCHARACTER ch)
	{
		if (!ch)
			return 11200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: return 11400;
			case JOB_SURA:     return 11600;
			case JOB_SHAMAN:   return 11800;
			default:           return 11200;
		}
	}

	// The best piece an NPC merchant actually stocks for a wear slot that
	// this bot's level and class can use. The progression ladder walks
	// item_proto by stride and names tiers no shop sells - body armour at
	// level 9, and everything from level 34 up - so a bot between two
	// stocked tiers, or above the top one, could never buy and walked the
	// world in an empty slot: a quarter of the cohort had no body armour
	// (Tieru, 13 September). This finds the highest stocked piece the bot
	// qualifies for, so the slot is filled and the blacksmith can raise it.
	// Whether this character's class and sex may wear a proto at all: the
	// anti-flag half of CItem::CanUsedBy and IsPlayerBotEquipmentCandidate, for
	// a piece the bot does not hold yet.
	bool IsPlayerBotProtoForCharacter(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!ch || !proto)
			return false;
		DWORD classFlag = 0;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  classFlag = ITEM_ANTIFLAG_WARRIOR; break;
			case JOB_ASSASSIN: classFlag = ITEM_ANTIFLAG_ASSASSIN; break;
			case JOB_SURA:     classFlag = ITEM_ANTIFLAG_SURA; break;
			case JOB_SHAMAN:   classFlag = ITEM_ANTIFLAG_SHAMAN; break;
			default: break;
		}
		if (classFlag != 0 && IS_SET(proto->dwAntiFlags, classFlag))
			return false;
		return !IS_SET(proto->dwAntiFlags,
				GET_SEX(ch) == SEX_MALE ? ITEM_ANTIFLAG_MALE : ITEM_ANTIFLAG_FEMALE);
	}

	// Whether some counter in this world holds a level-30 weapon this bot
	// could wield, read off the market ledger without walking to one.
	bool PlayerBotMarketHasClassLevel30Weapon(LPCHARACTER ch)
	{
		static const DWORD families[] = { 290, 1170, 2150, 3210, 5110, 7160 };
		for (size_t i = 0; ch && i < sizeof(families) / sizeof(families[0]); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(families[i]);
			if (!proto || proto->bType != ITEM_WEAPON || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType) ||
					!IsPlayerBotProtoForCharacter(ch, proto))
				continue;
			for (DWORD plus = 0; plus < 10; ++plus)
			{
				const TPlayerBotMarketLedgerEntry* entry = GetPlayerBotMarketLedgerEntry(families[i] + plus);
				if (entry && entry->dwSupplyUnits > 0)
					return true;
			}
		}
		return false;
	}

	DWORD FindPlayerBotBestMerchantSlotVnum(LPCHARACTER ch, int wearCell)
	{
		if (!ch)
			return 0;
		DWORD lo = 0, hi = 0;
		BYTE subtype = 0;
		switch (wearCell)
		{
			case WEAR_BODY:   lo = GetPlayerBotArmorClassBase(ch); hi = lo + 199; subtype = ARMOR_BODY; break;
			case WEAR_HEAD:   lo = 12000; hi = 12999; subtype = ARMOR_HEAD; break;
			case WEAR_SHIELD: lo = 13000; hi = 13999; subtype = ARMOR_SHIELD; break;
			case WEAR_FOOTS:  lo = 15000; hi = 15999; subtype = ARMOR_FOOTS; break;
			default: return 0;
		}
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		DWORD bestVnum = 0;
		int bestLevel = -1;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const DWORD vnum = offers[k].vnum;
				if (vnum < lo || vnum > hi)
					continue;
				TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
				if (!proto || proto->bType != ITEM_ARMOR || proto->bSubType != subtype)
					continue;
				// A merchant's helmets are every class's at once, told apart only
				// by the anti-flags. A sura was sold the warrior's Tradycyjny Helm,
				// could never wear it, and went without a helmet to level 74
				// (NaCoPaczysz, 14 September).
				if (!IsPlayerBotProtoForCharacter(ch, proto))
					continue;
				const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
				if (reqLevel > (int)ch->GetLevel())
					continue;
				if (reqLevel > bestLevel)
				{
					bestVnum = vnum;
					bestLevel = reqLevel;
				}
			}
		}
		return bestVnum;
	}

	// Fill an armour slot from the merchant with the best it stocks, unless
	// the bot already holds (worn or in the bag) a piece of at least that
	// level for the slot - so it never buys a second copy of a piece the
	// merchant cannot better, and never a downgrade.
	bool BuyPlayerBotBestMerchantSlotGear(LPCHARACTER ch, int wearCell, const char* category)
	{
		if (!ch)
			return false;
		const DWORD vnum = FindPlayerBotBestMerchantSlotVnum(ch, wearCell);
		if (vnum == 0 || HasPlayerBotProgressionGear(ch, vnum, wearCell))
			return false;
		return BuyPlayerBotProgressionGear(ch, vnum, category);
	}

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category)
	{
		if (!ch || vnum == 0)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) < 0)
			return false;

		long long price = 0;
		if (!FindPlayerBotMerchantOffer(vnum, &price))
			return false;
		if (price <= 0)
			price = proto->dwShopBuyPrice > 0 ? proto->dwShopBuyPrice : proto->dwGold;
		price = std::max<long long>(100, price);
		if (ch->GetGold() < price)
			return false;

		LPITEM item = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!item)
			return false;
		PlayerBotChangeGold(ch, -price);
		sys_log(0, "PLAYERBOT_GEAR: bought progression %s pid=%u name=%s vnum=%u required_level=%d price=%lld",
				category ? category : "gear", ch->GetPlayerID(), ch->GetName(), vnum,
				item->GetLevelLimit(), price);
		return true;
	}

	// Arrows this bot can nock now. A progression chest hands an archer the
	// next tier early - 8003 wants level forty, 8004 forty-five - and counting
	// those said "a hundred arrows, no need to buy" to a bot of thirty-four
	// whose bow had nothing to fire: PrepareWeapon failed on every tick, the
	// tick left through a town visit no frontier map can start, and twelve
	// archers stood at arrival points for twenty minutes at a time.
	bool IsPlayerBotUsableArrow(LPCHARACTER ch, LPITEM item)
	{
		return item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW &&
				item->GetCount() > 0 && item->GetLevelLimit() <= ch->GetLevel();
	}

	int CountPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int count = 0;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (IsPlayerBotUsableArrow(ch, worn))
			count += worn->GetCount();
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotUsableArrow(ch, item))
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotUsableArrow(ch, item) && PlayerBotEquipItem(ch, item, WEAR_ARROW))
				return true;
		}
		return false;
	}

	// Whether a shield slot is a slot this bot can fill at all: never with a
	// bow or a two-handed weapon in hand. Counting it as "missing" for an
	// archer made every archer critically short of town services for life -
	// sent out of M3 the moment it arrived and straight back by the weapon
	// hunt, fifteen seconds a round trip.
	bool PlayerBotWantsShield(LPCHARACTER ch)
	{
		LPITEM weapon = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return !(weapon && weapon->GetType() == ITEM_WEAPON &&
				(weapon->GetSubType() == WEAPON_BOW || weapon->GetSubType() == WEAPON_TWO_HANDED));
	}

	bool NeedsPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2)
			return false;
		return CountPlayerBotArrows(ch) < PLAYERBOT_ARROW_RESTOCK_THRESHOLD;
	}

	// Whether a dropper's archer, already past the need above, still fills its
	// quiver at the merchant (PLAYERBOT_DROPPER_ARROW_STOCK). The need stays
	// what ends a dungeon visit and holds a trip; this only decides how much is
	// bought once the bot stands at the counter.
	bool WantsPlayerBotArrowTopUp(LPCHARACTER ch)
	{
		// A trial archer too: the hundred kills of the battle-horse trial cost
		// more arrows than the hundred the restock threshold leaves, and every
		// trip back for them is a stay on the desert lost.
		return ch && ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2 &&
				(IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) ||
				 IsPlayerBotOnBattleHorseTrial(ch)) &&
				CountPlayerBotArrows(ch) < PLAYERBOT_DROPPER_ARROW_STOCK;
	}

	long long GetPlayerBotNpcPurchasePrice(const TItemTable* proto, int count)
	{
		if (!proto || count <= 0)
			return 0;
		if (IS_SET(proto->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			return proto->dwGold == 0 ? count : count / proto->dwGold;
		return (long long)proto->dwGold * count;
	}

	// Kamien Duszy, the stone for a weapon or armour socket. The vnum is
	// 28[grade][kind]: 28037 is Potwora +0, 28437 Potwora +4 - the grade is the
	// hundreds digit, and reading it from the last digit (as this once did)
	// made every stone a +0 and switched the grade rules off.
	int GetPlayerBotSoulStoneGrade(DWORD vnum) { return (int)((vnum / 100) % 10); }
	int GetPlayerBotSoulStoneKind(DWORD vnum) { return (int)(vnum % 100); }
	bool IsPlayerBotWeaponSoulStoneKind(int kind) { return kind >= 30 && kind <= 37; }
	bool IsPlayerBotArmorSoulStoneKind(int kind) { return kind >= 38 && kind <= 43; }

	// What a stone is worth to the hunting set, by Iwakura's list alone: his
	// PvE tier when he lets it into a socket and rates it neutral or better,
	// else nothing. Before his list this was a table of our own by school, and
	// it seated a +0 to +2 on anything under +6 - which his list forbids
	// ("Boty maja calkowity zakaz umieszczania Kamieni Duszy +0, +1 i +2 w
	// broniach i zbrojach") outside the operator's exception for weak pieces
	// (PLAYERBOT_SOUL_STONE_WEAK_GEAR_*). A stone of a banned grade takes the
	// best tier his list gives its kind, so the exception seats the kinds he
	// wants and not Magii or Powtorki. The class stones he keeps for a PvP
	// weapon, which no bot assembles, so they are never seated.
	int GetPlayerBotSoulStoneSeatTier(DWORD vnum)
	{
		int tier = GetPlayerBotSoulStoneTier(vnum, false);
		if (GetPlayerBotSoulStoneGrade(vnum) < PLAYERBOT_SOUL_STONE_MIN_GRADE)
		{
			const int kind = GetPlayerBotSoulStoneKind(vnum);
			for (size_t i = 0; i < sizeof(PLAYERBOT_SOUL_STONE_TIERS) / sizeof(PLAYERBOT_SOUL_STONE_TIERS[0]); ++i)
			{
				const TPlayerBotSoulStoneTier& row = PLAYERBOT_SOUL_STONE_TIERS[i];
				if (GetPlayerBotSoulStoneKind(row.dwVnum) == kind && !row.bPvpOnly)
					tier = std::max<int>(tier, row.bPve);
			}
		}
		return tier >= PLAYERBOT_SOUL_STONE_MIN_PVE_TIER ? tier : 0;
	}

	// The operator's weak piece: level 21 or less and +6 or less.
	bool IsPlayerBotWeakSoulStoneGear(LPITEM gear)
	{
		return gear && gear->GetLevelLimit() <= PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_LEVEL &&
				gear->GetRefineLevel() <= PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_REFINE;
	}

	// Whether the worn piece for this kind of stone has a socket open for it
	// and does not hold the same kind already (the engine refuses a second).
	bool FindPlayerBotSoulStoneSocket(LPCHARACTER ch, int kind, DWORD stoneValue5, LPITEM* outGear, int* outSocket)
	{
		if (!ch)
			return false;
		LPITEM gear = IsPlayerBotWeaponSoulStoneKind(kind) ? ch->GetWear(WEAR_WEAPON)
				: (IsPlayerBotArmorSoulStoneKind(kind) ? ch->GetWear(WEAR_BODY) : NULL);
		if (!gear)
			return false;
		int openSocket = -1;
		for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
		{
			const DWORD inSocket = (DWORD)gear->GetSocket(socketIdx);
			if (inSocket == 1 && openSocket < 0)
				openSocket = socketIdx;
			if (inSocket <= 2 || inSocket == PLAYERBOT_BROKEN_SOUL_STONE_VNUM)
				continue;
			const TItemTable* seated = ITEM_MANAGER::instance().GetTable(inSocket);
			if (seated && (DWORD)seated->alValues[5] == stoneValue5)
				return false;
		}
		if (openSocket < 0)
			return false;
		if (outGear)
			*outGear = gear;
		if (outSocket)
			*outSocket = openSocket;
		return true;
	}

	// A seating is a 30% roll, and the other 70% welds a cracked stone into
	// the socket for good. A stone under Iwakura's lowest grade goes only into
	// the operator's weak piece; a +3 or +4 is never spent on a piece below
	// +6, and on a +8 or +9 the socket waits for the +4.
	bool ShouldPlayerBotSeatSoulStone(LPITEM gear, int grade)
	{
		if (!gear)
			return false;
		if (grade < PLAYERBOT_SOUL_STONE_MIN_GRADE)
			return grade >= PLAYERBOT_SOUL_STONE_WEAK_MIN_GRADE &&
					IsPlayerBotWeakSoulStoneGear(gear);
		const int refine = gear->GetRefineLevel();
		if (refine >= PLAYERBOT_SOUL_STONE_TOP_GEAR_REFINE)
			return grade >= PLAYERBOT_SOUL_STONE_TOP_GEAR_MIN_GRADE;
		return refine >= PLAYERBOT_SOUL_STONE_MIN_GEAR_REFINE;
	}

	// Whether this stone would go into a socket of what the bot wears now:
	// what the counter must not sell.
	bool CanPlayerBotSeatSoulStone(LPCHARACTER ch, DWORD vnum, DWORD stoneValue5)
	{
		const int kind = GetPlayerBotSoulStoneKind(vnum);
		if (GetPlayerBotSoulStoneSeatTier(vnum) <= 0)
			return false;
		LPITEM gear = NULL;
		int socket = -1;
		return FindPlayerBotSoulStoneSocket(ch, kind, stoneValue5, &gear, &socket) &&
				ShouldPlayerBotSeatSoulStone(gear, GetPlayerBotSoulStoneGrade(vnum));
	}

	// Whether the bot would buy this stone off a counter: one it would seat,
	// of Iwakura's grades - the weak piece's +0..+2 are for what drops.
	bool WantsPlayerBotSoulStone(LPCHARACTER ch, DWORD vnum, DWORD stoneValue5)
	{
		return GetPlayerBotSoulStoneGrade(vnum) >= PLAYERBOT_SOUL_STONE_MIN_GRADE &&
				CanPlayerBotSeatSoulStone(ch, vnum, stoneValue5);
	}

	// Does this bot have a socket that a stone worth having could still fill?
	// The market question, asked before a trip.
	bool PlayerBotHasOpenSoulStoneSocket(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const BYTE slots[2] = { WEAR_WEAPON, WEAR_BODY };
		for (int s = 0; s < 2; ++s)
		{
			LPITEM gear = ch->GetWear(slots[s]);
			if (!gear || gear->GetRefineLevel() < PLAYERBOT_SOUL_STONE_MIN_GEAR_REFINE)
				continue;
			for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
				if ((DWORD)gear->GetSocket(socketIdx) == 1)
					return true;
		}
		return false;
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

	// Is this piece one the bot could put on right now, and better than what it
	// already wears there?
	//
	// The stall listed any weapon or armour whose slot was already filled, which
	// reads as "this is a spare" and nearly always is. A player handing a bot a
	// pair of +9 boots does not fill an empty slot, it beats a full one - and the
	// counter got there first, because the private shop pass runs at the top of
	// the tick and the equipment pass three hundred lines below it. Reported from
	// the Discord by three people in one afternoon, each of whom had just given a
	// bot something good and watched it go on sale at the top of the counter: a
	// spare at +6 or better is the highest-scoring thing a stall can carry.
	//
	// "Could put on right now" is the engine's own CanEquipNow, so a piece the
	// bot has not grown into is not held off the market on a promise: a level-30
	// sword in the bag of a bot of five is goods, and stays goods.
	bool IsPlayerBotWearableUpgrade(LPCHARACTER ch, LPITEM item, WORD cell)
	{
		if (!IsPlayerBotEquipmentCandidate(ch, item))
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		LPITEM worn = ch->GetWear((BYTE)wearCell);
		if (worn && IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE))
			return false;
		if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
			return false;
		return !worn || GetPlayerBotEquipmentScore(item, ch) >
				GetPlayerBotEquipmentScore(worn, ch);
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		// Only a pass that frees a cell is worth its moves. A bot drinks from
		// the first stack of a kind, so a pass that topped the first stack up
		// from the last one ran again as soon as a few potions had gone: 29 000
		// passes an hour on one core of the test world, "freed_stacks=0" on
		// nearly every one, and every unit moved a save for the db core (23
		// September). Filling from the front leaves the fewest stacks there
		// can be, so a kind is poured only while its units would fit in fewer
		// stacks than it has from this one on.
		for (WORD destinationCell = 0; destinationCell < PLAYERBOT_BAG_CELLS; ++destinationCell)
		{
			LPITEM destination = ch->GetInventoryItem(destinationCell);
			if (!destination ||
					GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE)
				continue;
			const DWORD maxStack = (DWORD)std::max(1, PlayerBotMaxStack(destination));
			if ((DWORD)destination->GetCount() >= maxStack)
				continue;
			DWORD units = (DWORD)destination->GetCount(), stacks = 1;
			for (WORD cell = destinationCell + 1; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (!CanMergePlayerBotPotionStacks(destination, other))
					continue;
				units += (DWORD)other->GetCount();
				++stacks;
			}
			if (stacks <= (units + maxStack - 1) / maxStack)
				continue;
			for (WORD sourceCell = destinationCell + 1;
					sourceCell < PLAYERBOT_BAG_CELLS && (DWORD)destination->GetCount() < maxStack;
					++sourceCell)
			{
				LPITEM source = ch->GetInventoryItem(sourceCell);
				if (!CanMergePlayerBotPotionStacks(destination, source))
					continue;
				const DWORD sourceCount = source->GetCount();
				const DWORD transfer = std::min<DWORD>(maxStack - (DWORD)destination->GetCount(), sourceCount);
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
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && excess > 0; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != saleOrder[order])
						continue;
					const DWORD unitPrice = GetPlayerBotNpcSellUnitPrice(item);
					if (unitPrice == 0)
						continue;
					const DWORD count = std::min<DWORD>(excess, item->GetCount());
					item->SetCount(item->GetCount() - count);
					PlayerBotChangeGold(ch, (long long)unitPrice * count);
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
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
				PlayerBotChangeGold(ch, (long long)price * count);
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
		if (!NeedsPlayerBotArrows(ch) && !WantsPlayerBotArrowTopUp(ch))
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_WOODEN_ARROW_VNUM);
		if (!proto)
			return false;

		const long long smallPrice = GetPlayerBotNpcPurchasePrice(
				proto, PLAYERBOT_ARROW_SMALL_BUNDLE);
		if (ch->GetGold() < smallPrice)
		{
			// Running out is worth selling potions for; a top-up is not.
			if (!NeedsPlayerBotArrows(ch))
				return false;
			RaisePlayerBotEmergencyGold(ch, smallPrice, "arrows");
		}

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
		// AutoGiveItem hands the item back even when it had nowhere to put it:
		// with no free cell the bundle goes on the ground at the bot's feet, the
		// bot pays, still "needs arrows", and buys again on the next pass - a
		// market square carpeted in Wooden Arrows, twenty purchases an hour per
		// archer. The junk sale has already run by now; a bag still full holds
		// things worth keeping, and the arrows wait for the next visit.
		if (ch->GetEmptyInventory(1) < 0)
		{
			sys_log(0, "PLAYERBOT_GEAR: no room for arrows pid=%u name=%s arrows=%d",
					ch->GetPlayerID(), ch->GetName(), CountPlayerBotArrows(ch));
			return false;
		}

		LPITEM arrows = ch->AutoGiveItem(
				PLAYERBOT_WOODEN_ARROW_VNUM, bundle, -1, false);
		if (!arrows)
			return false;
		PlayerBotChangeGold(ch, -price);
		const bool equipped = EnsurePlayerBotArrowsEquipped(ch);
		sys_log(0, "PLAYERBOT_GEAR: bought wooden arrows pid=%u name=%s vnum=%u count=%d price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_WOODEN_ARROW_VNUM,
				bundle, price, equipped ? 1 : 0);
		// A dropper's archer fills the quiver on the same visit, a bundle at a
		// time, for as long as the yang and the cells allow.
		if (WantsPlayerBotArrowTopUp(ch))
			BuyPlayerBotArrowsAtMerchant(ch);
		return true;
	}

	bool EquipFirstAvailablePlayerBotWeapon(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotWeapon(ch, item))
				continue;

			const DWORD vnum = item->GetVnum();
			if (PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)) && PlayerBotEquipItem(ch, item))
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

		// A weapon the bag already holds is put on, not bought a second time.
		// The weapon merchant bought for any empty hand, and a new bot's hand
		// is empty with its starter weapon or a chest's in the bag - twelve
		// purchases in five minutes on a world started that morning, and bots
		// of level one carrying four swords (Iwakura, 23 September). One the
		// engine refuses only for the moment (the second and a half after a
		// blow) is worn on the next try, so it stops the purchase too.
		if (EquipFirstAvailablePlayerBotWeapon(ch))
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (IsPlayerBotWeapon(ch, held) && held->CanUsedBy(ch) &&
					held->GetLevelLimit() <= ch->GetLevel())
				return false;
		}

		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		const long long price = GetPlayerBotEmergencyWeaponPrice(ch);
		if (vnum == 0)
			return false;
		// Room first: AutoGiveItem puts what the bag cannot take on the ground
		// and returns it, and a weapon equipped from the ground is in the slot
		// and on the ground at once (IsPlayerBotWornItemSound).
		const TItemTable* weaponProto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!weaponProto || ch->GetEmptyInventory(std::max(1, (int)weaponProto->bSize)) < 0)
			return false;
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, "weapon");
		if (ch->GetGold() < price)
			return false;

		LPITEM weapon = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!weapon)
			return false;
		if (weapon->GetOwner() != ch || weapon->GetWindow() != INVENTORY)
		{
			sys_err("PLAYERBOT_AI: emergency weapon did not reach the bag pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), vnum);
			return false;
		}

		PlayerBotChangeGold(ch, -price);
		const bool equipped = PlayerBotEquipItem(ch, weapon);

		sys_log(0, "PLAYERBOT_AI: bought emergency weapon pid=%u name=%s vnum=%u price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), vnum, price, equipped ? 1 : 0);
		return equipped;
	}

	bool PrepareWeapon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		LPITEM equippedWeapon = ch->GetWear(WEAR_WEAPON);
		// PlayerBotWeaponFitsNow and not IsPlayerBotWeapon: the Archer's dagger
		// on a stone is the right weapon for the moment, not a profession
		// mismatch to be taken off.
		if (equippedWeapon && PlayerBotWeaponFitsNow(ch, state, equippedWeapon))
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
			// This runs every second while the bot has no weapon, so a box the
			// engine refused waits out the chest pass's retry clock instead of
			// being asked for again on the next one.
			if (IsPlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow))
				continue;

			sys_log(0, "PLAYERBOT_AI: opening weapon recovery chest pid=%u name=%s vnum=%u cell=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, cell);
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
			{
				NotePlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow);
				continue;
			}
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

	// Whether the bag can take everything a chest may hand out, judged the way
	// the engine places items: a piece needs its height in one column of one
	// page, a stackable merges into a stack of the same vnum first, and every
	// reward is placed on a copy of the grid before the next one is asked.
	//
	// The engine gives a chest's rewards one by one through AutoGiveItem, which
	// puts what does not fit on the ground and still reports success - so
	// "GetEmptyInventory(3) >= 0" before the chest let a full bag spill the
	// rest of the set (D01 of the 10 September audit: "przedmioty ze skrzyn
	// wypadaja na ziemie"). The set is the group's own list: for a PCT group
	// every line may come at once (the starter chests are that), for the others
	// exactly one line does, so the room asked for is the largest line. mt2009
	// exposes the type (GetGroupType, added by playerbotify.py) and the lines;
	// r40250 exposes neither, so that line keeps the old five-cell heuristic,
	// as does a group the manager does not know.
	bool PlayerBotBagTakesGroup(LPCHARACTER ch, DWORD dwGroupVnum, int& iCellsNeeded)
	{
		iCellsNeeded = 0;
		if (!ch || !ch->IsItemLoaded())
			return false;
		const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupVnum);
#if !defined(PLAYERBOT_ENGINE_MT2009)
		// r40250's CSpecialItemGroup has neither GetItems() nor a size, so its
		// lines cannot be walked from here; that line keeps the old heuristic.
		pGroup = NULL;
#endif
		if (!pGroup)
		{
			// The grid, not the pointers: see CountPlayerBotFreeInventoryCells
			// (defined later in the include order, hence the loop repeated).
			int freeCells = 0;
			for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, (WORD)cell), 1))
					++freeCells;
			return freeCells >= PLAYERBOT_CHEST_FREE_CELLS && ch->GetEmptyInventory(3) >= 0;
		}
#if defined(PLAYERBOT_ENGINE_MT2009)

		bool occupied[PLAYERBOT_BAG_CELLS];
		std::map<DWORD, int> headroom; // vnum -> units a stack of it can still take
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			occupied[cell] = false;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (!held || held->GetCell() != cell)
				continue;
			const int size = std::max<int>(1, held->GetSize());
			for (int k = 0; k < size; ++k)
				if (cell + k * 5 < PLAYERBOT_BAG_CELLS)
					occupied[cell + k * 5] = true;
			if (held->IsStackable() && held->GetCount() < ITEM_MAX_COUNT)
				headroom[held->GetVnum()] += ITEM_MAX_COUNT - held->GetCount();
		}

		const std::vector<CSpecialItemGroup::CSpecialItemInfo> lines = pGroup->GetItems();
		const bool everyLine = pGroup->GetGroupType() == CSpecialItemGroup::PCT;
		// The rewards to place: (size, units) per line, or one line - the one
		// that needs the most cells - when only one comes.
		std::vector<std::pair<int, int> > rewards;
		int biggestCells = -1;
		std::pair<int, int> biggest(0, 0);
		for (size_t i = 0; i < lines.size(); ++i)
		{
			const TItemTable* table = ITEM_MANAGER::instance().GetTable(lines[i].vnum);
			if (!table)
				continue;
			int units = std::max(1, lines[i].count);
			const bool stackable = IS_SET(table->dwFlags, ITEM_FLAG_STACKABLE);
			if (stackable)
			{
				std::map<DWORD, int>::iterator room = headroom.find(lines[i].vnum);
				if (room != headroom.end())
				{
					const int merged = std::min(room->second, units);
					units -= merged;
					if (everyLine)
						room->second -= merged;
				}
				if (units <= 0)
					continue;
				units = (units + ITEM_MAX_COUNT - 1) / ITEM_MAX_COUNT; // stacks to place
			}
			const int size = std::max<int>(1, table->bSize);
			if (everyLine)
				rewards.push_back(std::make_pair(size, units));
			else if (size * units > biggestCells)
			{
				biggestCells = size * units;
				biggest = std::make_pair(size, units);
			}
		}
		if (!everyLine && biggestCells > 0)
			rewards.push_back(biggest);

		const int rowsPerPage = PLAYERBOT_INVENTORY_PAGE_SIZE / 5;
		for (size_t r = 0; r < rewards.size(); ++r)
		{
			const int size = rewards[r].first;
			for (int n = 0; n < rewards[r].second; ++n)
			{
				int placed = -1;
				for (int cell = 0; cell < PLAYERBOT_BAG_CELLS && placed < 0; ++cell)
				{
					const int row = (cell % PLAYERBOT_INVENTORY_PAGE_SIZE) / 5;
					if (row + size > rowsPerPage)
						continue;
					bool free = true;
					for (int k = 0; k < size && free; ++k)
						if (occupied[cell + k * 5])
							free = false;
					if (free)
						placed = cell;
				}
				if (placed < 0)
					return false;
				for (int k = 0; k < size; ++k)
					occupied[placed + k * 5] = true;
				iCellsNeeded += size;
			}
		}
		return true;
#endif
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
			// A box the engine refused this bot waits out the chest pass's retry
			// clock. This pass comes back every ten to fifteen seconds and used to
			// remember nothing, so the lv60 chest's Skrzynia Mistrza II, whose
			// group the mt2009 share did not have, was asked for by every bot of
			// seventy holding one: 258 175 lines of "cannot find special item
			// group 50194" in thirty hours on one core, 560 a minute by the end.
			if (IsPlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow))
				continue;

			// The whole set or nothing: a chest whose rewards would spill stays
			// closed until the town errand for a full bag has made room.
			int cellsNeeded = 0;
			if (!PlayerBotBagTakesGroup(ch, chestVnum, cellsNeeded))
			{
				if (dwNow >= state.dwNextBagFullLogTime)
				{
					state.dwNextBagFullLogTime = dwNow + 60000;
					sys_log(0, "PLAYERBOT_GEAR: chest waits for room pid=%u name=%s vnum=%u level=%u",
							ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel());
				}
				continue;
			}

			// A giftbox opens only into a free column of three (UseItemEx asks
			// GetEmptyInventory(3)), and 16 of the 23 bags looked at that refused
			// Skrzynia Mistrza II had free cells and no such column. The chest pass
			// makes one; this pass asks for it the same way, and uses the chest by
			// its own cell afterwards, since a single-cell chest may be what moved.
			if (ch->GetEmptyInventory(3) < 0)
				FreePlayerBotGiftboxColumn(ch);
			if (!ch->UseItem(TItemPos(INVENTORY, item->GetCell())))
			{
				// group=0 is a box the share gives no group, room3=0 the engine's
				// own refusal for want of a three-cell space; anything else was a
				// refusal of the moment (a busy action, the item-use pulse).
				NotePlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow);
				PlayerBotLogThrottled("progression_chest_refused", dwNow,
						"PLAYERBOT_CHEST: progression chest refused pid=%u name=%s vnum=%u level=%u group=%d room3=%d",
						ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel(),
						ITEM_MANAGER::instance().GetSpecialItemGroup(chestVnum) ? 1 : 0,
						ch->GetEmptyInventory(3) >= 0 ? 1 : 0);
				continue;
			}
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

		// A duel is fought without drinking. The operator's rule, and the only
		// thing that makes a bot-against-bot fight worth watching: two bots with
		// full bags of red potions do not have a fight, they have an endurance
		// test. Asked before the clock below, so a duel does not spend the
		// bot's next potion attempt either.
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return false;

		if (dwNow < state.dwNextPotionTime)
			return false;

		state.dwNextPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27051 is the beginner red potion supplied by the level-one chest.
		// 71018 and 71020 are the chest's Blessings of Life and of the Dragon: a
		// full restore each, drunk last, when the ordinary reds have run out.
		// ... and the grilled fish that heal: Crucian, Big Crucian, Tenchi.
		const DWORD redPotionVnums[] = { 27051, 27001, 27002, 27003, 71018, 71020, 27863, 27865, 27875 };
		for (size_t potionIndex = 0; potionIndex < sizeof(redPotionVnums) / sizeof(redPotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
		const DWORD bluePotionVnums[] = { 27052, 27004, 27005, 27006, 71020, 27864, 27876 };
		for (size_t potionIndex = 0; potionIndex < sizeof(bluePotionVnums) / sizeof(bluePotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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

	// Every bot wears the third hand, and keeps wearing it.
	//
	// Without it a kill's yang lands on the ground as coin piles and the bot has
	// to walk to each one; with it the engine credits the money on the spot
	// (CHARACTER::RewardGold, IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT)). The
	// bot spends its ticks fighting rather than fetching, and the hunting
	// grounds stop filling with yang nobody collects.
	//
	// Nothing here is a purchase: the item is made, worn, and its wear clock
	// wound back up before it can run out. See PLAYERBOT_THIRD_HAND_VNUM for
	// why that clock has to be touched at all, and why it is 72018 and not the
	// 71010 an item shop would sell.
	void ManagePlayerBotThirdHand(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return;
		if (dwNow < state.dwNextThirdHandTime)
			return;
		state.dwNextThirdHandTime = dwNow + PLAYERBOT_THIRD_HAND_INTERVAL;

		// Since patch 0010 every kill's yang goes straight to the purse, for
		// bots and players alike, so the Third Hand only takes a slot ("da sie
		// dodac status trzeciej reki bez zajmowania slota w eq?"). Whatever a
		// bot still wears or carries of the group is taken away.
		for (int pass = 0; pass < 2; ++pass)
		{
			LPITEM hand = NULL;
			for (BYTE wear = 0; wear < WEAR_MAX_NUM && !hand; ++wear)
			{
				LPITEM worn = ch->GetWear(wear);
				if (worn && worn->GetVnum() >= PLAYERBOT_THIRD_HAND_VNUM_FIRST &&
						worn->GetVnum() <= PLAYERBOT_THIRD_HAND_VNUM)
					hand = worn;
			}
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !hand; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (item && item->GetVnum() >= PLAYERBOT_THIRD_HAND_VNUM_FIRST &&
						item->GetVnum() <= PLAYERBOT_THIRD_HAND_VNUM && !item->isLocked())
					hand = item;
			}
			if (!hand)
				return;
			sys_log(0, "PLAYERBOT_GEAR: third hand retired pid=%u name=%s vnum=%u worn=%d",
					ch->GetPlayerID(), ch->GetName(), hand->GetVnum(), hand->IsEquipped() ? 1 : 0);
			ITEM_MANAGER::instance().RemoveItem(hand, "PLAYERBOT_THIRD_HAND_RETIRED");
		}
	}
}

#endif
