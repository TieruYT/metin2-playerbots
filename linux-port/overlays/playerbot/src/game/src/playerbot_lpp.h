#ifndef __INC_METIN2_PLAYERBOT_LPP_H__
#define __INC_METIN2_PLAYERBOT_LPP_H__

// Iwakura's Useful Items List ("Lista Przydatnych Przedmiotow", SYSTEM
// OSOBOWOSCI v2.0), under the PERSONA switch: what a bot keeps at the
// storekeeper instead of selling (playerbot_persona_rules.h has the rules).
//
// A kept piece is neither merchant scrap (IsPlayerBotJunkItem) nor counter
// goods (ScorePlayerBotShopStock); one already standing on an offline counter
// comes home on the next service visit (BotOfflineUnwantedLine). The Trader's
// visit - a bag at eighty percent - takes them to the storekeeper with the
// other surplus (CollectPlayerBotSafeboxLpp, beside the books and materials
// in playerbot_town.h), and a stored piece the bot has outgrown, or a plain
// copy of a family it now wears at +9, comes back out for the market, once
// (setLppReleased keeps it from going back down as dead stock).
//
// Which families and how many is the document's: jewellery and boots of tier
// 3 to 6 on his list, the weapons his level bands name (the level-30 set of
// his "30 lvl+" band has the operator's own rules already - the anvil's share,
// the grind for sale - and is left to them), the level-61 shields with
// resistances and the armours of his "70 lvl", and any piece with a tier 5-6
// line rolled at least half-way up; two of a weapon or an armour and three of
// a small piece for the bot's own class, one for another class. The soul
// stones he keeps (every +4, a +3 of PvE tier 3) are held five of a kind and
// go to the box when the bot has no socket for them; the herbs a bot picks up
// go there too under bag pressure, and come back out for the Zielarz.
//
// The box is only seen when it is open, so what it holds is remembered from
// the last visit (TPlayerBotPersona::mapLppStored). Until a visit has looked,
// it counts as empty - which keeps more rather than less, and the visit that
// follows puts it right.
//
// An implementation fragment in the sense playerbot_types.h describes. Include
// it exactly once, after playerbot_gambler.h: the deposit leaves a gambler's
// session alone. playerbot_economy.h and playerbot_town.h forward-declare what
// they ask of it.

namespace
{
	// What the list has moved lately, for the census below.
	unsigned int s_uPlayerBotLppDeposits = 0;
	unsigned int s_uPlayerBotLppReleased = 0;
	unsigned int s_uPlayerBotLppVisits = 0;
	unsigned int s_uPlayerBotLppBoxesFull = 0;

	void NotePlayerBotLppDeposit() { ++s_uPlayerBotLppDeposits; }

	// Iwakura's community patch 2, point 9: the list by name. The jewellery
	// and boots are these twenty-four families (their +0 vnums, read off
	// world.item_proto, where a few names are cut short - "Bransol. Z Bial.
	// Zlota", "Kolczyki Z Niebian.Lez"); every body armour over level 33 and
	// every shield over level 20 is on it too. The weapons are the ones his
	// document's bands already named (PLAYERBOT_LPP_WEAPONS). Kept here rather
	// than in playerbot_persona_tables.h, which his older document renders.
	const DWORD PLAYERBOT_LPP_JEWELS[] = {
		17100, // Ebonitowe Kolczyki
		17200, // Kolczyki z Niebianskich Lez
		14200, // Bransoleta z Niebianskich Lez
		16200, // Naszyjnik z Niebianskich Lez
		15200, // Buty Feniksa
		14040, // Srebrna Bransoleta
		14140, // Bialozlota Bransoleta
		15080, // Skorzane Kozaki
		16060, // Zloty Naszyjnik
		15160, // Ekstazyjne Buty
		17000, // Drewniane Kolczyki
		17020, // Miedziane Kolczyki
		17040, // Srebrne Kolczyki
		17060, // Zlote Kolczyki
		17080, // Jadeitowe Kolczyki
		17160, // Krysztalowe Kolczyki
		17180, // Ametystowe Kolczyki
		16040, // Srebrny Naszyjnik
		16100, // Ebonitowy Naszyjnik
		15040, // Drewniane Buty
		15060, // Buty Wyszywane Zlotem
		15120, // Buty Z Brazu
		15180, // Deszczowe Buty
		15220, // Buty Ognistego Ptaka
	};
	const int PLAYERBOT_LPP_ARMOUR_OVER_LEVEL = 33;
	const int PLAYERBOT_LPP_SHIELD_OVER_LEVEL = 20;

	// The level at which each of his weapon bands begins in this world, read
	// off the rendered table (PLAYERBOT_LPP_WEAPONS): a band is passed when the
	// next one's weapons can be worn.
	BYTE GetPlayerBotLppBandStart(BYTE band)
	{
		BYTE start = 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_LPP_WEAPONS) / sizeof(PLAYERBOT_LPP_WEAPONS[0]); ++i)
			if (PLAYERBOT_LPP_WEAPONS[i].bBand == band &&
					(start == 0 || PLAYERBOT_LPP_WEAPONS[i].bLevel < start))
				start = PLAYERBOT_LPP_WEAPONS[i].bLevel;
		return start;
	}

	bool IsPlayerBotLppListed(const DWORD* list, size_t count, DWORD family)
	{
		for (size_t i = 0; i < count; ++i)
			if (list[i] == family)
				return true;
		return false;
	}

	int GetPlayerBotLppWearCell(LPITEM item)
	{
		if (!item)
			return -1;
		if (item->GetType() == ITEM_WEAPON)
			return item->GetSubType() == WEAPON_ARROW ? -1 : WEAR_WEAPON;
		if (item->GetType() != ITEM_ARMOR)
			return -1;
		switch (item->GetSubType())
		{
			case ARMOR_BODY:   return WEAR_BODY;
			case ARMOR_HEAD:   return WEAR_HEAD;
			case ARMOR_SHIELD: return WEAR_SHIELD;
			case ARMOR_WRIST:  return WEAR_WRIST;
			case ARMOR_FOOTS:  return WEAR_FOOTS;
			case ARMOR_NECK:   return WEAR_NECK;
			case ARMOR_EAR:    return WEAR_EAR;
			default:           return -1;
		}
	}

	DWORD GetPlayerBotLppFamily(LPITEM item)
	{
		return item ? item->GetVnum() - (DWORD)std::max(0, item->GetRefineLevel()) : 0;
	}

	// Whether a piece is on the list at all, and as what; `family` is its +0
	// vnum. The level-30 set is left to its own rules.
	bool ClassifyPlayerBotLppItem(LPCHARACTER ch, LPITEM item, playerbot_persona::TLppPiece& piece,
			DWORD& family)
	{
		piece = playerbot_persona::TLppPiece();
		family = 0;
		if (!ch || !item || !item->GetProto())
			return false;
		const BYTE type = item->GetType();
		if ((type != ITEM_WEAPON && type != ITEM_ARMOR) || GetPlayerBotLppWearCell(item) < 0 ||
				IsPlayerBotSpecialLevel30Weapon(item))
			return false;
		family = GetPlayerBotLppFamily(item);
		piece.level = GetPlayerBotPersonaLevelLimit(item);
		piece.ownClass = IsPlayerBotProtoForCharacter(ch, item->GetProto());
		const int job = piece.ownClass ? (int)ch->GetJob() : -1;
		const BYTE sub = item->GetSubType();
		piece.small = type == ITEM_ARMOR && sub != ARMOR_BODY && sub != ARMOR_HEAD;
		if (type == ITEM_WEAPON)
		{
			for (size_t i = 0; i < sizeof(PLAYERBOT_LPP_WEAPONS) / sizeof(PLAYERBOT_LPP_WEAPONS[0]); ++i)
			{
				const TPlayerBotLppWeapon& row = PLAYERBOT_LPP_WEAPONS[i];
				if (row.dwBaseVnum != family)
					continue;
				// His list is the weapons of tier 3 in PvE; a family his tier
				// sheet rates lower drops out, one it leaves unrated stays.
				const int tier = GetPlayerBotItemTier(family, job, false);
				if (tier != 0 && tier < playerbot_persona::LPP_WEAPON_MIN_TIER)
					break;
				piece.kind = playerbot_persona::LPP_WEAPON;
				piece.band = row.bBand;
				piece.nextBandLevel = GetPlayerBotLppBandStart((BYTE)(row.bBand + 1));
				piece.onlyOne = row.bOnlyOne != 0;
				piece.target = piece.nextBandLevel == 0;
				break;
			}
		}
		else if (sub == ARMOR_WRIST || sub == ARMOR_NECK || sub == ARMOR_EAR || sub == ARMOR_FOOTS)
		{
			// His names since community patch 2 (PLAYERBOT_LPP_JEWELS), where
			// the first document let his tier sheet choose.
			if (IsPlayerBotLppListed(PLAYERBOT_LPP_JEWELS,
					sizeof(PLAYERBOT_LPP_JEWELS) / sizeof(PLAYERBOT_LPP_JEWELS[0]), family))
				piece.kind = playerbot_persona::LPP_JEWEL;
		}
		else if (sub == ARMOR_SHIELD)
		{
			// Every shield over level twenty; the four his first document named
			// stay targets, never outgrown.
			piece.target = IsPlayerBotLppListed(PLAYERBOT_LPP_TARGET_SHIELDS,
					sizeof(PLAYERBOT_LPP_TARGET_SHIELDS) / sizeof(PLAYERBOT_LPP_TARGET_SHIELDS[0]), family);
			if (piece.target || (int)piece.level > PLAYERBOT_LPP_SHIELD_OVER_LEVEL)
				piece.kind = playerbot_persona::LPP_SHIELD;
		}
		else if (sub == ARMOR_BODY)
		{
			// And every body armour over level thirty-three.
			piece.target = IsPlayerBotLppListed(PLAYERBOT_LPP_TARGET_ARMOURS,
					sizeof(PLAYERBOT_LPP_TARGET_ARMOURS) / sizeof(PLAYERBOT_LPP_TARGET_ARMOURS[0]), family);
			if (piece.target || (int)piece.level > PLAYERBOT_LPP_ARMOUR_OVER_LEVEL)
				piece.kind = playerbot_persona::LPP_ARMOUR;
		}
		if (piece.kind == playerbot_persona::LPP_NONE)
		{
			// "Wysoka Wartosc": a line of his tier 5 or 6, rolled half-way up.
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			{
				const BYTE apply = item->GetAttributeType(i);
				if (apply == 0)
					continue;
				const int bonusTier = std::max(GetPlayerBotBonusTier(apply, job, false),
						GetPlayerBotBonusTier(apply, job, true));
				if (playerbot_persona::LppValueLine(bonusTier, item->GetAttributeValue(i),
						GetPlayerBotBonusMaxRoll(item, apply)))
				{
					piece.kind = playerbot_persona::LPP_VALUE;
					break;
				}
			}
		}
		return piece.kind != playerbot_persona::LPP_NONE;
	}

	// How a kept copy ranks against another of its family: the plus first,
	// then how many lines it carries.
	int GetPlayerBotLppRank(LPITEM item)
	{
		int lines = 0;
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0)
				++lines;
		return item ? std::max(0, item->GetRefineLevel()) * 16 + lines : 0;
	}

	// The better copies of its family kept already: what the box holds, then
	// the bag's copies that rank above it (or level with it, in an earlier
	// cell). A piece that is not in the bag - a line on a counter - has every
	// bag copy of its rank or better ahead of it.
	int CountPlayerBotLppKeptAhead(LPCHARACTER ch, const TPlayerBotPersona& p, LPITEM item, DWORD family)
	{
		int ahead = 0;
		std::map<DWORD, BYTE>::const_iterator stored = p.mapLppStored.find(family);
		if (stored != p.mapLppStored.end())
			ahead += stored->second;
		const bool inBag = item->GetWindow() == INVENTORY && !item->IsEquipped() && ch->GetInventoryItem(item->GetCell()) == item;
		const int rank = GetPlayerBotLppRank(item);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->GetCell() != cell || other->IsEquipped() ||
					other->GetType() != item->GetType() || GetPlayerBotLppFamily(other) != family)
				continue;
			const int otherRank = GetPlayerBotLppRank(other);
			if (otherRank > rank || (otherRank == rank && (!inBag || cell < item->GetCell())))
				++ahead;
		}
		return ahead;
	}

	// The family worn at +9: its backups are no longer needed.
	bool IsPlayerBotLppFamilyPerfect(LPCHARACTER ch, LPITEM item, DWORD family)
	{
		const int cell = GetPlayerBotLppWearCell(item);
		LPITEM worn = cell >= 0 ? ch->GetWear((WORD)cell) : NULL;
		return worn && worn != item && GetPlayerBotLppFamily(worn) == family &&
				worn->GetRefineLevel() >= (int)playerbot_persona::LPP_PERFECT_PLUS;
	}

	bool IsPlayerBotLppHerb(LPITEM item)
	{
		return item && item->GetVnum() >= PLAYERBOT_HERB_VNUM_FIRST && item->GetVnum() <= PLAYERBOT_HERB_VNUM_LAST;
	}

	// Whether the bot keeps this piece - in its bag, on its way to the box, or
	// standing on its counter (then it comes home). Soul stones by their own
	// rule, five of a kind counting the box's.
	bool IsPlayerBotLppKeptItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end() || !st->second.persona.bRestored)
			return false;
		const TPlayerBotPersona& p = st->second.persona;
		// A box with no room keeps nothing more: the bag's pieces sell as they
		// always did until a visit finds room again (RefreshPlayerBotLppStored).
		if (p.bLppBoxFull)
			return false;
		if (item->GetType() == ITEM_METIN)
		{
			const DWORD vnum = item->GetVnum();
			std::map<DWORD, BYTE>::const_iterator stored = p.mapLppStored.find(vnum);
			const int heldAhead = (stored != p.mapLppStored.end() ? stored->second : 0) +
					(item->GetWindow() == INVENTORY ? CountPlayerBotVnumUnitsAhead(ch, item) : 0);
			return playerbot_persona::LppKeepsStone(GetPlayerBotSoulStoneGrade(vnum),
					GetPlayerBotSoulStoneTier(vnum, false), heldAhead);
		}
		playerbot_persona::TLppPiece piece;
		DWORD family = 0;
		if (!ClassifyPlayerBotLppItem(ch, item, piece, family))
			return false;
		return playerbot_persona::LppKeeps(piece, (int)ch->GetLevel(),
				IsPlayerBotLppFamilyPerfect(ch, item, family),
				CountPlayerBotLppKeptAhead(ch, p, item, family));
	}

	// A piece on the list the list does not keep - past its limit, outgrown,
	// or kept by a box with no room - is counter goods, never the merchant's:
	// "wszystko ponad ten limit musi natychmiast trafic na sklep, aby inni
	// Hazardzisci mogli je odkupic i ulepszac" (community patch 2, point 9).
	bool IsPlayerBotLppSurplusGoods(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || !IsPlayerBotPersonaEnabled())
			return false;
		playerbot_persona::TLppPiece piece;
		DWORD family = 0;
		return ClassifyPlayerBotLppItem(ch, item, piece, family) && !IsPlayerBotLppKeptItem(ch, item);
	}

	// A piece in the box the list no longer keeps: outgrown, or a plain copy
	// of a family now worn at +9. A piece that is not on the list at all - the
	// dead stock an older rule put down - is left where it is.
	bool IsPlayerBotLppStoredSurplus(LPCHARACTER ch, LPITEM item)
	{
		playerbot_persona::TLppPiece piece;
		DWORD family = 0;
		if (!ClassifyPlayerBotLppItem(ch, item, piece, family))
			return false;
		return playerbot_persona::LppObsolete(piece, (int)ch->GetLevel()) ||
				playerbot_persona::LppLimit(piece, IsPlayerBotLppFamilyPerfect(ch, item, family)) == 0;
	}

	// What goes down on this visit: the kept pieces the bag has no use for
	// now - at every visit since community patch 2 ("obowiazek chowac DO
	// MAGAZYNU, a nie trzymac w ekwipunku jak dotychczas"); it used to wait
	// for a bag at eighty percent or under pressure, like every collector
	// beside it, and the bags held the list's pieces for good. Never a piece
	// the bot is about to wear, work at the anvil, or carry as its backup or
	// its stone weapon; never a stone it has a socket for; never a herb the
	// Zielarz is going to brew; nothing while a gambler's session is on.
	void CollectPlayerBotSafeboxLpp(LPCHARACTER ch, const TPlayerBotAIState& state, std::vector<WORD>& cells)
	{
		cells.clear();
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored ||
				state.persona.bLppBoxFull || IsPlayerBotGambling(state, get_dword_time()))
			return;
		// The herbs keep the old rule: they go down under bag pressure only.
		const bool pressure = IsPlayerBotBagFull(ch) ||
				CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
		LPITEM backup = FindPlayerBotBackupWeapon(ch);
		LPITEM stoneWeapon = FindPlayerBotStoneWeapon(ch, false);
		const bool zielarz = IsPlayerBotZielarz(ch);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->isLocked() ||
					GetPlayerBotItemPolicy(item) != PLAYERBOT_ITEM_POLICY_NONE)
				continue;
			if (IsPlayerBotLppHerb(item))
			{
				if (!zielarz && pressure)
					cells.push_back(cell);
				continue;
			}
			if (item->GetType() == ITEM_METIN)
			{
				if (IsPlayerBotLppKeptItem(ch, item) &&
						!CanPlayerBotSeatSoulStone(ch, item->GetVnum(), (DWORD)item->GetValue(5)))
					cells.push_back(cell);
				continue;
			}
			if (item == backup || item == stoneWeapon || !IsPlayerBotLppKeptItem(ch, item))
				continue;
			if (IsPlayerBotWearableUpgrade(ch, item, cell) || IsPlayerBotHigherTierSpare(ch, item) ||
					IsPlayerBotRefineBagCandidate(ch, item))
				continue;
			cells.push_back(cell);
		}
	}

	// What the box holds of each kept family, counted while it is open.
	void RefreshPlayerBotLppStored(LPCHARACTER ch, TPlayerBotPersona& p, CSafebox* box)
	{
		if (!ch || !box)
			return;
		p.mapLppStored.clear();
		int freeCells = 0;
		for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM; ++pos)
		{
			if (!box->IsValidPosition(pos))
				continue;
			if (box->IsEmpty(pos, 1))
				++freeCells;
			LPITEM item = box->Get(pos);
			if (!item)
				continue;
			if (item->GetType() == ITEM_METIN)
			{
				const int grade = GetPlayerBotSoulStoneGrade(item->GetVnum());
				if (playerbot_persona::LppKeepsStone(grade, GetPlayerBotSoulStoneTier(item->GetVnum(), false), 0))
				{
					BYTE& n = p.mapLppStored[item->GetVnum()];
					n = (BYTE)std::min<int>(255, n + std::max<int>(1, item->GetCount()));
				}
				continue;
			}
			playerbot_persona::TLppPiece piece;
			DWORD family = 0;
			if (!ClassifyPlayerBotLppItem(ch, item, piece, family) ||
					playerbot_persona::LppObsolete(piece, (int)ch->GetLevel()))
				continue;
			BYTE& n = p.mapLppStored[family];
			if (n < 255)
				++n;
		}
		p.bLppStoredKnown = true;
		++s_uPlayerBotLppVisits;
		const bool wasFull = p.bLppBoxFull;
		p.bLppBoxFull = freeCells < PLAYERBOT_LPP_BOX_MIN_FREE_CELLS;
		if (p.bLppBoxFull)
			++s_uPlayerBotLppBoxesFull;
		if (p.bLppBoxFull != wasFull)
			sys_log(0, "PLAYERBOT_LPP: box %s pid=%u name=%s free_cells=%d families=%u",
					p.bLppBoxFull ? "full, the bag's pieces sell" : "has room again",
					ch->GetPlayerID(), ch->GetName(), freeCells, (unsigned int)p.mapLppStored.size());
	}

	// A stored piece released for the market: remembered so the dead-stock
	// rule, which sends a piece nobody buys to the box, does not send it back
	// down - the list let it go once, and a counter's discount is its way out.
	void NotePlayerBotLppReleased(TPlayerBotPersona& p, DWORD itemId)
	{
		++s_uPlayerBotLppReleased;
		if (p.setLppReleased.size() >= 64)
			p.setLppReleased.erase(p.setLppReleased.begin());
		p.setLppReleased.insert(itemId);
	}

	// Every ten minutes: what the list kept, let go and could not fit.
	void ReportPlayerBotLppCensus(DWORD dwNow)
	{
		static DWORD s_dwReported = 0;
		if (s_dwReported != 0 && dwNow - s_dwReported < 600000)
			return;
		const bool first = s_dwReported == 0;
		s_dwReported = dwNow;
		if (first || !IsPlayerBotPersonaEnabled())
			return;
		sys_log(0, "PLAYERBOT_LPP: census deposits=%u released=%u visits=%u boxes_full=%u",
				s_uPlayerBotLppDeposits, s_uPlayerBotLppReleased, s_uPlayerBotLppVisits,
				s_uPlayerBotLppBoxesFull);
		s_uPlayerBotLppDeposits = s_uPlayerBotLppReleased = 0;
		s_uPlayerBotLppVisits = s_uPlayerBotLppBoxesFull = 0;
	}

	bool IsPlayerBotLppReleased(LPCHARACTER ch, DWORD itemId)
	{
		TPlayerBotAIStateMap::const_iterator st = ch ? s_mapPlayerBotAIStates.find(ch->GetPlayerID())
				: s_mapPlayerBotAIStates.end();
		return st != s_mapPlayerBotAIStates.end() &&
				st->second.persona.setLppReleased.find(itemId) != st->second.persona.setLppReleased.end();
	}
}

#endif
