# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | **Türkçe** | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | [한국어](README_KO.md)

[![Web Sitesi](https://img.shields.io/badge/Web_Sitesi-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-Topluluğa_katıl-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Bir_kahve_ısmarla-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Lisans](https://img.shields.io/badge/Lisans-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.tr)

> Bu dosya [README.md](README.md) dosyasının Türkçe çevirisidir. Aralarında bir fark olursa Lehçe sürüm geçerlidir. Launcher düğmeleri, launcher'ın Türkçe arayüzündeki adlarıyla yazılmıştır; ilk geçtikleri yerde Lehçe arayüzdeki adları da parantez içinde verilmiştir.

Haritalarda koşturan ve gerçekten oyun oynayan otonom karakterlerin (Playerbots) yaşadığı yerel bir Metin2 singleplayer dünyası: seviye atlarlar, tek başlarına ve grup hâlinde savaşırlar, birbirleriyle ticaret yaparlar, ekipmanlarını yükseltirler, Metin Taşlarını ve bossları avlarlar, lonca kurarlar ve ilerlemelerini sunucunun veritabanına kaydederler.

Botlar harici programlar değildir. Sunucu motorunun içinde yapay zekâ tarafından yönetilen tam teşekküllü karakterlerdir; bu yüzden oyuncu onların hareketini, dövüşünü, becerilerini ve ekipmanını diğer oyuncuların karakterlerini gördüğü gibi görür.

> [!IMPORTANT]
> **2.2.17 sürümünden itibaren projenin kaynak kodu bu depoda yayımlanmıyor.** Burada güncelleme paketlerini içeren sürümleri (releases), değişiklik listesini ([CHANGELOG.md](CHANGELOG.md)) ve launcher ile panellerin kullandığı dosyaları bulabilirsin. 2.2.16 dahil önceki sürümler MIT lisansıyla yayımlandı ve geçmişleri depoda kalıyor. Daha yeni sürümler [CC BY-NC-ND 4.0](LICENSE) lisansıyla paylaşılıyor.

## 📥 Nasıl oynanır

1. **Tam paketi** (sunucu ve istemci tek arşivde) projenin Discord sunucusundan indir: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Docker Desktop'ı kur ve „Engine running” yazısını görene kadar bekle.
3. Paketi sıradan bir klasöre çıkar, örneğin `C:\Oyunlar\Metin2 Singleplayer` (Masaüstüne ya da OneDrive ile eşitlenen Belgeler klasörüne değil) ve `Serwer\Metin2-Launcher-GUI.bat` dosyasını çalıştır.
4. Launcher güncelleme önerdiğinde **Tümünü güncelle** (Lehçe arayüzde: Aktualizuj wszystko) seçeneğini seç: tam paket en yeni sürümden daha eskidir.
5. Önce **1. KUR / HAZIRLA** (Lehçe arayüzde: 1. ZAINSTALUJ / PRZYGOTUJ), sonra **2. OYNA** (Lehçe arayüzde: 2. GRAJ) düğmesine tıkla. İlk başlatmada sunucu Docker içinde hazır dosyalardan kurulur ve birkaç dakika sürer; sonraki başlatmalar kısa sürer.

> [!NOTE]
> **2.2.39 sürümünden itibaren sunucu korumalı sürümdür.** Kaynak kodu olmadan, hazır sunucu dosyaları olarak gelir ve bilgisayarında hiçbir şey derlenmez; bu sayede kurulum ve güncellemeler daha kısa sürer. Eski, normal bir kurulum bir sonraki güncellemede kendiliğinden korumalı sürüme geçer (launcher bunu günlüğünde 10 saniyelik bir geri sayımla bildirir); dünya, karakterler ve ayarlar korunur. Düzenleyebileceğin oyun dosyaları (droplar, görevler, haritalar) artık `Serwer\linux-port\docker\game\share\locale\poland\` klasöründedir ve güncellemeler bunların üzerine yazmaz. Tek istisna Ay Işığı Define Sandığı'dır: kendi sürümünü `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` dosyasına kaydet (aynı klasördeki `share-add\special_item_group.moonlight.txt` dosyasını kopyala ve `Vnum 50011` satırını koru).

Sunucunun ya da istemcinin yeni bir sürümü çıktığında launcher bunu açılışta kendisi sorar. **GÜNCELLEMELERİ DENETLE** (Lehçe arayüzde: SPRAWDZ AKTUALIZACJE) düğmesiyle istediğin zaman da kontrol edebilirsin. Paketler [bu depodaki sürümlerden](https://github.com/TieruYT/metin2-playerbots/releases) indirilir. Linux'taki bir sunucuyu, sunucu klasöründe `sh linux-port/tools/update.sh` komutuyla güncellersin; VPS üzerindeki bir sunucuyu ise launcher'dan kurup güncelleyebilirsin (**VPS'TE SUNUCU**, Lehçe arayüzde: SERWER NA VPS). VPS'te de sunucu derleme yapılmadan hazır dosyalardan kurulur.

Bir hata mı buldun? Launcher'daki **HATA BİLDİR** (Lehçe arayüzde: ZGŁOŚ BŁĄD) düğmesi açıklamanı günlüklerle birlikte doğrudan bize gönderir. Fikir ve önerilerini Discord'daki öneriler kanalına yazabilirsin.

Sistem gereksinimleri, adım adım kurulum rehberi ve sık sorulan soruların yanıtları [mt2sp.com](https://mt2sp.com) sitesinde ve Discord'daki yardım kanallarında.

## 💬 Topluluk ve projeye destek

- **[Proje sitesi — mt2sp.com](https://mt2sp.com)**: projenin tanıtımı, kurulum rehberi ve SSS; Lehçe, İngilizce, Almanca ve Türkçe.
- **[Discord sunucusu](https://discord.gg/DyCxXdZtnV)**: indirilebilir tam paket, yardım, hata bildirimleri, fikirler ve yeni sürüm duyuruları.
- **[buycoffee.to üzerinden destek](https://buycoffee.to/metin2-playerbots)**: gönüllü bağışlar; araçların, test sunucusunun ve projenin geliştirilmesinde kullanılan yapay zekâ modellerinin masraflarını karşılamaya yardımcı olur.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="buycoffee.to üzerinden bir kahve ısmarla"></a>

Testler, hata bildirimleri, fikirler ve bağışlar: her türlü destek giderek daha canlı bir Metin2 dünyası yaratmaya yardımcı oluyor.

---

## 🌟 Botlar neler yapabiliyor

- ⚔️ **Oyuncular gibi savaşırlar**: tüm sınıflar ve yollar (Savaşçı, Sura, Ninja, Şaman), kombolar, beceri rotasyonları, bufflar, oklarıyla okçular ve savaş atının üstünden kesilen Metin Taşları. Botlar ayrıca kendi krallıklarının köylerinde birbirleriyle idman düellosu yapar.
- 🗺️ **Üç krallık ve tüm dünya**: kendi köyleriyle Shinsoo, Chunjo ve Jinno; Seungryong Vadisi (Ork Vadisi), Yongbi Çölü, Sohan Dağı, Hwang Tapınağı, Ateş Diyarı (Doyyumhwan), ormanlar, Maymun ve Örümcek Zindanları ile Sürgün Mağaraları. Botlar haritayı ve yeri seviyelerine göre seçer, rotalarını haritanın gerçek çarpışma ızgarası (collision grid) üzerinde hesaplar.
- 🧠 **Kişilikler**: Iwakura'nın ruh hâllerini de içeren kişilik sistemi. Botlar Grinder (kasıcı), Fatih, Kumarbaz, Mükemmeliyetçi, Tüccar, yoldaş, paralı asker ya da Metin Taşı dropçusu olabilir; arada bir de nadir bir kişilik çıkar.
- 🛡️ **Loncalar, savaşlar ve baskınlar**: botlar güçlerine göre lonca kurar, lonca haritasında raunt usulü lonca savaşları yapar (oyuncuların loncalarına karşı da), tüm loncayla Şeytan Kulesi'ne çıkar, dünya bosslarını birlikte keser ve Azrail için Şeytan Katakombu'na iner.
- 🎉 **Etkinlikler**: Korsan Tanaka, Zuo'nun Metin yağmuru, Metin Taşlarından düşen Altıgen Sandıklar ve „Soluk Ay Gecesi” Cadılar Bayramı etkinliği; hepsi panelden başlatılır. Botlar da bu etkinliklere kendiliğinden savaşmaya gider.
- 🏰 **Zindanlar ve eklentiler**: Veba Zindanı (45-120. seviyeler) ve 50. seviye silahlarıyla Avcı Demirhanesi (ikisi de varsayılan olarak kapalıdır ve panelden açılır), Beran-Setaou ve saldırı atıyla birlikte Snakefield betası.
- 🏪 **Bot pazarı**: köylerdeki tezgâhlarda gerçek çevrimdışı dükkânlar (offline shop), Iwakura'nın enflasyonlu ve talebe göre ayarlanan fiyat listesinden gelen fiyatlar ve tüm krallıkların tezgâhlarından alışveriş yapan botlar. **Dom Towarowy** (Büyük Mağaza) tüm dükkânların tekliflerini tek pencerede gösterir, bir eşyayı satışa koyarken fiyat önerir („Auto cena” — otomatik fiyat) ve aradığın eşyayı biri satışa koyduğunda sana haber veren bir istek listesi sunar. Metin taşlarından ve bosslardan düşen Ejderha Paralarıyla ödeme yapılan **ItemShop**'ta ayrıca Şans Çarkı ve flaş açık artırmalar da var.
- 🔨 **Karakter gelişimi**: „+N'ye kadar yükselt” seçeneği ve koleksiyoncu deposundan alınan malzemelerle Demirci, yükseltme kâğıtları, bonuslar, ruh taşları, Ejderha Taşı Simyası, beceri kitapları, 30 puana kadar Kombo, eşyalara takılan taşlar (Spirit Stones), Biyolog görevleri, görevler (quest), savaş atına kadar at, balıkçılık, madencilik ve şifalı bitki toplama.
- 💬 **Sohbetler**: botlar fısıltılara yanıt verir, sohbet üzerinden ticaret yapar („Kupię…” — alırım, „Sprzedam…” — satarım), Şaman bufflarının ne işe yaradığını söyler, çağırdığın bot yanına gelir. Botlar okuyucunun dilinde yazar: Lehçe, İngilizce, Almanca, Fransızca, Portekizce, Rumence veya Türkçe (diğer tüm istemcilerde İngilizce).
- 🤝 **Towarzysz (Yoldaş)**: gruptaki kalıcı yol arkadaşın. Yanında savaşır, seni bufflar, seninle ticaret yapar; ekipmanını ve becerilerini sen ayarlarsın. Kendi deposu ve Simyası vardır, balık tutar, bir arkadaşının lideri olduğu gruba katılır („Grupa” — Grup) ve kendisi de grup liderliği yapabilir. Sen yokken kendi başına oynar („Gra beze mnie” — Bensiz oyna): kitap okur, pazardan alışveriş yapar, Demirci'de yükseltme yapar, Biyolog görevlerini ve diğer görevleri yapar, döndüğünde de sana neler yaptığını anlatır. Onu sıradan bir karakter gibi de oynayabilirsin.
- 🎯 **Auto Łowy (Otomatik Av)**: oyuncu için otomatik avlanma; toplama filtresi ve balıkçılık modu da var. Dünya ayarlarından herkese açık mı olacağını, yoksa ItemShop'tan satın alınarak mı kullanılacağını seçersin.
- 🖥️ **Oyun istemcisi**: dokuz dil (Lehçe, İngilizce, Almanca, Fransızca, İspanyolca, İtalyanca, Portekizce, Rumence ve Türkçe), dropları, sandıkları ve rehberleri içeren oyun içi Wiki, Topluluk penceresi, GF tarzı görev penceresi ve oyuncuların üstünde [AUTO]/[AFK] işaretleri.
- 🎛️ **Paneller ve launcher**: canlı dünya haritası, sıralamalar, botların ekipmanı, yapay zekâ davranış kaydırıcıları, zamanlı etkinlikler, bossların yeniden doğma süreleri ve veritabanı düzenleyicisi içeren iki web paneli; güncellemeler, dünya yedeği, zorluk seviyesi, ek kanallar, COOP (arkadaşlarınla kendi dünyanda ücretsiz oyun) ve VPS'te sunucu sunan bir launcher.
- 💾 **Kalıcı dünya**: her botun MariaDB veritabanında kendi hesabı ve karakteri vardır; bu yüzden seviye, eşyalar ve yang yeniden başlatmadan sonra da korunur.

## 🆕 Son eklenenler (2.2.140–2.2.146 sürümleri)

- **Towarzysz** sen yokken kendi kendine gelişir, grup liderliği yapabilir ve yemler ile avın özetiyle birlikte bambaşka bir şekilde balık tutar.
- **Kombo** artık 30 puan ve üç kitabı canavarlardan düşüyor.
- **Dom Towarowy**'nin bir istek listesi var; botların tezgâhlarına konan yeni kâğıtlar, yükseltme malzemeleri ve kitaplar önce oyuncuları bekler.
- **Demirci** seçtiğin artıya kadar otomatik yükseltme yapar; Topluluk penceresi, görev penceresi ve Wiki'deki şifalı bitki rehberleri de yeni.
- **Botlar** köylerde idman düellosu yapar, dropçu olarak Metin Taşı keser, nerede savaşacaklarını daha akıllıca seçer ve oyuncuların yanında daha adil davranır.
- Değişikliklerin tam listesi [CHANGELOG.md](CHANGELOG.md) dosyasındadır.

## ⌨️ Oyun içi kısayollar

| Tuş | İşlev |
|---|---|
| `P` | Towarzysz (Yoldaş) penceresi |
| `K` | Otomatik Av |
| `H` | tuş ve komut listesini içeren yardım |
| `X` | kanal penceresi (`X` + `1`–`4` seni doğrudan CH1–CH4'e geçirir) |
| `` ` `` (tilde) | yakındaki tüm eşyaları topla |
| `F5` | çevrimdışı dükkânlarda eşya arama |
| `F9` | GM paneli (yalnızca GM karakterleri) |
| `F1`–`F3` | giriş ekranında: görünen sayfadaki kayıtlı hesap (5 sayfada 15 hesap) |

## 🎮 Oyun içi komutlar (GM)

| Komut | Yetki | Açıklama | Örnek |
|---|---|---|---|
| `/bot_spawn <id> <krallık: 1-3>` | Yönetici | Verilen ID'ye sahip botu oyuna sokar (`1` = Shinsoo, `2` = Chunjo, `3` = Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Yönetici | Botu dünyadan çıkarır (oturumunu kapatır). | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <adet> <krallık>` | Yönetici | Bir grup botu oyuna sokar. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <adet>` | Yönetici | Bir grup botu dünyadan çıkarır. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Herkes | Aktif botların seviye sıralamasını sohbette gösterir. | `/bot_rank` |

## 🗄️ Veritabanı erişimi (Navicat, HeidiSQL, DBeaver)

Sunucunun veritabanı bir konteyner içinde çalışan MariaDB'dir ve **yalnızca bu bilgisayardan**
erişilebilir (`127.0.0.1`, port `3306` ya da `.env` dosyasında `M2_DB_PUBLISH_PORT` ayarlandıysa başka bir port).
Veritabanı istemcisinde yeni bağlantı: tür MySQL/MariaDB, host `127.0.0.1`, port `3306`.

| Hesap | Ne için | Parola |
|---|---|---|
| `root` | her şey | sunucu klasöründeki `linux-port\docker\.env` dosyasında `M2_DB_ROOT_PASSWORD` |
| `metin2` | yalnızca oyunun veritabanları (`account`, `player`, `log`, `common`, `hotbackup`) | aynı dosyada `M2_DB_PASSWORD` |

En hızlı yol: launcher'daki **VERİTABANI BİLGİLERİ (NAVICAT)** (Lehçe arayüzde: DANE DO BAZY (NAVICAT)) düğmesi host'u,
portu ve iki parolayı kopyalanabilir alanlarda gösterir. Parolalar ilk çalıştırmada
rastgele oluşturulur ve herhangi bir "varsayılan" parola yoktur. Bunları Discord'a yapıştırma.

Veritabanı istemcisi `1045 - Access denied for user 'root'@'172.18.0.1'` yanıtını verirse,
veritabanı şu an `.env` dosyasında bulunandan farklı bir parolayla oluşturulmuştur.
**VERİTABANI ERİŞİMİNİ ONAR** (Lehçe arayüzde: NAPRAW DOSTEP DO BAZY) düğmesine tıkla: launcher sunucuyu durdurur ve
`metin2` ile `root` hesaplarının parolalarını `.env` dosyasındakilere ayarlar; karakterlere,
eşyalara ve botlara dokunulmaz. Ardından OYNA düğmesine tıkla ve yeniden giriş yap.

## 📜 Lisans

2.2.55 sürümünden itibaren proje **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.tr)** (Atıf-GayriTicari-Türetilemez) lisansıyla sunuluyor:

- yazarı belirtip projenin bağlantısını vermek koşuluyla oynamak, kendin ve arkadaşların için sunucu çalıştırmak ve resmî paketleri değiştirmeden paylaşmak **serbesttir**;
- değiştirilmiş sürümleri ya da ayrı dağıtımları yaymak **yasaktır**; projeye eklentiler yapılabilir ve bunlar projenin Discord sunucusunda paylaşılabilir;
- projeden para kazanmak **yasaktır**: projeyi satmak, sunucuya erişim için ücret almak ya da oyun içi eşya satmak olmaz. Oyunda avantaj sağlamayan, sunucunun ayakta tutulması için yapılan gönüllü bağışlar ile oynanışı kaydetmek ve yayınlamak (stream) sorun değildir.

2.2.16 dahil önceki sürümler MIT lisansı ([LICENSE-MIT.txt](LICENSE-MIT.txt)), 2.2.17 ile 2.2.54 arasındaki sürümler ise CC BY-NC-SA 4.0 lisansı altındadır. Metin2, Ymir Interactive ve Webzen'e; sunucu dosyası paketleri ise kendi yazarlarına aittir. Ayrıntılar [LICENSE](LICENSE) ve [NOTICE.md](NOTICE.md) dosyalarındadır.

**Paketlerdeki Metin2 dosyaları.** Projenin paketleri (tam paket ile istemci ve sunucu güncellemeleri), proje yazarlarının emeği olmayan dosyalar da içerir: Ymir Interactive ve Webzen'e ait Metin2 istemcisi ve oyun verileri ile sunucu dosyası paketlerinin dosyaları (TMP4'ün r40250'si, Martysama'nın mt2009'u). Projenin lisansı bunları kapsamaz ve bunlar üzerinde hiçbir hak vermez. Paketleri değiştirmeden paylaşma izni yalnızca proje yazarlarının emeği için geçerlidir.

### Kaldırma talepleri

Hak sahibiysen (Ymir Interactive, Webzen, Gameforge, bir sunucu dosyası paketinin yazarı ya da başka biri) ve buradaki bir şeyin kaldırılmasını istiyorsan, bu depoda [bir issue aç](https://github.com/TieruYT/metin2-playerbots/issues) ya da [projenin Discord sunucusuna](https://discord.gg/DyCxXdZtnV) yaz. Dosyaları ya da sürümleri belirtmen yeterli. Bunları tartışmadan ve resmî bir DMCA bildirimi gerekmeden hemen kaldırırız; elbette o da mümkündür.

## 🤝 Teşekkürler

- **AzzlackSyndicate**: projenin üzerine kurulduğu orijinal Linux port tabanının, kurulum programlarının ve panelin yazarı (MIT lisansı, bkz. [NOTICE.md](NOTICE.md)).
- **Iwakura**: botların kişilik sistemi, fiyat listesi, eşya kademeleri (tier), dükkân ve lonca adları, botların takma adları ve Community Patch'ler.
- **seban latino**: kurulumdaki ikinci panel olan, canlı harita, profiller, sıralamalar ve ekonomi içeren Metin2 Singleplayer Panel.
- **ĹŌŞƬĒĶ**: giriş ekranı ve Discord Rich Presence, dil paketi, fısıltı üzerinden botlarla sohbet, botun üstündeki kişilik satırı ve Şans Çarkı'nın diski.
- **Colide**: Otomatik Av penceresi, Topluluk penceresi, yoldaşın deposu, çevrimdışı planı ve balıkçılığı ile [AUTO]/[AFK] işaretlerinin çizimleri.
- **OskarPWA**: bot depo penceresi, beceri simgeleri ve F9 GM panelinin ilk sürümü.
- **Tyrion**: çevrimdışı dükkânlarda belirli bir eşyayı arama, Cadılar Bayramı etkinliği ve Almanca, Fransızca, Portekizce ve Rumence çeviriler.
- **Uxìĕ [DSO]**: Dom Towarowy, tüm ekrandaki canavarları çeken Cesaret Pelerini ve envanterin yan paneli.
- **Gibon**: sandık içeriği ve canavar dropu önizlemesi.
- **Kiciamol**: hedefin canının çubukta gösterilmesi (savaştaki oyuncular dahil), F9 GM paneli, rehberli oyun içi Wiki, Şans Çarkı, Altıgen Sandıklar, 30 puana kadar Kombo ve Snakefield betası.
- **NerrVoVy**: Veba Zindanı, Avcı Demirhanesi, GF tarzı görev penceresi, ItemShop'un flaş açık artırmaları, hedefin bonus önizlemesi ve sohbetteki krallık bayrakları.
- **Said (saido_0)**: Türkçe çeviri ve Bitkicinin Yolu.
- **blasty**: bonus değiştirici.
- **Piciu713**: Demirci penceresinde yükseltme şansı.
- **Kenny, Pabloo, Mur4s**: grupta eşya toplama, oyuncunun grubundaki botların görevleri, ayı görevi ve Işınlama Yüzüğü düzeltmeleri.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): MMO oyunlarında otonom ajanlar üzerine araştırmalar için ilham kaynağı.
- Discord topluluğu: bu projenin büyük bölümünün doğduğu testler, hata bildirimleri ve fikirler.
