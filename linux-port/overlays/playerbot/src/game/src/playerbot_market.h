#ifndef __INC_METIN2_PLAYERBOT_MARKET_H__
#define __INC_METIN2_PLAYERBOT_MARKET_H__

// Buying from another bot's stall, and going out of one's way to do it.
//
// The stalls existed already; nobody ever bought from one, so a well-refined
// spare sat on a counter until its keeper packed up and eventually vendored it.
// This closes the loop: a bot with money in its pocket walks the market strip
// and buys what it actually needs - a refine material it is short of, a horse
// medal, or a piece of gear better than what it is wearing.
//
// The first version only bought from a counter that happened to be within
// twenty metres of wherever the bot was standing. That is not shopping, and it
// showed: twelve purchases in half an hour across seven hundred bots, all of
// them accidents of where a town errand had left somebody. So a bot that is
// short of something now walks to the ring, picks the nearest counter with that
// something on it, walks up to that counter, and buys there - which is also
// what a market is supposed to look like from the outside.
//
// It reads the offer from the seller's own AI state rather than from CShop,
// whose item list is private. That is not a workaround: we are the ones who put
// the items on the counter, in that order, so the recorded offer is exactly
// what is on it and its index is the index CShopManager::Buy expects.
//
// The purchase itself goes through the engine's ordinary path. Setting the shop
// owner is what a client does when a player clicks a stall, and everything
// after it - the price, the gold, the inventory space, moving the item - is the
// engine's own code, so a bot cannot buy anything a player could not.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_town.h - that file puts the goods on the counters and owns
// the walk into town this one borrows.

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

	// Is there anything at all a market could sell this bot? Asked before the
	// walk, so it has to be answerable without reading a single counter: these
	// are the three things a bot is reliably short of and a stall reliably has.
	bool PlayerBotWantsAnythingFromMarket(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		// A refine material for something it is carrying below its target. This
		// is the common case by a long way - half the counters in this world are
		// materials, because half of what a bot needs is.
		if (PlayerBotNeedsAnyRefineMaterial(ch))
			return true;
		// A horse medal, while there is still a horse to raise.
		if (CanPlayerBotAdvanceHorse(ch))
			return true;
		// And the level-30 weapon it would otherwise cross the world to farm.
		return ch->GetLevel() >= 30 && !HasPlayerBotSpecialLevel30Weapon(ch, false);
	}

	// One line of one counter: what a buyer decided it wants, where it is, and
	// which slot number CShopManager::Buy will need.
	struct TPlayerBotStallPick
	{
		LPCHARACTER keeper;
		DWORD dwVnum;
		DWORD dwPrice;
		BYTE bSlot;
		BYTE bRefine;

		TPlayerBotStallPick()
			: keeper(NULL), dwVnum(0), dwPrice(0), bSlot(0), bRefine(0)
		{
		}
	};

	// The nearest counter in reach with something on it this bot would rather
	// have than its money. Nearest rather than best: the counters are all within
	// the same ring, so walking past three of them to reach a fourth buys nothing
	// extra, and a bot that goes to the closest one gets there before the keeper
	// packs up.
	bool FindPlayerBotStallPick(LPCHARACTER ch, TPlayerBotStallPick& outPick)
	{
		if (!ch || !ch->GetSectree())
			return false;

		CCollectPlayerBotStalls collector(ch, PLAYERBOT_SHOPPING_RANGE);
		ch->GetSectree()->ForEachAround(collector);

		int bestDistance = -1;
		for (size_t i = 0; i < collector.m_stalls.size(); ++i)
		{
			LPCHARACTER keeper = collector.m_stalls[i];
			if (!keeper || !keeper->GetMyShop())
				continue;
			TPlayerBotAIStateMap::const_iterator it =
					s_mapPlayerBotAIStates.find(keeper->GetPlayerID());
			if (it == s_mapPlayerBotAIStates.end() || it->second.vecShopOffers.empty())
				continue;

			const int distance = DISTANCE_APPROX(ch->GetX() - keeper->GetX(),
					ch->GetY() - keeper->GetY());
			if (bestDistance >= 0 && distance >= bestDistance)
				continue; // a nearer counter has already offered something

			// A counter holds several things. Walk it and take the first line the
			// buyer actually wants; the index is the slot, because the offers were
			// recorded in the order they were handed to OpenMyShop.
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
				// Room for this particular thing, not room in general. The engine
				// refuses the whole purchase when the item does not fit, and a
				// weapon is three cells against the two the trip checked for -
				// so a bot with a two-cell gap would ask for a sword, be refused,
				// and ask again.
				if (ch->GetEmptyInventory(candidateItem->GetSize()) < 0)
					continue;
				outPick.keeper = keeper;
				outPick.dwVnum = candidate.dwVnum;
				outPick.dwPrice = candidate.dwPrice;
				outPick.bSlot = (BYTE)k;
				outPick.bRefine = candidate.bRefine;
				bestDistance = distance;
				break;
			}
		}
		return bestDistance >= 0;
	}

	// Exactly what a client does when a player clicks a stall, in the same order.
	// Both halves are required and neither is optional: CShopManager::Buy returns
	// immediately unless the buyer is registered as a guest of the shop (AddGuest
	// is what sets ch->GetShop()) *and* has the shop owner set. Setting only the
	// owner, which is the obvious half, silently bought nothing at all.
	bool BuyFromPlayerBotStall(LPCHARACTER ch, const TPlayerBotStallPick& pick)
	{
		if (!ch || !pick.keeper)
			return false;
		LPSHOP shop = pick.keeper->GetMyShop();
		if (!shop || ch->GetShop() || ch->GetExchange())
			return false;
		if (!shop->AddGuest(ch, pick.keeper->GetVID(), false))
			return false;

		const int goldBefore = ch->GetGold();
		ch->SetShopOwner(pick.keeper);
		CShopManager::instance().Buy(ch, pick.bSlot);
		// Leaving either of these set would point this bot at a character it is no
		// longer standing next to.
		ch->SetShopOwner(NULL);
		shop->RemoveGuest(ch);

		if (ch->GetGold() >= goldBefore)
			return false;

		sys_log(0, "PLAYERBOT_MARKET: bought pid=%u name=%s from=%s slot=%u vnum=%u refine=%u asked=%u paid=%d gold=%d",
				ch->GetPlayerID(), ch->GetName(), pick.keeper->GetName(),
				(unsigned int)pick.bSlot, pick.dwVnum, (unsigned int)pick.bRefine,
				pick.dwPrice, goldBefore - ch->GetGold(), ch->GetGold());
		return true;
	}

	// Ending a trip says why, the way closing a stall does. Without it the only
	// measurable thing about shopping was the purchases, and a market with no
	// purchases could equally mean nobody set off, nobody arrived, or nobody
	// found anything - three different faults with three different fixes.
	void EndPlayerBotMarketTrip(LPCHARACTER ch, TPlayerBotAIState& state,
			const char* reason)
	{
		if (ch && state.bMarketTrip)
			sys_log(0, "PLAYERBOT_MARKET: trip over pid=%u name=%s reason=%s pos=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), reason, ch->GetX(), ch->GetY());
		state.bMarketTrip = false;
		state.dwMarketTripUntil = 0;
		state.dwMarketBrowseTime = 0;
		state.dwMarketStallVID = 0;
	}

	// Money it may actually spend, and somewhere to put what it buys. Both are
	// re-asked every tick of a trip: a bot whose bag filled up on the way has
	// nothing left to go to the market for.
	bool CanPlayerBotAffordMarket(LPCHARACTER ch)
	{
		return ch && ch->GetGold() - GetPlayerBotReservedGold(ch) >
					(int)PLAYERBOT_SHOPPING_GOLD_FLOOR &&
				ch->GetEmptyInventory(2) >= 0;
	}

	// One tick of a shopping trip: read the counters now and then, walk to the
	// one that has something, and buy when standing at it. Claims the tick for as
	// long as the trip lasts, which is what keeps the bot walking instead of
	// planning a hunt halfway across the market.
	bool ContinuePlayerBotMarketTrip(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, long pitchX, long pitchY)
	{
		if (dwNow >= state.dwMarketTripUntil || !CanPlayerBotAffordMarket(ch))
		{
			EndPlayerBotMarketTrip(ch, state,
					dwNow >= state.dwMarketTripUntil ? "timeout" : "broke");
			return false;
		}
		SetPlayerBotAction(state, BOT_ACTION_MARKET, dwNow);

		// The counters change while their customer is walking over - a keeper
		// packs up, another opens - so what the bot is heading for is re-decided
		// every couple of seconds rather than once at the start of the trip.
		TPlayerBotStallPick pick;
		bool havePick = false;
		if (dwNow >= state.dwMarketBrowseTime)
		{
			state.dwMarketBrowseTime = dwNow + PLAYERBOT_MARKET_BROWSE_INTERVAL;
			havePick = FindPlayerBotStallPick(ch, pick);
			state.dwMarketStallVID = havePick ? pick.keeper->GetVID() : 0;
			if (!havePick &&
					DISTANCE_APPROX(ch->GetX() - pitchX, ch->GetY() - pitchY) <=
						PLAYERBOT_SHOP_RING_RADIUS)
			{
				// Standing in the ring with nothing on it worth buying. The trip
				// is over rather than a bot loitering for another minute.
				EndPlayerBotMarketTrip(ch, state, "nothing_on_offer");
				return false;
			}
		}

		// A keeper that has packed up since the last look stops being a
		// destination, and the bot falls back on the middle of the ring.
		LPCHARACTER keeper = state.dwMarketStallVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwMarketStallVID) : NULL;
		if (keeper && !keeper->GetMyShop())
		{
			keeper = NULL;
			state.dwMarketStallVID = 0;
		}

		if (!MovePlayerBotTownLeg(ch, state, dwNow,
				keeper ? keeper->GetX() : pitchX,
				keeper ? keeper->GetY() : pitchY,
				keeper ? PLAYERBOT_MARKET_STALL_APPROACH : PLAYERBOT_MARKET_ARRIVE))
			return true; // still walking

		if (keeper && havePick && pick.keeper == keeper)
		{
			if (!BuyFromPlayerBotStall(ch, pick))
			{
				// The engine said no - no room for that size, not enough gold,
				// the line sold to somebody else while this bot walked over - and
				// it says so at a log level nobody runs with. Whatever it was, it
				// will be just as true on the next tick, so asking again only
				// produces one Shop::Buy per second until the trip times out.
				// Which is precisely what an operator photographed.
				EndPlayerBotMarketTrip(ch, state, "refused");
				return false;
			}
			// Bought. A bot that came for two things gets the second without
			// walking off, but through the ordinary browse interval rather than
			// on this same tick.
			state.dwMarketStallVID = 0;
			state.dwMarketBrowseTime = dwNow + PLAYERBOT_MARKET_BROWSE_INTERVAL;
		}
		return true;
	}

	bool ManagePlayerBotShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return false;
		// A keeper minding its own counter is not also a customer.
		if (ch->GetMyShop() || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession)
		{
			EndPlayerBotMarketTrip(ch, state, "busy");
			return false;
		}
		// Stalls stand in both towns - most of them in Joan, round the village
		// guard - and nowhere else, so the pitch is both the test for "is there a
		// market here" and the place a shopping trip walks to.
		long pitchX = 0, pitchY = 0;
		if (!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY) ||
				!ch->GetSectree())
		{
			EndPlayerBotMarketTrip(ch, state, "left_town");
			return false;
		}

		if (state.bMarketTrip)
			return ContinuePlayerBotMarketTrip(ch, state, dwNow, pitchX, pitchY);

		if (dwNow < state.dwNextShoppingTime)
			return false;
		state.dwNextShoppingTime = dwNow + number(
				PLAYERBOT_SHOPPING_INTERVAL_MIN, PLAYERBOT_SHOPPING_INTERVAL_MAX);
		if (!CanPlayerBotAffordMarket(ch))
			return false;

		// bVisitingShop does not stop a bot buying from a counter it is already
		// standing beside - a bot anywhere near the market is on a town errand
		// almost by definition, and excluding them left the market with no
		// customers at all. It does stop it walking off across town: the errand
		// owns the bot's feet until it is finished.
		TPlayerBotStallPick pick;
		const bool haveStallInReach = FindPlayerBotStallPick(ch, pick);
		if (state.bVisitingShop)
			return haveStallInReach && BuyFromPlayerBotStall(ch, pick);

		// Something in reach, or a reason to go and look: either way it is a trip,
		// so the bot walks up to the counter instead of buying from twenty metres
		// off. That is the difference between a market and a vending machine.
		if (!haveStallInReach && !PlayerBotWantsAnythingFromMarket(ch))
			return false;
		if (!haveStallInReach &&
				DISTANCE_APPROX(ch->GetX() - pitchX, ch->GetY() - pitchY) >
					PLAYERBOT_MARKET_TRIP_RANGE)
			return false; // wants something, but the market is a hunt away

		state.bMarketTrip = true;
		state.dwMarketTripUntil = dwNow + PLAYERBOT_MARKET_TRIP_TIMEOUT;
		state.dwMarketBrowseTime = dwNow + PLAYERBOT_MARKET_BROWSE_INTERVAL;
		state.dwMarketStallVID = haveStallInReach ? pick.keeper->GetVID() : 0;
		sys_log(0, "PLAYERBOT_MARKET: trip pid=%u name=%s map=%ld stall=%u pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
				(unsigned int)state.dwMarketStallVID, ch->GetX(), ch->GetY());
		return ContinuePlayerBotMarketTrip(ch, state, dwNow, pitchX, pitchY);
	}
}

#endif
