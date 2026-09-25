#ifndef __INC_METIN2_PLAYERBOT_CONV_INTENTS_H__
#define __INC_METIN2_PLAYERBOT_CONV_INTENTS_H__

// PlayerBot Conversation v6 - intent detection (pure).
//
// TOKENS -> CONCEPTS -> INTENT (+ topic, question type, follow-up kind)
//
// An intent is chosen by scoring rules over the concept set, not by matching
// sentences. "co robisz", "co porabiasz", "czym sie zajmujesz", "co tam
// robisz teraz" all carry {C_WHAT, C_DO} and so are all I_ACTIVITY; "gdzie
// teraz exp", "gdzie bijesz", "gdzie expisz" all carry {C_WHERE, C_EXP|C_HIT}
// and are I_ACTIVITY_LOCATION. Word order does not matter, extra words do not
// matter, a missing '?' does not matter.
//
// A rule is: all of `req`, at least one of `oneof` (when given), none of
// `forbid`; score = base + 4 per `bonus` concept present. Preference words
// (lubisz, gdybys, marzysz...) take 25 points off every game rule that does
// not ask for them, which is what keeps "co lubisz robic" out of I_ACTIVITY.
//
// Short lines that carry no content of their own ("gdzie?", "duzo ich?",
// "sam?", "dlaczego?", "a ty?", "serio?") become I_FOLLOW_UP with a kind; what
// they mean is decided from the conversation memory (playerbot_conv_memory.h).
//
// Adding an intent: one enum value, one or more rules below, one generator.

#include "playerbot_conv_lexicon.h"

namespace playerbot_conv
{
	enum EIntent
	{
		I_NONE = 0,
		// social
		I_GREETING, I_FAREWELL, I_THANKS, I_APOLOGY, I_HOW_ARE_YOU, I_HELP,
		I_IS_BOT, I_INSULT, I_PRAISE, I_AGE, I_ORIGIN, I_KS, I_READY, I_GOODLUCK, I_BRB,
		// a person talking at the bot rather than asking it anything ("przestan
		// do mnie pisac", a ban threatened, banter), a sum to work out, and
		// "przeciez mowiles, ze..."
		I_STOP_TALKING, I_THREAT, I_MOCK, I_MATH, I_CONTRADICTION,
		// identity
		I_NAME, I_LEVEL, I_CLASS, I_EMPIRE, I_PERSONALITY, I_MOOD,
		// state
		I_ACTIVITY, I_ACTIVITY_LOCATION, I_LOCATION, I_TARGET, I_MOB_COUNT,
		I_GOAL, I_NEXT_PLAN, I_HP, I_GOLD, I_HORSE, I_EQUIPMENT, I_INVENTORY,
		I_INVENTORY_SPACE, I_ITEM_OWN, I_PARTY, I_GUILD, I_FISHING, I_MINING,
		I_HERBALISM, I_BIOLOGIST, I_METIN, I_DEMON_TOWER, I_GUILD_WAR, I_MERCENARY,
		I_PARTY_REQUEST, I_SHOP, I_MARKET, I_BUY, I_SELL, I_SKILLS, I_PVP, I_TRAVEL,
		I_REST, I_REFINE, I_MISSIONS, I_DEATH, I_RELATIONSHIP, I_TIME_HERE,
		I_MAP_OPINION, I_DROP_LUCK, I_PROGRESS_TODAY, I_PRICE, I_ITEMSHOP,
		// the class's path, and what a Shaman's buffs give
		I_BUILD, I_BUFFS,
		// the bot's gear argued about ("czemu nie wymienisz broni?", "kup sobie
		// riba", "co myslisz o srednich?"), a better map suggested, an item
		// shown off, and one offered
		I_GEAR_WHY, I_GEAR_ADVICE, I_GEAR_OPINION, I_MAP_ADVICE, I_SHOW_ITEM, I_GIFT_OFFER,
		// asking the bot to come over, and letting it go again
		I_SUMMON, I_DISMISS,
		// conversation mechanics
		I_FOLLOW_UP, I_ACK, I_LAUGH, I_YES, I_NO, I_ANSWER_TO_BOT,
		// everything that is not the game
		I_GENERAL,
		I_UNKNOWN_QUESTION, I_UNKNOWN_STATEMENT,
		I_COUNT
	};

	enum ETopic
	{
		T_NONE = 0,
		T_WEATHER, T_SEASON, T_DAYTIME, T_SLEEP, T_TIRED, T_BORED, T_HOBBY, T_FOOD,
		T_DRINK, T_TRAVEL, T_MUSIC, T_MOVIES, T_GAMES, T_HUMOR, T_LUCK, T_FRIENDSHIP,
		T_TEAMWORK, T_LONELY, T_RISK, T_MONEY, T_WORK, T_SCHOOL, T_LIFE, T_DREAMS,
		T_FEAR, T_ANNOY, T_JOY, T_FEELINGS, T_ANIMALS, T_SPORT, T_LOVE, T_BOOKS,
		T_NATURE, T_ADVENTURE, T_COLOR,
		T_COUNT
	};

	enum EQType
	{
		Q_STATEMENT = 0,   // "zimno dzisiaj"
		Q_LIKE,            // "lubisz zime?"
		Q_DISLIKE,         // "czego nie lubisz?"
		Q_WHAT_LIKE,       // "co lubisz robic?"
		Q_FAVORITE,        // "jaki twoj ulubiony film?"
		Q_CHOICE,          // "wolisz zime czy lato?"
		Q_HYPO,            // "gdybys mogl..."
		Q_DREAM,           // "masz jakies marzenia?"
		Q_FEAR,            // "czego sie boisz?"
		Q_ANNOY,           // "co cie denerwuje?"
		Q_JOY,             // "co cie cieszy?"
		Q_OPINION,         // "co myslisz o..."
		Q_CAN,             // "umiesz grac na gitarze?"
		Q_EVER,            // "byles kiedys nad morzem?"
		Q_KNOW,            // "znasz jakies dobre filmy?"
		Q_WANT,            // "chcialbys..."
		Q_FACT,            // "ile jest gwiazd", "kto wygral mecz"
		Q_OPEN,            // any other question
		Q_MIRROR           // "a ty?" - the bot's own view on the player's topic
	};

	enum EFollow
	{
		F_NONE = 0,
		F_WHERE, F_COUNT, F_ALONE, F_WITHWHO, F_WHY, F_HOW, F_CONFIRM, F_NEXT,
		F_MIRROR, F_WHAT, F_THIS, F_WHEN, F_WHO
	};

	struct TAnalysis
	{
		EIntent intent;       // after context resolution
		EIntent rawIntent;    // what the line alone said
		EIntent subject;      // for follow-ups / why / confirm: what it is about
		EFollow follow;
		ETopic topic;
		EQType qtype;
		TConceptSet concepts;
		TTokens tokens;
		std::string object;   // "zime", "gitarze", "kotach"
		std::string objectB;  // second option of a choice
		int score;
		bool question;
		bool greetingToo;     // "hej, co robisz?" - answer and greet
		bool thanksToo;
		bool returnToTopic;   // back to the game after small talk
		bool topicChange;
		bool repeated;        // the same line again, shortly after
		bool polarityNegative;// "malo?", "nie lubie"
		u32 at;
		u32 gapBefore;        // ms since the player's previous line (0 = first)
		long long offerYang;  // "za 2kk" - a sum named in the line
		long mentionMap;      // "v1", "m1", "dolina" - a place named (see FindMapAlias)
		// For I_ANSWER_TO_BOT: which of the bot's questions (EBotAsk) it answers.
		// The memory closes the question the moment the line arrives, and the
		// reply is composed a second later, so it has to travel with the line.
		unsigned char answeredAsk;
		// An item named in a line about gear, or shown: "fms 9" is 9, a link
		// "[Miecz Pelni Ksiezyca+9]" 9, -1 when the line says no refine.
		int objectPlus;
		// "bronia na 15 level": a level the line names, 0 when none.
		int levelNamed;
		bool itemLink;          // the item came as a shift-clicked link (or in brackets)
		std::string itemShown;  // how a reply says it back: the link as it came, "FMS +9"
		// I_MATH: the result as a person writes it ("4", "2,5"), and why there is none.
		std::string mathText;
		bool mathMixed;
		bool mathDivZero;
		bool mathTooBig;

		TAnalysis() : intent(I_NONE), rawIntent(I_NONE), subject(I_NONE), follow(F_NONE),
			topic(T_NONE), qtype(Q_STATEMENT), score(0), question(false), greetingToo(false),
			thanksToo(false), returnToTopic(false), topicChange(false), repeated(false),
			polarityNegative(false), at(0), gapBefore(0), offerYang(0), mentionMap(0), answeredAsk(0),
			objectPlus(-1), levelNamed(0), itemLink(false), mathMixed(false), mathDivZero(false),
			mathTooBig(false) {}
	};

	// The summon and its release are requests, not a subject the conversation
	// returns to, so they sit outside the game range.
	inline bool IsGameIntent(EIntent i)
	{
		return i >= I_NAME && i <= I_GIFT_OFFER;
	}

	inline bool IsSocialIntent(EIntent i)
	{
		return i >= I_GREETING && i <= I_CONTRADICTION;
	}

	// A line that talks at the bot rather than to it: the reply is short and
	// asks nothing back, and no mood is volunteered after it.
	inline bool IsColdIntent(EIntent i)
	{
		return i == I_STOP_TALKING || i == I_THREAT || i == I_INSULT || i == I_MOCK;
	}

	// A line arguing with the bot about what it has, where it is or what it
	// said: never an answer to a question the bot asked.
	inline bool IsArgumentIntent(EIntent i)
	{
		return i == I_GEAR_WHY || i == I_GEAR_ADVICE || i == I_GEAR_OPINION || i == I_MAP_ADVICE ||
				i == I_SHOW_ITEM || i == I_GIFT_OFFER || i == I_CONTRADICTION;
	}

	inline bool IsReactionIntent(EIntent i)
	{
		return i == I_ACK || i == I_LAUGH || i == I_YES || i == I_NO;
	}

	// Groups of game intents that are "the same subject": a return to any of
	// them after small talk reads as going back to the topic.
	inline int GameTopicGroup(EIntent i)
	{
		switch (i)
		{
			case I_ACTIVITY: case I_ACTIVITY_LOCATION: case I_LOCATION: case I_TARGET:
			case I_MOB_COUNT: case I_TIME_HERE: case I_MAP_OPINION: case I_TRAVEL: case I_REST:
			case I_MAP_ADVICE:
				return 1;
			case I_GOAL: case I_NEXT_PLAN: case I_PROGRESS_TODAY:
				return 2;
			case I_PARTY: case I_PARTY_REQUEST: case I_GUILD: case I_GUILD_WAR: case I_MERCENARY:
				return 3;
			case I_GOLD: case I_SHOP: case I_MARKET: case I_BUY: case I_SELL: case I_INVENTORY:
			case I_INVENTORY_SPACE: case I_ITEM_OWN: case I_EQUIPMENT: case I_REFINE:
			case I_PRICE: case I_ITEMSHOP: case I_GEAR_WHY: case I_GEAR_ADVICE: case I_GEAR_OPINION:
			case I_SHOW_ITEM: case I_GIFT_OFFER:
				return 4;
			case I_FISHING: case I_MINING: case I_HERBALISM: case I_BIOLOGIST: case I_METIN:
			case I_DEMON_TOWER: case I_MISSIONS: case I_DROP_LUCK:
				return 5;
			default:
				return IsGameIntent(i) ? 6 : 0;
		}
	}

	inline const char* IntentName(EIntent i)
	{
		static const char* const kNames[] = {
			"NONE", "GREETING", "FAREWELL", "THANKS", "APOLOGY", "HOW_ARE_YOU", "HELP",
			"IS_BOT", "INSULT", "PRAISE", "AGE", "ORIGIN", "KS", "READY", "GOOD_LUCK", "BRB",
			"STOP_TALKING", "THREAT", "MOCK", "MATH", "CONTRADICTION",
			"NAME", "LEVEL", "CLASS", "EMPIRE", "PERSONALITY", "MOOD",
			"CURRENT_ACTIVITY", "ACTIVITY_LOCATION", "LOCATION", "TARGET", "MOB_COUNT",
			"GOAL", "NEXT_PLAN", "HP", "GOLD", "HORSE", "EQUIPMENT", "INVENTORY",
			"INVENTORY_SPACE", "ITEM_OWN", "PARTY", "GUILD", "FISHING", "MINING",
			"HERBALISM", "BIOLOGIST", "METIN", "DEMON_TOWER", "GUILD_WAR", "MERCENARY",
			"PARTY_REQUEST", "SHOP", "MARKET", "BUY", "SELL", "SKILLS", "PVP", "TRAVEL",
			"REST", "REFINE", "MISSIONS", "DEATH", "RELATIONSHIP", "TIME_HERE",
			"MAP_OPINION", "DROP_LUCK", "PROGRESS_TODAY", "PRICE", "ITEMSHOP",
			"BUILD", "BUFFS", "GEAR_WHY", "GEAR_ADVICE", "GEAR_OPINION", "MAP_ADVICE", "SHOW_ITEM",
			"GIFT_OFFER", "SUMMON", "DISMISS",
			"FOLLOW_UP", "ACK", "LAUGH", "YES", "NO", "ANSWER_TO_BOT",
			"GENERAL_CONVERSATION", "UNKNOWN_QUESTION", "UNKNOWN_STATEMENT"
		};
		typedef char TIntentNamesFit[sizeof(kNames) / sizeof(kNames[0]) == I_COUNT ? 1 : -1];
		(void)sizeof(TIntentNamesFit);
		return (i >= 0 && i < I_COUNT) ? kNames[i] : "?";
	}

	inline const char* TopicName(ETopic t)
	{
		static const char* const kNames[] = {
			"NONE", "WEATHER", "SEASON", "DAYTIME", "SLEEP", "TIRED", "BORED", "HOBBY", "FOOD",
			"DRINK", "TRAVEL", "MUSIC", "MOVIES", "GAMES", "HUMOR", "LUCK", "FRIENDSHIP",
			"TEAMWORK", "LONELY", "RISK", "MONEY", "WORK", "SCHOOL", "LIFE", "DREAMS",
			"FEAR", "ANNOY", "JOY", "FEELINGS", "ANIMALS", "SPORT", "LOVE", "BOOKS",
			"NATURE", "ADVENTURE", "COLOR"
		};
		return (t >= 0 && t < T_COUNT) ? kNames[t] : "?";
	}

	inline const char* FollowName(EFollow f)
	{
		static const char* const kNames[] = {
			"NONE", "WHERE", "COUNT", "ALONE", "WITH_WHO", "WHY", "HOW", "CONFIRM", "NEXT",
			"MIRROR", "WHAT", "THIS", "WHEN", "WHO"
		};
		return (f >= 0 && f <= F_WHO) ? kNames[f] : "?";
	}

	// ------------------------------------------------------------------ rules

	struct TIntentRule
	{
		unsigned char intent;
		unsigned char topic;
		short score;
		unsigned char req[3];
		unsigned char oneof[5];
		unsigned char forbid[5];
		unsigned char bonus[3];
	};

#define PBC_R(i, t, s) (unsigned char)(i), (unsigned char)(t), (short)(s)

	inline const TIntentRule* GetIntentRules(size_t& count)
	{
		static const TIntentRule kRules[] = {
			// ---------------- social
			{ PBC_R(I_GREETING, 0, 58), { C_GREET }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_FAREWELL, 0, 60), { C_BYE }, { 0 }, { C_WHERE, C_WHY }, { 0 } },
			{ PBC_R(I_THANKS, 0, 60), { C_THANKS }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_APOLOGY, 0, 60), { C_SORRY }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_HOW_ARE_YOU, 0, 62), { C_HOWAREYOU }, { 0 }, { C_DO, C_WHERE }, { 0 } },
			{ PBC_R(I_HOW_ARE_YOU, 0, 44), { C_HOW, C_YOU }, { 0 }, { C_LIKE, C_THINK, C_HYPO }, { 0 } },
			{ PBC_R(I_HELP, 0, 60), { C_HELP }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_IS_BOT, 0, 68), { C_BOT }, { 0 }, { C_METIN }, { C_BE, C_YOU } },
			{ PBC_R(I_INSULT, 0, 72), { C_INSULT }, { 0 }, { 0 }, { C_YOU } },
			{ PBC_R(I_PRAISE, 0, 55), { C_PRAISE }, { 0 }, { 0 }, { C_YOU } },
			{ PBC_R(I_AGE, 0, 72), { C_AGE }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_ORIGIN, 0, 66), { C_ORIGIN }, { 0 }, { C_HYPO }, { C_BE, C_YOU } },
			// ---------------- identity
			{ PBC_R(I_NAME, 0, 65), { C_NAME }, { 0 }, { C_HORSE, C_GUILD, C_LIKE }, { C_YOU } },
			{ PBC_R(I_LEVEL, 0, 62), { C_LEVEL }, { 0 }, { C_SKILL, C_HORSE, C_GUILD }, { C_WHICH, C_HOWMUCH, C_HAVE } },
			{ PBC_R(I_CLASS, 0, 60), { C_CLASS }, { 0 }, { C_LIKE, C_FAV }, { C_WHICH, C_YOU } },
			{ PBC_R(I_EMPIRE, 0, 62), { C_EMPIRE }, { 0 }, { 0 }, { C_WHICH } },
			{ PBC_R(I_PERSONALITY, 0, 62), { C_CHARACTER }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_MOOD, 0, 62), { C_MOOD }, { 0 }, { C_HUMOR }, { C_HOW, C_YOU } },
			// ---------------- state
			{ PBC_R(I_ACTIVITY, 0, 70), { C_DO }, { C_WHAT, C_NOW, C_YOU, C_HOWAREYOU },
				{ C_WHERE, C_PLAN, C_NEXT, C_WORK, C_WHY }, { C_NOW, C_THERE } },
			{ PBC_R(I_ACTIVITY, 0, 57), { C_WHAT, C_NOW }, { 0 },
				{ C_WHERE, C_HORSE, C_BIO, C_METIN, C_GUILD }, { 0 } },
			{ PBC_R(I_ACTIVITY, 0, 50), { C_EXP }, { 0 }, { C_WHERE, C_MAP, C_PLACE, C_WITHME, C_WITHWHO }, { C_NOW, C_YOU } },
			{ PBC_R(I_ACTIVITY, 0, 72), { C_TRAVEL, C_EXP }, { 0 }, { C_WHERE, C_WITHME }, { 0 } },
			{ PBC_R(I_ACTIVITY_LOCATION, 0, 78), { C_WHERE }, { C_EXP, C_HIT, C_MOB, C_METIN },
				{ C_TRAVELG }, { C_NOW } },
			{ PBC_R(I_ACTIVITY_LOCATION, 0, 66), { C_EXP }, { C_MAP, C_PLACE, C_WHICH }, { C_LIKE }, { 0 } },
			{ PBC_R(I_LOCATION, 0, 68), { C_WHERE }, { C_BE, C_NOW, C_YOU, C_THERE },
				{ C_TRAVELG, C_WANT, C_NEXT, C_PLAN, C_TRAVEL }, { C_NOW } },
			{ PBC_R(I_LOCATION, 0, 64), { C_MAP }, { C_WHICH, C_WHERE, C_WHAT, C_BE },
				{ C_THINK, C_TRAVELG }, { 0 } },
			{ PBC_R(I_MOB_COUNT, 0, 72), { C_MANY, C_MOB }, { 0 }, { 0 }, { C_THERE } },
			{ PBC_R(I_MOB_COUNT, 0, 72), { C_HOWMUCH, C_MOB }, { 0 }, { 0 }, { C_THERE } },
			{ PBC_R(I_MOB_COUNT, 0, 58), { C_MOB }, { C_BE, C_THERE, C_HAVE }, { C_HIT }, { 0 } },
			{ PBC_R(I_TARGET, 0, 72), { C_WHAT, C_HIT }, { 0 }, { C_WHERE }, { C_NOW } },
			{ PBC_R(I_TARGET, 0, 70), { C_WHO, C_HIT }, { 0 }, { C_WHERE }, { C_NOW } },
			{ PBC_R(I_TARGET, 0, 62), { C_TARGET }, { 0 }, { C_WHERE }, { C_WHAT, C_WHICH } },
			{ PBC_R(I_TARGET, 0, 52), { C_HIT }, { C_NOW, C_YOU }, { C_WHERE, C_PVP, C_WITHME }, { 0 } },
			{ PBC_R(I_GOAL, 0, 66), { C_GOAL }, { C_WHICH, C_WHAT, C_HAVE, C_YOU }, { C_TARGET, C_HIT }, { 0 } },
			{ PBC_R(I_GOAL, 0, 60), { C_ACHIEVE }, { C_WANT, C_WHAT }, { 0 }, { 0 } },
			{ PBC_R(I_NEXT_PLAN, 0, 68), { C_PLAN }, { 0 }, { C_HYPO }, { C_NEXT, C_WHAT } },
			{ PBC_R(I_NEXT_PLAN, 0, 64), { C_NEXT }, { C_WHAT, C_DO, C_WHERE }, { C_HYPO, C_THANKS }, { C_DO } },
			{ PBC_R(I_HP, 0, 66), { C_HP }, { 0 }, { C_HERB }, { C_HOWMUCH, C_HAVE, C_WHATWITH } },
			{ PBC_R(I_HP, 0, 64), { C_LIFE }, { C_HOWMUCH, C_HAVE, C_WHATWITH }, { C_LIFEG }, { 0 } },
			{ PBC_R(I_GOLD, 0, 70), { C_GOLD }, { C_HAVE, C_HOWMUCH, C_MANY, C_YOU }, { C_TRADE }, { 0 } },
			{ PBC_R(I_GOLD, 0, 60), { C_MONEY }, { C_HAVE, C_HOWMUCH }, { C_LIFEG, C_HAPPY }, { 0 } },
			{ PBC_R(I_GOLD, 0, 52), { C_GOLD }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_HORSE, 0, 66), { C_HORSE }, { 0 }, { C_LIKE, C_ANIMAL }, { C_WHATWITH, C_HAVE } },
			{ PBC_R(I_INVENTORY_SPACE, 0, 74), { C_FREE }, { C_EQ, C_PLACE, C_HAVE }, { 0 }, { 0 } },
			{ PBC_R(I_INVENTORY_SPACE, 0, 72), { C_PLACE, C_EQ }, { 0 }, { 0 }, { C_HOWMUCH } },
			{ PBC_R(I_INVENTORY, 0, 64), { C_EQ }, { C_WHAT, C_HAVE }, { C_FREE, C_PLACE }, { 0 } },
			{ PBC_R(I_INVENTORY, 0, 52), { C_EQ }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_EQUIPMENT, 0, 60), { C_GEAR }, { 0 }, { C_BUYME, C_SELLYOU, C_TRADE, C_SHOP, C_UPGRADE }, { C_WHICH, C_WHAT, C_HAVE } },
			{ PBC_R(I_PARTY, 0, 62), { C_PARTY }, { 0 }, { C_JOIN, C_WANT, C_CAN, C_WITHME }, { C_BE, C_HAVE } },
			{ PBC_R(I_PARTY, 0, 70), { C_WITHWHO }, { C_BE, C_EXP, C_PLAY, C_YOU, C_HIT }, { 0 }, { 0 } },
			{ PBC_R(I_PARTY, 0, 66), { C_ALONE }, { C_BE, C_EXP, C_PLAY, C_YOU, C_HIT }, { C_LONELY }, { 0 } },
			{ PBC_R(I_PARTY_REQUEST, 0, 66), { C_JOIN }, { 0 }, { C_HYPO }, { C_WITHME, C_PARTY, C_EXP } },
			{ PBC_R(I_PARTY_REQUEST, 0, 76), { C_CAN, C_WITHYOU }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_PARTY_REQUEST, 0, 74), { C_WITHME }, { C_JOIN, C_EXP, C_WANT, C_CAN, C_HIT }, { C_PVP }, { 0 } },
			{ PBC_R(I_PARTY_REQUEST, 0, 76), { C_PARTY }, { C_JOIN, C_WANT, C_CAN }, { 0 }, { 0 } },
			{ PBC_R(I_PARTY_REQUEST, 0, 66), { C_WITHYOU }, { C_EXP, C_WANT, C_JOIN, C_PLAY }, { 0 }, { 0 } },
			{ PBC_R(I_GUILD, 0, 66), { C_GUILD }, { 0 }, { C_WAR }, { C_HAVE, C_WHICH, C_WHATWITH } },
			{ PBC_R(I_FISHING, 0, 64), { C_FISH }, { 0 }, { C_FOOD }, { C_NOW, C_WHAT } },
			{ PBC_R(I_MINING, 0, 64), { C_MINE }, { 0 }, { 0 }, { C_NOW, C_WHAT } },
			{ PBC_R(I_HERBALISM, 0, 62), { C_HERB }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_BIOLOGIST, 0, 68), { C_BIO }, { 0 }, { 0 }, { C_WHATWITH } },
			{ PBC_R(I_METIN, 0, 66), { C_METIN }, { 0 }, { C_WHERE }, { C_WHATWITH, C_HIT } },
			{ PBC_R(I_DEMON_TOWER, 0, 64), { C_DT }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GUILD_WAR, 0, 66), { C_WAR }, { 0 }, { 0 }, { C_GUILD } },
			{ PBC_R(I_MERCENARY, 0, 64), { C_MERC }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_SHOP, 0, 64), { C_SHOP }, { 0 }, { C_BUYME, C_SELLYOU }, { C_WHAT, C_HAVE } },
			{ PBC_R(I_SHOP, 0, 66), { C_WHAT, C_TRADE }, { 0 }, { C_BUYME, C_SELLYOU, C_THINK }, { 0 } },
			{ PBC_R(I_MARKET, 0, 54), { C_TRADE }, { 0 }, { C_BUYME, C_SELLYOU }, { 0 } },
			{ PBC_R(I_BUY, 0, 84), { C_BUYME }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_SELL, 0, 84), { C_SELLYOU }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_SKILLS, 0, 62), { C_SKILL }, { 0 }, { 0 }, { C_WHICH, C_HAVE } },
			{ PBC_R(I_PVP, 0, 66), { C_PVP }, { 0 }, { 0 }, { C_WITHME } },
			{ PBC_R(I_TRAVEL, 0, 76), { C_WHERE, C_TRAVEL }, { 0 }, { C_HYPO, C_TRAVELG }, { 0 } },
			{ PBC_R(I_TRAVEL, 0, 60), { C_TRAVEL }, { 0 }, { C_HYPO, C_TRAVELG, C_WITHME }, { C_NOW } },
			{ PBC_R(I_REST, 0, 60), { C_REST }, { 0 }, { 0 }, { C_NOW } },
			{ PBC_R(I_REFINE, 0, 60), { C_UPGRADE }, { 0 }, { 0 }, { C_GEAR } },
			{ PBC_R(I_MISSIONS, 0, 60), { C_QUEST }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_DEATH, 0, 62), { C_KILLED }, { C_YOU, C_HOWMUCH, C_TODAY }, { 0 }, { 0 } },
			{ PBC_R(I_DEATH, 0, 50), { C_KILLED }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_RELATIONSHIP, 0, 82), { C_LIKE, C_ME }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_RELATIONSHIP, 0, 80), { C_THINK, C_ME }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_RELATIONSHIP, 0, 72), { C_FRIEND }, { C_WE, C_ME }, { 0 }, { 0 } },
			{ PBC_R(I_TIME_HERE, 0, 68), { C_LONG }, { C_BE, C_THERE, C_EXP, C_PLAY, C_HIT }, { 0 }, { 0 } },
			{ PBC_R(I_MAP_OPINION, 0, 78), { C_LIKE }, { C_MAP, C_THERE, C_EXP }, { 0 }, { 0 } },
			{ PBC_R(I_MAP_OPINION, 0, 76), { C_THINK, C_MAP }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_DROP_LUCK, 0, 66), { C_DROP }, { 0 }, { C_TRADE }, { C_HOW, C_WHATWITH, C_TODAY } },
			{ PBC_R(I_DROP_LUCK, 0, 60), { C_LUCK }, { C_TODAY, C_HAVE, C_YOU }, { C_HYPO }, { 0 } },
			{ PBC_R(I_PROGRESS_TODAY, 0, 68), { C_DID }, { C_TODAY, C_MANY }, { 0 }, { 0 } },

			// ---------------- general conversation
			{ PBC_R(I_GENERAL, T_WEATHER, 58), { C_WEATHER }, { 0 }, { 0 }, { C_TODAY } },
			{ PBC_R(I_GENERAL, T_SEASON, 58), { C_SEASON }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_DAYTIME, 52), { C_DAYTIME }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_SLEEP, 56), { C_SLEEP }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_TIRED, 60), { C_TIRED }, { 0 }, { 0 }, { C_YOU, C_BE } },
			{ PBC_R(I_GENERAL, T_BORED, 60), { C_BORED }, { 0 }, { 0 }, { C_YOU } },
			{ PBC_R(I_GENERAL, T_HOBBY, 66), { C_HOBBY }, { 0 }, { 0 }, { C_HAVE } },
			{ PBC_R(I_GENERAL, T_HOBBY, 70), { C_LIKE, C_DO }, { 0 }, { 0 }, { C_WHAT } },
			{ PBC_R(I_GENERAL, T_FOOD, 58), { C_FOOD }, { 0 }, { 0 }, { C_LIKE, C_FAV } },
			{ PBC_R(I_GENERAL, T_DRINK, 56), { C_DRINK }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_TRAVEL, 60), { C_TRAVELG }, { 0 }, { 0 }, { C_HYPO, C_WHERE, C_WANT } },
			{ PBC_R(I_GENERAL, T_MUSIC, 60), { C_MUSIC }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_MOVIES, 60), { C_MOVIE }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_GAMES, 58), { C_GAMES }, { 0 }, { 0 }, { C_LIKE, C_PLAY } },
			{ PBC_R(I_GENERAL, T_HUMOR, 60), { C_HUMOR }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_LUCK, 52), { C_LUCK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_FRIENDSHIP, 58), { C_FRIEND }, { 0 }, { 0 }, { C_HAVE } },
			{ PBC_R(I_GENERAL, T_TEAMWORK, 56), { C_TEAMWORK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_LONELY, 62), { C_LONELY }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_RISK, 58), { C_RISK }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_MONEY, 54), { C_MONEY }, { 0 }, { 0 }, { C_HAPPY, C_LIFEG } },
			{ PBC_R(I_GENERAL, T_WORK, 58), { C_WORK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_SCHOOL, 58), { C_SCHOOL }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_LIFE, 58), { C_LIFEG }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_LIFE, 50), { C_LIFE }, { 0 }, { 0 }, { C_LIKE, C_THINK } },
			{ PBC_R(I_GENERAL, T_DREAMS, 72), { C_DREAM }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_FEAR, 70), { C_FEAR }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_ANNOY, 70), { C_ANNOY }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_JOY, 68), { C_JOY }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_FEELINGS, 58), { C_SAD }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_FEELINGS, 56), { C_HAPPY }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_ANIMALS, 58), { C_ANIMAL }, { 0 }, { 0 }, { C_LIKE, C_HAVE } },
			{ PBC_R(I_GENERAL, T_SPORT, 58), { C_SPORT }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_LOVE, 58), { C_LOVE }, { 0 }, { 0 }, { C_HAVE } },
			{ PBC_R(I_GENERAL, T_BOOKS, 58), { C_BOOKS }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_NATURE, 54), { C_NATURE }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_ADVENTURE, 58), { C_ADVENTURE }, { 0 }, { 0 }, { C_LIKE } },
			{ PBC_R(I_GENERAL, T_COLOR, 60), { C_COLOR }, { 0 }, { 0 }, { C_FAV } },
			// preference / hypothetical words with no topic of their own
			{ PBC_R(I_GENERAL, T_NONE, 62), { C_HYPO }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 52), { C_LIKE }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 56), { C_DISLIKE }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 56), { C_FAV }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 54), { C_PREFER }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 46), { C_THINK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 44), { C_CANYOU }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_GENERAL, T_NONE, 40), { C_KNOW }, { 0 }, { 0 }, { 0 } },

			// ---------------- the shorthand of the game
			{ PBC_R(I_KS, 0, 76), { C_KS }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_READY, 0, 64), { C_READY }, { 0 }, { C_HYPO }, { C_YOU } },
			{ PBC_R(I_GOODLUCK, 0, 62), { C_GOODLUCK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_BRB, 0, 64), { C_BRB }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_ITEMSHOP, 0, 70), { C_ITEMSHOP }, { 0 }, { 0 }, { C_HAVE, C_HOWMUCH } },
			{ PBC_R(I_PRICE, 0, 78), { C_PRICEQ }, { 0 }, { C_BUYME, C_SELLYOU }, { C_ITEMWORD } },
			{ PBC_R(I_PRICE, 0, 72), { C_HOWMUCH, C_ITEMWORD }, { 0 }, { C_HAVE }, { 0 } },
			{ PBC_R(I_ITEM_OWN, 0, 70), { C_HAVE, C_ITEMWORD }, { 0 }, { C_SHOP, C_PRICEQ, C_WHICH }, { 0 } },
			{ PBC_R(I_ITEM_OWN, 0, 68), { C_HAVE, C_HERB }, { 0 }, { C_SHOP, C_PRICEQ }, { 0 } },
			{ PBC_R(I_SHOP, 0, 74), { C_SHOP, C_ITEMWORD }, { 0 }, { C_BUYME, C_SELLYOU }, { 0 } },
			{ PBC_R(I_LOCATION, 0, 74), { C_MAPNAME }, { C_BE, C_EXP, C_HIT, C_YOU, C_NOW },
				{ C_TRAVEL, C_LIKE, C_THINK, C_HYPO, C_TRAVELG }, { 0 } },
			{ PBC_R(I_TRAVEL, 0, 78), { C_MAPNAME, C_TRAVEL }, { 0 }, { C_HYPO }, { 0 } },
			{ PBC_R(I_MAP_OPINION, 0, 80), { C_MAPNAME, C_LIKE }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_EQUIPMENT, 0, 62), { C_BONUS }, { 0 }, { C_BUYME, C_SELLYOU }, { C_HAVE, C_WHICH } },
			{ PBC_R(I_MOB_COUNT, 0, 62), { C_MOB, C_HOW }, { 0 }, { C_HIT, C_LIKE }, { 0 } },
			// "masz tarcze?" - is it in the bag (the engine looks), not "what do you wear"
			{ PBC_R(I_ITEM_OWN, 0, 68), { C_HAVE, C_GEAR }, { 0 }, { C_WHICH, C_WHAT, C_FREE }, { 0 } },

			// ---------------- the class's path and a Shaman's buffs
			// "jaka masz profesje?", "jestes body czy mental?", "grasz archerem?"
			{ PBC_R(I_BUILD, 0, 72), { C_BUILD }, { 0 }, { C_GIVE, C_BUFF, C_BUFFNAME, C_BUYME, C_SELLYOU },
				{ C_WHICH, C_YOU, C_PLAY } },
			// "jakie masz buffy?", "co daja twoje buffy?", "zbuffujesz mnie?"
			{ PBC_R(I_BUFFS, 0, 74), { C_BUFF }, { 0 }, { C_BUYME, C_SELLYOU, C_PRICEQ }, { C_WHAT, C_GIVE, C_HAVE } },
			// "ile daje blogoslawienstwo?", "co daje odbicie", "ile leczysz?"
			{ PBC_R(I_BUFFS, 0, 76), { C_BUFFNAME }, { 0 }, { C_BUYME, C_SELLYOU, C_PRICEQ }, { C_GIVE, C_HOWMUCH, C_WHAT } },
			// "co daje smok?" - what the path's buffs give
			{ PBC_R(I_BUFFS, 0, 74), { C_GIVE }, { C_BUILD }, { C_BUYME, C_SELLYOU, C_PRICEQ }, { C_WHAT, C_HOWMUCH } },

			// ---------------- come over, and go back to your own life
			// "chodz do mnie do pt" is an invitation, and the party rule has it.
			{ PBC_R(I_SUMMON, 0, 80), { C_SUMMON }, { 0 }, { C_HYPO, C_PARTY, C_DISMISS }, { C_ME, C_THERE } },
			{ PBC_R(I_DISMISS, 0, 82), { C_DISMISS }, { 0 }, { C_HYPO }, { 0 } },

			// ---------------- a person talking at the bot
			// These outrank every topic the words inside them name: "przestan do
			// mnie pisac gold diggerze" is not a question about yang, "bana ci
			// daje" not one about buffs, "zawijaj stad" not a farewell.
			{ PBC_R(I_STOP_TALKING, 0, 95), { C_STOPTALK }, { 0 }, { 0 }, { 0 } },
			{ PBC_R(I_THREAT, 0, 90), { C_THREAT }, { 0 }, { 0 }, { C_YOU } },
			{ PBC_R(I_MOCK, 0, 80), { C_MOCK }, { 0 }, { 0 }, { C_YOU } },
			// Swearing aimed at the bot. Under the activity rule's 70 on purpose:
			// "kurwa, co ty robisz" is still a question.
			{ PBC_R(I_INSULT, 0, 66), { C_SWEAR, C_YOU }, { 0 }, { C_WHAT, C_WHERE, C_HOW, C_WHY }, { 0 } },
			// "przeciez mowiles, ze w Joan"; "co powiedziales?" asks to hear it again.
			{ PBC_R(I_CONTRADICTION, 0, 84), { C_SAIDBEFORE }, { 0 }, { C_WHAT }, { 0 } },

			// ---------------- the bot's gear argued about
			// "zoba jaki fms 9" - an item shown, not a question about the bot's.
			{ PBC_R(I_SHOW_ITEM, 0, 86), { C_SHOWOFF }, { C_ITEMWORD, C_GEAR },
				{ C_PRICEQ, C_BUYME, C_SELLYOU, C_SHOP }, { 0 } },
			// "a jakbym ci dal riba +9 ze srednimi" - a gift, even a hypothetical one.
			{ PBC_R(I_GIFT_OFFER, 0, 90), { C_GIFT }, { C_GEAR, C_ITEMWORD, C_BONUS, C_SWAP },
				{ C_PRICEQ, C_SELLYOU, C_BUYME, C_WHO, C_WHERE }, { 0 } },
			// "co myslisz o broni ze srednimi?", "jaka bron jest najlepsza?"
			{ PBC_R(I_GEAR_OPINION, 0, 82), { C_THINK }, { C_GEAR, C_ITEMWORD, C_BONUS }, { C_MAP, C_MAPNAME }, { 0 } },
			{ PBC_R(I_GEAR_OPINION, 0, 82), { C_ADVICE, C_GEAR }, { C_WHICH, C_WHAT, C_OR },
				{ C_WHY, C_MAP, C_MAPNAME, C_HAVE }, { 0 } },
			{ PBC_R(I_GEAR_OPINION, 0, 82), { C_ADVICE, C_ITEMWORD }, { C_WHICH, C_WHAT, C_OR },
				{ C_WHY, C_MAP, C_MAPNAME, C_HAVE }, { 0 } },
			// "czemu nie wymienisz broni?", "czemu nie kupisz sobie riba?",
			// "czemu, przeciez ta bron ma srednie" - why the bot holds what it holds.
			{ PBC_R(I_GEAR_WHY, 0, 82), { C_WHY, C_GEAR }, { 0 }, { C_BUYME, C_SELLYOU, C_TRADE, C_SHOP, C_PRICEQ },
				{ C_SWAP, C_ADVICE, C_BONUS } },
			{ PBC_R(I_GEAR_WHY, 0, 82), { C_WHY, C_ITEMWORD }, { 0 }, { C_BUYME, C_SELLYOU, C_TRADE, C_SHOP, C_PRICEQ },
				{ C_SWAP, C_ADVICE, C_BONUS } },
			{ PBC_R(I_GEAR_WHY, 0, 80), { C_WHY, C_BONUS }, { 0 }, { C_BUYME, C_SELLYOU, C_TRADE, C_SHOP }, { C_SWAP, C_ADVICE } },
			{ PBC_R(I_GEAR_WHY, 0, 78), { C_WHY, C_SWAP }, { 0 }, { C_MAP, C_MAPNAME, C_GUILD, C_PARTY, C_SKILL }, { C_ADVICE } },
			// "zmien bron", "potrzebne ci sa obrazenia", "kup sobie riba"
			{ PBC_R(I_GEAR_ADVICE, 0, 80), { C_SWAP }, { C_GEAR, C_ITEMWORD, C_BONUS },
				{ C_BUYME, C_SELLYOU, C_PRICEQ, C_MAP, C_MAPNAME }, { C_ADVICE } },
			{ PBC_R(I_GEAR_ADVICE, 0, 78), { C_ADVICE }, { C_GEAR, C_ITEMWORD, C_BONUS },
				{ C_BUYME, C_SELLYOU, C_PRICEQ, C_MAP, C_HAVE }, { 0 } },
			// "to nie lepiej na jakas wyzsza mape isc?", "czemu zmieniles mape?"
			{ PBC_R(I_MAP_ADVICE, 0, 80), { C_ADVICE }, { C_MAP, C_MAPNAME }, { C_BUYME, C_SELLYOU, C_GEAR, C_ITEMWORD }, { 0 } },
			{ PBC_R(I_MAP_ADVICE, 0, 80), { C_WHY, C_MAP }, { 0 }, { C_TRAVELG, C_LIKE }, { C_SWAP, C_ADVICE } },
			{ PBC_R(I_MAP_ADVICE, 0, 76), { C_SWAP, C_MAP }, { 0 }, { C_GEAR, C_ITEMWORD, C_TRAVELG }, { 0 } },
			// weak catch-alls
			{ PBC_R(I_ITEM_OWN, 0, 42), { C_HAVE }, { 0 }, { C_WHICH, C_DREAM, C_HOBBY, C_WHAT }, { 0 } },
		};
		count = sizeof(kRules) / sizeof(kRules[0]);
		return kRules;
	}

#undef PBC_R

	// The arguments about the bot's gear are opinions and hypotheticals by
	// nature ("wymienilbys?", "jakbym ci dal..."), so a preference word is
	// what they are made of, not a sign of small talk.
	inline bool IsPreferenceRule(const TIntentRule& r)
	{
		return r.intent == I_GENERAL || r.intent == I_RELATIONSHIP || r.intent == I_MAP_OPINION ||
				r.intent == I_GIFT_OFFER || r.intent == I_GEAR_WHY || r.intent == I_GEAR_ADVICE ||
				r.intent == I_GEAR_OPINION || r.intent == I_MAP_ADVICE || r.intent == I_SHOW_ITEM;
	}

	inline int ScoreRule(const TIntentRule& r, const TConceptSet& c)
	{
		for (int i = 0; i < 3; ++i)
			if (r.req[i] && !c.Has(r.req[i]))
				return 0;
		bool any = r.oneof[0] == 0;
		for (int i = 0; i < 5 && r.oneof[i]; ++i)
			if (c.Has(r.oneof[i]))
				any = true;
		if (!any)
			return 0;
		for (int i = 0; i < 5; ++i)
			if (r.forbid[i] && c.Has(r.forbid[i]))
				return 0;
		int score = r.score;
		for (int i = 0; i < 3; ++i)
			if (r.bonus[i] && c.Has(r.bonus[i]))
				score += 4;
		if (!IsPreferenceRule(r) && IsGameIntent((EIntent)r.intent) &&
				(c.Has(C_LIKE) || c.Has(C_HYPO) || c.Has(C_DREAM) || c.Has(C_FAV) ||
				 c.Has(C_PREFER) || c.Has(C_DISLIKE) || c.Has(C_FEAR)))
			score -= 25;
		return score;
	}

	// ------------------------------------------------------------ extraction

	inline bool IsFillerWord(const std::string& w)
	{
		static const char* const kFill[] = {
			"ta", "ten", "te", "to", "tych", "sie", "cos", "jakies", "jakis", "jakas", "moze", "ze",
			"a", "no", "ty", "tez", "bardzo", "w", "na", "o", "czy", "jak", "mnie", "cie", "ciebie",
			"bys", "by", "ci", "mi", "tak", "naprawde", "serio", "kiedys", "juz", "dzisiaj", "teraz",
			"ogolnie", "wogole", "mocno", "sobie", "stary", "ziomek"
		};
		for (size_t i = 0; i < sizeof(kFill) / sizeof(kFill[0]); ++i)
			if (w == kFill[i])
				return true;
		return false;
	}

	// The words after the trigger word, up to three, without fillers at the
	// ends. "czy lubisz bardzo zime ?" -> "zime".
	inline std::string ExtractObjectAfter(const TTokens& tok, int triggerWord, size_t maxWords = 3)
	{
		if (triggerWord < 0)
			return std::string();
		std::vector<std::string> picked;
		for (size_t i = (size_t)triggerWord + 1; i < tok.words.size() && picked.size() < maxWords; ++i)
		{
			const std::string& w = tok.words[i];
			if (w == "czy" || w == "albo" || w == "lub" || w == "bo" || w == "i")
				break;
			if (picked.empty() && IsFillerWord(w))
				continue;
			picked.push_back(w);
		}
		while (!picked.empty() && IsFillerWord(picked.back()))
			picked.pop_back();
		std::string out;
		for (size_t i = 0; i < picked.size(); ++i)
		{
			if (i)
				out += ' ';
			out += picked[i];
		}
		if (out.size() > 28)
			out.clear();
		return out;
	}

	// "sprzedasz mi miecz pelni", "chce kupic od ciebie ku aura", "kupie kosc":
	// the item after the trade words, whatever they were.
	inline std::string ExtractTradeObject(const TTokens& tok, int from)
	{
		if (from < 0)
			return std::string();
		static const char* const kSkip[] = {
			"sprzedasz", "odsprzedasz", "sprzedam", "sprzedac", "kupie", "kupic", "kupisz", "szukam", "chce",
			"chcialbym", "oddam", "mi", "ci", "od", "ode", "ciebie", "mnie", "na", "sprzedaz", "masz", "moze", "jakis",
			"jakas", "jakies", "cos", "tobie", "za", "ile", "po", "kosztuje", "cena", "cene", "ceny", "chcesz",
			"stragan", "straganie", "straganu", "straganem", "wystawiony", "wystawione", "wystawiles", "wystawila",
			"sklep", "sklepie", "sklepu", "z", "ze", "sprzedaje", "sprzedajesz", "czy", "jest", "twoj", "twoim",
			"twoja", "twojej", "twoich", "tym", "posiadasz", "co", "jakie", "jaki", "jaka", "cokolwiek",
			"ciekawego", "w", "eq", "ekwipunku", "plecaku", "a", "no", "hej", "stoi", "chodzi", "warte", "wart",
			"teraz", "jeszcze", "tam", "tu", "u", "dla", "mnie", "wycenisz", "wycen", "ty", "ziomek", "prosze",
			"zobacz", "sprawdz", "moglbys", "mozesz", "sprzedalbys", "wiesz", "chodza", "sa", "jakiegos", "jakas",
			"gdzie"
		};
		size_t i = (size_t)from;
		for (; i < tok.words.size(); ++i)
		{
			bool skip = false;
			for (size_t k = 0; k < sizeof(kSkip) / sizeof(kSkip[0]); ++k)
				if (tok.words[i] == kSkip[k])
					skip = true;
			if (!skip)
				break;
		}
		std::string out;
		for (size_t n = 0; i < tok.words.size() && n < 4; ++i, ++n)
		{
			const std::string& w = tok.words[i];
			// "fms za 2kk": the price is not part of the name.
			if (w == "za" || w == "po" || w == "czy" || w == "bo" || w == "i")
				break;
			std::vector<std::string> one(1, w);
			if (ParseYangAmount(one) > 0)
				break;
			if (w == "teraz" || w == "jeszcze" || w == "moze" || w == "tam" || w == "prosze")
				continue;
			if (!out.empty())
				out += ' ';
			out += w;
		}
		return out.size() > 40 ? std::string() : out;
	}

	inline int FindWordIndexOfConcept(const TConceptSet& c, int conceptId)
	{
		return c.Has(conceptId) ? c.firstWord[conceptId] : -1;
	}

	inline EQType DetectQType(const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		const bool q = a.question || c.Has(C_WHAT) || c.Has(C_WHERE) || c.Has(C_HOW) || c.Has(C_WHY) ||
				c.Has(C_WHO) || c.Has(C_WHICH) || c.Has(C_HOWMUCH) || c.Has(C_WHEN) || a.tokens.Has("czy");
		if (c.Has(C_HYPO)) return Q_HYPO;
		if (c.Has(C_DREAM)) return Q_DREAM;
		if (c.Has(C_FEAR) && q) return Q_FEAR;
		if (c.Has(C_ANNOY) && q) return Q_ANNOY;
		if (c.Has(C_JOY) && q) return Q_JOY;
		if (c.Has(C_PREFER)) return Q_CHOICE;
		if (c.Has(C_DISLIKE)) return Q_DISLIKE;
		if (c.Has(C_FAV)) return Q_FAVORITE;
		if (c.Has(C_LIKE) && (c.Has(C_WHAT) || c.Has(C_DO))) return Q_WHAT_LIKE;
		if (c.Has(C_LIKE)) return Q_LIKE;
		if (c.Has(C_THINK)) return Q_OPINION;
		if (c.Has(C_CANYOU)) return Q_CAN;
		if (c.Has(C_EVER) && (c.Has(C_BE) || a.tokens.Has("byles") || a.tokens.Has("bylas"))) return Q_EVER;
		if (c.Has(C_WANT) && q) return Q_WANT;
		if (c.Has(C_EVER) && q) return Q_EVER;
		if (c.Has(C_KNOW) && q) return Q_KNOW;
		if ((c.Has(C_WHO) || c.Has(C_HOWMUCH) || c.Has(C_WHEN)) && !c.Has(C_YOU))
			return Q_FACT;
		if (q) return Q_OPEN;
		return Q_STATEMENT;
	}

	inline void ExtractObject(TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		int trig = -1;
		const int order[] = { C_DISLIKE, C_PREFER, C_LIKE, C_FAV, C_THINK, C_CANYOU, C_KNOW, C_WANT, C_EVER };
		for (size_t i = 0; i < sizeof(order) / sizeof(order[0]) && trig < 0; ++i)
			trig = FindWordIndexOfConcept(c, order[i]);
		if (c.Has(C_DISLIKE) && trig >= 0)
			++trig; // "nie lubisz X": skip "lubisz" as well
		if (trig < 0)
			return;
		if (c.Has(C_THINK))
		{
			// "co myslisz o kotach" / "co sadzisz o tej mapie"
			const int o = a.tokens.Find("o");
			if (o > trig)
				trig = o;
		}
		a.object = ExtractObjectAfter(a.tokens, trig);
		if (a.qtype == Q_CHOICE || a.tokens.Has("czy") || a.tokens.Has("albo"))
		{
			int sep = a.tokens.Find("czy");
			if (sep <= trig)
				sep = a.tokens.Find("albo");
			if (sep <= trig)
				sep = a.tokens.Find("lub");
			if (sep > trig)
			{
				a.objectB = ExtractObjectAfter(a.tokens, sep);
				if (!a.objectB.empty())
					a.qtype = Q_CHOICE;
			}
		}
	}

	// ------------------------------------------------------------- follow-up

	// Words that make a line "about something" on its own. A line with none of
	// these and a follow-up word is a follow-up.
	inline bool HasContentConcept(const TConceptSet& c)
	{
		for (int i = C_DO; i < C_COUNT; ++i)
		{
			// A give word is about something only beside what gives it: "a ile
			// daje?" after the buffs is a follow-up about the buffs. A curse is
			// how some people talk: "kurde, gdzie?" is still a follow-up.
			if (i == C_MANY || i == C_ALONE || i == C_ACK || i == C_LAUGH || i == C_SURPRISE ||
					i == C_YES || i == C_NO || i == C_BUDDY || i == C_OR || i == C_NEXT ||
					i == C_TODAY || i == C_NOW || i == C_POSITIVE || i == C_NEGATIVE || i == C_GIVE ||
					i == C_SWEAR)
				continue;
			if (c.Has(i))
				return true;
		}
		return false;
	}

	inline EFollow DetectFollow(const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		const size_t n = a.tokens.words.size();
		if (n == 0 || n > 5)
			return F_NONE;
		if (HasContentConcept(c))
			return F_NONE;
		if (c.Has(C_WHY)) return F_WHY;
		if (c.Has(C_WITHWHO)) return F_WITHWHO;
		if (c.Has(C_ALONE)) return F_ALONE;
		if (c.Has(C_WHERE)) return F_WHERE;
		if (c.Has(C_MANY) || c.Has(C_HOWMUCH)) return F_COUNT;
		if (c.Has(C_NEXT)) return F_NEXT;
		if (c.Has(C_WHEN)) return F_WHEN;
		if (c.Has(C_SURPRISE) && n <= 3) return F_CONFIRM;
		if (c.Has(C_HOW)) return F_HOW;
		if (c.Has(C_YOU) && n <= 3) return F_MIRROR;
		if (c.Has(C_WHO) && n <= 2) return F_WHO;
		if (c.Has(C_AND) && n <= 3) return F_NEXT;
		if (c.Has(C_WHAT) && n <= 2 && !c.Has(C_HOWAREYOU)) return F_WHAT;
		if ((c.Has(C_THIS) || c.Has(C_THERE)) && n <= 3 && (a.tokens.Has("a") || a.question)) return F_THIS;
		return F_NONE;
	}

	// ------------------------------------------------------- gear in a line

	// "+9" -> 9; with `bare`, "9" as well ("fms 9"). A refine runs 1..15 on
	// this line of the game; anything else is some other number.
	inline int PlusOfWord(const std::string& w, bool bare)
	{
		size_t p = 0;
		if (!w.empty() && w[0] == '+')
			p = 1;
		else if (!bare)
			return -1;
		if (p >= w.size())
			return -1;
		int v = 0;
		for (size_t i = p; i < w.size(); ++i)
		{
			if (w[i] < '0' || w[i] > '9')
				return -1;
			v = v * 10 + (w[i] - '0');
			if (v > 15)
				return -1;
		}
		return v >= 1 ? v : -1;
	}

	// "bronia na 15 level", "64 lvl", "poziom 30": a level the line names.
	inline int FindLevelNamed(const TTokens& tok)
	{
		const std::vector<std::string>& w = tok.words;
		for (size_t i = 0; i < w.size(); ++i)
		{
			const std::string& n = w[i];
			if (n.empty() || n.size() > 3 || n[0] < '0' || n[0] > '9')
				continue;
			int v = 0;
			bool digits = true;
			for (size_t k = 0; k < n.size(); ++k)
			{
				if (n[k] < '0' || n[k] > '9')
					digits = false;
				else
					v = v * 10 + (n[k] - '0');
			}
			if (!digits || v < 1 || v > 150)
				continue;
			const std::string next = i + 1 < w.size() ? w[i + 1] : std::string();
			const std::string prev = i > 0 ? w[i - 1] : std::string();
			if (next == "lvl" || StartsWith(next, "level") || StartsWith(next, "poziom") ||
					prev == "lvl" || StartsWith(prev, "level") || StartsWith(prev, "poziom"))
				return v;
		}
		return 0;
	}

	// The item a line about gear names, as the players name it: "rib", "fms
	// 9", "riba +9". What a reply says back is the players' word with the
	// plus ("FMS +9"); a book, a potion or a stone is not gear.
	inline void ExtractGearObject(TAnalysis& a)
	{
		if (a.itemLink)
			return;
		const std::vector<std::string>& w = a.tokens.words;
		for (size_t i = 0; i < w.size(); ++i)
		{
			const TItemAlias* al = FindItemAlias(w[i]);
			if (!al || !IsGearAlias(al))
				continue;
			int plus = i + 1 < w.size() ? PlusOfWord(w[i + 1], true) : -1;
			for (size_t k = 0; k < w.size() && plus < 0; ++k)
				plus = PlusOfWord(w[k], false);
			a.object = al->alias;
			a.objectPlus = plus;
			a.itemShown = ItemAliasDisplay(al);
			if (plus > 0)
				a.itemShown += " +" + ToString((long long)plus);
			return;
		}
	}

	// ------------------------------------------------------------- analysis

	// The line on its own: no memory yet.
	inline void AnalyzeLine(const char* raw, TAnalysis& a, u32 now)
	{
		a = TAnalysis();
		a.at = now;
		Normalize(raw, a.tokens);
		a.question = a.tokens.question;
		ExtractConcepts(a.tokens, a.concepts);
		const TConceptSet& c = a.concepts;
		a.offerYang = ParseYangAmount(a.tokens.words);
		{
			size_t at = 0;
			a.mentionMap = FindMapAlias(a.tokens.words, at);
		}
		a.levelNamed = FindLevelNamed(a.tokens);

		// A sum that is the whole line ("ile to 2+2") is answered as one;
		// read as words it was "ile" and nothing, and the bot said "Ale czego?"
		{
			TArithmetic math;
			if (ParseArithmetic(raw, math))
			{
				a.intent = a.rawIntent = I_MATH;
				a.score = 90;
				a.question = true;
				a.mathDivZero = math.divZero;
				a.mathTooBig = math.tooBig;
				a.mathMixed = math.mixed;
				if (!math.divZero && !math.tooBig)
					a.mathText = FormatNumberPl(math.value);
				return;
			}
		}

		// A shift-clicked item with nothing asked about it is somebody showing
		// it off. The old reader took the words of the name for a question
		// about the bot's own weapon and recited its gear back.
		std::string link;
		const bool hasLink = ExtractItemLink(raw, link);
		if (hasLink && !c.Has(C_BUYME) && !c.Has(C_SELLYOU) && !c.Has(C_PRICEQ) && !c.Has(C_SHOP) &&
				!c.Has(C_HAVE) && !c.Has(C_GIFT) && a.offerYang == 0)
		{
			a.intent = a.rawIntent = I_SHOW_ITEM;
			a.score = 88;
			a.itemLink = true;
			a.itemShown = link;
			a.object = FoldName(link.c_str());
			a.objectPlus = RefineOfName(link);
			a.greetingToo = c.Has(C_GREET);
			return;
		}

		size_t ruleCount = 0;
		const TIntentRule* rules = GetIntentRules(ruleCount);
		int best = 0;
		const TIntentRule* bestRule = NULL;
		int bestNonSocial = 0;
		const TIntentRule* bestNonSocialRule = NULL;
		for (size_t i = 0; i < ruleCount; ++i)
		{
			const int s = ScoreRule(rules[i], c);
			if (s <= 0)
				continue;
			if (s > best)
			{
				best = s;
				bestRule = &rules[i];
			}
			const EIntent ri = (EIntent)rules[i].intent;
			if (ri != I_GREETING && ri != I_THANKS && ri != I_FAREWELL && ri != I_APOLOGY &&
					s > bestNonSocial)
			{
				bestNonSocial = s;
				bestNonSocialRule = &rules[i];
			}
		}

		// "hej, co robisz?" answers the question and greets; "dzieki, a gdzie
		// teraz?" thanks and answers. The question wins when it is a real one.
		if (bestRule && bestNonSocialRule && bestNonSocialRule != bestRule && bestNonSocial >= 44)
		{
			const EIntent social = (EIntent)bestRule->intent;
			if (social == I_GREETING)
				a.greetingToo = true;
			else if (social == I_THANKS)
				a.thanksToo = true;
			if (social == I_GREETING || social == I_THANKS)
			{
				bestRule = bestNonSocialRule;
				best = bestNonSocial;
			}
		}
		else if (bestRule && bestRule->intent != I_GREETING && c.Has(C_GREET))
			a.greetingToo = true;
		if (bestRule && bestRule->intent == I_HOW_ARE_YOU && c.Has(C_GREET))
			a.greetingToo = true;

		a.follow = DetectFollow(a);
		// A real rule of its own wins ("co teraz?", "gdzie jestes?"); a bare
		// "gdzie?" / "duzo ich?" has none and is a follow-up.
		if (a.follow != F_NONE && best < 55)
		{
			a.intent = a.rawIntent = I_FOLLOW_UP;
			a.score = 50;
			a.polarityNegative = a.tokens.Has("malo") || a.tokens.Has("pusto") || a.tokens.Has("pustki");
			return;
		}
		a.follow = F_NONE;

		// "zginalem na metinie", "wbilem 50 lvl" - the player telling about
		// himself; the answer is to that, not a report on the bot's own Metin.
		const bool firstPersonNews = !a.question && (c.Has(C_POSITIVE) || c.Has(C_NEGATIVE)) &&
				!c.Has(C_WHAT) && !c.Has(C_WHERE) && !c.Has(C_HOWMUCH) && !c.Has(C_WHY) && !c.Has(C_YOU) &&
				!c.Has(C_BUYME) && !c.Has(C_SELLYOU);
		// A line that talks at the bot or argues about its gear is that, even
		// with a "zle" or a "super" in it.
		const bool pointed = bestRule && (IsColdIntent((EIntent)bestRule->intent) ||
				IsArgumentIntent((EIntent)bestRule->intent));
		if (firstPersonNews && (!bestRule || bestRule->intent != I_GENERAL) && !pointed)
		{
			a.intent = a.rawIntent = I_UNKNOWN_STATEMENT;
			a.score = 45;
			return;
		}
		if (bestRule && best >= 40)
		{
			a.intent = a.rawIntent = (EIntent)bestRule->intent;
			a.topic = (ETopic)bestRule->topic;
			a.score = best;
		}
		else
		{
			// Reactions: a line that is only "ok", "xD", "tak", "nie".
			const size_t n = a.tokens.words.size();
			if (n <= 3 && c.Has(C_LAUGH)) a.intent = I_LAUGH;
			else if (n <= 3 && c.Has(C_YES)) a.intent = I_YES;
			else if (n <= 3 && c.Has(C_NO)) a.intent = I_NO;
			else if (n <= 3 && c.Has(C_ACK)) a.intent = I_ACK;
			else if (n == 0 && a.tokens.smile) a.intent = I_LAUGH;
			else if (n == 0 && a.question) { a.intent = I_FOLLOW_UP; a.follow = F_WHAT; }
			else if (a.question || c.Has(C_WHAT) || c.Has(C_WHO) || c.Has(C_HOWMUCH) || c.Has(C_WHY) ||
					c.Has(C_WHEN) || c.Has(C_WHICH) || c.Has(C_HOW) || a.tokens.Has("czy"))
				a.intent = I_UNKNOWN_QUESTION;
			else
				a.intent = I_UNKNOWN_STATEMENT;
			a.rawIntent = a.intent;
			a.score = 20;
		}

		if (a.intent == I_GENERAL || a.intent == I_UNKNOWN_QUESTION || a.intent == I_UNKNOWN_STATEMENT)
		{
			a.qtype = DetectQType(a);
			ExtractObject(a);
			// "co lubisz robic" with no topic of its own is hobbies.
			if (a.intent == I_GENERAL && a.topic == T_NONE && a.qtype == Q_WHAT_LIKE)
				a.topic = T_HOBBY;
			if (a.intent == I_UNKNOWN_QUESTION && a.qtype != Q_OPEN && a.qtype != Q_STATEMENT)
				a.intent = a.rawIntent = I_GENERAL;
		}
		a.polarityNegative = a.tokens.Has("malo") || a.tokens.Has("pusto") || a.concepts.Has(C_DISLIKE);
		if (a.intent == I_ITEM_OWN)
			a.object = ExtractTradeObject(a.tokens, FindWordIndexOfConcept(c, C_HAVE));
		if (a.intent == I_SHOP || a.intent == I_PRICE)
			a.object = ExtractTradeObject(a.tokens, 0);
		if (a.intent == I_BUY)
			a.object = ExtractTradeObject(a.tokens, FindWordIndexOfConcept(c, C_BUYME));
		if (a.intent == I_SELL)
			a.object = ExtractTradeObject(a.tokens, FindWordIndexOfConcept(c, C_SELLYOU));

		// "dam ci 2kk za fms" is an offer to buy, not a gift: a sum named for
		// an item, and no word that says it is free.
		if (a.intent == I_GIFT_OFFER && a.offerYang > 0 && !a.tokens.Has("darmo") && !a.tokens.Has("free") &&
				!a.tokens.Has("gratis"))
		{
			const int za = a.tokens.Find("za");
			const std::string obj = za >= 0 ? ExtractTradeObject(a.tokens, za + 1) : std::string();
			if (!obj.empty())
			{
				a.intent = a.rawIntent = I_BUY;
				a.object = obj;
			}
		}
		if (hasLink)
		{
			// The link names the item whatever the words around it say.
			a.itemLink = true;
			a.itemShown = link;
			a.object = FoldName(link.c_str());
			a.objectPlus = RefineOfName(link);
		}
		else if (a.intent == I_SHOW_ITEM || a.intent == I_GIFT_OFFER || a.intent == I_GEAR_WHY ||
				a.intent == I_GEAR_ADVICE || a.intent == I_GEAR_OPINION)
			ExtractGearObject(a);
	}
}

#endif
