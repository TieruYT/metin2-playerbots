#ifndef PLAYERBOT_OFFLINE_SHOP_H
#define PLAYERBOT_OFFLINE_SHOP_H
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
// Included once after playerbot_town.h. All transfers use native Ikarus.
namespace {
    using NativeShop = ikashop::CShopManager::SHOP_HANDLE;
    // The core's offline-shop mutations, as a bucket: one token every
    // PLAYERBOT_OFFLINE_MUTATION_MS, at most PLAYERBOT_OFFLINE_MUTATION_BURST
    // in hand. It was one a second with nothing saved, so two visits asking in
    // the same second had one refused while the seconds before had gone
    // unused - 35 to 47 granted a minute against 25 to 213 refused on m2zip
    // on 18 September, with a thousand keepers waiting to restock. Each token
    // is one small ikashop request to the db core, and the db core's queue is
    // the thing to read if this is ever raised again.
    const DWORD PLAYERBOT_OFFLINE_MUTATION_MS = 500;
    const unsigned PLAYERBOT_OFFLINE_MUTATION_BURST = 5;
    unsigned s_botOfflineTokens = PLAYERBOT_OFFLINE_MUTATION_BURST;
    DWORD s_botOfflineTokenTime = 0;
    // What the budget handed out and turned away in the last minute. Granted
    // near its ceiling (120) means every keeper's visit is queueing behind the
    // others; measure it before moving the budget or the slice.
    DWORD s_botOfflineBudgetMinute = 0;
    unsigned s_botOfflineBudgetGranted = 0, s_botOfflineBudgetRefused = 0;

    bool BotOfflineBudget(DWORD now) {
        if (s_botOfflineBudgetMinute == 0) s_botOfflineBudgetMinute = now;
        if ((int)(now - s_botOfflineBudgetMinute) >= 60000) {
            sys_log(0, "PLAYERBOT_OFFLINE: budget last_minute granted=%u refused=%u",
                s_botOfflineBudgetGranted, s_botOfflineBudgetRefused);
            s_botOfflineBudgetMinute = now;
            s_botOfflineBudgetGranted = s_botOfflineBudgetRefused = 0;
        }
        if (s_botOfflineTokenTime == 0) s_botOfflineTokenTime = now;
        const DWORD earned = (now - s_botOfflineTokenTime) / PLAYERBOT_OFFLINE_MUTATION_MS;
        if (earned > 0) {
            s_botOfflineTokens = std::min<unsigned>(PLAYERBOT_OFFLINE_MUTATION_BURST, s_botOfflineTokens + earned);
            s_botOfflineTokenTime += earned * PLAYERBOT_OFFLINE_MUTATION_MS;
        }
        if (s_botOfflineTokens == 0) {
            ++s_botOfflineBudgetRefused;
            return false;
        }
        --s_botOfflineTokens;
        ++s_botOfflineBudgetGranted;
        return true;
    }
    bool BotOfflineBusy(LPCHARACTER ch, const TPlayerBotAIState& state) {
        return !ch || !ch->IsItemLoaded() || ch->IsDead() || ch->IsStun() ||
            ch->GetExchange() || ch->GetShop() || ch->GetSafebox() || ch->IsBusy() ||
            (ch->GetVictim() && !ch->GetVictim()->IsDead()) ||
            state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
            state.bRecoveringAfterDeath || state.bTacticalRetreat || state.bMultiPullActive ||
            state.bFishingSession ||
            // Nor off the desert in the middle of its battle-horse trial: the
            // service walk every ten to fifteen minutes was 23 of 80 desert
            // departures an hour, and the trial is a hundred kills on that map.
            (IsPlayerBotOnBattleHorseTrial(ch) && ch->GetMapIndex() == PLAYERBOT_MAP_DESERT) ||
            // A bot in a player's party does not warp off to its counter every
            // ten minutes; the stand keeps selling until the party ends.
            (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
            // Nor off a mercenary's contract, or away from a person it leads
            // (playerbot_companions.h).
            IsPlayerBotHeldForCompany(ch) ||
            // Nor out of the Demon Tower, nor off a raid on its way there.
            IsPlayerBotOnTowerBusiness(ch, state) ||
            // Nor out of a Monkey Dungeon: a visit is half an hour in rooms
            // joined only by their doors, and a keeper warped out of it has the
            // whole way back in to walk. The service waits for the way out.
            IsPlayerBotMonkeyMap(ch->GetMapIndex());
    }
    // The wait before the next service visit: the long one for a dropper
    // (PLAYERBOT_DROPPER_SHOP_SERVICE_MIN_MS), ten to fifteen minutes for
    // everybody else.
    DWORD BotOfflineServiceGap(const TPlayerBotAIState& state) {
        if (IsPlayerBotDropper(state.bPersonality))
            return (DWORD)number((int)PLAYERBOT_DROPPER_SHOP_SERVICE_MIN_MS, (int)PLAYERBOT_DROPPER_SHOP_SERVICE_MAX_MS);
        return (DWORD)number(600000, 900000);
    }
    void BotOfflineFinishVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        auto& o = state.offlineShop;
        if (o.visiting) {
            ikashop::GetManager().RecvCloseMyShopBoardClientPacket(ch);
            ikashop::GetManager().RecvShopSafeboxCloseClientPacket(ch);
            ClearPlayerBotRoute(state, true);
        }
        o.visiting = false;
        o.visitUntil = 0;
        o.nextService = now + BotOfflineServiceGap(state);
    }
    bool BotOfflinePoll(LPCHARACTER ch, DWORD now) {
        auto it = playerbot_offline::requests.find(ch->GetPlayerID());
        if (it == playerbot_offline::requests.end()) return false;
        auto& r = it->second;
        // A create whose answer never came, read against the shop the core
        // holds. Standing again (duration above zero) is the answer arriving by
        // another road - the ack was lost, not the shop. Still expired a while
        // later is a renewal the db core turned down: it carries no goods, so
        // nothing can be doubled by asking again, and holding the request for
        // good (as every unresolved request was held) left the keeper with an
        // expired stand full of goods until the next restart - 368 such
        // renewals on m2zip from 17 to 20 September. The db core refuses a
        // create while its own copy of the shop has time left, and that copy
        // ran behind this core's until 2.0.95 (see apply_shop_clock in
        // playerbotify.py). A fresh create is left alone: its goods are
        // already in the shop's window in the database.
        if (!r.done && r.sent && r.op == playerbot_offline::Create) {
            auto shop = ikashop::GetManager().GetShopByOwnerID(ch->GetPlayerID());
            if (shop && shop->GetDuration() > 0) {
                r.done = true;
                r.success = true;
            } else if (shop && uint32_t(now - r.started) >= PLAYERBOT_OFFLINE_RENEW_ABANDON_MS) {
                sys_err("PLAYERBOT_OFFLINE: renewal went unanswered pid=%u name=%s age_s=%u; dropped, the next service visit asks again",
                    ch->GetPlayerID(), ch->GetName(), unsigned((now - r.started) / 1000U));
                playerbot_offline::requests.erase(it);
                return false;
            }
        }
        if (r.done) {
            if (r.success && r.op == playerbot_offline::Buy && r.count && r.unitPrice)
                RememberPlayerBotSale(r.vnum, r.refine, r.unitPrice, now, r.skill);
            sys_log(0, "PLAYERBOT_OFFLINE: ack pid=%u op=%u item=%u ok=%d",
                ch->GetPlayerID(), unsigned(r.op), r.item, r.success);
            playerbot_offline::requests.erase(it);
            return false;
        }
        if (!r.warned && uint32_t(now - r.started) >= 30000) {
            r.warned = true;
            sys_err("PLAYERBOT_OFFLINE: unresolved pid=%u op=%u item=%u; commerce paused, gameplay continues; reconcile DB before retry",
                ch->GetPlayerID(), unsigned(r.op), r.item);
        }
        return true;
    }
    // Unregistered comparison object: no persistent ID allocation, save queue,
    // expiry events or item-manager insertion. Destroy with M2_DELETE below.
    LPITEM BotOfflinePreview(const ikashop::CShopItem& source) {
        const auto& i = source.GetInfo();
        auto proto = source.GetTable();
        if (!proto) return NULL;
        auto item = M2_NEW CItem(i.vnum);
        item->Initialize();
        item->SetProto(proto);
        item->SetSkipSave(true);
        item->SetCount(i.count);
        item->SetAttributes(i.aAttr);
        item->SetSockets(i.alSockets);
#ifdef ENABLE_CHANGELOOK_SYSTEM
        item->SetTransmutation(i.dwTransmutation);
#endif
        return item;
    }
    bool BotOfflineValid(LPCHARACTER ch, LPITEM item, int cell) {
        if (!item || item->GetOwner() != ch || item->IsEquipped() || item->isLocked()) return false;
#ifdef ENABLE_SOULBIND_SYSTEM
        if (item->IsSealed()) return false;
#endif
        return playerbot_offline::Fits(cell, item->GetSize(), SHOP_PLAYER_WIDTH,
                SHOP_PLAYER_HOST_ITEM_MAX_NUM) && ch->CanAddItemToShop(item, BYTE(cell));
    }
    int BotOfflineSlot(LPCHARACTER ch, NativeShop shop, LPITEM item) {
        bool used[SHOP_PLAYER_HOST_ITEM_MAX_NUM]{};
        if (shop) for (const auto& [id, line] : shop->GetItems()) {
            if (!line || !line->GetTable()) continue;
            int pos = line->GetInfo().pos, size = line->GetTable()->bSize;
            if (!playerbot_offline::Fits(pos, size, SHOP_PLAYER_WIDTH, SHOP_PLAYER_HOST_ITEM_MAX_NUM)) return -1;
            for (int y = 0; y < size; ++y) used[pos + y * SHOP_PLAYER_WIDTH] = true;
        }
        for (int pos = 0; pos < SHOP_PLAYER_HOST_ITEM_MAX_NUM; ++pos) {
            if (!BotOfflineValid(ch, item, pos)) continue;
            bool free = true;
            for (int y = 0; y < item->GetSize(); ++y) free &= !used[pos + y * SHOP_PLAYER_WIDTH];
            if (free) return pos;
        }
        return -1;
    }
    bool SubmitPlayerBotOfflineShop(LPCHARACTER ch, TPlayerBotAIState& state,
            DWORD now, const char* sign, TShopItemTable* table, BYTE count) {
        using namespace playerbot_offline;
        // Shops are the first channel's alone (ManagePlayerBotPrivateShop): the
        // last line of defence at the native submit, and with the assignment
        // table a bot elsewhere that got this far asks to be moved there.
        if (!EnsurePlayerBotPrivateShopChannel(ch, state, now, "submit_guard")) return false;
        auto& manager = ikashop::GetManager();
        state.dwNextShopKeepTime = now + 120000;
        if (manager.GetShopByOwnerID(ch->GetPlayerID()) || requests.count(ch->GetPlayerID()) ||
                !db_clientdesc || !db_clientdesc->IsPhase(PHASE_DBCLIENT) || !count) return false;
        // Keep the existing prices and selection, but revalidate every line,
        // including vertical grid cells, before OpenMyShop removes anything.
        std::set<WORD> cells;
        bool grid[SHOP_PLAYER_HOST_ITEM_MAX_NUM]{};
        long long total = ch->GetGold();
        for (BYTE n = 0; n < count; ++n) {
            auto item = ch->GetItem(table[n].pos);
            int pos = table[n].display_pos;
            if (!BotOfflineValid(ch, item, pos) || !cells.insert(table[n].pos.cell).second ||
                    table[n].price <= 0 || table[n].price >= GOLD_MAX) return false;
            for (int y = 0; y < item->GetSize(); ++y) {
                int c = pos + y * SHOP_PLAYER_WIDTH;
                if (grid[c]) return false;
                grid[c] = true;
            }
            total += table[n].price;
            if (total >= GOLD_MAX) return false;
        }
        constexpr BYTE duration = 1; // constants.cpp: 8 hours, 6000 Yang
        // Not in the first two minutes after this bot spawned. The core loads
        // the shop list from the db core when it connects, long before any bot
        // spawns, but a keeper parked at its pitch by the last session rolls a
        // stall on its first tick; a create for an owner the engine already
        // has a shop for is an EXTEND (duration reset, lines added, 6000 yang
        // paid again), harmless to the goods but pointless - and this spreads
        // the reopenings after a restart instead of one a second.
        if (now - state.dwSpawnTime < 120000) return false;
        if (ch->GetGold() - aOfflineShopTime[duration].price < GetPlayerBotReservedGold(ch) ||
                !BotOfflineBudget(now) || !Begin(ch->GetPlayerID(), Create, 0, now)) return false;
        ch->OpenMyShop(sign, table, count, duration);
        bool sent = EndCall(ch->GetPlayerID());
        state.dwNextShopKeepTime = now + (sent ? 600000 : 120000);
        state.offlineShop.nextService = now + number(600000, 900000);
        ClearPlayerBotRoute(state, true);
        state.vecShopOffers.clear(); // native ownership, not the old inventory mirror
        sys_log(0, "PLAYERBOT_OFFLINE: create pid=%u sent=%d lines=%u map=%ld sign=\"%s\"",
            ch->GetPlayerID(), sent, unsigned(count), ch->GetMapIndex(), sign);
        if (!sent) {
            // OpenMyShop refuses silently - a chat line to a descriptor nobody
            // reads - and refuses the whole shop over one condition, so name
            // the ones it tests (char_shop.cpp) the way the classic stall's
            // "refused" line did: a quest script running, the saddle, a
            // polymorph, a busy window, and the first item as the sample.
            quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
            LPITEM first = ch->GetItem(table[0].pos);
            const TItemTable* rp = first ? first->GetProto() : NULL;
            sys_log(0, "PLAYERBOT_OFFLINE: refused pid=%u name=%s lines=%u quest=%d riding=%d poly=%d part=%d busy=%d "
                "myshop=%d ikashop=%d vnum=%u anti=%u equipped=%d locked=%d sign=%d gold=%d level=%d",
                ch->GetPlayerID(), ch->GetName(), unsigned(count),
                (pc && pc->IsRunning()) ? 1 : 0, ch->IsHorseRiding() ? 1 : 0,
                ch->IsPolymorphed() ? 1 : 0, (int)ch->GetPart(PART_MAIN), ch->IsBusy() ? 1 : 0,
                ch->GetMyShop() ? 1 : 0, ch->GetIkarusShop() ? 1 : 0,
                first ? first->GetVnum() : 0u, rp ? rp->dwAntiFlags : 0u,
                first && first->IsEquipped() ? 1 : 0, first && first->isLocked() ? 1 : 0,
                (int)strlen(sign), (int)(ch->GetGold() / 1000), (int)ch->GetLevel());
        }
        return false; // the independent entity owns the stall; AI resumes now
    }
    bool HasPlayerBotOfflineShop(LPCHARACTER ch) {
        return ch && (ikashop::GetManager().GetShopByOwnerID(ch->GetPlayerID()) ||
            playerbot_offline::requests.count(ch->GetPlayerID()));
    }
    // What sold while the owner was off hunting. The native manager records a
    // sale as it happens (playerbot_offline::NoteSold) because it is the only
    // side that knows: the goods belong to the shop entity, so the classic
    // stall's "one pass over my own bag" cannot see it, and that whole branch
    // of ManagePlayerBotShopLifetime is unreachable on this engine anyway.
    // Drained on the owner's own tick, so both the demand memory and the gear
    // history get what the classic stall used to give them.
    void BotOfflineDrainSales(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        auto it = playerbot_offline::sold.find(ch->GetPlayerID());
        if (it == playerbot_offline::sold.end()) return;
        auto& o = state.offlineShop;
        for (const auto& line : it->second) {
            auto known = o.listed.find(line.item);
            const bool haveListing = known != o.listed.end();
            const DWORD vnum = haveListing && known->second.vnum ? known->second.vnum : line.vnum;
            const BYTE refine = haveListing ? (BYTE)known->second.refine : (BYTE)0;
            const DWORD skill = haveListing ? (DWORD)known->second.skill : 0U;
            // A line that left within PLAYERBOT_MARKET_FAST_SALE_MS of going
            // up is Iwakura's "wysoki popyt": the next counter carrying this
            // thing asks more. A line whose listing time this core never saw -
            // it went up before the last restart - is sold, but not timed.
            if (haveListing && known->second.when != 0 &&
                    now - known->second.when < PLAYERBOT_MARKET_FAST_SALE_MS) {
                NotePlayerBotFastSale(vnum, refine, now, skill);
                sys_log(0, "PLAYERBOT_MARKET: fast sale pid=%u name=%s vnum=%u+%u skill=%u in=%u s",
                    ch->GetPlayerID(), ch->GetName(), vnum, (unsigned int)refine, skill,
                    (unsigned int)((now - known->second.when) / 1000));
            }
            char hint[64];
            snprintf(hint, sizeof(hint), "%u x%u za %lld", vnum,
                (unsigned int)line.count, (long long)line.price);
            LogManager::instance().ItemLog(ch, (int)line.item, (int)vnum,
                "PLAYERBOT_STALL_SOLD", hint);
            // Every sale with how long its line stood, which is what the work on
            // unsold stock has to be measured by. -1 for a line this core never
            // saw go up: a restart inherited it, and its age is not known.
            sys_log(0, "PLAYERBOT_OFFLINE: sold pid=%u name=%s vnum=%u skill=%u count=%u price=%lld listed_s=%d",
                ch->GetPlayerID(), ch->GetName(), vnum, skill, (unsigned int)line.count,
                (long long)line.price, haveListing && known->second.when != 0
                    ? (int)((now - known->second.when) / 1000) : -1);
            if (haveListing) o.listed.erase(known);
        }
        playerbot_offline::sold.erase(it);
    }
    // The first line this counter would not take today: gear under level thirty
    // below PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE, or past the
    // PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES of it one counter carries. lowGear is
    // what stays of that gear, for the add that follows. An item the operator
    // put on "stall" is never taken home to be scrapped or cut, but it is
    // still one of a counter's lines of a kind (the two caps first below).
    DWORD BotOfflineUnwantedLine(LPCHARACTER ch, NativeShop shop, int& lowGear,
            const char** why = NULL) {
        lowGear = 0;
        DWORD unwanted = 0;
        // Which rule sent it home: the take-off line says so, or a counter's
        // traffic cannot be told apart in a log (Iwakura's list, the caps, the
        // packs, the dyes, the level-30 anvil).
        const char* reason = "";
        if (why) *why = reason;
        if (!shop) return 0;
        // Moonlight chests stand on a counter in packs, and only on the counter
        // of a bot that sells them (IsPlayerBotSurplusChest): a line of eleven
        // to thirty never sold, and one a bot that opens its chests put up
        // before 2.0.53 comes home to be opened.
        const DWORD owner = shop->GetOwnerPID();
        const bool sellsChests = IsPlayerBotResourceTrader(owner) ||
            IsPlayerBotDropper(GetPlayerBotPersonalityByPID(owner));
        int marbles = 0;
        std::set<long> marbleMobs;
        std::map<DWORD, int> sameVnum;
        for (const auto& [id, line] : shop->GetItems()) {
            if (!line) continue;
            LPITEM preview = BotOfflinePreview(*line);
            if (!preview) continue;
            // A piece Iwakura's list keeps for the storekeeper comes home, one a
            // visit, and goes down on the next Trader's visit (playerbot_lpp.h).
            if (IsPlayerBotLppKeptItem(ch, preview)) {
                if (!unwanted) { unwanted = id; reason = "lpp"; }
                M2_DELETE(preview);
                continue;
            }
            // A counter shows PLAYERBOT_SHOP_MARBLE_LINES marbles, never two of
            // one monster; the ones that went up before 2.0.78 - up to
            // thirty-one on one counter, and none ever sold - come home one a
            // visit to make room for goods that do (PLAYERBOT_SHOP_POLYMORPH_SCORE).
            // "Stall" included: the operator's word sends an item to the
            // counter instead of the merchant, not over the whole counter.
            // With marbles and Kawalek Lodu on "stall" in the test world's
            // policy file those two stood 6 735 lines over three of a kind on
            // 19 September (3 877 marbles; 2 858 of Kawalek Lodu, 46 on one
            // counter) against 3 343 for everything else together.
            if (preview->GetType() == ITEM_POLYMORPH) {
                if (!marbleMobs.insert(preview->GetSocket(0)).second ||
                        ++marbles > PLAYERBOT_SHOP_MARBLE_LINES) {
                    if (!unwanted) { unwanted = id; reason = "marble"; }
                }
                M2_DELETE(preview);
                continue;
            }
            // Past PLAYERBOT_SHOP_SAME_VNUM_LINES of one item - 46 lines of one
            // material stood on one counter - the rest come home one a visit,
            // "stall" or not.
            if (IsPlayerBotSameVnumCapped(preview) &&
                    ++sameVnum[preview->GetVnum()] > PLAYERBOT_SHOP_SAME_VNUM_LINES) {
                if (!unwanted) { unwanted = id; reason = "same_vnum"; }
                M2_DELETE(preview);
                continue;
            }
            // One of Iwakura's junk weapons while the bots' counters carry more
            // than PLAYERBOT_JUNK_WEAPON_MARKET_CAP of them comes home, and the
            // junk rule sends it to the merchant (community patch 2, point 13).
            if (IsPlayerBotCappedJunkWeapon(preview) &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL &&
                    s_iPlayerBotJunkWeaponsOnCounters > PLAYERBOT_JUNK_WEAPON_MARKET_CAP) {
                if (!unwanted) { unwanted = id; reason = "junk_weapon"; }
                M2_DELETE(preview);
                continue;
            }
            // A dye from the water the owner does not keep for sale comes home
            // to be thrown away (PLAYERBOT_HAIR_DYE_KEEP_PERMILLE): 5 147 of
            // them stood on the counters.
            if (IsPlayerBotFishedHairDye(preview->GetVnum()) &&
                    GetPlayerBotItemPolicy(preview) == PLAYERBOT_ITEM_POLICY_NONE &&
                    !IsPlayerBotHairDyeKeptForSaleId(id)) {
                if (!unwanted) { unwanted = id; reason = "hair_dye"; }
                M2_DELETE(preview);
                continue;
            }
            // A level-30 weapon of another class its owner grinds for sale
            // comes home while the next step can be paid
            // (PlayerBotRefinesLevel30ForSale): the counters held 2 717 of
            // them at +0 and +2.
            if (ch && PlayerBotRefinesLevel30ForSale(ch, preview, id) &&
                    preview->GetRefineLevel() < GetPlayerBotLevel30SaleTarget(preview) &&
                    CanPlayerBotPayRefineStep(ch, preview)) {
                if (!unwanted) { unwanted = id; reason = "level30_anvil"; }
                M2_DELETE(preview);
                continue;
            }
            if (preview->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL &&
                    (!sellsChests || (int)preview->GetCount() > PLAYERBOT_CHEST_LINE_UNITS)) {
                if (!unwanted) { unwanted = id; reason = "chest_pack"; }
                M2_DELETE(preview);
                continue;
            }
            // A soul stone line longer than PLAYERBOT_GRAND_MASTER_STONE_KEEP
            // is one no bot can buy - none is ever short of more - and the
            // stand used to put a stack up whole; it comes home to go up a
            // stone at a time (BotOfflinePrepareLine).
            if (preview->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM &&
                    (int)preview->GetCount() > PLAYERBOT_GRAND_MASTER_STONE_KEEP &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL) {
                if (!unwanted) { unwanted = id; reason = "gm_stone"; }
                M2_DELETE(preview);
                continue;
            }
            // A scroll line of more than PLAYERBOT_SHOP_SCROLL_LINE_UNITS went
            // up as a whole stack before 2.0.55; it comes home to be cut.
            if (IsPlayerBotSafeRefineScroll(preview->GetVnum()) &&
                    (int)preview->GetCount() > PLAYERBOT_SHOP_SCROLL_LINE_UNITS &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL) {
                if (!unwanted) { unwanted = id; reason = "scroll_pack"; }
                M2_DELETE(preview);
                continue;
            }
            // A line of more than PLAYERBOT_SHOP_HORSE_MEDAL_LINE_UNITS horse
            // medals went up as the stack it was; it comes home to be cut.
            if (preview->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM &&
                    (int)preview->GetCount() > PLAYERBOT_SHOP_HORSE_MEDAL_LINE_UNITS &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL) {
                if (!unwanted) { unwanted = id; reason = "medal_pack"; }
                M2_DELETE(preview);
                continue;
            }
            // A herb line under a heap and a material line over a hoard's
            // pack went up before 2.0.68 - the stand put up whatever stack a
            // cell held: 1171 single roots and 1084 material lines of more
            // than ten on m2zip. They come home to be merged or cut.
            if (GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL &&
                    ((IsPlayerBotBulkGoods(preview) &&
                      (int)preview->GetCount() < PLAYERBOT_SHOP_BULK_MIN_UNITS) ||
                     (IsPlayerBotTradeableMaterial(preview) &&
                      (int)preview->GetCount() > PLAYERBOT_SHOP_HOARD_PACK_UNITS))) {
                if (!unwanted) { unwanted = id; reason = "hoard_pack"; }
                M2_DELETE(preview);
                continue;
            }
            if (IsPlayerBotLowLevelGear(preview) &&
                    GetPlayerBotItemPolicy(preview) != PLAYERBOT_ITEM_POLICY_STALL) {
                const bool capped = CountsAgainstPlayerBotLowGearCap(preview);
                if (preview->GetRefineLevel() < GetPlayerBotLowGearMinRefine(preview) ||
                        (capped && lowGear >= PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES)) {
                    if (!unwanted) { unwanted = id; reason = "low_gear"; }
                } else if (capped) {
                    ++lowGear;
                }
            }
            M2_DELETE(preview);
        }
        if (why) *why = reason;
        return unwanted;
    }
    // One line back into the owner's bag, through the journal like every other
    // mutation. A bag with no room refuses it synchronously and the next visit
    // asks again. True when the request reached the DB core.
    bool BotOfflineTakeOff(LPCHARACTER ch, TPlayerBotAIState& state, DWORD itemid, int lowGear, DWORD now,
            const char* why = "") {
        using namespace playerbot_offline;
        if (!Begin(ch->GetPlayerID(), Remove, itemid, now)) return false;
        ikashop::GetManager().RecvShopRemoveItemClientPacket(ch, itemid);
        if (!EndCall(ch->GetPlayerID())) return false;
        state.offlineShop.listed.erase(itemid);
        // Off the world's count at once, so the next keeper's visit this
        // minute does not take a second one home for the same surplus.
        if (why && strcmp(why, "junk_weapon") == 0 && s_iPlayerBotJunkWeaponsOnCounters > 0)
            --s_iPlayerBotJunkWeaponsOnCounters;
        sys_log(0, "PLAYERBOT_OFFLINE: took off pid=%u name=%s item=%u low_gear_kept=%d reason=%s",
            ch->GetPlayerID(), ch->GetName(), itemid, lowGear, why);
        return true;
    }
    // A piece on the owner's own counter it should be wearing: better, by the
    // equipment pass's own score, than what it has on and than anything in its
    // bag for the slot. Nothing asked the counter for gear - only for room to
    // add goods - so a warrior of 75 whose weapon burned at the anvil fought on
    // with a Gilotynowe Ostrze +7 of level ten while a Halabarda +6 and three
    // swords of level 55 stood on her own counter (CiosZKarpia, Tieru,
    // 15 September). A line taken back within six hours is not taken again:
    // a piece the equipment pass will not put on would otherwise go back on
    // the counter and come off it every visit.
    DWORD BotOfflineReclaimLine(LPCHARACTER ch, const TPlayerBotAIState& state, NativeShop shop,
            DWORD now, long long& gain) {
        gain = 0;
        DWORD best = 0;
        if (!ch || !shop) return 0;
        const auto& o = state.offlineShop;
        for (const auto& [id, line] : shop->GetItems()) {
            if (!line || !line->GetTable()) continue;
            const BYTE type = line->GetTable()->bType;
            if (type != ITEM_WEAPON && type != ITEM_ARMOR) {
                LPITEM needed = BotOfflinePreview(*line);
                bool reclaim = needed && IsPlayerBotProgressionOffer(ch, needed);
                if (needed) M2_DELETE(needed);
                if (reclaim) { gain = 1; return id; }
                continue;
            }
            if (id == o.lastReclaimItem && !playerbot_offline::Due(now, o.lastReclaimAt + 21600000U)) continue;
            LPITEM preview = BotOfflinePreview(*line);
            if (!preview) continue;
            long long lineGain = 0;
            const int wearCell = IsPlayerBotEquipmentCandidate(ch, preview) &&
                    preview->GetLevelLimit() <= ch->GetLevel() ? preview->FindEquipCell(ch) : -1;
            if (wearCell >= 0 && wearCell < WEAR_MAX_NUM &&
                    (wearCell != WEAR_SHIELD || PlayerBotWantsShield(ch))) {
                LPITEM worn = ch->GetWear((BYTE)wearCell);
                if (!worn || !IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE)) {
                    // A rod or a pickaxe in the weapon slot is the session's tool,
                    // not the weapon to beat; that one waits in the bag.
                    long long baseline = worn && worn->GetType() == type ? GetPlayerBotEquipmentScore(worn, ch) : 0;
                    for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell) {
                        LPITEM held = ch->GetInventoryItem(cell);
                        if (!held || held->IsEquipped() || held->GetType() != type ||
                                !IsPlayerBotEquipmentCandidate(ch, held) ||
                                held->GetLevelLimit() > ch->GetLevel() || held->FindEquipCell(ch) != wearCell)
                            continue;
                        baseline = std::max(baseline, GetPlayerBotEquipmentScore(held, ch));
                    }
                    lineGain = GetPlayerBotEquipmentScore(preview, ch) - baseline;
                    // Worth a round trip and a line off the counter only by a margin.
                    if (baseline > 0 && lineGain * 100 < baseline * PLAYERBOT_OFFLINE_RECLAIM_MIN_GAIN_PERCENT)
                        lineGain = 0;
                }
            }
            M2_DELETE(preview);
            if (lineGain > gain) {
                gain = lineGain;
                best = id;
            }
        }
        return best;
    }
    // Back into the bag through the journal, for the equipment pass to put on.
    bool BotOfflineReclaim(LPCHARACTER ch, TPlayerBotAIState& state, DWORD itemid, long long gain, DWORD now) {
        using namespace playerbot_offline;
        if (!Begin(ch->GetPlayerID(), Remove, itemid, now)) return false;
        ikashop::GetManager().RecvShopRemoveItemClientPacket(ch, itemid);
        if (!EndCall(ch->GetPlayerID())) return false;
        state.offlineShop.listed.erase(itemid);
        state.offlineShop.lastReclaimItem = itemid;
        state.offlineShop.lastReclaimAt = now;
        sys_log(0, "PLAYERBOT_OFFLINE: took back to wear pid=%u name=%s item=%u gain=%lld",
            ch->GetPlayerID(), ch->GetName(), itemid, gain);
        return true;
    }
    // How many lines of this vnum the counter carries already.
    int BotOfflineLinesOf(NativeShop shop, DWORD vnum) {
        int lines = 0;
        if (shop)
            for (const auto& [id, line] : shop->GetItems())
                if (line && line->GetInfo().vnum == vnum) ++lines;
        return lines;
    }
    // How many lines of this item's kind kept by count - the books of its
    // skill, the soul stone (GetPlayerBotStallKindKey) - the counter carries.
    int BotOfflineKindLinesOf(NativeShop shop, LPITEM item) {
        const DWORD kind = GetPlayerBotStallKindKey(item);
        int lines = 0;
        if (!shop || !kind) return 0;
        for (const auto& [id, line] : shop->GetItems()) {
            if (!line || !line->GetTable()) continue;
            const auto& info = line->GetInfo();
            if (GetPlayerBotStallKindKeyOf(info.vnum, line->GetTable()->bType,
                    info.alSockets[0], line->GetTable()->alValues[0]) == kind)
                ++lines;
        }
        return lines;
    }
    // The polymorph marbles a counter carries, and whether one of them is this
    // monster's (socket 0).
    int BotOfflineMarbleLines(NativeShop shop, long mob, bool& sameMob) {
        int lines = 0;
        sameMob = false;
        if (shop)
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line || !line->GetTable() || line->GetTable()->bType != ITEM_POLYMORPH) continue;
                ++lines;
                if (line->GetInfo().alSockets[0] == mob) sameMob = true;
            }
        return lines;
    }
    // What a counter takes no more lines of: a material past
    // PLAYERBOT_SHOP_MATERIAL_LINES and a heap of cheap goods past
    // PLAYERBOT_SHOP_BULK_LINES (each a pack cut by BotOfflinePrepareLine),
    // the Moonlight chests, the safe scrolls, the books and stones kept by
    // count, and a marble past PLAYERBOT_SHOP_MARBLE_LINES or of a monster
    // the counter shows already - an item on "stall" as much as any. The line
    // chosen before the board opens and the add itself ask this one question,
    // or a line cut for the add would stand in the bag unadded.
    bool BotOfflineCounterRefuses(NativeShop shop, LPITEM item) {
        if (!item) return true;
        if (IsPlayerBotTradeableMaterial(item) &&
                BotOfflineLinesOf(shop, item->GetVnum()) >= PLAYERBOT_SHOP_MATERIAL_LINES) return true;
        if (IsPlayerBotBulkGoods(item) &&
                BotOfflineLinesOf(shop, item->GetVnum()) >= PLAYERBOT_SHOP_BULK_LINES) return true;
        if (item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
                BotOfflineLinesOf(shop, item->GetVnum()) >= PLAYERBOT_CHEST_COUNTER_LINES) return true;
        if (IsPlayerBotSafeRefineScroll(item->GetVnum()) &&
                BotOfflineLinesOf(shop, item->GetVnum()) >= PLAYERBOT_SHOP_SCROLL_LINES) return true;
        if (IsPlayerBotCountedSingleGoods(item) &&
                BotOfflineKindLinesOf(shop, item) >= PLAYERBOT_SHOP_COUNTED_SINGLE_LINES) return true;
        if (item->GetType() == ITEM_POLYMORPH) {
            bool sameMob = false;
            if (BotOfflineMarbleLines(shop, item->GetSocket(0), sameMob) >= PLAYERBOT_SHOP_MARBLE_LINES || sameMob)
                return true;
        }
        // Nor one of Iwakura's junk weapons past the world's cap.
        if (IsPlayerBotCappedJunkWeapon(item) && IsPlayerBotJunkWeaponMarketFull()) return true;
        // And no more than PLAYERBOT_SHOP_SAME_VNUM_LINES of anything else.
        if (IsPlayerBotSameVnumCapped(item) &&
                BotOfflineLinesOf(shop, item->GetVnum()) >= PLAYERBOT_SHOP_SAME_VNUM_LINES)
            return true;
        return false;
    }
    // The bag cell of the line to add. A hoard's pack of ten, a single key, a
    // pack of Moonlight chests (PLAYERBOT_CHEST_LINE_UNITS), a line of
    // refine scrolls (PLAYERBOT_SHOP_SCROLL_LINE_UNITS), a pack of a refine
    // material (PLAYERBOT_SHOP_PACK_UNITS) or a heap of cheap goods
    // (PLAYERBOT_SHOP_BULK_PACK_UNITS)
    // is cut off its stack into a free cell (GetPlayerBotStallLineUnitsFor);
    // anything else goes up as the stack it is, as it always has - a stand
    // adds one line a visit. -1 when no line can be cut without the stack's
    // reserve or the bag's last free cells.
    int BotOfflinePrepareLine(LPCHARACTER ch, WORD cell) {
        LPITEM item = ch->GetInventoryItem(cell);
        if (!item) return -1;
        const int units = GetPlayerBotStallLineUnitsFor(ch, item);
        // A material went up as the stack it was, the anvil's reserve
        // included, and a herb as whatever a cell held. Both are cut the way
        // a scroll is: what is over the keep, counted over every stack of the
        // kind, up to the line - and a heap is never under its minimum.
        const bool bulk = IsPlayerBotBulkGoods(item);
        // A safe refine scroll is a material too (recipe 501) and keeps its
        // own branch below, keep and all.
        if (bulk || (IsPlayerBotTradeableMaterial(item) &&
                !IsPlayerBotSafeRefineScroll(item->GetVnum()))) {
            const int keep = GetPlayerBotStallBaseKeep(ch, item);
            const int spare = (int)ch->CountSpecifyItem(item->GetVnum()) - keep;
            const int take = std::min(units, std::min((int)item->GetCount(), spare));
            if (take <= 0 || (bulk && take < PLAYERBOT_SHOP_BULK_MIN_UNITS)) return -1;
            if (take >= (int)item->GetCount()) return cell;
            if (CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS) return -1;
            const int to = ch->GetEmptyInventory(item->GetSize());
            if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, (WORD)to), take))
                return -1;
            sys_log(0, "PLAYERBOT_OFFLINE: cut a line pid=%u name=%s vnum=%u units=%d left=%u keep=%d",
                ch->GetPlayerID(), ch->GetName(), item->GetVnum(), take, (unsigned int)item->GetCount(), keep);
            return to;
        }
        if (IsPlayerBotSafeRefineScroll(item->GetVnum()) || item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM) {
            // A scroll line is what the bot holds over its own keep
            // (GetPlayerBotStallBaseKeep), up to the line: a stack of five
            // with a keep of three is a line of two, never the whole stack.
            // The keep is counted over every stack of the kind, so a line
            // already cut off (BotOfflinePrepareVisitLine) goes up whole while
            // the rest of the scrolls stay in the stack it came from. Horse
            // medals the same way, two to a line over the two a bot keeps.
            const int keep = GetPlayerBotStallBaseKeep(ch, item);
            const int spare = (int)ch->CountSpecifyItem(item->GetVnum()) - keep;
            const int take = std::min(units, std::min((int)item->GetCount(), spare));
            if (take <= 0) return -1;
            if (take >= (int)item->GetCount()) return cell;
            if (CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS) return -1;
            const int to = ch->GetEmptyInventory(item->GetSize());
            if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, (WORD)to), take))
                return -1;
            sys_log(0, "PLAYERBOT_OFFLINE: cut a line pid=%u name=%s vnum=%u units=%d left=%u keep=%d",
                ch->GetPlayerID(), ch->GetName(), item->GetVnum(), take, (unsigned int)item->GetCount(), keep);
            return to;
        }
        if (IsPlayerBotCountedSingleGoods(item)) {
            // A book of its own skill and a soul stone keep their count over the
            // whole kind (GetPlayerBotCountedGoodsKeep), and a line is one unit
            // of what is over it: a bot buys a line only when all of it fits
            // what it is short of, so a stack went up whole and stood there -
            // ten stones against a buyer short of three. The stack keeps the
            // rest, the keep included, and the next visit cuts the next.
            const int keep = GetPlayerBotCountedGoodsKeep(ch, item);
            const int total = CountPlayerBotStallKindUnits(ch, item);
            const int take = playerbot_stall_rules::LineTake((int)item->GetCount(), total, keep, units);
            if (take <= 0) return -1;
            if (take >= (int)item->GetCount()) return cell;
            if (CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS) return -1;
            const int to = ch->GetEmptyInventory(item->GetSize());
            if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, (WORD)to), take))
                return -1;
            sys_log(0, "PLAYERBOT_OFFLINE: cut a line pid=%u name=%s vnum=%u units=%d left=%u keep=%d kind=%u total=%d",
                ch->GetPlayerID(), ch->GetName(), item->GetVnum(), take, (unsigned int)item->GetCount(), keep,
                (unsigned int)(GetPlayerBotStallKindKey(item) & 0x7fffffffU), total);
            return to;
        }
        const bool cut = units == PLAYERBOT_SHOP_HOARD_PACK_UNITS ||
            (units == 1 && item->GetType() == ITEM_TREASURE_KEY) ||
            item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM;
        if (!cut || (int)item->GetCount() <= units) return cell;
        if ((int)item->GetCount() - units < GetPlayerBotStallBaseKeep(ch, item) ||
                CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS)
            return -1;
        const int to = ch->GetEmptyInventory(item->GetSize());
        if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, (WORD)to), units))
            return -1;
        sys_log(0, "PLAYERBOT_OFFLINE: cut a line pid=%u name=%s vnum=%u units=%d left=%u",
            ch->GetPlayerID(), ch->GetName(), item->GetVnum(), units, (unsigned int)item->GetCount());
        return to;
    }
    // The line this visit will add, cut out of its stack while the bot can
    // still handle its bag. Once the board is open - looking at the shop, its
    // safebox, edit mode - IsBusy is true and MoveItem refuses through
    // CanHandleItem, so every cut in the add loop failed silently: not one
    // scroll, hoard pack or chest pack was ever cut on a service visit, and
    // the diag lines of 16 September read score 800, a slot, a keep of three
    // and lineCell=-1 for every scroll. Remembered by item id and cell for the
    // add of the same visit; a cut left behind by a visit that ended early is
    // an ordinary split stack, poured back by the merge pass.
    void BotOfflinePrepareVisitLine(LPCHARACTER ch, TPlayerBotAIState& state, NativeShop shop) {
        auto& o = state.offlineShop;
        o.preparedItem = 0;
        if (!ch || !shop || shop->GetDuration() == 0) return;
        int lowGear = 0;
        BotOfflineUnwantedLine(ch, shop, lowGear);
        std::vector<std::pair<int, WORD> > scored;
        CollectPlayerBotShopItems(ch, scored, IsPlayerBotStallKeeper(state), lowGear);
        for (auto [score, cell] : scored) {
            LPITEM item = ch->GetInventoryItem(cell);
            if (!item || BotOfflineSlot(ch, shop, item) < 0) continue;
            if (BotOfflineCounterRefuses(shop, item)) continue;
            const int lineCell = BotOfflinePrepareLine(ch, cell);
            if (lineCell < 0) continue;
            LPITEM line = ch->GetInventoryItem((WORD)lineCell);
            if (!line) continue;
            o.preparedItem = line->GetID();
            o.preparedCell = (uint32_t)lineCell;
            return;
        }
    }
    // A name for what the shop holds now, by Iwakura's rules over previews of
    // its own lines.
    bool BotOfflineNameForGoods(LPCHARACTER ch, NativeShop shop, char* out, size_t outSize, const char** how) {
        std::vector<LPITEM> goods;
        if (shop)
            for (const auto& [id, line] : shop->GetItems())
                if (line)
                    if (LPITEM preview = BotOfflinePreview(*line))
                        goods.push_back(preview);
        const bool named = ChoosePlayerBotShopName(ch, goods, out, outSize, how);
        for (LPITEM preview : goods)
            M2_DELETE(preview);
        return named;
    }
    bool ManagePlayerBotOfflineService(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        if (!ch) return false;
        BotOfflineDrainSales(ch, state, now);
        auto& o = state.offlineShop;
        auto& manager = ikashop::GetManager();
        auto shop = manager.GetShopByOwnerID(ch->GetPlayerID());
        ch->SetIkarusShop(shop); // boot/relog: native PID map is authoritative
        if (BotOfflinePoll(ch, now)) {
            // Never stand waiting for the DB, nor leave edit mode locking sales.
            if (o.visiting) BotOfflineFinishVisit(ch, state, now);
            if (o.repriceSteps) o.nextService = now + 2000;
            return false;
        }
        // The first visit after a spawn is spread over a whole service interval.
        // "pid % 60000" was meant to spread it over a minute, but every
        // registered pid is below 2504, so every keeper went thirty to thirty-
        // two seconds after a restart: 451 map changes in the first two minutes
        // of 14 September, each keeper pulled out of the dungeon or frontier it
        // had just been spawned on, and the same wave again ten to fifteen
        // minutes later because the whole population's clocks started together.
        // The thirty seconds stay: the shop list has to arrive from the DB first.
        // A dropper's first visit is spread over its own long round: the ten
        // minutes pulled the medal droppers out of the second village 69 times
        // in the first fourteen minutes after a restart.
        if (o.nextService == 0)
            o.nextService = now + 30000 + PlayerBotNavHash(ch->GetPlayerID() ^ 0x4f534856U) %
                (IsPlayerBotDropper(state.bPersonality) ? PLAYERBOT_DROPPER_SHOP_SERVICE_MAX_MS : (DWORD)600000);
        if (BotOfflineBusy(ch, state) || !db_clientdesc || !db_clientdesc->IsPhase(PHASE_DBCLIENT)) {
            if (o.visiting) BotOfflineFinishVisit(ch, state, now);
            return false;
        }
        // An empty hand does not wait out the service interval when its own
        // counter holds something to wear (BotOfflineReclaimLine); probed once
        // a minute.
        if (!o.visiting && shop && !ch->GetWear(WEAR_WEAPON) && Due(now, o.nextReclaimProbe)) {
            o.nextReclaimProbe = now + 60000;
            long long gain = 0;
            if (BotOfflineReclaimLine(ch, state, shop, now, gain) != 0)
                o.nextService = now;
        }
        if (!Due(now, o.nextService)) return false;
        // A purchase under way comes first. A service visit opens the board,
        // and the buyer gives its pick up to anybody with the board open: four
        // of the first 155 walks over from a second village lost the line they
        // came for to their own keeper's visit starting on arrival (23
        // September). Both waits are bounded by their own clocks.
        if (!o.visiting && ((o.buyOwner && !Due(now, o.buyUntil)) ||
                (state.bMarketTrip && !Due(now, state.dwMarketTripUntil))))
            return false;
        if (!shop) {
            // Proceeds even after sell-out deleted the empty shop.
            auto box = manager.GetShopSafeboxByOwnerID(ch->GetPlayerID());
            long x = 0, y = 0;
            if (box && GetPlayerBotShopCentre(ch->GetMapIndex(), x, y) &&
                    DISTANCE_APPROX(ch->GetX()-x, ch->GetY()-y) <= PLAYERBOT_SHOP_RING_RADIUS + 1000 &&
                    BotOfflineBudget(now)) {
                manager.RecvShopSafeboxOpenClientPacket(ch);
                if (box->GetValutes().yang > 0) manager.RecvShopSafeboxGetValutesClientPacket(ch);
                // Returned items use the DB-confirmed inventory-space check.
                if (!box->GetItems().empty()) {
                    auto itemid = box->GetItems().begin()->first;
                    if (Begin(ch->GetPlayerID(), WithdrawItem, itemid, now)) {
                        manager.RecvShopSafeboxGetItemClientPacket(ch, itemid);
                        EndCall(ch->GetPlayerID());
                    }
                }
                manager.RecvShopSafeboxCloseClientPacket(ch);
            }
            // A slice cut short by the stand selling out has nothing left to
            // reprice, and a step count left behind would keep bringing the
            // keeper back every two seconds (the poll branch above).
            o.repriceSteps = 0;
            o.nextService = now + BotOfflineServiceGap(state);
            return false;
        }
        const auto spawn = shop->GetSpawn();
        // The stand is on the other channel: with the assignment table the
        // owner asks to be moved to it - opening, repricing and emptying the
        // counter all need the owner beside it. A move is a logout and a
        // login, dearer than the map change the long round below spares, so
        // the stand waits a long round of its own, counted from the bot's
        // arrival on this channel (the clock of its last service stayed on
        // the core it came from) and spread by pid, or a channel's whole
        // cohort asks in the same minute after a start. The first build
        // asked at the first service of every keeper here: 235 of 397 bots of
        // the second channel waiting within twelve minutes, against 33 places
        // a gate. An expired stand waits the same round: the second build
        // asked for one at once, and a stand its owner will not renew - the
        // TRADE slider, no yang for the fee - stays expired with its goods
        // for good, so its owner went back and forth between the channels
        // for nothing. Once due, the bot asks again every retry until the
        // coordinator moves it.
        if (spawn.channel != g_bChannel && CPlayerBotManager::instance().IsChannelTableMode()) {
            const bool expired = shop->GetDuration() == 0;
            const DWORD since = state.dwSpawnTime ? state.dwSpawnTime : now;
            const DWORD wait = PLAYERBOT_SHOP_CHANNEL_SERVICE_MIN_MS +
                    PlayerBotNavHash(ch->GetPlayerID() ^ 0x43485356U) % PLAYERBOT_SHOP_CHANNEL_SERVICE_SPREAD_MS;
            BotOfflineFinishVisit(ch, state, now);
            if (!Due(now, since + wait)) {
                o.nextService = now + PLAYERBOT_OFFLINE_FAR_SERVICE_RETRY_MS;
                return false;
            }
            if (CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID()))
                PlayerBotLogThrottled("shop_channel_service", now,
                        "PLAYERBOT_CHANNEL: pid=%u name=%s asks for the shop channel to serve its stand (here %u, expired %d)",
                        ch->GetPlayerID(), ch->GetName(), (unsigned int)g_bChannel, expired ? 1 : 0);
            o.nextService = now + PLAYERBOT_SHOP_CHANNEL_REQUEST_RETRY_MS;
            return false;
        }
        if (spawn.channel != g_bChannel ||
                playerbot_empire_rules::GetMapOwnerEmpire(spawn.map) != ch->GetEmpire() ||
                !IsPlayerBotMapHostedHere(spawn.map)) {
            BotOfflineFinishVisit(ch, state, now);
            return false;
        }
        // Where this visit is served. An expired stand on a map that no longer
        // takes one (a second village with the SHOP_M2 switch off) is renewed
        // on its owner's first village ring instead: a renewal stands where its
        // owner stands (OpenOfflineShop), so the stand moves there rather than
        // being walked back to. A stand still running is served where it is,
        // and moves when it has run out.
        long serviceMap = spawn.map, serviceX = spawn.x, serviceY = spawn.y;
        if (shop->GetDuration() == 0 && !IsPlayerBotShopMapAllowed(spawn.map)) {
            const long home = playerbot_empire_rules::GetHomeMap((int)ch->GetEmpire(),
                    playerbot_empire_rules::MAP_ROLE_M1);
            long px = 0, py = 0;
            if (IsPlayerBotMapHostedHere(home) && GetPlayerBotShopCentre(home, px, py)) {
                long ox = 0, oy = 0;
                GetPlayerBotStableOffset(ch->GetPlayerID(), 0x4d4b5450U,
                        PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS, ox, oy);
                serviceMap = home;
                serviceX = px + ox;
                serviceY = py + oy;
            }
        }
        // A shop on another map waits for the long round
        // (PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS): two map changes a visit for
        // every keeper out on the frontier was most of the gates' traffic. An
        // empty hand with a weapon on its own counter does not wait (the
        // reclaim probe above), nor does the first visit after a start.
        if (!o.visiting && ch->GetMapIndex() != serviceMap && ch->GetWear(WEAR_WEAPON) &&
                o.lastServedAt != 0 && !Due(now, o.lastServedAt + PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS)) {
            o.nextService = now + PLAYERBOT_OFFLINE_FAR_SERVICE_RETRY_MS;
            return false;
        }
        if (!o.visiting) {
            o.visiting = true;
            o.visitUntil = now + 90000; // absolute upper bound, including travel
            o.nextStep = 0;
        }
        if (Due(now, o.visitUntil)) {
            BotOfflineFinishVisit(ch, state, now);
            return false;
        }
        SetPlayerBotAction(state, BOT_ACTION_TRAVEL, now);
        if (ch->GetMapIndex() != serviceMap) {
            // Reuses world-travel safety checks; never manufactures cross-core warps.
            TransitionPlayerBotMap(ch, state, serviceMap, serviceX, serviceY, now, "offline_shop_service");
            return true;
        }
        if (!MovePlayerBotTownLeg(ch, state, now, serviceX, serviceY, 800)) return true;
        if (!Due(now, o.nextStep)) return true;
        o.nextStep = now + 3000;
        if (!BotOfflineBudget(now)) return true;
        o.lastServedAt = now;
        // A visit that reprices adds nothing - the restock loop below stops at
        // once on the same test - so it cuts no line: the cut would only be
        // poured back by the merge pass, after a whole bag's scoring for it.
        if (!o.restockTurn && o.nextReprice && Due(now, o.nextReprice) && !shop->GetItems().empty())
            o.preparedItem = 0;
        else
            BotOfflinePrepareVisitLine(ch, state, shop);
        ch->SetLookingShopOwner(true);
        manager.RecvShopSafeboxOpenClientPacket(ch);
        auto box = ch->GetIkarusShopSafebox();
        if (box && box->GetValutes().yang > 0) manager.RecvShopSafeboxGetValutesClientPacket(ch);
        manager.RecvShopSafeboxCloseClientPacket(ch);
        if (shop->GetDuration() == 0) {
            // An expired stand needs no edit mode to give a line back, and one
            // it would no longer take comes off before the stand is renewed.
            int lowGear = 0;
            const char* why = "";
            const DWORD unwanted = BotOfflineUnwantedLine(ch, shop, lowGear, &why);
            if (unwanted && BotOfflineTakeOff(ch, state, unwanted, lowGear, now, why)) {
                BotOfflineFinishVisit(ch, state, now);
                return false;
            }
            // So does a piece the owner should be wearing.
            {
                long long gain = 0;
                const DWORD reclaim = BotOfflineReclaimLine(ch, state, shop, now, gain);
                if (reclaim && BotOfflineReclaim(ch, state, reclaim, gain, now)) {
                    BotOfflineFinishVisit(ch, state, now);
                    return false;
                }
            }
            // The operator moved the TRADE slider while this stand was up. An
            // eight-hour offline stand is not worth closing early - the fee is
            // paid and the goods are with the entity - but it is not renewed
            // under a weight that no longer wants it. The four exceptions
            // (Merchant, poor, full bag, dropper pressure) are not asked: the
            // slider never applied to them.
            const bool stillWanted = !IsPlayerBotShopReasonRolled(state.bShopOpenReason) ||
                    ShouldPlayerBotKeepShop(ch, state);
            if (stillWanted && !shop->GetItems().empty() &&
                    ch->GetGold() - aOfflineShopTime[1].price >= GetPlayerBotReservedGold(ch) &&
                    Begin(ch->GetPlayerID(), Create, 0, now)) {
                // Renamed for what it holds now - eight hours of service visits
                // have added to it - by the same rules as a new stand. A name
                // from before those rules is not renewed either.
                char sign[SHOP_SIGN_MAX_LEN + 1];
                const char* how = "kept";
                if (!BotOfflineNameForGoods(ch, shop, sign, sizeof(sign), &how))
                    strlcpy(sign, shop->GetName(), sizeof(sign));
                manager.RecvShopReopenClientPacket(ch, sign, 1);
                if (EndCall(ch->GetPlayerID()))
                    sys_log(0, "PLAYERBOT_OFFLINE: reopen pid=%u name=%s lines=%u sign=\"%s\" how=%s moved_from=%ld",
                        ch->GetPlayerID(), ch->GetName(), unsigned(shop->GetItems().size()), sign, how,
                        serviceMap != spawn.map ? (long)spawn.map : 0L);
            }
            BotOfflineFinishVisit(ch, state, now);
            return false;
        }
        // Everything on the counter this core did not watch go up - the lines
        // a restart inherited. Recorded without a time, so a sale is still
        // written to the gear history and only the "how fast" is left out
        // rather than guessed at. emplace keeps a real listing time where
        // there is one.
        for (const auto& [lineId, line] : shop->GetItems())
            if (line)
                o.listed.emplace(lineId,
                    playerbot_offline::ListedLine{ line->GetVnum(), 0u, 0u, 0 });
        // Edit is opened for one bounded operation only, never during hunting.
        if (!manager.RecvShopRequestEditClientPacket(ch, true)) {
            BotOfflineFinishVisit(ch, state, now);
            return false;
        }
        // A line the counter would not take today comes off before anything
        // goes on - that is the operation of this visit. The stands already up
        // when the rule arrived held 2 409 such lines between them; a bag with
        // no room for the piece refuses, and then the visit adds instead.
        int lowGearOnCounter = 0;
        {
            const char* why = "";
            const DWORD unwanted = BotOfflineUnwantedLine(ch, shop, lowGearOnCounter, &why);
            if (unwanted && BotOfflineTakeOff(ch, state, unwanted, lowGearOnCounter, now, why)) {
                BotOfflineFinishVisit(ch, state, now);
                return false;
            }
        }
        // And a piece the owner should be wearing comes home before anything
        // goes on (BotOfflineReclaimLine).
        {
            long long gain = 0;
            const DWORD reclaim = BotOfflineReclaimLine(ch, state, shop, now, gain);
            if (reclaim && BotOfflineReclaim(ch, state, reclaim, gain, now)) {
                BotOfflineFinishVisit(ch, state, now);
                return false;
            }
        }
        std::vector<std::pair<int, WORD> > scored;
        CollectPlayerBotShopItems(ch, scored, IsPlayerBotStallKeeper(state), lowGearOnCounter);
        // The line cut before the board opened goes first, whatever it scores
        // now: it is exactly a line, so BotOfflinePrepareLine below hands it
        // back as it is. A stale cell (the item gone, or another in its
        // place) is simply not it.
        if (o.preparedItem) {
            LPITEM line = ch->GetInventoryItem((WORD)o.preparedCell);
            if (line && line->GetID() == o.preparedItem)
                scored.insert(scored.begin(), std::make_pair(1000000, (WORD)o.preparedCell));
            o.preparedItem = 0;
        }
        bool sent = false;
        // The first visit after a spawn restocks, as it always did: the
        // counters were priced a minute before the restart, so the reprice
        // clock starts an hour on and the two take turns from then on. The
        // stamp is taken to be the generation this core runs, for the same
        // reason: left at 0 until a rotation came round - fifteen hours for
        // a counter of thirty lines at two an hour - it could not tell a yang
        // rate moved in the panel from no change at all.
        if (o.nextReprice == 0) o.nextReprice = now + PLAYERBOT_OFFLINE_REPRICE_MS;
        if (o.priceGeneration == 0) o.priceGeneration = GetPlayerBotPriceGeneration();
        const bool allowRestock = o.restockTurn;
        o.restockTurn = false;
        for (auto [score, cell] : scored) {
            if (!allowRestock && Due(now, o.nextReprice) && !shop->GetItems().empty()) break;
            auto item = ch->GetInventoryItem(cell);
            int pos = BotOfflineSlot(ch, shop, item);
            if (pos < 0) continue;
            if (BotOfflineCounterRefuses(shop, item)) continue;
            const int lineCell = BotOfflinePrepareLine(ch, cell);
            if (lineCell < 0) continue;
            const WORD at = (WORD)lineCell;
            item = ch->GetInventoryItem(at);
            if (!item || !BotOfflineValid(ch, item, pos)) continue;
            ikashop::TPriceInfo price{};
            price.yang = std::max(GetPlayerBotShopAskingPrice(item), GetPlayerBotRefineInvestment(item));
            if (price.yang <= 0 || price.yang >= GOLD_MAX ||
                    shop->GetTotalYangValue() >= GOLD_MAX - price.yang) continue;
            DWORD id = item->GetID();
            if (Begin(ch->GetPlayerID(), Add, id, now)) {
                manager.RecvShopAddItemClientPacket(ch, TItemPos(INVENTORY, at), price, pos);
                sent = EndCall(ch->GetPlayerID());
                // Remembered while the item is still in hand: once it sells,
                // the only thing left is its id. The skill is socket 0 - every
                // ordinary book is vnum 50300 and one cheap sale of a spare
                // must not set the price of Aura Miecza.
                if (sent) {
                    o.listed[id] = playerbot_offline::ListedLine{
                        item->GetVnum(),
                        item->GetType() == ITEM_SKILLBOOK ? (uint32_t)item->GetSocket(0) : 0u,
                        now, (uint8_t)item->GetRefineLevel() };
                    // On the ledger at once, by its village, like a classic
                    // stall's lines: the next keeper there must not put the
                    // same material up against the player's floor in the
                    // minute before the ledger is rebuilt.
                    AddPlayerBotMarketSupply(item->GetVnum(), (WORD)item->GetCount(), shop->GetSpawn().map);
                    NotePlayerBotJunkWeaponOnCounter(item->GetVnum(), (int)item->GetCount());
                }
            }
            break; // at most one item per short service visit
        }
        if (!sent && Due(now, o.nextReprice)) {
            if (o.repriceSteps == 0) o.repriceSteps = PLAYERBOT_OFFLINE_REPRICE_SLICE;
            // Rotate by ID, one existing offer per visit; recompute from market
            // policy, not a repeated percentage markdown tending towards zero.
            auto it = shop->GetItems().upper_bound(o.repriceItem);
            bool wrapped = (it == shop->GetItems().end());
            if (wrapped) it = shop->GetItems().begin();
            if (it != shop->GetItems().end() && it->second) {
                o.repriceItem = it->first;
                auto preview = BotOfflinePreview(*it->second);
                if (preview) {
                    ikashop::TPriceInfo price{};
                    // A line nobody has bought comes down a step for every
                    // PLAYERBOT_OFFLINE_UNSOLD_STEP_MS it has stood, to the ceiling
                    // the classic stall's markdown has and never under what the
                    // blacksmith was paid (Tieru, 16 September). The clock is the
                    // listing's own (o.listed); a line from before this core
                    // started is clocked from the first visit that sees it.
                    auto listed = o.listed.find(it->first);
                    if (listed == o.listed.end())
                        listed = o.listed.emplace(it->first, playerbot_offline::ListedLine{
                                preview->GetVnum(),
                                preview->GetType() == ITEM_SKILLBOOK ? (uint32_t)preview->GetSocket(0) : 0u,
                                now, (uint8_t)preview->GetRefineLevel() }).first;
                    // Unknown age after restart is not the process uptime.
                    if (listed->second.when == 0 && listed->second.observedSince == 0)
                        listed->second.observedSince = now;
                    const uint32_t since = listed->second.when ? listed->second.when : listed->second.observedSince;
                    const uint32_t standing = now - since;
                    int discount = (int)(standing / PLAYERBOT_OFFLINE_UNSOLD_STEP_MS) * PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT;
                    if (discount > PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL)
                        discount = PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL;
                    const long long asking = (long long)GetPlayerBotShopAskingPrice(preview) * (100 - discount) / 100;
                    price.yang = std::max(asking, (long long)GetPlayerBotRefineInvestment(preview));
                    if (discount > 0 && price.yang != it->second->GetPrice().yang)
                        PlayerBotLogThrottled("offline_markdown", now, "PLAYERBOT_OFFLINE: marked down pid=%u name=%s item=%u vnum=%u standing_min=%u discount=%d%% price=%lld",
                                ch->GetPlayerID(), ch->GetName(), it->first, preview->GetVnum(),
                                standing / 60000U, discount, (long long)price.yang);
                    M2_DELETE(preview);
                    if (price.yang > 0 && price.yang < GOLD_MAX && price.yang != it->second->GetPrice().yang &&
                            Begin(ch->GetPlayerID(), Edit, it->first, now)) {
                        manager.RecvShopEditItemClientPacket(ch, it->first, price);
                        EndCall(ch->GetPlayerID());
                    }
                }
            }
            // PLAYERBOT_OFFLINE_REPRICE_SLICE lines a slice, one visit and one
            // native ACK each. Complete slices alternate with a restock
            // opportunity; neither starves the other.
            if (wrapped) o.priceGeneration = GetPlayerBotPriceGeneration();
            if (--o.repriceSteps > 0 && !wrapped) {
                // The next step is a visit of its own, two seconds on, after
                // the ACK (the poll above holds it while one is pending). It
                // used to stay in the open visit instead: an ACK back before
                // the next tick let that tick ask for edit mode again while
                // the board was still open, ikashop refused it as busy
                // (BUSY_SHOP_MANAGE), the visit ended there and the slice was
                // finished a service interval later - so on m2zip nothing was
                // added to any counter for the first sixteen minutes after a
                // restart, against 169 adds on the build before.
                o.nextReprice = now;
                BotOfflineFinishVisit(ch, state, now);
                o.nextService = now + 2000;
                return false;
            }
            o.repriceSteps = 0;
            o.restockTurn = true; // maintenance cannot starve new goods either
            // Every step spent a mutation of the core's shared budget, so the
            // ten-minute pace is for a counter priced against an older
            // generation than this core runs - a yang rate moved in the panel -
            // and hourly otherwise (PLAYERBOT_OFFLINE_REPRICE_*). A restart is
            // not a change (the first visit stamps the counter, above): the
            // counters were priced by the same table a minute before, and
            // walking every one of them again at the fast pace took 284 of the
            // first fifteen minutes' 468 mutations on m2zip, when the first
            // visits of every keeper already queue for them. The stamp lives in
            // memory only, so a new price table - which arrives with a restart
            // and nothing else - reaches the counters at the hourly pace.
            o.nextReprice = now + (o.priceGeneration != GetPlayerBotPriceGeneration()
                    ? PLAYERBOT_OFFLINE_REPRICE_CATCHUP_MS : PLAYERBOT_OFFLINE_REPRICE_MS);
        }
        BotOfflineFinishVisit(ch, state, now);
        return false;
    }
}
#endif
#endif
