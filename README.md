# Metin2 Playerbots

**Polski** | [English (README_EN.md)](README_EN.md)

Lokalny, hobbystyczny świat Metin2, w którym autonomiczne postacie graczy naprawdę grają na serwerze: zdobywają doświadczenie, walczą solo i w grupach, zbierają łup, rozwijają ekwipunek, odwiedzają NPC i zapisują swój postęp w tej samej bazie co zwykły gracz.

Projekt rozwija klasyczne pliki serwerowe r40250 o serwerowy system **Playerbots**. Bot nie jest atrapą potwora ani zewnętrznym programem klikającym w klienta. Jest postacią typu PC sterowaną przez AI wewnątrz game core, dlatego pozostali gracze widzą jego ruch, walkę, wyposażenie i rozwój przez zwykły protokół gry.

> [!IMPORTANT]
> Głównym sposobem grania i testowania jest **natywny klient Metin2 dla Windows**. Panel WWW służy wyłącznie do administracji, obserwacji świata i diagnostyki. Klient przeglądarkowy jest dodatkiem opcjonalnym i eksperymentalnym — serwer ani AI botów nie zależą od niego.

## Status projektu

To działający prototyp badawczo-hobbystyczny, a nie gotowa dystrybucja produkcyjna. Obecnie testowany jest przede wszystkim ekosystem M1, przejścia do M2 i łatwy Loch Małp. Priorytetem jest wiarygodne zachowanie wielu niezależnych postaci, nie automatyzacja każdego questa z gry.

> [!NOTE]
> **Obsługiwane Królestwo:** Obecna wersja projektu była wdrażana i testowana wyłącznie dla **Królestwa Chunjo (Żółci / Mapa 21 – Joan)**, w tym dedykowana siatka kolizji NavGrid 2D oraz rozkład 32 hubów expienia i łowców Metinów. Pozostałe królestwa (**Shinsoo / Czerwoni – Mapa 1 Yongan** oraz **Jinno / Niebiescy – Mapa 41 Pyungmoo**) nie zostały jeszcze wdrożone i są zaplanowane w kolejnych etapach rozwoju.

| Obszar | Stan | Uwagi |
|---|---|---|
| Rdzeń botów i trwałość postaci | działający | Postacie PC, zapis poziomu, EXP, Yang, ekwipunku, przedmiotów i flag postępu w standardowej bazie gry. |
| Nawigacja i unikanie kolizji | działający, rozwijany | Nawigacja po danych kolizji mapy, planowanie tras, przejścia, portale oraz odzyskiwanie bota po zakleszczeniu. |
| Walka wręcz i wybór celów | działający | Kombosy, obrażenia obszarowe, rozdzielanie celów, ocena ryzyka i współpraca grupowa. |
| Ninja Dagger/Archer i animacje skilli | działający | Ataki podstawowe, sztylety, łuk, pociski i skille są odtwarzane w natywnym kliencie; pozostają testy regresji kolejnych klientów i profesji. |
| Łup, ekwipunek i gospodarka NPC | działający, rozwijany | Podnoszenie Yang/przedmiotów, ocena sprzętu, zakupy, sprzedaż, strzały, mikstury i ulepszanie u kowala. |
| Profesje, statystyki i skille | działający, rozwijany | Wybór profesji, rozdawanie punktów i używanie zestawów skilli odpowiednich dla klasy. |
| Biolog i koń | wczesna implementacja | Pierwsze misje biologa oraz zdobywanie medali i rozwój konia w oparciu o rzeczywisty postęp. |
| M1, M2 i łatwy Loch Małp | testy integracyjne | Działają cele wielomapowe i przejścia; zachowanie w portalach i lokalnych teleportach wymaga dalszych testów. |
| Panel administracyjny i Live Map | działający | Mapa, statusy, wyposażenie, statystyki, skille, rankingi i narzędzia GM. |
| Gra w przeglądarce | opcjonalna/eksperymentalna | Osobny klient WebAssembly i most WebSocket; domyślnie wyłączone. |

Liczba botów jest konfigurowalna. Scenariuszem testowym są setki aktywnych postaci, ale realny limit zależy od procesora, konfiguracji ticków, logowania i liczby obserwujących klientów. Przy **350 aktywnych botach** bieżący stos runtime zajmuje około **1,60 GiB RAM**: game 1,05 GiB, MariaDB 154 MiB, panel 383 MiB i wsbridge 27 MiB. Cała maszyna WSL2 zajmowała w chwili pomiaru 5,85 GiB working set (obejmuje również Dockera, cache kompilacji i kontener-builder), a natywny klient około 158 MiB. Pomiar wykonano 23 sierpnia 2026 na komputerze z Ryzen 7 9700X, 32 GB RAM i GeForce RTX 4070 Ti; należy traktować go jako migawkę, nie limit. Headless game core nie korzysta z GPU — karta graficzna ma znaczenie dla uruchomionego klienta.

## Co boty potrafią obecnie

### Życie postaci

- tworzyć trwałe postacie różnych klas i płci oraz pojawiać się w prawidłowych lokacjach startowych;
- zdobywać poziomy, EXP i Yang, przeżywać restart serwera oraz odtwarzać stan z bazy;
- wstawać po śmierci w tym samym miejscu, leczyć się przed ponowną walką i wycofywać przy zbyt niskim HP;
- dobierać podstawowe statystyki i profesję, rozwijać właściwe skille i używać mikstur pomocniczych;
- działać solo albo w ograniczonej liczbie grup, dzielić cele i wspólnie podejmować silniejsze potwory lub Kamienie Metin.

### Walka

- wybierać cel z uwzględnieniem poziomu, ryzyka, zajęcia celu przez inne boty i celu grupy;
- wykonywać normalne sekwencje ataków, walkę obszarową oraz skille ofensywne i wspierające;
- używać właściwego rodzaju broni dla klasy i profesji, w tym łuku oraz strzał u Archera;
- utrzymywać buffy bez bezcelowego ponawiania ich co najkrótszy cooldown;
- uciekać od przeciwników, których bot solo nie powinien podejmować, oraz wracać do walki po regeneracji;
- priorytetyzować Kamienie Metin i podejmować silniejsze cele jako drużyna.

### Łup, wyposażenie i ekonomia

- wykonywać po walce dokładne przeszukanie obszaru i podnosić Yang oraz przedmioty należące do bota;
- porównywać broń i zbroję z aktualnym wyposażeniem, zakładać lepszy sprzęt i zachowywać wartościowe rezerwy;
- przekazywać użyteczny sprzęt botom tej samej klasy zamiast automatycznie go niszczyć;
- sprzedawać niepotrzebny łup u handlarza odpowiedniej kategorii, co rozprasza ruch NPC w mieście;
- kupować mikstury, strzały oraz brakującą broń lub zbroję;
- ulepszać kwalifikujące się przedmioty u prawdziwego kowala, z użyciem standardowych kosztów, materiałów i szansy powodzenia;
- wykonywać krótkie, losowe postoje przy handlarzach oraz ograniczone czasowo wizyty u kowala, aby zachowanie nie wyglądało mechanicznie.

### Nawigacja i świat

- odczytywać serwerowe atrybuty terenu i omijać wodę, mury oraz niedostępne komórki;
- budować trasy na siatce nawigacyjnej, upraszczać je po widoczności i realizować etapami;
- obsługiwać bramy, wąskie przejścia, mosty, portale oraz teleporty lokalne;
- wykrywać brak postępu, unieważniać złą trasę i podejmować bezpieczną próbę odzyskania;
- planować wyprawy między M1, M2 i łatwym Lochem Małp.

### Postęp poboczny

- wykonywać pierwsze misje Biologa wymagające zdobycia i oddania przedmiotów;
- zdobywać Medale Konne w Lochu Małp i dostarczać je Stajennemu;
- zapisywać poziom konia i postęp zadań w danych postaci;
- realizować podstawowe misje łowieckie i rozwijać cele niezależne od samego wbijania poziomu.

## Architektura

```text
┌──────────────────────────┐          zwykły protokół TCP
│ Natywny klient Windows   │ ─────────────────────────────────┐
└──────────────────────────┘                                  │
                                                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Kontener game                                                          │
│                                                                         │
│  auth core      channel/game cores      db core                         │
│                     │                    │                               │
│                     ▼                    │                               │
│       PlayerBotManager + stan AI        │                               │
│          │       │       │               │                               │
│          ▼       ▼       ▼               │                               │
│       CHARACTER  ITEM   PARTY/QUEST/SHOP/REFINE                         │
│          │                              │                               │
│          └──────── standardowy zapis ───┘                               │
└───────────────────────────────────┬─────────────────────────────────────┘
                                    │
                                    ▼
                           ┌────────────────┐
                           │ MariaDB        │
                           │ konta/postacie │
                           │ itemy/postęp   │
                           └───────┬────────┘
                                   │ odczyt administracyjny
                                   ▼
                           ┌────────────────┐
                           │ Panel Flask    │
                           │ Live Map / GM  │
                           └────────────────┘

Opcjonalnie: przeglądarka ── WebSocket ── wsbridge ── TCP ── game
```

Najważniejsza własność tej architektury: jedna postać-bot **nie uruchamia osobnego klienta gry i nie otwiera osobnego połączenia TCP**. Decyzje AI są wykonywane wewnątrz game core, a rezultat jest rozsyłany obserwującym klientom standardowymi pakietami ruchu, walki i stanu postaci.

### Główne komponenty

| Komponent | Lokalizacja | Odpowiedzialność |
|---|---|---|
| PlayerBot AI | `linux-port/overlays/playerbot/src/game/src/playerbot_manager.cpp` | Stan botów, cele, walka, loot, NPC, progresja, party i nawigacja. |
| Integracja game core | `linux-port/overlays/playerbot/patches/0001-core-integration.patch` | Postacie PC, wirtualne deskryptory, komendy GM oraz integracja z istniejącymi systemami gry. |
| Panel | `files/admin_panel.py` | Administracja, Live Map, rankingi, ekwipunek, skille i diagnostyka; pełny kod lokalnej strony jest częścią repozytorium. |
| Stos Docker | `linux-port/docker/docker-compose.yml` | MariaDB, game, panel oraz opcjonalne profile narzędziowe. |
| Instalatory | `installer/` | Lokalna instalacja Windows i instalacja Linux. |
| Szybka kompilacja | `tools/fast-game-build/` | Przyrostowa kompilacja zmian C/C++ bez ponownego wysyłania całego kontekstu z dysku Windows. |

MariaDB przechowuje konta, postacie, przedmioty i postęp. Wolumen `game-var` przechowuje między innymi logi i dane runtime, a konfiguracja panelu znajduje się w osobnym trwałym wolumenie. Baza nie jest publikowana na porcie hosta.

## Wymagania

| Element | Wymaganie |
|---|---|
| Architektura | x86-64 Intel/AMD; game core jest kompilowany jako 32-bitowy ELF x86. ARM nie jest wspierany. |
| Windows | Windows 10/11, Docker Desktop z WSL2 oraz natywny klient Metin2. |
| Linux | Ubuntu 22.04/24.04 lub Debian 12/13, Docker Engine i Docker Compose v2. |
| Procesor | Co najmniej kilka logicznych rdzeni; duża populacja botów korzysta z wyższego taktowania pojedynczego rdzenia. |
| Pamięć | Runtime z 350 botami: około **1,60 GiB**; cały WSL2 w zmierzonym środowisku: **5,85 GiB working set**. Zalecane minimum 8 GiB RAM, wygodnie 16–32 GiB. |
| Dysk | Około 40 GB wolnego miejsca na źródła robocze, obrazy, klienta i logi. |
| Klient | Legalnie pozyskany, zgodny klient Windows i zgodne dane/pliki serwerowe r40250. |

## Instalacja

### Windows 10/11 — zalecana konfiguracja lokalna

1. Zainstaluj Docker Desktop i włącz backend WSL2.
2. Po instalacji lub zmianie członkostwa w grupie Dockera uruchom ponownie Windows.
3. Uruchom Docker Desktop i poczekaj na komunikat **Engine running**.
4. Sklonuj repozytorium i uruchom lokalny instalator:

```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git metin2-playerbots
Set-Location .\metin2-playerbots
& .\installer\install.ps1
```

Instalator Windows przygotowuje stos w Docker Desktop, buduje serwer i domyślnie wiąże auth, kanał oraz panel wyłącznie z `127.0.0.1`. Nie otwiera reguł firewalla i nie wystawia świata do sieci.

Jeśli pliki źródłowe lub archiwum zostały legalnie pozyskane lokalnie, można wskazać je przed startem instalatora:

```powershell
$env:M2_SRC_ARCHIVE = 'D:\Metin2\serverfiles.zip'
# albo:
$env:M2_SRC_REFERENCE_DIR = 'D:\Metin2\40250-reference'
& .\installer\install.ps1
```

### Linux — lokalnie/offline

```sh
git clone https://github.com/TieruYT/metin2-playerbots.git metin2-playerbots
cd metin2-playerbots
sudo sh ./installer/install.sh --local
```

Opcja `--local` wiąże usługi z `127.0.0.1` i nie modyfikuje firewalla. To zalecany wariant dla tego projektu. Udostępnianie serwera poza komputer wymaga osobnej decyzji dotyczącej adresów, bezpieczeństwa, prawa i kopii zapasowych; nie jest domyślnym celem Playerbots.

### Ręczne przygotowanie stosu deweloperskiego

Po przygotowaniu legalnego źródła i kontekstu:

```sh
cd linux-port
./fetch-sources.sh --archive /sciezka/do/serverfiles.zip
cd docker
cp .env.example .env
# uzupełnij M2_PUBLIC_ADDRESS i hasła
docker compose up -d --build
```

Plik `.env` zawiera sekrety. Nigdy nie należy go commitować ani wysyłać w zgłoszeniu błędu.

## Codzienny rozruch i zatrzymanie

Polecenia wykonuje się w katalogu stosu:

```powershell
Set-Location .\linux-port\docker
docker compose up -d
docker compose ps
```

Przydatne logi:

```powershell
docker compose logs -f game
docker compose logs -f panel
docker compose logs -f mariadb
```

Bezpieczne zatrzymanie bez kasowania danych:

```powershell
docker compose stop
```

`docker compose down` usuwa kontenery i sieć, ale zachowuje nazwane wolumeny. **Nie używaj `docker compose down -v`**, jeżeli nie zamierzasz usunąć bazy postaci i pozostałego trwałego stanu.

## Natywny klient Windows

Natywny klient jest właściwym interfejsem do obserwowania botów i ręcznej gry:

1. przygotuj zgodnego klienta z legalnie posiadanych plików;
2. ustaw adres serwera na `127.0.0.1`, auth na port `11000` i kanał na zakres skonfigurowany w `.env` (domyślnie `13000–13002`);
3. uruchom klienta dopiero po osiągnięciu przez `game`, `mariadb` i `panel` stanu działającego;
4. używaj panelu tylko pomocniczo — do uruchamiania populacji, śledzenia statusu, diagnostyki i operacji GM.

Opcjonalny `client-builder` może przygotować paczkę klienta z legalnie dostarczonego archiwum. Nie zmienia to faktu, że klient Windows jest główną ścieżką gry.

Środowisko referencyjne tego projektu jest obecnie testowane plikiem `Client/Metin2Distribute.exe` z klasycznego klienta r40250: `Metin2Client`, wersja pliku `1.0.28249.1`, skonfigurowanym na `127.0.0.1`. Repozytorium nie zawiera tego pliku ani zasobów klienta; nazwa i wersja służą wyłącznie do precyzyjnego opisania zgodności testowej.

## Panel administracyjny i Live Map

Lokalny panel jest dostępny pod adresem:

```text
http://127.0.0.1:7788
```

Jeżeli `M2_PANEL_PASSWORD` pozostanie pusty podczas pierwszej instalacji, panel wygeneruje hasło i wypisze je jednorazowo w logu. Panel zapewnia między innymi:

- mapę pozycji botów i ich bieżące statusy;
- filtrowanie botów solo, w party, walczących z Metinem i według poziomu;
- podgląd wyposażenia, plecaka, statystyk oraz rozdanych skilli;
- rankingi poziomu, Yang, broni, zbroi, przedmiotów, konia, Biologa i polowań;
- podgląd logów wybranego bota i narzędzia administracyjne/GM;
- zarządzanie ratami EXP, dropu i Yang.

Panel nie podejmuje decyzji za boty. Jest warstwą obserwacji i administracji nad światem działającym w game core.

### Opcjonalny klient przeglądarkowy

Gra w przeglądarce jest domyślnie wyłączona (`M2_BROWSER_PLAY=0`). Wymaga osobno przygotowanego klienta WebAssembly oraz profilu `wsbridge`:

```sh
docker compose --profile browser up -d wsbridge
```

Repozytorium nie dostarcza własnościowych danych klienta. Brak klienta przeglądarkowego nie ogranicza Playerbots, panelu ani natywnego klienta Windows.

## Konfiguracja botów

Najważniejsza zmienna populacji znajduje się w `.env`:

```ini
PLAYERBOT_AUTOSPAWN_COUNT=350
```

Wartość jest przykładem bieżącego środowiska, a nie gwarantowanym limitem. Po zmianie konfiguracji:

```powershell
docker compose up -d --force-recreate game
```

Pozostałe istotne ustawienia:

| Zmienna | Znaczenie |
|---|---|
| `M2_MAX_LEVEL` | Maksymalny poziom postaci obsługiwany przez serwer i panel. |
| `M2_AUTH_PORT` | Port logowania. |
| `M2_GAME_PORT_RANGE` | Porty game core kanału. |
| `M2_PANEL_PUBLIC_PORT` | Port panelu. |
| `M2_LOCAL_ONLY` | Informacja, że instalacja jest dostępna wyłącznie lokalnie. |
| `M2_UPDATE_CHECK` | Ustaw `0`, jeżeli środowisko ma pozostać całkowicie offline. |

Pełny, komentowany zestaw opcji znajduje się w `linux-port/docker/.env.example`.

## Komendy w grze (GM Commands)

Postaciami botów można zarządzać również bezpośrednio z poziomu czatu w grze (wymagane uprawnienia GM / Administratora):

| Komenda | Uprawnienia | Opis | Przykład użycia |
|---|---|---|---|
| `/bot_spawn <player_id> <empire: 1-3>` | GM | Ręczne zespawnowanie konkretnego bota o danym ID gracza do wybranego królestwa (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <player_id>` | GM | Ręczne odłączenie / usunięcie bota ze świata gry. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count: 1-500> <empire: 1-3>` | GM | Masowe zespawnowanie grupy botów z zadanego zakresu ID do wybranego królestwa. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count: 1-500>` | GM | Masowe wylogowanie grupy botów z zadanego zakresu ID. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Wszyscy gracze | Wyświetla na czacie aktualny ranking poziomów i pozycje aktywnych botów. | `/bot_rank` |

## Kompilacja i wdrażanie zmian

### Pełny build game core

Używaj go po zmianach Dockerfile, zależności, modułu DB, questów, danych gry lub wielu elementów runtime:

```powershell
Set-Location .\linux-port\docker
docker compose build game
docker compose up -d --force-recreate game
docker compose logs -f game
```

Pierwszy build tworzy 32-bitowe zależności i trwa wyraźnie dłużej. Kolejne korzystają z warstw Dockera oraz `ccache`, o ile kontekst i zależności się nie zmieniły.

### Szybki build C/C++ Playerbots

Do iteracji nad `playerbot_manager.cpp` służy trwały kontener-builder w `tools/fast-game-build/`. Jednorazowo:

```sh
bash tools/fast-game-build/setup.sh
```

Po zmianie jednego lub kilku plików C/C++:

```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp
docker restart metin2-game
```

Skrypt przyrostowo kompiluje game core, weryfikuje 32-bitowy ELF i buduje obraz `metin2/game:40250`. Nie restartuje serwera samodzielnie; podaje bezpieczną komendę rekreacji wyłącznie kontenera `game`. Ścieżki są wyliczane względem repozytorium, więc klon może znajdować się w dowolnym katalogu.

Szybkiego trybu nie należy używać po zmianie zależności, Dockerfile, DB, questów lub danych — wtedy potrzebny jest pełny build.

### Build panelu

```powershell
Set-Location .\linux-port\docker
docker compose build panel
docker compose up -d --force-recreate panel
```

## Diagnostyka

Najpierw sprawdź stan usług i log game core:

```powershell
docker compose ps
docker compose logs --tail 300 game
```

W przypadku zachowania konkretnego bota porównaj trzy źródła:

1. stan i trasę na Live Map;
2. log bota w panelu oraz wpisy `PLAYERBOT_AI` w logu game;
3. to, co faktycznie renderuje natywny klient Windows.

To rozróżnienie jest ważne. Obrażenia, cooldown i zmiana stanu mogą być poprawne po stronie serwera, a jednocześnie klient może nie odtworzyć właściwego motion packetu. Analogicznie wizualnie prosta droga może być zablokowana przez serwerowy atrybut mapy.

Przy problemach ze skalą najpierw zmniejsz `PLAYERBOT_AUTOSPAWN_COUNT`, odtwórz game container i sprawdź pojedynczą klasę/postać. Dopiero później porównuj zachowanie pełnej populacji.

## Roadmapa

Roadmapa opisuje kierunek, nie obietnicę terminu.

### 1. Stabilizacja obecnego M1

- pełna zgodność animacji normalnych ataków i skilli wszystkich klas, szczególnie Ninja Dagger/Archer;
- poprawne pociski, dystans i tempo łuku widoczne w każdym kliencie;
- testy regresji wizyt u kowala: zdejmowania przedmiotu, seryjnego ulepszania i zakładania najlepszego wyniku przed odejściem;
- testy regresji na małej i dużej populacji;
- czytelna telemetria decyzji AI i chmurka stanu nad botem w natywnym kliencie.

### 2. Inteligentny ekosystem graczy

- długoterminowe cele i różne „osobowości” botów: progresja, handel, Metiny, zadania, pomoc innym;
- role w party, dobór składu i ocena siły grupy przed trudnym celem;
- przekazywanie i docelowo wystawianie sprzętu w prywatnych sklepach;
- wycena przedmiotów na podstawie poziomu, plusa, bonusów, podaży i popytu;
- ograniczanie tłoku przy NPC i naturalne rozłożenie aktywności w czasie.

### 3. Pełniejsza progresja świata

- stabilne M2, Dolina Orków, Pustynia, lochy i kolejne mapy poziomowe;
- Wieża Demonów: wejście, piętra, cele, bossy, kowal i warunki przejścia;
- samodzielny wybór mapy i przeciwników odpowiednich do poziomu oraz sprzętu;
- progresja aż do skonfigurowanego maksymalnego poziomu;
- wyprawy wieloosobowe i powrót do miasta po zakończeniu celu.

### 4. Skille i rozwój postaci

- kompletne zestawy skilli każdej profesji wraz z właściwymi animacjami i targetowaniem;
- czytanie Ksiąg Umiejętności, Kamieni Duchowych i obsługa okresów oczekiwania;
- świadome buildy statystyk i kolejności rozwijania skilli;
- dobór rotacji, buffów i mikstur do celu, klasy oraz składu party;
- rozwój wyposażenia dopasowany do aktualnego i następnego progu poziomowego.

### 5. Questy, Biolog i koń

- kolejne podstawowe misje Biologa, w tym Zęby Orka i zadania z czasem oddawania;
- pełny cykl konia zwykłego, bojowego i militarnego z Medalami Konnymi;
- walka z konia i decyzja, kiedy wierzchowiec daje przewagę;
- wybrane questy fabularne i poziomowe o wysokiej wartości dla progresji;
- ogólny interpreter stanów questa/NPC zamiast osobnego wyjątku dla każdej misji;
- rozszerzone polowania i śledzenie wykonanych etapów w panelu.

### 6. Aktywności niezwiązane z EXP

- łowienie ryb wraz z zakupem wędki, przynęty i rozwojem wędki;
- górnictwo: wyszukiwanie żył, zakup kilofa, wydobycie i przetwarzanie rud;
- crafting, uszlachetnianie i decyzje ekonomiczne dotyczące materiałów;
- odpoczynek, handel i zachowania społeczne, aby świat nie wyglądał jak jedna farma EXP.

## Najważniejsze wyzwania techniczne

- **Nawigacja po starych mapach.** Dane atrybutów, wizualna geometria klienta i faktyczna kolizja serwera nie zawsze są identyczne. Bramy, mosty i teleporty wymagają semantyki wyższego poziomu niż samo A*.
- **Synchronizacja walki.** Serwer rozstrzyga trafienie i obrażenia, ale klient osobno odtwarza motion, combo, skill i pocisk. Poprawny wynik serwerowy nie gwarantuje poprawnej animacji obserwatora.
- **Skala.** Setki autonomicznych postaci nie mogą wykonywać kosztownego wyszukiwania celu, trasy lub zapytania SQL w tym samym ticku. Zadania muszą być budżetowane, rozłożone w czasie i cache'owane.
- **Trwałość.** Bot powinien używać istniejących mechanizmów zapisu, a nie omijać je bezpośrednimi zmianami bazy podczas gry.
- **Ekonomia.** Realistyczna cena przedmiotu wymaga historii transakcji, oceny bonusów, podaży, popytu i siły nabywczej populacji.
- **Kompatybilność r40250.** To stary, 32-bitowy kod C/C++ z ograniczeniami ABI, protokołu i klienta; nowoczesne biblioteki nie zawsze można podmienić bez migracji formatu danych.
- **Testowalność AI.** Zachowanie jest losowe i długotrwałe, dlatego potrzebne są deterministyczne seedy, zdarzenia diagnostyczne, replay decyzji i testy scenariuszowe.

## Bezpieczeństwo, prywatność i sekrety

- Dla projektu offline binduj auth, kanały, panel i most wyłącznie do `127.0.0.1`.
- Nie publikuj portu db core ani MariaDB.
- Nie commituj `.env`, haseł panelu, dumpów SQL, wolumenów Dockera, logów zawierających dane graczy, tokenów ani kluczy.
- Nie commituj gotowych archiwów klienta, binariów gry, pełnego stagingu źródeł lub danych pochodzących z paczki r40250.
- Przed publikacją historii uruchom skan sekretów i sprawdź także wcześniejsze commity — dodanie wpisu do `.gitignore` nie usuwa danych z historii.
- Kopie bazy i wolumenów przechowuj poza repozytorium oraz testuj ich odtworzenie.

## Legalność i zakres repozytorium

**Metin2, nazwy, znaki towarowe, klient, dane gry i oryginalne pliki serwerowe należą do ich odpowiednich właścicieli, w szczególności Ymir Interactive/Webzen. Projekt nie jest z nimi powiązany ani przez nich wspierany.**

Repozytorium powinno zawierać wyłącznie własny kod Playerbots, patche, konfigurację, narzędzia i dokumentację, do których autorzy mają prawa. Nie udostępnia i nie może udzielić licencji na:

- klienta Metin2, pliki pack, modele, tekstury, dźwięki i mapy;
- oryginalne źródła lub gotowe binaria serwera r40250;
- dumpy bazy, konta, hasła ani inne sekrety środowiska;
- materiały pobrane z zewnętrznych paczek bez prawa do redystrybucji.

Użytkownik musi samodzielnie zapewnić legalnie pozyskane, kompatybilne pliki i ocenić zgodność uruchomienia prywatnego serwera z prawem obowiązującym w jego kraju. Projekt jest przeznaczony do lokalnych badań, nauki i zastosowań hobbystycznych. Nie daje zgody na komercjalizację, podszywanie się pod oficjalny serwer ani naruszanie praw właścicieli gry.

Przed utworzeniem zdalnego repozytorium należy szczególnie sprawdzić generowane katalogi `game/src`, archiwa klienta, `work/m2src-cache`, `backups`, `builds`, logi i dumpy SQL. Prywatne repozytorium technicznie ogranicza widoczność, ale nie zastępuje prawa do redystrybucji.

## Credits

- [AzzlackSyndicate/metin2-singleplayer-serverfiles-linux](https://github.com/AzzlackSyndicate/metin2-singleplayer-serverfiles-linux) — upstream portu Linux, instalatorów, stosu Docker i bazowego panelu.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — inspiracja i punkt odniesienia dla eksperymentów z autonomicznymi graczami MMO.
- Społeczność badająca format r40250 i prywatne środowiska Metin2 — wiedza o protokole, mapach, klientach i systemach gry.

Kod i dokumentacja tego projektu opisują wyłącznie rozszerzenia oraz narzędzia stworzone na potrzeby Playerbots. Prawa do gry i jej zasobów pozostają przy odpowiednich właścicielach.
