// PlayerBot Conversation v6 - unit and scenario tests for the pure layer.
//
// Build (from the game source directory):
//   g++ -std=c++11 -Wall -Wextra -I. tests/playerbot_conversation_test.cpp -o /tmp/pbconv && /tmp/pbconv
//   clang++ -std=c++20 -Wall -I. tests/playerbot_conversation_test.cpp -o /tmp/pbconv && /tmp/pbconv
// Pass -v to print every conversation.

#include "playerbot_conv_engine.h"
#include <cstdio>
#include <cstdlib>

using namespace playerbot_conv;

static int g_failures = 0;
static int g_checks = 0;
static bool g_verbose = false;

#define CHECK(cond, ...) do { ++g_checks; if (!(cond)) { ++g_failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ------------------------------------------------------------------ mock host

struct TSent
{
	u32 at;
	std::string text;
};

class CMockWorld : public IConvWorld
{
	public:
		bool FindItem(const std::string& q, std::string& name, unsigned int& count)
		{
			if (q.find("tarcz") != std::string::npos)
			{
				name = "Tarcza Bojowa";
				count = 1;
				return true;
			}
			return false;
		}
		std::string AnswerBuy(const std::string& q) { return "Mam " + q + " na straganie, 50k."; }
		std::string AnswerSell(const std::string& q) { return "Nie potrzebuje " + q + "."; }
};

class CMockHost : public IConvHost
{
	public:
		TBotSnapshot snap;
		std::vector<TSent> sent;
		std::vector<std::string> logs;
		u32 now;
		bool alive;
		CMockWorld world;
		CMockHost() : now(0), alive(true)
		{
			snap.name = "Punnane";
			snap.askerName = "Lost3k";
			snap.level = 42;
			snap.job = 0;
			snap.empire = 2;
			snap.mapIndex = 64;
			snap.action = A_FIGHT;
			snap.goal = G_LEVEL;
			snap.targetName = "Ork Wojownik";
			snap.hpPct = 80;
			snap.spPct = 70;
			snap.gold = 3400000;
			snap.horseLevel = 11;
			snap.freeCells = 17;
			snap.bagCells = 90;
			snap.weaponName = "Miecz Pelni";
			snap.weaponPlus = 6;
			snap.armorName = "Zbroja Tygrysa";
			snap.armorPlus = 5;
			snap.mobsNear = 7;
			snap.style = S_WANDERER;
			snap.mood = MOOD_NEUTRAL;
			snap.onlineMinutes = 95;
			snap.actionMinutes = 12;
			snap.askerLevel = 40;
			snap.hour = 20;
		}
		bool BuildSnapshot(u32, u32, TBotSnapshot& out) { if (!alive) return false; out = snap; return true; }
		IConvWorld* World(u32, u32) { return &world; }
		void Send(u32, u32, const std::string& text)
		{
			TSent s;
			s.at = now;
			s.text = text;
			sent.push_back(s);
			if (g_verbose)
				printf("    [%6.2fs] BOT: %s\n", now / 1000.0, text.c_str());
		}
		void Log(const std::string& line) { logs.push_back(line); if (false) printf("      %s\n", line.c_str()); }
};

struct TScenario
{
	CConvEngine engine;
	CMockHost host;
	u32 t;
	TScenario(u32 seed = 1234) : t(1000000)
	{
		engine.Seed(seed);
		engine.SetDebug(true);
		engine.SetInitiative(false);
	}
	EIntent Say(const char* text)
	{
		host.now = t;
		if (g_verbose)
			printf("    [%6.2fs] GRACZ: %s\n", t / 1000.0, text);
		return engine.OnPlayerLine(host, 7, 99, text, t, "Lost3k", "Punnane");
	}
	// Advance the clock in 50 ms steps, pumping like the timer does.
	void Wait(u32 ms)
	{
		const u32 end = t + ms;
		while (t < end)
		{
			t += 50;
			host.now = t;
			engine.Pump(host, t);
		}
	}
	size_t Sent() const { return host.sent.size(); }
	std::string Last() const { return host.sent.empty() ? std::string() : host.sent.back().text; }
};

// ---------------------------------------------------------------- analysis

static EIntent IntentOf(const char* text)
{
	TAnalysis a;
	AnalyzeLine(text, a, 1);
	return a.intent;
}

static void TestNormalization()
{
	TTokens t;
	Normalize("Gdzie JESTEŚ???", t); // UTF-8
	CHECK(t.norm == "gdzie jestes" && t.question, "utf8 fold: '%s'", t.norm.c_str());
	const char cp1250[] = { 'g', 'd', 'z', 'i', 'e', ' ', 'j', 'e', 's', 't', 'e', (char)0x9C, 0 };
	Normalize(cp1250, t);
	CHECK(t.norm == "gdzie jestes", "cp1250 fold: '%s'", t.norm.c_str());
	Normalize("  co   ROBISZ  teraz?! ", t);
	CHECK(t.norm == "co robisz teraz" && t.question && t.exclaim, "spaces: '%s'", t.norm.c_str());
	Normalize("siemaaaaa nwm", t);
	CHECK(t.norm == "siema nie wiem", "collapse+rewrite: '%s'", t.norm.c_str());
	Normalize("Ż ź ć ń ó ł ę ą ś", t);
	CHECK(t.norm == "z z c n o l e a s", "all letters: '%s'", t.norm.c_str());
	Normalize("xD :)", t);
	CHECK(t.smile, "smile");
	CHECK(EditDistance("porabiasz", 9, "porabaisz", 9, 1) == 1, "transposition");
	CHECK(FuzzyEquals("jestse", "jestes"), "fuzzy jestes");
	CHECK(!FuzzyEquals("jest", "jestes"), "short not fuzzy");
}

struct TIntentCase
{
	const char* text;
	EIntent intent;
};

static void TestIntents()
{
	static const TIntentCase kCases[] = {
		// CURRENT_ACTIVITY in many forms
		{ "co robisz?", I_ACTIVITY }, { "co teraz?", I_ACTIVITY }, { "co porabiasz", I_ACTIVITY },
		{ "czym sie zajmujesz?", I_ACTIVITY }, { "co robisz teraz", I_ACTIVITY }, { "co tam robisz?", I_ACTIVITY },
		{ "Co RObisz", I_ACTIVITY }, { "co porabaisz", I_ACTIVITY }, { "cp robisz", I_ACTIVITY },
		// LOCATION / ACTIVITY_LOCATION
		{ "gdzie jestes?", I_LOCATION }, { "gdzie jesteś", I_LOCATION }, { "gdzie teraz?", I_LOCATION },
		{ "na jakiej mapie?", I_LOCATION }, { "gdzie bijesz?", I_ACTIVITY_LOCATION },
		{ "gdzie expisz?", I_ACTIVITY_LOCATION }, { "gdzie teraz exp", I_ACTIVITY_LOCATION },
		{ "gdzei expisz", I_ACTIVITY_LOCATION }, { "exp teraz gdzie", I_ACTIVITY_LOCATION },
		// few-word meaning
		{ "duzo mobow tam", I_MOB_COUNT }, { "masz wolne eq", I_INVENTORY_SPACE }, { "duzo masz yang", I_GOLD },
		{ "jaka gildia", I_GUILD }, { "z kim jestes", I_PARTY }, { "co bijesz", I_TARGET }, { "jak z hp", I_HP },
		{ "co z koniem", I_HORSE }, { "co z biologiem", I_BIOLOGIST }, { "co kopiesz", I_MINING },
		{ "lowisz cos", I_FISHING }, { "co z metinami", I_METIN }, { "co planujesz dalej", I_NEXT_PLAN },
		{ "jaki masz lvl?", I_LEVEL }, { "ile masz lvl", I_LEVEL }, { "masz gildie?", I_GUILD },
		{ "masz konia?", I_HORSE }, { "ile yang", I_GOLD }, { "jestes w pt?", I_PARTY },
		{ "moge z toba?", I_PARTY_REQUEST }, { "chodz ze mna na exp", I_PARTY_REQUEST }, { "wbijesz do pt?", I_PARTY_REQUEST },
		{ "dokad idziesz", I_TRAVEL }, { "jaki masz cel?", I_GOAL }, { "ile miejsca w eq", I_INVENTORY_SPACE },
		{ "masz tarcze?", I_ITEM_OWN }, { "sprzedasz mi miecz", I_BUY }, { "jak drop?", I_DROP_LUCK },
		{ "lubisz ta mape?", I_MAP_OPINION }, { "lubisz tu expic?", I_MAP_OPINION }, { "dlugo tu jestes?", I_TIME_HERE },
		{ "duzo dzis zrobiles?", I_PROGRESS_TODAY }, { "masz szczescie?", I_DROP_LUCK }, { "lubisz mnie?", I_RELATIONSHIP },
		{ "zginales dzis?", I_DEATH }, { "jak sie nazywasz", I_NAME }, { "jaka masz klase", I_CLASS },
		{ "z jakiego imperium jestes", I_EMPIRE }, { "jaki masz nastroj", I_MOOD }, { "wieza demonow?", I_DEMON_TOWER },
		// social
		{ "hej", I_GREETING }, { "siemka", I_GREETING }, { "nara", I_FAREWELL }, { "dzieki!", I_THANKS },
		{ "co tam?", I_HOW_ARE_YOU }, { "jak tam?", I_HOW_ARE_YOU }, { "jak leci", I_HOW_ARE_YOU },
		{ "wszystko ok?", I_HOW_ARE_YOU }, { "co potrafisz?", I_HELP }, { "jestes botem?", I_IS_BOT },
		{ "ile masz lat", I_AGE }, { "debil", I_INSULT },
		// follow-ups
		{ "gdzie?", I_FOLLOW_UP }, { "a gdzie?", I_FOLLOW_UP }, { "duzo?", I_FOLLOW_UP }, { "duzo ich?", I_FOLLOW_UP },
		{ "jest ich sporo?", I_FOLLOW_UP }, { "sam?", I_FOLLOW_UP }, { "z kim?", I_FOLLOW_UP }, { "dlaczego?", I_FOLLOW_UP },
		{ "czemu?", I_FOLLOW_UP }, { "po co?", I_FOLLOW_UP }, { "a potem?", I_FOLLOW_UP }, { "i co dalej?", I_NEXT_PLAN },
		{ "serio?", I_FOLLOW_UP }, { "naprawde?", I_FOLLOW_UP }, { "a ty?", I_FOLLOW_UP }, { "i?", I_FOLLOW_UP },
		{ "no i?", I_FOLLOW_UP }, { "jak?", I_FOLLOW_UP }, { "malo?", I_FOLLOW_UP }, { "a tam?", I_FOLLOW_UP },
		{ "co potem?", I_NEXT_PLAN },
		// reactions
		{ "ok", I_ACK }, { "aha", I_ACK }, { "spoko", I_ACK }, { "xD", I_LAUGH }, { "haha", I_LAUGH }, { "tak", I_YES },
		// general
		{ "zimno dzisiaj", I_GENERAL }, { "lubisz zime?", I_GENERAL }, { "co lubisz robic?", I_GENERAL },
		{ "masz jakies marzenia?", I_GENERAL }, { "czego sie boisz?", I_GENERAL }, { "co cie denerwuje?", I_GENERAL },
		{ "co cie cieszy?", I_GENERAL }, { "gdybys mogl wybrac dowolne miejsce na swiecie gdzie bys pojechal?", I_GENERAL },
		{ "lubisz muzyke?", I_GENERAL }, { "jaki twoj ulubiony film", I_GENERAL }, { "wolisz zime czy lato?", I_GENERAL },
		{ "jestem zmeczony", I_GENERAL }, { "nudzisz sie?", I_GENERAL }, { "co jadles dzisiaj", I_GENERAL },
		{ "lubisz pizze?", I_GENERAL }, { "gdzie bys pojechal na wakacje", I_GENERAL }, { "pada deszcz", I_GENERAL },
		{ "masz psa?", I_GENERAL }, { "czego nie lubisz?", I_GENERAL },
	};
	for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i)
	{
		const EIntent got = IntentOf(kCases[i].text);
		CHECK(got == kCases[i].intent, "\"%s\" -> %s, expected %s", kCases[i].text, IntentName(got), IntentName(kCases[i].intent));
	}

	TAnalysis a;
	AnalyzeLine("lubisz zime?", a, 1);
	CHECK(a.qtype == Q_LIKE && a.object == "zime" && a.topic == T_SEASON, "lubisz zime: q=%d obj='%s' topic=%s", a.qtype, a.object.c_str(), TopicName(a.topic));
	AnalyzeLine("wolisz zime czy lato?", a, 1);
	CHECK(a.qtype == Q_CHOICE && a.object == "zime" && a.objectB == "lato", "choice: q=%d '%s' '%s'", a.qtype, a.object.c_str(), a.objectB.c_str());
	AnalyzeLine("gdybys mogl wybrac dowolne miejsce na swiecie gdzie bys pojechal?", a, 1);
	CHECK(a.qtype == Q_HYPO && a.topic == T_TRAVEL, "hypo travel: q=%d topic=%s", a.qtype, TopicName(a.topic));
	AnalyzeLine("hej, co robisz?", a, 1);
	CHECK(a.intent == I_ACTIVITY && a.greetingToo, "greet+activity %s %d", IntentName(a.intent), a.greetingToo);
	AnalyzeLine("sprzedasz mi miecz pelni", a, 1);
	CHECK(a.object == "miecz pelni", "buy object '%s'", a.object.c_str());
	AnalyzeLine("chce kupic od ciebie ku aura miecza", a, 1);
	CHECK(a.intent == I_BUY && a.object == "ku aura miecza", "buy book '%s'", a.object.c_str());
	AnalyzeLine("kupie kosc", a, 1);
	CHECK(a.intent == I_BUY && a.object == "kosc", "kupie '%s'", a.object.c_str());
	AnalyzeLine("sprzedam ci rude", a, 1);
	CHECK(a.intent == I_SELL && a.object == "rude", "sell '%s'", a.object.c_str());
}

// --------------------------------------------------------------- scenarios

static bool Contains(const std::string& s, const char* what)
{
	return s.find(what) != std::string::npos;
}

static void Test1_FollowUpMobs()
{
	if (g_verbose) printf("\n  TEST 1: co robisz / gdzie / duzo mobow\n");
	TScenario s;
	s.Say("co robisz?");
	s.Wait(1600);
	CHECK(s.Sent() == 1, "one reply after first line (%u)", (unsigned)s.Sent());
	s.Wait(1500);
	s.Say("gdzie?");
	s.Wait(1600);
	CHECK(s.Sent() == 2, "reply to gdzie");
	CHECK(Contains(s.Last(), "Dolin") || Contains(s.Last(), "Orkow"), "gdzie -> map: '%s'", s.Last().c_str());
	s.Wait(1500);
	s.Say("duzo mobow?");
	s.Wait(1600);
	CHECK(s.Sent() == 3, "reply to duzo mobow");
	CHECK(Contains(s.Last(), "roche") || Contains(s.Last(), "Jest co bic") || Contains(s.Last(), "w sam raz"), "7 mobs -> troche: '%s'", s.Last().c_str());
	s.Wait(1500);
	s.Say("duzo ich?");
	s.Wait(1600);
	TConvPair* p = s.engine.FindPair(7, 99);
	CHECK(p && p->mem.turns[0].intent == I_MOB_COUNT, "duzo ich -> MOB_COUNT (%s)", p ? IntentName(p->mem.turns[0].intent) : "-");
	s.Wait(1500);
	s.Say("sam?");
	s.Wait(1600);
	CHECK(Contains(s.Last(), "sam") || Contains(s.Last(), "Sam") || Contains(s.Last(), "Solo"), "sam -> alone: '%s'", s.Last().c_str());
	s.Wait(1500);
	s.Say("mogę z tobą?");
	s.Wait(1600);
	CHECK(Contains(s.Last(), "apros"), "join -> invite: '%s'", s.Last().c_str());
	s.Wait(1500);
	s.Say("a potem?");
	s.Wait(1600);
	CHECK(p && p->mem.turns[0].intent == I_NEXT_PLAN, "a potem -> NEXT_PLAN (%s)", p ? IntentName(p->mem.turns[0].intent) : "-");
}

static void Test2_TopicChange()
{
	if (g_verbose) printf("\n  TEST 2/3/7: gra -> small talk -> temat obcy -> powrot\n");
	TScenario s(77);
	s.Say("co robisz?");
	s.Wait(2500);
	s.Say("zimno dzisiaj");
	s.Wait(2500);
	TConvPair* p = s.engine.FindPair(7, 99);
	CHECK(p && p->mem.turns[0].intent == I_GENERAL && p->mem.turns[0].topic == T_WEATHER, "zimno -> GENERAL/WEATHER");
	CHECK(!Contains(s.Last(), "expie") && !Contains(s.Last(), "Dolin"), "topic changed: '%s'", s.Last().c_str());
	s.Say("lubisz zime?");
	s.Wait(2500);
	s.Say("co lubisz robic?");
	s.Wait(2500);
	CHECK(p->mem.turns[0].intent == I_GENERAL && p->mem.turns[0].topic == T_HOBBY, "hobby");
	s.Say("masz jakies marzenia?");
	s.Wait(2500);
	CHECK(p->mem.turns[0].topic == T_DREAMS || p->mem.turns[0].qtype == Q_DREAM, "dreams");
	s.Say("a gdzie teraz expisz?");
	s.Wait(2500);
	CHECK(p->mem.turns[0].intent == I_ACTIVITY_LOCATION, "back to game");
	CHECK(Contains(s.Last(), "Dolin"), "back answers map: '%s'", s.Last().c_str());
	// With the same seed repeated a few times, "Wracajac" should appear at least sometimes.
	int returned = 0;
	for (u32 seed = 1; seed <= 20; ++seed)
	{
		TScenario r(seed);
		r.Say("co robisz?"); r.Wait(2500);
		r.Say("zimno dzisiaj"); r.Wait(2500);
		r.Say("a gdzie teraz expisz?"); r.Wait(2500);
		if (Contains(r.Last(), "racajac"))
			++returned;
	}
	CHECK(returned >= 8, "return-to-topic phrasing appears (%d/20)", returned);
}

static void Test4_Why()
{
	if (g_verbose) printf("\n  TEST 4: co robisz / gdzie / dlaczego\n");
	TScenario s(5);
	s.Say("co robisz?");
	s.Wait(2500);
	s.Say("gdzie?");
	s.Wait(2500);
	s.Say("dlaczego?");
	s.Wait(2500);
	CHECK(Contains(s.Last(), "Bo ") || Contains(s.Last(), "bo "), "why answers with a reason: '%s'", s.Last().c_str());
}

static void Test5_Merge()
{
	if (g_verbose) printf("\n  TEST 5: hej / jak tam / co robisz / masz gildie / jaki lvl (szybko)\n");
	TScenario s(9);
	s.Say("hej"); s.Wait(200);
	s.Say("jak tam?"); s.Wait(200);
	s.Say("co robisz?"); s.Wait(200);
	s.Say("masz gildie?"); s.Wait(200);
	s.Say("jaki lvl?");
	s.Wait(3000);
	CHECK(s.Sent() <= 2, "merged into at most two whispers (%u)", (unsigned)s.Sent());
	std::string all;
	for (size_t i = 0; i < s.host.sent.size(); ++i)
		all += s.host.sent[i].text + " ";
	CHECK(Contains(all, "42"), "level answered: '%s'", all.c_str());
	CHECK(Contains(all, "gildi"), "guild answered: '%s'", all.c_str());
	// First whisper within 1.5 s of the first line (+ one pump step).
	CHECK(!s.host.sent.empty() && s.host.sent[0].at - 1000000 <= 1550, "first reply at %u ms", s.host.sent.empty() ? 0 : s.host.sent[0].at - 1000000);

	if (g_verbose) printf("\n  TEST 5b: co robisz / gdzie / duzo mobow / jaki lvl (szybko)\n");
	TScenario m(11);
	m.Say("co robisz?"); m.Wait(200);
	m.Say("gdzie?"); m.Wait(200);
	m.Say("duzo mobow?"); m.Wait(200);
	m.Say("jaki lvl?");
	m.Wait(3000);
	CHECK(m.Sent() == 1, "four quick lines -> one whisper (%u)", (unsigned)m.Sent());
	CHECK(Contains(m.Last(), "Dolin") && Contains(m.Last(), "42"), "merged content: '%s'", m.Last().c_str());
	// The map is said once.
	const std::string l = m.Last();
	size_t first = l.find("Dolin");
	CHECK(first != std::string::npos && l.find("Dolin", first + 1) == std::string::npos, "map once: '%s'", l.c_str());
}

static void Test6_Spam()
{
	if (g_verbose) printf("\n  TEST 6: 20 wiadomosci w 2 sekundy\n");
	TScenario s(3);
	const char* spam[] = { "hej", "co tam", "co robisz", "gdzie", "jaki lvl", "masz gildie", "masz konia", "ile yang",
		"co bijesz", "chodz", "hej", "hej", "co robisz", "xd", "ok", "gdzie", "lvl", "pt?", "halo", "odpisz" };
	for (size_t i = 0; i < 20; ++i)
	{
		s.Say(spam[i]);
		s.Wait(100);
	}
	s.Wait(6000);
	CHECK(s.Sent() >= 1 && s.Sent() <= 5, "spam -> a few whispers, not 20 (%u)", (unsigned)s.Sent());
	for (size_t i = 1; i < s.host.sent.size(); ++i)
		CHECK(s.host.sent[i].at - s.host.sent[i - 1].at >= 850, "no machine-gun: gap %u ms", s.host.sent[i].at - s.host.sent[i - 1].at);
	// Conversation still works afterwards.
	s.Wait(5000);
	const size_t before = s.Sent();
	s.Say("co robisz?");
	s.Wait(1600);
	CHECK(s.Sent() == before + 1, "not blocked after spam");
}

static void TestNoLostMessages()
{
	if (g_verbose) printf("\n  TEST: wiadomosc zaraz po odpowiedzi nie ginie\n");
	TScenario s(21);
	s.Say("co robisz?");
	s.Wait(1600);
	CHECK(s.Sent() == 1, "first");
	s.Say("gdzie?"); // right after the reply - the old limiter dropped this
	s.Wait(1700);
	CHECK(s.Sent() == 2, "second answered without repeating (%u)", (unsigned)s.Sent());
	const u32 gap = s.host.sent[1].at - s.host.sent[0].at;
	CHECK(gap >= 700, "second not instant (%u)", gap);
}

static void TestDelays()
{
	for (u32 seed = 1; seed < 60; ++seed)
	{
		TScenario s(seed);
		s.Say("co robisz?");
		s.Wait(2000);
		CHECK(s.Sent() == 1, "sent");
		const u32 d = s.host.sent.empty() ? 0 : s.host.sent[0].at - 1000000;
		CHECK(d >= 700 && d <= 1550, "delay %u ms in 700..1500", d);
	}
}

static void TestConsistency()
{
	// Low HP must never sound great.
	for (u32 seed = 1; seed < 30; ++seed)
	{
		TScenario s(seed);
		s.host.snap.hpPct = 15;
		s.Say("co robisz?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "HP") || Contains(s.Last(), "ledwo"), "low hp shows: '%s'", s.Last().c_str());
		CHECK(!Contains(s.Last(), "swietn") && !Contains(s.Last(), "Swietn"), "low hp not great");
	}
	// In Joan, never Dolina.
	{
		TScenario s(2);
		s.host.snap.mapIndex = 21;
		s.host.snap.action = A_IDLE;
		s.host.snap.inTown = true;
		s.Say("gdzie jestes?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "Joan") && !Contains(s.Last(), "Dolin"), "Joan: '%s'", s.Last().c_str());
		s.Say("duzo mobow?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "miescie") || Contains(s.Last(), "mobow"), "no mobs in town: '%s'", s.Last().c_str());
	}
	// No guild -> never a guild name; alone -> never PT mates.
	for (u32 seed = 1; seed < 20; ++seed)
	{
		TScenario s(seed);
		s.Say("masz gildie?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "Nie") || Contains(s.Last(), "nie") || Contains(s.Last(), "Bez"), "no guild: '%s'", s.Last().c_str());
		s.Say("z kim expisz?");
		s.Wait(2000);
		CHECK(!Contains(s.Last(), "PT z") && !Contains(s.Last(), "osob"), "alone: '%s'", s.Last().c_str());
	}
	// With a guild, its name.
	{
		TScenario s(4);
		s.host.snap.inGuild = true;
		s.host.snap.guildName = "Smoki";
		s.host.snap.guildMembers = 12;
		s.Say("jaka gildia?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "Smoki"), "guild name: '%s'", s.Last().c_str());
		s.Say("duzo ich?");
		s.Wait(2000);
		CHECK(Contains(s.Last(), "12"), "guild count via follow-up: '%s'", s.Last().c_str());
	}
}

static void TestPersonaOpenQuestion()
{
	const int styles[] = { S_WANDERER, S_MERCHANT, S_GRINDER, S_METIN, S_COMPANION };
	std::set<std::string> answers;
	for (size_t i = 0; i < 5; ++i)
	{
		TScenario s(100 + i);
		s.host.snap.style = styles[i];
		if (g_verbose) printf("\n  OPEN (styl %d):\n", styles[i]);
		s.Say("gdybys mogl wybrac dowolne miejsce na swiecie gdzie bys pojechal?");
		s.Wait(2000);
		CHECK(!Contains(s.Last(), "rozumiem"), "no 'nie rozumiem': '%s'", s.Last().c_str());
		answers.insert(s.Last());
	}
	CHECK(answers.size() >= 4, "personas answer differently (%u)", (unsigned)answers.size());
	// Opinions are stable per bot.
	TScenario a(1), b(2);
	a.Say("lubisz zime?"); a.Wait(2000);
	b.Say("lubisz zime?"); b.Wait(2000);
	const bool likeA = !Contains(a.Last(), "nie") && !Contains(a.Last(), "Nie") && !Contains(a.Last(), "wole");
	const bool likeB = !Contains(b.Last(), "nie") && !Contains(b.Last(), "Nie") && !Contains(b.Last(), "wole");
	CHECK(likeA == likeB, "same bot, same opinion: '%s' / '%s'", a.Last().c_str(), b.Last().c_str());
}

static void TestFactUnknown()
{
	TScenario s(8);
	s.Say("kto wygral wczoraj mecz?");
	s.Wait(2000);
	CHECK(Contains(s.Last(), "wiem") || Contains(s.Last(), "pojecia") || Contains(s.Last(), "znam"), "unknown fact honest: '%s'", s.Last().c_str());
}

// The 2.x line's counter stands without its keeper: the bot names the town it
// stands in and what is on it, and never claims to be standing at it.
static void TestOfflineShop()
{
	TScenario s(11);
	s.host.snap.shopTown = "Joan";
	s.host.snap.shopItems = 7;
	s.host.snap.shopSummary = "Tarcza Bojowa za 120k";
	s.Say("co masz w sklepie?");
	s.Wait(2000);
	CHECK(Contains(s.Last(), "Joan") && Contains(s.Last(), "Tarcza Bojowa"), "offline shop named: '%s'", s.Last().c_str());
	CHECK(!Contains(s.Last(), "stoje"), "a keeper out hunting is not at its stall: '%s'", s.Last().c_str());

	TScenario none(12);
	none.Say("co masz na straganie?");
	none.Wait(2000);
	CHECK(!Contains(none.Last(), "Joan"), "no shop, no town: '%s'", none.Last().c_str());
}

static void TestAnswerToBot()
{
	// Force an ask-back by talking to a social bot until it asks.
	int ok = 0;
	for (u32 seed = 1; seed < 30 && ok == 0; ++seed)
	{
		TScenario s(seed);
		s.host.snap.style = S_COMPANION;
		s.Say("jak tam?");
		s.Wait(2000);
		TConvPair* p = s.engine.FindPair(7, 99);
		if (p && p->mem.botAsk == ASK_HOW_ARE_YOU)
		{
			s.Say("dobrze");
			s.Wait(2000);
			CHECK(p->mem.turns[0].intent == I_ANSWER_TO_BOT, "answer read as answer (%s)", IntentName(p->mem.turns[0].intent));
			ok = 1;
		}
	}
	CHECK(ok == 1, "bot asked back at least once");
}

static void TestReactions()
{
	int silent = 0, spoke = 0;
	for (u32 seed = 1; seed < 40; ++seed)
	{
		TScenario s(seed);
		s.Say("ok");
		s.Wait(2000);
		if (s.Sent()) ++spoke; else ++silent;
	}
	CHECK(silent > 0 && spoke > 0, "acks: sometimes silent (%d), sometimes a word (%d)", silent, spoke);
}

static void TestRepeatAndInsult()
{
	TScenario s(6);
	s.Say("jaki lvl?"); s.Wait(2000);
	s.Say("jaki lvl?"); s.Wait(2000);
	s.Say("jaki lvl?"); s.Wait(2000);
	CHECK(Contains(s.Last(), "42"), "repeat still answers: '%s'", s.Last().c_str());
	for (int i = 0; i < 5; ++i) { s.Say("debil"); s.Wait(2000); }
	TConvPair* p = s.engine.FindPair(7, 99);
	CHECK(p && ComputeTier(p->mem, 0, false) == TIER_HOSTILE, "insults -> hostile tier");
}

static void TestBotGone()
{
	TScenario s(12);
	s.Say("co robisz?");
	s.host.alive = false;
	s.Wait(3000);
	CHECK(s.Sent() == 0 && !s.engine.HasPending(), "queue dropped when bot gone");
}

static void TestInitiative()
{
	TScenario s(31);
	s.engine.SetInitiative(true);
	s.host.snap.askerNear = true;
	const char* chat[] = { "hej", "co robisz", "jak tam", "dzieki", "fajnie", "ok", "jaki lvl", "dzieki" };
	for (int i = 0; i < 8; ++i) { s.Say(chat[i]); s.Wait(2000); }
	const size_t before = s.Sent();
	s.host.snap.level = 43; // level up since the last talk
	s.Wait(12u * 60u * 1000u);
	CHECK(s.Sent() >= before + 1 && s.Sent() <= before + 3, "initiative: spoke first, not spam (%u)", (unsigned)(s.Sent() - before));
}

static void TestMemoryBounds()
{
	CConvEngine e;
	CMockHost h;
	for (u32 i = 0; i < 5000; ++i)
	{
		e.OnPlayerLine(h, 1000 + i, 99, "hej", 1000 + i);
		e.Pump(h, 1000 + i + 2000);
	}
	CHECK(e.PairCount() <= CONV_MAX_PAIRS, "pair cap (%u)", (unsigned)e.PairCount());
}

static void DemoConversation()
{
	if (!g_verbose) return;
	printf("\n  DEMO: pelna rozmowa z przykladu (rozne style)\n");
	const int styles[] = { S_WANDERER, S_GRINDER, S_MERCHANT };
	for (size_t k = 0; k < 3; ++k)
	{
		printf("\n  --- styl %d ---\n", styles[k]);
		TScenario s(500 + k);
		s.host.snap.style = styles[k];
		const char* lines[] = { "hej", "co robisz?", "duzo ich?", "sam?", "moge z toba?", "a potem?", "zimno dzisiaj",
			"lubisz zime?", "co lubisz robic?", "masz jakies marzenia?", "a gdzie teraz expisz?", "dlaczego tam?",
			"co cie denerwuje?", "wolisz pizze czy kebab?", "ile masz yang?", "jak z hp", "nara" };
		for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i)
		{
			s.Say(lines[i]);
			s.Wait(2200);
		}
	}
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
		if (!strcmp(argv[i], "-v")) g_verbose = true;
	TestNormalization();
	TestIntents();
	Test1_FollowUpMobs();
	Test2_TopicChange();
	Test4_Why();
	Test5_Merge();
	Test6_Spam();
	TestNoLostMessages();
	TestDelays();
	TestConsistency();
	TestPersonaOpenQuestion();
	TestFactUnknown();
	TestOfflineShop();
	TestAnswerToBot();
	TestReactions();
	TestRepeatAndInsult();
	TestBotGone();
	TestInitiative();
	TestMemoryBounds();
	DemoConversation();
	printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
