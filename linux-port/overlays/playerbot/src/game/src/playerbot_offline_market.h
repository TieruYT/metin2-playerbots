#ifndef PLAYERBOT_OFFLINE_MARKET_H
#define PLAYERBOT_OFFLINE_MARKET_H
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
// Included after playerbot_market.h so the existing demand/gear rules are used.
namespace {
    // The line a walk over from a second village is for. Every bot of a
    // village read the same first village's lines and set off for the best
    // of them, and of the first 155 walks on the test world 31 found their
    // line sold on arrival - 29 of them to another bot that had walked over
    // for the same line (23 September). A walk claims its line for as long
    // as the walk and the ride to the stand may take, and every other look,
    // the browse of the stands in reach included, passes over it.
    struct TPlayerBotFarClaim { DWORD pid; DWORD until; };
    std::unordered_map<DWORD, TPlayerBotFarClaim> s_mapPlayerBotFarClaims;

    bool IsPlayerBotLineClaimedByOther(DWORD itemId, DWORD pid, DWORD now) {
        auto it = s_mapPlayerBotFarClaims.find(itemId);
        if (it == s_mapPlayerBotFarClaims.end()) return false;
        if (playerbot_offline::Due(now, it->second.until)) {
            s_mapPlayerBotFarClaims.erase(it);
            return false;
        }
        return it->second.pid != pid;
    }
    void ClaimPlayerBotFarLine(DWORD itemId, DWORD pid, DWORD now) {
        if (!itemId) return;
        // A claim whose walk ended some other way than at the stand is left
        // to run out, and the ones that did are swept once the map is big.
        if (s_mapPlayerBotFarClaims.size() >= 4096)
            for (auto it = s_mapPlayerBotFarClaims.begin(); it != s_mapPlayerBotFarClaims.end(); )
                it = playerbot_offline::Due(now, it->second.until) ? s_mapPlayerBotFarClaims.erase(it) : std::next(it);
        s_mapPlayerBotFarClaims[itemId] = TPlayerBotFarClaim{ pid,
            now + PLAYERBOT_MARKET_JOAN_WALK_TIMEOUT + PLAYERBOT_MARKET_FAR_PICK_WALK_MS };
    }
    void ReleasePlayerBotFarLine(DWORD itemId, DWORD pid) {
        auto it = s_mapPlayerBotFarClaims.find(itemId);
        if (it != s_mapPlayerBotFarClaims.end() && it->second.pid == pid)
            s_mapPlayerBotFarClaims.erase(it);
    }

    // The buyer gives its pick up. A pick a walk over was made for says why,
    // once: the lines left after the claim are the ones whose reason is not
    // yet known, and "every way this function can decline looks identical
    // from outside" is how a walk over came to end nine times in ten with
    // nothing bought.
    void DropPlayerBotOfflinePick(LPCHARACTER ch, TPlayerBotAIState& state, const char* reason) {
        auto& o = state.offlineShop;
        if (o.buyOwner) {
            ReleasePlayerBotFarLine(o.buyItem, ch->GetPlayerID());
            if (o.farBuy)
                sys_log(0, "PLAYERBOT_MARKET: far pick lost pid=%u name=%s owner=%u item=%u reason=%s",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, reason);
        }
        o.farBuy = false;
        o.buyOwner = 0;
    }

    // One line of one stand as this bot would rate it: -1 for a line it would
    // not buy, else the priority a browse ranks by. The browse of the stands
    // in reach and the look at the first village's from a second village
    // (FindPlayerBotFarOfflinePick) ask this one function, so a walk over is
    // made for exactly what the buyer would take on arrival.
    template<class Shop, class Line>
    int RatePlayerBotOfflineLine(LPCHARACTER ch, const Shop& shop, const Line& line,
            long long budget, bool personFirst, long long& outPrice) {
        if (!line) return -1;
        const long long price = (long long)line->GetPrice().GetTotalYangAmount();
        if (price <= 0 || price > budget) return -1;
        auto preview = BotOfflinePreview(*line);
        if (!preview) return -1;
        const bool want = WantsPlayerBotStallItem(ch, preview) &&
            CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
        int priority = IsPlayerBotProgressionOffer(ch, preview) ? 200 : 0;
        // The class's level-30 weapon comes first, and of those the
        // highest average line, the price only breaking a tie: "12% za
        // 300k albo 26% za 450k - wybierze drozsza" (community patch 2).
        // A finished piece the market Perfectionist's anvil waits for.
        if (want && priority < 300 && IsPlayerBotReadyGearOffer(ch, preview))
            priority = 300;
        if (want && IsPlayerBotClassLevel30Weapon(ch, preview))
            priority = 400 + (int)std::min<long>(99, std::max<long>(0,
                SumPlayerBotItemLines(preview, APPLY_NORMAL_HIT_DAMAGE_BONUS)));
        if (want && personFirst &&
                !CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID())) {
            const long long fair = GetPlayerBotShopAskingPrice(preview);
            if (fair > 0 && price <= fair * PLAYERBOT_MARKET_PERSON_PRICE_PERCENT / 100)
                priority += 100;
        }
        M2_DELETE(preview);
        outPrice = price;
        return want ? priority : -1;
    }

    bool ManagePlayerBotOfflineShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        auto& manager = ikashop::GetManager();
        const bool busy = BotOfflineBusy(ch, state);
        if (busy || requests.count(ch->GetPlayerID()) || o.visiting || state.bMarketTrip) {
            if (o.buyOwner)
                DropPlayerBotOfflinePick(ch, state, busy ? "busy" : o.visiting ? "service_visit" :
                    requests.count(ch->GetPlayerID()) ? "request_in_flight" : "market_trip");
            return false;
        }
        long pitchX = 0, pitchY = 0;
        if (!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY)) return false;
        if (!o.buyOwner) {
            if (!Due(now, o.nextBrowse)) return false;
            o.nextBrowse = now + number(120000, 240000);
            const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
            if (budget <= 0) return false;
            std::vector<std::pair<int, NativeShop> > shops;
            // Every stand is on the shop channel. With the assignment table a
            // bot elsewhere still reads the stands of its own map and asks to
            // be moved there when one holds something worth buying.
            const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                    ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
            for (const auto& [pid, shop] : manager.GetPlayerBotOfflineShops()) {
                if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
                const auto spawn = shop->GetSpawn();
                if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
                const int distance = DISTANCE_APPROX(spawn.x-ch->GetX(), spawn.y-ch->GetY());
                if (distance <= PLAYERBOT_MARKET_TRIP_RANGE) shops.emplace_back(distance, shop);
            }
            std::sort(shops.begin(), shops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            // Resume within the shop too: a full counter must not hide item 65.
            int bestPriority = -1;
            long long bestPrice = 0;
            const bool personFirst = number(1, 100) <= PLAYERBOT_MARKET_PERSON_FIRST_PERCENT;
            BrowseLines(shops, o, 64, [&](auto shop, auto id, const auto& line) {
                if (IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now)) return;
                long long price = 0;
                const int priority = RatePlayerBotOfflineLine(ch, shop, line, budget, personFirst, price);
                if (priority < 0 || priority < bestPriority ||
                        (priority == bestPriority && price >= bestPrice)) return;
                bestPriority = priority;
                bestPrice = price;
                o.buyOwner = shop->GetOwnerPID();
                o.buyItem = id;
                o.buyUntil = now + 45000;
            });
            // A first village's stands with nothing on them this bot came for:
            // it asks on the trade channel, as a trip that found the market
            // empty always did. The classic trip was that trip's only caller,
            // and on this engine it no longer sets off (ManagePlayerBotShopping).
            // The shout is throttled world-wide and says nothing for a bag
            // that wants no material.
            if (!o.buyOwner && IsPlayerBotM1Map(ch->GetMapIndex()))
                AnnouncePlayerBotNeed(ch);
        }
        if (!o.buyOwner) return false;
        if (g_bChannel != playerbot_channel_rules::SHOP_CHANNEL) {
            // Something worth buying, on the shop channel: ask to be moved, at
            // most this often, and forget the pick - the purchase is made there,
            // by the browse after the move.
            DropPlayerBotOfflinePick(ch, state, "other_channel");
            if (Due(now, state.dwNextBuyChannelRequestTime)) {
                state.dwNextBuyChannelRequestTime = now + PLAYERBOT_SHOP_CHANNEL_BUY_REQUEST_GAP_MS;
                if (CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID()))
                    PlayerBotLogThrottled("shop_channel_buy", now,
                            "PLAYERBOT_CHANNEL: pid=%u name=%s asks for the shop channel to buy (here %u)",
                            ch->GetPlayerID(), ch->GetName(), (unsigned int)g_bChannel);
            }
            return false;
        }
        auto shop = manager.GetShopByOwnerID(o.buyOwner);
        const char* lost = !shop ? "shop_gone" : shop->GetDuration() == 0 ? "shop_expired" :
            Due(now, o.buyUntil) ? "timeout" :
            (shop->GetSpawn().map != ch->GetMapIndex() || shop->GetSpawn().channel != g_bChannel) ? "elsewhere" : NULL;
        if (lost) {
            DropPlayerBotOfflinePick(ch, state, lost);
            ClearPlayerBotRoute(state, true);
            return false;
        }
        auto line = shop->GetItem(o.buyItem);
        if (!line) { DropPlayerBotOfflinePick(ch, state, "sold"); return false; }
        SetPlayerBotAction(state, BOT_ACTION_TRAVEL, now);
        if (!MovePlayerBotTownLeg(ch, state, now, shop->GetSpawn().x, shop->GetSpawn().y, 600)) return true;
        // The keeper is serving its stand, which is a few seconds of edit
        // mode: the buyer waits them out at the counter instead of giving up
        // the line it came for - which it did, whatever the walk had cost.
        if (shop->IsEditMode()) return true;
        auto price = line->GetPrice().GetTotalYangAmount();
        auto finalPreview = BotOfflinePreview(*line);
        const bool wanted = finalPreview && WantsPlayerBotStallItem(ch, finalPreview);
        const bool payable = wanted && CanPlayerBotPayForOffer(ch, finalPreview, price);
        const bool stillWanted = payable && ch->GetEmptyInventory(finalPreview->GetSize()) >= 0;
        if (finalPreview) M2_DELETE(finalPreview);
        if (!stillWanted) {
            DropPlayerBotOfflinePick(ch, state, !wanted ? "no_longer_wanted" : !payable ? "cannot_pay" : "no_room");
            ClearPlayerBotRoute(state, true);
            return false;
        }
        if (!BotOfflineBudget(now)) return true;
        // Read before the request: the log line below must not touch the shop
        // line once the purchase is in the engine's hands.
        const DWORD boughtVnum = line->GetInfo().vnum;
        if (boughtVnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
            NotePlayerBotChestBought(ch->GetPlayerID(), now);
        if (Begin(ch->GetPlayerID(), Buy, o.buyItem, now)) {
            auto& request = requests.at(ch->GetPlayerID());
            request.vnum = line->GetInfo().vnum;
            request.count = line->GetInfo().count;
            request.unitPrice = uint32_t(price / std::max<uint32_t>(1, request.count));
            if (auto preview = BotOfflinePreview(*line)) {
                request.refine = preview->GetRefineLevel();
                request.skill = preview->GetType() == ITEM_SKILLBOOK ? GetPlayerBotSkillBookSkillVnum(preview) : 0;
                // Out of the book purse as it is asked for: a refused purchase
                // costs a window's share, which the next window gives back.
                if (preview->GetType() == ITEM_SKILLBOOK)
                    NotePlayerBotBookBought(ch, price);
                M2_DELETE(preview);
            }
            manager.RecvShopOpenClientPacket(ch, o.buyOwner);
            manager.RecvShopBuyItemClientPacket(ch, o.buyOwner, o.buyItem, false, price);
            const bool sent = EndCall(ch->GetPlayerID());
            manager.RecvCloseShopGuestClientPacket(ch);
            sys_log(0, "PLAYERBOT_OFFLINE: purchase_requested buyer=%u owner=%u item=%u vnum=%u price=%lld sent=%d",
                ch->GetPlayerID(), o.buyOwner, o.buyItem, (unsigned int)boughtVnum, (long long)price, sent);
        }
        ReleasePlayerBotFarLine(o.buyItem, ch->GetPlayerID());
        o.farBuy = false;
        o.buyOwner = 0;
        ClearPlayerBotRoute(state, true);
        return false; // DB completion owns delivery; never synthesize money/items
    }
    // From a second village the buyer above cannot see its first village's
    // stands: they stand on another map. StartPlayerBotFarMarketWalk asks this
    // before the walk over - PLAYERBOT_MARKET_FAR_LOOK_LINES lines on a cursor
    // of their own, the next look going on where this one stopped - and the
    // line found is kept for the buyer, who takes it on arrival
    // (HandPlayerBotFarPickToBuyer). The walk used to be made for whatever
    // the bot might want, and nine times in ten it bought nothing.
    bool FindPlayerBotFarOfflinePick(LPCHARACTER ch, TPlayerBotAIState& state, long mapIndex) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        o.farPickOwner = o.farPickItem = 0;
        const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
        if (budget <= 0) return false;
        // In the order of their distance from the market's middle, which does
        // not move, so the cursor means the same thing at the next look.
        long centreX = 0, centreY = 0;
        GetPlayerBotShopCentre(mapIndex, centreX, centreY);
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        std::vector<std::pair<int, NativeShop> > shops;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != mapIndex || (int)spawn.channel != shopChannel) continue;
            shops.emplace_back(DISTANCE_APPROX(spawn.x - centreX, spawn.y - centreY), shop);
        }
        if (shops.empty()) return false;
        std::sort(shops.begin(), shops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        // A first look starts at a stand drawn by pid. From the middle, every
        // bot of a village read the same sixty-four lines and set off for the
        // same one - three walks in the first minute on the test world for
        // owner 1863's line, which one of them could buy.
        if (!o.farBrowseOwner) {
            o.farBrowseOwner = shops[ch->GetPlayerID() % shops.size()].second->GetOwnerPID();
            o.farBrowseItem = 0;
        }
        int bestPriority = -1;
        long long bestPrice = 0;
        const bool personFirst = number(1, 100) <= PLAYERBOT_MARKET_PERSON_FIRST_PERCENT;
        const DWORD now = get_dword_time();
        std::swap(o.browseOwner, o.farBrowseOwner);
        std::swap(o.browseItem, o.farBrowseItem);
        BrowseLines(shops, o, PLAYERBOT_MARKET_FAR_LOOK_LINES, [&](auto shop, auto id, const auto& line) {
            if (IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now)) return;
            long long price = 0;
            const int priority = RatePlayerBotOfflineLine(ch, shop, line, budget, personFirst, price);
            if (priority < 0 || priority < bestPriority ||
                    (priority == bestPriority && price >= bestPrice)) return;
            bestPriority = priority;
            bestPrice = price;
            o.farPickOwner = shop->GetOwnerPID();
            o.farPickItem = id;
        });
        std::swap(o.browseOwner, o.farBrowseOwner);
        std::swap(o.browseItem, o.farBrowseItem);
        return o.farPickOwner != 0;
    }

    // Across: the line the walk was made for is the buyer's pick now, with
    // the time to ride from the gate to the stand, and the stands in reach
    // are read at once should it have sold meanwhile. Called on the tick of
    // arrival, and it claims that tick while the bot walks to the stand, or
    // the travel pass further down the tick would take it straight back.
    bool HandPlayerBotFarPickToBuyer(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        auto& o = state.offlineShop;
        if (o.farPickOwner) {
            o.buyOwner = o.farPickOwner;
            o.buyItem = o.farPickItem;
            o.buyUntil = now + PLAYERBOT_MARKET_FAR_PICK_WALK_MS;
            o.farBuy = true;
        }
        o.farPickOwner = o.farPickItem = 0;
        o.nextBrowse = 0;
        return ManagePlayerBotOfflineShopping(ch, state, now);
    }

    void AddPlayerBotOfflineLedger(DWORD& stalls, DWORD& lines) {
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || shop->GetDuration() == 0 || shop->GetSpawn().channel != g_bChannel) continue;
            ++stalls;
            ++s_mapPlayerBotStallsByMap[shop->GetSpawn().map];
            if (IsPlayerBotM2Map(shop->GetSpawn().map)) ++s_iPlayerBotStallsInM2;
            const bool botShop = CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID());
            for (const auto& [id, item] : shop->GetItems()) {
                if (!item) continue;
                AddPlayerBotMarketSupply(item->GetVnum(), item->GetInfo().count, shop->GetSpawn().map);
                if (botShop) NotePlayerBotJunkWeaponOnCounter(item->GetVnum(), item->GetInfo().count);
                ++lines;
            }
        }
    }
}
#endif
#endif
