# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | [English (README_EN.md)](README_EN.md) | **Deutsch** | [Română (README_RO.md)](README_RO.md) | [Português (README_PT.md)](README_PT.md) | [Türkçe (README_TR.md)](README_TR.md)

[![Webseite](https://img.shields.io/badge/Webseite-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Community_beitreten-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Spendier_einen_Kaffee-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Lizenz](https://img.shields.io/badge/Lizenz-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.de)

> Diese Datei ist eine Übersetzung von [README.md](README.md). Bei Abweichungen gilt die polnische Fassung.

Eine lokale Metin2-Singleplayer-Welt, in der autonome Charaktere (Playerbots) über die Karten laufen und wirklich spielen: Sie steigen im Level auf, kämpfen allein und in Gruppen, handeln miteinander, verbessern ihre Ausrüstung, jagen Metinsteine und Bosse, gründen Gilden und speichern ihren Fortschritt in der Datenbank des Servers.

Die Bots sind keine externen Programme. Es sind vollwertige, von einer KI gesteuerte Charaktere innerhalb der Server-Engine, deshalb sieht der Spieler ihre Bewegung, ihren Kampf, ihre Fertigkeiten und ihre Ausrüstung genauso wie die Charaktere anderer Spieler.

> [!IMPORTANT]
> **Ab Version 2.2.17 wird der Quellcode des Projekts nicht in diesem Repository veröffentlicht.** Hier findest du die Releases mit den Update-Paketen, die Liste der Änderungen ([CHANGELOG.md](CHANGELOG.md)) und die Dateien, die der Launcher und die Panels verwenden. Die Versionen bis einschließlich 2.2.16 wurden unter der MIT-Lizenz veröffentlicht, und ihre Geschichte bleibt im Repository. Neuere Versionen werden unter der Lizenz [CC BY-NC-ND 4.0](LICENSE) bereitgestellt.

## 📥 So spielst du

Die Namen der Schaltflächen stehen unten so, wie der Launcher sie zeigt (auf Polnisch), die deutsche Bedeutung steht in Klammern.

1. Lade das **vollständige Paket** (Server und Client in einem Archiv) vom Discord-Server des Projekts herunter: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Installiere Docker Desktop und warte, bis es „Engine running“ anzeigt.
3. Entpacke das Paket in einen gewöhnlichen Ordner, z. B. `C:\Spiele\Metin2 Singleplayer` (nicht auf den Desktop und nicht in die mit OneDrive synchronisierten Dokumente), und starte `Serwer\Metin2-Launcher-GUI.bat`.
4. Wenn der Launcher ein Update anbietet, wähle **Aktualizuj wszystko** (Alles aktualisieren): Das vollständige Paket ist älter als das neueste Release.
5. Klicke auf **1. ZAINSTALUJ / PRZYGOTUJ** (Installieren / Vorbereiten) und danach auf **2. GRAJ** (Spielen). Beim ersten Start wird der Server in Docker aus fertigen Dateien zusammengesetzt, das dauert einige Minuten, die nächsten Starts dauern nur einen Moment.

> [!NOTE]
> **Seit Version 2.2.39 ist der Server die geschützte Version.** Er kommt als fertige Serverdateien, ohne Quellcode und ohne Kompilieren auf deinem Computer, deshalb dauern Installation und Updates kürzer. Eine ältere, gewöhnliche Installation wechselt beim nächsten Update von selbst auf die geschützte Version (der Launcher sagt das in seinem Log mit einem 10-Sekunden-Countdown), und Welt, Charaktere und Einstellungen bleiben erhalten. Die Spieldateien, die du bearbeiten kannst (Drops, Quests, Karten), liegen jetzt in `Serwer\linux-port\docker\game\share\locale\poland\`, und Updates überschreiben sie nicht. Die Ausnahme ist die Mondlicht-Schatztruhe: Speichere deine eigene in `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (kopiere `share-add\special_item_group.moonlight.txt` aus diesem Ordner und behalte die Zeile `Vnum 50011`).

Nach einer neuen Server- oder Client-Version fragt der Launcher beim Start von selbst. Du kannst auch mit der Schaltfläche **SPRAWDZ AKTUALIZACJE** (Nach Updates suchen) nachsehen. Die Pakete werden aus den [Releases in diesem Repository](https://github.com/TieruYT/metin2-playerbots/releases) heruntergeladen. Einen Server unter Linux aktualisierst du mit dem Befehl `sh linux-port/tools/update.sh` im Serverordner, und einen Server auf einem VPS kannst du über den Launcher einrichten und aktualisieren (**SERWER NA VPS** — Server auf einem VPS). Auch auf dem VPS wird der Server aus fertigen Dateien zusammengesetzt, ohne Kompilieren.

Ein Fehler? Die Schaltfläche **ZGŁOŚ BŁĄD** (Fehler melden) im Launcher schickt deine Beschreibung zusammen mit den Logs direkt an uns. Ideen und Vorschläge meldest du auf Discord im Vorschlagskanal.

Systemanforderungen, eine Schritt-für-Schritt-Anleitung und Antworten auf häufige Fragen findest du auf [mt2sp.com](https://mt2sp.com) und in den Hilfekanälen auf Discord.

## 💬 Community und Unterstützung des Projekts

- **[Projektseite — mt2sp.com](https://mt2sp.com)**: Beschreibung des Projekts, Installationsanleitung und FAQ, auf Polnisch, Englisch, Deutsch und Türkisch.
- **[Discord-Server](https://discord.gg/DyCxXdZtnV)**: das vollständige Paket zum Herunterladen, Hilfe, Fehlermeldungen, Ideen und Informationen über neue Versionen.
- **[Unterstützung über buycoffee.to](https://buycoffee.to/metin2-playerbots)**: Freiwillige Spenden helfen, die Kosten für die Werkzeuge, den Testserver und die KI-Modelle zu decken, die bei der Entwicklung des Projekts verwendet werden.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Spendier einen Kaffee über buycoffee.to"></a>

Jede Form der Unterstützung hilft, eine immer lebendigere Metin2-Welt zu schaffen: Tests, Fehlermeldungen, Ideen und Spenden.

---

## 🌟 Was die Bots können

- ⚔️ **Kampf wie Spieler**: alle Klassen und Wege (Krieger, Sura, Ninja, Schamane), Kombos, Fertigkeitsrotationen, Buffs, Bogenschützen mit Pfeilen und Metinsteine, die vom Schlachtross aus zerschlagen werden.
- 🗺️ **Drei Reiche und die ganze Welt**: Shinsoo, Chunjo und Jinno mit ihren eigenen Dörfern, das Tal der Orks, die Yongbi-Wüste, der Berg Sohan, der Hwang-Tempel, das Land des Feuers (Doyyumhwaji), die Wälder sowie die Affen- und Spinnen-Dungeons. Die Bots wählen Karte und Ort nach ihrem Level und berechnen ihre Wege über das echte Kollisionsraster der Karte.
- 🧠 **Persönlichkeiten**: Iwakuras Persönlichkeitssystem mit Stimmungen. Bots sind mal Grinder, Eroberer, Glücksspieler, Perfektionisten, Händler, Begleiter und Söldner, und manchmal taucht eine seltene Persönlichkeit auf.
- 🛡️ **Gilden, Kriege und Raids**: Die Bots gründen Gilden nach ihrer Stärke, führen Gildenkriege in Runden auf der Gildenkarte (auch gegen Gilden von Spielern), ziehen als ganze Gilde in den Dämonenturm, besiegen gemeinsam die Bosse der Welt und steigen in die Teufelskatakombe hinab, um Azrael zu holen.
- 🎉 **Events**: Pirat Tanaka und Zuos Metinregen mit Ankündigungen und Belohnungen, und die Bots ziehen von selbst dorthin in den Kampf.
- 🏪 **Markt der Bots**: echte Offline-Shops an den Ständen in den Dörfern, Preise aus Iwakuras Preisliste mit Inflation, angepasst an die Nachfrage, und Käufe zwischen den Bots. Das **Dom Towarowy** (im deutschen Client „Flohmarkt“) zeigt in einem Fenster die Angebote aller Shops und schlägt beim Einstellen eines Gegenstands den Preis der Bots vor (mit der Option „Auto cena“ — automatischer Preis). Im Spiel gibt es außerdem einen **ItemShop** für Drachenmünzen, die von Metinsteinen und Bossen fallen.
- 🔨 **Charakterentwicklung**: Schmied und Schriftrollen, Boni, Geiststeine, Fertigkeitsbücher und Seelensteine, die Missionen des Biologen, das Pferd bis zum Schlachtross, Angeln, Bergbau und Kräuterkunde.
- 💬 **Gespräche**: Die Bots antworten auf Flüstern, handeln über den Chat („Kupię…“ — ich kaufe, „Sprzedam…“ — ich verkaufe), ein Schamane sagt dir, was seine Buffs bringen, und ein gerufener Bot kommt zu dir.
- 🤝 **Towarzysz (Begleiter)**: dein fester Gefährte in der Gruppe. Er kämpft an deiner Seite, bufft dich und handelt mit dir, seine Ausrüstung und Fertigkeiten stellst du selbst ein. Er tritt auch einer Gruppe bei, die ein Freund führt („Grupa“ — Gruppe), und kann allein spielen, wenn du nicht da bist („Gra beze mnie“ — Spiel ohne mich).
- 🎯 **Auto Łowy (Autojagd)**: automatisches Jagen für den Spieler. In den Welteinstellungen wählst du, ob es für jeden ist oder nach einem Kauf im ItemShop.
- 🎛️ **Panels und Launcher**: zwei Web-Panels mit Live-Weltkarte, Ranglisten, der Ausrüstung der Bots, Reglern für das Verhalten der KI und zeitgesteuerten Events; ein Launcher mit Updates, Weltsicherung, Schwierigkeitsgrad, zweitem Kanal, COOP (mit Freunden in deiner Welt spielen, kostenlos) und Server auf einem VPS.
- 💾 **Dauerhafte Welt**: Jeder Bot hat ein eigenes Konto und einen eigenen Charakter in der MariaDB-Datenbank, deshalb bleiben Level, Gegenstände und Yang nach einem Neustart erhalten.

## ⌨️ Tastenkürzel im Spiel

| Taste | Wirkung |
|---|---|
| `P` | Fenster des Towarzysz (Begleiter) |
| `K` | Autojagd |
| `` ` `` (Tilde) | alle Gegenstände in der Nähe aufheben |
| `F5` | Suche nach Gegenständen in den Offline-Shops |
| `F9` | GM-Panel (nur GM-Charaktere) |
| `F1`–`F3` | auf dem Anmeldebildschirm: gespeichertes Konto der angezeigten Seite (15 Konten auf 5 Seiten) |

## 🎮 Befehle im Spiel (GM)

| Befehl | Berechtigung | Beschreibung | Beispiel |
|---|---|---|---|
| `/bot_spawn <id> <Reich: 1-3>` | Administrator | Bringt den Bot mit der angegebenen ID ins Spiel (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrator | Meldet den Bot aus der Welt ab. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <Anzahl> <Reich>` | Administrator | Bringt eine Gruppe von Bots ins Spiel. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <Anzahl>` | Administrator | Meldet eine Gruppe von Bots ab. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Alle | Zeigt im Chat die Level-Rangliste der aktiven Bots. | `/bot_rank` |

## 🗄️ Zugriff auf die Datenbank (Navicat, HeidiSQL, DBeaver)

Die Datenbank des Servers ist MariaDB in einem Container, erreichbar **nur auf diesem Computer**
(`127.0.0.1`, Port `3306` oder ein anderer, wenn in `.env` `M2_DB_PUBLISH_PORT` gesetzt ist).
Neue Verbindung im Datenbank-Client: Typ MySQL/MariaDB, Host `127.0.0.1`, Port `3306`.

| Konto | Wofür | Passwort |
|---|---|---|
| `root` | alles | `M2_DB_ROOT_PASSWORD` in der Datei `linux-port\docker\.env` im Serverordner |
| `metin2` | nur die Datenbanken des Spiels (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` in derselben Datei |

Am schnellsten: Die Schaltfläche **DANE DO BAZY (NAVICAT)** (Zugangsdaten der Datenbank) im Launcher zeigt Host,
Port und beide Passwörter in Feldern zum Kopieren. Die Passwörter werden beim ersten
Start zufällig erzeugt, und es gibt kein „Standardpasswort“. Füge sie nicht auf Discord ein.

Wenn der Datenbank-Client `1045 - Access denied for user 'root'@'172.18.0.1'` antwortet,
wurde die Datenbank mit einem anderen Passwort angelegt als dem, das jetzt in `.env` steht.
Klicke auf **NAPRAW DOSTEP DO BAZY** (Datenbankzugriff reparieren): Der Launcher hält den Server an und setzt die Konten
`metin2` und `root` auf die Passwörter aus `.env`, Charaktere, Gegenstände und Bots bleiben
unberührt. Klicke danach auf GRAJ (Spielen) und melde dich erneut an.

## 📜 Lizenz

Ab Version 2.2.55 steht das Projekt unter der Lizenz **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.de)** (Namensnennung - Nicht kommerziell - Keine Bearbeitungen):

- **erlaubt** ist es, zu spielen, einen Server für dich und deine Freunde zu betreiben und die offiziellen Pakete unverändert weiterzugeben, unter Nennung des Autors und mit einem Link zum Projekt;
- **nicht erlaubt** ist es, Bearbeitungen oder eigene Distributionen zu verbreiten; Erweiterungen für das Projekt dürfen erstellt und auf dem Discord des Projekts geteilt werden;
- **nicht erlaubt** ist es, damit Geld zu verdienen: es zu verkaufen, Gebühren für den Zugang zu einem Server zu verlangen oder Gegenstände im Spiel zu verkaufen. Freiwillige Spenden für den Unterhalt eines Servers, wenn sie keinen Vorteil im Spiel bringen, sowie das Aufnehmen und Streamen des Spiels sind in Ordnung.

Die Versionen bis einschließlich 2.2.16 stehen unter der MIT-Lizenz ([LICENSE-MIT.txt](LICENSE-MIT.txt)), die Versionen von 2.2.17 bis 2.2.54 unter CC BY-NC-SA 4.0. Metin2 gehört Ymir Interactive und Webzen, die Pakete der Serverdateien ihren Autoren. Einzelheiten stehen in den Dateien [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md).

**Metin2-Dateien in den Paketen.** Die Pakete des Projekts (das Komplettpaket sowie die Client- und Server-Updates) enthalten auch Dateien, die nicht von den Autoren des Projekts stammen: den Metin2-Client und die Spieldaten, die Ymir Interactive und Webzen gehören, sowie die Dateien der Serverdatei-Pakete (r40250 von TMP4, mt2009 von Martysama). Die Lizenz des Projekts umfasst sie nicht und gewährt keinerlei Rechte an ihnen. Die Erlaubnis, die Pakete unverändert weiterzugeben, betrifft nur die Arbeit der Autoren des Projekts.

### Löschanfragen

Wenn du Rechteinhaber bist (Ymir Interactive, Webzen, Gameforge, der Autor eines Serverdatei-Pakets oder jemand anderes) und möchtest, dass hier etwas entfernt wird, [eröffne ein Issue](https://github.com/TieruYT/metin2-playerbots/issues) in diesem Repository oder schreib auf dem [Discord des Projekts](https://discord.gg/DyCxXdZtnV). Es genügt, die Dateien oder Releases zu nennen. Wir entfernen sie umgehend, ohne Diskussion und ohne dass eine förmliche DMCA-Meldung nötig ist, auch wenn diese natürlich ebenfalls möglich ist.

## 🤝 Danksagungen

- **AzzlackSyndicate**: Autor der ursprünglichen Grundlage des Linux-Ports, der Installer und des Panels, auf denen das Projekt entstanden ist (MIT-Lizenz, siehe [NOTICE.md](NOTICE.md)).
- **Iwakura**: das Persönlichkeitssystem der Bots, die Preisliste, die Stufen (Tiers) der Gegenstände, die Namen der Shops und Gilden, die Namen der Bots und die Community Patches.
- **seban latino**: das Metin2 Singleplayer Panel, das zweite Panel der Installation, mit Live-Karte, Profilen, Ranglisten und Wirtschaft.
- **ĹŌŞƬĒĶ**: der Anmeldebildschirm und Discord Rich Presence, das Sprachpaket, Gespräche mit den Bots per Flüstern und die Persönlichkeitszeile über dem Bot.
- **Colide**: das Fenster der Autojagd.
- **OskarPWA**: das Lagerfenster des Bots, die Fertigkeitssymbole und das GM-Panel unter F9.
- **Tyrion**: die Suche nach einem bestimmten Gegenstand in den Offline-Shops.
- **Uxìĕ [DSO]**: das Dom Towarowy und der Tapferkeitsumhang, der Monster vom ganzen Bildschirm anzieht.
- **Gibon**: die Vorschau auf den Inhalt von Truhen und den Drop von Monstern.
- **Kiciamol**: das Leben des Ziels auf der Leiste, auch das eines Spielers im Kampf.
- **Piciu713**: die Verbesserungschance im Fenster des Schmieds.
- **Kenny, Pabloo, Mur4s**: Korrekturen beim Aufheben von Gegenständen in der Gruppe, bei den Aufgaben der Bots in der Gruppe eines Spielers, bei der Bären-Quest und beim Teleportationsring.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): Inspiration für die Forschung an autonomen Agenten in MMO-Spielen.
- Die Discord-Community: Tests, Fehlermeldungen und Ideen, aus denen der größte Teil dieses Projekts entstanden ist.
