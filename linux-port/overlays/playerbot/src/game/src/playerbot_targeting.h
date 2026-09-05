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
						 !IsPlayerBotMetinWorthFighting(m_owner, candidate)) ||
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
		if (!ch || !ch->GetParty() || state.bVisitingShop || state.bRecoveringAfterDeath ||
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
			CFindPlayerBotEngagedTarget(LPCHARACTER owner) :
				m_owner(owner), m_target(NULL), m_bestPriority(INT_MIN) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (!candidate || candidate == m_owner || !candidate->IsMonster() ||
						candidate->IsDead() || candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return true;

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
	};

	LPCHARACTER FindPlayerBotEngagedTarget(LPCHARACTER ch)
	{
		if (!ch || !ch->GetSectree() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return NULL;
		CFindPlayerBotEngagedTarget finder(ch);
		ch->GetSectree()->ForEachAround(finder);
		return finder.GetTarget();
	}

	class CCountPlayerBotStoneAttackers
	{
		public:
			CCountPlayerBotStoneAttackers(LPCHARACTER stone) :
				m_stone(stone), m_count(0) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER attacker = static_cast<LPCHARACTER>(entity);
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
					++m_count;
				return true;
			}

			BYTE GetCount() const { return m_count; }

		private:
			LPCHARACTER m_stone;
			BYTE m_count;
	};

	BYTE CountPlayerBotStoneAttackers(LPCHARACTER stone)
	{
		if (!stone || !stone->GetSectree())
			return 0;
		CCountPlayerBotStoneAttackers counter(stone);
		stone->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
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
				m_huntM2Bestials(owner && owner->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 &&
						ShouldPlayerBotHuntM2Bestials(owner)),
				m_pWantedDrops(NULL)
			{
			}

			// The materials the owner is short of, computed once by the caller.
			void SetWantedDrops(const std::set<DWORD>* pWanted) { m_pWantedDrops = pWanted; }

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || (!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead())
					return false;
				if (IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;

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
					if (!IsPlayerBotMetinWorthFighting(m_owner, candidate))
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

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > m_maxDistance)
					return false;

				const int botLevel = m_owner->GetLevel();
				const int mobLevel = candidate->GetLevel();
				const int levelDelta = mobLevel - botLevel;
				const bool isQuestTarget = candidate->IsMonster() &&
						m_desiredMobVnum != 0 && candidate->GetRaceNum() == m_desiredMobVnum;
				const bool isBestialWeaponTarget = candidate->IsMonster() &&
						m_huntM2Bestials &&
						(candidate->GetRaceNum() == 533 || candidate->GetRaceNum() == 534);

				// Do not cross a hunting field for obsolete prey, but kill a weaker mob
				// which is already on the route. This makes local grinding look like a
				// player holding Space instead of visibly walking past living packs.
				if (candidate->IsMonster() && !isQuestTarget && botLevel >= 6 &&
						levelDelta <= -3 && candidate->GetVictim() != m_owner &&
						distance > PLAYERBOT_LOCAL_CHAIN_RANGE)
					return false;

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
				const bool isMetinHunter = (sit != s_mapPlayerBotAIStates.end() && sit->second.bBotRole == BOT_ROLE_METIN_HUNTER);

				if (candidate->IsStone())
				{
					baseScore = isMetinHunter ? 1500000 : 350000;
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
							(botLevel >= 11 && (raceVnum >= 110 && raceVnum <= 115 || (raceVnum >= 301 && raceVnum <= 394))))
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
			std::vector<TTargetCandidate> m_targets;
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
	// is why the caller rations it.
	class CFindPlayerBotWantedDrop
	{
		public:
			CFindPlayerBotWantedDrop(LPCHARACTER seeker, const std::set<DWORD>& wanted,
					int maxDistance)
				: m_seeker(seeker), m_wanted(wanted), m_maxDistance(maxDistance),
				  m_best(NULL), m_bestDistance(INT_MAX), m_dropVnum(0)
			{
			}

			void operator()(LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER mob = static_cast<LPCHARACTER>(entity);
				if (!mob->IsMonster() || mob->IsDead() || mob->IsStone())
					return;
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
		if (!ch)
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

		// Nothing wanted is the common case and costs a walk over the bag, not
		// over the map, so it is settled before the tick's scan budget is asked.
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		if (wanted.empty())
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
		state.dwNextMaterialScanTime = dwNow + PLAYERBOT_MATERIAL_SCAN_INTERVAL;

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex());
		if (!map)
			return false;

		CFindPlayerBotWantedDrop finder(ch, wanted, PLAYERBOT_MATERIAL_HUNT_RANGE);
		map->for_each(finder);
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

		DWORD desiredBiologistMobVnum = 0;
		const TPlayerBotBiologistMission* biologistMission =
				GetActivePlayerBotBiologistMission(ch);
		if (biologistMission && !state.bVisitingBiologist)
		{
			const int accepted = std::max(0, ch->GetQuestFlag(
					GetPlayerBotBiologistFlag(*biologistMission, "collect_count")));
			const int remaining = std::max(0,
					(int)biologistMission->requiredCount - accepted);
			if (ch->CountSpecifyItem(biologistMission->itemVnum) < remaining)
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

		const int targetSearchRange = ch->GetParty()
				? PLAYERBOT_PARTY_COHESION_RADIUS : PLAYERBOT_SEARCH_RANGE;
		CCollectPlayerBotTargets collector(ch, targetSearchRange, maxLevel,
				partyChallengeMaxLevel, desiredQuestMobVnum, dwAvoidVID,
				lAvoidX, lAvoidY, avoidRadius, state.mapFailedStones,
				state.mapFailedTargets, dwNow);
		std::set<DWORD> wantedDrops;
		CollectPlayerBotWantedMaterials(ch, wantedDrops);
		collector.SetWantedDrops(&wantedDrops);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();

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

		// Chain ordinary combat into the closest unclaimed pack. A nearby quest or
		// Bestial objective wins over generic prey, but a far-away objective no longer
		// makes the bot walk past mobs at its feet. Metin hunters retain their global
		// stone scoring and reservations below.
		if (state.bBotRole != BOT_ROLE_METIN_HUNTER)
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
		const size_t poolSize = availableTargets.empty() ? targets.size() : availableTargets.size();
		const size_t choiceCount = std::min(poolSize, PLAYERBOT_TARGET_CHOICE_WINDOW);
		const size_t choiceIndex = (size_t)number(0, (int)choiceCount - 1);
		const DWORD targetVID = availableTargets.empty()
			? targets[choiceIndex].dwVID
			: availableTargets[choiceIndex];

		return CHARACTER_MANAGER::instance().Find(targetVID);
	}

	class CCollectPlayerBotMeleeTargets
	{
		public:
			CCollectPlayerBotMeleeTargets(LPCHARACTER owner, DWORD primaryVID) :
				m_owner(owner),
				m_primaryVID(primaryVID)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || candidate->GetVID() == m_primaryVID ||
						(!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead())
					return false;

				if (candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()) ||
						(candidate->IsMonster() && candidate->GetLevel() > m_owner->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA))
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance <= PLAYERBOT_MELEE_SPLASH_RANGE)
					m_targets.push_back(std::make_pair(distance, candidate->GetVID()));

				return true;
			}

			void Sort()
			{
				std::sort(m_targets.begin(), m_targets.end());
			}

			const std::vector<std::pair<int, DWORD> >& GetTargets() const { return m_targets; }

		private:
			LPCHARACTER m_owner;
			DWORD m_primaryVID;
			std::vector<std::pair<int, DWORD> > m_targets;
	};

	DWORD AttackPlayerBotMeleeGroup(LPCHARACTER ch, LPCHARACTER primary)
	{
		if (!ch || !primary || !ch->GetSectree())
			return 0;

		const bool bIsTargetValid = (primary->IsMonster() || primary->IsStone());
		if (!bIsTargetValid || primary->IsDead())
			return 0;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW);
		LPITEM arrow = NULL;
		if (isBow && ch->GetArrowAndBow(&weapon, &arrow, 1) != 1)
			return 0;

		int iDamage = isBow ? CalcArrowDamage(ch, primary, weapon, arrow, false) : CalcMeleeDamage(ch, primary, false, false);
		if (iDamage < 5)
			iDamage = number(15, 35) + ch->GetLevel() * 4;

		DWORD hitCount = 1;
		primary->Damage(ch, iDamage, DAMAGE_TYPE_NORMAL);
		if (isBow)
			ch->UseArrow(arrow, 1);
		primary->SetSyncOwner(ch);
		if (!primary->IsDead() && primary->CanBeginFight())
			primary->BeginFight(ch);

		if (!isBow)
		{
			CCollectPlayerBotMeleeTargets collector(ch, primary->GetVID());
			ch->GetSectree()->ForEachAround(collector);
			collector.Sort();

			const std::vector<std::pair<int, DWORD> >& targets = collector.GetTargets();
			for (size_t i = 0; i < targets.size() && hitCount < PLAYERBOT_MAX_MELEE_TARGETS; ++i)
			{
				LPCHARACTER secondary = CHARACTER_MANAGER::instance().Find(targets[i].second);
				if (!secondary || secondary->IsDead() || (!secondary->IsMonster() && !secondary->IsStone()))
					continue;

				int iSecDamage = CalcMeleeDamage(ch, secondary, false, false);
				if (iSecDamage < 5)
					iSecDamage = number(12, 28) + ch->GetLevel() * 3;

				secondary->Damage(ch, iSecDamage, DAMAGE_TYPE_NORMAL);
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
			const DWORD found = PLAYERBOT_SWING_MS[job][subType][step];
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
				(!target->IsMonster() && !target->IsStone()) ||
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
			if (ch->GetArrowAndBow(&bow, &arrow, 1) != 1)
				return false;
		}
		const int combatRange = isBow ? 800 : 280;
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
			if (state.bComboMotion > MOTION_COMBO_ATTACK_4)
				state.bComboMotion = MOTION_COMBO_ATTACK_1;
		}
		return true;
	}

	bool IsPlayerBotMultiPullBuild(LPCHARACTER ch, bool* naturalTank)
	{
		if (naturalTank)
			*naturalTank = false;
		if (!ch || ch->GetLevel() < 15 || ch->GetParty() ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
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

		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch);
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
					(current && !current->IsDead()) || FindPlayerBotEngagedTarget(ch))
				return false;

			LPCHARACTER first = FindPlayerBotPullTarget(ch, state.vecMultiPullCenters);
			if (!first)
			{
				state.dwNextMultiPullTime = dwNow + number(10000, 20000);
				return false;
			}

			state.bMultiPullActive = true;
			state.bMultiPullGroups = 0;
			state.bMultiPullDesiredGroups = naturalTank
					? (BYTE)(2 + PlayerBotNavHash(ch->GetPlayerID() ^
							(dwNow / 60000U)) % 3U) : 2;
			state.dwMultiPullStartedTime = dwNow;
			state.dwNextMultiPullActionTime = dwNow;
			state.iMultiPullStartHPPercent = hpPercent;
			state.dwMultiPullTargetVID = first->GetVID();
			state.dwTargetVID = first->GetVID();
			state.vecMultiPullCenters.clear();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PULL: started pid=%u name=%s level=%u desired_groups=%u hp=%d/%d natural_tank=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bMultiPullDesiredGroups, ch->GetHP(),
					ch->GetMaxHP(), naturalTank ? 1 : 0);
		}

		const int aggressors = CountPlayerBotPullAggressors(ch);
		if (hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT ||
				state.iMultiPullStartHPPercent - hpPercent >= PLAYERBOT_MULTI_PULL_MAX_HP_LOSS_PERCENT ||
				aggressors >= PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS ||
				dwNow - state.dwMultiPullStartedTime >= PLAYERBOT_MULTI_PULL_TIMEOUT)
		{
			const char* reason = hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT
					? "low_hp" : (aggressors >= PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS
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
