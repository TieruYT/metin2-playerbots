# ⚔️ Metin2 Playerbots

[Polski](README.md) | [English](README_EN.md) | [Deutsch](README_DE.md) | [Français](README_FR.md) | [Español](README_ES.md) | [Italiano](README_IT.md) | [Português](README_PT.md) | [Română](README_RO.md) | [Türkçe](README_TR.md) | [Magyar](README_HU.md) | [Čeština](README_CS.md) | [Nederlands](README_NL.md) | [Dansk](README_DA.md) | [Русский](README_RU.md) | [Ελληνικά](README_EL.md) | [العربية](README_AR.md) | **한국어**

[![웹사이트](https://img.shields.io/badge/%EC%9B%B9%EC%82%AC%EC%9D%B4%ED%8A%B8-mt2sp.com-2EA44F?style=for-the-badge&logo=firefoxbrowser&logoColor=white)](https://mt2sp.com)
[![Discord](https://img.shields.io/badge/Discord-%EC%BB%A4%EB%AE%A4%EB%8B%88%ED%8B%B0_%EC%B0%B8%EC%97%AC-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/DyCxXdZtnV)
[![BuyCoffee](https://img.shields.io/badge/BuyCoffee-%ED%94%84%EB%A1%9C%EC%A0%9D%ED%8A%B8_%ED%9B%84%EC%9B%90-FF813F?style=for-the-badge&logo=coffeescript&logoColor=white)](https://buycoffee.to/metin2-playerbots)
[![라이선스](https://img.shields.io/badge/%EB%9D%BC%EC%9D%B4%EC%84%A0%EC%8A%A4-CC_BY--NC--ND_4.0-EF9421?style=for-the-badge&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.ko)

자율적으로 움직이는 캐릭터(Playerbots)가 맵을 누비며 진짜로 게임을 하는 로컬 싱글플레이 Metin2 월드입니다. 봇들은 레벨을 올리고, 혼자서 또는 파티로 싸우고, 서로 거래하고, 장비를 강화하고, 메틴석과 보스를 사냥하고, 길드를 창설하며, 진행 상황을 서버 데이터베이스에 저장합니다.

봇은 외부 프로그램이 아닙니다. 서버 엔진 내부의 AI가 조종하는 완전한 캐릭터이기 때문에, 플레이어는 다른 플레이어의 캐릭터를 보는 것과 똑같이 봇의 이동, 전투, 스킬, 장비를 보게 됩니다.

게임 클라이언트는 폴란드어, 영어, 독일어, 프랑스어, 스페인어, 이탈리아어, 포르투갈어, 루마니아어, 터키어로 제공됩니다. 게임 인터페이스의 한국어 버전은 커뮤니티 번역으로 준비 중입니다([metin2-singleplayer-translations](https://github.com/TieruYT/metin2-singleplayer-translations)). 그때까지는 영어로 플레이할 수 있습니다.

> [!IMPORTANT]
> **버전 2.2.17부터 프로젝트의 소스 코드는 이 저장소에 공개되지 않습니다.** 여기에서는 업데이트 패키지가 포함된 릴리스, 변경 사항 목록([CHANGELOG.md](CHANGELOG.md)), 그리고 런처와 패널이 읽는 파일을 찾을 수 있습니다. 2.2.16 이하 버전은 MIT 라이선스로 공개되었으며 그 이력은 저장소에 그대로 남아 있습니다. 그 이후 버전은 [CC BY-NC-ND 4.0](LICENSE) 라이선스로 배포됩니다.

## 📥 플레이 방법

1. 프로젝트의 Discord 서버에서 **전체 패키지**(서버와 클라이언트가 하나의 압축 파일에 포함)를 다운로드하세요: [discord.gg/DyCxXdZtnV](https://discord.gg/DyCxXdZtnV).
2. Docker Desktop을 설치하고 "Engine running"이 표시될 때까지 기다리세요.
3. 패키지를 일반 폴더에 압축 해제하세요. 예: `C:\Games\Metin2 Singleplayer` (OneDrive가 동기화하는 바탕 화면이나 문서 폴더는 피하세요). 그런 다음 `Serwer\Metin2-Launcher-GUI.bat`을 실행하세요.
4. 런처가 업데이트를 제안하면 **Update everything**(모두 업데이트)을 선택하세요. 전체 패키지는 최신 릴리스보다 오래되었기 때문입니다.
5. **1. INSTALL / PREPARE**(설치 / 준비)를 누른 다음 **2. PLAY**(플레이)를 누르세요. 첫 실행 시 준비된 파일로 Docker 안에 서버를 구성하므로 몇 분 걸리고, 이후에는 금방 시작됩니다.

> [!NOTE]
> **버전 2.2.39부터 서버는 보호된 버전입니다.** 소스 코드 없이 완성된 서버 파일 형태로 제공되며 PC에서 아무것도 컴파일하지 않으므로, 설치와 업데이트에 걸리는 시간이 줄어듭니다. 예전의 일반 설치는 다음 업데이트 때 자동으로 보호된 버전으로 전환되며(런처가 10초 카운트다운과 함께 로그에 알려 줍니다), 월드, 캐릭터, 설정은 그대로 유지됩니다. 직접 수정할 수 있는 게임 파일(드롭, 퀘스트, 맵)은 이제 `Serwer\linux-port\docker\game\share\locale\poland\`에 있으며, 업데이트가 이 파일들을 덮어쓰지 않습니다. 예외는 달빛 상자입니다. 직접 만든 설정은 `Serwer\linux-port\docker\game\special_item_group.moonlight.custom.txt`에 저장하세요(같은 폴더의 `share-add\special_item_group.moonlight.txt`를 복사하고 `Vnum 50011` 줄은 그대로 두세요).

런처는 시작할 때 새 서버 또는 클라이언트 버전을 알아서 제안하며, **CHECK FOR UPDATES**(업데이트 확인)로 언제든지 새 버전을 찾을 수 있습니다. 패키지는 [이 저장소의 릴리스](https://github.com/TieruYT/metin2-playerbots/releases)에서 받아옵니다. Linux 서버는 서버 폴더에서 `sh linux-port/tools/update.sh`로 업데이트하고, VPS 서버는 런처에서 바로 설정하고 업데이트할 수 있습니다(**SERVER ON A VPS** — VPS 서버). VPS에서도 서버는 완성된 파일로 구성되며 아무것도 컴파일하지 않습니다.

버그를 발견했나요? 런처의 **REPORT A BUG**(버그 신고) 버튼을 누르면 설명과 로그가 함께 저희에게 바로 전송됩니다. 아이디어와 제안은 Discord의 제안 채널에 보내 주세요.

요구 사양, 단계별 가이드, 자주 묻는 질문에 대한 답변은 [mt2sp.com](https://mt2sp.com)과 Discord의 도움 채널에서 확인할 수 있습니다.

## 💬 커뮤니티 및 프로젝트 후원

- **[프로젝트 웹사이트 — mt2sp.com](https://mt2sp.com)**: 프로젝트 소개, 설치 가이드, FAQ를 폴란드어, 영어, 독일어, 터키어로 제공합니다.
- **[Discord 서버](https://discord.gg/DyCxXdZtnV)**: 전체 패키지, 도움말, 버그 신고, 아이디어, 새 버전 소식.
- **[buycoffee.to에서 후원하기](https://buycoffee.to/metin2-playerbots)**: 자발적인 후원은 프로젝트 개발에 쓰이는 도구, 테스트 서버, AI 모델 비용을 충당하는 데 도움이 됩니다.

<a href="https://buycoffee.to/metin2-playerbots" target="_blank"><img src="https://buycoffee.to/btn/buycoffeeto-btn-primary.svg" style="height: 42px;" alt="buycoffee.to에서 후원하기"></a>

테스트, 버그 신고, 아이디어, 후원 등 어떤 형태의 지원이든 더 생동감 있는 Metin2 월드를 만드는 데 도움이 됩니다.

---

## 🌟 봇이 하는 일

- ⚔️ **플레이어처럼 싸웁니다**: 모든 직업과 계열(무사, 수라, 자객, 무당), 콤보, 스킬 로테이션, 버프, 화살을 쓰는 궁수, 그리고 전투마 위에서 부수는 메틴석까지. 봇들은 자기 왕국의 마을에서 서로 대련도 합니다.
- 🗺️ **세 왕국과 온 세상**: 각자의 마을을 가진 신수국, 천조국, 진노국, 오크 계곡, 영비 사막, 서한산, 황사원, 불의 땅(Doyyumhwaji), 숲, 원숭이 던전과 거미 던전, 추방의 동굴. 봇은 레벨에 맞춰 맵과 장소를 고르고, 맵의 실제 충돌 격자를 따라 경로를 계획합니다.
- 🧠 **성격**: Iwakura가 만든 기분 시스템이 포함된 성격 시스템. 봇은 그라인더, 정복자, 도박꾼, 완벽주의자, 상인, 동료, 용병, 메틴석 드롭퍼 중 하나가 될 수 있고, 가끔은 희귀한 성격이 나타나기도 합니다.
- 🛡️ **길드, 전쟁, 레이드**: 봇은 힘에 따라 길드를 창설하고, 길드 맵에서 라운드제 길드전을 치르며(플레이어 길드와도 싸웁니다), 길드 단위로 악마의 탑에 가고, 함께 월드 보스를 쓰러뜨리고, 아즈라엘을 쫓아 카타콤으로 내려갑니다.
- 🎉 **이벤트**: 해적 타나카, 조의 메틴 비, 메틴석에서 나오는 육각 상자, 할로윈 이벤트 "창백한 달의 밤" — 모두 패널에서 시작합니다. 봇들은 알아서 그곳으로 싸우러 갑니다.
- 🏰 **던전과 애드온**: 역병 던전(레벨 45-120)과 레벨 50용 무기가 있는 사냥꾼의 대장간 — 둘 다 기본적으로 꺼져 있으며 패널에서 켤 수 있습니다 — 그리고 Beran-Setaou, 돌격마가 나오는 Snakefield 베타.
- 🏪 **봇 시장**: 마을 노점의 진짜 오프라인 상점, 인플레이션이 반영되고 수요에 따라 조정되는 Iwakura 가격표의 가격, 그리고 모든 왕국의 판매대에서 물건을 사는 봇들. **Dom Towarowy**(백화점)는 모든 상점의 매물을 한 창에 보여 주고, 아이템을 등록할 때 가격을 제안하며("Auto cena" — 자동 가격), 누군가 당신이 찾는 아이템을 올리면 알려 주는 위시리스트도 있습니다. 메틴석과 보스에게서 떨어지는 용의 코인으로 결제하는 **ItemShop**에는 행운의 수레바퀴와 반짝 경매도 있습니다.
- 🔨 **캐릭터 성장**: "+N까지 강화" 기능과 수집가의 창고에서 가져오는 재료를 지원하는 대장장이, 강화 주문서, 보너스, 영혼석, 용석 연금술, 스킬북, 최대 30포인트의 콤보, 정령석, 생물학자 임무, 퀘스트, 전투마까지 키우는 말, 낚시, 채광, 약초 채집.
- 💬 **대화**: 봇은 귓속말에 답하고 채팅으로 거래합니다("Kupię…" — 삽니다, "Sprzedam…" — 팝니다). 무당은 자기 버프가 무엇을 주는지 알려 주고, 부른 봇은 당신에게 다가옵니다. 봇은 읽는 사람의 언어로 씁니다: 폴란드어, 영어, 독일어, 프랑스어, 포르투갈어, 루마니아어, 터키어(그 밖의 클라이언트에서는 영어).
- 🤝 **Towarzysz, 당신의 동료**: 파티에 늘 함께하는 영구적인 파트너. 당신 곁에서 싸우고, 버프를 걸어 주고, 당신과 거래하며, 장비와 스킬은 당신이 직접 정합니다. 자신만의 창고와 연금술이 있고, 낚시를 하고, 친구가 이끄는 파티에 들어가며("Grupa" — 파티), 직접 파티를 이끌 수도 있습니다. 당신이 자리를 비우면 혼자 플레이합니다("Gra beze mnie" — 나 없이 플레이): 책을 읽고, 시장에서 쇼핑하고, 대장장이에게서 강화하고, 생물학자 임무와 퀘스트를 하고, 당신이 돌아오면 보고합니다. 일반 캐릭터처럼 직접 플레이할 수도 있습니다.
- 🎯 **Auto Hunt(자동 사냥)**: 플레이어를 위한 자동 사냥으로, 줍기 필터와 낚시 모드가 있습니다. 모두에게 제공할지 ItemShop에서 구매하게 할지는 월드 설정에 따라 정해집니다.
- 🖥️ **게임 클라이언트**: 9개 언어(폴란드어, 영어, 독일어, 프랑스어, 스페인어, 이탈리아어, 포르투갈어, 루마니아어, 터키어), 드롭·상자·가이드가 담긴 게임 내 위키, 커뮤니티 창, GF 스타일 퀘스트 창, 플레이어 머리 위의 [AUTO]/[AFK] 표시.
- 🎛️ **패널과 런처**: 실시간 월드 맵, 랭킹, 봇 장비, AI 행동 슬라이더, 예약 이벤트, 보스 리스폰 시간, 데이터베이스 편집기를 갖춘 두 개의 웹 패널; 업데이트, 월드 백업, 난이도 설정, 추가 채널, COOP(친구가 당신의 월드에서 플레이, 무료), VPS 서버를 지원하는 런처.
- 💾 **영구적인 월드**: 모든 봇은 MariaDB에 자신만의 계정과 캐릭터를 가지고 있어서, 재시작 후에도 레벨, 아이템, 양이 그대로 남습니다.

## 🆕 최근 추가된 기능 (버전 2.2.140–2.2.146)

- **Towarzysz**는 당신이 없는 동안 스스로 성장하고, 파티를 이끌 수 있으며, 미끼와 어획 요약이 있는 완전히 새로운 방식으로 낚시를 합니다.
- **콤보**가 30포인트가 되었고, 세 권의 콤보 책이 몬스터에게서 떨어집니다.
- **Dom Towarowy**에 위시리스트가 생겼고, 봇 판매대에 새로 올라온 주문서, 강화 재료, 책은 먼저 플레이어를 기다립니다.
- **대장장이**는 당신이 고른 강화 수치까지 자동으로 강화합니다. 커뮤니티 창, 퀘스트 창, 위키의 약초 채집 가이드도 새로 추가되었습니다.
- **봇**은 마을에서 대련하고, 드롭퍼로서 메틴석을 부수고, 싸울 장소를 더 합리적으로 고르며, 플레이어 곁에서 더 공정하게 행동합니다.
- 전체 변경 사항은 [CHANGELOG.md](CHANGELOG.md)에서 확인하세요.

## ⌨️ 게임 내 단축키

| 키 | 기능 |
|---|---|
| `P` | 동료 창 |
| `K` | 자동 사냥(Auto Hunt) |
| `H` | 키와 명령어 목록이 있는 도움말 |
| `X` | 채널 창 (`X` + `1`–`4`로 CH1–CH4로 바로 이동) |
| `` ` `` (tilde) | 근처의 모든 아이템 줍기 |
| `F5` | 오프라인 상점에서 아이템 검색 |
| `F9` | GM 패널 (GM 캐릭터 전용) |
| `F1`–`F3` | 로그인 화면에서: 현재 표시된 페이지의 저장된 계정 (5페이지에 15개 계정) |

## 🎮 게임 내 명령어 (GM)

| 명령어 | 권한 | 설명 | 예시 |
|---|---|---|---|
| `/bot_spawn <id> <empire: 1-3>` | 관리자 | 해당 ID의 봇을 게임에 등장시킵니다 (`1` = 신수국, `2` = 천조국, `3` = 진노국). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | 관리자 | 봇을 월드에서 로그아웃시킵니다. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <empire>` | 관리자 | 일정 범위의 봇들을 게임에 등장시킵니다. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | 관리자 | 일정 범위의 봇들을 로그아웃시킵니다. | `/bot_despawn_many 4 350` |
| `/bot_rank` | 모두 | 활성 봇들의 레벨 랭킹을 채팅에 표시합니다. | `/bot_rank` |

## 🗄️ 데이터베이스 접속 (Navicat, HeidiSQL, DBeaver)

서버의 데이터베이스는 컨테이너 안의 MariaDB이며, **이 컴퓨터에서만** 접속할 수
있습니다(`127.0.0.1`, 포트 `3306`, 또는 `.env`에 `M2_DB_PUBLISH_PORT`가 설정되어
있다면 다른 포트). 데이터베이스 클라이언트에서 새 연결: 유형 MySQL/MariaDB, 호스트
`127.0.0.1`, 포트 `3306`.

| 계정 | 용도 | 비밀번호 |
|---|---|---|
| `root` | 전체 | 서버 폴더의 `linux-port\docker\.env`에 있는 `M2_DB_ROOT_PASSWORD` |
| `metin2` | 게임 데이터베이스 전용 (`account`, `player`, `log`, `common`, `hotbackup`) | 같은 파일의 `M2_DB_PASSWORD` |

가장 빠른 방법: 런처의 **DATABASE LOGIN (NAVICAT)**(데이터베이스 로그인) 버튼을
누르면 호스트, 포트, 두 비밀번호가 복사할 수 있는 입력란에 표시됩니다. 비밀번호는
첫 실행 때 무작위로 생성되며 "기본" 비밀번호는 없습니다. Discord에 붙여 넣지 마세요.

데이터베이스 클라이언트가 `1045 - Access denied for user 'root'@'172.18.0.1'`이라고
응답한다면, 데이터베이스가 지금 `.env`에 있는 것과 다른 비밀번호로 만들어진 것입니다.
**REPAIR DATABASE ACCESS**(데이터베이스 접속 복구)를 누르세요. 런처가 서버를 멈추고
`metin2`와 `root` 계정의 비밀번호를 `.env`에 있는 값으로 설정하며, 캐릭터, 아이템,
봇은 전혀 건드리지 않습니다. 그런 다음 PLAY를 누르고 다시 로그인하세요.

## 📜 라이선스

버전 2.2.55부터 이 프로젝트는 **[CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/deed.ko)**(저작자표시-비영리-변경금지) 라이선스를 따릅니다:

- 플레이하고, 자신과 친구들을 위한 서버를 운영하고, 공식 패키지를 변경 없이 공유하는 것은 **허용됩니다**. 이때 프로젝트 링크와 함께 저작자를 표시해야 합니다.
- 수정된 버전이나 별도의 배포판을 배포하는 것은 **허용되지 않습니다**. 프로젝트용 애드온은 만들어서 프로젝트의 Discord에서 공유할 수 있습니다.
- 이것으로 돈을 버는 것은 **허용되지 않습니다**: 판매 금지, 서버 접속에 대한 요금 부과 금지, 게임 내 아이템 판매 금지. 게임 내에서 아무런 이점도 주지 않는 서버 운영을 위한 자발적 후원과 게임 플레이 녹화 및 스트리밍은 괜찮습니다.

2.2.16 이하 버전은 MIT 라이선스([LICENSE-MIT.txt](LICENSE-MIT.txt)), 2.2.17부터 2.2.54까지의 버전은 CC BY-NC-SA 4.0을 따릅니다. Metin2는 Ymir Interactive와 Webzen의 소유이며, 서버 파일 패키지는 각 제작자의 소유입니다. 자세한 내용은 [LICENSE](LICENSE)와 [NOTICE.md](NOTICE.md)에 있습니다.

**패키지에 포함된 Metin2 파일.** 프로젝트의 패키지(전체 패키지와 클라이언트 및 서버 업데이트)에는 프로젝트 제작자의 작업물이 아닌 파일도 포함되어 있습니다: Ymir Interactive와 Webzen 소유의 Metin2 클라이언트 및 게임 데이터, 그리고 서버 파일 패키지의 파일(TMP4의 r40250, Martysama의 mt2009). 프로젝트의 라이선스는 이 파일들에 적용되지 않으며 이에 대한 어떠한 권리도 부여하지 않습니다. 패키지를 변경 없이 공유할 수 있는 허가는 프로젝트 제작자의 작업물에만 해당합니다.

### 삭제 요청

권리 보유자(Ymir Interactive, Webzen, Gameforge, 서버 파일 패키지의 제작자 또는 그 밖의 누구든)로서 여기 있는 무언가의 삭제를 원하신다면, 이 저장소에 [issue를 열거나](https://github.com/TieruYT/metin2-playerbots/issues) [프로젝트의 Discord](https://discord.gg/DyCxXdZtnV)에 글을 남겨 주세요. 파일이나 릴리스를 알려 주시는 것으로 충분합니다. 이의 없이, 공식 DMCA 통지 없이도 신속하게 삭제하겠습니다. 물론 DMCA 통지도 가능합니다.

## 🤝 감사의 말

- **AzzlackSyndicate**: 이 프로젝트의 출발점이 된 원래의 Linux 포트 기반, 설치 프로그램, 패널의 제작자 (MIT, [NOTICE.md](NOTICE.md) 참고).
- **Iwakura**: 봇의 성격 시스템, 가격표, 아이템 티어, 상점과 길드 이름, 봇의 닉네임, Community Patches.
- **seban latino**: 설치에 포함된 두 번째 패널인 Metin2 Singleplayer Panel과 그 실시간 맵, 프로필, 랭킹, 경제.
- **ĹŌŞƬĒĶ**: 로그인 화면과 Discord Rich Presence, 언어 팩, 봇과의 귓속말 대화, 봇 머리 위의 성격 표시줄, 행운의 수레바퀴 원판.
- **Colide**: 자동 사냥 창, 커뮤니티 창, 동료의 창고·오프라인 계획·낚시, [AUTO]/[AFK] 표시 그래픽.
- **OskarPWA**: 봇 창고 창, 스킬 아이콘, F9 GM 패널의 첫 버전.
- **Tyrion**: 오프라인 상점에서 특정 아이템 검색, 할로윈 이벤트, 독일어·프랑스어·포르투갈어·루마니아어 번역.
- **Uxìĕ [DSO]**: Dom Towarowy, 화면 전체의 몬스터를 끌어오는 용기의 망토, 인벤토리 사이드 패널.
- **Gibon**: 상자에 무엇이 들어 있는지, 몬스터가 무엇을 드롭하는지 보여 주는 미리보기.
- **Kiciamol**: 대상 체력 바의 체력 표시(전투 중인 플레이어 포함), F9 GM 패널, 가이드가 포함된 게임 내 위키, 행운의 수레바퀴, 육각 상자, 최대 30포인트의 콤보, Snakefield 베타.
- **NerrVoVy**: 역병 던전, 사냥꾼의 대장간, GF 스타일 퀘스트 창, ItemShop 반짝 경매, 대상 보너스 미리보기, 채팅의 왕국 깃발.
- **Said (saido_0)**: 터키어 번역과 약초꾼의 길.
- **blasty**: 보너스 스위처.
- **Piciu713**: 대장장이 창의 강화 확률 표시.
- **Kenny, Pabloo, Mur4s**: 파티 아이템 줍기, 플레이어 파티 속 봇의 심부름, 곰 퀘스트, 순간이동 반지에 대한 수정.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab): MMO 게임 속 자율 에이전트에 대한 연구적 영감.
- Discord 커뮤니티: 이 프로젝트의 대부분이 탄생한 테스트, 버그 신고, 아이디어.
