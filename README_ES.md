# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | **Español** | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Sitio web](https://img.shields.io/badge/Sitio_web-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-%C3%9Anete_a_la_comunidad-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Apoya_el_proyecto-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licencia](https://img.shields.io/badge/Licencia-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.es)

Un mundo de Metin2 para un solo jugador en local, en el que personajes autónomos (Playerbots) recorren los mapas y juegan de verdad: suben de nivel, luchan solos y en grupo, comercian entre ellos, mejoran su equipo, cazan piedras Metin y jefes, fundan gremios y guardan su progreso en la base de datos del servidor.

Los bots no son programas externos. Son personajes de pleno derecho controlados por una IA dentro del motor del servidor, así que un jugador ve sus movimientos, sus combates, sus habilidades y su equipo exactamente igual que los de los personajes de otros jugadores.

> [!IMPORTANT]
> **Desde la versión 2.2.17, el código fuente del proyecto no se publica en este repositorio.** Aquí encontrarás las versiones con sus paquetes de actualización, el registro de cambios ([CHANGELOG.md](CHANGELOG.md)) y los archivos que leen el launcher y los paneles. Las versiones hasta la 2.2.16 inclusive se publicaron bajo la licencia MIT y su historial sigue en el repositorio. Las versiones más recientes se comparten bajo la licencia [CC BY-NC-ND 4.0](LICENSE).

## 📥 Cómo jugar

1. Descarga el **paquete completo** (servidor y cliente en un solo archivo) desde el servidor de Discord del proyecto: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Instala Docker Desktop y espera a que muestre «Engine running».
3. Descomprime el paquete en una carpeta normal, por ejemplo `C:\Games\Metin2 Singleplayer` (no en el Escritorio ni en Documentos sincronizados por OneDrive), y ejecuta `Serwer\Metin2-Launcher-GUI.bat`.
4. Cuando el launcher ofrezca una actualización, elige **Update everything** (actualizar todo): el paquete completo es más antiguo que la última versión.
5. Haz clic en **1. INSTALL / PREPARE** (instalar / preparar) y después en **2. PLAY** (jugar). El primer arranque monta el servidor en Docker a partir de archivos ya preparados y tarda unos minutos; los siguientes, solo un momento.

> [!NOTE]
> **Desde la versión 2.2.39, el servidor es la versión protegida.** Viene como archivos de servidor ya preparados, sin código fuente y sin compilar nada en tu PC, así que la instalación y las actualizaciones llevan menos tiempo. Una instalación más antigua y normal pasa sola a la versión protegida con su siguiente actualización (el launcher lo indica en su registro con una cuenta atrás de 10 segundos), y el mundo, los personajes y los ajustes se conservan. Los archivos del juego que puedes editar (drops, misiones, mapas) están ahora en `Serwer\linux-port\docker\game\share\locale\poland\`, y las actualizaciones no los sobrescriben. El cofre Luz de Luna es la excepción: guarda el tuyo en `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copia `share-add\special_item_group.moonlight.txt` de esa carpeta y conserva la línea `Vnum 50011`).

El launcher ofrece por sí solo una nueva versión del servidor o del cliente al arrancar, y **CHECK FOR UPDATES** (buscar actualizaciones) busca una en cualquier momento. Los paquetes vienen de [las versiones publicadas en este repositorio](https://github.com/TieruYT/metin2-playerbots/releases). Un servidor en Linux se actualiza con `sh linux-port/tools/update.sh` en la carpeta del servidor, y un servidor en un VPS se puede instalar y actualizar desde el launcher (**SERVER ON A VPS**, servidor en un VPS). También en un VPS el servidor se monta a partir de archivos ya preparados, sin compilar nada.

¿Un error? El botón **REPORT A BUG** (informar de un error) del launcher nos envía tu descripción junto con los registros directamente. Las ideas y sugerencias, al canal de propuestas en Discord.

Los requisitos, una guía paso a paso y las respuestas a las preguntas frecuentes están en [mt2sp.com](https://mt2sp.com) y en los canales de ayuda de Discord.

## 💬 Comunidad y apoyo al proyecto

- **[Web del proyecto — mt2sp.com](https://mt2sp.com)**: qué es el proyecto, la guía de instalación y las preguntas frecuentes, en polaco, inglés, alemán y turco.
- **[Servidor de Discord](https://discord.gg/DyCxXdZtnV)**: el paquete completo, ayuda, informes de errores, ideas y novedades sobre las nuevas versiones.
- **[Apoyo en buycoffee.to](https://buycoffee.to/metin2-playerbots)**: las donaciones voluntarias ayudan a cubrir las herramientas, el servidor de pruebas y los modelos de IA que se usan para desarrollar el proyecto.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Apoya en buycoffee.to"></a>

Cualquier tipo de apoyo ayuda a construir un mundo de Metin2 más vivo: las pruebas, los informes de errores, las ideas y las donaciones.

---

## 🌟 Qué hacen los bots

- ⚔️ **Luchan como jugadores**: todas las clases y caminos (Guerrero, Sura, Ninja, Chamán), combos, rotaciones de habilidades, buffs, arqueros con flechas y piedras Metin destruidas desde un caballo de batalla. Los bots también hacen duelos amistosos entre ellos en las aldeas de su reino.
- 🗺️ **Tres reinos y el mundo entero**: Shinsoo, Chunjo y Jinno con sus propias aldeas, el Valle de los Orcos, el Desierto de Yongbi, el Monte Sohan, el Templo Hwang, la Tierra de Fuego (Doyyumhwaji), los bosques, las Mazmorras de los Monos y de las Arañas y las Grutas del Exilio. Los bots eligen el mapa y el lugar según su nivel y planifican sus rutas sobre la verdadera rejilla de colisiones del mapa.
- 🧠 **Personalidades**: el sistema de personalidades de Iwakura, con estados de ánimo. Un bot puede ser un Farmeador (Grinder), un Conquistador, un Apostador, un Perfeccionista, un Comerciante, un compañero, un mercenario o un dropper de piedras Metin, y de vez en cuando aparece una personalidad rara.
- 🛡️ **Gremios, guerras y raids**: los bots fundan gremios según su fuerza, libran guerras de gremio por rondas en el mapa del gremio (también contra gremios de jugadores), suben como gremio a la Torre de los Demonios, derriban juntos a los jefes del mundo y bajan a las Catacumbas en busca de Azrael.
- 🎉 **Eventos**: el pirata Tanaka, la lluvia de Metin de Zuo, los Cofres hexagonales de las piedras Metin y el evento de Halloween «La Noche de la Luna Pálida», todos iniciados desde el panel. Los bots acuden solos a luchar.
- 🏰 **Mazmorras y complementos**: la Mazmorra de la Peste (niveles 45-120) y la Forja del Cazador con armas para el nivel 50, ambas desactivadas por defecto y activables en el panel, Beran-Setaou y la beta de Snakefield con el caballo de asalto.
- 🏪 **Un mercado de bots**: tiendas offline de verdad en los puestos de las aldeas, precios de la lista de precios de Iwakura con inflación, ajustados según la demanda, y bots que compran en los mostradores de todos los reinos. El **Dom Towarowy** (grandes almacenes) muestra las ofertas de todas las tiendas en una sola ventana, sugiere un precio cuando pones un objeto a la venta («Auto cena» — precio automático) y tiene una lista de deseos que te avisa cuando alguien pone a la venta el objeto que buscas. La **ItemShop**, que se paga con Monedas del Dragón que caen de las piedras Metin y de los jefes, también tiene la Rueda de la Fortuna y subastas flash.
- 🔨 **Progreso del personaje**: el Herrero con «Mejorar hasta +N» y los materiales tomados del almacén del coleccionista, pergaminos de mejora, bonus, piedras de alma, la Alquimia de las Piedras del Dragón, libros de habilidad, el Combo hasta 30 puntos, Piedras espirituales, las misiones del Biólogo, misiones, el caballo hasta el caballo de batalla, la pesca, la minería y la herboristería.
- 💬 **Conversaciones**: los bots responden a los susurros y comercian por el chat («Kupię…» — compro, «Sprzedam…» — vendo), un Chamán te dirá qué dan sus buffs y un bot al que llames vendrá hacia ti. Los bots escriben en el idioma del lector: polaco, inglés, alemán, francés, portugués, rumano o turco (inglés para cualquier otro cliente).
- 🤝 **Towarzysz, tu compañero**: un compañero permanente en tu grupo. Lucha a tu lado, te da buffs y comercia contigo, y tú mismo eliges su equipo y sus habilidades. Tiene su propio almacén y su propia Alquimia, pesca, se une a un grupo que lidera un amigo («Grupa» — grupo) y puede liderar un grupo él mismo. Mientras no estás, juega por su cuenta («Gra beze mnie» — juega sin mí): lee libros, compra en el mercado, mejora en el Herrero, hace el Biólogo y misiones, y te cuenta lo que ha hecho cuando vuelves. También puedes jugarlo como un personaje normal.
- 🎯 **Caza automática**: caza automática para el jugador, con un filtro de recogida y un modo de pesca. Los ajustes del mundo deciden si es para todos o se compra en la ItemShop.
- 🖥️ **El cliente del juego**: nueve idiomas (polaco, inglés, alemán, francés, español, italiano, portugués, rumano y turco), una Wiki dentro del juego con drops, cofres y guías, la ventana Comunidad, una ventana de misiones al estilo GF y marcadores [AUTO]/[AFK] sobre los jugadores.
- 🎛️ **Paneles y launcher**: dos paneles web con un mapa del mundo en directo, clasificaciones, el equipo de los bots, controles deslizantes del comportamiento de la IA, eventos programados, tiempos de reaparición de los jefes y un editor de la base de datos; un launcher con actualizaciones, copias de seguridad del mundo, el ajuste de dificultad, canales adicionales, COOP (amigos jugando en tu mundo, gratis) y un servidor en un VPS.
- 💾 **Un mundo persistente**: cada bot tiene su propia cuenta y su propio personaje en MariaDB, así que los niveles, los objetos y el yang sobreviven a un reinicio.

## 🆕 Novedades recientes (versiones 2.2.140–2.2.146)

- **Towarzysz** progresa por su cuenta mientras no estás, puede liderar un grupo y pesca de una forma totalmente nueva, con cebos y un resumen de las capturas.
- El **Combo** tiene 30 puntos, y sus tres libros caen de los monstruos.
- El **Dom Towarowy** tiene una lista de deseos, y los nuevos pergaminos, materiales de mejora y libros en los mostradores de los bots esperan primero a los jugadores.
- **El Herrero** mejora automáticamente hasta el + que elijas; la ventana Comunidad, la ventana de misiones y las guías de herboristería de la Wiki también son nuevas.
- **Los bots** hacen duelos amistosos en las aldeas, destruyen piedras Metin como droppers, eligen con más sensatez dónde luchar y se comportan de forma más justa junto a los jugadores.
- La lista completa de cambios está en [CHANGELOG.md](CHANGELOG.md).

## ⌨️ Teclas en el juego

| Tecla | Qué hace |
|---|---|
| `P` | la ventana del compañero |
| `K` | Caza automática |
| `H` | ayuda con la lista de teclas y comandos |
| `X` | la ventana de canales (`X` + `1`–`4` te lleva directamente a CH1–CH4) |
| `` ` `` (tilde) | recoger todos los objetos cercanos |
| `F5` | buscar un objeto en las tiendas offline |
| `F9` | el panel de GM (solo personajes GM) |
| `F1`–`F3` | en la pantalla de inicio de sesión: una cuenta guardada de la página visible (15 cuentas en 5 páginas) |

## 🎮 Comandos en el juego (GM)

| Comando | Permiso | Descripción | Ejemplo |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | Administrador | Hace entrar en el juego al bot con ese ID (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrador | Desconecta al bot del mundo. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | Administrador | Hace entrar en el juego a un rango de bots. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Administrador | Desconecta a un rango de bots. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Todos | Muestra en el chat la clasificación por nivel de los bots activos. | `/bot_rank` |

## 🗄️ Acceso a la base de datos (Navicat, HeidiSQL, DBeaver)

La base de datos del servidor es MariaDB en un contenedor, accesible **solo desde
este ordenador** (`127.0.0.1`, puerto `3306`, u otro si `M2_DB_PUBLISH_PORT` está
definido en `.env`). Nueva conexión en un cliente de bases de datos: tipo
MySQL/MariaDB, host `127.0.0.1`, puerto `3306`.

| Cuenta | Para | Contraseña |
|---|---|---|
| `root` | todo | `M2_DB_ROOT_PASSWORD` en `linux-port\docker\.env` dentro de la carpeta del servidor |
| `metin2` | solo las bases de datos del juego (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` en el mismo archivo |

La forma más rápida: el botón **DATABASE LOGIN (NAVICAT)** del launcher muestra el
host, el puerto y las dos contraseñas en campos para copiar. Las contraseñas se
generan al azar en el primer arranque y no existe ninguna «por defecto». No las
pegues en Discord.

Si el cliente de bases de datos responde `1045 - Access denied for user 'root'@'172.18.0.1'`,
la base de datos se creó con una contraseña distinta de la que hay ahora en `.env`.
Haz clic en **REPAIR DATABASE ACCESS** (reparar el acceso a la base de datos): el
launcher detiene el servidor y pone a las cuentas `metin2` y `root` las contraseñas
de `.env`, sin tocar los personajes, los objetos ni los bots. Después haz clic en PLAY
y vuelve a iniciar sesión.

## 📜 Licencia

Desde la versión 2.2.55, el proyecto está bajo la licencia **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.es)** (Atribución-NoComercial-SinDerivadas):

- **puedes** jugar, mantener un servidor para ti y tus amigos, y compartir los paquetes oficiales sin modificar, citando al autor con un enlace al proyecto;
- **no puedes** distribuir versiones modificadas ni distribuciones separadas; se pueden crear complementos para el proyecto y compartirlos en el Discord del proyecto;
- **no puedes** ganar dinero con él: nada de venderlo, nada de cobrar por el acceso a un servidor, nada de vender objetos del juego. Las donaciones voluntarias para mantener un servidor que no den ninguna ventaja en el juego, así como grabar y retransmitir partidas, están permitidas.

Las versiones hasta la 2.2.16 inclusive están bajo la licencia MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), y las versiones 2.2.17 a 2.2.54 bajo CC BY-NC-SA 4.0. Metin2 pertenece a Ymir Interactive y Webzen, y los paquetes de archivos de servidor a sus autores. Los detalles están en [LICENSE](LICENSE) y [NOTICE.md](NOTICE.md).

**Archivos de Metin2 en los paquetes.** Los paquetes del proyecto (el paquete completo y las actualizaciones del cliente y del servidor) también contienen archivos que no son obra de los autores del proyecto: el cliente de Metin2 y los datos del juego, que pertenecen a Ymir Interactive y Webzen, y los archivos de los paquetes de archivos de servidor (r40250 de TMP4, mt2009 de Martysama). La licencia del proyecto no los cubre y no otorga ningún derecho sobre ellos. El permiso para compartir los paquetes sin modificar cubre únicamente la obra de los autores del proyecto.

### Solicitudes de retirada

Si eres titular de derechos (Ymir Interactive, Webzen, Gameforge, el autor de un paquete de archivos de servidor o cualquier otra persona) y quieres que se retire algo de aquí, [abre una issue](https://github.com/TieruYT/metin2-playerbots/issues) en este repositorio o escribe en el [Discord del proyecto](https://discord.gg/DyCxXdZtnV). Basta con indicar los archivos o las versiones. Los retiraremos con prontitud, sin discusión y sin necesidad de una notificación DMCA formal, aunque por supuesto también es posible.

## 🤝 Créditos

- **AzzlackSyndicate**: autor de la base original del port para Linux, los instaladores y el panel de los que nació el proyecto (MIT, ver [NOTICE.md](NOTICE.md)).
- **Iwakura**: el sistema de personalidades de los bots, la lista de precios, los niveles de los objetos, los nombres de las tiendas y de los gremios, los apodos de los bots y los Community Patches.
- **seban latino**: el Metin2 Singleplayer Panel, el segundo panel de la instalación, con su mapa en directo, perfiles, clasificaciones y economía.
- **ĹŌŞƬĒĶ**: la pantalla de inicio de sesión y la Rich Presence de Discord, el paquete de idiomas, las conversaciones por susurro con los bots, la fila de personalidad sobre un bot y el disco de la Rueda de la Fortuna.
- **Colide**: la ventana de la Caza automática, la ventana Comunidad, el almacén, el plan offline y la pesca del compañero, y el diseño de los marcadores [AUTO]/[AFK].
- **OskarPWA**: la ventana del depósito de los bots, los iconos de las habilidades y la primera versión del panel de GM en F9.
- **Tyrion**: la búsqueda de un objeto concreto en las tiendas offline, el evento de Halloween y las traducciones al alemán, francés, portugués y rumano.
- **Uxìĕ [DSO]**: el Dom Towarowy, la Capa del Valor que atrae a los monstruos de toda la pantalla y el panel lateral del inventario.
- **Gibon**: la vista previa de lo que contiene un cofre y de lo que suelta un monstruo.
- **Kiciamol**: la vida del objetivo en su barra (también la de un jugador en combate), el panel de GM en F9, la Wiki del juego con guías, la Rueda de la Fortuna, los Cofres hexagonales, el Combo hasta 30 puntos y la beta de Snakefield.
- **NerrVoVy**: la Mazmorra de la Peste, la Forja del Cazador, la ventana de misiones al estilo GF, las subastas flash de la ItemShop, la vista previa de los bonus del objetivo y las banderas de los reinos en el chat.
- **Said (saido_0)**: la traducción al turco y el Camino del Herbolario.
- **blasty**: el cambiador de bonus.
- **Piciu713**: la probabilidad de mejora en la ventana del Herrero.
- **Kenny, Pabloo, Mur4s**: correcciones de la recogida de objetos en grupo, de los encargos de los bots en el grupo de un jugador, de la misión del oso y del Anillo de Teletransporte.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspiración para la investigación sobre agentes autónomos en juegos MMO.
- La comunidad de Discord: las pruebas, los informes de errores y las ideas de las que salió la mayor parte de este proyecto.
