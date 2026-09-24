#ifndef __INC_METIN2_PLAYERBOT_CONV_STATE_H__
#define __INC_METIN2_PLAYERBOT_CONV_STATE_H__

// PlayerBot Conversation v6 - what the bot IS, as the conversation sees it.
//
// BOT AI STATE -> PERSONA -> MOOD
//
// TBotSnapshot is filled by the engine side (playerbot_chat_conversation.h)
// from TPlayerBotAIState and the character, once per reply, only when a reply
// is actually being written. Everything a reply says comes from here, which is
// what keeps the bot from claiming a guild it does not have or a map it is not
// on. The conversation layer never writes to the AI.

#include "playerbot_conv_memory.h"

namespace playerbot_conv
{
	// Mirrors of the AI enums (playerbot_types.h). The engine side maps them;
	// they are repeated here so this layer stays free of engine headers.
	enum EConvAction
	{
		A_IDLE = 0, A_TRAVEL, A_FIGHT, A_LOOT, A_RECOVER, A_TRAIN, A_SHOP, A_REFINE,
		A_READ_BOOK, A_SOCKET, A_PARTY_ASSEMBLE, A_BIOLOGIST, A_STABLE, A_STALL, A_FISHING,
		A_MARKET, A_LURE, A_TOWN_REST, A_MINING
	};

	enum EConvGoal
	{
		G_LEVEL = 0, G_SURVIVE, G_PROFESSION, G_EQUIPMENT, G_RESTOCK, G_REFINE, G_SKILL,
		G_METIN, G_PARTY_CHALLENGE, G_BIOLOGIST, G_HUNTING, G_HORSE, G_FISHING
	};

	// Speaking styles, from the persona (Iwakura's) or the older personality.
	enum EStyle
	{
		S_ADVENTURER = 0, S_GRINDER, S_CONQUEROR, S_MERCHANT, S_GAMBLER, S_PERFECTIONIST,
		S_METIN, S_MINER, S_FISHER, S_MERC, S_COMPANION, S_WANDERER, S_COLLECTOR, S_GEAR,
		S_DROPPER, S_COUNT
	};

	// Five voices are enough for flavour; the style keeps its own interests.
	enum EVoice
	{
		V_GRINDER = 0,   // short, to the point, exp first
		V_WANDERER,      // chatty, curious
		V_MERCHANT,      // prices, deals
		V_FIGHTER,       // Metins, fights, risk
		V_SOCIAL,        // company, asks back
		V_COUNT
	};

	enum EMoodTone
	{
		MOOD_BAD = 0,
		MOOD_NEUTRAL,
		MOOD_GOOD
	};

	inline int VoiceOf(int style)
	{
		switch (style)
		{
			case S_GRINDER: case S_CONQUEROR: case S_PERFECTIONIST: case S_GEAR: case S_DROPPER:
				return V_GRINDER;
			case S_MERCHANT: case S_GAMBLER:
				return V_MERCHANT;
			case S_METIN: case S_MERC:
				return V_FIGHTER;
			case S_COMPANION:
				return V_SOCIAL;
			default:
				return V_WANDERER;
		}
	}

	// ------------------------------------------------ the path and its skills

	// The spoken name of a path: what players write ("body", "WP", "smok")
	// and the game's own word for it.
	inline const char* BuildShortName(int build)
	{
		static const char* const k[B_COUNT] = {
			"", "body", "mental", "dagger", "archer", "WP", "BM", "smok", "heal" };
		return build > B_NONE && build < B_COUNT ? k[build] : "";
	}

	inline const char* BuildGameName(int build)
	{
		static const char* const k[B_COUNT] = {
			"", "cialo", "umysl", "skrytobojca", "lucznik", "magiczna bron", "czarna magia", "smok", "leczenie" };
		return build > B_NONE && build < B_COUNT ? k[build] : "";
	}

	// The window's own grades: 1-19, M1-M10 from twenty, G1-G10 from thirty,
	// P at forty.
	inline std::string SkillGradeText(int level)
	{
		char buf[8];
		if (level >= 40)
			return "P";
		if (level >= 30)
			snprintf(buf, sizeof(buf), "G%d", level - 29);
		else if (level >= 20)
			snprintf(buf, sizeof(buf), "M%d", level - 19);
		else
			snprintf(buf, sizeof(buf), "%d", level < 0 ? 0 : level);
		return buf;
	}

	// The class skills by vnum, as the item table names their books (the
	// price table's names, ASCII).
	inline const char* SkillNameOf(unsigned int vnum)
	{
		static const struct { unsigned int vnum; const char* name; } k[] = {
			{ 1, "Trzystronne Ciecie" }, { 2, "Wir Miecza" }, { 3, "Berserk" }, { 4, "Aura Miecza" },
			{ 5, "Szarza" }, { 16, "Duchowe Uderzenie" }, { 17, "Tapniecie" }, { 18, "Uderzenie Miecza" },
			{ 19, "Silne Cialo" }, { 20, "Walniecie" }, { 31, "Zasadzka" }, { 32, "Szybki Atak" },
			{ 33, "Wirujacy Sztylet" }, { 34, "Krycie Sie" }, { 35, "Trujaca Chmura" },
			{ 46, "Powtarzalny Strzal" }, { 47, "Deszcz Strzal" }, { 48, "Ognista Strzala" },
			{ 49, "Bezszelestny Chod" }, { 50, "Trujaca Strzala" }, { 61, "Uderzenie Palcem" },
			{ 62, "Smoczy Wir" }, { 63, "Czarowane Ostrze" }, { 64, "Strach" }, { 65, "Czarowana Zbroja" },
			{ 66, "Rozproszenie Magii" }, { 76, "Mroczne Uderzenie" }, { 77, "Ogniste Uderzenie" },
			{ 78, "Ognisty Duch" }, { 79, "Mroczna Ochrona" }, { 80, "Duchowy Cios" }, { 81, "Mroczny Kamien" },
			{ 91, "Latajacy Talizman" }, { 92, "Strzelajacy Smok" }, { 93, "Smoczy Skowyt" },
			{ 94, "Blogoslawienstwo" }, { 95, "Odbicie" }, { 96, "Pomoc Smoka" }, { 106, "Blyskawiczny Rzut" },
			{ 107, "Przywolanie Blyskawicy" }, { 108, "Szpon Blyskawicy" }, { 109, "Leczenie" },
			{ 110, "Zwinnosc" }, { 111, "Zwiekszenie Ataku" }
		};
		for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
			if (k[i].vnum == vnum)
				return k[i].name;
		return "";
	}

	// The Shaman's buffs, three to a path: the dragon's first, then healing.
	const unsigned int CONV_SKILL_BLESSING = 94;
	const unsigned int CONV_SKILL_REFLECT = 95;
	const unsigned int CONV_SKILL_DRAGON_AID = 96;
	const unsigned int CONV_SKILL_CURE = 109;
	const unsigned int CONV_SKILL_SWIFTNESS = 110;
	const unsigned int CONV_SKILL_ATTACK_UP = 111;

	inline int BuffBuildOf(unsigned int skill)
	{
		if (skill >= CONV_SKILL_BLESSING && skill <= CONV_SKILL_DRAGON_AID)
			return B_DRAGON;
		if (skill >= CONV_SKILL_CURE && skill <= CONV_SKILL_ATTACK_UP)
			return B_HEAL;
		return B_NONE;
	}

	// One buff as the engine would put it on the person asking: its own skill
	// proto evaluated with ComputeSkill's variables at the bot's level and
	// stats (playerbot_chat_conversation.h), so the numbers are the game's.
	struct TBuffLine
	{
		unsigned int skill;
		int level;          // the bot's level in it, 0 = not learnt
		bool known;         // the proto was found and evaluated
		int amount;         // the effect (percent, points or the heal)
		int amountMax;      // the heal at the top of the weapon's roll; = amount otherwise
		int amount2;        // Swiftness: casting speed percent
		int amount3;        // Cure (mt2009): the shield against monsters
		int seconds;        // how long it lasts, 0 for an instant heal
		int seconds3;       // how long the shield lasts
		TBuffLine() : skill(0), level(0), known(false), amount(0), amountMax(0), amount2(0), amount3(0),
			seconds(0), seconds3(0) {}
	};

	struct TBuffReport
	{
		int count;
		TBuffLine lines[3];
		bool onAsker;       // evaluated with the person as the target
		bool cutForOthers;  // mt2009: a buff on somebody else is cut (the lines already say so)
		TBuffReport() : count(0), onAsker(false), cutForOthers(false) {}
	};

	// ------------------------------------------------------------- summoning

	// Near enough to stand still: the engine's walk stops inside this and the
	// reply says "Jestem obok" inside it. One number for both.
	const int CONV_SUMMON_NEAR_DISTANCE = 600;

	// Why a bot cannot come over now. The engine side decides it from the AI
	// state (GetPlayerBotSummonBlock); the conversation only says it.
	enum ESummonBlock
	{
		SB_NONE = 0,
		SB_OTHER_MAP,      // the person is on another map
		SB_STALL,          // standing behind its own stall
		SB_FISHING,        // a fishing session
		SB_MINING,         // at a vein
		SB_DUEL,
		SB_GUILD_WAR,
		SB_TOWER,          // the Demon Tower: a raid, a floor, or waiting for one
		SB_DUNGEON,        // any other dungeon instance
		SB_MERC,           // a mercenary's contract
		SB_OTHER_PARTY,    // with another person: their party, their order
		SB_OTHER_SUMMON,   // already on its way to somebody else
		SB_DEAD,
		SB_COUNT
	};

	inline const char* SummonBlockName(int b)
	{
		static const char* const k[SB_COUNT] = {
			"none", "other_map", "stall", "fishing", "mining", "duel", "guild_war", "tower", "dungeon",
			"merc", "other_party", "other_summon", "dead" };
		return b >= 0 && b < SB_COUNT ? k[b] : "?";
	}

	// What StartSummon answers.
	enum ESummonStart
	{
		SUMMON_START_OK = 0,     // on its way (or already beside the person)
		SUMMON_START_RENEWED,    // called again by the same person: the stay starts over
		SUMMON_START_BLOCKED,    // something the snapshot did not see yet (a race)
		SUMMON_START_FAILED      // the person or the bot is gone
	};

	// Why a summon ended.
	enum ESummonEnd
	{
		SUMMON_END_NONE = 0,
		SUMMON_END_DISMISSED,    // "mozesz isc", "dzieki", "nara"
		SUMMON_END_EXPIRED,      // the few minutes are over
		SUMMON_END_PLAYER_LEFT,  // the person left the map
		SUMMON_END_PLAYER_GONE,  // the person logged out
		SUMMON_END_UNREACHABLE,  // the walk did not get there in time
		SUMMON_END_BLOCKED,      // something with a better claim came (a duel, a war...)
		SUMMON_END_NOT_SUMMONED, // nothing to end
		SUMMON_END_COUNT
	};

	inline const char* SummonEndName(int e)
	{
		static const char* const k[SUMMON_END_COUNT] = {
			"none", "dismissed", "expired", "player_left", "player_gone", "unreachable", "blocked", "not_summoned" };
		return e >= 0 && e < SUMMON_END_COUNT ? k[e] : "?";
	}

	// --------------------------------------------------------------- snapshot

	struct TBotSnapshot
	{
		std::string name;
		std::string askerName;
		int level;
		int job;            // 0 warrior 1 ninja 2 sura 3 shaman
		int empire;         // 1 Shinsoo 2 Chunjo 3 Jinno
		long mapIndex;
		bool inTown;
		bool safeZone;
		bool inDungeon;
		int action;         // EConvAction
		int goal;           // EConvGoal
		long travelMap;
		bool riding;
		std::string targetName;
		bool targetStone;
		bool targetBoss;
		bool targetPlayer;
		int hpPct;
		int spPct;
		bool dead;
		int expPct;         // -1 unknown
		long long gold;
		int horseLevel;
		bool inParty;
		int partySize;
		std::string partyLeader;
		bool leaderIsMe;
		bool askerInParty;
		bool inGuild;
		std::string guildName;
		int guildMembers;
		int freeCells;
		int bagCells;
		std::string weaponName;
		int weaponPlus;
		std::string armorName;
		int armorPlus;
		bool fishing;
		bool mining;
		bool herbUnlocked;
		std::string bioWanted;
		bool metinHunter;
		bool demonTower;
		bool guildWar;
		bool mercContract;
		bool luring;
		bool luringForAsker;
		bool shopOpen;          // a stall of any kind is up (classic or the offline shop)
		bool shopStanding;      // the classic one: the bot itself stands behind it
		long shopMapIndex;      // where the stall stands (the offline one stays while the bot hunts)
		bool shopOtherChannel;
		int shopItems;
		std::string shopSummary;
		std::string bagSummary;   // a few things from the bag, "Kosc x3, Miecz +5"
		bool marketTrip;
		int mobsNear;       // -1 unknown
		int playersNear;
		int style;          // EStyle
		int mood;           // EMoodTone
		bool unlucky;       // long drought of good drops
		bool euphoria;      // a big refine lately
		int affinity;       // the AI's friend ledger for the asker
		u32 onlineMinutes;
		u32 goalMinutes;
		u32 actionMinutes;
		int recentDeaths;
		u32 minutesSinceDeath; // 0xFFFFFFFF never
		int askerLevel;
		bool askerNear;
		int hour;           // server local 0..23
		bool afk;
		std::string huntMob;
		int huntRemaining;
		int dragonCoins;
		bool dragonKnown;
		// The class's path and its skills.
		int skillGroup;             // 0 before the trainer, 1 or 2 after
		unsigned int skillVnums[6]; // the path's skills in the build's order, 0 = none
		int skillLevels[6];         // their levels, 0 = not learnt
		unsigned int mainSkill;     // the one the build raises first
		// Coming over when called (playerbot_chat_conversation.h).
		int summonBlock;            // ESummonBlock: why it could not come now
		bool summoned;              // on its way to, or standing with, somebody who called it
		bool summonedByAsker;
		bool summonArrived;         // standing with them, not still walking
		std::string summonerName;   // who, when it is somebody else
		bool askerOnMap;
		int askerDistance;          // units, -1 on another map

		TBotSnapshot() : level(1), job(0), empire(0), mapIndex(0), inTown(false), safeZone(false),
			inDungeon(false), action(A_IDLE), goal(G_LEVEL), travelMap(0), riding(false),
			targetStone(false), targetBoss(false), targetPlayer(false), hpPct(100), spPct(100),
			dead(false), expPct(-1), gold(0), horseLevel(0), inParty(false), partySize(0),
			leaderIsMe(false), askerInParty(false), inGuild(false), guildMembers(0), freeCells(0),
			bagCells(0), weaponPlus(0), armorPlus(0), fishing(false), mining(false),
			herbUnlocked(false), metinHunter(false), demonTower(false), guildWar(false),
			mercContract(false), luring(false), luringForAsker(false), shopOpen(false), shopStanding(false), shopMapIndex(0),
			shopOtherChannel(false), shopItems(0),
			marketTrip(false), mobsNear(-1), playersNear(0), style(S_ADVENTURER), mood(MOOD_NEUTRAL),
			unlucky(false), euphoria(false), affinity(0), onlineMinutes(0), goalMinutes(0),
			actionMinutes(0), recentDeaths(0), minutesSinceDeath(0xFFFFFFFFu), askerLevel(0),
			askerNear(false), hour(12), afk(false), huntRemaining(0), dragonCoins(0), dragonKnown(false),
			skillGroup(0), mainSkill(0), summonBlock(SB_NONE), summoned(false), summonedByAsker(false),
			summonArrived(false), askerOnMap(false), askerDistance(-1)
		{
			for (int i = 0; i < 6; ++i)
			{
				skillVnums[i] = 0;
				skillLevels[i] = 0;
			}
		}

		int Build() const { return BuildOf(job, skillGroup); }
	};

	// Things only the engine can look up, asked for while a reply is written.
	class IConvWorld
	{
		public:
			virtual ~IConvWorld() {}
			// "masz tarcze?" - an item in the bag matching the folded query.
			virtual bool FindItem(const std::string& query, std::string& outName, unsigned int& outCount) = 0;
			// "masz na straganie X?" - a line of the bot's own stall (classic or
			// the offline shop) matching the folded query, aliases included.
			virtual bool FindShopItem(const std::string& query, std::string& outName, long long& outPrice,
					unsigned int& outCount) = 0;
			// "ile chodzi X?" - the cheapest line of X on the market's stalls, or
			// the sale memory's price. False when nobody knows.
			virtual bool FindMarketPrice(const std::string& query, std::string& outName, long long& outPrice,
					unsigned int& outSellers) = 0;
			// "kupisz ode mnie X" - the trade layer's answer, or empty.
			virtual std::string AnswerSell(const std::string& query) = 0;
			// "co daja twoje buffy?" - a Shaman's buffs of its path, evaluated
			// the way the engine would cast them on the person. False when the
			// bot has no buffs to evaluate.
			virtual bool DescribeBuffs(TBuffReport& out) { (void)out; return false; }
			// "chodz do mnie" - the walk to the person begins (ESummonStart). The
			// engine asks its own gates once more; the reply says what it
			// answered.
			virtual int StartSummon() { return SUMMON_START_FAILED; }
			// "mozesz isc" - the bot goes back to its own life (ESummonEnd).
			virtual int EndSummon() { return SUMMON_END_NOT_SUMMONED; }
	};

	// ------------------------------------------------------------------ maps

	struct TMapWords
	{
		long index;
		const char* name;     // "Dolina Orkow"
		const char* at;       // "w Dolinie Orkow"
		const char* atShort;  // "w Dolinie"
		const char* to;       // "do Doliny Orkow"
		bool town;
	};

	inline const TMapWords& GetMapWords(long mapIndex)
	{
		static const TMapWords kMaps[] = {
			{ 1, "Yongan", "w Yongan", "w Yongan", "do Yongan", true },
			{ 2, "Waryong", "w Waryong", "w Waryong", "do Waryong", false },
			{ 3, "Jayang", "w Jayang", "w Jayang", "do Jayang", true },
			{ 4, "Ziemia Klanu", "na Ziemi Klanu", "na Ziemi Klanu", "na Ziemie Klanu", false },
			{ 5, "Loch Malp", "w Lochu Malp", "w Lochu Malp", "do Lochu Malp", false },
			{ 21, "Joan", "w Joan", "w Joan", "do Joan", true },
			{ 23, "Bokjung", "w Bokjung", "w Bokjung", "do Bokjung", true },
			{ 24, "Ziemia Klanu", "na Ziemi Klanu", "na Ziemi Klanu", "na Ziemie Klanu", false },
			{ 25, "Loch Malp", "w Lochu Malp", "w Lochu Malp", "do Lochu Malp", false },
			{ 41, "Pyongmoo", "w Pyongmoo", "w Pyongmoo", "do Pyongmoo", true },
			{ 43, "Bakra", "w Bakra", "w Bakra", "do Bakra", true },
			{ 44, "Ziemia Klanu", "na Ziemi Klanu", "na Ziemi Klanu", "na Ziemie Klanu", false },
			{ 45, "Loch Malp", "w Lochu Malp", "w Lochu Malp", "do Lochu Malp", false },
			{ 61, "Gora Sohan", "na Gorze Sohan", "na Sohan", "na Gore Sohan", false },
			{ 62, "Ognista Ziemia", "na Ognistej Ziemi", "na Ognistej", "na Ognista Ziemie", false },
			{ 63, "Pustynia Yongbi", "na Pustyni Yongbi", "na Pustyni", "na Pustynie Yongbi", false },
			{ 64, "Dolina Orkow", "w Dolinie Orkow", "w Dolinie", "do Doliny Orkow", false },
			{ 65, "Swiatynia Hwang", "w Swiatyni Hwang", "w Swiatyni", "do Swiatyni Hwang", false },
			{ 66, "Wieza Demonow", "w Wiezy Demonow", "w Wiezy", "do Wiezy Demonow", false },
			{ 67, "Las Duchow", "w Lesie Duchow", "w Lesie Duchow", "do Lasu Duchow", false },
			{ 68, "Czerwony Las", "w Czerwonym Lesie", "w Czerwonym Lesie", "do Czerwonego Lasu", false },
			{ 71, "Loch Pajakow II", "w Lochu Pajakow II", "w Lochu Pajakow", "do Lochu Pajakow II", false },
			{ 104, "Loch Pajakow", "w Lochu Pajakow", "w Lochu Pajakow", "do Lochu Pajakow", false },
			{ 108, "Loch Malp II", "w Lochu Malp II", "w Lochu Malp", "do Lochu Malp II", false },
			{ 109, "Loch Malp III", "w Lochu Malp III", "w Lochu Malp", "do Lochu Malp III", false },
		};
		static const TMapWords kUnknown = { 0, "", "tutaj", "tutaj", "", false };
		for (size_t i = 0; i < sizeof(kMaps) / sizeof(kMaps[0]); ++i)
			if (kMaps[i].index == mapIndex)
				return kMaps[i];
		return kUnknown;
	}

	inline bool IsKnownMap(long mapIndex)
	{
		return GetMapWords(mapIndex).index != 0;
	}

	inline const char* ClassName(int job)
	{
		switch (job)
		{
			case 0: return "wojownik";
			case 1: return "ninja";
			case 2: return "sura";
			case 3: return "szaman";
			default: return "postac";
		}
	}

	// "Gram wojownikiem" - the instrumental a sentence needs.
	inline const char* ClassNameInstr(int job)
	{
		switch (job)
		{
			case 0: return "wojownikiem";
			case 1: return "ninja";
			case 2: return "sura";
			case 3: return "szamanem";
			default: return "swoja postacia";
		}
	}

	inline const char* EmpireName(int empire)
	{
		switch (empire)
		{
			case 1: return "Shinsoo";
			case 2: return "Chunjo";
			case 3: return "Jinno";
			default: return "";
		}
	}

	// "Dziki Wilk" -> "wilki". A plural a person would use, or empty when the
	// name is not one of the families. The proto name is folded by the caller.
	inline const char* MobFamilyPlural(const std::string& foldedName)
	{
		static const char* const kFam[][2] = {
			{ "ork", "orki" }, { "pajak", "pajaki" }, { "wilk", "wilki" }, { "niedzwied", "niedzwiedzie" },
			{ "tygrys", "tygrysy" }, { "dzik", "dziki" }, { "malp", "malpy" }, { "szkielet", "szkielety" },
			{ "duch", "duchy" }, { "demon", "demony" }, { "zombi", "zombie" }, { "skorpion", "skorpiony" },
			{ "bestia", "bestie" }, { "zolnierz", "zolnierzy" }, { "yeti", "yeti" }, { "lisy", "lisy" }, { "lis ", "lisy" },
			{ "jelen", "jelenie" }, { "waz", "weze" }, { "zmij", "zmije" }, { "golem", "golemy" },
			{ "upior", "upiory" }, { "drzew", "drzewce" }, { "kaplan", "kaplanow" }, { "bandyt", "bandytow" },
			{ "lucznik", "lucznikow" }, { "wojownik", "wojownikow" }, { "lodow", "lodowe potwory" },
			{ "ognist", "ogniste potwory" }, { "sura", "surow" }, { "nietoperz", "nietoperze" },
		};
		for (size_t i = 0; i < sizeof(kFam) / sizeof(kFam[0]); ++i)
			if (foldedName.find(kFam[i][0]) != std::string::npos)
				return kFam[i][1];
		return "";
	}
}

#endif
