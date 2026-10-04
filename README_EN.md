# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | **English**

[![Website](https://img.shields.io/badge/Website-metin2singleplayer.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://metin2singleplayer.com)
[![Discord](https://img.shields.io/badge/Discord-Join_Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Support_the_Project-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licence-CC_BY--NC--SA_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en)

A local Metin2 singleplayer world in which autonomous characters (Playerbots) run across the maps and genuinely play: they level up, fight solo and in parties, trade with each other, refine their gear, hunt Metin stones and bosses, found guilds and keep their progress in the server's database.

The bots are not external programs. They are first-class characters driven by AI inside the server engine, so a player sees their movement, combat, skills and equipment exactly as they see other players' characters.

> [!IMPORTANT]
> **From version 2.2.17 the project's source code is not published in this repository.** Here you will find the releases with their update packages, the changelog ([CHANGELOG.md](CHANGELOG.md)) and the files the launcher and the panels read. Versions up to and including 2.2.16 were released under the MIT licence and their history stays in the repository. Newer versions are shared under the [CC BY-NC-SA 4.0](LICENSE) licence.

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

Requirements, a step-by-step guide and answers to common questions are on [metin2singleplayer.com](https://metin2singleplayer.com) and in the help channels on Discord.

## 💬 Community & Project Support

- **[Project website — metin2singleplayer.com](https://metin2singleplayer.com)**: what the project is, the install guide and the FAQ, in Polish and English.
- **[Discord server](https://discord.gg/DyCxXdZtnV)**: the full package, help, bug reports, ideas and news about new versions.
- **[Support on buycoffee.to](https://buycoffee.to/metin2-playerbots)**: voluntary donations help cover the tools, the test server and the AI models used to develop the project.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Support on buycoffee.to"></a>

Every kind of support helps build a livelier Metin2 world: testing, bug reports, ideas and donations.

---

## 🌟 What the bots do

- ⚔️ **They fight like players**: every class and path (Warrior, Sura, Ninja, Shaman), combos, skill rotations, buffs, archers with arrows, and Metin stones broken from a battle horse.
- 🗺️ **Three kingdoms and the whole world**: Shinsoo, Chunjo and Jinno with their own villages, Orc Valley, the Yongbi Desert, Mount Sohan, the Hwang Temple, the Fire Land (Doyyumhwaji), the forests, and the Monkey and Spider Dungeons. Bots pick the map and the spot by their level and plan their routes over the map's real collision grid.
- 🧠 **Personalities**: Iwakura's personality system with moods. A bot may be a Grinder, a Conqueror, a Gambler, a Perfectionist, a Trader, a companion or a mercenary, and now and then a rare personality turns up.
- 🛡️ **Guilds, wars and raids**: bots found guilds by strength, fight guild wars in rounds on the guild map (players' guilds included), go to the Demon Tower as a guild, bring down the world's bosses together and go down into the Catacombs after Azrael.
- 🎉 **Events**: Pirate Tanaka and Zuo's Metin rain with announcements and rewards, and the bots head there to fight by themselves.
- 🏪 **A bot market**: real offline shops on the village stalls, prices from Iwakura's price list with inflation, adjusted by demand, and purchases between bots. The **Dom Towarowy** shows every shop's offers in one window and suggests the bots' price when you list an item (with an "Auto cena" option). The game also has an **ItemShop** for Dragon Coins, which drop from Metin stones and bosses.
- 🔨 **Character progression**: the Blacksmith and refine scrolls, bonuses, soul stones, skill books and Spirit Stones, the Biologist's missions, the horse up to the battle horse, fishing, mining and herbalism.
- 💬 **Conversations**: bots answer whispers and trade over the chat ("Kupię…", "Sprzedam…"), a Shaman will tell you what its buffs give, and a bot you call will come over. The bots talk in Polish.
- 🤝 **Towarzysz, your companion**: a permanent partner in your party. It fights beside you, buffs you and trades with you, and you set its equipment and skills yourself. It also joins a party a friend leads ("Grupa") and can play on its own while you are away ("Gra beze mnie").
- 🎯 **Auto Hunt**: automatic hunting for the player. The world's settings decide whether it is for everyone or bought in the ItemShop.
- 🎛️ **Panels and launcher**: two web panels with a live world map, rankings, the bots' equipment, AI behaviour sliders and timed events; a launcher with updates, world backups, the difficulty setting, a second channel, COOP (friends playing in your world, free) and a server on a VPS.
- 💾 **A persistent world**: every bot has its own account and character in MariaDB, so levels, items and yang survive a restart.

## ⌨️ In-game keys

| Key | What it does |
|---|---|
| `P` | the companion's window |
| `K` | Auto Hunt |
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

## 🤝 Credits

- **AzzlackSyndicate**: author of the original Linux port foundation, installers and panel the project grew from (MIT, see [NOTICE.md](NOTICE.md)).
- **Iwakura**: the bots' personality system, the price list, the item tiers, the shop and guild names, the bots' nicknames and the Community Patches.
- **seban latino**: the Metin2 Singleplayer Panel, the second panel in the install, with its live map, profiles, rankings and economy.
- **ĹŌŞƬĒĶ**: the login screen and Discord Rich Presence, the language pack, whispered conversations with the bots and the personality row over a bot.
- **Colide**: the Auto Hunt window.
- **OskarPWA**: the bot depot window, the skill icons and the F9 GM panel.
- **Tyrion**: searching the offline shops for one particular item.
- **Uxìĕ [DSO]**: the Dom Towarowy and the Cape of Courage pulling monsters from the whole screen.
- **Gibon**: previews of what a chest holds and what a monster drops.
- **Kiciamol**: the target's health on its bar, a player's in a fight too.
- **Piciu713**: the refine chance in the Blacksmith's window.
- **Kenny, Pabloo, Mur4s**: fixes to party pickups, to bots' errands in a player's party, to the bear quest and to the Teleport Ring.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): research inspiration for autonomous agents in MMO games.
- The Discord community: the tests, bug reports and ideas most of this project came from.
