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
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	bool ManagePlayerBotOfflineShopping(LPCHARACTER, TPlayerBotAIState&, DWORD);
	void AddPlayerBotOfflineLedger(DWORD&, DWORD&);
	bool FindPlayerBotFarOfflinePick(LPCHARACTER, TPlayerBotAIState&, long);
	bool HandPlayerBotFarPickToBuyer(LPCHARACTER, TPlayerBotAIState&, DWORD);
	void ClaimPlayerBotFarLine(DWORD, DWORD, DWORD);
#endif
	// Defined with the chat trade, after this file: the bot that found the
	// market empty of what it came for asks the world channel.
	void AnnouncePlayerBotNeed(LPCHARACTER ch);

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

	// What a bot saves up for rather than buys on a whim: see
	// PLAYERBOT_STRATEGIC_BUDGET_PERCENT.
	bool IsPlayerBotStrategicPurchase(DWORD vnum)
	{
		return IsPlayerBotSpecialLevel30WeaponVnum(vnum) || vnum == PLAYERBOT_HORSE_MEDAL_VNUM ||
				IsPlayerBotSafeRefineScroll(vnum);
	}

	// The most a strategic purchase may cost this bot: a share of what it can
	// spend above the reserve and the shopping floor.
	long long GetPlayerBotStrategicPurchaseCap(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		return spare > 0 ? spare * PLAYERBOT_STRATEGIC_BUDGET_PERCENT / 100 : 0;
	}

	// A Moonlight chest is opened, so a bot with room buys one off a counter
	// (Tieru, 15 September: "wazne przedmioty dla botow, duzo fajnych itemow im
	// z tego dropi"). No rule wanted one before: every chest a trader listed
	// stayed listed - 3 000 on AkhiGubernator's counters and not one sold. Not a
	// trader, which sells them; nobody under PLAYERBOT_CHEST_BUY_MIN_LEVEL or
	// holding PLAYERBOT_CHEST_BUY_HOLD already; and only into a bag that takes
	// the chest's whole group and the engine's free column of three.
	// When each bot last bought a chest, for PLAYERBOT_CHEST_BUY_COOLDOWN.
	std::map<DWORD, DWORD> s_mapPlayerBotChestBoughtAt;

	void NotePlayerBotChestBought(DWORD dwPlayerID, DWORD dwNow)
	{
		s_mapPlayerBotChestBoughtAt[dwPlayerID] = dwNow;
	}

	bool WantsPlayerBotMoonlightChest(LPCHARACTER ch)
	{
		// The counters are not emptied for the players' sake: while the ledger
		// counts no more than the reserve on every counter of the world, or this
		// bot bought one within the cooldown, the answer is no.
		if (ch)
		{
			const TPlayerBotMarketLedgerEntry* chests =
					GetPlayerBotMarketLedgerEntry(PLAYERBOT_MOONLIGHT_CHEST_VNUM);
			if (chests && chests->dwSupplyUnits <= PLAYERBOT_CHEST_MARKET_RESERVE)
				return false;
			std::map<DWORD, DWORD>::const_iterator bought =
					s_mapPlayerBotChestBoughtAt.find(ch->GetPlayerID());
			if (bought != s_mapPlayerBotChestBoughtAt.end() &&
					get_dword_time() - bought->second < PLAYERBOT_CHEST_BUY_COOLDOWN)
				return false;
		}
		if (!ch || !ch->IsItemLoaded() || (int)ch->GetLevel() < PLAYERBOT_CHEST_BUY_MIN_LEVEL ||
				IsPlayerBotResourceTrader(ch->GetPlayerID()) ||
				IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) ||
				(long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
					(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR <
					MAX(PLAYERBOT_CHEST_BUY_MIN_GOLD, PLAYERBOT_CHEST_BUY_PRICE_MULTIPLE *
						(long long)GetPlayerBotMaterialAskingBase(PLAYERBOT_MOONLIGHT_CHEST_VNUM)) ||
				(int)ch->CountSpecifyItem(PLAYERBOT_MOONLIGHT_CHEST_VNUM) >= PLAYERBOT_CHEST_BUY_HOLD ||
				CountPlayerBotFreeInventoryCells(ch) < PLAYERBOT_CHEST_BUY_MIN_FREE_CELLS ||
				ch->GetEmptyInventory(3) < 0)
			return false;
		int cellsNeeded = 0;
		return PlayerBotBagTakesGroup(ch, PLAYERBOT_MOONLIGHT_CHEST_VNUM, cellsNeeded);
	}

	// A bot with a weapon that goes to the anvil under scrolls - one it may
	// refine no other way, a level-30 weapon in its hand, the one it is
	// grinding, or the only weapon it has at a step that burns - buys a few,
	// up to PLAYERBOT_LEVEL30_SCROLL_WANT.
	bool PlayerBotNeedsScrollForWeapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() ||
				CountPlayerBotSafeRefineScrolls(ch) >= PLAYERBOT_LEVEL30_SCROLL_WANT)
			return false;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetRefinedVnum() != 0 &&
				(IsPlayerBotScrollOnlyWeapon(worn) || IsPlayerBotSpecialLevel30Weapon(worn)))
			return true;
		// The weapon in the hand the anvil would burn with nothing behind it
		// (IsPlayerBotWornWeaponAtRisk), while it is short of its target: a rich
		// bot buys its way past the hold rather than waiting for a drop.
		if (worn && worn->GetRefineLevel() < GetPlayerBotRefineTarget(ch, worn) &&
				IsPlayerBotWornWeaponAtRisk(ch, worn))
			return true;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		return view.project != NULL && view.project->GetRefinedVnum() != 0;
	}

	// Would this bot rather have the item than the money?
	bool WantsPlayerBotStallItem(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer)
			return false;

		// Development demand is shared with the journey and own-shop reclaim.
		if (offer->GetType() == ITEM_SKILLBOOK || offer->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return IsPlayerBotProgressionOffer(ch, offer);
		if (GetPlayerBotBiologistPurchaseNeed(ch, offer->GetVnum()) > 0)
			return IsPlayerBotProgressionOffer(ch, offer);

		// The bean for a bot standing out a negative rank in town: the one thing
		// that lifts it there (KeepPlayerBotNegativeRankInTown), one at a time.
		if (offer->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM)
			return ch->GetRealAlignment() < 0 &&
					ch->CountSpecifyItem(PLAYERBOT_ZEN_BEAN_VNUM) == 0;

		// A Moonlight chest, to open (WantsPlayerBotMoonlightChest).
		if (offer->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return WantsPlayerBotMoonlightChest(ch);

		// A material it is short of right now. This is the whole reason a bot
		// walks the market: the alternative is farming the same material for an
		// hour while a neighbour has spares on a counter three metres away.
		if (PlayerBotNeedsRefineMaterial(ch, offer->GetVnum()))
			return true;

		// Iwakura's gambler buys what its session runs on (playerbot_gambler.h).
		if (WantsPlayerBotGambleOffer(ch, offer))
			return true;

		// A skill book for a skill this bot is actually raising.
		//
		// There was no branch for these at all, so no bot ever bought one off a
		// counter: books piled up on stalls, and the only way to a skill was to
		// find the book yourself. What a bot wants is a small working stock of
		// its own build's skills - two or three, not every book to Grand Master
		// - and only while the skill can still be read up.
		// A Forgetting Scroll on somebody's counter is what a bot past the old
		// woman's thirty with a skill stuck at seventeen came to market for.
		if (offer->GetVnum() == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return GetPlayerBotStuckSkill(ch) != 0 &&
					ch->GetLevel() > PLAYERBOT_SKILL_RESET_MAX_LEVEL &&
					ch->CountSpecifyItem(PLAYERBOT_SKILL_FORGET_SCROLL_VNUM) == 0;

		// A horse medal, if this bot still has a horse to raise. Buying one is
		// hours of the Monkey Dungeon it does not have to run.
		if (offer->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return CanPlayerBotAdvanceHorse(ch);

		// A Forgetting Scroll, while a skill stands at seventeen unmastered.
		if (offer->GetVnum() == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return GetPlayerBotStuckSkill(ch) != 0;

		// A soul stone of its set, at a grade its piece deserves, for a socket
		// it has open.
		if (offer->GetType() == ITEM_METIN)
			return WantsPlayerBotSoulStone(ch, offer->GetVnum(), (DWORD)offer->GetValue(5));

		// A level-30 weapon of its own class. This is the item bots cross the
		// world to farm; buying one off a counter is the whole point of there
		// being a market. It used to be "when it has none", so a bot holding any
		// level-30 weapon at all - a +0 with no line in the bag - never looked
		// at a better one. Now it is the damage model's answer: the offer's blow
		// at PLAYERBOT_LEVEL30_PROJECT_PLUS against the best the bot has, its
		// own project included (IsPlayerBotBetterLevel30Offer).
		if (IsPlayerBotSpecialLevel30Weapon(offer))
			return IsPlayerBotBetterLevel30Offer(ch, offer) || IsPlayerBotMandatedLevel30Offer(ch, offer);
		// And a refine scroll, for a weapon that is refined under one.
		if (IsPlayerBotSafeRefineScroll(offer->GetVnum()) && PlayerBotNeedsScrollForWeapon(ch))
			return true;
		// A key for a chest it holds and cannot open.
		if (offer->GetType() == ITEM_TREASURE_KEY)
			return PlayerBotWantsTreasureKey(ch, offer);

		// Gear only when it is genuinely better than what is worn. A bot that
		// buys sideways upgrades spends its yang on nothing.
		if (!IsPlayerBotEquipmentCandidate(ch, offer))
			return false;
		if (offer->GetLevelLimit() > ch->GetLevel())
			return false;
		const int wearCell = offer->FindEquipCell(ch);
		if (wearCell < 0)
			return false;
		// Not when the bag already holds one at least as good for the same
		// slot. The comparison below is against what is worn, and what is
		// worn does not change until the gear pass runs - so a bot standing at
		// the ring bought the same +6 armour three times over, two seconds
		// apart, each one better than what it had on and none of them on yet.
		const long long offerScore = GetPlayerBotEquipmentScore(offer, ch);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM spare = ch->GetInventoryItem(cell);
			if (!spare || spare->IsEquipped() || !IsPlayerBotEquipmentCandidate(ch, spare) ||
					spare->FindEquipCell(ch) != wearCell)
				continue;
			if (GetPlayerBotEquipmentScore(spare, ch) * (100 + PLAYERBOT_MARKET_GEAR_MARGIN_PERCENT) / 100 >= offerScore)
				return false;
		}
		LPITEM worn = ch->GetWear((BYTE)wearCell);
		if (!worn)
			return true;
		// A big bonus line is worth having even when the base item scores level
		// with what is worn - a thousand health does not show up in the equipment
		// score, and it is exactly what a player would buy the piece for.
		if (HasPlayerBotValuableBonus(offer) && !HasPlayerBotValuableBonus(worn))
			return true;
		return offerScore >
				GetPlayerBotEquipmentScore(worn, ch) * (100 + PLAYERBOT_MARKET_GEAR_MARGIN_PERCENT) / 100;
	}

	// Is there anything at all a market could sell this bot? Asked before the
	// walk, so it has to be answerable without reading a single counter: these
	// are the three things a bot is reliably short of and a stall reliably has.
	bool PlayerBotWantsAnythingFromMarket(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (PlayerBotNeedsProgressionShopping(ch)) return true;
		// A bean for a negative rank (WantsPlayerBotStallItem).
		if (ch->GetRealAlignment() < 0 && ch->CountSpecifyItem(PLAYERBOT_ZEN_BEAN_VNUM) == 0)
			return true;
		// A Moonlight chest, while some counter holds one: the ledger counts the
		// counters (AddPlayerBotMarketSupply, the offline shops included), so the
		// question is still answered without reading one. Without this branch the
		// chest was bought only on a trip made for something else.
		{
			const TPlayerBotMarketLedgerEntry* chests =
					GetPlayerBotMarketLedgerEntry(PLAYERBOT_MOONLIGHT_CHEST_VNUM);
			if (chests && chests->dwSupplyUnits > 0 && WantsPlayerBotMoonlightChest(ch))
				return true;
		}
		// A refine material for something it is carrying below its target. This
		// is the common case by a long way - half the counters in this world are
		// materials, because half of what a bot needs is.
		if (PlayerBotNeedsAnyRefineMaterial(ch))
			return true;
		// A horse medal, while there is still a horse to raise.
		if (CanPlayerBotAdvanceHorse(ch))
			return true;
		// A Forgetting Scroll for a skill stuck at seventeen.
		if (GetPlayerBotStuckSkill(ch) != 0)
			return true;
		// A socket open on a piece it keeps.
		if (PlayerBotHasOpenSoulStoneSocket(ch))
			return true;
		// And a piece of gear for a slot that is empty or behind the ladder.
		//
		// This branch was missing, and it is the whole of why "I put +8 battle
		// shields on a stall for one yang and the bots would not buy them"
		// happens: WantsPlayerBotStallItem has always known how to compare an
		// offered piece against what is worn, but nothing ever walked a bot to
		// a counter to look. Gear was reachable only by accident, on a trip the
		// bot made for a refine material. The question has to be answerable
		// without reading a counter, and these predicates are exactly that -
		// the same ones the tick uses to decide a merchant trip is due.
		if (NeedsPlayerBotProgressionWeapon(ch) || NeedsPlayerBotProgressionArmor(ch) ||
				NeedsPlayerBotProgressionShield(ch) || NeedsPlayerBotProgressionHelmet(ch) ||
				NeedsPlayerBotProgressionBoots(ch) || NeedsPlayerBotProgressionWrist(ch) ||
				NeedsPlayerBotProgressionNecklace(ch) || NeedsPlayerBotProgressionEarring(ch))
			return true;
		// A scroll for a weapon that is refined under one.
		if (PlayerBotNeedsScrollForWeapon(ch))
			return true;
		// A weapon the atlas says it has outgrown, when it could pay for the one
		// it is after (playerbot_weapon_goal.h).
		{
			const TPlayerBotWeaponGoal& goal = GetPlayerBotWeaponGoal(ch, get_dword_time());
			if (IsPlayerBotWeaponOutclassed(goal) &&
					GetPlayerBotStrategicPurchaseCap(ch) >= (long long)GetPlayerBotWeaponGoalPrice(goal.family))
				return true;
		}
		// The finished piece a market Perfectionist's anvil is waiting for
		// (community patch 2, point 11).
		if (IsPlayerBotPersonaEnabled())
		{
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
			if (st != s_mapPlayerBotAIStates.end() && st->second.persona.dwReadyGearWaitUntil != 0 &&
					get_dword_time() < st->second.persona.dwReadyGearWaitUntil)
				return true;
		}
		// The class's level-30 weapon, which a bot of thirty or more without one
		// has to have (community patch 2, point 1), while a counter holds one
		// and it can pay for it.
		if (PlayerBotLacksClassLevel30Weapon(ch) && PlayerBotMarketHasClassLevel30Weapon(ch) &&
				GetPlayerBotLevel30PurchaseCap(ch) >=
					(long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_LEVEL30_BASE_PRICE))
			return true;
		// And the level-30 weapon it would otherwise cross the world to farm -
		// unless it is grinding one already, wears a finished one, no such
		// weapon could beat what it has (PlayerBotCouldUseLevel30Weapon), or it
		// could not pay for one.
		return PlayerBotCouldUseLevel30Weapon(ch) &&
				GetPlayerBotStrategicPurchaseCap(ch) >=
					(long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_LEVEL30_BASE_PRICE);
	}

	bool CanPlayerBotPayForOffer(LPCHARACTER ch, LPITEM item, long long price) {
		if (!ch || !item || price <= 0) return false;
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) - PLAYERBOT_SHOPPING_GOLD_FLOOR;
		if (price > spare) return false;
		if (IsPlayerBotProgressionOffer(ch, item)) {
			const long long fair = GetPlayerBotShopAskingPrice(item);
			// A book comes out of the visit's book purse (community patch 2,
			// point 5), which counts what the visit has already spent on books.
			if (item->GetType() == ITEM_SKILLBOOK)
				return fair > 0 && price <= fair * 2 && price <= GetPlayerBotBookBudgetLeft(ch);
			return fair > 0 && price <= fair * 2 && price <= spare * 30 / 100;
		}
		// The class's level-30 weapon has its own share (community patch 2,
		// point 1): PLAYERBOT_LEVEL30_BUDGET_PERCENT for the purchase and the
		// anvil together.
		if (IsPlayerBotClassLevel30Weapon(ch, item))
			return price <= GetPlayerBotLevel30PurchaseCap(ch);
		// A finished piece the market Perfectionist came for is paid from the
		// Perfectionist's share (community patch 2, point 11).
		if (IsPlayerBotReadyGearOffer(ch, item))
			return price <= spare * playerbot_persona::PERFECT_BUDGET_PERCENT / 100;
		if (IsPlayerBotStrategicPurchase(item->GetVnum()) || IsPlayerBotStrategicWeaponOffer(ch, item))
			return price <= GetPlayerBotStrategicPurchaseCap(ch);
		const long long cap = (long long)GetPlayerBotMarketMedianWallet() * PLAYERBOT_MARKET_STACK_WALLET_PERCENT / 100;
		return cap <= 0 || price <= cap;
	}

	// One line of one counter: what a buyer decided it wants, where it is, and
	// which slot number CShopManager::Buy will need.
	struct TPlayerBotStallPick
	{
		LPCHARACTER keeper;
		DWORD dwVnum;
		DWORD dwPrice;
		// Which skill, for a skill book. The purchase is recorded against the
		// book's own market, not against every book that shares vnum 50300.
		DWORD dwSkillVnum;
		BYTE bSlot;
		BYTE bRefine;
		WORD wCount;

		TPlayerBotStallPick()
			: keeper(NULL), dwVnum(0), dwPrice(0), dwSkillVnum(0), bSlot(0),
			  bRefine(0), wCount(0)
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
			// A player's counter is read from the engine's own shop through the
			// accessor patch 0007 adds - the offer list only exists for bots. A
			// line a player asks more than a share of the median wallet for is
			// passed over: the bots are customers, not a way to print yang.
			std::vector<TPlayerBotShopOffer> playerOffers;
			if (it == s_mapPlayerBotAIStates.end())
			{
				if (keeper->GetDesc() && keeper->GetDesc()->IsBot())
					continue;
				const std::vector<CShop::SHOP_ITEM>& lines = keeper->GetMyShop()->GetItemVector();
				for (size_t k = 0; k < lines.size(); ++k)
				{
					const CShop::SHOP_ITEM& line = lines[k];
					if (!line.pkItem || line.vnum == 0 || !CanPlayerBotPayForOffer(ch, line.pkItem, line.price))
						continue;
					TPlayerBotShopOffer offer;
					offer.dwVnum = line.vnum;
					offer.dwPrice = (DWORD)line.price;
					offer.bRefine = line.pkItem->GetRefineLevel();
					offer.wCount = line.count;
					offer.dwItemID = (DWORD)line.itemid;
					offer.bSlot = (BYTE)k;
					// A player's counter, read only to decide what to buy from
					// it: nothing here asks this offer for a book's skill, and
					// the demand signal is the keeper's own bookkeeping. Set
					// anyway - TPlayerBotShopOffer has no constructor, so a
					// field left alone is whatever was on the stack.
					offer.dwSkillVnum = 0;
					playerOffers.push_back(offer);
				}
			}
			const std::vector<TPlayerBotShopOffer>& offers =
					it != s_mapPlayerBotAIStates.end() ? it->second.vecShopOffers : playerOffers;
			if (offers.empty())
				continue;

			const int distance = DISTANCE_APPROX(ch->GetX() - keeper->GetX(),
					ch->GetY() - keeper->GetY());
			if (bestDistance >= 0 && distance >= bestDistance)
				continue; // a nearer counter has already offered something

			// A counter holds several things. Walk it and take the first line the
			// buyer actually wants; the offer carries the engine's slot for it.
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TPlayerBotShopOffer& candidate = offers[k];
				// Never spend down to nothing: potions and the next weapon first.
				if (candidate.dwPrice == 0 ||
						ch->GetGold() - (int)candidate.dwPrice <
							(int)PLAYERBOT_SHOPPING_GOLD_FLOOR)
					continue;
				// The real item, with its sockets and bonus lines, is still in
				// the keeper's bag to be looked at - or it is sold, and it is not.
				LPITEM candidateItem = FindPlayerBotOfferItem(keeper, candidate);
				if (!WantsPlayerBotStallItem(ch, candidateItem) ||
						!CanPlayerBotPayForOffer(ch, candidateItem, candidate.dwPrice))
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
				outPick.bSlot = candidate.bSlot;
				outPick.bRefine = candidate.bRefine;
				outPick.wCount = candidate.wCount;
				outPick.dwSkillVnum = candidateItem->GetType() == ITEM_SKILLBOOK
						? GetPlayerBotSkillBookSkillVnum(candidateItem) : 0;
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
		// Revalidate the native slot immediately before sending the buy.
		const std::vector<CShop::SHOP_ITEM>& lines = shop->GetItemVector();
		if (pick.bSlot >= lines.size()) return false;
		const CShop::SHOP_ITEM& line = lines[pick.bSlot];
		if (!line.pkItem || line.price != pick.dwPrice || !WantsPlayerBotStallItem(ch, line.pkItem) ||
				!CanPlayerBotPayForOffer(ch, line.pkItem, line.price)) return false;
		if (!shop->AddGuest(ch, pick.keeper->GetVID(), false))
			return false;

		const int goldBefore = ch->GetGold();
		// Read before the purchase: a sold line's item is the buyer's after it.
		const bool book = line.pkItem->GetType() == ITEM_SKILLBOOK;
		ch->SetShopOwner(pick.keeper);
		CShopManager::instance().Buy(ch, pick.bSlot);
		// Leaving either of these set would point this bot at a character it is no
		// longer standing next to.
		ch->SetShopOwner(NULL);
		shop->RemoveGuest(ch);

		if (ch->GetGold() >= goldBefore)
			return false;

		const int paid = goldBefore - ch->GetGold();
		if (book)
			NotePlayerBotBookBought(ch, paid);
		// A sale is the one measurement of demand there is. The asking price on
		// a counter is what a seller hoped for; this is what a buyer did.
		// With the skill, so a book sale lands on its own market.
		RememberPlayerBotSale(pick.dwVnum, pick.bRefine,
				(DWORD)paid / std::max<DWORD>(1, pick.wCount), get_dword_time(),
				pick.dwSkillVnum);
		if (pick.dwVnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			NotePlayerBotChestBought(ch->GetPlayerID(), get_dword_time());
		// A gambler's purchase is charged to the session's budget.
		NotePlayerBotGamblePurchase(ch, paid);
		sys_log(0, "PLAYERBOT_MARKET: bought pid=%u name=%s from=%s slot=%u vnum=%u refine=%u count=%u asked=%u paid=%lld gold=%lld",
				ch->GetPlayerID(), ch->GetName(), pick.keeper->GetName(),
				(unsigned int)pick.bSlot, pick.dwVnum, (unsigned int)pick.bRefine,
				(unsigned int)pick.wCount, pick.dwPrice, (long long)paid, (long long)ch->GetGold());
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
		// Joan was looked at and had nothing this bot wanted, so Bokjung is
		// worth a walk for a while. Without this a shopper would cross to the
		// quiet market for ever and never see the busy one.
		// A walk to Joan that ran out of time counts as Joan looked at, or the
		// next shopping pass would set off again from wherever it gave up.
		if (ch && state.bMarketTrip &&
				(IsPlayerBotM1Map(ch->GetMapIndex()) || state.bMarketToJoan))
			state.dwMarketM2AllowedUntil = get_dword_time() +
					PLAYERBOT_MARKET_M2_FALLBACK;
		state.bMarketTrip = false;
		state.bMarketToJoan = false;
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
		// The first leg of a "Joan first" trip: keep walking the portal until
		// the map changes. The shopping pass runs every two to five minutes,
		// and asking for the portal once left the route to the tick's
		// continuation passes - which walked the bot to the gate cell and
		// then handed it to the wander. 152 of 160 walks to that gate in ten
		// minutes were this, with one crossing; the bots stood at the gate
		// with "Sohan" or "Monkey Dungeon" over their heads and rode off.
		if (state.bMarketToJoan)
		{
			if (!IsPlayerBotM2Map(ch->GetMapIndex()))
			{
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
				// Across. On this engine the walk was made for one line of one
				// stand (StartPlayerBotFarMarketWalk), and the classic browse
				// below has nothing to read here: a keeper's goods are a shop
				// entity, not a counter it stands behind. The buyer takes it on.
				EndPlayerBotMarketTrip(ch, state, "arrived");
				return HandPlayerBotFarPickToBuyer(ch, state, dwNow);
#endif
				state.bMarketToJoan = false;
				state.dwMarketTripUntil = dwNow + PLAYERBOT_MARKET_TRIP_TIMEOUT;
				state.dwMarketBrowseTime = dwNow;
			}
			else
			{
				if (state.lDepartureMap != 0)
				{
					EndPlayerBotMarketTrip(ch, state, "departure_set");
					return false;
				}
				// This kingdom's own gate and this kingdom's own first village.
				// The walk is the same one it has always been; which market it
				// ends at is whichever one the bot's second village opens onto.
				const int owner = playerbot_empire_rules::GetMapOwnerEmpire(ch->GetMapIndex());
				const long firstVillage = playerbot_empire_rules::GetHomeMap(owner,
						playerbot_empire_rules::MAP_ROLE_M1);
				playerbot_empire_rules::TKingdomGate gate;
				playerbot_empire_rules::TPoint pitch;
				if (!playerbot_empire_rules::FindKingdomGate(owner, ch->GetMapIndex(),
							firstVillage, gate) ||
						!playerbot_empire_rules::GetTownPitch(firstVillage, pitch))
				{
					EndPlayerBotMarketTrip(ch, state, "no_gate_home");
					return false;
				}
				return MovePlayerBotToWorldPortal(ch, state,
						gate.gate.x, gate.gate.y,
						firstVillage, pitch.x, pitch.y, dwNow, "market_to_m1");
			}
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
				// is over rather than a bot loitering for another minute - and
				// the world channel hears what it came for.
				EndPlayerBotMarketTrip(ch, state, "nothing_on_offer");
				AnnouncePlayerBotNeed(ch);
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

		// Standing at the counter. The line is read again now, whatever the
		// browse clock says: what was on it two seconds ago is what the bot
		// walked over for, what is on it this tick is what it can buy. Gone,
		// and the bot heads for the next counter that has it, or the ring.
		if (keeper && !havePick)
		{
			havePick = FindPlayerBotStallPick(ch, pick);
			state.dwMarketBrowseTime = dwNow + PLAYERBOT_MARKET_BROWSE_INTERVAL;
			state.dwMarketStallVID = havePick ? pick.keeper->GetVID() : 0;
			if (!havePick || pick.keeper != keeper)
				return true;
		}
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
			// on this same tick - and the gear pass runs first, so a bought piece
			// is worn before the next counter is read.
			state.dwNextEquipmentCheckTime = dwNow;
			state.dwMarketStallVID = 0;
			state.dwMarketBrowseTime = dwNow + PLAYERBOT_MARKET_BROWSE_INTERVAL;
		}
		return true;
	}

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	// The one walk the offline buyer cannot make: from a second village over
	// to its own first village's stands, and only for a line one of them holds
	// that this bot would buy and can pay for - the same rule the buyer asks
	// in reach, applied to the far market first (FindPlayerBotFarOfflinePick).
	// The walk keeps "Joan first"'s own conditions: not for a bot on its way
	// to the frontier or the Monkey Dungeon, nor one serving a person.
	bool StartPlayerBotFarMarketWalk(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long pitchX, long pitchY)
	{
		if (!IsPlayerBotM2Map(ch->GetMapIndex()) || dwNow < state.dwMarketM2AllowedUntil ||
				state.lDepartureMap != 0 || GetPlayerBotFrontierMapForLevel(ch) != 0 ||
				state.bLongTermGoal == BOT_GOAL_HORSE ||
				(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
				IsPlayerBotHeldForCompany(ch) || !PlayerBotWantsAnythingFromMarket(ch))
			return false;
		const int owner = playerbot_empire_rules::GetMapOwnerEmpire(ch->GetMapIndex());
		const long firstVillage = playerbot_empire_rules::GetHomeMap(owner,
				playerbot_empire_rules::MAP_ROLE_M1);
		if (!FindPlayerBotFarOfflinePick(ch, state, firstVillage))
			return false;
		// The stands are the first channel's (playerbot_channel_rules.h): a bot
		// on the second asks to be moved, as the buyer does for a line in
		// reach, and looks again once it is there.
		if (CPlayerBotManager::instance().IsChannelTableMode() &&
				g_bChannel != playerbot_channel_rules::SHOP_CHANNEL)
		{
			state.offlineShop.farPickOwner = state.offlineShop.farPickItem = 0;
			if (playerbot_offline::Due(dwNow, state.dwNextBuyChannelRequestTime))
			{
				state.dwNextBuyChannelRequestTime = dwNow + PLAYERBOT_SHOP_CHANNEL_BUY_REQUEST_GAP_MS;
				if (CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID()))
					PlayerBotLogThrottled("shop_channel_far_buy", dwNow,
							"PLAYERBOT_CHANNEL: pid=%u name=%s asks for the shop channel to buy in its first village (here %u)",
							ch->GetPlayerID(), ch->GetName(), (unsigned int)g_bChannel);
			}
			return false;
		}
		state.bMarketTrip = true;
		state.bMarketToJoan = true;
		state.dwMarketTripUntil = dwNow + PLAYERBOT_MARKET_JOAN_WALK_TIMEOUT;
		state.dwMarketBrowseTime = 0;
		state.dwMarketStallVID = 0;
		ClaimPlayerBotFarLine(state.offlineShop.farPickItem, ch->GetPlayerID(), dwNow);
		sys_log(0, "PLAYERBOT_MARKET: looking in Joan first pid=%u name=%s pos=(%ld,%ld) owner=%u item=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
				state.offlineShop.farPickOwner, state.offlineShop.farPickItem);
		return ContinuePlayerBotMarketTrip(ch, state, dwNow, pitchX, pitchY);
	}
#endif

	bool ManagePlayerBotShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return false;
		// The Demon Tower first (playerbot_demon_tower.h).
		if (IsPlayerBotOnTowerBusiness(ch, state))
			return false;
		// A dropper farms one thing for the counters and buys nothing off them.
		// The medal droppers went shopping all the same: 350 trips for 116 of
		// them in the first twenty-five minutes after a restart, 75 of them a
		// walk from the second village back to the first, while three of them
		// reached the Monkey Dungeon ("lataja po m2", sizowski). A dropper
		// standing out a negative rank in town may shop, for the bean that lifts
		// it (KeepPlayerBotNegativeRankInTown).
		if (IsPlayerBotDropper(state.bPersonality) && ch->GetRealAlignment() >= 0)
		{
			EndPlayerBotMarketTrip(ch, state, "dropper");
			return false;
		}
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (ManagePlayerBotOfflineShopping(ch, state, dwNow)) return true;
		if (playerbot_offline::requests.count(ch->GetPlayerID())) return false;
#endif
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
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		// On this engine the bots' counters are offline shops, and the buyer
		// at the top of this pass reads every one in reach on its own clock.
		// The trip below looks only for a keeper standing behind a counter -
		// there are none - so it ended "nothing_on_offer" every time: 4 406
		// trips in an hour on one core of the test world, the buyer held off
		// for as long as each walk lasted, and 576 of them a crossing from the
		// second village to Joan and back, one in ten with a purchase on the
		// way (23 September). What is left for this pass is the walk the buyer
		// cannot make.
		if (!haveStallInReach)
			return StartPlayerBotFarMarketWalk(ch, state, dwNow, pitchX, pitchY);
#endif

		// Something in reach, or a reason to go and look: either way it is a trip,
		// so the bot walks up to the counter instead of buying from twenty metres
		// off. That is the difference between a market and a vending machine.
		if (!haveStallInReach && !PlayerBotWantsAnythingFromMarket(ch))
			return false;
		// A market is counters, and the ledger counts them once a minute. With
		// none in reach and none on this map there is nothing to walk to, and
		// with none in the first village either there is nothing to cross for:
		// on a young world no bot is old enough to open one, and the trip was
		// a walk to an empty pitch under "Szukam czegos na straganach".
		const int owner = playerbot_empire_rules::GetMapOwnerEmpire(ch->GetMapIndex());
		const long firstVillage = playerbot_empire_rules::GetHomeMap(owner,
				playerbot_empire_rules::MAP_ROLE_M1);
		const bool stallsHere = GetPlayerBotStallsOnMap(ch->GetMapIndex()) > 0;
		const bool stallsInJoan = GetPlayerBotStallsOnMap(firstVillage) > 0;
		if (!haveStallInReach && !stallsHere && !stallsInJoan)
			return false;
		// Joan first. A shopper standing in Bokjung crosses to the quieter market
		// before browsing the one under its nose: that is what gives the Joan
		// counters customers, and it is also what stops five hundred bots
		// circling the same seven stalls. Bokjung opens up again for a while
		// once Joan has been looked at and had nothing.
		// Only for a bot whose place is Bokjung: one that is leaving for the
		// frontier, or is held back from it by an errand, shops in reach and
		// goes - the same line the stall's walk to Joan draws.
		// Not a bot in a player's party either: the walk to the Joan gate is a
		// map change the follow pass undoes a second later. Nor a bot on its way
		// to the Monkey Dungeon, whose gate stands in this village: the walk to
		// Joan took it back out through the gate it had just come in by.
		if (!haveStallInReach && stallsInJoan && IsPlayerBotM2Map(ch->GetMapIndex()) &&
				dwNow >= state.dwMarketM2AllowedUntil &&
				state.lDepartureMap == 0 && GetPlayerBotFrontierMapForLevel(ch) == 0 &&
				state.bLongTermGoal != BOT_GOAL_HORSE &&
				!(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) &&
				!IsPlayerBotHeldForCompany(ch))
		{
			state.bMarketTrip = true;
			state.bMarketToJoan = true;
			state.dwMarketTripUntil = dwNow + PLAYERBOT_MARKET_JOAN_WALK_TIMEOUT;
			state.dwMarketBrowseTime = 0;
			state.dwMarketStallVID = 0;
			sys_log(0, "PLAYERBOT_MARKET: looking in Joan first pid=%u name=%s pos=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY());
			return ContinuePlayerBotMarketTrip(ch, state, dwNow, pitchX, pitchY);
		}
		if (!haveStallInReach && !stallsHere)
			return false; // the only counters are in Joan, and Joan was looked at
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

	// Once a minute, the ledger playerbot_world_memory.h keeps: every open
	// counter's lines, and every bot short of a material with the money and
	// the bag room to go and buy it. Walked here rather than kept up to date
	// by the sites that change it, because those sites are a purchase, a
	// stall closing, a refine consuming a material, a drop landing in a bag
	// and a bot outgrowing a piece - and one walk a minute is cheaper than
	// getting all five right. Eight hundred bags once a minute is what one
	// target scan costs, and a target scan happens hundreds of times a minute.
	//
	// Every ten minutes it is written down: the counters, the shortages, and
	// what the listing decisions said in between. Read the top of that list
	// against the drops: a material with thirty bots short and nothing on any
	// counter is not being held back by the ledger, it is not being found.
	// Declared in playerbot_economy.h for the junk rule.
	DWORD GetPlayerBotLedgerDemand(DWORD vnum)
	{
		TPlayerBotMarketLedger::const_iterator it = s_mapMarketLedger.find(vnum);
		return it == s_mapMarketLedger.end() ? 0 : it->second.dwDemandBots;
	}

	// Community patch 2, point 1, measured with the ledger's report: of the
	// bots of thirty and more that are not droppers, how many hold the class's
	// level-30 weapon, at which plus, and how many do not have one at all.
	void ReportPlayerBotLevel30Census()
	{
		unsigned int eligible = 0, lacking = 0, low = 0, mid = 0;
		unsigned int auPlus[10] = { 0 };
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsItemLoaded() || ch->GetLevel() < 30 ||
					IsPlayerBotDropper(GetPlayerBotPersonalityByPID(it->first)))
				continue;
			++eligible;
			LPITEM weapon = FindPlayerBotClassLevel30Weapon(ch);
			if (!weapon)
			{
				++lacking;
				continue;
			}
			const int plus = std::min(9, std::max(0, (int)weapon->GetRefineLevel()));
			++auPlus[plus];
			if (plus <= 3)
				++low;
			else if (plus <= 5)
				++mid;
		}
		sys_log(0, "PLAYERBOT_MARKET: level-30 census eligible=%u lacking=%u plus0_3=%u plus4_5=%u plus6=%u plus7=%u plus8=%u plus9=%u",
				eligible, lacking, low, mid, auPlus[6], auPlus[7], auPlus[8], auPlus[9]);
	}

	void RefreshPlayerBotMarketLedger(DWORD dwNow)
	{
		if (s_dwMarketLedgerTime != 0 &&
				dwNow - s_dwMarketLedgerTime < PLAYERBOT_MARKET_LEDGER_INTERVAL)
			return;
		s_dwMarketLedgerTime = dwNow;
		s_mapMarketLedger.clear();
		s_mapMarketLocalSupply.clear();
		s_iPlayerBotJunkWeaponsOnCounters = 0;
		RefreshPlayerBotWorldYang(dwNow);

		DWORD stalls = 0, lines = 0, demandBots = 0;
		DWORD auStallsByReason[PLAYERBOT_SHOP_REASON_MAX] = { 0 };
		s_iPlayerBotStallsInM2 = 0;
		s_mapPlayerBotStallsByMap.clear();
		std::set<DWORD> wanted;
		std::vector<DWORD> wallets;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsItemLoaded())
				continue;
			const TPlayerBotAIState& state = it->second;
			if (ch->GetMyShop() && !state.vecShopOffers.empty())
			{
				++stalls;
				++auStallsByReason[state.bShopOpenReason < PLAYERBOT_SHOP_REASON_MAX
						? state.bShopOpenReason : PLAYERBOT_SHOP_REASON_NONE];
				if (IsPlayerBotM2Map(ch->GetMapIndex()))
					++s_iPlayerBotStallsInM2;
				++s_mapPlayerBotStallsByMap[ch->GetMapIndex()];
				for (size_t k = 0; k < state.vecShopOffers.size(); ++k)
				{
					const TPlayerBotShopOffer& offer = state.vecShopOffers[k];
					// A line that has been bought stays in the offer list; the
					// item does not stay in the bag.
					if (!FindPlayerBotOfferItem(ch, offer))
						continue;
					AddPlayerBotMarketSupply(offer.dwVnum, offer.wCount, ch->GetMapIndex());
					NotePlayerBotJunkWeaponOnCounter(offer.dwVnum, offer.wCount);
					++lines;
				}
			}
			// A keeper counts as a buyer too: its counter closes within the
			// half hour and its own anvil is still waiting.
			if (!CanPlayerBotAffordMarket(ch))
				continue;
			wallets.push_back((DWORD)std::max<long long>(0,
					(long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch)));
			CollectPlayerBotWantedMaterials(ch, wanted);
			if (wanted.empty())
				continue;
			++demandBots;
			for (std::set<DWORD>::const_iterator w = wanted.begin(); w != wanted.end(); ++w)
				++s_mapMarketLedger[*w].dwDemandBots;
		}

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		AddPlayerBotOfflineLedger(stalls, lines);
#endif
		if (!wallets.empty())
		{
			std::sort(wallets.begin(), wallets.end());
			s_dwMarketMedianWallet = wallets[wallets.size() / 2];
		}

		if (s_dwMarketReportTime != 0 &&
				dwNow - s_dwMarketReportTime < PLAYERBOT_MARKET_REPORT_INTERVAL)
			return;
		s_dwMarketReportTime = dwNow;

		std::vector<std::pair<DWORD, DWORD> > ranked; // demand, vnum
		for (TPlayerBotMarketLedger::const_iterator e = s_mapMarketLedger.begin();
				e != s_mapMarketLedger.end(); ++e)
			ranked.push_back(std::make_pair(e->second.dwDemandBots, e->first));
		std::sort(ranked.rbegin(), ranked.rend());
		std::string top;
		for (size_t i = 0; i < ranked.size() && i < 8; ++i)
		{
			const TPlayerBotMarketLedgerEntry& entry = s_mapMarketLedger[ranked[i].second];
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(ranked[i].second);
			char buf[128];
			snprintf(buf, sizeof(buf), " %s(%u) D=%u S=%u/%u ask=%u",
					proto ? proto->szLocaleName : "?", ranked[i].second,
					entry.dwDemandBots, entry.dwSupplyUnits, entry.dwSupplyStalls,
					GetPlayerBotLastAsk(ranked[i].second, 0, dwNow));
			top += buf;
		}
		// Who is trading and why, against the TRADE weight in force: the number
		// an operator needs before deciding the slider "does nothing".
		sys_log(0, "PLAYERBOT_SHOP: census stalls=%u trade_weight=%d merchant=%u poor=%u bag_full=%u dropper_pressure=%u books=%u dropper_roll=%u roll=%u spare=%u hoard=%u",
				stalls, GetPlayerBotWeight(PLAYERBOT_WEIGHT_TRADE),
				auStallsByReason[PLAYERBOT_SHOP_REASON_MERCHANT], auStallsByReason[PLAYERBOT_SHOP_REASON_POOR],
				auStallsByReason[PLAYERBOT_SHOP_REASON_BAG_FULL], auStallsByReason[PLAYERBOT_SHOP_REASON_DROPPER_PRESSURE],
				auStallsByReason[PLAYERBOT_SHOP_REASON_BOOKS], auStallsByReason[PLAYERBOT_SHOP_REASON_DROPPER_ROLL],
				auStallsByReason[PLAYERBOT_SHOP_REASON_ROLL], auStallsByReason[PLAYERBOT_SHOP_REASON_SPARE],
				auStallsByReason[PLAYERBOT_SHOP_REASON_HOARD]);
		ReportPlayerBotWeaponGoals(dwNow);
		ReportPlayerBotLevel30Census();
		sys_log(0, "PLAYERBOT_MARKET: ledger stalls=%u lines=%u vnums=%u demand_bots=%u wallet=%u junk_weapons=%d/%d decisions list=%u probe=%u no_demand=%u overstock=%u floor=%u top:%s",
				stalls, lines, (unsigned int)s_mapMarketLedger.size(), demandBots,
				s_dwMarketMedianWallet, s_iPlayerBotJunkWeaponsOnCounters, PLAYERBOT_JUNK_WEAPON_MARKET_CAP,
				s_auMarketDecisions[PLAYERBOT_LIST_LIST], s_auMarketDecisions[PLAYERBOT_LIST_PROBE],
				s_auMarketDecisions[PLAYERBOT_LIST_NO_DEMAND], s_auMarketDecisions[PLAYERBOT_LIST_OVERSTOCK],
				s_auMarketDecisions[PLAYERBOT_LIST_FLOOR], top.c_str());
		for (int d = 0; d < PLAYERBOT_LIST_DECISIONS; ++d)
			s_auMarketDecisions[d] = 0;
	}
}

#endif
