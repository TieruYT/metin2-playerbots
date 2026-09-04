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

	const char* GetPlayerBotGoalLabel(BYTE goal)
	{
		switch (goal)
		{
			case BOT_GOAL_SURVIVE: return "regeneracja";
			case BOT_GOAL_CHOOSE_PROFESSION: return "profesja";
			case BOT_GOAL_GET_EQUIPMENT: return "ekwipunek";
			case BOT_GOAL_RESTOCK: return "zapasy";
			case BOT_GOAL_REFINE: return "ulepszanie";
			case BOT_GOAL_MASTER_SKILL: return "rozwoj skilla";
			case BOT_GOAL_HUNT_METIN: return "Metiny";
			case BOT_GOAL_PARTY_CHALLENGE: return "silne moby PT";
			case BOT_GOAL_BIOLOGIST: return "Biolog";
			case BOT_GOAL_HUNTING: return "Polowanie";
			case BOT_GOAL_HORSE: return "rozwoj konia";
			case BOT_GOAL_FISHING: return "lowienie ryb";
			default: return "poziom";
		}
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
			default: return "mysle";
		}
	}

	void SendPlayerBotOverheadChat(LPCHARACTER ch, const char* szText)
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

	const char* GetPlayerBotTownStatusLabel(const TPlayerBotAIState& state)
	{
		switch (state.bTownVisitPhase)
		{
			case BOT_TOWN_PHASE_TRAINER: return "Ide po profesje";
			case BOT_TOWN_PHASE_TRAINER_WAIT: return "Wybieram profesje";
			case BOT_TOWN_PHASE_WEAPON_MERCHANT: return "Ide do handlarza bronia";
			case BOT_TOWN_PHASE_WEAPON_WAIT: return "Handluje bronia";
			case BOT_TOWN_PHASE_ARMOR_MERCHANT: return "Ide do handlarza zbroja";
			case BOT_TOWN_PHASE_ARMOR_WAIT: return "Handluje zbroja";
			case BOT_TOWN_PHASE_MISC_MERCHANT: return "Ide do handlarki roznosci";
			case BOT_TOWN_PHASE_MISC_WAIT: return "Kupuje potki i sprzedaje lup";
			case BOT_TOWN_PHASE_BLACKSMITH: return "Ide do kowala";
			case BOT_TOWN_PHASE_BLACKSMITH_WAIT: return "Ulepszam ekwipunek";
			case BOT_TOWN_PHASE_GATE_IN:
			case BOT_TOWN_PHASE_GATE_CROSS_IN: return "Ide do miasta";
			case BOT_TOWN_PHASE_GATE_OUT:
			case BOT_TOWN_PHASE_GATE_CROSS_OUT: return "Wracam na exp";
			default: return "Zalatwiam sprawy w miescie";
		}
	}

	void BuildPlayerBotStatusText(LPCHARACTER ch, const TPlayerBotAIState& state,
			char* status, size_t statusSize)
	{
		if (!ch || !status || statusSize == 0)
			return;

		const char* prefix = ch->GetParty() ? "[PT] " : "";
		const char* goal = GetPlayerBotGoalLabel(state.bLongTermGoal);
		if (state.bVisitingShop)
		{
			snprintf(status, statusSize, "%s%s (cel: %s)", prefix,
					GetPlayerBotTownStatusLabel(state), goal);
			return;
		}

		if (state.bTacticalRetreat)
		{
			snprintf(status, statusSize, "%sUciekam - mam malo HP", prefix);
			return;
		}
		if (state.bRecoveringAfterDeath)
		{
			snprintf(status, statusSize, "%sOdpoczywam po smierci", prefix);
			return;
		}

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		switch (state.bCurrentAction)
		{
			case BOT_ACTION_FIGHT:
				if (target && target->IsStone())
					snprintf(status, statusSize, "%sRozbijam %s", prefix, target->GetName());
				else if (target && target->IsMonster())
				{
					int huntingRemaining = 0;
					const DWORD huntingMob = GetActivePlayerBotHuntingMobVnum(
							ch, &huntingRemaining);
					if (huntingMob != 0 && target->GetRaceNum() == huntingMob)
					{
						snprintf(status, statusSize, "%sPolowanie: %s (zostalo %d)",
								prefix, target->GetName(), huntingRemaining);
						break;
					}
					LPITEM weapon = ch->GetWear(WEAR_WEAPON);
					const bool bow = weapon && weapon->GetType() == ITEM_WEAPON &&
							weapon->GetSubType() == WEAPON_BOW;
					const int range = bow ? 800 : 280;
					const int distance = DISTANCE_APPROX(
							ch->GetX() - target->GetX(), ch->GetY() - target->GetY());
					if (distance > range)
						snprintf(status, statusSize, "%sGonie %s", prefix, target->GetName());
					else
						snprintf(status, statusSize, "%sWalcze z %s", prefix, target->GetName());
				}
				else
					snprintf(status, statusSize, "%sSzukam przeciwnika", prefix);
				break;
			case BOT_ACTION_LOOT:
				snprintf(status, statusSize, "%sPodnosze lup", prefix);
				break;
			case BOT_ACTION_RECOVER:
				snprintf(status, statusSize, "%sRegeneruje HP", prefix);
				break;
			case BOT_ACTION_TRAIN:
				snprintf(status, statusSize, "%sWybieram profesje", prefix);
				break;
			case BOT_ACTION_SHOP:
				snprintf(status, statusSize, "%sHandluje", prefix);
				break;
			case BOT_ACTION_REFINE:
				snprintf(status, statusSize, "%sUlepszam ekwipunek", prefix);
				break;
			case BOT_ACTION_READ_BOOK:
				snprintf(status, statusSize, "%sCzytam ksiege umiejetnosci", prefix);
				break;
			case BOT_ACTION_SOCKET_STONE:
				snprintf(status, statusSize, "%sWkladam kamien duszy", prefix);
				break;
			case BOT_ACTION_PARTY_ASSEMBLE:
				snprintf(status, statusSize, "%sSzukam celu dla grupy", prefix);
				break;
			case BOT_ACTION_BIOLOGIST:
			{
				const TPlayerBotBiologistMission* mission =
						GetActivePlayerBotBiologistMission(ch);
				if (!mission)
					snprintf(status, statusSize, "%sWracam od Biologa", prefix);
				else if (state.bVisitingBiologist &&
						DISTANCE_APPROX(ch->GetX() - PLAYERBOT_BIOLOGIST_X,
								ch->GetY() - PLAYERBOT_BIOLOGIST_Y) > 850)
					snprintf(status, statusSize, "%sIde do Biologa z: %s", prefix, mission->itemLabel);
				else if (state.bVisitingBiologist)
					snprintf(status, statusSize, "%sOddaje Biologowi: %s", prefix, mission->itemLabel);
				else
					snprintf(status, statusSize, "%sZbieram dla Biologa: %s", prefix, mission->itemLabel);
				break;
			}
			case BOT_ACTION_STABLE:
				if (DISTANCE_APPROX(ch->GetX() - PLAYERBOT_STABLE_BOY_X,
						ch->GetY() - PLAYERBOT_STABLE_BOY_Y) > 850)
					snprintf(status, statusSize, "%sIde do Stajennego z medalem", prefix);
				else
					snprintf(status, statusSize, "%sOddaje medal konny (%u/21)", prefix,
							(unsigned int)ch->GetHorseLevel());
				break;
			case BOT_ACTION_FISHING:
				if (ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
						PLAYERBOT_FISHING_BAIT_RESTOCK)
					snprintf(status, statusSize, "%sIde do Rybaka po przynete", prefix);
				else if (DISTANCE_APPROX(ch->GetX() - PLAYERBOT_FISHING_BANK_X,
						ch->GetY() - PLAYERBOT_FISHING_BANK_Y) > 850)
					snprintf(status, statusSize, "%sIde nad rzeke lowic ryby", prefix);
				else if (state.bIsFishing)
					snprintf(status, statusSize, "%sLowie ryby - czekam na branie", prefix);
				else
					snprintf(status, statusSize, "%sZakladam przynete na wedke", prefix);
				break;
			case BOT_ACTION_TRAVEL:
				if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, "%sIde przez portal do M2 po Medal Konny", prefix);
				else if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 &&
						ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) == 0 &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, "%sIde do Lochu Malp po Medal Konny", prefix);
				else if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0)
					snprintf(status, statusSize, "%sIde do najblizszego Stajennego z Medalem", prefix);
				else if (ch->GetMapIndex() == PLAYERBOT_MAP_MONKEY_EASY)
					snprintf(status, statusSize, "%sWychodze z Lochu Malp", prefix);
				else
					snprintf(status, statusSize, "%sSzukam miejsca do expa (cel: %s)", prefix, goal);
				break;
			default:
				snprintf(status, statusSize, "%sPlanuje: %s", prefix, goal);
				break;
		}
	}

	void ManagePlayerBotStatusOverhead(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
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
		SendPlayerBotOverheadChat(ch, szStatus);
		state.dwLastStatusChatTime = dwNow;
		state.dwNextStatusProbeTime = dwNow + 2500;
		state.dwNextChatTime = dwNow + number(9000, 14000);
		state.bLastStatusAction = state.bCurrentAction;
		state.bLastStatusGoal = state.bLongTermGoal;
		state.bLastStatusTownPhase = relevantTownPhase;
		state.bLastStatusParty = inParty;
		state.dwLastStatusTargetVID = relevantTargetVID;
	}
}

#endif
