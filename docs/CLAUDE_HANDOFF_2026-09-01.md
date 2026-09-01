# Metin2 Playerbots — handoff techniczny dla Claude Pro (2026-09-01)

## Cel projektu

Projekt rozwija lokalny świat Metin2 r40250 z autonomicznymi postaciami graczy.
Boty korzystają z normalnych rekordów `account`, `player`, `item`, `quest` i
`affect`, mają trwały postęp oraz zachowują się jak gracze: walczą, zbierają
łup, rozwijają ekwipunek i skille, tworzą PT, odwiedzają NPC, wykonują Biologa,
Polowania i rozwijają konia. Panel WWW pokazuje mapę, stan i ekwipunek botów.

Repozytorium publiczne: <https://github.com/TieruYT/metin2-playerbots>

## Ważne ograniczenia

- GitHub zawiera kod projektu, poprawki i narzędzia, ale nie zawiera
  własnościowego klienta gry ani kompletnej binarnej paczki serverfiles.
- Lokalna paczka All-in-One zawiera dodatkowy launcher Windows oraz gotowy
  kontekst uruchomieniowy. Launcher nie jest obecnie częścią zwykłego commita
  źródłowego; jest dostarczany w archiwum serwera i docelowo w kontrolowanym
  GitHub Release.
- Nigdy nie wykonuj `docker compose down -v`. Wolumen `db-data` zawiera konta,
  postacie, przedmioty i cały postęp botów.
- Nie nadpisuj `.env`, `.m2install.json`, `.m2launcher.json` ani
  `.m2launcher-state.json` podczas aktualizacji.
- Drzewo robocze zawiera celowe, jeszcze niezatwierdzone pliki launchera oraz
  zmiany pakietowe. Nie używaj `git reset --hard`, `git checkout -- .` ani
  `git add .`.

## Najważniejsze katalogi i źródła prawdy

| Ścieżka | Rola |
| --- | --- |
| `linux-port/overlays/playerbot/src/game/src/playerbot_manager.cpp` | Główna logika AI Playerbots — źródło prawdy. |
| `linux-port/overlays/playerbot/src/game/src/playerbot_manager.h` | Interfejs managera botów. |
| `linux-port/overlays/playerbot/src/game/src/playerbot_world_rules.h` | Małe, czyste reguły świata testowane bez silnika. |
| `linux-port/overlays/playerbot/patches/0001-core-integration.patch` | Integracja managera z bazowym `game/db/common`: pakiety, load, komendy GM i autospawn. |
| `linux-port/overlays/playerbot/tools/generate_seed.py` | Deterministyczny generator 350 kont i postaci botów. |
| `linux-port/overlays/playerbot/sql/playerbots_seed.sql` | Wygenerowany seed oraz rejestr `common.playerbot_seed_state`. Nie edytować ręcznie. |
| `linux-port/docker/prepare-context.sh` | Składa kontekst kompilacji, nakłada patch integracyjny i kopiuje overlay. |
| `linux-port/docker/game/src/server/...` | Wygenerowany/runtime build context. Musi odpowiadać overlayowi, ale nie jest głównym źródłem zmian. |
| `linux-port/docker/docker-compose.yml` | Stos MariaDB, migrator botów, game i panel. |
| `files/admin_panel.py` | Główne źródło panelu WWW. |
| `linux-port/docker/panel/app/admin_panel.py` | Kopia uruchomieniowa panelu używana przy budowie paczki. |
| `installer/install.ps1` | Starszy instalator repozytorium dla Windows. |
| `start-server.ps1` | Start All-in-One, inicjalizacja tożsamości instalacji i Docker Desktop. |
| `Metin2-Launcher.ps1` | Tekstowy kontroler: start/stop, update, diagnostyka i logi. |
| `Metin2-Launcher-GUI.ps1` | WinForms GUI dla laików. |
| `launcher/Metin2Launcher.psm1` | Bezpieczne pobieranie, SHA-256, ZIP, backup i support bundle. |
| `launcher/Metin2Launcher.Diagnostics.psm1` | Diagnostyka Docker/WSL/wirtualizacji/portów i czytelne komunikaty. |
| `launcher/server-update-files.txt` | Jawna allowlista plików trafiających do paczki aktualizacji. |
| `tools/New-M2UpdatePackage.ps1` | Buduje różnicowy ZIP oraz fragment manifestu z SHA-256. |

## Dwa sposoby uruchamiania

1. **All-in-One (zalecany użytkownikom):**
   `Metin2-Launcher-GUI.bat` → GUI → `Metin2-Launcher.ps1` →
   `start-server.ps1` → `linux-port/docker/docker-compose.yml`.
2. **Starszy instalator repo:**
   `installer/install.ps1` składa serwer w wybranym katalogu i uruchamia
   Compose. Nowy launcher musi rozpoznać taki istniejący projekt i przejąć jego
   tożsamość oraz wolumen zamiast tworzyć drugi stos.

Każda niezależna świeża instalacja dostaje `.m2install.json`, nazwę projektu
Compose i prefiks kontenerów. Migracja starej instalacji może przyjąć jej nazwę
projektu oraz `.env`, ale tylko gdy znaleziono dokładnie jednego zgodnego
kandydata. Przy wielu instalacjach launcher ma przerwać bez zmian.

## Cykl życia Playerbota i krytyczna granica bezpieczeństwa

Seed tworzy 350 kont `playerbot_NNN` i zapisuje PID-y w
`common.playerbot_seed_state`. Autospawn nie może zakładać, że dowolny ciąg PID
`4..353` nadal należy do botów. Usunięta baza, częściowy seed albo inna kolejność
tworzenia postaci może umieścić zwykłą postać użytkownika w tym zakresie.

Wymagana zasada: **botem może zostać wyłącznie postać obecna jako aktywna w
rejestrze seeda i nadal należąca do kanonicznego konta `playerbot_%`.** Walidacja
ma działać przed użyciem cache gracza oraz przed wejściem fake-descriptora do
gry. Inaczej zwykła postać zostaje załadowana offline jako bot, a normalne
logowanie zawiesza się na ekranie ładowania.

## Launcher i aktualizacje

Manifest domyślny:
`https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/update-manifest.json`.

`Sprawdź aktualizacje` jest operacją tylko do odczytu. Brak manifestu (HTTP 404)
oznacza nieopublikowany kanał i ma kończyć się informacją, nie czerwonym błędem,
nie instalacją oraz nie uruchomieniem drugiego stosu. Instalacja aktualizacji:

1. pobiera ZIP wyłącznie z HTTPS;
2. weryfikuje dokładne SHA-256;
3. odrzuca absolute path i `..` w ZIP;
4. odrzuca ścieżki chronione;
5. robi backup istniejących plików;
6. kopiuje tylko allowlistę;
7. przebudowuje Compose bez kasowania wolumenów;
8. zapisuje wersję i przy ponownym kliknięciu pomija wersję już zainstalowaną.

Publiczny Release z paczkami aktualizacji nie został jeszcze opublikowany.
Przygotowanie lokalne nie jest równoznaczne z zgodą na publiczny upload.

## Zgłoszenia naprawiane w tej serii

1. Zwykła postać użytkownika była automatycznie ładowana jako bot i blokowała
   późniejsze logowanie, szczególnie widoczne w Chunjo M1.
2. Starszy `installer/install.ps1` potrafił utworzyć w kontekście Dockera katalog
   `artifacts.json`, a później `docker cp` próbował nadpisać go plikiem.
3. Launcher w nowym folderze nie rozpoznawał wcześniejszego projektu Compose i
   tworzył nową nazwę, co kończyło się konfliktem portu `127.0.0.1:7788`.
4. Błąd GitHub 404 był przedstawiany jako awaria launchera.
5. Brakowało zrozumiałej diagnostyki dla wyłączonej wirtualizacji, niesprawnego
   WSL 2, startującego Docker Engine i zajętego portu panelu.

## Zasady pracy dla kolejnego modelu

- Najpierw ustal, czy edytowany plik jest źródłem prawdy, patchem integracyjnym
  czy tylko wygenerowaną kopią runtime.
- Przy zmianie integracji core aktualizuj `0001-core-integration.patch`, a potem
  regeneruj kontekst przez `prepare-context.sh`; nie zostawiaj poprawki wyłącznie
  w `linux-port/docker/game/src/server`.
- Po zmianie generatora uruchom:
  `python linux-port/overlays/playerbot/tools/generate_seed.py --check`.
- Parser PowerShell:
  `[System.Management.Automation.Language.Parser]::ParseFile(...)` dla każdego
  zmienionego `.ps1/.psm1`.
- Test reguł świata kompiluje się bez całego serwera z
  `tests/playerbot_world_rules_test.cpp`.
- Pełna kompilacja serwera odbywa się przez Docker/Compose i może potrwać kilka
  minut. Zmiany kilku plików C++ powinny korzystać z istniejącego cache builda.
- Test migracji musi potwierdzić, że identyfikator projektu i nazwany wolumen
  bazy są zachowane. Niedozwolone jest testowanie przez usunięcie wolumenu.
- Nie dodawaj klienta, archiwów RAR/ZIP, sekretów, `.env` ani lokalnych logów do
  commita.

## Stan przekazania

Zmiany tej serii są integrowane i testowane w lokalnym drzewie roboczym. Przed
commitem należy sprawdzić `git diff`, jawnie wybrać pliki źródłowe i nie dodawać
całego brudnego drzewa.

### Domknięcie sesji 2026-09-01 (druga tura, Claude)

Kontynuacja przerwanej pracy nad launcherem. Znaleziono i naprawiono realny błąd,
przez który migracja starej instalacji nie wykrywała wielu stosów.

**Naprawiony błąd krytyczny — wykrywanie instalacji Docker (Windows PowerShell 5.1).**
W `start-server.ps1` wzorzec `@(<pipeline> | ConvertFrom-Json)` w Windows
PowerShell 5.1 nie enumeruje wieloelementowej tablicy JSON — zwija ją do jednego
zagnieżdżonego elementu. Skutek: `docker inspect` dla wielu kontenerów dawał 1
„pseudo-obiekt" bez etykiet, `Get-CompatibleDockerInstallations` odrzucał go i
zwracał 0 kandydatów, więc launcher zamiast przerwać jako „ambiguous" cichcem
tworzył świeżą tożsamość `m2pb-...`. Poprawka: najpierw przypisać wynik do
zmiennej, dopiero potem `@(...)` — `$parsed = ... | ConvertFrom-Json; $objects =
@($parsed)`. Zastosowano w 3 miejscach (linie ~114, ~159, ~298). To realna
regresja produktowa, nie tylko artefakt testu: z prawdziwym Dockerem, który
zwraca 2+ kontenery, guard nazwy projektu był rozbrajany.

**Poprawka testu.** `tests/launcher_legacy_identity_test.ps1` asercja „ambiguous
detection must not select a project" używała `(?m)^M2_COMPOSE_PROJECT_NAME=$`,
które w .NET nie matchuje przed `\r` w pliku CRLF. Ponieważ launcher w przypadku
niejednoznacznym poprawnie NIE dotyka `.env` (zostaje CRLF), asercja fałszywie
padała. Zmieniono na `=\r?$` (intencja: wartość nadal pusta), niezależnie od
końców linii. Ścieżka „single install" przechodzi bez zmian, bo tam launcher
przepisuje linię i normalizuje ją do LF.

**Weryfikacja (wszystko zielone):**

- `tests/launcher_legacy_identity_test.ps1` → PASS (single adopted, exact volume
  `legacy_db-data`, sekrety zachowane, wiele instalacji odrzucone, zero
  destrukcyjnych komend docker).
- `tests/launcher_diagnostics_test.ps1` → PASS (6 sklasyfikowanych przypadków,
  fallback `UNKNOWN`).
- `[Parser]::ParseFile` bez błędów dla wszystkich zmienionych `.ps1/.psm1`
  (`Metin2-Launcher.ps1`, `Metin2-Launcher-GUI.ps1`, `start-server.ps1`,
  `installer/install.ps1`, oba moduły w `launcher/`, `tools/New-M2UpdatePackage.ps1`,
  oba pliki testów).
- `python .../tools/generate_seed.py --check` → OK (350 botów, SHA-256 seeda
  zgodne).
- Zweryfikowano zgodność zapytania walidacyjnego w `playerbot_manager.cpp` ze
  schematem seeda: `login=playerbot_NNN`, `social_id='9'+12 cyfr`, `pi.pid1=pid`,
  `pid2..4=0`, `empire=2`, `state IN ('complete','adopted')` — dokładnie
  odzwierciedla kanoniczną walidację z `playerbots_seed.sql`. Fail-closed uruchamia
  się tylko przy realnie pustym/uszkodzonym rejestrze.
- Brak kompilatora C++ w środowisku Windows deweloperskim — testu
  `tests/playerbot_world_rules_test.cpp` ani pełnego managera nie da się tu
  skompilować; kompilacja odbywa się w obrazie gry (Docker).

**Stan budowy fixu „kradzieży konta" (zweryfikowany bajtowo).** Zmiany w
`playerbot_manager.cpp/.h` są w overlayu (źródło prawdy). Wbrew wcześniejszej
notatce (fałszywy negatyw z `Select-String -SimpleMatch` na wzorcu z `|`)
runtime-kopia `linux-port/docker/game/src/server/game/src/playerbot_manager.cpp`
NIE jest przestarzała — jej SHA-256 jest identyczne z overlayem, guard
`IsRegistered`/`PLAYERBOT_AUTH` jest obecny, a staged `input_db.cpp` woła
`SpawnRegistered(...)` w autospawnie. Kontekst builda jest więc aktualny; `docker
compose build game` skompilowałby fix od razu.

**Naprawiona niespójność patcha integracyjnego.** Staged `input_db.cpp` używał już
`SpawnRegistered((size_t)autoSpawnCount, 2)`, ale `0001-core-integration.patch`
miał w tym miejscu jeszcze starą pętlę `for (pid=4..) Spawn(pid,2)`. Pełna
re-asemblacja (`prepare-context.sh` ponownie nakłada patch na świeże źródła)
cofnęłaby autospawn do pętli. Zaktualizowałem hunk patcha (linia
`@@ -1355,0 +1363,22 @@` → `,23`), tak by generował dokładnie ten sam blok, co
staged tree (potwierdzone bajtowo: tabulacje i LF). Obie wersje są bezpieczne
(guard jest w `Spawn()`), ale teraz patch = staged = overlay.

**Co znaczy „live".** Na tej maszynie NIE istnieje jeszcze żaden obraz `metin2`
(Docker był wyłączony, `docker images` puste). Fix stanie się aktywny przy
pierwszej budowie obrazu gry: `1. ZAINSTALUJ` (pełna asemblacja z prepare-context)
albo `docker compose build game`. `2. GRAJ` (`docker compose up -d`) używa gotowego
obrazu i nie buduje. Pełna asemblacja od zera jest długa i wymaga lokalnej paczki
źródeł r40250 (`-Archive`/`-ReferenceDir`) — projekt nie publikuje plików gry.

### Pliki dotknięte w tej turze

- `start-server.ps1` — 3× poprawka enumeracji `ConvertFrom-Json` (plik nietrackowany).
- `tests/launcher_legacy_identity_test.ps1` — tolerancyjny regex CRLF (plik nietrackowany).
- `linux-port/overlays/playerbot/patches/0001-core-integration.patch` — hunk
  autospawnu przełączony na `SpawnRegistered`, spójny ze staged tree (walidacja
  niezmienników unified-diff przeszła).
- `docs/CLAUDE_HANDOFF_2026-09-01.md` — ta sekcja.

Pozostałe zmiany (bot ownership w C++, `installer/install.ps1` artifacts.json,
`docker-compose.yml` tożsamość projektu, `.env.example`, `.gitignore`,
`10-import-dumps.sh`) pochodzą z pierwszej tury i pozostają nietknięte —
zweryfikowane pod kątem spójności, bez modyfikacji.

### Sesja debugowa: postawienie serwera launcherem (2026-09-01, trzecia tura)

Uruchomiono pełny stack natywną ścieżką launchera i przetestowano go na żywo.

**Sprostowanie do notatki wyżej:** obrazy `metin2/*` JEDNAK istnieją (poprzednie
`docker images` było puste tylko dlatego, że Docker był wyłączony). Obecny stack
startuje z gotowego `metin2/game:40250` — zbudowanego PRZED fixami tej serii, więc
guard „kradzieży konta" nie jest jeszcze w działającym obrazie. Aktywacja fixu
nadal wymaga rebuildu obrazu gry.

**Znaleziony i naprawiony błąd launchera (to najpewniej „Błąd: Start (kod )" z
GUI).** `docker compose` wypisuje zwykły postęp („Container … Recreate/Started")
na **stderr**. Pod `$ErrorActionPreference='Stop'` Windows PowerShell 5.1 promuje
pierwszy wiersz stderr do błędu terminującego (`NativeCommandError`), więc start
kończył się wyjątkiem i czerwonym „BŁĄD", mimo że wszystkie kontenery wstawały.
Plik stosował już obejście w `Test-DockerApi`/`Invoke-DockerQuery`, ale trzy
wywołania `docker compose` je pomijały. Naprawione (uruchamiać pod `'Continue'`,
decydować z `$LASTEXITCODE`):
- `start-server.ps1` — `docker compose up` + `docker compose ps` (ścieżka „2. GRAJ"),
- `Metin2-Launcher.ps1` → `Stop-Server` — `docker compose stop` („2. Zatrzymaj"),
- `Metin2-Launcher.ps1` → `Rebuild-Server` — `docker compose up -d --build` (update).

Weryfikacja cyklu przez launcher: `Start` → exit 0, `Stop` → exit 0, `Start` →
exit 0 (wcześniej `Start` dawał exit 1 z „BŁĄD: Container metin2-db Recreate").

**Stan serwera po postawieniu (projekt `metin2`, reużyty istniejący wolumen):**
- `metin2-db` healthy, `metin2-playerbot-migrate` Exited 0, `metin2-game` healthy,
  `metin2-panel` healthy.
- Porty: `11000` (auth) + `13000-13002` (rdzenie) + `7788` (panel) — wszystkie
  nasłuchują; panel `/map` → HTTP 200.
- `player.player` z prefiksem `bot%`: **668** postaci (świat aktywny, boty biegają).

**Ustalenia diagnostyczne dla świadomości:**
- migrate: `WARNING: existing non-canonical Playerbot cohort detected — preserving
  it unchanged; canonical seed skipped` (przy `PLAYERBOT_SEED_STRICT=0`). To NIE
  jest świeży kanoniczny seed 350 — to zachowany wcześniejszy świat (668 botów) z
  wolumenu `metin2_db-data`. Świeży kanoniczny świat wymagałby nowej tożsamości/
  wolumenu.
- Nieudany `metin2-playerbot-migrate` (Exited 1) sprzed ~19 h wynikał ze starej
  niezgodności haseł (`Access denied for user 'metin2'`); aktualne `.env` już
  pasuje (użytkownik `metin2` uwierzytelnia się) i migrate przechodzi.
- Na dysku zalega 5 stacków (`metin2` + `m2pb-34c3e45f/84d22dbd/a947c2ba/a273f69c`)
  i ich wolumeny — kandydaci do sprzątania, ale świadomie nietknięte (zakaz
  `down -v`; każdy `*_db-data` to potencjalny postęp świata).
- Szum w logach gry to tylko `PLAYERBOT_NAV: no progress/unreachable` (nawigacja
  botów) i brak koreańskiego stringa lokalizacji — nic krytycznego.

**Pliki dotknięte w tej turze (obie nietrackowane):**
- `start-server.ps1` — obejście stderr dla `compose up`/`compose ps`.
- `Metin2-Launcher.ps1` — obejście stderr dla `Stop-Server` i `Rebuild-Server`.

### Sesja: test paczki „od zera" przez GUI (2026-09-01, czwarta tura)

Użytkownik rozpakował paczkę `Metin2_Singleplayer_Server_r40250_FIXED.zip`
(izolowana tożsamość `m2fresh`) i kliknął GRAJ. Serwer wstał POPRAWNIE (`m2fresh-db`
healthy, migrate Exited 0, `m2fresh-game`/`m2fresh-panel` healthy, panel `/map`
HTTP 200, **350 kanonicznych botów** — świeży seed), ale GUI pokazało czerwone
„Błąd: Start (kod )" z pustym kodem i okno „Operacja nie powiodła się".

**Przyczyna (osobny bug od stderr, dotyczy GUI):** `Metin2-Launcher-GUI.ps1`
uruchamia akcje przez `Start-Process powershell.exe -PassThru
-RedirectStandardOutput/-RedirectStandardError`, a potem czyta
`$activeProcess.ExitCode`. Z przekierowanymi strumieniami `Start-Process -PassThru`
NIE zachowuje uchwytu OS procesu, więc `.ExitCode` po zakończeniu czyta się jako
`$null` — dla KAŻDEJ akcji, także w pełni udanej. `$null -eq 0` → gałąź „Błąd",
a „kod $exitCode" → „kod " (puste). To najpewniej pierwotny objaw z pierwszego
zgłoszenia (pusty kod, nie liczbowy).

**Fix:** zbuforować uchwyt zaraz po `Start-Process`:
`try { $null = $script:activeProcess.Handle } catch {}` w `Start-LauncherAction`.
Zweryfikowane repro: bez `.Handle` → ExitCode `[]`; z `.Handle` → 0/3 poprawnie.
Fix wgrany do źródła, do rozpakowanej paczki użytkownika i do ZIP-a w Downloads.

**Plik dotknięty:** `Metin2-Launcher-GUI.ps1` (nietrackowany) — cache `.Handle`
w `Start-LauncherAction`.

### Sesja: trzy nowe funkcje (2026-09-01, piąta tura)

Na prośbę użytkownika (pod publiczne wydanie na Discord).

**(B) Fix mapy — bot znika po zmianie nazwy.** Panel filtrował boty po nazwie
postaci (`admin_panel.py:5274`: `WHERE name LIKE 'bot%'`), więc zmiana nazwy w
`player.player` usuwała je z mapy. Zmieniono na wykrywanie po **koncie**:
`LEFT JOIN account.account a ... WHERE (LEFT(a.login,10)='playerbot_' OR p.name
LIKE 'bot%')` — odporne na zmianę nazwy, bez regresji (ramię `bot%` zostaje).
Obie kopie: `files/admin_panel.py` i `linux-port/docker/panel/app/admin_panel.py`.
Zweryfikowano na żywej bazie (350 botów, wykrywane po `playerbot_001…`). **Wymaga
przebudowy obrazu panelu.**

**(C) Regulacja liczby grających botów (0–350).** Zmienna
`PLAYERBOT_AUTOSPAWN_COUNT` już sterowała tym w `.env`; dodano wygodne ustawianie:
- `Metin2-Launcher.ps1` → funkcje `Get/Set-PlayerbotCount`, akcja `-Action SetBots
  [-BotCount N] [-Yes]`, pozycja 13 w menu. Headless-safe: przy `-BotCount` nie
  woła `Read-Host`; restart tylko przy `-Yes`.
- `Metin2-Launcher-GUI.ps1` → przycisk „USTAW LICZBĘ BOTÓW" + InputBox
  (`Microsoft.VisualBasic`). Limit efektywny = liczba zaseedowanych botów (350).
  Zmiana wymaga restartu serwera (env czytany przy starcie rdzenia).

**(A) Import bazy z innej instalacji Docker (wyższe postacie).** Nowe funkcje w
`launcher/Metin2Launcher.psm1`: `Get-M2DbDataVolumes`, `Get-M2VolumeWorldStats`,
`Invoke-M2DatabaseImport`. Mechanika: throwaway MariaDB z `--skip-grant-tables`
(nie trzeba znać haseł wolumenów) → `mariadb-dump` 5 baz ze źródła → drop+create+
load do celu. `mysql.*` (użytkownik DB gry i granty) NIE jest ruszane, więc gra
łączy się dalej hasłem CELU. Źródło tylko czytane; cel backupowany przed
nadpisaniem do `backups/db-import-<stamp>/`. Wpięte: `Metin2-Launcher.ps1`
`-Action ImportDb [-ImportSource <projekt|wolumen>] [-Yes]` + pozycja 14 w menu +
przycisk „IMPORTUJ BAZĘ" w GUI (dialog wyboru źródła). Import zatrzymuje serwer
(zwolnienie wolumenu), po imporcie użytkownik klika GRAJ.
Zweryfikowano end-to-end: import `m2pb-84d22dbd` (352 postaci, level 105) do
świeżego wolumenu → 352/105 potwierdzone; backup utworzony; źródło nietknięte.

**Aktualizacje:** do allowlisty (`launcher/server-update-files.txt`) dodano
`0001-core-integration.patch` (spójność przy re-asemblacji po update). Wszystkie
pozostałe zmienione pliki (oba panele, manager, skrypty launchera, compose) już
były w allowliście — auto-update dowiezie te fixy (kopia plików + `up -d --build`
przebudowuje obrazy gry i panelu bez ruszania wolumenów).

**Pliki dotknięte:** `files/admin_panel.py`,
`linux-port/docker/panel/app/admin_panel.py`, `Metin2-Launcher.ps1`,
`Metin2-Launcher-GUI.ps1`, `launcher/Metin2Launcher.psm1`,
`launcher/server-update-files.txt`.

**Walidacja tej tury:** wszystkie `.ps1/.psm1` parsują; oba testy launchera PASS;
oba `admin_panel.py` kompilują; GUI buduje formularz; import round-trip OK.

### Sesja: dostęp do bazy (Navicat) + publikacja update (2026-09-01, szósta tura)

**Fix dostępu do bazy (Navicat/HeidiSQL/DBeaver).** Zgłoszenia: „serwer odrzuca
połączenie", „hasło nieprawidłowe". Przyczyna: mariadb miała tylko `expose: 3306`
(sieć compose), BEZ `ports:` — nic nie nasłuchiwało na hoście. Hasła i uprawnienia
były OK (potwierdzone: `root@%` z `local-playerbots-root` i `metin2@%` z
`local-playerbots-game` uwierzytelniają się zdalnie). Fix: opublikowano
`127.0.0.1:${M2_DB_PUBLISH_PORT:-3306}:3306` (tylko loopback) w
`docker-compose.yml`; dodano `M2_DB_PUBLISH_PORT=3306` do `.env.example`.
Zweryfikowano: po recreate kontenera `127.0.0.1:3306` osiągalny z hosta, root i
metin2 logują się, złe hasło odrzucone. Aktywuje się po recreate mariadb (u
istniejących userów przez update `up -d --build`, u nowych od razu).

**Import bazy — więcej danych w wyborze źródła.** `Get-M2VolumeWorldStats`
zwraca dodatkowo `Created` (z `docker volume inspect .CreatedAt` — kiedy świat
powstał) i `LastPlay` (`MAX(player.player.last_play)` — ostatnia gra). Pokazywane
w akcji `ImportDb`. „Jak długo baza była włączona" nie jest metryką wolumenu w
Dockerze — zamiast tego pokazujemy datę utworzenia i ostatniej gry.

**Publikacja kanału aktualizacji.** VERSION 1.18.0 → 1.19.0. Zbudowano paczkę
update z allowlisty, opublikowano GitHub Release `v1.19.0` (asset = ZIP update),
utworzono `update-manifest.json` (server: version/url/sha256) w repo → manifest
`raw.githubusercontent.com/.../main/update-manifest.json` przestał być 404.
Od teraz „ZAINSTALUJ AKTUALIZACJE" pobiera i nakłada łatki (kopia allowlisty +
`up -d --build`, bez ruszania wolumenów). Auto-update aktualizuje też same skrypty
launchera (są w allowliście), więc kolejne poprawki launchera dojdą tą drogą.

**Pliki dotknięte:** `linux-port/docker/docker-compose.yml`,
`linux-port/docker/.env.example`, `launcher/Metin2Launcher.psm1`,
`Metin2-Launcher.ps1`, `VERSION`, `update-manifest.json` (nowy).

