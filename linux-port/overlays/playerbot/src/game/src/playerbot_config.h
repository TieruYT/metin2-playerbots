#ifndef __INC_METIN2_PLAYERBOT_CONFIG_H__
#define __INC_METIN2_PLAYERBOT_CONFIG_H__

// Live tuning: the weights an operator moves in the panel while the world runs.
//
// Every number a bot decides with used to be a constant, which meant that
// changing "fewer anglers, more metin hunters" cost a rebuild of the game image
// and a restart of the world - a quarter of an hour to answer a question that
// takes a minute to ask. These weights are read from a file instead, so the
// answer takes effect on the next planning tick.
//
// The file is a two-column TSV the panel writes into the rate spool, which is
// the one directory the panel and the game container share. Its format is the
// operator's, not ours: a name, a tab, a number, and anything after # ignored.
//
//     RESTOCK	100
//     METIN	140      # more metin hunters than usual
//
// 100 is neutral and is what every weight is worth when the file is missing,
// unreadable, or says nothing about it. That is deliberate: a fresh install and
// a broken file must both behave exactly like the build did before this
// existed. The range is 25 to 250 - a quarter as often, to two and a half times
// as often - and anything outside it is clamped rather than rejected, because a
// slider that silently does nothing is worse than one that stops at its end.

namespace
{
	enum EPlayerBotWeight
	{
		PLAYERBOT_WEIGHT_RESTOCK = 0,   // buying potions before anything else
		PLAYERBOT_WEIGHT_REFINE,        // the blacksmith
		PLAYERBOT_WEIGHT_SKILL,         // reading a skill book to master
		PLAYERBOT_WEIGHT_HORSE,         // the stable and the medal hunt
		PLAYERBOT_WEIGHT_BIOLOG,        // the Biologist's collections
		PLAYERBOT_WEIGHT_METIN,         // hunting metin stones
		PLAYERBOT_WEIGHT_PARTY,         // fighting as a party
		PLAYERBOT_WEIGHT_HUNTING,       // the level-up hunt mission
		PLAYERBOT_WEIGHT_LEVEL,         // plain grinding, the fallback goal
		PLAYERBOT_WEIGHT_FISHING,       // how many bots take up fishing at all
		PLAYERBOT_WEIGHT_TRADE,         // how many bots keep a market stall
		PLAYERBOT_WEIGHT_MAX
	};

	const int PLAYERBOT_WEIGHT_NEUTRAL = 100;
	const int PLAYERBOT_WEIGHT_MIN = 25;
	const int PLAYERBOT_WEIGHT_LIMIT = 250;

	// How often the file is looked at. Cheap - one stat(2) - and only re-read
	// when the timestamp or the size actually moved.
	const DWORD PLAYERBOT_WEIGHT_RELOAD_INTERVAL = 5000;

	// The rate spool: the same volume at the same path in both containers, so a
	// file the panel writes here is the file the game reads. An environment
	// variable overrides it for a build that puts the spool somewhere else.
	const char* const PLAYERBOT_WEIGHT_DEFAULT_PATH = "/opt/m2spool/playerbot_weights.tsv";

	struct TPlayerBotWeightName
	{
		const char* szName;
		BYTE bWeight;
	};

	const TPlayerBotWeightName PLAYERBOT_WEIGHT_NAMES[] = {
		{ "RESTOCK", PLAYERBOT_WEIGHT_RESTOCK },
		{ "REFINE",  PLAYERBOT_WEIGHT_REFINE  },
		{ "SKILL",   PLAYERBOT_WEIGHT_SKILL   },
		{ "HORSE",   PLAYERBOT_WEIGHT_HORSE   },
		{ "BIOLOG",  PLAYERBOT_WEIGHT_BIOLOG  },
		{ "METIN",   PLAYERBOT_WEIGHT_METIN   },
		{ "PARTY",   PLAYERBOT_WEIGHT_PARTY   },
		{ "HUNTING", PLAYERBOT_WEIGHT_HUNTING },
		{ "LEVEL",   PLAYERBOT_WEIGHT_LEVEL   },
		{ "FISHING", PLAYERBOT_WEIGHT_FISHING },
		{ "TRADE",   PLAYERBOT_WEIGHT_TRADE   },
	};

	int s_aiPlayerBotWeights[PLAYERBOT_WEIGHT_MAX];
	bool s_bPlayerBotWeightsInitialised = false;
	DWORD s_dwPlayerBotWeightNextCheck = 0;
	time_t s_tPlayerBotWeightMtime = 0;
	long s_lPlayerBotWeightSize = -1;

	const char* GetPlayerBotWeightPath()
	{
		const char* override_path = getenv("PLAYERBOT_WEIGHTS_FILE");
		if (override_path && *override_path)
			return override_path;
		return PLAYERBOT_WEIGHT_DEFAULT_PATH;
	}

	void ResetPlayerBotWeights()
	{
		for (int i = 0; i < PLAYERBOT_WEIGHT_MAX; ++i)
			s_aiPlayerBotWeights[i] = PLAYERBOT_WEIGHT_NEUTRAL;
		s_bPlayerBotWeightsInitialised = true;
	}

	int ClampPlayerBotWeight(long value)
	{
		if (value < PLAYERBOT_WEIGHT_MIN)
			return PLAYERBOT_WEIGHT_MIN;
		if (value > PLAYERBOT_WEIGHT_LIMIT)
			return PLAYERBOT_WEIGHT_LIMIT;
		return (int)value;
	}

	// Written out rather than strcasecmp, which lives in <strings.h> on glibc and
	// only reaches us by accident through another header.
	bool PlayerBotWeightNameEquals(const char* szLeft, const char* szRight)
	{
		for (; *szLeft && *szRight; ++szLeft, ++szRight)
		{
			const char a = (*szLeft >= 'a' && *szLeft <= 'z') ? (char)(*szLeft - 32) : *szLeft;
			const char b = (*szRight >= 'a' && *szRight <= 'z') ? (char)(*szRight - 32) : *szRight;
			if (a != b)
				return false;
		}
		return *szLeft == 0 && *szRight == 0;
	}

	void ApplyPlayerBotWeightLine(const char* szKey, long value)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_WEIGHT_NAMES) /
				sizeof(PLAYERBOT_WEIGHT_NAMES[0]); ++i)
		{
			if (!PlayerBotWeightNameEquals(szKey, PLAYERBOT_WEIGHT_NAMES[i].szName))
				continue;
			s_aiPlayerBotWeights[PLAYERBOT_WEIGHT_NAMES[i].bWeight] =
					ClampPlayerBotWeight(value);
			return;
		}
		// An unknown name is not an error: the panel of a newer build may write
		// weights this core has never heard of, and the sane answer is to keep
		// running on the ones it does know.
		sys_log(0, "PLAYERBOT_CONFIG: unknown weight '%s' ignored", szKey);
	}

	void ReadPlayerBotWeightFile(const char* szPath)
	{
		FILE* fp = fopen(szPath, "r");
		if (!fp)
			return;

		// Every weight the file does not mention goes back to neutral, so
		// removing a line from it undoes that line rather than leaving the last
		// value in place until the next restart.
		ResetPlayerBotWeights();

		char line[256];
		int applied = 0;
		while (fgets(line, sizeof(line), fp))
		{
			char* comment = strchr(line, '#');
			if (comment)
				*comment = '\0';

			char* cursor = line;
			while (*cursor == ' ' || *cursor == '\t')
				++cursor;

			char* key = cursor;
			while (*cursor && *cursor != ' ' && *cursor != '\t' &&
					*cursor != '\r' && *cursor != '\n')
				++cursor;
			if (cursor == key)
				continue;
			const char terminator = *cursor;
			*cursor = '\0';
			if (terminator == '\0')
				continue;   // a name with no value at all

			++cursor;
			while (*cursor == ' ' || *cursor == '\t')
				++cursor;
			if (!*cursor)
				continue;

			ApplyPlayerBotWeightLine(key, strtol(cursor, NULL, 10));
			++applied;
		}
		fclose(fp);

		sys_log(0, "PLAYERBOT_CONFIG: reloaded %d weights from %s", applied, szPath);
	}

	// Called once per tick. Does nothing at all between checks, and nothing but
	// a stat(2) when the file has not changed since the last one.
	void RefreshPlayerBotWeights(DWORD dwNow)
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		if (dwNow < s_dwPlayerBotWeightNextCheck)
			return;
		s_dwPlayerBotWeightNextCheck = dwNow + PLAYERBOT_WEIGHT_RELOAD_INTERVAL;

		const char* szPath = GetPlayerBotWeightPath();
		struct stat st;
		if (stat(szPath, &st) != 0)
		{
			// The file was there and is not any more: back to how the build
			// behaves with no panel at all.
			if (s_lPlayerBotWeightSize >= 0)
			{
				sys_log(0, "PLAYERBOT_CONFIG: %s is gone, weights back to neutral", szPath);
				ResetPlayerBotWeights();
				s_tPlayerBotWeightMtime = 0;
				s_lPlayerBotWeightSize = -1;
			}
			return;
		}

		if (st.st_mtime == s_tPlayerBotWeightMtime &&
				(long)st.st_size == s_lPlayerBotWeightSize)
			return;

		s_tPlayerBotWeightMtime = st.st_mtime;
		s_lPlayerBotWeightSize = (long)st.st_size;
		ReadPlayerBotWeightFile(szPath);
	}

	int GetPlayerBotWeight(BYTE bWeight)
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		if (bWeight >= PLAYERBOT_WEIGHT_MAX)
			return PLAYERBOT_WEIGHT_NEUTRAL;
		return s_aiPlayerBotWeights[bWeight];
	}

	// A weighted priority. Integer arithmetic on purpose: the planner compares
	// these against each other and must not depend on floating point rounding
	// differing between builds.
	int WeighPlayerBotPriority(int iBase, BYTE bWeight)
	{
		return iBase * GetPlayerBotWeight(bWeight) / PLAYERBOT_WEIGHT_NEUTRAL;
	}

	// A weighted roll. The caller keeps the odds it always had - "twenty in a
	// hundred", "one hundred in a thousand" - and this stretches or shrinks them
	// by the weight. dwRoll and iChance must be drawn against the same space.
	bool PlayerBotWeightedRoll(DWORD dwRoll, int iChance, BYTE bWeight)
	{
		const long long threshold =
				(long long)iChance * GetPlayerBotWeight(bWeight) / PLAYERBOT_WEIGHT_NEUTRAL;
		return (long long)dwRoll < threshold;
	}
}

#endif
