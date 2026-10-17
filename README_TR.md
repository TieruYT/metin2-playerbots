# ⚔️ Metin2 Playerbots

[Polski (README.md)](README.md) | [English (README_EN.md)](README_EN.md) | **Türkçe**

[![Web Sitesi](https://img.shields.io/badge/Web_Sitesi-metin2singleplayer.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://metin2singleplayer.com)
[![Discord](https://img.shields.io/badge/Discord-Topluluğa_katıl-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-Bir_kahve_ısmarla-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![Lisans](https://img.shields.io/badge/Lisans-CC_BY--NC--SA_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.tr)

> Bu dosya [README.md](README.md) dosyasının Türkçe çevirisidir. Aralarında bir fark olursa Lehçe sürüm geçerlidir.

Haritalarda koşturan ve gerçekten oyun oynayan otonom karakterlerin (Playerbots) yaşadığı yerel bir Metin2 singleplayer dünyası: seviye atlarlar, tek başlarına ve grup halinde savaşırlar, birbirleriyle ticaret yaparlar, ekipmanlarını yükseltirler, Metin taşlarını ve bossları avlarlar, lonca kurarlar ve ilerlemelerini sunucunun veritabanına kaydederler.

Botlar harici programlar değildir. Sunucu motorunun içinde yapay zekâ tarafından yönetilen tam teşekküllü karakterlerdir; bu yüzden oyuncu onların hareketini, dövüşünü, becerilerini ve ekipmanını diğer oyuncuların karakterlerini gördüğü gibi görür.

> [!IMPORTANT]
> **2.2.17 sürümünden itibaren projenin kaynak kodu bu depoda yayımlanmıyor.** Burada güncelleme paketlerini içeren sürümleri (releases), değişiklik listesini ([CHANGELOG.md](CHANGELOG.md)) ve launcher ile panellerin kullandığı dosyaları bulabilirsin. 2.2.16 dahil önceki sürümler MIT lisansıyla yayımlandı ve geçmişleri depoda kalıyor. Daha yeni sürümler [CC BY-NC-SA 4.0](LICENSE) lisansıyla paylaşılıyor.

## 📥 Nasıl oynanır

Düğme adları aşağıda launcher'da göründükleri gibi (Lehçe) yazılmıştır, Türkçe karşılıkları parantez içindedir.

1. **Tam paketi** (sunucu ve istemci tek arşivde) projenin Discord sunucusundan indir: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Docker Desktop'ı kur ve „Engine running” yazısını görene kadar bekle.
3. Paketi sıradan bir klasöre çıkar, örneğin `C:\Oyunlar\Metin2 Singleplayer` (Masaüstüne ya da OneDrive ile eşitlenen Belgeler klasörüne değil) ve `Serwer\Metin2-Launcher-GUI.bat` dosyasını çalıştır.
4. Launcher güncelleme önerdiğinde **Aktualizuj wszystko** (Tümünü güncelle) seçeneğini seç: tam paket en yeni sürümden daha eskidir.
5. Önce **1. ZAINSTALUJ / PRZYGOTUJ** (Kur / Hazırla), sonra **2. GRAJ** (Oyna) düğmesine tıkla. İlk başlatmada sunucu Docker içinde hazır dosyalardan kurulur ve birkaç dakika sürer; sonraki başlatmalar kısa sürer.

> [!NOTE]
> **2.2.39 sürümünden itibaren sunucu korumalı sürümdür.** Kaynak kodu olmadan, hazır sunucu dosyaları olarak gelir ve bilgisayarında hiçbir şey derlenmez; bu sayede kurulum ve güncellemeler daha kısa sürer. Eski, normal bir kurulum bir sonraki güncellemede kendiliğinden korumalı sürüme geçer (launcher bunu günlüğünde 10 saniyelik bir geri sayımla bildirir); dünya, karakterler ve ayarlar korunur. Düzenleyebileceğin oyun dosyaları (droplar, görevler, haritalar) artık `Serwer\linux-port\docker\game\share\locale\poland\` klasöründedir ve güncellemeler bunların üzerine yazmaz. Tek istisna Ay Işığı Define Sandığı'dır: kendi sürümünü `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt` dosyasına kaydet (aynı klasördeki `share-add\special_item_group.moonlight.txt` dosyasını kopyala ve `Vnum 50011` satırını koru).

Sunucunun ya da istemcinin yeni bir sürümü çıktığında launcher bunu açılışta kendisi sorar. **SPRAWDZ AKTUALIZACJE** (Güncellemeleri denetle) düğmesiyle istediğin zaman da kontrol edebilirsin. Paketler [bu depodaki sürümlerden](https://github.com/TieruYT/metin2-playerbots/releases) indirilir. Linux'taki bir sunucuyu, sunucu klasöründe `sh linux-port/tools/update.sh` komutuyla güncellersin; VPS üzerinde bir sunucuyu ise launcher'dan kurup güncelleyebilirsin (**SERWER NA VPS** — VPS'te sunucu). VPS'te de sunucu derleme yapılmadan hazır dosyalardan kurulur.

Bir hata mı buldun? Launcher'daki **ZGŁOŚ BŁĄD** (Hata bildir) düğmesi açıklamanı günlüklerle birlikte doğrudan bize gönderir. Fikir ve önerilerini Discord'daki öneriler kanalına yazabilirsin.

Sistem gereksinimleri, adım adım kurulum rehberi ve sık sorulan soruların yanıtları [metin2singleplayer.com](https://metin2singleplayer.com) sitesinde ve Discord'daki yardım kanallarında.

## 💬 Topluluk ve projeye destek

- **[Proje sitesi — metin2singleplayer.com](https://metin2singleplayer.com)**: projenin tanıtımı, kurulum rehberi ve SSS; Lehçe ve İngilizce.
- **[Discord sunucusu](https://discord.gg/DyCxXdZtnV)**: indirilebilir tam paket, yardım, hata bildirimleri, fikirler ve yeni sürüm duyuruları.
- **[buycoffee.to üzerinden destek](https://buycoffee.to/metin2-playerbots)**: gönüllü bağışlar; araçların, test sunucusunun ve projenin geliştirilmesinde kullanılan yapay zekâ modellerinin masraflarını karşılamaya yardımcı olur.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="buycoffee.to üzerinden bir kahve ısmarla"></a>

Testler, hata bildirimleri, fikirler ve bağışlar: her türlü destek giderek daha canlı bir Metin2 dünyası yaratmaya yardımcı oluyor.

---

## 🌟 Botlar neler yapabiliyor

- ⚔️ **Oyuncular gibi savaşırlar**: tüm sınıflar ve yollar (Savaşçı, Sura, Ninja, Şaman), kombolar, beceri rotasyonları, bufflar, oklarıyla okçular ve savaş atının üstünden kesilen Metin taşları.
- 🗺️ **Üç krallık ve tüm dünya**: kendi köyleriyle Shinsoo, Chunjo ve Jinno; Seungryong Vadisi (Ork Vadisi), Yongbi Çölü, Sohan Dağı, Hwang Tapınağı, Ateş Diyarı (Doyyumhwan), ormanlar ile Maymun ve Örümcek Zindanları. Botlar haritayı ve yeri seviyelerine göre seçer, rotalarını haritanın gerçek çarpışma ızgarası (collision grid) üzerinde hesaplar.
- 🧠 **Kişilikler**: Iwakura'nın ruh hâllerini de içeren kişilik sistemi. Botlar Grinder (kasıcı), Fatih, Kumarbaz, Mükemmeliyetçi, Tüccar, yoldaş ya da paralı asker olabilir; arada bir de nadir bir kişilik çıkar.
- 🛡️ **Loncalar, savaşlar ve baskınlar**: botlar güçlerine göre lonca kurar, lonca haritasında raunt usulü lonca savaşları yapar (oyuncuların loncalarına karşı da), tüm loncayla Şeytan Kulesi'ne çıkar, dünya bosslarını birlikte keser ve Azrael için Şeytan Katakombu'na iner.
- 🎉 **Etkinlikler**: duyuruları ve ödülleriyle Korsan Tanaka ve Zuo'nun Metin yağmuru; botlar da bu etkinliklere kendiliğinden savaşmaya gider.
- 🏪 **Bot pazarı**: köylerdeki tezgâhlarda gerçek çevrimdışı dükkânlar (offline shop), Iwakura'nın enflasyonlu ve talebe göre ayarlanan fiyat listesinden gelen fiyatlar ve botlar arası alışveriş. **Dom Towarowy** (Büyük Mağaza) tüm dükkânların tekliflerini tek pencerede gösterir ve bir eşyayı satışa koyarken botların fiyatını önerir („Auto cena” — otomatik fiyat seçeneğiyle). Oyunda ayrıca Metin taşlarından ve bosslardan düşen Ejderha Paraları ile alışveriş yapılan bir **ItemShop** da var.
- 🔨 **Karakter gelişimi**: Demirci ve yükseltme kâğıtları, bonuslar, ruh taşları, beceri kitapları ve eşyalara takılan taşlar (Spirit Stones), Biyolog görevleri, savaş atına kadar at, balıkçılık, madencilik ve şifalı bitki toplama.
- 💬 **Sohbetler**: botlar fısıltılara yanıt verir, sohbet üzerinden ticaret yapar („Kupię…” — alırım, „Sprzedam…” — satarım), Şaman bufflarının ne işe yaradığını söyler, çağırdığın bot yanına gelir. Botlar Lehçe konuşur.
- 🤝 **Towarzysz (Yoldaş)**: gruptaki kalıcı yol arkadaşın. Yanında savaşır, seni bufflar, seninle ticaret yapar; ekipmanını ve becerilerini sen ayarlarsın. Bir arkadaşının lideri olduğu gruba da katılır („Grupa” — Grup) ve sen yokken kendi başına oynayabilir („Gra beze mnie” — Bensiz oyna).
- 🎯 **Auto Łowy (Otomatik Av)**: oyuncu için otomatik avlanma. Dünya ayarlarından herkese açık mı olacağını, yoksa ItemShop'tan satın alınarak mı kullanılacağını seçersin.
- 🎛️ **Paneller ve launcher**: canlı dünya haritası, sıralamalar, botların ekipmanı, yapay zekâ davranış kaydırıcıları ve zamanlı etkinlikler içeren iki web paneli; güncellemeler, dünya yedeği, zorluk seviyesi, ikinci kanal, COOP (arkadaşlarınla kendi dünyanda ücretsiz oyun) ve VPS'te sunucu sunan bir launcher.
- 💾 **Kalıcı dünya**: her botun MariaDB veritabanında kendi hesabı ve karakteri vardır; bu yüzden seviye, eşyalar ve yang yeniden başlatmadan sonra da korunur.

## ⌨️ Oyun içi kısayollar

| Tuş | İşlev |
|---|---|
| `P` | Towarzysz (Yoldaş) penceresi |
| `K` | Otomatik Av |
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

En hızlı yol: launcher'daki **DANE DO BAZY (NAVICAT)** (Veritabanı bilgileri) düğmesi host'u,
portu ve iki parolayı kopyalanabilir alanlarda gösterir. Parolalar ilk çalıştırmada
rastgele oluşturulur ve herhangi bir „varsayılan” parola yoktur. Bunları Discord'a yapıştırma.

Veritabanı istemcisi `1045 - Access denied for user 'root'@'172.18.0.1'` yanıtını verirse,
veritabanı şu an `.env` dosyasında bulunandan farklı bir parolayla oluşturulmuştur.
**NAPRAW DOSTEP DO BAZY** (Veritabanı erişimini onar) düğmesine tıkla: launcher sunucuyu durdurur ve
`metin2` ile `root` hesaplarının parolalarını `.env` dosyasındakilere ayarlar; karakterlere,
eşyalara ve botlara dokunulmaz. Ardından GRAJ (Oyna) düğmesine tıkla ve yeniden giriş yap.

## 📜 Lisans

2.2.55 sürümünden itibaren proje **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.tr)** (Atıf-GayriTicari-Türetilemez) lisansıyla sunuluyor:

- yazarı belirtip projenin bağlantısını vermek koşuluyla oynamak, kendin ve arkadaşların için sunucu çalıştırmak ve resmi paketleri değiştirmeden paylaşmak **serbesttir**;
- değiştirilmiş sürümleri ya da ayrı dağıtımları yaymak **yasaktır**; projeye eklentiler yapılabilir ve bunlar projenin Discord sunucusunda paylaşılabilir;
- projeden para kazanmak **yasaktır**: projeyi satmak, sunucuya erişim için ücret almak ya da oyun içi eşya satmak olmaz. Oyunda avantaj sağlamayan, sunucunun ayakta tutulması için yapılan gönüllü bağışlar ile oynanışı kaydetmek ve yayınlamak (stream) sorun değildir.

2.2.16 dahil önceki sürümler MIT lisansı ([LICENSE-MIT.txt](LICENSE-MIT.txt)), 2.2.17 ile 2.2.54 arasındaki sürümler ise CC BY-NC-SA 4.0 lisansı altındadır. Metin2, Ymir Interactive ve Webzen'e; sunucu dosyası paketleri ise kendi yazarlarına aittir. Ayrıntılar [LICENSE](LICENSE) ve [NOTICE.md](NOTICE.md) dosyalarındadır.

## 🤝 Teşekkürler

- **AzzlackSyndicate**: projenin üzerine kurulduğu orijinal Linux port tabanının, kurulum programlarının ve panelin yazarı (MIT lisansı, bkz. [NOTICE.md](NOTICE.md)).
- **Iwakura**: botların kişilik sistemi, fiyat listesi, eşya kademeleri (tier), dükkân ve lonca adları, botların takma adları ve Community Patch'ler.
- **seban latino**: kurulumdaki ikinci panel olan, canlı harita, profiller, sıralamalar ve ekonomi içeren Metin2 Singleplayer Panel.
- **ĹŌŞƬĒĶ**: giriş ekranı ve Discord Rich Presence, dil paketi, fısıltı üzerinden botlarla sohbet ve botun üstündeki kişilik satırı.
- **Colide**: Otomatik Av penceresi.
- **OskarPWA**: bot depo penceresi, beceri simgeleri ve F9 GM paneli.
- **Tyrion**: çevrimdışı dükkânlarda belirli bir eşyayı arama.
- **Uxìĕ [DSO]**: Dom Towarowy ve tüm ekrandaki canavarları çeken Cesaret Pelerini.
- **Gibon**: sandık içeriği ve canavar dropu önizlemesi.
- **Kiciamol**: hedefin canının çubukta gösterilmesi (savaştaki oyuncular dahil).
- **Piciu713**: Demirci penceresinde yükseltme şansı.
- **Kenny, Pabloo, Mur4s**: grupta eşya toplama, oyuncunun grubundaki botların görevleri, ayı görevi ve Işınlanma Yüzüğü düzeltmeleri.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): MMO oyunlarında otonom ajanlar üzerine araştırmalar için ilham kaynağı.
- Discord topluluğu: bu projenin büyük bölümünün doğduğu testler, hata bildirimleri ve fikirler.
