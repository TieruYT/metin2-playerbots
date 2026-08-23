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

## 🚀 Instalacja

### 1. Windows 10 / 11 (Zalecane)

1. Zainstaluj **Docker Desktop** i upewnij się, że włączony jest backend **WSL2**.
2. Uruchom Docker Desktop i poczekaj, aż pojawi się status **Engine running**.
3. Sklonuj repozytorium i uruchom instalator PowerShell:

```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1
```

Instalator automatycznie:
- Skonfiguruje środowisko Docker Compose.
- Zbuduje obraz serwera z obsługą Playerbotów.
- Zwiąże wszystkie usługi bezpiecznie z lokalnym adresem `127.0.0.1`.

---

### 2. Linux (Ubuntu / Debian)

```sh
git clone https://github.com/TieruYT/metin2-playerbots.git
cd metin2-playerbots
sudo sh ./installer/install.sh --local
```

Flaga `--local` gwarantuje, że serwer nasłuchuje wyłącznie na `127.0.0.1` bez otwierania portów na świat.

---

## ⚙️ Konfiguracja (.env)

Główna konfiguracja znajduje się w pliku `linux-port/docker/.env`.

Najważniejsze opcje:
```ini
# Liczba automatycznie spawnowanych botów po starcie serwera
PLAYERBOT_AUTOSPAWN_COUNT=350

# Port logowania (Auth)
M2_AUTH_PORT=11000

# Porty kanału gry (Channel 1)
M2_GAME_PORT_RANGE=13000-13002

# Port panelu webowego
M2_PANEL_PUBLIC_PORT=7788

# Maksymalny poziom postaci
M2_MAX_LEVEL=120
```

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

Wszystkie polecenia wykonuj w katalogu `linux-port/docker/`:

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
