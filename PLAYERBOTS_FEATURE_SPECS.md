# 🚀 MASTER SPECIFICATION: METIN2 PLAYERBOTS EXPANSION
> Kompleksowa dokumentacja techniczna nowych systemów Playerbotów (inspirowanych badaniami z projektu Posłańca z MPCForum) dla silnika C++ r40250 oraz panelu Flask (admin_panel.py).

---

## 📑 SPIS TREŚCI
1. [Moduł 1: Bonus Intelligence, Map Memory & Dynamic Reroll AI](#moduł-1-bonus-intelligence-map-memory--dynamic-reroll-ai)
2. [Moduł 2: System Konia & Walka z Siodła (Mounted Combat)](#moduł-2-system-konia--walka-z-siodła-mounted-combat)
3. [Moduł 3: Autonomiczne Łowienie Ryb (Fishing AI)](#moduł-3-autonomiczne-łowienie-ryb-fishing-ai)
4. [Moduł 4: Gildie Botów, Loga Gildii & Relacje Społeczne](#moduł-4-gildie-botów-loga-gildii--relacje-społeczne)
5. [Moduł 5: Centralny AI Config (Suwaki wag Utility na żywo)](#moduł-5-centralny-ai-config-suwaki-wag-utility-na-żywo)
6. [Moduł 6: Sezon Tygodniowy & Rekordy Serwera (Web Analytics)](#moduł-6-sezon-tygodniowy--rekordy-serwera-web-analytics)
7. [Moduł 7: Inteligentny Dozorca (Safebox Storage AI)](#moduł-7-inteligentny-dozorca-safebox-storage-ai)

---

## Moduł 1: Bonus Intelligence, Map Memory & Dynamic Reroll AI

### 1. Cel
Boty nie powinny nosić pustych przedmiotów bez bonusów. System uczy się dominujących ras mobów na mapach i autonomicznie dodaje oraz miksuje bonusy (1–5) w przedmiotach, szukając kluczowych parametrów (NNO w tarczy, Średnie w broni 30 lv, PŻ w zbroi/biżuterii).

### 2. Map Race Memory (Wspólna wiedza o rasach)
W playerbot_manager.cpp:
struct TMapRaceKnowledge
{
    DWORD dwSamplesTotal;
    DWORD dwAnimalKills;
    DWORD dwOrcKills;
    DWORD dwMilgyoKills;
    DWORD dwUndeadKills;
    DWORD dwDevilKills;
    DWORD dwHumanKills;
};
static std::map<long, TMapRaceKnowledge> s_mapRaceMemory;

Przy zabiciu potwora (OnKill):
if (pkVictim && pkVictim->IsMonster())
{
    const long mapIdx = ch->GetMapIndex();
    TMapRaceKnowledge& mem = s_mapRaceMemory[mapIdx];
    mem.dwSamplesTotal++;

    if (pkVictim->IsRaceFlag(RACE_FLAG_ANIMAL))  mem.dwAnimalKills++;
    if (pkVictim->IsRaceFlag(RACE_FLAG_ORC))     mem.dwOrcKills++;
    if (pkVictim->IsRaceFlag(RACE_FLAG_MILGYO))  mem.dwMilgyoKills++;
    if (pkVictim->IsRaceFlag(RACE_FLAG_UNDEAD))  mem.dwUndeadKills++;
    if (pkVictim->IsRaceFlag(RACE_FLAG_DEVIL))   mem.dwDevilKills++;
    if (pkVictim->IsRaceFlag(RACE_FLAG_HUMAN))   mem.dwHumanKills++;
}

### 3. Autonomiczne bonowanie (Wzmocnienie 71085 i Zaczarowanie 71084)
- Wzmocnienie: Jeśli item->GetAttributeCount() < 5, bot dokłada kolejne bonusy aż do uzyskania 5 slotów.
- Kryteria zatrzymania miksowania (Keep Conditions):
  * Tarcze (ARMOR_SHIELD): Bezwzględny priorytet to APPLY_IMMUNE_STUN (NNO / Odporność na omdlenie).
  * Bronie 30/75 (FMS 290..299, RIB 3210..3219 itp.): Bezwzględny priorytet to APPLY_NORMAL_HIT_DAMAGE_BONUS >= 30% (Średnie Obrażenia).
  * Zbroje: Priorytet: APPLY_MAX_HP >= 1500 oraz APPLY_ATT_GRADE_BONUS lub Odporność na Strzały.
  * Biżuteria: Priorytet: APPLY_MAX_HP >= 1500 oraz APPLY_CRITICAL_PCT >= 5%.

### 4. Ogłoszenia ulepszania na czacie Wołaj (CHAT_TYPE_SHOUT)
Gdy bot pomyślnie ulepszy przedmiot na +7, +8 lub +9:
- Szansa np. 45%, globalny cooldown 2 minuty.
- Naturalne kwestie w stylu graczy Metin2 (np. Kowal wreszcie nie spalił, %s wskoczył na +7 :D, Siadło na +8! Jeszcze tylko jeden i max :D, MAM TO! %s na +9 wszedł! 🔥).
- Zero odzywek w trakcie walki PvP.

---

## Moduł 2: System Konia & Walka z Siodła (Mounted Combat)

### 1. Cel
Boty z koniem bojowym (11+ lvl) powinny bić Metiny i duże spoty potworów z siodła, zamiast natychmiast zsiadać z konia przy podejściu do celu.

### 2. Zmiana w UpdatePlayerBotTravelMount:
Obecnie PLAYERBOT_HORSE_DISMOUNT_DISTANCE wynosi 400 i zawsze zsiada. Wprowadzamy funkcję sprawdzającą:
bool CanPlayerBotFightOnHorse(LPCHARACTER ch, LPCHARACTER target)
{
    if (!ch || ch->GetHorseLevel() < 11)
        return false;

    LPITEM weapon = ch->GetWear(WEAR_WEAPON);
    if (!weapon || weapon->GetSubType() == WEAPON_BOW)
        return false;

    // Przeciwko Metinom bojowiec to priorytet #1
    if (target && target->IsStone())
        return true;

    // Wojownik i Sura biją spoty z konia
    if (ch->GetJob() == JOB_WARRIOR || ch->GetJob() == JOB_SURA)
        return true;

    return false;
}
Jeśli CanPlayerBotFightOnHorse zwraca true, bot nie zsiada z konia i zadaje obrażenia z siodła!

### 3. Karmienie konia:
W ManagePlayerBotHorse:
- Siano (50054) dla zwykłego konia (1–10).
- Marchewka (50055) dla bojowca (11–20).
- Czerwony Żeń-szeń (50056) dla militara (21).

---

## Moduł 3: Autonomiczne Łowienie Ryb (Fishing AI)

### 1. Cel
Boty w M1 (np. archetyp calm lub collector) kupują wędkę i schodzą nad brzeg rzeki przy Rybaku (NPC 9009), wprowadzając do obiegu rzadkie Małże i Perły.

### 2. Przedmioty i stałe:
- Rybak NPC: 9009
- Wędka: 27400 (slot WEAR_WEAPON)
- Przynęta (Robaki): 27801
- Ryby: Karaś (35901) itp.
- Drop z patroszenia: Ość (27990), Małż (27987)
- Drop z Małży: Biała Perła (27992), Niebieska Perła (27993), Krwawa Perła (27994)

### 3. Pętla łowienia:
1. Podejście do brzegu wody (sprawdzenie IsAttr(x, y, ATTR_WATER)).
2. Założenie przynęty i zarzucenie wędki (ch->fishing()).
3. Odczekanie 3–7 sekund na branie.
4. Wyciągnięcie (ch->fishing_take()).
5. Kliknięcie PPM na złowioną rybę (patroszenie) -> szansa na Małż -> otwarcie Małży -> zdobycie Perły!

---

## Moduł 4: Gildie Botów, Loga Gildii & Relacje Społeczne

### 1. Zakładanie gildii przez boty
- Bot na 40+ poziomie z 200k Yang zakłada gildię u Strażnika Wsi za pomocą natywnego CGuildManager::instance().CreateGuild(gcp).
- Nazwy gildii przypisane do królestw:
  * Chunjo: ChunjoElite, ZlotaGward
  * Shinsoo: CzerwSmoki, KrwawiRycerze
  * Jinno: NiebWilki, SrebrnaStal
- Zapraszanie innych botów: guild->RequestAddMember(targetBot, GUILD_GENERAL_GRADE).
- Automatyczne przypisanie herbów gildii z katalogu mark/.

### 2. Relacje (Przyjaciele vs Rywale)
W TPlayerBotAIState:
struct TPlayerBotSocialRelation
{
    DWORD dwPID;
    int iAffinity;   // Pomoc, wspólne PT, handel ulepszaczami
    int iHostility;  // Zabicie w PvP, podkradanie bossów/metinów
    DWORD dwLastInteractionTime;
};
Boty pamiętają swoich przyjaciół (pierwszeństwo do Party) oraz rywali/wrogów.

---

## Moduł 5: Centralny AI Config (Suwaki wag Utility na żywo)

### 1. Plik playerbot_weights.tsv
Silnik C++ czyta plik co 5 sekund bez rekompilacji (sprawdzając st_mtime pliku):
EXP	100
METIN	105
BOSS	100
TRADE	95
REFINE	100
BIOLOG	100
HORSE	110
QUEST	90
PARTY	100
RESTOCK	100
FISHING	80
PVP	50
Zakres: 25 (mocno ogranicz) do 250 (mocno promuj), 100 = neutralnie.

### 2. Panel Flask (admin_panel.py)
- Podstrona / modal AI Config z suwakami HTML5 dla każdego celu.
- Zapisywanie przez formularz POST bezpośrednio do pliku w kontenerze game.

---

## Moduł 6: Sezon Tygodniowy & Rekordy Serwera (Web Analytics)

### 1. Statystyki bota (w playerbot_status.tsv i w DB):
- dwTotalExp — zdobyty EXP
- dwMetinsDestroyed — zniszczone Metiny
- dwBossesKilled — ubite Bossy
- dwRefineSuccess — sukcesy u kowala na +7..+9
- dwPvPWins — wygrane pojedynki
- bHorseLevel — poziom konia

### 2. Wzór na wynik:
score = (exp // 100000) + (metins * 150) + (bosses * 500) + (refine_success * 200) + (pvp_wins * 300) + (horse_level * 100)

### 3. Prezentacja w panelu WWW:
- Sezon Tygodniowy: Rankingowa tabela botów z kolumnami: #, BOT, WYNIK, EXP, METINY, BOSSY, PVP, KOŃ, BIOLOG.
- Rekordy Serwera: Kafle: highest_level, most_metins, most_bosses, most_kills, highest_horse, most_refine_success.

---

## Moduł 7: Inteligentny Dozorca (Safebox Storage AI)

### 1. Cel
Zabezpieczenie przed zapychaniem ekwipunku cennymi ulepszaczami.

### 2. Implementacja w C++:
Wykorzystanie natywnego CSafebox (safebox.cpp):
bool DepositPlayerBotItemToSafebox(LPCHARACTER ch, LPITEM item)
{
    if (!ch || !item) return false;
    CSafebox* safebox = ch->GetSafebox();
    if (!safebox)
    {
        ch->LoadSafebox(SAFEBOX_PAGE_SIZE * 3, 0, 0);
        safebox = ch->GetSafebox();
        if (!safebox) return false;
    }

    for (BYTE cell = 0; cell < safebox->GetSize(); ++cell)
    {
        if (safebox->IsEmpty(cell, item->GetSize()))
        {
            ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
            item->RemoveFromCharacter();
            safebox->Add(cell, item);
            safebox->Save();
            return true;
        }
    }
    return false;
}
- Co trafia do Dozorcy: Zapasowe Kamienie Duszy +3/+4, Zwoje Błogosławieństwa, Zaczarowania Przedmiotu, Perły, Ości, Księgi Umiejętności.
