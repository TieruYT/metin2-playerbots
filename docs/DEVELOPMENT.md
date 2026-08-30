# 💻 Przewodnik Deweloperski i Diagnostyka

[English (DEVELOPMENT_EN.md)](DEVELOPMENT_EN.md) | **Polski**

Informacje techniczne o strukturze kodu, szybkiej kompilacji oraz debugowaniu zachowań Playerbotów.

---

## 📁 Struktura kodu i Overlay

Wszystkie autorskie modyfikacje Playerbotów znajdują się w katalogu `linux-port/overlays/playerbot/`:

```text
linux-port/overlays/playerbot/
├── src/game/src/
│   ├── playerbot_manager.cpp    # Główna logika AI, NavGrid 2D, role, walka, zakupy, kowal
│   ├── playerbot_manager.h      # Nagłówek i definicje klas
│   └── playerbot_world_rules.h  # Czyste, testowalne reguły podróży między mapami
├── sql/
│   └── playerbots_seed.sql      # Seed bazy MariaDB dla 350 unikalnych postaci
├── serverfiles/
│   └── mob_drop_item.m3.append.txt # Dodatkowy drop progresyjny M3
├── tools/
│   └── generate_seed.py         # Generator kohorty botów (klasy, ekwipunek, imiona)
└── patches/
    ├── 0001-core-integration.patch  # Patch integrujący Playerboty z silnikiem gry
    └── 0002-economy-yang-x5.patch   # Modyfikacja mnożników ekonomii
```

---

## ⚡ Szybka kompilacja C++ (8 sekund!)

Do szybkiej pracy nad kodem `playerbot_manager.cpp` służy dedykowany kontener-builder w `tools/fast-game-build/`. Pozwala on na przyrostową kompilację pojedynczego pliku bez konieczności przebudowywania całego stosu Dockera.

### 1. Jednorazowa konfiguracja
```sh
bash tools/fast-game-build/setup.sh
```

### 2. Kompilacja po edycji kodu
```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp
docker restart metin2-game
```

Skrypt w ~8 sekund kompiluje 32-bitowy plik ELF `game` i automatycznie aktualizuje obraz `metin2/game:40250`.

Po pierwszej pełnej kompilacji warto zachować kontener-builder. Następne zmiany
w managerze mogą wtedy kopiować także zmienione nagłówki, np.:

```sh
bash tools/fast-game-build/build.sh playerbot_manager.cpp playerbot_world_rules.h
```

Czyste reguły mają testy niewymagające uruchamiania serwera. Ich aktualny
zakres i plan dalszego dzielenia kodu opisuje [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 🔨 Pełna przebudowa (Full Build)

Jeśli zmieniłeś pliki konfiguracyjne, Dockerfile, questy lub strukturę bazy danych, wykonaj pełny build:

```powershell
Set-Location .\linux-port\docker
docker compose build game
docker compose up -d --force-recreate game
docker compose logs -f game
```

---

## 🔍 Logi i diagnostyka AI

Główne logi silnika gry znajdują się w kontenerze `game` w ścieżce `/opt/metin2/var/channel1/game1/syslog`.

### Przydatne polecenia diagnostyczne:

```sh
# Śledzenie decyzji AI botów na żywo
docker exec metin2-game tail -f /opt/metin2/var/channel1/game1/syslog | grep "PLAYERBOT_AI"

# Weryfikacja załadowania siatki kolizji NavGrid 2D
docker exec metin2-game grep "PLAYERBOT_NAVGRID" /opt/metin2/var/channel1/game1/syslog

# Podgląd celów wybieranych przez boty
docker exec metin2-game grep "target acquired" /opt/metin2/var/channel1/game1/syslog

# Sprawdzenie ewentualnych błędów (syserr)
docker exec metin2-game cat /opt/metin2/var/channel1/game1/syserr
```

---

## 🖥️ Panel Webowy (Live Map & Admin)

Kod panelu znajduje się w pliku `files/admin_panel.py`.

Przebudowa i restart panelu po edycji:
```powershell
Set-Location .\linux-port\docker
docker compose build panel
docker compose up -d --force-recreate panel
```

Panel dostępny jest pod adresem: `http://127.0.0.1:7788`.

---

## 💾 Kopia bazy i diagnostyka konia

Przed większą zmianą AI zatrzymaj procesy zapisujące stan i wykonaj sprawdzoną, skompresowaną kopię całej bazy:

```sh
docker compose stop game panel
./backup_database.sh /bezpieczna/sciezka/database-all.sql.gz
docker compose start
```

Hasło bazy nie jest wypisywane ani przechowywane w skrypcie — polecenie odczytuje je wyłącznie wewnątrz kontenera `metin2-db`. Skrypt sprawdza również integralność powstałego pliku gzip.

Pełną, tylko-do-odczytu diagnostykę podróży po Medal Konny uruchomisz poleceniem:

```sh
./diagnose_horse_journeys.sh
```

Raport pokazuje rozmieszczenie botów na M1/M2/M3 i w Lochu Małp, posiadane medale, poziomy konia oraz utrwalone znaczniki zebrania i oddania medalu.
