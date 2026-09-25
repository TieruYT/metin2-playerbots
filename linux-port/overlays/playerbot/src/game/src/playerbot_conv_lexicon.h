#ifndef __INC_METIN2_PLAYERBOT_CONV_LEXICON_H__
#define __INC_METIN2_PLAYERBOT_CONV_LEXICON_H__

// PlayerBot Conversation v6 - lexicon (pure).
//
// Words are not matched to answers. Words are matched to CONCEPTS
// ("gdzie", "dokad" -> C_WHERE; "expisz", "farmisz", "grindujesz" -> C_EXP),
// and intents are decided from the set of concepts a line carries
// (playerbot_conv_intents.h). Adding a synonym is one line here; adding a new
// topic is one concept here plus one rule there.
//
// Match modes:
//   EXACT  - the word itself,
//   PREFIX - any word starting with the stem ("rob" -> robisz, robie, robic),
//            and, for stems of 5+ letters, a word one typo away from the stem,
//   FUZZY  - the word itself or one typo away (5+ letters),
//   PHRASE - several words in a row, matched on word boundaries,
//   SUFFIX - any word of 5+ letters ending with the stem ("bys" -> zrobilbys).

#include "playerbot_conv_aliases.h"

namespace playerbot_conv
{
	enum EConcept
	{
		C_NONE = 0,
		// question words and pronouns
		C_WHAT, C_WHERE, C_HOW, C_HOWMUCH, C_WHO, C_WHY, C_WHEN, C_WHICH,
		C_YOU, C_ME, C_WE, C_THEM, C_THIS, C_THERE, C_BE, C_HAVE, C_CAN, C_WANT,
		C_AND, C_WHATWITH, C_WITHWHO, C_WITHYOU, C_WITHME, C_KNOW, C_CANYOU, C_EVER,
		C_PLAY,
		// time
		C_NOW, C_NEXT, C_TODAY, C_LONG, C_WHENTIME,
		// game
		C_DO, C_PLAN, C_GOAL, C_ACHIEVE, C_EXP, C_HIT, C_MOB, C_MANY, C_MAP, C_PLACE,
		C_LEVEL, C_CLASS, C_EMPIRE, C_NAME, C_HP, C_LIFE, C_GOLD, C_HORSE, C_EQ, C_FREE,
		C_GEAR, C_PARTY, C_ALONE, C_JOIN, C_GUILD, C_TARGET, C_FISH, C_MINE, C_HERB,
		C_BIO, C_METIN, C_DT, C_WAR, C_MERC, C_TRADE, C_SHOP, C_BUYME, C_SELLYOU,
		C_SKILL, C_PVP, C_TRAVEL, C_REST, C_TOWN, C_DROP, C_LUCK, C_UPGRADE, C_QUEST,
		C_KILLED, C_CHARACTER, C_MOOD, C_DID, C_BOSS,
		// social
		C_GREET, C_BYE, C_THANKS, C_SORRY, C_HOWAREYOU, C_HELP, C_BOT, C_INSULT, C_PRAISE,
		C_AGE, C_ORIGIN, C_ACK, C_LAUGH, C_SURPRISE, C_YES, C_NO, C_BUDDY,
		// preference / open questions
		C_LIKE, C_DISLIKE, C_FAV, C_PREFER, C_HYPO, C_DREAM, C_FEAR, C_ANNOY, C_JOY, C_THINK,
		C_OR,
		// general topics
		C_WEATHER, C_COLD, C_WARM, C_RAIN, C_SEASON, C_DAYTIME, C_SLEEP, C_TIRED, C_BORED,
		C_HOBBY, C_FOOD, C_DRINK, C_TRAVELG, C_MUSIC, C_MOVIE, C_GAMES, C_HUMOR, C_FRIEND,
		C_TEAMWORK, C_LONELY, C_RISK, C_MONEY, C_WORK, C_SCHOOL, C_LIFEG, C_SAD, C_HAPPY,
		C_ANIMAL, C_SPORT, C_LOVE, C_BOOKS, C_NATURE, C_ADVENTURE, C_COLOR,
		C_POSITIVE, C_NEGATIVE,
		// items, prices, the shorthand of the game
		C_ITEMWORD, C_PRICEQ, C_ITEMSHOP, C_KS, C_READY, C_GOODLUCK, C_BRB, C_MAPNAME, C_BONUS,
		// a class's path, a Shaman's buffs, a person calling the bot over and letting it go
		C_BUILD, C_BUFF, C_BUFFNAME, C_GIVE, C_SUMMON, C_DISMISS,
		// a person talking AT the bot rather than asking it something: told to
		// stop writing, threatened with a ban, mocked, sworn at
		C_STOPTALK, C_THREAT, C_MOCK, C_SWEAR,
		// the bot's gear argued about: an item shown off, one to swap to, advice,
		// a gift offered - and "you said something else before"
		C_SHOWOFF, C_SWAP, C_ADVICE, C_GIFT, C_SAIDBEFORE,
		C_COUNT
	};

	typedef char TConceptCountFits[C_COUNT <= 256 ? 1 : -1];

	enum EMatchMode
	{
		M_EXACT = 0,
		M_PREFIX,
		M_FUZZY,
		M_PHRASE,
		M_SUFFIX
	};

	struct TLexEntry
	{
		const char* text;
		unsigned char conceptId;
		unsigned char mode;
	};

	// Four words of bits is room for 256 concepts.
	struct TConceptSet
	{
		unsigned long long bits[4];
		signed char firstWord[C_COUNT];

		TConceptSet() { Clear(); }
		void Clear()
		{
			bits[0] = bits[1] = bits[2] = bits[3] = 0;
			for (int i = 0; i < C_COUNT; ++i)
				firstWord[i] = -1;
		}
		void Set(int c, int word)
		{
			if (c <= C_NONE || c >= C_COUNT)
				return;
			bits[c >> 6] |= (1ULL << (c & 63));
			if (firstWord[c] < 0 || (word >= 0 && word < firstWord[c]))
				firstWord[c] = (signed char)word;
		}
		void Unset(int c)
		{
			if (c <= C_NONE || c >= C_COUNT)
				return;
			bits[c >> 6] &= ~(1ULL << (c & 63));
			firstWord[c] = -1;
		}
		bool Has(int c) const
		{
			if (c <= C_NONE || c >= C_COUNT)
				return false;
			return (bits[c >> 6] & (1ULL << (c & 63))) != 0;
		}
		int Count() const
		{
			int n = 0;
			for (int i = 1; i < C_COUNT; ++i)
				if (Has(i))
					++n;
			return n;
		}
	};

	inline const TLexEntry* GetLexicon(size_t& count)
	{
		static const TLexEntry kLex[] = {
			// ---- question words / pronouns
			{ "co", C_WHAT, M_EXACT }, { "czym", C_WHAT, M_EXACT }, { "czego", C_WHAT, M_EXACT },
			{ "cos", C_WHAT, M_EXACT },
			{ "gdzie", C_WHERE, M_FUZZY }, { "dokad", C_WHERE, M_FUZZY }, { "gdzies", C_WHERE, M_EXACT },
			{ "jak", C_HOW, M_EXACT }, { "jakos", C_HOW, M_EXACT },
			{ "ile", C_HOWMUCH, M_EXACT }, { "ilu", C_HOWMUCH, M_EXACT },
			{ "kto", C_WHO, M_EXACT }, { "kogo", C_WHO, M_EXACT }, { "komu", C_WHO, M_EXACT },
			{ "dlaczego", C_WHY, M_FUZZY }, { "czemu", C_WHY, M_EXACT }, { "po co", C_WHY, M_PHRASE },
			{ "z jakiego powodu", C_WHY, M_PHRASE }, { "czemuz", C_WHY, M_EXACT },
			{ "kiedy", C_WHEN, M_EXACT }, { "byles", C_EVER, M_EXACT }, { "bylas", C_EVER, M_EXACT }, { "o ktorej", C_WHEN, M_PHRASE }, { "kiedys", C_EVER, M_EXACT },
			{ "jaki", C_WHICH, M_EXACT }, { "jaka", C_WHICH, M_EXACT }, { "jakie", C_WHICH, M_EXACT },
			{ "jakiej", C_WHICH, M_EXACT }, { "jakim", C_WHICH, M_EXACT }, { "jakiego", C_WHICH, M_EXACT },
			{ "jakis", C_WHICH, M_EXACT }, { "jakies", C_WHICH, M_EXACT }, { "jakas", C_WHICH, M_EXACT },
			{ "ktory", C_WHICH, M_EXACT }, { "ktora", C_WHICH, M_EXACT }, { "ktore", C_WHICH, M_EXACT },
			{ "ktorej", C_WHICH, M_EXACT }, { "ktorym", C_WHICH, M_EXACT },
			{ "ty", C_YOU, M_EXACT }, { "ciebie", C_YOU, M_EXACT }, { "cie", C_YOU, M_EXACT },
			{ "tobie", C_YOU, M_EXACT }, { "toba", C_YOU, M_EXACT }, { "twoj", C_YOU, M_PREFIX },
			{ "twoi", C_YOU, M_EXACT }, { "u ciebie", C_YOU, M_PHRASE },
			{ "ja", C_ME, M_EXACT }, { "mnie", C_ME, M_EXACT }, { "mi", C_ME, M_EXACT },
			{ "mna", C_ME, M_EXACT }, { "moj", C_ME, M_PREFIX }, { "mojego", C_ME, M_EXACT },
			{ "my", C_WE, M_EXACT }, { "nas", C_WE, M_EXACT }, { "jestesmy", C_WE, M_EXACT },
			{ "nami", C_WE, M_EXACT },
			{ "ich", C_THEM, M_EXACT }, { "nich", C_THEM, M_EXACT }, { "oni", C_THEM, M_EXACT },
			{ "one", C_THEM, M_EXACT }, { "tych", C_THEM, M_EXACT },
			{ "ten", C_THIS, M_EXACT }, { "ta", C_THIS, M_EXACT }, { "to", C_THIS, M_EXACT },
			{ "tym", C_THIS, M_EXACT }, { "tego", C_THIS, M_EXACT }, { "tamten", C_THIS, M_EXACT },
			{ "te", C_THIS, M_EXACT }, { "tej", C_THIS, M_EXACT },
			{ "tam", C_THERE, M_EXACT }, { "tu", C_THERE, M_EXACT }, { "tutaj", C_THERE, M_EXACT },
			{ "tutej", C_THERE, M_EXACT }, { "tamtej", C_THERE, M_EXACT },
			{ "jestes", C_BE, M_FUZZY }, { "jestescie", C_BE, M_EXACT }, { "siedzisz", C_BE, M_FUZZY },
			{ "stoisz", C_BE, M_EXACT }, { "przebywasz", C_BE, M_EXACT }, { "znajdujesz", C_BE, M_EXACT },
			{ "bywasz", C_BE, M_EXACT }, { "jestem", C_BE, M_EXACT },
			{ "masz", C_HAVE, M_EXACT }, { "posiadasz", C_HAVE, M_FUZZY }, { "macie", C_HAVE, M_EXACT },
			{ "moge", C_CAN, M_EXACT }, { "mozna", C_CAN, M_EXACT }, { "mozemy", C_CAN, M_EXACT },
			{ "mozesz", C_CAN, M_EXACT }, { "da sie", C_CAN, M_PHRASE },
			{ "chcesz", C_WANT, M_FUZZY }, { "chcialbys", C_WANT, M_FUZZY }, { "ochot", C_WANT, M_PREFIX },
			{ "chce", C_WANT, M_EXACT },
			{ "i", C_AND, M_EXACT }, { "no i", C_AND, M_PHRASE }, { "i co", C_AND, M_PHRASE },
			{ "co z", C_WHATWITH, M_PHRASE }, { "a co z", C_WHATWITH, M_PHRASE }, { "jak z", C_WHATWITH, M_PHRASE },
			{ "jak ze", C_WHATWITH, M_PHRASE }, { "co ze", C_WHATWITH, M_PHRASE },
			{ "z kim", C_WITHWHO, M_PHRASE }, { "z kims", C_WITHWHO, M_PHRASE },
			{ "z toba", C_WITHYOU, M_PHRASE }, { "z tb", C_WITHYOU, M_PHRASE }, { "z tobom", C_WITHYOU, M_PHRASE },
			{ "ze mna", C_WITHME, M_PHRASE }, { "z nami", C_WITHME, M_PHRASE },
			{ "wiesz", C_KNOW, M_EXACT }, { "znasz", C_KNOW, M_EXACT }, { "slyszales", C_KNOW, M_FUZZY },
			{ "umiesz", C_CANYOU, M_EXACT }, { "potrafisz", C_CANYOU, M_FUZZY },
			{ "grasz", C_PLAY, M_EXACT }, { "grales", C_PLAY, M_EXACT }, { "grac", C_PLAY, M_EXACT },
			// ---- time
			{ "teraz", C_NOW, M_FUZZY }, { "aktualnie", C_NOW, M_FUZZY }, { "obecnie", C_NOW, M_FUZZY },
			{ "wlasnie", C_NOW, M_FUZZY }, { "akurat", C_NOW, M_FUZZY }, { "w tej chwili", C_NOW, M_PHRASE },
			{ "potem", C_NEXT, M_EXACT }, { "dalej", C_NEXT, M_EXACT }, { "zaraz", C_NEXT, M_EXACT },
			{ "po tym", C_NEXT, M_PHRASE }, { "nastepne", C_NEXT, M_EXACT }, { "jutro", C_NEXT, M_EXACT },
			{ "dzisiaj", C_TODAY, M_EXACT }, { "dzien", C_TODAY, M_EXACT }, { "dnia", C_TODAY, M_EXACT },
			{ "dlugo", C_LONG, M_EXACT }, { "od dawna", C_LONG, M_PHRASE }, { "od kiedy", C_LONG, M_PHRASE },
			{ "ile czasu", C_LONG, M_PHRASE },
			{ "godzina", C_WHENTIME, M_EXACT }, { "ktora godzina", C_WHENTIME, M_PHRASE },
			// ---- game
			{ "rob", C_DO, M_PREFIX }, { "porabia", C_DO, M_PREFIX }, { "zajmuj", C_DO, M_PREFIX },
			{ "zajety", C_DO, M_EXACT }, { "zajeta", C_DO, M_EXACT }, { "zajecie", C_DO, M_EXACT },
			{ "plan", C_PLAN, M_PREFIX }, { "zamierz", C_PLAN, M_PREFIX }, { "zamiar", C_PLAN, M_PREFIX },
			{ "cel", C_GOAL, M_EXACT }, { "celem", C_GOAL, M_EXACT }, { "cele", C_GOAL, M_EXACT },
			{ "celu", C_GOAL, M_EXACT }, { "dazysz", C_GOAL, M_EXACT }, { "ambicj", C_GOAL, M_PREFIX },
			{ "osiagn", C_ACHIEVE, M_PREFIX }, { "zdobyc", C_ACHIEVE, M_EXACT },
			{ "exp", C_EXP, M_PREFIX }, { "farm", C_EXP, M_PREFIX }, { "grind", C_EXP, M_PREFIX },
			{ "lvluj", C_EXP, M_PREFIX }, { "leveluj", C_EXP, M_PREFIX }, { "nabijasz", C_EXP, M_EXACT },
			{ "levelowa", C_EXP, M_PREFIX }, { "ekspi", C_EXP, M_PREFIX },
			{ "bij", C_HIT, M_PREFIX }, { "bic", C_HIT, M_EXACT }, { "atak", C_HIT, M_PREFIX },
			{ "zbic", C_HIT, M_EXACT }, { "zbij", C_HIT, M_EXACT }, { "zbijesz", C_HIT, M_EXACT },
			{ "zbijam", C_HIT, M_EXACT }, { "zbije", C_HIT, M_EXACT }, { "zbijemy", C_HIT, M_EXACT },
			{ "walcz", C_HIT, M_PREFIX }, { "walk", C_HIT, M_PREFIX }, { "tlucz", C_HIT, M_PREFIX },
			{ "klepiesz", C_HIT, M_EXACT }, { "ubij", C_HIT, M_PREFIX }, { "zabijasz", C_HIT, M_EXACT },
			{ "siekasz", C_HIT, M_EXACT }, { "mordujesz", C_HIT, M_EXACT },
			{ "mob", C_MOB, M_PREFIX }, { "potwor", C_MOB, M_PREFIX }, { "stwor", C_MOB, M_PREFIX },
			{ "potworow", C_MOB, M_EXACT },
			{ "duzo", C_MANY, M_EXACT }, { "sporo", C_MANY, M_EXACT }, { "wiele", C_MANY, M_EXACT },
			{ "wielu", C_MANY, M_EXACT }, { "kupa", C_MANY, M_EXACT }, { "pelno", C_MANY, M_EXACT },
			{ "mnostwo", C_MANY, M_EXACT }, { "masa", C_MANY, M_EXACT }, { "tloczno", C_MANY, M_EXACT },
			{ "gesto", C_MANY, M_EXACT }, { "malo", C_MANY, M_EXACT }, { "pusto", C_MANY, M_EXACT },
			{ "pustki", C_MANY, M_EXACT }, { "tlok", C_MANY, M_EXACT }, { "maly", C_MANY, M_EXACT },
			{ "map", C_MAP, M_PREFIX }, { "spot", C_MAP, M_PREFIX }, { "expowisk", C_MAP, M_PREFIX },
			{ "lokac", C_MAP, M_PREFIX }, { "teren", C_MAP, M_PREFIX }, { "okolic", C_MAP, M_PREFIX },
			{ "miejsc", C_PLACE, M_PREFIX },
			{ "lvl", C_LEVEL, M_EXACT }, { "level", C_LEVEL, M_PREFIX }, { "poziom", C_LEVEL, M_PREFIX },
			{ "lwl", C_LEVEL, M_EXACT },
			{ "klas", C_CLASS, M_PREFIX }, { "postac", C_CLASS, M_PREFIX }, { "wojownik", C_CLASS, M_PREFIX },
			{ "ninja", C_CLASS, M_PREFIX }, { "sura", C_CLASS, M_EXACT }, { "szaman", C_CLASS, M_PREFIX },
			{ "profes", C_CLASS, M_PREFIX }, { "czym grasz", C_CLASS, M_PHRASE }, { "kim grasz", C_CLASS, M_PHRASE },
			{ "imperi", C_EMPIRE, M_PREFIX }, { "krolestw", C_EMPIRE, M_PREFIX }, { "shinsoo", C_EMPIRE, M_FUZZY },
			{ "chunjo", C_EMPIRE, M_FUZZY }, { "jinno", C_EMPIRE, M_FUZZY }, { "flag", C_EMPIRE, M_PREFIX },
			{ "imie", C_NAME, M_EXACT }, { "imienia", C_NAME, M_EXACT }, { "nazyw", C_NAME, M_PREFIX },
			{ "nick", C_NAME, M_PREFIX }, { "zwiesz", C_NAME, M_EXACT },
			{ "hp", C_HP, M_EXACT }, { "sp", C_HP, M_EXACT }, { "mana", C_HP, M_EXACT }, { "many", C_HP, M_EXACT },
			{ "zdrow", C_HP, M_PREFIX }, { "punkty zycia", C_HP, M_PHRASE },
			{ "zycie", C_LIFE, M_EXACT }, { "zycia", C_LIFE, M_EXACT }, { "zyciem", C_LIFE, M_EXACT },
			{ "zyciu", C_LIFE, M_EXACT },
			{ "yang", C_GOLD, M_EXACT }, { "kasa", C_GOLD, M_EXACT }, { "hajs", C_GOLD, M_EXACT },
			{ "zlot", C_GOLD, M_PREFIX }, { "gold", C_GOLD, M_EXACT }, { "siana", C_GOLD, M_EXACT },
			{ "pieniadz", C_MONEY, M_PREFIX }, { "pieniedz", C_MONEY, M_PREFIX }, { "kasiora", C_GOLD, M_EXACT },
			{ "kon", C_HORSE, M_EXACT }, { "konia", C_HORSE, M_EXACT }, { "koniem", C_HORSE, M_EXACT },
			{ "koniu", C_HORSE, M_EXACT }, { "konie", C_HORSE, M_EXACT }, { "wierzchow", C_HORSE, M_PREFIX },
			{ "rumak", C_HORSE, M_PREFIX },
			{ "eq", C_EQ, M_EXACT }, { "plecak", C_EQ, M_PREFIX }, { "torb", C_EQ, M_PREFIX },
			{ "inwentar", C_EQ, M_PREFIX }, { "bagaz", C_EQ, M_PREFIX },
			{ "woln", C_FREE, M_PREFIX }, { "peln", C_FREE, M_PREFIX }, { "zapchan", C_FREE, M_PREFIX },
			{ "slot", C_FREE, M_PREFIX },
			{ "bron", C_GEAR, M_EXACT }, { "broni", C_GEAR, M_EXACT }, { "bronia", C_GEAR, M_EXACT },
			{ "zbroj", C_GEAR, M_PREFIX }, { "miecz", C_GEAR, M_PREFIX }, { "luk", C_GEAR, M_EXACT },
			{ "tarcz", C_GEAR, M_PREFIX }, { "helm", C_GEAR, M_PREFIX }, { "buty", C_GEAR, M_EXACT },
			{ "sprzet", C_GEAR, M_PREFIX }, { "item", C_GEAR, M_PREFIX }, { "nosisz", C_GEAR, M_EXACT },
			{ "set", C_GEAR, M_EXACT }, { "seta", C_GEAR, M_EXACT }, { "sztylet", C_GEAR, M_PREFIX },
			{ "ostrz", C_GEAR, M_PREFIX }, { "dzwon", C_GEAR, M_PREFIX }, { "wachlarz", C_GEAR, M_PREFIX },
			{ "naszyjnik", C_GEAR, M_PREFIX }, { "bransolet", C_GEAR, M_PREFIX }, { "kolczyk", C_GEAR, M_PREFIX },
			{ "pt", C_PARTY, M_EXACT }, { "druzyn", C_PARTY, M_PREFIX }, { "grup", C_PARTY, M_PREFIX },
			{ "ekip", C_PARTY, M_PREFIX }, { "lider", C_PARTY, M_PREFIX }, { "team", C_PARTY, M_PREFIX },
			{ "sam", C_ALONE, M_EXACT }, { "sama", C_ALONE, M_EXACT }, { "samemu", C_ALONE, M_EXACT },
			{ "solo", C_ALONE, M_EXACT }, { "samotnie", C_ALONE, M_EXACT },
			{ "dolacz", C_JOIN, M_PREFIX }, { "zapro", C_JOIN, M_PREFIX }, { "chodz", C_JOIN, M_EXACT },
			{ "chodzmy", C_JOIN, M_EXACT }, { "idziemy", C_JOIN, M_EXACT }, { "wbijaj", C_JOIN, M_EXACT },
			{ "wbijesz", C_JOIN, M_EXACT }, { "pojdziesz", C_JOIN, M_EXACT }, { "przyjmiesz", C_JOIN, M_EXACT },
			{ "przyjmij", C_JOIN, M_EXACT }, { "wezmiesz", C_JOIN, M_EXACT }, { "wez mnie", C_JOIN, M_PHRASE },
			{ "pomozesz", C_JOIN, M_EXACT }, { "pomoz mi", C_JOIN, M_PHRASE }, { "razem", C_JOIN, M_EXACT },
			{ "gildi", C_GUILD, M_PREFIX }, { "gildyj", C_GUILD, M_PREFIX },
			{ "target", C_TARGET, M_PREFIX }, { "celown", C_TARGET, M_PREFIX }, { "celujesz", C_TARGET, M_EXACT },
			{ "namierz", C_TARGET, M_PREFIX },
			{ "low", C_FISH, M_PREFIX }, { "ryb", C_FISH, M_PREFIX }, { "wedk", C_FISH, M_PREFIX },
			{ "kopi", C_MINE, M_PREFIX }, { "kopac", C_MINE, M_EXACT }, { "kopal", C_MINE, M_PREFIX },
			{ "rud", C_MINE, M_PREFIX }, { "gornik", C_MINE, M_PREFIX }, { "kilof", C_MINE, M_PREFIX },
			{ "wydobyw", C_MINE, M_PREFIX },
			{ "zielar", C_HERB, M_PREFIX }, { "ziol", C_HERB, M_PREFIX }, { "mikstur", C_HERB, M_PREFIX },
			{ "potk", C_HERB, M_PREFIX }, { "potek", C_HERB, M_EXACT }, { "alchemi", C_HERB, M_PREFIX },
			{ "biolog", C_BIO, M_PREFIX },
			{ "metin", C_METIN, M_PREFIX }, { "kamien", C_METIN, M_PREFIX }, { "stone", C_METIN, M_EXACT },
			{ "wiez", C_DT, M_PREFIX }, { "dt", C_DT, M_EXACT }, { "demon", C_DT, M_PREFIX },
			{ "tower", C_DT, M_EXACT },
			{ "wojn", C_WAR, M_PREFIX },
			{ "najemni", C_MERC, M_PREFIX }, { "kontrakt", C_MERC, M_PREFIX },
			{ "handl", C_TRADE, M_PREFIX }, { "handel", C_TRADE, M_EXACT }, { "sprzedajesz", C_TRADE, M_FUZZY },
			{ "kupujesz", C_TRADE, M_FUZZY }, { "rynek", C_TRADE, M_EXACT }, { "rynku", C_TRADE, M_EXACT },
			{ "targ", C_TRADE, M_EXACT }, { "targu", C_TRADE, M_EXACT }, { "targowisk", C_TRADE, M_PREFIX },
			{ "cena", C_TRADE, M_EXACT }, { "ceny", C_TRADE, M_EXACT }, { "cenach", C_TRADE, M_EXACT },
			{ "cene", C_TRADE, M_EXACT },
			{ "stragan", C_SHOP, M_PREFIX }, { "sklep", C_SHOP, M_PREFIX }, { "shop", C_SHOP, M_EXACT },
			{ "wystawiasz", C_SHOP, M_EXACT }, { "wystawione", C_SHOP, M_EXACT },
			{ "sprzedasz mi", C_BUYME, M_PHRASE }, { "odsprzedasz mi", C_BUYME, M_PHRASE },
			{ "odsprzedasz", C_BUYME, M_EXACT }, { "chce kupic od ciebie", C_BUYME, M_PHRASE },
			{ "chcialbym kupic od ciebie", C_BUYME, M_PHRASE }, { "kupie od ciebie", C_BUYME, M_PHRASE },
			{ "masz na sprzedaz", C_BUYME, M_PHRASE }, { "chce kupic", C_BUYME, M_PHRASE },
			{ "kupie", C_BUYME, M_EXACT }, { "szukam", C_BUYME, M_EXACT }, { "sprzedam", C_SELLYOU, M_EXACT },
			{ "sprzedam ci", C_SELLYOU, M_PHRASE }, { "chce ci sprzedac", C_SELLYOU, M_PHRASE },
			{ "chcialbym ci sprzedac", C_SELLYOU, M_PHRASE }, { "kupisz ode mnie", C_SELLYOU, M_PHRASE },
			{ "kupisz", C_SELLYOU, M_EXACT }, { "oddam ci", C_SELLYOU, M_PHRASE },
			{ "skill", C_SKILL, M_PREFIX }, { "umiejetnos", C_SKILL, M_PREFIX }, { "ku", C_SKILL, M_EXACT },
			{ "ksieg", C_SKILL, M_PREFIX }, { "perfekt", C_SKILL, M_PREFIX }, { "trening", C_SKILL, M_PREFIX },
			{ "pvp", C_PVP, M_EXACT }, { "pojedyn", C_PVP, M_PREFIX }, { "duel", C_PVP, M_PREFIX },
			{ "pk", C_PVP, M_EXACT }, { "1v1", C_PVP, M_EXACT }, { "solowk", C_PVP, M_PREFIX },
			{ "walcz ze mna", C_PVP, M_PHRASE }, { "bijesz sie", C_PVP, M_PHRASE },
			{ "jedziesz", C_TRAVEL, M_FUZZY }, { "idziesz", C_TRAVEL, M_FUZZY }, { "podroz", C_TRAVEL, M_PREFIX },
			{ "drog", C_TRAVEL, M_PREFIX }, { "zmierz", C_TRAVEL, M_PREFIX }, { "teleport", C_TRAVEL, M_PREFIX },
			{ "jedzie", C_TRAVEL, M_EXACT }, { "wracasz", C_TRAVEL, M_FUZZY },
			{ "odpocz", C_REST, M_PREFIX }, { "przerw", C_REST, M_PREFIX }, { "afk", C_REST, M_EXACT },
			{ "regener", C_REST, M_PREFIX },
			{ "miast", C_TOWN, M_PREFIX }, { "miescie", C_TOWN, M_EXACT }, { "wiosk", C_TOWN, M_PREFIX },
			{ "wiosce", C_TOWN, M_EXACT },
			{ "drop", C_DROP, M_PREFIX }, { "lup", C_DROP, M_PREFIX }, { "loot", C_DROP, M_PREFIX },
			{ "wypad", C_DROP, M_PREFIX },
			{ "szczesc", C_LUCK, M_PREFIX }, { "fart", C_LUCK, M_PREFIX }, { "pech", C_LUCK, M_PREFIX },
			{ "ulepsz", C_UPGRADE, M_PREFIX }, { "kowal", C_UPGRADE, M_PREFIX }, { "plusow", C_UPGRADE, M_PREFIX },
			{ "misj", C_QUEST, M_PREFIX }, { "quest", C_QUEST, M_PREFIX }, { "zadani", C_QUEST, M_PREFIX },
			{ "polowani", C_QUEST, M_PREFIX },
			{ "zgin", C_KILLED, M_PREFIX }, { "umarl", C_KILLED, M_PREFIX }, { "padles", C_KILLED, M_EXACT },
			{ "zabili", C_KILLED, M_EXACT }, { "zabil", C_KILLED, M_EXACT }, { "smierc", C_KILLED, M_PREFIX },
			{ "charakter", C_CHARACTER, M_PREFIX }, { "osobowos", C_CHARACTER, M_PREFIX },
			{ "jaki jestes", C_CHARACTER, M_PHRASE }, { "jaki masz typ", C_CHARACTER, M_PHRASE },
			{ "nastroj", C_MOOD, M_PREFIX }, { "humor", C_MOOD, M_PREFIX }, { "samopoczu", C_MOOD, M_PREFIX },
			{ "czujesz", C_MOOD, M_EXACT },
			{ "zrobil", C_DID, M_PREFIX }, { "udalo", C_DID, M_PREFIX }, { "nabiles", C_DID, M_EXACT },
			{ "boss", C_BOSS, M_PREFIX },
			// ---- social
			{ "czesc", C_GREET, M_EXACT }, { "hej", C_GREET, M_EXACT }, { "siema", C_GREET, M_EXACT },
			{ "witaj", C_GREET, M_EXACT }, { "witam", C_GREET, M_EXACT }, { "dzien dobry", C_GREET, M_PHRASE },
			{ "dobry wieczor", C_GREET, M_PHRASE }, { "serwus", C_GREET, M_EXACT }, { "hejo", C_GREET, M_EXACT },
			{ "nara", C_BYE, M_EXACT }, { "pa", C_BYE, M_EXACT }, { "do zobaczenia", C_BYE, M_PHRASE },
			{ "dobranoc", C_BYE, M_EXACT }, { "lece", C_BYE, M_EXACT }, { "spadam", C_BYE, M_EXACT },
			{ "uciekam", C_BYE, M_EXACT }, { "zmykam", C_BYE, M_EXACT }, { "trzymaj sie", C_BYE, M_PHRASE },
			{ "na razie", C_BYE, M_PHRASE }, { "narazie", C_BYE, M_EXACT }, { "pozdrawiam", C_BYE, M_EXACT },
			{ "dzieki", C_THANKS, M_EXACT }, { "dziekuje", C_THANKS, M_EXACT }, { "wielkie dzieki", C_THANKS, M_PHRASE },
			{ "przepraszam", C_SORRY, M_FUZZY }, { "sorry", C_SORRY, M_EXACT }, { "sory", C_SORRY, M_EXACT },
			{ "sorki", C_SORRY, M_EXACT }, { "wybacz", C_SORRY, M_PREFIX }, { "moja wina", C_SORRY, M_PHRASE },
			{ "co tam", C_HOWAREYOU, M_PHRASE }, { "jak tam", C_HOWAREYOU, M_PHRASE }, { "jak leci", C_HOWAREYOU, M_PHRASE },
			{ "co slychac", C_HOWAREYOU, M_PHRASE }, { "jak sie masz", C_HOWAREYOU, M_PHRASE },
			{ "wszystko ok", C_HOWAREYOU, M_PHRASE }, { "wszystko dobrze", C_HOWAREYOU, M_PHRASE },
			{ "jak zycie", C_HOWAREYOU, M_PHRASE }, { "jak dzien", C_HOWAREYOU, M_PHRASE },
			{ "jak idzie", C_HOWAREYOU, M_PHRASE }, { "jak ci idzie", C_HOWAREYOU, M_PHRASE },
			{ "co u ciebie", C_HOWAREYOU, M_PHRASE }, { "jak mija", C_HOWAREYOU, M_PHRASE },
			{ "wszystko git", C_HOWAREYOU, M_PHRASE }, { "jak samopoczucie", C_HOWAREYOU, M_PHRASE },
			{ "pomocy", C_HELP, M_EXACT }, { "co potrafisz", C_HELP, M_PHRASE }, { "co umiesz", C_HELP, M_PHRASE },
			{ "komend", C_HELP, M_PREFIX }, { "jak z toba gadac", C_HELP, M_PHRASE }, { "o co moge pytac", C_HELP, M_PHRASE },
			{ "bot", C_BOT, M_EXACT }, { "botem", C_BOT, M_EXACT }, { "boty", C_BOT, M_EXACT },
			{ "bota", C_BOT, M_EXACT }, { "botow", C_BOT, M_EXACT }, { "npc", C_BOT, M_EXACT },
			{ "czlowiek", C_BOT, M_PREFIX }, { "prawdziwy gracz", C_BOT, M_PHRASE }, { "sztuczna inteligencja", C_BOT, M_PHRASE },
			{ "debil", C_INSULT, M_PREFIX }, { "idiot", C_INSULT, M_PREFIX }, { "glupi", C_INSULT, M_PREFIX },
			{ "noob", C_INSULT, M_PREFIX }, { "nub", C_INSULT, M_EXACT }, { "frajer", C_INSULT, M_PREFIX },
			{ "kretyn", C_INSULT, M_PREFIX }, { "lamus", C_INSULT, M_PREFIX }, { "spadaj", C_INSULT, M_EXACT },
			{ "zamknij sie", C_INSULT, M_PHRASE }, { "ssiesz", C_INSULT, M_EXACT }, { "cienias", C_INSULT, M_PREFIX },
			{ "pajac", C_INSULT, M_PREFIX }, { "baran", C_INSULT, M_EXACT }, { "gamon", C_INSULT, M_PREFIX },
			{ "kozak", C_PRAISE, M_PREFIX }, { "szacun", C_PRAISE, M_PREFIX }, { "gratk", C_PRAISE, M_PREFIX },
			{ "gratulac", C_PRAISE, M_PREFIX }, { "brawo", C_PRAISE, M_EXACT }, { "fajny jestes", C_PRAISE, M_PHRASE },
			{ "spoko jestes", C_PRAISE, M_PHRASE }, { "mily jestes", C_PRAISE, M_PHRASE }, { "dobry jestes", C_PRAISE, M_PHRASE },
			{ "fajny z ciebie", C_PRAISE, M_PHRASE }, { "equ", C_PRAISE, M_EXACT },
			{ "ile masz lat", C_AGE, M_PHRASE }, { "lat masz", C_AGE, M_PHRASE }, { "wiek", C_AGE, M_EXACT },
			{ "ile lat", C_AGE, M_PHRASE }, { "stary jestes", C_AGE, M_PHRASE },
			{ "skad jestes", C_ORIGIN, M_PHRASE }, { "pochodzisz", C_ORIGIN, M_FUZZY }, { "skad", C_ORIGIN, M_EXACT },
			{ "ok", C_ACK, M_EXACT }, { "aha", C_ACK, M_EXACT }, { "aa", C_ACK, M_EXACT }, { "dobra", C_ACK, M_EXACT },
			{ "spoko", C_ACK, M_EXACT }, { "git", C_ACK, M_EXACT }, { "rozumiem", C_ACK, M_EXACT },
			{ "luz", C_ACK, M_EXACT }, { "jasne", C_ACK, M_EXACT }, { "no", C_ACK, M_EXACT },
			{ "ta", C_ACK, M_EXACT }, { "mhm", C_ACK, M_EXACT }, { "okej", C_ACK, M_EXACT }, { "dobrze", C_ACK, M_EXACT },
			{ "xd", C_LAUGH, M_EXACT }, { "haha", C_LAUGH, M_EXACT }, { "hehe", C_LAUGH, M_EXACT },
			{ "serio", C_SURPRISE, M_EXACT }, { "naprawde", C_SURPRISE, M_FUZZY }, { "na pewno", C_SURPRISE, M_PHRASE },
			{ "powaznie", C_SURPRISE, M_FUZZY }, { "nie gadaj", C_SURPRISE, M_PHRASE }, { "no co ty", C_SURPRISE, M_PHRASE },
			{ "wow", C_SURPRISE, M_EXACT }, { "o kurcze", C_SURPRISE, M_PHRASE }, { "ooo", C_SURPRISE, M_EXACT },
			{ "tak", C_YES, M_EXACT }, { "pewnie", C_YES, M_EXACT }, { "oczywiscie", C_YES, M_FUZZY },
			{ "no ba", C_YES, M_PHRASE }, { "zgadza sie", C_YES, M_PHRASE }, { "dokladnie", C_YES, M_FUZZY },
			{ "nie", C_NO, M_EXACT }, { "nope", C_NO, M_EXACT }, { "raczej nie", C_NO, M_PHRASE },
			{ "ziomek", C_BUDDY, M_EXACT }, { "stary", C_BUDDY, M_EXACT },
			// ---- preferences / open questions
			{ "lubisz", C_LIKE, M_FUZZY }, { "lubi", C_LIKE, M_EXACT }, { "lubicie", C_LIKE, M_EXACT },
			{ "podoba", C_LIKE, M_PREFIX }, { "kochasz", C_LIKE, M_EXACT }, { "przepadasz", C_LIKE, M_EXACT },
			{ "nie lubisz", C_DISLIKE, M_PHRASE }, { "nienawidzisz", C_DISLIKE, M_FUZZY },
			{ "nie znosisz", C_DISLIKE, M_PHRASE }, { "nie cierpisz", C_DISLIKE, M_PHRASE },
			{ "ulubion", C_FAV, M_PREFIX }, { "najbardziej lubisz", C_FAV, M_PHRASE },
			{ "wolisz", C_PREFER, M_EXACT }, { "wolal", C_PREFER, M_PREFIX },
			{ "gdybys", C_HYPO, M_EXACT }, { "jakbys", C_HYPO, M_EXACT }, { "bys", C_HYPO, M_EXACT },
			{ "bys", C_HYPO, M_SUFFIX }, { "wyobraz", C_HYPO, M_PREFIX }, { "powiedzmy ze", C_HYPO, M_PHRASE },
			{ "marz", C_DREAM, M_PREFIX },
			{ "boisz", C_FEAR, M_EXACT }, { "boi", C_FEAR, M_EXACT }, { "strach", C_FEAR, M_PREFIX },
			{ "przeraz", C_FEAR, M_PREFIX }, { "obawiasz", C_FEAR, M_EXACT }, { "leka", C_FEAR, M_EXACT },
			{ "denerw", C_ANNOY, M_PREFIX }, { "wkurz", C_ANNOY, M_PREFIX }, { "irytuj", C_ANNOY, M_PREFIX },
			{ "wnerw", C_ANNOY, M_PREFIX }, { "drazni", C_ANNOY, M_EXACT },
			{ "ciesz", C_JOY, M_PREFIX }, { "rados", C_JOY, M_PREFIX }, { "raduje", C_JOY, M_EXACT },
			{ "myslisz", C_THINK, M_EXACT }, { "sadzisz", C_THINK, M_EXACT }, { "uwazasz", C_THINK, M_EXACT },
			{ "zdanie", C_THINK, M_EXACT }, { "opini", C_THINK, M_PREFIX },
			{ "czy", C_OR, M_EXACT }, { "albo", C_OR, M_EXACT }, { "lub", C_OR, M_EXACT },
			// ---- general topics
			{ "pogod", C_WEATHER, M_PREFIX }, { "aura", C_WEATHER, M_EXACT }, { "ponuro", C_WEATHER, M_EXACT },
			{ "szaro", C_WEATHER, M_EXACT }, { "chmur", C_WEATHER, M_PREFIX }, { "wiatr", C_WEATHER, M_PREFIX },
			{ "wieje", C_WEATHER, M_EXACT }, { "mgla", C_WEATHER, M_EXACT },
			{ "zimn", C_COLD, M_PREFIX }, { "mroz", C_COLD, M_PREFIX }, { "snieg", C_COLD, M_PREFIX },
			{ "sniez", C_COLD, M_PREFIX }, { "chlodn", C_COLD, M_PREFIX }, { "ziab", C_COLD, M_PREFIX },
			{ "marzn", C_COLD, M_PREFIX },
			{ "ciepl", C_WARM, M_PREFIX }, { "goraco", C_WARM, M_EXACT }, { "goracy", C_WARM, M_EXACT }, { "goraca", C_WARM, M_EXACT }, { "upal", C_WARM, M_PREFIX },
			{ "slonc", C_WARM, M_PREFIX }, { "duchota", C_WARM, M_EXACT },
			{ "deszcz", C_RAIN, M_PREFIX }, { "pada", C_RAIN, M_EXACT }, { "padalo", C_RAIN, M_EXACT },
			{ "leje", C_RAIN, M_EXACT }, { "burz", C_RAIN, M_PREFIX }, { "ulew", C_RAIN, M_PREFIX },
			{ "zima", C_SEASON, M_EXACT }, { "zime", C_SEASON, M_EXACT }, { "zimie", C_SEASON, M_EXACT },
			{ "zimy", C_SEASON, M_EXACT }, { "zimowy", C_SEASON, M_PREFIX }, { "lato", C_SEASON, M_EXACT },
			{ "lecie", C_SEASON, M_EXACT }, { "latem", C_SEASON, M_EXACT }, { "wiosn", C_SEASON, M_PREFIX },
			{ "jesien", C_SEASON, M_PREFIX }, { "pora roku", C_SEASON, M_PHRASE }, { "pore roku", C_SEASON, M_PHRASE },
			{ "rano", C_DAYTIME, M_EXACT }, { "ranek", C_DAYTIME, M_EXACT }, { "wieczor", C_DAYTIME, M_PREFIX },
			{ "noc", C_DAYTIME, M_EXACT }, { "nocy", C_DAYTIME, M_EXACT }, { "noca", C_DAYTIME, M_EXACT },
			{ "nock", C_DAYTIME, M_PREFIX }, { "pozno", C_DAYTIME, M_EXACT }, { "wczesnie", C_DAYTIME, M_EXACT },
			{ "polnoc", C_DAYTIME, M_EXACT },
			{ "spac", C_SLEEP, M_EXACT }, { "spanie", C_SLEEP, M_EXACT }, { "spisz", C_SLEEP, M_EXACT },
			{ "sen", C_SLEEP, M_EXACT }, { "senn", C_SLEEP, M_PREFIX }, { "spiac", C_SLEEP, M_PREFIX },
			{ "wysp", C_SLEEP, M_PREFIX }, { "zasypiam", C_SLEEP, M_EXACT },
			{ "zmecz", C_TIRED, M_PREFIX }, { "wykoncz", C_TIRED, M_PREFIX }, { "padam", C_TIRED, M_EXACT },
			{ "nie mam sil", C_TIRED, M_PHRASE }, { "zajechany", C_TIRED, M_EXACT },
			{ "nud", C_BORED, M_PREFIX }, { "znudz", C_BORED, M_PREFIX },
			{ "hobby", C_HOBBY, M_EXACT }, { "zainteresowan", C_HOBBY, M_PREFIX }, { "pasj", C_HOBBY, M_PREFIX },
			{ "wolnym czasie", C_HOBBY, M_PHRASE }, { "wolny czas", C_HOBBY, M_PHRASE }, { "lubisz robic", C_HOBBY, M_PHRASE },
			{ "jedzeni", C_FOOD, M_PREFIX }, { "jesc", C_FOOD, M_EXACT }, { "jadl", C_FOOD, M_PREFIX },
			{ "glodn", C_FOOD, M_PREFIX }, { "pizz", C_FOOD, M_PREFIX }, { "obiad", C_FOOD, M_PREFIX },
			{ "kolacj", C_FOOD, M_PREFIX }, { "sniadan", C_FOOD, M_PREFIX }, { "kebab", C_FOOD, M_PREFIX },
			{ "zup", C_FOOD, M_PREFIX }, { "jem", C_FOOD, M_EXACT }, { "zjadl", C_FOOD, M_PREFIX },
			{ "gotujesz", C_FOOD, M_EXACT }, { "gotowac", C_FOOD, M_EXACT }, { "pierog", C_FOOD, M_PREFIX },
			{ "schabow", C_FOOD, M_PREFIX }, { "burger", C_FOOD, M_PREFIX }, { "frytk", C_FOOD, M_PREFIX },
			{ "slodycz", C_FOOD, M_PREFIX }, { "czekolad", C_FOOD, M_PREFIX }, { "lody", C_FOOD, M_EXACT },
			{ "owoc", C_FOOD, M_PREFIX }, { "miesa", C_FOOD, M_EXACT }, { "miesko", C_FOOD, M_EXACT },
			{ "zarcie", C_FOOD, M_EXACT }, { "zarcia", C_FOOD, M_EXACT }, { "jedzenie", C_FOOD, M_EXACT },
			{ "kawa", C_DRINK, M_EXACT }, { "kawe", C_DRINK, M_EXACT }, { "kawy", C_DRINK, M_EXACT },
			{ "kawka", C_DRINK, M_EXACT }, { "herbat", C_DRINK, M_PREFIX }, { "piw", C_DRINK, M_PREFIX },
			{ "pic", C_DRINK, M_EXACT }, { "napoj", C_DRINK, M_PREFIX }, { "sok", C_DRINK, M_EXACT },
			{ "wakacj", C_TRAVELG, M_PREFIX }, { "zwiedz", C_TRAVELG, M_PREFIX }, { "wyjazd", C_TRAVELG, M_PREFIX },
			{ "wyjech", C_TRAVELG, M_PREFIX }, { "morze", C_TRAVELG, M_EXACT }, { "morza", C_TRAVELG, M_EXACT },
			{ "morzu", C_TRAVELG, M_EXACT }, { "morzem", C_TRAVELG, M_EXACT }, { "gory", C_TRAVELG, M_EXACT }, { "gorach", C_TRAVELG, M_EXACT },
			{ "za granice", C_TRAVELG, M_PHRASE }, { "swiat", C_TRAVELG, M_EXACT }, { "swiata", C_TRAVELG, M_EXACT },
			{ "swiecie", C_TRAVELG, M_EXACT }, { "kraj", C_TRAVELG, M_PREFIX }, { "pojech", C_TRAVELG, M_PREFIX },
			{ "urlop", C_TRAVELG, M_PREFIX }, { "plaz", C_TRAVELG, M_PREFIX }, { "podroze", C_TRAVELG, M_EXACT },
			{ "podrozowac", C_TRAVELG, M_EXACT }, { "podrozowanie", C_TRAVELG, M_EXACT },
			{ "muzyk", C_MUSIC, M_PREFIX }, { "piosenk", C_MUSIC, M_PREFIX }, { "sluch", C_MUSIC, M_PREFIX },
			{ "rap", C_MUSIC, M_EXACT }, { "rapu", C_MUSIC, M_EXACT }, { "rock", C_MUSIC, M_PREFIX },
			{ "metal", C_MUSIC, M_EXACT }, { "disco", C_MUSIC, M_EXACT }, { "spiew", C_MUSIC, M_PREFIX },
			{ "gitar", C_MUSIC, M_PREFIX }, { "techno", C_MUSIC, M_EXACT }, { "koncert", C_MUSIC, M_PREFIX },
			{ "film", C_MOVIE, M_PREFIX }, { "kino", C_MOVIE, M_EXACT }, { "kinie", C_MOVIE, M_EXACT },
			{ "kina", C_MOVIE, M_EXACT }, { "serial", C_MOVIE, M_PREFIX }, { "oglad", C_MOVIE, M_PREFIX },
			{ "netflix", C_MOVIE, M_PREFIX }, { "anime", C_MOVIE, M_EXACT }, { "bajk", C_MOVIE, M_PREFIX },
			{ "gier", C_GAMES, M_EXACT }, { "gry", C_GAMES, M_EXACT }, { "gierk", C_GAMES, M_PREFIX },
			{ "konsol", C_GAMES, M_PREFIX }, { "minecraft", C_GAMES, M_PREFIX }, { "fortnite", C_GAMES, M_PREFIX },
			{ "gta", C_GAMES, M_EXACT }, { "tibi", C_GAMES, M_PREFIX }, { "margonem", C_GAMES, M_PREFIX },
			{ "inne gry", C_GAMES, M_PHRASE }, { "w co grasz", C_GAMES, M_PHRASE }, { "gra", C_GAMES, M_EXACT },
			{ "kawal", C_HUMOR, M_EXACT }, { "kawaly", C_HUMOR, M_EXACT }, { "dowcip", C_HUMOR, M_PREFIX },
			{ "zart", C_HUMOR, M_PREFIX }, { "smieszn", C_HUMOR, M_PREFIX }, { "rozsmie", C_HUMOR, M_PREFIX },
			{ "zabawn", C_HUMOR, M_PREFIX },
			{ "przyjaci", C_FRIEND, M_PREFIX }, { "przyjazn", C_FRIEND, M_PREFIX }, { "kumpl", C_FRIEND, M_PREFIX },
			{ "kumpel", C_FRIEND, M_EXACT }, { "znajom", C_FRIEND, M_PREFIX }, { "koleg", C_FRIEND, M_PREFIX },
			{ "koledz", C_FRIEND, M_PREFIX },
			{ "wspolprac", C_TEAMWORK, M_PREFIX }, { "zgran", C_TEAMWORK, M_PREFIX },
			{ "samotn", C_LONELY, M_PREFIX },
			{ "ryzyk", C_RISK, M_PREFIX }, { "niebezpiecz", C_RISK, M_PREFIX },
			{ "prac", C_WORK, M_PREFIX }, { "robota", C_WORK, M_EXACT }, { "robocie", C_WORK, M_EXACT },
			{ "zawod", C_WORK, M_PREFIX }, { "etat", C_WORK, M_PREFIX },
			{ "szkol", C_SCHOOL, M_PREFIX }, { "nauk", C_SCHOOL, M_PREFIX }, { "uczysz", C_SCHOOL, M_EXACT },
			{ "ucze", C_SCHOOL, M_EXACT }, { "studi", C_SCHOOL, M_PREFIX }, { "egzamin", C_SCHOOL, M_PREFIX },
			{ "lekcj", C_SCHOOL, M_PREFIX }, { "matur", C_SCHOOL, M_PREFIX }, { "sesj", C_SCHOOL, M_PREFIX },
			{ "sens zycia", C_LIFEG, M_PHRASE }, { "zyjesz", C_LIFEG, M_EXACT }, { "filozof", C_LIFEG, M_PREFIX },
			{ "o zyciu", C_LIFEG, M_PHRASE },
			{ "smutn", C_SAD, M_PREFIX }, { "przygnebi", C_SAD, M_PREFIX }, { "dolek", C_SAD, M_EXACT },
			{ "zle mi", C_SAD, M_PHRASE }, { "zly dzien", C_SAD, M_PHRASE }, { "kiepsko", C_SAD, M_EXACT },
			{ "szczesliw", C_HAPPY, M_PREFIX }, { "wesol", C_HAPPY, M_PREFIX }, { "dobrze mi", C_HAPPY, M_PHRASE },
			{ "fajnie mi", C_HAPPY, M_PHRASE }, { "super dzien", C_HAPPY, M_PHRASE },
			{ "pies", C_ANIMAL, M_EXACT }, { "psa", C_ANIMAL, M_EXACT }, { "psy", C_ANIMAL, M_EXACT },
			{ "psem", C_ANIMAL, M_EXACT }, { "kot", C_ANIMAL, M_EXACT }, { "kota", C_ANIMAL, M_EXACT },
			{ "koty", C_ANIMAL, M_EXACT }, { "kotek", C_ANIMAL, M_EXACT }, { "kotem", C_ANIMAL, M_EXACT },
			{ "zwierz", C_ANIMAL, M_PREFIX }, { "chomik", C_ANIMAL, M_PREFIX },
			{ "sport", C_SPORT, M_PREFIX }, { "pilk", C_SPORT, M_PREFIX }, { "silown", C_SPORT, M_PREFIX },
			{ "biegan", C_SPORT, M_PREFIX }, { "mecz", C_SPORT, M_EXACT }, { "meczu", C_SPORT, M_EXACT },
			{ "futbol", C_SPORT, M_PREFIX }, { "rower", C_SPORT, M_PREFIX },
			{ "dziewczyn", C_LOVE, M_PREFIX }, { "chlopak", C_LOVE, M_PREFIX }, { "milos", C_LOVE, M_PREFIX },
			{ "randk", C_LOVE, M_PREFIX }, { "zakochan", C_LOVE, M_PREFIX },
			{ "ksiazk", C_BOOKS, M_PREFIX }, { "lektur", C_BOOKS, M_PREFIX }, { "czytasz ksiazki", C_BOOKS, M_PHRASE },
			{ "las", C_NATURE, M_EXACT }, { "lesie", C_NATURE, M_EXACT }, { "natur", C_NATURE, M_PREFIX },
			{ "przyrod", C_NATURE, M_PREFIX }, { "rzek", C_NATURE, M_PREFIX }, { "jezior", C_NATURE, M_PREFIX },
			{ "przygod", C_ADVENTURE, M_PREFIX },
			{ "kolor", C_COLOR, M_PREFIX },
			// ---- prices, item shop, shorthand
			{ "za ile", C_PRICEQ, M_PHRASE }, { "ile za", C_PRICEQ, M_PHRASE }, { "po ile", C_PRICEQ, M_PHRASE },
			{ "ile kosztuje", C_PRICEQ, M_PHRASE }, { "kosztuje", C_PRICEQ, M_EXACT }, { "ile chcesz", C_PRICEQ, M_PHRASE },
			{ "cena", C_PRICEQ, M_EXACT }, { "cene", C_PRICEQ, M_EXACT }, { "wycen", C_PRICEQ, M_PREFIX },
			{ "ile stoi", C_PRICEQ, M_PHRASE }, { "ile chodzi", C_PRICEQ, M_PHRASE }, { "ile warte", C_PRICEQ, M_PHRASE },
			{ "ile jest wart", C_PRICEQ, M_PHRASE },
			{ "itemshop", C_ITEMSHOP, M_EXACT }, { "item shop", C_ITEMSHOP, M_PHRASE }, { "smocze monety", C_ITEMSHOP, M_PHRASE },
			{ "smoczych monet", C_ITEMSHOP, M_PHRASE }, { "monet", C_ITEMSHOP, M_PREFIX }, { "smocz", C_ITEMSHOP, M_PREFIX },
			{ "ksujesz", C_KS, M_EXACT }, { "kradniesz", C_KS, M_FUZZY }, { "ukradles", C_KS, M_FUZZY },
			{ "moj mob", C_KS, M_PHRASE }, { "moje moby", C_KS, M_PHRASE }, { "mojego moba", C_KS, M_PHRASE },
			{ "gotowy", C_READY, M_EXACT }, { "gotow", C_READY, M_EXACT },
			{ "powodzenia", C_GOODLUCK, M_FUZZY }, { "udanego expa", C_GOODLUCK, M_PHRASE },
			{ "zaraz wracam", C_BRB, M_PHRASE }, { "zaraz bede", C_BRB, M_PHRASE }, { "chwila przerwy", C_BRB, M_PHRASE },
			{ "krytyk", C_BONUS, M_PREFIX }, { "przeszyw", C_BONUS, M_PREFIX }, { "obrazen", C_BONUS, M_PREFIX },
			{ "obron", C_BONUS, M_PREFIX }, { "abs", C_BONUS, M_EXACT }, { "nno", C_BONUS, M_EXACT },
			{ "ono", C_BONUS, M_EXACT }, { "nns", C_BONUS, M_EXACT }, { "bonus", C_BONUS, M_PREFIX },
			{ "odpornos", C_BONUS, M_PREFIX }, { "staty", C_BONUS, M_EXACT }, { "statystyk", C_BONUS, M_PREFIX },
			// "srednie" is the average-damage line; "srednio" (so-so) is not, so
			// these are exact.
			{ "srednie", C_BONUS, M_EXACT }, { "srednimi", C_BONUS, M_EXACT }, { "srednich", C_BONUS, M_EXACT },
			{ "srednia", C_BONUS, M_EXACT }, { "sredniej", C_BONUS, M_EXACT }, { "sredniaki", C_BONUS, M_EXACT },
			{ "sredniakow", C_BONUS, M_EXACT },
			{ "respawn", C_MOB, M_EXACT }, { "magazyn", C_TOWN, M_PREFIX }, { "polimorfi", C_SKILL, M_PREFIX },
			{ "rewanz", C_PVP, M_EXACT }, { "sprzedasz", C_BUYME, M_EXACT },
			{ "wystawiony", C_SHOP, M_EXACT }, { "wystawiles", C_SHOP, M_EXACT }, { "wystawila", C_SHOP, M_EXACT },
			// ---- a class's path, a Shaman's buffs. The path's own names (body,
			// mental, archer, BM, smok, heal ...) and the six buffs' names are
			// read by NamedBuildsMask / NamedBuffSkill below, which set C_BUILD
			// and C_BUFFNAME themselves: one list, so the concept and the answer
			// cannot disagree about what a word names.
			// In this game "profesja" is the path a trainer gives at level five
			// (the goal "wybrac profesje"), so it asks for the path as well as
			// the class; the build rule outranks the class rule.
			{ "build", C_BUILD, M_PREFIX }, { "specjaliz", C_BUILD, M_PREFIX }, { "sciezk", C_BUILD, M_PREFIX },
			{ "profes", C_BUILD, M_PREFIX }, { "doktryn", C_BUILD, M_PREFIX },
			{ "buff", C_BUFF, M_PREFIX }, { "buf", C_BUFF, M_EXACT }, { "bufy", C_BUFF, M_EXACT },
			{ "bufa", C_BUFF, M_EXACT }, { "bufow", C_BUFF, M_EXACT }, { "bufuj", C_BUFF, M_PREFIX },
			{ "zbuf", C_BUFF, M_PREFIX },
			// A give word counts only beside a buff or a path (the I_BUFFS rules):
			// "ile dasz za fms" is still a price.
			{ "daje", C_GIVE, M_EXACT }, { "dajesz", C_GIVE, M_EXACT }, { "daja", C_GIVE, M_EXACT },
			{ "dasz", C_GIVE, M_EXACT }, { "daj", C_GIVE, M_EXACT }, { "zwieksza", C_GIVE, M_EXACT },
			{ "podbija", C_GIVE, M_EXACT }, { "wzmacnia", C_GIVE, M_EXACT }, { "leczy", C_GIVE, M_EXACT },
			// ---- "chodz do mnie": come over. A bare "chodz" stays an invitation
			// to a party (C_JOIN); the imperatives are exact, because a prefix of
			// "przyjdz" is one typo away from "przejdz" and a portal.
			{ "chodz do mnie", C_SUMMON, M_PHRASE }, { "chodz tu", C_SUMMON, M_PHRASE },
			{ "chodz tutaj", C_SUMMON, M_PHRASE }, { "choc do mnie", C_SUMMON, M_PHRASE },
			{ "choc tu", C_SUMMON, M_PHRASE }, { "chodzze", C_SUMMON, M_EXACT },
			{ "przyjdz", C_SUMMON, M_EXACT }, { "przyjdzcie", C_SUMMON, M_EXACT }, { "przyjdziesz", C_SUMMON, M_EXACT },
			{ "przyjdzze", C_SUMMON, M_EXACT }, { "podejdz", C_SUMMON, M_EXACT }, { "podejdziesz", C_SUMMON, M_EXACT },
			{ "przybadz", C_SUMMON, M_EXACT }, { "wracaj do mnie", C_SUMMON, M_PHRASE },
			{ "wroc do mnie", C_SUMMON, M_PHRASE },
			// The infinitive only beside "do mnie": "moge przyjsc?" is the person
			// asking to come to the bot.
			{ "przyjsc do mnie", C_SUMMON, M_PHRASE }, { "do mnie przyjsc", C_SUMMON, M_PHRASE },
			{ "podejsc do mnie", C_SUMMON, M_PHRASE }, { "do mnie podejsc", C_SUMMON, M_PHRASE },
			// ---- and go back to your own life
			{ "mozesz isc", C_DISMISS, M_PHRASE }, { "mozesz juz isc", C_DISMISS, M_PHRASE },
			{ "mozesz odejsc", C_DISMISS, M_PHRASE }, { "mozesz wracac", C_DISMISS, M_PHRASE },
			{ "mozesz juz wracac", C_DISMISS, M_PHRASE }, { "wracaj do siebie", C_DISMISS, M_PHRASE },
			{ "wroc do siebie", C_DISMISS, M_PHRASE }, { "wracaj do swoich", C_DISMISS, M_PHRASE },
			{ "wracaj do swojego", C_DISMISS, M_PHRASE }, { "idz juz", C_DISMISS, M_PHRASE },
			{ "juz idz", C_DISMISS, M_PHRASE }, { "idz sobie", C_DISMISS, M_PHRASE },
			{ "zmykaj", C_DISMISS, M_EXACT }, { "odejdz", C_DISMISS, M_EXACT },
			{ "nie potrzebuje cie", C_DISMISS, M_PHRASE }, { "juz cie nie potrzebuje", C_DISMISS, M_PHRASE },
			// ---- a person talking at the bot: "przestan do mnie pisac" is not a
			// question about anything, and the words in it ("gold digger") are
			// not the subject. A bare "nie pisz" is left out: "nie pisz tak
			// szybko" asks for less, not for nothing.
			{ "przestan do mnie pisac", C_STOPTALK, M_PHRASE }, { "przestan mi pisac", C_STOPTALK, M_PHRASE },
			{ "przestan pisac", C_STOPTALK, M_PHRASE }, { "przestan do mnie gadac", C_STOPTALK, M_PHRASE },
			{ "przestan gadac", C_STOPTALK, M_PHRASE }, { "przestan spamowac", C_STOPTALK, M_PHRASE },
			{ "przestan mnie zaczepiac", C_STOPTALK, M_PHRASE }, { "nie pisz do mnie", C_STOPTALK, M_PHRASE },
			{ "nie pisz mi", C_STOPTALK, M_PHRASE }, { "nie pisz juz", C_STOPTALK, M_PHRASE },
			{ "nie pisz wiecej", C_STOPTALK, M_PHRASE }, { "nie gadaj do mnie", C_STOPTALK, M_PHRASE },
			{ "nie zaczepiaj", C_STOPTALK, M_PHRASE }, { "odczep sie", C_STOPTALK, M_PHRASE },
			{ "odwal sie", C_STOPTALK, M_PHRASE }, { "odwalcie sie", C_STOPTALK, M_PHRASE },
			{ "daj mi spokoj", C_STOPTALK, M_PHRASE }, { "zostaw mnie", C_STOPTALK, M_PHRASE },
			{ "nie spamuj", C_STOPTALK, M_PHRASE }, { "nie odzywaj sie", C_STOPTALK, M_PHRASE },
			{ "nie chce z toba gadac", C_STOPTALK, M_PHRASE }, { "nie chce z toba rozmawiac", C_STOPTALK, M_PHRASE },
			{ "nie mam ochoty gadac", C_STOPTALK, M_PHRASE }, { "koniec rozmowy", C_STOPTALK, M_PHRASE },
			{ "ban", C_THREAT, M_EXACT }, { "bana", C_THREAT, M_EXACT }, { "banem", C_THREAT, M_EXACT },
			{ "banik", C_THREAT, M_EXACT }, { "bany", C_THREAT, M_EXACT }, { "zbanow", C_THREAT, M_PREFIX },
			{ "zbanuj", C_THREAT, M_PREFIX }, { "banuj", C_THREAT, M_PREFIX }, { "zglosze", C_THREAT, M_EXACT },
			{ "zglaszam", C_THREAT, M_EXACT }, { "zglosic", C_THREAT, M_EXACT }, { "zgloszen", C_THREAT, M_PREFIX },
			{ "report", C_THREAT, M_PREFIX }, { "gmowi", C_THREAT, M_EXACT }, { "adminowi", C_THREAT, M_EXACT },
			{ "do gma", C_THREAT, M_PHRASE }, { "do gm", C_THREAT, M_PHRASE }, { "do admina", C_THREAT, M_PHRASE },
			{ "wyrzuce cie", C_THREAT, M_PHRASE }, { "dostaniesz bana", C_THREAT, M_PHRASE },
			// Mockery is banter, not abuse: "bieda", "zawijaj stad", "tyle jestes
			// warta", "daleko w zyciu zajdziesz". It is answered with a joke.
			{ "bied", C_MOCK, M_PREFIX }, { "zawijaj", C_MOCK, M_EXACT }, { "zawijajcie", C_MOCK, M_EXACT },
			{ "zawijaj stad", C_MOCK, M_PHRASE }, { "wypad stad", C_MOCK, M_PHRASE }, { "spadaj stad", C_MOCK, M_PHRASE },
			{ "slabiak", C_MOCK, M_EXACT }, { "slabiaku", C_MOCK, M_EXACT }, { "slabeusz", C_MOCK, M_EXACT },
			{ "cienki", C_MOCK, M_EXACT }, { "cienka", C_MOCK, M_EXACT }, { "cieniutki", C_MOCK, M_EXACT },
			{ "slaby jestes", C_MOCK, M_PHRASE }, { "slaba jestes", C_MOCK, M_PHRASE },
			{ "jestes slaby", C_MOCK, M_PHRASE }, { "jestes slaba", C_MOCK, M_PHRASE },
			{ "jestes cienki", C_MOCK, M_PHRASE }, { "jestes cienka", C_MOCK, M_PHRASE },
			{ "slaby z ciebie", C_MOCK, M_PHRASE }, { "slaba z ciebie", C_MOCK, M_PHRASE },
			{ "slabo grasz", C_MOCK, M_PHRASE }, { "grasz slabo", C_MOCK, M_PHRASE },
			{ "jestes wart", C_MOCK, M_PHRASE }, { "jestes warta", C_MOCK, M_PHRASE }, { "jestes warty", C_MOCK, M_PHRASE },
			{ "tyle wart", C_MOCK, M_PHRASE }, { "tyle warta", C_MOCK, M_PHRASE }, { "tyle jestes", C_MOCK, M_PHRASE },
			{ "daleko zajdziesz", C_MOCK, M_PHRASE }, { "w zyciu zajdziesz", C_MOCK, M_PHRASE },
			{ "zajdziesz daleko", C_MOCK, M_PHRASE }, { "daleko nie zajdziesz", C_MOCK, M_PHRASE },
			{ "zenada", C_MOCK, M_EXACT }, { "zenujace", C_MOCK, M_EXACT }, { "zenujacy", C_MOCK, M_EXACT },
			{ "smiech na sali", C_MOCK, M_PHRASE }, { "zal mi cie", C_MOCK, M_PHRASE },
			// Directed abuse, beside the old list.
			{ "spierd", C_INSULT, M_PREFIX }, { "wypierd", C_INSULT, M_PREFIX }, { "odpierd", C_INSULT, M_PREFIX },
			{ "cwel", C_INSULT, M_PREFIX }, { "ciota", C_INSULT, M_EXACT }, { "cioto", C_INSULT, M_EXACT },
			{ "szmat", C_INSULT, M_PREFIX }, { "gnoj", C_INSULT, M_PREFIX }, { "leszcz", C_INSULT, M_PREFIX },
			// "digger" exact, never a stem: one letter from "dagger", the Ninja's path.
			{ "smieciu", C_INSULT, M_EXACT }, { "digger", C_INSULT, M_EXACT }, { "diggerze", C_INSULT, M_EXACT },
			{ "diggerka", C_INSULT, M_EXACT }, { "diggerko", C_INSULT, M_EXACT }, { "golddigg", C_INSULT, M_PREFIX },
			{ "gold digger", C_INSULT, M_PHRASE }, { "glupek", C_INSULT, M_EXACT }, { "glupku", C_INSULT, M_EXACT },
			{ "dzban", C_INSULT, M_PREFIX }, { "matol", C_INSULT, M_PREFIX }, { "palant", C_INSULT, M_PREFIX },
			{ "kmiot", C_INSULT, M_PREFIX },
			// Swearing is how people talk, not a line about the bot; only aimed at
			// the bot (the I_INSULT rule with C_YOU) is it abuse.
			{ "kurw", C_SWEAR, M_PREFIX }, { "kurde", C_SWEAR, M_EXACT }, { "choler", C_SWEAR, M_PREFIX },
			{ "huj", C_SWEAR, M_PREFIX }, { "chuj", C_SWEAR, M_PREFIX }, { "gown", C_SWEAR, M_PREFIX },
			{ "pierdol", C_SWEAR, M_PREFIX }, { "pizd", C_SWEAR, M_PREFIX }, { "jeba", C_SWEAR, M_PREFIX },
			{ "jebn", C_SWEAR, M_PREFIX }, { "jebie", C_SWEAR, M_EXACT },
			// ---- the bot's gear argued about
			{ "zoba", C_SHOWOFF, M_EXACT }, { "zobacz", C_SHOWOFF, M_EXACT }, { "zobaczcie", C_SHOWOFF, M_EXACT },
			{ "patrz", C_SHOWOFF, M_EXACT }, { "popatrz", C_SHOWOFF, M_EXACT }, { "patrzcie", C_SHOWOFF, M_EXACT },
			{ "zerknij", C_SHOWOFF, M_EXACT }, { "spojrz", C_SHOWOFF, M_EXACT }, { "podziwiaj", C_SHOWOFF, M_EXACT },
			{ "pochwale", C_SHOWOFF, M_EXACT }, { "pochwalic", C_SHOWOFF, M_EXACT }, { "chwale", C_SHOWOFF, M_EXACT },
			{ "wymien", C_SWAP, M_PREFIX }, { "zamien", C_SWAP, M_PREFIX }, { "zmien", C_SWAP, M_PREFIX },
			{ "zaloz", C_SWAP, M_PREFIX }, { "ubierz", C_SWAP, M_PREFIX },
			{ "powinien", C_ADVICE, M_PREFIX }, { "powinn", C_ADVICE, M_PREFIX }, { "lepiej", C_ADVICE, M_EXACT },
			{ "lepsz", C_ADVICE, M_PREFIX }, { "najlepsz", C_ADVICE, M_PREFIX }, { "wyzsz", C_ADVICE, M_PREFIX },
			{ "przydal", C_ADVICE, M_PREFIX }, { "radze", C_ADVICE, M_EXACT }, { "polecam", C_ADVICE, M_EXACT },
			{ "polecalbym", C_ADVICE, M_EXACT }, { "potrzebujesz", C_ADVICE, M_EXACT }, { "musisz", C_ADVICE, M_EXACT },
			{ "kup sobie", C_ADVICE, M_PHRASE }, { "kupisz sobie", C_ADVICE, M_PHRASE }, { "kupic sobie", C_ADVICE, M_PHRASE },
			{ "kupilbys sobie", C_ADVICE, M_PHRASE }, { "wez sobie", C_ADVICE, M_PHRASE },
			{ "potrzebne ci", C_ADVICE, M_PHRASE }, { "potrzebny ci", C_ADVICE, M_PHRASE },
			{ "potrzebna ci", C_ADVICE, M_PHRASE }, { "nie lepiej", C_ADVICE, M_PHRASE },
			{ "dam ci", C_GIFT, M_PHRASE }, { "ci dam", C_GIFT, M_PHRASE }, { "dac ci", C_GIFT, M_PHRASE },
			{ "ci dac", C_GIFT, M_PHRASE }, { "dal ci", C_GIFT, M_PHRASE }, { "ci dal", C_GIFT, M_PHRASE },
			{ "ci dala", C_GIFT, M_PHRASE }, { "dala ci", C_GIFT, M_PHRASE }, { "za darmo", C_GIFT, M_PHRASE },
			{ "za free", C_GIFT, M_PHRASE }, { "w prezencie", C_GIFT, M_PHRASE }, { "dalbym", C_GIFT, M_EXACT },
			{ "dalabym", C_GIFT, M_EXACT }, { "dostalbys", C_GIFT, M_EXACT }, { "dostalabys", C_GIFT, M_EXACT },
			{ "podaruje", C_GIFT, M_EXACT }, { "podarowac", C_GIFT, M_EXACT }, { "gratis", C_GIFT, M_EXACT },
			{ "prezent", C_GIFT, M_PREFIX },
			{ "powiedziales", C_SAIDBEFORE, M_EXACT }, { "powiedzialas", C_SAIDBEFORE, M_EXACT },
			{ "mowiles", C_SAIDBEFORE, M_EXACT }, { "mowilas", C_SAIDBEFORE, M_EXACT }, { "pisales", C_SAIDBEFORE, M_EXACT },
			{ "pisalas", C_SAIDBEFORE, M_EXACT }, { "twierdziles", C_SAIDBEFORE, M_EXACT },
			{ "twierdzilas", C_SAIDBEFORE, M_EXACT }, { "klamiesz", C_SAIDBEFORE, M_EXACT }, { "klamca", C_SAIDBEFORE, M_EXACT },
			{ "klamczuch", C_SAIDBEFORE, M_EXACT }, { "sciemniasz", C_SAIDBEFORE, M_EXACT },
			{ "kitujesz", C_SAIDBEFORE, M_EXACT }, { "nieprawda", C_SAIDBEFORE, M_EXACT },
			{ "nie prawda", C_SAIDBEFORE, M_PHRASE },
			// ---- sentiment of a statement
			{ "super", C_POSITIVE, M_EXACT }, { "fajnie", C_POSITIVE, M_EXACT }, { "ekstra", C_POSITIVE, M_EXACT },
			{ "wbilem", C_POSITIVE, M_EXACT }, { "dropnalem", C_POSITIVE, M_EXACT }, { "dropnelo", C_POSITIVE, M_EXACT },
			{ "udalo mi", C_POSITIVE, M_PHRASE }, { "zdobylem", C_POSITIVE, M_EXACT }, { "wygralem", C_POSITIVE, M_EXACT },
			{ "ulepszylem", C_POSITIVE, M_EXACT }, { "mam nowy", C_POSITIVE, M_PHRASE }, { "mam nowa", C_POSITIVE, M_PHRASE },
			{ "zajebi", C_POSITIVE, M_PREFIX }, { "swietnie", C_POSITIVE, M_EXACT }, { "niezle", C_POSITIVE, M_EXACT },
			{ "zginalem", C_NEGATIVE, M_EXACT }, { "umarlem", C_NEGATIVE, M_EXACT }, { "stracilem", C_NEGATIVE, M_EXACT },
			{ "spalilo", C_NEGATIVE, M_EXACT }, { "spalil", C_NEGATIVE, M_PREFIX }, { "przegralem", C_NEGATIVE, M_EXACT },
			{ "slabo", C_NEGATIVE, M_EXACT }, { "masakra", C_NEGATIVE, M_EXACT }, { "szkoda", C_NEGATIVE, M_EXACT },
			{ "zle", C_NEGATIVE, M_EXACT }, { "beznadzie", C_NEGATIVE, M_PREFIX }, { "okradli", C_NEGATIVE, M_EXACT },
		};
		count = sizeof(kLex) / sizeof(kLex[0]);
		return kLex;
	}

	// Real words that sit one letter away from a stem of another meaning.
	inline bool IsNoFuzzyWord(const std::string& w)
	{
		static const char* const kWords[] = {
			"pojecia", "pojecie", "pojeciem", "przygotuj", "przygotowac", "przygotowuje",
			"szczesliwy", "szczesliwa", "szczesliwie", "zadasz", "robota", "robocie"
		};
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (w == kWords[i])
				return true;
		return false;
	}

	inline bool LexWordMatches(const std::string& w, const TLexEntry& e)
	{
		switch (e.mode)
		{
			case M_EXACT:
				return w == e.text;
			case M_FUZZY:
				return w == e.text || FuzzyEquals(w, e.text);
			case M_PREFIX:
			{
				if (StartsWith(w, e.text))
					return true;
				const size_t ls = strlen(e.text);
				// One typo inside a long stem: "porabaisz" still starts like "porabia".
				// Six letters or more - "wiosn" one letter off "wiosk" is a village.
				if (ls >= 6 && w.size() >= ls && w[0] == e.text[0] && !IsNoFuzzyWord(w))
					return EditDistance(w.c_str(), ls, e.text, ls, 1) <= 1;
				return false;
			}
			case M_SUFFIX:
			{
				const size_t ls = strlen(e.text);
				return w.size() >= 5 && w.size() > ls && w.compare(w.size() - ls, ls, e.text) == 0;
			}
			default:
				return false;
		}
	}

	// Words of `phrase` found in `words` starting at `at`.
	inline bool PhraseAt(const std::vector<std::string>& words, size_t at, const char* phrase, size_t& len)
	{
		std::vector<std::string> pw;
		SplitWords(phrase, pw);
		len = pw.size();
		if (pw.empty() || at + pw.size() > words.size())
			return false;
		for (size_t i = 0; i < pw.size(); ++i)
			if (words[at + i] != pw[i])
				return false;
		return true;
	}

	// ------------------------------------------------------ a class's path

	// The two paths of each class, in the order (job, skill group) gives them,
	// so BuildOf is arithmetic and nothing has to keep two tables in step.
	enum EBuild
	{
		B_NONE = 0,
		B_BODY, B_MENTAL,         // warrior: skill group 1, 2
		B_DAGGER, B_ARCHER,       // ninja
		B_WEAPON, B_BLACK_MAGIC,  // sura
		B_DRAGON, B_HEAL,         // shaman
		B_COUNT
	};

	typedef char TBuildMaskFits[B_COUNT <= 32 ? 1 : -1];

	inline int BuildOf(int job, int group)
	{
		if (job < 0 || job > 3 || group < 1 || group > 2)
			return B_NONE;
		return 1 + job * 2 + (group - 1);
	}

	inline int BuildJob(int build)
	{
		return build > B_NONE && build < B_COUNT ? (build - 1) / 2 : -1;
	}

	// The words players use for a path. "heal" and "leczenie" are also the
	// name of the Shaman's Cure; which one a line means is decided in
	// ExtractConcepts, where the rest of the line is known.
	inline int BuildOfWord(const std::string& w)
	{
		static const struct { const char* word; unsigned char build; } kWords[] = {
			{ "body", B_BODY }, { "bodziak", B_BODY }, { "bodziaka", B_BODY }, { "bodziakiem", B_BODY },
			{ "cialo", B_BODY }, { "ciala", B_BODY }, { "cialem", B_BODY }, { "ciele", B_BODY },
			{ "mental", B_MENTAL }, { "mentala", B_MENTAL }, { "mentalem", B_MENTAL }, { "mentalny", B_MENTAL },
			{ "mentalnym", B_MENTAL }, { "umysl", B_MENTAL }, { "umyslu", B_MENTAL }, { "umyslem", B_MENTAL },
			{ "dagger", B_DAGGER }, { "daggera", B_DAGGER }, { "daggerem", B_DAGGER }, { "sztyletach", B_DAGGER },
			{ "sztyleciarz", B_DAGGER }, { "sztyleciarzem", B_DAGGER }, { "skrytobojca", B_DAGGER },
			{ "skrytobojcy", B_DAGGER }, { "asasyn", B_DAGGER }, { "asasynem", B_DAGGER },
			{ "assassin", B_DAGGER },
			{ "archer", B_ARCHER }, { "archera", B_ARCHER }, { "archerem", B_ARCHER }, { "archerka", B_ARCHER },
			{ "lucznik", B_ARCHER }, { "lucznika", B_ARCHER }, { "lucznikiem", B_ARCHER }, { "luczniczka", B_ARCHER },
			{ "wp", B_WEAPON }, { "weapon", B_WEAPON }, { "weapona", B_WEAPON }, { "weaponem", B_WEAPON },
			{ "bm", B_BLACK_MAGIC }, { "bmem", B_BLACK_MAGIC }, { "bmka", B_BLACK_MAGIC },
			{ "smok", B_DRAGON }, { "smoka", B_DRAGON }, { "smokiem", B_DRAGON }, { "dragon", B_DRAGON },
			{ "dragona", B_DRAGON },
			{ "heal", B_HEAL }, { "heala", B_HEAL }, { "healem", B_HEAL }, { "healer", B_HEAL },
			{ "healera", B_HEAL }, { "healerem", B_HEAL }, { "healerka", B_HEAL }, { "hil", B_HEAL },
			{ "hila", B_HEAL }, { "hilem", B_HEAL }, { "leczacy", B_HEAL }, { "leczaca", B_HEAL },
			{ "leczacym", B_HEAL }, { "leczenie", B_HEAL }, { "leczeniem", B_HEAL }
		};
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (w == kWords[i].word)
				return kWords[i].build;
		return B_NONE;
	}

	// The words that name the Cure and the Healing path at once.
	inline bool IsHealWord(const std::string& w)
	{
		return w == "heal" || w == "heala" || w == "healem" || w == "hil" || w == "hila" || w == "hilem" ||
				w == "leczenie" || w == "leczeniem" || w == "leczenia";
	}

	// Every path the line names, as a mask of 1 << EBuild, and the first word
	// that named one. Three phrases are not a path: "pomoc smoka" is the
	// Dragon's Aid, "silne cialo" a Mental warrior's skill, and "czarna magia"
	// is two words for the one path.
	inline unsigned int NamedBuildsMask(const TTokens& tok, int& firstWord)
	{
		unsigned int mask = 0;
		firstWord = -1;
		const std::vector<std::string>& w = tok.words;
		for (size_t i = 0; i < w.size(); ++i)
		{
			int build = BuildOfWord(w[i]);
			if (build == B_DRAGON && i > 0 && (w[i - 1] == "pomoc" || w[i - 1] == "pomocy" || w[i - 1] == "pomoca"))
				build = B_NONE;
			if (build == B_BODY && i > 0 && StartsWith(w[i - 1], "siln"))
				build = B_NONE;
			if (build == B_NONE && StartsWith(w[i], "czarn") && i + 1 < w.size() && StartsWith(w[i + 1], "magi"))
				build = B_BLACK_MAGIC;
			if (build == B_NONE && StartsWith(w[i], "magiczn") && i + 1 < w.size() && StartsWith(w[i + 1], "bron"))
				build = B_WEAPON;
			if (build == B_NONE)
				continue;
			mask |= 1u << build;
			if (firstWord < 0)
				firstWord = (int)i;
		}
		return mask;
	}

	// The Shaman buff the line names by its own name, as the skill vnum, or 0.
	// "Zwoj blogoslawienstwa" is a refine scroll and not the buff.
	inline unsigned int NamedBuffSkill(const TTokens& tok, int& word)
	{
		word = -1;
		const std::vector<std::string>& w = tok.words;
		for (size_t i = 0; i < w.size(); ++i)
		{
			const std::string& x = w[i];
			const std::string next = i + 1 < w.size() ? w[i + 1] : std::string();
			unsigned int skill = 0;
			if ((StartsWith(x, "blogoslawienstw") || x == "bless" || x == "blessa" || x == "blessing" || x == "hosin") &&
					!(i > 0 && StartsWith(w[i - 1], "zwoj")))
				skill = 94;
			else if (StartsWith(x, "odbici") || x == "reflect" || x == "reflecta" || x == "boho")
				skill = 95;
			else if ((x == "pomoc" || x == "pomocy" || x == "pomoca") && (StartsWith(next, "smok") || StartsWith(next, "smocz")))
				skill = 96;
			else if (x == "gicheon")
				skill = 96;
			else if (IsHealWord(x) || x == "cure" || x == "uzdrawianie" || x == "leczysz")
				skill = 109;
			else if (StartsWith(x, "zwinnos") || x == "swiftness" || x == "swift" || x == "kwaesok")
				skill = 110;
			else if (StartsWith(x, "zwiekszen") && StartsWith(next, "atak"))
				skill = 111;
			else if ((x == "attack" || x == "atak" || x == "atack") && next == "up")
				skill = 111;
			else if (x == "jeungryeok")
				skill = 111;
			if (skill)
			{
				word = (int)i;
				return skill;
			}
		}
		return 0;
	}

	inline void ExtractConcepts(const TTokens& tok, TConceptSet& out)
	{
		out.Clear();
		size_t count = 0;
		const TLexEntry* lex = GetLexicon(count);
		const std::string padded = " " + tok.norm + " ";
		std::string needle;
		for (size_t e = 0; e < count; ++e)
		{
			const TLexEntry& entry = lex[e];
			if (entry.mode == M_PHRASE)
			{
				// Word boundaries on the normalized line: " co tam " in " no co tam robisz ".
				needle.assign(" ");
				needle += entry.text;
				needle += ' ';
				const size_t pos = padded.find(needle);
				if (pos != std::string::npos)
				{
					int word = 0;
					for (size_t k = 1; k <= pos && k < padded.size(); ++k)
						if (padded[k] == ' ')
							++word;
					out.Set(entry.conceptId, word);
				}
				continue;
			}
			for (size_t i = 0; i < tok.words.size(); ++i)
			{
				if (LexWordMatches(tok.words[i], entry))
				{
					out.Set(entry.conceptId, (int)i);
					break;
				}
			}
		}

		// The players' names for items and places (playerbot_conv_aliases.h).
		for (size_t i = 0; i < tok.words.size(); ++i)
			if (IsItemAliasWord(tok.words[i]))
			{
				out.Set(C_ITEMWORD, (int)i);
				break;
			}
		{
			size_t at = 0;
			if (FindMapAlias(tok.words, at) != 0)
				out.Set(C_MAPNAME, (int)at);
		}

		// Collisions the prefix stems cannot avoid on their own.
		// "lowisz" is fishing, "lowca" is not a question about fishing; "dolina"
		// is a map, "zalowac" is regret. A bare "low..." shorter than 5 letters
		// is left alone as well.
		if (out.Has(C_FISH))
		{
			bool real = false;
			for (size_t i = 0; i < tok.words.size(); ++i)
			{
				const std::string& w = tok.words[i];
				if (StartsWith(w, "lowi") || StartsWith(w, "lowie") || StartsWith(w, "ryb") ||
						StartsWith(w, "wedk") || w == "low" || StartsWith(w, "lowic"))
					real = true;
			}
			if (!real)
				out.Unset(C_FISH);
		}
		// "rud" catches "rudy" (a redhead) too; a miner says ruda/rude/rudy/rudzie.
		// Keep it - the game meaning is the common one in a bot whisper.
		// "zimno" is a temperature; "zima" a season. Both are weather.
		if (out.Has(C_COLD) || out.Has(C_WARM) || out.Has(C_RAIN))
			out.Set(C_WEATHER, out.firstWord[C_COLD] >= 0 ? out.firstWord[C_COLD] :
					(out.firstWord[C_WARM] >= 0 ? out.firstWord[C_WARM] : out.firstWord[C_RAIN]));
		// "nie lubisz" also contains "lubisz".
		if (out.Has(C_DISLIKE))
			out.Unset(C_LIKE);
		// "jak tam" and friends contain "jak" and "tam"; they are one greeting.
		if (out.Has(C_HOWAREYOU))
		{
			if (tok.words.size() <= 4)
			{
				out.Unset(C_HOW);
				out.Unset(C_THERE);
			}
		}
		// "co z koniem": "co" is part of the pattern, not a question on its own.
		if (out.Has(C_WHATWITH))
			out.Set(C_WHAT, out.firstWord[C_WHATWITH]);
		// "z kim" - "kim" is not a class question.
		if (out.Has(C_WITHWHO))
			out.Unset(C_WHO);
		// "z toba" is company, not "you" as the subject of the question.
		// (C_YOU is still set by "toba" itself - that is fine.)
		// "sam" in "sam nie wiem" is not about being alone.
		if (out.Has(C_ALONE) && tok.norm.find("sam nie wiem") != std::string::npos)
			out.Unset(C_ALONE);
		// "pomoz mi" is a request, "pomocy" is a help call.
		// "gra" as the only games word inside "jak gra" etc. is too weak.
		if (out.Has(C_GAMES) && tok.Has("gra") && !tok.Has("gry") && !tok.Has("gier") &&
				!out.Has(C_LIKE) && !out.Has(C_FAV))
			out.Unset(C_GAMES);
		// "co tam" is a greeting even with "co".
		if (out.Has(C_HOWAREYOU) && tok.words.size() <= 3)
			out.Unset(C_WHAT);

		// Words that name something inside an insult or an offer and are not
		// the subject: "gold digger" is no question about yang, "kup sobie X"
		// is advice and not an offer to buy X from the bot, "czemu nie kupisz
		// X?" asks why the bot does not buy it, not whether it buys from the
		// person.
		for (size_t i = 0; i < tok.words.size(); ++i)
		{
			const std::string& w = tok.words[i];
			if (StartsWith(w, "digger") || StartsWith(w, "golddigg"))
				out.Unset(C_GOLD);
			if (StartsWith(w, "kup") && i + 1 < tok.words.size() && tok.words[i + 1] == "sobie")
			{
				out.Unset(C_BUYME);
				out.Unset(C_SELLYOU);
			}
		}
		if (out.Has(C_WHY) && tok.Has("kupisz") && !tok.Has("ode") && !tok.Has("odemnie"))
			out.Unset(C_SELLYOU);
		// "zbic konia" in a line that says what the other one may do - "teraz
		// to najwyzej mozesz mi zbic konia" - is a jibe, not a question about
		// the bot's horse. A horse word is a question only beside a question
		// word or the bot.
		if (out.Has(C_HORSE) && out.Has(C_HIT) && !out.Has(C_YOU) && !out.Has(C_WHAT) &&
				!out.Has(C_HOWMUCH) && !out.Has(C_WHICH) && !out.Has(C_HAVE) && !out.Has(C_LEVEL))
			out.Unset(C_HORSE);

		// A path named by its own word ("body", "archerem", "smok") and a buff
		// named by its own ("odbicie", "pomoc smoka"). "heal" and "leczenie"
		// name both the Healing path and the Cure: beside a give or a buff word
		// or "ile" the line is about what the Cure does ("ile leczy twoj
		// heal?"), otherwise about the path ("jestes heal czy smok?").
		int buildWord = -1;
		unsigned int builds = NamedBuildsMask(tok, buildWord);
		int buffWord = -1;
		const unsigned int buff = NamedBuffSkill(tok, buffWord);
		if (buff == 109 && buffWord >= 0 && IsHealWord(tok.words[buffWord]))
		{
			if (out.Has(C_GIVE) || out.Has(C_BUFF) || out.Has(C_HOWMUCH))
				builds &= ~(1u << B_HEAL);
			else
				buffWord = -1;
		}
		// A path word alone is a weak signal - "bijesz smoka?" is a monster -
		// so it counts beside a question about the bot, a class, a choice or
		// a buff, or as a line of one or two words ("archer?").
		if (builds && (out.Has(C_BE) || out.Has(C_PLAY) || out.Has(C_OR) || out.Has(C_CLASS) ||
				out.Has(C_WHICH) || out.Has(C_YOU) || out.Has(C_HAVE) || out.Has(C_GIVE) || out.Has(C_BUFF) ||
				out.Has(C_BUILD) || tok.words.size() <= 2))
			out.Set(C_BUILD, buildWord);
		if (buff && buffWord >= 0)
			out.Set(C_BUFFNAME, buffWord);
	}

	// The paths a line names, filtered the way ExtractConcepts filters them,
	// for the generator's yes/no ("grasz archerem?" - "Nie, gram ...").
	inline unsigned int NamedBuildsInLine(const TTokens& tok, const TConceptSet& c)
	{
		if (!c.Has(C_BUILD))
			return 0;
		int first = -1;
		unsigned int builds = NamedBuildsMask(tok, first);
		int buffWord = -1;
		if (NamedBuffSkill(tok, buffWord) == 109 && buffWord >= 0 && IsHealWord(tok.words[buffWord]) &&
				(c.Has(C_GIVE) || c.Has(C_BUFF) || c.Has(C_HOWMUCH)))
			builds &= ~(1u << B_HEAL);
		return builds;
	}
}

#endif
