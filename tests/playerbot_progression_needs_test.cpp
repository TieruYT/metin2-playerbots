#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
#include <cstdint>
using DWORD=uint32_t; using WORD=uint16_t; using BYTE=uint8_t;
enum {ITEM_SKILLBOOK=1, SKILL_MASTER=1, SKILL_GRAND_MASTER=2,
    PLAYERBOT_BAG_CELLS=90, PLAYERBOT_GRAND_MASTER_STONE_VNUM=50513,
    PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL=30, PLAYERBOT_SHOPPING_GOLD_FLOOR=100,
    PLAYERBOT_GRAND_MASTER_STONE_KEEP=3, PLAYERBOT_PROGRESSION_TRIP_PER_MILLE=30,
    PLAYERBOT_PROGRESSION_TRIP_MS=600000, PLAYERBOT_PROGRESSION_TRIP_FIRST_MIN_MS=60000,
    PLAYERBOT_PROGRESSION_TRIP_FIRST_MAX_MS=1800000, PLAYERBOT_PROGRESSION_TRIP_RETRY_MIN_MS=1800000,
    PLAYERBOT_PROGRESSION_TRIP_RETRY_MAX_MS=2700000};
void sys_log(int, const char*, ...) {}
struct Item {
    int cell=0, type=ITEM_SKILLBOOK, count=1; DWORD vnum=50300, skill=1;
    int GetCell(){return cell;} int GetType(){return type;} int GetCount(){return count;}
    DWORD GetVnum(){return vnum;}
};
using LPITEM=Item*;
struct Character {
    bool loaded=true; int group=1, level=75, gold=10000, alignment=0; DWORD pid=42;
    std::map<int,Item*> bag; std::map<DWORD,int> mastery, quantities;
    LPITEM GetInventoryItem(int cell){auto it=bag.find(cell);return it==bag.end()?nullptr:it->second;}
    int GetSkillGroup(){return group;} int GetJob(){return 0;} DWORD GetPlayerID(){return pid;}
    const char* GetName(){return "bot";} void* GetParty(){return nullptr;} long GetMapIndex(){return 21;}
    int GetSkillMasterType(DWORD skill){return mastery[skill];}
    int GetSkillLevel(DWORD skill){return mastery[skill]==SKILL_GRAND_MASTER?30:20;}
    int GetRealAlignment(){return alignment;} int GetLevel(){return level;} int GetGold(){return gold;}
    bool IsItemLoaded(){return loaded;} int CountSpecifyItem(DWORD v){return quantities[v];}
};
using LPCHARACTER=Character*;
struct TJobSkillBuild { BYTE bSkillCount=2; DWORD dwSkills[2]={1,2}; };
TJobSkillBuild GetPlayerBotSkillBuild(int,int,DWORD){return {};}
DWORD GetPlayerBotSkillBookSkillVnum(LPITEM i){return i->skill;}
bool IsPlayerBotOwnSkill(LPCHARACTER,DWORD s){return s==1||s==2;}
int GetPlayerBotBookKeepLimit(LPCHARACTER c,DWORD s){return c->mastery[s]>=2?0:12;}
bool dropper=false, complete=false, open=true, key=false, hosted=true, horse=false;
int GetPlayerBotPersonalityByPID(DWORD){return 0;} bool IsPlayerBotDropper(int){return dropper;}
struct TPlayerBotBiologistMission{DWORD itemVnum; int requiredLevel; DWORD mobVnum;};
const TPlayerBotBiologistMission PLAYERBOT_BIOLOGIST_MISSIONS[]={{50701,4,173},{30006,30,601}};
const int PLAYERBOT_BIOLOGIST_MISSION_COUNT=2;
bool IsPlayerBotBiologistMissionOpen(LPCHARACTER,size_t){return open;}
bool IsPlayerBotBiologistMissionComplete(LPCHARACTER,size_t){return complete;}
bool IsPlayerBotBiologistKeyPhase(LPCHARACTER,size_t){return key;}
bool IsPlayerBotHuntingMobHosted(DWORD){return hosted;}
int GetPlayerBotBiologistReserve(LPCHARACTER,DWORD){return 17;}
struct TPlayerBotMarketLedgerEntry{int dwSupplyUnits=0;};
std::map<DWORD,TPlayerBotMarketLedgerEntry> testSupply;
const TPlayerBotMarketLedgerEntry* GetPlayerBotMarketLedgerEntry(DWORD v){return &testSupply[v];}
// Iwakura's Patch 3, point 7: the mad scientist's trip asks its rare state
// (playerbot_types.h, playerbot_persona_rules.h).
namespace playerbot_persona { enum { RARE_NONE = 0, RARE_NAUKOWIEC = 3 }; }
struct TPlayerBotPersona{BYTE bRare=0, bRareStage=0;};
bool rareNow=false;
bool IsPlayerBotRareNow(const TPlayerBotPersona&, BYTE, DWORD){return rareNow;}
struct TPlayerBotAIState{DWORD dwProgressionTripNext=0,dwProgressionTripUntil=0; BYTE bPersonality=0; TPlayerBotPersona persona;};
bool IsPlayerBotHumanLedParty(void*){return false;}
int alive=1000; int GetPlayerBotsAlive(){return alive;}
bool IsPlayerBotOnBattleHorseTrial(LPCHARACTER){return horse;}
bool IsPlayerBotOnMilitaryHorseTrial(LPCHARACTER){return false;}
DWORD PlayerBotNavHash(DWORD v){return v;}
int GetPlayerBotReservedGold(LPCHARACTER){return 500;}
// Iwakura's Student buys at the market only once the big three stand at +7,
// and only while his personalities are switched on.
bool personaOn=false; bool IsPlayerBotPersonaEnabled(){return personaOn;}
bool bigThree=false; bool IsPlayerBotBigThreeAtPlus(LPCHARACTER,int){return bigThree;}
// Community patch 2, point 5: the Trader in town buys its own books whatever
// its gear stands at, from its book purse.
bool trader=false; bool PlayerBotBuysBooksAsTrader(LPCHARACTER){return trader;}
#include "../linux-port/overlays/playerbot/src/game/src/playerbot_progression_needs.h"
int main(){
    Character c; c.mastery[1]=SKILL_MASTER;
    Item owned; owned.cell=80; owned.count=7; c.bag[80]=&owned;
    // A multi-cell alias must not count twice; seller/preview cell is irrelevant.
    c.bag[81]=&owned;
    Item offer; offer.cell=0; offer.count=5;
    assert(CountPlayerBotOwnedSkillBooks(&c,1)==7);
    assert(IsPlayerBotProgressionOffer(&c,&offer));
    offer.cell=85; assert(IsPlayerBotProgressionOffer(&c,&offer));
    offer.count=6; assert(!IsPlayerBotProgressionOffer(&c,&offer));
    offer.skill=99; assert(!IsPlayerBotProgressionOffer(&c,&offer));
    offer.skill=1; c.mastery[1]=SKILL_GRAND_MASTER;
    assert(!IsPlayerBotProgressionOffer(&c,&offer));
    Item stone; stone.type=0; stone.vnum=50513; stone.count=3;
    assert(IsPlayerBotProgressionOffer(&c,&stone));
    c.quantities[50513]=1; assert(!IsPlayerBotProgressionOffer(&c,&stone));
    stone.count=2; assert(IsPlayerBotProgressionOffer(&c,&stone));
    assert(PlayerBotNeedsTrainingRank(&c));
    c.alignment=1000; assert(!PlayerBotNeedsTrainingRank(&c));
    c.alignment=0;
    c.mastery[1]=3; assert(!IsPlayerBotProgressionOffer(&c,&stone));
    Item tooth; tooth.type=0; tooth.vnum=30006; tooth.count=10;
    c.quantities[30006]=7; assert(IsPlayerBotProgressionOffer(&c,&tooth));
    open=false; assert(!IsPlayerBotProgressionOffer(&c,&tooth)); open=true;
    key=true; assert(!IsPlayerBotProgressionOffer(&c,&tooth)); key=false;
    complete=true; assert(!IsPlayerBotProgressionOffer(&c,&tooth)); complete=false;
    dropper=true; assert(!IsPlayerBotProgressionOffer(&c,&tooth)); dropper=false;
    hosted=false; assert(!IsPlayerBotProgressionOffer(&c,&tooth)); hosted=true;
    tooth.vnum=50701; assert(!IsPlayerBotProgressionOffer(&c,&tooth));
    c.mastery[1]=SKILL_MASTER; testSupply[50300].dwSupplyUnits=1;
    TPlayerBotAIState s;
    assert(!ShouldPlayerBotVisitProgressionMarket(&c,s,1000)); // stagger startup
    DWORD start=s.dwProgressionTripNext;
    assert(ShouldPlayerBotVisitProgressionMarket(&c,s,start));
    assert(ShouldPlayerBotVisitProgressionMarket(&c,s,start+100));
    horse=true; assert(!ShouldPlayerBotVisitProgressionMarket(&c,s,start+200)); horse=false;
    assert(!ShouldPlayerBotVisitProgressionMarket(&c,s,start+600001));
    c.gold=100; assert(!ShouldPlayerBotVisitProgressionMarket(&c,s,s.dwProgressionTripNext));
    c.gold=10000;
    // A dropper never goes, and a trip it was on ends.
    TPlayerBotAIState d; dropper=true;
    ShouldPlayerBotVisitProgressionMarket(&c,d,5000);
    assert(!ShouldPlayerBotVisitProgressionMarket(&c,d,d.dwProgressionTripNext));
    assert(d.dwProgressionTripUntil==0); dropper=false;
    // The share: 30 per mille of 40 bots is under one, so one place, and a
    // second bot waits while the first is out; the place frees on expiry.
    alive=40; s_mapPlayerBotProgressionTrip.clear();
    Character c2=c; c2.pid=43; TPlayerBotAIState s1, s2;
    ShouldPlayerBotVisitProgressionMarket(&c,s1,1000); ShouldPlayerBotVisitProgressionMarket(&c2,s2,1000);
    const DWORD t=std::max(s1.dwProgressionTripNext,s2.dwProgressionTripNext);
    s1.dwProgressionTripNext=t; s2.dwProgressionTripNext=t;
    assert(ShouldPlayerBotVisitProgressionMarket(&c,s1,t));
    assert(!ShouldPlayerBotVisitProgressionMarket(&c2,s2,t));
    assert(CountPlayerBotProgressionTrips(t+PLAYERBOT_PROGRESSION_TRIP_MS)==0);

    // The Student's gate: with the personalities on, nothing is bought at the
    // market until the weapon, the armour and the shield stand at +7.
    alive=1000; s_mapPlayerBotProgressionTrip.clear();
    Item book; book.type=ITEM_SKILLBOOK; book.skill=1; book.vnum=50300; book.count=1;
    assert(GetPlayerBotProgressionNeed(&c,&book)>0);
    personaOn=true; bigThree=false;
    assert(GetPlayerBotProgressionNeed(&c,&book)==0);
    bigThree=true;
    assert(GetPlayerBotProgressionNeed(&c,&book)>0);
    // Community patch 2, point 5: the Trader buys books before the big three
    // stand at +7 - and only books: the spirit stone keeps the Student's gate.
    bigThree=false; trader=true;
    assert(GetPlayerBotProgressionNeed(&c,&book)>0);
    c.mastery[1]=SKILL_GRAND_MASTER; c.quantities[50513]=0;
    Item stone2; stone2.type=0; stone2.vnum=50513; stone2.count=1;
    assert(GetPlayerBotProgressionNeed(&c,&stone2)==0);
    c.mastery[1]=SKILL_MASTER; trader=false;
    personaOn=false;

    // The mad scientist goes at once, past the stagger and the share, once:
    // the stage it sets is what keeps the state from a second trip.
    s_mapPlayerBotProgressionTrip.clear(); alive=40; rareNow=true;
    TPlayerBotAIState m;
    assert(PlayerBotNeedsMasterBooks(&c));
    assert(ShouldPlayerBotVisitProgressionMarket(&c,m,1000));
    assert(m.persona.bRareStage==1 && m.dwProgressionTripUntil==1000+PLAYERBOT_PROGRESSION_TRIP_MS);
    assert(!ShouldPlayerBotVisitProgressionMarket(&c,m,1000+PLAYERBOT_PROGRESSION_TRIP_MS+1));
    // With its books in hand it has nothing to go for.
    TPlayerBotAIState m2; owned.count=12;
    assert(!PlayerBotNeedsMasterBooks(&c));
    assert(!ShouldPlayerBotVisitProgressionMarket(&c,m2,1000) && m2.persona.bRareStage==0);
    owned.count=7; rareNow=false; alive=1000;
    std::printf("playerbot_progression_needs: all tests passed\n");
}
