#ifndef __INC_METIN2_PLAYERBOT_CONV_TEXT_H__
#define __INC_METIN2_PLAYERBOT_CONV_TEXT_H__

// PlayerBot Conversation v6 - text layer (pure, no engine types).
//
// MESSAGE -> NORMALIZATION -> TOKENIZATION
//
// A whisper arrives as CP1250 from the Polish client (sometimes UTF-8 from
// a patched one). Everything here folds it to lowercase ASCII words:
//   - Polish letters to their base (a c e l n o s z z), both encodings,
//   - punctuation to spaces, but '?' / '!' / smileys are remembered,
//   - runs of three or more identical letters collapsed ("siemaaaa" -> "siema"),
//   - common chat abbreviations and typos rewritten ("nwm", "tera", "gdzei").
// Fuzzy matching (one edit / one transposition) is available to the lexicon,
// but only for words of five letters or more and only when the first letter
// agrees - short words are too easy to confuse.
//
// Unit tested by tests/playerbot_conversation_test.cpp.

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace playerbot_conv
{
	typedef unsigned int u32;

	const size_t CONV_MAX_INPUT = 256;
	const size_t CONV_MAX_WORDS = 32;

	struct TTokens
	{
		std::string norm;               // "gdzie teraz expisz"
		std::vector<std::string> words; // {"gdzie","teraz","expisz"}
		bool question;                  // contained '?'
		bool exclaim;                   // contained '!'
		bool smile;                     // :) :D xD ;) :P
		bool sad;                       // :( ;(
		size_t rawLength;

		TTokens() : question(false), exclaim(false), smile(false), sad(false), rawLength(0) {}

		bool Has(const char* w) const
		{
			for (size_t i = 0; i < words.size(); ++i)
				if (words[i] == w)
					return true;
			return false;
		}
		int Find(const char* w) const
		{
			for (size_t i = 0; i < words.size(); ++i)
				if (words[i] == w)
					return (int)i;
			return -1;
		}
	};

	// ------------------------------------------------------------------ fold

	inline unsigned char FoldCp1250(unsigned char c)
	{
		switch (c)
		{
			case 0xA5: case 0xB9: return 'a';
			case 0xC6: case 0xE6: return 'c';
			case 0xCA: case 0xEA: return 'e';
			case 0xA3: case 0xB3: return 'l';
			case 0xD1: case 0xF1: return 'n';
			case 0xD3: case 0xF3: return 'o';
			case 0x8C: case 0x9C: return 's';
			case 0x8F: case 0x9F: case 0xAF: case 0xBF: return 'z';
			default: break;
		}
		if (c >= 'A' && c <= 'Z')
			return (unsigned char)(c - 'A' + 'a');
		if (c >= 0x80)
			return ' ';
		return c;
	}

	// UTF-8 two-byte Polish letters. Returns 0 when the pair is not one.
	inline unsigned char FoldUtf8Pair(unsigned char a, unsigned char b)
	{
		if (a == 0xC3)
		{
			if (b == 0x93 || b == 0xB3) return 'o';
			return 0;
		}
		if (a == 0xC4)
		{
			switch (b)
			{
				case 0x84: case 0x85: return 'a';
				case 0x86: case 0x87: return 'c';
				case 0x98: case 0x99: return 'e';
				default: return 0;
			}
		}
		if (a == 0xC5)
		{
			switch (b)
			{
				case 0x81: case 0x82: return 'l';
				case 0x83: case 0x84: return 'n';
				case 0x9A: case 0x9B: return 's';
				case 0xB9: case 0xBA: case 0xBB: case 0xBC: return 'z';
				default: return 0;
			}
		}
		return 0;
	}

	// Chat shorthand and the typos people actually make. Applied to whole
	// words after folding. A replacement may contain a space ("nie wiem").
	inline const char* RewriteWord(const std::string& w)
	{
		static const char* const kMap[][2] = {
			{ "nwm", "nie wiem" }, { "nw", "nie wiem" }, { "niewiem", "nie wiem" },
			{ "tera", "teraz" }, { "terz", "teraz" }, { "tearz", "teraz" }, { "trz", "teraz" },
			{ "gdzei", "gdzie" }, { "gdize", "gdzie" }, { "gzie", "gdzie" }, { "gdz", "gdzie" },
			{ "gdie", "gdzie" }, { "dzie", "gdzie" }, { "gdze", "gdzie" },
			{ "cp", "co" }, { "czo", "co" },
			{ "wgl", "wogole" }, { "wogle", "wogole" },
			{ "cb", "ciebie" }, { "cie", "cie" }, { "ciebie", "ciebie" },
			{ "mb", "moze" }, { "mze", "moze" },
			{ "spk", "spoko" }, { "spoks", "spoko" }, { "spoczko", "spoko" },
			{ "oki", "ok" }, { "okej", "ok" }, { "okey", "ok" }, { "okk", "ok" }, { "okay", "ok" },
			{ "kk", "ok" }, { "k", "ok" }, { "oke", "ok" }, { "okie", "ok" },
			{ "dzk", "dzieki" }, { "dzieks", "dzieki" }, { "dziex", "dzieki" }, { "dziena", "dzieki" },
			{ "dzienki", "dzieki" }, { "dziekuwa", "dzieki" }, { "thx", "dzieki" }, { "thanks", "dzieki" },
			{ "dziekuje", "dzieki" }, { "dziekujemy", "dzieki" },
			{ "pls", "prosze" }, { "plz", "prosze" }, { "prosz", "prosze" },
			{ "lv", "lvl" }, { "lwl", "lvl" }, { "lewel", "level" }, { "lvla", "lvl" }, { "lvlu", "lvl" },
			{ "xdd", "xd" }, { "xddd", "xd" }, { "xdddd", "xd" },
			{ "hahaha", "haha" }, { "hahah", "haha" }, { "hah", "haha" }, { "ahah", "haha" },
			{ "ahaha", "haha" }, { "hehehe", "hehe" }, { "heh", "hehe" }, { "hihi", "hehe" },
			{ "lol", "haha" }, { "lmao", "haha" }, { "rotfl", "haha" },
			{ "czm", "czemu" }, { "czmu", "czemu" }, { "dlaczgo", "dlaczego" }, { "dlaczeg", "dlaczego" },
			{ "dlaczego", "dlaczego" }, { "dlaczeog", "dlaczego" }, { "dlczego", "dlaczego" },
			{ "robsz", "robisz" }, { "robiz", "robisz" }, { "rbisz", "robisz" }, { "robis", "robisz" },
			{ "robiesz", "robisz" }, { "robsiz", "robisz" }, { "robsisz", "robisz" },
			{ "jestres", "jestes" }, { "jestesz", "jestes" }, { "jestas", "jestes" }, { "jsetes", "jestes" },
			{ "jestesc", "jestes" }, { "jestecie", "jestescie" },
			{ "siem", "siema" }, { "siemka", "siema" }, { "siemano", "siema" }, { "siemanko", "siema" },
			{ "siemaa", "siema" }, { "siemson", "siema" }, { "elo", "siema" }, { "elko", "siema" },
			{ "eluwa", "siema" }, { "elos", "siema" }, { "hejka", "hej" }, { "hejo", "hej" },
			{ "hejjo", "hej" }, { "hey", "hej" }, { "hi", "hej" }, { "hello", "hej" }, { "helo", "hej" },
			{ "cze", "czesc" }, { "czesc", "czesc" }, { "czesa", "czesc" }, { "yo", "hej" }, 
			{ "narka", "nara" }, { "naraa", "nara" }, { "bb", "nara" }, { "bye", "nara" }, { "papa", "pa" },
			{ "pozdro", "pozdrawiam" },
			{ "ty", "ty" }, { "tb", "tobie" }, { "ciebi", "ciebie" },
			{ "yangow", "yang" }, { "yangi", "yang" }, { "yangu", "yang" }, { "jang", "yang" }, { "jangi", "yang" },
			{ "kaske", "kasa" }, { "kasiory", "kasa" }, { "kasiore", "kasa" }, { "kase", "kasa" }, { "kasy", "kasa" },
			{ "hajsu", "hajs" }, { "hajsik", "hajs" },
			{ "party", "pt" }, { "ptk", "pt" }, { "pty", "pt" }, { "ptki", "pt" },
			{ "expa", "exp" }, { "expik", "exp" }, { "expie", "expie" }, { "exper", "exp" },
			{ "expisz", "expisz" }, { "expujesz", "expisz" }, { "exipsz", "expisz" }, { "epxisz", "expisz" },
			{ "expiesz", "expisz" }, { "expis", "expisz" },
			{ "mobkow", "mobow" }, { "mobki", "moby" }, { "mobuf", "mobow" }, { "mobuw", "mobow" },
			{ "metki", "metiny" }, { "metka", "metin" }, { "metinki", "metiny" }, { "kamole", "metiny" },
			{ "bio", "biolog" }, { "biologa", "biologa" }, { "bilog", "biolog" },
			{ "eq", "eq" }, { "ekwipunek", "eq" }, { "ekwipunku", "eq" }, { "ekwip", "eq" },
			{ "naprawde", "naprawde" }, { "naprawd", "naprawde" }, { "napewno", "na pewno" },
			{ "serio", "serio" }, { "serjo", "serio" }, { "sreio", "serio" },
			{ "wiesz", "wiesz" }, { "wiem", "wiem" },
			{ "moge", "moge" }, { "moglbym", "moge" }, { "moglabym", "moge" }, { "mozna", "mozna" },
			{ "dobrze", "dobrze" }, { "dobre", "dobre" }, { "dobra", "dobra" },
			{ "potem", "potem" }, { "pozniej", "potem" }, { "puzniej", "potem" }, { "nastepnie", "potem" },
			{ "kiedys", "kiedys" },
			{ "gildja", "gildia" }, { "gilda", "gildia" }, { "gildyja", "gildia" }, { "gildie", "gildie" },
			{ "gildi", "gildii" }, { "gilde", "gildie" }, { "gildyjny", "gildii" }, { "guild", "gildia" },
			{ "koniaa", "konia" }, { "konika", "konia" }, { "konik", "kon" },
			{ "mape", "mape" }, { "mapka", "mapa" }, { "mapke", "mape" }, { "mapce", "mapie" },
			{ "dzis", "dzisiaj" }, { "dzisaj", "dzisiaj" }, { "dzisj", "dzisiaj" }, { "dzisiej", "dzisiaj" },
			{ "jutr", "jutro" }, { "wczoraj", "wczoraj" },
			{ "zimo", "zimno" }, { "zinmo", "zimno" }, { "ziomno", "zimno" },
			{ "ziom", "ziomek" }, { "ziomus", "ziomek" }, { "ziomal", "ziomek" }, { "stary", "stary" },
			{ "byku", "ziomek" }, { "mordo", "ziomek" }, { "mordko", "ziomek" }, { "brachu", "ziomek" },
			{ "bracie", "ziomek" }, { "kolego", "ziomek" }, { "kolezko", "ziomek" }, { "koles", "ziomek" },
			{ "szefie", "ziomek" }, { "wariacie", "ziomek" }, { "misiu", "ziomek" },
			// chat shorthand
			{ "gz", "gratki" }, { "gratz", "gratki" }, { "grats", "gratki" }, { "gg", "gratki" },
			{ "gl", "powodzenia" }, { "glhf", "powodzenia" }, { "hf", "powodzenia" },
			{ "brb", "zaraz wracam" }, { "relog", "zaraz wracam" }, { "zw", "zaraz wracam" },
			{ "np", "spoko" }, { "nmzc", "spoko" }, { "nzc", "spoko" }, { "nop", "spoko" },
			{ "nq", "nara" }, { "cya", "nara" }, { "cu", "nara" }, { "bb", "nara" },
			{ "btw", "" }, { "imo", "" }, { "tbh", "" },
			{ "omg", "wow" }, { "omfg", "wow" }, { "wtf", "wow" }, { "rofl", "haha" },
			{ "rdy", "gotowy" }, { "ready", "gotowy" }, { "gotowa", "gotowy" },
			{ "rew", "rewanz" }, { "tp", "teleport" }, { "tepa", "teleport" },
			{ "metek", "metin" }, { "metka", "metin" }, { "metki", "metiny" }, { "moob", "mob" },
			{ "mooby", "moby" }, { "mobki", "moby" },
			{ "poli", "polimorfia" }, { "polimorf", "polimorfia" },
			{ "militar", "kon" }, { "militara", "konia" }, { "mount", "kon" }, { "mounta", "konia" },
			{ "pz", "hp" }, { "mp", "sp" }, { "pe", "sp" },
			{ "kt", "kupie" }, { "sell", "sprzedam" }, { "wts", "sprzedam" }, { "wtb", "kupie" },
			{ "is", "itemshop" }, { "itemshopa", "itemshop" }, { "sm", "smocze monety" },
			{ "depo", "magazyn" }, { "dozo", "magazyn" }, { "dozorca", "magazyn" },
			{ "pvm", "exp" }, { "pve", "exp" }, { "pvb", "boss" },
			{ "ksujesz", "ksujesz" }, { "ksuj", "ksujesz" }, { "ksuje", "ksujesz" }, { "ks", "ksujesz" },
			{ "kryt", "krytyk" }, { "kryty", "krytyk" }, { "dmg", "obrazenia" }, { "def", "obrona" },
			{ "resp", "respawn" }, { "respi", "respawn" }, { "respa", "respawn" },
		};
		for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i)
			if (w == kMap[i][0])
				return kMap[i][1];
		return NULL;
	}

	inline void SplitWords(const std::string& s, std::vector<std::string>& out)
	{
		size_t i = 0;
		while (i < s.size())
		{
			while (i < s.size() && s[i] == ' ')
				++i;
			size_t j = i;
			while (j < s.size() && s[j] != ' ')
				++j;
			if (j > i && out.size() < CONV_MAX_WORDS)
				out.push_back(s.substr(i, j - i));
			i = j;
		}
	}

	// The whole normalization. `in` is a raw whisper, CP1250 or UTF-8.
	inline void Normalize(const char* in, TTokens& out)
	{
		out = TTokens();
		if (!in)
			return;
		std::string folded;
		folded.reserve(64);
		size_t n = strlen(in);
		if (n > CONV_MAX_INPUT)
			n = CONV_MAX_INPUT;
		out.rawLength = n;
		const unsigned char* p = (const unsigned char*)in;

		// Smileys before punctuation is dropped. "xd" is a word and survives.
		for (size_t i = 0; i + 1 < n; ++i)
		{
			const unsigned char a = p[i], b = p[i + 1];
			if ((a == ':' || a == ';' || a == '=') &&
					(b == ')' || b == 'D' || b == 'P' || b == 'p' || b == ']' || b == '3' || b == '*'))
				out.smile = true;
			if ((a == ':' || a == ';') && (b == '(' || b == '['))
				out.sad = true;
		}

		for (size_t i = 0; i < n; ++i)
		{
			unsigned char c = p[i];
			if (c >= 0xC3 && c <= 0xC5 && i + 1 < n && p[i + 1] >= 0x80 && p[i + 1] <= 0xBF)
			{
				const unsigned char f = FoldUtf8Pair(c, p[i + 1]);
				++i;
				folded += f ? (char)f : ' ';
				continue;
			}
			if (c == '?')
			{
				out.question = true;
				folded += ' ';
				continue;
			}
			if (c == '!')
			{
				out.exclaim = true;
				folded += ' ';
				continue;
			}
			// "1.5kk" and "+9" keep their punctuation: a sum and a refine level.
			if ((c == '.' || c == ',') && i > 0 && i + 1 < n && p[i - 1] >= '0' && p[i - 1] <= '9' &&
					p[i + 1] >= '0' && p[i + 1] <= '9')
			{
				folded += '.';
				continue;
			}
			if (c == '+' && i + 1 < n && p[i + 1] >= '0' && p[i + 1] <= '9')
			{
				if (!folded.empty() && folded[folded.size() - 1] != ' ')
					folded += ' ';
				folded += '+';
				continue;
			}
			c = FoldCp1250(c);
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
				folded += (char)c;
			else
				folded += ' ';
		}

		// Collapse runs of 3+ identical letters: "noooo" -> "no".
		std::string collapsed;
		collapsed.reserve(folded.size());
		for (size_t i = 0; i < folded.size(); )
		{
			size_t j = i;
			while (j < folded.size() && folded[j] == folded[i])
				++j;
			const size_t run = j - i;
			// Not 'k' or digits: "1kkk" is a billion and "150000" is not "150".
			if (folded[i] != ' ' && folded[i] != 'k' && (folded[i] < '0' || folded[i] > '9') && run >= 3)
				collapsed += folded[i];
			else
				collapsed.append(folded, i, run);
			i = j;
		}

		std::vector<std::string> raw;
		SplitWords(collapsed, raw);
		// "2 kk" is one sum; a lone "k"/"kk" is "ok" only in a short line
		// ("kk", "ok kk"), in a longer one it is money or Kawalek Klejnotu.
		{
			std::vector<std::string> merged;
			for (size_t i = 0; i < raw.size(); ++i)
			{
				const std::string& w = raw[i];
				const bool unit = w == "k" || w == "kk" || w == "kkk";
				if (unit && !merged.empty())
				{
					const std::string& prev = merged.back();
					bool number = !prev.empty();
					for (size_t k = 0; k < prev.size() && number; ++k)
						number = (prev[k] >= '0' && prev[k] <= '9') || prev[k] == '.';
					if (number)
					{
						merged.back() += w;
						continue;
					}
				}
				merged.push_back(w);
			}
			raw.swap(merged);
		}
		const bool shortLine = raw.size() <= 2;
		for (size_t i = 0; i < raw.size(); ++i)
		{
			if (!shortLine && (raw[i] == "k" || raw[i] == "kk" || raw[i] == "kkk"))
			{
				if (out.words.size() < CONV_MAX_WORDS)
					out.words.push_back(raw[i]);
				continue;
			}
			// "np" alone is "nie ma problemu"; inside a sentence it is "na
			// przyklad" ("czemu nie wymienisz broni? np. rib ze srednimi"), and
			// read as "spoko" it put an acknowledgement into an argument.
			if (!shortLine && raw[i] == "np")
				continue;
			const char* rw = RewriteWord(raw[i]);
			if (rw)
				SplitWords(rw, out.words);
			else if (out.words.size() < CONV_MAX_WORDS)
				out.words.push_back(raw[i]);
		}
		for (size_t i = 0; i < out.words.size(); ++i)
		{
			if (i)
				out.norm += ' ';
			out.norm += out.words[i];
		}
		if (out.Has("xd") || out.Has("haha") || out.Has("hehe"))
			out.smile = true;
	}

	// A proto name (CP1250) folded to lowercase ASCII, nothing else changed.
	inline std::string FoldName(const char* in)
	{
		std::string out;
		const unsigned char* p = (const unsigned char*)(in ? in : "");
		for (; *p; ++p)
		{
			if (*p >= 0xC3 && *p <= 0xC5 && p[1] >= 0x80 && p[1] <= 0xBF)
			{
				const unsigned char f = FoldUtf8Pair(p[0], p[1]);
				out += f ? (char)f : ' ';
				++p;
				continue;
			}
			out += (char)FoldCp1250(*p);
		}
		return out;
	}

	// ----------------------------------------------------------------- fuzzy

	// Restricted Damerau-Levenshtein distance, early-out above `limit`.
	inline int EditDistance(const char* a, size_t la, const char* b, size_t lb, int limit)
	{
		if ((int)(la > lb ? la - lb : lb - la) > limit)
			return limit + 1;
		if (la > 24 || lb > 24)
			return limit + 1;
		int d[26][26];
		for (size_t i = 0; i <= la; ++i) d[i][0] = (int)i;
		for (size_t j = 0; j <= lb; ++j) d[0][j] = (int)j;
		for (size_t i = 1; i <= la; ++i)
		{
			int rowMin = limit + 1;
			for (size_t j = 1; j <= lb; ++j)
			{
				const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
				int v = d[i - 1][j] + 1;
				if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1;
				if (d[i - 1][j - 1] + cost < v) v = d[i - 1][j - 1] + cost;
				if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] &&
						d[i - 2][j - 2] + 1 < v)
					v = d[i - 2][j - 2] + 1;
				d[i][j] = v;
				if (v < rowMin) rowMin = v;
			}
			if (rowMin > limit)
				return limit + 1;
		}
		return d[la][lb];
	}

	// A word typed by a person against a word the lexicon knows. Five letters
	// or more, same first letter, one mistake.
	inline bool FuzzyEquals(const std::string& typed, const char* known)
	{
		const size_t lk = strlen(known);
		if (lk < 5 || typed.size() < 4 || typed[0] != known[0])
			return false;
		return EditDistance(typed.c_str(), typed.size(), known, lk, 1) <= 1;
	}

	inline bool StartsWith(const std::string& s, const char* prefix)
	{
		const size_t l = strlen(prefix);
		return s.size() >= l && s.compare(0, l, prefix) == 0;
	}

	// ---------------------------------------------------------------- output

	inline void CapitalizeFirst(std::string& s)
	{
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == ' ')
				continue;
			if (s[i] >= 'a' && s[i] <= 'z')
				s[i] = (char)(s[i] - 'a' + 'A');
			return;
		}
	}

	inline void LowerFirst(std::string& s)
	{
		// Only a plain word start, never a name the template quoted.
		if (s.size() >= 2 && s[0] >= 'A' && s[0] <= 'Z' && s[1] >= 'a' && s[1] <= 'z')
			s[0] = (char)(s[0] - 'A' + 'a');
	}

	inline void ReplaceAll(std::string& s, const char* from, const std::string& to)
	{
		const size_t lf = strlen(from);
		if (!lf)
			return;
		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos)
		{
			s.replace(pos, lf, to);
			pos += to.size();
		}
	}

	inline std::string ToString(long long v)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%lld", v);
		return buf;
	}

	// 12345678 -> "12kk", 540000 -> "540k". How players say yang.
	inline std::string FormatYang(long long v)
	{
		char buf[32];
		if (v >= 1000000000LL)
			snprintf(buf, sizeof(buf), "%lld kkk", v / 1000000000LL);
		else if (v >= 1000000LL)
		{
			const long long kk = v / 1000000LL;
			const long long rest = (v % 1000000LL) / 100000LL;
			if (kk < 10 && rest)
				snprintf(buf, sizeof(buf), "%lld.%lldkk", kk, rest);
			else
				snprintf(buf, sizeof(buf), "%lldkk", kk);
		}
		else if (v >= 1000LL)
			snprintf(buf, sizeof(buf), "%lldk", v / 1000LL);
		else
			snprintf(buf, sizeof(buf), "%lld", v);
		return buf;
	}

	// ----------------------------------------------------------- arithmetic

	// "ile to 2+2", "7 razy 8", "10/4", "2+2*2": one sum the whole line is
	// about. "Ale czego?" to "ile to 2+2" was a person testing the bot and the
	// bot failing the test. The operators are read from the raw line, because
	// normalization turns every one of them but a '+' before a digit into a
	// space.
	struct TArithmetic
	{
		double value;
		int terms;
		bool divZero;
		bool tooBig;
		bool mixed;      // + or - beside * or /: "2+2*2" is 6, and it is asked to catch somebody out
		TArithmetic() : value(0), terms(0), divZero(false), tooBig(false), mixed(false) {}
	};

	// The words a sum may be wrapped in. Anything else in the line - "mam 2+2
	// miecze", "na 3-4 lvl", "fms +9" - makes it a line about something else.
	inline bool IsArithmeticFiller(const std::string& w)
	{
		static const char* const kWords[] = {
			"ile", "to", "jest", "bedzie", "wynosi", "daje", "da", "a", "no", "hej", "siema", "policz", "oblicz",
			"policzysz", "obliczysz", "policzyc", "obliczyc", "mi", "szybko", "rowna", "rowne", "sie", "wynik",
			"prosze", "pls", "czy", "wiesz", "umiesz", "powiedz", "ty", "ziomek", "kolego", "stary", "hmm",
			"xd", "haha", "hehe", "lol", "ok", "dobra", "zagadka", "pytanie", "matma", "matematyka", "matme",
			"sprawdzmy", "sprawdze", "zobaczymy", "test", "testuje"
		};
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (w == kWords[i])
				return true;
		return false;
	}

	inline char ArithmeticWordOperator(const std::string& w)
	{
		if (w == "plus" || w == "dodac" || w == "dodaj")
			return '+';
		if (w == "minus" || w == "odjac" || w == "odejmij")
			return '-';
		if (w == "razy" || w == "x" || StartsWith(w, "pomnoz"))
			return '*';
		if (w == "przez" || StartsWith(w, "podziel") || StartsWith(w, "dzielon") || w == "dzielic")
			return '/';
		return 0;
	}

	inline bool ParseArithmetic(const char* raw, TArithmetic& out)
	{
		out = TArithmetic();
		if (!raw)
			return false;
		struct TTok
		{
			char kind;        // 'n' a number, 'o' an operator, 'w' a word
			double num;
			char op;
			std::string word;
		};
		std::vector<TTok> toks;
		const unsigned char* p = (const unsigned char*)raw;
		size_t n = strlen(raw);
		if (n > CONV_MAX_INPUT)
			n = CONV_MAX_INPUT;
		size_t i = 0;
		while (i < n && toks.size() < 48)
		{
			const unsigned char c = p[i];
			if (c >= '0' && c <= '9')
			{
				size_t j = i;
				double v = 0;
				int digits = 0;
				while (j < n && p[j] >= '0' && p[j] <= '9')
				{
					if (digits < 15)
						v = v * 10 + (p[j] - '0');
					++digits;
					++j;
				}
				if (j + 1 < n && (p[j] == '.' || p[j] == ',') && p[j + 1] >= '0' && p[j + 1] <= '9')
				{
					++j;
					double scale = 0.1;
					while (j < n && p[j] >= '0' && p[j] <= '9')
					{
						v += (p[j] - '0') * scale;
						scale /= 10;
						++j;
					}
				}
				TTok t;
				// "7x8": the x between two numbers is a times.
				const bool times = j + 1 < n && (p[j] == 'x' || p[j] == 'X') && p[j + 1] >= '0' && p[j + 1] <= '9';
				// "12d", "2kk", "1v1", "50lvl": a number glued to letters is a name.
				if (!times && j < n && ((p[j] >= 'a' && p[j] <= 'z') || (p[j] >= 'A' && p[j] <= 'Z') || p[j] >= 0x80))
				{
					t.kind = 'w';
					t.num = 0;
					t.op = 0;
					while (i < n && ((p[i] >= 'a' && p[i] <= 'z') || (p[i] >= 'A' && p[i] <= 'Z') ||
							(p[i] >= '0' && p[i] <= '9') || p[i] >= 0x80))
						t.word += (char)FoldCp1250(p[i++]);
					toks.push_back(t);
					continue;
				}
				t.kind = 'n';
				t.num = digits > 15 ? 1e18 : v;
				t.op = 0;
				toks.push_back(t);
				i = j;
				if (times)
				{
					TTok o;
					o.kind = 'o';
					o.num = 0;
					o.op = '*';
					toks.push_back(o);
					++i;
				}
				continue;
			}
			if (c == '+' || c == '-' || c == '*' || c == '/')
			{
				TTok o;
				o.kind = 'o';
				o.num = 0;
				o.op = (char)c;
				toks.push_back(o);
				++i;
				continue;
			}
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80)
			{
				std::string w;
				while (i < n && ((p[i] >= 'a' && p[i] <= 'z') || (p[i] >= 'A' && p[i] <= 'Z') || p[i] >= 0x80))
				{
					if (p[i] >= 0xC3 && p[i] <= 0xC5 && i + 1 < n && p[i + 1] >= 0x80 && p[i + 1] <= 0xBF)
					{
						const unsigned char f = FoldUtf8Pair(p[i], p[i + 1]);
						if (f)
							w += (char)f;
						i += 2;
						continue;
					}
					const unsigned char f = FoldCp1250(p[i]);
					if (f != ' ')
						w += (char)f;
					++i;
				}
				// "pomnozone przez", "podzielic na": the second word belongs to the first.
				if ((w == "przez" || w == "na") && !toks.empty() && toks.back().kind == 'o')
					continue;
				const char op = ArithmeticWordOperator(w);
				TTok t;
				t.kind = op ? 'o' : 'w';
				t.num = 0;
				t.op = op;
				t.word = w;
				toks.push_back(t);
				continue;
			}
			++i; // '?', '!', '=', ',', spaces: nothing of their own
		}

		// The first run of number (operator number)+.
		size_t start = toks.size(), end = toks.size();
		for (size_t k = 0; k < toks.size() && start == toks.size(); ++k)
		{
			if (toks[k].kind != 'n')
				continue;
			size_t e = k + 1;
			while (e + 1 < toks.size() && toks[e].kind == 'o' && toks[e + 1].kind == 'n')
				e += 2;
			if (e > k + 1)
			{
				start = k;
				end = e;
			}
		}
		if (start == toks.size())
			return false;
		for (size_t k = 0; k < toks.size(); ++k)
		{
			if (k >= start && k < end)
				continue;
			if (toks[k].kind != 'w' || !IsArithmeticFiller(toks[k].word))
				return false;
		}

		// Times and division first, then plus and minus, as at school.
		std::vector<double> sums;
		std::vector<char> signs;
		double acc = toks[start].num;
		bool additive = false, multiplicative = false;
		out.terms = 1;
		for (size_t k = start + 1; k + 1 < end; k += 2)
		{
			const char op = toks[k].op;
			const double b = toks[k + 1].num;
			++out.terms;
			if (b > 1e15 || acc > 1e15)
				out.tooBig = true;
			if (op == '*' || op == '/')
			{
				multiplicative = true;
				if (op == '/' && b == 0)
				{
					out.divZero = true;
					return true;
				}
				acc = op == '*' ? acc * b : acc / b;
			}
			else
			{
				additive = true;
				sums.push_back(acc);
				signs.push_back(op);
				acc = b;
			}
		}
		sums.push_back(acc);
		double result = sums[0];
		for (size_t k = 0; k < signs.size(); ++k)
			result = signs[k] == '+' ? result + sums[k + 1] : result - sums[k + 1];
		out.value = result;
		out.mixed = additive && multiplicative;
		if (result > 1e15 || result < -1e15)
			out.tooBig = true;
		return true;
	}

	// 4 -> "4", 2.5 -> "2,5", 3.3333 -> "3,33": the way a person writes it.
	inline std::string FormatNumberPl(double v)
	{
		char buf[48];
		const double rounded = v < 0 ? -(double)(long long)(-v + 0.5) : (double)(long long)(v + 0.5);
		const double diff = v - rounded;
		if (diff < 1e-9 && diff > -1e-9)
		{
			snprintf(buf, sizeof(buf), "%lld", (long long)rounded);
			return buf;
		}
		snprintf(buf, sizeof(buf), "%.2f", v);
		std::string s = buf;
		while (!s.empty() && s[s.size() - 1] == '0')
			s.erase(s.size() - 1);
		if (!s.empty() && s[s.size() - 1] == '.')
			s.erase(s.size() - 1);
		for (size_t i = 0; i < s.size(); ++i)
			if (s[i] == '.')
				s[i] = ',';
		return s;
	}

	// ------------------------------------------------------------ item links

	// A shift-clicked item. The client sends "|cffffc700|Hitem:4e21:0:0:0|h
	// [Miecz Pelni Ksiezyca+9]|h|r" and draws only the bracketed name; the
	// words of it made the old reader answer "[Miecz Pelni Ksiezyca+9]" with
	// the bot's own gear. A line typed with brackets reads the same, but
	// "[GA]Seban" is a guild tag in a name, so a bare bracket counts only round
	// a name of two words or one carrying a refine. The name is kept as it
	// came (the proto's own CP1250), less anything but letters, digits,
	// spaces and "+.-", so a colour code cannot be echoed back.
	inline bool ExtractItemLink(const char* raw, std::string& name)
	{
		name.clear();
		if (!raw)
			return false;
		const char* link = strstr(raw, "|Hitem");
		const char* open = link ? strchr(link, '[') : NULL;
		if (!open)
			open = strchr(raw, '[');
		if (!open)
			return false;
		const char* close = strchr(open + 1, ']');
		if (!close || close - open - 1 < 2 || close - open - 1 > 64)
			return false;
		std::string out;
		bool letter = false;
		for (const unsigned char* q = (const unsigned char*)open + 1; q < (const unsigned char*)close; ++q)
		{
			const unsigned char c = *q;
			const bool isLetter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
			const bool keep = isLetter || (c >= '0' && c <= '9') || c == ' ' || c == '+' || c == '.' || c == '-';
			if (!keep)
				continue;
			if (c == ' ' && (out.empty() || out[out.size() - 1] == ' '))
				continue;
			if (isLetter)
				letter = true;
			out += (char)c;
		}
		while (!out.empty() && out[out.size() - 1] == ' ')
			out.erase(out.size() - 1);
		if (!letter || out.size() < 3)
			return false;
		if (!link && out.find(' ') == std::string::npos && out.find('+') == std::string::npos)
			return false;
		name = out;
		return true;
	}

	// "Miecz Pelni Ksiezyca+9" -> 9, -1 when the name carries no refine.
	inline int RefineOfName(const std::string& name)
	{
		const size_t plus = name.find_last_of('+');
		if (plus == std::string::npos || plus + 1 >= name.size())
			return -1;
		int v = 0;
		for (size_t i = plus + 1; i < name.size(); ++i)
		{
			if (name[i] < '0' || name[i] > '9')
				return -1;
			v = v * 10 + (name[i] - '0');
			if (v > 99)
				return -1;
		}
		return v;
	}

	// --------------------------------------------------------------- random

	// Small deterministic RNG so tests can pin it and the engine can seed it.
	struct TRng
	{
		u32 s;
		explicit TRng(u32 seed = 0x9E3779B9u) : s(seed ? seed : 0x9E3779B9u) {}
		u32 Next()
		{
			s ^= s << 13;
			s ^= s >> 17;
			s ^= s << 5;
			return s;
		}
		u32 Range(u32 n) { return n ? Next() % n : 0; }
		bool Chance(u32 percent) { return Range(100) < percent; }
	};

	inline u32 HashStr(const char* s, u32 seed)
	{
		u32 h = 2166136261u ^ seed;
		for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p; ++p)
			h = (h ^ *p) * 16777619u;
		h ^= h >> 15;
		h *= 0x2c1b3c6du;
		h ^= h >> 12;
		return h;
	}
}

#endif
