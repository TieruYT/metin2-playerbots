# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | **Français** | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Site web](https://img.shields.io/badge/Site_web-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Rejoindre_la_communaut%C3%A9-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Soutenir_le_projet-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licence](https://img.shields.io/badge/Licence-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.fr)

Un monde Metin2 solo en local, dans lequel des personnages autonomes (Playerbots) parcourent les cartes et jouent vraiment : ils montent de niveau, combattent seuls et en groupe, font du commerce entre eux, améliorent leur équipement, chassent les pierres Metin et les boss, fondent des guildes et conservent leur progression dans la base de données du serveur.

Les bots ne sont pas des programmes externes. Ce sont des personnages à part entière, pilotés par une IA à l'intérieur du moteur du serveur : un joueur voit leurs déplacements, leurs combats, leurs compétences et leur équipement exactement comme ceux des personnages des autres joueurs.

> [!IMPORTANT]
> **Depuis la version 2.2.17, le code source du projet n'est plus publié dans ce dépôt.** Vous y trouverez les versions avec leurs paquets de mise à jour, le journal des modifications ([CHANGELOG.md](CHANGELOG.md)) et les fichiers que lisent le launcher et les panneaux. Les versions jusqu'à la 2.2.16 incluse ont été publiées sous licence MIT et leur historique reste dans le dépôt. Les versions plus récentes sont partagées sous licence [CC BY-NC-ND 4.0](LICENSE).

## 📥 Comment jouer

1. Téléchargez le **paquet complet** (serveur et client dans une seule archive) sur le serveur Discord du projet : [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Installez Docker Desktop et attendez qu'il affiche « Engine running ».
3. Décompressez le paquet dans un dossier ordinaire, par exemple `C:\Games\Metin2 Singleplayer` (pas le Bureau ni les Documents synchronisés par OneDrive), puis lancez `Serwer\Metin2-Launcher-GUI.bat`.
4. Quand le launcher propose une mise à jour, choisissez **Update everything** (tout mettre à jour) : le paquet complet est plus ancien que la dernière version.
5. Cliquez sur **1. INSTALL / PREPARE** (installer / préparer), puis sur **2. PLAY** (jouer). Le premier démarrage assemble le serveur dans Docker à partir de fichiers prêts à l'emploi et prend quelques minutes ; les suivants ne prennent qu'un instant.

> [!NOTE]
> **Depuis la version 2.2.39, le serveur est la version protégée.** Il est livré sous forme de fichiers serveur prêts à l'emploi, sans code source et sans rien compiler sur votre PC, donc l'installation et les mises à jour prennent moins de temps. Une installation plus ancienne et ordinaire passe d'elle-même à la version protégée lors de sa prochaine mise à jour (le launcher l'annonce dans son journal avec un compte à rebours de 10 secondes), et le monde, les personnages et les réglages sont conservés. Les fichiers du jeu que vous pouvez modifier (drops, quêtes, cartes) se trouvent désormais dans `Serwer\linux-port\docker\game\share\locale\poland\`, et les mises à jour ne les écrasent pas. Le coffre Clair de lune fait exception : enregistrez le vôtre dans `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copiez `share-add\special_item_group.moonlight.txt` depuis ce dossier et gardez la ligne `Vnum 50011`).

Au démarrage, le launcher propose de lui-même une nouvelle version du serveur ou du client, et **CHECK FOR UPDATES** (rechercher des mises à jour) en cherche une à tout moment. Les paquets proviennent [des versions publiées dans ce dépôt](https://github.com/TieruYT/metin2-playerbots/releases). Un serveur sous Linux se met à jour avec `sh linux-port/tools/update.sh` dans le dossier du serveur, et un serveur sur un VPS peut être installé et mis à jour depuis le launcher (**SERVER ON A VPS**, serveur sur un VPS). Sur un VPS aussi, le serveur est assemblé à partir de fichiers prêts à l'emploi, sans rien compiler.

Un bug ? Le bouton **REPORT A BUG** (signaler un bug) du launcher nous envoie directement votre description avec les journaux. Les idées et suggestions vont sur le canal des propositions sur Discord.

La configuration requise, un guide pas à pas et les réponses aux questions fréquentes se trouvent sur [mt2sp.com](https://mt2sp.com) et dans les canaux d'aide sur Discord.

## 💬 Communauté et soutien au projet

- **[Site du projet — mt2sp.com](https://mt2sp.com)** : ce qu'est le projet, le guide d'installation et la FAQ, en polonais, anglais, allemand et turc.
- **[Serveur Discord](https://discord.gg/DyCxXdZtnV)** : le paquet complet, l'aide, les rapports de bugs, les idées et les nouvelles des nouvelles versions.
- **[Soutien sur buycoffee.to](https://buycoffee.to/metin2-playerbots)** : les dons volontaires aident à couvrir les outils, le serveur de test et les modèles d'IA utilisés pour développer le projet.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Soutenir sur buycoffee.to"></a>

Toute forme de soutien aide à bâtir un monde Metin2 plus vivant : les tests, les rapports de bugs, les idées et les dons.

---

## 🌟 Ce que font les bots

- ⚔️ **Ils combattent comme des joueurs** : toutes les classes et toutes les voies (Guerrier, Sura, Ninja, Chamane), les combos, les rotations de compétences, les buffs, les archers avec leurs flèches, et les pierres Metin brisées depuis un cheval de combat. Les bots s'affrontent aussi en duel amical dans les villages de leur royaume.
- 🗺️ **Trois royaumes et le monde entier** : Shinsoo, Chunjo et Jinno avec leurs propres villages, la Vallée des Orcs, le Désert de Yongbi, le Mont Sohan, le Temple Hwang, la Terre de Feu (Doyyumhwaji), les forêts, les Donjons des Singes et des Araignées et les Grottes de l'Exil. Les bots choisissent la carte et l'endroit selon leur niveau et planifient leurs trajets sur la véritable grille de collision de la carte.
- 🧠 **Personnalités** : le système de personnalités d'Iwakura, avec des humeurs. Un bot peut être un Farmeur (Grinder), un Conquérant, un Parieur, un Perfectionniste, un Marchand, un compagnon, un mercenaire ou un dropper de pierres Metin, et de temps en temps une personnalité rare fait son apparition.
- 🛡️ **Guildes, guerres et raids** : les bots fondent des guildes selon leur force, mènent des guerres de guilde en manches sur la carte de guilde (y compris contre les guildes des joueurs), montent en guilde dans la Tour des Démons, abattent ensemble les boss du monde et descendent dans les Catacombes à la poursuite d'Azrael.
- 🎉 **Événements** : le pirate Tanaka, la pluie de Metin de Zuo, les Coffres hexagonaux des pierres Metin et l'événement d'Halloween « La Nuit de la Lune pâle », tous lancés depuis le panneau. Les bots s'y rendent d'eux-mêmes pour combattre.
- 🏰 **Donjons et extensions** : le Donjon de la Peste (niveaux 45-120) et la Forge du Chasseur avec des armes pour le niveau 50, tous deux désactivés par défaut et activables dans le panneau, Beran-Setaou, ainsi que la bêta de Snakefield avec le cheval d'assaut.
- 🏪 **Un marché des bots** : de vraies boutiques hors ligne sur les étals des villages, des prix tirés de la liste de prix d'Iwakura avec inflation, ajustés selon la demande, et des bots qui achètent sur les comptoirs de tous les royaumes. Le **Dom Towarowy** (grand magasin) affiche les offres de toutes les boutiques dans une seule fenêtre, suggère un prix quand vous mettez un objet en vente (« Auto cena » — prix automatique) et dispose d'une liste de souhaits qui vous prévient quand quelqu'un met en vente l'objet que vous cherchez. L'**ItemShop**, payé en Pièces du Dragon qui tombent des pierres Metin et des boss, propose aussi la Roue de la Fortune et des ventes flash.
- 🔨 **Progression du personnage** : le Forgeron avec « Améliorer jusqu'à +N » et les matériaux pris dans l'entrepôt du collectionneur, les parchemins d'amélioration, les bonus, les pierres d'âme, l'Alchimie des pierres du Dragon, les livres de compétences, le Combo jusqu'à 30 points, les Pierres spirituelles, les missions du Biologiste, les quêtes, le cheval jusqu'au cheval de combat, la pêche, l'exploitation minière et l'herboristerie.
- 💬 **Conversations** : les bots répondent aux chuchotements et commercent via le chat (« Kupię… » — j'achète, « Sprzedam… » — je vends), un Chamane vous dira ce que donnent ses buffs, et un bot que vous appelez viendra vous rejoindre. Les bots écrivent dans la langue du lecteur : polonais, anglais, allemand, français, portugais, roumain ou turc (anglais pour tous les autres clients).
- 🤝 **Towarzysz, votre compagnon** : un partenaire permanent dans votre groupe. Il combat à vos côtés, vous donne des buffs et commerce avec vous, et c'est vous qui réglez son équipement et ses compétences. Il a son propre entrepôt et sa propre Alchimie, pêche, rejoint un groupe mené par un ami (« Grupa » — groupe) et peut lui-même mener un groupe. Pendant votre absence, il joue seul (« Gra beze mnie » — joue sans moi) : il lit des livres, fait ses achats sur le marché, améliore son équipement chez le Forgeron, s'occupe du Biologiste et des quêtes, et vous fait un compte rendu à votre retour. Vous pouvez aussi le jouer comme un personnage ordinaire.
- 🎯 **Chasse auto** : la chasse automatique pour le joueur, avec un filtre de ramassage et un mode pêche. Les réglages du monde décident si elle est accessible à tous ou achetée dans l'ItemShop.
- 🖥️ **Le client du jeu** : neuf langues (polonais, anglais, allemand, français, espagnol, italien, portugais, roumain et turc), un Wiki en jeu avec les drops, les coffres et des guides, la fenêtre Communauté, une fenêtre de quêtes façon GF et des marqueurs [AUTO]/[AFK] au-dessus des joueurs.
- 🎛️ **Panneaux et launcher** : deux panneaux web avec une carte du monde en direct, des classements, l'équipement des bots, des curseurs de comportement de l'IA, des événements programmés, les temps de réapparition des boss et un éditeur de base de données ; un launcher avec les mises à jour, les sauvegardes du monde, le réglage de la difficulté, des canaux supplémentaires, le COOP (des amis qui jouent dans votre monde, gratuitement) et un serveur sur un VPS.
- 💾 **Un monde persistant** : chaque bot a son propre compte et son propre personnage dans MariaDB, si bien que les niveaux, les objets et les yangs survivent à un redémarrage.

## 🆕 Ajouts récents (versions 2.2.140–2.2.146)

- **Towarzysz** progresse seul pendant votre absence, peut mener un groupe et pêche d'une toute nouvelle façon, avec des appâts et un résumé des prises.
- Le **Combo** a 30 points, et ses trois livres tombent des monstres.
- Le **Dom Towarowy** a une liste de souhaits, et les nouveaux parchemins, matériaux d'amélioration et livres sur les comptoirs des bots attendent d'abord les joueurs.
- **Le Forgeron** améliore automatiquement jusqu'au + que vous choisissez ; la fenêtre Communauté, la fenêtre de quêtes et les guides d'herboristerie du Wiki sont également nouveaux.
- **Les bots** s'affrontent en duel amical dans les villages, brisent des pierres Metin en tant que droppers, choisissent plus judicieusement où combattre et se comportent plus loyalement à côté des joueurs.
- La liste complète des modifications se trouve dans [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Touches en jeu

| Touche | Ce qu'elle fait |
|---|---|
| `P` | la fenêtre du compagnon |
| `K` | Chasse auto |
| `H` | l'aide avec la liste des touches et des commandes |
| `X` | la fenêtre des canaux (`X` + `1`–`4` vous envoie directement sur CH1–CH4) |
| `` ` `` (tilde) | ramasser tous les objets à proximité |
| `F5` | chercher un objet dans les boutiques hors ligne |
| `F9` | le panneau GM (personnages GM uniquement) |
| `F1`–`F3` | sur l'écran de connexion : un compte enregistré de la page affichée (15 comptes sur 5 pages) |

## 🎮 Commandes en jeu (GM)

| Commande | Permission | Description | Exemple |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Administrateur | Fait entrer dans le jeu le bot portant cet ID (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrateur | Déconnecte le bot du monde. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Administrateur | Fait entrer dans le jeu une plage de bots. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Administrateur | Déconnecte une plage de bots. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Tout le monde | Affiche dans le chat le classement par niveau des bots actifs. | `/bot_rank` |

## 🗄️ Accès à la base de données (Navicat, HeidiSQL, DBeaver)

La base de données du serveur est MariaDB dans un conteneur, accessible **uniquement
depuis cet ordinateur** (`127.0.0.1`, port `3306`, ou un autre si `M2_DB_PUBLISH_PORT`
est défini dans `.env`). Nouvelle connexion dans un client de base de données : type
MySQL/MariaDB, hôte `127.0.0.1`, port `3306`.

| Compte | Pour | Mot de passe |
|---|---|---|
| `root` | tout | `M2_DB_ROOT_PASSWORD` dans `linux-port\docker\.env` du dossier du serveur |
| `metin2` | uniquement les bases du jeu (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` dans le même fichier |

Le plus rapide : le bouton **DATABASE LOGIN (NAVICAT)** du launcher affiche l'hôte,
le port et les deux mots de passe dans des champs à copier. Les mots de passe sont
générés au premier démarrage et il n'existe pas de mot de passe « par défaut ».
Ne les collez pas sur Discord.

Si le client de base de données répond `1045 - Access denied for user 'root'@'172.18.0.1'`,
la base a été créée avec un autre mot de passe que celui qui se trouve maintenant dans
`.env`. Cliquez sur **REPAIR DATABASE ACCESS** (réparer l'accès à la base) : le launcher
arrête le serveur et règle les comptes `metin2` et `root` sur les mots de passe de `.env`,
sans toucher aux personnages, aux objets ni aux bots. Cliquez ensuite sur PLAY et
reconnectez-vous.

## 📜 Licence

Depuis la version 2.2.55, le projet est sous licence **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.fr)** (Attribution - Pas d'Utilisation Commerciale - Pas de Modification) :

- **vous pouvez** jouer, faire tourner un serveur pour vous et vos amis, et partager les paquets officiels sans modification, en créditant l'auteur avec un lien vers le projet ;
- **vous ne pouvez pas** distribuer de versions modifiées ni de distributions séparées ; des extensions au projet peuvent être créées et partagées sur le Discord du projet ;
- **vous ne pouvez pas** en tirer de l'argent : pas de vente, pas d'accès payant à un serveur, pas de vente d'objets en jeu. Les dons volontaires pour faire tourner un serveur qui ne donnent aucun avantage en jeu, ainsi que l'enregistrement et la diffusion en direct de parties, sont autorisés.

Les versions jusqu'à la 2.2.16 incluse sont sous licence MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), et les versions 2.2.17 à 2.2.54 sous CC BY-NC-SA 4.0. Metin2 appartient à Ymir Interactive et Webzen, et les paquets de fichiers serveur à leurs auteurs. Les détails se trouvent dans [LICENSE](LICENSE) et [NOTICE.md](NOTICE.md).

**Fichiers de Metin2 dans les paquets.** Les paquets du projet (le paquet complet et les mises à jour du client et du serveur) contiennent aussi des fichiers qui ne sont pas l'œuvre des auteurs du projet : le client Metin2 et les données du jeu, qui appartiennent à Ymir Interactive et Webzen, ainsi que les fichiers des paquets de fichiers serveur (r40250 de TMP4, mt2009 de Martysama). La licence du projet ne les couvre pas et ne confère aucun droit sur eux. L'autorisation de partager les paquets sans modification ne couvre que l'œuvre des auteurs du projet.

### Demandes de retrait

Si vous êtes titulaire de droits (Ymir Interactive, Webzen, Gameforge, l'auteur d'un paquet de fichiers serveur ou toute autre personne) et souhaitez que quelque chose soit retiré d'ici, [ouvrez une issue](https://github.com/TieruYT/metin2-playerbots/issues) dans ce dépôt ou écrivez sur le [Discord du projet](https://discord.gg/DyCxXdZtnV). Il suffit d'indiquer les fichiers ou les versions concernés. Nous les retirerons rapidement, sans discussion et sans qu'une notification DMCA formelle soit nécessaire, même si celle-ci reste bien entendu possible.

## 🤝 Remerciements

- **AzzlackSyndicate** : auteur de la base du portage Linux d'origine, des installateurs et du panneau dont le projet est issu (MIT, voir [NOTICE.md](NOTICE.md)).
- **Iwakura** : le système de personnalités des bots, la liste de prix, les paliers d'objets, les noms des boutiques et des guildes, les pseudos des bots et les Community Patches.
- **seban latino** : le Metin2 Singleplayer Panel, le deuxième panneau de l'installation, avec sa carte en direct, ses profils, ses classements et son économie.
- **ĹŌŞƬĒĶ** : l'écran de connexion et la Rich Presence Discord, le pack de langues, les conversations chuchotées avec les bots, la ligne de personnalité au-dessus d'un bot et le disque de la Roue de la Fortune.
- **Colide** : la fenêtre de la Chasse auto, la fenêtre Communauté, l'entrepôt, le plan hors ligne et la pêche du compagnon, ainsi que les illustrations des marqueurs [AUTO]/[AFK].
- **OskarPWA** : la fenêtre du dépôt des bots, les icônes des compétences et la première version du panneau GM sur F9.
- **Tyrion** : la recherche d'un objet précis dans les boutiques hors ligne, l'événement d'Halloween, et les traductions allemande, française, portugaise et roumaine.
- **Uxìĕ [DSO]** : le Dom Towarowy, la Cape de Bravoure qui attire les monstres de tout l'écran, et le panneau latéral de l'inventaire.
- **Gibon** : l'aperçu du contenu d'un coffre et de ce que lâche un monstre.
- **Kiciamol** : la santé de la cible sur sa barre (y compris celle d'un joueur en combat), le panneau GM sur F9, le Wiki en jeu avec ses guides, la Roue de la Fortune, les Coffres hexagonaux, le Combo jusqu'à 30 points et la bêta de Snakefield.
- **NerrVoVy** : le Donjon de la Peste, la Forge du Chasseur, la fenêtre de quêtes façon GF, les ventes flash de l'ItemShop, l'aperçu des bonus de la cible et les drapeaux des royaumes dans le chat.
- **Said (saido_0)** : la traduction turque et la Voie de l'Herboriste.
- **blasty** : le changeur de bonus.
- **Piciu713** : la chance d'amélioration dans la fenêtre du Forgeron.
- **Kenny, Pabloo, Mur4s** : des corrections du ramassage en groupe, des tâches des bots dans le groupe d'un joueur, de la quête de l'ours et de l'Anneau de téléportation.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) : une source d'inspiration pour la recherche sur les agents autonomes dans les jeux MMO.
- La communauté Discord : les tests, les rapports de bugs et les idées dont est née la plus grande partie de ce projet.
