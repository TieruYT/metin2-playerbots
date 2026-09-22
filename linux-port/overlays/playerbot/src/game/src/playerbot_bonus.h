#ifndef __INC_METIN2_PLAYERBOT_BONUS_H__
#define __INC_METIN2_PLAYERBOT_BONUS_H__

// The bonus lines on a worn item, and what a bot is willing to spend to change
// them.
//
// Gear is only half a bot's power and these are the other half: a
// level-appropriate weapon rolled into five resistances is genuinely worse
// than the plain one it replaced. Two engine items do the work and neither can
// be dropped, sold, traded or shopped, so there is no market to walk to - a bot
// pays for one the way it pays for its stall.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_economy.h - it uses that file's idea of what
// a bot is short of - and before playerbot_town.h, which is where a bot decides
// to go and do this.

namespace
{
	// --- Bonus lines ---------------------------------------------------------
	// Gear is only half a bot's power; the four bonus lines are the other half. A
	// level-appropriate weapon rolled into four resistances is genuinely worse
	// than the one it replaced, and until now nothing ever looked at them.
	//
	// The scoring below is deliberately coarse. It exists to tell "worth keeping"
	// from "roll it again", not to model the damage formula: every line is scored
	// as points-per-typical-roll so that a +2000 HP line and a +15 attack line
	// can be compared at all.
	bool IsPlayerBotCaster(LPCHARACTER ch)
	{
		return ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
	}

	bool IsPlayerBotOffensiveSlot(BYTE wearCell)
	{
		return wearCell == WEAR_WEAPON;
	}

	// What one line is worth to this bot, in points per typical roll.
	//
	// Three of the world's own tables decide it and none of it is taste. A
	// player sent in a spreadsheet of "which bonus is worth having on which
	// piece, on which map"; this is that spreadsheet checked against our files,
	// which disagree with it in several places worth knowing about.
	//
	//  * `player.item_attr` says what may roll where and how high. Health does
	//    not roll on a helmet or an earring, critical does not roll on a wrist
	//    or an earring, attack value rolls on a body and nowhere else, and
	//    block only on a shield. That is why the finishing rule below is per
	//    slot: the old one asked a helmet for health and attack value, so no
	//    helmet in the world could ever be finished and every one of them was
	//    rerolled for as long as its owner had gold.
	//  * `battle.cpp` says what a line does. BLOCK stops a melee hit outright
	//    and 73% of this world's monsters are melee. DODGE and RESIST_BOW only
	//    answer a ranged attacker, and ranged monsters are 0-24% of a map. An
	//    elemental resistance is applied at thirty percent of its own number,
	//    so fifteen points of it is four and a half percent. And the five
	//    weapon-type resistances never fire against a monster at all: that
	//    branch reads the attacker's WEAR_WEAPON and a monster wears none, so
	//    "odpornosc na miecze" is a line for fighting players.
	//  * `tools/analyse_map_races.py` says what each map is made of, which is
	//    what makes a race line worth having - or not, on the three maps whose
	//    monsters no item can be strong against.
	//
	// The scoring stays coarse on purpose: it tells "worth keeping" from "roll
	// it again", it does not model the damage formula.
	int ScorePlayerBotBonusLineRaw(LPCHARACTER ch, BYTE wearCell, BYTE type, short value)
	{
		// A negative roll exists (movement speed on some sets) and is worth less
		// than nothing, so it must not be able to prop up a bad item's total.
		if (value <= 0)
			return 0;

		const bool bCaster = IsPlayerBotCaster(ch);

		switch (type)
		{
			// The two damage lines are not the same line for every character,
			// and weighting them alike had one class rerolling away the only
			// bonus that does anything for it.
			//
			// Measured over every attribute on this world's items: average
			// damage rolls up to 46 and skill damage only to 18. At twelve and
			// ten a maximum average roll scored 460 against a maximum skill
			// roll's 216, so average damage won by more than two to one - for
			// everybody, a Shaman included, whose damage is very nearly all
			// skills. A caster that rolled the best skill-damage line in the
			// game would throw it away on the next pass.
			//
			// So the weights are per build, and chosen against those two
			// ceilings rather than by feel: a caster's best skill roll (18 x 30
			// = 540) beats its best average roll (46 x 6 = 276), and for
			// everyone else the order stays as it was.
			case APPLY_SKILL_DAMAGE_BONUS:
				return bCaster ? value * 30 : value * 12;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
				return bCaster ? value * 6 : value * 10;

			// "Silny przeciwko Orkom" and its five siblings - the line the
			// player's table is really about, and the one this pass used to
			// throw away. It fell through to the default and was worth its own
			// number, so twenty percent against orcs scored twenty points and
			// lost to six points of movement speed, while the equipment pass was
			// paying twelve thousand for the same line. The bot bought the
			// shield for it and rerolled it off at the next blacksmith.
			//
			// It multiplies the whole attack - normal hits and skills alike -
			// against every monster of that race, so where the map is that race
			// it beats any other line a shield or an earring can roll. Where it
			// is not, it is worth keeping only because bots change maps.
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:
			case APPLY_ATTBONUS_HUMAN:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			{
				int racePercent = 0;
				const int race = GetPlayerBotFightingRace(ch, &racePercent);
				const int onMap = (race != PLAYERBOT_RACE_NONE &&
						GetPlayerBotRaceApplyType(race) == type)
						? PLAYERBOT_BONUS_RACE_ON_MAP * racePercent / 100 : 0;
				return value * std::max(onMap, PLAYERBOT_BONUS_RACE_OFF_MAP);
			}
			// Worth having and worth nothing to chase: "Silny przeciwko
			// Potworom" raises damage against every monster and against Metin
			// stones, which is the whole of what a bot ever fights. It is not in
			// player.item_attr at all, so no reroll can produce one;
			// item_attr_rare carries it at ten, and the pieces that have it keep
			// it.
			case APPLY_ATTBONUS_MONSTER:        return value * 14;

			case APPLY_CRITICAL_PCT:            return value * 10;
			case APPLY_PENETRATE_PCT:           return value * 10;
			// Attack speed is a straight multiplier on everything a bot does and
			// it rolls only to eight, so a maximum roll is eight percent more of
			// every swing, every shot and every skill. It was worth sixty-four
			// points, less than a mediocre health roll.
			case APPLY_ATT_SPEED:               return value * 15;
			// Life stolen per hit is what keeps a grinder off the potions and
			// out of town, which is the errand that costs a bot the most time.
			case APPLY_STEAL_HP:                return value * 12;
			// Rolls on a body and nowhere else, to fifty.
			case APPLY_ATT_GRADE_BONUS:         return value * 5;
			case APPLY_CAST_SPEED:              return bCaster ? value * 8 : value;
			case APPLY_MAX_HP_PCT:              return value * 15;
			case APPLY_DEF_GRADE_BONUS:
				return IsPlayerBotOffensiveSlot(wearCell) ? value * 2 : value * 6;
			// A bot walks kilometres between hubs and the horse is not always
			// under it, but speed is not power: a real line, not a great one.
			case APPLY_MOV_SPEED:               return value * 4;

			// The four stats, which roll to twelve on a weapon and a shield. A
			// point of the school's own stat is attack; a point of vitality is
			// health no reroll can take away. They used to be worth their own
			// number, so a maximum roll of the best stat in the game scored
			// twelve and lost to two percent of anything.
			case APPLY_CON:                     return value * 20;
			case APPLY_STR:                     return bCaster ? value * 8 : value * 25;
			case APPLY_INT:                     return bCaster ? value * 25 : value * 8;
			case APPLY_DEX:                     return value * 12;

			// A blocked hit is a hit that did not happen, and it answers melee -
			// 73% of the monsters in this world. Fifteen percent of every hit is
			// the roll a player keeps a shield for, after immunity to stun.
			case APPLY_BLOCK:                   return value * 20;
			// Dodge and arrow resistance answer a ranged attacker only, and
			// ranged monsters are between nothing and a quarter of a map: real,
			// and a fraction of what block is worth.
			case APPLY_DODGE:                   return value * 6;
			case APPLY_RESIST_BOW:              return value * 4;
			// battle_hit reads the attacker's WEAR_WEAPON to choose which of
			// these applies and a monster wears no weapon, so against anything a
			// bot fights these five do nothing whatever. Left at a point a line
			// rather than zero, because a line is still a line.
			case APPLY_RESIST_SWORD:
			case APPLY_RESIST_TWOHAND:
			case APPLY_RESIST_DAGGER:
			case APPLY_RESIST_BELL:
			case APPLY_RESIST_FAN:              return value;
			// An elemental resistance is applied at thirty percent of its own
			// number and only against a monster carrying that attack flag, so
			// the fifteen of a maximum roll is four and a half percent off the
			// hits of about half of one map. The player's table wanted a
			// resistance chosen per map; measured, the whole axis is too small
			// to plan a piece of gear around.
			case APPLY_RESIST_FIRE:
			case APPLY_RESIST_ELEC:
			case APPLY_RESIST_WIND:
			case APPLY_RESIST_ICE:
			case APPLY_RESIST_EARTH:
			case APPLY_RESIST_DARK:
			case APPLY_RESIST_MAGIC:            return value * 3;
			case APPLY_REFLECT_MELEE:           return value * 6;

			// A stunned monster does not hit back, which is worth more to a bot
			// than to a player: nothing here retreats from a fight it is winning.
			case APPLY_STUN_PCT:                return value * 10;
			case APPLY_SLOW_PCT:                return value * 6;
			case APPLY_POISON_PCT:              return value * (ch && (int)ch->GetLevel() >= PLAYERBOT_POISON_BOSS_LEVEL ? 16 : 8);

			// The economy lines. A bot's drops are its gear, its refines, its
			// stall and its fares, so twenty percent more of them is a real
			// upgrade; experience is what the whole population is for.
			case APPLY_ITEM_DROP_BONUS:         return value * 8;
			case APPLY_EXP_DOUBLE_BONUS:        return value * 8;
			case APPLY_GOLD_DOUBLE_BONUS:       return value * 4;
			case APPLY_HP_REGEN:
			case APPLY_SP_REGEN:                return value * 2;

			// Big absolute numbers that have to be scaled down to compare with the
			// percentage lines above.
			case APPLY_MAX_HP:                  return value / 4;
			// The immunities roll as a 1, so they used to fall through to the
			// default and be worth one point - less than a point of movement
			// speed. Immunity to stun is the roll a player keeps a shield for
			// the rest of the game, and a bot was rerolling it away.
			case APPLY_IMMUNE_STUN:             return 400;
			case APPLY_IMMUNE_SLOW:             return 250;
			case APPLY_IMMUNE_FALL:             return 120;
			// Mana is what stops a bot keeping its buffs up - 213 of 1300 held
			// less than the 300 SP a mastered aura costs - and it rolls to
			// eighty, so this is one of the few lines that can fix that.
			case APPLY_MAX_SP:                  return bCaster ? value * 2 : value / 2;
			// Everything else - stamina, poison reduction, mana burn - is real but
			// minor for a bot that only grinds. Never zero: a line is still a line.
			default:                            return value;
		}
	}

	// The measured weight above, scaled by Iwakura's PvE tier of the line
	// (playerbot_item_tiers.h, PLAYERBOT_BONUS_TIER_PERCENT): what he calls
	// wspanialy is worth a third more, what he calls bardzo zly a quarter.
	// The equipment score scales its lines the same way
	// (ScorePlayerBotApplyTiered), so buying and rerolling agree.
	// The three slots a young bot is told to bonus first, and what it is told
	// to want on each (Community Patch 1). They are the cheap pieces - the
	// jewellery and the boots a bot of twenty wears are the weakest thing it
	// owns - and that is the point: a line on one of them is worth more early
	// than a grade of refine on any of them.
	bool IsPlayerBotEarlySlotLine(BYTE wearCell, BYTE type)
	{
		switch (wearCell)
		{
			case WEAR_FOOTS:
				// Maks. PZ, Szansa na cios krytyczny, Szybkosc ataku
				return type == APPLY_MAX_HP || type == APPLY_CRITICAL_PCT ||
						type == APPLY_ATT_SPEED;
			case WEAR_NECK:
				// Maks. PZ, Szansa na cios krytyczny, Szansa na przeszywajace uderzenie
				return type == APPLY_MAX_HP || type == APPLY_CRITICAL_PCT ||
						type == APPLY_PENETRATE_PCT;
			case WEAR_WRIST:
				// Maks. PZ, x% obrazen dodanych do PZ, przeszywajace uderzenie,
				// Silny przeciwko Zwierzetom, Silny przeciwko Orkom
				return type == APPLY_MAX_HP || type == APPLY_STEAL_HP ||
						type == APPLY_PENETRATE_PCT ||
						type == APPLY_ATTBONUS_ANIMAL || type == APPLY_ATTBONUS_ORC;
			default:
				return false;
		}
	}

	// Under PLAYERBOT_EARLY_BONUS_MAX_LEVEL the jewellery and the boots come
	// before everything else, and the change stone's refine floor does not
	// apply to them.
	bool IsPlayerBotEarlyBonusSlot(LPCHARACTER ch, BYTE wearCell)
	{
		if (!ch || ch->GetLevel() >= PLAYERBOT_EARLY_BONUS_MAX_LEVEL)
			return false;
		return wearCell == WEAR_NECK || wearCell == WEAR_WRIST || wearCell == WEAR_FOOTS;
	}

	int ScorePlayerBotBonusLine(LPCHARACTER ch, BYTE wearCell, BYTE type, short value)
	{
		const int raw = ScorePlayerBotBonusLineRaw(ch, wearCell, type, value);
		const int tier = ch ? GetPlayerBotBonusTier(type, (int)ch->GetJob(), false) : 0;
		int score = tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
		if (IsPlayerBotEarlyBonusSlot(ch, wearCell) && IsPlayerBotEarlySlotLine(wearCell, type))
			score = score * PLAYERBOT_EARLY_BONUS_PERCENT / 100;
		return score;
	}

	// The one roll that finishes an item, and it is a different roll for every
	// slot because `player.item_attr` lets a different set of lines onto every
	// slot.
	//
	// Everything above is a score, and a score can always be beaten by another
	// score - which means a perfect item is one unlucky comparison away from
	// being rerolled. These are the rolls a player stops on.
	//
	// The old rule asked a helmet for fifteen hundred health and either attack
	// value or arrow resistance, and asked an earring and a wrist for health and
	// critical. None of those five lines can roll on those slots at all - health
	// rolls on body, wrist, foots and neck, critical on weapon, foots and neck,
	// attack value on a body and nowhere else - so a helmet, an earring and a
	// wrist could never be finished, and were rerolled for as long as their
	// owner had gold. What each of them is actually worn for is here instead:
	// attack speed or arrow dodge on a helmet, the race line and item drop on an
	// earring, penetration and stolen life on a wrist.
	//
	// An item that has one is never rerolled again. It may still have a line
	// ADDED, because that cannot lose what is already there.
	bool HasPlayerBotFinishedBonus(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!item)
			return false;

		// Under level forty-five the boots, the necklace and the bracelet are
		// finished by the lines the patch requires of them, not by a score:
		// the health line and at least one more of the slot's list
		// (IsPlayerBotEarlySlotLine) - community patch 2, point 3.
		if (IsPlayerBotEarlyBonusSlot(ch, wearCell))
		{
			long earlyHp = 0;
			int others = 0;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			{
				const BYTE type = item->GetAttributeType(i);
				const long value = item->GetAttributeValue(i);
				if (value <= 0)
					continue;
				if (type == APPLY_MAX_HP)
					earlyHp = value;
				else if (IsPlayerBotEarlySlotLine(wearCell, type))
					++others;
			}
			return earlyHp >= PLAYERBOT_BONUS_KEEP_HP && others >= 1;
		}

		long hp = 0, attGrade = 0, resistBow = 0, crit = 0, penetrate = 0;
		long average = 0, skill = 0, block = 0, dodge = 0, attSpeed = 0;
		long steal = 0, drop = 0, mov = 0, race = 0;
		bool immuneStun = false;
		// The race this bot is being paid for; a line against any other race is
		// not what a piece is kept for, however high it rolled.
		const BYTE wantedRace = GetPlayerBotRaceApplyType(GetPlayerBotFightingRace(ch));
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			if (value <= 0)
				continue;
			if (wantedRace != APPLY_NONE && type == wantedRace)
				race = value;
			switch (type)
			{
				case APPLY_IMMUNE_STUN:             immuneStun = true; break;
				case APPLY_MAX_HP:                  hp = value; break;
				case APPLY_ATT_GRADE_BONUS:         attGrade = value; break;
				case APPLY_RESIST_BOW:              resistBow = value; break;
				case APPLY_CRITICAL_PCT:            crit = value; break;
				case APPLY_PENETRATE_PCT:           penetrate = value; break;
				case APPLY_NORMAL_HIT_DAMAGE_BONUS: average = value; break;
				case APPLY_SKILL_DAMAGE_BONUS:      skill = value; break;
				case APPLY_BLOCK:                   block = value; break;
				case APPLY_DODGE:                   dodge = value; break;
				case APPLY_ATT_SPEED:               attSpeed = value; break;
				case APPLY_STEAL_HP:                steal = value; break;
				case APPLY_ITEM_DROP_BONUS:         drop = value; break;
				case APPLY_MOV_SPEED:               mov = value; break;
				default: break;
			}
		}

		switch (wearCell)
		{
			case WEAR_SHIELD:
				// Immunity to stun first, as it always was; then the two lines a
				// shield is otherwise kept for, and both of them roll here and
				// nowhere else worth speaking of.
				return immuneStun || block >= PLAYERBOT_BONUS_KEEP_BLOCK ||
						race >= PLAYERBOT_BONUS_KEEP_RACE;
			case WEAR_WEAPON:
			{
				// A level-30 or level-75 weapon a player hand-tuned is finished
				// the moment it lands an average-damage or average-skill line
				// over the lock, so the mixer leaves it alone (Ciapek).
				const int lvl = item->GetLevelLimit();
				if ((lvl == 30 || lvl == 75) &&
						(average >= PLAYERBOT_BONUS_WEAPON_LOCK_PCT ||
						 skill >= PLAYERBOT_BONUS_WEAPON_LOCK_PCT))
					return true;
				// Any weapon, not only the level-30 family: with the vnum test
				// here a bow of forty-five with a 40% average was "unfinished"
				// and rerolled towards the line score until the average was
				// gone ("boty zmixowaly wysokie srednie 35+ na duzo mniejsze").
				//
				// The caster's clause is not generosity: item_addon.cpp draws
				// the skill line and then sets the average to minus twice it, so
				// a weapon cannot carry both and a Shaman that only ever stopped
				// on the average line never stopped at all.
				//
				// And a big skill line is finished for every class, not only a
				// caster: it is a PvP prize this world will use later, and mixing
				// it off would waste it ("szkoda tracic takiego ladnego bonusu do
				// PvP", Tieru). PvE still wears the average weapon - this only
				// stops the reroll from destroying the skill one.
				// Thirty since community patch 2, point 3 ("tak, aby wynosily one
				// 30%+ SR"); it was PLAYERBOT_BONUS_KEEP_AVERAGE, twenty.
				return average >= PLAYERBOT_BONUS_WEAPON_TARGET_AVERAGE ||
						skill > PLAYERBOT_BONUS_SKILL_PVP_PCT ||
						(IsPlayerBotCaster(ch) && skill >= PLAYERBOT_BONUS_KEEP_SKILL);
			}
			case WEAR_BODY:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(attGrade > 0 || resistBow > 0 ||
						 steal >= PLAYERBOT_BONUS_KEEP_STEAL);
			case WEAR_HEAD:
				// No health, no attack value, no arrow resistance rolls here.
				return attSpeed >= PLAYERBOT_BONUS_KEEP_ATT_SPEED ||
						dodge >= PLAYERBOT_BONUS_KEEP_DODGE ||
						race >= PLAYERBOT_BONUS_KEEP_RACE;
			case WEAR_FOOTS:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(attSpeed >= PLAYERBOT_BONUS_KEEP_ATT_SPEED ||
						 crit >= PLAYERBOT_BONUS_KEEP_CRIT ||
						 dodge >= PLAYERBOT_BONUS_KEEP_DODGE ||
						 mov >= PLAYERBOT_BONUS_KEEP_MOV);
			case WEAR_WRIST:
				// Critical does not roll on a wrist; penetration does.
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(penetrate >= PLAYERBOT_BONUS_KEEP_CRIT ||
						 steal >= PLAYERBOT_BONUS_KEEP_STEAL ||
						 drop >= PLAYERBOT_BONUS_KEEP_DROP ||
						 race >= PLAYERBOT_BONUS_KEEP_RACE);
			case WEAR_NECK:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						crit >= PLAYERBOT_BONUS_KEEP_CRIT;
			case WEAR_EAR:
				// Neither health nor critical rolls on an earring. What does is
				// the race line, item drop and movement speed.
				return race >= PLAYERBOT_BONUS_KEEP_RACE ||
						drop >= PLAYERBOT_BONUS_KEEP_DROP ||
						mov >= PLAYERBOT_BONUS_KEEP_MOV;
			default:
				return false;
		}
	}

	// The rolls a piece is priced up for: the top of what a line can be, or near
	// enough that a player keeps the item for it. A deliberately shorter list
	// than ScorePlayerBotBonusLine - the question here is not "is this line
	// good" but "is this the line somebody pays extra for" - and every number
	// is the lv5 column of `player.item_attr`, so it is the top of what this
	// world can actually roll rather than the top of what the wiki lists.
	//
	// Two entries used to name lines that cannot roll here at all: health as a
	// percentage is in no attribute table, and "strong against monsters" is only
	// in the rare one, where it is ten and not twenty.
	bool IsPlayerBotTopBonusLine(BYTE type, long value)
	{
		switch (type)
		{
			case APPLY_MAX_HP:                  return value >= 2000;
			case APPLY_MAX_SP:                  return value >= 80;
			case APPLY_CON:
			case APPLY_INT:
			case APPLY_STR:
			case APPLY_DEX:                     return value >= 12;
			case APPLY_CRITICAL_PCT:            return value >= 10;
			case APPLY_PENETRATE_PCT:           return value >= 10;
			case APPLY_SKILL_DAMAGE_BONUS:      return value >= 15;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS: return value >= 20;
			// Twenty on the five ordinary races, ten on human, which rolls half
			// as high and half as often.
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:          return value >= 20;
			case APPLY_ATTBONUS_HUMAN:          return value >= 10;
			case APPLY_ATTBONUS_MONSTER:        return value >= 10;
			case APPLY_ATT_GRADE_BONUS:         return value >= 50;
			case APPLY_ATT_SPEED:               return value >= 8;
			case APPLY_MOV_SPEED:               return value >= 20;
			case APPLY_STEAL_HP:                return value >= 10;
			case APPLY_BLOCK:
			case APPLY_DODGE:                   return value >= 15;
			case APPLY_ITEM_DROP_BONUS:
			case APPLY_EXP_DOUBLE_BONUS:        return value >= 20;
			case APPLY_IMMUNE_STUN:
			case APPLY_IMMUNE_SLOW:             return true;
			default:                            return false;
		}
	}

	// Iwakura's bonus multipliers - the rows per slot and line, and the tiers
	// of a weapon's two damage lines - are generated into
	// playerbot_price_tables.h from his sheet, each row checked against
	// world.item_attr for the slot he put it under.
	// The whole product is capped here - hundredths, so ten thousand is a
	// hundredfold; a weapon of sixty average and thirty skill would be
	// 2800 times its base otherwise.
	const long long PLAYERBOT_BONUS_PRICE_MAX_PCT = 10000;

	BYTE GetPlayerBotPriceSlot(LPITEM item)
	{
		if (!item)
			return 0;
		if (item->GetType() == ITEM_WEAPON)
			return PRICE_SLOT_WEAPON;
		if (item->GetType() != ITEM_ARMOR)
			return 0;
		switch (item->GetSubType())
		{
			case ARMOR_BODY:   return PRICE_SLOT_BODY;
			case ARMOR_HEAD:   return PRICE_SLOT_HEAD;
			case ARMOR_SHIELD: return PRICE_SLOT_SHIELD;
			case ARMOR_FOOTS:  return PRICE_SLOT_FOOTS;
			case ARMOR_WRIST:  return PRICE_SLOT_WRIST;
			case ARMOR_NECK:   return PRICE_SLOT_NECK;
			case ARMOR_EAR:    return PRICE_SLOT_EAR;
			default:           return 0;
		}
	}

	// The top roll of an apply on this item's attribute set, from the
	// engine's own table; zero when the table has no such line.
	long GetPlayerBotBonusMaxRoll(LPITEM item, BYTE bApply)
	{
		TItemAttrMap::const_iterator it = g_map_itemAttr.find(bApply);
		if (it == g_map_itemAttr.end())
			return 0;
		const TItemAttrTable& row = it->second;
		const int set = item ? item->GetAttributeSetIndex() : -1;
		int level = (set >= 0 && set < ATTRIBUTE_SET_MAX_NUM) ? row.bMaxLevelBySet[set] : 0;
		if (level <= 0 || level > ITEM_ATTRIBUTE_MAX_LEVEL)
			level = ITEM_ATTRIBUTE_MAX_LEVEL;
		return row.lValues[level - 1];
	}

	// A weapon damage line's multiplier on Iwakura's sheet, read between his
	// bands. The sheet gives one number to a band - average 10-19 x1.2, 20-29
	// x1.5, skill 1-10 x1.2 - and read as steps, a 19% average asked what a 10%
	// one did, and exactly what a weapon of 1% average and 3% skill did: two
	// Ostrza z Czerwonej Stali +0 at 15 150 000 each ("czy nie pracowalismy nad
	// tym, aby premiowana bardziej byla z wyzszymi srednimi?", Tieru,
	// 15 September). His number is taken as what a roll in the middle of its
	// band is worth, and the multiplier runs in a straight line from one band's
	// middle to the next: a better roll asks more, a worse one less, and a
	// band's rolls average his price. It starts from no premium one point under
	// the first band. The last band is a single value and ends the line.
	WORD GetPlayerBotDamageTierPct(const TPlayerBotDamageTier* tiers, size_t count, long value)
	{
		if (!tiers || count == 0)
			return 100;
		// Doubled, so the middle of a band is a whole number.
		const long v2 = 2L * value;
		long prevX = 2L * ((long)tiers[0].bFrom - 1);
		long prevPct = 100;
		if (v2 <= prevX)
			return 100;
		for (size_t i = 0; i < count; ++i)
		{
			const long from = tiers[i].bFrom;
			const long end = i + 1 < count ? (long)tiers[i + 1].bFrom - 1 : from;
			const long midX = from + end;
			const long pct = tiers[i].wPct;
			if (v2 <= midX)
				return (WORD)(prevPct + (pct - prevPct) * (v2 - prevX) / std::max(1L, midX - prevX));
			prevX = midX;
			prevPct = pct;
		}
		return (WORD)prevPct;
	}

	// What the lines on an item add to its asking price, as a percentage:
	// Iwakura's multipliers compounded, less the one the base already is.
	//
	// No character is asked for, on purpose: this is what any buyer pays, not
	// what one bot would wear, so the caster and weapon-slot weightings of
	// ScorePlayerBotItemBonuses are left out of it.
	int GetPlayerBotBonusPricePercent(LPITEM item)
	{
		if (!item)
			return 0;
		const BYTE slot = GetPlayerBotPriceSlot(item);
		if (slot == 0)
			return 0;
		const int level = item->GetLevelLimit();
		long long product = 100; // hundredths
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			if (type == 0 || value <= 0)
				continue;
			WORD pct = 100;
			if (slot == PRICE_SLOT_WEAPON && type == APPLY_NORMAL_HIT_DAMAGE_BONUS)
				pct = GetPlayerBotDamageTierPct(PLAYERBOT_AVERAGE_DAMAGE_TIERS,
						sizeof(PLAYERBOT_AVERAGE_DAMAGE_TIERS) / sizeof(PLAYERBOT_AVERAGE_DAMAGE_TIERS[0]), value);
			else if (slot == PRICE_SLOT_WEAPON && type == APPLY_SKILL_DAMAGE_BONUS)
				pct = GetPlayerBotDamageTierPct(PLAYERBOT_SKILL_DAMAGE_TIERS,
						sizeof(PLAYERBOT_SKILL_DAMAGE_TIERS) / sizeof(PLAYERBOT_SKILL_DAMAGE_TIERS[0]), value);
			else
			{
				for (size_t r = 0; r < sizeof(PLAYERBOT_BONUS_PRICE_ROWS) / sizeof(PLAYERBOT_BONUS_PRICE_ROWS[0]); ++r)
				{
					const TPlayerBotBonusPriceRow& row = PLAYERBOT_BONUS_PRICE_ROWS[r];
					if (row.bApply != type || (row.bSlots & slot) == 0 ||
							level < row.bMinLevel || level > row.bMaxLevel)
						continue;
					const long maxRoll = GetPlayerBotBonusMaxRoll(item, type);
					pct = (maxRoll > 0 && value >= maxRoll) ? row.wMaxPct : row.wOtherPct;
					break;
				}
			}
			product = product * pct / 100;
			if (product >= PLAYERBOT_BONUS_PRICE_MAX_PCT)
			{
				product = PLAYERBOT_BONUS_PRICE_MAX_PCT;
				break;
			}
		}
		return (int)(product - 100);
	}

	int ScorePlayerBotItemBonuses(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!ch || !item)
			return 0;
		int score = 0;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			score += ScorePlayerBotBonusLine(ch, wearCell,
					item->GetAttributeType(i), item->GetAttributeValue(i));
		}
		return score;
	}

	// An item the engine will actually accept a stone on. UseItemEx refuses an
	// equipped item outright ("if (item2->IsEquipped()) return false"), costumes,
	// and anything without an attribute set, so a bot has to take the piece off
	// first - exactly as a player does.
	// A Marmur Blogoslawienstwa in the bag: the one item that adds a fifth
	// line (USE_ADD_ATTRIBUTE2, vnums 39004/70024/70124/76015 on these files;
	// asked by subtype so a renamed one still counts). Nothing sells it, so
	// it comes from drops and chests, and a bot without one stops at four
	// like a player without one.
	int FindPlayerBotBlessingMarbleCell(LPCHARACTER ch)
	{
		if (!ch)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_USE && item->GetSubType() == USE_ADD_ATTRIBUTE2 &&
					item->GetCount() > 0 && !item->isLocked())
				return (int)cell;
		}
		return -1;
	}

	bool CanPlayerBotRerollItem(LPITEM item)
	{
		return item && item->GetType() != ITEM_COSTUME && !item->isLocked() &&
				!item->IsExchanging() && item->GetAttributeSetIndex() != -1 &&
				item->GetRefineLevel() >= PLAYERBOT_BONUS_MIN_REFINE;
	}

	// The same, for a piece in its slot: the young bot's boots, necklace and
	// bracelet are bonused at any refine ("niezaleznie od poziomu ulepszenia",
	// community patch 2, point 3). The refine floor above kept every one of
	// them under +4 out of the pass, which with the jewellery a bot of twenty
	// wears is nearly all of it - the patch's "boty aplikowaly je losowo".
	bool CanPlayerBotRerollItemFor(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (IsPlayerBotEarlyBonusSlot(ch, wearCell))
			return item && item->GetType() != ITEM_COSTUME && !item->isLocked() &&
					!item->IsExchanging() && item->GetAttributeSetIndex() != -1;
		return CanPlayerBotRerollItem(item);
	}

	// How many of the boots, the necklace and the bracelet worn carry a
	// health line - the weapon's turn comes at two (community patch 2).
	int CountPlayerBotEarlyHpPieces(LPCHARACTER ch)
	{
		static const BYTE slots[] = { WEAR_FOOTS, WEAR_NECK, WEAR_WRIST };
		int pieces = 0;
		for (size_t s = 0; ch && s < sizeof(slots) / sizeof(slots[0]); ++s)
		{
			LPITEM worn = ch->GetWear(slots[s]);
			for (int i = 0; worn && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (worn->GetAttributeType(i) == APPLY_MAX_HP && worn->GetAttributeValue(i) > 0)
				{
					++pieces;
					break;
				}
		}
		return pieces;
	}

	// Zielony Czar and Zielona Sila (71151/76023, 71152/76024) are the change
	// and add stones of the same kind as 71084/71085, and the engine lets a
	// player spend one only on a weapon or a body armour of level forty or less
	// (char_item.cpp, USE_CHANGE_ATTRIBUTE and USE_ADD_ATTRIBUTE). The bots never
	// spent one: under PLAYERBOT_BONUS_MIN_LEVEL no stone was spent at all, and
	// those are the bots whose gear the green stones are for ("Boty nie uzywaja
	// zielonego czaru i zielonego wzmocnienia", Sammy Suricate, 18 September).
	// The pass calls AddAttribute itself, so it has to keep the engine's rule
	// on its own: a green stone never goes on a helmet or a level-70 armour.
	bool IsPlayerBotGreenBonusStone(DWORD vnum)
	{
		return vnum == 71151 || vnum == 71152 || vnum == 76023 || vnum == 76024;
	}

	bool CanPlayerBotSpendGreenBonusStoneOn(LPITEM target)
	{
		if (!target)
			return false;
		if (target->GetType() != ITEM_WEAPON &&
				!(target->GetType() == ITEM_ARMOR && target->GetSubType() == ARMOR_BODY))
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (target->GetLimitType(i) == LIMIT_LEVEL &&
					target->GetLimitValue(i) > PLAYERBOT_GREEN_BONUS_MAX_LEVEL)
				return false;
		return true;
	}

	// The bag stone of the kind a vnum names: the change stone is
	// USE_CHANGE_ATTRIBUTE and the add stone USE_ADD_ATTRIBUTE, and on these
	// files each comes in three vnums (71084/71151/76023, 71085/71152/76024) -
	// a bot counting only its own vnum vendored the others. For a target, the
	// stone that may go on it: a green one first where the target takes one -
	// it is good for nothing else - and a plain one otherwise, unless the bot
	// is young enough to spend green ones only (greenOnly). Without a target,
	// any stone of the kind: the "is there anything to spend" of the pass.
	int FindPlayerBotBonusStoneCellLike(LPCHARACTER ch, DWORD vnum, LPITEM target = NULL, bool greenOnly = false)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!ch || !proto)
			return -1;
		int plain = -1;
		int green = -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetType() != proto->bType || stone->GetSubType() != proto->bSubType)
				continue;
			if (IsPlayerBotGreenBonusStone(stone->GetVnum()))
			{
				if (green < 0)
					green = cell;
			}
			else if (plain < 0)
				plain = cell;
		}
		if (!target)
			return greenOnly ? green : (plain >= 0 ? plain : green);
		if (green >= 0 && CanPlayerBotSpendGreenBonusStoneOn(target))
			return green;
		return greenOnly ? -1 : plain;
	}

	// A bot spends the stones it holds and no others. It used to make one out
	// of nothing whenever the bag had none - AutoGiveItem for a price in yang,
	// on the grounds that the stones could not be dropped, traded or shopped.
	// That was never true of mt2009: both drop from monsters
	// (mob_drop_item.txt) and come out of chests and the Moonlight chest, and on
	// a world with the chests switched off the gear history showed bots
	// spending Wzmocnienie Przedmiotu that no bag had ever received and the
	// economy charts had none of ("boty zmieniaja oraz dodaja bonusy bez
	// przedmiotu", seban latino and Drip, 15 September). The operator's rule is
	// the marble's: a bot without a stone does without, the way a player does.
	bool HasPlayerBotBonusStone(LPCHARACTER ch, DWORD vnum, bool greenOnly = false)
	{
		return ch && FindPlayerBotBonusStoneCellLike(ch, vnum, NULL, greenOnly) >= 0;
	}

	// The stone FindPlayerBotBonusStoneCellLike chose for a piece, by its cell.
	bool ConsumePlayerBotBonusStoneAt(LPCHARACTER ch, int cell)
	{
		if (!ch || cell < 0 || cell >= PLAYERBOT_BAG_CELLS)
			return false;
		LPITEM stone = ch->GetInventoryItem((WORD)cell);
		if (!stone)
			return false;
		if (stone->GetCount() > 1)
			stone->SetCount(stone->GetCount() - 1);
		else
			ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_BONUS");
		return true;
	}

	// Whether the bag holds a stone this piece could take now - an add stone
	// while it has room for a line, a change stone once it is full (from +5,
	// or at any refine on a young bot's jewellery). The swap rule waits only
	// while the waiting can end (IsPlayerBotSwapHeldForBonus), so this asks
	// first what the bonus pass's own loop for the held piece asks: an armour
	// under PLAYERBOT_BONUS_MIN_REFINE, or a piece that takes no lines at all,
	// is never worked on there, and a stone in the bag for it would have held
	// a bought +0 armour back from its owner for good.
	bool PlayerBotHoldsBonusStoneFor(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const int slot = item->FindEquipCell(ch);
		if (slot < 0 || slot >= WEAR_MAX_NUM || !CanPlayerBotRerollItemFor(ch, item, (BYTE)slot))
			return false;
		const bool early = IsPlayerBotEarlyBonusSlot(ch, (BYTE)slot);
		const bool greenOnly = ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL && !early;
		if (item->GetAttributeCount() < PLAYERBOT_BONUS_MAX_LINES)
			return FindPlayerBotBonusStoneCellLike(ch, PLAYERBOT_BONUS_ADD_VNUM, item, greenOnly) >= 0;
		return (early || item->GetRefineLevel() >= PLAYERBOT_BONUS_CHANGE_MIN_REFINE) &&
				FindPlayerBotBonusStoneCellLike(ch, PLAYERBOT_BONUS_CHANGE_VNUM, item, greenOnly) >= 0;
	}

	// Worn gear, and the new piece the swap rule holds back (see the loop at
	// the end). Other spares in the bag are sold or put in a stall long before
	// they are worth polishing, and rerolling them would spend the gold the bot
	// needs for its next real upgrade.
	bool ManagePlayerBotBonusReroll(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextBonusCheckTime)
			return false;
		state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_INTERVAL;
		// A young bot spends the green stones only, on the gear they are for.
		const bool greenOnly = ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL;
		// The green stones the engine lets a young bot use work on a weapon and
		// on body armour of level forty or less, and on nothing else - so the
		// necklace, the wrist and the boots Community Patch 1 asks a young bot
		// to bonus first can only be done with an ordinary stone. Under the
		// level those three slots are therefore allowed one, and every other
		// slot is still green-only.
		const bool ordinaryForJewellery = greenOnly &&
				(HasPlayerBotBonusStone(ch, PLAYERBOT_BONUS_ADD_VNUM, false) ||
				 HasPlayerBotBonusStone(ch, PLAYERBOT_BONUS_CHANGE_VNUM, false));
		// Nothing to spend, nothing to weigh: the pass below scores every line
		// of eight worn pieces, and a bag with no stone and no marble ends here.
		if (!HasPlayerBotBonusStone(ch, PLAYERBOT_BONUS_ADD_VNUM, greenOnly) &&
				!HasPlayerBotBonusStone(ch, PLAYERBOT_BONUS_CHANGE_VNUM, greenOnly) &&
				!ordinaryForJewellery &&
				(greenOnly || FindPlayerBotBlessingMarbleCell(ch) < 0))
			return false;

		// The order the stones are spent in. Past the early band it is the
		// order the gear matters in; under it, Community Patch 1 puts the
		// necklace, the wrist and the boots first, because that is where a
		// line is worth more than a grade of refine.
		const BYTE wearSlotsLate[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		const BYTE wearSlotsEarly[] = {
			WEAR_NECK, WEAR_WRIST, WEAR_FOOTS,
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_EAR
		};
		const bool early = ch->GetLevel() < PLAYERBOT_EARLY_BONUS_MAX_LEVEL;
		const BYTE* wearSlots = early ? wearSlotsEarly : wearSlotsLate;
		const size_t wearSlotCount = early ? sizeof(wearSlotsEarly) / sizeof(wearSlotsEarly[0])
				: sizeof(wearSlotsLate) / sizeof(wearSlotsLate[0]);

		int stonesUsed = 0;
		for (size_t i = 0; i < wearSlotCount &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++i)
		{
			const BYTE wearCell = wearSlots[i];
			LPITEM item = ch->GetWear(wearCell);
			if (!CanPlayerBotRerollItemFor(ch, item, wearCell))
				continue;
			// The weapon waits for the health lines (community patch 2, point
			// 3): two of the boots, the necklace and the bracelet first.
			if (early && wearCell == WEAR_WEAPON &&
					CountPlayerBotEarlyHpPieces(ch) < PLAYERBOT_EARLY_HP_PIECES_FOR_WEAPON)
				continue;
			// A green stone cannot touch jewellery or boots at all, so the
			// three early slots take an ordinary one even under the level.
			const bool slotGreenOnly = greenOnly && !IsPlayerBotEarlyBonusSlot(ch, wearCell);

			const int count = item->GetAttributeCount();
			const int score = ScorePlayerBotItemBonuses(ch, item, wearCell);

			// An empty line is free power: add before rerolling, always. Only once
			// the item is full does the quality of what it rolled start to matter,
			// and USE_CHANGE_ATTRIBUTE needs at least one line to work on anyway.
			// Four by the stone; the fifth is the marble's, below, and only when
			// the bag holds one.
			const bool bWantAdd = count < PLAYERBOT_BONUS_MAX_LINES;
			const int marbleCell = (count == PLAYERBOT_BONUS_MAX_LINES && !slotGreenOnly)
					? FindPlayerBotBlessingMarbleCell(ch) : -1;
			const bool bWantMarble = marbleCell >= 0;
			// An item that has landed the roll its slot is bought for is finished.
			// It can still gain a line - that cannot lose what is already there -
			// but it is never rerolled, whatever the score says.
			// A level-30 weapon is rerolled until it lands its average line,
			// whatever the score says: the score is a sum of good lines and a
			// weapon full of them at twelve percent average was "good enough"
			// to the score and not to anybody who looked at it.
			// The young bot's jewellery and boots are changed until they carry
			// the lines the patch requires, whatever the score (community
			// patch 2, point 3).
			const bool bWantChange = !bWantAdd && !bWantMarble &&
					(item->GetRefineLevel() >= PLAYERBOT_BONUS_CHANGE_MIN_REFINE ||
					 IsPlayerBotEarlyBonusSlot(ch, wearCell)) &&
					!HasPlayerBotFinishedBonus(ch, item, wearCell) &&
					(score < PLAYERBOT_BONUS_KEEP_SCORE ||
					 IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum()) ||
					 IsPlayerBotEarlyBonusSlot(ch, wearCell));
			if (!bWantAdd && !bWantMarble && !bWantChange)
				continue;

			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM
					: PLAYERBOT_BONUS_CHANGE_VNUM;
			const int stoneCell = bWantMarble ? -1
					: FindPlayerBotBonusStoneCellLike(ch, stoneVnum, item, slotGreenOnly);
			if (!bWantMarble && stoneCell < 0)
				continue;

			// The piece has to come off for the engine to touch it, and it has to go
			// back on afterwards - a bot walking around with its weapon in the bag
			// would be worse than any bonus line it could win.
			if (!ch->UnequipItem(item))
				continue;

			// The engine's own odds for a line, aiItemAttributeAddPercent by the
			// count already there (100/80/60/50, and 30 for the marble's fifth);
			// the stone or the marble is spent whether the roll lands or not,
			// as at the counter.
			bool landed = true;
			if (bWantMarble)
			{
				landed = number(1, 100) <= aiItemAttributeAddPercent[count];
				if (landed)
					item->AddAttribute();
				LPITEM marble = ch->GetInventoryItem((WORD)marbleCell);
				if (marble)
					marble->SetCount(marble->GetCount() - 1);
			}
			else if (bWantAdd)
			{
				landed = number(1, 100) <= aiItemAttributeAddPercent[count];
				if (landed)
					item->AddAttribute();
			}
			else
				item->ChangeAttribute();

			if (!bWantMarble)
				ConsumePlayerBotBonusStoneAt(ch, stoneCell);
			++stonesUsed;

			const int newScore = ScorePlayerBotItemBonuses(ch, item, wearCell);
			if (!PlayerBotEquipItem(ch, item))
			{
				sys_err("PLAYERBOT_BONUS: could not re-equip pid=%u name=%s vnum=%u slot=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
						(unsigned int)wearCell);
				continue;
			}

			// The gear history shows the stone spent (PLAYERBOT_BONUS); this
			// names the piece it was spent on, which is what a player asks -
			// "na jaki przedmiot" (Tieru, 13 September).
			LogManager::instance().ItemLog(ch, item,
					bWantMarble ? "PLAYERBOT_BONUS_MARBLE"
						: (bWantAdd ? "PLAYERBOT_BONUS_ADD" : "PLAYERBOT_BONUS_CHANGE"),
					item->GetName());
			sys_log(0, "PLAYERBOT_BONUS: %s pid=%u name=%s vnum=%u slot=%u lines=%d->%d score=%d->%d gold=%d",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(),
					item->GetVnum(), (unsigned int)wearCell, count,
					item->GetAttributeCount(), score, newScore,
					(int)(ch->GetGold() / 1000));
		}

		// The new piece the swap rule holds in the bag (IsPlayerBotSwapHeldForBonus,
		// community patch 2, point 3) is bonused where it lies - no unequipping,
		// the engine only refuses a worn item - until its lines beat the worn
		// piece's and the equipment pass puts it on.
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					!IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			const int slot = item->FindEquipCell(ch);
			if (slot < 0 || slot >= WEAR_MAX_NUM)
				continue;
			LPITEM worn = ch->GetWear((BYTE)slot);
			if (!worn || !IsPlayerBotSwapHeldForBonus(ch, item, worn) ||
					!CanPlayerBotRerollItemFor(ch, item, (BYTE)slot))
				continue;
			const int count = item->GetAttributeCount();
			const bool bWantAdd = count < PLAYERBOT_BONUS_MAX_LINES;
			if (!bWantAdd && item->GetRefineLevel() < PLAYERBOT_BONUS_CHANGE_MIN_REFINE &&
					!IsPlayerBotEarlyBonusSlot(ch, (BYTE)slot))
				continue;
			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM : PLAYERBOT_BONUS_CHANGE_VNUM;
			const bool slotGreenOnly = greenOnly && !IsPlayerBotEarlyBonusSlot(ch, (BYTE)slot);
			const int stoneCell = FindPlayerBotBonusStoneCellLike(ch, stoneVnum, item, slotGreenOnly);
			if (stoneCell < 0)
				continue;
			const long long before = GetPlayerBotItemLineScore(item, ch);
			if (bWantAdd)
			{
				if (number(1, 100) <= aiItemAttributeAddPercent[count])
					item->AddAttribute();
			}
			else
				item->ChangeAttribute();
			ConsumePlayerBotBonusStoneAt(ch, stoneCell);
			++stonesUsed;
			LogManager::instance().ItemLog(ch, item,
					bWantAdd ? "PLAYERBOT_BONUS_ADD" : "PLAYERBOT_BONUS_CHANGE", item->GetName());
			sys_log(0, "PLAYERBOT_BONUS: %s held piece pid=%u name=%s vnum=%u slot=%d lines=%d->%d value=%lld->%lld worn_value=%lld",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(), item->GetVnum(), slot,
					count, item->GetAttributeCount(), before, GetPlayerBotItemLineScore(item, ch),
					GetPlayerBotItemLineScore(worn, ch));
		}

		// The level-30 weapons in the bag are goods, and a level-30 weapon
		// sells for its average line (PLAYERBOT_PRIZE_AVERAGE_DAMAGE). A stone
		// costs a fortieth of what the finished piece asks, so the ones that
		// have not rolled it yet are worked on here too - no unequipping, the
		// engine only refuses a worn item.
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || !IsPlayerBotSpecialLevel30Weapon(item) ||
					!CanPlayerBotRerollItem(item))
				continue;
			const int count = item->GetAttributeCount();
			const bool bWantAdd = count < PLAYERBOT_BONUS_MAX_LINES;
			if (!bWantAdd && HasPlayerBotFinishedBonus(ch, item, WEAR_WEAPON))
				continue;
			// No change stone below +5, worn or in the bag.
			if (!bWantAdd && item->GetRefineLevel() < PLAYERBOT_BONUS_CHANGE_MIN_REFINE)
				continue;
			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM
					: PLAYERBOT_BONUS_CHANGE_VNUM;
			// A weapon, so the green stone is the one a young bot may use here.
			const int stoneCell = FindPlayerBotBonusStoneCellLike(ch, stoneVnum, item, greenOnly);
			if (stoneCell < 0)
				continue;
			const int score = ScorePlayerBotItemBonuses(ch, item, WEAR_WEAPON);
			// The engine's odds, as for the worn pieces above.
			if (bWantAdd)
			{
				if (number(1, 100) <= aiItemAttributeAddPercent[count])
					item->AddAttribute();
			}
			else
				item->ChangeAttribute();
			ConsumePlayerBotBonusStoneAt(ch, stoneCell);
			++stonesUsed;
			LogManager::instance().ItemLog(ch, item,
					bWantAdd ? "PLAYERBOT_BONUS_ADD" : "PLAYERBOT_BONUS_CHANGE",
					item->GetName());
			sys_log(0, "PLAYERBOT_BONUS: %s goods pid=%u name=%s vnum=%u lines=%d->%d score=%d->%d gold=%d",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(),
					item->GetVnum(), count, item->GetAttributeCount(), score,
					ScorePlayerBotItemBonuses(ch, item, WEAR_WEAPON), (int)(ch->GetGold() / 1000));
		}
		return stonesUsed > 0;
	}
}

#endif
