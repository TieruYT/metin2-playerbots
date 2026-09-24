#ifndef PLAYERBOT_PROGRESSION_NEEDS_H
#define PLAYERBOT_PROGRESSION_NEEDS_H
namespace {
    int CountPlayerBotOwnedSkillBooks(LPCHARACTER ch, DWORD skill) {
        int count = 0;
        for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell) {
            LPITEM item = ch->GetInventoryItem(cell);
            if (item && item->GetCell() == cell && item->GetType() == ITEM_SKILLBOOK &&
                    GetPlayerBotSkillBookSkillVnum(item) == skill)
                count += item->GetCount();
        }
        return count;
    }
    bool PlayerBotHasGrandMasterToTrain(LPCHARACTER ch) {
        if (!ch || !ch->GetSkillGroup()) return false;
        const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
        for (BYTE i = 0; i < build.bSkillCount; ++i) {
            DWORD skill = build.dwSkills[i];
            if (skill && ch->GetSkillMasterType(skill) == SKILL_GRAND_MASTER &&
                    ch->GetSkillLevel(skill) >= 30 && ch->GetSkillLevel(skill) < 40) return true;
        }
        return false;
    }
    bool PlayerBotNeedsTrainingRank(LPCHARACTER ch) {
        if (!PlayerBotHasGrandMasterToTrain(ch) || !ch->CountSpecifyItem(PLAYERBOT_GRAND_MASTER_STONE_VNUM)) return false;
        const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
        bool blocked = false;
        for (BYTE i = 0; i < build.bSkillCount; ++i) {
            DWORD skill = build.dwSkills[i];
            if (!skill || ch->GetSkillMasterType(skill) != SKILL_GRAND_MASTER) continue;
            const int level = ch->GetSkillLevel(skill);
            if (level < 30 || level >= 40) continue;
            if (ch->GetRealAlignment() >= 1000 + 500 * (level - 30)) return false;
            blocked = true;
        }
        return blocked;
    }
    // The open collection chain, independent of whether a hunting slot is free.
    // Herbs and quest keys stay on their own quest path, never a new market.
    int GetPlayerBotBiologistPurchaseNeed(LPCHARACTER ch, DWORD vnum) {
        if (!ch || IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID()))) return 0;
        for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i) {
            const TPlayerBotBiologistMission& m = PLAYERBOT_BIOLOGIST_MISSIONS[i];
            if (m.itemVnum != vnum || m.requiredLevel < PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL ||
                    ch->GetLevel() < m.requiredLevel || !IsPlayerBotBiologistMissionOpen(ch, i) ||
                    IsPlayerBotBiologistMissionComplete(ch, i) || IsPlayerBotBiologistKeyPhase(ch, i) ||
                    !IsPlayerBotHuntingMobHosted(m.mobVnum)) continue;
            return std::max(0, GetPlayerBotBiologistReserve(ch, vnum) - (int)ch->CountSpecifyItem(vnum));
        }
        return 0;
    }
    // Iwakura's Student (SUB-OSOBOWOSCI CHWILOWE): a book or a spirit stone a
    // bot drops it reads on the spot, but under the personality system it
    // buys one only once its big three - weapon, armour, shield - stand at +7
    // ("Grinder ... nigdy nie kupuje KU na rynku, gdyz priorytetowo odklada
    // Yang na ulepszenie sprzetu"). The keep limits below were already the
    // other half of his rule: a book for a skill at Master, a stone for one at
    // Grand Master, never one "na zapas".
    bool PlayerBotStudiesAtTheMarket(LPCHARACTER ch) {
        return !IsPlayerBotPersonaEnabled() || IsPlayerBotBigThreeAtPlus(ch, 7);
    }
    // Quantity needed, not a boolean reason to buy an arbitrarily large stack.
    int GetPlayerBotProgressionNeed(LPCHARACTER ch, LPITEM offer) {
        if (!ch || !offer || !ch->IsItemLoaded()) return 0;
        if (offer->GetType() == ITEM_SKILLBOOK) {
            DWORD skill = GetPlayerBotSkillBookSkillVnum(offer);
            if (!ch->GetSkillGroup() || !IsPlayerBotOwnSkill(ch, skill) ||
                    !(PlayerBotStudiesAtTheMarket(ch) || PlayerBotBuysBooksAsTrader(ch))) return 0;
            return std::max(0, GetPlayerBotBookKeepLimit(ch, skill) - CountPlayerBotOwnedSkillBooks(ch, skill));
        }
        if (offer->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
            return PlayerBotHasGrandMasterToTrain(ch) && PlayerBotStudiesAtTheMarket(ch)
                ? std::max(0, PLAYERBOT_GRAND_MASTER_STONE_KEEP - (int)ch->CountSpecifyItem(offer->GetVnum())) : 0;
        return GetPlayerBotBiologistPurchaseNeed(ch, offer->GetVnum());
    }
    bool IsPlayerBotProgressionOffer(LPCHARACTER ch, LPITEM offer) {
        return offer && offer->GetCount() > 0 &&
            (int)offer->GetCount() <= GetPlayerBotProgressionNeed(ch, offer);
    }
    // A skill of the bot's build at Master still short of its books: what the
    // mad scientist goes to the market for (playerbot_rare_persona.h).
    bool PlayerBotNeedsMasterBooks(LPCHARACTER ch) {
        if (!ch || !ch->IsItemLoaded() || !ch->GetSkillGroup()) return false;
        const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
        for (BYTE i = 0; i < build.bSkillCount; ++i) {
            DWORD skill = build.dwSkills[i];
            if (skill && ch->GetSkillMasterType(skill) == SKILL_MASTER &&
                    CountPlayerBotOwnedSkillBooks(ch, skill) < GetPlayerBotBookKeepLimit(ch, skill)) return true;
        }
        return false;
    }
    bool PlayerBotNeedsProgressionShopping(LPCHARACTER ch) {
        if (!ch || !ch->IsItemLoaded() || !ch->GetSkillGroup()) return false;
        const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
        const bool studies = PlayerBotStudiesAtTheMarket(ch);
        // Books are the trader's too (community patch 2, point 5).
        const bool studiesBooks = studies || PlayerBotBuysBooksAsTrader(ch);
        for (BYTE i = 0; studiesBooks && i < build.bSkillCount; ++i) {
            DWORD skill = build.dwSkills[i];
            if (skill && ch->GetSkillMasterType(skill) == SKILL_MASTER &&
                    CountPlayerBotOwnedSkillBooks(ch, skill) < GetPlayerBotBookKeepLimit(ch, skill)) return true;
        }
        if (studies && PlayerBotHasGrandMasterToTrain(ch) &&
                ch->CountSpecifyItem(PLAYERBOT_GRAND_MASTER_STONE_VNUM) < PLAYERBOT_GRAND_MASTER_STONE_KEEP) return true;
        for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
            if (GetPlayerBotBiologistPurchaseNeed(ch, PLAYERBOT_BIOLOGIST_MISSIONS[i].itemVnum) > 0) return true;
        return false;
    }

    bool PlayerBotProgressionSupplyExists(LPCHARACTER ch) {
        // The ledger is advisory; the actual offer is revalidated at purchase.
        const bool studies = PlayerBotStudiesAtTheMarket(ch);
        const TPlayerBotMarketLedgerEntry* books = GetPlayerBotMarketLedgerEntry(50300);
        if ((studies || PlayerBotBuysBooksAsTrader(ch)) && books && books->dwSupplyUnits > 0 && ch->GetSkillGroup()) {
            const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
            for (BYTE i = 0; i < build.bSkillCount; ++i) {
                DWORD skill = build.dwSkills[i];
                if (skill && ch->GetSkillMasterType(skill) == SKILL_MASTER &&
                        CountPlayerBotOwnedSkillBooks(ch, skill) < GetPlayerBotBookKeepLimit(ch, skill)) return true;
            }
        }
        const TPlayerBotMarketLedgerEntry* stones = GetPlayerBotMarketLedgerEntry(PLAYERBOT_GRAND_MASTER_STONE_VNUM);
        if (studies && stones && stones->dwSupplyUnits > 0 && PlayerBotHasGrandMasterToTrain(ch) &&
                ch->CountSpecifyItem(PLAYERBOT_GRAND_MASTER_STONE_VNUM) < PLAYERBOT_GRAND_MASTER_STONE_KEEP) return true;
        for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i) {
            const DWORD vnum = PLAYERBOT_BIOLOGIST_MISSIONS[i].itemVnum;
            const TPlayerBotMarketLedgerEntry* supply = GetPlayerBotMarketLedgerEntry(vnum);
            if (supply && supply->dwSupplyUnits > 0 && GetPlayerBotBiologistPurchaseNeed(ch, vnum) > 0) return true;
        }
        return false;
    }
    // Who is out on a trip now, pid -> when it ends. A map rather than a count,
    // so a bot that despawns mid-trip frees its place when the trip would have
    // ended instead of holding it for ever.
    std::map<DWORD, DWORD> s_mapPlayerBotProgressionTrip;

    size_t CountPlayerBotProgressionTrips(DWORD now) {
        for (std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotProgressionTrip.begin();
                it != s_mapPlayerBotProgressionTrip.end(); ) {
            if ((int)(now - it->second) >= 0) s_mapPlayerBotProgressionTrip.erase(it++);
            else ++it;
        }
        return s_mapPlayerBotProgressionTrip.size();
    }

    bool ShouldPlayerBotVisitProgressionMarket(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        // A dropper's time is its dungeon's (ManagePlayerBotShopping refuses
        // it anyway, so the trip would be ten minutes on the square for
        // nothing), and a bot in a player's party goes where the player goes.
        if (!ch || IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch) ||
                IsPlayerBotDropper(state.bPersonality) ||
                (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))) {
            if (state.dwProgressionTripUntil) {
                s_mapPlayerBotProgressionTrip.erase(ch ? ch->GetPlayerID() : 0);
                state.dwProgressionTripUntil = 0;
            }
            return false;
        }
        if (state.dwProgressionTripUntil && (int)(now - state.dwProgressionTripUntil) < 0) {
            if (PlayerBotNeedsProgressionShopping(ch)) return true;
            s_mapPlayerBotProgressionTrip.erase(ch->GetPlayerID());
            state.dwProgressionTripUntil = 0;
        }
        // Iwakura's Patch 3, point 7: the mad scientist goes to the market for
        // its books at once, past the share and the clocks - one trip, whose
        // end is the end of the state (playerbot_rare_persona.h).
        if (IsPlayerBotRareNow(state.persona, playerbot_persona::RARE_NAUKOWIEC, now)) {
            if (state.persona.bRareStage != 0 || !PlayerBotNeedsMasterBooks(ch)) return false;
            state.persona.bRareStage = 1;
            state.dwProgressionTripUntil = now + PLAYERBOT_PROGRESSION_TRIP_MS;
            s_mapPlayerBotProgressionTrip[ch->GetPlayerID()] = state.dwProgressionTripUntil;
            sys_log(0, "PLAYERBOT_MARKET: progression trip pid=%u name=%s map=%ld level=%d gold=%lld mad_scientist=1",
                ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), (int)ch->GetLevel(), (long long)ch->GetGold());
            return true;
        }
        if (!state.dwProgressionTripNext) {
            state.dwProgressionTripNext = now + PLAYERBOT_PROGRESSION_TRIP_FIRST_MIN_MS +
                PlayerBotNavHash(ch->GetPlayerID()) %
                (PLAYERBOT_PROGRESSION_TRIP_FIRST_MAX_MS - PLAYERBOT_PROGRESSION_TRIP_FIRST_MIN_MS);
            return false;
        }
        if ((int)(now - state.dwProgressionTripNext) < 0) return false;
        state.dwProgressionTripNext = now + PLAYERBOT_PROGRESSION_TRIP_RETRY_MIN_MS +
            PlayerBotNavHash(ch->GetPlayerID() ^ now) %
            (PLAYERBOT_PROGRESSION_TRIP_RETRY_MAX_MS - PLAYERBOT_PROGRESSION_TRIP_RETRY_MIN_MS);
        if ((long long)ch->GetGold() <= GetPlayerBotReservedGold(ch) + PLAYERBOT_SHOPPING_GOLD_FLOOR ||
                !PlayerBotNeedsProgressionShopping(ch) || !PlayerBotProgressionSupplyExists(ch)) return false;
        const size_t cap = std::max<size_t>(1,
            (size_t)GetPlayerBotsAlive() * PLAYERBOT_PROGRESSION_TRIP_PER_MILLE / 1000);
        if (CountPlayerBotProgressionTrips(now) >= cap) return false;
        state.dwProgressionTripUntil = now + PLAYERBOT_PROGRESSION_TRIP_MS;
        s_mapPlayerBotProgressionTrip[ch->GetPlayerID()] = state.dwProgressionTripUntil;
        sys_log(0, "PLAYERBOT_MARKET: progression trip pid=%u name=%s map=%ld level=%d gold=%lld trips=%u cap=%u",
            ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), (int)ch->GetLevel(),
            (long long)ch->GetGold(), (unsigned)s_mapPlayerBotProgressionTrip.size(), (unsigned)cap);
        return true;
    }
}
#endif
