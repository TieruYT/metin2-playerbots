# ⚔️ Metin2 Playerbots

[Polski](README.md) | **English** | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Website](https://img.shields.io/badge/Website-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Join_Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Support_the_Project-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licence-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.en)

A local Metin2 singleplayer world in which autonomous characters (Playerbots) run across the maps and genuinely play: they level up, fight solo and in parties, trade with each other, refine their gear, hunt Metin stones and bosses, found guilds and keep their progress in the server's database.

The bots are not external programs. They are first-class characters driven by AI inside the server engine, so a player sees their movement, combat, skills and equipment exactly as they see other players' characters.

> [!IMPORTANT]
> **From version 2.2.17 the project's source code is not published in this repository.** Here you will find the releases with their update packages, the changelog ([CHANGELOG.md](CHANGELOG.md)) and the files the launcher and the panels read. Versions up to and including 2.2.16 were released under the MIT licence and their history stays in the repository. Newer versions are shared under the [CC BY-NC-ND 4.0](LICENSE) licence.

## 📥 How to play

1. Download the **full package** (server and client in one archive) from the project's Discord server: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Install Docker Desktop and wait until it shows "Engine running".
3. Unpack the package into an ordinary folder, e.g. `C:\Games\Metin2 Singleplayer` (not the Desktop or Documents synced by OneDrive), and run `Serwer\Metin2-Launcher-GUI.bat`.
4. When the launcher offers an update, choose **Update everything**: the full package is older than the newest release.
5. Click **1. INSTALL / PREPARE**, then **2. PLAY**. The first start assembles the server in Docker from ready-made files and takes a few minutes; later ones take a moment.

> [!NOTE]
> **Since version 2.2.39 the server is the protected version.** It comes as ready-made server files, with no source code and nothing compiled on your PC, so installing and updating take less time. An older, ordinary installation moves onto the protected version by itself with its next update (the launcher says so in its log with a 10-second countdown), and the world, characters and settings stay. The game files you can edit (drops, quests, maps) are now in `Serwer\linux-port\docker\game\share\locale\poland\`, and updates do not overwrite them. The Moonlight chest is the exception: save your own in `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copy `share-add\special_item_group.moonlight.txt` from that folder and keep the `Vnum 50011` line).

The launcher offers a new server or client version by itself when it starts, and **CHECK FOR UPDATES** looks for one at any time. The packages come from [the releases in this repository](https://github.com/TieruYT/metin2-playerbots/releases). A server on Linux is updated with `sh linux-port/tools/update.sh` in the server folder, and a server on a VPS can be set up and updated from the launcher (**SERVER ON A VPS**). On a VPS, too, the server is assembled from ready-made files, with nothing compiled.

A bug? The **REPORT A BUG** button in the launcher sends your description together with the logs straight to us. Send ideas and suggestions to the proposals channel on Discord.

Requirements, a step-by-step guide and answers to common questions are on [mt2sp.com](https://mt2sp.com) and in the help channels on Discord.

## 💬 Community & Project Support

- **[Project website — mt2sp.com](https://mt2sp.com)**: what the project is, the install guide and the FAQ, in Polish, English, German and Turkish.
- **[Discord server](https://discord.gg/DyCxXdZtnV)**: the full package, help, bug reports, ideas and news about new versions.
- **[Support on buycoffee.to](https://buycoffee.to/metin2-playerbots)**: voluntary donations help cover the tools, the test server and the AI models used to develop the project.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Support on buycoffee.to"></a>

Every kind of support helps build a livelier Metin2 world: testing, bug reports, ideas and donations.

---

## 🌟 What the bots do

- ⚔️ **They fight like players**: every class and path (Warrior, Sura, Ninja, Shaman), combos, skill rotations, buffs, archers with arrows, and Metin stones broken from a battle horse. Bots also spar with each other in the villages of their kingdom.
- 🗺️ **Three kingdoms and the whole world**: Shinsoo, Chunjo and Jinno with their own villages, Orc Valley, the Yongbi Desert, Mount Sohan, the Hwang Temple, the Fire Land (Doyyumhwaji), the forests, the Monkey and Spider Dungeons and the Grottoes of Exile. Bots pick the map and the spot by their level and plan their routes over the map's real collision grid.
- 🧠 **Personalities**: Iwakura's personality system with moods. A bot may be a Grinder, a Conqueror, a Gambler, a Perfectionist, a Trader, a companion, a mercenary or a Metin stone dropper, and now and then a rare personality turns up.
- 🛡️ **Guilds, wars and raids**: bots found guilds by strength, fight guild wars in rounds on the guild map (players' guilds included), go to the Demon Tower as a guild, bring down the world's bosses together and go down into the Catacombs after Azrael.
- 🎉 **Events**: Pirate Tanaka, Zuo's Metin rain, Hexagonal Chests from Metin stones and the Halloween event "Night of the Pale Moon", all started from the panel. The bots head there to fight by themselves.
- 🏰 **Dungeons and add-ons**: the Plague Dungeon (levels 45-120) and the Hunter's Forge with weapons for level 50, both off by default and switched on in the panel, Beran-Setaou, and the Snakefield beta with the assault horse.
- 🏪 **A bot market**: real offline shops on the village stalls, prices from Iwakura's price list with inflation, adjusted by demand, and bots buying from the counters of every kingdom. The **Dom Towarowy** (department store) shows every shop's offers in one window, suggests a price when you list an item ("Auto cena") and has a wish list that tells you when someone lists the item you are looking for. The **ItemShop**, paid in Dragon Coins that drop from Metin stones and bosses, also has the Wheel of Fortune and flash auctions.
- 🔨 **Character progression**: the Blacksmith with "Refine up to +N" and materials taken from the collector's warehouse, refine scrolls, bonuses, soul stones, Dragon Stone Alchemy, skill books, Combo up to 30 points, Spirit Stones, the Biologist's missions, quests, the horse up to the battle horse, fishing, mining and herbalism.
- 💬 **Conversations**: bots answer whispers and trade over the chat ("Kupię…" — I buy, "Sprzedam…" — I sell), a Shaman will tell you what its buffs give, and a bot you call will come over. The bots write in the reader's language: Polish, English, German, French, Portuguese, Romanian or Turkish (English for every other client).
- 🤝 **Towarzysz, your companion**: a permanent partner in your party. It fights beside you, buffs you and trades with you, and you set its equipment and skills yourself. It has its own storage and Alchemy, fishes, joins a party a friend leads ("Grupa") and can lead a party itself. While you are away it plays on its own ("Gra beze mnie"): it reads books, shops on the market, refines at the Blacksmith, does the Biologist and quests, and reports back when you return. You can also play it like an ordinary character.
- 🎯 **Auto Hunt**: automatic hunting for the player, with a pick-up filter and a fishing mode. The world's settings decide whether it is for everyone or bought in the ItemShop.
- 🖥️ **The game client**: nine languages (Polish, English, German, French, Spanish, Italian, Portuguese, Romanian and Turkish), an in-game Wiki with drops, chests and guides, the Community window, a GF-style quest window and [AUTO]/[AFK] markers over players.
- 🎛️ **Panels and launcher**: two web panels with a live world map, rankings, the bots' equipment, AI behaviour sliders, timed events, boss respawn times and a database editor; a launcher with updates, world backups, the difficulty setting, extra channels, COOP (friends playing in your world, free) and a server on a VPS.
- 💾 **A persistent world**: every bot has its own account and character in MariaDB, so levels, items and yang survive a restart.

## 🆕 Recently added (versions 2.2.140–2.2.146)

- **Towarzysz** develops on its own while you are away, can lead a party and fishes in a whole new way, with baits and a summary of the catch.
- **Combo** has 30 points, and its three books drop from monsters.
- **Dom Towarowy** has a wish list, and new scrolls, refine materials and books on the bots' counters wait for players first.
- **The Blacksmith** refines automatically up to the plus you choose; the Community window, the quest window and the herbalism guides in the Wiki are new as well.
- **Bots** spar in the villages, break Metin stones as droppers, choose more sensibly where to fight and behave more fairly next to players.
- The full list of changes is in [CHANGELOG.md](CHANGELOG.md).

## ⌨️ In-game keys

| Key | What it does |
|---|---|
| `P` | the companion's window |
| `K` | Auto Hunt |
| `H` | help with the list of keys and commands |
| `X` | the channel window (`X` + `1`–`4` moves you straight to CH1–CH4) |
| `` ` `` (tilde) | pick up every item nearby |
| `F5` | search the offline shops for an item |
| `F9` | the GM panel (GM characters only) |
| `F1`–`F3` | on the login screen: a saved account from the page on show (15 accounts on 5 pages) |

## 🎮 In-game commands (GM)

| Command | Permission | Description | Example |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Administrator | Brings the bot with that ID into the game (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrator | Logs the bot out of the world. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Administrator | Brings a range of bots into the game. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Administrator | Logs a range of bots out. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Everybody | Shows the level ranking of the active bots in the chat. | `/bot_rank` |

## 🗄️ Database access (Navicat, HeidiSQL, DBeaver)

The server's database is MariaDB in a container, reachable **on this computer
only** (`127.0.0.1`, port `3306`, or another one if `M2_DB_PUBLISH_PORT` is set
in `.env`). A new connection in a database client: type MySQL/MariaDB, host
`127.0.0.1`, port `3306`.

| Account | For | Password |
|---|---|---|
| `root` | everything | `M2_DB_ROOT_PASSWORD` in `linux-port\docker\.env` in the server folder |
| `metin2` | the game's databases only (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` in the same file |

The quickest way: the launcher's **DATABASE LOGIN (NAVICAT)** button shows the
host, the port and both passwords in fields to copy. The passwords are drawn at
the first start and there is no "default" one. Do not paste them on Discord.

If the database client answers `1045 - Access denied for user 'root'@'172.18.0.1'`,
the database was created with another password than the one now in `.env`.
Click **REPAIR DATABASE ACCESS**: the launcher stops the server and sets the
`metin2` and `root` accounts to the passwords in `.env`, and characters, items
and bots stay untouched. Then click PLAY and log in again.

## 📜 Licence

From version 2.2.55 the project is under **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.en)** (Attribution-NonCommercial-NoDerivatives):

- **you may** play, run a server for yourself and your friends, and share the official packages unchanged, crediting the author with a link to the project;
- **you may not** distribute modified versions or separate distributions; add-ons to the project may be made and shared on the project's Discord;
- **you may not** make money from it: no selling it, no charging for access to a server, no selling items in the game. Voluntary donations towards running a server that give no advantage in the game, and recording and streaming gameplay, are fine.

Versions up to and including 2.2.16 are under the MIT licence ([LICENSE-MIT.txt](LICENSE-MIT.txt)), and versions 2.2.17 to 2.2.54 under CC BY-NC-SA 4.0. Metin2 belongs to Ymir Interactive and Webzen, and the server-file packages to their authors. Details are in [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

**Metin2 files in the packages.** The project's packages (the full package and the client and server updates) also contain files that are not the work of the project's authors: the Metin2 client and game data, which belong to Ymir Interactive and Webzen, and the files of the server-file packages (r40250 by TMP4, mt2009 by Martysama). The project's licence does not cover them and gives no rights to them. The permission to share the packages unchanged covers only the work of the project's authors.

### Removal requests

If you are a rights holder (Ymir Interactive, Webzen, Gameforge, the author of a server-file package or anyone else) and want something here taken down, [open an issue](https://github.com/TieruYT/metin2-playerbots/issues) in this repository or write on the [project's Discord](https://discord.gg/DyCxXdZtnV). Naming the files or releases is enough. We will remove them promptly, without argument and without the need for a formal DMCA notice, though that is of course possible too.

## 🤝 Credits

- **AzzlackSyndicate**: author of the original Linux port foundation, installers and panel the project grew from (MIT, see [NOTICE.md](NOTICE.md)).
- **Iwakura**: the bots' personality system, the price list, the item tiers, the shop and guild names, the bots' nicknames and the Community Patches.
- **seban latino**: the Metin2 Singleplayer Panel, the second panel in the install, with its live map, profiles, rankings and economy.
- **ĹŌŞƬĒĶ**: the login screen and Discord Rich Presence, the language pack, whispered conversations with the bots, the personality row over a bot and the Wheel of Fortune's disc.
- **Colide**: the Auto Hunt window, the Community window, the companion's storage, offline plan and fishing, and the [AUTO]/[AFK] marker artwork.
- **OskarPWA**: the bot depot window, the skill icons and the first version of the F9 GM panel.
- **Tyrion**: searching the offline shops for one particular item, the Halloween event, and the German, French, Portuguese and Romanian translations.
- **Uxìĕ [DSO]**: the Dom Towarowy, the Cape of Courage pulling monsters from the whole screen, and the inventory side panel.
- **Gibon**: previews of what a chest holds and what a monster drops.
- **Kiciamol**: the target's health on its bar (a player's in a fight too), the F9 GM panel, the in-game Wiki with guides, the Wheel of Fortune, the Hexagonal Chests, Combo up to 30 points and the Snakefield beta.
- **NerrVoVy**: the Plague Dungeon, the Hunter's Forge, the GF-style quest window, the ItemShop's flash auctions, the target's bonus preview and the kingdom flags in the chat.
- **Said (saido_0)**: the Turkish translation and the Herbalist's Path.
- **blasty**: the bonus switcher.
- **Piciu713**: the refine chance in the Blacksmith's window.
- **Kenny, Pabloo, Mur4s**: fixes to party pickups, to bots' errands in a player's party, to the bear quest and to the Teleport Ring.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): research inspiration for autonomous agents in MMO games.
- The Discord community: the tests, bug reports and ideas most of this project came from.
