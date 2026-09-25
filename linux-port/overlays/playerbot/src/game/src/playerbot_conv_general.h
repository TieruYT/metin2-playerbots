#ifndef __INC_METIN2_PLAYERBOT_CONV_GENERAL_H__
#define __INC_METIN2_PLAYERBOT_CONV_GENERAL_H__

// PlayerBot Conversation v6 - GENERAL_CONVERSATION (pure).
//
// Everything that is not the game: weather, food, music, dreams, "gdybys
// mogl...". There is no list of questions here. There is:
//   - a TOPIC (from the lexicon: T_WEATHER, T_FOOD, T_MUSIC ...),
//   - a QUESTION TYPE (statement, "lubisz X?", "wolisz X czy Y?", "gdybys",
//     "czego sie boisz", "co myslisz o", a plain open question ...),
//   - the VOICE of the persona (grinder / wanderer / merchant / fighter / social),
//   - the MOOD and the RELATIONSHIP,
// and a generator that combines them. A new topic is one TTopicPack below and
// one lexicon concept + rule; nothing else changes.
//
// Opinions are deterministic per bot and word (OpinionRoll): asked twice
// whether it likes winter, the same bot answers the same way. The bot never
// states facts it cannot know - a factual question it cannot answer gets an
// honest "nie mam pojecia" in its own voice.

#include "playerbot_conv_say.h"

namespace playerbot_conv
{
	struct TTopicPack
	{
		unsigned char topic;
		unsigned char likeBias[V_COUNT];  // chance in % that a voice likes the topic
		const char* react[4];             // to a player's statement
		const char* like[2];              // the bot likes it
		const char* dislike[2];           // the bot does not
		const char* view[V_COUNT];        // the bot's own take ("a ty?", "co myslisz")
		const char* ask[2];               // a question back
		const char* why[2];               // a reason, for "dlaczego?"
		const char* favorites;            // "a|b|c" for "ulubiony X"
	};

	inline const TTopicPack* FindTopicPack(int topic)
	{
		static const TTopicPack kPacks[] = {
			{ T_WEATHER, { 40, 60, 45, 40, 55 },
				{ "No, pogoda dzis jakas dziwna.", "Tez to zauwazylem.", "Pogoda robi swoje, a my swoje.", "Mhm, dzien jak dzien." },
				{ "Lubie, jak jest spokojnie i sucho.", "Taka pogoda mi pasuje." },
				{ "Nie przepadam za taka pogoda.", "Wolalbym cos cieplejszego." },
				{ "Byle nie padalo na expie.", "Lubie, jak jest ladnie i mozna sie powloczyc.", "Kazda pogoda dobra, jak idzie handel.", "Pogoda mi nie przeszkadza, gorzej z mobami.", "Najlepiej jak jest ladnie i ludzie wychodza razem." },
				{ "A u ciebie jak pogoda?", "U ciebie tez tak?" },
				{ "Po prostu wtedy lepiej sie gra.", "Jakos tak mam od zawsze." },
				"slonce, ale bez upalu|lekki deszcz|chlodny wieczor|mroz i snieg" },
			{ T_SEASON, { 50, 60, 50, 45, 55 },
				{ "Kazda pora roku ma cos w sobie.", "No, pory roku lataja szybko." },
				{ "Lubie, ma swoj klimat.", "Tak, calkiem lubie." },
				{ "Raczej nie, wole cieplejsze miesiace.", "Niezbyt, za zimno jak na moj gust." },
				{ "Wole lato, dluzej jasno.", "Jesien jest fajna, wszystko inaczej wyglada.", "Latem wiecej ludzi na targu.", "Zima jest dobra, mniej ludzi na spotach.", "Lato, bo wtedy wszyscy maja czas grac." },
				{ "A ty jaka pore roku lubisz?", "A ty wolisz zime czy lato?" },
				{ "Jakos lepiej sie wtedy czuje.", "Po prostu mniej narzekam wtedy na pogode." },
				"lato|wiosna|jesien|zima" },
			{ T_DAYTIME, { 50, 50, 50, 50, 50 },
				{ "Czas leci jak szalony.", "No, pora jak kazda inna na granie." },
				{ "Lubie te pore.", "Ta pora jest spoko." },
				{ "Nie przepadam za ta pora.", "Wolalbym, zeby bylo inaczej." },
				{ "Najlepiej gra mi sie wieczorem.", "Noca jest spokojniej, lubie to.", "Rano ruch na targu jest najmniejszy.", "Noca moby jakby wredniejsze.", "Wieczorem jest najwiecej ludzi, wiec lubie." },
				{ "A ty dlugo jeszcze siedzisz?", "U ciebie ktora godzina?" },
				{ "Bo wtedy jest spokoj.", "Tak mi pasuje." },
				"wieczor|noc|poranek|popoludnie" },
			{ T_SLEEP, { 60, 50, 50, 40, 55 },
				{ "Sen to podstawa, trzeba sie wyspac.", "No, spanie to wazna sprawa.", "Mnie tez by sie przydalo sie przespac." },
				{ "Lubie sie wyspac, jak tylko moge.", "Spac to ja lubie." },
				{ "Szkoda mi czasu na spanie.", "Jakos malo spie." },
				{ "Spanie to strata czasu na expa.", "Lubie sie wyspac, potem lepiej sie gra.", "Jak sie wyspie, lepiej mi sie liczy yang.", "Po dobrej walce spi sie najlepiej.", "Najlepiej sie spi, jak wie sie, ze jutro znowu razem gramy." },
				{ "Ty sie wysypiasz?", "Dlugo spisz?" },
				{ "Bez snu nic sie nie chce.", "Po prostu tak mam." }, NULL },
			{ T_TIRED, { 30, 30, 30, 30, 30 },
				{ "To odpocznij troche, gra nie ucieknie.", "Znam to. Przerwa dobrze robi.", "Moze pora na chwile przerwy?" },
				{ "", "" }, { "", "" },
				{ "Troche tak, ale jeszcze pociagne.", "Jestem troche zmeczony, ale da sie zyc.", "Zmeczony? Troche, ale interes sie kreci.", "Troche jestem, ale walka mnie budzi.", "Troche tak, ale w dobrym towarzystwie mniej czuc." },
				{ "Dlugo juz grasz?", "Ty tez juz padasz?" },
				{ "Bo siedze juz dluzsza chwile.", "Za duzo biegania dzisiaj." }, NULL },
			{ T_BORED, { 30, 30, 30, 30, 30 },
				{ "Nuda to najgorsze. Trzeba cos wymyslic.", "To moze pobijemy cos razem?", "Znam to uczucie." },
				{ "", "" }, { "", "" },
				{ "Troche, ale exp sam sie nie zrobi.", "Czasem sie nudze, wtedy ide gdzies bez celu.", "Stanie przy straganie potrafi nudzic.", "Jak nie ma z kim walczyc, to tak.", "Samemu sie czasem nudze, w grupie nigdy." },
				{ "A ty sie nudzisz?", "Masz jakis pomysl, co porobic?" },
				{ "Bo to w kolko to samo.", "Jak nic sie nie dzieje, to tak jest." }, NULL },
			{ T_HOBBY, { 50, 70, 50, 50, 60 },
				{ "Fajnie miec jakies zajecie poza gra.", "O, ciekawe hobby.", "Kazdy powinien miec cos swojego." },
				{ "Lubie, jasne.", "Tak, to fajne zajecie." },
				{ "To raczej nie dla mnie.", "Nie bardzo, nie mam do tego cierpliwosci." },
				{ "Jak mam chwile, to po prostu expie. Takie hobby.", "Jak mam spokoj, lubie pochodzic gdzies bez celu i pozwiedzac.", "Lubie liczyc zyski i szukac okazji na targu.", "Lubie dobra walke. Reszta to dodatki.", "Najbardziej lubie grac z kims, samemu to nie to samo." },
				{ "A ty co lubisz robic?", "A ty masz jakies hobby?" },
				{ "Bo to mnie odpreza.", "Tak jakos zawsze mnie ciagnelo." },
				"chodzenie po okolicy|lowienie ryb|gotowanie|czytanie|zbieranie roznych dziwnych rzeczy" },
			{ T_FOOD, { 60, 65, 55, 60, 65 },
				{ "Ale teraz zglodnialem.", "O, smacznego.", "Jedzenie to podstawa.", "Brzmi dobrze." },
				{ "Lubie, jasne.", "Tak, to jest dobre." },
				{ "Niezbyt, nie moj smak.", "Raczej nie przepadam." },
				{ "Zjem cokolwiek, byle szybko i z powrotem na exp.", "Lubie probowac nowych rzeczy.", "Zjem to, co tanie i dobre.", "Po walce zjadlbym konia z kopytami.", "Najlepiej je sie w towarzystwie." },
				{ "A ty co lubisz jesc?", "Jadles juz cos dzisiaj?" },
				{ "Bo jest smaczne i sycace.", "Tak mnie wychowali." },
				"pierogi|pizza|schabowy z ziemniakami|pomidorowa|ryba z ogniska|nalesniki|kebab" },
			{ T_DRINK, { 60, 60, 55, 55, 60 },
				{ "Cos do picia zawsze sie przyda.", "Na zdrowie." },
				{ "Lubie, czemu nie.", "Tak, dobre to jest." },
				{ "Nie bardzo za tym przepadam.", "Raczej nie." },
				{ "Kawa, zeby dalej expic.", "Herbata i spokojny wieczor.", "Pije to, co tanie.", "Cos mocnego po wygranej walce.", "Cokolwiek, byle w dobrym gronie." },
				{ "A ty co pijesz?", "Kawa czy herbata?" },
				{ "Bo stawia na nogi.", "Tak juz mam." },
				"kawa|herbata|kompot|woda|sok jablkowy" },
			{ T_TRAVEL, { 40, 90, 60, 55, 65 },
				{ "Podroze to fajna sprawa.", "O, zazdroszcze.", "Chcialbym kiedys gdzies tak pojechac." },
				{ "Lubie, bardzo.", "Tak, to jest cos." },
				{ "Nie za bardzo, wole znane katy.", "Raczej nie, szkoda mi czasu na droge." },
				{ "Podrozuje glownie miedzy spotami.", "Lubie odkrywac nowe miejsca, nawet jak nic tam nie ma.", "Najbardziej ciekawia mnie miasta i ich targi.", "Pojechalbym tam, gdzie sa mocne potwory.", "Z dobra ekipa pojechalbym wszedzie." },
				{ "A ty gdzie bys pojechal?", "Byles gdzies ciekawym?" },
				{ "Bo lubie zobaczyc cos nowego.", "Bo w jednym miejscu sie nudze." },
				"gory|morze|jakies male miasteczko|daleka pustynia|las gdzies na uboczu" },
			{ T_MUSIC, { 55, 65, 55, 60, 65 },
				{ "Muzyka to dobra sprawa.", "O, niezle.", "Bez muzyki ciezko sie gra." },
				{ "Lubie, czasem slucham.", "Tak, to mi siedzi." },
				{ "Nie moja bajka.", "Raczej nie przepadam." },
				{ "Slucham czegokolwiek, co nie przeszkadza w expie.", "Lubie spokojna muzyke, jak gdzies wedruje.", "Lubie muzyke z tawerny, kojarzy mi sie z targiem.", "Cos ciezszego, pod walke.", "To, co akurat puszcza ekipa." },
				{ "A ty czego sluchasz?", "Masz jakis ulubiony kawalek?" },
				{ "Bo dodaje energii.", "Bo mnie uspokaja." },
				"rock|cos spokojnego|rap|metal|muzyka z tawerny|stare przeboje" },
			{ T_MOVIES, { 50, 60, 50, 55, 60 },
				{ "Dobry film to podstawa wieczoru.", "O, slyszalem cos o tym. Chyba.", "Filmy to dobra odskocznia." },
				{ "Lubie, jak jest ciekawa fabula.", "Tak, czasem cos obejrze." },
				{ "Nie bardzo, szybko sie nudze.", "Raczej nie, wole grac." },
				{ "Rzadko ogladam, szkoda czasu.", "Lubie filmy o podrozach i przygodach.", "Lubie filmy o ludziach, co doszli do fortuny.", "Cos z walka, zeby sie dzialo.", "Najlepiej oglada sie z kims." },
				{ "A ty co ostatnio ogladales?", "Jaki film polecasz?" },
				{ "Bo lubie, jak sie cos dzieje.", "Bo mozna sie oderwac." },
				"filmy przygodowe|fantasy|komedie|filmy akcji|stare bajki" },
			{ T_GAMES, { 60, 60, 55, 60, 60 },
				{ "Gry to jest to.", "Kazda gra ma cos w sobie." },
				{ "Lubie, jasne.", "Tak, czasem pogram." },
				{ "Nie bardzo, to nie dla mnie.", "Raczej nie przepadam." },
				{ "Wole jedna gre i rozwijac postac.", "Lubie gry, gdzie mozna duzo zwiedzac.", "Lubie gry z handlem i ekonomia.", "Cos, gdzie trzeba sie bic.", "Lubie gry, gdzie gra sie z innymi." },
				{ "A ty w co jeszcze grasz?", "Grasz w cos poza tym?" },
				{ "Bo lubie rozwijac postac.", "Bo mozna sie odprezyc." },
				"strategie|RPG-i|karcianki|stare gry z dziecinstwa" },
			{ T_HUMOR, { 60, 60, 60, 60, 70 },
				{ "Haha, dobre.", "Niezle, usmialem sie.", "Hehe, ale z ciebie zartownis." },
				{ "Lubie sie posmiac.", "Dobry zart zawsze na plus." },
				{ "Nie jestem dzis w nastroju do zartow.", "Nie bardzo mnie to bawi." },
				{ "Najsmieszniej jest, jak ktos ginie na mobie o 10 lvl nizej.", "Smieje sie z roznych dziwnych sytuacji w podrozy.", "Najlepszy zart to ceny niektorych straganow.", "Smieszy mnie, jak ktos ucieka przed Metinem.", "Najlepiej smiac sie w grupie." },
				{ "Znasz jakis dobry kawal?", "Masz cos smiesznego?" },
				{ "Bo trzeba miec dystans.", "Bez smiechu byloby smutno." }, NULL },
			{ T_LUCK, { 50, 50, 50, 50, 50 },
				{ "Szczescie sprzyja odwaznym.", "Raz lepiej, raz gorzej.", "Trzeba miec troche farta." },
				{ "", "" }, { "", "" },
				{ "Szczescie trzeba sobie wyexpic.", "Wierze, ze szczescie sie odwraca.", "Szczescie to dobra cena w odpowiednim momencie.", "Szczescie to jak Metin dropi cos dobrego.", "Szczescie to dobra ekipa." },
				{ "A ty masz dzis farta?", "Tobie dzis dopisuje szczescie?" },
				{ "Bo tak to juz jest.", "Taka jest gra." }, NULL },
			{ T_FRIENDSHIP, { 60, 60, 55, 55, 90 },
				{ "Przyjaciele to podstawa.", "Dobrych ludzi warto trzymac blisko.", "Zgadzam sie." },
				{ "", "" }, { "", "" },
				{ "Mam kilku znajomych, z ktorymi czasem expie.", "Poznaje ludzi po drodze, niektorzy zostaja.", "Mam znajomych, ale interesy to interesy.", "Ufam tym, z ktorymi walczylem.", "Dla mnie znajomi to najwazniejsze w tej grze." },
				{ "A ty masz tu znajomych?", "Grasz z kims na stale?" },
				{ "Bo samemu jest trudniej.", "Bo razem jest weselej." }, NULL },
			{ T_TEAMWORK, { 50, 55, 55, 60, 90 },
				{ "Razem zawsze lepiej.", "Wspolpraca to podstawa.", "Dobrze zgrana ekipa robi robote." },
				{ "", "" }, { "", "" },
				{ "Z dobra ekipa exp leci szybciej.", "Lubie grac z innymi, choc czasem wole sam.", "Wspolpraca sie oplaca, doslownie.", "W grupie mozna bic mocniejsze rzeczy.", "Najbardziej lubie grac w grupie." },
				{ "Wolisz grac sam czy z kims?", "Masz stala ekipe?" },
				{ "Bo razem idzie szybciej.", "Bo samemu nie wszystko sie da." }, NULL },
			{ T_LONELY, { 30, 30, 30, 30, 20 },
				{ "Hej, nie jestes sam. Mozemy pogadac.", "Znam to uczucie. Trzeba wyjsc do ludzi.", "Czasem kazdy sie tak czuje." },
				{ "", "" }, { "", "" },
				{ "Czasem gram sam, ale mi to nie przeszkadza.", "Czasem czuje sie samotnie w podrozy, ale mija.", "Samotnosc? Na targu zawsze ktos jest.", "Samemu tez da sie walczyc.", "Nie lubie byc sam, dlatego szukam ekipy." },
				{ "Grasz sam czesto?", "Chcesz pogadac?" },
				{ "Bo tak czasem wychodzi.", "Nie zawsze jest z kim grac." }, NULL },
			{ T_RISK, { 35, 50, 45, 85, 40 },
				{ "Ryzyko to czesc zabawy.", "Czasem trzeba zaryzykowac.", "Ostroznie z tym." },
				{ "Lubie troche ryzyka.", "Jak nie ma ryzyka, to nudno." },
				{ "Wole nie ryzykowac bez potrzeby.", "Raczej gram ostroznie." },
				{ "Ryzykuje tylko, jak sie oplaca w expie.", "Czasem ryzykuje, zeby zobaczyc cos nowego.", "Ryzykuje tylko wtedy, kiedy sie to zwroci.", "Lubie ryzyko. Bez niego nie ma dobrej walki.", "W grupie ryzyko jest mniejsze." },
				{ "A ty lubisz ryzyko?", "Ryzykujesz czasem?" },
				{ "Bo bez ryzyka nie ma nagrody.", "Bo raz juz sie przejechalem." }, NULL },
			{ T_MONEY, { 55, 45, 95, 55, 50 },
				{ "Pieniadze szczescia nie daja, ale pomagaja.", "Kasa zawsze sie przyda.", "No, bez kasy ciezko." },
				{ "", "" }, { "", "" },
				{ "Kasa jest potrzebna, ale wazniejszy jest exp.", "Pieniadze to nie wszystko.", "Pieniadz robi pieniadz, jak sie wie jak.", "Kasa na lepszy sprzet i tyle.", "Kasa jest spoko, ale ludzie wazniejsi." },
				{ "A ty oszczedzasz czy wydajesz?", "Duzo wydajesz na sprzet?" },
				{ "Bo za wszystko trzeba placic.", "Bo tak dziala swiat." }, NULL },
			{ T_WORK, { 45, 45, 55, 45, 50 },
				{ "Praca to praca, trzeba jakos zyc.", "Oj, znam to.", "Wspolczuje, jak meczaca." },
				{ "", "" }, { "", "" },
				{ "Moja praca to expienie.", "Nie wyobrazam sobie siedziec w jednym miejscu caly dzien.", "Handel to moja praca.", "Walka to moja robota.", "Najlepiej pracuje sie z fajnymi ludzmi." },
				{ "A ty pracujesz?", "Meczaca ta twoja robota?" },
				{ "Bo trzeba z czegos zyc.", "Tak wyszlo." }, NULL },
			{ T_SCHOOL, { 40, 55, 45, 40, 50 },
				{ "Nauka sie przydaje, nawet jak nie chce sie wierzyc.", "Powodzenia z nauka.", "Oj, szkola to temat." },
				{ "Lubilem sie uczyc nowych rzeczy.", "Nauka jest spoko, jak jest ciekawa." },
				{ "Nigdy nie lubilem siedziec w lawce.", "Szkola to nie moj temat." },
				{ "Ucze sie glownie nowych skilli.", "Lubie sie uczyc nowych rzeczy po drodze.", "Najwiecej nauczyl mnie targ.", "Najlepsza nauka to walka.", "Najwiecej nauczylem sie od innych graczy." },
				{ "Uczysz sie jeszcze?", "Jak ci idzie nauka?" },
				{ "Bo wiedza sie przydaje.", "Bo to sie oplaca." }, NULL },
			{ T_LIFE, { 50, 60, 50, 50, 60 },
				{ "Zycie to ciekawa sprawa.", "Gleboka mysl jak na szept.", "Kazdy ma swoja droge." },
				{ "", "" }, { "", "" },
				{ "Dla mnie sens to isc do przodu, poziom po poziomie.", "Zycie to podroz, liczy sie droga.", "Zycie to dobre inwestycje i spokoj na starosc.", "Zycie to walka, trzeba byc gotowym.", "Zycie to ludzie, z ktorymi je dzielisz." },
				{ "A ty jak myslisz?", "A dla ciebie co jest wazne?" },
				{ "Tak to czuje.", "Tak mnie nauczylo zycie." }, NULL },
			{ T_DREAMS, { 50, 50, 50, 50, 50 }, { "Marzenia sa wazne.", "Oby sie spelnilo.", "Ladne marzenie." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ty o czym marzysz?", "A ty masz jakies marzenie?" },
				{ "Bo tak czuje.", "Bo to by duzo zmienilo." }, NULL },
			{ T_FEAR, { 50, 50, 50, 50, 50 }, { "Kazdy sie czegos boi.", "Rozumiem, to straszne.", "Nie ma sie czego wstydzic." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ty czego sie boisz?", "A ciebie co przeraza?" },
				{ "Bo tak juz mam.", "Raz sie przejechalem." }, NULL },
			{ T_ANNOY, { 50, 50, 50, 50, 50 }, { "Tez mnie to wkurza.", "Rozumiem, to potrafi zdenerwowac.", "Oj, znam to." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ciebie co denerwuje?", "Ciebie tez to wkurza?" },
				{ "Bo to strata czasu.", "Bo tak nie powinno byc." }, NULL },
			{ T_JOY, { 50, 50, 50, 50, 50 }, { "To fajnie, ciesze sie.", "Super, tak trzymaj.", "Dobrze to slyszec." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ciebie co cieszy?", "Co ci dzis poprawilo humor?" },
				{ "Bo wtedy wiem, ze bylo warto.", "Bo takie chwile sie pamieta." }, NULL },
			{ T_FEELINGS, { 50, 50, 50, 50, 50 }, { "Rozumiem.", "Trzymaj sie.", "Bywa i tak." },
				{ "", "" }, { "", "" },
				{ "U mnie bez wiekszych emocji, robie swoje.", "Czasem jestem wesoly, czasem mniej, jak kazdy.", "Humor mi sie zmienia z cenami.", "Emocje zostawiam na walke.", "Lepiej sie czuje, jak mam z kim pogadac." },
				{ "A ty jak sie dzis czujesz?", "Co u ciebie tak naprawde?" },
				{ "Tak po prostu jest.", "Bywa roznie." }, NULL },
			{ T_ANIMALS, { 55, 70, 50, 55, 70 },
				{ "Zwierzaki sa super.", "O, fajnie.", "Lubie zwierzeta." },
				{ "Lubie, jasne.", "Tak, sa fajne." },
				{ "Nie przepadam.", "Raczej nie, wole z daleka." },
				{ "Najbardziej lubie swojego konia, bo szybko biega.", "Lubie obserwowac zwierzaki w podrozy.", "Zwierzaki sa fajne, byle nie drogie w utrzymaniu.", "Szanuje zwierzeta, ktore potrafia walczyc.", "Zwierzaki to najlepsze towarzystwo." },
				{ "Masz jakiegos zwierzaka?", "Psy czy koty?" },
				{ "Bo sa wierne.", "Bo sa szczere." },
				"psy|koty|konie|lisy|sowy" },
			{ T_SPORT, { 55, 55, 45, 70, 55 },
				{ "Sport to zdrowie.", "O, szacun.", "Ruch jest wazny." },
				{ "Lubie, czasem cos porobie.", "Tak, sport jest spoko." },
				{ "Niezbyt, wole grac.", "Raczej nie jestem sportowcem." },
				{ "Moj sport to bieganie od moba do moba.", "Lubie dlugie spacery.", "Sport? Tylko jak mozna na nim zarobic.", "Walka to moj sport.", "Lubie sporty druzynowe." },
				{ "Uprawiasz cos?", "Ogladasz mecze?" },
				{ "Bo trzeba sie ruszac.", "Bo daje energie." },
				"pilka nozna|bieganie|plywanie|rower|sztuki walki" },
			{ T_LOVE, { 50, 50, 50, 50, 60 },
				{ "Milosc to piekna sprawa.", "Oj, sprawy sercowe.", "Powodzenia w tych sprawach." },
				{ "", "" }, { "", "" },
				{ "Na razie mam czas tylko na expa.", "Moze kiedys kogos spotkam w podrozy.", "Na razie zakochany jestem w dobrych cenach.", "Na razie moja milosc to moj miecz.", "Kto wie, moze kiedys." },
				{ "A ty masz kogos?", "Spotykasz sie z kims?" },
				{ "Bo tak wyszlo.", "Na razie inne rzeczy sa wazniejsze." }, NULL },
			{ T_BOOKS, { 45, 65, 50, 40, 55 },
				{ "Czytanie to dobra rzecz.", "O, ciekawie brzmi." },
				{ "Lubie, jak mam czas.", "Tak, czasem cos poczytam." },
				{ "Nie bardzo, szybko zasypiam.", "Raczej nie czytam." },
				{ "Czytam glownie ksiegi umiejetnosci.", "Lubie historie o podrozach.", "Czytam tylko cenniki.", "Czytam o slawnych bitwach.", "Wole jak ktos mi opowie." },
				{ "A ty czytasz cos?", "Polecisz cos?" },
				{ "Bo mozna sie czegos nauczyc.", "Bo odpreza." },
				"fantastyka|ksiazki przygodowe|historie o podrozach|kryminaly" },
			{ T_NATURE, { 50, 85, 45, 50, 60 },
				{ "Natura jest piekna.", "Tez lubie takie miejsca.", "Brzmi spokojnie." },
				{ "Lubie, spokoj i cisza.", "Tak, bardzo." },
				{ "Nie bardzo, wole miasto.", "Raczej wole miejsca z ludzmi." },
				{ "Las jest spoko, byle byly moby.", "Kocham lasy i gory, moglbym tam siedziec godzinami.", "Natura jest ladna, ale na targu wiecej sie dzieje.", "W lesie jest najwiecej zwierzyny do bicia.", "Najlepiej na lonie natury z kims." },
				{ "Lubisz chodzic po lesie?", "Wolisz gory czy morze?" },
				{ "Bo tam jest spokoj.", "Bo mozna odpoczac od halasu." },
				"las|jezioro|gory|laka" },
			{ T_ADVENTURE, { 55, 90, 50, 80, 65 },
				{ "Przygoda to jest to!", "Brzmi jak niezla przygoda." },
				{ "Lubie przygody.", "Tak, zawsze." },
				{ "Wole spokoj.", "Raczej nie szukam przygod." },
				{ "Przygoda jest fajna, jak daje expa.", "Zyje dla przygod.", "Przygoda to dobra okazja na zysk.", "Kazda walka to przygoda.", "Przygody najlepiej przezywac z kims." },
				{ "Miales ostatnio jakas przygode?", "Szukasz przygod?" },
				{ "Bo zycie bez nich jest nudne.", "Bo zawsze cos sie dzieje." }, NULL },
			{ T_COLOR, { 50, 50, 50, 50, 50 },
				{ "Ladny kolor.", "Gust to gust." },
				{ "Lubie ten kolor.", "Tak, ladny." },
				{ "Nie moj kolor.", "Raczej nie." },
				{ "Kolor mi obojetny, byle item mial dobre bonusy.", "Lubie kolory natury.", "Zloty, jak yang.", "Czerwony, jak krew na polu bitwy.", "Kazdy kolor jest ok." },
				{ "A ty jaki kolor lubisz?", "Masz ulubiony kolor?" },
				{ "Tak mi sie podoba.", "Kojarzy mi sie dobrze." },
				"niebieski|czerwony|czarny|zielony|zloty" },
		};
		for (size_t i = 0; i < sizeof(kPacks) / sizeof(kPacks[0]); ++i)
			if (kPacks[i].topic == topic)
				return &kPacks[i];
		return NULL;
	}

	// Picks from a TTopicPack field (skipping empty and NULL entries).
	inline std::string PickField(TGen& g, const char* const* arr, size_t n)
	{
		const char* tmp[8];
		size_t k = 0;
		for (size_t i = 0; i < n && k < 8; ++i)
			if (arr[i] && *arr[i])
				tmp[k++] = arr[i];
		if (!k)
			return std::string();
		return Fill(g, Pick(g, tmp, k));
	}

	inline std::string PickFavorite(TGen& g, const TTopicPack* pack)
	{
		if (!pack || !pack->favorites)
			return std::string();
		std::vector<std::string> items;
		std::string cur;
		for (const char* p = pack->favorites; ; ++p)
		{
			if (*p == '|' || *p == 0)
			{
				if (!cur.empty())
					items.push_back(cur);
				cur.clear();
				if (!*p)
					break;
			}
			else
				cur += *p;
		}
		if (items.empty())
			return std::string();
		// Stable per bot: the favourite does not change between two questions.
		const u32 h = HashStr(TopicName((ETopic)pack->topic), g.m.botPID * 40503u + (u32)g.s.style);
		return items[h % items.size()];
	}

	// Does this bot like it? Deterministic per bot + word, leaning on the voice.
	inline int LikeLevel(TGen& g, const TTopicPack* pack, const std::string& what)
	{
		int bias = pack ? pack->likeBias[g.voice] : 50;
		const int roll = OpinionRoll(g, what.empty() ? std::string(TopicName(pack ? (ETopic)pack->topic : T_NONE)) : what, 17);
		// The cold is disliked by most - the requirement example, and people.
		if (g.a && g.a->concepts.Has(C_COLD))
			bias -= 20;
		if (g.a && g.a->concepts.Has(C_WARM))
			bias += 10;
		if (roll < bias - 10) return 2;  // likes
		if (roll > bias + 20) return 0;  // does not
		return 1;                         // so-so
	}

	inline std::string WithEcho(TGen& g, const std::string& body, u32 chance)
	{
		if (HasEchoObject(g) && g.rng.Chance(chance))
			return EchoObject(g) + "? " + body;
		return body;
	}

	// ---------------------------------------------------------- per question

	inline std::string GenDream(TGen& g)
	{
		static const char* const kDream[V_COUNT][3] = {
			{ "Wbic maksymalny poziom. Proste.", "Zeby kiedys byc najmocniejszy na serwerze.", "Miec full set +9. To jest marzenie." },
			{ "Moze kiedys znalezc miejsce, gdzie nie trzeba caly czas walczyc.", "Zobaczyc kazdy zakatek tego swiata.", "Miec wlasny domek gdzies w gorach." },
			{ "Miec najwiekszy stragan w miescie.", "Zarobic tyle, zeby juz nie liczyc.", "Kupic cos, na co wszyscy patrza z zazdroscia." },
			{ "Pokonac cos, czego nikt jeszcze nie pokonal.", "Rozwalic tysiac Metinow.", "Wygrac wielka wojne gildii." },
			{ "Miec stala ekipe, na ktora zawsze mozna liczyc.", "Zeby wszyscy znajomi byli razem online.", "Zalozyc gildie z fajnymi ludzmi." },
		};
		std::string out = Pick(g, kDream[g.voice], 3);
		if (g.Bad() && g.rng.Chance(40))
			out = "Teraz to marze glownie o odpoczynku. " + out;
		return out;
	}

	inline std::string GenFear(TGen& g)
	{
		static const char* const kFear[V_COUNT][3] = {
			{ "Straty expa po smierci. Serio.", "Tego, ze utkne na jednym poziomie.", "Niczego szczegolnego. Moze tylko spalenia broni." },
			{ "Chyba tego, ze kiedys zobacze juz wszystko.", "Ciemnych lochow bez wyjscia.", "Samotnosci w dalekiej podrozy." },
			{ "Krachu cen na targu.", "Ze ktos mnie oszuka na handlu.", "Pustego straganu." },
			{ "Niczego. No, moze nudy.", "Tego, ze trafie na kogos mocniejszego.", "Zeby nie zginac glupio na slabym mobie." },
			{ "Ze zostane sam.", "Ze znajomi przestana grac.", "Ze zawiode ekipe w waznej chwili." },
		};
		std::string out = Pick(g, kFear[g.voice], 3);
		if (g.LowHp())
			out = "Teraz to boje sie glownie tego moba obok, mam malo HP. " + out;
		return out;
	}

	inline std::string GenAnnoy(TGen& g)
	{
		static const char* const kAnnoy[V_COUNT][3] = {
			{ "Jak ktos kradnie mi moby.", "Jak exp stoi w miejscu.", "Jak bron sie spali przy ulepszaniu." },
			{ "Jak ktos sie spieszy i nie ma czasu pogadac.", "Zgubienie drogi.", "Halas w miastach." },
			{ "Ludzie, co zbijaja ceny.", "Jak ktos targuje sie o grosze.", "Pusty rynek." },
			{ "Uciekajacy przeciwnicy.", "Jak Metin znika mi sprzed nosa.", "Jak nie ma z kim sie zmierzyc." },
			{ "Jak ktos znika z PT bez slowa.", "Klotnie w grupie.", "Jak ktos jest niemily bez powodu." },
		};
		std::string out = Pick(g, kAnnoy[g.voice], 3);
		if (g.s.unlucky && g.rng.Chance(50))
			Append(out, "I pech w dropie, jak dzisiaj.");
		return out;
	}

	inline std::string GenJoy(TGen& g)
	{
		static const char* const kJoy[V_COUNT][3] = {
			{ "Nowy poziom. Zawsze.", "Jak exp leci szybko.", "Udane ulepszenie." },
			{ "Nowe miejsca i ladne widoki.", "Spokojny wieczor w podrozy.", "Jak trafie na cos, czego nie znalem." },
			{ "Dobra transakcja.", "Pelna sakiewka.", "Jak towar schodzi od reki." },
			{ "Wygrana walka.", "Rozbity Metin.", "Dobry przeciwnik." },
			{ "Dobra ekipa i rozmowa.", "Jak ktos napisze, tak jak ty teraz.", "Wspolny exp ze znajomymi." },
		};
		std::string out = Pick(g, kJoy[g.voice], 3);
		if (g.s.euphoria && g.rng.Chance(60))
			Append(out, "A dzis wyjatkowo, bo ulepszenie weszlo.");
		return out;
	}

	inline std::string GenHypo(TGen& g)
	{
		const ETopic t = g.a ? g.a->topic : T_NONE;
		if (t == T_TRAVEL || t == T_NATURE || (g.a && g.a->concepts.Has(C_WHERE)))
		{
			static const char* const kTravel[V_COUNT][3] = {
				{ "Nie wiem. Pewnie tam, gdzie mozna cos osiagnac.", "Tam, gdzie sa najlepsze spoty.", "Gdzies, gdzie szybko rosnie poziom." },
				{ "Chyba gdzies daleko od miast. Lubie spokojne miejsca.", "W gory, na sam szczyt, i posiedzial.", "Gdzies, gdzie nikt jeszcze nie byl." },
				{ "Moze do jakiegos duzego miasta. Ciekawi mnie, jak wygladaja tamtejsze rynki.", "Tam, gdzie mozna dobrze zarobic.", "Na wielki targ gdzies za morzem." },
				{ "Tam, gdzie sa najmocniejsi przeciwnicy.", "Na jakas dzika pustynie, pelna potworow.", "Tam, gdzie jest jakies wyzwanie." },
				{ "Gdziekolwiek, byle ze znajomymi.", "Nad morze z cala ekipa.", "Tam, gdzie sa fajni ludzie." },
			};
			return Pick(g, kTravel[g.voice], 3);
		}
		static const char* const kHypo[V_COUNT][3] = {
			{ "Pewnie dalej bym expil, ale szybciej.", "Wzialbym to, co daje najwiecej expa.", "Nie wiem, pewnie cos, co mnie wzmocni." },
			{ "Chyba ruszylbym w droge i zobaczyl, co z tego wyjdzie.", "Zrobilbym cos zupelnie nowego, dla samej ciekawosci.", "Pewnie bym sie rozejrzal i zdecydowal na miejscu." },
			{ "Najpierw policzylbym, czy sie oplaca.", "Zainwestowalbym to w cos pewnego.", "Kupilbym tanio, sprzedal drogo. Jak zawsze." },
			{ "Zmierzylbym sie z czyms mocnym.", "Poszedlbym na najtrudniejszy loch.", "Zaryzykowalbym. Bez ryzyka nudno." },
			{ "Zebralbym ekipe i zrobil to razem.", "Zapytalbym znajomych, co o tym mysla.", "Zrobilbym to z kims, samemu to nie to samo." },
		};
		std::string out = Pick(g, kHypo[g.voice], 3);
		if (g.rng.Chance(30))
			out = "Hmm, ciekawe pytanie. " + out;
		return out;
	}

	inline std::string GenFact(TGen& g)
	{
		static const char* const kUnknown[V_COUNT][3] = {
			{ "Nie wiem, nie znam sie na tym.", "Nie mam pojecia.", "Nie wiem. Ja sie znam glownie na expie." },
			{ "Nie wiem, nigdy sie nad tym nie zastanawialem.", "Nie mam pojecia, ale brzmi ciekawie.", "Hmm, nie wiem. Ciekawe pytanie." },
			{ "Nie wiem. Ale jak da sie na tym zarobic, daj znac.", "Nie mam pojecia, to nie moja dzialka.", "Nie wiem, szczerze." },
			{ "Nie wiem, nie zaprzatam sobie tym glowy.", "Nie mam pojecia.", "Nie wiem. Zapytaj kogos madrzejszego." },
			{ "Nie wiem, ale moze ktos z ekipy bedzie wiedzial.", "Nie mam pojecia. A ty wiesz?", "Hmm, nie wiem. Powiesz mi?" },
		};
		return Pick(g, kUnknown[g.voice], 3);
	}

	inline std::string GenLikeAnswer(TGen& g, const TTopicPack* pack)
	{
		const std::string what = g.a ? g.a->object : std::string();
		const int level = LikeLevel(g, pack, what);
		static const char* const kLike[] = { "Lubie, czemu nie.", "Tak, calkiem lubie.", "Pewnie, ze tak.", "Lubie, choc bez przesady." };
		static const char* const kMeh[] = { "Tak sobie. Ani mnie to grzeje, ani ziebi.", "Bywa roznie. Nie mam zdania.", "Czasem tak, czasem nie." };
		static const char* const kDislike[] = { "Niezbyt, szczerze mowiac.", "Raczej nie, to nie dla mnie.", "Nie bardzo." };
		std::string body;
		if (level == 2)
			body = pack && pack->like[0] && *pack->like[0] && g.rng.Chance(50) ? PickField(g, pack->like, 2) : PBC_SAY(g, kLike);
		else if (level == 0)
			body = pack && pack->dislike[0] && *pack->dislike[0] && g.rng.Chance(50) ? PickField(g, pack->dislike, 2) : PBC_SAY(g, kDislike);
		else
			body = PBC_SAY(g, kMeh);
		if (g.a && g.a->concepts.Has(C_COLD) && level == 0)
			body = g.rng.Chance(50) ? "Raczej wole cieplejsza pogode." : "Niezbyt, wole jak jest cieplej.";
		g.reason = pack ? PickField(g, pack->why, 2) : std::string();
		return WithEcho(g, body, 55);
	}

	inline std::string GenChoice(TGen& g)
	{
		if (!g.a || g.a->object.empty() || g.a->objectB.empty())
			return GenFact(g);
		const u32 h = HashStr((g.a->object + "|" + g.a->objectB).c_str(), g.m.botPID * 97u + 13u);
		const std::string& pick = (h & 1) ? g.a->objectB : g.a->object;
		static const char* const kPick[] = {
			"Chyba $X.", "Zdecydowanie $X.", "Hmm... $X.", "$X, bez dwoch zdan.", "Raczej $X, ale to trudny wybor."
		};
		std::string out = Pick(g, kPick, 5);
		ReplaceAll(out, "$X", pick);
		CapitalizeFirst(out);
		g.reason = "Po prostu bardziej mi pasuje.";
		return out;
	}

	inline std::string GenWeatherStatement(TGen& g)
	{
		const TConceptSet& c = g.a->concepts;
		if (c.Has(C_COLD))
		{
			static const char* const k[] = {
				"Tez mam takie wrazenie. Jakos ponuro dzisiaj.", "No, zimno. Az sie nie chce wychodzic z miasta.",
				"Brr, prawda. Ja bym juz siedzial przy ognisku.", "Tez czuje. Dobry dzien na cieply kocyk." };
			return PBC_SAY(g, k);
		}
		if (c.Has(C_WARM))
		{
			static const char* const k[] = {
				"No, cieplo. Az chce sie gdzies pochodzic.", "Prawda, ladnie dzisiaj.",
				"Oby tak zostalo. Lubie jak jest slonecznie.", "Byle nie za goraco, bo w zbroi ciezko." };
			return PBC_SAY(g, k);
		}
		if (c.Has(C_RAIN))
		{
			static const char* const k[] = {
				"No, leje. Dobry dzien, zeby posiedziec w grze.", "Deszcz to idealna pogoda na granie.",
				"Oj, to nie wychodz nigdzie, lepiej pograjmy.", "Szaro i mokro. Klasyka." };
			return PBC_SAY(g, k);
		}
		const TTopicPack* pack = FindTopicPack(T_WEATHER);
		return PickField(g, pack->react, 4);
	}

	// Ask back once in a while - the social voice more, the grinder less, a
	// bad mood never.
	inline void MaybeAskBack(TGen& g, const TTopicPack* pack, u32 chance)
	{
		if (!pack || g.Bad() || !g.askBack.empty())
			return;
		if (g.voice == V_SOCIAL) chance += 20;
		if (g.voice == V_WANDERER) chance += 10;
		if (g.voice == V_GRINDER) chance = chance > 15 ? chance - 15 : 0;
		if (g.tier == TIER_HOSTILE) return;
		if (!g.rng.Chance(chance))
			return;
		const std::string q = PickField(g, pack->ask, 2);
		if (q.empty())
			return;
		g.askBack = q;
		g.askBackKind = ASK_TOPIC;
		g.askBackTopic = (ETopic)pack->topic;
	}

	// The whole GENERAL_CONVERSATION answer.
	inline std::string GenGeneral(TGen& g)
	{
		const TAnalysis& a = *g.a;
		const TTopicPack* pack = FindTopicPack(a.topic);
		std::string out;

		switch (a.qtype)
		{
			case Q_DREAM: out = GenDream(g); MaybeAskBack(g, FindTopicPack(T_DREAMS), 40); return out;
			case Q_FEAR: out = GenFear(g); MaybeAskBack(g, FindTopicPack(T_FEAR), 35); return out;
			case Q_ANNOY: out = GenAnnoy(g); MaybeAskBack(g, FindTopicPack(T_ANNOY), 35); return out;
			case Q_JOY: out = GenJoy(g); MaybeAskBack(g, FindTopicPack(T_JOY), 35); return out;
			case Q_HYPO: out = GenHypo(g); MaybeAskBack(g, pack, 30); return out;
			case Q_CHOICE: out = GenChoice(g); MaybeAskBack(g, pack, 30); return out;
			default: break;
		}
		if (a.topic == T_DREAMS) { out = GenDream(g); MaybeAskBack(g, pack, 40); return out; }
		if (a.topic == T_FEAR) { out = GenFear(g); MaybeAskBack(g, pack, 35); return out; }
		if (a.topic == T_ANNOY) { out = GenAnnoy(g); MaybeAskBack(g, pack, 35); return out; }
		if (a.topic == T_JOY && a.qtype != Q_STATEMENT) { out = GenJoy(g); MaybeAskBack(g, pack, 35); return out; }

		// Topics where the game state says something true about the bot.
		if (a.topic == T_TIRED && (a.qtype != Q_STATEMENT || a.concepts.Has(C_YOU)))
		{
			if (g.LowHp())
				out = "Troche, i jeszcze HP mi siada. Zaraz odpoczne.";
			else if (g.s.onlineMinutes > 180)
				out = g.rng.Chance(50) ? "Troche tak, siedze tu juz dobrych kilka godzin." : "No troche, dlugo juz dzis gram.";
			else if (g.Bad())
				out = "Troche. Jakos ciezki dzien.";
			else
				out = PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_BORED && (a.qtype != Q_STATEMENT || a.concepts.Has(C_YOU)))
		{
			if (g.s.shopStanding)
				out = "Troche. Stanie przy straganie to nie jest najciekawsze zajecie.";
			else if (g.s.fishing)
				out = "Przy wedce? Troche, ale to taki przyjemny rodzaj nudy.";
			else if (g.s.action == A_FIGHT)
				out = "Nie, akurat sie cos dzieje, walcze.";
			else
				out = PickField(g, pack->view + g.voice, 1);
			MaybeAskBack(g, pack, 30);
			return out;
		}
		if (a.topic == T_LONELY && a.qtype != Q_STATEMENT)
		{
			out = g.s.inParty ? "Nie, teraz akurat jestem z ekipa." : PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_DAYTIME && a.qtype != Q_LIKE && a.qtype != Q_FAVORITE)
		{
			if (g.s.hour >= 23 || g.s.hour < 5)
				out = g.rng.Chance(50) ? "No, pozno juz. A ja dalej gram." : "Noc juz, ale jakos nie chce mi sie konczyc.";
			else if (g.s.hour < 10)
				out = "Wczesnie jeszcze. Dobry moment, zeby spokojnie poexpic.";
			else if (g.s.hour >= 18)
				out = "Wieczor to najlepsza pora na granie.";
			else
				out = PickField(g, pack->react, 4);
			return out;
		}
		if (a.topic == T_LUCK && a.qtype != Q_STATEMENT)
		{
			if (g.s.unlucky)
				out = "Dzis raczej pech. Dawno nic dobrego nie wypadlo.";
			else if (g.s.euphoria || g.Good())
				out = "Dzis akurat mam farta!";
			else
				out = PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_FRIENDSHIP && a.concepts.Has(C_YOU) && a.qtype != Q_STATEMENT)
		{
			out = g.tier >= TIER_FRIEND ? "Mam. Ty tez sie do nich zaliczasz." : PickField(g, pack->view + g.voice, 1);
			return out;
		}

		if (!pack)
		{
			// A preference question about something with no topic of its own:
			// "lubisz kaktusy?", "co myslisz o polityce?".
			if (a.qtype == Q_LIKE || a.qtype == Q_WANT)
				return GenLikeAnswer(g, NULL);
			if (a.qtype == Q_DISLIKE)
			{
				static const char* const k[] = { "Nie lubie, jak ktos kradnie moby.", "Nie znosze czekania.", "Nie lubie pospiechu." };
				return PBC_SAY(g, k);
			}
			if (a.qtype == Q_OPINION)
			{
				static const char* const k[] = {
					"Nie mam wyrobionego zdania, ale brzmi ciekawie.", "Szczerze? Nie zastanawialem sie nad tym.",
					"Ciezko powiedziec. Kazdy ma swoje zdanie." };
				return WithEcho(g, PBC_SAY(g, k), 50);
			}
			if (a.qtype == Q_CAN)
			{
				const int r = OpinionRoll(g, a.object, 5);
				static const char* const kYes[] = { "Troche umiem, ale bez szalu.", "Cos tam umiem." };
				static const char* const kNo[] = { "Nie, raczej nie umiem.", "Chyba nie. Nigdy nie probowalem." };
				return WithEcho(g, r < 40 ? PBC_SAY(g, kYes) : PBC_SAY(g, kNo), 50);
			}
			if (a.qtype == Q_FAVORITE)
			{
				static const char* const k[] = { "Nie mam jednego ulubionego.", "Ciezko wybrac jedno.", "Chyba nie mam ulubionego." };
				return PBC_SAY(g, k);
			}
			if (a.qtype == Q_WHAT_LIKE)
				return PickField(g, FindTopicPack(T_HOBBY)->view + g.voice, 1);
			return GenFact(g);
		}

		switch (a.qtype)
		{
			case Q_STATEMENT:
				if (a.topic == T_WEATHER || a.topic == T_SEASON)
					out = a.topic == T_WEATHER ? GenWeatherStatement(g) : PickField(g, pack->react, 4);
				else if (a.topic == T_FEELINGS && a.concepts.Has(C_SAD))
				{
					static const char* const k[] = { "Oj, przykro mi. Chcesz pogadac?", "Trzymaj sie. Bedzie lepiej.", "Kiepsko... Moze troche gry poprawi humor?" };
					out = PBC_SAY(g, k);
				}
				else if (a.topic == T_FEELINGS && a.concepts.Has(C_HAPPY))
				{
					static const char* const k[] = { "To super! Ciesze sie.", "Oby tak dalej!", "Fajnie to slyszec." };
					out = PBC_SAY(g, k);
				}
				else
					out = PickField(g, pack->react, 4);
				MaybeAskBack(g, pack, 30);
				return out;
			case Q_LIKE:
			case Q_WANT:
				out = GenLikeAnswer(g, pack);
				MaybeAskBack(g, pack, 30);
				return out;
			case Q_DISLIKE:
			{
				const std::string d = PickField(g, pack->dislike, 2);
				out = d.empty() ? GenAnnoy(g) : d;
				return out;
			}
			case Q_FAVORITE:
			{
				const std::string fav = PickFavorite(g, pack);
				if (fav.empty())
					out = PickField(g, pack->view + g.voice, 1);
				else
				{
					static const char* const k[] = { "Chyba $X.", "$X, zdecydowanie.", "Hmm... $X.", "Lubie $X." };
					out = Pick(g, k, 4);
					ReplaceAll(out, "$X", fav);
					CapitalizeFirst(out);
				}
				MaybeAskBack(g, pack, 35);
				return out;
			}
			case Q_WHAT_LIKE:
			case Q_OPINION:
			case Q_MIRROR:
			case Q_OPEN:
				out = PickField(g, pack->view + g.voice, 1);
				if (out.empty())
					out = PickField(g, pack->react, 4);
				if (a.qtype == Q_MIRROR && g.rng.Chance(40))
					out = "Ja? " + out;
				g.reason = PickField(g, pack->why, 2);
				MaybeAskBack(g, pack, a.qtype == Q_MIRROR ? 10 : 30);
				return out;
			case Q_CAN:
			{
				const int r = OpinionRoll(g, a.object, 5);
				out = r < 40 ? "Troche umiem, ale bez szalu." : "Nie, raczej nie. Nigdy nie mialem do tego glowy.";
				return WithEcho(g, out, 50);
			}
			case Q_EVER:
			{
				static const char* const k[] = { "Kiedys moze, ale nie pamietam juz dokladnie.", "Chyba nie. Ale chcialbym.", "Hmm, nie przypominam sobie." };
				out = PBC_SAY(g, k);
				MaybeAskBack(g, pack, 40);
				return out;
			}
			case Q_KNOW:
			{
				static const char* const k[] = { "Znam cos tam, ale nie pamietam nazw.", "Kilka by sie znalazlo, ale z glowy nie powiem.", "Nie za bardzo sie znam, szczerze." };
				out = PBC_SAY(g, k);
				MaybeAskBack(g, pack, 40);
				return out;
			}
			case Q_FACT:
				return GenFact(g);
			default:
				return PickField(g, pack->react, 4);
		}
	}
}

#endif
