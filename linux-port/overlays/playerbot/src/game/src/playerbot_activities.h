#ifndef __INC_METIN2_PLAYERBOT_ACTIVITIES_H__
#define __INC_METIN2_PLAYERBOT_ACTIVITIES_H__

// The things a bot does that are not fighting: raising a horse, and fishing.
//
// Both own the whole tick while they run - the rod sits in the weapon slot, and
// a bot walking to the stable is not hunting - which is why they read as
// separate activities rather than as steps inside the combat loop.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_movement.h (it walks) and playerbot_gear.h (it buys).

namespace
{
	BYTE GetPlayerBotNextHorseRequiredLevel(BYTE horseLevel)
	{
		if (horseLevel >= 21)
			return 255;
		if (horseLevel >= 20)
			return 50; // military horse milestone
		if (horseLevel >= 10)
			return 35; // combat horse milestone
		return PLAYERBOT_HORSE_REQUIRED_LEVEL;
	}

	bool CanPlayerBotAdvanceHorse(LPCHARACTER ch)
	{
		return ch && ch->GetHorseLevel() < 21 &&
				ch->GetLevel() >= GetPlayerBotNextHorseRequiredLevel(ch->GetHorseLevel());
	}

	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	bool ManagePlayerBotHorse(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2) || state.bVisitingShop ||
				state.bVisitingBiologist)
			return false;
		if (!state.bVisitingStable && dwNow < state.dwNextHorseCheckTime)
			return false;
		if (!state.bVisitingStable)
			state.dwNextHorseCheckTime = dwNow + 3000;

		const BYTE horseLevel = ch->GetHorseLevel();
		const bool bBattleHorseWaiting = IsPlayerBotBattleHorseEarned(ch) &&
				ch->GetGold() >= (int)PLAYERBOT_BATTLE_HORSE_FEE;
		if (!bBattleHorseWaiting && (!CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0))
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			return false;
		}

		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!state.bVisitingStable && victim && !victim->IsDead())
			return false;

		if (!state.bVisitingStable)
		{
			int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
			if (delivered < horseLevel)
			{
				delivered = horseLevel;
				ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
			}
			state.bVisitingStable = true;
			state.dwNextHorseActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_HORSE: going to stable pid=%u name=%s medals=%d horse_level=%u delivered=%d",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM), horseLevel, delivered);
		}

		SetPlayerBotGoal(ch, state, BOT_GOAL_HORSE, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_STABLE, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;
		const long stableX = inM2 ? PLAYERBOT_M2_STABLE_BOY_X : PLAYERBOT_STABLE_BOY_X;
		const long stableY = inM2 ? PLAYERBOT_M2_STABLE_BOY_Y : PLAYERBOT_STABLE_BOY_Y;
		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), stableX, stableY,
				inM2 ? 0x4d324853U : 0x484f5253U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingStable = false;
				state.dwNextHorseCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_HORSE: route failed pid=%u name=%s map=%ld from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "stable_interaction");
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextHorseActionTime == 0)
		{
			state.dwNextHorseActionTime = dwNow + number(5000, 15000);
			return true;
		}
		if (dwNow < state.dwNextHorseActionTime)
			return true;

		// The trial first: a bot that has earned the battle horse is here to
		// collect it, not to hand in a medal it does not have.
		if (CollectPlayerBotBattleHorse(ch))
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(15000, 30000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		ch->RemoveSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM, 1);
		int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
		delivered = std::max(delivered, (int)ch->GetHorseLevel()) + 1;
		delivered = std::min(delivered, 21);
		ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
		ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_DELIVERY_TIME_FLAG, get_global_time());
		if (ch->GetHorseLevel() < delivered)
			ch->SetHorseLevel(delivered);
		ch->SetSkillLevel(131, 10);

		const char* stage = delivered >= 21 ? "military" :
				(delivered >= 11 ? "combat" : "normal");
		sys_log(0, "PLAYERBOT_HORSE: medal delivered pid=%u name=%s delivered=%d horse_level=%u stage=%s medals_left=%d",
				ch->GetPlayerID(), ch->GetName(), delivered, ch->GetHorseLevel(), stage,
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM));

		if (delivered >= 21 || !CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		state.dwNextHorseActionTime = dwNow + number(5000, 10000);
		return true;
	}

	// A small, stable slice of the M1 population fishes. Careful collectors are the
	// natural anglers -- pearls are a collector's prize -- but a few other
	// personalities join them so the bank is never one archetype deep. The roll is
	// derived from the player id, so a bot keeps the same hobby across restarts.
	// The rod is a weapon and carries a level limit like any other. Asking the
	// item what it needs keeps this honest: the hand-picked level 10 was below
	// the rod's real requirement of 30, so a level-13 bot bought tackle it could
	// never equip and then retried the same failing step for good.
	bool CanPlayerBotUseFishingRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_ROD_VNUM);
		if (!proto)
			return false;
		return GetPlayerBotProtoLevelLimit(proto) <= (int)ch->GetLevel();
	}

	bool IsPlayerBotAngler(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!CanPlayerBotUseFishingRod(ch))
			return false;
		const DWORD roll = PlayerBotNavHash(ch->GetPlayerID() ^ 0x46495348U) % 100U;
		// Twenty collectors in a hundred and two of everyone else, stretched or
		// shrunk by the FISHING weight. At the neutral 100 the two thresholds are
		// exactly the ones this has always used.
		const int chance = state.bPersonality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 20 : 2;
		return PlayerBotWeightedRoll(roll, chance, PLAYERBOT_WEIGHT_FISHING);
	}

	// Anglers spread out along the shoreline rather than stacking on one tile.
	// The band is walked along Y because that is the way this stretch of bank
	// runs; every resulting point is inside the verified standable rectangle.
	void GetPlayerBotFishingStand(DWORD playerID, long& standX, long& standY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ 0x42414e4bU);
		standX = PLAYERBOT_FISHING_BANK_X + (long)(hash % 5U) * 50;
		standY = PLAYERBOT_FISHING_BANK_Y + (long)((hash / 5U) % 10U) * 50;
	}

	bool IsPlayerBotHoldingRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return rod && rod->GetType() == ITEM_ROD;
	}

	bool EquipPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotHoldingRod(ch))
			return true;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_ROD)
				continue;

			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && !ch->UnequipItem(worn))
				return false;
			if (ch->EquipItem(item))
			{
				sys_log(0, "PLAYERBOT_FISHING: rod equipped pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum());
				return true;
			}
		}
		return false;
	}

	// The rod is worthless in a fight, so a finished session always puts the real
	// weapon back before the bot rejoins the grind.
	void StowPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return;
		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!rod || rod->GetType() != ITEM_ROD)
			return;
		if (!ch->UnequipItem(rod))
			return;
		EquipFirstAvailablePlayerBotWeapon(ch);
	}

	// Bait does not sit in the pouch while fishing: using it moves its value into
	// the rod's socket 2, which is what the engine actually checks before a cast.
	bool BaitPlayerBotRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		if (!rod || rod->GetType() != ITEM_ROD)
			return false;
		if (rod->GetSocket(2) != 0)
			return true;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_FISHING_BAIT_VNUM)
				continue;
			ch->UseItem(TItemPos(INVENTORY, cell));
			return rod->GetSocket(2) != 0;
		}
		return false;
	}

	// One item per pass. Gutting a fish and prying a shell open both run through
	// UseItem, which frees the very inventory slot being iterated over.
	bool ProcessPlayerBotCatch(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD vnum = item->GetVnum();
			const bool aliveFish = item->GetType() == ITEM_FISH &&
					item->GetSubType() == FISH_ALIVE;
			if (!aliveFish && vnum != PLAYERBOT_SHELLFISH_VNUM)
				continue;
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;

			sys_log(0, "PLAYERBOT_FISHING: processed catch pid=%u name=%s vnum=%u kind=%s",
					ch->GetPlayerID(), ch->GetName(), vnum,
					aliveFish ? "fish" : "shellfish");
			return true;
		}
		return false;
	}

	bool EndPlayerBotFishingSession(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		if (ch && ch->m_pkFishingEvent)
			ch->fishing_take();

		state.bFishingSession = false;
		state.bIsFishing = false;
		state.dwFishingCastTime = 0;
		state.dwFishingSessionEndTime = 0;
		state.dwNextFishingActionTime = 0;
		state.dwNextFishingCheckTime = dwNow +
				number(PLAYERBOT_FISHING_REST_MIN, PLAYERBOT_FISHING_REST_MAX);
		StowPlayerBotRod(ch);
		ClearPlayerBotRoute(state, true);
		if (ch)
			sys_log(0, "PLAYERBOT_FISHING: session over pid=%u name=%s pearls=%d/%d/%d reason=%s",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM + 1),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_LAST_VNUM),
					reason ? reason : "?");
		return false;
	}

	// Rod and bait both come from the Rybak, who stands on the bank the bots fish
	// from, so restocking and fishing share one walk.
	bool RestockPlayerBotTackle(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;

		bool bought = false;
		if (!IsPlayerBotHoldingRod(ch) && ch->CountSpecifyItem(PLAYERBOT_FISHING_ROD_VNUM) <= 0)
		{
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_ROD_VNUM);
			if (!proto)
				return false;
			const long long price = GetPlayerBotNpcPurchasePrice(proto, 1);
			if (ch->GetGold() < price)
				RaisePlayerBotEmergencyGold(ch, price, "fishing_rod");
			if (price <= 0 || ch->GetGold() < price)
				return false;
			if (!ch->AutoGiveItem(PLAYERBOT_FISHING_ROD_VNUM, 1, -1, false))
				return false;
			ch->PointChange(POINT_GOLD, -price);
			bought = true;
			sys_log(0, "PLAYERBOT_FISHING: bought rod pid=%u name=%s vnum=%u price=%lld",
					ch->GetPlayerID(), ch->GetName(), PLAYERBOT_FISHING_ROD_VNUM, price);
		}

		if (ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) < PLAYERBOT_FISHING_BAIT_RESTOCK)
		{
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_BAIT_VNUM);
			if (!proto)
				return bought;
			const long long price = GetPlayerBotNpcPurchasePrice(proto, PLAYERBOT_FISHING_BAIT_BUNDLE);
			if (ch->GetGold() < price)
				RaisePlayerBotEmergencyGold(ch, price, "fishing_bait");
			if (price <= 0 || ch->GetGold() < price)
				return bought;
			if (!ch->AutoGiveItem(PLAYERBOT_FISHING_BAIT_VNUM, PLAYERBOT_FISHING_BAIT_BUNDLE, -1, false))
				return bought;
			ch->PointChange(POINT_GOLD, -price);
			bought = true;
			sys_log(0, "PLAYERBOT_FISHING: bought bait pid=%u name=%s vnum=%u count=%d price=%lld",
					ch->GetPlayerID(), ch->GetName(), PLAYERBOT_FISHING_BAIT_VNUM,
					PLAYERBOT_FISHING_BAIT_BUNDLE, price);
		}
		return bought;
	}

	bool ManagePlayerBotFishing(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		if (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1)
		{
			// The rod must not travel to a hunting map in the weapon slot.
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "left_m1");
			return false;
		}
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive)
		{
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "town_errand");
			return false;
		}

		if (!state.bFishingSession)
		{
			if (dwNow < state.dwNextFishingCheckTime || !IsPlayerBotAngler(ch, state))
				return false;
			// Never walk off mid-fight; finish the pack first.
			LPCHARACTER victim = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (victim && !victim->IsDead())
				return false;

			state.bFishingSession = true;
			state.bIsFishing = false;
			state.dwFishingCastTime = 0;
			state.dwNextFishingActionTime = 0;
			state.dwNextFishingProgressLogTime = 0;
			state.dwFishingSessionEndTime = dwNow +
					number(PLAYERBOT_FISHING_SESSION_MIN, PLAYERBOT_FISHING_SESSION_MAX);
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_FISHING: heading for the bank pid=%u name=%s level=%u personality=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bPersonality);
		}

		// A session only ends between casts, so a fish already on the hook is
		// still landed.
		if (dwNow >= state.dwFishingSessionEndTime && !state.bIsFishing)
			return EndPlayerBotFishingSession(ch, state, dwNow, "session_finished");

		SetPlayerBotGoal(ch, state, BOT_GOAL_FISHING, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_FISHING, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Rod first, then worms: both come from the Rybak, who stands a short walk
		// upstream of the bank. Running out of bait sends the bot back to him.
		const bool needsTackle =
				(!IsPlayerBotHoldingRod(ch) &&
					ch->CountSpecifyItem(PLAYERBOT_FISHING_ROD_VNUM) <= 0) ||
				ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
					PLAYERBOT_FISHING_BAIT_RESTOCK;

		long destX = 0, destY = 0;
		if (needsTackle)
		{
			GetPlayerBotNpcApproach(ch->GetPlayerID(), PLAYERBOT_FISHERMAN_X,
					PLAYERBOT_FISHERMAN_Y, 0x46495348U, destX, destY);
		}
		else
		{
			GetPlayerBotFishingStand(ch->GetPlayerID(), destX, destY);
			// The bank spots are hand-picked world coordinates. server_attr is the
			// only authority on whether one is standable, and the town services
			// already learned that a hand-picked point can be a cell the navigation
			// refuses. Snap to a verified walkable cell before walking at it.
			CPlayerBotNavigation& navigation =
					CPlayerBotNavigation::instance(ch->GetMapIndex());
			PIXEL_POSITION bank;
			if (navigation.Init(ch->GetMapIndex()) &&
					navigation.FindNearestWalkableWorld(destX, destY, 12, bank,
							ch->GetPlayerID()))
			{
				destX = bank.x;
				destY = bank.y;
			}
		}

		// bFishingSession exempts a bot from the inactivity watchdog - standing
		// still at the bank is the activity - which also means a session that goes
		// wrong is completely silent. One throttled line says where it actually is.
		if (dwNow >= state.dwNextFishingProgressLogTime)
		{
			state.dwNextFishingProgressLogTime = dwNow + PLAYERBOT_FISHING_PROGRESS_LOG;
			sys_log(0, "PLAYERBOT_FISHING: progress pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) dist=%ld tackle=%d rod=%d bait=%d casting=%d stuck=%u riding=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY,
					(long)DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY),
					needsTackle ? 1 : 0, IsPlayerBotHoldingRod(ch) ? 1 : 0,
					ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM),
					state.bIsFishing ? 1 : 0, (unsigned int)state.bStuckCounter,
					ch->IsRiding() ? 1 : 0);
		}

		// A session that never reaches the water is the worst of both worlds: the
		// bot has paid for tackle, stopped hunting, and walks the same failing
		// approach for as long as the server runs. Observed on a live world - a
		// level-34 bot stood at the Rybak with a rod in its bag and never cast.
		// Give up out loud instead, so the log says which leg failed.
		if (state.dwFishingSessionEndTime != 0 && !state.bIsFishing &&
				dwNow >= state.dwFishingSessionEndTime)
		{
			sys_err("PLAYERBOT_FISHING: never reached the water pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) tackle=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY, needsTackle ? 1 : 0);
			return EndPlayerBotFishingSession(ch, state, dwNow, "never_reached_water");
		}

		if (DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) >
				PLAYERBOT_FISHING_ARRIVE)
		{
			// Riding there is fine; the line simply cannot go in from a saddle.
			if (MovePlayerBot(ch, destX, destY, dwNow, 16, true, true) ||
					state.bStuckCounter < PLAYERBOT_FISHING_STUCK_LIMIT)
				return true;

			// Out of route. The tackle leg cannot be skipped - only the Rybak sells
			// rods - but the bank can be: fishing() in r40250 asks for a
			// non-blocking tile, a rod of type ITEM_ROD and bait in socket 2, and
			// never looks for water at all (it computes a facing offset and then
			// discards it). Casting where the bot already stands is therefore a
			// real cast, and it beats spending the entire session walking at a
			// bank the navigation cannot reach.
			if (needsTackle ||
					IsPlayerBotPositionBlocked(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			{
				sys_err("PLAYERBOT_FISHING: route failed pid=%u name=%s from=(%ld,%ld) to=(%ld,%ld) tackle=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
						destX, destY, needsTackle ? 1 : 0);
				return EndPlayerBotFishingSession(ch, state, dwNow, "route_failed");
			}

			sys_log(0, "PLAYERBOT_FISHING: bank unreachable, casting in place pid=%u name=%s pos=(%ld,%ld) bank=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY);
			state.bStuckCounter = 0;
			ClearPlayerBotRoute(state, true);
		}

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "fishing");
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_STANDING);

		if (needsTackle)
		{
			if (!RestockPlayerBotTackle(ch, state, dwNow))
				return EndPlayerBotFishingSession(ch, state, dwNow, "cannot_afford_tackle");
			return true;
		}
		if (!EquipPlayerBotRod(ch))
			return EndPlayerBotFishingSession(ch, state, dwNow, "rod_not_equippable");

		if (dwNow < state.dwNextFishingActionTime)
			return true;

		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!state.bIsFishing && rod && rod->GetSocket(2) == 0 && !BaitPlayerBotRod(ch))
		{
			// The pouch ran dry between passes; the walk back to the Rybak is
			// picked up by the tackle check at the top of the next pass.
			state.dwNextFishingActionTime = dwNow + number(1000, 2000);
			return true;
		}

		// The engine holds the whole cast in one event: step 0 is the line in the
		// water, step 1 means a fish is on and starts the 6 s window to pull.
		fishing::fishing_event_info* info = ch->m_pkFishingEvent
				? dynamic_cast<fishing::fishing_event_info*>(ch->m_pkFishingEvent->info)
				: NULL;

		if (!state.bIsFishing || !info)
		{
			if (info)
			{
				// A cast survived from an earlier pass; adopt it rather than
				// stacking a second one.
				state.bIsFishing = true;
				state.dwFishingCastTime = dwNow;
				return true;
			}
			if (state.bIsFishing)
			{
				// The event ended on its own -- the bite window elapsed. The engine
				// already cleared the bait, so the next pass re-baits and recasts.
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				ProcessPlayerBotCatch(ch);
				return true;
			}

			// CHARACTER::fishing() dereferences the sectree map and the tile under
			// the bot without checking either, so never call it blind.
			if (!ch->GetSectree() ||
					!SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex()))
			{
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}

			// Face straight across at the river rather than along the bank: the
			// water lies due east of this stretch.
			ch->SetRotationToXY(PLAYERBOT_FISHING_WATER_X, ch->GetY());
			ch->fishing();
			if (!ch->m_pkFishingEvent)
			{
				// Blocked tile or missing bait; step away and try again shortly.
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}
			state.bIsFishing = true;
			state.dwFishingCastTime = dwNow;
			return true;
		}

		if (info->step < 1)
		{
			// Still waiting for a bite. The engine takes 10-40 s; anything past a
			// minute means the event is wedged.
			if (dwNow - state.dwFishingCastTime > PLAYERBOT_FISHING_CAST_TIMEOUT)
			{
				ch->fishing_take();
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				sys_log(0, "PLAYERBOT_FISHING: cast timed out pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			return true;
		}

		// A fish is on. fishing::Compute() peaks around 3 s after the bite, so wait
		// out that band before pulling instead of yanking the rod instantly.
		const DWORD hooked = get_dword_time() - info->hang_time;
		const DWORD pullAt = PLAYERBOT_FISHING_PULL_MIN_DELAY +
				PlayerBotNavHash(ch->GetPlayerID() ^ info->hang_time) %
				(PLAYERBOT_FISHING_PULL_MAX_DELAY - PLAYERBOT_FISHING_PULL_MIN_DELAY + 1U);
		if (hooked < pullAt)
			return true;

		ch->fishing_take();
		state.bIsFishing = false;
		state.dwNextFishingActionTime = dwNow + number(2000, 4000);
		state.dwLastMeaningfulActivityTime = dwNow;
		ProcessPlayerBotCatch(ch);
		sys_log(0, "PLAYERBOT_FISHING: pulled pid=%u name=%s hooked_ms=%u fish=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)hooked, info->fish_id);
		return true;
	}
}

#endif
