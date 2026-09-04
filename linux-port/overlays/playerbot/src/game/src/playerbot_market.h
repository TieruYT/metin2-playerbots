#ifndef __INC_METIN2_PLAYERBOT_MARKET_H__
#define __INC_METIN2_PLAYERBOT_MARKET_H__

// Buying from another bot's stall.
//
// The stalls existed already; nobody ever bought from one, so a well-refined
// spare sat on a counter until its keeper packed up and eventually vendored it.
// This closes the loop: a bot in Bokjung with money in its pocket walks the
// market strip and buys what it actually needs - a refine material it is short
// of, or a piece of gear better than what it is wearing.
//
// It reads the offer from the seller's own AI state rather than from CShop,
// whose item list is private. That is not a workaround: our stalls carry
// exactly one item and we are the ones who put it there, so the recorded offer
// is exactly what is on the counter.
//
// The purchase itself goes through the engine's ordinary path. Setting the shop
// owner is what a client does when a player clicks a stall, and everything
// after it - the price, the gold, the inventory space, moving the item - is the
// engine's own code, so a bot cannot buy anything a player could not.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_town.h.

namespace
{
	class CCollectPlayerBotStalls
	{
		public:
			CCollectPlayerBotStalls(LPCHARACTER buyer, int maxDistance)
				: m_buyer(buyer), m_maxDistance(maxDistance)
			{
			}

			void operator()(LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER) || !m_buyer)
					return;
				LPCHARACTER keeper = (LPCHARACTER)entity;
				if (keeper == m_buyer || !keeper->IsPC() || !keeper->GetMyShop())
					return;
				if (DISTANCE_APPROX(m_buyer->GetX() - keeper->GetX(),
						m_buyer->GetY() - keeper->GetY()) > m_maxDistance)
					return;
				m_stalls.push_back(keeper);
			}

			std::vector<LPCHARACTER> m_stalls;

		private:
			LPCHARACTER m_buyer;
			int m_maxDistance;
	};

	// The item behind an offer. A private shop leaves its stock in the owner's
	// inventory, so the real item - with its sockets and bonus lines - is still
	// there to be looked at before deciding.
	LPITEM FindPlayerBotStallItem(LPCHARACTER keeper, DWORD vnum, BYTE refine)
	{
		if (!keeper || vnum == 0 || !keeper->IsItemLoaded())
			return NULL;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = keeper->GetInventoryItem(cell);
			if (item && item->GetVnum() == vnum &&
					item->GetRefineLevel() == refine)
				return item;
		}
		return NULL;
	}

	// Would this bot rather have the item than the money?
	bool WantsPlayerBotStallItem(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer)
			return false;

		// A material it is short of right now. This is the whole reason a bot
		// walks the market: the alternative is farming the same material for an
		// hour while a neighbour has spares on a counter three metres away.
		if (PlayerBotNeedsRefineMaterial(ch, offer->GetVnum()))
			return true;

		// A horse medal, if this bot still has a horse to raise. Buying one is
		// hours of the Monkey Dungeon it does not have to run.
		if (offer->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return CanPlayerBotAdvanceHorse(ch);

		// A level-30 weapon of its own class, when it has none. This is the item
		// bots cross the world to farm; buying one off a counter is the whole
		// point of there being a market.
		if (IsPlayerBotSpecialLevel30Weapon(offer) && IsPlayerBotWeapon(ch, offer) &&
				offer->GetLevelLimit() <= ch->GetLevel() &&
				!HasPlayerBotSpecialLevel30Weapon(ch, false))
			return true;

		// Gear only when it is genuinely better than what is worn. A bot that
		// buys sideways upgrades spends its yang on nothing.
		if (!IsPlayerBotEquipmentCandidate(ch, offer))
			return false;
		if (offer->GetLevelLimit() > ch->GetLevel())
			return false;
		const int wearCell = offer->FindEquipCell(ch);
		if (wearCell < 0)
			return false;
		LPITEM worn = ch->GetWear((BYTE)wearCell);
		if (!worn)
			return true;
		// A big bonus line is worth having even when the base item scores level
		// with what is worn - a thousand health does not show up in the equipment
		// score, and it is exactly what a player would buy the piece for.
		if (HasPlayerBotValuableBonus(offer) && !HasPlayerBotValuableBonus(worn))
			return true;
		return GetPlayerBotEquipmentScore(offer, ch) >
				GetPlayerBotEquipmentScore(worn, ch);
	}

	bool ManagePlayerBotShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return false;
		// A keeper minding its own counter is not also a customer.
		if (ch->GetMyShop())
			return false;
		// bVisitingShop is deliberately not in this list. A bot standing anywhere
		// near the market is on a town errand almost by definition, so excluding
		// them left the market with no customers at all - browsing a stall while
		// in town for potions is the whole idea, not a conflict with it.
		if (state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession)
			return false;
		// Stalls stand in both towns now - most of them in Joan, round the village
		// guard - so a buyer looks wherever it happens to be. Restricting this to
		// Bokjung would have left nine stalls in ten with nobody to sell to.
		if (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2)
			return false;
		if (dwNow < state.dwNextShoppingTime)
			return false;

		state.dwNextShoppingTime = dwNow + number(
				PLAYERBOT_SHOPPING_INTERVAL_MIN, PLAYERBOT_SHOPPING_INTERVAL_MAX);

		if (ch->GetGold() <= (int)PLAYERBOT_SHOPPING_GOLD_FLOOR)
			return false;
		if (ch->GetEmptyInventory(2) < 0)
			return false;
		if (!ch->GetSectree())
			return false;

		CCollectPlayerBotStalls collector(ch, PLAYERBOT_SHOPPING_RANGE);
		ch->GetSectree()->ForEachAround(collector);
		if (collector.m_stalls.empty())
			return false;

		for (size_t i = 0; i < collector.m_stalls.size(); ++i)
		{
			LPCHARACTER keeper = collector.m_stalls[i];
			if (!keeper || !keeper->GetMyShop())
				continue;
			TPlayerBotAIStateMap::const_iterator it =
					s_mapPlayerBotAIStates.find(keeper->GetPlayerID());
			if (it == s_mapPlayerBotAIStates.end() || it->second.vecShopOffers.empty())
				continue;

			// A counter holds several things now. Walk it and take the first the
			// buyer actually wants; the index is the slot, because the offers were
			// recorded in the order they were handed to OpenMyShop.
			BYTE slot = 0;
			LPITEM offer = NULL;
			DWORD price = 0;
			for (size_t k = 0; k < it->second.vecShopOffers.size(); ++k)
			{
				const TPlayerBotShopOffer& candidate = it->second.vecShopOffers[k];
				// Never spend down to nothing: potions and the next weapon first.
				if (candidate.dwPrice == 0 ||
						ch->GetGold() - (int)candidate.dwPrice <
							(int)PLAYERBOT_SHOPPING_GOLD_FLOOR)
					continue;
				LPITEM candidateItem = FindPlayerBotStallItem(keeper,
						candidate.dwVnum, candidate.bRefine);
				if (!WantsPlayerBotStallItem(ch, candidateItem))
					continue;
				slot = (BYTE)k;
				offer = candidateItem;
				price = candidate.dwPrice;
				break;
			}
			if (!offer)
				continue;

			const DWORD vnum = offer->GetVnum();
			const BYTE refine = offer->GetRefineLevel();
			const int goldBefore = ch->GetGold();

			// Exactly what a client does when a player clicks a stall, in the same
			// order. Both halves are required and neither is optional:
			// CShopManager::Buy returns immediately unless the buyer is registered
			// as a guest of the shop (AddGuest is what sets ch->GetShop()) *and*
			// has the shop owner set. Setting only the owner, which is the obvious
			// half, silently bought nothing at all.
			LPSHOP shop = keeper->GetMyShop();
			if (!shop || ch->GetShop() || ch->GetExchange())
				continue;
			if (!shop->AddGuest(ch, keeper->GetVID(), false))
				continue;
			ch->SetShopOwner(keeper);
			CShopManager::instance().Buy(ch, slot);
			// Leaving either of these set would point this bot at a character it
			// is no longer standing next to.
			ch->SetShopOwner(NULL);
			shop->RemoveGuest(ch);

			if (ch->GetGold() < goldBefore)
			{
				sys_log(0, "PLAYERBOT_MARKET: bought pid=%u name=%s from=%s slot=%u vnum=%u refine=%u asked=%u paid=%d gold=%d",
						ch->GetPlayerID(), ch->GetName(), keeper->GetName(),
						(unsigned int)slot, vnum, (unsigned int)refine, price,
						goldBefore - ch->GetGold(), ch->GetGold());
				return true;
			}
		}
		return false;
	}
}

#endif
