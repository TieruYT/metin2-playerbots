#ifndef PLAYERBOT_OFFLINE_MARKET_H
#define PLAYERBOT_OFFLINE_MARKET_H
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
// Included after playerbot_market.h so the existing demand/gear rules are used.
namespace {
    bool ManagePlayerBotOfflineShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        auto& manager = ikashop::GetManager();
        if (BotOfflineBusy(ch, state) || requests.count(ch->GetPlayerID()) || o.visiting || state.bMarketTrip) {
            o.buyOwner = 0;
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
                if (!line) return;
                const auto price = line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > budget) return;
                auto preview = BotOfflinePreview(*line);
                if (!preview) return;
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
                if (!want || priority < bestPriority ||
                        (priority == bestPriority && price >= bestPrice)) return;
                bestPriority = priority;
                bestPrice = price;
                o.buyOwner = shop->GetOwnerPID();
                o.buyItem = id;
                o.buyUntil = now + 45000;
            });
        }
        if (!o.buyOwner) return false;
        if (g_bChannel != playerbot_channel_rules::SHOP_CHANNEL) {
            // Something worth buying, on the shop channel: ask to be moved, at
            // most this often, and forget the pick - the purchase is made there,
            // by the browse after the move.
            o.buyOwner = 0;
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
        if (!shop || shop->GetDuration() == 0 || shop->IsEditMode() || Due(now, o.buyUntil) ||
                shop->GetSpawn().map != ch->GetMapIndex() || shop->GetSpawn().channel != g_bChannel) {
            o.buyOwner = 0;
            ClearPlayerBotRoute(state, true);
            return false;
        }
        auto line = shop->GetItem(o.buyItem);
        if (!line) { o.buyOwner = 0; return false; }
        SetPlayerBotAction(state, BOT_ACTION_TRAVEL, now);
        if (!MovePlayerBotTownLeg(ch, state, now, shop->GetSpawn().x, shop->GetSpawn().y, 600)) return true;
        auto price = line->GetPrice().GetTotalYangAmount();
        auto finalPreview = BotOfflinePreview(*line);
        const bool stillWanted = finalPreview && WantsPlayerBotStallItem(ch, finalPreview) &&
            CanPlayerBotPayForOffer(ch, finalPreview, price) && ch->GetEmptyInventory(finalPreview->GetSize()) >= 0;
        if (finalPreview) M2_DELETE(finalPreview);
        if (!stillWanted) {
            o.buyOwner = 0;
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
        o.buyOwner = 0;
        ClearPlayerBotRoute(state, true);
        return false; // DB completion owns delivery; never synthesize money/items
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
