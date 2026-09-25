#ifndef __INC_METIN2_PLAYERBOT_CONV_ENGINE_H__
#define __INC_METIN2_PLAYERBOT_CONV_ENGINE_H__

// PlayerBot Conversation v6 - the conversation engine (pure).
//
// MESSAGE -> ... -> RESPONSE GENERATOR -> RESPONSE MERGER -> REPLY QUEUE
//   -> DELAY ~0.7-1.5 s -> SEND
//
// Every whisper is ANALYSED THE MOMENT IT ARRIVES (so "gdzie?" after "co
// robisz?" is resolved against the right context), and QUEUED for an answer.
// Nothing is ever dropped because of a cooldown.
//
// The queue, per (player, bot):
//   - the first line of a burst is due 700..1150 ms after it arrived,
//   - lines arriving while one is pending join it; the reply is pushed back by
//     at most ~350 ms per line, never later than 1500 ms after the FIRST line,
//     so "co robisz? / gdzie? / duzo mobow? / jaki lvl?" typed in one second
//     becomes ONE whisper: "Wlasnie expie w Dolinie Orkow. Troche ich jest,
//     ale spot jest spokojny. Mam 42 poziom.",
//   - a line typed after the bot answered starts a new reply of its own
//     (normal back-and-forth stays back-and-forth),
//   - at most CONV_QUEUE_MAX lines wait; duplicates collapse, "ok"/"xD" are
//     dropped first when questions are pending, the oldest go last,
//   - a burst (CONV_SPAM_LINES in CONV_BURST_WINDOW_MS) is spam: still one
//     merged answer to the first few distinct questions, never twenty,
//   - a reply too long for one whisper is split in two, the second ~1 s later.
//
// The engine side calls OnPlayerLine() from the whisper hook and Pump() from a
// timer (and the manager's tick as a fallback). Nothing blocks, nothing sleeps.

#include "playerbot_conv_generator.h"
#include <map>
#include <set>
#include <algorithm>

namespace playerbot_conv
{
	const size_t CONV_QUEUE_MAX = 6;
	const size_t CONV_MAX_PIECES = 4;
	const size_t CONV_SPAM_PIECES = 3;
	const u32 CONV_DELAY_MIN_MS = 700;
	const u32 CONV_DELAY_SPREAD_MS = 450;
	const u32 CONV_DELAY_CAP_MS = 1500;
	const u32 CONV_JOIN_PUSH_MS = 350;
	const u32 CONV_MIN_GAP_MS = 900;
	const u32 CONV_SPLIT_DELAY_MS = 1000;
	const u32 CONV_BURST_WINDOW_MS = 4000;
	const u32 CONV_SPAM_LINES = 6;
	const size_t CONV_SPLIT_AT = 190;
	const size_t CONV_MAX_PAIRS = 4096;
	const u32 CONV_PRUNE_INTERVAL_MS = 60 * 1000;
	const u32 CONV_INITIATIVE_INTERVAL_MS = 5 * 1000;
	const u32 CONV_INITIATIVE_PAIR_GAP_MS = 6 * 60 * 1000;
	const u32 CONV_INITIATIVE_GLOBAL_GAP_MS = 15 * 1000;
	const u32 CONV_INITIATIVE_CHECK_MS = 20 * 1000;
	const u32 CONV_MOOD_MENTION_MS = 10 * 60 * 1000;

	struct TConvQueue
	{
		std::vector<TAnalysis> items;
		u32 firstAt;
		u32 dueAt;
		u32 lastSentAt;
		u32 burstStart;
		u32 burstCount;
		bool spam;
		std::string carry;
		u32 carryDue;
		u32 dropped;

		TConvQueue() : firstAt(0), dueAt(0), lastSentAt(0), burstStart(0), burstCount(0),
			spam(false), carryDue(0), dropped(0) {}

		bool Pending() const { return !items.empty() || !carry.empty(); }
	};

	struct TConvPair
	{
		TConvMemory mem;
		TConvQueue queue;
	};

	class IConvHost
	{
		public:
			virtual ~IConvHost() {}
			// False when one side is gone (logged out, warped away): the queue is dropped.
			virtual bool BuildSnapshot(u32 playerPID, u32 botPID, TBotSnapshot& out) = 0;
			virtual IConvWorld* World(u32 playerPID, u32 botPID) = 0;
			virtual void Send(u32 playerPID, u32 botPID, const std::string& text) = 0;
			virtual void Log(const std::string& line) = 0;
	};

	// ---------------------------------------------------------- queue policy

	inline int QueuePriority(const TAnalysis& a)
	{
		if (IsReactionIntent(a.intent)) return 0;
		if (a.intent == I_GREETING || a.intent == I_THANKS || a.intent == I_UNKNOWN_STATEMENT) return 1;
		if (a.intent == I_FOLLOW_UP && a.follow == F_CONFIRM) return 1;
		return 2;
	}

	inline bool SameQuestion(const TAnalysis& x, const TAnalysis& y)
	{
		if (x.intent != y.intent)
			return false;
		if (x.intent == I_GENERAL)
			return x.topic == y.topic && x.qtype == y.qtype && x.object == y.object;
		if (x.intent == I_FOLLOW_UP)
			return x.follow == y.follow && x.subject == y.subject;
		if (x.intent == I_ITEM_OWN || x.intent == I_BUY || x.intent == I_SELL)
			return x.object == y.object;
		if (x.intent == I_MATH)
			return x.mathText == y.mathText && x.tokens.norm == y.tokens.norm;
		// Two lines of one argument are two points, not one question twice.
		if (x.intent == I_UNKNOWN_QUESTION || x.intent == I_UNKNOWN_STATEMENT || IsArgumentIntent(x.intent) ||
				IsColdIntent(x.intent))
			return x.tokens.norm == y.tokens.norm;
		return true;
	}

	// A new line into the queue. Returns the delay (ms from now) the queue is due in.
	inline u32 QueueLine(TConvQueue& q, const TAnalysis& a, u32 now, TRng& rng)
	{
		// Burst tracking for the spam guard.
		if (q.burstStart == 0 || now - q.burstStart > CONV_BURST_WINDOW_MS)
		{
			q.burstStart = now;
			q.burstCount = 0;
			q.spam = false;
		}
		++q.burstCount;
		if (q.burstCount >= CONV_SPAM_LINES)
			q.spam = true;

		if (q.items.empty())
		{
			q.firstAt = now;
			q.dueAt = now + CONV_DELAY_MIN_MS + rng.Range(CONV_DELAY_SPREAD_MS + 1);
			// Right after a reply (or while its second half waits), leave a gap.
			u32 floor = 0;
			if (q.lastSentAt != 0 && now - q.lastSentAt < CONV_MIN_GAP_MS)
				floor = q.lastSentAt + CONV_MIN_GAP_MS;
			if (!q.carry.empty())
				floor = std::max(floor, q.carryDue + CONV_MIN_GAP_MS);
			if (floor && (int)(floor - q.dueAt) > 0)
				q.dueAt = floor;
		}
		else
		{
			const u32 pushed = now + CONV_JOIN_PUSH_MS;
			if ((int)(pushed - q.dueAt) > 0)
				q.dueAt = pushed;
			const u32 cap = q.firstAt + CONV_DELAY_CAP_MS;
			if ((int)(q.dueAt - cap) > 0 && (int)(cap - now) > 0)
				q.dueAt = cap;
			else if ((int)(q.dueAt - cap) > 0)
				q.dueAt = now + 150;
		}

		// The same question twice collapses into the newer copy.
		for (size_t i = 0; i < q.items.size(); ++i)
		{
			if (SameQuestion(q.items[i], a))
			{
				q.items.erase(q.items.begin() + i);
				break;
			}
		}
		// In a spam burst, lines past the cap are counted and ignored.
		if (q.spam && q.items.size() >= CONV_QUEUE_MAX)
		{
			++q.dropped;
			return q.dueAt - now;
		}
		q.items.push_back(a);
		while (q.items.size() > CONV_QUEUE_MAX)
		{
			size_t victim = 0;
			int lowest = 99;
			for (size_t i = 0; i < q.items.size() - 1; ++i)
			{
				const int p = QueuePriority(q.items[i]);
				if (p < lowest)
				{
					lowest = p;
					victim = i;
				}
			}
			q.items.erase(q.items.begin() + victim);
			++q.dropped;
		}
		return (int)(q.dueAt - now) > 0 ? q.dueAt - now : 0;
	}

	// ------------------------------------------------------------ the merger

	inline bool IsCommonStarter(const std::string& s)
	{
		static const char* const kWords[] = {
			"Jestem", "Wlasnie", "Bije", "Expie", "Teraz", "Mam", "Tak", "Nie", "Aktualnie", "Siedze",
			"Tutaj", "Tu", "W", "Na", "Z", "Ide", "Jade", "Troche", "Sporo", "Duzo", "Malo", "Pusto",
			"Zaraz", "Najpierw", "Potem", "Pozniej", "Dalej", "Ogolnie", "Moj", "Glownie", "Rozbijam",
			"Lowie", "Kopie", "Stoje", "Odpoczywam", "Zbieram", "Robie", "Kupuje", "Ulepszam", "Czytam",
			"Chodze", "Krece", "Rozgladam", "Nic", "Gram", "Prawie", "Okolo", "Sam", "Solo", "Prowadze",
			"Przeciez", "Liderem", "Pelne", "Dobrze", "Bardzo", "Moze", "Dopiero", "Kiepsko", "Swietnie",
			"Calkiem", "Normalnie", "Lubie", "Chyba", "Jeszcze", "Zmierzam", "Przemieszczam", "Leca",
			"Walcze", "Kilka", "Jest", "Jak", "Nigdzie", "Wolnych", "Zero", "Nosze", "Poluje",
			// "A wracajac do gry, Ciezko powiedziec..." kept its capital.
			"Ciezko", "Wiem", "Nikt", "Zalezy", "Raczej", "Pewnie", "Ostatnio", "Bylem", "Wczesniej",
			"Kosztuje", "Stoi", "Wychodzi", "Juz", "Srednio"
		};
		size_t end = s.find_first_of(" ,.!?");
		const std::string first = s.substr(0, end);
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (first == kWords[i])
				return true;
		return false;
	}

	inline std::string LowerStarter(const std::string& s)
	{
		std::string out = s;
		if (!out.empty() && IsCommonStarter(out) && out[0] >= 'A' && out[0] <= 'Z')
			out[0] = (char)(out[0] - 'A' + 'a');
		return out;
	}

	// Split a long reply at the sentence boundary nearest the middle.
	inline void SplitReply(const std::string& text, std::string& first, std::string& second)
	{
		first = text;
		second.clear();
		if (text.size() <= CONV_SPLIT_AT)
			return;
		size_t best = std::string::npos;
		size_t bestDist = (size_t)-1;
		const size_t mid = text.size() / 2;
		for (size_t i = 1; i + 1 < text.size(); ++i)
		{
			if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && text[i + 1] == ' ')
			{
				const size_t d = i > mid ? i - mid : mid - i;
				if (d < bestDist)
				{
					bestDist = d;
					best = i;
				}
			}
		}
		if (best == std::string::npos)
			return;
		first = text.substr(0, best + 1);
		second = text.substr(best + 2);
	}

	struct TComposeResult
	{
		std::string text;
		EIntent lastIntent;
		size_t answered;
		size_t skipped;
		TComposeResult() : lastIntent(I_NONE), answered(0), skipped(0) {}
	};

	inline TComposeResult ComposeReply(std::vector<TAnalysis>& items, const TBotSnapshot& snap,
			TConvMemory& mem, IConvWorld* world, TRng& rng, u32 now, bool spam)
	{
		TComposeResult res;
		TGen g(snap, mem, rng, world, now);
		g.tier = ComputeTier(mem, snap.affinity, snap.askerInParty);

		// What is worth answering: reactions only when nothing else is asked,
		// a greeting folded into a short "Hej!" when a question comes with it.
		bool anyQuestion = false;
		bool greet = false;
		bool thanks = false;
		// Told to stop writing, threatened, mocked: the reply is short and
		// warm-free - no "Hej!", no mood, no question back.
		bool cold = IsQuiet(mem, now);
		// Argued with, or answering the bot's own question: no mood tail
		// either ("Dobra, innym razem :) Humor mi dzis dopisuje!") - nor on a
		// plain "co z twoja bronia?" in the middle of that argument.
		bool argued = IsArgumentIntent(mem.lastAnswered) && now - mem.lastAnsweredAt < CONV_CONTEXT_TTL_MS;
		for (size_t i = 0; i < items.size(); ++i)
		{
			const EIntent it = items[i].intent;
			if (!IsReactionIntent(it) && it != I_GREETING && it != I_THANKS)
				anyQuestion = true;
			if (it == I_GREETING || items[i].greetingToo)
				greet = true;
			if (it == I_THANKS || items[i].thanksToo)
				thanks = true;
			if (IsColdIntent(it))
				cold = true;
			if (IsArgumentIntent(it) || it == I_ANSWER_TO_BOT || it == I_MATH)
				argued = true;
		}
		std::vector<const TAnalysis*> todo;
		for (size_t i = 0; i < items.size(); ++i)
		{
			const TAnalysis& a = items[i];
			if (anyQuestion && (IsReactionIntent(a.intent) || a.intent == I_GREETING || a.intent == I_THANKS))
			{
				++res.skipped;
				continue;
			}
			bool dup = false;
			for (size_t k = 0; k < todo.size(); ++k)
				if (SameQuestion(*todo[k], a))
					dup = true;
			if (dup)
			{
				++res.skipped;
				continue;
			}
			todo.push_back(&a);
		}
		const size_t maxPieces = spam ? CONV_SPAM_PIECES : CONV_MAX_PIECES;
		if (todo.size() > maxPieces)
		{
			res.skipped += todo.size() - maxPieces;
			todo.resize(maxPieces);
		}
		g.groupSize = todo.size();
		for (size_t i = 0; i < todo.size(); ++i)
			if (todo[i]->intent == I_ACTIVITY)
				g.groupHasActivity = true;


		std::string out;
		if (spam && items.size() >= 5)
		{
			static const char* const k[] = { "Spokojnie, nie nadazam pisac :D", "Po kolei, po kolei :)", "Wolniej troche :D" };
			out = PBC_SAY(g, k);
		}
		if (anyQuestion && greet && !cold)
		{
			Append(out, GenGreeting(g, true));
		}
		if (anyQuestion && thanks && !greet && !cold)
			Append(out, "Spoko.");

		bool returned = false;
		for (size_t i = 0; i < todo.size(); ++i)
		{
			const TAnalysis& a = *todo[i];
			g.index = i;
			if ((a.intent == I_LOCATION || a.intent == I_ACTIVITY_LOCATION) && g.saidMap && g.saidActivity)
			{
				++res.skipped;
				continue;
			}
			std::string piece = GenerateOne(g, a);
			if (piece.empty())
				continue;
			CapitalizeFirst(piece);
			if (a.repeated && mem.repeatCount >= 1 && g.rng.Chance(60))
				piece = (mem.repeatCount >= 3 ? "Przeciez pisalem :) " : "Jak mowilem, ") + LowerStarter(piece);
			else if (a.returnToTopic && !returned && IsGameIntent(a.intent) && g.rng.Chance(75))
			{
				static const char* const k[] = { "Wracajac do tego, ", "A wracajac do gry, ", "Wracajac do tematu - " };
				piece = Pick(g, k, 3) + LowerStarter(piece);
				returned = true;
			}
			if (out.find(piece) != std::string::npos)
				continue;
			Append(out, piece);
			++res.answered;
			res.lastIntent = a.intent;
		}

		// Nothing to say (a laugh let pass, an ignored insult).
		if (out.empty())
		{
			if (!todo.empty() && !anyQuestion && greet)
				out = GenGreeting(g, false);
			if (out.empty())
				return res;
		}

		// The mood shows, sometimes - never after a jibe ("daleko zajdziesz"
		// answered with "Humor mi dzis dopisuje!" was a non sequitur).
		if (!spam && !cold && !argued && res.answered > 0 && (mem.moodMentionAt == 0 || now - mem.moodMentionAt > CONV_MOOD_MENTION_MS))
		{
			bool moodSaid = false;
			for (size_t i = 0; i < todo.size(); ++i)
				if (todo[i]->intent == I_MOOD || todo[i]->intent == I_HOW_ARE_YOU || todo[i]->intent == I_DROP_LUCK)
					moodSaid = true;
			if (!moodSaid && g.Bad() && g.rng.Chance(30))
			{
				Append(out, g.s.unlucky ? "Swoja droga, dzis jakis pech mnie trzyma." : "Dzis jakos slabo mi idzie, swoja droga.");
				mem.moodMentionAt = now;
			}
			else if (!moodSaid && (g.Good() || g.s.euphoria) && g.rng.Chance(15))
			{
				Append(out, "Humor mi dzis dopisuje!");
				mem.moodMentionAt = now;
			}
			else if (moodSaid)
				mem.moodMentionAt = now;
		}

		// One question back, only in a short reply. "Po co mam przyjsc?" is the
		// one a cold reply still asks: without it the answer is not read as one.
		if (!g.askBack.empty() && res.answered <= 2 && !spam && (!cold || g.askBackKind == ASK_SUMMON))
		{
			Append(out, g.askBack);
			mem.botAsk = g.askBackKind;
			mem.botAskTopic = g.askBackTopic;
			mem.botAskAt = now;
		}
		if (!g.reason.empty())
			mem.lastReason = g.reason;
		else if (res.answered > 0)
			mem.lastReason.clear();
		mem.lastKnownLevel = snap.level;
		// The map a reply named is what a teleport since will be measured
		// against ("Bylem w Joan, teraz jestem juz w Dolinie Orkow").
		if (IsKnownMap(snap.mapIndex) && out.find(GetMapWords(snap.mapIndex).atShort) != std::string::npos)
			NoteSaidMap(mem, snap.mapIndex, now);
		// Generic answers in a row: the next one steers (GenUnknown*).
		if (res.answered > 0)
			mem.fallbackStreak = res.lastIntent == I_UNKNOWN_QUESTION || res.lastIntent == I_UNKNOWN_STATEMENT ?
					mem.fallbackStreak + 1 : 0;
		res.text = out;
		return res;
	}

	// ------------------------------------------------------------ initiative

	// The bot speaking first. Called only for pairs that have talked lately;
	// every condition is a reason to stay quiet.
	inline std::string ChooseInitiative(TConvMemory& m, const TBotSnapshot& s, u32 now, TRng& rng)
	{
		// "przestan do mnie pisac" is kept.
		if (IsQuiet(m, now))
			return std::string();
		const int tier = ComputeTier(m, s.affinity, s.askerInParty);
		if (tier < TIER_KNOWN || s.dead || s.afk)
			return std::string();
		if (!s.askerNear && tier < TIER_FRIEND)
			return std::string();
		if (s.action == A_FIGHT && s.hpPct < 40)
			return std::string(); // busy
		TGen g(s, m, rng, NULL, now);
		g.tier = tier;
		std::string out;
		unsigned char ask = ASK_NONE;
		if (m.lastKnownLevel > 0 && s.level > m.lastKnownLevel)
		{
			static const char* const k[] = { "Wbilem $LVL!", "O, $LVL poziom wpadl!" };
			out = PBC_SAY(g, k);
		}
		else if (s.bagCells > 0 && s.freeCells <= 1 && !s.inTown && rng.Chance(60))
		{
			static const char* const k[] = { "Chyba zaraz bede wracal do miasta, EQ mam pelne.", "EQ pelne, lece zaraz sprzedac drop." };
			out = PBC_SAY(g, k);
		}
		else if (s.action == A_RECOVER && s.hpPct < 40 && rng.Chance(50))
			out = "Musialem sie wycofac, prawie mnie ubili.";
		else if (s.askerNear && !s.inParty && !s.askerInParty && !s.shopStanding && rng.Chance(35))
		{
			static const char* const k[] = { "$PLAYER, idziesz na exp?", "$PLAYER, moze razem pobijemy?" };
			out = PBC_SAY(g, k);
			ask = ASK_JOIN;
		}
		else if (!m.playerLikes.empty() && rng.Chance(15))
		{
			out = Fill(g, "Ej $PLAYER, a ty dalej lubisz $LIKES?");
			ask = ASK_TOPIC;
		}
		else if (rng.Chance(20))
		{
			static const char* const k[] = { "$PLAYER, znalazles cos ciekawego?", "$PLAYER, jak tam drop?" };
			out = PBC_SAY(g, k);
			ask = ASK_FOUND;
		}
		else if ((g.Good() || s.euphoria) && rng.Chance(30))
			out = "Ale dzis leci drop!";
		if (out.empty())
			return out;
		CapitalizeFirst(out);
		m.lastInitiativeAt = now;
		m.lastBotAt = now;
		m.lastKnownLevel = s.level;
		if (ask != ASK_NONE)
		{
			m.botAsk = ask;
			m.botAskAt = now;
			m.botAskTopic = m.lastTopic;
		}
		return out;
	}

	// ---------------------------------------------------------------- engine

	class CConvEngine
	{
		public:
			typedef unsigned long long TKey;
			typedef std::map<TKey, TConvPair> TPairs;

			CConvEngine() : m_rng(0x2545F491u), m_lastPrune(0), m_lastInitiativeScan(0),
				m_lastInitiative(0), m_debug(false), m_initiative(true),
				m_statLines(0), m_statReplies(0), m_statMerged(0), m_statDropped(0) {}

			static TKey Key(u32 playerPID, u32 botPID)
			{
				return ((TKey)playerPID << 32) | (TKey)botPID;
			}

			void SetDebug(bool on) { m_debug = on; }
			bool Debug() const { return m_debug; }
			void SetInitiative(bool on) { m_initiative = on; }
			void Seed(u32 seed) { m_rng = TRng(seed); }

			size_t PairCount() const { return m_pairs.size(); }
			size_t PendingCount() const { return m_pending.size(); }
			TConvPair* FindPair(u32 playerPID, u32 botPID)
			{
				TPairs::iterator it = m_pairs.find(Key(playerPID, botPID));
				return it == m_pairs.end() ? NULL : &it->second;
			}

			// The whisper hook. Analyses at once, answers later.
			EIntent OnPlayerLine(IConvHost& host, u32 playerPID, u32 botPID, const char* text,
					u32 now, const char* playerName = NULL, const char* botName = NULL)
			{
				++m_statLines;
				TConvPair& pair = GetPair(playerPID, botPID, now);
				TConvMemory& mem = pair.mem;
				TAnalysis a;
				AnalyzeLine(text, a, now);
				a.gapBefore = mem.lastPlayerAt ? now - mem.lastPlayerAt : 0;
				BeginPlayerLine(mem, now);
				const TTurn* prev = mem.Prev(now, 0);
				const EIntent previous = prev ? TurnSubject(*prev) : I_NONE;
				ResolveContext(a, mem, now);
				RememberPlayerLine(mem, a, now);
				const bool wasEmpty = !pair.queue.Pending();
				const size_t droppedBefore = pair.queue.dropped;
				const u32 delay = QueueLine(pair.queue, a, now, m_rng);
				m_statDropped += pair.queue.dropped - droppedBefore;
				m_pending.insert(Key(playerPID, botPID));
				if (m_debug)
				{
					char line[512];
					snprintf(line, sizeof(line),
							"PLAYERBOT_CONV: player=%s bot=%s text=\"%s\" intent=%s raw=%s context=%s follow=%s previous=%s topic=%s qtype=%d return=%d repeated=%d",
							playerName ? playerName : "?", botName ? botName : "?", a.tokens.norm.c_str(),
							IntentName(a.intent), IntentName(a.rawIntent), IntentName(a.subject),
							FollowName(a.follow), IntentName(previous), TopicName(a.topic), (int)a.qtype,
							a.returnToTopic ? 1 : 0, a.repeated ? 1 : 0);
					host.Log(line);
					snprintf(line, sizeof(line), "PLAYERBOT_CONV_QUEUE: player=%s bot=%s queued=%u delay=%ums burst=%u spam=%d dropped=%u %s",
							playerName ? playerName : "?", botName ? botName : "?",
							(unsigned int)pair.queue.items.size(), delay, pair.queue.burstCount,
							pair.queue.spam ? 1 : 0, pair.queue.dropped, wasEmpty ? "new" : "joined");
					host.Log(line);
				}
				return a.intent;
			}

			// Due replies, the second halves, the housekeeping. Cheap when idle:
			// it walks only the pairs that have something pending.
			void Pump(IConvHost& host, u32 now)
			{
				if (!m_pending.empty())
				{
					std::vector<TKey> done;
					for (std::set<TKey>::iterator it = m_pending.begin(); it != m_pending.end(); ++it)
					{
						TPairs::iterator p = m_pairs.find(*it);
						if (p == m_pairs.end())
						{
							done.push_back(*it);
							continue;
						}
						if (!ServePair(host, p->first, p->second, now))
							done.push_back(*it);
					}
					for (size_t i = 0; i < done.size(); ++i)
						m_pending.erase(done[i]);
				}
				if (m_initiative && now - m_lastInitiativeScan >= CONV_INITIATIVE_INTERVAL_MS)
				{
					m_lastInitiativeScan = now;
					ScanInitiative(host, now);
				}
				if (now - m_lastPrune >= CONV_PRUNE_INTERVAL_MS)
				{
					m_lastPrune = now;
					Prune(now);
				}
			}

			bool HasPending() const { return !m_pending.empty(); }

			void Stats(u32& lines, u32& replies, u32& merged, u32& dropped) const
			{
				lines = m_statLines;
				replies = m_statReplies;
				merged = m_statMerged;
				dropped = m_statDropped;
			}

			void ForgetBot(u32 botPID)
			{
				for (TPairs::iterator it = m_pairs.begin(); it != m_pairs.end(); )
				{
					if ((u32)(it->first & 0xFFFFFFFFULL) == botPID)
					{
						m_pending.erase(it->first);
						m_pairs.erase(it++);
					}
					else
						++it;
				}
			}

		private:
			TConvPair& GetPair(u32 playerPID, u32 botPID, u32 now)
			{
				const TKey key = Key(playerPID, botPID);
				TPairs::iterator it = m_pairs.find(key);
				if (it != m_pairs.end())
					return it->second;
				if (m_pairs.size() >= CONV_MAX_PAIRS)
					EvictOldest(64);
				TConvPair& pair = m_pairs[key];
				pair.mem.playerPID = playerPID;
				pair.mem.botPID = botPID;
				pair.mem.firstAt = now;
				return pair;
			}

			void EvictOldest(size_t count)
			{
				std::vector<std::pair<u32, TKey> > ages;
				ages.reserve(m_pairs.size());
				for (TPairs::iterator it = m_pairs.begin(); it != m_pairs.end(); ++it)
					if (!it->second.queue.Pending())
						ages.push_back(std::make_pair(it->second.mem.lastPlayerAt, it->first));
				if (ages.empty())
					return;
				if (count > ages.size())
					count = ages.size();
				std::partial_sort(ages.begin(), ages.begin() + count, ages.end());
				for (size_t i = 0; i < count; ++i)
					m_pairs.erase(ages[i].second);
			}

			void Prune(u32 now)
			{
				for (TPairs::iterator it = m_pairs.begin(); it != m_pairs.end(); )
				{
					const TConvMemory& m = it->second.mem;
					if (!it->second.queue.Pending() && m.lastPlayerAt != 0 && now - m.lastPlayerAt > CONV_MEMORY_TTL_MS)
						m_pairs.erase(it++);
					else
						++it;
				}
				if (m_debug && m_statLines)
				{
					// Nothing to log to here - the host reads Stats().
				}
			}

			// True while the pair still has something pending.
			bool ServePair(IConvHost& host, TKey key, TConvPair& pair, u32 now)
			{
				TConvQueue& q = pair.queue;
				const u32 playerPID = (u32)(key >> 32);
				const u32 botPID = (u32)(key & 0xFFFFFFFFULL);

				if (!q.carry.empty() && (int)(now - q.carryDue) >= 0)
				{
					host.Send(playerPID, botPID, q.carry);
					q.carry.clear();
					q.lastSentAt = now;
					pair.mem.lastBotAt = now;
				}
				if (q.items.empty())
					return q.Pending();
				if ((int)(now - q.dueAt) < 0)
					return true;
				if (!q.carry.empty())
					return true; // the first half's second part goes first

				TBotSnapshot snap;
				if (!host.BuildSnapshot(playerPID, botPID, snap))
				{
					q.items.clear();
					q.carry.clear();
					return false;
				}
				const size_t count = q.items.size();
				TComposeResult res = ComposeReply(q.items, snap, pair.mem, host.World(playerPID, botPID),
						m_rng, now, q.spam);
				q.items.clear();
				if (res.text.empty())
					return q.Pending();
				std::string first, second;
				SplitReply(res.text, first, second);
				host.Send(playerPID, botPID, first);
				++m_statReplies;
				if (count > 1)
					++m_statMerged;
				q.lastSentAt = now;
				if (!second.empty())
				{
					q.carry = second;
					q.carryDue = now + CONV_SPLIT_DELAY_MS;
				}
				RememberBotReply(pair.mem, res.lastIntent, res.text, now);
				if (m_debug)
				{
					char line[768];
					snprintf(line, sizeof(line), "PLAYERBOT_CONV_REPLY: player=%s bot=%s lines=%u answered=%u skipped=%u tier=%s split=%d reply=\"%s\"",
							snap.askerName.c_str(), snap.name.c_str(), (unsigned int)count,
							(unsigned int)res.answered, (unsigned int)res.skipped,
							TierName(ComputeTier(pair.mem, snap.affinity, snap.askerInParty)),
							second.empty() ? 0 : 1, res.text.c_str());
					host.Log(line);
				}
				return q.Pending();
			}

			void ScanInitiative(IConvHost& host, u32 now)
			{
				if (m_lastInitiative != 0 && now - m_lastInitiative < CONV_INITIATIVE_GLOBAL_GAP_MS)
					return;
				for (TPairs::iterator it = m_pairs.begin(); it != m_pairs.end(); ++it)
				{
					TConvMemory& m = it->second.mem;
					if (it->second.queue.Pending())
						continue;
					// Cheap filters before anything touches the world.
					if (m.lastPlayerAt == 0 || now - m.lastPlayerAt < 45000 || now - m.lastPlayerAt > 15u * 60u * 1000u)
						continue;
					if (m.lastBotAt != 0 && now - m.lastBotAt < 60000)
						continue;
					if (m.lastInitiativeAt != 0 && now - m.lastInitiativeAt < CONV_INITIATIVE_PAIR_GAP_MS)
						continue;
					if (m.lastCheckAt != 0 && now - m.lastCheckAt < CONV_INITIATIVE_CHECK_MS)
						continue;
					if (m.talks < 3 && m.sessions < 2)
						continue;
					if (IsQuiet(m, now))
						continue;
					m.lastCheckAt = now;
					if (!m_rng.Chance(25))
						continue;
					TBotSnapshot snap;
					if (!host.BuildSnapshot(m.playerPID, m.botPID, snap))
						continue;
					const std::string text = ChooseInitiative(m, snap, now, m_rng);
					if (text.empty())
						continue;
					host.Send(m.playerPID, m.botPID, text);
					it->second.queue.lastSentAt = now;
					m_lastInitiative = now;
					if (m_debug)
					{
						char line[512];
						snprintf(line, sizeof(line), "PLAYERBOT_CONV_INITIATIVE: player=%s bot=%s text=\"%s\"",
								snap.askerName.c_str(), snap.name.c_str(), text.c_str());
						host.Log(line);
					}
					return; // one a scan at most
				}
			}

			TPairs m_pairs;
			std::set<TKey> m_pending;
			TRng m_rng;
			u32 m_lastPrune;
			u32 m_lastInitiativeScan;
			u32 m_lastInitiative;
			bool m_debug;
			bool m_initiative;
			u32 m_statLines;
			u32 m_statReplies;
			u32 m_statMerged;
			u32 m_statDropped;
	};
}

#endif
