#ifndef __INC_METIN2_PLAYERBOT_STATUS_H__
#define __INC_METIN2_PLAYERBOT_STATUS_H__

// What a bot shows above its head, and the words for it.
//
// This is the only place a bot is described in a player's language rather than
// in the code's. Strings here are Polish and ASCII-only: the client renders
// them in a font that has no diacritics, so an accented character comes out as
// a box.
//
// The text is recomputed only when something it depends on actually changed -
// several hundred bots re-broadcasting an identical line every tick is a lot of
// packets for no new information.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	// Defined with the luring course (playerbot_lure.h), which is included
	// below this file: what a bot holding a person's order is waiting for
	// before it sets off. A status line that says only "Czekam" is exactly
	// what a person cannot report.
	const char* GetPlayerBotLureWaitReason(DWORD dwPID);

	// Whether a real player is close enough for any of this to be seen. The
	// overhead text exists for them, so with nobody watching there is nothing
	// to broadcast.
	class CCheckNearbyHumanPlayer
	{
		public:
			CCheckNearbyHumanPlayer(LPCHARACTER owner, int maxDist) : m_owner(owner), m_maxDist(maxDist), m_bFound(false) {}
			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER ch = static_cast<LPCHARACTER>(entity);
				if (ch && ch->IsPC() && ch->GetDesc() != NULL && ch != m_owner)
				{
					if (DISTANCE_APPROX(m_owner->GetX() - ch->GetX(), m_owner->GetY() - ch->GetY()) <= m_maxDist)
					{
						m_bFound = true;
						return false; // Stop search
					}
				}
				return true;
			}
			LPCHARACTER m_owner;
			int m_maxDist;
			bool m_bFound;
	};

	// The line over a bot's head in two languages. The Polish one goes to the
	// panel's status file and to every client; the English one travels beside
	// it in the same command, and a client set to any language but Polish draws
	// that one instead (playerbot_status_tail.py) - English being what a
	// Romanian or a German reads before Polish ("Chat language", JFK, 23
	// September). A monster's or an item's name in the English line is a
	// "{m<vnum>}" or "{i<vnum>}" the client fills in from its own tables, in its
	// own language; a player's and a guild's name stay what they are. The two
	// formats of a pair must carry the same conversions: the English one is a
	// runtime string, so the compiler checks neither against the arguments.
	inline const char* PBT(bool en, const char* pl, const char* english)
	{
		return en ? english : pl;
	}

	// A monster's or a stone's name for the line: the server's own, or the
	// placeholder the client fills in.
	const char* PlayerBotStatusMobName(LPCHARACTER target, bool en, char* buf, size_t size)
	{
		if (!target)
			return "?";
		if (!en || target->IsPC())
			return target->GetName();
		snprintf(buf, size, "{m%u}", (unsigned int)target->GetRaceNum());
		return buf;
	}

	const char* GetPlayerBotGoalLabel(BYTE goal, bool en = false)
	{
		switch (goal)
		{
			case BOT_GOAL_SURVIVE: return PBT(en, "regeneracja", "recovery");
			case BOT_GOAL_CHOOSE_PROFESSION: return PBT(en, "profesja", "profession");
			case BOT_GOAL_GET_EQUIPMENT: return PBT(en, "ekwipunek", "equipment");
			case BOT_GOAL_RESTOCK: return PBT(en, "zapasy", "supplies");
			case BOT_GOAL_REFINE: return PBT(en, "ulepszanie", "upgrading");
			case BOT_GOAL_MASTER_SKILL: return PBT(en, "rozwoj skilla", "skill training");
			case BOT_GOAL_HUNT_METIN: return PBT(en, "Metiny", "Metin stones");
			case BOT_GOAL_PARTY_CHALLENGE: return PBT(en, "silne moby PT", "strong monsters in a party");
			case BOT_GOAL_BIOLOGIST: return PBT(en, "Biolog", "Biologist");
			case BOT_GOAL_HUNTING: return PBT(en, "Polowanie", "Hunting");
			case BOT_GOAL_HORSE: return PBT(en, "rozwoj konia", "horse training");
			case BOT_GOAL_FISHING: return PBT(en, "lowienie ryb", "fishing");
			default: return PBT(en, "poziom", "levelling");
		}
	}

	// The map a player would name, not the one the code names. Used by the
	// travel line, which used to announce every journey as a hunt for
	// experience whatever the errand actually was.
	// Polish declines the destination, so the table carries the phrase that
	// follows "ide" rather than the bare name: "Ide na Dolina Orkow" is not a
	// sentence anybody would write.
	const char* GetPlayerBotMapDestinationPl(long mapIndex)
	{
		switch (mapIndex)
		{
			// The names are the engine's own: new_quest_lv52 reads the first
			// villages out of { "Yongan", "Joan", "Pyongmoo" } by empire and
			// new_quest_lv7 names the second ones Jayang, Bokjung and Bakra.
			// Map 24 used to be labelled Pyungmoo here, which is Jinno's
			// capital and not Chunjo's guild ground.
			case 1: return "do Yongan";
			case 3: return "do Jayang";
			case PLAYERBOT_MAP_CHUNJO_M1: return "do Joan";
			case PLAYERBOT_MAP_CHUNJO_M2: return "do Bokjung";
			case 41: return "do Pyongmoo";
			case 43: return "do Bakra";
			case 4:
			case PLAYERBOT_MAP_CHUNJO_M3:
			case 44: return "na Ziemie Klanu";
			case 5:
			case 45:
			case PLAYERBOT_MAP_MONKEY_EASY: return "do Lochu Malp";
			case PLAYERBOT_MAP_MONKEY_MEDIUM: return "do Lochu Malp II";
			case PLAYERBOT_MAP_MONKEY_HARD: return "do Lochu Malp III";
			case PLAYERBOT_MAP_DESERT: return "na Pustynie Yongbi";
			case PLAYERBOT_MAP_ORC_VALLEY: return "do Doliny Orkow";
			case PLAYERBOT_MAP_SOHAN: return "na Gore Sohan";
			case PLAYERBOT_MAP_SPIDER_V1: return "do Lochu Pajakow";
			case PLAYERBOT_MAP_SPIDER_V2: return "do Lochu Pajakow 2";
			case PLAYERBOT_MAP_HWANG: return "do Swiatyni Hwang";
			case PLAYERBOT_MAP_FOREST: return "do Lasu";
			case PLAYERBOT_MAP_RED_FOREST: return "do Czerwonego Lasu";
			case PLAYERBOT_MAP_DEMON_TOWER: return "do Wiezy Demonow";
			case PLAYERBOT_MAP_FIRE_LAND: return "do Doyyumhwaji";
			default: return "";
		}
	}

	// The same destinations for the English line.
	const char* GetPlayerBotMapDestinationEn(long mapIndex)
	{
		switch (mapIndex)
		{
			case 1: return "to Yongan";
			case 3: return "to Jayang";
			case PLAYERBOT_MAP_CHUNJO_M1: return "to Joan";
			case PLAYERBOT_MAP_CHUNJO_M2: return "to Bokjung";
			case 41: return "to Pyongmoo";
			case 43: return "to Bakra";
			case 4:
			case PLAYERBOT_MAP_CHUNJO_M3:
			case 44: return "to the Guild Land";
			case 5:
			case 45:
			case PLAYERBOT_MAP_MONKEY_EASY: return "to the Monkey Dungeon";
			case PLAYERBOT_MAP_MONKEY_MEDIUM: return "to the Monkey Dungeon II";
			case PLAYERBOT_MAP_MONKEY_HARD: return "to the Monkey Dungeon III";
			case PLAYERBOT_MAP_DESERT: return "to the Yongbi Desert";
			case PLAYERBOT_MAP_ORC_VALLEY: return "to the Orc Valley";
			case PLAYERBOT_MAP_SOHAN: return "to Mount Sohan";
			case PLAYERBOT_MAP_SPIDER_V1: return "to the Spider Dungeon";
			case PLAYERBOT_MAP_SPIDER_V2: return "to the Spider Dungeon 2";
			case PLAYERBOT_MAP_HWANG: return "to the Hwang Temple";
			case PLAYERBOT_MAP_FOREST: return "to the Ghost Wood";
			case PLAYERBOT_MAP_RED_FOREST: return "to the Red Wood";
			case PLAYERBOT_MAP_DEMON_TOWER: return "to the Demon Tower";
			case PLAYERBOT_MAP_FIRE_LAND: return "to Doyyumhwaji";
			default: return "";
		}
	}

	const char* PlayerBotStatusMapDestination(long mapIndex, bool en)
	{
		return en ? GetPlayerBotMapDestinationEn(mapIndex) : GetPlayerBotMapDestinationPl(mapIndex);
	}

	const char* GetPlayerBotActionLabel(BYTE action)
	{
		switch (action)
		{
			case BOT_ACTION_TRAVEL: return "ide";
			case BOT_ACTION_FIGHT: return "walcze";
			case BOT_ACTION_LOOT: return "zbieram";
			case BOT_ACTION_RECOVER: return "odpoczywam";
			case BOT_ACTION_TRAIN: return "wybieram profesje";
			case BOT_ACTION_SHOP: return "handluje";
			case BOT_ACTION_REFINE: return "ulepszam";
			case BOT_ACTION_READ_BOOK: return "czytam KU";
			case BOT_ACTION_SOCKET_STONE: return "wkladam KD";
			case BOT_ACTION_PARTY_ASSEMBLE: return "zbieram PT";
			case BOT_ACTION_BIOLOGIST: return "robie misje Biologa";
			case BOT_ACTION_STABLE: return "odwiedzam Stajennego";
			case BOT_ACTION_STALL: return "prowadze stragan";
			case BOT_ACTION_MARKET: return "jestem na zakupach";
			case BOT_ACTION_LURE: return "podciagam moby dla PT";
			case BOT_ACTION_TOWN_REST: return "odpoczywam w miescie";
			case BOT_ACTION_MINING: return "kopie rude";
			default: return "mysle";
		}
	}

	// What a bot says aloud: the ordinary chat of the people round it, the
	// packet a player's own line is (CInputMain::Chat), so every client puts
	// it in the chat window and over the bot's head. Not the shout: a line that
	// concerns the people standing there is theirs, not the kingdom's.
	void SendPlayerBotLocalChat(LPCHARACTER ch, const char* szText)
	{
		if (!ch || !szText || !szText[0] || !ch->GetSectree())
			return;
		char chatbuf[256];
		int len = snprintf(chatbuf, sizeof(chatbuf), "%s : %s", ch->GetName(), szText);
		if (len <= 0)
			return;
		if (len >= (int)sizeof(chatbuf))
			len = sizeof(chatbuf) - 1;
		// The regular talking packet contains its trailing NUL.  Keeping the packet
		// identical to a real player's chat is what makes every native/wasm client
		// render it as a text tail above the bot without a client fork.
		++len;

		TPacketGCChat pack_chat;
		pack_chat.header = HEADER_GC_CHAT;
		pack_chat.size = sizeof(TPacketGCChat) + len;
		pack_chat.type = CHAT_TYPE_TALKING;
		pack_chat.id = ch->GetVID();
		pack_chat.bEmpire = 0;

		TEMP_BUFFER buf;
		buf.write(&pack_chat, sizeof(TPacketGCChat));
		buf.write(chatbuf, len);
		ch->PacketAround(buf.read_peek(), buf.size());
	}

	// The line over a bot's head. On the 2.x line it is the server command
	// "PlayerBotStatus <vid> <hex>", which the client root draws as a text tail
	// and nothing else (playerbot_status_tail.py): the client puts every TALKING
	// packet from a character into the chat history beside its tail
	// (RecvChatPacket), so a town of bots filled the chat window with statuses.
	// The text goes as hex because the client's command parser splits its line
	// on spaces; the bytes are the status's CP1250, and the name stays out of it,
	// because the client draws the name over the head already. A root without
	// the handler writes "Unknown Server Command" to its syserr.txt and draws
	// nothing. The r40250 client has no handler, so that line keeps talking.
	// The status as hex, the way the client's command parser can carry it.
	void EncodePlayerBotStatusHex(const char* szText, char* hex)
	{
		static const char kHexDigits[] = "0123456789abcdef";
		size_t n = 0;
		for (; szText && n < PLAYERBOT_STATUS_TAIL_MAX_BYTES && szText[n]; ++n)
		{
			unsigned char c = (unsigned char)szText[n];
			// The client refuses a control byte; a space keeps the rest of the line.
			if (c < 32 || c == 127)
				c = ' ';
			hex[n * 2] = kHexDigits[c >> 4];
			hex[n * 2 + 1] = kHexDigits[c & 15];
		}
		hex[n * 2] = '\0';
	}

	// szTextEn, when there is one, is the English line: a third word of the
	// command, which a client root from before it ignores (the handler takes
	// *rest) and a newer one draws when its language is not Polish. Two lines
	// of PLAYERBOT_STATUS_TAIL_MAX_BYTES as hex stay far under the 1024 bytes
	// the client reads a chat packet into.
	void SendPlayerBotOverheadChat(LPCHARACTER ch, const char* szText, const char* szTextEn = NULL)
	{
		if (!ch || !szText || !szText[0] || !ch->GetSectree())
			return;

#if defined(PLAYERBOT_ENGINE_MT2009)
		char hex[PLAYERBOT_STATUS_TAIL_MAX_BYTES * 2 + 1];
		EncodePlayerBotStatusHex(szText, hex);
		char hexEn[PLAYERBOT_STATUS_TAIL_MAX_BYTES * 2 + 1];
		hexEn[0] = '\0';
		if (szTextEn && szTextEn[0])
			EncodePlayerBotStatusHex(szTextEn, hexEn);

		char command[sizeof(hex) + sizeof(hexEn) + 40];
		int commandLen = hexEn[0]
				? snprintf(command, sizeof(command), "PlayerBotStatus %u %s %s",
						(unsigned int)ch->GetVID(), hex, hexEn)
				: snprintf(command, sizeof(command), "PlayerBotStatus %u %s",
						(unsigned int)ch->GetVID(), hex);
		if (commandLen <= 0 || commandLen >= (int)sizeof(command))
			return;
		++commandLen;   // the trailing NUL every chat packet carries

		TPacketGCChat pack_command;
		pack_command.header = HEADER_GC_CHAT;
		pack_command.size = sizeof(TPacketGCChat) + commandLen;
		pack_command.type = CHAT_TYPE_COMMAND;
		pack_command.id = 0;   // the bot's VID travels in the command
		pack_command.bEmpire = 0;

		TEMP_BUFFER commandBuf;
		commandBuf.write(&pack_command, sizeof(TPacketGCChat));
		commandBuf.write(command, commandLen);
		ch->PacketAround(commandBuf.read_peek(), commandBuf.size());
		return;
#endif

		SendPlayerBotLocalChat(ch, szText);
	}

	const char* GetPlayerBotTownStatusLabel(const TPlayerBotAIState& state, bool en = false)
	{
		switch (state.bTownVisitPhase)
		{
			case BOT_TOWN_PHASE_TRAINER: return PBT(en, "Ide po profesje", "Going for a profession");
			case BOT_TOWN_PHASE_TRAINER_WAIT: return PBT(en, "Wybieram profesje", "Choosing a profession");
			case BOT_TOWN_PHASE_WEAPON_MERCHANT: return PBT(en, "Ide do handlarza bronia", "Going to the weapon dealer");
			case BOT_TOWN_PHASE_WEAPON_WAIT: return PBT(en, "Handluje bronia", "Trading with the weapon dealer");
			case BOT_TOWN_PHASE_ARMOR_MERCHANT: return PBT(en, "Ide do handlarza zbroja", "Going to the armour dealer");
			case BOT_TOWN_PHASE_ARMOR_WAIT: return PBT(en, "Handluje zbroja", "Trading with the armour dealer");
			case BOT_TOWN_PHASE_MISC_MERCHANT: return PBT(en, "Ide do handlarki roznosci", "Going to the general store");
			case BOT_TOWN_PHASE_MISC_WAIT: return PBT(en, "Kupuje potki i sprzedaje lup", "Buying potions, selling loot");
			case BOT_TOWN_PHASE_BLACKSMITH: return PBT(en, "Ide do kowala", "Going to the blacksmith");
			case BOT_TOWN_PHASE_BLACKSMITH_WAIT: return PBT(en, "Ulepszam ekwipunek", "Upgrading equipment");
			case BOT_TOWN_PHASE_SAFEBOX: return PBT(en, "Ide do magazynu z ksiegami", "Taking books to the storage");
			case BOT_TOWN_PHASE_SAFEBOX_WAIT: return PBT(en, "Oddaje ksiegi do magazynu", "Putting books into storage");
			case BOT_TOWN_PHASE_GATE_IN:
			case BOT_TOWN_PHASE_GATE_CROSS_IN: return PBT(en, "Ide do miasta", "Going into town");
			case BOT_TOWN_PHASE_GATE_OUT:
			case BOT_TOWN_PHASE_GATE_CROSS_OUT: return PBT(en, "Wracam na exp", "Heading back to hunt");
			default: return PBT(en, "Zalatwiam sprawy w miescie", "Running errands in town");
		}
	}

	// Either side of a mercenary's contract, and the walk to offer one
	// (playerbot_companions.h).
	bool BuildPlayerBotMercStatus(LPCHARACTER ch, const TPlayerBotAIState& state, const char* prefix,
			char* status, size_t statusSize, bool en = false);
	// A bot a person called over: on its way, or standing with them
	// (playerbot_chat_conversation.h, which comes after this file).
	inline bool BuildPlayerBotSummonStatus(LPCHARACTER ch, const TPlayerBotAIState& state, const char* prefix,
			char* status, size_t statusSize, bool en);

	void BuildPlayerBotStatusText(LPCHARACTER ch, const TPlayerBotAIState& state,
			char* status, size_t statusSize, bool en = false)
	{
		if (!ch || !status || statusSize == 0)
			return;
		char mobName[16];
		char itemName[16];

		const char* prefix = ch->GetParty() ? "[PT] " : "";
		const char* goal = GetPlayerBotGoalLabel(state.bLongTermGoal, en);
		// The Demon Tower: the floor a bot is on, or the raid it is going to
		// (playerbot_demon_tower.h).
		if (IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
		{
			LPDUNGEON dungeon = ch->GetDungeon();
			snprintf(status, statusSize, PBT(en, "%sWieza Demonow: pietro %d", "%sDemon Tower: floor %d"), prefix,
					dungeon ? GetPlayerBotDungeonLevel(dungeon) + 2 : 0);
			return;
		}
		if (state.dwTowerRaidGuild != 0 || state.bTowerSummoned)
		{
			snprintf(status, statusSize, PBT(en, "%sZbiorka gildii: Wieza Demonow", "%sGuild gathering: Demon Tower"), prefix);
			return;
		}
		// A boss raid (playerbot_boss_raid.h), named by the boss.
		if (state.wBossRaidRace != 0)
		{
			const CMob* boss = CMobManager::instance().Get(state.wBossRaidRace);
			snprintf(mobName, sizeof(mobName), "{m%u}", (unsigned int)state.wBossRaidRace);
			snprintf(status, statusSize, PBT(en, "%sRajd na bossa: %s", "%sBoss raid: %s"), prefix,
					en ? mobName : (boss ? boss->m_table.szLocaleName : "boss"));
			return;
		}
		// A guild war outranks every errand while it lasts (playerbot_guild_war.h).
		if (state.dwGuildWarEnemyGID != 0)
		{
			CGuild* enemy = CGuildManager::instance().FindGuild(state.dwGuildWarEnemyGID);
			// The war's first seconds are the muster at the camp
			// (PLAYERBOT_GUILD_WAR_MUSTER_SECONDS, playerbot_guild_war.h).
			CGuild* mine = ch ? ch->GetGuild() : NULL;
			const DWORD startedAt = (mine && enemy) ? mine->GetWarStartTime(enemy->GetID()) : 0;
			if (startedAt != 0 && (DWORD)get_global_time() < startedAt + PLAYERBOT_GUILD_WAR_MUSTER_SECONDS)
				snprintf(status, statusSize, PBT(en, "%sZbiorka przed wojna gildii z %s", "%sMustering for the guild war with %s"),
						prefix, enemy ? enemy->GetName() : "?");
			else
				snprintf(status, statusSize, PBT(en, "%sWojna gildii z %s", "%sGuild war with %s"), prefix, enemy ? enemy->GetName() : "?");
			return;
		}
		// A player's companion at its owner's side says whose it is
		// (playerbot_sidekick.h), and what it is doing for the owner.
		if (const char* owner = GetPlayerBotSidekickOwnerName(ch))
		{
			if (state.bCurrentAction == BOT_ACTION_FIGHT)
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - walcze", "%s's companion - fighting"), owner);
			else if (state.bCurrentAction == BOT_ACTION_LOOT)
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - zbieram drop", "%s's companion - picking up"), owner);
			else if (state.bCurrentAction == BOT_ACTION_REFINE)
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - u kowala", "%s's companion - at the blacksmith"), owner);
			else if (state.bCurrentAction == BOT_ACTION_SHOP)
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - u handlarza", "%s's companion - at the merchant"), owner);
			else if (state.bRecoveringAfterDeath)
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - wracam do sil", "%s's companion - recovering"), owner);
			else if (IsPlayerBotSidekickHolding(ch))
				snprintf(status, statusSize, PBT(en, "Towarzysz %s - czekam", "%s's companion - waiting"), owner);
			else
				snprintf(status, statusSize, PBT(en, "Towarzysz %s", "%s's companion"), owner);
			return;
		}
		if (state.bVisitingShop)
		{
			// "Handluje bronia (cel: zapasy)" says what the bot is standing at
			// and nothing about what it came for. When the errand is potions,
			// the numbers are the whole story - and they are the one thing an
			// operator can check against the shelf.
			if (state.bLongTermGoal == BOT_GOAL_RESTOCK)
			{
				size_t redCount = 0, blueCount = 0;
				CountPlayerBotPotions(ch, redCount, blueCount);
				snprintf(status, statusSize, PBT(en, "%s%s - potki %u/%u", "%s%s - potions %u/%u"), prefix,
						GetPlayerBotTownStatusLabel(state, en),
						(unsigned int)redCount, (unsigned int)blueCount);
				return;
			}
			snprintf(status, statusSize, PBT(en, "%s%s (cel: %s)", "%s%s (goal: %s)"), prefix,
					GetPlayerBotTownStatusLabel(state, en), goal);
			return;
		}

		if (state.bTacticalRetreat)
		{
			snprintf(status, statusSize, PBT(en, "%sUciekam - mam malo HP", "%sRetreating - low HP"), prefix);
			return;
		}
		// An errand the watchdog interrupted, and the map the bot still means to
		// leave for. The audit asked for exactly this pair - "Uzupelniam
		// mikstury; potem Sohan" - because an observer cannot otherwise tell a
		// bot that is stuck from one that is waiting.
		// A keeper carrying its goods to the other town because this ring
		// is full: the walk, not the goal, is what a player sees.
		if (state.bServicePending)
		{
			const char* where = state.lDepartureMap != 0
					? PlayerBotStatusMapDestination(state.lDepartureMap, en) : "";
			if (where[0])
				snprintf(status, statusSize, PBT(en, "%sCzekam na trase do handlarza; potem %s", "%sWaiting for a route to the merchant; then %s"),
						prefix, where);
			else
				snprintf(status, statusSize, PBT(en, "%sCzekam na trase do handlarza", "%sWaiting for a route to the merchant"), prefix);
			return;
		}
		// The luring course says which stage it is in, because "walking away
		// from the party" and "bringing nine monsters back to it" look the same
		// from outside and are not the same thing at all.
		// A standing order says whose it is: an operator reading the panel wants
		// to know that a bot standing about is waiting to pull for somebody, not
		// that it has run out of things to do.
		char forWhom[CHARACTER_NAME_MAX_LEN + 8];
		forWhom[0] = 0;
		if (state.dwLurePlayerPID != 0)
		{
			LPCHARACTER askedBy =
					CHARACTER_MANAGER::instance().FindByPID(state.dwLurePlayerPID);
			snprintf(forWhom, sizeof(forWhom), PBT(en, " dla %s", " for %s"),
					askedBy ? askedBy->GetName() : PBT(en, "gracza", "a player"));
		}
		if (state.bLureStage != LURE_STAGE_NONE)
		{
			switch (state.bLureStage)
			{
				case LURE_STAGE_PLAN:
					snprintf(status, statusSize, PBT(en, "%sSzykuje lur%s", "%sPreparing a lure%s"), prefix,
							forWhom[0] ? forWhom : PBT(en, " dla druzyny", " for the party"));
					return;
				case LURE_STAGE_RETURN:
					snprintf(status, statusSize, PBT(en, "%sWracam%s: prowadze %d mobow", "%sComing back%s: leading %d monsters"),
							prefix, forWhom[0] ? forWhom : PBT(en, " do druzyny", " to the party"), state.iLureChasing);
					return;
				case LURE_STAGE_HANDOFF:
					snprintf(status, statusSize, PBT(en, "%sPrzekazuje moby%s: %d przyprowadzonych, %d nadal za mna", "%sHanding the pack over%s: %d brought, %d still after me"),
							prefix, forWhom, state.iLureDelivered, state.iLureChasing);
					return;
				case LURE_STAGE_RECOVER:
					snprintf(status, statusSize, PBT(en, "%sWstrzymuje lur: %s jeszcze walczy", "%sLure on hold: %s still fighting"),
							prefix, forWhom[0] ? PBT(en, "gracz", "the player is") : PBT(en, "druzyna", "the party is"));
					return;
				default:
					snprintf(status, statusSize, PBT(en, "%sLuruje%s: %u/%u grupy, sciga mnie %d", "%sLuring%s: %u/%u groups, %d chasing me"),
							prefix, forWhom[0] ? forWhom : PBT(en, " dla PT", " for the party"),
							(unsigned int)state.bLureGroupsTagged,
							(unsigned int)state.bLureGroupsPlanned, state.iLureChasing);
					return;
			}
		}
		if (forWhom[0])
		{
			// And what it is waiting for. A bot reading "Czekam, zeby lurowac
			// dla X" for twenty minutes and never setting off is the whole of
			// what a person sees of this feature going wrong; the word in
			// brackets is what turns that into a report somebody can act on.
			const char* waitFor = GetPlayerBotLureWaitReason(ch ? ch->GetPlayerID() : 0);
			if (waitFor)
				snprintf(status, statusSize, PBT(en, "%sCzekam, zeby lurowac%s (%s)", "%sWaiting to lure%s (%s)"),
						prefix, forWhom, waitFor);
			else
				snprintf(status, statusSize, PBT(en, "%sCzekam, zeby lurowac%s", "%sWaiting to lure%s"), prefix, forWhom);
			return;
		}
		if (state.bRecoveringAfterDeath)
		{
			snprintf(status, statusSize, PBT(en, "%sOdpoczywam po smierci", "%sResting after death"), prefix);
			return;
		}
		// The two habits of a SLABY mood (playerbot_persona.h): a player
		// looking at a bot standing still is told why.
		{
			const DWORD now = get_dword_time();
			if (state.persona.dwAfkUntil != 0 && now < state.persona.dwAfkUntil)
			{
				snprintf(status, statusSize, PBT(en, "%sAFK - zaraz wracam", "%sAFK - back soon"), prefix);
				return;
			}
			if (state.persona.dwPauseUntil != 0 && now < state.persona.dwPauseUntil)
			{
				snprintf(status, statusSize, PBT(en, "%sChwila przerwy", "%sTaking a short break"), prefix);
				return;
			}
		}

		// A contract says whom the bot is with, unless it is fighting: then the
		// fight says what it is fighting.
		if (state.bCurrentAction != BOT_ACTION_FIGHT &&
				BuildPlayerBotMercStatus(ch, state, prefix, status, statusSize, en))
			return;
		// So does a person's call ("Ide do X", "Stoje przy X"), with the same
		// exception for a fight.
		if (BuildPlayerBotSummonStatus(ch, state, prefix, status, statusSize, en))
			return;

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		switch (state.bCurrentAction)
		{
			case BOT_ACTION_FIGHT:
				if (target && target->IsStone())
					snprintf(status, statusSize, PBT(en, "%sRozbijam %s", "%sBreaking %s"), prefix, PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
				else if (target && target->IsMonster())
				{
					int huntingRemaining = 0;
					const DWORD huntingMob = GetActivePlayerBotHuntingMobVnum(
							ch, &huntingRemaining);
					if (huntingMob != 0 && target->GetRaceNum() == huntingMob)
					{
						snprintf(status, statusSize, PBT(en, "%sPolowanie: %s (zostalo %d)", "%sHunt: %s (%d left)"),
								prefix, PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)), huntingRemaining);
						break;
					}
					LPITEM weapon = ch->GetWear(WEAR_WEAPON);
					const bool bow = weapon && weapon->GetType() == ITEM_WEAPON &&
							weapon->GetSubType() == WEAPON_BOW;
					const int range = bow ? 800 : 280;
					const int distance = DISTANCE_APPROX(
							ch->GetX() - target->GetX(), ch->GetY() - target->GetY());
					// Action plus what for. "Walcze z X" is only half of what an
					// observer needs - the audit's complaint was that a status
					// never says why this monster and not another one. The
					// reason is the combat policy's own, recorded when it last
					// looked at this target.
					if (state.bLastCombatReason ==
							(BYTE)playerbot_combat_value::ALLOW_SELF_DEFENSE)
						snprintf(status, statusSize, PBT(en, "%sBronie sie przed %s", "%sDefending myself against %s"), prefix,
								PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
					else if (state.bLastCombatReason ==
							(BYTE)playerbot_combat_value::ALLOW_PARTY_DEFENSE)
						snprintf(status, statusSize, PBT(en, "%sPomagam druzynie: %s", "%sHelping the party: %s"), prefix,
								PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
					else if (state.bLastCombatReason ==
							(BYTE)playerbot_combat_value::ALLOW_MATERIAL)
						snprintf(status, statusSize, PBT(en, "%sZbieram material z %s", "%sGathering materials from %s"), prefix,
								PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
					else if (distance > range)
						snprintf(status, statusSize, PBT(en, "%sGonie %s", "%sChasing %s"), prefix, PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
					else
						snprintf(status, statusSize, PBT(en, "%sWalcze z %s", "%sFighting %s"), prefix, PlayerBotStatusMobName(target, en, mobName, sizeof(mobName)));
				}
				else if (target && target->IsPC())
				{
					// A player: the Anti-PK protocol says why (playerbot_anti_pk.h);
					// otherwise a duel or a war, which this line used to call
					// "looking for an opponent" in the middle of the fight.
					if (state.persona.dwFoeVID == (DWORD)target->GetVID())
						switch (state.persona.bFoeReason)
						{
							case BOT_FOE_STRUCK:
								snprintf(status, statusSize, PBT(en, "%sBronie sie przed %s", "%sDefending myself against %s"), prefix, target->GetName());
								break;
							case BOT_FOE_PARTY:
								snprintf(status, statusSize, PBT(en, "%sBronie druzyny przed %s", "%sDefending the party against %s"), prefix, target->GetName());
								break;
							case BOT_FOE_GRUDGE:
								snprintf(status, statusSize, PBT(en, "%sWracam po rewanz na %s", "%sBack for revenge on %s"), prefix, target->GetName());
								break;
							case BOT_FOE_STONE_RIVAL:
								snprintf(status, statusSize, PBT(en, "%sOdganiam %s od Metina", "%sChasing %s away from the Metin"), prefix, target->GetName());
								break;
							case BOT_FOE_GUILD:
								snprintf(status, statusSize, PBT(en, "%sBronie gildii przed %s", "%sDefending the guild against %s"), prefix, target->GetName());
								break;
							default:
								snprintf(status, statusSize, PBT(en, "%sWalcze z %s", "%sFighting %s"), prefix, target->GetName());
								break;
						}
					else
						snprintf(status, statusSize, PBT(en, "%sWalcze z %s", "%sFighting %s"), prefix, target->GetName());
				}
				else
					snprintf(status, statusSize, PBT(en, "%sSzukam przeciwnika", "%sLooking for a foe"), prefix);
				break;
			case BOT_ACTION_LOOT:
				snprintf(status, statusSize, PBT(en, "%sPodnosze lup", "%sPicking up loot"), prefix);
				break;
			case BOT_ACTION_RECOVER:
				snprintf(status, statusSize, PBT(en, "%sRegeneruje HP", "%sRecovering HP"), prefix);
				break;
			case BOT_ACTION_TRAIN:
				if (state.bVisitingShop &&
						(state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET ||
						 state.bTownVisitPhase == BOT_TOWN_PHASE_SKILL_RESET_WAIT))
					snprintf(status, statusSize, PBT(en, "%sResetuje umiejetnosci u staruszki", "%sResetting skills at the Old Lady"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sWybieram profesje", "%sChoosing a profession"), prefix);
				break;
			case BOT_ACTION_SHOP:
				// The Alchemist's exchange (ManagePlayerBotAlchemist) walks under
				// this action too, and "trading" over a bot crossing the village
				// to an NPC says nothing.
				if (state.bVisitingAlchemist)
					snprintf(status, statusSize, PBT(en, "%sNiose Alchemikowi kamienie duszy na pyl",
							"%sTaking soul stones to the Alchemist for dust"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sHandluje", "%sTrading"), prefix);
				break;
			case BOT_ACTION_REFINE:
				snprintf(status, statusSize, PBT(en, "%sUlepszam ekwipunek", "%sUpgrading equipment"), prefix);
				break;
			case BOT_ACTION_READ_BOOK:
				snprintf(status, statusSize, PBT(en, "%sCzytam ksiege umiejetnosci", "%sReading a skill book"), prefix);
				break;
			case BOT_ACTION_SOCKET_STONE:
				snprintf(status, statusSize, PBT(en, "%sWkladam kamien duszy", "%sSetting a spirit stone"), prefix);
				break;
			case BOT_ACTION_PARTY_ASSEMBLE:
				snprintf(status, statusSize, PBT(en, "%sSzukam celu dla grupy", "%sLooking for a target for the group"), prefix);
				break;
			case BOT_ACTION_BIOLOGIST:
			{
				const TPlayerBotBiologistMission* mission =
						GetActivePlayerBotBiologistMission(ch);
				if (!mission)
					snprintf(status, statusSize, PBT(en, "%sWracam od Biologa", "%sComing back from the Biologist"), prefix);
				else if (state.bVisitingBiologist &&
						DISTANCE_APPROX(ch->GetX() - PLAYERBOT_BIOLOGIST_X,
								ch->GetY() - PLAYERBOT_BIOLOGIST_Y) > 850)
					snprintf(status, statusSize, PBT(en, "%sIde do Biologa z: %s", "%sTaking to the Biologist: %s"), prefix, (en ? (snprintf(itemName, sizeof(itemName), "{i%u}", (unsigned int)mission->itemVnum), itemName) : mission->itemLabel));
				else if (state.bVisitingBiologist)
					snprintf(status, statusSize, PBT(en, "%sOddaje Biologowi: %s", "%sHanding in to the Biologist: %s"), prefix, (en ? (snprintf(itemName, sizeof(itemName), "{i%u}", (unsigned int)mission->itemVnum), itemName) : mission->itemLabel));
				else
					snprintf(status, statusSize, PBT(en, "%sZbieram dla Biologa: %s", "%sCollecting for the Biologist: %s"), prefix, (en ? (snprintf(itemName, sizeof(itemName), "{i%u}", (unsigned int)mission->itemVnum), itemName) : mission->itemLabel));
				break;
			}
			case BOT_ACTION_STABLE:
			{
				// The stable keeper of the map the bot is on: measured against
				// Joan's alone, a bot handing its medal over in Bokjung was
				// "on its way" for the whole visit.
				playerbot_empire_rules::TTownServices svc;
				const bool haveStable = playerbot_empire_rules::GetTownServices(ch->GetMapIndex(), svc);
				const long stableX = haveStable ? svc.stableKeeper.x : ch->GetX();
				const long stableY = haveStable ? svc.stableKeeper.y : ch->GetY();
				const bool bFar = DISTANCE_APPROX(ch->GetX() - stableX, ch->GetY() - stableY) > 850;
				if (IsPlayerBotBattleHorseEarned(ch))
					snprintf(status, statusSize, bFar ? PBT(en, "%sIde do Stajennego po konia bojowego", "%sGoing to the Stable Boy for a battle horse")
							: PBT(en, "%sOdbieram konia bojowego u Stajennego", "%sCollecting a battle horse from the Stable Boy"), prefix);
				else if (bFar)
					snprintf(status, statusSize, PBT(en, "%sIde do Stajennego z medalem", "%sTaking a medal to the Stable Boy"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sOddaje medal konny (%u/21)", "%sHanding in a horse medal (%u/21)"), prefix,
							(unsigned int)ch->GetHorseLevel());
				break;
			}
			case BOT_ACTION_FISHING:
				if (ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
						PLAYERBOT_FISHING_BAIT_RESTOCK)
					snprintf(status, statusSize, PBT(en, "%sIde do Rybaka po przynete", "%sGoing to the Fisherman for bait"), prefix);
				else if (GetPlayerBotFishingBank(ch->GetMapIndex()) == NULL ||
						DISTANCE_APPROX(
							ch->GetX() - GetPlayerBotFishingBank(ch->GetMapIndex())->centre.x,
							ch->GetY() - GetPlayerBotFishingBank(ch->GetMapIndex())->centre.y) >
						GetPlayerBotFishingBank(ch->GetMapIndex())->radius)
					snprintf(status, statusSize, PBT(en, "%sIde nad rzeke lowic ryby", "%sGoing to the river to fish"), prefix);
				else if (state.bIsFishing)
					snprintf(status, statusSize, PBT(en, "%sLowie ryby - czekam na branie", "%sFishing - waiting for a bite"), prefix);
				else if (!IsPlayerBotHoldingRod(ch))
					// The old text here was a plain else, so an angler standing at
					// the water with no rod on its back announced that it was
					// baiting one - which is what got reported as "bots put bait
					// on weapons". Nothing was ever put on a weapon; the label
					// was simply wrong about what the bot was doing.
					snprintf(status, statusSize, PBT(en, "%sSzukam wedki", "%sLooking for a fishing rod"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sZakladam przynete na wedke", "%sBaiting the rod"), prefix);
				break;
			case BOT_ACTION_MINING:
				// Walking to a vein and digging at one are different things to
				// watch, and "Kopie rude" over a bot crossing the valley is the
				// shape of mistake the Monkey Dungeon exit line already made.
				if (ch->GetWear(WEAR_WEAPON) &&
						ch->GetWear(WEAR_WEAPON)->GetType() == ITEM_PICK)
					snprintf(status, statusSize, PBT(en, "%sKopie rude", "%sMining ore"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sIde do zyly rudy", "%sGoing to an ore vein"), prefix);
				break;
			case BOT_ACTION_TOWN_REST:
				// The linger after a town errand. It reads as browsing only
				// where there are counters to browse; on a world too young
				// for a single stall it was "what stalls, there are none".
				if (GetPlayerBotStallsOnMap(ch->GetMapIndex()) > 0)
					snprintf(status, statusSize, PBT(en, "%sOgladam stragany", "%sBrowsing the stalls"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sOdpoczywam w miescie", "%sResting in town"), prefix);
				break;
			case BOT_ACTION_MARKET:
				if (state.dwMarketStallVID != 0)
					snprintf(status, statusSize, PBT(en, "%sOgladam stragan", "%sLooking at a stall"), prefix);
				else
					snprintf(status, statusSize, PBT(en, "%sSzukam czegos na straganach", "%sLooking for something at the stalls"), prefix);
				break;
			case BOT_ACTION_TRAVEL:
				if (IsPlayerBotM1Map(ch->GetMapIndex()) &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, PBT(en, "%sIde przez portal do M2 po Medal Konny", "%sGoing through the portal to M2 for a Horse Medal"), prefix);
				else if (IsPlayerBotM2Map(ch->GetMapIndex()) &&
						ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) == 0 &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, PBT(en, "%sIde do Lochu Malp po Medal Konny", "%sGoing to the Monkey Dungeon for a Horse Medal"), prefix);
				else if (IsPlayerBotOnBattleHorseTrial(ch))
					snprintf(status, statusSize, PBT(en, "%sZdobywam konia bojowego na pustyni (%d/%d)", "%sEarning a battle horse in the desert (%d/%d)"), prefix,
							GetPlayerBotBattleHorseKills(ch), PLAYERBOT_BATTLE_HORSE_KILLS);
				// M3 is the level-30 weapon's farm, whatever the planner's goal:
				// a bot walking between its hubs read "Zbieram dla Biologa: Zab
				// Orka" there, and the Orc Tooth is not on the guild map
				// (Champion of urtopy's world, 21 September).
				else if (IsPlayerBotM3Map(ch->GetMapIndex()) &&
						(IsPlayerBotM3DropperOnFarm(ch) || !HasPlayerBotSpecialLevel30Weapon(ch, true)))
					snprintf(status, statusSize, PBT(en, "%sSzukam broni na 30 poziom na M3", "%sLooking for a level 30 weapon on M3"), prefix);
				// The second tier's Grinder, with the weapon already in hand.
				else if (IsPlayerBotM3Map(ch->GetMapIndex()) && IsPlayerBotM3TierGrinder(ch))
					snprintf(status, statusSize, PBT(en, "%sExpie na M3 (Tier 2)", "%sLevelling on M3 (Tier 2)"), prefix);
				// Only a medal the bot can hand in. A horse at ten waits for
				// level thirty-five, a medal dropper carries them for its
				// counter, and both used to announce the stable keeper on every
				// leg they rode - "idzie do stajennego przez godzine".
				else if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0 &&
						CanPlayerBotAdvanceHorse(ch))
					snprintf(status, statusSize, PBT(en, "%sIde do najblizszego Stajennego z Medalem", "%sTaking a Medal to the nearest Stable Boy"), prefix);
				else if (IsPlayerBotMonkeyMap(ch->GetMapIndex()))
				{
					// Only when the bot has actually decided to go. This was a
					// plain else on a monkey map, so every bot moving inside the
					// dungeon announced that it was leaving - and moving is what
					// a bot in here does all the time: the maze is eleven
					// chambers joined only by GOTO NPCs, and crossing to the
					// next one is a walk like any other. Reported from the
					// Discord as "the bubble says they are leaving and they do
					// not leave". They were not leaving. Leaving is instant when
					// it happens at all - the exit is a direct map change, not a
					// walk - so a bot that is still here is doing something else.
					// Never "leaving" while the bot is still here, because
					// leaving is not something that takes time: the exit is a
					// direct map change made on the tick the decision is taken,
					// so a bot anybody can still see in the dungeon is by
					// definition not on its way out. Gating on the goal was not
					// enough - BOT_GOAL_HORSE is what a medal expedition carries
					// for its whole visit, so twenty-one of thirty bots still
					// announced an exit they were nowhere near. Say the true
					// thing instead: it is crossing the maze.
					snprintf(status, statusSize, PBT(en, "%sSzukam drogi przez Loch Malp", "%sFinding my way through the Monkey Dungeon"), prefix);
				}
				// "Szukam miejsca do expa (cel: zapasy)" was said over a bot
				// walking to a merchant, which is the audit's example of a
				// status that describes an action without its purpose. Say
				// where the bot is going, and when the errand is not experience,
				// say the errand instead.
				else if (state.bLongTermGoal == BOT_GOAL_RESTOCK)
					snprintf(status, statusSize, PBT(en, "%sIde do miasta po zapasy", "%sGoing to town for supplies"), prefix);
				else if (state.bLongTermGoal == BOT_GOAL_REFINE)
					snprintf(status, statusSize, PBT(en, "%sIde do kowala ulepszyc ekwipunek", "%sGoing to the blacksmith to upgrade"), prefix);
				else if (state.bLongTermGoal == BOT_GOAL_BIOLOGIST)
				{
					// The Biologist only for a bot carrying the hand-in or visiting
					// him: 27 of 68 bots in Orc Valley read "Ide do Biologa" with no
					// tooth in the bag, hunting the row's monsters (m2zip, 17
					// September) - the Biologist stands in the first village.
					size_t missionIndex = 0;
					const TPlayerBotBiologistMission* mission =
							GetActivePlayerBotBiologistMission(ch, &missionIndex);
					if (mission && !state.bVisitingBiologist &&
							!PlayerBotBiologistHoldsHandIn(ch, mission, missionIndex))
						snprintf(status, statusSize, PBT(en, "%sZbieram dla Biologa: %s", "%sCollecting for the Biologist: %s"), prefix, (en ? (snprintf(itemName, sizeof(itemName), "{i%u}", (unsigned int)mission->itemVnum), itemName) : mission->itemLabel));
					else
						snprintf(status, statusSize, PBT(en, "%sIde do Biologa", "%sGoing to the Biologist"), prefix);
				}
				else if (state.bLongTermGoal == BOT_GOAL_FISHING)
					snprintf(status, statusSize, PBT(en, "%sIde nad rzeke lowic ryby", "%sGoing to the river to fish"), prefix);
				else if (state.bLongTermGoal == BOT_GOAL_GET_EQUIPMENT)
					snprintf(status, statusSize, PBT(en, "%sIde do miasta po ekwipunek", "%sGoing to town for equipment"), prefix);
				else
				{
					// The frontier only for a bot the travel would actually
					// send there: a medal dropper never leaves for it
					// (ShouldPlayerBotLeaveForFrontier), and eleven of them at
					// thirty-three read "Ide na Pustynie Yongbi (cel: rozwoj
					// konia)" in Bokjung while riding to the Monkey Dungeon
					// (16 September).
					const long wantMap = ShouldPlayerBotLeaveForFrontier(ch)
							? GetPlayerBotFrontierMapForLevel(ch) : 0;
					const char* where = wantMap != 0 && wantMap != ch->GetMapIndex()
							? PlayerBotStatusMapDestination(wantMap, en) : "";
					// The frontier is reached from Bokjung through the
					// Teleporter, at his price; a bot that cannot pay is not
					// going anywhere, and "Ide na Gore Sohan" over a bot that
					// has stood in Bokjung for an hour is what an operator
					// reads as a bot that cannot find the portal.
					if (where[0] && IsPlayerBotM2Map(ch->GetMapIndex()) &&
							ch->GetGold() < GetPlayerBotTeleporterFee(ch))
						snprintf(status, statusSize, PBT(en, "%sZbieram yang na Teleporter %s (%lld/%d)", "%sSaving yang for the Teleporter %s (%lld/%d)"),
								prefix, where, (long long)ch->GetGold(), GetPlayerBotTeleporterFee(ch));
					else if (where[0])
						snprintf(status, statusSize, PBT(en, "%sIde %s (cel: %s)", "%sGoing %s (goal: %s)"), prefix,
								where, goal);
					else
						snprintf(status, statusSize, PBT(en, "%sSzukam lepszego miejsca (cel: %s)", "%sLooking for a better spot (goal: %s)"),
								prefix, goal);
				}
				break;
			case BOT_ACTION_STALL:
				// The head carries the sign in the world; the panel read
				// "Planuje: poziom" for a keeper at its counter and an operator
				// counted thirty-nine idle bots in the Joan square.
				snprintf(status, statusSize, PBT(en, "%sProwadze stragan (%s)", "%sRunning a stall (%s)"), prefix,
						GetPlayerBotShopReasonName(state.bShopOpenReason, en));
				break;
			default:
				snprintf(status, statusSize, PBT(en, "%sPlanuje: %s", "%sPlanning: %s"), prefix, goal);
				break;
		}
	}

	void ManagePlayerBotStatusOverhead(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// A keeper's head already carries its shop sign. Writing the status line
		// over it replaces the one label a passing player actually needs - the
		// name of the stall they are deciding whether to open.
		if (ch && ch->GetMyShop())
			return;
		// The operator's switch in the panel. The status text still goes to the
		// panel's snapshot; only the line over the head is silenced.
		if (!IsPlayerBotOverheadChatEnabled())
			return;
		if (!ch)
			return;

		const BYTE inParty = ch->GetParty() ? 1 : 0;
		const DWORD relevantTargetVID = state.bCurrentAction == BOT_ACTION_FIGHT
				? state.dwTargetVID : 0;
		const BYTE relevantTownPhase = state.bVisitingShop
				? state.bTownVisitPhase : BOT_TOWN_PHASE_NONE;
		const bool changed =
				state.bLastStatusAction != state.bCurrentAction ||
				state.bLastStatusGoal != state.bLongTermGoal ||
				state.bLastStatusTownPhase != relevantTownPhase ||
				state.bLastStatusParty != inParty ||
				state.dwLastStatusTargetVID != relevantTargetVID;
		const bool keepAliveDue = dwNow >= state.dwNextChatTime;
		if (!changed && !keepAliveDue)
			return;
		if (dwNow < state.dwNextStatusProbeTime)
			return;
		if (state.dwLastStatusChatTime != 0 &&
				dwNow - state.dwLastStatusChatTime < 2500)
		{
			state.dwNextStatusProbeTime = state.dwLastStatusChatTime + 2500;
			return;
		}

		// Do not make 350 bots fill the chat window or spend time formatting text
		// nobody can see. A player entering the area gets the current state within
		// three seconds; state changes are otherwise published immediately.
		CCheckNearbyHumanPlayer humanChecker(ch, 2500);
		if (ch->GetSectree())
			ch->GetSectree()->ForEachAround(humanChecker);
		if (!humanChecker.m_bFound)
		{
			state.dwNextStatusProbeTime = dwNow + 3000;
			return;
		}

		char szStatus[160];
		BuildPlayerBotStatusText(ch, state, szStatus, sizeof(szStatus));
		char szStatusEn[160];
		BuildPlayerBotStatusText(ch, state, szStatusEn, sizeof(szStatusEn), true);
		SendPlayerBotOverheadChat(ch, szStatus, szStatusEn);
		state.dwLastStatusChatTime = dwNow;
		state.dwNextStatusProbeTime = dwNow + 2500;
		state.dwNextChatTime = dwNow + number(9000, 14000);
		state.bLastStatusAction = state.bCurrentAction;
		state.bLastStatusGoal = state.bLongTermGoal;
		state.bLastStatusTownPhase = relevantTownPhase;
		state.bLastStatusParty = inParty;
		state.dwLastStatusTargetVID = relevantTargetVID;
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// A bot's personality where a player's alignment title stands (Pabloo's
	// proof of concept of 15 September, "osobowosc zamiast rangi"): the server
	// command "PlayerBotTitle <vid> <personality>", drawn by the client root with
	// textTail.AttachTitle and put back whenever an alignment refresh takes the
	// place (playerbot_status_tail.py). Its own pass and its own clock beside
	// ManagePlayerBotStatusOverhead: the panel's switch for the status line and a
	// keeper's early return belong to that line, not to the title. A player's
	// alignment title is untouched.
	std::map<DWORD, DWORD> s_mapPlayerBotTitleNext;

	void ManagePlayerBotPersonalityTitle(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return;
		DWORD& next = s_mapPlayerBotTitleNext[ch->GetPlayerID()];
		if (next != 0 && dwNow < next)
			return;
		CCheckNearbyHumanPlayer humanChecker(ch, 2500);
		ch->GetSectree()->ForEachAround(humanChecker);
		if (!humanChecker.m_bFound)
		{
			next = dwNow + PLAYERBOT_TITLE_PROBE_MS;
			return;
		}

		// Under Iwakura's personalities the title is the one that claims the
		// bot now, at PERSONA_TITLE_BASE + its id: a client that does not know
		// those ids draws nothing, rather than an old personality's name.
		const unsigned int titleId = (IsPlayerBotPersonaEnabled() && state.persona.bRestored)
				? playerbot_persona::PERSONA_TITLE_BASE + (unsigned int)state.persona.bPersona
				: (unsigned int)state.bPersonality;
		char command[64];
		int commandLen = snprintf(command, sizeof(command), "PlayerBotTitle %u %u",
				(unsigned int)ch->GetVID(), titleId);
		if (commandLen <= 0 || commandLen >= (int)sizeof(command))
			return;
		++commandLen;   // the trailing NUL every chat packet carries

		TPacketGCChat pack_command;
		pack_command.header = HEADER_GC_CHAT;
		pack_command.size = sizeof(TPacketGCChat) + commandLen;
		pack_command.type = CHAT_TYPE_COMMAND;
		pack_command.id = 0;   // the bot's VID travels in the command
		pack_command.bEmpire = 0;

		TEMP_BUFFER commandBuf;
		commandBuf.write(&pack_command, sizeof(TPacketGCChat));
		commandBuf.write(command, commandLen);
		ch->PacketAround(commandBuf.read_peek(), commandBuf.size());
		next = dwNow + (DWORD)number((int)PLAYERBOT_TITLE_RESEND_MIN_MS, (int)PLAYERBOT_TITLE_RESEND_MAX_MS);
	}
#endif
}

#endif
