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

	int ScorePlayerBotBonusLine(LPCHARACTER ch, bool bOffensiveSlot, BYTE type, short value)
	{
		// A negative roll exists (movement speed on some sets) and is worth less
		// than nothing, so it must not be able to prop up a bad item's total.
		if (value <= 0)
			return 0;

		switch (type)
		{
			case APPLY_SKILL_DAMAGE_BONUS:      return value * 12;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS: return value * 10;
			case APPLY_CRITICAL_PCT:            return value * 10;
			case APPLY_PENETRATE_PCT:           return value * 10;
			case APPLY_ATTBONUS_MONSTER:        return value * 8;
			case APPLY_ATT_SPEED:               return value * 8;
			case APPLY_STEAL_HP:                return value * 6;
			case APPLY_ATT_GRADE_BONUS:         return bOffensiveSlot ? value * 5 : value * 3;
			case APPLY_CAST_SPEED:              return IsPlayerBotCaster(ch) ? value * 8 : value;
			case APPLY_MAX_HP_PCT:              return value * 15;
			case APPLY_DEF_GRADE_BONUS:         return bOffensiveSlot ? value * 2 : value * 6;
			case APPLY_MOV_SPEED:               return value * 3;
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
			case APPLY_MAX_SP:                  return IsPlayerBotCaster(ch) ? value / 4 : value / 12;
			// Everything else - resistances, stamina, experience bonus - is real but
			// minor for a bot that only grinds. Never zero: a line is still a line.
			default:                            return value;
		}
	}

	// The one roll that finishes an item.
	//
	// Everything above is a score, and a score can always be beaten by another
	// score - which means a perfect item is one unlucky comparison away from
	// being rerolled. These are the rolls a player stops on: a shield that can
	// no longer be stunned, a level-30 weapon at thirty percent average damage,
	// armour and jewellery carrying the health that keeps a character standing.
	// An item that has one is never rerolled again. It may still have a line
	// ADDED, because that cannot lose what is already there.
	bool HasPlayerBotFinishedBonus(LPITEM item, BYTE wearCell)
	{
		if (!item)
			return false;

		long hp = 0, attGrade = 0, resistBow = 0, crit = 0, average = 0;
		bool immuneStun = false;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			if (value <= 0)
				continue;
			switch (type)
			{
				case APPLY_IMMUNE_STUN:             immuneStun = true; break;
				case APPLY_MAX_HP:                  hp = value; break;
				case APPLY_ATT_GRADE_BONUS:         attGrade = value; break;
				case APPLY_RESIST_BOW:              resistBow = value; break;
				case APPLY_CRITICAL_PCT:            crit = value; break;
				case APPLY_NORMAL_HIT_DAMAGE_BONUS: average = value; break;
				default: break;
			}
		}

		switch (wearCell)
		{
			case WEAR_SHIELD:
				return immuneStun;
			case WEAR_WEAPON:
				// Only on the level-30 families. On anything else average damage
				// is a good line, not a reason to stop: the weapon itself is
				// going to be replaced.
				return IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum()) &&
						average >= PLAYERBOT_BONUS_KEEP_AVERAGE;
			case WEAR_BODY:
			case WEAR_HEAD:
			case WEAR_FOOTS:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(attGrade > 0 || resistBow > 0);
			case WEAR_WRIST:
			case WEAR_NECK:
			case WEAR_EAR:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						crit >= PLAYERBOT_BONUS_KEEP_CRIT;
			default:
				return false;
		}
	}

	bool IsPlayerBotOffensiveSlot(BYTE wearCell)
	{
		return wearCell == WEAR_WEAPON;
	}

	int ScorePlayerBotItemBonuses(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!ch || !item)
			return 0;
		const bool bOffensive = IsPlayerBotOffensiveSlot(wearCell);
		int score = 0;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			score += ScorePlayerBotBonusLine(ch, bOffensive,
					item->GetAttributeType(i), item->GetAttributeValue(i));
		}
		return score;
	}

	// An item the engine will actually accept a stone on. UseItemEx refuses an
	// equipped item outright ("if (item2->IsEquipped()) return false"), costumes,
	// and anything without an attribute set, so a bot has to take the piece off
	// first - exactly as a player does.
	bool CanPlayerBotRerollItem(LPITEM item)
	{
		return item && item->GetType() != ITEM_COSTUME && !item->isLocked() &&
				!item->IsExchanging() && item->GetAttributeSetIndex() != -1;
	}

	// The stones cannot be dropped, sold, traded or shopped, so there is no market
	// to walk to: the bot pays for one the same way it pays for its stall.
	bool BuyPlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		if (ch->CountSpecifyItem(vnum) > 0)
			return true;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;
		if (ch->GetEmptyInventory(1) < 0)
			return false;
		if (!ch->AutoGiveItem(vnum, 1, -1, false))
			return false;
		ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_BONUS_STONE_PRICE);
		return true;
	}

	bool ConsumePlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetVnum() != vnum)
				continue;
			if (stone->GetCount() > 1)
				stone->SetCount(stone->GetCount() - 1);
			else
				ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_BONUS");
			return true;
		}
		return false;
	}

	// Worn gear only. Spares in the bag are sold or put in a stall long before
	// they are worth polishing, and rerolling them would spend the gold the bot
	// needs for its next real upgrade.
	bool ManagePlayerBotBonusReroll(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextBonusCheckTime)
			return false;
		state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_INTERVAL;
		if (ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL)
			return false;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		int stonesUsed = 0;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]) &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++i)
		{
			const BYTE wearCell = wearSlots[i];
			LPITEM item = ch->GetWear(wearCell);
			if (!CanPlayerBotRerollItem(item))
				continue;

			const int count = item->GetAttributeCount();
			const int score = ScorePlayerBotItemBonuses(ch, item, wearCell);

			// An empty line is free power: add before rerolling, always. Only once
			// the item is full does the quality of what it rolled start to matter,
			// and USE_CHANGE_ATTRIBUTE needs at least one line to work on anyway.
			// Five, not four: MAX_NORM_ATTR_NUM is 5 and AddAttribute happily
			// fills the fifth, so stopping at four left a line on the table.
			const bool bWantAdd = count < PLAYERBOT_BONUS_MAX_LINES;
			// An item that has landed the roll its slot is bought for is finished.
			// It can still gain a line - that cannot lose what is already there -
			// but it is never rerolled, whatever the score says.
			const bool bWantChange = !bWantAdd && !HasPlayerBotFinishedBonus(item, wearCell) &&
					score < PLAYERBOT_BONUS_KEEP_SCORE;
			if (!bWantAdd && !bWantChange)
				continue;

			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM
					: PLAYERBOT_BONUS_CHANGE_VNUM;
			if (!BuyPlayerBotBonusStone(ch, stoneVnum))
				continue;

			// The piece has to come off for the engine to touch it, and it has to go
			// back on afterwards - a bot walking around with its weapon in the bag
			// would be worse than any bonus line it could win.
			if (!ch->UnequipItem(item))
				continue;

			if (bWantAdd)
				item->AddAttribute();
			else
				item->ChangeAttribute();

			ConsumePlayerBotBonusStone(ch, stoneVnum);
			++stonesUsed;

			const int newScore = ScorePlayerBotItemBonuses(ch, item, wearCell);
			if (!ch->EquipItem(item))
			{
				sys_err("PLAYERBOT_BONUS: could not re-equip pid=%u name=%s vnum=%u slot=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
						(unsigned int)wearCell);
				continue;
			}

			sys_log(0, "PLAYERBOT_BONUS: %s pid=%u name=%s vnum=%u slot=%u lines=%d->%d score=%d->%d gold=%d",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(),
					item->GetVnum(), (unsigned int)wearCell, count,
					item->GetAttributeCount(), score, newScore,
					(int)(ch->GetGold() / 1000));
		}

		return stonesUsed > 0;
	}
}

#endif
