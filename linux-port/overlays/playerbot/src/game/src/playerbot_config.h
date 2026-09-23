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

// The Moonlight chest event lives in the engine (patch 0006): thousandths of
// a chance per kill and per Metin stone, read from CONFIG at start. The panel
// moves them here through the same file as the weights, so the operator does
// not edit .env and restart to see more chests.
extern int g_iMoonlightChestPermille;
extern int g_iMoonlightChestStonePermille;

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

	// Night on the server. The engine has no day cycle of its own; what it has
	// is the Christmas event flag "xmas_snow", which the client answers with
	// the night sky (and the snow) - a GM turns it on by hand with /xmas_snow.
	// The NIGHT switch in the weights file does that on a clock instead:
	// between these hours of the container's local time (M2_TZ) the flag is
	// raised, outside them it is lowered. The flag goes through the DB core
	// and comes back as a broadcast to every client, so a change is asked
	// for at most once a minute and only when the flag disagrees.
	const int PLAYERBOT_NIGHT_START_HOUR = 22;
	const int PLAYERBOT_NIGHT_END_HOUR = 6;
	const DWORD PLAYERBOT_NIGHT_CHECK_INTERVAL = 60000;
	const char* const PLAYERBOT_NIGHT_EVENT_FLAG = "xmas_snow";

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
	// Whether bots say what they are doing over their heads. Off is what
	// players who called it spam asked for; the refine shouts on the world
	// channel are not covered - those are one line every few minutes.
	bool s_bPlayerBotOverheadChat = true;
	// Percent of stall keepers that sell scrap gear. Zero is off, and the
	// default: it is the "hard server" flavour, asked for by name.
	int s_iPlayerBotScrapPercent = 0;
	// Percent of the bots that finish an errand in a first village and stay
	// a while on the market ring (PLAYERBOT_TOWN_LINGER_*). A hundred is the
	// author's town; zero is the operator who wants every bot hunting, asked
	// for by name. The level floor beside it is PLAYERBOT_TOWN_REST_MIN_LEVEL.
	int s_iPlayerBotRestPercent = 100;
	// The manager tick's time budget per pass, in milliseconds (TICK_MS; see
	// PLAYERBOT_TICK_BUDGET_MS_DEFAULT). Zero is no budget.
	int s_iPlayerBotTickBudgetMs = PLAYERBOT_TICK_BUDGET_MS_DEFAULT;
	// Percent of the bots that will pick a fight with a bot of another kingdom.
	// Zero is off, and the default, and that is deliberate: this changes how the
	// world behaves towards itself rather than how one bot spends its time, so
	// it stays a decision the operator makes on purpose. The share is by pid, so
	// the same bots are the aggressive ones from one restart to the next - which
	// is what "some aggressive, some neutral" has to mean if a kingdom is to
	// have a character rather than a mood.
	int s_iPlayerBotKingdomPvpPercent = 0;
	// The lowest plus a refine under a Blessing or Dragon God scroll may land
	// on. One is no floor, and the default: the refine passes keep their own
	// rules about where a scroll is worth it (from +7, earlier for a worn piece
	// that can burn and for a prize piece). Seven puts a scroll on the steps to
	// +7, +8 and +9 only, and every step under that goes to the plain anvil the
	// way a bot with no scroll refines - asked for as one setting for the whole
	// world ("tylko mozna np uzywac na +7 +8 +9", Tieru).
	int s_iPlayerBotScrollFromPlus = 1;
	// Whether a bot reads its books without the engine's day between them.
	// On by default: the day is what makes a book a month's project, and the
	// books were rotting in the bags of bots that could not read them yet.
	bool s_bPlayerBotFastBooks = true;
	bool s_bPlayerBotFastBooksReported = true;
	// Whether the night clock runs. On by default: asked for by the players,
	// and the switch in the panel is for the ones who would rather not.
	bool s_bPlayerBotNight = true;
	bool s_bPlayerBotNightReported = true;
	// "Boty graja jak zywi ludzie" (the LIFE key): sessions and rests, in
	// CPlayerBotManager::ManageLifeSchedule. Off until the panel says so.
	bool s_bPlayerBotLifeSchedule = false;
	bool s_bPlayerBotLifeScheduleReported = false;
	// Guild wars between the bots' guilds (the WARS key), playerbot_guild_war.h.
	bool s_bPlayerBotGuildWars = true;
	bool s_bPlayerBotGuildWarsReported = true;
	// The bot guilds' Demon Tower raids (the TOWER key), playerbot_demon_tower.h.
	bool s_bPlayerBotTowerRaids = true;
	bool s_bPlayerBotTowerRaidsReported = true;
	// The bots' ItemShop purchases (the ISHOP key), playerbot_itemshop.h.
	bool s_bPlayerBotItemShop = true;
	bool s_bPlayerBotItemShopReported = true;
	// Whether a bot's stand may stand in a second village too (the SHOP_M2
	// key). Off by default: Iwakura's idea (Kuszaa's reasoning, 22 September)
	// is the stands in the first villages alone - "przy obecnych sklepach
	// offline sklepy w M2 sa troche useless", the players shop where the
	// market is - with this switch for a world that wants both.
	bool s_bPlayerBotShopsInM2 = false;
	bool s_bPlayerBotShopsInM2Reported = false;
	// Iwakura's personality system (the PERSONA key): moods, the personalities
	// that follow a bot's situation, the Grinder's experience locks and the
	// Law of Advancement (playerbot_persona.h). On by default - the operator
	// asked for it (19 September) - and the switch is the way back to the
	// world as it was, whole, while a world is running.
	bool s_bPlayerBotPersona = true;
	bool s_bPlayerBotPersonaReported = true;
	// What the clock last asked the DB core for, so a request is not repeated
	// every minute while the round trip is still in flight, and so switching
	// the clock off in the middle of a night lowers the flag it raised.
	int s_iPlayerBotNightRequested = -1;
	DWORD s_dwPlayerBotNightNextCheck = 0;
	// What CONFIG said before the file ever overrode it, so a file that stops
	// mentioning the chests hands the numbers back to CONFIG.
	int s_iPlayerBotChestConfigPermille = -1;
	int s_iPlayerBotChestStoneConfigPermille = -1;
	bool s_bPlayerBotChestFromFile = false;
	bool s_bPlayerBotOverheadChatReported = true;
	bool s_bPlayerBotWeightsInitialised = false;
	DWORD s_dwPlayerBotWeightNextCheck = 0;
	time_t s_tPlayerBotWeightMtime = 0;
	long s_lPlayerBotWeightSize = -1;
	// Bumped every time the weights change (a new file, or the file gone), so
	// a decision taken under the old numbers can tell it is stale.
	DWORD s_dwPlayerBotWeightsGeneration = 0;

	const char* GetPlayerBotWeightPath()
	{
		const char* override_path = getenv("PLAYERBOT_WEIGHTS_FILE");
		if (override_path && *override_path)
			return override_path;
		return PLAYERBOT_WEIGHT_DEFAULT_PATH;
	}

	// playerbot_events.h: whether the chest gate is holding the engine's chest
	// figures at zero right now - it is whenever no chest event runs.
	bool IsPlayerBotChestGateClosed();
	// The sliders' figure (CONFIG's until a weights file names one), which is
	// what the gate opens the drop to - never the engine's variable, which
	// the gate itself may be holding at zero.
	int GetPlayerBotChestConfigPermille(bool stone)
	{
		return stone ? s_iPlayerBotChestStoneConfigPermille : s_iPlayerBotChestConfigPermille;
	}

	void ResetPlayerBotWeights()
	{
		++s_dwPlayerBotWeightsGeneration;
		for (int i = 0; i < PLAYERBOT_WEIGHT_MAX; ++i)
			s_aiPlayerBotWeights[i] = PLAYERBOT_WEIGHT_NEUTRAL;
		s_bPlayerBotOverheadChat = true;
		s_iPlayerBotScrapPercent = 0;
		s_iPlayerBotRestPercent = 100;
		s_iPlayerBotTickBudgetMs = PLAYERBOT_TICK_BUDGET_MS_DEFAULT;
		s_iPlayerBotKingdomPvpPercent = 0;
		s_iPlayerBotScrollFromPlus = 1;
		s_bPlayerBotFastBooks = true;
		s_bPlayerBotNight = true;
		s_bPlayerBotLifeSchedule = false;
		s_bPlayerBotGuildWars = true;
		s_bPlayerBotTowerRaids = true;
		s_bPlayerBotItemShop = true;
		s_bPlayerBotShopsInM2 = false;
		s_bPlayerBotPersona = true;
		if (s_iPlayerBotChestConfigPermille < 0)
		{
			s_iPlayerBotChestConfigPermille = g_iMoonlightChestPermille;
			s_iPlayerBotChestStoneConfigPermille = g_iMoonlightChestStonePermille;
		}
		// While a chest window holds the engine's figures shut
		// (playerbot_events.h) the sliders' figure is kept here and the gate
		// puts it back when the window opens; writing it now opened the drop
		// for up to a second on every save of the weights file.
		if (!IsPlayerBotChestGateClosed())
		{
			g_iMoonlightChestPermille = s_iPlayerBotChestConfigPermille;
			g_iMoonlightChestStonePermille = s_iPlayerBotChestStoneConfigPermille;
		}
		s_bPlayerBotChestFromFile = false;
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
		// Not a weight: a switch, in the same file because the same five-second
		// reload already delivers it to a running core.
		if (PlayerBotWeightNameEquals(szKey, "CHAT"))
		{
			const bool enabled = value != 0;
			// Reported against what was last reported, not the current flag: the
			// reload resets the flag before this line is read, so "on" would never
			// be seen as a change.
			if (enabled != s_bPlayerBotOverheadChatReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: overhead chat %s", enabled ? "on" : "off");
				s_bPlayerBotOverheadChatReported = enabled;
			}
			s_bPlayerBotOverheadChat = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "BOOKS"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotFastBooksReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: fast books %s", enabled ? "on" : "off");
				s_bPlayerBotFastBooksReported = enabled;
			}
			s_bPlayerBotFastBooks = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "NIGHT"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotNightReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: night clock %s", enabled ? "on" : "off");
				s_bPlayerBotNightReported = enabled;
			}
			s_bPlayerBotNight = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "LIFE"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotLifeScheduleReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: life schedule %s", enabled ? "on" : "off");
				s_bPlayerBotLifeScheduleReported = enabled;
			}
			s_bPlayerBotLifeSchedule = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "SHOP_M2"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotShopsInM2Reported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: bot stands in the second villages %s", enabled ? "on" : "off");
				s_bPlayerBotShopsInM2Reported = enabled;
			}
			s_bPlayerBotShopsInM2 = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "WARS"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotGuildWarsReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: guild wars %s", enabled ? "on" : "off");
				s_bPlayerBotGuildWarsReported = enabled;
			}
			s_bPlayerBotGuildWars = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "TOWER"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotTowerRaidsReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: tower raids %s", enabled ? "on" : "off");
				s_bPlayerBotTowerRaidsReported = enabled;
			}
			s_bPlayerBotTowerRaids = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "ISHOP"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotItemShopReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: itemshop %s", enabled ? "on" : "off");
				s_bPlayerBotItemShopReported = enabled;
			}
			s_bPlayerBotItemShop = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "PERSONA"))
		{
			const bool enabled = value != 0;
			if (enabled != s_bPlayerBotPersonaReported)
			{
				sys_log(0, "PLAYERBOT_CONFIG: personalities (Iwakura v2) %s", enabled ? "on" : "off");
				s_bPlayerBotPersonaReported = enabled;
			}
			s_bPlayerBotPersona = enabled;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "CHEST") || PlayerBotWeightNameEquals(szKey, "CHEST_STONE"))
		{
			const int permille = value < 0 ? 0 : (value > 1000 ? 1000 : (int)value);
			const bool stone = !PlayerBotWeightNameEquals(szKey, "CHEST");
			int& wanted = stone ? s_iPlayerBotChestStoneConfigPermille : s_iPlayerBotChestConfigPermille;
			if (wanted != permille)
				sys_log(0, "PLAYERBOT_CONFIG: moonlight chest %s %d -> %d permille%s",
						stone ? "stone" : "kill", wanted, permille,
						IsPlayerBotChestGateClosed() ? " (held shut until the chest window)" : "");
			wanted = permille;
			// The engine's variable only while no chest window holds it shut:
			// the gate (playerbot_events.h) reads the figure kept above.
			if (!IsPlayerBotChestGateClosed())
				(stone ? g_iMoonlightChestStonePermille : g_iMoonlightChestPermille) = permille;
			s_bPlayerBotChestFromFile = true;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "SCRAP"))
		{
			const int percent = value < 0 ? 0 : (value > 100 ? 100 : (int)value);
			if (percent != s_iPlayerBotScrapPercent)
				sys_log(0, "PLAYERBOT_CONFIG: scrap keepers %d%%", percent);
			s_iPlayerBotScrapPercent = percent;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "REST"))
		{
			const int percent = value < 0 ? 0 : (value > 100 ? 100 : (int)value);
			if (percent != s_iPlayerBotRestPercent)
				sys_log(0, "PLAYERBOT_CONFIG: town rest %d%%", percent);
			s_iPlayerBotRestPercent = percent;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "TICK_MS"))
		{
			const int budget = value < 0 ? 0 : (value > 1000 ? 1000 : (int)value);
			if (budget != s_iPlayerBotTickBudgetMs)
				sys_log(0, "PLAYERBOT_CONFIG: tick budget %d ms%s", budget, budget ? "" : " (none)");
			s_iPlayerBotTickBudgetMs = budget;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "KINGDOMPVP"))
		{
			const int percent = value < 0 ? 0 : (value > 100 ? 100 : (int)value);
			if (percent != s_iPlayerBotKingdomPvpPercent)
				sys_log(0, "PLAYERBOT_CONFIG: kingdom hostility %d%% of bots", percent);
			s_iPlayerBotKingdomPvpPercent = percent;
			return;
		}
		if (PlayerBotWeightNameEquals(szKey, "SCROLL_FROM"))
		{
			const int plus = value < 1 ? 1 : (value > PLAYERBOT_SCROLL_REFINE_MAX_PLUS
					? (int)PLAYERBOT_SCROLL_REFINE_MAX_PLUS : (int)value);
			if (plus != s_iPlayerBotScrollFromPlus)
				sys_log(0, "PLAYERBOT_CONFIG: refine scrolls from +%d", plus);
			s_iPlayerBotScrollFromPlus = plus;
			return;
		}
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

	// --- The F9 panel's side of the same file ---------------------------------
	//
	// The web panel writes playerbot_weights.tsv and the core re-reads it every
	// five seconds; the GM panel in the client is a second writer of the same
	// file, so it goes through here rather than growing its own idea of the
	// format. Two functions, both called from cmd_gm.cpp through
	// playerbot_manager.h - nothing in this namespace is reachable from an
	// engine translation unit.
	//
	// The order below is the panel's wire order and is fixed: the client zips
	// its own row table against it by position (GM_PANEL_AI_WEIGHT_SERVER_ORDER
	// in interfacemodule.py), so a name may be appended here and never moved.
	const char* const PLAYERBOT_PANEL_WEIGHT_ORDER[] = {
		"RESTOCK", "REFINE", "SKILL", "HORSE", "BIOLOG", "METIN", "PARTY",
		"HUNTING", "LEVEL", "FISHING", "TRADE",
		"CHAT", "BOOKS", "NIGHT", "SCRAP", "CHEST", "CHEST_STONE", "REST",
	};
	const size_t PLAYERBOT_PANEL_WEIGHT_COUNT =
			sizeof(PLAYERBOT_PANEL_WEIGHT_ORDER) / sizeof(PLAYERBOT_PANEL_WEIGHT_ORDER[0]);

	// What the file would have to say to produce the state the core is in.
	// -1 for the two chest keys while no file has set them: the chest odds then
	// come from CONFIG and the panel must show "-" rather than a number it did
	// not choose, or the first slider drag would silently take them over.
	// The chest figures are the sliders' own, not the zero the event gate
	// (playerbot_events.h, later in the include order) may be holding the
	// engine's variables at while no chest window is open.
	int GetPlayerBotChestWantedPermille(bool stone);
	long GetPlayerBotPanelWeightValue(const char* szKey)
	{
		if (PlayerBotWeightNameEquals(szKey, "CHAT"))
			return s_bPlayerBotOverheadChat ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "BOOKS"))
			return s_bPlayerBotFastBooks ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "NIGHT"))
			return s_bPlayerBotNight ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "LIFE"))
			return s_bPlayerBotLifeSchedule ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "WARS"))
			return s_bPlayerBotGuildWars ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "TOWER"))
			return s_bPlayerBotTowerRaids ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "ISHOP"))
			return s_bPlayerBotItemShop ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "SHOP_M2"))
			return s_bPlayerBotShopsInM2 ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "PERSONA"))
			return s_bPlayerBotPersona ? 1 : 0;
		if (PlayerBotWeightNameEquals(szKey, "SCRAP"))
			return s_iPlayerBotScrapPercent;
		if (PlayerBotWeightNameEquals(szKey, "REST"))
			return s_iPlayerBotRestPercent;
		if (PlayerBotWeightNameEquals(szKey, "KINGDOMPVP"))
			return s_iPlayerBotKingdomPvpPercent;
		if (PlayerBotWeightNameEquals(szKey, "CHEST"))
			return !s_bPlayerBotChestFromFile ? -1 :
					(GetPlayerBotChestWantedPermille(false) >= 0 ? GetPlayerBotChestWantedPermille(false) : g_iMoonlightChestPermille);
		if (PlayerBotWeightNameEquals(szKey, "CHEST_STONE"))
			return !s_bPlayerBotChestFromFile ? -1 :
					(GetPlayerBotChestWantedPermille(true) >= 0 ? GetPlayerBotChestWantedPermille(true) : g_iMoonlightChestStonePermille);
		for (size_t i = 0; i < sizeof(PLAYERBOT_WEIGHT_NAMES) /
				sizeof(PLAYERBOT_WEIGHT_NAMES[0]); ++i)
		{
			if (PlayerBotWeightNameEquals(szKey, PLAYERBOT_WEIGHT_NAMES[i].szName))
				return s_aiPlayerBotWeights[PLAYERBOT_WEIGHT_NAMES[i].bWeight];
		}
		return -1;
	}

	bool BuildPlayerBotPanelWeightReport(char* szOut, size_t len)
	{
		if (!szOut || len == 0)
			return false;
		szOut[0] = '\0';
		size_t used = 0;
		for (size_t i = 0; i < PLAYERBOT_PANEL_WEIGHT_COUNT; ++i)
		{
			const int written = snprintf(szOut + used, len - used, "%s%ld",
					i ? "|" : "", GetPlayerBotPanelWeightValue(PLAYERBOT_PANEL_WEIGHT_ORDER[i]));
			if (written < 0 || (size_t)written >= len - used)
				return false;
			used += (size_t)written;
		}
		return true;
	}

	// The bounds each key is written within. The reader clamps too, but a file
	// an operator opens should not carry a number the core would refuse - and
	// the panel is not the only thing that reads it.
	bool ClampPlayerBotPanelWeightValue(const char* szKey, long& value)
	{
		if (PlayerBotWeightNameEquals(szKey, "CHAT") ||
				PlayerBotWeightNameEquals(szKey, "BOOKS") ||
				PlayerBotWeightNameEquals(szKey, "NIGHT") ||
				PlayerBotWeightNameEquals(szKey, "LIFE") ||
				PlayerBotWeightNameEquals(szKey, "WARS") ||
				PlayerBotWeightNameEquals(szKey, "TOWER") ||
				PlayerBotWeightNameEquals(szKey, "ISHOP") ||
				PlayerBotWeightNameEquals(szKey, "SHOP_M2") ||
				PlayerBotWeightNameEquals(szKey, "PERSONA"))
		{
			value = value ? 1 : 0;
			return true;
		}
		if (PlayerBotWeightNameEquals(szKey, "SCRAP"))
		{
			value = value < 0 ? 0 : (value > 100 ? 100 : value);
			return true;
		}
		if (PlayerBotWeightNameEquals(szKey, "CHEST") ||
				PlayerBotWeightNameEquals(szKey, "CHEST_STONE"))
		{
			value = value < 0 ? 0 : (value > 1000 ? 1000 : value);
			return true;
		}
		for (size_t i = 0; i < sizeof(PLAYERBOT_WEIGHT_NAMES) /
				sizeof(PLAYERBOT_WEIGHT_NAMES[0]); ++i)
		{
			if (!PlayerBotWeightNameEquals(szKey, PLAYERBOT_WEIGHT_NAMES[i].szName))
				continue;
			value = ClampPlayerBotWeight(value);
			return true;
		}
		return false;
	}

	// One key changed, everything else in the file kept as it stands - the
	// comments at the top included, because an operator reads them and the web
	// panel wrote them. Written beside the file and renamed over it, so a core
	// re-reading on its five-second clock never sees a half-written table.
	//
	// The game runs as metin2 and the spool is group-writable (gid m2spool in
	// both images), which is what makes the rename possible over a file the
	// panel container created as root.
	bool WritePlayerBotPanelWeight(const char* szKey, long value)
	{
		if (!szKey || !*szKey)
			return false;
		if (!ClampPlayerBotPanelWeightValue(szKey, value))
			return false;   // a name this core does not know; refuse rather than append

		const char* szPath = GetPlayerBotWeightPath();
		std::vector<std::string> lines;
		bool replaced = false;
		FILE* fp = fopen(szPath, "r");
		if (fp)
		{
			char line[512];
			while (fgets(line, sizeof(line), fp))
			{
				// The key is the first field of a line that is not a comment.
				// Everything else - blank lines, the header, a key we are not
				// touching - is copied through byte for byte.
				const char* cursor = line;
				while (*cursor == ' ' || *cursor == '\t')
					++cursor;
				if (*cursor != '#' && *cursor != '\0' && *cursor != '\r' && *cursor != '\n')
				{
					const char* keyStart = cursor;
					while (*cursor && *cursor != ' ' && *cursor != '\t' &&
							*cursor != '\r' && *cursor != '\n')
						++cursor;
					const std::string found(keyStart, (size_t)(cursor - keyStart));
					if (PlayerBotWeightNameEquals(found.c_str(), szKey))
					{
						char rewritten[64];
						snprintf(rewritten, sizeof(rewritten), "%s\t%ld\n", szKey, value);
						lines.push_back(std::string(rewritten));
						replaced = true;
						continue;
					}
				}
				lines.push_back(std::string(line));
			}
			fclose(fp);
		}
		if (!replaced)
		{
			char appended[64];
			snprintf(appended, sizeof(appended), "%s\t%ld\n", szKey, value);
			lines.push_back(std::string(appended));
		}

		char szTemp[256];
		snprintf(szTemp, sizeof(szTemp), "%s.gmpanel", szPath);
		FILE* out = fopen(szTemp, "w");
		if (!out)
		{
			sys_err("PLAYERBOT_CONFIG: cannot write %s", szTemp);
			return false;
		}
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (fputs(lines[i].c_str(), out) == EOF)
			{
				fclose(out);
				unlink(szTemp);
				sys_err("PLAYERBOT_CONFIG: short write to %s", szTemp);
				return false;
			}
		}
		if (fclose(out) != 0 || rename(szTemp, szPath) != 0)
		{
			unlink(szTemp);
			sys_err("PLAYERBOT_CONFIG: cannot replace %s", szPath);
			return false;
		}

		// Applied here as well as written: the reload would pick it up within
		// five seconds anyway, but a GM dragging a slider watches the world and
		// not the clock, and the file's own mtime check makes this harmless.
		ApplyPlayerBotWeightLine(szKey, value);
		sys_log(0, "PLAYERBOT_CONFIG: F9 panel set %s = %ld", szKey, value);
		return true;
	}

	// ---------------------------------------------------------------------
	//  The bots held at the door
	// ---------------------------------------------------------------------
	// A world that has just been made is a world whose rates, respawns and
	// personalities nobody has set yet, and the moment the first bot walks in
	// it is too late to set them without something having happened already
	// (NerrVoVy, 20 September). So the migrator writes this file for a fresh
	// world when the launcher was told to hold them, and the panel's "Wpusc
	// boty do swiata" writes a zero into it.
	//
	// A file rather than a key of the weights, because the two are written by
	// different hands at different moments and sharing one file would be a
	// race for no reason; a file rather than an environment variable, because
	// letting the bots in must outlive a restart of the cores, and .env cannot
	// be edited from the panel. "1" holds, anything else - including no file
	// at all - does not, so an install that never heard of this behaves as it
	// always did.
	const char* const PLAYERBOT_HOLD_PATH = "/opt/m2spool/playerbot_hold";
	bool s_bPlayerBotSpawnHeld = false;
	bool s_bPlayerBotSpawnHeldRead = false;
	bool s_bPlayerBotSpawnHeldReported = false;
	DWORD s_dwPlayerBotHoldNextCheck = 0;

	void ReadPlayerBotHoldFile()
	{
		s_bPlayerBotSpawnHeldRead = true;
		bool held = false;
		FILE* fp = fopen(PLAYERBOT_HOLD_PATH, "r");
		if (fp)
		{
			char line[32] = { 0 };
			if (fgets(line, sizeof(line), fp))
			{
				for (size_t i = 0; i < sizeof(line) && line[i]; ++i)
				{
					if (line[i] == '1')
					{
						held = true;
						break;
					}
					if (line[i] != ' ' && line[i] != '\t')
						break;
				}
			}
			fclose(fp);
		}
		if (held != s_bPlayerBotSpawnHeld || !s_bPlayerBotSpawnHeldReported)
		{
			s_bPlayerBotSpawnHeldReported = true;
			sys_log(0, "PLAYERBOT_CONFIG: the bots are %s (%s)",
					held ? "held at the door" : "free to come in", PLAYERBOT_HOLD_PATH);
		}
		s_bPlayerBotSpawnHeld = held;
	}

	// Asked by every path that would put a bot into the world. The first
	// question reads the file itself: the bootstrap's own spawn runs before
	// the first tick, so waiting for the clock would let a cohort in through
	// the door this is supposed to hold shut.
	bool IsPlayerBotSpawnHeld()
	{
		if (!s_bPlayerBotSpawnHeldRead)
			ReadPlayerBotHoldFile();
		return s_bPlayerBotSpawnHeld;
	}

	void RefreshPlayerBotHold(DWORD dwNow)
	{
		if (s_bPlayerBotSpawnHeldRead && dwNow < s_dwPlayerBotHoldNextCheck)
			return;
		s_dwPlayerBotHoldNextCheck = dwNow + PLAYERBOT_WEIGHT_RELOAD_INTERVAL;
		ReadPlayerBotHoldFile();
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
		RefreshPlayerBotHold(dwNow);

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
		++s_dwPlayerBotWeightsGeneration;
	}

	// The item policy file: one line per vnum or per type, a word after it.
	//
	//   30048	stall	# Kawalek Lodu
	//   type:19	stall	# marmury polimorfii
	//   50703	drop	# Kwiat Kaki po zaliczonym biologu
	//
	// Read the way the weights are: a stat every five seconds, re-read when
	// the file changed, empty when it is gone. Written by the panel's
	// /ai/items page or by hand.
	const char* const PLAYERBOT_ITEM_POLICY_PATH = "/opt/m2spool/playerbot_item_policy.tsv";
	std::map<DWORD, BYTE> s_mapPlayerBotItemPolicyByVnum;
	std::map<BYTE, BYTE> s_mapPlayerBotItemPolicyByType;
	time_t s_tPlayerBotItemPolicyMtime = 0;
	long s_lPlayerBotItemPolicySize = -1;
	DWORD s_dwPlayerBotItemPolicyNextCheck = 0;

	BYTE ParsePlayerBotItemPolicyWord(const char* word)
	{
		if (!word)
			return PLAYERBOT_ITEM_POLICY_NONE;
		if (PlayerBotWeightNameEquals(word, "keep") || PlayerBotWeightNameEquals(word, "zostaw"))
			return PLAYERBOT_ITEM_POLICY_KEEP;
		if (PlayerBotWeightNameEquals(word, "stall") || PlayerBotWeightNameEquals(word, "stragan"))
			return PLAYERBOT_ITEM_POLICY_STALL;
		if (PlayerBotWeightNameEquals(word, "merchant") || PlayerBotWeightNameEquals(word, "handlarz"))
			return PLAYERBOT_ITEM_POLICY_MERCHANT;
		if (PlayerBotWeightNameEquals(word, "drop") || PlayerBotWeightNameEquals(word, "wyrzuc"))
			return PLAYERBOT_ITEM_POLICY_DROP;
		return PLAYERBOT_ITEM_POLICY_NONE;
	}

	void ReadPlayerBotItemPolicyFile(const char* szPath)
	{
		s_mapPlayerBotItemPolicyByVnum.clear();
		s_mapPlayerBotItemPolicyByType.clear();
		FILE* fp = fopen(szPath, "r");
		if (!fp)
			return;
		char line[256];
		int rows = 0;
		while (fgets(line, sizeof(line), fp))
		{
			char* hash = strchr(line, '#');
			if (hash)
				*hash = 0;
			char key[64] = { 0 }, word[32] = { 0 };
			if (sscanf(line, " %63s %31s", key, word) != 2)
				continue;
			const BYTE policy = ParsePlayerBotItemPolicyWord(word);
			if (policy == PLAYERBOT_ITEM_POLICY_NONE)
				continue;
			if (strncmp(key, "type:", 5) == 0)
			{
				const int type = atoi(key + 5);
				if (type > 0 && type < 256)
					s_mapPlayerBotItemPolicyByType[(BYTE)type] = policy;
			}
			else
			{
				const long vnum = atol(key);
				if (vnum > 0)
					s_mapPlayerBotItemPolicyByVnum[(DWORD)vnum] = policy;
			}
			++rows;
		}
		fclose(fp);
		sys_log(0, "PLAYERBOT_CONFIG: item policy read from %s rows=%d vnums=%u types=%u",
				szPath, rows, (unsigned int)s_mapPlayerBotItemPolicyByVnum.size(),
				(unsigned int)s_mapPlayerBotItemPolicyByType.size());
	}

	void RefreshPlayerBotItemPolicy(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotItemPolicyNextCheck)
			return;
		s_dwPlayerBotItemPolicyNextCheck = dwNow + PLAYERBOT_WEIGHT_RELOAD_INTERVAL;
		struct stat st;
		if (stat(PLAYERBOT_ITEM_POLICY_PATH, &st) != 0)
		{
			if (s_lPlayerBotItemPolicySize >= 0)
			{
				sys_log(0, "PLAYERBOT_CONFIG: %s is gone, item policy empty", PLAYERBOT_ITEM_POLICY_PATH);
				s_mapPlayerBotItemPolicyByVnum.clear();
				s_mapPlayerBotItemPolicyByType.clear();
				s_tPlayerBotItemPolicyMtime = 0;
				s_lPlayerBotItemPolicySize = -1;
			}
			return;
		}
		if (st.st_mtime == s_tPlayerBotItemPolicyMtime && (long)st.st_size == s_lPlayerBotItemPolicySize)
			return;
		s_tPlayerBotItemPolicyMtime = st.st_mtime;
		s_lPlayerBotItemPolicySize = (long)st.st_size;
		ReadPlayerBotItemPolicyFile(PLAYERBOT_ITEM_POLICY_PATH);
	}

	// The vnum's word, else the type's, else nothing.
	BYTE GetPlayerBotItemPolicy(LPITEM item)
	{
		if (!item)
			return PLAYERBOT_ITEM_POLICY_NONE;
		std::map<DWORD, BYTE>::const_iterator v = s_mapPlayerBotItemPolicyByVnum.find(item->GetVnum());
		if (v != s_mapPlayerBotItemPolicyByVnum.end())
			return v->second;
		std::map<BYTE, BYTE>::const_iterator t = s_mapPlayerBotItemPolicyByType.find(item->GetType());
		if (t != s_mapPlayerBotItemPolicyByType.end())
			return t->second;
		return PLAYERBOT_ITEM_POLICY_NONE;
	}

	DWORD GetPlayerBotWeightsGeneration()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_dwPlayerBotWeightsGeneration;
	}

	// Whether this bot is one of the scrap keepers: a fixed share by pid, so
	// the same bots keep the role between restarts and the panel's slider
	// says how many there are.
	bool IsPlayerBotScrapKeeper(DWORD dwPID)
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		if (s_iPlayerBotScrapPercent <= 0)
			return false;
		return (int)((dwPID * 2654435761U) % 100U) < s_iPlayerBotScrapPercent;
	}

	// Whether this bot trades its own resources - the unopened chests and the
	// refine scrolls - instead of spending every one of them on itself. A
	// fixed share by pid like the scrap keeper above, and salted apart from it
	// so the two roles do not land on the same bots. See
	// PLAYERBOT_RESOURCE_TRADER_PERCENT for why this is a proportion and not a
	// switch. Not a panel slider: the number is the operator's decision only
	// if it turns out to need tuning, and one reader is not a feature.
	bool IsPlayerBotResourceTrader(DWORD dwPID)
	{
		if (PLAYERBOT_RESOURCE_TRADER_PERCENT <= 0)
			return false;
		return (int)(((dwPID ^ 0x5bf03635U) * 2246822519U) % 100U) <
				PLAYERBOT_RESOURCE_TRADER_PERCENT;
	}

	int GetPlayerBotScrollFromPlus()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_iPlayerBotScrollFromPlus;
	}

	// Whether the refine from this plus may go under a Blessing or Dragon God
	// scroll: it lands on plusLevel + 1, and SCROLL_FROM is the lowest landing
	// a scroll is spent on. Every pass that reaches for a scroll asks this.
	bool IsPlayerBotScrollStepAllowed(BYTE plusLevel)
	{
		return (int)plusLevel + 1 >= GetPlayerBotScrollFromPlus();
	}

	int GetPlayerBotRestPercent()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_iPlayerBotRestPercent;
	}

	int GetPlayerBotTickBudgetMs()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_iPlayerBotTickBudgetMs;
	}

	// The market ledger's count of open counters on a map (playerbot_market.h,
	// which comes long after this fragment).
	int GetPlayerBotStallsOnMap(long lMapIndex);
	// Whether the bot's mood lets it rest at all (playerbot_mood.h): under the
	// PERSONA switch only a SLABY bot does.
	bool PlayerBotMoodAllowsTownRest(LPCHARACTER ch);

	// Whether this bot may stand about in town at all: in a first village, old
	// enough, the REST key above zero, and counters on the map to stand among.
	// A rest is a stroll between stalls, and with none open it was a walk
	// between empty pitches under "Odpoczywam w miescie" - "jak nie ma zadnego
	// sklepu wystawionego, to niech nie ogladaja straganow, bo ich nie ma".
	// Asked when a rest is rolled and on every tick of one, so a slider moved
	// to zero ends the rests already running rather than waiting them out.
	// A dropper does not: its time is its table's, and ten medal droppers were
	// found resting on Yongan's square between two dungeon trips.
	// Under Iwakura's personalities a rest is SLABY's alone: NORMALNY and
	// BARDZO DOBRY stop only for what the game makes them do, and the REST
	// slider now says what share of the SLABY bots rest (19 September).
	bool MayPlayerBotRestInTown(LPCHARACTER ch)
	{
		return ch && IsPlayerBotM1Map(ch->GetMapIndex()) &&
				!IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) &&
				ch->GetLevel() >= PLAYERBOT_TOWN_REST_MIN_LEVEL &&
				GetPlayerBotRestPercent() > 0 &&
				GetPlayerBotStallsOnMap(ch->GetMapIndex()) > 0 &&
				PlayerBotMoodAllowsTownRest(ch);
	}

	bool RollPlayerBotTownRest(LPCHARACTER ch)
	{
		return MayPlayerBotRestInTown(ch) &&
				number(1, 100) <= GetPlayerBotRestPercent();
	}

	bool IsPlayerBotOverheadChatEnabled()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_bPlayerBotOverheadChat;
	}

	// The LIFE switch, asked by CPlayerBotManager::ManageLifeSchedule.
	bool IsPlayerBotShopsInM2Enabled()
	{
		return s_bPlayerBotShopsInM2;
	}

	bool IsPlayerBotLifeScheduleEnabled()
	{
		return s_bPlayerBotLifeSchedule;
	}

	// The PERSONA switch: Iwakura's personalities and moods
	// (playerbot_mood.h, playerbot_persona.h). Every rule that behaves
	// differently under them asks this, so off is today's world, whole.
	bool IsPlayerBotPersonaEnabled()
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_bPlayerBotPersona;
	}

	// The WARS switch, asked by ManagePlayerBotGuildWars.
	bool IsPlayerBotGuildWarsEnabled()
	{
		return s_bPlayerBotGuildWars;
	}

	// The TOWER switch, asked by ManagePlayerBotTowerRaids.
	bool IsPlayerBotTowerRaidsEnabled()
	{
		return s_bPlayerBotTowerRaids;
	}

	// The ISHOP switch, asked by ManagePlayerBotItemShop.
	bool IsPlayerBotItemShopEnabled()
	{
		return s_bPlayerBotItemShop;
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// The bots' wait between two books of one skill, in seconds: the world's
	// difficulty, the event flag m2_bot_book_wait that the migrator writes
	// from M2_DIFFICULTY and the classic panel's difficulty card sets live.
	// The players' wait is the engine's own (m2_book_wait, playerbotify
	// apply_book_wait); the two are separate because a world may want its
	// people to wait and its bots not, or the other way round (drip9660,
	// 23 September). Zero, the easy world's number, is the next book at once.
	int GetPlayerBotBookWaitSeconds()
	{
		const int wait = quest::CQuestManager::instance().GetEventFlag("m2_bot_book_wait");
		return wait > 0 ? wait : 0;
	}
#endif

	bool IsPlayerBotFastBooksEnabled()
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The difficulty decides on this line; the BOOKS switch of the weights
		// file is still read, so an old file writes no warning, and ignored.
		return GetPlayerBotBookWaitSeconds() <= 0;
#else
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		return s_bPlayerBotFastBooks;
#endif
	}

	bool IsPlayerBotNightHour(int hour)
	{
		return hour >= PLAYERBOT_NIGHT_START_HOUR || hour < PLAYERBOT_NIGHT_END_HOUR;
	}

	// The night clock: once a minute, compare what the hour wants with what
	// the event flag says, and ask the DB core to move the flag when they
	// disagree. With the clock off nothing is touched - a GM's own
	// /xmas_snow stays as set - except a night this clock itself raised,
	// which is lowered once so that turning the switch off ends the night.
	void ManagePlayerBotNight(DWORD dwNow)
	{
		if (!s_bPlayerBotWeightsInitialised)
			ResetPlayerBotWeights();
		if (dwNow < s_dwPlayerBotNightNextCheck)
			return;
		s_dwPlayerBotNightNextCheck = dwNow + PLAYERBOT_NIGHT_CHECK_INTERVAL;

		const int current = quest::CQuestManager::instance().GetEventFlag(PLAYERBOT_NIGHT_EVENT_FLAG) ? 1 : 0;
		int wanted;
		if (s_bPlayerBotNight)
		{
			time_t now = time(NULL);
			struct tm local;
			localtime_r(&now, &local);
			wanted = IsPlayerBotNightHour(local.tm_hour) ? 1 : 0;
		}
		else if (s_iPlayerBotNightRequested == 1)
			wanted = 0;
		else
		{
			s_iPlayerBotNightRequested = -1;
			return;
		}

		if (current == wanted)
		{
			s_iPlayerBotNightRequested = wanted;
			return;
		}
		if (s_iPlayerBotNightRequested != wanted)
			sys_log(0, "PLAYERBOT_CONFIG: night clock asks %s=%d (flag=%d)",
					PLAYERBOT_NIGHT_EVENT_FLAG, wanted, current);
		s_iPlayerBotNightRequested = wanted;
		quest::CQuestManager::instance().RequestSetEventFlag(PLAYERBOT_NIGHT_EVENT_FLAG, wanted);
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
