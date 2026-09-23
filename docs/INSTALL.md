# 🛠️ Instalacja i Konfiguracja

[English (INSTALL_EN.md)](INSTALL_EN.md) | **Polski**

Przewodnik instalacji i konfiguracji lokalnego serwera Metin2 ze zintegrowanym systemem Playerbots.

> [!IMPORTANT]
> **Dwie linie, dwie drogi instalacji.** Obecna linia **2.x** (serwer mt2009) jest rozdawana
> jako pełna paczka `Metin2-Singleplayer-<wersja>.zip` — klient i serwer z launcherem — i
> instaluje się ją tak, jak opisuje rozdział „Linia 2.x (pełna paczka)” poniżej. Reszta tego
> dokumentu — `installer/install.ps1`, `installer/install.sh`, katalog `/opt/metin2/stack`,
> archiwa BYOF — dotyczy linii **1.x** (r40250). Instalator 1.x linii 2.x nie postawi.

---

## 📋 Wymagania sprzętowe

| Element | Wymaganie |
|---|---|
| **System operacyjny** | Windows 10/11 (z Docker Desktop + WSL2) lub Linux (Ubuntu / Debian z Dockerem) |
| **Architektura** | x86-64 (Intel / AMD). Serwer gry kompiluje się jako 32-bitowy ELF x86. |
| **Pamięć RAM** | Minimum 8 GB (dla 350 botów zalecane 16 GB+) |
| **Miejsce na dysku** | ~25–40 GB na kontenery, bazy danych i źródła |
| **Klient gry** | Dowolny standardowy klient kompatybilny z plikami r40250 (np. klasyczny `Metin2Client` v1.0.28249.1) |

---

## 📦 Wymagane pliki zewnętrzne (BYOF)

Repozytorium zawiera Playerboty, patche, instalatory i opakowanie Docker, ale
nie zawiera kodu lub danych gry. Do czystej instalacji przygotuj:

- zgodne archiwum serwera r40250 zawierające `metin2_server+src.tar.gz` oraz `metin2_mysql_dump.zip`, albo rozpakowany katalog `[40250] Reference Serverfile`;
- opcjonalnie archiwum kompatybilnego natywnego klienta Windows. Możesz też użyć własnego, już skonfigurowanego klienta.

Obecna bazowa wersja `metin2_server+src.tar.gz`, wobec której tworzony jest
port, ma SHA-256:

```text
6e9e7339935058f73fead81e609219b496adbc867dfeca70f633031730313001
```

Sprawdzisz plik w PowerShellu poleceniem
`Get-FileHash .\metin2_server+src.tar.gz -Algorithm SHA256`. Odświeżenie TMP4
z 31.03.2025 (`e72d7881...`) zawiera zmienione źródła i nie powinno być
przepuszczane na siłę przez patch bazowy. Instalator rozpoznaje ten wariant i,
jeśli dry-run się nie powiedzie, zapisuje bezpieczny raport w
`C:\Metin2Server\diagnostics\compatibility-report.txt` do dołączenia do issue #5.

„Klient r40250” nie oznacza dowolnego klienta znalezionego pod taką nazwą.
Musi mieć zgodne protokoły/pakiety, `item_proto`/`mob_proto` i locale. Najpewniejszą
parą jest klient pochodzący z tego samego wydania co dostarczone pliki serwera.

WebClient nie jest częścią tego forka i instalator zawsze go wyłącza. Nie dodawaj
archiwów gry do Git — patrz [NOTICE.md](../NOTICE.md) i [ATTRIBUTION.md](ATTRIBUTION.md).

---

## 📦 Linia 2.x (pełna paczka)

### Windows

Rozpakuj paczkę w całości do jednego folderu, uruchom `Serwer\Metin2-Launcher-GUI.bat`,
potem **ZAINSTALUJ / PRZYGOTUJ** i **GRAJ**. Pierwszy start buduje serwer (kilkanaście
minut), kolejne trwają chwilę. Aktualizacje: przycisk **ZAINSTALUJ AKTUALIZACJE**.

### Linux / VPS

Potrzebny jest Docker z wtyczką compose oraz `python3` albo `curl` + `unzip` + `sha256sum`.

1. Rozpakuj folder `Serwer` z pełnej paczki do nowego, pustego katalogu, np. `/opt/metin2/serwer`.
2. Utwórz `.env` z przykładu:
   ```sh
   cd /opt/metin2/serwer
   cp linux-port/docker/.env.example linux-port/docker/.env
   ```
   i ustaw w nim:
   - `M2_DB_ROOT_PASSWORD` i `M2_DB_PASSWORD` — własne, losowe hasła (np. `openssl rand -hex 16`);
     bez nich compose nie wystartuje,
   - `M2_PUBLIC_ADDRESS` — publiczny adres IP albo domena serwera; bez tego klienci zawisną na
     „łączeniu z serwerem”,
   - `M2_PANEL_BIND_ADDRESS=127.0.0.1` — na linii 2.x oba panele domyślnie nie pytają o hasło,
     więc nie wystawiaj ich na świat. Otwierasz je przez tunel SSH:
     ```sh
     ssh -L 7788:127.0.0.1:7788 -L 7790:127.0.0.1:7790 user@twoj-serwer
     ```
     i w przeglądarce `http://127.0.0.1:7788` (panel klasyczny) oraz `http://127.0.0.1:7790`
     (panel zaawansowany).
3. W tym samym folderze uruchom:
   ```sh
   sh linux-port/tools/update.sh
   ```
   Skrypt pobiera najnowszą wersję opublikowaną na GitHubie, sprawdza jej sumę SHA-256,
   przygotowuje kontekst budowania panelu (`linux-port/docker/panel/app` i `panel/schema` z
   folderu `files/`) i uruchamia `docker compose up -d --build`. Kolejne aktualizacje robisz tą
   samą komendą; `sh linux-port/tools/update.sh check` tylko porównuje wersje.
   Jeśli skrypt odpowie, że wersja jest już najnowsza, a budowa staje na `COPY schema/`,
   przygotuj sam kontekst panelu i zbuduj:
   ```sh
   sh linux-port/tools/update.sh stage
   cd linux-port/docker && docker compose up -d --build
   ```
4. W zaporze otwórz porty TCP `11000` (logowanie) i `13000-13002` (kanał 1; z drugim kanałem
   `13000-13012`).

Nie uruchamiaj na linii 2.x `installer/install.sh` ani `m2-updater` z repozytorium — to narzędzia
linii 1.x i odmawiają pracy na folderze 2.x.

---

## 🚀 Instalacja linii 1.x (r40250, z repozytorium)

### 1. Windows 10 / 11 (Zalecane)

1. Zainstaluj **Docker Desktop** i upewnij się, że włączony jest backend **WSL2**.
2. Uruchom Docker Desktop i poczekaj, aż pojawi się status **Engine running**.
3. Sklonuj repozytorium i uruchom instalator PowerShell, podając własne archiwa:

```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1 `
    -Archive 'C:\PlikiMetin2\Reference_Server.zip' `
    -ClientArchive 'C:\PlikiMetin2\Reference_Client.zip' `
    -NoWebClient
```

Jeżeli korzystasz z już skonfigurowanego klienta, uruchom instalację serwera bez
budowania klienta:

```powershell
& .\installer\install.ps1 `
    -Archive 'C:\PlikiMetin2\Reference_Server.zip' `
    -NoClient -NoWebClient
```

Zamiast archiwum można użyć `-ReferenceDir 'C:\ścieżka\[40250] Reference Serverfile'`.
Po pierwszym złożeniu źródła instalator trzyma je w wolumenie Dockera, więc
aktualizacja nie wymaga ponownego podawania archiwum.

Instalator automatycznie:
- Skonfiguruje środowisko Docker Compose.
- Zbuduje obraz serwera z obsługą Playerbotów.
- Zwiąże wszystkie usługi bezpiecznie z lokalnym adresem `127.0.0.1`.

---

### 2. Linux (Ubuntu / Debian)

```sh
git clone https://github.com/TieruYT/metin2-playerbots.git
cd metin2-playerbots
sudo sh ./installer/install.sh --local \
  --archive '/ścieżka/Reference_Server.zip' \
  --no-client --no-web-client
```

Flaga `--local` gwarantuje, że serwer nasłuchuje wyłącznie na `127.0.0.1` bez otwierania portów na świat.

---

## ⚙️ Konfiguracja (.env)

Po instalacji główna konfiguracja znajduje się domyślnie w
`%USERPROFILE%\Metin2Server\.env` na Windows lub `/opt/metin2/stack/.env` na Linux.

Najważniejsze opcje:
```ini
# Liczba automatycznie spawnowanych botów po starcie serwera
PLAYERBOT_AUTOSPAWN_COUNT=350

# Opcjonalnie: minimalna liczba botów, która musi już istnieć w trwałym świecie
# (0 wyłącza kontrolę; ustaw po pierwszym uruchomieniu/odtworzeniu kopii)
PLAYERBOT_EXPECT_MIN_EXISTING_BOTS=0

# Port logowania (Auth)
M2_AUTH_PORT=11000

# Porty kanału gry (Channel 1)
M2_GAME_PORT_RANGE=13000-13002

# Port panelu webowego (tylko liczba; adres bindowania jest osobną opcją)
M2_PANEL_PUBLIC_PORT=7788

# Maksymalny poziom postaci
M2_MAX_LEVEL=120

# Domyślny język tekstów wysyłanych przez serwer (nazwy mobów/przedmiotów/questy)
M2_DEFAULT_GAME_LANGUAGE=pl
```

Jeżeli rozwijasz istniejący świat, ustaw
`PLAYERBOT_EXPECT_MIN_EXISTING_BOTS` na jego bezpieczne minimum. Start zostanie
zatrzymany, gdy Docker wskaże inny daemon albo świeży wolumen z mniejszą liczbą
botów. Przy pierwszej instalacji pozostaw `0`.

Po zmianie konfiguracji w `.env` wystarczy zrestartować kontener gry:
```powershell
Set-Location .\linux-port\docker
docker compose up -d --force-recreate game
```

---

## 🎮 Podłączenie klienta gry

1. W pliku `serverinfo.py` lub konfiguracji launchera ustaw adres IP: `127.0.0.1`.
2. Port logowania (Auth): `11000`.
3. Porty gry: `13000`, `13001`, `13002`.
4. Uruchom klienta gry (`Metin2Distribute.exe`).
5. Możesz stworzyć własną postać i grać ramię w ramię z botami w mieście Joan (Chunjo)!

---

## 🗄️ Dostęp do bazy danych (Navicat, HeidiSQL, DBeaver)

Baza serwera to MariaDB w kontenerze, wystawiona **tylko na tym komputerze**
(`127.0.0.1`, port `3306` — albo inny, jeśli w `.env` ustawiono `M2_DB_PUBLISH_PORT`).
Nowe połączenie w kliencie bazy: typ MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Konto | Do czego | Hasło |
|---|---|---|
| `root` | wszystko | `M2_DB_ROOT_PASSWORD` w `linux-port\docker\.env` |
| `metin2` | tylko bazy gry (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` w tym samym pliku |

Najszybciej: w launcherze GUI przycisk **DANE DO BAZY (NAVICAT)** pokazuje
host, port i oba hasła w polach do skopiowania (w konsoli: akcja `DbAccess`,
pozycja 16 menu). Hasła są losowane przy pierwszym uruchomieniu i nie ma
żadnego „domyślnego” — nie wklejaj ich na Discordzie.

Jeśli klient odpowiada `1045 - Access denied for user 'root'@'172.18.0.1'`,
baza została zainicjalizowana pod innym hasłem niż to, które jest teraz w `.env`.
Kliknij **NAPRAW DOSTĘP DO BAZY** (akcja `RepairDb`): zatrzymuje serwer i
ustawia konta `metin2` i `root` na hasła z `.env`; postacie, przedmioty i boty
zostają nietknięte. Potem GRAJ i zaloguj się jeszcze raz.

## 🔄 Codzienne zarządzanie serwerem

Wszystkie polecenia wykonuj w katalogu instalacji (domyślnie
`%USERPROFILE%\Metin2Server` na Windows):

```powershell
# Uruchomienie serwera w tle
docker compose up -d

# Sprawdzenie stanu kontenerów
docker compose ps

# Podgląd logów gry na żywo
docker compose logs -f game

# Bezpieczne zatrzymanie serwera (zapis bazy postaci)
docker compose stop
```

> [!WARNING]
> Nie używaj polecenia `docker compose down -v`, chyba że celowo chcesz skasować wszystkie postacie i bazę danych!
