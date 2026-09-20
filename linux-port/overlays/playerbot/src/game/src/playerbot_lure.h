#ifndef __INC_METIN2_PLAYERBOT_LURE_H__
#define __INC_METIN2_PLAYERBOT_LURE_H__

// The Archer's job in a party: bring the monsters to the people who can kill
// them.
//
// This replaces the occasional extra shot the Archer used to take mid-fight.
// That version tagged one distant monster, invented a damage number when the
// real one came out too low, and then went back to its own target - so nothing
// about it was a pull: the monster arrived at whoever happened to be nearest,
// or did not arrive at all, and nobody was waiting for it.
//
// A course is a whole errand, and it either delivers monsters or says why not:
//
//   WAIT_READY -> PLAN -> APPROACH -> TAG -> CONFIRM -> (APPROACH | RETURN)
//                 -> HANDOFF -> RECOVER -> WAIT_READY
//
// The three things this is built around, because each of them is how a luring
// bot goes wrong:
//
// - **A pull is what came back, not what was shot at.** CONFIRM counts the live
//   monsters actually chasing the Archer, so a miss, a monster killed by the
//   arrow, and a pack that never woke up all count as nothing. "Oddano strzal"
//   is not a metric.
// - **The party has to be standing there.** One lurer per party, and the
//   receivers are counted alive, on this map and around the gathering point -
//   with the Archer itself excluded from that test, because walking away is its
//   whole job and must not cancel its own course.
// - **Nothing here touches the engine's aggro.** No SetVictim on the monster,
//   no clearing of aggro tables: the receivers take the monsters over by
//   fighting them, the way a party does, and a handover may be partial.
//
// A person may also ask for it, and that one exception is the last of those
// three: "luruj" in a whisper (playerbot_chat_trade.h) makes the course theirs
// until "przestan lurowac", and the pack is put on them with the engine's own
// aggro calls rather than left for them to beat off the bot. Everything else
// about the course is the same machinery, with the rules that exist to keep a
// bot from luring for nobody - three members, a receiver that is not a person,
// a frontier map, the role's own pacing - giving way to the fact that somebody
// asked.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_targeting.h, whose pull-target finder,
// aggressor count and ordinary bow shot this reuses rather than growing second
// versions of.

namespace
{
	// Which bot lures for which party. Keyed by the leader's player id and
	// resolved again every tick - a character pointer must not outlive the tick
	// that found it, and a party that changes leader is a party whose claim has
	// to be made again.
	struct TPlayerBotLureClaim
	{
		DWORD dwLurerPID;
		DWORD dwSessionId;
		DWORD dwExpireTime;

		TPlayerBotLureClaim() : dwLurerPID(0), dwSessionId(0), dwExpireTime(0) {}
	};

	// Keyed by the party's leader for the bots' own role and by the person who
	// gave the order for theirs, because two people in one party may each have
	// a bot pulling for them and neither is the other's leader. Released by
	// walking the map for this bot rather than by recomputing the key: by the
	// time a course ends, what the key would be has often changed.
	std::map<DWORD, TPlayerBotLureClaim> s_mapPlayerBotLureClaims;
	DWORD s_dwPlayerBotLureNextSessionId = 1;

	// One party member as the course needs to see it. Copied out of the party
	// rather than held as pointers: the decisions below take several steps and
	// nothing here may still be a character by the end of them.
	struct TPlayerBotLureMember
	{
		DWORD dwPID;
		long x;
		long y;
		int iMaxHP;
		bool bBot;
		bool bCanReceive;
		bool bFighting;

		TPlayerBotLureMember() :
			dwPID(0), x(0), y(0), iMaxHP(0),
			bBot(false), bCanReceive(false), bFighting(false)
		{
		}
	};

	class CPlayerBotLureRoster
	{
		public:
			CPlayerBotLureRoster() {}

			void operator () (LPCHARACTER member)
			{
				if (!member || member->IsDead())
					return;
				TPlayerBotLureMember row;
				row.dwPID = member->GetPlayerID();
				row.x = member->GetX();
				row.y = member->GetY();
				row.iMaxHP = member->GetMaxHP();
				row.bBot = member->GetDesc() && member->GetDesc()->IsBot();
				row.bFighting = member->GetVictim() != NULL;
				// Who can be asked to hold a pack: a bot that fights at arm's
				// length. A second Archer is a poor anchor for a pull and a human
				// is not ours to give orders to, so neither is chosen - both
				// still count towards the party being big enough.
				row.bCanReceive = row.bBot && !IsPlayerBotArcher(member);
				m_members.push_back(row);
			}

			std::vector<TPlayerBotLureMember> m_members;
	};

	// Monsters that are fighting somebody in this party near the gathering
	// point. This is what "the party is still busy" means, and what tells a
	// delivered monster from one that is still chasing the Archer.
	class CCountPlayerBotLureEngaged
	{
		public:
			CCountPlayerBotLureEngaged(LPCHARACTER owner, long anchorX, long anchorY) :
				m_owner(owner), m_anchorX(anchorX), m_anchorY(anchorY),
				m_onParty(0), m_onOwner(0)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (!candidate->IsMonster() || candidate->IsDead())
					return true;
				LPCHARACTER victim = candidate->GetVictim();
				if (!victim || !victim->IsPC())
					return true;
				if (DISTANCE_APPROX(candidate->GetX() - m_anchorX,
						candidate->GetY() - m_anchorY) > PLAYERBOT_LURE_ANCHOR_RADIUS)
					return true;
				if (victim == m_owner)
					++m_onOwner;
				else if (m_owner->GetParty() &&
						m_owner->GetParty()->IsMember(victim->GetPlayerID()))
					++m_onParty;
				return true;
			}

			int OnParty() const { return m_onParty; }
			int OnOwner() const { return m_onOwner; }

		private:
			LPCHARACTER m_owner;
			long m_anchorX;
			long m_anchorY;
			int m_onParty;
			int m_onOwner;
	};

	void CountPlayerBotLureEngaged(LPCHARACTER ch, long anchorX, long anchorY,
			int* onParty, int* onOwner)
	{
		if (onParty)
			*onParty = 0;
		if (onOwner)
			*onOwner = 0;
		if (!ch || !ch->GetSectree())
			return;
		CCountPlayerBotLureEngaged counter(ch, anchorX, anchorY);
		ch->GetSectree()->ForEachAround(counter);
		if (onParty)
			*onParty = counter.OnParty();
		if (onOwner)
			*onOwner = counter.OnOwner();
	}

	// The pack this course should go and wake up.
	//
	// The multi-pull's finder was tried first and answered "nothing" on every
	// course: it looks within 2200 for a monster of the puller's own level that
	// nobody has claimed, which on a map carrying eight hundred bots describes
	// the ground the party is already standing on and nothing else. A lure
	// wants the opposite - a pack far enough out that the party has not reached
	// it, and clear of the fight already in progress - so it asks its own
	// question. What is worth pulling once found is still the shared combat
	// policy's answer, not this one's.
	// The window a course looks through. Two roles ask opposite questions of
	// the same finder - "a pack nobody has reached" and "whatever stands round
	// this person" - so the numbers are an argument rather than a constant.
	struct TPlayerBotLureSearch
	{
		int iMinDistance;
		int iMaxDistance;
		int iAnchorClearance;
		int iSeparation;
		int iMaxLevel;
	};

	class CFindPlayerBotLurePack
	{
		public:
			CFindPlayerBotLurePack(LPCHARACTER owner, long anchorX, long anchorY,
					const std::vector<PIXEL_POSITION>& taken,
					const TPlayerBotLureSearch& search) :
				m_owner(owner), m_anchorX(anchorX), m_anchorY(anchorY),
				m_taken(taken), m_search(search), m_bestVID(0), m_bestScore(INT_MAX),
				m_seen(0), m_busy(0), m_level(0), m_tooClose(0), m_tooFar(0), m_anchor(0),
				m_reserved(0), m_claimed(0), m_unreachable(0)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || !candidate->IsMonster() ||
						candidate->IsStone() || candidate->IsDead() ||
						candidate->GetMobRank() >= MOB_RANK_BOSS ||
						candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(),
								candidate->GetX(), candidate->GetY()))
					return true;
				// From here every rejection is counted. A course that finds
				// nothing says which rule emptied the field, because "no_pack"
				// on its own is a symptom and could mean any of six things.
				++m_seen;
				if (candidate->GetVictim() != NULL)
				{
					++m_busy;
					return true;
				}
				if ((int)candidate->GetLevel() > m_search.iMaxLevel)
				{
					++m_level;
					return true;
				}

				const int fromMe = DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				// Counted apart, because they ask for opposite corrections and
				// one number cannot say which. The first course run after the
				// party threshold was lowered rejected 114 of 120 monsters here
				// and the log could only say "range".
				if (fromMe < m_search.iMinDistance)
				{
					++m_tooClose;
					return true;
				}
				if (fromMe > m_search.iMaxDistance)
				{
					++m_tooFar;
					return true;
				}
				if (DISTANCE_APPROX(candidate->GetX() - m_anchorX,
						candidate->GetY() - m_anchorY) < m_search.iAnchorClearance)
				{
					++m_anchor;
					return true;
				}
				for (size_t i = 0; i < m_taken.size(); ++i)
				{
					if (DISTANCE_APPROX(candidate->GetX() - m_taken[i].x,
							candidate->GetY() - m_taken[i].y) < m_search.iSeparation)
					{
						++m_reserved;
						return true;
					}
				}
				if (IsTargetClaimedByAnotherBot(m_owner, candidate->GetVID()))
				{
					++m_claimed;
					return true;
				}
				if (!IsPlayerBotReachable(m_owner->GetMapIndex(),
						m_owner->GetX(), m_owner->GetY(),
						candidate->GetX(), candidate->GetY()))
				{
					++m_unreachable;
					return true;
				}

				// Nearest wins - the walk out is time the party spends waiting -
				// with a little jitter so two Archers on one map do not queue up
				// behind the same pack.
				const int score = fromMe + (int)(PlayerBotNavHash(
						m_owner->GetPlayerID() ^ candidate->GetVID()) % 250U);
				if (score < m_bestScore)
				{
					m_bestScore = score;
					m_bestVID = candidate->GetVID();
				}
				return true;
			}

			DWORD GetBestVID() const { return m_bestVID; }

			void Explain(char* out, size_t size) const
			{
				snprintf(out, size,
						"seen=%d busy=%d level=%d too_close=%d too_far=%d anchor=%d reserved=%d claimed=%d unreachable=%d",
						m_seen, m_busy, m_level, m_tooClose, m_tooFar, m_anchor,
						m_reserved, m_claimed, m_unreachable);
			}

		private:
			LPCHARACTER m_owner;
			long m_anchorX;
			long m_anchorY;
			const std::vector<PIXEL_POSITION>& m_taken;
			const TPlayerBotLureSearch& m_search;
			DWORD m_bestVID;
			int m_bestScore;
			int m_seen;
			int m_busy;
			int m_level;
			int m_tooClose;
			int m_tooFar;
			int m_anchor;
			int m_reserved;
			int m_claimed;
			int m_unreachable;
	};

	// The window for a course, by who asked for it. The commander is the person
	// on an order and NULL for the bots' own role.
	TPlayerBotLureSearch GetPlayerBotLureSearch(LPCHARACTER ch, LPCHARACTER commander)
	{
		TPlayerBotLureSearch search;
		search.iMaxDistance = PLAYERBOT_LURE_MAX_PACK_DISTANCE;
		if (commander)
		{
			search.iMinDistance = PLAYERBOT_LURE_PLAYER_MIN_PACK_DISTANCE;
			search.iAnchorClearance = PLAYERBOT_LURE_PLAYER_ANCHOR_CLEARANCE;
			search.iSeparation = PLAYERBOT_LURE_PLAYER_GROUP_SEPARATION;
			search.iMaxLevel = (int)std::max(ch->GetLevel(), commander->GetLevel()) +
					PLAYERBOT_LURE_PLAYER_MAX_LEVEL_OVER;
		}
		else
		{
			search.iMinDistance = PLAYERBOT_LURE_MIN_PACK_DISTANCE;
			search.iAnchorClearance = PLAYERBOT_LURE_ANCHOR_CLEARANCE;
			search.iSeparation = PLAYERBOT_LURE_GROUP_SEPARATION;
			search.iMaxLevel = (int)ch->GetLevel() + PLAYERBOT_LURE_MAX_LEVEL_OVER;
		}
		return search;
	}

	LPCHARACTER FindPlayerBotLurePack(LPCHARACTER ch, long anchorX, long anchorY,
			const std::vector<PIXEL_POSITION>& taken,
			const TPlayerBotLureSearch& search, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		CFindPlayerBotLurePack finder(ch, anchorX, anchorY, taken, search);
		ch->GetSectree()->ForEachAround(finder);
		if (finder.GetBestVID() == 0)
		{
			char why[192];
			finder.Explain(why, sizeof(why));
			PlayerBotLogThrottled("lure_no_pack", dwNow,
					"PLAYERBOT_LURE: no pack pid=%u name=%s map=%ld level=%u max_level=%d min=%d %s",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
					ch->GetLevel(), search.iMaxLevel, search.iMinDistance, why);
			return NULL;
		}
		return CHARACTER_MANAGER::instance().Find(finder.GetBestVID());
	}

	const char* GetPlayerBotLureStageName(BYTE stage)
	{
		switch (stage)
		{
			case LURE_STAGE_PLAN:     return "plan";
			case LURE_STAGE_APPROACH: return "approach";
			case LURE_STAGE_TAG:      return "tag";
			case LURE_STAGE_CONFIRM:  return "confirm";
			case LURE_STAGE_RETURN:   return "return";
			case LURE_STAGE_HANDOFF:  return "handoff";
			case LURE_STAGE_RECOVER:  return "recover";
			default:                  return "none";
		}
	}

	void SetPlayerBotLureStage(LPCHARACTER ch, TPlayerBotAIState& state,
			BYTE stage, DWORD dwNow)
	{
		if (state.bLureStage == stage)
			return;
		state.bLureStage = stage;
		state.dwLureStageTime = dwNow;
		sys_log(1, "PLAYERBOT_LURE: stage pid=%u name=%s session=%u stage=%s groups=%u/%u chasing=%d",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?",
				state.dwLureSessionId, GetPlayerBotLureStageName(stage),
				(unsigned int)state.bLureGroupsTagged,
				(unsigned int)state.bLureGroupsPlanned, state.iLureChasing);
	}

	// End of a course, whatever happened. One line per session with everything
	// the acceptance tests ask about, and the party's claim released - a lurer
	// that stopped answering must not hold the role.
	void FinishPlayerBotLure(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		if (state.bLureStage == LURE_STAGE_NONE && state.dwLureSessionId == 0)
			return;

		// By the bot, not by the key: a course ends for reasons that have
		// already changed what the key would be - the party lost its leader,
		// the person took their order back - and a claim left behind holds the
		// role against every other Archer for PLAYERBOT_LURE_SESSION_TTL. A bot
		// can only ever hold one, so there is nothing to disambiguate.
		if (ch)
			for (std::map<DWORD, TPlayerBotLureClaim>::iterator it =
					s_mapPlayerBotLureClaims.begin();
					it != s_mapPlayerBotLureClaims.end(); )
			{
				if (it->second.dwLurerPID == ch->GetPlayerID())
					s_mapPlayerBotLureClaims.erase(it++);
				else
					++it;
			}

		sys_log(0, "PLAYERBOT_LURE: finished pid=%u name=%s session=%u stage=%s groups=%u/%u tagged=%u delivered=%d chasing=%d course_ms=%u hp=%d/%d streak=%u reason=%s",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?",
				state.dwLureSessionId, GetPlayerBotLureStageName(state.bLureStage),
				(unsigned int)state.bLureGroupsTagged,
				(unsigned int)state.bLureGroupsPlanned,
				(unsigned int)state.bLureTagAttempts, state.iLureDelivered,
				state.iLureChasing,
				state.dwLureCourseTime != 0 ? dwNow - state.dwLureCourseTime : 0,
				ch ? ch->GetHP() : 0, ch ? ch->GetMaxHP() : 0,
				(unsigned int)state.bLureGoodCourses, reason ? reason : "?");

		state.bLureStage = LURE_STAGE_NONE;
		state.dwLureSessionId = 0;
		state.dwLureStageTime = 0;
		state.dwLureCourseTime = 0;
		state.dwLureShotTime = 0;
		state.dwLureTargetVID = 0;
		state.dwLureReceiverPID = 0;
		state.bLureGroupsPlanned = 0;
		state.bLureGroupsTagged = 0;
		state.bLureBudget = 0;
		state.bLureTagAttempts = 0;
		state.iLureDelivered = 0;
		state.iLureChasing = 0;
		state.vecMultiPullCenters.clear();
		// A standing order is not a bot pacing itself: the person is waiting
		// for the next pack, so the wait between two courses is seconds.
		state.dwLureNextTime = state.dwLurePlayerPID != 0
				? dwNow + number((int)PLAYERBOT_LURE_PLAYER_COOLDOWN_MIN,
						(int)PLAYERBOT_LURE_PLAYER_COOLDOWN_MAX)
				: dwNow + number((int)PLAYERBOT_LURE_COOLDOWN_MIN,
						(int)PLAYERBOT_LURE_COOLDOWN_MAX);
		if (ch)
			ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
	}

	// Why a course has not started yet, for the one person who is standing
	// there waiting for it. It lives beside the pass rather than in
	// TPlayerBotAIState - a new field there has to be initialised in
	// declaration order or -Wreorder fires, and this is nobody else's business
	// - and it is read through the forward declaration in playerbot_status.h,
	// which is included above this file. "Czekam, zeby lurowac dla X" for
	// twenty minutes is what a person sees when one of the gates below is shut
	// for good (marcinxboss, 20 September); saying which one turns the next
	// report into one that can be read.
	std::map<DWORD, const char*> s_mapPlayerBotLureWaitReason;

	void NotePlayerBotLureWait(LPCHARACTER ch, const char* reason, bool forPlayer)
	{
		if (!ch)
			return;
		std::map<DWORD, const char*>::iterator it =
				s_mapPlayerBotLureWaitReason.find(ch->GetPlayerID());
		const char* was = it != s_mapPlayerBotLureWaitReason.end() ? it->second : NULL;
		// Nothing is kept for the bots' own role. Every bot in a party reaches
		// this hook, so recording all of them would be an entry per bot in the
		// world for something only a person's order ever reads - and the end of
		// an order comes through here with forPlayer false, which is what takes
		// the entry away again.
		if (reason == NULL || !forPlayer)
		{
			if (it != s_mapPlayerBotLureWaitReason.end())
				s_mapPlayerBotLureWaitReason.erase(it);
			return;
		}
		s_mapPlayerBotLureWaitReason[ch->GetPlayerID()] = reason;
		// Only a change, and only where somebody is actually waiting: the
		// bots' own role has an Archer in every party on the map and its gates
		// move every couple of seconds, while a person who asked for a pull is
		// one person. The map is kept for both, because the status reads it.
		if (was != reason)
			sys_log(0, "PLAYERBOT_LURE: waiting pid=%u name=%s reason=%s",
					ch->GetPlayerID(), ch->GetName(), reason);
	}

	const char* GetPlayerBotLureWaitReason(DWORD dwPID)
	{
		if (dwPID == 0)
			return NULL;
		std::map<DWORD, const char*>::const_iterator it =
				s_mapPlayerBotLureWaitReason.find(dwPID);
		return it != s_mapPlayerBotLureWaitReason.end() ? it->second : NULL;
	}

	// The person this bot is luring for, or NULL - resolved every tick, because
	// a character pointer must not outlive the tick that found it and an order
	// whose player has gone is an order that has ended. What ends it is written
	// into *why*; a dead player only suspends it, because standing up takes
	// twenty seconds and the order was not given for twenty seconds.
	LPCHARACTER GetPlayerBotLureCommander(LPCHARACTER ch, const TPlayerBotAIState& state,
			DWORD dwNow, const char** why, bool* waiting)
	{
		if (why)
			*why = NULL;
		if (waiting)
			*waiting = false;
		if (!ch || state.dwLurePlayerPID == 0)
			return NULL;
		LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(state.dwLurePlayerPID);
		const char* reason = NULL;
		if (!player || !player->GetDesc())
			reason = "player_gone";
		else if (!ch->GetParty() || ch->GetParty() != player->GetParty())
			reason = "party_over";
		else if (player->GetMapIndex() != ch->GetMapIndex())
			reason = "player_other_map";
		else if (DISTANCE_APPROX(ch->GetX() - player->GetX(),
				ch->GetY() - player->GetY()) > PLAYERBOT_LURE_PLAYER_MAX_SEPARATION)
			reason = "player_too_far";
		else if (dwNow - state.dwLurePlayerTime > PLAYERBOT_LURE_PLAYER_ORDER_TTL)
			reason = "order_expired";
		if (reason)
		{
			if (why)
				*why = reason;
			return NULL;
		}
		if (player->IsDead())
		{
			if (waiting)
				*waiting = true;
			return NULL;
		}
		return player;
	}

	// The order is over. Cleared, logged, and - where there is still somebody to
	// tell - said out loud, because a bot that silently stops doing what it was
	// asked to do reads as a bot that broke.
	void EndPlayerBotLureOrder(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		if (!ch || state.dwLurePlayerPID == 0)
			return;
		LPCHARACTER player = CHARACTER_MANAGER::instance().FindByPID(state.dwLurePlayerPID);
		sys_log(0, "PLAYERBOT_LURE: order over pid=%u name=%s player_pid=%u held_ms=%u reason=%s",
				ch->GetPlayerID(), ch->GetName(), state.dwLurePlayerPID,
				state.dwLurePlayerTime != 0 ? dwNow - state.dwLurePlayerTime : 0,
				reason ? reason : "?");
		if (player && player->GetDesc() && ch->GetParty() &&
				ch->GetParty() == player->GetParty())
		{
			if (strcmp(reason, "player_too_far") == 0)
				SendPlayerBotWhisper(ch, player, "Zgubilem cie - koncze lurowanie");
			else if (strcmp(reason, "order_expired") == 0)
				SendPlayerBotWhisper(ch, player, "Koncze lurowanie. Napisz \"luruj\", jesli mam dalej");
		}
		state.dwLurePlayerPID = 0;
		state.dwLurePlayerTime = 0;
		NotePlayerBotLureWait(ch, NULL, false);
	}

	// Give the pack to the person who asked for it.
	//
	// The bots' own role never does this: a receiver takes a monster over by
	// hitting it, which is what a party does, and a partial handover is a real
	// outcome. A person asked for the pack *on them*, though, and waiting for a
	// bot to be beaten off it is not that - so the two calls the engine has for
	// it, in the order that makes them stick:
	//
	//   UpdateAggrPoint puts the person in the monster's damage map with more
	//   aggro than the two arrows of a tag can have earned - the monster's own
	//   maximum health, a number it supplies itself - as DAMAGE_TYPE_SPECIAL,
	//   the one type UpdateAggrPointEx does not then spread over the victim's
	//   party. On its own it is often refused: it ends in ChangeVictimByAggro,
	//   which does nothing for three seconds after any victim change, and a
	//   monster that has just turned to chase the Archer has had one.
	//   SetVictim therefore turns it, and restarts that three-second lock,
	//   which is long enough for the person to land the blows that keep it.
	//
	// A monster that cannot attack the person - they are standing in a safe
	// zone, most of the time - is left where it is: the engine's own rule, and
	// not one to be written round here.
	class CGivePlayerBotLurePack
	{
		public:
			CGivePlayerBotLurePack(LPCHARACTER owner, LPCHARACTER to) :
				m_owner(owner), m_to(to), m_given(0), m_unreachable(0)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER monster = static_cast<LPCHARACTER>(entity);
				if (monster == m_owner || !monster->IsMonster() || monster->IsDead() ||
						monster->GetVictim() != m_owner)
					return true;
				if (DISTANCE_APPROX(monster->GetX() - m_to->GetX(),
						monster->GetY() - m_to->GetY()) > PLAYERBOT_LURE_ANCHOR_RADIUS)
					return true;
				if (!battle_is_attackable(monster, m_to))
				{
					++m_unreachable;
					return true;
				}
				monster->UpdateAggrPoint(m_to, DAMAGE_TYPE_SPECIAL, monster->GetMaxHP());
				monster->SetVictim(m_to);
				++m_given;
				return true;
			}

			int Given() const { return m_given; }
			int Unreachable() const { return m_unreachable; }

		private:
			LPCHARACTER m_owner;
			LPCHARACTER m_to;
			int m_given;
			int m_unreachable;
	};

	int GivePlayerBotLurePack(LPCHARACTER ch, LPCHARACTER to, int* unreachable)
	{
		if (unreachable)
			*unreachable = 0;
		if (!ch || !to || !ch->GetSectree())
			return 0;
		CGivePlayerBotLurePack giver(ch, to);
		ch->GetSectree()->ForEachAround(giver);
		if (unreachable)
			*unreachable = giver.Unreachable();
		return giver.Given();
	}

	// Has this bot got a bow it can actually shoot? The lure fires the ordinary
	// arrow through ExecutePlayerBotBasicAttack, so it must not start a course
	// it has no ammunition to finish.
	bool CanPlayerBotLureShoot(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotArcher(ch))
			return false;
		LPITEM bow = NULL;
		LPITEM arrow = NULL;
		if (!EnsurePlayerBotArrowsEquipped(ch))
			return false;
		return ch->GetArrowAndBow(&bow, &arrow, 1) == 1;
	}

	// The whole course, one stage per tick. Returns true when it has taken the
	// tick - including while it is waiting for the bow's own rhythm, because
	// falling through to the ordinary grind in the middle of a pull is how the
	// Archer ends up fighting what it was supposed to be delivering.
	bool HandlePlayerBotLureCourse(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const bool inSession = state.bLureStage != LURE_STAGE_NONE;

		// The person's order first, because it decides which rules below apply
		// and because it has to be cleared even on the paths that return early:
		// a party that ended is exactly one of them, and an order nobody
		// cancelled would otherwise outlive the party it was given in.
		const char* orderOver = NULL;
		bool commanderWaiting = false;
		LPCHARACTER commander = GetPlayerBotLureCommander(ch, state, dwNow,
				&orderOver, &commanderWaiting);
		if (orderOver)
		{
			// The course first, while the order is still on the state: that is
			// what decides the pace of the next one and what a log line says
			// the session was for.
			if (inSession)
				FinishPlayerBotLure(ch, state, dwNow, orderOver);
			EndPlayerBotLureOrder(ch, state, dwNow, orderOver);
			return false;
		}
		// A person on the ground is not a person to bring a pack to. The order
		// stands; the course waits for them to get up.
		if (commanderWaiting)
		{
			if (inSession && state.bLureStage != LURE_STAGE_RETURN &&
					state.bLureStage != LURE_STAGE_HANDOFF &&
					state.bLureStage != LURE_STAGE_RECOVER)
				FinishPlayerBotLure(ch, state, dwNow, "player_down");
			return false;
		}
		const bool forPlayer = commander != NULL;
		const int minParty = forPlayer ? PLAYERBOT_LURE_PLAYER_MIN_PARTY_MEMBERS
				: PLAYERBOT_ARCHER_LURE_MIN_PARTY_MEMBERS;

		// Saving your own life, the errand you are already on, and being dead
		// all outrank the role. A course in progress ends here rather than
		// being suspended: half a pull is not a state worth keeping.
		//
		// Named one by one rather than counted. "busy" stood for seven flags
		// and a map rule at once, and the first report that needed it could not
		// be read at all: a person's order on a village map logged
		// "reason=busy" for four minutes while the bot walked its town errand
		// (l0st3k, 20 September). A conjunction that refuses has to say which
		// clause did it.
		const char* ineligible = NULL;
		if (!ch || !ch->GetParty())
			ineligible = "no_party";
		else if (ch->IsDead())
			ineligible = "dead";
		else if (!IsPlayerBotArcher(ch))
			ineligible = "no_bow";
		else if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			ineligible = "safe_zone";
		// "In a party, on a big spot" is the whole point of the role, and there
		// was no map rule at all: an Archer with a party anywhere outside a safe
		// zone planned a course. Measured on Yongan - a party of six, five
		// receivers ready, and "no pack seen=0" a second later, because a first
		// village has no pack to pull. The frontier maps are where the packs and
		// the party cohort both are. A person who asked for a pull has said
		// where they hunt by standing there, so the rule is theirs to make.
		else if (!forPlayer && !IsPlayerBotFrontierMapIndex(ch->GetMapIndex()))
			ineligible = "off_frontier";
		else if (state.bTacticalRetreat)
			ineligible = "retreat";
		else if (state.bRecoveringAfterDeath)
			ineligible = "recovering";
		else if (state.bVisitingShop)
			ineligible = "town_visit";
		else if (state.bVisitingBiologist)
			ineligible = "biologist";
		else if (state.bVisitingStable)
			ineligible = "stable";
		else if (state.bMarketTrip)
			ineligible = "market_trip";
		else if (state.bFishingSession)
			ineligible = "fishing";
		if (ineligible)
		{
			if (inSession)
				FinishPlayerBotLure(ch, state, dwNow, "ineligible");
			NotePlayerBotLureWait(ch, ineligible, forPlayer);
			return false;
		}

		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		// The leader anchors the bots' own role - it is who the claim is keyed
		// by and who the party is gathered round. On an order it is the person
		// who gave it, who has already been checked for both, and who may well
		// not be leading the party they are in.
		if (!forPlayer && (!leader || leader->GetMapIndex() != ch->GetMapIndex()))
		{
			if (inSession)
				FinishPlayerBotLure(ch, state, dwNow, "no_leader");
			return false;
		}
		const DWORD claimKey = forPlayer ? commander->GetPlayerID()
				: (leader ? leader->GetPlayerID() : 0);

		// Who is here. Counted every tick because a party loses people to
		// death, logout and their own errands while the Archer is away.
		CPlayerBotLureRoster roster;
		ch->GetParty()->ForEachOnMapMember(roster, ch->GetMapIndex());

		int present = 0;
		bool humanPresent = false;
		const TPlayerBotLureMember* receiver = NULL;
		for (size_t i = 0; i < roster.m_members.size(); ++i)
		{
			++present;
			if (!roster.m_members[i].bBot)
				humanPresent = true;
			if (roster.m_members[i].dwPID == ch->GetPlayerID() ||
					!roster.m_members[i].bCanReceive)
				continue;
			// The sturdiest available body, ties broken by player id so two
			// Archers in one party would not pick different anchors.
			if (!receiver || roster.m_members[i].iMaxHP > receiver->iMaxHP ||
					(roster.m_members[i].iMaxHP == receiver->iMaxHP &&
					 roster.m_members[i].dwPID < receiver->dwPID))
				receiver = &roster.m_members[i];
		}

		// On an order the receiver is the person who gave it. The roster refuses
		// a human on purpose - a bot is not to pick a person to hold a pack for
		// it - but this person asked to hold one.
		if (forPlayer)
		{
			receiver = NULL;
			for (size_t i = 0; i < roster.m_members.size(); ++i)
				if (roster.m_members[i].dwPID == commander->GetPlayerID())
				{
					receiver = &roster.m_members[i];
					break;
				}
		}

		if (present < minParty || !receiver)
		{
			// A party that shrank mid-course does not get another pack, but the
			// monsters already following the Archer still have to be brought
			// home - dropping the course here would leave them on a bot that
			// has stopped running anywhere.
			if (!inSession)
				return false;
			if (state.bLureStage != LURE_STAGE_RETURN &&
					state.bLureStage != LURE_STAGE_HANDOFF &&
					state.bLureStage != LURE_STAGE_RECOVER)
			{
				if (state.iLureChasing <= 0)
				{
					FinishPlayerBotLure(ch, state, dwNow, "party_too_small");
					return false;
				}
				SetPlayerBotLureStage(ch, state, LURE_STAGE_RETURN, dwNow);
			}
		}

		// How many of them are standing where the monsters are to be brought.
		// The Archer is left out of this on purpose: it is supposed to be away.
		const long anchorX = state.bLureStage == LURE_STAGE_NONE
				? (receiver ? receiver->x : ch->GetX()) : state.lLureAnchorX;
		const long anchorY = state.bLureStage == LURE_STAGE_NONE
				? (receiver ? receiver->y : ch->GetY()) : state.lLureAnchorY;
		int ready = 0;
		int fighting = 0;
		for (size_t i = 0; i < roster.m_members.size(); ++i)
		{
			if (roster.m_members[i].dwPID == ch->GetPlayerID())
				continue;
			if (DISTANCE_APPROX(roster.m_members[i].x - anchorX,
					roster.m_members[i].y - anchorY) > PLAYERBOT_LURE_ANCHOR_RADIUS)
				continue;
			++ready;
			// Not a condition - a party standing idle is exactly the one worth
			// bringing work to - but it belongs in the line that opens a course,
			// because a handover onto people who were already busy is a
			// different result from one onto people who were waiting.
			if (roster.m_members[i].bFighting)
				++fighting;
		}

		const int hpPercent = ch->GetMaxHP() > 0
				? ch->GetHP() * 100 / ch->GetMaxHP() : 0;

		// ------------------------------------------------------------------
		// WAIT_READY: no session. Everything that has to be true before one
		// starts, and the party's claim on the role.
		// ------------------------------------------------------------------
		if (state.bLureStage == LURE_STAGE_NONE)
		{
			if (dwNow < state.dwLureNextTime)
				return false;
			// Asking again is not free - the busy count below is a sector scan,
			// and an Archer that is not going anywhere would run one four times
			// a second for ever. A failed check therefore costs a couple of
			// seconds of quiet, which no cooldown already running is shortened
			// by.
			if (state.dwLureNextTime < dwNow + PLAYERBOT_LURE_READY_RECHECK)
				state.dwLureNextTime = dwNow + PLAYERBOT_LURE_READY_RECHECK;

			if (present < minParty || !receiver || ready < minParty - 1)
			{
				NotePlayerBotLureWait(ch, "party", forPlayer);
				return false;
			}
			// A course opens at nine tenths of health for a party of bots that
			// is standing and waiting. Beside a person who is fighting, the bot
			// is taking hits, and that gate never opens.
			if (hpPercent < (forPlayer ? PLAYERBOT_LURE_PLAYER_START_HP_PERCENT
					: PLAYERBOT_LURE_START_HP_PERCENT))
			{
				NotePlayerBotLureWait(ch, "hp", forPlayer);
				return false;
			}
			if (!CanPlayerBotLureShoot(ch))
			{
				NotePlayerBotLureWait(ch, "no_bow", forPlayer);
				return false;
			}

			// Not while the party still has its hands full: the point of a
			// course is to keep them fed, not to bury them. That is a rule
			// about bots - it exists so a course does not bury a party that is
			// already busy - and a person who typed "luruj" has said they want
			// the next pack whatever they are holding; if they are swamped they
			// say "przestan lurowac". It is the gate that shut this feature on
			// a real spot: four monsters on the person is an ordinary Sunday,
			// and PLAYERBOT_LURE_BUSY_MONSTERS is three.
			//
			// A pack on the Archer itself still stops it, for both: walking
			// away from monsters that are chasing you is how a pull is lost.
			// With the grinding stopped in the value policy, that count now
			// falls to zero by itself within a few seconds.
			int onParty = 0, onOwner = 0;
			CountPlayerBotLureEngaged(ch, anchorX, anchorY, &onParty, &onOwner);
			if (!forPlayer && onParty > PLAYERBOT_LURE_BUSY_MONSTERS)
			{
				NotePlayerBotLureWait(ch, "party_busy", forPlayer);
				return false;
			}
			if (onOwner > 0)
			{
				NotePlayerBotLureWait(ch, "monsters_on_me", forPlayer);
				return false;
			}

			// One lurer per party. A live claim by somebody else stands; a
			// stale one is taken over, which is what makes a lurer that died or
			// logged out cost the party one expiry and no more.
			TPlayerBotLureClaim& claim = s_mapPlayerBotLureClaims[claimKey];
			if (claim.dwLurerPID != 0 && claim.dwLurerPID != ch->GetPlayerID() &&
					dwNow < claim.dwExpireTime)
			{
				NotePlayerBotLureWait(ch, "another_lurer", forPlayer);
				return false;
			}
			state.dwLureNextTime = 0;
			NotePlayerBotLureWait(ch, NULL, forPlayer);

			state.dwLureSessionId = s_dwPlayerBotLureNextSessionId++;
			claim.dwLurerPID = ch->GetPlayerID();
			claim.dwSessionId = state.dwLureSessionId;
			claim.dwExpireTime = dwNow + PLAYERBOT_LURE_SESSION_TTL;

			// What this party has earned. A course that delivered without
			// costing anybody grows the plan by a group every few courses; a
			// party with a human in it stays at the opening size, because
			// nothing here can promise the human will be fighting.
			const int earned = humanPresent
					? 0 : std::min((int)state.bLureGoodCourses / PLAYERBOT_LURE_GROWTH_STREAK,
							PLAYERBOT_LURE_MAX_GROUPS - PLAYERBOT_LURE_FIRST_GROUPS);
			// A person asked for the monsters round them, so the plan is the
			// gathering rather than one fetched pack; it is bounded by the
			// gather clock, the health gate and the budget like any other.
			state.bLureGroupsPlanned = (BYTE)(forPlayer ? PLAYERBOT_LURE_PLAYER_GROUPS
					: PLAYERBOT_LURE_FIRST_GROUPS + earned);
			state.bLureBudget = (BYTE)(forPlayer ? PLAYERBOT_LURE_PLAYER_BUDGET
					: std::min(PLAYERBOT_LURE_FIRST_BUDGET + earned * 3,
							PLAYERBOT_LURE_MAX_BUDGET));
			state.bLureGroupsTagged = 0;
			state.bLureTagAttempts = 0;
			state.iLureDelivered = 0;
			state.iLureChasing = 0;
			state.dwLureTargetVID = 0;
			state.dwLureReceiverPID = receiver->dwPID;
			state.lLureAnchorX = anchorX;
			state.lLureAnchorY = anchorY;
			state.iLureStartHPPercent = hpPercent;
			state.dwLureCourseTime = dwNow;
			// A course renews the order, so the deadline means "nothing has
			// happened for three quarters of an hour" rather than "you asked
			// three quarters of an hour ago". A person hunting all evening
			// should not have to say it again every forty-five minutes; a bot
			// still holding an order for somebody who wandered off should let
			// go of it.
			if (forPlayer)
				state.dwLurePlayerTime = dwNow;
			state.vecMultiPullCenters.clear();
			ClearPlayerBotRoute(state, true);
			SetPlayerBotLureStage(ch, state, LURE_STAGE_PLAN, dwNow);
			sys_log(0, "PLAYERBOT_LURE: planned pid=%u name=%s session=%u level=%u party=%d ready=%d fighting=%d human=%d for_player=%s receiver_pid=%u anchor=(%ld,%ld) groups=%u budget=%u",
					ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
					ch->GetLevel(), present, ready, fighting, humanPresent ? 1 : 0,
					forPlayer ? commander->GetName() : "-",
					state.dwLureReceiverPID, anchorX, anchorY,
					(unsigned int)state.bLureGroupsPlanned,
					(unsigned int)state.bLureBudget);
			SetPlayerBotAction(state, BOT_ACTION_LURE, dwNow);
			return true;
		}

		// From here a session is running. Keep the claim alive, and keep the
		// Archer out of the party's shared focus: a pack two thousand units
		// away must not become the target the rest of the party walks to.
		{
			std::map<DWORD, TPlayerBotLureClaim>::iterator it =
					s_mapPlayerBotLureClaims.find(claimKey);
			if (it != s_mapPlayerBotLureClaims.end() &&
					it->second.dwLurerPID == ch->GetPlayerID())
				it->second.dwExpireTime = dwNow + PLAYERBOT_LURE_SESSION_TTL;
		}
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_LURE, dwNow);

		if (dwNow - state.dwLureCourseTime > PLAYERBOT_LURE_SESSION_TTL)
		{
			FinishPlayerBotLure(ch, state, dwNow, "session_expired");
			return false;
		}

		// On the way home the gathering point follows the receiver. The party
		// does not stand still while the Archer is away - it is fighting - and
		// coming back to where it stood twenty seconds ago is how a pull gets
		// delivered to an empty field. During the gathering the original point
		// stays put, because that is what bounds how far the course may go.
		if (state.bLureStage == LURE_STAGE_RETURN ||
				state.bLureStage == LURE_STAGE_HANDOFF ||
				state.bLureStage == LURE_STAGE_RECOVER)
		{
			const TPlayerBotLureMember* anchorOn = NULL;
			for (size_t i = 0; i < roster.m_members.size(); ++i)
				if (roster.m_members[i].dwPID == state.dwLureReceiverPID)
				{
					anchorOn = &roster.m_members[i];
					break;
				}
			// The receiver died or left: any other body that can hold a pack
			// will do, and the course is finished rather than abandoned.
			if (!anchorOn && receiver)
			{
				anchorOn = receiver;
				state.dwLureReceiverPID = receiver->dwPID;
			}
			if (anchorOn)
			{
				state.lLureAnchorX = anchorOn->x;
				state.lLureAnchorY = anchorOn->y;
			}
		}

		const int fromAnchor = DISTANCE_APPROX(ch->GetX() - state.lLureAnchorX,
				ch->GetY() - state.lLureAnchorY);

		switch (state.bLureStage)
		{
			case LURE_STAGE_PLAN:
				SetPlayerBotLureStage(ch, state, LURE_STAGE_APPROACH, dwNow);
				return true;

			case LURE_STAGE_APPROACH:
			case LURE_STAGE_TAG:
			case LURE_STAGE_CONFIRM:
			{
				// Reasons to stop gathering and start walking back. Each of
				// them ends the gathering, never the course: whatever is
				// already following has to be taken somewhere.
				const char* stop = NULL;
				if (dwNow - state.dwLureCourseTime > PLAYERBOT_LURE_GATHER_TIME)
					stop = "gather_time";
				else if (fromAnchor > PLAYERBOT_LURE_MAX_COURSE_RANGE)
					stop = "too_far";
				else if (hpPercent < (forPlayer
								? PLAYERBOT_LURE_PLAYER_BREAK_HP_PERCENT
								: PLAYERBOT_LURE_BREAK_HP_PERCENT) ||
						state.iLureStartHPPercent - hpPercent >= (forPlayer
								? PLAYERBOT_LURE_PLAYER_MAX_HP_LOSS_PERCENT
								: PLAYERBOT_LURE_MAX_HP_LOSS_PERCENT))
					stop = "low_hp";
				else if (state.bLureGroupsTagged >= state.bLureGroupsPlanned ||
						state.iLureChasing >= (int)state.bLureBudget)
					stop = "budget";
				else if (!CanPlayerBotLureShoot(ch))
					stop = "no_arrows";
				if (stop)
				{
					if (state.iLureChasing > 0)
					{
						sys_log(0, "PLAYERBOT_LURE: gathering over pid=%u name=%s session=%u groups=%u/%u chasing=%d reason=%s",
								ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
								(unsigned int)state.bLureGroupsTagged,
								(unsigned int)state.bLureGroupsPlanned,
								state.iLureChasing, stop);
						SetPlayerBotLureStage(ch, state, LURE_STAGE_RETURN, dwNow);
						ClearPlayerBotRoute(state, true);
						return true;
					}
					FinishPlayerBotLure(ch, state, dwNow, stop);
					return false;
				}

				LPCHARACTER target = state.dwLureTargetVID != 0
						? CHARACTER_MANAGER::instance().Find(state.dwLureTargetVID) : NULL;
				if (target && (target->IsDead() || !target->IsMonster() ||
						target->GetMapIndex() != ch->GetMapIndex()))
					target = NULL;

				if (state.bLureStage == LURE_STAGE_CONFIRM)
				{
					// Read the reaction, not the shot. Anything that is not a
					// live monster now running at the Archer is not a pull.
					if (dwNow - state.dwLureShotTime < PLAYERBOT_LURE_CONFIRM_DELAY)
						return true;
					const int chasing = CountPlayerBotPullAggressors(ch);
					if (chasing > state.iLureChasing)
					{
						PIXEL_POSITION center;
						center.x = ch->GetX();
						center.y = ch->GetY();
						center.z = 0;
						if (target)
						{
							center.x = target->GetX();
							center.y = target->GetY();
						}
						state.vecMultiPullCenters.push_back(center);
						++state.bLureGroupsTagged;
						sys_log(0, "PLAYERBOT_LURE: pack answered pid=%u name=%s session=%u groups=%u/%u chasing=%d gained=%d",
								ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
								(unsigned int)state.bLureGroupsTagged,
								(unsigned int)state.bLureGroupsPlanned,
								chasing, chasing - state.iLureChasing);
						state.iLureChasing = chasing;
						state.dwLureTargetVID = 0;
						state.bLureTagAttempts = 0;
						SetPlayerBotLureStage(ch, state, LURE_STAGE_APPROACH, dwNow);
						ClearPlayerBotRoute(state, true);
						return true;
					}
					if (dwNow - state.dwLureShotTime < PLAYERBOT_LURE_CONFIRM_TIMEOUT)
						return true;
					// One more arrow at the same pack, then leave it alone. A
					// pack that does not answer twice is a pack that is not
					// coming, and standing there shooting it is the failure
					// this timeout exists to end.
					if (target && state.bLureTagAttempts < 2)
					{
						SetPlayerBotLureStage(ch, state, LURE_STAGE_TAG, dwNow);
						return true;
					}
					if (target)
					{
						PIXEL_POSITION center;
						center.x = target->GetX();
						center.y = target->GetY();
						center.z = 0;
						state.vecMultiPullCenters.push_back(center);
					}
					PlayerBotLogThrottled("lure_no_answer", dwNow,
							"PLAYERBOT_LURE: pack ignored the arrow pid=%u name=%s session=%u attempts=%u",
							ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
							(unsigned int)state.bLureTagAttempts);
					state.dwLureTargetVID = 0;
					state.bLureTagAttempts = 0;
					SetPlayerBotLureStage(ch, state, LURE_STAGE_APPROACH, dwNow);
					return true;
				}

				if (!target)
				{
					// A pack nobody needs is not pulled just because a course
					// is running: the shared combat policy answers here too.
					const TPlayerBotLureSearch search =
							GetPlayerBotLureSearch(ch, commander);
					target = FindPlayerBotLurePack(ch, state.lLureAnchorX,
							state.lLureAnchorY, state.vecMultiPullCenters,
							search, dwNow);
					if (target && !IsPlayerBotTargetWorthNow(ch, target, state, dwNow))
						target = NULL;
					if (!target)
					{
						if (state.iLureChasing > 0)
						{
							SetPlayerBotLureStage(ch, state, LURE_STAGE_RETURN, dwNow);
							ClearPlayerBotRoute(state, true);
							return true;
						}
						FinishPlayerBotLure(ch, state, dwNow, "no_pack");
						return false;
					}
					state.dwLureTargetVID = (DWORD)target->GetVID();
					state.bLureTagAttempts = 0;
					ClearPlayerBotRoute(state, true);
				}

				const int distance = DISTANCE_APPROX(ch->GetX() - target->GetX(),
						ch->GetY() - target->GetY());
				if (distance > PLAYERBOT_LURE_SHOT_RANGE)
				{
					// Walk into range like anything else does - on foot, since
					// the shot has to be taken standing still anyway.
					SetPlayerBotLureStage(ch, state, LURE_STAGE_APPROACH, dwNow);
					MovePlayerBot(ch, target->GetX(), target->GetY(), dwNow, 4,
							true, false, false);
					return true;
				}

				SetPlayerBotLureStage(ch, state, LURE_STAGE_TAG, dwNow);
				if (ch->IsStateMove())
				{
					ch->Stop();
					return true;
				}
				// The bow's own rhythm, from the ordinary attack. Waiting for
				// it is taking the tick, not failing to act.
				if (dwNow < state.dwNextAttackTime)
					return true;
				ch->SetRotationToXY(target->GetX(), target->GetY());
				if (!ExecutePlayerBotBasicAttack(ch, target, state, dwNow))
					return true;
				++state.bLureTagAttempts;
				state.dwLureShotTime = dwNow;
				SetPlayerBotLureStage(ch, state, LURE_STAGE_CONFIRM, dwNow);
				return true;
			}

			case LURE_STAGE_RETURN:
			{
				state.iLureChasing = CountPlayerBotPullAggressors(ch);
				if (dwNow - state.dwLureStageTime > PLAYERBOT_LURE_RETURN_TIME)
				{
					FinishPlayerBotLure(ch, state, dwNow, "return_time");
					return false;
				}
				if (fromAnchor <= PLAYERBOT_LURE_HANDOFF_RANGE)
				{
					state.iLureDelivered = state.iLureChasing;
					sys_log(0, "PLAYERBOT_LURE: back with the party pid=%u name=%s session=%u groups=%u chasing=%d walk_ms=%u",
							ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
							(unsigned int)state.bLureGroupsTagged, state.iLureChasing,
							dwNow - state.dwLureStageTime);
					SetPlayerBotLureStage(ch, state, LURE_STAGE_HANDOFF, dwNow);
					ClearPlayerBotRoute(state, true);
					return true;
				}
				// No skill rotation and no stopping to fight on the way home:
				// a pull that turns into a fight halfway back is a pull that
				// was never delivered.
				MovePlayerBot(ch, state.lLureAnchorX, state.lLureAnchorY, dwNow, 6,
						true, false, false);
				return true;
			}

			case LURE_STAGE_HANDOFF:
			{
				int onParty = 0, onOwner = 0;
				CountPlayerBotLureEngaged(ch, state.lLureAnchorX, state.lLureAnchorY,
						&onParty, &onOwner);
				state.iLureChasing = onOwner;
				if (fromAnchor > PLAYERBOT_LURE_HANDOFF_RANGE)
				{
					MovePlayerBot(ch, state.lLureAnchorX, state.lLureAnchorY, dwNow, 6,
							true, false, false);
					return true;
				}
				// A person asked for the pack *on them*, so it goes on them the
				// tick the bot is back - not after the seven seconds the bots'
				// own handover waits to be judged, which for a person watching
				// is seven seconds of a bot being chewed in front of them.
				if (forPlayer && onOwner > 0)
				{
					int unreachable = 0;
					const int given = GivePlayerBotLurePack(ch, commander, &unreachable);
					sys_log(0, "PLAYERBOT_LURE: pack handed to the player pid=%u name=%s session=%u player=%s given=%d on_me=%d unreachable=%d",
							ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
							commander->GetName(), given, onOwner, unreachable);
					if (given > 0)
					{
						CountPlayerBotLureEngaged(ch, state.lLureAnchorX,
								state.lLureAnchorY, &onParty, &onOwner);
						state.iLureChasing = onOwner;
					}
					else if (unreachable > 0)
						SendPlayerBotWhisper(ch, commander,
								"Nie moge ich na ciebie zrzucic - wyjdz ze strefy bezpieczenstwa");
				}
				if (dwNow - state.dwLureStageTime < (forPlayer
						? PLAYERBOT_LURE_PLAYER_HANDOFF_WAIT : PLAYERBOT_LURE_HANDOFF_WAIT))
					return true;

				// What the receivers actually took over. Outside an order the
				// engine's aggro is never written to - a partial handover is a
				// real outcome and is reported as one.
				const bool taken = onParty > 0;
				sys_log(0, "PLAYERBOT_LURE: handover pid=%u name=%s session=%u delivered=%d taken_by_party=%d still_on_me=%d",
						ch->GetPlayerID(), ch->GetName(), state.dwLureSessionId,
						state.iLureDelivered, onParty, onOwner);
				if (taken && state.bLureGroupsTagged > 0)
				{
					if (state.bLureGoodCourses < 200)
						++state.bLureGoodCourses;
				}
				else
				{
					// A course nobody picked up is not repeated at the same
					// size: the plan drops back to the opening one.
					state.bLureGoodCourses = 0;
				}
				SetPlayerBotLureStage(ch, state, LURE_STAGE_RECOVER, dwNow);
				return true;
			}

			case LURE_STAGE_RECOVER:
			{
				int onParty = 0, onOwner = 0;
				CountPlayerBotLureEngaged(ch, state.lLureAnchorX, state.lLureAnchorY,
						&onParty, &onOwner);
				state.iLureChasing = onOwner;
				// The next course waits for this one to be over. Not on a
				// clock: on the monsters actually still standing.
				if (onParty <= PLAYERBOT_LURE_BUSY_MONSTERS && onOwner == 0)
				{
					FinishPlayerBotLure(ch, state, dwNow, "done");
					return false;
				}
				if (dwNow - state.dwLureStageTime > PLAYERBOT_LURE_RETURN_TIME)
				{
					FinishPlayerBotLure(ch, state, dwNow, "recover_time");
					return false;
				}
				// Anything still on the Archer is its own fight now, and the
				// ordinary combat pass is better at it than this is.
				return onOwner == 0;
			}

			default:
				FinishPlayerBotLure(ch, state, dwNow, "unknown_stage");
				return false;
		}
	}
}

#endif
