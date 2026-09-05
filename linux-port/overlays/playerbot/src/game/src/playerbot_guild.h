#ifndef __INC_METIN2_PLAYERBOT_GUILD_H__
#define __INC_METIN2_PLAYERBOT_GUILD_H__

// Guilds, and who a bot has got on with.
//
// The engine's CGuildManager::CreateGuild imposes no level of its own - the
// forty a player needs lives in the NPC's quest script, not in the code - so
// the rule is ours to state, and it is stated as a player's: level forty and
// the two hundred thousand yang the engine takes as the fee. A bot that would
// be left broke by founding one does not found one.
//
// No guild marks. A mark is a 16x12 image that has to reach every client that
// can see the guild, which is a delivery problem rather than a behaviour one,
// and it is the part of this that can break a client rather than a bot.
//
// Relations are affinity only. The specification also wants hostility, from
// PvP kills and stolen bosses; this world has no PvP at all and no hook that
// can honestly attribute a stolen kill, so a hostility counter here would be a
// field that is always zero. It is left out rather than left lying.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_planner.h and before playerbot_town.h.

namespace
{
	// ---------------------------------------------------------------- friends
	//
	// Who a bot has actually done something with. Kept small and by worth: a
	// bot that has hunted with two hundred others remembers the eight it got on
	// with best, which is what a person would do and what a lookup on every
	// party check can afford.

	int GetPlayerBotAffinity(const TPlayerBotAIState& state, DWORD dwPID)
	{
		for (size_t i = 0; i < state.vecFriends.size(); ++i)
			if (state.vecFriends[i].dwPID == dwPID)
				return state.vecFriends[i].iAffinity;
		return 0;
	}

	void RememberPlayerBotFriend(TPlayerBotAIState& state, DWORD dwPID, int iDelta, DWORD dwNow)
	{
		if (dwPID == 0 || iDelta == 0)
			return;

		for (size_t i = 0; i < state.vecFriends.size(); ++i)
		{
			if (state.vecFriends[i].dwPID != dwPID)
				continue;
			state.vecFriends[i].iAffinity += iDelta;
			if (state.vecFriends[i].iAffinity > PLAYERBOT_FRIEND_MAX_AFFINITY)
				state.vecFriends[i].iAffinity = PLAYERBOT_FRIEND_MAX_AFFINITY;
			state.vecFriends[i].dwLastInteractionTime = dwNow;
			return;
		}

		if (iDelta <= 0)
			return;

		TPlayerBotFriend fresh;
		fresh.dwPID = dwPID;
		fresh.iAffinity = iDelta;
		fresh.dwLastInteractionTime = dwNow;

		if (state.vecFriends.size() < PLAYERBOT_FRIEND_SLOTS)
		{
			state.vecFriends.push_back(fresh);
			return;
		}

		// Full: the weakest tie gives way, and only to something stronger. A new
		// acquaintance does not displace a long-standing one on its first favour.
		size_t weakest = 0;
		for (size_t i = 1; i < state.vecFriends.size(); ++i)
			if (state.vecFriends[i].iAffinity < state.vecFriends[weakest].iAffinity)
				weakest = i;
		if (state.vecFriends[weakest].iAffinity < fresh.iAffinity)
			state.vecFriends[weakest] = fresh;
	}

	// Both sides of an interaction, because getting on with somebody is not a
	// thing one character does to another.
	void RememberPlayerBotEncounter(LPCHARACTER ch, LPCHARACTER other, int iDelta, DWORD dwNow)
	{
		if (!ch || !other || ch == other)
			return;
		const DWORD mine = ch->GetPlayerID();
		const DWORD theirs = other->GetPlayerID();
		if (mine == 0 || theirs == 0)
			return;

		TPlayerBotAIStateMap::iterator itMine = s_mapPlayerBotAIStates.find(mine);
		if (itMine != s_mapPlayerBotAIStates.end())
			RememberPlayerBotFriend(itMine->second, theirs, iDelta, dwNow);

		TPlayerBotAIStateMap::iterator itTheirs = s_mapPlayerBotAIStates.find(theirs);
		if (itTheirs != s_mapPlayerBotAIStates.end())
			RememberPlayerBotFriend(itTheirs->second, mine, iDelta, dwNow);
	}

	// ----------------------------------------------------------------- guilds

	// Two names per empire. Twelve characters is the engine's limit
	// (GUILD_NAME_MAX_LEN); "KrwawiRycerze" was thirteen and would have been
	// refused had a Shinsoo bot ever founded a guild, which none does.
	const char* GetPlayerBotGuildName(BYTE bEmpire, size_t index)
	{
		static const char* kChunjo[] = { "BialyLotos", "CichyOrszak" };
		static const char* kShinsoo[] = { "CzerwSmoki", "KrwawyRycerz" };
		static const char* kJinno[] = { "NiebWilki", "SrebrnaStal" };
		const char** pool = kChunjo;
		if (bEmpire == 1)
			pool = kShinsoo;
		else if (bEmpire == 3)
			pool = kJinno;
		return index < PLAYERBOT_GUILD_NAMES_PER_EMPIRE ? pool[index] : NULL;
	}

	// Founding is rare on purpose. Every eligible bot starting a guild of its
	// own would give a world of guilds of one, which is the opposite of what a
	// guild is for.
	bool ShouldPlayerBotFoundGuild(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->GetGuild() != NULL || state.bFoundedGuild)
			return false;
		if (ch->GetLevel() < PLAYERBOT_GUILD_MIN_LEVEL)
			return false;
		// The engine takes the fee after the guild exists, so a bot that cannot
		// afford it would found one and go broke. Check first.
		if (ch->GetGold() < (int)(PLAYERBOT_GUILD_CREATE_FEE + PLAYERBOT_GUILD_GOLD_RESERVE))
			return false;
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x47554c44U) %
				PLAYERBOT_GUILD_FOUNDER_SHARE) == 0;
	}

	bool FoundPlayerBotGuild(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		CGuildManager& gm = CGuildManager::instance();
		for (size_t i = 0; i < PLAYERBOT_GUILD_NAMES_PER_EMPIRE; ++i)
		{
			const char* szName = GetPlayerBotGuildName(ch->GetEmpire(), i);
			if (!szName || gm.FindGuildByName(szName) != NULL)
				continue;

			TGuildCreateParameter cp;
			memset(&cp, 0, sizeof(cp));
			cp.master = ch;
			strlcpy(cp.name, szName, sizeof(cp.name));

			const DWORD dwGuildID = gm.CreateGuild(cp);
			if (dwGuildID == 0)
				continue;

			// The engine charges this in CInputMain::GuildCreate, not in
			// CreateGuild, so a caller that is not the packet handler has to pay
			// it - otherwise a bot founds a guild for nothing.
			ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_GUILD_CREATE_FEE);
			sys_log(0, "PLAYERBOT_GUILD: founded pid=%u name=%s guild=%s id=%u gold=%d",
					ch->GetPlayerID(), ch->GetName(), szName, dwGuildID,
					(int)(ch->GetGold() / 1000));
			return true;
		}
		return false;
	}

	// A master looks around and asks whoever is standing there. No invitation
	// window exists for a bot to accept, so this is the engine's own
	// RequestAddMember - the same call the accept path makes.
	struct CPlayerBotGuildRecruiter
	{
		CPlayerBotGuildRecruiter(LPCHARACTER me, CGuild* guild, int room)
			: m_me(me), m_guild(guild), m_room(room), m_invited(0) {}

		bool operator()(LPENTITY ent)
		{
			if (m_room <= 0 || !ent || !ent->IsType(ENTITY_CHARACTER))
				return m_room > 0;
			LPCHARACTER candidate = static_cast<LPCHARACTER>(ent);
			if (candidate == m_me || candidate->IsMonster() || candidate->IsStone() ||
					candidate->IsDead() || candidate->GetGuild() != NULL)
				return true;
			if (!candidate->GetDesc() || !candidate->GetDesc()->IsBot())
				return true;
			if (candidate->GetEmpire() != m_me->GetEmpire())
				return true;
			if (DISTANCE_APPROX(m_me->GetX() - candidate->GetX(),
					m_me->GetY() - candidate->GetY()) > PLAYERBOT_GUILD_INVITE_RANGE)
				return true;

			m_guild->RequestAddMember(candidate, PLAYERBOT_GUILD_MEMBER_GRADE);
			sys_log(0, "PLAYERBOT_GUILD: invited pid=%u name=%s by master_pid=%u guild=%s",
					candidate->GetPlayerID(), candidate->GetName(),
					m_me->GetPlayerID(), m_guild->GetName());
			--m_room;
			++m_invited;
			return m_room > 0;
		}

		LPCHARACTER m_me;
		CGuild* m_guild;
		int m_room;
		int m_invited;
	};

	// Upkeep, not an activity: this never claims the tick. Founding is one call
	// and recruiting is one sweep of the sector the bot is already standing in.
	void ManagePlayerBotGuild(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextGuildCheckTime)
			return;
		state.dwNextGuildCheckTime = dwNow + PLAYERBOT_GUILD_CHECK_INTERVAL +
				number(0, 30000);

		CGuild* guild = ch->GetGuild();
		if (!guild)
		{
			if (ShouldPlayerBotFoundGuild(ch, state))
				state.bFoundedGuild = FoundPlayerBotGuild(ch);
			return;
		}

		// Only the master recruits, and only where there is room. GetMaxMemberCount
		// grows with the guild's level, so this is asked every time rather than
		// remembered.
		if (guild->GetMasterPID() != ch->GetPlayerID())
			return;
		const int room = guild->GetMaxMemberCount() - guild->GetMemberCount();
		if (room <= 0 || !ch->GetSectree())
			return;

		CPlayerBotGuildRecruiter recruiter(ch, guild,
				MIN(room, (int)PLAYERBOT_GUILD_INVITES_PER_PASS));
		ch->GetSectree()->ForEachAround(recruiter);
	}
}

#endif
