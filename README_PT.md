# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | **Português** | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Site](https://img.shields.io/badge/Site-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Entra_na_comunidade-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Oferece_um_café-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licença](https://img.shields.io/badge/Licença-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.pt)

> Este ficheiro é uma tradução de [README.md](README.md). Em caso de diferenças, prevalece a versão polaca.

Um mundo local de Metin2 singleplayer em que personagens autónomas (Playerbots) correm pelos mapas e jogam a sério: sobem de nível, lutam sozinhas e em grupo, negoceiam entre si, melhoram o equipamento, caçam pedras Metin e bosses, fundam guildas e guardam o seu progresso na base de dados do servidor.

Os bots não são programas externos. São personagens de pleno direito, controladas por uma IA dentro do motor do servidor, por isso o jogador vê o seu movimento, combate, habilidades e equipamento tal como vê as personagens dos outros jogadores.

> [!IMPORTANT]
> **A partir da versão 2.2.17, o código-fonte do projeto não é publicado neste repositório.** Aqui encontras as versões (releases) com os pacotes de atualização, a lista de alterações ([CHANGELOG.md](CHANGELOG.md)) e os ficheiros usados pelo launcher e pelos painéis. As versões até à 2.2.16, inclusive, foram publicadas sob a licença MIT e o seu histórico permanece no repositório. As versões mais recentes são disponibilizadas sob a licença [CC BY-NC-ND 4.0](LICENSE).

## 📥 Como jogar

Os nomes dos botões aparecem abaixo tal como o launcher os mostra (em polaco), com o significado em português entre parênteses.

1. Descarrega o **pacote completo** (servidor e cliente num só arquivo) no servidor de Discord do projeto: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Instala o Docker Desktop e espera até que mostre “Engine running”.
3. Extrai o pacote para uma pasta normal, por exemplo `C:\Jogos\Metin2 Singleplayer` (não para o Ambiente de Trabalho nem para os Documentos sincronizados com o OneDrive), e executa `Serwer\Metin2-Launcher-GUI.bat`.
4. Quando o launcher propuser uma atualização, escolhe **Aktualizuj wszystko** (Atualizar tudo): o pacote completo é mais antigo do que a versão mais recente.
5. Clica em **1. ZAINSTALUJ / PRZYGOTUJ** (Instalar / Preparar) e depois em **2. GRAJ** (Jogar). O primeiro arranque monta o servidor no Docker a partir de ficheiros prontos e demora alguns minutos; os seguintes demoram um instante.

> [!NOTE]
> **Desde a versão 2.2.39, o servidor está na versão protegida.** Chega como ficheiros de servidor prontos, sem código-fonte e sem compilação no teu computador, por isso a instalação e as atualizações demoram menos. Uma instalação mais antiga e normal passa sozinha para a versão protegida na atualização seguinte (o launcher indica-o no registo com uma contagem decrescente de 10 segundos), e o mundo, as personagens e as definições mantêm-se. Os ficheiros do jogo que podes editar (drops, quests, mapas) estão agora em `Serwer\linux-port\docker\game\share\locale\poland\`, e as atualizações não os substituem. A exceção é o Baú do Luar: guarda o teu em `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copia `share-add\special_item_group.moonlight.txt` dessa pasta e mantém a linha `Vnum 50011`).

O launcher pergunta sozinho por uma nova versão do servidor ou do cliente ao arrancar. Também a podes procurar com o botão **SPRAWDZ AKTUALIZACJE** (Procurar atualizações). Os pacotes são descarregados das [versões neste repositório](https://github.com/TieruYT/metin2-playerbots/releases). Um servidor em Linux atualiza-se com o comando `sh linux-port/tools/update.sh` na pasta do servidor, e um servidor num VPS pode ser criado e atualizado a partir do launcher (**SERWER NA VPS** — servidor num VPS). Também no VPS o servidor é montado a partir de ficheiros prontos, sem compilação.

Um erro? O botão **ZGŁOŚ BŁĄD** (Comunicar um erro) no launcher envia a tua descrição, juntamente com os registos, diretamente para nós. Ideias e propostas envia-as no Discord, no canal de propostas.

Os requisitos, as instruções passo a passo e as respostas às perguntas frequentes estão em [mt2sp.com](https://mt2sp.com) e nos canais de ajuda do Discord.

## 💬 Comunidade e apoio ao projeto

- **[Site do projeto — mt2sp.com](https://mt2sp.com)**: descrição do projeto, instruções de instalação e FAQ, em polaco, inglês, alemão e turco.
- **[Servidor de Discord](https://discord.gg/DyCxXdZtnV)**: o pacote completo para descarregar, ajuda, comunicação de erros, ideias e informações sobre novas versões.
- **[Apoio no buycoffee.to](https://buycoffee.to/metin2-playerbots)**: os donativos voluntários ajudam a cobrir os custos das ferramentas, do servidor de testes e dos modelos de IA usados no desenvolvimento do projeto.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Oferece um café no buycoffee.to"></a>

Qualquer forma de apoio ajuda a criar um mundo de Metin2 cada vez mais vivo: testes, comunicação de erros, ideias e donativos.

---

## 🌟 O que os bots sabem fazer

- ⚔️ **Combate como os jogadores**: todas as classes e caminhos (Guerreiro, Sura, Ninja, Xamã), combos, rotações de habilidades, buffs, arqueiros com flechas e pedras Metin partidas do cavalo de batalha. Os bots também fazem combates de treino entre si nas aldeias do seu reino.
- 🗺️ **Três reinos e o mundo inteiro**: Shinsoo, Chunjo e Jinno com as suas próprias aldeias, o Vale dos Orcs, o Deserto de Yongbi, o Monte Sohan, o Templo de Hwang, a Terra do Fogo (Doyyumhwan), as florestas, as Masmorras dos Macacos e das Aranhas e as Grutas do Exílio. Os bots escolhem o mapa e o local de acordo com o seu nível e calculam as rotas sobre a verdadeira grelha de colisões do mapa.
- 🧠 **Personalidades**: o sistema de personalidades de Iwakura, com estados de espírito. Os bots podem ser Grinders, Conquistadores, Apostadores, Perfeccionistas, Comerciantes, companheiros, mercenários ou droppers de pedras Metin, e de vez em quando aparece uma personalidade rara.
- 🛡️ **Guildas, guerras e raids**: os bots fundam guildas de acordo com a sua força, travam guerras de guilda por rondas no mapa da guilda (também contra guildas de jogadores), partem com a guilda inteira para a Torre dos Demónios, derrotam juntos os bosses do mundo e descem às Catacumbas do Diabo atrás de Azrael.
- 🎉 **Eventos**: o Pirata Tanaka, a chuva de Metins de Zuo, os Baús Hexagonais das pedras Metin e o evento de Halloween “Noite da Lua Pálida”, todos iniciados a partir do painel. Os bots vão sozinhos para lá combater.
- 🏰 **Masmorras e extras**: a Masmorra da Peste (níveis 45-120) e a Forja do Caçador com armas para o nível 50, ambas desligadas por predefinição e ativadas no painel, Beran-Setaou e a beta do Snakefield com o cavalo de assalto.
- 🏪 **Mercado dos bots**: verdadeiras lojas offline nas bancas das aldeias, preços da tabela de preços de Iwakura com inflação, ajustados pela procura, e bots que compram nas bancas de todos os reinos. O **Dom Towarowy** (grandes armazéns; no cliente português “Feira da ladra”) mostra numa só janela as ofertas de todas as lojas, sugere um preço quando pões um item à venda (“Auto cena” — preço automático) e tem uma lista de desejos que te avisa quando alguém põe à venda o item que procuras. A **ItemShop**, paga com Moedas do Dragão que caem das pedras Metin e dos bosses, tem também a Roda da Sorte e leilões relâmpago.
- 🔨 **Desenvolvimento da personagem**: o Ferreiro com “Melhorar até +N” e materiais tirados do armazém do colecionador, os pergaminhos, bónus, pedras espirituais, a Alquimia das Pedras do Dragão, livros de habilidade, o Combo até 30 pontos, Pedras de alma, as missões do Biólogo, quests, o cavalo até ao cavalo de batalha, a pesca, a mineração e a herborística.
- 💬 **Conversas**: os bots respondem a sussurros, negoceiam pelo chat (“Kupię…” — compro, “Sprzedam…” — vendo), o Xamã diz-te o que dão os seus buffs, e um bot chamado vem ter contigo. Os bots escrevem na língua de quem lê: polaco, inglês, alemão, francês, português, romeno ou turco (inglês para qualquer outro cliente).
- 🤝 **Towarzysz (Companheiro)**: o teu parceiro permanente no grupo. Luta ao teu lado, dá-te buffs, negoceia contigo, e és tu quem define o seu equipamento e as suas habilidades. Tem o seu próprio armazém e a sua própria Alquimia, pesca, entra num grupo liderado por um amigo (“Grupa” — Grupo) e pode ele próprio liderar um grupo. Quando não estás, joga sozinho (“Gra beze mnie” — Joga sem mim): lê livros, faz compras no mercado, melhora itens no Ferreiro, faz as missões do Biólogo e quests, e conta-te tudo quando voltas. Também o podes jogar como uma personagem normal.
- 🎯 **Auto Łowy (Caça auto)**: caça automática para o jogador, com filtro de recolha e modo de pesca. Nas definições do mundo escolhes se é para todos ou se é comprada na ItemShop.
- 🖥️ **O cliente do jogo**: nove idiomas (polaco, inglês, alemão, francês, espanhol, italiano, português, romeno e turco), uma Wiki no jogo com drops, baús e guias, a janela Comunidade, uma janela de missões ao estilo GF e marcadores [AUTO]/[AFK] por cima dos jogadores.
- 🎛️ **Painéis e launcher**: dois painéis web com o mapa do mundo em tempo real, classificações, o equipamento dos bots, controlos deslizantes do comportamento da IA, eventos programados, tempos de respawn dos bosses e um editor da base de dados; um launcher com atualizações, cópia de segurança do mundo, nível de dificuldade, canais extra, COOP (jogar com amigos no teu mundo, de graça) e servidor num VPS.
- 💾 **Mundo persistente**: cada bot tem a sua própria conta e personagem na base de dados MariaDB, por isso o nível, os itens e o yang mantêm-se depois de um reinício.

## 🆕 Novidades recentes (versões 2.2.140–2.2.146)

- O **Towarzysz** evolui sozinho enquanto não estás, pode liderar um grupo e pesca de uma forma totalmente nova, com iscos e um resumo da pescaria.
- O **Combo** tem 30 pontos, e os seus três livros caem dos monstros.
- O **Dom Towarowy** tem uma lista de desejos, e os novos pergaminhos, materiais de melhoria e livros nas bancas dos bots esperam primeiro pelos jogadores.
- **O Ferreiro** melhora automaticamente até ao + que escolheres; a janela Comunidade, a janela de missões e os guias de herborística na Wiki também são novos.
- **Os bots** fazem combates de treino nas aldeias, partem pedras Metin como droppers, escolhem com mais critério onde combater e comportam-se de forma mais justa ao lado dos jogadores.
- A lista completa de alterações está em [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Atalhos no jogo

| Tecla | Ação |
|---|---|
| `P` | janela do Towarzysz (Companheiro) |
| `K` | Caça auto |
| `H` | ajuda com a lista de teclas e comandos |
| `X` | janela dos canais (`X` + `1`–`4` leva-te diretamente para CH1–CH4) |
| `` ` `` (til) | apanhar todos os itens por perto |
| `F5` | pesquisa de itens nas lojas offline |
| `F9` | painel de GM (apenas personagens GM) |
| `F1`–`F3` | no ecrã de início de sessão: conta guardada da página visível (15 contas em 5 páginas) |

## 🎮 Comandos no jogo (GM)

| Comando | Permissões | Descrição | Exemplo |
|---|---|---|---|
| `/bot_spawn <id> <reino: 1-3>` | Administrador | Põe em jogo o bot com o ID indicado (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrador | Retira o bot do mundo (termina a sessão). | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <quantidade> <reino>` | Administrador | Põe em jogo um grupo de bots. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <quantidade>` | Administrador | Retira um grupo de bots do mundo. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Todos | Mostra no chat a classificação de níveis dos bots ativos. | `/bot_rank` |

## 🗄️ Acesso à base de dados (Navicat, HeidiSQL, DBeaver)

A base de dados do servidor é MariaDB num contentor, acessível **apenas neste computador**
(`127.0.0.1`, porta `3306` ou outra, se em `.env` estiver definido `M2_DB_PUBLISH_PORT`).
Nova ligação no cliente de base de dados: tipo MySQL/MariaDB, host `127.0.0.1`, porta `3306`.

| Conta | Para quê | Palavra-passe |
|---|---|---|
| `root` | tudo | `M2_DB_ROOT_PASSWORD` no ficheiro `linux-port\docker\.env` na pasta do servidor |
| `metin2` | apenas as bases de dados do jogo (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` no mesmo ficheiro |

O mais rápido: o botão **DANE DO BAZY (NAVICAT)** (Dados de acesso à base de dados) no launcher mostra o host,
a porta e ambas as palavras-passe em campos para copiar. As palavras-passe são geradas aleatoriamente no primeiro
arranque e não existe nenhuma palavra-passe “predefinida”. Não as coles no Discord.

Se o cliente de base de dados responder `1045 - Access denied for user 'root'@'172.18.0.1'`,
a base de dados foi criada com uma palavra-passe diferente da que está agora em `.env`.
Clica em **NAPRAW DOSTEP DO BAZY** (Reparar o acesso à base de dados): o launcher para o servidor e define as contas
`metin2` e `root` com as palavras-passe de `.env`, e as personagens, os itens e os bots ficam
intactos. Depois clica em GRAJ (Jogar) e inicia sessão de novo.

## 📜 Licença

A partir da versão 2.2.55, o projeto está sob a licença **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.pt)** (Atribuição-NãoComercial-SemDerivações):

- **é permitido** jogar, manter um servidor para ti e para os teus amigos e partilhar os pacotes oficiais sem alterações, indicando o autor e o link para o projeto;
- **não é permitido** distribuir versões modificadas nem distribuições separadas; podem criar-se extras (add-ons) para o projeto e partilhá-los no Discord do projeto;
- **não é permitido** ganhar dinheiro com ele: vendê-lo, cobrar pelo acesso a um servidor ou vender itens no jogo. Os donativos voluntários para a manutenção de um servidor, se não derem vantagens no jogo, bem como gravar e transmitir (stream) a jogabilidade, são aceitáveis.

As versões até à 2.2.16, inclusive, estão sob a licença MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), e as versões da 2.2.17 à 2.2.54 sob a CC BY-NC-SA 4.0. Metin2 pertence à Ymir Interactive e à Webzen, e os pacotes de ficheiros de servidor aos seus autores. Os pormenores estão nos ficheiros [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md).

**Ficheiros do Metin2 nos pacotes.** Os pacotes do projeto (o pacote completo e as atualizações do cliente e do servidor) contêm também ficheiros que não são obra dos autores do projeto: o cliente e os dados do jogo Metin2, que pertencem à Ymir Interactive e à Webzen, e os ficheiros dos pacotes de ficheiros de servidor (r40250 de TMP4, mt2009 de Martysama). A licença do projeto não os abrange e não dá quaisquer direitos sobre eles. A permissão para partilhar os pacotes sem alterações diz respeito apenas à obra dos autores do projeto.

### Pedidos de remoção

Se és titular de direitos (Ymir Interactive, Webzen, Gameforge, o autor de um pacote de ficheiros de servidor ou qualquer outra pessoa) e queres que algo seja retirado daqui, [abre uma issue](https://github.com/TieruYT/metin2-playerbots/issues) neste repositório ou escreve no [Discord do projeto](https://discord.gg/DyCxXdZtnV). Basta indicar os ficheiros ou as versões. Retirá-los-emos sem demora, sem discussão e sem necessidade de uma notificação DMCA formal, embora esta também seja, naturalmente, possível.

## 🤝 Agradecimentos

- **AzzlackSyndicate**: autor da base original do port para Linux, dos instaladores e do painel sobre os quais o projeto nasceu (licença MIT, ver [NOTICE.md](NOTICE.md)).
- **Iwakura**: o sistema de personalidades dos bots, a tabela de preços, os níveis (tiers) dos itens, os nomes das lojas e das guildas, os nomes dos bots e os Community Patches.
- **seban latino**: o Metin2 Singleplayer Panel, o segundo painel da instalação, com mapa em tempo real, perfis, classificações e economia.
- **ĹŌŞƬĒĶ**: o ecrã de início de sessão e o Discord Rich Presence, o pacote de idioma, as conversas com os bots por sussurros, a linha da personalidade por cima do bot e o disco da Roda da Sorte.
- **Colide**: a janela da Caça auto, a janela Comunidade, o armazém, o plano offline e a pesca do companheiro, e os gráficos dos marcadores [AUTO]/[AFK].
- **OskarPWA**: a janela do armazém do bot, os ícones das habilidades e a primeira versão do painel de GM no F9.
- **Tyrion**: a pesquisa de um item concreto nas lojas offline, o evento de Halloween e as traduções para alemão, francês, português e romeno.
- **Uxìĕ [DSO]**: o Dom Towarowy, a Capa da bravura, que atrai os monstros de todo o ecrã, e o painel lateral do inventário.
- **Gibon**: a pré-visualização do conteúdo dos baús e do drop dos monstros.
- **Kiciamol**: a vida do alvo na barra (também a de um jogador em combate), o painel de GM no F9, a Wiki no jogo com guias, a Roda da Sorte, os Baús Hexagonais, o Combo até 30 pontos e a beta do Snakefield.
- **NerrVoVy**: a Masmorra da Peste, a Forja do Caçador, a janela de missões ao estilo GF, os leilões relâmpago da ItemShop, a pré-visualização dos bónus do alvo e as bandeiras dos reinos no chat.
- **Said (saido_0)**: a tradução turca e o Caminho do Herbalista.
- **blasty**: o alternador de bónus.
- **Piciu713**: a probabilidade de melhoria na janela do Ferreiro.
- **Kenny, Pabloo, Mur4s**: correções na recolha de itens em grupo, nas tarefas dos bots no grupo de um jogador, na missão dos ursos e no Anel de teletransporte.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspiração para a investigação sobre agentes autónomos em jogos MMO.
- A comunidade do Discord: os testes, a comunicação de erros e as ideias de onde nasceu a maior parte deste projeto.
