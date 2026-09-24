#ifndef __INC_METIN2_PLAYERBOT_TARGETING_H__
#define __INC_METIN2_PLAYERBOT_TARGETING_H__

// Choosing what to hit, and hitting it.
//
// The hard part is not finding a monster - it is that several hundred bots are
// looking at the same field. A crowd that all picks the highest-scoring target
// looks nothing like a populated world, so a candidate is scored, claimed
// against the other bots, and abandoned when someone else got there first.
// That claim, and the stone-attacker count that decides when a metin is a lost
// cause, are the reason this file is one piece rather than several: every part
// of it reads the same shared view of who is fighting what.
//
// Multi-pulling lives here for the same reason. It is not a separate mode but a
// different answer to the same question, taken by builds that can survive it.
//
// playerbot_combat.h is the layer below - it knows how to send a swing. This
// one decides whether to.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	struct TPlayerBotPartyStrength
	{
		TPlayerBotPartyStrength() : iReadyMembers(0), iTotalLevels(0), iHighestLevel(0), iChallengeMaxLevel(0) {}

		int iReadyMembers;
		int iTotalLevels;
		int iHighestLevel;
		int iChallengeMaxLevel;
	};

	class FCollectPlayerBotPartyStrength
	{
		public:
			FCollectPlayerBotPartyStrength(LPCHARACTER anchor, DWORD dwNow, TPlayerBotPartyStrength& strength) :
				m_anchor(anchor), m_dwNow(dwNow), m_strength(strength)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || member->IsDead() || member->GetMapIndex() != m_anchor->GetMapIndex() ||
						!member->GetDesc() || !member->GetDesc()->IsBot())
					return;

				if (DISTANCE_APPROX(member->GetX() - m_anchor->GetX(), member->GetY() - m_anchor->GetY()) >
						PLAYERBOT_PARTY_CHALLENGE_RADIUS)
					return;

				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(member->GetPlayerID());
				if (it == s_mapPlayerBotAIStates.end() || it->second.bVisitingShop ||
						it->second.bRecoveringAfterDeath)
					return;

				if (it->second.dwLastDeathTime != 0 && m_dwNow - it->second.dwLastDeathTime < 60000)
					return;

				if (member->GetMaxHP() <= 0 ||
						member->GetHP() * 100 < member->GetMaxHP() * PLAYERBOT_PARTY_READY_HP_PERCENT ||
						member->GetWear(WEAR_WEAPON) == NULL)
					return;

				++m_strength.iReadyMembers;
				m_strength.iTotalLevels += member->GetLevel();
				m_strength.iHighestLevel = std::max(m_strength.iHighestLevel, (int)member->GetLevel());
			}

		private:
			LPCHARACTER m_anchor;
			DWORD m_dwNow;
			TPlayerBotPartyStrength& m_strength;
	};

	bool GetPlayerBotPartyStrength(LPCHARACTER ch, DWORD dwNow, TPlayerBotPartyStrength& strength)
	{
		if (!ch || !ch->GetParty())
			return false;

		FCollectPlayerBotPartyStrength collector(ch, dwNow, strength);
		ch->GetParty()->ForEachOnMapMember(collector, ch->GetMapIndex());

		if (strength.iReadyMembers < PLAYERBOT_PARTY_CHALLENGE_MIN_MEMBERS)
			return false;

		// Two independent limits prevent one strong bot from dragging weak party
		// members into a suicidal fight. Five level-15 bots can challenge level 35,
		// while three such bots remain restricted to normal M1 opponents.
		const int formationLimit = strength.iHighestLevel +
				(strength.iReadyMembers - 1) * PLAYERBOT_PARTY_LEVEL_BONUS_PER_MEMBER;
		const int combinedPowerLimit = strength.iTotalLevels / 2;
		strength.iChallengeMaxLevel = std::min(formationLimit, combinedPowerLimit);
		return strength.iChallengeMaxLevel > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA;
	}

	bool CanPlayerBotPartyChallenge(LPCHARACTER ch, LPCHARACTER target, DWORD dwNow,
			TPlayerBotPartyStrength* outStrength)
	{
		if (!ch || !target || !target->IsMonster() || target->IsDead() ||
				target->GetMapIndex() != ch->GetMapIndex())
			return false;

		TPlayerBotPartyStrength strength;
		if (!GetPlayerBotPartyStrength(ch, dwNow, strength) ||
				target->GetLevel() > strength.iChallengeMaxLevel)
			return false;

		if (outStrength)
			*outStrength = strength;
		return true;
	}

	class FFindPlayerBotPartyFocus
	{
		public:
			FFindPlayerBotPartyFocus(LPCHARACTER owner, DWORD dwNow) :
				m_owner(owner), m_dwNow(dwNow), m_target(NULL), m_bLeaderTarget(false)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || !member->GetDesc() || !member->GetDesc()->IsBot())
					return;

				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(member->GetPlayerID());
				if (it == s_mapPlayerBotAIStates.end() || it->second.dwTargetVID == 0)
					return;

				LPCHARACTER candidate = CHARACTER_MANAGER::instance().Find(it->second.dwTargetVID);
				if (!candidate || (!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead() ||
						candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()) ||
						!IsPlayerBotReachable(m_owner->GetMapIndex(),
								m_owner->GetX(), m_owner->GetY(), candidate->GetX(), candidate->GetY()) ||
						DISTANCE_APPROX(candidate->GetX() - m_owner->GetX(), candidate->GetY() - m_owner->GetY()) > PLAYERBOT_PARTY_COHESION_RADIUS ||
						(candidate->IsStone() &&
						 !IsPlayerBotMetinWorthFighting(m_owner, candidate) &&
						 (int)candidate->GetLevel() > (int)m_owner->GetLevel() + PLAYERBOT_STONE_JOIN_LEVEL_DELTA) ||
						(candidate->IsMonster() &&
							 candidate->GetLevel() > m_owner->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
							 !CanPlayerBotPartyChallenge(m_owner, candidate, m_dwNow, NULL)))
					return;

				const bool bCandidateIsLeaderTarget =
						(m_owner->GetParty() && member == m_owner->GetParty()->GetLeaderCharacter());
				if (!m_target || (bCandidateIsLeaderTarget && !m_bLeaderTarget))
				{
					m_target = candidate;
					m_bLeaderTarget = bCandidateIsLeaderTarget;
				}
			}

			LPCHARACTER GetTarget() const { return m_target; }

		private:
			LPCHARACTER m_owner;
			DWORD m_dwNow;
			LPCHARACTER m_target;
			bool m_bLeaderTarget;
	};

	LPCHARACTER FindPlayerBotPartyFocusTarget(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// The party's shared target is a fresh fight like any other, so the map
		// rule applies to it too: on ground this bot has outgrown, joining a
		// friend's grind is still a grind. Defence reaches the bot by another
		// road and is not affected.
		if (!ch || !ch->GetParty() || state.bVisitingShop || state.bRecoveringAfterDeath ||
				state.bServicePending || !IsPlayerBotGrindAllowedHere(ch) ||
				!IsPlayerBotPartyCohesive(ch, 2, PLAYERBOT_PARTY_COHESION_RADIUS) ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				(state.dwLastDeathTime != 0 && dwNow - state.dwLastDeathTime < 60000) ||
				ch->GetMaxHP() <= 0 ||
				ch->GetHP() * 100 < ch->GetMaxHP() * PLAYERBOT_PARTY_READY_HP_PERCENT)
			return NULL;

		FFindPlayerBotPartyFocus finder(ch, dwNow);
		ch->GetParty()->ForEachOnMapMember(finder, ch->GetMapIndex());
		return finder.GetTarget();
	}

	class CFindPlayerBotEngagedTarget
	{
		public:
			CFindPlayerBotEngagedTarget(LPCHARACTER owner, const std::map<DWORD, DWORD>* failed, DWORD dwNow) :
				m_owner(owner), m_target(NULL), m_bestPriority(INT_MIN), m_failed(failed), m_dwNow(dwNow) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (!candidate || candidate == m_owner || !candidate->IsMonster() ||
						candidate->IsDead() || candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return true;
				// A monster the attack pass has just given up on for want of a
				// way to it is not engaged, whatever it is doing. Without this
				// the mark lasted no time at all: the pass dropped a monkey on a
				// ledge, this finder handed the same monkey straight back, and
				// Pokonany stood under it for thirteen minutes - 373 unreachable
				// plans to one spot seven hundred units away, eight watchdog
				// resets (14 September).
				if (m_failed)
				{
					std::map<DWORD, DWORD>::const_iterator failed = m_failed->find(candidate->GetVID());
					if (failed != m_failed->end() && m_dwNow < failed->second)
						return true;
				}

				LPCHARACTER victim = candidate->GetVictim();
				const bool attacksOwner = victim == m_owner;
				const bool attacksParty = victim && m_owner->GetParty() &&
						victim->GetParty() == m_owner->GetParty();
				if (!attacksOwner && !attacksParty)
					return true;

				const int distance = DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > 2500)
					return true;
				const int priority = (attacksOwner ? 100000 : 50000) - distance;
				if (!m_target || priority > m_bestPriority)
				{
					m_target = candidate;
					m_bestPriority = priority;
				}
				return true;
			}

			LPCHARACTER GetTarget() const { return m_target; }

		private:
			LPCHARACTER m_owner;
			LPCHARACTER m_target;
			int m_bestPriority;
			const std::map<DWORD, DWORD>* m_failed;
			DWORD m_dwNow;
	};

	LPCHARACTER FindPlayerBotEngagedTarget(LPCHARACTER ch, const TPlayerBotAIState* state = NULL, DWORD dwNow = 0)
	{
		if (!ch || !ch->GetSectree() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return NULL;
		CFindPlayerBotEngagedTarget finder(ch, state ? &state->mapFailedTargets : NULL, dwNow);
		ch->GetSectree()->ForEachAround(finder);
		return finder.GetTarget();
	}

	class CCountPlayerBotStoneAttackers
	{
		public:
			CCountPlayerBotStoneAttackers(LPCHARACTER stone, LPCHARACTER exclude = NULL) :
				m_stone(stone), m_exclude(exclude), m_count(0), m_bots(0), m_players(0) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER attacker = static_cast<LPCHARACTER>(entity);
				if (attacker == m_exclude)
					return true;
				if (!attacker || !attacker->IsPC() || attacker->IsDead() ||
						attacker->GetMapIndex() != m_stone->GetMapIndex() ||
						DISTANCE_APPROX(attacker->GetX() - m_stone->GetX(),
								attacker->GetY() - m_stone->GetY()) > PLAYERBOT_STONE_SUPPORT_RANGE)
					return true;

				bool attacksStone = attacker->GetVictim() == m_stone;
				TPlayerBotAIStateMap::const_iterator it =
						s_mapPlayerBotAIStates.find(attacker->GetPlayerID());
				if (it != s_mapPlayerBotAIStates.end() &&
						it->second.dwTargetVID == (DWORD)m_stone->GetVID())
					attacksStone = true;
				if (attacksStone && m_count < 255)
				{
					++m_count;
					if (it != s_mapPlayerBotAIStates.end())
						++m_bots;
					else
						++m_players;
				}
				return true;
			}

			BYTE GetCount() const { return m_count; }
			int GetBots() const { return m_bots; }
			int GetPlayers() const { return m_players; }

		private:
			LPCHARACTER m_stone;
			LPCHARACTER m_exclude;
			BYTE m_count;
			int m_bots;
			int m_players;
	};

	BYTE CountPlayerBotStoneAttackers(LPCHARACTER stone, LPCHARACTER exclude = NULL)
	{
		if (!stone || !stone->GetSectree())
			return 0;
		CCountPlayerBotStoneAttackers counter(stone, exclude);
		stone->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
	}

	void CountPlayerBotStoneAttackersByKind(LPCHARACTER stone, LPCHARACTER exclude,
			int& bots, int& players)
	{
		bots = 0;
		players = 0;
		if (!stone || !stone->GetSectree())
			return;
		CCountPlayerBotStoneAttackers counter(stone, exclude);
		stone->GetSectree()->ForEachAround(counter);
		bots = counter.GetBots();
		players = counter.GetPlayers();
	}

	// Iwakura's stone hunter counts only its own: "jesli Metina bije juz 3 lub
	// wiecej botow z tego samego krolestwa, bot rezygnuje". A bot is one this
	// core runs - the state map says so - and "breaking" is the same test as
	// the plain count's: its victim, or its AI's target.
	class CCountPlayerBotStoneKingdomBots
	{
		public:
			CCountPlayerBotStoneKingdomBots(LPCHARACTER stone, LPCHARACTER exclude, BYTE empire) :
				m_stone(stone), m_exclude(exclude), m_empire(empire), m_count(0) {}

			void operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER attacker = static_cast<LPCHARACTER>(entity);
				if (attacker == m_exclude || !attacker->IsPC() || attacker->IsDead() ||
						attacker->GetEmpire() != m_empire ||
						attacker->GetMapIndex() != m_stone->GetMapIndex() ||
						DISTANCE_APPROX(attacker->GetX() - m_stone->GetX(),
								attacker->GetY() - m_stone->GetY()) > PLAYERBOT_STONE_SUPPORT_RANGE)
					return;
				TPlayerBotAIStateMap::const_iterator it =
						s_mapPlayerBotAIStates.find(attacker->GetPlayerID());
				if (it == s_mapPlayerBotAIStates.end())
					return;
				if (attacker->GetVictim() == m_stone ||
						it->second.dwTargetVID == (DWORD)m_stone->GetVID())
					++m_count;
			}

			int GetCount() const { return m_count; }

		private:
			LPCHARACTER m_stone;
			LPCHARACTER m_exclude;
			BYTE m_empire;
			int m_count;
	};

	int CountPlayerBotStoneKingdomBots(LPCHARACTER stone, LPCHARACTER ch)
	{
		if (!stone || !ch || !stone->GetSectree())
			return 0;
		CCountPlayerBotStoneKingdomBots counter(stone, ch, ch->GetEmpire());
		stone->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
	}

	// And the other half of his rule: somebody of another kingdom breaking the
	// stone - a bot, or a person ("doslownie z dokumentu", Tieru, 19 September)
	// - whom this bot's blow can reach. The nearest to the bot.
	class FFindPlayerBotStoneRival
	{
		public:
			FFindPlayerBotStoneRival(LPCHARACTER ch, LPCHARACTER stone) :
				m_ch(ch), m_stone(stone), m_found(NULL), m_bestDistance(INT_MAX) {}

			void operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER other = static_cast<LPCHARACTER>(entity);
				if (other == m_ch || !other->IsPC() || other->IsDead() ||
						other->GetEmpire() == m_ch->GetEmpire() ||
						other->GetMapIndex() != m_stone->GetMapIndex() ||
						other->IsAffectFlag(AFF_REVIVE_INVISIBLE) ||
						(m_ch->GetParty() && other->GetParty() == m_ch->GetParty()) ||
						DISTANCE_APPROX(other->GetX() - m_stone->GetX(),
								other->GetY() - m_stone->GetY()) > PLAYERBOT_STONE_SUPPORT_RANGE)
					return;
				bool attacksStone = other->GetVictim() == m_stone;
				if (!attacksStone)
				{
					TPlayerBotAIStateMap::const_iterator it =
							s_mapPlayerBotAIStates.find(other->GetPlayerID());
					attacksStone = it != s_mapPlayerBotAIStates.end() &&
							it->second.dwTargetVID == (DWORD)m_stone->GetVID();
				}
				if (!attacksStone || !CanPlayerBotStrikeCharacter(m_ch, other) ||
						IsPlayerBotSafeZone(other->GetMapIndex(), other->GetX(), other->GetY()))
					return;
				const int distance = DISTANCE_APPROX(m_ch->GetX() - other->GetX(),
						m_ch->GetY() - other->GetY());
				if (distance < m_bestDistance)
				{
					m_bestDistance = distance;
					m_found = other;
				}
			}

			LPCHARACTER GetFound() const { return m_found; }

		private:
			LPCHARACTER m_ch;
			LPCHARACTER m_stone;
			LPCHARACTER m_found;
			int m_bestDistance;
	};

	LPCHARACTER FindPlayerBotStoneRival(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || !stone->GetSectree())
			return NULL;
		FFindPlayerBotStoneRival finder(ch, stone);
		stone->GetSectree()->ForEachAround(finder);
		return finder.GetFound();
	}

	// Somebody is already breaking this stone, and it is somebody a bot
	// joins: another bot always, a player only with PLAYERBOT_STONE_JOIN_PLAYERS.
	bool IsPlayerBotStoneUnderJoinableAttack(LPCHARACTER ch, LPCHARACTER stone)
	{
		int bots = 0, players = 0;
		CountPlayerBotStoneAttackersByKind(stone, ch, bots, players);
		return bots > 0 || (PLAYERBOT_STONE_JOIN_PLAYERS && players > 0);
	}

	// A stone above the bot's own band that others are already breaking. "Jesli
	// nie da sobie rady, niech dolacza jesli ktos w danym momencie bije kamien"
	// (Tieru, 16 September): up to PLAYERBOT_STONE_JOIN_LEVEL_DELTA over the bot,
	// never one it has outgrown by PLAYERBOT_STONE_OUTGROWN_LEVELS (the drop
	// curve is gone there for everybody) - sixteen either way, the operator's
	// band - and never a dungeon trigger, unless the bot is climbing the tower
	// with a player, for whom it is the floor's objective and needs nobody
	// else on it first. Kiciamol's report was the other half - one bot on a
	// stone and the rest walking past, because a claimed target was a claimed
	// target; see IsTargetClaimedByAnotherBot.
	bool IsPlayerBotStoneJoinable(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
			return false;
		if (IsPlayerBotDungeonTriggerStone(stone->GetRaceNum()))
			return IsPlayerBotDungeonStoneObjective(ch, stone);
		// Under Iwakura's system the band of the stone hunter is the band of
		// every stone, joined or not: ten levels either way.
		if (IsPlayerBotPersonaEnabled())
		{
			if (!playerbot_persona::InPogromcaBand((int)ch->GetLevel(), (int)stone->GetLevel()))
				return false;
		}
		else if ((int)stone->GetLevel() > (int)ch->GetLevel() + PLAYERBOT_STONE_JOIN_LEVEL_DELTA ||
				(int)ch->GetLevel() > (int)stone->GetLevel() + PLAYERBOT_STONE_OUTGROWN_LEVELS)
			return false;
		return IsPlayerBotStoneUnderJoinableAttack(ch, stone);
	}

	// An Archer breaks a Metin with a dagger, and only a refined one manages it.
	// Without a +4 dagger it must not take a stone on its own - a bow does not
	// break stones and a +0 dagger barely scratches one, so it "pada na gleba x
	// razy i rezygnuje" - but it may still help a stone somebody else is already
	// breaking, from range with the bow ("no chyba ze ktos inny bije kamien metin
	// to on moze z luku go bic", Tieru). Melee classes are unchanged.
	bool CanPlayerBotEngageStone(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone)
			return false;
		if (!IsPlayerBotArcherBuild(ch))
			return true;
		if (HasPlayerBotUsableStoneDagger(ch))
			return true;
		// No usable dagger: only join a stone others are already breaking. Count
		// the other attackers, never this bot, or its own first bow shot would
		// keep it going after the others had left.
		return CountPlayerBotStoneAttackers(stone, ch) > 0;
	}

	void ResetPlayerBotStoneProgress(TPlayerBotAIState& state)
	{
		state.dwStoneFightStartTime = 0;
		state.dwStoneProgressVID = 0;
		state.dwStoneLastProgressTime = 0;
		state.dwNextStoneProgressCheckTime = 0;
		state.iLastStoneHP = 0;
		state.bLastStoneAttackerCount = 0;
	}

	bool ShouldPlayerBotAbandonStone(LPCHARACTER ch, LPCHARACTER stone,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
		{
			ResetPlayerBotStoneProgress(state);
			return false;
		}

		if (state.dwStoneProgressVID != (DWORD)stone->GetVID())
		{
			ResetPlayerBotStoneProgress(state);
			state.dwStoneProgressVID = stone->GetVID();
			state.dwStoneFightStartTime = dwNow;
			state.dwStoneLastProgressTime = dwNow;
			state.dwNextStoneProgressCheckTime =
					dwNow + PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL;
			state.iLastStoneHP = stone->GetHP();
			state.bLastStoneAttackerCount = CountPlayerBotStoneAttackers(stone);
			return false;
		}

		if (dwNow < state.dwNextStoneProgressCheckTime)
			return false;
		state.dwNextStoneProgressCheckTime =
				dwNow + PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL;

		const BYTE attackerCount = CountPlayerBotStoneAttackers(stone);
		// A new helper may turn a regenerative stalemate into real progress. Give the
		// enlarged group a complete observation window instead of abandoning just as
		// help arrives.
		if (attackerCount > state.bLastStoneAttackerCount)
			state.dwStoneLastProgressTime = dwNow;
		state.bLastStoneAttackerCount = attackerCount;

		const int meaningfulDamage = std::max(1, stone->GetMaxHP() / 200);
		if (stone->GetHP() + meaningfulDamage <= state.iLastStoneHP)
		{
			state.iLastStoneHP = stone->GetHP();
			state.dwStoneLastProgressTime = dwNow;
		}

		if (dwNow - state.dwStoneFightStartTime < PLAYERBOT_STONE_INITIAL_GRACE)
			return false;
		const DWORD stallTimeout = attackerCount >= 2
				? PLAYERBOT_STONE_GROUP_STALL_TIMEOUT
				: PLAYERBOT_STONE_SOLO_STALL_TIMEOUT;
		if (dwNow - state.dwStoneLastProgressTime < stallTimeout)
			return false;

		const DWORD failedVID = stone->GetVID();
		const int currentHP = stone->GetHP();
		const int maxHP = stone->GetMaxHP();
		state.mapFailedStones[failedVID] = dwNow + PLAYERBOT_STONE_FAILED_COOLDOWN;
		ReleasePlayerBotMetinReservation(ch, stone);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwNextWanderTime = dwNow + number(1000, 2500);
		sys_log(0, "PLAYERBOT_METIN: abandoned stalled stone pid=%u name=%s stone_vid=%u stone=%s hp=%d/%d best_hp=%d attackers=%u fight_ms=%u stalled_ms=%u cooldown_ms=%u",
				ch->GetPlayerID(), ch->GetName(), failedVID, stone->GetName(),
				currentHP, maxHP, state.iLastStoneHP, (unsigned int)attackerCount,
				(unsigned int)(dwNow - state.dwStoneFightStartTime),
				(unsigned int)(dwNow - state.dwStoneLastProgressTime),
				(unsigned int)PLAYERBOT_STONE_FAILED_COOLDOWN);
		ResetPlayerBotStoneProgress(state);
		return true;
	}

	void ResetPlayerBotFightProgress(TPlayerBotAIState& state)
	{
		state.dwFightProgressVID = 0;
		state.bFightProgressBoss = false;
		state.dwFightStartTime = 0;
		state.dwFightLastProgressTime = 0;
		state.iLastFightHP = 0;
	}

	// The fight that goes nowhere.
	//
	// A stalled Metin has been released for a long time. An ordinary monster had
	// no such rule: the only ways out of a fight were killing it, dying, or
	// failing three times to walk to it. None of the three happens when the two
	// of them heal as fast as they hurt each other - botserqet spent an evening
	// on one Black Orc at 3261 of 5114 health, drinking a red potion every time
	// the bar dropped, winning nothing and losing nothing. An operator watching
	// asked whether it would ever decide it was too weak and go and find
	// something easier. It would not.
	//
	// So it gets the test the stones get, and the same shape: progress means
	// half a per cent of the monster's health, which no rounding can fake, and
	// the best health ever reached is what improvement is measured against - a
	// monster that regenerates between blows makes no progress however many
	// times the same points are taken off it again.
	//
	// The loser goes on the failed list rather than merely being dropped.
	// Without that the very next scan picks the same monster, being the nearest,
	// and the stalemate resumes with the clock reset.
	bool ShouldPlayerBotAbandonFight(LPCHARACTER ch, LPCHARACTER target,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || target->IsStone() || target->IsDead() ||
				!target->IsMonster())
			return false;

		const DWORD vid = (DWORD)target->GetVID();
		if (state.dwFightProgressVID != vid)
		{
			state.dwFightProgressVID = vid;
			state.bFightProgressBoss = target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS;
			state.dwFightStartTime = dwNow;
			state.dwFightLastProgressTime = dwNow;
			state.iLastFightHP = target->GetHP();
			return false;
		}

		const int meaningfulDamage = std::max(1, target->GetMaxHP() / 200);
		if (target->GetHP() + meaningfulDamage <= state.iLastFightHP)
		{
			state.iLastFightHP = target->GetHP();
			state.dwFightLastProgressTime = dwNow;
		}

		if (dwNow - state.dwFightStartTime < PLAYERBOT_FIGHT_INITIAL_GRACE ||
				dwNow - state.dwFightLastProgressTime < PLAYERBOT_FIGHT_STALL_TIMEOUT)
			return false;

		sys_log(0, "PLAYERBOT_AI: abandoned stalled fight pid=%u name=%s level=%u target=%s target_level=%u hp=%d/%d best_hp=%d fight_ms=%u cooldown_ms=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
				target->GetName(), target->GetLevel(),
				target->GetHP(), target->GetMaxHP(), state.iLastFightHP,
				(unsigned int)(dwNow - state.dwFightStartTime),
				(unsigned int)PLAYERBOT_FIGHT_FAILED_COOLDOWN);
		state.mapFailedTargets[vid] = dwNow + PLAYERBOT_FIGHT_FAILED_COOLDOWN;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwNextWanderTime = dwNow + number(1000, 2500);
		ResetPlayerBotFightProgress(state);
		return true;
	}

	bool IsTargetClaimedByAnotherBot(LPCHARACTER owner, DWORD dwTargetVID)
	{
		if (!owner || dwTargetVID == 0)
			return false;

		// A stone is nobody's: it is broken together, and the only claim on it
		// is a full crowd. One bot's claim kept every other off the one stone in
		// sight ("jak jest jeden metek to jeden bije a reszta sie nie dolacza",
		// Kiciamol, 16 September).
		{
			LPCHARACTER target = CHARACTER_MANAGER::instance().Find(dwTargetVID);
			if (target && target->IsStone())
			{
				// Iwakura's crowd is the bot's own kingdom: three of its bots
				// and it goes back to what it was doing. Another kingdom's on
				// the stone is not a crowd but a rival (playerbot_anti_pk.h).
				if (IsPlayerBotPersonaEnabled())
					return playerbot_persona::IsPogromcaCrowded(
							CountPlayerBotStoneKingdomBots(target, owner));
				return CountPlayerBotStoneAttackers(target, owner) >= PLAYERBOT_STONE_MAX_ATTACKERS;
			}
		}

		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (it->first == owner->GetPlayerID() || it->second.dwTargetVID != dwTargetVID)
				continue;

			LPCHARACTER claimant = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (owner->GetParty() && claimant && claimant->GetParty() == owner->GetParty())
				continue;

			return true;
		}

		return false;
	}

	struct TTargetCandidate
	{
		DWORD dwVID;
		int distance;
		int level;
		bool bIsStone;
		bool bPriorityObjective;
		// This monster's DROP_ITEM is a material the bot is short of.
		bool bWantedDrop;
		int score;

		bool operator < (const TTargetCandidate& other) const
		{
			return score > other.score; // Higher score first
		}
	};

	// A monster the bot's horse trial wants dead: the desert's two archers
	// while the battle horse is being earned, the Demon Tower's four while the
	// military one is. A quest target to the policy and to the score, whatever
	// the bot's level says about the experience - a bot of seventy on the
	// desert was refusing every scorpion as worthless, and 161 of the 178 bots
	// of seventy and up with a horse at ten on the test world had never made a
	// single kill of the trial (16 September).
	bool IsPlayerBotHorseTrialTarget(LPCHARACTER ch, LPCHARACTER candidate)
	{
		if (!ch || !candidate || !candidate->IsMonster())
			return false;
		const DWORD race = candidate->GetRaceNum();
		return (IsPlayerBotOnBattleHorseTrial(ch) && IsPlayerBotBattleHorseTrialMob(race)) ||
				(IsPlayerBotOnMilitaryHorseTrial(ch) && IsPlayerBotMilitaryHorseTrialMob(race));
	}

	// The monster this bot's own errands want it to kill, if any. One
	// definition, because the collector and the re-check below must not
	// disagree about what counts as an active hunt.
	DWORD GetPlayerBotDesiredQuestMobVnum(LPCHARACTER ch,
			const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return 0;
		// The battle horse's trial first, in the desert: the Biologist yields
		// to it anyway (GetPlayerBotBiologistHuntMob), and its two archers are
		// what the walk below has to find.
		const DWORD trialMob = GetPlayerBotHorseTrialHuntMob(ch);
		if (trialMob != 0)
			return trialMob;
		DWORD desiredBiologistMobVnum = 0;
		size_t biologistIndex = 0;
		const TPlayerBotBiologistMission* biologistMission =
				GetActivePlayerBotBiologistMission(ch, &biologistIndex);
		if (biologistMission && !state.bVisitingBiologist)
		{
			const int accepted = std::max(0, ch->GetQuestFlag(
					GetPlayerBotBiologistFlag(*biologistMission, "collect_count")));
			const int remaining = std::max(0,
					(int)biologistMission->requiredCount - accepted);
			// In a row's second half the bot hunts whatever the quest's own kill
			// hook rolls the key on, which is the row's business and not this
			// function's: the Orc Tooth wants the Elite Orc, the Curse Book the
			// same Tormentor that carries the specimen.
			if (IsPlayerBotBiologistKeyPhase(ch, biologistIndex))
				desiredBiologistMobVnum = biologistMission->keyMobVnum;
			else if (ch->CountSpecifyItem(biologistMission->itemVnum) < remaining)
				desiredBiologistMobVnum = biologistMission->mobVnum;
		}
		const DWORD desiredHuntingMobVnum = GetActivePlayerBotHuntingMobVnum(ch);
		DWORD desiredQuestMobVnum = desiredBiologistMobVnum;
		if (desiredHuntingMobVnum != 0)
		{
			// When both activities are open, rotate small deterministic cohorts every
			// two minutes. The world then looks like independent players choosing
			// goals, not one synchronized swarm finishing Biologist first.
			if (desiredQuestMobVnum == 0 ||
					((ch->GetPlayerID() + dwNow / 120000) % 3) == 0)
				desiredQuestMobVnum = desiredHuntingMobVnum;
		}
		return desiredQuestMobVnum;
	}

	// The one place a combat Context is built.
	//
	// playerbot_combat_value_policy.h decides whether an ordinary monster is
	// worth fighting; everything that has to ask the engine is here, so the
	// collector, the manager's re-check and the party and multi-pull selectors
	// all get the same answer to the same question. Duplicating "what counts as
	// an active hunt" or "which drop is wanted" in each of those was how a bot
	// came to be refused a monster in one place and handed it in another.
	//
	// Ordinary monsters only. A Metin stone keeps its own rules and its own
	// reservation, and nothing here is asked about players.
	// The Demon Tower's objectives (playerbot_demon_tower.h, later in the
	// include order): everything on a floor, and the ground floor's stone
	// for a raid breaking it.
	bool IsPlayerBotDemonTowerTarget(LPCHARACTER ch, LPCHARACTER candidate);

	playerbot_combat_value::Context BuildPlayerBotCombatContext(
			LPCHARACTER ch, LPCHARACTER candidate, const TPlayerBotAIState& state,
			bool baseEligible, DWORD desiredMobVnum, bool huntM2Bestials,
			const std::set<DWORD>* wantedDrops, DWORD dwNow)
	{
		playerbot_combat_value::Context context;
		if (!ch || !candidate)
			return context;
		context.baseEligible = baseEligible;

		// Retreating outranks every errand; a bot breaking off a losing fight
		// does not get to pick a new one on the way out.
		if (state.bTacticalRetreat || state.bRecoveringAfterDeath)
			context.mode = playerbot_combat_value::RETREAT;
		// A person's standing order to lure is a job, and grinding is not part
		// of it. Nothing said so in 2.0.89, so between courses the Archer went
		// hunting like any other bot - "Ninja zabija ich zamiast przyniesc do
		// mnie" (marcinxboss, 20 September) - and then could never set off,
		// because a monster chasing the Archer is what a course refuses to
		// start with. Self-defence never asks this policy and party defence is
		// answered above COMMITTED_TRAVEL, so the bot still hits back and still
		// helps the person it is luring for; what it stops doing is walking up
		// to things on its own.
		else if (state.dwLurePlayerPID != 0)
			context.mode = playerbot_combat_value::COMMITTED_TRAVEL;
		// "Committed" means the errand has actually begun, not that the goal
		// says so: a bot carrying BOT_GOAL_RESTOCK that never started a visit is
		// simply standing about, and the audit was explicit that the goal string
		// is not the proof.
		else if (state.bVisitingShop || state.bVisitingBiologist ||
				state.bVisitingStable || state.bMarketTrip || state.bFishingSession)
			context.mode = playerbot_combat_value::COMMITTED_TRAVEL;
		// An errand the watchdog interrupted is still this bot's job. Without
		// this the reset handed an unmet need straight back to the target
		// picker: "shop=1 route=0/0" in the watchdog line, an attack skill one
		// second later, and the merchant never reached.
		else if (state.bServicePending)
			context.mode = playerbot_combat_value::COMMITTED_TRAVEL;
		// The battle horse's trial in the desert is the same shape as the
		// residence rule below: the trial's archers are a quest target and
		// count, defence and a wanted drop count, and plain experience does
		// not. A bot of thirty-five to forty always had a spider or a scorpion
		// king in reach, so it never reached the map scan that walks it to the
		// archers (StartPlayerBotMaterialHunt runs on a tick with nothing to
		// fight): one walk in twenty minutes on m2zip for 38 bots on the trial
		// (24 September).
		else if (GetPlayerBotHorseTrialHuntMob(ch) != 0)
			context.mode = playerbot_combat_value::SERVICE_ONLY;
		// And the residence rule: on a map this bot has outgrown, a named
		// errand still counts and plain experience does not.
		else if (!IsPlayerBotGrindAllowedHere(ch))
			context.mode = playerbot_combat_value::SERVICE_ONLY;
		else
			context.mode = playerbot_combat_value::OBJECTIVES;

		// A course that is already running is choosing its pack, and the mode
		// above is exactly what would refuse it. Two halves, both needed: only
		// on a person's order, because that order is what put the bot in
		// COMMITTED_TRAVEL in the first place and the bots' own role never
		// leaves OBJECTIVES; and only while a course is actually running, so
		// that between courses the order still stops the Archer hunting what it
		// was asked to fetch.
		context.lureCourseTarget = state.dwLurePlayerPID != 0 &&
				state.bLureStage != LURE_STAGE_NONE;

		// One defence episode per bot, not one per attacker.
		//
		// The first draft let a different attacker start a fresh episode
		// whenever the previous one expired, so two monsters taking turns kept
		// the exception alive for ever - the bound existed on paper only. The
		// episode now belongs to the bot: while one is running, any attacker is
		// answered within its time and its leash; once it has run out, no new
		// one begins until the fighting has actually stopped for
		// PLAYERBOT_DEFENCE_QUIET_TIME (cleared in the tick). A bot still being
		// hit after that does not stand and take it - breaking off is the
		// survival pass's job, and RETREAT outranks every exception here.
		//
		// Party defence lives inside the same episode, so it is bounded in time
		// as well as in distance.
		const bool onTheLeash = state.dwDefenceEpisodeStart == 0 ||
				DISTANCE_APPROX(ch->GetX() - state.lDefenceAnchorX,
						ch->GetY() - state.lDefenceAnchorY) <= PLAYERBOT_DEFENCE_LEASH;
		const bool episodeLive = state.dwDefenceEpisodeStart != 0 && onTheLeash &&
				dwNow - state.dwDefenceEpisodeStart <= PLAYERBOT_DEFENCE_EPISODE_TIME;
		const bool mayDefend = episodeLive || state.dwDefenceEpisodeStart == 0;

		// Hitting back at whatever is hitting you is not a choice, and it is not
		// bounded by a clock. The episode's job is to stop "it hit me first"
		// becoming a licence to work a map: what does that is the leash - the
		// bot answers where it stands and does not get walked across the world
		// by a chain of attackers. Ten seconds of it was a different rule
		// altogether, and it showed: nine strong monsters dropped next to a
		// group of bots killed all of them, because after ten seconds every one
		// of those monsters was refused as a target while it was still killing
		// its bot. Breaking off a fight it cannot win is the survival pass's
		// decision, and RETREAT still outranks everything here.
		if (onTheLeash && candidate->GetVictim() == ch)
			context.boundedSelfDefense = true;

		// A party member actually under attack, near enough to help. Being the
		// party's focus is not the same thing and does not count - and helping
		// is optional in a way defending yourself is not, so this one keeps the
		// episode's clock.
		if (mayDefend && !context.boundedSelfDefense && ch->GetParty())
		{
			LPCHARACTER victim = candidate->GetVictim();
			if (victim && victim != ch && victim->IsPC() && !victim->IsDead() &&
					victim->GetMapIndex() == ch->GetMapIndex() &&
					ch->GetParty()->IsMember(victim->GetPlayerID()) &&
					DISTANCE_APPROX(ch->GetX() - victim->GetX(),
							ch->GetY() - victim->GetY()) <= PLAYERBOT_PARTY_DEFENCE_RANGE)
				context.boundedPartyDefense = true;
		}

		if ((desiredMobVnum != 0 && candidate->IsMonster() &&
				IsPlayerBotBiologistHuntRace(desiredMobVnum, candidate->GetRaceNum())) ||
				IsPlayerBotHorseTrialTarget(ch, candidate) ||
				IsPlayerBotDemonTowerTarget(ch, candidate))
			context.activeQuestTarget = true;

		// A material the bot is actually short of. CollectPlayerBotWantedMaterials
		// builds that set from real shortages, and GetMobDropItemVnum is the
		// designated etc-drop this monster carries - not a model of every drop
		// table, so a false answer here means "not known to drop it", never "it
		// cannot".
		//
		// CollectPlayerBotWantedMaterials only puts a material in that set for a
		// piece below its refine target whose recipe needs more than the bot is
		// carrying, so the shortage is real and the exception ends when the
		// stock is filled. What that function cannot know is whether this
		// particular monster can still yield it: CreateDropItem multiplies every
		// drop by the same PERCENT_LVDELTA as experience, so fifteen levels
		// above leaves one percent of the chance. A need is not a reason to farm
		// something that will effectively never drop it.
		if (wantedDrops && !wantedDrops->empty() && candidate->IsMonster() &&
				PERCENT_LVDELTA(ch->GetLevel(), candidate->GetLevel()) >=
					PLAYERBOT_MATERIAL_MIN_DROP_PERCENT)
		{
			const DWORD drop = candidate->GetMobDropItemVnum();
			if (drop != 0 && wantedDrops->find(drop) != wantedDrops->end())
				context.activeMaterialTarget = true;
		}

		if (huntM2Bestials && candidate->IsMonster() &&
				(candidate->GetRaceNum() == 533 || candidate->GetRaceNum() == 534))
			context.activeEquipmentTarget = true;

		// The engine's own level table, read with the engine's own argument
		// order: PERCENT_LVDELTA(me, victim) in constants.h, 1 at fifteen levels
		// above the monster and 100 at parity. This is the share of the base
		// experience that survives the level difference - not a prediction of
		// the experience actually granted, which party sharing and rounding
		// still act on.
		if (candidate->IsMonster())
		{
			context.expEvidenceKnown = true;
			context.levelExpPercent =
					PERCENT_LVDELTA(ch->GetLevel(), candidate->GetLevel());
			context.canReceiveExp = candidate->GetMobTable().dwExp > 0;
			// Outgrown prey across the field. The level table is too kind to
			// catch it (see PLAYERBOT_VILLAGE_OUTGROWN_LEVELS), so the rule is
			// the level difference itself, and it only applies where the walk
			// would take the bot away from its own band's ground: the villages,
			// first and second. The second joined in 2.0.58, when the bots were
			// found chain-killing outward from the gate they came in by and the
			// far half of Jayang and Bakra stood empty.
			if ((IsPlayerBotM1Map(ch->GetMapIndex()) || IsPlayerBotM2Map(ch->GetMapIndex())) &&
					(int)ch->GetLevel() - (int)candidate->GetLevel() >=
						PLAYERBOT_VILLAGE_OUTGROWN_LEVELS &&
					DISTANCE_APPROX(ch->GetX() - candidate->GetX(),
							ch->GetY() - candidate->GetY()) > PLAYERBOT_OUTGROWN_CHAIN_RANGE)
				context.outgrownPrey = true;
		}
		return context;
	}

	playerbot_combat_value::Decision DecidePlayerBotCombatValue(
			LPCHARACTER ch, LPCHARACTER candidate, const TPlayerBotAIState& state,
			bool baseEligible, DWORD desiredMobVnum, bool huntM2Bestials,
			const std::set<DWORD>* wantedDrops, DWORD dwNow)
	{
		playerbot_combat_value::Policy policy;
		policy.minLevelExpPercent = PLAYERBOT_COMBAT_MIN_EXP_PERCENT;
		return playerbot_combat_value::Evaluate(
				BuildPlayerBotCombatContext(ch, candidate, state, baseEligible,
						desiredMobVnum, huntM2Bestials, wantedDrops, dwNow),
				policy);
	}

	// Stamp the episode when an attacker is accepted as a target, so the clock
	// and the leash below have somewhere to start from. Called once per new
	// attacker, never per tick: renewing it is what the bound is against.
	void NotePlayerBotDefenceEpisode(LPCHARACTER ch, TPlayerBotAIState& state,
			LPCHARACTER target, DWORD dwNow)
	{
		if (!ch || !target || target->GetVictim() != ch)
			return;
		// Only when no episode is running. Stamping again - for another
		// attacker, or for the same one on the next tick - is the renewal this
		// bound exists to prevent.
		if (state.dwDefenceEpisodeStart != 0)
			return;
		state.dwDefenceTargetVID = (DWORD)target->GetVID();
		state.dwDefenceEpisodeStart = dwNow;
		state.lDefenceAnchorX = ch->GetX();
		state.lDefenceAnchorY = ch->GetY();
	}

	// Is the monster this bot is already fighting still worth fighting?
	//
	// Filtering only new candidates would leave the old loophole open: a bot
	// that picked something up before its errand changed, or before the last
	// unit of a material was collected, would keep swinging at it for as long
	// as it lived. Asked on a clock rather than every tick, because the answer
	// needs the bot's material shortages and those cost a walk of the bag.
	// Stones are not asked: they keep their own worth rule and reservation.
	bool IsPlayerBotTargetWorthNow(LPCHARACTER ch, LPCHARACTER target,
			const TPlayerBotAIState& state, DWORD dwNow, BYTE* pReasonOut = NULL)
	{
		if (!ch || !target || !target->IsMonster() || target->IsStone())
			return true;
		std::set<DWORD> wantedDrops;
		CollectPlayerBotWantedMaterials(ch, wantedDrops);
		const bool huntBestials = IsPlayerBotM2Map(ch->GetMapIndex()) &&
				ShouldPlayerBotHuntM2Bestials(ch);
		const playerbot_combat_value::Decision decision =
				DecidePlayerBotCombatValue(ch, target, state, true,
						GetPlayerBotDesiredQuestMobVnum(ch, state, dwNow),
						huntBestials, &wantedDrops, dwNow);
		// Handed back rather than written into the state: this overload is the
		// one the collector calls with a const state, and the line over a bot's
		// head needs the reason its own fight was allowed for.
		if (pReasonOut)
			*pReasonOut = (BYTE)decision.reason;
		if (!decision.allowed)
			PlayerBotLogThrottled("combat_dropped", dwNow,
					"PLAYERBOT_COMBAT: refused target pid=%u name=%s target=%s target_level=%u reason=%s",
					ch->GetPlayerID(), ch->GetName(), target->GetName(),
					target->GetLevel(), playerbot_combat_value::ReasonName(decision.reason));
		return decision.allowed;
	}

	// Why is this bot still standing in Bokjung?
	//
	// A count of level-40 bots on the map says nothing on its own - buying,
	// hunting a real mission, passing through and having nothing to do all look
	// identical from outside. These are the reasons, in the order that decides:
	// what the bot is actually doing beats what it might be planning.
	const char* ClassifyPlayerBotTownStay(LPCHARACTER ch,
			const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return "none";
		if (state.bTacticalRetreat || state.bRecoveringAfterDeath)
			return "retreat";
		if (state.dwDefenceEpisodeStart != 0 &&
				dwNow - state.dwDefenceEpisodeStart <= PLAYERBOT_DEFENCE_EPISODE_TIME)
			return "defence";
		if (state.bVisitingShop)
			return "visit";
		if (state.bVisitingBiologist || state.bVisitingStable)
			return "errand";
		if (state.bMarketTrip)
			return "market";
		if (state.bFishingSession)
			return "fishing";
		if (ch->GetMyShop())
			return "stall";
		if (GetPlayerBotDesiredQuestMobVnum(ch, state, dwNow) != 0)
			return "quest";
		{
			std::set<DWORD> wantedDrops;
			CollectPlayerBotWantedMaterials(ch, wantedDrops);
			if (!wantedDrops.empty())
				return "material";
		}
		if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size())
			return "travel";
		return "no_plan";
	}

	// The same question on a clock, for the monster a bot is already fighting.
	bool IsPlayerBotHeldTargetStillWorth(LPCHARACTER ch, LPCHARACTER target,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || !target->IsMonster() || target->IsStone())
			return true;
		if (dwNow < state.dwNextCombatRecheckTime)
			return true;
		state.dwNextCombatRecheckTime = dwNow + PLAYERBOT_COMBAT_RECHECK_INTERVAL;
		return IsPlayerBotTargetWorthNow(ch, target, state, dwNow,
				&state.bLastCombatReason);
	}

	class CCollectPlayerBotTargets
	{
		public:
			CCollectPlayerBotTargets(LPCHARACTER owner, int maxDistance, int maxLevel,
					int partyChallengeMaxLevel, DWORD desiredMobVnum, DWORD dwAvoidVID,
					long lAvoidX, long lAvoidY, int avoidRadius,
					const std::map<DWORD, DWORD>& failedStones,
					const std::map<DWORD, DWORD>& failedTargets, DWORD dwNow) :
				m_owner(owner),
				m_maxDistance(maxDistance),
				m_maxLevel(maxLevel),
				m_partyChallengeMaxLevel(partyChallengeMaxLevel),
				m_desiredMobVnum(desiredMobVnum),
				m_dwAvoidVID(dwAvoidVID),
				m_lAvoidX(lAvoidX),
				m_lAvoidY(lAvoidY),
				m_avoidRadius(avoidRadius),
				m_failedStones(failedStones),
				m_failedTargets(failedTargets),
				m_dwNow(dwNow),
				m_huntM2Bestials(owner && IsPlayerBotM2Map(owner->GetMapIndex()) &&
						ShouldPlayerBotHuntM2Bestials(owner)),
				m_pWantedDrops(NULL),
				m_pState(NULL),
				m_iMonstersNear(0),
				m_iMonsterLevelSum(0)
			{
			}

			// The materials the owner is short of, computed once by the caller.
			void SetWantedDrops(const std::set<DWORD>* pWanted) { m_pWantedDrops = pWanted; }
			void SetState(const TPlayerBotAIState* pState) { m_pState = pState; }
			// Monsters within reach when this ran, whatever their level: what the
			// spot memory learns a place by.
			int MonstersNear() const { return m_iMonstersNear; }
			int MonsterLevelSum() const { return m_iMonsterLevelSum; }

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || (!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead())
					return false;
				if (IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;

				// Counted before any filter below has its say, so a level-46 camp
				// is remembered as full by the level-36 bot that could not touch it.
				if (candidate->IsMonster() && candidate->GetMapIndex() == m_owner->GetMapIndex() &&
						DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
								m_owner->GetY() - candidate->GetY()) <= m_maxDistance)
				{
					++m_iMonstersNear;
					m_iMonsterLevelSum += candidate->GetLevel();
				}

				if (candidate->IsStone())
					RememberPlayerBotMetin(candidate, m_dwNow);

				if (m_dwAvoidVID != 0 && candidate->GetVID() == m_dwAvoidVID)
					return false;

				std::map<DWORD, DWORD>::const_iterator failedTarget = m_failedTargets.find(candidate->GetVID());
				if (failedTarget != m_failedTargets.end() && m_dwNow < failedTarget->second)
					return false;

				// Stones must remain inside the useful drop window and not be in the
				// failed-stone cooldown. This also keeps over-levelled bots away from
				// decorative low Metins which no longer reward their time.
				if (candidate->IsStone())
				{
					if (!IsPlayerBotMetinWorthFighting(m_owner, candidate) &&
							!IsPlayerBotStoneJoinable(m_owner, candidate))
						return false;

					// An Archer with no refined dagger does not solo a stone; it
					// only joins one others are already breaking (then with the
					// bow). Melee classes are unaffected.
					if (!CanPlayerBotEngageStone(m_owner, candidate))
						return false;

					std::map<DWORD, DWORD>::const_iterator stit = m_failedStones.find(candidate->GetVID());
					if (stit != m_failedStones.end() && m_dwNow < stit->second)
						return false;
				}
				else if (candidate->IsMonster())
				{
					if (candidate->GetLevel() > m_maxLevel)
						return false;
				}

				if (candidate->GetMapIndex() != m_owner->GetMapIndex())
					return false;

				if (m_avoidRadius > 0 && m_lAvoidX != 0 && m_lAvoidY != 0)
				{
					const int deathDist = DISTANCE_APPROX(
							m_lAvoidX - candidate->GetX(),
							m_lAvoidY - candidate->GetY());
					if (deathDist <= m_avoidRadius)
						return false;
				}
				// And the ground a capitulation gave up (the Anti-PK protocol),
				// for its forty-five minutes.
				if (m_pState && IsPlayerBotAvoidedSpot(*m_pState, candidate->GetMapIndex(),
						candidate->GetX(), candidate->GetY(), m_dwNow))
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > m_maxDistance)
					return false;

				const int botLevel = m_owner->GetLevel();
				const int mobLevel = candidate->GetLevel();
				const int levelDelta = mobLevel - botLevel;
				const bool isQuestTarget = (candidate->IsMonster() &&
						IsPlayerBotBiologistHuntRace(m_desiredMobVnum, candidate->GetRaceNum())) ||
						IsPlayerBotHorseTrialTarget(m_owner, candidate);
				const bool isBestialWeaponTarget = candidate->IsMonster() &&
						m_huntM2Bestials &&
						(candidate->GetRaceNum() == 533 || candidate->GetRaceNum() == 534);

				// Is this monster worth fighting at all?
				//
				// This used to be "do not cross a field for obsolete prey, but
				// kill a weaker mob already on the route" - which let any very
				// weak monster inside PLAYERBOT_LOCAL_CHAIN_RANGE into the
				// ranking, because standing nearby is not evidence of being
				// worth killing. Worse, the test ran before the drop this
				// monster carries was even looked at, so a genuine material
				// target could be thrown out before anything recognised it as
				// one.
				//
				// Both are now the policy's business: it asks for a reason -
				// bounded defence, an active hunt, a material or piece the bot
				// actually needs, or enough of the base experience surviving the
				// level difference - and the wanted drop is worked out first, so
				// it can be that reason. Stones keep their own rules entirely.
				if (candidate->IsMonster() && m_pState)
				{
					const playerbot_combat_value::Decision decision =
							DecidePlayerBotCombatValue(m_owner, candidate, *m_pState,
									true, m_desiredMobVnum, m_huntM2Bestials,
									m_pWantedDrops, m_dwNow);
					if (!decision.allowed)
						return false;
				}

				// Component reachability lets the bot route around a wall while still
				// rejecting monsters on disconnected islands or terrain components.
				if (!IsPlayerBotReachable(m_owner->GetMapIndex(), m_owner->GetX(), m_owner->GetY(), candidate->GetX(), candidate->GetY()))
				{
					if (candidate->GetVictim() != m_owner)
						return false;
				}

				TTargetCandidate tc;
				tc.dwVID = candidate->GetVID();
				tc.distance = distance;
				tc.level = mobLevel;
				tc.bIsStone = candidate->IsStone();
				tc.bPriorityObjective = isQuestTarget || isBestialWeaponTarget;

				int baseScore = 0;
				TPlayerBotAIStateMap::iterator sit = s_mapPlayerBotAIStates.find(m_owner->GetPlayerID());
				const bool isMetinHunter = (sit != s_mapPlayerBotAIStates.end() &&
						IsPlayerBotMetinHunting(sit->second, m_dwNow));

				if (candidate->IsStone())
				{
					// A stone is the game's key fight for every bot, not a hunter's
					// speciality (Tieru, 16 September): above the sweet-spot monster
					// for anybody, and one somebody is already on comes first of all.
					baseScore = isMetinHunter ? 1500000 : PLAYERBOT_STONE_BASE_SCORE;
					if (IsPlayerBotStoneUnderJoinableAttack(m_owner, candidate))
						baseScore += PLAYERBOT_STONE_JOIN_BONUS;
				}
				else
				{
					// An active research task is a real alternative to generic levelling.
					// It must outrank a convenient nearby pack, otherwise an over-levelled
					// bot would never return to the alpha wolves required by early quests.
					if (isQuestTarget)
						baseScore += 1800000;
					if (isBestialWeaponTarget)
						baseScore += 1750000;

					// If mob is attacking the bot, give high defense priority
					if (candidate->GetVictim() == m_owner)
					{
						baseScore += 500000;
					}

					if (m_partyChallengeMaxLevel > 0 &&
							mobLevel > botLevel + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
							mobLevel <= m_partyChallengeMaxLevel)
					{
						baseScore += 1200000 + mobLevel * 1000;
					}
					// A boss the raid has already set out for outranks the trash
					// round her. Within the level delta she scored as any other
					// far-off monster - delta twelve is the ten-thousand bucket -
					// so the raiders who reached the Spider Queen fought her
					// soldiers beside her. See PLAYERBOT_RAID_SWARM_MIN.
					if (candidate->GetMobRank() >= MOB_RANK_BOSS &&
							CountPlayerBotRaiders(candidate->GetRaceNum(), m_dwNow) >=
								PLAYERBOT_RAID_SWARM_MIN)
						baseScore += PLAYERBOT_RAID_SWARM_TARGET_BONUS;

					// For dedicated Metin breakers, normal mobs get low score unless attacking
					if (isMetinHunter && candidate->GetVictim() != m_owner)
					{
						baseScore += 5000;
					}
					else
					{
						// Sweet spot: mob level within [-2, +5] of bot level gets HUGE priority
						if (levelDelta >= -2 && levelDelta <= 5)
							baseScore += 300000 + (10 - abs(levelDelta)) * 5000;
						else if (levelDelta > 5 && levelDelta <= 9)
							baseScore += 150000;
						else if (levelDelta < -2 && levelDelta >= -5)
							baseScore += 50000;
						else if (levelDelta < -5)
							baseScore += 5000; // Low score for dogs when high level, but allows killing them along the way
						else
							baseScore += 10000;

						// Priority on appropriate level hunting mobs (Bears, Tigers, White Oath for Lv 10+)
						const DWORD raceVnum = candidate->GetRaceNum();
						if ((botLevel <= 5 && (raceVnum == 101 || raceVnum == 102 || raceVnum == 103)) ||
							(botLevel >= 6 && botLevel <= 10 && (raceVnum == 104 || raceVnum == 106 || raceVnum == 107 || raceVnum == 108 || raceVnum == 109)) ||
							(botLevel >= 11 && ((raceVnum >= 110 && raceVnum <= 115) ||
								(raceVnum >= 139 && raceVnum <= 142) || (raceVnum >= 180 && raceVnum <= 183) ||
								(raceVnum >= 301 && raceVnum <= 394))))
						{
							baseScore += 80000; // Extra focus on hunting mobs!
						}

						// If another player/bot is already fighting this normal mob, spread out to unengaged mobs
						if (candidate->GetVictim() != NULL && candidate->GetVictim() != m_owner)
						{
							baseScore -= 180000;
						}
					}
				}

				// A monster that drops what the bot is short of is worth walking past
				// its neighbours for. This is what turns "needs an Orc Amulet" into
				// killing orcs: the material errand used to end at the market, and a
				// market where nobody kills orcs has no amulets on it.
				tc.bWantedDrop = false;
				if (!tc.bIsStone && m_pWantedDrops && !m_pWantedDrops->empty())
				{
					const DWORD drop = candidate->GetMobDropItemVnum();
					if (drop != 0 && m_pWantedDrops->find(drop) != m_pWantedDrops->end())
					{
						tc.bWantedDrop = true;
						baseScore += PLAYERBOT_WANTED_DROP_BONUS;
					}
				}

				// Distance penalty: only 2 points per unit so level-appropriate mobs within 2000 distance beat low-level dogs
				tc.score = baseScore - (distance * 2);
				m_targets.push_back(tc);

				return true;
			}

			void Sort()
			{
				std::sort(m_targets.begin(), m_targets.end());
			}

			const std::vector<TTargetCandidate>& GetTargets() const { return m_targets; }

		private:
			LPCHARACTER m_owner;
			int m_maxDistance;
			int m_maxLevel;
			int m_partyChallengeMaxLevel;
			DWORD m_desiredMobVnum;
			DWORD m_dwAvoidVID;
			long m_lAvoidX;
			long m_lAvoidY;
			int m_avoidRadius;
			const std::map<DWORD, DWORD>& m_failedStones;
			const std::map<DWORD, DWORD>& m_failedTargets;
			DWORD m_dwNow;
			bool m_huntM2Bestials;
			const std::set<DWORD>* m_pWantedDrops;
			// The bot's own state, for the combat value policy: whether it is
			// retreating, whether an errand is actually under way, and the
			// bounds of any self-defence episode.
			const TPlayerBotAIState* m_pState;
			std::vector<TTargetCandidate> m_targets;
			int m_iMonstersNear;
			int m_iMonsterLevelSum;
	};

	// Worth the swing, or scenery? See PLAYERBOT_TRIVIAL_LEVEL_GAP. Stones are
	// never scenery - a Metin is judged by its own rules elsewhere.
	bool IsPlayerBotWorthwhilePrey(LPCHARACTER ch, int iMobLevel, bool bIsStone)
	{
		if (!ch || bIsStone)
			return true;
		return iMobLevel + PLAYERBOT_TRIVIAL_LEVEL_GAP >= (int)ch->GetLevel();
	}

	// The nearest living monster on the map whose DROP_ITEM is one of the
	// wanted materials. Runs over a snapshot of every entity on the map, which
	// is why the caller rations it. The same pass keeps the nearest of the
	// bot's collect-row family (huntMob, IsPlayerBotBiologistHuntRace), which
	// the caller walks to first.
	class CFindPlayerBotWantedDrop
	{
		public:
			CFindPlayerBotWantedDrop(LPCHARACTER seeker, const std::set<DWORD>& wanted,
					int maxDistance, DWORD huntMob = 0)
				: m_seeker(seeker), m_wanted(wanted), m_maxDistance(maxDistance),
				  m_best(NULL), m_bestDistance(INT_MAX), m_dropVnum(0),
				  m_huntMob(huntMob), m_bestHunt(NULL), m_bestHuntDistance(INT_MAX)
			{
			}

			void operator()(LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER mob = static_cast<LPCHARACTER>(entity);
				if (!mob->IsMonster() || mob->IsDead() || mob->IsStone())
					return;
				// Not an archer of the trial the bot may not walk up to on its
				// own: the snake archer is fifty-one, sixteen levels over a bot
				// of thirty-five, and the walk would end beside a monster the
				// target search refuses.
				if (m_huntMob != 0 && IsPlayerBotBiologistHuntRace(m_huntMob, mob->GetRaceNum()) &&
						!(IsPlayerBotBattleHorseTrialMob(mob->GetRaceNum()) &&
							mob->GetLevel() > m_seeker->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA))
				{
					const int huntDistance = DISTANCE_APPROX(m_seeker->GetX() - mob->GetX(),
							m_seeker->GetY() - mob->GetY());
					if (huntDistance <= m_maxDistance && huntDistance < m_bestHuntDistance)
					{
						m_bestHunt = mob;
						m_bestHuntDistance = huntDistance;
					}
				}
				const DWORD drop = mob->GetMobDropItemVnum();
				if (drop == 0 || m_wanted.find(drop) == m_wanted.end())
					return;
				const int distance = DISTANCE_APPROX(m_seeker->GetX() - mob->GetX(),
						m_seeker->GetY() - mob->GetY());
				if (distance > m_maxDistance || distance >= m_bestDistance)
					return;
				m_best = mob;
				m_bestDistance = distance;
				m_dropVnum = drop;
			}

			LPCHARACTER m_seeker;
			const std::set<DWORD>& m_wanted;
			int m_maxDistance;
			LPCHARACTER m_best;
			int m_bestDistance;
			DWORD m_dropVnum;
			DWORD m_huntMob;
			LPCHARACTER m_bestHunt;
			int m_bestHuntDistance;
	};

	// The material errand's second half. Preferring the right monster among the
	// ones in sight only helps if one is in sight; a bot short of an Orc Amulet
	// on the wrong island of Orc Valley has nothing to prefer. So when the bot
	// has nothing to fight, and a material is wanted, and the scan is due, it
	// looks across the whole map for the nearest monster that carries it and
	// walks that way. Returns true when it set off, claiming the tick.
	DWORD s_dwMaterialScanStamp = 0;
	int s_iMaterialScansThisTick = 0;

	bool StartPlayerBotMaterialHunt(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// A dropper is not sent across the map after a refine material: the
		// medal droppers took 124 of these errands in their first twenty-five
		// minutes after a restart, the first two after a tiger thirty-three and
		// a bear fourteen thousand units away.
		if (!ch || IsPlayerBotDropper(state.bPersonality))
			return false;
		if (state.dwNextMaterialScanTime == 0)
		{
			// The first scan lands somewhere inside the interval, by pid, or
			// every bot in the world takes its snapshot in the same second after a
			// restart. Eight hundred and fifty of them did, once.
			state.dwNextMaterialScanTime = dwNow + 1 +
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d415453U) %
					PLAYERBOT_MATERIAL_SCAN_INTERVAL;
			return false;
		}
		if (dwNow < state.dwNextMaterialScanTime)
			return false;
		if (ch->GetParty() && ch->GetParty()->GetLeaderCharacter() != ch)
			return false;
		// Not on a map this bot has outgrown, and not while it is meant to be
		// leaving: a level-61 Metin hunter back in Bokjung for a weapon spent
		// seven minutes circling the spawns after a Black Wind Yak-To for a
		// refine material before it walked to the Teleporter - "kolka po m2
		// po spotach zbierajac itemy po innych botach". The material is still
		// wanted; it is found where the bot is going to hunt, not on the way
		// out of town.
		if (!IsPlayerBotGrindAllowedHere(ch) || state.lDepartureMap != 0)
			return false;

		// Nothing wanted is the common case and costs a walk over the bag, not
		// over the map, so it is settled before the tick's scan budget is asked.
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		// A collect row's monsters are wanted the same way (the valley's and
		// the tower's; a herb row's hubs are already chosen for its level).
		// Bots of seventy on the Orc Tooth row chose a band hub every thirty
		// seconds and found nothing in reach: 51 of 86 bots in the valley read
		// "Szukam celu dla grupy" (m2zip, 17 September).
		DWORD huntMob = GetPlayerBotDesiredQuestMobVnum(ch, state, dwNow);
		if (huntMob < 500)
			huntMob = 0;
		if (wanted.empty() && huntMob == 0)
		{
			state.dwNextMaterialScanTime = dwNow + PLAYERBOT_MATERIAL_SCAN_INTERVAL;
			return false;
		}
		if (s_dwMaterialScanStamp != dwNow)
		{
			s_dwMaterialScanStamp = dwNow;
			s_iMaterialScansThisTick = 0;
		}
		if (s_iMaterialScansThisTick >= PLAYERBOT_MATERIAL_SCANS_PER_TICK)
			return false; // budget spent; the time is not consumed, so next tick
		++s_iMaterialScansThisTick;
		++s_uPlayerBotLoadScans;
		TPlayerBotLoadTimer scanTimer(s_uPlayerBotLoadScanUs);
		state.dwNextMaterialScanTime = dwNow + PLAYERBOT_MATERIAL_SCAN_INTERVAL;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex());
		if (!map)
			return false;

		CFindPlayerBotWantedDrop finder(ch, wanted, PLAYERBOT_MATERIAL_HUNT_RANGE, huntMob);
		map->for_each(finder);
		// The row's family first. "In the scan" is the target search's own
		// radius, which for a bot in a party is the cohesion radius.
		const int inScan = ch->GetParty() ? PLAYERBOT_PARTY_COHESION_RADIUS : PLAYERBOT_SEARCH_RANGE;
		if (finder.m_bestHunt && finder.m_bestHuntDistance > inScan)
		{
			// "Zbieram dla Biologa", not a party looking for something to do -
			// or, on the battle trial, the travel whose status is the trial's.
			const bool trial = IsPlayerBotBattleHorseTrialMob(huntMob);
			SetPlayerBotAction(state, trial ? BOT_ACTION_TRAVEL : BOT_ACTION_BIOLOGIST, dwNow);
			sys_log(0, "PLAYERBOT_HUNT: %s errand pid=%u name=%s map=%ld hunt=%u mob=%s distance=%d pos=(%ld,%ld)",
					trial ? "horse trial" : "biologist",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), huntMob,
					finder.m_bestHunt->GetName(), finder.m_bestHuntDistance,
					finder.m_bestHunt->GetX(), finder.m_bestHunt->GetY());
			MovePlayerBot(ch, finder.m_bestHunt->GetX(), finder.m_bestHunt->GetY(), dwNow, 24, true, true);
			state.dwNextWanderTime = dwNow + 8000;
			// Kept past the next fight on the way (ManagePlayerBotWandering).
			state.lBiologistWalkMap = ch->GetMapIndex();
			state.lBiologistWalkX = finder.m_bestHunt->GetX();
			state.lBiologistWalkY = finder.m_bestHunt->GetY();
			state.dwBiologistWalkUntil = dwNow + PLAYERBOT_BIOLOGIST_WALK_STICK_MS;
			return true;
		}
		if (finder.m_bestHunt)
			return false; // one is in reach: the target search takes it
		if (!finder.m_best || finder.m_bestDistance <= PLAYERBOT_SEARCH_RANGE)
			return false; // nothing carries it here, or it is already in the scan

		state.dwMaterialHuntVnum = finder.m_dropVnum;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		sys_log(0, "PLAYERBOT_HUNT: material errand pid=%u name=%s map=%ld wants=%u mob=%s distance=%d pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), finder.m_dropVnum,
				finder.m_best->GetName(), finder.m_bestDistance,
				finder.m_best->GetX(), finder.m_best->GetY());
		MovePlayerBot(ch, finder.m_best->GetX(), finder.m_best->GetY(), dwNow, 24, true, true);
		state.dwNextWanderTime = dwNow + 8000;
		return true;
	}

	LPCHARACTER FindDistributedTarget(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return NULL;

		// Clean up expired failed stone entries
		if (!state.mapFailedStones.empty())
		{
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedStones.begin();
					it != state.mapFailedStones.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedStones.erase(it++);
				else
					++it;
			}
		}

		if (!state.mapFailedTargets.empty())
		{
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedTargets.begin();
					it != state.mapFailedTargets.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedTargets.erase(it++);
				else
					++it;
			}
		}

		// If bot died recently (< 60 seconds ago), be cautious:
		// 1) Avoid the killer monster VID and death pack zone (800 range around death location).
		// 2) Lower max target level to at most (bot level - 1) to hunt easier, safer mobs.
		const bool bRecentDeath = (state.dwLastDeathTime != 0 && (dwNow - state.dwLastDeathTime < 60000));
		const DWORD dwAvoidVID = bRecentDeath ? state.dwLastKillerVID : 0;
		const long lAvoidX = bRecentDeath ? state.lDeathX : 0;
		const long lAvoidY = bRecentDeath ? state.lDeathY : 0;
		const int avoidRadius = bRecentDeath ? 800 : 0;

		int maxLevel = ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA;
		int partyChallengeMaxLevel = 0;
		TPlayerBotPartyStrength partyStrength;
		if (!bRecentDeath && ch->GetParty() && ch->GetParty()->GetLeaderCharacter() == ch &&
				GetPlayerBotPartyStrength(ch, dwNow, partyStrength))
		{
			partyChallengeMaxLevel = partyStrength.iChallengeMaxLevel;
			maxLevel = std::max(maxLevel, partyChallengeMaxLevel);
		}
		if (bRecentDeath)
		{
			maxLevel = std::max(1, ch->GetLevel() - 1);
		}

		const DWORD desiredQuestMobVnum =
				GetPlayerBotDesiredQuestMobVnum(ch, state, dwNow);

		const int targetSearchRange = ch->GetParty()
				? PLAYERBOT_PARTY_COHESION_RADIUS : PLAYERBOT_SEARCH_RANGE;
		CCollectPlayerBotTargets collector(ch, targetSearchRange, maxLevel,
				partyChallengeMaxLevel, desiredQuestMobVnum, dwAvoidVID,
				lAvoidX, lAvoidY, avoidRadius, state.mapFailedStones,
				state.mapFailedTargets, dwNow);
		std::set<DWORD> wantedDrops;
		CollectPlayerBotWantedMaterials(ch, wantedDrops);
		collector.SetWantedDrops(&wantedDrops);
		collector.SetState(&state);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		RememberPlayerBotSpotSighting(ch->GetMapIndex(), ch->GetX(), ch->GetY(),
				collector.MonstersNear(), collector.MonsterLevelSum(), dwNow);

		std::vector<TTargetCandidate> targets = collector.GetTargets();

		// Fallback: If no safer/lower level targets found in range, allow normal level cap but still avoid exact killer
		if (targets.empty() && bRecentDeath)
		{
			CCollectPlayerBotTargets fallbackCollector(ch, targetSearchRange,
					ch->GetLevel(), 0, desiredQuestMobVnum, dwAvoidVID, 0, 0, 0,
					state.mapFailedStones, state.mapFailedTargets, dwNow);
			ch->GetSectree()->ForEachAround(fallbackCollector);
			fallbackCollector.Sort();
			targets = fallbackCollector.GetTargets();
		}

		if (targets.empty())
			return NULL;

		// A ready leader does not roll the party challenge together with ordinary
		// mobs. Pick the best unclaimed elite deterministically; party-to-party
		// claim separation still distributes multiple groups over different elites.
		if (partyChallengeMaxLevel > 0)
		{
			for (size_t i = 0; i < targets.size(); ++i)
			{
				if (targets[i].level > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
						targets[i].level <= partyChallengeMaxLevel &&
						!IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
					return CHARACTER_MANAGER::instance().Find(targets[i].dwVID);
			}
		}

		// Iwakura's stone hunter: a stone in sight that the collector let
		// through (its band, not failed, one the bot can break) and that its
		// own kingdom has not crowded comes before anything at the bot's feet
		// ("rzuca wszystko ... i pedzi prosto do kamienia"). The candidates are
		// sorted, so the first such stone is the best one. A party is the
		// party's to aim (FindPlayerBotPartyFocusTarget) and is left out.
		if (IsPlayerBotPersonaEnabled() && !ch->GetParty())
		{
			for (size_t i = 0; i < targets.size(); ++i)
				if (targets[i].bIsStone && !IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
					return CHARACTER_MANAGER::instance().Find(targets[i].dwVID);
		}

		// Chain ordinary combat into the closest unclaimed pack. A nearby quest or
		// Bestial objective wins over generic prey, but a far-away objective no longer
		// makes the bot walk past mobs at its feet. Metin hunters retain their global
		// stone scoring and reservations below.
		if (!IsPlayerBotMetinHunting(state, dwNow))
		{
			// Twice: the first pass will not look at anything too far beneath the
			// bot to be worth a swing, the second takes whatever is here. A quest
			// objective is always worth it, whatever its level - that is what the
			// errand is for.
			DWORD chainedVID = 0;
			// Three passes now: first the monsters that drop something the bot
			// is short of, then the ones worth a swing, then anything.
			for (int pass = 0; pass < 3 && chainedVID == 0; ++pass)
			{
				const bool bWantedOnly = (pass == 0);
				const bool bWorthwhileOnly = (pass <= 1);
				DWORD closestObjectiveVID = 0;
				DWORD closestLocalVID = 0;
				int closestObjectiveDistance = INT_MAX;
				int closestLocalDistance = INT_MAX;
				for (size_t i = 0; i < targets.size(); ++i)
				{
					if (targets[i].bIsStone || targets[i].distance > PLAYERBOT_LOCAL_CHAIN_RANGE ||
							IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
						continue;
					if (targets[i].bPriorityObjective &&
							targets[i].distance < closestObjectiveDistance)
					{
						closestObjectiveDistance = targets[i].distance;
						closestObjectiveVID = targets[i].dwVID;
					}
					if (bWantedOnly && !targets[i].bWantedDrop)
						continue;
					if (bWorthwhileOnly && !IsPlayerBotWorthwhilePrey(
							ch, targets[i].level, targets[i].bIsStone))
						continue;
					if (targets[i].distance < closestLocalDistance)
					{
						closestLocalDistance = targets[i].distance;
						closestLocalVID = targets[i].dwVID;
					}
				}
				chainedVID = closestObjectiveVID != 0
						? closestObjectiveVID : closestLocalVID;
			}
			if (chainedVID != 0)
				return CHARACTER_MANAGER::instance().Find(chainedVID);
		}

		std::vector<DWORD> availableTargets;
		std::vector<DWORD> worthwhileTargets;
		std::vector<DWORD> wantedTargets;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			if (IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
				continue;
			availableTargets.push_back(targets[i].dwVID);
			if (IsPlayerBotWorthwhilePrey(ch, targets[i].level, targets[i].bIsStone))
				worthwhileTargets.push_back(targets[i].dwVID);
			if (targets[i].bWantedDrop)
				wantedTargets.push_back(targets[i].dwVID);
		}
		// Same rule for the wider pick, same fallback: a map that holds nothing
		// but monsters a bot has outgrown still gives it something to do. And
		// the same order: what it needs, then what is worth it, then anything.
		if (!wantedTargets.empty())
			availableTargets.swap(wantedTargets);
		else if (!worthwhileTargets.empty())
			availableTargets.swap(worthwhileTargets);

		// Prefer a target no other bot has claimed. Randomizing inside a bounded
		// nearest-candidate window spreads bots without sending them across the map.
		//
		// An empty pool now means there is nothing here worth fighting, and that
		// is an answer. It used to fall back to the unfiltered list - which
		// handed back exactly the monsters the filter had just refused, and
		// would have made the policy above decorative. The caller has somewhere
		// to go with "no target": the wandering pass moves the bot on, and the
		// travel pass reconsiders the map.
		if (availableTargets.empty())
			return NULL;
		const size_t choiceCount = std::min(availableTargets.size(),
				PLAYERBOT_TARGET_CHOICE_WINDOW);
		const size_t choiceIndex = (size_t)number(0, (int)choiceCount - 1);

		return CHARACTER_MANAGER::instance().Find(availableTargets[choiceIndex]);
	}

	class CCollectPlayerBotMeleeTargets
	{
		public:
			CCollectPlayerBotMeleeTargets(LPCHARACTER owner, LPCHARACTER primary) :
				m_owner(owner),
				m_primaryVID(primary->GetVID()),
				m_dirX((float)(primary->GetX() - owner->GetX())),
				m_dirY((float)(primary->GetY() - owner->GetY()))
			{
				// The direction of the blow is the line to what is being struck,
				// not GetRotation(): the rotation is only set at the end of this
				// swing, so on the first one of a fight it still points wherever
				// the bot last walked.
				const float length = sqrtf(m_dirX * m_dirX + m_dirY * m_dirY);
				if (length > 0.0f)
				{
					m_dirX /= length;
					m_dirY /= length;
				}
				else
					m_dirX = m_dirY = 0.0f;
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				// A sweep that grazes a Demon Tower stone can break it, and the
				// kill is the bot's: see PLAYERBOT_DEVIL_TOWER_STONE_FIRST - unless
				// the bot is climbing with a player, for whom that is the point.
				if (candidate == m_owner || candidate->GetVID() == m_primaryVID ||
						(!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead() ||
						(candidate->IsStone() && IsPlayerBotDungeonTriggerStone(candidate->GetRaceNum()) &&
							!IsPlayerBotDungeonStoneObjective(m_owner, candidate)))
					return false;

				if (candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()) ||
						(candidate->IsMonster() && candidate->GetLevel() > m_owner->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA))
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > PLAYERBOT_MELEE_SPLASH_RANGE || !IsInFrontOfTheBlow(candidate))
					return true;

				m_targets.push_back(std::make_pair(distance, candidate->GetVID()));
				return true;
			}

			void Sort()
			{
				std::sort(m_targets.begin(), m_targets.end());
			}

			const std::vector<std::pair<int, DWORD> >& GetTargets() const { return m_targets; }

		private:
			// The arc of the swing, as the client's collision spheres describe it
			// for a player: the cosine of the angle between the blow and this
			// candidate, against PLAYERBOT_MELEE_SPLASH_FACING_DOT. A candidate
			// standing on the bot answers yes - the blow lands on whatever is
			// inside the body whichever way it turns.
			bool IsInFrontOfTheBlow(LPCHARACTER candidate) const
			{
				if (m_dirX == 0.0f && m_dirY == 0.0f)
					return true;

				float toX = (float)(candidate->GetX() - m_owner->GetX());
				float toY = (float)(candidate->GetY() - m_owner->GetY());
				const float length = sqrtf(toX * toX + toY * toY);
				if (length <= 0.0f)
					return true;

				toX /= length;
				toY /= length;
				return (m_dirX * toX + m_dirY * toY) >= PLAYERBOT_MELEE_SPLASH_FACING_DOT;
			}

			LPCHARACTER m_owner;
			DWORD m_primaryVID;
			float m_dirX;
			float m_dirY;
			std::vector<std::pair<int, DWORD> > m_targets;
	};

	DWORD AttackPlayerBotMeleeGroup(LPCHARACTER ch, LPCHARACTER primary)
	{
		if (!ch || !primary || !ch->GetSectree())
			return 0;

		// A duel opponent is a character, and this is the one place where a swing
		// becomes damage. Opening the guard in ExecutePlayerBotBasicAttack was
		// half the job: eleven duellists closed to between twenty-three and
		// ninety-two units of one another, played the whole combo animation -
		// which is all SendPlayerBotAttackPacket does - and not one of them lost
		// a single point of health, because this function returned zero before
		// Damage was ever called.
		//
		// And only a foe the engine agrees may be struck: Damage asks nothing
		// by itself, so opening this door without that question is what made
		// duel kills count as murders - see CanPlayerBotStrikeCharacter.
		// A war foe and the Anti-PK protocol's foe the same way as a duellist
		// (IsPlayerBotSanctionedFoe): until 2.0.95 this door opened for the
		// duel alone, so a swing at either played its combo and hurt nobody.
		const bool bIsDuel = !primary->IsMonster() && !primary->IsStone() &&
				IsPlayerBotSanctionedFoe(ch, primary, get_dword_time()) &&
				CanPlayerBotStrikeCharacter(ch, primary);
		const bool bIsTargetValid = (primary->IsMonster() || primary->IsStone() || bIsDuel);
		if (!bIsTargetValid || primary->IsDead())
			return 0;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW);
		LPITEM arrow = NULL;
		// An empty arrow slot is filled from the bag before the shot is
		// refused: seven archers of the test world stood with a bow, nothing
		// in the arrow slot and a thousand arrows in the bag, and only the
		// skill path put them back (16 September). No arrow anywhere, no shot -
		// the engine's GetArrowAndBow is the rule for a player too. The arrow
		// is only shown, never spent: see below.
		if (isBow && (!EnsurePlayerBotArrowsEquipped(ch) ||
				ch->GetArrowAndBow(&weapon, &arrow, 1) != 1))
			return 0;

		// What the game says this blow is worth, and nothing else. The invented
		// figure that used to stand here - number(15, 35) + level * 4, so about
		// three hundred for a bot of seventy whatever it held - made a weak
		// weapon hit as hard as a good one, and no equipment decision below it
		// could be read from the outside. The engine has a floor of its own and
		// it is small: CalcBattleDamage ends in `if (iDam < 3) iDam =
		// number(1, 5)`, which a player gets too.
		const int iDamage = isBow ? CalcArrowDamage(ch, primary, weapon, arrow, false)
				: CalcMeleeDamage(ch, primary, false, false);

		DWORD hitCount = 1;
		primary->Damage(ch, iDamage, DAMAGE_TYPE_NORMAL);
		// No UseArrow: a bot's quiver never empties (Tieru, 24 September), so an
		// Archer does not walk to town for arrows every half hour. The skill
		// path in playerbot_combat.h does the same.
		primary->SetSyncOwner(ch);
		if (!primary->IsDead() && primary->CanBeginFight())
			primary->BeginFight(ch);

		// The sweep stays off a duel: it is a fight between two characters, and
		// the monsters standing round them are nobody's business here - the
		// collector keeps them out by itself, but sweeping at all would let a
		// duel drag bystanders in the moment that ever changed.
		if (!isBow && !bIsDuel)
		{
			CCollectPlayerBotMeleeTargets collector(ch, primary);
			ch->GetSectree()->ForEachAround(collector);
			collector.Sort();

			const std::vector<std::pair<int, DWORD> >& targets = collector.GetTargets();
			for (size_t i = 0; i < targets.size() && hitCount < PLAYERBOT_MAX_MELEE_TARGETS; ++i)
			{
				LPCHARACTER secondary = CHARACTER_MANAGER::instance().Find(targets[i].second);
				if (!secondary || secondary->IsDead() || (!secondary->IsMonster() && !secondary->IsStone()))
					continue;

				secondary->Damage(ch, CalcMeleeDamage(ch, secondary, false, false),
						DAMAGE_TYPE_NORMAL);
				++hitCount;
			}
		}

		if (!primary->IsDead())
		{
			ch->SetVictim(primary);
			ch->SetRotationToXY(primary->GetX(), primary->GetY());
		}

		return hitCount;
	}

	// The same from the saddle: share/data/pc/<class>/horse_<weapon>/combo_NN.msa,
	// three steps, not four, and each its own DirectInputTime. The on-foot table
	// was used for a rider too, so a warrior on its battle horse hacking a stone
	// sent the fourth swing the horse set has no motion for, and the next one
	// early: the rider's combo never played through. Zero falls back to the
	// on-foot figure; the bow has no saddle set and never fights from one.
	const DWORD PLAYERBOT_HORSE_SWING_MS[4][6][3] = {
		{ // warrior
			{ 777, 530, 511 },    // onehand_sword
			{ 701, 514, 534 },    // dualhand: no set, the two-handed one
			{   0,   0,   0 },    // bow
			{ 701, 514, 534 },    // twohand_sword
			{   0,   0,   0 },    // bell
			{   0,   0,   0 },    // fan
		},
		{ // assassin
			{ 792, 400, 499 },    // onehand_sword
			{ 676, 557, 524 },    // dualhand_sword
			{   0,   0,   0 },    // bow
			{ 676, 557, 524 },    // twohand: no set, the dagger one
			{   0,   0,   0 },    // bell
			{   0,   0,   0 },    // fan
		},
		{ // sura
			{ 792, 479, 491 },    // onehand_sword
			{ 792, 479, 491 },    // dualhand: no set
			{   0,   0,   0 },    // bow
			{ 792, 479, 491 },    // twohand: no set
			{   0,   0,   0 },    // bell
			{   0,   0,   0 },    // fan
		},
		{ // shaman
			{   0,   0,   0 },    // onehand_sword
			{   0,   0,   0 },    // dualhand_sword
			{   0,   0,   0 },    // bow
			{   0,   0,   0 },    // twohand_sword
			{ 730, 431, 440 },    // bell
			{ 982, 679, 775 },    // fan
		},
	};

	// The pause a swing needs before the next one, in milliseconds. The table is
	// measured at attack speed 100; the client plays the motion faster as that
	// rises, so the window moves with it.
	DWORD GetPlayerBotSwingInterval(LPCHARACTER ch, BYTE comboMotion)
	{
		if (!ch)
			return PLAYERBOT_SWING_MS_FALLBACK;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const BYTE subType = (weapon && weapon->GetType() == ITEM_WEAPON)
				? weapon->GetSubType() : (BYTE)WEAPON_SWORD;
		// GetJob carries the gender as well; the motion set does not.
		const BYTE job = (BYTE)(ch->GetJob() % 4);
		DWORD base = PLAYERBOT_SWING_MS_FALLBACK;
		if (job < 4 && subType < 6)
		{
			const BYTE step = (comboMotion >= MOTION_COMBO_ATTACK_1 &&
					comboMotion <= MOTION_COMBO_ATTACK_4)
					? (BYTE)(comboMotion - MOTION_COMBO_ATTACK_1) : (BYTE)0;
			DWORD found = PLAYERBOT_SWING_MS[job][subType][step];
			if (ch->IsRiding() && step < 3 && PLAYERBOT_HORSE_SWING_MS[job][subType][step] > 0)
				found = PLAYERBOT_HORSE_SWING_MS[job][subType][step];
			if (found > 0)
				base = found;
		}
		int attSpeed = ch->GetPoint(POINT_ATT_SPEED);
		if (attSpeed <= 0)
			attSpeed = 100;
		const DWORD scaled = (base * 100U) / (DWORD)attSpeed;
		// Even a heavily buffed character cannot outrun its own animation by much;
		// the floor stops an extreme attack speed turning into a packet storm.
		return scaled < 200U ? 200U : scaled;
	}

	bool ExecutePlayerBotBasicAttack(LPCHARACTER ch, LPCHARACTER target,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || ch->IsDead() || target->IsDead() ||
				state.bVisitingShop || state.bRecoveringAfterDeath ||
				// A duel opponent, a war foe and the Anti-PK protocol's foe are
				// the characters a bot may swing at (IsPlayerBotSanctionedFoe).
				// Every other road to a target asks for a monster or a stone,
				// which is exactly why an agreed duel used to end in the two of
				// them standing and looking at one another - and why a guild war
				// was fought with skills alone until 2.0.95.
				(!target->IsMonster() && !target->IsStone() &&
					(!IsPlayerBotSanctionedFoe(ch, target, dwNow) ||
					 !CanPlayerBotStrikeCharacter(ch, target))) ||
				ch->GetMapIndex() != target->GetMapIndex() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(target->GetMapIndex(), target->GetX(), target->GetY()))
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		if (!weapon || weapon->GetType() != ITEM_WEAPON)
			return false;

		const bool isBow = weapon->GetSubType() == WEAPON_BOW;
		if (isBow)
		{
			LPITEM bow = NULL;
			LPITEM arrow = NULL;
			// Nocked from the bag before the shot is refused. The blow itself
			// nocks too (AttackPlayerBotMeleeGroup, 16 September), but it is
			// only reached once this test has passed, so an emptied quiver
			// stopped every plain shot until a skill - whose path nocks - came
			// off its cooldown ("archer podczas PVP stoi w miejscu i nie auto
			// atakuje", prodnathin, 24 September).
			if (!EnsurePlayerBotArrowsEquipped(ch) || ch->GetArrowAndBow(&bow, &arrow, 1) != 1)
				return false;
		}
		const int combatRange = isBow ? GetPlayerBotBowRange(ch->GetMapIndex()) : 280;
		if (DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY()) > combatRange)
			return false;

		// The light combat pass must never interrupt navigation.  The full AI pass
		// stops the bot as soon as it reaches combat range; subsequent combo hits
		// can then be emitted on every 250 ms manager tick.
		if (ch->IsStateMove() || dwNow < state.dwNextAttackTime)
			return false;

		// How long this particular swing takes, from the client's own motion data
		// rather than one number for the whole game. The flat 480 ms this replaces
		// was earlier than every weapon in the game will accept: a two-handed sword
		// wants 932 ms and a bow a full second, so both were being cut in half, and
		// the finishing strike of a bell combo - which does not chain at all and
		// has to play whole - was cut worst of any.
		const int hitInterval = (int)GetPlayerBotSwingInterval(ch, state.bComboMotion);

		ch->SetPosition(POS_FIGHTING);
		ch->SetVictim(target);
		ch->SetRotationToXY(target->GetX(), target->GetY());
		state.dwNextAttackTime = dwNow + hitInterval;
		state.dwLastCombatActionTime = dwNow;
		SendPlayerBotAttackPacket(ch, target, state.bComboMotion);
		AttackPlayerBotMeleeGroup(ch, target);

		if (isBow)
			state.bComboMotion = MOTION_COMBO_ATTACK_1;
		else
		{
			++state.bComboMotion;
			// Three swings in the saddle, four on foot: the horse sets stop at
			// combo_03, and a fourth is a motion the client does not have.
			if (state.bComboMotion > (ch->IsRiding() ? MOTION_COMBO_ATTACK_3 : MOTION_COMBO_ATTACK_4))
				state.bComboMotion = MOTION_COMBO_ATTACK_1;
		}
		return true;
	}

	// An Archer Ninja with a bow in hand.
	bool IsPlayerBotArcher(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		return weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW;
	}

	bool IsPlayerBotMultiPullBuild(LPCHARACTER ch, bool* naturalTank)
	{
		if (naturalTank)
			*naturalTank = false;
		if (!ch || ch->GetLevel() < 15 || ch->GetParty() ||
				!IsPlayerBotVillageMap(ch->GetMapIndex()))
			return false;
		// Gathering four packs at once is the largest grind there is, so it
		// answers to the map rule before anything else about the build.
		if (!IsPlayerBotGrindAllowedHere(ch))
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
		// The archer pulls the way it plays on the hard servers: three or four
		// on the string at once. No shield to ask for; the cap on attackers is
		// what keeps it alive.
		if (IsPlayerBotArcher(ch))
			return armor && helmet;
		if (!weapon || !armor || !shield || !helmet ||
				(weapon->GetType() == ITEM_WEAPON &&
				 weapon->GetSubType() == WEAPON_BOW))
			return false;

		const bool isNaturalTank =
				(ch->GetJob() == JOB_WARRIOR && ch->GetSkillGroup() == 2) ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 1);
		const bool isHeavilyArmored = armor->GetRefineLevel() >= 5 &&
				shield->GetRefineLevel() >= 5 && helmet->GetRefineLevel() >= 4;
		if (naturalTank)
			*naturalTank = isNaturalTank;
		return isNaturalTank || isHeavilyArmored;
	}

	class CCountPlayerBotPullAggressors
	{
		public:
			CCountPlayerBotPullAggressors(LPCHARACTER owner) : m_owner(owner), m_count(0) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate != m_owner && candidate->IsMonster() &&
						!candidate->IsDead() && candidate->GetVictim() == m_owner &&
						DISTANCE_APPROX(candidate->GetX() - m_owner->GetX(),
								candidate->GetY() - m_owner->GetY()) <= PLAYERBOT_MULTI_PULL_SEARCH_RANGE)
					++m_count;
				return true;
			}

			int GetCount() const { return m_count; }

		private:
			LPCHARACTER m_owner;
			int m_count;
	};

	int CountPlayerBotPullAggressors(LPCHARACTER ch)
	{
		if (!ch || !ch->GetSectree())
			return 0;
		CCountPlayerBotPullAggressors counter(ch);
		ch->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
	}

	class CFindPlayerBotPullTarget
	{
		public:
			CFindPlayerBotPullTarget(LPCHARACTER owner,
					const std::vector<PIXEL_POSITION>& centers) :
				m_owner(owner), m_centers(centers), m_bestVID(0), m_bestScore(INT_MAX)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || !candidate->IsMonster() || candidate->IsStone() ||
						candidate->IsDead() || candidate->GetVictim() != NULL ||
						candidate->GetMobRank() >= MOB_RANK_BOSS ||
						candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;

				const int minLevel = std::max(1, (int)m_owner->GetLevel() - 3);
				const int maxLevel = (int)m_owner->GetLevel() + 2;
				if (candidate->GetLevel() < minLevel || candidate->GetLevel() > maxLevel)
					return false;

				const int distance = DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > PLAYERBOT_MULTI_PULL_SEARCH_RANGE ||
						!IsPlayerBotReachable(m_owner->GetMapIndex(), m_owner->GetX(), m_owner->GetY(),
								candidate->GetX(), candidate->GetY()) ||
						IsTargetClaimedByAnotherBot(m_owner, candidate->GetVID()))
					return false;

				for (size_t i = 0; i < m_centers.size(); ++i)
				{
					if (DISTANCE_APPROX(candidate->GetX() - m_centers[i].x,
							candidate->GetY() - m_centers[i].y) <
							PLAYERBOT_MULTI_PULL_GROUP_SEPARATION)
						return false;
				}

				// A small deterministic jitter distributes simultaneous tanks without
				// sacrificing the preference for a nearby pack.
				const int score = distance + (int)(PlayerBotNavHash(
						m_owner->GetPlayerID() ^ candidate->GetVID()) % 350U);
				if (score < m_bestScore)
				{
					m_bestScore = score;
					m_bestVID = candidate->GetVID();
				}
				return true;
			}

			DWORD GetBestVID() const { return m_bestVID; }

		private:
			LPCHARACTER m_owner;
			const std::vector<PIXEL_POSITION>& m_centers;
			DWORD m_bestVID;
			int m_bestScore;
	};

	LPCHARACTER FindPlayerBotPullTarget(LPCHARACTER ch,
			const std::vector<PIXEL_POSITION>& centers)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		CFindPlayerBotPullTarget finder(ch, centers);
		ch->GetSectree()->ForEachAround(finder);
		return finder.GetBestVID() != 0
				? CHARACTER_MANAGER::instance().Find(finder.GetBestVID()) : NULL;
	}

	void FinishPlayerBotMultiPull(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		const BYTE pulledGroups = state.bMultiPullGroups;
		const BYTE desiredGroups = state.bMultiPullDesiredGroups;
		state.bMultiPullActive = false;
		state.bMultiPullGroups = 0;
		state.bMultiPullDesiredGroups = 0;
		state.dwMultiPullStartedTime = 0;
		state.dwNextMultiPullActionTime = 0;
		state.dwMultiPullTargetVID = 0;
		state.vecMultiPullCenters.clear();
		state.dwNextMultiPullTime = dwNow + number(
				PLAYERBOT_MULTI_PULL_MIN_COOLDOWN, PLAYERBOT_MULTI_PULL_MAX_COOLDOWN);

		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
		state.dwTargetVID = engaged ? engaged->GetVID() : 0;
		if (engaged)
			ch->SetVictim(engaged);
		else
			ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_PULL: finished pid=%u name=%s groups=%u/%u aggressors=%d hp=%d/%d reason=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)pulledGroups,
				(unsigned int)desiredGroups, CountPlayerBotPullAggressors(ch),
				ch->GetHP(), ch->GetMaxHP(), reason ? reason : "?");
	}

	bool HandlePlayerBotMultiPull(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// A rider on a transport horse is on its way somewhere; the pack it
		// would gather is the target section's to notice, which dismounts.
		if (ch && ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
			return false;
		bool naturalTank = false;
		const bool buildEligible = IsPlayerBotMultiPullBuild(ch, &naturalTank);
		const bool goalEligible = state.bBotRole == BOT_ROLE_MOB_GRINDER &&
				(state.bLongTermGoal == BOT_GOAL_LEVEL_UP ||
				 state.bLongTermGoal == BOT_GOAL_HUNTING);
		const bool recentlyDied = state.dwLastDeathTime != 0 &&
				dwNow - state.dwLastDeathTime < 120000;
		size_t redPots = 0, bluePots = 0;
		CountPlayerBotPotions(ch, redPots, bluePots);
		const int hpPercent = ch && ch->GetMaxHP() > 0
				? ch->GetHP() * 100 / ch->GetMaxHP() : 0;

		if (!buildEligible || !goalEligible || recentlyDied || redPots < 30 ||
				state.bVisitingShop || state.bRecoveringAfterDeath || state.bTacticalRetreat)
		{
			if (state.bMultiPullActive)
				FinishPlayerBotMultiPull(ch, state, dwNow, "eligibility_lost");
			return false;
		}

		if (!state.bMultiPullActive)
		{
			if (state.dwNextMultiPullTime == 0)
			{
				state.dwNextMultiPullTime = dwNow + 5000 +
						PlayerBotNavHash(ch->GetPlayerID() ^ 0x50554c4cU) % 40000U;
				return false;
			}
			LPCHARACTER current = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (dwNow < state.dwNextMultiPullTime || hpPercent < PLAYERBOT_MULTI_PULL_START_HP_PERCENT ||
					(current && !current->IsDead()) || FindPlayerBotEngagedTarget(ch, &state, dwNow))
				return false;

			LPCHARACTER first = FindPlayerBotPullTarget(ch, state.vecMultiPullCenters);
			if (!first)
			{
				state.dwNextMultiPullTime = dwNow + number(10000, 20000);
				return false;
			}

			state.bMultiPullActive = true;
			state.bMultiPullGroups = 0;
			state.bMultiPullDesiredGroups = IsPlayerBotArcher(ch) ? 1 : (naturalTank
					? (BYTE)(2 + PlayerBotNavHash(ch->GetPlayerID() ^
							(dwNow / 60000U)) % 3U) : 2);
			state.dwMultiPullStartedTime = dwNow;
			state.dwNextMultiPullActionTime = dwNow;
			state.iMultiPullStartHPPercent = hpPercent;
			state.dwMultiPullTargetVID = first->GetVID();
			state.dwTargetVID = first->GetVID();
			NotePlayerBotDefenceEpisode(ch, state, first, dwNow);
			state.vecMultiPullCenters.clear();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PULL: started pid=%u name=%s level=%u desired_groups=%u hp=%d/%d natural_tank=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bMultiPullDesiredGroups, ch->GetHP(),
					ch->GetMaxHP(), naturalTank ? 1 : 0);
		}

		const int aggressors = CountPlayerBotPullAggressors(ch);
		const int aggressorCap = IsPlayerBotArcher(ch)
				? PLAYERBOT_MULTI_PULL_ARCHER_MAX_AGGRESSORS : PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS;
		if (hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT ||
				state.iMultiPullStartHPPercent - hpPercent >= PLAYERBOT_MULTI_PULL_MAX_HP_LOSS_PERCENT ||
				aggressors >= aggressorCap ||
				dwNow - state.dwMultiPullStartedTime >= PLAYERBOT_MULTI_PULL_TIMEOUT)
		{
			const char* reason = hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT
					? "low_hp" : (aggressors >= aggressorCap
						? "aggressor_cap" : (dwNow - state.dwMultiPullStartedTime >=
							PLAYERBOT_MULTI_PULL_TIMEOUT ? "timeout" : "hp_loss"));
			FinishPlayerBotMultiPull(ch, state, dwNow, reason);
			return false;
		}

		if (state.bMultiPullGroups >= state.bMultiPullDesiredGroups)
		{
			FinishPlayerBotMultiPull(ch, state, dwNow, "desired_groups_ready");
			return false;
		}

		LPCHARACTER target = state.dwMultiPullTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwMultiPullTargetVID) : NULL;
		if (!target || target->IsDead() || !target->IsMonster() || target->IsStone() ||
				target->GetMapIndex() != ch->GetMapIndex() ||
				(target->GetVictim() != NULL && target->GetVictim() != ch))
		{
			target = FindPlayerBotPullTarget(ch, state.vecMultiPullCenters);
			// A pull is a fresh fight like any other, so it answers to the same
			// policy: a group nobody needs is not pulled just because the bot is
			// already in the middle of pulling.
			if (target && !IsPlayerBotTargetWorthNow(ch, target, state, dwNow))
				target = NULL;
			state.dwMultiPullTargetVID = target ? target->GetVID() : 0;
			state.dwTargetVID = state.dwMultiPullTargetVID;
			ClearPlayerBotRoute(state, true);
			if (!target)
			{
				FinishPlayerBotMultiPull(ch, state, dwNow, "no_fresh_pack");
				return false;
			}
		}

		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		state.dwTargetVID = target->GetVID();
		RememberPlayerBotMapRace(ch, target);
		RememberPlayerBotSpotFight(ch->GetMapIndex(), target->GetX(), target->GetY(), dwNow);
		ch->SetVictim(target);
		// Aggressive packs often wake up as soon as the bot enters their radius. In
		// that case running on is the authentic pull action; attacking would stop to
		// clear the very first pack instead of gathering the planned spot.
		if (target->GetVictim() == ch)
		{
			PIXEL_POSITION center;
			center.x = target->GetX();
			center.y = target->GetY();
			center.z = 0;
			state.vecMultiPullCenters.push_back(center);
			++state.bMultiPullGroups;
			state.dwMultiPullTargetVID = 0;
			state.dwTargetVID = 0;
			state.dwNextMultiPullActionTime = dwNow + PLAYERBOT_MULTI_PULL_ACTION_DELAY;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PULL: aggroed pack pid=%u name=%s groups=%u/%u target=%s aggressors=%d hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bMultiPullGroups,
					(unsigned int)state.bMultiPullDesiredGroups, target->GetName(),
					CountPlayerBotPullAggressors(ch), ch->GetHP(), ch->GetMaxHP());
			return true;
		}
		const int distance = DISTANCE_APPROX(ch->GetX() - target->GetX(),
				ch->GetY() - target->GetY());
		if (distance > PLAYERBOT_MELEE_RANGE)
		{
			// A warrior/sura with a battle horse gathers the valour-cloak spot from
			// the saddle instead of climbing down between packs.
			const bool fightOnHorse = CanPlayerBotFightOnHorse(ch, target);
			MovePlayerBot(ch, target->GetX(), target->GetY(), dwNow, 4, false,
					fightOnHorse, fightOnHorse);
			return true;
		}

		if (dwNow < state.dwNextMultiPullActionTime)
			return true;
		if (ch->IsStateMove())
			ch->Stop();
		if (!ExecutePlayerBotBasicAttack(ch, target, state, dwNow))
			return true;

		PIXEL_POSITION center;
		center.x = target->GetX();
		center.y = target->GetY();
		center.z = 0;
		state.vecMultiPullCenters.push_back(center);
		++state.bMultiPullGroups;
		state.dwMultiPullTargetVID = 0;
		state.dwTargetVID = 0;
		state.dwNextMultiPullActionTime = dwNow + PLAYERBOT_MULTI_PULL_ACTION_DELAY;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_PULL: tagged pack pid=%u name=%s groups=%u/%u target=%s aggressors=%d hp=%d/%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bMultiPullGroups,
				(unsigned int)state.bMultiPullDesiredGroups, target->GetName(),
				CountPlayerBotPullAggressors(ch), ch->GetHP(), ch->GetMaxHP());
		return true;
	}
}

#endif
