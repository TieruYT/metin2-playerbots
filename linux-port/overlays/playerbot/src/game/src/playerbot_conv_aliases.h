#ifndef __INC_METIN2_PLAYERBOT_CONV_ALIASES_H__
#define __INC_METIN2_PLAYERBOT_CONV_ALIASES_H__

// PlayerBot Conversation v6 - the players' own words for things (pure).
//
// Three dictionaries a Metin2 player takes for granted:
//   - ITEMS: "FMS" is Miecz Pelni Ksiezyca, "12D"/"duszki" Miecz Dwunastu
//     Duchow, "bodzio" Zwoj Blogoslawienstwa, "ebo" Ebonitowe Kolczyki ...
//     ItemNameMatches() tries the query itself and every expansion, so the
//     stall, the bag, the shout market and the price questions all understand
//     them the same way,
//   - PLACES: M1/M2/M3 (per kingdom), V1/V2, DT, Sohan, Red Las, AV ...,
//   - MONEY: 500k, 2kk, 1.5kk, 1kkk, "2 kk", "300 tys".
// Chat shorthand (gz, gl, brb, np, nmzc, cya, btw, rdy ...) is in the text
// layer's RewriteWord() and the lexicon, because it changes what a line MEANS
// rather than what it names.
//
// Everything is folded ASCII (see Normalize / FoldName). Adding an alias is one
// line. Several aliases may share an expansion; one alias may have several
// (KD = Kamien Duszy or Kamien Duchowy, depending on the server's proto).

#include "playerbot_conv_text.h"

namespace playerbot_conv
{
	struct TItemAlias
	{
		const char* alias;   // what players type, folded
		const char* name;    // words of the proto name, folded
	};

	inline const TItemAlias* GetItemAliases(size_t& count)
	{
		static const TItemAlias kAliases[] = {
			// weapons
			{ "fms", "miecz pelni ksiezyca" }, { "pelnia", "miecz pelni ksiezyca" },
			{ "rib", "ostrze czerwonej stali" }, { "hms", "polksiezycowy miecz" },
			{ "12d", "miecz dwunastu duchow" }, { "duszki", "miecz dwunastu duchow" },
			{ "duszek", "miecz dwunastu duchow" }, { "12stka", "miecz dwunastu duchow" },
			{ "nimfa", "miecz nimfy" }, { "barba", "miecz barbarzyncy" }, { "barbarzynca", "miecz barbarzyncy" },
			{ "szpon", "miecz szponu ducha" }, { "parta", "partyzana" }, { "halka", "halabarda" },
			{ "kruk", "stalowy luk kruka" }, { "jelonek", "luk rogu jelenia" },
			{ "koziki", "kozik czarnego liscia" }, { "kozik", "kozik czarnego liscia" },
			{ "kozy", "kozik czarnego liscia" }, { "antyk", "antyczny dzwon" },
			{ "jesion", "wachlarz jesiennego wiatru" }, { "jesionek", "wachlarz jesiennego wiatru" },
			{ "morela", "boski luk moreli" }, { "morelek", "morelowy dzwon" },
			{ "gitara", "ostrze zbawienia" }, { "wioslo", "ostrze zbawienia" }, { "lopata", "ostrze zbawienia" },
			{ "magnetyk", "magnetyczne ostrze" }, { "magneto", "magnetyczne ostrze" },
			// books, stones, refining
			{ "ku", "instr" }, { "ku", "ksiega umiejetnosci" },
			{ "kd", "kamien duszy" }, { "kd", "kamien duchowy" },
			{ "km", "ksiega misji" }, { "oz", "opaska zapomnienia" },
			{ "bodzio", "zwoj blogoslawienstwa" }, { "bodzia", "zwoj blogoslawienstwa" },
			{ "bodzie", "zwoj blogoslawienstwa" }, { "bodki", "zwoj blogoslawienstwa" },
			{ "bogdan", "zwoj blogoslawienstwa" }, { "bogdana", "zwoj blogoslawienstwa" },
			{ "perla", "perla" }, { "perly", "perla" }, { "perle", "perla" },
			{ "kk", "kawalek klejnotu" }, { "klejnot", "kawalek klejnotu" },
			{ "przetop", "przetop" }, { "przetopka", "przetop" },
			// ring, potions, jewellery
			{ "pd", "pierscien doswiadczenia" }, { "expring", "pierscien doswiadczenia" },
			{ "exp ring", "pierscien doswiadczenia" }, { "pierscien expa", "pierscien doswiadczenia" },
			{ "poty", "mikstura" }, { "potki", "mikstura" }, { "potke", "mikstura" }, { "potek", "mikstura" },
			{ "potka", "mikstura" }, { "poty", "mikstur" },
			{ "ebo", "ebonitowe kolczyki" }, { "ebonitki", "ebonitowe kolczyki" }, { "ebonity", "ebonitowe kolczyki" },
			{ "bransa", "bransoleta" }, { "bransy", "bransoleta" }, { "bransoletka", "bransoleta" },
			{ "kolce", "kolczyki" }, { "kolczyk", "kolczyki" },
			{ "naszyjka", "naszyjnik" }, { "naszyjnik", "naszyjnik" },
		};
		count = sizeof(kAliases) / sizeof(kAliases[0]);
		return kAliases;
	}

	// "fmsa", "fmsem", "halke", "duszkow" - an alias with a Polish ending on it.
	inline bool AliasWordMatches(const std::string& word, const char* alias)
	{
		const size_t la = strlen(alias);
		if (word == alias)
			return true;
		if (la >= 3 && word.size() > la && word.size() <= la + 3 && word.compare(0, la, alias) == 0)
			return true;
		// "halke" for "halka": the alias less its final vowel, plus an ending.
		if (la >= 5)
		{
			const char last = alias[la - 1];
			if ((last == 'a' || last == 'e' || last == 'i' || last == 'o' || last == 'y') &&
					word.size() >= la && word.size() <= la + 2 && word.compare(0, la - 1, alias, la - 1) == 0)
				return true;
		}
		return false;
	}

	inline bool IsItemAliasWord(const std::string& word)
	{
		size_t n = 0;
		const TItemAlias* al = GetItemAliases(n);
		for (size_t i = 0; i < n; ++i)
			if (!strchr(al[i].alias, ' ') && AliasWordMatches(word, al[i].alias))
				return true;
		return false;
	}

	// The query itself first, then the query with each alias replaced by its
	// expansion: "fms +9" -> { "fms +9", "miecz pelni ksiezyca +9" }.
	inline void ExpandItemQuery(const std::string& query, std::vector<std::string>& out)
	{
		out.clear();
		std::vector<std::string> words;
		SplitWords(query, words);
		size_t n = 0;
		const TItemAlias* al = GetItemAliases(n);
		// A word that IS an alias ("morelek") takes only exact matches, so it
		// is not also read as another alias with an ending ("morela" + "k"),
		// and then the query is not matched as plain words either ("morelek"
		// would otherwise find "Moreli").
		std::vector<bool> exact(words.size(), false);
		bool anyExact = false;
		for (size_t i = 0; i < words.size(); ++i)
			for (size_t a = 0; a < n; ++a)
				if (words[i] == al[a].alias)
					exact[i] = anyExact = true;
		if (!anyExact)
			out.push_back(query);
		else
		{
			// The line as typed still names what it names - "szpon wilka" is
			// Szpon Wilka, not only Miecz Szponu Ducha - but its alias words
			// count only as whole words ('='), so "morelek" still misses Moreli.
			std::string marked;
			for (size_t i = 0; i < words.size(); ++i)
			{
				if (!marked.empty())
					marked += ' ';
				if (exact[i])
					marked += '=';
				marked += words[i];
			}
			out.push_back(marked);
		}
		for (size_t a = 0; a < n; ++a)
		{
			std::vector<std::string> aw;
			SplitWords(al[a].alias, aw);
			for (size_t i = 0; i + aw.size() <= words.size(); ++i)
			{
				bool hit = true;
				for (size_t k = 0; k < aw.size() && hit; ++k)
					hit = aw.size() == 1
						? (exact[i] ? words[i] == aw[0] : AliasWordMatches(words[i], aw[0].c_str()))
						: words[i + k] == aw[k];
				if (!hit)
					continue;
				std::string expanded;
				for (size_t k = 0; k < words.size(); ++k)
				{
					if (k == i)
					{
						if (!expanded.empty()) expanded += ' ';
						expanded += al[a].name;
						k += aw.size() - 1;
						continue;
					}
					if (!expanded.empty()) expanded += ' ';
					expanded += words[k];
				}
				bool seen = false;
				for (size_t k = 0; k < out.size(); ++k)
					if (out[k] == expanded)
						seen = true;
				if (!seen && out.size() < 8)
					out.push_back(expanded);
				break;
			}
		}
	}

	// `word` in `foldedName` with no letter or digit either side of it.
	inline bool HasWholeWord(const std::string& foldedName, const std::string& word)
	{
		if (word.empty())
			return false;
		for (size_t pos = foldedName.find(word); pos != std::string::npos; pos = foldedName.find(word, pos + 1))
		{
			const size_t end = pos + word.size();
			const char before = pos > 0 ? foldedName[pos - 1] : ' ';
			const char after = end < foldedName.size() ? foldedName[end] : ' ';
			const bool wordBefore = (before >= 'a' && before <= 'z') || (before >= '0' && before <= '9');
			const bool wordAfter = (after >= 'a' && after <= 'z') || (after >= '0' && after <= '9');
			if (!wordBefore && !wordAfter)
				return true;
		}
		return false;
	}

	// Every word of `query` (less a Polish ending) somewhere in the folded proto
	// name. Words of one or two letters are ignored ("z", "na"), a "+9" must be
	// in the name as it is, and a word marked '=' (an alias typed as itself,
	// see ExpandItemQuery) must be there whole.
	inline bool ItemWordsMatch(const std::string& foldedName, const std::string& query)
	{
		std::vector<std::string> words;
		SplitWords(query, words);
		int used = 0;
		for (size_t i = 0; i < words.size(); ++i)
		{
			const std::string& w = words[i];
			if (w.size() >= 2 && w[0] == '=')
			{
				if (!HasWholeWord(foldedName, w.substr(1)))
					return false;
				++used;
				continue;
			}
			if (w.size() >= 2 && w[0] == '+')
			{
				if (foldedName.find(w) == std::string::npos && foldedName.find(w.substr(1)) == std::string::npos)
					return false;
				++used;
				continue;
			}
			if (w.size() < 3)
				continue;
			const size_t stemLen = w.size() > 5 ? w.size() - 2 : (w.size() > 3 ? w.size() - 1 : w.size());
			if (foldedName.find(w.substr(0, stemLen)) == std::string::npos)
				return false;
			++used;
		}
		return used > 0;
	}

	// Against a list prepared once (ExpandItemQuery), for loops over many lines.
	inline bool ItemNameMatchesAny(const std::string& foldedName, const std::vector<std::string>& candidates)
	{
		for (size_t i = 0; i < candidates.size(); ++i)
			if (ItemWordsMatch(foldedName, candidates[i]))
				return true;
		return false;
	}

	// The one matcher everything uses: the query or any of its expansions.
	inline bool ItemNameMatches(const char* protoName, const std::string& foldedQuery)
	{
		if (!protoName || foldedQuery.empty())
			return false;
		const std::string name = FoldName(protoName);
		std::vector<std::string> candidates;
		ExpandItemQuery(foldedQuery, candidates);
		for (size_t i = 0; i < candidates.size(); ++i)
			if (ItemWordsMatch(name, candidates[i]))
				return true;
		return false;
	}

	// ----------------------------------------------------------------- places

	// Map codes players use. Negative: the kingdom's own M1/M2/M3, resolved
	// against the bot's empire. MAP_ALIAS_UNLISTED: a place people name that
	// this server's table does not know (AV) - the bot is certainly not on it.
	const long MAP_ALIAS_M1 = -1;
	const long MAP_ALIAS_M2 = -2;
	const long MAP_ALIAS_M3 = -3;
	const long MAP_ALIAS_UNLISTED = -100;

	struct TMapAlias
	{
		const char* alias;
		long map;
	};

	inline long FindMapAlias(const std::vector<std::string>& words, size_t& at)
	{
		static const TMapAlias kMaps[] = {
			{ "m1", MAP_ALIAS_M1 }, { "m2", MAP_ALIAS_M2 }, { "m3", MAP_ALIAS_M3 },
			{ "v1", 104 }, { "v2", 71 }, { "dt", 66 }, { "sohan", 61 }, { "sohanie", 61 },
			{ "red las", 68 }, { "czerwony las", 68 }, { "czerwonym lesie", 68 }, { "redzie", 68 },
			{ "las duchow", 67 }, { "lesie duchow", 67 }, { "dolina", 64 }, { "dolinie", 64 },
			{ "doline", 64 }, { "pustynia", 63 }, { "pustyni", 63 }, { "pustynie", 63 },
			{ "hwang", 65 }, { "swiatynia", 65 }, { "swiatyni", 65 }, { "ognista", 62 }, { "ognistej", 62 },
			{ "joan", 21 }, { "bokjung", 23 }, { "yongan", 1 }, { "jayang", 3 }, { "pyongmoo", 41 },
			{ "pyungmoo", 41 }, { "bakra", 43 }, { "waryong", 2 },
			{ "av", MAP_ALIAS_UNLISTED }, { "av1", MAP_ALIAS_UNLISTED }, { "av2", MAP_ALIAS_UNLISTED },
			{ "atlantyda", MAP_ALIAS_UNLISTED }, { "atlantydzie", MAP_ALIAS_UNLISTED },
			{ "grota", MAP_ALIAS_UNLISTED }, { "grocie", MAP_ALIAS_UNLISTED },
		};
		for (size_t i = 0; i < words.size(); ++i)
		{
			for (size_t m = 0; m < sizeof(kMaps) / sizeof(kMaps[0]); ++m)
			{
				std::vector<std::string> aw;
				SplitWords(kMaps[m].alias, aw);
				if (i + aw.size() > words.size())
					continue;
				bool hit = true;
				for (size_t k = 0; k < aw.size() && hit; ++k)
					hit = words[i + k] == aw[k];
				if (hit)
				{
					at = i;
					return kMaps[m].map;
				}
			}
		}
		return 0;
	}

	inline long ResolveMapAlias(long alias, int empire)
	{
		static const long kHome[3][3] = { { 1, 3, 4 }, { 21, 23, 24 }, { 41, 43, 44 } };
		if (alias >= MAP_ALIAS_M3 && alias <= MAP_ALIAS_M1)
		{
			if (empire < 1 || empire > 3)
				return 0;
			return kHome[empire - 1][-alias - 1];
		}
		return alias;
	}

	// ------------------------------------------------------------------ money

	// "500k" 500 000, "2kk" 2 000 000, "1.5kk" 1 500 000, "1kkk" 10^9,
	// "300 tys", "2 kk", "150000", "150 000 yang". 0 when the line names no sum.
	inline long long ParseYangAmount(const std::vector<std::string>& words, size_t* where = NULL)
	{
		for (size_t i = 0; i < words.size(); ++i)
		{
			const std::string& w = words[i];
			if (w.empty() || w[0] < '0' || w[0] > '9')
				continue;
			size_t p = 0;
			long long whole = 0, frac = 0, fracDiv = 1;
			while (p < w.size() && w[p] >= '0' && w[p] <= '9' && whole < 100000000000LL)
				whole = whole * 10 + (w[p++] - '0');
			if (p < w.size() && w[p] == '.')
			{
				++p;
				while (p < w.size() && w[p] >= '0' && w[p] <= '9' && fracDiv < 1000)
				{
					frac = frac * 10 + (w[p++] - '0');
					fracDiv *= 10;
				}
			}
			std::string suffix = w.substr(p);
			if (suffix.empty() && i + 1 < words.size())
			{
				const std::string& n = words[i + 1];
				if (n == "k" || n == "kk" || n == "kkk" || n == "tys" || n == "tysiecy" || n == "mln" || n == "kkw")
					suffix = n;
			}
			long long mult = 1;
			if (suffix == "k" || suffix == "tys" || suffix == "tysiecy") mult = 1000LL;
			else if (suffix == "kk" || suffix == "mln" || suffix == "m") mult = 1000000LL;
			else if (suffix == "kkk" || suffix == "mld") mult = 1000000000LL;
			else if (!suffix.empty() && suffix != "y" && suffix != "yang")
				continue; // "12d", "v1", "m2" - not money
			// A bare small number is a level, a count, "2" in "ile to 2+2" - not money.
			if (mult == 1 && whole < 1000)
				continue;
			if (where)
				*where = i;
			// "999999999999kkk" is past any purse, not a signed overflow.
			const long long cap = 4000000000000000000LL;
			if (whole >= cap / mult)
				return cap;
			return whole * mult + frac * mult / fracDiv;
		}
		return 0;
	}
}

#endif
