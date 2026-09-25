#ifndef PLAYERBOT_OFFLINE_POLICY_H
#define PLAYERBOT_OFFLINE_POLICY_H
#include <cstdint>
#include <map>
#include <vector>

// Process-local request journal; no character/item pointers. Only the AI arms
// requests. Native DB handlers mark transmission and completion. Never retry a
// transmitted request on timeout: native Ikarus has no idempotency key.
namespace playerbot_offline {
enum Op { None, Create, Add, Edit, Remove, WithdrawItem, Buy };
struct Request {
    Op op = None;
    uint32_t item = 0, started = 0;
    bool sent = false, done = false, success = false, warned = false;
    uint32_t vnum = 0, count = 0, unitPrice = 0, skill = 0;
    uint8_t refine = 0;
};
inline std::map<uint32_t, Request> requests;
// A line that sold while its owner was off hunting. On this engine the goods
// on a counter belong to the shop entity, not to the bag, so the classic
// stall's way of noticing a sale - one pass over the owner's own inventory -
// cannot work here; and that whole branch of ManagePlayerBotShopLifetime is
// unreachable on 2.x anyway, which is why the fast-sale memory and the
// PLAYERBOT_STALL_SOLD history row have been dead since the offline shops
// arrived (diagnosed from the binary by AkhiGubernator, 13 September). The
// native manager is the one place that knows a line has gone, so it records
// the sale here and the owner's own tick drains it.
struct SoldLine {
    uint32_t item = 0, vnum = 0, count = 0;
    long long price = 0;
};
inline std::map<uint32_t, std::vector<SoldLine> > sold;
inline void NoteSold(uint32_t ownerid, uint32_t item, uint32_t vnum, uint32_t count, long long price) {
    if (!ownerid) return;
    auto& lines = sold[ownerid];
    // An owner nobody drains - a real player, or a bot that has left the
    // world - must not grow this without bound.
    if (lines.size() >= 32) lines.erase(lines.begin());
    lines.push_back(SoldLine{item, vnum, count, price});
}
// What the bot put on the counter and when, so the drain can say what sold
// and how fast. Keyed by item id, kept in that bot's own state.
struct ListedLine {
    uint32_t vnum = 0, skill = 0, when = 0;
    uint8_t refine = 0;
    uint32_t observedSince = 0;
};
inline bool Due(uint32_t now, uint32_t at) {
    return at == 0 || int32_t(now - at) >= 0;
}
inline bool Begin(uint32_t pid, Op op, uint32_t item, uint32_t now) {
    if (requests.count(pid)) return false;
    requests.emplace(pid, Request{op, item, now});
    return true;
}
inline void Sent(uint32_t pid, Op op, uint32_t item) {
    auto it = requests.find(pid);
    if (it != requests.end() && it->second.op == op && it->second.item == item)
        it->second.sent = true;
}
inline void Complete(uint32_t pid, Op op, uint32_t item, bool ok = true) {
    auto it = requests.find(pid);
    if (it != requests.end() && it->second.sent && it->second.op == op && it->second.item == item) {
        it->second.done = true;
        it->second.success = ok;
    }
}
inline bool EndCall(uint32_t pid) {
    auto it = requests.find(pid);
    if (it == requests.end()) return false;
    if (it->second.sent) return true;
    requests.erase(it); // synchronous refusal, no DB mutation submitted
    return false;
}
struct State {
    uint32_t nextService = 0, visitUntil = 0, nextStep = 0;
    uint32_t nextReprice = 0, repriceItem = 0;
    uint32_t nextBrowse = 0, buyOwner = 0, buyItem = 0, buyUntil = 0;
    uint32_t observedShop = 0;
    uint32_t browseOwner = 0, browseItem = 0;
    // The first village's stands, read from a second village before the
    // walk over: a cursor of their own, so the look at the far market does
    // not lose the place of the browse at the near one, and the line the
    // walk is for, which becomes the buyer's pick on arrival.
    uint32_t farBrowseOwner = 0, farBrowseItem = 0;
    uint32_t farPickOwner = 0, farPickItem = 0;
    // The buyer's pick is the one a walk over was made for, from the hand-over
    // to the purchase or the moment it is given up, which says why.
    bool farBuy = false;
    uint32_t repriceSteps = 0;
    // Which compiled price table this shop was last priced against.
    uint32_t priceGeneration = 0;
    // The empty-hand probe of the counter, and the last line taken back to
    // wear with when, so a piece the bot will not put on is not taken back
    // and listed again every visit.
    uint32_t nextReclaimProbe = 0, lastReclaimItem = 0, lastReclaimAt = 0;
    // The line cut out of its stack before the shop board opened, for the
    // add of the same visit (BotOfflinePrepareVisitLine): item id and cell.
    uint32_t preparedItem = 0, preparedCell = 0;
    // When the keeper last stood at its shop and served it: a shop on
    // another map waits PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS from here.
    uint32_t lastServedAt = 0;
    // When the visit under way began, and how many visits in a row ended
    // before the keeper stood at its counter (BotOfflineInterruptVisit).
    uint32_t visitStarted = 0;
    uint32_t interrupted = 0;
    std::map<uint32_t, ListedLine> listed;
    bool visiting = false;
    bool restockTurn = false;
};
    template<class Shops, class Visitor>
    unsigned BrowseLines(const Shops& shops, State& state, unsigned limit, Visitor visit) {
        unsigned checked = 0;
        size_t start = 0;
        for (size_t i = 0; i < shops.size(); ++i)
            if (shops[i].second->GetOwnerPID() == state.browseOwner) { start = i; break; }
        for (size_t n = 0; n < shops.size() && checked < limit; ++n) {
            auto shop = shops[(start+n) % shops.size()].second;
            const auto& items = shop->GetItems();
            auto it = n == 0 && shop->GetOwnerPID() == state.browseOwner
                ? items.upper_bound(state.browseItem) : items.begin();
            for (; it != items.end() && checked < limit; ++it) {
                ++checked;
                state.browseOwner = shop->GetOwnerPID();
                state.browseItem = it->first;
                visit(shop, it->first, it->second);
            }
            if (it == items.end()) {
                state.browseOwner = shops[(start+n+1) % shops.size()].second->GetOwnerPID();
                state.browseItem = 0;
            }
        }
        return checked;
    }
inline bool Fits(int cell, int height, int width, int cells) {
    return width > 0 && height > 0 && cell >= 0 && cell < cells &&
        height <= cells / width && cell + (height - 1) * width < cells;
}
inline int64_t Affordable(int64_t wallet, int64_t reserve, int64_t floor) {
    return wallet > reserve + floor ? wallet - reserve - floor : 0;
}
}
#endif
