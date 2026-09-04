#ifndef __INC_METIN2_PLAYERBOT_COMBAT_H__
#define __INC_METIN2_PLAYERBOT_COMBAT_H__

// The fight itself: the packets a swing or a cast is made of, combat buffs,
// attack skills, holding a party together, and an archer's pull.
//
// A bot has no client to send these for it, so every motion a real player's
// client would generate has to be built here by hand and broadcast to whoever
// can see it. That is why this reads as protocol rather than as behaviour.
//
// Depends on playerbot_skills.h for the build it is executing.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	void SendPlayerBotFlyTargetPacket(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !ch->GetSectree() ||
				ch->GetMapIndex() != target->GetMapIndex())
			return;

		// Only reproduce the visual packet emitted by a real client.  Calling
		// CHARACTER::FlyTarget here would also mutate m_dwFlyTargetID and force us
		// through Shoot(), which applies damage a second time and was the source of
		// the latest archer regression.
		TPacketGCFlyTargeting pack;
		pack.bHeader = HEADER_GC_FLY_TARGETING;
		pack.dwShooterVID = ch->GetVID();
		pack.dwTargetVID = target->GetVID();
		pack.x = target->GetX();
		pack.y = target->GetY();

		ch->PacketAround(&pack, sizeof(TPacketGCFlyTargeting), ch);
	}

	void SendPlayerBotAttackPacket(LPCHARACTER ch, LPCHARACTER target, BYTE comboMotion)
	{
		if (!ch || !ch->GetSectree())
			return;

		ch->OnMove(true);
		ch->ResetStopTime();

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW;
		if (isBow)
		{
			// Bow mode registers only COMBO_ATTACK_1 (bow/attack.msa).  Cycling
			// through 2..4 selects missing motions and leaves the archer frozen.
			comboMotion = MOTION_COMBO_ATTACK_1;
			SendPlayerBotFlyTargetPacket(ch, target);
		}
		else if (comboMotion < MOTION_COMBO_ATTACK_1 || comboMotion > MOTION_COMBO_ATTACK_4)
			comboMotion = MOTION_COMBO_ATTACK_1;

		TPacketGCMove pack;
		pack.bHeader = HEADER_GC_MOVE;
		// This exact COMBO_ATTACK_1 path is the build visually verified by the
		// user for both bow and dagger.  Bow differs only in being pinned to its
		// sole registered combo key instead of cycling through 1..4.
		pack.bFunc = FUNC_COMBO;
		pack.bArg = comboMotion;
		pack.bRot = (BYTE)(ch->GetRotation() / 5);
		pack.dwVID = ch->GetVID();
		pack.lX = ch->GetX();
		pack.lY = ch->GetY();
		pack.dwTime = get_dword_time();
		pack.dwDuration = 0;

		ch->PacketAround(&pack, sizeof(TPacketGCMove));
	}

	BYTE GetPlayerBotSkillMotionIndex(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch)
			return (BYTE)(skillVnum & 0x7F);

		// skilldesc registers the six skills of each profession under motion
		// slots 1..6 (group 1) or 16..21 (group 2), independently of the
		// server-side skill VNUM.  Every mastery grade is one SKILL_GRADEGAP
		// (25 motions) further.  Sending the raw VNUM happened to work for a
		// few Warrior motions, but points Ninja/Sura/Shaman at empty keys.
		if (skillVnum >= 1 && skillVnum <= 111)
		{
			const BYTE baseMotion = (BYTE)(((skillVnum - 1) % 30) + 1);
			int mastery = ch->GetSkillMasterType(skillVnum);
			if (mastery < SKILL_NORMAL)
				mastery = SKILL_NORMAL;
			else if (mastery > SKILL_PERFECT_MASTER)
				mastery = SKILL_PERFECT_MASTER;
			return (BYTE)(baseMotion + mastery * 25);
		}

		return (BYTE)(skillVnum & 0x7F);
	}

	void SendPlayerBotSkillPacket(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || !ch->GetSectree())
			return;

		ch->OnMove();
		ch->ResetStopTime();

		const BYTE motionIndex = GetPlayerBotSkillMotionIndex(ch, skillVnum);
		TPacketGCMove pack;
		pack.bHeader = HEADER_GC_MOVE;
		pack.bFunc = FUNC_SKILL | motionIndex;
		// bArg is the animation loop count, not the N/M/G/P grade.  Zero is the
		// native/default single-play value used by the previously working build.
		pack.bArg = 0;
		pack.bRot = (BYTE)(ch->GetRotation() / 5);
		pack.dwVID = ch->GetVID();
		pack.lX = ch->GetX();
		pack.lY = ch->GetY();
		pack.dwTime = get_dword_time();
		pack.dwDuration = 0;

		ch->PacketAround(&pack, sizeof(TPacketGCMove));
		sys_log(1, "PLAYERBOT_AI: skill motion pid=%u skill=%u motion=%u mastery=%d",
				ch->GetPlayerID(), skillVnum, motionIndex,
				ch->GetSkillMasterType(skillVnum));
	}

	bool ManagePlayerBotCombatBuffs(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetSkillGroup() == 0 || dwNow < state.dwNextBuffCheckTime)
			return false;

		// These used to be cast only once a target had been acquired, which is
		// why a bot was hardly ever seen with its aura up: it walked into every
		// fight unbuffed, spent the first five-second window casting instead of
		// hitting, and stood there bare again as soon as the fight ended. The
		// self-buffs are what these builds are built around, so they are kept up
		// out in the world too - just not during a town errand, a retreat or a
		// pull, each of which owns the tick and would be interrupted by a cast
		// claiming it.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession)
			return false;

		if (ch->GetMapIndex() == 21)
		{
			const long townX = 60600;
			const long townY = 170900;
			if (DISTANCE_APPROX(ch->GetX() - townX, ch->GetY() - townY) <= 3000)
				return false; // Inside city center / near town merchants
		}

		state.dwNextBuffCheckTime = dwNow + 5000;

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD buffVnum = build.dwBuffSkills[i];
			if (buffVnum == 0 || ch->GetSkillLevel(buffVnum) == 0)
				continue;

			// Check if buff is currently active (including toggle skills like Enchanted Blade / Flame Spirit)
			if (IsPlayerBotBuffActive(ch, buffVnum, dwNow, state))
				continue;

			if (buffVnum == 109) // Cure / Heal
			{
				if (ch->GetMaxHP() <= 0 || (ch->GetHP() * 100) / ch->GetMaxHP() > 60)
					continue;
			}

			// Self-buff if not active
			if (ch->UseSkill(buffVnum, ch))
			{
				SendPlayerBotSkillPacket(ch, buffVnum);
				state.dwLastBotSkillTime = dwNow;
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				// FindAffect/AFF_* above is authoritative.  Keep a conservative
				// fallback as well, because some client/server skill tables do not
				// expose every buff through the same affect flag.  Cure is instant
				// and therefore only needs its normal skill cooldown.
				state.mapBuffActiveUntil[buffVnum] = dwNow +
						(buffVnum == 109 ? 10000 : PLAYERBOT_BUFF_FALLBACK_DURATION);
				sys_log(0, "PLAYERBOT_AI: activated self buff skill pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), buffVnum);
				return true;
			}

			// Party buffs for Shaman (Blessing, Dragon Aid, Swiftness, Attack Up, Heal)
			if (ch->GetJob() == JOB_SHAMAN && ch->GetParty())
			{
				struct FPartyBuffShaman
				{
					LPCHARACTER m_shaman;
					DWORD m_buffVnum;
					DWORD m_dwNow;
					TPlayerBotAIState& m_state;
					bool m_bApplied;

					FPartyBuffShaman(LPCHARACTER shaman, DWORD buffVnum, DWORD dwNow, TPlayerBotAIState& state) :
						m_shaman(shaman), m_buffVnum(buffVnum), m_dwNow(dwNow), m_state(state), m_bApplied(false)
					{
					}

					void operator()(LPCHARACTER member)
					{
						if (m_bApplied || !member || member == m_shaman || member->IsDead())
							return;

						if (DISTANCE_APPROX(m_shaman->GetX() - member->GetX(), m_shaman->GetY() - member->GetY()) > 2000)
							return;

						if (m_buffVnum == 109) // Cure/Heal
						{
							if (member->GetMaxHP() > 0 && (member->GetHP() * 100) / member->GetMaxHP() <= 60)
							{
								if (m_shaman->UseSkill(m_buffVnum, member))
								{
									SendPlayerBotSkillPacket(m_shaman, m_buffVnum);
									m_state.dwLastBotSkillTime = m_dwNow;
									m_state.dwNextAttackTime = m_dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
									m_bApplied = true;
									sys_log(0, "PLAYERBOT_AI: shaman healed party member pid=%u target_pid=%u",
											m_shaman->GetPlayerID(), member->GetPlayerID());
								}
							}
						}
						else if (member->FindAffect(m_buffVnum) == NULL)
						{
							if (m_shaman->UseSkill(m_buffVnum, member))
							{
								SendPlayerBotSkillPacket(m_shaman, m_buffVnum);
								m_state.dwLastBotSkillTime = m_dwNow;
								m_state.dwNextAttackTime = m_dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
								m_bApplied = true;
								sys_log(0, "PLAYERBOT_AI: shaman buffed party member pid=%u target_pid=%u vnum=%u",
										m_shaman->GetPlayerID(), member->GetPlayerID(), m_buffVnum);
							}
						}
					}
				};

				FPartyBuffShaman buffFunctor(ch, buffVnum, dwNow, state);
				ch->GetParty()->ForEachOnMapMember(buffFunctor, ch->GetMapIndex());
				if (buffFunctor.m_bApplied)
					return true;
			}
		}

		return false;
	}

	bool ExecutePlayerBotAttackSkill(LPCHARACTER ch, LPCHARACTER target, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || ch->GetSkillGroup() == 0 || dwNow < state.dwNextSkillCastTime)
			return false;
		LPITEM archerBow = NULL;
		LPITEM archerArrow = NULL;
		if (ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2)
		{
			if (!EnsurePlayerBotArrowsEquipped(ch) ||
					ch->GetArrowAndBow(&archerBow, &archerArrow, 1) != 1)
				return false;
		}

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwOffensiveSkills) / sizeof(build.dwOffensiveSkills[0]); ++i)
		{
			const DWORD skillVnum = build.dwOffensiveSkills[i];
			if (skillVnum == 0 || ch->GetSkillLevel(skillVnum) == 0)
				continue;

			if (ch->UseSkill(skillVnum, target))
			{
				if (ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2)
					SendPlayerBotFlyTargetPacket(ch, target);

				// Keep the single, proven server-side damage path.  Shoot() would
				// consume the pending target and run a second damage path.  The visual
				// packet follows the same order as the build verified in the client.
				ch->ComputeSkill(skillVnum, target);
				SendPlayerBotSkillPacket(ch, skillVnum);
				if (archerArrow)
					ch->UseArrow(archerArrow, 1);
				state.dwLastBotSkillTime = dwNow;
				state.dwLastCombatActionTime = dwNow;
				// Shamans should weave weapon attacks between spells.  Casting an
				// offensive spell every global AI tick looks like repeated buffing
				// in the client and leaves almost no visible normal attacks.
				state.dwNextSkillCastTime = dwNow +
						(ch->GetJob() == JOB_SHAMAN
						 ? PLAYERBOT_SHAMAN_ATTACK_SKILL_INTERVAL
						 : PLAYERBOT_SKILL_ATTACK_INTERVAL);
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				sys_log(0, "PLAYERBOT_AI: used attack skill pid=%u name=%s vnum=%u target_vid=%u",
						ch->GetPlayerID(), ch->GetName(), skillVnum, target->GetVID());
				return true;
			}
		}

		return false;
	}

	class FPlayerBotPartyCohesion
	{
		public:
			FPlayerBotPartyCohesion(LPCHARACTER leader, int maxDistance) :
				m_leader(leader), m_maxDistance(maxDistance), m_onlineBots(0),
				m_togetherBots(0)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || !member->GetDesc() || !member->GetDesc()->IsBot())
					return;
				++m_onlineBots;
				if (member->GetMapIndex() == m_leader->GetMapIndex() &&
						DISTANCE_APPROX(member->GetX() - m_leader->GetX(),
							member->GetY() - m_leader->GetY()) <= m_maxDistance)
					++m_togetherBots;
			}

			int OnlineBots() const { return m_onlineBots; }
			int TogetherBots() const { return m_togetherBots; }

		private:
			LPCHARACTER m_leader;
			int m_maxDistance;
			int m_onlineBots;
			int m_togetherBots;
	};

	bool IsPlayerBotPartyCohesive(LPCHARACTER ch, int minMembers, int maxDistance)
	{
		if (!ch || !ch->GetParty() || ch->GetParty()->GetMemberCount() < (DWORD)minMembers)
			return false;
		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		if (!leader || leader->GetMapIndex() != ch->GetMapIndex())
			return false;

		FPlayerBotPartyCohesion cohesion(leader, maxDistance);
		ch->GetParty()->ForEachOnlineMember(cohesion);
		return cohesion.OnlineBots() >= minMembers &&
				cohesion.TogetherBots() == cohesion.OnlineBots() &&
				cohesion.OnlineBots() == (int)ch->GetParty()->GetMemberCount();
	}

	bool ExecutePlayerBotArcherLuring(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2 || !ch->GetParty())
			return false;
		// Luring is a group role, not a solo Archer shortcut. Every party member
		// must be online, on this map and inside one local formation; otherwise an
		// Archer could pull for a nominal party scattered across different zones.
		if (!IsPlayerBotPartyCohesive(ch, PLAYERBOT_ARCHER_LURE_MIN_PARTY_MEMBERS,
				PLAYERBOT_PARTY_COHESION_RADIUS))
			return false;

		LPITEM weapon = NULL;
		LPITEM arrow = NULL;
		if (!EnsurePlayerBotArrowsEquipped(ch) ||
				ch->GetArrowAndBow(&weapon, &arrow, 1) != 1)
			return false; // Only lure when equipped with a Bow!

		if (dwNow < state.dwNextLureTime)
			return false;

		state.dwNextLureTime = dwNow + number(3500, 5500);

		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		if (!leader || leader->GetMapIndex() != ch->GetMapIndex())
			return false;

		// Scan for distant mob around party to pull
		if (!ch->GetSectree())
			return false;

		struct TLureCollector
		{
			TLureCollector(LPCHARACTER me) : m_me(me), m_targetVID(0), m_bestDist(99999) {}
			bool operator()(LPENTITY ent)
			{
				if (!ent || !ent->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER mob = static_cast<LPCHARACTER>(ent);
				if (!mob->IsMonster() || mob->IsDead() || mob->GetVictim() != NULL ||
						IsPlayerBotSafeZone(mob->GetMapIndex(), mob->GetX(), mob->GetY()) ||
						mob->GetLevel() > m_me->GetLevel() + 10)
					return false;
				int dist = DISTANCE_APPROX(m_me->GetX() - mob->GetX(), m_me->GetY() - mob->GetY());
				if (dist >= 1200 && dist <= 3000 && dist < m_bestDist &&
						IsPlayerBotReachable(m_me->GetMapIndex(), m_me->GetX(), m_me->GetY(),
							mob->GetX(), mob->GetY()))
				{
					m_bestDist = dist;
					m_targetVID = mob->GetVID();
				}
				return true;
			}
			LPCHARACTER m_me;
			DWORD m_targetVID;
			int m_bestDist;
		};

		TLureCollector collector(ch);
		ch->GetSectree()->ForEachAround(collector);

		if (collector.m_targetVID != 0)
		{
			LPCHARACTER mob = CHARACTER_MANAGER::instance().Find(collector.m_targetVID);
			if (mob && !mob->IsDead())
			{
				ch->SetRotationToXY(mob->GetX(), mob->GetY());
				// Use a normal bow shot for the pull. Fire Arrow is part of the normal
				// offensive rotation and was almost always on its real skill cooldown,
				// which made the old lure silently fail even in a valid six-person PT.
				int damage = CalcArrowDamage(ch, mob, weapon, arrow, false);
				if (damage < 5)
					damage = number(10, 20) + ch->GetLevel() * 2;
				SendPlayerBotAttackPacket(ch, mob, MOTION_COMBO_ATTACK_1);
				mob->Damage(ch, damage, DAMAGE_TYPE_NORMAL);
				ch->UseArrow(arrow, 1);
				mob->SetSyncOwner(ch);
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				state.dwLastCombatActionTime = dwNow;
				sys_log(0, "PLAYERBOT_AI: archer lured distant mob pid=%u name=%s target_vid=%u target=%s damage=%d",
						ch->GetPlayerID(), ch->GetName(), mob->GetVID(), mob->GetName(), damage);
				return true;
			}
		}

		return false;
	}
}

#endif
