#ifndef __INC_METIN2_PLAYERBOT_WORLD_MEMORY_H__
#define __INC_METIN2_PLAYERBOT_WORLD_MEMORY_H__

// What the bot population has learned about the world, as opposed to what any
// one bot knows about itself. Written by whoever makes the observation and read
// by whoever needs it later, which is why it cannot live inside either of them.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, from playerbot_manager.cpp, before the subsystems that read
// it.

namespace
{
	enum EPlayerBotRaceSlot
	{
		PLAYERBOT_RACE_ANIMAL = 0,
		PLAYERBOT_RACE_UNDEAD,
		PLAYERBOT_RACE_DEVIL,
		PLAYERBOT_RACE_ORC,
		PLAYERBOT_RACE_MILGYO,
		PLAYERBOT_RACE_SLOTS,
		PLAYERBOT_RACE_NONE = -1
	};

	// What the population has learned about each map: which kind of monster
	// actually lives there. Shared across every bot, because it is a fact about
	// the world rather than about any one character. Feeds equipment scoring, so
	// a race-attack bonus is worth more where that race is what you fight.
	struct TPlayerBotMapRaces
	{
		DWORD dwSamples;
		DWORD dwByRace[PLAYERBOT_RACE_SLOTS];
		TPlayerBotMapRaces() : dwSamples(0) { memset(dwByRace, 0, sizeof(dwByRace)); }
	};
	typedef std::map<long, TPlayerBotMapRaces> TPlayerBotMapRaceMap;
	TPlayerBotMapRaceMap s_mapRaceMemory;

	void RememberPlayerBotMapRace(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !target->IsMonster())
			return;
		TPlayerBotMapRaces& mem = s_mapRaceMemory[ch->GetMapIndex()];
		++mem.dwSamples;
		if (target->IsRaceFlag(RACE_FLAG_ANIMAL)) ++mem.dwByRace[PLAYERBOT_RACE_ANIMAL];
		if (target->IsRaceFlag(RACE_FLAG_UNDEAD)) ++mem.dwByRace[PLAYERBOT_RACE_UNDEAD];
		if (target->IsRaceFlag(RACE_FLAG_DEVIL))  ++mem.dwByRace[PLAYERBOT_RACE_DEVIL];
		if (target->IsRaceFlag(RACE_FLAG_ORC))    ++mem.dwByRace[PLAYERBOT_RACE_ORC];
		if (target->IsRaceFlag(RACE_FLAG_MILGYO)) ++mem.dwByRace[PLAYERBOT_RACE_MILGYO];
	}

	BYTE GetPlayerBotRaceApplyType(int race)
	{
		switch (race)
		{
			case PLAYERBOT_RACE_ANIMAL: return APPLY_ATTBONUS_ANIMAL;
			case PLAYERBOT_RACE_UNDEAD: return APPLY_ATTBONUS_UNDEAD;
			case PLAYERBOT_RACE_DEVIL:  return APPLY_ATTBONUS_DEVIL;
			case PLAYERBOT_RACE_ORC:    return APPLY_ATTBONUS_ORC;
			case PLAYERBOT_RACE_MILGYO: return APPLY_ATTBONUS_MILGYO;
			default: return APPLY_NONE;
		}
	}

	// What the market actually paid, per item and refine. The last few unit
	// prices and when the last one happened; the median of those is the price,
	// and how long ago the last sale was is the demand. A stall used to ask
	// merchant-unit-price times three for everything, which is a fact about the
	// merchant, not about whether any bot wants the thing.
	struct TPlayerBotSaleMemory
	{
		DWORD dwUnitPrice[PLAYERBOT_SALE_MEMORY];
		BYTE bCount;
		BYTE bNext;
		DWORD dwLastSaleTime;
		TPlayerBotSaleMemory() : bCount(0), bNext(0), dwLastSaleTime(0)
		{
			memset(dwUnitPrice, 0, sizeof(dwUnitPrice));
		}
	};
	typedef std::map<DWORD, TPlayerBotSaleMemory> TPlayerBotSaleMap;
	TPlayerBotSaleMap s_mapSaleMemory;

	DWORD PlayerBotSaleKey(DWORD vnum, BYTE refine)
	{
		return vnum * 16 + (refine & 15);
	}

	void RememberPlayerBotSale(DWORD vnum, BYTE refine, DWORD unitPrice, DWORD dwNow)
	{
		if (vnum == 0 || unitPrice == 0)
			return;
		TPlayerBotSaleMemory& mem = s_mapSaleMemory[PlayerBotSaleKey(vnum, refine)];
		mem.dwUnitPrice[mem.bNext] = unitPrice;
		mem.bNext = (BYTE)((mem.bNext + 1) % PLAYERBOT_SALE_MEMORY);
		if (mem.bCount < PLAYERBOT_SALE_MEMORY)
			++mem.bCount;
		mem.dwLastSaleTime = dwNow;
	}

	// The unit price the market has shown it will pay, nudged by how recently
	// it paid it - or 0 while there are not enough sales to say. A median
	// rather than a mean, so one bot overpaying once does not move it.
	DWORD GetPlayerBotSaleUnitPrice(DWORD vnum, BYTE refine, DWORD dwNow, size_t* pSamples)
	{
		if (pSamples)
			*pSamples = 0;
		TPlayerBotSaleMap::const_iterator it = s_mapSaleMemory.find(PlayerBotSaleKey(vnum, refine));
		if (it == s_mapSaleMemory.end() || it->second.bCount < PLAYERBOT_SALE_MIN_SAMPLES)
			return 0;
		const TPlayerBotSaleMemory& mem = it->second;
		std::vector<DWORD> sorted(mem.dwUnitPrice, mem.dwUnitPrice + mem.bCount);
		std::sort(sorted.begin(), sorted.end());
		DWORD median = sorted[sorted.size() / 2];
		if (pSamples)
			*pSamples = mem.bCount;
		const DWORD since = dwNow - mem.dwLastSaleTime;
		if (since <= PLAYERBOT_SALE_RECENT)
			median = median * 115 / 100;
		else if (since >= PLAYERBOT_SALE_STALE)
			median = median * 85 / 100;
		return std::max<DWORD>(1, median);
	}

	// The race a map is made of, or PLAYERBOT_RACE_NONE while the sample is too
	// small or too mixed to call. A guess made from ten kills is worse than none.
	int GetPlayerBotDominantRace(long mapIndex)
	{
		TPlayerBotMapRaceMap::const_iterator it = s_mapRaceMemory.find(mapIndex);
		if (it == s_mapRaceMemory.end() || it->second.dwSamples < 200)
			return PLAYERBOT_RACE_NONE;
		int best = PLAYERBOT_RACE_NONE;
		DWORD bestCount = 0;
		for (int race = 0; race < PLAYERBOT_RACE_SLOTS; ++race)
		{
			if (it->second.dwByRace[race] > bestCount)
			{
				bestCount = it->second.dwByRace[race];
				best = race;
			}
		}
		// Half the encounters have to agree before this counts as "the" race.
		return (bestCount * 2 >= it->second.dwSamples) ? best : PLAYERBOT_RACE_NONE;
	}
}

#endif
