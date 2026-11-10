# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | **Italiano** | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Sito web](https://img.shields.io/badge/Sito_web-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Unisciti_alla_community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Sostieni_il_progetto-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licenza](https://img.shields.io/badge/Licenza-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.it)

Un mondo di Metin2 in singleplayer, in locale, in cui personaggi autonomi (Playerbots) girano per le mappe e giocano davvero: salgono di livello, combattono da soli e in gruppo, commerciano tra loro, potenziano l'equipaggiamento, cacciano pietre Metin e boss, fondano gilde e salvano i propri progressi nel database del server.

I bot non sono programmi esterni. Sono personaggi a tutti gli effetti, guidati da un'IA all'interno del motore del server, quindi un giocatore vede i loro movimenti, i loro combattimenti, le loro abilità e il loro equipaggiamento esattamente come quelli dei personaggi degli altri giocatori.

> [!IMPORTANT]
> **Dalla versione 2.2.17 il codice sorgente del progetto non viene pubblicato in questo repository.** Qui trovi le release con i loro pacchetti di aggiornamento, l'elenco delle modifiche ([CHANGELOG.md](CHANGELOG.md)) e i file che leggono il launcher e i pannelli. Le versioni fino alla 2.2.16 inclusa sono state pubblicate con licenza MIT e la loro cronologia resta nel repository. Le versioni più recenti sono condivise con licenza [CC BY-NC-ND 4.0](LICENSE).

## 📥 Come giocare

1. Scarica il **pacchetto completo** (server e client in un unico archivio) dal server Discord del progetto: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Installa Docker Desktop e aspetta che mostri "Engine running".
3. Estrai il pacchetto in una cartella normale, ad esempio `C:\Games\Metin2 Singleplayer` (non sul Desktop né nei Documenti sincronizzati da OneDrive), e avvia `Serwer\Metin2-Launcher-GUI.bat`.
4. Quando il launcher propone un aggiornamento, scegli **Update everything** (aggiorna tutto): il pacchetto completo è più vecchio dell'ultima release.
5. Clicca **1. INSTALL / PREPARE** (installa / prepara), poi **2. PLAY** (gioca). Il primo avvio assembla il server in Docker a partire da file già pronti e richiede qualche minuto; i successivi solo un attimo.

> [!NOTE]
> **Dalla versione 2.2.39 il server è la versione protetta.** Arriva come file server già pronti, senza codice sorgente e senza compilare nulla sul tuo PC, quindi installazione e aggiornamenti richiedono meno tempo. Un'installazione più vecchia e normale passa da sola alla versione protetta con il suo aggiornamento successivo (il launcher lo segnala nel suo log con un conto alla rovescia di 10 secondi), e il mondo, i personaggi e le impostazioni restano. I file del gioco che puoi modificare (drop, quest, mappe) ora si trovano in `Serwer\linux-port\docker\game\share\locale\poland\`, e gli aggiornamenti non li sovrascrivono. Fa eccezione il forziere Chiaro di Luna: salva il tuo in `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copia `share-add\special_item_group.moonlight.txt` da quella cartella e mantieni la riga `Vnum 50011`).

All'avvio il launcher propone da solo una nuova versione del server o del client, e **CHECK FOR UPDATES** (cerca aggiornamenti) ne cerca una in qualsiasi momento. I pacchetti provengono dalle [release di questo repository](https://github.com/TieruYT/metin2-playerbots/releases). Un server su Linux si aggiorna con `sh linux-port/tools/update.sh` nella cartella del server, e un server su un VPS si può configurare e aggiornare dal launcher (**SERVER ON A VPS**, server su un VPS). Anche su un VPS il server viene assemblato da file già pronti, senza compilare nulla.

Un bug? Il pulsante **REPORT A BUG** (segnala un bug) del launcher ci invia direttamente la tua descrizione insieme ai log. Idee e suggerimenti vanno nel canale delle proposte su Discord.

Requisiti, una guida passo passo e le risposte alle domande frequenti si trovano su [mt2sp.com](https://mt2sp.com) e nei canali di aiuto su Discord.

## 💬 Community e supporto al progetto

- **[Sito del progetto — mt2sp.com](https://mt2sp.com)**: che cos'è il progetto, la guida all'installazione e le FAQ, in polacco, inglese, tedesco e turco.
- **[Server Discord](https://discord.gg/DyCxXdZtnV)**: il pacchetto completo, aiuto, segnalazioni di bug, idee e novità sulle nuove versioni.
- **[Supporto su buycoffee.to](https://buycoffee.to/metin2-playerbots)**: le donazioni volontarie aiutano a coprire gli strumenti, il server di test e i modelli di IA usati per sviluppare il progetto.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Sostieni su buycoffee.to"></a>

Ogni forma di supporto aiuta a costruire un mondo di Metin2 più vivo: i test, le segnalazioni di bug, le idee e le donazioni.

---

## 🌟 Che cosa fanno i bot

- ⚔️ **Combattono come i giocatori**: tutte le classi e tutte le vie (Guerriero, Sura, Ninja, Sciamano), combo, rotazioni di abilità, buff, arcieri con le frecce e pietre Metin distrutte da un cavallo da battaglia. I bot fanno anche duelli amichevoli tra loro nei villaggi del loro regno.
- 🗺️ **Tre regni e il mondo intero**: Shinsoo, Chunjo e Jinno con i loro villaggi, la Valle degli Orchi, il Deserto di Yongbi, il Monte Sohan, il Tempio Hwang, la Terra del Fuoco (Doyyumhwaji), le foreste, i Dungeon delle Scimmie e dei Ragni e le Grotte dell'Esilio. I bot scelgono la mappa e il punto in base al loro livello e pianificano i percorsi sulla vera griglia di collisione della mappa.
- 🧠 **Personalità**: il sistema di personalità di Iwakura, con gli stati d'animo. Un bot può essere un Farmatore (Grinder), un Conquistatore, uno Scommettitore, un Perfezionista, un Mercante, un compagno, un mercenario o un dropper di pietre Metin, e ogni tanto compare una personalità rara.
- 🛡️ **Gilde, guerre e raid**: i bot fondano gilde in base alla loro forza, combattono guerre di gilda a round sulla mappa di gilda (anche contro le gilde dei giocatori), salgono come gilda nella Torre dei Demoni, abbattono insieme i boss del mondo e scendono nelle Catacombe a caccia di Azrael.
- 🎉 **Eventi**: il pirata Tanaka, la pioggia di Metin di Zuo, i Forzieri esagonali dalle pietre Metin e l'evento di Halloween "La Notte della Luna Pallida", tutti avviati dal pannello. I bot ci vanno da soli a combattere.
- 🏰 **Dungeon e aggiunte**: il Dungeon della Peste (livelli 45-120) e la Forgia del Cacciatore con armi per il livello 50, entrambi disattivati di default e attivabili nel pannello, Beran-Setaou e la beta di Snakefield con il cavallo d'assalto.
- 🏪 **Un mercato dei bot**: veri negozi offline sulle bancarelle dei villaggi, prezzi presi dal listino di Iwakura con inflazione, adeguati alla domanda, e bot che comprano dai banchi di tutti i regni. Il **Dom Towarowy** (grande magazzino) mostra le offerte di tutti i negozi in un'unica finestra, suggerisce un prezzo quando metti in vendita un oggetto ("Auto cena" — prezzo automatico) e ha una lista dei desideri che ti avvisa quando qualcuno mette in vendita l'oggetto che cerchi. L'**ItemShop**, che si paga con le Monete del Drago che cadono dalle pietre Metin e dai boss, ha anche la Ruota della Fortuna e le aste lampo.
- 🔨 **Progressione del personaggio**: il Fabbro con "Potenzia fino a +N" e i materiali presi dal magazzino del collezionista, pergamene di potenziamento, bonus, pietre dell'anima, l'Alchimia delle Pietre del Drago, libri delle abilità, la Combo fino a 30 punti, le Pietre Spirituali, le missioni del Biologo, le quest, il cavallo fino al cavallo da battaglia, la pesca, l'estrazione mineraria e l'erboristeria.
- 💬 **Conversazioni**: i bot rispondono ai sussurri e commerciano in chat ("Kupię…" — compro, "Sprzedam…" — vendo), uno Sciamano ti dirà che cosa danno i suoi buff e un bot che chiami verrà da te. I bot scrivono nella lingua di chi legge: polacco, inglese, tedesco, francese, portoghese, rumeno o turco (inglese per tutti gli altri client).
- 🤝 **Towarzysz, il tuo compagno**: un partner fisso nel tuo gruppo. Combatte al tuo fianco, ti dà buff e commercia con te, e sei tu a scegliere il suo equipaggiamento e le sue abilità. Ha il suo magazzino e la sua Alchimia, pesca, entra in un gruppo guidato da un amico ("Grupa" — gruppo) e può guidare lui stesso un gruppo. Quando non ci sei gioca da solo ("Gra beze mnie" — gioca senza di me): legge libri, fa acquisti al mercato, potenzia dal Fabbro, fa il Biologo e le quest, e al tuo ritorno ti racconta cosa ha fatto. Puoi anche giocarlo come un personaggio normale.
- 🎯 **Caccia automatica**: caccia automatica per il giocatore, con un filtro di raccolta e una modalità pesca. Le impostazioni del mondo decidono se è per tutti o si compra nell'ItemShop.
- 🖥️ **Il client di gioco**: nove lingue (polacco, inglese, tedesco, francese, spagnolo, italiano, portoghese, rumeno e turco), una Wiki in gioco con drop, forzieri e guide, la finestra Community, una finestra delle quest in stile GF e i marcatori [AUTO]/[AFK] sopra i giocatori.
- 🎛️ **Pannelli e launcher**: due pannelli web con una mappa del mondo in tempo reale, classifiche, l'equipaggiamento dei bot, cursori del comportamento dell'IA, eventi programmati, tempi di respawn dei boss e un editor del database; un launcher con aggiornamenti, backup del mondo, impostazione della difficoltà, canali aggiuntivi, COOP (amici che giocano nel tuo mondo, gratis) e un server su un VPS.
- 💾 **Un mondo persistente**: ogni bot ha il suo account e il suo personaggio in MariaDB, quindi livelli, oggetti e yang sopravvivono a un riavvio.

## 🆕 Aggiunte recenti (versioni 2.2.140–2.2.146)

- **Towarzysz** progredisce da solo mentre non ci sei, può guidare un gruppo e pesca in un modo tutto nuovo, con le esche e un riepilogo del pescato.
- La **Combo** ha 30 punti, e i suoi tre libri cadono dai mostri.
- Il **Dom Towarowy** ha una lista dei desideri, e le nuove pergamene, i materiali di potenziamento e i libri sui banchi dei bot aspettano prima i giocatori.
- **Il Fabbro** potenzia automaticamente fino al + che scegli; anche la finestra Community, la finestra delle quest e le guide all'erboristeria nella Wiki sono nuove.
- **I bot** fanno duelli amichevoli nei villaggi, distruggono pietre Metin come dropper, scelgono con più giudizio dove combattere e si comportano in modo più corretto accanto ai giocatori.
- L'elenco completo delle modifiche è in [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Tasti in gioco

| Tasto | Cosa fa |
|---|---|
| `P` | la finestra del compagno |
| `K` | Caccia automatica |
| `H` | aiuto con l'elenco dei tasti e dei comandi |
| `X` | la finestra dei canali (`X` + `1`–`4` ti porta direttamente su CH1–CH4) |
| `` ` `` (tilde) | raccogliere tutti gli oggetti nelle vicinanze |
| `F5` | cercare un oggetto nei negozi offline |
| `F9` | il pannello GM (solo personaggi GM) |
| `F1`–`F3` | nella schermata di login: un account salvato della pagina mostrata (15 account su 5 pagine) |

## 🎮 Comandi in gioco (GM)

| Comando | Permesso | Descrizione | Esempio |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Amministratore | Fa entrare in gioco il bot con quell'ID (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Amministratore | Disconnette il bot dal mondo. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Amministratore | Fa entrare in gioco un intervallo di bot. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Amministratore | Disconnette un intervallo di bot. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Tutti | Mostra in chat la classifica per livello dei bot attivi. | `/bot_rank` |

## 🗄️ Accesso al database (Navicat, HeidiSQL, DBeaver)

Il database del server è MariaDB in un container, raggiungibile **solo da questo
computer** (`127.0.0.1`, porta `3306`, oppure un'altra se in `.env` è impostato
`M2_DB_PUBLISH_PORT`). Nuova connessione in un client di database: tipo
MySQL/MariaDB, host `127.0.0.1`, porta `3306`.

| Account | Per | Password |
|---|---|---|
| `root` | tutto | `M2_DB_ROOT_PASSWORD` in `linux-port\docker\.env` nella cartella del server |
| `metin2` | solo i database del gioco (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` nello stesso file |

Il modo più rapido: il pulsante **DATABASE LOGIN (NAVICAT)** del launcher mostra
l'host, la porta e le due password in campi da copiare. Le password vengono generate
al primo avvio e non esiste una password "predefinita". Non incollarle su Discord.

Se il client del database risponde `1045 - Access denied for user 'root'@'172.18.0.1'`,
il database è stato creato con una password diversa da quella che ora si trova in
`.env`. Clicca **REPAIR DATABASE ACCESS** (ripara l'accesso al database): il launcher
ferma il server e imposta per gli account `metin2` e `root` le password di `.env`,
senza toccare personaggi, oggetti e bot. Poi clicca PLAY e accedi di nuovo.

## 📜 Licenza

Dalla versione 2.2.55 il progetto è sotto licenza **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.it)** (Attribuzione - Non commerciale - Non opere derivate):

- **puoi** giocare, gestire un server per te e per i tuoi amici e condividere i pacchetti ufficiali senza modifiche, citando l'autore con un link al progetto;
- **non puoi** distribuire versioni modificate né distribuzioni separate; si possono creare aggiunte al progetto e condividerle sul Discord del progetto;
- **non puoi** guadagnarci: niente vendita, niente accesso a pagamento a un server, niente vendita di oggetti in gioco. Le donazioni volontarie per mantenere un server che non danno alcun vantaggio in gioco, così come registrare e trasmettere in streaming le partite, sono consentite.

Le versioni fino alla 2.2.16 inclusa sono sotto licenza MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), e le versioni dalla 2.2.17 alla 2.2.54 sotto CC BY-NC-SA 4.0. Metin2 appartiene a Ymir Interactive e Webzen, e i pacchetti di file server ai loro autori. I dettagli sono in [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md).

**File di Metin2 nei pacchetti.** I pacchetti del progetto (il pacchetto completo e gli aggiornamenti del client e del server) contengono anche file che non sono opera degli autori del progetto: il client di Metin2 e i dati di gioco, che appartengono a Ymir Interactive e Webzen, e i file dei pacchetti di file server (r40250 di TMP4, mt2009 di Martysama). La licenza del progetto non li copre e non conferisce alcun diritto su di essi. Il permesso di condividere i pacchetti senza modifiche copre soltanto l'opera degli autori del progetto.

### Richieste di rimozione

Se sei titolare di diritti (Ymir Interactive, Webzen, Gameforge, l'autore di un pacchetto di file server o chiunque altro) e vuoi che qualcosa venga rimosso da qui, [apri una issue](https://github.com/TieruYT/metin2-playerbots/issues) in questo repository o scrivi sul [Discord del progetto](https://discord.gg/DyCxXdZtnV). Basta indicare i file o le release. Li rimuoveremo prontamente, senza discussioni e senza bisogno di una notifica DMCA formale, anche se naturalmente è possibile anche quella.

## 🤝 Ringraziamenti

- **AzzlackSyndicate**: autore della base originale del port per Linux, degli installer e del pannello da cui è nato il progetto (MIT, vedi [NOTICE.md](NOTICE.md)).
- **Iwakura**: il sistema di personalità dei bot, il listino prezzi, i livelli degli oggetti, i nomi dei negozi e delle gilde, i nickname dei bot e i Community Patches.
- **seban latino**: il Metin2 Singleplayer Panel, il secondo pannello dell'installazione, con la sua mappa in tempo reale, i profili, le classifiche e l'economia.
- **ĹŌŞƬĒĶ**: la schermata di login e la Rich Presence di Discord, il pacchetto delle lingue, le conversazioni sussurrate con i bot, la riga della personalità sopra un bot e il disco della Ruota della Fortuna.
- **Colide**: la finestra della Caccia automatica, la finestra Community, il magazzino, il piano offline e la pesca del compagno, e la grafica dei marcatori [AUTO]/[AFK].
- **OskarPWA**: la finestra del deposito dei bot, le icone delle abilità e la prima versione del pannello GM su F9.
- **Tyrion**: la ricerca di un oggetto preciso nei negozi offline, l'evento di Halloween e le traduzioni in tedesco, francese, portoghese e rumeno.
- **Uxìĕ [DSO]**: il Dom Towarowy, il Mantello del Coraggio che attira i mostri da tutto lo schermo e il pannello laterale dell'inventario.
- **Gibon**: l'anteprima di ciò che contiene un forziere e di ciò che lascia cadere un mostro.
- **Kiciamol**: la vita del bersaglio sulla sua barra (anche quella di un giocatore in combattimento), il pannello GM su F9, la Wiki in gioco con le guide, la Ruota della Fortuna, i Forzieri esagonali, la Combo fino a 30 punti e la beta di Snakefield.
- **NerrVoVy**: il Dungeon della Peste, la Forgia del Cacciatore, la finestra delle quest in stile GF, le aste lampo dell'ItemShop, l'anteprima dei bonus del bersaglio e le bandiere dei regni in chat.
- **Said (saido_0)**: la traduzione in turco e il Sentiero dell'Erborista.
- **blasty**: il cambiabonus.
- **Piciu713**: la probabilità di potenziamento nella finestra del Fabbro.
- **Kenny, Pabloo, Mur4s**: correzioni alla raccolta degli oggetti in gruppo, alle commissioni dei bot nel gruppo di un giocatore, alla quest dell'orso e all'Anello del Teletrasporto.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): ispirazione per la ricerca sugli agenti autonomi nei giochi MMO.
- La community di Discord: i test, le segnalazioni di bug e le idee da cui è nata la maggior parte di questo progetto.
