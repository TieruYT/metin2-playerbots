# ⚔️ Metin2 Playerbots

**Polski** | [English (README_EN.md)](README_EN.md)

Lokalny świat Metin2 singleplayer, w którym po mapie biegają i autentycznie grają autonomiczne postacie (Playerbots): zdobywają poziomy, walczą solo i w party, zbierają łup, ulepszają ekwipunek u Kowala, polują na Metiny i zapisują swój postęp w standardowej bazie danych.

## 💬 Społeczność i wsparcie projektu

- **[Dołącz do serwera Discord](https://discord.gg/6v4WkDY6a)** — porozmawiaj o projekcie, podziel się testami i pomysłami oraz śledź aktualności z rozwoju botów.
- **[Wesprzyj rozwój na Zrzutka.pl](https://zrzutka.pl/rw4g7p)** — dobrowolne wpłaty pomagają pokrywać koszty narzędzi i modeli AI wykorzystywanych podczas rozwijania projektu.

Każda forma wsparcia — testy, zgłoszenia błędów, propozycje, kod lub wpłata — pomaga nam tworzyć coraz bardziej samodzielny i żywy świat Metin2.

> [!IMPORTANT]
> Projekt działa wyłącznie z **natywnym klientem Windows**. Nie zawiera ani nie pobiera automatycznie plików Metin2, pakietu r40250 lub wycofanego WebClienta. Do instalacji potrzebujesz własnej zgodnej kopii plików. Zobacz [pochodzenie projektu i atrybucję](docs/ATTRIBUTION.md).

W przeciwieństwie do tradycyjnych botów-klientów, boty w tym projekcie są **pełnoprawnymi bytami PC sterowanymi przez AI bezpośrednio wewnątrz silnika serwera (`game core`)**. Oznacza to, że zwykły gracz po wejściu do gry widzi ich naturalny ruch, animacje ataków, skille oraz ekwipunek przez standardowy protokół gry.

---

## 🌟 Możliwości botów

- ⚔️ **Inteligentna walka**: Obsługa wszystkich klas (Wojownik, Sura BM/WP, Ninja Dagger/Archer, Szaman), płynne animacje kombosów, ataki z łuku z uwzględnieniem strzał, utrzymywanie buffów i rotacje skilli.
- 🗺️ **Globalna nawigacja 2D NavGrid (A*)**: Własna siatka kolizji generowana z atrybutów mapy (`server_attr`) oraz algorytm A* z wygładzaniem tras (*String Pulling*). Boty sprawnie omijają góry, rzeki i mury miejskie.
- 🚪 **Podróże między mapami**: Autonomiczne przejścia przez portale M1 ↔ M2 ↔ M3 oraz połączenia wewnątrz Łatwego Lochu Małp. Boty dobierają strefę do poziomu i jakości ekwipunku.
- 💎 **Polowanie na Metiny**: Dedykowana rola łowców Metinów patrolujących całą mapę, niszczących kamienie i czyszczących fale potworów.
- 🎒 **Loot i ekonomia miejska**: Zbieranie Yang i przedmiotów po walce, automatyczne ubieranie lepszego ekwipunku wraz z tarczami, regularne powroty do odpowiednich handlarzy, uzupełnianie mikstur i ulepszanie u **Kowala**.
- 🐴 **Rozwój konia**: Wyprawy po prawdziwe Medale Konne do Lochu Małp, oddawanie ich najbliższemu Stajennemu i używanie konia do długich podróży.
- 👥 **Grupy i Party**: Dynamiczne tworzenie 2–3 osobowych drużyn, formacje bojowe i wspólne expienie w gęstych obozach potworów.
- 🏪 **Stragany i rynek bot–bot**: Boty otwierają prywatne stragany w Bokjung i **kupują od siebie nawzajem** — ulepszacze, których akurat komuś brakuje, i sprzęt lepszy od noszonego. Przedmiot na +7 lub wyżej nigdy nie trafia do handlarza NPC.
- 🧬 **Misje Biologa**: Zbieranie okazów i oddawanie ich Biologowi, etapami, bez okna dialogowego questa.
- 🎣 **Łowienie ryb**: Pełna sesja z przynętą w gnieździe wędki, czekaniem na branie i wyciąganiem w oknie 6 sekund.
- ✨ **Przerzucanie bonusów**: Boty używają Kamieni Zmiany i Dodania Bonusu na sprzęcie, którego akurat nie mają założonego.
- 🧠 **Osobowość i cele**: Każdy bot ma własny charakter i ambicję (łowca Metinów, kolekcjoner, hodowca konia), które decydują, co robi w danej godzinie.
- 💾 **Trwały zapis w bazie**: Każdy bot posiada własne konto i postać w bazie MariaDB — zachowuje poziom, przedmioty, Yang i postępy po restarcie serwera.

---

## 📊 Status projektu

Projekt jest w fazie aktywnego rozwoju.

> [!NOTE]
> **Obsługiwane Królestwo:** Obecnie autonomiczny świat obejmuje **Chunjo**: Joan (M1, mapa 21), Bokjung (M2, mapa 23), Waryong/M3 (mapa 24), Łatwy Loch Małp (mapa 25, poziomy 18–26), **Dolinę Orków** (mapa 64) oraz **Pustynię Yongbi** (mapa 63). Obsługa kolejnych regionów Chunjo i królestw (*Shinsoo – Czerwoni* oraz *Jinno – Niebiescy*) jest zaplanowana w dalszych etapach.
>
> W Dolinie Orków boty poruszają się po głównym, spójnym lądzie. Wyspy połączone mostami pozostają poza zasięgiem: pokłady mostów są elementem klienta i nie ma ich w warstwie kolizji serwera, po której boty wyznaczają trasę.

### Zużycie zasobów (pomiar przy 742 żywych botach)
- **Serwer gry (`game core`)**: ~1.76 GiB RAM, ~32% jednego rdzenia
- **Baza danych (`MariaDB`)**: ~89 MiB RAM
- **Panel Webowy Live Map**: ~270 MiB RAM
- Całość bez problemu działa lokalnie w tle na maszynie deweloperskiej.
- Liczbę botów ustawia `PLAYERBOT_AUTOSPAWN_COUNT`, ale sufitem jest liczba kanonicznych tożsamości w bazie (`BOT_COUNT` w `generate_seed.py`), a nie sam suwak.

---

## 🚀 Szybki start (Quickstart)

### 1. Przygotowanie plików

Przygotuj lokalnie zgodne archiwum serwera r40250. Opcjonalnie przygotuj także archiwum natywnego klienta Windows, najlepiej z dokładnie tego samego wydania. Sama etykieta „r40250” nie gwarantuje zgodności protokołu i plików proto. Żaden z tych plików nie może być publikowany w tym repozytorium.

### 2. Klonowanie i instalacja (Windows)
```powershell
git clone https://github.com/TieruYT/metin2-playerbots.git
Set-Location .\metin2-playerbots
& .\installer\install.ps1 `
    -Archive 'C:\ścieżka\Reference_Server.zip' `
    -ClientArchive 'C:\ścieżka\Reference_Client.zip' `
    -NoWebClient
```

Jeśli masz już skonfigurowanego klienta, użyj zamiast `-ClientArchive` przełącznika `-NoClient`.

### 3. Uruchomienie serwera po instalacji
```powershell
Set-Location "$env:USERPROFILE\Metin2Server"
docker compose up -d
```

### 4. Wejście do gry
Skonfiguruj klienta z tego samego kompatybilnego zestawu r40250 na adres `127.0.0.1` (port Auth `11000`, porty gry `13000–13002`) i ciesz się tętniącym życiem światem w Chunjo!

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
- [x] **Faza 2A**: Przejścia M1/M2/M3, expienie strefowe, Łatwy Loch Małp i rzeczywiste wyprawy po Medale Konne.
- [ ] **Faza 2B**: Dolina Orków ✅ i Pustynia Yongbi ✅ — pozostają wyprawy na **Wieżę Demonów (DT)**.
- [ ] **Faza 3**: Czytanie Ksiąg Umiejętności (KU) i Kamieni Duchowych (KD), zaawansowane buildy skilli.
- [x] **Faza 4A**: Podstawowe misje Biologa i pierwszy etap konia oparty na prawdziwym dropie Medali Konnych.
- [ ] **Faza 4B** *(następne w kolejce)*: Zęby Orka i późniejsze misje Biologa oraz koń bojowy i militarny. Boty mają zachować takie same odstępy czasowe jak gracz i korzystać z eliksiru skracającego oczekiwanie, z tą samą szansą powodzenia co w questach.
- [ ] **Faza 5**: Łowienie ryb ✅ — pozostają kilof, wydobywanie rud i alchemia.
- [x] **Faza 6**: Prywatne stragany botów w mieście i handel bot–bot. Wycena opiera się na razie na stałych progach, nie na popycie.

---

## 📚 Dokumentacja projektu

Szczegółowe informacje podzielone na dedykowane poradniki:

- 📖 **[Instalacja i Konfiguracja (docs/INSTALL.md)](docs/INSTALL.md)** – Docker, WSL2, Linux, `.env`, podłączanie klienta.
- 💻 **[Przewodnik Deweloperski (docs/DEVELOPMENT.md)](docs/DEVELOPMENT.md)** – Szybka kompilacja C++ w 8s (`fast-game-build`), debugowanie i logi AI.
- 🧩 **[Architektura i refaktoryzacja (docs/ARCHITECTURE.md)](docs/ARCHITECTURE.md)** – granice modułów oraz bezpieczny plan dzielenia AI na testowalne części.
- 🧾 **[Pochodzenie i atrybucja (docs/ATTRIBUTION.md)](docs/ATTRIBUTION.md)** – historia forka, granice licencji i status WebClienta.

---

## 🤝 Podziękowania i Credits

- **AzzlackSyndicate** — autor pierwotnej bazy linuksowego portu, instalatorów i panelu. Repozytorium źródłowe jest obecnie prywatne; zachowujemy historię Git i pełną atrybucję.
- **OskarPWA** — okno magazynu bota i ikony umiejętności na stronie pochodzą z panelu, który zbudował i udostępnił do przeniesienia.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — Inspiracja badawcza dla autonomicznych agentów w grach MMO.
- Społeczność badaczy i entuzjastów platformy Metin2.
