#ifndef __INC_METIN2_PLAYERBOT_TRUCE_RULES_H__
#define __INC_METIN2_PLAYERBOT_TRUCE_RULES_H__

// A person's truce with the bots, as pure policy.
//
// The Anti-PK protocol (playerbot_anti_pk.h) answers a person's blow with the
// struck bot, its party and its guild - every member within twelve kilometres,
// for fifteen seconds after the last blow - and a person fighting a boss beside
// bots grazes them with every area skill, so the guild's call was renewed for
// as long as the fight lasted: "zrob cos z tym zeby one se odpuszczaly po
// jakims czasie" (Mur4s, 25 September, at the Ezot. chief with a bot guild
// "defending itself" round him). A truce is the way out, by two roads: a
// whisper to any bot - "poddaje sie", the operator's promise on the Discord
// the same minute - and the person's second death in a quarter of an hour
// while bots fought them (Tieru: "jak gracz padnie z 2/3 razy"). During it no
// bot takes the person on for any of the protocol's reasons, and only a blow
// aimed at a bot ends it: one at the person's selected target, not the graze
// of an area skill cast at something else.
//
// This file is the words and the counting; the engine side is in
// playerbot_anti_pk.h and the whisper arrives through playerbot_chat_trade.h.
// Folding is the caller's, as for the lure order: CP1250 to lowercase ASCII,
// the Polish letters on their base. Tested in
// tests/playerbot_truce_rules_test.cpp.

#include <cstring>

namespace playerbot_truce_rules
{
	enum ESurrender
	{
		SURRENDER_NONE = 0,
		// Words that can only mean giving up: taken from anybody.
		SURRENDER_PLAIN,
		// Words that mean it only from somebody the bots are fighting ("dosc",
		// "przepraszam"): from anybody else they are conversation, or the stop
		// of a lure order.
		SURRENDER_IN_A_FIGHT
	};

	inline bool IsWordChar(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
	}

	// The phrase as whole words - "odpusc" in "odpusc mi", not in "odpuscilem"
	// unless the entry ends in '*', which makes it a stem - and not right after
	// a "nie", which turns "poddaje sie" into defiance.
	inline bool Heard(const char* folded, const char* entry)
	{
		char phrase[48];
		size_t n = strlen(entry);
		if (n == 0 || n >= sizeof(phrase))
			return false;
		const bool stem = entry[n - 1] == '*';
		if (stem)
			--n;
		memcpy(phrase, entry, n);
		phrase[n] = 0;
		for (const char* p = strstr(folded, phrase); p; p = strstr(p + 1, phrase))
		{
			if (p != folded && IsWordChar(p[-1]))
				continue;
			if (!stem && IsWordChar(p[n]))
				continue;
			const size_t before = (size_t)(p - folded);
			if (before >= 4 && strncmp(p - 4, "nie ", 4) == 0 && (before == 4 || !IsWordChar(p[-5])))
				continue;
			return true;
		}
		return false;
	}

	inline ESurrender ParseSurrender(const char* folded)
	{
		if (!folded || !*folded)
			return SURRENDER_NONE;
		// A line about luring is the lure order's ("odpusc lurowanie",
		// "przestan lurowac"), whatever else is in it.
		if (strstr(folded, "lur"))
			return SURRENDER_NONE;
		// "poddaj sie" is somebody telling the bot to give up, and "poddajesz
		// sie?" asking it - neither is on the list, so neither is a surrender.
		static const char* kPlain[] = {
			"poddaje", "poddajemy", "poddam", "poddamy", "rozejm*", "odpusc*", "odpuszczam",
			"litosc*", "mam dosc", "mamy dosc", "biala flaga", "biala flage", "daj mi spokoj",
			"dajcie mi spokoj", "daj spokoj", "dajcie spokoj", "zostaw mnie", "zostawcie mnie",
			"przestancie", "koniec walki", "nie bij mnie", "nie bijcie", "surrender", "i give up",
			"truce"
		};
		static const char* kInAFight[] = {
			"dosc", "wystarczy", "stop", "przestan", "koniec", "zostaw", "przepraszam", "sorry",
			"sory", "sorki", "sorka", "wybacz", "wybaczcie", "gg"
		};
		for (size_t i = 0; i < sizeof(kPlain) / sizeof(kPlain[0]); ++i)
			if (Heard(folded, kPlain[i]))
				return SURRENDER_PLAIN;
		for (size_t i = 0; i < sizeof(kInAFight) / sizeof(kInAFight[0]); ++i)
			if (Heard(folded, kInAFight[i]))
				return SURRENDER_IN_A_FIGHT;
		return SURRENDER_NONE;
	}

	// A truce by the clock the game ticks in (milliseconds that wrap, compared
	// by difference). brokenAt is the moment the person's own blow ended the
	// last one: a surrender is not taken again for a while after it, or a
	// person could strike, whisper, strike again.
	struct TTruce
	{
		unsigned int until;
		unsigned int brokenAt;
		bool active;
		TTruce() : until(0), brokenAt(0), active(false) {}
	};

	inline bool IsActive(const TTruce& t, unsigned int now)
	{
		return t.active && (int)(t.until - now) > 0;
	}

	enum EAnswer
	{
		ANSWER_GRANTED = 0,
		ANSWER_ALREADY,
		ANSWER_REFUSED_AFTER_BREAK
	};

	inline unsigned int MinutesUp(unsigned int ms)
	{
		return (ms + 59999U) / 60000U;
	}

	// A person asked for one. minutes: how long the truce runs, or how long
	// until the person may ask again.
	inline EAnswer AskTruce(TTruce& t, unsigned int now, unsigned int truceMs, unsigned int refuseMs,
			unsigned int& minutes)
	{
		if (IsActive(t, now))
		{
			minutes = MinutesUp(t.until - now);
			return ANSWER_ALREADY;
		}
		if (t.brokenAt != 0 && now - t.brokenAt < refuseMs)
		{
			minutes = MinutesUp(refuseMs - (now - t.brokenAt));
			return ANSWER_REFUSED_AFTER_BREAK;
		}
		t.active = true;
		t.until = now + truceMs;
		t.brokenAt = 0;
		minutes = MinutesUp(truceMs);
		return ANSWER_GRANTED;
	}

	// The bots gave it, after the deaths: whatever the person did before.
	inline void GrantTruce(TTruce& t, unsigned int now, unsigned int truceMs)
	{
		t.active = true;
		t.until = now + truceMs;
		t.brokenAt = 0;
	}

	inline void BreakTruce(TTruce& t, unsigned int now)
	{
		t.active = false;
		t.until = 0;
		t.brokenAt = now != 0 ? now : 1;
	}

	// The person's deaths while bots fought them. Every bot fighting the
	// person sees the same death, so one within dedupMs of the last seen is
	// that one again - also after a tally was spent on it; a tally older than
	// windowMs starts over. True when this death reaches `deaths`, and the
	// tally is spent.
	struct TDeathTally
	{
		unsigned int firstAt;
		unsigned int lastAt;
		int count;
		bool seen;
		TDeathTally() : firstAt(0), lastAt(0), count(0), seen(false) {}
	};

	inline bool NoteDeath(TDeathTally& d, unsigned int now, unsigned int windowMs, unsigned int dedupMs,
			int deaths)
	{
		if (d.seen && now - d.lastAt < dedupMs)
			return false;
		d.seen = true;
		d.lastAt = now;
		if (d.count == 0 || now - d.firstAt >= windowMs)
		{
			d.firstAt = now;
			d.count = 0;
		}
		++d.count;
		if (d.count < deaths)
			return false;
		d.count = 0;
		return true;
	}
}

#endif
