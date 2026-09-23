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
//
// The conversation itself - normalization, intents, context, memory,
// persona, mood, relationship, general conversation, merging, the queue - is
// the pure layer (playerbot_conv_*.h), unit tested without the engine:
// tests/playerbot_conversation_test.cpp.
//
// The layer READS the AI (TPlayerBotAIState, the persona and mood, the friend
// ledger, the party, the guild, the counter, the bag). It never writes to it.
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
		if (item->GetRefineLevel() > 0 && (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR))
		{
			char plus[8];
			snprintf(plus, sizeof(plus), " +%d", item->GetRefineLevel());
			name += plus;
		}
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
						outName = PlayerBotConvItemName(item) + " (mam na sobie)";
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

		private:
			LPCHARACTER m_bot;
			LPCHARACTER m_player;
	};

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
