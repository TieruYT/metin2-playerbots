#ifndef __INC_METIN2_PLAYERBOT_CHAT_CONVERSATION_H__
#define __INC_METIN2_PLAYERBOT_CHAT_CONVERSATION_H__

// PlayerBot Conversation v6 - the engine side.
//
// A whisper to a bot that is not a lure order or a trade line
// (playerbot_chat_trade.h) comes here. This file is the only one that knows
// both the conversation layer and the engine:
//
//   HandlePlayerBotConversation()  - the whisper hook: analyse now, queue the reply
//   PumpPlayerBotConversation()    - called by a short timer while replies are
//                                    pending and by CPlayerBotManager::Update
//   CPlayerBotConvHost             - builds the TBotSnapshot from the real AI state
//                                    and character, sends the whisper, logs
//   DescribePlayerBotBuffs()       - a Shaman's three buffs, evaluated from the
//                                    world's own skill_proto the way ComputeSkill does
//   ManagePlayerBotSummon()        - "chodz do mnie": the walk to the person and a few
//                                    minutes beside them (one line in the tick, after
//                                    the follow pass); IsPlayerBotSummoned for the
//                                    passes that must leave such a bot alone
//
// The summon is the one thing here that moves a bot. It keeps its own state
// (s_mapPlayerBotSummons) instead of a field in TPlayerBotAIState, and ends by
// itself: the few minutes over, the person gone or off the map, a walk that
// does not arrive, or a claim with a better right (a duel, a war, a stall).
//
// The conversation itself - normalization, intents, context, memory,
// persona, mood, relationship, general conversation, merging, the queue - is
// the pure layer (playerbot_conv_*.h), unit tested without the engine:
// tests/playerbot_conversation_test.cpp.
//
// The layer READS the AI (TPlayerBotAIState, the persona and mood, the friend
// ledger, the party, the guild, the counter, the bag). It never writes to it,
// but for the summon's walk and its guard, which drop the bot's own route and
// target while a person has called it.
//
// Runtime switches (files in the game core's working directory, checked every
// 30 s, no restart needed):
//   playerbot_conv_debug   - exists: PLAYERBOT_CONV / _QUEUE / _REPLY lines in syslog
//   playerbot_conv_noinit  - exists: bots never start a conversation themselves
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_chat_trade.h, playerbot_status.h and
// the subsystems it reads (mood, mining, herbalism, missions, guild, town).

#include "playerbot_conv_engine.h"
#include <ctime>
// CMobManager (the hunting mission's mob name) comes from mob_manager.h,
// included with the other engine headers at the top of playerbot_manager.cpp.

#ifndef PLAYERBOT_CONV_DEBUG_DEFAULT
#define PLAYERBOT_CONV_DEBUG_DEFAULT 0
#endif

namespace
{
	const DWORD PLAYERBOT_CONV_SWITCH_CHECK_MS = 30000;
	const DWORD PLAYERBOT_CONV_STATS_INTERVAL_MS = 10 * 60 * 1000;
	const int PLAYERBOT_CONV_AROUND_RADIUS = 2500;
	const int PLAYERBOT_CONV_NEAR_RADIUS = 3000;
	const size_t PLAYERBOT_CONV_BAG_SUMMARY_ITEMS = 4;
	const size_t PLAYERBOT_CONV_SHOP_SUMMARY_ITEMS = 3;

	playerbot_conv::CConvEngine s_PlayerBotConvEngine;
	LPEVENT s_pkPlayerBotConvEvent = NULL;
	DWORD s_dwPlayerBotConvSwitchCheck = 0;
	DWORD s_dwPlayerBotConvStatsTime = 0;
	unsigned int s_uPlayerBotConvStatsLines = 0;
	bool s_bPlayerBotConvSwitchesRead = false;

	bool PlayerBotConvFlagFile(const char* path)
	{
		struct stat st;
		return path && stat(path, &st) == 0;
	}

	void RefreshPlayerBotConvSwitches(DWORD dwNow)
	{
		if (s_bPlayerBotConvSwitchesRead && dwNow - s_dwPlayerBotConvSwitchCheck < PLAYERBOT_CONV_SWITCH_CHECK_MS)
			return;
		s_bPlayerBotConvSwitchesRead = true;
		s_dwPlayerBotConvSwitchCheck = dwNow;
		const bool debug = PLAYERBOT_CONV_DEBUG_DEFAULT || PlayerBotConvFlagFile("playerbot_conv_debug");
		const bool initiative = !PlayerBotConvFlagFile("playerbot_conv_noinit");
		if (debug != s_PlayerBotConvEngine.Debug())
			sys_log(0, "PLAYERBOT_CONV: debug %s", debug ? "on" : "off");
		s_PlayerBotConvEngine.SetDebug(debug);
		s_PlayerBotConvEngine.SetInitiative(initiative);
	}

	// ------------------------------------------------------------ AI mirrors

	int MapPlayerBotConvAction(BYTE action)
	{
		using namespace playerbot_conv;
		switch (action)
		{
			case BOT_ACTION_TRAVEL: return A_TRAVEL;
			case BOT_ACTION_FIGHT: return A_FIGHT;
			case BOT_ACTION_LOOT: return A_LOOT;
			case BOT_ACTION_RECOVER: return A_RECOVER;
			case BOT_ACTION_TRAIN: return A_TRAIN;
			case BOT_ACTION_SHOP: return A_SHOP;
			case BOT_ACTION_REFINE: return A_REFINE;
			case BOT_ACTION_READ_BOOK: return A_READ_BOOK;
			case BOT_ACTION_SOCKET_STONE: return A_SOCKET;
			case BOT_ACTION_PARTY_ASSEMBLE: return A_PARTY_ASSEMBLE;
			case BOT_ACTION_BIOLOGIST: return A_BIOLOGIST;
			case BOT_ACTION_STABLE: return A_STABLE;
			case BOT_ACTION_STALL: return A_STALL;
			case BOT_ACTION_FISHING: return A_FISHING;
			case BOT_ACTION_MARKET: return A_MARKET;
			case BOT_ACTION_LURE: return A_LURE;
			case BOT_ACTION_TOWN_REST: return A_TOWN_REST;
			case BOT_ACTION_MINING: return A_MINING;
			default: return A_IDLE;
		}
	}

	int MapPlayerBotConvGoal(BYTE goal)
	{
		using namespace playerbot_conv;
		switch (goal)
		{
			case BOT_GOAL_SURVIVE: return G_SURVIVE;
			case BOT_GOAL_CHOOSE_PROFESSION: return G_PROFESSION;
			case BOT_GOAL_GET_EQUIPMENT: return G_EQUIPMENT;
			case BOT_GOAL_RESTOCK: return G_RESTOCK;
			case BOT_GOAL_REFINE: return G_REFINE;
			case BOT_GOAL_MASTER_SKILL: return G_SKILL;
			case BOT_GOAL_HUNT_METIN: return G_METIN;
			case BOT_GOAL_PARTY_CHALLENGE: return G_PARTY_CHALLENGE;
			case BOT_GOAL_BIOLOGIST: return G_BIOLOGIST;
			case BOT_GOAL_HUNTING: return G_HUNTING;
			case BOT_GOAL_HORSE: return G_HORSE;
			case BOT_GOAL_FISHING: return G_FISHING;
			default: return G_LEVEL;
		}
	}

	// Iwakura's persona when the system is on, the older personality otherwise.
	int MapPlayerBotConvStyle(const TPlayerBotAIState& state)
	{
		using namespace playerbot_conv;
		if (IsPlayerBotPersonaEnabled() && state.persona.bPersona < playerbot_persona::PERSONA_COUNT)
		{
			switch (state.persona.bPersona)
			{
				case playerbot_persona::PERSONA_GRINDER: return S_GRINDER;
				case playerbot_persona::PERSONA_ZDOBYWCA: return S_CONQUEROR;
				case playerbot_persona::PERSONA_HANDLARZ: return S_MERCHANT;
				case playerbot_persona::PERSONA_HAZARDZISTA: return S_GAMBLER;
				case playerbot_persona::PERSONA_PERFEKCJONISTA: return S_PERFECTIONIST;
				case playerbot_persona::PERSONA_POGROMCA: return S_METIN;
				case playerbot_persona::PERSONA_GORNIK: return S_MINER;
				case playerbot_persona::PERSONA_RYBAK: return S_FISHER;
				case playerbot_persona::PERSONA_NAJEMNIK: return S_MERC;
				case playerbot_persona::PERSONA_TOWARZYSZ: return S_COMPANION;
				case playerbot_persona::PERSONA_METINOLOG: return S_METIN;
				case playerbot_persona::PERSONA_NALOGOWIEC: return S_GAMBLER;
				case playerbot_persona::PERSONA_NAUKOWIEC: return S_PERFECTIONIST;
				case playerbot_persona::PERSONA_EGZEKUTOR: return S_MERC;
				case playerbot_persona::PERSONA_WEDKARZ: return S_FISHER;
				default: break;
			}
		}
		switch (state.bPersonality)
		{
			case BOT_PERSONALITY_METIN_BREAKER: return S_METIN;
			case BOT_PERSONALITY_TEAM_COMPANION: return S_COMPANION;
			case BOT_PERSONALITY_GEAR_SPECIALIST: return S_GEAR;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR: return S_COLLECTOR;
			case BOT_PERSONALITY_MERCHANT: return S_MERCHANT;
			case BOT_PERSONALITY_WANDERER: return S_WANDERER;
			case BOT_PERSONALITY_METIN_DROPPER:
			case BOT_PERSONALITY_M3_DROPPER:
			case BOT_PERSONALITY_M2_DROPPER:
			case BOT_PERSONALITY_MEDAL_DROPPER: return S_DROPPER;
			default: return S_ADVENTURER;
		}
	}

	int MapPlayerBotConvMood(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		switch (GetPlayerBotPlayMood(ch, state, dwNow))
		{
			case playerbot_persona::MOOD_SLABY: return playerbot_conv::MOOD_BAD;
			case playerbot_persona::MOOD_BARDZO_DOBRY: return playerbot_conv::MOOD_GOOD;
			default: return playerbot_conv::MOOD_NEUTRAL;
		}
	}

	// Monsters and people around the bot, one walk of the sectrees nearby -
	// only when a reply is being written, never on the tick.
	class FPlayerBotConvCountAround
	{
		public:
			FPlayerBotConvCountAround(LPCHARACTER me, int radius) : m_me(me), m_radius(radius), m_mobs(0), m_players(0) {}
			void operator () (LPENTITY ent)
			{
				if (!ent || !ent->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
				if (!ch || ch == m_me || ch->IsDead())
					return;
				if (DISTANCE_APPROX(ch->GetX() - m_me->GetX(), ch->GetY() - m_me->GetY()) > m_radius)
					return;
				if (ch->IsMonster() || ch->IsStone())
					++m_mobs;
				else if (ch->IsPC())
					++m_players;
			}
			LPCHARACTER m_me;
			int m_radius;
			int m_mobs;
			int m_players;
	};

	std::string PlayerBotConvItemName(LPITEM item)
	{
		if (!item || !item->GetProto())
			return std::string();
		std::string name = item->GetProto()->szLocaleName;
		// The table's name already carries the grade ("Pajecza Wlocznia+8"), so
		// appending it said the plus twice.
		if (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR)
			name = playerbot_conv::GearName(name, item->GetRefineLevel());
		else if (item->GetCount() > 1)
		{
			char count[16];
			snprintf(count, sizeof(count), " x%u", (unsigned int)item->GetCount());
			name += count;
		}
		return name;
	}

	// "tarcze" finds "Tarcza Bojowa", "fms" finds "Miecz Pelni Ksiezyca":
	// the players' aliases and Polish endings (playerbot_conv_aliases.h).
	bool PlayerBotConvNameMatches(const char* protoName, const std::string& query)
	{
		return playerbot_conv::ItemNameMatches(protoName, query);
	}

	// The whisper packet, as SendPlayerBotWhisper builds it, without its
	// per-line syslog entry: a conversation is many lines and the debug switch
	// logs them when wanted.
	void SendPlayerBotConvWhisper(LPCHARACTER bot, LPCHARACTER to, const char* text)
	{
		if (!bot || !to || !to->GetDesc() || !text || !*text)
			return;
		const size_t len = std::min<size_t>(strlen(text), CHAT_MAX_LEN);
		TPacketGCWhisper pack;
		pack.bHeader = HEADER_GC_WHISPER;
		pack.bType = WHISPER_TYPE_NORMAL;
		pack.wSize = (WORD)(sizeof(TPacketGCWhisper) + len);
		strlcpy(pack.szNameFrom, bot->GetName(), sizeof(pack.szNameFrom));
		TEMP_BUFFER tmpbuf;
		tmpbuf.write(&pack, sizeof(pack));
		tmpbuf.write(text, (int)len);
		to->GetDesc()->Packet(tmpbuf.read_peek(), tmpbuf.size());
	}

	// ------------------------------------------------------ a Shaman's buffs

	// One buff as CHARACTER::ComputeSkill would put it on `victim`: the skill's
	// own proto, its k (GetSkillPower times bMaxLevel over a hundred) and the
	// variables ComputeSkill sets, in its order - so the numbers are whatever
	// this world's skill_proto says, on either engine, and nothing here knows
	// a formula. Two things are the engine's and not the proto's: the magic
	// weapon's random draw (SetPolyVarForAttack), evaluated at both ends, which
	// is the range a heal lands in; and on mt2009 the cut a buff takes on
	// somebody else (ComputeSkill's IsBuffSkill branch: 75%, the Cure's heal
	// 110% and its shield 70%) with the caster's POINT_SKILL_DURATION. A
	// Grand Master casts the master bonus poly, as ComputeSkill does. Setting
	// the proto's variables is safe: ComputeSkill sets every one of them again
	// before its own Eval.
	void EvaluatePlayerBotBuff(LPCHARACTER bot, LPCHARACTER victim, DWORD vnum, playerbot_conv::TBuffLine& line)
	{
		line = playerbot_conv::TBuffLine();
		line.skill = vnum;
		if (!bot || !victim)
			return;
		line.level = bot->GetSkillLevel(vnum);
		if (line.level <= 0)
			return;
		CSkillProto* pk = CSkillManager::instance().Get(vnum);
		if (!pk)
			return;
		const BYTE level = (BYTE)std::min<int>(line.level, SKILL_MAX_LEVEL);
		const float k = 1.0 * bot->GetSkillPower(vnum, level) * pk->bMaxLevel / 100;
		pk->SetPointVar("k", k);
		if (pk->bPointOn == POINT_MOV_SPEED)
			pk->SetPointVar("maxv", victim->GetLimitPoint(POINT_MOV_SPEED));
#if defined(PLAYERBOT_ENGINE_MT2009)
		pk->SetPointVar("gr", bot->GetSkillMasterType(vnum));
		pk->SetPointVar("sl", line.level);
#endif
		pk->SetPointVar("lv", bot->GetLevel());
		pk->SetPointVar("iq", bot->GetPoint(POINT_IQ));
		pk->SetPointVar("str", bot->GetPoint(POINT_ST));
		pk->SetPointVar("dex", bot->GetPoint(POINT_DX));
		pk->SetPointVar("con", bot->GetPoint(POINT_HT));
		pk->SetPointVar("maxhp", victim->GetMaxHP());
		pk->SetPointVar("maxsp", victim->GetMaxSP());
		pk->SetPointVar("chain", 0);
		pk->SetPointVar("ar", CalcAttackRating(bot, victim));
		pk->SetPointVar("def", bot->GetPoint(POINT_DEF_GRADE));
		pk->SetPointVar("odef", bot->GetPoint(POINT_DEF_GRADE) - bot->GetPoint(POINT_DEF_GRADE_BONUS));
		pk->SetPointVar("horse_level", bot->GetHorseLevel());
		int magicLow = 0;
		int magicHigh = 0;
		int weaponAverage = 0;
		int magicAverage = 0;
		LPITEM weapon = bot->GetWear(WEAR_WEAPON);
		if (weapon && weapon->GetType() == ITEM_WEAPON)
		{
			magicLow = weapon->GetValue(1) + weapon->GetValue(5);
			magicHigh = weapon->GetValue(2) + weapon->GetValue(5);
			weaponAverage = (weapon->GetValue(3) + weapon->GetValue(4)) / 2 + weapon->GetValue(5);
			magicAverage = (weapon->GetValue(1) + weapon->GetValue(2)) / 2 + weapon->GetValue(5);
		}
		pk->SetPointVar("wep", weaponAverage);
#if defined(PLAYERBOT_ENGINE_MT2009)
		pk->SetPointVar("amwep", magicAverage);
#else
		(void)magicAverage;
#endif
		pk->SetDurationVar("k", k);

		const bool grand = bot->GetSkillMasterType(vnum) >= SKILL_GRAND_MASTER;
		pk->SetPointVar("mwep", magicLow);
		pk->SetPointVar("mtk", magicLow);
		int low = grand ? (int)pk->kMasterBonusPoly.Eval() : (int)pk->kPointPoly.Eval();
		pk->SetPointVar("mwep", magicHigh);
		pk->SetPointVar("mtk", magicHigh);
		int high = grand ? (int)pk->kMasterBonusPoly.Eval() : (int)pk->kPointPoly.Eval();
		int amount2 = pk->bPointOn2 != POINT_NONE ? (int)pk->kPointPoly2.Eval() : 0;
		int amount3 = 0;
		if (pk->bPointOn3 != POINT_NONE)
		{
#if defined(PLAYERBOT_ENGINE_MT2009)
			// CanSkillAddThirdPoint: a third point marked self-only stays on the caster.
			if (!IS_SET(pk->dwFlag, SKILL_FLAG_THIRD_POINT_SELFONLY) || victim == bot)
				amount3 = (int)pk->kPointPoly3.Eval();
#else
			// r40250 adds the third point for a Grand Master only.
			if (grand)
				amount3 = (int)pk->kPointPoly3.Eval();
#endif
		}
		int seconds = (int)pk->kDurationPoly.Eval();
		int seconds3 = amount3 != 0 ? (int)pk->kDurationPoly3.Eval() : 0;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (victim != bot)
		{
			const int percent1 = vnum == playerbot_conv::CONV_SKILL_CURE ? 110 : 75;
			const int percent3 = vnum == playerbot_conv::CONV_SKILL_CURE ? 70 : 75;
			low = low * percent1 / 100;
			high = high * percent1 / 100;
			amount2 = amount2 * 75 / 100;
			amount3 = amount3 * percent3 / 100;
		}
		const int durationBonus = bot->GetPoint(POINT_SKILL_DURATION);
		if (seconds > 0)
			seconds += seconds * durationBonus / 100;
		if (seconds3 > 0)
			seconds3 += seconds3 * durationBonus / 100;
#endif
		if (seconds > 0)
			seconds += bot->GetPoint(POINT_PARTY_BUFFER_BONUS);
		if (seconds3 > 0)
			seconds3 += bot->GetPoint(POINT_PARTY_BUFFER_BONUS);
		line.known = true;
		line.amount = std::min(low, high);
		line.amountMax = std::max(low, high);
		line.amount2 = amount2;
		line.amount3 = amount3;
		line.seconds = seconds > 0 ? seconds : 0;
		line.seconds3 = seconds3 > 0 ? seconds3 : 0;
	}

	// The three buffs of a Shaman's path, for the person asking when there is
	// one, the bot itself otherwise.
	bool DescribePlayerBotBuffs(LPCHARACTER bot, LPCHARACTER player, playerbot_conv::TBuffReport& out)
	{
		using namespace playerbot_conv;
		out = TBuffReport();
		if (!bot || bot->GetJob() != JOB_SHAMAN)
			return false;
		const BYTE group = bot->GetSkillGroup();
		if (group != 1 && group != 2)
			return false;
		static const DWORD kDragon[3] = { CONV_SKILL_BLESSING, CONV_SKILL_REFLECT, CONV_SKILL_DRAGON_AID };
		static const DWORD kHealing[3] = { CONV_SKILL_CURE, CONV_SKILL_SWIFTNESS, CONV_SKILL_ATTACK_UP };
		const DWORD* buffs = group == 1 ? kDragon : kHealing;
		LPCHARACTER victim = player ? player : bot;
		out.onAsker = victim != bot;
#if defined(PLAYERBOT_ENGINE_MT2009)
		out.cutForOthers = out.onAsker;
#endif
		for (int i = 0; i < 3; ++i)
			EvaluatePlayerBotBuff(bot, victim, buffs[i], out.lines[out.count++]);
		return true;
	}

	// --------------------------------------------------------- coming over

	// "Chodz do mnie": the walk to the person and a few minutes beside them.
	// The state lives in its own map here, not in TPlayerBotAIState: nothing
	// else writes it, and a field there would have to be initialised in
	// declaration order for -Wreorder's sake.
	const DWORD PLAYERBOT_SUMMON_STAY_MS = 4 * 60 * 1000;       // "a few minutes" beside the person
	const DWORD PLAYERBOT_SUMMON_WALK_MAX_MS = 3 * 60 * 1000;   // a walk not done by then is given up
	const int PLAYERBOT_SUMMON_STAY_DISTANCE = playerbot_conv::CONV_SUMMON_NEAR_DISTANCE;
	const int PLAYERBOT_SUMMON_FOLLOW_DISTANCE = 900;           // the person walked off: after them
	const int PLAYERBOT_SUMMON_OFFSET = 200;                    // beside the person, not on top of them
	const int PLAYERBOT_SUMMON_SNAP_CELLS = 4;
	const int PLAYERBOT_SUMMON_GUARD_RANGE = 1200;              // a monster at the person, this near them
	const int PLAYERBOT_SUMMON_SELF_GUARD_RANGE = 1500;         // a monster at the bot, this near it
	const DWORD PLAYERBOT_SUMMON_GUARD_SCAN_MS = 700;
	const DWORD PLAYERBOT_SUMMON_PRUNE_MS = 30000;
	// The walk's goal snaps up to PLAYERBOT_SUMMON_SNAP_CELLS and MovePlayerBot
	// calls a walk done PLAYERBOT_NAV_ARRIVAL_DISTANCE short: both have to fit
	// inside the distance that counts as beside the person, or the bot stands
	// in the gap for good (the arrival trap in CLAUDE.md, a fourth time).
	static_assert(PLAYERBOT_SUMMON_SNAP_CELLS * PLAYERBOT_NAV_CELL + PLAYERBOT_SUMMON_OFFSET +
			PLAYERBOT_NAV_ARRIVAL_DISTANCE <= PLAYERBOT_SUMMON_STAY_DISTANCE,
			"a summon's walk must end inside the distance that counts as beside the person");
	static_assert(PLAYERBOT_SUMMON_FOLLOW_DISTANCE > PLAYERBOT_SUMMON_STAY_DISTANCE,
			"the walk resumes past the stop distance, or it starts and stops on one step");

	struct TPlayerBotSummon
	{
		DWORD dwPlayerPID;
		DWORD dwStartedAt;      // the walk's clock; held while the bot recovers from a death
		DWORD dwArrivedAt;      // 0 while it walks
		DWORD dwUntil;          // the stay ends then; set on arrival
		DWORD dwNextGuardScanAt;
		DWORD dwGuardVID;
		long lMapIndex;         // the map it was called on; a bot moved off it is released
		bool bFollowing;        // after arrival: walking after the person until close again
		TPlayerBotSummon() : dwPlayerPID(0), dwStartedAt(0), dwArrivedAt(0), dwUntil(0), dwNextGuardScanAt(0),
			dwGuardVID(0), lMapIndex(0), bFollowing(false) {}
	};

	typedef std::map<DWORD, TPlayerBotSummon> TPlayerBotSummonMap;
	TPlayerBotSummonMap s_mapPlayerBotSummons;   // by the bot's pid
	DWORD s_dwPlayerBotSummonPruneTime = 0;

	// Defined with the targeting (playerbot_targeting.h), which comes later.
	bool ExecutePlayerBotBasicAttack(LPCHARACTER ch, LPCHARACTER target, TPlayerBotAIState& state, DWORD dwNow);

	// Whether a bot is on its way to, or standing with, somebody who called it.
	// The passes that would take it away - the market, the service walk to its
	// own stand, the world travel, the town errands - ask
	// IsPlayerBotHeldForCompany, which is where this belongs. Inline because
	// its callers are in later files, and a build without them must not warn.
	inline bool IsPlayerBotSummoned(DWORD botPID)
	{
		return s_mapPlayerBotSummons.find(botPID) != s_mapPlayerBotSummons.end();
	}

	DWORD GetPlayerBotSummonerPID(DWORD botPID)
	{
		TPlayerBotSummonMap::const_iterator it = s_mapPlayerBotSummons.find(botPID);
		return it != s_mapPlayerBotSummons.end() ? it->second.dwPlayerPID : 0;
	}

	// A person in the bot's party who is not the one asking.
	bool PlayerBotPartyHasOtherPerson(LPPARTY party, LPCHARACTER asker)
	{
		if (!party)
			return false;
		struct FFindOtherPerson
		{
			LPCHARACTER asker;
			bool found;
			explicit FFindOtherPerson(LPCHARACTER a) : asker(a), found(false) {}
			void operator()(LPCHARACTER member)
			{
				if (member && member != asker && member->IsPC() &&
						(!member->GetDesc() || !member->GetDesc()->IsBot()))
					found = true;
			}
		};
		FFindOtherPerson finder(asker);
		party->ForEachOnlineMember(finder);
		return finder.found;
	}

	// Why the bot cannot come to `player` now (playerbot_conv::ESummonBlock).
	// Everything here is something that owns the bot for a reason of its own:
	// a counter it stands behind, a session at the water or the vein, a fight
	// the engine made it part of, another person's claim.
	int GetPlayerBotSummonBlock(LPCHARACTER bot, const TPlayerBotAIState& state, LPCHARACTER player, DWORD dwNow)
	{
		using namespace playerbot_conv;
		if (!bot || !player)
			return SB_OTHER_MAP;
		if (bot->IsDead())
			return SB_DEAD;
		const long mapIndex = bot->GetMapIndex();
		if (IsPlayerBotDemonTowerInstance(mapIndex) || state.lTowerInstance != 0 ||
				state.dwTowerRaidGuild != 0 || state.bTowerSummoned)
			return SB_TOWER;
		if (mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			return SB_DUNGEON;
		if (player->GetMapIndex() != mapIndex)
			return SB_OTHER_MAP;
		if (playerbot_pvp::IsInDuel(bot->GetPlayerID(), dwNow))
			return SB_DUEL;
		if (state.dwGuildWarEnemyGID != 0)
			return SB_GUILD_WAR;
		if (bot->GetMyShop())
			return SB_STALL;
		if (state.bFishingSession || state.bIsFishing)
			return SB_FISHING;
		if (IsPlayerBotMiningNow(bot->GetPlayerID(), dwNow))
			return SB_MINING;
		if (IsPlayerBotOnMercContract(bot->GetPlayerID()))
			return SB_MERC;
		if (state.dwLurePlayerPID != 0 && state.dwLurePlayerPID != player->GetPlayerID())
			return SB_OTHER_PARTY;
		// A person's party, or a companion holding a person in its own: the
		// follow pass keeps such a bot beside them, and a walk to somebody
		// else would be undone on the next tick. The leader's pid answers for
		// a person on another core, whose character this one cannot see.
		LPPARTY party = bot->GetParty();
		if (party && party != player->GetParty() &&
				(IsPlayerBotHumanLedParty(party) || PlayerBotPartyHasOtherPerson(party, player)))
			return SB_OTHER_PARTY;
		const DWORD summoner = GetPlayerBotSummonerPID(bot->GetPlayerID());
		if (summoner != 0 && summoner != player->GetPlayerID())
			return SB_OTHER_SUMMON;
		return SB_NONE;
	}

	// The stay is over: the bot goes back to its own life. What it was doing
	// before kept its flags and resumes; only the walk's route and a guard's
	// target are dropped. A bot that leaves by itself says so, if the person is
	// still in the game to read it.
	void EndPlayerBotSummon(DWORD botPID, int reason, DWORD dwNow)
	{
		using namespace playerbot_conv;
		TPlayerBotSummonMap::iterator it = s_mapPlayerBotSummons.find(botPID);
		if (it == s_mapPlayerBotSummons.end())
			return;
		const TPlayerBotSummon summon = it->second;
		s_mapPlayerBotSummons.erase(it);
		LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(botPID);
		LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(summon.dwPlayerPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(botPID);
		if (bot && st != s_mapPlayerBotAIStates.end())
		{
			TPlayerBotAIState& state = st->second;
			if (summon.dwGuardVID != 0 && state.dwTargetVID == summon.dwGuardVID)
			{
				state.dwTargetVID = 0;
				bot->SetVictim(NULL);
			}
			ClearPlayerBotRoute(state, true);
			state.dwLastMeaningfulActivityTime = dwNow;
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		}
		sys_log(0, "PLAYERBOT_SUMMON: over pid=%u name=%s by=%s reason=%s walked_ms=%u stayed_ms=%u",
				botPID, bot ? bot->GetName() : "?", player ? player->GetName() : "?", SummonEndName(reason),
				summon.dwArrivedAt ? summon.dwArrivedAt - summon.dwStartedAt : dwNow - summon.dwStartedAt,
				summon.dwArrivedAt ? dwNow - summon.dwArrivedAt : 0);
		const char* words = NULL;
		switch (reason)
		{
			case SUMMON_END_EXPIRED: words = "Dobra, musze wracac do swoich spraw. Na razie!"; break;
			case SUMMON_END_UNREACHABLE: words = "Nie moge do ciebie dojsc, sorki. Wracam do swoich spraw."; break;
			case SUMMON_END_BLOCKED: words = "Musze isc, cos mi wypadlo."; break;
			case SUMMON_END_PLAYER_LEFT: words = "Poszedles gdzies, to wracam do swoich spraw."; break;
			default: break;
		}
		if (words && bot && player && player->GetDesc())
			SendPlayerBotConvWhisper(bot, player, words);
	}

	// The conversation's "chodz do mnie": the gates asked once more, and the walk
	// begins (playerbot_conv::ESummonStart). The same person calling again
	// starts the stay over.
	int StartPlayerBotSummon(LPCHARACTER bot, LPCHARACTER player, DWORD dwNow)
	{
		using namespace playerbot_conv;
		if (!bot || !player || !player->GetDesc())
			return SUMMON_START_FAILED;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return SUMMON_START_FAILED;
		TPlayerBotAIState& state = st->second;
		TPlayerBotSummonMap::iterator it = s_mapPlayerBotSummons.find(bot->GetPlayerID());
		if (it != s_mapPlayerBotSummons.end() && it->second.dwPlayerPID == player->GetPlayerID())
		{
			if (it->second.dwArrivedAt != 0)
				it->second.dwUntil = dwNow + PLAYERBOT_SUMMON_STAY_MS;
			sys_log(0, "PLAYERBOT_SUMMON: renewed pid=%u name=%s by=%s arrived=%d",
					bot->GetPlayerID(), bot->GetName(), player->GetName(), it->second.dwArrivedAt ? 1 : 0);
			return SUMMON_START_RENEWED;
		}
		const int block = GetPlayerBotSummonBlock(bot, state, player, dwNow);
		if (block != SB_NONE)
		{
			sys_log(0, "PLAYERBOT_SUMMON: refused pid=%u name=%s by=%s reason=%s",
					bot->GetPlayerID(), bot->GetName(), player->GetName(), SummonBlockName(block));
			return SUMMON_START_BLOCKED;
		}
		TPlayerBotSummon summon;
		summon.dwPlayerPID = player->GetPlayerID();
		summon.dwStartedAt = dwNow;
		summon.lMapIndex = bot->GetMapIndex();
		s_mapPlayerBotSummons[bot->GetPlayerID()] = summon;
		// The bot's own fight and route are dropped; its errands keep their
		// flags and wait for the stay to end.
		state.dwTargetVID = 0;
		bot->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		state.dwLastMeaningfulActivityTime = dwNow;
		sys_log(0, "PLAYERBOT_SUMMON: called pid=%u name=%s by=%s map=%ld distance=%d",
				bot->GetPlayerID(), bot->GetName(), player->GetName(), bot->GetMapIndex(),
				DISTANCE_APPROX(bot->GetX() - player->GetX(), bot->GetY() - player->GetY()));
		return SUMMON_START_OK;
	}

	// "mozesz isc" from the person who called: ends the stay (ESummonEnd).
	int EndPlayerBotSummonBy(LPCHARACTER bot, LPCHARACTER player, DWORD dwNow)
	{
		if (!bot || !player || GetPlayerBotSummonerPID(bot->GetPlayerID()) != player->GetPlayerID())
			return playerbot_conv::SUMMON_END_NOT_SUMMONED;
		EndPlayerBotSummon(bot->GetPlayerID(), playerbot_conv::SUMMON_END_DISMISSED, dwNow);
		return playerbot_conv::SUMMON_END_DISMISSED;
	}

	// A monster that is after the bot or after the person: the only fights a
	// summoned bot takes. The nearest to the bot.
	struct FPlayerBotSummonThreat
	{
		LPCHARACTER m_bot;
		LPCHARACTER m_player;
		LPCHARACTER m_best;
		int m_bestDistance;
		FPlayerBotSummonThreat(LPCHARACTER bot, LPCHARACTER player)
			: m_bot(bot), m_player(player), m_best(NULL), m_bestDistance(INT_MAX) {}
		void operator () (LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER mob = static_cast<LPCHARACTER>(ent);
			if (!mob->IsMonster() || mob->IsDead())
				return;
			LPCHARACTER victim = mob->GetVictim();
			const int toBot = DISTANCE_APPROX(mob->GetX() - m_bot->GetX(), mob->GetY() - m_bot->GetY());
			if (victim == m_bot)
			{
				if (toBot > PLAYERBOT_SUMMON_SELF_GUARD_RANGE)
					return;
			}
			else if (victim == m_player)
			{
				if (DISTANCE_APPROX(mob->GetX() - m_player->GetX(), mob->GetY() - m_player->GetY()) >
						PLAYERBOT_SUMMON_GUARD_RANGE)
					return;
			}
			else
				return;
			if (IsPlayerBotSafeZone(mob->GetMapIndex(), mob->GetX(), mob->GetY()))
				return;
			if (toBot < m_bestDistance)
			{
				m_best = mob;
				m_bestDistance = toBot;
			}
		}
	};

	// The summon's part of the tick, right after the follow pass
	// (playerbot_manager.cpp). While a summon holds it claims every full tick,
	// because everything below would take the bot somewhere of its own - the
	// loot, the travel, the town, the wander, the hunt - and it does for
	// itself what those passes would have: the recovery after a death, the
	// potions, and the fights it is allowed, which are the monsters after it or
	// after the person. The light tick keeps walking the route and swinging at
	// the guard's target between two full ticks. Standing beside the person is
	// the errand, so the inactivity watchdog is told so every tick.
	bool ManagePlayerBotSummon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		using namespace playerbot_conv;
		if (!ch)
			return false;
		TPlayerBotSummonMap::iterator it = s_mapPlayerBotSummons.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotSummons.end())
			return false;
		TPlayerBotSummon& summon = it->second;
		LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(summon.dwPlayerPID);
		int end = SUMMON_END_NONE;
		if (!player || !player->GetDesc())
			end = SUMMON_END_PLAYER_GONE;
		// Moved off the map it was called on - a warp, a dungeon's jump, a GM:
		// whatever did it had the better claim.
		else if (ch->GetMapIndex() != summon.lMapIndex)
			end = SUMMON_END_BLOCKED;
		else if (player->GetMapIndex() != ch->GetMapIndex())
			end = SUMMON_END_PLAYER_LEFT;
		else if (summon.dwArrivedAt == 0 && dwNow - summon.dwStartedAt > PLAYERBOT_SUMMON_WALK_MAX_MS)
			end = SUMMON_END_UNREACHABLE;
		else if (summon.dwArrivedAt != 0 && (int)(dwNow - summon.dwUntil) >= 0)
			end = SUMMON_END_EXPIRED;
		else
		{
			const int block = GetPlayerBotSummonBlock(ch, state, player, dwNow);
			if (block != SB_NONE && block != SB_DEAD)
				end = SUMMON_END_BLOCKED;
		}
		if (end != SUMMON_END_NONE)
		{
			EndPlayerBotSummon(ch->GetPlayerID(), end, dwNow);
			return false;
		}
		if (ch->IsDead())
			return false;
		// What the passes below would have done to keep it standing. A bot
		// standing up from a death is healing, invisible, and not walking, so
		// the walk's clock is held until it is on its feet again.
		if (HandlePostDeathRecovery(ch, state, dwNow))
		{
			if (summon.dwArrivedAt == 0)
				summon.dwStartedAt = dwNow;
			return true;
		}
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();

		// The guard: a monster after the bot or the person.
		LPCHARACTER threat = summon.dwGuardVID ? CHARACTER_MANAGER::instance().Find(summon.dwGuardVID) : NULL;
		if (threat && (threat->IsDead() || threat->GetMapIndex() != ch->GetMapIndex() ||
				(threat->GetVictim() != ch && threat->GetVictim() != player)))
			threat = NULL;
		if (!threat && dwNow >= summon.dwNextGuardScanAt && ch->GetSectree())
		{
			summon.dwNextGuardScanAt = dwNow + PLAYERBOT_SUMMON_GUARD_SCAN_MS;
			FPlayerBotSummonThreat finder(ch, player);
			ch->GetSectree()->ForEachAround(finder);
			threat = finder.m_best;
		}
		if (!threat && summon.dwGuardVID != 0)
		{
			if (state.dwTargetVID == summon.dwGuardVID)
			{
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
			}
			summon.dwGuardVID = 0;
		}
		if (threat)
		{
			summon.dwGuardVID = (DWORD)threat->GetVID();
			state.dwTargetVID = summon.dwGuardVID;
			if (ch->IsRiding() && !CanPlayerBotFightOnHorse(ch, threat))
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "summon_guard");
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			const bool bow = weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW;
			const int reach = bow ? 750 : 250;
			if (DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY()) > reach)
				MovePlayerBot(ch, threat->GetX(), threat->GetY(), dwNow, 2, true, false, false, false);
			else
			{
				if (ch->IsStateMove())
					ch->Stop();
				ExecutePlayerBotBasicAttack(ch, threat, state, dwNow);
			}
			SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
			return true;
		}

		const int distance = DISTANCE_APPROX(ch->GetX() - player->GetX(), ch->GetY() - player->GetY());
		if (summon.dwArrivedAt == 0 && distance <= PLAYERBOT_SUMMON_STAY_DISTANCE)
		{
			summon.dwArrivedAt = dwNow;
			summon.dwUntil = dwNow + PLAYERBOT_SUMMON_STAY_MS;
			ClearPlayerBotRoute(state, true);
			if (ch->IsStateMove())
				ch->Stop();
			sys_log(0, "PLAYERBOT_SUMMON: arrived pid=%u name=%s by=%s walk_ms=%u",
					ch->GetPlayerID(), ch->GetName(), player->GetName(), dwNow - summon.dwStartedAt);
		}
		// After arrival the bot stays put while the person moves about near it,
		// and once they are past PLAYERBOT_SUMMON_FOLLOW_DISTANCE it walks after
		// them until it is close again - two distances, or a person standing on
		// the edge of one would start and stop it on every step.
		if (summon.dwArrivedAt != 0)
		{
			if (distance > PLAYERBOT_SUMMON_FOLLOW_DISTANCE)
				summon.bFollowing = true;
			else if (distance <= PLAYERBOT_SUMMON_STAY_DISTANCE)
				summon.bFollowing = false;
		}
		if (summon.dwArrivedAt == 0 || summon.bFollowing)
		{
			// Beside the person, at a place of its own round them.
			static const int kSide[8][2] = {
				{ 200, 0 }, { 141, 141 }, { 0, 200 }, { -141, 141 }, { -200, 0 }, { -141, -141 }, { 0, -200 }, { 141, -141 } };
			const int* side = kSide[ch->GetPlayerID() % 8];
			const long goalX = player->GetX() + side[0] * PLAYERBOT_SUMMON_OFFSET / 200;
			const long goalY = player->GetY() + side[1] * PLAYERBOT_SUMMON_OFFSET / 200;
			// The horse for a long walk; UpdatePlayerBotTravelMount refuses it
			// for a short one by itself.
			MovePlayerBot(ch, goalX, goalY, dwNow, PLAYERBOT_SUMMON_SNAP_CELLS, true, true, false, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			return true;
		}
		if (!state.vecRoute.empty())
			ClearPlayerBotRoute(state, true);
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// The summons of bots that are no longer in the game.
	void PrunePlayerBotSummons(DWORD dwNow)
	{
		if (s_dwPlayerBotSummonPruneTime != 0 && dwNow - s_dwPlayerBotSummonPruneTime < PLAYERBOT_SUMMON_PRUNE_MS)
			return;
		s_dwPlayerBotSummonPruneTime = dwNow;
		for (TPlayerBotSummonMap::iterator it = s_mapPlayerBotSummons.begin(); it != s_mapPlayerBotSummons.end(); )
		{
			if (s_mapPlayerBotAIStates.find(it->first) == s_mapPlayerBotAIStates.end() ||
					!CHARACTER_MANAGER::instance().FindByPID(it->first))
				s_mapPlayerBotSummons.erase(it++);
			else
				++it;
		}
	}

	// The line over the bot's head while it is called (playerbot_status.h may
	// ask it the way it asks BuildPlayerBotMercStatus). A fight says what it
	// is fighting. Inline because nothing in this file calls it.
	inline bool BuildPlayerBotSummonStatus(LPCHARACTER ch, const TPlayerBotAIState& state, const char* prefix,
			char* status, size_t statusSize, bool en)
	{
		if (!ch || !status || statusSize == 0 || state.bCurrentAction == BOT_ACTION_FIGHT)
			return false;
		TPlayerBotSummonMap::const_iterator it = s_mapPlayerBotSummons.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotSummons.end())
			return false;
		LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(it->second.dwPlayerPID);
		const char* who = player ? player->GetName() : PBT(en, "gracza", "a player");
		if (it->second.dwArrivedAt == 0)
			snprintf(status, statusSize, PBT(en, "%sIde do %s", "%sGoing to %s"), prefix ? prefix : "", who);
		else
			snprintf(status, statusSize, PBT(en, "%sStoje przy %s", "%sStaying with %s"), prefix ? prefix : "", who);
		return true;
	}

	// --------------------------------------------------------------- world

	// The cheapest single-piece price of a matching line, per piece for stacks.
	void NotePlayerBotConvMarketLines(const TPlayerBotStall& stall, const std::vector<std::string>& candidates,
			DWORD skill, std::string& outName, long long& outPrice, unsigned int& outSellers, DWORD& seenVnum)
	{
		for (size_t i = 0; i < stall.lines.size(); ++i)
		{
			const TPlayerBotStallLine& line = stall.lines[i];
			if (!PlayerBotStallLineMatches(line, candidates, skill != 0, skill))
				continue;
			const long long unit = line.count > 1 ? line.price / line.count : line.price;
			if (unit <= 0)
				continue;
			seenVnum = line.vnum;
			++outSellers;
			if (outPrice == 0 || unit < outPrice)
			{
				outPrice = unit;
				outName = line.name;
			}
			return; // one line per stall is enough for "the cheapest"
		}
	}

	class CPlayerBotConvWorld : public playerbot_conv::IConvWorld
	{
		public:
			CPlayerBotConvWorld() : m_bot(NULL), m_player(NULL) {}
			void Bind(LPCHARACTER bot, LPCHARACTER player) { m_bot = bot; m_player = player; }

			bool FindItem(const std::string& query, std::string& outName, unsigned int& outCount)
			{
				if (!m_bot || !m_bot->IsItemLoaded())
					return false;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				{
					LPITEM item = m_bot->GetInventoryItem(cell);
					if (item && item->GetProto() && PlayerBotConvNameMatches(item->GetProto()->szLocaleName, query))
					{
						outName = item->GetProto()->szLocaleName;
						outCount = (unsigned int)item->GetCount();
						return true;
					}
				}
				const BYTE worn[] = { WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR };
				for (size_t i = 0; i < sizeof(worn) / sizeof(worn[0]); ++i)
				{
					LPITEM item = m_bot->GetWear(worn[i]);
					if (item && item->GetProto() && PlayerBotConvNameMatches(item->GetProto()->szLocaleName, query))
					{
						outName = PlayerBotConvItemName(item) + " (na sobie)";
						outCount = 1;
						return true;
					}
				}
				return false;
			}

			// "masz na straganie X?": the bot's own stall - classic or the Ikarus
			// offline shop (GetPlayerBotStall, playerbot_chat_trade.h).
			bool FindShopItem(const std::string& query, std::string& outName, long long& outPrice,
					unsigned int& outCount)
			{
				if (!m_bot)
					return false;
				TPlayerBotStall stall;
				if (!GetPlayerBotStall(m_bot->GetPlayerID(), m_bot, stall))
					return false;
				std::string rest;
				const DWORD skill = GetPlayerBotStallBookQuery(query, rest);
				std::vector<std::string> candidates;
				playerbot_conv::ExpandItemQuery(query, candidates);
				for (size_t i = 0; i < stall.lines.size(); ++i)
				{
					if (!PlayerBotStallLineMatches(stall.lines[i], candidates, skill != 0, skill))
						continue;
					outName = stall.lines[i].name;
					outPrice = stall.lines[i].price;
					outCount = stall.lines[i].count;
					return true;
				}
				return false;
			}

			// "ile chodzi X?": the cheapest line of X on this channel's stalls
			// (the other bots' and, on mt2009, every offline shop), else the
			// sale memory's median for it.
			bool FindMarketPrice(const std::string& query, std::string& outName, long long& outPrice,
					unsigned int& outSellers)
			{
				std::string rest;
				const DWORD skill = GetPlayerBotStallBookQuery(query, rest);
				std::vector<std::string> candidates;
				playerbot_conv::ExpandItemQuery(query, candidates);
				outPrice = 0;
				outSellers = 0;
				DWORD seenVnum = 0;
				const DWORD self = m_bot ? m_bot->GetPlayerID() : 0;
				TPlayerBotStall stall;
				for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
						it != s_mapPlayerBotAIStates.end(); ++it)
				{
					if (it->first == self)
						continue;
					LPCHARACTER keeper = CHARACTER_MANAGER::instance().FindByPID(it->first);
					if (!keeper || !keeper->GetMyShop() || !GetPlayerBotStall(it->first, keeper, stall))
						continue;
					NotePlayerBotConvMarketLines(stall, candidates, skill, outName, outPrice, outSellers, seenVnum);
				}
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
				for (const auto& entry : ikashop::GetManager().GetPlayerBotOfflineShops())
				{
					if (entry.first == self || !entry.second || entry.second->GetDuration() == 0 ||
							entry.second->GetSpawn().channel != g_bChannel)
						continue;
					if (!GetPlayerBotStall(entry.first, NULL, stall))
						continue;
					NotePlayerBotConvMarketLines(stall, candidates, skill, outName, outPrice, outSellers, seenVnum);
				}
#endif
				if (outPrice > 0)
					return true;
				if (seenVnum)
				{
					size_t samples = 0;
					const DWORD unit = GetPlayerBotSaleUnitPrice(seenVnum, 0, get_dword_time(), &samples, skill);
					if (unit > 0)
					{
						outPrice = unit;
						return true;
					}
				}
				return false;
			}

			// "kupisz ode mnie X": whether the bot is short of that material.
			std::string AnswerSell(const std::string& query)
			{
				if (!m_bot)
					return std::string();
				DWORD wanted = 0;
				const std::set<DWORD>& materials = GetPlayerBotRefineMaterialVnums();
				for (std::set<DWORD>::const_iterator it = materials.begin(); it != materials.end(); ++it)
				{
					const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
					if (proto && PlayerBotConvNameMatches(proto->szLocaleName, query))
					{
						wanted = *it;
						break;
					}
				}
				if (!wanted)
					return "Tego raczej nie szukam.";
				if (!PlayerBotNeedsRefineMaterial(m_bot, wanted))
					return "Mam tego na razie dosc.";
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(wanted);
				char reply[CHAT_MAX_LEN + 1];
				snprintf(reply, sizeof(reply), "O, %s mi sie przyda. Wystaw na straganie, na pewno zajrze.",
						proto ? proto->szLocaleName : query.c_str());
				return reply;
			}

			// "co daja twoje buffy?": the three of the bot's path, evaluated as
			// the engine would cast them on the person asking.
			bool DescribeBuffs(playerbot_conv::TBuffReport& out)
			{
				return DescribePlayerBotBuffs(m_bot, m_player, out);
			}

			// "chodz do mnie" and "mozesz isc", made at the moment the reply is
			// composed, which is when the snapshot that decided them was read.
			int StartSummon()
			{
				return StartPlayerBotSummon(m_bot, m_player, get_dword_time());
			}

			int EndSummon()
			{
				return EndPlayerBotSummonBy(m_bot, m_player, get_dword_time());
			}

		private:
			LPCHARACTER m_bot;
			LPCHARACTER m_player;
	};

	// ---------------------------------------------------------------- host

	class CPlayerBotConvHost : public playerbot_conv::IConvHost
	{
		public:
			bool BuildSnapshot(playerbot_conv::u32 playerPID, playerbot_conv::u32 botPID, playerbot_conv::TBotSnapshot& s)
			{
				using namespace playerbot_conv;
				LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(botPID);
				LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(playerPID);
				if (!bot || !player || !player->GetDesc())
					return false;
				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(botPID);
				if (it == s_mapPlayerBotAIStates.end())
					return false;
				const TPlayerBotAIState& state = it->second;
				const DWORD now = get_dword_time();

				s = TBotSnapshot();
				s.name = bot->GetName();
				s.askerName = player->GetName();
				s.level = bot->GetLevel();
				s.job = bot->GetJob();
				s.empire = bot->GetEmpire();
				const long mapIndex = bot->GetMapIndex();
				const long baseMap = mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? mapIndex / 10000 : mapIndex;
				s.mapIndex = baseMap;
				s.inDungeon = mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN;
				s.inTown = IsPlayerBotVillageMap(baseMap);
				s.safeZone = IsPlayerBotSafeZone(mapIndex, bot->GetX(), bot->GetY());
				s.action = MapPlayerBotConvAction(state.bCurrentAction);
				s.goal = MapPlayerBotConvGoal(state.bLongTermGoal);
				s.travelMap = state.lRouteMapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
					? state.lRouteMapIndex / 10000 : state.lRouteMapIndex;
				s.riding = bot->IsRiding();

				if (state.dwTargetVID)
				{
					LPCHARACTER target = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
					if (target && !target->IsDead())
					{
						s.targetName = target->GetName();
						s.targetStone = target->IsStone();
						s.targetBoss = target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS;
						s.targetPlayer = target->IsPC();
					}
				}

				s.dead = bot->IsDead();
				s.hpPct = bot->GetMaxHP() > 0 ? (int)((long long)bot->GetHP() * 100 / bot->GetMaxHP()) : 100;
				s.spPct = bot->GetMaxSP() > 0 ? (int)((long long)bot->GetSP() * 100 / bot->GetMaxSP()) : 100;
				if (s.hpPct < 0) s.hpPct = 0;
				if (s.spPct < 0) s.spPct = 0;
				const DWORD nextExp = bot->GetNextExp();
				s.expPct = nextExp > 0 ? (int)((unsigned long long)bot->GetExp() * 100ULL / nextExp) : -1;
				s.gold = (long long)bot->GetGold();
				s.horseLevel = bot->GetHorseLevel();
				// The path and its skills as the build knows them (playerbot_skills.h):
				// "jestes body czy mental?" and "jaki masz skill?" read these.
				s.skillGroup = bot->GetSkillGroup();
				{
					const TJobSkillBuild build = GetPlayerBotSkillBuild(bot->GetJob(), bot->GetSkillGroup(), botPID);
					for (BYTE i = 0; i < build.bSkillCount && i < 6; ++i)
					{
						s.skillVnums[i] = build.dwSkills[i];
						s.skillLevels[i] = build.dwSkills[i] ? bot->GetSkillLevel(build.dwSkills[i]) : 0;
					}
					s.mainSkill = build.dwPrimaryMaxSkill;
				}

				LPPARTY party = bot->GetParty();
				if (party)
				{
					s.inParty = true;
					s.partySize = (int)party->GetMemberCount();
					LPCHARACTER leader = party->GetLeaderCharacter();
					s.partyLeader = leader ? leader->GetName() : "";
					s.leaderIsMe = party->GetLeaderPID() == botPID;
					s.askerInParty = player->GetParty() == party;
				}
				CGuild* guild = bot->GetGuild();
				if (guild)
				{
					s.inGuild = true;
					s.guildName = guild->GetName();
					s.guildMembers = guild->GetMemberCount();
				}

				if (bot->IsItemLoaded())
				{
					s.bagCells = PLAYERBOT_BAG_CELLS;
					// Free is what the item grid says: a cell with no item
					// pointer is also the bottom of every sword and armour.
					s.freeCells = CountPlayerBotFreeInventoryCells(bot);
					size_t listed = 0;
					for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
					{
						LPITEM item = bot->GetInventoryItem(cell);
						if (!item)
							continue;
						if (listed < PLAYERBOT_CONV_BAG_SUMMARY_ITEMS && item->GetProto())
						{
							if (!s.bagSummary.empty())
								s.bagSummary += ", ";
							s.bagSummary += PlayerBotConvItemName(item);
							++listed;
						}
					}
				}
				LPITEM weapon = bot->GetWear(WEAR_WEAPON);
				if (weapon && weapon->GetProto())
				{
					s.weaponName = weapon->GetProto()->szLocaleName;
					s.weaponPlus = weapon->GetRefineLevel();
					s.weaponLevel = weapon->GetLevelLimit();
				}
				// "czemu biegasz z bronia na 15 level?" is answered from what the AI
				// is playing for (playerbot_weapon_goal.h) - read from its cache and
				// never worked out from here, because the conversation writes
				// nothing to the AI. A bot whose goal was never read says no goal.
				{
					std::map<DWORD, TPlayerBotWeaponGoal>::const_iterator known = s_mapPlayerBotWeaponGoals.find(botPID);
					LPITEM hand = GetPlayerBotHandWeapon(bot);
					if (known != s_mapPlayerBotWeaponGoals.end() && known->second.when != 0 && known->second.family)
					{
						const TPlayerBotWeaponGoal& goal = known->second;
						const TItemTable* goalProto = ITEM_MANAGER::instance().GetTable(goal.family->dwBaseVnum);
						if (goalProto)
						{
							s.weaponGoal = playerbot_conv::GearName(goalProto->szLocaleName, 0);
							s.weaponGoalPrice = (long long)GetPlayerBotWeaponGoalPrice(goal.family);
						}
						s.weaponIsGoal = hand && hand->GetVnum() - (DWORD)hand->GetRefineLevel() == goal.family->dwBaseVnum;
						if (s.weaponIsGoal)
							s.weaponOutclassed = false;
						else if (hand && goal.handVnum == hand->GetVnum())
							s.weaponOutclassed = IsPlayerBotWeaponOutclassed(goal);
						else
						{
							// The hand changed since the goal was read: its blow now.
							const long long handBlow = hand ? GetPlayerBotWeaponHitDamage(hand, bot) : 0;
							s.weaponOutclassed = goal.goalBlow * 100 >= handBlow * (100 + PLAYERBOT_WEAPON_OUTCLASSED_PERCENT);
						}
					}
				}
				LPITEM body = bot->GetWear(WEAR_BODY);
				if (body && body->GetProto())
				{
					s.armorName = body->GetProto()->szLocaleName;
					s.armorPlus = body->GetRefineLevel();
				}

				s.fishing = state.bFishingSession || state.bIsFishing;
				s.mining = IsPlayerBotMiningNow(botPID, now);
				s.herbUnlocked = IsPlayerBotHerbalismUnlocked(bot);
				for (size_t i = 0; i < sizeof(PLAYERBOT_BIOLOGIST_MISSIONS) / sizeof(PLAYERBOT_BIOLOGIST_MISSIONS[0]); ++i)
				{
					if (!IsPlayerBotBiologistMissionOpen(bot, i))
						continue;
					const DWORD wanted = GetPlayerBotBiologistWantedItem(bot, i, NULL);
					const TItemTable* proto = wanted ? ITEM_MANAGER::instance().GetTable(wanted) : NULL;
					if (proto)
					{
						s.bioWanted = proto->szLocaleName;
						break;
					}
				}
				{
					int remaining = 0;
					const DWORD huntVnum = GetActivePlayerBotHuntingMobVnum(bot, &remaining);
					const CMob* mob = huntVnum ? CMobManager::instance().Get(huntVnum) : NULL;
					if (mob)
					{
						s.huntMob = mob->m_table.szLocaleName;
						s.huntRemaining = remaining;
					}
				}
				s.metinHunter = state.bBotRole == BOT_ROLE_METIN_HUNTER || state.bLongTermGoal == BOT_GOAL_HUNT_METIN;
				s.demonTower = IsPlayerBotDemonTowerInstance(mapIndex) || state.bTowerSummoned || state.dwTowerRaidGuild != 0;
				s.guildWar = state.dwGuildWarEnemyGID != 0;
				s.mercContract = IsPlayerBotOnMercContract(botPID);
				s.luring = state.bCurrentAction == BOT_ACTION_LURE;
				s.luringForAsker = state.dwLurePlayerPID == playerPID;

				{
					TPlayerBotStall stall;
					s.shopStanding = bot->GetMyShop() != NULL;
					if (GetPlayerBotStall(botPID, bot, stall))
					{
						s.shopOpen = true;
						s.shopMapIndex = stall.mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? stall.mapIndex / 10000 : stall.mapIndex;
						s.shopOtherChannel = stall.channel != g_bChannel;
						s.shopItems = (int)stall.lines.size();
						for (size_t i = 0; i < stall.lines.size() && i < PLAYERBOT_CONV_SHOP_SUMMARY_ITEMS; ++i)
						{
							if (!s.shopSummary.empty())
								s.shopSummary += ", ";
							s.shopSummary += stall.lines[i].name;
							if (stall.lines[i].count > 1)
								s.shopSummary += " x" + ToString((long long)stall.lines[i].count);
							s.shopSummary += " za ";
							s.shopSummary += FormatYang(stall.lines[i].price);
						}
					}
				}
				s.dragonCoins = state.iDragonCoins;
				s.dragonKnown = state.bDragonBalanceKnown;
				s.marketTrip = state.bMarketTrip;

				if (bot->GetSectree())
				{
					FPlayerBotConvCountAround around(bot, PLAYERBOT_CONV_AROUND_RADIUS);
					bot->GetSectree()->ForEachAround(around);
					s.mobsNear = around.m_mobs;
					s.playersNear = around.m_players;
				}

				s.style = MapPlayerBotConvStyle(state);
				s.mood = MapPlayerBotConvMood(bot, state, now);
				if (IsPlayerBotPersonaEnabled() && state.persona.bRestored)
				{
					s.unlucky = state.persona.mood.droughtMs >= 20u * 60u * 1000u;
					s.euphoria = state.persona.mood.lockKind == playerbot_persona::MOOD_LOCK_EUPHORIA &&
							state.persona.mood.lockLeftMs > 0;
				}
				s.affinity = GetPlayerBotAffinity(state, playerPID);
				s.onlineMinutes = state.dwSpawnTime ? (now - state.dwSpawnTime) / 60000 : 0;
				s.goalMinutes = state.dwGoalStartedTime ? (now - state.dwGoalStartedTime) / 60000 : 0;
				s.actionMinutes = state.dwActionChangedTime ? (now - state.dwActionChangedTime) / 60000 : 0;
				if (state.dwLastDeathTime)
				{
					s.minutesSinceDeath = (now - state.dwLastDeathTime) / 60000;
					if (s.minutesSinceDeath < 60)
						s.recentDeaths = state.bDeathCount > 0 ? state.bDeathCount : 1;
				}
				s.askerLevel = player->GetLevel();
				s.askerNear = player->GetMapIndex() == mapIndex &&
						DISTANCE_APPROX(player->GetX() - bot->GetX(), player->GetY() - bot->GetY()) <= PLAYERBOT_CONV_NEAR_RADIUS;
				// "chodz do mnie": where the person is, whether the bot is already
				// called, and what would stop it coming now.
				s.askerOnMap = player->GetMapIndex() == mapIndex;
				s.askerDistance = s.askerOnMap
						? DISTANCE_APPROX(player->GetX() - bot->GetX(), player->GetY() - bot->GetY()) : -1;
				{
					TPlayerBotSummonMap::const_iterator sm = s_mapPlayerBotSummons.find(botPID);
					if (sm != s_mapPlayerBotSummons.end())
					{
						s.summoned = true;
						s.summonedByAsker = sm->second.dwPlayerPID == playerPID;
						s.summonArrived = sm->second.dwArrivedAt != 0;
						if (!s.summonedByAsker)
						{
							LPCHARACTER summoner = CHARACTER_MANAGER::instance().FindByPID(sm->second.dwPlayerPID);
							s.summonerName = summoner ? summoner->GetName() : "";
						}
					}
				}
				s.summonBlock = GetPlayerBotSummonBlock(bot, state, player, now);
				s.afk = state.persona.dwAfkUntil != 0 && now < state.persona.dwAfkUntil;
				{
					const time_t t = time(0);
					const struct tm* lt = localtime(&t);
					s.hour = lt ? lt->tm_hour : 12;
				}
				return true;
			}

			playerbot_conv::IConvWorld* World(playerbot_conv::u32 playerPID, playerbot_conv::u32 botPID)
			{
				m_world.Bind(CHARACTER_MANAGER::instance().FindByPID(botPID),
						CHARACTER_MANAGER::instance().FindByPID(playerPID));
				return &m_world;
			}

			void Send(playerbot_conv::u32 playerPID, playerbot_conv::u32 botPID, const std::string& text)
			{
				LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(botPID);
				LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(playerPID);
				if (bot && player)
					SendPlayerBotConvWhisper(bot, player, text.c_str());
			}

			void Log(const std::string& line)
			{
				sys_log(0, "%s", line.c_str());
			}

		private:
			CPlayerBotConvWorld m_world;
	};

	CPlayerBotConvHost s_PlayerBotConvHost;

	// -------------------------------------------------------------- timer

	void PumpPlayerBotConversation(DWORD dwNow)
	{
		RefreshPlayerBotConvSwitches(dwNow);
		s_PlayerBotConvEngine.Pump(s_PlayerBotConvHost, dwNow);
		PrunePlayerBotSummons(dwNow);
		if (s_dwPlayerBotConvStatsTime == 0)
			s_dwPlayerBotConvStatsTime = dwNow;
		else if (dwNow - s_dwPlayerBotConvStatsTime >= PLAYERBOT_CONV_STATS_INTERVAL_MS)
		{
			s_dwPlayerBotConvStatsTime = dwNow;
			playerbot_conv::u32 lines = 0, replies = 0, merged = 0, dropped = 0;
			s_PlayerBotConvEngine.Stats(lines, replies, merged, dropped);
			if (lines != s_uPlayerBotConvStatsLines)
			{
				s_uPlayerBotConvStatsLines = lines;
				sys_log(0, "PLAYERBOT_CONV_STATS: lines=%u replies=%u merged=%u dropped=%u pairs=%u pending=%u",
						lines, replies, merged, dropped, (unsigned int)s_PlayerBotConvEngine.PairCount(),
						(unsigned int)s_PlayerBotConvEngine.PendingCount());
			}
		}
	}

	EVENTINFO(playerbot_conv_event_info)
	{
		int unused;
		playerbot_conv_event_info() : unused(0) {}
	};

	// Runs only while a reply is waiting: ~every 80-100 ms, so a reply due at
	// 1000 ms goes out at 1000-1100 ms, not at the next quarter-second tick.
	// Ends itself when the queue is empty; the next whisper starts it again.
	EVENTFUNC(playerbot_conv_event)
	{
		PumpPlayerBotConversation(get_dword_time());
		if (!s_PlayerBotConvEngine.HasPending())
		{
			s_pkPlayerBotConvEvent = NULL;
			return 0;
		}
		const long step = PASSES_PER_SEC(1) / 10;
		return step > 0 ? step : 1;
	}

	void EnsurePlayerBotConvTimer()
	{
		if (s_pkPlayerBotConvEvent)
			return;
		playerbot_conv_event_info* info = AllocEventInfo<playerbot_conv_event_info>();
		const long step = PASSES_PER_SEC(1) / 10;
		s_pkPlayerBotConvEvent = event_create(playerbot_conv_event, info, step > 0 ? step : 1);
	}

	// ---------------------------------------------------------------- hook

	// A whisper from a person to a bot. Always true when the bot is ours: the
	// line is analysed at once and answered from the queue, never dropped.
	bool HandlePlayerBotConversation(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		if (!player || !bot || !text || !*text)
			return false;
		if (s_mapPlayerBotAIStates.find(bot->GetPlayerID()) == s_mapPlayerBotAIStates.end())
			return false;
		const DWORD now = get_dword_time();
		RefreshPlayerBotConvSwitches(now);
		s_PlayerBotConvEngine.OnPlayerLine(s_PlayerBotConvHost, player->GetPlayerID(), bot->GetPlayerID(),
				text, now, player->GetName(), bot->GetName());
		EnsurePlayerBotConvTimer();
		return true;
	}
}

#endif
