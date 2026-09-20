#ifndef __INC_METIN2_PLAYERBOT_LURE_ORDER_RULES_H__
#define __INC_METIN2_PLAYERBOT_LURE_ORDER_RULES_H__

// What a person's whisper to a bot means, as pure policy.
//
// "Luruj" is an order a player gives an Archer in their own party, and
// "przestan lurowac" takes it back (playerbot_chat_trade.h sends it,
// playerbot_lure.h runs it). The words are the whole public surface of the
// feature - somebody who types the wrong thing gets a trade reply and
// concludes the bot is broken - so they are decided here, against a folded
// line, with no engine types anywhere near them and a test that says what is
// understood: tests/playerbot_lure_order_rules_test.cpp.
//
// Folding is the caller's: CP1250 to lowercase ASCII, the Polish letters to
// their base. Everything below therefore compares plain lowercase text.

#include <cstring>

namespace playerbot_lure_rules
{
	enum EOrder
	{
		ORDER_NONE = 0,
		ORDER_START,
		ORDER_STOP
	};

	// What separates words in a line somebody typed. The trade parser asks the
	// same question, so there is one answer to it.
	inline bool IsSeparator(char c)
	{
		return c == ' ' || c == '\t' || c == ':' || c == ',' || c == '.' || c == '!' ||
				c == '?' || c == '-' || c == '"' || c == '\'';
	}

	// Forgiving on purpose: this is a line a person types mid-fight, not a
	// command syntax. Anything that mentions luring is an order; anything that
	// mentions it beside a word of refusal is the order being taken back -
	// tested in that order, because "przestan lurowac" mentions both. A bare
	// stop word is a stop as well, because luring is the only thing anybody can
	// ask a bot to do, so "stop" can only be about this.
	inline EOrder ParseOrder(const char* folded)
	{
		if (!folded)
			return ORDER_NONE;
		while (*folded && IsSeparator(*folded))
			++folded;
		size_t end = strlen(folded);
		while (end > 0 && IsSeparator(folded[end - 1]))
			--end;
		if (end == 0)
			return ORDER_NONE;

		// "lur" catches lur, luruj, lurowac, lurowanie and the imperatives
		// nobody spells the same way twice; the rest are what people wrote in
		// the first week of asking for it.
		static const char* kLure[] = {
			"lur", "pull", "przyciag", "ciagnij", "ciagnac", "przyprowadz"
		};
		static const char* kStop[] = {
			"przestan", "przestac", "stop", "koniec", "dosc", "wystarczy",
			"przerwij", "zostaw", "nie lur", "juz nie"
		};

		bool lure = false;
		for (size_t i = 0; i < sizeof(kLure) / sizeof(kLure[0]) && !lure; ++i)
			lure = strstr(folded, kLure[i]) != NULL;
		bool stop = false;
		for (size_t i = 0; i < sizeof(kStop) / sizeof(kStop[0]) && !stop; ++i)
			stop = strstr(folded, kStop[i]) != NULL;
		bool bareStop = false;
		for (size_t i = 0; i < sizeof(kStop) / sizeof(kStop[0]) && !bareStop; ++i)
			bareStop = strlen(kStop[i]) == end && strncmp(folded, kStop[i], end) == 0;

		if (stop && (lure || bareStop))
			return ORDER_STOP;
		if (lure)
			return ORDER_START;
		return ORDER_NONE;
	}
}

#endif
