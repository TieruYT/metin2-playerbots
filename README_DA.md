# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | **Dansk** | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Website](https://img.shields.io/badge/Hjemmeside-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Bliv_en_del_af_f%C3%A6llesskabet-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-St%C3%B8t_projektet-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licens-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.da)

En lokal Metin2-singleplayerverden, hvor selvstændige karakterer (Playerbots) drager rundt på kortene og spiller for alvor: de stiger i level, kæmper alene og i grupper, handler med hinanden, opgraderer deres udstyr, jager Metin-sten og bosser, stifter guilds og gemmer deres fremskridt i serverens database.

Botterne er ikke eksterne programmer. De er fuldgyldige karakterer, der styres af AI inde i servermotoren, så en spiller ser deres bevægelser, kampe, færdigheder og udstyr præcis, som han ser andre spilleres karakterer.

Spilklienten findes på polsk, engelsk, tysk, fransk, spansk, italiensk, portugisisk, rumænsk og tyrkisk. En dansk version af spillets brugerflade er under forberedelse som en fællesskabsoversættelse ([metin2-singleplayer-translations](https://github.com/TieruYT/metin2-singleplayer-translations)); indtil da kan du spille på engelsk.

> [!IMPORTANT]
> **Fra version 2.2.17 offentliggøres projektets kildekode ikke i dette repository.** Her finder du udgivelserne med deres opdateringspakker, ændringsloggen ([CHANGELOG.md](CHANGELOG.md)) og de filer, som launcheren og panelerne læser. Versioner til og med 2.2.16 blev udgivet under MIT-licensen, og deres historik bliver i repositoryet. Nyere versioner deles under licensen [CC BY-NC-ND 4.0](LICENSE).

## 📥 Sådan spiller du

1. Hent den **fulde pakke** (server og klient i ét arkiv) fra projektets Discord-server: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Installér Docker Desktop, og vent, til den viser "Engine running".
3. Pak pakken ud i en almindelig mappe, f.eks. `C:\Games\Metin2 Singleplayer` (ikke på Skrivebordet eller i Dokumenter, der synkroniseres af OneDrive), og kør `Serwer\Metin2-Launcher-GUI.bat`.
4. Når launcheren tilbyder en opdatering, så vælg **Update everything** (opdatér alt): den fulde pakke er ældre end den nyeste udgivelse.
5. Klik på **1. INSTALL / PREPARE** (installér / forbered) og derefter på **2. PLAY** (spil). Første start samler serveren i Docker af færdige filer og tager et par minutter; de senere starter tager kun et øjeblik.

> [!NOTE]
> **Siden version 2.2.39 er serveren den beskyttede version.** Den leveres som færdige serverfiler uden kildekode, og intet kompileres på din pc, så installation og opdatering tager kortere tid. En ældre, almindelig installation skifter af sig selv til den beskyttede version ved sin næste opdatering (launcheren skriver det i sin log med en nedtælling på 10 sekunder), og verdenen, karaktererne og indstillingerne bevares. De spilfiler, du kan redigere (drops, quests, kort), ligger nu i `Serwer\linux-port\docker\game\share\locale\poland\`, og opdateringer overskriver dem ikke. Moonlight-kisten er undtagelsen: gem din egen i `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (kopiér `share-add\special_item_group.moonlight.txt` fra den mappe, og behold linjen `Vnum 50011`).

Launcheren tilbyder selv en ny server- eller klientversion, når den starter, og **CHECK FOR UPDATES** (søg efter opdateringer) leder efter en når som helst. Pakkerne kommer fra [udgivelserne i dette repository](https://github.com/TieruYT/metin2-playerbots/releases). En server på Linux opdateres med `sh linux-port/tools/update.sh` i servermappen, og en server på en VPS kan sættes op og opdateres fra launcheren (**SERVER ON A VPS**). Også på en VPS samles serveren af færdige filer, uden at noget kompileres.

En fejl? Knappen **REPORT A BUG** (rapportér en fejl) i launcheren sender din beskrivelse sammen med logfilerne direkte til os. Send idéer og forslag til forslagskanalen på Discord.

Systemkrav, en trin-for-trin-guide og svar på almindelige spørgsmål finder du på [mt2sp.com](https://mt2sp.com) og i hjælpekanalerne på Discord.

## 💬 Fællesskab og støtte til projektet

- **[Projektets hjemmeside — mt2sp.com](https://mt2sp.com)**: hvad projektet er, installationsguiden og FAQ, på polsk, engelsk, tysk og tyrkisk.
- **[Discord-server](https://discord.gg/DyCxXdZtnV)**: den fulde pakke, hjælp, fejlrapporter, idéer og nyheder om nye versioner.
- **[Støt på buycoffee.to](https://buycoffee.to/metin2-playerbots)**: frivillige donationer hjælper med at dække værktøjerne, testserveren og de AI-modeller, der bruges til at udvikle projektet.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Støt på buycoffee.to"></a>

Al slags støtte hjælper med at bygge en mere levende Metin2-verden: test, fejlrapporter, idéer og donationer.

---

## 🌟 Hvad botterne gør

- ⚔️ **De kæmper som spillere**: alle klasser og retninger (Kriger, Sura, Ninja, Shaman), combos, færdighedsrotationer, buffs, bueskytter med pile og Metin-sten, der slås i stykker fra en kamphest. Botterne sparrer også med hinanden i deres kongeriges landsbyer.
- 🗺️ **Tre kongeriger og hele verden**: Shinsoo, Chunjo og Jinno med deres egne landsbyer, Orkdalen, Yongbi-ørkenen, Sohan-bjerget, Hwang-templet, Ildlandet (Doyyumhwaji), skovene, Abe- og Edderkoppe-dungeonerne og Eksilets Grotter. Botterne vælger kort og sted efter deres level og planlægger deres ruter over kortets rigtige kollisionsgitter.
- 🧠 **Personligheder**: Iwakuras personlighedssystem med humør. En bot kan være Grinder, Erobrer, Gambler, Perfektionist, Handelsmand, følgesvend, lejesoldat eller Metin-sten-dropper, og nu og da dukker en sjælden personlighed op.
- 🛡️ **Guilds, krige og raids**: botterne stifter guilds efter styrke, udkæmper guildkrige i runder på guildkortet (også mod spilleres guilds), tager til Dæmontårnet som guild, nedlægger verdens bosser sammen og går ned i Katakomberne efter Azrael.
- 🎉 **Events**: Piraten Tanaka, Zuos Metin-regn, Sekskantede kister fra Metin-sten og Halloween-eventet "Night of the Pale Moon" (Den blege månes nat), alle startet fra panelet. Botterne tager selv derhen for at kæmpe.
- 🏰 **Dungeons og tilføjelser**: Pest-dungeonen (level 45-120) og Jægerens Smedje med våben til level 50, begge slået fra som standard og slås til i panelet, Beran-Setaou og Snakefield-betaen med angrebshesten.
- 🏪 **Et botmarked**: rigtige offline-butikker på landsbyernes boder, priser fra Iwakuras prisliste med inflation, justeret efter efterspørgslen, og botter, der køber fra diskene i alle kongeriger. **Dom Towarowy** (stormagasinet) viser alle butikkers tilbud i ét vindue, foreslår en pris, når du sætter en genstand til salg ("Auto cena" — automatisk pris), og har en ønskeliste, der giver dig besked, når nogen sætter den genstand til salg, du leder efter. **ItemShoppen**, hvor der betales med Dragemønter, som droppes af Metin-sten og bosser, har også Lykkehjulet og lynauktioner.
- 🔨 **Karakterudvikling**: Smeden med "Refine up to +N" (opgradér op til +N) og materialer hentet fra samlerens lager, opgraderingsruller, bonusser, sjælesten, Dragesten-alkymi, færdighedsbøger, Combo op til 30 point, Åndesten, Biologens missioner, quests, hesten helt op til kamphesten, fiskeri, minedrift og urtekundskab.
- 💬 **Samtaler**: botterne svarer på hviskebeskeder og handler over chatten ("Kupię…" — jeg køber, "Sprzedam…" — jeg sælger), en Shaman fortæller dig, hvad dens buffs giver, og en bot, du kalder på, kommer hen til dig. Botterne skriver på læserens sprog: polsk, engelsk, tysk, fransk, portugisisk, rumænsk eller tyrkisk (engelsk for alle andre klienter).
- 🤝 **Towarzysz, din følgesvend**: en fast partner i din gruppe. Den kæmper ved din side, buffer dig og handler med dig, og du indstiller selv dens udstyr og færdigheder. Den har sit eget lager og sin egen Alkymi, fisker, slutter sig til en gruppe, som en ven leder ("Grupa" — gruppe), og kan selv lede en gruppe. Mens du er væk, spiller den på egen hånd ("Gra beze mnie" — spil uden mig): den læser bøger, handler på markedet, opgraderer hos Smeden, klarer Biologen og quests og aflægger rapport, når du vender tilbage. Du kan også spille den som en almindelig karakter.
- 🎯 **Auto Hunt**: automatisk jagt for spilleren med et opsamlingsfilter og en fisketilstand. Verdenens indstillinger afgør, om den er for alle eller skal købes i ItemShoppen.
- 🖥️ **Spilklienten**: ni sprog (polsk, engelsk, tysk, fransk, spansk, italiensk, portugisisk, rumænsk og tyrkisk), en Wiki i spillet med drops, kister og guides, Fællesskabsvinduet, et questvindue i GF-stil og [AUTO]/[AFK]-markører over spillere.
- 🎛️ **Paneler og launcher**: to webpaneler med et live verdenskort, ranglister, botternes udstyr, skydere for AI-adfærd, tidsstyrede events, respawntider for bosser og en databaseeditor; en launcher med opdateringer, backup af verdenen, sværhedsgradsindstillingen, ekstra kanaler, COOP (venner, der spiller i din verden, gratis) og en server på en VPS.
- 💾 **En vedvarende verden**: hver bot har sin egen konto og karakter i MariaDB, så levels, genstande og yang overlever en genstart.

## 🆕 Nyt for nylig (versioner 2.2.140–2.2.146)

- **Towarzysz** udvikler sig selv, mens du er væk, kan lede en gruppe og fisker på en helt ny måde, med agn og en opsummering af fangsten.
- **Combo** har 30 point, og dens tre bøger droppes af monstre.
- **Dom Towarowy** har fået en ønskeliste, og nye ruller, opgraderingsmaterialer og bøger på botternes diske venter først på spillerne.
- **Smeden** opgraderer automatisk op til det plus, du vælger; Fællesskabsvinduet, questvinduet og urtekundskabsguiderne i Wikien er også nye.
- **Botterne** sparrer i landsbyerne, slår Metin-sten i stykker som droppere, vælger mere fornuftigt, hvor de kæmper, og opfører sig mere fair over for spillere.
- Den fulde liste over ændringer findes i [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Taster i spillet

| Tast | Hvad den gør |
|---|---|
| `P` | følgesvendens vindue |
| `K` | Auto Hunt |
| `H` | hjælp med listen over taster og kommandoer |
| `X` | kanalvinduet (`X` + `1`–`4` flytter dig direkte til CH1–CH4) |
| `` ` `` (tilde) | saml alle genstande i nærheden op |
| `F5` | søg efter en genstand i offline-butikkerne |
| `F9` | GM-panelet (kun GM-karakterer) |
| `F1`–`F3` | på loginskærmen: en gemt konto fra den viste side (15 konti på 5 sider) |

## 🎮 Kommandoer i spillet (GM)

| Kommando | Tilladelse | Beskrivelse | Eksempel |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Administrator | Bringer botten med det ID ind i spillet (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrator | Logger botten ud af verdenen. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Administrator | Bringer en række botter ind i spillet. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Administrator | Logger en række botter ud. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Alle | Viser levelranglisten over de aktive botter i chatten. | `/bot_rank` |

## 🗄️ Adgang til databasen (Navicat, HeidiSQL, DBeaver)

Serverens database er MariaDB i en container og kan **kun nås fra denne
computer** (`127.0.0.1`, port `3306` eller en anden, hvis `M2_DB_PUBLISH_PORT`
er sat i `.env`). En ny forbindelse i en databaseklient: type MySQL/MariaDB,
vært `127.0.0.1`, port `3306`.

| Konto | Til | Adgangskode |
|---|---|---|
| `root` | alt | `M2_DB_ROOT_PASSWORD` i `linux-port\docker\.env` i servermappen |
| `metin2` | kun spillets databaser (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` i samme fil |

Den hurtigste vej: launcherens knap **DATABASE LOGIN (NAVICAT)** viser værten,
porten og begge adgangskoder i felter, der kan kopieres. Adgangskoderne trækkes
tilfældigt ved første start, og der findes ingen "standard"-adgangskode. Indsæt
dem ikke på Discord.

Hvis databaseklienten svarer `1045 - Access denied for user 'root'@'172.18.0.1'`,
blev databasen oprettet med en anden adgangskode end den, der nu står i `.env`.
Klik på **REPAIR DATABASE ACCESS** (reparér databaseadgang): launcheren stopper
serveren og sætter kontiene `metin2` og `root` til adgangskoderne i `.env`, og
karakterer, genstande og botter forbliver urørte. Klik derefter på PLAY, og log ind igen.

## 📜 Licens

Fra version 2.2.55 er projektet under **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.da)** (Kreditering-IkkeKommerciel-IngenBearbejdelser):

- du **må** spille, køre en server for dig selv og dine venner og dele de officielle pakker uændret, med kreditering af forfatteren og et link til projektet;
- du **må ikke** distribuere ændrede versioner eller separate distributioner; tilføjelser til projektet må laves og deles på projektets Discord;
- du **må ikke** tjene penge på det: intet salg af det, ingen betaling for adgang til en server, intet salg af genstande i spillet. Frivillige donationer til driften af en server, som ikke giver nogen fordel i spillet, samt optagelse og streaming af gameplay er i orden.

Versioner til og med 2.2.16 er under MIT-licensen ([LICENSE-MIT.txt](LICENSE-MIT.txt)), og versionerne 2.2.17 til 2.2.54 under CC BY-NC-SA 4.0. Metin2 tilhører Ymir Interactive og Webzen, og serverfilpakkerne tilhører deres forfattere. Detaljer findes i [LICENSE](LICENSE) og [NOTICE.md](NOTICE.md).

**Metin2-filer i pakkerne.** Projektets pakker (den fulde pakke samt klient- og serveropdateringerne) indeholder også filer, der ikke er projektforfatternes værk: Metin2-klienten og spildataene, som tilhører Ymir Interactive og Webzen, og filerne fra serverfilpakkerne (r40250 af TMP4, mt2009 af Martysama). Projektets licens dækker dem ikke og giver ingen rettigheder til dem. Tilladelsen til at dele pakkerne uændret dækker kun projektforfatternes værk.

### Anmodninger om fjernelse

Hvis du er rettighedshaver (Ymir Interactive, Webzen, Gameforge, forfatteren af en serverfilpakke eller nogen anden) og ønsker noget her fjernet, så [opret et issue](https://github.com/TieruYT/metin2-playerbots/issues) i dette repository, eller skriv på [projektets Discord](https://discord.gg/DyCxXdZtnV). Det er nok at nævne filerne eller udgivelserne. Vi fjerner dem hurtigt, uden diskussion og uden behov for en formel DMCA-meddelelse, selvom det naturligvis også er muligt.

## 🤝 Tak til

- **AzzlackSyndicate**: forfatter til det oprindelige grundlag for Linux-porten, installationsprogrammerne og panelet, som projektet voksede ud af (MIT, se [NOTICE.md](NOTICE.md)).
- **Iwakura**: botternes personlighedssystem, prislisten, genstandstiers, navnene på butikker og guilds, botternes kaldenavne og Community Patches.
- **seban latino**: Metin2 Singleplayer Panel, det andet panel i installationen, med sit live kort, profiler, ranglister og økonomi.
- **ĹŌŞƬĒĶ**: loginskærmen og Discord Rich Presence, sprogpakken, hviskede samtaler med botterne, personlighedslinjen over en bot og Lykkehjulets skive.
- **Colide**: Auto Hunt-vinduet, Fællesskabsvinduet, følgesvendens lager, offline-plan og fiskeri samt grafikken til [AUTO]/[AFK]-markørerne.
- **OskarPWA**: vinduet for botdepotet, færdighedsikonerne og den første version af F9-GM-panelet.
- **Tyrion**: søgning i offline-butikkerne efter én bestemt genstand, Halloween-eventet og de tyske, franske, portugisiske og rumænske oversættelser.
- **Uxìĕ [DSO]**: Dom Towarowy, Modets Kappe, der trækker monstre til fra hele skærmen, og inventarets sidepanel.
- **Gibon**: forhåndsvisninger af, hvad en kiste indeholder, og hvad et monster dropper.
- **Kiciamol**: målets liv på dets bjælke (også en spillers i kamp), F9-GM-panelet, Wikien i spillet med guides, Lykkehjulet, de Sekskantede kister, Combo op til 30 point og Snakefield-betaen.
- **NerrVoVy**: Pest-dungeonen, Jægerens Smedje, questvinduet i GF-stil, ItemShoppens lynauktioner, forhåndsvisningen af målets bonusser og kongerigeflagene i chatten.
- **Said (saido_0)**: den tyrkiske oversættelse og Urtekyndiges Vej.
- **blasty**: bonusskifteren.
- **Piciu713**: opgraderingschancen i Smedens vindue.
- **Kenny, Pabloo, Mur4s**: rettelser til opsamling i grupper, til botters ærinder i en spillers gruppe, til bjørnequesten og til Teleportringen.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): forskningsinspiration til autonome agenter i MMO-spil.
- Discord-fællesskabet: de tests, fejlrapporter og idéer, som det meste af dette projekt er kommet fra.
