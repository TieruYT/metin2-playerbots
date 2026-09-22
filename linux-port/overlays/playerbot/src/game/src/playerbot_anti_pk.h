#ifndef __INC_METIN2_PLAYERBOT_ANTI_PK_H__
#define __INC_METIN2_PLAYERBOT_ANTI_PK_H__

// Iwakura's Anti-PK protocol and the stone hunter's quarrel ("SYSTEM
// OSOBOWOSCI v2.0", 19 September), under the PERSONA switch.
//
// A bot struck by a player - of an enemy kingdom, or of its own in free mode,
// which is what the engine's CanAttack lets through - drops what it is doing
// and fights back to the end ("walczy do samego konca"), with its skills, its
// potions and its herbalist's brews; its party answers for it ("cala grupa
// rzuca sie na agresora"). One death at a player's hands is an incident: the
// bot comes back and takes the same fight again. The fifth inside fifteen
// minutes on the same ground is harassment, and the bot capitulates: it stops
// fighting players, its mood is locked at SLABY for forty-five minutes, it
// gives the ground up for as long, and one time in twelve - outside a party,
// from level thirty - it goes fishing for an hour instead.
//
// The stone hunter (Pogromca) is the other half. A stone in sight within ten
// levels either way claims the bot even in the middle of a pack; three bots of
// its own kingdom on one are enough and it goes back to what it was doing;
// anybody of another kingdom breaking it is a rival, fought to drive off the
// stone; below 35% health with the stone's monsters on it the bot turns on
// them and comes back when they are gone; and a stone that has killed it more
// than six times is given up.
//
// Who struck a bot is the one thing the engine does not keep: CHARACTER::Damage
// tells the manager on mt2009 (CPlayerBotManager::OnPlayerStruck, playerbotify
// apply_player_struck). r40250 has no such call, so there the protocol never
// hears of a blow - its fight back and its party's never start; the stone
// hunter's rules and the capitulation's deaths (none counted as a player's
// there, see WasPlayerBotKilledByPlayer) are all it has.
//
// An implementation fragment in the sense playerbot_types.h describes. Include
// it exactly once, after playerbot_guild_war.h: the fight is the war's shape
// and its life pass (KeepPlayerBotAliveAtWar) is the war's own.

namespace
{
	// Defined in playerbot_manager.cpp beside the duel it was written for.
	const char* GetPlayerBotDuelUnreadiness(LPCHARACTER ch, DWORD dwNow);

	const char* GetPlayerBotFoeReasonName(BYTE reason)
	{
		switch (reason)
		{
			case BOT_FOE_STRUCK: return "struck";
			case BOT_FOE_PARTY: return "party";
			case BOT_FOE_GRUDGE: return "grudge";
			case BOT_FOE_STONE_RIVAL: return "stone_rival";
			case BOT_FOE_GUILD: return "guild";
			default: return "none";
		}
	}

	// A person in a party with bots, struck by another player: the one strike
	// the bots' own state cannot hold, kept by pid for the party's answer.
	struct TPlayerBotHumanStruck
	{
		DWORD dwAttackerVID;
		DWORD dwAttackerPID;
		DWORD dwAt;
	};
	std::map<DWORD, TPlayerBotHumanStruck> s_mapPlayerBotHumanStruck;

	// A guild's call to arms (community patch 2, point 15): the person who
	// last struck one of its bots, keyed by guild id. Only a person's blow
	// opens one. A bot's own blows at a player are all answers or rivalries
	// (the stone, the grudge, a defence), and a guild that answered those
	// would draw the other guild's answer to its own defenders - two guilds of
	// twenty on one map fighting because a stone was contested.
	struct TPlayerBotGuildCall
	{
		DWORD dwAttackerVID;
		DWORD dwAttackerPID;
		DWORD dwVictimPID;
		DWORD dwAt;
		long lMapIndex;
	};
	std::map<DWORD, TPlayerBotGuildCall> s_mapPlayerBotGuildCall;

	// The player who last killed a bot, and until when the bot comes back for
	// him: Step 2's "ponownie probuje go przejac (po raz kolejny wdaje sie w
	// walke z tym samym oprawca)".
	struct TPlayerBotGrudge
	{
		DWORD dwKillerPID;
		DWORD dwUntil;
	};
	std::map<DWORD, TPlayerBotGrudge> s_mapPlayerBotGrudge;
	const DWORD PLAYERBOT_ANTIPK_GRUDGE_MS = 10 * 60 * 1000;

	// Whether a blow between these two is one the protocol has any business
	// with: not an agreed duel (CPVPManager holds the pair; a bot's own duel
	// pass fights it), not a guild war between their guilds.
	bool IsPlayerBotBlowConsensual(LPCHARACTER victim, LPCHARACTER attacker)
	{
		if (!victim || !attacker)
			return true;
		CPVP key(victim->GetPlayerID(), attacker->GetPlayerID());
		if (CPVPManager::instance().Find(key.GetCRC()) != NULL)
			return true;
		CGuild* mine = victim->GetGuild();
		CGuild* theirs = attacker->GetGuild();
		return mine && theirs && mine != theirs && mine->UnderWar(theirs->GetID());
	}

	// CHARACTER::Damage, through the manager: a player's blow at a bot, or at a
	// person who is in a party (a party with bots in it answers for its person).
	void NotePlayerBotStruck(LPCHARACTER victim, LPCHARACTER attacker, DWORD dwNow)
	{
		if (!victim || !attacker || victim == attacker || !attacker->IsPC() ||
				IsPlayerBotBlowConsensual(victim, attacker))
			return;
		// A bot's blow counts only when it was meant: the bot is aiming at this
		// character, or fighting it. A splash skill lands on every attackable
		// thing in its radius (FuncSplashDamage asks battle_is_attackable and
		// nothing else), and on the shared maps that is the other kingdoms' bots
		// hunting beside it - the first hour of this protocol answered every
		// such graze, and the answer drew the next: 258 blows in four minutes on
		// maps 65, 67 and 68, bots of three kingdoms at war with nobody having
		// meant it. A person's blow is taken as meant: there is no telling.
		TPlayerBotAIStateMap::const_iterator attackerState =
				s_mapPlayerBotAIStates.find(attacker->GetPlayerID());
		if (attackerState != s_mapPlayerBotAIStates.end() &&
				attackerState->second.dwTargetVID != (DWORD)victim->GetVID() &&
				attackerState->second.persona.dwFoeVID != (DWORD)victim->GetVID())
			return;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(victim->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
		{
			if (!victim->GetParty())
				return;
			// Only the last few seconds matter; the rest goes before the map grows.
			if (s_mapPlayerBotHumanStruck.size() >= 256)
				for (std::map<DWORD, TPlayerBotHumanStruck>::iterator old = s_mapPlayerBotHumanStruck.begin();
						old != s_mapPlayerBotHumanStruck.end(); )
				{
					if (dwNow - old->second.dwAt >= PLAYERBOT_ANTIPK_PARTY_MEMORY_MS)
						s_mapPlayerBotHumanStruck.erase(old++);
					else
						++old;
				}
			TPlayerBotHumanStruck& struck = s_mapPlayerBotHumanStruck[victim->GetPlayerID()];
			struck.dwAttackerVID = attacker->GetVID();
			struck.dwAttackerPID = attacker->GetPlayerID();
			struck.dwAt = dwNow;
			return;
		}
		TPlayerBotPersona& p = it->second.persona;
		// The guild's half: a person's blow at a bot of a guild, not a
		// guild-mate's (free mode lets one strike his own guild, and the guild
		// does not go to war with itself).
		CGuild* guild = victim->GetGuild();
		if (guild && attacker->GetDesc() && !attacker->GetDesc()->IsBot() &&
				attacker->GetGuild() != guild)
		{
			TPlayerBotGuildCall& call = s_mapPlayerBotGuildCall[guild->GetID()];
			const bool freshCall = call.dwAttackerPID != attacker->GetPlayerID() ||
					dwNow - call.dwAt >= PLAYERBOT_ANTIPK_GUILD_MEMORY_MS;
			call.dwAttackerVID = attacker->GetVID();
			call.dwAttackerPID = attacker->GetPlayerID();
			call.dwVictimPID = victim->GetPlayerID();
			call.dwAt = dwNow;
			call.lMapIndex = victim->GetMapIndex();
			if (freshCall && IsPlayerBotPersonaEnabled())
				sys_log(0, "PLAYERBOT_ANTIPK: guild called pid=%u name=%s guild=%u by_pid=%u by=%s by_level=%u map=%ld",
						victim->GetPlayerID(), victim->GetName(), guild->GetID(),
						attacker->GetPlayerID(), attacker->GetName(),
						(unsigned int)attacker->GetLevel(), victim->GetMapIndex());
		}
		const bool fresh = p.dwStruckByPID != attacker->GetPlayerID() ||
				dwNow - p.dwStruckAt >= PLAYERBOT_ANTIPK_STRUCK_MEMORY_MS;
		p.dwStruckByVID = attacker->GetVID();
		p.dwStruckByPID = attacker->GetPlayerID();
		p.dwStruckAt = dwNow;
		if (fresh && IsPlayerBotPersonaEnabled())
			sys_log(0, "PLAYERBOT_ANTIPK: struck pid=%u name=%s level=%u empire=%u by_pid=%u by=%s by_level=%u by_empire=%u person=%d map=%ld",
					victim->GetPlayerID(), victim->GetName(), (unsigned int)victim->GetLevel(),
					(unsigned int)victim->GetEmpire(), attacker->GetPlayerID(), attacker->GetName(),
					(unsigned int)attacker->GetLevel(), (unsigned int)attacker->GetEmpire(),
					attacker->GetDesc() && !attacker->GetDesc()->IsBot() ? 1 : 0, victim->GetMapIndex());
	}

	// A foe this bot can fight now: standing, on its map, in reach, out of the
	// safe zones - the engine refuses every blow there - not invisible after
	// its own death, and one the engine lets it strike.
	bool IsPlayerBotFoeFightable(LPCHARACTER ch, LPCHARACTER foe,
			int range = PLAYERBOT_ANTIPK_FOE_RANGE)
	{
		if (!ch || !foe || foe == ch || !foe->IsPC() || foe->IsDead() ||
				foe->GetMapIndex() != ch->GetMapIndex() || IsPlayerBotWarFoeRecovering(foe))
			return false;
		if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(foe->GetMapIndex(), foe->GetX(), foe->GetY()))
			return false;
		if (DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY()) > range)
			return false;
		return CanPlayerBotStrikeCharacter(ch, foe);
	}

	LPCHARACTER BeginPlayerBotFoe(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER foe,
			BYTE reason, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		p.dwFoeVID = foe->GetVID();
		p.bFoeReason = reason;
		p.dwFoeSince = dwNow;
		sys_log(0, "PLAYERBOT_ANTIPK: fights pid=%u name=%s level=%u foe_pid=%u foe=%s foe_level=%u person=%d reason=%s hp=%d/%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), foe->GetPlayerID(),
				foe->GetName(), (unsigned int)foe->GetLevel(),
				foe->GetDesc() && !foe->GetDesc()->IsBot() ? 1 : 0, GetPlayerBotFoeReasonName(reason),
				ch->GetHP(), ch->GetMaxHP(), ch->GetMapIndex());
		return foe;
	}

	// A member of this bot's party struck by a player this recently, and near:
	// the attacker is the whole party's.
	LPCHARACTER FindPlayerBotPartyAggressor(LPCHARACTER ch, DWORD dwNow)
	{
		LPPARTY party = ch ? ch->GetParty() : NULL;
		if (!party)
			return NULL;
		struct FFindStruckMember
		{
			LPCHARACTER m_ch;
			DWORD m_now;
			LPCHARACTER m_found;
			FFindStruckMember(LPCHARACTER ch, DWORD now) : m_ch(ch), m_now(now), m_found(NULL) {}
			void operator () (LPCHARACTER member)
			{
				if (m_found || !member || member == m_ch ||
						DISTANCE_APPROX(member->GetX() - m_ch->GetX(), member->GetY() - m_ch->GetY()) >
							PLAYERBOT_ANTIPK_PARTY_RANGE)
					return;
				DWORD vid = 0, pid = 0, at = 0;
				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(member->GetPlayerID());
				if (it != s_mapPlayerBotAIStates.end())
				{
					vid = it->second.persona.dwStruckByVID;
					pid = it->second.persona.dwStruckByPID;
					at = it->second.persona.dwStruckAt;
				}
				else
				{
					std::map<DWORD, TPlayerBotHumanStruck>::const_iterator human =
							s_mapPlayerBotHumanStruck.find(member->GetPlayerID());
					if (human == s_mapPlayerBotHumanStruck.end())
						return;
					vid = human->second.dwAttackerVID;
					pid = human->second.dwAttackerPID;
					at = human->second.dwAt;
				}
				if (at == 0 || m_now - at >= PLAYERBOT_ANTIPK_PARTY_MEMORY_MS)
					return;
				LPCHARACTER attacker = CHARACTER_MANAGER::instance().Find(vid);
				if (attacker && attacker->GetPlayerID() == pid && IsPlayerBotFoeFightable(m_ch, attacker))
					m_found = attacker;
			}
		};
		FFindStruckMember finder(ch, dwNow);
		party->ForEachOnMapMember(finder, ch->GetMapIndex());
		return finder.m_found;
	}

	// A person who has just struck a bot of this bot's guild, on this map and
	// within PLAYERBOT_ANTIPK_GUILD_RANGE of it. A bot in a person's party is
	// that person's to lead, and its own party answers for it anyway.
	LPCHARACTER FindPlayerBotGuildAggressor(LPCHARACTER ch, DWORD dwNow)
	{
		CGuild* guild = ch ? ch->GetGuild() : NULL;
		if (!guild || (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())))
			return NULL;
		std::map<DWORD, TPlayerBotGuildCall>::const_iterator call =
				s_mapPlayerBotGuildCall.find(guild->GetID());
		if (call == s_mapPlayerBotGuildCall.end() || call->second.lMapIndex != ch->GetMapIndex() ||
				call->second.dwVictimPID == ch->GetPlayerID() ||
				dwNow - call->second.dwAt >= PLAYERBOT_ANTIPK_GUILD_MEMORY_MS)
			return NULL;
		LPCHARACTER attacker = CHARACTER_MANAGER::instance().Find(call->second.dwAttackerVID);
		if (!attacker || attacker->GetPlayerID() != call->second.dwAttackerPID ||
				attacker->GetGuild() == guild ||
				!IsPlayerBotFoeFightable(ch, attacker, PLAYERBOT_ANTIPK_GUILD_RANGE))
			return NULL;
		return attacker;
	}

	// Who this bot fights now, if anybody. The foe in hand first - to the end,
	// or for a stone's rival until it has left the stone - then the player who
	// has just struck it, one who has struck its party, one who killed it and
	// is still about, and a rival at its stone.
	LPCHARACTER PickPlayerBotPersonaFoe(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		// Capitulated: it avoids the fight for the lock's length ("stara sie
		// uniknac walki").
		if (p.dwCapitulatedUntil != 0 && dwNow < p.dwCapitulatedUntil)
		{
			p.dwFoeVID = 0;
			p.bFoeReason = BOT_FOE_NONE;
			return NULL;
		}
		if (p.dwFoeVID != 0)
		{
			LPCHARACTER held = CHARACTER_MANAGER::instance().Find(p.dwFoeVID);
			// A guild's aggressor is held from as far as the call reached.
			bool keep = IsPlayerBotFoeFightable(ch, held, p.bFoeReason == BOT_FOE_GUILD
					? PLAYERBOT_ANTIPK_GUILD_RANGE : PLAYERBOT_ANTIPK_FOE_RANGE);
			const char* why = keep ? "" : (!held || held->IsDead() ? "foe_down" : "out_of_reach");
			if (keep && p.bFoeReason == BOT_FOE_STONE_RIVAL)
			{
				// Driven off the stone is what the fight was for.
				LPCHARACTER stone = p.dwPogromcaStoneVID != 0
						? CHARACTER_MANAGER::instance().Find(p.dwPogromcaStoneVID) : NULL;
				keep = stone && stone->IsStone() && !stone->IsDead() &&
						DISTANCE_APPROX(held->GetX() - stone->GetX(), held->GetY() - stone->GetY()) <=
							PLAYERBOT_STONE_SUPPORT_RANGE;
				if (!keep)
					why = "left_the_stone";
			}
			if (keep)
				return held;
			sys_log(0, "PLAYERBOT_ANTIPK: foe let go pid=%u name=%s reason=%s why=%s fought_s=%u hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), GetPlayerBotFoeReasonName(p.bFoeReason), why,
					(unsigned int)((dwNow - p.dwFoeSince) / 1000), ch->GetHP(), ch->GetMaxHP());
			p.dwFoeVID = 0;
			p.bFoeReason = BOT_FOE_NONE;
		}
		if (p.dwStruckAt != 0 && dwNow - p.dwStruckAt < PLAYERBOT_ANTIPK_STRUCK_MEMORY_MS)
		{
			LPCHARACTER attacker = CHARACTER_MANAGER::instance().Find(p.dwStruckByVID);
			if (attacker && attacker->GetPlayerID() == p.dwStruckByPID &&
					IsPlayerBotFoeFightable(ch, attacker))
				return BeginPlayerBotFoe(ch, state, attacker, BOT_FOE_STRUCK, dwNow);
		}
		if (LPCHARACTER aggressor = FindPlayerBotPartyAggressor(ch, dwNow))
			return BeginPlayerBotFoe(ch, state, aggressor, BOT_FOE_PARTY, dwNow);
		if (LPCHARACTER aggressor = FindPlayerBotGuildAggressor(ch, dwNow))
			return BeginPlayerBotFoe(ch, state, aggressor, BOT_FOE_GUILD, dwNow);
		std::map<DWORD, TPlayerBotGrudge>::iterator grudge = s_mapPlayerBotGrudge.find(ch->GetPlayerID());
		if (grudge != s_mapPlayerBotGrudge.end())
		{
			if (dwNow >= grudge->second.dwUntil)
				s_mapPlayerBotGrudge.erase(grudge);
			else if (!state.bRecoveringAfterDeath)
			{
				LPCHARACTER killer = CHARACTER_MANAGER::instance().FindByPID(grudge->second.dwKillerPID);
				if (killer && IsPlayerBotFoeFightable(ch, killer))
					return BeginPlayerBotFoe(ch, state, killer, BOT_FOE_GRUDGE, dwNow);
			}
		}
		// A rival at the bot's stone, looked for on a clock and only while
		// the bot is breaking it.
		if (p.dwPogromcaStoneVID != 0 && !ch->GetParty() && dwNow >= p.dwNextRivalScan &&
				state.dwTargetVID == p.dwPogromcaStoneVID)
		{
			p.dwNextRivalScan = dwNow + PLAYERBOT_POGROMCA_RIVAL_SCAN_MS;
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(p.dwPogromcaStoneVID);
			if (stone && stone->IsStone() && !stone->IsDead() &&
					DISTANCE_APPROX(ch->GetX() - stone->GetX(), ch->GetY() - stone->GetY()) <=
						PLAYERBOT_STONE_SUPPORT_RANGE)
			{
				LPCHARACTER rival = FindPlayerBotStoneRival(ch, stone);
				if (rival && IsPlayerBotFoeFightable(ch, rival))
					return BeginPlayerBotFoe(ch, state, rival, BOT_FOE_STONE_RIVAL, dwNow);
			}
		}
		return NULL;
	}

	// The fight, at the top of the tick like a duel or a war: it is the thing
	// the bot is doing now ("natychmiast przerywa swoje dotychczasowe zajecie").
	// Life first - the recovery after a death, the potions, a brew - then the
	// fight a duel is: the aura, a caster from its range, a warrior across the
	// gap, a blade from where it reaches.
	bool ManagePlayerBotPersonaFoe(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return false;
		// Not with a rod or a pickaxe in the hand, nor with nothing in it: the
		// session that holds the tool ends at a blow, and the weapon is back
		// on a tick or two later.
		if (GetPlayerBotDuelUnreadiness(ch, dwNow) != NULL)
			return false;
		LPCHARACTER foe = PickPlayerBotPersonaFoe(ch, state, dwNow);
		if (!foe)
			return false;
		if (KeepPlayerBotAliveAtWar(ch, state, dwNow))
			return true;
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "anti_pk");
		DrinkPlayerBotCraftedPotion(ch, foe, dwNow, true);

		state.dwTargetVID = (DWORD)foe->GetVID();
		ch->SetVictim(foe);
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		ch->SetRotationToXY(foe->GetX(), foe->GetY());
		state.dwLastMeaningfulActivityTime = dwNow;

		const int distance = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		if (distance <= PLAYERBOT_DUEL_BUFF_RANGE && ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
			return true;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW);
		const int combatRange = isBow ? 800 : PLAYERBOT_DUEL_MELEE_RANGE;
		const bool caster = ch->GetJob() == JOB_SHAMAN ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2);
		if (distance > combatRange)
		{
			if (!isBow && caster && distance <= PLAYERBOT_DUEL_CASTER_RANGE &&
					dwNow >= state.dwNextSkillCastTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				if (CastPlayerBotDuelSkill(ch, foe, state, dwNow))
					return true;
			}
			if (!isBow && TryPlayerBotDuelGapCloser(ch, foe, state, dwNow, distance))
				return true;
			MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 4, false, false);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_FIGHTING);
		if (!CastPlayerBotDuelSkill(ch, foe, state, dwNow))
			ExecutePlayerBotBasicAttack(ch, foe, state, dwNow);
		return true;
	}

	// A death, as the protocol and the stone hunter count it (from
	// NotePlayerBotPersonaDeath, which HandleDeath calls on the tick it
	// happened). byPlayer is the engine's own count on mt2009.
	void NotePlayerBotAntiPkDeath(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, bool byPlayer)
	{
		TPlayerBotPersona& p = state.persona;
		// The stone that killed it: more than six deaths and it is given up,
		// by monsters or by rivals alike.
		if (p.dwPogromcaStoneVID != 0)
		{
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(p.dwPogromcaStoneVID);
			if (!stone || !stone->IsStone() || stone->IsDead())
			{
				p.dwPogromcaStoneVID = 0;
				p.bPogromcaDeaths = 0;
			}
			else if (DISTANCE_APPROX(ch->GetX() - stone->GetX(), ch->GetY() - stone->GetY()) <=
					PLAYERBOT_STONE_SUPPORT_RANGE * 2)
			{
				if (p.bPogromcaDeaths < 255)
					++p.bPogromcaDeaths;
				if (playerbot_persona::PogromcaGivesUp(p.bPogromcaDeaths))
				{
					state.mapFailedStones[p.dwPogromcaStoneVID] = dwNow + PLAYERBOT_POGROMCA_GIVE_UP_MS;
					sys_log(0, "PLAYERBOT_PERSONA: pogromca gives the stone up pid=%u name=%s level=%u stone=%s stone_level=%u deaths=%u",
							ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), stone->GetName(),
							(unsigned int)stone->GetLevel(), (unsigned int)p.bPogromcaDeaths);
					if (state.dwTargetVID == p.dwPogromcaStoneVID)
						state.dwTargetVID = 0;
					p.dwPogromcaStoneVID = 0;
					p.bPogromcaDeaths = 0;
				}
			}
		}
		if (!byPlayer)
			return;
		// Who: the player whose blow the engine last reported, if it was recent.
		// None means the blows that killed were not the protocol's business - a
		// duel the bot agreed to, a guild war, another bot's splash that grazed
		// it (NotePlayerBotStruck keeps none of those) - and such a death is
		// no harassment to capitulate over.
		const DWORD killerPid = (p.dwStruckAt != 0 && dwNow - p.dwStruckAt < PLAYERBOT_ANTIPK_STRUCK_MEMORY_MS)
				? p.dwStruckByPID : 0;
		if (killerPid == 0)
			return;
		p.dwFoeVID = 0;
		p.bFoeReason = BOT_FOE_NONE;
		if (!playerbot_persona::NotePkDeath(p.pkDeaths, dwNow, ch->GetMapIndex(), ch->GetX(), ch->GetY()))
		{
			// An incident: it comes back for the same player.
			TPlayerBotGrudge& grudge = s_mapPlayerBotGrudge[ch->GetPlayerID()];
			grudge.dwKillerPID = killerPid;
			grudge.dwUntil = dwNow + PLAYERBOT_ANTIPK_GRUDGE_MS;
			sys_log(0, "PLAYERBOT_ANTIPK: killed by a player pid=%u name=%s level=%u killer_pid=%u deaths_here=%u map=%ld",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), killerPid,
					p.pkDeaths.count, ch->GetMapIndex());
			return;
		}
		// The fifth: capitulation.
		s_mapPlayerBotGrudge.erase(ch->GetPlayerID());
		p.dwCapitulatedUntil = dwNow + playerbot_persona::PK_CAPITULATION_MS;
		p.lAvoidSpotMap = ch->GetMapIndex();
		p.lAvoidSpotX = ch->GetX();
		p.lAvoidSpotY = ch->GetY();
		p.dwAvoidSpotUntil = dwNow + playerbot_persona::PK_CAPITULATION_MS;
		const bool locked = playerbot_persona::OnCapitulation(p.mood);
		const bool fishing = playerbot_persona::RollCapitulationFishing((uint32_t)number(0, 999),
				(int)ch->GetLevel(), ch->GetParty() != NULL);
		if (fishing)
			p.dwFishingSpellUntil = dwNow + playerbot_persona::PK_FISHING_MS;
		p.bDirty = true;
		// Off the ground it gave up: the next hunt is chosen elsewhere.
		state.dwTargetVID = 0;
		state.dwNextWanderTime = dwNow;
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_ANTIPK: capitulates pid=%u name=%s level=%u mood_locked=%d fishing=%d spot=(%ld,%ld) map=%ld minutes=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), locked ? 1 : 0,
				fishing ? 1 : 0, p.lAvoidSpotX, p.lAvoidSpotY, p.lAvoidSpotMap,
				(unsigned int)(playerbot_persona::PK_CAPITULATION_MS / 60000));
	}

	// The stone hunter's look round: the nearest stone the known-Metin registry
	// holds on this map, in sight, in the band, not given up, one the bot can
	// break and its kingdom has not crowded, and on ground it can walk to.
	LPCHARACTER FindPlayerBotPogromcaStone(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		LPCHARACTER best = NULL;
		int bestDistance = INT_MAX;
		for (TKnownPlayerBotMetinMap::const_iterator it = s_mapKnownPlayerBotMetins.begin();
				it != s_mapKnownPlayerBotMetins.end(); ++it)
		{
			const TKnownPlayerBotMetin& known = it->second;
			if (known.lMapIndex != ch->GetMapIndex())
				continue;
			const int distance = DISTANCE_APPROX(ch->GetX() - known.lX, ch->GetY() - known.lY);
			if (distance > PLAYERBOT_SEARCH_RANGE || distance >= bestDistance)
				continue;
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(it->first);
			if (!stone || !stone->IsStone() || stone->IsDead() ||
					stone->GetMapIndex() != ch->GetMapIndex() ||
					IsPlayerBotSafeZone(stone->GetMapIndex(), stone->GetX(), stone->GetY()))
				continue;
			std::map<DWORD, DWORD>::const_iterator failed = state.mapFailedStones.find(it->first);
			if (failed != state.mapFailedStones.end() && dwNow < failed->second)
				continue;
			if (IsPlayerBotAvoidedSpot(state, stone->GetMapIndex(), stone->GetX(), stone->GetY(), dwNow))
				continue;
			if (!IsPlayerBotMetinWorthFighting(ch, stone) || !CanPlayerBotEngageStone(ch, stone) ||
					IsTargetClaimedByAnotherBot(ch, stone->GetVID()) ||
					!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), stone->GetX(), stone->GetY()))
				continue;
			best = stone;
			bestDistance = distance;
		}
		return best;
	}

	// The stone hunter in the target section, before the held target is
	// judged: which stone the bot is breaking, the turn on the pack below 35%,
	// and every PLAYERBOT_POGROMCA_PROBE_MS a look round for a stone that
	// takes the bot off whatever monster it has ("rzuca wszystko"). A party is
	// the party's to aim, and a duel foe is the duel's.
	void ManagePlayerBotPogromcaTarget(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER& target,
			DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !state.persona.bRestored)
			return;
		TPlayerBotPersona& p = state.persona;
		if (p.dwPogromcaStoneVID != 0)
		{
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(p.dwPogromcaStoneVID);
			if (!stone || !stone->IsStone() || stone->IsDead())
			{
				p.dwPogromcaStoneVID = 0;
				p.bPogromcaDeaths = 0;
				p.bPogromcaClearing = false;
			}
		}
		if (ch->GetParty() || IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return;
		const bool onStone = target && target->IsStone() && !target->IsDead();
		if (onStone && (DWORD)target->GetVID() != p.dwPogromcaStoneVID)
		{
			p.dwPogromcaStoneVID = target->GetVID();
			p.bPogromcaDeaths = 0;
		}
		const bool retreat = playerbot_persona::IsPogromcaRetreat(ch->GetHP(), ch->GetMaxHP());
		if (onStone && retreat)
		{
			LPCHARACTER pack = FindPlayerBotEngagedTarget(ch, &state, dwNow);
			if (pack && pack->IsMonster())
			{
				if (!p.bPogromcaClearing)
					PlayerBotLogThrottled("pogromca_clears", dwNow,
							"PLAYERBOT_PERSONA: pogromca turns on the pack pid=%u name=%s hp=%d/%d stone=%s pack=%s",
							ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP(),
							target->GetName(), pack->GetName());
				p.bPogromcaClearing = true;
				target = pack;
				state.dwTargetVID = (DWORD)pack->GetVID();
				ClearPlayerBotRoute(state, true);
			}
			return;
		}
		if (p.bPogromcaClearing)
		{
			// While anything of the pack is on the bot, the pack.
			if (target && target->IsMonster() && !target->IsDead() && target->GetVictim() == ch)
				return;
			LPCHARACTER pack = FindPlayerBotEngagedTarget(ch, &state, dwNow);
			if (pack && pack->IsMonster())
			{
				target = pack;
				state.dwTargetVID = (DWORD)pack->GetVID();
				return;
			}
			p.bPogromcaClearing = false;
			p.dwNextStoneProbe = 0;
		}
		if (onStone || dwNow < p.dwNextStoneProbe)
			return;
		p.dwNextStoneProbe = dwNow + PLAYERBOT_POGROMCA_PROBE_MS;
		if (retreat)
			return;
		LPCHARACTER stone = FindPlayerBotPogromcaStone(ch, state, dwNow);
		if (!stone || stone == target)
			return;
		PlayerBotLogThrottled("pogromca_rush", dwNow,
				"PLAYERBOT_PERSONA: pogromca drops everything for a stone pid=%u name=%s level=%u stone=%s stone_level=%u was=%s dist=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), stone->GetName(),
				(unsigned int)stone->GetLevel(), target ? target->GetName() : "-",
				(int)DISTANCE_APPROX(ch->GetX() - stone->GetX(), ch->GetY() - stone->GetY()));
		target = stone;
		state.dwTargetVID = (DWORD)stone->GetVID();
		p.dwPogromcaStoneVID = stone->GetVID();
		p.bPogromcaDeaths = 0;
		ReservePlayerBotMetin(ch, stone, dwNow);
		ClearPlayerBotRoute(state, true);
	}
}

#endif
