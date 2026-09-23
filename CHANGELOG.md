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

## 2.2.0 — 2026-09-23

Serwer i panel WWW; klient zostaje 2.0.26. Zawiera wszystko z 2.1.0.

### Nowa mapa dla botów: Doyyumhwaji (Ognista Ziemia)

Boty od 66 do 80 poziomu polują teraz także w Doyyumhwaji. Trafia tam co
trzeci z nich, reszta jak dotąd do lasów, na Górę Sohan i do Świątyni Hwang.

- Każde królestwo wchodzi tam, gdzie po teleporcie staje gracz z tego
  królestwa: Shinsoo w północno-zachodnim rogu mapy, Chunjo w
  południowo-zachodnim, Jinno na wschodnim skraju. I wraca przez własną
  bramę, do swojej drugiej wioski (Jayang, Bokjung, Bakra).
- Na mapie: Sługa Walczącego Tygrysa i Ognisty Duch (69), Walczący Tygrys
  (70), Płomień (71) i Ognisty Wojownik (72); co 28–42 minuty dwa elitarne
  potwory z obstawą (Piekielny Żarłacz Dusz, Płomienny Egzekutor); cztery
  Metiny Morderstwa (70), więc zagląda tam też część łowców Metinów; i mniej
  więcej co dwie godziny Ognisty Król (73) w jednym z trzech miejsc, na
  którego boty ruszają drużynami.
- Do gospodarki trafiają ulepszacze z tej mapy: Pazur Tygrysa, Płonąca
  Grzywa, Łuski Smoka i Symbol Wojownika.
- Boty polują w 27 miejscach wyznaczonych z samej mapy, także we wschodniej
  części, gdzie wchodzi Jinno.
- Mapa przeszła na ten sam rdzeń serwera, na którym żyją boty (game1). Gracze
  dostają się na nią jak dotąd.
- Mapa na żywo w panelu WWW ma Doyyumhwaji na liście map, z terenem.

Sprawdzone na naszym świecie testowym: boty każdego królestwa stają przy
swoim wejściu i wracają przez swoją bramę, a z każdego wejścia da się dojść
do wszystkich miejsc polowania. Boty 66+ nie polowały tam jeszcze na naszym
świecie, bo nie ma na nim tak wysokich botów.

### Boty w drużynie z graczem robią zakupy i ulepszenia w mieście

Bot w drużynie gracza nie robił żadnych sprawunków, żeby nie odchodził od
gracza, więc przez całą drużynę nie ulepszał ekwipunku, nie sprzedawał i nie
kupował mikstur („if you don't quit the party, the other 7 players won't
upgrade their equipment”, JFK). Teraz, gdy gracz z drużyny stoi w tej samej
wiosce, bot idzie do kowala, handlarzy i magazyniera jak każdy inny bot, a
gracz nie ściąga go z powrotem, dopóki nie skończy. Gdy gracz zmieni mapę,
bot idzie za nim od razu, jak dotąd. Stajenny, Biolog i wyprawy na inne mapy
dalej czekają do końca drużyny.

### Boty nie chodzą na rynek na darmo

Na linii 2.x stoiska botów to sklepy offline, a boty wciąż wybierały się na
„wycieczki na rynek” napisane dla zwykłych stoisk, których tu nie ma. Każda
taka wycieczka kończyła się niczym: w godzinę na jednym rdzeniu naszego
świata testowego było ich ponad 4 400, w tym około 580 kursów z drugiej
wioski do pierwszej i z powrotem, z których tylko co jedenasty kończył się
zakupem. Na czas wycieczki bot nie kupował nawet ze sklepów offline, które
miał tuż obok.

Teraz:

- w pierwszej wiosce bot kupuje ze stoisk w zasięgu, bez spacerów na środek
  rynku;
- z drugiej wioski idzie do pierwszej tylko po konkretną rzecz z konkretnego
  stoiska, której szuka i na którą go stać, a po dojściu idzie prosto do tego
  stoiska;
- rzecz, po którą bot już idzie, jest dla niego zarezerwowana: inne boty jej
  nie wybierają, więc kilka nie ściga się już o jedną linię;
- bot, który dojdzie do stoiska akurat wtedy, gdy właściciel je obsługuje,
  czeka chwilę zamiast odchodzić, a właściciel nie zaczyna obsługi własnego
  stoiska w trakcie swoich zakupów;
- bot, który nie znajdzie potrzebnego materiału, nadal woła o niego na czacie
  handlowym.

Na naszym świecie testowym zakupem kończy się teraz 87% kursów do pierwszej
wioski (wcześniej 9%), a kursów bez celu jest kilka razy mniej.

### Aktualizacja na Linuksie (update.sh)

`update.sh` rozpakowywał paczkę, w której jest on sam, nadpisując pliki w
miejscu. Uruchomiona powłoka czytała wtedy dalej już nowy plik, od środka, i
po udanej aktualizacji kończyła się błędem („Unterminated quoted string”,
kod 2) albo wykonywała kawałek przypadkowej linii (zgłosił Tyrion). Teraz
każdy plik jest podmieniany w całości, a skrypt jest wczytany do końca,
zanim cokolwiek podmieni.

Pierwsza aktualizacja do tej wersji jest jeszcze wykonywana przez stary
skrypt, więc może po zakończeniu pokazać ten sam błąd albo zbudować serwer
drugi raz. Serwer jest wtedy już zaktualizowany; od następnej aktualizacji
tego nie będzie.

### Rybak

Część botów nie mogła dojść do Rybaka: miejsce, do którego szły, wypadało na
odciętym kawałku terenu, więc sesja łowienia kończyła się od razu, za każdym
razem. Teraz taki bot podchodzi do Rybaka z najbliższego miejsca, do którego
da się dojść.

### Mikstury

Boty co chwilę przelewały mikstury z tylnego stosu do przedniego, nie
zwalniając przy tym żadnego miejsca w plecaku, dziesiątki tysięcy razy na
godzinę. Teraz łączą stosy tylko wtedy, gdy to zwalnia kratkę.

### Drobne

- Boty na nowym świecie potrafiły mieć na 1 poziomie po 2–4 bronie (zgłosił
  Iwakura): handlarz bronią sprzedawał im broń „awaryjną”, gdy nie trzymały
  nic w ręce, nawet jeśli w plecaku leżała broń domyślna albo broń ze
  skrzyni. Teraz bot najpierw zakłada to, co ma w plecaku, i kupuje broń
  tylko wtedy, gdy naprawdę nie ma żadnej do założenia.
- Bot idący do Lasu, Czerwonego Lasu albo Wieży Demonów miał nad głową
  „Szukam lepszego miejsca”; teraz pisze, dokąd idzie.

## 2.1.0 — 2026-09-23

Serwer i launcher; klient zostaje 2.0.26. Zawiera wszystko z 2.0.99.

**Kto ma ustawiony poziom trudności średni albo trudny:** od tej wersji ten
poziom obejmuje też księgi umiejętności (7 h albo 21 h czekania między
dwiema księgami tej samej umiejętności, dla graczy i dla botów). Jeśli tego
nie chcesz, wybierz poziom „Własny” i wpisz przy księgach 0. Na poziomie
łatwym, domyślnym, nic się nie zmienia.

### Czas czekania na księgi umiejętności w poziomie trudności

Od 2.0.12 każdą kolejną księgę dało się przeczytać od razu, więc Zwój
Egzorcyzmu nie miał nic do roboty. Teraz czas między dwiema księgami tej
samej umiejętności jest częścią poziomu trudności, osobno dla graczy
i osobno dla botów (pomysł drip9660):

- **łatwy**: bez czekania, jak dotąd;
- **średni**: 7 godzin;
- **trudny**: 21 godzin, tak jak w oryginalnej grze;
- **własny**: dowolna liczba godzin, osobno dla graczy i dla botów
  (0 = od razu, ułamki dozwolone).

Zwój Egzorcyzmu pomija to czekanie, także botom, które mają go w plecaku.
Obniżony czas działa od razu: nikt nie czeka dłużej, niż mówi aktualne
ustawienie. Ten sam czas obowiązuje przy Instrukcji Jazdy Konnej, tak jak
w oryginale.

### Poziom trudności w panelu WWW

- **Panel WWW ma kartę „Poziom trudności”** (strona z mnożnikami serwera):
  Biolog, Stajenny i księgi graczy oraz botów. Zmiana działa od razu, gdy
  ktoś jest w grze, i zostaje po restarcie serwera, dopóki nie zmienisz
  poziomu trudności w launcherze. Obowiązuje to ustawienie, które
  zmieniono ostatnio.
- **Okno poziomu trudności w launcherze ma dwa nowe pola**: księgi graczy
  i księgi botów.
- Przełącznik „Księgi umiejętności bez dobowej przerwy” na stronie AI
  zastąpił czas dla botów w poziomie trudności.
- Poprawione: w oknie poziomu trudności godziny z ułamkiem (np. 0,5)
  pokazywały się jako 0, a „Zastosuj” zapisywało to 0.

## 2.0.99 — 2026-09-23

Tylko serwer i launcher; klient zostaje 2.0.26. Zawiera wszystko z 2.0.98.

### Launcher rozpoznaje zepsuty dysk Dockera

Gdy Docker Desktop przy budowaniu serwera trafi na błąd zapisu, jego dysk
(plik docker_data.vhdx) przełącza się w tryb tylko do odczytu. Od tej chwili
każda próba kończyła się „kodem 1”, a Diagnostyka mówiła, że można uruchomić
serwer. Zgłosił to gracz, który w ten sposób pięć razy pobrał i podmienił tę
samą aktualizację.

- **Launcher sprawdza, czy da się zapisać na dysku Dockera**: w Diagnostyce,
  przed aktualizacją i przed każdym budowaniem. Gdy się nie da, mówi to wprost
  i podaje kroki naprawy, a aktualizacji w ogóle nie pobiera.
- **Diagnostyka ostrzega, gdy na dysku z Dockerem zostało mniej niż 15 GB
  wolnego miejsca.** Brak miejsca to najczęstsza przyczyna takiego błędu.
- **Ponowna aktualizacja tylko dokańcza budowanie.** Gdy pliki nowej wersji
  są już na dysku, a budowanie się nie udało, launcher nie pobiera ich
  drugi raz i nie robi kolejnej kopii plików.
- **Launcher nie gubi zapisanej wersji klienta**, gdy budowanie serwera nie
  doszło do końca. Wcześniej proponował potem aktualizację klienta, który
  był już aktualny.

### Paczka z logami

- **Nie zawiera hasła do panelu WWW.** Launcher wypisuje je po polsku, a
  filtr haseł rozpoznawał tylko angielskie wpisy. Panel działa domyślnie
  tylko na Twoim komputerze, więc nikt z zewnątrz nie mógł go użyć, ale
  hasło nie powinno trafiać do plików, które wysyła się innym.
- **Zawiera informację o wolnym miejscu na dyskach** i rozmiarze dysku
  Dockera, bo przy takich błędach to pierwsze pytanie.

## 2.0.98 — 2026-09-23

Serwer 2.0.98 i klient 2.0.26. **Zaktualizuj też klienta** („AKTUALIZUJ
wszystko” w launcherze). Kto gra po niemiecku, hiszpańsku, włosku,
portugalsku, rumuńsku albo turecku, bez nowego klienta nie ulepszy przedmiotu
u kowala. Zawiera wszystko z 2.0.97.

### Kowal i zablokowany ekwipunek w innych językach niż polski

Po oddaniu przedmiotu kowalowi nic się nie działo, a potem nie dało się ruszyć
w ekwipunku niczego: przesunąć, wyrzucić, użyć. Pomagało dopiero ponowne
zalogowanie. Zgłosili to gracze z kanału angielskiego („nothing happens when
I put the item on him”). Dotyczyło to wszystkich grających po niemiecku,
hiszpańsku, włosku, portugalsku, rumuńsku i turecku.

Przyczyna: w tych językach tekst „koszt ulepszenia” w pakiecie tłumaczeń
oczekiwał liczby, a gra podaje kwotę jako tekst („1.000 Yang”). Okno kowala
wywracało się, zanim się pokazało. Serwer w tym czasie czekał już na
odpowiedź z tego okna i blokował cały ekwipunek.

- **Klient 2.0.26 ma poprawione teksty we wszystkich językach.** Sprawdziliśmy
  cały pakiet tłumaczeń tak, jak czyta go gra, i poprawiliśmy 89 tekstów
  w siedmiu językach, także angielskim. Każdy z nich mógł wywrócić jakieś
  okno albo komunikat: podnoszenie yang, zrzut ekranu, przycisk kupna
  w ItemShopie, cofanie punktów statystyk, odnawianie Ducha Smoka w gildii,
  długość ryby, umiejętności drużyny, sprzedaż kilku sztuk u handlarza.
- **Okno kowala nie zablokuje już ekwipunku.** Jeśli mimo wszystko się nie
  otworzy, klient sam anuluje ulepszanie, więc serwer odblokowuje ekwipunek.
  Koszt i szansa pokażą się wtedy jako zwykłe liczby.
- **Po stronie serwera:** kto odejdzie od kowala dalej niż 20 metrów, odzyskuje
  ekwipunek, nawet ze starym klientem. Dopóki ktoś nie zaktualizuje klienta,
  wystarczy odejść od kowala, bez wylogowywania.
- **Klient po hiszpańsku w ogóle się nie uruchamiał.** W hiszpańskim pliku
  tłumaczeń brakowało znaku końca ostatniej linii. Gra obcinała przez to
  ostatnią literę i przed ekranem logowania pokazywała błąd. Poprawione.
- Po angielsku: szept do osoby, którą masz zablokowaną, pokazuje teraz
  komunikat zamiast błędu w tle.

### Launcher nie przycina się przy zmianie rozmiaru okna

Przeciąganie krawędzi nowego okna launchera było bardzo powolne: przy każdym
ruchu myszy okno przeliczało i od nowa rysowało tło oraz wszystkie przyciski.
Tło jest teraz przygotowane raz dla danego rozmiaru, a układ przelicza się
dopiero po puszczeniu krawędzi. W naszym pomiarze jeden krok przeciągania trwa
teraz około 29 ms zamiast 378 ms. Wygląd okna jest taki sam jak wcześniej.

### Czat z botami 1.1b — ĹŌŞƬĒĶ (l0st3k)

Druga wersja systemu rozmów od **ĹŌŞƬĒĶ (l0st3k)**, wydana dzień po pierwszej.

- **Bot wie, co ma na straganie.** Zapytany o przedmiot, który ma wystawiony
  (na przykład „masz ku aura miecza?”), odpowiada, że go ma, w którym mieście
  stoi sklep i za ile. Dotyczy to też sklepu offline, a na tej linii serwera
  każdy stragan bota jest sklepem offline. Wersja 1.1a przeszukiwała tylko
  zwykłe stragany i na takie pytanie odpowiadała, że nic nie ma.
- **Słownik metinowca.** Bot rozumie skróty, którymi piszą gracze: KK (Kawałek
  Klejnotu), KD (Kamień Duszy albo Kamień Duchowy), KU (księga umiejętności),
  FMS, RIB, 12D, bodzio, ebo, PD i inne. Rozumie też nazwy map (M1, M2, M3,
  V1, V2, DT, Sohan, Czerwony Las) i kwoty (500k, 2kk, 1,5kk, 300 tys).
- **Ceny i targowanie.** Na „ile chodzi FMS?” bot podaje najniższą cenę
  z wystawionych straganów, a gdy nikt tego nie wystawia, cenę, za którą
  ostatnio się sprzedawało. Na „sprzedasz mi FMS za 2kk?” porównuje ofertę
  ze swoją ceną: zgodzi się, powie, że to za mało, albo że na straganie stoi
  taniej.
- **Wołaj.** Gdy na Wołaj napiszesz „Kupię …”, na priv odpisze bot, który ma
  to na straganie. Teraz dotyczy to także sklepów offline i skrótów ze słownika
  („Kupię FMS”, „Kupię KK”). Na „Sprzedam …” odzywa się bot, który tego
  potrzebuje, i on również rozumie skróty.
- Krótkie zwroty z gry: gz, gl, brb, gotowy, „ksujesz”.

Nasze poprawki do wersji autora, znalezione w teście na żywo:

- Nazwa przedmiotu, w której jest słowo ze słownika, znów działa. „Szpon”
  to w słowniku Miecz Szponu Ducha i przez to „Kupię szpon wilka” zostawało
  bez odpowiedzi, choć bot miał Szpon Wilka na straganie.
- „Kupię księgę misji” na Wołaj jest szukane jak zwykły przedmiot. Wcześniej
  „księga” oznaczała wyłącznie księgę umiejętności, a taka linia przepadała
  bez śladu.
- Na „gdzie masz stragan?” bot mówi, gdzie ma stragan, zamiast szukać na nim
  przedmiotu o nazwie „gdzie”.
- Dwuliterowe skróty („Kupię KK”) przechodzą przez Wołaj, a „Sprzedam …”
  korzysta ze słownika.
- Wolne miejsca w plecaku są liczone tak jak w grze, a absurdalnie duża kwota
  w targowaniu nie psuje rachunku.

Sprawdzone na naszym serwerze testowym przy około 850 botach. W dwóch
przebiegach boty zadały sobie ponad 900 pytań, także o przedmioty, które
naprawdę stoją na ich straganach. Po poprawkach wszystkie 35 wołań „Kupię …”
dostało odpowiedź od bota, który ma dany przedmiot. Odpowiedź na wołanie
zajmuje serwerowi około 2 ms, a żaden rdzeń nie padł.

Czego bot jeszcze nie rozumie: pytanie „masz księgę misji?”, „masz marmur
polimorfii?” albo „masz kamień duchowy?” bierze za rozmowę o umiejętnościach
albo o Metinach. Działa skrót („masz km?”, „masz kd?”) i wołanie „Kupię …”.

---

## 2.0.97 — 2026-09-23

Serwer 2.0.97. Klient zostaje przy 2.0.25, więc aktualizacja klienta nie jest
potrzebna. Zawiera wszystko z 2.0.96.

### Boty odpisują na priv — system rozmów od ĹŌŞƬĒĶ (l0st3k)

System napisany przez **ĹŌŞƬĒĶ (l0st3k)** sprawia, że bot odpisuje na szept
tak, jak zrobiłby to gracz. Całość jest w C++ i działa lokalnie w rdzeniu gry,
bez żadnej zewnętrznej usługi. Wchodzi w całości, z kilkoma naszymi poprawkami. Co
potrafi:

- **Rozumie zwykłe pytania**: co robisz, gdzie expisz, jaki masz poziom, ile
  masz yang, jak drop, czy jest w PT albo w gildii, co ma w sklepie, czy
  sprzeda lub kupi przedmiot albo księgę („masz ku aura miecza?”). Rozmawia
  też o luźnych rzeczach: czy lubi zimę, czego się boi, czy jest botem.
- **Odpowiada tym, co naprawdę robi w tej chwili**: mapą, celem, poziomem,
  bronią i zbroją, drużyną, gildią i nastrojem z systemu osobowości Iwakury.
- **Pamięta rozmowę**: rozumie dopytania („a dlaczego?”, „a potem?”, „a ty?”),
  nie powtarza w kółko tych samych zdań, a stałego rozmówcę z czasem traktuje
  jak znajomego.
- **Odpisuje po chwili**, jak człowiek (około sekundy). Kilka szybkich
  wiadomości łączy w jedną odpowiedź, a na zalew wiadomości odpowiada krótko,
  zamiast gubić wątek.
- **Czasem sam zagada** do gracza, z którym już wcześniej rozmawiał.

„Kupię …” i „Sprzedam …” działają jak dotąd, czyli odpowiada bot, który ma
towar. „Luruj” też działa bez zmian.

Nasze poprawki: bot wie o swoim sklepie offline. Mówi, w którym mieście stoi
sklep i co na nim jest, a na „sprzedasz mi …” odpowiada ceną z własnych
półek. Wcześniej na tej linii serwera każdy bot odpowiadał, że straganu nie
ma. Poprawiliśmy też liczenie wolnych miejsc w plecaku, tak jak liczy je gra.

Dla administratora: plik `playerbot_conv_debug` w katalogu rdzenia włącza
szczegółowe linie `PLAYERBOT_CONV` w logu, a `playerbot_conv_noinit` sprawia,
że boty nie zaczynają rozmów same. Oba działają bez restartu.

Sprawdzone na naszym serwerze testowym przy około 850 botach: przez 12 minut
boty zadały sobie nawzajem 885 pytań, w tym serie po sześć wiadomości naraz.
Boty udzieliły 542 odpowiedzi (130 z nich łączyło kilka wiadomości), a żaden
rdzeń nie padł. Z żywymi graczami system działa już na serwerze autora.

### Bot po śmierci nie biegnie w ścianę

Po wstaniu bot odchodzi kawałek od miejsca śmierci. Kierunek był losowany
z ośmiu, a w wąskich komnatach Lochu Małp większość z nich to ściana. Bot
planował wtedy drogę donikąd przy każdym ruchu, aż się wyleczył, i były to
najczęstsze linie w syserr serwera. Teraz wybiera pierwszy kierunek, do
którego naprawdę da się dojść, a gdy żadnego nie ma, zostaje w miejscu,
niewidzialny, i się leczy.

## 2.0.96 — 2026-09-23

Serwer 2.0.96, klient 2.0.25. Zawiera wszystko z 2.0.95. **Zaktualizuj też
klienta** (przycisk „Aktualizuj klienta”): Auto Łowy 2.0 potrzebują nowego
`metin2client.exe`.

### Auto Łowy 2.0 od Colide'a

Colide przebudował Auto Łowy i przysłał swój kod — łącznie z częścią dla
klienta w C++. Wchodzi w całości, a do tego nasze poprawki. Nowości:

- **Moby, Metiny, Bossy** — trzy przełączniki w oknie ustawień decydują, co
  postać atakuje. Gdy włączone są wszystkie, serwer zawsze wybiera najpierw
  bossa, potem Metina, a dopiero potem zwykłe potwory. Domyślnie włączone są
  Moby i Metiny.
- **Po zabiciu potwora postać od razu bierze następny cel.** Martwy potwór
  leży na mapie jeszcze 2–3 sekundy, a postać czekała przy nim na kolejną
  odpowiedź serwera. Teraz klient sam widzi, że potwór padł, i od razu prosi
  o następny cel.
- **Łuk w ręce strzela z daleka** — do 2400 jednostek. Z mieczem lub
  sztyletami postać podchodzi jak dotąd.
- **Zasięg widać na ziemi.** Przy otwartym oknie ustawień gra rysuje okrąg:
  niebieski idzie za postacią, a zielony, gdy włączone jest „Wracaj”, stoi w
  miejscu startu z krzyżykiem w punkcie powrotu. Zasięg ustawia się suwakiem
  (300–5000).
- **Bez „Wracaj” zasięg idzie za postacią**, zamiast trzymać się miejsca
  startu.
- **Okna pamiętają swoje miejsce** (`autohunt/config.cfg`), a każda postać ma
  swoje ustawienia w `autohunt/postacie/<nick>.cfg`. Ustawienia z poprzednich
  wersji są czytane do pierwszego zapisu, więc nic nie ginie.

Okrąg, zasięg łuku i puszczanie trupa to funkcje nowego exe klienta. Ze
starszym exe Auto Łowy działają bez nich — łucznik ze szkołą łuku nadal
strzela z daleka.

Nasze poprawki w tej wersji: mikstura, której nie ma w plecaku, jest szukana
raz na sekundę zamiast w każdej klatce (przeszukanie czterech stron plecaka
kosztowało klatkę), niebieska mikstura jest rozpoznawana po tabeli
przedmiotu, a próg HP po wskrzeszeniu nie może przekroczyć 100%.

### Szybkie podnoszenie działało tylko do pierwszego teleportu

Zgłosił **GorącyDelfin**: klawisz podnoszenia wszystkiego naraz (`)
działał tylko w M1. Zegar klienta liczy od zera przy każdym połączeniu z
serwerem, czyli po każdym teleporcie, a klawisz pamiętał czas sprzed
teleportu — więc blokował się na tyle, ile postać grała przed przeniesieniem.
To samo dotyczyło przycisku „Scal i uporządkuj”, przenoszenia stosów do
magazynu, pompy cen w sklepie, tytułów botów i Auto Łowów. Wszystkie liczą
teraz czas zegarem, który teleport nie zeruje.

### Boty w drużynie gracza trzymają się gracza

Zgłosił **SIZOWSKI**: szamani dodani do drużyny robili swoje, zamiast iść za
graczem, więc nikogo nie buffowali. Bot szuka potworów do 6000 jednostek od
siebie, a za graczem idzie tylko wtedy, gdy nie walczy. Bot, który zawsze
miał następnego potwora w zasięgu, odchodził więc paczka po paczce i nie
wracał. Teraz bot w drużynie gracza atakuje tylko potwory **do 2500 jednostek
od gracza** — chyba że potwór bije jego samego, bo obrony nic nie wstrzymuje.
Gdy nie ma z czym walczyć, wraca do gracza i go buffuje.

### Bot bez broni na 30 poziom nie kręci się już w kółko przy M3

Zgłosił **urtopy**, na przykładzie bota Champion: bot bez broni na 30 poziom,
któremu skończył się czas wizyty w M3, szedł do M2 i po dwóch sekundach
wracał do M3 — bez końca. Status pokazywał przy tym cel planera („Zbieram dla
Biologa: Ząb Orka”), choć Ząb Orka nie leży w M3. Teraz po wizycie, która
skończyła się bez broni, bot wraca do M3 dopiero po 45–90 minutach i w tym
czasie załatwia resztę (dolina, koń, Biolog). W M3 jego status to
„Szukam broni na 30 poziom na M3”.

### Nowy wygląd launchera

Launcher dostał nowy wygląd z tłem:

- menu po lewej: Pulpit, Świat i boty, COOP, Baza danych, Logi i
  diagnostyka;
- pod menu wersje serwera, launchera i klienta oraz przycisk „Postaw
  kawkę”;
- w nagłówku link do metin2singleplayer.com i przełącznik języka;
- akcje jako karty z opisem;
- log operacji, który można zwinąć;
- przycisk rat EXP / DROP / YANG, który otwiera edytor rat w panelu WWW.

Wszystkie przyciski robią dokładnie to samo co wcześniej, z tymi samymi
oknami potwierdzeń. Zniknął tylko „URUCHOM DOCKER” — Dockera, jak dotąd,
uruchamia przycisk GRAJ. Okno mieści się też na mniejszym ekranie (laptop
1366×768). Gdyby nowy wygląd nie uruchomił się na danym komputerze, launcher
powie o tym i przy następnym uruchomieniu otworzy się w starym wyglądzie
(zostawia plik `.m2launcher-classic-layout`; jego usunięcie to kolejna próba).
Tło to grafika w klimacie Metin2, nie oficjalny render z gry.

---

## 2.0.95 — 2026-09-22

Serwer 2.0.95, klient 2.0.24. Zawiera wszystko z 2.0.94. Większość tej wersji
to **Community Patch 2 od Iwakury** — zmiany osobowości opisane niżej — oraz
najpoważniejsze błędy z kanału „błędy i bugi".

### Boty wracają z odpoczynku („Boty grają jak żywi ludzie")

Zgłosił **Kuszaa**, z wykresem: po włączeniu przełącznika boty schodziły z
serwera na odpoczynek i świat pustoszał, bo odpoczynek trwa kilka godzin, a
sesja jedną–dwie. Rachunek dawał to samo co jego wykres — po kilku godzinach
większość kohorty odpoczywała naraz. Teraz odpoczywa najwyżej **40% botów**
jednocześnie; bot, którego sesja kończy się, gdy limit jest pełny, gra dalej
jeszcze 10–40 minut. Spis w logu (`PLAYERBOT_LIFE: census`) podaje też
`held_on=` i `cap=`.

### Wojny gildii

Zgłaszało wielu graczy, najdokładniej **prodnathin** (zrzuty z 21 i 22
września) i **Dixdros**: boty na wojnie tylko rzucały umiejętności, nie biły
zwykłym atakiem, stały w ścianach i uciekały z pola. Przyczyny były trzy:

- **Zwykły atak bota działał tylko w pojedynku.** Wróg z wojny gildii i
  gracz, z którym bot walczy w ramach Anti-PK, nie byli przeciwnikami w
  pojedynku, więc bot bił ich samymi umiejętnościami, a między nimi stał.
  Teraz zwykły atak dosięga wszystkich trzech; o tym, czy cios może paść,
  nadal decyduje silnik.
- **Pole bitwy nie miało granic.** Bot gonił wroga przez całą mapę gildii, w
  tym w skały między polem a mostem. Teraz bot walczy tylko w promieniu 1800
  jednostek od miejsca zbiórki; kto z niego wyjdzie, wraca, a wróg poza polem
  nie jest celem.
- **Przełącznik wojen w panelu klasycznym kłamał.** Po każdym odświeżeniu
  strony pokazywał wojny jako włączone, a następny zapis dowolnej zmiany
  włączał je z powrotem — więc wyłączenie wojen z panelu nigdy nie trzymało.

### Sklepy offline: wygasłe sklepy i sklepy-widma

Zgłosili **ElGrande** i **Iwakura**: sklep, który wygasł, nie dawał się otworzyć
ponownie, a w M1 i M2 stały sklepy, w które nie dało się kliknąć. To jedna
przyczyna: sklep offline liczy czas na dwóch zegarach — rdzenia gry i rdzenia
bazy — a o wygaśnięciu decyduje ten drugi. Zegar obciążonego rdzenia gry
chodzi szybciej (zmierzyliśmy 26,6 taktu na sekundę zamiast 25), więc sklep
dochodził u niego do zera wcześniej. Przez ten czas stał na mapie, ale kliknięcie
nic nie robiło, a odnowienie — już po pobraniu opłaty — rdzeń bazy odrzucał bez
słowa. Na naszym świecie testowym tak utknęło 368 odnowień botów. Teraz o zerze
decyduje wyłącznie rdzeń bazy, a odnowienie bez odpowiedzi jest po pięciu
minutach porzucane i ponawiane.

### Czerwone Konary u Biologa

Zgłosił **! AmaZing ¡**: zadanie Biologa z Czerwonymi Konarami (85 poziom)
nie dawało się oddać. Paczkowy quest sprawdzał w plecaku **inny przedmiot**
(30166) niż ten, którego żąda (30167). Wysyłamy poprawioną kopię questa.

### Pilne z Community Patch 2

- **Exp w drużynie „Na równym"** (**SIZOWSKI**): postać gracza nie dostawała
  doświadczenia za potwory zabite przez boty z drużyny, dopóki sama czegoś nie
  uderzyła. Przyczyną była blokada doświadczenia Grindera: silnik pomijał
  zablokowanego napastnika w podziale łupu *całkowicie*, także dla drużyny.
  Teraz jego ciosy liczą się dla drużyny, jeśli ktokolwiek w niej może zdobywać
  doświadczenie.
- **Suwak wędkowania** (**blasty**): przy 200% liczba rybaków nie rosła, bo boty
  na mapach frontu (Dolina Orków, pustynia i dalej) nigdy nie ruszały na ryby
  — łowisko jest w pierwszej wiosce. Teraz bot z frontu, który ma wędkować,
  wraca na nie jak na każdą inną sprawę w mieście.
- **Fasolki Zen** nie trafiały na sklepy: bot trzymał cały swój stos, więc u
  blastyego w plecakach leżało 18 000 fasolek, a na ladach żadna. Teraz bot
  zatrzymuje losowo od 10 do 15 sztuk na trening umiejętności do P (Kamienie
  Duchowe kosztują rangę, a fasolka ją odbudowuje) i wystawia nadwyżkę po
  5 sztuk.

### Community Patch 2 — zmiany osobowości (Iwakura)

**Broń na 30 poziom.** Bot od 30 poziomu bez broni na 30 poziom swojej klasy
— Miecza Pełni Księżyca, Ostrza z Czerwonej Stali, Łuku z Rogu Jelenia,
Kozika Czarnego Liścia, Antycznego Dzwonu albo Wachlarza Jesiennego Wiatru —
kupuje ją na rynku przy najbliższej wizycie w mieście, niezależnie od swojego
poziomu. Z ofert wybiera tę z **najwyższymi średnimi obrażeniami**, a cena
rozstrzyga dopiero remis („12% za 300k albo 26% za 450k — weźmie droższą").
Przy włączonych osobowościach na zakup i ulepszanie razem idzie najwyżej 60%
jego yang, a ulepsza ją jak Perfekcjonista: 60% botów do +6, 13% do +7, 8% do
+8, 5% do +9 (pozostałe 14%, których patch nie przydzielił, też do +6), a przy
każdej kolejnej wizycie dalej, aż do +8 (+9 dla tych, które wylosowały +9). Ta
broń jest pierwsza w kolejce do kowala. Broń z 34%+ średnich idzie od +3 pod
Zwój Błogosławieństwa, a taką z rynku bot dokupuje, jeśli ma wyższe średnie
niż każda, którą już ma — w ramach tego samego budżetu. Bot, który może ją
kupić, nie jedzie już po nią farmić na M3.

**Grinder.** 13% botów w ogóle nie zatrzymuje się na blokadach poziomu i
ulepsza postać głównie przedmiotami z rynku. Kolejne 10% może porzucić styl
Grindera: na każdym tierze, gdy jego sprzęt spełni Prawo Awansu, ma 33% szans,
że przestanie się blokować na zawsze.

**Bonusy.** Poprawka błędów z Patcha 1, przez które boty bonowały
przypadkowo:

- Buty, naszyjnik i bransoleta przed 45 poziomem są bonowane **na każdym
  poziomie ulepszenia** — wcześniej próg +4 wykluczał prawie całą biżuterię
  młodego bota.
- Są zmieniane, dopóki nie mają wymaganych linii: PŻ i co najmniej jednej z
  pozostałych (buty: kryt, szybkość ataku; naszyjnik: kryt, przeszywanie;
  bransoleta: % obrażeń do PŻ, przeszywanie, silny na zwierzęta, silny na orki).
- Broń dostaje kamienie dopiero, gdy dwa z tych trzech przedmiotów mają PŻ, a
  jej celem jest **30%+ średnich**.
- **Wymiana sprzętu**: nowy przedmiot czeka w plecaku, dopóki jego bonusy nie
  przebiją bonusów noszonego — i jest bonowany tam, gdzie leży. Czeka tylko
  wtedy, gdy bot ma kamień, który może na nim użyć, a sam przedmiot nadaje się
  do bonowania (od +4; buty, naszyjnik i bransoleta młodego bota na każdym
  plusie). Inaczej bot zakłada go od razu, jak dotąd — bez tego zbroja +0
  kupiona u handlarza czekałaby w plecaku bez końca.

**Loch Małp (Tier 4).** Dropków medali jest około cztery razy więcej niż
dotąd. Pod osobowościami wylosowany dropek medali zostawał Wędrowcem, więc
medale dropiło tylko 26 botów na tysiąc; teraz zostaje dropkiem, a kolejne
14% botów bez innej roli jest nim dodatkowo. Na naszym świecie testowym
dropkiem medali jest teraz co dziesiąty bot spoza kohorty operatora.
Blokują się na poziomie swojego lochu (33), nie na blokadzie Grindera, i nie
„awansują" z niej na Zdobywcę; panele pokazują im właśnie tę blokadę. 3% z
nich zbiera medale tak długo, aż starczy im na broń i zbroję +9 oraz hełm i
tarczę +7 na swój poziom — wtedy kończą z lochem i kupują: najpierw broń od
30 poziomu, poniżej niego zbroję.

**Księgi umiejętności.** Bot w mieście w roli Handlarza kupuje księgi
swoich umiejętności na poziomie M (do G) bez względu na stan sprzętu, za
najwyżej 30% yang na wizytę, i czyta je od razu. Księgi innych klas wystawia
chętniej: trafiają na ladę przed zwykłym towarem, a już trzy takie księgi są
powodem, by otworzyć sklep.

**Szkatułki z bossów** (50070–50082, 50090, 50097, 50098) podnoszą nastrój
bota o jeden stopień.

**Łup przed AFK.** Bot w słabym nastroju przed odejściem od klawiatury
najpierw podnosi z ziemi swój łup — najwyżej przez półtorej minuty, żeby łup,
do którego nie da się dojść, nie odbierał mu nawyku na zawsze.

**Inflacja.** Każde 2,5 miliarda yang zgromadzone łącznie na wszystkich
postaciach serwera podnosi ceny z cennika o 5%, ponad przelicznik dropu yang.
Suma jest liczona z bazy co dziesięć minut; sklepy dostosowują ceny, gdy
inflacja przeskoczy próg. W logu: `PLAYERBOT_MARKET: world yang=... inflation=...`.

**Hazardzista i Lista Przydatnych Przedmiotów.** Lista jest teraz dokładnie
tą z patcha: wymienione z nazwy kolczyki, bransolety, naszyjniki i buty, bronie
z jego listy, wszystkie zbroje powyżej 33 poziomu i wszystkie tarcze powyżej 20.
Przedmioty z listy trafiają **do magazynu przy każdej wizycie**, a nie dopiero z
pełnego plecaka. To, czego lista nie zatrzymuje, idzie na sklep dla innych
Hazardzistów, a nie do handlarza. Broni na 30 poziom swojej klasy bot trzyma
do czterech; takich broni innych klas nie chomikuje już wcale.

**Hazardzista w mieście.** Bot w pierwszej wiosce z bronią +7, zbroją +6 i
3 000 000 yang (według przelicznika dropu yang i inflacji) zostaje
Hazardzistą bez losowania: wybiera przedmiot, dokupuje brakujące ulepszacze i
ulepsza go według zasad tej osobowości.

**Perfekcjonista na rynku.** 15% Perfekcjonistów przed pójściem do
kowala sprawdza rynek: gdy stoi na nim broń, zbroja, tarcza albo hełm +8/+9 na
ich poziom (nie niższy niż o 10), kowal czeka 20 minut na zakup, a bot płaci z
budżetu Perfekcjonisty. Gdy nic takiego nie ma, ulepsza sam.

**Śmieciowe bronie na rynku.** 54 bronie z listy Iwakury na +0..+3 mogą
stać najwyżej w **pięciu sztukach łącznie** na sklepach wszystkich botów; każda
następna idzie do handlarza, a nadwyżka wystawiona wcześniej wraca z lady.
Broń z cennymi bonusami nie jest śmieciem i do limitu się nie liczy.

**Farma w M1.** Boty w pierwszej wiosce biją zaprzysiężonych i
niedźwiedzie, z których lecą ulepszacze, zamiast psów przy bramie: wybierają
miejsca, gdzie te potwory stoją (zmierzone na plikach serwera — 15–18 takich
miejsc w każdej wiosce). 1% Grinderów pierwszej wioski zostaje tam, dopóki nie
zbierze na broń na 30 poziom +8 i zbroję na 18 lub 26 poziom +9.

**Obrona gildii** (pomysł **Amosa**). Gdy gracz zaatakuje bota, boty z jego
gildii w promieniu 12 000 jednostek od napastnika rzucają wszystko i atakują
go razem. Nad głową: „Bronię gildii przed …".

### Pomysły

- **Sklepy botów bez kłódek** (Iwakura; na warunek SIZOWSKIEGO — tylko boty):
  boty mają do dyspozycji całą ladę, a gracz nie widzi na ich sklepach kłódek.
  Sklepy graczy bez zmian.
- **Sklepy tylko w M1** (pomysł Kuszaa): nowe sklepy botów stają tylko w
  pierwszych wioskach, bo tam kupują gracze. Sklep, który stał w drugiej wiosce,
  po wygaśnięciu zostaje odnowiony na rynku pierwszej wioski. Przełącznik na
  stronie AI panelu klasycznego przywraca sklepy w obu wioskach.
- **Skrzynia Ucznia jako opcja** (pomysł seban latino): launcher pyta o nią
  razem z ratami przy tworzeniu nowego świata; w pliku `.env` to
  `M2_STARTER_CHEST` (1 — włączona, jak dotąd).

### Inne poprawki

- **Raty podczas eventu** (**Monek**, **CarloMontana**): zmiana rat w panelu
  w czasie eventu wyglądała, jakby się nie zapisała, i nie działała też po
  jego końcu. Teraz panel zapisuje ją jako stawkę bazową eventu, a koniec
  eventu wraca właśnie do niej. Stawkę bazową, którą zostawił event zakończony
  przy wyłączonym serwerze albo stara wersja, serwer sprząta po starcie — to
  przez nią rat nie dało się zmienić nawet wtedy, gdy żaden event nie trwał.
- **Kopia świata** (**uxietoszef**): kopia przerywała się na uszkodzonej bazie
  `log`, a razem z nią reset świata. Teraz zrzut próbuje jeszcze raz z
  pominięciem uszkodzonych tabel, a gdy i to się nie uda, kopia idzie dalej
  bez tej bazy (to sama historia, gra jej nie czyta) i pisze o tym w pliku
  README kopii. Każda inna baza to sam świat i nadal zatrzymuje kopię.
- **Pierścień Teleportacji** działa — poprawkę znalazł **Mur4s**, potwierdził
  **NerrVoVy**.
- **Zmiana cen w sklepie offline** (**blastyw**, klient 2.0.24): na dużym
  świecie czat zgłaszał nieudane zmiany, choć wszystkie ceny zmieniły się za
  pierwszym razem — klient nie czekał wystarczająco długo na odpowiedź
  serwera i wysyłał zmiany jeszcze raz.
- **Mur Joan** (świat **Sammy Suricate**): bot, który wrócił do przerwanej
  wizyty w mieście po złej stronie muru, stał w miejscu, aż po półtorej minuty
  ruszył go strażnik bezczynności. Teraz przechodzi z powrotem przez bramę.
- **Mapa gildii Jinno** (logi **urtopy**): jedno z miejsc polowań leżało na
  odciętym kawałku terenu, do którego nie da się dojść; zastąpione osiągalnym.

### Czego nie sprawdziliśmy

Obie linie silnika kompilują się bez błędów i bez nowych ostrzeżeń, wszystkie
testy jednostkowe przechodzą, a świat testowy chodzi na tej wersji bez żadnej
awarii. Na nim już widać działanie części zmian: inflację (+5% przy 3,19 mld
yang), licznik śmieciowych broni na rynku, 95 Grinderów, którzy przestali się
blokować, i dwóch, którzy porzucili ten styl, Perfekcjonistów sprawdzających
rynek przed kowalem, pierwsze nowe przedmioty wybonowane w plecaku ponad
noszone i dropki medali w Lochach Małp.

Większości tego patcha nie da się jednak dziś zmierzyć, bo nasz świat testowy
jest świeżo wyzerowany: boty mają poziomy 0–29, a od 30 poziomu wzwyż są tylko
cztery. Broń na 30 poziom, Hazardzista w mieście, zakupy Perfekcjonisty, cel
dropków medali, wojny gildii i obrona gildii przed graczem ruszą dopiero na
światach z wyższymi poziomami, a obrony gildii i wymiany expa w drużynie z
graczem nie mamy na czym sprawdzić — na naszym świecie nie gra żaden człowiek.
Limit odpoczynku przy „Boty grają jak żywi ludzie" jest policzony, ale nie
obejrzany: u nas ten przełącznik jest wyłączony. Poprawka zegara sklepów
offline działa od startu; to, że odnowienia nie utykają, pokażą dopiero
godziny.

Pomogą w tym linie w logu: `PLAYERBOT_MARKET: level-30 census` (co dziesięć
minut: ile botów od 30 poziomu nie ma broni i na jakim plusie są pozostałe),
`PLAYERBOT_ANTIPK: guild called`, `PLAYERBOT_PERSONA: medal dropper met its
goal`, `PLAYERBOT_LIFE: census ... held_on=`, `PLAYERBOT_OFFLINE: renewal
went unanswered`, `PLAYERBOT_BONUS` (teraz także w paczce wsparcia) oraz
`PLAYERBOT_OFFLINE: reopen ... moved_from=`.

## 2.0.94 — 2026-09-20

Serwer 2.0.94. Klient bez zmian (2.0.23). Zawiera wszystko z 2.0.93.

### Boty nie zatrzymują się już całą wsią na jednym poziomie

Zgłosił **Iwakura**: „boty masowo zatrzymują się na 25 lvlu, a ja nawet takiego
przedziału nie dawałem". Reguła tierów jest czystą funkcją, więc dało się ją
policzyć poza silnikiem, dla całej kohorty — i liczby przyznały mu rację.

**Gdzie zatrzymywał się bot rosnący od pierwszego poziomu** (2500 botów): na
poziomach 19–24 po 3,4–3,8% każdy, a na **25 aż 15,6%** — cztery i pół raza
więcej, choć Community Patch 1 obiecywał, że blokada M3 jest „losowana równo".
W logach naszego świata testowego to samo: blokada 25 padła 118 razy, a każda
z 19–24 od 6 do 16 razy.

**Przyczyna.** Losowanie było jedno na bota, wspólne dla wszystkich tierów, a
pierwsza wioska (13–19) i M3 (19–25) mają tę samą szerokość — siedem poziomów.
Bot, który wylosował szóstą pozycję w przedziale, dostawał w wiosce blokadę 19,
której jej pasmo (10–18) nigdy nie osiąga; wychodził z wioski niezatrzymany,
wchodził na M3 z poziomem 19 i losował tę samą szóstą pozycję — czyli 25,
szczyt tieru. Co siódmy bot świata, z konstrukcji.

Każdy tier losuje teraz osobno. Ten sam rachunek daje po poprawce 4,8–5,4% na
każdym z poziomów 19–25.

### Powyżej Góry Sohan nie ma już blokady

Tabela tierów kończy się na Sohanie (55–62), a bot, który ją przerósł,
blokował się **dokładnie tam, gdzie akurat stanął**. W historii naszego świata
testowego są blokady na 63, 64, 65 i wyżej — w tym **112 botów zamrożonych na
71 poziomie**, na liczbie, której żaden tier nie wymienia. Powyżej ostatniego
tieru Grinder nie jest już trzymany nigdzie, a blokada zapisana pod starą
regułą jest zdejmowana przy pierwszym sprawdzeniu.

### Czego to jeszcze nie naprawia

Uczciwie, bo Iwakura wskazał więcej: **awans nadal kończy blokady na zawsze**.
Bot, który raz spełnił Prawo Awansu w pierwszej wiosce albo na M3, nigdy nie
dostaje blokady M2 (30–35), Doliny (40–48) ani Sohanu (55–62) — wraca pod nią
tylko przez trzy śmierci, i wtedy staje tam, gdzie zginął. Dokument mówi co
innego: „Grinder NIGDY nie przechodzi na kolejny Tier bez odpowiedniego
przygotowania", czyli awans ma być **wejściem w następny tier**, nie końcem
blokad.

Do tego Iwakura zaprojektował rozłam po pierwszej wiosce: garstka botów
zostaje na M1 po ulepszacze i księgi umiejętności, reszta rozchodzi się na M2,
M3 i loch małp. To jest zmiana zachowania całego świata i wymaga własnego
wydania — wchodzi w następnym.

## 2.0.93 — 2026-09-20

Serwer 2.0.93. Klient bez zmian (2.0.23). Zawiera wszystko z 2.0.92.

### Lurowanie na zlecenie gracza

Zgłoszone tego samego wieczora przez **ĹŌŞƬĒĶ (l0st3k)** i **nerrvous_s**, z
logami — i okazało się, że to **dwie różne awarie naraz**, a dwie z trzech
przyczyn wprowadziliśmy sami w dwóch poprzednich wydaniach.

**Bot przyjmował zlecenie i nie ruszał się.** W 2.0.89 stojące zlecenie
przestawiało bota w tryb „jestem w drodze, nie poluję" — po to, żeby między
kursami nie zabijał tego, co miał przyprowadzić. Tyle że reguła oceniająca, czy
warto z czymś walczyć, odrzuca w tym trybie **wszystko** — również paczkę, po
którą kurs właśnie wyszedł. Zlecenie przyjęte, „Juz dla ciebie luruje"
powiedziane, i każdy kurs kończył się „brak paczki" ułamek sekundy później. W
logu nerrvous_s to widać co do milisekundy: kurs otwarty, 957 ms, koniec, i tak
w kółko.

**Bot otwierał kurs i zamykał go w tym samym momencie.** 2.0.90 obniżyło próg
życia, przy którym kurs się zaczyna, do 55% — a próg, przy którym kurs się
przerywa, został na 70%. Bot między jedną a drugą wartością robił to co
sekundę.

**Lur szukał paczek tam, gdzie gracz ich nie ma.** Ta rola była napisana dla
drużyn botów: „przyprowadź paczkę, do której drużyna jeszcze nie doszła" — więc
odrzucała wszystko bliżej niż 1100 jednostek i wszystko w promieniu 900 od
miejsca zbiórki, a poziom potwora mierzyła **względem bota**. Na zrzucie
l0st3ka towarzysz z 19 poziomu stał przy graczu z 33 i odrzucał każdego moba
wokół: za blisko, za nisko, albo o osiem poziomów za wysoko — względem siebie,
nie względem gracza, który i tak miał je bić.

Teraz na zlecenie od człowieka **lur zbiera to, co stoi wokół**: bez minimalnej
odległości, bez odstępu od miejsca zbiórki, okno poziomu liczone od gracza, a
plan to pięć grup i dwanaście strzałów zamiast dwóch grup i siedmiu. Oddawanie
agro graczowi działało wcześniej i jest nietknięte.

### Bot towarzysz robił zakupy zamiast słuchać

Drugi log — l0st3ka — pokazał coś innego: kurs **w ogóle się nie zaczynał**,
przez cztery minuty, z powodem `busy`. Powód okazał się workiem na siedem flag
naraz, więc najpierw dostał osobne nazwy (`town_visit`, `biologist`, `stable`,
`market_trip`, `fishing`, `retreat`, `recovering`, `off_frontier`, `dead`) —
bez tego następne zgłoszenie też byłoby nie do odczytania.

A pod spodem była reguła z 2.0.48: „bot w drużynie gracza nie chodzi na
errandy". Sprawdza ona, czy **liderem drużyny jest człowiek** — a bot
*Towarzysz* zaprasza gracza **do swojej własnej** drużyny, więc liderem jest
bot. Przez to wszystkie sześć bramek (stajnia, Biolog, zielarz, blokada przy
ujemnej randze oraz start i kontynuacja wizyty w mieście) było po cichu
wyłączonych **akurat dla tych botów, z którymi ludzie grają**. Teraz pytają o
jedną regułę: drużyna człowieka, kontrakt najemnika, towarzysz trzymający
człowieka w drużynie albo stojące zlecenie lurowania.

### Czego nie sprawdziliśmy

Uczciwie, bo tym razem wyszło mniej, niż chcieliśmy. Obie linie silnika
kompilują się bez błędów i bez nowych ostrzeżeń, nowy kod jest w gotowym
pliku wykonywalnym (sprawdzone po napisach, żeby mieć pewność, że optymalizator
go nie wyciął), świat testowy chodzi na tej wersji bez ani jednego padnięcia
rdzenia, a wizyty w mieście i drużyny działają jak przedtem.

Czego **nie** zmierzyliśmy: samego lurowania. Dla człowieka nie da się — na
świecie testowym nie ma gracza, który stanąłby w drużynie bota. A własnej roli
botów nie dało się dziś zmierzyć, bo nasz świat testowy ma obecnie cztery
postacie od 30 poziomu wzwyż, a ta rola działa tylko na mapach frontu, dokąd
nikt z tak niskim poziomem nie idzie.

Zostaje więc to, co zobaczą zgłaszający. W logu szukać:
`PLAYERBOT_LURE: planned` (kurs ruszył), `pack answered` (paczka odpowiedziała),
`pack handed to the player` (agro przeszło na gracza) oraz
`PLAYERBOT_LURE: waiting reason=` z konkretnym powodem, jeśli bot dalej stoi.

## 2.0.92 — 2026-09-20

Serwer 2.0.92, **klient 2.0.23**. Zawiera wszystko z 2.0.91.

### Gra mówi w ośmiu językach

**Polski, English, Deutsch, Español, Italiano, Português, Română, Türkçe.**
Język wybiera się w oknie konfiguracji przy logowaniu, obok rozdzielczości —
zmiana wymaga ponownego uruchomienia klienta.

Paczka językowa jest w całości robotą **ĹŌŞƬĒĶ (l0st3k)**: siedem nowych
katalogów locale, tabele przedmiotów i potworów, opisy, okna logowania. My
wstawiliśmy to do naszego klienta i sprawdziliśmy, co następuje:

- wszystkie **762 pliki** naszej dotychczasowej paczki językowej są w jego
  bajtowo identyczne — nic naszego nie zginęło (tło wyboru postaci, ekran
  ładowania, regulamin);
- jego root był zbudowany na starszym naszym kliencie i niósł **starsze wersje
  trzech naszych plików** — auto-łowów, tytułów botów i sklepu. Wzięliśmy z
  niego wyłącznie wybór języka, a nasze pliki zostały nasze, więc zmiana
  zachowania auto-łowów, którą po drodze wprowadził, **nie wchodzi**;
- nasz `metin2client.exe` już umiał `GetLanguage`, więc nie trzeba było nowego
  pliku wykonywalnego.

### Angielski interfejs

Dodatkowo 281 napisów interfejsu i komunikatów po angielsku, nakładanych
**tylko przy wybranym „en"** — każdy inny język widzi dokładnie to, co dotąd.
Doszły do tego cztery nagłówki, które do tej pory stały w skryptach na sztywno
i były polskie w każdym języku: „Status postaci", „Atrybuty" w oknie postaci
oraz „Kategorie" i „Stan Konta" w sklepie z przedmiotami.

Sprawdzone uczciwie, bez klikania w grze: wszystkie 33 skrypty kompilują się w
Pythonie 2.7 klienta, a prawdziwe loadery locale uruchomione z atrapami silnika
dla **PL, EN, DE i TR** potwierdzają, że po angielsku wchodzą angielskie
wartości, a w pozostałych językach **żaden napis się nie ruszył**
(`tests/client_locale_loader_test.py`). Przepakowany root różni się od
poprzedniego dokładnie jedenastoma naszymi plikami.

### Czego to jeszcze nie tłumaczy

Żeby nie było niespodzianki po wybraniu angielskiego — **po polsku zostają**:

- **nazwy NPC** („Agent Lądowy"), **treści questów** („Osiągnij 1 poziom
  jeździectwa…") i wszystko, co przysyła serwer: to jest baza i questy serwera,
  nie klient;
- **nazwy przedmiotów i kategorie w sklepie z przedmiotami** — też z serwera;
- część opisów w oknie bonusów postaci i opisy umiejętności: to brakujące
  wiersze w plikach `locale_game.txt` wybranego języka (dla angielskiego 676
  kluczy), a nie kod.

To jest osobna, większa robota po stronie serwera i danych — nie chcemy
mówić, że gra jest przetłumaczona, kiedy przetłumaczony jest interfejs.

**Paczka klienta urosła z 68 MB do 96 MB** — to te siedem nowych katalogów
językowych.

## 2.0.91 — 2026-09-20

Serwer 2.0.91, klient bez zmian (2.0.22). Zawiera wszystko z 2.0.90.

### Drużyny botów były w większości jednoosobowe

I dlatego wyglądały, jakby ich nie było. `CParty::Quit` wyjmuje członka i
**zostawia drużynę stojącą**; silnik kasuje ją tylko wtedy, gdy odchodzi lider.
Więc kiedy z pary wychodził zwykły członek — a wychodził stale, bo hub do
polowania bywa dwadzieścia kilometrów od drugiego bota — **drugi zostawał sam
w „drużynie" jednoosobowej**. Dla kodu to wciąż drużyna: `GetParty()` odpowiada,
pass drużyn wraca w pierwszej linii, i ten bot przestaje szukać kogokolwiek.

Własny kod silnika nigdy na to nie pozwala: przy dwóch członkach rozwiązuje
drużynę. Dlatego **gracz nie może być w drużynie jednoosobowej** i od strony
gracza nie dało się tego zobaczyć.

Jak to wyglądało na naszym świecie testowym — od 94 do 2519 założonych drużyn
**na godzinę**, i w każdym spisie tyle samo drużyn co botów w drużynach. Teraz
boty wychodzą z drużyny tą samą drogą co gracz, a drużyna jednoosobowa, jeśli
się gdzieś ostała, jest rozwiązywana w minutę. Zmierzone dziesięć minut po
wdrożeniu: stosunek z 1,00 na 1,375 (świat samych par daje 1,33 — spis ogląda
połowę botów, więc to jest wartość docelowa, nie 2).

Przy okazji wraca lurowanie **dla drużyn botów**: kurs Łucznika potrzebuje
trzech osób i przez pięć godzin nie odpalił ani razu, bo trójki nie miały z
czego powstać. Zlecenie od gracza („luruj" z 2.0.90) działało niezależnie od
tego — tam wystarczy para.

### Świeża instalacja nie wywala się już na pustym katalogu

„Launcher każe mi ogarniać pliki r40250" (**dekri**) — przy serwerze 2.0.89,
czyli na linii, która paczki r40250 nigdy nie używa. Brakowało dokładnie
jednego katalogu, i to **pustego**: `serverfiles\share\package`. Jest pusty na
każdej instalacji obu linii, Docker tylko go kopiuje — ale git pustego katalogu
nie przechowa wcale, a paczka niesie go jako jeden z dwunastu gołych wpisów
w zipie, które część programów do rozpakowywania po prostu pomija.

Zamiast odmawiać, launcher **tworzy go teraz sam** — na wszystkich trzech
drogach, które prowadzą do budowy, i tak samo `update.sh` na Linuksie. Komunikat
odsyłający do instalatora r40250 nie pojawi się już z tego powodu.

## 2.0.90 — 2026-09-20

Serwer 2.0.90, klient 2.0.22. Zawiera wszystko z 2.0.89.

### Lurowanie na zlecenie: Ninja w końcu wychodzi po moby

„Ninja zabija ich zamiast przynieść do mnie i pisze mu cały czas »Czekam żeby
lurować dla nick«" (**marcinxboss**). Jedno i drugie to ten sam błąd: kurs nie
mógł się zacząć, więc bot spadał do zwykłego zdobywania celu i zabijał to, co
miał przyprowadzić.

Trzy bramki zamykały go **na zawsze**, i wszystkie trzy są z czasów, gdy lurował
tylko dla drużyny botów stojącej bezczynnie i czekającej na pakiet:

- **„drużyna jest zajęta"** — kurs nie ruszał, gdy w pobliżu było więcej niż
  trzy potwory na drużynie. Cztery moby na graczu stojącym na spocie to zwykła
  niedziela. Ta zasada istnieje po to, żeby bot nie zasypał drużyny, która już
  ma pełne ręce — ale ktoś, kto napisał „luruj", sam o to poprosił, a jak ma
  dość, to napisze „przestań lurować".
- **dziewięć dziesiątych życia** — przy graczu bot obrywa od tego, z czym gracz
  walczy, więc ten próg nie otwierał się nigdy. Na zlecenie wystarczy 55%.
- **„coś mnie goni"** — ta zostaje, bo odejście z paczką na plecach to jest
  właśnie zgubiony lur. Ale teraz sama schodzi do zera, bo:

**bot ze zleceniem nie poluje na własną rękę.** Tego nie mówiła żadna zasada,
więc między kursami Ninja szła bić jak każdy inny bot — i już nie mogła wyruszyć,
bo miała potwory na sobie. Obrona własna i pomoc osobie, dla której luruje,
zostają bez zmian; znika chodzenie do potworów z własnej woli. Do kompletu: łuk
zostaje w ręce na czas zlecenia (sztylet dobyty na jednego metina wyłączał cały
kurs, bo lurowanie idzie z łuku).

A gdy bot naprawdę na coś czeka, **mówi na co**: „Czekam, żeby lurować dla X
(hp)", „(monsters_on_me)", „(no_bow)", „(safe_zone)". Poprzednia wersja
powtarzała samo „Czekam" i nie dało się z tego nic zgłosić.

Zastrzeżenie, bo na takie rzeczy zasługuje jasna odpowiedź: to jest skompilowane
i przeczytane, ale **nie obejrzane w grze z żywym graczem** — nasz świat testowy
nie ma ani jednego. Jeśli dalej będzie coś nie tak, słowo w nawiasie nad głową
bota powie, co go blokuje.

### Świeża instalacja na Linuksie znów się buduje

**Jeśli stawiałeś serwer od zera na VPS-ie albo mini PC i nic nie wstawało —
to było to.** Cały build przewracał się na jednej linii:

```
target seban-panel: failed to solve: failed to compute cache key: "/VERSION": not found
```

a razem z nim gra, panel i itemshop — więc nie startowało nic, i żadne
„kliknij jeszcze raz" nie mogło pomóc.

Plik, którego brakowało, jest w `.gitignore`, a tworzą go tylko dwa skrypty:
ten od instalacji na starej linii i launcher windowsowy. Czyli istniał na
każdej maszynie, która kiedykolwiek któryś z nich uruchomiła — i na niczyjej
innej. Instalacja 2.x na Linuksie nie uruchamia żadnego z nich, a paczka nie
mogła go przenieść, bo powstaje z czystego eksportu z repozytorium.

Build nie zależy już od tego pliku, a aktualizacja sama go dokłada. Zgłosił
**superjerry925** z pełnym logiem — bez niego to by leżało dalej.

Przy okazji naprawione narzędzie, które miało takie rzeczy wyłapywać przed
wydaniem: pytało dysk zamiast repozytorium, więc wierzyło plikowi, który jest
tylko u nas. Teraz pyta, **kto** dany plik tworzy i czy ten ktoś w ogóle
działa na danej linii.

### Ceny hurtem w sklepie: powtarza to, co nie weszło

„Zmienianie cen działa, ale potrafi ominąć nawet parę itemów, gdy ma się ich
dużo w sklepie" (**blastyw**). Ctrl + prawy wysyłał pakiety w odstępach, bo
serwer przyjmuje jedną operację na sklepie co 200 ms — ale **wysyłał i
zapominał**. Ćwierć sekundy liczona zegarem klienta potrafi dolecieć wewnątrz
jednego okna serwera po zaciętej klatce, a wtedy ta jedna pozycja przepadała
bezpowrotnie.

Teraz klient sprawdza, co faktycznie weszło: czeka na powrót listy sklepu,
porównuje ceny i powtarza to, co się nie udało — trzy podejścia, a potem mówi
na czacie, ile zostało. Pozycja sprzedana w międzyczasie nie liczy się jako
pominięta. **Zaktualizuj też klienta.**

### Liczba botów per królestwo nie otwiera się już na zerach

W oknie LICZBA BOTÓW ptaszek „Indywidualne wartości dla królestw" pokazywał
**0 / 0 / 0**, bo te trzy ustawienia nie istnieją w `.env` na żadnej
instalacji starszej niż 2.0.83. Zaznaczenie go i wpisanie liczby tylko w
jednym polu zostawiało dwa pozostałe królestwa bez ani jednego bota — i nic w
grze tego nie mówiło.

Pola otwierają się teraz na równym podziale twojej liczby botów, a zero
wpisane świadomie jest honorowane i powiedziane wprost: „UWAGA: Shinsoo i
Jinno nie wystartuje żadnego bota".

To **nie jest** potwierdzona przyczyna „nowe postacie tworzą się tylko w
Chunjo" (**NerrVoVy**) — on sam dopisał tego samego dnia, że widział to również
przy wyłączonym ptaszku, samym suwakiem. Dwie rzeczy zmierzone u nas, żeby
wykluczyć to, co się najpierw nasuwa: podział liczby botów między królestwa jest
**równy** (przy 1000 botów rdzeń zgłasza „world split 334/333/333"), a drugi
kanał nie ma z tym nic wspólnego — przy ch2 włączonym oba kanały niosą wszystkie
trzy królestwa (kanał 1: 320 / 1185 / 347, kanał 2: 180 / 315 / 153). Zostaje
liczba **zdatnych tożsamości** w danym świecie; rdzeń wypisuje ją przy starcie i
to jest pierwsza linia, o którą poprosimy przy następnym zgłoszeniu:

```
PLAYERBOT_AUTH: loaded N registered bot identities (shinsoo=.. chunjo=.. jinno=..)
PLAYERBOT_AUTH: registry rows=.. usable=.. rejected: ...
```

Przy okazji, bo to wyszło w tej samej rozmowie: **`.env` nie jest miejscem, w
którym stoją twoje aktualne raty.** Na linii 2.x raty to flagi zdarzeń w bazie —
panel je zmienia i gra liczy nimi od razu — a `M2_RATE_*` w `.env` to tylko
wartości startowe świeżego świata. To, że w `.env` stoi 100, a w panelu 125, nie
jest błędem i niczego nie cofa.

---

## 2.0.89 — 2026-09-20

Serwer 2.0.89, klient bez zmian (2.0.21). Zawiera wszystko z 2.0.88.

### Poproś ninję o lurowanie

Bot z łukiem w twojej drużynie przyjmuje teraz dwie komendy **na priv**:

- **„luruj"** — bierze zlecenie (rozumie też „lur", „pull", „przyciągnij",
  „ciągnij")
- **„przestań lurować"** — oddaje je (albo „koniec", „dość", „stop")

Kiedy zlecenie stoi, kurs jest twój, a **paczka ląduje na tobie, nie na bocie**.
To jedyne miejsce w całym lurowaniu, które rusza agro silnika — kiedy boty
lurują dla siebie, potwory trzeba odbić walką — i robi to dwoma wywołaniami
silnika w kolejności, która się trzyma: bot oddaje ci potwora z większym agro, niż zdążył
zarobić strzałami, a potem odwraca go na ciebie. Masz około trzech sekund na
pierwsze uderzenia, zanim silnik znów zacznie liczyć agro normalnie.

Ustępują tylko te reguły, które istnieją po to, żeby bot nie lurował dla nikogo:
trzech w drużynie → dwóch (para to drużyna), odbiorcą może być człowiek, znika
wymóg mapy frontierowej, a przerwa między kursami spada z 20-50 s do 4-9 s.
Próg życia, strefa bezpieczeństwa i „czy ta walka jest warta" zostają.

Bot odmawia konkretnie: „Najpierw zaproś mnie do drużyny", „Nie mam teraz łuku
w ręce", „Jesteśmy w strefie bezpieczeństwa — wyjdź na łowisko". Nad jego głową
i w panelu widać, **dla kogo** luruje.

### Raty: jedna prawda dla obu paneli

„Jak ustawiałem wcześniej raty u tiera, to u sebana narzucał poprzednie"
(NerrVoVy) i „ustawiłem 10000%, a boty działają jakby miały x3/5" (marcol_) to
było to samo.

Rata to **flaga zdarzenia** w bazie — wszystko inne to jej kopie. Flagi
zapisują cztery rzeczy (oba panele, pomocnik w grze, eventy czasowe) i nie
wszystkie odświeżają każdą kopię. Panel zaawansowany czytał plik ze spoola jako
pierwszy, a ten się rozjeżdżał: na świecie testowym mówił drop 150 / yang 120,
kiedy świat naprawdę chodził na 200 / 200 — i w chwili kliknięcia „Zapisz"
wpisałby te stare liczby jako nowe raty.

Oba panele czytają teraz flagi. W trakcie eventu strona pokazuje **twoje**
ustawienie, nie podbicie, więc zapis podczas eventu nie zamieni podbicia w nową
normę.

### +7 nigdy nie jest złomem, sprzęt startowy też

Jedna liczba była dwoma progami naraz: od ilu stragan bierze przedmiot i
poniżej ilu handlarz go skupuje. Dla broni i zbroi na 1 lvl próg wynosił +8, więc
+7 wpadał między nie i szedł do handlarza — kilkanaście sztuk dziennie. Teraz +7
trafia na stragan; do handlarza idzie tylko +4 do +6.

### Ciszej w logach, i jedna prośba, której silnik nigdy nie spełniał

Bot prosił o dosiadanie konia w chwilach, w których silnik zawsze odmawiał —
przy otwartym straganie, przy magazynie, w sklepie z przedmiotami, przy
warzeniu mikstur. Tysiące linii błędu dziennie, a koń był w porządku. Teraz bot
pyta o to zawczasu i po prostu czeka.

Audyt siatki plecaka pisał do logu błędów przy każdym sortowaniu, także wtedy,
gdy niczego nie zepsuł: 2285 przebiegów na 1002 plecakach w ciągu dnia i ani
jednego, który skończył gorzej, niż zaczął. Pisze teraz tylko wtedy, gdy
naprawdę coś się pogorszyło.

Support bundle dostaje mniej śmieci, a prawdziwe błędy przestają w nich ginąć.

---

## 2.0.88 — 2026-09-20

Serwer 2.0.88, klient 2.0.21. Zawiera wszystko z 2.0.87.

### Nazwy osobowości wracają nad głowy botów

Serwer od 2.0.86 wysyła nad głową bota nazwę osobowości, która go teraz trzyma
— Grinder, Zdobywca, Handlarz i tak dalej. Klient 2.0.20 tych nazw **nie znał**:
poprawka leżała w repozytorium od 19 września, ale nie trafiła do paczki klienta,
więc bot dostawał numer, którego klient nie umiał nazwać, i nie rysował nic. Kto
zauważył, że osobowości zniknęły — miał rację. **Zaktualizuj też klienta.**

### Auto Łowy: odpały idą po jednym

Silnik przyjmuje jedno użycie przedmiotu naraz i odrzuca resztę, więc klatka
wysyłająca cztery odpały kończyła się tym, że działał pierwszy. Teraz między dwoma
użyciami jest sto milisekund, a reszta idzie w następnych przebiegach. Poprawka
od **Colide**, autora tego okna.

### Nowy cennik Iwakury

Ceny w tym projekcie nie są wpisywane ręcznie — są renderowane z arkusza
Iwakury. Ten jest z 20 września i ruszył się w nim prawie każdy numer, większość
materiałów o jakieś siedemdziesiąt procent:

| | było | jest |
|---|---|---|
| Ząb Orka | 136 730 | 232 441 |
| Zwój Błogosławieństwa | 180 000 | 550 000 |
| Kamień duchowy | 500 000 | 750 000 |

Doszły też **receptury zielarskie** — czterdzieści pozycji, wszystkie po 450 000,
których cennik wcześniej w ogóle nie wyceniał.

Stragany, które już stoją, przecenia się same przy najbliższej wizycie
właściciela — nie trzeba nic robić ani zaczynać od nowa.

---

## 2.0.87 — 2026-09-20

Serwer 2.0.87. Zawiera wszystko z 2.0.86.

### Nowy świat zaczyna się wtedy, kiedy chcesz

Do tej pory po wyzerowaniu świata serwer ruszał natychmiast, a panel pokazywał
650% doświadczenia, którego w grze nie było: mnożniki stawek na tej wersji
silnika powstają dopiero przy pierwszym kliknięciu **Zastosuj** na stronie
„Stawki”, więc świeży świat chodził na 100%, a panel obiecywał coś innego.
Pierwsze kliknięcie — choćby bez ruszania pól — robiło z tej obietnicy prawdę.

- **Launcher pyta o stawki, gdy zakładasz nowy świat.** Normalnie (100/100/100),
  Spokojnie (300/200/200), Szybko (1000/500/500) albo własne liczby. To, co
  wybierzesz, jest zapisywane zanim rdzenie wstaną, więc pierwszy bot gra już
  na Twoich ustawieniach. Świat, który raz je dostał, zmienia się wyłącznie ze
  strony „Stawki” w panelu — tak jak dotąd.
- **Boty mogą poczekać przy drzwiach.** Przy zerowaniu świata możesz zaznaczyć,
  że świat ma wstać pusty. Ustawiasz wtedy spokojnie stawki, respawny,
  osobowości i cokolwiek jeszcze, a potem klikasz **Wpuść boty do świata** na
  stronie AI w panelu. Boty wchodzą stopniowo, tak jak po zwykłym starcie.
  Domyślnie wyłączone — kto nic nie zaznaczy, dostaje to, co dotąd.
- Kto nie korzysta z launchera, ma w `.env` trzy nowe linie (`M2_RATE_EXP`,
  `M2_RATE_DROP`, `M2_RATE_YANG`) i przełącznik `M2_PLAYERBOT_START_HELD`.

### Iwakura Community Patch 1

Pierwsza poprawka Iwakury do systemu osobowości:

- **Grinder nie zatrzymuje się już całą wsią na jednym poziomie.** Blokada
  pierwszej wioski jest losowana równo z przedziału 13–19, a blokada M3 z
  19–25. Co czwarty bot w ogóle nie zatrzymuje się w pierwszej wiosce — od 13.
  poziomu idzie prosto na M3 i dopiero tam staje.
- **Od 35. poziomu Prawo Awansu jest ostrzejsze:** broń przynajmniej +8, a do
  zbroi i tarczy dochodzi maska albo hełm na +6. Wcześniej hełmu nikt nie
  wymaga — w pierwszych wioskach nie ma go gdzie kupić.
- **Przed 45. poziomem bot bonuje najpierw naszyjnik, bransoletę i buty**, bez
  względu na to, jak słabe są i na ile ulepszone, i celuje w linie z tabeli
  Iwakury dla każdego z tych slotów. Młody bot sięga po to zwykły kamień
  zmiany/dodania — zielone działają wyłącznie na broni i zbroi, więc inaczej
  reguła nie miałaby jak zadziałać.
- Ulepszanie idzie kolejnością: bronie, zbroje, tarcze, hełmy.
- **Kamień Duszy +2** wolno włożyć w słaby sprzęt wczesnej gry; +0 i +1 już nie.

### Walka botów

- **Cios trafia to, co jest przed botem.** Bot bił do czterech potworów w
  promieniu 300 jednostek, także tych za plecami. Gracz trafia wszystko przed
  sobą, bo łuk broni zakreśla się z przodu — bot ma teraz ten sam łuk. Liczby
  celów nie zmieniamy: gracz może trafić szesnaście, bot najwyżej cztery.
- **Słaba broń bije jak słaba broń.** Gdy wyliczone obrażenia wychodziły małe,
  bot podstawiał wymyśloną liczbę — około trzystu dla bota z siedemdziesiątki,
  niezależnie od tego, co trzymał. Teraz cios jest wart tyle, ile liczy gra.

### Panel

- Z okna ekwipunku znika przycisk **odśwież**: wejście w postać i tak pobiera
  dane od nowa, a teraz robi to z gwarancją, że przeglądarka nie poda starej
  kopii.

---

## 2.0.86 — 2026-09-20

Serwer 2.0.86, klient 2.0.20. Zawiera wszystko z 2.0.85.

> **Zalecamy zaczac na nowym swiecie.** To wydanie zmienia zachowanie botow i
> ich gospodarke mocniej niz jakiekolwiek wczesniej. Swiat, ktory gral na
> starych zasadach, niesie ze soba plecaki, sklepy i magazyny ulozone pod nie.
> Nowy swiat pokaze te systemy tak, jak maja dzialac. W launcherze:
> **KOPIA / NOWY SWIAT**.

### System osobowosci Iwakury

Najwieksza zmiana w dzialaniu botow od poczatku projektu. Do tej pory bot
dostawal osobowosc raz, przy wejsciu do gry, i trzymal sie jej do konca. Teraz
o tym, kim jest w danej chwili, decyduje jego wlasna sytuacja — sprawdzana co
dwie sekundy.

**Nastroje (BMS).** Kazdy bot ma nastroj: SLABY, NORMALNY albo BARDZO DOBRY.
Posucha bez niczego wartosciowego go obniza, dobry drop albo udane ulepszenie
podnosi, spalenie przedmiotu na +8 czy +9 obniza o jeden, a piec smierci z reki
gracza blokuje na SLABYM na trzy kwadranse. Tylko bot w slabym nastroju robi
przerwy miedzy grupami potworow, odchodzi na chwile od klawiatury i odpoczywa w
miescie. W towarzystwie — w druzynie, w lochu, na rajdzie, na wojnie, w
pojedynku, na kontrakcie — kazdy gra jak NORMALNY, niezaleznie od tego, co
czuje.

**Kim bot jest teraz.** Kolejnosc jest taka: kontrakt najemnika, druzyna
(Towarzysz), wedka, kilof, metin pod mlotkiem, sesja hazardzisty, kowadlo dla
siebie (Perfekcjonista), plecak przy osiemdziesieciu procentach (Handlarz), a
na koncu Grinder albo — gdy spelni Prawo Awansu — Zdobywca.

**Towarzysz.** Suwak PT ustawia teraz udzial *czasu*, ktory bot spedza w
druzynie, a nie sam ranking. Druzyna konczy sie tak, jak opisal Iwakura: przy
pelnym plecaku, gdy poziomy sie rozjada, albo gdy reszta odejdzie — nie na
minutnik. Bot zaprasza takze **graczy**: samotnego gracza zaprosi do druzyny, a
do druzyny gracza z wolnym miejscem poprosi o przyjecie. Oba szanuja blokady z
opcji gry i sa mocno racjonowane, zeby nikogo nie zasypac. Towarzysz-szaman
trzyma buffy calej druzynie, graczom w pierwszej kolejnosci.

**Najemnik.** Bot, ktory trzy razy w pol godziny zginal od potworow, jest w
tarapatach. Silniejszy bot z jego krolestwa, na tej samej mapie, podchodzi i
proponuje godzine opieki za 250 000 yang przeliczone przez stawki. Prowadzi
druzyne, poluje tam, gdzie jest klient, klient dostaje polowe z kazdego zabicia.
Pelny plecak najemnika wstrzymuje kontrakt — druzyna zostaje, zegar staje,
najemnik wraca sam — a po godzinie klient placi dalej, jesli nadal potrzebuje.

**Lista Przydatnych Przedmiotow.** Decyduje, co bot odklada u Dozorcy, zamiast
sprzedawac: bizuteria i buty tieru 3-6, bronie z jego pasm poziomow, tarcze z
61 i zbroje z 66, oraz kazdy przedmiot z linia tieru 5-6 wyrzucona co najmniej
do polowy. Dwie sztuki broni albo zbroi i trzy drobne dla swojej klasy, jedna
dla obcej. Co bot przerosnie — wraca na rynek.

**Hazardzista** dokupuje bazy i materialy z lokalnych sklepow offline, w ramach
budzetu sesji.

Calosc chodzi na przelaczniku **OSOBOWOSCI** na stronie AI w panelu
klasycznym — domyslnie wlaczonym. Wylaczenie wraca dokladnie do starego
zachowania. Dawne osobowosci zostaly jako ukryty charakter, ktory tylko
przechyla szanse; dropki Metinow, M2 i M3 zastapily progi Grindera, a kohorta
dropkow medali operatora zostaje bez zmian.

Nad glowa bota, w panelu i w pliku statusu widnieje ta sama odpowiedz;
`PLAYERBOT_PERSONA: census` liczy je co dziesiec minut razem z nastrojami.

### Okno ulepszania nie zamyka sie po kazdej probie

- Ulepszasz dalej bez klikania od nowa w kowala i w przedmiot. System byl w
  serwerze od 2.0.31, ale tylko dla kogos, kto wiedzial o komendzie
  `/refine_keep_open 1` — teraz dziala u kazdego (podpowiedzial sizowski).
- Kto woli po staremu, wpisuje `/refine_keep_open 0`. Wybor zostaje po
  wylogowaniu, a kto juz wczesniej ustawil sobie wlaczone, niczego nie traci.
- Bez zmian w Wiezy Demona — tamto ulepszanie zamyka sie jak dotad.

### GRAJ moze uruchomic sam serwer

- Pod przyciskiem GRAJ jest **„Uruchom takze klienta gry”**. Odznacz, jesli
  trzymasz swiat wlaczony dla innych i nie chcesz, zeby za kazdym razem
  otwieralo Ci sie okno gry.
- Napis na przycisku mowi wtedy „GRAJ (SAM SERWER)”, wiec widac, co sie stanie.
  Wybor pamieta sie na stale — odznaczasz raz.

### Kazde ustawienie w `.env` opisane po polsku

- Nad kazda linia w `linux-port\docker\.env` jest teraz komentarz `# PL:`
  mowiacy, co ten parametr robi i co mozna w nim wpisac. Dotad plik byl
  opisany wylacznie po angielsku i to stad bralo sie najwiecej pytan.
- Opis musi byc w osobnej linii **nad** ustawieniem. Dopisanie komentarza w tej
  samej linii (`KLUCZ=1 # opis`) psuje wartosc — Docker czyta wtedy „1 # opis”.

### Zmiana ceny wielu przedmiotow w sklepie (klient 2.0.20)

- Ctrl + prawy przycisk na przedmiocie w swoim sklepie zmienia cene wszystkich
  takich samych. Dotad zmieniala sie cena jednego, a reszta wyrzucala blad
  „poczekaj chwile” — serwer przyjmuje jedna operacje na 200 ms, a klient
  wysylal wszystkie naraz (zglosili uxietoszef i Nagash). Teraz wychodza po
  kolei; dwadziescia przedmiotow zajmuje okolo pieciu sekund.

### Poprawki

- **Swiat opisany osobno dla kazdego krolestwa** liczyl sie jako maly i trafial
  na jeden rdzen. Trzy razy po 700 botow to 2100 botow, a nie „nic”: taki swiat
  zostaje teraz na trzech rdzeniach, tak jak mial.
- **Log serwera przestal straszyc** przy sortowaniu ekwipunku. Kontrola zajetosci
  liczyla dolna polowe kazdego miecza i kazdej zbroi jako bled i pisala o tym do
  `syserr` — 2188 wpisow o plecakach, z ktorymi nic nie bylo nie tak. Samo
  sortowanie bylo i jest bezpieczne: w 2188 pomiarach nie zmienilo stanu ani
  jednej komorki.
- **Kamienie bonusu** (zmianki, dodania): sprawdzone do konca. Nie da sie ich
  wystawic na straganie i nie jest to wina botow — sam silnik na to nie
  pozwala (te przedmioty maja blokade sklepu, gracz tez ich nie wystawi).
  Boty ich uzywaja normalnie do mixowania; zajmuja srednio 1,6 komorki, bo sie
  stackuja.

---

## 2.0.85 — 2026-09-20

Serwer 2.0.85. Klient bez zmian (2.0.19).

### Wszystkie trzy krolestwa na jednym swiecie, domyslnie

- `M2_PLAYERBOT_WORLD_LAYOUT` jest teraz ustawiony na `unified` — takze na
  serwerach, ktore juz stoja (raz, przy najblizszym starcie). Wczesniej kazde
  krolestwo dostawalo osobny rdzen, a bot nie potrafi przejsc miedzy rdzeniami:
  caly front — Dolina Orkow, Pustynia, Sohan, Lochy Pajakow — hostuje rdzen
  Chunjo, wiec boty Shinsoo i Jinno konczyly na okolicach 36 poziomu i nie mialy
  gdzie polowac. Przelacznik istnial od 2.0.30 i prawie nikt o nim nie wiedzial:
  gracze przesylali sobie zrzuty ekranu z linijka do wklejenia do `.env`.
- Swiat, ktory prosi o wiecej niz 1500 botow, zostaje na `split` — przy takiej
  liczbie jeden rdzen bylby za wolny. Wlasny wybor w `.env` jest respektowany;
  zmiana jest jednorazowa i zapisuje `M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=1`.

### Poprawki

- **Sortowanie ekwipunku nie zostawia juz slotow, ktorych nie da sie uzyc**
  (Dearminder). Po „Scal i uporzadkuj" miejsca zwolnione przez sortowanie
  potrafily zostac dla klienta zajete — nie dalo sie tam nic przeniesc
  z magazynu, a pomagalo dopiero przelogowanie. Serwer na koniec operacji
  podaje klientowi jeszcze raz stan kazdej komorki, ktorej dotknal, i
  sprawdza przy okazji wlasna siatke zajetosci.
- **Boty nie chomikuja juz medali konnych** (Greess). Bot z koniem na
  dziesiatym poziomie i powyzej 35 poziomu postaci nie mogl ani wydac medalu
  (czeka na probe konia bojowego), ani go sprzedac — wiec zbieral je bez konca;
  jeden z graczy pokazal ponad czterdziesci sztuk w plecaku. Teraz wszystko
  ponad dwa medale trafia na sklep jak kazdy inny towar. Na swiecie testowym
  bylo 37 627 medali w plecakach przeciwko 5 346 na ladach.
- **Boty wystawiaja nadmiar kamieni bonusu** (Nagash). Kamien zmiany, kamien
  dodania i Marmur Blogoslawienstwa nigdy nie moga trafic do handlarza — i
  zadna lada ich tez nie przyjmowala, wiec to, czego bot nie zdazyl zuzyc,
  zostawalo w plecaku na zawsze; jeden z graczy pokazal sto dziewiecdziesiat
  sztuk u jednego bota. Na swiecie testowym bylo 20 387 kamieni w plecakach i
  **zero** na ladach, 374 boty ponad dziesiec sztuk. Teraz bot trzyma dziesiec
  na wlasne mixowanie, a reszte wystawia.
- **Drugi kanal jest osiagalny takze na Linuksie** (GorącyDelfin). Kanal 2
  nasluchuje na portach 13010-13012, ale otwieral je tylko launcher na
  Windows — wiec na serwerze linuksowym (i po wlaczeniu kanalu w panelu) boty
  na CH2 graly, a nikt z zewnatrz nie mogl sie tam zalogowac. Aktualizacja
  ustawia zakres portow sama, zanim wystartuja kontenery.
- **Panel zaawansowany pokazuje, ile botow jest na ktorym kanale** (hunmar).
  Rozbicie pojawia sie obok lacznej liczby tylko wtedy, gdy drugi kanal
  faktycznie dziala.

## 2.0.84 — 2026-09-19

Serwer 2.0.84 i klient 2.0.19. W launcherze ZAINSTALUJ AKTUALIZACJE: nowe
Auto Łowy są w kliencie.

### Drugi kanał dostaje swoich botów (SIZOWSKI, Xewi, Mkls)

- Z włączonym drugim kanałem CH2 dostaje tylu botów, ile ustawia suwak
  (domyślnie 40%). Wcześniej każdy bot, który miał kiedyś sklep offline, był
  na stałe przypisany do CH1. Na świecie, który już trochę pograł, sklep ma
  prawie każdy bot, więc CH2 dostawał kilkadziesiąt botów zamiast kilkuset.
- Sklepy dalej stoją tylko na CH1. Bot z CH2, który chce otworzyć albo
  obsłużyć swój sklep albo coś kupić, przechodzi na CH1, a wolny bot z CH1
  przechodzi na jego miejsce na CH2. Przejście to wylogowanie z jednego
  kanału i zalogowanie na drugim. Bot nigdy nie jest na obu kanałach naraz.
- Bot z CH2 obsługuje swój sklep co 45–75 minut, a nie co kwadrans:
  przejście między kanałami kosztuje więcej niż zwykła zmiana mapy.
- Ustawiony udział to najmniej tyle botów na CH2. Gdy nikt nie czeka na
  przejście, CH2 może przejąć do 10 punktów więcej. Dropki medali zostają na
  CH1.
- Projekt przejść między kanałami przysłał SIZOWSKI. Dziękujemy!

### Serwer wstaje po nagłym wyłączeniu komputera (Greess)

- Gdy komputer wyłączy się nagle w trakcie aktualizacji, plik
  linux-port\docker\.env potrafi zostać wypełniony zerami. Serwer nie
  startował wtedy wcale (w logu: „unexpected character "\x00"”), a hasła do
  bazy postaci były tylko w tym pliku.
- Launcher sam naprawia taki plik przy następnym starcie albo aktualizacji:
  przywraca kopię z ostatniego udanego startu, a gdy jej nie ma, odczytuje
  ustawienia i hasła z kontenerów serwera. Uszkodzony plik zostaje obok jako
  kopia. Postacie i baza zostają nietknięte.
- Plik .env jest teraz zapisywany tak, żeby nagłe wyłączenie komputera go nie
  zniszczyło, a przy każdym starcie launcher zostawia jego kopię
  (.env.last-good).

### Kamienie Duszy według tierów Iwakury

- Boty wkładają Kamienie Duszy według nowej listy Iwakury: tylko +3 i +4, i
  tylko te, które do expienia ocenił co najmniej na 3. Kamień +3 albo +4
  trafia do przedmiotu od +6 w górę, a przedmiot +8 i +9 czeka na +4.
  Kamienie klasowe (Wojownika, Sury, Ninja, Szamana) są tylko do PvP i boty
  ich nie wkładają.
- Kamienie +0, +1 i +2 bot wkłada tylko wtedy, gdy mu wypadną, i tylko do
  słabego przedmiotu: do 21 poziomu i najwyżej +6. Rodzaj kamienia musi mieć
  u Iwakury ocenę co najmniej 3. Takich kamieni boty nie kupują.
- Na targu boty kupują tylko Kamienie Duszy +3 i +4.
- Przy wyborze ekwipunku liczą się włożone kamienie, więc bot nie zamieni
  przedmiotu z dobrymi kamieniami na goły.

### Nowe nicki od Iwakury

- Lista nicków urosła do 1800. Bot, który ma już nick, zostaje przy nim.
  Nowe nicki dostają tylko boty, które jeszcze nie mają nicku.

### Auto Łowy od Colide'a, wersja druga (klient 2.0.19)

- 12 przedmiotów na czasie, w dwóch rzędach.
- Podnoszenie ma własne, przesuwane okno „Auto Łowy – Łupy”, więc oba okna
  mieszczą się na ekranie 800×600.
- Ustawienia zapisane w poprzedniej wersji zostają.

### Klient 2.0.19

- Auto Łowy w drugiej wersji od Colide'a (opis wyżej).

## 2.0.83 — 2026-09-19

Serwer 2.0.83 i klient 2.0.18. W launcherze ZAINSTALUJ AKTUALIZACJE: nowe okno
magazynu potrzebuje obu.

### Magazyn: sortowanie, łączenie i rozdzielanie stosów (blasty)

- W oknie magazynu, obok tytułu, jest przycisk „Scal i uporządkuj”, taki sam
  jak w ekwipunku. Scala stosy tych samych przedmiotów i układa magazyn:
  najpierw mikstury, potem ekwipunek, księgi, ulepszacze, skrzynki i reszta.
- Przy otwartym magazynie ekwipunek działa normalnie: można w nim rozdzielać
  i łączyć stosy, a także go porządkować. Wcześniej serwer po cichu odmawiał
  każdego przesunięcia w torbie, dopóki magazyn był otwarty.
- Przedmiot przeciągnięty na taki sam przedmiot łączy się z nim: z magazynu
  do ekwipunku, z ekwipunku do magazynu i w samym magazynie. Gdy docelowy
  stos jest pełny albo leży tam inny przedmiot, czat mówi, czemu nic się nie
  stało.
- Shift i kliknięcie stosu w magazynie otwiera okno wyboru ilości, jak w
  ekwipunku. Wybraną część można upuścić w ekwipunku, na wolnym polu
  magazynu albo na takim samym przedmiocie. Część stosu z ekwipunku (też
  Shift i kliknięcie) trafia tak samo do magazynu.
- Boty układają swoje magazyny przy każdej wizycie.

### Wojny gildii: koniec z biciem się w kółko w miejscu (Hiob)

- Bot, który zginął na wojnie, wstaje niewidzialny, odpoczywa do trzech
  czwartych życia i dopiero wtedy wraca do walki. Wcześniej wstawał z jedną
  piątą życia wprost pod miecze zabójców i ginął od nowa, a wrogowie machali
  w niewidzialnego. Stąd boty, które „w nieskończoność biją się w miejscu”.
  Wrogowie nie atakują już wstających.
- Wyłączenie wojen w panelu kończy też wojnę, która trwa: boty od razu
  schodzą z pola bitwy i wracają do wioski. Wcześniej wyłącznik blokował
  tylko następną wojnę, a trwająca szła do końca swoich 30 minut.

### Okrzyk o ulepszeniu podaje stopień raz (archonek)

- Bot chwali się ulepszeniem tak: „Pajęcza Włócznia z +6 na +7, kowal dziś
  łaskawy”. Wcześniej stopień padał dwa razy: „no i mam +7 na Pajęcza
  Włócznia+7”.

### Klient 2.0.18

- Okno magazynu: przycisk „Scal i uporządkuj”, Shift i kliknięcie oraz
  łączenie przez przeciąganie (opis wyżej). Potrzebuje serwera 2.0.83.
- COOP w testach u patronów: Dolacz.bat mówi, którego VPN brakuje, gdy świat
  jest hostowany przez VPN.

## 2.0.82 — 2026-09-19

Serwer 2.0.82, klient bez zmian (2.0.17). W launcherze ZAINSTALUJ AKTUALIZACJE.

### COOP w testach u patronów

- Hostowanie w COOP działa też przez VPN (Radmin VPN, Tailscale, ZeroTier,
  Hamachi) tam, gdzie operator internetu nie daje publicznego adresu.

## 2.0.81 — 2026-09-19

Serwer 2.0.81, klient bez zmian (2.0.17). W launcherze ZAINSTALUJ AKTUALIZACJE.

### Tereny gildii z paczki są wolne (Mat, NerrVoVy)

- Baza z paczki przyniosła 28 terenów gildii i 62 budynki z serwera, z którego
  ją zrobiono, ale żadnej z gildii, do których należały. Te tereny uchodziły
  więc za zajęte: nie stał przy nich zarządca terenu i nie dało się ich kupić,
  a budynki stały na ziemi, której nikt nie miał. Gildia bota założona pod
  jednym z tamtych numerów „dostawała” cudzy teren z budynkami.
- Przy pierwszym starcie po aktualizacji te wpisy znikają: tereny z paczki są
  do kupienia, zarządca terenu stoi na każdym, a budynki z paczki znikają.
  Tereny kupione przez graczy i postawione na nich budynki zostają.

### Dropki nie robią próby konia (Urtopy)

- Dropki (Metinów, M2, M3, medali) to postacie do dropu: nie robią Biologa ani
  próby konia bojowego. Status „Zdobywam konia bojowego na pustyni (0/100)”
  u dropka stojącego w M3 był fałszywy i już się nie pokazuje, a dropek nie
  jest też kierowany na pustynię.
- Panel przy dropku pokazuje w Biologu „nie dotyczy — dropper nie robi
  Biologa” zamiast 0/7.

### Wieża Demonów: boty biją kilka potworów naraz (Nagash)

- Paczka botów na piętrze nie ustawia się już w kolejce do jednego potwora.
  Każdy bot bierze jednego z kilku potworów stojących najbliżej paczki
  (mniej więcej dwa boty na potwora), więc piętra idą szybciej, a boty dalej
  walczą w jednym miejscu. Metiny i bossów biją wszyscy razem.

### Launcher po aktualizacji (Urtopy)

- Po aktualizacji launcher uruchamia się ponownie sam. Na Windows 11 nowe
  okno potrafiło paść z błędem „cmd.exe — Aplikacja nie została właściwie
  uruchomiona (0xc0000142)”. Stare okno czeka teraz, aż nowe naprawdę
  wystartuje, a gdy nowe padnie, launcher próbuje drugi raz inną drogą.
  Zadziała od następnej aktualizacji, bo tę uruchamia jeszcze stary launcher.

## 2.0.80 — 2026-09-19

Serwer 2.0.80, klient 2.0.17. W launcherze ZAINSTALUJ AKTUALIZACJE.

### Nowe okno Auto Łowów (Colide)

- Okno Auto Łowów (klawisz K) przebudował Colide, gracz, który zrobił je
  dla siebie i przysłał: 12 umiejętności w dwóch rzędach, 6 miejsc na
  mikstury, każde z własnym progiem w procentach, i 6 przedmiotów używanych
  co podaną liczbę sekund (peleryny, rosy).
- Mikstura na manę sama pilnuje PE, a każda inna HP. Nie trzeba już trzymać
  czerwonej na pierwszym miejscu i niebieskiej na drugim.
- Przełączniki: Atak, Umiejętności, Wskrzeszenie, Mikstury, Odpały, Metiny
  i Wracaj. Umiejętności działają na swoich zegarach także przy wyłączonym
  ataku (wtedy wyłącz też Wracaj, bo postać wraca w miejsce startu).
- „HP po wskrz. %”: po odrodzeniu postać pije mikstury i nie rusza do walki,
  dopóki nie ma tylu procent HP, także po ręcznym „Odródź tutaj”.
- Ustawienia każdej postaci zapisują się w folderze autohunt obok klienta.
  Dotychczasowe ustawienia przechodzą same: umiejętności zostają na swoich
  miejscach, mikstury HP i PE trafiają na dwa pierwsze miejsca mikstur,
  a przedmioty do rzędu Odpały.

### Serwer nie budował się przy źle ustawionym zegarze Windows (Xewi)

- Docker bierze czas od Windows. Gdy zegar Windows jest cofnięty (u Xewi
  o 3 godziny), serwery pakietów odrzucają pobieranie („Release file ... is
  not valid yet”) i każde GRAJ zatrzymywało się na budowie panelu. Panel
  i serwer gry budują się teraz mimo takiego zegara.
- Launcher przy starcie porównuje zegar Windows z czasem z internetu,
  a zegar Dockera z zegarem Windows. Gdy któryś odbiega o ponad 5 minut,
  mówi, co ustawić: Ustawienia → Czas i język → Data i godzina → „Ustaw
  czas automatycznie” i „Synchronizuj teraz”, potem zamknij Docker Desktop
  i kliknij GRAJ.

### COOP w testach u patronów

- COOP, czyli gra ze znajomymi przez internet, jest na razie w testach
  u patronów: przycisk COOP w launcherze prosi o hasło, które dostają patroni.

## 2.0.79 — 2026-09-19

Serwer 2.0.79, klient bez zmian (2.0.16). W launcherze ZAINSTALUJ AKTUALIZACJE.

### Klątwa małp usunięta (SIZOWSKI)

- Quest z paczki mt2009 (`monkey_curse`) po 55 minutach w łatwym Lochu Małp
  (35 w średnim, 25 w trudnym) zamieniał postać na 5 minut w małpę i wyrzucał
  ją do wioski, chyba że działało zioło z małp. Boty tych ziół nie używają,
  więc dropiarze medali wylatywali z lochów jako małpy. Klątwy nie ma już dla
  nikogo, ani dla graczy, ani dla botów: obraz gry przy każdej budowie usuwa
  pliki tego questa, więc następna aktualizacja jej nie przywróci.
- Zioła z małp zostają w dropach, bo quest poboczny na poziomy 55–57 wymaga
  zioła z trudnych małp. Samo użycie zioła niczego już nie robi.

### Przedmioty na „stall” też mają limit linii na ladzie (Tieru)

- Przedmiot ustawiony w panelu (Polityka przedmiotów) na „stall” dalej trafia
  na ladę przed wszystkim innym i nigdy do handlarza, ale na jednej ladzie
  stoją najwyżej 3 linie tego samego przedmiotu, a marmurów najwyżej 3, każdy
  innego potwora. Reszta czeka w plecaku. Do tej pory taki wpis zapychał całe
  lady jednym przedmiotem: na naszym serwerze testowym Kawałek Lodu ustawiony
  na „stall” stał w 46 liniach na jednej ladzie.
- Linie ponad limit wracają z lad same, po jednej przy każdej wizycie
  sprzedawcy.

## 2.0.78 — 2026-09-18

Serwer 2.0.78, klient bez zmian (2.0.16). W launcherze ZAINSTALUJ AKTUALIZACJE.

### Aktualizacja, która nie mogła zbudować serwera (archonek)

- Jeśli w plikach serwera został `messenger_manager.cpp` z cudzej przeróbki
  (systemu towarzysza z innego forka), budowa rdzenia gry kończyła się
  błędem „'class CPlayerBotManager' has no member named 'GetCompanionOwner'”
  i każda aktualizacja od 2.0.74 przerywała się w tym samym miejscu.
  Aktualizacja podmieniała nasze pliki, a ten zostawiała.
- Paczka aktualizacji zawiera teraz fabryczny `messenger_manager.cpp`, więc
  po kliknięciu ZAINSTALUJ AKTUALIZACJE plik wraca do wersji z paczki
  i serwer się buduje. Przeróbka tego pliku (lista znajomych z forka) znika.
- Launcher przy takim błędzie budowy mówi teraz wprost, który plik silnika
  pochodzi z cudzej przeróbki i jak przywrócić fabryczny, zamiast ogólnego
  „Operacja nie powiodła się”.

### Różnorodność na ladach (Tieru, Hiob)

- Na jednej ladzie stoją najwyżej 3 linie tego samego przedmiotu. Do tej
  pory zdarzały się lady z 46 liniami Kawałka Lodu albo z kilkunastoma
  liniami jednej farby do włosów. Nadmiar wraca do torby sprzedawcy, po
  jednej linii przy każdej wizycie przy sklepie, i robi miejsce innemu
  towarowi. Księgi umiejętności, Zwoje Zapomnienia i Kamienie Duchowe mają
  dalej własne limity.
- Marmury Polimorfii: najwyżej 3 na ladzie, nigdy dwa tego samego potwora.
  Przez dwa dni nie sprzedał się ani jeden z 6581 wystawionych, a zapychały
  lady, na których brakowało miejsca na materiały.
- Materiał, którego w danej wiosce nikt nie wystawia, trafia na ladę przed
  zwykłym sprzętem. W drugich wioskach (Bokjung, Jayang, Bakra) sprzedawcy
  mieli brakujące materiały w torbach, ale ich lady były pełne.

### Broń na 30 poziom innej klasy (Tieru)

- Połowa broni na 30 poziom, których bot nie może założyć, idzie najpierw do
  kowala. Ulepsza je na zwykłym kowadle, bez zwojów, do sufitu z tabeli dla
  jej średnich obrażeń: do +7 przy średnich do 21%, do +6 przy 22–29%, do +4
  przy 30–36%. Gotowy przedmiot trafia na ladę droższy, a te, które spłoną,
  przestają zapychać lady. Broń z co najmniej 37% średnich albo 15% obrażeń
  umiejętności nie idzie na kowadło i trafia na ladę jak dotąd.
- Dla porządku: przy szansach tej rodziny (90/85/75/65/55/45/35/25/20%)
  +9 zostaje rzadkością. Od +5 do +9 pod Zwojem Błogosławieństwa to średnio
  ok. 200 zwojów, a na zwykłym kowadle dochodzi 0,8% broni.

### Farby do włosów i fryzury (Tieru)

- Farby do włosów z łowienia boty wyrzucają, tak jak robi większość graczy.
  Zostawiają jedną, jeśli same nie mają jeszcze koloru włosów, i bardzo
  rzadko (3 na 100) jakąś na sprzedaż. Farby już wystawione wracają z lad.
- Co trzeci sprzedawca, któremu Smocze Monety nie są potrzebne na nic
  własnego, kupuje w ItemShopie fryzurę, której sam nie założy, i wystawia
  ją na ladę z ceną wyjściową ok. 2 mln yang (więcej przy wyższych
  stawkach yang).
  Fryzury z ItemShopu w tej paczce nie mają bonusów, więc sprzedaje się
  wygląd. Monety boty mają z kuponów z metinów i bossów, więc przy domyślnej
  szansie na kupon takich fryzur będzie niewiele.

### Wojny gildii: pole bitwy dalej od strefy bezpiecznej (gregory_955)

- Pole bitwy na mapach gildyjnych Chunjo i Jinno stało przy samej granicy
  strefy bezpiecznej (50–100 jednostek od niej), więc część walczących
  zawsze stała w strefie, gdzie nie da się zadać ciosu. W wojnie Chunjo
  z 18 września było to 11 z 67 botów. Pole bitwy trzyma się teraz co
  najmniej 800 jednostek od strefy: na mapie Chunjo przesuwa się o ok. 1000
  jednostek, na mapie Jinno o ok. 800.
- Ostatnia para wojny zapisuje się w bazie, więc restart przy aktualizacji
  nie oddaje pierwszej wojny tym samym dwóm gildiom.

## 2.0.77 — 2026-09-18

Serwer 2.0.77, klient bez zmian (2.0.16). W launcherze ZAINSTALUJ AKTUALIZACJE.

### Aktualizacja i zbieranie logów (archonek, Urtopy, DUDU)

- „ZBIERZ / WYŚLIJ LOGI” w 2.0.76 kończyło się błędem „You cannot call
  a method on a null-valued expression” na każdym serwerze bez drugiego
  kanału. Naprawione, działa od razu po zainstalowaniu tej aktualizacji.
- Starszy Docker Compose próbował przy starcie pobrać z internetu obraz
  panelu zaawansowanego (metin2/seban-panel), który serwer buduje sam,
  i przerywał start albo aktualizację błędem „pull access denied”. Od
  2.0.77 go nie pobiera.
- Launcher pokazywał „Kanał aktualizacji nie został jeszcze opublikowany”,
  gdy w logach nieudanej akcji trafiła się gdziekolwiek liczba 404, choć
  manifest był na miejscu. Teraz ten komunikat oznacza tylko prawdziwy brak
  manifestu, a błąd pobierania obrazu ma własną wskazówkę.
- Aktualizacja klienta odmawia, gdy gra jest włączona. Zamknij grę (sprawdź
  też Menedżer zadań, czy metin2client.exe nie został w tle) i kliknij
  ZAINSTALUJ AKTUALIZACJE jeszcze raz.

### Więcej materiałów na ladach (Hiob, Xewi)

- Rynek botów liczył tylko to, czego brakuje botom, a zakupów gracza nie
  widzi, więc większość materiałów uznawał za nadmiar. Boty trzymały
  w torbach i magazynach ok. 1,3 mln sztuk materiałów, a na ladach było
  ok. 105 tys. W prawie co trzeciej parze „materiał × wioska” nie było ani
  sztuki (np. Czarny Uniform w Pyongmoo i Bakrze, Księga Klątw i Ząb Orka
  nigdzie).
- Teraz każda wioska ma na ladach co najmniej 50 sztuk każdego materiału do
  ulepszania, który boty w niej mają ponad własny zapas na kowala. To, czego
  bot potrzebuje do własnych ulepszeń, zostaje w torbie.
- Sklepy botów uzupełniają się szybciej: budżet zmian w sklepach offline
  jest dwa razy większy i nie gubi wolnych sekund.
- Nowe linie przybywają stopniowo, bo sprzedawca dokłada jedną przy każdej
  wizycie przy sklepie (co 10–15 minut), więc ladom trzeba dać godzinę
  lub dwie.
- Wyszukiwarka przedmiotów pokazuje sklepy tylko z mapy, na której stoisz.
  Szukaj w wiosce, w której stoją sklepy.

### Wojny gildii (gregory_955)

- Boty nie gonią już przeciwników stojących w strefie bezpiecznej, gdzie
  nikogo nie da się uderzyć. Bot, który sam stoi w strefie, najpierw
  wychodzi na miejsce zbiórki.
- Te same dwie gildie nie walczą już co dwie godziny. Para z ostatniej
  wojny w królestwie czeka, a pierwszeństwo mają gildie, które dawno nie
  walczyły.
- Wynik trwającej wojny widać w panelu na stronie gildii
  (⚔ z gildią … wynik:wynik).

### Czerwony Las

- Punkt wyjścia z Czerwonego Lasu stał na zablokowanym terenie: boty, które
  chciały opuścić mapę, co 20 sekund planowały drogę donikąd. Wejście,
  wyjście i po jednym hubie w obu Lasach przeniesione na sprawdzony teren.

## 2.0.76 — 2026-09-18

Serwer 2.0.76, klient 2.0.16. W launcherze ZAINSTALUJ AKTUALIZACJE — podnoszenie
pod tyldą, drugi kanał i poprawione okno juków potrzebują nowego serwera
i nowego klienta.

### Cały drop pod tyldą ` (vanderro, SIZOWSKI)

- **Z** działa jak dotąd — podnosi jeden przedmiot.
- **`** (tylda, klawisz nad Tab) podnosi naraz wszystko, co leży w zasięgu
  podnoszenia i należy do Ciebie albo do Twojej grupy, zaczynając od
  najbliższego. Gdy plecak się zapełni, przestaje i nic nie ląduje na ziemi.
  Przytrzymany klawisz ponawia co pół sekundy.

### Liczba botów osobno dla każdego królestwa (Greess)

- Launcher → **LICZBA BOTÓW** → **Indywidualne wartości dla królestw**.
  Trzy pola: Shinsoo (czerwone), Chunjo (żółte), Jinno (niebieskie). Każde
  królestwo uruchamia tyle botów, ile mu wpiszesz, ale nie więcej, niż ma
  postaci. Nagłówek okna pokazuje ich sumę.
- Bez zaznaczenia wszystko działa jak dotąd: jedna liczba dzielona po równo.
- Menu tekstowe launchera też o to pyta. Zmiana działa po restarcie serwera.

### Drugi kanał (CH2) — domyślnie wyłączony (pomysł i pomiary: SIZOWSKI)

- Włączasz go w launcherze (**LICZBA BOTÓW** → **Drugi kanał (CH2) dla botów
  i graczy**, z ustawieniem, ile procent botów gra na CH2; domyślnie 40)
  albo w panelu WWW na stronie Stawki.
- Serwer uruchamia wtedy trzy kolejne rdzenie gry, a boty dzielą się między
  dwa kanały. Serwer rozkłada je więc na dwa rdzenie procesora i udźwignie
  więcej botów naraz. Każdy bot gra zawsze na tym samym kanale.
- Liczba botów z launchera dotyczy całego świata: najpierw dzieli się na
  królestwa, potem każde królestwo między kanały. Tak samo liczby
  indywidualne dla królestw.
- **Wszystkie sklepy offline, botów i graczy, stoją tylko na CH1.** Na CH2
  nie da się otworzyć sklepu. Bot, który kiedykolwiek miał sklep, gra na CH1
  na stałe, żeby zawsze mógł obsłużyć swoją ladę.
- Boty z CH2 nie handlują: nie wystawiają sklepów i nie kupują z lad, bo te
  stoją na CH1, a bot nie przechodzi na inny kanał. Towar, którego nie
  sprzeda, przy pełnym plecaku oddaje handlarzowi albo odkłada do magazynu.
  Im większa część botów na CH2, tym mniej botów handluje.
- Wojny gildii botów wypowiada CH1, a walczą w nich boty z obu kanałów.
  Rajdy Wieży Demonów, zakładanie gildii i eventy czasowe prowadzi CH1.
  Boty z CH2 oddają swojej gildii doświadczenie jak dotąd.
- Po włączeniu z launchera otwierane są porty 13010–13012. Klient 2.0.16
  pokazuje CH2 na liście kanałów tylko wtedy, gdy serwer go uruchomił.
- Zmiana działa po restarcie serwera. Ustawienie z panelu WWW przenosi boty
  na CH2 przy najbliższym restarcie. Gracze wejdą na CH2, gdy launcher
  otworzy porty, czyli po następnym GRAJ.
- CH2 zajmuje około 1 GB pamięci więcej.

### Płynny ruch przy dużej liczbie botów (SIZOWSKI)

- Budżet czasu z 2.0.74 zostawiał na chwilę bez ruchu boty, do których
  przebieg AI jeszcze nie dotarł. Przy ponad ~1100 botach na jednym rdzeniu
  „robiły dwa kroki i stawały”.
- Teraz każdy bot w każdym takcie idzie dalej swoją trasą i bije cel,
  a na swoją kolej czeka tylko planowanie. Linia `PLAYERBOT_LOAD` ma nowe
  pole `light_ms`.

### Wyrzucanie z serwera przy szkoleniu konia (Dearminder)

- Rdzeń padał, gdy zadanie usuwało swój własny cel w chwili dojścia do punktu
  (szkolenie konia na Ognistej Ziemi, trzeci punkt). Po ponownym zalogowaniu
  obok punktu padał znowu.
- Przyczyną był błąd w silniku zdarzeń: zdarzenie anulowane w trakcie
  działania zapisywało do zwolnionej pamięci. Po aktualizacji misja idzie
  dalej, także z miejsca, w którym utknęła.

### Szybkość biegu z panelu działa (archonek)

- „Szybkość biegu” na stronie postaci dawała na tej wersji serwera punkty many
  zamiast prędkości. Teraz daje prędkość, na 30 dni albo do wybrania
  „Normalna”.
- Nie rusza serwerowego bonusu biegu ani premii od Biologa.
- Przy pierwszym użyciu zdejmuje manę dodaną wcześniej przez pomyłkę.

### Okno juków konnych liczy wszystkie strony ekwipunku (blasty)

- Wymagania odblokowania juków i wymagania sklepów specjalnych liczyły
  przedmioty tylko z dwóch pierwszych stron ekwipunku. Od czterech stron okno
  pokazywało „0 na 60”, choć przedmioty leżały na stronie III, IV albo w jukach.
- Serwer przez cały czas liczył poprawnie, poprawione jest samo okno
  (klient 2.0.16).

### Boty nie oddają handlarzowi cennych rzeczy

- Czerwone Nasiono, Zwój Kamienia Duszy, Instr. Oswajania Konia,
  Zaaw. Ks. Polimorfii i reszta przedmiotów, które cennik Iwakury wycenia
  na ponad 5000 yang, idą na stragan zamiast do handlarza. Handlarz dostaje je tylko
  przy pełnym plecaku, gdy nie da się otworzyć straganu.
- Materiały z receptur ulepszeń wyższych poziomów boty zatrzymują na własne
  ulepszanie.

### Tłum botów na jednym wzgórzu w wiosce (Remigiusz)

- Bot, który przerósł wszystkie tereny wioski, szedł zawsze na te same dwa
  miejsca. Przykład: poziom 28–35 w Joan, gdzie najsilniejsze potwory mają
  około 21 poziomu.
- Teraz rozchodzi się po najwyższych terenach wioski, na co najmniej sześć
  miejsc.

### Panel zaawansowany

- W Zarządzaniu wróciło **🎁 Masowe nadawanie przedmiotów**. Link zniknął przy
  aktualizacji panelu Sebana 16 września, a sama strona działała cały czas
  (archonek, DUDU).
- „Plan wejścia botów” jest opisany prostym językiem. Na serwerze bez
  integracji Sebana zamiast niedziałającego formularza jest wskazówka, gdzie
  to ustawić: launcher, LICZBA BOTÓW (Remigiusz).
- Oba panele pokazują też boty z CH2.

### Na VPS: brak ikon przedmiotów w panelu WWW (DUDU, GorącyDelfin)

Serwer aktualizowany z wersji starszej niż 2.0.74 skryptem `update.sh` nie
kopiował ikon do panelu. Skrypt z 2.0.74 robi to już sam, ale pierwszą
aktualizację po nim wykonuje jeszcze stara wersja. Jeśli ikon nadal nie ma,
raz w folderze serwera:

```
sh linux-port/tools/update.sh stage
cd linux-port/docker && docker compose up -d --build panel
```

## 2.0.75 — 2026-09-18

Serwer 2.0.75, klient 2.0.15. W launcherze ZAINSTALUJ AKTUALIZACJE — przycisk
porządkowania potrzebuje nowego serwera i nowego klienta.

### „Scal i uporządkuj” — jedno kliknięcie, cały ekwipunek (Tieru; plan z audytu Codexa)

Przycisk w oknie ekwipunku (dotąd „automatyczne łączenie”) wysyła teraz do
serwera jedno polecenie. Serwer w ułamku sekundy:

- łączy stosy tego samego przedmiotu, ale tylko identyczne — te same kamienie,
  bonusy i wygląd — i tylko do limitu stosu danego przedmiotu; najpełniejszy
  stos zostaje, a wyczerpane znikają tak jak przy ręcznym łączeniu;
- układa wszystkie cztery strony od nowa według rodzaju: najpierw mikstury,
  potem broń, zbroje i biżuteria, księgi, ulepszacze i kamienie, skrzynie
  i klucze, inne przedmioty użytkowe, rybactwo i zbieractwo, przedmioty
  z misji i cała reszta;
- przepina skróty z paska na nowe miejsca przedmiotów.

Nic nie jest tworzone od nowa: każdy przedmiot zachowuje swoje kamienie,
bonusy i numer. Strona konia, pas, założony sprzęt, smocze kamienie i magazyn
zostają nietknięte, a włączony Eliksir Słońca lub Księżyca zostaje na swoim
polu. Przy prawie pełnym ekwipunku serwer próbuje kilku sposobów ułożenia
i wybiera taki, który się mieści — nigdy nie wyrzuca niczego na ziemię.

Porządkowanie nie działa podczas handlu, przy otwartym sklepie, magazynie,
ulepszaniu i innych oknach (czat powie dlaczego), po śmierci, z przedmiotem
trzymanym kursorem ani częściej niż co 2 sekundy. Wynik — ile przedmiotów
przestawiono i ile stosów połączono — pojawia się na czacie dopiero wtedy,
gdy serwer skończy. Drugie kliknięcie na uporządkowanym ekwipunku niczego nie
rusza.

Dotychczasowy przycisk wysyłał setki pojedynczych ruchów (wcześniej wyrzucał
za to z gry) i potrafił tylko łączyć stosy.

### Boty porządkują torby tak samo

Boty na linii 2.x co mniej więcej pół godziny robią dokładnie to samo
porządkowanie zamiast dotychczasowego przesuwania mikstur na początek, więc
ich torby oglądane w panelu są ułożone jak po kliknięciu przycisku. Łączenie
stosów co 5 minut zostaje bez zmian.

### Liczba potworów w respie (Kiciamol)

Na stronie **Stawki** panelu jest nowa karta **„Liczba potworów w respie”**:
osobno dla Metinów i bossów, osobno dla zwykłych potworów, od ×1 do ×4
(także ×1,5 i ×2,5). ×2 znaczy, że w każdym miejscu respu stoi dwa razy
więcej potworów niż normalnie. Nie trzeba restartu, a ustawienie zostaje po
restarcie. Dodatkowe potwory pojawiają się przy najbliższym odrodzeniu
danego miejsca: zwykłe potwory w ciągu kilku minut, Metiny i bossowie po
swoim czasie odradzania (zwykle 15–25 minut).

Bez zmian zostają postacie niezależne (sklepikarze, portale, konie), żyły
rud i krzaki ziół, lochy i jednorazowe respy z misji. Po zmniejszeniu
mnożnika nadmiarowe potwory znikają dopiero wtedy, gdy ktoś je zabije.

Uwaga: więcej potworów to więcej pracy dla serwera i botów.

### „Czas odradzania” Metinów i bossów wreszcie działa

Pole „Metiny i bossowie” w karcie „Czas odradzania” (od 2.0.64) nie
działało: serwer sprawdzał, czy linia respu to boss albo Metin, zanim
odczytał, co to za potwór, więc żadna linia nim nie była, a bossowie
i Metiny odradzali się według pola zwykłych potworów. Teraz każde pole
działa na swoją grupę. Linie zapisane jako „grupa grup” serwer sprawdza tak
samo jak zwykłe grupy, więc Metin z takiej linii też liczy się jako Metin.
Pole „Metiny i bossowie” obejmuje też żyły rud i krzaki ziół, które stoją
w plikach Metinów.

### Launcher: aktualizacja klienta przy włączonej grze (Ratorex)

Gdy gra była uruchomiona (albo jej kopia wisiała w tle), aktualizacja
klienta pobierała całą paczkę i dopiero przy podmianie pliku kończyła się
komunikatem Windowsa „plik jest używany przez inny proces”. Launcher
sprawdza to teraz przed pobieraniem — przy „aktualizuj wszystko” jeszcze
przed aktualizacją serwera — i mówi wprost: zamknij grę, sprawdź Menedżer
zadań, kliknij ponownie. Poprawka launchera działa od następnej
aktualizacji po tej (aktualizację wykonuje zawsze poprzednia wersja).

### Reset świata i kopia na dużym świecie (uxietoszef)

NOWY ŚWIAT (reset) i KOPIA kończyły się błędem „Stream was too long”
(„Strumień jest za długi”), gdy któryś zrzut bazy — zwykle historia w bazie
`log` — przekraczał 2 GB. Reset zawsze najpierw robi kopię, więc na takim
świecie nie dało się go zrobić (świat zostawał nietknięty). Kopia jest teraz
pakowana w sposób, który nie ma tego limitu. Sprawdzone na pliku 2,4 GB,
razem z przywróceniem kopii. Po aktualizacji wystarczy spróbować jeszcze
raz — ta poprawka działa od razu, nie dopiero od następnej wersji.

### Skrót z paska przy miksturach w pasie

Gdy kończył się stos mikstur położony w pasie, skrót mógł przeskoczyć na
zupełnie inny przedmiot w torbie (błąd z 2.0.74, gdy pas przesunął się razem
z czterema stronami ekwipunku).

### Sprawdzone przed wydaniem

Na świecie testowym (1099 botów, cztery strony ekwipunku):

- **Porządkowanie botów:** w pierwszych dziesięciu minutach 397 uporządkowanych
  toreb (mediana 68 przedmiotów, 61 przestawionych), najdłuższe trwało poniżej
  milisekundy. Serwer po każdym porządkowaniu przelicza przedmioty — ani razu
  nie zgadzało się inaczej niż przed nim — a po restarcie baza nie miała ani
  jednego podwójnie zajętego pola w 2505 torbach.
- **Przycisk gracza:** sprawdzony testami modułu klienta (Python 2.7 i 3); w
  samym kliencie jeszcze nie klikany.
- **Liczba potworów w respie:** przy ×1 na mapach stało około 41 tys.
  potworów, po ustawieniu ×2 w panelu 80 tys. po dwóch minutach. Przy
  Metinach na ×2 i zwykłych potworach na ×1 Metinów przybywało w miarę
  odradzania się kolejnych miejsc (113 → 231 w dwadzieścia minut), a żyły
  rud i zioła wracały pojedynczo, tak jak powinny. Nadmiarowe zwykłe potwory
  po powrocie z ×2 na ×1 schodziły same, przez zabijanie (81 tys. → 72 tys. w
  dwadzieścia minut). Przy ×2 czas pracy botów na tym świecie wzrósł z około
  13–14 do 16–18 sekund na minutę.
- Wszystkie testy (C++ i Pythona na wersjach 2.7 i 3, testy launchera) przechodzą.

## 2.0.74 — 2026-09-18

Serwer 2.0.74, klient 2.0.14. **Serwer i klient muszą mieć tę samą wersję.**
Stary klient nie zaloguje się do nowego serwera. Przycisk ZAINSTALUJ
AKTUALIZACJE w launcherze aktualizuje oba naraz.

### Cztery strony ekwipunku

Ekwipunek ma cztery strony (I–IV, 180 pól) zamiast dwóch. Dotyczy to graczy i
botów.

- **Wyjątek od reguły „aktualizacja nie rusza bazy”.** Przy pierwszym starcie
  serwer przenosi w bazie przedmioty, które leżały za drugą stroną: stronę
  konia i pola pasa przesuwa na ich nowe miejsca, a pasek skrótów zapisuje w
  nowym, szerszym formacie. Dzieje się to raz, zanim ktokolwiek się zaloguje.
  W logu rdzenia bazy zostaje linia `INVENTORY_PAGES: four pages ...`.
  **Tego nie da się cofnąć:** powrót do 2.0.73 lub starszej wersji wymaga
  kopii świata sprzed aktualizacji. Jeśli chcesz mieć drogę powrotu, zrób ją
  przed aktualizacją (launcher: KOPIA / NOWY SWIAT).
- Starszy klient przy logowaniu dostaje komunikat „Wymagana aktualizacja
  klienta gry przez Patcher.” Wystarczy zainstalować aktualizację w launcherze.
- Boty też mają cztery strony, więc dłużej noszą łupy, zanim je sprzedadzą
  albo odłożą do magazynu.
- Oba panele pokazują w podglądzie postaci wszystkie cztery strony. Panel
  klasyczny nie pokazywał wcześniej niczego ze stron III i IV, a panel Sebana
  rysował te przedmioty na stronie II.

### Klient 2.0.14

- Klient jest skompilowany u nas z kodu źródłowego z paczki: to ten sam
  klient co 2.0.13, z czterema stronami ekwipunku.
- Discord Rich Presence pokazuje „Metin2 SinglePlayer”, a przycisk prowadzi
  na kanał YouTube. Przycisków nie widać na własnym profilu — tak działa
  Discord, a nie błąd.
- Auto Łowy: z włączonymi Metinami kamień ma pierwszeństwo przed potworami.
  Cel, do którego postać nie może dojść, jest pomijany przez minutę, zamiast
  wracać do niej od razu (blasty).

### Szkatułki Blasku Księżyca tylko w trakcie eventu (NerrVoVy)

Szkatułki wypadały także poza eventem, z dwóch powodów:

- rdzeń, na którym nie ma botów (w układzie `unified` rdzenie first i game2),
  nigdy nie zamykał u siebie bramki skrzynek;
- bez żadnego harmonogramu bramka stała otwarta wszędzie.

Teraz każdy rdzeń ma własny zegar, a Szkatułki wypadają wyłącznie w trakcie
eventu skrzynkowego ze strony Eventy (w harmonogramie albo po „Aktywuj
teraz”). Bez eventu nie wypadają wcale.

### Logowanie na dużym świecie (SIZOWSKI)

Rdzeń gry obsługuje boty i graczy w jednym wątku. Długi przebieg botów (przy
starcie ponad 0,7 s) wstrzymywał więc każde logowanie. Teraz przebieg botów
ma budżet 120 ms, a to, co się nie zmieści, dokończy następny przebieg ćwierć
sekundy później. Na serwerze testowym z budżetem 20 ms najdłuższy przebieg
przy 1099 botach spadł z 343 do 34 ms. Budżet ustawia klucz `TICK_MS` w
pliku wag (0 oznacza brak budżetu).

### Boty

- Boty poniżej 40 poziomu używają zielonych kamieni bonusów (dodawania i
  zmiany) na broni i zbroi do 40 poziomu. Do tej pory kamienie leżały im w
  torbach (Sammy).
- Siano, marchewki i Księgi Misji nie trafiają już do handlarza: boty je
  podnoszą i wystawiają w sklepach (Greess).

### Panel na Linuksie / VPS (DUDU)

Budowa panelu na Linuksie kończyła się błędem `/schema: not found`. Paczka nie
wiozła schematu, a katalog panelu przygotowywał tylko launcher na Windowsie.
Teraz przygotowuje go `update.sh` przed budową, a paczka zawiera schemat.

### Sprawdzone przed wydaniem

Serwer testowy z 1099 botami:

- logowanie nowym klientem i przenoszenie przedmiotów między stronami I–IV
  sprawdził Tieru;
- migracja poszerzyła 1143 paski skrótów;
- przez 17 minut po zmianie nie wypadła żadna Szkatułka, a godzinę wcześniej
  w 6 minut było ich 39;
- bot na 25 poziomie dodał bonusy zielonymi kamieniami do Gilotynowego
  Ostrza+7 i Zbroi Płytowej Tygrysa+6;
- boty podnosiły Księgi Misji około trzy razy częściej;
- oba panele pokazują strony I–IV.

## 2.0.73 — 2026-09-18

Serwer 2.0.73, klient bez zmian (2.0.13).

### Księgi i Kamienie Duchowe na ladach po jednej sztuce (przegląd Codexa)

Bot kupuje z lady linię tylko wtedy, gdy cała mieści się w tym, czego mu brakuje.
Tymczasem sklep offline wystawiał księgi i Kamienie Duchowe całymi stosami (do
dziesięciu sztuk), więc bot, któremu brakowało trzech kamieni, nie mógł kupić
dziesięciu. Teraz nadwyżka idzie na ladę po jednej sztuce, najwyżej trzy linie
jednego rodzaju naraz (księgi liczone osobno dla każdej umiejętności). Linia
kamieni dłuższa niż trzy wraca przy najbliższej wizycie do torby, żeby wyjść
ponownie po sztuce.

Poprawiona jest też rezerwa, którą bot zostawia sobie:

- Kamienie Duchowe: liczone były tylko kamienie w komórkach torby *przed* danym
  stosem, więc jeden stos dziesięciu kamieni przy rezerwie trzech nigdy nie
  trafiał na ladę. Teraz rezerwa liczy się w sztukach w całej torbie, a na ladę
  idzie wyłącznie nadwyżka.
- Księgi własnej umiejętności: rezerwa („dwanaście ksiąg do czytania”) liczyła
  stosy zamiast sztuk, więc przy stosach po dziesięć bot trzymał do stu
  dwudziestu ksiąg jednej umiejętności. Teraz dwanaście znaczy dwanaście.
- Bot nigdy nie wystawia tego, co za chwilę sam chciałby kupić: rezerwa
  sprzedawcy to dokładnie ta liczba, której brakuje kupującemu.

### Bot nie „czyta” już księgi, której silnik mu nie da

Silnik czyta księgę tylko postaci, która ma przy sobie 20 000 doświadczenia
(każde czytanie tyle kosztuje). Z mniejszą ilością odmawia, zostawia księgę —
a bot próbował dalej co 8 sekund i w logu wyglądało to jak nieudane czytanie.
Na serwerze testowym w 12 minut było 7 095 takich prób i tylko 23 prawdziwe
odczyty. Najczęściej dotyczyło to dropperów z zablokowanym doświadczeniem i
botów na 40 poziomie w drugich wioskach. Teraz bot czeka na doświadczenie, a
log mówi to wprost (`book read waits for experience`).

### Pomiary

W logu serwera każda sprzedaż z lady offline ma teraz linię
`PLAYERBOT_OFFLINE: sold` z czasem, przez jaki oferta stała na ladzie.

### Sprawdzone przed wydaniem

Serwer testowy z 1099 botami, 32 minuty nowej wersji wobec 37 minut 2.0.72:
80 linii ksiąg poszło na lady po jednej sztuce i żadna nie naruszyła rezerwy
właściciela, boty kupowały 93 księgi na godzinę wobec 69, na lady trafiały 872
linie na godzinę wobec 736, a pustych prób czytania było 57 na godzinę zamiast
33 383. Zero resetów strażnika bezczynności i żadnego padu rdzenia.

## 2.0.72 — 2026-09-18

Serwer 2.0.72, klient bez zmian (2.0.13).

### Boty kupują to, czego brakuje im do rozwoju (Tieru; wdrożył Codex)

Do tej pory bot kupował z lad materiały, sprzęt i kilka rzeczy strategicznych,
a księgi umiejętności, Kamienie Duchowe i okazy dla Biologa zdobywał raczej
przypadkiem. Teraz liczy, czego mu brakuje, i kupuje dokładnie tyle:

- księgi umiejętności, która jest na poziomie M — do zapasu dwunastu;
- Kamienie Duchowe, gdy ma umiejętność na G1–G10 — do trzech;
- okazy do otwartej zbiórki Biologa (Ząb Orka, Księga Klątw, Pamiątka po
  Demonie) — tyle, ile brakuje do oddania.

Linia z lady jest kupowana tylko wtedy, gdy cała mieści się w tej potrzebie,
więc bot nie wykupi stosu dwudziestu ksiąg, bo brakowało mu jednej. Na zakup
rozwojowy przeznacza najwyżej 30% własnych wolnych pieniędzy i nie zapłaci
więcej niż dwukrotność uczciwej ceny. Tuż przed zakupem jeszcze raz sprawdza
cenę i to, czy wciąż czegoś potrzebuje — zmieniona cena albo rzecz już zdobyta
odwołują zakup.

Co jakiś czas bot wybiera się też na targ pierwszej wioski, kiedy na ladach są
księgi, kamienie albo okazy, a jemu ich brakuje. Naraz robi to najwyżej 3% botów
— bez tłumu przy bramach — a droppery, boty w drużynie gracza i boty w trakcie
próby konia nie wybierają się wcale. Jeśli bot sam wystawił coś, czego teraz potrzebuje, zdejmuje to z własnej
lady.

Przeglądając duży sklep, bot nie zaczyna już za każdym razem od początku: dociera
też do pozycji dalszych niż 64. na ladzie.

### Księgi i trening arcymistrza

- Gdy umiejętność główna czeka na kolejną księgę, bot czyta księgę innej swojej
  umiejętności, zamiast nie czytać nic.
- Przy włączonym przełączniku ksiąg w panelu bot nie zużywa już Zwoju Egzorcyzmu,
  który i tak nic by mu nie dał.
- Trening arcymistrza pomija umiejętność, na którą bota nie stać rangą, i trenuje
  inną. Gdy żadnej, bot poluje, żeby rangę odrobić.

### Sklepy offline botów

- Przeceny i dokładanie towaru nie blokują się już nawzajem: bot przecenia dwie
  linie, potem przychodzi kolej na nowy towar, i tak na zmianę. Wcześniej sklep,
  któremu zawsze było co dołożyć, nie przeceniał niczego.
- Przecena idzie co godzinę, a co 10 minut tylko wtedy, gdy w trakcie gry zmieniono
  kurs yang w panelu. Restart serwera nie uruchamia już przeglądu wszystkich lad od
  nowa: pierwsza wizyta bota przy sklepie po restarcie dokłada towar. Wszystkie
  sklepy dzielą jedną operację na sekundę, a ten przegląd zabierał ją dokładaniu
  towaru i zakupom botów przez pierwsze minuty po starcie.
- Oferta, która stała na ladzie już przed restartem, jest przeceniana za zaleganie
  od chwili, gdy bot pierwszy raz po restarcie ją przelicza.
- Kamień Duchowy może trafić na ladę, jeśli bot ma go ponad własny zapas
  (wcześniej nigdy).
- W logu serwera co minutę jest linia `PLAYERBOT_OFFLINE: budget`: ile operacji na
  sklepach offline boty dostały, a ile musiało poczekać.

### Sprawdzone przed wydaniem

Serwer testowy, 1099 botów, 15 minut po restarcie, w porównaniu z 2.0.71: towaru
dokładanego na lady tyle samo (170 wobec 169 pozycji), wypraw na targ po rzeczy
do rozwoju naraz najwyżej 32, zero resetów strażnika bezczynności i żadnego padu
rdzenia. Boty
kupiły mniej ksiąg (9 wobec 24), bo nie dokupują już ponad własny zapas —
wcześniej liczyły tylko część ksiąg z własnej torby.

## 2.0.71 — 2026-09-18

Serwer 2.0.71, klient bez zmian (2.0.13).

### Boty uczą się zielarstwa (Iwakura)

Paczka od zawsze zawierała cały system zielarstwa u Baek-Go — misję startową,
jego sklep z Nożykiem Zielarza i pustymi butelkami oraz 77 receptur w tabeli
wytwarzania — i nikt go nigdy nie użył. Receptury wypadające z metinów boty
sprzedawały handlarzowi jako nieznany przedmiot, a zioła szły na lady jako
zwykły towar.

Teraz bot przechodzi misję startową u Baek-Go na tych samych warunkach co
gracz (dziesięć Kwiatów Brzoskwini), czyta zdobyte receptury z tą samą szansą
i tym samym licznikiem wiedzy co człowiek, a przy warsztacie kupuje butelki po
cenie z jego sklepu, zużywa zioła, płaci i rzuca na szansę — dokładnie w
kolejności, jakiej używa okno wytwarzania, łącznie z tym, że **nieudana próba
zużywa materiały**. Gotowe wzmocnienia bot pije przed bossem i metinem, a
nadwyżkę wystawia na ladzie: do tej pory mikstur zielarskich nie dało się
kupić w tym świecie nigdzie.

Po drodze okazało się, że boty podnosiły z ziemi tylko dwa z szesnastu ziół —
Korzeń Gango i Grzyb Tue, bo tych chcą misje Biologa. W bagażach leżało 86 496
korzeni i 14 515 grzybów, a Kwiatu Brzoskwini, którego wymaga misja startowa,
było w całym świecie **jedenaście sztuk**. Teraz boty zbierają wszystkie zioła
zielarskie i zostawiają sobie zapas, zanim reszta trafi na ladę.

Uwaga dla zbierających: zioła Biologa i zioła na mikstury to osobne przedmioty
mimo identycznych nazw. Pierwsze wypadają tylko przy aktywnej misji i idą
prosto do torby, drugie lecą na ziemię jak każdy inny łup — boty pracują
wyłącznie na tych drugich.

### Bot z małym HP nie ucieka już bez końca (Urtopy)

Bot, który zaczął taktyczną ucieczkę, mógł z niej nigdy nie wyjść: warunkiem
było, żeby potwór przestał go ścigać, a potwór, który nie może dojść — bo bot
stoi za ścianą na skraju mapy — nie przestaje nigdy. Do tego ucieczka nie
miała żadnego limitu czasu, a strażnik bezczynności jej nie łapał, bo bot cały
czas biegał. Efekt: zdrowa postać dreptająca po tym samym skrawku pustyni
godzinami.

Ucieczka kończy się teraz po oddaleniu się na bezpieczny dystans albo po
minucie, niezależnie od tego, co potwór o tym sądzi. Powód trafia do logu.

### Broń 30 poziomu: kowal do progu, dalej zwoje (Tieru)

Ile stopni bot wywalczy u kowala, zależy teraz od średnich obrażeń broni — im
lepsza broń, tym wcześniej przestaje ryzykować:

- do 14% — kowal do +7, a powyżej i tak czasem spróbuje;
- 15–21% — kowal do +7;
- 22–29% — kowal do +6;
- 30–36% — kowal do +4;
- od 37% — wyłącznie zwoje, od +0.

Bez zwoju powyżej swojego progu bot czeka, zamiast spalić dobrą broń.
(Sprawdzone w tabeli ulepszeń: dla broni 30 lv kowal nigdy nie daje stu
procent — szanse spadają od 80% na +1 do 10% na +8.)

Zmieniło się też, co bot z taką bronią robi: dwie trzecie sztuk zatrzymuje dla
siebie i ulepsza, resztę wystawia. Wcześniej praktycznie wszystkie lądowały na
ladach nieulepszone — w dniu wydania stało ich tam 2603 na +0, a nosiło je 28
botów.

### Biolog: kolejka wypraw posprzątana

Trzy poprawki z audytu:

- odczyt misji Biologa przydzielał i zwalniał miejsca na wyprawy przy każdym
  wywołaniu — a robi to również panel, więc samo zaglądanie do bota zmieniało
  grę. Teraz miejsca przydzielają tylko te przebiegi, które faktycznie
  decydują o podróży;
- bot, który niósł już okazy, mijał własną misję, gdy nie dostał miejsca w
  kolejce. Oddawanie okazu nie jest wyprawą i nie wymaga już miejsca;
- po wyczerpaniu czasu wyprawy bot wraca na koniec kolejki, zamiast zajmować
  zwolnione miejsce od razu z powrotem.

### Broń 30 poziomu przestała blokować szukanie lepszej

Bot noszący taką broń na +7 odrzucał każdą ofertę z rynku niezależnie od tego,
co miał na niej wyrolowane. Teraz decyduje porównanie potencjału: dobra broń
dalej odrzuca oferty, słaba pozwala szukać lepszej.

## 2.0.70 — 2026-09-17

Serwer 2.0.70, klient 2.0.13.

### Boty nadrabiają misje Biologa (Tieru)

Bot z wysokiego poziomu, któremu zostały pierwsze wiersze Biologa (zioła z
pierwszej wioski), czekał na nie godzinami: wyjazd po zioła ma limit, żeby nie
powtórzyła się sytuacja z 2.0.60, gdy pół świata ruszyło naraz do M1. Teraz:

- bot, który **i tak jest w pierwszej wiosce** (zapasy, targ, oddawanie okazów),
  zbiera zioła z otwartego wiersza przy okazji, przez 10 minut od przyjścia i bez
  zajmowania miejsca w limicie — to nie dokłada ani jednego przejścia między
  mapami;
- na osobny wyjazd po zioła jedzie naraz 7% botów zamiast 2,5%, a wyprawa trwa
  do 2 godzin zamiast godziny;
- wyprawa nie wygasa, póki bot niesie zebrane okazy — wcześniej zegar potrafił
  skończyć wyprawę tuż przed oddaniem i cała podróż szła na marne.

Reguła „żaden wiersz nie jest za niski” i pierwszeństwo próby konia zostają bez
zmian.

### Koniec krążenia materiałów do magazynu i z powrotem (Tieru)

Bot wkładał do magazynu **cały stos** materiału razem z rezerwą, którą trzyma na
własne ulepszanie, a sekundę później — już z pustą torbą — wyjmował go z
powrotem, bo „brakuje mu na kowala”. I tak co kilka minut. Na naszym serwerze
3574 z 4698 wyjęć z magazynu w godzinę to były rzeczy włożone w tej samej
wizycie, a przy okazji sypało to do logu błędy `ITEM_ID_DUP` (650 dziennie u 26
botów), bo baza nie nadążała zapisać, że przedmiot jest już w torbie. Teraz do
magazynu idzie tylko nadwyżka ponad rezerwę kowala, a wyjmowanie nie rusza tego,
co ta sama wizyta włożyła.

### Sklep bota widoczny w panelu (Tieru)

W oknie ekwipunku bota, obok magazynu, jest ikona sklepu: otwiera okno z nazwą
lady, mapą i pozycją oraz listą wystawionych rzeczy z cenami. Wcześniej widać
było tylko torbę i magazyn, a to, co bot sprzedaje, leży w jego sklepie
offline. Obok jest też przycisk odświeżania.

### Ekwipunek w panelu bez opóźnienia (Tieru)

Panel czyta bazę, a rdzeń trzymał zmianę przedmiotu w pamięci nawet siedem
minut — dlatego zaraz po założeniu tarczy slot potrafił świecić pustką. Teraz
założony i zdjęty element zapisuje się od razu.

### Klient 2.0.13: osobowość bota w osobnym wierszu (ĹŌŞƬĒĶ)

Osobowość bota ma własny wiersz nad głową, między nickiem a nazwą gildii, więc
ranga jest znowu zawsze widoczna i nic się nie bije o to samo miejsce.
Przełącznik w opcjach gry („Osobowość / Wyłączone”) działa od razu, bez
ponownego logowania. Cała zmiana po stronie klienta jest autorstwa ĹŌŞƬĒĶ.

## 2.0.69 — 2026-09-17

Serwer 2.0.69; klient bez zmian (2.0.12).

### Karta Wędkarska od 30 poziomu (Hiob, Tieru)

Wędkę na tym serwerze zakłada się od 30 poziomu i od 30 można łowić, ale sklep
Rybaka sprzedawał Kartę Wędkarską dopiero od 50 — gracz między 30 a 49 poziomem
miał wędkę i nie mógł łowić. Próg karty w sklepie Rybaka jest teraz 30, tak samo
jak w opcji „Karta Wedkarska”. Cena i limit (25 000 yang, 5× Materiały
Rzemieślnicze, raz na 22 godziny) bez zmian; do łowienia nadal trzeba ukończyć
misję wprowadzającą Rybaka.

## 2.0.68 — 2026-09-17

Serwer 2.0.68; klient bez zmian (2.0.12).

### Zioła na ladzie w dużych paczkach, ulepszacze w małych (Tieru, Hiob, uxietoszef)

Sklep offline bota wystawiał przy każdej wizycie jedną linię — taki stos, jaki
akurat leżał w komórce torby. Korzenie Gango i Grzyby Tue szły więc po 1–2
sztuki (na naszym serwerze 3343 linie ziół, z tego 1171 pojedynczych, jeden
sklep z 34 liniami ziół), a ulepszacz, który nie był „zapasem”, szedł całym
stosem razem z rezerwą na własne ulepszanie (1084 linie ponad 10 sztuk, np.
25 Kawałków Lodu za 19,7 mln w jednej linii). Teraz:

- tanie towary (według cennika Iwakury do 5000 yang przed przelicznikiem stawek,
  czyli zioła i rudy) idą paczkami po 50, nigdy poniżej 10 sztuk, najwyżej dwie
  linie jednego rodzaju;
- ulepszacz idzie paczkami po 5 (zapas ponad 50 sztuk — po 10), najwyżej trzy
  linie jednego rodzaju, i nigdy z rezerwy, którą bot trzyma na swoje ulepszanie;
- stojące już linie ziół poniżej 10 sztuk i linie ulepszaczy ponad 10 sztuk bot
  zdejmuje przy kolejnych wizytach przy sklepie, żeby wystawić je od nowa.

Po wdrożeniu u nas linii ziół było już 1955 (pojedynczych 545), a linii
ulepszaczy ponad 10 sztuk 657 — i dalej ubywa, bo bot zdejmuje jedną linię na
wizytę.

### Biolog nie ściąga całego świata do Doliny Orków (SIZOWSKI)

U SIZOWSKIEGO 997 z 1621 botów stało w Dolinie Orków, u nas 277 z 1098 — prawie
wszystkie na Zębach Orka, a 129 w drużynach „Szukam celu dla grupy”. Przyczyny
z plików tego świata: skrypt misji daje ząb tylko z „Orka” (601), który w Dolinie
stoi w dwóch punktach przy bossach; zęby spadają naprawdę z Czarnych Orków
(46 lvl), a silnik tnie taki drop do 1% przy 15+ poziomach różnicy — bot 70 lvl
miał ząb raz na ok. 17 000 zabójstw. Bot polował tylko na potwora z nazwy misji,
więc resztę orków odrzucał jako bezwartościowe. Teraz:

- cel misji to cała rodzina potworów, z których przedmiot faktycznie spada
  (Zęby: Ork i Czarne Orki; klucz: Elitarne Orki 631–637; Księgi Klątw: oba
  Dręczyciele; klucz: 701–707 i 731–737; Pamiątka i jej klucz: demony wieży);
- bot ponad poziomem potwora dostaje brakującą część szansy, którą zabrała
  różnica poziomów — dokładnie tyle, ile dostałby gracz na poziomie potwora,
  z uwzględnieniem stawek świata; tylko dopóki Biolog jeszcze czegoś potrzebuje;
- bot z misją, który nie widzi takiego potwora, skanuje mapę i idzie do
  najbliższego, a walka po drodze nie odsyła go już do obozu wybranego po
  poziomie (wcześniej 51 z 86 botów w Dolinie stało w drużynach „Szukam celu
  dla grupy”);
- na przerośnięte misje zbierackie (Zęby, Księgi, Pamiątka) naraz jedzie
  najwyżej 10% botów, każdy na 2 godziny; reszta gra normalnie na swoich mapach;
- status mówi, co bot robi: „Zbieram dla Biologa: Zab Orka” w drodze do potworów,
  a „Ide do Biologa” tylko wtedy, gdy niesie okazy do oddania.

Po wdrożeniu u nas: w Dolinie 46 botów zamiast 277, „Szukam celu dla grupy”
u 8 zamiast 129, a w kwadrans po restarcie 10 botów oddało komplet zębów
i przeszło do drugiej części misji.

### Mniej teleportów do sklepów offline (gregoszky)

W 95 minut na naszym serwerze było 7951 przejść między mapami: 3405 to przeskoki
botów do własnego sklepu, a większość reszty — powroty (250 razy tam i z
powrotem w niecałe 30 sekund). To jest ten ruch przy teleporterach. Bot, który
stoi na innej mapie niż jego sklep, obsługuje go teraz raz na 45 minut; na mapie
sklepu nadal co 10–15 minut. Po pierwszej fali wizyt takich przeskoków było
2–12 na minutę zamiast 36.

### Karta Wędkarska do kupienia u Rybaka (Greess, SIZOWSKI)

Paczka sprzedaje Kartę Wędkarską tylko w specjalnym sklepie Rybaka (25 000 yang
i 5× Materiały Rzemieślnicze, od 50 lvl, raz na 22 godziny), ale żaden skrypt
tego sklepu nie otwierał, więc gracz nie mógł łowić. Rybak ma teraz opcję
„Karta Wedkarska”. Do łowienia nadal trzeba ukończyć jego misję wprowadzającą.
Quest jest skompilowany i wgrany na nasz serwer, ale opcji nie klikaliśmy
jeszcze w kliencie — jeśli sklep się nie otworzy, dajcie znać.

## 2.0.67 — 2026-09-17

Serwer 2.0.67; klient bez zmian (2.0.12).

### Próba konia bojowego od pierwszego do setnego zabójstwa

Po 2.0.66 boty z próbą docierały na pustynię i dwa ją ukończyły, ale reszta
dalej wracała: reguła „blokuje podróż” liczy torbę bez wolnej kolumny trzech
komórek (handlarz z 16 luźnymi wolnymi komórkami ma to na stałe — GumbASSx
wracał pięć razy w 40 minut, pobyty 97–426 s), a zegar wizyty na froncie
kończył próbę w dwóch trzecich („frontier_visit_complete” po 41 minutach).
Na próbie bot wraca tylko, gdy nie może walczyć (brak broni, zbroi, mikstur,
strzał), wizyta nie wygasa, a łucznik bierze u handlarza pełny kołczan (jak
dropek), żeby strzał starczyło na sto zabójstw.

### Pierścień Teleportacji: druga paczka i samonaprawa stanu

Paczka NerrVoVy z 12:43 (2.0.65) pokazała 20 linii `QUEST_ITEM: use` — użycie
dociera do silnika (flagi 0, żaden quest nie zawieszony, poziom 75, mapa 1), a
handler questu nie rusza i nie ma błędu Lua. Quest jest zarejestrowany i ma
jeden stan; jedyne, czego dopasowanie nie umie uruchomić, to gracz ze stanem
`teleport_ring` innym niż start. 2.0.67 loguje przy każdym użyciu przedmiotu
questowego, ile questów dopasowano i pominięto (`QUEST_ITEM: event`), a
użycie pierścienia ze stanem innym niż start najpierw zeruje ten stan (quest
nie ma innych stanów, więc nic nie ginie).

## 2.0.66 — 2026-09-17

Serwer 2.0.66; klient bez zmian (2.0.12).

### Bot z próbą konia bojowego zostaje na pustyni

Po 2.0.65 (zioła ustępują próbie) boty z otwartą próbą i tak niczego nie
kończyły: na m2zip w godzinę 85 wjazdów na pustynię, 75 pobytów po średnio
344 s (67 krótszych niż 10 minut), licznik zabójstw próby rósł o 25 co pół
godziny, a 102 ze 120 botów z próbą stało w wioskach z „0/100”. Do wioski
wysyłał je hand-in Biologa, który nieśli (29 wyjazdów na godzinę), i obsługa
własnego sklepu co 10–15 minut (23 na godzinę). Oba czekają teraz, dopóki
próba na pustyni trwa; potrzeba blokująca (brak broni, mikstur) nadal
wygrywa. I druga połowa: bot z otwartą próbą w wiosce w ogóle rzadko na
pustynię ruszał (126 takich botów, 3 wjazdy w 25 minut) — trzymała go
„miękka” potrzeba sprawunków, torba zapełniona w 45 %, którą handlarz z
towarem nosi zawsze: wizyta w mieście → rynek → drużyna → wizyta w mieście,
przez cały dzień. Dla bota z próbą ta połowa nie liczy się; brak broni,
zbroi, mikstur, strzał albo wolnej kolumny nadal wysyła do miasta. A „brak
wolnej kolumny” był u handlarzy stanem trwałym: bot 40 lvl z 200 mln yang
nosił 38 stosów materiałów „na ladę” (lada mieści kilka linii) przy 4
przedmiotach w magazynie, bo materiał, którego ktoś na świecie szuka, nigdy
nie szedł do Dozorcy. Przy pełnej torbie (80 %) idzie; wypłata z magazynu
wraca po niego na ladę tylko, gdy torba ma miejsce.

### Launcher ponawia pobieranie paczki (Hiob)

Aktualizacja 2.0.61 → 2.0.65 padła dwa razy sekundę po odczycie manifestu:
„połączenie nieoczekiwanie zakończone”, potem „(500) Wewnętrzny błąd
serwera” — odpowiedź GitHuba, ten sam zip minutę później schodził
normalnie. Launcher pobierał raz i przy pierwszym potknięciu kończył
błędem; teraz próbuje trzy razy z pięciosekundową przerwą (blokada
antywirusa nadal od razu). Nowy launcher przyjeżdża w paczce, więc działa
od następnej aktualizacji.

### Karta czasu odradzania z mnożnikami (kiciamol)

Przy każdym z dwóch pól na stronie „Stawki” są przyciski ×1 … ×10 (wpisują
odpowiedni procent) i podpis „≈ ×N szybciej niż w grze” liczony na żywo. Pod
spodem zostaje procent, więc polecenie `REGEN` i flagi silnika bez zmian.

## 2.0.65 — 2026-09-17

Serwer 2.0.65; klient bez zmian (2.0.12). Dwie rzeczy z przedpołudnia po 2.0.64.

### Pierścień Teleportacji: użycie zostawia ślad w logu (NerrVoVy)

Po 2.0.64 pierścień (70058) u gracza dalej „nic nie robi”, a paczka
diagnostyczna nie ma o tym ani jednej linii. Diagnoza z 2.0.62 była chybiona:
flaga 8192 na pierścieniu to w tym silniku `ITEM_FLAG_LOG`, nie `APPLICABLE`
(ta jest 1<<14), więc gałąź „przeciągnij na inny przedmiot” nigdy nie była
przyczyną, a migrator czyścił nieszkodliwą flagę. Od 2.0.65 każde zwykłe użycie
przedmiotu questowego zapisuje w syslogu linię `QUEST_ITEM: use` z flagami,
mapą, poziomem i informacją, czy gracz ma **zawieszony quest** (otwarte okno
zadania — wtedy silnik po cichu odrzuca każde nowe zdarzenie questowe; gracz
dostaje teraz podpowiedź na czacie), a sam quest pierścienia loguje, że się
uruchomił. Paczka diagnostyczna zbiera tag `QUEST_ITEM`. To pozwoli nazwać
przyczynę z następnej paczki zamiast zgadywać.

### Kopanie rud pieszo (Remigiusz)

Bot kopał rudę siedząc na koniu (zrzut: TheBlady2 z kilofem na białym koniu
przy Stercie Muszli) — silnik nie pyta o konia ani przy machnięciu, ani przy
założeniu kilofa. Górnik schodzi teraz z konia przy żyle i odsyła go, jak
wędkarz przy wodzie; wsiada z powrotem na drogę.

### Zioła Biologa ustępują otwartej próbie konia bojowego

Bot z otwartą próbą konia (70+, koń na 10) szedł do pierwszej wioski po zioła
Biologa (wiersze 1–6) i przez cały ten czas nad głową miał „Zdobywam konia
bojowego na pustyni (0/100)” — na m2zip 88 ze 124 takich botów miało za cel
Biologa, a na pustyni stało ich 8. Losowanie frontu i wiersz z potworem
ustępowały próbie od 2.0.61, wyprawa po zioła nie. Teraz zioła czekają na
konia; hand-in już niesionych okazów nadal idzie. Przy okazji: miejsce na
wyprawę po zioła jest przydzielane dopiero dla wybranego wiersza, nie przy
każdym sprawdzeniu — bot niosący Zęby Orka brał miejsce i oddawał je w tej
samej klatce (300 linii logu na minutę po restarcie).

## 2.0.64 — 2026-09-17

Serwer 2.0.64; klient bez zmian (2.0.12). Poranne zgłoszenia z Discorda po
nocnych wydaniach 2.0.62/2.0.63 i panel Sebana 1.55.0.

### Szkatułki Blasku Księżyca dropiły dwa razy częściej, niż mówił suwak

W dostarczanym `item_manager.cpp` blok losowania szkatułki (i kuponu Smoczych
Monet) siedział w `CreateDropItem` dwa razy — od co najmniej 2.0.50 każde
zabójstwo i każdy Metin losowały szkatułkę dwukrotnie, więc 10‰ z panelu
znaczyło w praktyce ~20‰. Przyczyna: skrypt portu rozpoznawał „już nałożone”
po pełnym tekście bloku, a każda zmiana komentarza w tym bloku wstawiała go
ponownie. Blok jest teraz jeden, a skrypt rozpoznaje go po stałym markerze.

### Szkatułki tylko w oknie eventu — dwie dziury zamknięte (NerrVoVy)

„Mam włączone blaski 10/100, harmonogram na 2 h dziennie, a dropią też poza
konkursem”: po pierwsze, każdy zapis pliku wag z panelu wpisywał suwak wprost
do silnika i bramka eventu zerowała go dopiero w następnej sekundzie; teraz
wartość suwaka jest trzymana obok, a silnik dostaje ją tylko, gdy okno jest
otwarte. Po drugie, własne tabele dropu paczki mają linie ze szkatułką (50011),
których żadna bramka nie obejmowała: gdy oba suwaki są na zerze (okno
zamknięte albo wyłącznik), szkatułki z tabel też nie wypadają. Trzeci powód to
podwójne losowanie z sekcji wyżej.

### Wędkarze: karta wchodzi na zajęte sloty, Rybak ma własny próg dojścia (seban latino)

Dwie paczki diagnostyczne z 1013 i 1044 botami: na pierwszej nie łowił nikt —
każda linia wędkowania to „karta w torbie, ale nie założona”. Karta Wędkarska
jest unikatem, a bot z dwoma zajętymi slotami unikatów (np. Symbol Króla
Przepowiedni + pierścień) nie mógł jej założyć i próbował znów za godzinę.
Teraz zdejmuje jeden unikat (najpierw taki, który nic mu nie daje, pierścień na
zegarze na końcu), a przy każdym zarzuceniu sprawdza, czy karta jest na sobie —
730 odmów silnika „You need to have a fishing pass” w dwie minuty i 24 sesje
zakończone bez rzutu to była karta, która zeszła w trakcie sesji. Osobno: bot
zawieszony na „Idę do Rybaka po przynętę” (Przepotenszny, MrocznyNinjaxD… na
Yongan i Pyongmoo) stał 119–177 jednostek od punktu podejścia, bo punkt leżał na
zablokowanym gruncie, a próg dojścia był stanowiskiem wędkarskim (100) — Rybak
to lada, dostała własny próg (400) i dopasowany snap celu. Do tego bot, który
oznaczył swoje stanowisko jako suche (brak wody obok), dostawał je z powrotem,
gdy wędkarzy było więcej niż stanowisk — stał na piasku do końca limitu
bezczynności; teraz dzieli z kimś mokre stanowisko, a gdy cały brzeg jest
suchy, kończy sesję od razu.

### Wojna gildii botów pieszo i od razu na środku (NerrVoVy, Tieru)

Boty odwołują konie na polu bitwy (dotąd bojowy koń zostawał w siodle) i obie
strony zbierają się na tym samym otwartym gruncie na środku mapy gildyjnej,
zamiast w dwóch kolumnach 700 jednostek od siebie.

### Zbroja na plecach nie idzie na ryzykowny ulepszacz bez zastępstwa (THC)

Reguła „broni w ręce” z 2.0.49 dostała bliźniaczkę dla zbroi: na kroku, który
może spalić (od +4 na +5 wzwyż), noszona zbroja idzie pod zwój albo czeka, jeśli
w torbie nie ma innej, którą bot mógłby założyć. Dotąd bot na 20. poziomie
przepalał jedyny pancerz i szedł farmić bez zbroi.

### Czas odradzania Metinów, bossów i potworów — suwak w panelu (Hiob)

Silnik od dawna czyta flagi `fastBossSpawn<mapa>` / `fastMobSpawn<mapa>`
(procent zwykłego czasu odradzania linii regenu); 2.0.64 dodaje ich wersje
bez numeru mapy jako domyślne dla całego świata i formularz na stronie
„Stawki” panelu klasycznego: osobno Metiny i bossowie, osobno zwykłe potwory,
10–100 % zwykłego czasu. Działa od razu przez pomocnika w grze (polecenie
`REGEN` questu web_admin) i zostaje po restarcie (flagi w `player.quest`).

### update.sh dopisuje brakujące klucze .env (GorącyDelfin)

Aktualizacja z paczki na Linuksie nigdy nie dopisywała nowych kluczy do `.env`
(robił to tylko launcher na Windows), więc po 2.0.57 brakowało `M2_DIFFICULTY`
i godzin oczekiwania. `update.sh` dopisuje teraz z `.env.example` tylko te
klucze, których wartość przykładowa jest domyślną z compose (trudność, plan
wejścia botów, dropki medali, szkatułki, układ świata) — nigdy hasła, portu ani
adresu; klucz już obecny zostaje jak jest.

### Panel Sebana 1.55.0

Plan wejścia botów w /manage (okno kohorty, późno dołączający), rozbudowane
gildie (królestwo z flagą po liderze), planer eventów, ItemShop w monitoringu
gospodarki (Smocze Monety i Znaki botów, zakupy z `log.itemshop`), diagnostyka
wędkarstwa, sprzedawca ze sprzedaży prowadzi do karty postaci, wyszukiwanie
przedmiotów przechodzi wprost do wyników. Nasze zabezpieczenia zachowane; plan
wejścia botów w panelu działa tylko z integracją Sebana (bez niej panel odsyła
do launchera / `.env`).

## 2.0.63 — 2026-09-17

Serwer 2.0.63; klient bez zmian (2.0.12). Ostatnie poprawki nocy po
pierwszych testach Wieży Demonów z 2.0.62.

### Wieża Demonów: na 7. piętrze grupa toruje sobie drogę do Metina Morderstwa

Na 7. piętrze grupa toruje sobie drogę do Metina Morderstwa: potwory są
wybierane od kamienia, więc czyszczony jest teren wokół niego i kamień jest
rozbijany, gdy nic przy nim nie stoi. Dotąd grupa szła za najbliższym
demonem i kamień stał nietknięty, choć boty żyły i zabijały (na serwerze
testowym: 9 minut, 63 zgony, cztery Metiny Śmierci rozbite w 49 s, Metin
Morderstwa nietknięty). Ta poprawka została skompilowana i przeczytana, ale
nie obejrzana na żywo — serwer testowy był już wyłączany; skrzynia i mapa z
7. piętra, klucz z 8. i Rozpruwacz z 9. nie były jeszcze osiągnięte przez
żadnego bota. Prosimy o zgłoszenia z pięter 7–9 na Discordzie.

## 2.0.62 — 2026-09-16

Serwer 2.0.62; klient bez zmian. Wydanie do testów Wieży Demonów — prosimy o
zgłoszenia na Discordzie.

### Gildie botów chodzą do Wieży Demonów (przełącznik „Gildie botów chodzą do Wieży Demonów”, domyślnie włączony)

- **Wyprawa**: mniej więcej co półtorej godziny (pierwsza kilkanaście minut
  po starcie serwera) jedna gildia botów rdzenia — z co najmniej czterema
  członkami od 40. poziomu online, najchętniej taka, w której ktoś ma 75. —
  zwołuje na czacie gildii zbiórkę na parterze Wieży (do 16 najsilniejszych
  członków), a serwer ogłasza to wszystkim: „Gildia X (Chunjo) rusza na Wieżę
  Demonów: zbiórka na parterze wiezy, start za 4 minuty. Kto stoi na parterze,
  wchodzi razem z nimi.” Po czterech minutach (albo gdy wszyscy dotarli) boty
  razem rozbijają Metin Twardości. Log: `PLAYERBOT_TOWER: raid called /
  breaking the stone / raid inside / raid over`.
- **Kto stoi na parterze, wchodzi** — bot na własnej misji (Pamiątka po
  Demonie), gracz, który przyszedł popatrzeć: gra wciąga do nowej instancji
  wszystkich z mapy zabójcy kamienia, a w środku każdy bot gra według zasad
  piętra, wyprawa czy nie.
- **Piętra** jak w grze: 2. — wybić wszystko; 3. — Król Demonów i reszta;
  4. — Metin Diabła, potem siedem Metinów Upadku (fałszywe znikają przy 50 %,
  prawdziwy trzeba rozbić); 5. — Kamienie Otwarcia z demonów niesione do
  pięciu Starożytnych Pieczęci; 6. — Elit. Król Demonów, potem kowal Wieży;
  7. — cztery Metiny Śmierci, potem Metin Morderstwa, Nieznana Stara
  Skrzynia i Mapa Wieży Zin-Sa-Gui; 8. — Klucz Zin-Bong-In z Nieśm. Duchów
  do Sa-Soe; 9. — Umarły Rozpruwacz. Klucze zbiera zwykły pas lootu, bot ich
  używa albo oddaje, komu trzeba. Od 6. piętra potrzebny jest bot z 75.
  poziomem (zasada gry: kowal puszcza dalej tylko takiego); bez niego wyprawa
  kończy się na szóstym piętrze i boty wychodzą. Run bez postępu przez 20
  minut, piętro powyżej 35 minut albo całość powyżej 2 godzin — boty wychodzą.
  Polegli wstają na miejscu jak dotąd.
- **Gildia gracza**: gdy mistrz gildii (gracz) stoi na parterze Wieży,
  jego boty od 40. poziomu przychodzą i stoją przy nim; gdy gracz rozbije
  kamień, wchodzą z nim i walczą piętro po piętrze. Bot w drużynie gracza
  wchodzi jak dotąd z drużyną.
- **Panel klasyczny**: przełącznik na stronie zachowania botów i przycisk
  „Wyprawa do Wieży Demonów teraz” (rdzeń zwołuje wyprawę przy najbliższym
  sprawdzeniu, jeśli żadna nie trwa); na stronie „Gildie” przy gildii w
  Wieży stoi „w Wieży Demonów”. Status nad głową: „Wieza Demonow: pietro N”,
  „Zbiorka gildii: Wieza Demonow”. Gildia w Wieży nie jest wybierana do wojny.

- **Zmierzone na serwerze testowym** (16 września, jedna wyprawa 16 botów
  gildii MINISTRANCI, bez wycieków z instancji): 2. piętro 208 s, 3. z Królem
  Demonów 253 s, 4. Metiny 107 s, 5. pięć Kamieni Otwarcia do Pieczęci 177 s,
  6. Elitarny Król Demonów 104 s i kowal Wieży przepuścił bota 75. poziomu.
  Na 7. piętrze pierwsza wyprawa utknęła: 214 demonów, boty rozproszone po
  piętrze, połowa w pętli śmierć/wskrzeszenie, 6 minut bez zabójstwa. Stąd
  walka w grupie w tym wydaniu: cel wybierany od środka gildii (wszyscy biją
  tego samego demona), kamienie piętra dopiero gdy zostanie najwyżej 25
  potworów, maruder bez celu wraca do grupy. Wieża idzie jako nowość do
  testów; piętra 7–9 prosimy zgłaszać na Discordzie.

- **Po pierwszym teście na serwerze testowym boty walczą na piętrach w
  grupie**: cel jest wybierany od środka grupy, więc szesnastka bije tego
  samego demona, a maruder bez celu wraca do reszty — na 7. piętrze zgony
  spadły z 253 na 43 na dziesięć minut, a zabójstwa z zera do piętnastu na
  minutę. Kamień piętra (Metin Morderstwa, Metiny Śmierci) jest rozbijany,
  gdy grupa oczyści teren wokół niego, bo regen 7. piętra dorzuca potwory
  szybciej, niż da się je wybić do końca. Drugi przebieg z walką w grupie:
  piętra 2–6 w 222/256/153/206/156 s, kowal przepuścił bota 76. poziomu;
  7. piętro było w toku w chwili wydania.

### Koniec pętli Joan↔Bokjung u botów z ziołowym wierszem Biologa

Bot z otwartym ziołowym wierszem (potwór w pierwszej wiosce) wchodził z
Bokjung do Joan po zioła, a w Joan reguła „wyrosłeś z tej mapy” od razu
wysyłała go z powrotem — punkty wejścia obu bram stoją obok bramy
powrotnej, więc okrążenie trwało 4 sekundy (Greess, log TAKAMURU1: „nie
przechodzą przez teleporty”). Polowanie na zioła jest teraz powodem, by
zostać w pierwszej wiosce, tak samo jak było powodem, by do niej przyjść.
Przy okazji policzone: Teleporter działa w każdej pierwszej i drugiej
wiosce trzech królestw (na serwerze testowym w 45 min: z 1/21/41 do Doliny
361/377/252 razy, na pustynię 44/58/58; z 3/23/43 do Doliny 103/54/67), a
bramy M1↔M2, do Lochów Małp i z M3 po wojnie tak samo.

### Koniec pętli M2↔M3 u dropperów M3

Dropper M3 (osobowość, która farmi mapę gildyjną pod bronie 30 lvl na
sprzedaż) z taką bronią w torbie był wysyłany na M3 jako dropper, a gałąź
M3 odsyłała go do M2 jako bota, który „znalazł broń” — punkt przybycia
Teleportera na mapie gildyjnej stoi obok bramy powrotnej, więc okrążenie
trwało 5 sekund (paczka seban latino: po dwa boty na Shinsoo i Jinno,
62–65 okrążeń w 6 minut, „level30_weapon_to_m3” / „m3_weapon_found”). Dla
droppera M3 broń w torbie to towar, nie powód do powrotu; obie reguły —
wejścia i wyjścia — pytają teraz o to samo.

### Próba konia bojowego tylko tam, gdzie jest jej mapa

W układzie split pustynia (i Wieża Demonów dla konia wojskowego) stoi na
jednym rdzeniu, a próba pytała tylko o poziom, konia i zabójstwa: 58 botów
Shinsoo i 42 Jinno stało w drugich wioskach z napisem „Zdobywam konia
bojowego na pustyni (0/100)” (paczka seban latino), losowanie frontieru
odpowiadało pustynią i było filtrowane do niczego, a od 2.0.61 Biolog
ustępował próbie — więc te boty nie miały ani frontieru, ani wiersza
Biologa. Próba jest otwarta tylko na rdzeniu, który hostuje jej mapę; na
pozostałych bot jest losowany po poziomie i robi Biologa jak każdy inny.

### Naprawa paczki: 5. piętro Wieży Demonów było nie do przejścia

Quest paczki liczył na 5. piętrze zabójstwa Brutalnego Demona Łucznika
(1062), a regen tego piętra w tej paczce stawia Demonów Żołnierzy, Łuczników,
Włóczników i Szamanów oraz ich Podłe odmiany (1002–1004, 1031–1034) — nigdy
1062. Żaden Kamień Otwarcia nie mógł więc wypaść i nikt, gracz ani bot, nie
przechodził dalej. Kopia questu w obrazie serwera liczy potwory, które na
tym piętrze naprawdę stoją.

### Bot przechodzi przez każdy warp gry

Silnikowy `WarpSet` — skok lochu, wyjście z lochu (`d.exit_all`), `pc.warp`
questu, `/warp` GM-a — zdejmował bota z mapy i czekał na klienta, którego
bot nie ma (ratunek stawiał go na starcie jego mapy). Teraz bot na tym
rdzeniu przechodzi po stronie serwera tam, gdzie gra go posłała, z
członkostwem w lochu jak gracz po ponownym połączeniu; warp na mapę innego
rdzenia jest odmawiany z wpisem w logu (`PLAYERBOT_WORLD: warpset refused`).

## 2.0.61 — 2026-09-16

Serwer 2.0.61 i klient 2.0.12. Wydanie z tabelą tierów Iwakury, cennikiem 1.2 i
poprawkami z wieczoru 16 września.

### Tiery ekwipunku i bonusów Iwakury (PvE)

Iwakura ocenił każdą rodzinę bransolet, kolczyków, naszyjników, butów i broni
oraz każdy bonus w skali 1 (bardzo zły) – 6 (wspaniały), osobno do PvE i do
PvP, z dopiskami „+1 dla Wojownika” itp. (`data/iwakura_tiery.txt`; zbroje,
hełmy i tarcze ocenia po poziomie i bonusach, więc nie są w tabeli). Boty
używają kolumny PvE:

- **Ocena sprzętu** przesuwa się o 8 % na stopień od neutralnej trójki
  (tier 6 to +24 %, tier 1 to −16 %), a każda linia bonusu na przedmiocie liczy
  się z wagą tieru (wspaniały ×1,3 … bardzo zły ×0,25). To dopisek do
  dotychczasowej, mierzonej oceny, nie nowy ranking: bot nadal nie zamieni
  Miedzianych Kolczyków +9 z bonusami na Ebonitowe +1 bez bonusów.
- **Losowanie bonusów** (kamienie i marmury) używa tych samych wag, więc to,
  co kowal losuje, i to, co bot zakłada, ocenia jedna tabela.
- **Zapas lepszego tieru** w torbie (np. Ebonitowe Kolczyki u bota w
  Miedzianych) jest traktowany jak zapas wyższego poziomu: nie idzie do
  handlarza, kowal go ulepsza i bonuje, a założony zostaje dopiero wtedy, gdy
  naprawdę wygrywa — zgodnie z instrukcją Iwakury „najpierw ulepszyć i
  wybonować”.
- Kolumna PvP jest w tabeli, ale jeszcze nieużywana — czeka na drugi zestaw
  ekwipunku botów.
- Narzędzie: `tools/generate_iwakura_tiers.py` renderuje
  `playerbot_item_tiers.h` (161 rodzin, 45 bonusów) i przerywa, gdy jakiejś
  nazwy nie da się związać z przedmiotem — jak generator cennika.

### Cennik Iwakury 1.2

Nowe pozycje (m.in. szkatułki i klucze, zwoje ulepszeń, księgi pasywne,
przedmioty konia, przedmioty z łowienia, receptury) i dostrojone ceny;
poprawione pisownie. Wszystkie lady przeceniają towar według nowej tabeli
w ciągu kilku wizyt serwisowych.

### Boty przyjmują zaproszenia do gildii od graczy

Zaproszenie do gildii wysłane botowi trafiało do deskryptora bez klienta i po
dziesięciu sekundach wygasało — zaproszenie bota nie robiło nic. Teraz bot bez
gildii przyjmuje zaproszenie od gracza od razu (każdy bot, także dropki),
zapraszający widzi na czacie „X przyjmuje zaproszenie do gildii Y”, a bot w
gildii gracza co godzinę oddaje jej część zdobytego doświadczenia (10 %, jak
w zwykłej gildii botów) i poza tym niczego w niej nie zmienia. Warunki silnika
bez zmian: to samo królestwo, wolne miejsce, gildia nie w wojnie.

### Wojny gildii botów rozłożone między królestwa, z zapowiedzią

Wszystkie trzy królestwa zaczynały wojnę pół godziny po starcie serwera i
kończyły razem, a potem przez dwie godziny nie było czego oglądać. Teraz
pierwsza wojna Shinsoo jest po 30 minutach, Chunjo po 70, Jinno po 110, a
kolejne 90 minut po zakończeniu poprzedniej (co 2 h w królestwie) — w
świecie prawie zawsze gdzieś trwa wojna. Przy wypowiedzeniu, minutę–dwie
przed pierwszymi ciosami, idzie ogłoszenie „Za chwile wojna gildii botow
(Chunjo): A kontra B. Pole bitwy: mapa gildyjna.”, a strona **Gildie** w
panelu klasycznym pokazuje „Następna wojna gildii botów: Shinsoo za ok. N
min, …” (trwa teraz / niezaplanowana).

### Pierścień Teleportacji działa u gracza

Przedmiot 70058 ma w tej paczce flagę APPLICABLE, przez którą gra traktuje
go jak „przeciągnij na inny przedmiot” i przy zwykłym użyciu w ogóle nie
pyta questu — dlatego quest z 2.0.52 nigdy nie odpalał. Migrator zdejmuje
flagę przy starcie; pierścień otwiera okno z listą miejsc jak Teleporter
(za tę samą opłatę, od 11 poziomu).

### Biolog: ziołowe wyprawy po trochu, koń bojowy przed nimi

2.0.60 kazało botom robić wiersze Biologa po kolei bez względu na poziom
— i każdy bot z nieskończonym ziołowym wierszem ruszył do pierwszej
wioski naraz: na serwerze testowym przejścia M2→M1 skoczyły z ok. 300 na
godzinę do 1 826 i 2 766, w pierwszych wioskach stało 580 z 1 099 botów,
a gracze nagrali tłum botów na koniach wjeżdżających w bramy i „boty
40–50+ expiące w M1” (uxietoszef, Bierzyn, 16 września). Wiersze nadal
idą po kolei, ale bot, który przerósł ziołowy wiersz o więcej niż 10
poziomów, jedzie po niego do wioski tylko wtedy, gdy ma wolne miejsce w
puli 2,5 % żywych botów (miejsce trzymane najwyżej godzinę, oddawane po
skończeniu ziół); bez miejsca robi następny wiersz w kolejności (Ząb Orka
w Dolinie) — także bot, który już stoi w pierwszej wiosce, więc wioski
opróżniają się z botów 40+ w kilkanaście minut po aktualizacji. Log:
`PLAYERBOT_BIOLOGIST: herb errand`.

#### Koń bojowy przed ziołami

Po 2.0.60 bot 70+ z koniem na 10. poziomie miał iść na pustynię, ale ziołowy
wiersz Biologa (Korzeń Gango w pierwszej wiosce) wysyłał go najpierw do M1 —
w Jayang stało 35 takich botów z napisem „Zdobywam konia bojowego na
pustyni (0/100)”, w innych wioskach podobnie. Dopóki bot ma otwartą próbę
konia (bojowego albo wojskowego), potwór wiersza Biologa nie jest celem
podróży; oddanie okazów nadal idzie normalnie. Na serwerze testowym pół
godziny po restarcie: wyjazdy Teleporterem na pustynię i pierwsze zabójstwa
próby u botów, które dotąd stały w wioskach.

### Niesprzedany towar na ladach tanieje z czasem

Linia na sklepie offline, której nikt nie kupił, tanieje o 10 % za każde
2 godziny stania, najwyżej o 50 %, i nigdy poniżej tego, co zapłacono
kowalowi za ulepszenie. Klasyczny stragan miał taką przecenę od dawna; sklep
offline dostaje ją na wizycie serwisowej (log `PLAYERBOT_OFFLINE: marked
down`).

### Klient 2.0.12

- Przełącznik „Tytuły botów” w opcjach gry wypisuje na czacie, że zmiana
  będzie widoczna po ponownym zalogowaniu (klient rysuje tytuł do następnej
  zmiany rangi bota).

## 2.0.60 — 2026-09-16

Serwer 2.0.60; klient bez zmian (2.0.11). Wydanie do testów systemu gildii i zakupów botów w ItemShopie —
prosimy o zgłoszenia na Discordzie.

### Gildie botów: klasy, rekrutacja według siły, exp dla gildii, umiejętności gildyjne

Dotąd gildia bota była nazwą i listą: bot zakładał ją losowo, zapraszał
kogo spotkał na placu, a gildia nigdy nie dostała punktu doświadczenia
(na serwerze testowym 25 gildii, wszystkie na poziomie 1). Teraz:

- **Siła bota** to jedna liczba: poziom, cios broni w ręce, poziomy
  umiejętności buildu, koń i zbroja. Co 10 minut rdzeń spisuje siłę botów
  każdego królestwa i dzieli je na percentyle (log
  `PLAYERBOT_GUILD: strength census`); pierwszy spis 10 minut po starcie.
- **Klasa gildii** wynika z percentyla założyciela: **Elitarna** (górne
  3 % królestwa), **Silna** (górne 15 %), **Średnia** (górna połowa),
  **Zwykła** (reszta). Elitarnych jest najwyżej 2 na królestwo, silnych 6;
  gdy klasa jest pełna, założyciel schodzi o jedną niżej. Elitarna ma 24
  miejsca, silna 40, pozostałe tyle, ile daje gra (48 + 4 na poziom).
- **Rekrutacja**: mistrz zaprasza najsilniejszych botów bez gildii z całego
  swojego królestwa (nie tylko z zasięgu wzroku), wyłącznie powyżej progu
  swojej klasy, po trzech na przebieg. W gildii nadal tylko jedno
  królestwo; dropki nadal poza gildiami.
- **Awans**: członek, którego siła przerosła klasę jego gildii, odchodzi do
  lepszej gildii z wolnym miejscem (raz na 6 godzin, najwyżej 3 na
  królestwo na spis). Mistrz nie odchodzi.
- **Exp dla gildii**: co godzinę członek oddaje gildii część doświadczenia
  zdobytego od poprzedniej ofiary — 15 % w gildii elitarnej, 12 % w silnej,
  10 % w pozostałych; co najmniej 10 000, nigdy więcej niż ma na bieżącym
  poziomie, więc bot nie traci poziomu. Gildia dostaje setną część oddanego
  expa, dokładnie jak u gracza (tabela poziomów gry: 15 000 na 2. poziom,
  685 000 na 10.). Log: `PLAYERBOT_GUILD: offered exp`,
  `PLAYERBOT_GUILD: guild X reached level N`.
- **Umiejętności gildyjne**: punkt z każdego poziomu mistrz wydaje
  schodkowo: Krew Smoczego Boga, Święta Zbroja, Gniew, Przyspieszenie,
  Błogosławieństwo, Pomoc w Rzucaniu. Uwaga: w tej wersji gry umiejętność
  gildyjną można użyć tylko na arenie wojennej, więc w wojnach polowych
  botów nie działa — punkty służą graczom w gildii bota i arenom.
- **Istniejące gildie botów** (założone przed 2.0.60) dostają klasę przy
  pierwszym sprawdzeniu po spisie, według siły mistrza. Klasa i królestwo
  gildii trzymane są w nowej tabeli `player.playerbot_guild` (tworzy ją
  migrator przy starcie).

### Wojny gildii botów (przełącznik „Wojny gildii botów”, domyślnie włączony)

- Mniej więcej **co 2 godziny** w każdym królestwie dwie gildie botów
  (co najmniej 8 botów online w każdej, najbliższe sobie klasy, rotacja)
  toczą **wojnę polową silnika**: wypowiedzenie przez jednego mistrza,
  przyjęcie przez drugiego, 30 minut, zabójstwa liczone przez grę,
  rozliczenie i ranking (ladder ±7 %) przez rdzeń bazy. Pierwsza wojna
  pół godziny po starcie serwera.
- **Pole bitwy** to mapa gildyjna królestwa (Waryong i jej odpowiedniki):
  komunikat na czacie „Wojna gildii: A kontra B! Pole bitwy: mapa gildyjna
  (Chunjo), 30 minut.”, zbiórka na najbliższym otwartym terenie przy punkcie
  wejścia mapy (na mapach Chunjo i Jinno sam punkt wejścia leży w strefie
  bez PvP, gdzie gra nie liczy żadnego ciosu), strony 700 jednostek od
  siebie. Boty schodzą z konia transportowego i biją
  najbliższego wroga tak jak w pojedynku (bufy, dystans casterów, szarża
  wojownika, skille, cios), polegli wracają z wioski na pole; po wojnie
  wszyscy wracają do drugiej wioski. Status nad głową: „Wojna gildii z X”.
  Bot w drużynie gracza na wojnę nie idzie.
- Wojna wypowiedziana gildii bota przez gracza nie jest przyjmowana (to
  osobna decyzja na później). Wyłączenie przełącznika wstrzymuje nowe
  wypowiedzenia; trwająca wojna dobiega końca. Log: `PLAYERBOT_GUILD: war
  declared / accepted / on / over`.

### Boty korzystają z ItemShopu (przełącznik „Boty kupują w ItemShopie”, domyślnie włączony)

Kupony SM (z metinów i bossów, stawki `M2_DRAGON_COIN_STONE_PERMILLE` i
`M2_DRAGON_COIN_BOSS_PERMILLE` w `.env`) leżały dotąd w torbach botów bez
użytku — na serwerze testowym 97 sztuk po trzech dniach. Teraz:

- **Kupon** bot wymienia od razu na Smocze Monety swojego konta, tą samą
  drogą co gracz (doładowanie przez rdzeń db, wpis w logu kuponów), tylko bez
  okienka. Saldo trzyma w pamięci i odczytuje z konta raz na godzinę.
- **Zakup** to zwykły zakup w sklepie gry (poziom, cena, log zakupów silnika,
  towar do torby), najwyżej raz na godzinę, i tylko to, z czego bot naprawdę
  korzysta: **Kamień Duchowy** (49 SM) dla bota z umiejętnością Wielkiego
  Mistrza gotową do treningu; **Zaczarowanie Przedmiotu** (69 SM) dla bota,
  którego noszona broń jest jeszcze warta losowania bonusów, a w torbie nie
  ma kamienia; za Smocze Znaki (naliczane 1:1 za wydane monety) **Zwój
  Błogosławieństwa** przy pracy pod zwój i **Atak Boga Smoków** x5; oraz
  **fryzura** (39 SM) — raz, dla jednego bota na czterech, gdy nic innego nie
  jest potrzebne.
- Bot nie kupuje przedmiotów VIP ani Przepustki Triumfu: każdy bot ma
  subskrypcję premium od zawsze (5 lat od spawnu), a sklep i tak odmawia
  VIP-a subskrybentowi. Nie kupuje resetów, Wykrywacza Metinów ani Magicznego
  Metalu.
- Naprawa poza botami: silnik logował każdy zakup w sklepie do tabeli
  `log.itemshop`, której paczka nigdy nie miała — migrator ją tworzy, więc
  zakupy graczy przestaną kończyć się błędem SQL w syserr.
- Log: `PLAYERBOT_ISHOP: voucher cashed / bought / census`, także w pakiecie
  wsparcia. Przy domyślnych stawkach bot znajduje kupon raz na miesiąc, więc
  zakupów jest ok. 30 dziennie na tysiąc botów; kto chce częściej, podnosi
  stawki w `.env`. Na serwerze testowym (30 ‰ z metinów): 97 kuponów
  wymienionych w minutę po starcie, 20 fryzur kupionych i założonych w 11
  minut przez boty z 50 SM, wpisy w logu sklepu, zero błędów.

### Biolog: żaden etap nie jest „za niski dla bota”

Bot robił misje Biologa tylko w swoim paśmie poziomów, a wiersze, z których
wyrósł, pomijał — stąd „Ząb Orka 4/10 • za niskie dla bota, pominięte: 4”
nad botem 78 poziomu, który ani zębów nie dokończył, ani do ziół nie
wrócił. Teraz wiersze idą po kolei niezależnie od poziomu: bot kończy ten,
którego okazy nosi, potem pierwszy nieukończony. Po okaz jedzie tam, gdzie
stoi potwór: po ziołowe wiersze do pierwszej wioski (i tam poluje w paśmie
wiersza, nie swoim), po Ząb Orka i Księgę Klątw do Doliny Orków, po
Pamiątkę po Demonie do Wieży Demonów. Okaz daje własny hak questa, więc
różnica poziomów nie przeszkadza. Panel nie pokazuje już „pominięte”.

### Koń bojowy: bot 70+ z koniem na 10. poziomie robi próbę na pustyni

Na serwerze testowym 161 ze 178 botów 70+ z koniem na 10. poziomie nie
miało ani jednego zabójstwa z próby konia bojowego: polityka walki
odrzucała skorpiony i węże pustyni jako bezwartościowe dla tak wysokiego
bota, więc bot na pustyni nic nie bił i wracał do miasta. Potwory próby
(dwóch łuczników pustyni; dla konia wojskowego cztery demony Wieży) są
teraz celem questowym — bite bez względu na poziom — a pustynia jest dla
takiego bota mapą pogranicza, dopóki nie zbierze stu zabójstw; potem
Stajenny za 500 000 yang wydaje konia bojowego jak dotąd.

### Panel klasyczny: strona „Gildie”

- Karta na pulpicie i przycisk na stronie zachowania botów: lista gildii
  botów z królestwem, klasą, poziomem, liczbą członków i botów online,
  mistrzem, średnią siłą, rankingiem, bilansem wojen (Z/R/P), expem
  otrzymanym od startu rdzeni i toczoną wojną z wynikiem. Rdzenie zapisują
  `playerbot_guild_status.tsv` co minutę; panel składa trzy pliki.

## 2.0.59 — 2026-09-16

Serwer 2.0.59; klient bez zmian (2.0.11).

### Stopniowe wpuszczanie botów

Dotąd cała kohorta wchodziła w ciągu minuty od startu, a gracz, który
uruchomił 2000 botów, dostał plac „jak w szpitalu”. Teraz w launcherze
(przycisk **LICZBA BOTÓW**, pozycja 13 menu tekstowego) obok liczby botów
ustawia się plan wejścia, w `.env` jako `PLAYERBOT_SPAWN_WINDOW_MINUTES`,
`PLAYERBOT_LATE_JOINERS` i `PLAYERBOT_LATE_JOIN_HOURS`:

- **okno wejścia** — w ciągu ilu minut od startu wchodzi kohorta
  (1–180; 1 = jak dotąd);
- **dodatkowi boty** — ilu botów ponad liczbę kohorty dołącza później,
  pojedynczo i równomiernie, w ciągu ilu godzin (1–168). Np. 1000 botów
  w 15 minut i 500 kolejnych w ciągu doby.

Dodatkowi dołączają jako „następne postacie za kohortą” każdego królestwa
(w proporcji do zarejestrowanych), a od chwili wejścia są traktowani jak
reszta: uzupełnianie po wypadnięciu i bany działają tak samo. Log:
`PLAYERBOT: late joiners`, `PLAYERBOT: late joiner`.

### Boty grają jak żywi ludzie (eksperymentalne, domyślnie wyłączone)

Na stronie zachowania botów panelu klasycznego doszedł przełącznik
**„Boty grają jak żywi ludzie”**. Włączony: każdy bot gra sesję 3–6 godzin
(pierwszą po starcie serwera od pół godziny wzwyż, żeby wylogowania
rozłożyły się w czasie), wylogowuje się, odpoczywa 3–9 godzin i wraca —
w danej chwili online jest około dwóch botów na pięć. Bot w drużynie gracza
czeka z wylogowaniem; jego stragan (sklep offline) stoi dalej jak u gracza.
Wyłączenie sprowadza odpoczywające boty z powrotem w kilka minut. Działa
w ciągu minuty od zapisania, bez restartu. Log: `PLAYERBOT_LIFE`, także
w pakiecie wsparcia.

### Stajenny: zmiana poziomu trudności skraca też trwające czekanie

Postać, która zamówiła konia (kucyka, Księgę Konia) albo trening pod
dłuższym czekaniem — np. 12 h z poziomu hard — czekała dalej stary czas
po przełączeniu na easy czy medium; 2.0.57 obiecywało inaczej (Hiob:
„zmieniałem tryby na łatwy, własny, dalej trzeba czekać”). Teraz przy
logowaniu i przy każdej rozmowie ze Stajennym pozostałe czekanie jest
przycinane do bieżącego ustawienia: na easy koń jest od razu (Stajenny
mówi „Gotowe! Porozmawiaj ze mną jeszcze raz”), na medium co najwyżej
4 h od tej chwili. Dotyczy pięciu questów konia: kucyk, dwie Księgi
Konia, treningi 1–10 i 11–19. Sama zmiana poziomu nadal wymaga restartu
serwera (`reload q` nie odświeża flag trudności).

### Metiny: przedział 16 poziomów, a kamienie Wieży Demonów jako cel

Przedział postaci bijących zwykły kamień Metin to najwyżej 16 poziomów
w każdą stronę: bot dołącza do bitego kamienia najwyżej 16 poziomów ponad
sobą (dotąd 30) i nie bije kamienia, z którego wyrósł o więcej niż 16
(krzywa dropu daje 1 % przy piętnastu poziomach przewagi — z takiego
kamienia nic już nie leci). Sam zaczyna kamień do 9 poziomów ponad sobą,
jak dotąd. Kamienie Wieży Demonów (8015–8019) to nie metiny, tylko cel
piętra: bot wspinający się z graczem (w jego drużynie, gracz na tej samej
mapie) bije je bez względu na poziom, a splash i zamach w ich pobliżu nie
są już wstrzymywane; bot sam omija je jak dotąd, bo ich rozbicie
teleportuje wszystkich z mapy.

## 2.0.58 — 2026-09-16

Serwer 2.0.58; klient bez zmian (2.0.11).

### Bot w drużynie gracza nie zmienia podziału doświadczenia

Bot dodany do drużyny gracza przy każdym sprawdzeniu drużyny (co
kilkanaście sekund) ustawiał podział z powrotem na równy, choć liderem był
gracz — reguła pisana dla drużyn botów była pytana także o drużynę gracza
(Dearminder). W drużynie prowadzonej przez gracza podział ustawia wyłącznie
lider; drużyny botów dalej dzielą po równo.

### Boty nie wykupują wszystkich Szkatułek Blasku Księżyca

Od 2.0.53 bot bez szkatułek kupował je z lad, i tysiąc botów opróżniało
wszystkie lady świata (sizowski: „wykupują dosłownie WSZYSTKIE”). Dwa
hamulce: gdy księga rynku liczy 30 lub mniej szkatułek na wszystkich ladach
świata, żaden bot nie kupuje kolejnej, a bot, który kupił jedną, czeka
20 minut przed następną. Otwieranie szkatułek z własnych dropów bez zmian.

### Druga wioska: boty docierają do jej drugiej połowy

Boty z Shinsoo omijały północ Jayang, a z Jinno południe Bakry (blasty).
Mosty i granice map są w porządku — sprawdzone na plikach map, z miasta
osiągalne jest 99,9 % terenu i 100 % grup spawnu. Bot wchodził bramą (na
Jayang na południu, na Bakrze na północy), zaczynał bić to, co miał pod
nosem, i tak rozchodził się od bramy do końca życia: tabela miejsc łowów
drugiej wioski nie miała pasm poziomów, a przejście do kolejnego miejsca
uruchamia się tylko wtedy, gdy nic w zasięgu nie jest warte bicia — bot na
33 poziomie dalej bił tygrysy z 18. Druga połowa mapy (Wielka Banda,
29–36 lvl) stała pusta.

- Każde miejsce łowów w M2 (Jayang, Bokjung, Bakra) ma zmierzone pasmo
  poziomu (mediana potworów w promieniu 25 m) i bot idzie tylko na miejsca
  swojego pasma (a gdy pasmo ma ich mniej niż cztery, dobiera najbliższe);
  tabele urosły z 12 do 24 miejsc na mapę. Dwanaście ręcznych miejsc
  Bokjung zmierzono przy okazji: trzy stały 2–4 km od najbliższego spawnu,
  dwa przy kilkunastu punktach.
- Potwór 6 i więcej poziomów pod botem, dalej niż 8 m, przestaje być celem
  także w drugiej wiosce (tak jak w pierwszej) — dzięki temu bot, który
  wyrósł z terenu przy bramie, rusza na teren swojego pasma. Obrona,
  zadania, materiały i sprzęt jak dotąd mają pierwszeństwo.

## 2.0.57 — 2026-09-16

Serwer 2.0.57; klient bez zmian (2.0.11).

### Poziom trudności świata: easy / medium / hard / własny

W launcherze doszedł przycisk **POZIOM TRUDNOŚCI** (i pozycja 22 w menu
tekstowym), a w `.env` klucze `M2_DIFFICULTY`, `M2_BIOLOGIST_WAIT_HOURS`
i `M2_HORSE_WAIT_HOURS`. Poziom mówi, ile gracz czeka u Biologa między
oddaniami i u Stajennego (kucyk, każda Księga Konia, treningi medalami
poziomów 1–10 i 11–19):

| poziom | Biolog | kucyk i Księgi Konia | treningi 1–10 / 11–19 |
|---|---|---|---|
| easy (domyślnie, jak dotąd) | 0 | 0 | 0 |
| medium | 8 h | 4 h | 6 h / 7 h |
| hard (jak w oryginale) | 24 h | 12 h | 18 h / 21 h |
| custom | własna liczba godzin | własna liczba godzin (jedna na wszystko) | jak obok |

Zmiana działa od następnego startu serwera (migrator zapisuje flagi
zdarzeń, questy je czytają). Postać, która już czeka, dostaje konia po
upływie nowego czasu. Botów to nie dotyczy — ich Biolog i stajenny nigdy
nie czekali. Z dyskusji na ogólnym („z harda na easy nieeee”).

### Eventy: szkatułki i raty o wybranych porach

W panelu klasycznym doszła strona **Eventy** (karta na pulpicie i przycisk
na stronie zachowania botów). Harmonogram to okna tygodniowe: dni, od
której do której, a dla rat o ile procent ponad ustawione raty serwera:

- **Szkatułki Blasku Księżyca** — gdy w harmonogramie jest choć jedno okno
  szkatułek, szkatułki dropią wyłącznie w oknach (suwaki na stronie AI
  mówią, jak często w oknie). Bez okna wszystko działa jak dotąd.
- **Doświadczenie, drop, yang** — w oknie raty serwera rosną o podany
  procent (50 = +50%) i wracają po jego końcu; raty ustawione ręcznie w
  trakcie eventu zostają, jak je ustawiono.
- **Aktywuj teraz** — włącza event na wybraną liczbę minut (15–360), z
  własnym procentem; przycisk **Zakończ** kończy go wcześniej.
- Na czacie pojawia się ogłoszenie na początku eventu, co piętnaście minut
  w jego trakcie i na końcu („Event: +50% doswiadczenia do 21:00!”).

Rdzeń gry odczytuje harmonogram w pięć sekund; nic się nie restartuje.
Status („aktywny do”, „następny”) pokazuje strona Eventy.

### Metiny dla każdego bota, bite razem

Zajęty kamień nie odstrasza już reszty: dotąd cel jednego bota był celem
zajętym, więc przy jednym metinie bił jeden bot, a reszta szła dalej
(Kiciamol). Kamień może bić do sześciu botów naraz. Każdy bot ocenia metin w
swoim paśmie wyżej niż zwykłe potwory, a kamień, który już ktoś bije, staje
się najważniejszym celem w zasięgu — bot dołącza nawet do metina do 30
poziomów ponad sobą, jeśli bije go inny bot. Kamień, który bije tylko gracz,
boty zostawiają graczowi (drop idzie do tego, kto zadał najwięcej obrażeń).

### Loot po bossie

Po zabiciu bossa bot dostaje takie samo okno na loot jak po rozbitym metinie:
przez 20 sekund zbiera, co leży w promieniu 15 m, zanim raid „wróci do pracy”.
Zgłoszenie Ciapka: skrzynia Umarłego Rozpruwacza (bronie 75 ze średnimi)
zostawiona na śniegu.

### Księgi Kombo i Dowodzenia

Sztuka Combo (trzy księgi) i Sztuka Wojny Sun Zi / Wu Zi / WeiLiao Zi
(Dowodzenie) szły do handlarza za tysiąc yang, bo nie są zwykłymi księgami
umiejętności. Bot czyta je teraz, gdy może (Kombo od 30 i od 50 poziomu,
Dowodzenie po dwadzieścia poziomów na księgę), zostawia sobie do trzech
sztuk, a resztę wystawia na straganie; handlarz ich nie dostaje. Kombo daje
botowi cios w więcej celów naraz, Dowodzenie liczy się dla bonusów grupy.
Zgłoszenie sizowskiego.

### Drobne

- Linia „login phase does not handle this packet! header 100” w syserr
  rdzenia logowania zniknęła: to logowanie osobnego połączenia klienta po
  znaki gildii, nie błąd (sizowski, wątek o logowaniu).

## 2.0.56 — 2026-09-16

Serwer (rdzeń gry i AI botów); klient bez zmian (2.0.11).

### Zwoje Błogosławieństwa trafiają na lady

Boty miały zwoje (na świecie testowym 1405 w torbach 294 botów), a na ladach
stało 5. Reguła „zostaw trzy dla własnego kowala” liczyła zwoje leżące w
torbie *przed* danym stosem, a wizyta serwisowa sklepu offline niczego wcześniej
nie dzieli — więc jedyny stos bota był zatrzymywany w całości niezależnie od
wielkości, a cięcie po 5 z 2.0.55 nie miało czego ciąć. Teraz zapas to liczba
zwojów: bot zostawia sobie trzy (handlarz surowcami jeden, bot bez pracy dla
zwoju żadnego), a nadwyżkę wystawia w liniach po 1–5. Zgłoszenie z kanału
ogólnego („A bodzi jak nie było tak nie ma”).

### Dropek medali z pełnym zapasem nie wraca do lochu

Dropek medali z pełnym zapasem (200 medali w torbie) wychodził z Lochu Małp
regułą wyjścia i w tej samej minucie wracał regułą wyprawy: na świecie
testowym 703 z 854 wizyt w godzinę trwały poniżej dziesięciu sekund, jeden
bot co pięćdziesiąt sekund. Reguła wyprawy pyta teraz o zapas tak jak reguła
wyjścia; pełny dropek poluje na ziemi swojej wioski, aż linia na jego ladzie
się sprzeda.

### Scalanie stosów według limitu przedmiotu

Medal Konny i Zwój Błogosławieństwa mają w tej wersji silnika stos po 20, strzały
po 1000. Scalanie stosów w torbie liczyło wszystko do 200, więc pełne stosy
medali były „scalane” bez skutku co pięć sekund u 110 botów (14 321 wpisów w
logu w dziesięć minut). Torba i magazyn liczą teraz według limitu przedmiotu;
zabezpiecza to też wpłatę do magazynu, która przy złym limicie mogła zgubić
stos.

### Drobne

- Łucznik zakłada strzały z torby przed zwykłym strzałem, nie dopiero przy
  umiejętności. Bot nigdy nie strzela bez strzał — silnik tego nie pozwala;
  zgłoszenie o strzelaniu bez strzał najpewniej brało się z widoku torby,
  który nie pokazuje kołczanu.
- Status podróży nie ogłasza już pustyni ani Doliny Orków botowi, którego
  podróż tam nie wysyła (dropki medali w drodze do Lochu Małp).

### Stajenny wydaje konia od razu

Kupno kucyka i każde ulepszenie konia (Księga Konia Wojskowego) kazało czekać
dwanaście godzin i wrócić po odbiór, a trening poziomów 11–19 — dwadzieścia
jeden godzin między dwoma treningami. Wszystkie te czasy są zniesione,
niezależnie od poziomu konia; poziomy 1–10 nie czekały już od 2.0.12.
Zgłoszenie greess.

### Panel: wyłącznik dropu Szkatułek Blasku Księżyca

Na stronie AI panelu (pod suwakami szkatułek) jest pole „Wyłącz drop Szkatułek
Blasku Księżyca”. Zaznaczone i zapisane zeruje oba suwaki dla gry (działa w
pięć sekund), a ich ustawienie pamięta i przywraca po odznaczeniu.

## 2.0.55 — 2026-09-16

Serwer (rdzeń gry i AI botów) i klient 2.0.11.

### Zwoje Błogosławieństwa w liniach po 1–5

Sklep offline bota wystawiał przy wizycie serwisowej cały stos — 20 zwojów za
4,7 mln w jednej linii, których nikt nie kupi do jednego ulepszenia. Bezpieczne
zwoje ulepszania (Zwój Błogosławieństwa, Magiczny Kamień, Zwój Boga Wojny +10 i
+15) idą teraz w liniach po najwyżej 5 sztuk, do 3 takich linii na jednym
straganie. Linia większa niż 5, która już stoi, wraca do torby przy najbliższej
wizycie serwisowej i wchodzi z powrotem podzielona. Zgłoszenie jaksiezabic.

### Sklep gracza od 15 poziomu bez 800 zabójstw

Silnik wymagał od gracza ośmiuset zabitych potworów, zanim otworzył „tobołek”,
jak na serwerze publicznym. Boty i GM-owie byli już zwolnieni, teraz gracz też:
wystarczy 15 poziom. Zgłoszenie gregoszky.

### Biolog bez limitu czasu również dla gracza

Doba oczekiwania między kolejnymi oddaniami okazów jest wyłączona; kolejny okaz
można oddać od razu. Eliksir Poszukiwacza nie jest już zużywany, bo nie ma czego
skracać. Boty nigdy tego czasu nie trzymały. Zgłoszenie namiot_.

### Tytuły botów do wyboru (klient 2.0.11)

W opcjach gry jest nowy wiersz „Tytuły botów”: Osobowość (jak od 2.0.53) albo
Klasyczne (tytuł rangi). Wybór zapisuje się w pliku `playerbot_titles.cfg`
obok klienta. Po przełączeniu na Klasyczne tytuł osobowości znika przy
najbliższej zmianie rangi bota, czyli po jego następnym zabiciu potwora.
Propozycja NerrVoVy. Sprawdzone kompilacją i testami, nie w grze — dajcie
znać, czy wiersz jest widoczny i przełącza.

### Panel Sebana 1.54.1

Aktualizator Seban dla VPS (skrypty hosta w `seban-panel/updater/`), wersja
mobilna, rankingi z prawdziwymi graczami, poprawiony ranking +9 i skuteczności
ulepszeń, liczba transakcji w gospodarce sklepów. Trzy nowe odczyty jego panelu
sięgały do tabeli, którą ma tylko jego serwer, i strona główna, `/manage` oraz
rankingi odpowiadały błędem 500 — panel tworzy ją teraz sam przy starcie.
Nasze poprawki z 2.0.47–2.0.49 zachowane.

## 2.0.54 — 2026-09-16

Serwer (rdzeń gry i AI botów) i klient 2.0.10.

### Auto Łowy podnoszą drop (klient 2.0.10)

Auto Łowy nie podnosiły dropu niezależnie od ustawień. Klient liczy pozycje od
rogu swojej mapy, a serwer podawał położenie przedmiotu we współrzędnych całego
świata, więc dla klienta każdy przedmiot leżał daleko poza zasięgiem
podniesienia. Teraz serwer podaje, gdzie leży przedmiot względem postaci, a
klient dolicza to do swojej pozycji. Tak samo punkt startu: zasięg łowów liczy
się od miejsca, w którym włączono Auto Łowy, a nie od miejsca, w którym postać
akurat stoi.

Wymaga serwera 2.0.54 i klienta 2.0.10. Tego nie dało się sprawdzić w kliencie
na naszym świecie testowym — dajcie znać, czy postać podnosi drop.

### Skrzynie startowe w pełnej torbie

Skrzynia startowa, której gra nie pozwalała otworzyć z braku wolnej kolumny
trzech pól (na przykład Skrzynia Mistrza II), otwiera się teraz tak jak
Szkatułka Blasku Księżyca: bot najpierw przekłada drobne przedmioty, żeby
zrobić miejsce.

## 2.0.53 — 2026-09-15

Serwer (AI botów, rdzeń gry, pliki gry, panel) i klient 2.0.9.

### Szkatułki Blasku Księżyca

Boty kupują i otwierają dużo Szkatułek Blasku Księżyca, a dropki je zbierają
i sprzedają. Do tej pory żaden bot szkatułek nie kupował, a te wystawione na
ladach się nie sprzedawały. Teraz:

- dropki (medali, M2, M3 i Metinów) podnoszą szkatułki, trzymają do 30 na
  sklep i wystawiają je w paczkach po 5; nadmiar otwierają,
- handlarz surowcami trzyma na ladę najwyżej 6 szkatułek, resztę otwiera,
- pozostałe boty szkatułek nie wystawiają, tylko je otwierają; gdy w pełnej
  torbie brakuje wolnej kolumny trzech pól, której gra wymaga przy otwarciu,
  bot przekłada drobne przedmioty, żeby ją zrobić,
- bot od 20 poziomu kupuje szkatułki z lady, dopóki ma ich mniej niż 10, gdy
  ma na nie miejsce i wolne złoto równe trzykrotności ceny (nie mniej niż
  milion), a na targ po szkatułkę idzie tylko wtedy, gdy któraś lada ją ma,
- na ladzie stoją najwyżej 3 linie szkatułek po 5 sztuk; większe linie, za
  drogie dla kupujących, wracają do torby przy obsłudze sklepu,
- szkatułkę, której gra nie pozwoliła otworzyć, bot próbuje znowu po minucie.

Na świecie testowym przed zmianami było 4643 szkatułki w torbach i 1916 na
ladach. Kwadrans po ostatnim wdrożeniu było ich 2761 w torbach i 851 na
ladach, a boty otwierały około 85 szkatułek na minutę zamiast 39. Najwięcej
zostaje w pełnych torbach: 26 botów z najwyżej dwoma wolnymi polami trzyma
601 szkatułek.

### Skrzynie Mistrza od 70 poziomu

Skrzynia Mistrza II (70 poziom) nie dawała nic ani botom, ani graczom: na
serwerach 2.x łańcuch skrzyń kończył się na 60 poziomie, a gra przy każdej
próbie otwarcia zapisywała błąd w syserr. Doszły zawartości Skrzyni Mistrza II,
Skrzyni Mistrza III i Skrzyni Arcymistrza, takie same jak na serwerach 1.x.
Bot, któremu gra odmówiła otwarcia skrzyni, nie próbuje już znowu co kilka
sekund. Na świecie testowym w pierwszych dziesięciu minutach 201 botów
otworzyło Skrzynię Mistrza II.

### Koń: boty nie schodzą bez potrzeby

Boty schodziły z konia na końcu każdej drogi, przed każdą księgą i przy
stajennym, a zaraz potem wsiadały z powrotem: na świecie testowym 24 389 zejść
w 36 minut, a 14 502 z 24 379 wsiadań wypadało w ciągu 6 sekund po zejściu.
Teraz jeździec zostaje w siodle przy NPC, ladzie, kowalu, skrzyni, księdze,
zmianie ekwipunku i w drodze do portalu. Z konia schodzi tylko wtedy, gdy
wymaga tego gra: do walki na zwykłym koniu, do pojedynku, do umiejętności i do
łowienia. Po każdym zejściu wsiada najwcześniej po 6 sekundach.

Książki boty czytają z siodła, a stajenny obsługuje jeźdźca. W pierwszych
dziesięciu minutach: 214 wsiadań na minutę zamiast 682, ponowne wsiadanie
w ciągu 6 sekund 20 razy na minutę zamiast 406, a przeczytanych książek 231
na minutę zamiast 106.

### Prędkość ruchu +20% na serwerach 2.x

Serwerowy bonus +20% prędkości ruchu na linii 2.x nigdy nie dawał prędkości:
quest podawał numer bonusu, a ten silnik oczekuje numeru cechy postaci, więc
każda postać dostawała +20 do maksymalnych PE. Teraz postać dostaje prędkość,
a stary bonus do PE znika przy najbliższym sprawdzeniu (co minutę). Klient
2.0.8 niczego w prędkości ruchu nie zmieniał, a limit prędkości w grze wynosi
200.

### Biolog po kolei

Zadania Biologa idą w kolejności z gry: Ząb Orka, potem Księga Klątw, potem
Pamiątka po Demonie. Boty rozpoczynały dalsze zadania bez ukończenia
poprzednich (na świecie testowym 866 postaci miało Księgę Klątw bez Zęba Orka).
Rozpoczęte już zadania zostają, ale bot bierze kolejne dopiero po ukończeniu
poprzedniego. Panel pokazuje ten sam etap co gra.

### Osobowość bota nad nickiem (klient 2.0.9)

Nad botem, w miejscu tytułu rangi, widać jego osobowość: Wytrwały poszukiwacz,
Pogromca Metinów, Towarzysz drużyny, Mistrz ekwipunku, Rozważny zbieracz,
Handlarz, Wędrowiec albo rodzaj dropki, każda w swoim kolorze. Pomysł: Kenny.
Wymaga serwera 2.0.53 i klienta 2.0.9.

### Fryzury z ItemShopu

Fryzury znów można zakładać. Pozostałe kostiumy nadal są wyłączone.

### Cennik Iwakury 1.1

Ceny botów pochodzą z nowego cennika Iwakury: niższe mnożniki części bonusów
(między innymi silny przeciwko ludziom, maks. PŻ, wartość ataku, odporności),
nowe ceny ulepszaczy, szkatułka 85 000, medal konny 180 000, złoty i srebrny
klucz, Gourou, wybielacz i farby do włosów. Wszystkie lady przeliczają ceny
przy najbliższej obsłudze sklepu.

Zasady wystawiania ekwipunku ustalone wcześniej zostają: ekwipunek poniżej 30
poziomu trafia na ladę od +6, a broń i zbroja na 1 poziom (np. Miecz) od +8.
Niżej idą do handlarza.

### Ekwipunek na ladzie

- Limit dwóch linii ekwipunku poniżej 30 poziomu liczy tylko przedmioty
  poniżej +7, więc +7 i wyżej bot wystawia bez limitu.
- Bot zatrzymuje w torbie jedną broń zapasową, a pozostałe wystawia.

### Ogłoszenia ulepszeń

Bot ogłasza na czacie tylko udane ulepszenie na +7, +8 albo +9, z nazwą nowego
przedmiotu. Wcześniej ogłaszał też nieudane próby ze zwojem („+4 na +3”).

### Płaszcze, symbole i martwe ryby

- Boty nie podnoszą Płaszcza Uciekiniera ani Symb. Króla Przepowiedni, a te,
  które mają w torbie (także z łowienia), sprzedają u handlarza. Założony
  symbol zostaje; gdy oba sloty są zajęte, jego miejsce zajmuje pierścień
  doświadczenia albo rękawica złodzieja.
- Z martwych ryb bot zatrzymuje najwyżej 10 (gdy ma drewno na ognisko), resztę
  sprzedaje.

### Auto Łowy (klient 2.0.9)

Uwaga: podnoszenie dropu w Auto Łowach nadal nie działa, niezależnie od
ustawień. Panel wymaga naprawy podnoszenia i będziemy nad tym pracować
w następnych aktualizacjach.

- Przełączniki podnoszenia to zwykłe przyciski z napisem, tak jak Metiny,
  Wstawaj i Wracaj: „Podnos: tak”, „Bron: nie”. Wcześniej wciśnięty przycisk
  znaczył „podnosi”, łatwo było go wziąć za wyłączony.
- Gdy podnoszenie jest wyłączone, przy starcie Auto Łowów pisze o tym czat.

### Tło wyboru postaci (klient 2.0.9)

Nowe tło ekranu wyboru postaci przy logowaniu, autorstwa ĹŌŞƬĒĶ.

### Questy

W questach i quizie gra nazywa się Metin2 SinglePlayer zamiast Metin2009
(plik tłumaczeń od l0st3ka).

## 2.0.52 — 2026-09-15

Serwer (AI botów, rdzeń gry, baza, panel) i klient 2.0.8.

### Auto Łowy dla graczy

Nowe okno Auto Łowów pod klawiszem K, za darmo i bez wymagań, z tym, co w
oficjalnej grze jest płatne: sześć umiejętności, dwie mikstury i trzy
przedmioty na zegarze, wskrzeszenie z ustawianym opóźnieniem, kamienie Metin
na życzenie i powrót na miejsce startu. Cel wskazuje serwer: potwory w zasięgu
od miejsca startu, najpierw te, które atakują gracza. Podnoszenie według
rodzaju: broń, zbroje, biżuteria, mikstury, księgi, kamienie i reszta. Każdy
rodzaj włącza się osobnym przyciskiem, a yang postać zbiera przy każdym
włączonym rodzaju. Przedmiot, do którego postać nie dojdzie w sześć sekund,
jest pomijany na dziesięć. Wymaga serwera 2.0.52 i klienta 2.0.8.

### Poprawki klienta

- Przycisk łączenia stosów w ekwipunku wysyłał wszystkie przesunięcia w jednej
  klatce (300 dla 25 stosów), a serwer przy 300 pakietach na sekundę zamyka
  połączenie, więc gracz wracał do ekranu logowania. Przesunięcia idą teraz po
  sześć co dziesiątą część sekundy.
- Kliknięcie pustego pola w edycji sklepu offline nie wywołuje już błędu.
- Opis Zielonej i Fioletowej Mikstury (prędkość ataku i ruchu) znów się
  wyświetla.

### Dropki zajmują się dropieniem

Dropki medali na 25 poziomie robiły wszystko poza lochem: Biologa, wyprawy,
odpoczynek na placu, gildie, targ, łowienie, kopanie i wyprawy po materiały.
Teraz żadna dropka (medali, M2, M3 ani Metinów):

- nie robi Biologa i nie odpoczywa w mieście,
- nie zakłada gildii ani do niej nie wstępuje, a z obecnej wychodzi (mistrz
  przekazuje gildię najsilniejszemu botowi, który nie jest dropką, a gildia
  jednoosobowa jest rozwiązywana),
- nie chodzi na targ, nie łowi, nie kopie i nie szuka materiałów,
- swój sklep offline obsługuje co 40–60 minut zamiast co 10–15.

Dropki medali, M2 i M3 nie jeżdżą na wyprawy po Metiny. Dropka medali nie idzie
na pogranicze, zostaje w Lochu Małp, dopóki ma miejsce na medal, zbiera zapas
200 medali i podnosi tylko medale, rzeczy do dalszej przeróbki, księgi i to, co
dołoży do posiadanego stosu. Dropka-łuczniczka nosi zapas 1000 strzał i dokupuje
je u kupca, bo wychodziła z lochu z pustym kołczanem i setkami mikstur. Żaden
bot nie wychodzi już z Lochu Małp obsługiwać sklepu offline.

Każdy bot w drodze po medal do konia nie jest już zawracany przez spacer na
targ w Joan ani przez plecak zapełniony w 45%.

### Unikaty

Boty nie noszą Pierścienia Niejawności (ukrywał poziom), Płaszcza Uciekiniera
ani Maski Sabaha. Pierścienie doświadczenia i rękawice złodzieja zakładają
tylko na polowanie, bo ich czas płynie wyłącznie wtedy, gdy są noszone:
zdejmują je w mieście, na sprawunkach, przy łowieniu i kopaniu, za ladą, w
pojedynku i po chwili bez walki. Dropka z blokadą doświadczenia nosi rękawice,
a pierścienia nie.

### Rzeczy do dalszej przeróbki

Korzeń Gango i Grzyb Tue od zielarza, Kryształowe Kolczyki, Zbroja Twarzy
Ducha, broń na 65 poziom, Fasolka Zen i Pigułka Krwi: boty zawsze je podnoszą,
nie sprzedają ich kupcowi (chyba że plecak jest pełny, a sklepu otworzyć nie
mogą) i wystawiają je na ladach obok ulepszaczy.

### Pojedynki

Bot w pojedynku zakłada buffy (np. Aurę Miecza), bije z bliska i regularnie
używa umiejętności. Szaman i sura czarnej magii walczą z dystansu, a wojownik
doskakuje do przeciwnika Szarżą albo Uderzeniem Miecza. Na czas pojedynku bot
wyłącza Eliksir Słońca i Eliksir Księżyca, którymi wcześniej leczył się mimo
zakazu mikstur, i włącza je z powrotem po walce.

### Świątynia Hwang bez klątwy i bez Maski Sabaha

W Świątyni Hwang ciosy bez Maski Sabaha nie chybiają już co drugi raz. Maska
przestała wypadać z potworów i ze skrzyni Hwang, nie ma jej w nagrodzie za
wprowadzenie do świątyni ani w sklepie, a przy każdym starcie serwera znika z
plecaków, magazynów, sklepów offline i slotów wszystkich postaci, także
graczy.

### Targi w Shinsoo i Jinno przy strażnikach

W Yongan, Jayang, Pyongmoo i Bakra targ stoi teraz wokół strażnika na okrągłym
placu, tak jak w Chunjo. Wcześniej stał przy handlarzach, a w Pyongmoo w
połowie poza strefą bezpieczną. Sklepy offline botów z dawnego targu
przenoszą się na nowe miejsce raz, przy pierwszym starcie po aktualizacji, z
zachowaniem układu. Sklepy graczy zostają tam, gdzie je postawili.

### Handel zamiast rozdawania

Boty nie oddają już za darmo ekwipunku słabszym botom ani nie przekazują
drużynie ksiąg i materiałów. To, co zdejmą, zostaje w plecaku, idzie na ladę
albo do kupca.

### Kamienie Duchowe, Fasolka Zen i ranga

- Boty czytają Kamienie Duchowe (trening Wielkiego Mistrza) zamiast sprzedawać
  je kupcowi. Czytają tylko wtedy, gdy ranga po zapłaceniu pełnej ceny
  zostaje nieujemna, a kamieni nie wystawiają na ladę.
- Bot z ujemną rangą zjada Fasolkę Zen, a pierwsze fasolki zostawia sobie w
  plecaku zamiast je wystawiać.
- Bot z ujemną rangą nie wychodzi ze strefy bezpiecznej, dopóki fasolka jej nie
  podniesie: na mapie wioski idzie na targ, z innej mapy wraca do swojej
  pierwszej wioski. Po fasolkę idzie na targ, także dropka.

### Szansa na otrucie

Od 50 poziomu boty cenią szansę na otrucie dwa razy wyżej przy przerzucaniu
bonusów i przy wyborze ekwipunku: jedno otrucie zabiera bossowi ćwierć życia.

### Broń na 30 poziom u kowala

Broń na 30 poziom ze średnimi obrażeniami poniżej 30% bot ulepsza do +4 u
kowala, a zwoje zużywa na nią dopiero od kroku na +5. Wcześniej jeden bot
zużył 10 z 12 zwojów na +3 i +4 broni z 1% średnich.

### Panel

Ranking Biologa i karta bota pokazują, ile zadań bot ukończył i nad czym
pracuje teraz, np. „6/9 ukończone • teraz: Ząb Orka 1/10”. Karta pokazuje też,
ile zadań pominął jako za niskie. Wcześniej obok liczby stała nazwa zadania o
tym numerze w tabeli, co nic nie mówiło o postępie.

### Konfiguracja

Wartości `M2_MOONLIGHT_CHEST_PERMILLE`, `M2_MOONLIGHT_CHEST_STONE_PERMILLE`,
`M2_DRAGON_COIN_STONE_PERMILLE` i `M2_DRAGON_COIN_BOSS_PERMILLE` z pliku `.env`
(szansa na Szkatułkę Blasku Księżyca i Smocze Monety) docierają teraz do
serwera. Na linii 2.x nie docierały nigdy i obowiązywały wartości domyślne.

## 2.0.51 — 2026-09-15

Serwer (AI botów, rdzeń gry, panel) i klient 2.0.7.

### Dymki nad głowami botów bez czatu

Status bota (dokąd idzie, z kim walczy, co robi w mieście) widać już tylko w
dymku nad jego głową. Wcześniej każdy status był też zwykłą wypowiedzią, więc
trafiał do historii czatu, a miasto pełne botów zapychało okno czatu. Wymaga
klienta 2.0.7: stary klient nie pokaże dymków statusu, ale też niczego nie
wpisze na czat. Ogłoszenia udanych ulepszeń +7/+8/+9 (najwyżej jedno na trzy
minuty na cały świat) i okrzyki handlowe zostają na czacie.

### Broń w ręce nie spala się bez zapasu

Bot nie ulepsza u zwykłego kowala broni, którą trzyma w ręce, gdy krok może
ją spalić, a nie ma zwoju, zapasowej broni ani kupca sprzedającego broń na jej
poziom. Kupuje wtedy zwoje z lad. Trzyma jedną zapasową broń (co najmniej o
połowie siły tej w ręce) i nie oddaje jej, nie sprzedaje ani nie wystawia. Ze
swojego sklepu offline odbiera przedmiot lepszy od noszonego co najmniej o
10%. Wojownik mentalny woli broń dwuręczną o 20% jej siły, a nie o stałą
premię, więc Gilotynowe Ostrze nie wygrywa już na 75 poziomie z mieczem na 55.

### Atlas broni

Boty znają każdą broń świata: klasę, poziom i źródło (kupiec, wspólny drop,
potwór, skrzynia). Bot z bronią wyraźnie słabszą od najlepszej osiągalnej na
jego poziom idzie na rynek, a broń silniejszą o 25% może kupić z oszczędności.
Wędka i kilof w ręce nie są liczone jako broń.

### Ceny broni według średnich

Średnie obrażenia i obrażenia umiejętności są wyceniane płynnie między progami
cennika Iwakury, więc 19% średnich kosztuje wyraźnie więcej niż 1%. Sklepy
przeceniają się same.

### Klucze, nadmiar towaru i marmury

- Klucze bez pasującej skrzyni (ponad dwa jednego rodzaju) trafiają na ladę, a
  przy pełnym plecaku do magazynu. Bot ze skrzynią kupuje do niej klucz.
- Chomikowane ulepszacze (ponad 50 sztuk ponad własne potrzeby) idą na ladę
  paczkami po 10, najwyżej trzy linie jednego rodzaju.
- Marmury polimorfii, klucze i nadmiar towaru są powodem do otwarcia sklepu.
- Sortowanie plecaka zamienia przedmioty miejscami: najpierw mikstury, potem
  skrzynie i klucze.
- Hełmy i tarcze bot podnosi zawsze.
- Materiał, na który jest popyt, bot wyjmuje z magazynu tylko wtedy, gdy
  plecak się przez to nie zapełni.

### Zwoje i szkatułki na rynku

Zwój Błogosławieństwa nie jest już wyceniany jak ulepszacz. Bot zostawia sobie
3 zwoje (handlarz zasobów 1), a resztę wystawia. Zwoje nie trafiają do
magazynu, a te, które już tam są, wracają do plecaka. Handlarz zasobów nie
otwiera Szkatułek Blasku Księżyca, tylko je sprzedaje (trzyma do 20 sztuk).

### Historia ekwipunku w panelu

Przy ulepszeniu w nawiasie widać, czym je zrobiono: (Kowal), (Kowal w Wieży
Demonów) albo nazwa zwoju, np. (Zwój Błogosławieństwa). Wpisy sprzed
aktualizacji pokazują (Kowal) albo (zwój). Nieudany zwój, który obniżył
przedmiot o poziom, nie jest już pokazywany jako „Spalone przy ulepszaniu”.

### Stabilność

Broń awaryjna kupiona przy pełnym plecaku lądowała na ziemi, a bot zakładał ją
z ziemi. Kończyło się to zniszczonym przedmiotem w slocie broni i wyrzuceniem
bota z gry. Bot kupuje teraz tylko z miejscem w plecaku i nie rusza slotu,
którego silnik naprawdę nie nosi. Wędkę i kilof zdejmuje tylko przy wolnym
miejscu w plecaku.

### Aktualizacja na Linuksie

`m2-updater` i instalator linii 1.x odmawiają pracy na serwerze 2.x i wskazują
`linux-port/tools/update.sh`. Wcześniej `docker compose exec updater
m2-updater` potrafił wgrać na serwer 2.x plik compose z MariaDB 10.11.

## 2.0.50 — 2026-09-15

Serwer (AI botów i rdzeń gry). Klient zostaje w wersji 2.0.6.

### Crash rdzenia przy wsiadaniu na konia

Rdzeń gry potrafił paść, gdy bot wsiadał na konia (dwa razy w sześć godzin
przy 2000 botów). Koń zniszczony inaczej niż przez zsiadanie jeźdźca —
najpewniej przez czyjąś umiejętność obszarową — zostawiał jeźdźcowi wskaźnik
na siebie, a najbliższe wsiadanie sięgało do pamięci po nim. Teraz zniszczony
koń zawsze odpina się od jeźdźca, a przywołanego konia nie da się zranić. Ten
sam crash groził też graczom, tylko rzadziej.

### Lochy Małp w Shinsoo i Jinno

Boty czerwonego i niebieskiego królestwa biły małpy tylko w pierwszej sali
swojego lochu, a boty Chunjo chodziły po wszystkich. AI znało układ sal i
drzwi tylko w lochu Chunjo i w dwóch trudniejszych. Teraz boty wszystkich
królestw chodzą po wszystkich jedenastu salach.

### Bonusy tylko z własnych kamieni

Boty dodawały i zmieniały bonusy bez Zaczarowania i Wzmocnienia Przedmiotu:
brakujący kamień powstawał z niczego za 25 000 yang i od razu był zużyty, a
historia ekwipunku pokazywała tylko jego zużycie. Teraz bot używa wyłącznie
kamieni, które ma w ekwipunku (z dropu i skrzyń). Bez kamienia nie bonusuje,
tak jak gracz.

### Panel F9/F10 u zwykłych graczy

Postać bez rangi GM dostawała „Ta komenda nie istnieje.” po każdym
teleporcie i zalogowaniu, a także po wciśnięciu F9 albo F10. Klient pyta
wtedy serwer, czy postać jest GM-em. Teraz serwer zwykłemu graczowi po prostu
nie odpowiada.

### /transfer na bota

`/transfer <nick bota>` przenosi bota do GM-a. Wcześniej bot znikał i wracał
w punkcie startowym swojej mapy. Bot z innego rdzenia (układ `split`) nie
może przejść na mapę rdzenia GM-a, więc GM dostaje o tym wiadomość zamiast
„Transfer requested.”.

### Boty w grupie gracza

Bot w grupie gracza nie odchodzi od niego do Biologa, handlarza, kowala ani
stajennego. Swoje sprawy wznawia po wyjściu z grupy. Walczy jak dotąd, a
szaman buffuje gracza także wtedy, gdy miał rozpoczęte zakupy. Poprawka
Pabloo.

### Dropki

Bot, który przerósł poziom swojego zajęcia o więcej niż dwa poziomy (np.
dropek na 45 poziomie przy metinach 35 poziomu w M2), nie dostaje już
osobowości dropka. Po restarcie serwera gra jak zwykły bot i znów zdobywa
doświadczenie.

### Górnictwo

Bot bity przy kopaniu rudy przestaje kopać i się broni. Wcześniej kopał do
śmierci, a po odrodzeniu porzucał rudę na 15–45 minut. Teraz po walce albo po
odrodzeniu wraca do żyły po około 45 sekundach.

### Panel i aktualizator

- Klasyczny panel: w teleportach postaci są też Jayang (M2 Shinsoo) i Bakra
  (M2 Jinno).
- Aktualizator na Linuksie: pobieranie manifestu przez curl ma limit czasu,
  więc nie wisi bez końca na „[1/4] reading what is published”.

## 2.0.49 — 2026-09-15

Serwer (AI botów) i panel Sebana 1.48.0. Klient zostaje w wersji 2.0.6.

### Boty znają obrażenia swojej broni

Boty liczą cios bronią tak, jak liczy go gra: szansę trafienia z
Zręczności i poziomu, obronę potwora, średnie obrażenia i obrażenia
umiejętności. Doliczają też ukryty bonus z poziomu broni, którego nie widać
w opisie przedmiotu. Na tym silniku bronie od 32 do 65 poziomu biją potwory
mocniej o 6–16% (Krwawy Miecz o 10%), a bronie na 70 i 75 poziom o 10%.
Bronie na 30 poziom tego bonusu nie mają. Szamani i sury czarnej magii
wreszcie cenią na broni bonus obrażeń umiejętności, który wcześniej nic dla
nich nie znaczył.

### Bronie na 30 poziom

- Bot liczy, jak mocno dana broń na 30 będzie bić na +7. Jeśli wyjdzie
  wyraźnie lepiej (o co najmniej 10%) niż wszystko, co ma, kupuje ją ze
  sklepu i ulepsza aż do +9. Dla wojownika na 45 poziomie Miecz Pełni
  Księżyca +7 z 25% średnich bije zwykłym ciosem o około 8% mocniej niż
  Krwawy Miecz +6, mimo ukrytego bonusu tego drugiego; żeby bot go kupił,
  miecz potrzebuje około 33% średnich.
- Nie kupuje drugiej takiej broni, gdy jedną już ulepsza albo nosi gotową
  (+7 lub więcej). Broni, którą ulepsza, nie wystawia na straganie.
- Broń z co najmniej 37% średnich obrażeń (albo 15% obrażeń umiejętności)
  ulepsza tylko zwojami, nigdy u kowala. Bez zwoju czeka i sama dokupuje
  zwoje na rynku.
- Słabsze bronie na 30 ulepsza u kowala do +9, a na ryzykownych krokach
  używa zwoju, jeśli go ma.
- Bronie na 30, medale konne i zwoje ulepszeń bot może kupić nawet za 80%
  wolnego złota. Przy wysokim kursie yang takie bronie kosztują miliony, a
  dotychczasowy limit (część średniego portfela botów) nie przepuszczał
  żadnej: w sklepach serwera testowego stało 2315 broni na 30, prawie
  wszystkie +0 do +3.
- Boty rozpoznają zwoje po tym, jak działają, a nie po numerze: Zwój Wojny
  (pewne ulepszenie do +4), Podręcznik Kowala i Zwój Boga Smoków (większa
  szansa), Magiczny Kamień (bez utraty poziomu, oszczędzany na najtrudniejsze
  kroki). Wcześniej używały tylko Zwoju Błogosławieństwa i jednego Zwoju Boga
  Smoków. Gwarancji, która niszczy przedmiot przy porażce, nie używają.

### Najpierw biolog

- Boty w każdym wieku oddają biologowi Zęby Orka, Księgi Klątw i Pamiątki
  po Demonie, które noszą, zanim wystawią je na sprzedaż albo zużyją u
  kowala. Dotąd bot starszy o ponad 10 poziomów od zadania trzymał je w
  torbie: na serwerze testowym 358 botów nosiło 1484 zęby, a zadania z zębami
  nie skończył żaden.
- Kowal zostawia w torbie tyle okazów, ile biolog jeszcze potrzebuje, razem
  z zapasem na odrzucone sztuki.
- Gdy biolog czeka już tylko na kamień duszy, nadmiarowe okazy idą na
  sprzedaż.

### Konie

Boty bez konia bojowego chodzą po medale do lochów małp dwa razy częściej,
także do średniego i trudnego. Na serwerze testowym w lochach było 17 botów
na 999, a przez godzinę oddano jeden medal. Boty powyżej 64 poziomu nie
chodzą już do lochu, bo przy takiej różnicy poziomów medale prawie nie
wypadają. Kupują je w sklepach.

### Pojedynki bez wędki

Bot z wędką albo kilofem w ręku nie przyjmuje już pojedynku i sam nikogo
nie wyzywa, a inne boty go nie zaczepiają. Pojedynek, który już trwał, gdy
bot wyjął wędkę, kończy się, zamiast toczyć się na wędki. Gracz, który wyzwie
łowiącego bota, dostanie odpowiedź, że bot łowi ryby.

### Stali dropiacy medali (do włączenia)

Nowe ustawienie w `.env`: `PLAYERBOT_MEDAL_DROPPERS` — tyle dodatkowych botów
na każde królestwo, ponad liczbę botów z launchera, z osobowością dropka
medali. Chodzą do Lochu Małp swojego królestwa, a od poziomu
`PLAYERBOT_MEDAL_DROPPER_LEVEL` (domyślnie 25) nie zdobywają już
doświadczenia, więc medale padają im na pełnej szansie i trafiają na
stragany. Są wybierani z postaci, które jeszcze nie grały, i po każdym
restarcie są to te same boty. Domyślnie ustawienie jest wyłączone (0).

### Ceny po zmianie kursu yang

Ceny w sklepach botów liczą się od kursu yang ustawionego w panelu. Po
zmianie kursu boty pamiętały jednak stare ceny i wystawiały przedmioty z
zerem za dużo albo za mało, a sklep przeceniał jeden przedmiot na godzinę.
Teraz zmiana kursu czyści pamięć cen, a każdy sklep przecenia cały towar
przy kolejnych wizytach właściciela.

### Panel Sebana 1.48.0

- Po restarcie i aktualizacji panel nie pokazuje przez kilka minut
  „Internal Server Error”.
- Profil gracza: sklep offline z przyciskiem teleportu do straganu, historia
  ekwipunku, logi na żywo, magazyn ze stronami i ikony umiejętności.
- Nowy ranking skuteczności ulepszeń, a karuzela rankingów na stronie głównej
  przewija się sama.
- Tooltipy przedmiotów liczą atak i obronę z ulepszeniem i mają poprawione
  nazwy bonusów.
- Poprawne granice map Las, Czerwony Las i Wieża Demonów.

Kontrolki liczby botów, respawnu na mapach i skrzyni startowej działają tylko
ze skryptami Sebana, więc zostają ukryte. Włącza je teraz zmienna
`M2_PANEL_CUSTOM_PATCHES=1` zamiast `SEBAN_GAME_INTEGRATION=1`.

## 2.0.48 — 2026-09-14

Serwer (AI botów, silnik gry, questy, baza logów i panel zaawansowany),
launcher i klient 2.0.6 z nowym ekranem logowania.

### Nowy klient 2.0.6

Nowy ekran logowania od ĹŌŞƬĒĶ: animowane tło, nowe logo i przycisk
Discorda. Klient pokazuje też status gry na Discordzie (Discord Rich
Presence). Launcher zaproponuje aktualizację klienta przy starcie. Paczka
podmienia w folderze klienta `pack/root`, `pack/locale` i `metin2client.exe`,
więc przed aktualizacją zamknij grę.

### Mniej taniego sprzętu na straganach

Boty zasypywały sklepy sprzętem +4 i +5 z niskich poziomów (9, 18, 26).
Na serwerze testowym było tego ponad 2400 linii, a jedna zbroja leżała
naraz w 348 sklepach. Teraz sprzęt poniżej 30 poziomu trafia na stragan
dopiero od +6, i najwyżej dwie takie rzeczy w jednym sklepie. Słabszy sprzęt
z tych poziomów, którego bot już nie potrzebuje, idzie do handlarza.

Sklepy, które już stoją, pozbywają się takich przedmiotów same: przy każdej
wizycie bot zdejmuje z lady jedną niepasującą rzecz.

### Nazwy sklepów od Iwakury

Boty nazywają sklepy tylko nazwami z listy Iwakury i dobierają je do towaru:
sklep z zębami orka nazywa się inaczej niż sklep z księgami klątw, a rybny
ma nazwę rybną. Sklep z przedmiotem +7, +8 lub +9 nosi nazwę tego przedmiotu
z plusem, najlepszym bonusem, dopiskiem „KD” i „TANIO” albo „OKAZJA”.
Kamienie duszy dają nazwę najcenniejszego kamienia, a sprzęt +0 do +3 –
nazwę z kategorii „do spalenia”. Co trzeci sklep dostaje losową nazwę
neutralną, bez względu na towar.

Znikają dawne przedrostki („Tanio:”, „Okazja:”, „Sprzedam”, „Wyprzedaz:”)
i napisy „Bron 30:”. Stojące sklepy dostają nową nazwę przy odnowieniu.

Dziewięć nazw z listy gra by odrzuciła: osiem jest dłuższych niż 32 znaki,
a „Nauka czytania dla opornych” zawiera zakazane słowo („porn” w środku).
Na razie ich nie ma; wystarczy je skrócić na liście.

### GM gra jak zwykły gracz

Postać GM kupuje w sklepach botów i graczy, otwiera własny sklep bez
nabijania 800 potworów, a jej poziom widać obok nicku. Nie dostaje już
wymuszonej ochrony PvP – obowiązują zwykłe zasady, z ochroną niskich
poziomów. Znaczek GM zostaje.

### Kostiumów nie da się założyć

Kostium założony na postać nie dawał się zdjąć, a postać było widać jako
samą broń. Teraz gra nie pozwala założyć kostiumu i odpowiada na czacie
„Kostiumy sa na tym serwerze wylaczone.”. Kostium, który ktoś ma już na
sobie, zostaje na postaci; żaden przedmiot nie jest usuwany.

### Pierścień Teleportacji działa

Użycie Pierścienia Teleportacji otwiera tę samą listę co Teleporter: w mieście
mapy wyjazdowe, poza miastem powrót do wiosek. Opłata jak u Teleportera.
Wcześniej pierścień nic nie robił, bo w paczce nie było questa, który
obsługuje jego użycie.

### Boty w Twojej grupie zostają przy Tobie

Bot w grupie gracza nie rusza już we własne podróże (Hwang, Sohan, pustynia,
targ w Joan, wizyta przy własnym sklepie). Wcześniej odlatywał, po sekundzie
wracał do gracza teleportem i po chwili odlatywał znowu, więc szaman prawie
nie miał kiedy rzucić wzmocnień.

### Hełmy i zbroje botów

Boty kupowały u handlarza hełmy innej klasy: sura dostawał hełm wojownika,
nie mógł go założyć i chodził bez hełmu. Na serwerze testowym 120 z 198 botów
bez hełmu miało w torbie cudzy. Teraz kupują hełm swojej klasy. Bot na
wysokim poziomie wybiera też zbroję z wyższego progu, zamiast nosić starą
+6 z pierwszego poziomu.

### Karta Wędkarska nie wyrzuca bota z gry

Wędkujący bot zakładał Kartę Wędkarską, przegląd ekwipunku zaraz zamieniał
ją na lepszy przedmiot do tego samego miejsca (Maskę Sabaha), a wędkowanie
zakładało kartę z powrotem. Zamiana co sekundę lub dwie uruchamiała w grze
ochronę przed zbyt szybką zmianą ekwipunku, która wyrzucała bota z gry co dwie
minuty (19 razy w ciągu 36 minut na serwerze testowym). Teraz karta, o którą
poprosiło wędkowanie, zostaje na postaci przez 10 minut.

### Kowal nie zdejmuje przedmiotu, którego nie ulepszy

Bot u kowala zdejmował założony przedmiot do ulepszenia, zanim sprawdził,
czy ma na nie materiały i yang. Kowal odmawiał, bot zakładał przedmiot
z powrotem, a po trzech sekundach znowu go zdejmował – i tak przez całą
wizytę. Na serwerze testowym boty zakładały w ten sposób zbroję około 3000
razy na godzinę. Teraz bot zdejmuje przedmiot tylko wtedy, gdy ma wszystko,
czego wymaga ulepszenie.

### Panel zaawansowany zaraz po aktualizacji

Kolektor panelu Sebana ponawia połączenie z bazą po kilku sekundach, a nie po
5 minutach, więc strona główna i strona sklepów nie zwracają już błędu 500
zaraz po aktualizacji.

### Launcher

Linia z wersją klienta nie jest ucinana, gdy launcher pokazuje informację
o nowej wersji.

### Ulepszanie u kowala

Wpis bota o ulepszeniu w logach pokazuje, jakich materiałów wymagała
receptura i ile bot ich miał tuż przed próbą.

### Dziennik podejrzanych zachowań

Tabela `log.hack_log`, do której serwer zapisuje wykryte podejrzane zachowania
(na przykład zbyt szybką zmianę ekwipunku), nie miała dwóch kolumn: loginu
i adresu IP. Każdy taki zapis kończył się błędem w syserr, a tabela zostawała
pusta. Aktualizacja dodaje brakujące kolumny przy starcie serwera i poszerza
kolumnę z nazwą postaci do 24 znaków. Istniejące dane zostają bez zmian.

## 2.0.47 — 2026-09-14

Serwer (AI, silnik gry, questy i panel zaawansowany). Nowy cennik Iwakury dla
botów, poprawione podnoszenie przedmiotów w grupie, naprawiony quest „Zbadaj
przeklęte zwierzęta” i panel Sebana w wersji 1.41.0. Klient bez zmian (zostaje
2.0.5).

### Cennik Iwakury v1.0

Boty wyceniają towar na straganach według nowego, pełnego cennika Iwakury.
Doszły ceny bransolet, naszyjników, kolczyków, butów i tarcz, rud i przetopów,
materiałów gildii oraz ulepszaczy z mt2009, a ceny ksiąg, ulepszaczy i opasek
zostały zaktualizowane.

Wszystkie ceny rosną teraz z mnożnikiem dropu yang według jednej tabeli
z cennika: 100% to x1, 200% to x2,2, 500% to x5 i tak dalej aż do 10000%, czyli
x100. Wcześniej księgi i ulepszacze liczyły się każde po swojemu.

Bransolety, naszyjniki, kolczyki, buty i najprostsza tarcza na +0 do +3 idą
do handlarza, a nie na stragan, tak jak w cenniku. Mnożniki bonusów obejmują
teraz buty, bransolety, naszyjniki, kolczyki i tarcze. Poprawione są trzy
bonusy, które wcześniej nie podnosiły ceny: szansa na kradzież PE, punkty
doświadczenia i odbicie ciosu na zbroi.

Cena na straganie zmienia się stopniowo, o kilka procent co kilka minut.
Po aktualizacji stragany dochodzą więc do nowych cen w ciągu kilku godzin.

### Podnoszenie przedmiotów w grupie

Gdy bot z Twojej grupy podnosił przedmiot, który wypadł dla Ciebie, przedmiot
trafiał do Ciebie, ale czat pisał, że otrzymuje go bot. Przedmiot nie łączył
się też z takim samym przedmiotem w Twoim ekwipunku, tylko zajmował nowe pole.
Teraz komunikat podaje Ciebie, a przedmiot najpierw dokłada się do stosu, który
już masz. Na nowe pole trafia tylko to, co się w stosie nie zmieści. Błąd
zgłosił mkls6649, a przyczynę i poprawkę przygotował Kenny.

### Quest „Zbadaj przeklęte zwierzęta” (19 poziom)

Z czterech przeklętych niedźwiedzi quest liczył tylko dwa: Grizzly i Czarnego.
Przeklęty Niedźwiedź i Przeklęty Brązowy Niedźwiedź nigdy nie dawały skóry.
Teraz daje ją każdy z czterech, z tą samą szansą. Postać, która ma quest
w toku, niczego nie traci. Przyczynę znalazł Pabloo po zgłoszeniu Dixdrosa.

### Panel zaawansowany (Seban) 1.41.0

Nowa wersja panelu od Sebana. W sklepach offline jest podgląd sprzedaży na
żywo, ranking najlepiej sprzedających się ksiąg i wykres tempa sprzedaży.
Doszły mapy Las, Czerwony Las i Wieża Demonów. W profilu postaci są nowe
akcje: VIP, Smocze Monety, zmiana nicku, powrót do stolicy i usunięcie
postaci.

Trzy ustawienia z tej wersji, czyli docelowa liczba botów, respawny na mapach
i wyłączanie skrzyni startowej, wymagają skryptów gry, których nasz serwer nie
ma. Są więc ukryte, zamiast udawać, że działają. Poprawione są też granice map
Las, Czerwony Las i Wieża Demonów oraz strona sklepów offline, która na
świeżej instalacji kończyła się błędem.

## 2.0.46 — 2026-09-14

Serwer (AI i launcher). Boty nie stoją już nad łupem, który nie mieści się
w torbie, i nie szukają Kapitana Bestii z cudzej wioski. Godzina w panelu
i w logach to teraz godzina Twojego komputera. Klient bez zmian (zostaje 2.0.5).

### Łup, który nie mieści się w torbie

Bot szedł po każdy przedmiot na ziemi, jeśli miał w torbie choć jedno wolne
pole. Miecz albo zbroja zajmują jednak dwa lub trzy pola w jednej kolumnie,
więc silnik odmawiał podniesienia, a bot co kilka sekund wracał do tego samego
przedmiotu i stał nad nim, dopóki serwer go nie zresetował. Na naszym świecie
było to około 56 odmów na minutę. Teraz bot przed wyjściem sprawdza, czy
przedmiot się zmieści, i odmów jest zero.

### Potwór, do którego nie da się dojść

Gdy bot trzy razy nie znalazł drogi do potwora, odkładał go na pół minuty.
Część AI, która sprawdza, kto go w tej chwili atakuje, od razu mu go jednak
oddawała. Jeden z naszych botów stał tak 13 minut w Lochu Małp pod małpą
strzelającą z półki skalnej. Teraz obie części pamiętają ten sam znacznik.

### Wieża Demonów

Od 2.0.44 boty nie celują w kamień Wieży i nie trafiają go zamachem. Kamień
mogła jeszcze trafić umiejętność obszarowa rzucona w potwora obok. Na mapie
Wieży bot nie używa już takiej umiejętności, jeśli w jej zasięgu stoi kamień.
Nie rozbije go więc przypadkiem i nie przeniesie wszystkich z mapy do nowej
Wieży.

### Kapitan Bestii z innej wioski

Boty od 35 poziomu w drugiej wiosce (Jayang, Bokjung, Bakra) idą na Kapitana
Bestii, kiedy ten się pojawi. Gdy wszystkie królestwa działają na jednym
rdzeniu (`M2_PLAYERBOT_WORLD_LAYOUT=unified`), serwer pamiętał tylko, że
Kapitan stoi, ale nie w której wiosce. Boty z Bokjung próbowały więc dojść
do Kapitana z Jayang albo z Bakry. Takiego punktu nie ma na ich mapie, więc
nie mogły wyznaczyć drogi i zamiast polować stały w miejscu, dopóki Kapitan
w innej wiosce nie padł. Teraz każda wioska ma własną odpowiedź. Bot nie idzie też do żadnego punktu spoza swojej mapy, a serwer
zapisuje taki przypadek w logu błędów.

### Godzina w panelu i w logach

Kontenery serwera liczyły czas w UTC, bo tak było w przykładowym pliku
ustawień. Dlatego panel i logi były o dwie godziny za zegarem komputera.
Launcher na Windows ustawia teraz raz w `.env` strefę czasową komputera
(`M2_TZ`), a kontenery przejmują ją przy starcie. Na Linuksie robi to
`update.sh` uruchomiony na samej maszynie. Jeśli aktualizujesz serwer na
Linuksie lub VPS przyciskiem w panelu, aktualizator nie widzi strefy maszyny.
Wpisz ją wtedy sam, na przykład `M2_TZ=Europe/Warsaw`, i uruchom serwer
ponownie.

Strefy wpisanej wcześniej ręcznie nic nie nadpisuje. Kto chce zostać przy UTC,
może po aktualizacji wpisać `M2_TZ=UTC`, a launcher nie zmieni tego drugi raz.
Od tej chwili godziny w logach i w paczce ZBIERZ LOGI to czas lokalny.

## 2.0.45 — 2026-09-14

Serwer (AI i launcher). Bot w grupie gracza idzie za nim przez teleport, a
szaman z grupy buffuje gracza. Klient bez zmian (zostaje 2.0.5).

### Bot idzie za graczem przez teleport

Bot zaproszony do grupy szedł za graczem tylko po tej samej mapie. Gdy gracz
przechodził przez portal, korzystał z Teleportera albo z pierścienia, bot
zostawał tam, gdzie był. Teraz, kiedy gracz stanie na nowej mapie, bot z jego
grupy przenosi się w to samo miejsce. Nie wejdzie za graczem do lochu z
osobną instancją, takiego jak Wieża Demonów, ani na mapę, na której boty tego
serwera nie mogą stanąć. Do Lochu Pająków dochodzi jak zawsze, przez pustynię.

### Szaman buffuje gracza

Szaman w grupie gracza rzuca swoje wzmocnienia najpierw na gracza, a dopiero
potem na siebie, i leczy gracza, który ma mniej niż 60% życia. Jeśli stoi za
daleko, podchodzi bliżej, a ze zwykłego konia zsiada, bo z niego nie da się
rzucać umiejętności. Wzmocnień bojowych używa w walce, a szybkości i leczenia
także poza nią.

### Paczka logów

ZBIERZ LOGI zbiera teraz także przyjęcia zaproszeń do grupy, więc z paczki
widać, kiedy bot do niej wszedł.

Na naszym świecie nie da się tego sprawdzić bez drugiego gracza — jeśli bot
nie pójdzie za Tobą albo szaman Cię nie buffuje, wciśnij ZBIERZ LOGI i napisz,
o której to było.

## 2.0.44 — 2026-09-14

Serwer (AI). Automatyczne mikstury bez pętli, która obciążała serwer, boty,
które nie przenoszą już graczy do Wieży Demonów, i boty, które zostają w
grupie gracza, dopóki gracz jej nie rozwiąże. Klient bez zmian (zostaje
2.0.5).

### Koniec pętli Eliksiru Księżyca

Boty brały Eliksir Słońca i Eliksir Księżyca — automatyczne mikstury HP i
PE — za eliksir doświadczenia i próbowały go użyć przy każdym przebiegu.
Prawie każdy bot nosi pusty Eliksir Księżyca ze skrzyni ucznia, więc silnik
raz za razem odpowiadał, że mikstura jest pusta: na naszym świecie około
580 tysięcy wpisów w logu na godzinę i praca rdzenia na nic. Teraz bot
włącza pełną automatyczną miksturę raz i zostawia ją włączoną, a pustą
sprzedaje u handlarza. Sprawdzone: log rdzenia gry zmalał o 60%, a pustych
eliksirów w torbach botów ubyło w kwadrans z 992 do 457.

### Boty nie przenoszą już graczy do Wieży Demonów

Zniszczenie Metinu Twardości w Wieży Demonów uruchamia quest, który po
sześciu sekundach przenosi do nowej Wieży wszystkie postacie z mapy, na
której stoi wtedy ten, kto kamień zniszczył. Boty rozbijały ten kamień jak
każdy inny Metin, a gdy bot zdążył w tych sześciu sekundach zmienić mapę,
do Wieży trafiali wszyscy z mapy, na którą przeszedł. Tak sizowski, stojący
pod Lochem Małp w Bokjung, znalazł się nagle na piętrze Wieży. Boty nie
atakują już kamieni questowych Wieży (8015–8019) i nie ranią ich
uderzeniem obszarowym. Na naszym świecie jedno zabicie tego kamienia
przeniosło naraz 10 postaci; od poprawki nie było ani jednego takiego
przeniesienia.

### Bot zostaje w grupie gracza

Bot zaproszony przez gracza wypadał z grupy po chwili, a wyrzucały go z niej
trzy rzeczy. Reset po półtorej minuty stania — a bot przy stojącym graczu
stoi właśnie dlatego, że idzie za nim. Każde przeniesienie bota na inną
mapę, także powrót na nogi po przeniesieniu do Wieży Demonów, stąd gracz
zostawał sam w swojej grupie. I kilka sekund teleportu samego gracza, w
których grupa nie widzi jego postaci, a bot brał ją wtedy za grupę botów:
w logach sizowskiego bot jest w jego grupie o 15:42:40, o 15:42:54 postać
gracza wchodzi do gry na nowo (tak wygląda każdy teleport), a o 15:42:56
bota w grupie już nie ma. Teraz o końcu grupy decyduje tylko gracz. Na naszym świecie nie da się tego sprawdzić bez
drugiego gracza — jeśli bot nadal wyjdzie z Twojej grupy, wciśnij ZBIERZ
LOGI i napisz, o której to było.

## 2.0.43 — 2026-09-14

Serwer (AI, panel i launcher). Suwak zwojów w panelu, pojedynki kończące
się tam, gdzie się rozstrzygnęły, grupy botów w obrębie królestwa, porządek
w magazynach i boty, które nie rzucają się już na każdą miksturę. Klient bez
zmian (zostaje 2.0.5).

### Od jakiego plusa boty używają zwojów

W klasycznym panelu, w zachowaniu botów, jest nowy suwak „Zwoje
Błogosławieństwa i Boga Smoków” od +1 do +9. Ustawia najniższe ulepszenie,
na które bot użyje takiego zwoju. Przy +7 zwój idzie tylko na ulepszenia na
+7, +8 i +9, a każde niższe bot robi u kowala bez zwoju, jak gracz, który
zwojów nie ma — więc przedmiot może spłonąć. Przy +1 (domyślnie) nic się nie
zmienia. Działa w pięć sekund, bez restartu, a panel zaawansowany zachowuje
to ustawienie przy zapisie swoich suwaków.

Sprawdzone na naszym świecie z progiem +8: przez 23 minuty 24 ulepszenia pod
zwojem na +8, 3 na +9 i ani jednego na +7 lub niżej. Przy okazji Zwój Boga
Smoków od +7 działa teraz także przy ulepszaniu poza kowalem, tak jak u
kowala.

### Pojedynek kończy się tam, gdzie się rozstrzygnął

Bot, który padł w pojedynku, kończy go w tej samej chwili. Wcześniej wstawał
i jeszcze kilkanaście sekund „walczył”, a przed 2.0.41 potrafił wtedy bić
zwycięzcę, który nie mógł mu oddać (Drip). Pojedynek znika też z silnika:
dotąd po wygranej z botem gracz przez dziesięć minut nie mógł go ani
uderzyć, ani wyzwać ponownie. Bot na zwykłym koniu zsiada do pojedynku —
silnik nie pozwala bić z takiego konia, więc taki bot w ogóle nie walczył.

Sprawdzone: 30 pojedynków, każdy zakończony razem z parą w silniku, każda
przegrana w sekundzie śmierci bota, 5 zejść z konia i zero kar rangi.

### Grupy tylko w obrębie królestwa

Boty zakładały grupy z botami innych królestw, czego gracz zrobić nie może
(l0st3k). Teraz dobierają się tylko we własnym królestwie, tak jak wymaga
silnik. Sprawdzone: 385 nowych grup, żadna mieszana.

### Magazyn bez rozsypanych stosów

Magazyn w tym silniku nie łączy stosów, a bot odkładał każdy stos na nowe
pole, więc magazyny zapełniały się osobnymi paczkami tych samych
materiałów (jaksiezabic). Teraz bot dokłada do stosu, który już leży w
magazynie, a przy każdej wizycie scala stosy rozsypane wcześniej. Na naszym
świecie w 16 minut rozsypanych stosów ubyło z 357 do 234.

### Tanie łupy zostają na ziemi

Bot od 40 poziomu, który ma co najmniej 500 tys. yang, nie biegnie już po
mikstury, sprzęt co najmniej 10 poziomów poniżej swojego (do +3, bez cennych
bonusów) ani zioła, jeśli są warte u handlarza mniej niż 40 tys. yang
(sizowski, próg od Tieru). Materiały do ulepszeń, księgi, zwoje, szkatułki,
kamienie, sprzęt, który mógłby założyć, i yang zbiera jak dotąd, a młode i
biedne boty zbierają wszystko.

### Paczka diagnostyczna

ZBIERZ LOGI zbiera teraz także linie pojedynków, łupów i teleportów z panelu
GM (F9), a panel zapisuje w logu każde użycie „Teleportuj mnie” — żeby
zgłoszenie „nagle mnie przeniosło” dało się sprawdzić w logach.

## 2.0.42 — 2026-09-14

Serwer (panel). Klasyczny panel pokazuje wreszcie Las, Czerwony Las i Wieżę
Demonów na mapie na żywo. Klient bez zmian (zostaje 2.0.5).

### Trzy mapy, na których boty polują, a panel ich nie pokazywał

Od 2.0.39 boty expią w Lesie i w Czerwonym Lesie i polują w Wieży Demonów,
a klasyczny panel znał tylko nazwy i granice tych map. Nie było ich w
filtrze „Mapa” i nie miały tła, więc nie dało się ich wybrać ani zobaczyć
botów, które tam są (Mkls na Discordzie, o lesie).

Teraz wszystkie trzy są w filtrze na stronie mapy, a tło każdej jest
narysowane z tych samych plików terenu, po których chodzą boty. Wieża
Demonów wygląda jak dziewięć okrągłych pięter, bo tak jest zbudowana.
Sprawdzone na naszym świecie: 9 botów w Wieży Demonów i 1 w Lesie, każdy na
swoim miejscu na tle.

Panel zaawansowany (Sebana) zostaje bez zmian: tam mapy dodaje autor tego
panelu.

## 2.0.41 — 2026-09-14

Serwer. Pojedynki botów przestały się liczyć jako morderstwa, więc ranga
wraca do zera i już nie spada. Boty wchodzą w głąb lochów małp zamiast stać
w pierwszym korytarzu, zwój błogosławieństwa chroni przedmiot, który bot ma
na sobie, a zioła i Dłonie przestają zapychać plecaki. Po starcie serwera
właściciele sklepów offline nie wychodzą już wszyscy naraz z lochów i z
frontu. Klient bez zmian (zostaje 2.0.5).

### Pojedynki: cios pada tylko tam, gdzie silnik na niego pozwala

Boty z siódmego czy dziewiątego poziomu chodziły jako „Agresywny” i
„Złośliwy”, a pojedynki poniżej piętnastego poziomu i w mieście trwały bez
końca, z samymi animacjami umiejętności (nerrvous_s, djariczek). Przyczyna
była jedna i siedziała w naszym kodzie.

Zwykły cios bota zadawał obrażenia wprost, a silnik przy takim wywołaniu nie
pyta o nic: ani o zgodę na pojedynek, ani o ochronę poniżej piętnastego
poziomu, ani o strefę bezpieczną. Wyzywający uderzał więc, zanim drugi bot
się zgodził, a zwycięzca bił dalej przeciwnika po jego odrodzeniu, choć
silnik uznał już pojedynek za zakończony. Dla silnika każde takie zabójstwo
było morderstwem we własnym królestwie: **minus dwadzieścia tysięcy rangi**,
rozdzielone między członków drużyny zabijającego. Stąd ujemna ranga u botów,
które same nikogo nie zabiły. Na naszym świecie takich botów było **105,
najniższa ranga −149 547**, a do południa silnik rozdzielił na drużyny 47
takich kar. Umiejętności szły drogą, która o zgodę pyta, dlatego tam, gdzie
cios nie mógł paść, zostawały same animacje.

Teraz każdy cios i każda umiejętność w postać przechodzi przez tę samą
kontrolę silnika. Pojedynek, w którym silnik odmawia ciosu przez piętnaście
sekund, bot uznaje za skończony. Wcześniej żaden pojedynek się nie kończył,
tylko wygasał po trzech minutach. Bot nie wyzywa i nie przyjmuje pojedynku
poniżej piętnastego poziomu ani w strefie bezpiecznej, a gracz dostaje na
czacie informację, dlaczego.

Ranga zepsuta przez ten błąd jest zerowana przy starcie serwera, zanim
rdzeń wczyta boty. U nas: **105 botów, po migracji 0**.

W pierwszych dziesięciu minutach po wdrożeniu: 9 wyzwań, 5 zgód, 4 odmowy w
strefie bezpiecznej i 8 pojedynków zakończonych (6, bo silnik nie pozwalał
już zadać ciosu, 2 w strefie bezpiecznej). **Ani jednej kary rangi**, a w
bazie nadal zero botów z ujemną rangą.

### Lochy małp: boty idą w głąb

„Boty biegają w jednym miejscu, a nie po całym lochu” (bierzyn, uxietoszef).
Loch to jedenaście komór połączonych drzwiami, które przenoszą każdego, kto
podejdzie na trzysta jednostek. Były tu dwa błędy i jeden brak.

Pierwszy: bot, który przeszedł przez drzwi, lądował obok drzwi prowadzących z
powrotem i wracał nimi, bo stanął tam do walki albo trasa do starego celu
prowadziła przez te same drzwi. Z 87 szybkich powrotów do komory wejściowej
52 zrobił silnik, a 35 nasza nawigacja. Teraz drzwi nie przenoszą bota
przez 45 sekund od ostatniego przejścia, a nawigacja w tym czasie nie
planuje trasy przez drzwi i zapomina cel z poprzedniej komory.

Brak: komora wejściowa ma 6–7% miejsc odrodzenia potworów, a gościła prawie
wszystkich odwiedzających. Teraz bot przy wejściu losuje, zależnie od
liczby potworów w komorach, czy zostaje, czy idzie do jednych z drzwi. Po
drodze broni się przed tym, co go atakuje.

Drugi błąd wyszedł w pierwszym pomiarze: marsz miał czterdzieści sekund
liczonych razem z walką, a korytarz wejściowy ma do drzwi 18–21 tysięcy
jednostek i jest pełen agresywnych małp. Doszły dwa marsze z sześciu.
Teraz liczy się sam marsz, do minuty, z górnym limitem trzech minut.
Zmierzone po poprawce: **9 z 9 dokończonych marszów przeszło do innej
komory, żaden się nie poddał**, a stary limit zawróciłby 7 z tych 9.

Uczciwie: na mapie 108 boty często znajdują medal i wychodzą z lochu, zanim
dojdą do drzwi. Idą wtedy korytarzem, a nie stoją przy wejściu, ale do
innej komory nie docierają.

### Zwój błogosławieństwa chroni założony przedmiot

„Boty biegają do kowala, palą swój główny przedmiot, kupują nowy i robią to
samo, zamiast ulepszać zwojem” (uxietoszef). Zwój w plecaku podnosił cel
ulepszania do +9, ale sam zwój był używany dopiero od +6. Kroki z +4 i z +5,
czyli 80 i 60 procent szansy, szły do zwykłego kowala, który przy porażce
niszczy przedmiot. Teraz przedmiot, który bot ma na sobie, idzie pod zwój
przy każdym kroku o szansie 80% lub mniejszej, jeśli bot ma zwój. Zapasowe
przedmioty z plecaka zostają przy starej zasadzie od +6, żeby nie
przepalać zwojów, których na rynku brakuje.

Uczciwie: na naszym świecie zwojów jest mało (98 sztuk w 20 plecakach),
więc w dziesięć minut widać tu ledwie pojedyncze kroki poniżej +6 pod
zwojem. Efekt pokażą światy, na których zwojów jest dużo.

### Zioła i Dłonie nie zapychają plecaków

Zioła (Pokrzywa, Kwiat Brzoskwini, Korzeń Gango, Kwiat Kaki i reszta do
Jaskiniowego Grzyba) oraz dwa wywary służą w tym świecie wyłącznie do
ulepszania Nożyka Zielarza. Żaden przepis na broń ani zbroję ich nie
potrzebuje. Boty trzymały je jako materiały, na naszym świecie w **850
plecakach i 400 magazynach**. Teraz sprzedają je handlarzowi, a te leżące w
magazynie wyjmują przy wizycie u magazyniera: **413 wyjęć w pierwszych
dziesięciu minutach**. Lista nie jest wpisana na sztywno: bot odczytuje ją z
tabel przedmiotów i przepisów, więc świat z innymi przepisami dostanie
własną odpowiedź. Rudy do Kilofa i ryby do Wędki, które też nie idą na broń
ani zbroję, zostają przy swoich zasadach z górnictwa i wędkowania.

Dłoni Krytyka i Przebicia nie da się ani sprzedać, ani wystawić na straganie,
a przy wysokich szansach na szkatułki zajmowały po trzy pełne stosy (uxietoszef).
Bot zatrzymuje teraz sto sztuk każdej, a resztę wyrzuca przy wizycie u
handlarza.

### Po starcie serwera sklepy nie wyciągają wszystkich naraz

Pierwsza wizyta właściciela w sklepie offline miała być rozłożona na minutę,
ale wszystkie numery postaci są na tyle małe, że rozrzut wynosił dwie i pół
sekundy. Po restarcie **560 botów poszło do sklepów w medianie 33 sekund**,
451 zmian mapy w dwie minuty. Każdy właściciel sklepu, który wystartował w
lochu albo na froncie, był z niego wyciągany po pół minucie, a fala wracała
co 10–15 minut. Teraz pierwsza wizyta wypada między pół minuty a dziesięć
i pół minuty: zmierzone **od 90 do 585 sekund (10.–90. percentyl),
najwyżej 67 wizyt na minutę zamiast 230**.

### Już działało, zgłoszone ponownie

- Omdlenie działa na boty od 2.0.39. Zgłoszenie (cyfrowy_mat) przyszło
  kilka godzin przed tym wydaniem.
- Boty wyjmują rzeczy z magazynu od 2.0.39 (akhigubernator): książki, do
  których dorosły, i materiały, na które jest popyt.

### Drobne

Log `used attack skill` podawał dla każdego bota ten sam `target_vid`
(4294758416), bo w miejsce numeru trafiał adres. Teraz podaje prawdziwy
numer celu. Marsz w lochu zapisuje odległość do drzwi, czas marszu i czas
od przydziału.

## 2.0.40 — 2026-09-14

Serwer. Boty biją się wreszcie w pojedynkach, na które się zgodziły, nie marnują
umiejętności pod marmurem polimorfii, a łucznik wabi już przy trzech osobach w
drużynie zamiast pięciu. Do tego wydanie pilnuje teraz własnego manifestu — bo
2.0.38 nie dotarło do nikogo i nikt się o tym nie dowiedział. Klient bez zmian
(zostaje 2.0.5).

### Pojedynek, w którym padają ciosy

Bot przyjmował wyzwanie i wracał do swoich potworów. Przyczyny były dwie i obie
po naszej stronie.

Pierwsza: wybór przeciwnika siedział w sekcji celu, a nad nią kilkanaście
przebiegów kończy turę bota — zakupy, kowal, wizyta w mieście. Bot, który
właśnie zgodził się na pojedynek, prawie zawsze jest w środku któregoś z nich i
do wyboru przeciwnika nigdy nie docierał. Zmierzone sześć sekund po zgodzie, na
tej samej mapie, **siedemdziesiąt jednostek od siebie**: jeden szedł do
handlarza bronią, drugi szukał potwora, obaj na pełnym życiu.

Druga wyszła dopiero po naprawieniu pierwszej. Funkcja, która zamienia zamach w
obrażenia, zaczynała od odrzucenia wszystkiego, co nie jest potworem ani
metinem. Boty schodziły się więc na dwadzieścia trzy jednostki, odgrywały całe
kombo — bo wysyłanie animacji to osobna rzecz — i nie zadawały sobie nic.
**Jedenastu pojedynkujących się, ani jednego zadrapania.**

Po poprawce, na ustabilizowanym świecie: **4440 punktów życia wymienionych
między pięcioma z dwudziestu trzech walczących**, wobec zera zawsze i bez
wyjątku wcześniej. Pojedynek ma teraz własną linię w logu
(`PLAYERBOT_PVP: fighting the duel`), bo bez niej nie dało się tego sprawdzić —
sąsiednia linia o wyborze celu jest poziomu, którego ten rdzeń nie zapisuje.

Bot w pojedynku nadal nie pije mikstur życia; to działało już wcześniej.

### Pod marmurem nie rzuca się umiejętności

Silnik odmawia rzucenia czegokolwiek postaci pod marmurem polimorfii — w pięciu
osobnych miejscach. Bot o tym nie wiedział i przepalał całą rotację na
odmowach, zamiast bić, a marmur zakłada się właśnie po to, żeby zwykły atak
bolał bardziej.

### Wabienie: próg trzech osób i dalsza paczka

Łucznik wymagał pięcioosobowej drużyny. Zmierzone na własnym świecie: drużyny
chodzą po jednej–dwóch osobach (18 botów w 15 drużynach), pięcioosobowe nie
powstają praktycznie nigdy, a ostatni kurs w logach miał dwa dni. Próg to teraz
trzy — tyle, ile wynosi sens pociągnięcia paczki na czekających.

Po obniżeniu progu kursy ruszyły i od razu pokazały następną przeszkodę: ze 120
widzianych potworów 114 odrzucano „po zasięgu". Licznik nie rozróżniał jednak
„za blisko" od „za daleko", a to dwie przeciwne poprawki, więc najpierw został
rozbity na pół. Odpowiedź okazała się jednostronna — `za blisko=0`,
`za daleko=33` i `za daleko=62` — czyli sufit trzech tysięcy jednostek był tym,
co wyłączało rolę. Okno sięga teraz czterech tysięcy, z zapasem na drogę
powrotną wewnątrz budżetu kursu.

Uczciwie: samo poszerzenie okna nie zdążyło jeszcze doczekać się kursu na
żywym świecie. Rozbity licznik jest w logu i następny kurs to rozstrzygnie.

### Zaproszenie do drużyny: log powie, co odrzuciło

Bot **nadal nie dołącza** do drużyny gracza i nie umiem powiedzieć dlaczego —
w stu dwóch plikach logów nie ma ani jednego przyjęcia, odkąd ta funkcja
istnieje. Żadna z czterech bramek silnika tego nie tłumaczy: królestwo się
zgadza, różnica poziomów dopuszcza 2153 z 2500 botów, a we własnych drużynach
siedzi ledwie czternaście procent.

Każda odmowa kończy się komunikatem na czacie zapraszającego i cichym
powrotem, więc log milczał. Teraz nazwie przyczynę numerem. Jedno zaproszenie
wystarczy, żeby zamknąć temat w następnym wydaniu.

### Dlaczego 2.0.38 nie dotarło do nikogo

Wydanie 2.0.38 wyszło na GitHuba i nie zainstalował go ani jeden gracz.
Manifest — jedyny plik, z którego launcher czyta, co pobrać — został zmieniony
lokalnie i nigdy wysłany, a następne wydanie go nadpisało. Historia idzie więc
2.0.37 → 2.0.39, bez niczego pomiędzy.

Awaria była cicha po obu stronach: u nas wydanie wyglądało na opublikowane, a u
gracza launcher uczciwie porównywał 2.0.37 z 2.0.37 i odpowiadał „masz
najnowszą wersję". W logu zgłaszającego stoi to siedem razy z rzędu, bez
jednego pobrania i bez jednego błędu.

Od tego wydania osobna bramka odmawia publikacji, gdy manifest wciąż wskazuje
poprzednią wersję. Sprawdzona na prawdziwym przypadku: przepuszcza 2.0.39 i
odrzuca 2.0.38.

### Drobne

Pakiet diagnostyczny niesie teraz konfigurację launchera (przepuszczoną przez
ten sam filtr co logi). Bez niej nie da się odczytać, z którego manifestu czyta
launcher gracza, a linie 1.x i mt2009 mają osobne — przez co dwie zupełnie
różne przyczyny wyglądają w logu identycznie.

Domyślny numer wersji w panelu zaawansowanym stał na 2.0.23 i szedł własnym
kanałem, którego nikt nie podbijał. Idzie teraz za wersją wydania.

## 2.0.39 — 2026-09-14

Serwer. Pojedynki, dwie nowe mapy do polowania, Wieża Demonów, koń militarny,
ostatni etap Biologa, górnictwo z wytapianiem ebonitu oraz wrogość między
królestwami do włączenia. Klient bez zmian (zostaje 2.0.5).

### Pojedynki

Gracz może wyzwać bota i bot **zawsze się zgodzi** — po trzech sekundach, tak
jak prosiłeś. Dotąd wyzwanie bota nie robiło nic: `CPVPManager::Insert` to zgoda
obustronna, a bot nie miał klienta, który odpisze tym samym. Teraz silnik
zapisuje wyzwanie, a tick bota odpowiada tą samą drogą, którą poszedłby klient.

Boty wyzywają też siebie nawzajem — rzadko i tylko sensownie: sześć na tysiąc
przy jednym losowaniu na minutę, wyłącznie blisko siebie, w promieniu pięciu
poziomów i przy pełnym życiu obu stron.

**W pojedynku nie piją potek.** Silnikowego `IsFighting` nie dało się do tego
użyć, bo na jednej linii siedzi pod `ENABLE_NEWSTUFF`, a na drugiej nie istnieje
w ogóle — więc bot pamięta swój pojedynek sam.

**Czego to jeszcze nie robi, i mówię wprost:** bot przyjmuje wyzwanie i na tym
koniec — **nie atakuje przeciwnika**. Cała ścieżka wybierania celu była pisana o
potworach i metinach i odrzuca postacie graczy, więc pojedynek kończy się
uściskiem dłoni. Zmierzone na żywym świecie: dwadzieścia zgód na pojedynek,
zero walk. Walka w pojedynkach to osobna robota i wchodzi w następnym wydaniu —
dotyczy to także wrogości między królestwami niżej.

I jedna rzecz, która nie jest naszym błędem, a wygląda jak nasz: **jadąc na
koniu nie zadaje się obrażeń w PvP**. Silnikowe `CanAttack` odrzuca atak
jeźdźca, którego koń ma grade poniżej 2, a grade to `(poziom konia − 1) / 10 + 1`
— czyli każdy koń do dziesiątego poziomu włącznie. Dotyczy gracza tak samo jak
bota, więc jeśli obaj jesteście na koniach, nie trafi żaden z was. Zsiądź.

### Magazyn u Dozorcy działa w obie strony

Magazyn był składem bez drzwi — istniało wyłącznie wkładanie, a komentarz w
kodzie mówił to wprost: „rzeczy włożone nigdy nie są wyjmowane". Zgłosił to
**akhigubernator** na Discordzie i miał rację: strona w końcu zapełni się do
końca i nigdy nie zostanie zwolniona, rzecz bezużyteczna godzinę temu może mieć
popyt teraz, a bot potrafi siedzieć na księgach, w które sam już dorósł.

Bot wyjmuje teraz dokładnie to, czego trzy reguły wkładania **przestały**
uznawać za nadwyżkę: księgę, która nie jest już zbędna (umiejętność doszła do
Mistrza albo bot wreszcie ma grupę umiejętności), oraz materiał, którego sam
potrzebuje przy kowadle lub na który jest popyt i bot może go wystawić.
Wyjmowanie biegnie **po** wkładaniu, na tej samej otwartej skrzyni — najpierw
zwalniają się komórki plecaka, dopiero potem bot zabiera to, po co przyszedł.
Ograniczone na wizytę i wielkością plecaka, bo wyjęcie pełnego plecaka
skończyłoby się odwiezieniem wszystkiego z powrotem następnym razem.

**I druga rzecz, znaleziona przy okazji: opłacona strona nigdy się nie
zapisywała.** `SetSafeboxSize` przyjmuje **liczbę stron** i odrzuca wszystko od
trzech wzwyż, a my podawaliśmy `SAFEBOX_PAGE_SIZE` — czyli 45 komórek. Funkcja
wychodziła bez zrobienia czegokolwiek, za każdym razem. To jest prawdziwa
przyczyna gałęzi „strona magazynu jeszcze nie gotowa", a nie wolna odpowiedź
bazy. Teraz i pakiet do bazy, i rozmiar w pamięci mówią **dwie strony** — tyle,
ile pokazuje okno Dozorcy.

### Kilof, żyły rud i wytapianie ebonitu

Kopanie siedzi w silniku od zawsze — `mining.cpp` ma tabelę rud, szanse i event
uderzenia — ale **ten świat nie stawiał ani jednej żyły**. Sprawdziłem wszystkie
109 map: zero żył rudy (20047–20059) i zero alchemików w jakimkolwiek pliku
odrodzeń. Brakowało więc nie AI, tylko świata.

Dwadzieścia żył stoi teraz na trzech mapach frontieru — Dolina Orków, pustynia i
Góra Sohan — a każda na rzeczywistym punkcie odrodzenia danej mapy, nie na
zgadniętej współrzędnej. Żyła kasuje się sama po 7–15 minutach (tak działa
silnik i tak ma być), więc rdzeń dostawia brakujące raz na minutę.

Bot od 30 poziomu kupuje kilof za 80 000, zakłada go w slot broni, dochodzi do
żyły i tłucze. Sklep `pick_shop` stoi na trzech mapach, na które żaden bot nie
chodzi, więc kilof powstaje za cenę sklepową — tak samo jak karta wędkarska.
Ruda spada na ziemię i jest podnoszona zwykłą drogą. Sto sztuk rudy to jedno
wytopienie: **Ruda Ebonitu → Ebonit**, i analogicznie dla wszystkich trzynastu
rud. Alchemika w tym świecie nie ma nigdzie, więc wytapianie liczy się tam,
gdzie bot stoi.

Ruda surowa i wytopiona nigdy nie idą do handlarza — trafiają na stragany, bo o
handel rudą prosiłeś.

### Wrogość między królestwami (domyślnie wyłączona)

Nowy suwak w panelu: **Wrogość między królestwami**, na starcie 0% — czyli świat
zachowuje się dokładnie tak jak dotąd, dopóki sam go nie podniesiesz.

Powyżej zera podany procent botów wyzywa boty innych królestw spotkane na
**wspólnym terenie**: w Dolinie, na pustyni, na Sohanie, w lochach. To, które
boty są agresywne, jest przypisane na stałe do postaci, a nie losowane co chwilę
— więc zaczepiają wciąż te same, a reszta spokojnie poluje. Nigdy w wiosce,
nigdy na graczu i nigdy na bocie rannym albo już walczącym.

Oparłem to na pojedynku, a nie na wpuszczeniu postaci graczy do kolektora celów.
Pojedynek kończy się sam, gdy ktoś padnie, nie da się nim przeciągnąć bota przez
pół mapy, a reguła „w pojedynku nie piją potek" już działa — to odpowiedź na
„bez pętli" i „ten, który ginie, odpuszcza i bierze inny spot".

### Łowienie od 30 poziomu — domknięte

W tym samym wydaniu zjechały wcześniej dwie bramki z pięćdziesiątki na
trzydziestkę: silnikowa `CHARACTER::fishing()` i bramka AI. Trzeciej nie było
widać — **sama wędka ma limit poziomu 50**, więc bot na trzydziestce i tak nie
mógł jej założyć ani zostać wędkarzem. Limit zszedł na 30 dla wszystkich
dwudziestu wędek. Poprawka siedzi w bootstrapie bazy, więc obejmuje i ten świat,
i świeże instalacje.

Gdy ryby wreszcie zaczęły brać, wyszła druga rzecz, która spała od zawsze:
**każdy połów szedł do kosza**. Tabela `log.fish_log` miała osiem kolumn wzięte z
drugiego silnika, a ten wpisuje sześć — więc każda złowiona ryba kończyła się
błędem „Column count doesn't match value count" w `syserr` (364 linie w pierwszych
dziesięciu minutach) i nie zapisywała się nigdzie. Nikt tego nie widział, bo na
tej linii nikt nigdy nie łowił. Tabela ma teraz kształt, który ten silnik
faktycznie zapisuje; stara jest przebudowywana przy starcie, ale tylko wtedy, gdy
ma ten zły kształt — historii, jeśli kiedyś powstanie, nic nie rusza.

### Ulepszanie wędki u Rybaka

Silnik ma ten mechanizm w dwóch połowach, a boty robiły tylko jedną. Przy każdym
połowie wędka losuje punkt do socketu (dla +0 jeden na pięć), aż do swojego
pułapu — i dopiero wtedy wolno ją ulepszyć. Nikt o to nigdy nie prosił, bo
ulepszenie wywołuje wyłącznie komenda GM i dialog questa, którego bot nie
otworzy. Zmierzone przed poprawką: **62 z 64 wędek w świecie stały na równo
dziesięciu punktach — pełne, na zawsze**.

Liczby są wędki, nie moje: **+0 → +1 to 100% i porażka nie istnieje**, dalej
robi się ryzykownie (88%, 77%, 66%, 55%), a nieudane ulepszenie zabiera stopień.
Po godzinie działania: 62 wędki na +1.

### Ognisko i pieczone ryby

Ognisko miało **dwa** błędy. Drewno kupowane jest u Rybaka po drodze nad wodę, a
warunek zakupu wymagał pięciu martwych ryb — które pojawiają się dopiero w
trakcie sesji. Pętla nie do domknięcia: 27 sesji skończyło się ścieżką
rozpalającą, dwa boty w całym świecie miały kiedykolwiek drewno, ogień nie
zapłonął ani razu.

Drugi błąd siedział w silniku i trzeba go było przeczytać, bo w logu nie było po
nim śladu: `ITEM_CAMPFIRE` sprawdza kafelek **sto jednostek przed postacią** i
odrzuca wodę. A wędkarz stoi obrócony twarzą do rzeki. Każde drewno leciało do
wody, a silnik tłumaczył się klientowi, którego bot nie ma. Teraz bot odwraca się
tyłem do wody przed zapaleniem.

Przy okazji: zakres pieczonych ryb kończył się na 27876, a rodzina sięga 27883 —
więc **siedem najlepszych szło do handlarza za grosze**, w tym karp dający
**+20 prędkości ruchu na 600 sekund** i złoty karaś z bonusem na 1800 sekund.

### Stajenny w drugiej wiosce Jinno

Noga marszu do Stajennego używała zwykłego marszu, a nie marszu miejskiego —
czyli **nigdy nie pytała o osiągalność** i nie przesuwała celu na grunt
połączony z botem. Stajenny w Bakrze stoi na kawałku terenu odciętym od placu:
**600 odmów trasy, wszystkie na mapie 43**, podczas gdy stajnie pozostałych
królestw obsłużyły 193 wizyty i przyjęły 82 medale. Po poprawce: **zero**.

### Omdlenie działa na boty

Zgłosił **cyfrowy_mat**: „omdlenie szarżą nie działa na botach z innego
królestwa, bot po sekundzie już biegnie dalej". Miał rację i to był nasz błąd,
nie silnika — silnik nakłada omdlenie botowi dokładnie tak jak graczowi, przez
tę samą odporność, i nigdzie nie pyta, czy to bot. To **nasza pętla nigdy nie
sprawdzała**, czy bot jest ogłuszony: szedł dalej, bił dalej i planował dalej.
Jedyne takie sprawdzenie w całym kodzie botów pilnowało otwierania sklepu
offline.

Teraz brama stoi na samej górze pętli, obok sprawdzenia śmierci, więc obejmuje
ruch, walkę i wszystkie podsystemy naraz.

### Wabienie tam, gdzie są watahy

Łucznik z drużyny planował wabienie **gdziekolwiek** poza strefą bezpieczną — nie
było żadnej reguły mapy. Zmierzone w Yongan: drużyna sześciu, pięciu odbiorców
gotowych i „brak watahy" sekundę później, bo w pierwszej wiosce nie ma czego
przyciągać. Teraz tylko mapy frontieru, a maksimum grup zeszło z czterech na
trzy, zgodnie z regułą „aggro 1–3 boty z drużyny".

### Las, Czerwony Las i Wieża Demonów

Trzy mapy, które istniały w plikach, ale hostował je rdzeń bez botów — czyli
żaden bot nie mógł na nie wejść. Przeniesione tam, gdzie boty żyją:

- **Las (67)**: Duchy Drzewa, Pniaka, Driady i Złe Drzewa, poziomy 65–71,
  527 punktów odrodzenia, osiem hubów.
- **Czerwony Las (68)**: ich czerwone odpowiedniki, 74–82, 693 punkty, osiem hubów.
- **Wieża Demonów (66)**: Demony 57–60.

Huby nie są zgadnięte: każdy stoi na rzeczywistym punkcie odrodzenia z regenu
mapy, w najgęstszej komórce, a pasmo poziomów to mediana potworów tej komórki.
W Wieży Demonów boty **nie łamią metinów** — tak jak prosiłeś, dungeon zostaje
na później.

### Ostatni etap Biologa

Pamiątka Po Demonie była w tabeli od 2.0.37, ale celowo pomijana: jej potwory
stoją wyłącznie w Wieży Demonów. Przenosiny mapy odblokowały ją same z siebie i
panel liczy znów dziewięć etapów.

### Koń militarny

Medale prowadzą konia do **dwudziestego** poziomu i tam się zatrzymują.
Dwudziesty pierwszy pochodzi z **próby w Wieży Demonów** — pięćdziesiąt zabitych
demonów, bez limitu czasowego — dokładnie tak, jak koń bojowy jest próbą na
pustyni. Bot na próbie poluje tam, gdzie próba, cokolwiek mówiłby jego poziom.

### Drobne

Eliksiry Słońca (39037–39039) dołączyły do eliksirów doświadczenia. Wcześniej
bot traktował je jak zwykły łup, choć Eliksiry Księżyca znał od dawna.

---

Sprawdzone na żywym świecie, po podniesieniu stawek do 100× i doprowadzeniu
populacji do 340 botów powyżej 34 poziomu (najwyższy 90):

- **górnictwo** — dwadzieścia żył postawionych i utrzymywanych, `spawned=20
  refused=0`, ruda w torbach, pierwsze wytopienie (Ruda Miedzi → Miedź);
- **ulepszanie wędki** — 62 wędki przeszły na +1 w pierwszej godzinie;
- **łowienie od 30** — wędki w rękach botów na poziomach 30, 31, 33, 35 i 36,
  czego przy limicie pięćdziesiątki nie dało się w ogóle zrobić;
- **magazyn w obie strony** — pierwsze wyjęcia po kilku minutach, po godzinie 66;
- **Stajenny** — z 600 odmów tras na mapie 43 zrobiło się zero;
- **pojedynki** — zgody działają (dwadzieścia), walki jeszcze nie ma;
- **wrogość królestw** — zero zaczepek, bo suwak stoi na zerze, tak jak ma stać.

Czego nie dało się potwierdzić w grze: **ognisko** (poprawka wdrożona, ale w
oknie testu żadna sesja wędkarska nie doszła do końca właściwą ścieżką) i
**omdlenie** (wymaga gracza, który trafi szarżą). Tick przy 350 botach: 1,7–2,8 s
z 60 000, watchdog zero.

## 2.0.38 — 2026-09-14

Serwer. Boty przyjmują zaproszenie do drużyny i biegną z graczem, łowią od 30
poziomu, zakładają marmury polimorfii na bossów, a dropperzy przestają zbierać
doświadczenie na poziomie swojego łowiska. Klient bez zmian (zostaje 2.0.5).

### Zaproszenie do drużyny wreszcie dociera do bota

Do tej pory zaproszenie gracza do bota **nie robiło dosłownie nic** i nie
zostawiało po sobie śladu w żadnym logu. Przyczyna: `CHARACTER::PartyInvite`
kończy się wysłaniem pakietu na deskryptor zapraszanego. Bot ma deskryptor, ale
nie ma za nim klienta — pakiet szedł w próżnię, nikt nie klikał „Akceptuj", a po
dziesięciu sekundach zaproszenie cicho wygasało.

Teraz silnik odkłada takie zaproszenie w osobnym dzienniku, a tick bota
odpowiada na nie tą samą metodą, którą wywołałby klient. Bot **nigdy nie
odmawia** — wszystkie warunki, które mogą odrzucić, są silnikowe i o nich warto
wiedzieć: to samo królestwo, różnica do trzydziestu poziomów i wolne miejsce w
drużynie ośmioosobowej.

Do tego dwie rzeczy, bez których to nie miałoby sensu. Drużyna prowadzona przez
**gracza** jest wyjęta ze wszystkich reguł rotacji botów — kohorty, wygasania po
5–15 minutach i promienia maruderów — bo inaczej bot dołączyłby i wyszedł w
ciągu minuty. I biegnie za graczem: gdy oddali się o ponad 1500 jednostek,
rusza za nim, z koniem, zanim pas wędrowania zdąży go wysłać na własne łowisko.

Czego jeszcze nie ma: bot-lider nie zaprasza kolejnych botów. Każdego zapraszasz
sam.

### Łowienie od 30 poziomu

Wymóg pięćdziesiątki siedział w trzech miejscach naraz — w samym silniku
(`CHARACTER::fishing()`) i w dwóch bramkach botów, żeby nikt nie szedł nad wodę,
która i tak by go odprawiła. Wszystkie trzy mówią teraz trzydzieści. Reszta
warunków bez zmian: mapa pierwszej wioski, przepustka i przynęta.

### Marmury polimorfii na bossów

Dotąd **żaden bot nigdy nie użył marmuru** — były wyłącznie towarem na stragan.
Teraz bot zakłada marmur, gdy bije bossa, który ma jeszcze co najmniej 90%
życia. Nie robi tego w siodle ani pod inną przemianą, bo silnik i tak by
odmówił. Zakaz używania umiejętności pod marmurem jest silnikowy i nic nie
trzeba było dodawać — dlatego marmur idzie tylko na bossa, gdzie rotacja i tak
nie decyduje o walce.

### Dropperzy zatrzymują poziom

Każdy drop w tym silniku blednie wraz z różnicą poziomów, więc farmer, który się
dalej rozwija, wychodzi z własnej tabeli: medal to grupa „kill", a przy
piętnastu poziomach nad potworem jest wart tyle co nic. Dropper dochodzi teraz
do poziomu swojego łowiska i **zatrzymuje doświadczenie na stałe** — medale 33,
M2 36, M3 30, metiny 40 — po czym robi w kółko to, po co jest: dropi i sprzedaje.

### Launcher mówi po angielsku, że mówi po angielsku

Przełącznik języka był, ale napis „JEZYK: POLSKI" nie mówi anglojęzycznemu, w co
kliknąć. Przycisk pokazuje teraz oba języki naraz.

---

Sprawdzone przed wydaniem: składnia nakładki bez błędów na obu silnikach, obraz
mt2009 przebudowany, rdzeń wstał z 2500 tożsamościami bez nowych błędów, a
binarka niesie wszystkie nowe linie logu. Tick 2,8 s z 60 przy 318 botach,
watchdog zero. Czego **nie** udało się zobaczyć na żywo: żadnej z czterech
nowych rzeczy w działaniu — w świecie testowym jest ośmiu botów powyżej
trzydziestki, jedyny dropper wśród nich ma 31 poziom przy progu 33, a
zaproszenia do drużyny nie ma kto wysłać bez klienta.

## 2.0.37 — 2026-09-13

Serwer i panel. Łańcuch Biologa nie kończy się już na Zębie Orka — dochodzi
etap 40 z nagrodami jeden do jednego z tym, co dostaje gracz. Do tego launcher
mruga na czerwono, kiedy jest nowa wersja.

### Biolog idzie dalej: Księga Klątw

Quest `collect_quest_lv30` w swoim ostatnim stanie sam uruchamia `lv40`, a ten
uruchamia `lv50` — u gracza łańcuch leci dalej, a boty kończyły na siódmym
etapie i stały na „7/7" do końca świata.

Dochodzi **Księga Klątw**: piętnaście okazów po 60% przyjęcia, potem druga
połowa — Świątynny Kamień Duszy, jeden na pięćset zabić — a na koniec nagroda
questa: **+5 szybkości ataku na stałe** i szkatuła.

Gdzie to jest, zmierzone na plikach tego świata, nie z wiki: Księgę noszą
Dręczyciele (706 i 756, poziom 49) z centralnej wyspy Doliny Orków, po 68
punktów odrodzenia każdy, a klucz leci z tego samego hooka na 701–707 w Dolinie
i 731–737 w Świątyni. Bot poziomu 40 sięga potwora 49 — limit to piętnaście
poziomów — więc etap jest wykonalny od pierwszej chwili, w której się otwiera.

### Nagroda, która kasowała poprzednią nagrodę

Nagrody miały być jeden do jednego z graczem i przy okazji wyszło, że dotąd nie
były. Silnikowe `affect.add_collect` robi trzy rzeczy: znajduje istniejący efekt
**tego samego typu punktu**, dodaje do niego nową wartość i zapisuje z flagą
`IsCube`. Boty wołały `AddAffect` wprost, bez sumowania i bez tej flagi.

To nie jest drobiazg, bo przy `IsCube = false` silnik szuka efektu **po samym
typie**, ignorując punkt. Nagroda za Księgę Klątw nadpisałaby wtedy prędkość
ruchu z Zęba Orka zamiast stanąć obok niej. Teraz wypłata idzie dokładnie tą
samą drogą co u gracza.

### Etap, którego na tym świecie nie da się skończyć

Kolejny etap łańcucha — **Pamiątka Po Demonie** — jest w tabeli, ale boty go nie
wezmą, i to jest celowe. Zarówno okaz, jak i klucz pochodzą wyłącznie od
potworów 1001–1004, a te stoją na **jednej** mapie w całym tym świecie: w Wieży
Demona (indeks 66), którą hostuje rdzeń `game2`, podczas gdy wszystkie boty żyją
na `game1`. Mapy, której rdzeń nie hostuje, bot nie osiągnie nigdy.

Gdyby etap po prostu dopisać, każdy bot po pięćdziesiątce utknąłby na „Pamiątka
Po Demonie 0/15" — dokładnie tak, jak kiedyś cały świat tkwił na „Korzeń Gango
0/5". Dlatego etap, którego potwór nie stoi na żadnej hostowanej mapie, jest
pomijany przez wszystkie reguły wyboru, a panel liczy osiem etapów zamiast
dziewięciu, żeby nikt nie oglądał wiecznego „8/9". Wiersz zacznie działać sam,
jeśli ta mapa kiedyś trafi na rdzeń z botami.

### Launcher mruga, kiedy jest nowa wersja

Stopka z wersjami zapala się na czerwono i mruga, kiedy serwer albo klient mają
nowsze wydanie, z napisem wprost mówiącym, czego dotyczy i że trzeba kliknąć
ZAINSTALUJ AKTUALIZACJE.

## 2.0.36 — 2026-09-13

Serwer. Rynek przestaje zgadywać ceny — dostaje cenniki Iwakury. Do tego
poprawka aktualizatora na Linuksie i VPS, przez którą serwer po aktualizacji
podawał starą wersję i instalował to samo wydanie w kółko.

### Aktualizacja mówiła, że nic się nie zmieniło — i miała rację

„Drugi raz robię aktualizację z 2.0.34 do 2.0.35 i drugi raz komunikat:
*the server is now running version 2.0.34*" (Mkls).

Paczka aktualizacji nie zawierała pliku `VERSION` w korzeniu instalacji.
Zawierała go — ale pod `linux-port/VERSION`, czyli tam, gdzie **nikt go nie
czyta**. Wzięło się to z mapowania ścieżek: drzewo 2.x wdraża się pod nazwą
`linux-port`, a reguła przenosiła cały katalog razem z plikiem wersji.

To nie był tylko mylący komunikat. `tools/update.sh` czyta `<korzeń>/VERSION`
dwa razy: żeby powiedzieć, co jest zainstalowane, i żeby **zdecydować, czy w
ogóle jest co instalować**. Skoro numer nigdy się nie zmieniał, porównanie
z manifestem nigdy nie trafiało — więc każde uruchomienie pobierało i
rozpakowywało to samo wydanie od nowa.

Naprawione w trzech miejscach naraz: plik wersji trafia do korzenia,
dokumentacja wydania nosi pełne mapowanie (brakowało w niej dwóch wierszy,
choć narzędzie `New-M2DeployTree.ps1` miało je od zawsze), a pakowarka
**odmawia zbudowania** paczki serwera bez `VERSION` w korzeniu. Przy okazji
wraca do paczek `PACZKA_INFO.txt`, który ginął z tego samego powodu.

Po zainstalowaniu 2.0.36 numer w korzeniu wreszcie się przesunie i kółko się
zatrzyma.

### Bronie i zbroje: 147 rodzin, cena za każdy plus

Do tej pory broń i zbroja powyżej +6 miały trzy wymyślone ceny: 150 000 za +7,
400 000 za +8, 900 000 za +9. Wymyślone, bo tabele gry nie zawierają ceny
ulepszonego przedmiotu i trzeba ją było kiedyś zgadnąć. Skutek: **Zatruty Miecz
+9 i zwykły Miecz +9 stały na straganie za te same 900 000**.

Teraz każda rodzina ma własną cenę na każdym poziomie ulepszenia, wprost z jego
arkusza: Miecz +9 to 120 000, Zatruty Miecz +9 to 13 000 000. Rodzin jest 147 —
miecze, sztylety, łuki, bronie dwuręczne, dzwony, wachlarze i zbroje wszystkich
czterech klas.

### Kamienie duszy w sockecie podnoszą cenę przedmiotu

Też z jego arkusza, w dwóch krokach: najpierw ile kamieni siedzi w przedmiocie
(jeden ×1.2, dwa ×1.3, trzy ×1.5), potem które konkretnie — Kamień Duszy Potwora
+4 mnoży przez 1.8, Śmierci +4 przez 1.7, i tak dalej. Pęknięty kamień nie liczy
się wcale, dokładnie jak u niego.

### Marmury, opaski, zioła, kamienie duszy, skrzynki

- **Marmury polimorfii** wyceniane po potworze siedzącym w sockecie: trzynaście
  nazwanych wyjątków (od 40 000 za Wojownika z Toporem po 75 000 za Małego
  Trującego Pająka), reszta z pasma 15 000–35 000, losowana raz na marmur.
- **Opaski Zapomnienia** po umiejętności — 44 pozycje. Siedem z nich Iwakura
  oznaczył „do sprzedaży u handlarki": te nie zajmują już miejsca na straganie.
- **Kamienie duszy** po rodzaju i stopniu: osiemnaście wyjątków (Potwora +4 za
  350 000, Śmierci +4 za 250 000), reszta po cenie swojego stopnia.
- **Zioła** (osiem), **Szkatuła Blasku Księżyca** 35 000 i **Medal Konny**
  250 000.

### Jak to zrobione

Ceny nie są przepisane ręcznie. Generator czyta jego pliki i wiąże **każdą**
nazwę z vnumem tego świata, pytając `item_proto` i `mob_proto`; jeśli choć
jednej nazwy nie da się dopasować, **odmawia zapisania tabeli**. Dzięki temu
nowa pozycja w arkuszu zatrzyma budowę zamiast po cichu zostawić starą cenę.
Dziesięć nazw, które gra skraca („Zbr. Płyt." wobec jego „Zbroja Płytowa"),
ma jawną tabelę aliasów — każdy sprawdzony jako jedyny kandydat.

Wszystko skaluje się mnożnikiem dropu yang tak, jak napisał na górze obu
arkuszy: cena bazowa razy stawka przez sto. Na serwerze ze stawką 100% obowiązuje
cena z tabeli, przy 500% jest pięć razy wyższa.

Numer wersji cennika idzie na 2, więc stragany stojące na starych cenach
przeceniają się na najbliższej wizycie serwisowej, zamiast trzymać je przez całe
osiem godzin stoiska.

## 2.0.35 — 2026-09-13

Serwer. Pięć zgłoszeń z Discorda z jednego wieczoru, każde z inną przyczyną —
i dwa z nich okazały się tym samym: bot z Shinsoo albo Jinno żył w świecie
opisanym współrzędnymi Chunjo.

### Konie w Shinsoo i Jinno — każde królestwo ma własny Loch Małp

„Tylko boty z chunjo levelują konia" (RetroGracz38), „bo z innych nie wchodzą do
lochu dlatego" (NerrVoVy), „jest problem z robieniem konia w shinsoo i jinno,
przez to jak ktoś pisał, że nie mają ustawionych kordów do lochu w swoim m2"
(Kiciamol). Sprawdzone na naszym własnym świecie przed poprawką: **Shinsoo 500
postaci i ani jednego konia, Jinno 500 i ani jednego**, Chunjo jedyne z koniem.

Każde królestwo ma swój własny łatwy Loch Małp — to trzy osobne mapy (5, 25
i 45), o tej samej geometrii, w trzech różnych miejscach świata. Nakładka znała
tylko mapę Chunjo. Bot z Shinsoo przechodził własną bramą i lądował na mapie,
której kod nie rozpoznawał jako lochu: bez komór, bez punktów polowania, bez
medalu. Bot po 33. poziomie był z kolei wysyłany do trudnego lochu, którego jego
rdzeń w ogóle nie hostuje, więc warp był odrzucany za każdym razem.

Poziom nie wskazuje już mapy, tylko **pasmo**; mapę dobiera się do bota — jego
królestwo i to, co ten rdzeń faktycznie hostuje. Królestwo bez dostępu do
trudniejszych lochów pracuje we własnym, dopóki to się jeszcze opłaca (medal to
losowanie z grupy zabójstw, a mnożnik za różnicę poziomów wygasa piętnaście
poziomów nad potworem).

### Każde królestwo wchodzi na mapy wspólne własnym wejściem

„Wszystkie boty po wejściu do doliny, niezależnie od królestwa z którego są,
wchodzą w miejscu wejścia żółtych. To samo się dzieje z pustynią" (SIZOWSKI,
potwierdzone przez NerrVoVy).

Dolina Orków, Pustynia Yongbi i Góra Sohan mają po trzy wejścia i po trzy bramy
— po jednym na królestwo. Tabela z tymi punktami istniała w kodzie od dawna
i była poprawna; podróż po prostu jej nie pytała i brała stałą Chunjo. Teraz
pyta. Bramy powrotne dobrano tak samo: bot wychodzi przez NPC-a stojącego obok
**własnego** wejścia, a nie przez bramę żółtych na drugim końcu mapy.

Punkty pochodzą z plików samej mapy (`Town.txt` — ogólny punkt i po jednej parze
na królestwo, oraz `npc.txt` dla bram) i wszystkie osiemnaście sprawdzono na
`server_attr`: każdy stoi na gruncie, po którym da się chodzić. Wiersze Chunjo
odtwarzają co do jednostki stałe używane do tej pory, co jest dowodem, że
pozostałe sześć jest odczytane tak samo. Test jednostkowy tego pilnuje.

### Stragany offline: wracają trzy mechanizmy (mt2009)

AkhiGubernator przeczytał binarkę i pokazał, że cała klasyczna obsługa straganu
w `ManagePlayerBotShopLifetime` jest na tej linii silnika **nieosiągalna
z konstrukcji** (dwa wyczerpujące wczesne `return`), więc kompilator usuwa ją od
`-O1` wzwyż. Diagnoza była w punkt. Padły przez to trzy rzeczy:

- **„Wysoki popyt"** (Iwakura, 2.0.33) — pamięć szybkiej sprzedaży nigdy nie
  dostawała ani jednego wpisu, więc narzut za popyt zawsze wynosił zero.
- **`PLAYERBOT_STALL_SOLD`** w historii sprzętu — wpisu nie było wcale.
- **Ponowna ocena stojącego straganu** po ruszeniu suwaka TRADE.

Na tej linii towar na ladzie należy do encji sklepu, a nie do plecaka bota, więc
„jedno przejście po własnym plecaku" nie może niczego zauważyć. Sprzedaż
zapisuje teraz natywny menedżer w chwili, w której ona następuje — jest jedyną
stroną, która o niej wie — a bot odbiera ten zapis na swoim własnym ticku.
Stragan, który wygasł, nie jest już odnawiany pod wagą, która go nie chce.

### Panel: „Szybkość biegu" i stempel zajęcia wiersza

„Bez względu na to czy postać jest zalogowana czy nie wywala błąd przy próbie
nadania szybkości" (Sammy Suricate) — komunikat brzmiał „Coś poszło nie tak
(w1x257t780)".

Przyczyny były **dwie**, jedna pod drugą, i obie są naprawione.

`w1x257t780` to nie status, tylko **stempel**, którym quest w grze zajmuje
wiersz kolejki, zanim zacznie pracę. Panel brał każdą wartość inną niż `pending`
za odpowiedź końcową i meldował ten stempel operatorowi jako błąd. Czeka teraz
na słowo z listy questa; wszystko inne znaczy „jeszcze pracuje". Dotyczy to
wszystkich komend panelu, nie tylko tej jednej.

Ale sam stempel nie zniknąłby stamtąd nigdy, i to jest druga połowa. Quest
zdejmuje własne efekty szybkości, zanim doda nowy — po nazwie, a nie hurtem,
żeby nie zabrać graczowi mikstur i błogosławieństw — i woła do tego
`affect.remove_collect`. **Tego wiązania na tej linii silnika nie ma**: jest
`remove_all_collect` i nic poza tym. Quest wywracał się więc na wywołaniu
nieistniejącej funkcji, nigdy nie dochodził do zapisu wyniku i zostawiał wiersz
ze swoim stemplem na zawsze. Widać to było w `syserr`:
`LUA_ERROR: attempt to call field 'remove_collect' (a nil value)`.

Brakujące wiązanie jest dopisane do silnika (tą samą drogą, którą dokładane są
inne). Implementacja różni się od r40250 celowo: tam każde wywołanie tworzy
własny efekt i usuwa się go po wartości, tutaj silnik **sumuje** efekty w jeden
na typ punktu, więc dopasowanie po wartości nigdy by nie trafiło.

Bez tej drugiej połowy sama poprawka panelu zamieniłaby tylko dziwny komunikat
na uczciwe „przekroczono czas oczekiwania" — i nic więcej.

### Dlaczego broń na 30 poziom nie jest ulepszana — najpierw pomiar

„Na 341 broni na serwerze praktycznie wszystkie są +0 (max +2)" (Iwakura). Broń
w plecaku może zostać pominięta przez cztery różne reguły, a z zewnątrz wyglądają
identycznie — dlatego tego zgłoszenia nie dało się ani potwierdzić, ani wyjaśnić
z żadnego logu. Zamiast zgadywać, rdzeń mówi teraz **która** reguła ją pominęła
(`PLAYERBOT_AI: level-30 weapon not refined ... reason=`), raz na minutę dla całej
populacji. Poprawka pójdzie po tym, co pokaże pierwszy log — nasz własny świat
nie ma ani jednej takiej broni, więc nie ma tu czego odtworzyć.

## 2.0.34 — 2026-09-13

Serwer **i klient** — pierwsza zmiana klienta od 2.0.3. Panel GM na F9 od
OskarPWA wchodzi na tę linię silnika, razem z osobnym oknem administracji botami
na F10.

### Panel GM (F9) — OskarPWA

Trzydzieści jeden komend serwera pod jednym oknem: wyszukiwanie gracza,
teleportacja po mapach i zapisane miejsca, tworzenie przedmiotów (także prosto
do skrytki i do sklepu z monetami), podgląd ekwipunku i zmiana statystyk,
umiejętności, yang i monet, spawn potworów, metinów i botów, nadawanie rang GM,
stawki serwera oraz suwaki AI botów.

Każde kliknięcie to zwykła komenda czatu, a serwer sprawdza poziom GM osobno
przy każdej z nich (tabela `cmd_info[]`) — samo okno niczego nie odblokowuje.
Stawki działają również na tej linii silnika: przycisk restartu zapisuje je do
tego samego kanału, który obsługuje `m2-rates`.

### Okno administracji botami (F10)

Ile botów żyje, ile jest w drużynie, ile stoi na straganie; lista botów
z poziomem, królestwem i pozycją; ostatnie akcje wybranego bota; nadanie mu
przedmiotu; tablica osiągnięć. Trzy rzeczy, których nasz rdzeń nie potrafił,
zostały dopisane:

- **Ostatnie akcje** — nic nie zapisywało historii pojedynczego bota, bo log
  rdzenia celowo skleja linię napisaną przez trzysta botów w jedną. Teraz
  migawka statusu, która i tak co dwie sekundy układa jedno zdanie na bota,
  odkłada je wtedy, gdy się **zmieni**. Bot stojący dwadzieścia minut przy
  straganie nie zapcha sobie historii jednym powtórzonym zdaniem.
- **Osiągnięcia** — „Pierwszy 30/60/90 poziom”. Jedyny uczciwy sposób, żeby
  wiedzieć, kto był pierwszy, to zobaczyć, jak ktoś przekracza próg: pierwsze
  spojrzenie na bota tylko zapisuje, gdzie on już jest, a nagroda przypada
  dopiero temu, kogo rdzeń widział niżej, a potem wyżej. Na świecie, którego
  boty są już po trzydziestce, tablica zostaje pusta, dopóki ktoś naprawdę nie
  awansuje — i tak ma być. Zwycięzcy przeżywają restart serwera.
- **Liczby na zakładce ogólnej** — liczone po żywych botach tego rdzenia. „Prowadzą stragan” pyta natywny rejestr sklepów offline o każdego bota z osobna, a nie o jego akcję: na tej linii silnika bot otwiera stoisko i **wraca na łowy**, bo stoiskiem opiekuje się encja — liczone po akcji pokazywałoby zero przy trzydziestu ośmiu stojących straganach. Rejestr trzyma też sklep gracza, więc pytanie idzie po właścicielu, nie hurtem.

### Panel nie zatrzyma już wczytywania klienta

Wersja autora budowała swoje okna bezwarunkowo. To dokładnie ten mechanizm,
przez który przed 1.33.3 pięciu graczom klient stawał na 100% z pustym ekranem —
wystarczył jeden wyjątek w oknie panelu, żeby przerwać budowę całego interfejsu.
Wszystkie trzy okna powstają teraz w osłonie: gdy któreś się nie zbuduje, powód
trafia do `syserr.txt`, gra wstaje bez niego, a próba otwarcia mówi o tym na
czacie zamiast wywalać błąd.

### Stragany przeceniają się po zmianie cennika (Iwakura)

„Pełno w m1 sklepów, gdzie Zwoje Błogosławieństwa nadal są po 9000”. Cena w kodzie była
już poprawna (150 000 od 2.0.32) — to wystawki były stare. Otwarty stragan przeceniał
**jeden przedmiot na godzinę**, więc stoisko z kilkunastoma liniami schodziło do nowych
cen kilkanaście godzin.

Cennik ma teraz numer wersji. Stragan, który został wyceniony według starszego,
przecenia się **na każdej wizycie serwisowej** (co 10–15 minut) zamiast raz na godzinę,
aż obejdzie cały swój blat — dopiero wtedy dostaje nową pieczątkę. Tempo wraca do
godziny, gdy nie ma czego nadrabiać. Żadnej masowej przeceny na raz: to ta sama
ścieżka żądań, co zwykle, tylko częściej pytana.

### Naprawy przy okazji

- **Aktualizacja z paczki na Linuksie i VPS działa ponownie.** Skrypt portujący
  silnik zatrzymywał się przy drugim przebiegu: jedna z edycji rozpoznawała swoją
  pracę po własnym wstawionym tekście, a późniejsza edycja ten tekst rozbijała —
  więc pierwsza dokładała nagłówek drugi raz, a druga traciła kotwicę i cały
  skrypt padał. Instalacje windowsowe tego nie widziały, bo dostają gotowe pliki.
- Dwie nasze wcześniejsze wstawki w plikach silnika miały zakończenia linii
  niezgodne z resztą pliku, przez co ten sam skrypt ich nie rozpoznawał.

## 2.0.33 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Domknięcie systemu cen
Iwakury: rynek reaguje teraz na to, co się sprzedaje, a co zalega.

### Znika „Polowanie” tam, gdzie polowania nie ma (Tieru)

Misja polowania (`levelup.quest`) leży na tej linii silnika w `quest/_unused` —
żaden hook zabicia nie strzela, więc licznik stoi na zerze dla każdego bota.
Rdzeń dawno to ignoruje, ale panele nadal o tym mówiły:

- W karcie postaci na mapie świata wiersz **„Polowanie: Brak danych”** pokazywał
  się przy każdym bocie. Wiersz, który potrafi powiedzieć wyłącznie „brak
  danych”, nie jest wierszem — pojawia się teraz tylko wtedy, gdy naprawdę jest
  co pokazać.
- Zakładka **„Polowanie”** w rankingach botów znika z obu paneli. W panelu
  zaawansowanym dawała sto pozycji z „Ukończone do Lv 0”.

Na silniku r40250, gdzie ta misja działa, jedno i drugie zostaje bez zmian.

### Podaż i popyt na straganach (Iwakura)

Ostatnia część jego dokumentów cenowych, opisana tam jako „mechanizmy rynkowe”.
Do tej pory bot miał tylko połowę: przecenę za zaleganie (sztywne −10% za każde
stoisko) i uśrednianie po cenach, za które faktycznie kupowano. Brakowało
reakcji na popyt — rzecz, która schodziła natychmiast, następnym razem trafiała
na stragan za tę samą kwotę.

- **Wysoki popyt**: jeśli przedmiot zniknie ze straganu w ciągu pięciu minut od
  wystawienia, rynek to zapamiętuje i kolejne sztuki idą drożej o 10–25%.
  Z każdą kolejną szybką sprzedażą cena rośnie dalej, do czterech kroków.
  Godzina bez szybkiej sprzedaży i zwyżka wygasa.
- **Niski popyt**: przecena za każde stoisko, z którego nic nie zeszło, to teraz
  10–25% (losowane przy wystawieniu) zamiast sztywnych 10%.

Jedno ograniczenie jest moje, nie jego: przecena za zaleganie ma **sufit 50%**.
Iwakura nie podał granicy, a cztery stoiska po 25% zabrałyby całą cenę — rabat
ma schodzić z marży, nie z przedmiotu.

Obie zmiany działają na cenie tego konkretnego straganu, już po ustaleniu ceny
rynkowej. To celowe: ogranicznik tempa zmian pozwala kotwicy rynku dryfować
o 5% na dziesięć minut i gdyby sygnał popytu szedł przez niego, albo zostałby
połknięty, albo pociągnąłby za sobą wszystkie inne stragany. Szybka sprzedaż
zapisywana jest per przedmiot, a dla ksiąg per umiejętność — Aura Miecza ma
własny popyt, nie wspólny ze wszystkimi księgami. W logu widać to jako
`PLAYERBOT_MARKET: fast sale`.

---

## 2.0.32 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. **Naprawa błędu z
2.0.31, który potrafił zablokować start serwera** — jeśli masz 2.0.31,
zaktualizuj się. Poza tym: przywracanie kopii świata znów działa, ceny ksiąg
i ulepszaczy idą według nowych tabel Iwakury, a launcher pokazuje, czy długa
operacja faktycznie postępuje.

### Serwer odmawiał startu: „port 13001 zajmuje com.docker.backend” (sizowski)

Błąd wprowadzony przeze mnie w 2.0.31 razem ze sprawdzaniem portów. Docker
publikuje zakres portów jako **jeden** wpis — `127.0.0.1:13000-13002->13000-13002/tcp`
— a launcher szukał w nim dosłownie `13001->`. Nie znajdował więc żadnego
z trzech kanałów gry, uznawał własny, działający serwer gracza za obcy program
i odmawiał startu, radząc „zamknij com.docker.backend”, czego nie da się zrobić.

Launcher rozumie teraz zakresy portów tak samo jak pojedyncze wpisy i adresy
IPv6. Dołożona jest też druga warstwa zabezpieczenia: jeśli port trzyma sam
Docker, a launcher nie rozpozna którego kontenera to sprawa, zgłasza
ostrzeżenie i **pozwala wystartować**, zamiast blokować. Nierozpoznany kontener
to luka w sprawdzeniu, nigdy powód, żeby nie uruchomić serwera. Przypadek
z zakresem portów ma teraz własny test.

### Launcher mówi, czy długa operacja jeszcze idzie

Podczas aktualizacji pasek postępu potrafił przez wiele minut pokazywać to samo
(„m2zip-db: Healthy”) bez żadnej wskazówki, czy to praca, czy zastój — a stało
za tym czekanie na bazę. Teraz obok nazwy etapu widać **jak długo ten etap
trwa**, a po czterech minutach bez zmian dochodzi ostrzeżenie „bez zmian —
sprawdź DIAGNOSTYKA” wraz z wpisem w logu. Launcher czyta też komunikaty
migracji bazy i pokazuje je wprost („migracja bazy: baza nie odpowiada”),
zamiast wyświetlać sprzed minuty nazwę ostatniego kontenera.

### Polskie znaki w panelu i na czacie (seban latino)

Poprawka od latino. Kolumna `hint` w `log.log` jest zadeklarowana jako big5,
podczas gdy silnik zapisuje do niej CP1250, więc sterownik bazy zwracał krzaki
dla wszystkiego poza ASCII („Skórzane” jako „SkĂłrzane”). Panel pobiera teraz tę
kolumnę przez `HEX()` i dekoduje bajty samodzielnie — tą samą metodą, którą już
stosował dla nazw przedmiotów.

### Przywracanie kopii świata kończyło się błędem (NieBijOddam)

„Błąd występujący podczas przywracania kopii serwera”, a w logu:
`Cannot convert value "Singleplayer\Serwer\backups\db-backup-….zip" to type
"System.Int32"`. To był błąd po naszej stronie i trafiał w **każdego**, kto ma
serwer w folderze ze spacją w nazwie — czyli w domyślnej instalacji „Metin2
Singleplayer” w praktyce we wszystkich.

Launcher graficzny uruchamia część konsolową przez `Start-Process`, która skleja
argumenty spacjami i **niczego nie cytuje**. Ścieżka do kopii rozpadała się więc
na dwa argumenty: `C:\…\Metin2` szło do `-RestoreSource`, a ogon
`Singleplayer\Serwer\backups\…zip` lądował pozycyjnie na następnym parametrze,
którym jest `-BotCount` typu `int` — stąd dosłownie „nie można przekonwertować na
System.Int32”. Teraz każda wartość przekazywana do części konsolowej jest
cytowana; nazwy parametrów przechodzą bez zmian. Dotyczyło to tak samo importu
bazy z innej instalacji, jeśli jej ścieżka miała spację.

### Ceny ksiąg umiejętności według nowej tabeli (Iwakura)

Nowa wycena wszystkich 44 ksiąg — Aura Miecza 123 500, Czarowane Ostrze 65 000,
Berserk 58 500, Strach 52 000 i tak dalej; kolejność umiejętności w każdej
klasie jest ta sama co w poprzedniej tabeli, więc zmieniły się wyłącznie kwoty.

Zmienił się też **przelicznik od dropu yang**. Tabela Iwakury zaczyna się od
×1,1 przy dropie 100% i rośnie proporcjonalnie (200% → ×2,2, 500% → ×5,5,
900% → ×9,9, 1500% → ×16,5). Dotąd bot skalował ceny samym mnożnikiem serwera
(100% → ×1,0), czyli o jedną dziesiątą za nisko względem tabeli, którą miał
realizować. Dwa zaokrąglone wpisy z jego rozpiski (1000% → ×11,1 i
10000% → ×111,0) leżą jakieś pół procenta nad tą prostą i potraktowałem je jako
jego własne zaokrąglenie, bo załamanie krzywej w tych dwóch punktach kłóciłoby
się z pięcioma pozostałymi.

Losowy rozrzut (od −20% do +25% na wystawienie) i pamięć sprzedaży działają jak
dotąd.

### Ceny ulepszaczy według tabeli Iwakury

Wszystkie 78 materiałów do ulepszania ma teraz cenę wpisaną ręcznie — od Futra
Wilka za 2 000 po Białą Perłę za 1 100 000 — zamiast wyliczanej z ceny
handlarza. Skalowanie jest liniowe od mnożnika dropu yang (100% → ×1,0,
200% → ×2,0, i tak dalej), czyli inne niż przy księgach; taka jest jego reguła
dla tej tabeli. Rozrzut −20%/+25% na wystawienie działa tu tak samo jak przy
księgach, więc dwa stragany nie proszą równo za ten sam Ząb Orka.

Trzy pozycje z jego listy zmieniają to, co plik zakładał do tej pory: perły idą
w drugą stronę (Biała 1 100 000, Niebieska 820 000, Krwawa 650 000, wcześniej
odpowiednio 2 mln, 3 mln i 6 mln rosnąco), a Małż spada ze 100 000 na 93 000.
Dwie nazwy występują w grze pod dwoma numerami przedmiotu (Nieznany Talizman+
i Żabie Udka) — obydwa dostają tę samą cenę, bo bot wycenia przedmiot, a nie
nazwę.

### Ceny wyglądają jak wystawione przez człowieka (Iwakura)

„1 591 511” czyta się jak maszyna. Kwoty na straganach są teraz zaokrąglane w
górę do kroku wynikającego z ich własnego rzędu wielkości: 12 555 → 12 600,
401 501 → 402 000, 1 241 412 → 1 245 000, 11 512 125 → 11 550 000,
121 314 515 → 121 500 000. Każdy z tych wyników mieści się w przedziale, który
Iwakura podał dla swojego poziomu. Ceny poniżej 10 000 yang zostają nietknięte —
materiał za 300 yang nie staje się ładniejszy przez to, że kosztuje 400.

Zaokrąglanie objęło **wszystkie** wyjścia wyceny, łącznie z płaskimi cenami
+7/+8/+9 — czyli dokładnie tymi, na które gracze patrzą najczęściej.

---

## 2.0.31 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Misja na konia
bojowego wreszcie się liczy, szkatułki blasku i zwoje błogosławieństwa trafiają
na stragany, przełącznik układu świata pojawia się w `.env`, okno ulepszania
może zostawać otwarte, a launcher sam zwalnia zajęte porty.

### Misja na konia bojowego stała na 0/100 (sosen)

„Boty się bugują i nie wykonują misji na konia bojowego — cały czas 0/100
i błądzenie po mapie”. Tak było i nie mogło być inaczej: próba liczyła
zabójstwa potworów o numerach **401–404** (Czarny Wiatr), a tych na pustyni nie
ma ani jednego — mieszkają na drugich wioskach (mapy a3/b3/c3). Bot stał więc na
właściwej mapie i zabijał właściwe potwory, tylko żadne z nich nie było tym,
czego licznik szukał.

Skąd ten błąd: w `regen.txt` pustyni prawie każda linia jest typu `r`, a jej
ostatnie pole to numer **grupy grup**, nie numer potwora — i akurat te numery to
401–404. Ktoś (my) odczytał je jako vnumy potworów i wpisał do kodu, dopisując
przy tym nieprawdę, że potwory z oryginalnej misji „nie są nigdzie
zespawnowane”.

Po rozwinięciu grup przez globalne `group_group.txt` i `group.txt` pustynia
niesie dokładnie to, co mówi wiki: **Skorpion Łucznik (2105, 47 lvl)** —
998 punktów spawnu — i **Wężowy Łucznik (2107, 51 lvl)** — 760 punktów. Próba
liczy teraz je, tak jak misja u Stajennego. Limitu 30 minut, który ma wersja dla
graczy, nadal celowo nie ma: bot kuje aż skończy.

### Szkatułki blasku i zwoje błogosławieństwa trafiają na stragany (sizowski, Iwakura)

„Żaden bot nie sprzedaje szkat blasku i zwojów błogosławieństwa”. Bo każdy bot
zużywał wszystko na siebie: szkatułka szła na stragan dopiero od stosu pięciu,
a zwoje zostawały w plecaku, dopóki cokolwiek noszonego było poniżej +9 — czyli
u bota, który wciąż się przezbraja, praktycznie zawsze.

Zgodnie z propozycją sizowskiego („4 używają do rozwijania postaci, 1 sprzedaje
— jak prawdziwy gracz”) **co piąty bot jest teraz handlarzem zasobów**:
wystawia szkatułki już od stosu dwóch i zostawia sobie jeden zwój zamiast
trzech. Rola jest stała (losowana z PID), więc nie miga między restartami, i
jest rozdzielona od roli „skupuje złom”. Pozostałe cztery piąte populacji
zachowuje się jak dotąd — nadal otwierają i nadal ulepszają.

### Przełącznika układu świata nie było w `.env` (NerrVoVy)

„Przełącznik `M2_PLAYERBOT_WORLD_LAYOUT` nie występuje po aktualizacji”.
Zgadza się — 2.0.30 dodało go do obu plików Compose i do `m2-render-config`, ale
nie do `.env.example`, a to jedyna droga, którą nowe ustawienie trafia do
istniejącego `.env`. Nic się nie psuło (Compose ma własną wartość domyślną
`split`), tylko nie dało się tego włączyć bez ręcznego dopisania linii. Teraz
jest w `.env.example` wraz z opisem obu trybów i kosztu każdego z nich.

### Okno ulepszania może zostawać otwarte (Paweł „Pabloo”)

Kod przygotowany, przeniesiony na naszą wersję i przetestowany w grze przez
Pabloo. Dwie rzeczy po stronie serwera:

- **`m_iRefineAdditionalCell` nie był inicjowany** przy tworzeniu postaci, więc
  pierwsza sesja ulepszania czytała komórkę zwoju ze śmieci. Samodzielna
  poprawka bezpieczeństwa, niezależna od reszty.
- **„Nie zamykaj okna”**: po próbie serwer sam otwiera okno ulepszania jeszcze
  raz, zamiast zostawiać zamknięte. Włącza się komendą `/refine_keep_open 1`,
  wybór przeżywa relog. Przy zwoju sprawdzane jest dodatkowo, czy w zapamiętanej
  komórce nadal leży poprawny zwój — to naprawia przypadek zużycia ostatniego
  Zwoju Błogosławieństwa.

**Żadnego auto-refine**: każda próba nadal wymaga świadomego kliknięcia, a cała
logika ulepszania zostaje po stronie serwera. Zabezpieczenie jednej sekundy
zostaje — przeniesione za podstawowe walidacje, bo wcześniej przy otwartym oknie
psuło sesję ulepszania. Sprawdzenia NPC i dystansu z 2.0.29 nietknięte, Wieża
Demona celowo bez tej opcji.

Dwa checkboxy w oknie („Nie zamykaj okna”, „Potwierdzaj Enterem”) to zmiana po
stronie klienta i **nie ma jej w tej paczce** — wymaga osobnego wydania klienta.

### Launcher: koniec z „port jest już zajęty” (Tieru)

Przy każdej próbie aktualizacji launcher przerywał budowanie komunikatem
o zajętym porcie, a wyłączanie Dockera nie pomagało. Trzy przyczyny naraz:

- na jednej maszynie potrafi być kilka instalacji tego samego serwera, każda
  jako osobny projekt Dockera, i wszystkie publikują te same porty;
- każdy kontener ma politykę `restart: unless-stopped`, czyli Docker wskrzesza
  starą instalację przy każdym uruchomieniu silnika — a ta polityka czeka
  dokładnie na to, co robił operator, czyli na wyłączenie Dockera;
- sprawdzenie przed startem patrzyło **wyłącznie na port 7788**, a kolizja była
  na 7790 (panel zaawansowany), więc launcher meldował „porty wolne” i dopiero
  Docker przerywał budowanie po kilkunastu minutach.

Teraz launcher sprawdza **wszystkie** publikowane porty (7788, 7790, 7791,
11000, 13000–13002, 3306), a gdy któryś jest zajęty, mówi wprost, który kontener
go trzyma, z jakiej instalacji i **z którego folderu**. GRAJ i ZAINSTALUJ
AKTUALIZACJE zwalniają takie porty same; w wersji konsolowej jest to opcja 21
(„Zwolnij porty”). Zatrzymywana jest tylko obca instalacja — bez dotykania bazy,
wolumenów i postępu botów.

---

## 2.0.30 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Shinsoo i Jinno mogą
wyjść z M2, ceny na straganach różnią się między botami, liczba botów domyślnie
1500.

### Shinsoo i Jinno wychodzą z M2 — tryb jednego świata (thespartanin, sizowski)

„Czerwoni i niebiescy nie mogą wyjść z M2”. Cały front (Dolina Orków, Pustynia,
Sohan, Lochy Pająków, Hwang) hostuje wyłącznie rdzeń Chunjo (game1), a bot nie ma
klienta, więc nie przechodzi między rdzeniami — Shinsoo (rdzeń first) i Jinno
(rdzeń game2) nie miały dokąd pójść po M2 i kapowały na ~36 lvl. To nie był błąd
kodu, tylko podział świata na trzy rdzenie.

Nowy przełącznik **`M2_PLAYERBOT_WORLD_LAYOUT`**:
- `split` (domyślnie) — dotychczasowy układ trzy rdzenie, pełna równoległość, bez
  zmian dla nikogo; Shinsoo/Jinno nadal kapują ~36.
- `unified` — mapy wszystkich trzech królestw i cały front lądują na jednym
  rdzeniu (game1), więc **każdy** bot przechodzi 1→104 i może stanąć na ziemi
  dowolnego królestwa. Kod bootstrapu jest generyczny (królestwo startuje tam,
  gdzie rdzeń hostuje jego wioskę), więc żaden kod rdzenia się nie zmienia; first
  i game2 trzymają swoje mapy gildii/eventów i nie mają botów.

Sprawdzone na żywo (stos testowy, 1500 botów, `unified`): po 500 botów każdego
królestwa wystartowało na game1, boty Shinsoo/Jinno podbite do 40 lvl ruszają na
front tak jak Chunjo. Koszt: cały świat tyka na jednym rdzeniu — zmierzony tick
game1 przy 1500 botach to **9,4 s z każdych 60** (pojedynczy tick maks. 48 ms),
w pełni grywalny. To tryb dla jednej maszyny z jednym światem; przy bardzo dużej
populacji zostaw `split`. Boty widmowe z poprzedniego układu są czyszczone przy
starcie (rdzeń bez botów nie zostawia już nieaktualnego `playerbot_status.tsv`).

### Ceny +7/+8/+9 różnią się między straganami (Iwakura)

Iwakura zauważył, że itemy +7 bez bonusów wszędzie kosztowały równo 150 000 —
brak konkurencji cenowej. Każdy bot ma teraz stały (per bot i per przedmiot)
rozrzut do ±20% na cenach +7/+8/+9, więc rynek pokazuje widełki, jeden bot
podbija drugiego. Rozrzut jest stały (nie miga między stoiskami) i nie psuje
pamięci sprzedaży. Sprawdzone: ceny +7 rozjechały się na 139 500–168 000 zamiast
jednej 150 000.

### Liczba botów 1500

Domyślny sufit populacji na serwerze testowym i głównym ustawiony na 1500 — tyle
w zupełności wystarcza, a przy trybie `unified` trzyma tick w rozsądnych
granicach.

---

## 2.0.29 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Zbanowany bot znika
i nie wraca.

### Zbanowany bot loguje się z powrotem (mateuszp211)

„Po zbanowaniu bota i kicku bot loguje się z powrotem”. Kick usuwał postać, a
pas dopełniania kohorty (`TopUpMissingBots`) minutę później widział brak i
wstawiał bota z powrotem — bo nikt nie sprawdzał bana. Oczywista blokada, czyli
`account.status='BLOCK'`, tu nie działa: **każde konto bota jest z założenia
`BLOCK`** (żeby nikt się na nie nie zalogował), więc ta kolumna nie odróżnia
zbanowanego bota od zwykłego. Rejestrem, który ban faktycznie zapisuje
(`/block_player` → `account.account_block`), jest osobna tabela — pusta, dopóki
ktoś nie zbanuje. Rdzeń czyta ją co minutę: bota z wpisem w `account_block`
usuwa ze świata i nie wstawia z powrotem, a zdjęcie bana (usunięcie wpisu)
pozwala mu wrócić przy najbliższym dopełnieniu. Sprawdzone na żywo: ban jednego
bota usuwa dokładnie tego jednego, reszta kohorty stoi nietknięta.

### Dla darkroom22: „segmentation fault” na 2.0.14

To ta sama wywałka, którą zdiagnozował kimakatsu: linia logu odmowy Teleportera
miała `%d` dla 64-bitowego yang przed `%s`, więc rdzeń czytał numer mapy jako
wskaźnik i padał (ślad stosu w Twojej paczce prowadzi przez `CPlayerBotManager::Update`
do funkcji formatującej tekst — co do joty ta sama sygnatura). Naprawione w
2.0.27; aktualizacja rozwiązuje Twój crash.

---

## 2.0.28 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Poprawia bieganie
botów „tam i z powrotem” po 2.0.26, daje botom matematyczny wybór broni,
naprawia wyzerowanie świata w launcherze i uzupełnia pakiet wsparcia o logi
sklepów offline.

### Boty biegają tam i z powrotem przy pierścieniu straganów (FanFar)

„Boty po aktualizacji sklepów offline biegają w jedną i drugą stronę bez celu”.
Z pakietu wsparcia: na jednym rdzeniu ~430 z 500 botów co ~5 s rozdzielało te
same stosy „na ladę” (8250 linii `split` w 13 minut) — a sklepy offline
powstawały normalnie (284 na świecie po pięciu minutach). Mechanizm: dwa wyjścia
na końcu przejścia otwierającego stragan — trwałe zawiniątko sklepu i brak yang
na zawiniątko — odmawiały **bez ustawienia zegara**, już po rozdzieleniu stosów;
pas scalania stosów (po wyczerpaniu budżetu wraca po 5 s) zlewał pojedyncze
sztuki z powrotem, pas wędrówki robił krok w bok, a następny tick znów
rozdzielał, szedł na stanowisko i odmawiał. Teraz każde takie wyjście odczekuje
(2–4 min, a z trwałym zawiniątkiem 10–15 min), a tanie sprawdzenia — czy bot ma
na zawiniątko, opłatę sklepu offline i rezerwę na Teleporter, i czy minęły dwie
minuty od spawnu — stoją **przed** skanem, rozdzielaniem i marszem. Odmowa
jest logowana (`PLAYERBOT_SHOP: refused ... reason=cannot_pay`).

### Bot liczy obrażenia na cios i tak wybiera broń (Tieru)

„Wprowadź matematyczny algorytm dla bota, który przelicza atak per hit z danej
broni uwzględniając bonusy i średnie broni”. Wynik broni w wyborze ekwipunku to
teraz jeden zwykły cios, tak jak liczy go `battle.cpp`: rzut broni (wartości
3–4 podwojone przez silnik; sztylet i łuk jak dotąd), klasa ataku (poziom, SIŁA,
linie klasy ataku), linie procentowe ataku, linia rasy ważona udziałem tej rasy
na mapie, średnie obrażenia, trafienie krytyczne (drugi cios na każdy procent) i
przebicie (połowa). To, co kandydat zmieniłby na postaci (SIŁA, klasa ataku,
procenty, kryty), jest liczone względem postaci **bez** noszonej broni — więc
broń w ręku i dwie w plecaku są czytane na tym samym ciele. Obrażenia
umiejętności nie wchodzą w cios (linia PvP, której świat jeszcze nie używa);
szybkość ataku to ciosy na sekundę, nie obrażenia na cios. Żadna sztuczna
„premia” nie jest już potrzebna, żeby Riba 48% śr. wygrała z niższą bronią +9 —
robią to liczby.

### Wyzeruj świat i zacznij od nowa — działa (NieBijOddam)

„Brak możliwości wyzerowania serwera”: cztery próby z rzędu kończyły się „Nie
udało się usunąć wolumenu … czy serwer na pewno jest zatrzymany?”. Był
zatrzymany — ale launcher zatrzymuje stos przez `compose stop`, które zostawia
kontenery, a zatrzymany kontener wciąż trzyma swój wolumen. Reset usuwa najpierw
każdy kontener, który się do wolumenu odwołuje, potem wolumen; następny start
odtwarza kontenery jak po aktualizacji. Opcja nazywa się teraz „Wyzeruj świat i
zacznij od nowa” (przycisk **KOPIA / NOWY SWIAT**, w menu tekstowym pozycja 17):
po wyzerowaniu serwer uruchamia się sam na nowym świecie, bez klikania GRAJ.
Kopia starego świata jak dotąd trafia do `backups`.

### Pakiet wsparcia niesie logi sklepów offline

Pakiet FanFara nie miał **ani jednej** linii o sklepach offline, bo lista
wzorców `grep` w launcherze nie znała `PLAYERBOT_OFFLINE`; „czy sklepy w ogóle
powstały” trzeba było wnioskować ze spisu straganów. Dochodzą `PLAYERBOT_OFFLINE`,
`PLAYERBOT_MARKET` i `PLAYERBOT_BAG` (scalanie i sortowanie plecaka — to ono
zdradza pętlę wyżej).

### Sprostowanie do 2.0.27

Opis 2.0.27 twierdził, że odmowa Teleportera była sprawdzona na żywo. Nie była:
poprawka jest sprawdzona kompilacją na obu silnikach, a na świecie testowym nie
dało się wymusić samej odmowy (boty z niedoborem yang nie są tam wysyłane pod
Teleporter). Potwierdzenie przyjdzie z logów graczy; wpis w wątku kimakatsu
poprawiony.

### Dla sizowskiego: „boty nie wystawiają sklepów”

Pakiet z 11:57 był jeszcze z 2.0.25: spis pokazywał 1–21 klasycznych straganów
na rdzeń, bo po serii stoisk straganiarz odpoczywa 30–90 min, a 2.0.24 zabrało
straganom materiały (idą do magazynu) i podniosło próg zapasowej broni do +7.
Od 2.0.26 straganu klasycznego nie ma — bot stawia sklep offline na 8 h i idzie
grać, a po 8 h opłaca go na nowo, jeśli został towar. Jeśli po 2.0.28 sklepów
offline dalej nie widać, nowy pakiet wsparcia pokaże `PLAYERBOT_OFFLINE: create`
albo powód odmowy.

---

## 2.0.27 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. **Pilna poprawka:
wywałka rdzenia** — aktualizuj od razu.

### Wywałka rdzenia, gdy Teleporter odmawia botowi (kimakatsu)

kimakatsu zdiagnozował to co do joty: linia logu, którą bot pisze, gdy Teleporter
odmawia mu przejazdu (za niski poziom albo za mało yang), miała dla yang miejsce
32-bitowe, a rdzeń tej linii trzyma yang 64-bitowo. Pozostałe wartości przesuwały
się o jedno miejsce, numer mapy trafiał tam, gdzie miał być tekst powodu, rdzeń
próbował go czytać jako tekst — i padał. Każda odmowa zabijała rdzeń, więc ta
linia nigdy nie trafiła do żadnego logu. Poprawione tu i w dwunastu innych
miejscach, gdzie yang szedł do logu tym samym błędnym wzorcem (te tylko
pokazywały złe liczby). Poprawka sprawdzona kompilacją na obu silnikach; samej
odmowy nie udało się wymusić na świecie testowym (boty z niedoborem yang nie są
tam w ogóle wysyłane pod Teleporter), więc potwierdzenie na żywo przyjdzie z
logów graczy.

### Boty używają wspomagaczy, kamieni bonusów i eliksirów zamiast je sprzedawać (Pasywny)

„Bot zamiast użyć i dodać bony to posprzedawał handlarzowi”. Bot rozpoznawał
wspomagacze i kamienie bonusów po numerach (vnum), a na tych plikach każda z tych
rzeczy ma po kilka numerów — Mikstura Ataku +10 ma trzy, Zielona Siła/Zielony Czar
(dodanie/zmiana bonusu) po trzy; kopie z ItemShopu (76xxx) nie mają do tego
blokady sprzedaży, więc dostane z panelu szły do handlarza za grosze. Teraz bot
rozpoznaje je po tym, **co robi z nimi gra**, nie po numerze: każdy czasowy
wspomagacz (atak, obrona, szybkość, krytyk, przebicie, zestaw Boga Smoków,
pierścień doświadczenia, zielone/fioletowe mikstury, soki, sushi) jest wypijany
na początku walki — jednego rodzaju naraz, a gra sama odmawia drugiej tej samej
mikstury, póki pierwsza działa; każdy kamień dodania/zmiany bonusu i marmur idą
na ekwipunek (bot najpierw sięga po ten, który ma w plecaku, dopiero potem
kupuje); **Eliksir Księżyca** (doświadczenie) jest wypijany od razu, gdy bot go
ma; a **Wykrywacz Kamieni Metin** — botowi na nic (rysuje po kliencie), graczom
potrzebny — trafia na ladę jako towar. Żadna z tych rzeczy nie jest już złomem
dla handlarza. Łuk +0 bez bonusów u handlarza zostaje, jak było.

## 2.0.26 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty prowadzą prawdziwe sklepy offline i idą grać dalej

Dotąd bot wystawiał klasyczny stragan: stał przy nim jako tobołek i czekał, aż
sprzeda wszystko albo minie czas. Teraz otwiera **prawdziwy sklep offline** —
ten sam, który mają gracze na tych plikach (ikashop): niezależny byt na
pierścieniu straganów, opłacony na 8 godzin (6000 yang), z szyldem, ceną i
towarem wybranym po staremu — a sam bot **od razu wraca do gry**: poluje,
expi, robi Biologa, jeździ do kowala. Co 10–15 minut wraca do sklepu na krótką
wizytę serwisową: odbiera utarg ze skrytki sklepu, dokłada jeden przedmiot,
raz na godzinę przelicza jedną cenę wg rynku, a wygasły sklep z towarem opłaca
na nowo. Boty kupują też u siebie nawzajem w sklepach offline (te same reguły
„czy chcę to kupić” co przy straganach). Każde żądanie do bazy (założenie,
dołożenie, zmiana ceny, odbiór, zakup) jest zapisane w dzienniku i nigdy nie
jest ponawiane po przekroczeniu czasu — silnik nie ma klucza idempotentności,
więc powtórka mogłaby zdublować przedmiot lub yang; nierozwiązane żądanie
tylko wstrzymuje handel tego bota, gra idzie dalej. Zmiana bazuje na pracy
Codexa (handoff 2026-09-13): dziennik żądań z testem jednostkowym, serwis
sklepu, zakupy, hooki `Sent/Complete` w silniku przez `playerbotify.py`, oraz
poprawka rdzenia db, która odsyła kupującemu odmowę blokady (dwóch kupujących,
jeden przedmiot). Sprawdzone na żywo: 47 sklepów założonych i potwierdzonych
przez rdzeń db, 24 zakupy bot→bot, właściciele w statusie „Walczy / Podróżuje /
Robi Biologa”, a nie „Prowadzi stragan”. Na www ranking „Stragany” czyta
teraz tabelę sklepów offline (`player.ikashop_offlineshop`), a „mapa” to
miejsce sklepu, nie miejsce, gdzie akurat poluje właściciel. Klasyczny stragan
zostaje na linii r40250; na tej linii otwarty jeszcze stragan klasyczny jest
zamykany raz przy pierwszym ticku („migrate_offline”), bez utraty przedmiotów.

### Poziom startowy botów: zawsze 1

Sprawdzone od podszewki po zgłoszeniu „postacie 56 lvl mają itemy z M2”: seed
w każdej wersji w historii wstawia `level = 1`, oba wyrenderowane SQL-e też, a
w bazie serwera z tym zgłoszeniem 1500 botów miało 1–5 lvl. Jedyna droga do
skoku poziomu bez expa to karta „⭐ Ustaw poziom” w panelu, klikana ręcznie na
konkretnej postaci — taki bot zostaje ze starym ekwipunkiem, aż przebierze go
zakup zbroi (2.0.22) i premia broni (2.0.24/25). Poprawiono też mylną notatkę w
dokumentacji o „kohorcie 50 lvl z seeda” — nigdy jej nie było.

## 2.0.25 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian. Poprawki do 2.0.24 wg uwag Tieru.

### Broń: w PvE liczą się średnie obrażenia, umiejętności zostają do PvP

Premia „którą broń założyć” liczy się teraz **ze średnich obrażeń dla każdej klasy**
— także dla sury i maga. W PvE (rozbijanie metinów i potworów zwykłym atakiem) liczą
się średnie obrażenia, a nie umiejętności; linia umiejętności to bonus do PvP, którego
jeszcze nie wdrożyliśmy. Dzięki temu bot zakłada Ribę 48% czy Antyka 40% średnich i już
nie zostaje w wachlarzu +9 obok Antyka ze średnimi. Osobno: broń ze **średnimi
umiejętności powyżej 21%** nie jest już mieszana Zmianą Bonusów u żadnej klasy — zostaje
jako ładny bonus do PvP albo idzie na stragan offline, zamiast zostać zniszczona.

### Łucznicy-ninja: sztylet +4 u kowala, albo nie zaczepiają metina sami

Sztylet do metinów jest ulepszany **u kowala** do co najmniej +4 (poprzednio napisałem
mylnie „w plecaku” — ulepszanie idzie przez kowala, tak jak dla noszonej broni). Dopóki
łucznik nie ma sztyletu co najmniej +4, **nie zaczepia metina w pojedynkę** — samym
łukiem i tak sobie nie poradzi. Może za to dobić metina z łuku, jeśli ktoś inny (bot
albo gracz) już go bije.

## 2.0.24 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty zakładają broń 30 lvl ze średnimi zamiast gorszej ulepszonej

„Boty mają w dupie bronie 30 lvl ze średnimi — bot ma Ribę 48 średnie w EQ, a
biega w Kosie Bojowej +6” (sosen94). Nieulepszona broń-nagroda ze średnią 48%
przegrywała w ocenie z bronią +6/+9 niższego poziomu, bo mnożnik działał na jej
niską bazę. Teraz broń z linią średnich (albo, u maga, umiejętności) na poziomie
progu (25%) dostaje premię proporcjonalną do tej linii, więc bot ją zakłada i
dopiero ulepsza — a przy tym gorsza, ale bardziej ulepszona broń już nie wygrywa.

### Łucznicy-ninja ulepszają sztylet do metinów (co najmniej +4)

„Powinni używać ulepszonych sztyletów na co najmniej +4, nie niżej, bo nic z
tego nie będzie” (Tieru). Łucznik rozbija metiny sztyletem, ale sztylet leży w
plecaku (gdy trzyma łuk), więc nigdy nie liczył się ani jako ulepszenie do
założenia, ani jako lepszy zapas — i zostawał na +0, którym metina się nie
rozbije. Teraz wybrany sztylet do metinów jest ulepszany w plecaku jak noszona
broń, z celem +4: kroki +1..+4 to na tych plikach po 90%, więc dojście jest
tanie i prawie bez spalenia, a zwój w plecaku dalej niesie go wyżej.

### Stragany na duplikaty nie zalewają miasta

„759 botów »Prowadzę stragan (zbędny duplikat)«” (AkhiGubernator) — poprawka z
2.0.22 otwierała stragan na każdy duplikat od +4, więc niemal każdy bot z drugą
bronią czy zbroją w plecaku wystawiał się, ignorując suwak handlu. Teraz stragan
na zbędny duplikat otwiera się dopiero od +7 (naprawdę wartościowy, jak FMS +9 z
pierwotnego zgłoszenia), więc miasto nie jest zalewane.

### Materiały pod presją plecaka trafiają do magazynu, nie do handlarza

Na prośbę Tieru: gdy w plecaku brakuje miejsca, ulepszacze (materiały) idą teraz
do magazynu u Dozorcy (M1/M2), a nie do handlarza za grosze. Materiał, którego
ktoś potrzebuje, dalej trafia na stragan; do magazynu idzie tylko to, czego bot
nie sprzeda na ladzie.

### Handlarz tylko dla przypartego do ściany

„Jeśli wystawiają stragan bo mają extra, a nie że potrzebują yang, to mogą stać
ile trzeba” (AkhiGubernator). Przymusowa sprzedaż niesprzedanego towaru
handlarzowi po sześciu wystawieniach działa teraz tylko pod presją plecaka. Bot,
który po prostu ma nadwyżkę, może wystawiać ją dowolnie długo.

### Koniec drżenia konia przy podróży (wsiada/zsiada co sekundę)

Bot z koniem transportowym w drodze przez zatłoczoną mapę (np. pustynię) łapał
mijanego potwora, zsiadał do walki pieszej, a pas podróży zaraz znów go wsadzał
na konia — i tak w kółko, po parę razy na sekundę, nie robiąc ani kroku
(sizowski: KimJestes2 wsiadał i zsiadał co sekundę przez minuty). Teraz koń
transportowy nie wsiada na etap podróży, dopóki bot ma żywy cel do walki
pieszej — walkę oddaje pasowi walki, a po jej końcu jedzie dalej. Samoobrona
działa jak dawniej.

### Panel: martwy suwak i tłok w mieście

Suwak „Polowanie” (HUNTING) sterował misjami polowań, które na tej linii są
wyłączone (quest w `_unused`), więc nic nie robił — ukryty na wersji mt2009;
leveling steruje suwak „Poziom”. Tłum botów „Oglądam stragany” w mieście to
odpoczynek (suwak „Odpoczynek”): to celowe i sterowane suwakiem, nie błąd.

## 2.0.23 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty wracają Pierścieniem Teleportacji, gdy skończą się mikstury

Bot na mapie frontu (Dolina Orków, Pustynia, Góra Sohan, Loch Pająków), któremu
skończyły się mikstury albo broń, i który ma w plecaku Pierścień Teleportacji
(poziom 30+), wraca teraz do miasta od razu, zamiast iść pieszo do portalu przez
całą mapę. Pierścień nie znika — działa raz na 30 minut, jak jego własny czas
odnowienia.

### Boty sortują ekwipunek

Co jakiś czas bot porządkuje plecak: mikstury na pierwszych liniach, potem
wspomagacze i mikstury szybkości, potem skrzynie i klucze. Przenoszone są tylko
pojedyncze przedmioty i wyłącznie na puste, wcześniejsze pola — silnikowym
`MoveItem`, który nigdy nie usuwa ani nie nadpisuje, więc nic nie może zginąć ani
się zbugować. Ekwipunek (zbroje, bronie) zostaje na miejscu.

### Kupony Smoczych Monet z metinów i bossów

ItemShop używa Smoczych Monet, a w grze nie było jak ich zdobyć. Teraz z rozbicia
Metina i z zabicia bossa jest niewielka szansa na Kupon SM (użyty dolicza monety
na konto). Domyślnie ostrożnie: 3‰ z metina, 50‰ z bossa — do wyważenia przez
`M2_DRAGON_COIN_STONE_PERMILLE` i `M2_DRAGON_COIN_BOSS_PERMILLE`.

## 2.0.22 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty noszą zbroję — kupują u handlarza, gdy nie ma dokładnego poziomu

„Czemu większość botów nie nosi zbroi?” Handlarz zbroi (9002) sprzedaje pancerz
tylko na poziomy 0/18/26 danej klasy, a drabinka progresji celuje w poziomy 9,
34, 42 i wyżej — więc bot między dwoma sprzedawanymi poziomami albo powyżej
najwyższego nigdy nie mógł kupić i chodził z pustym slotem (ćwierć populacji nie
miała zbroi na tułowiu). Teraz, gdy handlarz nie ma dokładnego poziomu, bot
kupuje najlepszy pancerz/tarczę/hełm, jaki handlarz ma i na jaki pozwala jego
poziom, zakłada go i ulepsza u kowala (co najmniej +6). Nie kupuje drugiej kopii
tego, czego handlarz nie przebije, ani gorszego.

### Boty sprzedają zbędny duplikat (np. drugi FMS +9)

„Boty nie sprzedają FMS +9, gdy mają dwa w ekwipunku” (Ciapek). Straganiarz już
wystawiał taki zapas, ale nic nie otwierało straganu, gdy plecak był poza tym
pusty. Teraz zbędny duplikat broni lub zbroi (slot już zajęty lepszym lub równym,
a duplikat jest od +4) jest powodem, by otworzyć stragan — niezależnie od losu i
zapełnienia plecaka.

### Bonusy: broń 30/75 z wysoką średnią nie jest mieszana, zmiana tylko od +5

„Dałem botowi FMS z bonusami po 1000, a on zmienił bonusy” (Ciapek). Broń na
poziom 30 lub 75, która ma linię średnich obrażeń lub średnich obrażeń
umiejętności ≥ 25%, jest teraz „skończona” — Zmiana Bonusów jej nie rusza. Kamień
Zmiany nie jest już używany na przedmiotach od +0 do +4; dopiero od +5.

### Magazyn na księgi: pierwsza wizyta nie gubiła depozytu

Strona magazynu, którą opłata dopiero co utworzyła, powstaje w rdzeniu db o jedno
odbicie później, więc przy pierwszej płatnej wizycie skrytka ładowała się bez
ważnego miejsca i depozyt lądował donikąd (deposited=0, 74 księgi w plecaku na
stałe — u uxietoszefa). Teraz bot rozpoznaje „strona jeszcze niegotowa”, nie
zgłasza fikcyjnego depozytu i wraca przy następnej wizycie (bez ponownej opłaty).

### Misje polowania wyłączone na tych plikach

Quest `levelup.quest` (misje „Polowanie: Lv X • Potwór: 0/N”) leży w
`quest/_unused` — żaden hook zabójstwa nie działa, więc licznik nigdy nie schodził
z „0/40”, a misja wysyłała niedozbrojone boty pod swojego potwora (np. na Górę
Sohan). Wyłączona w kodzie i ukryta w panelu; boty polują teraz według doboru map
frontu i pasm poziomów w hubach.

### Nazwy ksiąg umiejętności w ekwipunku na www

„W ekwipunku księgi umiejętności — nie wiadomo jakie”. Klasyczny panel pokazuje
teraz nazwę umiejętności z gniazda księgi, np. „Księga Umiejętności: Aura Miecza”.

### Historia ekwipunku: na jaki przedmiot nałożono bonus

Obok zużytego kamienia historia zapisuje teraz przedmiot, na którym użyto
Wzmocnienia, Zmiany lub Marmuru — „Zmieniono bonusy” przy danej broni czy zbroi.

### Panel bez hasła administratora

Projekt jest singleplayer, więc panele (klasyczny i zaawansowany) dają od razu
pełny dostęp bez pytania o hasło. Serwer nasłuchuje tylko lokalnie; operator, który
wystawi panel publicznie, włącza hasło (M2_PANEL_LOCAL_ONLY=0 albo tryb za nginx).

## 2.0.21 — 2026-09-13

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Bot kupował wędkę za wędką

„Bot nie ogarnął, że jedna wędka wystarczy” (sizowski: osiem Wędek+0 w
osiem sekund, potem plecak z piętnastoma). Wędka zajmuje trzy pola, a
kontrola miejsca przed zakupem pytała o jedno: plecak z pojedynczymi
dziurami i bez wolnej kolumny przechodził test, bot płacił, silnik kładł
wędkę na ziemi (`AutoGiveItem` nie odmawia — zrzuca), licznik wędek dalej
mówił zero i w następnym ticku bot kupował znowu; potem zbierał je z
ziemi. Zakup pyta o miejsce wielkości przedmiotu i sprawdza, że kupiona
rzecz trafiła do plecaka, a nie na trawę.

### Cztery bonusy przez Wzmocnienie, piąty tylko Marmurem

„Boty dodają sobie do przedmiotów 5 bonusów, gdzie wzmocnienie dodaje
maksymalnie 4”. Bot wołał `AddAttribute()` wprost, bez szansy i do pięciu
linii. Teraz jak gracz: Wzmocnienie Przedmiotu dodaje linię tylko poniżej
czterech, z szansą silnika (100/80/60/50%), a piątą dodaje wyłącznie Marmur
Błogosławieństwa z plecaka, na przedmiocie z dokładnie czterema, z szansą
30%; kamień i marmur schodzą przy każdej próbie, także nieudanej. Marmuru
nikt nie sprzedaje, więc bot bez niego kończy na czterech.

### Biolog: po dziesięciu zębach bot oddawał kolejne

„Bot skończył misję, a mimo to dalej chodzi i oddaje zęby orka” (martynka19cm:
22 zęby). Indeks stanu skompilowanego questu to hash nazwy i bywa ujemny —
`key_item` questu Zęba Orka to −1726153001 — a kod traktował ujemny indeks
jako „nieznany”. Po dziesiątym zaakceptowanym zębie stan nigdy nie
przechodził do `key_item`, licznik zostawał na dziesięciu i każda wizyta z
zębem w plecaku była kolejnym oddaniem. „Nieznany” ma teraz własną wartość,
więc po dziesięciu zębach bot idzie po Kamień Duszy z Elitarnych Orków, a z
nim po nagrodę. Bot, który utknął z dziesięcioma zaliczonymi, przejdzie
dalej przy najbliższym zaakceptowanym zębie.

### Boty sprzedawały handlarzowi ulepszacze, marmury i materiały

„Boty sprzedają ulepszacze oraz marmury polimorfii handlarzowi” (sizowski:
Kawałek Lodu, Stalowy Grot, Futro Yeti, Zwój Kamienia Duszy, Marmur —
wszystko za grosze u handlarza). Trzy reguły naraz:

- **Marmur Polimorfii** (typ 19) był złomem — nic go nie wyłączało z domyślnej
  odpowiedzi „sprzedaj”. Teraz to towar na stragan; handlarz bierze go tylko
  pod presją plecaka, gdy bot nie ma gdzie otworzyć lady.
- **Materiał, którego nikt nie potrzebował** (ledger: popyt 0), szedł do
  handlarza z plecaka z zapasem miejsca. Teraz każdy materiał trafia do
  handlarza wyłącznie pod presją plecaka, a przy popycie tylko wtedy, gdy bot
  nie może prowadzić straganu.
- **Kup, a potem sprzedaj po tej samej cenie** (Żółć Niedźwiedzia x2 za
  47 006): bot brakujący jednej sztuki kupował paczkę dwóch, przestawał
  „brakować” i wystawiał obie na własnej ladzie. Lada wystawia tylko nadwyżkę
  ponad **rezerwę** — dwukrotność największej liczby z przepisu, tę samą miarą,
  którą liczy „brak” — a podział na paczki zostawia rezerwę w stosie bazowym.

### Plik polityki przedmiotów: `playerbot_item_policy.tsv`

Na prośbę „stwórz mi jakiś prosty plik, gdzie oznaczę wartość przedmiotów”.
W spoolu (`/opt/m2spool/playerbot_item_policy.tsv`, obok wag) jedna linia na
przedmiot: numer (vnum) albo cały typ (`type:19`), tabulator lub spacja i
słowo: `keep`/`zostaw` (nigdy nie opuszcza plecaka), `stall`/`stragan` (towar
na ladę, przed wszystkim innym), `merchant`/`handlarz` (do handlarza NPC przy
najbliższej wizycie), `drop`/`wyrzuc` (wyrzucony przy wizycie u handlarza, bez
sprzedaży). `#` zaczyna komentarz. Rdzeń czyta plik jak wagi — co pięć sekund
sprawdza, czy się zmienił — i loguje `PLAYERBOT_CONFIG: item policy read`.
Czego w pliku nie ma, podlega regułom botów. Panel klasyczny ma stronę
**Zachowanie botów → Co boty mogą sprzedawać** (`/ai/items`): pole tekstowe,
kontrola każdej linii, zapis. Przykład:

```
30048	stall	# Kawalek Lodu
type:19	stall	# marmury polimorfii
50703	drop	# Kwiat Kaki po zaliczonym biologu
```

### Okazy biologa nie idą na stragan

„Boty wystawiają przedmioty do badań — niech ich nie wystawiają, jak mają
quest zrobiony, to niech sprzedają u handlarza albo wyrzucają”. Kwiat
Brzoskwini, Pokrzywa, Kwiat Kaki, Korzeń Gango, Bez i Grzyb Tue z zaliczonego
etapu były towarem na ladę (350 punktów). Teraz to złom dla handlarza —
sprzedawany przy najbliższej wizycie w mieście, a `drop` w pliku polityki
każe je wyrzucić. Ząb Orka zostaje materiałem (ledger), Kamień Duszy nigdy nie
jest nadwyżką.

### Straganiarz krążący do niedostępnego stanowiska

Z badania AkhiGubernatora: bot `PoMieLoNy` (map 3) przez noc powtarzał marsz na
własne stanowisko. Losowe przesunięcie w pierścieniu targu trafiło na ziemię,
której teren bota nie łączy; marsz do miasta przenosił cel na własny grunt,
bot dochodził, test dojścia (liczony do stanowiska) nie przechodził i ten sam
odcinek planował się od nowa (`goal moved onto reachable ground` 3691/min na
mapie 41). Przed marszem stanowisko jest sprawdzane (`CanReach`), próbowane są
cztery alternatywne przesunięcia (`PLAYERBOT_SHOP_PITCH_TRIES`), a gdy żadne
nie łączy, stragan odkłada się na 5–10 minut z linią `PLAYERBOT_SHOP: pitch
unreachable`. Pierścień, w którym rescue szuka własnego gruntu, mieści się
teraz w promieniu dojścia łącznie z narożnikami (`arrivalDistance / 71`
komórek) — wcześniej narożnik za promieniem był celem, do którego bot
dochodził i „nie dochodził”.

### Panel seban: „Możliwie zawieszony” dla każdego straganiarza

Flaga liczyła się z tekstu statusu (tylko „łowi”/„ryb”), a lista akcji kończyła
się na 12 — stragan (13), wędka (14), przegląd straganów (15), wabienie (16) i
odpoczynek (17) wyświetlały się jako `#13`…`#17` i każdy bot za ladą był
„zawieszony”. Flaga pyta akcji rdzenia (trener, handlarz, kowal, stragan,
wędka, targ, odpoczynek stoją w miejscu z własnej woli), a nazwy akcji sięgają
do 17.

### Polskie znaki w nazwach NPC i na czacie: rdzeń db łączy się w cp1250

Baza jest w CP1250 (2.0.20), ale rdzeń db (`Main.cpp`) domyślnie ustawia
`LOCALE = latin2` i tak nazywa swoje połączenia SQL — nazwy NPC i przedmiotów
przechodziły przez latin2 i traciły ł/ś/ź. `m2-render-config` wpisuje
`LOCALE = "cp1250"` do `db/conf.txt`; w logu rdzenia db pojawia się
`mysql_set_character_set(cp1250)`. Diagnoza: audyt Codex
`polskie-znaki-mt2009-20260912`.

### „250 niewidzialnych botów na 1 lvl z 2500 yang” (Matthaeus)

To nie boty, tylko tożsamości z seedu, których rdzeń nie wystartował: paczka
wsparcia jest z 2.0.8 (log z 11 września), `autospawn requested=117
started=117`, rejestr 2499 użytecznych. Reszta to postacie w bazie, widoczne w
rankingach panelu, nieobecne w świecie. Po aktualizacji i podniesieniu suwaka
liczby botów wstają — nie ma czego naprawiać w rdzeniu.

## 2.0.20 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Stragany znikały: pętla dzielenia i scalania stosów przy pełnym plecaku

„Stan Chunjo M1: 2 sklepy” (sizowski, z paczką: cenzus 50 → 15 → 2 stragany
w dwadzieścia minut, 6 066 „split for the counter” i 4 054 „merged” w
kwadrans na jednym rdzeniu). Licznik wolnych pól plecaka patrzył na
wskaźniki przedmiotów, a silnik trzyma wskaźnik tylko w górnej komórce i
znaczy resztę w siatce zajętości — broń o trzech polach wyglądała jak jedno
zajęte i dwa wolne. Straganiarz dzielił stosy na sztuki „zostawiając trzy
wolne pola”, które były dołami mieczy, pakiet sklepu nie miał pola,
scalanie zwalniało jedno, dzielenie brało je z powrotem — co trzy sekundy,
bez końca, i stragan nigdy nie stawał. Wolne pole to teraz to, co mówi
siatka silnika (`IsEmptyItemGrid`); ta sama miara stoi pod progiem pełnego
plecaka, ciśnieniem w plecaku i rezerwą na zestaw ze skrzyni, więc wszystkie
trzy były dotąd zawyżone o wysokość sprzętu w torbie.

### Ceny wg Iwakury: księgi umiejętności i mnożniki bonusów

Księga Umiejętności ma cenę bazową na umiejętność (44 pozycje z jego listy,
od Aury Miecza 75 000 do Bezszelestnego Chodu 5 000), skalowaną mnożnikiem
yang serwera (`mob_gold`: przy 200% ceny podwajają się), z losowym
rozrzutem 0,8–1,25 na każde wystawienie; pamięć sprzedaży działa jak dotąd.
Portfele botów nie podbijają już cen ksiąg. Bonusy na sprzęcie mnożą cenę
wg jego tabeli: dla każdego slotu i bonusu jeden mnożnik za wartość
maksymalną (odczytaną z własnej tabeli silnika `item_attr`) i jeden za każdą
inną, rasy z podziałem na poziom 33; średnie obrażenia i obrażenia
umiejętności na broni progami (średnie 40–45 to ×6, 60 to ×70; umiejętności
25–29 to ×14, 30 to ×40). Mnożniki się składają, z sufitem stukrotności.
Bonusy spoza tabeli (regeneracja staminy, czas trwania umiejętności, odbicie
pocisku) nie zmieniają ceny. „Mnożnik bywa agresywny” — do delikatnej
korekty, gdy ceny w praniu wyjdą nieodpowiednie.

## 2.0.19 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Ulepszony przedmiot nie schodzi na straganie poniżej tego, co kosztował u kowala

„Miecz+4 za 90 yang, Sejmitar+4 za 582, sztylet +5 za 117 — niech cena
minimalna będzie tym, co poszło na ulepszenie” (djariczek). Cena wywoławcza
skalowała cenę handlarza za przedmiot bazowy medianą portfeli, a na świeżym
świecie jedno i drugie to grosze. Teraz podłogą jest suma opłat kowala za
każdy krok od przedmiotu bazowego do tego plusa, odczytana z własnych tabel
silnika (`refine_proto`), każdy krok liczony po tym, ile średnio kosztuje
jego przejście: koszt razy 100 przez szansę — bo krok, który przy +7 nie
wychodzi sześć razy na dziesięć, płaci się więcej niż raz, a ostatnia
porażka zabiera przedmiot. To jest ten procent za ryzyko: 11% przy +1, 100%
przy +6, ponad 200% przy +9. Miecz+4 nie schodzi poniżej 6 778 yang (opłaty
400 + 800 + 1 600 + 3 300), Mnisia Zbroja Płytowa+4 poniżej 8 400. Podłoga
trzyma pod każdym wyjściem wyceny — pod cenami stałymi +7/+8/+9, pod ceną
złomu, pod pamięcią sprzedaży i limiterem kroku — oraz pod rabatami za
niesprzedane stoiska i „Wyprzedaż” biednego straganiarza: rabat schodzi z
marży, nie z opłat. Materiały do ulepszeń nie są wliczane; mają własny
rynek. Przedmiot, który nie stoi na zwykłej drabinie plusów (wędrówka po
`refined_vnum` nie wraca do jego numeru), podłogi nie dostaje.

### Zwój w plecaku to drabina do +9

„Mnóstwo zwojów na serwerze, a boty chyba ich nie używają”. Używały, ale
rzadko: cel ulepszeń bota wyznaczała jego osobowość — sześć botów na
dziesięć kończyło na +6 i tam stawało, choćby miało w plecaku Zwój
Błogosławieństwa, a zwoje szły na stragan. Na stosie testowym leżało 660
zwojów u 482 botów, a na 45 ulepszeń do +7 tylko 9 poszło pod zwojem.
Tymczasem pod Zwojem Błogosławieństwa albo Boga Smoków silnik nigdy nie
niszczy przedmiotu (porażka to poziom w dół albo nic), więc powód, dla
którego bot bał się +7, znika. Teraz zwój w plecaku podnosi cel do +9 —
u kowala i w polu, bo zwój nie potrzebuje kowala — dla noszonych
przedmiotów i broni z 30 poziomu, od +6 w górę, Boga Smoków od +7, gdy
bot go ma; bez zwoju wraca dawna ambicja. Stragan zostawia botowi trzy
pierwsze zwoje, dopóki jakiś noszony przedmiot ma je do czego użyć;
reszta jest towarem, bo inne boty też ich potrzebują. Opłata i materiały
z tabeli obowiązują jak dotąd.

### Lagi klienta co kilkanaście sekund: jedno planowanie trasy trwało pięć sekund

„Klient laguje, stałe lagi co około 10–20 s” (sizowski, z paczką wsparcia).
W paczce: rdzeń `game1` (1127 botów, mapy wspólne) spędzał w ticku botów
20–32 s z każdych 60, a pojedynczy tick sięgał 5,1 s — na ten czas rdzeń
nie obsługuje nikogo, więc każdy gracz na jego mapach zamiera. W środku
siedziały pojedyncze dalekie planowania trasy w Dolinie Orków po 1–5 s
(`PLAYERBOT_NAV: far plan map=64 … cost_ms=5066`), a nawet średnie plany
były 16–90 razy droższe niż na naszym stosie testowym przy tej samej
liczbie botów. Przyczyna: wyszukiwanie korytarzowe (A* z ważoną
heurystyką) nigdy nie zamykało komórki — komórka osiągnięta później
taniej wracała na stertę i była rozwijana ponownie, a przy karach za wodę
w dolinie dróg prawie równych jest bez liku, więc te same komórki
rozwijały się w kółko. Komórka zdjęta ze sterty jest teraz zamknięta na
stałe (trasa najwyżej odrobinę dłuższa), a za tym stoi twardy limit
60 000 rozwinięć: po nim wyszukiwanie oddaje najlepszą trasę częściową —
do komórki najbliższej celu — a bot planuje resztę stamtąd, zamiast
zgłaszać dotarcie. Każdy plan ponad 250 ms (i każdy daleki) zapisuje w
logu, co kosztował: `abstract_ms`, `regions`, `fine_ms`, `expanded`,
`partial` — następna wolna maszyna będzie do odczytania, nie do
zgadywania.

### syserr: 2 773 linii o „Miksturze Ataku +15” w dwanaście minut

Z tej samej paczki: `GetRefineLevel` silnika porównuje plus z nazwy
bazowej z plusem z nazwy polskiej i przy różnicy pisze do syserr — a
„Mikstura Ataku +15” (71034/76018) to mikstura, której koreańska nazwa
kończy się gołym „+”. Każde spojrzenie bota do plecaka z taką miksturą to
była linia na dysku. Sprawdzenie dotyczy teraz tylko broni i zbroi.

### Rdzeń db: brakująca tabela `log.ikarusshop_log`

Każde otwarcie sklepu offline i każda sprzedaż w nim kończyły się w syserr
rdzenia db linią „Table 'log.ikarusshop_log' doesn't exist” — żaden zrzut
w pakiecie nie definiuje tabeli, do której rdzeń pisze. Jest w schemacie
logów (`logschemify.py`), dokładany przy każdym starcie.

### Crashe rdzeni z 2.0.11 i 2.0.12 — ślady zebrane

Ta sama paczka przyniosła pierwsze pliki `crash-*.txt`: sześć segfaultów
między 11:08 a 13:30, wszystkie na 2.0.11/2.0.12, żadnego od 2.0.13 przez
pięć i pół godziny. Trzy ślady kończą się w
`CHARACTER::GetMoveMotionSpeed` wołanym z `Goto` w ticku botów — to
wygląda na wiszący wskaźnik założonej broni, co pasuje do poprawki liczby
komórek plecaka z 2.0.13 (do 2.0.12 boty iterowały po 135 komórkach
zamiast 90). Dwa pozostałe ślady (libc z ticku) czekają na zbudowanie
binarki 2.0.11 do symbolizacji; jeśli plik `crash-*.txt` pojawi się na
2.0.17 lub nowszej, proszę o paczkę wsparcia.

## 2.0.18 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Suwak „Grupy (PT)” naprawdę steruje liczbą grup

„Czy ustawiony na minimum, czy maksimum, grup na serwerze jest zawsze tyle
samo” (jaksiezabic, z tabelą: 1000 botów, 37 w grupach przy 25 i przy 250).
Waga PARTY docierała tylko do plannera — do rangi celu „wyzwanie grupowe”
bota, który już jest w grupie — a o tym, kto w ogóle może być w grupie,
decydowała rola „wojownik grupowy” losowana raz przy logowaniu (7% botów,
30% łuczników) i, na mapach frontowych, poziom obozu. Suwak nie miał więc
czego zmienić. Teraz waga wyznacza udział populacji dopuszczony do grup:
20% przy 100 („jak w grze”), 5% przy 25, 50% przy 250; wojownicy grupowi
zajmują pierwsze miejsca losowania, więc są ostatnimi, których suwak
zabiera, i pierwszymi, których oddaje. Na froncie podstawą jest cała mapa,
jak dotąd — obozy Czarnych Orków i bossowie to praca grupy — więc przy
wadze 100 nic się tam nie zmienia. Losowanie jest stałe po pid: ten sam
suwak jutro daje te same grupy. Bot poza kohortą opuszcza grupę przy
najbliższym sprawdzeniu (kilkanaście sekund od przesunięcia suwaka), nowi
dołączają w ciągu paru minut. Do logu trafia co dziesięć minut
`PLAYERBOT_PARTY: census` (ilu dopuszczonych, ilu w grupie, ile grup, waga).
Zmierzone na stosie testowym (2478 botów): przed poprawką 7% botów w
grupach; przy 250 — 30% (755 botów, 247 grup) po trzynastu minutach; po
przestawieniu na 25 — 3% (78 botów) po pięciu minutach. Tick rdzenia z
największą mapą: 9,6 s na 60 przed, 10,2 s przy 250, 8,4 s przy 25.

## 2.0.17 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Skrzynie Blasku Księżyca: boty je otwierają, a na mt2009 w ogóle wypadają

„Boty mają po 600 szkat w eq i nie ruszają ich” (uxietoszef, z tabelą:
165 663 nieotwartych Szkatułek Blasku Księżyca w plecakach botów;
zombian: „2.0.14 dalej nie otwierają”). Pamięć odmów otwarcia była jedna
dla całej populacji, po numerze skrzyni: bot, któremu silnik odmówił, bo w
tej chwili nie miał miejsca na zawartość, wyłączał tę skrzynię wszystkim na
dziesięć minut — a przy dwóch tysiącach botów zawsze jakiś ma pełny
plecak, więc skrzynia była wyłączona bez przerwy. Pamięć jest teraz na
bota i skrzynię, a skrzynia ponad poziom bota (Skrzynia Eksperta III od
50, Mistrza I od 60) jest pomijana bez pytania silnika i idzie na ladę jako
towar. Druga rzecz: na linii mt2009 konfiguracja rdzenia nie dostawała
`MOONLIGHT_CHEST_PERMILLE`, więc Szkatułka Blasku Księżyca z naszej listy
(zwoje bonusów, mikstury, wzmocnienia, czasem księga) nie wypadała tam
wcale — na stosie testowym po sześciu godzinach zero sztuk. Tokeny są
w konfiguracji obu linii (domyślnie 10‰ z potwora, 300‰ z metina;
`M2_MOONLIGHT_CHEST_PERMILLE` i `M2_MOONLIGHT_CHEST_STONE_PERMILLE` w
`.env`).

### Wyszukiwarka: znacznik straganu bez niemieckiej reklamy

„Niemiecki komunikat nad każdym sklepem po wyszukaniu zbroi” (vasils.).
Efekt użyty w 2.0.15 jako kolumna światła to w tym kliencie promocja
„Noch 1 Level-Up! … siehe www.metin2.de” rysowana nad postacią. Znacznikiem
jest teraz fajerwerk, i tak też mówi komunikat w czacie.

## 2.0.16 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty łowią ryby na mt2009

„Boty nie łowią ryb, brakuje ulepszaczy” (sizowski). Na tych plikach
łowienie wymaga poziomu 50, flagi ukończonego wprowadzenia u Rybaka,
przynęty na wędce i **karty wędkarskiej** — przedmiotu unikalnego na dobę,
którego nikt nie sprzedaje, bo pochodzi z questa. Bot flagę ustawiał sobie
sam i minigrę rozgrywał po stronie serwera, ale reguła wyprawy odmawiała
mu wędkowania bez karty, a karty nie miał skąd wziąć — więc na żadnym
świecie mt2009 żaden bot nigdy nie łowił. Bot od 50 poziomu kupuje kartę
tak, jak kupuje Zwój Zapomnienia: za 50 tys. yang tworzoną na miejscu i od
razu zakładaną, a kolejną, gdy ta wygaśnie. Poniżej 50 poziomu bot nad wodę
nie idzie, bo silnik i tak odmówi. Do tego stanowisko, przy którym silnik
nie widzi wody (tabele brzegu mierzone są na mapie drugiego silnika, a
mapa mt2009 różni się o komórkę tu i tam — stanowisko Joan (67175,158125)
jest na mt2009 suche), jest porzucane na stałe i bot idzie na następne;
dotąd stał przy nim do końca sesji jako „never_cast”.

### Księga z metina tylko do piętnastu poziomów nad kamieniem

„Na 46 poziomie leci drop KU z metinów 5, 10, 15” (cyfrowy_mat,
uxietoszef). Dopisywanie jednej księgi do każdego metina (od 1.29) nie
patrzyło na różnicę poziomów, choć własne tabele silnika wygaszają drop z
różnicą poziomów. Księga jest dopisywana tylko, gdy zabójca ma najwyżej
piętnaście poziomów więcej niż kamień; własny los tabeli metina zostaje.

## 2.0.15 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Kowal tylko dla tego, co bot założy

„Pomimo że nosi lepszą broń, uparcie przepala yang u kowala na ulepszanie
broni na 1 poziom, które są nikomu niepotrzebne, i to jednocześnie
różnych” (elgrandebgc, z historią ekwipunku: Miecz +1 → +2 → +3, Glaive,
Drewniane Kolczyki, Miedziany Naszyjnik, jeden spalony, pod Gilotynowym
Ostrzem +4 i z 14 tys. yang w sakwie). Reguła złomu zostawia w plecaku
sporo rzeczy celowo — towar zbieracza, wszystko od +4 na ladę, przedmiot z
dobrymi liniami — a przejście kowala brało do ulepszania wszystko, czego
reguła złomu nie oddała handlarzowi. Ulepszany z plecaka jest teraz tylko
przedmiot, który bot założy: ulepszenie czekające na przejście ekwipunku
albo jedyna na slot część wyższej rangi, z której kowal może zrobić
ulepszenie. Towar idzie na ladę taki, jaki jest. Pomiar na stosie
testowym (2482 boty, świat młody, 11–16 lvl): ulepszeń przedmiotu słabszej
rangi niż noszony w tym slocie było 175 na pół godziny przed zmianą i 70
po niej; ulepszeń noszonych części tyle samo co wcześniej.

### Wyszukiwarka oznacza stragany botów kolumną światła

„Znaleziono sklepy, ale nie są ani podświetlane, ani zaznaczone na mapie”
(sizowski, po 2.0.13). Klient tych plików podświetla i rysuje na mapie
tylko byty sklepów offline — trzyma ich listę z własnego pakietu systemu
ikarus — a stragan bota to zwykły sklep prywatny na postaci, więc numer z
listy wyników niczego mu nie wskazywał. Nad każdym znalezionym straganem
bota pojawia się teraz kolumna światła (efekt awansu, widoczny tylko dla
szukającego), a czat mówi, ile straganów botów znaleziono. Na mapie
klient nadal zaznacza tylko sklepy offline — to jest po jego stronie.

### Kompilacja dopasowana do pamięci, nie do rdzeni

„Aktualizator zatrzymuje się na build game 2/3 67%” (.unright, laptop
8 GB): Docker Desktop daje maszynie budującej połowę pamięci komputera,
a `cmd_general.cpp` czy `char.cpp` z `-O2 -g` biorą ponad gigabajt na
kompilator — cztery naraz na czterech gigabajtach mieliły dyskiem, aż
budowa wyglądała na zawieszoną. `make -j` bierze teraz mniejszą z liczby
rdzeni i pamięci podzielonej przez 1400 MB (na 8 GB: 2 wątki, wolniej, ale
do końca); `M2_MAKE_JOBS` w `.env` nadal ma pierwszeństwo. Obie linie.

## 2.0.14 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Panel klasyczny: polskie znaki w nazwach przedmiotów

W 2.0.13 panel czyta nazwy z `item_proto` tego świata, ale kolumna jest w
CP1250, a połączenie panelu z bazą w latin1 — serwer bazy konwertował
nazwy po drodze i każda litera, której latin1 nie ma („ł, ś, ż, ę, ń”),
przychodziła jako „?”: „Skrzyd?a Demona Chakr.”, „Zw?j B?ogos?awie?stwa”.
Panel prosi teraz o surowe bajty i dekoduje je sam. Sprawdzone na 6001
nazwach: ani jednego znaku zapytania.

### Panel klasyczny: bazowe statystyki przedmiotu w dymku

Dymek przedmiotu pokazywał linie bonusów i sloty, a wartość ataku, obronę,
stałe bonusy przedmiotu i wymagany poziom brał ze statycznej tabeli linii
1.x — dla przedmiotu, który ma tam ten sam numer, były to liczby drugiego
silnika, dla reszty nic. Na mt2009 te pola też idą z `item_proto` tego
świata i dymek liczy je jak klient: Lwi Miecz +9 to „Wartość Ataku 226–251”,
„Wartość Magicznego Ataku 163–209”, „Szybkość Ataku +26%” i „Wymagany
Poziom 75”, dokładnie jak w grze.

## 2.0.13 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Profil wyposażenia czterech GM na koncie admin

Konto `admin` ma cztery postacie GM — Admin (wojownik), AdminNinja,
AdminSura i postać operatora — i dotąd tylko generator `gm_characters.sql`
je wyposażał: raz, przy tworzeniu, bez umiejętności, bez konia w siodle i
bez eliksirów; postać założoną ręcznie przed dodaniem generatora (Tieru,
szamanka na poziomie 1 z wachlarzem +0) omijał w całości, a stosy 50 zwojów
i 50 medali w jednym rekordzie były stosami, których silnik nie utrzymuje
(proto: 20). Nowy quest `gm_profile` uruchamia się przy każdym logowaniu
postaci GM z konta `admin` i, gdy wersja profilu postaci jest starsza,
doprowadza ją do profilu przez własne ścieżki silnika: poziom 90, 500 mln
yang, grupa umiejętności 1 i umiejętności profesji na P (40), wspólne na
własnych sufitach (jazda 21, przywołanie konia 10, języki i konne 20),
koń 21, komplet +9 klasy z bonusami z audytu (`docs/codex-audits/admin-gm-
mt2009-20260912`) założony na postać, druga broń, 1000 strzał dla ninja,
plecak (księga wojskowego konia 50053, po 200 dużych eliksirów, zwoje po 20,
pierścień teleportacji, pełne automatyczne eliksiry 72726/72730 po dwie
sztuki), a kilka sekund po zalogowaniu włącza po jednym eliksirze każdego
rodzaju — przez tę samą ścieżkę użycia, którą idzie kliknięcie w kliencie,
bo tylko ona zakłada efekt regeneracji. Każdy krok pyta najpierw, co postać
już ma: przedmiot noszony lub leżący w plecaku nie jest dawany drugi raz,
stosy są dopełniane do minimum, a przy ponownym logowaniu i po restarcie
nic się nie dubluje (flaga wersji zapisywana na końcu). Starterowy złom —
broń +0 klasy i małe mikstury — schodzi dopiero, gdy broń profilu jest
założona; questy startowe dostają znacznik „wydano”, żeby nie nadały go
ponownie. Dobre przedmioty zostają. Do tego dwie poprawki silnika:
fabryka przedmiotów tworzyła każdy automatyczny eliksir jako zużyty (gniazdo
„zużyte” równe pojemności — świeży eliksir z NPC lub z panelu mówił
„pusty”), a questy dostały `item.use()` (przez `UseItemEx`, bo `UseItem`
odmawia wszystkiego, gdy działa skrypt questa). Silnik dopuszcza pięć
założeń na pół sekundy, a logowanie potrafi je zużyć, więc to, czego
logowanie nie założyło, ubiera timer kilka sekund później.
`gm_characters.sql` wpisuje stosy zgodne z proto i nie daje medali konnych.
Sprawdzone na stosie testowym na wszystkich czterech postaciach (w tym
Tieru z poziomu 1 i ninja z łukiem w ręku): komplet założony, eliksiry
włączone przez quest, po dwa–trzy logowania i restart rdzenia z tą samą
liczbą przedmiotów.

### Gildie botów z nazwami od Iwakury

Boty zakładały gildie pod dwiema nazwami na królestwo — sześć gildii na
świat, a każdy kolejny założyciel zastawał obie nazwy zajęte. Pula to teraz
sto nazw z listy Iwakury (`data/guild_names_iwakura.txt`, „Shire”,
„UrzadPracy”, „TotalneBoty”…): założyciel zaczyna od miejsca wyliczonego z
własnego pid, bierze pierwszą nazwę, której żadna gildia na świecie nie
nosi, i pomija te dłuższe niż limit silnika (14 znaków na mt2009). Sam
system — założenie przez bota od 40 poziomu z 200 tys. yang, zaproszenia
dla botów tego samego królestwa w pobliżu, wspólne polowania na bossa przez
czat gildii — bez zmian; w tej wersji sprawdzony na żywo na stosie
testowym: stu botom Chunjo podniesiono poziom do 41, siedem gildii
powstało w trzy sekundy od startu (Biedronka, TotalneBoty, Vitality,
BlackWolfs, Zawodowcy, ZakonBigosu, Anarchia), 24 członków po trzech
minutach, tabele `guild` i `guild_member` wypełnione.

### Linux: aktualizacja z paczki, nie z gita

Na serwerze bez launchera (Debian/VPS) jedyną drogą aktualizacji był
aktualizator linii 1.x: `installer/install.sh` i kontener `updater`, które
odświeżają checkout repozytorium i nakładają drzewo `linux-port/` — linii 1.x.
Na serwerze 2.x kończyło się to wersją 1.33.3 w `VERSION`, stawkami
zawieszonymi w `state=running` (skrypt stawek 1.x nie zna flag zdarzeń
mt2009), panelem, który nie rozmawia z grą, i aktualizacją z panelu stojącą
na 40% (l0st3k, 12 września: „checkout z main melduje 1.33.3”). Nowy
`linux-port/tools/update.sh` robi na Linuksie to, co launcher na Windows:
czyta `update-manifest-mt2009.json`, pobiera paczkę tej linii, sprawdza sumę
SHA-256, rozpakowuje ją na folder serwera (`.env` zostaje) i uruchamia
`docker compose up -d --build`. Kontener `updater` z profilu `update`
uruchamia ten skrypt w trybie `watch` (te same pliki `request` i
`update.status`, które pisze i czyta panel), a przycisk w panelu klasycznym
pokazuje polecenie dla tej linii. `PACZKA_INFO.txt` ma sekcję dla Linuksa.
Sprawdzone w kontenerze `python:3-alpine`: pobranie, suma, 6454 plików
rozpakowanych na miejsce, `.env` nietknięty, tryb `watch` odpowiada na
żądanie panelu.

### Skrzynie nie wysypują się na ziemię

„Postać nadal źle sprawdza zajętość ekwipunku: otwierając skrzynię, wypada
to na ziemię” (sizowski, zrzut z Zieloną Siłą i Skrzynią Eksperta I na
trawie). Na mt2009 stała `INVENTORY_MAX_NUM` to 135 komórek: dwie strony
plecaka plus strona ekwipunku konia, do której silnik nie wkłada niczego,
dopóki postać jej nie odblokuje — bot nigdy tego nie robi. Każde liczenie
wolnych komórek w botach szło po 135, widziało 45 komórek‑duchów i skrzynia
„mieściła się” w plecaku, którego nie było; to samo przekłamanie siedziało
w regułach pełnego plecaka, nacisku na plecak i straganu. Boty liczą teraz
po `INVENTORY_DEFAULT_MAX_NUM` (90) — 84 miejsca w kodzie, jedna stała
`PLAYERBOT_BAG_CELLS`.

### Wyszukiwarka przedmiotów widzi stragany botów

„Wyszukiwarka pokazuje 0 sklepów, mimo że przedmioty na pewno są na
straganach” (sizowski). Wyszukiwarka z tych plików przeszukiwała tylko
sklepy offline (system ikarus); stragan bota to zwykły sklep prywatny, więc
plac z trzystoma straganami odpowiadał „Znaleziono 0 sklepów”. Ten sam
przełącznik kategorii pyta teraz także lady każdego bota z otwartym
straganem na tej mapie w zasięgu wyszukiwarki i oznacza je na mapie jak
sklepy offline (`ikarus_shop_manager.cpp`, plik w paczce).

### Panel klasyczny: nazwy, dane i rozmiary przedmiotów z tego świata

„Pomieszane nazwy angielskie/polskie, brak danych przedmiotu, miecz na dwa
sloty widoczny w jednym” (Tieru). Panel czytał nazwy i rozmiary z plików
linii 1.x (`items.json`, `item_names_pl.txt`) i tłumaczył resztę słowo po
słowie — stąd „Leather Buty”, „Azure Suit”, „Przedmiot #30347”. Na mt2009
panel czyta teraz `player.item_proto` tego świata (rdzeń db odświeża ją z
`item_names.txt` paczki przy każdym starcie): polskie nazwy, typ i rozmiar
w komórkach; siatka ekwipunku rysuje przedmiot na tyle komórek, ile ma.
Tabela ładuje się przy pierwszym użyciu i odświeża co godzinę.

### Panel klasyczny bez hasła na serwerze tylko lokalnym

„Nie wiem, gdzie mam hasło admina — może niech każdy będzie automatycznie
zalogowany, skoro to singleplayer” (Tieru). Panel ma od dawna tryb bez
hasła, ale włączał go tylko instalator linii 1.x; paczka 2.x nie ma
instalatora. Panel dostaje teraz adres, na którym go opublikowano
(`M2_HOST_BIND_ADDRESS`, w paczce 127.0.0.1): na adresie lokalnym nikt poza
tym komputerem go nie otworzy, więc nie pyta o hasło. Serwer za proxy
(nginx) też wiąże panel na 127.0.0.1 i jest publiczny — tam operator
ustawia `M2_PANEL_LOCAL_ONLY=0` w `.env` (nowy klucz, opisany w
`.env.example`); instalator 1.x z nginx nadal wymusza hasło sam.

### Teleport do bota z panelu klasycznego

„Nie działa teleportowanie do bota” (Tieru). Przycisk wybierał postać
gracza po najnowszym `last_play`, a ten zapis powstaje przy zapisie
postaci, minuty po zalogowaniu — wybierał więc postać, która grała
poprzednio, kolejkował teleport dla kogoś, kto nie jest w grze, odpowiadał
„timeout” i zostawiał wiersz w kolejce, żeby teleportować tamtą postać przy
jej następnym logowaniu. Odtworzone na stosie testowym: w grze Tieru, panel
kolejkował dla AdminSura. Panel pyta teraz wszystkie postacie graczy z
ostatniego tygodnia naraz, bierze pierwszą odpowiedź (tylko postać online
ją da) i wycofuje resztę. Sprawdzone: `WarpSet Tieru … target map 1`,
ponowne logowanie na rdzeniu docelowym w 3 sekundy, kolejka pusta.

### Paczka logów zbiera też linie skrzyń i walk

`PLAYERBOT_CHEST`, `PLAYERBOT_COMBAT` i `PLAYERBOT_STOCK` w filtrze
sysloga — zgłoszenie o skrzyni na ziemi nie miało w paczce ani jednej
linii o skrzyniach.

### Dwie tabele logów, których brakowało

„Unknown column 'hwid' in loginlog2” przy każdym logowaniu na świecie
założonym przed dodaniem tej kolumny (`CREATE TABLE IF NOT EXISTS` nigdy nie
dokłada kolumny) i „Table 'log.itemshop_dragon_scroll' doesn't exist” przy
zakupie w ItemShopie (quest paczki pisze do tabeli, której żaden zrzut nie
definiuje). Migrator dokłada kolumnę i tabelę przy każdym starcie;
sprawdzone dwukrotnym uruchomieniem na bazie testowej.

## 2.0.12 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Logowanie nie wisi przy dużej liczbie botów

„Po aktualizacji 2.0.11 ciągle wisi na ekranie logowania”, „po zmniejszeniu
botów do 1500 działa” (sizowski). Odtworzone na stosie testowym z 2482
botami: rdzeń db odpowiadał na logowanie po 14–28 sekundach, klient dawał
sobie spokój wcześniej, a w logu rdzenia kanału zostawało
`LoginSuccess - cannot find handle [admin]`. Powód: tabele gry są na tej
linii InnoDB, a MariaDB domyślnie czeka na fsync przy każdym zapisie — na
dysku Docker Desktop to około 70 zapisów na sekundę, a 2482 boty zapisują
około stu (flagi statystyk, flagi questów, przedmioty). Kolejka zapytań
rdzenia db rosła do ~1900, a SELECT logowania stał na jej końcu.
`innodb_flush_log_at_trx_commit = 2` w `99-metin2.cnf`: log InnoDB jest
zrzucany co sekundę zamiast przy każdym zapisie. Przy awarii zasilania
przepada najwyżej sekunda zapisów; dla świata jednego gracza to właściwa
proporcja. Ustawienie wchodzi przy restarcie kontenera bazy (launcher robi
to przy aktualizacji). Zmierzone po zmianie, ten sam świat i 2482 boty:
kolejka rdzenia db 0, odpowiedź na logowanie po sekundzie, logowanie z
klienta przechodzi do wyboru postaci. To samo stało za „teleport na GM-ie
wywala mnie i nie mogę się zalogować ponownie” (marcol_): teleport między
rdzeniami to ponowne logowanie kluczem, które czekało w tej samej kolejce.

### Boty na mt2009 wystawiają stragany

„2500 botów — 0 sklepów” (sizowski), „u mnie nie ma sklepów” (namiot_,
pasywnezarabianie, mlodszygie). Dwie przyczyny, obie po stronie tego silnika:

- Siatka prywatnego sklepu ma tu dziesięć kolumn (`SHOP_PLAYER_WIDTH`), a
  prawa połowa to sloty zablokowane albo premium. Boty układały linie na
  pięciu kolumnach jak na r40250 i każdą linię powyżej piątej wysyłały w
  zablokowaną połowę — silnik odrzucał cały stragan (w logu
  `PLAYERBOT_SHOP: refused`). Linia trafia teraz w ten sam wiersz i kolumnę
  siatki silnika.
- Ten silnik daje prawo do sklepu od 15 poziomu i 800 zabitych potworów
  (`CanOpenShop`). Reguła dla ludzi; bot po każdym restarcie świata zaczynał
  liczyć od nowa, więc przez wiele godzin żaden nie handlował. Bot jest z niej
  zwolniony (zmiana w `char_shop.cpp`, plik w paczce).

Zmierzone na stosie testowym z 2482 botami: przed zmianą 0 straganów w
godzinę, po zmianie 113 otwartych w pięć minut od startu (13 Yongan, 66
Joan, 34 Pyongmoo), dwie odmowy silnika na 116 prób.

### Księgi i medale bez czekania

„Księgi co 24 h” (namiot_), prośba operatora: bez limitu czasu na kolejną
Księgę Umiejętności i na kolejny medal konia. `SKILLBOOK_LEARN_DELAY` (21 h
w paczce mt2009) jest zerem — dla gracza i dla bota. Stajenny nie każe już
czekać 18 godzin na kolejny medal: `pony_levelup.quest` z paczki jedzie w
naszym katalogu questów z wyłączoną bramką `next_time` i jest kompilowany
przy budowie obrazu na miejsce oryginału.

### M2_PLAYERBOT_KINGDOMS=0 wyłącza Shinsoo i Jinno także po zasiewie

„Ustawiłem KINGDOMS=0, a boty i tak pojawiają się w Jinno i Shinsoo”
(adijhos). Przełącznik działał tylko na zasiew: świat, który raz ruszył z 1,
miał tożsamości obu królestw w bazie i każdy rdzeń je startował. Rdzeń czyta
teraz ten sam przełącznik ze środowiska usługi `game` (linia w logu
`PLAYERBOT: M2_PLAYERBOT_KINGDOMS=0 …`) i przy 0 nie daje Shinsoo ani Jinno
żadnego przydziału. Boty zostają w bazie; 1 uruchamia je z powrotem.

### Panel klasyczny sprawdza wersję swojej linii

„Sprawdź najnowszą wersję” mówiło 2.0.5, że jest najnowsza (archded,
l0st3k). Panel czytał `VERSION` z korzenia repozytorium, czyli wersję linii
1.x (1.33.3), i porównywał ją ze swoją 2.x. Panel mt2009 czyta
`linux-port-mt2009/VERSION`; panel 1.x dostaje dodatkowo zdanie, że istnieje
linia 2.x i że to osobna paczka z Discorda, nie aktualizacja.

### Paczka logów: naprawiony grep, logowanie i rdzeń auth

`game-supervise.txt` w 2.0.11 zawierał tylko błąd PowerShella (brak `grep`
w Windows). Filtr robi teraz PowerShell. Do paczki wchodzą też linie
logowania z każdego rdzenia kanału (`login-<rdzeń>.txt`), rdzenia auth
(`login-auth.txt`, `syserr-auth.txt`) i rdzenia db (`login-db.txt`) — bez
nich zgłoszenie „wisi na logowaniu” nie miało w paczce ani jednego wiersza.

### Bot w wiosce poluje na potwory swojego poziomu

„Biegają z 16 levelami po 11 h” (l0st3k), „boty biją na 9/10 lvlach nadal
psy”. Zmierzone na stosie testowym (350 botów, mnożnik expa 100%): jedna
trzecia walk w pierwszych wioskach toczyła się z potworem o sześć i więcej
poziomów niższym od bota, poziom w okolicach 8–9 zajmował 40 minut, a 13–15
około dwóch godzin. Winne było pasmo hubu: bot brał każdy hub, którego
potwory są do siedmiu poziomów niżej od niego, więc bot na 9 poziomie szedł
według pid równie chętnie do psów (Dziki Pies, 15 expa) co do wilków swojego
poziomu (Nieb. Alfa Wilk, 111 expa). Tabela kar za różnicę poziomów w tym
silniku tego nie ogranicza — sześć poziomów niżej to wciąż 90% — niska jest
sama baza. Hub jest teraz brany od dwóch poziomów nad botem do trzech pod
nim, a gdy w wiosce nie ma hubu w tym paśmie, bot bierze huby najbliższego
pasma zamiast dowolnego (bot na 16 poziomie w Yongan szedł dotąd gdziekolwiek,
także do tygrysów na 25, których nie pokona, i do psów). Obozy grup wybierane
są tą samą regułą.

Samo pasmo podwoiło awanse i nie ruszyło nikogo z miejsca: zasięg szukania celu
to 6000 jednostek, tyle co komórka hubu, więc bot stojący przy wilkach swojego
poziomu dalej widział psy sześć kilometrów dalej, a wędrówka do hubu — która
dostaje tick tylko wtedy, gdy nie ma czego bić — nigdy go nie dostawała: 96 ze
115 botów w Joan stało dalej niż 2500 jednostek od jakiegokolwiek hubu. Druga
połowa zmiany: w pierwszej wiosce potwór o sześć i więcej poziomów niższy od
bota i dalej niż 800 jednostek nie jest celem („outgrown_prey” w logu), więc
bot idzie do swojego hubu, a po drodze bije to, co ma pod nogami. Obrona,
zadanie, materiał i polowanie na broń mają pierwszeństwo, jak dotąd.

Zmierzone na tym samym świecie w oknach po 10 minut, awanse na królestwo
(117 botów): Shinsoo 30 → 47, Chunjo 5 → 38, Jinno 37 → 57 — przy rosnących
poziomach, więc każdy kolejny awans kosztuje więcej expa. Walki z potworem
o sześć i więcej poziomów niżej: z 36–40% do 9–18% (reszta to obrona i to,
co stało w zasięgu 800). Tick, liczba szukań celu i resety watchdoga bez
zmian; odmów „outgrown_prey” 5–40 na minutę na rdzeń.

## 2.0.11 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Boty Shinsoo i Jinno nie stoją już na 5 poziomie

„W królestwie czerwonym i niebieskim boty na 5 lv się bugują”, „stoją przy
kowalu i handlarzach”, „2500 botów — 0 sklepów” (greess, martynka19cm,
mrgixon, sizowski). Od 2.0.8 Yongan i Pyongmoo idą do NPC „wprost”, jak
Bokjung — ale lista faz „wprost” była listą Bokjung, gdzie nie ma trenera ani
Starszej Pani. Bot na 5 poziomie potrzebuje trenera (wybór grupy
umiejętności), zaczynał więc wizytę z pustą listą, kończył ją w tym samym
ticku i zaczynał od nowa w następnym: stał na placu z celem „wybór profesji”,
resetowany przez watchdoga co 90 sekund, i nigdy nie wyszedł poza 5 poziom.
W paczce logów sizowskiego: 500 takich botów na rdzeń. Lista „wprost” ma
teraz trenera i Starszą Panią na początku, jak lista Joan, a wizyta, której
lista nie ma czym obsłużyć, w ogóle się nie zaczyna (z linią w logu). Bokjung
tego nie dotyczy — w drugiej wiosce potrzeba trenera nigdy nie jest ustawiana.

### Jedna Księga Umiejętności z kamienia Metin, nie trzy

„Graczowi lvl 46 lecą 3 KU ze wszystkich metinów w M1” (cyfrowy_mat). Od
1.29 każdy kamień dawał trzy księgi, obojętnie od poziomu kamienia. Teraz
kamień dopełnia do jednej: jeśli tabela dropu nic nie wylosowała, jedna
księga i tak wypada, jeśli wylosowała, zostaje to, co wylosowała. Dotyczy obu
silników (łatka 0006 i port mt2009).

### Bot przemianowany ręcznie zachowuje nazwę

„Dałem botowi ADAM miecz +9, rano nie było już bota o tym nicku” (gregoszky):
2.0.10 przemianowało wszystkie boty na nową listę, także te, którym operator
sam zmienił nazwę w bazie. Nazwa, która nie jest ani z puli, ani z zasiewu,
jest czyimś wyborem: taki bot nie jest już przemianowywany (przez żadną
kolejną wersję listy), a jego nazwa nie trafia do nikogo innego. Migrator
liczy je osobno („N bot(s) renamed by hand keep their names”).

### Paczka logów zbiera wszystkie trzy rdzenie i ślady padów

ZBIERZ / WYŚLIJ LOGI zbierało syslog i status tylko z rdzenia Chunjo
(`game1`); zgłoszenie o botach Shinsoo i Jinno nie miało ani jednej linii z
rdzeni, na których stały. Teraz paczka ma osobno syslog, syserr, status i
pliki `crash-*.txt` (ślad stosu po padzie, od 2.0.8) dla `first`, `game1` i
`game2`, syserr rdzenia bazy oraz osobny, dłuższy log kontenera gry — wspólne
800 linii logów compose mieściło 50 sekund, bo zalewały je resety watchdoga i
komunikaty MariaDB o zerwanych połączeniach, a linia „CORE DIED” ze śladem
stosu wypadała z okna, zanim paczka powstała.

## 2.0.10 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Bonusy przedmiotów w panelach mają nazwy, nie numery

„Zamiast nazwy bonusu jest numer #139” (djariczek): silnik mt2009 ma 31
linii bonusów, których stary silnik nie znał — od 138 do 168 w `length.h`
(regeneracja wytrzymałości, czas trwania umiejętności, silny przeciw
Metinom/bossom, pochłanianie obrażeń, przełamania odporności…) — i nie mają
one numeru APPLY, przez który oba panele nazywają linie. Panel klasyczny i
panel Sebana dostały etykiety dla wszystkich 31 (po polsku i po angielsku,
z właściwą jednostką); 139 to „Regeneracja wytrzymałości”, 147 „Czas
trwania umiejętności”. Sprawdzone: tabela etykiet parsuje się (118 wpisów),
oba panele kompilują się.

### Nowa lista nicków Iwakury, po królestwach, bez duplikatów

Iwakura dopisała listę do 1500 nicków (po 500 na królestwo, w tym nicki
wspierających; „te generowane przez AI są okropne”, więc wszystko ręcznie)
i podała zasadę: nicki brane z listy w losowej kolejności, po równo na
królestwo, a dopiero gdy królestwo ma więcej botów niż nicków, do jego
nicków dopisywane „2”, „v2”, „3” lub „v3”. Do tej pory pula szła po kolei od
Chunjo, które brało pierwszy tysiąc nazw, a Shinsoo i Jinno dostawały kopie
z v2/v3. Teraz lista (1475 poprawnych nazw; odrzucone: same cyfry, „GM…”,
dwa wulgarne) jest tasowana raz, dzielona na trzy równe udziały po 491, i
tylko Chunjo — 1500 zasianych postaci — dostaje dalej swoje nazwy z „2”/„v2”,
potem „3”/„v3”. Każdy bot zostanie przemianowany przy pierwszym starcie po
aktualizacji (nowa wersja puli); `M2_PLAYERBOT_HUMAN_NAMES=0` zostawia nazwy,
`restore` przywraca zasiane.

Przy okazji naprawiony błąd, który dotyczy każdego świata po 2.0.8:
**dosiane boty Shinsoo i Jinno dostały te same nicki co boty Chunjo**. SQL
numerował czekające boty od jedynki i dawał im nazwy od początku listy, nie
patrząc, czy inny bot już je nosi — na świecie testowym 999 nazw było
zdublowanych (np. 000Achtung000 na dwóch postaciach). Nazwa noszona przez
bota, który nie czeka na nową, nie jest już wolna. Sprawdzone na tym
świecie: 2499 botów przemianowanych, zero duplikatów, postać gracza na koncie
admin nietknięta, drugi start nic nie zmienia, trzy boty pozbawione nicku
dostają wolne nazwy bez kolizji.

## 2.0.9 — 2026-09-12

Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Panel Sebana: zakładanie kont znów działa na 2.x

„Nie utworzono konta: Unknown column 'empire' in 'INSERT INTO'” na każdym
z trzech królestw (NieBijOddam). Tabela kont w plikach mt2009 nie ma kolumny
`empire` (królestwo konta trzyma `player_index`, a pierwsza postać gracza
dostaje je od gry), a formularz wstawiał ją zawsze. Na 2.x konto powstaje
bez tej kolumny; postać GM zakładana z panelu dostaje królestwo w
`player_index` jak dotąd. Do tego login ma na mt2009 najwyżej 16 znaków
(tyle mieści kolumna) — formularz mówi to od razu zamiast błędu „Data too
long” z bazy.

### Suwak „Odpoczynek w mieście”, a poniżej 18 poziomu nikt nie odpoczywa

W obu panelach (Zachowanie botów) jest nowy suwak `REST`, 0–100%, domyślnie
100: udział botów, które po załatwieniu spraw w pierwszej wiosce zostają na
rynku około trzech minut i spacerują między straganami. Kto ustawi 0, ma boty
cały czas na expie — do miasta przychodzą tylko w sprawach (mikstury, kowal,
handlarz, stragan) i od razu wracają. Suwak działa na żywo: bot, który już
odpoczywa, kończy odpoczynek przy najbliższym ticku po zmianie na 0.
Niezależnie od suwaka dwie nowe reguły: **bot poniżej 18 poziomu nie
odpoczywa nigdy** (ma poziomy do zdobycia i nic do oglądania), a **bez
wystawionego straganu na mapie nikt nie „ogląda straganów”** — do tej pory
bot spacerował między pustymi stanowiskami pod napisem „Odpoczywam w
mieście”, co na młodym świecie wyglądało jak bezczynność (`MayPlayerBotRestInTown`
w `playerbot_config.h` jest całą regułą; licznik straganów pochodzi z księgi
rynku, odświeżanej co minutę). Sprawdzone na stosie testowym po przebudowie:
przy 353 botach poniżej 18 poziomu żaden nie odpoczywa, a rdzeń przy zmianie
suwaka pisze `PLAYERBOT_CONFIG: town rest N%`.

### Cztery postacie GM także na koncie, na którym już ktoś gra

„Na moim koncie admin nie ma postaci GM, tylko moja Tieru” — skrypt z 2.0.4
tworzył Admin, AdminNinja, AdminSura i AdminSzaman tylko wtedy, gdy konto
`admin` nie miało **żadnej** postaci, więc kto założył sobie postać wcześniej,
nie dostawał ich nigdy. Teraz skrypt dosiewa brakujące klasy do wolnych
miejsc (konto ma cztery), pomija klasę, którą ktoś na koncie już gra, i nie
rusza tego, co na koncie jest; ekran wyboru postaci pokazuje stare postacie
na swoich miejscach, a nowe za nimi. Nowe postacie stoją w pierwszej wiosce
swojego królestwa (Joan, Yongan albo Pyongmoo), bo konto ma jedno królestwo.
Sprawdzone na świecie testowym z jedną szamanką na koncie: powstały Admin,
AdminNinja i AdminSura, drugi start nic już nie zmienia. Dzieje się przy
najbliższym starcie serwera po aktualizacji.

### Launcher: trzy wiersze wersji na dole

Stopka launchera pokazuje osobno serwer, launcher i klienta — zainstalowaną
i najnowszą wersję każdego z nich (launcher jest częścią paczki serwera, więc
jego „najnowsza” to wersja serwera; po aktualizacji w tej samej sesji stopka
mówi, że na dysku jest nowsza i trzeba uruchomić launcher ponownie). Do tego
zabezpieczenie: gdy launcher nie wystartuje albo któryś przycisk wyrzuci
wyjątek, treść błędu trafia do `launcher-logs` i do okna z komunikatem — po
restarcie z 2.0.8 jeden z graczy zobaczył okno błędu, którego log nie
zawierał, bo urywał się na „Uruchamiam launcher ponownie”. Odtworzenie tej
ścieżki (prawdziwa aktualizacja serwera → Tak → restart) przeszło bez błędu;
następny raport będzie miał treść.

### Sprawdzanie wersji bez pięciominutowego opóźnienia

Launcher czyta manifest aktualizacji najpierw przez API GitHuba (świeże w
minutę), a dopiero potem z raw.githubusercontent.com, którego CDN trzyma
stary plik przez pięć minut — stąd „Masz już najnowszą wersję (2.0.7)” w
pięć minut po wydaniu 2.0.8.

## 2.0.8 — 2026-09-11

### Boty Shinsoo i Jinno wreszcie chodzą do kowala i handlarki różności

„Tylko boty z Chunjo ulepszają ekwipunek, do kowala nie podbiega żaden z
innych królestw” (nerrvous_s, potwierdzone na 2.0.6). Odtworzone na stosie
testowym z trzema królestwami: w osiem minut rdzeń Chunjo miał 131 wizyt u
kowala i 3918 ulepszeń, rdzenie Shinsoo i Jinno zero — przy wizytach u
handlarzy broni i zbroi u wszystkich. Przyczyna: wizyta w pierwszej wiosce
ma osobną nogę „brama” do wnętrza Joan (handlarka różności i kowal stoją za
murem), a jej współrzędne są Joan (60300, 169400) — na mapach 1 i 41 to
nigdzie, więc noga kończyła się „unreachable” i faz wnętrza nigdy nie było.
Zmierzone na `server_attr` mt2009: w Yongan i Pyongmoo kowal i handlarka
różności stoją w tym samym spójnym obszarze co handlarz broni, bez muru,
więc te wioski idą wprost do każdego NPC, jak druga wioska
(`IsPlayerBotGatedVillage` — brama zostaje tylko dla Joan). Zmierzone po poprawce na tym samym stosie, dziewięć minut od startu:
Shinsoo 308 wizyt u kowala i 4792 ulepszenia, Jinno 329 i 5475, Chunjo 134
i 3920 — a handlarka różności odpowiednio 297, 309 i 169 wizyt.

### Trzy królestwa domyślnie u każdego, boty dzielone po równo

Do 2.0.7 `M2_PLAYERBOT_KINGDOMS=0` był domyślny, więc każdy świat 2.x miał
boty tylko w Chunjo (paczka logów procedera: `shinsoo=0 chunjo=1500
jinno=0`). Teraz domyślnie 1: migrator dosiewa kohorty Shinsoo i Jinno (po
500, raz, także na istniejącym świecie — chwilę dłuższy start), a każdy
rdzeń uruchamia swoje królestwo; liczba botów z suwaka dzieli się po równo
(zmierzone: 970 → 324/323/323). Plik `.env` gracza jest pisany raz i nigdy
nadpisywany, więc launcher przełącza stare `0` na `1` dokładnie raz i
zapisuje to w `M2_PLAYERBOT_KINGDOMS_DEFAULTED`; kto potem ustawi 0, zostaje
przy 0. „Żółci mają przewagę” (sosen94) bierze się z liczby powyżej 1500:
Shinsoo i Jinno mają po 500 postaci, reszta idzie do Chunjo — do 1500 botów
podział jest równy. Ściana poziomów Shinsoo/Jinno (36 lvl) bez zmian.

### Padający rdzeń zapisuje własny backtrace

„Wywala co 2 minuty” (proceder): rdzeń `ch1-game1` padał z sygnałem 11
83–86 s po każdym starcie, a w paczce logów było tylko „Segmentation fault
(core dumped)” — Docker Desktop kieruje zrzuty pamięci do WSL
(`/wsl-capture-crash`), a limit rdzenia w kontenerze to 0, więc nie ma
czego oglądać. Od teraz `libthecore` przechwytuje SIGSEGV/SIGBUS/SIGFPE/
SIGILL/SIGABRT, zapisuje ślad stosu (`backtrace()`) do `crash.txt` w katalogu
rdzenia i na stderr, po czym oddaje sygnał domyślnej obsłudze; rdzenie są
linkowane z `-rdynamic`, więc ramki niosą nazwy funkcji (zmanglowane —
`c++filt`). `m2-supervise` po „CORE DIED” drukuje ślad do logu kontenera
(trafia do paczki logów) i odkłada go jako `crash-<data>.txt` obok syserr.
Przyczyna padu u procedera nie jest jeszcze znana — następna paczka logów ją
pokaże.

### Launcher: „The property 'Count' cannot be found on this object”

Start, URUCHOM DOCKER i paczka logów kończyły się tym błędem u co najmniej
dwóch graczy (sykesal420). Wszystkie trzy uruchamiają sprawdzenie Dockera,
które pyta Windows o zarezerwowane zakresy portów; funkcja zwracająca
dokładnie jeden zakres (albo żadnego) oddaje pojedynczy obiekt (albo nic)
zamiast tablicy, a moduł działa w trybie strict, w którym `.Count` na takim
obiekcie to ten właśnie błąd. Wynik jest teraz zawsze tablicą. Osobna sprawa
u drugiego gracza — „The Compose app is no longer running” — to uszkodzony
Docker Desktop, nie launcher.

### Dane do Navicat i inne drobiazgi

Bez zmian w danych; patrz 2.0.7.

---

## 2.0.7 — 2026-09-11

### Risk mode naprawdę znika (2.0.6 tylko usunęło plik)

Sizowski po 2.0.6: „risk mode nadal jest”. Aktualizacja nigdy nie kasuje
plików, więc każde drzewo z 2.0.0–2.0.5 wciąż ma `quest/high_risk.quest`, a
pętla kompilacji w obrazie gry nadal miała `high_risk` na liście — 2.0.6
zdjęło plik z repozytorium i nic więcej, więc u graczy quest kompilował się
dalej. Bramką jest lista w Dockerfile, nie katalog: `high_risk` zniknęło z
listy, a plik zostawiony w drzewie gracza jest ignorowany. Po tej aktualizacji
nowe konto nie dostaje już wyboru High Risk / No Risk.

### Launcher sam pyta o aktualizację przy starcie

Po otwarciu okna launcher czyta kanał aktualizacji (jak dotąd, raz na sesję)
i gdy serwer w kanale jest nowszy niż zainstalowany, pyta od razu: „Znaleziono
nowszą wersję serwera X — czy chcesz dokonać aktualizacji teraz? Tak / Nie”.
TAK uruchamia tę samą aktualizację, co przycisk AKTUALIZUJ; NIE odkłada
pytanie do następnej wersji (zapamiętane w `.m2launcher-offers.json`), a
przycisk działa zawsze. To samo dla klienta na linii 2.x: po aktualizacji
serwera (albo od razu, gdy serwer jest aktualny) pyta o nowszą wersję
klienta i podmienia packi przyciskiem AKTUALIZUJ KLIENTA. Świeża instalacja z
pełnej paczki wie, jaki klient dostała: plik `CLIENT_VERSION` obok `VERSION`
(dotąd launcher mówił „nieznana” i pytałby o klienta, który już jest);
instalacje z 2.0.0–2.0.6 nie mają tego pliku, więc pytanie o klienta 2.0.3
pojawi się u nich raz — TAK jest nieszkodliwe (podmienia te same packi),
NIE zapamiętuje.

### Dane do Navicat sprawdzone i uzupełnione

Sprawdzone na plikach 2.x: host 127.0.0.1, port z `M2_DB_PUBLISH_PORT`
(domyślnie 3306), konto root z `M2_DB_ROOT_PASSWORD`, konto gry (`M2_DB_USER`,
domyślnie `metin2`) z `M2_DB_PASSWORD` — wszystko czytane z `.env` tej
instalacji, więc zgadza się z tym, na czym stoi baza. Dopisana uwaga, której
brakowało osobom edytującym bazę ręcznie: na 2.x przedmioty i potwory
(`item_proto`, `mob_proto`) są w bazie `world`, a `player.item_proto` i
`player.mob_proto` to tylko widoki; zmiany w `world` zostają po restarcie
(inaczej niż na 1.33, gdzie tabele były nadpisywane z plików txt).

### Suwak „Stragany” ma jawną umowę i dosięga stojących straganów (audyt D11)

„Minimalny suwak, a 180 z 280 botów handluje”: stragan, który już stał, nigdy
nie był pytany ponownie, a cztery wyjątki od suwaka nie były nigdzie
opisane. Teraz stragan pamięta powód otwarcia — Handlarz, brak yang na
mikstury, pełny plecak, dropper pod presją plecaka (te cztery suwak omija,
bo lada to jedyny sposób opróżnienia plecaka), nadmiar ksiąg, los droppera,
„jeden na dziesięciu” (te trzy suwak rusza). Status bota mówi „Prowadze
stragan (los)” zamiast samego „Prowadze stragan”, log otwarcia niesie powód,
a co dziesięć minut rdzeń pisze spis `PLAYERBOT_SHOP: census` z liczbą
straganów według powodu i obowiązującą wagą. Po zmianie suwaka każdy
stojący stragan z losowanego powodu jest sądzony ponownie pod nową wagą
(los jest po PID, więc odpowiedź jest ta sama, jaką dostałby nowy stragan)
i ten, który przegrał, zwija się w ciągu pięciu minut, rozłożonych po PID,
zamiast wszystkie w jednej sekundzie. Opis suwaka w obu panelach mówi to
samo, co kod.

### Martwy zapas ma koniec: magazyn, nie handlarz (audyt D14)

„Szaman trzyma stal wojownika +9, sura przedmioty innych klas”: ekwipunek
powyżej +4, którego handlarz nigdy nie bierze, po czterech stoiskach bez
kupca miał już pełny rabat i żadnej dalszej drogi — jechał w plecaku po
kamieniach do końca życia bota. Teraz każdy wiersz lady ma zapisany czas
pierwszego wystawienia (`mapStockFirstListed`), a przedmiot niesprzedany
przez osiem stoisk (`PLAYERBOT_SHOP_UNSOLD_SAFEBOX_STANDS`) idzie pod
presją plecaka do magazynu razem z nadwyżką ksiąg — zachowany, nigdy
złomowany, poza plecakiem; log `PLAYERBOT_STOCK: to safebox` mówi ile
stoisk i minut za sobą miał. Przedmiot, który bot powinien nosić, nigdy nie
trafia do magazynu. Linia otwarcia straganu mówi też, co się nie zmieściło
i dlaczego (`left_behind no_line= no_slot= antiflag=`), czyli rejestr
powodów niewystawienia z audytu. Skompilowane na obu silnikach; na
serwerach testowych z młodymi botami (poziom 2–4, brak straganów) nie
zaobserwowane w ruchu.

### Masowe nadawanie nie zatrzymuje się na botach offline (audyt D02)

Zmierzone na 1500 zarejestrowanych botach, z których 349 było w świecie:
paczka dla wszystkich doszła do 350 nadanych w dwie minuty i stanęła na
`queued=10` na stałe. Dziesięć miejsc kolejki gry (`MAX_PENDING`) zajmowały
postacie offline — każda czeka 30 s na sweep questa, 60 s na wycofanie przez
workera i wraca po dwóch minutach — więc dziesięć nieobecnych nazwisk
blokowało każdego obecnego za nimi. To „zatrzymanie po 333” z audytu: nie
stan końcowy, tylko kolejka z głową offline. Worker czyta pięciominutową
migawkę kolektora (kto jest w świecie), obecnych wysyła pierwszych, a sondy
dla nieobecnych zajmują najwyżej cztery z dziesięciu miejsc, po dwie na tick.
Zmierzone po zmianie: 40 obecnych botów nadanych w 15 s przy 1146
nieobecnych w ogonie, `pending` w kolejce gry nie przekracza czterech.

Strona nadawania pokazuje stan workera: bicie serca co tick (kontener
`seban-item-grants` stoi → wprost, z poleceniem uruchomienia), ostatni błąd,
liczby i wiek najstarszego zlecenia w każdym stanie, oraz ostrzeżenie, gdy
zlecenie w kolejce gry leży ponad 45 s bez odbioru (pomocnik w grze budzi się
przy pierwszym logowaniu po starcie).

---

## 2.0.6 — 2026-09-11

### Panel klasyczny: przedmioty, teleport, szybkość, poziom znów działają na 2.x

Każde polecenie z panelu klasycznego (nadanie przedmiotu, yang, poziom,
teleport, szybkość biegu) kończyło się na 2.0.x komunikatem „Nic w grze nie
odpowiedziało” (czerwcuu, u4nt). Quest `web_admin` czyta kolejkę panelu przez
`mysql_direct_query()`, a ta funkcja nie jest jedna na obu silnikach: na
r40250 (nasza łatka) zwraca wiersze jako tabelę, na mt2009 zwraca NAJPIERW
liczbę zmienionych wierszy, a wiersze jako drugą wartość. Quest brał pierwszą
wartość za tabelę i nigdy nie widział polecenia. Wspólna owijka bierze tę z
dwóch wartości, która jest tabelą. To samo naprawia masowe nadawanie z panelu
zaawansowanego, bo idzie tą samą kolejką.

### Mnożniki EXP, dropu i yang naprawdę działają na 2.x

Strona mnożników obu paneli mówiła na zielono „Zapisano! Serwer właśnie się
restartuje”, a na serwerze nic się nie zmieniało (wątek z 11 września: „panel
www niby na zielono informuje o zmianie różnych parametrów, a one się nie
zmieniają”). Na r40250 mnożnik to przepisanie `mob_proto.txt` i tabel dropu
przez `m2-rates` w kontenerze gry — obraz mt2009 nie miał tego programu, więc
zlecenie panelu leżało w kolejce i nikt go nie czytał. Silnik mt2009 ma
mnożniki własne: flagi zdarzeń `mob_exp`, `mob_item`, `mob_gold` (i bliźniacze
`_buyer` dla kont premium), przez które `CHARACTER_MANAGER` mnoży
doświadczenie, szansę dropu i ilość yang. Flaga to wiersz `player.quest` z
`dwPID = 0`, wczytywany przez rdzeń bazy przy starcie i rozsyłany do rdzeni
gry. Panel klasyczny zapisuje sześć wierszy, a potem prosi pomocnika w grze
(`web_admin`, polecenie `RATES` na timerze serwera, `game.set_event_flag`) o
ustawienie ich na żywo — zmierzone 2–3 s, bez restartu, we wszystkich trzech
rdzeniach naraz. Gdy nikt nie jest zalogowany i pomocnik nie odpowie, panel
zleca restart jak dotąd, a obraz mt2009 dostał własny `m2-rates`, który nic
nie przepisuje, tylko restartuje rdzenie i raportuje wynik (zmierzone 35 s od
zlecenia do „wszystkie rdzenie działają”, flagi wczytane z bazy). Panel
zaawansowany zapisuje te same wiersze i idzie ścieżką restartu; oba panele
pokazują te same liczby. Wartości 100% oznaczają grę bez zmian.

### Nadanie rangi GM na 2.x mówi prawdę

Silnik mt2009 nie ma gniazda administracyjnego, więc `m2-gm` (przeładowanie
listy GM na żywo) nie istnieje w tym obrazie, a panel i tak pisał „Działa od
razu, w grze”. Listę odczytuje na nowo `/reload a` — polecenie, które pomocnik
w grze może wykonać tylko jako zalogowany IMPLEMENTOR. Panel próbuje tego
przez kolejkę (`GM_RELOAD`), a gdy żaden IMPLEMENTOR nie jest w grze, mówi:
uprawnienia wczytają się przy najbliższym restarcie albo po `/reload a`
wpisanym przez zalogowanego GM.

### Tryb ryzyka (Risk mode) zdjęty z linii 2.x

Quest `high_risk` z plików 1.33 (wybór „High Risk / No Risk” przy logowaniu)
przyszedł do obrazu mt2009 razem z resztą i nie powinien tu być. Nie jest już
kompilowany; kto wybrał tryb wcześniej, ma go zwyczajnie wyłączonego.

### Zamiar wyjazdu gaśnie po dotarciu (audyt D12)

`lDepartureMap` — „ten bot ma wyjechać na mapę X, gdy skończy sprawunki” —
był ustawiany raz i nigdy zerowany. Bot raz wstrzymany w Bokjung nosił go do
końca życia: po powrocie do miasta po zapasy odmawiał każdego polowania na
materiał i każdej wyprawy do Joan, a nad głową miał cel wyjazdu, na który
już dawno dotarł. Zamiar gaśnie po przejściu na docelową mapę (albo na
dowolne pogranicze, gdy pasmo poziomu przesunęło cel w międzyczasie), a
wstrzymany dłużej niż dziesięć minut jest raportowany raz na dziesięć minut
(`PLAYERBOT_DEPARTURE: overdue`) z tym, co go trzyma: broń, zbroja,
mikstury, strzały, miejsce w plecaku, wizyta w mieście, złoto.

### Skrzynia otwiera się tylko wtedy, gdy zmieści się cała paczka (audyt D01)

Silnik wydaje nagrody ze skrzyni po jednej przez `AutoGiveItem`, a to, co się
nie mieści, kładzie na ziemi i zgłasza sukces. Bot pytał przed skrzynią o
pięć wolnych kratek i jedno miejsce na trzy pola — dla paczki z sześciu
przedmiotów to za mało („boty mając pełne EQ otwierają skrzynie ucznia i
wszystko wylatuje na glebę”). Teraz przed każdą skrzynią — ucznia, Moonlight,
skrzynką bossa i skrzynią na klucz — bot układa całą paczkę grupy na kopii
swojej siatki tak, jak robi to silnik: przedmiot potrzebuje swojej wysokości
w jednej kolumnie jednej strony, stos dokłada się do stosu tego samego vnumu,
każda nagroda zajmuje miejsce przed sprawdzeniem następnej. Dla grupy typu
Pct (skrzynie ucznia) liczą się wszystkie linie naraz, dla pozostałych jedna,
największa. Gdy się nie mieści, skrzynia czeka (raz na minutę
`PLAYERBOT_GEAR: chest waits for room`), a plecak opróżnia wizyta w mieście.
Na mt2009 typ grupy odsłania jednolinijkowy getter dodawany przez
`playerbotify.py`; na r40250 liczone są wszystkie linie.

### Launcher mówi, ile trwała każda faza

Skarga „aktualizacja i start trwają bardzo długo” nie miała do tej pory
liczby, którą dałoby się sprawdzić: log launchera znał tylko początek i koniec
akcji. Launcher pisze teraz linie `[faza] … (+N s od początku akcji)` po
sprawdzeniu Dockera, po przygotowaniu kontekstu budowania, przy starcie i
końcu `docker compose up --build`, po pobraniu pakietu (rozmiar i czas) i po
podmianie plików; `start-server.ps1` podaje czas samego `docker compose up`,
czyli od bazy przez migrator do zdrowej gry. Okno launchera pokazuje bieżącą
fazę w pasku stanu. Zmierzone na maszynie deweloperskiej dla 2.0.5: start
zatrzymanego serwera 52 s, aktualizacja z zbudowanymi obrazami około minuty,
pełna budowa obrazu gry od zera (bez cache kompilatora) 5 min 9 s, z czego
2 min to pakiety Ubuntu i biblioteki zewnętrzne, 94 s rdzeń gry, 31 s apt
obrazu docelowego. Na czterordzeniowym laptopie należy się spodziewać trzech,
czterech razy tyle; to czas pierwszego uruchomienia, aktualizacja kompiluje
tylko zmienione pliki botów przez ccache.

---

## 2.0.5 — 2026-09-11

### Postacie GM w zestawie, który klient umie pokazać

Zbroja 90 poziomu i broń 87 poziomu z 2.0.4 istnieją w `item_proto` serwera,
ale nie w kliencie: jego `pc2/*.msm` znają kształty zbroi 0–12, 14–22 i 24
(Pancerz Diabelskiego Rogu to 13, Zbroja z Niebieskiej Stali to 26), a
pakiet `item` ma modele broni tylko do zestawu 75 poziomu. Postać w takiej
zbroi była niewidzialna i nie mogła się ruszać (Iwakura). Czwórka GM nosi
teraz zbroję 66 poziomu +9 swojej klasy (Zbroja z Czarnej Stali, Ubranie
Czarnego Wiatru, Zbroja Płytowa Czarnej Magii, Czarna Szata) i broń 75
poziomu +9 (Zatruty Miecz, Skrzydła Demona Chakram, Lwi Miecz, Bambusowy
Dzwon; w plecaku Miecz Żalu, Stalowy Łuk Kruka, Wachlarz Ośmiu Trigramów),
a do tego Olejek Wygnania do zmiany królestwa. Świat, który dostał czwórkę w
2.0.4, ma podmieniane te same sloty przy najbliższym starcie — bonusy
zostają, zmienia się tylko vnum.

### Panel: ikony przedmiotów u każdego, historia ekwipunku działa

Panel klasyczny pokazywał u każdego gracza przekreślony obrazek zamiast
ikony przedmiotu: 1572 ikony generował na 1.33 instalator na maszynie
operatora i nigdy nie były w repozytorium ani w paczce, a linia 2.x nie
generowała ich nigdzie. Ikony są teraz robione z pakietu `icon` klienta
mt2009 (`port/iconify.py`: 1557 ikon per-item z klienta plus 253 ze starego
zestawu dla przedmiotów, których klient trzyma tylko w atlasach — mikstury
startowe, księga umiejętności, skrzynie ucznia) i jadą w `files/static/icons`
(8,6 MB w każdej aktualizacji serwera). Przedmiot bez ikony (741 z 6001
vnumów świata, prawie żaden w obiegu) dostaje szare pole zamiast
przekreślonego obrazka.

„Historia ekwipunku” pokazywała `startswith first arg must be bytes…`: na
mt2009 kolumny `how`, `hint` i `type` w `log.log` są `varbinary` i przychodzą
jako bajty. Oba panele dekodują je teraz (CP1250) przed użyciem.

### Pełny plecak: bot nie udaje, że podnosi, i robi miejsce

Bot z zerem wolnych kratek podchodził do każdego dropu, pokazywał „Podnosze
lup”, dostawał odmowę od silnika i próbował następnego — a między tym
polował dalej („bot dropi, itemki pod nim leżą, mówi że podnosi łup, ale nie
robi nic, bo nie ma miejsca” — JaroszV2). Zbieracz łupu pomija teraz
przedmioty, które nie zmieszczą się w plecaku (zostaje tylko to, co dołoży
się do istniejącego stosu), a raz na minutę pisze do logu
`PLAYERBOT_LOOT: bag full pid= name= drops_in_reach= can_open_shop=`.

Handlarz może dostać ekwipunek tylko do +4 — także z reguły „niesprzedane
przez sześć straganów” (dotąd do +6). +5 i wyżej nigdy nie idzie do NPC: to
towar na stragan, a co bot nosi sam, kowal podnosi dalej. Gdy plecak jest
pod presją (8 wolnych kratek lub mniej) i bot nie może otworzyć straganu (na
mt2009 przed 15 poziomem i 800 zabiciami), ekwipunek do +4, którego sam by
nie założył, i nadwyżka materiałów idą do handlarza — wcześniej „towar na
ladę” bez lady był plecakiem na zawsze.

---

## 2.0.4 — 2026-09-11

Czwarta poprawka pierwszego dnia: cztery wyposażone postacie GM i skrzynia
startowa dla gracza. Tylko serwer (ZAINSTALUJ AKTUALIZACJE); klient bez zmian.

### Cztery postacie GM z pełnym wyposażeniem

Zamiast jednej postaci Admin konto `admin` ma cztery, po jednej na klasę:
**Admin** (wojownik), **AdminNinja**, **AdminSura** i **AdminSzaman**, każda
90 poziomu z uprawnieniami IMPLEMENTOR. Każda nosi najlepszy zestaw +9, jaki
ma ta paczka dla jej klasy (broń 87 poziomu, zbroja 90, hełm 80, Tarcza
Tytanów, granatowa biżuteria, buty 65) z mocnymi bonusami — w tym linią
średnich obrażeń 45% i obrażeń umiejętności 20% na broni — a w plecaku drugą
broń (dwuręczna, łuk ze strzałami, wachlarz), po 200 mikstur XL, zwoje
błogosławieństwa, Boga Smoków i teleportu, medale konne, 500 milionów yang,
konia 21 poziomu i Księgę Wojskowego Konia do jego przywołania. Postacie
powstają na świeżym świecie, a na istniejącym przy najbliższym starcie, o ile
konto `admin` nie ma jeszcze żadnej postaci; konto, na którym ktoś już gra,
zostaje jak było (jego pierwsza postać dostała GM w 2.0.2). PID-y 9001–9004,
poza zakresem seeda botów.

### Nowa postać gracza dostaje Skrzynię Ucznia

Boty zaczynają ze Skrzynią Ucznia I w plecaku, gracz zaczynał z niczym
(„gracz jest gorszy niż bot” — sizowski). Quest `starter_chest` daje ją przy
pierwszym wejściu do gry postaci do 5 poziomu, według klasy (wojownik i sura
50187, ninja 50212, szaman 50213) — dalsze skrzynie łańcucha otwierają się jak
u botów, od 10 poziomu. Bot do 5 poziomu też dostanie jedną przy najbliższym
logowaniu; quest nie odróżnia bota od gracza, a to garść skrzyń więcej.

---

## 2.0.3 — 2026-09-11

Trzecia poprawka pierwszego dnia: konto premium dla botów i tooltipy dla
GM-a. Serwer: ZAINSTALUJ AKTUALIZACJE; klient: AKTUALIZUJ KLIENTA (pakiet
`root`).

### Każdy bot ma konto premium

Ten silnik ma abonament premium (50% więcej doświadczenia, podwójna szansa
na drop przedmiotów i yang, automatyczne podnoszenie yang, dodatkowa strona
magazynu, więcej miejsc w sklepie, lepsze wyławianie ryb). Bot nie loguje się
przez serwer auth, więc nigdy go nie miał. Od tej wersji każdy bot — obecny
i każdy nowy — dostaje abonament przy wejściu do gry, bez wpisów w bazie:
premium siedzi w tabeli konta deskryptora, z której silnik czyta je przy
ładowaniu postaci (`SetPlayerProto`), i stamtąd odpowiadają wszystkie
sprawdzenia `GetPremiumRemainSeconds`. Ważne pięć lat od każdego startu.

### GM widzi tooltipy przedmiotów

Postać z uprawnieniami GM nie widziała nazwy ani statystyk żadnego
przedmiotu po najechaniu („Nie widać nazw itemów” — davids998; „tylko gdy
jesteś GM” — sizowski). Gałąź GM w tooltipie klienta iteruje po
`auxiliaryDict.items()`, a `auxiliaryDict` jest w tym roocie pustym napisem
(jego przypisanie jest wykomentowane), więc każdy tooltip umierał na
`AttributeError`, zanim został pokazany. Gałąź jest teraz strzeżona; GM
widzi zwykły tooltip plus linie VNUM/TYPE/SOCKET. Zmiana w kliencie:
AKTUALIZUJ KLIENTA.

---

## 2.0.2 — 2026-09-11

Druga poprawka z pierwszego dnia: zgłoszenia z Discorda po południu, nowa
pula nicków i aktualizacja klienta (przycisk „AKTUALIZUJ KLIENTA” w
launcherze — dwa pliki pakietu `root`).

### Konto admin ma postać GM

Paczka mt2009 przychodzi z pustą listą GM i bez żadnej postaci na koncie
`admin`, więc kto zalogował się na konto testowe, dostawał zwykłego gracza
(archded: „loguję się admin admin, a tam nie ma postaci GM”). Świeży świat
tworzy teraz na tym koncie postać **Admin** (wojownik 90 poziomu, stoi w
Joan) z uprawnieniami IMPLEMENTOR. Świat założony na 2.0.0 lub 2.0.1 dostaje
to przy najbliższym starcie: pierwsza postać konta `admin` zostaje GM (jeśli
na koncie nie ma jeszcze postaci — pierwsza, którą utworzysz, po kolejnym
starcie). Nadanie jest jednorazowe i tylko na pustej liście; świat, który
kiedykolwiek miał własnego GM, zostaje jak był.

### „Doładuj SM!” prowadzi na buycoffee, nie na mt2009.pl

Przycisk „Doładuj SM!” w sklepie z przedmiotami i przycisk w oknie
abonamentu otwierały stronę publicznego serwera mt2009 (sizowski, matthaeu;
„powinno przenosić na buycoffee” — kuszaa). Oba prowadzą teraz na
https://buycoffee.to/metin2-playerbots, a „Wsparcie” w menu systemowym na nasz
Discord. To zmiana w kliencie: w launcherze kliknij „AKTUALIZUJ KLIENTA”.
Smoczych Monet na tym serwerze nikt nie sprzedaje — dopisujesz je sobie w
bazie (`account.account.cash`).

### Boty nie oglądają straganów, których nie ma

Po załatwieniu spraw w mieście bot chwilę zostaje na placu, a nad głową
miał „Ogladam stragany” — także na świecie, na którym żaden bot nie dorósł
jeszcze do otwarcia sklepu („jakie stragany oglądają jak żadnego nie ma” —
.christoff). Bez lady na mapie mówi teraz „Odpoczywam w miescie”. To samo z
zakupami: wyprawa na targ („Szukam czegos na straganach”) rusza tylko wtedy,
gdy na tej mapie albo w pierwszej wiosce królestwa stoi jakiś stragan;
wcześniej bot szedł na pusty pitch i przeglądał powietrze.

### Panel: ranking nie wisi na „Ładowanie…”

Lista rankingu w panelu klasycznym zmieniała się tylko po udanej odpowiedzi
API, a pierwsze pobranie ruszało dopiero po pozycjach botów — na świecie,
który jeszcze nie miał botów w grze, albo po błędzie API napis „Ładowanie…”
zostawał na zawsze (davids998). Odmowa API jest teraz wypisywana w miejscu
listy, a ranking pobiera się od razu.

### Nicki botów: lista Iwakury, w całości i tylko ona

Iwakura spisał 1000 nicków z polskich serwerów z lat 2010–2012 („tu jest
postaranie”). Ta lista jest teraz jedyną pulą: znika poprzednia (lista
jaksiezabica, pierwsza lista Iwakury i nicki składane z ich słów) i znika
dopasowanie do klasy i płci — nazwy idą po kolei według PID, jak zostały
napisane. Każdy bot, który nosił nick ze starej puli, dostaje nowy przy
najbliższym starcie serwera (pula ma wersję; historia w
`common.playerbot_name_history` pamięta nazwę z seeda, więc „restore” dalej
działa). Gdy botów jest więcej niż nazw, lista idzie drugi raz z „v2” na
końcu, potem „v3”; Iwakura dopisze kolejne nicki w tygodniu. Odrzucone: dwa
wulgarne, dwa udające GM i pięć złożonych z samych cyfr. Podkreślniki są
wycinane, bo klient ich nie przyjmuje.

---

## 2.0.1 — 2026-09-11

Pierwsza poprawka linii 2.x, z pierwszego dnia instalacji u graczy.

### Budowa nie stoi już na `deps 7/7`

Pierwsza budowa pobiera pakiety Ubuntu, a archive.ubuntu.com serwowało dziś z
Polski kilka kilobajtów na sekundę: 10 MB szło kilkanaście minut, budowa
„wisiała” na `deps 7/7` po 20–40 minut (jaksiezabic, drip9660, matthaeu,
u4nt). Wpisanie w `.env` polskiego mirrora skracało to do dwóch minut
(piotrekf3). Ten mirror (`http://ubuntu.task.gda.pl/ubuntu/`) jest teraz
domyślny: pusty `M2_APT_MIRROR` w istniejącym `.env` też go dostaje, więc
nikt nie musi niczego edytować. Inny kraj wpisuje bliższy mirror w
`M2_APT_MIRROR`.

### Nazwy straganów od społeczności

Iwakura spisał nazwy sklepów z polskich serwerów z lat 2010–2012, z podziałem
na kategorie. Stragan botów ma teraz szyld według tego, co przeważa na ladzie:
ryby i małże („Sklep Rybny u Janusza ;]”, „Malze po 3000 sztuka!”), księgi
umiejętności („Makulatura po dziadku”), ulepszacze („Zolc i skora
niedzwiedzia”), ekwipunek („Zbroje na chude klaty”), medale konne, zwoje
błogosławieństwa, kamienie duchów — a stragan mieszany bierze jedną z
kilkudziesięciu uniwersalnych („TANIEJ NIZ OBOK >>>>>”, „ZBANUJCIE TE
BOTY!!!”). Broń 30 poziomu i duży ulepszony przedmiot dalej idą na szyld pod
własną nazwą. Szyld zmienia się z każdym kolejnym straganem tego samego bota.

### Stragan dopiero od 15 poziomu i 800 zabitych potworów

Ten silnik pozwala otworzyć sklep dopiero od 15 poziomu i 800 zabić
(`CanOpenShop`); młodszy bot dostawał od silnika odmowę przy każdej próbie —
39 z rzędu w młodym świecie. Bot pyta o to przed wyjściem na pitch i wraca po
dziesięciu minutach.

---

## 2.0.0 — 2026-09-11

**Nowa linia serwera: pliki serwerowe mt2009 (Martysama r41023) zamiast r40250.**
To osobna, pełna paczka — klient i serwer razem — i osobny kanał aktualizacji.
Instalacja 1.33.x niczego z tej wersji nie dostanie i nie powinna: stary
launcher i stary klient nie pasują do nowych plików. Pełną paczkę pobierasz z
Discorda (discord.gg/pt5tvnrN6), a od tej chwili aktualizacje serwera i klienta
przychodzą już przez launcher, jak dotąd.

### Podziękowania

To wydanie powstało dzięki ostatnim wpłatom. Bez nich nie byłoby czasu ani
środków na to, co się tu wydarzyło: dziesiątki tysięcy linii kodu, zmiana
architektury serwera i zbudowanie jej od nowa na innym silniku — przy
zachowaniu wszystkiego, co boty i panele potrafiły do tej pory. Dziękuję.
Wsparcie projektu: buycoffee.to/metin2-playerbots.

### Serwer

- Silnik mt2009 przeniesiony na Linuksa i budowany w Dockerze tak jak
  dotąd: cztery etapy obrazu, protosy w bazie (`PROTO_FROM_DB`), sześć baz na
  MariaDB 11.8, te same porty, ten sam launcher.
- Port jest zapisany jako skrypty, nie jako ręczne poprawki: każdy plik
  silnika, bazy, compose i share, który różni się od paczki, jest renderowany
  z oryginału r40250 albo z pakietu i da się odtworzyć jedną komendą.
- Świat startuje otwarty. Pakiet uruchamiał każdy serwer w „przerwie
  technicznej” do czasu, aż GM wpisze `/maintenance 0`; zwykłe konto widziało
  na ostatnim kroku logowania „Obecnie trwa przerwa techniczna”.
- Schemat logów uzupełniony o 23 tabele, które silnik zapisuje, a których
  pakiet nie miał; `account.social_id` poszerzony do 18 znaków (identyfikatory
  botów były ucinane); kolumny `mileage`/`jackpot` dla ItemShopu.
- Skrzynie startowe botów, skrzynia księżycowa i drop broni 30 poziomu na
  mapie gildii dopisane do plików pakietu; brakujący przedmiot w takiej
  grupie wywraca cały plik przy starcie, więc render sprawdza vnumy względem
  zrzutu świata.

### Playerbots

- Cała logika botów bez zmian w zachowaniu. Różnice silnika są zamknięte w
  jednym nagłówku (`playerbot_engine_compat.h`): numeracja bonusów
  (`POINT_*` zamiast `APPLY_*`), nazwy afektów, złoto (`ChangeGold` zamiast
  `PointChange`, który tu jest odrzucany), zakładanie przedmiotów, flagi
  specjalne postaci, wędkowanie.
- Zakładanie ekwipunku: mt2009 odpowiada na `CanEquipNow` sześć razy na pół
  sekundy na postać, a przebieg ekwipunku pytał o każdy przedmiot w torbie —
  63 tysiące odrzuconych ulepszeń na godzinę. Zegar jest zerowany przed
  każdym pytaniem; po poprawce 6940 założeń i 213 odmów w tym samym czasie.
- Flagi specjalne postaci (statystyki `stat_*`) ładują się dla botów: pakiet
  ładowania bota niesie id konta, bez którego rdzeń db odpowiadał flagami
  wszystkich botów naraz i gra je odrzucała.
- Panele klasyczny, Sebana i ItemShop działają na obu silnikach z jednym
  przełącznikiem (`M2PANEL_ENGINE` / `PLAYERBOTS_ENGINE`); ranking broni
  liczy właściwe linie obrażeń.

### Klient

- Klient w paczce jest gotowy do gry na tym komputerze (127.0.0.1); launcher
  znajduje go sam w folderze `Klient` obok `Serwer` i nie pyta o plik EXE.
- Własny regulamin w oknie logowania: co to za projekt, zasady, wsparcie na
  buycoffee, Discord, prywatność. Przyciski okna logowania prowadzą do
  GitHuba projektu, buycoffee i naszego Discorda.
- Narzędzie do packów klienta rozumie układ PackMakerLite: ten klient
  sprawdza CRC32 każdego pliku typu 2 i root spakowany po staremu nie
  uruchamiał gry (`RunMain Error`).

### Launcher i paczka

- Jeden zip na hosting (`Metin2-Singleplayer-2.0.0.zip`): `Klient\`,
  `Serwer\` i `CZYTAJ.txt`. Bez haseł, tożsamości instalacji, logów i kopii —
  launcher tworzy je przy pierwszym starcie.
- Launcher rozpoznaje silnik po pliku `linux-port\docker\ENGINE`: własny
  kanał aktualizacji (`update-manifest-mt2009.json`), lista zrzutów, brak
  łatek r40250, lista modułów kontekstu budowania.
- Adopcja istniejącej instalacji Dockera tylko tego samego silnika: kto
  przechodzi z r40250, ma na dysku jedną instalację — starą — i launcher
  podpiąłby nowy serwer pod jej bazę. Teraz stara instalacja zostaje obok,
  nietknięta, a nowa dostaje własną.
- Przycisk „PANEL GM F9” na tej linii to zwykłe „AKTUALIZUJ KLIENTA”.

### Czego jeszcze nie ma

- Panel GM F9 (część serwerowa i kliencka) — celowo pominięty, żeby nie
  kolidować z klientem; wróci osobno.
- Misje poziomowe botów: ten pakiet nie ma questa `levelup`, boty pomijają
  ten etap rozwoju.
- Wędkowanie: silnik wymaga 50 poziomu i przepustki wędkarskiej; boty na razie
  nie łowią.

---

## 1.33.3 — 2026-09-11

Poranek po nocy zgłoszeń: siedem poprawek, każda sprawdzona na żywym serwerze
testowym. Tym razem **zmienia się też klient** — paczka klienta 1.33.3.

### Panel GM nie może już zatrzymać ładowania gry

Po zainstalowaniu panelu GM u części graczy pasek ładowania stawał na 100% i
gra nie wchodziła (Dixdros, jaroszv2, .unright, ligivanestrea). Okno panelu
było budowane bezwarunkowo w środku budowy całego interfejsu, więc jakikolwiek
wyjątek w nim — kontrolka, której nie ma w innej wersji klienta, klucz, którego
nie ma w innym pakiecie językowym — przerywał budowę wszystkiego. Stockowy root
wchodził na tych samych maszynach.

Teraz panel buduje się w osobnym bloku: jeśli się nie zbuduje, gra wchodzi bez
niego, a powód trafia do `syserr.txt` w folderze klienta. F9 mówi wtedy na
czacie, że panelu nie ma i gdzie szukać przyczyny. **Sama przyczyna jest wciąż
nieznana** — nikt jeszcze nie przysłał `syserr.txt`. To zabezpieczenie zamienia
„gra nie wchodzi" w zgłoszenie, które niesie odpowiedź.

### Pełny kanał przy 2500 botach

Lista kanałów pokazywała FULL, a klient odmawiał wejścia, gdy botów było więcej
niż 1200 — silnik liczył boty jak graczy, i przy statusie kanału, i przy limicie
logowania. Boty nie są graczami w tej rachubie: liczone są tylko postacie ludzi,
także te zalogowane na innych rdzeniach. Rdzeń wypisuje co pięć minut
`CHANNEL_STATUS: players=… status=…`, więc następny zrzut ekranu będzie miał
liczbę.

### Boty przestały grzęznąć w M3

Po wejściu na mapę gildii bot lądował w punkcie (179500, 1000) — to komórka
(3, 10) mapy, nieprzechodni północno-zachodni róg. Nie miał trasy do niczego,
watchdog resetował go co półtorej minuty w tym samym miejscu i nic nie mogło go
ruszyć; teleport z panelu też nie, bo bot nie ma klienta, który mógłby przeżyć
zmianę mapy po stronie klienta. Zgłoszone z logiem przez greessa, potwierdzone
przez dwie kolejne osoby.

Ten punkt pochodził z tabeli questu Teleportera; stała w kodzie miała już od
dawna właściwy, z `Town.txt` mapy. Tabela dla trzech królestw przywróciła stary
numer. Wszystkie trzy mapy gildii lądują teraz na własnym punkcie z `Town.txt`
i test jednostkowy to przypina. Sprawdzone: ten sam bot, który rano utknął w
rogu, po poprawce wylądował na (221900, 9200), zaatakował i dosiadł konia.

### Koreański tekst zamiast „nie straciłeś doświadczenia"

Jeden plik silnika trafił do paczki przekodowany na UTF-8. Gra szuka
komunikatów po ich koreańskim kluczu, bajt w bajt, a pliki tłumaczeń są w
kodowaniu koreańskim — więc trzynaście komunikatów z tego pliku nie trafiało w
tłumaczenie i wracało po koreańsku, a każda śmierć z błogosławieństwem Boga
Smoków zostawiała błąd w logu (276 w jednym zgłoszeniu). Od 1.31.6. Plik wrócił
do właściwego kodowania; po restarcie zero takich błędów.

### Wędkarze nie rozrzucają ryb po trawie

Złowiona ryba idzie do plecaka funkcją, która pełnego plecaka nie odmawia —
kładzie rybę na ziemi i zgłasza sukces. Stąd zdjęcie od bierzyna: wędkarze
„upuszczają drop", a potem wszystkie boty biegną po niego. Sesja wędkowania
kończy się teraz, gdy w plecaku nie ma wolnego pola, i bot idzie go opróżnić.

### Nicki botów: 1000 od Iwakury, klasa i płeć się zgadzają

Iwakura przysłał tysiąc nicków z prośbą, żeby były „bardziej różne". Po
odsianiu duplikatów i tego, czego silnik nie przyjmie, 747 nowych trafiło do
puli. Reszta puli jest składana ze słów wyjętych z obu list i w ich kształtach —
poprzednia wersja składała wszystko w jednej gramatyce z czterdziestu słów.

Nick, który mówi o klasie — Włócznia, Szaman, Sura, Woj, Fms, Ninja — trafia do
bota tej klasy, a nick wyraźnie kobiecy — Szamanka, Królowa, Marysia — do postaci
kobiecej. „Ninja szamanka, ale to sura" było prawdziwym zrzutem ekranu. Zmierzone
na 2500 botach: wojownicy dostali wojownicze nicki w 80%, nicki kobiece trafiły
wyłącznie na postacie kobiece. Boty, które nick już mają, zachowują go.

### Martwy towar w końcu idzie do handlarza

Reguła „czego nikt nie kupił przez sześć stoisk, to złom" stała pod regułą „+4
i wyżej nigdy do NPC", więc nigdy nie dotyczyła tego, co stragan naprawdę
trzyma. Plecak pełen +5, których nikt nie kupuje, był plecakiem na zawsze — i to
jest bot, który „siedzi w M1, ogląda stragany i od wczoraj nie wbił poziomu".
Reguła działa teraz do +6; +7 i wyżej nadal nigdy nie idzie do NPC.

### ItemShop: moneta i zdrowie kontenera

Adres sklepu z gry miał na końcu losowy bajt zamiast kodu kraju, bo zmienna nie
była zainicjalizowana dla locale spoza listy silnika — Apache odpowiadał 400,
zanim PHP w ogóle wystartowało (archded, z logiem). Ma teraz wartość domyślną.
Do tego błąd PHP zwraca 500 zamiast 200 z treścią błędu, więc healthcheck
przestaje nazywać zepsuty sklep zdrowym, a wbudowana przeglądarka klienta nie
zapamiętuje strony błędu na stałe.

---

## 1.33.2 — 2026-09-11

Trzy zgłoszenia z Discorda z jednego wieczoru. Wszystkie sprawdzone odtworzonym
błędem, nie z lektury kodu. Klient bez zmian — aktualizuje się tylko serwer.

### Suwak liczby botów wreszcie znaczy to, co pokazuje

Suwak w launcherze sięga 2500 i tak jest podpisany, a rdzeń gry ścinał tę liczbę
do 1000. Kto ustawił więcej, dostawał dokładnie tysiąc botów i nigdzie nie było
powiedziane dlaczego — ani w logu, ani w panelu.

Sufit w rdzeniu to teraz też 2500, a gdy liczba z `.env` jest większa, rdzeń
wypisuje, co uciął. Prawdziwym ograniczeniem nigdy nie był ten sufit, tylko liczba
tożsamości botów w bazie: prośba o 3000 daje na naszym świecie 2012 botów
(shinsoo 500, chunjo 1012, jinno 500) i tak ma być.

**Zmiana suwaka działa dopiero po restarcie serwera** — rdzeń czyta tę liczbę raz,
przy starcie. To było prawdą od zawsze, ale nigdzie nie napisane.

Zmierzone przy pełnej kohorcie: 2004 boty w świecie, 33 sekundy rdzenia na minutę
dla całego kontenera gry — niecałe 0,6 jednego rdzenia. Rozdzielenie botów między
trzy królestwa jest tym, co czyni to tanim: królestwo z mapami współdzielonymi
kosztuje czterokrotnie tyle, co królestwo wioskowe.

### Launcher mówi, dlaczego nie mógł zapisać pliku klienta

„Odmowa dostępu do ścieżki" to zdanie, którym Windows opisuje co najmniej cztery
różne problemy, a launcher przepisywał je bez zmian. Gracz, który próbował
zainstalować panel GM dziewięć razy w ciągu dnia, dziewięć razy dostawał to samo
zdanie i nie miał z czym pójść dalej.

Teraz launcher sprawdza i nazywa przyczynę: proces trzymający plik, atrybut
tylko-do-odczytu (zdejmowany automatycznie), Ochrona folderów w Windows
Defenderze, albo uprawnienia NTFS — z rozróżnieniem, czy nie da się pisać do
całego folderu, czy tylko do jednego pliku.

Przy okazji wyszło coś gorszego: wycofywanie nieudanej aktualizacji przywracało
kopię zapasową na ten sam plik, którego przed chwilą nie dało się zapisać,
dostawało tę samą odmowę — i to jej komunikat wychodził na wierzch, kasując
właściwą diagnozę. Wycofywane jest teraz wyłącznie to, co naprawdę zapisano,
a żadne przywracanie nie może już przesłonić przyczyny.

**Uwaga dla graczy z panelem GM na F9:** przycisk PANEL GM to po prostu
aktualizacja klienta. Jeśli wyskakuje odmowa dostępu, to nie panel jest zepsuty,
tylko launcher nie może podmienić dwóch plików w folderze gry.

### Zaawansowany panel mówi, że tabela jest uszkodzona

Panel odpowiadał samym „Internal Server Error", bez żadnej wskazówki. Pod spodem
były uszkodzone tabele bazy `log` — silnik gry używa MyISAM, a te nie przeżywają
nagłego zatrzymania: wystarczy zamknięcie Dockera w trakcie zapisu albo zanik
zasilania.

Stąd bardzo mylący objaw: **zwykły panel działa, a zaawansowany nie** — ten drugi
czyta tabelę logów na samej stronie głównej, do rankingu wędkarzy. I stąd druga
myląca rzecz: **aktualizacja tego nie naprawia**, bo uszkodzenie jest w danych na
dysku, a nie w programie.

Panel pokazuje teraz stronę, która nazywa uszkodzoną tabelę, podaje gotową
komendę naprawy, mówi wprost, że aktualizacja nie pomoże, i wskazuje opróżnienie
tabel `log` jako ostateczność — to wyłącznie historia, gra jej nie czyta i żadna
postać ani przedmiot od niej nie zależą. Inne błędy bazy przechodzą dalej bez
zmian, żeby ta strona nie zasłaniała prawdziwych awarii.

---

## 1.33.1 — 2026-09-10

Poprawki z audytu zgłoszeń z Discorda. Wszystkie sprawdzone na żywym serwerze
testowym, nie tylko skompilowane.

### Masowe nadawanie przedmiotów gubiło sztuki i mówiło, że się udało

Panel przyjmował do 65535 sztuk, a silnik czyta tę liczbę jako jeden bajt:
300 stawało się 44, 256 stawało się zerem, 65535 stawało się 255. Nikt się o tym
nie dowiadywał, bo paczka kończyła się statusem „Nadano”.

Prawdziwy sufit jednego wydania to 200 sztuk — pełny stos, i tyle silnik potrafi
dodać za jednym razem. Powyżej tej liczby panel odmawia teraz wprost, a po
wydaniu porównuje zawartość plecaka przed i po, bo samo „silnik coś zwrócił” nie
dowodzi dostarczenia: przy braku miejsca ten sam kod kładzie przedmiot na ziemi
i też zgłasza sukces.

**Do wiadomości operatorów:** kto dotąd wpisywał 300, dostawał 44 i widział
„Nadano”. Teraz zobaczy czytelną odmowę. Limit 200 na jedno żądanie jest nowy.

### Nagrody ze skrzyń przestają lądować na ziemi

Bot sprawdzał miejsce w plecaku dwoma pytaniami o to samo pole — silnikowa
funkcja zwraca *pozycję* wolnego miejsca, a nie ich liczbę, więc dwa wywołania
obok siebie mogą wskazać tę samą kratkę i nie rezerwują niczego. Skrzynia wydaje
nagrody po kolei, a to, co się nie mieści, spada na trawę. Teraz liczone są
rzeczywiste wolne pola i skrzynia otwiera się dopiero, gdy jest ich pięć.

To zabezpieczenie, nie pełne rozwiązanie: docelowo zestaw nagród trzeba wylosować
raz, sprawdzić miejsce na całość i dopiero potem zużyć skrzynię, a to zmiana
w silniku, która musi dostać własny tryb „do plecaka albo wcale”, żeby nie ruszyć
nagród graczy.

### Bot przestał chodzić do kowala po nic

Planer i wykonawca oceniały zawartość plecaka inaczej: planer przyjmował wszystko,
co bot potrafi założyć, a wykonawca odrzucał to, co reguła złomu przeznaczyła dla
handlarza. Rozpoczęta wizyta w mieście jest zobowiązaniem, którego planer nie
cofnie, więc bot szedł przez pół mapy i wracał z niczym — zgłoszone jako „mam
wszystko +9 założone, a bot dalej lezie do kowala”. Obie strony pytają teraz
jednej funkcji.

### Świeża instalacja bazy nie zbuduje się już w połowie

Skrypt startowy bazy sprawdzał istnienie plików z danymi świata, a potem próbował
je przeczytać kontem `mysql` przez katalog wpięty z dysku hosta. Paczka
rozpakowana pod ścisłą maską uprawnień na Linuksie dawała prawa, których to konto
nie miało — import przerywał się w połowie, już po utworzeniu baz i użytkownika,
a katalog danych przestawał być pusty. Ten krok wykonuje się raz na wolumen
i nigdy więcej, więc świat zostawał na stałe w połowie zbudowany, za zdrowym
healthcheckiem.

Każdy z pięciu plików jest teraz czytany na jeden bajt, zanim powstanie
cokolwiek trwałego, a komunikat nazywa po imieniu plik brakujący, nieczytelny
i pusty. Instalator Linuksa przy okazji normalizuje uprawnienia całego kontekstu
budowy, żeby ta sytuacja nie powstała.

### Sklep z przedmiotami odpowiadał 403

`COPY` w obrazie zachowuje uprawnienia źródła, więc pliki sklepu skopiowane
z katalogu o ścisłych prawach były nieczytelne dla serwera WWW w środku
kontenera. Uprawnienia są teraz normalizowane w obrazie.

### Migrator bazy przestał czekać pół godziny na złe hasło

Błędne hasło do bazy jest odpowiedzią ostateczną, a nie chwilową niedostępnością
— migrator ponawiał je przez trzydzieści minut i kończył komunikatem o czasie
oczekiwania, który niczego nie tłumaczył. Po pięciu odmowach uwierzytelnienia
z rzędu kończy teraz pracę i pisze, co jest nie tak. Zmierzone: 9 sekund zamiast
pół godziny.

### Rankingi w panelach pokazywały nie to, co obiecywały

Trzy osobne błędy w tej samej okolicy:

- **Obrażenia od umiejętności i średnie były zamienione miejscami.** Numery 71
  i 72 stały odwrotnie w obu panelach, w panelu zaawansowanym w trzech miejscach
  naraz — w tym w nazwach kolumn, po których sortuje baza, więc pierwsza setka
  wyników była wybierana według niewłaściwej wartości.
- **Ranking +9 nie widział tarcz ani biżuterii.** Filtr odcinał wszystko powyżej
  pewnego numeru przedmiotu, co miało wykluczyć materiały, a wykluczało również
  tarcze. Panel pyta teraz bazę, co jest wyposażeniem: 17 przedmiotów zamiast 9.
- **Ranking umiejętności brał pod uwagę tylko czterystu najwyższych poziomem.**
  Bot trzydziestego poziomu z mistrzowską umiejętnością nie miał szans się
  pojawić. Liczony jest cały zbiór.

Do tego dwie osobowości botów były podpisane nawzajem: handlarz jako wędrowiec
i odwrotnie.

### Czego to wydanie nie naprawia

- **Nazwy przedmiotów w historii ekwipunku.** Tabela logów jest zadeklarowana
  w chińskim kodowaniu, a gra pisze do niej po polsku. 128 573 z 279 242 wpisów
  spoza ASCII ma w tym miejscu nieodwracalny znak zapytania — zmiana deklaracji
  naprawi to, co zostanie zapisane dalej, i nie odzyska niczego. Tabela ma
  22 miliony wierszy i 1,75 GB, więc każda zmiana to minuty przerwy w działaniu.
  To zaplanowana migracja z kopią zapasową, a nie poprawka przy okazji. Dotyczy
  wyłącznie historii — nic w grze tej kolumny nie czyta.
- **Cztery zgłoszenia czekają na powtórzenie:** wyjątki suwaka straganów, bot
  krążący po pierwszej wiosce mimo celu na pustyni, boty zamarzające lub
  znikające, oraz cenne przedmioty innych klas zapychające plecak. Każde z nich
  wymaga najpierw diagnostyki, nie ślepej poprawki.

---

## 1.33.0 — 2026-09-10

### Gra znowu jest po polsku po każdej aktualizacji

Przełącznik języka podmienia cztery pliki w `share/`: nazwy przedmiotów, nazwy
potworów i dwa pliki tłumaczeń. `share/` jest wpieczone w obraz gry, a każda
aktualizacja ten obraz przebudowuje — więc angielskie oryginały wracały na
miejsce, podczas gdy zapamiętany wybór, strona języka w panelu i status dalej
mówiły „polski". Świat był nazwany po połowie w każdym języku: nasze polskie
napisy obok „Skill Book" na szyldzie straganu, misje i potwory po angielsku.

Kontener nakłada teraz zapamiętany język przy każdym starcie. Jedno uruchomienie
naprawia instalację, która dziś jest po angielsku — nie trzeba niczego migrować.

### Hasło do panelu przestało być tajemnicą przed właścicielem serwera

`M2_PANEL_PASSWORD` bywało puste — plik `.env` przepisany z przykładu, przerwana
instalacja, serwer odpalony samym `docker compose`. Panel wymyślał wtedy
dwudziestoznakowe hasło, zapisywał sam jego skrót i wypisywał je raz do logu
kontenera, którego nikt nie czyta. Od tej chwili panel miał hasło, które nie
istniało nigdzie.

Launcher wypełnia teraz tę lukę, zanim Docker ją zobaczy, i pokazuje hasło.
A przycisk OTWÓRZ PANEL WWW ma trzecią pozycję — „Nie mogę się zalogować" —
która pokazuje hasło z `.env` i proponuje reset, gdy panel pamięta starsze.
Baza, świat, postacie i boty nie są tym ruszane.

### Strona panelu domyślnie po polsku

Panel pytał najpierw przeglądarkę i spadał na angielski, więc Polak na
angielskim Windowsie dostawał angielski. To, co widział, było gorsze od obu
języków osobno: ten panel jest przetłumaczony w połowie, więc angielska strona
to polska strona z dziurami. Teraz domyślny jest polski, a wybór z przełącznika
w nagłówku jest pamiętany przez rok.

### Boty noszą ludzkie nicki

Trzy tysiące sześćset nicków: 1491 napisanych przez jaksiezabica dla tego
serwera, reszta złożona z tego samego słownictwa. Bot utworzony minutę wcześniej
dostaje nick na tym samym starcie. `M2_PLAYERBOT_HUMAN_NAMES`: 1 nadaje
(domyślnie), 0 nie rusza niczego, „restore" przywraca stare nazwy — stara nazwa
jest zapamiętana, więc to odwracalne.

Nazwa jest jedyną częścią tożsamości bota, od której nic nie zależy: rdzeń
dopasowuje postać po loginie konta, a oba panele robią to samo. Nasiona wymagały
poprawki w pięciu miejscach, żeby przemianowany bot nadal był ich botem.

### Bonusy wyceniane według map i slotów

Linia „silny przeciwko orkom" mnoży cały atak przeciwko każdemu potworowi tej
rasy. Pas ekwipunku wyceniał ją wysoko, a pas mixowania na jeden punkt — bot
kupował tarczę dla tej linii i zrzucał ją u pierwszego kowala. Teraz obie strony
pytają o to samo i skalują udziałem rasy w mapie, zmierzonym po wszystkich
punktach odrodzenia: Dolina Orków to 63% orków, każda druga wioska 100% ludzi,
Lochy Małp 100% zwierząt, Sohan 46% nieumarłych. Na Pustyni i w obu Lochach
Pająków żadna taka linia nie działa i bot już o tym wie.

Druga rzecz: reguła „przedmiot skończony" pytała hełm o życie i wartość ataku, a
kolczyki o życie i krytyczne — żadna z tych linii nie może wypaść na tych
slotach, więc hełm, kolczyk i bransoleta były mixowane bez końca. Każdy slot ma
teraz warunek z linii, które na nim faktycznie wypadają.

### Flaga królestwa przy nicku bota

W obu panelach. Przy okazji wyszło, że konta botów Shinsoo i Jinno twierdziły,
że są z Chunjo — nasiona wpisywały tam dosłownie 2 dla całej kohorty. Naprawione
także dla kont już założonych.

### Kopia świata w launcherze

Przycisk KOPIA ŚWIATA: zapisz, przywróć, zacznij od zera. Kopia to pięć zrzutów
SQL, plik z datą i liczbą postaci oraz jeden zip w folderze `backups`.
Przywracanie zapisuje najpierw obecny świat do własnej kopii. Reset odmawia,
dopóki pliki, z których powstaje nowa baza, nie leżą na dysku.

Przy okazji: obietnica z okna importu — „kopia trafi do folderu backups" — nie
była prawdziwa. Sonda sprawdzająca istnienie bazy miała cudzysłów w miejscu, w
którym PowerShell go nie przepuszcza, więc folder z kopią był pusty przy każdym
imporcie, jaki ktokolwiek wykonał.

### Panel GM (F9): osiem nowych komend

Nowa wersja panelu OskarPWA dostała stronę serwera: losowe bossy i metiny (pula
metinów budowana z plików serwera, nie z zaszytej listy), marmur przemiany,
siedemnaście suwaków AI zapisywanych do tego samego pliku co panel webowy, trzy
raty i restart. Klientowa połowa jedzie w paczce klienta tej wersji.

---

## 1.32.5 — 2026-09-10

### Suwak straganiarzy znowu coś znaczy

Bot z sześcioma nadmiarowymi książkami umiejętności otwierał stragan
bezwarunkowo, z pominięciem suwaka handlu — i było to w kodzie zapisane jako
zamierzone. Rzecz w tym, że książki po kilku godzinach polowania na metiny ma
praktycznie każdy świat, więc ta jedna reguła decydowała o udziale straganiarzy,
a suwak nie ruszał niczego. Zgłosił Shenyo: 180 straganów na 288 botów przy
suwaku ustawionym na minimum.

Zmierzone u nas: 238 botów z 970 ma sześć lub więcej nadmiarowych książek, czyli
czwarta część populacji kwalifikowała się bez względu na ustawienie. Teraz i ta
reguła pyta o wagę handlu. Przy ustawieniu domyślnym zachowanie jest takie jak
dotąd — sprawdzone po wdrożeniu — a przy minimum stragany faktycznie przestają
powstawać.

### Panel nie każe instalować czegoś, czego tu nie ma

Konsola ustawień w panelu zaawansowanym pokazywała ostrzeżenie „Zainstaluj
integrację `m2-server-settings` i `m2-supervise`". Ta wersja serwera takiej
integracji nie zawiera i nie potrzebuje: restart serwera oraz zmiana rat działają
bez niej i zawsze działały. Niedostępna jest wyłącznie zmiana respawnów map, i
tylko to komunikat mówi teraz.

---

## 1.32.4 — 2026-09-10

### Boty oddają wreszcie okazy Biologowi

Bot z dwoma Zębami Orka w plecaku przechodził obok Biologa i je zatrzymywał.
Próg czterech sztuk, który miał sens jako powód do wyprawy z frontu do M1,
stosował się też do bota, który już stoi w swojej wiosce — a quest przyjmuje
jedną sztukę na raz, więc nie było na co czekać.

Zmierzone przed poprawką: 656 botów od trzydziestego poziomu nie oddało ani
jednej sztuki, niosąc między sobą 577 zębów. Po niej, w dziesięć minut: 1766
przyjętych okazów i 778 wizyt u Biologa.

Przy okazji Shinsoo i Jinno dostały swoich Biologów. Warunek sprawdzający mapę
Chunjo przetrwał w tym miejscu przenosiny na katalog królestw i odcinał oba nowe
królestwa od ich własnego NPC.

### Ulepszanie broni ruszyło z miejsca

Cenny przedmiot był wstrzymywany, gdy szansa powodzenia wynosiła mniej niż sto
procent, czyli na każdym kroku tabeli ulepszeń, a zwoju szukano dopiero od +6.
Poniżej +6 przedmiot nie mógł więc być ani ryzykowany, ani chroniony i nie
ruszał się wcale (zgłosił sekuras).

Zmierzone: 451 z 959 botów posiadających zwój nosiło broń dokładnie na +4, a 230
na +0, przy 1287 Zwojach Boga Smoków i 1002 Zwojach Błogosławieństwa w
plecakach. Teraz zwoju wolno szukać przy każdym poziomie ulepszenia, a
wstrzymanie zostaje tylko tam, gdzie porażka naprawdę kosztuje. Po wdrożeniu:
6361 prób ulepszenia w pięć minut wobec praktycznego zastoju wcześniej.

Uczciwie o drugiej połowie tego zgłoszenia: pomiar pokazał, że większość botów
stojących na +4 nie ma po prostu materiału. Krok z +4 na +5 wymaga dwóch sztuk
Nieznanego Lekarstwa+, a w całym świecie jest ich 586 na około 360 takich botów.
Tego kod nie naprawi, to kwestia dropu i rynku.

### Skrzynie nie wysypują się już na ziemię

Przed otwarciem skrzyni sprawdzane było miejsce na jeden mały przedmiot, a
skrzynia wydaje kilka — sama broń zajmuje trzy komórki. Silnik nie odmawia przy
pełnym plecaku, tylko rzuca resztę na ziemię i zgłasza sukces, więc zawartość
lądowała pod nogami bota na oczach wszystkich (zgłosił archonek2137). Teraz bot
pyta o miejsce na broń, zanim otworzy.

### Boty kupują tylko to, co handlarz naprawdę ma

Bot nie kupował drogich broni — on je tworzył. Zakup przedmiotu z drabinki
rozwoju wywoływał silnikowe „daj przedmiot" i liczył cenę z tabeli
przedmiotów, nie zaglądając wcale do asortymentu NPC. Stąd bot w Masce Strachu
na sześćdziesiąty poziom kupionej za 20 000 yang u handlarza zbrojami, choć
żaden sklep w tym świecie jej nie ma (zgłosił jaksiezabic, z linią logu na
dowód).

Teraz bot kupuje wyłącznie to, co stoi na ladzie u handlarza bronią, zbrojami
albo różności, i po cenie sklepu. Warto wiedzieć, co to znaczy: te trzy sklepy
mają razem 64 pozycje, broń do 36 poziomu, zbroje do 26, hełmy tylko startowe.
Wszystko powyżej ma pochodzić z dropu, straganów i kowala — dokładnie tak, jak
u gracza.

---

## 1.32.3 — 2026-09-10

### Trzy królestwa dało się włączyć tylko na świeżej instalacji

Plik `.env` powstaje raz i nigdy nie jest przepisywany, bo trzyma Wasze hasła,
których nikt inny nie ma. Skutkiem ubocznym było to, że każdy przełącznik
dodany do wzorca po Waszej instalacji po prostu u Was nie istniał, a rada
„ustaw `M2_PLAYERBOT_KINGDOMS=1`" dotyczyła linii, której w pliku nie ma
(zgłosił jaksiezabic). Nic nie wywalało błędu, bo Docker ma własną wartość
domyślną. Po prostu nie było czym włączyć królestw.

Launcher dopisuje teraz do `.env` wyłącznie ustawienia, których w nim nie ma, i
zawsze z wartością domyślną z pliku wzorcowego. Nie rusza żadnej istniejącej
linii i nigdy nie tyka haseł ani kluczy. Sprawdzone na prawdziwym `.env`
starszej instalacji: dopisało siedem ustawień, które narosły przez ostatnie
wydania, i nie zmieniło ani jednej linii, która już tam była.

### Panel zaawansowany po nieczystym zatrzymaniu serwera

Siedemdziesiąt trzy z siedemdziesięciu pięciu tabel tej gry to MyISAM, a jedno
nieczyste zatrzymanie wystarczy, żeby oznaczyć tabelę jako uszkodzoną. Od tej
chwili każdy, kto z niej czyta, dostaje błąd, więc klasyczny panel działa
normalnie (czyta pliki), a zaawansowany wywala się na „Internal Server Error"
(czyta wyłącznie bazę). Zgłosił archonek2137, z wklejonym błędem — co skróciło
szukanie do minuty.

Automatyczna naprawa w konfiguracji obejmuje tylko tabele otwierane po tym, jak
ustawienie weszło w życie, więc baza, która już chodziła w chwili aktualizacji,
zostawała na starym zachowaniu aż do restartu. Teraz przy każdym starcie serwera
idzie przebieg naprawczy, który ogląda wyłącznie tabele niezamknięte poprawnie i
naprawia uszkodzone, zanim cokolwiek z nich przeczyta. Na zdrowym świecie
kosztuje 417 milisekund przy tabeli `log` wielkości 1130 MB i 21,7 miliona
wierszy, więc startu nie opóźnia, a błąd nigdy nie zatrzymuje uruchamiania.

### ItemShop odpowiadał błędem 400

Rdzeń składa link, który otwiera klient, jako `http://` plus zawartość
`M2_MALL_URL`. Adres wpisany razem ze schematem dawał więc
`http://http://127.0.0.1:7791/ishop?...`, a wbudowana przeglądarka klienta
odpowiadała gołym HTTP 400, o którym w logach serwera nie ma ani słowa (zgłosił
jaroszv2). Napisanie `http://` przed adresem jest odruchem, więc adres jest
teraz przyjmowany w każdej postaci, a schemat i końcowy ukośnik są zdejmowane
przy starcie.

Przy okazji: `M2_MALL_URL` występował w pliku wzorcowym dwa razy. Docker bierze
ostatnie wystąpienie, więc kto przeczytał opis przy pierwszym i tam wpisał swój
adres, nie dostawał z tego nic. Został jeden wpis, ten z opisem.

---

## 1.32.2 — 2026-09-10

### Suwak liczby botów sięga teraz 2500

Przy włączonych trzech królestwach dosiew tworzy 2500 botów, a suwak w
launcherze kończył się na 1500. Tysiąca dosianych botów nie dało się w ogóle
poprosić z poziomu okienka. Zgłosił xewi zaraz po 1.32.0.

Limit siedział w dwóch miejscach i podniesienie samego suwaka nic by nie dało:
funkcja zapisująca liczbę do `.env` zaciskała ją z powrotem do 1500, więc
widzielibyście 2500, a do pliku poszłoby 1500. Poprawione są oba miejsca, a
także tryb konsolowy i pozycja w menu.

Dla samego Chunjo, czyli przy domyślnych ustawieniach, sufitem dalej jest 1500,
bo tyle tożsamości tworzy ziarno. Proszenie o więcej, niż świat ma, było i jest
bezpieczne: rdzeń uruchamia tyle botów, ile ma w rejestrze, i wypisuje w logu
ile poproszono, ile jest zarejestrowanych i ile wystartowało.

### Masowe dawanie przedmiotów botom

Nic tu nie zmieniamy w kodzie, ale warto wiedzieć, skąd się brało „nie działa"
(zgłosili zombian. i archded). Panel zleca nadanie przez wiersz w bazie, a
podejmuje go pomocniczy quest w grze. Quest jest kompilowany przy budowaniu
obrazu gry, nigdy przy starcie kontenera, więc serwer, którego obraz zbudowano
przed 7 września, ma starszego questa i na każde zlecenie odpowiada „Quest
wymaga aktualizacji". Wystarczy zaktualizować i kliknąć GRAJ, czekając aż
przejdzie budowanie obrazu.

Sprawdzone po przebudowaniu: zlecenie kończy się statusem „Nadano" w trzy
sekundy, a log gry pokazuje utworzenie przedmiotu na koncie bota.

---

## 1.32.1 — 2026-09-10

### Sprawdzanie aktualizacji mówi prawdę

Przez dwadzieścia minut po wydaniu 1.32.0 launcher pokazywał wszystkim naraz
dwa sprzeczne komunikaty: w stopce „Najnowsza wersja: nie udało się sprawdzić",
a w okienku „Kanał aktualizacji nie ma obecnie nowej wersji serwera". Żaden z
nich nie był prawdziwy. Nowa wersja była, tylko launcher nie potrafił odczytać
pliku, w którym o niej pisze. Zgłosili to kiciamol i jaksiezabic, jeden był o
krok od skasowania instalacji i postawienia jej od zera.

Winny był plik manifestu, który poszedł ze znacznikiem BOM na początku —
żaden wcześniejszy go nie miał. Sam manifest został poprawiony od razu i
naprawa nie wymagała żadnej aktualizacji po Waszej stronie. To wydanie
naprawia drugą połowę problemu, czyli to, że launcher w ogóle mógł tak
skłamać:

- **Manifest jest teraz czytany i rozbierany u nas, a nie przez
  `Invoke-RestMethod`.** Ta komenda nie zgłasza błędu, gdy odpowiedź nie jest
  poprawnym JSON-em: po cichu oddaje surowy tekst zamiast obiektu. Launcher
  pytał wtedy taki tekst o wersję serwera, nie znajdował jej i uznawał, że
  aktualizacji nie ma.
- **Znacznik BOM jest zdejmowany przed odczytem**, więc ta sama pomyłka nie
  zablokuje już nikomu aktualizacji.
- **Pusta odpowiedź, strona HTML podstawiona przez proxy albo firmową sieć i
  każdy inny plik, który nie jest manifestem, kończą się teraz czytelnym
  błędem**, który wprost mówi, że problem jest po stronie kanału aktualizacji,
  a nie Waszej instalacji.
- Komunikat o braku wersji mówi „kanał nie podał wersji serwera" zamiast
  twierdzić, że nowej wersji nie ma.

Sprawdzone w tym samym Windows PowerShellu 5.1, w którym chodzi launcher:
poprawny plik i żywy adres dają wersję, plik z BOM przechodzi, a strona HTML i
pusta odpowiedź dają nazwany błąd.

---

## 1.32.0 — 2026-09-10

### Trzy królestwa

Dotąd cały świat botów był jednym królestwem: Chunjo, jego dwie wioski i jego
mapy. Shinsoo i Jinno stały puste. Od tego wydania boty mogą żyć we wszystkich
trzech królestwach naraz, każde w swoich wioskach, u swoich kupców i na swoich
łowiskach.

**Domyślnie nic się nie zmienia.** Przełącznik `M2_PLAYERBOT_KINGDOMS` w
`.env` jest ustawiony na `0`, więc świat, który masz, zostaje dokładnie taki,
jaki był: 1500 botów Chunjo, te same postacie, te same poziomy, ten sam
ekwipunek. Żeby dołożyć dwa nowe królestwa, ustaw `M2_PLAYERBOT_KINGDOMS=1` i
uruchom serwer ponownie. Dosiewane jest wtedy 500 botów Shinsoo i 500 Jinno
obok istniejących; nic z tego, co już masz, nie jest ruszane ani przepisywane.

Co dostaje każde królestwo:

- **Własne wioski.** Yongan i Jayang dla Shinsoo, Joan i Bokjung dla Chunjo,
  Pyongmoo i Bakra dla Jinno. Nazwy są te, których używają questy silnika, a
  nie zgadywane.
- **Własne usługi.** Handlarz bronią, handlarz zbrojami, handlarka różności,
  dozorca, kowal, stajenny, starsza pani i Teleporter tej wioski, w której bot
  stoi. Wcześniej bot Shinsoo pisał nad głową „Ide do kowala" i szedł do kowala
  osiemdziesiąt kilometrów dalej, bo w kodzie były wpisane współrzędne Chunjo.
- **Własnych trenerów zawodu.** Ośmiu w każdej pierwszej wiosce. Drugie wioski
  nie mają żadnego i nigdy nie miały, i właśnie dlatego bot bez grupy
  umiejętności wraca do M1.
- **Własnego Biologa**, własny rynek ze straganami i własne łowisko z Rybakiem.
- **Własne tereny łowieckie.** Huby, pasma poziomów, punkty metinów i Bestialni
  bossowie każdej drugiej wioski, zmierzone z plików mapy tego królestwa.
- **Własne bramy.** Każdy przeskok między mapami królestwa czyta bramę i punkt
  lądowania z nazwy samego NPC, tak jak robi to silnik. Sprawdzone: wszystkie
  osiemnaście bram prowadzi tam, gdzie ma prowadzić, i wypuszcza postać na
  gruncie, po którym da się chodzić.

Chunjo nie został ruszony. Pomiar odtwarza jego stare, ręcznie wpisane
współrzędne co do jednostki — łącznie z trzema bossami Bokjung i ośmioma
trenerami Joan — i test pilnuje, żeby tak zostało.

### Panele widzą cały świat

- **Klasyczny panel** dostał granice i mapy terenu ośmiu nowych map, nazwy
  wiosek w filtrze i w rankingu. Do tej pory boty Shinsoo i Jinno były żywe i
  niewidoczne.
- **Panel zaawansowany** dostał te same granice, więc jego żywa mapa i lista
  śledzonych map obejmują nowe królestwa, a konsola respawnu ziemie klanowe.
- Mapa 24 była podpisana „Pyungmoo" i „Waryong". Pyongmoo to stolica Jinno, nie
  ziemia klanowa Chunjo; obie nazwy poprawione.

### Czego nowe królestwa jeszcze nie mają

Wspólne mapy świata — Dolina Orków, Pustynia Yongbi, Góra Sohan, oba Lochy
Pająków, Świątynia Hwang i dwa trudniejsze Lochy Małp — są hostowane przez ten
sam rdzeń co Chunjo, a bot nie może przejść na mapę, której jego rdzeń nie
hostuje (przeniesienie postaci między rdzeniami wymaga ponownego połączenia
klienta, a bot klienta nie ma). Dlatego Shinsoo i Jinno mają na razie własne
cztery mapy, które niosą je mniej więcej do trzydziestego szóstego poziomu.

Żeby to nie kończyło się botami stojącymi bezczynnie: bot nie jest wysyłany na
mapę spoza swojego rdzenia, a królestwo bez frontu może polować w swojej drugiej
wiosce także powyżej trzydziestego piątego poziomu. Rozwiązanie docelowe to
decyzja o tym, jak ma być poukładany serwer, i czeka na Ciebie.

### Drobne

- **Raport rejestru mówił nieprawdę.** Liczył tożsamość jako odrzuconą, gdy jej
  królestwo nie było Chunjo, więc wypisywał „wrong_empire=1000" obok modułu,
  który przed chwilą przyjął wszystkie tysiąc. Teraz odrzuceniem jest tylko
  królestwo spoza zakresu 1-3.

### Pomiar

Na serwerze testowym, 970 botów w trzech królestwach naraz:

| co | ile |
|---|---|
| procesor | 12,59 s rdzenia na 65 s ściany, czyli 19,4% jednego rdzenia |
| tick, rdzeń Shinsoo | 1,45 s z 60 |
| tick, rdzeń Chunjo | 2,16 s z 60 |
| tick, rdzeń Jinno | 1,41 s z 60 |
| resety watchdoga | 0, 1, 0 |

Budżet, którego pilnujemy, to 40% rdzenia przy 850 botach. W cztery minuty nowe
królestwa zrobiły po ponad 600 wizyt u własnych kupców i po ponad 300 u własnych
trenerów.

---

## 1.31.8 — 2026-09-10

### Boss z pustyni w Bokjung

- **Boss nie skacze już za ofiarą na inną mapę** (zgłosił Pasywny: komuś
  przeteleportował się Olbrzymi Żółw z Pustyni Yongbi do M2). Błąd jest w
  samym silniku, w `char_state.cpp`: żółw (rasa 2191) co jakiś czas
  przeskakuje pod swoją ofiarę przez `Show(victim->GetMapIndex(), ...)`,
  a więc bierze mapę **ofiary**, nie swoją. Gdy ofiara w tym czasie przeszła
  przez bramę albo zapłaciła Teleporterowi, boss szedł za nią i lądował w
  mieście. Łatka 0011 pozwala bossowi gonić i przeskakiwać tylko do ofiary
  na tej samej mapie; ta sama poprawka usuwa bezsensowną pogoń za
  współrzędnymi z mapy, której ofiara już nie ma. Dotyczy też graczy, nie
  tylko botów. Plik `char_state.cpp` jedzie w aktualizacji jak `char.cpp`.

### Aktualizacja na Linuksie i VPS

- **Aktualizacja nie zatrzymuje się już na łatce 0009** (zgłosił archded, wraz
  z trafną diagnozą). `prepare-context.sh` sprawdzał każdą łatkę osobno, na
  nietkniętym drzewie — a to inne pytanie niż to, o które chodzi: pierwszy
  hunk łatki 0009 ma w kontekście `#include "playerbot_manager.h"`, który
  dokłada łatka 0001. Osobno nie przechodzi, po kolei przechodzi bez zarzutu.
  Każda instalacja na Linuksie i VPS stawała w tym miejscu („Hunk #1 FAILED at
  37") i nie dało się zaktualizować; Windows tego nie widział, bo tam launcher
  wykłada pliki już połatane. Teraz próba jest kumulacyjna: pliki, których
  dotyka seria, lądują w katalogu roboczym poza kontekstem budowy i cała seria
  jest tam nakładana naprawdę. Prawdziwe drzewo zostaje ruszone dopiero wtedy,
  gdy próba przejdzie do końca — zasada „albo wszystkie, albo żadna" zostaje.

### Baza danych

- **Uszkodzona tabela naprawia się sama** (zgłosił cyckiseusmaz: „Błąd: (144,
  Table './player/quest' is marked as crashed and last (automatic?) repair
  failed")). Siedemdziesiąt trzy z siedemdziesięciu pięciu tabel gry to MyISAM,
  który nie znosi nagłego zatrzymania — wyciągnięta wtyczka, ubity kontener
  albo pełny dysk zostawiają tabelę oznaczoną jako uszkodzona i od tej chwili
  wszystko, co ją czyta, pada. Domyślne `BACKUP,QUICK` naprawia tylko plik
  indeksu, więc gdy uszkodzony jest plik danych, automat się poddaje — i to
  właśnie mówi ten komunikat. Ustawiamy `BACKUP,FORCE`: pełna naprawa przy
  otwarciu, z kopią uszkodzonego pliku obok, żeby nic nie znikło po cichu.
  **Kto ma ten błąd teraz**, naprawi go jednym poleceniem, zanim zaktualizuje:
  `docker compose exec mariadb mysqlcheck -uroot -p"$M2_DB_ROOT_PASSWORD" --auto-repair --check --all-databases`

### Boty i ulepszacze

- **Bot nie sprzedaje już zwojów ulepszeń handlarzowi** (zgłosił jaroszv2).
  Reguła złomu nie miała dla nich żadnej gałęzi, a handlarz płaci grosze za
  jedyną rzecz, bez której nie da się ulepszać powyżej +6: u nas przez dobę
  poszło tak 471 Zwojów Błogosławieństwa, podczas gdy bronie na nie czekały.
  Objęte są wszystkie zwoje, które zna silnik: Błogosławieństwa, Magiczny
  Kamień, Podręcznik Kowala, Zwój Boga Wojny i Zwój Boga Smoków. Na straganie
  nadal mogą stać — inny bot też ich potrzebuje.

### Miasta

- **Zdjęte limity straganów i zwiedzających.** Bokjung miał limit straganów
  (8 promili żywych botów), a straganiarz, który zastał pełny rynek, szedł
  z towarem do Joan. Jedno i drugie zniknęło: miasto ma się zaludniać, a
  odprawiony straganiarz to bot bez zajęcia. Joan bierze stragany od botów,
  które w Joan stoją.
- **Bot odsyła konia, gdy zsiada w strefie bezpiecznej.** `StopRiding`
  zostawia konia jako towarzysza, więc każde zsiadanie w mieście dokładało
  wierzchowca do tłumu na placu (u nas 295 zsiadań na M1 w kwadrans).
  Sprawdzone w `server_attr`: oba rynki, w Joan i w Bokjung, mają flagę
  strefy bezpiecznej, więc koń znika dokładnie tam, gdzie stoi tłum. Na
  mapach łowieckich koń zostaje, bo bot zaraz znów go dosiądzie.

---

## 1.31.7 — 2026-09-10

### Boty w grze (sosen, „Ulepszanie broni na 30 lvl oraz zmiany w umiejętnościach”)

- **Punkty skilli przenoszone Księgą Zapomnienia.** Po wprowadzeniu
  priorytetów bot z szesnastoma punktami w Tąpnięciu wkładał nowe punkty w
  Duchowe, a stare zostawały. Teraz bot bez wolnych punktów zdejmuje jeden
  punkt Księgą Zapomnienia z najniżej stojącego skilla, który ma ich więcej
  niż jeden, i wkłada go w najwyżej stojący jeszcze bez Mistrza — jeden punkt
  na 30 s, księga za 20 000 yang, od piątego poziomu. Skille już na Mistrzu
  zostają, bo silnik ich nie obniża.
- **Każda broń ze średnią ≥ 20% jest gotowa i nie jest przelosowywana.**
  Reguła „gotowa broń” działała tylko dla rodziny broni 30 lv, więc np. łuk
  45 lv ze średnią 40% był losowany Zaczarowaniem, aż dobił wynik punktowy,
  a średnia znikała. To były te „zmiksowane średnie 35+”.
- **Broń 30 lv od +6 w górę tylko na zwoju.** Bez Zwoju Błogosławieństwa
  (albo lepszego) bot nie niesie jej do kowala, tylko czeka — dotąd czekała
  tylko broń z nagrodowymi liniami.

---

## 1.31.6 — 2026-09-10

### Yang prosto do sakiewki, dla każdego

- **Yang z zabójstwa trafia prosto do sakiewki każdemu — graczom i botom —
  bez Trzeciej Ręki** (Invisible: „czy da się dodać status trzeciej ręki bez
  zajmowania slota w eq?”; Tieru: „na większości serwerów tak jest
  domyślnie”). Łatka silnika 0010 w `CHARACTER::RewardGold` uznaje każdego
  zabójcę za wyposażonego w automatyczne zbieranie; Trzecia Ręka i premium
  nadal są honorowane, ale niepotrzebne. Plik `char_battle.cpp` jedzie w
  aktualizacji jak `char.cpp`.
- **Boty zdejmują i oddają Trzecią Rękę** (72016–72018): pass, który do tej
  pory ją tworzył, zakładał i nakręcał, teraz ją usuwa, żeby nie zajmowała
  slota. Gracze swoje egzemplarze zachowują.

---

## 1.31.5 — 2026-09-10

### Boty w grze

- **Bot znów zbiera cudzy drop, gdy ma okazję** (decyzja Tieru). Blokada z
  1.31.4 („przedmiot bez właściciela podnosi tylko bot, który widział go,
  gdy był jego”) cofnięta: po dziesięciu sekundach drop przestaje mieć
  właściciela i bot bierze go jak każdy gracz, który stoi obok. Reszta
  1.31.4 bez zmian.

---

## 1.31.4 — 2026-09-10

### Aktualizacja, która się nie budowała

- **Obraz ItemShopu bez `apt-get`** (Marcol, paczka z 23:43: „target itemshop:
  failed to solve … Splitting of clearsigned file failed”). Obraz `php:8.2-apache`
  przeszedł na Debiana trixie, którego `apt` weryfikuje repozytorium przez
  sequoia i na niektórych Docker Desktopach pada; jedna nieudana warstwa
  anulowała całą budowę i serwer zostawał na starej wersji. ItemShop jest
  teraz przypięty do bookworm i nie stawia żadnej paczki Debiana (healthcheck
  pyta samo PHP), a panel klasyczny też jest przypięty do bookworm. Jeśli u
  kogoś strona ItemShopu „się nie ładuje” (sosen), to najpewniej ten sam
  powód: kontener sklepu nigdy nie powstał.

### Boty w grze

- **NPC z siodła** (Tieru). Bot nie zsiada już z konia przy sklepie, kowalu,
  dozorcy, Biologu ani przy portalu — silnik obsługuje jeźdźca przy każdej
  ladzie, odmawia tylko czytania księgi (tu bot zsiada) i stroju. Zsiadanie
  przy każdym NPC i wsiadanie zaraz potem było najbardziej widoczną częścią
  wizyty w mieście.
- **Koń odwołany na czas łowienia** (cyfrowy_mat: „wszystkie moje boty łowią
  z końmi obok”). Zsiadając przy wodzie bot odsyła konia, jak gracz, i
  przywołuje go do jazdy.
- **Bot nie zbiera cudzego dropu** (Kuszaa: „bije metina w M1, podchodzi
  jakiś koks i zbiera mój złom”). Silnik po dziesięciu sekundach zdejmuje
  własność z przedmiotu i od tej chwili każdemu odpowiada „twój”; pass lootu
  brał to dosłownie. Teraz przedmiot bez właściciela podnosi tylko bot, który
  widział go, gdy jeszcze był jego.
- **Martwy towar na straganie** (sekuras, cyfrowy_mat, jaksiezabic). Linia,
  która wróciła z lady niesprzedana, jest na następnym stoisku o 10% tańsza
  (do 40% po czterech), a broń albo zbroja poniżej +4, której nikt nie chciał
  przez sześć stoisk, idzie do handlarza. Od +4 w górę nic się nie zmienia —
  tego handlarz od bota nie dostaje.

- **Priorytety wbijania skilli według listy sosena** (wątek „Priorytety
  wbijania skilli przez botów”). Każdy build ma teraz kolejność punktów:
  Wojownik Body: Aura, potem Berek/Wir (losowo), potem Szarża/Trójstronne;
  Mental: Silne Ciało, Duchowe/Walnięcie, Tąpnięcie, Uderzenie Miecza; Sura
  WP: Czarowane Ostrze, potem cztery losowo, Rozproszenie na końcu; Sura BM:
  Ognisty Duch, Mroczna Ochrona, potem cztery losowo; Szaman Smok: Pomoc
  Smoka, Błogosławieństwo/Skowyt, Talizman/Strzelający Smok, Odbicie;
  Healer: Leczenie, Zwinność/Błyskawica/Szpon, Piorun/Zwiększenie Ataku;
  Ninja Dagger: Chmura/Zasadzka, Sztylet/Szybki Atak, Krycie; Archer: Ognista,
  Trująca, potem trzy losowo. „Losowo” to stały los per bot, więc dwa boty
  jednego buildu różnią się, a jeden bot jutro chce tego samego. Pierwszy
  skill z listy idzie do Mistrza przed drugim punktem w czymkolwiek innym.

### ItemShop

- **Księga Zapomnienia (70037) w ItemShopie** (sosen): cofa punkt wybranej
  umiejętności, dla skilla, który utknął na 17 po trzydziestym poziomie.
  Trafia też do sklepów już założonych, raz, pod kolejnym wolnym numerem.

---

## 1.31.3 — 2026-09-09

### Łucznik kontra Metin

- **Łucznik bije kamienie Metin sztyletem albo mieczem, nie łukiem** (Kuszaa,
  „Archer vs metin”: łucznik przewracał się przy kamieniu kilka razy i
  odpuszczał). Kamień nie rusza się z miejsca, strzały się kończą, a strzał z
  łuku to ułamek uderzenia wręcz. Łucznik trzyma teraz w torbie jedną broń na
  kamienie — sztylet przed mieczem, bo jest szybszy i tańszy; miecz tylko
  gdy sztyletu brak — dobiera ją, gdy celem jest Metin, i wraca do łuku,
  gdy kamień pęknie. Bez łuku w ręce nie
  rzuca umiejętności (silnik liczy je ze strzały: bez łuku dają 0), tylko
  zwykłe ciosy. Kupiec broni sprzedaje mu sztylet na jego poziom, gdy w
  torbie nie ma żadnego; ani stragan, ani sprzedawca tej broni nie zabierają.
  Panel klasyczny: pod znacznikami `PLAYERBOT_GEAR: archer draws the stone
  weapon` / `takes the bow back` w logu.

### Boty przy bramie do Joan w Bokjung

- **Boty nie kotłują się już przy bramie do Joan** (Kuszaa, nagranie z
  1.31.1: boty ze statusem „Ide na Gore Sohan”, „Ide do Lochu Malp”, „Ide do
  kowala” dojeżdżają do bramy, zsiadają z konia, po chwili wsiadają i
  odjeżdżają, a po kilku minutach wracają). Diagnostyka na naszym serwerze:
  152 ze 160 marszów pod tę bramę w dziesięć minut to targ — bot bez
  straganu w zasięgu miał „najpierw zajrzeć do Joan”, ale pass targu prosił
  o portal raz na dwie–pięć minut, więc trasę do bramy dokańczały zwykłe
  passy ruchu, a przy samej bramie nikt już o przejście nie prosił; bot
  oddawał tick wędrówce i odjeżdżał, a po następnym pytaniu targu wracał.
  Teraz marsz na targ w Joan jest zobowiązaniem: bot prosi o portal co tick,
  aż zmieni mapę, a bot, którego miejsce jest na pograniczu (albo którego
  wyjazd wstrzymał sprawunek), w ogóle tam nie idzie — kupuje w zasięgu w
  Bokjung. Do tego passy, które tylko kontynuują trasę do portalu, nie
  zsiadają już z konia kilometr przed bramą.

### Bot bez yangów na Teleporter

- **Bot, którego nie stać na Teleporter, poluje w Bokjung na opłatę, zamiast
  pytać Teleportera co tick.** Pomiar u nas: 268 z 362 botów 40+ w Bokjung
  miało mniej yangów niż jedna opłata (najbiedniejszy 79), a Teleporter
  odpowiadał odmową 26 000 razy na minutę — bo odczekanie z 1.30.42 ustawiało
  zegar, którego gałąź wyjazdu na pogranicze nigdy nie czytała, a powyżej
  pułapu Bokjung bot nie miał prawa polować, więc nie miał z czego zapłacić.
  Teraz: (1) po odmowie bot naprawdę czeka pięć minut; (2) bot bez opłaty
  nie idzie do Teleportera, tylko poluje w Bokjung, aż uzbiera trzy opłaty
  (status „Zbieram yang na Teleporter”); (3) kowal, zmiana bonusów, targ,
  reset umiejętności i Zwój Zapomnienia zostawiają w sakiewce trzy opłaty
  dla bota, którego łowisko leży za Teleporterem — to wydawanie wszystkiego u
  kowala po powrocie z pogranicza robiło z botów nędzarzy.

---

## 1.31.2 — 2026-09-09

### Panel zaawansowany

- **Panel Sebana 1.40.0** (z 1.38.5): zwijany poradnik aktualizatora na
  VPS w Zarządzaniu, motywy dziedziczone przez tabele i kafelki, ikony
  przedmiotów w bazie, flagi i nazwy królestw, portrety klas w profilach,
  listach i rankingach, kreator GM z wyborem płci. Nasze poprawki nałożone
  na nowo: Świątynia Hwang i Loch Pająków V2 w nazwach, granicach i
  respawnach, przełącznik „Noc na serwerze”, ranking broni z właściwymi
  kolumnami średnich i umiejętności (71/72), restart bez helpera Sebana,
  panel startowy bez błędu w pierwszych minutach świeżej bazy.

- **Opis „Szybkie czytanie ksiąg” mówi to, co robi kod** (Kenny: „na stronie
  jest co pół godziny, w kodzie czyta od razu”). Od 1.30.30 bot czyta księgę
  od razu, gdy ją ma; jedyny hamulec to sama gra — 20 000 doświadczenia i
  rzut przy każdej lekturze. Opis w obu panelach poprawiony.

---

## 1.31.1 — 2026-09-09

> **Ta aktualizacja jest mocno eksperymentalna.** ItemShop i panel GM to
> nowe, obce nam systemy (autor: OskarPWA) spięte z naszym stosem w jeden
> wieczór. ItemShop jedzie do każdego w zwykłej aktualizacji serwera (bez
> zmian w kliencie). Panel GM na F9 wymaga podmiany plików klienta i jest
> **opcjonalny**: instaluje go osobny przycisk launchera „PANEL GM F9
> (TEST)”, po ostrzeżeniu, z kopią zapasową poprzednich plików. Zwykła
> aktualizacja („SPRAWDŹ AKTUALIZACJE”) nie dotyka klienta.

### Panel GM (F9) i klient

- **Panel GM na F9 oraz przyciski EQ/Sprawdź w menu postaci** (autor:
  OskarPWA). Serwer: 21 komend `gmpanel_*` scalonych z naszym rdzeniem
  (`cmd_gm.cpp`, `cmd.cpp`, łatka 0009 dla instalacji linuksowych), każda
  sprawdza poziom GM po stronie serwera; „Spawn Botów” korzysta z naszego
  `CPlayerBotManager` (nowa metoda `GetAvailableBots`). Klient: cztery pliki
  (`game.py`, `interfacemodule.py`, `uitarget.py`, `constinfo.py`) w
  `pack/root.epk` — przepakowane narzędziem `tools/eterpack.py` (własny
  czytnik i zapis archiwów `.eix/.epk`, klucze stockowe r40250, weryfikacja
  obiegu: 90 plików, różnią się dokładnie 4 podmienione). Aktualizacja
  klienta jedzie jako składnik `client` manifestu i launcher nakłada ją na
  folder klienta. Panel otwiera się klawiszem F9 tylko postacią GM.

### Loch Małp i medale

- **Więcej wypraw po Medal Konny** (na rynku jest ich za mało). Bot z koniem
  bojowym miał 1–4% szansy na wyprawę w każdym półgodzinnym oknie, a to
  właśnie boty 46+ z takim koniem chodzą do trudnego lochu, gdzie medal
  wypada im z pełną szansą. Szansa po koniu bojowym potrojona (wojownik i
  sura broni 12%, sura magii, ninja sztylet i szaman 6%, łucznik 3%);
  przed koniem bez zmian. Pasma bez zmian: łatwy do 32, średni 33–45,
  trudny od 46.

### Naprawy z paczek graczy

- **Koń nie jest dosiadany i zsiadany co sekundę przy Metinie** (Kuszaa,
  botgrom2 w Lochu Pająków: „mounted_combat” i „near_destination” na zmianę,
  sto par na minutę). Przejście celowania dosiada konia bojowego do walki, a
  marsz, który przyprowadził bota pod kamień, zsiadał, bo koniec trasy był
  blisko. Marsz nie zsiada, gdy bot ma cel, z którym może walczyć z siodła.

- **Straganiarz z pogranicza nie jedzie z towarem do Joan** i spacer z
  towarem ma własny status „Ide z towarem na targ w Joan” (Kuszaa: botgrom2
  ze statusem „Ide na Gore Sohan” jechał do bramy M1). Bot, którego miejsce
  jest na pograniczu, przy pełnym Bokjung czeka zamiast iść do Joan.

- **Launcher GUI nie wysypuje się przy „Zainstaluj/Przygotuj”** (Uxie:
  „The property 'Count' cannot be found on this object”). Przy niekompletnej
  paczce lista brakujących plików była pojedynczym tekstem, a tryb ścisły
  PowerShella nie zna `.Count` na tekście. Teraz pokazuje komunikat
  o niekompletnej paczce, tak jak miał.

### Ulepszanie

- **Od +7 bot używa Zwoju Boga Smoków, gdy go ma**, zamiast Zwoju
  Błogosławieństwa (oba działają bez kowala — przejście ze zwojem chodzi
  tam, gdzie bot stoi). Uwaga do faktów silnika (`char_item.cpp`): Zwój
  Boga Smoków ma tu 25% przy +7→+8 i 20% przy +8→+9, Zwój Błogosławieństwa
  40% i 30% (jak u kowala), oba przy porażce cofają o poziom; Podręcznik
  Kowala 30% i 20%. Wybór zgodnie z prośbą, liczby do wiadomości.

---

## 1.31.0 — 2026-09-09

### ItemShop

- **ItemShop w grze** (autor: OskarPWA; wdrożenie jako usługa w naszym
  stosie). Kliknięcie monety na pasku otwiera w wbudowanej przeglądarce
  klienta sklep za Smocze Monety (`account.cash`) i Smocze Znaki
  (`account.mileage`): kategorie Ulepszanie, Bonusy, Koń i pomoc, Za Smocze
  Znaki (17 pozycji na start, do urządzenia przez operatora w bazie
  `itemshop`) oraz koło szczęścia za 10 SM. Zakup trafia do
  `player.item_award`, a rdzeń db dostarcza przedmiot przy najbliższym
  logowaniu. Rdzeń r40250 i klient miały już potrzebne części (komenda
  `in_game_mall` z podpisem, `WebWindow`, mapowanie „mall”) — dochodzi tylko
  usługa `itemshop` (PHP, port `M2_ITEMSHOP_PUBLIC_PORT`, domyślnie 7791) i
  schemat nakładany przez `playerbot-migrate` przy każdym starcie
  (idempotentnie; zasiew tylko do pustego sklepu). Hasło bazy i sekret
  podpisu idą ze środowiska, nie z plików. Adres sklepu (`MALL_URL`) rdzeń
  dostaje z `M2_MALL_URL`, a gdy pusty — z `M2_PUBLIC_ADDRESS` i portu.
  Sprawdzone u nas: podpisany link loguje automatycznie, zły podpis odsyła
  do logowania. Nie ma jeszcze: panelu GM na F9 i przycisków EQ/Sprawdź z
  paczki Oskara (wymagają przepakowania `root.epk` w kliencie), dropu
  Smoczych Monet z metinów i bossów dla botów i graczy, zakupów botów w
  sklepie — to następne kroki.

---

## 1.30.42 — 2026-09-09

### Loch Pająków 2

- **Boty od 54 poziomu chodzą do Lochu Pająków 2** (mapa 71,
  `metin2_map_spiderdungeon_02`). Z wiki i plików serwera: trujące pająki
  60–68 poziomu, które nie atakują pierwsze, bez metinów, na końcu warp do
  V3 i Elitarna Królowa Pająków (97 lvl, 2,6 mln HP — bez hubu, za silna).
  Do lochu wchodzi się u Chuk-Sala na końcu V1 za Przepustkę; boty na razie
  wchodzą bez przepustki, tą samą drogą co do V1: przez pustynię do bramy
  Kuahlo Dong i dalej po stronie serwera. Losowanie pogranicza od 54 poziomu:
  V2, Sohan albo Świątynia Hwang (łowca metinów zamiast V2 idzie na Sohan,
  bo w lochach nie ma kamieni). 11 hubów na prawdziwych punktach spawnu z
  `regen.txt` (668 punktów przez `group.txt` i `group_group.txt`), punkty
  przybycia (384,273) i wyjścia sprawdzone na `server_attr` (81/81 wolnych
  komórek, cała mapa to jeden spójny obszar). Mapa 71 przeniesiona na rdzeń
  game1, dodana do listy dozwolonych w `apply.sh`, do obu paneli (nazwa,
  granice, kafelek terenu).

### Ekonomia

- **Bot nie ulepsza tego, co za chwilę sprzeda handlarzowi** (jaksiezabic:
  Ametystowy Naszyjnik+0 → +1 o 17:58, sprzedany handlarzowi o 18:19; u nas
  w 6 godzin 12 534 przedmioty ulepszone w plecaku i potem sprzedane).
  Przejście ulepszania brało z plecaka każdy przedmiot, który bot może
  założyć, a reguła złomu sprzedawała każdy, który nie jest lepszy od
  noszonego i ma mniej niż +6. Teraz z plecaka ulepszane jest tylko to, co
  bot zatrzyma: ulepszenie noszonego, przedmiot wyższego poziomu niż noszony
  w tym slocie (nowa reguła — najlepszy taki zapas na slot zostaje w
  plecaku, żeby kowal mógł go doprowadzić do stanu lepszego od noszonego),
  rezerwa od +6.

- **Przedmiot od +4 nie idzie do handlarza** — to towar na stragan (stragan
  wystawia od +4), a handlarz brał każdy +4 i +5 (gregory: Szata Zach.
  Nieba+4, Złote Buty+4 sprzedane zaraz po ulepszeniu).

- **Materiał, na który jest popyt, nie idzie do handlarza** (gregory: Ogon
  Skorpiona, Worek z Pajęczymi Jajami, Igła Skorpiona sprzedane handlarzowi,
  podczas gdy boty kupują Ogon Skorpiona na straganach po 58 894). Reguła
  złomu pyta ledger rynku: materiał z popytem to towar na stragan; do
  handlarza idzie tylko taki, którego nikt nie potrzebuje, i tylko pod
  presją plecaka.

### Panel

- **Ranking botów pokazuje do 1000 pozycji** (Iwakura: „poproszę żeby mogło
  pokazywać więcej niż 100”). Selektor ma 200/500/1000, a API przestało
  ucinać do 100.

### Podróże

- **30% botów wraca z pogranicza po usługi do Joan, nie do Bokjung**
  (jaksiezabic: „M1 przy 1000 botów wygląda jak Balmora, a M2 jak Baerim”).
  Dotąd każdy powrót po zapasy, do kowala czy handlarza szedł do Bokjung.
  Udział wybierany raz na bota (po pid); Joan ma wszystkie usługi, kosztem
  jest powrót przez Bokjung do Teleportera. Medal, polowanie na broń i
  wyrośnięcie z mapy nadal prowadzą do Bokjung. W logu:
  `frontier_services_to_m1`.

---

## 1.30.41 — 2026-09-09

### Stragany i podróże

- **Limit straganów na Bokjung rośnie z liczbą botów.** Poprawka z 1.30.40
  trzymała tylko pierwszą serię: po restarcie wszyscy straganiarze otwierali
  w tym samym ticku (licznik odświeża się co minutę), a potem każde ponowne
  otwarcie odbijało się o limit 7 straganów na Bokjung — 229 odmów na minutę
  — i bot szedł z towarem do Joan, gdzie planista wysyłał go po zakupy
  zamiast na plac. Z 90 straganów po półtorej godziny zostało 28 (6 na
  Bokjung, 21 w Joan). Limit to teraz 8% żywych botów, nigdy mniej niż 7:
  80 dla tysiąca, 28 dla 350.

- **Bot bez yangów na Teleporter nie pyta go co tick.** Bot 58 poziomu z 799
  yang wobec opłaty 11 000 był odrzucany 24 000 razy na minutę, a jego status
  brzmiał „Ide na Gore Sohan” (Kuszaa: boty, które chcą na Sohan, kręcą się
  po Bokjung). Po odmowie podróż czeka 5 minut, w tym czasie działa wizyta w
  mieście i stragan, które zarabiają opłatę; status mówi „Zbieram yang na
  Teleporter na Gore Sohan (799/11000)”.

### Launcher

- **`playerbot-syslog.txt` w paczce diagnostycznej nie jest już pusty** (Kuszaa,
  1.30.40). Windows PowerShell 5.1 owija argument natywnego polecenia w
  cudzysłowy, nie escapując tych, które już w nim są — pierwszy cudzysłów
  wewnątrz polecenia `sh -c` kończył argument i wycinek wychodził pusty.
  Polecenie nie ma już żadnego cudzysłowu w środku (wzorce przez `grep -e`).
  Sprawdzone u nas: 60 000 linii, ~6 MB.

---

## 1.30.40 — 2026-09-09

### Stragany

- **Stragany nie znikają godzinę po restarcie** (jaksiezabic: „na 1000 botów
  11 sklepów 1 h po restarcie, 5 min po restarcie 50–100”; Oskar: liczba
  sklepów co jakiś czas spada do zera). Odtworzone u nas: 94 straganiarzy
  10 minut po restarcie, 33 po 25 minutach, bez żadnego restartu. Mechanizm:
  stragan stał 10–25 minut, po zamknięciu bot dostawał 30–90 minut przerwy,
  a ponownie otwierał dopiero po zakończeniu następnej wizyty w mieście —
  więc po restarcie (każdy straganiarz stoi tam, gdzie miał stragan) otwierali
  wszyscy naraz, a potem wygasali szybciej, niż wracali. Teraz stragan, który
  wygasł, otwiera się ponownie na tym samym miejscu (nowa wycena, nowy szyld,
  towar z plecaka), do 3 stanowisk z rzędu; dwa stanowiska z rzędu bez żadnej
  sprzedaży kończą serię wcześniej. Dopiero po serii bot bierze 30–90 minut
  przerwy i idzie grać. W logu: `PLAYERBOT_SHOP: another stand`.

### Launcher

- **Paczka diagnostyczna niesie log podróży botów.** Do kontenera trafia
  tylko syserr, więc paczka wysłana o „boty idą do złego portalu” (Kuszaa)
  nie miała ani jednej linii o tym, dokąd bot chciał iść. DIAGNOSTYKA dokłada
  teraz `playerbot-syslog.txt` (przejścia między mapami, portale, nawigacja,
  watchdog, cele, stragany, miasto, koń, Loch Małp — tylko linie botów, bez
  czatu graczy) i `playerbot-status.tsv` (aktualny status każdego bota).

---

## 1.30.39 — 2026-09-09

### Broń

- **Pomiar broni per klasa: procenty średnich i umiejętności mnożą obrażenia
  broni** (Iwakura: bot z Łukiem z Rogu Jelenia+8 151–244 i +47% średnich w
  plecaku nosił Miedziany Łuk+4 90–156). Dotąd linia procentowa była
  płaską sumą kilku tysięcy obok miliona za obrażenia. Teraz obrażenia broni
  (fizyczne dla wojownika/ninji/sury broni, magiczne dla szamana i sury
  czarnej magii; sztylet i łuk ×2 jak w silniku) są mnożone przez
  `100 + średnie·waga + umiejętności·waga`: build zwykłych ciosów czuje
  100% linii średnich i 35% umiejętności, build umiejętności odwrotnie.
  Ujemne linie (np. −17% umiejętności) liczą się tak samo. Broń na 30 i 75
  poziom z liniami nagrody wygrywa z „miedzianym” bez nich o tyle, o ile
  naprawdę bije mocniej.

- **Lepsza broń w plecaku jest zakładana także w ciągłej walce.** Silnik
  odmawia założenia w 1,5 s po ataku lub czarze, a bot, który nie przestaje
  atakować, nigdy nie miał okna — na teście 247 z 970 botów nosiło broń o
  ⅓ słabszą od tej w plecaku. Dotąd pauza w walce była tylko dla pustego
  slotu; teraz dla każdego lepszego przedmiotu (do 5 s, najwyżej raz na
  minutę, gdy okno nie przyszło). Drugi powód: przejście ekwipunku siedziało
  na końcu ticku, za straganem, lootem, koniem, wędkowaniem, podróżą i
  wędrówką, z których każde przejmuje tick — bot ciągle czymś zajęty nie
  zaglądał do plecaka wcale (wojownik 28 lvl bił Mieczem+6 z 1 poziomu mając
  Długi Miecz+4 w plecaku). Przejście uruchamia się teraz na początku ticku,
  poza otwartym straganem, wizytą w mieście, wędkowaniem i stajnią.

- **Broń z linią nagrody nie idzie na kowadło bez Zwoju Błogosławieństwa**
  (Oskar: „51% i spalił u kowala”). W tym silniku każde nieudane ulepszenie
  niszczy przedmiot (nie ma progu +3), a szanse od +5 to 80/60/50/40/30%.
  Broń ze średnimi ≥ 20% albo umiejętnościami ≥ 15% bot ulepsza tylko pod
  zwojem (nieudane = poziom niżej, nie strata), a bez zwoju czeka. To samo
  dotyczy każdego przedmiotu, który ma już 5 linii bonusów.

- **Wzmocnienie Przedmiotu i Zaczarowanie dopiero od +4.** Bot nie wydaje
  kamieni na przedmiot poniżej +4 — najpierw ulepszenie (i ryzyko spalenia),
  potem bony.

### Panel i launcher

- **Panel klasyczny (7788) pokazuje wersję, którą naprawdę ma** (azzyl5021:
  „Masz uruchomioną 1.29.0. Dostępna jest 1.30.38”, mimo aktualizacji i
  przebudowy panelu). Plik VERSION i CHANGELOG do obrazu panelu kopiował tylko
  `start-server.ps1` — za wczesnym `return` gałęzi `-IdentityOnly`, którą
  wywołuje launcher przed własnym `docker compose up --build`. Kliknięcie
  GRAJ/AKTUALIZUJ nigdy tam nie docierało, więc obraz panelu był budowany ze
  starym VERSION i przebudowa nic nie zmieniała. Launcher kopiuje teraz
  VERSION, CHANGELOG, `admin_panel.py`, `items.json`, schemat i questy panelu
  do kontekstu budowy w tym samym miejscu, w którym kopiuje źródła botów.

- **Panel zaawansowany (7790) pokazuje wersję z pliku VERSION.** Dotąd brał
  ją z `M2_PLAYERBOTS_VERSION` w `.env` (którego nikt nie ustawia) albo z
  domyślnej w `docker-compose.yml`, która stanęła na 1.30.29. Launcher i
  `start-server.ps1` przekazują ją z VERSION przez środowisko procesu (compose
  czyta je przed `.env`; sam `.env` nie jest dotykany), a domyślna w compose
  podniesiona do 1.30.39.

- **Logi na żywo bota pokazują tylko tego bota** (szubartov: przy „botgrom”
  były też linie botgrom2…botgrom6). Filtr dopasowywał nazwę jako fragment
  linii; teraz dopasowuje całe słowo — nazwa w logu silnika jest ograniczona
  spacją, `=`, `:` albo końcem linii, nigdy własną cyfrą.

---

## 1.30.38 — 2026-09-09

### Z kanału propozycji

- **Historia ekwipunku bota w panelu klasycznym** (Iwakura: „czemu moja top1
  sura nagle nie ma FMS-a +8, tylko lata z useless bronią”). Na stronie
  postaci, nad dziennikiem na żywo, sekcja „Historia ekwipunku” czytana z
  `log.log`: ulepszenia udane i nieudane, spalone przedmioty, założenia (co
  i zamiast czego), prezenty dla innych botów i od nich, sprzedaż na
  straganie (ile i za ile), zakupy na straganach, sprzedaż handlarzowi,
  kamienie zużyte na przemianę bonusów, depozyty do magazynu, Szkatułki
  Blasku. Domyślnie 60 ostatnich wpisów, przycisk „Pokaż starsze” — 400.
  Rdzeń dopisuje do `log.log` to, czego silnik sam nie zapisywał: założenie
  (`PLAYERBOT_EQUIP`), obie strony prezentu (`PLAYERBOT_GIFT_OUT/IN`),
  sprzedaż na straganie po stronie sprzedawcy (`PLAYERBOT_STALL_SOLD`) i
  depozyt u Dozorcy (`SAFEBOX PUT`, jak silnik dla gracza).

---

## 1.30.37 — 2026-09-09

### Zmienione

- **Plecak zapełniony w 80% to sprawa do załatwienia, nie stan do polowania**
  (Iwakura: „eq pełne od dawna, a on napierdala 2 godziny małpy po medal,
  który nie mieści się do eq”). Od 72 z 90 pól: żadna wyprawa po drop nie
  rusza — ani do Lochu Małp po medal (bot już w środku kończy medal i
  wychodzi; dropek medali też), ani do M3 po broń — a w mieście bot ma co
  robić: otwiera stragan niezależnie od osobowości (nawet z jedną linią, bez
  wyprzedażowej zniżki), księgi ponad zapas niesie do Dozorcy, złom do
  handlarza, materiały zużywa u kowala. Zbieracz ekwipunku, który nie odda
  zapasowych +8 handlarzowi, wystawia je na ladę.

- **Bramy brane z mapy, Teleporter za opłatą** (Kuszaa, nadal na 1.30.36:
  bot dojeżdża pod portal M1, zawraca i krąży). Punkt bramy nie jest już
  stałą z `npc.txt`: bot szuka na swojej mapie żywego NPC-warpa, którego
  cel (z nazwy NPC, tak jak czyta ją silnik) leży na docelowej mapie, i
  idzie do niego — więc gdy paczka serwera stawia bramę gdzie indziej, bot
  trafia tam, gdzie brama stoi. Do M3 prowadzi teraz brama Waryong, nie
  Teleporter. Teleporter (9012) działa jak dla gracza: od 11 poziomu, za
  `floor(poziom/5)·1000` yang (min. 1000) — bot bez pieniędzy nie
  teleportuje się. Zawieszony marsz do portalu trafia też do `syserr`, więc
  będzie w paczce diagnostycznej.

- **Stragan bez pola na pakiet scala stosy zamiast się poddawać.** Bot z
  plecakiem 90/90 nosił 12 małży w 12 polach: skan straganu pociął je na
  pojedyncze linie, stragan nie otworzył się z braku pola na pakiet, a
  scalanie czekało na zamknięcie, które nigdy nie nastąpiło.

---

## 1.30.36 — 2026-09-09

### Instalator i launcher (raport Sykesa)

- **Brak zrzutów SQL po przygotowaniu paczki kończył się „database not ready
  after 30 minutes”.** Instalator sprawdzał tylko, czy katalog
  `mariadb/initdb.d/dumps` istnieje, launcher nie sprawdzał go wcale; MariaDB
  startowała pusta i zgłaszała „healthy”, a `playerbot-migrate` czekał 30
  minut na schemat, którego nie było — prawdziwy błąd leżał w logu MariaDB.
  Teraz pięć plików (`account`, `common`, `player`, `log`, `hotbackup.sql`,
  cztery pierwsze niepuste) sprawdza **przed** `docker compose up`:
  instalator (`Test-ContextComplete`, z listą brakujących), `start-server.ps1`
  i ścieżka GRAJ w launcherze (obie — bo to dwie różne drogi do builda), oraz
  przycisk przygotowania paczki w GUI. Instalacja z już zainicjalizowaną bazą
  nie jest wstrzymywana (zrzuty czyta się tylko przy pierwszym starcie).
  Komunikat mówi, skąd wziąć zrzuty (`Server\metin2_mysql_dump.zip` z paczki
  r40250, `$env:M2_SRC_ARCHIVE` dla instalatora).

- **`playerbot-migrate` rozróżnia trzy czekania.** Zamiast jednego „still
  waiting for the database”: „MariaDB odpowiada, ale nie ma ŻADNEJ z tabel
  r40250 — baza zainicjalizowała się bez zrzutów, czekanie tego nie naprawi”,
  „import pierwszego startu w toku: X/8 tabel” albo „baza jeszcze nie
  odpowiada — duży świat odzyskuje się dłużej”.

---

## 1.30.35 — 2026-09-09

### Naprawione

- **„Bot, który chce iść na Sohan, kieruje się do portalu M1, zawraca i
  robi kółko wokół M2” (Kuszaa), „boty do łatwego lochu małp też jadą
  najpierw do portalu M1”.** Skutek 1.30.34: straganiarzem stał się każdy
  bot z ≥6 nadmiarowymi księgami, a pas straganu — który biegnie w ticku
  przed podróżą między mapami — gdy ring w Bokjung jest pełny (7 straganów),
  prowadził każdego straganiarza z towarem do portalu M1, „żeby otworzyć
  stragan w Joan”. Na serwerze z pełnymi plecakami ring jest pełny zawsze,
  więc setki botów z celem na pograniczu były co tick zawracane do portalu
  M1. Do Joan z towarem idzie teraz tylko kupiec albo dropek z osobowości,
  i tylko bez zaplanowanego wyjazdu; reszta czeka 10 minut na wolne miejsce
  na ringu i podróżuje tam, gdzie chciała.

- **„Boty nie wychodzą z lochu małp — zatrzymują się tuż przed portalem i
  zawracają” (Sekuras).** Bot stojący dokładnie na środku pola, z którego
  następny odcinek trasy „ociera się” o róg ściany: planer (siatka
  statyczna) ten odcinek zaplanował, test na żywo (supercover) go odrzuca —
  i odrzuci identycznie przy każdym ponownym planowaniu. Do 1.30.33 bot
  stał tam po cichu (reset watchdoga co 90 s); od 1.30.34 zjadał punkt pod
  nogami i przez gałąź „odcinek zasłonięty” planował w kółko, aż marsz do
  portalu poddawał się i bot zawracał. Trzeci ratunek: gdy oba końce są już
  środkami pól, a rogu nie da się ominąć, bot idzie tym odcinkiem — serwer
  prowadzi bota po prostej bez kolizji, więc najwyżej otrze się ramieniem o
  dekoracyjny róg. W logu `PLAYERBOT_NAV: forced through a grazed corner`,
  w linii watchdoga `nav_out=12`.

- **„Boty kupują więcej niż jedną wędkę — tyle, ile mają miejsca w eq”
  (FanFar).** Silnik ulepsza wędkę w trakcie łowienia (rzut przy każdym
  połowie, Wędka+1 staje się nowym przedmiotem Wędka+2 itd.), a warunek
  „mam wędkę” liczył wyłącznie vnum Wędki+1 — po pierwszym ulepszeniu bot
  kupował nową co sesję. Liczy się teraz każda wędka w plecaku, do ręki idzie
  najlepsza, a wędki zapasowe (słabsze lub równe innej) bot sprzedaje
  handlarzowi przy najbliższej wizycie.

---

## 1.30.34 — 2026-09-09

### Zmienione

- **Księga umiejętności nigdy nie trafia do handlarza.** Zapas własnych ksiąg
  zostaje w plecaku, nadmiar — cudze klasy i własne ponad zapas — jest towarem
  na stragan, a to, czego plecak pod presją nie mieści ponad 12 ksiąg towaru
  (dropek metinów: 20), bot zanosi do **Dozorcy** i wkłada do magazynu
  (jedna strona, 45 pól) — każdy bot do własnego, na koncie `playerbot_NNN`
  (deskryptor bota dostał wreszcie id konta; dotąd miał 0). Jak gracz: za
  pierwszym razem płaci Dozorcy 500 yang za stronę, otwiera magazyn tylko
  stojąc przy nim; PIN-u nie ustawia, bo silnik przyjmuje domyślne „000000”
  dla konta bez hasła. Nowy etap wizyty w mieście, w Joan
  i w Bokjung; status „Ide do magazynu z ksiegami” / „Oddaje ksiegi do
  magazynu”. Pełny magazyn zostawia resztę w plecaku jako towar — do
  handlarza nie idzie nic. Reguła z 1.30.33 („nadmiar ponad 20 ksiąg do
  handlarza”) wycofana.

- **Skill, który nie wszedł na M — Księga Zapomnienia powyżej 30 poziomu.**
  Bot nie wkłada punktów ponad 17 od 1.30.2x; do 30 poziomu resetuje skille
  u Starszej Pani, a powyżej miał używać Księgi Zapomnienia z rynku — której
  nikt w tym świecie nie sprzedaje ani nie dropi, więc skill stał na 17 do
  końca życia (81 botów nosiło 18–19 sprzed ograniczenia). Bot powyżej 30 lv
  kupuje księgę jak w sklepie itemowym (200 000 yang prosto do plecaka, przy
  zapasie 300 000), sparowaną ze skillem, i od razu ją czyta: jeden punkt
  z powrotem, kolejny rzut na M przy 17. Księga wystawiona na czyimś
  straganie też jest kupowana. Poniżej 30 lv Starsza Pani jak dotąd.

- **Stragany: kto ma nadmiar ksiąg, ten handluje; materiały w pakietach;
  scalanie zaraz po zamknięciu** (Oskar, jaksiezabic: „21 sklepów na 1000
  botów, na nich kilka KU, a boty latają po metinach z pełnym eq”). Los
  osobowości wybierał jednego bota na dziesięciu, a księgi leżały u
  pozostałych dziewięciu. Teraz każdy bot z co najmniej 6 nadmiarowymi
  księgami (cudzych klas albo własnych ponad zapas do czytania) otwiera
  stragan niezależnie od losu — bez czekania na pełny plecak, bo na świecie
  pełnych plecaków każdy by się kwalifikował, a na świecie półpustych nikt.
  Materiały (ości, skóry, talizmany…) idą na ladę w pakietach po 2, do 8
  linii jednego rodzaju — 16 ości to osiem linii po 2, nie jedna po 16;
  perły i małż pojedynczo, mikstury/zwoje/kamienie jak dotąd po 1. Po
  zamknięciu straganu bot scala stosy po 5 s, nie po 5 minutach, i wraca po
  kolejną porcję, dopóki jest co scalać. Cena małża (ok. 100 tys.) to
  celowo dziesiąta część oczekiwanej wartości pereł w środku — nie zmieniam.

### Naprawione

- **Marsz do Handlarki Różności w Bokjung „nieosiągalny” — 260 ratunków
  serwisowych dziennie.** Punkt podejścia do handlarki (141300, 240400) leży
  na pasku gruntu odciętym od placu w `server_attr`; planer odpowiadał
  „unreachable” trzy razy, a szósta porażka przenosiła bota. Etap wizyty
  sprawdza teraz osiągalność celu i, gdy cel leży na cudzym terenie, idzie
  do najbliższego pola własnego terenu w promieniu przybycia — tam, gdzie
  ratunek i tak by go postawił, bez sześciu nieudanych planów.

- **Bot stojący na własnym pierwszym punkcie trasy stał w nieskończoność.**
  16 z 18 resetów watchdoga po południu to `nav_out=11` (Goto odrzucone)
  przy `route=0/2` na środku pola: bot stał dokładnie na punkcie 0, następny
  odcinek był zasłonięty z jego dokładnej pozycji, pętla zjadania punktów nie
  zjadała punktu 0 („najpierw dojdź do środka pola” — a już tam był), Goto
  odmawiało ruchu w to samo miejsce, a marsz meldował „idę”. Jeden bot stał
  tak 20 minut przy kowalu w Joan. Punkt, na którym bot stoi, jest zjadany;
  zasłonięty odcinek idzie przez gałąź z ratunkami i licznikiem utknięcia.

### Świątynia Hwang

- Pomiar całego `regen.txt` (407 spawnów przez `group_group`): trzy komórki
  po 800–1200 spawnów bez huba w promieniu 19 km — południowo-wschodni róg,
  ziemia na wschód od środka i polana żab na północ od wejścia. Cztery nowe
  huby (każdy sprawdzony w `server_attr`), więc boty obiegają całą
  świątynię, nie tylko zachodnie i wschodnie pasmo.
- **Boss: Zjawa Żółtego Tygrysa** (1304, 75 lv, 178 040 HP, co 2 h z
  `boss.txt`, przy (575000, 93200) z dwoma Ropuszymi Generałami i dwoma
  Drzewnymi Żabimi Przywoływaczami) ma hub bossa jak Królowa Pająków i
  Dziewięć Ogonów: wyprawa grupy od 55 lv, pełna grupa może wyzwać 75,
  a po trzech wyruszających boss wyprzedza żaby wokół niego. Wejście do
  Wieży Demonów (Strażnik przy (590800, 110800)) stoi na ziemi bez spawnów,
  więc huba tam nie ma — boty przechodzą obok, idąc do środkowego huba.

---

## 1.30.33 — 2026-09-09

### Naprawione

- **Łucznicy zamarzali na punktach przybycia — na pustyni, w M3, w Dolinie
  Orków — na dwadzieścia minut i dłużej.** Znalezione w obserwacji po 1.30.32:
  dwanaście botów resetowanych przez watchdoga co 90 s, wszystkie łuczniczki.
  Każda miała w plecaku wyłącznie strzały ponad swój poziom (skrzynia
  postępu daje 8003 na 40 poziom botowi na 34, 8004 na 45 botowi na 41) i
  nic w slocie strzał. Licznik strzał liczył je jako zapas („sto strzał, nie
  trzeba kupować”), a łuk nie miał czym strzelać: przygotowanie broni
  zawodziło co tick i tick wychodził przez wizytę w mieście, której na mapie
  bez handlarza nie da się zacząć. Strzały ponad poziom nie liczą się do
  zapasu; bot bez strzał na mapie bez handlarza rusza do miasta przez zwykłą
  podróż między mapami, a gdy ta nie ma nic do powiedzenia — przynajmniej
  wędruje. Linia watchdoga mówi teraz, na czym tick stanął
  (`equip_pending`, `service`, `riding`, `nav_out`, `wander_in`).

- **Slot tarczy nie jest „brakującym slotem” dla łuku ani broni dwuręcznej.**
  Ta sama obserwacja: każdy łucznik był na stałe „w krytycznej potrzebie
  usług miasta”, więc z M3 wychodził w chwili przyjścia, a polowanie na broń
  30 odsyłało go z powrotem — piętnaście sekund na okrążenie. Do tego pauza
  „czekam na okno ekwipunku” dostała limit 5 s zamiast nieskończoności.

- **„Dropek metinów” z plecakiem pełnym ksiąg nie sprzedaje ich i nic nie
  podnosi (Sekuras).** Osobowość trzyma każdą księgę dla swojego straganu, a
  gdy los na stragan nie wypadł, trzymała je w nieskończoność: 80 ksiąg, brak
  miejsca na łup, bot dalej rozbija metiny. Pod presją plecaka dropek otwiera
  stragan niezależnie od losu, a to, co i tak nie mieści się ponad dwadzieścia
  ksiąg, sprzedaje handlarzowi. Księgi cudzych klas u każdego bota pod presją
  plecaka też idą do handlarza zamiast blokować łup.

- **„Bot z koniem na 10 poziomie krzyczy »idę do stajennego« i jeździ w kółko
  godzinę” (CarloMontana).** Bot nie jechał do stajennego — napis kłamał.
  Każdy bot w podróży z medalem w plecaku ogłaszał stajennego, a medal, którego
  nie da się oddać, zostaje w plecaku na długo: koń na 10 czeka na 35 poziom
  postaci, dropek medali nosi je na stragan. Napis mówi teraz o stajennym tylko
  wtedy, gdy bot naprawdę może oddać medal; bot w próbie konia bojowego pisze
  „Zdobywam konia bojowego na pustyni (x/100)”, a odbiór konia bojowego ma
  własne linie. Status u stajennego w Bokjung mierzył odległość do stajennego
  w Joan, więc oddawanie medalu w Bokjung wyglądało jak marsz. Przy okazji
  marsz do stajennego dostał przyciąganie celu w promieniu przybycia (6 pól
  zamiast 20), tak jak wcześniej wizyta w mieście i marsz do portalu.

### Z kanału propozycji

- **Noc na serwerze (Oskar).** Rdzeń między 22:00 a 05:59 czasu serwera
  (`M2_TZ` z `.env`) podnosi flagę `xmas_snow` — tę samą, którą GM ustawia
  komendą `/xmas_snow 1` — i rano ją opuszcza; klient pokazuje wtedy nocne
  niebo i, bo to flaga świąteczna, śnieg. Flaga idzie przez rdzeń bazy i wraca
  rozgłoszona do wszystkich klientów, sprawdzana raz na minutę. Przełącznik
  „Noc na serwerze” jest w obu panelach (Boty → Zachowanie na żywo; domyślnie
  włączony); po wyłączeniu w środku nocy rdzeń opuszcza flagę sam. Ręczne
  `/xmas_snow` GM-a działa jak dotąd, gdy przełącznik jest wyłączony.

### Obserwacja po 1.30.32 (serwer testowy, 970 botów)

Portale: 0 zablokowanych marszów, 0 nieudanych przejść, 1 400 przejść między
mapami w 36 minut. Cel bez migotania, tick 7–11 s z każdych 60. Jedyny
problem to zamrożeni łucznicy wyżej.

---

## 1.30.32 — 2026-09-09

### Z kanału propozycji

- **Przejrzyste nazwy sklepów (Xewi, Mat).** Szyld mówi, jaki to sklep,
  zamiast wypisywać nazwę pierwszego przedmiotu. Stragan, na którym
  większość to księgi, nazywa się np. „Ksiegi umiejetnosci”, „KU dla kazdej
  klasy”, „Biblioteka - ksiegi”; z przewagą materiałów — „Ulepki z M1/M2”,
  „Materialy do kowala”, „Skory, zeby i kly”; mieszany — jeden z ośmiu
  okrzyków targowych losowanych po bocie: „Zobacz kotku co mam w srodku”,
  „Zaczynam gre, kup cos”, „<nazwa> - najnizsze ceny”, „Wszystko za grosze”,
  „Tanio jak barszcz”… Szyldy z nazwą przedmiotu zostają tam, gdzie po ten
  przedmiot idzie się przez cały rynek: broń na 30 poziom, wysokie ulepszenie.

- **Nadmiar okazów biologa na stragan (Xewi, Oskar).** Okaz, którego
  zadanie bot już oddał, przestaje być „postępem zadania” i staje się
  towarem — kwiaty, korzenie, bez, grzyby idą na ladę. Zęby orka to także
  materiał do ulepszania, więc — jak zauważył Oskar — trafiają na rynek
  przez zwykłą regułę materiałów i księgę popytu, a nie pod biologa.
  Kamień duszy nigdy nie jest nadmiarem (klucz do drugiej połowy zadania).

- **Wyprzedaż biednego bota (Oskar, dziewięć głosów).** Bot od 20 poziomu,
  którego nie stać na wyprawę po mikstury (300 czerwonych i 200
  niebieskich po cenach handlarki — ok. 12 tys. yang, od 40 lv ok. 25 tys.),
  otwiera stragan niezależnie od losu osobowości, nawet z jedną linią, po
  70% ceny wywoławczej, pod szyldem „Wyprzedaz: …”. W danej godzinie robi
  to jedna czwarta takich botów (losowanie po bocie), więc rynek nie
  zamienia się w tłum biedaków — pierwsza wersja z progiem 30 tys. yang
  postawiła 36 botów 16–19 lv na ringu w Joan w pierwszej minucie.
  Zniżka jest nakładana po wycenie, więc pamięć sprzedaży dalej uczy się
  cen rynku, nie wyprzedaży.

### Naprawione

- **„Za dużo botów kręci się w strefie bezpiecznej” (Dixdros).** Stragan
  w panelu opisywał się „Planuje: poziom” — dla operatora to wyglądało jak
  bezczynny tłum na placu; w świecie bot i tak nosi szyld. Status mówi
  teraz „Prowadze stragan”. Reszta tłumu w Joan to wędkarze, zakupy,
  biolog — z celem, nie bez.

- **„Boty na 9–10 lv biją psy, na 19–20 wilki” (DavidS).** 32 huby
  łowieckie Joan były losowane po numerze bota, bez patrzenia na poziom:
  bot na dziesiątce trafiał do tygrysów na południowym wschodzie, a bot
  na dwudziestce do psów na wschodzie. Zmierzono medianę poziomu potworów
  w promieniu 2500 j. od każdego huba (regen.txt przez group.txt i
  group_group.txt, poziomy z mob_proto — od 1 do 21) i bot wybiera hub,
  którego pasmo (mediana −2 … +7) mieści jego poziom; numer bota dalej
  rozrzuca populację po hubach z pasma. To samo dla ośmiu obozów grup.

### Nie w tym wydaniu

- **Noc na serwerze 22–5:59 (Oskar).** To ustawienie klienta (środowisko
  mapy), nie serwera ani botów — Oskar wprowadził je u siebie po stronie
  klienta. Serwer nie ma czego przełączyć; paczka klienta to osobny temat.
- Suwak respawnów spotów/metinów/bossów (U4NT) — wymaga helpera
  gry z integracji Sebana; grupy z botami, PvP, królestwa (dixdros,
  Remigiusz) — zanotowane na później.

---

## 1.30.31 — 2026-09-09

### Naprawione

- **Boty na mapach frontowych zmieniały cel co pięć sekund: „zapasy” ↔
  „poziom”.** Zgłaszane jako „boty w M2 / na pustyni nie wiedzą, co robić”.
  Dziewięć miejsc w kodzie podróży (przejście przez pustynię, wyjścia z M3
  i z frontu, wyjścia na front, powrót do Joan, wyjścia po biologa, konia,
  sprzęt i polowanie) wpisywało cel w każdym ticku, a planer pięć sekund
  później przywracał swój — zmierzone: **350 botów, 12 000 z
  20 000 linii zmiany celu** w oknie pomiaru, status „Ide do miasta po
  zapasy” i „Ide do Lochu Pajakow” na zmianę u tego samego bota. Podróż nie
  dotyka już celu; decyduje planer. Po poprawce, w takim samym
  oknie na serwerze testowym (970 botów): par „zapasy↔poziom” **0** (było
  5 828), wszystkich linii zmiany celu 1 988 (było 13 766); zostało tylko
  naturalne „przeżyć↔zapasy” przy niskim PŻ.

- **„Boty 30+ zrobiły wymarsz na M3” — 60% serwera na jednej mapie.**
  Zgłoszone ze zrzutem mapy. Reguła z 1.30.29 („każdy powyżej 35 bez broni
  na 30 idzie ją wyfarmić”) była dobra dla świata, którego boty mają
  głównie 50 lv, i katastrofalna dla świata z botami 36–40. M3 przyjmuje
  najwyżej **15% żywej populacji** (nie mniej niż 30), a bot, który już tam
  jest, zostaje, dopóki tłum nie przekroczy półtorakrotności tego progu —
  drzwi nie migają.

- **Bot wracający do Bokjung po sprzęt krążył po spotach za materiałem.**
  Zgłoszone z logiem (Kuszaa): metinowiec 61 lv po zakupach w M2 przez
  siedem minut gonił Jak-To Czarnego Wiatru po materiał do ulepszania,
  zbierając przy okazji przedmioty po innych botach, zanim poszedł do
  teleportera. Wyprawa po materiał nie startuje na mapie, na której bot nie
  ma prawa grindować, ani gdy ma już zaplanowane wyjście.

- **Wędkarze: „zapasy” ↔ „wędkowanie” co pięć sekund.** Ta sama choroba
  w innym miejscu: sesja wędkarska wpisywała cel co tick, planer „zapasy”
  co pięć sekund — 43 wędkarzy, 6 000 linii w 12 minut. Planer respektuje
  trwającą sesję wędkarską. Podobnie wyjście z frontu po zakupy przestało
  wpisywać „poziom”.

- **Bonowanie broni na 30 poziom: aż do średnich ≥ 20%.** Zgłoszone z
  Discorda („boty za rzadko bonują”, „broń 30 z ŚR niżej 20% niech mixują
  aż się uda”). Próg „skończona” dla broni 30 spadł z 30% do 20% średnich,
  a bot przebija ją dopóki tego nie osiągnie — niezależnie od sumy
  pozostałych linii, która wcześniej mówiła „wystarczy” przy 12% średnich.
  Bronie 30 w plecaku (towar na stragan) też są bonowane do tego progu —
  kamień kosztuje czterdziestą część ceny gotowej sztuki. Zwoje z plecaka
  idą pierwsze, dokupywane są dopiero, gdy ich nie ma. Zmierzone przed:
  580 zmian i 123 dodania w 3 godziny na 970 botów.

- **Panel zaawansowany, ranking „Broń 30 lv”: ŚR i UM.** Zgłoszone
  dwukrotnie. Sprawdzone na surowych wierszach bazy: typ 72 niesie średnie
  (do +46), typ 71 obrażenia umiejętności (broń 30 ma je w parze, ujemne —
  stąd „−5%” na zrzutach). Zapytanie nazywa kolumny wprost, bez zamiany po
  fakcie, a sortowanie „według średnich” sortuje według średnich.

### Sprawdzone i nie jest błędem

- **„Wojownicy nie kupują mikstur”.** Zmierzone na 970 botach: wojownicy
  kupują **najwięcej** czerwonych ze wszystkich klas (529 zakupów, 345 tys.
  sztuk, ~650 na wizytę, przychodzą do handlarki ze średnio 130) — i wypijają
  18,8 tys. dziennie. Mają ich mało w plecaku, bo je zużywają, nie dlatego,
  że nie kupują. Sury trzymają 650–740, bo prawie ich nie piją.
- **Pętla konia w M2 i na pustyni (1.30.29)** — to regresja naprawiona w
  1.30.30; logi Kordyla i Kuszaa pochodzą sprzed tej wersji.

---

## 1.30.30 — 2026-09-09

### Naprawione

- **Boty na mapach łowieckich wsiadały i zsiadały z konia co sekundę i nie
  robiły ani kroku na długiej trasie.** Regresja z 1.30.28 („koń biegnie za
  botem"): wędrówka na końcu ticku wsadzała bota na konia na długą nogę, a
  początek następnego ticku zdejmował go bezwarunkowo „do walki" — i każdy z
  tych kroków kasował trasę. Zmierzone na 970 botach: **133 000 wsiadań i
  zsiadań w 28 minut u 261 botów, 6 sekund z każdych 60 sekund ticku** i ani
  jednej przejścia między hubami. To dlatego rajd na Królową Pająków nigdy
  nie dochodził do skutku: 27 decyzji „idę na bossa" i zero botów w promieniu
  trzech kilometrów od niej. Koń transportowy schodzi teraz tylko wtedy, gdy
  jest z kim walczyć (cel albo napastnik), sekcja celu zsiada w ticku, w
  którym cel wybrała, a bufy i wieloprzyciąganie nie ruszają z siodła (silnik
  odmawia umiejętności z konia bez umiejętności konnych). Po poprawce:
  z 4 732 wsiadań na minutę do ~200, tick z 12,1 s na 6–7 s z każdych 60.

- **Boss szukany był w dziewięciu sektorach wokół punktu z tabeli — a Królowa
  Pająków łazi za tymi, którzy ją biją.** Znaleziona 5 km od huba, trzy minuty
  później „powalona" bez żadnego BOSS_KILL w logu, po chwili znów stojąca
  11 km dalej — i każdy rajd był odsyłany do pracy, gdy ona stała. Boss jest
  teraz szukany na całej mapie (jedna migawka bytów mapy, wynik pamiętany
  30 s). Uczciwie: rajd wciąż jej nie zabija — ma 193 408 PŻ i 60 poziom, a
  bierze się za nią 8–10 botów z 48–56 poziomem, które giną lub uciekają
  (59 śmierci od potworów na tej mapie w godzinę). Do przemyślenia liczba
  rajdowiczów; mechanika chodzenia i szukania jest już poprawna.

- **Bot z celem „Metiny" szedł do Lochu Pająków, gdzie kamieni nie ma.**
  Zgłoszone z Discorda ze zrzutem („Ide do Lochu Pajakow (cel: Metiny)").
  Loch Pająków i trzy Lochy Małp nie mają `stone.txt`. Metinowiec z roli
  losuje zamiast Lochu Pająków Górę Sohan, a wyprawa metinowa nie startuje z
  mapy bez kamieni ani w jej stronę (los za godzinę, z miejsca, gdzie bot stoi).

- **Księgi umiejętności zalegały w plecakach.** Zgłoszone z Discorda
  („głównie są to KU, cały dzień nic z nimi nie robią"). Bot trzymał
  dwanaście ksiąg każdej własnej umiejętności — także tej, której czytać
  jeszcze nie może, bo nie ma jej na M. Zmierzone: **2 636 ksiąg w 929
  plecakach, 5 przeczytanych w 3 godziny, 4 sprzedane.** Dwanaście zostaje
  tylko dla umiejętności na M1–M9 (czytelnych teraz), trzy na zapas dla
  reszty, zero dla umiejętności na G; nadwyżka idzie na stragan jak księgi
  cudzych klas, a do handlarza dopiero pod presją plecaka.

- **Stosy w plecaku i pojedyncze sztuki na straganie.** Zgłoszone z
  Discorda: dwa stosy tego samego przedmiotu, których nie da się złączyć.
  Reguła silnika jest jedna — ten sam numer przedmiotu i identyczne gniazda,
  do 200 sztuk — i tylko przeciągnięcie ręką ją uruchamia, którego bot nie
  ma. Bot co pięć minut zlewa swoje rozdzielone stosy (po częściowym zakupie,
  sprzedaży czy podniesieniu do pełnego stosu), a na straganie robi odwrotnie:
  ze zwojów i kamieni duszy odłącza do czterech pojedynczych sztuk na osobne
  linie, bo prywatny sklep sprzedaje linię w całości — dwadzieścia zwojów na
  jednej linii to dwadzieścia albo nic. Materiały zostają w stosach, bo boty,
  które je kupują, kupują stos. U gracza dwóch stosów o różnych gniazdach
  silnik nie złączy nigdy — to nie jest coś, co można naprawić po stronie
  botów.

- **Panel „Loch Pająków" pokazywał 0 widocznych postaci przy 150 botach na
  mapie.** Lista pozycji brała tylko boty z zapisanym `map_index` z listy
  starych map. Filtr mapy w zapytaniu obejmuje teraz każdą mapę z granicami
  (Loch Pająków, oba nowe Lochy Małp, Sohan, Hwang): 846 pozycji zamiast 587.

- **Mikstury: 300 czerwonych i 200 niebieskich na wyprawę, duże od 40
  poziomu.** Pytanie z Discorda „czemu tak mało potek". Handlarka sprzedaje
  duże mikstury (27003/27006) i od czterdziestki bot kupuje właśnie je; zakup
  liczony do pojemności plecaka, nie do zamierzonej liczby.

- **Małże po 7 000 na straganie.** Cena wywoławcza małża ma próg 100 000 —
  jak perły — zamiast trzykrotności ceny handlarza.

- **Ubijanie kamienia.** Bot nie odpuszcza metina poniżej 15% jego PŻ, dopóki
  sam ma powyżej 10%: kamień nie goni, a wracał do pełnego zdrowia.

### Panel zaawansowany 1.38.5 (Seban)

Scalone z tym wydaniem: konto GM z gotową postacią (klasa, punkt startowy),
nazwy przedmiotów w kanale zdarzeń, sezon tygodniowy z właściwych wpisów
logu (`REFINE SUCCESS`, `BOSS_KILL`), przycisk aktualizacji przez nasz
`updater` (profil `update`; panel dostaje wolumen `update-spool` w compose),
ciasteczko sesji osobne od panelu klasycznego. Zachowane nasze poprawki:
Świątynia Hwang (nazwa, granice, kafelek), poprawne ŚR/UM w tablicy bonusów,
odporność na brak migawki systemu na świeżej instalacji. Restart i zapis rat
z konsoli restartu działają bez helpera z `integration/` (którego ten obraz
nie zawiera) — odmawiana jest tylko zmiana respawnów, i panel mówi dlaczego.

### Launcher

- **Dane do bazy (Navicat, HeidiSQL, DBeaver).** Zgłoszone z Discorda
  („1045 - Access denied for user 'root'@'172.18.0.1'"). Nowy przycisk
  **DANE DO BAZY (NAVICAT)** w GUI (akcja `DbAccess`, pozycja 16 w menu
  konsolowym) pokazuje host, port i oba konta z hasłami z `.env` — w polach
  do skopiowania, nie w logu, bo log trafia do paczek diagnostycznych.
  **NAPRAW DOSTĘP DO BAZY** ustawia teraz oprócz konta `metin2` także
  `root@'%'` na hasło z `.env`, więc po nim dane z tego przycisku zawsze
  działają. Opis w README i `docs/INSTALL.md`.

### Spawn

- Kolejność spawnu: najpierw boty grane w ostatnim tygodniu, potem świeże
  (poziom ≤4), potem reszta — suwak podniesiony o 120 daje 120 nowych
  postaci od pierwszego poziomu (zweryfikowane: `registered_started=970`,
  120 botów na 1–10 poziomie po kwadransie).

### Weryfikacja

Cztery pełne przebiegi na serwerze testowym (970 botów): koń, rajd, księgi,
stosy, panel Sebana, akcja DbAccess. Zmierzone przed i po; liczby wyżej.
Paczka aktualizacji sprawdzona pod kątem kompletności (`check-update-covers-build`)
i przeskanowana Defenderem przed publikacją.

---

## 1.30.29 — 2026-09-08

### Naprawione

- **Wędki leżały na ziemi, a wokół stało stado koni.** Zgłoszone z Discorda ze
  zdjęciem i słusznie: to był nasz błąd z 1.30.26. Silnik przy pełnym plecaku
  **kładzie kupiony przedmiot na ziemi i zgłasza sukces** — `AutoGiveItem` nie
  zwraca błędu, tylko `AddToGround`. Nowy zakup wędki tego nie sprawdzał: bot
  płacił, wędka lądowała na trawie, bot nadal jej nie miał i kupował następną.
  Konie wokół to towarzysze wezwani przy zsiadaniu. Kod strzał znał tę pułapkę od
  dawna („rynek wyłożony drewnianymi strzałami") — zakup wędki powstał dzień
  później bez tej wiedzy. Sprzęt wędkarski, drewno na ognisko i mikstury sprawdzają
  teraz miejsce w plecaku **przed** zapłatą; mikstury kupowane są najwyżej do
  pojemności, a nie do zamierzonej liczby.

- **Bot na czterdziestym poziomie „zbierał Korzeń Gango 0/5" do końca świata.**
  Zgłoszone czterokrotnie z Discorda i jako pytanie od operatora. Dwa osobne błędy,
  oba potwierdzone liczbowo.

  W rdzeniu: przejście „noszę okazy" wygrywało z każdym innym i liczyło **jeden**
  okaz. Bot na czterdziestce z jednym Korzeniem Gango z wyprawy wędkarskiej do Joan
  był przypięty do wiersza z piętnastego poziomu na zawsze: wiersz przerośnięty,
  więc nie poluje na tego potwora, więc nigdy nie zbierze pięciu, więc nigdy nie
  odda. Zmierzone: **38 botów na 26+ trzyma korzenie, 36 z nich po jednym do
  czterech sztuk.** Przerośnięty wiersz liczy się teraz tylko wtedy, gdy plecak
  trzyma już całe oddanie; wiersz własnego pasma działa jak dotąd.

  W panelu klasycznym: „Etap Biologa" pokazywał **pierwszy nieukończony** wiersz,
  bez związku z tym, co rdzeń naprawdę robi — stąd „w panelu bota jedno, w rankingu
  drugie". Panel liczy teraz tą samą regułą co rdzeń, z odczytem plecaka.
  Sprawdzone na trzech przypiętych botach: dwa na 33 i 34 poziomie pokazują Grzyb
  Tue, jeden na 49 — Ząb Orka, czyli najwyższy otwarty wiersz, gdy wszystkie są
  przerośnięte.

- **Przycisk „ZAINSTALUJ / PRZYGOTUJ" w launcherze mówił „Paczka jest gotowa"
  paczce bez źródeł gry.** Zgłoszone z Discorda po 1.30.27: „ponowne uruchomienie
  instalatora przez Install in GUI nie odtworzyło źródeł". Nie mogło — ten
  przycisk **nigdy nie był instalatorem**. Sprawdza Dockera, tworzy skrót na
  pulpicie i zakłada, że źródła już są na dysku; nie ma w nim ani jednego
  odwołania do `installer\install.ps1`. Teraz wykrywa brak źródeł i mówi po
  polsku, czego brakuje, że żaden przycisk ani aktualizacja tego nie pobierze, i
  podaje dokładne polecenie z `M2_SRC_ARCHIVE`.

### Zmienione

- **Broń z trzydziestego poziomu: boty jej szukają dłużej, wracają po nią z
  rubieży i wyceniają ją jak nagrodę.** Trzy zgłoszenia z Discorda naraz.

  Pułap polowania wynosił 24 (Loch M3) i 35 (Bestiale w Bokjungu) — broń była
  czymś, co bot albo zdobył młodo, albo nigdy. Zmierzone: **205 z 393** botów na
  36+ z ponad milionem yang nie miało jej nigdzie — „latają na 37 z sześcioma
  milionami i stożkowym mieczem +6 po dolinie". Farmą teraz do czterdziestki
  (krzywa dropu trzyma siedemdziesiąt procent dziesięć poziomów nad potworem), a
  rubież **oddaje** bezbronnego bota do M2, skąd idzie na M3. Trzy minuty po
  wdrożeniu: trzy powroty z rubieży, siedem wyjść na M3 i **sześć znalezionych
  broni**.

  Cena: nieulepszona broń 30 wpadała do gałęzi „szrot" (cena handlarza razy dwa,
  29 032 na naszej ladzie), podczas gdy Futro Tygrysa obok stało po kilkaset
  tysięcy z podłogi materiałowej. Propozycja z Discorda — piętnaście do dwudziestu
  razy drożej — ma rację co do rzędu wielkości: podłoga 250 000, między +7 a +8
  zwykłego sprzętu. Na ladach po zmianie: **312 500, 375 000, 512 500.** Do tego
  osobny, stromy schodek (+300%) za linię **średniej 24%+ albo umiejętności
  15%+** — progi dobrane do realnych zakresów tego świata (46 i 18), tylko na
  tym zestawie broni.

- **Dwie rzeczy z tej samej listy zgłoszeń celowo bez zmiany.** Bransoleta,
  naszyjnik i kolczyk **są kupowane** u handlarza zbroi, a zwoje błogosławieństwa
  **są używane** przy ulepszaniu — oba mechanizmy istniały wcześniej i działają;
  ogranicza je 30 poziom i trzy zwoje na wizytę.

---

## 1.30.28 — 2026-09-08

### Naprawione

- **Ostrzeżenie o brakujących źródłach było w połowie kodu, której „GRAJ" nie
  uruchamia.** Zgłoszone z Discorda **drugi raz**, przeciwko wersji, która miała
  to naprawić — i było to słuszne zgłoszenie. 1.30.27 dodała sprawdzenie
  kompletności kontekstu budowy do `start-server.ps1`. Tyle że launcher woła ten
  skrypt z przełącznikiem `-IdentityOnly`, który kończy się zaraz po zapisaniu
  `.env` — **w linii 557, a sprawdzenie stało w 713** — po czym launcher buduje
  sam, wywołując `docker compose up --build` bezpośrednio. Gracz klikający GRAJ
  nigdy tego sprawdzenia nie widział i dostawał piętnaście linii
  `failed to calculate checksum ... not found`, tak samo jak przedtem.

  Sprawdzenie stoi teraz tam, gdzie budowa naprawdę zachodzi. Zamiast piętnastu
  błędów Dockera pada jedno zdanie: czego brakuje, że to **nie jest** błąd
  Dockera, WSL ani aktualizacji, że te pliki pochodzą z własnej paczki serwera
  r40250 i żadna aktualizacja ich nie przywróci — oraz że ratunkiem jest ponowne
  uruchomienie instalatora, przy nietkniętej bazie, postaciach i ustawieniach.

- **Boty w Lochu Małp ogłaszały wyjście, którego nie było.** Zgłoszone z Discorda:
  „mimo chmurki, że wychodzą z lochu, one nie wychodzą". Napis „Wychodzę z Lochu
  Małp" był zwykłym „w przeciwnym razie" — mówił go **każdy** bot, który na mapie
  lochu się przemieszczał. A w lochu przemieszczanie się to stan normalny:
  jedenaście komnat połączonych wyłącznie NPC-ami GOTO, więc przejście do
  następnej to zwykły marsz. Złapany bot z tą chmurką miał cel **polowanie na
  metiny**.

  Rzecz, która czyniła to szczególnie mylącym: **wyjście z lochu jest
  natychmiastowe** — to bezpośrednia zmiana mapy w tym samym ticku, nie marsz do
  portalu. Bot, którego widać w lochu, z definicji nie wychodzi. Napis brzmi teraz
  „Szukam drogi przez Loch Małp" i słowo „wychodzę" nie pada tam w ogóle.

  Trzeba dodać, czego pomiar **nie** potwierdził: zarzutu o zepsute poruszanie się
  i teleporty. Przez półtorej minuty obserwacji **jeden bot na trzydzieści** nie
  drgnął, i ten najpewniej walczył. Geometria wczytuje się kompletna na wszystkich
  trzech mapach, przejścia między komnatami zachodzą, wejść i wyjść jest tyle
  samo. Problemem była sama chmurka.

### Zmienione

- **Bonusy ważone według klasy, a nie jednakowo dla wszystkich.** Mechanika
  działała od dawna — zmierzone **2155 użyć** zwojów, a bot nie dokupuje zwoju,
  gdy jakiś ma. Zły był wybór tego, co warto zatrzymać.

  Zmierzone na każdym atrybucie każdego przedmiotu w tym świecie: **średnie
  obrażenia losują się do 46, a obrażenia umiejętności tylko do 18**. Przy wagach
  dwanaście i dziesięć — tych samych dla wszystkich — najlepsze możliwe średnie
  obrażenia dawały 460 punktów, a najlepsze obrażenia umiejętności 216. Czyli
  **szaman, który wylosował najlepszą linię w grze dla swojego buildu, wyrzucał ją
  na następnym przebiegu**, choć jego obrażenia to niemal wyłącznie umiejętności.
  Wagi są teraz dobrane wprost do tych dwóch sufitów: dla maga najlepszy skill
  bije najlepsze średnie, dla reszty kolejność zostaje bez zmian.

  I rzecz, którą trzeba powiedzieć wprost, bo zmienia oczekiwania: **„Silny
  przeciwko Potworom" w tym świecie nie występuje ani razu.** Poradniki słusznie
  stawiają go bardzo wysoko — podnosi obrażenia wobec wszystkich potworów i
  kamieni metinu, czyli wobec wszystkiego, z czym bot kiedykolwiek walczy — ale
  żadna zmianka go tu nie wylosuje. Jego waga została podniesiona dla przedmiotów,
  które mają go wbudowanego, i to wszystko, co da się z nim zrobić.

  Uczciwie o stanie pomiaru: nowe wagi są policzone z realnych zakresów i
  wdrożone, ale w oknie obserwacji zdążyły zajść dopiero dwa przebiegi — za mało,
  by pokazać zmianę w liczbach. Sama zmiana dotyczy wyłącznie tego, którą linię
  bot uznaje za lepszą.

---

## 1.30.27 — 2026-09-08

### Naprawione

- **Instalacja, która straciła źródła gry, nie mówiła o tym ani słowa.**
  Zgłoszone z Discorda po 1.30.26: budowa kończy się piętnastoma liniami
  `failed to calculate checksum ... not found`, wymieniającymi `src/server/common`,
  `libgame`, `extern`, `serverfiles/share/...` — a Docker i WSL działają poprawnie.

  Liczba, która to rozstrzygnęła: kontekst budowy miał **1,61 MB**, a pełny liczy
  się w setkach megabajtów. Paczka aktualizacji wkłada pod `game/src` dokładnie
  **trzydzieści osiem plików** — sam `src/server/game`, bo tam należą źródła botów.
  Cała reszta drzewa pochodzi z **własnej paczki serwera r40250** operatora i jest
  rozpakowywana raz, przy instalacji; aktualizacja jej nie przywraca, bo nie wolno
  nam jej rozpowszechniać. Na maszynie, która to drzewo straciła, aktualizacja
  **wytwarza** wiarygodnie wyglądający `src/server/game` — i budowa dochodzi dalej
  tylko po to, by paść na wszystkim obok.

  A dlaczego drzewo znikało: `prepare-context.sh` kasował kontekst budowy, **zanim**
  sprawdził, czy ma z czego go odtworzyć. Kontrola na górze pliku patrzyła tylko,
  czy katalog `server/` istnieje — nie czy zawiera dziewięć modułów. Niepełne
  drzewo przechodziło tę kontrolę, po czym skrypt **niszczył działającą
  instalację i dopiero potem umierał**. Teraz sprawdza wszystkie dziewięć modułów
  i cztery katalogi `share/` przed skasowaniem czegokolwiek, a gdy źródeł brakuje,
  zostawia istniejący kontekst w spokoju.

  Drugie zabezpieczenie jest po stronie gracza: launcher sprawdza kompletność
  kontekstu **przed** uruchomieniem budowy i mówi jednym zdaniem po polsku, czego
  brakuje, że to nie jest błąd Dockera ani aktualizacji, i że ratunkiem jest
  ponowne uruchomienie instalatora — baza, postacie i ustawienia zostają nietknięte.

- **Boty przemierzały pół mapy pieszo, a koń biegł za nimi.** Zgłoszone z Discorda
  dokładnie taką obserwacją — i tak właśnie było. Silnik przy zsiadaniu
  **przywołuje konia jako towarzysza**, a nic nie sadzało jeźdźca z powrotem:
  każde przejście wędrowne prosiło o trasę z końmi **wyłączonymi**, choć miejsce
  polowania wybierane jest nawet dwadzieścia kilometrów dalej. Koń wraca na trasy
  wędrowne, do znanego metina, na wyprawę na bossa i na powrót po śmierci; krótkie
  przebieżki są nietknięte, bo dosiad i tak nie zachodzi bliżej niż 1800 jednostek.

  Zmierzone po wdrożeniu: **304 dosiady na długą trasę w pięć minut** — wobec
  **siedmiu w dwadzieścia pięć minut** przed poprawką.

- **Bot odchodził od metina, któremu został ułamek życia.** Zgłoszone przez dwie
  osoby. To nie był odwrót taktyczny — kamień jest w silniku `CHAR_TYPE_STONE`, a
  nie `CHAR_TYPE_MONSTER`, więc odwrót go nie dotyczy. Winna była **awaryjna
  regeneracja przy dwudziestu procentach życia**, która porzuca cel bez względu na
  to, czym on jest. Dla potwora to słuszne: goni i zabije. Kamień **nikogo nie
  goni**, a odejście od niego wyrzuca całą walkę — bot wraca do kamienia w pełni
  sił albo nie zastaje go wcale. Teraz dobija kamień poniżej piętnastu procent
  jego wytrzymałości i tylko dopóki sam jest powyżej dziesięciu. Śmierć kosztuje
  doświadczenie i to nie jest przyzwolenie na stanie w fali uderzeniowej.

- **Przynęta kupowana za tyle, ile bot ma — ale nie do ostatniego yanga.**
  Poprawka z 1.30.26 zadziałała aż za dobrze: ten sam bot przeszedł z 556 przez
  601 do **jednego yanga**, wydając ostatni grosz na robaki i nie zostawiając nic
  na miksturkę. Rezerwa kwotowa nie da się tu zastosować — każda dość duża, by
  miała sens, jest większa niż majątek botów, którym ta poprawka pomaga, więc
  odmówiłaby dokładnie tego zakupu, dla którego powstała. Jest udziałem: bot
  wydaje najwyżej dziewięćdziesiąt procent sakiewki.

### Nowe

- **Boty resetują umiejętności u staruszki, gdy nie mają już czym próbować.**
  Zaproponowane na Discordzie. Sprawdzone w plikach serwerowych i zrobione
  dokładnie tak, jak robi to gracz: **NPC 9006** na południe od Joan, koszt
  `10000 + poziom × 2000`, odmowa poniżej piątego i **powyżej trzydziestego**
  poziomu — wszystko wprost z `skill_reset2.quest`.

  Bot idzie tam tylko wtedy, gdy naprawdę utknął: umiejętność stoi na siedemnastu
  bez mistrza **i** nie został mu ani jeden punkt umiejętności. Reset kasuje
  wszystkie poziomy umiejętności, więc bot z punktami w ręce ma je najpierw wydać
  i rzucać dalej za darmo. Do tego pół godziny odstępu i zapas w sakiewce, żeby
  reset nie zostawił go bez pieniędzy na mikstury.

  Zmierzone na żywo, od zapłaty po powrót do pracy:

      19:36:44  reset u staruszki  poziom=27 grupa=2 zablokowana=48 koszt=64000 punkty=26
      19:36:49  cel: wybór profesji
      19:37:10  wybrał grupę u trenera  punkty=26
      19:37:14  wrócił do roboty

  Koszt 64 000 to dokładnie `10000 + 27 × 2000`, a 26 punktów to dokładnie tyle,
  ile zwraca silnik przy czyszczeniu umiejętności. **Dwadzieścia sześć sekund
  po zapłacie bot stał u trenera** — bo grupa wyzerowana to dokładnie ten stan,
  który od zawsze odsyłał bota do trenera, więc druga połowa sprawy nie
  potrzebowała żadnej nowej mechaniki.

### Zmienione

- **Trzy pozycje robią stragan.** Zgłoszone z Discorda: „bez sensu, że boty mają
  dużo yang i miejsce w ekwipunku, a wystawiają na siłę sklep z jedną Niedźwiedzią
  Skórą czy innym ornamentem". Zasada obok tego progu od zawsze mówiła „trzy",
  podczas gdy liczba mówiła „dwa" — a dwa tanie materiały to ta sama skarga jedną
  linię dalej. Prawdziwy łup otwiera stragan sam jak dotąd: broń z trzydziestego
  poziomu, cokolwiek na +6, duży bonus, medal konny. Materiał nigdy nie zbliża
  się do tego progu.

---

## 1.30.26 — 2026-09-08

### Naprawione

- **Bot, którego nie stać na całą paczkę przynęty, nie kupował jej wcale.**
  Zgłoszone z Discorda jako „stoją pod Rybakiem, a przynęty nie kupują", i
  w 1.30.25 nie umieliśmy tego rozstrzygnąć — zakup mógł się nie udać na trzy
  różne sposoby, a wszystkie wychodziły jednymi drzwiami. Kiedy każda odmowa
  zaczęła mówić własnym zdaniem, przyczyna znalazła się **na naszym własnym
  serwerze w dwie minuty**:

      cannot afford fishing_bait vnum=27801 count=20 price=800 gold=556

  Paczka to dwadzieścia robaków za osiemset yang. Bot miał pięćset
  pięćdziesiąt sześć — czyli trzynaście robaków i całą sesję łowienia — i nie
  kupował żadnego, bo kupno było „cała paczka albo nic". Teraz bierze tyle, na
  ile go stać. Dotyczy to rzeczy kupowanych na sztuki: wędka jest jedna i albo
  na nią stać, albo nie.

  Trzeba powiedzieć uczciwie, czego **nie** widać w pomiarze: przez pół godziny
  po wdrożeniu ta gałąź nie miała okazji się odpalić — osiemdziesiąt trzy
  zakupy przynęty i żadnej odmowy, bo nasze boty są zamożne. Zmiana jest
  bezpieczna z konstrukcji: po zmniejszeniu ilości cena jest przeliczana i
  sprawdzana ponownie, więc w najgorszym razie kończy się tą samą odmową co
  dotąd.

### Potwierdzone po 1.30.25

- **Portale trzymają przy pełnej obsadzie.** Szesnaście minut pomiaru przy
  850 botach: **1455 przejść portalami i zero zacięć**. Przed poprawką było
  dziewięćdziesiąt do stu zacięć na minutę.

- **Świat rozkłada się na dziewięć map.** To było pytanie z Discorda —
  „nie wiem jak jest z górą Sohan, jest tam mało botów" — i odpowiedź okazała
  się inna, niż wyglądała. Sohan nie był pusty z powodu Sohanu: boty nie mogły
  wyjść z miasta. Po naprawie portali Bokjung spadł z 465 botów na 107, a
  reszta rozeszła się po świecie:

  | mapa | boty |
  |---|---|
  | Pustynia Yongbi | 239 |
  | Joan | 201 |
  | Mount Sohan | **108** |
  | Bokjung | 107 |
  | Dolina Orków | 105 |
  | Loch Pająków V1 | 36 |
  | Loch Małp trudny | 20 |
  | Świątynia Hwang | 18 |
  | Loch Małp średni | 12 |

- **Drużyny na rubieżach.** 354 boty z 846 polują w drużynie.

---

## 1.30.25 — 2026-09-08

### Naprawione

- **Świeża instalacja nie budowała się — i nie chodziło o jeden plik.**
  Zgłoszone z Discorda: budowa zatrzymywała się na
  `failed to compute cache key ... "/rates": not found`, a katalogu `rates/`
  nie było ani w instalacji, ani w żadnej kopii aktualizacji.
  Sprawdziliśmy **wszystkie 33 instrukcje `COPY`** we wszystkich Dockerfile'ach
  i brakowało **sześciu** rzeczy: `game/rates/`, `client-builder/pack/`,
  `client-builder/bin/`, `updater/bin/` i `wsbridge/bin/`.

  Wniosek, który trzeba zapisać wprost, bo to już trzeci raz ta sama pomyłka:
  **bycie w repozytorium to nie to samo co bycie dostarczonym.** Instalacja
  złożona z paczki ma dokładnie to, co wymienia lista plików —
  `panel/bin/apply_rates.sh` był w repozytorium i mimo to go tam nie było.
  Doszło `tools/check-update-covers-build.py`, które czyta każdy `COPY`
  każdego Dockerfile'a i odmawia, gdy cokolwiek, czego budowa dotyka, nie
  jechałoby w aktualizacji. Paczka tego wydania została nim sprawdzona:
  1878 plików, nakładka i kopia sceniczna zgodne co do pliku.

- **Boty nie potrafiły wejść w portal — w żaden portal.** Zgłoszone z Discorda dla
  Lochu Małp („cały czas pisze nad ich głowami «Ide do lochu po medal konny»
  i tak stoją na koniach"), potwierdzone dla M3, a zmierzone u nas na
  **pięciu portalach na czterech mapach naraz**.

  Przyczyna nie była ani w terenie, ani w trasie. **Planer i marsz mierzyły co
  innego.** Planer prostuje trasę po środkach komórek swojej siatki; marsz
  sprawdza rzeczywisty odcinek z dokładnej pozycji postaci, przejściem, które
  liczy komórkę muśniętą o milimetr rogu. Cel portalu to surowa współrzędna
  ze stałej — na granicy komórki, nie w jej środku. Te dwa testy różnią się
  właśnie tam, a różnica jest **trwała**: ten sam plan wraca za każdym razem.

  Bot odrzucał więc własny jedyny punkt trasy **czterdzieści dwa razy w
  dwadzieścia sekund**, i robił to **w zupełnej ciszy**: gałąź, która wyrzuca
  trasę, nie logowała niczego, a sąsiednia odzywa się tylko przy pierwszej i
  trzeciej porażce, po czym milknie na zawsze. Dlatego nie było tego widać w
  żadnym logu i dlatego trzeba było to najpierw oprzyrządować.

  Marsz do portalu celuje teraz w środek komórki, na której planer liczył — to
  najwyżej trzydzieści pięć jednostek różnicy przy progu przejścia dwustu — a
  odrzucony odcinek ma dwie próby ratunku, zanim trasa pójdzie do kosza. Sama
  gałąź przestała być cicha, a linia zacięcia niesie teraz komplet: ile razy
  marsz naprawdę pytano, jaka była trasa i która dokładnie odmowa zapadła.

  Zmierzone u nas, ta sama wersja co u graczy:

  | | przed | po |
  |---|---|---|
  | zacięcia portali | ~90–100 na minutę | **0** |
  | przejścia portalami | — | **569** w ośmiu minutach |
  | boty w Lochach Małp | 1 i 1 | **10 i 5** |
  | `tick_ms` | 9558–9939 | **4389** |
  | trasy planowane na minutę | 4108 | **623** |

- **Marsz uznawany za zacięty, gdy bot legalnie obchodził budynek.** Postęp
  mierzono wyłącznie skracaniem się linii prostej do portalu, a portal
  praktycznie zawsze stoi przy zabudowie — więc obejście było regułą, nie
  wyjątkiem. Złapany na żywo bot miał trasę w punkcie piątym z siedmiu,
  licznik zacięć na zerze i był wyrzucany ze swojej trasy co dwadzieścia
  sekund. Zaliczony punkt trasy jest teraz postępem na równi ze skróceniem
  odległości.

- **Zegar zacięcia liczył czas ścienny, nie próby marszu.** Zegar biegł także
  wtedy, gdy bot robił coś zupełnie innego — walczył, zbierał, stał u kupca —
  więc pierwszy tick podróży po zajętych dwudziestu sekundach ogłaszał
  zacięcie bota, który dostał dokładnie jedną szansę, żeby zrobić krok.
  Złapany na gorącym uczynku: jeden przebieg, pełna trasa i osiemdziesiąt
  osiem kilometrów do celu.

- **Sprzęt nie był powodem, by pójść na stragany.** Zgłoszone z Discorda:
  „wystawiłem bojowe tarcze +8 za 1 yang i boty nie kupują, mimo że mają
  tylko +4". Porównywanie oferty z tym, co bot nosi, działało od dawna — ale
  brama, która decyduje, czy w ogóle wybrać się pod ladę, znała tylko
  materiały do ulepszeń, medal konny, zwój zapomnienia, wolne gniazdo na
  kamień duszy i broń z trzydziestego poziomu. Sprzęt był osiągalny wyłącznie
  przypadkiem, przy okazji wyprawy po coś innego.

- **Odmowa zakupu przynęty w końcu mówi, dlaczego odmawia.** Zgłoszone
  z Discorda: „stoją pod Rybakiem, a przynęty nie kupują". Trzeba powiedzieć
  wprost: **tego jeszcze nie rozstrzygnęliśmy** — u nas ta ścieżka działa,
  w godzinie pomiaru zanotowaliśmy 69 zakupów po dwadzieścia robaków za 800
  yang. Powodem, dla którego nie da się tego orzec ze zgłoszenia, jest sam log:
  zakup mógł się nie udać na **trzy zupełnie różne sposoby** — przedmiot bez
  ceny w serverfiles, bot bez pieniędzy, plecak bez miejsca — a wszystkie trzy
  wychodziły jednymi drzwiami jako „cannot_afford_tackle". Teraz każdy mówi
  własnym zdaniem, z ceną, stanem sakiewki i numerem przedmiotu.
  Przy okazji naprawiona prawdziwa wada: funkcja zwracała „czy cokolwiek
  kupiono", więc „nie było czego kupować" i „kupno się nie udało" dawały tę samą
  odpowiedź. Sprawdzone też w silniku, żeby nie zgadywać: `AutoGiveItem`
  dokłada do istniejącego stosu, zanim poszuka wolnej komórki — pełny plecak
  blokuje więc przynętę tylko temu botowi, który nie ma jej wcale.

  Drugie zgłoszenie z tego samego wątku — bot z napisem „Zakladam przynete na
  wedke", który nie ma wędki na plecach — zostało poprawione w **1.30.23**
  i ten zrzut pochodzi ze starszej wersji.

### Zmienione

- **Na rubieżach częściej w drużynie, i częściej z szamanem.** Szaman już
  wcześniej rzucał na całą drużynę — Błogosławieństwo, Pomoc Smoka,
  Chyżość, Wzmocnienie Ataku i leczenie — tyle że polował samotnie, więc
  rzucał to na nikogo. Przy doborze partnera na mapach rubieży para
  z **dokładnie jednym** szamanem ma teraz wyraźną przewagę (drugi szaman nic
  nie dodaje, bo buffy i tak obejmują drużynę), drużyna z szamanem ma
  pierwszeństwo przed drużyną bez niego, a skłonność do samotnego polowania
  spada tam z dwudziestu pięciu procent do dziesięciu.

---

## 1.30.24 — 2026-09-08

### Naprawione

- **Wędkarze stali w miejscu zamiast łowić — i był to nasz błąd z 1.30.22.**
  Żeby trzymali metr odstępu, promień „dotarłem" został tam zawężony z dwustu
  jednostek do dwudziestu pięciu. Tyle że marsz zatrzymuje się przy stu — to
  `PLAYERBOT_NAV_ARRIVAL_DISTANCE`, próg, przy którym nawigacja uznaje cel za
  osiągnięty i przestaje iść. Między dwudziestoma pięcioma a stoma powstała
  martwa strefa: nawigacja melduje sukces i staje, pas wędkowania prosi o
  kolejny krok, nic się nie rusza, licznik zacięć pokazuje zero i w żadnym logu
  nie ma śladu porażki. Dwa boty złapane na żywo stały siedemdziesiąt jeden i
  siedemdziesiąt sześć jednostek od celu, którego żaden nigdy nie osiągnął.

  Promień wrócił do stu — nigdy poniżej progu, przy którym marsz staje — a sama
  zasada jest teraz **sprawdzana przy kompilacji**, nie zapisana w komentarzu:

      static_assert(PLAYERBOT_FISHING_ARRIVE >= PLAYERBOT_NAV_ARRIVAL_DISTANCE,
              "an arrival radius below the navigation's own strands the bot short of it");

  Zmierzone po wdrożeniu: wędkarzy nad wodą 43 → **53**, z tego łowiących
  11 → **53**. Nikt już nie chodzi do Rybaka w kółko ani nie szuka wędki.
  Odstępy na brzegu: najmniejszy **60** jednostek zamiast dwunastu, średnio
  **198** do najbliższego sąsiada.

  Trzeba powiedzieć wprost, czego to nie daje: **metr odstępu nie jest
  gwarantowany.** Przy stanowiskach co sto pięćdziesiąt jednostek i stu
  jednostkach tolerancji na każdym końcu dwaj sąsiedzi mogą się zejść bliżej i
  czasem schodzą. Żeby zagwarantować metr, stanowiska musiałyby stać co trzysta,
  a wtedy na tym odcinku rzeki zmieści się około czterdziestu przy pięćdziesięciu
  sześciu wędkarzach — więc na razie zostaje ta ziarnistość, opisana zamiast
  obiecywanej.

  Przy okazji sprawdzone: drugi i ostatni promień przybycia w kodzie,
  `PLAYERBOT_MARKET_ARRIVE`, wynosi 450 i leży bezpiecznie powyżej progu.

- **Panel podstawowy zaniżał liczbę botów.** Zgłoszone z Discorda ze zrzutami obu
  paneli obok siebie: zaawansowany pokazywał **999 botów w grze**, podstawowy
  **399**. Boty były — mylił się licznik. Odczyt migawki stanu miał `int()`
  wewnątrz `try`, które obejmowało **całą pętlę po pliku**, więc jedna nieczytelna
  linia — wystarczy rozdarty odczyt w chwili, gdy rdzeń przepisuje plik —
  wyrzucała wszystkie pozostałe wiersze tego pliku. Stąd dokładnie taka liczba:
  parsowanie urwane w połowie. Teraz zła linia kosztuje jedną linię, a panel
  zapisuje w swoim logu, ile wierszy pominął. Sprawdzone na próbie: plik z 999
  wierszami i jednym uszkodzonym daje 998 odczytanych i jeden pominięty.

---

## 1.30.23 — 2026-09-08

### Naprawione

- **Botów ubywało i nic ich nie przywracało.** Zgłoszone z Discorda: „z tysiąca
  po godzinie mam trzysta pięćdziesiąt". Kolejka odrodzeń była napełniana
  **raz**, przy starcie, i opróżniana przez minutę — potem nikt już nigdy nie
  liczył. Bot, któremu nie powiodło się wejście do świata, albo który z niego
  wypadł, był stracony aż do restartu serwera.
  Teraz raz na minutę rdzeń przelicza, ilu z zamówionych naprawdę jest w
  świecie, i dosyła brakujących tą samą rozłożoną w czasie kolejką. Ograniczone
  do tego, co zamówiono, więc odbudowuje obsadę i nigdy jej nie powiększa.
  Zmierzone na naszym własnym, zdrowym serwerze zaraz po wdrożeniu: **jedenastu
  botów nie było** po pierwszym napełnieniu — i przed tą zmianą zostaliby poza
  światem do końca dnia.

- **Nie dało się dowiedzieć, dlaczego botów jest mniej, niż się zamówiło.**
  Rdzeń przyjmuje tożsamość tylko wtedy, gdy przejdzie sześć warunków naraz —
  jeden wielki AND — a wiersz, który poległ, znikał bez słowa. Operator prosił o
  tysiąc, dostawał sześćset pięćdziesiąt i nie miał się czego chwycić.
  Teraz przy starcie idzie jedna linia z rozbiciem. U nas wygląda tak:
  `registry rows=1182 usable=1012 rejected: login=170 social_id=101
  other_characters=62` — czyli sufitu nie wyznacza suwak w launcherze, tylko
  konta, których login albo social_id nie pasuje do wzorca generatora.

- **Skrypt, który jedzie tylko w instalatorze, to skrypt nie do naprawienia.**
  Zgłoszone z Discorda: instalacja bez `panel/bin/apply_rates.sh` nie miała skąd
  go wziąć. Sprawdziliśmy całość i było gorzej: **z dziewięciu skryptów
  kontenera gry aktualizacja wysyłała jeden.** `m2-rates`, `m2-supervise`,
  `m2-gm`, `m2-lang`, `entrypoint.sh` — czyli wszystko, czym ten kontener jest
  sterowany — nie jechało nigdy. Teraz idzie całe `bin/` obu kontenerów.

- **Panel zaawansowany zamieniał miejscami dwa bonusy.** Zgłoszone z Discorda ze
  zrzutem: w rankingu „ŚR" i „UM" pokazywały się na odwrót względem opisu
  przedmiotu w grze. Kanoniczna tabela mówi `71 = Obrażenie Umiejętności`,
  `72 = Średnie Obrażenia`, a zapytanie panelu miało te dwa numery zamienione.

- **Wędkarz stał nad wodą godzinami i twierdził, że zakłada przynętę.** Zgłoszone
  z Discorda jako „boty zakładają przynętę na bronie" — i tu trzeba powiedzieć
  wprost: **na broń nic nie było zakładane.** Kod przynęty sprawdza, czy w ręce
  jest wędka, i odmawia. Kłamał napis: był zwykłym „w przeciwnym razie", więc
  każdy wędkarz nad wodą, który akurat nie zarzucił, ogłaszał zakładanie
  przynęty — również taki, który wędki nie miał na sobie w ogóle. Teraz w tym
  przypadku pisze „Szukam wędki".
  Prawdziwy był drugi zarzut: **nic nie ograniczało tego stania.** Zegar
  pilnował zarzutu, który nie bierze, ale kroku wcześniej — bota gotowego do
  łowienia, który nie zarzuca ani razu — nie pilnowało nic. Sesja bez jednego
  zarzutu kończy się po dwóch minutach i zostawia w logu powód razem ze stanem
  wędki i przynęty.

### Zmienione

- **Pustynia przestaje być korytarzem.** To najbogatsza mapa tego świata —
  **14 026 punktów odrodzenia** przeciwko 8122 w Dolinie Orków — a polowało na
  niej wyłącznie pasmo 30–35. Wszyscy od 36 wzwyż szli do Doliny, więc mapa, na
  której sam Król Skorpion stoi w 2234 miejscach, służyła za przejście do Lochu
  Pająków. Pasmo 36–47 dzieli się teraz między Dolinę i pustynię, tak jak 30–35
  już się dzieliło.
  Zmierzone: Dolina Orków z 308–388 botów na **141**, pustynia z 97–123 na
  **232**. Przy okazji spadło obciążenie — rozłożenie ludzi na więcej map ścięło
  pracę nawigacji do jednej trzeciej: `tick_ms` z trzynastu–dziewiętnastu tysięcy
  na **5244**, odrzuconych tras z kilkuset na **jedną**.

- **Droper medali nie czeka już na swoją ambicję.** Zdobywanie medali to całość
  tego, po co ta osobowość istnieje, a ambicja się rotuje: siedemdziesiąt trzy
  boty z ośmiuset trzydziestu ośmiu miały ją w danej chwili, więc trzynastu
  droperów siedziało bezczynnie dziewięć razy na dziesięć i wszystkie trzy Lochy
  Małp stały niemal puste. Reszta botów nadal potrzebuje ambicji, więc loch nie
  zamienia się w taśmociąg.

---

## 1.30.22 — 2026-09-08

### Naprawione

- **Instalacje na Linuksie i VPS-ach nie dawały się zaktualizować.** Zgłoszone z
  Discorda z dokładną diagnozą, za którą dziękujemy: łatka silnika
  `0008-warp-npc-ignores-playerbots.patch` jest ucięta. Jej nagłówek deklaruje
  `@@ -6447,7 +6447,19 @@`, a treść niesie sześć starych i osiemnaście nowych
  linii — brakuje ostatniej linii kontekstu z zamykającą klamrą. `patch` odrzuca
  taki plik, `prepare-context.sh` przerywa i aktualizacja staje w pół drogi.
  Dotyczyło to wydań od 1.30.13 do 1.30.20; w 1.30.12 tej łatki jeszcze nie ma.
  Brakująca linia jest dopisana, a łatka przechodzi teraz próbę na sucho z
  `--fuzz=0` przeciwko czystemu drzewu portu.
  Dlaczego nikt tego u nas nie złapał: **na Windowsie ta łatka w ogóle się nie
  uruchamia** — launcher wgrywa gotowy, już załatany `char.cpp`, a
  `prepare-context.sh` odpala się tylko tam, gdzie serwer buduje się ze źródeł.
  Cały nasz tor testowy jest windowsowy, więc trafiło to wyłącznie w instalacje,
  których nie testujemy. To się zmienia: próba na sucho z `--fuzz=0` wchodzi na
  stałe do sprawdzania łatek.

- **Jeden uszkodzony plik potrafił zatrzymać cały serwer i żadna aktualizacja go
  nie naprawiała.** Zgłoszone z Discorda: „nie mogę aktualizować ani grać".
  W logu widać `linux-port/docker/panel/Dockerfile` o rozmiarze **dwóch bajtów**
  (u nas ma prawie pięć kilobajtów). `docker compose up` buduje trzy obrazy
  jednym przebiegiem, więc panel wywalał się na pierwszym kroku, a gra i panel
  zaawansowany szły z nim (`CANCELED`) — nie wstawało nic. Rada z komunikatu,
  żeby kliknąć GRAJ ponownie, nie miała jak pomóc, bo nic tego pliku nie
  zastępowało: **z siedmiu Dockerfile'ów, które budują się na maszynie gracza,
  aktualizacja wysyłała tylko ten od gry.** Teraz jadą wszystkie, razem z plikami
  `.dockerignore`, więc następna aktualizacja odbudowuje uszkodzony plik sama.

- **Dwóch wędkarzy stawało na jednym stanowisku.** Dwie przyczyny, obie po
  naszej stronie. Po pierwsze, po wyborze miejsca kod dociągał je do
  „najbliższej chodzalnej komórki" w promieniu **dwunastu komórek, czyli
  sześciuset jednostek**, przy promieniu przybycia równym dwudziestu pięciu —
  więc dwa stanowiska odległe o sto pięćdziesiąt mogły trafić na jedno miejsce.
  To ten sam błąd, który w tym samym wydaniu naprawiliśmy w marszu do portalu.
  Po drugie, i głębiej: **nawigacja ocenia komórkę, próbkując jej środek**
  (`podstawa + n*50 + 25`), a stanowiska były generowane na wielokrotnościach
  pięćdziesięciu, czyli na rogach komórek. Sprawdzaliśmy jeden punkt, a silnik
  sądził po sąsiednim. Cała tablica jest przeliczona na środki komórek.

### Nowe

- **Świątynia Hwang.** Mapa 65 wchodzi jako szósty teren dla botów, dla postaci
  od 52 poziomu, obok Sohanu i Lochu Pająków. Wszystko wzięte z plików samego
  serwera, nie z opisu: 5088 punktów odrodzenia, potwory od 52 do 61, mediana
  56. Zachodnia połowa to Elit. Ezoterycy 52–55 i tam się na mapę wchodzi,
  wschodnia to Drzewne Żółwie i Straszydła 55–58 — i tak samo dzieli się
  dziewięć miejsc łowieckich, policzonych z gęstości odrodzeń i sprawdzonych co
  do jednego, czy da się na nich stanąć.
  Metinów tam nie ma; to, co u innych map jest plikiem kamieni, tutaj jest
  szesnastoma żyłami rudy. Nie ma też miejsca zbiórki na bossa: dwa punkty
  losują wśród trzech ras, a jedna z nich to Zjawa Żółtego Tygrysa na
  siedemdziesiątym piątym poziomie — nie ma po co wysyłać tam pięćdziesiątki.
  **Prawdziwym powodem jest jednak drop.** Język Żaby, Żabie Udka, Liść,
  Nieznany Talizman+ i Księga Klątw+ występują w osiemnastu recepturach
  ulepszania i nie wypadały na żadnej mapie, po której chodziły boty — rejestr
  rynku prosił o pierwszy z nich przy podaży dokładnie zero. Botom nie trzeba
  było o nich mówić ani słowa: kod czyta tabelę receptur samego silnika, więc
  stały się towarem w tej samej chwili, w której bot mógł stanąć tam, gdzie
  wypadają.

### Zmienione

- **Koniec ze staniem w mieście — boty chodzą po straganach.** Poproszone na
  Discordzie. Postój po załatwionych sprawach skrócony z czterech–dziesięciu
  minut do około trzech, a zamiast stać w jednym punkcie rynku bot co
  sześć–czternaście sekund wybiera inną ladę i tam idzie; nad głową pisze
  „Oglądam stragany". Lada, do której nie potrafi dojść, jest porzucana po
  dwudziestu sekundach — bez tego jeden bot ze zniszczoną trasą stałby przez
  cały postój, czyli dokładnie tak, jak przedtem.
  Zmierzone: cztery boty z napisem 'Ogladam stragany', wszystkie w ruchu — zaden nie trafil na liste stojacych.

- **Wędkarze stoją wzdłuż rzeki, nie na trawie.** Poprzednie wydanie rozstawiło
  ich na prostokącie, a ta rzeka się wije — brzeg biegnie od x 69 900 na północy
  przez 67 200 w środku do 67 800 na południu — więc prostokąt dość szeroki, by
  pomieścić pięćdziesiąt osób, sięgał tam, gdzie wody nie ma wcale.
  Stanowiska są teraz tablicą, tak jak miejsca łowieckie są tablicą: każda
  chodzalna komórka wzdłuż rzeki, posortowana po odległości do wody i przyjęta
  tylko wtedy, gdy żadne przyjęte wcześniej nie leży bliżej niż sto pięćdziesiąt
  jednostek. **162 miejsca**, każde najwyżej trzysta pięćdziesiąt jednostek od
  wody. Każde niesie też własny punkt wody, bo obracanie się na wschód jest
  poprawne dla jednego prostego odcinka i błędne wszędzie tam, gdzie rzeka
  skręca. Zmierzone: trzydziestu wedkarzy rozlozonych wzdluz rzeki, najblizsza para dwanascie jednostek od siebie. To wciaz mniej niz zalozone poltora metra i nie jest to jeszcze wyjasnione — osobno tloczy sie tez kolejka dwudziestu dziewieciu botow po przynete u Rybaka, ktora rozstawia zupelnie inny kod.

---

## 1.30.21 — 2026-09-08

### Naprawione

- **Bot sprzedawał prezent zamiast go założyć.** Zgłoszone z Discorda trzy razy
  jednego popołudnia przez ludzi, którzy właśnie dali botowi coś dobrego: buty
  +9 z trzema bonusami i 1500 HP wylądowały na ladzie zamiast na nogach.
  To nie była ocena przedmiotu, tylko wyścig w takcie. Stragan uznaje za „zapas"
  każdą broń i zbroję, której slot jest już zajęty — a prezent nie zajmuje
  pustego slotu, tylko bije zajęty. Pas straganu stoi w takcie kilkaset linii
  przed pasem ekwipunku, więc lada dostawała rzecz pierwsza, i to na sam szczyt:
  „zapas +6 lub lepszy" to najwyżej punktowany towar, jaki stragan może nieść.
  Teraz przed uznaniem czegoś za zapas pada pytanie, czy bot powinien to na
  sobie mieć — silnikowe `CanEquipNow` plus ta sama punktacja, której używa pas
  ekwipunku, więc liczą się linie bonusów i HP. Granica jest przy „może założyć
  **teraz**": miecz na trzydziesty poziom w plecaku bota z piątego zostaje
  towarem. Do handlarza NPC nic takiego nigdy nie trafiało — tam +6 i wyżej jest
  wykluczone; jedyną drogą utraty prezentu była lada.

- **Setki botów stały w miejscu, twierdząc, że idą.** Zgłoszone z Discorda przez
  dwie osoby: „nie chcą iść tam, gdzie piszą, że idą" i „500 na 750 botów stoi
  w kółku". U nas stało 148 z 838, z czego 53 w podróży, która nigdzie nie
  prowadziła. Złożyły się na to trzy rzeczy.

  Po pierwsze, **budżet planowania tras liczył sztuki**. Na minutę przy 839
  botach: 7440 planów, z tego 7086 to skoki do sąsiedniego potwora kosztujące
  **41 milisekund razem**, a całe jedenaście z dwunastu sekund zjadało 148
  długich tras. Skok za sześć mikrosekund zajmował dokładnie ten sam slot co
  przejście przez Dolinę Orków — i to długie trasy były wypychane z kolejki.
  Połowa wszystkich żądań odrzucana, jedno z nich czekało **trzynaście minut**.
  Plan kosztuje teraz wedle dystansu, a budżet mikrosekundowy dalej ogranicza
  cały takt.

  Po drugie, **żądanie odrzucone dwadzieścia razy przestaje stać w kolejce** za
  licznikiem. Dalej podlega budżetowi czasu i limitowi dalekich tras — to nie
  otwiera dziury, tylko przestaje trzymać jednego bota na końcu kolejki na
  zawsze.

  Po trzecie, **zsiadanie z konia zjadało takt ucieczki**. Zatrzymany marsz do
  portalu oddaje takt właśnie po to, żeby bot poszedł gdzie indziej i zaplanował
  od nowa; pas „koń transportowy nie walczy" ten takt zabierał, bot wsiadał z
  powrotem i stał kolejne dwadzieścia sekund. Czterdzieści sześć botów robiło to
  na wyjściu z Sohanu, wsiadając i zsiadając co dwadzieścia sekund bez jednego
  kroku. Teraz marsz sam zsiada, zanim odda takt.

  Zmierzone po wdrożeniu: odroczeń 3738 → 264 na minutę, zagłodzonych żądań
  170 → 0, botów stojących w podróży 53 → 13.

- **Marsz do portalu celował o kilometr obok.** Promień dociągania celu wynosił
  24 komórki nawigacji, czyli 1200 jednostek świata, a przejście przez portal
  testuje 200. Trasa mogła się legalnie skończyć kilometr od bramy: bot stawał
  na jej końcu, po dwudziestu sekundach marsz się poddawał, a następny takt
  planował to samo. To ta sama pułapka, którą zastawił kiedyś marsz do NPC w
  mieście, i ta sama zasada ją zamyka — dociągnięty cel musi zmieścić się w
  promieniu, który testuje przybycie.

### Zmienione

- **Wędkarze stoją metr od siebie.** Zgłoszone z Discorda ze zdjęciem: kilkadziesiąt
  tabliczek z imionami w jednym stosie i jeden widoczny bot pod spodem. Wszyscy
  szli na tę samą łatkę dwa na cztery metry, a gniazd było pięćdziesiąt co pół
  metra i rozdawał je sam hash — przy pięćdziesięciu wędkarzach i pięćdziesięciu
  gniazdach kolizje są regułą.
  Brzeg został zmierzony z `server_attr` mapy Joan: stojący grunt z otwartą wodą
  na wschód biegnie przez 2250 jednostek. Na nim leży **dziewięćdziesiąt
  stanowisk, sześć kolumn na piętnaście rzędów co 150 jednostek**, każde
  sprawdzone. Stanowisko jest zajmowane na czas sesji i zwalniane razem z nią,
  jak miejsce przy ladzie; zajęcie, którego nikt nie dotknął przez dwie minuty,
  wygasa, żeby bot wylogowany w połowie zarzutu nie trzymał miejsca na zawsze.
  Promień „dotarłem" zszedł z 200 na 25 jednostek — to on decyduje, jak blisko
  siebie kończą dwaj sąsiedzi, i przy dwustu można było stanąć na cudzym
  miejscu.
  Silnik niczego tu nie narzuca: `CHARACTER::fishing()` liczy punkt czterysta
  jednostek przed postacią i nigdy go nie odczytuje, a jedyny teren, jaki
  sprawdza, to kafelek pod nogami.

### Dla budujących z repozytorium

- **Szybki build kopiuje teraz skrypty kontenera.** Do tej pory podmieniał
  wyłącznie skompilowany rdzeń i brał całą resztę z warstwy bazowej, czyli z
  tego, co zostawił ostatni pełny build. Serwer testowy cicho odjeżdżał od tego,
  co dostają gracze: jego warstwa bazowa nosiła przydział map sprzed
  przeniesienia Lochu Pająków V1 i Trudnego Lochu Małp na rdzeń botów, więc
  `m2-render-config` pisał `MAP_ALLOW` bez nich, każdy bot dochodzący do którejś
  z tych bram był odprawiany z kwitkiem **ponad dziesięć tysięcy razy na
  minutę**, a cały dzień pomiarów mówił, że te mapy są puste z powodów, które
  nigdy nie były prawdziwe. Wydania tego nie dotyczyło — `m2-render-config`
  jedzie w paczce aktualizacji od zawsze i u gracza, który się zaktualizował,
  przydział był poprawny.
  Po naprawie narzędzia i przebudowie: V1 z 0 na 55 botów, Trudny Loch Małp z 0
  na 18, Średni z 6 na 14, a korek tranzytowy na pustyni ze 123 na 77.

---

## 1.30.20 — 2026-09-08

### Naprawione

- **Jeden yang za medal konny.** Zgłoszone z Discorda: boty dostały medale konne
  i zaczęły je wystawiać po jednym yangu. `item_proto` daje medalowi (50050) cenę
  zero, więc wycena mnożyła zero przez marżę handlarza i wychodziło z niej
  `max(1, 0)`. Dokładnie tak samo stały wszystkie skrzynki i szkatułki — 50011,
  50192 i 50193 też mają tam zero. Gorzej: taka cena zostawała na zawsze.
  Ogranicznik kroku pozwala ruszyć kotwicą o pięć procent na dziesięć minut, a
  pięć procent z jednego yanga to w liczbach całkowitych zero — więc kotwica
  poniżej czterech yangów nie mogła się już nigdy ruszyć, a każdy stragan
  wystawiający ten przedmiot odświeżał jej zegar, zanim zdążyła się zestarzeć.
  Teraz przedmiot, którego handlarz nie kupi, dostaje własną cenę wyjściową
  (medal konny 400 tys., reszta 30 tys.), kotwica poniżej stu yangów jest
  traktowana jak pomyłka i liczona od nowa, a krok ceny przesuwa ją o co najmniej
  jednego yanga. Zmierzone po wdrożeniu: 39 otwartych straganów i ani
  jednej linii poniżej stu yangów, przy medianie 150 000 yang.

- **Stragan bez zawiniątka kręcił bota w kółko.** Zgłoszone z Discorda razem ze
  zrzutem logu: osiem identycznych linii `no room for bundle` na sekundę dla
  jednego bota, który chodził w tę i z powrotem wzdłuż jednej linii. Gdy plecak
  jest pełny, nie ma gdzie położyć kupionego zawiniątka — a kod nie ustawiał
  wtedy żadnego zegara, więc pytał ponownie na każdym takcie i sam sobie odbierał
  kolejkę, w której handlarz opróżniłby plecak. Teraz odczekuje od minuty do
  trzech, a linia w logu jest dławiona jak każda inna.

- **Biolog stawał na Korzeniu Gango.** Zgłoszone z Discorda: Sura czterdziestego
  drugiego poziomu z celem „Etap Biologa: Korzeń Gango 0/5", bijąca orki. Wybór
  misji szedł trzema przebiegami — co bot niesie, co stoi na jego mapie, pierwsza
  niezrobiona — i ten trzeci wręczał postaci z Doliny Orków zbiórkę z poziomu
  piętnastego, której potwór stoi w Joan. Bot tam nie chodzi, więc łańcuch stawał
  na tym wierszu i nie ruszał się dalej. Siedem etapów Biologa to siedem osobnych
  zadań, nie jeden łańcuch, więc wiersz wyrośnięty o dziesięć poziomów jest teraz
  pomijany zamiast blokować wszystko za sobą, a gdy wyrośnięte są już wszystkie,
  bot bierze najwyższy, jaki mu został — czyli Ząb Orka, po który i tak chodzi.

- **Konsola restartu w panelu zaawansowanym nikogo o restart nie prosiła.**
  Zgłoszone z Discorda: „Zleć restart" nic nie robi, a konsola stoi na „Ostatni
  ukończony restart: brak zarejestrowanych danych". Panel publikował własny plik
  `server-settings.request`, którego po stronie gry nie czyta nikt — kontener
  pilnuje pliku `request` i tylko jego. Nikt tego pliku również nie kasował, więc
  po pierwszym kliknięciu każde następne było odrzucane jako „poprzednie zlecenie
  nadal trwa", i to na stałe. Teraz oba przyciski zlecają restart tą samą drogą,
  którą od zawsze działa strona mnożników, a blokada podwójnego kliknięcia wygasa
  po dziesięciu minutach, żeby milczący kontener nie zablokował konsoli na dobre.
  To jest ta droga, którą wchodzą zmiany wprowadzone ręcznie w bazie: rdzenie
  czytają `item_proto` tylko przy starcie.

### Zmienione

- **Nieotwarte skrzynki trafiają na lady.** Zaproponowane na Discordzie, żeby boty
  wystawiały blaski i szkatułki zamiast otwierać wszystko. Większość i tak się
  otwiera — stamtąd biorą mikstury i wzmocnienia — ale dwa rodzaje idą na ladę:
  te, których silnik na tym serwerze nie otworzy w ogóle (50192 i 50193, w tej
  chwili blisko osiemset sztuk zajmujących po jednej komórce w plecakach), oraz nadwyżka stosu
  od pięciu sztuk w górę. Nieotwarte pudełko to jedyna rzecz na tym rynku, na
  którą gracz może zagrać w ciemno, a dotąd nie było ani jednego. Bot nie próbuje
  otworzyć pudełka, które sam wystawił: silnik odmawia na zablokowanym
  przedmiocie, a taka odmowa jest zapamiętywana dla całego świata i uciszyłaby
  otwieranie u wszystkich. Kupującym jest tu gracz, nie bot: żaden bot nie ma w
  sobie powodu, żeby kupić pudełko, i to się nie zmienia. Zmierzone: cztery
  odmowy silnika zapisane w ciągu pół godziny, czyli warunek wystawienia jest
  spełniony — sama lada nie jest w logu widoczna poza najlepszą linią, więc
  pudełko na straganie zobaczycie w grze, nie w pomiarze.

### Czego w tym wydaniu nie ma

- Respawn ustawiany z panelu zaawansowanego nadal nie działa — po stronie gry nie
  ma modułu, który zapisywałby pliki `regen.txt`. Panel przestał przynajmniej
  twierdzić, że zadziałał.
- Stackowanie odłamków: `Odłamek Smoczego Kamienia` (30270) nie ma w `item_proto`
  flagi stosu, więc każda sztuka zajmuje osobną komórkę. To zmiana w danych
  serwera i klienta naraz, nie w kodzie botów.
- Zakładki w składzie i misja na powiększenie plecaka — to funkcje silnika, nie AI.

---

## 1.30.19 — 2026-09-08

### Nowe

- **Launcher po angielsku.** Na dole okna jest przycisk „JĘZYK / LANGUAGE" —
  przełącza całe okno między polskim a angielskim i zapamiętuje wybór w
  `.m2launcher.json`, więc następne uruchomienie startuje w wybranym języku.
  Przetłumaczony jest interfejs launchera: przyciski, nagłówki, okna wyboru
  botów, panelu i importu bazy. **Panele WWW zostają po polsku** — to dwie
  osobne aplikacje z ponad tysiącem własnych napisów każda i ich tłumaczenie to
  oddzielna robota, nie dopisek do tego wydania.

### Naprawione

- **Boty wreszcie chodzą do Biologa.** Ząb Orka stał na 0/10 dla całego świata,
  a 700 botów nosiło 2219 zębów w plecakach — po trzy na głowę. Powód: wyprawa
  do Biologa zaczynała się dopiero, gdy bot miał **cały** brakujący komplet
  naraz, czyli dziesięć sztuk. Miało to dziesięć osób w całym świecie.
  Tymczasem samo oddawanie zawsze działało po jednej sztuce, z sześćdziesięciu-
  procentową szansą przyjęcia — więc na komplet nie było na co czekać. Teraz
  wystarczą cztery sztuki, żeby wyprawa się opłacała, i ten sam próg otwiera
  drogę z Bokjung do Joan. Zmierzone przez pierwsze minuty po wdrożeniu:
  1742 oddanych okazów.
- **Szanse w małżu były cztery razy zawyżone.** Kod AI zakładał 10% białej
  perły, 7% niebieskiej i 3% krwawej. Silnik ma dwie tabele wybierane flagą
  `g_iUseLocale`, a `common.locale` tego świata to **english**, co ustawia tę
  flagę — czyli obowiązuje druga tabela: 50% kamień, 45% nic, **2% / 2% / 1%**
  na perły. Każda decyzja „otworzyć czy sprzedać" liczyła się więc z wartości
  cztery razy za wysokiej. Stałe poprawione na te, które silnik naprawdę
  stosuje.
- **Aura Miecza przestaje kosztować tyle co byle księga.** Rynek pamiętał ceny
  po numerze przedmiotu, a każda zwykła księga to ten sam numer 50300 — skill
  siedzi w gnieździe. Sprzedaż czyjejś zbędnej księgi ustawiała więc cenę Aury,
  a sprzedaż Aury cenę wszystkich pozostałych. Klucz rynku zawiera teraz
  umiejętność: każda księga ma własną historię cen, własny limit kroku ceny i
  własną cenę startową (Aura 250 tys., Czarowane Ostrze 220 tys., Silne Ciało
  180 tys., zwykłe od 45 tys.). Perły dostały to samo — 2 / 3 / 6 mln.
- **Boty kupują wreszcie księgi umiejętności.** W kodzie kupującego nie było
  dla nich żadnej gałęzi, więc żaden bot nigdy nie kupił księgi ze straganu —
  same wisiały. Teraz bot bierze księgę swojej profesji i swojego skilla,
  dopóki nie ma jeszcze roboczego zapasu i dopóki skill da się jeszcze
  podnieść. Bez kupujących samo podniesienie ceny zrobiłoby tylko drogie,
  niesprzedające się sklepy.
- **Księga obcej profesji nie idzie już do handlarza za grosze.** Aura
  znaleziona przez ninję była złomem — teraz trafia na stragan, gdzie stoi po
  nią wojownik.
- **Cena mogła skoczyć przy każdym wywołaniu.** Ogranicznik liczył
  `1 + czas/interwał`, więc nawet przy zerowym czasie dawał jeden pełny krok, a
  każdy krok zerował zegar. Czterdzieści straganów otwartych w tej samej minucie
  przesuwało wspólną kotwicę czterdzieści razy, mimo komentarza o pięciu
  procentach na dziesięć minut. Teraz liczą się wyłącznie pełne interwały.

### Zmienione

- **W Bokjung stoi najwyżej siedem straganów.** Ósmy kupiec zabiera towar do
  Joan zamiast dokładać ladę, której i tak nikt nie zobaczy. A kupujący
  zaglądają **najpierw do Joan** — dopiero gdy tam niczego nie znajdą, przez
  dziesięć minut wolno im szukać w Bokjung. To jest to, co ożywia drugie
  miasto: nie sam stragan, tylko klienci, którzy do niego przychodzą.
  Zmierzone: 20 straganow w Joan przeciwko dokladnie siedmiu w Bokjung straganów przeniesionych do Joan i 300 żywych
  botów na jej mapie zamiast dziewiętnastu z rana.

---

## 1.30.18 — 2026-09-08

### Naprawione

- **Niedokończona sprawa w mieście przestaje trafiać do przypadkowej walki.**
  To była pętla opisana w audycie z 8 września i widać ją w logu jak na dłoni:
  bot potrzebuje handlarza → trasa zostaje odroczona przez budżet planowania →
  po dziewięćdziesięciu sekundach bezruchu budzi go watchdog → wizyta zostaje
  skasowana → sekundę później bot rzuca umiejętność w pierwszego moba obok, a
  potrzeba, po którą przyszedł, dalej jest niezaspokojona. I tak w kółko, z
  zapasami topniejącymi po drodze.
  Watchdog nadal robi to, do czego służy — kasuje martwą trasę i odblokowuje
  postać — ale **sprawa zostaje przy bocie**. Dostaje własny termin ponowienia
  (15–40 s), a do tego czasu bot jest klientem, nie myśliwym: zwykłe walki są
  dla niego zamknięte, obrona nie. Sprawa, której nie da się załatwić przez
  piętnaście minut, jest jawnie porzucana z powodem w logu, żeby nic nie mogło
  utknąć na zawsze.
- **Bokjung przestaje być expowiskiem dla tych, którzy z niego wyrośli.**
  Filtr opłacalności odcinał skrajnie słabe cele, ale nie zabraniał grindu na
  mapie, z której bot już wyrósł — a to dwie różne reguły i audyt słusznie się
  tego czepił. Powyżej progu kohorty (35) Bokjung jest miejscem, gdzie można
  kupować, przechodzić, handlować i **dokończyć konkretne zadanie** — z nazwanym
  potworem albo z materiałem, którego naprawdę brakuje i który realnie z niego
  wypada. Samo „mam ambicję Metiny" albo „mam ambicję konia" nie jest zgodą na
  polowanie.
  Reguła obowiązuje wszystkie drogi do walki, nie tylko wybór nowego celu:
  wspólny cel drużyny, cel już trzymany, przeciwnik zaangażowany, podciąganie
  grup i lur łucznika. Ratowanie życia jest poza nią — bot bije się z tym, co
  bije jego, gdziekolwiek stoi.
- **Zamiar wyjazdu przeżywa wizytę i watchdog.** Bot, który wrócił do miasta po
  zapasy, pamięta teraz, dokąd zmierzał. Zakupy odraczają wyjazd, ale go nie
  kasują, a po załatwieniu sprawy nie trzeba czekać na kolejne losowanie
  ambicji. W logu widać to jako `PLAYERBOT_DEPARTURE: held`.

### Dla ciekawych

- **Odroczenie trasy mówi wreszcie, które ograniczenie ją wstrzymało.** Trzy
  różne budżety — plany na takt, czas planowania na takt i dalekie trasy na
  minutę — zwracały jedną i tę samą odpowiedź, więc „odroczono" bywało czytane
  jako „nie ma drogi". Log podaje teraz powód i to, jak długo bot czeka — i od
  razu się to opłaciło: przez 26 minut **108 odroczających to limit planów na
  takt, 10 to budżet czasu, a limit dalekich tras nie zatrzymał ani jednej**.
  Audyt ostrzegał, żeby nie zakładać, że chodzi o dalekie trasy, i miał rację.
- **Każdy bot 40+ w Bokjung ma odczytywalny powód i następny krok.** Rdzeń pisze
  `PLAYERBOT_M2: why` dla rotacyjnej garstki botów na minutę: poziom, cel, czy
  wolno mu tu polować, stan sprawy i jej wiek, mapa docelowa wyjazdu, stan
  mikstur, aktualny cel walki wraz z powodem, w którym miejscu jest trasa i jak
  długo czeka na planowanie. To odpowiedź na zarzut audytu, że po samym opisie
  akcji nie da się orzec, czy bot działa sensownie.
  Zmierzone przez 26 minut po wdrożeniu: 2870 odmów walki z powodu
  reguły mapy, 422 zapamiętanych zamiarów wyjazdu, 9 (z czego 4 zakonczone zakupem, zero porzuconych)
  spraw przejętych przez naprawę po watchdogu.
- Czego **nie** zrobiłem z tego audytu: sprawiedliwej kolejki planowania tras z
  wiekiem zlecenia i rezerwacją części budżetu dla usług krytycznych. To
  przebudowa gorącej ścieżki wydajnościowej i chcę ją mierzyć osobno, a nie
  doklejać do wydania naprawiającego pętlę usług. Odroczenia są na razie tylko
  opisane w logu.

---

## 1.30.17 — 2026-09-08

### Zmienione

- **Bot mówi nad głową, co robi i po co.** Do tej pory opis mówił o czynności i
  milczał o jej celu — a czasem obie części sobie przeczyły: „Szukam miejsca do
  expa (cel: zapasy)" wisiało nad postacią idącą do handlarza. Teraz:
  - podróż nazywa miejsce i powód — „Idę do Doliny Orków (cel: poziom)",
    „Idę do miasta po zapasy", „Idę do kowala ulepszyć ekwipunek", „Idę do
    Biologa", „Idę nad rzekę łowić ryby";
  - wizyta po zapasy pokazuje stan mikstur, czyli jedyną rzecz, którą da się
    sprawdzić w ekwipunku — „Kupuje potki i sprzedaje lup - potki 143/86";
  - walka mówi, **dlaczego ta walka** — „Bronię się przed …", „Pomagam
    drużynie: …", „Zbieram materiał z …" zamiast samego „Walczę z …". Powód
    bierze się z tego samego modułu, który decyduje, czy walka ma sens, a nie
    ze zgadywania po tekście.
- **Joan ma po co żyć.** Bot, który załatwi sprawę albo skończy łowienie w Joan,
  zostaje teraz na cztery do dziesięciu minut na rynku zamiast wychodzić w pole w
  tej samej sekundzie —
  a wędkarzy jest więcej (8 na stu zamiast 2, a wśród rozważnych zbieraczy 30
  zamiast 20). Łowienie to jedyna czynność, która sama z siebie prowadzi bota do
  Joan: brzeg, Rybak sprzedający przynętę i pierścień straganów leżą na tej
  samej mapie, więc wędkarz jest przy okazji klientem i sprzedawcą.
  Bokjung świadomie pominięty — tam tłok jest problemem, nie brakiem.

### Dla ciekawych

- Poprzednia wersja opierała się na złym pomiarze i trzeba to sprostować:
  kolumna `map_index` w bazie to **ostatnia zapisana pozycja każdego
  zarejestrowanego bota**, także tych niewłączonych. Mówiła o 400 botach na
  mapie Joan, podczas gdy plik statusów — a ten zawiera wyłącznie żywe postacie
  — mówił o 19 na 837. Stąd pusty rynek: nie brakowało straganów,
  brakowało ludzi, bo prawie cała żyjąca populacja to poziom 40+, a ci dawno
  wyjechali na pogranicze. Po zmianie w Joan stoi 52 botów — prawie
  trzy razy więcej, i to zasługa samych wędkarzy.
- Odpoczynek na rynku jest w tej wersji **niepotwierdzony w grze**. Kod jest
  wdrożony i sprawdzony kompilatorem, ale jego wyzwalacze — załatwiona sprawa w
  Joan albo skończona sesja wędkarska — zdarzają się rzadko, a sesja trwa od
  piętnastu do czterdziestu minut, więc każde przebudowanie serwera zerowało
  licznik, zanim cokolwiek zdążyło się wydarzyć. Zobaczymy to dopiero po
  dłuższej pracy bez restartu.

---

## 1.30.16 — 2026-09-08

### Naprawione

- **Aura, szał i zaklęcia wchodzą od razu, a nie po kolei co pięć sekund.**
  Rzucenie buffa zajmuje całą turę bota, a kolejnej próby nie było przez pięć
  sekund — więc wojownik potrzebował dziesięciu sekund na aurę i szał, a sura
  broni piętnastu na trzy zaklęcia. Aura Miecza trwa od trzydziestu sekund i ma
  trzydziestosekundowy cooldown, więc bot spędzał na jej odnawianiu tyle czasu,
  ile trwała, i przez większość walk stał bez niej. Po rzuceniu jednego buffa
  bot wraca po następny po 1,2 sekundy; pełny zestaw staje w trzy sekundy
  zamiast piętnastu, a zwykłe pięć sekund wraca, gdy niczego nie brakuje.
- **Trująca chmura i trująca strzała nie lecą już w kamień Metin.** Rotacja
  bierze pierwszą umiejętność, która zeszła z cooldownu, a kamień trwa
  wystarczająco długo, żeby zdjąć z listy te dobre — więc w kamień szła ta
  obszarowa, czyli akurat najsłabsza na jeden cel. Trująca Chmura to
  `-(lv*2 + (atk + str*3 + dex*18)*k)` przy `-(atk + (1,6*atk + …))` Szybkiego
  Ataku: jedna wartość ataku wobec dwóch i pół, za te same 1,4 sekundy blokady
  animacji. Kamień nigdy nie jest tłumem, więc bot pomija obszarówki i wraca do
  zwykłych ciosów, które w tym czasie zadają więcej. Silnik jest pytany o flagę
  `SPLASH`, więc serwer z inną tablicą umiejętności też dostanie dobrą odpowiedź.
- **Perła kosztuje jak perła, a nie jak małż.** Ceny na straganach mają podłogę
  liczoną z zasobności kupujących — i ta podłoga była jedna dla wszystkiego. Przy
  medianie portfela 2,6 mln wypadała na ~38 tysiącach, więc małż, którego
  handlarz wycenia na 3 000, i biała perła za 12 000 stały na ladzie w tej samej
  cenie, a krwawa perła ledwie wyżej. Podłoga jest teraz skalowana tym, ile
  handlarz płaci za tę konkretną rzecz — i **tylko w górę**, więc nic nie
  tanieje: biała perła idzie do ok. 150 tys., krwawa do ok. 300 tys., a małż
  zostaje tam, gdzie był.
  Do tego niedobór wreszcie coś znaczy: dopłata za brak towaru mogła podnieść
  cenę najwyżej o jedną trzecią, co przy pięciuset botach szukających materiału,
  którego nie ma na żadnym straganie, nie jest odpowiedzią rynku. Teraz sięga
  dwukrotności.
- **Stragany stają w Joan.** Bot losował miasto dla każdego straganu — dziewięć
  razy na dziesięć Joan — a potem odmawiał otwarcia, jeśli akurat tam nie stał.
  Ponieważ boty z towarem stoją w Bokjung, dziewięć losowań na dziesięć szło do
  kosza i w Joan nie było ani jednego straganu, czyli dokładnie odwrotnie, niż
  to losowanie miało robić. Kupiec handluje teraz w mieście, w którym stoi.
  Na mapie 21 jest 399 botów, a przy jej rynku stało pięciu —
  przeglądanie straganów i tak zawsze czytało pierścień tej mapy, na której jest
  bot, więc stragan w Joan ma od razu swoich klientów.

---

## 1.30.15 — 2026-09-08

### Nowe

- **Ninja łucznik podciąga potwory dla drużyny.** W drużynie pięciu i więcej
  jeden łucznik dostaje rolę lurera: wybiega, budzi paczkę jednym zwykłym
  strzałem, sprawdza, czy faktycznie za nim ruszyła, i prowadzi ją z powrotem
  do towarzyszy, którzy ją przejmują normalną walką.
  Zastępuje to dawne okazjonalne strzelanie w bok, które nie było
  podciąganiem: łucznik pukał w jednego potwora, dopisywał sobie obrażenia,
  gdy prawdziwe wyszły za niskie, i wracał do swojego celu — nikt na to nie
  czekał i nic z tego nie wynikało.
  Kurs ma swoje granice i każda z nich broni przed konkretną wpadką: dwanaście
  sekund na zbieranie, smycz 4500 jednostek od miejsca zbiórki, przerwanie
  przy spadku HP, jeden lurer na drużynę i limit potworów, który drużyna
  podnosi dopiero po kilku udanych kursach. Odbiorca musi żyć i stać na
  miejscu — a to, że sam ninja się oddala, nie unieważnia jego własnej misji.
  Liczy się to, co wróciło, nie to, ile razy strzelono: potwór zabity strzałem
  albo taki, który nie zareagował, nie jest dostarczony. Nad głową widać etap:
  „Luruje dla PT", „Wracam do druzyny: prowadze 9 mobow", „Przekazuje moby".
  Agresja potworów nie jest przy tym nigdzie podmieniana — drużyna przejmuje
  je tak, jak przejmuje wszystko inne, i przejęcie bywa częściowe.

### Naprawione

- **Bot broni się tak długo, jak jest bity.** W 1.30.14 ograniczyliśmy epizod
  obrony do dziesięciu sekund, żeby „ono zaatakowało pierwsze" nie było
  wymówką do grindu. Skutek uboczny był dotkliwy: po dziesięciu sekundach
  potwór, który wciąż zabijał bota, przestawał być dopuszczalnym celem —
  postać stała bezczynnie albo uciekała i ginęła. Przy silnym potworze
  dochodziło do tego drugie zabezpieczenie, które porzucało cel dziesięć
  poziomów wyżej nawet wtedy, gdy ten cel właśnie bił.
  Obronę własną ogranicza teraz **smycz, nie zegar**: bot odpowiada temu, co
  go bije, tak długo, jak go bije, ale nie daje się przy tym wywlec z miejsca,
  w którym walka się zaczęła. Od zrywania przegranej walki jest ucieczka i ona
  dalej ma pierwszeństwo. Pomoc towarzyszowi z drużyny zostaje ograniczona
  także czasem — bronić siebie trzeba, pomagać można.
- **Gildia jedzie na bossa dopiero wtedy, gdy ktoś go widzi.** Raz wybrany
  punkt polowania trzymał się cztery minuty **bez ponownego pytania, czy boss
  jeszcze stoi**. Boss padał, a boty dalej szły na współrzędne z tabeli i tam
  stały — stąd te kolumny postaci w szczerym polu. Teraz punkt bossa jest
  sprawdzany na bieżąco i gdy bossa nie ma, boty wracają do swojej roboty.
  Do tego boss przestał być wart tyle, że opłacał się każdemu: **kto go
  zobaczy, woła swoją gildię**, i to ta gildia ma pierwszeństwo do dwunastu
  miejsc, a reszta świata do sześciu. Na żywo: sto czterdzieści pięć postaci
  ruszających na jednego potwora spadło do trzynastu, a w logu pojawiły się
  wołania w rodzaju „Wodz Orkow stoi! Zbieramy sie na niego."
- **Śmierć wygląda jak śmierć gracza.** Ciało leży dziesięć sekund i dopiero
  potem postać wstaje — ale wstawała niewidzialna na dziesięć sekund, dwa razy
  dłużej niż człowiek po `restart_here`. Teraz to te same pięć sekund, które
  silnik daje graczowi.
- **Wędkarz nie tonie we własnym połowie.** Ryby otwierały się tylko między
  zarzuceniami, więc bot, który odszedł od brzegu, nosił je ze sobą — a żywa
  ryba się nie stackuje, więc trzydzieści ryb to trzydzieści pól i plecak bez
  miejsca na cokolwiek innego. Połów jest teraz opracowywany wszędzie, przy
  okazji zwykłego utrzymania. W plecakach świata nie została ani jedna
  nieotwarta ryba.
- **Panel: wróciła ramka okna ekwipunku.** Style panelu odwołują się do
  `inventory-background.png`, którego nie ma w żadnej paczce autora — ani w
  1.37.1, ani w 1.30.1 — więc u wszystkich poza nim okno ekwipunku było płaskim
  tłem z ikonami. Grafika z klienta nie jest nasza do rozprowadzania, więc
  narysowaliśmy własną ramkę w dokładnie tej geometrii, której oczekują style:
  wnęki na dwanaście gniazd ekwipunku, siatka pięć na dziewięć i pasek na yang.
- **Launcher mówi, który plik zablokował antywirus.** Gdy Windows przerywa
  aktualizację komunikatem „plik zawiera wirusa lub potencjalnie niechciane
  oprogramowanie", w logu zostawało samo to zdanie — bez nazwy pliku, czyli bez
  niczego, co dałoby się sprawdzić. Teraz launcher rozpoznaje ten błąd przy
  pobieraniu, przy rozpakowywaniu każdego pliku z osobna i przy wgrywaniu na
  miejsce, podaje ścieżkę, przypomina, że nic nie zostało zainstalowane i że
  poprzednia wersja działa dalej, oraz podpowiada wykluczenie w Zabezpieczeniach
  Windows.

### Zmienione

- **Małże są towarem, nie losem na loterii.** Pierwsze cztery bot trzyma
  zawsze — dwadzieścia sześć receptur zużywa małża takiego, jaki jest, i tyle
  właśnie za niego płacą. Otwiera dopiero nadmiar, i tylko wtedy, gdy
  oczekiwana wartość perły bije cenę całej sztuki.
- **Farba do włosów trafia na stragany, a raz w życiu na głowę bota.** Silnik
  przyjmuje ją wprost, kolor jest trwały, więc bot, który ją znajdzie, farbuje
  się raz — a reszta idzie do sprzedaży zamiast do handlarza jako złom. Osiemset
  postaci przestaje wyglądać jak jedna skopiowana osiemset razy.

---

## 1.30.14 — 2026-09-07

### Zmienione

- **Bot bije to, z czego cokolwiek ma.** Wysokopoziomowe boty koczowały w
  Bokjung i tłukły potwory dwadzieścia poziomów niżej. Silnik mnoży przez
  `aiPercentByDeltaLev` zarówno doświadczenie, jak i drop: piętnaście poziomów
  nad potworem to jeden procent jednego i drugiego, więc z takiego grindu nie
  było nic poza zajętą mapą i zatrzymanym rozwojem postaci.
  Decyzję „czy ta walka ma sens" podejmuje teraz jeden wspólny moduł i
  podejmuje ją tak samo w każdym miejscu, które pyta: przy wyborze nowego
  celu, przy podciąganiu grup i — co trzy sekundy — dla potwora, którego bot
  już bije. Wcześniej filtr działał tylko przy wyborze, więc przeciwnik wzięty
  chwilę przed zmianą zadania był bity do końca.
  Powody, dla których wolno walczyć, są policzalne: realne doświadczenie,
  potwór z aktualnego zadania, brakujący materiał do ulepszenia, kamień Metin
  i ograniczona obrona. Nic więcej.
- **Obrona ma koniec, także kiedy zmienia się napastnik.** Wyjątek „ono
  zaatakowało pierwsze" musi być ograniczony, bo inaczej jest pozwoleniem na
  dowolny grind. W pierwszej wersji epizod obrony był liczony osobno dla
  każdego napastnika — dwa potwory na zmianę utrzymywały go więc bez końca.
  Teraz epizod należy do bota: dopóki trwa, odpiera każdego napastnika w
  swoim czasie i w swoim promieniu, a nowy zaczyna się dopiero wtedy, gdy
  walka faktycznie ustała na kilkanaście sekund. Obrona towarzysza z drużyny
  mieści się w tym samym epizodzie, więc jest ograniczona nie tylko
  odległością, ale i czasem.
- **Potrzeba materiału nie usprawiedliwia polowania na coś, co go nie da.**
  Wyjątek materiałowy sprawdzał, czy botowi rzeczywiście brakuje surowca do
  receptury — i tylko to. Drop podlega jednak temu samemu przelicznikowi
  poziomów co doświadczenie, więc „potrzebuję" potrafiło wysłać postać na
  potwora, z którego ten materiał praktycznie nie wypada. Teraz wyjątek
  wymaga jeszcze realnej szansy dropu.
- **Panel seban latino w wersji 1.37.1, tym razem w całości.** Poprzednie
  wdrożenie było zlepkiem starszej wersji z naszymi poprawkami: brakowało
  grafik, styli i oryginalnego layoutu. Teraz jest pełny pakiet autora —
  szablony, style i 1599 plików statycznych — z zachowanymi dwiema naszymi
  poprawkami: pierwsza migawka kolektora robi się od razu zamiast po pięciu
  minutach, a pulpit nie wywraca się na świeżej bazie, w której nie ma
  jeszcze tabeli migawek. Panel zna też wreszcie wersję serwera, na który
  patrzy, zamiast pokazywać „nieustawiona".
- **„OTWÓRZ PANEL WWW" pyta, który panel.** Panele są dwa, działają
  jednocześnie i żaden nie zastępuje drugiego, więc przycisk nie decyduje za
  nikogo: pokazuje wybór między **oryginalnym panelem** (mapa i sterowanie) a
  **zaawansowanym panelem seban latino** (profile, rankingi, gospodarka,
  obciążenie). Porty odczytuje z `.env` tej instalacji, więc świat, który je
  przesunął, otwiera się pod właściwym adresem.

### Dla ciekawych

- Rdzeń pisze teraz raz na minutę linię `PLAYERBOT_M2: census` — ilu botów od
  czterdziestego poziomu stoi w Bokjung i z jakiego powodu (zadanie,
  materiały, wizyta, podróż, obrona, brak planu) — oraz `PLAYERBOT_M2: left
  after errand` z czasem, jaki bot potrzebował na opuszczenie mapy po
  załatwieniu swojej sprawy. Sama liczba postaci w mieście nigdy nie
  odpowiadała na pytanie, czy coś jest zepsute; te dwie linie odpowiadają.
  Zmierzone na żywo: przez dziesięć minut w Bokjung stało od 45 do 60 botów od czterdziestego poziomu wzwyż i każdy z konkretnego powodu — 297 razy zadanie, 197 razy wizyta u handlarza, 11 razy obrona, ani razu „brak planu"; 222 wyjścia z mapy po załatwionej sprawie, mediana 21 sekund, 182 z nich poniżej pół minuty; 1872 odmowy walki, wszystkie z powodu zerowego doświadczenia.

---

## 1.30.13 — 2026-09-07

### Zmienione

- **Boty dochodzą do portalu, zamiast znikać dziewięć metrów przed nim.**
  Zmiana mapy nigdy nie mogła iść przez silnikowy portal: `WarpSet` każe
  klientowi połączyć się z rdzeniem, który obsługuje mapę docelową, a bot nie
  ma klienta, który by odpowiedział — zostałby po prostu porzucony przy
  portalu. Dlatego przejście robiliśmy po stronie serwera z bezpiecznym
  zapasem 900 jednostek, i to ten zapas widać było w grze: postać biegła do
  wyjścia i znikała w szczerym polu.
  Nowa łatka silnika **0008** każe `warp_npc_event` pomijać boty, więc zapas
  nie musi już chronić przed niczym — przejście robi się 200 jednostek od
  portalu, czyli bot dochodzi pod sam NPC i znika tam, gdzie znika gracz.
  Portale wewnątrz mapy (te w Lochu Małp) działają jak dotąd: to lokalne
  przeniesienie, nie zmiana rdzenia, i łatka ich nie dotyka.
  Sprawdzone na żywo: 398 przejść między mapami w dziewięć minut, w tym
  dwanaście wejść i dwanaście wyjść z Lochu Pająków, i ani jeden zgubiony bot.
- **Poprawka prywatnych straganów dociera wreszcie do graczy.** Łatka 0004
  zamieniła w `OpenMyShop` test `GetPart(PART_MAIN) > 2` na `IsPolymorphed()`
  — bez tego silnik odmawia otwarcia straganu każdemu, kto ma na sobie zbroję,
  również żywemu graczowi. Łatki silnika trafiają do gracza wyłącznie jako
  gotowy plik z listy aktualizacji, a `char.cpp` na tej liście nie było, więc
  ta poprawka od początku działała tylko na maszynie deweloperskiej. Teraz
  plik jest wysyłany razem z resztą.

- **Bot z celem „zapasy" nie zaczynał po nie iść.** Planista i wykonawca
  zadawali dwa różne pytania o mikstury. `NeedsPlayerBotPotions` liczy sztuki i
  mówi „trzeba" przy czerwonych poniżej 150 albo niebieskich poniżej 100 — i na
  tym stawia cel RESTOCK. Wyzwalacz wizyty liczył natomiast **stosy**, patrzył
  wyłącznie na cztery czerwone numery przedmiotów, niebieskich nie widział
  wcale, i odpalał się dopiero przy zerze mikstur w co najmniej w połowie pełnym
  plecaku. Bot z jednym stosem 32 czerwonych i 56 niebieskich nosił więc cel
  „zapasy", którego wizyta nigdy nie zaczynała, i stał w Bokjung bijąc, co
  akurat przeszło obok — 116 botów poziomu 40 i wyżej siedziało tak na M2.
  Wyzwalacz pyta teraz o dokładnie to samo, co planista. Pomiar po zmianie, w
  trzynaście minut: 944 rozpoczęte wizyty u handlarzy (w tym 333 u handlarki
  różności, gdzie kupuje się mikstury), 311 wyjazdów z M2 na mapy dalsze i
  spadek populacji 40+ w mieście do 93. Pętli zakupowych to nie tworzy: wizyty
  nadal bramkuje pięcio-dziesięciominutowy odstęp, a warunek wymaga pieniędzy
  na wyjazd.

  Ustalenie z audytu przekazanego przez Tieru; sprawdzone w kodzie, który
  właśnie zmieniałem, a nie w starszej kopii z buildera.

Zgłoszenie: Tieru.

---

## 1.30.12 — 2026-09-07

### Naprawione

- **Boty siedziały na tysiącach nieotwartych Szkatułek Blasku.** Przebieg
  otwierania kończył się na pierwszej skrzynce, której silnik nie otwiera —
  Skrzynia Eksperta III (50192) i Skrzynia Mistrza I (50193), razem blisko
  sześć tysięcy odmów na minutę — i przez to nigdy nie docierał do szkatułek
  leżących za nimi w plecaku. Do tego stał na samym końcu taktu, za walką,
  łupem, podróżą, miastem i wędrówką, z których każde przejmuje turę.
  Zmierzone przed zmianą: **9723 szkatułki u 587 botów**, największy stos 106,
  a otwieranych 190 na godzinę; plecaki miały średnio 29 zajętych kratek z 90,
  więc miejsce nie było przeszkodą. Po zmianie zaległość zeszła do **887 sztuk
  u 44 botów w osiem minut**, w szczycie 380 otwarć na minutę. Odmowa pomija
  teraz jedną skrzynkę zamiast kończyć przebieg i jest zapamiętywana na
  dziesięć minut, żeby bot nie pytał o to samo co osiem sekund.
- **Stragany wyceniały bonusy na zero.** Wszystko, co ustalało cenę — tabela
  sprzedawcy, pamięć sprzedaży, ogranicznik kroku — jest kluczowane numerem
  przedmiotu i poziomem ulepszenia, czyli dokładnie tą parą, która nie
  odróżnia butów +7 z pięcioma liniami bonusów od butów +7 bez żadnej. Obie
  szły po 150 000. Teraz do ceny dochodzi 25% za każdą linię, dodatkowe 100%
  od czwartej linii wzwyż i 80% za każdą rolkę, na której gracz się zatrzymuje
  (2000 PŻ, 10% krytyku, 10% przebicia, odporność na ogłuszenie), przy suficie
  600%. Premia obejmuje także ceny płaskie za +7, +8 i +9 oraz złom — one
  wychodziły z wyceny wcześniej i to je właśnie widać było na straganach.
- **Diagnostyka launchera meldowała „OK", gdy silnik Dockera odmawiał.**
  `docker info` kończy się kodem 0, choć zamiast wersji wypisuje odmowę demona,
  więc w logu gracza stało „OK: Docker Engine odpowiada (wersja Error response
  from daemon: Docker Desktop is unable to start)" i werdykt „można uruchomić
  serwer" — sześć razy z rzędu. Ostrzeżenie o zepsutym WSL, jedyne, które mówi
  co naprawić, podnosimy tylko gdy silnik jest znany jako wyłączony, więc przy
  fałszywym „OK" nie pojawiało się wcale. Za wersję uznajemy teraz wyłącznie to,
  co wygląda jak wersja; przy każdej innej odpowiedzi diagnostyka pokazuje
  dosłowną treść odmowy demona.

Zgłoszenie: Tieru, OskarPWA, BibiSiu.

---

## 1.30.11 — 2026-09-07

### Zmienione

- **Bot nie czeka już między księgami umiejętności.** Silnik trzyma od
  osiemnastu do trzydziestu godzin przerwy między dwoma czytaniami tej samej
  umiejętności; przełącznik KSIĘGI w panelu i tak ją zdejmował, ale bot
  narzucał sobie w zamian własne pół godziny. Tego półgodzinnego oczekiwania
  już nie ma: bot czyta wtedy, kiedy ma co czytać.
  Nie zmieniło się nic z tego, co decyduje, jak daleko umiejętność zajdzie, bo
  to reguły silnika: 20 000 punktów doświadczenia pobieranych za każde
  czytanie, losowanie o powodzenie i liczba udanych czytań potrzebnych na
  kolejny poziom mistrzowski, aż do G.

  Pomiar na 638 botach pokazał przy okazji, gdzie naprawdę jest wąskie gardło:
  480 botów ma umiejętność na Mistrzu, 326 z nich nosi jakieś księgi, ale tylko
  **44 mają księgę tej właśnie umiejętności**. Czytań przybyło (z jednego na
  dziewięć minut do jednego na dwie i pół minuty na całą populację), lecz
  tempo rozwoju umiejętności ogranicza teraz podaż właściwych ksiąg, a nie
  żaden zegar.

Zgłoszenie: Tieru.

---

## 1.30.10 — 2026-09-07

### Nowe

- **Każdy bot nosi Trzecią Rękę.** Z nią silnik dopisuje yang z zabitego
  potwora prosto do sakiewki zabójcy, zamiast rozsypywać kupki monet po ziemi
  (`CHARACTER::RewardGold` pyta o `UNIQUE_GROUP_AUTOLOOT`). Dla bota to nie
  wygoda, tylko czas: dojście do każdej kupki kosztuje wyliczenie trasy, a te,
  do których nie zdąży, i tak znikają — teraz bot zamiast biegać po monetach
  bije dalej, a na terenach łowieckich przestaje zalegać yang, którego nikt nie
  podnosi. Boty nie mają sklepu z przedmiotami, więc przedmiot jest im nadawany,
  a jego zegar zużycia nakręcany — tak samo jak `ManagePlayerBotSkillBooks`
  kasuje osiemnastogodzinną przerwę między czytaniem tej samej księgi.

Zgłoszenie: OskarPWA.

---

## 1.30.9 — 2026-09-07

### Naprawione

- **Boty tłoczyły się w jednym korytarzu Lochu Małp i nie korzystały z reszty.**
  Labirynt nie jest jedną przestrzenią. `server_attr` dzieli go na jedenaście
  osobnych komnat — dziewięć pokoi z regenu i dwie sale bossów — a łączą je
  wyłącznie NPC-e przenoszące, które teleportują każdego, kto podejdzie na trzy
  metry (silnik sprawdza to dwa razy na sekundę, nic się nie klika). Wybór
  pokoju pytał nawigację, czy da się tam dojść po ziemi, a ta odpowiada tylko za
  komnatę, w której bot stoi — więc każdy bot znajdował jeden pokój z ośmiu w
  tablicy, ten swój, i szedł na ten sam punkt spawnu co wszyscy, którzy weszli
  przez wejście. W godzinie logów: 130 wypraw do lochu i ani jedno zaplanowane
  przejście portalem, a w raporcie gęstości ze wszystkich trzech lochów były
  wyłącznie komórki komnaty wejściowej. Trzy komnaty — w tym największa, z
  dziewiętnastoma punktami spawnu, i obie sale bossów — nie miały nawet wpisu.
  Teraz bot patroluje punkty spawnu komnaty, w której stoi, a po czterech
  minutach idzie do przejścia prowadzącego do sąsiedniej i nie wraca tym samym,
  dopóki jest inne. Tablica obejmuje wszystkie jedenaście komnat i 148
  rzeczywistych punktów spawnu z `regen.txt`.
- **Przejścia czytane z samych NPC-ów, nie z tablicy.** Trzy Lochy Małp stoją na
  jednym `server_attr` i mają NPC-e przenoszące w tych samych komórkach, ale
  **każdy loch jest inaczej okablowany**: przejście przy komórce (80,308)
  prowadzi w łatwym lochu do (106,547), a w średnim do (520,352); średni i
  trudny mają dodatkowo dwa przejścia, których łatwy nie ma w ogóle, i przez nie
  jedenastą komnatę. Cel przejścia jest teraz odczytywany z nazwy NPC-a, tak jak
  robi to silnik, więc każdy loch chodzi po swoim własnym układzie.
- **Przekroczenie przejścia rozpoznawane co takt.** Teleport przenosi bota, nie
  ruszając jego trasy: przy następnym planowaniu nawigacja uznaje stary cel za
  nieosiągalny, szuka przejścia do niego i znajduje to, którym bot właśnie
  przyszedł. Bot dropiony między agresywne małpy walczy, a nie wędruje, więc
  zanim kolejna decyzja wędrówki doszła do słowa, mijał dwa albo trzy przejścia.
  Teraz komnata jest ustalana raz na takt, zanim cokolwiek zdąży przejąć turę.
- **Boty stojące bez ruchu w mieście.** Przystanek miejski pozwalał planerowi
  przyciągnąć cel do najbliższej kratki, na której da się stanąć, nawet o
  osiemset jednostek — a potem sprawdzał dojście promieniem od 350 do 850. Gdy
  sprzedawca stoi za ladą albo filarem, bot szedł do przyciągniętej kratki,
  wciąż był poza promieniem, czyścił trasę i planował tę samą. Zaliczenie punktu
  trasy zeruje licznik zacięć, więc ratunek serwisowy — ten po sześciu
  nieudanych próbach — nigdy się nie uruchamiał: jeden bot stał u nauczyciela
  umiejętności w Joan trzy dni, budzony przez watchdoga co dziewięćdziesiąt
  sekund i ani razu nie ruszył się z miejsca. Przyciąganie celu mieści się teraz
  w promieniu dojścia, a cel naprawdę nieosiągalny trafia w istniejący ratunek.

Zgłoszenie: Tieru, Remigiusz.

---

## 1.30.8 — 2026-09-07

### Naprawione

- **Tobołki straganów leżące na ziemi.** Przed otwarciem straganu bot
  kupuje tobołek (przedmiot 50200 „Tobół”) przez `AutoGiveItem`, a ten
  przy pełnym plecaku — a plecak tuż przed sprzedażą bywa właśnie pełny —
  kładzie go na ziemi. Stragan odmawiał (brak tobołka), następna próba
  kupowała kolejny i plac zapełniał się tobołkami z etykietą „botjade2's”
  (okno ochrony dropu), a boty biegały między nimi. Ta sama wada co
  strzały w 1.30.7: teraz tobołek kupowany jest tylko do wolnej kratki,
  bez niej stragan czeka na następną wizytę (`PLAYERBOT_SHOP: no room for
  bundle` w logu).

Zgłoszenie: Renagaruu.

---

## 1.30.7 — 2026-09-07

### Nowe

- **Średni i trudny Loch Małp.** Medal Konny to grupa dropu „kill”
  (jeden na 550 żołnierzy, 500 wojowników, 200 generałów), a silnik skaluje
  każdy taki rzut różnicą poziomów: piętnaście poziomów nad potworem
  zostaje 1 % szansy. Bot 45 poziomu w łatwym lochu (małpy 22–29)
  potrzebował więc ok. 50 tysięcy zabójstw na medal — 140 wypraw na godzinę
  przynosiło jeden. Teraz loch dobiera poziom: łatwy (25) do 32, średni
  (108, małpy 35–42) 33–45, trudny (109, małpy 45–54) od 46. Trzy lochy to
  ten sam labirynt (jeden `server_attr`, te same portale), więc nawigacja,
  pokoje i wyjście działają wszędzie; mapa 109 przeniesiona na rdzeń
  game1. Bot, który przerośnie swój loch, wychodzi i wraca do właściwego.
- **Po medal także z map granicznych.** Losowanie wyprawy po medal
  czytano tylko w mieście, a bot po czterdziestce mieszka na Dolinie,
  Pustyni, Sohan i w V1 — stąd 439 botów 40+ bez konia i 435 na koniu
  poniżej dziesiątki. Teraz wylosowany bot bez drużyny schodzi z mapy
  granicznej do Bokjung, idzie do swojego lochu i wraca; drużyn nie
  rozbija. Zniesiony też podział „po 26 poziomie tylko trzecia część
  szansy” i blokada dla konia ≥ 10: koń bojowy (11–19) dalej potrzebuje
  medali do dwudziestki.
- **Medal-dropper jest sklepikarzem medalowym.** Zbiera medale w lochu
  swojego poziomu (dotąd tylko do 32) i zawsze wystawia je na straganie —
  wcześniej trzymał je, dopóki jego własny koń mógł ich użyć, a dropper na
  koniu 10 (kandydat do bojowego, nie wolno mu wydać medalu) nie mógł ich
  ani użyć, ani sprzedać.
- **Pełne eq chętniej otwiera stragan.** Bot z każdym slotem zajętym i bez
  niczego do kupienia na drabince postępu trzyma stragan w trzech
  przypadkach na dziesięć zamiast jednego (ten sam rzut, więc dotychczasowi
  handlarze zostają).

### Naprawione

- **Łucznicy wysypywali strzały na ziemię.** `AutoGiveItem` oddaje przedmiot
  także wtedy, gdy nie miał go gdzie położyć — przy pełnym plecaku paczka
  strzał lądowała na ziemi, bot płacił, wciąż „potrzebował strzał” i
  kupował znowu (dwadzieścia razy na godzinę). Teraz przed zakupem sprawdza
  wolną kratkę, a bez niej czeka na następną wizytę.
- **Panel seban (port 7790): „Internal Server Error” zaraz po starcie.**
  Pulpit czyta tabelę zrzutów, którą kolektor zakłada przy pierwszym
  zrzucie — a kolektor startuje razem z bazą, która na świeżej lub właśnie
  zaktualizowanej instalacji jeszcze nie odpowiada; po nieudanej próbie
  czekał pięć minut i przez ten czas każdy, kto otworzył panel, widział
  błąd 500. Teraz do pierwszego udanego zrzutu kolektor ponawia co 15 s,
  a pulpit bez tabeli pokazuje pustą sekcję obciążenia zamiast błędu.

Zgłoszenie: OskarPWA, ŁOŚTEK, Remigiusz, stylowy26.

---

## 1.30.6 — 2026-09-07

### Naprawione

- **Świeża instalacja z archiwum repozytorium nie budowała panelu.** Launcher
  kopiuje przy starcie `files/web_admin_schema.sql` do
  `linux-port/docker/panel/schema/`, ale gdy tego katalogu nie było (Git go
  ignoruje, więc archiwum „metin2-playerbots-main” go nie zawiera), pomijał
  kopię — i budowa panelu padała na `COPY schema/` przy każdym Starcie, po
  aktualizacji też. Launcher tworzy teraz brakujący katalog. Obejście dla
  zainstalowanych: utworzyć ręcznie folder `linux-port\docker\panel\schema`
  i kliknąć Start.

---

## 1.30.5 — 2026-09-07

### Nowe

- **Dwa nowe bossy do wypadów.** Dziewięć Ogonów na Górze Sohan (1901,
  poziom 72, 166 tys. PZ, dwa lodowe golemy i yeti u boku, odrodzenie co
  2 godziny w promieniu 150×200 pól) — wypad drużyny co najmniej
  trzyosobowej, jak na Królową. Bestialski Kapitan w Bokjung (591, poziom
  42, 19 tys. PZ, co godzinę) — każdy bot od 35 poziomu skręca do niego,
  póki stoi, jak Dolina do Wodza.
- **Drużyny na wszystkich mapach granicznych.** Drużyny tworzyły się tylko
  na Dolinie Orków, a przejście mapy rozwiązuje drużynę, więc w V1 i na
  Sohan nie było ani jednej (16 botów w V1, wszystkie solo) i nikt nie mógł
  ruszyć na Królową ani Dziewięć Ogonów. Teraz drużyna zawiązuje się od 40
  poziomu na każdej mapie granicznej (Dolina, Pustynia, Sohan, V1), do
  ośmiu osób.
- **Koń bojowy = częstsze Metiny, i ciosy w rytmie konia.** Jeździec z
  koniem bojowym losuje wyprawę na Metiny dwa razy częściej. Kombo w siodle
  ma trzy ciosy, nie cztery, i własne czasy z danych klienta
  (`horse_<broń>/combo_NN.msa`); dotąd jeździec bijący kamień używał tabeli
  pieszej, wysyłał czwarty cios, którego koń nie ma, i kolejny za wcześnie,
  więc kombo nigdy nie grało do końca.

Zgłoszenie: Tieru.

---

## 1.30.4 — 2026-09-07

### Naprawione

- **Tarcza przy broni dwuręcznej.** W 1.30.3 bot odmawiał kupna tarczy,
  gdy nosił broń dwuręczną — a w Metin2 tarcza ma własny slot niezależny od
  broni (nosi ją każda postać, także z łukiem). Reguła usunięta; zostają
  te właściwe: nie kupuj, gdy w plecaku leży co najmniej równie dobry
  zamiennik, i wymagaj 15 % przewagi nad noszonym.

Zgłoszenie: Tieru.

---

## 1.30.3 — 2026-09-07

### Naprawione

- **Boty kupowały po trzy te same zbroje i tarcze, których nie noszą.**
  Ocena „czy chcę ten przedmiot ze straganu” porównywała go tylko z tym, co
  bot ma na sobie, a założenie kupionej rzeczy czekało na przegląd
  ekwipunku — więc bot stojący przy ladzie kupował tę samą zbroję +6 trzy
  razy co dwie sekundy, każda lepsza od noszonej i żadna jeszcze nie
  założona. Teraz bot nie kupuje, gdy w plecaku leży co najmniej równie
  dobry zamiennik na ten sam slot, a rzecz ze straganu musi być o 15 %
  lepsza od noszonej i od zapasu (dwie zbroje +6 różnią się tylko rzutem
  bonusów), nie kupuje tarczy przy broni dwuręcznej, a po zakupie przegląd
  ekwipunku rusza natychmiast.
- **Pętla Bokjung ↔ M3 M3-droppera.** Dropper z M3 ma tam zostać do 32
  poziomu, ale reguła „powyżej 24 opuść M3” wyrzucała go natychmiast, a w
  Bokjung ta sama logika słała go z powrotem: 148 przejść w kwartę u jednego
  bota, kolejka do teleportera w Bokjung i boty 25+ „expiące w M2” między
  skokami. Dropper zostaje na M3, dopóki chce tam być.

Zgłoszenie: OskarPWA.

---

## 1.30.2 — 2026-09-07

### Naprawione

- **Do V1 przez pustynię, jak w grze.** Bot przenosił się z teleportera w
  Bokjung wprost do Lochu Pająków i wprost z powrotem, czego żaden gracz nie
  może zrobić. Teraz wejście to dwa etapy: teleporter → Pustynia Yongbi
  (lewy górny róg) → marsz przez całą pustynię do bramy „Kuahlo Dong” w
  prawym dolnym rogu (NPC 10016) → V1; powrót: wyjście z V1 na pustynię w
  prawym dolnym rogu → marsz do bramy Bokjung w lewym górnym rogu → miasto.
  W drodze bot nie bije mobów (marsz zajmuje tick, wybór celu nie działa), a
  jedynie Metin w zasięgu 25 metrów, jeśli jest wart jego poziomu (ta sama
  reguła dropu co przy polowaniu: kamień nie niżej niż 10 i nie wyżej niż 9
  poziomów od bota); po rozbiciu idzie dalej. Log: `PLAYERBOT_WORLD:
  transitioned ... reason=desert_crossing_to_v1 / desert_gate_to_v1 /
  desert_crossing_from_v1 / desert_gate_to_bokjung` oraz `crossing stone`.
- **Wypady na bossa naprawdę się zdarzają.** Huby Wodza Orków (Dolina) i
  Królowej Pająków (V1) były oceniane jak każdy obóz — po gęstości potworów
  z pamięci spotów — a jeden boss co pół godziny to gęstość zero, więc w
  dobie logów ani jeden bot ich nie wybrał; Królowa nie zginęła ani razu.
  Teraz hub bossa pyta sektor, czy boss stoi (odpowiedź trzymana 30 s), i
  gdy stoi, wygrywa z każdym obozem: na Wodza (25 tys. PZ, co 30 minut)
  rzuca się każdy bot z pasma 45+, także bez drużyny; na Królową (193 tys.
  PZ, poziom 60, co ok. 4 godziny) idzie tylko lider drużyny co najmniej
  trzyosobowej, a członkowie za nim. Log: `PLAYERBOT_RAID: boss ...
  standing/down` i `heading for boss`.

Zgłoszenie: Tieru.

---

## 1.30.1 — 2026-09-07

### Nowe

- **Wyprawa na Metiny.** Dotąd kamienie biła tylko rola łowcy Metinów
  (co czwarty bot), a reszta rozbijała Metin wyłącznie wtedy, gdy stanął
  jej na drodze — nie tak gra człowiek, który umawia się na wieczór
  Metinów i przeczesuje mapę. Teraz każdy pozostały bot od 15 poziomu raz
  na godzinę losuje (25 % szansy, skalowane suwakiem „Metiny” w panelu)
  półgodzinną wyprawę: przez ten czas planuje, celuje i wędruje jak łowca —
  Metin znany z rejestru (widziany przez dowolnego bota na mapie) idzie
  przed hubem, kamień jest celem ważniejszym niż moby, a na mapach polowań
  bot zmienia hub co półtorej minuty zamiast co cztery, bo kamień znajduje
  się pokrywając teren. W każdej chwili około jeden bot na osiem spoza roli
  łowcy jest na wyprawie. Log: `PLAYERBOT_METIN: expedition start/over`.
- **Łowcy Metinów poza Joan szukają znanych kamieni.** Na Dolinie Orków,
  Pustyni, Sohan i w V1 łowca szedł do hubów jak każdy inny i bił kamień
  tylko z zasięgu skanu; teraz Metin z rejestru mapy jest jego celem
  wędrówki tak samo, jak na mapie startowej.

Zgłoszenie: OskarPWA.

---

## 1.30.0 — 2026-09-07

### Nowe

- **Metin2 Singleplayer Panel autorstwa seban latino — drugi panel w
  instalacji.** Startuje razem z serwerem pod `http://127.0.0.1:7790`
  (klasyczny panel zostaje pod 7788; 7789 należy do mostu przeglądarkowego).
  Mapa świata botów odświeżana na żywo z pozycjami, poziomami, grupami i
  botami przy Metinach; profile postaci z ekwipunkiem, magazynem, Yangami i
  tooltipami przedmiotów w stylu klienta (bonusy, kamienie duszy); rankingi
  poziomu, Yangów, broni 30 lv, konia, gildii; historia gospodarki (stan
  przedmiotów, obieg Yangów, wykresy per przedmiot); telemetria CPU, RAM i
  dysku; sterowanie zachowaniem botów, mnożnikami i bezpiecznym restartem;
  masowe nadawanie przedmiotów według poziomu, klasy, konia i czasu gry;
  przybornik GM, trzy motywy, opcjonalne hasło. Przy pierwszym wejściu
  kreator `/setup`.
- **Jak to jest wpięte.** Trzy kontenery z jednego obrazu w naszym compose
  (`seban-panel`, `seban-collector`, `seban-item-grants`) na tej samej bazie,
  plikach statusu i spoolu co klasyczny panel; hasła z naszego `.env`,
  wersja z pliku `VERSION`. Nowe, opcjonalne klucze `.env`:
  `M2_SEBAN_PANEL_PORT` (7790), `M2_SEBAN_SESSION_SECRET` (pusty = sekret
  strony admina), `M2_SEBAN_TIERU_PANEL_URL`. Quest `web_admin.quest`
  rozszerzony o polecenia nadań (`BULK_ITEM`, `BULK_MISSING`, `RIDER_ITEM`).
  Paczka aktualizacji rośnie o ikony przedmiotów (ok. 20 MB); pierwszy start
  po aktualizacji buduje nowy obraz panelu i grę z nowym questem.

Źródło: seban latino, Metin2 Singleplayer Panel 1.30.1, przygotowany pod
Playerbots 1.29.10 i tu dostosowany (wersja z pliku, jeden `COPY` kontekstu).

---

## 1.29.17 — 2026-09-07

### Nowe

- **Handel na czacie.** Bot, który otwiera stragan z czymś wartym przejścia
  przez miasto (broń 30 lv, +7 i wyżej, dobry bonus), woła raz na świecie:
  „Sprzedam Kozik Czar. Liś.+0 - stragan w Joan”; bot, który przyszedł na
  targ po materiał i nic nie zastał, woła „Kupie Amulet Orka - kto ma, niech
  wystawi w Joan”. Najwyżej jedno takie wołanie na 90 s na całym świecie i
  jedno na bota na 20 minut. Wołanie gracza „Kupię X” / „Szukam X” dostaje
  szept od najbliższego straganiarza, który ma X (z ceną i miastem);
  „Sprzedam X” — szept od bota, któremu X brakuje. „KU Aura” rozumiane
  (nazwy umiejętności z protos ksiąg). Szept do bota zwraca jego stan
  (co ma na straganie, albo że poluje). Dopasowanie nazw nie zważa na
  polskie znaki ani wielkość liter.
- **Boty kupują też na straganach graczy.** Prywatny sklep gracza jest
  czytany przez boty tak jak stragan bota; linia droższa niż 30 % mediany
  portfela botów jest pomijana, żeby nikt nie drukował yangów na botach.
  Gracz może więc odpowiedzieć na „Kupie X” bota, wystawić X w Joan lub
  Bokjung i sprzedać.
- **Łatka silnika 0007** (`input_main.cpp`, `shop.h`): wołanie i szept gracza
  trafiają do botów, a lada sklepu jest czytelna. Pliki są w paczce jak przy
  0006; pierwszy start po aktualizacji kompiluje grę na nowo.

### Wydajność

- **Dalekie plany tras.** Cache tras: 16 tras na cel zamiast 6, pół godziny
  zamiast dziesięciu minut, dołączenie z 2400 zamiast 1600 jednostek;
  długi korytarz szukany zachłanniej (waga 3); najwyżej 80 dalekich planów
  na minutę, reszta odroczona o sekundę-dwie. Pomiar na 837 botach: tick
  21–31 s → 8–17 s, planowanie 17–28 s z każdych 60 → 4–14 s, dalekie
  plany 100–120/min po 221 ms → 30–82/min po 120 ms, trafienia cache
  143–180 → 196–264/min.

---

## 1.29.16 — 2026-09-07

### Nowe

- **Księgi bez dobowej przerwy (przełącznik w panelu, zakładka AI).** Gra każe
  czekać 18–30 godzin między dwoma czytaniami tej samej umiejętności, więc
  bot potrzebował miesiąca ksiąg, by przejść z M1 na G1, a księgi zalegały w
  plecaku. Domyślnie włączone: bot czyta ponownie po pół godzinie — to, co
  gracz robi Zwojami Egzorcyzmu. Wyłączenie przywraca tempo gry. Klucz
  `BOOKS` w pliku wag, jak `CHAT` i `SCRAP`.
- **Limit ksiąg własnych.** Bot trzyma najwyżej 12 ksiąg jednej swojej
  umiejętności (dziesięć udanych czytań to M1→G1 z zapasem); nadmiar idzie na
  ladę lub do handlarza. Stragan nie wystawia już księgi, na którą jej
  właściciel czeka.
- **Skrzynki bez klucza pod presją plecaka.** Srebrne i złote szkatułki bez
  pasującego klucza są trzymane, dopóki jest miejsce; gdy w plecaku zostaje
  8 lub mniej wolnych pól, idą do handlarza, żeby loot i Szkatułki Blasku
  (które otwierają się same, ale tylko do wolnego pola) miały gdzie
  wylądować.

### Naprawione

- **Bot wolał smoczą zbroję +3 od zbroi z +1500 PZ.** Kara za przerośnięty
  o 20 poziomów pancerz była płaskimi 1500 punktami za poziom i zjadała
  linie bonusów: zbroja płytowa na 18 lv z 1500 PZ przegrywała na 50 lv ze
  zbroją o 7 punktów obrony więcej. Kara jest teraz procentem samej obrony
  (5 % za poziom), a punkt maksymalnego PZ liczy się 15 zamiast 10.
- **Panel: wyszukiwarka postaci na liście graczy.** Lista brała 200
  ostatnio grających postaci, a przy 1500 botach nie mieściła w niej żadnej
  postaci gracza — filtr w przeglądarce nic nie znajdował. Ludzie są teraz
  na liście zawsze, boty dopełniają ją do 200.

Łuk: log potwierdza, że łucznicy ciągną po 3–4 cele i strzelają obszarowymi
Strzałą Ognia i Strzałą Trucizny (do 12 trafień w promieniu 300); zwykły
strzał z łuku jest jednocelowy z natury silnika — bez zmian.

---

## 1.29.15 — 2026-09-06

### Naprawione

- **Koniec fałszywych odmów zakupu.** W godzinie 22–23 było 2158 odmów na
  501 zakupów, każda z wpisem silnika „this user seems to be a hacker”.
  Przyczyna: prywatny sklep w silniku to siatka 5 kolumn × 8 wierszy, broń
  zajmuje 3 pola w kolumnie, zbroja 2, a bot numerował linie 0, 1, 2…, więc
  każdą linię od drugiego rzędu pod bronią lub zbroją silnik po cichu
  pomijał („not empty position” w syserr, 280 razy na godzinę) i kupujący
  trafiali w pusty slot. Linie są teraz układane na siatce tak jak robi to
  silnik, oferta pamięta swój slot, a do tego jest identyfikowana po id
  przedmiotu, którego właścicielem wciąż jest straganiarz (ten sam test,
  który robi silnik), bo sprzedany stos bywa zastąpiony drugim takim samym
  z plecaka. Po dojściu do stoiska bot czyta ofertę jeszcze raz i kupuje
  to, co naprawdę tam leży, albo idzie do następnej lady. Wyprzedany
  stragan zamyka się po id, nie po vnum.

### Nowe

- **Ceny skalowane do portfeli kupujących.** Cena bazowa była trzykrotnością
  ceny u handlarza, a handlarz płaci grosze: materiał, którego brakowało
  czterystu botom, stał za 600 yang na rynku, gdzie klienci mają po milion.
  Co minutę liczona jest mediana wolnego złota botów, które chodzą po
  zakupy (`wallet=` w raporcie `PLAYERBOT_MARKET: ledger`), i sztuka na
  ladzie prosi o udział w niej: materiał 1,5 %, zapasowy sprzęt +4/+5/+6
  odpowiednio 3/4,5/6 %, reszta 1 %, a cały stos najwyżej 30 % — żeby bot z
  medianą portfela mógł go kupić. Marża handlarza zostaje tam, gdzie jest
  wyższa; kamienie duszy trzymają tabelę po stopniu; +7/+8/+9 bez zmian.
- **Pasmo rynku względem ceny bazowej.** Mediana transakcji koryguje cenę w
  przedziale od ćwierci do czterokrotności ceny bazowej (dotąd: do
  dwunastokrotności ceny u handlarza), więc jedna przepłata nie wywinduje
  materiału, a stare tanie sprzedaże nie ściągną go z powrotem do groszy.

---

## 1.29.14 — 2026-09-06

### Nowe

- **Księga rynku.** Co minutę serwer liczy, ile sztuk każdego materiału
  stoi na otwartych straganach i ilu botów brakuje go do własnej receptury
  (i ma za co kupić). Dotąd stragan wystawiał każdy zapasowy materiał, więc
  czterdzieści straganów pokazywało te same rzeczy, których nikt nie
  potrzebował.
- **Limit podaży z kodami powodów.** Materiał trafia na ladę tylko wtedy,
  gdy na straganach jest go mniej niż 1,5 × 5 sztuk na każdego
  potrzebującego bota (`LIST`). Gdy nikt go nie potrzebuje, jeden stos może
  stać jako sonda (`PROBE`); kolejne zostają w plecaku (`NO_DEMAND`), a przy
  pokrytym popycie — `OVERSTOCK`. Odmowy z liczbami idą do logu raz na
  minutę (`PLAYERBOT_MARKET: held`), a co 10 minut raport
  `PLAYERBOT_MARKET: ledger` pokazuje osiem najbardziej poszukiwanych
  materiałów (D = boty, S = sztuki/stragany, ask = ostatnia cena).
- **Wycena z kotwicą i regulatorem.** Cena bazowa (3× cena u handlarza, dla
  kamieni tabela po stopniu) łączy się z medianą prawdziwych transakcji
  wagą n/(n+4) w skali logarytmicznej: dwie sprzedaże przesuwają cenę o
  jedną trzecią, pełna pamięć ośmiu o dwie trzecie. Dotąd dwie sprzedaże
  zastępowały cenę bazową w całości. Dla materiałów dochodzi regulator
  popyt/podaż ((D+5)/(S+5))^0,2 w granicach 0,75–1,35, a cena rynkowa
  przedmiotu może przesunąć się najwyżej o 5 % na 10 minut (do 30 % naraz
  po godzinie bez ofert).
- **Materiał bez receptury nie jest towarem.** Przedmiot typu MATERIAL,
  którego żadna receptura nie zużywa, nie trafia już na ladę tylko dlatego,
  że ma taki typ.

Ograniczenie: zakupy graczy na straganach botów przechodzą przez silnik bez
śladu w tej pamięci — księga i mediana widzą wyłącznie handel bot–bot.

Z dokumentu o handlu i wycenie (etapy A, B i C).

---

## 1.29.13 — 2026-09-06

### Naprawione

- **Stopień Kamienia Duszy był czytany z niewłaściwej cyfry.** Vnum kamienia
  to 28[stopień][rodzaj] (28037 Potwora+0, 28437 Potwora+4); kod brał
  ostatnią cyfrę, więc każdy kamień liczył się jako +0, reguła „+3 i +4
  tylko na sprzęt +6” nigdy nie działała, a +4 Potwora szedł w byle co.
- **Kamienie Duszy nie idą już do handlarza.** Reguła śmieci uznawała je za
  złom — kamień, którego bot nie mógł od razu osadzić, sprzedawał NPC za
  jednego yanga.

### Nowe

- **Zestawy Kamieni Duszy według stylu walki.** Jedna wspólna ocena
  rodzaju kamienia dla kowala, lady i rynku: Potwora dla każdego, potem
  Śmierci; Penetracji dla szkół bijących, Powtórki dla skillowych; kamienie
  klasowe (Wojownika, Sury, Ninja, Szamana) są warte zero w świecie
  potworów. Na zbroi: Witalności, potem Uchylenia dla tych, co stoją w
  hordzie, Przyspieszenia dla dystansowych (łucznik, BM, szaman), potem
  Obrony i Uniku; Magii tylko dla skillowych. Silnik odmawia drugiego
  kamienia tego samego rodzaju, więc zestaw układa się sam.
- **Gniazdo warte czekania.** Osadzenie to 30% szansy, a 70% to pęknięty
  kamień wspawany w gniazdo na zawsze. Na sprzęcie +6 bot osadza tylko +3 i
  +4, na +8 tylko +4; +0..+2 idą na sprzęt tymczasowy, +3/+4 nigdy poniżej
  +6.
- **Kamienie na targowisku.** Straganiarz wystawia kamienie, których nie
  osadzi (zły rodzaj, brak gniazda, zły stopień na jego sprzęt), z ceną po
  stopniu (30–500 tys.) do czasu, aż targowisko wyceni je transakcjami; bot
  z wolnym gniazdem na sprzęcie +6 idzie na targ i kupuje kamień ze swojego
  zestawu.

Z audytu wiedzy o grze (sekcja 9).

---

## 1.29.12 — 2026-09-06

### Nowe

- **Pamięć dropu materiałów per spot, z zerami.** Każdy podniesiony materiał
  (typ `ITEM_MATERIAL`) jest liczony w komórce spotu, w której leżał. Bot,
  któremu brakuje materiału do własnej receptury, wybiera hub, gdzie ten
  materiał już wypadał (warto o połowę więcej), a komórka z długim
  rejestrem walk i bez tego dropu nie dostaje premii — zera są obserwacją
  tak samo jak trafienia. Dotąd wybór szedł wyłącznie z tabel dropu.
- **Bonus rasowy według tego, co bot naprawdę bije.** Ocena ekwipunku
  (Silny przeciw Orkom, Nieumarłym…) brała rasę dominującą całej mapy.
  Teraz każdy bot prowadzi własny histogram ras z ostatnich walk (połowiony
  co 10 minut) i po 20 walkach to on decyduje; mapa jest zapasem. Pustynia
  ma skorpiony obok nieumarłych, Dolina orków obok mistyków — liczy się
  konkretny cel.
- **Wojownik mentalny rozdaje statystyki jak tank.** 2 WIT : 1 SIŁ do 90,
  potem ZR; Body bez zmian (2 SIŁ : 1 WIT). Reszta klas była już zgodna z
  audytem (sura i szaman INT:WIT, ninja ZR:WIT).

Z audytu wiedzy o grze (sekcje 3, 8 i 11).

---

## 1.29.11 — 2026-09-06

### Naprawione

- **Launcher rozpoznaje „read-only file system” Dockera.** Gdy dysk maszyny
  WSL Docker Desktop przejdzie w tryb tylko do odczytu albo się zapełni,
  budowa obrazu pada na `desktop-containerd/.../meta.db`, a launcher
  odsyłał do logów. Teraz mówi, co zrobić: zamknąć Docker Desktop,
  `wsl --shutdown`, sprawdzić miejsce w `%LOCALAPPDATA%\Docker\wsl`,
  uruchomić Dockera i GRAJ — i czego nie robić („Clean / Purge data”
  kasuje bazę z postaciami). Zgłoszenie z serwera (Ciapek).

### Nowe

- **Ryby pieczone na ognisku.** Martwa ryba szła dotąd do handlarza. Rybak
  kupuje Wysuszone Drzewo (20 000 yang, więc dopiero gdy ma co najmniej 30
  martwych ryb), na koniec sesji rozpala ognisko (silnik stawia je na 40 s)
  i podaje mu martwe ryby; pieczone
  wracają do plecaka i są używane jak mikstury: Karaś, Duży Karaś i Tenchi
  jako czerwone, Ryba Mandaryna i Sum jako niebieskie, Pieczony Karp
  (+20 ruchu) i Krasnopiórka (+10 zręczności) jak boostery na początku
  walki. Wszystko przez natywną ścieżkę silnika (podanie przedmiotu
  ognisku, `Grill`).
- **Otwarcie małża to decyzja, nie odruch.** Silnik daje z małża w połowie
  Kawałek Kamienia, w 30% nic, w 10/7/3% białą, niebieską i krwawą perłę.
  Bot otwiera małża tylko wtedy, gdy oczekiwana wartość zawartości (ceny z
  transakcji na targowisku, a bez nich cena u NPC) przebija wartość całego
  małża; ostrożny kolekcjoner chce półtora raza tyle, specjalista od sprzętu
  zadowala się mniej. Małż potrzebny własnej recepturze nadal zostaje.
  Populacja pamięta wyniki wszystkich otwarć — także puste — i po 50
  otwarciach liczy szansę z własnych danych; raport `PLAYERBOT_SHELLFISH:`
  co 10 minut. Z audytu wiedzy o grze (sekcje 7 i 12).

---

## 1.29.10 — 2026-09-06

### Naprawione

- **„Nie znaleziono programu Docker Desktop” po STOP albo po aktualizacji.**
  Launcher szukał `Docker Desktop.exe` tylko w trzech standardowych
  katalogach; kto miał Dockera na innym dysku, ten po zatrzymaniu serwera
  (które zatrzymuje też Docker Desktop) nie mógł go już uruchomić z
  launchera, a aktualizacja, która trafiła w międzyczasie, dostawała winę.
  Launcher szuka teraz też obok `docker.exe` z PATH i w rejestrze (wpis
  odinstalowania, `InstallLocation`). Do tego czasu: uruchomić Docker
  Desktop ręcznie i kliknąć GRAJ.
- **Dalekie plany w Dolinie liczone raz, nie dla każdego bota z osobna.**
  Daleki plan trasy (ponad 12 km) kosztował 150–250 ms rdzenia, a w Dolinie
  szło ich 60–120 na minutę — 10–25 s z każdej minuty — i prawie wszystkie
  były tą samą drogą: wejście na mapę do huba, hub do wyjścia, hub do huba,
  proszone przez kolejne boty z tego samego kilkuset metrów. Rdzeń trzyma
  teraz policzone trasy (start i cel zaokrąglone do 1,2 km, dołączenie tylko
  przy czystym pierwszym odcinku, ważność 10 minut, do 400 tras na mapę) i
  bot idący tam, gdzie ktoś już szedł, dostaje gotową trasę bez liczenia.
  Linia `PLAYERBOT_LOAD` ma `cached=`, a każdy daleki plan ma wpis z celem,
  wynikiem i kosztem. Pomiar przy 843 botach: 107–176 gotowych tras
  na minutę, dalekie plany ze 100–170 na 38–93, rdzeń z 45–56% na 35–40%.

- **Boty po 40 nosiły Bojową Tarczę+6 z pierwszego poziomu.** Silnik liczy
  obronę jako `value1 + 2·value5`, więc Bojowa Tarcza+6 (39) wygrywała z
  Czarną Okrągłą Tarczą+3 (37) i bot nosił ją „uczciwie” — 61 botów po
  czterdziestce miało tarczę z 1 poziomu na grzbiecie, a 41-tarczę w
  plecaku. Element zbroi przerośnięty o ponad 20 poziomów traci punkty za
  każdy dalszy poziom, więc bot zakłada tarczę, zbroję, hełm i buty z
  właściwego progu i dopiero je ulepsza (cel ulepszania to nadal +6 i wyżej);
  41-tarcza od +4 wygrywa z 21-tarczą+6. Zgłoszenie z serwera.

### Nowe

- **Suwaki Szkatułek Księżycowych w panelu.** Zakładka zachowania botów ma
  dwa suwaki: szansa na szkatułkę z zabitego potwora (‰) i z rozbitego
  Metina (‰). Rdzeń przepisuje je do silnika w pięć sekund, bez restartu i
  bez edycji `.env`; dopóki nikt ich nie zapisze, obowiązuje `.env`.
- **Szyld straganu mówi, co jest na ladzie.** Dotąd szyld to była nazwa
  konta bota, więc targowisko z czterdziestoma straganami czytało się jak
  ściana identycznych napisów. Szyld powstaje z zawartości lady: „Bron 30:
  Miecz Zabojcy”, „Zbroja Smoka+7”, „Futro Wilka, Skora Niedzwiedzia”,
  „Ksiegi: Aura Miecza i inne”, „Zlom do palenia +0..+3” albo „Zolc
  Niedzwiedzia i inne”; krótki przedrostek zależny od bota (Tanio, Okazja,
  Sprzedam) tylko, gdy mieści się w 32 znakach. Szyld jest też w logu
  otwarcia straganu. Pomysł z Discorda (Renagaruu).

---

## 1.29.9 — 2026-09-06

### Nowe

- **Boty złomiarze (suwak w panelu, domyślnie wyłączone).** Zakładka
  zachowania botów ma suwak „Boty złomiarze” 0–100%: taki udział
  straganiarzy wystawia na ladę swoje słabe ulepszenia (+0 do +3) za
  dwukrotność ceny NPC zamiast sprzedawać je handlarzowi — złom do palenia
  u kowala, jak na serwerach hard. Złomiarz trzyma złom, dopóki ma ponad
  20 wolnych komórek w plecaku. Pomysł z Discorda (Remigiusz).
- **Łucznik ciągnie 3–4 cele naraz.** Multi-pull, dotąd tylko dla
  tarczowników i wojowników mentalnych, działa też dla łucznika: jedna
  grupa, limit czterech atakujących zamiast czternastu. Pomysł z Discorda
  (Archded).
- **Wypad po drop z Metina.** Przez 20 sekund od rozbicia kamienia bot idzie
  po swoje przedmioty w promieniu 15 m mimo trwającej walki z przywołaną
  hordą — jak gracz, który skacze po drop, zanim zabiorą go inni. Dotąd w
  walce podnosił tylko to, co leżało w zasięgu ręki (300 jednostek, tyle
  pozwala silnik). Pomysł z Discorda (Kordyl13).
- **Wypady na bossów.** Wódz Orków (Dolina, poziom 50, co 30 minut) i
  Królowa Pająków (koniec Lochu V1, poziom 60, co ~4 godziny) są hubami dla
  drużyn: przywódca z drużyną (najpierw współgildianie) może je wybrać jak
  obozy Czarnych Orków. Szkatułki bossów (Szkatułka Wodza Orków, Królowej
  Pająków i pozostałe) boty otwierają tak jak Szkatułkę Księżycową i nie
  sprzedają ich.

---

## 1.29.8 — 2026-09-06

### Nowe

- **Srebrne i złote skrzynie otwierane kluczem.** Bot, który ma skrzynię
  skarbów i pasujący klucz, używa klucza na skrzyni tak jak gracz: silnik
  zabiera oboje i wydaje zawartość. Skrzynie i klucze nie idą do handlarza.
- **Siedemnaście punktów i Zwój Zapomnienia.** Silnik losuje Mistrza przy
  każdym punkcie od siedemnastego; bot wbijał do dwudziestu i tracił punkty.
  Teraz zatrzymuje się na 17, a gdy Mistrz nie wszedł, szuka na targowisku
  Zwoju Zapomnienia (70037, wypada z potworów): zwój cofa umiejętność o jeden
  i oddaje punkt, bot wbija siedemnasty ponownie i losuje jeszcze raz. Bot
  ze zwojem, któremu nie jest potrzebny, wystawia go na ladę; zwój nie jest
  śmieciem.
- **Boty od 48 idą tylko do Lochu Pająków i na Sohan**, po połowie. Dolina
  zostaje dla 36–47, więc na obu wysokich mapach widać kogoś, mimo że
  populacja sięga dopiero 50 poziomu.
- **Przełącznik w panelu: czy boty piszą nad głową, co robią.** Zakładka
  zachowania botów ma pole „Boty piszą nad głową, co robią”; wyłączone
  ucisza napisy nad głowami (poluje, idzie do kowala, łowi) dla graczy,
  którym to przeszkadza. Rdzeń odczytuje to w pięć sekund, bez restartu.
  Wołanie na czacie świata o ulepszeniu na +7/+8/+9 zostaje niezależnie.

### Naprawione

- **Panel: Biolog liczył 6 misji, boty mają 7.** Ząb Orka dodany do listy
  panelu (ranking, karta bota, etap), licznik 0/7.

---

## 1.29.7 — 2026-09-06

### Naprawione

- **„Góra Sohan” botów była drugą wioską Jinno.** Od 1.29.0 boty 26–39
  szły na mapę 43 (`metin2_map_c3`, żołnierze 26–36) pod nazwą Sohan.
  Prawdziwa Góra Sohan to mapa 61 (`map_n_snowm_01`): Zarażeni 49–58 na
  południu, lodowe stwory i Yeti 62–66 na północy. Boty od 48 dzielą się
  teraz na trzy: Loch Pająków, Sohan i Dolinę; 30–35 na pół między wyspy
  Fanatyków i pustynię; poniżej 30 zostają w Bokjung. Czternaście hubów
  Sohan z regen tej mapy (osiem wśród Zarażonych od 48, sześć w lodzie od
  58), wejście ze spawnu miasta. Misje polowań na Zarażonych (wiersze od
  41) są wykonalne, bo ich potwory mają teraz mapę. Mapa 43 wraca na rdzeń
  Jinno, a boty, które na niej stały, migrator odsyła do Bokjung przy
  pierwszym starcie po aktualizacji. Panel ma granice i kafelek mapy 61;
  lista map, na których bot może stać, zna też 61 i 104 (dotąd boty z Lochu
  Pająków wracały do Bokjung po każdym restarcie).
- **Migrator nie ściąga już botów na środkową wyspę Doliny.** Krok z czasów,
  gdy nawigacja nie znała mostów, przy każdym starcie przenosił każdego bota
  spoza środkowej wyspy z powrotem na nią — 207 botów z wysp Fanatyków i
  obozów Czarnych Orków na każdy restart. Usunięty.

---

## 1.29.6 — 2026-09-06

### Naprawione

- **Panel odpowiadał „Access denied” do bazy, choć gra działała.** Panel
  czyta hasło bazy ze swojego pliku `m2panel.conf`, zapisywanego raz przy
  pierwszym uruchomieniu i trzymanego w wolumenie `panel-conf`. Instalacja
  przejęta z poprzedniej wersji miała tam stare hasło, a `.env` (i gra)
  nowe — pulpit panelu i statystyki padały na 1045, migrator i rdzeń
  działały normalnie. Przy starcie panelu adres, użytkownik i hasło bazy są
  teraz odświeżane z `.env`; hasło panelu, sól i sekret sesji zostają bez
  zmian. Plik startowy panelu jedzie w paczce aktualizacji, a aktualizacja
  przebudowuje obraz panelu.

---

## 1.29.5 — 2026-09-06

### Naprawione

- **Przejęcie starszej instalacji padało na „Cannot bind argument to parameter
  'Value' because it is an empty string”.** Gdy launcher znajdował istniejący
  serwer z poprzedniej wersji (kontenery `metin2-db` bez pliku tożsamości
  `.m2install.json`), przepisywał jego `.env` do nowego — a każdy prawdziwy
  `.env` ma puste wartości (`M2_BRAND=`, `M2_CLIENT_URL=`…), których funkcja
  zapisu nie przyjmowała. GRAJ i aktualizacja kończyły się tym błędem w
  kółko. Puste wartości są pomijane (nie nadpisują niczego), hasła i adresy
  ze starego pliku przechodzą. Test przejęcia starej instalacji ma teraz
  puste wartości w `.env`.

---

## 1.29.4 — 2026-09-06

### Naprawione

- **Wojownik mentalny brał iglicę +4 zamiast miecza +6 z 30% na potwory.**
  Premia +200000 punktów za broń dwuręczną biła każdą różnicę w ulepszeniu i
  bonusach. Premia działa teraz dopiero od 11 poziomu konia (koń bojowy) —
  wtedy dwuręczna ma sens, bo bot bije nią Metiny z siodła. Zgłoszenie od
  gracza z Discorda.
- **Zaparkowana trasa faktycznie wznawiana.** W 1.29.3 każdy krok do potwora
  w trakcie walki (inny cel niż hub) kasował zaparkowaną trasę, więc wznowień
  było 5–18 na minutę przy ~100 dalekich planach. Inny cel już jej nie
  kasuje, robi to tylko zmiana mapy. Po poprawce: 200–360 wznowień na
  minutę, dalekie plany spadły ze 100–170 do 46–94 na minutę, rdzeń z 56% na
  35% przy 843 botach.
- **Aktualizacja na świeżym silniku Dockera zgłaszała błąd, choć obrazy się
  zbudowały.** `up --build` ścigał się z własnym pobieraniem i padał na
  „No such image: mariadb:10.11”; druga próba przechodziła. Launcher pobiera
  teraz obrazy obce przed budową.

---

## 1.29.3 — 2026-09-06

### Naprawione

- **Świeża instalacja na Linuksie (`installer/install.sh`) padała na patchu
  szkatułek.** Hunk patcha 0006 zakładał, że w `item_manager.cpp` jest już
  `#include "high_risk.h"`, a tę linię wstawia dopiero krok High Risk, który
  w `prepare-context.sh` idzie po overlayu botów. Na czystym drzewie r40250
  `patch --fuzz=0` odrzucał hunk; na maszynie, gdzie w cache było już drzewo
  z High Risk, nakładał się przypadkiem. Kontekst hunku nie zależy już od tej
  linii. Zgłoszenie z dokładną diagnozą przyszło od gracza.
- **Paczka rozpakowana ręcznie (bez instalatora) nie miała pliku `.env` i nic
  nie działało.** GRAJ kończył się na „Missing Docker environment file”, a po
  aktualizacji dokańczanie budowy uruchamiało compose bez haseł bazy
  (`M2_DB_PASSWORD is missing a value`) — i tak w kółko, bo budowa szła
  przed krokiem, który plik tworzy. `start-server.ps1` tworzy teraz `.env`
  z `.env.example`, nowymi hasłami i adresami 127.0.0.1, wypisując hasło
  panelu; dokańczanie budowy najpierw przygotowuje `.env`. Gdy jest już
  baza z tej instalacji, a `.env` zniknął, launcher mówi wprost, że hasła
  trzeba przywrócić z kopii, zamiast wymyślać nowe, których baza nie przyjmie.
- **Zbieranie logów padało bez pliku `.env`** („WriteAllLines: wartość nie
  może być zerowa”) — akurat u gracza, któremu logi były najbardziej
  potrzebne. Pusta lista zmiennych zapisuje się jako „(brak pliku .env)”.
- **Przerwany marsz jest parkowany, nie kasowany.** Bot idący przez Dolinę do
  odległego huba zatrzymywał się na każdą walkę, trasa szła do kosza, a po
  walce rdzeń liczył ją od zera (200 ms za każdym razem, 100–170 dalekich
  planów na minutę). Trasa przerwana walką jest teraz zachowywana i
  podejmowana od najbliższego punktu, na który bot może wejść prosto.
  W pierwszej obserwacji wznowień jest jeszcze niewiele (7–18 na minutę);
  linia `PLAYERBOT_LOAD` mówi `resumed=`, a każdy daleki plan ma wpis
  `PLAYERBOT_NAV: far plan` z celem — dalsze strojenie w następnym wydaniu.

### Nowe

- **Misje polowań do 55 poziomu.** Tabela kończyła się na 25, a w praktyce
  prawie każdy bot stał od tygodni na misji z 14–17 poziomu: przyjął ją w
  Joan, wyrósł z jej potwora i nigdy po niego nie wrócił. Tabela sięga teraz
  55 (ostatni wiersz wykonalny na hostowanych mapach). Bot wybiera tę z dwóch
  opcji, której potwór stoi na jego mapie, a gdy żadna — tę, która stoi
  gdziekolwiek. Misja o ponad dziesięć poziomów niższa od bota, misja bez
  potwora na żadnej mapie, misja z potworem na innej mapie niż ta, na której
  bot osiadł, i misja wisząca dwie godziny są oddawane jako pominięte, bez
  nagrody, żeby nie blokowały następnych. Nagroda z
  doświadczenia zwęża się jak w questlib: 2–5% od 31, 1–4% od 51. Panel zna
  nazwy potworów nowych wierszy.
- **Ząb Orka u Biologa.** Siódma misja Biologa: dziesięć Zębów Orka z Orków w
  Dolinie, oddawane po jednym, 60% szansy na przyjęcie i spalony ząb przy
  porażce — jak u gracza bez eliksiru, tylko bez 22 godzin czekania między
  oddaniami. Po dziesiątym zębie quest czeka na Kamień Duszy Jinunggyi
  (1/500 z Elitarnych Orków, przez własny hook questu), bot poluje wtedy na
  Elitarne Orki, a Kamień oddaje Biologowi: +10 szybkości ruchu na 60 lat i
  skrzynka, jak w skrypcie. Ząb i Kamień nie idą do handlarza ani na ladę.
- **Misję Biologa bot bierze tam, gdzie stoi.** Dotąd przyjmował ją tylko na
  M1, a okazy spadają dopiero po przyjęciu — bot, który minął 25 poziom poza
  Joan, nie zbierał nic (663 z 876 botów miało Grzyb Tue nietknięty). Teraz
  przyjmuje ją gdziekolwiek, a do Biologa idzie dopiero z kompletem. Z
  zaległych misji wybiera tę, której okazy już niesie, potem tę, której potwór
  stoi na jego mapie — bot z Doliny zbiera zęby zamiast wracać po grzyby.

---

## 1.29.2 — 2026-09-06

### Naprawione

- **Aktualizacja do 1.29.1 padała przy budowaniu obrazu.** Nowy plik obrazu
  kopiuje tabelę szkatułki z kontekstu, a aktualizacja buduje obraz od razu
  po wgraniu plików, zanim `start-server.ps1` zdąży ją tam podstawić:
  „special_item_group.moonlight.txt: not found”. Paczka wkłada teraz ten plik
  (i tabelę dropu M3) prosto do kontekstu, a launcher podstawia je także przy
  aktualizacji. Kto już ma ten błąd: kliknięcie GRAJ dokańcza budowanie.

---

## 1.29.1 — 2026-09-06

### Naprawione

- **Szkatułki i trzy księgi z Metina nie docierały do graczy.** Zmiana w
  silniku z 1.29.0 była patchem, a na Windows launcher nie nakłada patchy —
  robi to tylko skrypt, który działa u autora. Do tego plik obrazu z podmianą
  zawartości szkatułki nie jechał w paczce. Efekt: po aktualizacji 30 Metinów
  nie dało ani jednej szkatułki i jedną księgę. Paczka wiezie teraz oba
  spatchowane pliki silnika tak samo jak źródła botów, plik obrazu i tabelę
  szkatułki; obraz zbudowany ścieżką gracza zawiera i tokeny, i tabelę.
- **Kowal ze Zwojem Błogosławieństwa.** Bez zwoju porażka ulepszania niszczy
  przedmiot — 1584 spalonych w jedno popołudnie. Ze zwojem z szkatułki bot
  ulepsza od +6 wzwyż ścieżką silnika ze zwojem: porażka to spadek o poziom,
  nie strata. Robi to z plecaka, w polu, bez kowala — jak gracz. W pierwsze
  trzy minuty: 10 sukcesów, 13 spadków o poziom, zero spalonych.

### Nowe

- **Góra Sohan i Loch Pająków V1 dla botów.** Sohan (Żołnierze Czarnego
  Wiatru i Dzicy, 26–36, trzy rodzaje Metinów) przyjmuje boty od 26: połowa
  26–29 idzie tam zamiast czekać w Bokjung, a 30–35 dzieli się na trzy
  między wyspy Fanatyków, pustynię i Sohan. Loch Pająków V1 (pająki 50–58)
  bierze połowę botów od 48 zamiast Doliny. Dziesięć hubów Sohan i osiem
  Lochu ze spawnów, wejścia z Town.txt, wyjścia przy NPC obok wejścia. Obie
  mapy przeniesione na rdzeń, na którym działają boty. Panel zna obie mapy i
  ma ich kafelki.

---

## 1.29.0 — 2026-09-06

### Nowe

- **Dolina Orków po poziomach, obozy Czarnych Orków dla grup po osiem.** Pięć
  wysp Fanatyków (35) i Arahanów (38) dla botów 30–39, szesnaście hubów
  gęstości dla 36+, trzy obozy Czarnych Orków (46) — (601,625), (774,923),
  (933,639) — dla grupy 40+, a wyspa środkowa z Dręczycielami (49), którzy
  noszą Księgę Klątw, dla grupy 45+. Boty 30–35 idą na wyspy Fanatyków albo
  na pustynię, po parzystości pidu. Grupa w Dolinie ma do ośmiu osób; każdy
  bot poziomu obozu może do niej wejść, nie tylko dziesięcioprocentowa
  kohorta; współgildianin liczy się w doborze za dwóch przyjaciół, a lider
  z gildią wybiera obóz po numerze gildii, więc jedna gildia zbiera się na
  jednym obozie. Marudera za liderem w drodze do nowego obozu grupa nie
  wyrzuca — 57 z 59 rozpadów pierwszej godziny to była właśnie ta droga.
- **Wspólna pamięć populacji: gdzie stoi najwięcej potworów.** Każde szukanie
  celu zapisuje w komórce 6400 jednostek, ile potworów było w zasięgu, jakiego
  poziomu i ile walk tam zaczęto; wpisy maleją o połowę co dziesięć minut.
  Hub wybiera się po udziale (potwory w zasięgu podzielone przez boty, które
  już tam są), o połowę taniej na 20 km, i trzyma cztery minuty. Obóz pełen
  potworów, których bot nie tknie sam, jest dla niego pusty. Raz na dziesięć
  minut `PLAYERBOT_SPOT:` wypisuje najbogatsze komórki każdej mapy — komórka
  bez huba, która wciąż wychodzi na wierzch, to hub, którego brakuje tabeli.
- **Szkatułka Blasku Księżyca jako event.** Szansa na tysiąc na zabity potwór
  (`M2_MOONLIGHT_CHEST_PERMILLE`, domyślnie 10) i osobno na kamień Metin
  (`M2_MOONLIGHT_CHEST_STONE_PERMILLE`, domyślnie 300), z CONFIG, bez
  przebudowy; 0 wyłącza. W środku, po wadze: Zaczarowanie i Wzmocnienie
  Przedmiotu, Zielona i Fioletowa Mikstura po pięć, Dłoń Krytyka i Dłoń
  Przebicia po trzy, Błogosławieństwo Życia i Smoka, Zwój Błogosławieństwa,
  Księga Umiejętności. Bot otwiera szkatułkę w ciągu sekund, zwoje bonusów
  idą przez istniejący dobór bonusów (własny zwój przed kupnem), mikstury
  szybkości przez istniejące picie, wzmocnienia pije na początku walki, a
  Błogosławieństwa są ostatnią miksturą w zapasie. W pierwsze pół godziny:
  315 otwartych szkatułek, 876 wypitych wzmocnień.
- **Trzy Księgi Umiejętności z każdego Metina.** Tabela dawała jedną z szansą
  od ćwierci do całości; teraz liczba ksiąg z kamienia jest dopełniana do
  trzech, każda z losową umiejętnością tak jak dotąd.
- **Cztery osobowości „Dropek”.** Dropek Metinów (co trzeci łowca Metinów)
  zbiera Księgi Umiejętności — także te, których nie przeczyta — i stawia je
  na straganie obok broni 30 lv. Dropek z M3 siedzi na Waryong po bronie 30
  lv do 32 poziomu, niezależnie od tego, czy ma własną. Dropek z M2 obozuje
  u Bestii w Bokjung po ich bronie do 40 poziomu. Dropek medali chodzi do
  Łatwego Lochu Małp po medale do 32 poziomu, nie tylko póki jego koń ich
  potrzebuje, i sprzedaje nadwyżkę. Co ósmy zwykły poszukiwacz jest jednym
  z trzech ostatnich. Dropek otwiera stragan na co trzeciej wizycie w mieście
  i dostaje stół handlarza. Panel zna ich nazwy.
- **Panel po polsku.** 305 wpisów słownika interfejsu miało tylko angielski,
  niemiecki i turecki, więc polska przeglądarka dostawała angielski; strona
  konta miała teksty wpisane na sztywno. Wszystko przetłumaczone.

### Naprawione

- **Launcher rozpoznaje port zarezerwowany przez Windows.** Po restarcie
  Hyper-V/WSL rezerwuje losowe zakresy portów; gdy 11000 albo 13000 w nie
  trafi, `docker compose` pada sekundę po zbudowaniu obrazów z „ports are
  not available … zabroniony przez uprawnienia”, a launcher pokazywał to jako
  nieznany błąd. Jeden gracz przeszedł tak pięć identycznych prób
  aktualizacji. Teraz diagnostyka sprawdza `netsh interface ipv4 show
  excludedportrange` przed startem, a po błędzie launcher wypisuje, co zrobić:
  `net stop winnat`, GRAJ, `net start winnat`.
- **Wybór huba nie goni najlepszego spotu przez całą mapę.** Pierwsza wersja
  pamięci spotów przełączała hub przy każdej decyzji: 160–334 dalekich planów
  na minutę, osiem tysięcy odroczeń, tick 57 s z 60. Waga odległości i
  przyklejenie do wyboru na cztery minuty sprowadziły to do 33–65 planów i
  ticku 10–18 s.

---

## 1.28.0 — 2026-09-06

### Nowe

- **Boty logują się stopniowo przez minutę po starcie serwera.** Do tej pory
  cała populacja wchodziła do świata w jednej sekundzie: 848 postaci, każda w
  pierwszym ticku prosiła o trasę, a budżet nawigacji to 32 plany na tick.
  Kto nie dostał trasy, stał; strażnik bezczynności go resetował; reset prosił
  znowu — 4075 resetów w pierwsze dziewięć minut i rdzeń na 99,9%. Teraz
  logowanie idzie partiami po 15 na sekundę, a pierwsze ciężkie przebiegi bota
  (ulepszanie, dobór sprzętu, zakupy, kamienie) są rozłożone po pidzie na tę
  samą minutę. Populacja 850 botów jest w świecie po 66 sekundach.
- **Raz na minutę serwer pisze, ile kosztowała populacja.** Linia
  `PLAYERBOT_LOAD:` w syslogu: czas ticku, liczba i czas planów tras w czterech
  koszykach odległości, odroczenia, szukanie celu, migawka panelu, skany mapy,
  zapisy, resety strażnika. Bez tej linii pierwsza wersja wędrówki po materiał
  została zdiagnozowana z samego CPU — i wszystkie skany populacji wpadły w
  jedną sekundę.

### Naprawione

- **Zużycie CPU przy 850 botach spadło z 63–68% do 26% rdzenia.** Planer tras
  pytał silnik o atrybut sektora na żywo dla każdego sąsiada każdego węzła A* —
  do 24 razy na węzeł — choć siatka nawigacji ma ten sam bit spróbkowany w tym
  samym punkcie. Tysiąc planów na minutę spędzało na tym 31 s z każdych 60.
  Teraz planer czyta siatkę; tylko chód (segment przed botem, raz na tick)
  pyta silnik, żeby wciąż zatrzymać bota przed czymś postawionym po zbudowaniu
  siatki. Plan przez całą Dolinę Orków: 513 ms → 50–80 ms; plan na 256–1024
  komórek: 35 ms → 5–10 ms.
- **Jeden tick nie zatrzymuje już świata na pół sekundy.** Budżet planów na
  tick liczył sztuki, a plan może kosztować 0,2 ms albo 80 ms. Obok liczby
  jest teraz czas: po 50 ms planowania reszta próśb w tym ticku idzie
  ścieżką odroczenia i wraca w ciągu dwóch sekund. Wiąże tylko w minucie po
  starcie.
- **Zapis postaci bota co 120 s zamiast co 30 s.** Silnik i tak zapisuje każdą
  postać co 120 s, a awans poziomu wymusza zapis natychmiast; boty dokładały
  cztery zbędne zapisy na jeden zapis silnika — blisko trzydzieści na sekundę.
- **Launcher mówi, czego brakuje, zamiast „exit 1".** Kontener
  `playerbot-migrate` kończy się w pierwszej sekundzie, gdy brak lub pusty jest
  plik `playerbots_seed.sql`; launcher kopiował go tylko wtedy, gdy źródło
  istniało, i milczał, gdy nie. Teraz brak źródła daje ostrzeżenie z pełną
  ścieżką, brak pliku w paczce zatrzymuje start przed `docker compose` z
  instrukcją co zrobić, a gdy `compose up` się nie uda, w logu launchera lądują
  ostatnie 40 linii z `playerbot-migrate`, `mariadb`, `game` i `panel`.

---

## 1.27.0 — 2026-09-06

### Nowe

- **Bot, któremu brakuje ulepszacza, idzie po niego do potwora, który go nosi.**
  Do tej pory brak materiału kończył się na targu — a jeśli lada była pusta,
  bot polował gdziekolwiek. Teraz przy wyborze celu potwór, którego przedmiot
  z tabeli dropu jest na liście braków, wygrywa z sąsiadami; a gdy w zasięgu
  nie ma żadnego, bot przeszukuje całą mapę i rusza do najbliższego. W pierwsze
  24 minuty 385 botów wyruszyło 1845 razy, z czego 500 po Amulet Orka.
- **Cena na straganie bierze się z zakończonych sprzedaży, nie ze stałego
  mnożnika.** Świat pamięta ostatnie ceny jednostkowe każdego przedmiotu.
  Przy dwóch sprzedażach lada wystawia medianę: 15% drożej, gdy ostatnia
  sprzedaż była w ciągu dziesięciu minut, 15% taniej, gdy nikt nie kupił od
  godziny — w klamrze między ceną u kupca a jej dwunastokrotnością. Cena
  wystawiona to nadzieja sprzedawcy; cena zapłacona to jedyny pomiar popytu.
- **Bot porównuje broń po tym, jak jego szkoła zadaje obrażenia.** Miecz sury
  Czarnej Magii i dzwon szamana mają w proto dwie pary wartości — fizyczną i
  magiczną — a wycena czytała tylko fizyczną. Szkoły czarujące (Czarna Magia,
  obie szkoły szamana) liczą teraz magię z połową fizyki; reszta fizykę z
  ćwiartką magii. Tak samo bonusy: średnie obrażenia są warte więcej szkołom
  ciosów, obrażenia umiejętności — szkołom skilli; wcześniej te drugie wpadały
  do domyślnego koszyka wartego dziesięć razy mniej.
- **Rybia Ość, Małż, perły i Zwój Błogosławieństwa są ulepszaczami.** Ośmiu z
  84 materiałów receptur nie ma typu „materiał"; Rybia Ość (13 receptur) szła
  do kupca jako „kieszonkowe wędkarza". Teraz o tym, co jest ulepszaczem,
  decyduje tabela receptur, nie typ przedmiotu.
- **Małż potrzebny do receptury nie jest otwierany.** 26 receptur zużywa go
  jako jest; otwieranie tego, po który bot i tak idzie do kowala, zamieniało
  pewny materiał na szansę na inny.
- **Kamień Duszy dobierany pod szkołę.** Kamień Potwora pierwszy dla każdego,
  Śmierci i Penetracji dla szkół ciosów, Powtórki dla szkół skilli, klasowe
  na końcu — w świecie potworów. Zbroja: Witalności, potem Uchylenia, Obrony.

### Naprawione

- **Bot wkładał Kamień Duszy z gwarancją sukcesu i za darmo.** Kamień był
  wpisywany wprost do gniazda i kasowany; gracz ma 30% szans, a przy porażce
  pęknięty kamień na stałe w gnieździe. Na żywym świecie stało 261 obsadzonych
  gniazd i zero pękniętych. Teraz kamień idzie tą samą ścieżką co u gracza —
  zdjęcie sprzętu, próba, odczyt gniazda, założenie. Pierwszego wieczoru:
  sześć pęknięć, jeden sukces.
- **Kamień Potwora był po stronie zbroi.** Podział rodzajów kamieni na
  broń/zbroję miał granicę o jeden za nisko; proto mówi jasno, że 28037 to
  broń. Poprzedni sposób wkładania nie zauważał różnicy.
- **Bot mógł przekuć broń w stopień, którego nie założy.** Silnik sprawdza
  poziom wyniku ulepszania tylko w wersji koreańskiej; ten serwer ma locale,
  więc kontrola była wyłączona. Pięć rodzin w zasięgu botów podnosi wymagany
  poziom z każdym plusem — Upiorna Kusza z 38 na 56, trzy dzwony z 52 na 60.
  Bot pyta teraz o poziom wyniku przed próbą.
- **Skan mapy po materiał spadał całym stadem naraz.** Po restarcie każdy bot
  robił go w tej samej sekundzie, i znowu co 90 s — rdzeń szedł na 100%.
  Pierwszy skan jest rozłożony po pidzie, a na jeden tick przypada ich
  najwyżej sześć. W stanie ustalonym rdzeń pracuje na 14%.

### Sprostowanie

Wcześniejsza analiza tego projektu twierdziła, że na hostowanych mapach da
się zdobyć 28 z 84 materiałów. To wynik błędnego odczytu `group_group.txt`.
Poprawnie: **64 z 84**, plus Rybia Ość z wędkowania. Skóra Niedźwiedzia,
której czekały 943 sztuki sprzętu na +6, wypada z niedźwiedzi na mapach 21
i 24 — nie brakowało dropu, brakowało kogoś, kto go zbierze.

---

## 1.26.0 — 2026-09-05

### Nowe

- **Boty wchodzą do Doliny Orków bramą Chunjo i rozchodzą się po wyspach.**
  Mapa ma cztery teleportery — po jednym na królestwo i jeden na środku,
  niczyj. Boty lądowały przy tym środkowym, więc cała populacja pojawiała się
  na wyspie centralnej. Teraz wychodzą tam, gdzie wychodzi postać z Chunjo.
- **Bot, który przez pięć minut nie ruszył się dalej niż o szerokość huba,
  idzie do następnego** — nawet jeśli akurat ma tu co bić. Rotacja po miejscach
  polowania działała wyłącznie w turze, w której nie było żadnego celu, a przy
  4041 punktach odrodzenia w Dolinie taka tura nie przychodziła nigdy. Razem
  z bramą Chunjo dało to jedną wyspę z dwudziestu trzech — a Amulet Orka
  i Ezoteryczny Przewodnik nie mają tam ani jednego punktu odrodzenia.
- **Ulepszacze krążą między botami.** Materiał, którego znalazca sam nie
  potrzebował, był złomem i szedł do kupca ogólnego przy najbliższej wizycie
  w mieście — niszczony w tempie, w jakim wypadał. Teraz zostaje jako towar na
  własny stragan, do ośmiu komórek (materiały się stackują). Na ladę trafiają
  wyłącznie te 84 materiały, których faktycznie żąda jakaś receptura, a nie
  wszystkie 240 przedmiotów typu materiał.
- **Boty nie zsiadają z konia przy teleporterze.** Zsiadanie ma sens przed
  rozmową z NPC; z teleporterem bot nie rozmawia, bo przejście jest serwerowe.
  W jeden wieczór było to 1362 zsiadania, każde z ponownym wsiadaniem trzy
  sekundy później po drugiej stronie.
- **Bot porównuje broń po obrażeniach w czasie, a nie po rodzinie.** Broń
  z zestawu na 30 poziom dostawała płaski bonus tak duży, że żaden bot nigdy
  jej nie wymieniał — FMS +4 wygrywał z mieczem na 36 poziom +7, który jest
  lepszy. Do tego silnik skraca o połowę odstęp między ciosami sztyletu
  i podwaja obrażenia łuku, więc na tych dwóch te same liczby są warte dwa
  razy tyle: sztylet na 30 poziom z 40-44 bije mocniej niż miecz z 57-73.
- **Mapy w panelu są dwa razy dokładniejsze.** Kafle 1024 zamiast 512, więc
  na mapie królestwa piksel to dwie komórki terenu, a nie cztery — mury,
  budynki i drogi przestały się rozmazywać. Jasne, piaskowe podłoże zamiast
  ciemnozielonego, bo nakładki cieplne są ciepłe i na ciemnym tle z nim
  konkurowały. Siatka mapy cieplnej z 44 na 72, więc ognisko wypada na
  budynku, a nie na dzielnicy. Doszła trzecia warstwa: **awanse umiejętności**.

### Naprawione

- **Boty potrafiły zamarznąć przy teleporterze na dobre.** Marsz do portalu
  ignorował to, czy udało się wyznaczyć trasę, i zawsze meldował sukces —
  a podróż zajmuje całą turę, więc nic poniżej już się nie wykonywało. Bot stał
  bez trasy, przeskakując cel co sześć sekund, a strażnik bezczynności resetował
  go w kółko prosto z powrotem w tę samą decyzję. Teraz mierzony jest postęp:
  bez ruchu przez dwadzieścia sekund bot oddaje turę i wraca do polowania.
- **Bot i potwór potrafili leczyć się nawzajem w nieskończoność.** Reguła
  „ta walka do niczego nie prowadzi" istniała tylko dla kamieni metin. Zwykły
  potwór był porzucany wyłącznie wtedy, gdy bot nie potrafił do niego dojść,
  więc walka z Czarnym Orkiem, który regenerował się szybciej, niż bot zdejmował
  mu życie, nie miała końca. Teraz obowiązuje ten sam test co przy kamieniach.
- **Boty przestały bić potwory dużo poniżej swojego poziomu.** Zwykły cel
  wybierany był po samej odległości, więc bot z 19 poziomu tłukł psy z 1
  poziomu pod murami Joan. Tabela doświadczenia silnika płaci za takiego
  potwora jedną setną — potwór niżej niż o dziewięć poziomów jest teraz
  pomijany, dopóki w zasięgu jest cokolwiek lepszego.
- **Postacie botów przepadały bezpowrotnie po przejściu na mapę drugiego
  rdzenia.** Takie przejście udaje się połowicznie: współrzędne się zmieniają,
  wstawienie do sektora nie — a zwykłe wylogowanie utrwala pozycję, której
  żaden start już nie wczyta. Bot nie był zablokowany ani bezczynny; był
  odrzucany przy każdym uruchomieniu i po prostu nie istniał. Teraz rozjazd
  między mapą a pozycją jest wykrywany i bot wraca do Bokjung.

  Postaci, które zdążyły utknąć wcześniej, poprawka nie odzyska — trzeba je
  przestawić raz w bazie:

  ```sql
  UPDATE player.player SET map_index = 21, x = 55700, y = 157900
  WHERE name LIKE 'bot%'
    AND map_index NOT IN (21,23,24,25,26,61,63,64,69,70,73,108,216,217);
  ```

- **Znak straganu znikał tylko tym, którzy akurat patrzyli.** Odwołanie szyldu
  leci do klientów będących w zasięgu w tej jednej chwili, więc kto podszedł
  sekundę później, widział bota z szyldem sklepu, którego nie da się otworzyć.
  Zamknięty stragan odbiera swój znak przez sześć sekund.
- **Szybka przebudowa serwera potrafiła po cichu wydać starą binarkę.** Dotyczy
  tylko osób budujących lokalnie przez `tools/fast-game-build`: ścieżka
  kontenera była przepisywana przez Git Bash, plik nie dostawał świeżego
  znacznika czasu i `make` uznawał go za aktualny. Teraz brak odświeżenia
  przerywa budowę zamiast wydać stary plik.

---

## 1.25.8 — 2026-09-05

### Nowe

- **Boty kupują wreszcie bransolety, naszyjniki i kolczyki.** Drabinę sprzętu
  miało pięć slotów; te trzy nie miały żadnej, więc bot nosił ozdobę tylko
  wtedy, gdy jakaś mu wypadła. Większość chodziła z trzema pustymi polami na
  karcie postaci przez całe życie.
- **Ozdoba jest dobierana do klasy, nie do poziomu.** Kolczyki rotują
  statystyką w miarę wzrostu — zwinność, siła, kondycja, inteligencja — więc
  najwyższy nie jest najlepszy dla każdego. Szaman na 62 poziomie bierze teraz
  kolczyk z 54 poziomu, bo ten ma inteligencję, a ten nowszy dałby mu siłę.
  Wojownik i sura walczący bronią biorą kolczyk z siłą, ninja ze zwinnością,
  szaman i sura magiczny z inteligencją.
- **Cała wycena sprzętu rozróżnia teraz klasy.** Siła, zwinność i inteligencja
  były warte po tyle samo, kimkolwiek byłeś. Statystyka, którą klasa walczy,
  jest teraz warta dwa razy tyle, a te nieużywane jedną piątą. Dotyczy to nie
  tylko zakupów, ale i porównywania tego, co wypadło z potwora.

### Naprawione

- **Do Lochu Małp praktycznie nikt nie chodził, a suwak konia w panelu nic nie
  robił.** Dwie osobne przyczyny. Warunek wyprawy wymaga poziomu 25, żeby koń
  mógł wejść z zera na jedynkę, a pasmo lochu kończyło się na 26 — razem dawało
  to okno **dokładnie dwa poziomy szerokie**, po którym drzwi zamykały się na
  zawsze. Na testowym świecie 446 botów było powyżej pasma i 435 z nich miało
  konia poniżej dziesiątki: odcięte od jedynego źródła medali, a przez to od
  konia bojowego. Teraz powyżej pasma bot nadal przychodzi po medal, dopóki ma
  konia do podniesienia, z trzykrotnie mniejszą szansą — i wychodzi, gdy tylko
  medal ma. Druga przyczyna: waga HORSE była czytana wyłącznie przez planer
  celów, a bramka, która faktycznie wysyła bota do lochu, nigdy o niej nie
  słyszała. Teraz suwak działa.
- **Boty kupowały najgorszą bransoletę w grze.** Wytrzymałość nie miała własnej
  wagi i wpadała do domyślnego kosza, więc dziesięć jej punktów ze startowej
  bransolety było warte więcej niż cokolwiek, co ta linia oferuje poniżej 46
  poziomu. Wytrzymałość to pasek biegu — jest teraz warta tyle co punkty many.

## 1.25.7 — 2026-09-05

### Naprawione

- **Bot sprzedawał konia bojowego kupcowi trzy minuty po tym, jak go zdobył.**
  Reguła złomowania mówi „to szrot", dopóki ktoś jej wyraźnie nie zaprzeczy.
  Medal konny taki wyjątek miał od dawna, z komentarzem o tym, że postęp w
  zadaniu musi przeżyć wizytę u kupca — nagroda, do której medal prowadzi, już
  nie. Pierwszy bot w historii, który ukończył próbę (dwadzieścia jeden medali,
  sto Czarnych Wichrów i 500 000 yang u stajennego), poszedł do miasta i sprzedał
  zwój przyzwania za swoją część z 1020 sztuk złota. W całym świecie nie został
  ani jeden.
- **Bot powyżej 36 poziomu przestawał ulepszać broń — na zawsze.** Drabina
  sprzętu sprawdzała tylko osiem stopni, a ósmy to w czterech z pięciu rodzin
  broń na 36 poziom. Bot uznawał, że jest już uzbrojony, i nigdy nie sięgał
  wyżej. Zgłoszony przypadek bota na 49 poziomie z bronią +0 nie był wyjątkiem,
  tylko regułą. Po zmianie drabina sięga tam, dokąd sięga rodzina:

  | broń | było | jest |
  |---|---|---|
  | miecz | 36 | **80** |
  | dwuręczny | 36 | **87** |
  | łuk | 36 | **80** |
  | dzwonek | 36 | **80** |
  | sztylet | 50 | **75** |

  Zbroja dostała dwa brakujące stopnie i sięga teraz 66 zamiast 54. Tarcze i
  buty już wcześniej dochodziły do końca swojej rodziny i nie były ruszane.
  Hełmy poszły w drugą stronę — o jeden stopień w dół, bo ósmy sięgał po hełm
  innej klasy; nic na tym nie tracimy, każda klasa dochodzi do swojego
  najlepszego hełmu w czwartym stopniu.
- **Boty z przemianowanym nickiem były niewidoczne na stronie.** Osiemnaście
  zapytań panelu pytało, czy nick zaczyna się od „bot" — więc mapa na żywo,
  wszystkie jedenaście rankingów, statystyki świata i strona sezonu przestawały
  je liczyć, gdy ktoś zmienił im nazwy. Panel pyta teraz o to samo co silnik: o
  konto `playerbot_NNN`, którego zmiana nicku nie dotyka. Stary test po nicku
  został jako drugie ramię, żeby ręcznie zrobiony bot na zwykłym koncie nadal
  był widoczny.

### Warto wiedzieć

- Wyższe stopnie broni (45 i dalej) oraz dwa nowe stopnie zbroi są na razie
  potwierdzone tylko z danych — żaden bot nie ma jeszcze poziomu, żeby po nie
  sięgnąć. Potwierdzone na żywym świecie zostały zakupy broni na 40 poziom,
  stopnia, który wcześniej dla bota nie istniał.

## 1.25.6 — 2026-09-05

### Naprawione

- **Bot w kółko próbował kupić to samo z tego samego straganu.** Regresja z
  1.25.5. Kiedy silnik odrzucał zakup, bot pytał o dokładnie to samo w następnym
  ticku — czyli raz na sekundę, aż wyprawa wygasła po półtorej minuty, po czym
  zaczynał od nowa. W dzienniku wyglądało to jak `Shop::Buy pos 0` bez końca.
  Silnik odmawia po cichu: „pełny plecak" i „za mało pieniędzy" zapisuje na
  poziomie logowania, którego nikt nie włącza, więc z zewnątrz widać było tylko
  powtarzane żądanie. Teraz odmowa kończy wyprawę, a bot z góry pomija pozycję,
  która mu się nie zmieści — **broń zajmuje trzy komórki plecaka, a wcześniej
  sprawdzane były dwie**.
- **Próba o konia bojowego liczyła mniej więcej co drugie zabicie.** Bot bije w
  dwóch miejscach: w pełnym cyklu decyzyjnym i w tańszym, pośrednim — a ten
  drugi nie zaliczał niczego. Pełny cykl nie mógł tego nadrobić, bo zabity cel
  jest podmieniany na żywego, zanim kod dojdzie do zaliczania. Do tego przepadały
  wszystkie zabicia dobite przez towarzyszy z grupy, więc bot polujący w drużynie
  potrafił nie ruszyć licznika ani razu. Zmierzone na żywym świecie: **2,6 → 7,7
  zabicia na minutę**.
- **Bot z medalem w plecaku tracił konia bojowego bezpowrotnie.** Próba wymaga
  konia dokładnie na dziesiątym poziomie, a nic nie powstrzymywało bota przed
  oddaniem kolejnego medalu i awansem na jedenastkę — po którym żaden medal,
  quest ani NPC już nie cofnie. Drabina zatrzymuje się teraz na dziesiątce,
  dopóki bojowiec jest do zdobycia. To nie jest zakleszczenie: odbiór sam ustawia
  konia na jedenaście i drabina rusza dalej. Przy okazji bot przestaje zbierać i
  kupować medale, których i tak nie może wydać.

### Warto wiedzieć

- Cały łańcuch konia bojowego przeszedł na żywym świecie **pierwszy raz**:
  medale u stajennego, dziesiąty poziom konia, wyjazd na pustynię, sto zabić
  Czarnych Wichrów, powrót, opłata 500 000 yang i zwój przyzwania w plecaku.
  Wcześniej zatrzymywał się na liczniku zabić i nikt nigdy nie dotarł do końca.

## 1.25.5 — 2026-09-05

### Naprawione

- **Boty w Dolinie Orków stały tylko na środku mapy.** Dolina to dwadzieścia trzy
  wyspy w delcie rzeki, połączone dwudziestoma dwoma mostami. W danych mapy rzeka
  ma znacznik blokady i znacznik wody, a **pomost nad tą rzeką ma samą wodę, bez
  blokady** — i właśnie tak mapa mówi „tędy można przejść". Nawigacja bota
  odrzucała każdą wodę, więc odrzucała też wszystkie mosty. Z punktu wejścia bot
  miał dostęp do 17,6% chodliwego terenu i 161 z 532 grup potworów, a mapa
  rozpadała mu się na dziewiętnaście osobnych kawałków. Teraz jest **jednym
  kawałkiem**. Woda jest przechodnia, ale kosztowna, więc bot wchodzi na most, bo
  innej drogi z wyspy nie ma, a płycizn na pustyni nie brodzi, kiedy obok jest
  suchy ląd.
- **Miejsca łowieckie Doliny obejmowały jedną wyspę.** Lista dwunastu punktów
  została kiedyś przycięta do tego, co bot potrafił osiągnąć przy zepsutej
  nawigacji — mieściła się w prostokącie 37×35 km na mapie, której potwory
  rozstawione są na 133×131 km, i obejmowała 128 z 532 grup. Nowe szesnaście
  punktów wyliczono z danych respawnu mapy: każdy to prawdziwe miejsce potworów,
  sprawdzone jako chodliwe i osiągalne z wejścia, rozstawione co najmniej 12 000
  jednostek od siebie. Obejmują 250 grup.
- **Na straganach leżał szrot.** Sprzęt na poziom 29 i niższy przy +4 albo +5 nie
  trafia już na ladę — z 487 takich sztuk w plecakach botów 272 były na poziom 29
  lub niżej, czyli o klasę za stare dla właściciela i bez wartości dla kogokolwiek
  innego. Wyjątki zostają: dobre bonusy, wszystko od +6, broń na 30. poziom,
  a także **tarcze i hełmy od 21. poziomu**, bo w tych dwóch slotach nie ma nic
  między sprzętem startowym a 41. poziomem.
- **Stragany z jedną sztuką.** Osiemnaście z trzydziestu czterech straganów
  otwartych w ciągu kwadransa niosło dokładnie jeden przedmiot. Lada wymaga teraz
  co najmniej dwóch pozycji — chyba że ta jedna jest tego warta sama w sobie:
  broń na 30. poziom, medal konny, mocny bonus albo ulepszenie od +6.

### Nowe

- **Boty chodzą na zakupy.** Wcześniej kupowały tylko z lady, która akurat stała
  dwadzieścia metrów od nich — stąd dwanaście zakupów na pół godziny przy siedmiu
  setkach botów. Bot, któremu brakuje składnika do ulepszenia, może rozwinąć konia
  albo nie ma broni na 30. poziom, idzie teraz na rynek, wybiera najbliższą ladę
  z tą rzeczą, **podchodzi pod nią** i tam kupuje; jeśli przyszedł po dwie rzeczy,
  zostaje po drugą. Wyprawa ma dziewięćdziesiąt sekund i kończy się w chwili, gdy
  bot stoi wśród straganów i nic go nie interesuje.
- **Mapa świata w panelu pokazuje przeprawy.** Tło mapy malowało każdą wodę jako
  rzekę, więc Dolina Orków wyglądała na deltę bez jednego przejścia — dokładnie
  tak, jak widziały ją boty. Mosty i brody mają teraz własny kolor, a kafle mają
  512 pikseli zamiast 256 i próbkują każdą komórkę pod pikselem, bo most ma 600
  jednostek szerokości i przy próbkowaniu samego środka połowa z nich znikała.

### Warto wiedzieć

- Jeśli u Ciebie Dolina Orków była pusta mimo botów na 36. poziomie i wyżej, to
  mogła być jeszcze jedna, osobna przyczyna: serwer dzieli mapy na trzy procesy,
  a bot nie ma klienta, który mógłby przelogować się między nimi, więc Dolina musi
  być na tym samym procesie co miasta. W kodzie jest tak od 1.22.0 i aktualizacja
  launcherem przebudowuje obrazy, więc nic nie musisz robić — ale jeśli obraz
  serwera jest starszy, boty będą stały pod teleporterem zamiast wejść na mapę.

## 1.25.4 — 2026-09-05

### Naprawione

- **Bot z szyldem sklepu biegający po lochu.** Przenosiny między mapami zwalniały
  wszystko, czego bot nie może zabrać ze sobą — wypisywały z grupy, kasowały cel,
  zsiadały z konia i odwoływały go, żeby nie został przy portalu — ale **sklepu
  nie ruszały**. Bot przeniesiony z otwartą ladą zabierał ją ze sobą, a sklep
  zamykał się dopiero na nowej mapie. Silnik rozsyła wtedy pusty szyld tylko do
  tych, którzy są w zasięgu wzroku **w tamtej chwili** — czyli do przypadkowych
  osób w miejscu docelowym. Ludzie, którzy ten szyld naprawdę widzieli, zostawali
  na rynku i nie dostawali niczego, więc szyld zostawał u nich przyklejony do
  postaci. Stragan jest teraz zamykany tam, gdzie został postawiony.
- **Aktualizacje trwały cztery minuty zamiast kilkunastu sekund.** Paczka 1.25.3
  niosła 4952 pliki zamiast ~200, bo wzorzec obejmujący zawartość `/static`
  zaciągał 4768 wygenerowanych ikon z kontekstu obrazu — a każdy z nich jest
  jeszcze kopiowany do kopii zapasowej. Wysyłane jest teraz samo źródło.
- **Dziennik launchera był częściowo nieczytelny.** Skrypt akcji działa ukryty, z
  wyjściem przekierowanym do pliku, więc pisał stroną kodową konsoli, a launcher
  czytał ten plik jako UTF-8. Dlatego w jednym logu stały obok siebie linie
  poprawne i rozsypane. Obie strony mówią teraz tym samym kodowaniem, a to, co
  trafia do pliku, jest dodatkowo zapisywane bez polskich znaków — bo ten plik
  ląduje w Notatniku i we wklejkach, gdzie ogonki i tak nie przechodzą. Okno
  launchera zostaje z ogonkami, tam wyświetlają się poprawnie.

---

## 1.25.3 — 2026-09-05

> Jeśli po aktualizacji panel nadal pokazuje starą wersję albo brakuje w nim
> nowych zakładek — **to jest ta poprawka**. Przebudowa obrazu, nawet z
> `--no-cache`, nie mogła tego naprawić.

### Naprawione

- **Panel raportował wersję sprzed wielu aktualizacji.** Panel czyta numer z
  `linux-port/docker/panel/app/VERSION`, który stawia tam `prepare-context.sh` —
  skrypt, jak dokumentuje sam `start-server.ps1`, **nigdy nieuruchamiany na
  maszynie gracza**. Plik zostawał więc na wartości z instalatora niezależnie od
  liczby aktualizacji: u jednego z operatorów panel po trzech wydaniach dalej
  mówił 1.15.6, a `docker compose build --no-cache panel` niczego nie zmieniał,
  bo budował ze starego pliku.
- **Dziewięć innych plików miało ten sam problem.** Przejrzeliśmy wszystko, co
  `prepare-context.sh` przygotowuje, i porównaliśmy z tym, co paczka wysyła.
  Zamrożone od instalacji były: `panel/app/VERSION`, `panel/app/CHANGELOG.md`
  (dziennik zmian w panelu), `items.json`, `favicon.png`, cała zawartość
  `/static`, `panel/schema/web_admin_schema.sql` (schemat bazy dla funkcji
  panelu), `game/quest/web_admin.quest` (pomocnik teleportacji z panelu),
  `high_risk.quest` oraz `mob_drop_item.m3.append.txt`. Skrypt startowy
  synchronizuje teraz wszystkie, a paczka niesie ich źródła.
- **Wzorzec `katalog/*` w liście aktualizacji schodzi w podkatalogi.** Dotąd
  łapał wyłącznie pliki bezpośrednio w katalogu, więc `files/static/*` nie
  pasował do niczego. Dodanie drugiego zestawu ikon nie wymaga już edycji listy.

---

## 1.25.2 — 2026-09-05

### Naprawione

- **Panel po aktualizacji zostawał stary.** Panel istnieje w dwóch kopiach:
  `files/admin_panel.py` jest źródłem, a `linux-port/docker/panel/app/admin_panel.py`
  jest tym, z czego powstaje obraz. Przepisuje jedną na drugą `prepare-context.sh`,
  który — jak `start-server.ps1` sam odnotowuje w trzech miejscach — **nigdy nie
  uruchamia się u gracza**. Skrypt startowy nadrabiał to dla źródeł botów, dla
  Makefile i dla seeda SQL, a o panelu zapomniano. Efekt: aktualizacje 1.25.0 i
  1.25.1 przynosiły poprawny panel w `files/`, a obraz i tak budował się ze
  starej kopii — więc suwaki zachowania botów i strona sezonu **nie pojawiały
  się mimo zainstalowanej nowej wersji**. Skrypt startowy synchronizuje teraz
  panel tak samo jak resztę, a obie kopie w paczce są zgodne.

---

## 1.25.1 — 2026-09-05

> Dalej **wydanie eksperymentalne**, na tych samych zasadach co 1.25.0: baza nie
> jest ruszana, powrót do poprzedniej wersji nic nie psuje.

### Naprawione

- **Stragany przestały wystawiać szrot.** W wycenie towaru siedziała łapanka
  `return 1` na wszystko, co nie wpadło do żadnej sensownej kategorii — więc bot
  z ośmioma spadami po +1 wystawiał całą ósemkę. W plecakach botów na
  testowanym świecie leżało **1902 sztuki sprzętu +0…+3** wobec 219 sztuk od +4
  wzwyż, czyli ta jedna linia decydowała o wyglądzie całego targu. Zwykły sprzęt
  wchodzi teraz na ladę **od +4**, a co nie jest ani sprzętem, ani nazwaną
  kategorią (broń na 30 lv, duży bonus, +6, medal konny, ulepszacz, księga) — nie
  jest towarem w ogóle. Lady mają 1–8 pozycji zamiast ośmiu napchanych.
- **Szyld sklepu, którego nie dało się otworzyć.** Silnik rozsyła szyld do
  wszystkich w pobliżu **jedną linijkę przed** utworzeniem sklepu, a gdy tworzenie
  się nie uda, nikt tego szyldu nie cofa: `CloseMyShop` kasuje go tylko wtedy, gdy
  jest co zamykać. Postać odchodziła ze sklepem, którego nie da się kliknąć.
  Teraz szyld jest zdejmowany zawsze, gdy sklep nie powstał albo zniknął bez
  naszego udziału.
- **Bot prowadzący stragan zdejmuje swoje buffy.** Nic nie rzucał — hak straganu
  przerywa pętlę decyzji przed umiejętnościami — ale aura rzucona przed
  siadnięciem do lady dopalała się jeszcze kilkanaście minut i wyglądała, jakby
  postać grała, stojąc w sklepie.

### Nowe

- **Boty zdobywają konia bojowego.** Wzorem jest quest stajennego i większość
  została: 35. poziom postaci, koń już na dziesiątce, sto zabitych potworów na
  pustyni, 500 000 yang na koniec, wydana Księga Opancerzonego Konia i zabrane
  Zdjęcie Konia. Trzech rzeczy nie dało się zachować i każda z nich to fakt o
  tym świecie, a nie decyzja: potwory z questa (2105 i 2107) **nie są nigdzie
  spawnowane**, więc próba liczy bandę Czarnego Wiatru z tej samej pustyni;
  limitu pół godziny nie ma, bo bot poluje godzinami i nie ma komu przegrać; a
  ośmiu do szesnastu godzin oczekiwania też nie, bo populacja, która nigdy się
  nie wylogowuje, przeczekałaby je w logu.
- **Bot, który zdobył konia, przestaje przepalać yang.** Pierwsza obserwacja na
  żywo pokazała bota, który skończył próbę z 432 000 yang i w dwadzieścia minut
  zszedł do 149 000 u kowala i na kamieniach bonusowych — konia nie odebrałby
  nigdy. Kowal, kamienie i zakupy na targu omijają teraz odłożoną opłatę.

---

## 1.25.0 — 2026-09-05

> **Wydanie eksperymentalne.** Wchodzi naraz kilka nowych systemów, które
> dotykają tego, jak boty planują cały swój czas, a nie pojedynczej funkcji.
> Baza danych nie jest ruszana i cofnięcie się do 1.24.9 nic nie psuje, ale
> spodziewaj się, że coś będzie wymagało dostrojenia. Zgłoszenia są mile
> widziane — szczególnie takie z opisem, co bot robił i gdzie.

### Nowe

- **Zachowanie botów regulujesz suwakami w panelu, na żywo.** Nowa strona
  **🧠 Zachowanie botów** daje jedenaście wag: kupowanie mikstur, kowal, księgi
  umiejętności, koń, Biolog, metiny, grupy, misje polowania, zwykłe bicie
  potworów, wędkowanie i stragany. 100 to dokładnie tak, jak serwer był
  zbudowany; 25 znaczy cztery razy rzadziej, 250 — dwa i pół raza częściej.
  Zapis działa w ciągu pięciu sekund, **bez restartu i bez rozłączania kogokolwiek**.
  Ucieczka z przegranej walki, wybór profesji i zdobycie broni nie podlegają
  suwakom — to nie są preferencje.
- **Sezon tygodniowy i rekordy serwera** pod adresem `/season`, dostępne bez
  logowania. Metiny, bossy i ulepszenia, które weszły na +7 lub wyżej, z
  ostatnich siedmiu dni, oraz kafle z rekordami od początku istnienia świata.
  Statystyki liczą się **wstecz przez całą historię serwera**, bo silnik i tak
  zapisywał te zdarzenia od pierwszego dnia — nie trzeba było niczego doliczać.
- **Boty zakładają gildie.** Na 40. poziomie i za 200 000 yang, dokładnie jak
  gracz u Strażnika Wsi. Mistrz gildii przyjmuje potem stojące obok boty tego
  samego królestwa. Herbów jeszcze nie ma.
- **Boty pamiętają, z kim polowały.** Wspólna grupa buduje znajomość, a przy
  szukaniu drużyny bot wybiera teraz tego, z kim już mu się układało, zamiast
  pierwszego napotkanego.

### Naprawione

- **Koń bojowy wreszcie może walczyć.** W pętli decyzji siedziało bezwarunkowe
  zsiadanie wykonywane przed wyborem celu — napisane, zanim walka z siodła w
  ogóle powstała. Bot dojeżdżał do metina konno i natychmiast lądował na ziemi.
  Teraz zsiadają tylko te boty, które i tak nie mogą bić z konia, a decyzja
  zapada tam, gdzie cel jest już znany — więc bot, który podszedł pieszo, potrafi
  też **wsiąść** przed walką.
- **Odporność na omdlenie (NNO) przestała być wyrzucana.** W wycenie bonusów nie
  miała własnego przypadku i wpadała do gałęzi domyślnej, a wartość linii
  immunitetu wynosi 1 — czyli najcenniejszy roll na tarczy w całej grze był dla
  bota wart mniej niż punkt szybkości ruchu i szedł do przerzucenia. Doszły też
  warunki zatrzymania: tarcza z NNO, broń na 30. poziom ze średnimi obrażeniami
  od 30%, pancerz i biżuteria z 1500 PŻ **nie są już nigdy przerzucane**, choć
  nadal mogą dostać kolejną linię.
- **Piąta linia bonusów.** Silnik pozwala na pięć, a pętla kończyła na czterech —
  każdy bot na świecie chodził o jedną linię uboższy. Na uruchomionym świecie
  przedmiotów z pięcioma liniami przybyło z 7 do 108 w ciągu godziny.
- **Boty przestają pilnować jednego respawnu.** Po dojściu na miejsce, na którym
  nie ma już czego bić, planer przesuwał je o siedemset jednostek i czekał
  kolejne 8–12 sekund, w kółko. Teraz idą do następnego miejsca, do którego da
  się dojść. Dotyczy Doliny Orków i Pustyni Yongbi.
- **Logi przestały zagłuszać serwer.** Katalog logów jednego rdzenia miał 4,5 GB
  przy 126 MB pozostałych i rósł o 155–278 MB na godzinę, przy 43 wpisach SYSERR
  na sekundę — a **żaden z nich nie sygnalizował usterki**. Cztery komunikaty
  pisane z pętli przez każdego bota na mapie: dwa nasze, dwa silnika, wszystkie
  na poziomie zapisywanym zawsze. Nasze mają teraz licznik i jedną linię na
  minutę, komunikaty silnika zeszły na poziom diagnostyczny. Po zmianie: **50 MB
  na godzinę i jeden wpis SYSERR na pięć sekund**.
  Efektem ubocznym okazał się wyraźny wzrost tempa gry — przy niezmienionej
  liczbie botów liczba zabić w oknie piętnastu minut wzrosła z ~265 do ~4100.
  Zapisywanie tych logów kosztowało serwer więcej, niż ktokolwiek podejrzewał.

### Zmienione

- Kod botów rozbity na kolejne moduły: planowanie celów, wagi z panelu, bonusy,
  gildie i wyciszanie logów mieszkają teraz w osobnych plikach.
- Łatki silnika są wyszukiwane po wzorcu, a nie wymieniane z nazwy. Jedna z list
  zdążyła się już rozjechać — łatka straganów była nakładana, ale nie wchodziła
  do sumy kontrolnej budowania, więc jej zmiana nie unieważniała gotowego obrazu.

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
