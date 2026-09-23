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

	// Not everything in a build's buff list is for fighting. Feather Walk and
	// Swiftness make a bot move faster, and moving is what it spends most of its
	// time doing - it walks a kilometre to its hunting ground. Cure is not a buff
	// at all: it is a heal, already gated on the bot's own health, and a wounded
	// Shaman limping back to town has more reason to cast it than one in a fight.
	// These stay available whenever the bot is out in the world; everything else
	// waits until there is something to fight.
	bool IsPlayerBotOutOfCombatBuff(DWORD buffVnum)
	{
		return buffVnum == 49 ||    // Bezszelestny Chod  (Ninja)
				buffVnum == 110 ||  // Zwinnosc           (Szaman)
				buffVnum == 109;    // Leczenie           (Szaman)
	}

	// duel: asked by the duel pass, which claims the tick above this one. A
	// duellist buffs wherever the duel stands and whatever errand it paused,
	// and counts as in combat from the start.
	bool ManagePlayerBotCombatBuffs(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			bool duel = false)
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
		if (!duel && (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession))
			return false;
		// Nor from the saddle of a transport horse: CHARACTER::UseSkill refuses
		// every non-horse skill while riding one, and a rider on a long leg
		// now keeps its horse until it has a target. The aura goes up on the
		// tick after it climbs down. A rider that fights from its battle horse
		// climbs down for the buff itself, below.
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
			return false;
		// A bot minding its own stall is not hunting. Casting does not close a
		// private shop - the engine only does that on stun, death and leaving the
		// world - but a keeper standing at its counter throwing auras is burning
		// mana on nothing and looks wrong to anyone walking past.
		if (ch->GetMyShop())
			return false;

		if (!duel && ch->GetMapIndex() == 21)
		{
			const long townX = 60600;
			const long townY = 170900;
			if (DISTANCE_APPROX(ch->GetX() - townX, ch->GetY() - townY) <= 3000)
				return false; // Inside city center / near town merchants
		}
		// The market is in Bokjung, and this check only ever covered Joan. A bot
		// browsing the stalls has no business buffing in the middle of them.
		playerbot_empire_rules::TPoint pitch;
		if (!duel && playerbot_empire_rules::GetTownPitch(ch->GetMapIndex(), pitch) &&
				DISTANCE_APPROX(ch->GetX() - pitch.x,
						ch->GetY() - pitch.y) <= PLAYERBOT_SHOPPING_RANGE)
			return false;

		state.dwNextBuffCheckTime = dwNow + 5000;

		// 1.24.2 dropped the requirement to hold a target, because bots were
		// entering every fight bare. It went too far the other way: they stood in
		// town casting an aura they would lose long before reaching the monsters a
		// kilometre away. "Hunting" is the middle ground - in a fight, or recently
		// enough in one that another is coming.
		const bool fighting = duel || state.dwTargetVID != 0 || ch->GetVictim() != NULL;
		const bool inCombat = fighting ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < PLAYERBOT_BUFF_COMBAT_WINDOW);

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD buffVnum = build.dwBuffSkills[i];
			if (buffVnum == 0 || ch->GetSkillLevel(buffVnum) == 0)
				continue;
			if (!inCombat && !IsPlayerBotOutOfCombatBuff(buffVnum))
				continue;

			// Check if buff is currently active (including toggle skills like Enchanted Blade / Flame Spirit)
			if (IsPlayerBotBuffActive(ch, buffVnum, dwNow, state))
				continue;

			if (buffVnum == 109) // Cure / Heal
			{
				if (ch->GetMaxHP() <= 0 || (ch->GetHP() * 100) / ch->GetMaxHP() > 60)
					continue;
			}

			// No skill of a class is cast from a saddle, a battle horse's
			// included: UseSkill refuses it without a word, and a warrior that
			// fought from one went without its aura and its berserk for good.
			// So the rider climbs down for the buff - only in a fight, and not
			// for the walking buffs, which a horse outruns anyway. The flip
			// hold keeps it on foot while the set goes up
			// (PLAYERBOT_HORSE_TRAVEL_FLIP_HOLD_MS against a cast every
			// PLAYERBOT_BUFF_RECHECK_FAST), and the target section puts it back
			// in the saddle afterwards.
			if (ch->IsRiding())
			{
				if (!fighting || IsPlayerBotOutOfCombatBuff(buffVnum))
					continue;
				if (!SetPlayerBotRidingForTravel(ch, state, false, dwNow, "buff"))
					return false;
				state.dwNextBuffCheckTime = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
				return true;
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
				// Straight back for the next one. The ordinary five seconds
				// resume on the first pass that finds nothing missing.
				state.dwNextBuffCheckTime = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
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

	// One support buff from a Shaman to the first of `members`, in the
	// caller's order, that lacks it and stands within the skill's reach: the
	// build's own buff list less what is SELFONLY, Cure only to a member under
	// PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT, and outside a fight only the
	// walking buffs. A rider climbs down first, because nothing of a class is
	// cast from a saddle, and the cast waits for the caller's next pass.
	// Returns 0 when nothing was done, 1 for the climb-down and 2 for a cast,
	// whose target and skill come back in outTarget and outVnum.
	int CastPlayerBotSupportBuff(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			const std::vector<LPCHARACTER>& members, bool hunting, const char* dismountReason,
			LPCHARACTER& outTarget, DWORD& outVnum)
	{
		outTarget = NULL;
		outVnum = 0;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD vnum = build.dwBuffSkills[i];
			if (vnum == 0 || ch->GetSkillLevel(vnum) == 0)
				continue;
			if (!hunting && !IsPlayerBotOutOfCombatBuff(vnum))
				continue;
			CSkillProto* proto = CSkillManager::instance().Get(vnum);
			if (!proto || IS_SET(proto->dwFlag, SKILL_FLAG_SELFONLY))
				continue;
			LPCHARACTER target = NULL;
			for (size_t m = 0; m < members.size(); ++m)
			{
				LPCHARACTER member = members[m];
				if (proto->dwTargetRange != 0 &&
						DISTANCE_APPROX(ch->GetX() - member->GetX(), ch->GetY() - member->GetY()) >
								(int)proto->dwTargetRange)
					continue;
				if (vnum == 109) // Cure / Heal
				{
					if (member->GetMaxHP() <= 0 ||
							(long long)member->GetHP() * 100 / member->GetMaxHP() > PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT)
						continue;
				}
				else if (IsPlayerBotBuffAffectOn(member, vnum))
					continue;
				target = member;
				break;
			}
			if (!target)
				continue;
			// From any saddle: a battle horse casts no skill of a class either
			// (PLAYERBOT_SADDLE_SKILL_LEVEL), and the cast below would be
			// refused without a word.
			if (ch->IsRiding())
			{
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, dismountReason);
				return 1;
			}
			if (!ch->UseSkill(vnum, target))
				continue;
			SendPlayerBotSkillPacket(ch, vnum);
			state.dwLastBotSkillTime = dwNow;
			state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
			outTarget = target;
			outVnum = vnum;
			return 2;
		}
		return 0;
	}

	// A splash skill is for a crowd, and a Metin stone is never a crowd.
	//
	// The rotation takes the first skill that is off cooldown, and a stone takes
	// long enough to put the good ones on cooldown - so what kept coming up
	// against stones was the splash skill, which is where these builds are
	// weakest on a single target. Poison Cloud is
	// -(lv*2 + (atk + str*3 + dex*18)*k) against Fast Attack's
	// -(atk + (1.6*atk + ...)): one attack rating against two and a half, for
	// the same 1.4 s of animation lock. Skipping it and letting the ordinary
	// swing chain through is strictly better on one target. Asked of the engine
	// rather than kept as a list of VNUMs, so a server whose skill table differs
	// still gets the right answer.
	bool IsPlayerBotSplashSkill(DWORD skillVnum)
	{
		CSkillProto* proto = CSkillManager::instance().Get(skillVnum);
		return proto && (proto->dwFlag & SKILL_FLAG_SPLASH) != 0;
	}

	// The character this bot agreed to duel, if it is still standing where the
	// bot can reach it. Resolved here rather than in the policy header because
	// that one is shared with an engine translation unit and knows no
	// LPCHARACTER; everything below the include of playerbot_combat.h may ask.
	LPCHARACTER FindPlayerBotDuelOpponent(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return NULL;
		const uint32_t foePid = playerbot_pvp::GetDuelOpponent(ch->GetPlayerID(), dwNow);
		if (foePid == 0)
			return NULL;
		LPCHARACTER foe = CHARACTER_MANAGER::instance().FindByPID((DWORD)foePid);
		if (!foe || foe == ch || foe->IsDead() || !foe->IsPC() ||
				foe->GetMapIndex() != ch->GetMapIndex())
			return NULL;
		return foe;
	}

	bool IsPlayerBotDuelOpponent(LPCHARACTER ch, LPCHARACTER target, DWORD dwNow)
	{
		if (!ch || !target)
			return false;
		return FindPlayerBotDuelOpponent(ch, dwNow) == target;
	}

	// The characters a bot may swing at besides monsters and stones: its duel
	// opponent, a member of the guild its own is at war with, and the player
	// the Anti-PK protocol is fighting (playerbot_anti_pk.h). Every one of them
	// still goes to battle_is_attackable (CanPlayerBotStrikeCharacter) before a
	// blow lands - that is the engine's word on who may be struck, and a war
	// between two guilds is the one thing it answers yes for by itself. The
	// basic swing knew only the duel, so a war was fought with skills alone
	// and a bot stood beside its foe between two casts: "boty jedynie uzywaja
	// umiejetnosci, nie autoatakuja, nie biegaja" (prodnathin, 21 September).
	// Every fight back of the Anti-PK protocol was the same, and a bot in the
	// saddle, which casts nothing a transport horse refuses, only followed its
	// attacker about (Dixdros, "PVP Bots", 21 September).
	bool IsPlayerBotSanctionedFoe(LPCHARACTER ch, LPCHARACTER target, DWORD dwNow)
	{
		if (!ch || !target || !target->IsPC())
			return false;
		if (IsPlayerBotDuelOpponent(ch, target, dwNow))
			return true;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotAIState& state = it->second;
		if (state.persona.dwFoeVID != 0 && state.persona.dwFoeVID == (DWORD)target->GetVID())
			return true;
		CGuild* theirs = target->GetGuild();
		return state.dwGuildWarEnemyGID != 0 && theirs && theirs->GetID() == state.dwGuildWarEnemyGID;
	}

	// Whether the engine will let a blow land on this character.
	//
	// CHARACTER::Damage asks nothing - not the agreement, not the protection
	// under PK_PROTECT_LEVEL, not the safe zone. battle_melee_attack and the
	// skill path ask battle_is_attackable first; the bots' own swing did not.
	// So from 2.0.39 a duellist's blow landed wherever the AI believed a duel
	// was on: a challenger struck before the other side had agreed, and a
	// winner went on striking the respawned loser after CPVP::Win had closed
	// the fight. To the engine each such kill was a murder in the killer's own
	// kingdom - minus twenty thousand alignment, shared over its party, which
	// is how bots of level nine came to wear "Zlosliwy" (nerrvous_s) and how 98
	// bots of our own world reached -151002. The skill path did ask, so the
	// same duel under level fifteen, or in a town, was an animation that never
	// hurt anybody and never ended (djariczek).
	bool CanPlayerBotStrikeCharacter(LPCHARACTER ch, LPCHARACTER victim)
	{
		return ch && victim && victim->IsPC() && battle_is_attackable(ch, victim);
	}

	// A duel ends for both of its sides at once, and the engine's half with it.
	//
	// CPVP::Win keeps the pair after a fight is decided: the loser may take a
	// revenge, and until it does the winner's client will not attack it
	// (PVP_MODE_REVENGE - "nie moge mu oddac", Drip). A bot takes no revenge,
	// so a player who beat one could neither hit it nor challenge it again
	// until CPVPManager::Process dropped the pair ten minutes later; and a
	// player beaten by a bot could take a revenge on a bot that no longer
	// counted itself in a duel, and so never hit back. Deleting the pair with
	// the engine's own NONE packet puts both clients back where they stood
	// before the challenge. When the other side is a bot its memory of the
	// duel goes too, or it would first be refused its blows for
	// PLAYERBOT_PVP_REFUSED_GIVE_UP.
	void EndPlayerBotDuel(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, const char* szReason)
	{
		if (!ch)
			return;
		const DWORD pid = ch->GetPlayerID();
		const DWORD foePid = (DWORD)playerbot_pvp::GetDuelOpponent(pid, dwNow);
		playerbot_pvp::EndDuel(pid);
		if (foePid == 0)
			return;
		LPCHARACTER foe = CHARACTER_MANAGER::instance().FindByPID(foePid);
		CPVP key(pid, foePid);
		CPVP* pair = CPVPManager::instance().Find(key.GetCRC());
		const bool pairRemoved = pair != NULL;
		if (pair)
		{
			pair->Packet(true);
			CPVPManager::instance().Delete(pair);
		}
		if (foe)
		{
			if (ch->GetVictim() == foe)
				ch->SetVictim(NULL);
			if (state.dwTargetVID == (DWORD)foe->GetVID())
				state.dwTargetVID = 0;
			if ((DWORD)playerbot_pvp::GetDuelOpponent(foePid, dwNow) == pid)
			{
				playerbot_pvp::EndDuel(foePid);
				if (foe->GetVictim() == ch)
					foe->SetVictim(NULL);
				TPlayerBotAIStateMap::iterator foeState = s_mapPlayerBotAIStates.find(foePid);
				if (foeState != s_mapPlayerBotAIStates.end() &&
						foeState->second.dwTargetVID == (DWORD)ch->GetVID())
					foeState->second.dwTargetVID = 0;
			}
		}
		sys_log(0, "PLAYERBOT_PVP: duel over pid=%u name=%s foe_pid=%u foe=%s reason=%s level=%u foe_level=%u pair_removed=%d",
				pid, ch->GetName(), foePid, foe ? foe->GetName() : "-", szReason,
				(unsigned int)ch->GetLevel(), foe ? (unsigned int)foe->GetLevel() : 0U, pairRemoved ? 1 : 0);
	}

	// A splash skill lands on every attackable thing in its radius, stones
	// included - FuncSplashDamage asks battle_is_attackable and nothing else -
	// so a bot fighting beside a Demon Tower stone could still break it with a
	// last blow meant for a monster, and the kill is the bot's: see
	// PLAYERBOT_DEVIL_TOWER_STONE_FIRST. The stone stands on map 66 alone (the
	// other four only inside instances no bot enters), so the look round is
	// paid there and nowhere else.
	class FPlayerBotTriggerStoneNear
	{
		public:
			FPlayerBotTriggerStoneNear(long x, long y, int range) :
				m_x(x), m_y(y), m_range(range), m_found(false) {}

			void operator () (LPENTITY entity)
			{
				if (m_found || !entity || !entity->IsType(ENTITY_CHARACTER))
					return;
				LPCHARACTER stone = static_cast<LPCHARACTER>(entity);
				if (stone->IsStone() && !stone->IsDead() &&
						IsPlayerBotDungeonTriggerStone(stone->GetRaceNum()) &&
						DISTANCE_APPROX(stone->GetX() - m_x, stone->GetY() - m_y) <= m_range)
					m_found = true;
			}

			bool Found() const { return m_found; }

		private:
			long m_x;
			long m_y;
			int m_range;
			bool m_found;
	};

	bool IsPlayerBotSplashNearTriggerStone(LPCHARACTER ch, LPCHARACTER target, DWORD skillVnum)
	{
		// Climbing with a player, the stone is the floor's objective and a
		// splash that reaches it is welcome.
		if (IsPlayerBotClimbingWithPlayer(ch) || IsPlayerBotTowerRaider(ch))
			return false;
		if (!ch || !target || ch->GetMapIndex() != PLAYERBOT_MAP_DEMON_TOWER || !ch->GetSectree())
			return false;
		CSkillProto* proto = CSkillManager::instance().Get(skillVnum);
		const int reach = (proto && proto->iSplashRange > 0
				? proto->iSplashRange : PLAYERBOT_SPLASH_STONE_DEFAULT_RANGE) + PLAYERBOT_SPLASH_STONE_MARGIN;
		// Round the caster and round the target: a splash is centred on one or
		// the other.
		FPlayerBotTriggerStoneNear nearCaster(ch->GetX(), ch->GetY(), reach);
		ch->GetSectree()->ForEachAround(nearCaster);
		if (nearCaster.Found())
			return true;
		FPlayerBotTriggerStoneNear nearTarget(target->GetX(), target->GetY(), reach);
		ch->GetSectree()->ForEachAround(nearTarget);
		return nearTarget.Found();
	}

	bool ExecutePlayerBotAttackSkill(LPCHARACTER ch, LPCHARACTER target, TPlayerBotAIState& state, DWORD dwNow)
	{
		// Under a polymorph marble the engine refuses every skill - five
		// separate IsPolymorphed() returns in char_skill.cpp - so a bot that
		// kept casting spent its whole rotation on refusals and swung at
		// nothing in between. The marble is used on a boss precisely because
		// the plain attack is what it multiplies, so this is also the right
		// thing to do rather than merely the cheap one ("na marmurach nie
		// uzywa sie skilli", Tieru).
		if (!ch || !target || ch->GetSkillGroup() == 0 || ch->IsPolymorphed() ||
				dwNow < state.dwNextSkillCastTime)
			return false;
		// A character is struck through the same gate as a swing, or the cast
		// animation plays at somebody nothing can hurt.
		if (target->IsPC() && !CanPlayerBotStrikeCharacter(ch, target))
			return false;
		LPITEM archerBow = NULL;
		LPITEM archerArrow = NULL;
		if (ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2)
		{
			// An Archer on a stone holds its dagger (bMeleeForStone), and every
			// Archer skill is SKILL_FLAG_USE_ARROW_DAMAGE: without a bow the
			// engine sets atk to 0 and the cast is an animation lock for nothing.
			// Plain swings until the bow is back.
			LPITEM held = ch->GetWear(WEAR_WEAPON);
			if (!held || held->GetType() != ITEM_WEAPON || held->GetSubType() != WEAPON_BOW)
				return false;
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
			if (target->IsStone() && IsPlayerBotSplashSkill(skillVnum))
				continue;
			if (IsPlayerBotSplashSkill(skillVnum) && IsPlayerBotSplashNearTriggerStone(ch, target, skillVnum))
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
						ch->GetPlayerID(), ch->GetName(), skillVnum, (DWORD)target->GetVID());
				return true;
			}
		}

		return false;
	}

	// A skill cast in a duel, on the duel's clock. A duel is short and the
	// rotation is the fight: on the hunt's clock (PLAYERBOT_SKILL_ATTACK_INTERVAL,
	// a Shaman's six seconds) a duel of twenty seconds saw one skill, and a
	// warrior's Wir Miecza and Szarza never came round.
	bool CastPlayerBotDuelSkill(LPCHARACTER ch, LPCHARACTER foe, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ExecutePlayerBotAttackSkill(ch, foe, state, dwNow))
			return false;
		const DWORD interval = ch->GetJob() == JOB_SHAMAN
				? PLAYERBOT_DUEL_SHAMAN_SKILL_INTERVAL : PLAYERBOT_DUEL_SKILL_INTERVAL;
		state.dwNextSkillCastTime = std::min(state.dwNextSkillCastTime, dwNow + interval);
		return true;
	}

	// The skill that closes a gap, per build: Szarza (5) for the body warrior,
	// Uderzenie Miecza (20, whose target range is 1200) for the mental one.
	DWORD GetPlayerBotDuelGapCloser(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_WARRIOR)
			return 0;
		if (ch->GetSkillGroup() == 1)
			return 5;
		return ch->GetSkillGroup() == 2 ? 20 : 0;
	}

	// A warrior charges a duellist standing off instead of walking up to him, as
	// a player does. Only from as far as the lunge carries -
	// PLAYERBOT_DUEL_CHARGE_RANGE - so the blow it lands is one that could land,
	// and the walk after the cast is the lunge itself. The cast is the
	// rotation's own: UseSkill, ComputeSkill, the motion packet.
	bool TryPlayerBotDuelGapCloser(LPCHARACTER ch, LPCHARACTER foe, TPlayerBotAIState& state,
			DWORD dwNow, int distance)
	{
		const DWORD skill = GetPlayerBotDuelGapCloser(ch);
		if (!ch || !foe || skill == 0 || distance < PLAYERBOT_DUEL_CHARGE_MIN_RANGE ||
				distance > PLAYERBOT_DUEL_CHARGE_RANGE || ch->GetSkillLevel(skill) == 0 ||
				ch->IsRiding() || ch->IsPolymorphed() ||
				dwNow < state.dwNextSkillCastTime || dwNow < state.dwNextAttackTime ||
				!CanPlayerBotStrikeCharacter(ch, foe))
			return false;
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetRotationToXY(foe->GetX(), foe->GetY());
		if (!ch->UseSkill(skill, foe))
			return false;
		ch->ComputeSkill(skill, foe);
		SendPlayerBotSkillPacket(ch, skill);
		state.dwLastBotSkillTime = dwNow;
		state.dwLastCombatActionTime = dwNow;
		state.dwNextSkillCastTime = dwNow + PLAYERBOT_DUEL_SKILL_INTERVAL;
		state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
		MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 4, false, false);
		sys_log(0, "PLAYERBOT_PVP: charged pid=%u name=%s foe_pid=%u skill=%u dist=%d",
				ch->GetPlayerID(), ch->GetName(), foe->GetPlayerID(), skill, distance);
		return true;
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

}

#endif
