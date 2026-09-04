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
