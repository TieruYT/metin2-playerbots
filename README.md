# ⚔️ Metin2 Playerbots

**Polski** | [English (README_EN.md)](README_EN.md)

Lokalny świat Metin2 singleplayer, w którym po mapie biegają i autentycznie grają autonomiczne postacie (Playerbots): zdobywają poziomy, walczą solo i w party, zbierają łup, ulepszają ekwipunek u Kowala, polują na Metiny i zapisują swój postęp w standardowej bazie danych.

W przeciwieństwie do tradycyjnych botów-klientów, boty w tym projekcie są **pełnoprawnymi bytami PC sterowanymi przez AI bezpośrednio wewnątrz silnika serwera (`game core`)**. Oznacza to, że zwykły gracz po wejściu do gry widzi ich naturalny ruch, animacje ataków, skille oraz ekwipunek przez standardowy protokół gry.

---

## 🌟 Możliwości botów

- ⚔️ **Inteligentna walka**: Obsługa wszystkich klas (Wojownik, Sura BM/WP, Ninja Dagger/Archer, Szaman), płynne animacje kombosów, ataki z łuku z uwzględnieniem strzał, utrzymywanie buffów i rotacje skilli.
- 🗺️ **Globalna nawigacja 2D NavGrid (A*)**: Własna siatka kolizji generowana z atrybutów mapy (`server_attr`) oraz algorytm A* z wygładzaniem tras (*String Pulling*). Boty sprawnie omijają góry, rzeki i mury miejskie.
- 💎 **Polowanie na Metiny**: Dedykowana rola łowców Metinów patrolujących całą mapę, niszczących kamienie i czyszczących fale potworów.
- 🎒 **Loot i ekonomia miejska**: Zbieranie Yang i przedmiotów po walce, automatyczne ubieranie lepszego ekwipunku, regularne powroty do miasta, sprzedaż zapchanego plecaka u Handlarki, uzupełnianie mikstur i ulepszanie u **Kowala**.
- 👥 **Grupy i Party**: Dynamiczne tworzenie 2–3 osobowych drużyn, formacje bojowe i wspólne expienie w gęstych obozach potworów.
- 💾 **Trwały zapis w bazie**: Każdy bot posiada własne konto i postać w bazie MariaDB — zachowuje poziom, przedmioty, Yang i postępy po restarcie serwera.

---

## 📊 Status projektu

Projekt jest w fazie aktywnego rozwoju.

> [!NOTE]
> **Obsługiwane Królestwo:** Obecnie pełna siatka nawigacyjna NavGrid 2D, rozkład 32 hubów expienia oraz punkty Metinów zostały wdrożone dla **Królestwa Chunjo (Żółci / Mapa 21 – Joan)**. Obsługa kolejnych królestw (*Shinsoo – Czerwoni* oraz *Jinno – Niebiescy*) jest zaplanowana w dalszych etapach.

### Zużycie zasobów (Snapshot dla 350 botów)
- **Serwer gry (`game core`)**: ~1.05 GiB RAM
- **Baza danych (`MariaDB`)**: ~154 MiB RAM
- **Panel Webowy Live Map**: ~383 MiB RAM
- Całość bez problemu działa lokalnie w tle na maszynie deweloperskiej.

---

## 🚀 Szybki start (Quickstart)

### 1. Klonowanie i instalacja (Windows)
```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1
```

### 2. Uruchomienie serwera
```powershell
Set-Location .\linux-port\docker
docker compose up -d
```

### 3. Wejście do gry
Skonfiguruj dowolnego klienta zgodnego z r40250 na adres `127.0.0.1` (port Auth `11000`, porty gry `13000–13002`) i ciesz się tętniącym życiem światem w Chunjo!

👉 **Szczegółowy przewodnik instalacji, konfiguracji `.env` i klienta znajdziesz w: [docs/INSTALL.md](docs/INSTALL.md)**

---

## 🎮 Komendy w grze (GM Commands)

Zarządzanie botami bezpośrednio z poziomu czatu w grze (dla konta GM / Administratora):

| Komenda | Uprawnienia | Opis | Przykład |
|---|---|---|---|
| `/bot_spawn <id> <królestwo: 1-3>` | GM | Ręczny spawn konkretnego bota o danym ID (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | GM | Usunięcie / wylogowanie bota ze świata. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <ilość> <królestwo>` | GM | Masowe zespawnowanie grupy botów. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <ilość>` | GM | Masowe wylogowanie grupy botów. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Wszyscy | Wyświetla na czacie aktualny ranking poziomów aktywnych botów. | `/bot_rank` |

---

## 🗺️ Roadmapa rozwoju

- [x] **Faza 1**: Pełne animacje wszystkich klas, łucznicy z pociskami, siatka 2D NavGrid (A*), ulepszanie u Kowala i 32 huby expienia w Chunjo.
- [ ] **Faza 2**: Prywatne tobołki/sklepy botów w miastach, handel między botami i dynamiczna wycena przedmiotów.
- [ ] **Faza 3**: Przejścia między mapami (M2, Dolina Orków, Pustynia) oraz wyprawy na **Wieżę Demonów (DT)**.
- [ ] **Faza 4**: Czytanie Ksiąg Umiejętności (KU) i Kamieni Duchowych (KD), zaawansowane buildy skilli.
- [ ] **Faza 5**: Misje Biologa (od zębów orka wzwyż) oraz pełny cykl rozwoju konia (zwykły, bojowy, militarny).
- [ ] **Faza 6**: Aktywności poboczne: łowienie ryb, kilof i wydobywanie rud z żył alchemii.

---

## 📚 Dokumentacja projektu

Szczegółowe informacje podzielone na dedykowane poradniki:

- 📖 **[Instalacja i Konfiguracja (docs/INSTALL.md)](docs/INSTALL.md)** – Docker, WSL2, Linux, `.env`, podłączanie klienta.
- 💻 **[Przewodnik Deweloperski (docs/DEVELOPMENT.md)](docs/DEVELOPMENT.md)** – Szybka kompilacja C++ w 8s (`fast-game-build`), debugowanie i logi AI.

---

## 🤝 Podziękowania i Credits

- [AzzlackSyndicate/metin2-singleplayer-serverfiles-linux](https://github.com/AzzlackSyndicate/metin2-singleplayer-serverfiles-linux) — Baza linuksowego portu serwera, instalatory i panel webowy.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Inspiracja badawcza dla autonomicznych agentów w grach MMO.
- Społeczność badaczy i entuzjastów platformy Metin2.
