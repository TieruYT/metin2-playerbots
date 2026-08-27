# 🛠️ Instalacja i Konfiguracja

[English (INSTALL_EN.md)](INSTALL_EN.md) | **Polski**

Przewodnik instalacji i konfiguracji lokalnego serwera Metin2 ze zintegrowanym systemem Playerbots.

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

WebClient nie jest częścią tego forka i instalator zawsze go wyłącza. Nie dodawaj
archiwów gry do Git — patrz [NOTICE.md](../NOTICE.md) i [ATTRIBUTION.md](ATTRIBUTION.md).

---

## 🚀 Instalacja

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
