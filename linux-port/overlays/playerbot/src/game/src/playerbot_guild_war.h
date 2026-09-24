#ifndef __INC_METIN2_PLAYERBOT_GUILD_WAR_H__
#define __INC_METIN2_PLAYERBOT_GUILD_WAR_H__

// Guild wars between the bots' guilds.
//
// Two bot guilds of one kingdom, of the same tier where there is a pair, fight
// a field war - the engine's own GUILD_WAR_TYPE_FIELD: declared by one master
// and accepted by the other through CGuild::RequestDeclareWar, thirty minutes
// on the db core's clock, every kill between the two counted by
// CGuildManager::Kill, the winner and the ladder points settled by the db
// core, the notices its own. A field war is fought anywhere, so the
// battlefield is ours to choose: the kingdom's guild map (Waryong and its two
// mirrors), which every core hosts for its own kingdom, is nearly empty, and
// needs none of the arena maps 110/111 that only the first core carries. Both
// guilds rally on the open, fightable ground nearest the map's Town.txt point
// (two of the three points sit inside a safe zone), a side apart, and every
// bot of either goes for the nearest enemy it can see; the dead stand up in
// their village and come back. "Potem stworzymy wojny gildii, gdzie beda chodzic na
// specjalna mape i walczyc jak gracze miedzy soba" (Tieru, 16 September).
//
// What this is not: a war with a player's guild. A player who declares war on
// a bot guild is refused nothing by the engine, but no bot master accepts, so
// the declaration stands until it times out; that is a decision for another
// day, not an oversight. And a guild skill cannot be used here: CGuild::UseSkill
// only works inside a war arena.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_targeting.h (the blows are that file's) and
// before playerbot_lure.h.

namespace
{
	struct TPlayerBotGuildWar
	{
		DWORD dwGuild1;
		DWORD dwGuild2;
		DWORD dwDeclaredAt;
		DWORD dwStartedAt;
		bool bStarted;
	};
	// One war a kingdom at a time.
	std::map<BYTE, TPlayerBotGuildWar> s_mapPlayerBotGuildWars;
	DWORD s_adwPlayerBotNextGuildWarTime[playerbot_empire_rules::EMPIRE_COUNT];
	DWORD s_dwNextPlayerBotGuildWarCheck = 0;
	unsigned int s_uPlayerBotGuildWarsFought = 0;
	// Who fought last, per kingdom and per guild, for the pick below: the
	// guild's latest declaration in unix seconds, kept in
	// player.playerbot_guild.last_war_at as well. Kept for the process alone
	// it was gone at every restart - and every update is one - so the first
	// war after each start went back to the pair of the smallest gap:
	// Tuskaffki and Przelew24 again at 21:36 on 18 September, the first war
	// after 2.0.77 went in.
	std::map<BYTE, std::pair<DWORD, DWORD> > s_mapPlayerBotLastWarPair;
	std::map<DWORD, DWORD> s_mapPlayerBotGuildLastWarAt;
	bool s_bPlayerBotGuildWarMemoryLoaded = false;

	// Once, before the first pick. A table from before the column (a core
	// started ahead of its migrator) answers with an error, and the memory
	// starts empty as it always used to; nothing else reads the column.
	void LoadPlayerBotGuildWarMemory()
	{
		s_bPlayerBotGuildWarMemoryLoaded = true;
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT guild_id, last_war_at FROM player.playerbot_guild WHERE last_war_at > 0"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_log(0, "PLAYERBOT_GUILD: no war memory in player.playerbot_guild (the migrator adds last_war_at); the first war may repeat a pair");
			return;
		}
		MYSQL_ROW row;
		unsigned int loaded = 0;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD gid = 0, at = 0;
			if (row[0]) str_to_number(gid, row[0]);
			if (row[1]) str_to_number(at, row[1]);
			if (gid == 0 || at == 0)
				continue;
			s_mapPlayerBotGuildLastWarAt[gid] = at;
			++loaded;
		}
		// A kingdom's last pair is its two guilds of the latest second: a
		// declaration stamps both with one number, and a kingdom has one war
		// at a time.
		std::map<BYTE, DWORD> latest;
		std::map<BYTE, std::vector<DWORD> > latestGuilds;
		for (std::map<DWORD, DWORD>::const_iterator it = s_mapPlayerBotGuildLastWarAt.begin();
				it != s_mapPlayerBotGuildLastWarAt.end(); ++it)
		{
			std::map<DWORD, TPlayerBotGuildInfo>::const_iterator info = s_mapPlayerBotGuildInfo.find(it->first);
			if (info == s_mapPlayerBotGuildInfo.end())
				continue;
			const BYTE empire = info->second.bEmpire;
			if (it->second > latest[empire])
			{
				latest[empire] = it->second;
				latestGuilds[empire].clear();
			}
			if (it->second == latest[empire])
				latestGuilds[empire].push_back(it->first);
		}
		for (std::map<BYTE, std::vector<DWORD> >::const_iterator it = latestGuilds.begin();
				it != latestGuilds.end(); ++it)
			if (it->second.size() == 2)
				s_mapPlayerBotLastWarPair[it->first] = std::make_pair(it->second[0], it->second[1]);
		sys_log(0, "PLAYERBOT_GUILD: war memory loaded guilds=%u kingdoms=%u",
				loaded, (unsigned int)s_mapPlayerBotLastWarPair.size());
	}

	bool IsPlayerBotGuildWarPair(DWORD a, DWORD b)
	{
		for (std::map<BYTE, TPlayerBotGuildWar>::const_iterator it = s_mapPlayerBotGuildWars.begin();
				it != s_mapPlayerBotGuildWars.end(); ++it)
			if ((it->second.dwGuild1 == a && it->second.dwGuild2 == b) ||
					(it->second.dwGuild1 == b && it->second.dwGuild2 == a))
				return true;
		return false;
	}

	// The guild this one is at war with, if the war is one of ours. The first
	// channel declares the wars and keeps the pair in s_mapPlayerBotGuildWars;
	// the second channel never runs that pass and has no record, so there a
	// field war between two guilds whose masters are both bots is ours - no
	// bot master accepts a player's declaration, so there is no other kind.
	CGuild* GetPlayerBotWarEnemy(CGuild* mine)
	{
		if (!mine)
			return NULL;
		// The panel's switch ends the bots' part in a war under way too, not
		// only the next declaration: the engine's war runs out its half hour,
		// but nobody walks to it or fights it, and whoever was on the ground
		// goes home. It used to stop the declarations alone, so an operator who
		// switched the wars off in the middle of one watched it go on to the
		// end - "wylaczylem w panelu, nic to nie dalo" (Hiob, 19 September).
		if (!IsPlayerBotGuildWarsEnabled())
			return NULL;
		const DWORD opp = mine->UnderAnyWar(GUILD_WAR_TYPE_FIELD);
		if (opp == 0)
			return NULL;
		if (g_bChannel == 1)
			return IsPlayerBotGuildWarPair(mine->GetID(), opp) ? CGuildManager::instance().FindGuild(opp) : NULL;
		CGuild* enemy = CGuildManager::instance().FindGuild(opp);
		if (!enemy)
			return NULL;
		const CPlayerBotManager& manager = CPlayerBotManager::instance();
		if (!manager.IsRegisteredBotPID(mine->GetMasterPID()) || !manager.IsRegisteredBotPID(enemy->GetMasterPID()))
			return NULL;
		return enemy;
	}

	struct TPlayerBotWarEntry
	{
		CGuild* guild;
		int tier;
		int online;
	};

	bool PlayerBotWarEntryOrder(const TPlayerBotWarEntry& a, const TPlayerBotWarEntry& b)
	{
		if (a.tier != b.tier)
			return a.tier < b.tier;
		return a.online > b.online;
	}

	// Two guilds of the kingdom that can fight now: both with
	// PLAYERBOT_GUILD_WAR_MIN_ONLINE bots in this core's world, neither at war
	// already, the closest tiers of any pair. The rotation by the minute alone
	// did not stop the same two meeting every time: a kingdom with exactly two
	// guilds of the top tier had one pair at a gap of none, and that pair won
	// every pick - Tuskaffki and Przelew24 fought every war of Shinsoo for a day
	// on m2zip, with seven more guilds ready (gregory_955, 17 September). So
	// the kingdom's last pair sits the next war out whenever a third guild is
	// ready, and between pairs of one gap the one whose latest war is oldest
	// goes first - a guild that has never fought before any that has.
	bool PickPlayerBotGuildWarPair(BYTE empire, DWORD dwNow, CGuild*& out1, CGuild*& out2)
	{
		std::vector<TPlayerBotWarEntry> ready;
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			if (it->second.bEmpire != empire)
				continue;
			CGuild* g = CGuildManager::instance().FindGuild(it->first);
			// A guild climbing the Demon Tower is not picked for a war
			// (playerbot_demon_tower.h).
			if (!g || g->UnderAnyWar() != 0 || IsPlayerBotGuildRaidingTower(it->first))
				continue;
			const int online = CountPlayerBotGuildOnline(g);
			if (online < PLAYERBOT_GUILD_WAR_MIN_ONLINE)
				continue;
			TPlayerBotWarEntry e;
			e.guild = g;
			e.tier = it->second.bTier;
			e.online = online;
			ready.push_back(e);
		}
		if (ready.size() < 2)
			return false;
		std::sort(ready.begin(), ready.end(), PlayerBotWarEntryOrder);
		std::pair<DWORD, DWORD> last(0, 0);
		std::map<BYTE, std::pair<DWORD, DWORD> >::const_iterator lastIt = s_mapPlayerBotLastWarPair.find(empire);
		if (lastIt != s_mapPlayerBotLastWarPair.end())
			last = lastIt->second;
		const bool skipLast = ready.size() >= 3;
		const size_t start = (size_t)(PlayerBotNavHash(dwNow / 60000U ^ 0x57415250U) % ready.size());
		bool found = false;
		size_t bestI = 0, bestJ = 1;
		int bestGap = INT_MAX;
		DWORD bestLastWar = 0;
		for (size_t n = 0; n < ready.size(); ++n)
		{
			const size_t i = (start + n) % ready.size();
			for (size_t m = 1; m < ready.size(); ++m)
			{
				const size_t j = (i + m) % ready.size();
				const DWORD gi = ready[i].guild->GetID();
				const DWORD gj = ready[j].guild->GetID();
				if (skipLast && ((gi == last.first && gj == last.second) || (gi == last.second && gj == last.first)))
					continue;
				const int gap = abs(ready[i].tier - ready[j].tier);
				std::map<DWORD, DWORD>::const_iterator wi = s_mapPlayerBotGuildLastWarAt.find(gi);
				std::map<DWORD, DWORD>::const_iterator wj = s_mapPlayerBotGuildLastWarAt.find(gj);
				const DWORD lastWar = std::max(wi == s_mapPlayerBotGuildLastWarAt.end() ? 0U : wi->second,
						wj == s_mapPlayerBotGuildLastWarAt.end() ? 0U : wj->second);
				if (!found || gap < bestGap || (gap == bestGap && lastWar < bestLastWar))
				{
					found = true;
					bestGap = gap;
					bestLastWar = lastWar;
					bestI = i;
					bestJ = j;
				}
			}
		}
		if (!found)
			return false;
		out1 = ready[bestI].guild;
		out2 = ready[bestJ].guild;
		return true;
	}

	const char* GetPlayerBotKingdomName(BYTE empire)
	{
		switch (empire)
		{
			case playerbot_empire_rules::EMPIRE_SHINSOO: return "Shinsoo";
			case playerbot_empire_rules::EMPIRE_CHUNJO: return "Chunjo";
			case playerbot_empire_rules::EMPIRE_JINNO: return "Jinno";
			default: return "?";
		}
	}

	// Once a minute for the world: the war in progress moved along, or the
	// next one declared when its time has come. A declaration is a round trip
	// through the db core - the other master accepts on a later minute, once
	// its guild reports GUILD_WAR_RECV_DECLARE - and a war the db core has
	// ended is noticed by UnderWar going false.
	void ManagePlayerBotGuildWars(DWORD dwNow)
	{
		// A war is declared once for the world, by the first channel; the bots
		// of a guild at war fight it on whichever channel they live on.
		if (g_bChannel != 1)
			return;
		if (s_dwNextPlayerBotGuildWarCheck != 0 && dwNow < s_dwNextPlayerBotGuildWarCheck)
			return;
		s_dwNextPlayerBotGuildWarCheck = dwNow + PLAYERBOT_GUILD_WAR_CHECK_INTERVAL;
		const bool enabled = IsPlayerBotGuildWarsEnabled();
		if (!s_bPlayerBotGuildWarMemoryLoaded)
			LoadPlayerBotGuildWarMemory();

		for (int empire = playerbot_empire_rules::EMPIRE_SHINSOO;
				empire <= playerbot_empire_rules::EMPIRE_JINNO; ++empire)
		{
			const long battlefield = playerbot_empire_rules::GetHomeMap(empire, playerbot_empire_rules::MAP_ROLE_M3);
			if (battlefield == 0 || !IsPlayerBotMapHostedHere(battlefield))
				continue;

			std::map<BYTE, TPlayerBotGuildWar>::iterator it = s_mapPlayerBotGuildWars.find((BYTE)empire);
			if (it != s_mapPlayerBotGuildWars.end())
			{
				TPlayerBotGuildWar& war = it->second;
				CGuild* g1 = CGuildManager::instance().FindGuild(war.dwGuild1);
				CGuild* g2 = CGuildManager::instance().FindGuild(war.dwGuild2);
				if (!g1 || !g2)
				{
					s_mapPlayerBotGuildWars.erase(it);
					continue;
				}
				if (!war.bStarted)
				{
					if (g1->UnderWar(g2->GetID()))
					{
						war.bStarted = true;
						war.dwStartedAt = dwNow;
						++s_uPlayerBotGuildWarsFought;
						char notice[200];
						snprintf(notice, sizeof(notice), "Wojna gildii: %s kontra %s! Pole bitwy: mapa gildyjna (%s), 30 minut.",
								g1->GetName(), g2->GetName(), GetPlayerBotKingdomName((BYTE)empire));
						BroadcastNotice(notice);
						sys_log(0, "PLAYERBOT_GUILD: war on %s vs %s empire=%d battlefield=%ld online=%d/%d",
								g1->GetName(), g2->GetName(), empire, battlefield,
								CountPlayerBotGuildOnline(g1), CountPlayerBotGuildOnline(g2));
					}
					// A declaration the switch finds pending is left to run out
					// (PLAYERBOT_GUILD_WAR_DECLARE_TIMEOUT) rather than accepted.
					else if (enabled && g2->GetGuildWarState(g1->GetID()) == GUILD_WAR_RECV_DECLARE)
					{
						g2->RequestDeclareWar(g1->GetID(), GUILD_WAR_TYPE_FIELD);
						sys_log(0, "PLAYERBOT_GUILD: war accepted by %s from %s", g2->GetName(), g1->GetName());
					}
					else if (dwNow - war.dwDeclaredAt > PLAYERBOT_GUILD_WAR_DECLARE_TIMEOUT)
					{
						sys_log(0, "PLAYERBOT_GUILD: war declaration went nowhere %s -> %s (state=%d), dropped",
								g1->GetName(), g2->GetName(), g2->GetGuildWarState(g1->GetID()));
						s_mapPlayerBotGuildWars.erase(it);
						s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_RETRY_MS;
					}
					continue;
				}
				if (!g1->UnderWar(g2->GetID()))
				{
					sys_log(0, "PLAYERBOT_GUILD: war over %s vs %s after %u min (wins/draws/losses %d/%d/%d and %d/%d/%d, ladder %d and %d)",
							g1->GetName(), g2->GetName(), (unsigned int)((dwNow - war.dwStartedAt) / 60000U),
							g1->GetGuildWarWinCount(), g1->GetGuildWarDrawCount(), g1->GetGuildWarLossCount(),
							g2->GetGuildWarWinCount(), g2->GetGuildWarDrawCount(), g2->GetGuildWarLossCount(),
							g1->GetLadderPoint(), g2->GetLadderPoint());
					s_mapPlayerBotGuildWars.erase(it);
					s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_INTERVAL;
				}
				else if (!enabled)
					PlayerBotLogThrottled("guild_war_off", dwNow,
							"PLAYERBOT_GUILD: wars switched off, the bots of %s and %s have left the war under way",
							g1->GetName(), g2->GetName());
				continue;
			}

			if (!enabled)
				continue;
			if (s_adwPlayerBotNextGuildWarTime[empire] == 0)
			{
				// One kingdom after another, PLAYERBOT_GUILD_WAR_KINGDOM_STAGGER
				// apart, so there is a war to watch somewhere for most of the
				// time and not three at once followed by ninety quiet minutes.
				s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_FIRST_DELAY +
						(DWORD)(empire - playerbot_empire_rules::EMPIRE_SHINSOO) * PLAYERBOT_GUILD_WAR_KINGDOM_STAGGER;
				continue;
			}
			if (dwNow < s_adwPlayerBotNextGuildWarTime[empire])
				continue;
			CGuild* a = NULL;
			CGuild* b = NULL;
			if (!PickPlayerBotGuildWarPair((BYTE)empire, dwNow, a, b))
			{
				s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_RETRY_MS;
				continue;
			}
			a->RequestDeclareWar(b->GetID(), GUILD_WAR_TYPE_FIELD);
			s_mapPlayerBotLastWarPair[(BYTE)empire] = std::make_pair(a->GetID(), b->GetID());
			// One second for both, which is how the next start finds the pair
			// again (LoadPlayerBotGuildWarMemory).
			const DWORD stamp = (DWORD)get_global_time();
			s_mapPlayerBotGuildLastWarAt[a->GetID()] = stamp;
			s_mapPlayerBotGuildLastWarAt[b->GetID()] = stamp;
			DBManager::instance().Query(
					"UPDATE player.playerbot_guild SET last_war_at=%u WHERE guild_id IN (%u, %u)",
					stamp, a->GetID(), b->GetID());
			TPlayerBotGuildWar war;
			war.dwGuild1 = a->GetID();
			war.dwGuild2 = b->GetID();
			war.dwDeclaredAt = dwNow;
			war.dwStartedAt = 0;
			war.bStarted = false;
			s_mapPlayerBotGuildWars[(BYTE)empire] = war;
			sys_log(0, "PLAYERBOT_GUILD: war declared %s -> %s empire=%d online=%d/%d",
					a->GetName(), b->GetName(), empire, CountPlayerBotGuildOnline(a), CountPlayerBotGuildOnline(b));
			// Said a minute or two before the blows, so a player who wants to
			// watch has the time to get to the guild map.
			char notice[200];
			snprintf(notice, sizeof(notice), "Za chwile wojna gildii botow (%s): %s kontra %s. Pole bitwy: mapa gildyjna.",
					GetPlayerBotKingdomName((BYTE)empire), a->GetName(), b->GetName());
			BroadcastNotice(notice);
		}
	}

	// Seconds until this kingdom's next war for the guild report: 0 while one
	// is declared or under way, -1 when none is scheduled (the switch is off,
	// the map is not hosted here, or the clock has not been set yet).
	int GetPlayerBotNextGuildWarInSeconds(BYTE empire, DWORD dwNow)
	{
		if (empire >= playerbot_empire_rules::EMPIRE_COUNT)
			return -1;
		if (s_mapPlayerBotGuildWars.find(empire) != s_mapPlayerBotGuildWars.end())
			return 0;
		if (!IsPlayerBotGuildWarsEnabled() || s_adwPlayerBotNextGuildWarTime[empire] == 0)
			return -1;
		const DWORD at = s_adwPlayerBotNextGuildWarTime[empire];
		return dwNow >= at ? 0 : (int)((at - dwNow) / 1000U);
	}

	// ------------------------------------------------------------ the ground
	//
	// Where the war is fought is not the Town.txt point. On metin2_map_guild_02
	// and _03 that point sits inside the map's safe zone - ATTR_BANPK two
	// kilometres across, where battle_is_attackable refuses every blow - and
	// the first wars on the test world ended 0:0 on both while Shinsoo's, whose
	// Town.txt is open ground, ran to 17074:14107. And the sides 1500 units off
	// it were blocked cells on two of the three maps. So the battlefield is
	// found at runtime from the map's own attributes: the open, fightable cell
	// nearest the Town.txt point, and each guild's side the open cell nearest a
	// short step from it. Once a map, kept for the process.
	bool IsPlayerBotWarGroundOpen(long lMapIndex, long x, long y)
	{
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		return tree && tree->GetAttributePtr() && !tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT | ATTR_BANPK);
	}

	// No ATTR_BANPK within margin of the point, sampled on rings of 200 units
	// in sixteen directions: the safe zone is one broad blob round the
	// Town.txt point, not a scatter of cells a sample could step between.
	bool IsPlayerBotWarGroundClearOfSafeZone(long lMapIndex, long x, long y, long margin)
	{
		for (long r = 200; r <= margin; r += 200)
		{
			for (int k = 0; k < 16; ++k)
			{
				const double angle = k * (3.14159265358979 / 8.0);
				const long px = x + (long)(r * cos(angle));
				const long py = y + (long)(r * sin(angle));
				LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, px, py);
				if (tree && tree->GetAttributePtr() && tree->IsAttr(px, py, ATTR_BANPK))
					return false;
			}
		}
		return true;
	}

	bool IsPlayerBotWarGroundFit(long lMapIndex, long x, long y, long margin)
	{
		return IsPlayerBotWarGroundOpen(lMapIndex, x, y) &&
				(margin <= 0 || IsPlayerBotWarGroundClearOfSafeZone(lMapIndex, x, y, margin));
	}

	bool FindPlayerBotWarGround(long lMapIndex, long x, long y, long radius, long& outX, long& outY,
			long margin = 0)
	{
		if (IsPlayerBotWarGroundFit(lMapIndex, x, y, margin))
		{
			outX = x;
			outY = y;
			return true;
		}
		for (long r = 100; r <= radius; r += 100)
		{
			for (long dx = -r; dx <= r; dx += 100)
			{
				const long step = (dx == -r || dx == r) ? 100 : 2 * r;
				for (long dy = -r; dy <= r; dy += step)
				{
					if (IsPlayerBotWarGroundFit(lMapIndex, x + dx, y + dy, margin))
					{
						outX = x + dx;
						outY = y + dy;
						return true;
					}
				}
			}
		}
		return false;
	}

	struct TPlayerBotWarSide
	{
		long x[2];
		long y[2];
		// The battlefield's own centre: the ground both sides were found
		// round, and what the field's leash is measured from.
		long groundX;
		long groundY;
		bool bKnown;
	};
	std::map<long, TPlayerBotWarSide> s_mapPlayerBotWarSides;

	// A bot's own spot on its guild's side of the battlefield: the side's
	// ground with a few hundred units of pid, snapped back onto open ground.
	bool GetPlayerBotWarRally(long lMapIndex, BYTE empire, int side, DWORD pid, long& outX, long& outY)
	{
		std::map<long, TPlayerBotWarSide>::iterator it = s_mapPlayerBotWarSides.find(lMapIndex);
		if (it == s_mapPlayerBotWarSides.end())
		{
			TPlayerBotWarSide sides;
			sides.bKnown = false;
			sides.groundX = sides.groundY = 0;
			playerbot_empire_rules::TPoint town;
			long cx = 0, cy = 0;
			// The margin first (PLAYERBOT_GUILD_WAR_SAFE_MARGIN); a map with no
			// such ground in reach still gets the nearest open cell, as before.
			long margin = PLAYERBOT_GUILD_WAR_SAFE_MARGIN;
			const bool haveTown = playerbot_empire_rules::GetTeleportArrival((int)empire,
					playerbot_empire_rules::TELEPORT_GUILD_MAP, town);
			bool found = haveTown &&
					FindPlayerBotWarGround(lMapIndex, town.x, town.y, PLAYERBOT_GUILD_WAR_GROUND_SEARCH, cx, cy, margin);
			if (!found && haveTown)
			{
				margin = 0;
				found = FindPlayerBotWarGround(lMapIndex, town.x, town.y, PLAYERBOT_GUILD_WAR_GROUND_SEARCH, cx, cy);
			}
			if (found)
			{
				sides.bKnown = true;
				sides.groundX = cx;
				sides.groundY = cy;
				for (int s = 0; s < 2 && sides.bKnown; ++s)
				{
					const long wantX = cx + (s == 0 ? -1 : 1) * PLAYERBOT_GUILD_WAR_RALLY_SPREAD;
					if (!FindPlayerBotWarGround(lMapIndex, wantX, cy, PLAYERBOT_GUILD_WAR_GROUND_SEARCH, sides.x[s], sides.y[s], margin))
						sides.bKnown = false;
				}
				sys_log(0, "PLAYERBOT_GUILD: battlefield map=%ld town=(%ld,%ld) ground=(%ld,%ld) sides=(%ld,%ld)/(%ld,%ld) known=%d safe_margin=%ld",
						lMapIndex, town.x, town.y, cx, cy, sides.x[0], sides.y[0], sides.x[1], sides.y[1], (int)sides.bKnown, margin);
			}
			else
				sys_err("PLAYERBOT_GUILD: no fightable ground near the Town.txt point of map %ld", lMapIndex);
			it = s_mapPlayerBotWarSides.insert(std::make_pair(lMapIndex, sides)).first;
		}
		if (!it->second.bKnown)
			return false;
		const int s = side < 0 ? 0 : 1;
		const long jx = it->second.x[s] + (long)(PlayerBotNavHash(pid ^ 0x57415221U) % 801U) - 400;
		const long jy = it->second.y[s] + (long)(PlayerBotNavHash(pid ^ 0x57415222U) % 801U) - 400;
		if (FindPlayerBotWarGround(lMapIndex, jx, jy, 400, outX, outY))
			return true;
		outX = it->second.x[s];
		outY = it->second.y[s];
		return true;
	}

	// Whether a point is on the battlefield: within
	// PLAYERBOT_GUILD_WAR_FIELD_RADIUS of the ground the sides were found round.
	// The war used to chase the nearest enemy wherever on the map it stood, so
	// a foe that walked off after a death drew its enemies after it - up the
	// slopes of Waryong, onto the bridge and the cliffs of the Shinsoo guild
	// map, a fight strung out over four kilometres with bots standing in the
	// rock faces between (prodnathin's screenshots of 21 and 22 September; the
	// bridge is at (72, 97), 3800 units from the ground). A map whose ground is
	// not known yet answers yes, as before.
	bool IsPlayerBotOnWarField(long lMapIndex, long x, long y)
	{
		std::map<long, TPlayerBotWarSide>::const_iterator it = s_mapPlayerBotWarSides.find(lMapIndex);
		if (it == s_mapPlayerBotWarSides.end() || !it->second.bKnown)
			return true;
		return DISTANCE_APPROX(x - it->second.groundX, y - it->second.groundY) <=
				PLAYERBOT_GUILD_WAR_FIELD_RADIUS;
	}

	// Whether a blow can land on this one where it stands. battle_is_attackable
	// refuses anybody on ATTR_BANPK, the struck and the striker alike, and the
	// guild map's arrival is inside its safe zone on two of the three maps.
	bool IsPlayerBotWarTargetable(LPCHARACTER other)
	{
		return other && !IsPlayerBotSafeZone(other->GetMapIndex(), other->GetX(), other->GetY());
	}

	// A foe that has just stood up is out of the fight until it has recovered.
	// It is invisible meanwhile, which mt2009's battle_is_attackable refuses
	// every blow at, so a bot that went on at it swung at nothing; and r40250
	// refuses nothing there, so it would have killed the same bot again the
	// moment it rose.
	bool IsPlayerBotWarFoeRecovering(LPCHARACTER other)
	{
		if (other->IsAffectFlag(AFF_REVIVE_INVISIBLE))
			return true;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(other->GetPlayerID());
		return it != s_mapPlayerBotAIStates.end() && it->second.bRecoveringAfterDeath;
	}

	// What the tick does for a bot's life before a fight, which this pass
	// claims the tick above: the recovery after a death, and the potions. A bot
	// that fell used to stand up where it fell at a fifth of its health and go
	// straight back at its killer: on Hiob's world 141 bots stood up 1662 times
	// in three and a half minutes, seventeen times the most, the others
	// swinging at them while they were invisible, and the guild map read as
	// "boty w nieskonczonosc sie bija ... w miejscu" (19 September). It
	// comes back now only once the recovery the rest of the tick gives it has
	// run (PLAYERBOT_RECOVERY_HP_PERCENT, invisible meanwhile). Unlike the
	// tower, a bot at war does not break off at
	// PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT: a kill is the war's score, and a
	// side that vanished at a fifth of its health would never lose one.
	bool KeepPlayerBotAliveAtWar(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (HandlePostDeathRecovery(ch, state, dwNow))
			return true;
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		return false;
	}

	// A bot of the enemy guild on the bot's map that a blow can reach, near
	// and not already everybody's (PLAYERBOT_GUILD_WAR_CROWD_PENALTY and its
	// neighbours). One standing in the safe zone was chosen like any other, so
	// its enemies walked in after it and swung at nothing for as long as it
	// stood there: "sporo stalo w bezpiecznej czesci i inne boty nie mogly ich
	// zaatakowac" (gregory_955, 17 September). The one pass over the roster
	// both finds the enemies and counts the chooser's own side on each of them.
	LPCHARACTER FindPlayerBotGuildWarFoe(LPCHARACTER ch, CGuild* mine, CGuild* enemy, DWORD heldVID)
	{
		std::vector<std::pair<LPCHARACTER, int> > foes;
		std::map<DWORD, int> attackers;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER other = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!other || other == ch || other->IsDead() || other->GetMapIndex() != ch->GetMapIndex())
				continue;
			CGuild* guild = other->GetGuild();
			if (guild == mine)
			{
				if (it->second.dwTargetVID != 0)
					++attackers[it->second.dwTargetVID];
				continue;
			}
			if (guild != enemy || !IsPlayerBotWarTargetable(other) ||
					it->second.bRecoveringAfterDeath || other->IsAffectFlag(AFF_REVIVE_INVISIBLE) ||
					!IsPlayerBotOnWarField(other->GetMapIndex(), other->GetX(), other->GetY()))
				continue;
			foes.push_back(std::make_pair(other,
					DISTANCE_APPROX(ch->GetX() - other->GetX(), ch->GetY() - other->GetY())));
		}
		LPCHARACTER best = NULL;
		long bestCost = LONG_MAX;
		for (size_t i = 0; i < foes.size(); ++i)
		{
			LPCHARACTER foe = foes[i].first;
			const DWORD vid = (DWORD)foe->GetVID();
			std::map<DWORD, int>::const_iterator crowd = attackers.find(vid);
			long cost = (long)foes[i].second +
					(long)(crowd != attackers.end() ? crowd->second : 0) * PLAYERBOT_GUILD_WAR_CROWD_PENALTY +
					(long)(PlayerBotNavHash(ch->GetPlayerID() * 2654435761U ^ foe->GetPlayerID()) %
							(DWORD)PLAYERBOT_GUILD_WAR_JITTER);
			if (vid == heldVID)
				cost -= PLAYERBOT_GUILD_WAR_KEEP_BONUS;
			if (cost < bestCost)
			{
				bestCost = cost;
				best = foe;
			}
		}
		// Once a minute, how the chooser's side is spread over its enemies -
		// the measure of "everybody on one", which deaths alone only hint at.
		if (!attackers.empty())
		{
			int attacking = 0, busiest = 0;
			for (std::map<DWORD, int>::const_iterator a = attackers.begin(); a != attackers.end(); ++a)
			{
				attacking += a->second;
				busiest = MAX(busiest, a->second);
			}
			PlayerBotLogThrottled("guild_war_spread", get_dword_time(),
					"PLAYERBOT_GUILD: war spread guild=%s enemies_up=%u attacking=%d targets=%u busiest=%d",
					mine->GetName(), (unsigned int)foes.size(), attacking,
					(unsigned int)attackers.size(), busiest);
		}
		return best;
	}

	// When each bot at war looks at its foe again, by pid.
	std::map<DWORD, DWORD> s_mapPlayerBotWarRetargetAt;

	// A bot's part in its guild's war. Claims the tick for the war's whole
	// half hour: the walk to the battlefield, the rally, the fight; and the way
	// home afterwards. A bot in a player's party stays with the player.
	bool ManagePlayerBotGuildWar(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		CGuild* mine = ch->GetGuild();
		CGuild* enemy = mine ? GetPlayerBotWarEnemy(mine) : NULL;
		if (!enemy)
		{
			if (state.dwGuildWarEnemyGID != 0)
			{
				state.dwGuildWarEnemyGID = 0;
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
				const long battlefield = playerbot_empire_rules::GetHomeMap(
						(int)ch->GetEmpire(), playerbot_empire_rules::MAP_ROLE_M3);
				if (ch->GetMapIndex() == battlefield)
				{
					long destMap = 0, destX = 0, destY = 0;
					if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
						TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "guild_war_over");
				}
			}
			return false;
		}
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		const BYTE empire = ch->GetEmpire();
		const long battlefield = playerbot_empire_rules::GetHomeMap((int)empire, playerbot_empire_rules::MAP_ROLE_M3);
		if (battlefield == 0 || !IsPlayerBotMapHostedHere(battlefield))
			return false;
		const DWORD pid = ch->GetPlayerID();
		const int side = mine->GetID() < enemy->GetID() ? -1 : 1;
		long rallyX = 0, rallyY = 0;
		if (!GetPlayerBotWarRally(battlefield, empire, side, pid, rallyX, rallyY))
			return false;

		if (state.dwGuildWarEnemyGID != enemy->GetID())
		{
			state.dwGuildWarEnemyGID = enemy->GetID();
			PlayerBotLogThrottled("guild_war_to", dwNow,
					"PLAYERBOT_GUILD: to war pid=%u name=%s guild=%s enemy=%s map=%ld rally=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), mine->GetName(), enemy->GetName(), ch->GetMapIndex(), rallyX, rallyY);
		}
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);

		if (ch->GetMapIndex() != battlefield)
		{
			if (dwNow < state.dwNextGuildWarMoveTime)
				return true;
			state.dwNextGuildWarMoveTime = dwNow + 5000;
			TransitionPlayerBotMap(ch, state, battlefield, rallyX, rallyY, dwNow, "guild_war");
			return true;
		}
		// Ahead of the horse: the recovery walks off the ground on its own
		// terms, and a dismount would only have it mount again.
		if (KeepPlayerBotAliveAtWar(ch, state, dwNow))
			return true;
		// A transport horse comes off for the fight, as in a duel.
		// On foot, every rider, and the horse sent away rather than left to
		// trot behind the fight: the operator's rule for a war ("niech boty
		// odwoluja konie i walcza tylko na pieszo", Tieru, 17 September),
		// which the battle horse's own fitness to fight used to exempt.
		if (ch->IsRiding())
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "guild_war");
			if (ch->GetHorse())
				ch->HorseSummon(false);
			return true;
		}
		if (ch->GetHorse())
			ch->HorseSummon(false);

		// Off the field - chased up a slope, stood up after a death somewhere
		// else, arrived at the map's edge - the bot walks back to its spot
		// before it looks for anybody (IsPlayerBotOnWarField).
		if (!IsPlayerBotOnWarField(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (dwNow >= state.dwNextGuildWarMoveTime)
			{
				state.dwNextGuildWarMoveTime = dwNow + 2000;
				MovePlayerBot(ch, rallyX, rallyY, dwNow, 8, true, false);
			}
			return true;
		}

		// The foe in hand is kept while it stands on the field, and looked at
		// again every PLAYERBOT_GUILD_WAR_RETARGET_MS: the search is every bot
		// in the world, so not on every tick, but often enough for a bot to
		// turn to the enemy who came up beside it and for the crowd on one
		// enemy to thin out.
		LPCHARACTER foe = NULL;
		if (state.dwTargetVID != 0)
		{
			LPCHARACTER held = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (held && !held->IsDead() && held->GetGuild() == enemy &&
					held->GetMapIndex() == ch->GetMapIndex() && IsPlayerBotWarTargetable(held) &&
					!IsPlayerBotWarFoeRecovering(held) &&
					IsPlayerBotOnWarField(held->GetMapIndex(), held->GetX(), held->GetY()))
				foe = held;
		}
		DWORD& retargetAt = s_mapPlayerBotWarRetargetAt[pid];
		if (!foe || dwNow >= retargetAt)
		{
			retargetAt = dwNow + PLAYERBOT_GUILD_WAR_RETARGET_MS;
			LPCHARACTER chosen = FindPlayerBotGuildWarFoe(ch, mine, enemy, foe ? (DWORD)foe->GetVID() : 0);
			if (chosen)
				foe = chosen;
		}
		if (!foe)
		{
			state.dwTargetVID = 0;
			if (DISTANCE_APPROX(ch->GetX() - rallyX, ch->GetY() - rallyY) > 600 &&
					dwNow >= state.dwNextGuildWarMoveTime)
			{
				state.dwNextGuildWarMoveTime = dwNow + 3000;
				MovePlayerBot(ch, rallyX, rallyY, dwNow, 8, true, false);
			}
			return true;
		}

		const int distance = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		state.dwTargetVID = (DWORD)foe->GetVID();
		ch->SetVictim(foe);
		ch->SetRotationToXY(foe->GetX(), foe->GetY());

		// The same fight a duel is: the aura first, a caster from its range, a
		// warrior across the gap, a blade from where it reaches.
		if (distance <= PLAYERBOT_DUEL_BUFF_RANGE && ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
			return true;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW);
		const int combatRange = isBow ? 800 : PLAYERBOT_DUEL_MELEE_RANGE;
		const bool caster = ch->GetJob() == JOB_SHAMAN ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2);
		// A bot standing in the safe zone - just arrived, or up again at the
		// town point - cannot strike from there either, so it walks at its foe
		// until it is out, whatever the range says.
		const bool inSafeZone = !IsPlayerBotWarTargetable(ch);
		if (distance > combatRange || inSafeZone)
		{
			if (!inSafeZone && !isBow && caster && distance <= PLAYERBOT_DUEL_CASTER_RANGE &&
					dwNow >= state.dwNextSkillCastTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				if (CastPlayerBotDuelSkill(ch, foe, state, dwNow))
					return true;
			}
			if (!inSafeZone && !isBow && distance <= PLAYERBOT_SEARCH_RANGE &&
					TryPlayerBotDuelGapCloser(ch, foe, state, dwNow, distance))
				return true;
			if (dwNow >= state.dwNextGuildWarMoveTime)
			{
				state.dwNextGuildWarMoveTime = dwNow + 1000;
				// Out of the safe zone by way of the rally, which is open,
				// fightable ground by construction (FindPlayerBotWarGround):
				// a walk at a foe a step away would count as arrived at once
				// and leave the bot standing where it cannot strike.
				if (inSafeZone)
					MovePlayerBot(ch, rallyX, rallyY, dwNow, 4, false, false);
				else
					MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 4, distance > PLAYERBOT_SEARCH_RANGE, false);
			}
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_FIGHTING);
		if (!CastPlayerBotDuelSkill(ch, foe, state, dwNow))
			ExecutePlayerBotBasicAttack(ch, foe, state, dwNow);
		return true;
	}
}

#endif
