# ⚔️ Metin2 Playerbots

**Polski** | [English (README_EN.md)](README_EN.md)

[![Strona](https://img.shields.io/badge/Strona-metin2singleplayer.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://metin2singleplayer.com)
[![Discord](https://img.shields.io/badge/Discord-Dołącz_do_społeczności-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Postaw_kaw%C4%99-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licencja](https://img.shields.io/badge/Licencja-CC_BY--NC--SA_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.pl)

Lokalny świat Metin2 singleplayer, w którym po mapach biegają i naprawdę grają autonomiczne postacie (Playerbots): zdobywają poziomy, walczą solo i w drużynach, handlują między sobą, ulepszają ekwipunek, polują na Metiny i bossów, zakładają gildie i zapisują swój postęp w bazie danych serwera.

Boty nie są zewnętrznymi programami. To pełnoprawne postacie sterowane przez AI wewnątrz silnika serwera, więc gracz widzi ich ruch, walkę, umiejętności i ekwipunek tak samo jak postacie innych graczy.

> [!IMPORTANT]
> **Od wersji 2.2.17 kod źródłowy projektu nie jest publikowany w tym repozytorium.** Znajdziesz tu wydania z paczkami aktualizacji, listę zmian ([CHANGELOG.md](CHANGELOG.md)) i pliki, z których korzystają launcher i panele. Wersje do 2.2.16 włącznie zostały wydane na licencji MIT i ich historia zostaje w repozytorium. Nowsze wersje są udostępniane na licencji [CC BY-NC-SA 4.0](LICENSE).

## 📥 Jak zagrać

1. Pobierz **pełną paczkę** (serwer i klient w jednym archiwum) z serwera Discord projektu: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Zainstaluj Docker Desktop i poczekaj, aż pokaże „Engine running”.
3. Rozpakuj paczkę do zwykłego folderu, np. `C:\Gry\Metin2 Singleplayer` (nie na Pulpit ani do Dokumentów synchronizowanych z OneDrive), i uruchom `Serwer\Metin2-Launcher-GUI.bat`.
4. Gdy launcher zaproponuje aktualizację, wybierz **Aktualizuj wszystko**: pełna paczka jest starsza niż najnowsze wydanie.
5. Kliknij **1. ZAINSTALUJ / PRZYGOTUJ**, a potem **2. GRAJ**. Pierwszy start składa serwer w Dockerze z gotowych plików i trwa kilka minut, kolejne trwają chwilę.

> [!NOTE]
> **Od wersji 2.2.39 serwer jest w wersji chronionej.** Przychodzi jako gotowe pliki serwera, bez kodu źródłowego i bez kompilacji na Twoim komputerze, więc instalacja i aktualizacje trwają krócej. Starsza, zwykła instalacja przechodzi na wersję chronioną sama przy najbliższej aktualizacji (launcher mówi o tym w logu z 10-sekundowym odliczaniem), a świat, postacie i ustawienia zostają. Pliki gry, które możesz edytować (dropy, questy, mapy), są teraz w `Serwer\linux-port\docker\game\share\locale\poland\` i aktualizacje ich nie nadpisują. Wyjątek to Szkatułka Blasku Księżyca: własną zapisz w `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (skopiuj `share-add\special_item_group.moonlight.txt` z tego folderu i zostaw linię `Vnum 50011`).

O nowej wersji serwera lub klienta launcher sam zapyta przy starcie. Możesz też sprawdzić ją przyciskiem **SPRAWDZ AKTUALIZACJE**. Paczki pobierają się z [wydań w tym repozytorium](https://github.com/TieruYT/metin2-playerbots/releases). Serwer na Linuksie aktualizujesz poleceniem `sh linux-port/tools/update.sh` w folderze serwera, a serwer na VPS możesz założyć i aktualizować z launchera (**SERWER NA VPS**). Na VPS-ie serwer też składa się z gotowych plików, bez kompilacji.

Błąd albo pomysł? Przycisk **ZGŁOŚ BŁĄD / POMYSŁ** w launcherze wysyła Twój opis razem z logami prosto do nas.

Wymagania, instrukcja krok po kroku i odpowiedzi na częste pytania są na stronie [metin2singleplayer.com](https://metin2singleplayer.com) i na kanałach pomocy na Discordzie.

## 💬 Społeczność i wsparcie projektu

- **[Strona projektu — metin2singleplayer.com](https://metin2singleplayer.com)**: opis projektu, instrukcja instalacji i FAQ, po polsku i po angielsku.
- **[Serwer Discord](https://discord.gg/DyCxXdZtnV)**: pełna paczka do pobrania, pomoc, zgłaszanie błędów, pomysły i informacje o nowych wersjach.
- **[Wsparcie na buycoffee.to](https://buycoffee.to/metin2-playerbots)**: dobrowolne wpłaty pomagają pokrywać koszty narzędzi, serwera testowego i modeli AI używanych przy rozwoju projektu.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Postaw kawę na buycoffee.to"></a>

Każda forma wsparcia pomaga tworzyć coraz bardziej żywy świat Metin2: testy, zgłoszenia błędów, pomysły i wpłaty.

---

## 🌟 Co potrafią boty

- ⚔️ **Walka jak gracze**: wszystkie klasy i ścieżki (Wojownik, Sura, Ninja, Szaman), kombosy, rotacje umiejętności, buffy, łucznicy ze strzałami i Metiny bite z konia bojowego.
- 🗺️ **Trzy królestwa i cały świat**: Shinsoo, Chunjo i Jinno z własnymi wioskami, Dolina Orków, Pustynia Yongbi, Góra Sohan, Świątynia Hwang, Ziemia Ognia, lasy oraz Lochy Małp i Pająków. Boty dobierają mapę i miejsce do swojego poziomu, a trasy liczą po prawdziwej siatce kolizji mapy.
- 🧠 **Osobowości**: system osobowości Iwakury z nastrojami. Boty bywają Grinderami, Zdobywcami, Hazardzistami, Perfekcjonistami, Handlarzami, towarzyszami i najemnikami, a czasem trafia się rzadka osobowość.
- 🛡️ **Gildie, wojny i rajdy**: boty zakładają gildie według siły, toczą wojny gildii na rundy na mapie gildii (także z gildiami graczy), ruszają całą gildią na Wieżę Demonów, wspólnie biją bossów świata i schodzą do Katakumb po Azraela.
- 🎉 **Wydarzenia**: pirat Tanaka i deszcz Metinów Zuo z ogłoszeniami i nagrodami, a boty same ruszają na nie do walki.
- 🏪 **Rynek botów**: prawdziwe sklepy offline na straganach w wioskach, ceny z cennika Iwakury z inflacją, korygowane popytem, i zakupy między botami. **Dom Towarowy** pokazuje w jednym oknie oferty wszystkich sklepów, a przy wystawianiu przedmiotu podpowiada cenę botów (z opcją „Auto cena”). W grze jest też **ItemShop** za Smocze Monety, które wypadają z Metinów i bossów.
- 🔨 **Rozwój postaci**: Kowal i zwoje, bonusy, kamienie duszy, księgi umiejętności i Kamienie Duchowe, misje Biologa, koń aż do bojowego, łowienie ryb, górnictwo i zielarstwo.
- 💬 **Rozmowy**: boty odpowiadają na szepty, handlują przez czat („Kupię…”, „Sprzedam…”), Szaman powie, co dają jego buffy, a zawołany bot przyjdzie.
- 🤝 **Towarzysz**: Twój stały kompan w drużynie. Walczy przy Tobie, buffuje, handluje z Tobą, a jego ekwipunek i umiejętności ustawiasz sam. Dołącza też do grupy prowadzonej przez znajomego („Grupa”) i może grać sam, gdy Ciebie nie ma („Gra beze mnie”).
- 🎯 **Auto Łowy**: automatyczne polowanie dla gracza. W ustawieniach świata wybierasz, czy jest dla każdego, czy po zakupie w ItemShopie.
- 🎛️ **Panele i launcher**: dwa panele WWW z mapą świata na żywo, rankingami, ekwipunkiem botów, suwakami zachowania AI i wydarzeniami czasowymi; launcher z aktualizacjami, kopią świata, poziomem trudności, drugim kanałem, COOP (gra ze znajomymi w Twoim świecie, za darmo) i serwerem na VPS.
- 💾 **Trwały świat**: każdy bot ma własne konto i postać w bazie MariaDB, więc poziom, przedmioty i yang zostają po restarcie.

## ⌨️ Skróty w grze

| Klawisz | Działanie |
|---|---|
| `P` | okno Towarzysza |
| `K` | Auto Łowy |
| `` ` `` (tylda) | podnieś wszystkie przedmioty w pobliżu |
| `F5` | wyszukiwarka przedmiotów w sklepach offline |
| `F9` | panel GM (tylko postacie GM) |
| `F1`–`F3` | na ekranie logowania: zapisane konto z widocznej strony (15 kont na 5 stronach) |

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

Od wersji 2.2.55 projekt jest na licencji **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.pl)** (Uznanie autorstwa - Użycie niekomercyjne - Bez utworów zależnych):

- **wolno** grać, prowadzić serwer dla siebie i znajomych oraz udostępniać oficjalne paczki bez zmian, z podaniem autora i linku do projektu;
- **nie wolno** rozpowszechniać przeróbek ani osobnych dystrybucji; dodatki do projektu można tworzyć i dzielić się nimi na Discordzie projektu;
- **nie wolno** na nim zarabiać: sprzedawać go, brać opłat za dostęp do serwera ani sprzedawać przedmiotów w grze. Dobrowolne wpłaty na utrzymanie serwera, jeśli nie dają korzyści w grze, oraz nagrywanie i streamowanie rozgrywki są w porządku.

Wersje do 2.2.16 włącznie są na licencji MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), a wersje od 2.2.17 do 2.2.54 na CC BY-NC-SA 4.0. Metin2 należy do Ymir Interactive i Webzen, a pakiety plików serwerowych do ich autorów. Szczegóły są w plikach [LICENSE](LICENSE) i [NOTICE.md](NOTICE.md).

## 🤝 Podziękowania

- **AzzlackSyndicate**: autor pierwotnej bazy portu linuksowego, instalatorów i panelu, na których projekt powstał (licencja MIT, zob. [NOTICE.md](NOTICE.md)).
- **Iwakura**: system osobowości botów, cennik, tiery przedmiotów, nazwy sklepów i gildii, nicki botów i Community Patche.
- **seban latino**: Metin2 Singleplayer Panel, drugi panel w instalacji, z mapą na żywo, profilami, rankingami i gospodarką.
- **ĹŌŞƬĒĶ**: ekran logowania i Discord Rich Presence, pakiet językowy, rozmowy z botami przez szepty i wiersz osobowości nad botem.
- **Colide**: okno Auto Łowów.
- **OskarPWA**: okno magazynu bota, ikony umiejętności i panel GM pod F9.
- **Tyrion**: wyszukiwanie konkretnego przedmiotu w sklepach offline.
- **Uxìĕ [DSO]**: Dom Towarowy i Peleryna Męstwa przyciągająca potwory z całego ekranu.
- **Gibon**: podgląd zawartości skrzyń i dropu potworów.
- **Kiciamol**: życie celu na pasku, także gracza w walce.
- **Piciu713**: szansa ulepszenia w oknie Kowala.
- **Kenny, Pabloo, Mur4s**: poprawki podnoszenia przedmiotów w drużynie, zadań botów w drużynie gracza, questu niedźwiedzi i Pierścienia Teleportacji.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspiracja do badań nad autonomicznymi agentami w grach MMO.
- Społeczność Discorda: testy, zgłoszenia błędów i pomysły, z których powstała większość tego projektu.
