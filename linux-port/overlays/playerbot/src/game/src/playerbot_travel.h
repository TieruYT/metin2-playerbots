#ifndef __INC_METIN2_PLAYERBOT_TRAVEL_H__
#define __INC_METIN2_PLAYERBOT_TRAVEL_H__

// Where a bot ought to be, and how it gets there across maps.
//
// Two things live here that look separate and are not. The first is what a bot
// still needs - potions, a weapon, a repair - because that is what decides
// whether it may leave for a hunting ground at all. The second is the crossing
// itself: which frontier suits its level, when a trip is worth making, and the
// difference between a warp and a walk to a portal.
//
// A server-side bot has no client to reconnect, so it cannot cross between game
// cores. Every route planned here has to stay on the cores that host its maps.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_movement.h - it decides the destination, that file walks to
// it - and after playerbot_economy.h, whose refine opportunities it consults.

namespace
{
	void CountPlayerBotPotions(LPCHARACTER ch, size_t& redCount, size_t& blueCount)
	{
		redCount = 0;
		blueCount = 0;
		if (!ch || !ch->IsItemLoaded())
			return;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051)
				redCount += item->GetCount();
			else if (vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				blueCount += item->GetCount();
		}
	}

	bool NeedsPlayerBotPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		if (ch->GetLevel() <= 10)
			return (redCount < 30 && ch->GetGold() >= 300) ||
					(isMage && blueCount < 20 && ch->GetGold() >= 400);
		// Only a belt that is nearly out is worth crossing a map for. A bot with
		// half its potions left has no business walking away from a good spot -
		// it will fill up anyway the next time something else brings it to town.
		return (redCount < PLAYERBOT_POTION_TRIP_RED && ch->GetGold() >= 1200) ||
				(blueCount < PLAYERBOT_POTION_TRIP_BLUE && ch->GetGold() >= 1200);
	}

	bool NeedsPlayerBotEmergencyPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		// Normal restocking happens at 50/30 (or 10) units. Cross-map travel is
		// justified only by a genuinely short combat reserve, not by one consumed pot.
		//
		// Blue potions restore SP, so an empty belt is an emergency for a caster
		// and nothing at all for a warrior. Treating "no blue potions" as critical
		// for everybody put 210 of 442 warriors and ninjas into a permanent fake
		// emergency: they were always considered one step from being unable to
		// fight, so they abandoned every trip the moment their items finished
		// loading and shuttled straight back to town.
		return redCount < 10 || (isMage && blueCount < 8);
	}

	bool NeedsPlayerBotCriticalTownServices(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// These problems can make continued combat impossible or waste most future
		// drops, so they justify an immediate cross-map return.
		if (ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				(PlayerBotWantsShield(ch) && ch->GetWear(WEAR_SHIELD) == NULL) ||
				ch->GetWear(WEAR_HEAD) == NULL ||
				ch->GetWear(WEAR_FOOTS) == NULL || NeedsPlayerBotEmergencyPotions(ch) ||
				NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0)
			return true;
		// The soft half - a bag at 45 percent - is what a keeper with goods
		// carries for good, and through the village branches of the world
		// travel it was a town visit every ten minutes for a bot whose errand
		// is a hundred kills on the desert: 126 trial bots in the villages,
		// three on the desert in twenty-five minutes, one of seventy in Joan
		// all day on town visit -> market -> party -> town visit (m2zip, 17
		// September). The trial is the errand; the bag waits for the horse.
		if (IsPlayerBotOnBattleHorseTrial(ch))
			return false;

		// Iwakura's Trader goes back to town at eighty percent: a break is for
		// what the game makes the bot do, and a bag at half is not that.
		if (IsPlayerBotPersonaEnabled())
			return IsPlayerBotBagFull(ch);

		size_t occupiedGridCells = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item)
				occupiedGridCells += std::max(1, (int)item->GetSize());
		}
		return occupiedGridCells * 100 >= PLAYERBOT_BAG_CELLS * 45;
	}

	// A missing weapon or body armour, an empty potion belt, no arrows or a full
	// inventory really do stop a bot from playing, and must outrank travelling.
	// A missing helmet, boots or shield only make it a little weaker. Treating
	// those as equally critical trapped a bot for good whenever the town could
	// not sell it the missing piece: it always wanted to shop, so it was never
	// allowed to travel, and it kept farming level-3 wolves in Joan at level 27.
	bool BlocksPlayerBotTravel(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		return ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				NeedsPlayerBotEmergencyPotions(ch) || NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0;
	}

	// Which kingdom's roads this bot is travelling on.
	//
	// Inside a kingdom it is the map's owner, because the gate in front of the
	// bot is the one it can actually walk to - a Jinno bot standing in Bokjung
	// leaves Bokjung by Bokjung's gate. Off the kingdom maps altogether - a
	// frontier, a dungeon, the guild ground - it is the bot's own empire,
	// because there "go home" can only mean its own home.
	int GetPlayerBotRoadsEmpire(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const int owner = playerbot_empire_rules::GetMapOwnerEmpire(ch->GetMapIndex());
		return owner != 0 ? owner : (int)ch->GetEmpire();
	}

	// A move between two roles of that kingdom: the gate to walk to, the map it
	// leads to, and the point the engine puts the character down on. Every leg
	// below used to be a pair of constants naming Chunjo's gate and Chunjo's
	// arrival, which is the whole reason a Shinsoo bot could reach Bokjung and
	// never find its way anywhere else.
	bool GetPlayerBotKingdomLeg(LPCHARACTER ch, playerbot_empire_rules::EMapRole from,
			playerbot_empire_rules::EMapRole to, long& gateX, long& gateY,
			long& destMap, long& destX, long& destY)
	{
		const int empire = GetPlayerBotRoadsEmpire(ch);
		const long fromMap = playerbot_empire_rules::GetHomeMap(empire, from);
		const long toMap = playerbot_empire_rules::GetHomeMap(empire, to);
		playerbot_empire_rules::TKingdomGate gate;
		if (fromMap == 0 || toMap == 0 ||
				!playerbot_empire_rules::FindKingdomGate(empire, fromMap, toMap, gate))
			return false;
		gateX = gate.gate.x;
		gateY = gate.gate.y;
		destMap = toMap;
		destX = gate.arrival.x;
		destY = gate.arrival.y;
		return true;
	}

	// Where a bot that has come back from a neutral map should stand: its own
	// kingdom's village square, which is what the return legs have always aimed
	// for. The pitch rather than the gate, because the errand that brought it
	// back is a town errand.
	bool GetPlayerBotVillageReturn(LPCHARACTER ch, playerbot_empire_rules::EMapRole role,
			long& destMap, long& destX, long& destY)
	{
		const int empire = GetPlayerBotRoadsEmpire(ch);
		const long map = playerbot_empire_rules::GetHomeMap(empire, role);
		playerbot_empire_rules::TPoint pitch;
		if (map == 0 || !playerbot_empire_rules::GetTownPitch(map, pitch))
			return false;
		destMap = map;
		destX = pitch.x;
		destY = pitch.y;
		return true;
	}

	// The Teleporter of the village the bot is standing in. Every long trip goes
	// through him and every village has one.
	bool GetPlayerBotLocalTeleporter(LPCHARACTER ch, long& outX, long& outY)
	{
		playerbot_empire_rules::TTownServices svc;
		if (!ch || !playerbot_empire_rules::GetTownServices(ch->GetMapIndex(), svc))
			return false;
		outX = svc.teleporter.x;
		outY = svc.teleporter.y;
		return true;
	}

	bool NeedsPlayerBotM1OnlyServices(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ShouldPlayerBotVisitProgressionMarket(ch, state, dwNow)) return true;
		if (!ch)
			return false;
		// Bokjung has no profession trainers, no Biologist and no old woman.
		// Everything else can be handled locally in M2, so only these real
		// activities justify M2 -> M1.
		if (ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN)
			return true;
		// Baek-Go stands in the first villages too, and a bot that already has
		// everything a row needs is the cheapest possible traveller: it knows
		// the recipe, the bag holds the herbs and the purse the fee, and the
		// only thing missing is the counter. Measured on 17 September, this is
		// why the board stayed idle - PiratTanaka and MordercaBezSerca3 each
		// stood in Bokjung with eleven Peach Blossoms and the recipe learnt,
		// and the visit only ever fires for a bot already standing in M1. The
		// gate is deliberately the whole shopping list, so this can never
		// become the crowd at the gates that 2.0.60 was.
		if (PlayerBotHasReadyCraftRow(ch))
			return true;
		// Her too: a skill stuck at seventeen with no points left to try
		// anything else is worth a trip to Joan while the character is still
		// young enough for her to serve it.
		if (ShouldPlayerBotResetSkills(ch, state, dwNow))
			return true;
		// Iwakura's Rybak: a bot back in a second village in a mood for the
		// water gives up the grind for the bank ("po powrocie do miasta
		// zrezygnuje z dalszego grindu"), and the banks are the first villages'.
		// Only from a village: a bot on the frontier goes home first.
		if (IsPlayerBotPersonaEnabled() && state.persona.bRestored &&
				IsPlayerBotVillageMap(ch->GetMapIndex()) && !IsPlayerBotM1Map(ch->GetMapIndex()) &&
				!state.bFishingSession && dwNow >= state.dwNextFishingCheckTime &&
				IsPlayerBotAngler(ch, state))
			return true;

		size_t missionIndex = 0;
		// One of the two owners of the errand queue: this is where a trip is
		// actually decided, so this is where a place may be taken.
		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch, &missionIndex, true);
		if (!mission)
			return false;
		// The hand-in: same threshold as the hand-in itself, or the trip would
		// never start for a bot the Biologist would happily serve.
		if (PlayerBotBiologistHoldsHandIn(ch, mission, missionIndex))
			return true;
		// The hunt for a first-village row: the six herb rows' monsters stand
		// in Joan and its two mirrors and nowhere else, so a bot anywhere else
		// with such a row open goes there for them - at seventy-eight as at
		// fifteen (Tieru, 16 September). The wander then picks the hubs for
		// the row's level (GetPlayerBotVillageHuntLevel).
		const DWORD huntMob = GetPlayerBotBiologistHuntMob(ch);
		return huntMob != 0 && huntMob < 500 && !IsPlayerBotM1Map(ch->GetMapIndex());
	}

	// Above this level Bokjung has nothing left to offer, so nothing there is
	// worth keeping a bot for either.
	const BYTE PLAYERBOT_M2_COHORT_MAX_LEVEL = 35;

	bool IsPlayerBotPastM2Ceiling(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL;
	}

	// May this bot start an ordinary fight where it is standing?
	//
	// Bokjung above the cohort ceiling is the one place where the answer is no.
	// A bot that has outgrown it may still be there as a customer, a traveller,
	// a trader or to finish a named errand - the combat policy keeps allowing a
	// quest target, a material it is genuinely short of and self-defence - but
	// experience is not a reason to be there, and "my ambition is Metins" is
	// not consent. Everywhere else this is true and nothing changes.
	// Defined below with the frontier draw it asks about.
	bool PlayerBotCoreHasAnyFrontier();

	bool IsPlayerBotGrindAllowedHere(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (!IsPlayerBotM2Map(ch->GetMapIndex()))
			return true;
		if (!IsPlayerBotPastM2Ceiling(ch))
			return true;
		// The ceiling says "you have outgrown this village, go to the frontier".
		// On a core that hosts no frontier there is nowhere to go, and refusing
		// the hunt as well would leave the whole kingdom standing in its own
		// second village with nothing it is allowed to do. Until the shared
		// maps are split between the cores, a kingdom without a frontier keeps
		// its village.
		if (!PlayerBotCoreHasAnyFrontier())
			return true;
		// Past the ceiling, one exception: the bot cannot pay the Teleporter
		// that would take it where it belongs. Refusing the hunt as well left
		// it asking the Teleporter every tick for ever ("Zbieram yang na
		// Teleporter" over a bot that could not gather any); it hunts here
		// until it holds a few fares and then goes.
		return GetPlayerBotFrontierMapForLevel(ch) != 0 &&
				ch->GetGold() < GetPlayerBotTeleporterFareEstimate(ch) *
					PLAYERBOT_TELEPORTER_FARE_RESERVE_COUNT;
	}

	bool IsPlayerBotM2LevelingCohort(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 20 ||
				ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL)
			return false;
		// Levels 20-21 still have a little useful M1 progression, so retain a small
		// stable minority there. At level 22 every ordinary leveler graduates to M2.
		return ch->GetLevel() >= 22 ||
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d325850U) % 10U) != 0;
	}

	bool ShouldPlayerBotLeaveRemoteMapForRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!HasPlayerBotPriorityRefineOpportunity(ch))
			return false;
		if (state.dwNextRemoteRefineReturnTime == 0)
		{
			const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^
					(dwNow / 60000U) ^ 0x52455455U) %
					(PLAYERBOT_REMOTE_REFINE_RETURN_MAX_DELAY -
					 PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + 1);
			state.dwNextRemoteRefineReturnTime = dwNow +
					PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + spread;
			return false;
		}
		return dwNow >= state.dwNextRemoteRefineReturnTime;
	}

	// Defined with the market-stall code in playerbot_town.h, which needs the
	// town and therefore comes later. Declared rather than included, the way
	// playerbot_gear.h declares GetPlayerBotNpcApproach.
	void ClosePlayerBotShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			const char* reason);

	bool IsPlayerBotFrontierMap(long mapIndex)
	{
		return IsPlayerBotFrontierMapIndex(mapIndex);
	}

	// Does this core host that map at all?
	//
	// One map lives on exactly one core, and a character cannot cross between
	// them: WarpSet tells a client to reconnect, and a bot has no client. Every
	// shared map in this world - the valley, the desert, Sohan, both Spider
	// Dungeons, Hwang, the two harder Monkey Dungeons - is hosted by the core
	// that also carries Chunjo, so a Shinsoo or Jinno bot asking for one is
	// asking for something that cannot happen. Left unchecked that is not a
	// quiet no: it is the shape this file has already been bitten by, ten
	// thousand refused warps a minute with one throttled line to show for it.
	bool IsPlayerBotMapHostedHere(long mapIndex)
	{
		return mapIndex != 0 && SECTREE_MANAGER::instance().GetMap(mapIndex) != NULL;
	}

	// Where this bot's kingdom enters a shared map, and where it leaves it.
	//
	// Every frontier has three entry points and three gates, one per kingdom:
	// the Town.txt pairs the engine itself spawns characters on, and the warp
	// NPC standing beside each. playerbot_empire_rules has held the entries
	// since the three-kingdom travel was written - the frontier tables in
	// playerbot_types.h hold one point per map, Chunjo's, and these two call
	// sites never asked the kingdom table at all. Every bot of every kingdom
	// therefore arrived through Chunjo's entrance and walked back to Chunjo's
	// gate, which from the far side of the valley is a crossing of the whole
	// map ("wszystkie boty po wejsciu do doliny, nie zaleznie od krolestwa z
	// ktorego sa, wchodza w miejscu wejscia zoltych. To samo sie dzieje z
	// pustynia", SIZOWSKI, 13 September).
	//
	// The shared point stays the answer where a map really has one entrance -
	// both Spider Dungeons and Hwang - and for a caller with no character.
	bool GetPlayerBotFrontierArrivalFor(LPCHARACTER ch, long mapIndex, long& outX, long& outY)
	{
		playerbot_empire_rules::ETeleportDestination where;
		playerbot_empire_rules::TPoint point;
		if (ch && playerbot_empire_rules::GetFrontierTeleportDestination(mapIndex, where) &&
				playerbot_empire_rules::GetTeleportArrival(
					GetPlayerBotRoadsEmpire(ch), where, point))
		{
			outX = point.x;
			outY = point.y;
			return true;
		}
		return GetPlayerBotFrontierArrival(mapIndex, outX, outY);
	}

	bool GetPlayerBotFrontierExitFor(LPCHARACTER ch, long mapIndex, long& outX, long& outY)
	{
		playerbot_empire_rules::TPoint gate;
		if (ch && playerbot_empire_rules::GetFrontierGate(
				GetPlayerBotRoadsEmpire(ch), mapIndex, gate))
		{
			outX = gate.x;
			outY = gate.y;
			return true;
		}
		return GetPlayerBotFrontierExit(mapIndex, outX, outY);
	}

	// The dungeon this bot earns medals in: its own kingdom's easy one, or the
	// shared harder pair once its level has outgrown the easy rooms. Zero when
	// there is none it can both reach and profit from.
	//
	// The level band alone cannot answer this, and answering it with a map was
	// the whole bug. The band named Chunjo's dungeon for everybody, so a
	// Shinsoo bot of eighteen walked through its own gate into map 5 - which
	// nothing in the overlay recognised as a dungeon - and one of thirty-three
	// was sent at 108, a map its core does not host, so the warp was refused
	// every time it was asked for. Measured on our own world before the fix:
	// Shinsoo 500 characters and not one horse, Jinno 500 and not one, Chunjo
	// the only kingdom levelling any ("tylko boty z chunjo leveluja konia",
	// RetroGracz38; "bo z innych nie wchodza do lochu dlatego", NerrVoVy).
	long GetPlayerBotMonkeyMapFor(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const EPlayerBotMonkeyBand band = GetPlayerBotMonkeyBandForLevel(ch->GetLevel());
		if (band == PLAYERBOT_MONKEY_BAND_NONE)
			return 0;
		const long own = playerbot_empire_rules::GetMonkeyEasyMap(GetPlayerBotRoadsEmpire(ch));
		if (band == PLAYERBOT_MONKEY_BAND_EASY)
			return IsPlayerBotMapHostedHere(own) ? own : 0;
		const long shared = band == PLAYERBOT_MONKEY_BAND_MEDIUM
				? PLAYERBOT_MAP_MONKEY_MEDIUM : PLAYERBOT_MAP_MONKEY_HARD;
		if (IsPlayerBotMapHostedHere(shared))
			return shared;
		// The harder two are on the core that carries Chunjo, so under the
		// default split layout the other two kingdoms have only their own
		// rooms - see PLAYERBOT_MONKEY_EASY_FALLBACK_MAX_LEVEL for how long
		// those are still worth the trip.
		if (ch->GetLevel() <= PLAYERBOT_MONKEY_EASY_FALLBACK_MAX_LEVEL &&
				IsPlayerBotMapHostedHere(own))
			return own;
		return 0;
	}

	long GetPlayerBotFrontierMapForLevelRaw(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		// A bot working on its battle horse hunts where the trial is, whatever
		// its level would otherwise say. By level 36 it would be off to Orc
		// Valley, and the Black Wind band it needs lives in the desert.
		if (IsPlayerBotOnBattleHorseTrial(ch))
			return PLAYERBOT_MAP_DESERT;
		// The Biologist's row is done where its monster stands, whatever the
		// level says: the Orc Tooth and the Curse Book in the valley, the Demon
		// Souvenir in the tower. A row is finished before the next is begun,
		// at any level ("nie ma czegos takiego jak za niskie dla bota", Tieru,
		// 16 September); the specimen comes from the quest's own kill hook,
		// which asks nothing about the level gap.
		{
			const long rowHome = GetPlayerBotHuntingMobHome(GetPlayerBotBiologistHuntMob(ch, true));
			if (rowHome != 0 && IsPlayerBotFrontierMapIndex(rowHome) && IsPlayerBotMapHostedHere(rowHome))
				return rowHome;
		}
		// And the military trial is in the Demon Tower, for the same reason: the
		// bot hunts where the trial is, whatever its level would otherwise say.
		if (IsPlayerBotOnMilitaryHorseTrial(ch))
			return PLAYERBOT_MAP_DEMON_TOWER;

		const BYTE level = ch->GetLevel();
		const DWORD draw = PlayerBotNavHash(ch->GetPlayerID() ^ 0x45534f54U);
		// Forty-eight and up: the Spider Dungeon for half, Mount Sohan for the
		// other half, decided once per character so the answer does not change
		// under a bot halfway there. The valley is for those still short of it.
		// Fifty-two and up has a third frontier: the Hwang Temple, whose Elite
		// Esoterics start exactly there and whose turtles and bogeys run to
		// fifty-eight. It is not more experience than Sohan or the Spider
		// Dungeon - it is the only hosted map that drops the Frog Tongue, the
		// Leaf, the Unknown Talisman+ and the Curse Book+, which eighteen refine
		// recipes want and no counter in this world has ever carried.
		// A Metin hunter by role never draws the Spider Dungeon: it has no
		// stones (PlayerBotMapHasMetinStones), and the draw is for life, so
		// that hunter would have spent its whole career looking for stones on
		// the one frontier map that spawns none. Sohan takes its share; the
		// expedition roll in playerbot_planner.h makes the same check for the
		// half-hour hunters, whose draw stands.
		TPlayerBotAIStateMap::const_iterator role = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const bool stoneHunter = role != s_mapPlayerBotAIStates.end() &&
				role->second.bBotRole == BOT_ROLE_METIN_HUNTER;
		// Fifty-four and up: the second Spider Dungeon takes the draw the
		// first one had - spiders of sixty to sixty-eight that never attack
		// first, against V1's fifty to fifty-eight - and a stone hunter
		// still goes to Sohan, because neither dungeon has a stone.
		// Seventy-one and up: the Red Forest, whose 2311-2315 run 74 to 82 and
		// are the highest ordinary ground this world hosts, with the Forest
		// below it as the other half of the draw. Neither carries a stone.txt,
		// so a Metin hunter by role keeps Sohan - the same rule the Spider
		// Dungeon already lives under.
		// Sixty-six to eighty: Doyyumhwaji takes a third of the draw, by a
		// hash of its own so that nobody else's answer moves. Its 69 to 72 lie
		// between the Forest's and the Red Forest's, on a map richer than both
		// together, and it keeps its stones, so a Metin hunter may go too.
		// Past eighty its monsters are too far under the bot to pay.
		if (level >= PLAYERBOT_FIRE_LAND_MIN_LEVEL && level <= PLAYERBOT_FIRE_LAND_MAX_LEVEL &&
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x464c414dU) % 3U == 0)
			return PLAYERBOT_MAP_FIRE_LAND;
		if (level >= PLAYERBOT_RED_FOREST_MIN_LEVEL)
		{
			switch (draw % 3U)
			{
				case 0: return stoneHunter ? PLAYERBOT_MAP_SOHAN : PLAYERBOT_MAP_RED_FOREST;
				case 1: return stoneHunter ? PLAYERBOT_MAP_SOHAN : PLAYERBOT_MAP_FOREST;
				default: return PLAYERBOT_MAP_HWANG;
			}
		}
		// Sixty-two and up: the Forest, 65 to 71.
		if (level >= PLAYERBOT_FOREST_MIN_LEVEL)
		{
			switch (draw % 3U)
			{
				case 0: return stoneHunter ? PLAYERBOT_MAP_SOHAN : PLAYERBOT_MAP_FOREST;
				case 1: return PLAYERBOT_MAP_SOHAN;
				default: return PLAYERBOT_MAP_HWANG;
			}
		}
		// The Demon Tower takes one draw in four from fifty-seven up, and no
		// more than that: it is where the Biologist's last specimen lives, not
		// a place to move into. It has a stone (8015) and
		// PlayerBotMapHasMetinStones says no anyway, so nobody is sent there to
		// break one - the operator asked that the dungeon stay unrun until it is
		// worked out properly.
		if (level >= PLAYERBOT_DEMON_TOWER_MIN_LEVEL && (draw % 4U) == 0 && !stoneHunter)
			return PLAYERBOT_MAP_DEMON_TOWER;
		if (level >= PLAYERBOT_SPIDER_V2_MIN_LEVEL)
		{
			switch (draw % 3U)
			{
				case 0: return stoneHunter ? PLAYERBOT_MAP_SOHAN : PLAYERBOT_MAP_SPIDER_V2;
				case 1: return PLAYERBOT_MAP_SOHAN;
				default: return PLAYERBOT_MAP_HWANG;
			}
		}
		if (level >= PLAYERBOT_HWANG_MIN_LEVEL)
		{
			switch (draw % 3U)
			{
				case 0: return stoneHunter ? PLAYERBOT_MAP_SOHAN : PLAYERBOT_MAP_SPIDER_V1;
				case 1: return PLAYERBOT_MAP_SOHAN;
				default: return PLAYERBOT_MAP_HWANG;
			}
		}
		if (level >= PLAYERBOT_SPIDER_MIN_LEVEL && level >= PLAYERBOT_SOHAN_MIN_LEVEL)
			return (draw & 1U) != 0 && !stoneHunter ? PLAYERBOT_MAP_SPIDER_V1 : PLAYERBOT_MAP_SOHAN;
		// Thirty-six to forty-seven: the valley and the desert share them, the
		// same way thirty to thirty-five already do. See
		// PLAYERBOT_DESERT_MAX_LEVEL for what was sitting unused.
		if (level >= PLAYERBOT_ORC_VALLEY_MIN_LEVEL && level <= PLAYERBOT_DESERT_MAX_LEVEL)
			return (draw & 1U) != 0 ? PLAYERBOT_MAP_ORC_VALLEY : PLAYERBOT_MAP_DESERT;
		if (level >= PLAYERBOT_ORC_VALLEY_MIN_LEVEL && level <= PLAYERBOT_ORC_VALLEY_MAX_LEVEL)
			return PLAYERBOT_MAP_ORC_VALLEY;
		// Thirty to thirty-five: the Fanatic islands and the desert share the
		// population. Below thirty Bokjung keeps everyone.
		if (level >= PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL && level < PLAYERBOT_ORC_VALLEY_MIN_LEVEL)
			return (draw & 1U) != 0 ? PLAYERBOT_MAP_ORC_VALLEY : PLAYERBOT_MAP_DESERT;
		return 0;
	}

	// The map whose ordinary spawns still sit inside this bot's useful level
	// window, or 0 when its own village is still the right place for it.
	long GetPlayerBotFrontierMapForLevel(LPCHARACTER ch)
	{
		const long map = GetPlayerBotFrontierMapForLevelRaw(ch);
		return IsPlayerBotMapHostedHere(map) ? map : 0;
	}

	// A kingdom whose core hosts no frontier at all. Its bots have four maps and
	// nothing beyond them, which changes what the ceiling in its second village
	// is allowed to mean - see IsPlayerBotGrindAllowedHere.
	bool PlayerBotCoreHasAnyFrontier()
	{
		// Only the frontiers that take a bot from the second village's ceiling
		// on. The forests and Doyyumhwaji are for sixty-two and up, so a core
		// hosting nothing else - on the r40250 line the forests stand on
		// Shinsoo's core and Doyyumhwaji on Jinno's - still has nowhere for a
		// bot of thirty-six, and counting them here would wedge it there.
		static const long candidates[] = {
			PLAYERBOT_MAP_ORC_VALLEY, PLAYERBOT_MAP_DESERT, PLAYERBOT_MAP_SOHAN,
			PLAYERBOT_MAP_SPIDER_V1, PLAYERBOT_MAP_SPIDER_V2, PLAYERBOT_MAP_HWANG
		};
		for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
			if (IsPlayerBotMapHostedHere(candidates[i]))
				return true;
		return false;
	}

	// How far from town a personality is willing to play, in eighths. Personality
	// used to decide only how far a bot would push a refine, so every character
	// hunted in the same places; this is what makes the trait visible in-world.
	BYTE GetPlayerBotFrontierAppetite(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return 7; // explorer: almost always out on the far maps
			case BOT_PERSONALITY_METIN_BREAKER:
				return 6; // both frontier maps carry their own Metin spawns
			case BOT_PERSONALITY_GEAR_SPECIALIST:
				return 6; // Orc Valley is where the level-30 weapons drop
			case BOT_PERSONALITY_TEAM_COMPANION:
				return 4; // stays near the party pool at least half the time
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return 2; // protects what it has earned, keeps a town run short
			default:
				return 5; // steady adventurer
		}
	}

	// Due for a fishing trip: old enough for the rod, and its cooldown is up.
	bool WantsPlayerBotFishingTrip(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!IsPlayerBotAngler(ch, state))
			return false;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// CHARACTER::fishing() here wants a level, the fishing pass (unique
		// item 27620) worn and water in front of the rod. The level is asked
		// here so a bot under it never walks to the water; the pass is bought
		// and worn on the spot (EnsurePlayerBotFishingPass), because nothing
		// sells one and refusing without it meant no bot ever fished here.
		if (ch->GetLevel() < PLAYERBOT_FISHING_MIN_LEVEL ||
				!EnsurePlayerBotFishingPass(ch, dwNow))
			return false;
#endif
		// A trip to a village with no measured bank is a walk to nowhere: the
		// fishing pass would refuse on arrival and the bot would stand there.
		if (GetPlayerBotFishingBank(playerbot_empire_rules::GetHomeMap(
					(int)ch->GetEmpire(), playerbot_empire_rules::MAP_ROLE_M1)) == NULL)
			return false;
		return state.dwNextFishingCheckTime == 0 || dwNow >= state.dwNextFishingCheckTime;
	}

	bool ShouldPlayerBotLeaveForFrontier(LPCHARACTER ch)
	{
		if (!ch || GetPlayerBotFrontierMapForLevel(ch) == 0)
			return false;
		TPlayerBotAIStateMap::const_iterator it =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = it != s_mapPlayerBotAIStates.end()
				? it->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// The medal dropper's ground is its kingdom's Monkey Dungeon, never the
		// frontier: three medal droppers of level thirty-three were seen riding
		// for the Yongbi Desert with "poziom" over their heads, levels their
		// lock gives them none of.
		if (personality == BOT_PERSONALITY_MEDAL_DROPPER)
			return false;
		// Above the Bokjung ceiling there is nothing there left to stay behind
		// for, so the reserve rule has nothing to protect and only strands bots.
		if (IsPlayerBotPastM2Ceiling(ch))
			return true;
		// Below it Bokjung must never empty out completely: even the keenest
		// explorer leaves one bot in eight behind for the town, its Bestials
		// and the local party pool.
		const DWORD appetite = GetPlayerBotFrontierAppetite(personality);
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x46524f4eU) % 8U) < appetite;
	}

	// A wanderer settles in for a long session; a careful collector treats the
	// trip as an errand and heads back while it still has potions left.
	DWORD GetPlayerBotFrontierVisitTime(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME * 2;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME / 2;
			default:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME;
		}
	}

	// The M3 dropper is on the guild map for the level-30 weapons it will
	// sell, so a weapon in its bag is goods and never the reason to leave;
	// it farms while the exp lock's band still holds it. Both the door
	// (ShouldPlayerBotVisitM3) and the exit (the M3 branch of the world
	// travel) ask this, because they used to disagree: the door sent a
	// dropper by level alone and the exit sent any bot holding a weapon
	// home, and the Teleporter's arrival stands beside the return gate,
	// so four droppers of seban latino's world crossed M2 <-> M3 every
	// five seconds for as long as the log runs (16 September).
	bool IsPlayerBotM3DropperOnFarm(LPCHARACTER ch)
	{
		return ch &&
				GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_M3_DROPPER &&
				ch->GetLevel() <= PLAYERBOT_EXP_LOCK_M3_DROPPER + PLAYERBOT_DROPPER_OUTGROWN_LEVELS;
	}

	// Iwakura's price curve lives with the town (playerbot_town.h), later.
	DWORD ScalePlayerBotIwakuraPrice(DWORD base);

	// Iwakura's second tier is M3's own ground (GRINDER_TIERS in
	// playerbot_persona_rules.h: "M3, the cursed animals and the level-30
	// weapons", 19 to 25), and Community Patch 1 sends the quarter that skips
	// the first village there from thirteen. Nothing sent a Grinder there for
	// its tier: the only road in was the level-30 weapon hunt - a third of the
	// bots without the weapon - and since Community Patch 2 a bot that can buy
	// the weapon does not farm it. M3 held ten of the 426 bots of fifteen to
	// twenty-five on m2zip, and on a young world none ("boty nie chodza wcale
	// na M3", Iwakura, 23 September). A Grinder of the tier - not a Conqueror,
	// not one that gave grinding up, no dropper - that meets the document's
	// entry (a weapon at +6 and an armour at +5, MeetsPlayerBotM3Survival)
	// hunts there, weapon or none, inside the crowd share the hunt has.
	bool IsPlayerBotM3TierGrinder(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotPersona& p = it->second.persona;
		if (!p.bRestored || p.bAdvanced || p.bQuitGrinding || IsPlayerBotDropper(it->second.bPersonality))
			return false;
		const uint8_t level = (uint8_t)std::min<int>(255, ch->GetLevel());
		const uint8_t tier = playerbot_persona::GrinderTierFor(level);
		const bool skipper = tier == 1 && level >= playerbot_persona::GRINDER_TIER1_SKIP_LEVEL &&
				playerbot_persona::SkipsFirstVillage(ch->GetPlayerID());
		return (tier == 2 || skipper) && MeetsPlayerBotM3Survival(ch);
	}

	bool ShouldPlayerBotVisitM3(LPCHARACTER ch)
	{
		const bool tierGrinder = IsPlayerBotM3TierGrinder(ch);
		if (!ch || (!tierGrinder && !HasPlayerBotM3ReadyEquipment(ch)))
			return false;
		// The farm is for a drop, and a full bag has no cell for it.
		if (IsPlayerBotBagFull(ch))
			return false;
		// The M3 dropper is there for the weapons it will sell, so owning one
		// changes nothing, and it stays as long as the map can still be hunted.
		if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_M3_DROPPER)
			return IsPlayerBotM3DropperOnFarm(ch);
		// Everything from here to the crowd is the weapon hunt's, and the
		// tier's Grinder is not there for the weapon.
		if (!tierGrinder && HasPlayerBotSpecialLevel30Weapon(ch, true))
			return false;
		// One a counter holds and the purse reaches is bought, not farmed
		// (community patch 2, point 1): the market trip is the next town
		// visit's, and M3 is for the bots it would not serve.
		if (!tierGrinder && PlayerBotMarketHasClassLevel30Weapon(ch) &&
				GetPlayerBotLevel30PurchaseCap(ch) >=
					(long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_LEVEL30_BASE_PRICE))
			return false;
		// Twenty-four was the cap, and it made the weapon a thing a bot either
		// got young or never got: past it the only route left was a counter it
		// never walked to. Measured: 205 of the 393 bots of thirty-six and up
		// holding over a million yang owned no level-30 weapon anywhere -
		// "bots flying round the valley at thirty-seven with six million in the
		// bag and a +6 cone sword", as the Discord put it. The farm stays worth
		// working to forty: the infected animals are around thirty, and the
		// kill-drop curve (aiPercentByDeltaLev) is still at seventy percent ten
		// levels above them and falls off a cliff only after that.
		if (ch->GetLevel() > PLAYERBOT_LEVEL30_WEAPON_HUNT_MAX_LEVEL)
			return false;
		// The farm is one map. See PLAYERBOT_M3_CROWD_SHARE_PERCENT: the door
		// closes at the share and the bots inside keep their place until the
		// crowd is well over it, so the ones sent home are not sent straight
		// back through the door they just left.
		// Per kingdom: three guild maps of the same size, so a share of the
		// whole population on one of them would be three times the crowd the
		// share was measured for.
		const long guildMap = playerbot_empire_rules::GetHomeMap(
				(int)ch->GetEmpire(), playerbot_empire_rules::MAP_ROLE_M3);
		const int crowd = GetPlayerBotsOnMap(guildMap);
		const int share = std::max(PLAYERBOT_M3_CROWD_MIN,
				GetPlayerBotsAlive() * PLAYERBOT_M3_CROWD_SHARE_PERCENT /
					(100 * playerbot_empire_rules::EMPIRE_COUNT_REAL));
		if (ch->GetMapIndex() == guildMap
				? crowd > share * PLAYERBOT_M3_CROWD_STAY_PERCENT / 100
				: crowd >= share)
			return false;
		if (tierGrinder)
			return true;
		// A stable third of the young population farms infected animals for
		// class-specific level-30 weapons; the rest stay in M2 for Bestials.
		// Past thirty-five nobody is left in M2 to share the work with, so the
		// third becomes everyone - within the share above.
		if (ch->GetLevel() > 35)
			return true;
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d335850U) % 3U) == 0;
	}

	bool ShouldPlayerBotHuntM2Bestials(LPCHARACTER ch)
	{
		if (!ch || !HasPlayerBotM3ReadyEquipment(ch))
			return false;
		// The M2 dropper camps the two Bestials for the weapons it will sell,
		// owned weapon or not, for as long as they are worth its level.
		if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_M2_DROPPER)
			return ch->GetLevel() >= 25 && ch->GetLevel() <= 40;
		if (ch->GetLevel() < 25 || ch->GetLevel() > PLAYERBOT_LEVEL30_WEAPON_HUNT_MAX_LEVEL ||
				HasPlayerBotSpecialLevel30Weapon(ch, true) ||
				ShouldPlayerBotVisitM3(ch))
			return false;
		// M3 already owns one third of the eligible weapon hunters. Half of the
		// remaining cohort searches the two rare Bestial spawns in M2, while the
		// rest keeps levelling normally instead of camping one pair of enemies.
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x42455354U) % 2U) == 0;
	}

	bool ShouldPlayerBotPursueHorseExpedition(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		// Nobody past PLAYERBOT_MONKEY_MEDAL_MAX_LEVEL farms medals, the dropper
		// included: the rolls there are worth a few percent of a medal, and such
		// a bot buys its medal from a counter instead.
		if (ch->GetLevel() > PLAYERBOT_MONKEY_MEDAL_MAX_LEVEL)
			return false;
		// The medal dropper goes for the medals themselves, whatever its own horse
		// needs, in whichever dungeon its level earns them, for as long as a medal
		// has a cell to land in. It used to leave at PLAYERBOT_BAG_FULL_PERCENT
		// like everybody else, and a keeper's bag is its counter's stock - fifty
		// to seventy of ninety cells - so a minute of the dungeon's loot took it
		// over the line: 29 of 37 visits after the dropper's errands were cleared
		// ended inside three minutes as "horse complete", with the planner turning
		// the goal back to levelling in the same second. A bag whose items have
		// not loaded yet (the seconds after a warp) is not a full one.
		if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_MEDAL_DROPPER)
		{
			if (ch->IsItemLoaded() && ch->GetEmptyInventory(1) < 0)
				return false;
			// And not with its stock in the bag: the dungeon's exit rule sends
			// a dropper out at PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK medals
			// (DecideMonkeyExit, MEDAL_READY) and this gate used to send it
			// straight back - 703 of 854 visits under ten seconds in an hour on
			// the test world, one bot every fifty seconds (16 September). It
			// hunts on its village's ground until a counter line sells.
			if (ch->IsItemLoaded() &&
					ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) >= PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK)
				return false;
			return GetPlayerBotMonkeyMapFor(ch) != 0;
		}
		// A medal needs a cell. This gate is what the Monkey Dungeon's exit
		// decision and the planner both read, so a bot already inside with a
		// full bag finishes its medal and leaves.
		if (IsPlayerBotBagFull(ch))
			return false;
		if (!CanPlayerBotAdvanceHorse(ch))
			return false;

		// There is a dungeon for every level from eighteen up - see
		// GetPlayerBotMonkeyMapFor for why the easy one alone gave a bot of
		// forty nothing - so the band no longer ends the errand: 439 bots past
		// forty stood on no horse at all, and 435 more on one below ten. This
		// gate also empties a dungeon: a bot inside re-evaluates the same call
		// every tick and walks out as soon as it is no longer chosen. It is
		// also what refuses the errand to a kingdom with no dungeon it can
		// reach, instead of sending it at one and having the warp refused.
		if (GetPlayerBotMonkeyMapFor(ch) == 0)
			return false;

		// A combat horse matters most to Warriors and weapon Suras, but it must be
		// one goal among several rather than a compulsory conveyor belt through the
		// dungeon.  The cohort rotates every 30 minutes and again after every earned
		// horse level.  Eventually every build gets opportunities while most bots
		// continue levelling in M2 at any given time.
		const bool hasCombatHorse = ch->GetHorseLevel() >= 11;
		BYTE chance = 10;
		switch (ch->GetJob())
		{
			// The medal is the market's short: a battle horse used to end a
			// bot's trips almost for good (4% a window), and the cohort past
			// forty-six has one, so the hard dungeon - the only place a medal
			// drops at full odds for them - saw a few bots an hour. Three
			// times the after-horse chance, the before-horse ones unchanged.
			case JOB_WARRIOR:
				chance = hasCombatHorse ? 12 : (ch->GetHorseLevel() == 0 ? 34 : 26);
				break;
			case JOB_SURA:
				// Skill group 1 is Weaponry (WP); group 2 is Black Magic.
				chance = ch->GetSkillGroup() == 1
						? (hasCombatHorse ? 12 : (ch->GetHorseLevel() == 0 ? 32 : 25))
						: (hasCombatHorse ? 6 : (ch->GetHorseLevel() == 0 ? 14 : 9));
				break;
			case JOB_ASSASSIN:
				chance = ch->GetSkillGroup() == 2
						? (hasCombatHorse ? 3 : (ch->GetHorseLevel() == 0 ? 6 : 4))
						: (hasCombatHorse ? 6 : (ch->GetHorseLevel() == 0 ? 18 : 14));
				break;
			case JOB_SHAMAN:
				chance = hasCombatHorse ? 6 : (ch->GetHorseLevel() == 0 ? 15 : 10);
				break;
		}
		// Twice as often before the battle horse: the chances above sent 17 of
		// 999 bots into a dungeon (PLAYERBOT_HORSE_EXPEDITION_NO_COMBAT_HORSE_MULT).
		if (!hasCombatHorse)
			chance = (BYTE)std::min<int>(PLAYERBOT_HORSE_EXPEDITION_MAX_CHANCE,
					chance * PLAYERBOT_HORSE_EXPEDITION_NO_COMBAT_HORSE_MULT);
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (stateIt != s_mapPlayerBotAIStates.end() &&
				stateIt->second.bAmbition == BOT_AMBITION_HORSE && !hasCombatHorse)
			chance = (BYTE)std::min<int>(PLAYERBOT_HORSE_EXPEDITION_MAX_CHANCE, chance + 15);

		const DWORD window = dwNow / (30U * 60U * 1000U);
		const DWORD seed = ch->GetPlayerID() ^ (window * 0x9e3779b9U) ^
				((DWORD)(ch->GetHorseLevel() + 1) * 0x85ebca6bU);
		// Through the weight, not past it. An operator who raises HORSE in the
		// panel is asking for exactly this - more bots in the dungeon - and until
		// now the slider moved the goal planner while the gate that actually
		// sends them ignored it, so nothing he did had any effect.
		return PlayerBotWeightedRoll(PlayerBotNavHash(seed ^ 0x484f5253U) % 100U,
				chance, PLAYERBOT_WEIGHT_HORSE);
	}

	int GetPlayerBotDesiredHorseMedalStock(LPCHARACTER ch)
	{
		if (!ch)
			return 1;
		if (GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_MEDAL_DROPPER)
			return PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK;
		// The high-priority builds occasionally prepare the next horse level in the
		// same visit. Other classes leave after one medal, freeing dungeon capacity
		// and returning to ordinary experience progression much sooner.
		const bool highPriority = ch->GetJob() == JOB_WARRIOR ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 1);
		return highPriority
				? 1 + (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d454441U) % 2U)
				: 1;
	}

	// A warp that half worked, and the only kind of damage a bot cannot walk off.
	//
	// The frontier maps carry warp NPCs of their own and a bot that strays into
	// one's trigger radius is handed to whatever map it points at. When that map
	// belongs to the other core, the warp goes half way: the character's
	// coordinates are set to the destination and the placement into a sector
	// fails, so it stands in Orc Valley carrying a position in Milgyo. The
	// engine notices and says so - "SECTREE DIFFER: botsimon 56x111 was 56x112" -
	// and then the ordinary logout saves the coordinates it cannot use:
	//
	//     PlayerLoad: cannot find valid location 553600 x 144100 (name: botsimon)
	//
	// From then on the character is refused at every login. It is not stuck, or
	// idle, or lost - it is gone, and the only sign is that the world runs fewer
	// bots than it was asked for. Eighty-five had accumulated this way, 10% of
	// the population, all of them saved at one identical coordinate.
	//
	// The recovery branch at the end of ManagePlayerBotWorldTravel cannot help:
	// it reads GetMapIndex(), which still says Orc Valley, because the sector is
	// the half of the warp that failed. So the mismatch itself is what has to be
	// looked for, while the bot is still logged in and can still be moved.
	bool IsPlayerBotPositionOffItsMap(LPCHARACTER ch)
	{
		if (!ch || !ch->GetSectree())
			return false;
		const int byPosition = SECTREE_MANAGER::instance().GetMapIndex(
				ch->GetX(), ch->GetY());
		return byPosition > 0 && byPosition != (int)ch->GetMapIndex();
	}

	// Defined beside the party pass in playerbot_manager.cpp.
	bool IsPlayerBotHumanLedParty(LPPARTY party);
	// A contract's client, a mercenary whose contract runs, and a bot leading
	// a party with a person in it keep their map (playerbot_companions.h).
	bool IsPlayerBotHeldForCompany(LPCHARACTER ch);

	bool TransitionPlayerBotMap(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		// V1 lies across the desert - see PLAYERBOT_DESERT_V1_GATE_X. A warp into
		// it from anywhere but the desert becomes a warp onto the desert with the
		// real destination remembered, and ManagePlayerBotWorldTravel walks the
		// bot to the gate; a warp out of it becomes the desert's far corner and
		// the walk back to the Bokjung gate. Both call back in here with the
		// desert as the target, which neither rule touches.
		if (IsPlayerBotSpiderMap(targetMap) && ch->GetMapIndex() != PLAYERBOT_MAP_DESERT &&
				!IsPlayerBotSpiderMap(ch->GetMapIndex()))
		{
			state.lDesertCrossingTo = targetMap;
			state.lDesertCrossingX = targetX;
			state.lDesertCrossingY = targetY;
			state.dwNextCrossingStoneCheck = 0;
			long desertX = 0, desertY = 0;
			GetPlayerBotFrontierArrivalFor(ch, PLAYERBOT_MAP_DESERT, desertX, desertY);
			return TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_DESERT,
					desertX, desertY, dwNow, "desert_crossing_to_v1");
		}
		if (IsPlayerBotSpiderMap(ch->GetMapIndex()) && targetMap != PLAYERBOT_MAP_DESERT &&
				!IsPlayerBotSpiderMap(targetMap))
		{
			state.lDesertCrossingTo = targetMap;
			state.lDesertCrossingX = targetX;
			state.lDesertCrossingY = targetY;
			state.dwNextCrossingStoneCheck = 0;
			return TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_DESERT,
					PLAYERBOT_DESERT_FROM_V1_X, PLAYERBOT_DESERT_FROM_V1_Y, dwNow, "desert_crossing_from_v1");
		}
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(targetMap);
		if (!navigation.Init(targetMap))
		{
			// Tagged by the reason, so a reason nobody has seen before still
			// speaks up inside the minute instead of hiding behind an old one.
			char szTag[64];
			snprintf(szTag, sizeof(szTag), "world_nav:%s", reason ? reason : "?");
			PlayerBotLogThrottled(szTag, dwNow,
					"PLAYERBOT_WORLD: target navigation unavailable pid=%u name=%s map=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), targetMap, reason ? reason : "?");
			return false;
		}

		const long oldMap = ch->GetMapIndex();
		const bool wasRiding = ch->IsRiding();
		// A bot party is one camp and ends with the map. A player's party is the
		// player's: a bot off to town, or put back on its feet by the sectree
		// rescue, is still in it - the rescue after a Demon Tower warp left a
		// player alone in his own party (sizowski, 14 September).
		if (ch->GetParty() && !IsPlayerBotHumanLedParty(ch->GetParty()))
			LeavePlayerBotParty(ch);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		// A PC mount and the separately summoned horse are two different server
		// entities. StopRiding() summons the latter on the old map, so explicitly
		// remove it before Show(). Otherwise a rider can leave behind an orphaned
		// horse at a dungeon portal (issue #4).
		if (wasRiding)
			ch->StopRiding();
		ch->HorseSummon(false);
		// And the stall, for the same reason and in the same place: the sign has
		// to be taken back on the map where it was put up. Closing it after the
		// warp broadcasts the clear to whoever happens to be standing in the
		// destination, while the people who actually saw it are still on the
		// market - which is how a bot ends up in the Monkey Dungeon wearing a
		// shop nobody can open.
		ClosePlayerBotShop(ch, state, dwNow, "map_transition");
		ClearPlayerBotRoute(state, true);
		state.bVisitingShop = false;
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		if (!ch->Show(targetMap, targetX, targetY, 0))
		{
			if (wasRiding && !ch->IsRiding())
				ch->StartRiding();
			sys_err("PLAYERBOT_WORLD: transition failed pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason ? reason : "?");
			return false;
		}
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, targetX, targetY, 0, dwNow);
		if (wasRiding && ch->GetHorseHealth() > 0 && ch->GetHorseStamina() > 0)
			ch->StartRiding();
		ch->Save();
		state.dwNextWanderTime = dwNow + number(1500, 4500);
		state.dwNextHorseRideCheckTime = dwNow + 1000;
		state.dwDungeonEnteredTime = IsPlayerBotMonkeyMap(targetMap) ? dwNow : 0;
		state.dwM3EnteredTime = IsPlayerBotM3Map(targetMap) ? dwNow : 0;
		state.dwFrontierEnteredTime = IsPlayerBotFrontierMap(targetMap) ? dwNow : 0;
		// Landed anywhere but the desert: whatever crossing was under way is over.
		if (targetMap != PLAYERBOT_MAP_DESERT)
			state.lDesertCrossingTo = 0;
		// Everybody enters Orc Valley at the same point, so without this the
		// whole population lands on the doorstep and stays there - the rotation
		// that would move it on runs only on a tick with nothing to fight, and
		// that map has four thousand spawn points. Start the walk to this bot's
		// own hub on arrival instead of after the first camp timer expires.
		state.lCampX = targetX;
		state.lCampY = targetY;
		state.dwCampSince = dwNow;
		state.dwRelocateSince = IsPlayerBotFrontierMap(targetMap) ? dwNow : 0;
		if (IsPlayerBotM3Map(targetMap))
			state.dwNextRemoteRefineReturnTime = 0;
		// The departure intent is done when the bot lands where it meant to
		// go - or on any frontier, if the level band moved the target while
		// it waited. Nothing ever cleared it before: a bot held once in
		// Bokjung carried lDepartureMap for the rest of its life, back in town
		// for services it refused every material hunt and every trip to Joan
		// ("Ide na pustynie" over a bot circling M1 - D12 of the audit).
		if (state.lDepartureMap != 0 &&
				(state.lDepartureMap == targetMap || IsPlayerBotFrontierMap(targetMap)))
		{
			sys_log(0, "PLAYERBOT_DEPARTURE: arrived pid=%u name=%s to=%ld wanted=%ld after_ms=%u",
					ch->GetPlayerID(), ch->GetName(), targetMap, state.lDepartureMap,
					state.dwDepartureSince != 0 ? dwNow - state.dwDepartureSince : 0);
			state.lDepartureMap = 0;
			state.dwDepartureSince = 0;
		}
		sys_log(0, "PLAYERBOT_WORLD: transitioned pid=%u name=%s from=%ld to=%ld pos=(%ld,%ld) reason=%s",
				ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, targetX, targetY,
				reason ? reason : "?");
		// How long the bot stayed in town after its errand was done. Asked for
		// by name: "sam spadek liczby atakow nie dowodzi naprawy".
		if (IsPlayerBotM2Map(oldMap) && state.dwErrandDoneTime != 0 &&
				ch->GetLevel() >= 40)
		{
			sys_log(0, "PLAYERBOT_M2: left after errand pid=%u name=%s level=%u waited_ms=%u to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					dwNow - state.dwErrandDoneTime, targetMap, reason ? reason : "?");
			state.dwErrandDoneTime = 0;
		}
		return true;
	}

	// The gate itself, found on the map. A warp NPC (CHAR_TYPE_WARP) carries
	// its destination in its name - "%s %ld %ld", world coordinates in cells,
	// exactly what FuncCheckWarp in char.cpp reads - and the engine sends a
	// player within 300 units of it there. Our portal constants were read out
	// of npc.txt once; the package an operator installed from may place a
	// gate elsewhere, and a bot walking to where the gate used to be stands
	// beside it for good ("do portalu, tuz przed nim, zawraca, kolko wokol
	// M2"). So the point walked to is the living NPC whose destination lies
	// on the target map, the one nearest the point asked for, remembered per
	// map and destination for PLAYERBOT_WARP_NPC_CACHE_MS. The Teleporter is
	// not a warp NPC - he is a quest - so a trip through him keeps the
	// constant, and pays.
	struct FPlayerBotFindWarp
	{
		long m_lTargetMap;
		long m_lNearX, m_lNearY;
		LPCHARACTER m_found;
		long m_lBest;
		FPlayerBotFindWarp(long targetMap, long nearX, long nearY)
			: m_lTargetMap(targetMap), m_lNearX(nearX), m_lNearY(nearY), m_found(NULL), m_lBest(-1) {}
		void operator()(LPENTITY entity)
		{
			if (!entity || !entity->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER npc = static_cast<LPCHARACTER>(entity);
			if (!npc->IsWarp())
				return;
			char szTmp[64];
			long lToX = 0, lToY = 0;
			if (3 != sscanf(npc->GetName(), " %63s %ld %ld ", szTmp, &lToX, &lToY))
				return;
			if (SECTREE_MANAGER::instance().GetMapIndex(lToX * 100, lToY * 100) != m_lTargetMap)
				return;
			const long d = DISTANCE_APPROX(npc->GetX() - m_lNearX, npc->GetY() - m_lNearY);
			if (m_lBest < 0 || d < m_lBest)
			{
				m_lBest = d;
				m_found = npc;
			}
		}
	};

	bool FindPlayerBotWarpNpc(long mapIndex, long targetMap, long nearX, long nearY,
			DWORD dwNow, long& outX, long& outY)
	{
		struct TGateAnswer { DWORD dwStamp; bool bFound; long lX; long lY; };
		static std::map<std::pair<long, long>, TGateAnswer> s_mapGates;
		const std::pair<long, long> key(mapIndex, targetMap);
		std::map<std::pair<long, long>, TGateAnswer>::iterator it = s_mapGates.find(key);
		if (it != s_mapGates.end() && dwNow - it->second.dwStamp < PLAYERBOT_WARP_NPC_CACHE_MS)
		{
			outX = it->second.lX;
			outY = it->second.lY;
			return it->second.bFound;
		}
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
		if (!pMap)
			return false;
		FPlayerBotFindWarp finder(targetMap, nearX, nearY);
		pMap->for_each(finder);
		TGateAnswer& answer = s_mapGates[key];
		answer.dwStamp = dwNow;
		answer.bFound = finder.m_found != NULL;
		answer.lX = answer.bFound ? finder.m_found->GetX() : 0;
		answer.lY = answer.bFound ? finder.m_found->GetY() : 0;
		if (it == s_mapGates.end())
		{
			if (answer.bFound)
				sys_log(0, "PLAYERBOT_WORLD: gate to map=%ld on map=%ld at (%ld,%ld) name=%s asked=(%ld,%ld) off_by=%ld",
						targetMap, mapIndex, answer.lX, answer.lY, finder.m_found->GetName(),
						nearX, nearY, finder.m_lBest);
			else
				sys_log(0, "PLAYERBOT_WORLD: no warp npc to map=%ld on map=%ld, keeping the point (%ld,%ld)",
						targetMap, mapIndex, nearX, nearY);
		}
		outX = answer.lX;
		outY = answer.lY;
		return answer.bFound;
	}

	bool IsPlayerBotTeleporterPoint(long x, long y)
	{
		return (x == PLAYERBOT_M1_TELEPORTER_X && y == PLAYERBOT_M1_TELEPORTER_Y) ||
				(x == PLAYERBOT_M2_TO_M3_TELEPORTER_X && y == PLAYERBOT_M2_TO_M3_TELEPORTER_Y);
	}

	int GetPlayerBotTeleporterFee(LPCHARACTER ch)
	{
		return std::max(PLAYERBOT_TELEPORTER_FEE_PER_FIVE_LEVELS,
				((int)ch->GetLevel() / 5) * PLAYERBOT_TELEPORTER_FEE_PER_FIVE_LEVELS);
	}

	bool MovePlayerBotToWorldPortal(LPCHARACTER ch, TPlayerBotAIState& state,
			long portalX, long portalY, long targetMap, long targetX, long targetY,
			DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Through the Teleporter, at his price, or through the gate where it
		// actually stands.
		const bool viaTeleporter = IsPlayerBotTeleporterPoint(portalX, portalY);
		if (viaTeleporter)
		{
			if (ch->GetLevel() < PLAYERBOT_TELEPORTER_MIN_LEVEL ||
					ch->GetGold() < GetPlayerBotTeleporterFee(ch))
			{
				PlayerBotLogThrottled("teleporter_refused", dwNow,
						"PLAYERBOT_WORLD: teleporter refuses pid=%u name=%s level=%u gold=%lld fee=%d to=%ld reason=%s",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (long long)ch->GetGold(),
						GetPlayerBotTeleporterFee(ch), targetMap, reason ? reason : "?");
				// Not again this tick, nor the next thousand: the fee is earned
				// at a counter or a merchant, and both run only on a tick the
				// travel pass does not take. A departure held for a purchase
				// keeps its map (lDepartureMap) and comes back when the wait
				// is over.
				state.dwNextWorldTravelTime = dwNow + PLAYERBOT_TELEPORTER_RETRY_MS;
				return false;
			}
		}
		else
		{
			long gateX = 0, gateY = 0;
			if (FindPlayerBotWarpNpc(ch->GetMapIndex(), targetMap, portalX, portalY, dwNow, gateX, gateY))
			{
				portalX = gateX;
				portalY = gateY;
			}
		}

		// Warp NPCs trigger at 300 units and hand the client to whichever core
		// hosts the target map, which is not a conversation a bot descriptor can
		// have - so the switch is made here, server-side, before the bot gets
		// that close. It used to happen at 900 units, nine metres short of the
		// portal, and a player watching one leave saw it vanish out of clear
		// ground. Patch 0008 makes warp_npc_event ignore bots outright, so the
		// margin only has to cover one tick of running now: the character walks
		// up to the portal and goes from there, the way a player does.
		const int distance = DISTANCE_APPROX(ch->GetX() - portalX, ch->GetY() - portalY);
		if (distance <= PLAYERBOT_PORTAL_SWITCH_DISTANCE)
		{
			state.dwPortalWalkSince = 0;
			state.iPortalWalkBest = 0;
			state.wPortalWalkTicks = 0;
			state.wPortalWalkRouteIndex = 0;
			const int fee = viaTeleporter ? GetPlayerBotTeleporterFee(ch) : 0;
			if (!TransitionPlayerBotMap(ch, state, targetMap, targetX, targetY, dwNow, reason))
				return false;
			if (fee > 0)
			{
				PlayerBotChangeGold(ch, -fee);
				sys_log(0, "PLAYERBOT_WORLD: teleporter fee pid=%u name=%s level=%u fee=%d to=%ld gold_left=%lld",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), fee, targetMap, (long long)ch->GetGold());
			}
			return true;
		}

		// Walking to a portal has to be able to fail, and this is what it looked
		// like when it could not.
		//
		// The old body ignored what MovePlayerBot returned and ended in an
		// unconditional "return true", so a bot whose route could not be planned
		// reported a successful travel step - and travel claims the tick, so
		// nothing below it in the update ever ran again. Four of them were found
		// standing at the Bokjung teleporter: no route (route=0/0), no movement,
		// nothing in the log but their goal flipping between LEVEL_UP and BIOLOGIST
		// every six seconds as the planner and this branch overwrote each other.
		// The inactivity watchdog reset them 209 times in an evening and every
		// reset delivered them straight back into the same decision.
		//
		// Neither the map nor the route was at fault: 99.9% of Bokjung is one
		// walkable component and that portal is reachable from where they stood.
		// The navigation has several paths that stand still and report success - a
		// deferred plan when the per-tick planning budget is spent, and the
		// back-off window after a failure. Any of them, entered every tick, freezes
		// a bot for good once the caller treats them as progress.
		//
		// So progress is what is measured, not the return value. A bot that has not
		// moved for PLAYERBOT_PORTAL_WALK_TIMEOUT hands the tick back, and that is
		// all it takes: the update falls through to hunting and wandering, the bot
		// ends up somewhere else, and the next attempt plans from there.
		// Progress is the straight line shortening - or a waypoint going by.
		//
		// The straight line alone declared a stall on a bot that was walking
		// perfectly well: measured live at the Orc Valley teleporter, route
		// five of seven, stuck counter zero, three hundred and seventy units
		// away and thrown off its route every twenty seconds for the crime of
		// going round the building rather than through it. A portal stands
		// against scenery far more often than in open ground, so the detour is
		// the normal case and not the exception.
		const WORD routeIndex = (WORD)std::min<size_t>(state.uRouteIndex, 65535);
		if (state.dwPortalWalkSince == 0 ||
				distance + PLAYERBOT_PORTAL_WALK_PROGRESS <= state.iPortalWalkBest ||
				distance >= state.iPortalWalkBest + PLAYERBOT_PORTAL_WALK_PROGRESS ||
				routeIndex > state.wPortalWalkRouteIndex)
		{
			state.dwPortalWalkSince = dwNow;
			state.iPortalWalkBest = distance;
			state.wPortalWalkTicks = 0;
			state.wPortalWalkRouteIndex = routeIndex;
		}
		else if (dwNow - state.dwPortalWalkSince >= PLAYERBOT_PORTAL_WALK_TIMEOUT &&
				state.wPortalWalkTicks >= PLAYERBOT_PORTAL_WALK_MIN_TICKS)
		{
			state.dwPortalWalkSince = 0;
			state.iPortalWalkBest = 0;
			const WORD ticks = state.wPortalWalkTicks;
			state.wPortalWalkTicks = 0;
			state.wPortalWalkRouteIndex = 0;
			// The rider keeps the saddle on the way out. It used to get off here,
			// because the manager's "a transport horse must not fight" pass took any
			// rider off and claimed the tick - the bot's whole escape - and
			// forty-six bots at the Sohan exit mounted and dismounted every twenty
			// seconds without a step. That pass wants a target now (combat_ready),
			// and nothing the escape falls through to wants the ground.
			// Everything needed to tell the three failures apart without a
			// second deploy: whether the walk was ever asked to happen (ticks),
			// whether it had a route to follow (route), whether the planner was
			// holding it off (plan_in), and whether the straight line to the
			// portal is clear at all (seg).
			CPlayerBotNavigation& diagNav =
					CPlayerBotNavigation::instance(ch->GetMapIndex());
			const int segClear = diagNav.Init(ch->GetMapIndex())
					? (diagNav.SegmentClearWorld(ch->GetX(), ch->GetY(), portalX, portalY) ? 1 : 0)
					: -1;
			// Tagged by the portal, not by the whole subsystem: one tag for
			// every portal in the world meant one line a minute between them,
			// and the busiest one hid the other nine behind its own count.
			char szStuckTag[64];
			snprintf(szStuckTag, sizeof(szStuckTag), "portal_stuck:%s",
					reason ? reason : "?");
			// Into syserr as well: a support bundle carries syserr and not
			// syslog, and the one bundle sent about a bot circling a gate held
			// nothing about the gate.
			PlayerBotErrThrottled(szStuckTag, dwNow,
					"PLAYERBOT_WORLD: portal walk stalled pid=%u name=%s map=%ld pos=(%ld,%ld) portal=(%ld,%ld) distance=%d reason=%s riding=%d last=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
					portalX, portalY, distance, reason ? reason : "?", ch->IsRiding() ? 1 : 0,
					(unsigned int)state.bLastNavOutcome);
			PlayerBotLogThrottled(szStuckTag, dwNow,
					"PLAYERBOT_WORLD: portal walk stalled pid=%u name=%s map=%ld pos=(%ld,%ld) portal=(%ld,%ld) distance=%d reason=%s "
					"ticks=%u route=%u/%u plan_in=%d defer=%u stuck=%u seg=%d riding=%d last=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
					ch->GetX(), ch->GetY(), portalX, portalY, distance,
					reason ? reason : "?", (unsigned int)ticks,
					(unsigned int)state.uRouteIndex, (unsigned int)state.vecRoute.size(),
					state.dwNextNavPlanTime > dwNow ? (int)(state.dwNextNavPlanTime - dwNow) : 0,
					(unsigned int)state.bNavDeferredCount, (unsigned int)state.bStuckCounter,
					segClear, ch->IsRiding() ? 1 : 0,
					(unsigned int)state.bLastNavOutcome);
			return false;
		}

		// Mounted all the way up to it. A teleporter is scenery to a bot - the warp
		// happens server-side and nothing is said to anybody - so the dismount
		// MovePlayerBot performs on arrival at an ordinary destination buys nothing
		// here. It was 1362 of one evening's dismounts, each followed by a remount
		// on the far side three seconds later.
		if (state.wPortalWalkTicks < 65535)
			++state.wPortalWalkTicks;
		// Walk to the middle of the portal's cell, not to the coordinate the
		// constant names. The planner works in cells and strings its corners
		// straight between their centres; the walk tests the real segment with
		// a supercover traversal that counts a cell grazed by a millimetre. Aim
		// at a raw point on a cell boundary and the two disagree about the last
		// segment, permanently - the plan comes back identical every time, and
		// a bot refuses its own only waypoint forty-two times in twenty seconds
		// without a word in any log. Measured at five portals on four maps. The
		// centre is at most thirty-five units off, against a switch distance of
		// two hundred.
		long walkX = portalX, walkY = portalY;
		CPlayerBotNavigation& portalNav = CPlayerBotNavigation::instance(ch->GetMapIndex());
		if (portalNav.Init(ch->GetMapIndex()))
			portalNav.CellCentreWorld(portalX, portalY, walkX, walkY);
		MovePlayerBot(ch, walkX, walkY, dwNow, PLAYERBOT_PORTAL_SNAP_CELLS,
				true, true, false, true);
		return true;
	}

	// Being in a fight is not the same as having something selected. A bot is in
	// the fight when blows are being exchanged, when the monster is coming for
	// it, or when it already stands within reach. A mob picked out across the
	// field is none of those and must not postpone a decision to leave the map.
	bool IsPlayerBotEngagedWith(LPCHARACTER ch, LPCHARACTER victim,
			const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !victim)
			return false;
		if (victim->GetVictim() == ch)
			return true;
		if (state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime <= PLAYERBOT_TRAVEL_ENGAGED_WINDOW)
			return true;
		return DISTANCE_APPROX(ch->GetX() - victim->GetX(),
				ch->GetY() - victim->GetY()) <= PLAYERBOT_TRAVEL_ENGAGED_RANGE;
	}

	// The nearest stone within reach that is worth this bot's level. The
	// crossing fights nothing else, and this is looked for by hand because the
	// target scan does not run while a walk owns the tick.
	struct FPlayerBotFindStoneNearby
	{
		LPCHARACTER m_owner;
		LPCHARACTER m_found;
		int m_bestDistance;
		FPlayerBotFindStoneNearby(LPCHARACTER owner, int range)
			: m_owner(owner), m_found(NULL), m_bestDistance(range) {}
		void operator()(LPENTITY entity)
		{
			if (!entity || !entity->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER stone = static_cast<LPCHARACTER>(entity);
			if (!stone->IsStone() || stone->IsDead() ||
					!IsPlayerBotMetinWorthFighting(m_owner, stone))
				return;
			const int distance = DISTANCE_APPROX(stone->GetX() - m_owner->GetX(),
					stone->GetY() - m_owner->GetY());
			if (distance < m_bestDistance)
			{
				m_bestDistance = distance;
				m_found = stone;
			}
		}
	};

	LPCHARACTER FindPlayerBotStoneNearby(LPCHARACTER ch, int range)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		FPlayerBotFindStoneNearby finder(ch, range);
		ch->GetSectree()->ForEachAround(finder);
		return finder.m_found;
	}

	// The Teleport Ring recall (see PLAYERBOT_TELEPORT_RING_VNUM). Keyed by
	// pid so the ring keeps its cooldown without a state-struct field.
	std::map<DWORD, DWORD> s_mapPlayerBotTeleportRingReady;

	bool PlayerBotHoldsTeleportRing(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() >= PLAYERBOT_TELEPORT_RING_MIN_LEVEL &&
				ch->CountSpecifyItem(PLAYERBOT_TELEPORT_RING_VNUM) > 0;
	}

	// Recall home now instead of walking to the exit. Same destination the
	// walk would reach (so same core - only Chunjo bots stand on the shared
	// frontier, and their village is on that core), just instant.
	bool TryPlayerBotTeleportRingHome(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long destMap, long destX, long destY, const char* reason)
	{
		if (!PlayerBotHoldsTeleportRing(ch))
			return false;
		std::map<DWORD, DWORD>::const_iterator it =
				s_mapPlayerBotTeleportRingReady.find(ch->GetPlayerID());
		if (it != s_mapPlayerBotTeleportRingReady.end() && dwNow < it->second)
			return false;
		if (!TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, reason))
			return false;
		s_mapPlayerBotTeleportRingReady[ch->GetPlayerID()] = dwNow + PLAYERBOT_TELEPORT_RING_COOLDOWN_MS;
		sys_log(0, "PLAYERBOT_WORLD: teleport ring home pid=%u name=%s to_map=%ld (%s)",
				ch->GetPlayerID(), ch->GetName(), destMap, reason ? reason : "");
		return true;
	}

	// Defined in playerbot_gambler.h, after the town visit that runs it.
	bool IsPlayerBotGambling(const TPlayerBotAIState& state, DWORD dwNow);

	bool ManagePlayerBotWorldTravel(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || state.bVisitingBiologist ||
				state.bVisitingStable || state.bRecoveringAfterDeath || state.bTacticalRetreat)
			return false;
		// A gambler whose visit was cut short (a death, the watchdog) is back at
		// the anvil when the town check comes round again in a minute or two;
		// the road would take it to another map for the rest of its session.
		if (IsPlayerBotGambling(state, dwNow) && IsPlayerBotVillageMap(ch->GetMapIndex()))
			return false;
		// A bot in a player's party goes where the player goes
		// (ManagePlayerBotFollowHumanLeader), not where its own plans send it.
		// The follow pass leaves the bot to the rest of the tick once it stands
		// near the player, and this pass then sent it off: three shamans in
		// sizowski's party paid the Teleporter for Hwang, Sohan and the desert,
		// were warped back to him a second later, and set off again - six round
		// trips in two minutes, and a buff landed once (14 September). Every
		// departure waits for the party to end; a crossing that was under way
		// is dropped rather than resumed from wherever the player has led.
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
		{
			state.lDesertCrossingTo = 0;
			return false;
		}
		// A mercenary's contract is played on the client's map: the client
		// stays for it, and the mercenary until the town wants it - a paused
		// contract lets it go and brings it back itself. A party a bot leads
		// with a person in it keeps its map the way a person's party does.
		if (IsPlayerBotHeldForCompany(ch))
		{
			state.lDesertCrossingTo = 0;
			return false;
		}

		const long mapIndex = ch->GetMapIndex();
		const bool hasMedal = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0;
		// A medal in a medal dropper's bag is stock for its counter, not an errand
		// at the stable: neither village holds such a bot back for one, and its
		// expedition goes on past it (GetPlayerBotDesiredHorseMedalStock).
		// Under Iwakura's system only a medal the stable would take holds a bot
		// back: a Grinder on its first horse carries them as stock for a
		// counter (IsPlayerBotGrinderRider), and a village that held it for one
		// would hold it for good.
		const bool holdsMedalToHandIn = hasMedal &&
				state.bPersonality != BOT_PERSONALITY_MEDAL_DROPPER &&
				(!IsPlayerBotPersonaEnabled() || CanPlayerBotAdvanceHorse(ch));
		// A trader does not down tools to go and farm horse medals in the Monkey
		// Dungeon. That errand takes a bot right across the world for the better
		// part of an hour, and it is exactly the striving this personality exists
		// not to do.
		const bool pursuesHorseExpedition =
				state.bPersonality != BOT_PERSONALITY_MERCHANT &&
				ShouldPlayerBotPursueHorseExpedition(ch, dwNow);
		const bool needsHorseExpedition = pursuesHorseExpedition && !holdsMedalToHandIn;
		// GetWear answers NULL for every slot until the item cache has loaded,
		// which is exactly the state a character is in for the first seconds after
		// a map change. Trusting it there made a bot believe it had lost its
		// weapon the moment it arrived somewhere.
		const bool needsEssentialWeaponSupply = ch->IsItemLoaded() &&
				(ch->GetWear(WEAR_WEAPON) == NULL || NeedsPlayerBotArrows(ch));
		const bool m2LevelingCohort = IsPlayerBotM2LevelingCohort(ch);
		// The door waits after a visit that ran out without the weapon
		// (PLAYERBOT_M3_REVISIT_WAIT_MIN_MS); the M3 dropper and the second
		// tier's Grinder live there.
		const bool wantsM3 = ShouldPlayerBotVisitM3(ch) &&
				(IsPlayerBotM3DropperOnFarm(ch) || IsPlayerBotM3TierGrinder(ch) ||
				 (int)(dwNow - state.dwM3RevisitAfter) >= 0 || state.dwM3RevisitAfter == 0);
		const bool needsCriticalTownServices = NeedsPlayerBotCriticalTownServices(ch);
		const bool needsM1OnlyServices = NeedsPlayerBotM1OnlyServices(ch, state, dwNow);
		// M2 has its own blacksmith. Only the remote M3 farm needs to schedule a
		// return to town for equipment progression.
		const bool scheduledRemoteRefine = IsPlayerBotM3Map(mapIndex) &&
				ShouldPlayerBotLeaveRemoteMapForRefining(ch, state, dwNow);

		// Leaving the Monkey Dungeon is a decision, not a pathfinding exercise.
		// Evaluate it before yielding to an existing victim: a monster near the
		// portal must not keep a finished, timed-out or unequipped bot here forever.
		if (IsPlayerBotMonkeyMap(mapIndex))
		{
			if (state.dwDungeonEnteredTime == 0)
				state.dwDungeonEnteredTime = dwNow;
			const bool visitExpired = dwNow - state.dwDungeonEnteredTime >=
					PLAYERBOT_MONKEY_MAX_VISIT_TIME;
			const int medalCount = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM);
			playerbot_world_rules::TMonkeyVisitContext context;
			context.needsEssentialSupply = needsEssentialWeaponSupply;
			context.needsPotions = NeedsPlayerBotEmergencyPotions(ch);
			context.medalCount = medalCount;
			context.desiredMedalCount = GetPlayerBotDesiredHorseMedalStock(ch);
			context.visitExpired = visitExpired;
			// Re-evaluate the rotating cohort even inside the dungeon. Bots which are
			// no longer selected finish their current medal (if any) and leave instead
			// of occupying the dungeon until its absolute 30-minute timeout. A bot
			// that has levelled into the next dungeon's band leaves for the same
			// reason - the rooms here have stopped paying. The medal dropper farms
			// medals to sell, so its own horse does not gate it.
			const bool bMedalDropper = state.bPersonality == BOT_PERSONALITY_MEDAL_DROPPER;
			context.canAdvanceHorse = pursuesHorseExpedition &&
					(bMedalDropper || CanPlayerBotAdvanceHorse(ch)) &&
					GetPlayerBotMonkeyMapFor(ch) == mapIndex;
			const playerbot_world_rules::EMonkeyExitDecision exitDecision =
					playerbot_world_rules::DecideMonkeyExit(context);
			if (exitDecision != playerbot_world_rules::MONKEY_STAY)
			{
				const char* reason = "monkey_horse_complete_direct";
				if (exitDecision == playerbot_world_rules::MONKEY_EXIT_RESTOCK)
					reason = "monkey_restock_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_MEDAL_READY)
					reason = "monkey_medal_found_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_TIMEOUT)
					reason = "monkey_timeout_direct";
				// Four answers leave through one transition line, and "horse
				// complete" says nothing of which gate closed; the line below
				// is what named the dropper's full bag.
				sys_log(0, "PLAYERBOT_MONKEY: exit pid=%u name=%s map=%ld reason=%s pursues=%d free_cells=%d medals=%d stock=%d visit_s=%u",
						ch->GetPlayerID(), ch->GetName(), mapIndex, reason,
						pursuesHorseExpedition ? 1 : 0, CountPlayerBotFreeInventoryCells(ch),
						medalCount, context.desiredMedalCount,
						(unsigned int)((dwNow - state.dwDungeonEnteredTime) / 1000));
				long homeMap = 0, homeX = 0, homeY = 0;
				if (!GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2,
							homeMap, homeX, homeY))
					return false;
				const bool transitioned = TransitionPlayerBotMap(ch, state,
						homeMap, homeX, homeY, dwNow, reason);
				if (transitioned && medalCount == 0)
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return transitioned;
			}
		}

		// M3 is a focused level-30 weapon farm, not a levelling map. A bot which
		// reaches level 25 graduates immediately, even if an old victim is still
		// alive, and resumes normal progression in M2.
		// ...unless it is there on purpose: the M3 dropper stays to 32, and
		// graduating it at 25 sent it back to Bokjung, where the same rule sent
		// it to M3 again - a hundred and fifty warps an hour per bot.
		// The second tier ends at twenty-five, and whether this bot is its
		// Grinder is not known until the persona flags have arrived.
		if (IsPlayerBotM3Map(mapIndex) && ch->GetLevel() > 24 &&
				(!IsPlayerBotPersonaEnabled() || state.persona.bRestored) &&
				!ShouldPlayerBotVisitM3(ch))
		{
			// The walk does not own the goal - see the desert crossing below.
			long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
			if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M3,
						playerbot_empire_rules::MAP_ROLE_M2, gateX, gateY, destMap, destX, destY))
				return false;
			return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
					destMap, destX, destY, dwNow, "m3_level_graduated");
		}

		// A bot should not walk away from a fight -- but "holds a target" was
		// standing in for "is fighting", and in a dense respawn those are not the
		// same thing at all. Something is always in range, so this check used to
		// return before the routing below had ever been consulted, and the
		// arrival areas of frontier maps quietly became one-way (issue #10).
		//
		// Three cases now. A Metin already under the hammer is always finished
		// first; ShouldPlayerBotAbandonStone releases a stalled one. A live fight
		// holds travel back, but only for a bounded grace period, so an endless
		// chain of packs can no longer outrank the decision to leave. A target
		// merely selected across the field does not delay anything.
		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!victim || victim->IsDead())
			state.dwTravelBlockedSince = 0;
		else
		{
			if (state.dwTravelBlockedSince == 0)
				state.dwTravelBlockedSince = dwNow;
			const bool graceLeft = dwNow - state.dwTravelBlockedSince <
					PLAYERBOT_TRAVEL_FIGHT_GRACE;
			if (victim->IsStone() ||
					(graceLeft && IsPlayerBotEngagedWith(ch, victim, state, dwNow)))
				return false;
		}

		// Crossing the desert between its two gates: a walk that owns the tick,
		// so nothing on the way is fought - the target scan never runs - except
		// a stone worth the level, taken when one stands within reach, finished
		// (the victim rule above holds the walk for a stone) and then the walk
		// goes on. A player heading for V1 does exactly this.
		if (mapIndex == PLAYERBOT_MAP_DESERT && state.lDesertCrossingTo != 0)
		{
			if (dwNow >= state.dwNextCrossingStoneCheck)
			{
				state.dwNextCrossingStoneCheck = dwNow + PLAYERBOT_CROSSING_STONE_CHECK_INTERVAL;
				LPCHARACTER stone = FindPlayerBotStoneNearby(ch, PLAYERBOT_CROSSING_STONE_RANGE);
				if (stone)
				{
					RememberPlayerBotMetin(stone, dwNow);
					ReservePlayerBotMetin(ch, stone, dwNow);
					state.dwTargetVID = stone->GetVID();
					ch->SetVictim(stone);
					ClearPlayerBotRoute(state, true);
					sys_log(0, "PLAYERBOT_WORLD: crossing stone pid=%u name=%s stone_vid=%u level=%u pos=(%ld,%ld)",
							ch->GetPlayerID(), ch->GetName(), (unsigned int)stone->GetVID(),
							stone->GetLevel(), stone->GetX(), stone->GetY());
					return false;
				}
			}
			const bool toV1 = IsPlayerBotSpiderMap(state.lDesertCrossingTo);
			// The walk does not own the goal. This leg, and the four others
			// like it, stamped LEVEL_UP on every tick they ran; the planner put
			// its own answer back five seconds later, and 350 bots on the
			// frontier flipped "zapasy" <-> "poziom" for as long as a crossing
			// took - twelve thousand of twenty thousand goal lines, and a
			// status that read "to town for supplies" and "to the Spider
			// Dungeon" by turns. The planner decides; the walk walks.
			long desertExitX = 0, desertExitY = 0;
			GetPlayerBotFrontierExitFor(ch, PLAYERBOT_MAP_DESERT, desertExitX, desertExitY);
			return MovePlayerBotToWorldPortal(ch, state,
					toV1 ? PLAYERBOT_DESERT_V1_GATE_X : desertExitX,
					toV1 ? PLAYERBOT_DESERT_V1_GATE_Y : desertExitY,
					state.lDesertCrossingTo, state.lDesertCrossingX, state.lDesertCrossingY,
					dwNow, toV1 ? "desert_gate_to_v1" : "desert_gate_to_bokjung");
		}

		if (IsPlayerBotM1Map(mapIndex))
		{
			// Compact and sell an oversized potion reserve before the first trip to
			// M2. Once the bot is already outside M1, excess potions alone must not
			// drag it back across maps; it can keep levelling until a real restock or
			// inventory visit is needed.
			const bool townVisitRecentlyCompleted = state.dwNextShopCheckTime != 0 &&
					dwNow < state.dwNextShopCheckTime;
			const bool needsAnyRefine = HasPlayerBotRefineOpportunity(ch);
			const bool needsTownPreparation = NeedsPlayerBotPotions(ch) ||
					CountPlayerBotJunkItems(ch) >= 12 || needsAnyRefine;
			// The soft needs get one town visit to be met. If the bot has just
			// been shopping and still wants something, the town cannot supply it,
			// and standing here is worse than moving on.
			// A herb row of the Biologist is hunted here and nowhere else, so it
			// holds the bot exactly as the errand that brought it (see
			// PlayerBotHuntsVillageHerbs for the four-second Joan <-> Bokjung loop).
			if (holdsMedalToHandIn || BlocksPlayerBotTravel(ch) || needsM1OnlyServices ||
					PlayerBotHuntsVillageHerbs(ch) || HasPlayerBotExcessPotions(ch) ||
					((needsTownPreparation || needsCriticalTownServices) &&
					 !townVisitRecentlyCompleted))
				return false;
			// The M2 cohort ends at 35, but the frontier maps begin at 30 and run
			// far past it. Testing only the cohort here meant a level 36+ bot fell
			// out of this gate on every tick and stayed in Joan for good, hunting
			// level-3 wolves - which is the "boty bija psy" the Discord keeps
			// reporting. Anything with somewhere better to be gets through.
			if (!needsHorseExpedition && !m2LevelingCohort && !wantsM3 &&
					GetPlayerBotFrontierMapForLevel(ch) == 0 &&
					!IsPlayerBotPastM2Ceiling(ch))
				return false;
			if (state.dwNextWorldTravelTime == 0)
			{
				const bool graduatedFromM1 = ch->GetLevel() >= 22 && !needsHorseExpedition;
				const DWORD minDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MIN_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MIN_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MIN_DELAY);
				const DWORD maxDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MAX_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MAX_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MAX_DELAY);
				const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x54524156U) %
						(maxDelay - minDelay + 1);
				state.dwNextWorldTravelTime = dwNow + minDelay + spread;
				return false;
			}
			if (dwNow < state.dwNextWorldTravelTime)
				return false;

			// Joan has a Teleporter of its own, so a bot which has already outgrown
			// Bokjung can leave for the frontier directly. Routing it through M2
			// first would queue it behind every town errand in the village, which
			// is what left Orc Valley empty while the Desert filled up from the
			// bots that happened to already be in Bokjung.
			// Let it fish before it is handed the next hunting destination.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
				return false;

			if (!needsHorseExpedition && !wantsM3)
			{
				const long directMap = ShouldPlayerBotLeaveForFrontier(ch)
						? GetPlayerBotFrontierMapForLevel(ch) : 0;
				if (directMap != 0)
				{
					long arriveX = 0, arriveY = 0;
					GetPlayerBotFrontierArrivalFor(ch, directMap, arriveX, arriveY);
					long teleX = 0, teleY = 0;
					if (!GetPlayerBotLocalTeleporter(ch, teleX, teleY))
						return false;
					char reason[48];
					snprintf(reason, sizeof(reason), "m1_direct_to_%s", GetPlayerBotFrontierName(directMap));
					return MovePlayerBotToWorldPortal(ch, state, teleX, teleY,
							directMap, arriveX, arriveY, dwNow, reason);
				}
			}

			long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
			if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M1,
						playerbot_empire_rules::MAP_ROLE_M2, gateX, gateY, destMap, destX, destY))
				return false;
			return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
					destMap, destX, destY,
					dwNow, needsHorseExpedition ? "horse_to_m2" : "level_to_m2");
		}

		if (IsPlayerBotM2Map(mapIndex))
		{
			// ManagePlayerBotHorse owns the medal on M2 and walks to the local Stable
			// Boy. Returning to M1 here was the source of the needless three-map trip.
			if (holdsMedalToHandIn)
				return false;

			// Profession trainers and the Biologist only exist in Joan. Routine gear,
			// potion, inventory and refine needs are served by the real Bokjung NPCs.
			if (needsM1OnlyServices)
			{
				long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
				if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M2,
							playerbot_empire_rules::MAP_ROLE_M1, gateX, gateY, destMap, destX, destY))
					return false;
				return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
						destMap, destX, destY, dwNow, "m1_only_service");
			}
			// Same rule in Bokjung: its own shops own this need, but only until a
			// visit has actually happened. Otherwise a bot the town cannot equip
			// would never reach the frontier maps either.
			// The intent to leave outlives the errand that holds it up. The
			// audit's point: a higher-priority purchase defers the departure,
			// it does not cancel it, and after the visit the traveller comes
			// straight back rather than waiting for another roll of ambition.
			// A trip for a medal is not held back by the soft half of that list.
			// It counts a bag at 45% as critical, and a keeper's bag is its
			// counter's stock: the medal droppers carried fifty to seventy of
			// their ninety cells, twenty-four of them materials, and three of
			// 116 got into the dungeon in twenty-five minutes. What stops a fight
			// still holds, and a full bag or the last potion ends the errand on
			// its own (ShouldPlayerBotPursueHorseExpedition, DecideMonkeyExit).
			if (BlocksPlayerBotTravel(ch) ||
					(needsCriticalTownServices && !needsHorseExpedition &&
						(state.dwNextShopCheckTime == 0 || dwNow < state.dwNextShopCheckTime)))
			{
				const long wantMap = GetPlayerBotFrontierMapForLevel(ch);
				if (wantMap != 0 && state.lDepartureMap != wantMap)
				{
					state.lDepartureMap = wantMap;
					state.dwDepartureSince = dwNow;
					state.dwNextDepartureLogTime = dwNow + PLAYERBOT_DEPARTURE_OVERDUE_MS;
					sys_log(0, "PLAYERBOT_DEPARTURE: held pid=%u name=%s level=%u to=%ld reason=%s",
							ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), wantMap,
							BlocksPlayerBotTravel(ch) ? "gear_or_potions" : "town_visit");
				}
				// A departure held for longer than an errand takes is a bot the
				// audit called "krazy po M1 mimo celu Pustynia". Every ten
				// minutes it says what is still holding it, with the numbers the
				// hold is made of, so the next report names a cause.
				else if (state.lDepartureMap != 0 && state.dwDepartureSince != 0 &&
						dwNow >= state.dwNextDepartureLogTime)
				{
					state.dwNextDepartureLogTime = dwNow + PLAYERBOT_DEPARTURE_OVERDUE_MS;
					sys_log(0, "PLAYERBOT_DEPARTURE: overdue pid=%u name=%s level=%u to=%ld held_ms=%u map=%ld pos=(%ld,%ld) weapon=%d body=%d potions_short=%d arrows_short=%d bag_3cell=%d critical_services=%d shop_check_in_ms=%d service_pending=%d gold=%lld",
							ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), state.lDepartureMap,
							dwNow - state.dwDepartureSince, ch->GetMapIndex(), ch->GetX(), ch->GetY(),
							ch->GetWear(WEAR_WEAPON) != NULL, ch->GetWear(WEAR_BODY) != NULL,
							NeedsPlayerBotEmergencyPotions(ch) ? 1 : 0, NeedsPlayerBotArrows(ch) ? 1 : 0,
							ch->GetEmptyInventory(3) >= 0, needsCriticalTownServices ? 1 : 0,
							state.dwNextShopCheckTime > dwNow ? (int)(state.dwNextShopCheckTime - dwNow) : 0,
							state.bServicePending ? 1 : 0, (long long)ch->GetGold());
				}
				return false;
			}

			if (needsHorseExpedition)
			{
				// Honour the rest period set by a failed/timed-out expedition and
				// stagger fresh M2 populations after a restart. Without this guard a
				// direct exit was followed by an immediate direct re-entry.
				if (state.dwNextWorldTravelTime == 0)
				{
					const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d4f4e4bU) %
							(PLAYERBOT_HORSE_TRAVEL_MAX_DELAY -
							 PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + 1);
					state.dwNextWorldTravelTime = dwNow +
							PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + spread;
					return false;
				}
				if (playerbot_world_rules::IsTravelCooldownActive(
						dwNow, state.dwNextWorldTravelTime))
					return false;
				const long monkeyMap = GetPlayerBotMonkeyMapFor(ch);
				if (monkeyMap == 0)
					return false;
				long monkeyX = 0, monkeyY = 0;
				GetPlayerBotMonkeyArrival(monkeyMap, monkeyX, monkeyY);
				char reason[48];
				snprintf(reason, sizeof(reason), "horse_to_monkey_%s", GetPlayerBotMonkeyName(monkeyMap));
				long gateX = 0, gateY = 0, easyMap = 0, easyX = 0, easyY = 0;
				if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M2,
							playerbot_empire_rules::MAP_ROLE_MONKEY_EASY,
							gateX, gateY, easyMap, easyX, easyY))
					return false;
				// The easy dungeon is entered by that kingdom's own gate, and
				// each kingdom has one of its own; the harder two are shared and
				// are reached from inside the maze, so their arrival stands.
				const bool toEasy = monkeyMap == easyMap;
				return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
						toEasy ? easyMap : monkeyMap,
						toEasy ? easyX : monkeyX,
						toEasy ? easyY : monkeyY, dwNow, reason);
			}

			if (wantsM3 && !needsCriticalTownServices)
			{
				// The same Teleporter, the same wait and the same fare.
				if (playerbot_world_rules::IsTravelCooldownActive(dwNow, state.dwNextWorldTravelTime) ||
						ch->GetGold() < GetPlayerBotTeleporterFee(ch))
					return false;
				long teleX = 0, teleY = 0;
				playerbot_empire_rules::TPoint arrival;
				const int empire = GetPlayerBotRoadsEmpire(ch);
				if (!GetPlayerBotLocalTeleporter(ch, teleX, teleY) ||
						!playerbot_empire_rules::GetTeleportArrival(empire,
							playerbot_empire_rules::TELEPORT_GUILD_MAP, arrival))
					return false;
				return MovePlayerBotToWorldPortal(ch, state, teleX, teleY,
						playerbot_empire_rules::GetHomeMap(empire, playerbot_empire_rules::MAP_ROLE_M3),
						arrival.x, arrival.y, dwNow,
						IsPlayerBotM3TierGrinder(ch) ? "tier2_grinder_to_m3" : "level30_weapon_to_m3");
			}

			// The river is in Joan, and every bot old enough to hold a rod has long
			// since left it - so fishing only ever happens if the trip is a real
			// destination. Ranked above the frontier maps: a session is short, and
			// the pearls it brings back are worth more than the hunting it skips.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
			{
				long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
				if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M2,
							playerbot_empire_rules::MAP_ROLE_M1, gateX, gateY, destMap, destX, destY))
					return false;
				return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
						destMap, destX, destY, dwNow, "fishing_to_m1");
			}

			// Bokjung's own spawns stop paying long before the M2 band ends. Bots
			// which have outgrown them move on to Orc Valley and then the Desert
			// instead of grinding monsters they would rather walk past.
			const long frontierMap = ShouldPlayerBotLeaveForFrontier(ch)
					? GetPlayerBotFrontierMapForLevel(ch) : 0;
			if (frontierMap != 0)
			{
				// The Teleporter's refusal sets dwNextWorldTravelTime and this
				// branch never read it: 1.30.42 announced a five-minute wait and
				// the bot asked again on the next tick, 26 000 times a minute
				// across the cohort. A bot short of the fare is not sent either;
				// the grind rule above lets it earn the fare where it stands.
				if (playerbot_world_rules::IsTravelCooldownActive(dwNow, state.dwNextWorldTravelTime) ||
						ch->GetGold() < GetPlayerBotTeleporterFee(ch))
					return false;
				long arriveX = 0, arriveY = 0;
				GetPlayerBotFrontierArrivalFor(ch, frontierMap, arriveX, arriveY);
				long teleX = 0, teleY = 0;
				if (!GetPlayerBotLocalTeleporter(ch, teleX, teleY))
					return false;
				char reason[48];
				snprintf(reason, sizeof(reason), "level_to_%s", GetPlayerBotFrontierName(frontierMap));
				return MovePlayerBotToWorldPortal(ch, state, teleX, teleY,
						frontierMap, arriveX, arriveY, dwNow, reason);
			}

			// Sending a bot back to Joan is only right when it has outgrown
			// nothing yet. Doing it above the ceiling produced an M1 <-> M2 loop:
			// Joan let it leave, Bokjung refused to keep it, and neither ever
			// hunted anything.
			if (!m2LevelingCohort && !IsPlayerBotPastM2Ceiling(ch))
			{
				long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
				if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M2,
							playerbot_empire_rules::MAP_ROLE_M1, gateX, gateY, destMap, destX, destY))
					return false;
				const bool moving = MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
						destMap, destX, destY, dwNow, "m2_level_range_complete");
				if (IsPlayerBotM1Map(ch->GetMapIndex()))
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return moving;
			}

			return false; // designated M2 leveler: hunt normally
		}

		if (IsPlayerBotM3Map(mapIndex))
		{
			if (state.dwM3EnteredTime == 0)
				state.dwM3EnteredTime = dwNow;
			// The second tier's Grinder is home here and has no visit to run
			// out: it leaves when it no longer belongs - past the tier, moved on
			// as a Conqueror, over the crowd, a full bag.
			const bool tierGrinder = IsPlayerBotM3TierGrinder(ch);
			const bool visitExpired = tierGrinder ? !ShouldPlayerBotVisitM3(ch)
					: dwNow - state.dwM3EnteredTime >= PLAYERBOT_M3_MAX_VISIT_TIME;
			// An open town visit is reason enough to leave, critical or not.
			//
			// M3 is a guild map: no merchant, no blacksmith, no trainer. A bot
			// that decided in Bokjung to go shopping and then came here could
			// neither shop - HandlePlayerBotTownVisit refuses on any map but M1
			// and M2 - nor leave, because only a *critical* need opened this
			// gate. So it stood on the arrival point with "going to town for
			// supplies" over its head until the twenty-minute visit timer ran
			// out. Two of them were photographed doing exactly that.
			// The weapon is what everybody else came for; the M3 dropper came
			// for the ones it will sell (see IsPlayerBotM3DropperOnFarm).
			const bool weaponFound = !IsPlayerBotM3DropperOnFarm(ch) && !tierGrinder &&
					HasPlayerBotSpecialLevel30Weapon(ch, true);
			if (!visitExpired && !state.bVisitingShop &&
					!needsCriticalTownServices && !needsM1OnlyServices &&
					!scheduledRemoteRefine && !weaponFound)
				return false;

			// The walk does not own the goal - see the desert crossing above. This
			// line stamped "poziom" on every tick a bot left the frontier for a
			// non-critical errand, against the planner's "zapasy" five seconds later.
			const char* reason = "m3_weapon_found";
			if (needsCriticalTownServices || needsM1OnlyServices)
				reason = "m3_services_to_m2";
			else if (state.bVisitingShop)
				reason = "m3_shopping_to_m2";
			else if (scheduledRemoteRefine)
				reason = "m3_scheduled_refine_to_m2";
			else if (visitExpired)
				reason = "m3_visit_complete";
			// A visit that ran out without the weapon closes the door for a
			// while, so the M2 branch gives the valley, the horse and the river
			// their turn instead of sending the bot straight back here.
			if (visitExpired && !weaponFound && !IsPlayerBotM3DropperOnFarm(ch) && !tierGrinder &&
					(state.dwM3RevisitAfter == 0 || (int)(dwNow - state.dwM3RevisitAfter) >= 0))
			{
				state.dwM3RevisitAfter = dwNow + PLAYERBOT_M3_REVISIT_WAIT_MIN_MS +
						PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d335257U) %
						(PLAYERBOT_M3_REVISIT_WAIT_MAX_MS - PLAYERBOT_M3_REVISIT_WAIT_MIN_MS);
				sys_log(0, "PLAYERBOT_WORLD: m3 visit ran out without the weapon pid=%u name=%s level=%u back_in_min=%u",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(),
						(unsigned int)((state.dwM3RevisitAfter - dwNow) / 60000));
			}
			long gateX = 0, gateY = 0, destMap = 0, destX = 0, destY = 0;
			if (!GetPlayerBotKingdomLeg(ch, playerbot_empire_rules::MAP_ROLE_M3,
						playerbot_empire_rules::MAP_ROLE_M2, gateX, gateY, destMap, destX, destY))
				return false;
			return MovePlayerBotToWorldPortal(ch, state, gateX, gateY,
					destMap, destX, destY, dwNow, reason);
		}

		if (IsPlayerBotFrontierMap(mapIndex))
		{
			if (state.dwFrontierEnteredTime == 0)
				state.dwFrontierEnteredTime = dwNow;
			const DWORD stayed = dwNow - state.dwFrontierEnteredTime;
			// The battle-horse trial is a hundred kills of two archers on this one
			// map; everything below that would send the bot home before the
			// hundredth waits for it (2.0.66, 2.0.67).
			const bool onBattleTrialHere = mapIndex == PLAYERBOT_MAP_DESERT &&
					IsPlayerBotOnBattleHorseTrial(ch);
			// The personality's visit clock ended a trial two-thirds done
			// ("frontier_visit_complete" after 41 minutes, m2zip 17 September).
			const bool visitExpired = !onBattleTrialHere && stayed >=
					GetPlayerBotFrontierVisitTime(state.bPersonality);
			// Two minutes of actually playing here before anything but a real
			// emergency may send the bot home again.
			const bool settledIn = stayed >= PLAYERBOT_FRONTIER_MIN_VISIT_TIME;
			// Outgrowing the map matters as much as running out of potions: neither
			// Orc Valley nor the Desert has a merchant, a blacksmith or a trainer.
			const bool outOfBand = GetPlayerBotFrontierMapForLevel(ch) != mapIndex;
			// Only a need that actually stops the bot playing is worth the trip
			// home. Sending it back for a helmet the town cannot sell turned the
			// journey into a shuttle: it arrived, saw the same unmet need, and
			// left again without ever fighting anything here.
			// A blocking need still wins immediately - a bot with no weapon left
			// cannot wait out a timer. Everything else waits until the bot has
			// been here long enough for the trip to have been worth making.
			// A trial bot is blocked by what stops the fight - no weapon, no
			// armour, no potions, no arrows - and not by the bag with no free
			// three-cell column that stops a pickup: an assassin of fifty-two
			// with a full belt and sixteen free cells came home from the desert
			// five times in forty minutes for that column, 97 to 426 s a stay
			// (GumbASSx, m2zip 17 September), and the town visit could not make
			// one either.
			const bool blocked = onBattleTrialHere
					? (ch->IsItemLoaded() &&
						(ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
						 NeedsPlayerBotEmergencyPotions(ch) || NeedsPlayerBotArrows(ch)))
					: BlocksPlayerBotTravel(ch);
			// The Biologist hand-in a trial bot carries sent it home for the
			// hand-in every few minutes: 75 desert stays of 344 s on average in
			// an hour, 67 under ten minutes, the trial's kills 25 at a time half
			// an hour apart, and 102 of 120 trial bots at 0/100 in the villages
			// (m2zip, 17 September). The hand-in waits for the horse.
			const bool needsTown = blocked ||
					(settledIn && ((needsM1OnlyServices && !onBattleTrialHere) || needsEssentialWeaponSupply));
			// The Monkey Dungeons are reached from Bokjung, and nothing here ever
			// went back for one: the roll that sends a bot for a medal was only
			// read in town, and a bot past forty lives out here - which is how
			// 439 of them came to have no horse. A party is not broken up for it;
			// the trip is taken alone, the party re-forms on the way back.
			const bool wantsMedal = settledIn && needsHorseExpedition &&
					ch->GetParty() == NULL;
			// And the level-30 weapon, for the same reason as the medal: the
			// decision to farm it was only ever read in town, and a bot that
			// reached the frontier without one had no way back to the farm.
			// Handed to M2, where the ordinary M3 branch takes over.
			const bool wantsWeapon = settledIn && !wantsMedal &&
					ShouldPlayerBotVisitM3(ch) && ch->GetParty() == NULL;
			// And the river, for the same reason again: the fishing trip was
			// read in the two villages alone, and every bot old enough for the
			// rod lives out here - so the FISHING slider moved a handful of bots
			// of thirty to thirty-five and nobody else ("ustawilem 200%, a liczba
			// rybakow nie rosnie", blasty, Community Patch 2). A Rybak's roll
			// (IsPlayerBotRybakNow) now takes it home to the bank from anywhere;
			// a trial and a party are not interrupted for it.
			const bool wantsFishing = settledIn && !onBattleTrialHere && !wantsMedal && !wantsWeapon &&
					ch->GetParty() == NULL && WantsPlayerBotFishingTrip(ch, state, dwNow);
			if (!visitExpired && !outOfBand && !needsTown && !wantsMedal && !wantsWeapon && !wantsFishing)
				return false;

			// A share of the bots keeps Joan as home: the services trip goes
			// there, and only the services trip - a medal, a weapon hunt and
			// a graduation are Bokjung's business.
			const bool joanHome = (needsTown &&
					(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4a4f414eU) % 1000U) <
						(DWORD)PLAYERBOT_JOAN_HOME_PER_MILLE) ||
					// The bank, the bait merchant and the Fisherman are all in
					// the first village.
					(!needsTown && !outOfBand && wantsFishing);
			const char* reason = "frontier_visit_complete";
			if (joanHome && wantsFishing && !needsTown && !outOfBand)
				reason = "frontier_fishing_to_m1";
			else if (joanHome)
				reason = "frontier_services_to_m1";
			else if (needsTown)
				reason = "frontier_services_to_m2";
			else if (outOfBand)
				reason = "frontier_level_graduated";
			else if (wantsMedal)
				reason = "frontier_horse_to_m2";
			else if (wantsWeapon)
				reason = "frontier_weapon_to_m2";
			long exitX = 0, exitY = 0;
			GetPlayerBotFrontierExitFor(ch, mapIndex, exitX, exitY);
			long destMap = 0, destX = 0, destY = 0;
			if (!GetPlayerBotVillageReturn(ch, joanHome ? playerbot_empire_rules::MAP_ROLE_M1
						: playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
				return false;
			// Out of potions or a weapon far from town: the ring recalls now.
			if (blocked && TryPlayerBotTeleportRingHome(ch, state, dwNow, destMap, destX, destY, reason))
				return true;
			return MovePlayerBotToWorldPortal(ch, state, exitX, exitY,
					destMap, destX, destY, dwNow, reason);
		}

		if (IsPlayerBotMonkeyMap(mapIndex))
			return false; // stay and fight; departure was handled before victim yielding

		// Anything else means the bot was moved somewhere no branch above owns:
		// see IsPlayerBotPositionOffItsMap for the case where the move only half
		// happened and the map index still reads as the one the bot is standing
		// on. Both end here, and both are put back in Bokjung.
		// the frontier maps carry real warp NPCs of their own, and walking inside
		// one's trigger radius hands the character to a map the AI has no plan
		// for. Nothing would ever bring it back, so it would sit there for good.
		long strandedMap = 0, strandedX = 0, strandedY = 0;
		if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2,
					strandedMap, strandedX, strandedY) &&
				TransitionPlayerBotMap(ch, state, strandedMap, strandedX, strandedY,
					dwNow, "stranded_recovery"))
		{
			sys_log(0, "PLAYERBOT_WORLD: recovered pid=%u name=%s from unmanaged map=%ld",
					ch->GetPlayerID(), ch->GetName(), mapIndex);
			return true;
		}
		return false;
	}
}

#endif
