#ifndef __INC_METIN2_PLAYERBOT_CONV_MEMORY_H__
#define __INC_METIN2_PLAYERBOT_CONV_MEMORY_H__

// PlayerBot Conversation v6 - conversation memory and context (pure).
//
// CONTEXT RESOLUTION -> CONVERSATION MEMORY -> PLAYER RELATIONSHIP
//
// One TConvMemory per (player, bot) pair. It holds:
//   - the last few turns (intent, subject, topic, text, time),
//   - the last game subject and how many small-talk turns happened since
//     (so "a gdzie teraz expisz?" after talking about winter reads as
//     "wracajac do tego..."),
//   - what the bot last asked the player ("A ty?") so that the answer is read
//     as an answer and not as a new question,
//   - the last reply and the templates used lately (no parroting),
//   - a small relationship ledger: lines, sessions, kindness, insults,
//     one thing the player said they like.
//
// Context expires (CONV_CONTEXT_TTL_MS); the relationship ledger lives much
// longer (CONV_MEMORY_TTL_MS) and is capped in count by the engine side.

#include "playerbot_conv_intents.h"

namespace playerbot_conv
{
	const u32 CONV_CONTEXT_TTL_MS = 3 * 60 * 1000;      // follow-ups older than this mean nothing
	const u32 CONV_RETURN_TTL_MS = 15 * 60 * 1000;      // "wracajac do tego" window
	const u32 CONV_SESSION_GAP_MS = 20 * 60 * 1000;     // a new conversation starts after this
	const u32 CONV_MEMORY_TTL_MS = 3 * 60 * 60 * 1000;  // the pair is forgotten after this
	const u32 CONV_BOT_ASK_TTL_MS = 90 * 1000;          // an answer to "a ty?" is expected this long
	const size_t CONV_TURNS = 4;
	const size_t CONV_RECENT_TEMPLATES = 12;

	enum EBotAsk
	{
		ASK_NONE = 0,
		ASK_HOW_ARE_YOU,   // "A u ciebie jak?"
		ASK_ACTIVITY,      // "A ty co robisz?"
		ASK_TOPIC,         // "A ty lubisz zime?" - botAskTopic says which
		ASK_JOIN,          // "Idziesz na exp?"
		ASK_FOUND,         // "Znalazles cos ciekawego?"
		ASK_SUMMON         // "Po co mam przyjsc?" - a stranger called the bot over
	};

	enum ETier
	{
		TIER_HOSTILE = 0,
		TIER_STRANGER,
		TIER_KNOWN,
		TIER_FRIEND,
		TIER_BUDDY
	};

	inline const char* TierName(int t)
	{
		switch (t)
		{
			case TIER_HOSTILE: return "HOSTILE";
			case TIER_STRANGER: return "STRANGER";
			case TIER_KNOWN: return "KNOWN";
			case TIER_FRIEND: return "FRIEND";
			default: return "BUDDY";
		}
	}

	struct TTurn
	{
		EIntent intent;
		EIntent subject;
		ETopic topic;
		EFollow follow;
		EQType qtype;
		bool question;
		u32 at;
		std::string norm;
		std::string object;
		TTurn() : intent(I_NONE), subject(I_NONE), topic(T_NONE), follow(F_NONE), qtype(Q_STATEMENT),
			question(false), at(0) {}
	};

	struct TConvMemory
	{
		u32 playerPID;
		u32 botPID;
		u32 firstAt;
		u32 lastPlayerAt;
		u32 lastBotAt;
		u32 lastInitiativeAt;
		u32 lastCheckAt;
		u32 talks;
		u32 sessions;
		int positive;
		int negative;

		TTurn turns[CONV_TURNS]; // [0] newest
		size_t turnCount;

		EIntent lastGameIntent;
		u32 lastGameAt;
		int generalStreak;
		ETopic lastTopic;
		u32 lastTopicAt;

		EIntent lastAnswered;
		u32 lastAnsweredAt;
		std::string lastReply;
		std::string lastReason;   // the "because" of the last answer, for "dlaczego?"

		unsigned char botAsk;
		ETopic botAskTopic;
		u32 botAskAt;

		u32 recentTemplates[CONV_RECENT_TEMPLATES];
		size_t recentIndex;

		u32 moodMentionAt;
		u32 greetedAt;
		std::string playerLikes;
		std::string playerDislikes;
		int lastKnownLevel;
		u32 repeatCount;

		TConvMemory() : playerPID(0), botPID(0), firstAt(0), lastPlayerAt(0), lastBotAt(0),
			lastInitiativeAt(0), lastCheckAt(0), talks(0), sessions(0), positive(0), negative(0),
			turnCount(0), lastGameIntent(I_NONE), lastGameAt(0), generalStreak(0), lastTopic(T_NONE),
			lastTopicAt(0), lastAnswered(I_NONE), lastAnsweredAt(0), botAsk(ASK_NONE),
			botAskTopic(T_NONE), botAskAt(0), recentIndex(0), moodMentionAt(0), greetedAt(0),
			lastKnownLevel(0), repeatCount(0)
		{
			for (size_t i = 0; i < CONV_RECENT_TEMPLATES; ++i)
				recentTemplates[i] = 0;
		}

		const TTurn* Prev(u32 now, size_t back = 0) const
		{
			if (back >= turnCount)
				return NULL;
			const TTurn& t = turns[back];
			if (t.at == 0 || now - t.at > CONV_CONTEXT_TTL_MS)
				return NULL;
			return &t;
		}

		bool UsedTemplateRecently(u32 id) const
		{
			for (size_t i = 0; i < CONV_RECENT_TEMPLATES; ++i)
				if (recentTemplates[i] == id)
					return true;
			return false;
		}

		void NoteTemplate(u32 id)
		{
			recentTemplates[recentIndex % CONV_RECENT_TEMPLATES] = id;
			++recentIndex;
		}

		void ClearContext()
		{
			turnCount = 0;
			lastGameIntent = I_NONE;
			lastGameAt = 0;
			generalStreak = 0;
			lastTopic = T_NONE;
			botAsk = ASK_NONE;
			lastAnswered = I_NONE;
			lastReason.clear();
		}
	};

	// What a resolved turn is "about" - the intent itself, or what a
	// follow-up was about.
	inline EIntent TurnSubject(const TTurn& t)
	{
		if (t.intent == I_FOLLOW_UP || t.intent == I_ANSWER_TO_BOT)
			return t.subject;
		return t.intent;
	}

	// The most recent turn that was about something (a bare "ok" or "xD" does
	// not reset what the conversation is about).
	inline const TTurn* PrevMeaningful(const TConvMemory& m, u32 now)
	{
		for (size_t i = 0; i < m.turnCount; ++i)
		{
			const TTurn* t = m.Prev(now, i);
			if (!t)
				return NULL;
			if (IsReactionIntent(t->intent))
				continue;
			if (TurnSubject(*t) == I_NONE && t->topic == T_NONE)
				continue;
			return t;
		}
		return NULL;
	}

	inline bool IsActivityish(EIntent i)
	{
		return i == I_ACTIVITY || i == I_ACTIVITY_LOCATION || i == I_LOCATION || i == I_TARGET ||
				i == I_MOB_COUNT || i == I_METIN || i == I_TIME_HERE || i == I_MAP_OPINION ||
				i == I_BIOLOGIST || i == I_MISSIONS || i == I_DROP_LUCK;
	}

	inline bool IsQuestionLine(const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		return a.question || c.Has(C_WHAT) || c.Has(C_WHERE) || c.Has(C_WHY) || c.Has(C_WHO) ||
				c.Has(C_WHICH) || c.Has(C_HOWMUCH) || c.Has(C_WHEN) || a.tokens.Has("czy") ||
				(c.Has(C_HOW) && !c.Has(C_HOWAREYOU));
	}

	// ------------------------------------------------------ context resolver

	inline void ResolveContext(TAnalysis& a, const TConvMemory& m, u32 now)
	{
		const TTurn* prev = PrevMeaningful(m, now);
		const EIntent base = prev ? TurnSubject(*prev) : I_NONE;
		const ETopic baseTopic = prev ? prev->topic : T_NONE;
		const bool baseGeneral = base == I_GENERAL || (base == I_NONE && baseTopic != T_NONE);

		// The same line again within a minute.
		for (size_t i = 0; i < m.turnCount && i < 3; ++i)
		{
			const TTurn* t = m.Prev(now, i);
			if (t && now - t->at < 60000 && a.tokens.norm.size() > 2 && t->norm == a.tokens.norm)
				a.repeated = true;
		}

		// An answer to what the bot asked. Only when the line is not a new
		// question and not one of the fixed social moves. The summon and its
		// release are moves of their own too ("chodz tu" again, "mozesz isc");
		// but "pomoz mi" after "Po co mam przyjsc?" is the reason asked for,
		// which the party words would otherwise take for an invitation. And
		// the reason often sounds like a question ("pokaze ci cos", "pomozesz
		// mi z metinem?"): after that question only one about the bot itself
		// ("jaki masz lvl?") is a new question.
		const bool newQuestion = IsQuestionLine(a) &&
				(m.botAsk != ASK_SUMMON || IsGameIntent((EIntent)a.intent));
		if (m.botAsk != ASK_NONE && now - m.botAskAt < CONV_BOT_ASK_TTL_MS && !newQuestion &&
				a.intent != I_FAREWELL && a.intent != I_GREETING && a.intent != I_INSULT &&
				a.intent != I_BUY && a.intent != I_SELL &&
				(a.intent != I_PARTY_REQUEST || m.botAsk == ASK_SUMMON) &&
				a.intent != I_SUMMON && a.intent != I_DISMISS && a.intent != I_THANKS)
		{
			a.subject = (EIntent)a.intent;
			a.intent = I_ANSWER_TO_BOT;
			a.answeredAsk = (unsigned char)m.botAsk;
			if (a.topic == T_NONE && m.botAsk == ASK_TOPIC)
				a.topic = m.botAskTopic;
			return;
		}

		// "a za 3kk?" after talking about an item: an offer for that item.
		if (a.offerYang > 0 && a.object.empty() && prev && !prev->object.empty() &&
				(base == I_BUY || base == I_SHOP || base == I_PRICE || base == I_ITEM_OWN) &&
				a.intent != I_SELL && a.intent != I_GOLD && a.tokens.words.size() <= 5)
		{
			a.intent = I_BUY;
			a.subject = I_BUY;
			a.object = prev->object;
			return;
		}

		if (a.intent == I_FOLLOW_UP)
		{
			a.subject = base;
			switch (a.follow)
			{
				case F_WHERE:
					if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else if (base == I_NEXT_PLAN || base == I_GOAL) a.intent = I_NEXT_PLAN;
					else if (base == I_TRAVEL || base == I_REST) a.intent = I_TRAVEL;
					else if (base == I_GUILD || base == I_PARTY) a.intent = I_LOCATION;
					else if (base == I_SHOP || base == I_MARKET || base == I_BUY || base == I_SELL) a.intent = I_SHOP;
					else if (base == I_FISHING || base == I_MINING) a.intent = I_LOCATION;
					else a.intent = I_ACTIVITY_LOCATION;
					break;
				case F_COUNT:
					if (IsActivityish(base) || base == I_ACTIVITY) a.intent = I_MOB_COUNT;
					else if (base == I_GOLD || base == I_SHOP || base == I_MARKET) a.intent = I_GOLD;
					else if (base == I_PARTY || base == I_PARTY_REQUEST) a.intent = I_PARTY;
					else if (base == I_GUILD || base == I_GUILD_WAR) a.intent = I_GUILD;
					else if (base == I_INVENTORY || base == I_INVENTORY_SPACE || base == I_ITEM_OWN ||
							base == I_EQUIPMENT) a.intent = I_INVENTORY_SPACE;
					else if (base == I_LEVEL || base == I_PROGRESS_TODAY) a.intent = base;
					else if (base == I_HP || base == I_FISHING || base == I_DEATH || base == I_HORSE ||
							base == I_SKILLS || base == I_BUFFS) a.intent = base;
					else if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_FOLLOW_UP; // "ale czego?"
					break;
				case F_ALONE:
				case F_WITHWHO:
					if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_PARTY;
					break;
				case F_NEXT:
					if (baseGeneral) { a.intent = I_FOLLOW_UP; }
					else a.intent = I_NEXT_PLAN;
					break;
				case F_WHAT:
					if (base == I_ACTIVITY || base == I_ACTIVITY_LOCATION || base == I_MOB_COUNT) a.intent = I_TARGET;
					else a.intent = I_FOLLOW_UP;
					break;
				case F_WHO:
					if (base == I_PARTY || base == I_GUILD || base == I_PARTY_REQUEST) a.intent = base;
					else if (base == I_TARGET || base == I_ACTIVITY) a.intent = I_TARGET;
					else a.intent = I_FOLLOW_UP;
					break;
				case F_THIS:
					if (base != I_NONE && base != I_GENERAL) a.intent = base;
					else if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_FOLLOW_UP;
					break;
				case F_MIRROR:
				{
					// "a ty?" - the bot's own take on what the player just said.
					const TTurn* last = m.Prev(now, 0);
					if (last && (last->topic != T_NONE) && last->intent != I_FOLLOW_UP)
					{
						a.intent = I_GENERAL;
						a.topic = last->topic;
						a.qtype = Q_MIRROR;
						a.object = last->object;
					}
					else if (last && IsGameIntent(TurnSubject(*last)))
						a.intent = TurnSubject(*last);
					else
						a.intent = I_HOW_ARE_YOU;
					break;
				}
				default:
					// WHY / HOW / CONFIRM / WHEN stay follow-ups about `base`.
					break;
			}
			if (a.intent != I_FOLLOW_UP && a.intent != I_GENERAL)
				a.subject = a.intent;
		}

		// Back to the game after small talk.
		// Only to the same subject: talking about winter and then asking for yang
		// is a new question, not "wracajac do tego".
		if (IsGameIntent(a.intent) && m.generalStreak >= 1 && m.lastGameIntent != I_NONE &&
				now - m.lastGameAt < CONV_RETURN_TTL_MS &&
				GameTopicGroup(a.intent) == GameTopicGroup(m.lastGameIntent))
			a.returnToTopic = true;
		// Away from the game.
		if (a.intent == I_GENERAL && prev && IsGameIntent(base))
			a.topicChange = true;
	}

	// ---------------------------------------------------------------- update

	inline void BeginPlayerLine(TConvMemory& m, u32 now)
	{
		if (m.firstAt == 0)
			m.firstAt = now;
		if (m.lastPlayerAt == 0 || now - m.lastPlayerAt > CONV_SESSION_GAP_MS)
		{
			++m.sessions;
			m.ClearContext();
		}
		else if (now - m.lastPlayerAt > CONV_CONTEXT_TTL_MS)
			m.ClearContext();
	}

	inline void RememberPlayerLine(TConvMemory& m, const TAnalysis& a, u32 now)
	{
		for (size_t i = CONV_TURNS - 1; i > 0; --i)
			m.turns[i] = m.turns[i - 1];
		TTurn& t = m.turns[0];
		t = TTurn();
		t.intent = a.intent;
		t.subject = a.subject;
		t.topic = a.topic;
		t.follow = a.follow;
		t.qtype = a.qtype;
		t.question = a.question;
		t.at = now;
		t.norm = a.tokens.norm;
		t.object = a.object;
		if (m.turnCount < CONV_TURNS)
			++m.turnCount;

		++m.talks;
		m.lastPlayerAt = now;
		const EIntent subj = a.intent == I_FOLLOW_UP ? a.subject : a.intent;
		if (IsGameIntent(subj))
		{
			m.lastGameIntent = subj;
			m.lastGameAt = now;
			m.generalStreak = 0;
		}
		else if (a.intent == I_GENERAL || (a.intent == I_ANSWER_TO_BOT && a.topic != T_NONE) ||
				(a.intent == I_UNKNOWN_STATEMENT && a.topic != T_NONE))
			++m.generalStreak;
		if (a.topic != T_NONE)
		{
			m.lastTopic = a.topic;
			m.lastTopicAt = now;
		}
		if (a.intent == I_INSULT)
			++m.negative;
		if (a.intent == I_THANKS || a.intent == I_PRAISE || a.thanksToo || a.intent == I_APOLOGY)
			++m.positive;
		if (a.intent == I_APOLOGY && m.negative > 0)
			--m.negative;
		if (a.repeated)
			++m.repeatCount;
		// "lubie zime" - one thing to remember about the person.
		const int lubie = a.tokens.Find("lubie");
		if (lubie >= 0)
		{
			const std::string obj = ExtractObjectAfter(a.tokens, lubie, 2);
			if (!obj.empty())
			{
				if (lubie > 0 && a.tokens.words[lubie - 1] == "nie")
					m.playerDislikes = obj;
				else
					m.playerLikes = obj;
			}
		}
		// An answer closes the question; anything else lets it expire.
		if (a.intent == I_ANSWER_TO_BOT || now - m.botAskAt > CONV_BOT_ASK_TTL_MS)
			m.botAsk = ASK_NONE;
	}

	inline void RememberBotReply(TConvMemory& m, EIntent answered, const std::string& reply, u32 now)
	{
		m.lastAnswered = answered;
		m.lastAnsweredAt = now;
		m.lastReply = reply;
		m.lastBotAt = now;
	}

	// How well the bot knows the person. `affinity` is the AI's own friend
	// ledger (party, gifts, trade - playerbot_guild.h), `sameParty` whether
	// they are in one party right now.
	inline int ComputeTier(const TConvMemory& m, int affinity, bool sameParty)
	{
		if (m.negative >= 3 && m.negative > m.positive + 1)
			return TIER_HOSTILE;
		int score = affinity;
		score += (int)(m.talks > 40 ? 40 : m.talks) / 2;
		score += (int)(m.sessions > 10 ? 10 : m.sessions) * 3;
		score += m.positive * 2 - m.negative * 6;
		if (sameParty)
			score += 15;
		if (score < 6) return TIER_STRANGER;
		if (score < 25) return TIER_KNOWN;
		if (score < 55) return TIER_FRIEND;
		return TIER_BUDDY;
	}
}

#endif
