# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | **Nederlands** | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Website](https://img.shields.io/badge/Website-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Word_lid_van_de_community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Steun_het_project-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licentie-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.nl)

Een lokale Metin2-singleplayerwereld waarin zelfstandige personages (Playerbots) over de kaarten trekken en echt spelen: ze levelen, vechten solo en in groepen, handelen met elkaar, upgraden hun uitrusting, jagen op Metin-stenen en bosses, richten gildes op en bewaren hun voortgang in de database van de server.

De bots zijn geen externe programma's. Het zijn volwaardige personages die binnen de server-engine door AI worden aangestuurd, dus een speler ziet hun bewegingen, gevechten, skills en uitrusting precies zoals hij de personages van andere spelers ziet.

De gameclient is beschikbaar in het Pools, Engels, Duits, Frans, Spaans, Italiaans, Portugees, Roemeens en Turks. Een Nederlandse versie van de game-interface wordt voorbereid als communityvertaling ([metin2-singleplayer-translations](https://github.com/TieruYT/metin2-singleplayer-translations)); tot die tijd kun je in het Engels spelen.

> [!IMPORTANT]
> **Vanaf versie 2.2.17 wordt de broncode van het project niet in deze repository gepubliceerd.** Hier vind je de releases met hun updatepakketten, de changelog ([CHANGELOG.md](CHANGELOG.md)) en de bestanden die de launcher en de panelen lezen. Versies tot en met 2.2.16 zijn uitgebracht onder de MIT-licentie en hun geschiedenis blijft in de repository. Nieuwere versies worden gedeeld onder de [CC BY-NC-ND 4.0](LICENSE)-licentie.

## 📥 Zo speel je

1. Download het **volledige pakket** (server en client in één archief) van de Discord-server van het project: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Installeer Docker Desktop en wacht tot het "Engine running" laat zien.
3. Pak het pakket uit in een gewone map, bijv. `C:\Games\Metin2 Singleplayer` (niet op het Bureaublad of in Documenten die door OneDrive worden gesynchroniseerd), en start `Serwer\Metin2-Launcher-GUI.bat`.
4. Als de launcher een update aanbiedt, kies dan **Update everything** (alles bijwerken): het volledige pakket is ouder dan de nieuwste release.
5. Klik op **1. INSTALL / PREPARE** (installeren / voorbereiden) en daarna op **2. PLAY** (spelen). Bij de eerste start wordt de server in Docker opgebouwd uit kant-en-klare bestanden, wat een paar minuten duurt; latere starts duren maar even.

> [!NOTE]
> **Sinds versie 2.2.39 is de server de beschermde versie.** Die wordt geleverd als kant-en-klare serverbestanden, zonder broncode en zonder dat er iets op je pc wordt gecompileerd, dus installeren en updaten kost minder tijd. Een oudere, gewone installatie stapt bij de volgende update vanzelf over op de beschermde versie (de launcher meldt dat in zijn log met een aftelling van 10 seconden), en de wereld, personages en instellingen blijven behouden. De gamebestanden die je kunt bewerken (drops, quests, kaarten) staan nu in `Serwer\linux-port\docker\game\share\locale\poland\` en updates overschrijven ze niet. De Moonlight-kist is de uitzondering: sla je eigen versie op in `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (kopieer `share-add\special_item_group.moonlight.txt` uit die map en behoud de regel `Vnum 50011`).

De launcher biedt bij het starten vanzelf een nieuwe server- of clientversie aan, en **CHECK FOR UPDATES** (controleren op updates) zoekt er op elk moment naar. De pakketten komen uit [de releases in deze repository](https://github.com/TieruYT/metin2-playerbots/releases). Een server op Linux werk je bij met `sh linux-port/tools/update.sh` in de servermap, en een server op een VPS kun je vanuit de launcher opzetten en bijwerken (**SERVER ON A VPS**). Ook op een VPS wordt de server opgebouwd uit kant-en-klare bestanden, zonder dat er iets wordt gecompileerd.

Een bug gevonden? De knop **REPORT A BUG** (bug melden) in de launcher stuurt je beschrijving samen met de logs rechtstreeks naar ons. Ideeën en suggesties stuur je naar het voorstellenkanaal op Discord.

Systeemvereisten, een stapsgewijze handleiding en antwoorden op veelgestelde vragen vind je op [mt2sp.com](https://mt2sp.com) en in de helpkanalen op Discord.

## 💬 Community & steun voor het project

- **[Website van het project — mt2sp.com](https://mt2sp.com)**: wat het project is, de installatiehandleiding en de FAQ, in het Pools, Engels, Duits en Turks.
- **[Discord-server](https://discord.gg/DyCxXdZtnV)**: het volledige pakket, hulp, bugmeldingen, ideeën en nieuws over nieuwe versies.
- **[Steun via buycoffee.to](https://buycoffee.to/metin2-playerbots)**: vrijwillige donaties helpen de kosten te dekken van de tools, de testserver en de AI-modellen die bij de ontwikkeling van het project worden gebruikt.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Steun via buycoffee.to"></a>

Elke vorm van steun helpt om een levendigere Metin2-wereld te bouwen: testen, bugmeldingen, ideeën en donaties.

---

## 🌟 Wat de bots doen

- ⚔️ **Ze vechten als spelers**: elke klasse en elk pad (Krijger, Sura, Ninja, Sjamaan), combo's, skillrotaties, buffs, boogschutters met pijlen en Metin-stenen die vanaf een strijdpaard worden kapotgeslagen. Bots houden ook oefengevechten met elkaar in de dorpen van hun koninkrijk.
- 🗺️ **Drie koninkrijken en de hele wereld**: Shinsoo, Chunjo en Jinno met hun eigen dorpen, het Orkdal, de Yongbi-woestijn, de berg Sohan, de Hwang-tempel, het Vuurland (Doyyumhwaji), de bossen, de Apen- en Spinnenkerkers en de Grotten van Ballingschap. Bots kiezen de kaart en de plek op basis van hun level en plannen hun routes over het echte botsingsraster van de kaart.
- 🧠 **Persoonlijkheden**: het persoonlijkheidssysteem van Iwakura, met stemmingen. Een bot kan een Grinder, een Veroveraar, een Gokker, een Perfectionist, een Handelaar, een metgezel, een huurling of een Metin-stenendropper zijn, en af en toe duikt er een zeldzame persoonlijkheid op.
- 🛡️ **Gildes, oorlogen en raids**: bots richten gildes op naar sterkte, voeren gildeoorlogen in rondes op de gildekaart (ook tegen gildes van spelers), gaan als gilde naar de Demonentoren, verslaan samen de bosses van de wereld en dalen af in de Catacomben, op jacht naar Azrael.
- 🎉 **Events**: Piraat Tanaka, Zuo's Metin-regen, Zeshoekige kisten uit Metin-stenen en het Halloween-event "Night of the Pale Moon" (Nacht van de Bleke Maan), allemaal te starten vanuit het panel. De bots gaan er uit zichzelf op af om te vechten.
- 🏰 **Kerkers en add-ons**: de Pestkerker (levels 45-120) en de Smidse van de Jager met wapens voor level 50, beide standaard uitgeschakeld en in het panel in te schakelen, Beran-Setaou, en de Snakefield-bèta met het aanvalspaard.
- 🏪 **Een botmarkt**: echte offline winkels op de marktkramen in de dorpen, prijzen uit de prijslijst van Iwakura met inflatie, bijgesteld naar de vraag, en bots die kopen van de toonbanken van elk koninkrijk. Het **Dom Towarowy** (warenhuis) toont de aanbiedingen van alle winkels in één venster, stelt een prijs voor als je een item te koop zet ("Auto cena" — automatische prijs) en heeft een verlanglijst die je laat weten wanneer iemand het item te koop zet dat je zoekt. De **ItemShop**, waar je betaalt met Drakenmunten die uit Metin-stenen en bosses vallen, heeft ook het Rad van Fortuin en flitsveilingen.
- 🔨 **Karakterontwikkeling**: de Smid met "Refine up to +N" (upgraden tot +N) en materialen uit het verzamelaarsmagazijn, upgradeperkamenten, bonussen, zielenstenen, Drakensteenalchemie, skillboeken, Combo tot 30 punten, Geeststenen, de missies van de Bioloog, quests, het paard tot en met het strijdpaard, vissen, mijnbouw en kruidenkunde.
- 💬 **Gesprekken**: bots beantwoorden fluisterberichten en handelen via de chat ("Kupię…" — ik koop, "Sprzedam…" — ik verkoop), een Sjamaan vertelt je wat zijn buffs geven en een bot die je roept, komt naar je toe. De bots schrijven in de taal van de lezer: Pools, Engels, Duits, Frans, Portugees, Roemeens of Turks (Engels voor elke andere client).
- 🤝 **Towarzysz, je metgezel**: een vaste partner in je groep. Hij vecht naast je, buft je en handelt met je, en jij stelt zelf zijn uitrusting en skills in. Hij heeft zijn eigen opslag en Alchemie, vist, sluit zich aan bij een groep die een vriend leidt ("Grupa" — groep) en kan zelf een groep leiden. Terwijl je weg bent, speelt hij op eigen houtje ("Gra beze mnie" — spelen zonder mij): hij leest boeken, winkelt op de markt, upgradet bij de Smid, doet de Bioloog en quests en brengt verslag uit als je terugkomt. Je kunt hem ook als een gewoon personage spelen.
- 🎯 **Auto Hunt**: automatisch jagen voor de speler, met een opraapfilter en een vismodus. De instellingen van de wereld bepalen of het voor iedereen is of in de ItemShop wordt gekocht.
- 🖥️ **De gameclient**: negen talen (Pools, Engels, Duits, Frans, Spaans, Italiaans, Portugees, Roemeens en Turks), een in-game Wiki met drops, kisten en handleidingen, het Community-venster, een questvenster in GF-stijl en [AUTO]/[AFK]-markeringen boven spelers.
- 🎛️ **Panelen en launcher**: twee webpanelen met een live wereldkaart, ranglijsten, de uitrusting van de bots, schuifregelaars voor het AI-gedrag, getimede events, respawntijden van bosses en een database-editor; een launcher met updates, back-ups van de wereld, de moeilijkheidsinstelling, extra kanalen, COOP (vrienden die in jouw wereld spelen, gratis) en een server op een VPS.
- 💾 **Een blijvende wereld**: elke bot heeft zijn eigen account en personage in MariaDB, dus levels, items en yang overleven een herstart.

## 🆕 Recent toegevoegd (versies 2.2.140–2.2.146)

- **Towarzysz** ontwikkelt zich vanzelf terwijl je weg bent, kan een groep leiden en vist op een compleet nieuwe manier, met aas en een overzicht van de vangst.
- **Combo** heeft 30 punten, en de drie boeken ervan vallen uit monsters.
- **Dom Towarowy** heeft een verlanglijst, en nieuwe perkamenten, upgradematerialen en boeken op de toonbanken van de bots wachten eerst op spelers.
- **De Smid** upgradet automatisch tot de plus die je kiest; ook het Community-venster, het questvenster en de kruidenkundehandleidingen in de Wiki zijn nieuw.
- **Bots** houden oefengevechten in de dorpen, slaan als dropper Metin-stenen kapot, kiezen verstandiger waar ze vechten en gedragen zich eerlijker naast spelers.
- De volledige lijst met wijzigingen staat in [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Toetsen in het spel

| Toets | Wat het doet |
|---|---|
| `P` | het venster van de metgezel |
| `K` | Auto Hunt |
| `H` | hulp met de lijst van toetsen en commando's |
| `X` | het kanaalvenster (`X` + `1`–`4` brengt je direct naar CH1–CH4) |
| `` ` `` (tilde) | alle items in de buurt oprapen |
| `F5` | de offline winkels doorzoeken op een item |
| `F9` | het GM-panel (alleen voor GM-personages) |
| `F1`–`F3` | op het inlogscherm: een opgeslagen account van de getoonde pagina (15 accounts op 5 pagina's) |

## 🎮 Commando's in het spel (GM)

| Commando | Recht | Beschrijving | Voorbeeld |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Beheerder | Brengt de bot met dat ID in het spel (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Beheerder | Logt de bot uit de wereld. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Beheerder | Brengt een reeks bots in het spel. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Beheerder | Logt een reeks bots uit. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Iedereen | Toont in de chat de levelranglijst van de actieve bots. | `/bot_rank` |

## 🗄️ Toegang tot de database (Navicat, HeidiSQL, DBeaver)

De database van de server is MariaDB in een container, **alleen bereikbaar
vanaf deze computer** (`127.0.0.1`, poort `3306`, of een andere als
`M2_DB_PUBLISH_PORT` in `.env` is ingesteld). Een nieuwe verbinding in een
databaseclient: type MySQL/MariaDB, host `127.0.0.1`, poort `3306`.

| Account | Waarvoor | Wachtwoord |
|---|---|---|
| `root` | alles | `M2_DB_ROOT_PASSWORD` in `linux-port\docker\.env` in de servermap |
| `metin2` | alleen de databases van het spel (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` in hetzelfde bestand |

De snelste manier: de knop **DATABASE LOGIN (NAVICAT)** in de launcher toont de
host, de poort en beide wachtwoorden in velden om te kopiëren. De wachtwoorden
worden bij de eerste start willekeurig gekozen en er is geen "standaard"
wachtwoord. Plak ze niet op Discord.

Als de databaseclient antwoordt met `1045 - Access denied for user 'root'@'172.18.0.1'`,
dan is de database aangemaakt met een ander wachtwoord dan het wachtwoord dat nu in `.env` staat.
Klik op **REPAIR DATABASE ACCESS** (databasetoegang herstellen): de launcher stopt
de server en zet de accounts `metin2` en `root` op de wachtwoorden in `.env`, en
personages, items en bots blijven onaangeroerd. Klik daarna op PLAY en log opnieuw in.

## 📜 Licentie

Vanaf versie 2.2.55 valt het project onder **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.nl)** (Naamsvermelding-NietCommercieel-GeenAfgeleideWerken):

- je **mag** spelen, een server draaien voor jezelf en je vrienden, en de officiële pakketten ongewijzigd delen, met vermelding van de auteur en een link naar het project;
- je **mag geen** gewijzigde versies of afzonderlijke distributies verspreiden; add-ons voor het project mogen worden gemaakt en gedeeld op de Discord van het project;
- je **mag er geen** geld mee verdienen: niet verkopen, geen geld vragen voor toegang tot een server, geen items in het spel verkopen. Vrijwillige donaties voor het draaien van een server die geen voordeel in het spel opleveren, en het opnemen en streamen van gameplay, zijn toegestaan.

Versies tot en met 2.2.16 vallen onder de MIT-licentie ([LICENSE-MIT.txt](LICENSE-MIT.txt)), en versies 2.2.17 tot en met 2.2.54 onder CC BY-NC-SA 4.0. Metin2 is eigendom van Ymir Interactive en Webzen, en de serverbestandenpakketten zijn van hun auteurs. Details staan in [LICENSE](LICENSE) en [NOTICE.md](NOTICE.md).

**Metin2-bestanden in de pakketten.** De pakketten van het project (het volledige pakket en de client- en serverupdates) bevatten ook bestanden die niet het werk zijn van de auteurs van het project: de Metin2-client en -gamedata, die eigendom zijn van Ymir Interactive en Webzen, en de bestanden van de serverbestandenpakketten (r40250 van TMP4, mt2009 van Martysama). De licentie van het project is daar niet op van toepassing en geeft er geen rechten op. De toestemming om de pakketten ongewijzigd te delen geldt alleen voor het werk van de auteurs van het project.

### Verzoeken tot verwijdering

Als je rechthebbende bent (Ymir Interactive, Webzen, Gameforge, de auteur van een serverbestandenpakket of iemand anders) en wilt dat hier iets wordt verwijderd, [open dan een issue](https://github.com/TieruYT/metin2-playerbots/issues) in deze repository of schrijf op de [Discord van het project](https://discord.gg/DyCxXdZtnV). Het noemen van de bestanden of releases is genoeg. We verwijderen ze snel, zonder discussie en zonder dat een formele DMCA-melding nodig is, al kan dat natuurlijk ook.

## 🤝 Met dank aan

- **AzzlackSyndicate**: auteur van de oorspronkelijke basis van de Linux-port, de installers en het panel waaruit het project is gegroeid (MIT, zie [NOTICE.md](NOTICE.md)).
- **Iwakura**: het persoonlijkheidssysteem van de bots, de prijslijst, de itemtiers, de namen van winkels en gildes, de bijnamen van de bots en de Community Patches.
- **seban latino**: het Metin2 Singleplayer Panel, het tweede panel in de installatie, met zijn live kaart, profielen, ranglijsten en economie.
- **ĹŌŞƬĒĶ**: het inlogscherm en Discord Rich Presence, het taalpakket, gefluisterde gesprekken met de bots, de persoonlijkheidsregel boven een bot en de schijf van het Rad van Fortuin.
- **Colide**: het Auto Hunt-venster, het Community-venster, de opslag, het offlineplan en het vissen van de metgezel, en de artwork van de [AUTO]/[AFK]-markeringen.
- **OskarPWA**: het venster van het botdepot, de skillpictogrammen en de eerste versie van het F9-GM-panel.
- **Tyrion**: het doorzoeken van de offline winkels op één bepaald item, het Halloween-event en de Duitse, Franse, Portugese en Roemeense vertalingen.
- **Uxìĕ [DSO]**: het Dom Towarowy, de Mantel van Moed die monsters van het hele scherm aantrekt, en het zijpaneel van de inventaris.
- **Gibon**: voorbeelden van wat een kist bevat en wat een monster dropt.
- **Kiciamol**: de gezondheid van het doelwit op zijn balk (in een gevecht ook die van een speler), het F9-GM-panel, de in-game Wiki met handleidingen, het Rad van Fortuin, de Zeshoekige kisten, Combo tot 30 punten en de Snakefield-bèta.
- **NerrVoVy**: de Pestkerker, de Smidse van de Jager, het questvenster in GF-stijl, de flitsveilingen van de ItemShop, de bonusvoorvertoning van het doelwit en de koninkrijksvlaggen in de chat.
- **Said (saido_0)**: de Turkse vertaling en het Pad van de Kruidenkundige.
- **blasty**: de bonuswisselaar.
- **Piciu713**: de upgradekans in het venster van de Smid.
- **Kenny, Pabloo, Mur4s**: fixes voor het oprapen in groepen, voor de boodschappen van bots in de groep van een speler, voor de berenquest en voor de Teleportatiering.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): onderzoeksinspiratie voor autonome agenten in MMO-games.
- De Discord-community: de tests, bugmeldingen en ideeën waar het grootste deel van dit project uit is voortgekomen.
