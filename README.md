# ⚔️ Metin2 Playerbots

**Polski** | [English (README_EN.md)](README_EN.md)

[![Strona](https://img.shields.io/badge/Strona-metin2singleplayer.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://metin2singleplayer.com)
[![Discord](https://img.shields.io/badge/Discord-Dołącz_do_społeczności-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/pt5tvnrN6)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Postaw_kaw%C4%99-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)

Lokalny świat Metin2 singleplayer, w którym po mapach biegają i naprawdę grają autonomiczne postacie (Playerbots): zdobywają poziomy, walczą solo i w drużynach, handlują między sobą, ulepszają ekwipunek, polują na Metiny i bossów, zakładają gildie i zapisują swój postęp w bazie danych serwera.

Boty nie są zewnętrznymi programami. To pełnoprawne postacie sterowane przez AI wewnątrz silnika serwera, więc gracz widzi ich ruch, walkę, umiejętności i ekwipunek tak samo jak postacie innych graczy.

> [!IMPORTANT]
> **Od wersji 2.2.17 kod źródłowy projektu nie jest publikowany w tym repozytorium.** Znajdziesz tu wydania z paczkami aktualizacji, listę zmian ([CHANGELOG.md](CHANGELOG.md)) i pliki, z których korzystają launcher i panele. Wersje do 2.2.16 włącznie zostały wydane na licencji MIT i ich historia zostaje w repozytorium. Nowsze wersje są objęte [licencją autora](LICENSE).

## 📥 Jak zagrać

1. Pobierz **pełną paczkę** (serwer i klient w jednym archiwum) z serwera Discord projektu: [discord.gg/pt5tvnrN6](https://discord.gg/pt5tvnrN6).
2. Zainstaluj Docker Desktop i poczekaj, aż pokaże „Engine running”.
3. Rozpakuj paczkę do zwykłego folderu, np. `C:\Gry\Metin2 Singleplayer` (nie na Pulpit ani do Dokumentów synchronizowanych z OneDrive), i uruchom `Serwer\Metin2-Launcher-GUI.bat`.
4. Kliknij **1. ZAINSTALUJ / PRZYGOTUJ**, a potem **2. GRAJ**. Pierwszy start buduje serwer i trwa kilkanaście minut, kolejne trwają chwilę.

O nowej wersji serwera lub klienta launcher sam zapyta przy starcie. Możesz też sprawdzić ją przyciskiem **SPRAWDZ AKTUALIZACJE**. Paczki pobierają się z [wydań w tym repozytorium](https://github.com/TieruYT/metin2-playerbots/releases). Serwer na Linuksie aktualizujesz poleceniem `sh linux-port/tools/update.sh` w folderze serwera, a serwer na VPS możesz założyć i aktualizować z launchera (**SERWER NA VPS**).

Wymagania, instrukcja krok po kroku i odpowiedzi na częste pytania są na stronie [metin2singleplayer.com](https://metin2singleplayer.com) i na kanałach pomocy na Discordzie.

## 💬 Społeczność i wsparcie projektu

- **[Strona projektu — metin2singleplayer.com](https://metin2singleplayer.com)**: opis projektu, instrukcja instalacji i FAQ, po polsku i po angielsku.
- **[Serwer Discord](https://discord.gg/pt5tvnrN6)**: pełna paczka do pobrania, pomoc, zgłaszanie błędów, pomysły i informacje o nowych wersjach.
- **[Wsparcie na buycoffee.to](https://buycoffee.to/metin2-playerbots)**: dobrowolne wpłaty pomagają pokrywać koszty narzędzi, serwera testowego i modeli AI używanych przy rozwoju projektu.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Postaw kawę na buycoffee.to"></a>

Każda forma wsparcia pomaga tworzyć coraz bardziej żywy świat Metin2: testy, zgłoszenia błędów, pomysły i wpłaty.

---

## 🌟 Co potrafią boty

- ⚔️ **Walka jak gracze**: wszystkie klasy i ścieżki (Wojownik, Sura, Ninja, Szaman), kombosy, rotacje umiejętności, buffy, łucznicy ze strzałami i Metiny bite z konia bojowego.
- 🗺️ **Trzy królestwa i cały świat**: Shinsoo, Chunjo i Jinno z własnymi wioskami, Dolina Orków, Pustynia Yongbi, Góra Sohan, Świątynia Hwang, Ziemia Ognia, lasy oraz Lochy Małp i Pająków. Boty dobierają mapę i miejsce do swojego poziomu, a trasy liczą po prawdziwej siatce kolizji mapy.
- 🧠 **Osobowości**: system osobowości Iwakury z nastrojami. Boty bywają Grinderami, Zdobywcami, Hazardzistami, Perfekcjonistami, Handlarzami, towarzyszami i najemnikami, a czasem trafia się rzadka osobowość.
- 🛡️ **Gildie, wojny i Wieża Demonów**: boty zakładają gildie według siły, toczą wojny gildii na mapie gildii (także z gildiami graczy), ruszają całą gildią na Wieżę Demonów i wspólnie biją bossów świata.
- 🏪 **Rynek botów**: prawdziwe sklepy offline na straganach w wioskach, ceny z cennika Iwakury korygowane popytem i zakupy między botami.
- 🔨 **Rozwój postaci**: Kowal i zwoje, bonusy, kamienie duszy, księgi umiejętności i Kamienie Duchowe, misje Biologa, koń aż do bojowego, łowienie ryb, górnictwo i zielarstwo.
- 💬 **Rozmowy**: boty odpowiadają na szepty, handlują przez czat („Kupię…”, „Sprzedam…”), Szaman powie, co dają jego buffy, a zawołany bot przyjdzie.
- 🤝 **Towarzysz**: Twój stały kompan w drużynie. Walczy przy Tobie, buffuje, handluje z Tobą, a jego ekwipunek i umiejętności ustawiasz sam.
- 🎯 **Auto Łowy**: automatyczne polowanie dla gracza, bez wymagań i opłat.
- 🎛️ **Panele i launcher**: dwa panele WWW z mapą świata na żywo, rankingami, ekwipunkiem botów, suwakami zachowania AI i wydarzeniami czasowymi; launcher z aktualizacjami, kopią świata, poziomem trudności i serwerem na VPS.
- 💾 **Trwały świat**: każdy bot ma własne konto i postać w bazie MariaDB, więc poziom, przedmioty i yang zostają po restarcie.

## ⌨️ Skróty w grze

| Klawisz | Działanie |
|---|---|
| `P` | okno Towarzysza |
| `K` | Auto Łowy |
| `` ` `` (tylda) | podnieś wszystkie przedmioty w pobliżu |
| `F9` | panel GM (tylko postacie GM) |

## 🎮 Komendy w grze (GM)

| Komenda | Uprawnienia | Opis | Przykład |
|---|---|---|---|
| `/bot_spawn <id> <królestwo: 1-3>` | Administrator | Wprowadza do gry bota o danym ID (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrator | Wylogowuje bota ze świata. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <ilość> <królestwo>` | Administrator | Wprowadza do gry grupę botów. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <ilość>` | Administrator | Wylogowuje grupę botów. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Wszyscy | Pokazuje na czacie ranking poziomów aktywnych botów. | `/bot_rank` |

## 🗄️ Dostęp do bazy danych (Navicat, HeidiSQL, DBeaver)

Baza serwera to MariaDB w kontenerze, dostępna **tylko na tym komputerze**
(`127.0.0.1`, port `3306` albo inny, jeśli w `.env` ustawiono `M2_DB_PUBLISH_PORT`).
Nowe połączenie w kliencie bazy: typ MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Konto | Do czego | Hasło |
|---|---|---|
| `root` | wszystko | `M2_DB_ROOT_PASSWORD` w pliku `linux-port\docker\.env` w folderze serwera |
| `metin2` | tylko bazy gry (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` w tym samym pliku |

Najszybciej: w launcherze przycisk **DANE DO BAZY (NAVICAT)** pokazuje host,
port i oba hasła w polach do skopiowania. Hasła są losowane przy pierwszym
uruchomieniu i nie ma żadnego „domyślnego”. Nie wklejaj ich na Discordzie.

Jeśli klient bazy odpowiada `1045 - Access denied for user 'root'@'172.18.0.1'`,
baza została utworzona z innym hasłem niż to, które jest teraz w `.env`.
Kliknij **NAPRAW DOSTEP DO BAZY**: launcher zatrzyma serwer i ustawi konta
`metin2` i `root` na hasła z `.env`, a postacie, przedmioty i boty zostaną
nietknięte. Potem kliknij GRAJ i zaloguj się jeszcze raz.

## 📜 Licencja

- **Wolno** pobierać projekt z oficjalnych źródeł, grać w niego prywatnie i ze znajomymi, robić kopie na własny użytek, zmieniać ustawienia, modyfikować go na własny użytek oraz nagrywać i streamować rozgrywkę.
- **Nie wolno bez pisemnej zgody autora** rozpowszechniać paczek ani kodu z nich, sprzedawać projektu ani dostępu do serwera, który na nim działa, ani tworzyć i publikować przeróbek.
- Wersje do 2.2.16 włącznie są na licencji MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)).
- Metin2 należy do Ymir Interactive i Webzen, a pakiety plików serwerowych do ich autorów. Szczegóły są w [NOTICE.md](NOTICE.md).

Pełna treść: [LICENSE](LICENSE).

## 🤝 Podziękowania

- **AzzlackSyndicate**: autor pierwotnej bazy portu linuksowego, instalatorów i panelu, na których projekt powstał (licencja MIT, zob. [NOTICE.md](NOTICE.md)).
- **Iwakura**: system osobowości botów, cennik, tiery przedmiotów, nazwy sklepów i gildii, nicki botów i Community Patche.
- **seban latino**: Metin2 Singleplayer Panel, drugi panel w instalacji, z mapą na żywo, profilami, rankingami i gospodarką.
- **ĹŌŞƬĒĶ**: ekran logowania i Discord Rich Presence, pakiet językowy, rozmowy z botami przez szepty i wiersz osobowości nad botem.
- **Colide**: okno Auto Łowów.
- **OskarPWA**: okno magazynu bota, ikony umiejętności i panel GM pod F9.
- **SIZOWSKI**: projekt dynamicznego podziału botów między kanały.
- **Tyrion**: wyszukiwanie konkretnego przedmiotu w sklepach offline.
- **Kenny, Pabloo, Mur4s**: poprawki podnoszenia przedmiotów w drużynie, zadań botów w drużynie gracza, questu niedźwiedzi i Pierścienia Teleportacji.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspiracja do badań nad autonomicznymi agentami w grach MMO.
- Społeczność Discorda: testy, zgłoszenia błędów i pomysły, z których powstała większość tego projektu.
