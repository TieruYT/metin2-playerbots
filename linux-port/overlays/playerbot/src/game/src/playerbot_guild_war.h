#ifndef __INC_METIN2_PLAYERBOT_GUILD_WAR_H__
#define __INC_METIN2_PLAYERBOT_GUILD_WAR_H__

// Guild wars: between the bots' guilds, and between a bot guild and a
// player's.
//
// Two bot guilds of one kingdom, of the same tier where there is a pair, fight
// a field war - the engine's own GUILD_WAR_TYPE_FIELD: declared by one master
// and accepted by the other through CGuild::RequestDeclareWar, thirty minutes
// on the db core's clock, every kill between the two counted by
// CGuildManager::Kill, the winner and the ladder points settled by the db
// core, the notices its own. A field war is fought anywhere, so the
// battlefield is ours to choose: the kingdom's guild map (Waryong and its two
// mirrors), which every core hosts for its own kingdom, is nearly empty, and
// needs none of the arena maps 110/111 that only the first core carries. The
// middle is open, fightable ground near the map's Town.txt point (two of the
// three points sit inside a safe zone), each guild has a camp of its
// own on either side of it, and a war opens with a muster: each side at its
// camp, buffing, for PLAYERBOT_GUILD_WAR_MUSTER_SECONDS. Then every bot goes
// for the nearest enemy it can see, which brings the two sides together in
// the middle; the dead stand up at their own camp and come back. "Potem
// stworzymy wojny gildii, gdzie beda chodzic na specjalna mape i walczyc jak
// gracze miedzy soba" (Tieru, 16 September).
//
// A player's guild takes on a bot guild of its own kingdom by the master's
// ordinary declaration (the guild window, or /war). The engine refuses a
// player every field war (CGuild::CanStartWar) and an arena war needs the
// arena's map on the player's own core, so the engine hands a declaration on
// a bot guild here (playerbotify.py, apply_player_war_on_bot_guilds); it is
// always a field war on the kingdom's guild map, and the bot guild answers it
// on the guild chat, with a rest between wars for both sides. "Mozliwosc
// rozpoczecia wojny gildii na gildie botow" (Remigiusz, 18 September). A
// guild skill cannot be used here: CGuild::UseSkill only works inside a war
// arena.
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
		// A player's guild against a bot guild: dwGuild1 is the player's,
		// which declared, and dwGuild2 the bots', which accepted.
		bool bPlayerWar;
	};
	// One war a kingdom at a time, a player's included: the kingdom has one
	// battlefield.
	std::map<BYTE, TPlayerBotGuildWar> s_mapPlayerBotGuildWars;
	DWORD s_adwPlayerBotNextGuildWarTime[playerbot_empire_rules::EMPIRE_COUNT];
	// A kingdom whose last pick found no pair: its clock is a retry, not a war
	// on its way. The tower keeps a guild out of a raid for the ten minutes
	// before its kingdom's war, and the retry re-arms that clock ten minutes
	// ahead every ten minutes - so a kingdom with one bot guild could never
	// raid the Tower again after the first try of a start ("nie wyswietla sie
	// powiadomienie na chacie jak jakas gildia idzie na dt", prodnathin,
	// 25 September: ViceVersa, Shinsoo's only guild, thirteen bots of forty,
	// skipped by every pick while its next war stood at 59..539 seconds).
	bool s_abPlayerBotGuildWarNoPair[playerbot_empire_rules::EMPIRE_COUNT];
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
	// A player's declaration on a bot guild, waiting for the bots' answer
	// (NotePlayerBotGuildWarDeclared, which the engine calls on every core).
	struct TPlayerBotWarOffer
	{
		DWORD dwFrom;
		DWORD dwTo;
		BYTE bType;
		DWORD dwAt;
	};
	std::vector<TPlayerBotWarOffer> s_vecPlayerBotWarOffers;
	// When a player's guild last went to war with bots, in unix seconds. Kept
	// for the process only: a restart forgives it, the bot guild's own rest
	// (last_war_at) is on the table.
	std::map<DWORD, DWORD> s_mapPlayerBotPlayerGuildLastWarAt;

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

	// A guild whose master is a bot: every guild the bots founded. Bots never
	// invite people, so a person is never in one.
	bool IsPlayerBotGuild(CGuild* guild)
	{
		return guild && CPlayerBotManager::instance().IsRegisteredBotPID(guild->GetMasterPID());
	}

	// The kingdom a bot guild belongs to: its row in player.playerbot_guild,
	// else its master's.
	BYTE GetPlayerBotGuildEmpire(CGuild* guild)
	{
		if (!guild)
			return 0;
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();
		std::map<DWORD, TPlayerBotGuildInfo>::const_iterator info = s_mapPlayerBotGuildInfo.find(guild->GetID());
		if (info != s_mapPlayerBotGuildInfo.end() && info->second.bEmpire != 0)
			return info->second.bEmpire;
		LPCHARACTER master = guild->GetMasterCharacter();
		if (master)
			return master->GetEmpire();
		return CPlayerBotManager::instance().GetRegisteredEmpire(guild->GetMasterPID());
	}

	// The kingdom of a player's guild: its master's, wherever he is logged in.
	// Zero when the master is not in the game at all.
	BYTE GetPlayerBotPersonGuildEmpire(CGuild* guild)
	{
		if (!guild)
			return 0;
		LPCHARACTER master = guild->GetMasterCharacter();
		if (master)
			return master->GetEmpire();
		CCI* cci = P2P_MANAGER::instance().FindByPID(guild->GetMasterPID());
		return cci ? cci->bEmpire : 0;
	}

	// The guild this one is at war with, if the war is one of ours. The first
	// channel declares and answers the wars and keeps the pair in
	// s_mapPlayerBotGuildWars; the second channel never runs that pass and has
	// no record, so there a field war between two guilds whose masters are
	// both bots is ours. A player's war is fought on the first channel only.
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
		if (!IsPlayerBotGuild(mine) || !IsPlayerBotGuild(enemy))
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

	// ------------------------------------------------------------ the ground
	//
	// Where the war is fought is not the Town.txt point. On metin2_map_guild_02
	// and _03 that point sits inside the map's safe zone - ATTR_BANPK two
	// kilometres across, where battle_is_attackable refuses every blow - and
	// the first wars on the test world ended 0:0 on both while Shinsoo's, whose
	// Town.txt is open ground, ran to 17074:14107. So the battlefield is found
	// at runtime from the map's own attributes: the middle is the open,
	// fightable cell nearest the Town.txt point - or up to
	// PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT from it where that gives the camps more
	// room - and each guild's camp open ground on either side of it
	// (PLAYERBOT_GUILD_WAR_CAMP_DISTANCES). Once a map, kept for the process.
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

	// The share, in percent, of the cells within PLAYERBOT_GUILD_WAR_OPEN_RADIUS
	// of a point that a fight could stand on: neither blocked nor the safe zone.
	int GetPlayerBotWarGroundOpenness(long lMapIndex, long x, long y)
	{
		const long r = PLAYERBOT_GUILD_WAR_OPEN_RADIUS;
		int samples = 0, open = 0;
		for (long dx = -r; dx <= r; dx += PLAYERBOT_GUILD_WAR_OPEN_SAMPLE)
		{
			for (long dy = -r; dy <= r; dy += PLAYERBOT_GUILD_WAR_OPEN_SAMPLE)
			{
				if (dx * dx + dy * dy > r * r)
					continue;
				++samples;
				LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x + dx, y + dy);
				if (tree && tree->GetAttributePtr() &&
						!tree->IsAttr(x + dx, y + dy, ATTR_BLOCK | ATTR_OBJECT | ATTR_BANPK))
					++open;
			}
		}
		return samples > 0 ? open * 100 / samples : 0;
	}

	// The most open ground within PLAYERBOT_GUILD_WAR_OPEN_SEARCH of the
	// Town.txt point and reachable from it, the nearest of the most open
	// (PLAYERBOT_GUILD_WAR_OPEN_*). Once a map, at its first war: some four
	// thousand candidates, most refused by the first attribute they ask.
	bool FindPlayerBotOpenWarGround(long lMapIndex, long townX, long townY, long& outX, long& outY,
			int& outOpen)
	{
		int best = -1;
		long bestDistance = 0;
		for (long x = townX - PLAYERBOT_GUILD_WAR_OPEN_SEARCH; x <= townX + PLAYERBOT_GUILD_WAR_OPEN_SEARCH;
				x += PLAYERBOT_GUILD_WAR_OPEN_STEP)
		{
			for (long y = townY - PLAYERBOT_GUILD_WAR_OPEN_SEARCH; y <= townY + PLAYERBOT_GUILD_WAR_OPEN_SEARCH;
					y += PLAYERBOT_GUILD_WAR_OPEN_STEP)
			{
				if (!IsPlayerBotWarGroundFit(lMapIndex, x, y, PLAYERBOT_GUILD_WAR_SAFE_MARGIN))
					continue;
				const int open = GetPlayerBotWarGroundOpenness(lMapIndex, x, y);
				const long distance = DISTANCE_APPROX(x - townX, y - townY);
				if (open < best || (open == best && distance >= bestDistance))
					continue;
				if (!IsPlayerBotReachable(lMapIndex, townX, townY, x, y))
					continue;
				best = open;
				bestDistance = distance;
				outX = x;
				outY = y;
			}
		}
		outOpen = best;
		return best >= 0;
	}

	struct TPlayerBotWarSide
	{
		// Each side's camp; both are the middle where the ground has no room.
		long campX[2];
		long campY[2];
		// The battlefield's middle: where the sides meet, and what the field's
		// leash is measured from.
		long groundX;
		long groundY;
		bool bKnown;
		bool bCamps;
	};
	std::map<long, TPlayerBotWarSide> s_mapPlayerBotWarSides;

	// Two camps on opposite sides of a middle, the given distance apart from
	// it, along the first of eight axes at which both ends are open ground
	// clear of the safe zone and joined to the middle by the navigation grid.
	bool FindPlayerBotWarCampPair(long lMapIndex, long midX, long midY, long distance,
			long& ax, long& ay, long& bx, long& by)
	{
		for (int k = 0; k < 8; ++k)
		{
			const double angle = k * (3.14159265358979 / 8.0);
			const long dx = (long)(distance * cos(angle));
			const long dy = (long)(distance * sin(angle));
			if (!FindPlayerBotWarGround(lMapIndex, midX + dx, midY + dy,
						PLAYERBOT_GUILD_WAR_CAMP_SNAP, ax, ay, PLAYERBOT_GUILD_WAR_CAMP_SAFE_MARGIN) ||
					!FindPlayerBotWarGround(lMapIndex, midX - dx, midY - dy,
						PLAYERBOT_GUILD_WAR_CAMP_SNAP, bx, by, PLAYERBOT_GUILD_WAR_CAMP_SAFE_MARGIN))
				continue;
			if (IsPlayerBotReachable(lMapIndex, midX, midY, ax, ay) &&
					IsPlayerBotReachable(lMapIndex, midX, midY, bx, by))
				return true;
		}
		return false;
	}

	// The camps farthest apart of PLAYERBOT_GUILD_WAR_CAMP_DISTANCES, about the
	// ground found round the Town.txt point or a middle moved from it by up to
	// PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT (the nearest middle of the farthest
	// pair). A moved middle is ground as fit as the first - margin is the one
	// that ground was found with - and joined to it. Once a map: five tries at
	// most on the three guild maps, measured offline on their server_attr.
	bool FindPlayerBotWarCamps(long lMapIndex, long margin, TPlayerBotWarSide& sides)
	{
		std::vector<std::pair<long, std::pair<long, long> > > middles;
		for (long ox = -PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT; ox <= PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT;
				ox += PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT_STEP)
			for (long oy = -PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT; oy <= PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT;
					oy += PLAYERBOT_GUILD_WAR_MIDDLE_SHIFT_STEP)
				middles.push_back(std::make_pair(ox * ox + oy * oy, std::make_pair(ox, oy)));
		std::stable_sort(middles.begin(), middles.end());
		const size_t distances = sizeof(PLAYERBOT_GUILD_WAR_CAMP_DISTANCES) / sizeof(PLAYERBOT_GUILD_WAR_CAMP_DISTANCES[0]);
		long best = 0;
		long bestMidX = 0, bestMidY = 0;
		long camps[4] = { 0, 0, 0, 0 };
		for (size_t i = 0; i < middles.size(); ++i)
		{
			const long ox = middles[i].second.first;
			const long oy = middles[i].second.second;
			const long midX = sides.groundX + ox;
			const long midY = sides.groundY + oy;
			if ((ox != 0 || oy != 0) && (!IsPlayerBotWarGroundFit(lMapIndex, midX, midY, margin) ||
						!IsPlayerBotReachable(lMapIndex, sides.groundX, sides.groundY, midX, midY)))
				continue;
			for (size_t d = 0; d < distances; ++d)
			{
				const long distance = PLAYERBOT_GUILD_WAR_CAMP_DISTANCES[d];
				if (distance <= best)
					break;
				long ax = 0, ay = 0, bx = 0, by = 0;
				if (!FindPlayerBotWarCampPair(lMapIndex, midX, midY, distance, ax, ay, bx, by))
					continue;
				best = distance;
				bestMidX = midX;
				bestMidY = midY;
				camps[0] = ax;
				camps[1] = ay;
				camps[2] = bx;
				camps[3] = by;
				break;
			}
			if (best == PLAYERBOT_GUILD_WAR_CAMP_DISTANCES[0])
				break;
		}
		if (best == 0)
			return false;
		sides.groundX = bestMidX;
		sides.groundY = bestMidY;
		sides.campX[0] = camps[0];
		sides.campY[0] = camps[1];
		sides.campX[1] = camps[2];
		sides.campY[1] = camps[3];
		return true;
	}

	const TPlayerBotWarSide* GetPlayerBotWarSides(long lMapIndex, BYTE empire)
	{
		std::map<long, TPlayerBotWarSide>::iterator it = s_mapPlayerBotWarSides.find(lMapIndex);
		if (it == s_mapPlayerBotWarSides.end())
		{
			TPlayerBotWarSide sides;
			sides.bKnown = false;
			sides.bCamps = false;
			sides.groundX = sides.groundY = 0;
			playerbot_empire_rules::TPoint town;
			long cx = 0, cy = 0;
			// The margin first (PLAYERBOT_GUILD_WAR_SAFE_MARGIN); a map with no
			// such ground in reach still gets the nearest open cell, as before.
			long margin = PLAYERBOT_GUILD_WAR_SAFE_MARGIN;
			const bool haveTown = playerbot_empire_rules::GetTeleportArrival((int)empire,
					playerbot_empire_rules::TELEPORT_GUILD_MAP, town);
			// The most open ground in reach first (PLAYERBOT_GUILD_WAR_OPEN_*),
			// the nearest open cell only where none is found.
			int openness = -1;
			bool found = haveTown &&
					FindPlayerBotOpenWarGround(lMapIndex, town.x, town.y, cx, cy, openness);
			if (!found && haveTown)
				found = FindPlayerBotWarGround(lMapIndex, town.x, town.y, PLAYERBOT_GUILD_WAR_GROUND_SEARCH, cx, cy, margin);
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
				sides.bCamps = FindPlayerBotWarCamps(lMapIndex, margin, sides);
				if (!sides.bCamps)
					for (int s = 0; s < 2; ++s)
					{
						sides.campX[s] = cx;
						sides.campY[s] = cy;
					}
				sys_log(0, "PLAYERBOT_GUILD: battlefield map=%ld town=(%ld,%ld) ground=(%ld,%ld) open=%d middle=(%ld,%ld) middle_open=%d camps=(%ld,%ld)/(%ld,%ld) apart=%d camp_gap=%d safe_margin=%ld",
						lMapIndex, town.x, town.y, cx, cy, openness, sides.groundX, sides.groundY,
						GetPlayerBotWarGroundOpenness(lMapIndex, sides.groundX, sides.groundY),
						sides.campX[0], sides.campY[0], sides.campX[1], sides.campY[1], (int)sides.bCamps,
						DISTANCE_APPROX(sides.campX[0] - sides.campX[1], sides.campY[0] - sides.campY[1]), margin);
			}
			else
				sys_err("PLAYERBOT_GUILD: no fightable ground near the Town.txt point of map %ld", lMapIndex);
			it = s_mapPlayerBotWarSides.insert(std::make_pair(lMapIndex, sides)).first;
		}
		return it->second.bKnown ? &it->second : NULL;
	}

	// A bot's own spot about a point: up to jitter units of pid, snapped back
	// onto open ground.
	void GetPlayerBotWarSpot(long lMapIndex, long baseX, long baseY, long jitter, DWORD pid, DWORD salt,
			long& outX, long& outY)
	{
		const long jx = baseX + (long)(PlayerBotNavHash(pid ^ salt) % (DWORD)(2 * jitter + 1)) - jitter;
		const long jy = baseY + (long)(PlayerBotNavHash(pid ^ (salt + 1U)) % (DWORD)(2 * jitter + 1)) - jitter;
		if (FindPlayerBotWarGround(lMapIndex, jx, jy, jitter, outX, outY))
			return;
		outX = baseX;
		outY = baseY;
	}

	// A bot's place at its guild's camp, side 0 or 1.
	bool GetPlayerBotWarCamp(long lMapIndex, BYTE empire, int side, DWORD pid, long& outX, long& outY)
	{
		const TPlayerBotWarSide* sides = GetPlayerBotWarSides(lMapIndex, empire);
		if (!sides)
			return false;
		const int s = side <= 0 ? 0 : 1;
		GetPlayerBotWarSpot(lMapIndex, sides->campX[s], sides->campY[s], 250, pid, 0x57415223U, outX, outY);
		return true;
	}

	// A bot's place in the middle, where it holds when it has nobody to fight.
	bool GetPlayerBotWarMiddle(long lMapIndex, BYTE empire, DWORD pid, long& outX, long& outY)
	{
		const TPlayerBotWarSide* sides = GetPlayerBotWarSides(lMapIndex, empire);
		if (!sides)
			return false;
		GetPlayerBotWarSpot(lMapIndex, sides->groundX, sides->groundY, 400, pid, 0x57415221U, outX, outY);
		return true;
	}

	// Whether a point is on the battlefield: within PLAYERBOT_GUILD_WAR_FIELD_RADIUS
	// of the middle, or about either camp. The war used to chase the nearest
	// enemy wherever on the map it stood, so a foe that walked off after a
	// death drew its enemies after it - up the slopes of Waryong, onto the
	// bridge and the cliffs of the Shinsoo guild map, a fight strung out over
	// four kilometres with bots standing in the rock faces between
	// (prodnathin's screenshots of 21 and 22 September; the bridge is at (72,
	// 97), 3800 units from the ground). The camps widen the field along their
	// own axis only, not to every side. A map whose ground is not known yet
	// answers yes, as before.
	bool IsPlayerBotOnWarField(long lMapIndex, long x, long y)
	{
		std::map<long, TPlayerBotWarSide>::const_iterator it = s_mapPlayerBotWarSides.find(lMapIndex);
		if (it == s_mapPlayerBotWarSides.end() || !it->second.bKnown)
			return true;
		const TPlayerBotWarSide& sides = it->second;
		if (DISTANCE_APPROX(x - sides.groundX, y - sides.groundY) <= PLAYERBOT_GUILD_WAR_FIELD_RADIUS)
			return true;
		if (!sides.bCamps)
			return false;
		for (int s = 0; s < 2; ++s)
			if (DISTANCE_APPROX(x - sides.campX[s], y - sides.campY[s]) <=
					PLAYERBOT_GUILD_WAR_CAMP_RADIUS + PLAYERBOT_GUILD_WAR_FIELD_BEYOND_CAMP)
				return true;
		return false;
	}

	// Whether the war's first seconds are still going: each side at its camp,
	// buffing (PLAYERBOT_GUILD_WAR_MUSTER_SECONDS). The engine stamps the
	// war's start on every core, so the second channel keeps the same clock.
	bool IsPlayerBotWarMustering(CGuild* mine, CGuild* enemy)
	{
		if (!mine || !enemy)
			return false;
		const DWORD startedAt = mine->GetWarStartTime(enemy->GetID());
		return startedAt != 0 && (DWORD)get_global_time() < startedAt + PLAYERBOT_GUILD_WAR_MUSTER_SECONDS;
	}

	// A player's "Tak" to the letter that asks whether to join the war
	// (guild_war_join, "czy chcesz wziac udzial w wojnie?"). The engine's
	// CGuild::GuildWarEntryAccept returns at once for a field war, which has
	// no war map, so in a war on a bot guild - always a field war, fought on
	// the kingdom's guild map - the answer took the player nowhere (Remigiusz,
	// 24 September, with a video: the letter, "Tak", and Joan still round
	// him). It takes the player to its own guild's camp there now, the side
	// the engine's arenas would give it (the lower guild id is side 0). The
	// bots fight on channel 1 only, so a player elsewhere is told to change
	// channel. Any other field war is fought wherever the guilds meet.
	void EnterPlayerBotFieldWar(LPCHARACTER ch, DWORD dwMyGuild, DWORD dwOppGuild)
	{
		if (!ch || !ch->IsPC() || !ch->GetDesc() || ch->GetDesc()->IsBot())
			return;
		CGuild* mine = CGuildManager::instance().FindGuild(dwMyGuild);
		CGuild* enemy = CGuildManager::instance().FindGuild(dwOppGuild);
		BYTE empire = 0;
		if (enemy && IsPlayerBotGuild(enemy))
			empire = GetPlayerBotGuildEmpire(enemy);
		else if (mine && IsPlayerBotGuild(mine))
			empire = GetPlayerBotGuildEmpire(mine);
		if (empire == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] To wojna w polu: walczycie tam, gdzie sie spotkacie.");
			return;
		}
		if (g_bChannel != 1)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Boty walcza w wojnach gildii tylko na kanale 1 - zmien kanal i kliknij jeszcze raz.");
			return;
		}
		const long battlefield = playerbot_empire_rules::GetHomeMap((int)empire, playerbot_empire_rules::MAP_ROLE_M3);
		const int side = dwMyGuild < dwOppGuild ? 0 : 1;
		long x = 0, y = 0;
		bool camp = battlefield != 0 && IsPlayerBotMapHostedHere(battlefield) &&
				GetPlayerBotWarCamp(battlefield, empire, side, ch->GetPlayerID(), x, y);
		if (!camp)
		{
			// Another core hosts the guild map: the kingdom's own arrival on
			// it, and the engine's warp does the rest.
			playerbot_empire_rules::TPoint town;
			if (!playerbot_empire_rules::GetTeleportArrival((int)empire,
					playerbot_empire_rules::TELEPORT_GUILD_MAP, town))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Nie znam pola bitwy tej wojny.");
				return;
			}
			x = town.x;
			y = town.y;
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Przenosze cie do obozu twojej gildii na mapie gildyjnej.");
		sys_log(0, "PLAYERBOT_GUILD: player joins the field war pid=%u name=%s guild=%u enemy=%u empire=%d map=%ld side=%d camp=%d to=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), dwMyGuild, dwOppGuild, (int)empire, battlefield, side,
				camp ? 1 : 0, x, y);
		ch->WarpSet(x, y);
	}

	// ------------------------------------------------ a player's declaration

	// The master's declaration on a bot guild (cmd_general.cpp's do_war,
	// through the manager). A field war, whatever the window asked for: the
	// arena maps are not on the village cores, and a player may not declare a
	// field war at all by the engine's rules, which is why it is declared here
	// and past them. What can be told on the spot is told on the spot; the
	// rest is the bots' answer (AnswerPlayerBotWarOffer), on the guild chat.
	bool HandlePlayerWarOnBotGuild(LPCHARACTER ch, CGuild* mine, CGuild* opp)
	{
		if (!ch || !mine || !opp || !IsPlayerBotGuild(opp) || IsPlayerBotGuild(mine))
			return false;
		if (!IsPlayerBotGuildWarsEnabled())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Wojny z gildiami botow sa wylaczone w panelu serwera.");
			return true;
		}
		const BYTE empire = GetPlayerBotGuildEmpire(opp);
		if (empire != 0 && empire != ch->GetEmpire())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Wojne mozna wypowiedziec tylko gildii botow z twojego krolestwa.");
			return true;
		}
		if (opp->UnderAnyWar() != 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Gildia %s walczy teraz w innej wojnie.", opp->GetName());
			return true;
		}
		const int stateNow = mine->GetGuildWarState(opp->GetID());
		if (stateNow == GUILD_WAR_SEND_DECLARE)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Wojna gildii %s jest juz wypowiedziana - boty odpowiedza na czacie gildii.", opp->GetName());
			return true;
		}
		if (stateNow != GUILD_WAR_NONE)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Z gildia %s trwa juz wojna albo jej konczenie.", opp->GetName());
			return true;
		}
		mine->RequestDeclareWar(opp->GetID(), GUILD_WAR_TYPE_FIELD);
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Wypowiedziano wojne gildii botow %s. Z botami to zawsze wojna na mapie gildyjnej waszego krolestwa. Odpowiedz przyjdzie za kilka sekund na czacie gildii.", opp->GetName());
		sys_log(0, "PLAYERBOT_GUILD: player war declared by pid=%u name=%s guild=%s on %s",
				ch->GetPlayerID(), ch->GetName(), mine->GetName(), opp->GetName());
		return true;
	}

	// Every declaration, on every core (CInputDB::GuildWar through the
	// manager): one by a player's guild on a bot guild waits here for the
	// bots' answer. The bots' own declarations are the scheduler's.
	void NotePlayerBotGuildWarDeclared(DWORD dwFrom, DWORD dwTo, BYTE bType)
	{
		CGuild* from = CGuildManager::instance().FindGuild(dwFrom);
		CGuild* to = CGuildManager::instance().FindGuild(dwTo);
		if (!from || !to || IsPlayerBotGuild(from) || !IsPlayerBotGuild(to))
			return;
		for (size_t i = 0; i < s_vecPlayerBotWarOffers.size(); ++i)
			if (s_vecPlayerBotWarOffers[i].dwFrom == dwFrom && s_vecPlayerBotWarOffers[i].dwTo == dwTo)
				return;
		TPlayerBotWarOffer offer;
		offer.dwFrom = dwFrom;
		offer.dwTo = dwTo;
		offer.bType = bType;
		offer.dwAt = get_dword_time();
		s_vecPlayerBotWarOffers.push_back(offer);
	}

	// The bots' answer to one declaration, from the core that fights the
	// kingdom's wars; every other core forgets it.
	void AnswerPlayerBotWarOffer(const TPlayerBotWarOffer& offer, DWORD dwNow)
	{
		CGuild* person = CGuildManager::instance().FindGuild(offer.dwFrom);
		CGuild* bots = CGuildManager::instance().FindGuild(offer.dwTo);
		// Withdrawn, refused or answered meanwhile.
		if (!person || !bots || bots->GetGuildWarState(offer.dwFrom) != GUILD_WAR_RECV_DECLARE)
			return;
		const BYTE empire = GetPlayerBotGuildEmpire(bots);
		const long battlefield = empire == 0 ? 0 :
				playerbot_empire_rules::GetHomeMap((int)empire, playerbot_empire_rules::MAP_ROLE_M3);
		if (battlefield == 0 || !IsPlayerBotMapHostedHere(battlefield))
			return;

		char why[192] = "";
		const DWORD stamp = (DWORD)get_global_time();
		const BYTE personEmpire = GetPlayerBotPersonGuildEmpire(person);
		std::map<BYTE, TPlayerBotGuildWar>::const_iterator slot = s_mapPlayerBotGuildWars.find(empire);
		std::map<DWORD, DWORD>::const_iterator botsLast = s_mapPlayerBotGuildLastWarAt.find(offer.dwTo);
		std::map<DWORD, DWORD>::const_iterator personLast = s_mapPlayerBotPlayerGuildLastWarAt.find(offer.dwFrom);
		const int online = CountPlayerBotGuildOnline(bots);
		if (offer.bType != GUILD_WAR_TYPE_FIELD)
			snprintf(why, sizeof(why), "boty walcza tylko w wojnie na mapie gildyjnej");
		else if (!IsPlayerBotGuildWarsEnabled())
			snprintf(why, sizeof(why), "wojny z gildiami botow sa wylaczone w panelu serwera");
		else if (personEmpire != 0 && personEmpire != empire)
			snprintf(why, sizeof(why), "walczymy tylko z gildiami z naszego krolestwa");
		else if (bots->UnderAnyWar() != 0 || IsPlayerBotGuildRaidingTower(offer.dwTo))
			snprintf(why, sizeof(why), "walczymy teraz gdzie indziej (wojna albo Wieza Demonow)");
		else if (slot != s_mapPlayerBotGuildWars.end())
		{
			CGuild* g1 = CGuildManager::instance().FindGuild(slot->second.dwGuild1);
			CGuild* g2 = CGuildManager::instance().FindGuild(slot->second.dwGuild2);
			const int left = slot->second.bStarted
					? std::max(1, 30 - (int)((dwNow - slot->second.dwStartedAt) / 60000U)) : 31;
			snprintf(why, sizeof(why), "na mapie gildyjnej trwa juz wojna %s kontra %s, sprobujcie za okolo %d min",
					g1 ? g1->GetName() : "?", g2 ? g2->GetName() : "?", left);
		}
		else if (online < PLAYERBOT_GUILD_WAR_MIN_ONLINE)
			snprintf(why, sizeof(why), "w grze jest nas za malo (%d z %d)", online, PLAYERBOT_GUILD_WAR_MIN_ONLINE);
		else if (botsLast != s_mapPlayerBotGuildLastWarAt.end() && stamp < botsLast->second + PLAYERBOT_GUILD_WAR_BOT_REST_SECONDS)
			snprintf(why, sizeof(why), "odpoczywamy po ostatniej wojnie, sprobujcie za %u min",
					(botsLast->second + PLAYERBOT_GUILD_WAR_BOT_REST_SECONDS - stamp + 59) / 60);
		else if (personLast != s_mapPlayerBotPlayerGuildLastWarAt.end() && stamp < personLast->second + PLAYERBOT_GUILD_WAR_PLAYER_REST_SECONDS)
			snprintf(why, sizeof(why), "wasza gildia walczyla z botami niedawno, sprobujcie za %u min",
					(personLast->second + PLAYERBOT_GUILD_WAR_PLAYER_REST_SECONDS - stamp + 59) / 60);
		else if (!GetPlayerBotWarSides(battlefield, empire))
			snprintf(why, sizeof(why), "na mapie gildyjnej nie ma gdzie walczyc");

		char chat[320];
		if (why[0])
		{
			bots->RequestRefuseWar(offer.dwFrom);
			snprintf(chat, sizeof(chat), "[Wojna] Gildia botow %s odmawia: %s.", bots->GetName(), why);
			person->Chat(chat);
			sys_log(0, "PLAYERBOT_GUILD: player war refused %s -> %s empire=%d online=%d why=%s",
					person->GetName(), bots->GetName(), (int)empire, online, why);
			return;
		}

		bots->RequestDeclareWar(offer.dwFrom, GUILD_WAR_TYPE_FIELD);
		TPlayerBotGuildWar war;
		war.dwGuild1 = offer.dwFrom;
		war.dwGuild2 = offer.dwTo;
		war.dwDeclaredAt = dwNow;
		war.dwStartedAt = 0;
		war.bStarted = false;
		war.bPlayerWar = true;
		s_mapPlayerBotGuildWars[empire] = war;
		s_mapPlayerBotGuildLastWarAt[offer.dwTo] = stamp;
		s_mapPlayerBotPlayerGuildLastWarAt[offer.dwFrom] = stamp;
		DBManager::instance().Query("UPDATE player.playerbot_guild SET last_war_at=%u WHERE guild_id=%u",
				stamp, offer.dwTo);
		snprintf(chat, sizeof(chat), "[Wojna] Gildia botow %s przyjmuje wyzwanie! Pole bitwy: mapa gildyjna (%s), kanal 1. Boty buffuja sie %u s przy swoim obozie i ruszaja na srodek.",
				bots->GetName(), GetPlayerBotKingdomName(empire), (unsigned int)PLAYERBOT_GUILD_WAR_MUSTER_SECONDS);
		person->Chat(chat);
		sys_log(0, "PLAYERBOT_GUILD: player war accepted %s -> %s empire=%d online=%d",
				person->GetName(), bots->GetName(), (int)empire, online);
	}

	void ProcessPlayerBotWarOffers(DWORD dwNow)
	{
		for (size_t i = 0; i < s_vecPlayerBotWarOffers.size(); )
		{
			if (dwNow - s_vecPlayerBotWarOffers[i].dwAt < PLAYERBOT_GUILD_WAR_OFFER_THINK_MS)
			{
				++i;
				continue;
			}
			const TPlayerBotWarOffer offer = s_vecPlayerBotWarOffers[i];
			s_vecPlayerBotWarOffers.erase(s_vecPlayerBotWarOffers.begin() + i);
			AnswerPlayerBotWarOffer(offer, dwNow);
		}
	}

	// Once a minute for the world: the war in progress moved along, or the
	// next one declared when its time has come. A declaration is a round trip
	// through the db core - the other master accepts on a later minute, once
	// its guild reports GUILD_WAR_RECV_DECLARE - and a war the db core has
	// ended is noticed by UnderWar going false. A player's declaration is
	// answered within seconds, ahead of the minute.
	void ManagePlayerBotGuildWars(DWORD dwNow)
	{
		// A war is declared once for the world, by the first channel; the bots
		// of a guild at war fight it on whichever channel they live on.
		if (g_bChannel != 1)
		{
			s_vecPlayerBotWarOffers.clear();
			return;
		}
		if (!s_bPlayerBotGuildWarMemoryLoaded)
			LoadPlayerBotGuildWarMemory();
		if (!s_vecPlayerBotWarOffers.empty())
			ProcessPlayerBotWarOffers(dwNow);
		if (s_dwNextPlayerBotGuildWarCheck != 0 && dwNow < s_dwNextPlayerBotGuildWarCheck)
			return;
		s_dwNextPlayerBotGuildWarCheck = dwNow + PLAYERBOT_GUILD_WAR_CHECK_INTERVAL;
		const bool enabled = IsPlayerBotGuildWarsEnabled();

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
						if (war.bPlayerWar)
							snprintf(notice, sizeof(notice), "Wojna gildii: %s kontra gildia botow %s! Pole bitwy: mapa gildyjna (%s), 30 minut.",
									g1->GetName(), g2->GetName(), GetPlayerBotKingdomName((BYTE)empire));
						else
							snprintf(notice, sizeof(notice), "Wojna gildii: %s kontra %s! Pole bitwy: mapa gildyjna (%s), 30 minut.",
									g1->GetName(), g2->GetName(), GetPlayerBotKingdomName((BYTE)empire));
						BroadcastNotice(notice);
						sys_log(0, "PLAYERBOT_GUILD: war on %s vs %s empire=%d battlefield=%ld online=%d/%d player=%d",
								g1->GetName(), g2->GetName(), empire, battlefield,
								CountPlayerBotGuildOnline(g1), CountPlayerBotGuildOnline(g2), (int)war.bPlayerWar);
					}
					// A declaration the switch finds pending is left to run out
					// (PLAYERBOT_GUILD_WAR_DECLARE_TIMEOUT) rather than accepted.
					// A player's war was accepted the moment it was answered.
					else if (!war.bPlayerWar && enabled && g2->GetGuildWarState(g1->GetID()) == GUILD_WAR_RECV_DECLARE)
					{
						g2->RequestDeclareWar(g1->GetID(), GUILD_WAR_TYPE_FIELD);
						sys_log(0, "PLAYERBOT_GUILD: war accepted by %s from %s", g2->GetName(), g1->GetName());
					}
					else if (dwNow - war.dwDeclaredAt > PLAYERBOT_GUILD_WAR_DECLARE_TIMEOUT)
					{
						sys_log(0, "PLAYERBOT_GUILD: war declaration went nowhere %s -> %s (state=%d player=%d), dropped",
								g1->GetName(), g2->GetName(), g2->GetGuildWarState(g1->GetID()), (int)war.bPlayerWar);
						if (!war.bPlayerWar)
							s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_RETRY_MS;
						s_mapPlayerBotGuildWars.erase(it);
					}
					continue;
				}
				if (!g1->UnderWar(g2->GetID()))
				{
					sys_log(0, "PLAYERBOT_GUILD: war over %s vs %s after %u min player=%d (wins/draws/losses %d/%d/%d and %d/%d/%d, ladder %d and %d)",
							g1->GetName(), g2->GetName(), (unsigned int)((dwNow - war.dwStartedAt) / 60000U), (int)war.bPlayerWar,
							g1->GetGuildWarWinCount(), g1->GetGuildWarDrawCount(), g1->GetGuildWarLossCount(),
							g2->GetGuildWarWinCount(), g2->GetGuildWarDrawCount(), g2->GetGuildWarLossCount(),
							g1->GetLadderPoint(), g2->GetLadderPoint());
					// A player's war takes the kingdom's battlefield out of the
					// bots' rotation for its half hour, and puts the next bot war
					// off no more than AFTER_PLAYER_WAR_MS past its end.
					if (war.bPlayerWar)
						s_adwPlayerBotNextGuildWarTime[empire] = std::max(s_adwPlayerBotNextGuildWarTime[empire],
								dwNow + PLAYERBOT_GUILD_WAR_AFTER_PLAYER_WAR_MS);
					else
						s_adwPlayerBotNextGuildWarTime[empire] = dwNow + PLAYERBOT_GUILD_WAR_INTERVAL;
					s_mapPlayerBotGuildWars.erase(it);
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
				s_abPlayerBotGuildWarNoPair[empire] = true;
				continue;
			}
			s_abPlayerBotGuildWarNoPair[empire] = false;
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
			war.bPlayerWar = false;
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
	// the map is not hosted here, the clock has not been set yet, or the last
	// look found no pair and the clock is only its retry).
	int GetPlayerBotNextGuildWarInSeconds(BYTE empire, DWORD dwNow)
	{
		if (empire >= playerbot_empire_rules::EMPIRE_COUNT)
			return -1;
		if (s_mapPlayerBotGuildWars.find(empire) != s_mapPlayerBotGuildWars.end())
			return 0;
		if (!IsPlayerBotGuildWarsEnabled() || s_adwPlayerBotNextGuildWarTime[empire] == 0 ||
				s_abPlayerBotGuildWarNoPair[empire])
			return -1;
		const DWORD at = s_adwPlayerBotNextGuildWarTime[empire];
		return dwNow >= at ? 0 : (int)((at - dwNow) / 1000U);
	}

	// ------------------------------------------------------------- the fight

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
	// moment it rose. And a bot at its camp in the grace after that is left
	// to buff (PLAYERBOT_GUILD_WAR_CAMP_GRACE_MS).
	bool IsPlayerBotWarFoeRecovering(LPCHARACTER other)
	{
		if (other->IsAffectFlag(AFF_REVIVE_INVISIBLE))
			return true;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(other->GetPlayerID());
		return it != s_mapPlayerBotAIStates.end() &&
				(it->second.bRecoveringAfterDeath || it->second.dwGuildWarCampUntil > get_dword_time());
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

	// The people of a player's guild on this core, looked for every
	// PLAYERBOT_GUILD_WAR_HUMANS_REFRESH_MS: the roster below is the bots', and
	// a person is on it only as a character the engine holds.
	struct TPlayerBotWarHumans
	{
		DWORD dwRefreshedAt;
		std::vector<DWORD> pids;
	};
	std::map<DWORD, TPlayerBotWarHumans> s_mapPlayerBotWarHumans;

	const std::vector<DWORD>& GetPlayerBotWarHumans(CGuild* enemy, DWORD dwNow)
	{
		TPlayerBotWarHumans& humans = s_mapPlayerBotWarHumans[enemy->GetID()];
		if (humans.dwRefreshedAt == 0 || dwNow - humans.dwRefreshedAt >= PLAYERBOT_GUILD_WAR_HUMANS_REFRESH_MS)
		{
			humans.dwRefreshedAt = dwNow;
			humans.pids.clear();
			const CHARACTER_MANAGER::NAME_MAP& pcs = CHARACTER_MANAGER::instance().GetPCMap();
			for (CHARACTER_MANAGER::NAME_MAP::const_iterator it = pcs.begin(); it != pcs.end(); ++it)
			{
				LPCHARACTER pc = it->second;
				if (pc && pc->GetDesc() && !pc->GetDesc()->IsBot() && pc->GetGuild() == enemy)
					humans.pids.push_back(pc->GetPlayerID());
			}
		}
		return humans.pids;
	}

	// Whether this enemy may be chosen now: alive on the bot's map, where a
	// blow lands, on the field, up from its last death and out of its grace.
	bool IsPlayerBotWarFoeUp(LPCHARACTER ch, LPCHARACTER other)
	{
		return other && other != ch && !other->IsDead() && other->GetMapIndex() == ch->GetMapIndex() &&
				IsPlayerBotWarTargetable(other) && !IsPlayerBotWarFoeRecovering(other) &&
				IsPlayerBotOnWarField(other->GetMapIndex(), other->GetX(), other->GetY());
	}

	// A foe of the enemy guild - a bot, or a person of a player's guild - on
	// the bot's map that a blow can reach, near and not already everybody's
	// (PLAYERBOT_GUILD_WAR_CROWD_PENALTY and its neighbours), and within
	// maxDistance of the bot when that is given (the muster defends its camp
	// and charges nobody). One standing in the safe zone was chosen like any
	// other, so its enemies walked in after it and swung at nothing for as
	// long as it stood there: "sporo stalo w bezpiecznej czesci i inne boty
	// nie mogly ich zaatakowac" (gregory_955, 17 September). The one pass over
	// the roster both finds the enemies and counts the chooser's own side on
	// each of them.
	LPCHARACTER FindPlayerBotGuildWarFoe(LPCHARACTER ch, CGuild* mine, CGuild* enemy, DWORD heldVID,
			long maxDistance, DWORD dwNow)
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
			if (guild != enemy || !IsPlayerBotWarFoeUp(ch, other))
				continue;
			const int distance = DISTANCE_APPROX(ch->GetX() - other->GetX(), ch->GetY() - other->GetY());
			if (maxDistance > 0 && distance > maxDistance)
				continue;
			foes.push_back(std::make_pair(other, distance));
		}
		// A player's guild: its people too, not only its bots.
		if (!IsPlayerBotGuild(enemy))
		{
			const std::vector<DWORD>& humans = GetPlayerBotWarHumans(enemy, dwNow);
			for (size_t i = 0; i < humans.size(); ++i)
			{
				LPCHARACTER other = CHARACTER_MANAGER::instance().FindByPID(humans[i]);
				if (!IsPlayerBotWarFoeUp(ch, other) || other->GetGuild() != enemy)
					continue;
				const int distance = DISTANCE_APPROX(ch->GetX() - other->GetX(), ch->GetY() - other->GetY());
				if (maxDistance > 0 && distance > maxDistance)
					continue;
				foes.push_back(std::make_pair(other, distance));
			}
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
			PlayerBotLogThrottled("guild_war_spread", dwNow,
					"PLAYERBOT_GUILD: war spread guild=%s enemies_up=%u attacking=%d targets=%u busiest=%d",
					mine->GetName(), (unsigned int)foes.size(), attacking,
					(unsigned int)attackers.size(), busiest);
		}
		return best;
	}

	// When each bot at war looks at its foe again, by pid.
	std::map<DWORD, DWORD> s_mapPlayerBotWarRetargetAt;

	// A bot put back at its camp after a death: the same map, so no warp -
	// the rest of TransitionPlayerBotMap (the party, the stall, the travel
	// clocks) belongs to a real map change.
	bool PlacePlayerBotAtWarCamp(LPCHARACTER ch, TPlayerBotAIState& state, long x, long y, DWORD dwNow)
	{
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
		if (!ch->Show(ch->GetMapIndex(), x, y, 0))
			return false;
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
		return true;
	}

	// A bot's part in its guild's war. Claims the tick for the war's whole
	// half hour: the walk to the battlefield, the muster at the camp, the
	// fight in the middle, the stand-up at the camp after every death; and the
	// way home afterwards. A bot in a player's party stays with the player.
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
				state.dwGuildWarCampUntil = 0;
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
		const int side = mine->GetID() < enemy->GetID() ? 0 : 1;
		long campX = 0, campY = 0, middleX = 0, middleY = 0;
		if (!GetPlayerBotWarCamp(battlefield, empire, side, pid, campX, campY) ||
				!GetPlayerBotWarMiddle(battlefield, empire, pid, middleX, middleY))
			return false;
		const bool mustering = IsPlayerBotWarMustering(mine, enemy);

		if (state.dwGuildWarEnemyGID != enemy->GetID())
		{
			state.dwGuildWarEnemyGID = enemy->GetID();
			PlayerBotLogThrottled("guild_war_to", dwNow,
					"PLAYERBOT_GUILD: to war pid=%u name=%s guild=%s enemy=%s map=%ld camp=(%ld,%ld) middle=(%ld,%ld) muster=%d",
					ch->GetPlayerID(), ch->GetName(), mine->GetName(), enemy->GetName(), ch->GetMapIndex(),
					campX, campY, middleX, middleY, (int)mustering);
		}
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);

		// Onto the battlefield at the bot's own camp, whatever phase the war is
		// in: a late arrival runs to the middle from there like the rest.
		if (ch->GetMapIndex() != battlefield)
		{
			if (dwNow < state.dwNextGuildWarMoveTime)
				return true;
			state.dwNextGuildWarMoveTime = dwNow + 5000;
			TransitionPlayerBotMap(ch, state, battlefield, campX, campY, dwNow, "guild_war");
			return true;
		}
		// A bot that fell stands up at its own camp, not among the enemies
		// that killed it, and heals there out of sight; the walk away from the
		// spot of its death is for a hunting map.
		if (state.bRecoveringAfterDeath)
		{
			if (DISTANCE_APPROX(ch->GetX() - campX, ch->GetY() - campY) > PLAYERBOT_GUILD_WAR_CAMP_RADIUS &&
					PlacePlayerBotAtWarCamp(ch, state, campX, campY, dwNow))
				PlayerBotLogThrottled("guild_war_camp", dwNow,
						"PLAYERBOT_GUILD: up at the camp pid=%u name=%s guild=%s camp=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), mine->GetName(), campX, campY);
			state.lDeathX = 0;
			state.lDeathY = 0;
			state.dwGuildWarCampUntil = dwNow + PLAYERBOT_GUILD_WAR_CAMP_GRACE_MS;
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

		const bool atCamp = DISTANCE_APPROX(ch->GetX() - campX, ch->GetY() - campY) <= PLAYERBOT_GUILD_WAR_CAMP_RADIUS;
		// The grace ends the moment the bot steps out of its camp: it is for
		// buffing, not for walking into the fight untouchable.
		if (!atCamp)
			state.dwGuildWarCampUntil = 0;

		// Off the field - chased up a slope, arrived at the map's edge - the bot
		// walks back before it looks for anybody (IsPlayerBotOnWarField): to its
		// camp while the muster lasts, to the middle after.
		if (!IsPlayerBotOnWarField(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (dwNow >= state.dwNextGuildWarMoveTime)
			{
				state.dwNextGuildWarMoveTime = dwNow + 2000;
				MovePlayerBot(ch, mustering ? campX : middleX, mustering ? campY : middleY, dwNow, 8, true, false);
			}
			return true;
		}

		// The whole set of buffs at the camp - while the muster lasts, and
		// after every stand-up - before the run to the middle: "pare sekund na
		// zbuffowanie sie i dopiero wtedy ogien" (prodnathin, 24 September).
		if (atCamp && ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
			return true;

		// The foe in hand is kept while it stands on the field, and looked at
		// again every PLAYERBOT_GUILD_WAR_RETARGET_MS: the search is every bot
		// in the world, so not on every tick, but often enough for a bot to
		// turn to the enemy who came up beside it and for the crowd on one
		// enemy to thin out. While the muster lasts only a foe who has come up
		// to the camp is fought - and where the map left room for camps only
		// close together, a foe standing at his own camp has not.
		long reach = 0;
		if (mustering)
		{
			reach = PLAYERBOT_GUILD_WAR_CAMP_DEFEND_RANGE;
			const TPlayerBotWarSide* sides = GetPlayerBotWarSides(battlefield, empire);
			if (sides && sides->bCamps)
			{
				const long half = DISTANCE_APPROX(sides->campX[0] - sides->campX[1], sides->campY[0] - sides->campY[1]) / 2;
				reach = std::min(reach, std::max(200L, half - 200));
			}
		}
		LPCHARACTER foe = NULL;
		if (state.dwTargetVID != 0)
		{
			LPCHARACTER held = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (held && held->GetGuild() == enemy && IsPlayerBotWarFoeUp(ch, held) &&
					(reach == 0 || DISTANCE_APPROX(ch->GetX() - held->GetX(), ch->GetY() - held->GetY()) <= reach))
				foe = held;
		}
		DWORD& retargetAt = s_mapPlayerBotWarRetargetAt[pid];
		if (!foe || dwNow >= retargetAt)
		{
			retargetAt = dwNow + PLAYERBOT_GUILD_WAR_RETARGET_MS;
			LPCHARACTER chosen = FindPlayerBotGuildWarFoe(ch, mine, enemy, foe ? (DWORD)foe->GetVID() : 0, reach, dwNow);
			if (chosen)
				foe = chosen;
		}
		if (!foe)
		{
			state.dwTargetVID = 0;
			// Nobody to fight: the camp while the muster lasts, facing the
			// middle; the middle after it, where the enemy comes.
			const long spotX = mustering ? campX : middleX;
			const long spotY = mustering ? campY : middleY;
			if (DISTANCE_APPROX(ch->GetX() - spotX, ch->GetY() - spotY) > (mustering ? 200 : 600))
			{
				if (dwNow >= state.dwNextGuildWarMoveTime)
				{
					state.dwNextGuildWarMoveTime = dwNow + 3000;
					MovePlayerBot(ch, spotX, spotY, dwNow, 8, true, false);
				}
			}
			else if (mustering)
			{
				if (ch->IsStateMove())
					ch->Stop();
				ch->SetRotationToXY(middleX, middleY);
			}
			return true;
		}

		const int distance = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		state.dwTargetVID = (DWORD)foe->GetVID();
		ch->SetVictim(foe);
		ch->SetRotationToXY(foe->GetX(), foe->GetY());
		// A bot that has picked a foe is in the fight, grace or no grace: the
		// buffs above had the tick for as long as one was missing.
		state.dwGuildWarCampUntil = 0;

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
				// Out of the safe zone by way of the middle, which is open,
				// fightable ground by construction (FindPlayerBotWarGround):
				// a walk at a foe a step away would count as arrived at once
				// and leave the bot standing where it cannot strike.
				if (inSafeZone)
					MovePlayerBot(ch, middleX, middleY, dwNow, 4, false, false);
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
