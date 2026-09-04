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

	class CCollectPlayerBotLoot
	{
		public:
			CCollectPlayerBotLoot(LPCHARACTER owner, int maxDistance, const std::map<DWORD, DWORD>& failedLoot, DWORD dwNow) :
				m_owner(owner),
				m_maxDistance(maxDistance),
				m_failedLoot(failedLoot),
				m_dwNow(dwNow)
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
				if (distance <= m_maxDistance)
					m_items.push_back(std::make_pair(distance, item));

				return true;
			}

			void Sort()
			{
				std::sort(m_items.begin(), m_items.end());
			}

			const std::vector<std::pair<int, LPITEM> >& GetItems() const { return m_items; }

		private:
			LPCHARACTER m_owner;
			int m_maxDistance;
			const std::map<DWORD, DWORD>& m_failedLoot;
			DWORD m_dwNow;
			std::vector<std::pair<int, LPITEM> > m_items;
	};

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
			const DWORD visibleDelay = PLAYERBOT_LOOT_VISIBLE_DELAY_MIN +
					(PlayerBotNavHash(itemVID ^ ch->GetPlayerID()) %
					 (PLAYERBOT_LOOT_VISIBLE_DELAY_MAX - PLAYERBOT_LOOT_VISIBLE_DELAY_MIN + 1));
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
		state.dwNextLootPickupTime = dwNow + number(
				PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN, PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX);
		if (ch->PickupItem(itemVID))
		{
			state.mapLootSeenSince.erase(itemVID);
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
		if (bFightingActiveTarget || state.bLootThreatNearby)
		{
			TryPlayerBotCombatPickup(ch, state, dwNow);
			return false;
		}
		if (dwNow < state.dwNextLootSearchTime)
			return false;

		CCollectPlayerBotLoot collector(ch, PLAYERBOT_LOOT_SEARCH_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		const std::vector<std::pair<int, LPITEM> >& items = collector.GetItems();
		if (items.empty())
		{
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
		const DWORD visibleDelay = PLAYERBOT_LOOT_VISIBLE_DELAY_MIN +
				(PlayerBotNavHash(nearestVID ^ ch->GetPlayerID()) %
				 (PLAYERBOT_LOOT_VISIBLE_DELAY_MAX - PLAYERBOT_LOOT_VISIBLE_DELAY_MIN + 1));
		const bool visibleLongEnough = dwNow - firstSeen >= visibleDelay;

		if (items.front().first <= PLAYERBOT_PICKUP_RANGE)
		{
			ch->Stop();
			if (!visibleLongEnough || dwNow < state.dwNextLootPickupTime)
				return true;

			const DWORD itemVnum = nearest->GetVnum();
			state.dwNextLootPickupTime = dwNow + number(
					PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN, PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX);
			if (ch->PickupItem(nearestVID))
			{
				state.mapLootSeenSince.erase(nearestVID);
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
