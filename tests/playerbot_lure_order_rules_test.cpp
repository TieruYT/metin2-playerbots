// What a person's whisper to a bot is taken to mean: "luruj" starts the
// Archer's course for them, "przestan lurowac" ends it, and everything else is
// somebody talking to a bot (playerbot_lure_order_rules.h).
//
// The words are the whole public surface of the feature - a line that is not
// understood falls through to the trade reply, and the person concludes the
// bot is broken - so what is understood is written down here rather than left
// to whoever next edits the list.
//
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_lure_order_rules_test.cpp
#include "playerbot_lure_order_rules.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace playerbot_lure_rules;

namespace {

int g_checks = 0;

// The fold the game does before asking: CP1250 to lowercase ASCII with the
// Polish letters on their base. Only the letters these tests type are here -
// the real one is FoldPlayerBotChatText and covers the whole high half.
std::string fold(const std::string& in)
{
	static const char* kFrom[] = { "ą", "ć", "ę", "ł", "ń",
			"ó", "ś", "ź", "ż" };
	static const char kTo[] = { 'a', 'c', 'e', 'l', 'n', 'o', 's', 'z', 'z' };
	std::string out;
	for (size_t i = 0; i < in.size(); )
	{
		bool folded = false;
		for (size_t k = 0; k < sizeof(kTo) && !folded; ++k)
		{
			const std::string from(kFrom[k]);
			if (in.compare(i, from.size(), from) == 0)
			{
				out += kTo[k];
				i += from.size();
				folded = true;
			}
		}
		if (folded)
			continue;
		const unsigned char c = (unsigned char)in[i++];
		out += (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
	}
	return out;
}

void check(const char* line, EOrder want, const char* what)
{
	++g_checks;
	const EOrder got = ParseOrder(fold(line).c_str());
	if (got != want)
	{
		std::fprintf(stderr, "FAIL %-34s \"%s\" -> %d, oczekiwano %d\n", what, line, (int)got, (int)want);
		assert(false);
	}
}

}  // namespace

int main()
{
	// The two documented commands, as the bot itself tells the player to type
	// them, with the diacritics a Polish keyboard produces.
	check("luruj", ORDER_START, "komenda z instrukcji");
	check("przestań lurować", ORDER_STOP, "komenda z instrukcji");

	// Nobody types an imperative the same way twice, and none of these should
	// cost the person a second attempt.
	check("Luruj!", ORDER_START, "wielka litera i wykrzyknik");
	check("  luruj  ", ORDER_START, "spacje po bokach");
	check("lur", ORDER_START, "skrot");
	check("moze bys polurowal?", ORDER_START, "zdanie");
	check("przyciągnij mi moby", ORDER_START, "inne slowo");
	check("ciągnij", ORDER_START, "jeszcze inne");
	check("pull", ORDER_START, "po angielsku");
	check("przyprowadź coś", ORDER_START, "przyprowadz");

	// Taking it back. "przestan lurowac" mentions luring as well, so the two
	// lists are asked in the order that makes the refusal win - this is the
	// case the order of the tests exists for.
	check("przestań lurować", ORDER_STOP, "odwolanie");
	check("koniec lurowania", ORDER_STOP, "koniec");
	check("dość lurowania", ORDER_STOP, "dosc");
	check("nie luruj", ORDER_STOP, "zaprzeczenie");
	check("już nie luruj", ORDER_STOP, "juz nie");
	check("wystarczy lurowania", ORDER_STOP, "wystarczy");
	check("przerwij lur", ORDER_STOP, "przerwij");
	check("stop", ORDER_STOP, "gole stop");
	check("STOP!", ORDER_STOP, "gole stop z wykrzyknikiem");
	check("koniec", ORDER_STOP, "gole koniec");
	check("wystarczy", ORDER_STOP, "gole wystarczy");

	// Everything else is a person talking to a bot, and must reach the trade
	// reply rather than being swallowed here. "Kupie" is the case that
	// matters: the trade parser runs after this one.
	check("kupie ku aura miecza", ORDER_NONE, "handel");
	check("sprzedam ząb orka", ORDER_NONE, "handel");
	check("czesc", ORDER_NONE, "powitanie");
	check("", ORDER_NONE, "pusta linia");
	check("   ", ORDER_NONE, "same spacje");
	check("...", ORDER_NONE, "same separatory");
	// A stop word inside a sentence that says nothing about luring is not an
	// order: "zostaw ten kamien" ends nothing. On its own it is one, because a
	// person says "koniec" to a bot for exactly one reason, and a bot that is
	// not luring for them answers "nie luruje dla ciebie" and no harm is done.
	check("zostaw ten kamien", ORDER_NONE, "stop w zdaniu bez lura");

	std::printf("playerbot_lure_order_rules: %d sprawdzen OK\n", g_checks);
	return 0;
}
