#ifndef __INC_METIN2_PLAYERBOT_COMPANIONS_H__
#define __INC_METIN2_PLAYERBOT_COMPANIONS_H__

// Iwakura's two social personalities ("SYSTEM OSOBOWOSCI v2.0", 19
// September), under the PERSONA switch: the companion (Towarzysz), who plays
// in a party for the company, and the mercenary (Najemnik), who plays in one
// for money.
//
// The companion is the party system the bots have always had, told how often
// to play in one rather than who may: the PARTY slider's share is drawn afresh
// at every phase instead of once by pid, a Shaman draws low three times as
// often as anybody else, and a party ends the document's way - at eighty
// percent of the bag, when the others leave or walk off, when the levels drift
// apart - and not on a timer. It looks for a person to play with as well as a
// bot ("graczy badz innych botow"): a person with no party is invited, one in
// a person's party with room is asked to be let in, and both are rationed hard
// (PLAYERBOT_COMPANION_HUMAN_*) and refused by the game options' "block party
// invites" and "block party requests" as a player's would be. A companion
// Shaman keeps the whole party's buffs up, the persons first.
//
// The mercenary carries a weaker bot for an hour at a time. A bot that dies to
// monsters three times in half an hour is in distress; a stronger bot of its
// kingdom hunting the same map - three levels up at least, within the engine's
// thirty, and much better gear by level, pluses and Iwakura's tiers - walks up,
// makes the offer, and on the client's yes leads a party of the two, paid up
// front: 250 000 through the yang curve (ScalePlayerBotIwakuraPrice). The
// mercenary goes on hunting where the client is and the client follows it,
// taking half of every kill's experience and its turn at the drops; neither
// changes map while the contract runs. A full bag or an empty potion belt
// pauses the contract - the mercenary goes to town and comes back, and the
// clock stops meanwhile ("PT moze zostac zamrozone ... czas kontraktu wznawia
// swoj bieg") - and at the hour the client pays again unless it has had what it
// came for: three levels, a full bag, or its own gear raised.
//
// An implementation fragment in the sense playerbot_types.h describes. Include
// it exactly once, after playerbot_demon_tower.h: a contract keeps out of the
// tower's business and the war's, and both have to be defined to be asked.

namespace
{
	// -----------------------------------------------------------------------
	// The mercenary's contracts and the population's distress.
	// -----------------------------------------------------------------------
	struct TPlayerBotMercContract
	{
		DWORD mercPid;
		DWORD clientPid;
		// The two characters as they were when the contract was made: a pid
		// that comes back after a logout is another incarnation, and no party
		// it is in is this contract's.
		DWORD mercVid;
		DWORD clientVid;
		long map;
		DWORD startedAt;
		DWORD lastTick;
		DWORD nextCheck;
		// Paid play left. It stands still while the contract is paused.
		DWORD leftMs;
		DWORD pausedSince;
		const char* pauseReason;
		BYTE clientStartLevel;
		int clientStartPower;
		BYTE renewals;
		long long paid;
		TPlayerBotMercContract() : mercPid(0), clientPid(0), mercVid(0), clientVid(0), map(0),
			startedAt(0), lastTick(0), nextCheck(0), leftMs(0), pausedSince(0), pauseReason(""),
			clientStartLevel(0), clientStartPower(0), renewals(0), paid(0) {}
	};

	struct TPlayerBotDistress
	{
		long map;
		BYTE empire;
		DWORD since;
		DWORD mercPid;
		TPlayerBotDistress() : map(0), empire(0), since(0), mercPid(0) {}
	};

	typedef std::map<DWORD, TPlayerBotMercContract> TPlayerBotMercContractMap;
	TPlayerBotMercContractMap s_mapPlayerBotMercContracts;       // by the mercenary's pid
	std::map<DWORD, DWORD> s_mapPlayerBotMercClientOf;           // client pid -> mercenary pid
	std::map<DWORD, TPlayerBotDistress> s_mapPlayerBotDistress;  // by the struggling bot's pid
	std::map<DWORD, DWORD> s_mapPlayerBotMercClientRest;         // client pid -> no contract before

	unsigned int s_uPlayerBotMercOffers = 0;
	unsigned int s_uPlayerBotMercHired = 0;
	unsigned int s_uPlayerBotMercRenewed = 0;
	unsigned int s_uPlayerBotMercRefused = 0;
	unsigned int s_uPlayerBotMercEnded = 0;
	unsigned int s_uPlayerBotMercEndedDone = 0;
	unsigned int s_uPlayerBotMercEndedExpired = 0;
	unsigned int s_uPlayerBotMercPauses = 0;
	unsigned int s_uPlayerBotMercScans = 0;
	unsigned int s_uPlayerBotMercScansFree = 0;
	unsigned int s_uPlayerBotMercScansCapped = 0;
	unsigned int s_uPlayerBotHumanAsks = 0;
	unsigned int s_uPlayerBotHumanJoins = 0;

	bool IsPlayerBotOnMercContract(DWORD pid)
	{
		return s_mapPlayerBotMercContracts.find(pid) != s_mapPlayerBotMercContracts.end() ||
				s_mapPlayerBotMercClientOf.find(pid) != s_mapPlayerBotMercClientOf.end();
	}

	bool IsPlayerBotMercenaryOnContract(DWORD pid)
	{
		return s_mapPlayerBotMercContracts.find(pid) != s_mapPlayerBotMercContracts.end();
	}

	bool IsPlayerBotHiredClient(DWORD pid)
	{
		return s_mapPlayerBotMercClientOf.find(pid) != s_mapPlayerBotMercClientOf.end();
	}

	// The party a contract made: led by a mercenary on one.
	bool IsPlayerBotContractParty(LPPARTY party)
	{
		return party && IsPlayerBotMercenaryOnContract(party->GetLeaderPID());
	}

	// A character by pid, and only the incarnation the contract was made with.
	LPCHARACTER FindPlayerBotMercPartner(DWORD pid, DWORD vid)
	{
		LPCHARACTER ch = pid != 0 ? CHARACTER_MANAGER::instance().FindByPID(pid) : NULL;
		if (!ch || (vid != 0 && (DWORD)ch->GetVID() != vid))
			return NULL;
		return ch;
	}

	long long GetPlayerBotMercPrice()
	{
		return (long long)ScalePlayerBotIwakuraPrice(playerbot_persona::MERC_BASE_PRICE);
	}

	// What a bot wears, as the mercenary rule measures it: the hand's weapon
	// (not a rod or a pickaxe a session put there), the body, the helmet, the
	// shield, the shoes and the three pieces of jewellery, each with its level
	// limit, its plus and Iwakura's PvE tier for the bot's job.
	playerbot_persona::TMercPiece GetPlayerBotMercPiece(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->GetProto())
			return playerbot_persona::TMercPiece();
		const int refine = item->GetRefineLevel();
		const int tier = GetPlayerBotItemTier(item->GetVnum() - (DWORD)std::max(0, refine),
				(int)ch->GetJob(), false);
		return playerbot_persona::TMercPiece((uint8_t)std::max(0, std::min(refine, 255)),
				(uint8_t)GetPlayerBotPersonaLevelLimit(item), (uint8_t)std::max(0, std::min(tier, 255)));
	}

	int GetPlayerBotMercPower(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		playerbot_persona::TMercGear gear;
		gear.piece[0] = GetPlayerBotMercPiece(ch, GetPlayerBotHandWeapon(ch));
		gear.piece[1] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_BODY));
		gear.piece[2] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_HEAD));
		gear.piece[3] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_SHIELD));
		gear.piece[4] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_FOOTS));
		gear.piece[5] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_WRIST));
		gear.piece[6] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_NECK));
		gear.piece[7] = GetPlayerBotMercPiece(ch, ch->GetWear(WEAR_EAR));
		return playerbot_persona::MercGearPower(gear);
	}

	// Ground where nobody takes on a contract or is carried: a dungeon
	// instance, the Demon Tower, a guild war's field and a raid.
	bool IsPlayerBotMercGroundTaken(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		return !ch || ch->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ||
				IsPlayerBotDemonTowerInstance(ch->GetMapIndex()) || ch->GetDungeon() != NULL ||
				state.dwGuildWarEnemyGID != 0 || state.dwTowerRaidGuild != 0 || state.bTowerSummoned;
	}

	// A bot hunting, with nothing else asking for it: the only time the
	// document lets the mercenary appear ("podczas expienia lub dropienia").
	bool IsPlayerBotMercAvailable(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || (int)ch->GetLevel() < PLAYERBOT_MERC_MIN_LEVEL || ch->GetParty())
			return false;
		const TPlayerBotPersona& p = state.persona;
		if ((p.dwMercCooldownUntil != 0 && dwNow < p.dwMercCooldownUntil) ||
				(p.dwCapitulatedUntil != 0 && dwNow < p.dwCapitulatedUntil))
			return false;
		if (CPlayerBotManager::instance().IsMedalDropperCohortPID(ch->GetPlayerID()) ||
				IsPlayerBotMercGroundTaken(ch, state) ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return false;
		if (state.bVisitingShop || state.bFishingSession || state.bMarketTrip ||
				state.bVisitingBiologist || state.bVisitingStable || state.bVisitingHerbalist ||
				state.bServicePending || state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				ch->GetMyShop() != NULL || IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) ||
				IsPlayerBotGambling(state, dwNow) || FindPlayerBotDuelOpponent(ch, dwNow) != NULL)
			return false;
		return !NeedsPlayerBotCriticalTownServices(ch);
	}

	// The weaker side: a bot in distress, alone, not busy elsewhere, not
	// carried a moment ago, and able to pay keeping a quarter of its purse.
	// Returns why not, or NULL when it may be carried - the look round counts
	// the reasons, because a contract that never happens looks the same from
	// outside whichever of them stopped it.
	const char* GetPlayerBotMercClientRefusal(LPCHARACTER client, DWORD dwNow, long long price)
	{
		if (!client || client->IsDead())
			return "dead";
		if (!client->GetDesc() || !client->GetDesc()->IsBot())
			return "person";
		if (client->GetParty() || IsPlayerBotOnMercContract(client->GetPlayerID()))
			return "party";
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(client->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end() || !it->second.persona.bRestored)
			return "state";
		const TPlayerBotAIState& cs = it->second;
		std::map<DWORD, DWORD>::const_iterator rest = s_mapPlayerBotMercClientRest.find(client->GetPlayerID());
		if (rest != s_mapPlayerBotMercClientRest.end() && dwNow < rest->second)
			return "rest";
		if (CPlayerBotManager::instance().IsMedalDropperCohortPID(client->GetPlayerID()) ||
				IsPlayerBotMercGroundTaken(client, cs))
			return "ground";
		if (cs.bVisitingShop || cs.bFishingSession || cs.bMarketTrip || cs.bServicePending ||
				client->GetMyShop() != NULL || IsPlayerBotMiningNow(client->GetPlayerID(), dwNow) ||
				IsPlayerBotGambling(cs, dwNow))
			return "busy";
		// Something only the town can mend comes first: a bot out of potions is
		// not carried, it is sent shopping. A bag already at eighty percent is
		// the same thing: it is what ends a contract ("uzbiera przedmioty z
		// ziemi"), and the first one struck on m2zip was over five seconds
		// after the client had paid its 7.5 million for the hour.
		if (BlocksPlayerBotTravel(client) || cs.persona.bBagFull)
			return "needs_town";
		if (!playerbot_persona::MercClientCanPay((long long)client->GetGold(),
				(long long)GetPlayerBotReservedGold(client), price))
			return "cannot_pay";
		return NULL;
	}

	bool IsPlayerBotMercClientAvailable(LPCHARACTER client, DWORD dwNow, long long price)
	{
		return GetPlayerBotMercClientRefusal(client, dwNow, price) == NULL;
	}

	int GetPlayerBotMercContractCap()
	{
		return std::max(PLAYERBOT_MERC_CONTRACTS_MIN,
				GetPlayerBotsAlive() * PLAYERBOT_MERC_CONTRACTS_PER_MILLE / 1000);
	}

	int CountPlayerBotMercEngagements()
	{
		int engaged = (int)s_mapPlayerBotMercContracts.size();
		for (std::map<DWORD, TPlayerBotDistress>::const_iterator it = s_mapPlayerBotDistress.begin();
				it != s_mapPlayerBotDistress.end(); ++it)
			if (it->second.mercPid != 0)
				++engaged;
		return engaged;
	}

	// Three deaths to monsters in half an hour (playerbot_persona.h calls
	// this): a bot that visibly cannot cope, which a stronger one may offer
	// to carry for a while.
	void NotePlayerBotDistress(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored ||
				IsPlayerBotOnMercContract(ch->GetPlayerID()))
			return;
		TPlayerBotDistress& d = s_mapPlayerBotDistress[ch->GetPlayerID()];
		d.map = ch->GetMapIndex();
		d.empire = ch->GetEmpire();
		d.since = dwNow;
		PlayerBotLogThrottled("merc_distress", dwNow,
				"PLAYERBOT_MERC: in distress pid=%u name=%s level=%u map=%ld deaths=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex(),
				(unsigned int)playerbot_persona::MERC_CLIENT_DEATHS);
	}

	void PrunePlayerBotDistress(DWORD dwNow)
	{
		for (std::map<DWORD, TPlayerBotDistress>::iterator it = s_mapPlayerBotDistress.begin();
				it != s_mapPlayerBotDistress.end();)
		{
			// A reservation by a mercenary that has gone, or is walking to
			// somebody else now, is nobody's.
			if (it->second.mercPid != 0)
			{
				TPlayerBotAIStateMap::const_iterator ms = s_mapPlayerBotAIStates.find(it->second.mercPid);
				if (ms == s_mapPlayerBotAIStates.end() || ms->second.persona.dwMercClientPid != it->first)
					it->second.mercPid = 0;
			}
			if (it->second.mercPid == 0 && dwNow - it->second.since >= PLAYERBOT_MERC_DISTRESS_MS)
				s_mapPlayerBotDistress.erase(it++);
			else
				++it;
		}
		for (std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotMercClientRest.begin();
				it != s_mapPlayerBotMercClientRest.end();)
		{
			if (dwNow >= it->second)
				s_mapPlayerBotMercClientRest.erase(it++);
			else
				++it;
		}
	}

	void AbandonPlayerBotMercApproach(LPCHARACTER ch, TPlayerBotAIState& state, const char* reason, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		std::map<DWORD, TPlayerBotDistress>::iterator d = s_mapPlayerBotDistress.find(p.dwMercClientPid);
		if (d != s_mapPlayerBotDistress.end() && ch && d->second.mercPid == ch->GetPlayerID())
			d->second.mercPid = 0;
		if (ch)
			sys_log(0, "PLAYERBOT_MERC: offer dropped pid=%u name=%s client_pid=%u reason=%s",
					ch->GetPlayerID(), ch->GetName(), p.dwMercClientPid, reason);
		p.dwMercClientPid = 0;
		p.dwMercApproachUntil = 0;
		p.dwMercCooldownUntil = dwNow + PLAYERBOT_MERC_REFUSED_COOLDOWN_MS;
		ClearPlayerBotRoute(state, true);
	}

	// The end of a contract, from either side. The mercenary dissolves the
	// party ("Najemnik natychmiast rozwiazuje grupe"): its leader leaving is
	// the engine's disband. A party that is not the contract's any more -
	// a person's, which either may have been invited into meanwhile - is
	// left alone.
	void EndPlayerBotMercContract(DWORD mercPid, const char* reason, DWORD dwNow)
	{
		TPlayerBotMercContractMap::iterator it = s_mapPlayerBotMercContracts.find(mercPid);
		if (it == s_mapPlayerBotMercContracts.end())
			return;
		const TPlayerBotMercContract c = it->second;
		s_mapPlayerBotMercContracts.erase(it);
		s_mapPlayerBotMercClientOf.erase(c.clientPid);
		LPCHARACTER merc = FindPlayerBotMercPartner(c.mercPid, c.mercVid);
		LPCHARACTER client = FindPlayerBotMercPartner(c.clientPid, c.clientVid);
		LPPARTY party = merc ? merc->GetParty() : NULL;
		if (party && party->GetLeaderPID() == c.mercPid && !IsPlayerBotHumanLedParty(party))
			LeavePlayerBotParty(merc);
		else if (client && client->GetParty() && client->GetParty()->GetLeaderPID() == c.mercPid)
			LeavePlayerBotParty(client);
		TPlayerBotAIStateMap::iterator ms = s_mapPlayerBotAIStates.find(c.mercPid);
		if (ms != s_mapPlayerBotAIStates.end())
		{
			ms->second.persona.dwMercCooldownUntil = dwNow +
					number((int)PLAYERBOT_MERC_COOLDOWN_MIN_MS, (int)PLAYERBOT_MERC_COOLDOWN_MAX_MS);
			ms->second.dwNextPartyCheckTime = dwNow + number(30000, 90000);
		}
		TPlayerBotAIStateMap::iterator cs = s_mapPlayerBotAIStates.find(c.clientPid);
		if (cs != s_mapPlayerBotAIStates.end())
			cs->second.dwNextPartyCheckTime = dwNow + number(30000, 90000);
		s_mapPlayerBotMercClientRest[c.clientPid] = dwNow + PLAYERBOT_MERC_CLIENT_COOLDOWN_MS;
		s_mapPlayerBotDistress.erase(c.clientPid);
		++s_uPlayerBotMercEnded;
		if (!strcmp(reason, "client_done"))
			++s_uPlayerBotMercEndedDone;
		else if (!strcmp(reason, "expired"))
			++s_uPlayerBotMercEndedExpired;
		sys_log(0, "PLAYERBOT_MERC: contract over merc_pid=%u merc=%s client_pid=%u client=%s reason=%s minutes=%u renewals=%u paid=%lld levels=%d",
				c.mercPid, merc ? merc->GetName() : "?", c.clientPid, client ? client->GetName() : "?",
				reason, (unsigned int)((dwNow - c.startedAt) / 60000u), (unsigned int)c.renewals, c.paid,
				client ? (int)client->GetLevel() - (int)c.clientStartLevel : 0);
	}

	// The offer, made standing beside the client. The client pays if it can
	// and nothing else holds it (asked again here: a minute can pass on the
	// walk), the engine's own party rules are asked of the two as they would
	// be of players, and the mercenary leads.
	bool OfferPlayerBotMercContract(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER client, DWORD dwNow)
	{
		const long long price = GetPlayerBotMercPrice();
		const char* refusal = NULL;
		if (!IsPlayerBotMercClientAvailable(client, dwNow, price))
			refusal = "client_declines";
		else if (client->GetEmpire() != ch->GetEmpire() ||
				abs((int)ch->GetLevel() - (int)client->GetLevel()) > playerbot_persona::MERC_PARTY_LEVEL_GAP ||
				ch->GetParty() != NULL)
			refusal = "party_rule";
		LPPARTY party = NULL;
		if (!refusal)
		{
			party = CPartyManager::instance().CreateParty(ch);
			if (!party)
				refusal = "no_party";
		}
		if (refusal)
		{
			++s_uPlayerBotMercRefused;
			// A client that would not or could not is nobody else's to try
			// for a while either.
			s_mapPlayerBotDistress.erase(client->GetPlayerID());
			AbandonPlayerBotMercApproach(ch, state, refusal, dwNow);
			return false;
		}
		party->Link(ch);
		party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
		party->Join(client->GetPlayerID());
		party->Link(client);
		PlayerBotChangeGold(client, -price);
		PlayerBotChangeGold(ch, price);

		TPlayerBotMercContract c;
		c.mercPid = ch->GetPlayerID();
		c.clientPid = client->GetPlayerID();
		c.mercVid = (DWORD)ch->GetVID();
		c.clientVid = (DWORD)client->GetVID();
		c.map = ch->GetMapIndex();
		c.startedAt = dwNow;
		c.lastTick = dwNow;
		c.nextCheck = dwNow + PLAYERBOT_MERC_CHECK_INTERVAL;
		c.leftMs = playerbot_persona::MERC_CONTRACT_MS;
		c.clientStartLevel = (BYTE)std::min<int>(255, client->GetLevel());
		c.clientStartPower = GetPlayerBotMercPower(client);
		c.paid = price;
		s_mapPlayerBotMercContracts[c.mercPid] = c;
		s_mapPlayerBotMercClientOf[c.clientPid] = c.mercPid;
		s_mapPlayerBotDistress.erase(c.clientPid);

		TPlayerBotPersona& p = state.persona;
		p.dwMercClientPid = 0;
		p.dwMercApproachUntil = 0;
		state.dwPartyExpireTime = 0;
		TPlayerBotAIStateMap::iterator cs = s_mapPlayerBotAIStates.find(c.clientPid);
		if (cs != s_mapPlayerBotAIStates.end())
		{
			cs->second.dwPartyExpireTime = 0;
			ClearPlayerBotRoute(cs->second, true);
		}
		RememberPlayerBotEncounter(ch, client, PLAYERBOT_FRIEND_PARTY_POINTS, dwNow);
		++s_uPlayerBotMercHired;
		sys_log(0, "PLAYERBOT_MERC: hired merc_pid=%u merc=%s level=%u power=%d client_pid=%u client=%s client_level=%u client_power=%d price=%lld map=%ld",
				c.mercPid, ch->GetName(), (unsigned int)ch->GetLevel(), GetPlayerBotMercPower(ch),
				c.clientPid, client->GetName(), (unsigned int)client->GetLevel(), c.clientStartPower,
				price, c.map);
		return true;
	}

	// The walk to a client chosen on the last look round. A fight on the way
	// is fought first; the offer is made within PLAYERBOT_MERC_OFFER_DISTANCE.
	bool ApproachPlayerBotMercClient(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		LPCHARACTER client = CHARACTER_MANAGER::instance().FindByPID(p.dwMercClientPid);
		const long long price = GetPlayerBotMercPrice();
		const char* why = NULL;
		if (dwNow >= p.dwMercApproachUntil)
			why = "timeout";
		else if (!client || client->GetMapIndex() != ch->GetMapIndex())
			why = "client_gone";
		else if (!IsPlayerBotMercAvailable(ch, state, dwNow))
			why = "busy";
		else if (!IsPlayerBotMercClientAvailable(client, dwNow, price))
			why = "client_busy";
		if (why)
		{
			AbandonPlayerBotMercApproach(ch, state, why, dwNow);
			return false;
		}
		if (ch->GetVictim() && !ch->GetVictim()->IsDead())
			return false;
		const int dist = DISTANCE_APPROX(ch->GetX() - client->GetX(), ch->GetY() - client->GetY());
		if (dist > PLAYERBOT_MERC_OFFER_DISTANCE)
		{
			if (!MovePlayerBot(ch, client->GetX(), client->GetY(), dwNow, 8, true, true))
			{
				if (state.bStuckCounter >= 4)
					AbandonPlayerBotMercApproach(ch, state, "unreachable", dwNow);
				return false;
			}
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			return true;
		}
		OfferPlayerBotMercContract(ch, state, client, dwNow);
		return true;
	}

	// The look round, on the bot's own clock: the nearest bot in distress on
	// this map, of this kingdom, that this bot outclasses, not already taken by
	// another mercenary, while the core carries fewer than its share of
	// contracts.
	void ScanPlayerBotMercClients(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (p.dwNextMercScan != 0 && dwNow < p.dwNextMercScan)
			return;
		p.dwNextMercScan = dwNow + number((int)PLAYERBOT_MERC_SCAN_MIN_MS, (int)PLAYERBOT_MERC_SCAN_MAX_MS);
		PrunePlayerBotDistress(dwNow);
		// How many bots even look, and how many of those are free to carry
		// anybody: without this a contract that never happens says nothing
		// about which half of the rule stopped it.
		++s_uPlayerBotMercScans;
		if (s_mapPlayerBotDistress.empty() || !IsPlayerBotMercAvailable(ch, state, dwNow))
			return;
		++s_uPlayerBotMercScansFree;
		if (CountPlayerBotMercEngagements() >= GetPlayerBotMercContractCap())
		{
			++s_uPlayerBotMercScansCapped;
			return;
		}
		const long long price = GetPlayerBotMercPrice();
		const int myPower = GetPlayerBotMercPower(ch);
		LPCHARACTER best = NULL;
		int bestDist = INT_MAX;
		int bestPower = 0;
		// Why nobody here could be carried, for the line below.
		int here = 0, notOutclassed = 0;
		std::map<std::string, int> refusals;
		for (std::map<DWORD, TPlayerBotDistress>::const_iterator it = s_mapPlayerBotDistress.begin();
				it != s_mapPlayerBotDistress.end(); ++it)
		{
			const TPlayerBotDistress& d = it->second;
			if (it->first == ch->GetPlayerID() || d.mercPid != 0 || d.map != ch->GetMapIndex() ||
					d.empire != ch->GetEmpire())
				continue;
			LPCHARACTER client = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!client || client->GetMapIndex() != ch->GetMapIndex())
				continue;
			++here;
			const char* refusal = GetPlayerBotMercClientRefusal(client, dwNow, price);
			if (refusal)
			{
				++refusals[refusal];
				continue;
			}
			const int power = GetPlayerBotMercPower(client);
			if (!playerbot_persona::MercOutclasses((int)ch->GetLevel(), myPower, (int)client->GetLevel(), power))
			{
				++notOutclassed;
				continue;
			}
			const int dist = DISTANCE_APPROX(ch->GetX() - client->GetX(), ch->GetY() - client->GetY());
			if (dist > PLAYERBOT_MERC_NOTICE_RANGE)
			{
				++refusals["too_far"];
				continue;
			}
			if (dist < bestDist)
			{
				best = client;
				bestDist = dist;
				bestPower = power;
			}
		}
		if (!best)
		{
			if (here > 0)
			{
				std::string why;
				for (std::map<std::string, int>::const_iterator r = refusals.begin(); r != refusals.end(); ++r)
				{
					char part[48];
					snprintf(part, sizeof(part), " %s=%d", r->first.c_str(), r->second);
					why += part;
				}
				PlayerBotLogThrottled("merc_nobody", dwNow,
						"PLAYERBOT_MERC: nobody to carry pid=%u name=%s level=%u power=%d map=%ld distress_here=%d not_outclassed=%d%s",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), myPower,
						ch->GetMapIndex(), here, notOutclassed, why.c_str());
			}
			return;
		}
		s_mapPlayerBotDistress[best->GetPlayerID()].mercPid = ch->GetPlayerID();
		p.dwMercClientPid = best->GetPlayerID();
		p.dwMercApproachUntil = dwNow + PLAYERBOT_MERC_APPROACH_MS;
		ClearPlayerBotRoute(state, true);
		++s_uPlayerBotMercOffers;
		sys_log(0, "PLAYERBOT_MERC: offers to carry pid=%u name=%s level=%u power=%d client_pid=%u client=%s client_level=%u client_power=%d price=%lld dist=%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), myPower,
				best->GetPlayerID(), best->GetName(), (unsigned int)best->GetLevel(), bestPower,
				price, bestDist, ch->GetMapIndex());
	}

	// Out of the contract's reach for now: a town errand, an empty belt, a
	// full bag - what pauses a contract and what it waits for.
	bool IsPlayerBotMercAway(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		return !ch || state.bVisitingShop || state.bServicePending || NeedsPlayerBotCriticalTownServices(ch);
	}

	void PausePlayerBotMercContract(LPCHARACTER ch, TPlayerBotMercContract& c, const char* reason, DWORD dwNow)
	{
		if (c.pausedSince != 0)
			return;
		c.pausedSince = dwNow;
		c.pauseReason = reason;
		++s_uPlayerBotMercPauses;
		sys_log(0, "PLAYERBOT_MERC: contract paused merc_pid=%u merc=%s client_pid=%u reason=%s left_min=%u",
				c.mercPid, ch ? ch->GetName() : "?", c.clientPid, reason, c.leftMs / 60000u);
	}

	// The party of the two again, after a pause took it apart: the mercenary
	// leads, as it did.
	bool RejoinPlayerBotMercParty(LPCHARACTER ch, LPCHARACTER client)
	{
		LPPARTY party = ch->GetParty();
		if (party && party->GetLeaderPID() != ch->GetPlayerID())
			return false;
		if (client->GetParty() && client->GetParty() != party)
			return false;
		if (!party)
		{
			party = CPartyManager::instance().CreateParty(ch);
			if (!party)
				return false;
			party->Link(ch);
			party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
		}
		if (client->GetParty() != party)
		{
			party->Join(client->GetPlayerID());
			party->Link(client);
		}
		return true;
	}

	// The mercenary's side of a contract, every tick: the paid clock, the
	// pause and the way back, the client's goal, and the hour. Returns true
	// while it walks (or warps) back to its client.
	bool ManagePlayerBotMercContract(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotMercContract& c, DWORD dwNow)
	{
		const DWORD dt = c.lastTick != 0 ? dwNow - c.lastTick : 0;
		c.lastTick = dwNow;
		if (c.pausedSince == 0)
			c.leftMs = dt >= c.leftMs ? 0 : c.leftMs - dt;
		const DWORD mercPid = c.mercPid;
		LPCHARACTER client = FindPlayerBotMercPartner(c.clientPid, c.clientVid);
		if (!IsPlayerBotPersonaEnabled())
		{
			EndPlayerBotMercContract(mercPid, "switch_off", dwNow);
			return false;
		}
		if (!client)
		{
			EndPlayerBotMercContract(mercPid, "client_gone", dwNow);
			return false;
		}
		if (dwNow - c.startedAt >= PLAYERBOT_MERC_WALL_MAX_MS)
		{
			EndPlayerBotMercContract(mercPid, "worn_out", dwNow);
			return false;
		}
		// A person's party either of them was invited into meanwhile ends it:
		// the bot never refuses a person.
		if ((ch->GetParty() && ch->GetParty()->GetLeaderPID() != mercPid) ||
				(client->GetParty() && client->GetParty()->GetLeaderPID() != mercPid))
		{
			EndPlayerBotMercContract(mercPid, "party_taken", dwNow);
			return false;
		}
		// So does either of them called to its guild's war or the Demon Tower:
		// those move a bot about by themselves, and a contract chasing it round
		// the maps would be chasing the guild.
		TPlayerBotAIStateMap::const_iterator clientState = s_mapPlayerBotAIStates.find(c.clientPid);
		if (IsPlayerBotMercGroundTaken(ch, state) ||
				(clientState != s_mapPlayerBotAIStates.end() &&
				 IsPlayerBotMercGroundTaken(client, clientState->second)))
		{
			EndPlayerBotMercContract(mercPid, "called_away", dwNow);
			return false;
		}
		const bool together = ch->GetParty() != NULL && ch->GetParty() == client->GetParty();

		if (c.pausedSince != 0)
		{
			if (dwNow - c.pausedSince >= PLAYERBOT_MERC_PAUSE_MAX_MS)
			{
				EndPlayerBotMercContract(mercPid, "away_too_long", dwNow);
				return false;
			}
			if (ch->IsDead() || IsPlayerBotMercAway(ch, state))
				return false;
			if (ch->GetVictim() && !ch->GetVictim()->IsDead())
				return false;
			// Back to the client's map the way a player comes back to a friend:
			// straight to them. The warp is the one ManagePlayerBotFollowHumanLeader
			// makes for a player's party, and for the same reason - a bot has no
			// client to take a Teleporter's warp with.
			if (ch->GetMapIndex() != client->GetMapIndex())
			{
				if (client->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ||
						!IsPlayerBotMapHostedHere(client->GetMapIndex()))
				{
					EndPlayerBotMercContract(mercPid, "client_unreachable", dwNow);
					return false;
				}
				if (!TransitionPlayerBotMap(ch, state, client->GetMapIndex(), client->GetX(), client->GetY(),
						dwNow, "merc_return"))
					return false;
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				return true;
			}
			const int dist = DISTANCE_APPROX(ch->GetX() - client->GetX(), ch->GetY() - client->GetY());
			if (dist > PLAYERBOT_MERC_REJOIN_DISTANCE)
			{
				if (!MovePlayerBot(ch, client->GetX(), client->GetY(), dwNow, 8, true, true))
					return false;
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				state.dwLastMeaningfulActivityTime = dwNow;
				return true;
			}
			if (!together && !RejoinPlayerBotMercParty(ch, client))
			{
				EndPlayerBotMercContract(mercPid, "party_refused", dwNow);
				return false;
			}
			sys_log(0, "PLAYERBOT_MERC: contract resumed merc_pid=%u merc=%s client_pid=%u client=%s paused_min=%u left_min=%u",
					mercPid, ch->GetName(), c.clientPid, client->GetName(),
					(dwNow - c.pausedSince) / 60000u, c.leftMs / 60000u);
			c.pausedSince = 0;
			c.pauseReason = "";
			c.map = ch->GetMapIndex();
			return false;
		}

		// Running. What makes the mercenary go to town pauses it; a party the
		// engine took apart (a death on another map, a warp) is put together
		// again the same way.
		if (IsPlayerBotMercAway(ch, state))
		{
			PausePlayerBotMercContract(ch, c, IsPlayerBotBagFull(ch) ? "bag_full" : "town", dwNow);
			return false;
		}
		if (!together || ch->GetMapIndex() != client->GetMapIndex())
		{
			PausePlayerBotMercContract(ch, c, "party_lost", dwNow);
			return false;
		}
		if (dwNow < c.nextCheck)
			return false;
		c.nextCheck = dwNow + PLAYERBOT_MERC_CHECK_INTERVAL;

		// The client's side of the bargain: it leaves once it has had what it
		// came for ("sam dobrowolnie opusci grupe przed czasem"), and it goes
		// to town for what the town alone mends.
		const int levelsGained = (int)client->GetLevel() - (int)c.clientStartLevel;
		const int gearGain = GetPlayerBotMercPower(client) - c.clientStartPower;
		const bool goal = playerbot_persona::MercClientGoalReached(levelsGained,
				IsPlayerBotBagFull(client), gearGain);
		if (goal)
		{
			EndPlayerBotMercContract(mercPid, "client_done", dwNow);
			return false;
		}
		if (BlocksPlayerBotTravel(client))
		{
			EndPlayerBotMercContract(mercPid, "client_needs_town", dwNow);
			return false;
		}
		if (c.leftMs > 0)
			return false;
		// The hour is up: another, if the client still needs one and can pay.
		const long long price = GetPlayerBotMercPrice();
		const bool canPay = playerbot_persona::MercClientCanPay((long long)client->GetGold(),
				(long long)GetPlayerBotReservedGold(client), price);
		const bool outclassed = playerbot_persona::MercOutclasses((int)ch->GetLevel(),
				GetPlayerBotMercPower(ch), (int)client->GetLevel(), GetPlayerBotMercPower(client));
		if (!playerbot_persona::MercClientRenews(goal, canPay, outclassed))
		{
			EndPlayerBotMercContract(mercPid, "expired", dwNow);
			return false;
		}
		PlayerBotChangeGold(client, -price);
		PlayerBotChangeGold(ch, price);
		c.leftMs = playerbot_persona::MERC_CONTRACT_MS;
		c.paid += price;
		++c.renewals;
		++s_uPlayerBotMercRenewed;
		sys_log(0, "PLAYERBOT_MERC: contract renewed merc_pid=%u merc=%s client_pid=%u client=%s price=%lld renewals=%u",
				mercPid, ch->GetName(), c.clientPid, client->GetName(), price, (unsigned int)c.renewals);
		return false;
	}

	// The client's side: it keeps up with its mercenary while the contract
	// runs, and plays on by itself on the same map while it is paused.
	bool ManagePlayerBotMercClient(LPCHARACTER ch, TPlayerBotAIState& state, DWORD mercPid, DWORD dwNow)
	{
		TPlayerBotMercContractMap::iterator it = s_mapPlayerBotMercContracts.find(mercPid);
		if (it == s_mapPlayerBotMercContracts.end())
		{
			s_mapPlayerBotMercClientOf.erase(ch->GetPlayerID());
			return false;
		}
		TPlayerBotMercContract& c = it->second;
		LPCHARACTER merc = FindPlayerBotMercPartner(c.mercPid, c.mercVid);
		if (!merc)
		{
			EndPlayerBotMercContract(mercPid, "mercenary_gone", dwNow);
			return false;
		}
		if (c.pausedSince != 0 || ch->IsDead() || merc->GetMapIndex() != ch->GetMapIndex())
			return false;
		if (ch->GetVictim() && !ch->GetVictim()->IsDead())
			return false;
		const int dist = DISTANCE_APPROX(ch->GetX() - merc->GetX(), ch->GetY() - merc->GetY());
		if (dist <= PLAYERBOT_MERC_CARRY_RANGE)
			return false;
		static std::map<DWORD, DWORD> s_mapPlayerBotMercFollowNext;
		DWORD& next = s_mapPlayerBotMercFollowNext[ch->GetPlayerID()];
		if (dwNow < next)
			return false;
		next = dwNow + PLAYERBOT_MERC_FOLLOW_INTERVAL;
		if (!MovePlayerBot(ch, merc->GetX(), merc->GetY(), dwNow, 8, true, true))
			return false;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		return true;
	}

	// The mercenary, each tick: a contract's upkeep for either side, the walk
	// to a client chosen on the last look round, and the look round itself.
	bool ManagePlayerBotMercenary(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotMercContractMap::iterator own = s_mapPlayerBotMercContracts.find(pid);
		if (own != s_mapPlayerBotMercContracts.end())
			return ManagePlayerBotMercContract(ch, state, own->second, dwNow);
		std::map<DWORD, DWORD>::const_iterator hired = s_mapPlayerBotMercClientOf.find(pid);
		if (hired != s_mapPlayerBotMercClientOf.end())
			return ManagePlayerBotMercClient(ch, state, hired->second, dwNow);
		if (!IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
		{
			if (state.persona.dwMercClientPid != 0)
				AbandonPlayerBotMercApproach(ch, state, "switch_off", dwNow);
			return false;
		}
		if (ch->IsDead())
			return false;
		if (state.persona.dwMercClientPid != 0)
			return ApproachPlayerBotMercClient(ch, state, dwNow);
		ScanPlayerBotMercClients(ch, state, dwNow);
		return false;
	}

	// Whether a bot keeps its map for somebody else's sake: a client for its
	// contract, a mercenary while the contract runs and whenever nothing in
	// town still wants it (a paused one is let go to the town and brought back
	// by the contract, not by the road), and a bot in a party it leads with a
	// person in it, which keeps its map as a player's party does.
	bool IsPlayerBotPartyWithHuman(LPPARTY party)
	{
		if (!party)
			return false;
		struct FFindPerson
		{
			bool found;
			FFindPerson() : found(false) {}
			void operator()(LPCHARACTER member)
			{
				if (member && member->IsPC() && (!member->GetDesc() || !member->GetDesc()->IsBot()))
					found = true;
			}
		};
		FFindPerson finder;
		party->ForEachOnlineMember(finder);
		return finder.found;
	}

	bool IsPlayerBotHeldForCompany(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const DWORD pid = ch->GetPlayerID();
		if (IsPlayerBotHiredClient(pid))
			return true;
		TPlayerBotMercContractMap::const_iterator own = s_mapPlayerBotMercContracts.find(pid);
		if (own != s_mapPlayerBotMercContracts.end())
		{
			if (own->second.pausedSince == 0)
				return true;
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
			return st != s_mapPlayerBotAIStates.end() && !IsPlayerBotMercAway(ch, st->second);
		}
		LPPARTY party = ch->GetParty();
		return party && party->GetLeaderPID() == pid && !IsPlayerBotHumanLedParty(party) &&
				IsPlayerBotPartyWithHuman(party);
	}

	// The line over the bot's head for either side of a contract, and for the
	// walk to offer one. Only when it is not fighting: a fight says what it is
	// fighting.
	bool BuildPlayerBotMercStatus(LPCHARACTER ch, const TPlayerBotAIState& state, const char* prefix,
			char* status, size_t statusSize)
	{
		if (!ch || !status || statusSize == 0)
			return false;
		const DWORD pid = ch->GetPlayerID();
		const DWORD dwNow = get_dword_time();
		TPlayerBotMercContractMap::const_iterator own = s_mapPlayerBotMercContracts.find(pid);
		if (own != s_mapPlayerBotMercContracts.end())
		{
			const TPlayerBotMercContract& c = own->second;
			LPCHARACTER client = CHARACTER_MANAGER::instance().FindByPID(c.clientPid);
			if (c.pausedSince != 0)
				snprintf(status, statusSize, "%sNajemnik: przerwa w kontrakcie, wroce do %s", prefix,
						client ? client->GetName() : "klienta");
			else
				snprintf(status, statusSize, "%sNajemnik: chronie %s (jeszcze %u min)", prefix,
						client ? client->GetName() : "klienta", c.leftMs / 60000u + 1u);
			return true;
		}
		std::map<DWORD, DWORD>::const_iterator hired = s_mapPlayerBotMercClientOf.find(pid);
		if (hired != s_mapPlayerBotMercClientOf.end())
		{
			TPlayerBotMercContractMap::const_iterator it = s_mapPlayerBotMercContracts.find(hired->second);
			if (it == s_mapPlayerBotMercContracts.end())
				return false;
			LPCHARACTER merc = CHARACTER_MANAGER::instance().FindByPID(hired->second);
			if (it->second.pausedSince != 0)
				snprintf(status, statusSize, "%sCzekam na najemnika %s", prefix,
						merc ? merc->GetName() : "?");
			else
				snprintf(status, statusSize, "%sWynajalem najemnika %s (jeszcze %u min)", prefix,
						merc ? merc->GetName() : "?", it->second.leftMs / 60000u + 1u);
			return true;
		}
		if (state.persona.dwMercClientPid != 0 && dwNow < state.persona.dwMercApproachUntil)
		{
			LPCHARACTER client = CHARACTER_MANAGER::instance().FindByPID(state.persona.dwMercClientPid);
			snprintf(status, statusSize, "%sIde zaoferowac pomoc %s (%lld yang za godzine)", prefix,
					client ? client->GetName() : "?", GetPlayerBotMercPrice());
			return true;
		}
		return false;
	}

	void ReportPlayerBotMercCensus(DWORD dwNow)
	{
		static DWORD s_dwReported = 0;
		if (s_dwReported != 0 && dwNow - s_dwReported < 600000)
			return;
		const bool first = s_dwReported == 0;
		s_dwReported = dwNow;
		if (first || !IsPlayerBotPersonaEnabled())
			return;
		unsigned int paused = 0;
		for (TPlayerBotMercContractMap::const_iterator it = s_mapPlayerBotMercContracts.begin();
				it != s_mapPlayerBotMercContracts.end(); ++it)
			if (it->second.pausedSince != 0)
				++paused;
		unsigned int approaching = 0;
		std::map<long, int> distressByMap;
		for (std::map<DWORD, TPlayerBotDistress>::const_iterator it = s_mapPlayerBotDistress.begin();
				it != s_mapPlayerBotDistress.end(); ++it)
		{
			if (it->second.mercPid != 0)
				++approaching;
			++distressByMap[it->second.map];
		}
		// Which maps they are on: a bot in distress where no bot of its kingdom
		// hunts alone is a contract that cannot happen, and the line has to say
		// so rather than leave "distress=7 hired=0" to be guessed at.
		std::string maps;
		for (std::map<long, int>::const_iterator m = distressByMap.begin(); m != distressByMap.end(); ++m)
		{
			char part[32];
			snprintf(part, sizeof(part), " %ld:%d", m->first, m->second);
			maps += part;
		}
		sys_log(0, "PLAYERBOT_MERC: census contracts=%u paused=%u approaching=%u distress=%u cap=%d | offers=%u hired=%u renewed=%u refused=%u pauses=%u ended=%u client_done=%u expired=%u | players_asked=%u players_joined=%u price=%lld | scans=%u free=%u capped=%u | distress_maps%s",
				(unsigned int)s_mapPlayerBotMercContracts.size(), paused, approaching,
				(unsigned int)s_mapPlayerBotDistress.size(), GetPlayerBotMercContractCap(),
				s_uPlayerBotMercOffers, s_uPlayerBotMercHired, s_uPlayerBotMercRenewed,
				s_uPlayerBotMercRefused, s_uPlayerBotMercPauses, s_uPlayerBotMercEnded,
				s_uPlayerBotMercEndedDone, s_uPlayerBotMercEndedExpired,
				s_uPlayerBotHumanAsks, s_uPlayerBotHumanJoins, GetPlayerBotMercPrice(),
				s_uPlayerBotMercScans, s_uPlayerBotMercScansFree, s_uPlayerBotMercScansCapped,
				maps.c_str());
		s_uPlayerBotMercOffers = s_uPlayerBotMercHired = s_uPlayerBotMercRenewed = 0;
		s_uPlayerBotMercRefused = s_uPlayerBotMercPauses = s_uPlayerBotMercEnded = 0;
		s_uPlayerBotMercEndedDone = s_uPlayerBotMercEndedExpired = 0;
		s_uPlayerBotHumanAsks = s_uPlayerBotHumanJoins = 0;
		s_uPlayerBotMercScans = s_uPlayerBotMercScansFree = s_uPlayerBotMercScansCapped = 0;
	}

	// -----------------------------------------------------------------------
	// The companion.
	// -----------------------------------------------------------------------

	// People the companions have asked, and when anybody may ask them again;
	// and when this bot last asked this person.
	std::map<DWORD, DWORD> s_mapPlayerBotHumanAskNext;
	std::map<unsigned long long, DWORD> s_mapPlayerBotHumanAskedPair;

	enum EPlayerBotHumanAsk
	{
		BOT_HUMAN_ASK_NONE = 0,
		BOT_HUMAN_ASK_INVITE,
		BOT_HUMAN_ASK_REQUEST
	};

	// A person worth asking: of this kingdom, a few levels either way, near,
	// playing (not in a dungeon, not behind a counter, not a Game Master), and
	// not asked by anybody lately. One in a party is asked through its leader,
	// who must be a person too and have room - a bot's party is the bot
	// finder's business.
	bool IsPlayerBotHumanCompanionCandidate(LPCHARACTER me, LPCHARACTER human, DWORD dwNow)
	{
		if (!me || !human || human == me || !human->IsPC() || human->IsDead() ||
				!human->GetDesc() || human->GetDesc()->IsBot())
			return false;
		if (human->GetGMLevel() > GM_PLAYER || human->GetEmpire() != me->GetEmpire() ||
				human->GetMapIndex() != me->GetMapIndex() || human->GetDungeon() != NULL ||
				human->IsObserverMode() || human->GetMyShop() != NULL)
			return false;
		if (abs((int)human->GetLevel() - (int)me->GetLevel()) > PLAYERBOT_COMPANION_HUMAN_LEVEL_RANGE)
			return false;
		if (DISTANCE_APPROX(me->GetX() - human->GetX(), me->GetY() - human->GetY()) > PLAYERBOT_COMPANION_HUMAN_RANGE)
			return false;
		std::map<DWORD, DWORD>::const_iterator next = s_mapPlayerBotHumanAskNext.find(human->GetPlayerID());
		if (next != s_mapPlayerBotHumanAskNext.end() && dwNow < next->second)
			return false;
		const unsigned long long pairKey = ((unsigned long long)me->GetPlayerID() << 32) | human->GetPlayerID();
		std::map<unsigned long long, DWORD>::const_iterator pair = s_mapPlayerBotHumanAskedPair.find(pairKey);
		if (pair != s_mapPlayerBotHumanAskedPair.end() && dwNow - pair->second < PLAYERBOT_COMPANION_HUMAN_PAIR_GAP)
			return false;
		LPPARTY party = human->GetParty();
		if (!party)
			return !human->IsBlockMode(BLOCK_PARTY_INVITE);
		LPCHARACTER leader = party->GetLeaderCharacter();
		return leader && IsPlayerBotHumanLedParty(party) &&
				(DWORD)party->GetMemberCount() < (DWORD)PARTY_MAX_MEMBER &&
				!leader->IsBlockMode(BLOCK_PARTY_REQUEST);
	}

	struct FFindPlayerBotHumanCompanion
	{
		LPCHARACTER me;
		DWORD now;
		LPCHARACTER best;
		int bestDist;
		FFindPlayerBotHumanCompanion(LPCHARACTER ch, DWORD dwNow) : me(ch), now(dwNow), best(NULL), bestDist(INT_MAX) {}
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
			if (!IsPlayerBotHumanCompanionCandidate(me, ch, now))
				return;
			const int dist = DISTANCE_APPROX(me->GetX() - ch->GetX(), me->GetY() - ch->GetY());
			if (dist < bestDist)
			{
				best = ch;
				bestDist = dist;
			}
		}
	};

	// A companion with nobody yet, or leading a party of bots with room, asks
	// a person nearby: an invitation to a person on their own, a request to a
	// person's party. The engine's own gates answer for the rest - another
	// kingdom, thirty levels, a full party - and so do the game options.
	bool AskPlayerBotHumanCompanion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return false;
		TPlayerBotPersona& p = state.persona;
		if (p.dwAskedHumanPid != 0 || (p.dwNextHumanAsk != 0 && dwNow < p.dwNextHumanAsk))
			return false;
		LPPARTY mine = ch->GetParty();
		if (mine && (mine->GetLeaderPID() != ch->GetPlayerID() ||
				(DWORD)mine->GetMemberCount() >= (DWORD)PARTY_MAX_MEMBER || IsPlayerBotHumanLedParty(mine) ||
				IsPlayerBotContractParty(mine)))
			return false;
		FFindPlayerBotHumanCompanion finder(ch, dwNow);
		ch->GetSectree()->ForEachAround(finder);
		LPCHARACTER human = finder.best;
		if (!human)
			return false;
		LPPARTY theirs = human->GetParty();
		BYTE how = BOT_HUMAN_ASK_NONE;
		if (theirs)
		{
			// A bot that leads its own party does not ask to join another.
			if (mine || !ch->RequestToParty(theirs->GetLeaderCharacter()))
				return false;
			how = BOT_HUMAN_ASK_REQUEST;
		}
		else
		{
			ch->PartyInvite(human);
			how = BOT_HUMAN_ASK_INVITE;
		}
		s_mapPlayerBotHumanAskNext[human->GetPlayerID()] = dwNow + PLAYERBOT_COMPANION_HUMAN_ASK_GAP;
		s_mapPlayerBotHumanAskedPair[((unsigned long long)ch->GetPlayerID() << 32) | human->GetPlayerID()] = dwNow;
		p.dwAskedHumanPid = human->GetPlayerID();
		p.dwAskedHumanAt = dwNow;
		p.bAskedHow = how;
		p.dwNextHumanAsk = dwNow + PLAYERBOT_COMPANION_ASK_GAP;
		++s_uPlayerBotHumanAsks;
		sys_log(0, "PLAYERBOT_PARTY: asked a player pid=%u name=%s level=%u player_pid=%u player=%s player_level=%u how=%s map=%ld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), human->GetPlayerID(),
				human->GetName(), (unsigned int)human->GetLevel(),
				how == BOT_HUMAN_ASK_REQUEST ? "request" : "invite", ch->GetMapIndex());
		return true;
	}

	// The answer, read once the engine's ten seconds are over: in the same
	// party is a yes, anything else a no, and a no leaves the person alone for
	// longer.
	void ResolvePlayerBotHumanAsk(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (p.dwAskedHumanPid == 0 || dwNow - p.dwAskedHumanAt < PLAYERBOT_COMPANION_ASK_ANSWER_MS)
			return;
		LPCHARACTER human = CHARACTER_MANAGER::instance().FindByPID(p.dwAskedHumanPid);
		const bool joined = human && ch->GetParty() && human->GetParty() == ch->GetParty();
		if (joined)
		{
			++s_uPlayerBotHumanJoins;
			sys_log(0, "PLAYERBOT_PARTY: asked a player, yes pid=%u name=%s player_pid=%u player=%s how=%s",
					ch->GetPlayerID(), ch->GetName(), p.dwAskedHumanPid, human->GetName(),
					p.bAskedHow == BOT_HUMAN_ASK_REQUEST ? "request" : "invite");
		}
		else
		{
			DWORD& next = s_mapPlayerBotHumanAskNext[p.dwAskedHumanPid];
			next = std::max<DWORD>(next, dwNow + PLAYERBOT_COMPANION_HUMAN_DECLINED_GAP);
			sys_log(0, "PLAYERBOT_PARTY: asked a player, no pid=%u name=%s player_pid=%u how=%s",
					ch->GetPlayerID(), ch->GetName(), p.dwAskedHumanPid,
					p.bAskedHow == BOT_HUMAN_ASK_REQUEST ? "request" : "invite");
		}
		p.dwAskedHumanPid = 0;
		p.dwAskedHumanAt = 0;
		p.bAskedHow = BOT_HUMAN_ASK_NONE;
	}

	// The companion's phase, asked by the party pass. Returns whether
	// Iwakura's rules decide this bot's parties. A party that has just ended -
	// whoever ended it - is followed by a few minutes alone and then a fresh
	// draw; a draw that has held its 45 to 90 minutes with the bot alone is
	// drawn again; in a party the draw stands still.
	bool UpdatePlayerBotCompanionPhase(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return false;
		TPlayerBotPersona& p = state.persona;
		ResolvePlayerBotHumanAsk(ch, state, dwNow);
		const bool inParty = ch->GetParty() != NULL;
		if (p.bWasInParty && !inParty)
		{
			p.dwCompanionBreakUntil = dwNow + number((int)playerbot_persona::COMPANION_BREAK_MIN_MS,
					(int)playerbot_persona::COMPANION_BREAK_MAX_MS);
			p.dwCompanionPhaseEnd = p.dwCompanionBreakUntil;
		}
		p.bWasInParty = inParty;
		if (!inParty && (p.wCompanionDraw == playerbot_persona::COMPANION_DRAW_NONE ||
				p.dwCompanionPhaseEnd == 0 || dwNow >= p.dwCompanionPhaseEnd))
		{
			const bool shaman = ch->GetJob() == JOB_SHAMAN;
			const bool character = p.bDrawnPersonality == BOT_PERSONALITY_TEAM_COMPANION;
			const bool fighter = state.bBotRole == BOT_ROLE_PARTY_FIGHTER;
			p.wCompanionDraw = playerbot_persona::CompanionDraw((uint32_t)number(0, 0x7fffffff),
					shaman, character, fighter);
			p.dwCompanionPhaseEnd = dwNow + number((int)playerbot_persona::COMPANION_PHASE_MIN_MS,
					(int)playerbot_persona::COMPANION_PHASE_MAX_MS);
			p.dwCompanionBreakUntil = 0;
		}
		return true;
	}

	// A companion Shaman keeps its party's buffs up ("regularnie i bez
	// opoznien rzuca na czlonkow grupy potezne zaklecia wspierajace"): the
	// leader pass for a player's party (ManagePlayerBotBuffHumanLeader)
	// pointed at every member on the map, the persons first, then the leader,
	// then the nearest. One cast a pass; a member out of the skill's reach is
	// passed over rather than walked to - the formation keeps a party within
	// it, and a Shaman walking between members is a Shaman not casting.
	bool ManagePlayerBotBuffCompanions(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotCompanionBuffNext;
		if (!ch || ch->IsDead() || ch->GetJob() != JOB_SHAMAN || ch->GetSkillGroup() == 0 ||
				!IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return false;
		LPPARTY party = ch->GetParty();
		if (!party)
			return false;
		DWORD& next = s_mapPlayerBotCompanionBuffNext[ch->GetPlayerID()];
		if (dwNow < next)
			return false;
		next = dwNow + PLAYERBOT_COMPANION_BUFF_INTERVAL;
		if (state.bRecoveringAfterDeath || state.bTacticalRetreat || state.bMultiPullActive ||
				state.bFishingSession || state.bVisitingShop || ch->GetMyShop())
			return false;

		struct FCollectMembers
		{
			LPCHARACTER me;
			std::vector<LPCHARACTER> members;
			FCollectMembers(LPCHARACTER ch) : me(ch) {}
			void operator()(LPCHARACTER member)
			{
				if (member && member != me && !member->IsDead())
					members.push_back(member);
			}
		};
		FCollectMembers collect(ch);
		party->ForEachOnMapMember(collect, ch->GetMapIndex());
		if (collect.members.empty())
			return false;
		LPCHARACTER leader = party->GetLeaderCharacter();
		struct FMemberOrder
		{
			LPCHARACTER me;
			LPCHARACTER leader;
			int Rank(LPCHARACTER m) const
			{
				const bool person = !m->GetDesc() || !m->GetDesc()->IsBot();
				return person ? 0 : (m == leader ? 1 : 2);
			}
			bool operator()(LPCHARACTER a, LPCHARACTER b) const
			{
				const int ra = Rank(a), rb = Rank(b);
				if (ra != rb)
					return ra < rb;
				return DISTANCE_APPROX(me->GetX() - a->GetX(), me->GetY() - a->GetY()) <
						DISTANCE_APPROX(me->GetX() - b->GetX(), me->GetY() - b->GetY());
			}
		};
		FMemberOrder order;
		order.me = ch;
		order.leader = leader;
		std::sort(collect.members.begin(), collect.members.end(), order);

		const bool fighting = ch->GetVictim() && !ch->GetVictim()->IsDead();
		const bool hunting = fighting || state.dwTargetVID != 0 ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < PLAYERBOT_BUFF_COMBAT_WINDOW);
		LPCHARACTER target = NULL;
		DWORD vnum = 0;
		const int done = CastPlayerBotSupportBuff(ch, state, dwNow, collect.members, hunting,
				"party_buff", target, vnum);
		if (done == 0)
			return false;
		next = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
		if (done == 2)
			PlayerBotLogThrottled("companion_buff", dwNow,
					"PLAYERBOT_PARTY: buffed a member pid=%u name=%s member=%s person=%d vnum=%u",
					ch->GetPlayerID(), ch->GetName(), target->GetName(),
					(!target->GetDesc() || !target->GetDesc()->IsBot()) ? 1 : 0, vnum);
		return true;
	}
}

#endif
