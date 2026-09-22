#ifndef __INC_METIN2_PLAYERBOT_LOOT_H__
#define __INC_METIN2_PLAYERBOT_LOOT_H__

// Picking things up, in and out of a fight.
//
// Two constraints shape all of it. A drop belongs to whoever earned it for a
// few seconds, so a bot has to respect ownership and its party's share rather
// than sweep the floor. And ForEachAround snapshots every entity in nine
// sectrees before the three-metre radius is even applied, which makes an empty
// scan as expensive as a successful one - so scans are throttled whether or not
// they find anything. Several hundred bots doing this untimed was thousands of
// full sector walks a second.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	// Dropped yang. ITEM_ELK is the money type, not a vnum: the drop is a real
	// item on the ground like any other, which is why it queues behind herbs and
	// hides unless something says otherwise.
	bool IsPlayerBotMoneyDrop(LPITEM item)
	{
		return item && item->GetType() == ITEM_ELK;
	}

	// Yang first, then by distance. A bot that walks past three coin piles to
	// reach a hide, then walks back for each pile, spends its time crossing a
	// field it has already cleared - and the yang is what pays for the potions
	// that keep it killing.
	struct FPlayerBotLootOrder
	{
		bool operator()(const std::pair<int, LPITEM>& a,
				const std::pair<int, LPITEM>& b) const
		{
			const bool aMoney = IsPlayerBotMoneyDrop(a.second);
			const bool bMoney = IsPlayerBotMoneyDrop(b.second);
			if (aMoney != bMoney)
				return aMoney;
			return a.first < b.first;
		}
	};

	// How long a drop has to have been lying there before this bot reaches for
	// it. Long enough for a person to have noticed it - except for yang, which
	// nobody hesitates over.
	DWORD GetPlayerBotLootVisibleDelay(LPITEM item, DWORD itemVID, DWORD playerID)
	{
		const DWORD roll = PlayerBotNavHash(itemVID ^ playerID);
		if (IsPlayerBotMoneyDrop(item))
			return PLAYERBOT_LOOT_MONEY_DELAY_MIN +
					(roll % (PLAYERBOT_LOOT_MONEY_DELAY_MAX -
							PLAYERBOT_LOOT_MONEY_DELAY_MIN + 1));
		return PLAYERBOT_LOOT_VISIBLE_DELAY_MIN +
				(roll % (PLAYERBOT_LOOT_VISIBLE_DELAY_MAX -
						PLAYERBOT_LOOT_VISIBLE_DELAY_MIN + 1));
	}

	DWORD GetPlayerBotLootPickupInterval(LPITEM item)
	{
		if (IsPlayerBotMoneyDrop(item))
			return number(PLAYERBOT_LOOT_MONEY_INTERVAL_MIN,
					PLAYERBOT_LOOT_MONEY_INTERVAL_MAX);
		return number(PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN,
				PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX);
	}

	class FPlayerBotPartyLootOwner
	{
		public:
			FPlayerBotPartyLootOwner(LPITEM item) : m_item(item), m_bFound(false) {}

			void operator () (LPCHARACTER member)
			{
				if (!m_bFound && member && m_item && m_item->IsOwnership(member))
					m_bFound = true;
			}

			bool Found() const { return m_bFound; }

		private:
			LPITEM m_item;
			bool m_bFound;
	};

	bool IsPlayerBotPartyLoot(LPCHARACTER owner, LPITEM item)
	{
		if (!owner || !item)
			return false;
		if (item->IsOwnership(owner))
			return true;
		if (!owner->GetParty() ||
				IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP))
			return false;

		// Metin drops are distributed between party members.  PickupItem already
		// supports a nearby member collecting an item for its assigned owner, so
		// the AI search must expose party-owned drops too.  Previously every bot
		// only saw its own PID and most of a party Metin drop was left behind as
		// soon as the individual owners moved toward another target.
		FPlayerBotPartyLootOwner finder(item);
		owner->GetParty()->ForEachOnlineMember(finder);
		return finder.Found();
	}

	// Whether a drop would land in a bag with no free cell: only by merging
	// into a stack of the same thing, the way the engine's own pickup does
	// (same vnum, same sockets, room under ITEM_MAX_COUNT).
	bool PlayerBotLootMergesIntoStack(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->IsStackable())
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (!held || held->GetVnum() != item->GetVnum() ||
					held->GetCount() + item->GetCount() > ITEM_MAX_COUNT)
				continue;
			bool sameSockets = true;
			for (int s = 0; s < ITEM_SOCKET_MAX_NUM; ++s)
				if (held->GetSocket(s) != item->GetSocket(s))
					sameSockets = false;
			if (sameSockets)
				return true;
		}
		return false;
	}

	// Whether the engine's pickup would find this drop a place: a stack of the
	// same thing to pour it into, or room of its own size - GetEmptyInventoryEx
	// on mt2009, which also knows the pages a material or a book goes to, and
	// the item's height on r40250. Yang never needs a cell.
	bool PlayerBotBagTakesDrop(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		if (IsPlayerBotMoneyDrop(item) || PlayerBotLootMergesIntoStack(ch, item))
			return true;
#if defined(PLAYERBOT_ENGINE_MT2009)
		return ch->GetEmptyInventoryEx(item) != -1;
#else
		return ch->GetEmptyInventory(item->GetSize()) != -1;
#endif
	}

	// A bot past the age of pennies leaves the pennies on the ground.
	//
	// Every drop in reach was loot, so a bot of sixty with millions in its
	// purse ran for a small red potion, a level-ten sword and a handful of
	// herbs like a bot of ten, and carried them to the merchant for a few
	// hundred yang ("boty rzucaja sie jak zombie po przedmioty", sizowski).
	// The operator's rule: once a bot has the level and the yang to be past
	// it, merchant fodder worth under PLAYERBOT_LOOT_CHOOSY_MAX_VALUE is not
	// worth a step. Fodder is exactly three things - potions, gear it has
	// outgrown by PLAYERBOT_LOOT_OUTGROWN_GEAR_LEVELS under
	// PLAYERBOT_PRECIOUS_REFINE with no prize lines, and the herbs the merchant
	// takes - so a refine material, a book, a scroll, a chest, a stone, a gear
	// piece it could still wear and yang are picked up by everybody as before,
	// and an item with no merchant price counts as unknown, never as cheap.
	bool IsPlayerBotChoosyLooter(LPCHARACTER ch)
	{
		return ch && (int)ch->GetLevel() >= PLAYERBOT_LOOT_CHOOSY_MIN_LEVEL &&
				(long long)ch->GetGold() >= PLAYERBOT_LOOT_CHOOSY_MIN_GOLD;
	}

	bool IsPlayerBotLootBeneathBot(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->GetProto())
			return false;
		// Never the goods a player crafts further, whatever the merchant pays:
		// a bot of seventy-three walked past Grzyb Tue, Korzen Gango and a
		// Zbroja Twarzy Ducha+3 on a floor (Tieru, 15 September).
		if (IsPlayerBotPickupGoods(item))
			return false;
		const long long unit = (long long)GetPlayerBotNpcSellUnitPrice(item);
		if (unit <= 0 || unit * (long long)item->GetCount() >= PLAYERBOT_LOOT_CHOOSY_MAX_VALUE)
			return false;
		switch (item->GetType())
		{
			case ITEM_USE:
				return item->GetSubType() == USE_POTION ||
						item->GetSubType() == USE_POTION_NODELAY;
			case ITEM_MATERIAL:
				return IsPlayerBotNonGearMaterial(item->GetVnum());
			case ITEM_WEAPON:
			case ITEM_ARMOR:
			{
				// Helmets and shields are picked up whatever their merchant price:
				// the ones of level 21, 41 and 61 are worth more than it says, and a
				// dungeon floor kept its Upiorna Maska while bots of fifty walked
				// past (Tieru, 15 September: "tarcze na 21 41 61 poziom czy helmy
				// ... warto podnosic tak czy siak").
				if (item->GetType() == ITEM_ARMOR &&
						(item->GetSubType() == ARMOR_HEAD || item->GetSubType() == ARMOR_SHIELD))
					return false;
				if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE ||
						IsPlayerBotPrizeItem(item) || IsPlayerBotSpecialLevel30Weapon(item))
					return false;
				int levelLimit = 0;
				for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
					if (item->GetProto()->aLimits[i].bType == LIMIT_LEVEL)
						levelLimit = (int)item->GetProto()->aLimits[i].lValue;
				return levelLimit + PLAYERBOT_LOOT_OUTGROWN_GEAR_LEVELS <= (int)ch->GetLevel();
			}
			default:
				return false;
		}
	}

	// What a medal dropper bends down for. Its bag is its counter's stock
	// already - fifty to seventy of ninety cells - and a Monkey Dungeon floor
	// filled the rest in two to five minutes: once the dropper was let stay while
	// a medal had a cell, 28 of 37 visits ended with no cell left and the average
	// visit lasted under three minutes. It takes the medal, the goods a player
	// crafts further, a skill book, a Moonlight chest (for its counter,
	// PLAYERBOT_CHEST_DROPPER_HOLD) and whatever pours into a stack it already
	// carries; the rest stays on the floor for whoever wants it.
	bool IsPlayerBotMedalDropperLoot(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->GetProto())
			return false;
		if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM || item->GetType() == ITEM_SKILLBOOK ||
				item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM || IsPlayerBotPickupGoods(item))
			return true;
		return PlayerBotLootMergesIntoStack(ch, item);
	}

	class CCollectPlayerBotLoot
	{
		public:
			CCollectPlayerBotLoot(LPCHARACTER owner, int maxDistance, const std::map<DWORD, DWORD>& failedLoot, DWORD dwNow) :
				m_owner(owner),
				m_maxDistance(maxDistance),
				m_failedLoot(failedLoot),
				m_dwNow(dwNow),
				// One count for the whole sweep: a full bag is a full bag for
				// every drop in it.
				m_bagFull(CountPlayerBotFreeInventoryCells(owner) == 0),
				m_skippedNoRoom(0),
				m_choosy(IsPlayerBotChoosyLooter(owner)),
				m_skippedCheap(0),
				m_medalDropper(owner && GetPlayerBotPersonalityByPID(owner->GetPlayerID()) ==
						BOT_PERSONALITY_MEDAL_DROPPER)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_ITEM))
					return false;

				LPITEM item = static_cast<LPITEM>(entity);
				// Ground items in this source tree retain entity map index 0.
				// Being in one of the owner's neighbouring sectrees is the reliable
				// same-map test; checking item->GetMapIndex() rejects every drop.
				if (!item->GetSectree() || !IsPlayerBotPartyLoot(m_owner, item))
					return false;

				std::map<DWORD, DWORD>::const_iterator fit = m_failedLoot.find(item->GetVID());
				if (fit != m_failedLoot.end() && m_dwNow < fit->second)
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - item->GetX(),
						m_owner->GetY() - item->GetY());
				if (distance > m_maxDistance)
					return true;
				// A key of the Demon Tower is the floor's, whoever the bot is
				// (playerbot_demon_tower.h uses or hands it in).
				const bool towerKey = IsPlayerBotDemonTowerKey(item->GetVnum());
				if (!towerKey && m_medalDropper && !IsPlayerBotMedalDropperLoot(m_owner, item))
					return true;
				// A cape or a symbol nobody wears (IsPlayerBotLeftOnGroundItem).
				if (IsPlayerBotLeftOnGroundItem(item->GetVnum()))
					return true;
				if (!towerKey && m_choosy && IsPlayerBotLootBeneathBot(m_owner, item))
				{
					++m_skippedCheap;
					return true;
				}
				// A drop the bag cannot take is not loot: walking up to it,
				// announcing the pickup and being refused by the engine every
				// five seconds is what "mowi ze podnosi lup ale nie robi nic"
				// was. Counted, so the pass can say so once a minute. And
				// "cannot take" is the engine's own test for this drop, not a bag
				// with no cell at all: a bag with single holes and no free column
				// is refused a sword or a breastplate ("No empty inventory ...
				// size 2", 7736 times in two hours from 320 bots on the test
				// world), and the bot stood at the drop asking every five seconds
				// until the watchdog moved it.
				if (m_bagFull ? !PlayerBotLootMergesIntoStack(m_owner, item)
						: !PlayerBotBagTakesDrop(m_owner, item))
				{
					++m_skippedNoRoom;
					return true;
				}
				m_items.push_back(std::make_pair(distance, item));

				return true;
			}

			void Sort()
			{
				std::sort(m_items.begin(), m_items.end(), FPlayerBotLootOrder());
			}

			const std::vector<std::pair<int, LPITEM> >& GetItems() const { return m_items; }
			int SkippedNoRoom() const { return m_skippedNoRoom; }
			int SkippedCheap() const { return m_skippedCheap; }

		private:
			LPCHARACTER m_owner;
			int m_maxDistance;
			const std::map<DWORD, DWORD>& m_failedLoot;
			DWORD m_dwNow;
			bool m_bagFull;
			int m_skippedNoRoom;
			bool m_choosy;
			int m_skippedCheap;
			bool m_medalDropper;
			std::vector<std::pair<int, LPITEM> > m_items;
	};

	size_t CountPlayerBotLootToTake(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return 0;
		CCollectPlayerBotLoot loot(ch, PLAYERBOT_LOOT_SEARCH_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(loot);
		return loot.GetItems().size();
	}

	class CDetectPlayerBotCombatThreat
	{
		public:
			CDetectPlayerBotCombatThreat(LPCHARACTER owner) : m_owner(owner), m_found(false) {}

			bool operator () (LPENTITY entity)
			{
				if (m_found || !entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER mob = static_cast<LPCHARACTER>(entity);
				if (!mob || !mob->IsMonster() || mob->IsDead() ||
						mob->GetMapIndex() != m_owner->GetMapIndex())
					return false;
				LPCHARACTER victim = mob->GetVictim();
				if (victim == m_owner || (victim && m_owner->GetParty() &&
						victim->GetParty() == m_owner->GetParty()))
				{
					if (DISTANCE_APPROX(m_owner->GetX() - mob->GetX(),
							m_owner->GetY() - mob->GetY()) <= 2500)
						m_found = true;
				}
				return false;
			}

			bool Found() const { return m_found; }

		private:
			LPCHARACTER m_owner;
			bool m_found;
	};

	bool TryPlayerBotCombatPickup(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() || dwNow < state.dwNextLootPickupTime)
			return false;

		// Set the throttle before scanning.  An empty floor used to leave the
		// timestamp untouched, so the second HandleLoot call in the same update and
		// every following update repeated a complete nine-sectree snapshot.
		state.dwNextLootPickupTime = dwNow + number(
				PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MIN,
				PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MAX);

		// This is the server equivalent of repeatedly pressing Z: inspect only the
		// immediate pickup circle, never Stop(), never clear the victim and never
		// walk toward an item while a pack is still engaged.
		CCollectPlayerBotLoot collector(ch, PLAYERBOT_PICKUP_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		const std::vector<std::pair<int, LPITEM> >& items = collector.GetItems();
		if (items.empty())
			return false;

		LPITEM pickup = NULL;
		DWORD firstSeen = 0;
		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item || !item->GetSectree())
				continue;
			const DWORD itemVID = item->GetVID();
			std::map<DWORD, DWORD>::iterator seen = state.mapLootSeenSince.find(itemVID);
			if (seen == state.mapLootSeenSince.end())
			{
				state.mapLootSeenSince[itemVID] = dwNow;
				continue;
			}
			const DWORD visibleDelay = GetPlayerBotLootVisibleDelay(
					item, itemVID, ch->GetPlayerID());
			if (dwNow - seen->second >= visibleDelay)
			{
				pickup = item;
				firstSeen = seen->second;
				break;
			}
		}
		if (!pickup)
			return false;

		const DWORD itemVID = pickup->GetVID();
		const DWORD itemVnum = pickup->GetVnum();
		const bool material = pickup->GetType() == ITEM_MATERIAL;
		// Read before the pickup: a stack that merges is gone after it.
		const long itemSocket0 = pickup->GetSocket(0);
		const BYTE itemType = pickup->GetType();
		state.dwNextLootPickupTime = dwNow + GetPlayerBotLootPickupInterval(pickup);
		if (ch->PickupItem(itemVID))
		{
			NotePlayerBotMoodValuable(ch, itemVnum, itemSocket0, itemType, "pickup");
			state.mapLootSeenSince.erase(itemVID);
			if (material)
				RememberPlayerBotSpotDrop(ch->GetMapIndex(), ch->GetX(), ch->GetY(), itemVnum);
			if (itemVnum == PLAYERBOT_HORSE_MEDAL_VNUM)
			{
				const int looted = std::max(0,
						ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG)) + 1;
				ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG, looted);
				ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_MAP_FLAG, ch->GetMapIndex());
				ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_TIME_FLAG, get_global_time());
			}
			sys_log(1, "PLAYERBOT_AI: combat-Z pickup pid=%u name=%s item_vid=%u vnum=%u visible_ms=%u",
					ch->GetPlayerID(), ch->GetName(), itemVID, itemVnum,
					(unsigned int)(dwNow - firstSeen));
			return true;
		}

		state.mapFailedLootVIDs[itemVID] = dwNow + 5000;
		state.mapLootSeenSince.erase(itemVID);
		return false;
	}

	bool HandleLoot(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return false;

		// Cleanup must also run for bots which spend minutes in continuous combat.
		// Keep it periodic: walking both maps on every AI tick is unnecessary.
		if (dwNow >= state.dwNextLootCleanupTime)
		{
			state.dwNextLootCleanupTime = dwNow + PLAYERBOT_LOOT_CLEANUP_INTERVAL +
					(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedLootVIDs.begin();
					it != state.mapFailedLootVIDs.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedLootVIDs.erase(it++);
				else
					++it;
			}
			for (std::map<DWORD, DWORD>::iterator it = state.mapLootSeenSince.begin();
					it != state.mapLootSeenSince.end(); )
			{
				if (dwNow - it->second > 120000)
					state.mapLootSeenSince.erase(it++);
				else
					++it;
			}
		}

		LPCHARACTER activeTarget = state.dwTargetVID != 0
			? CHARACTER_MANAGER::instance().Find(state.dwTargetVID)
			: NULL;
		const bool bFightingActiveTarget = activeTarget && !activeTarget->IsDead() &&
				(activeTarget->IsMonster() || activeTarget->IsStone());
		const bool bRecentCombat = state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime < 1800;
		if (!bFightingActiveTarget && (bRecentCombat ||
				dwNow >= state.dwNextLootThreatCheckTime))
		{
			CDetectPlayerBotCombatThreat threat(ch);
			ch->GetSectree()->ForEachAround(threat);
			state.bLootThreatNearby = threat.Found();
			state.dwNextLootThreatCheckTime = dwNow + number(
					PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MIN,
					PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MAX);
		}
		// A dead primary target does not mean its group is finished. While either a
		// live target or an attacking pack exists, perform only non-blocking Z pickup.
		// Once the threat scan says the pack is clear, recent combat no longer hides
		// the 25 m loot search: the bot finishes its own drop before choosing a new mob.
		// A Metin just broke: its drops are the bot's for a few seconds and lie
		// in a ring round where the stone stood, and the pack it summoned is
		// still on the bot. Go for them anyway, within reach, the way a player
		// dashes for them - or the bots that are not fighting will have them.
		const bool metinDash = state.dwStoneBrokenTime != 0 &&
				dwNow - state.dwStoneBrokenTime < PLAYERBOT_METIN_LOOT_DASH_TIME;
		if ((bFightingActiveTarget || state.bLootThreatNearby) && !metinDash)
		{
			TryPlayerBotCombatPickup(ch, state, dwNow);
			return false;
		}
		if (dwNow < state.dwNextLootSearchTime)
			return false;

		CCollectPlayerBotLoot collector(ch,
				metinDash ? PLAYERBOT_METIN_LOOT_DASH_RANGE : PLAYERBOT_LOOT_SEARCH_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		if (collector.SkippedCheap() > 0)
			PlayerBotLogThrottled("loot_left_cheap", dwNow,
					"PLAYERBOT_LOOT: left merchant fodder pid=%u name=%s level=%u gold=%lld drops=%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(),
					(long long)ch->GetGold(), collector.SkippedCheap());
		const std::vector<std::pair<int, LPITEM> >& items = collector.GetItems();
		if (items.empty())
		{
			// Drops in reach and no cell to put one in. Said once a minute per
			// bot with the count, so the next "boty maja zapchane eq" report
			// carries a number; the town errand for a full bag is the
			// manager's (bInventoryFull) and the junk rule's, not this pass's.
			if (collector.SkippedNoRoom() > 0 && dwNow >= state.dwNextBagFullLogTime)
			{
				state.dwNextBagFullLogTime = dwNow + 60000;
				sys_log(0, "PLAYERBOT_LOOT: bag full pid=%u name=%s map=%ld drops_in_reach=%d level=%d can_open_shop=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
						collector.SkippedNoRoom(), ch->GetLevel(), PlayerBotCanOpenShop(ch) ? 1 : 0);
			}
			// An empty 25 m search used to run twice per second for every peaceful
			// bot.  Delay only the next empty-floor query; as soon as an item is seen,
			// the normal 500 ms walking/visibility cadence remains unchanged.
			state.dwNextLootSearchTime = dwNow + number(
					PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MIN,
					PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MAX);
			return false;
		}
		state.dwNextLootSearchTime = 0;
		SetPlayerBotAction(state, BOT_ACTION_LOOT, dwNow);

		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item || !item->GetSectree())
				continue;
			const DWORD itemVID = item->GetVID();
			if (state.mapLootSeenSince.find(itemVID) == state.mapLootSeenSince.end())
				state.mapLootSeenSince[itemVID] = dwNow;
		}

		LPITEM nearest = items.front().second;
		if (!nearest || !nearest->GetSectree())
			return false;
		const DWORD nearestVID = nearest->GetVID();
		const DWORD firstSeen = state.mapLootSeenSince[nearestVID];
		const DWORD visibleDelay = GetPlayerBotLootVisibleDelay(
				nearest, nearestVID, ch->GetPlayerID());
		const bool visibleLongEnough = dwNow - firstSeen >= visibleDelay;

		if (items.front().first <= PLAYERBOT_PICKUP_RANGE)
		{
			ch->Stop();
			if (!visibleLongEnough || dwNow < state.dwNextLootPickupTime)
				return true;

			const DWORD itemVnum = nearest->GetVnum();
			const bool material = nearest->GetType() == ITEM_MATERIAL;
			const long itemSocket0 = nearest->GetSocket(0);
			const BYTE itemType = nearest->GetType();
			state.dwNextLootPickupTime = dwNow + GetPlayerBotLootPickupInterval(nearest);
			if (ch->PickupItem(nearestVID))
			{
				NotePlayerBotMoodValuable(ch, itemVnum, itemSocket0, itemType, "pickup");
				state.mapLootSeenSince.erase(nearestVID);
				if (material)
					RememberPlayerBotSpotDrop(ch->GetMapIndex(), ch->GetX(), ch->GetY(), itemVnum);
				if (itemVnum == PLAYERBOT_HORSE_MEDAL_VNUM)
				{
					const int looted = std::max(0,
							ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG)) + 1;
					ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG, looted);
					ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_MAP_FLAG, ch->GetMapIndex());
					ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_TIME_FLAG, get_global_time());
					sys_log(0, "PLAYERBOT_HORSE: real medal looted pid=%u name=%s map=%ld total_looted=%d",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), looted);
				}
				sys_log(1, "PLAYERBOT_AI: picked up delayed loot pid=%u name=%s item_vid=%u vnum=%u visible_ms=%u",
						ch->GetPlayerID(), ch->GetName(), nearestVID, itemVnum,
						(unsigned int)(dwNow - firstSeen));
				return true;
			}

			state.mapFailedLootVIDs[nearestVID] = dwNow + 5000;
			state.mapLootSeenSince.erase(nearestVID);
			sys_log(1, "PLAYERBOT_AI: pickup failed pid=%u name=%s item_vid=%u vnum=%u -> retrying in 5s",
					ch->GetPlayerID(), ch->GetName(), nearestVID, itemVnum);
			return true;
		}

		// Filter out items in pickup range that just failed
		std::vector<std::pair<int, LPITEM> > pendingItems;
		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item)
				continue;
			if (state.mapFailedLootVIDs.find(item->GetVID()) != state.mapFailedLootVIDs.end())
				continue;
			pendingItems.push_back(items[i]);
		}

		if (pendingItems.empty())
			return false;

		nearest = pendingItems.front().second;
		if (!nearest || !nearest->GetSectree())
			return false;

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		if (pendingItems.front().first > PLAYERBOT_PICKUP_RANGE)
		{
			if (!MovePlayerBot(ch, nearest->GetX(), nearest->GetY(), dwNow) &&
					state.bStuckCounter >= 3)
			{
				state.mapFailedLootVIDs[nearest->GetVID()] = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				return false;
			}
		}
		else
			ch->Stop();

		return true;
	}
}

#endif
