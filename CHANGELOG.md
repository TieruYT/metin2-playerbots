# Changelog

Every release of this project, newest first. The admin panel reads this file to
show you what an update would bring before you install it.

Versions are `MAJOR.MINOR.PATCH`:

- **PATCH** — a fix. Nothing you do changes.
- **MINOR** — something new, or something behaves better than it did. Safe to
  take; nothing you have set up stops working.
- **MAJOR** — something you have to act on. A setting that has to move, a
  command that no longer exists, a manual step during the update. These say so
  at the top, in full, before anything else.

Updating never touches your database. Characters, accounts and settings survive
every version here.

---

## 1.24.9 — 2026-09-05

### Zmienione

- **Stragany wystawiają to, co gracz faktycznie chce kupić**, w tej kolejności:
  - **Broń na 30. poziom** — przy dowolnym ulepszeniu, także bez żadnego. To przedmiot, po który boty jeżdżą przez pół świata.
  - **Przedmioty z dużym bonusem** — od +1000 PŻ wzwyż, oraz tarcze z blokiem lub odbiciem ciosu, bo tarczę kupuje się dla tego, a nie dla liczby obrony.
  - **Zapasowy sprzęt od +6** zamiast dotychczasowego +7. To jednocześnie nowa granica, poniżej której nic nie trafia do handlarza NPC za jedną piątą wartości.
  - **Ulepszacze** — mają zarezerwowaną **połowę lady**, bo sama wycena je z niej wypychała.
  - **Księgi umiejętności** — żaden bot ich nie czyta, więc to czysty towar.
- **Medale konne trafiają na sprzedaż** u handlarzy oraz u botów, których koń i tak czeka na wyższy poziom postaci. Wcześniej były wykluczone całkowicie, więc **nikt nie mógł ich kupić**.
- **Kupujący chcą tego samego.** Medalu, jeśli mają jeszcze konia do wychowania — to godziny lochu, których nie muszą biegać. Broni lv 30 swojej klasy, gdy żadnej nie mają. Oraz przedmiotu z dużym bonusem nawet wtedy, gdy baza wypada na równi z noszonym, bo tysiąc życia nie wchodzi do wyceny sprzętu.

### Naprawione

- **Plac handlowy zwężony do zasięgu, w którym gra pozwala kupować.** `shop_manager.cpp` odrzuca zakup powyżej 2000 jednostek, czyli 20 metrów. Rozstawienie lad na 40 metrów sprawiło, że rynek wyglądał przestronnie i przestał działać: przy dziesięciu straganach i 89 zamożnych botach w mieście nie doszło do **ani jednej** transakcji przez piętnaście minut. Rozrzut wynosi teraz 4–17 metrów — nadal siedem razy więcej niż na początku, ale każda lada jest osiągalna. Tego sufitu nie da się podnieść po naszej stronie.

---

## 1.24.8 — 2026-09-05

### Naprawione

- **Stragany wreszcie powstają w Joan.** Losowanie 90/10 działało poprawnie i było bez znaczenia, bo w ścieżce straganu został twardy warunek z czasów, gdy rynek istniał tylko w Bokjung — i stał **przed** losowaniem. Bot stojący w Joan odpadał, zanim zdążył wybrać miasto. Sprawdzone na uruchomionym świecie: stragany stają teraz w obu miastach, bez ani jednej odmowy.

### Zmienione

- **Plac w Joan przeniesiony pod Strażnika Wsi** na komórkę (634, 639), a stragany rozstawiają się w promieniu **8–40 metrów** zamiast dotychczasowych dwóch i pół. Między ladami da się przejść, a każdy bot ma stały własny punkt, więc rynek nie przestawia się przy każdym otwarciu.
- **Nowa osobowość: Handlarz.** Mniej więcej co szósty bot bez innego powołania. Handlarz trzyma stragan zawsze, gdy może, a nie raz na dziesięć razy; wystawia **20 pozycji** zamiast ośmiu; pracuje na zmianę 10–20 minut; i **nie przerywa handlu, żeby biec po medale konne** — ta wyprawa zabiera pół świata i godzinę, czyli dokładnie to gonienie za czymś, czego ta osobowość ma nie robić. W panelu widnieje jako **Handlarz** z ambicją **Handel**.

### Warto wiedzieć

- **Ambicja bota jest wyłącznie etykietą.** Trafia do pliku statusu i na stronę, ale żaden kod nigdy się na niej nie rozgałęział — dotyczy to także ambicji, które istniały wcześniej. Zachowanie Handlarza wynika z osobowości, nie z ambicji.

---

## 1.24.7 — 2026-09-05

### Naprawione

- **Boty kończą uderzenia.** Rytm ataku był odmierzany płaskimi 480 ms, skalowanymi tylko szybkością ataku. Klient przyjmuje kolejne uderzenie combo dopiero od `DirectInputTime`, a ta wartość jest **późniejsza niż 480 ms dla każdej broni w grze**: 533 ms miecz jednoręczny, 732 szable, 932 dwuręczny, cała sekunda łuk. Trwająca animacja była więc za każdym razem ucinana.
  - **Ostatnie uderzenie było ucinane najmocniej.** Krok combo, który ma `DirectInputTime` równe zero, w ogóle się nie łączy — kończy sekwencję i klient odgrywa go w całości. Czwarte combo dzwonka jest właśnie takie i potrzebuje 1333 ms, a dostawało niecałe 400.
  - Czasy pochodzą z danych animacji, które serwer i tak wozi w `share/data/pc`, i są generowane przez `tools/generate_swing_timing.py` — 4 klasy × 6 rodzajów broni × 4 kroki combo. Dla łuku, który nie ma łańcucha combo, brana jest pełna długość animacji.
  - **Boty przez to zwolniły, celowo.** Biły mniej więcej dwa razy szybciej, niż pozwala animacja, a obrażenia idą razem z pakietem — więc tempo zdobywania poziomów spadnie proporcjonalnie. To do wyrównania osobnym mnożnikiem, nie oszukiwaniem animacji.
- **Ranking umiejętności pokazuje najwyższą rangę, nie sumę poziomów.** Sumowanie premiowało rozłożenie punktów po całym drzewku. Teraz widać to, co powiedziałby gracz: `M10 Silne Ciało`, `G1 Przywołanie Błyskawicy`, `P Berserk` — i po tym idzie sortowanie.

### Zmienione

- **Stragany mają wreszcie towar.** Bot wystawiał dokładnie jeden przedmiot; teraz na ladzie stoi do **ośmiu**, najlepsze na przedzie, więc ulepszacze i zapasowy ulepszony sprzęt idą pierwsze. Kupujący przechodzi całą ladę i bierze pozycję, która go interesuje, a nie zawsze pierwszą.
- **Stragan można wystawić od 1. poziomu.** Wymóg 20. poziomu był nasz, nie gry — tobołek (przedmiot 50200) ma w tabelach `LIMIT_NONE`. To dlatego w Joan nie było żadnego straganu: z pięciuset botów stojących tam pięć miało dwudziestkę, podczas gdy w Bokjung było ich sto dwadzieścia.
- **Wyprzedany stragan się zwija** i bot wraca do gry, zamiast stać przy pustej ladzie do końca licznika.

---

## 1.24.6 — 2026-09-04

### Naprawione

- **Boty przestały rzucać buffy bojowe poza walką.** Wersja 1.24.2 zdjęła wymóg posiadania celu, bo boty wchodziły do każdej walki bez niczego — ale poszła za daleko w drugą stronę: stały w mieście, rzucając aurę, silne ciało czy czarowane ostrze, które i tak wygasały, zanim dotarły do potworów kilometr dalej. Teraz buff bojowy wchodzi tylko wtedy, gdy bot jest w walce albo był w niej na tyle niedawno, że zaraz będzie w następnej (okno 60 sekund — dłuższe niż przerwa na dobiegnięcie do kolejnego potwora i podniesienie łupu, krótsze niż droga do miasta).
- **Umiejętności przydatne w drodze działają bez zmian, wszędzie.** To **Bezszelestny Chód** ninja i **Zwinność** szamana — obie przyspieszają ruch, a chodzenie jest tym, co bot robi najczęściej.
- **Leczenie szamana też zostało poza tą blokadą.** To nie buff, tylko leczenie zależne od własnego zdrowia, a ranny bot wracający do miasta ma więcej powodów, żeby je rzucić, niż ten w walce.

---

## 1.24.5 — 2026-09-04

### Dodane

- **Trzy nowe zakładki w rankingach na stronie**, obok Konia i Biologa:
  - **Otwarte sklepy** — kto właśnie prowadzi stragan i w którym mieście.
  - **Umiejętności** — suma wyuczonych poziomów umiejętności.
  - **Przedmiot +9** — boty posiadające dowolny sprzęt na +9, z ikoną i oznaczeniem, czy nosi go na sobie, czy trzyma w plecaku.
- **Boty raportują osobną akcję „Prowadzi stragan".** Dotąd sklepikarz zgłaszał tę samą akcję co bot w drodze, więc z zewnątrz nie dało się odróżnić handlującego od przechodzącego — a otwarty stragan istnieje wyłącznie w pamięci silnika i baza nic o nim nie wie. Panel czyta teraz tę akcję z pliku statusu i na niej opiera zakładkę.

### Uwagi

- Zakładka **Przedmiot +9** będzie pusta, dopóki któryś bot nie dobije do +9. W chwili wydania wszystkie przedmioty +9 w świecie należały do postaci gracza, nie do botów; botom udało się dojść do +8.

---

## 1.24.4 — 2026-09-04

### Zmienione

- **Stragany stoją teraz w kole.** W Joan wokół **Strażnika Wsi**, w Bokjung na dotychczasowym placu targowym. Każdy bot ma stały kąt na okręgu i wraca zawsze na to samo miejsce, więc krąg nie rozłazi się przy kolejnych otwarciach.
  - Pozycja strażnika nie jest zgadnięta: to NPC 11002 z komórki (633,640) mapy `metin2_map_b1`, której `BasePosition` wynosi (0,102400) — czyli świat (63300,166400). Ten sam rachunek daje istniejącej stałej `PLAYERBOT_M1_TELEPORTER` jej (51900,153600) z komórki (519,512), więc jest sprawdzony. Wokół strażnika nie stoi żaden inny NPC w promieniu czterdziestu komórek.
- **Przy każdym wystawieniu bot losuje miasto: 90% szans na Joan, 10% na Bokjung.** Losowanie jest za każdym razem od nowa, nie przypisane botowi na stałe. Nic nie jest zapamiętywane — bot, który wylosuje miasto, w którym akurat nie stoi, po prostu tym razem nie wystawia i losuje ponownie przy następnej próbie, więc nikt nie utknie w oczekiwaniu na miasto, do którego rzadko zagląda.
- **Kupujący szukają straganów w obu miastach.** Wcześniej przeglądali wyłącznie Bokjung — po przeniesieniu większości sklepów do Joan dziewięć straganów na dziesięć zostałoby bez klientów.

---

## 1.24.3 — 2026-09-04

### Naprawione

- **Prywatne stragany wreszcie działają — botów i graczy.** Silnik zaczyna `OpenMyShop` od warunku `GetPart(PART_MAIN) > 2`, a `PART_MAIN` przechowuje vnum **założonej zbroi**. Niezałatany silnik odmawiał więc straganu każdemu, kto ma na sobie zbroję — również żywemu graczowi, który próbował wystawić sklep sam.
  - Łatka `0004-private-shop-guard.patch`, która zamienia ten warunek na `IsPolymorphed()`, istnieje od dawna, jest w paczce aktualizacji i leży w każdej instalacji — ale nakłada ją `prepare-context.sh`, którego ścieżka przebudowy w launcherze **nigdy nie uruchamiała**. Łatka była dowożona i nigdy nakładana.
  - Launcher nakłada teraz łatki silnika przy każdej przebudowie, **odkrywając** katalog zamiast trzymać listę nazw. To trzeci błąd tej samej klasy po seedzie i wildcardzie w Makefile.
  - O tym, czy łatka już jest, decyduje **odczyt plików docelowych**, a nie kod wyjścia `patch`. Podczas pracy nad tym `patch -N` z busyboxa nałożył 18-kilobajtową łatkę integracji na drzewo, które już ją miało, i zdublował w niej wszystkie deklaracje. Porównywany jest cały blok hunka wraz z kontekstem — pojedyncza dodana linia nie wystarcza, bo `if (IsPolymorphed())` występuje w `char.cpp` w trzech innych miejscach.
  - Gdy nałożenie się nie powiedzie, launcher **mówi o tym wprost** zamiast po cichu zgłosić sukces.
- **Boty ze straganem nie rzucają już buffów.** Rzucanie skilla nie zamyka prywatnego sklepu — silnik zamyka go tylko przy ogłuszeniu, śmierci i wyjściu ze świata — ale sklepikarz machający aurą przy ladzie marnował manę i wyglądał niepoważnie.
- **Rynek w Bokjung jest wyłączony ze strefy buffowania.** Ten warunek obejmował dotąd wyłącznie mapę 21, a rynek jest na mapie 23.

---

## 1.24.2 — 2026-09-04

### Podziękowania

- **OskarPWA** udostępnił swoją wersję panelu i zgodził się, żeby wziąć z niej dwie rzeczy: **okno magazynu bota** oraz **ikony przy umiejętnościach**. Jedno i drugie jest w tym wydaniu. Dzięki!

### Dodane

- **Magazyn bota na stronie.** Obok ekwipunku doszła ikona magazynu. Okno jest niezależne od karty bota: można je przeciągać po stronie i zostaje otwarte, gdy zamkniesz i otworzysz kartę. Siatka 5×9 jest tą samą, którą rysuje ekwipunek.
  - **Uwaga na teraz:** boty jeszcze nie korzystają z magazynu — w kodzie AI nie ma ani jednego odwołania do skrytki — więc okno będzie pokazywać „Magazyn jest pusty", dopóki nie dołożymy botom samego zachowania. Sam widok działa.
- **Ikony przy umiejętnościach.** Każda umiejętność na karcie bota ma teraz swoją ikonę, z osobnym wariantem dla mistrzowskich (M/G/P). Gdy ikony nie ma w danej instalacji, nazwa wyświetla się sama, bez zepsutego obrazka.

- **Nazwy bonusów na przedmiotach są teraz takie jak w grze.** Panel miał własną, ręcznie pisaną listę; teraz opisy pochodzą wprost z klienta — numery z enuma serwera, przypisanie z `AFFECT_DICT` klienta, brzmienie z jego polskiego `locale_game.txt`. Nic nie jest tłumaczone na piechotę.
  - **`71` i `72` były zamienione miejscami.** `71` to Obrażenie Umiejętności, `72` to Średnie Obrażenia — istotne przy FMS/RIB.
  - **Brakowało bonusów `87`–`91`**, więc odporność na lód, ziemię i mrok oraz odporności na cios krytyczny i przeszywający pokazywały się jako „Bonus #89" i podobne.
  - **`42` i `47`** (szansa na odzyskanie PE/PŻ po zabiciu) były pokazywane jako zwykłe liczby zamiast procentów, a **`48`–`50`** dostawały doklejone „+1%", choć w grze to same nazwy bez wartości.
  - **`51` i `77` zniknęły z opisów.** `51` to spakowana liczba (numer umiejętności plus wartość), a nie bonus do pokazania; `77` nie występuje w tym kliencie.
  - Wartość ujemna nie da już zapisu `+-10`.

### Naprawione

- **Aktualizacja przestała kończyć się błędem Dockera.** Zatrzymanie serwera wyłącza także Docker Desktop, a aktualizacja szła prosto do budowania i trafiała na martwy silnik — już po podmianie plików. Rozsądna kolejność (zatrzymaj, potem zaktualizuj) zawodziła zawsze. Teraz silnik jest podnoszony przed budowaniem. To samo dotyczyło przycisku GRAJ, który dokańczał zaległe budowanie, zanim cokolwiek zdążyło uruchomić Dockera.
- **Launcher przestał zgłaszać wersję, której nie ma.** Numer zapisywał się dopiero po udanym budowaniu, więc po nieudanym launcher w kolejnych uruchomieniach wciąż podawał poprzednią wersję, proponował tę samą aktualizację i pobierał ją od nowa — za każdym razem z nowym katalogiem w `backups`.
- **Teleportacja do bota działa dla każdego.** Przycisk wysyłał na sztywno wpisaną nazwę postaci, więc na każdej instalacji przenosił tę jedną postać, a wszystkim pozostałym po cichu nie robił nic.
- **Boty mają włączone swoje najważniejsze umiejętności.** Buffy rzucały się wyłącznie wtedy, gdy bot miał już cel — czyli bot wchodził do każdej walki bez nich, tracił pierwsze sekundy na rzucanie, a po walce znów stał goły. Teraz utrzymuje je także poza walką.
  - **Sura WP zdejmowała sobie Czarowane Ostrze.** Na liście buffów było 66, czyli Rozproszenie Magii — a to nie buff, tylko atak z flagą `REMOVE_GOOD_AFFECT`, rzucany sam na siebie. Czarowana Zbroja (65) nie była buffowana w ogóle.
  - **Wojownik nigdy nie rzucał Berserku, a szaman jednego z dwóch buffów.** Numery umiejętności były skrzyżowane z flagami efektów, przez co włączony jeden buff raportował drugi jako już aktywny. Sprawdzone w `skill_proto`.

---

## 1.24.1 — 2026-09-04

### Fixed

- **Boty faktycznie kupują na rynku.** Wersja 1.24.0 wystawiała towar i nikt go nie brał, bo zakup wymaga **dwóch** rzeczy naraz, a ustawiona była jedna: `CShopManager::Buy` odrzuca kupującego, który nie jest zarejestrowany jako **gość straganu** (to `AddGuest` ustawia `ch->GetShop()`), nawet gdy właściciel sklepu jest ustawiony poprawnie. Bez tego funkcja wychodziła w pierwszej linii, po cichu.
  - Przy okazji filtr wykluczał boty z `bVisitingShop` — czyli akurat te, które są w mieście i stoją przy rynku. Rynek nie miał w ogóle klientów.
  - **Sprawdzone na serwerze:** transakcje bot–bot z przepływem yanga w obie strony.

---

## 1.24.0 — 2026-09-04

### Changed

- **Bot pilnujący straganu milczy.** Nad głową ma już szyld sklepu, a linia statusu zamazywała jedyną etykietę, której przechodzący gracz naprawdę potrzebuje — nazwę straganu, który właśnie rozważa otworzyć. Ta sama zasada wycisza okrzyk o udanym ulepszeniu.
- **Yang podnoszony pierwszy i niemal natychmiast.** Kolejka łupu sortowała się wyłącznie po odległości, więc bot mijał trzy kupki monet w drodze do skóry, a potem wracał po każdą z osobna — czas schodził na chodzenie po wyczyszczonym polu. Yang idzie teraz przed przedmiotami, a jego opóźnienie spada z 1000–1800 ms na **150–350 ms**. To zresztą yang płaci za mikstury, które pozwalają dalej zabijać.
- **Łatwy Loch Małp dostał własne pasmo poziomów: 18–26.** Wyprawa zależała dotąd tylko od poziomu konia i klasy, nigdy od poziomu postaci — na żywym świecie siedziało tam **48 botów na poziomach 25–35**, tracąc półgodzinne wizyty na medale warte grosze wobec tego, co ten sam czas daje na granicy.
  - Warunek sprawdzany jest w jednym miejscu i działa w obie strony: bot już w środku przelicza go co takt, więc ten, który wyrośnie z pasma, kończy, co robi, i wychodzi, zamiast czekać na trzydziestominutowy limit.
  - **Sprawdzone na serwerze:** po przebudowie **wszystkie** działające boty powyżej pasma opuściły loch (82 wyjścia z powodem `monkey_horse_complete_direct`).

### Added

- **Mapa w panelu ma prawdziwy teren pod spodem.** Rysowany z tego samego `server_attr`, po którym nawigują boty, a nie zrzucony z klienta: ląd, woda i blokady w trzech kolorach, 256×256, **20 KB na wszystkie sześć map**.
  - Pokrywa się ze znacznikami co do piksela, bo jedno i drugie liczone jest z tych samych granic mapy. Dzięki temu jest to tyle samo tło, co narzędzie diagnostyczne: jezioro na obrazku to jezioro, którego znaczniki nigdy nie przekroczą.
  - `tools/render_map_tiles.py` odtwarza kafle, gdy dojdzie nowa mapa.
- **Pasma filtra poziomów dopasowane do świata.** 1-5/6-10/11-15/16+ wrzucało prawie każdego bota do ostatniego kubełka, odkąd populacja sięga 37 poziomu. Teraz **1-15 / 16-25 / 26-35 / 36+**, gdzie 36+ to obsada Doliny Orków.

---

Pomysły gracza z `m2singleplayer.pl` (EXP, miejsce przebywania, umiejętności) panel ma już od dawna — jego strona to wersja 1.15.6. Jedyną rzeczą, której faktycznie brakowało, było tło mapy, i to zostało dodane.

---

## 1.23.10 — 2026-09-04

### Fixed

- **Dwa z dwunastu punktów łowieckich Doliny Orków leżały za wodą i nie dało się do nich dojść.** Zastąpione punktami wewnątrz obszaru, po którym boty faktycznie chodzą — wybranymi tak samo jak pozostałe: najgęstsze skupiska spawnów, przyciągnięte do prawdziwej współrzędnej z `regen.txt`.

### Mosty: boty ich nie przechodzą i nie powinny próbować

Dolina Orków to wysokie wyspy nad przepaściami z wodą na dnie, połączone mostami. Pytanie, czy boty przez nie przechodzą, było więc zasadne — i odpowiedź brzmi **nie**, ale konsekwencja jest odwrotna, niż mogłoby się wydawać.

Przeszukałem całą mapę pod kątem chodliwych pasm z wodą po obu stronach, czyli tego, jak most wygląda w `server_attr`. Na 3,16 mln chodliwych komórek znalazły się **dwa pasma po 16 komórek**, oba poza terenem botów. **Mosty tej mapy nie istnieją w danych kolizji serwera** — dno przepaści jest oznaczone jako woda, a pokład mostu nad nim nie został z niej wycięty.

Dlatego poluzowanie blokady wody **nie pomogłoby, tylko zaszkodziło**: bot nie wszedłby na most, tylko przeszedłby przez wodę na dnie przepaści. Blokada zostaje.

Co z tego wynika w praktyce:

- **Cały teren łowiecki botów to jeden lity ląd.** Sprawdzone wprost: wewnątrz tego obszaru nie ma ani jednej komórki o profilu mostu, więc **żadna trasa między punktami łowieckimi nie prowadzi przez most**.
- Odcinki wody odcinające resztę mapy mają **4 600–6 750 jednostek** — to przepaście, nie strumyki do przeskoczenia.
- **Mapa nigdy nic nie stawia na wodzie:** ani jeden z 532 punktów spawnu, ani jeden z 16 NPC.
- Silnik r40250 **w ogóle nie blokuje wody** — czyta `ATTR_WATER` tylko dla łowienia. Blokada jest naszą decyzją.

**Ograniczenie, wprost:** boty korzystają z około 30% Doliny Orków. Reszta leży za wodą, której nie przejdą, dopóki mosty nie zostaną opisane w danych kolizji.

---

## 1.23.9 — 2026-09-04

### Fixed

- **Dolina Orków była mapą, na którą bot mógł wejść, ale nie mógł na niej grać.** Boty lądowały w punkcie startowym imperium, a `map_n_threeway` to mapa graniczna trzech imperiów — **każdy róg jest odgrodzony murem**. Z tego miejsca bot sięgał **17 z 532 grup spawnu** i ani jednego z dwunastu punktów łowieckich.
  - Trzynaście botów siedziało tam na dokładnie wejściowym poziomie i nie awansowało, planując w kółko trasy, które nie mogły istnieć: **7812 z 8259** wpisów „unreachable" w jednej sesji pochodziło z tej jednej mapy.
  - Wejście i wyjście są teraz w największym spójnym obszarze mapy — **161 grup spawnu** i wszystkie punkty łowieckie. Oba to współrzędne z `regen.txt`, więc są chodliwe, i oba w tym samym obszarze: **do wyjścia bot idzie pieszo**, więc wyjście za murem zablokowałoby każdego, kto tam wszedł.
  - Migracja przenosi boty zostawione w starym rogu do terenu łowieckiego, zanim rdzeń gry je wczyta.
- **Wędrówka pyta, czy punkt jest osiągalny, zanim do niego pójdzie** — na mapach granicznych i w Małpiej Świątyni. Kosztuje to sprawdzenie spójnego obszaru, a oszczędza przeszukiwanie trasy, które i tak musi się nie udać. Bot po złej stronie muru poluje tam, gdzie stoi, zamiast cyklicznie próbować dwunastu niemożliwych celów.

### Added

- **`tools/analyse_map_reach.py`** — narzędzie, którym to zmierzono. Dekoduje `server_attr`, stosuje tę samą regułę blokowania co nawigacja botów, rozlewa się od zadanego punktu i mówi, jaka część tablicy spawnów jest po tej samej stronie murów. Pustynia dostaje z wejścia **1170 z 1172** — dlatego ta mapa zawsze działała, a tamta nigdy.

---

**Zmierzone na uruchomionym serwerze:** trasy nieosiągalne do Doliny Orków **7812 → 0**, w całym ostatnim oknie logu zostały **4 awarie od 2 botów**. Bot obecny na mapie stoi wewnątrz głównego terenu łowieckiego.

**Zamknięte zgłoszenia:** [#5](https://github.com/TieruYT/metin2-playerbots/issues/5) i [#7](https://github.com/TieruYT/metin2-playerbots/issues/7).

## 1.23.8 — 2026-09-04

### Changed

- **Podział kodu botów dokończony — wydanie bez zmian w zachowaniu.** `playerbot_manager.cpp` miał rano **12 527 linii**; ma **1 521**. Wszystko to przeniesienia, nic nie zostało przepisane.
  - Wydzielone w tym kroku: `playerbot_targeting.h` (wybór celu i rezerwowanie go przed setką innych botów), `playerbot_loot.h`, `playerbot_survival.h`, `playerbot_wandering.h`, `playerbot_status.h`.
  - Rdzeń celowania został **jednym plikiem**, choć ma 1266 linii: rezerwacja celu, licznik atakujących metin i decyzja o multi-pullu muszą widzieć ten sam obraz tego, kto z czym walczy. Rozbicie ich dałoby trzy pliki czytające trzy różne prawdy.
  - Sprawdzenie „czy gracz jest blisko" trafiło do statusu — to ono decyduje, że przy nikim nie ma po co nic nadawać.
  - W menedżerze została **rola menedżera**: kim bot jest, jego drużyna, drobne przeglądy, watchdog i sam takt. Takt zostaje tam celowo — jako jedyny musi widzieć wszystkie podsystemy.

---

**Weryfikacja:** kontrola `-m32` w gcc:13 po każdym cięciu — zero błędów, te same dwanaście ostrzeżeń przez cały czas, test jednostkowy przechodzi. Obraz zbudowany i uruchomiony: rejestr 1012 tożsamości, boty piją mikstury, ulepszają, zakładają sprzęt, rozdają punkty, wycofują się taktycznie i wspierają drużynę.

## 1.23.7 — 2026-09-04

### Changed

- **Kod botów podzielony na podsystemy — wydanie bez zmian w zachowaniu.** `playerbot_manager.cpp` miał rano **12 527 linii**; ma **4 001**. Nic nie zostało przepisane, wszystko to przeniesienia.

  | Plik | Co |
  |---|---|
  | `playerbot_navigation.h` | Gdzie bot może stanąć i czy dwa punkty się łączą |
  | `playerbot_world_memory.h` | Czego populacja nauczyła się o świecie |
  | `playerbot_movement.h` | Jazda trasą: koń, waypointy, portale, rejestr metinów |
  | `playerbot_gear.h` | Co bot nosi i dźwiga |
  | `playerbot_activities.h` | Koń i łowienie |
  | `playerbot_missions.h` | Biolog i polowanie na poziom, bez okna zadania |
  | `playerbot_skills.h` | Karta postaci: punkty, kolejność umiejętności, bufy |
  | `playerbot_combat.h` | Sama walka: pakiety ciosu i zaklęcia, umiejętności ataku |
  | `playerbot_economy.h` | Pieniądze i plecak: śmieci, handlarze, kowal, bonusy, stragany |
  | `playerbot_travel.h` | Gdzie bot powinien być i jak przechodzi między mapami |
  | `playerbot_town.h` | Wizyta w mieście od bramy do ostatniej sprawy |

  Lista `#include` na górze menedżera jest teraz **kolejnością zależności całego systemu**, czytaną z góry na dół. W jednej jednostce kompilacji i jednej anonimowej przestrzeni nazw kolejność definicji *jest* kolejnością zależności, więc plik, który idzie pierwszy, to ten, który nie woła niczego.

  W menedżerze została pętla walki (wybór celu, rezerwacja go przed innymi botami, atak, multi-pull, łup, drużyna, wędrówka), tekst statusu i sam takt.

---

**Weryfikacja:** kontrola `-m32` w gcc:13 po każdym cięciu — zero błędów, te same dwanaście ostrzeżeń przez cały czas, test jednostkowy przechodzi. Na koniec obraz zbudowany i uruchomiony: rejestr 1012 tożsamości, boty piją mikstury, ulepszają, kupują sprzęt, chodzą do kowala i handlarza, rozdają punkty.

## 1.23.6 — 2026-09-04

### Fixed

- **Rejestr po cichu odrzucał każdego bota od PID 1003 w górę.** `LPAD` w MySQL nie dopełnia, tylko **skraca**, gdy wartość jest dłuższa niż podana szerokość — więc `LPAD(1001,3,'0')` to `100`, czyli login zupełnie innego bota. Zapytanie rejestru porównywało z tym konto każdej postaci, przez co każdy bot powyżej PID 1002 nie przechodził warunku, którego nie mógł przejść. Bez żadnego błędu w logu.
  - Obsada nie mogła urosnąć powyżej tysiąca, **niezależnie od tego, ile postaci utworzyło ziarno**.
  - Zmierzone na żywym świecie z 1500 zasianymi botami: zapytanie zwracało **511**, po poprawce zwraca **1012**.

### Changed

- **Kanoniczna obsada powiększona do 1500** (PID 4..1503), a limit w launcherze razem z nią. Świat, który nosi boty ze starszego rozruchu, zachowuje je — ale zajmują one PID-y, których rejestr nigdy nie przyjmie, więc jedyny sposób, by dać takiemu światu więcej grających botów, to poszerzyć zakres kanoniczny poza nie.
- **Wydzielony `playerbot_movement.h`** (720 linii): trasy, wsiadanie na konia, przechodzenie waypointów, portale Małpiej Świątyni. Rejestr znanych metinów trafił tam razem z nimi — o tym, czy kamień warto zapamiętać, decyduje to, czy ktoś może do niego dojść, więc rozdzielenie ich znaczyłoby przekazywanie osiągalności z powrotem.
  - `playerbot_manager.cpp`: **8 388 linii** zamiast 12 527 z dzisiejszego rana.

---

**Zmierzone na działającym serwerze, krok po kroku:**

| | boty w grze |
|---|---|
| przed 1.23.5 | 180 |
| po 1.23.5 (ziarno wreszcie dociera) | 511 |
| po 1.23.6 (poprawka rejestru + obsada 1500) | **750** |

## 1.23.5 — 2026-09-04

### Fixed

- **Powiększona obsada botów nigdy do nikogo nie dotarła.** Kontener migracji montuje `linux-port/docker/mariadb/playerbot`, więc SQL, który faktycznie wykonuje, to **kopia** ziarna z overlaya — umieszczana tam przez `prepare-context.sh`, który wymaga czystego drzewa silnika i u gracza nie uruchamia się nigdy. Kopia, którą stosowały wszystkie instalacje, była **z 23 sierpnia**. Rejestr 1000 postaci z 1.23.2 i dzisiejsze dokładanie kont jechały w każdej paczce i nie zmieniały niczego.
  - Synchronizacja overlaya odświeża teraz również tę kopię — na tej samej zasadzie co kontekst budowania: overlay jest źródłem prawdy, a każda jego kopia musi być aktualna, zanim wystartuje to, co ją czyta.
  - **Sprawdzone na żywym świecie z 668 botami**: 668 zachowanych bez zmian, 332 utworzone, 1000 obecnych, a te 667, które miały postęp, nadal go mają. Druga migracja nie tworzy nic i nie zgłasza nic.
- **Nowy plik źródłowy silnika nie skompilowałby się u gracza.** `Makefile` w kontekście budowania jest łatany przez `prepare-context.sh`, więc zmiana na `$(wildcard playerbot_*.cpp)` naprawiła ścieżkę deweloperską i ominęła tę, która ma znaczenie. Naprawiane jest teraz to jedno miejsce przy synchronizacji — bez nadpisywania graczom całego `Makefile`.
- **Migracja przestała ostrzegać sama przed sobą.** PID zwykle wpada w kilka reguł pomijania naraz, a klucz główny zamieniał każde powtórzenie w ostrzeżenie o duplikacie — dwa tysiące linii przed operatorem, zasłaniających te dwie, które mówią, co migracja zrobiła.

### Changed

- **Kod botów podzielony na moduły.** `playerbot_manager.cpp` miał 12 527 linii, bo dodanie funkcji nic nie kosztowało, a dodanie **pliku** kosztowało pięć edycji w `prepare-context.sh` plus edycję łatki. Ta asymetria zniknęła: nic już nie wymienia tych plików z nazwy — skrypt odkrywa katalog, `Makefile` bierze wildcard, pakowarka rozwija wzorzec.
  - Usunięte **483 linie martwego kodu** — stara nawigacja (`CPlayerBotNavGrid`, `MovePlayerBotLegacy`) była osiągalna wyłącznie z siebie nawzajem.
  - Wydzielone `playerbot_navigation.h`, `playerbot_world_memory.h` i `playerbot_gear.h`. Menedżer ma **9 085 linii** zamiast 12 527.
  - To były czyste przeniesienia. **Sprawdzone na żywo**: 1000 botów chodzi, walczy, kupuje, ulepsza, handluje i łowi.

---

## 1.23.4 — 2026-09-04

### Added

- **Boty poprawiają bonusy na swoim sprzęcie.** Ekwipunek to tylko połowa siły postaci — druga połowa to cztery linie bonusów, a do tej pory nikt na nie nie patrzył. Bot na 30+ poziomie, stojąc u kowala, dokłada brakującą linię (`Wzmocnienie Przedmiotu`) albo przelosowuje słabe (`Zaczarowanie Przedmiotu`).
  - Oba przedmioty sprawdzone w `item_proto`, nie wzięte z notatek: **71084** to `USE_CHANGE_ATTRIBUTE`, **71085** to `USE_ADD_ATTRIBUTE`.
  - Silnik **odmawia** zmiany bonusów w założonym przedmiocie (`if (item2->IsEquipped()) return false`), więc bot zdejmuje część, losuje i zakłada z powrotem — dokładnie jak gracz.
  - Punktacja jest celowo zgrubna: ma odróżnić „warto zachować" od „losuj jeszcze raz", a nie odwzorować wzór na obrażenia. Broń ceni obrażenia od umiejętności, krytyki i przebicia; reszta sprzętu punkty życia i obronę.
  - Bot nie tknie ostatnich **120 tysięcy** yang — mikstury i sprzęt są ważniejsze — i wydaje najwyżej trzy kamienie na wizytę.

### Fixed

- **Bot potrafił przestać łowić na dobre.** Sesja wędkarska zwalnia bota z watchdoga bezczynności (stanie przy brzegu **jest** czynnością), więc bot, który nie mógł dojść nad wodę, stał w miejscu w całkowitej ciszy. Teraz:
  - punkt na brzegu jest **przyciągany do zweryfikowanej chodliwej komórki**, tak samo jak punkty usług w mieście;
  - gdy brzeg i tak jest nieosiągalny, bot **zarzuca tam, gdzie stoi** — `fishing()` w r40250 wymaga tylko niezablokowanego pola, wędki i przynęty, a wody nie sprawdza w ogóle (wylicza kierunek i go odrzuca);
  - co 15 sekund trafia do dziennika jedna linia `PLAYERBOT_FISHING: progress` z pozycją, celem, odległością i stanem sprzętu, więc następne zgłoszenie będzie można rozstrzygnąć jedną linią.
- **Panel nazywał co trzeci bonus „Bonus #63".** Tablica nazw pokrywała typy 1–25 i garść innych; brakowało między innymi odbicia ataku, odporności na omdlenie, magicznej wartości ataku i całej rodziny „silny przeciwko…". Dodane **31 brakujących** nazw, po polsku i angielsku.
- **Panel pokazywał cztery podstawowe statystyki jako procenty.** Siła, Zręczność, Inteligencja i Energia Życiowa to liczby, nie procenty — podobnie jak magiczna wartość ataku i obrony czy wytrzymałość. Rozróżnienie ma teraz własną listę zamiast testu na cztery typy.

---

## 1.23.3 — 2026-09-04

### Fixed

- **Serwer nie budował się po aktualizacji — to naprawiamy w pierwszej kolejności.** Zgłosili to `Archded` (build kończył się na `playerbot_types.h: No such file or directory`) i `Nagash` (`class 'CPlayerBotManager' does not have any field named 'm_bRegistryLoaded'`). Obie awarie miały **jedną przyczynę**: aktualizacja wysyłała nowy plik bota do kontekstu budowania, ale **bez jego własnych nagłówków**. Kompilator widział wtedy nowy kod obok nagłówka z poprzedniego wydania — albo bez nagłówka w ogóle.
  - Winne było założenie, że kontekst budowania utrzymuje `prepare-context.sh`. Ten skrypt wymaga czystego drzewa silnika, którego paczka **celowo nie zawiera**, więc u gracza nie uruchamiał się nigdy.
  - Teraz launcher i `start-server.ps1` **kopiują cały katalog źródeł bota** do kontekstu przed każdym budowaniem, więc kolejny nowy plik nie może się już zgubić. Dodatkowo pakowarka aktualizacji **odmawia zbudowania paczki**, w której źródło i jego kopia się rozjeżdżają.
  - `Nagash` podejrzewał, że zepsuła to jego zmiana nazw botów. Tak nie było — nazwy nie miały z tym nic wspólnego.
- **Sklepy botów nie były zamykane; nazwa stragana zostawała nad głową.** Zgłosił `Nagash`. Straganem zarządza silnik, a termin jego zamknięcia — kod bota, i zamykanie wykonywało się **za** watchdogiem bezczynności oraz ratunkami nawigacji. Każdy z nich przerywa takt, więc handlujący bot potrafił nigdy nie dojść do własnego kodu zamykania — a ratunek nawigacji mógł go **przenieść z rynku razem z wywieszonym szyldem**. Zamykanie stragana wykonuje się teraz **przed wszystkim innym**, a dodatkowo:
  - straganowi bez terminu zamknięcia termin jest **dopisywany**, więc żadna ścieżka nie zostawi go otwartego na zawsze;
  - bot, który zginął albo znalazł się poza rynkiem, zamyka stragan natychmiast;
  - stanie przy straganie **liczy się jako aktywność**, więc watchdog nie zgłasza już błędu co 90 sekund dla każdego handlarza.
- **Stragany stały zbyt długo, by ktokolwiek zobaczył ich zamknięcie.** Czas otwarcia zmieniony z 20–60 minut na **10–25 minut**.
- **Świeża instalacja z obsadą 1000 botów przerwałaby się na ziarnie.** Końcowa asercja porównywała wynik z liczbą **350** wpisaną na sztywno — przy rejestrze 1000 pozycji zgłaszała błąd przy **każdym** udanym zasianiu. Teraz porównuje z rzeczywistym rozmiarem rejestru.
- **Czytelny komunikat o limicie GitHuba.** Kilka kliknięć „Sprawdź aktualizacje" pod rząd wyczerpuje anonimowy limit zapytań, a launcher pokazywał wtedy tylko „Operacja nie powiodła się". Teraz mówi wprost, że to limit po stronie GitHuba, że instalacja jest sprawna i że wystarczy poczekać.

### Changed

- **Baza sama dokłada brakujące postacie botów.** Poprosił o to `Gacek`. Do tej pory **jedna** zmieniona nazwa bota unieważniała całe ziarno: świat zostawał na tylu botach, ile akurat miał, a suwak w launcherze obiecywał więcej, niż baza mogła dostarczyć. Teraz postać, która nie należy do ziarna, jest po prostu **pomijana** — a brakujące PID-y powstają normalnie.
  - **Żaden istniejący wiersz nie jest zmieniany ani usuwany.** Twoje postacie, przezwiska i postęp zostają nietknięte; dokładane są wyłącznie te, których w bazie nie ma.
  - Migracja wypisuje teraz, ile postaci zachowała i ile utworzyła.
- **Launcher pokazuje wersje na dole okna:** `Aktualna wersja` i `Najnowsza wersja`. Zielono, gdy masz najnowszą. Manifest czytany jest **raz na sesję**, żeby nie zużywać limitu GitHuba.

---

## 1.23.2 — 2026-09-04

### Changed

- **Suwak liczby botów sięga teraz 1000, a nie 668 — i ta liczba wreszcie coś znaczy.** Sedno problemu nie było w suwaku: spawnować mogą się **wyłącznie boty z rejestru nasion**, a ten miał **350 pozycji**. Dlatego gracz przesuwał suwak wyżej i dostawał 350. Rejestr obejmuje teraz 1000 postaci (PID 4..1003).
  - **Uwaga: istniejący świat zachowuje swoją obsadę.** Migracja celowo nie nadpisuje świata, który ma już własne boty — wypisuje ostrzeżenie i pomija nasiona. Nowy limit dostaną **świeże instalacje**; przeniesienie istniejącego świata na większą obsadę to osobna, świadoma operacja na bazie.

### Fixed

- **Bot wysyłał się na ryby, mimo że nie mógł unieść wędki.** Wędkarzem mógł zostać bot od 10 poziomu, a wędka wymaga 30 — więc bot kupował sprzęt, którego nigdy nie założy, i próbował w kółko. Zgłosił to `OskarPWA`, widząc bota na 13 poziomie zablokowanego przy Rybaku. Teraz o tym, kto może łowić, decyduje **wymagany poziom samej wędki**, odczytany z danych przedmiotu — więc zmiana wędki nie rozjedzie się z kodem.
- **Łowienie w ogóle nie mogło się odbyć.** Rzeka jest w Joan, a każdy bot dość wysoki, by unieść wędkę, dawno stamtąd wyszedł — więc warunek „łowimy tylko w M1" nie mógł być nigdy spełniony. Wyprawa na ryby jest teraz **prawdziwym celem podróży**: bot wraca do Joan, łowi i dopiero potem wraca do polowania.
- **Sesja łowienia ma twardy limit czasu.** Bot, który nie dotrze nad wodę, kończy sesję z wpisem w dzienniku, zamiast w nieskończoność chodzić tą samą nieudaną trasą.

---

## 1.23.1 — 2026-09-03

### Fixed

- **Boty, które utknęły bez sektora, wracają do gry.** Postać bez sektora nie może się ruszyć **w ogóle**, a kod tylko zapisywał błąd i próbował ponownie w następnym takcie. W logach z jednego dnia dało to **35 tysięcy takich wpisów od 45 botów**, które nie zrobiły już ani kroku, plus 7900 bezskutecznych resetów watchdoga. Teraz taki bot jest przenoszony na punkt wejścia swojej mapy.
- **Boty zapisane na mapie, której serwer nie prowadzi, nigdy się nie pojawiały.** Wczytanie postaci pytało o pozycję, nie dostawało odpowiedzi i się poddawało — te same dwa boty przepadały przy każdym z 17 startów w ciągu dnia, bez możliwości odzyskania, bo pętla AI widzi tylko boty, które się pojawiły. Migracja przy starcie przenosi je teraz do Bokjung. **U mnie odzyskała 4 boty.**
- **Koniec zalewania dziennika przy starcie.** Zabezpieczenie przed przejęciem cudzego konta zapisywało błąd dla każdego niezarejestrowanego numeru — 170 linii przy każdym uruchomieniu za coś, co działa dokładnie tak, jak ma działać. Teraz jest to jedna zbiorcza linia.
- **Paczka diagnostyczna zbierana przy wyłączonym Dockerze** była oznaczana jako udana, choć zamiast logów kontenerów zawierała same błędy połączenia. Teraz na górze pliku jest wyraźne **PACZKA NIEPEŁNA** z informacją, co zrobić.

---

## 1.23.0 — 2026-09-03

### Added

- **Boty prowadzą stragany w Bokjung.** Stała dziesiąta część botów po załatwieniu spraw w mieście idzie na rynek, zsiada z konia i wystawia jeden przedmiot na 20–60 minut: materiał albo naprawdę zbędną broń czy zbroję, w cenie liczonej z kursu NPC. Zapasy, noszony sprzęt i rzeczy, których gra zabrania sprzedawać, nie trafiają nigdy. **Możesz u nich normalnie kupować.**
- **Walka z siodła.** Koń bojowy (11+ poziom) jedzie teraz do walki, zamiast zostawać na podejściu. Metiny bije z siodła każdy, kto takiego konia ma; wojownicy i sury tłuką konno także zwykłe spoty. Łucznicy i zwykłe konie nadal walczą pieszo. Blokadą nie był dystans zsiadania, tylko podejście do walki wołające ruch bez zgody na konia — bot zsiadał za każdym razem.
- **Wędkowanie.** Część botów w M1 kupuje wędkę u Rybaka i łowi na brzegu poniżej niego. Zrobione pod to, co silnik faktycznie robi, a nie pod opis: przynęta siedzi w gnieździe 2 wędki, branie przychodzi po 10–40 s i daje 6 sekund okna, a szczyt wypada około 3 s po braniu — więc bot czeka na właściwy moment, zamiast szarpać. Połów jest patroszony, a małże otwierane, co wprowadza do gospodarki **perły**. Pas brzegu i kierunek odczytane z `server_attr` mapy, nie zgadnięte.
- **Ogłoszenia u kowala na Wołaj.** Udane ulepszenie na +7 i wyżej ma szansę trafić na czat, najwyżej raz na trzy minuty w skali świata. **Przy zaatakowaniu, w PvP i po zabiciu boty milczą.**
- **Pamięć ras map.** Populacja zapamiętuje, z czego składa się każda mapa, i waży tym bonusy rasowe — „silny przeciw orkom" liczy się bardziej tam, gdzie orki naprawdę są. Mapa musi mieć 200 obserwacji i wyraźną większość, zanim cokolwiek zostanie uznane za jej rasę.

### Fixed

- **Prywatnego sklepu nie mógł otworzyć nikt w zbroi — także Ty.** `OpenMyShop` zaczynał się od `GetPart(PART_MAIN) > 2`, a silnik trzyma w `PART_MAIN` numer noszonej zbroi, więc warunek odrzucał każdą ubraną postać. Miał chronić przed otwieraniem sklepu w transformacji i teraz pyta o to wprost. Zmiana idzie jako osobna łatka rdzenia.
- **Bot trzymający cel nie sprawdzał już podróży** ([#10](https://github.com/TieruYT/metin2-playerbots/issues/10)), przez co gęste spoty potrafiły go uwięzić.
- **Loch Małp nie pytał o mikstury** ([#11](https://github.com/TieruYT/metin2-playerbots/issues/11)) — bot bez mikstur zostawał i ginął w kółko. Zgłoszone razem z poprawką przez `sentydeploy`.
- **Paczka aktualizacji nie zawierała trzech plików**, bez których przebudowa u gracza kończy się błędem kompilacji: `playerbot_world_rules.h` (zyskał nowe pole), nowy `playerbot_types.h` oraz `prepare-context.sh`, który jako jedyny kopiuje pliki overlaya do budowania.

---

## 1.22.4 — 2026-09-03

### Changed

- **Boty idą po mikstury dopiero wtedy, gdy są na wykończeniu.** Poprzednia wersja wysyłała je do handlarki już przy połowie pasa, co kosztowało dobry spot za zakup, którego nie potrzebowały. Wyprawa po zakupy to teraz próg **150 czerwonych / 100 niebieskich**. Uzupełnianie **do pełna** (800/600) zostaje bez zmian, ale dzieje się przy okazji — gdy bot i tak stoi u handlarki, dokupienie nic nie kosztuje.

---

## 1.22.3 — 2026-09-03

### Fixed

- **Boty powyżej 35 poziomu nie mogły wyjść z Joan i biły tam wilki.** Zgłoszone przez `sentydeploy` w [#9](https://github.com/TieruYT/metin2-playerbots/issues/9) razem z pomiarem na 350 botach: **z 276 postaci powyżej 35 poziomu aż 243 tkwiły w Joan**, goniąc Dzikie Psy i Niebieskie Wilki. Bramka wyjścia z miasta sprawdzała tylko przynależność do grupy expiącej w Bokjung, która kończy się na 35 poziomie — więc bot 36+ odbijał się od niej przy każdym takcie i nigdy nie docierał do trasy na nowe mapy. To właśnie te „boty biją psy na 30 poziomie", które wracały na Discordzie. Przechodzi teraz każdy, kto ma dokąd pójść.
- **Pętla Joan ↔ Bokjung powyżej sufitu.** Bot, którego Joan wypuściła, docierał do Bokjung, a tam odsyłano go z powrotem, bo nie należał do tamtejszej grupy. Powyżej 35 poziomu nie ma już powodu wracać.

### Changed

- **Boty noszą znacznie większy zapas mikstur: 800 czerwonych i 600 niebieskich**, jeśli mają na to yang (wcześniej 300 i 80). Uzupełniają je po zejściu poniżej połowy, a nie dopiero przy pustym pasie, i nigdy nie wydają więcej niż połowy portfela. Limit odsprzedaży nadmiaru podniesiony razem z zakupem — inaczej boty od razu sprzedałyby to, co kupiły.

---

## 1.22.2 — 2026-09-02

### Fixed

- **Boty kupowały hełm innej klasy i nie mogły go założyć.** Zgłoszone przez `sentydeploy` w [#8](https://github.com/TieruYT/metin2-playerbots/issues/8) — bardzo dobra diagnoza. Rodziny hełmów dzieli 140 numerów, a pętla wyboru sięgała dokładnie `baza + 140`, czyli startowego hełmu sąsiedniej klasy. Ponieważ wygrywało „ostatnie trafienie", a startowy hełm wymaga 0 poziomu, zawsze nadpisywał poprawny wybór. Wojownik kupował hełm Ninji, Sura hełm Szamana — i gra odmawiała założenia. Widać to było w danych: Szaman, którego rodzina jest ostatnia w łańcuchu, miał hełm w 100% przypadków, pozostałe klasy poniżej 50%. Wybór idzie teraz według **najwyższego wymaganego poziomu**, więc przedmiot startowy obcej klasy nigdy nie przebije właściwego. Ta sama poprawka objęła wszystkie pięć list progresji (broń, zbroja, tarcza, hełm, buty).

- **Przerwana aktualizacja nie zostawia już serwera w martwym punkcie.** Gdy Docker nie dokończył budowania, nowe pliki (w tym `VERSION`) były już na dysku, więc launcher mówił „masz najnowszą wersję" i nigdy nie ponawiał budowania — a „GRAJ" startowało stare obrazy. **Właśnie dlatego część graczy nie widziała nowych map na stronie mimo aktualizacji.** Launcher zapisuje teraz znacznik nieukończonej przebudowy: dopóki istnieje, wersja liczy się jako nieznana, a kliknięcie **GRAJ** samo dokańcza budowanie. Nic nie trzeba robić ręcznie.

### Changed

- **Nowe pasma poziomów dla map.** Bokjung do 29, **Pustynia Yongbi 30–35**, **Dolina Orków od 36**. Zwykłe moby w Dolinie mają 18–25 poziom, ale jej **metiny mają 45, 48 i 50** — a metina opłaca się rozbijać dopiero od `poziom_metina − 9`. Dolina zaczyna się więc realnie opłacać dopiero od 36 poziomu i tam trafiła, mimo słabych mobów.
- **Większy zapas niebieskich mikstur dla klas walczących wręcz.** Wojownik i Ninja uzupełniają je teraz do 20 sztuk zamiast 10 — umiejętności zużywają SP bez przerwy. Brak niebieskich nadal nie blokuje podróży, bo to właśnie zamykało boty w mieście.

---

## 1.22.1 — 2026-09-02

### Fixed

- **Aktualizacja z launchera nie wywala już serwera przy dużym świecie.** Migracja bazy czekała na nią 5 minut i się poddawała, a Docker traktuje to jako nieudany start całego serwera — stąd „Docker nie zbudował serwera", mimo że uruchomienie ręczne minutę później działało. Świat, który nie został czysto zamknięty, potrafi odtwarzać się dłużej niż 5 minut, więc limit to teraz 30 minut, a skrypt na bieżąco pisze, że czeka. **To samo naprawia panel www, który u części graczy nie pokazywał nowych map** — przerwana aktualizacja nie zdążyła przebudować strony.
- **Migracja pokazuje wreszcie błąd bazy, zamiast go połykać.** Odrzucone logowanie wyglądało dokładnie tak samo jak wolny import. Teraz wypisuje powód, a przy „Access denied" wprost odsyła do przycisku „NAPRAW DOSTĘP DO BAZY".
- **Boty przestały odbijać się od nowych map.** Przyjeżdżały i wracały po kilku sekundach — 271 przyjazdów na Pustynię dawało 269 powrotów, więc obie mapy wyglądały na puste, bo każdy bot był akurat w drodze. Złożyły się na to trzy rzeczy: brak niebieskich mikstur uznawany za sytuację awaryjną **także u wojownika i ninja** (dotyczyło 210 z 442 postaci, które ich nie używają), sprawdzanie ekwipunku zanim zdążył się wczytać po zmianie mapy, oraz brak minimalnego czasu pobytu. Bot zostaje teraz co najmniej 2 minuty, chyba że naprawdę nie może walczyć.
- **Bot, który trafi na nieobsługiwaną mapę, wraca do Bokjung.** Na nowych mapach stoją prawdziwe NPC teleportujące; wejście w ich zasięg przenosiło postać tam, gdzie AI nie ma żadnego planu, i bot zostawał tam na zawsze.

### Changed

- Ostrzeżenie Dockera o wolumenie `db-data` nie pojawia się już w oknie launchera. Jest nieszkodliwe, ale słowo „warning" obok bazy skłaniało graczy do `down -v` — jedynej komendy, która naprawdę kasuje świat. W dzienniku sesji zostaje.

---

## 1.22.0 — 2026-09-02

### Added

- **Boty wychodzą wreszcie poza Bokjung — na Dolinę Orków i Pustynię Yongbi.** Progresja kończyła się na M2, którego moby przestają się opłacać koło 27 poziomu, więc bot, który je przerósł, był odsyłany do Joan bić wilki na 3 poziomie. Dolina Orków (moby 18–25) obsługuje poziomy 20–27, Pustynia (moby 26–30) poziomy 28–33. Boty podróżują przez Teleportera, którego każda wioska ma naprawdę, i wracają bramką kilka kroków od miejsca, w którym się pojawiły.
- **Obie mapy na stronie.** Selektor na `/map` ma teraz sześć pozycji, więc widać boty także na nowych mapach. Teleport do bota działa tam bez zmian.
- **Mapy cieplne.** Nowy przełącznik pokazuje, **gdzie boty giną** i **gdzie rozbijają metiny** — na podstawie zdarzeń, które serwer i tak zapisuje. Wielkość plamy skaluje się pierwiastkiem z liczby zdarzeń, więc jedno gorące miejsce nie zalewa całej mapy.

### Fixed

- **Brakujący hełm nie przykuwa już bota do jednej mapy.** Brak dowolnej części EQ liczył się jako krytyczna potrzeba miasta, a ta blokowała podróż bezterminowo. Bot zawsze chciał na zakupy, więc nigdy nie wolno mu było wyjść — i bił wilki na 3 poziomie mając 27 poziom i ponad milion yang. Teraz blokują tylko braki, które faktycznie uniemożliwiają grę (broń, zbroja, mikstury, strzały, pełny plecak); reszta dostaje jedną wizytę w mieście i jeśli miasto nie ma czym pomóc, bot rusza dalej.
- **Poprawione przyciski teleportacji w panelu.** „Pustynia" i „Ognista Ziemia" wskazywały współrzędne spoza świata gry, więc nigdy nie działały. Wszystkie miejsca celują teraz w punkt odrodzenia Chunjo danej mapy; doszły też Bokjung, Dolina Orków, Pustynia i Góra Sohan.

### Changed

- **Osobowość bota wpływa teraz na to, gdzie gra**, a nie tylko na to, jak ryzykownie ulepsza. Wędrowiec wyrusza na dalekie mapy 7 razy na 8 i zostaje tam dwa razy dłużej, ostrożny kolekcjoner 2 na 8 i wraca szybciej. Około 38% każdego pasma zostaje w Bokjung, żeby miasto, bestie i pula na drużyny nie pustoszały.
- Mapa 64 przeniesiona na rdzeń `game1`. Bot serwerowy nie ma klienta, który przełączyłby się między rdzeniami, więc cała jego trasa musi być na jednym — mapa 303 poszła w drugą stronę dla zachowania balansu.

---

## 1.21.2 — 2026-09-02

### Fixed

- **Launcher prosi o ponowne uruchomienie po aktualizacji.** Aktualizacja podmienia pliki samego launchera, ale okno, które masz otwarte, wczytało stary kod przy starcie — więc nowe przyciski się nie pojawiały i wyglądało to, jakby aktualizacja nie weszła (choć na dysku była). Teraz launcher wykrywa, że podmienił własne pliki, i pyta: „Uruchomić ponownie teraz?".
- **„IMPORTUJ BAZĘ" przy wyłączonym Dockerze nie kłamie już, że nie ma czego importować.** Wcześniej lista baz wychodziła pusta i komunikat brzmiał jak utrata danych. Teraz launcher sprawdza silnik i mówi wprost: uruchom Docker i spróbuj ponownie — dane są całe. To samo dotyczy „NAPRAW DOSTĘP DO BAZY".

### Added

- **Data utworzenia przy każdej bazie na liście importu.** Nazwy instalacji to nieczytelne skróty (`m2pb-34c3e45f`), więc lista pokazuje teraz `m2pb-34c3e45f   (utworzona 2026-08-31 23:51)` — widać, która jest która. Działa tak samo w launcherze graficznym i tekstowym.

---

## 1.21.1 — 2026-09-02

### Added

- **„ZBIERZ / WYŚLIJ LOGI" wysyła paczkę prosto na Discorda projektu.** Gdy kanał zgłoszeń jest włączony, jeden klik pakuje logi i wrzuca je na serwer autora — bez szukania folderu i przeciągania pliku. Paczka nadal ma wycięte hasła, a wiadomość nie może nikogo oznaczyć (żadnych `@everyone`).
- Gdy kanał jest wyłączony albo nie ma internetu, launcher robi to co dotąd: zapisuje ZIP, otwiera jego folder — a teraz dodatkowo otwiera zaproszenie na Discorda, żeby było wiadomo, gdzie go wrzucić.

### Changed

- **Adres kanału zgłoszeń jest pobierany z manifestu aktualizacji**, a nie wbudowany w launcher. Dzięki temu można go włączyć, zmienić albo unieważnić edycją jednego pliku na GitHubie — bez wydawania nowej wersji i bez ponownej instalacji u graczy. Własne ustawienie w `.m2launcher.json` nadal ma pierwszeństwo.
- Launcher rozpoznaje pomyłkę „zaproszenie zamiast webhooka" (`discord.gg/...`) i mówi wprost, jak wygląda poprawny adres, zamiast zgłaszać błąd HTTP.
- Paczka większa niż 10 MB jest odrzucana **przed** wysyłką, z informacją ile waży — wcześniej Discord odrzucał ją dopiero po przesłaniu całości.

---

## 1.21.0 — 2026-09-02

### Fixed

- **Import nie potrafi już zniszczyć bazy.** Jeśli baza tej instalacji jeszcze nie istniała, import montował nieistniejący wolumen — Docker tworzył go pustego, a MariaDB inicjalizowała bazę **bez hasła i bez schematów gry**. Ponieważ wolumen przestawał być pusty, właściwa inicjalizacja nigdy się już nie uruchamiała i instalacja zostawała trwale zepsuta (trzeba było kasować wolumeny). Teraz launcher odmawia i mówi wprost: najpierw uruchom serwer („GRAJ"), dopiero potem importuj.
- **Czytelna podpowiedź przy błędzie startu.** Gdy `playerbot-migrate` nie może zalogować się do bazy, launcher pokazuje konkretną instrukcję: kliknij „NAPRAW DOSTĘP DO BAZY", zamiast surowego błędu Dockera.

### Added

- **Suwak liczby botów (0–668).** Zamiast wpisywania liczby — suwak z podglądem wartości. Efektywny limit to liczba botów w Twoim świecie (kanoniczna paczka ma 350).
- **Widoczny postęp podczas uruchamiania.** Log w launcherze pokazuje teraz przebieg **na żywo**, a pasek postępu i status podają upływający czas oraz etap budowy (np. `game deps 4/6 (67%)`). Koniec z ciszą przez kilka minut i pytaniem „czy to się zawiesiło?". Przy pierwszym budowaniu launcher wprost uprzedza, że potrwa to kilkanaście–kilkadziesiąt minut.

### Changed

- **Jeden przycisk aktualizacji.** Zamiast osobnych „SPRAWDŹ" i „ZAINSTALUJ" jest teraz samo **SPRAWDŹ AKTUALIZACJE**: launcher sprawdza kanał i — jeśli faktycznie jest nowsza wersja — pyta „Znaleziono nową wersję. Zainstalować teraz?" (TAK/NIE). Gdy nic nowego nie ma, mówi to wprost, zamiast cokolwiek instalować.
- **Szybsze zgłaszanie problemów.** Przycisk „ZBIERZ / WYŚLIJ LOGI" potrafi od razu wysłać paczkę logów do autora, jeśli w konfiguracji ustawiono kanał pomocy; w przeciwnym razie zapisuje ZIP i otwiera folder do załączenia na Discordzie. Hasła są z logów usuwane.

## 1.20.0 — 2026-09-02

### Fixed

- **Import świata już nie blokuje startu serwera.** Po „IMPORTUJ BAZĘ" migrator botów potrafił nie uwierzytelnić się w bazie (`playerbot-migrate` kończył się błędem), przez co gra i panel nie wstawały. Import zamyka teraz bazę pomocniczą łagodnie (bez wymuszania odzysku InnoDB) i po podmianie świata odtwarza techniczne konto bazy wraz z uprawnieniami. Postacie, przedmioty i boty pozostają nietknięte.
- **Nowy przycisk „NAPRAW DOSTĘP DO BAZY".** Jednym kliknięciem odtwarza techniczne konto bazy dla osób, które zaimportowały świat wcześniejszą wersją i zostały z serwerem, który nie startuje. Nie zmienia postaci ani botów.

## 1.19.0 — 2026-09-01

### Added

- **Dostęp do bazy z Navicat / HeidiSQL / DBeaver.** Baza jest wystawiona na `127.0.0.1:3306` (tylko lokalnie). Loguj się jako `root` / `local-playerbots-root` albo `metin2` / `local-playerbots-game`, aby edytować itemy, NPC i questy.
- **Ustawianie liczby grających botów (0–350)** w launcherze — przycisk „LICZBA BOTÓW".
- **Import świata z innej instalacji** („IMPORTUJ BAZĘ") — kopiuje postacie/poziomy/ekwipunek z innego wolumenu Docker na tym komputerze, z kopią zapasową i bez ruszania źródła. Pokazuje datę utworzenia świata i ostatniej gry.

### Fixed

- **Koniec fałszywego „Błąd: Start (kod )".** Launcher nie myli już postępu Dockera na stderr (ani nieodczytanego kodu wyjścia w GUI) z awarią — udany start pokazuje „Gotowe".
- **Mapa botów po zmianie nazwy.** Panel rozpoznaje boty po koncie `playerbot_`, więc bot ze zmienioną nazwą znów jest widoczny na mapie.
- **Bezpieczeństwo.** Zwykła postać gracza nie może już zostać przejęta jako bot.
- Poprawne wykrywanie i przejmowanie istniejących instalacji Docker przy pierwszym uruchomieniu z nowego folderu.

## 1.17.0 — 2026-08-25

### Added

- **Autonomous Chunjo multi-map progression.** Bots now travel between Joan (M1), Bokjung (M2), Waryong (M3) and the Easy Monkey Dungeon through the real portals and local dungeon teleports. M1 remains the natural early zone, most level 20–35 characters move into M2, and a suitably equipped cohort visits M3.
- **A real Horse Medal journey.** Eligible bots enter the Easy Monkey Dungeon, navigate its disconnected rooms through the native GOTO portal graph, pick up actual Medal drops and deliver them to a Stable Boy. The M2 Stable Boy is used after a dungeon trip, avoiding the old M2 → M1 detour. Different bots deliberately farm a stock of one to three medals before leaving, with a bounded visit time.
- **Mounted travel.** Bots with a horse mount for long journeys to merchants, the Blacksmith, the Biologist and world portals, then dismount near the destination and before ordinary combat. Combat-horse fighting remains a later milestone.
- **M3 equipment progression.** Infected animals can drop the class-specific level-30 weapon families and the level-21 Pentagon Shield through an additive serverfiles overlay. Bots value positive average-damage weapons especially highly.
- **Operational safety tools.** Added a verified compressed MariaDB backup helper and a read-only horse-journey diagnostic report.
- **A bilingual live world panel.** `/map` now supports Polish and English throughout, filters M1/M2/M3/Monkey Dungeon positions, and localises classes, professions, skills, hunting missions, Biologist stages and item names from the stock Polish locale table.

### Changed

- Loot remains visible for 1.0–1.8 seconds and is picked up one stack at a time instead of disappearing in the kill tick. Bots finish an active pull before starting the loot sweep.
- Combat target selection now prioritises the remaining monsters already attacking the bot or its party, so a character does not abandon a half-finished group for a fresh target.
- Playerbot combat ignores both attackers and targets inside `ATTR_BANPK` safe zones, preventing endless attacks against invulnerable monsters near teleporters.
- Shields are now treated as core progression gear: bots buy a Battle Shield when missing one, equip better shields and include them in Blacksmith refinement decisions.
- Skill development is no longer identical within a class. Mental Warriors rotate between Strong Body, Bash, Spirit Strike and Stump priorities; Dragon Shamans alternate Dragon's Aid and Dragon Roar openings. Dagger Ninjas learn Stealth but do not waste PvE actions casting it.
- Long-distance navigation can retain its mounted state across incremental route updates. Monkey Dungeon routing blocks immediate reverse-portal loops.
- The Playerbot autospawn safety ceiling was raised from 350 to 1000 while the reproducible bundled seed remains 350 bots.
- Docker-published auth, game and panel ports accept the explicit `M2_HOST_BIND_ADDRESS` setting.

### Fixed

- Corrected the Waryong arrival point to the walkable `Town.txt` location. The previous generic teleporter coordinates landed at the unwalkable north-west border without a sectree.
- Bots carrying a Horse Medal in M2 no longer return to M1 before looking for a Stable Boy.
- Bots no longer instantly clear freshly spawned Metin loot or leave it behind because town-service logic ran first.
- Horse travel no longer repeatedly dismounts and remounts when a lightweight AI update continues an existing route.

### Verification

- Rebuilt the full r40250 game image and the panel image successfully.
- Verified 51 live bots moving on valid Waryong coordinates after deployment, with no recurrence of the old arrival coordinate or missing-sectree error.
- Verified both Polish and English panel views, map filters and localised equipment/skill details against a live 668-bot development world.

---

## 1.16.0 — 2026-08-24

### Changed

- **The project is now installable independently of the retired upstream repository.** Installer, updater, panel metadata, and image labels point at `TieruYT/metin2-playerbots`.
- **Native-client-only installation.** The withdrawn upstream WebClient is not fetched, published, or started. Legacy WebClient flags remain accepted only so old saved commands do not fail.
- **Bring your own files (BYOF).** No third-party r40250 server/client mirror is built in. Supply a local server archive/reference directory and, optionally, a native client archive you are authorised to use. Windows adds `-ClientArchive` and `-NoClient`.
- Added an explicit provenance and attribution document, retained upstream Git history, and clarified the licensing boundary for all game files.

### Fixed

- Corrected Docker Compose host/container port interpolation so a fresh local install can bind the panel and game ports without producing an invalid four-part port mapping.
- Verified the complete local archive path against `Reference_Server.zip`: checksum, r40250 baseline, extraction, 30 patched files, and staging all pass without fuzz or rejects.

---

## 1.15.6 — 2026-08-14

### Fixed

- **Fixed Issue with Admin Panel**

---

---

## 1.15.5 — 2026-08-14

### Added

- **The panel points at the community's Discord.** A quiet line in the footer of every page, and a proper card for anyone signed in to their game account, saying what is actually there: what changed in the last update, when the server is down for maintenance, and people who answer a question faster than you can search for it — and that a bug reported there is the quickest way to get it fixed. In all three languages the panel speaks.

- The address is **part of the software, not a setting.** It is written into the code rather than read from a config file, deliberately: it is where this project posts its news, so an install cannot quietly drift to a different address and leave its players pointed at nothing. An operator running their own community changes that one line, which is a change to the software and looks like one.

---

## 1.15.4 — 2026-08-14

### Fixed

- **Updating a server could leave it unable to start.** The scripts that start and supervise the game cores are installed into the images with `chmod +x`, and that cannot repair the one thing that was wrong with them. A file staged as "executable by everyone but readable only by its owner" already has the execute bit for everybody, so `+x` changes nothing — and the server does not run as the owner. A compiled program would still have run; a shell script cannot, because the shell has to *read* it. The result was a game container restarting in a loop, once a minute, on `Permission denied`, with every other container healthy and the cause nowhere near the message. The four images that install scripts now set an absolute mode instead, the way the bridge image always did — which is exactly why the bridge was the one part that never broke.

- **The updater could not apply any change to the game's own code.** Every customisation this project makes to the server — the Custom Experience, High Risk mode, the storeroom pages — is a Python script, and the updater image did not contain Python. So an update that carried one of them stopped with `FATAL: python3 is needed`, refused to go further, and left the server on its old version. It never showed up during a first install, because that runs on the host, where Python is present. The updater image now carries it.

### Note for anyone running a server

Both of these are in the images, so they take effect the next time the images are rebuilt — which is what an update does anyway. If your server is currently in the restart loop described above, the update that fixes it is the same update that was failing; rebuild the images once and it clears.

---

## 1.15.3 — 2026-08-14

### Added

- **The storeroom at the Storekeeper has three pages.** It opened with one page — 45 slots — and the only way the game ever gave you more was a premium account or an item-mall chest expander, neither of which exists on a server that sells nothing, so the second and third pages were unreachable by design. Every account now gets all three, 135 slots, the moment the cores are rebuilt. Nothing is stored per account and nothing has to be bought, claimed or unlocked. Three is also the most this game can hold: the chest is exactly 135 slots wide internally, and the build refuses to go past it rather than quietly corrupting itself. Nothing you have stored moves and nothing is lost — the extra pages are added after the slots you already have, and every client already draws as many page tabs as the server tells it to, so there is no client update to install.

- **The bonus drops off metins and bosses can be dropped, handed to another player and put in a private shop.** Every item in that drop group — Blessing Scroll, Bravery Cape, Exorcism Scroll, Concentrated Reading, the two bonus scrolls, the Experience Ring and the Thief's Gloves — shipped locked to the character who found it: no dropping it, no handing it over, no shop. All three locks are now off. Selling them to an NPC is still refused, deliberately: it is the one mistake you cannot undo.

- **Exorcism Scrolls and Concentrated Reading stack.** Both rows already claimed to be stackable and were vetoed by a second flag on the same row, so a handful of them took a handful of inventory slots. Using one out of a stack spends one — the branch that consumes them already counted down instead of deleting the item.

- **Experience Rings and Thief's Gloves stack, and only while they are unused.** A ring you have worn never merges back into the fresh pile, so you can never lose the time left on one by stacking it: a unique item keeps its remaining time in a socket, the countdown only runs while it is on, and all three places the server merges items already compare every socket before combining anything. Putting one on takes **one** out of the pile — the server moved the whole stacked item into the slot, which was harmless while these could not stack and would have destroyed the entire pile when the hour ran out, so the two changes are made together and neither is applied without the other. If your bags are completely full the ring is not put on at all, rather than the rest of the pile being thrown away to make room. Quivers are untouched.

### Fixed

- **The bonus items dropped by metin stones were the wrong ones.** They were the pair that re-rolls the two *rare* bonus slots — the fifth and sixth, which almost nothing in this game ever fills and no ordinary item shows. Players were being handed items that appeared to do nothing on everything they owned. They are now the pair that works on bonuses one to four, which is what a player actually reads off a weapon or a piece of armour.

- **Ordinary monsters dropped boss loot.** The drop table was attached to 130 monsters it was never meant to include — everything at the rank just below a boss, the stone apes among them. That rank is a common monster in this distribution, not a boss, so the metin and boss reward was falling off things that die by the dozen. The table now covers exactly what it says: 68 metin stones and 108 bosses.

### Note for anyone running a server

Two of these changes are in the game's C++ and not in a data file — the three storeroom pages and the split that takes a single ring out of a stack. **The cores have to be rebuilt** for this release to do anything; re-running the installer is enough. Your database is not touched, and characters, accounts and stored items all survive.

---

## 1.15.2 — 2026-08-13

### Fixed

- **Killing another High Risk player never costs reputation.** It was already meant to be free, and mostly was — but only as a side effect of the killer mark the mode keeps lit, and that mark blinks: it is cleared the moment its wearer dies and only comes back a few seconds later, and there is a gap right after someone opts in before it is lit at all. A kill landing in either gap quietly took 20,000 reputation off the killer for something the mode promises is free. The rule now asks whether the victim is in High Risk instead of watching for the mark.

- **Updating with `--domain` forgot the e-mail address your certificate is registered with.** It was only ever read back from your server's settings inside the branch that runs when you *don't* name a domain — and naming it is exactly what the panel's own update command does. Passing `--domain` therefore skipped it and handed the certificate tool an empty address. It is now read back first, whichever way you run it.

## 1.15.0 — 2026-08-13

### Fixed

- **High Risk did nothing at all, on every server built from this repository.** The mode was offered at level 15, the player chose it, the choice was saved — and then nothing happened: they were not attackable, not marked, dropped nothing extra on death and got none of the bonuses. Only the *quest* half was ever staged into a build. The half that gives the choice meaning is a change to the game core, and that change existed only on the machine where it had been applied by hand; the server tree is re-staged from the pristine archive on every build, so it was never in anyone else's server. It is now applied during the build, every time, and the build refuses to start if it is missing rather than quietly producing a server where the mode is decorative.
- If you have High Risk switched on, this is the update that makes it real. Nobody has to re-choose anything: the choice was being recorded correctly the whole time and takes effect the moment the cores are rebuilt.

### Changed

- **High Risk sets your combat mode for you.** Choosing the mode put you in reach of everyone else in it without letting you fight back: whether *you* may hit someone is decided by your own combat mode, and that is Peace until you change it by hand. It is now set to Free for as long as High Risk is on, and kept there — the game quietly resets it in a few places, such as when a duel ends, and it comes straight back. Game masters and characters below the protection level are left alone, exactly as the rest of the game leaves them alone.
- **High Risk is now a pool, not a licence.** It only ever pairs you with other players who also chose it. Someone in No Risk can no longer kill a High Risk player and take what they drop, and a High Risk player can no longer hunt someone who never opted in — neither direction works, inside an empire or across empires. Both of you chose, or there is no fight. Guild wars, castle sieges, duels and the arena are settled before this rule and keep working between the two modes, because those are consensual on their own terms.
- **A High Risk death costs more.** Dying with the mode on now drops something from your bags **one time in two** (was one in ten) and something you are wearing **one time in ten** (was one in five). Quantities are unchanged — eight items from the bags, one worn item — and so is everything about which items can be lost at all. Characters who are simply Cruel are not affected: they still drop on exactly the odds the game shipped with, which is what the mode borrowed before and no longer does.

### Added

- **Skill books stack, and only with books of the same skill.** Part of the Custom Experience. Books for one skill now merge into a single slot instead of taking a fresh one each, whether you pick them up, drag them together or store them in the chest. Books for *different* skills never merge, and this is not a promise the change had to make good on itself: the generic Skill Book is one and the same item number for every skill it can teach — it carries the skill in a socket rather than in its number — and all three places the server merges items already compare every socket before they combine anything. Two books that teach different things differ there and are refused.
- Reading one book out of a stack now spends one book. The core deleted the *item* when a book was read, which was the same thing while books could not stack and would have thrown away the whole pile once they could, so the two changes are made together and neither is applied without the other.
- Off unless the Custom Experience is on. Unlike the Blessing Scroll, there is nothing in the shipped files that contradicts itself here — all 45 skill books agree that they do not stack — so this is a deliberate change rather than a fix, and it sits with the other deliberate ones.

## 1.13.1 — 2026-08-13

### Fixed

- **Nothing could be picked up.** Neither items nor Yang, by key or by clicking — the character walked to the drop and left it lying there. Removing a diagnostic line before the release took the body of an `if` with it, so the branch adopted the next statement and the pick-up was nested inside a condition that could never be true at the same time. It compiled without a warning. Browser client 1.11.6; the game data is unchanged, so this is a 17 MB update rather than 1.7 GB.

## 1.13.0 — 2026-08-13

### Added

- **Enable Custom Experience? — one question, and everything behind it.** The installer now asks once, before it downloads anything, whether your server should play the way the original files do or the friendlier way this project has been using. It is **off** by default and nothing changes for anyone who says no. Say yes and you get: items and Yang picked up from twice as far away, a horse that always comes when you call it, no waiting until tomorrow between the Horse Medal steps, bonus drops on 283 metin stones and bosses, Musk Oil stocked in the General Store, High Risk offered to your players, and everyone 20% faster on foot. Rates are untouched and so are your existing characters.
- **It is written down, and it is replayable.** The answer is kept as `M2_CUSTOM_EXPERIENCE` in your server's `.env`, so every later update rebuilds the server you have instead of the one the defaults would produce, and the update command the panel shows you names it. For an unattended install, `--custom-experience` / `--no-custom-experience` on Linux, `-CustomExperience` / `-NoCustomExperience` on Windows, or `M2_CUSTOM_EXPERIENCE=1` in the environment. `--yes` takes the default and leaves it off. Everything behind the switch now lives in `files/custom/` and is applied to the server tree at every build, so it survives updates instead of being quietly reverted by the next one — which is exactly what used to happen to changes made by hand.
- Turning it on also turns High Risk on and sets the movement-speed bonus to 20%, as starting points rather than as locks: `M2_HIGH_RISK=0` and a movement-speed bonus you have chosen yourself both still win, and both are carried forward untouched by later updates.
- **High Risk — a mode your players choose, at level 15.** They are asked once, shortly after they reach it: live dangerously, or carry on as before. High Risk means anyone may attack and kill them, anywhere, their own empire included, and nobody is punished for doing it. In exchange they earn 50% more experience and find 50% more drops — and when they die they drop items the way the cruellest characters do, out of their bags and off their bodies, using the game's own Cruel rules rather than a new one. No Risk changes nothing at all.
- **It is never a trap.** A character in High Risk is drawn the way a player-killer is drawn, to themselves and to everyone around them, so nobody is in it without knowing and nobody attacks one by accident. There is a line in the chat window at every login while it is on, and the choice can be reversed as often as they like at any Guardian or City Guard.
- Town safe zones still protect everyone, High Risk included — killing inside one would break every shop standing in it. Characters below level 15 and game masters are outside the mode entirely.
- To run a server without it, delete `files/high_risk.quest` before assembling, or set `M2_HIGH_RISK=0`. The rest of the change does nothing on its own: without that file no character can enter the mode and every check falls back to the ordinary rules.
- **The browser client keeps what it downloads.** It used to fetch half a gigabyte of game data in the background and lose it again by the next visit — not because the server said so, but because a browser's ordinary cache is one shared pool that the game's own reading pushes things out of. It now keeps that data in storage of its own. Measured: 144 downloads on a first visit, **one** on the second.
- **If it crashes, it offers to say so.** A dialog appears with what went wrong, a box to describe what you were doing, and a button to send it. Nothing leaves the browser unless that button is pressed, and the complete report can be read first. Account name and password are not in it — they are not reachable from the page at all.
- **Playing in the browser fills the window.** The game followed a fixed 1024×768 and let the browser stretch it. It now follows the window, including full screen, and remembers if you pin a resolution instead.

### Fixed

- **The Drachenhort quest paid out its own fee.** Handing in the items credited 150,000 Yang instead of charging it, and it could be repeated. Anyone who found it had unlimited money. This one is not behind the Custom Experience switch and never will be: it is a hole in the shipped files, it is open on every server running them untouched, and every server gets it closed.
- **A character with a friend, or in a guild of more than one, could not play in the browser.** The screen stayed grey at login and never finished loading. One line of the interface used a variable name that Python 3 no longer allows there, and it ran on every friend and every guild member.
- **A script error no longer takes the whole browser client down with it.** It is written to the console and the game carries on — which is how the bug above was found in seconds after days of guessing.
- Skills and emoticons can be dragged onto the quick slots again, and from one slot to another.
- Selling something no longer quotes a price with decimals in it. The price charged was always correct; only the confirmation was wrong.
- A weapon no longer stays in your hands after you take it off, and a metin's aura stays on the metin instead of occasionally landing on the player.
- Items and Yang are picked up from 600 units away on foot and 800 on a mount, instead of 300. With the Custom Experience on.
- Blessing Scrolls and Bravery Capes stack, along with eight more items that claimed to and never did — including Blessing Marbles and Scrolls of Correction. Every server gets this: the rows say they stack and then do not, which is a mistake in the table rather than a decision.
- The Horse Medal quests no longer make you wait between steps, and characters already waiting are released instead of serving out the rest. With the Custom Experience on.
- The Musk Oil quest points at the General Store, where the oil is actually sold, instead of an Item Shop this server does not have. With the Custom Experience on, which is also what puts the oil on the counter — the two go together, or the quest would send players to an empty shelf.
- Seven quests handed out rewards that did not match what they promised — among them one that skipped its Yang, experience and level-up entirely. Every server gets these: in each case the quest contradicts its own text, and the pack itself says which side was meant.

## 1.12.2 — 2026-08-12

### Fixed

- **The installer survives being run from a directory that was deleted.** Uninstall, then reinstall from the same terminal, and the installer used to stop three minutes in with `fatal: Unable to read current working directory` — which reads like a network problem and is not one. It now says what actually happened and carries on. The same protection covers the installer's own doing: it refreshes its checkout by deleting it, which pulled the rug out from under anyone who happened to be standing in it.
- [UNINSTALL.md](UNINSTALL.md) now says to leave the directory before deleting it, and shows that error so it can be recognised.

## 1.12.1 — 2026-08-12

- **[UNINSTALL.md](UNINSTALL.md)** — how to remove an installation completely and put a fresh one in its place. It says which of the five places an installation lives in can be thrown away freely, which one holds every character on your server, and how to save that one first. It also lists the three cheaper things to try before wiping anything.

## 1.12.0 — 2026-08-12

### Added

- **A second place to download from.** When MEGA answers `509 over quota` — the share's daily allowance, spent by other people, nothing to do with your machine — the installer now moves straight on to another copy of the same archive instead of stopping. It is the identical file and it is checked against the same checksum, so an install that used to mean coming back in a few hours carries on within seconds. This covers the three big downloads: the browser client's data, the desktop client, and the server files.
- Nothing to set up. The links travel in `artifacts.json` and an update picks them up. A link you supplied yourself is still tried first, and a fallback is only ever reached after the one before it has already failed.

## 1.11.13 — 2026-08-12

### Fixed

- The installer no longer tells you to copy the browser client onto the panel's volume by hand. It printed `docker compose cp ./browser panel:…` whenever playing in the browser was switched on — a leftover from before 1.11.0, when that really was the only way. Since then the installer fetches and installs the client itself, and the command it printed can only fail: there is no `./browser` directory in an install, so it answers `lstat …/browser: no such file or directory`. Reported by an operator who did exactly what the installer told him to.
- The same instruction is corrected in the Docker README, where it also pointed one directory too high — a client copied there is invisible to both the panel and nginx, which serve `browser/current`.

## 1.11.12 — 2026-08-12

### Added

- **A server-wide movement-speed bonus.** Set `M2_MOVE_SPEED_BONUS` in `.env` to a percentage and every character gets it at login — no item, no button, nothing for players to know about. `0` is off, which is what a server that says nothing gets. Changing the number takes effect at each character's next login; nothing has to be cleaned up.

## 1.11.11 — 2026-08-12

- The "What is this?" box on the front page says you can play in the browser — on servers that offer it — and no longer explains the panel to people who are looking for the game.

## 1.11.10 — 2026-08-12

- The front page puts the game account first, then the ways to play in one frame: **JETZT IM BROWSER SPIELEN**, a line saying **ODER**, and the download with its steps.
- The account card says what an account is for and that the same one works in the browser and in the download.

## 1.11.9 — 2026-08-12

- After registering, a player is shown the ways into the game this server really offers — the browser, the download, or both side by side with **ODER** between them.
- The browser card stands out and its button opens the game in a **new tab**, so the panel stays open behind it.

## 1.11.8 — 2026-08-12

- You are asked which clients you want **before** anything large is downloaded, instead of after.
- The Windows installer downloads only the server too, about 220 MB instead of 1.6 GB.
- The release notes below are written in plain language.

## 1.11.7 — 2026-08-12

- The Windows installer no longer stops at the question about which clients to install.
- A new install downloads only the server, about 220 MB instead of 1.6 GB.
- The desktop client is fetched from its own download.

## 1.11.6 — 2026-08-12

- The admin panel starts again on servers that use a domain name.

## 1.11.5 — 2026-08-12

- **Play in Browser** hands out a link that works on servers with HTTPS.
- The client download is fast again and no longer blocks the panel while it runs.

## 1.11.4 — 2026-08-12

- Updating now really does update the browser client.
- The installer writes its web-server settings again.

## 1.11.3 — 2026-08-12

- The browser client starts instead of stopping at "starting…".
- The browser client reaches servers that use a domain name.
- **Play in Browser** works on servers with HTTPS.
- The game stutters far less in the browser.
- A file that was briefly missing is no longer remembered as missing for a year.

## 1.11.2 — 2026-08-12

- The **Play in Browser** card appears in the panel again.

## 1.11.1 — 2026-08-12

- A server that chose the browser client now actually gets one.

## 1.11.0 — 2026-08-11

# 🌍 Play in the browser — no download

## Your players click a link and they are in the game. No client to download, no installation, nothing to set up on their side.

The installer asks which clients you want to offer — browser, desktop, or both
— and fetches only what you chose. Servers installed before this keep working
exactly as they are; the browser client is simply offered on your next update.

### Added

- Play in the browser: a link is all a player needs.
- The installer asks which clients you want and downloads only those.
- The browser client is installed for you, checked, and kept up to date.
- An update to the browser client is a small download, not the whole game again.
- Updating it cannot leave a player with half a client, and it can be undone.
- **Play in Browser** appears on the panel's front page once everything is ready.

## 1.10.0 — 2026-08-11

- The item search works on a local install and in older browsers, and says so when it cannot reach the server.
- Setting a level above what your server allows now tells you, instead of quietly doing nothing.
- The highest character level is 120 by default. Change `M2_MAX_LEVEL` in `.env` to raise it.
- **Game language** and **Admin passphrase** moved to the bottom of the admin page, under their own heading.

---

## 1.9.0 — 2026-08-11

### Added

- **Game language** on the admin page. The server files carry fifteen
  languages; pick one and the game speaks it — quest text, system messages,
  item and monster names. The game restarts for well under a minute.
- The download page says which language the game is in.
- After a switch, the panel shows players who already downloaded the game how
  to change their copy: one file to rename, nothing to download again.
- The client the panel hands out is built in the server's language.

### Fixed

- The patch log button had two translations, of which only the second was ever
  used.

---

## 1.8.0 — 2026-08-10

### Added

- **Admin passphrase** on the admin page, just under the introduction. Pick
  your own instead of the generated one; it takes effect straight away and you
  stay logged in.

### Changed

- The installer shows the admin passphrase every time it runs, including one
  you chose yourself in the panel, and never changes it behind your back.
- The introduction no longer says teleport and running speed are missing from
  a normal install. They are there; they need the player to be logged in.

---

## 1.7.0 — 2026-08-10

### Fixed

- The item search shows the names your game actually uses. The index had been
  built from the German name file while the server and the client use the
  English one, so nothing you saw in game matched what the panel offered.
- Searching for several words now needs all of them. "Full Moon Sword" no
  longer offers Half Moon Sword as well.
- Item numbers work in the search box, with or without the `#`. Typing `299`
  or `#299` finds that item, and `29` offers everything starting with it.

### Added

- **Show more** at the bottom of the item list, which used to stop at forty
  without saying so.
- German and Turkish item names are search keywords, so an item can be found
  by whichever of the three names you know.

---

## 1.6.0 — 2026-08-10

### Added

- Game master ranks on the player page. Pick a rank to give somebody the
  in-game admin commands, or set them back to a normal player. Granting takes
  effect immediately, even mid-game; taking a rank away applies at the player's
  next login.

### Fixed

- The game cores' admin interface had no password, which made an empty one
  correct. It now gets a generated password, like the others.
- The tutorial no longer claims that teleport and running speed do not work.

---

## 1.5.0 — 2026-08-10

### Changed

- Updating no longer asks for the address players connect to, or for your
  domain name. Both are kept from your settings. The address is only asked
  about when this machine has moved to a different one.

### Added

- `--no-domain`, to drop the domain a server was set up with and go back to
  plain HTTP on its address.

---

## 1.4.1 — 2026-08-10

### Fixed

- Running speed can be changed back. "Normal (reset)" resets it, a slower
  setting is slower than a faster one, and characters that were sped up before
  this version are put right the next time you set their speed.

---

## 1.4.0 — 2026-08-10

### Added

- The panel's in-game actions work. Items, yang and levels reach a character
  who is logged in straight away instead of at their next login, and teleport
  and running speed work at all.

Set `M2_INGAME_HELPER=0` in `.env` to leave the helper out.

---

## 1.3.4 — 2026-08-10

### Fixed

- The in-game helper no longer crashes the channel it runs on. It is still not
  installed by default — nobody has played on the fix yet.

---

## 1.3.3 — 2026-08-10

### Fixed

- The item search works in English and Turkish. It matched the whole box
  against German names, so "Full Moon Sword" found nothing while
  "Vollmondschwert" worked. It now matches word by word, translates the common
  ones, and ranks by how many words fit.

---

## 1.3.2 — 2026-08-10

### Fixed

- The in-game helper introduced in 1.3.0 disconnects the character it acts on.
  It is no longer installed. Items, yang and levels work as they did before
  1.3.0 — written to the account, visible at the next login — and teleport and
  running speed refuse instead of dropping the player.

**If you are on 1.3.0 or 1.3.1, update.** Until you do, avoid the buttons on a
character's page while somebody is playing on them.

---

## 1.3.1 — 2026-08-10

### Fixed

- Updating a server never picked up changes to the game itself. The source was
  staged once and reused, so the rebuild produced the same binaries and every
  C++ change since the install was dropped. If you updated to 1.3.0 and teleport
  still does not work, update again.

---

## 1.3.0 — 2026-08-10

### Added

- Teleport and running speed work. The helper that carries them out is now
  built and installed with the server.
- Items, yang and levels reach a character who is logged in straight away,
  instead of at their next login.

### Fixed

- On a Linux host the game could start with no quests loaded at all, and still
  report itself healthy. Staged quest files could carry permissions the server
  account could not read.

### Security

- The database function the helper uses accepts only statements against the
  panel's own queue table.

---

## 1.2.3 — 2026-08-10

### Fixed

- The update command shown in the panel now includes the options the server was
  installed with, such as `--domain` and `--email`. It previously showed the
  bare one-liner, which on the next update would have dropped the certificate.

---

## 1.2.2 — 2026-08-10

### Fixed

- Giving an item, yang or a level to a character who is logged in no longer
  claims they were not in game. It says the change was written to the account
  and appears at their next login.
- Teleport and running speed now say that nothing in the game answered and
  that nothing was changed, instead of suggesting the game server might be
  down.

---

## 1.2.1 — 2026-08-10

### Changed

- Removed the paragraph about the deleted `admin` and `test` accounts from the
  panel's introduction.

---

## 1.2.0 — 2026-08-10

### Changed

- The patch log splits the changelog at the version you are running: "What an
  update would bring" lists only the releases you do not have yet, and "What
  you are running" the rest. No release appears in both.

---

## 1.1.9 — 2026-08-10

### Changed

- The patch log shows the changelog once. When an update was available it was
  printed twice, under two headings, with the same releases in both.

---

## 1.1.8 — 2026-08-10

### Changed

- The update page now says to re-run the command that installed the server,
  and what an update leaves alone, instead of explaining a setting that is off.

---

## 1.1.7 — 2026-08-10

### Changed

- Removed the heading above the changelog on the patch log page. The file
  brings its own, so there were two.

---

## 1.1.6 — 2026-08-10

### Added

- A "Check for the latest version" button on the patch log page. It asks
  straight away instead of waiting for the daily check.

---

## 1.1.5 — 2026-08-10

### Changed

- The installer now ends with the panel address and the game, instead of
  opening with them and scrolling them off the screen.
- A local Windows install no longer prints an admin passphrase. The panel does
  not ask for one there.
- Removed the note about the shipped `admin` and `test` accounts. They are
  deleted during setup.

---

## 1.1.4 — 2026-08-10

### Fixed

- The Windows installer now updates an existing server as well, and shows the
  installed and published versions before asking. Re-running it previously
  re-applied the settings and restarted without fetching anything.

---

## 1.1.3 — 2026-08-10

### Changed

- When a server is already installed, the installer shows which version it is
  on and which one is published, then asks whether to update or to only
  re-apply the settings and restart.
- A server installed before versions existed is recognised as such and offered
  the update.

---

## 1.1.2 — 2026-08-10

### Fixed

- Re-running the installer on a server that was already installed now updates
  it. It used to rewrite the settings and restart the containers without
  fetching anything, so the server stayed on the version it was installed with.

---

## 1.1.1 — 2026-08-10

### Changed

- The patch log has its own card in the admin area, with a button. It used to
  be a grey line at the bottom of the page.
- The card highlights itself when a newer version is available.
- The front page shows the version number only. The patch log and the update
  notice are in the admin area.
- Shorter wording when the update check cannot reach the server.

---

## 1.1.0 — 2026-08-09

### Added

- A `VERSION` file and this changelog.
- An update check in the admin panel. It compares your version against the
  published one roughly once a day and tells you when a newer one exists, with
  the release notes for it. Can be switched off with `M2_UPDATE_CHECK=0`.
- A patch-log page in the panel, showing this file.
- The command that updates your server, shown on that page. The installer
  records it, so the panel shows the exact line for your install. Re-running
  the installer pulls the published version, rebuilds and restarts, and keeps
  your database, passwords and settings.
- A one-click update button on Linux. Off by default; see
  [UPDATING.md](UPDATING.md) to turn it on. On Windows the panel shows the
  command to paste instead.

---

## 1.0.0 — 2026-08-09

First public release. A Metin2 r40250 server that runs on ordinary Linux — or
on a Windows PC, for one person — installed with a single command.

### Added

- The Linux port, as a 109 KB patch over 28 files. A fresh copy of the upstream
  package plus this patch reproduces the running server byte for byte.
- Docker packaging: game cores, MariaDB, the admin panel and a client builder,
  in one compose stack.
- One-command installers for Linux and Windows. The Linux one publishes the
  game ports and opens the firewall; the Windows one binds everything to
  `127.0.0.1` and creates no firewall rule.
- The admin panel — English, German and Turkish. Server rates, giving items and
  yang, levels, password-reset links, registration and a client download.
- A client builder that patches the game to point at your server and offers it
  for download. On a Windows install it unpacks the game and puts a
  `Metin2 Singleplayer` shortcut on the Desktop.

### Fixed

- The client download returned 500 on every request. The panel could not create
  the file it counts downloads in.
- The panel showed a running server as offline, and the player count stayed at
  zero.
- "Give 1000 potions" silently gave 255.
- Items were refused to characters that had room for them: the panel searched
  one inventory page of 45 where r40250 gives four.
- The dashboard reported "the database cannot be reached" for a character who
  had never played.
- The game archive was downloaded twice, once for the server and once for the
  client.
- A path containing a space broke the client build.

### Security

- The shipped `admin` and `test` accounts are deleted during setup, together
  with their game-master entry.
- Download limits: three per address per day, plus a server-wide daily ceiling.
- Rate limits on registration, account login and the admin passphrase.
- Passwords are generated on the machine at install time.
