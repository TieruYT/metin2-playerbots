# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | **Čeština** | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Website](https://img.shields.io/badge/Web-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-P%C5%99ipoj_se_ke_komunit%C4%9B-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Podpo%C5%99_projekt-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licence-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.cs)

Lokální singleplayerový svět Metin2, ve kterém po mapách běhají samostatné postavy (Playerboti) a opravdu hrají: získávají úrovně, bojují samy i ve skupinách, obchodují mezi sebou, vylepšují si vybavení, loví kameny Metin a bossy, zakládají cechy a svůj postup ukládají do databáze serveru.

Boti nejsou externí programy. Jsou to plnohodnotné postavy řízené umělou inteligencí přímo uvnitř serverového enginu, takže hráč vidí jejich pohyb, boj, dovednosti a vybavení přesně tak, jako vidí postavy ostatních hráčů.

Herní klient je dostupný v polštině, angličtině, němčině, francouzštině, španělštině, italštině, portugalštině, rumunštině a turečtině. Česká verze herního rozhraní se připravuje jako komunitní překlad ([metin2-singleplayer-translations](https://github.com/TieruYT/metin2-singleplayer-translations)); do té doby můžeš hrát v angličtině.

> [!IMPORTANT]
> **Od verze 2.2.17 se zdrojový kód projektu v tomto repozitáři nezveřejňuje.** Najdeš tu vydání s jejich aktualizačními balíčky, seznam změn ([CHANGELOG.md](CHANGELOG.md)) a soubory, které čte launcher a panely. Verze až do 2.2.16 včetně vyšly pod licencí MIT a jejich historie v repozitáři zůstává. Novější verze jsou sdíleny pod licencí [CC BY-NC-ND 4.0](LICENSE).

## 📥 Jak hrát

1. Stáhni si **plný balíček** (server a klient v jednom archivu) z Discord serveru projektu: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Nainstaluj Docker Desktop a počkej, až zobrazí "Engine running".
3. Rozbal balíček do obyčejné složky, např. `C:\Games\Metin2 Singleplayer` (ne na Plochu ani do Dokumentů synchronizovaných přes OneDrive), a spusť `Serwer\Metin2-Launcher-GUI.bat`.
4. Když launcher nabídne aktualizaci, zvol **Update everything** (Aktualizovat vše): plný balíček je starší než nejnovější vydání.
5. Klikni na **1. INSTALL / PREPARE** (Instalovat / připravit) a potom na **2. PLAY** (Hrát). První spuštění sestaví server v Dockeru z hotových souborů a trvá několik minut; další spuštění už jen chvilku.

> [!NOTE]
> **Od verze 2.2.39 je server chráněnou verzí.** Dodává se jako hotové serverové soubory, bez zdrojového kódu a bez kompilace na tvém PC, takže instalace i aktualizace zaberou méně času. Starší, běžná instalace přejde na chráněnou verzi sama s příští aktualizací (launcher to oznámí ve svém logu s 10sekundovým odpočtem) a svět, postavy i nastavení zůstanou. Herní soubory, které můžeš upravovat (dropy, questy, mapy), jsou nyní v `Serwer\linux-port\docker\game\share\locale\poland\` a aktualizace je nepřepisují. Výjimkou je truhla Moonlight: svou vlastní ulož do `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (zkopíruj `share-add\special_item_group.moonlight.txt` z téže složky a ponech řádek `Vnum 50011`).

Launcher sám nabídne novou verzi serveru nebo klienta, když se spustí, a **CHECK FOR UPDATES** (Zkontrolovat aktualizace) ji kdykoli vyhledá. Balíčky pocházejí z [vydání v tomto repozitáři](https://github.com/TieruYT/metin2-playerbots/releases). Server na Linuxu se aktualizuje příkazem `sh linux-port/tools/update.sh` ve složce serveru a server na VPS lze nastavit i aktualizovat z launcheru (**SERVER ON A VPS**). I na VPS se server sestavuje z hotových souborů, nic se nekompiluje.

Narazil jsi na chybu? Tlačítko **REPORT A BUG** (Nahlásit chybu) v launcheru nám pošle tvůj popis i s logy přímo k nám. Nápady a návrhy posílej do kanálu s návrhy na Discordu.

Požadavky, návod krok za krokem a odpovědi na časté otázky najdeš na [mt2sp.com](https://mt2sp.com) a v kanálech nápovědy na Discordu.

## 💬 Komunita a podpora projektu

- **[Web projektu — mt2sp.com](https://mt2sp.com)**: co je to za projekt, instalační návod a FAQ, v polštině, angličtině, němčině a turečtině.
- **[Discord server](https://discord.gg/DyCxXdZtnV)**: plný balíček, pomoc, hlášení chyb, nápady a novinky o nových verzích.
- **[Podpora na buycoffee.to](https://buycoffee.to/metin2-playerbots)**: dobrovolné dary pomáhají pokrýt nástroje, testovací server a AI modely, které se používají při vývoji projektu.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Podpora na buycoffee.to"></a>

Každá forma podpory pomáhá budovat živější svět Metin2: testování, hlášení chyb, nápady i dary.

---

## 🌟 Co boti dělají

- ⚔️ **Bojují jako hráči**: všechna povolání a cesty (Válečník, Sura, Ninja, Šaman), komba, rotace dovedností, buffy, lukostřelci se šípy a kameny Metin rozbíjené z bojového koně. Boti spolu také svádějí cvičné souboje ve vesnicích svého království.
- 🗺️ **Tři království a celý svět**: Shinsoo, Chunjo a Jinno s vlastními vesnicemi, Údolí orků, poušť Yongbi, hora Sohan, chrám Hwang, Ohnivá země (Doyyumhwaji), lesy, Opičí a Pavoučí dungeony a Jeskyně vyhnanství. Boti si vybírají mapu a místo podle své úrovně a trasy plánují po skutečné kolizní mřížce mapy.
- 🧠 **Osobnosti**: Iwakurův systém osobností s náladami. Bot může být Grinder, Dobyvatel, Hazardér, Perfekcionista, Obchodník, společník, žoldák nebo dropper kamenů Metin a tu a tam se objeví i vzácná osobnost.
- 🛡️ **Cechy, války a raidy**: boti zakládají cechy podle síly, vedou cechovní války na kola na cechovní mapě (i proti cechům hráčů), chodí jako cech do Démonské věže, společně porážejí bossy světa a sestupují do Katakomb za Azraelem.
- 🎉 **Eventy**: Pirát Tanaka, Zuův déšť Metinů, Šestiúhelníkové truhly z kamenů Metin a halloweenský event "Night of the Pale Moon" (Noc bledého měsíce), vše spouštěné z panelu. Boti tam sami vyrážejí bojovat.
- 🏰 **Dungeony a doplňky**: Morový dungeon (úrovně 45-120) a Lovcova kovárna se zbraněmi pro úroveň 50, obojí ve výchozím stavu vypnuté a zapínané v panelu, Beran-Setaou a beta Snakefield s útočným koněm.
- 🏪 **Trh botů**: skutečné offline obchody na stáncích ve vesnicích, ceny z Iwakurova ceníku s inflací, upravované podle poptávky, a boti nakupující z pultů všech království. **Dom Towarowy** (obchodní dům) ukazuje nabídky všech obchodů v jednom okně, při vystavení předmětu navrhne cenu ("Auto cena" — automatická cena) a má seznam přání, který ti dá vědět, když někdo vystaví předmět, který hledáš. **ItemShop**, kde se platí Dračími mincemi padajícími z kamenů Metin a bossů, má také Kolo štěstí a bleskové aukce.
- 🔨 **Rozvoj postavy**: Kovář s funkcí "Refine up to +N" (Vylepšit až na +N) a materiály branými ze sběratelského skladu, svitky vylepšení, bonusy, kameny duše, Alchymie dračích kamenů, knihy dovedností, Combo až do 30 bodů, Duchovní kameny, mise Biologa, questy, kůň až po bojového koně, rybaření, těžba a bylinkářství.
- 💬 **Konverzace**: boti odpovídají na šeptání a obchodují přes chat ("Kupię…" — koupím, "Sprzedam…" — prodám), Šaman ti řekne, co dávají jeho buffy, a bot, kterého zavoláš, přijde. Boti píšou v jazyce čtenáře: polsky, anglicky, německy, francouzsky, portugalsky, rumunsky nebo turecky (každému jinému klientovi anglicky).
- 🤝 **Towarzysz, tvůj společník**: stálý parťák ve tvé skupině. Bojuje po tvém boku, buffuje tě a obchoduje s tebou, a jeho vybavení i dovednosti si nastavuješ sám. Má vlastní sklad a Alchymii, rybaří, přidá se do skupiny, kterou vede kamarád ("Grupa" — skupina), a sám může skupinu vést. Když jsi pryč, hraje sám ("Gra beze mnie" — hra beze mě): čte knihy, nakupuje na trhu, vylepšuje u Kováře, plní Biologa a questy a po tvém návratu ti podá hlášení. Můžeš ho hrát i jako běžnou postavu.
- 🎯 **Auto Hunt**: automatický lov pro hráče s filtrem sbírání a rybářským režimem. Nastavení světa rozhoduje, zda je pro všechny, nebo se kupuje v ItemShopu.
- 🖥️ **Herní klient**: devět jazyků (polština, angličtina, němčina, francouzština, španělština, italština, portugalština, rumunština a turečtina), herní Wiki s dropy, truhlami a návody, okno Komunity, okno questů ve stylu GF a značky [AUTO]/[AFK] nad hráči.
- 🎛️ **Panely a launcher**: dva webové panely s živou mapou světa, žebříčky, vybavením botů, posuvníky chování AI, časovanými eventy, časy respawnu bossů a editorem databáze; launcher s aktualizacemi, zálohami světa, nastavením obtížnosti, extra kanály, COOP (přátelé hrající ve tvém světě, zdarma) a serverem na VPS.
- 💾 **Trvalý svět**: každý bot má vlastní účet a postavu v MariaDB, takže úrovně, předměty i yang přežijí restart.

## 🆕 Nedávno přidáno (verze 2.2.140–2.2.146)

- **Towarzysz** se sám rozvíjí, když jsi pryč, umí vést skupinu a rybaří úplně novým způsobem, s návnadami a souhrnem úlovku.
- **Combo** má 30 bodů a jeho tři knihy padají z monster.
- **Dom Towarowy** má seznam přání a nové svitky, materiály na vylepšení a knihy na pultech botů čekají nejdřív na hráče.
- **Kovář** vylepšuje automaticky až do zvoleného plusu; nové jsou také okno Komunity, okno questů a bylinkářské návody ve Wiki.
- **Boti** svádějí cvičné souboje ve vesnicích, rozbíjejí kameny Metin jako droppeři, rozumněji si vybírají, kde bojovat, a vedle hráčů se chovají férověji.
- Úplný seznam změn je v [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Klávesy ve hře

| Klávesa | Co dělá |
|---|---|
| `P` | okno společníka |
| `K` | Auto Hunt |
| `H` | nápověda se seznamem kláves a příkazů |
| `X` | okno kanálů (`X` + `1`–`4` tě přesune rovnou na CH1–CH4) |
| `` ` `` (tilde) | sebere všechny předměty poblíž |
| `F5` | vyhledá předmět v offline obchodech |
| `F9` | GM panel (jen pro GM postavy) |
| `F1`–`F3` | na přihlašovací obrazovce: uložený účet ze zobrazené stránky (15 účtů na 5 stránkách) |

## 🎮 Příkazy ve hře (GM)

| Příkaz | Oprávnění | Popis | Příklad |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Administrátor | Přivede bota s daným ID do hry (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrátor | Odhlásí bota ze světa. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Administrátor | Přivede do hry rozsah botů. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Administrátor | Odhlásí rozsah botů. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Všichni | Zobrazí v chatu žebříček úrovní aktivních botů. | `/bot_rank` |

## 🗄️ Přístup k databázi (Navicat, HeidiSQL, DBeaver)

Databáze serveru je MariaDB v kontejneru, dostupná **jen z tohoto
počítače** (`127.0.0.1`, port `3306`, nebo jiný, pokud je v `.env` nastaven
`M2_DB_PUBLISH_PORT`). Nové připojení v databázovém klientovi: typ
MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Účet | K čemu | Heslo |
|---|---|---|
| `root` | ke všemu | `M2_DB_ROOT_PASSWORD` v `linux-port\docker\.env` ve složce serveru |
| `metin2` | jen k herním databázím (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` ve stejném souboru |

Nejrychlejší cesta: tlačítko launcheru **DATABASE LOGIN (NAVICAT)** zobrazí
host, port a obě hesla v polích ke zkopírování. Hesla se náhodně vygenerují při
prvním spuštění a žádné "výchozí" heslo neexistuje. Nevkládej je na Discord.

Pokud databázový klient odpoví `1045 - Access denied for user 'root'@'172.18.0.1'`,
databáze byla vytvořena s jiným heslem, než jaké je teď v `.env`.
Klikni na **REPAIR DATABASE ACCESS** (Opravit přístup k databázi): launcher
zastaví server a nastaví účtům `metin2` a `root` hesla z `.env`, přičemž
postavy, předměty a boti zůstanou nedotčené. Pak klikni na PLAY a přihlas se znovu.

## 📜 Licence

Od verze 2.2.55 je projekt pod licencí **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.cs)** (Uveďte původ-Neužívejte komerčně-Nezpracovávejte):

- **smíš** hrát, provozovat server pro sebe a své přátele a sdílet oficiální balíčky beze změn, s uvedením autora a odkazem na projekt;
- **nesmíš** šířit upravené verze ani samostatné distribuce; doplňky k projektu se smějí vytvářet a sdílet na Discordu projektu;
- **nesmíš** na tom vydělávat: žádný prodej, žádné zpoplatnění přístupu na server, žádný prodej předmětů ve hře. Dobrovolné dary na provoz serveru, které nedávají žádnou výhodu ve hře, a nahrávání a streamování hraní jsou v pořádku.

Verze až do 2.2.16 včetně jsou pod licencí MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)) a verze 2.2.17 až 2.2.54 pod CC BY-NC-SA 4.0. Metin2 patří společnostem Ymir Interactive a Webzen a balíčky serverových souborů jejich autorům. Podrobnosti jsou v [LICENSE](LICENSE) a [NOTICE.md](NOTICE.md).

**Soubory Metin2 v balíčcích.** Balíčky projektu (plný balíček a aktualizace klienta a serveru) obsahují také soubory, které nejsou dílem autorů projektu: klienta a herní data Metin2, která patří společnostem Ymir Interactive a Webzen, a soubory balíčků serverových souborů (r40250 od TMP4, mt2009 od Martysama). Licence projektu se na ně nevztahuje a nedává k nim žádná práva. Povolení sdílet balíčky beze změn se vztahuje pouze na dílo autorů projektu.

### Žádosti o odstranění

Pokud jsi držitelem práv (Ymir Interactive, Webzen, Gameforge, autor balíčku serverových souborů nebo kdokoli jiný) a chceš, aby odsud bylo něco staženo, [založ issue](https://github.com/TieruYT/metin2-playerbots/issues) v tomto repozitáři nebo napiš na [Discord projektu](https://discord.gg/DyCxXdZtnV). Stačí uvést soubory nebo vydání. Odstraníme je rychle, bez diskuse a bez nutnosti formálního oznámení DMCA, i když i to je samozřejmě možné.

## 🤝 Poděkování

- **AzzlackSyndicate**: autor původního základu linuxového portu, instalátorů a panelu, ze kterých projekt vyrostl (MIT, viz [NOTICE.md](NOTICE.md)).
- **Iwakura**: systém osobností botů, ceník, tiery předmětů, názvy obchodů a cechů, přezdívky botů a Community Patche.
- **seban latino**: Metin2 Singleplayer Panel, druhý panel v instalaci, s živou mapou, profily, žebříčky a ekonomikou.
- **ĹŌŞƬĒĶ**: přihlašovací obrazovka a Discord Rich Presence, jazykový balíček, šeptané konverzace s boty, řádek osobnosti nad botem a disk Kola štěstí.
- **Colide**: okno Auto Hunt, okno Komunity, sklad, offline plán a rybaření společníka a grafika značek [AUTO]/[AFK].
- **OskarPWA**: okno skladu botů, ikony dovedností a první verze GM panelu F9.
- **Tyrion**: vyhledávání jednoho konkrétního předmětu v offline obchodech, halloweenský event a překlady do němčiny, francouzštiny, portugalštiny a rumunštiny.
- **Uxìĕ [DSO]**: Dom Towarowy, Plášť odvahy přitahující monstra z celé obrazovky a boční panel inventáře.
- **Gibon**: náhledy toho, co obsahuje truhla a co padá z monstra.
- **Kiciamol**: zdraví cíle na jeho liště (v boji i hráčovo), GM panel F9, herní Wiki s návody, Kolo štěstí, Šestiúhelníkové truhly, Combo až do 30 bodů a beta Snakefield.
- **NerrVoVy**: Morový dungeon, Lovcova kovárna, okno questů ve stylu GF, bleskové aukce ItemShopu, náhled bonusů cíle a vlajky království v chatu.
- **Said (saido_0)**: turecký překlad a Cesta bylinkáře.
- **blasty**: přepínač bonusů.
- **Piciu713**: šance na vylepšení v okně Kováře.
- **Kenny, Pabloo, Mur4s**: opravy sbírání předmětů ve skupině, pochůzek botů ve skupině hráče, questu s medvědem a Teleportačního prstenu.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): výzkumná inspirace pro autonomní agenty v MMO hrách.
- Komunita na Discordu: testy, hlášení chyb a nápady, ze kterých vzešla většina tohoto projektu.
