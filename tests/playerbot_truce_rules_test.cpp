// A person's truce with the bots (playerbot_truce_rules.h): which whispers
// are a surrender, when the bots grant one, and the deaths that make them.
//
// The words are the whole public surface - "napisz do bota poddaje sie" is
// what the operator promised on the Discord - so what is understood is
// written down here, and so is what must not be: "nie poddaje sie", "poddaj
// sie", anything about luring.
//
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_truce_rules_test.cpp
#include "playerbot_truce_rules.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace playerbot_truce_rules;

namespace {

int g_checks = 0;

// The fold the game does before asking (FoldPlayerBotChatText): only the
// letters these tests type.
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

void expect(const std::string& line, ESurrender want)
{
	++g_checks;
	const ESurrender got = ParseSurrender(fold(line).c_str());
	if (got != want)
	{
		std::fprintf(stderr, "\"%s\": got %d, want %d\n", line.c_str(), (int)got, (int)want);
		assert(false);
	}
}

void check(bool ok, const char* what)
{
	++g_checks;
	if (!ok)
	{
		std::fprintf(stderr, "failed: %s\n", what);
		assert(false);
	}
}

}

int main()
{
	// What people write when they have had enough, from anybody.
	expect("poddaję się", SURRENDER_PLAIN);
	expect("Poddaje sie!!", SURRENDER_PLAIN);
	expect("dobra poddaję się, odpuść", SURRENDER_PLAIN);
	expect("poddajemy sie", SURRENDER_PLAIN);
	expect("poddam się", SURRENDER_PLAIN);
	expect("rozejm?", SURRENDER_PLAIN);
	expect("rozejmu chce", SURRENDER_PLAIN);
	expect("odpuść", SURRENDER_PLAIN);
	expect("odpuśćcie mi", SURRENDER_PLAIN);
	expect("odpuście już", SURRENDER_PLAIN);
	expect("możesz mi odpuścić?", SURRENDER_PLAIN);
	expect("litości", SURRENDER_PLAIN);
	expect("mam dość", SURRENDER_PLAIN);
	expect("biała flaga", SURRENDER_PLAIN);
	expect("dajcie mi spokój", SURRENDER_PLAIN);
	expect("zostawcie mnie", SURRENDER_PLAIN);
	expect("przestańcie", SURRENDER_PLAIN);
	expect("koniec walki", SURRENDER_PLAIN);
	expect("prosze nie bij mnie", SURRENDER_PLAIN);
	expect("nie bijcie", SURRENDER_PLAIN);
	expect("I give up", SURRENDER_PLAIN);
	expect("truce", SURRENDER_PLAIN);

	// Words that mean it only from somebody the bots are fighting.
	expect("dość", SURRENDER_IN_A_FIGHT);
	expect("wystarczy!", SURRENDER_IN_A_FIGHT);
	expect("stop", SURRENDER_IN_A_FIGHT);
	expect("przestań", SURRENDER_IN_A_FIGHT);
	expect("zostaw", SURRENDER_IN_A_FIGHT);
	expect("przepraszam", SURRENDER_IN_A_FIGHT);
	expect("sorry", SURRENDER_IN_A_FIGHT);
	expect("sorki :(", SURRENDER_IN_A_FIGHT);
	expect("wybaczcie", SURRENDER_IN_A_FIGHT);
	expect("gg", SURRENDER_IN_A_FIGHT);

	// Not a surrender: defiance, telling or asking the bot, luring, and the
	// words inside other words.
	expect("nie poddaję się!", SURRENDER_NONE);
	expect("nigdy się nie poddam", SURRENDER_NONE);
	expect("poddaj się", SURRENDER_NONE);
	expect("poddajesz się?", SURRENDER_NONE);
	expect("nie odpuszczę", SURRENDER_NONE);
	expect("przestań lurować", SURRENDER_NONE);
	expect("odpuść lurowanie", SURRENDER_NONE);
	expect("luruj", SURRENDER_NONE);
	expect("stopy +7 sprzedam", SURRENDER_NONE);
	expect("kupie dosciana zbroje", SURRENDER_NONE);
	expect("co robisz?", SURRENDER_NONE);
	expect("", SURRENDER_NONE);
	++g_checks;
	assert(ParseSurrender(NULL) == SURRENDER_NONE);

	// A truce asked for, held, asked for again, broken, refused, and asked
	// for again once the refusal ran out.
	{
		const unsigned int kTruce = 30U * 60000U;
		const unsigned int kRefuse = 10U * 60000U;
		TTruce t;
		unsigned int minutes = 0;
		check(!IsActive(t, 1000), "a new truce is not active");
		check(AskTruce(t, 1000, kTruce, kRefuse, minutes) == ANSWER_GRANTED && minutes == 30, "granted for 30");
		check(IsActive(t, 1000 + kTruce - 1), "still active a moment before its end");
		check(!IsActive(t, 1000 + kTruce), "over at its end");
		check(AskTruce(t, 1000 + 60000, kTruce, kRefuse, minutes) == ANSWER_ALREADY && minutes == 29,
				"asked again: 29 minutes left");
		BreakTruce(t, 1000 + 120000);
		check(!IsActive(t, 1000 + 120001), "broken is not active");
		check(AskTruce(t, 1000 + 180000, kTruce, kRefuse, minutes) == ANSWER_REFUSED_AFTER_BREAK && minutes == 9,
				"refused a minute after the break: 9 minutes to wait");
		check(AskTruce(t, 1000 + 120000 + kRefuse, kTruce, kRefuse, minutes) == ANSWER_GRANTED,
				"granted again once the refusal ran out");
		// The bots grant one after the deaths whatever the person did.
		TTruce g;
		BreakTruce(g, 5000);
		GrantTruce(g, 6000, kTruce);
		check(IsActive(g, 6000 + kTruce - 1), "granted after the deaths inside the refusal");
		// A clock that wraps.
		TTruce w;
		const unsigned int nearWrap = 0xFFFFFFFFU - 1000U;
		AskTruce(w, nearWrap, kTruce, kRefuse, minutes);
		check(IsActive(w, nearWrap + 5000U), "active across the wrap of the clock");
		check(!IsActive(w, nearWrap + kTruce + 1U), "over across the wrap of the clock");
	}

	// Two deaths in a quarter of an hour; the same death seen by three bots
	// is one; a tally past its window starts again; a spent tally is not
	// restarted by the bots still noticing the death it was spent on.
	{
		const unsigned int kWindow = 15U * 60000U;
		const unsigned int kDedup = 20000U;
		TDeathTally d;
		check(!NoteDeath(d, 100000, kWindow, kDedup, 2), "first death");
		check(!NoteDeath(d, 101000, kWindow, kDedup, 2), "the same death seen by another bot");
		check(!NoteDeath(d, 105000, kWindow, kDedup, 2), "and by a third");
		check(NoteDeath(d, 400000, kWindow, kDedup, 2), "second death five minutes later: a truce");
		check(!NoteDeath(d, 401000, kWindow, kDedup, 2), "the second death seen again starts nothing");
		check(d.count == 0, "the spent tally stays empty");
		TDeathTally late;
		check(!NoteDeath(late, 0, kWindow, kDedup, 2), "a death");
		check(!NoteDeath(late, kWindow + 1000, kWindow, kDedup, 2), "the next one after the window starts over");
		check(NoteDeath(late, kWindow + 200000, kWindow, kDedup, 2), "and the one after it counts with it");
	}

	std::printf("playerbot_truce_rules: %d checks passed\n", g_checks);
	return 0;
}
