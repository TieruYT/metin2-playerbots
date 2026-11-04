# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | [English (README_EN.md)](README_EN.md) | [Deutsch (README_DE.md)](README_DE.md) | **Română** | [Português (README_PT.md)](README_PT.md) | [Türkçe (README_TR.md)](README_TR.md)

[![Site](https://img.shields.io/badge/Site-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Intră_în_comunitate-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Oferă_o_cafea-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Licență](https://img.shields.io/badge/Licență-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.ro)

> Acest fișier este o traducere a [README.md](README.md). Dacă între ele există diferențe, este valabilă versiunea poloneză.

O lume locală Metin2 singleplayer în care personaje autonome (Playerbots) aleargă pe hărți și chiar joacă: cresc în nivel, luptă singure și în grupuri, fac comerț între ele, își îmbunătățesc echipamentul, vânează pietre Metin și boși, întemeiază bresle și își salvează progresul în baza de date a serverului.

Boții nu sunt programe externe. Sunt personaje cu drepturi depline, conduse de o inteligență artificială din interiorul motorului serverului, așa că jucătorul le vede mișcarea, lupta, abilitățile și echipamentul la fel ca personajele altor jucători.

> [!IMPORTANT]
> **Începând cu versiunea 2.2.17, codul sursă al proiectului nu mai este publicat în acest depozit.** Aici găsești versiunile (releases) cu pachetele de actualizare, lista modificărilor ([CHANGELOG.md](CHANGELOG.md)) și fișierele folosite de launcher și de panouri. Versiunile până la 2.2.16 inclusiv au fost publicate sub licența MIT, iar istoricul lor rămâne în depozit. Versiunile mai noi sunt puse la dispoziție sub licența [CC BY-NC-ND 4.0](LICENSE).

## 📥 Cum se joacă

Numele butoanelor sunt scrise mai jos așa cum apar în launcher (în poloneză), iar sensul lor în română este în paranteză.

1. Descarcă **pachetul complet** (server și client într-o singură arhivă) de pe serverul de Discord al proiectului: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Instalează Docker Desktop și așteaptă până când afișează „Engine running”.
3. Dezarhivează pachetul într-un folder obișnuit, de exemplu `C:\Jocuri\Metin2 Singleplayer` (nu pe Desktop și nici în Documentele sincronizate cu OneDrive), și pornește `Serwer\Metin2-Launcher-GUI.bat`.
4. Când launcherul propune o actualizare, alege **Aktualizuj wszystko** (Actualizează tot): pachetul complet este mai vechi decât cea mai nouă versiune.
5. Apasă **1. ZAINSTALUJ / PRZYGOTUJ** (Instalează / Pregătește), apoi **2. GRAJ** (Joacă). Prima pornire asamblează serverul în Docker din fișiere gata făcute și durează câteva minute, următoarele durează o clipă.

> [!NOTE]
> **Începând cu versiunea 2.2.39, serverul este în versiunea protejată.** Vine ca fișiere de server gata făcute, fără cod sursă și fără compilare pe calculatorul tău, așa că instalarea și actualizările durează mai puțin. O instalare mai veche, obișnuită, trece singură la versiunea protejată la următoarea actualizare (launcherul spune asta în jurnalul său, cu o numărătoare inversă de 10 secunde), iar lumea, personajele și setările rămân. Fișierele jocului pe care le poți edita (drop-uri, questuri, hărți) se află acum în `Serwer\linux-port\docker\game\share\locale\poland\`, iar actualizările nu le suprascriu. Excepția este Cufărul Lumina Lunii: salvează-l pe al tău în `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` (copiază `share-add\special_item_group.moonlight.txt` din acel folder și păstrează linia `Vnum 50011`).

Despre o versiune nouă a serverului sau a clientului launcherul te întreabă singur la pornire. O poți verifica și cu butonul **SPRAWDZ AKTUALIZACJE** (Caută actualizări). Pachetele se descarcă din [versiunile din acest depozit](https://github.com/TieruYT/metin2-playerbots/releases). Un server pe Linux îl actualizezi cu comanda `sh linux-port/tools/update.sh` în folderul serverului, iar un server pe VPS îl poți crea și actualiza din launcher (**SERWER NA VPS** — server pe VPS). Și pe VPS serverul se asamblează din fișiere gata făcute, fără compilare.

O eroare? Butonul **ZGŁOŚ BŁĄD** (Raportează o eroare) din launcher ne trimite descrierea ta împreună cu jurnalele direct nouă. Ideile și propunerile le trimiți pe Discord, în canalul de propuneri.

Cerințele, instrucțiunile pas cu pas și răspunsurile la întrebările frecvente se găsesc pe [mt2sp.com](https://mt2sp.com) și în canalele de ajutor de pe Discord.

## 💬 Comunitatea și sprijinul pentru proiect

- **[Site-ul proiectului — mt2sp.com](https://mt2sp.com)**: descrierea proiectului, instrucțiunile de instalare și FAQ, în poloneză, engleză, germană și turcă.
- **[Serverul de Discord](https://discord.gg/DyCxXdZtnV)**: pachetul complet de descărcat, ajutor, raportarea erorilor, idei și informații despre versiunile noi.
- **[Sprijin pe buycoffee.to](https://buycoffee.to/metin2-playerbots)**: donațiile voluntare ajută la acoperirea costurilor uneltelor, ale serverului de test și ale modelelor AI folosite la dezvoltarea proiectului.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="Oferă o cafea pe buycoffee.to"></a>

Orice formă de sprijin ajută la crearea unei lumi Metin2 tot mai vii: teste, raportări de erori, idei și donații.

---

## 🌟 Ce pot face boții

- ⚔️ **Luptă ca jucătorii**: toate clasele și căile (Războinic, Sura, Ninja, Șaman), combo-uri, rotații de abilități, buffuri, arcași cu săgeți și pietre Metin sparte de pe calul de luptă.
- 🗺️ **Trei regate și întreaga lume**: Shinsoo, Chunjo și Jinno cu satele lor, Valea Orcilor, Deșertul Yongbi, Muntele Sohan, Templul Hwang, Ținutul Focului (Doyyumhwan), pădurile, precum și Temnițele Maimuțelor și ale Păianjenilor. Boții își aleg harta și locul după nivel, iar traseele le calculează pe grila reală de coliziuni a hărții.
- 🧠 **Personalități**: sistemul de personalități al lui Iwakura, cu stări de spirit. Boții sunt uneori Grinderi, Cuceritori, Jucători de noroc, Perfecționiști, Negustori, însoțitori și mercenari, iar câteodată apare o personalitate rară.
- 🛡️ **Bresle, războaie și raiduri**: boții întemeiază bresle după putere, poartă războaie de breaslă pe runde pe harta breslei (și cu breslele jucătorilor), pornesc cu toată breasla spre Turnul Demonilor, înving împreună boșii lumii și coboară în Catacomba Diavolului după Azrael.
- 🎉 **Evenimente**: Piratul Tanaka și ploaia de Metine a lui Zuo, cu anunțuri și recompense, iar boții pornesc singuri la luptă.
- 🏪 **Piața boților**: magazine offline adevărate la tarabele din sate, prețuri din lista de prețuri a lui Iwakura, cu inflație, corectate după cerere, și cumpărături între boți. **Dom Towarowy** (în clientul românesc „Talcioc”) arată într-o singură fereastră ofertele tuturor magazinelor, iar când pui un obiect la vânzare îți sugerează prețul boților (cu opțiunea „Auto cena” — preț automat). În joc există și un **ItemShop** pentru Monede Dragon, care cad din pietrele Metin și din boși.
- 🔨 **Dezvoltarea personajului**: Fierarul și pergamentele, bonusuri, pietre spirituale, cărți de abilitate și Pietre de suflet, misiunile Biologului, calul până la calul de luptă, pescuitul, mineritul și culesul plantelor.
- 💬 **Conversații**: boții răspund la șoapte, fac comerț prin chat („Kupię…” — cumpăr, „Sprzedam…” — vând), Șamanul îți spune ce dau buffurile lui, iar un bot chemat vine la tine.
- 🤝 **Towarzysz (Tovarăș)**: partenerul tău permanent din grup. Luptă lângă tine, îți dă buffuri, face comerț cu tine, iar echipamentul și abilitățile lui le setezi singur. Se alătură și unui grup condus de un prieten („Grupa” — Grup) și poate juca singur când nu ești acolo („Gra beze mnie” — Joacă fără mine).
- 🎯 **Auto Łowy (Vânătoare automată)**: vânătoare automată pentru jucător. În setările lumii alegi dacă este pentru oricine sau după cumpărare în ItemShop.
- 🎛️ **Panouri și launcher**: două panouri web cu harta lumii în timp real, clasamente, echipamentul boților, glisoare pentru comportamentul AI și evenimente programate; launcher cu actualizări, copie de siguranță a lumii, nivel de dificultate, al doilea canal, COOP (joc cu prietenii în lumea ta, gratuit) și server pe VPS.
- 💾 **Lume persistentă**: fiecare bot are propriul cont și propriul personaj în baza de date MariaDB, așa că nivelul, obiectele și yang-ul rămân după o repornire.

## ⌨️ Scurtături în joc

| Tastă | Acțiune |
|---|---|
| `P` | fereastra Towarzysz (Tovarăș) |
| `K` | Vânătoare automată |
| `` ` `` (tildă) | ridică toate obiectele din apropiere |
| `F5` | căutarea obiectelor în magazinele offline |
| `F9` | panoul GM (doar personajele GM) |
| `F1`–`F3` | pe ecranul de autentificare: contul salvat de pe pagina vizibilă (15 conturi pe 5 pagini) |

## 🎮 Comenzi în joc (GM)

| Comandă | Permisiuni | Descriere | Exemplu |
|---|---|---|---|
| `/bot_spawn <id> <regat: 1-3>` | Administrator | Aduce în joc botul cu ID-ul dat (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Administrator | Deconectează botul din lume. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <număr> <regat>` | Administrator | Aduce în joc un grup de boți. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <număr>` | Administrator | Deconectează un grup de boți. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Toți | Arată în chat clasamentul nivelurilor boților activi. | `/bot_rank` |

## 🗄️ Accesul la baza de date (Navicat, HeidiSQL, DBeaver)

Baza de date a serverului este MariaDB într-un container, accesibilă **doar pe acest calculator**
(`127.0.0.1`, portul `3306` sau altul, dacă în `.env` este setat `M2_DB_PUBLISH_PORT`).
Conexiune nouă în clientul de baze de date: tip MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Cont | Pentru ce | Parolă |
|---|---|---|
| `root` | tot | `M2_DB_ROOT_PASSWORD` în fișierul `linux-port\docker\.env` din folderul serverului |
| `metin2` | doar bazele de date ale jocului (`account`, `player`, `log`, `common`, `hotbackup`) | `M2_DB_PASSWORD` în același fișier |

Cel mai repede: butonul **DANE DO BAZY (NAVICAT)** (Date de acces la baza de date) din launcher arată hostul,
portul și ambele parole în câmpuri de copiat. Parolele sunt generate aleatoriu la prima
pornire și nu există nicio parolă „implicită”. Nu le lipi pe Discord.

Dacă clientul de baze de date răspunde `1045 - Access denied for user 'root'@'172.18.0.1'`,
baza de date a fost creată cu o altă parolă decât cea care se află acum în `.env`.
Apasă **NAPRAW DOSTEP DO BAZY** (Repară accesul la baza de date): launcherul oprește serverul și setează conturile
`metin2` și `root` la parolele din `.env`, iar personajele, obiectele și boții rămân
neatinse. Apoi apasă GRAJ (Joacă) și autentifică-te din nou.

## 📜 Licență

Începând cu versiunea 2.2.55, proiectul este sub licența **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.ro)** (Atribuire - Necomercial - Fără lucrări derivate):

- **este permis** să joci, să rulezi un server pentru tine și prietenii tăi și să distribui pachetele oficiale nemodificate, cu menționarea autorului și un link către proiect;
- **nu este permis** să distribui modificări sau distribuții separate; suplimentele (add-on-urile) pentru proiect pot fi create și împărtășite pe Discordul proiectului;
- **nu este permis** să câștigi bani din el: să-l vinzi, să ceri taxe pentru accesul la server sau să vinzi obiecte în joc. Donațiile voluntare pentru întreținerea serverului, dacă nu oferă avantaje în joc, precum și înregistrarea și transmiterea în direct (stream) a jocului sunt în regulă.

Versiunile până la 2.2.16 inclusiv sunt sub licența MIT ([LICENSE-MIT.txt](LICENSE-MIT.txt)), iar versiunile de la 2.2.17 la 2.2.54 sub CC BY-NC-SA 4.0. Metin2 aparține Ymir Interactive și Webzen, iar pachetele de fișiere de server aparțin autorilor lor. Detaliile se află în fișierele [LICENSE](LICENSE) și [NOTICE.md](NOTICE.md).

**Fișierele Metin2 din pachete.** Pachetele proiectului (pachetul complet și actualizările clientului și ale serverului) conțin și fișiere care nu sunt munca autorilor proiectului: clientul și datele jocului Metin2, care aparțin Ymir Interactive și Webzen, precum și fișierele pachetelor de fișiere de server (r40250 de TMP4, mt2009 de Martysama). Licența proiectului nu le acoperă și nu oferă niciun drept asupra lor. Permisiunea de a distribui pachetele nemodificate se referă doar la munca autorilor proiectului.

### Cereri de eliminare

Dacă ești deținătorul unor drepturi (Ymir Interactive, Webzen, Gameforge, autorul unui pachet de fișiere de server sau oricine altcineva) și vrei ca ceva de aici să fie eliminat, [deschide un issue](https://github.com/TieruYT/metin2-playerbots/issues) în acest repository sau scrie pe [Discordul proiectului](https://discord.gg/DyCxXdZtnV). Este suficient să indici fișierele sau versiunile. Le vom elimina imediat, fără discuții și fără să fie nevoie de o notificare DMCA formală, deși și aceasta este, desigur, posibilă.

## 🤝 Mulțumiri

- **AzzlackSyndicate**: autorul bazei originale a portului pentru Linux, al instalatoarelor și al panoului pe care s-a construit proiectul (licența MIT, vezi [NOTICE.md](NOTICE.md)).
- **Iwakura**: sistemul de personalități al boților, lista de prețuri, nivelurile (tier-urile) obiectelor, numele magazinelor și ale breslelor, numele boților și Community Patch-urile.
- **seban latino**: Metin2 Singleplayer Panel, al doilea panou din instalare, cu hartă în timp real, profiluri, clasamente și economie.
- **ĹŌŞƬĒĶ**: ecranul de autentificare și Discord Rich Presence, pachetul de limbă, conversațiile cu boții prin șoapte și rândul cu personalitatea de deasupra botului.
- **Colide**: fereastra Vânătorii automate.
- **OskarPWA**: fereastra depozitului botului, iconițele abilităților și panoul GM de sub F9.
- **Tyrion**: căutarea unui anumit obiect în magazinele offline.
- **Uxìĕ [DSO]**: Dom Towarowy și Mantia Curajului, care atrage monștrii de pe tot ecranul.
- **Gibon**: previzualizarea conținutului cuferelor și a drop-ului monștrilor.
- **Kiciamol**: viața țintei pe bară, inclusiv a unui jucător în luptă.
- **Piciu713**: șansa de îmbunătățire în fereastra Fierarului.
- **Kenny, Pabloo, Mur4s**: corecturi la ridicarea obiectelor în grup, la sarcinile boților din grupul unui jucător, la questul urșilor și la Inelul Teleportare.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): inspirație pentru cercetarea agenților autonomi în jocurile MMO.
- Comunitatea de pe Discord: testele, raportările de erori și ideile din care s-a născut cea mai mare parte a acestui proiect.
