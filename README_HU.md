# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | **Magyar** | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Website](https://img.shields.io/badge/Weboldal-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Csatlakozz_a_k%C3%B6z%C3%B6ss%C3%A9ghez-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-T%C3%A1mogasd_a_projektet-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licenc-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.hu)

Egy helyi, egyjátékos Metin2-világ, amelyben önálló karakterek (Playerbotok) járják a térképeket, és tényleg játszanak: szintet lépnek, egyedül és csapatban harcolnak, kereskednek egymással, fejlesztik a felszerelésüket, Metin-köveket és bossokat vadásznak, céheket alapítanak, és az előrehaladásukat a szerver adatbázisában őrzik.

A botok nem külső programok. Teljes értékű karakterek, amelyeket a szervermotoron belül egy MI irányít, így a játékos pontosan úgy látja a mozgásukat, a harcukat, a képességeiket és a felszerelésüket, mint más játékosok karaktereit.

A játékkliens lengyel, angol, német, francia, spanyol, olasz, portugál, román és török nyelven érhető el. A játék felületének magyar változata közösségi fordításként készül ([metin2-singleplayer-translations](https://github.com/TieruYT/metin2-singleplayer-translations)); addig angolul játszhatsz.

> [!IMPORTANT]
> **A 2.2.17-es verziótól a projekt forráskódja nem jelenik meg ebben a tárolóban.** Itt találod a kiadásokat a frissítőcsomagjaikkal, a változásnaplót ([CHANGELOG.md](CHANGELOG.md)), valamint azokat a fájlokat, amelyeket a launcher és a panelek olvasnak. A 2.2.16-os verzióig (azt is beleértve) a kiadások MIT-licenc alatt jelentek meg, és ezek előzményei a tárolóban maradnak. Az újabb verziók a [CC BY-NC-ND 4.0](LICENSE) licenc alatt érhetők el.

## 📥 Hogyan játssz

1. Töltsd le a **teljes csomagot** (szerver és kliens egy archívumban) a projekt Discord-szerveréről: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Telepítsd a Docker Desktopot, és várd meg, amíg megjelenik az "Engine running" felirat.
3. Csomagold ki a csomagot egy közönséges mappába, pl. `C:\Games\Metin2 Singleplayer` (ne az Asztalra vagy a OneDrive által szinkronizált Dokumentumok mappába), és indítsd el a `Serwer\Metin2-Launcher-GUI.bat` fájlt.
4. Amikor a launcher frissítést ajánl fel, válaszd az **Update everything** (Minden frissítése) lehetőséget: a teljes csomag régebbi a legújabb kiadásnál.
5. Kattints az **1. INSTALL / PREPARE** (Telepítés / előkészítés), majd a **2. PLAY** (Játék) gombra. Az első indításkor a szerver kész fájlokból áll össze a Dockerben, ez néhány percig tart; a későbbi indítások csak egy pillanatig.

> [!NOTE]
> **A 2.2.39-es verzió óta a szerver a védett változat.** Kész szerverfájlokként érkezik, forráskód nélkül, és semmit sem kell lefordítani a gépeden, így a telepítés és a frissítés kevesebb időt vesz igénybe. Egy régebbi, hagyományos telepítés a következő frissítéssel magától átáll a védett változatra (a launcher ezt a naplójában egy 10 másodperces visszaszámlálással jelzi), a világ, a karakterek és a beállítások pedig megmaradnak. A szerkeszthető játékfájlok (dropok, küldetések, térképek) mostantól a `Serwer\linux-port\docker\game\share\locale\poland\` mappában vannak, és a frissítések nem írják felül őket. Kivétel a Moonlight-láda: a sajátodat a `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` fájlba mentsd (másold le az ugyanabban a mappában lévő `share-add\special_item_group.moonlight.txt` fájlt, és tartsd meg a `Vnum 50011` sort).

A launcher induláskor magától felajánlja az új szerver- vagy kliensverziót, a **CHECK FOR UPDATES** (Frissítések keresése) gomb pedig bármikor rákeres. A csomagok [a tároló kiadásaiból](https://github.com/TieruYT/metin2-playerbots/releases) érkeznek. A Linuxon futó szervert a szerver mappájában a `sh linux-port/tools/update.sh` paranccsal frissítheted, a VPS-en futó szervert pedig a launcherből állíthatod be és frissítheted (**SERVER ON A VPS**). VPS-en is kész fájlokból áll össze a szerver, semmit sem kell lefordítani.

Hibát találtál? A launcher **REPORT A BUG** (Hiba jelentése) gombja a leírásodat a naplókkal együtt egyenesen nekünk küldi. Az ötleteket és javaslatokat a Discord javaslatcsatornájára küldd.

A követelményeket, egy lépésről lépésre haladó útmutatót és a gyakori kérdésekre adott válaszokat az [mt2sp.com](https://mt2sp.com) oldalon és a Discord segítségnyújtó csatornáin találod.

## 💬 Közösség és a projekt támogatása

- **[A projekt weboldala — mt2sp.com](https://mt2sp.com)**: mi ez a projekt, telepítési útmutató és GYIK, lengyel, angol, német és török nyelven.
- **[Discord-szerver](https://discord.gg/DyCxXdZtnV)**: a teljes csomag, segítség, hibajelentések, ötletek és hírek az új verziókról.
- **[Támogatás a buycoffee.to oldalon](https://buycoffee.to/metin2-playerbots)**: az önkéntes adományok segítenek fedezni az eszközöket, a tesztszervert és a projekt fejlesztéséhez használt MI-modelleket.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Támogatás a buycoffee.to oldalon"></a>

Minden támogatás segít egy élőbb Metin2-világot építeni: a tesztelés, a hibajelentések, az ötletek és az adományok is.

---

## 🌟 Mit csinálnak a botok

- ⚔️ **Úgy harcolnak, mint a játékosok**: minden kaszt és út (Harcos, Sura, Nindzsa, Sámán), kombók, képességrotációk, buffok, íjászok nyilakkal, és harci lóról széttört Metin-kövek. A botok a saját birodalmuk falvaiban egymással is gyakorlópárbajokat vívnak.
- 🗺️ **Három birodalom és az egész világ**: Shinsoo, Chunjo és Jinno a saját falvaikkal, az Ork-völgy, a Yongbi-sivatag, a Sohan-hegy, a Hwang-templom, a Tűzföld (Doyyumhwaji), az erdők, a Majom- és Pók-dungeonok, valamint a Száműzetés barlangjai. A botok a szintjük alapján választanak térképet és helyet, az útvonalukat pedig a térkép valódi ütközési rácsán tervezik meg.
- 🧠 **Személyiségek**: Iwakura személyiségrendszere hangulatokkal. Egy bot lehet Grinder, Hódító, Szerencsejátékos, Perfekcionista, Kereskedő, társ, zsoldos vagy Metin-kő-dropper, és időnként felbukkan egy ritka személyiség is.
- 🛡️ **Céhek, háborúk és raidek**: a botok erő szerint alapítanak céheket, körökre osztott céhháborúkat vívnak a céhtérképen (a játékosok céheivel is), céhként mennek a Démontoronyba, közösen döntik le a világ bossait, és leszállnak a Katakombákba Azrael után.
- 🎉 **Események**: Tanaka kalóz, Zuo Metin-esője, Hatszögletű ládák a Metin-kövekből és a "Night of the Pale Moon" (A Sápadt Hold éjszakája) Halloween-esemény, mind a panelből indítva. A botok maguktól odamennek harcolni.
- 🏰 **Dungeonok és kiegészítők**: a Pestis-dungeon (45-120. szint) és a Vadász Kovácsműhelye 50-es szintű fegyverekkel, mindkettő alapból ki van kapcsolva, és a panelben kapcsolható be, Beran-Setaou, valamint a Snakefield béta a rohamlóval.
- 🏪 **Botpiac**: valódi offline boltok a falusi standokon, Iwakura árlistájából származó árak inflációval, a kereslethez igazítva, és a botok minden birodalom pultjairól vásárolnak. A **Dom Towarowy** (áruház) egy ablakban mutatja az összes bolt ajánlatát, árat javasol, amikor kiteszel egy tárgyat ("Auto cena" — automatikus ár), és van kívánságlistája, amely szól, ha valaki kiteszi a tárgyat, amit keresel. A Metin-kövekből és bossokból eső Sárkányérmékkel fizetett **ItemShopban** a Szerencsekerék és villámaukciók is vannak.
- 🔨 **Karakterfejlődés**: a Kovács a "Refine up to +N" (Fejlesztés +N-ig) funkcióval és a gyűjtői raktárból vett alapanyagokkal, fejlesztőtekercsek, bónuszok, lélekkövek, Sárkánykő-alkímia, képességkönyvek, Kombó 30 pontig, Szellemkövek, a Biológus küldetései, küldetések, a ló egészen a harci lóig, horgászat, bányászat és gyógynövénygyűjtés.
- 💬 **Beszélgetések**: a botok válaszolnak a suttogásokra, és a chaten kereskednek ("Kupię…" — veszek, "Sprzedam…" — eladok), egy Sámán elmondja, mit adnak a buffjai, és a hívott bot odajön hozzád. A botok az olvasó nyelvén írnak: lengyelül, angolul, németül, franciául, portugálul, románul vagy törökül (minden más kliensnek angolul).
- 🤝 **Towarzysz, a társad**: állandó partner a csapatodban. Melletted harcol, buffol és kereskedik veled, a felszerelését és a képességeit pedig te magad állítod be. Saját raktára és alkímiája van, horgászik, csatlakozik egy barátod által vezetett csapathoz ("Grupa" — csapat), és maga is vezethet csapatot. Amíg távol vagy, egyedül játszik ("Gra beze mnie" — játék nélkülem): könyveket olvas, vásárol a piacon, fejleszt a Kovácsnál, intézi a Biológust és a küldetéseket, és beszámol, amikor visszatérsz. Úgy is játszhatsz vele, mint egy közönséges karakterrel.
- 🎯 **Auto Hunt**: automatikus vadászat a játékosnak, felvételi szűrővel és horgász móddal. A világ beállításai döntik el, hogy mindenki használhatja-e, vagy az ItemShopban kell megvenni.
- 🖥️ **A játékkliens**: kilenc nyelv (lengyel, angol, német, francia, spanyol, olasz, portugál, román és török), játékon belüli Wiki dropokkal, ládákkal és útmutatókkal, a Közösség ablak, GF-stílusú küldetésablak és [AUTO]/[AFK] jelölők a játékosok fölött.
- 🎛️ **Panelek és launcher**: két webes panel élő világtérképpel, ranglistákkal, a botok felszerelésével, MI-viselkedési csúszkákkal, időzített eseményekkel, a bossok újraéledési idejével és adatbázis-szerkesztővel; launcher frissítésekkel, világmentésekkel, nehézségi beállítással, extra csatornákkal, COOP-pal (a barátaid a te világodban játszhatnak, ingyen) és VPS-en futó szerverrel.
- 💾 **Tartós világ**: minden botnak saját fiókja és karaktere van a MariaDB-ben, így a szintek, a tárgyak és a yang túlélik az újraindítást.

## 🆕 Nemrég hozzáadva (2.2.140–2.2.146 verziók)

- **Towarzysz** magától fejlődik, amíg távol vagy, tud csapatot vezetni, és teljesen új módon horgászik, csalikkal és a fogás összesítésével.
- **A Kombó** 30 pontos lett, és a három könyve szörnyekből esik.
- **A Dom Towarowy** kívánságlistát kapott, a botok pultjaira kerülő új tekercsek, fejlesztési alapanyagok és könyvek pedig először a játékosokra várnak.
- **A Kovács** automatikusan fejleszt a választott pluszig; a Közösség ablak, a küldetésablak és a Wiki gyógynövénygyűjtési útmutatói is újak.
- **A botok** gyakorlópárbajokat vívnak a falvakban, dropperként törik a Metin-köveket, értelmesebben választják meg, hol harcolnak, és tisztességesebben viselkednek a játékosok mellett.
- A változások teljes listája a [CHANGELOG.md](CHANGELOG.md) fájlban található.

## ⌨️ Billentyűk a játékban

| Billentyű | Mit csinál |
|---|---|
| `P` | a társ ablaka |
| `K` | Auto Hunt |
| `H` | súgó a billentyűk és parancsok listájával |
| `X` | a csatornaablak (`X` + `1`–`4` egyenesen a CH1–CH4 csatornára visz) |
| `` ` `` (tilde) | minden közeli tárgy felvétele |
| `F5` | tárgy keresése az offline boltokban |
| `F9` | a GM-panel (csak GM-karaktereknek) |
| `F1`–`F3` | a bejelentkezési képernyőn: mentett fiók az éppen látható oldalról (15 fiók 5 oldalon) |

## 🎮 Parancsok a játékban (GM)

| Parancs | Jogosultság | Leírás | Példa |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Adminisztrátor | Beviszi a játékba az adott azonosítójú botot (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Adminisztrátor | Kilépteti a botot a világból. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Adminisztrátor | Botok egy tartományát viszi be a játékba. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Adminisztrátor | Botok egy tartományát lépteti ki. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Mindenki | Megmutatja a chaten az aktív botok szint szerinti ranglistáját. | `/bot_rank` |

## 🗄️ Adatbázis-hozzáférés (Navicat, HeidiSQL, DBeaver)

A szerver adatbázisa egy konténerben futó MariaDB, amely **csak erről a
számítógépről** érhető el (`127.0.0.1`, `3306`-os port, vagy egy másik, ha a
`.env` fájlban be van állítva az `M2_DB_PUBLISH_PORT`). Új kapcsolat egy
adatbázis-kliensben: típus MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Fiók | Mire való | Jelszó |
|---|---|---|
| `root` | mindenre | `M2_DB_ROOT_PASSWORD` a szerver mappájában lévő `linux-port\docker\.env` fájlban |
| `metin2` | csak a játék adatbázisaihoz (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` ugyanabban a fájlban |

A leggyorsabb mód: a launcher **DATABASE LOGIN (NAVICAT)** gombja másolható
mezőkben mutatja a hostot, a portot és mindkét jelszót. A jelszavak az első
indításkor véletlenszerűen jönnek létre, "alapértelmezett" jelszó nincs. Ne
illeszd be őket a Discordra.

Ha az adatbázis-kliens azt válaszolja, hogy `1045 - Access denied for user 'root'@'172.18.0.1'`,
akkor az adatbázis más jelszóval jött létre, mint ami most a `.env` fájlban van.
Kattints a **REPAIR DATABASE ACCESS** (Adatbázis-hozzáférés javítása) gombra: a
launcher leállítja a szervert, és a `metin2` és `root` fiók jelszavát a `.env`
fájlban lévőkre állítja, a karakterek, a tárgyak és a botok pedig érintetlenek
maradnak. Ezután kattints a PLAY gombra, és jelentkezz be újra.

## 📜 Licenc

A 2.2.55-ös verziótól a projekt a **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.hu)** (Nevezd meg! - Ne add el! - Ne változtasd!) licenc alatt áll:

- **szabad** játszani, szervert futtatni magadnak és a barátaidnak, és változatlanul megosztani a hivatalos csomagokat, a szerző feltüntetésével és a projektre mutató hivatkozással;
- **nem szabad** módosított változatokat vagy különálló disztribúciókat terjeszteni; a projekthez készülhetnek kiegészítők, és ezek megoszthatók a projekt Discordján;
- **nem szabad** pénzt keresni vele: nem szabad eladni, nem szabad pénzt kérni egy szerverhez való hozzáférésért, és nem szabad tárgyakat árulni a játékban. Az önkéntes adományok egy szerver fenntartására, amelyek semmilyen előnyt nem adnak a játékban, valamint a játékmenet felvétele és streamelése megengedettek.

A 2.2.16-os verzióig (azt is beleértve) a verziók MIT-licenc alatt állnak ([LICENSE-MIT.txt](LICENSE-MIT.txt)), a 2.2.17–2.2.54 verziók pedig CC BY-NC-SA 4.0 alatt. A Metin2 az Ymir Interactive és a Webzen tulajdona, a szerverfájl-csomagok pedig a szerzőiké. A részletek a [LICENSE](LICENSE) és a [NOTICE.md](NOTICE.md) fájlban találhatók.

**Metin2-fájlok a csomagokban.** A projekt csomagjai (a teljes csomag, valamint a kliens- és szerverfrissítések) olyan fájlokat is tartalmaznak, amelyek nem a projekt szerzőinek munkái: a Metin2 klienst és játékadatokat, amelyek az Ymir Interactive és a Webzen tulajdonát képezik, valamint a szerverfájl-csomagok fájljait (r40250 – TMP4, mt2009 – Martysama). A projekt licence nem terjed ki rájuk, és semmilyen jogot nem ad rájuk. A csomagok változatlan megosztására vonatkozó engedély csak a projekt szerzőinek munkájára terjed ki.

### Eltávolítási kérelmek

Ha jogtulajdonos vagy (Ymir Interactive, Webzen, Gameforge, egy szerverfájl-csomag szerzője vagy bárki más), és szeretnéd, hogy valamit eltávolítsunk innen, [nyiss egy issue-t](https://github.com/TieruYT/metin2-playerbots/issues) ebben a tárolóban, vagy írj a [projekt Discordjára](https://discord.gg/DyCxXdZtnV). Elég megnevezni a fájlokat vagy a kiadásokat. Gyorsan, vita nélkül és hivatalos DMCA-értesítés nélkül eltávolítjuk őket, bár természetesen az is lehetséges.

## 🤝 Köszönetnyilvánítás

- **AzzlackSyndicate**: az eredeti Linux-port alapjainak, a telepítőknek és a panelnek a szerzője, amelyből a projekt kinőtt (MIT, lásd [NOTICE.md](NOTICE.md)).
- **Iwakura**: a botok személyiségrendszere, az árlista, a tárgyszintek, a boltok és céhek nevei, a botok becenevei és a Community Patchek.
- **seban latino**: a Metin2 Singleplayer Panel, a telepítés második panelje, élő térképpel, profilokkal, ranglistákkal és gazdasággal.
- **ĹŌŞƬĒĶ**: a bejelentkezési képernyő és a Discord Rich Presence, a nyelvi csomag, a suttogott beszélgetések a botokkal, a bot fölötti személyiségsor és a Szerencsekerék tárcsája.
- **Colide**: az Auto Hunt ablak, a Közösség ablak, a társ raktára, offline terve és horgászata, valamint az [AUTO]/[AFK] jelölők grafikája.
- **OskarPWA**: a botraktár ablaka, a képességikonok és az F9 GM-panel első verziója.
- **Tyrion**: egy adott tárgy keresése az offline boltokban, a Halloween-esemény, valamint a német, francia, portugál és román fordítás.
- **Uxìĕ [DSO]**: a Dom Towarowy, a Bátorság köpenye, amely az egész képernyőről magához húzza a szörnyeket, és az inventory oldalpanelje.
- **Gibon**: előnézet arról, mi van egy ládában, és mit dob egy szörny.
- **Kiciamol**: a célpont életereje a sávján (harcban a játékosé is), az F9 GM-panel, a játékon belüli Wiki útmutatókkal, a Szerencsekerék, a Hatszögletű ládák, a Kombó 30 pontig és a Snakefield béta.
- **NerrVoVy**: a Pestis-dungeon, a Vadász Kovácsműhelye, a GF-stílusú küldetésablak, az ItemShop villámaukciói, a célpont bónusz-előnézete és a birodalmi zászlók a chatben.
- **Said (saido_0)**: a török fordítás és a Gyógynövényész útja.
- **blasty**: a bónuszváltó.
- **Piciu713**: a fejlesztési esély a Kovács ablakában.
- **Kenny, Pabloo, Mur4s**: javítások a csapatos tárgyfelvételhez, a botok megbízásaihoz egy játékos csapatában, a medvés küldetéshez és a Teleportgyűrűhöz.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): kutatási inspiráció az MMO-játékok autonóm ágenseihez.
- A Discord-közösség: a tesztek, a hibajelentések és az ötletek, amelyekből a projekt nagy része született.
