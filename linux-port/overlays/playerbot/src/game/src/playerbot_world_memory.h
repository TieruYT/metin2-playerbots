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

	// The one race a kill pays for, exactly as battle.cpp picks it: an else-if
	// chain in this order, stopping at the first flag the monster carries. The
	// five races below it in that chain - INSECT, FIRE, ICE, DESERT, TREE - have
	// a POINT and no APPLY, so nothing a bot can wear reaches them and a fight
	// with one of them is a fight that paid nothing.
	int GetPlayerBotTargetRaceSlot(LPCHARACTER target)
	{
		if (!target)
			return PLAYERBOT_RACE_OTHER;
		if (target->IsRaceFlag(RACE_FLAG_ANIMAL)) return PLAYERBOT_RACE_ANIMAL;
		if (target->IsRaceFlag(RACE_FLAG_UNDEAD)) return PLAYERBOT_RACE_UNDEAD;
		if (target->IsRaceFlag(RACE_FLAG_DEVIL))  return PLAYERBOT_RACE_DEVIL;
		if (target->IsRaceFlag(RACE_FLAG_HUMAN))  return PLAYERBOT_RACE_HUMAN;
		if (target->IsRaceFlag(RACE_FLAG_ORC))    return PLAYERBOT_RACE_ORC;
		if (target->IsRaceFlag(RACE_FLAG_MILGYO)) return PLAYERBOT_RACE_MILGYO;
		return PLAYERBOT_RACE_OTHER;
	}

	void RememberPlayerBotMapRace(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !target->IsMonster())
			return;
		const int slot = GetPlayerBotTargetRaceSlot(target);
		TPlayerBotMapRaces& mem = s_mapRaceMemory[ch->GetMapIndex()];
		++mem.dwSamples;
		++mem.dwByRace[slot];

		// And the bot's own account of it. The map is an approximation - the
		// desert has scorpions beside its undead, the valley orcs beside its
		// mystics - and the concrete target decides what a race bonus is worth
		// to the bot that wears it. Halved every ten minutes so a move to the
		// other end of the map is forgotten in half an hour.
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return;
		TPlayerBotAIState& state = it->second;
		const DWORD dwNow = get_dword_time();
		if (state.dwRaceHistogramStamp == 0)
			state.dwRaceHistogramStamp = dwNow;
		while (dwNow - state.dwRaceHistogramStamp >= PLAYERBOT_RACE_HISTOGRAM_DECAY)
		{
			for (int r = 0; r < PLAYERBOT_RACE_HISTOGRAM_SLOTS; ++r)
				state.awRaceHistogram[r] /= 2;
			state.dwRaceHistogramStamp += PLAYERBOT_RACE_HISTOGRAM_DECAY;
		}
		if (state.awRaceHistogram[slot] < 60000)
			++state.awRaceHistogram[slot];
	}

	// The race this bot has actually been fighting, when it has fought enough
	// to say; the map's aggregate until then. The slots are the enum's order.
	int GetPlayerBotFightingRace(LPCHARACTER ch, int* percentOut = NULL);

	BYTE GetPlayerBotRaceApplyType(int race)
	{
		switch (race)
		{
			case PLAYERBOT_RACE_ANIMAL: return APPLY_ATTBONUS_ANIMAL;
			case PLAYERBOT_RACE_UNDEAD: return APPLY_ATTBONUS_UNDEAD;
			case PLAYERBOT_RACE_DEVIL:  return APPLY_ATTBONUS_DEVIL;
			case PLAYERBOT_RACE_HUMAN:  return APPLY_ATTBONUS_HUMAN;
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

	// One key per commodity, and a skill book is not one commodity.
	//
	// vnum*16+refine put every ordinary book on the same market: 50300 is the
	// vnum whatever skill sits in its socket, so a cheap sale of somebody's
	// spare anchored Aura Miecza, and a sale of Aura moved the price of every
	// other book. Skills run 1..111, which is seven bits.
	DWORD PlayerBotSaleKey(DWORD vnum, BYTE refine, DWORD skillVnum = 0)
	{
		return (vnum * 16 + (refine & 15)) * 128 + (skillVnum & 127);
	}

	void RememberPlayerBotSale(DWORD vnum, BYTE refine, DWORD unitPrice, DWORD dwNow,
			DWORD skillVnum = 0)
	{
		if (vnum == 0 || unitPrice == 0)
			return;
		TPlayerBotSaleMemory& mem = s_mapSaleMemory[PlayerBotSaleKey(vnum, refine, skillVnum)];
		mem.dwUnitPrice[mem.bNext] = unitPrice;
		mem.bNext = (BYTE)((mem.bNext + 1) % PLAYERBOT_SALE_MEMORY);
		if (mem.bCount < PLAYERBOT_SALE_MEMORY)
			++mem.bCount;
		mem.dwLastSaleTime = dwNow;
	}

	// The unit price the market has shown it will pay, nudged by how recently
	// it paid it - or 0 while there are not enough sales to say. A median
	// rather than a mean, so one bot overpaying once does not move it.
	DWORD GetPlayerBotSaleUnitPrice(DWORD vnum, BYTE refine, DWORD dwNow, size_t* pSamples,
			DWORD skillVnum = 0)
	{
		if (pSamples)
			*pSamples = 0;
		TPlayerBotSaleMap::const_iterator it =
				s_mapSaleMemory.find(PlayerBotSaleKey(vnum, refine, skillVnum));
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

	// The ledger: how many units of a thing stand on open counters and how
	// many bots are short of it, rebuilt once a minute by
	// RefreshPlayerBotMarketLedger in playerbot_market.h from every stall and
	// every bag. Both are this minute's count, not a forecast: a bot short of a
	// material stays short until it buys, and the world has no clock a
	// forecast could run on. Before this, a counter carried every spare
	// material its keeper had, so a market of forty stalls was thirty stalls
	// of the same three things nobody was short of.
	//
	// What it cannot see is a human. A player buying from a counter goes
	// through CShopManager without touching this code, so a material a player
	// clears out every evening reads here as unsold. The sale memory above has
	// the same blind spot; both say so rather than pretend otherwise.
	struct TPlayerBotMarketLedgerEntry
	{
		DWORD dwSupplyUnits;
		DWORD dwSupplyStalls;
		DWORD dwDemandBots;
		TPlayerBotMarketLedgerEntry() : dwSupplyUnits(0), dwSupplyStalls(0), dwDemandBots(0)
		{
		}
	};
	typedef std::map<DWORD, TPlayerBotMarketLedgerEntry> TPlayerBotMarketLedger;
	TPlayerBotMarketLedger s_mapMarketLedger;
	// The same units by the map their counter stands on. A player - and the
	// item finder - sees one map's counters, and a core-wide count could call
	// a material plentiful while whole villages had none of it: on m2zip on
	// 18 September 167 of 564 pairs of a recipe material the bots held two
	// hundred of and a village had nothing on sale, Czarny Uniform 62 065 in
	// bags and none in Pyongmoo or Bakra.
	std::map<unsigned long long, DWORD> s_mapMarketLocalSupply;

	unsigned long long PlayerBotMarketLocalKey(long lMapIndex, DWORD vnum)
	{
		return ((unsigned long long)(DWORD)lMapIndex << 32) | vnum;
	}

	DWORD GetPlayerBotMarketLocalSupply(long lMapIndex, DWORD vnum)
	{
		std::map<unsigned long long, DWORD>::const_iterator it =
				s_mapMarketLocalSupply.find(PlayerBotMarketLocalKey(lMapIndex, vnum));
		return it == s_mapMarketLocalSupply.end() ? 0 : it->second;
	}
	DWORD s_dwMarketLedgerTime = 0;
	DWORD s_dwMarketReportTime = 0;
	// The median of what a shopping bot has to spend, from the same walk. Zero
	// until the first refresh, and the counters ask the merchant's markup alone.
	DWORD s_dwMarketMedianWallet = 0;

	DWORD GetPlayerBotMarketMedianWallet()
	{
		return s_dwMarketMedianWallet;
	}

	// What the listing decision said about a material, counted for the
	// ten-minute report. The names are the reason codes an operator reads in
	// PLAYERBOT_MARKET: held lines.
	enum EPlayerBotListDecision
	{
		PLAYERBOT_LIST_LIST = 0,
		PLAYERBOT_LIST_PROBE,
		PLAYERBOT_LIST_NO_DEMAND,
		PLAYERBOT_LIST_OVERSTOCK,
		// Listed because this village's counters hold less than a player's
		// floor of it, whatever the bots are short of.
		PLAYERBOT_LIST_FLOOR,
		PLAYERBOT_LIST_DECISIONS
	};
	DWORD s_auMarketDecisions[PLAYERBOT_LIST_DECISIONS] = { 0, 0, 0, 0, 0 };
	// When each bot's decisions were last counted. The bag is scored again on
	// every tick of the walk to the pitch - four times a second for half a
	// minute - and counting each of those made one keeper with five held
	// materials read as sixteen hundred refusals a minute.
	std::map<DWORD, DWORD> s_mapMarketDecisionStamp;

	bool ShouldReportPlayerBotMarketDecisions(DWORD pid, DWORD dwNow)
	{
		DWORD& stamp = s_mapMarketDecisionStamp[pid];
		if (stamp != 0 && dwNow - stamp < PLAYERBOT_MARKET_LEDGER_INTERVAL)
			return false;
		stamp = dwNow;
		return true;
	}
	const char* const s_apszMarketDecisionNames[PLAYERBOT_LIST_DECISIONS] = {
		"LIST", "PROBE", "NO_DEMAND", "OVERSTOCK", "FLOOR"
	};

	const TPlayerBotMarketLedgerEntry* GetPlayerBotMarketLedgerEntry(DWORD vnum)
	{
		TPlayerBotMarketLedger::const_iterator it = s_mapMarketLedger.find(vnum);
		return it == s_mapMarketLedger.end() ? NULL : &it->second;
	}

	// The weapons of PLAYERBOT_JUNK_WEAPON_BASES at +0..+3 standing on the
	// bots' counters (a person's counter is not counted): recounted with the
	// ledger once a minute and moved at once by every line put up or taken
	// off in between, so thirty keepers in one minute do not each find the
	// market empty of them.
	int s_iPlayerBotJunkWeaponsOnCounters = 0;

	void NotePlayerBotJunkWeaponOnCounter(DWORD vnum, int units)
	{
		if (IsPlayerBotJunkWeaponVnum(vnum))
			s_iPlayerBotJunkWeaponsOnCounters = std::max(0, s_iPlayerBotJunkWeaponsOnCounters + units);
	}

	bool IsPlayerBotJunkWeaponMarketFull()
	{
		return s_iPlayerBotJunkWeaponsOnCounters >= PLAYERBOT_JUNK_WEAPON_MARKET_CAP;
	}

	// A stall that has just opened goes on the ledger at once rather than at
	// the next refresh: three keepers scoring the same material in the same
	// minute would otherwise each see the counters empty of it and all three
	// put it up.
	void AddPlayerBotMarketSupply(DWORD vnum, WORD count, long lMapIndex)
	{
		if (vnum == 0 || count == 0)
			return;
		TPlayerBotMarketLedgerEntry& entry = s_mapMarketLedger[vnum];
		entry.dwSupplyUnits += count;
		++entry.dwSupplyStalls;
		if (lMapIndex > 0)
			s_mapMarketLocalSupply[PlayerBotMarketLocalKey(lMapIndex, vnum)] += count;
	}

	// The unit price the world's counters last asked for a thing, keyed like
	// the sale memory, so the next counter asks within a step of it.
	struct TPlayerBotAskMemory
	{
		DWORD dwUnit;
		DWORD dwAskTime;
		DWORD dwMovedTime;
		TPlayerBotAskMemory() : dwUnit(0), dwAskTime(0), dwMovedTime(0)
		{
		}
	};
	typedef std::map<DWORD, TPlayerBotAskMemory> TPlayerBotAskMap;
	TPlayerBotAskMap s_mapAskMemory;

	// The yang rate both memories were learned under. Every price Iwakura's
	// sheet sets is scaled by the rate, and the memories hold plain yang: the
	// ask anchor moves five percent per ten minutes and the sale median blends
	// in whatever was paid, so after the operator moved mob_gold the counters
	// went on asking the old rate's numbers for hours - a zero too many, or one
	// too few. A new rate is a new market, so both are forgotten.
	int s_iPlayerBotPriceRate = 0;

	void ForgetPlayerBotPricesOnRateChange()
	{
		const int rate = CHARACTER_MANAGER::instance().GetMobGoldAmountRate(NULL);
		if (rate == s_iPlayerBotPriceRate)
			return;
		if (s_iPlayerBotPriceRate != 0)
		{
			sys_log(0, "PLAYERBOT_MARKET: yang rate changed from %d to %d, forgetting asks=%u sales=%u",
					s_iPlayerBotPriceRate, rate, (unsigned int)s_mapAskMemory.size(),
					(unsigned int)s_mapSaleMemory.size());
			s_mapAskMemory.clear();
			s_mapSaleMemory.clear();
		}
		else
			sys_log(0, "PLAYERBOT_MARKET: yang rate %d", rate);
		s_iPlayerBotPriceRate = rate;
	}

	DWORD GetPlayerBotLastAsk(DWORD vnum, BYTE refine, DWORD dwNow)
	{
		TPlayerBotAskMap::const_iterator it = s_mapAskMemory.find(PlayerBotSaleKey(vnum, refine));
		if (it == s_mapAskMemory.end() || it->second.dwUnit == 0 ||
				dwNow - it->second.dwAskTime >= PLAYERBOT_MARKET_ASK_STALE)
			return 0;
		return it->second.dwUnit;
	}

	// What a counter may ask now, given what the last one asked: within
	// PLAYERBOT_MARKET_STEP_PERCENT of it per PLAYERBOT_MARKET_STEP_INTERVAL
	// since the price last moved, so the market's price of a thing drifts at a
	// bounded rate however far the regulator points. Forty stalls each stepping
	// five percent from the one before would otherwise walk a price sevenfold
	// in the ten minutes they take to open. An hour with no counter asking at
	// all and the memory is dropped: the next ask starts fresh.
	DWORD LimitPlayerBotAskStep(DWORD vnum, BYTE refine, DWORD wanted, DWORD dwNow,
			DWORD skillVnum = 0)
	{
		// Before the reference below is taken: a new rate clears the map.
		ForgetPlayerBotPricesOnRateChange();
		TPlayerBotAskMemory& mem = s_mapAskMemory[PlayerBotSaleKey(vnum, refine, skillVnum)];
		// An anchor under the floor is not a price to step away from, it is an
		// accident to forget. One yang got onto the counters because the median
		// wallet is zero until the ledger has run for the first time, and in
		// that first minute anything without a merchant price came out at
		// max(1, 0); after that the anchor could never move, because five
		// percent of one yang is nothing in integer arithmetic and every stall
		// that listed the item kept the memory too fresh to go stale.
		if (mem.dwUnit == 0 || mem.dwUnit < PLAYERBOT_MARKET_ASK_FLOOR ||
				dwNow - mem.dwAskTime >= PLAYERBOT_MARKET_ASK_STALE)
		{
			mem.dwUnit = wanted;
			mem.dwAskTime = mem.dwMovedTime = dwNow;
			return wanted;
		}
		mem.dwAskTime = dwNow;
		// Whole intervals only. The "1 +" here gave every call a free step even
		// when no time had passed, and each accepted step reset the clock - so
		// forty counters opening in the same minute moved the shared anchor
		// forty times, whatever the comment about five percent per ten minutes
		// said. A price is set once and then moves with the clock.
		const DWORD steps = std::min<DWORD>(PLAYERBOT_MARKET_STEP_MAX_STEPS,
				(dwNow - mem.dwMovedTime) / PLAYERBOT_MARKET_STEP_INTERVAL);
		const DWORD span = PLAYERBOT_MARKET_STEP_PERCENT * steps;
		// At least one yang of movement per whole interval. A purely
		// multiplicative step cannot leave any anchor below four, and the point
		// of a step limiter is to slow a price down, not to hold one still.
		const DWORD move = steps == 0 ? 0
				: std::max<DWORD>(steps, mem.dwUnit * span / 100);
		const DWORD lo = mem.dwUnit > move ? mem.dwUnit - move : 1;
		const DWORD hi = mem.dwUnit + move;
		const DWORD unit = std::min(hi, std::max(lo, wanted));
		if (unit != mem.dwUnit)
		{
			mem.dwUnit = unit;
			mem.dwMovedTime = dwNow;
		}
		return unit;
	}

	// How hot a commodity is: how many times in a row it has left a counter
	// almost as soon as it was put there. Iwakura's "wysoki popyt" - the bot
	// notices and asks more next time, and keeps asking more while it keeps
	// happening.
	//
	// Keyed like the sale memory, so a skill book counts per skill. This is a
	// market-wide count on purpose: what it measures is how fast buyers take
	// the thing, which is a fact about the thing and not about the keeper.
	struct TPlayerBotDemandMemory
	{
		BYTE bFastSales;
		DWORD dwLastFastSale;
		TPlayerBotDemandMemory() : bFastSales(0), dwLastFastSale(0) {}
	};
	typedef std::map<DWORD, TPlayerBotDemandMemory> TPlayerBotDemandMap;
	TPlayerBotDemandMap s_mapDemandMemory;

	void NotePlayerBotFastSale(DWORD vnum, BYTE refine, DWORD dwNow, DWORD skillVnum = 0)
	{
		if (vnum == 0)
			return;
		TPlayerBotDemandMemory& mem = s_mapDemandMemory[PlayerBotSaleKey(vnum, refine, skillVnum)];
		// A rush that stopped an hour ago is not a rush. Counted from the last
		// quick sale rather than decremented on a timer, because nothing here
		// runs on a clock of its own.
		if (mem.dwLastFastSale != 0 && dwNow - mem.dwLastFastSale >= PLAYERBOT_MARKET_DEMAND_DECAY)
			mem.bFastSales = 0;
		if (mem.bFastSales < PLAYERBOT_MARKET_DEMAND_MAX_STEPS)
			++mem.bFastSales;
		mem.dwLastFastSale = dwNow;
	}

	// What to add to this keeper's asking price, in percent, or zero. Drawn per
	// listing inside Iwakura's band, so two counters of a wanted thing do not
	// show the same number.
	int GetPlayerBotDemandPercent(DWORD vnum, BYTE refine, DWORD dwNow, DWORD skillVnum = 0)
	{
		TPlayerBotDemandMap::const_iterator it =
				s_mapDemandMemory.find(PlayerBotSaleKey(vnum, refine, skillVnum));
		if (it == s_mapDemandMemory.end() || it->second.bFastSales == 0)
			return 0;
		if (dwNow - it->second.dwLastFastSale >= PLAYERBOT_MARKET_DEMAND_DECAY)
			return 0;
		return (int)it->second.bFastSales *
				number(PLAYERBOT_MARKET_DEMAND_MIN_PERCENT, PLAYERBOT_MARKET_DEMAND_MAX_PERCENT);
	}

	// The race a map is made of, as the population has seen it, or
	// PLAYERBOT_RACE_NONE while the sample is too small or too mixed to call. A
	// guess made from ten kills is worse than none. Only a fallback now: for
	// every map a bot may stand on, PLAYERBOT_MAP_RACE_TABLE already holds the
	// answer counted off the spawn files, and this is what answers for a map
	// added to the frontier before its row was measured.
	int GetPlayerBotDominantRace(long mapIndex, int* percentOut)
	{
		TPlayerBotMapRaceMap::const_iterator it = s_mapRaceMemory.find(mapIndex);
		if (it == s_mapRaceMemory.end() || it->second.dwSamples < 200)
			return PLAYERBOT_RACE_NONE;
		int best = PLAYERBOT_RACE_NONE;
		DWORD bestCount = 0;
		for (int race = 0; race < PLAYERBOT_RACE_SLOTS; ++race)
		{
			// A fight that paid no race is counted, so that the desert can say
			// so - but it is never the answer.
			if (race == PLAYERBOT_RACE_OTHER)
				continue;
			if (it->second.dwByRace[race] > bestCount)
			{
				bestCount = it->second.dwByRace[race];
				best = race;
			}
		}
		if (best == PLAYERBOT_RACE_NONE)
			return PLAYERBOT_RACE_NONE;
		const int percent = (int)(bestCount * 100 / it->second.dwSamples);
		// A quarter of the map is enough to be worth a line: on Mount Sohan the
		// undead are 46% and the rest is ice, which pays nothing at all, so
		// "half the encounters must agree" would have thrown away the only line
		// that works there.
		if (percent < PLAYERBOT_RACE_WORTH_PERCENT)
			return PLAYERBOT_RACE_NONE;
		if (percentOut)
			*percentOut = percent;
		return best;
	}

	// The race this bot is actually being paid for, and how much of its fighting
	// that is. Its own history first, because a bot camped on Orc Valley's
	// Fanatic islands fights mystics on a map that is mostly orcs; the map's
	// measured table when it has not fought enough to say; and what the
	// population has seen for a map with no row.
	int GetPlayerBotFightingRace(LPCHARACTER ch, int* percentOut)
	{
		if (percentOut)
			*percentOut = 0;
		if (!ch)
			return PLAYERBOT_RACE_NONE;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it != s_mapPlayerBotAIStates.end())
		{
			DWORD total = 0;
			int best = PLAYERBOT_RACE_NONE;
			WORD bestCount = 0;
			for (int r = 0; r < PLAYERBOT_RACE_HISTOGRAM_SLOTS; ++r)
			{
				total += it->second.awRaceHistogram[r];
				if (r == PLAYERBOT_RACE_OTHER)
					continue;
				if (it->second.awRaceHistogram[r] > bestCount)
				{
					bestCount = it->second.awRaceHistogram[r];
					best = r;
				}
			}
			if (total >= PLAYERBOT_RACE_HISTOGRAM_MIN_SAMPLES)
			{
				const int percent = (int)((DWORD)bestCount * 100 / total);
				if (best == PLAYERBOT_RACE_NONE || percent < PLAYERBOT_RACE_WORTH_PERCENT)
					return PLAYERBOT_RACE_NONE;
				if (percentOut)
					*percentOut = percent;
				return best;
			}
		}
		int percent = 0;
		const int measured = GetPlayerBotMapRace(ch->GetMapIndex(), &percent);
		if (measured != PLAYERBOT_RACE_NONE)
		{
			if (percentOut)
				*percentOut = percent;
			return measured;
		}
		return GetPlayerBotDominantRace(ch->GetMapIndex(), percentOut);
	}

	// ------------------------------------------------------------ the spots
	//
	// Where the monsters are, as the population has seen it. Every target search
	// counts the monsters within reach of the bot that ran it, level aside, and
	// drops that count into the cell the bot stood in; every fight that starts
	// is a mark on the cell it started in. Hubs are then chosen by what the cell
	// under them has been seen to hold, divided among the bots already there.
	//
	// The hubs themselves stay hand-placed on spawn points: the memory says how
	// full a place is, the table says where a place is. Letting the memory
	// invent places would send bots to wherever they happened to stand when a
	// pack respawned round them, which is not where the pack lives.
	struct TPlayerBotSpotCell
	{
		DWORD dwSamples;
		DWORD dwMonsters;
		DWORD dwFights;
		// Sum of the levels of the monsters counted, so a cell can say how strong
		// its monsters are as well as how many: a camp of level-46 knights is
		// full, and no place for a bot of thirty-six on its own.
		DWORD dwLevelSum;
		DWORD dwDecayStamp;
		TPlayerBotSpotCell() : dwSamples(0), dwMonsters(0), dwFights(0), dwLevelSum(0), dwDecayStamp(0) {}
	};
	typedef std::map<unsigned long long, TPlayerBotSpotCell> TPlayerBotSpotMap;
	TPlayerBotSpotMap s_mapSpotMemory;
	DWORD s_dwSpotReportTime = 0;

	// What the population's shellfish held: stone, nothing, white, blue, red.
	// Empty results count - a memory of the pearls alone would say every
	// shell is worth prying open.
	enum EPlayerBotShellfishOutcome
	{
		PLAYERBOT_SHELL_STONE = 0,
		PLAYERBOT_SHELL_NOTHING,
		PLAYERBOT_SHELL_WHITE,
		PLAYERBOT_SHELL_BLUE,
		PLAYERBOT_SHELL_RED,
		PLAYERBOT_SHELL_MAX
	};
	DWORD s_auShellfishOutcomes[PLAYERBOT_SHELL_MAX] = { 0, 0, 0, 0, 0 };

	void RememberPlayerBotShellfishOutcome(int outcome)
	{
		if (outcome >= 0 && outcome < PLAYERBOT_SHELL_MAX)
			++s_auShellfishOutcomes[outcome];
	}

	DWORD GetPlayerBotShellfishSamples()
	{
		DWORD total = 0;
		for (int i = 0; i < PLAYERBOT_SHELL_MAX; ++i)
			total += s_auShellfishOutcomes[i];
		return total;
	}

	// Thousandths of an outcome, from what was seen; the table's figure until
	// enough shells have been opened for the count to mean anything.
	int GetPlayerBotShellfishPermille(int outcome, int tablePermille)
	{
		const DWORD total = GetPlayerBotShellfishSamples();
		if (total < PLAYERBOT_SHELLFISH_LEARN_SAMPLES || outcome < 0 || outcome >= PLAYERBOT_SHELL_MAX)
			return tablePermille;
		return (int)((unsigned long long)s_auShellfishOutcomes[outcome] * 1000ULL / total);
	}

	// What dropped where: a material vnum per spot cell, counted at pickup.
	// A cell with two hundred fights and no Bear Hide is as much a fact as one
	// with twenty hides - it is the zeros that stop a bot camping a barren
	// spot on the strength of a drop table.
	typedef std::map<unsigned long long, std::map<DWORD, DWORD> > TPlayerBotSpotDropMap;
	TPlayerBotSpotDropMap s_mapSpotDrops;

	unsigned long long PlayerBotSpotKey(long lMapIndex, long cellX, long cellY)
	{
		return ((unsigned long long)(DWORD)lMapIndex << 40) |
				((unsigned long long)((DWORD)cellY & 0xfffffU) << 20) |
				(unsigned long long)((DWORD)cellX & 0xfffffU);
	}

	void RememberPlayerBotSpotDrop(long lMapIndex, long x, long y, DWORD vnum)
	{
		++s_mapSpotDrops[PlayerBotSpotKey(lMapIndex, x / PLAYERBOT_SPOT_CELL, y / PLAYERBOT_SPOT_CELL)][vnum];
	}

	// Drops of a vnum seen in the cell, and how many fights that cell has had,
	// so the caller can tell "unknown" from "barren".
	DWORD GetPlayerBotSpotDropCount(long lMapIndex, long x, long y, DWORD vnum, DWORD* pFights)
	{
		const unsigned long long key = PlayerBotSpotKey(lMapIndex, x / PLAYERBOT_SPOT_CELL, y / PLAYERBOT_SPOT_CELL);
		if (pFights)
		{
			TPlayerBotSpotMap::const_iterator cell = s_mapSpotMemory.find(key);
			*pFights = cell != s_mapSpotMemory.end() ? cell->second.dwFights : 0;
		}
		TPlayerBotSpotDropMap::const_iterator it = s_mapSpotDrops.find(key);
		if (it == s_mapSpotDrops.end())
			return 0;
		std::map<DWORD, DWORD>::const_iterator drop = it->second.find(vnum);
		return drop != it->second.end() ? drop->second : 0;
	}


	void DecayPlayerBotSpotCell(TPlayerBotSpotCell& cell, DWORD dwNow)
	{
		if (cell.dwDecayStamp == 0)
			cell.dwDecayStamp = dwNow;
		while (dwNow - cell.dwDecayStamp >= PLAYERBOT_SPOT_DECAY_INTERVAL)
		{
			cell.dwSamples /= 2;
			cell.dwMonsters /= 2;
			cell.dwFights /= 2;
			cell.dwLevelSum /= 2;
			cell.dwDecayStamp += PLAYERBOT_SPOT_DECAY_INTERVAL;
			if (cell.dwSamples == 0 && cell.dwMonsters == 0 && cell.dwFights == 0)
			{
				cell.dwDecayStamp = dwNow;
				break;
			}
		}
	}

	TPlayerBotSpotCell& PlayerBotSpotCellAt(long lMapIndex, long x, long y, DWORD dwNow)
	{
		TPlayerBotSpotCell& cell = s_mapSpotMemory[PlayerBotSpotKey(lMapIndex,
				x / PLAYERBOT_SPOT_CELL, y / PLAYERBOT_SPOT_CELL)];
		DecayPlayerBotSpotCell(cell, dwNow);
		return cell;
	}

	void RememberPlayerBotSpotSighting(long lMapIndex, long x, long y, int iMonsters, int iLevelSum, DWORD dwNow)
	{
		if (x < 0 || y < 0)
			return;
		TPlayerBotSpotCell& cell = PlayerBotSpotCellAt(lMapIndex, x, y, dwNow);
		++cell.dwSamples;
		cell.dwMonsters += (DWORD)std::max(0, iMonsters);
		cell.dwLevelSum += (DWORD)std::max(0, iLevelSum);
	}

	void RememberPlayerBotSpotFight(long lMapIndex, long x, long y, DWORD dwNow)
	{
		if (x < 0 || y < 0)
			return;
		++PlayerBotSpotCellAt(lMapIndex, x, y, dwNow).dwFights;
	}

	// Monsters per look, in thousandths, over the cell and the eight around it -
	// a hub sits on a cell edge as often as not. Zero with no looks; the caller
	// decides what an unknown place is worth.
	int GetPlayerBotSpotDensityPermille(long lMapIndex, long x, long y, DWORD dwNow, DWORD* pdwSamples, int* piAverageLevel)
	{
		DWORD samples = 0, monsters = 0, levels = 0;
		const long cx = x / PLAYERBOT_SPOT_CELL;
		const long cy = y / PLAYERBOT_SPOT_CELL;
		for (long dy = -1; dy <= 1; ++dy)
		{
			for (long dx = -1; dx <= 1; ++dx)
			{
				TPlayerBotSpotMap::iterator it = s_mapSpotMemory.find(
						PlayerBotSpotKey(lMapIndex, cx + dx, cy + dy));
				if (it == s_mapSpotMemory.end())
					continue;
				DecayPlayerBotSpotCell(it->second, dwNow);
				samples += it->second.dwSamples;
				monsters += it->second.dwMonsters;
				levels += it->second.dwLevelSum;
			}
		}
		if (pdwSamples)
			*pdwSamples = samples;
		if (piAverageLevel)
			*piAverageLevel = monsters == 0 ? 0 : (int)(levels / monsters);
		return samples == 0 ? 0 : (int)((unsigned long long)monsters * 1000ULL / samples);
	}

	// Where the other bots on a map stand right now, taken once per decision
	// so that scoring twenty-five hubs does not mean twenty-five walks over the
	// state map. The asker's own party and guild are left out when asked to -
	// the ones a leader wants beside it are not a crowd.
	struct TPlayerBotCrowdEntry
	{
		long x;
		long y;
		LPPARTY pParty;
		CGuild* pGuild;
	};

	void CollectPlayerBotCrowd(LPCHARACTER me, long lMapIndex, std::vector<TPlayerBotCrowdEntry>& out)
	{
		out.clear();
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER other = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!other || other == me || other->GetMapIndex() != lMapIndex)
				continue;
			TPlayerBotCrowdEntry entry;
			entry.x = other->GetX();
			entry.y = other->GetY();
			entry.pParty = other->GetParty();
			entry.pGuild = other->GetGuild();
			out.push_back(entry);
		}
	}

	int CountPlayerBotsNear(LPCHARACTER me, const std::vector<TPlayerBotCrowdEntry>& crowd,
			long x, long y, int iRadius, bool bIgnoreOwnParty)
	{
		int count = 0;
		LPPARTY myParty = (me && bIgnoreOwnParty) ? me->GetParty() : NULL;
		CGuild* myGuild = (me && bIgnoreOwnParty) ? me->GetGuild() : NULL;
		for (size_t i = 0; i < crowd.size(); ++i)
		{
			if (DISTANCE_APPROX(crowd[i].x - x, crowd[i].y - y) > iRadius)
				continue;
			if (myParty && crowd[i].pParty == myParty)
				continue;
			if (myGuild && crowd[i].pGuild == myGuild)
				continue;
			++count;
		}
		return count;
	}

	// Once in a while, what the population thinks the richest ground is. Read
	// this against the hub tables: a cell nobody's hub covers that keeps coming
	// top is a hub the table is missing.
	void ReportPlayerBotSpotMemory(DWORD dwNow)
	{
		if (s_dwSpotReportTime == 0)
		{
			s_dwSpotReportTime = dwNow;
			return;
		}
		if (dwNow - s_dwSpotReportTime < PLAYERBOT_SPOT_REPORT_INTERVAL)
			return;
		s_dwSpotReportTime = dwNow;

		if (GetPlayerBotShellfishSamples() > 0)
			sys_log(0, "PLAYERBOT_SHELLFISH: opened=%u stone=%u nothing=%u white=%u blue=%u red=%u",
					GetPlayerBotShellfishSamples(), s_auShellfishOutcomes[PLAYERBOT_SHELL_STONE],
					s_auShellfishOutcomes[PLAYERBOT_SHELL_NOTHING], s_auShellfishOutcomes[PLAYERBOT_SHELL_WHITE],
					s_auShellfishOutcomes[PLAYERBOT_SHELL_BLUE], s_auShellfishOutcomes[PLAYERBOT_SHELL_RED]);

		std::map<long, std::vector<std::pair<int, unsigned long long> > > byMap;
		for (TPlayerBotSpotMap::iterator it = s_mapSpotMemory.begin(); it != s_mapSpotMemory.end(); ++it)
		{
			DecayPlayerBotSpotCell(it->second, dwNow);
			if (it->second.dwSamples < PLAYERBOT_SPOT_MIN_SAMPLES)
				continue;
			const long map = (long)(it->first >> 40);
			byMap[map].push_back(std::make_pair(
					(int)((unsigned long long)it->second.dwMonsters * 1000ULL / it->second.dwSamples), it->first));
		}
		for (std::map<long, std::vector<std::pair<int, unsigned long long> > >::iterator m = byMap.begin();
				m != byMap.end(); ++m)
		{
			std::sort(m->second.begin(), m->second.end());
			std::string line;
			int shown = 0;
			for (size_t i = m->second.size(); i > 0 && shown < 3; --i, ++shown)
			{
				const unsigned long long key = m->second[i - 1].second;
				const long cx = (long)(key & 0xfffffU);
				const long cy = (long)((key >> 20) & 0xfffffU);
				const TPlayerBotSpotCell& cell = s_mapSpotMemory[key];
				char buf[96];
				snprintf(buf, sizeof(buf), " (%ld,%ld)=%d.%d/look lvl~%u fights=%u looks=%u",
						cx * PLAYERBOT_SPOT_CELL + PLAYERBOT_SPOT_CELL / 2,
						cy * PLAYERBOT_SPOT_CELL + PLAYERBOT_SPOT_CELL / 2,
						m->second[i - 1].first / 1000, (m->second[i - 1].first % 1000) / 100,
						cell.dwMonsters ? (unsigned int)(cell.dwLevelSum / cell.dwMonsters) : 0U,
						cell.dwFights, cell.dwSamples);
				line += buf;
			}
			sys_log(0, "PLAYERBOT_SPOT: map=%ld cells=%u richest:%s",
					m->first, (unsigned int)m->second.size(), line.c_str());
		}
	}
}

#endif
