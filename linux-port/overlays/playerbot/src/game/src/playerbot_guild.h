#ifndef __INC_METIN2_PLAYERBOT_GUILD_H__
#define __INC_METIN2_PLAYERBOT_GUILD_H__

#include "playerbot_guild_names.h"

// Guilds, and who a bot has got on with.
//
// The engine's CGuildManager::CreateGuild imposes no level of its own - the
// forty a player needs lives in the NPC's quest script, not in the code - so
// the rule is ours to state, and it is stated as a player's: level forty and
// the two hundred thousand yang the engine takes as the fee. A bot that would
// be left broke by founding one does not found one.
//
// Since 2.0.60 a guild has a tier. Every bot's strength is one number (level,
// the blow of the weapon in its hand, the build's skill levels, the horse, the
// body armour), the population of a kingdom is cut into percentiles every ten
// minutes, and a guild is founded at the tier its founder's percentile puts it
// in: the top three percent found an elite guild, the top fifteen a strong one,
// the top half a medium one, the rest an ordinary one. A master recruits only
// above its tier's floor, strongest first and wherever they stand, and a member
// whose strength has grown past its guild's tier leaves for a better one.
// Members offer the guild a share of their experience every hour, and the
// master spends the skill points that come with the levels. "Gildie mega
// mocne, silne oraz srednie i slabsze ... rozwijali gildie oddajac swoj exp"
// (Tieru, 16 September). The tier is kept in player.playerbot_guild, which
// apply.sh creates, because a guild outlives every core restart.
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
// it exactly once, after playerbot_planner.h and before playerbot_town.h. The
// wars are playerbot_guild_war.h, after playerbot_targeting.h, because a war
// is fought with the attack code.

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

	// The names come from Iwakura's list (data/guild_names_iwakura.txt,
	// rendered into playerbot_guild_names.h). Until 2.0.13 there were two
	// hand-made names per kingdom, so a world could hold six bot guilds and
	// every later founder found both of its kingdom's names taken. One pool
	// for the whole world now: a founder starts at an offset drawn from its
	// pid, takes the first name no guild wears, and skips what the engine's
	// own limit refuses (GUILD_NAME_MAX_LEN is fourteen on mt2009 and twelve
	// on r40250, and check_name wants letters and digits, which the list is).
	// A name is worn once: FindGuildByName is the engine's own register.
	const char* GetPlayerBotGuildName(DWORD dwPID, size_t step)
	{
		if (step >= PLAYERBOT_GUILD_NAME_POOL_SIZE)
			return NULL;
		const size_t index = (PlayerBotNavHash(dwPID ^ 0x4e414d45U) + step) % PLAYERBOT_GUILD_NAME_POOL_SIZE;
		const char* szName = PLAYERBOT_GUILD_NAME_POOL[index];
		if (strlen(szName) > GUILD_NAME_MAX_LEN)
			return "";
		return szName;
	}

	// --------------------------------------------------------------- strength
	//
	// One number per bot. The units are arbitrary and only the order matters,
	// because a tier is a percentile of the kingdom, not a threshold: level a
	// thousand a level, the weapon's blow (GetPlayerBotWeaponHitDamageAt, the
	// same figure the equipment pass buys by), the build's six skill levels, the
	// horse, and the body armour's score. A bot with no profession yet has no
	// skills and counts by its level and its stick.

	enum EPlayerBotGuildTier
	{
		GUILD_TIER_ELITE = 0,
		GUILD_TIER_STRONG = 1,
		GUILD_TIER_MEDIUM = 2,
		GUILD_TIER_ORDINARY = 3
	};

	const char* GetPlayerBotGuildTierName(int tier)
	{
		switch (tier)
		{
			case GUILD_TIER_ELITE: return "Elita";
			case GUILD_TIER_STRONG: return "Silna";
			case GUILD_TIER_MEDIUM: return "Srednia";
			default: return "Zwykla";
		}
	}

	int GetPlayerBotStrength(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		long long strength = (long long)ch->GetLevel() * 1000;
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		if (hand && hand->GetProto())
			strength += std::min<long long>(GetPlayerBotWeaponHitDamageAt(hand, hand->GetProto(), ch) * 5, 20000);
		if (ch->GetSkillGroup() != 0)
		{
			const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
			for (BYTE i = 0; i < build.bSkillCount && i < 6; ++i)
				if (build.dwSkills[i] != 0)
					strength += (long long)ch->GetSkillLevel(build.dwSkills[i]) * 100;
		}
		strength += (long long)ch->GetHorseLevel() * 300;
		LPITEM body = ch->GetWear(WEAR_BODY);
		if (body)
			strength += std::min<long long>(GetPlayerBotEquipmentScore(body, ch) / 100, 10000);
		if (strength < 0)
			strength = 0;
		return (int)std::min<long long>(strength, INT_MAX / 2);
	}

	// The population's strengths and the kingdom's tier floors, refreshed
	// every PLAYERBOT_GUILD_STRENGTH_INTERVAL from the tick. A floor is the
	// strength of the last bot inside the tier's share (the top three percent
	// for the elite), so a bot qualifies at or above it; the ordinary tier's
	// floor is zero. Until the first census a kingdom's floors are zero and
	// nothing is founded or adopted, which keeps the first minutes after a
	// start - a few bots in the world - from setting the bands of the day.
	std::map<DWORD, int> s_mapPlayerBotStrength;
	int s_aiPlayerBotTierFloor[playerbot_empire_rules::EMPIRE_COUNT][PLAYERBOT_GUILD_TIER_COUNT];
	int s_aiPlayerBotStrengthCensus[playerbot_empire_rules::EMPIRE_COUNT];
	int s_aiPlayerBotGuildPromotions[playerbot_empire_rules::EMPIRE_COUNT];
	DWORD s_dwNextPlayerBotStrengthRefresh = 0;
	unsigned long long s_ullPlayerBotGuildExpOffered = 0;
	unsigned int s_uPlayerBotGuildExpOffers = 0;

	int GetPlayerBotStrengthCached(DWORD dwPID)
	{
		std::map<DWORD, int>::const_iterator it = s_mapPlayerBotStrength.find(dwPID);
		return it != s_mapPlayerBotStrength.end() ? it->second : 0;
	}

	bool PlayerBotKingdomHasStrengthCensus(BYTE empire)
	{
		return empire < playerbot_empire_rules::EMPIRE_COUNT &&
				s_aiPlayerBotTierFloor[empire][GUILD_TIER_MEDIUM] > 0;
	}

	void RefreshPlayerBotStrengths(DWORD dwNow)
	{
		// The census, the tier floors and the tiers written for the guilds are
		// the first channel's: two channels each counting half a kingdom would
		// write two sets of tiers into one table.
		if (g_bChannel != 1)
			return;
		if (s_dwNextPlayerBotStrengthRefresh == 0)
		{
			// The first census waits one interval: the minute after a start holds
			// the first bots of the spawn window, not the kingdom.
			s_dwNextPlayerBotStrengthRefresh = dwNow + PLAYERBOT_GUILD_STRENGTH_INTERVAL;
			return;
		}
		if (dwNow < s_dwNextPlayerBotStrengthRefresh)
			return;
		s_dwNextPlayerBotStrengthRefresh = dwNow + PLAYERBOT_GUILD_STRENGTH_INTERVAL;

		std::vector<int> byEmpire[playerbot_empire_rules::EMPIRE_COUNT];
		std::map<DWORD, int> fresh;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->GetDesc() || !ch->GetDesc()->IsBot())
				continue;
			const int strength = GetPlayerBotStrength(ch);
			fresh[it->first] = strength;
			const BYTE empire = ch->GetEmpire();
			if (empire < playerbot_empire_rules::EMPIRE_COUNT)
				byEmpire[empire].push_back(strength);
		}
		s_mapPlayerBotStrength.swap(fresh);

		for (int e = 0; e < playerbot_empire_rules::EMPIRE_COUNT; ++e)
		{
			std::vector<int>& v = byEmpire[e];
			s_aiPlayerBotStrengthCensus[e] = (int)v.size();
			s_aiPlayerBotGuildPromotions[e] = 0;
			if (v.empty())
			{
				for (int t = 0; t < PLAYERBOT_GUILD_TIER_COUNT; ++t)
					s_aiPlayerBotTierFloor[e][t] = 0;
				continue;
			}
			std::sort(v.begin(), v.end(), std::greater<int>());
			for (int t = 0; t < PLAYERBOT_GUILD_TIER_COUNT; ++t)
			{
				if (t == GUILD_TIER_ORDINARY)
				{
					s_aiPlayerBotTierFloor[e][t] = 0;
					continue;
				}
				size_t inside = (v.size() * (size_t)PLAYERBOT_GUILD_TIER_PERCENT[t] + 99) / 100;
				if (inside == 0)
					inside = 1;
				if (inside > v.size())
					inside = v.size();
				s_aiPlayerBotTierFloor[e][t] = std::max(1, v[inside - 1]);
			}
			sys_log(0, "PLAYERBOT_GUILD: strength census empire=%d bots=%u top=%d elite_floor=%d strong_floor=%d medium_floor=%d",
					e, (unsigned int)v.size(), v[0], s_aiPlayerBotTierFloor[e][GUILD_TIER_ELITE],
					s_aiPlayerBotTierFloor[e][GUILD_TIER_STRONG], s_aiPlayerBotTierFloor[e][GUILD_TIER_MEDIUM]);
		}
	}

	int GetPlayerBotStrengthTier(BYTE empire, int strength)
	{
		if (empire >= playerbot_empire_rules::EMPIRE_COUNT || strength <= 0)
			return GUILD_TIER_ORDINARY;
		for (int t = 0; t < GUILD_TIER_ORDINARY; ++t)
			if (s_aiPlayerBotTierFloor[empire][t] > 0 && strength >= s_aiPlayerBotTierFloor[empire][t])
				return t;
		return GUILD_TIER_ORDINARY;
	}

	// ---------------------------------------------------------- guild records
	//
	// What a guild is, beyond what the engine keeps: its tier, its kingdom and
	// who founded it. In player.playerbot_guild (apply.sh creates it), loaded
	// once on the first question, written when a guild is founded or adopted.
	// A guild the table does not know whose master is a registered bot is
	// adopted at the tier its master's strength gives it today - that is every
	// bot guild founded before 2.0.60 - and a player's guild is never touched.

	struct TPlayerBotGuildInfo
	{
		BYTE bTier;
		BYTE bEmpire;
		DWORD dwFounderPID;
	};
	std::map<DWORD, TPlayerBotGuildInfo> s_mapPlayerBotGuildInfo;
	bool s_bPlayerBotGuildInfoLoaded = false;

	void LoadPlayerBotGuildInfo()
	{
		s_bPlayerBotGuildInfoLoaded = true;
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT guild_id, tier, empire, founder_pid FROM player.playerbot_guild"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_log(0, "PLAYERBOT_GUILD: player.playerbot_guild not readable yet (the migrator creates it); tiers start empty");
			return;
		}
		MYSQL_ROW row;
		unsigned int loaded = 0;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD gid = 0, tier = 0, empire = 0, founder = 0;
			if (row[0]) str_to_number(gid, row[0]);
			if (row[1]) str_to_number(tier, row[1]);
			if (row[2]) str_to_number(empire, row[2]);
			if (row[3]) str_to_number(founder, row[3]);
			if (gid == 0)
				continue;
			TPlayerBotGuildInfo info;
			info.bTier = (BYTE)std::min<DWORD>(tier, GUILD_TIER_ORDINARY);
			info.bEmpire = (BYTE)std::min<DWORD>(empire, playerbot_empire_rules::EMPIRE_COUNT - 1);
			info.dwFounderPID = founder;
			s_mapPlayerBotGuildInfo[gid] = info;
			++loaded;
		}
		sys_log(0, "PLAYERBOT_GUILD: loaded %u guild records", loaded);
	}

	void SavePlayerBotGuildInfo(DWORD dwGuildID, const TPlayerBotGuildInfo& info)
	{
		DBManager::instance().Query(
				"INSERT INTO player.playerbot_guild (guild_id, tier, empire, founder_pid, founded_at) "
				"VALUES (%u, %u, %u, %u, NOW()) "
				"ON DUPLICATE KEY UPDATE tier=VALUES(tier), empire=VALUES(empire), founder_pid=VALUES(founder_pid)",
				dwGuildID, (unsigned int)info.bTier, (unsigned int)info.bEmpire, info.dwFounderPID);
	}

	int CountPlayerBotGuildsOfTier(BYTE empire, int tier)
	{
		int count = 0;
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			if (it->second.bEmpire != empire || it->second.bTier != tier)
				continue;
			if (CGuildManager::instance().FindGuild(it->first) != NULL)
				++count;
		}
		return count;
	}

	// The tier a founder of this strength gets today: its own, or the next one
	// down while its own is full for the kingdom.
	int GetPlayerBotFoundingTier(BYTE empire, int strength)
	{
		int tier = GetPlayerBotStrengthTier(empire, strength);
		while (tier < GUILD_TIER_ORDINARY &&
				PLAYERBOT_GUILD_TIER_MAX_PER_KINGDOM[tier] > 0 &&
				CountPlayerBotGuildsOfTier(empire, tier) >= PLAYERBOT_GUILD_TIER_MAX_PER_KINGDOM[tier])
			++tier;
		return tier;
	}

	const TPlayerBotGuildInfo* GetPlayerBotGuildInfo(CGuild* guild)
	{
		if (!guild)
			return NULL;
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();
		std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.find(guild->GetID());
		if (it != s_mapPlayerBotGuildInfo.end())
			return &it->second;

		// Unknown: a bot's guild from before the tiers, or a player's.
		const DWORD master = guild->GetMasterPID();
		if (!CPlayerBotManager::instance().IsRegisteredBotPID(master))
			return NULL;
		const BYTE empire = CPlayerBotManager::instance().GetRegisteredEmpire(master);
		if (!PlayerBotKingdomHasStrengthCensus(empire))
			return NULL;
		// The tier is the master's percentile, so the master has to have been
		// counted: a guild whose master is not in this core's world waits.
		const int masterStrength = GetPlayerBotStrengthCached(master);
		if (masterStrength <= 0)
			return NULL;
		TPlayerBotGuildInfo info;
		info.bEmpire = empire;
		info.bTier = (BYTE)GetPlayerBotFoundingTier(empire, masterStrength);
		info.dwFounderPID = master;
		s_mapPlayerBotGuildInfo[guild->GetID()] = info;
		SavePlayerBotGuildInfo(guild->GetID(), info);
		sys_log(0, "PLAYERBOT_GUILD: adopted guild=%s id=%u empire=%u tier=%s master_pid=%u master_strength=%d",
				guild->GetName(), guild->GetID(), (unsigned int)empire, GetPlayerBotGuildTierName(info.bTier),
				master, GetPlayerBotStrengthCached(master));
		return &s_mapPlayerBotGuildInfo[guild->GetID()];
	}

	// The engine's ceiling grows with the level; an elite or strong guild
	// keeps a smaller table on purpose.
	int GetPlayerBotGuildMemberCap(CGuild* guild, int tier)
	{
		const int engineCap = guild ? guild->GetMaxMemberCount() : 0;
		if (tier < 0 || tier >= PLAYERBOT_GUILD_TIER_COUNT || PLAYERBOT_GUILD_TIER_MEMBER_CAP[tier] <= 0)
			return engineCap;
		return std::min(engineCap, PLAYERBOT_GUILD_TIER_MEMBER_CAP[tier]);
	}

	// The guild's bots in this core's world, and their strength between them.
	int CountPlayerBotGuildOnline(CGuild* guild, long long* pStrengthSum = NULL)
	{
		int online = 0;
		long long sum = 0;
		if (!guild)
			return 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || ch->GetGuild() != guild)
				continue;
			++online;
			sum += GetPlayerBotStrengthCached(it->first);
		}
		if (pStrengthSum)
			*pStrengthSum = sum;
		return online;
	}

	// --------------------------------------------------------------- founding

	// Founding is rare on purpose. Every eligible bot starting a guild of its
	// own would give a world of guilds of one, which is the opposite of what a
	// guild is for. The tier is the founder's strength percentile, capped per
	// kingdom for the elite and the strong (GetPlayerBotFoundingTier).
	bool ShouldPlayerBotFoundGuild(LPCHARACTER ch, const TPlayerBotAIState& state, int& tierOut)
	{
		if (!ch || ch->GetGuild() != NULL || state.bFoundedGuild)
			return false;
		// Nor a dropper: see LeavePlayerBotGuildAsDropper.
		if (IsPlayerBotDropper(state.bPersonality))
			return false;
		if (ch->GetLevel() < PLAYERBOT_GUILD_MIN_LEVEL)
			return false;
		// The engine takes the fee after the guild exists, so a bot that cannot
		// afford it would found one and go broke. Check first.
		if (ch->GetGold() < (int)(PLAYERBOT_GUILD_CREATE_FEE + PLAYERBOT_GUILD_GOLD_RESERVE))
			return false;
		const BYTE empire = ch->GetEmpire();
		if (!PlayerBotKingdomHasStrengthCensus(empire))
			return false;
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x47554c44U) % PLAYERBOT_GUILD_FOUNDER_SHARE) != 0)
			return false;
		tierOut = GetPlayerBotFoundingTier(empire, GetPlayerBotStrengthCached(ch->GetPlayerID()));
		return true;
	}

	bool FoundPlayerBotGuild(LPCHARACTER ch, int tier)
	{
		if (!ch)
			return false;

		CGuildManager& gm = CGuildManager::instance();
		for (size_t i = 0; i < PLAYERBOT_GUILD_NAME_POOL_SIZE; ++i)
		{
			const char* szName = GetPlayerBotGuildName(ch->GetPlayerID(), i);
			if (!szName)
				break;
			if (!*szName || gm.FindGuildByName(szName) != NULL)
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
			PlayerBotChangeGold(ch, -(int)PLAYERBOT_GUILD_CREATE_FEE);
			TPlayerBotGuildInfo info;
			info.bTier = (BYTE)tier;
			info.bEmpire = ch->GetEmpire();
			info.dwFounderPID = ch->GetPlayerID();
			s_mapPlayerBotGuildInfo[dwGuildID] = info;
			SavePlayerBotGuildInfo(dwGuildID, info);
			sys_log(0, "PLAYERBOT_GUILD: founded pid=%u name=%s guild=%s id=%u tier=%s strength=%d gold=%d",
					ch->GetPlayerID(), ch->GetName(), szName, dwGuildID, GetPlayerBotGuildTierName(tier),
					GetPlayerBotStrengthCached(ch->GetPlayerID()), (int)(ch->GetGold() / 1000));
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------- recruiting
	//
	// The master asks the strongest guildless bots of its kingdom above the
	// tier's floor, wherever they stand - a guild of the strong is not a guild
	// of whoever happened to be at the pitch. No invitation window exists for
	// a bot to accept, so this is the engine's own RequestAddMember, the same
	// call the accept path makes. A few a pass, so a new guild fills over the
	// hour rather than in one tick.

	struct TPlayerBotGuildCandidate
	{
		LPCHARACTER ch;
		int iStrength;
	};

	bool PlayerBotGuildCandidateStronger(const TPlayerBotGuildCandidate& a, const TPlayerBotGuildCandidate& b)
	{
		return a.iStrength > b.iStrength;
	}

	void RecruitPlayerBotGuildMembers(LPCHARACTER master, CGuild* guild, const TPlayerBotGuildInfo& info, DWORD dwNow)
	{
		if (!master || !guild)
			return;
		const int room = GetPlayerBotGuildMemberCap(guild, info.bTier) - guild->GetMemberCount();
		if (room <= 0)
			return;
		const int floor = info.bTier < GUILD_TIER_ORDINARY ? s_aiPlayerBotTierFloor[info.bEmpire][info.bTier] : 0;

		std::vector<TPlayerBotGuildCandidate> candidates;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER candidate = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!candidate || candidate == master || candidate->IsDead() || candidate->GetGuild() != NULL ||
					!candidate->GetDesc() || !candidate->GetDesc()->IsBot())
				continue;
			if (candidate->GetEmpire() != info.bEmpire)
				continue;
			if (IsPlayerBotDropper(it->second.bPersonality) || IsPlayerBotSidekickPID(it->first))
				continue;
			const int strength = GetPlayerBotStrengthCached(it->first);
			if (floor > 0 && strength < floor)
				continue;
			TPlayerBotGuildCandidate c;
			c.ch = candidate;
			c.iStrength = strength;
			candidates.push_back(c);
		}
		if (candidates.empty())
			return;
		std::sort(candidates.begin(), candidates.end(), PlayerBotGuildCandidateStronger);

		const size_t invites = std::min<size_t>(candidates.size(),
				std::min<size_t>((size_t)room, PLAYERBOT_GUILD_INVITES_PER_PASS));
		for (size_t i = 0; i < invites; ++i)
		{
			guild->RequestAddMember(candidates[i].ch, PLAYERBOT_GUILD_MEMBER_GRADE);
			sys_log(0, "PLAYERBOT_GUILD: invited pid=%u name=%s strength=%d by master_pid=%u guild=%s tier=%s members=%d/%d",
					candidates[i].ch->GetPlayerID(), candidates[i].ch->GetName(), candidates[i].iStrength,
					master->GetPlayerID(), guild->GetName(), GetPlayerBotGuildTierName(info.bTier),
					guild->GetMemberCount(), GetPlayerBotGuildMemberCap(guild, info.bTier));
		}
	}

	// A member that has outgrown its guild leaves for a better one that has
	// room, once in PLAYERBOT_GUILD_PROMOTION_HOLD_MS and a few a kingdom per
	// census, so guilds change hands slowly. It leaves; the better guild's
	// master asks the strongest guildless bots on its next check, and that is
	// this one. Masters never leave: a guild without its founder is the
	// engine's problem, not a promotion.
	bool TryPlayerBotGuildPromotion(LPCHARACTER ch, TPlayerBotAIState& state, CGuild* guild,
			const TPlayerBotGuildInfo& info, DWORD dwNow)
	{
		const BYTE empire = info.bEmpire;
		if (empire >= playerbot_empire_rules::EMPIRE_COUNT)
			return false;
		const int strength = GetPlayerBotStrengthCached(ch->GetPlayerID());
		const int myTier = GetPlayerBotStrengthTier(empire, strength);
		if (myTier >= info.bTier)
			return false;
		if (state.dwLastGuildPromotionTime != 0 &&
				dwNow - state.dwLastGuildPromotionTime < PLAYERBOT_GUILD_PROMOTION_HOLD_MS)
			return false;
		if (s_aiPlayerBotGuildPromotions[empire] >= PLAYERBOT_GUILD_PROMOTIONS_PER_PASS)
			return false;

		CGuild* better = NULL;
		int betterTier = GUILD_TIER_ORDINARY;
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			if (it->second.bEmpire != empire || it->second.bTier >= info.bTier || it->second.bTier < myTier)
				continue;
			CGuild* g = CGuildManager::instance().FindGuild(it->first);
			if (!g || g == guild)
				continue;
			if (g->GetMemberCount() >= GetPlayerBotGuildMemberCap(g, it->second.bTier))
				continue;
			if (!better || it->second.bTier < betterTier)
			{
				better = g;
				betterTier = it->second.bTier;
			}
		}
		if (!better)
			return false;
		if (!guild->RequestRemoveMember(ch->GetPlayerID()))
			return false;
		state.dwLastGuildPromotionTime = dwNow;
		++s_aiPlayerBotGuildPromotions[empire];
		sys_log(0, "PLAYERBOT_GUILD: left for a stronger guild pid=%u name=%s strength=%d from=%s (%s) towards=%s (%s)",
				ch->GetPlayerID(), ch->GetName(), strength, guild->GetName(),
				GetPlayerBotGuildTierName(info.bTier), better->GetName(), GetPlayerBotGuildTierName(betterTier));
		return true;
	}

	// ------------------------------------------------------------- experience
	//
	// Once an hour a member offers the guild a share of the experience it has
	// gained since its last offer - never more than the level holds, so a bot
	// never loses a level to its guild, and CGuild::OfferExp refuses a bot under
	// the exp block anyway. The guild gets a hundredth of what is offered
	// (guild_exp_table: 15 000 for the second level, 685 000 for the tenth),
	// which is what makes a guild of forty bots of forty a level-ten guild in
	// about a day. "Boty nie oddaja expa do gildii" (Kiciamol, 16 September):
	// twenty-five guilds of level one on the test world, none with a point.
	void ManagePlayerBotGuildExp(LPCHARACTER ch, TPlayerBotAIState& state, CGuild* guild,
			const TPlayerBotGuildInfo& info, DWORD dwNow)
	{
		if (state.dwNextGuildExpOfferTime == 0)
		{
			state.dwNextGuildExpOfferTime = dwNow + PLAYERBOT_GUILD_EXP_OFFER_INTERVAL +
					PlayerBotNavHash(ch->GetPlayerID() ^ 0x47455850U) % (10U * 60U * 1000U);
			state.dwGuildExpAtLastOffer = ch->GetExp();
			state.bGuildLevelAtLastOffer = ch->GetLevel();
			return;
		}
		if (dwNow < state.dwNextGuildExpOfferTime)
			return;
		state.dwNextGuildExpOfferTime = dwNow + PLAYERBOT_GUILD_EXP_OFFER_INTERVAL;

		const DWORD exp = ch->GetExp();
		const DWORD gained = ch->GetLevel() != state.bGuildLevelAtLastOffer ? exp :
				(exp >= state.dwGuildExpAtLastOffer ? exp - state.dwGuildExpAtLastOffer : 0);
		state.dwGuildExpAtLastOffer = exp;
		state.bGuildLevelAtLastOffer = ch->GetLevel();

		const int percent = info.bTier < PLAYERBOT_GUILD_TIER_COUNT ? PLAYERBOT_GUILD_EXP_OFFER_PERCENT[info.bTier] : 10;
		unsigned long long amount = (unsigned long long)gained * (unsigned long long)percent / 100ULL;
		if (amount > (unsigned long long)(INT_MAX / 4))
			amount = INT_MAX / 4;
		if (amount < PLAYERBOT_GUILD_EXP_OFFER_MIN || amount > exp)
			return;
		// OfferExp itself refuses a bot under the experience block (mt2009).
		const BYTE levelBefore = guild->GetLevel();
		if (!guild->OfferExp(ch, (int)amount))
			return;
		state.dwGuildExpAtLastOffer = ch->GetExp();
		s_ullPlayerBotGuildExpOffered += amount / 100ULL;
		++s_uPlayerBotGuildExpOffers;
		PlayerBotLogThrottled("guild_exp", dwNow,
				"PLAYERBOT_GUILD: offered exp=%u (%d%% of %u gained) pid=%u name=%s guild=%s guild_level=%u",
				(unsigned int)amount, percent, gained, ch->GetPlayerID(), ch->GetName(),
				guild->GetName(), (unsigned int)guild->GetLevel());
		if (guild->GetLevel() != levelBefore)
			sys_log(0, "PLAYERBOT_GUILD: guild %s reached level %u (%s, %d members)",
					guild->GetName(), (unsigned int)guild->GetLevel(),
					GetPlayerBotGuildTierName(info.bTier), guild->GetMemberCount());
	}

	// ----------------------------------------------------------- guild skills
	//
	// A level brings a point, and the master spends it: the six skills climb
	// together, Blood of the Dragon God first at every step, then Holy Armour,
	// Wrath, Acceleration, Benediction and Casting Aid. On this line a guild
	// skill can only be used inside a war arena (CGuild::UseSkill asks
	// CWarMapManager::IsWarMap), which the bots' field wars are not, so the
	// points are spent for the guild's own sake and for the day the arena
	// wars come; a player in a bot guild has them ready.
	const DWORD PLAYERBOT_GUILD_SKILL_ORDER[] = { 152, 154, 156, 155, 153, 157 };

	void SpendPlayerBotGuildSkillPoints(LPCHARACTER master, CGuild* guild)
	{
		if (!master || !guild)
			return;
		const size_t count = sizeof(PLAYERBOT_GUILD_SKILL_ORDER) / sizeof(PLAYERBOT_GUILD_SKILL_ORDER[0]);
		for (size_t i = 0; i < count; ++i)
		{
			const DWORD vnum = PLAYERBOT_GUILD_SKILL_ORDER[i];
			const int level = guild->GetSkillLevel(vnum);
			if (level >= 7)
				continue;
			if (i > 0 && guild->GetSkillLevel(PLAYERBOT_GUILD_SKILL_ORDER[i - 1]) <= level)
				continue;
			guild->SkillLevelUp(vnum);
			if (guild->GetSkillLevel(vnum) > level)
				sys_log(0, "PLAYERBOT_GUILD: skill raised guild=%s skill=%u level=%d by master_pid=%u",
						guild->GetName(), vnum, guild->GetSkillLevel(vnum), master->GetPlayerID());
			return;
		}
	}

	// ----------------------------------------------------- the dropper's exit

	// A dropper is a drop character, not a guildmate: "takie postacie niech nie
	// dochodza do gildii, to tylko dropki" (Tieru, 15 September), and 238 of the
	// test world's 250 were in one. It is never asked and never founds one, and
	// one already inside leaves on its next guild check. A master cannot leave
	// its own guild, so a dropper master hands it to the strongest bot of that
	// guild standing near it, or disbands a guild of one.
	struct FPlayerBotGuildHeir
	{
		FPlayerBotGuildHeir(LPCHARACTER me, CGuild* guild) : m_me(me), m_guild(guild), m_best(NULL) {}

		bool operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return true;
			LPCHARACTER candidate = static_cast<LPCHARACTER>(ent);
			if (candidate == m_me || !candidate->IsPC() || candidate->IsDead() ||
					candidate->GetGuild() != m_guild || !candidate->GetDesc() ||
					!candidate->GetDesc()->IsBot() ||
					IsPlayerBotDropper(GetPlayerBotPersonalityByPID(candidate->GetPlayerID())))
				return true;
			if (!m_best || candidate->GetLevel() > m_best->GetLevel())
				m_best = candidate;
			return true;
		}

		LPCHARACTER m_me;
		CGuild* m_guild;
		LPCHARACTER m_best;
	};

	void LeavePlayerBotGuildAsDropper(LPCHARACTER ch, CGuild* guild)
	{
		const DWORD pid = ch->GetPlayerID();
		if (guild->GetMasterPID() != pid)
		{
			if (guild->RequestRemoveMember(pid))
				sys_log(0, "PLAYERBOT_GUILD: dropper left pid=%u name=%s guild=%s",
						pid, ch->GetName(), guild->GetName());
			return;
		}
		if (guild->GetMemberCount() <= 1)
		{
			sys_log(0, "PLAYERBOT_GUILD: dropper disbands its guild of one pid=%u name=%s guild=%s",
					pid, ch->GetName(), guild->GetName());
			guild->RequestDisband(pid);
			return;
		}
		FPlayerBotGuildHeir heir(ch, guild);
		if (ch->GetSectree())
			ch->GetSectree()->ForEachAround(heir);
		if (heir.m_best && guild->ChangeMasterTo(heir.m_best->GetPlayerID()))
			sys_log(0, "PLAYERBOT_GUILD: dropper handed its guild over pid=%u name=%s guild=%s heir_pid=%u heir=%s",
					pid, ch->GetName(), guild->GetName(), heir.m_best->GetPlayerID(),
					heir.m_best->GetName());
	}

	// A player's invitation. CGuild::Invite sends the invitee a packet a bot
	// has no client to answer, so the invitation event expired after ten
	// seconds and inviting a bot did nothing at all ("niech boty akceptuja
	// zaproszenia jesli nie sa w zadnej gildii a my je zapraszamy", Tieru,
	// 16 September). The engine hands the invitation to the manager on the
	// same call (playerbotify.py, apply_playerbot_guild_invites) and this
	// answers it at once, while the event is alive: yes for any bot with no
	// guild, dropper or not. The engine's own conditions - one kingdom, no
	// guild yet, room, no war, no withdrawal penalty - were checked by
	// Invite before the packet and are checked again by InviteAccept.
	bool AcceptPlayerBotGuildInvite(LPCHARACTER bot, CGuild* guild, LPCHARACTER inviter)
	{
		if (!bot || !guild || bot->GetGuild())
			return false;
		guild->InviteAccept(bot);
		sys_log(0, "PLAYERBOT_GUILD: accepted a player's invitation pid=%u name=%s guild=%s inviter=%s",
				bot->GetPlayerID(), bot->GetName(), guild->GetName(), inviter ? inviter->GetName() : "?");
		if (inviter)
			inviter->ChatPacket(CHAT_TYPE_INFO, "%s przyjmuje zaproszenie do gildii %s.", bot->GetName(), guild->GetName());
		return true;
	}

	// ------------------------------------------------------------- the report
	//
	// playerbot_guild_status.tsv beside playerbot_status.tsv, once a minute:
	// every bot guild this core knows, with its tier, level, members, the bots
	// of it in this core's world and their strength, the war it is in. The
	// classic panel's guild page reads the three cores' files.
	DWORD s_dwNextPlayerBotGuildStatusTime = 0;
	// Defined in playerbot_guild_war.h, which comes after targeting.h.
	int GetPlayerBotNextGuildWarInSeconds(BYTE empire, DWORD dwNow);
	// Defined in playerbot_demon_tower.h, after that.
	bool IsPlayerBotGuildRaidingTower(DWORD dwGuildID);

	void WritePlayerBotGuildStatus(DWORD dwNow)
	{
		// The report the panel reads is the first channel's, where the wars
		// and the tiers are kept.
		if (g_bChannel != 1)
			return;
		if (s_dwNextPlayerBotGuildStatusTime != 0 && dwNow < s_dwNextPlayerBotGuildStatusTime)
			return;
		s_dwNextPlayerBotGuildStatusTime = dwNow + PLAYERBOT_GUILD_STATUS_INTERVAL;
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();

		const char* tempPath = "playerbot_guild_status.tsv.tmp";
		const char* finalPath = "playerbot_guild_status.tsv";
		FILE* f = fopen(tempPath, "wb");
		if (!f)
			return;
		fprintf(f, "guild_id\tname\tempire\ttier\tlevel\tmembers\tonline\tmaster_pid\tmaster\tladder\twins\tdraws\tlosses\tavg_strength\twar_with\twar_score\twar_enemy_score\texp_offered_here\tnext_war_in_s\ttower_raid\n");
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			CGuild* g = CGuildManager::instance().FindGuild(it->first);
			if (!g)
				continue;
			long long strengthSum = 0;
			const int online = CountPlayerBotGuildOnline(g, &strengthSum);
			LPCHARACTER master = g->GetMasterCharacter();
			const DWORD opp = g->UnderAnyWar(GUILD_WAR_TYPE_FIELD);
			CGuild* enemy = opp ? CGuildManager::instance().FindGuild(opp) : NULL;
			fprintf(f, "%u\t%s\t%u\t%u\t%u\t%d\t%d\t%u\t%s\t%d\t%d\t%d\t%d\t%d\t%s\t%d\t%d\t%llu\t%d\t%d\n",
					it->first, g->GetName(), (unsigned int)it->second.bEmpire, (unsigned int)it->second.bTier,
					(unsigned int)g->GetLevel(), g->GetMemberCount(), online, g->GetMasterPID(),
					master ? master->GetName() : "", g->GetLadderPoint(), g->GetGuildWarWinCount(),
					g->GetGuildWarDrawCount(), g->GetGuildWarLossCount(),
					online > 0 ? (int)(strengthSum / online) : 0,
					enemy ? enemy->GetName() : "", opp ? g->GetWarScoreAgainstTo(opp) : 0,
					enemy ? enemy->GetWarScoreAgainstTo(g->GetID()) : 0,
					s_ullPlayerBotGuildExpOffered,
					GetPlayerBotNextGuildWarInSeconds(it->second.bEmpire, dwNow),
					IsPlayerBotGuildRaidingTower(it->first) ? 1 : 0);
		}
		fclose(f);
		rename(tempPath, finalPath);
	}

	// ----------------------------------------------------------------- upkeep

	// Upkeep, not an activity: this never claims the tick. Founding is one call,
	// the hour's offer one call, and recruiting one pass over the roster.
	void ManagePlayerBotGuild(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextGuildCheckTime)
			return;
		state.dwNextGuildCheckTime = dwNow + PLAYERBOT_GUILD_CHECK_INTERVAL +
				number(0, 30000);

		CGuild* guild = ch->GetGuild();
		// A player's companion (playerbot_sidekick.h) is in its owner's guild
		// or none: it founds no bot guild and leaves one it was in before.
		const bool sidekick = IsPlayerBotSidekickPID(ch->GetPlayerID());
		if (!guild)
		{
			if (sidekick)
				return;
			// Guilds are founded on the first channel, where the census is.
			int tier = GUILD_TIER_ORDINARY;
			if (g_bChannel == 1 && ShouldPlayerBotFoundGuild(ch, state, tier))
				state.bFoundedGuild = FoundPlayerBotGuild(ch, tier);
			return;
		}
		const TPlayerBotGuildInfo* info = GetPlayerBotGuildInfo(guild);
		// A player's guild (no bot master, so never adopted): the bot was
		// invited into it and stays - a dropper too, because a player chose
		// it - and offers its hour's share of experience at the ordinary
		// guild's rate, which is what a member of a player's guild is for.
		// Nothing else here is a bot's to decide about somebody else's guild.
		if (!info)
		{
			if (!CPlayerBotManager::instance().IsRegisteredBotPID(guild->GetMasterPID()))
			{
				static TPlayerBotGuildInfo s_playersGuild = { (BYTE)GUILD_TIER_ORDINARY, 0, 0 };
				ManagePlayerBotGuildExp(ch, state, guild, s_playersGuild, dwNow);
			}
			return;
		}
		if (IsPlayerBotDropper(state.bPersonality) || sidekick)
		{
			LeavePlayerBotGuildAsDropper(ch, guild);
			return;
		}

		ManagePlayerBotGuildExp(ch, state, guild, *info, dwNow);
		// On the second channel a member offers its experience and nothing
		// more: promotion, recruiting and the master's points follow the
		// census, which is the first channel's.
		if (g_bChannel != 1)
			return;

		if (guild->GetMasterPID() != ch->GetPlayerID())
		{
			TryPlayerBotGuildPromotion(ch, state, guild, *info, dwNow);
			return;
		}
		SpendPlayerBotGuildSkillPoints(ch, guild);
		RecruitPlayerBotGuildMembers(ch, guild, *info, dwNow);
	}
}

#endif
