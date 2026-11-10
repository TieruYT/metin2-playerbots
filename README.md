# ⚔️ Metin2 Playerbots

**Polski** | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Strona](https://img.shields.io/badge/Strona-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Dołącz_do_społeczności-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Postaw_kaw%C4%99-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licencja](https://img.shields.io/badge/Licencja-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.pl)

Lokalny świat Metin2 singleplayer, w którym po mapach biegają i naprawdę grają autonomiczne postacie (Playerbots): zdobywają poziomy, walczą solo i w drużynach, handlują między sobą, ulepszają ekwipunek, polują na Metiny i bossów, zakładają gildie i zapisują swój postęp w bazie danych serwera.

Boty nie są zewnętrznymi programami. To pełnoprawne postacie sterowane przez AI wewnątrz silnika serwera, więc gracz widzi ich ruch, walkę, umiejętności i ekwipunek tak samo jak postacie innych graczy.

> [!IMPORTANT]
> **Od wersji 2.2.17 kod źródłowy projektu nie jest publikowany w tym repozytorium.** Znajdziesz tu wydania z paczkami aktualizacji, listę zmian ([CHANGELOG.md](CHANGELOG.md)) i pliki, z których korzystają launcher i panele. Wersje do 2.2.16 włącznie zostały wydane na licencji MIT i ich historia zostaje w repozytorium. Nowsze wersje są udostępniane na licencji [CC BY-NC-ND 4.0](LICENSE).

## 📥 Jak zagrać

1. Pobierz **pełną paczkę** (serwer i klient w jednym archiwum) z serwera Discord projektu: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Zainstaluj Docker Desktop i poczekaj, aż pokaże „Engine running”.
3. Rozpakuj paczkę do zwykłego folderu, np. `C:\Gry\Metin2 Singleplayer` (nie na Pulpit ani do Dokumentów synchronizowanych z OneDrive), i uruchom `Serwer\Metin2-Launcher-GUI.bat`.
4. Gdy launcher zaproponuje aktualizację, wybierz **Aktualizuj wszystko**: pełna paczka jest starsza niż najnowsze wydanie.
5. Kliknij **1. ZAINSTALUJ / PRZYGOTUJ**, a potem **2. GRAJ**. Pierwszy start składa serwer w Dockerze z gotowych plików i trwa kilka minut, kolejne trwają chwilę.

> [!NOTE]
> **Od wersji 2.2.39 serwer jest w wersji chronionej.** Przychodzi jako gotowe pliki serwera, bez kodu źródłowego i bez kompilacji na Twoim komputerze, więc instalacja i aktualizacje trwają krócej. Starsza, zwykła instalacja przechodzi na wersję chronioną sama przy najbliższej aktualizacji (launcher mówi o tym w logu z 10-sekundowym odliczaniem), a świat, postacie i ustawienia zostają. Pliki gry, które możesz edytować (dropy, questy, mapy), są teraz w `Serwer\linux-port\docker\game\share\locale\poland\` i aktualizacje ich nie nadpisują. Wyjątek to Szkatułka Blasku Księżyca: własną zapisz w `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (skopiuj `share-add\special_item_group.moonlight.txt` z tego folderu i zostaw linię `Vnum 50011`).

O nowej wersji serwera lub klienta launcher sam zapyta przy starcie. Możesz też sprawdzić ją przyciskiem **SPRAWDZ AKTUALIZACJE**. Paczki pobierają się z [wydań w tym repozytorium](https://github.com/TieruYT/metin2-playerbots/releases). Serwer na Linuksie aktualizujesz poleceniem `sh linux-port/tools/update.sh` w folderze serwera, a serwer na VPS możesz założyć i aktualizować z launchera (**SERWER NA VPS**). Na VPS-ie serwer też składa się z gotowych plików, bez kompilacji.

Błąd? Przycisk **ZGŁOŚ BŁĄD** w launcherze wysyła Twój opis razem z logami prosto do nas. Pomysły i propozycje zgłaszaj na Discordzie w kanale propozycji.

Wymagania, instrukcja krok po kroku i odpowiedzi na częste pytania są na stronie [mt2sp.com](https://mt2sp.com) i na kanałach pomocy na Discordzie.

## 💬 Społeczność i wsparcie projektu

- **[Strona projektu — mt2sp.com](https://mt2sp.com)**: opis projektu, instrukcja instalacji i FAQ, po polsku, angielsku, niemiecku i turecku.
- **[Serwer Discord](https://discord.gg/DyCxXdZtnV)**: pełna paczka do pobrania, pomoc, zgłaszanie błędów, pomysły i informacje o nowych wersjach.
- **[Wsparcie na buycoffee.to](https://buycoffee.to/metin2-playerbots)**: dobrowolne wpłaty pomagają pokrywać koszty narzędzi, serwera testowego i modeli AI używanych przy rozwoju projektu.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Postaw kawę na buycoffee.to"></a>

Każda forma wsparcia pomaga tworzyć coraz bardziej żywy świat Metin2: testy, zgłoszenia błędów, pomysły i wpłaty.

---

## 🌟 Co potrafią boty

- ⚔️ **Walka jak gracze**: wszystkie klasy i ścieżki (Wojownik, Sura, Ninja, Szaman), kombosy, rotacje umiejętności, buffy, łucznicy ze strzałami i Metiny bite z konia bojowego. Boty robią też sparingi w wioskach swojego królestwa.
- 🗺️ **Trzy królestwa i cały świat**: Shinsoo, Chunjo i Jinno z własnymi wioskami, Dolina Orków, Pustynia Yongbi, Góra Sohan, Świątynia Hwang, Ziemia Ognia, lasy, Lochy Małp i Pająków oraz Groty Wygnańców. Boty dobierają mapę i miejsce do swojego poziomu, a trasy liczą po prawdziwej siatce kolizji mapy.
- 🧠 **Osobowości**: system osobowości Iwakury z nastrojami. Boty bywają Grinderami, Zdobywcami, Hazardzistami, Perfekcjonistami, Handlarzami, towarzyszami, najemnikami i dropkami kamieni Metin, a czasem trafia się rzadka osobowość.
- 🛡️ **Gildie, wojny i rajdy**: boty zakładają gildie według siły, toczą wojny gildii na rundy na mapie gildii (także z gildiami graczy), ruszają całą gildią na Wieżę Demonów, wspólnie biją bossów świata i schodzą do Katakumb po Azraela.
- 🎉 **Wydarzenia**: pirat Tanaka, deszcz Metinów Zuo, Heksagonalne Skrzynie z Metinów i event Halloween „Noc Bladego Księżyca”, uruchamiane z panelu. Boty same ruszają na nie do walki.
- 🏰 **Lochy i dodatki**: Loch Zarazy (poziomy 45-120) i Kuźnia Łowcy z broniami na 50 poziom, domyślnie wyłączone i włączane w panelu, Beran-Setaou oraz beta Pola Węży z koniem szturmowym.
- 🏪 **Rynek botów**: prawdziwe sklepy offline na straganach w wioskach, ceny z cennika Iwakury z inflacją, korygowane popytem, i zakupy między botami z lad wszystkich królestw. **Dom Towarowy** pokazuje w jednym oknie oferty wszystkich sklepów, podpowiada cenę przy wystawianiu („Auto cena”) i ma listę życzeń, która daje znać, gdy ktoś wystawi szukany przedmiot. W **ItemShopie** za Smocze Monety (wypadają z Metinów i bossów) są też Koło Fortuny i aukcje błyskawiczne.
- 🔨 **Rozwój postaci**: Kowal z opcją „Ulepszaj do +N” i materiałami z magazynu kolekcjonera, zwoje, bonusy, kamienie duszy, Alchemia Smoczych Kamieni, księgi umiejętności, Sztuka Combo do 30 punktów, Kamienie Duchowe, misje Biologa, questy, koń aż do bojowego, łowienie ryb, górnictwo i zielarstwo.
- 💬 **Rozmowy**: boty odpowiadają na szepty, handlują przez czat („Kupię…”, „Sprzedam…”), Szaman powie, co dają jego buffy, a zawołany bot przyjdzie. Boty piszą w języku czytającego: po polsku, angielsku, niemiecku, francusku, portugalsku, rumuńsku i turecku (w pozostałych klientach po angielsku).
- 🤝 **Towarzysz**: Twój stały kompan w drużynie. Walczy przy Tobie, buffuje, handluje z Tobą, a jego ekwipunek i umiejętności ustawiasz sam. Ma własny magazyn i Alchemię, łowi ryby, dołącza do grupy prowadzonej przez znajomego („Grupa”) i sam może prowadzić grupę. Gdy Ciebie nie ma, gra sam („Gra beze mnie”): czyta księgi, kupuje na rynku, ulepsza u kowala, robi Biologa i questy, a po Twoim powrocie zdaje raport. Możesz też zagrać nim jak zwykłą postacią.
- 🎯 **Auto Łowy**: automatyczne polowanie dla gracza, z filtrem podnoszenia przedmiotów i trybem łowienia ryb. W ustawieniach świata wybierasz, czy jest dla każdego, czy po zakupie w ItemShopie.
- 🖥️ **Klient gry**: dziewięć języków (polski, angielski, niemiecki, francuski, hiszpański, włoski, portugalski, rumuński i turecki), Wiki w grze z dropem, skrzyniami i poradnikami, okno Społeczności, okno misji w stylu GF i znaczniki [AUTO]/[AFK] nad graczami.
- 🎛️ **Panele i launcher**: dwa panele WWW z mapą świata na żywo, rankingami, ekwipunkiem botów, suwakami zachowania AI, wydarzeniami czasowymi, czasami odrodzeń bossów i edytorem bazy; launcher z aktualizacjami, kopią świata, poziomem trudności, kolejnymi kanałami, COOP (gra ze znajomymi w Twoim świecie, za darmo) i serwerem na VPS.
- 💾 **Trwały świat**: każdy bot ma własne konto i postać w bazie MariaDB, więc poziom, przedmioty i yang zostają po restarcie.

## 🆕 Ostatnio w grze (wersje 2.2.140–2.2.146)

- **Towarzysz** rozwija się sam pod Twoją nieobecność, może prowadzić grupę i łowi ryby od nowa, z przynętami i podsumowaniem połowu.
- **Sztuka Combo** ma 30 punktów, a jej trzy księgi wypadają z potworów.
- **Dom Towarowy** ma listę życzeń, a nowe zwoje, ulepszacze i księgi na ladach botów czekają najpierw na graczy.
- **Kowal** ulepsza automatycznie do wybranego plusa; nowe są też okno Społeczności, okno misji i poradniki zielarstwa w Wiki.
- **Boty** robią sparingi w wioskach, rozbijają Metiny jako dropkowie, rozsądniej wybierają, gdzie walczyć, i uczciwiej zachowują się obok graczy.
- Pełna lista zmian jest w [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Skróty w grze

| Klawisz | Działanie |
|---|---|
| `P` | okno Towarzysza |
| `K` | Auto Łowy |
| `H` | pomoc z listą skrótów i komend |
| `X` | okno kanałów (`X` + `1`–`4` przenosi od razu na CH1–CH4) |
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

**Pliki Metin2 w paczkach.** Paczki projektu (pełna paczka i aktualizacje klienta i serwera) zawierają też pliki, które nie są pracą autorów projektu: klienta i dane gry Metin2, należące do Ymir Interactive i Webzen, oraz pliki pakietów serwerowych (r40250 autorstwa TMP4, mt2009 Martysama). Licencja projektu ich nie obejmuje i nie daje do nich żadnych praw. Zgoda na udostępnianie paczek bez zmian dotyczy tylko pracy autorów projektu.

### Prośby o usunięcie

Jeśli jesteś właścicielem praw (Ymir Interactive, Webzen, Gameforge, autor pakietu plików serwerowych lub ktokolwiek inny) i chcesz, żeby coś stąd zniknęło, [załóż zgłoszenie](https://github.com/TieruYT/metin2-playerbots/issues) w tym repozytorium albo napisz na [Discordzie projektu](https://discord.gg/DyCxXdZtnV). Wystarczy wskazać pliki albo wydania. Usuniemy je niezwłocznie, bez dyskusji i bez potrzeby formalnego zgłoszenia DMCA, choć oczywiście i ono jest możliwe.

## 🤝 Podziękowania

- **AzzlackSyndicate**: autor pierwotnej bazy portu linuksowego, instalatorów i panelu, na których projekt powstał (licencja MIT, zob. [NOTICE.md](NOTICE.md)).
- **Iwakura**: system osobowości botów, cennik, tiery przedmiotów, nazwy sklepów i gildii, nicki botów i Community Patche.
- **seban latino**: Metin2 Singleplayer Panel, drugi panel w instalacji, z mapą na żywo, profilami, rankingami i gospodarką.
- **ĹŌŞƬĒĶ**: ekran logowania i Discord Rich Presence, pakiet językowy, rozmowy z botami przez szepty, wiersz osobowości nad botem i tarcza Koła Fortuny.
- **Colide**: okno Auto Łowów, okno Społeczności, magazyn, plan offline i łowienie Towarzysza oraz grafika znaczników [AUTO]/[AFK].
- **OskarPWA**: okno magazynu bota, ikony umiejętności i pierwsza wersja panelu GM pod F9.
- **Tyrion**: wyszukiwanie konkretnego przedmiotu w sklepach offline, event Halloween i tłumaczenia na niemiecki, francuski, portugalski i rumuński.
- **Uxìĕ [DSO]**: Dom Towarowy, Peleryna Męstwa przyciągająca potwory z całego ekranu i panel boczny ekwipunku.
- **Gibon**: podgląd zawartości skrzyń i dropu potworów.
- **Kiciamol**: życie celu na pasku (także gracza w walce), panel GM pod F9, Wiki w grze z poradnikami, Koło Fortuny, Heksagonalne Skrzynie, Sztuka Combo do 30 punktów i beta Pola Węży.
- **NerrVoVy**: Loch Zarazy, Kuźnia Łowcy, okno misji w stylu GF, aukcje błyskawiczne w ItemShopie, podgląd bonusów celu i flagi królestw na czacie.
- **Said (saido_0)**: tłumaczenie na turecki i Ścieżka Zielarza.
- **blasty**: przełącznik bonusów.
- **Piciu713**: szansa ulepszenia w oknie Kowala.
- **Kenny, Pabloo, Mur4s**: poprawki podnoszenia przedmiotów w drużynie, zadań botów w drużynie gracza, questu niedźwiedzi i Pierścienia Teleportacji.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspiracja do badań nad autonomicznymi agentami w grach MMO.
- Społeczność Discorda: testy, zgłoszenia błędów i pomysły, z których powstała większość tego projektu.
