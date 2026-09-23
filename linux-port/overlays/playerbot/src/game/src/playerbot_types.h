#ifndef __INC_METIN2_PLAYERBOT_TYPES_H__
#define __INC_METIN2_PLAYERBOT_TYPES_H__

// Tuning constants, enums and the per-bot state that the rest of the playerbot
// code is written against.
//
// This is an implementation fragment, not a normal header: it defines objects,
// it relies on the engine headers playerbot_manager.cpp includes above it, and
// its anonymous namespace is deliberately the same one the manager reopens --
// in a single translation unit those merge. Include it exactly once, from
// playerbot_manager.cpp, and nowhere else.

namespace
{
	const int PLAYERBOT_SEARCH_RANGE = 6000;
	const size_t PLAYERBOT_TARGET_CHOICE_WINDOW = 16;
	// Ordinary grinders chain into a nearby free pack before considering a
	// distant high-score target. Claims still spread a crowd over different mobs.
	const int PLAYERBOT_LOCAL_CHAIN_RANGE = 2500;
	// How far below itself a bot still counts a monster as prey. The engine's own
	// experience table settles the number: aiPercentByDeltaLev pays 70% at nine
	// levels down, 50% at ten and one per cent from fifteen. So nine is the last
	// step before the reward halves, and a level-19 bot swinging at one of Joan's
	// level-1 stray dogs earns a hundredth of a kill - which is the "boty bija
	// psy" the Discord keeps reporting. Anything lower is passed over while
	// something worth the swing is in reach, and taken when nothing else is.
	const int PLAYERBOT_TRIVIAL_LEVEL_GAP = 9;
	const int PLAYERBOT_MELEE_RANGE = 250;
	const int PLAYERBOT_MELEE_SPLASH_RANGE = 300;
	const size_t PLAYERBOT_MAX_MELEE_TARGETS = 4;
	// A swing is a swing in front of the character, and the client is where
	// that is decided for a player: CActorInstance::__NormalAttackProcess
	// refuses a victim further than 300 units (this range, to the unit) and
	// then tests the weapon's own collision spheres against it, which sweep an
	// arc ahead of the body - so a player kills the monsters it is facing and
	// never the one behind its back. A bot has no model to collide with
	// anything, so the arc is a dot product instead: half an angle of sixty
	// degrees either side of the blow. Without it a bot standing still cut down
	// whatever stood round it, which is what "kazdy bot gra jakby mial hacka"
	// was (Nagash, 20 September).
	const float PLAYERBOT_MELEE_SPLASH_FACING_DOT = 0.5f;
	const int PLAYERBOT_MAX_TARGET_LEVEL_DELTA = 15;
	const int PLAYERBOT_LOOT_SEARCH_RANGE = 2500;
	const int PLAYERBOT_PICKUP_RANGE = 300;
	// Keep a fresh drop visible for a human-readable moment and pick individual
	// stacks at a believable cadence instead of clearing the floor in one tick.
	const DWORD PLAYERBOT_LOOT_VISIBLE_DELAY_MIN = 1000;
	const DWORD PLAYERBOT_LOOT_VISIBLE_DELAY_MAX = 1800;
	const DWORD PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN = 450;
	const DWORD PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX = 850;
	// Yang is taken almost at once. The pause above exists so a bot does not
	// hoover a field the instant it drops, but a coin pile is one click a player
	// never hesitates over, and three of them in a row had bots standing in a
	// cleared field for six seconds instead of finding the next pack.
	const DWORD PLAYERBOT_LOOT_MONEY_DELAY_MIN = 150;
	const DWORD PLAYERBOT_LOOT_MONEY_DELAY_MAX = 350;
	const DWORD PLAYERBOT_LOOT_MONEY_INTERVAL_MIN = 150;
	const DWORD PLAYERBOT_LOOT_MONEY_INTERVAL_MAX = 300;
	// A combat pickup is a cheap-looking action but an expensive query: Metin2's
	// ForEachAround snapshots every entity in nine neighbouring sectrees before
	// the callback can apply the 3 m pickup radius.  Throttle empty scans as well
	// as successful pickups, otherwise hundreds of fighting bots repeat the same
	// work several thousand times per second.
	const DWORD PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MIN = 750;
	const DWORD PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MAX = 1000;
	const DWORD PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MIN = 750;
	const DWORD PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MAX = 1000;
	const DWORD PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MIN = 900;
	const DWORD PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MAX = 1300;
	const DWORD PLAYERBOT_LOOT_CLEANUP_INTERVAL = 10000;
	// Who leaves merchant fodder on the ground, and what fodder is worth - see
	// IsPlayerBotLootBeneathBot. Yang is long long on the 2.x line, so the
	// purse bound is too.
	const int PLAYERBOT_LOOT_CHOOSY_MIN_LEVEL = 40;
	const long long PLAYERBOT_LOOT_CHOOSY_MIN_GOLD = 500000LL;
	const long long PLAYERBOT_LOOT_CHOOSY_MAX_VALUE = 40000LL;
	const int PLAYERBOT_LOOT_OUTGROWN_GEAR_LEVELS = 10;
	// What is picked up and kept whatever the merchant pays for it, because a
	// player crafts or refines it further (IsPlayerBotPickupGoods): "Korzenie
	// Gango, Grzyby Tue, Krysztalowe Kolczyki, Zbroje Twarzy Ducha ... warto
	// podnosic, aby dalej przerabiac", the level-65 weapons, Fasolki Zen and
	// Pigulki Krwi with them (Tieru, 15 September). The herbs are the
	// herbalist's 50724 and 50726; the Biologist's 50704 and 50706 are quest
	// items and were never left behind.
	// The mission books and the horse's hay and carrots with them: a player uses
	// both and no bot does, and the merchant was paying five hundred yang for a
	// book ("Boty sprzedaja Ksiegi misji/marchewki/siano do handlarza. Lepiej
	// jakby wystawialy w sklepach", Greess, 18 September).
	const DWORD PLAYERBOT_PICKUP_GOODS_VNUMS[] = {
		70014,    // Pigulka Krwi
		70102,    // Fasolka Zen
		50054,    // Siano
		50055,    // Marchewka
		50307,    // Ksiega Misji (Latwa)
		50308,    // Ksiega Misji (Normalna)
		50309,    // Ksiega Misji (Trudna)
		50310,    // Ksiega Misji (ekspert)
	};
	// The herbalist's sixteen herbs, 50721-50736: every one of them is a
	// material of some row on Baek-Go's board (playerbot_herbalism.h), so all
	// of them are worth bending down for. This used to name two - the Gango
	// Root and the Tue Mushroom - and the measurement on 17 September is what
	// two costs: 86 496 roots and 14 515 mushrooms in the bots' bags against
	// ELEVEN Peach Blossoms in the whole world, which is the one herb the
	// onboarding quest asks ten of. The bots were not short of herbs; they were
	// short of the herbs nothing had told them to pick up.
	const DWORD PLAYERBOT_HERB_VNUM_FIRST = 50721;
	const DWORD PLAYERBOT_HERB_VNUM_LAST = 50736;
	const DWORD PLAYERBOT_PICKUP_EARRING_FIRST = 17160;    // Krysztalowe Kolczyki+0..+9
	const DWORD PLAYERBOT_PICKUP_ARMOUR_FIRST = 11670;     // Zbroja Twarzy Ducha+0..+9
	const int PLAYERBOT_PICKUP_WEAPON_LEVEL = 65;
	const DWORD PLAYERBOT_INVENTORY_MAINTENANCE_MIN = 30000;
	const DWORD PLAYERBOT_INVENTORY_MAINTENANCE_MAX = 60000;
	const int PLAYERBOT_POTION_HP_PERCENT = 65;
	// Below this many the belt is worth a trip, wherever the bot is.
	//
	// It was 150 red and 100 blue, on the argument that a bot with half its
	// potions has no business leaving a good spot - true, and also the reason
	// the population looked like this, measured over every bot of forty and up:
	// red potions at a MEDIAN of 32, 502 of 812 under the trigger, 473 under
	// fifty, against 254 holding the six hundred they set out with. A bot on
	// the frontier is a portal and a map from the merchant, and the travel
	// pass yields to fights on the way; a trigger of 150 fired with the belt
	// already at seven by the time it arrived - "potki 7/0" over a level 47
	// walking to the weapon merchant, photographed. Twice the distance, then,
	// so the walk starts while there is still something to fight with.
	const size_t PLAYERBOT_POTION_TRIP_RED = 300;
	const size_t PLAYERBOT_POTION_TRIP_BLUE = 200;
	// And the big potions from here on. See ManagePlayerBotMiscMerchant.
	const BYTE PLAYERBOT_BIG_POTION_MIN_LEVEL = 40;
	const int PLAYERBOT_POTION_SP_PERCENT = 30;
	const int PLAYERBOT_RECOVERY_HP_PERCENT = 75;
	const int PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT = 20;
	const int PLAYERBOT_RECOVERY_REST_HEAL_PERCENT = 5;
	const int PLAYERBOT_RETREAT_START_HP_PERCENT = 35;
	// Finishing a stone that is nearly broken, instead of walking away from it.
	//
	// Reported from the Discord by two people: "bots often fight a Metin and
	// leave at the end when their health goes". They do, and this is why - the
	// emergency recovery above drops the target at
	// PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT whatever the target is. For a
	// monster that is right: it chases, and a bot that stands there dies. A
	// Metin stone is the opposite case - it is CHAR_TYPE_STONE and not
	// CHAR_TYPE_MONSTER, it does not follow anybody, and leaving one at a
	// sliver of health throws away the whole fight, because the bot comes back
	// to a stone at full health or finds it gone.
	//
	// So a stone within sight of breaking is finished, and only while the bot is
	// still clearly above the floor where dying becomes the likely outcome.
	// Dying costs experience; this is not licence to stand in the shockwave.
	const int PLAYERBOT_STONE_FINISH_STONE_HP_PERCENT = 15;
	const int PLAYERBOT_STONE_FINISH_OWN_HP_PERCENT = 10;
	const int PLAYERBOT_RETREAT_END_HP_PERCENT = 65;
	// A retreat ends on its own, whatever the monster still thinks. Ending it
	// needed the threat to drop its aggro, and a monster that cannot reach the
	// bot keeps GetVictim() pointing at it for ever - so a bot at full health
	// ran the eight escape directions at a canyon wall for five hours, moving
	// the whole time, which is also why the inactivity watchdog never saw it
	// (MORDEGAPOTEGA on the desert, Urtopy, 17 September).
	const DWORD PLAYERBOT_RETREAT_MAX_MS = 60 * 1000;
	// And distance is what a retreat is for: once this far from the threat the
	// bot has escaped, whether or not the monster has noticed.
	const int PLAYERBOT_RETREAT_SAFE_DISTANCE = 3000;
	const DWORD PLAYERBOT_RETREAT_MOVE_INTERVAL = 1800;
	const DWORD PLAYERBOT_ATTACK_INTERVAL = 1200;
	const DWORD PLAYERBOT_POTION_INTERVAL = 1000;
	// How long a bot lies where it fell. A player sees a body on the ground and
	// then sees it get up; eleven seconds was close to that already and ten is
	// what it is meant to be.
	const DWORD PLAYERBOT_REVIVE_DELAY = 10000;
	const DWORD PLAYERBOT_GEAR_RETRY_INTERVAL = 1000;
	const DWORD PLAYERBOT_EQUIPMENT_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_EQUIPMENT_COMBAT_DELAY = 1700;
	const DWORD PLAYERBOT_GEAR_LOG_INTERVAL = 10000;
	const DWORD PLAYERBOT_WOODEN_ARROW_VNUM = 8000;
	const int PLAYERBOT_ARROW_RESTOCK_THRESHOLD = 100;
	const int PLAYERBOT_ARROW_SMALL_BUNDLE = 100;
	const int PLAYERBOT_ARROW_LARGE_BUNDLE = 200;
	// What a dropper's archer fills its quiver to at the weapon merchant, a
	// bundle at a time (WantsPlayerBotArrowTopUp). A Monkey Dungeon visit is half
	// an hour of shooting and a bundle of two hundred lasted minutes: 18 of the
	// 22 medal droppers that left a dungeon for supplies were archers, each with
	// more than five hundred red potions still in the bag.
	const int PLAYERBOT_DROPPER_ARROW_STOCK = 1000;
	const DWORD PLAYERBOT_POTION_LOG_INTERVAL = 10000;
	// The engine already saves every character on save_event_second_cycle,
	// which config.cpp sets to 120 s, and a level change forces a save below
	// regardless of this timer. At 30 s the bots were adding four extra saves
	// per engine save each - close to thirty a second across the population - for nothing
	// the engine's own cycle does not already cover.
	const DWORD PLAYERBOT_PERSIST_INTERVAL = 120000;

	// What the population cost this minute, counted where it happens and
	// reported once from Update. These are the things that do not log per
	// event and are therefore invisible when the core is hot: A* searches, whole
	// map snapshots for the material errand, and character saves.
	DWORD s_uPlayerBotLoadPlans = 0;
	DWORD s_uPlayerBotLoadScans = 0;
	DWORD s_uPlayerBotLoadSaves = 0;
	DWORD s_uPlayerBotLoadWatchdog = 0;
	// Passes of the manager's tick cut short by its time budget in the minute
	// (PLAYERBOT_TICK_BUDGET_MS_DEFAULT, the TICK_MS key of the weights file).
	DWORD s_uPlayerBotLoadSliced = 0;
	// And what the light ticks of the bots those passes did not reach cost
	// in the minute (RunPlayerBotLightTick), in microseconds.
	DWORD s_uPlayerBotLoadLightUs = 0;
	DWORD s_dwPlayerBotLoadReportTime = 0;
	const DWORD PLAYERBOT_LOAD_REPORT_INTERVAL = 60000;
	// How long one pass of CPlayerBotManager::Update may run before it stops
	// and leaves the rest of the bots to the next pass, a quarter of a second
	// later, which starts where it stopped. The pass is the one thing on a
	// core's main thread that grows with the bot count, and nothing bounded it:
	// on a five-euro VPS 1140 bots took up to a second a pass, four passes a
	// second, and a player's login went unanswered - "przez 4 sekundy rdzen nie
	// przetwarza nawet pakietu handshake" (SIZOWSKI, 18 September). A pass on
	// this project's own machine takes about fifty milliseconds with 1100 bots
	// and at most a hundred and thirty, so the budget costs nothing there.
	// Zero is no budget. The operator's knob is TICK_MS in the weights file.
	const int PLAYERBOT_TICK_BUDGET_MS_DEFAULT = 120;
	// A pass always serves at least this many bots, so the world's own work
	// ahead of the loop can never eat the whole budget and stop every bot.
	const unsigned int PLAYERBOT_TICK_MIN_BOTS = 50;
	// And how long they took. A count says how often; only the clock says
	// whether it matters. Microseconds from the monotonic clock, wrapping in a
	// DWORD every 71 minutes - which the unsigned subtraction below survives.
	DWORD s_uPlayerBotLoadPlanUs = 0;
	DWORD s_uPlayerBotLoadScanUs = 0;
	DWORD s_uPlayerBotLoadTickUs = 0;
	DWORD s_uPlayerBotLoadTickMaxUs = 0;
	DWORD s_uPlayerBotLoadTicks = 0;
	// The two passes inside the tick that sweep the nine sectrees around a bot
	// - looking for something to hit, and writing the panel snapshot - are
	// counted apart, because a bot with nothing to hit repeats the sweep every
	// tick and there is no event to see it by.
	DWORD s_uPlayerBotLoadTargetSearches = 0;
	DWORD s_uPlayerBotLoadTargetMisses = 0;
	DWORD s_uPlayerBotLoadTargetUs = 0;
	DWORD s_uPlayerBotLoadSnapshotUs = 0;
	// Plans by how far they reach, in grid cells: under 64, under 256, under
	// 1024, and beyond. A short hop to the next monster and a crossing of the
	// whole valley are both "a plan", and only the split says which one is
	// paying for the other.
	DWORD s_uPlayerBotLoadPlanBucket[4] = { 0, 0, 0, 0 };
	DWORD s_uPlayerBotLoadPlanBucketUs[4] = { 0, 0, 0, 0 };
	// Plans a tick turned away because it had already spent its planning time.
	// Deferred is not lost: the bot asks again within two seconds.
	DWORD s_uPlayerBotLoadPlanDeferred = 0;
	DWORD s_uPlayerBotLoadPlanResumed = 0;
	DWORD s_uPlayerBotLoadPlanCached = 0;

	inline DWORD PlayerBotClockUs()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (DWORD)((unsigned long long)ts.tv_sec * 1000000ULL + (unsigned long long)ts.tv_nsec / 1000ULL);
	}

	// Adds the scope's duration to a counter on the way out, whichever of the
	// function's returns is taken.
	struct TPlayerBotLoadTimer
	{
		DWORD& m_acc;
		DWORD m_start;
		explicit TPlayerBotLoadTimer(DWORD& acc) : m_acc(acc), m_start(PlayerBotClockUs()) {}
		~TPlayerBotLoadTimer() { m_acc += PlayerBotClockUs() - m_start; }
	};
	const DWORD PLAYERBOT_RECOVERY_PROTECTION_INTERVAL = 3000;
	const DWORD PLAYERBOT_RECOVERY_REST_HEAL_INTERVAL = 1000;
	const DWORD PLAYERBOT_BUFF_FALLBACK_DURATION = 60000;
	const DWORD PLAYERBOT_SHAMAN_ATTACK_SKILL_INTERVAL = 6000;
	const DWORD PLAYERBOT_STAT_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_SKILL_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_SKILL_BOOK_CHECK_INTERVAL = 8000;
	// What LearnSkillByBook wants in hand before it reads, class book or
	// Leadership or Combo, on every level under the cap (FN_should_check_exp).
	// Short of it the engine answers "lack of experience", keeps the book and
	// the use still returns true - so a bot tried again every
	// PLAYERBOT_SKILL_BOOK_CHECK_INTERVAL and logged a read: on m2zip on
	// 18 September 7 095 "read skill book" lines in twelve minutes against 23
	// reads the engine actually rolled, 78 of the 91 readers below the mark
	// (droppers whose experience is locked at 25 and 33, bots of forty in a
	// second village where they may not hunt). A class read also costs this
	// much experience, whatever it rolls.
	const int PLAYERBOT_BOOK_READ_EXP = 20000;
	// The two skills every class trains from a book that is not an
	// ITEM_SKILLBOOK: Sztuka Wojny Sun Zi / Wu Zi / WeiLiao Zi (50301-50303,
	// Leadership by twenty levels each) and Sztuka Combo (50304-50306, Combo
	// at 20/70/100 percent a read, from level 30 and 50). skill_length.h on
	// mt2009, skill.h on r40250 - the numbers are the same on both.
	const DWORD PLAYERBOT_SKILL_LEADERSHIP_VNUM = 121;
	const DWORD PLAYERBOT_SKILL_COMBO_VNUM = 122;
	// How many of a book the bot can still read it keeps; the rest are goods.
	const int PLAYERBOT_GENERAL_BOOK_KEEP = 3;
	// A Combo book is kept this many levels before the level that reads it.
	const int PLAYERBOT_GENERAL_BOOK_LEVEL_AHEAD = 5;
	// Iwakura's ordinary book, times this, is what a counter asks for them.
	const DWORD PLAYERBOT_GENERAL_BOOK_PRICE_MULT_LEADERSHIP = 2;
	const DWORD PLAYERBOT_GENERAL_BOOK_PRICE_MULT_COMBO = 4;
	// Kamien Duchowy, the Grand Master's book (ManagePlayerBotGrandMasterTraining):
	// how often a bot holding one looks, and the twelve hours the quest puts
	// between two reads. Fasolka Zen lifts a rank below zero, and a bot keeps
	// a few of them off its counter for that.
	const DWORD PLAYERBOT_GRAND_MASTER_STONE_VNUM = 50513;
	const DWORD PLAYERBOT_GRAND_MASTER_CHECK_INTERVAL = 30000;
	const int PLAYERBOT_GRAND_MASTER_TRAIN_SECONDS = 12 * 3600;
	const DWORD PLAYERBOT_ZEN_BEAN_VNUM = 70102;
	// What a bot keeps of its beans: ten to fifteen, drawn by pid, "pod
	// robienie skilli na P" - the Grand Master's reads cost rank, and a bean
	// is the one way back from below zero (Iwakura, Community Patch 2). The
	// rest is counter goods in lines of PLAYERBOT_ZEN_BEAN_LINE_UNITS. The
	// keep was two, counted only over the cells in front of a stack, so a
	// bot's one stack was never goods: 18 000 beans in bags on blasty's world
	// and not one on a counter (21 September).
	const int PLAYERBOT_ZEN_BEAN_KEEP_MIN = 10;
	const int PLAYERBOT_ZEN_BEAN_KEEP_MAX = 15;
	const int PLAYERBOT_ZEN_BEAN_LINE_UNITS = 5;
	const DWORD PLAYERBOT_ZEN_BEAN_CHECK_INTERVAL = 10000;
	// A bot with a negative rank waits inside its village's safe ring
	// (KeepPlayerBotNegativeRankInTown): the rest mark that keeps the inactivity
	// watchdog off a bot standing still on purpose is renewed for this long, and
	// the walk to the market pitch counts as arrived this close to it.
	const DWORD PLAYERBOT_NEGATIVE_RANK_HOLD_MS = 30000;
	const int PLAYERBOT_NEGATIVE_RANK_PITCH_ARRIVAL = 600;
	// How many books of one of its own skills a bot keeps. Ten successful
	// reads take a skill from M1 to G1 and a read succeeds two times in three,
	// so this is one skill's worth with a spare; the rest go on a counter or
	// to the merchant. Before this a bot kept every book for a skill it could
	// not read for weeks, and the bag filled with them.
	const int PLAYERBOT_BOOK_KEEP_PER_SKILL = 12;
	// And for a skill it cannot read yet - not at Master - only a few against
	// the day it gets there. Twelve of every own skill, readable or not, was
	// 2636 books in 929 bags on a world where the bots at forty still had
	// their skills in the teens: the books sat for weeks and the counters
	// carried four of them in two hours. A skill already at Grand Master
	// keeps none; a book cannot take it further.
	const int PLAYERBOT_BOOK_KEEP_UNREADABLE = 3;
	// The Metin dropper keeps every book for its counter - and a dropper whose
	// stall roll never came kept them for good: a bag of eighty books, the
	// loot pass with nowhere to put a drop, and the bot farming stones it
	// could not pick up ("chlop biega z calym eq i dalej farmi metiny nie
	// podnoszac nic"). Under bag pressure it opens a stall whatever the roll,
	// and what is still beyond this many books goes to the merchant.
	const int PLAYERBOT_DROPPER_BOOK_KEEP = 20;
	// A bag this short of cells is under pressure: what was worth keeping on
	// the chance of a key or a buyer goes to the merchant, so the chests and
	// the loot still have somewhere to land.
	const int PLAYERBOT_BAG_PRESSURE_FREE_CELLS = 8;
	// A bag this full is an errand, not a state to hunt in. Above this share
	// of the ninety cells the bot goes and does something about it - the
	// merchant, its own counter, the storekeeper, the blacksmith - and no
	// expedition whose point is a drop starts: a Master of Equipment ran the
	// Monkey Dungeon for two hours after a medal that had no cell to land in
	// ("eq pelne od dawna a on se napierdala 2 godziny malpy").
	const int PLAYERBOT_BAG_FULL_PERCENT = 80;
	// The storekeeper (Dozorca, 9005): npc.txt cell (609,596) on map 21, base
	// (0,102400); cell (471,347) on map 23, base (102400,204800). A bot's
	// safebox is one page of forty-five cells behind the default password -
	// the DB accepts "000000" for an account that never set one - and it is
	// where the skill books go that the bag cannot hold and the counter has
	// not sold: a book never goes to the merchant. PLAYERBOT_SAFEBOX_BOOK_KEEP
	// surplus books stay in the bag as goods for the counter; the rest are
	// deposited once the bag is under pressure.
	const long PLAYERBOT_STOREKEEPER_X = 60900;
	const long PLAYERBOT_STOREKEEPER_Y = 162000;
	const long PLAYERBOT_M2_STOREKEEPER_X = 149500;
	const long PLAYERBOT_M2_STOREKEEPER_Y = 239500;
	const char* const PLAYERBOT_SAFEBOX_PASSWORD = "000000";
	// What the storekeeper's quest charges once for the first page
	// (warehouse.quest: 500 yang, then set_safebox_level(1)), remembered in
	// the bot's own flag because the quest's state is not ours to set.
	const int PLAYERBOT_SAFEBOX_FEE = 500;
	const char* const PLAYERBOT_SAFEBOX_PAID_FLAG = "playerbot.safebox_paid";
	const DWORD PLAYERBOT_SAFEBOX_LOAD_WAIT_MS = 8000;
	const int PLAYERBOT_SAFEBOX_BOOK_KEEP = 12;
	// A page COUNT, not a cell count - and that distinction was a live bug.
	// CHARACTER::SetSafeboxSize refuses anything at or above SAFEBOX_PAGE_COUNT
	// (three), so the old call passing SAFEBOX_PAGE_SIZE - our own compat macro
	// for WIDTH * HEIGHT, forty-five - was rejected outright and the page the
	// bot had just paid for never registered in memory. The window the
	// storekeeper opens has two tabs, which is the real capacity: ninety cells.
	const int PLAYERBOT_SAFEBOX_PAGES = 2;
	// How much one visit may take back out. A withdrawal that filled the bag
	// would only be deposited again on the next trip, so the door opens a
	// crack rather than all the way.
	const int PLAYERBOT_SAFEBOX_WITHDRAW_MAX = 6;
	// Split stacks in the box poured together per visit (MergePlayerBotSafeboxStacks).
	const int PLAYERBOT_SAFEBOX_STACK_MERGES_PER_VISIT = 16;
	// Two stacks of one thing in two cells is what a partial purchase, a
	// partial sale and a pick-up into a full stack all leave behind, and the
	// engine only merges when a hand drags one onto the other - which a bot
	// has none of. Every five minutes a bot pours its split stacks together
	// (the engine's own rule: same vnum, same sockets, two hundred to a
	// stack); at a counter it does the opposite, and puts a few single units
	// of the goods a player buys one at a time - scrolls, soul stones - on
	// lines of their own, because a private shop sells a line whole.
	const DWORD PLAYERBOT_STACK_MERGE_INTERVAL = 300000;
	// A tidy bag: every potion first, then chests and keys, then the rest
	// (Tieru: "co jakis czas sortowac ekwipunek", and on 15 September
	// "wszelakie potki pierwsze a potem reszte"). Only single-cell items move,
	// each with the engine's own MoveItem into an empty cell - a swap goes
	// through a free cell in three - so nothing is merged, overwritten or
	// lost. Bounded per pass; runs on the stack-merge clock. r40250 only: on
	// the 2.x line the bag is laid out whole, below.
	const int PLAYERBOT_SORT_MAX_MOVES = 30;
	// The bots' "Scal i uporzadkuj" - the one a player's inventory button
	// asks for (playerbot_arrange.cpp): the stacks poured together and the
	// four pages laid out, potions first. Every half hour or so and not on
	// every stack-merge pass: an item picked up since the last one shifts
	// everything after its place in the order, and every item that moves is a
	// save for the db core. A bot that was busy (a counter being served, the
	// safebox open) is asked again a minute later.
	const DWORD PLAYERBOT_ARRANGE_INTERVAL = 1800000;
	const DWORD PLAYERBOT_ARRANGE_SPREAD = 600000;
	const DWORD PLAYERBOT_ARRANGE_BUSY_RETRY = 60000;
	// How long a bot may stand waiting for the engine's equip window before
	// the wait is abandoned. Twelve archers stood at arrival points for
	// twenty minutes, reset by the watchdog every ninety seconds, ticked and
	// silent: the tick left through the "core slot empty and an equip
	// pending" pause, and an archer's shield slot is empty for life.
	const DWORD PLAYERBOT_EQUIP_PENDING_MAX_MS = 5000;
	// After a pause that never got its window, how long before the next one.
	// Without this the pass came back a second later and a bot in a fight
	// that never ends stuttered for five seconds out of every six.
	const DWORD PLAYERBOT_EQUIP_PENDING_RETRY_MS = 60000;
	// A weapon's percent lines multiply the damage the weapon makes, so they
	// are scored against that damage and not as a flat sum: a +47% average
	// line on a bow of 151-244 is worth 47% of that bow, and nothing on a
	// dagger of 10-12. How much of each line a build feels - a skill build
	// lives on skill damage and still swings between casts, a normal-hit
	// build the other way round.
	const int PLAYERBOT_WEAPON_OWN_LINE_PERCENT = 100;
	const int PLAYERBOT_WEAPON_OTHER_LINE_PERCENT = 35;
	// A weapon carrying its build's damage line at or above the lock (25%) is
	// the prize a player hand-made, and it must beat a refined lower weapon
	// even unrefined - because once worn it is what the blacksmith raises.
	// "boty maja w dupie bronie 30lvl ze srednimi ... biega w kosie +6"
	// (sosen94): a Riba 48% or Antyk 40% in the bag while a +6/+9 lesser
	// weapon is worn. The multiplier alone could not win it: 48% of a low
	// unrefined base is less than a +9's raised base. Proportional to the
	// line, so a genuinely better weapon still outscores it and a 12% one
	// (which is not a prize) gets nothing.
	const long long PLAYERBOT_WEAPON_PRIZE_PER_PCT = 6000;
	// A skill line this high on a weapon is a prize line too (the bonus pass
	// keeps an average line from PLAYERBOT_BONUS_KEEP_AVERAGE).
	const long PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT = 15;
	// A stone is not spent on a piece under this refine: the piece is going
	// to be refined first, and a burn on the way there takes the lines with
	// it. And a piece carrying this many lines is finished in the only sense
	// that matters at the anvil - it is refined under a scroll or not at all.
	const BYTE PLAYERBOT_BONUS_MIN_REFINE = 4;
	const int PLAYERBOT_PRIZE_LINES = 5;
	// Above this chance a valuable piece is refined without a scroll.
	//
	// refine_proto runs 90/90/90/90/80/60/50/40/30, so this lets a prize item
	// climb to +5 on its own and demands a scroll only where a failure really
	// costs something. It used to demand one below 100%, which is every step
	// there is - and the scroll was only ever looked for from +6 up, so a prize
	// weapon below that could neither be risked nor protected and simply never
	// moved. Measured on this world: 451 of 959 bots holding a scroll wore a
	// weapon stuck at exactly +4, and another 230 at +0, while 1287 Dragon God
	// and 1002 Blessing scrolls sat in their bags (zglosil sekuras).
	const int PLAYERBOT_PRIZE_SAFE_REFINE_PROB = 80;
	// The step at which a worn piece goes under a scroll whatever its plus
	// (ManagePlayerBotRefining): the one from +4, at eighty percent, burns one
	// worn weapon in five, where the ninety-percent steps below it are not worth
	// a scroll the market is short of.
	const int PLAYERBOT_WORN_SCROLL_MAX_PROB = 80;
	// The weapon in the hand is not taken to the plain anvil at such a step
	// when nothing would replace it (IsPlayerBotWornWeaponAtRisk). On
	// 15 September a warrior of 75 gifted her Halabarda +8 at 01:52, burned
	// her Zabojca Lwow at +5 -> +6 with no scroll at 02:08, and fought on with
	// a Gilotynowe Ostrze +7 of level ten. "Nothing would replace it" is a bag
	// with no weapon scoring this share of the one in the hand, and no village
	// merchant selling the class a weapon of its level. That weapon is kept
	// too: never a gift, never scrap, never counter goods.
	const int PLAYERBOT_REFINE_BACKUP_SCORE_PERCENT = 50;
	// How long the backup weapon's id is trusted by the passes that ask about
	// every weapon in the bag (IsPlayerBotKeptBackupWeapon).
	const DWORD PLAYERBOT_BACKUP_WEAPON_CACHE_MS = 3000;
	// What a Mental Warrior on a battle horse adds to a two-handed weapon's
	// score, as a share of its own blow (GetPlayerBotEquipmentScore).
	const int PLAYERBOT_TWO_HANDED_PREFERENCE_PERCENT = 20;
	// The weapon a bot plays for (playerbot_weapon_goal.h): the best family of
	// the atlas its class may carry at its level and can get on a map the bots
	// walk. A goal whose blow at +0 beats the hand's by the first share sends a
	// bot that can pay for it to the market; a counter weapon beating the hand
	// by the second is saved for like a level-30 weapon, out of the bot's own
	// budget rather than a share of the median wallet. Re-read per bot this
	// often; priced at the sheet's +0, or at the fallback where the sheet has
	// no row.
	// A piece comes back off the bot's own counter to be worn only when it beats
	// the slot by this share of what the bot already has for it
	// (BotOfflineReclaimLine): the first run on the test world took pieces back
	// for half a point of blow, a db round trip and a counter line each.
	const int PLAYERBOT_OFFLINE_RECLAIM_MIN_GAIN_PERCENT = 10;
	const int PLAYERBOT_WEAPON_OUTCLASSED_PERCENT = 30;
	const int PLAYERBOT_WEAPON_STRATEGIC_GAIN_PERCENT = 25;
	const DWORD PLAYERBOT_WEAPON_GOAL_REFRESH_MS = 600000;
	// How many stale goals one census may work out again. The census walks the
	// whole population in one tick, and a goal is a pass over the atlas through
	// the damage model; the rest are counted from what they last read, and the
	// next census carries on from the pid this one stopped at.
	const int PLAYERBOT_WEAPON_CENSUS_REFRESHES = 400;
	const DWORD PLAYERBOT_WEAPON_GOAL_FALLBACK_PRICE = 500000;
	// The level-30 weapons (Tieru, 15 September): "taka bron +6/7 z srednimi
	// 25% jest znacznie lepsza niz krwawy miecz +5/6", the bots should want
	// them and grind them "nawet do +9", and from 37% average "tylko bodziami
	// lub zwojami, nigdy u kowala". A weapon at or above either line is refined
	// under a scroll at every step or not at all (IsPlayerBotScrollOnlyWeapon).
	const long PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE = 37;
	const long PLAYERBOT_WEAPON_SCROLL_ONLY_SKILL = 15;
	// Under this average a level-30 weapon goes to the plain anvil up to +4 and
	// takes a scroll only from the step to +5 (Tieru, 15 September: "jesli taka
	// bron ma mniejsze srednie niz 30% to warto zwojow uzywac dopiero od +5
	// wzwyz, a do +4 u kowala"). The family's steps to +3 and +4 run at 75 and
	// 65 percent, under PLAYERBOT_WORN_SCROLL_MAX_PROB, and CiosZKarpia put ten
	// of twelve scrolls in twenty minutes on those two steps of an Ostrze z
	// Czerwonej Stali of one percent.
	const long PLAYERBOT_LEVEL30_SCROLL_LOW_AVERAGE = 30;
	// How far a level-30 weapon may be pushed at the plain anvil before the
	// scrolls take over, by the average-damage line it carries. The operator's
	// table of 17 September, in his own words: a weak average is ground boldly
	// to +7, and the better the roll the earlier the risk stops being worth it,
	// because what is being protected is the roll, not the plus.
	//
	//   avg <= 14%      anvil to +7, and still a gamble above it
	//   avg 15..21%     anvil to +7
	//   avg 22..29%     anvil to +6
	//   avg 30..36%     anvil to +4
	//   avg >= 37%      scrolls from +0 (PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE)
	//
	// Measured on this world's own refine_proto, because the table's last line
	// says "unless the anvil is certain": the level-30 family runs
	// 80/70/60/50/40/30/20/10 percent from +0 to +8, so it never is - a weapon
	// over 37% average has no anvil step worth taking at all.
	const long PLAYERBOT_LEVEL30_ANVIL_AVG_CHEAP = 14;
	const long PLAYERBOT_LEVEL30_ANVIL_AVG_GOOD = 21;
	const long PLAYERBOT_LEVEL30_ANVIL_AVG_BETTER = 29;
	const long PLAYERBOT_LEVEL30_ANVIL_AVG_HIGH = 36;
	const int PLAYERBOT_LEVEL30_ANVIL_PLUS_CHEAP = 7;
	const int PLAYERBOT_LEVEL30_ANVIL_PLUS_GOOD = 7;
	const int PLAYERBOT_LEVEL30_ANVIL_PLUS_BETTER = 6;
	const int PLAYERBOT_LEVEL30_ANVIL_PLUS_HIGH = 4;
	// Above its ceiling a cheap roll is still worth a gamble now and then: the
	// weapon is common and the scroll is not ("ewentualnie szansa na to ze bot
	// pojdzie do kowala ulepszyc (40% zamiast bodziem)").
	const int PLAYERBOT_LEVEL30_CHEAP_ANVIL_PERCENT = 40;
	// And what a bot does with such a weapon at all: most of them are worth
	// working on rather than listing. A share by pid, so a keeper does not
	// change its mind about the same weapon every ten minutes.
	const int PLAYERBOT_LEVEL30_KEEP_PERCENT = 65;
	// How many such weapons one bag works on at a time; the rest are goods.
	// Four since community patch 2, point 9 ("wyjatek: bron na 30. poziom dla
	// klasy bota, ktorej limit wynosi 4 sztuki"), and only of its own class.
	const int PLAYERBOT_LEVEL30_KEEP_MAX = 4;
	// A level-30 weapon of a class this bot cannot wear is goods, and 2 717
	// of them stood on m2zip's counters at +0 and +2 on 18 September against
	// thirty weapons of any kind sold in two days. This share of them - drawn
	// by the pair, like the keep above - goes to the anvil first, as far as
	// the operator's ceiling for its line and never under a scroll (the
	// scrolls are for the bot's own gear), and is sold finished: "niech w 50%
	// przypadkach ryzykuja ulepszanie, zeby pozniej drozej sprzedac gotowy
	// przedmiot" (Tieru). Most burn on the way, which the counters can spare.
	const int PLAYERBOT_LEVEL30_SALE_REFINE_PERCENT = 50;
	const BYTE PLAYERBOT_LEVEL30_LOW_AVERAGE_SCROLL_FROM_PLUS = 4;
	// A level-30 weapon is judged at what it will be, not at what it is: its
	// blow at this plus (the family adds 48 attack by +7, nothing at +0)
	// against the best weapon the bot has, by a margin, so a draw is no reason
	// to spend a week's yang (ReadPlayerBotLevel30View).
	const BYTE PLAYERBOT_LEVEL30_PROJECT_PLUS = 7;
	const int PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT = 10;
	// The average line a hoped-for level-30 weapon is given when a bot asks
	// whether one could beat its own before it walks to a market.
	const long PLAYERBOT_LEVEL30_HOPED_AVERAGE = 20;
	// The things a bot saves up for rather than buys on a whim - a level-30
	// weapon, a horse medal, a refine scroll - may cost this share of what it
	// can spend; everything else stops at PLAYERBOT_MARKET_STACK_WALLET_PERCENT
	// of the median wallet. At mob_gold 3000 such a weapon asks millions and
	// that cap passed none of them: 2315 level-30 weapons stood on the test
	// world's counters on 15 September, 2295 of them at +0..+3, and 23 bots
	// wore one.
	const int PLAYERBOT_STRATEGIC_BUDGET_PERCENT = 80;
	// Iwakura's base for an unrefined level-30 weapon, scaled by the yang rate:
	// a bot that cannot spend that does not walk to a market for one.
	const DWORD PLAYERBOT_LEVEL30_BASE_PRICE = 500000;
	// How many safe scrolls a bot refining a weapon under them buys up to.
	const int PLAYERBOT_LEVEL30_SCROLL_WANT = 3;
	// Iwakura's community patch 2, point 1. The level-30 weapon of the bot's
	// class is every bot's from level thirty, whatever its level now, and is
	// bought off a counter when the bag has none. Purchase and anvil together
	// take at most PLAYERBOT_LEVEL30_BUDGET_PERCENT of what the bot holds.
	// Under the personalities (the Perfectionist's part) it is refined to a
	// first plus drawn by pid - 60% +6, 13% +7, 8% +8, 5% +9, and the 14% the
	// sheet leaves unnamed +6 like the majority - and at every later visit on
	// towards PLAYERBOT_LEVEL30_LONG_TERM_PLUS ("+8/+9 to ABSOLUTNY
	// PRIORYTET"), +9 for the ones that drew it.
	const int PLAYERBOT_LEVEL30_BUDGET_PERCENT = 60;
	const int PLAYERBOT_LEVEL30_FIRST_PLUS6_PERCENT = 60;
	const int PLAYERBOT_LEVEL30_FIRST_PLUS7_PERCENT = 13;
	const int PLAYERBOT_LEVEL30_FIRST_PLUS8_PERCENT = 8;
	const int PLAYERBOT_LEVEL30_FIRST_PLUS9_PERCENT = 5;
	const BYTE PLAYERBOT_LEVEL30_LONG_TERM_PLUS = 8;
	// From this average a level-30 weapon goes under a Blessing Scroll from +3
	// (the anvil's ceiling, GetPlayerBotLevel30AnvilCeiling), and one on a
	// counter carrying it is bought when it beats every one the bot holds.
	const long PLAYERBOT_LEVEL30_BLESSING_FROM_3_AVERAGE = 34;
	// The monster a blow is modelled against: the bot's own level, its defence
	// about fifteen over that on this proto (GetPlayerBotWeaponHitDamageAt).
	const int PLAYERBOT_MONSTER_DEFENCE_OVER_LEVEL = 15;
	// The Magic Stone keeps the level on a failure, so it is saved for the
	// steps at or under this chance (FindPlayerBotRefineScrollCell, mt2009).
	const int PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB = 45;

	const int PLAYERBOT_STACK_MERGES_PER_PASS = 4;
	const int PLAYERBOT_STACK_MAX = 200;
	const int PLAYERBOT_SHOP_SINGLE_UNITS = 4;
	const int PLAYERBOT_SHOP_SPLIT_KEEP_FREE_CELLS = 3;
	// A material goes on the counter in packs, not as the whole stack: sixteen
	// fishbones on one line were sixteen or nothing ("moze dzielic na pakiety
	// po 2 sztuki lub nawet sprzedawac detalicznie po 1"). Packs of this many,
	// up to this many lines of one kind; the rest of the stack stays in the
	// bag for the next opening. Pearls and the shell are singles.
	// Five since 2.0.68: a recipe step takes one or two, and the offline
	// stand adds one line a service visit, so packs of two were a counter of
	// pairs - and the stand never cut a pack at all, it put the stack up
	// whole (25 Kawalek Lodu for 19.7 million on one line, "wystawia ulepy w
	// stacku po 20-40 gdzie nikt tego nie kupi", uxietoszef, 17 September).
	const int PLAYERBOT_SHOP_PACK_UNITS = 5;
	const int PLAYERBOT_SHOP_PACK_LINES = 8;
	// Lines of one refine material on an offline stand, a hoard's packs of ten
	// or the ordinary packs above (BotOfflinePrepareVisitLine).
	const int PLAYERBOT_SHOP_MATERIAL_LINES = 3;
	// Goods worth pennies a piece go up by the heap (IsPlayerBotBulkGoods):
	// what Iwakura's sheet prices at this or less before the yang rate - the
	// herbs, the ores - in lines of PLAYERBOT_SHOP_BULK_PACK_UNITS, never
	// under PLAYERBOT_SHOP_BULK_MIN_UNITS (the herbalist's recipe takes ten),
	// PLAYERBOT_SHOP_BULK_LINES of a kind. Measured on m2zip on 17 September:
	// 3343 lines of Korzen Gango and Grzyb Tue, 1171 of them a single root,
	// one shop with 34 herb lines holding 89 units, while 517 bags held 62 813
	// roots in stacks of 200 ("korzenie gango i inne ziolka sa stackowane w
	// sklepach po 1, gdzie takie tanie przedmioty powinny byc stackowane w
	// duzych ilosciach", Tieru). The cheapest refine material on those
	// counters asked 240 thousand a unit, the dearest herb 67 thousand.
	const DWORD PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE = 5000;
	const int PLAYERBOT_SHOP_BULK_PACK_UNITS = 50;
	const int PLAYERBOT_SHOP_BULK_MIN_UNITS = 10;
	const int PLAYERBOT_SHOP_BULK_LINES = 2;
	// A hoard is goods whatever the ledger reads the market as: this many
	// units of a refine material over the anvil's reserve go on a counter in
	// packs of PLAYERBOT_SHOP_HOARD_PACK_UNITS, up to PLAYERBOT_SHOP_HOARD_LINES
	// of one kind on a counter (IsPlayerBotHoardedMaterial). "Niektore boty
	// maja po prawie 200 danego ulepszacza ... powinni wystawiac nie po 1
	// sztuce a po 10" (Tieru, 15 September): 158 bots held 19 577 Nieznane
	// Lekarstwo that day, and the ledger called 7182 of an hour's listing
	// decisions overstock, so none of it ever left a bag.
	const int PLAYERBOT_SHOP_HOARD_MIN_UNITS = 50;
	const int PLAYERBOT_SHOP_HOARD_PACK_UNITS = 10;
	const int PLAYERBOT_SHOP_HOARD_LINES = 3;
	const int PLAYERBOT_SHOP_HOARD_SCORE = 440;
	// A safe refine scroll goes on a counter in lines of at most this many,
	// up to PLAYERBOT_SHOP_SCROLL_LINES lines of them on one counter. The
	// classic stall cut singles; the offline stand's service visit put the
	// stack up as it was, and twenty Blessing Scrolls on one line are 4.7
	// million yang for somebody who wants one refine ("boty wrzucaja bodzia po
	// 20 sztuk na sklep, powinny rozdzielac po 1-5", jaksiezabic, 15
	// September). A line already standing with more comes home at the next
	// service visit (BotOfflineUnwantedLine) and goes up again in fives.
	const int PLAYERBOT_SHOP_SCROLL_LINE_UNITS = 5;
	const int PLAYERBOT_SHOP_SCROLL_LINES = 3;
	// Single lines of one kind kept by count - the books of one skill, the
	// soul stone - one offline counter carries at a time. A line is one unit
	// (GetPlayerBotStallLineUnits), because a bot buys a line only when all of
	// it fits what it is short of, and a stand adds one line a visit, so ten
	// spare books of one skill would otherwise be the next ten visits' only
	// goods. Three is the stone's keep: no bot is ever short of more stones.
	const int PLAYERBOT_SHOP_COUNTED_SINGLE_LINES = 3;
	// Keys of one kind a bot holds on to with no chest in the bag; the rest
	// are goods (IsPlayerBotSurplusTreasureKey). 2598 gold and silver keys lay
	// in 1057 bags on the test world on 15 September, and not one of those
	// bags held a chest they open.
	const int PLAYERBOT_TREASURE_KEY_KEEP = 2;
	const int PLAYERBOT_SHOP_KEY_SCORE = 360;
	// Surplus keys or polymorph marbles that open a counter by themselves
	// (HasPlayerBotHoardedGoods), per thousand at the neutral TRADE weight.
	const int PLAYERBOT_SHOP_HOARD_KEYS = 4;
	const int PLAYERBOT_SHOP_HOARD_MARBLES = 2;
	const int PLAYERBOT_SHOP_HOARD_ROLL = 1000;
	// How soon the bag is merged again after the counter closes: the singles
	// and packs were split for the counter, and a bag of them is a bag with
	// no room for loot until the five-minute clock came round.
	const DWORD PLAYERBOT_STACK_MERGE_AFTER_SHOP_MS = 5000;
	// At least this many surplus books opens a counter whatever the
	// personality rolled. 21 stalls on a thousand bots,
	// "a few KU on them", and bots flying round the stones with bags full of
	// books: the roll picked one bot in ten and the books sat with the other
	// nine.
	const int PLAYERBOT_SHOP_BOOK_PRESSURE_MIN = 6;
	// Iwakura's community patch 2, point 5. Another build's books go on the
	// counter ahead of the ordinary goods, and this many of them open a
	// counter by themselves (under the TRADE weight, like the books above).
	const int PLAYERBOT_SHOP_OTHER_CLASS_BOOK_SCORE = 600;
	// What a piece of Iwakura's list past its keep scores on a counter
	// (community patch 2, point 9): the gamblers' stock, above the materials.
	const int PLAYERBOT_SHOP_LPP_SURPLUS_SCORE = 700;
	const int PLAYERBOT_SHOP_OTHER_CLASS_BOOK_MIN = 3;
	// And a bot in town as the Trader buys the books of its own skills at
	// Master whatever its gear stands at, with at most this share of its yang
	// a visit - the window opens with the town visit, or lasts this long where
	// no visit opened one.
	const int PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT = 30;
	const DWORD PLAYERBOT_BOOK_BUDGET_WINDOW_MS = 60 * 60 * 1000;
	// How often a bag of surplus books alone opens a counter, per thousand,
	// before the TRADE weight is applied. A thousand means "always" at the
	// neutral weight, which is what this rule did before it answered to the
	// slider at all - and nothing at the minimum, which is what an operator
	// dragging the slider down is asking for. Measured on this world: 238 of
	// 970 bots hold six or more surplus books, so this one clause decided a
	// quarter of the population whatever the setting said (zglosil Shenyo:
	// 180 straganow na 288 botow przy suwaku na minimum).
	const int PLAYERBOT_SHOP_BOOK_ROLL = 1000;
	const DWORD PLAYERBOT_SOUL_STONE_CHECK_INTERVAL = 10000;
	// What UseItemEx leaves in the socket when the 30% roll fails. Defined as a
	// file-local const in char_item.cpp, so it is repeated here.
	const DWORD PLAYERBOT_BROKEN_SOUL_STONE_VNUM = 28960;
	// Iwakura's soul stones (playerbot_item_tiers.h, 19 September) say which
	// stones may go into the hunting set at all; these say which of those are
	// worth a socket. A socket takes a stone for good, so a stone he rates 1
	// ("calkowicie mija sie z celem") or 2 ("praktycznie bezuzyteczne") in PvE -
	// Magii and Powtorki +4 on his list - would only hold the socket against a
	// better one; neutral (3) and up is seated. And not on a piece under +6: a
	// +3 or +4 there is a stone thrown away with the piece the bot outgrows. On
	// a +8 or +9 the socket waits for a +4.
	const int PLAYERBOT_SOUL_STONE_MIN_PVE_TIER = 3;
	const int PLAYERBOT_SOUL_STONE_MIN_GEAR_REFINE = 6;
	const int PLAYERBOT_SOUL_STONE_TOP_GEAR_REFINE = 8;
	const int PLAYERBOT_SOUL_STONE_TOP_GEAR_MIN_GRADE = 4;
	// The operator's one exception to Iwakura's ban on +0..+2 (Tieru, 19
	// September: "te kamienie mozna wkladac jak sie dropnie do slabych itemow
	// do 21 levela jesli sa to itemy co najwyzej +6"): a stone of a banned
	// grade that the bot found goes into a piece of level 21 or less at +6 or
	// less, if its kind is one his list rates for the hunting set. It is never
	// bought for that - the market wants +3 and +4 only.
	const int PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_LEVEL = 21;
	const int PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_REFINE = 6;
	// And Community Patch 1 names the grade the exception is for: "w ekwipunku
	// przeznaczonym na poziomy 1-20 dopuszcza sie umieszczanie Kamieni Duszy
	// (KD) +2, pod warunkiem, ze sa to wartosciowe kamienie zgodnie z tabela
	// tierow KD". A +0 or a +1 goes nowhere now, whatever the piece; the kind
	// is judged by the table as it always was.
	const int PLAYERBOT_SOUL_STONE_WEAK_MIN_GRADE = 2;
	const DWORD PLAYERBOT_GOAL_PLAN_INTERVAL = 5000;
	// How long the population takes to log in after a start, and how often a
	// batch goes out. The whole cohort used to be asked for in one call, and the
	// database answered in one second: 848 characters entering the world at
	// once, every one of them asking for a route in its first tick against a
	// navigation budget of 32 plans per tick. What could not be planned stood
	// still, the inactivity watchdog reset it, and the reset asked again - 4075
	// resets in the first nine minutes, and one core pinned. A minute's worth of
	// batches is long enough that no tick sees more arrivals than it can plan
	// for, and short enough that nobody watching notices the world filling up.
	const DWORD PLAYERBOT_SPAWN_WINDOW = 60000;
	const DWORD PLAYERBOT_SPAWN_BATCH_INTERVAL = 1000;
	// And how often the world is counted afterwards, to put back what it has
	// lost. The queue used to be filled once at startup and never again: a bot
	// that failed to enter the world, or left it later for any reason, was gone
	// until the next restart. An operator reported a thousand asked for, six
	// hundred and fifty arriving, and three hundred and fifty an hour later -
	// and nothing in the core would have noticed any of that.
	const DWORD PLAYERBOT_TOPUP_INTERVAL = 60000;
	// The operator's spawn plan (PLAYERBOT_SPAWN_WINDOW_MINUTES,
	// PLAYERBOT_LATE_JOINERS, PLAYERBOT_LATE_JOIN_HOURS, read by the bootstrap
	// in input_db.cpp): the cohort may be asked to arrive over a quarter of an
	// hour instead of the minute above, and a second cohort may join one at a
	// time over a day - "1000 wbija w ciagu 15 minut, a dodatkowe 500 dolacza
	// stopniowo w ciagu 24 godzin" (Tieru, 16 September), the day a player
	// started two thousand at once and the square "looked like a hospital".
	// These are the bounds; the defaults are the minute above and nobody late.
	const DWORD PLAYERBOT_SPAWN_WINDOW_MAX_MINUTES = 180;
	const DWORD PLAYERBOT_LATE_JOIN_MAX_HOURS = 168;
	// "Boty graja jak zywi ludzie": the LIFE switch of the weights file, off
	// by default and experimental. A bot plays a session, logs out for a
	// rest, and the top-up brings it back afterwards. The first session after
	// a start is drawn from half an hour up, so the log-outs spread over the
	// day instead of the whole cohort leaving together hours after a restart.
	// At these figures about two bots in five are online at any moment, which
	// is the price of the thing.
	const DWORD PLAYERBOT_LIFE_CHECK_INTERVAL = 60 * 1000;
	const DWORD PLAYERBOT_LIFE_FIRST_SESSION_MIN_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_SESSION_MIN_MS = 3 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_SESSION_MAX_MS = 6 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_REST_MIN_MS = 3 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_REST_MAX_MS = 9 * 60 * 60 * 1000;
	// A bot beside a player is not logged out from under them; it waits.
	const DWORD PLAYERBOT_LIFE_POSTPONE_MS = 10 * 60 * 1000;
	// At most this share of the cohort rests at once (ManageLifeSchedule): a
	// session that ends past it goes on for another HOLD_MIN..MAX and asks
	// again. The free-running figures above settle at about three in five
	// resting, and after a start - every bot in at once, every first session
	// over inside six hours - at nine in ten for the evening; with the cap at
	// least three in five of the bots the operator asked for are in the world
	// at every moment, and every one of them still takes its rests.
	const size_t PLAYERBOT_LIFE_MAX_RESTING_PERCENT = 40;
	const DWORD PLAYERBOT_LIFE_HOLD_MIN_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_HOLD_MAX_MS = 40 * 60 * 1000;
	const DWORD PLAYERBOT_LIFE_CENSUS_INTERVAL = 10 * 60 * 1000;
	// And the same spread for a bot's own first heavy passes - the refine, the
	// gear pass, the shopping decision - which all had timers of zero and so
	// all ran on the bot's first tick, whichever second it logged in.
	const DWORD PLAYERBOT_FIRST_PASS_SPREAD = 60000;
	const DWORD PLAYERBOT_STATUS_SNAPSHOT_INTERVAL = 2000;
	// The most of a status the line over a bot's head carries on the 2.x line:
	// the status is built in 160 bytes, and the client root's decoder
	// (playerbot_status_tail.py, MAX_STATUS_BYTES) refuses anything longer.
	const size_t PLAYERBOT_STATUS_TAIL_MAX_BYTES = 159;
	// A bot's personality in its title's place (ManagePlayerBotPersonalityTitle):
	// sent while a player is near, again every PLAYERBOT_TITLE_RESEND_MIN_MS to
	// _MAX_MS, and a player is looked for every PLAYERBOT_TITLE_PROBE_MS otherwise.
	const DWORD PLAYERBOT_TITLE_RESEND_MIN_MS = 8000;
	const DWORD PLAYERBOT_TITLE_RESEND_MAX_MS = 12000;
	const DWORD PLAYERBOT_TITLE_PROBE_MS = 3000;
	// A Metin which repeatedly heals all dealt damage is not progress. Sample its
	// lowest observed HP at a deliberately cheap cadence, give a newcomer time to
	// change the outcome, and only then let the bot look for a productive target.
	const DWORD PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL = 4000;
	// The same three numbers for an ordinary monster. Slightly more patient than
	// the stone timings: a stone stands still and takes what it is given, while
	// a monster that fights back can leave a bot chasing it round a tree for a
	// few seconds without that meaning the fight is hopeless.
	const DWORD PLAYERBOT_FIGHT_INITIAL_GRACE = 20000;
	const DWORD PLAYERBOT_FIGHT_STALL_TIMEOUT = 30000;
	const DWORD PLAYERBOT_FIGHT_FAILED_COOLDOWN = 120000;

	const DWORD PLAYERBOT_STONE_INITIAL_GRACE = 18000;
	const DWORD PLAYERBOT_STONE_SOLO_STALL_TIMEOUT = 26000;
	const DWORD PLAYERBOT_STONE_GROUP_STALL_TIMEOUT = 42000;
	const DWORD PLAYERBOT_STONE_FAILED_COOLDOWN = 90000;
	const int PLAYERBOT_STONE_SUPPORT_RANGE = 2200;
	// A stone is broken together, not claimed. Up to this many bots may be on
	// one before the next is sent elsewhere; a bot joins a stone others are
	// already breaking up to PLAYERBOT_STONE_JOIN_LEVEL_DELTA over its own
	// ("jesli nie da sobie rady, niech dolacza", Tieru, 16 September) and
	// nobody fights one more than PLAYERBOT_STONE_OUTGROWN_LEVELS under itself:
	// the band of characters on an ordinary stone is sixteen levels either
	// way ("przedzial postaci bijacych metina niech wynosi maksymalnie 16
	// poziomow", Tieru, 16 September - the drop curve is 1% at fifteen over,
	// so past that a stone gives nothing). A Demon Tower stone is not a Metin
	// but a floor's objective and has no band: IsPlayerBotDungeonStoneObjective.
	// A stone only a player is hitting is left to the player unless the switch
	// says otherwise, because the drop goes to whoever dealt the most damage.
	// Every bot scores a stone in its band above the sweet-spot monster, and a
	// stone somebody is already on gets the join bonus on top.
	const BYTE PLAYERBOT_STONE_MAX_ATTACKERS = 6;
	const int PLAYERBOT_STONE_JOIN_LEVEL_DELTA = 16;
	const int PLAYERBOT_STONE_OUTGROWN_LEVELS = 16;
	const bool PLAYERBOT_STONE_JOIN_PLAYERS = false;
	const int PLAYERBOT_STONE_BASE_SCORE = 500000;
	const int PLAYERBOT_STONE_JOIN_BONUS = 600000;
	const DWORD PLAYERBOT_BUFF_INTERVAL = 2000;
	const DWORD PLAYERBOT_SKILL_ATTACK_INTERVAL = 2500;
	// A client-side skill motion is longer than one normal attack tick.  Without
	// this lock a basic bow/dagger hit replaced the skill animation after 600 ms,
	// although the server had already applied the skill damage.
	const DWORD PLAYERBOT_SKILL_ANIMATION_LOCK = 1400;
	const BYTE PLAYERBOT_RESERVE_GEAR_MIN_REFINE = 6;
	// A +7 or better is never NPC fodder. The merchant pays a fifth of the shop
	// price for it, and a bot vendored a Riba +9 for exactly that because it
	// happened to be carrying an axe +9 as well - the "keep only the best spare
	// per slot" rule had no idea what it was throwing away. Anything at this
	// refine goes on a stall instead, where another bot can pay properly.
	// How long after its last swing a bot still counts as hunting. A session is
	// a string of fights with gaps for walking and looting between them, so the
	// window has to outlast a gap without outlasting the walk back to town.
	const DWORD PLAYERBOT_BUFF_COMBAT_WINDOW = 60000;
	// How soon a bot comes back for the next buff once it has found one
	// missing. One cast claims the tick, so five seconds between them meant a
	// Warrior needed ten seconds for aura and berserk and a weapon Sura fifteen
	// for its three enchantments - longer than most of the fights they were
	// buffing for, which is why they were usually seen without them.
	const DWORD PLAYERBOT_BUFF_RECHECK_FAST = 1200;
	// An unfinished town errand is somebody's job until it is done.
	//
	// The 8 September audit traced the loop: the bot needs a merchant, the
	// route is deferred, the inactivity watchdog fires, the visit is thrown
	// away, and a second later the bot is casting an attack skill at whatever
	// stands nearby - with the need it came for still unmet. The watchdog may
	// cancel a stale route; it may not cancel the errand. These carry the
	// errand across the reset and keep the bot out of a fresh grind while it
	// waits for its retry.
	const DWORD PLAYERBOT_SERVICE_RETRY_MIN = 15000;
	const DWORD PLAYERBOT_SERVICE_RETRY_MAX = 40000;
	// How long a service may stay unfinished before it is given up and the
	// ordinary planner takes over again, so nothing can wedge for ever.
	const DWORD PLAYERBOT_SERVICE_GIVE_UP = 900000;
	// A town nobody stays in is a town nobody sees.
	//
	// Joan holds four hundred bots and its square holds a couple of dozen: a bot
	// comes in for an errand and leaves the moment it is done, so the market
	// ring the stalls stand on is empty of customers and of anything to look at.
	// A share of the bots that finish an errand in Joan now stay a while - which
	// is what a player does with a town, and what makes one look inhabited.
	// Bokjung is deliberately excluded: it is crowded already, and the whole
	// point of the M2 census work was to get level-40 bots out of it.
	// Every bot that finishes something in Joan, not half of them: the triggers
	// are rare enough on their own. An angler fishes for fifteen to forty
	// minutes and then rests for three quarters of an hour to two hours, so a
	// session ends about once a minute across the whole angler cohort - at half
	// that is three or four bots on the square at a time, which is not a market.
	// Since 2.0.9 the share is the REST key of the weights file
	// (GetPlayerBotRestPercent, a hundred by default, zero for an operator who
	// wants every bot hunting), and nobody under this level rests at all: a
	// bot of twelve has levels to gain and nothing to browse for, and the
	// operator who asked for the slider wants the young ones out whatever the
	// square looks like. A rest also needs counters on the map -
	// MayPlayerBotRestInTown in playerbot_config.h is the whole rule.
	const BYTE PLAYERBOT_TOWN_REST_MIN_LEVEL = 18;
	// Three minutes of walking the counters, not four to ten of standing.
	//
	// The first version parked a bot on one spot of the square and left it
	// there, which filled Joan and made it look like a car park: a hundred
	// people motionless for up to ten minutes. What a town needs is movement,
	// and the market ring is what there is to walk between - so the bot strolls
	// from counter to counter instead, picking a new one every few seconds.
	const DWORD PLAYERBOT_TOWN_LINGER_MIN = 150000;
	const DWORD PLAYERBOT_TOWN_LINGER_MAX = 210000;
	// How long a bot looks at one counter before moving to the next. Long
	// enough to read as looking at something, short enough that the square is
	// never still.
	const DWORD PLAYERBOT_TOWN_BROWSE_MIN = 6000;
	const DWORD PLAYERBOT_TOWN_BROWSE_MAX = 14000;
	// And how long after that a counter it never reached is given up on.
	const DWORD PLAYERBOT_TOWN_BROWSE_GIVE_UP = 20000;
	// Four, not six: a spare at +4 is what the counter lists
	// (PLAYERBOT_SHOP_MIN_GEAR_REFINE) and what a player buys, and at six
	// the merchant took every +4 and +5 the bot had just paid the blacksmith
	// for - Brwisty Wachlarz+4 refined at 17:58 and vendored at 18:19, in
	// one operator's equipment history; 12 534 pieces refined in the bag and
	// then vendored in six hours on our own world.
	const BYTE PLAYERBOT_PRECIOUS_REFINE = 4;
	// A worse duplicate of a filled slot opens a stall only when it is this
	// refined - a genuinely valuable spare, the +9 FMS the report was about.
	// At +4 it caught 759 bots at once ("759 Prowadze stragan (zbedny
	// duplikat)", akhigubernator): every second weapon or armour in a bag
	// qualified, and the town filled with keepers ignoring the trade slider.
	const BYTE PLAYERBOT_SHOP_SPARE_MIN_REFINE = 7;
	// An Archer breaks a Metin with a dagger, and a +0 dagger breaks nothing:
	// "powinni uzywac ulepszonych sztyletow na co najmniej +4, nie nizej bo nic
	// z tego nie bedzie" (Tieru). The stone dagger is worn only on a stone, so it
	// never counts as a wearable upgrade or a higher-tier spare and would never
	// be refined in the bag - this floor makes it a refine candidate and its
	// target. The +1..+4 steps are 90% each on this world's table, so reaching it
	// is cheap and low-burn; a scroll in the bag still carries it higher.
	const BYTE PLAYERBOT_ARCHER_STONE_MIN_REFINE = 4;
	// The lowest refine an ordinary spare may carry and still be worth a counter
	// slot. Below it nobody wants the thing: the market code buys medals,
	// level-30 weapons and big bonus rolls, and a person walking the market sees
	// a row of +1 armours and calls it junk - which it is.
	const BYTE PLAYERBOT_SHOP_MIN_GEAR_REFINE = 4;
	// And the level the piece is for. Of the 487 spares at +4 or +5 in this
	// world's bags, 272 are for level 29 or below - level-26 bodies, level-25
	// and lower weapons, level-17 boots, a handful of level-0 starter pieces -
	// each of them one tier behind what its owner is already wearing and worth
	// nothing to anybody who might walk past. The gear a player crosses a market
	// for starts at level 30.
	//
	// That rule was written and never ran: the precious-refine branch of
	// ScorePlayerBotShopStock returned first for anything at +4, so the level
	// test only ever saw +0 to +3, which it refused anyway. On 14 September the
	// counters of the test world carried 4 802 lines of gear under level thirty,
	// 2 409 of them at +4 and +5 - Czer. Ubranie Mrowki+5 on 348 lines, Lwia
	// Zbroja Plytowa+5 on 341 - and a player on the Discord asked whether every
	// server had "takie janusze biznesu". The operator's line: such a piece goes
	// on a counter at +6 or better, once the bot is done with it, and never a
	// counter full of it ("zeby nie robili takiej masowki"). Below +6 it is the
	// merchant's (IsPlayerBotJunkItem) - the eleventh of September's "nothing
	// above +4 to the merchant" still holds for the gear from level thirty.
	const int PLAYERBOT_SHOP_MIN_GEAR_LEVEL = 30;
	const BYTE PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE = 6;
	// How many lines of it one counter carries, counting what an offline shop
	// already holds; the service visit takes any more off, one a visit.
	const int PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES = 2;
	// Starter gear - a weapon or body armour of level one - goes up only from
	// +7: "Miecz+6, bo to bron na 1 lv, wiec nic nie warta, raczej do handlarza,
	// chyba ze bylaby +8 lub +9" (Tieru, 15 September) made it +8, and the
	// measurement of 20 September said what that cost - fifteen starter +7 a
	// day handed to a merchant, because this one number is both thresholds:
	// the counter takes a piece from it and the junk rule scraps everything
	// under it, so +7 fell between them. "+7 to nigdy nie jest zlom" (Tieru,
	// 20 September) is the rule that wins, and the only way to keep it without
	// leaving a +7 in the bag for good is to let the counter have it. Below it
	// the piece is still the merchant's. The cap above counts only lines under
	// PLAYERBOT_SHOP_LOW_GEAR_CAP_BELOW_REFINE: a sura of twenty-five kept a
	// Sejmitar+7, a Dlugi Miecz+6 and an armour+6 in its bag because a pair of
	// boots+9 and a sword+7 already held the two places.
	const int PLAYERBOT_SHOP_STARTER_GEAR_MAX_LEVEL = 1;
	const BYTE PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE = 7;
	const BYTE PLAYERBOT_SHOP_LOW_GEAR_CAP_BELOW_REFINE = 7;
	// Where it ranks: after the materials and the chests, before a scrap
	// keeper's fodder - and under PLAYERBOT_SHOP_PRIZE_SCORE, so it never
	// carries a stall on its own.
	const int PLAYERBOT_SHOP_LOW_GEAR_SCORE = 300;
	// How many lines a counter needs before it is worth a sign. One is not a
	// market stall: a player walks past, opens it, and finds a single spare.
	// Eighteen of the thirty-four stalls this world opened in the fourteen
	// minutes after a restart carried exactly one item.
	//
	// Two rather than three, measured rather than guessed. Three left eight
	// stalls standing where the old rule had left thirty-four - and the seven
	// with three lines or more were the same seven either way, so the extra
	// strictness bought nothing except a quieter market.
	// Three lines make a stall, which is what the rule beside it always said it
	// meant while the number said two. Reported from the Discord: "it makes no
	// sense that a bot with plenty of yang and free bag space forces a shop open
	// with one Bear Hide or some other trinket" - and two ordinary materials is
	// the same complaint one line further on. A stall exists to move a surplus;
	// a keeper with money and room has no surplus to move. What still opens on
	// its own is a genuine prize (PLAYERBOT_SHOP_PRIZE_SCORE): a level-30
	// weapon, anything at +6, a big bonus roll, a horse medal - a material never
	// scores that high.
	const size_t PLAYERBOT_SHOP_MIN_ITEMS = 3;
	// A bot that cannot afford its potions sells what it has, at a discount,
	// with one line if that is all it has - "wystawianie sklepu przez bota
	// jak ma malo yang", nine votes on the Discord. Below this much gold the
	// stall gate opens for anybody, and the asking prices come down.
	// "Too poor" is measured against what a potion trip costs at the bot's
	// level (PLAYERBOT_POTION_TRIP_RED/BLUE at the merchant's unit prices),
	// not a flat number: a flat thirty thousand made every bot under twenty
	// a keeper, and thirty-six of them stood at the Joan ring at level
	// seventeen on the first pass - which is what "too many bots wandering
	// at the safe zone and not levelling" looks like from a player's chair.
	// Under twenty a bot earns faster by levelling than by selling, and of
	// the poor only a share is at the ring in any hour, by pid.
	const BYTE PLAYERBOT_SHOP_POOR_MIN_LEVEL = 20;
	const DWORD PLAYERBOT_SHOP_POOR_ROTATION_MS = 3600000;
	const DWORD PLAYERBOT_SHOP_POOR_ROTATION_SHARE = 4;
	const int PLAYERBOT_SHOP_POOR_DISCOUNT_PERCENT = 70;
	// A Biologist specimen the bot no longer needs - its mission is handed in
	// - is goods for the counter, priced like a book. The Orc Tooth is also a
	// refine material and goes through the material rules and the ledger,
	// which is the market, not the Biologist's doorstep.
	const int PLAYERBOT_SHOP_SPECIMEN_SCORE = 350;
	// Unless that one line is the reason somebody would cross the market: a
	// level-30 weapon, a horse medal, a big bonus roll, anything at +6 or better.
	// This is the score at which a single item carries a stall on its own.
	const int PLAYERBOT_SHOP_PRIZE_SCORE = 900;
	// A bonus line big enough to make an item worth selling whatever else it is.
	// A thousand health is roughly what a good armour of the level range adds, so
	// anything at or above it was rolled well rather than ordinarily.
	const int PLAYERBOT_VALUABLE_HP_BONUS = 1000;
	// What a bot asks for a spare. Invented rather than derived: the item tables
	// carry no price for a refined weapon, and these are meant to be affordable
	// to a bot that has been hunting for an hour rather than a jackpot.
	const DWORD PLAYERBOT_SHOP_PRICE_PLUS7 = 150000;
	const DWORD PLAYERBOT_SHOP_PRICE_PLUS8 = 400000;
	const DWORD PLAYERBOT_SHOP_PRICE_PLUS9 = 900000;
	// Iwakura's price competition: two bots holding the same +N with the same
	// bonus lines would otherwise both ask the flat price above, so a market of
	// stalls shows one number instead of a spread. A stable per-keeper swing of
	// up to this many percent (Iwakura's "1-20%") lets one undercut the other.
	const DWORD PLAYERBOT_SHOP_PRICE_JITTER_PCT = 20;
	// Refine materials go up at a small markup over the merchant price, so a bot
	// that needs one can buy it from a neighbour instead of farming for it.
	const DWORD PLAYERBOT_SHOP_MATERIAL_MARKUP = 3;
	// A soul stone has no shop price in the proto, so a counter asks this by
	// grade (+0 to +4) until the market has paid something for it.
	const DWORD PLAYERBOT_SHOP_PRICE_SOUL_STONE[5] = { 30000, 60000, 120000, 250000, 500000 };
	// Browsing someone else's stall.
	const DWORD PLAYERBOT_SHOPPING_INTERVAL_MIN = 120000;
	const DWORD PLAYERBOT_SHOPPING_INTERVAL_MAX = 300000;
	// The engine refuses a purchase beyond 2000, so stay inside that.
	const int PLAYERBOT_SHOPPING_RANGE = 1800;
	// Gold a bot will not spend on the market; potions and gear come first.
	const DWORD PLAYERBOT_SHOPPING_GOLD_FLOOR = 200000;
	// The trip to the first village's counters for a skill book, a Kamien
	// Duchowy or a Biologist specimen (playerbot_progression_needs.h). Almost
	// every bot with a skill at Master is short of books, so the trip is a
	// share of the live population like the Biologist's errands (2.0.60 sent
	// "every bot with an outgrown herb row" and half the world rode into the
	// gates): PLAYERBOT_PROGRESSION_TRIP_PER_MILLE of the bots at a time, for
	// PLAYERBOT_PROGRESSION_TRIP_MS each, asked again every _RETRY_MIN to
	// _MAX, the first time within _FIRST_MAX of a spawn.
	const int PLAYERBOT_PROGRESSION_TRIP_PER_MILLE = 30;
	const DWORD PLAYERBOT_PROGRESSION_TRIP_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_PROGRESSION_TRIP_FIRST_MIN_MS = 60 * 1000;
	const DWORD PLAYERBOT_PROGRESSION_TRIP_FIRST_MAX_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_PROGRESSION_TRIP_RETRY_MIN_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_PROGRESSION_TRIP_RETRY_MAX_MS = 45 * 60 * 1000;
	// Kamienie Duchowe a bot with a skill at G1..G10 keeps for its training.
	const int PLAYERBOT_GRAND_MASTER_STONE_KEEP = 3;
	// Bonus stones (the change stone, the add stone and the blessing marble)
	// are exempt from the junk rule - a bot must never vendor one - and no
	// counter ever listed them either, so a bot that found more than it could
	// spend kept them for good: one player's screenshot had a hundred and
	// ninety in a single bag (Nagash, 19 September, "mozna by im chociaz
	// pozwolic wystawiac te dodania i zmianki na sklep"). This many are kept
	// for the bot's own rerolling and the rest are goods.
	const int PLAYERBOT_BONUS_STONE_KEEP = 10;
	// How many refine-material cells a bot carries as stock for its own counter.
	// They stack, so this is eight cells out of ninety however many pieces are
	// held - and eight is one full stall, which is as much as it can display.
	const size_t PLAYERBOT_MATERIAL_STOCK_SLOTS = 8;
	// Recipe ids are sparse - four hundred and seven of them scattered up to
	// 759 - and the manager offers no way to iterate, so this is where the walk
	// that collects their materials stops.
	const DWORD PLAYERBOT_REFINE_RECIPE_MAX_ID = 1000;
	// Going shopping, as opposed to buying whatever happens to be within twenty
	// metres. A bot that is short of something walks over to the stall ring and
	// reads the counters; this is how long it may spend on that before it goes
	// back to whatever it was doing. Long enough to cross a town, short enough
	// that a bot which cannot get there loses one errand and not its evening.
	const DWORD PLAYERBOT_MARKET_TRIP_TIMEOUT = 90000;
	// What a keeper does with a line it has carried home unsold: ten percent
	// off per stand, four stands deep, and a piece of gear under the precious
	// refine is merchant scrap after six ("jakas losowa halabarda +5 to ja
	// sprzedaje u handlarza" - the +5 stays, PLAYERBOT_PRECIOUS_REFINE is four).
	const int PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT = 10;
	const int PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_STANDS = 4;
	// Iwakura's supply and demand (13 September, both price documents): a thing
	// that leaves the counter at once is put up dearer next time and keeps
	// climbing with every quick sale, a thing that comes home unsold gets
	// cheaper - both by ten to twenty-five percent.
	//
	// The markdown already existed at a flat ten percent per stand; it is now
	// his range, drawn per listing. He gave no ceiling for it, and four stands
	// at twenty-five percent each would take a price to nothing, so the total
	// is capped - a discount is off the margin, not off the item.
	//
	// Both are applied where the unsold markdown already is: AFTER the asking
	// price is settled. LimitPlayerBotAskStep lets the market's anchor drift
	// five percent per ten minutes on purpose, and a demand signal pushed
	// through it would either be swallowed or would drag every other counter
	// with it. This moves what this keeper asks, not what the market believes.
	const int PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL = 50;
	// The offline stand's version of the same markdown: a line nobody has
	// bought comes down PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT for every
	// PLAYERBOT_OFFLINE_UNSOLD_STEP_MS it has stood, to the same ceiling and
	// never under the blacksmith's bill (Tieru, 16 September: "jesli nie
	// schodza po obecnych cenach to zmniejszaj ceny stopniowo do jakiegos
	// stopnia minimalnego").
	const DWORD PLAYERBOT_OFFLINE_UNSOLD_STEP_MS = 2 * 60 * 60 * 1000;
	// How often a stand's lines are repriced, and how many at a time. Every
	// step of a slice is a native edit and costs one of the core's offline
	// mutations (BotOfflineBudget, one a second for every keeper together),
	// and the night of 18 September already spent 1 863 of the 3 600 an hour
	// on m2zip - 775 of them edits - before a slice existed. So a slice runs
	// on the ten-minute catch-up only while this core has seen a counter
	// priced against an older table (a yang rate moved), hourly otherwise;
	// a restart is not a change, and its first visit restocks.
	const DWORD PLAYERBOT_OFFLINE_REPRICE_SLICE = 2;
	const DWORD PLAYERBOT_OFFLINE_REPRICE_CATCHUP_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_OFFLINE_REPRICE_MS = 60 * 60 * 1000;
	const int PLAYERBOT_MARKET_DEMAND_MIN_PERCENT = 10;
	const int PLAYERBOT_MARKET_DEMAND_MAX_PERCENT = 25;
	// A stand runs PLAYERBOT_SHOP_MIN..MAX_DURATION (10-25 min), so "went at
	// once" is a line gone within the first five minutes of being put up.
	const DWORD PLAYERBOT_MARKET_FAST_SALE_MS = 300000;
	// How far the climb goes, and how long a commodity stays hot: an hour with
	// no quick sale and the market has forgotten the rush.
	const BYTE PLAYERBOT_MARKET_DEMAND_MAX_STEPS = 4;
	const DWORD PLAYERBOT_MARKET_DEMAND_DECAY = 3600000;
	const int PLAYERBOT_SHOP_UNSOLD_SCRAP_STANDS = 6;
	// Gear the merchant may never have (above PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE
	// - a shaman's warrior steel +9) used to have no end at all: discounted to
	// this many stands, then carried round the stones for ever, and a bag of it
	// is "plecak pelen rzeczy innych klas" (audit D14). After this many stands
	// unsold it goes to the storekeeper with the surplus books, under bag
	// pressure - kept, never scrapped, and out of the bag.
	const int PLAYERBOT_SHOP_UNSOLD_SAFEBOX_STANDS = 8;
	// ...and up to this refine. The rule used to sit below "+4 and up never
	// goes to an NPC", so it applied to nothing the counter actually keeps:
	// a +5 nobody bought in six stands stayed in the bag for good, and a bot
	// with a bag of them stood in Joan opening stalls instead of hunting -
	// "ciule wszystko +5 wystawiaja i od wczoraj zaden nie wbil nawet lvla"
	// (gregoszky), "boty maja zapchane eq, nie wiedza co z tym robic"
	// (davids998), both on 10-11 September.
	// The operator's line (11 September evening): the merchant may have gear
	// up to +4 and nothing above it - a +5 goes on a counter, or to the
	// blacksmith first and then on a counter. So the unsold-stands rule stops
	// at +4 too, and PLAYERBOT_MERCHANT_MAX_REFINE is the one number both
	// rules read.
	const BYTE PLAYERBOT_MERCHANT_MAX_REFINE = 4;
	const BYTE PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE = PLAYERBOT_MERCHANT_MAX_REFINE;
	// The ride from Bokjung's square to the Joan gate is 38 km.
	const DWORD PLAYERBOT_MARKET_JOAN_WALK_TIMEOUT = 300000;
	// On the 2.x line the walk over is made for a line a first village's
	// stand holds, found before setting off (StartPlayerBotFarMarketWalk):
	// the lines read per look, on a cursor of their own - the browse of the
	// stands in reach reads sixty-four too, and each line read builds a
	// comparison item - and the time the buyer then has from the gate to the
	// stand, which is the town's width on a horse with a margin.
	const unsigned int PLAYERBOT_MARKET_FAR_LOOK_LINES = 64;
	const DWORD PLAYERBOT_MARKET_FAR_PICK_WALK_MS = 120000;
	// And how far away the stalls may be before it is not worth setting off:
	// the whole of the town, so that a bot which has just finished its errands
	// goes shopping while one that is out hunting stays where it is instead of
	// walking the timeout out and turning round empty-handed. Joan's ring stands
	// round the village guard, outside the town proper - the gate is 5750 from
	// him and the far corner of the service area 10900 - so nine thousand, which
	// was measured from the gate, excluded a bot standing at the blacksmith.
	const int PLAYERBOT_MARKET_TRIP_RANGE = 12000;
	// How often it re-reads the counters while it stands among them. The scan
	// walks every entity in the surrounding sectors, so it is not a per-tick job.
	const DWORD PLAYERBOT_MARKET_BROWSE_INTERVAL = 2000;
	// How close it walks up to the stall it has picked. The engine would let it
	// buy from twenty metres, but a market where the customers stand at the
	// counters looks like a market.
	const int PLAYERBOT_MARKET_STALL_APPROACH = 350;
	// Refining only runs while the bot is physically standing at the blacksmith.
	// A real player can click several times during one visit; a three-second cadence
	// permits several attempts without extending the absolute 6-24 s visit.
	const DWORD PLAYERBOT_REFINE_INTERVAL = 3000;
	// Bonus rerolling. Both verified against share/conf/item_proto.txt rather
	// than taken from the feature notes: 71084 is USE_CHANGE_ATTRIBUTE (rerolls
	// every line) and 71085 is USE_ADD_ATTRIBUTE (adds one). A bot spends only
	// the ones in its bag - HasPlayerBotBonusStone says why it no longer buys
	// them from nobody.
	const DWORD PLAYERBOT_BONUS_CHANGE_VNUM = 71084;
	const DWORD PLAYERBOT_BONUS_ADD_VNUM = 71085;
	// Below this the gear itself is still changing every few levels, and a
	// plain stone is worth more than the piece it would go on - so a bot this
	// young spends only the green ones, which are for that gear and no other
	// (IsPlayerBotGreenBonusStone).
	const BYTE PLAYERBOT_BONUS_MIN_LEVEL = 30;
	// Zielony Czar and Zielona Sila go on a weapon or a body armour of this
	// level or less and on nothing else (char_item.cpp, the engine's rule).
	const int PLAYERBOT_GREEN_BONUS_MAX_LEVEL = 40;
	// What the bot keeps: roughly one strong offensive line, or two decent ones.
	// --- Guilds and who a bot has got on with -------------------------------
	// Forty is what a player needs at the Village Guard, and the fee is what the
	// engine charges in CInputMain::GuildCreate - CreateGuild itself charges
	// nothing, so a caller that is not the packet handler has to pay it.
	const BYTE PLAYERBOT_GUILD_MIN_LEVEL = 40;
	const DWORD PLAYERBOT_GUILD_CREATE_FEE = 200000;
	// What a bot must still have afterwards. Founding a guild and then being
	// unable to buy a potion is not an ambition, it is a bug.
	const DWORD PLAYERBOT_GUILD_GOLD_RESERVE = 100000;
	// One eligible bot in twelve founds one. Any more and the world fills with
	// guilds of one member, which is the opposite of the point.
	const DWORD PLAYERBOT_GUILD_FOUNDER_SHARE = 12;
	// The lowest grade, which is what an ordinary member joins at.
	const int PLAYERBOT_GUILD_MEMBER_GRADE = 15;
	// A master asks this many a pass, from the whole kingdom's roster.
	const DWORD PLAYERBOT_GUILD_INVITES_PER_PASS = 3;
	const DWORD PLAYERBOT_GUILD_CHECK_INTERVAL = 120000;
	// Guild tiers (playerbot_guild.h): a guild is founded at the tier its
	// founder's strength percentile puts it in - the top three percent of a
	// kingdom's bots found an elite guild, the top fifteen a strong one, the
	// top half a medium one, the rest an ordinary one - and recruits only
	// above its tier's floor; the elite and the strong keep small tables and
	// are few a kingdom, so a thousand bots end with one or two elite guilds a
	// kingdom, a few strong ones and the rest ("gildie mega mocne, silne oraz
	// srednie i slabsze", Tieru, 16 September). Indexed by EPlayerBotGuildTier.
	const int PLAYERBOT_GUILD_TIER_COUNT = 4;
	const int PLAYERBOT_GUILD_TIER_PERCENT[PLAYERBOT_GUILD_TIER_COUNT] = { 3, 15, 50, 100 };
	const int PLAYERBOT_GUILD_TIER_MEMBER_CAP[PLAYERBOT_GUILD_TIER_COUNT] = { 24, 40, 0, 0 };
	const int PLAYERBOT_GUILD_TIER_MAX_PER_KINGDOM[PLAYERBOT_GUILD_TIER_COUNT] = { 2, 6, 0, 0 };
	const DWORD PLAYERBOT_GUILD_STRENGTH_INTERVAL = 10 * 60 * 1000;
	const DWORD PLAYERBOT_GUILD_PROMOTION_HOLD_MS = 6 * 60 * 60 * 1000;
	const int PLAYERBOT_GUILD_PROMOTIONS_PER_PASS = 3;
	// Guild experience: once an hour a member offers this share of the
	// experience it gained since its last offer (CGuild::OfferExp gives the
	// guild a hundredth of it), never under the minimum and never more than
	// the level holds. The elite give more.
	const DWORD PLAYERBOT_GUILD_EXP_OFFER_INTERVAL = 60 * 60 * 1000;
	const int PLAYERBOT_GUILD_EXP_OFFER_PERCENT[PLAYERBOT_GUILD_TIER_COUNT] = { 15, 12, 10, 10 };
	const DWORD PLAYERBOT_GUILD_EXP_OFFER_MIN = 10000;
	const DWORD PLAYERBOT_GUILD_STATUS_INTERVAL = 60 * 1000;
	// Guild wars (playerbot_guild_war.h): a field war between two bot guilds
	// of one kingdom on that kingdom's guild map, every so often, thirty
	// minutes by the engine's own clock; the first one half an hour after a
	// start, and a kingdom with no pair ready asks again after the retry.
	const DWORD PLAYERBOT_GUILD_WAR_CHECK_INTERVAL = 60 * 1000;
	// A kingdom's wars: the first PLAYERBOT_GUILD_WAR_FIRST_DELAY after the
	// core's start plus one PLAYERBOT_GUILD_WAR_KINGDOM_STAGGER per kingdom,
	// then PLAYERBOT_GUILD_WAR_INTERVAL after each war's end - a war of thirty
	// minutes every two hours in each kingdom, and with the stagger a war
	// somewhere in the world for ninety minutes of every two hours. The
	// first wars all began thirty minutes after the start and ended together,
	// so a player who came to watch half an hour later found none (Tieru,
	// 16 September).
	const DWORD PLAYERBOT_GUILD_WAR_INTERVAL = 90 * 60 * 1000;
	const DWORD PLAYERBOT_GUILD_WAR_FIRST_DELAY = 30 * 60 * 1000;
	const DWORD PLAYERBOT_GUILD_WAR_KINGDOM_STAGGER = 40 * 60 * 1000;
	const DWORD PLAYERBOT_GUILD_WAR_RETRY_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_GUILD_WAR_DECLARE_TIMEOUT = 3 * 60 * 1000;
	const int PLAYERBOT_GUILD_WAR_MIN_ONLINE = 8;
	// The sides stand this far apart on the battlefield, on open ground found
	// within this radius of the map's Town.txt point (playerbot_guild_war.h).
	// Both sides rally on the same ground, the open middle nearest the map's
	// Town.txt point, and fight from the first minute: a spread of 700 made
	// two columns standing apart ("niech ida od poczatku na srodek strefy
	// sie bic", Tieru, 17 September).
	const int PLAYERBOT_GUILD_WAR_RALLY_SPREAD = 0;
	const long PLAYERBOT_GUILD_WAR_GROUND_SEARCH = 6000;
	// And the ground keeps this far from the map's safe zone. The nearest open
	// cell to the Town.txt point is the zone's own edge - fifty units from
	// ATTR_BANPK on metin2_map_guild_02 and a hundred on _03, measured on
	// 18 September - and a war fought on the edge spills over it: four minutes
	// into the Chunjo war eleven of sixty-seven bots stood where no blow lands.
	// A bot's own spot is the ground and up to 400 units of pid, so eight
	// hundred keeps the whole crowd out; the ground moves about a kilometre.
	const long PLAYERBOT_GUILD_WAR_SAFE_MARGIN = 800;
	// And the battlefield is this far round that ground and no further: a foe
	// beyond it is not chased, and a bot beyond it walks back to its spot
	// (IsPlayerBotOnWarField). A spot is the ground and 400 of pid, so the
	// crowd stands well inside; the rest is room for a charge and a chase.
	const long PLAYERBOT_GUILD_WAR_FIELD_RADIUS = 1800;
	// The Demon Tower raid (playerbot_demon_tower.h): one bot guild at a
	// time on this core, the first a few minutes after a start and the next
	// an interval after a raid ends; the members gather on the ground floor
	// for GATHER_MS and break the stone together; a run that makes no
	// progress for STALL_MS, sits on one floor for FLOOR_MAX_MS or lasts
	// MAX_MS leaves. The last three floors want a bot of UPPER_LEVEL, the
	// game's own rule at the sixth floor's smith.
	const DWORD PLAYERBOT_TOWER_CHECK_INTERVAL = 30 * 1000;
	const DWORD PLAYERBOT_TOWER_FIRST_DELAY = 12 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_INTERVAL = 90 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_RETRY_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_GATHER_MS = 4 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_STONE_TIMEOUT_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_STALL_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_FLOOR_MAX_MS = 35 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_MAX_MS = 2 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_TOWER_SMITH_WAIT_MS = 60 * 1000;
	const DWORD PLAYERBOT_TOWER_SCAN_INTERVAL = 1500;
	const DWORD PLAYERBOT_TOWER_CENSUS_INTERVAL = 10 * 60 * 1000;
	const int PLAYERBOT_TOWER_MIN_LEVEL = 40;
	const int PLAYERBOT_TOWER_UPPER_LEVEL = 75;
	const int PLAYERBOT_TOWER_MIN_MEMBERS = 4;
	const int PLAYERBOT_TOWER_MAX_MEMBERS = 16;
	const int PLAYERBOT_TOWER_HANDIN_RANGE = 300;
	const int PLAYERBOT_TOWER_GATHER_FIGHT_RANGE = 2500;
	// The pack on a floor: a bot this far from where the others stand, with
	// nothing to fight within PACK_FIGHT_RANGE of itself, walks back; a
	// floor's stones are attacked once no more than STONE_CLEAR_LIMIT
	// monsters stand (the fourth floor, stones only, always).
	const int PLAYERBOT_TOWER_PACK_RADIUS = 2500;
	const int PLAYERBOT_TOWER_PACK_FIGHT_RANGE = 700;
	const int PLAYERBOT_TOWER_STONE_CLEAR_LIMIT = 25;
	// ... or with no monster this close to the stone itself.
	const int PLAYERBOT_TOWER_STONE_CLEAR_RADIUS = 1500;
	// The pack spreads its blows, not itself: an ordinary monster is taken
	// from the few standing nearest the pack, about this many bots to each,
	// and only among those no further than SPREAD_RANGE beyond the nearest
	// one - so the pack still fights in one place. A stone and a boss stay
	// everybody's. With one target for sixteen bots a floor was cleared a
	// monster at a time ("atakuja po jednym przeciwniku", Nagash, 19 September).
	const int PLAYERBOT_TOWER_BOTS_PER_MONSTER = 2;
	const int PLAYERBOT_TOWER_SPREAD_RANGE = 600;
	// The sixth floor's smith. Once the Elite Demon King is down every
	// character in the instance may have one piece raised there for the fee
	// alone - no materials, at the anvil's own odds, and a failure burns the
	// piece as at any blacksmith (DoRefine(item, true), the engine's
	// REFINE_TYPE_MONEY_ONLY). A bot of UPPER_LEVEL takes the run past him
	// once everybody has had the turn, or after SMITH_REFINE_WAIT_MS; it used
	// to do so the moment he stood, and nobody ever used him ("nikt nie
	// korzysta z mozliwosci ulepszania przedmiotow u kowala", prodnathin,
	// 23 September).
	const DWORD PLAYERBOT_TOWER_SMITH_REFINE_WAIT_MS = 150 * 1000;
	// The floors' drop. Inside, the fight never ends - a bot always holds a
	// foe and a pack always stands round it - so the ordinary loot pass only
	// ever took what lay at a bot's feet, and a floor jumps four to eight
	// seconds after its last monster falls: "sporo dropu zostaje na ziemi"
	// (prodnathin). Between two foes a bot takes what it may within
	// LOOT_RANGE, while its health holds LOOT_MIN_HP_PERCENT.
	const int PLAYERBOT_TOWER_LOOT_RANGE = 1500;
	const int PLAYERBOT_TOWER_LOOT_MIN_HP_PERCENT = 50;
	// A raid is a guild and not a party, so the party buffs never reached it:
	// the tower's Shaman keeps its fellows' buffs up itself, one cast a pass.
	const DWORD PLAYERBOT_TOWER_ALLY_BUFF_INTERVAL = 3000;
	// metin2_map_deviltower1's base in cells (Setting.txt), the ground
	// floor's entrance the quest warps a player to, and the Metin of
	// Toughness's spawn point (regen.txt: cell 195,690 off the base).
	const long PLAYERBOT_TOWER_BASE_CELL_X = 1280;
	const long PLAYERBOT_TOWER_BASE_CELL_Y = 7936;
	const long PLAYERBOT_TOWER_PARTER_CELL_X = 1397;
	const long PLAYERBOT_TOWER_PARTER_CELL_Y = 8550;
	const long PLAYERBOT_TOWER_STONE_X = 147500;
	const long PLAYERBOT_TOWER_STONE_Y = 862600;
	// The floors' actors, from deviltower_zone.quest: the stone that spawns
	// the seven of the fourth floor, the Metins of Death of the seventh,
	// the Opening Stone for the five Ancient Seals, the Unknown Old Chest
	// and the Map of the Tower, the Bong-In keys for Sa-Soe, the smiths.
	const DWORD PLAYERBOT_TOWER_STONE_FLOOR4 = 8016;
	const DWORD PLAYERBOT_TOWER_STONE_FLOOR7 = 8018;
	const DWORD PLAYERBOT_TOWER_OPENING_STONE = 50084;
	const DWORD PLAYERBOT_TOWER_CHEST_ITEM = 30300;
	const DWORD PLAYERBOT_TOWER_MAP_ITEM = 30302;
	const DWORD PLAYERBOT_TOWER_FAKE_KEY = 30303;
	const DWORD PLAYERBOT_TOWER_KEY_ITEM = 30304;
	const DWORD PLAYERBOT_TOWER_NPC_SEAL = 20073;
	const DWORD PLAYERBOT_TOWER_NPC_SMITH_FIRST = 20074;
	const DWORD PLAYERBOT_TOWER_NPC_SMITH_LAST = 20076;
	const DWORD PLAYERBOT_TOWER_NPC_SASOE = 20366;
	// "Aktywuj teraz" from the panel: the file's mtime is the request.
	const char* const PLAYERBOT_TOWER_NOW_PATH = "/opt/m2spool/playerbot_tower_now";
	// The ItemShop (playerbot_itemshop.h, the 2.x line only): a bot looks at
	// its vouchers and its wishes every ten minutes, buys at most once an
	// hour, and reads its account's balance back once an hour, because that
	// read is a synchronous query. The catalogue is rebuilt hourly from the
	// manager's table, whose indices run to a few hundred on this package.
	const DWORD PLAYERBOT_ISHOP_CHECK_INTERVAL = 10 * 60 * 1000;
	const DWORD PLAYERBOT_ISHOP_BUY_INTERVAL = 60 * 60 * 1000;
	const DWORD PLAYERBOT_ISHOP_BALANCE_INTERVAL = 60 * 60 * 1000;
	const DWORD PLAYERBOT_ISHOP_CATALOGUE_INTERVAL = 60 * 60 * 1000;
	const DWORD PLAYERBOT_ISHOP_CENSUS_INTERVAL = 10 * 60 * 1000;
	const int PLAYERBOT_ISHOP_MAX_INDEX = 2000;
	// Kupon SM 50/100/500/1000/250 (80017/80014/80015/80016/80018).
	const DWORD PLAYERBOT_ISHOP_VOUCHER_MIN_VNUM = 80014;
	const DWORD PLAYERBOT_ISHOP_VOUCHER_MAX_VNUM = 80018;
	// What is bought with Dragon Marks: the shop's Blessing Scroll (25041,
	// a plain tuning scroll on this package like 25040) and the Dragon
	// God's attack potions, five to a line.
	const DWORD PLAYERBOT_ISHOP_BLESSING_SCROLL_VNUM = 25041;
	const DWORD PLAYERBOT_ISHOP_ATTACK_POTION_VNUM = 71028;
	// One bot in this many buys a hairstyle, once, from this level.
	const DWORD PLAYERBOT_ISHOP_HAIR_SHARE = 4;
	const BYTE PLAYERBOT_ISHOP_HAIR_MIN_LEVEL = 30;
	// And one keeper in PLAYERBOT_ISHOP_HAIR_TRADE_SHARE buys a head it cannot
	// wear, for its counter, when its coins are wanted for nothing of its own:
	// the item shop's hairstyles are what a player should find on a counter,
	// where the dyes from the water stood (Tieru, 18 September). One at a
	// time, bag and counter together. No hairstyle on this package carries a
	// bonus - every applytype of the 96 in the shop is zero - so it is the
	// look that is for sale.
	const DWORD PLAYERBOT_ISHOP_HAIR_TRADE_SHARE = 3;
	// Asked like one of Iwakura's prices (ScalePlayerBotIwakuraPrice): the shop
	// sells every head for 39 Dragon Coins, and a bot finds a fifty-coin
	// voucher about once a month at the default permilles.
	const DWORD PLAYERBOT_PRIOR_ISHOP_HAIRSTYLE = 2000000;
	const int PLAYERBOT_SHOP_ISHOP_HAIR_SCORE = 950;

	// How many acquaintances a bot keeps, and how much any one of them can be
	// worth. Small on purpose: this is looked at on every party check, and a bot
	// that has hunted with two hundred others should remember the handful it got
	// on with rather than all of them.
	const size_t PLAYERBOT_FRIEND_SLOTS = 8;
	const int PLAYERBOT_FRIEND_MAX_AFFINITY = 100;
	const int PLAYERBOT_FRIEND_PARTY_POINTS = 4;
	const int PLAYERBOT_FRIEND_GIFT_POINTS = 10;
	const int PLAYERBOT_FRIEND_TRADE_POINTS = 6;

	const int PLAYERBOT_BONUS_KEEP_SCORE = 240;
	// MAX_NORM_ATTR_NUM in item_manager.h. Named here because the loop that fills
	// an item has to know it, and reading it from the engine header would tie a
	// tuning constant to a build detail.
	// Four by the stone - the engine's USE_ADD_ATTRIBUTE refuses a fifth - and
	// the fifth only the way a player gets it: a Marmur Blogoslawienstwa
	// (USE_ADD_ATTRIBUTE2) on a piece of exactly four, at its own odds. The
	// bots used to call AddAttribute() straight, no odds and up to five
	// ("boty dodaja sobie 5 bonusow", 12 September).
	const int PLAYERBOT_BONUS_MAX_LINES = 4;
	const int PLAYERBOT_BONUS_MARBLE_LINES = 5;
	// Iwakura's QUICK FIX nr 3 (23 September): "Boty moga uzywac zmianek
	// wylacznie na przedmiotach, ktore posiadaja juz co najmniej 3 dodane
	// bonusy (z priorytetem dobicia do pelnych 4 bonusow przed rozpoczeciem
	// mieszania)". A change stone waits for this many lines, and a piece of
	// three takes an add stone first whenever there is one it can use.
	const int PLAYERBOT_BONUS_CHANGE_MIN_LINES = 3;
	// What the lines rolled on a piece add to what a stall asks for it.
	//
	// A counter wanted the same 150 000 for boots +7 carrying five bonus lines
	// as for boots +7 carrying none, which is not a market: everything that set
	// the price - the merchant's table, the sale memory, the step limiter - is
	// keyed by vnum and refine, and that pair cannot tell the two apart.
	//
	// Two things carry it. How many lines there are, with a step at four
	// because that is where a piece stops being a drop and starts being
	// somebody's work; and whether any of them is a roll a player stops on -
	// two thousand health, ten percent critical, immunity to stun. The cap is
	// there because the buyers are bots with an hour's hunting in their pocket.
	const int PLAYERBOT_SHOP_BONUS_PER_LINE = 25;
	const int PLAYERBOT_SHOP_BONUS_FOUR_PLUS = 100;
	const int PLAYERBOT_SHOP_BONUS_TOP_LINE = 80;
	const int PLAYERBOT_SHOP_BONUS_MAX_PERCENT = 600;
	// The two lines that make a level-30 weapon the one everybody is looking
	// for, and the step they add on top of the ordinary line premium. Proposed
	// from the Discord in exactly these numbers - "average damage at least 24%,
	// or skill damage 15%+" - and they match what this world actually rolls:
	// average damage goes to 46 and skill damage to 18, so 24 and 15 are the
	// upper half of each. Only on the level-30 set; on other gear a good line
	// is still just a top line.
	const int PLAYERBOT_PRIZE_AVERAGE_DAMAGE = 24;
	const int PLAYERBOT_PRIZE_SKILL_DAMAGE = 15;
	const int PLAYERBOT_SHOP_BONUS_PRIZE_LINE = 300;
	// The rolls that finish an item for its slot. Thirty percent average damage
	// on a level-30 weapon, fifteen hundred health on armour or jewellery, five
	// percent critical on jewellery - the numbers a player stops rerolling at.
	// Twenty, not thirty: "jesli maja srednie nizsze niz 20% to niech mixuja
	// az im sie uda" - and thirty is a roll most weapons never see, so the
	// rerolling never stopped where a player would have stopped it.
	const long PLAYERBOT_BONUS_KEEP_AVERAGE = 20;
	// A hand-tuned weapon at the two tiers players care about (level 30 and
	// 75) is finished the moment it carries an average-damage or average-
	// skill line at or above this - USE_CHANGE_ATTRIBUTE never touches it
	// again. "dalem botowi fms z navi wartosci po 1000, debil zmienil bonusy"
	// (Ciapek, 13 September). And a change stone is never spent on a +0..+4
	// piece: raise it first, mix later.
	const long PLAYERBOT_BONUS_WEAPON_LOCK_PCT = 25;
	// A weapon with a skill-damage line above this is a PvP prize and is never
	// rerolled away, whatever the class - not only a caster's. The equip pass
	// values average damage for PvE (see the prize in playerbot_gear.h), but a
	// big skill line is a nice PvP bonus this world will use once PvP ships, and
	// "szkoda tracic takiego ladnego bonusu do PvP" (Tieru): the bot keeps such a
	// weapon, or sells it whole on an offline counter, rather than mixing it off.
	const long PLAYERBOT_BONUS_SKILL_PVP_PCT = 21;
	const BYTE PLAYERBOT_BONUS_CHANGE_MIN_REFINE = 5;
	// Community Patch 1 (Iwakura, 20 September): "na wczesnym etapie gry
	// (przed 45. poziomem) bonusy w bransoletach, naszyjnikach oraz butach sa
	// znacznie wazniejsze niz stopien ulepszenia tych przedmiotow ... nawet
	// jesli sa to przedmioty bazowo najslabsze i bez wzgledu na poziom ich
	// ulepszenia". So under this level those three slots are worked on first,
	// the change stone's +5 floor does not apply to them, and the lines he
	// names for each are worth half as much again as the table alone says.
	const BYTE PLAYERBOT_EARLY_BONUS_MAX_LEVEL = 45;
	// Iwakura's community patch 2, point 3. A weapon is finished at this
	// average ("30%+ SR"), and under PLAYERBOT_EARLY_BONUS_MAX_LEVEL it gets no
	// stone until this many of the boots, the necklace and the bracelet carry
	// a health line.
	const long PLAYERBOT_BONUS_WEAPON_TARGET_AVERAGE = 30;
	const int PLAYERBOT_EARLY_HP_PIECES_FOR_WEAPON = 2;
	const int PLAYERBOT_EARLY_BONUS_PERCENT = 150;
	const long PLAYERBOT_BONUS_KEEP_HP = 1500;
	const long PLAYERBOT_BONUS_KEEP_CRIT = 5;
	// The caster's half of the same rule, and it exists because the two damage
	// lines are one roll rather than two. item_addon.cpp draws the skill line
	// from a gaussian of sigma five and then sets the average line to minus
	// twice it plus a little noise, so no weapon can carry both: +18% skill is
	// -29% average on the same item. Eight is where the skill side is about as
	// rare as twenty is on the average side - one reroll in eighteen - so a
	// Shaman stops on a Warrior's odds instead of rerolling for ever.
	const long PLAYERBOT_BONUS_KEEP_SKILL = 8;
	// The rest of the finishing rolls, one per slot, each of them the fourth
	// tier of what `player.item_attr` lets that line reach: block and dodge go
	// to fifteen, attack speed to eight, movement speed and item drop to twenty,
	// stolen life to ten, and a race line to twenty (ten on human). Stopping at
	// the fifth tier would mean stopping almost never.
	const long PLAYERBOT_BONUS_KEEP_BLOCK = 10;
	const long PLAYERBOT_BONUS_KEEP_DODGE = 10;
	const long PLAYERBOT_BONUS_KEEP_ATT_SPEED = 5;
	const long PLAYERBOT_BONUS_KEEP_MOV = 10;
	const long PLAYERBOT_BONUS_KEEP_DROP = 8;
	const long PLAYERBOT_BONUS_KEEP_STEAL = 5;
	const long PLAYERBOT_BONUS_KEEP_RACE = 10;
	// What "silny przeciwko X" is worth per point, scaled by how much of the map
	// that race actually is, and what it is worth on a map that is something
	// else. Measured by tools/analyse_map_races.py over every map a bot may
	// stand on: Orc Valley is 63% orcs, all three second villages are 100%
	// human, the first villages 77% animal, the guild maps and all five Monkey
	// Dungeons 100% animal, Mount Sohan 46% undead, Hwang 68% mystic - and the
	// Yongbi Desert and both Spider Dungeons are made of DESERT, INSECT and ICE,
	// races char.cpp maps no APPLY onto, so on those three no race line can ever
	// do anything at all. The second number is not zero only because a bot
	// changes maps.
	const int PLAYERBOT_BONUS_RACE_ON_MAP = 16;
	// The same line in the equipment score, which counts in the thousands
	// because a point of defence does. Scaled by the same share, so a piece is
	// not bought for a line the reroll pass will then throw away.
	const int PLAYERBOT_GEAR_RACE_LINE_VALUE = 600;
	const int PLAYERBOT_BONUS_RACE_OFF_MAP = 2;
	const int PLAYERBOT_BONUS_STONES_PER_VISIT = 3;
	// Effectively once per town visit. A four-second cadence like the refiner's
	// would let one stop at the blacksmith burn a quarter of a million yang.
	const DWORD PLAYERBOT_BONUS_INTERVAL = 300000;
	const DWORD PLAYERBOT_INACTIVITY_RESET_TIME = 90000;
	const DWORD PLAYERBOT_WANDER_INTERVAL = 8000;
	const DWORD PLAYERBOT_PARTY_CHECK_INTERVAL = 10000;
	// Running with the player who invited you.
	//
	// A bot in a party of its own keeps station by the straggler radius and
	// leaves the party when it cannot; a bot in a PLAYER's party has to do the
	// opposite - stay, and walk after them. The distance is under the party
	// cohesion radius so the bot closes up before the leader is out of range of
	// anything shared, and the pass runs on its own short clock rather than the
	// party pass's ten seconds, because following at ten-second granularity is
	// a bot that is always a screen behind.
	const int PLAYERBOT_PARTY_FOLLOW_DISTANCE = 1500;
	// How far from the person a bot in a person's party may pick its monsters
	// (IsPlayerBotTargetOffHumanLeader): the search reaches
	// PLAYERBOT_SEARCH_RANGE from the bot itself, and a bot that always had its
	// next monster in reach never stood idle long enough to be walked back.
	const int PLAYERBOT_PARTY_HUMAN_HUNT_RANGE = 2500;
	const DWORD PLAYERBOT_PARTY_FOLLOW_INTERVAL = 2000;
	// A bot that could not follow its player onto another map tries again this
	// much later; TransitionPlayerBotMap already says why, once a minute.
	const DWORD PLAYERBOT_PARTY_WARP_FOLLOW_RETRY = 10000;
	// Map indexes from here up are dungeon instances - the map's own index
	// times ten thousand plus a serial, a copy made for one party - with no
	// navigation grid a bot could plan on and no way out the AI knows.
	const long PLAYERBOT_INSTANCE_MAP_INDEX_MIN = 10000;
	// How often a Shaman in a player's party looks at the player's buffs, and
	// the health under which it heals the player instead.
	const DWORD PLAYERBOT_PARTY_LEADER_BUFF_INTERVAL = 3000;
	const int PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT = 60;
	// A duel: three seconds between the challenge and the first blow, because
	// that is what the operator asked for and because agreeing on the same tick
	// reads like a script rather than an opponent.
	const DWORD PLAYERBOT_PVP_ACCEPT_DELAY = 3000;
	// How long the engine may refuse a duellist its blow before the bot takes
	// the duel as over: comfortably past the agreement above even on a busy
	// tick, well short of the bound below. See ManagePlayerBotDuelCombat.
	const DWORD PLAYERBOT_PVP_REFUSED_GIVE_UP = 15000;
	// How a duel is fought, as against a hunt (ManagePlayerBotDuelCombat): a
	// blade swings from where it reaches, a caster casts from further off, a
	// warrior charges a foe standing between the two, the aura goes up inside
	// the buff range, and the rotation runs on a shorter clock - a duel lasts
	// twenty seconds, and the hunt's pause between casts left room for one
	// skill in it (Tieru, 15 September: swords waved from afar, Trzystronne
	// Ciecie under no aura, no Szarza and no Wir Miecza).
	const int PLAYERBOT_DUEL_MELEE_RANGE = 170;
	const int PLAYERBOT_DUEL_CASTER_RANGE = 600;
	const int PLAYERBOT_DUEL_CHARGE_MIN_RANGE = 250;
	const int PLAYERBOT_DUEL_CHARGE_RANGE = 600;
	const int PLAYERBOT_DUEL_BUFF_RANGE = 1500;
	const DWORD PLAYERBOT_DUEL_SKILL_INTERVAL = 1800;
	const DWORD PLAYERBOT_DUEL_SHAMAN_SKILL_INTERVAL = 3000;
	// Poison is a boss's bane. poison_event takes GetPoisonDamageRate per mille
	// of the victim's maximum health ten times, three seconds apart, and the
	// rate is 25 for MOB_RANK_BOSS: a quarter of the Orc Chief's, Nine Tails',
	// the Spider Queen's or the Yellow Tiger Spectre's health for one proc -
	// none of the four is immune, and the engine's IsImmune(IMMUNE_POISON) test
	// is commented out anyway. For a king (the Spider Baroness, the Elite Queen)
	// it is 1. So the line is worth twice as much from the level the boss hubs
	// begin at, and no more ("przyda im sie w ekwipunku tez bonus szansa na
	// otrucie", Tieru, 15 September).
	const int PLAYERBOT_POISON_BOSS_LEVEL = 50;
	// How long the bot assumes an agreed duel lasts. The engine knows exactly
	// (CPVPManager), but its IsFighting sits behind ENABLE_NEWSTUFF on one line
	// and does not exist at all on the other, so the bot remembers instead. Only
	// the health-potion ban hangs on this, and a duel that is over costs nothing
	// but a few minutes of a bot not drinking while it is at full health anyway.
	const DWORD PLAYERBOT_PVP_DUEL_ASSUMED = 180000;
	// Bots challenging each other: rare, because a duel is a thing that happens
	// in a world, not the thing the world does. One roll a minute per bot.
	const DWORD PLAYERBOT_PVP_CHALLENGE_INTERVAL = 60000;
	const int PLAYERBOT_PVP_CHALLENGE_PER_MILLE = 6;
	const int PLAYERBOT_PVP_CHALLENGE_RANGE = 1200;
	const int PLAYERBOT_PVP_CHALLENGE_LEVEL_DELTA = 5;
	// Nobody starts a fight on a sliver of health, and nobody finishes one
	// without being able to walk away from it.
	const int PLAYERBOT_PVP_MIN_HP_PERCENT = 80;
	const int PLAYERBOT_PARTY_DESIRED_MAX = 6;
	const int PLAYERBOT_PARTY_COHESION_RADIUS = 2800;
	// Three, not five. A course needs somebody to pull for, and the operator's
	// rule is "aggro 1-3 party bots waiting nearer the middle" - so three is
	// what a pull is actually for. Five was a guess, and it was a guess that
	// switched the whole role off: measured on our own world, parties run at
	// one or two members (census: 18 bots in 15 parties), five-member ones
	// essentially never form, and the last lure session in the logs was two
	// days old and ended "no_pack".
	const int PLAYERBOT_ARCHER_LURE_MIN_PARTY_MEMBERS = 3;
	// The Archer's luring course, as a party role rather than an extra shot.
	//
	// A course is: walk out, tag a pack with one ordinary arrow, read whether it
	// actually came, and bring what came back to the people who can kill it.
	// Every number below bounds a real failure - an Archer that gathers for
	// ever, one that runs further than monsters will follow, one that arrives at
	// a party which has moved on - and none of them is a measured optimum yet.
	//
	// How far a receiver may be from the gathering point and still count as
	// ready. Wider than this and the party is not standing together at all.
	const int PLAYERBOT_LURE_ANCHOR_RADIUS = 2200;
	// Close enough to the receivers to call the monsters delivered.
	const int PLAYERBOT_LURE_HANDOFF_RANGE = 450;
	// How far a course may take the Archer from the gathering point. Beyond it
	// the monsters break off and walk home, which is a sprint for nothing.
	const int PLAYERBOT_LURE_MAX_COURSE_RANGE = 4500;
	// A bow's reach is the one the ordinary attack uses, less a margin for the
	// step the bot takes while the shot is being sent. A second definition of
	// range is how a lure comes to fire from where a fight could not.
	const int PLAYERBOT_LURE_SHOT_RANGE = 760;
	const int PLAYERBOT_LURE_START_HP_PERCENT = 90;
	const int PLAYERBOT_LURE_BREAK_HP_PERCENT = 70;
	const int PLAYERBOT_LURE_MAX_HP_LOSS_PERCENT = 12;
	// Gathering has a deadline, and so has the walk back: a course that stopped
	// making progress must end as a course, not as a bot standing in a field.
	const DWORD PLAYERBOT_LURE_GATHER_TIME = 12000;
	const DWORD PLAYERBOT_LURE_RETURN_TIME = 25000;
	// After the arrow: long enough for a pack to turn round, short enough that
	// one that is not coming does not cost the whole course.
	const DWORD PLAYERBOT_LURE_CONFIRM_DELAY = 1200;
	const DWORD PLAYERBOT_LURE_CONFIRM_TIMEOUT = 4500;
	// How long the Archer stands with the party before the handover is judged.
	const DWORD PLAYERBOT_LURE_HANDOFF_WAIT = 7000;
	// A session that outlives this is abandoned whatever stage it is in, so no
	// party is ever held by a lurer that stopped answering.
	const DWORD PLAYERBOT_LURE_SESSION_TTL = 75000;
	const DWORD PLAYERBOT_LURE_COOLDOWN_MIN = 20000;
	const DWORD PLAYERBOT_LURE_COOLDOWN_MAX = 50000;
	// Groups and monsters per course: what a first course asks for, and the
	// ceiling a party earns by finishing courses without losing anybody.
	const int PLAYERBOT_LURE_FIRST_GROUPS = 2;
	// Three, not four: the operator's rule for a pull is "aggro 1-3 party bots",
	// and a fourth group is a pack the waiting members cannot share out.
	const int PLAYERBOT_LURE_MAX_GROUPS = 3;
	const int PLAYERBOT_LURE_FIRST_BUDGET = 7;
	const int PLAYERBOT_LURE_MAX_BUDGET = 14;
	// Courses in a row without a death or a failed handover before the plan
	// grows by one group.
	const int PLAYERBOT_LURE_GROWTH_STREAK = 3;
	// What still counts as "the party is busy": a new course does not start
	// while this many delivered monsters are still on the receivers.
	const int PLAYERBOT_LURE_BUSY_MONSTERS = 3;
	// How often an Archer that cannot start a course asks again. The busy
	// count is a sector scan, and one per tick per Archer is a real cost
	// for an answer that does not change that fast.
	const DWORD PLAYERBOT_LURE_READY_RECHECK = 2000;
	// Where a pack worth pulling stands. Not the multi-pull's band, which looks
	// for whatever is at a solo bot's feet: a lure is for the packs the party
	// has not reached, so it starts beyond bow range and beyond the ground the
	// party is already fighting over, and it never takes a monster somebody
	// else has claimed.
	const int PLAYERBOT_LURE_MIN_PACK_DISTANCE = 1100;
	// Four thousand, derived rather than guessed. Once the range counter was
	// split into its two halves the answer was one-sided: two courses on the
	// Spider Dungeon read too_close=0 too_far=33 and too_close=0 too_far=62, so
	// every pack the Archer refused was beyond the window, never inside it.
	// The bound that matters is PLAYERBOT_LURE_MAX_COURSE_RANGE - the Archer
	// walks out to the pack and drags it back to the anchor - so the window
	// stays under it with room for the return leg.
	const int PLAYERBOT_LURE_MAX_PACK_DISTANCE = 4000;
	const int PLAYERBOT_LURE_ANCHOR_CLEARANCE = 900;
	const int PLAYERBOT_LURE_GROUP_SEPARATION = 700;
	// Above this over the Archer's own level a pack is not brought home, it is
	// an escort of things that kill the Archer on the way.
	const int PLAYERBOT_LURE_MAX_LEVEL_OVER = 3;
	// Luring on a person's word ("luruj" in a whisper, "przestan lurowac" to
	// end it). The role's own numbers above are what a party of bots needs to
	// make the pull worth having; a player who asks for one by name has already
	// decided that, so the rules that exist to keep bots from luring for nobody
	// are the ones that give way here - and nothing else is.
	//
	// The pair is the party: the person who asked is the receiver, so nobody
	// else has to be standing there.
	const int PLAYERBOT_LURE_PLAYER_MIN_PARTY_MEMBERS = 2;
	// Between two courses on a standing order. The role's own 20-50 s is a
	// bot pacing itself; a person who asked for pulls is waiting for the next
	// one.
	const DWORD PLAYERBOT_LURE_PLAYER_COOLDOWN_MIN = 4000;
	const DWORD PLAYERBOT_LURE_PLAYER_COOLDOWN_MAX = 9000;
	// The bots' own handover is judged after seven seconds of standing there,
	// because nothing forced it and the question is whether the receivers took
	// the pack. On an order the pack is put on the person outright, so the only
	// thing left to wait for is the engine registering the new victims.
	const DWORD PLAYERBOT_LURE_PLAYER_HANDOFF_WAIT = 1500;
	// An order nobody cancels ends by itself, and the player is told. Long
	// enough for a hunting session, short enough that a bot is not luring for
	// somebody who logged out an hour ago and came back to something else.
	const DWORD PLAYERBOT_LURE_PLAYER_ORDER_TTL = 45u * 60u * 1000u;
	// How far from the person the bot may be before the order is treated as a
	// party that has drifted apart rather than a course in progress. It is the
	// course range plus the anchor radius: past that the two are not hunting
	// together at all.
	const int PLAYERBOT_LURE_PLAYER_MAX_SEPARATION = 7000;
	// A course opens at nine tenths of health for the bots' own role, where the
	// party stands and waits and an Archer at 89% has simply not finished
	// resting. Beside a person it is a gate that never opens: the bot takes
	// hits from whatever the person is fighting, and the run out and back is
	// what the health is actually for. Low enough to survive the return leg,
	// high enough not to set off with a pack on a bot that is about to die.
	const int PLAYERBOT_LURE_PLAYER_START_HP_PERCENT = 55;
	// What a person means by "luruj", measured against what the bots' own role
	// means by it. That role fetches a pack the party has not reached, so it
	// starts beyond bow range and clear of the ground the party is fighting
	// over; a person standing on a spot wants the monsters *round them*
	// gathered onto them, and every one of those three windows refused exactly
	// that - on l0st3k's screenshot of 20 September the bot stood beside him
	// with monsters a few hundred units away and the whole field was
	// "too_close" and "anchor". So on an order there is no minimum and no
	// clearance, the groups already taken only reserve the ground right round
	// them, and the plan is bigger because gathering is the job rather than
	// one trip.
	const int PLAYERBOT_LURE_PLAYER_MIN_PACK_DISTANCE = 0;
	const int PLAYERBOT_LURE_PLAYER_ANCHOR_CLEARANCE = 0;
	const int PLAYERBOT_LURE_PLAYER_GROUP_SEPARATION = 250;
	const int PLAYERBOT_LURE_PLAYER_GROUPS = 5;
	const int PLAYERBOT_LURE_PLAYER_BUDGET = 12;
	// And the level window is the person's, not the bot's. The bots' own role
	// judges by the Archer because the Archer's party will fight what it
	// brings; on an order the person fights it, and a level-18 companion beside
	// a level-33 player refused every monster on the map for being eight levels
	// over *itself*. The bot only has to survive the walk back, which is what
	// the health gate and the leash are for.
	const int PLAYERBOT_LURE_PLAYER_MAX_LEVEL_OVER = 3;
	// And the two that end a gathering, which have to sit under the one that
	// opens it. 2.0.90 dropped the opening gate to 55% for an order and left
	// the break at the bots' own 70%, so a bot between the two opened a course
	// and ended it "low_hp" on the same tick, every few seconds, for as long as
	// the order stood. The loss window is wider for the same reason the level
	// window is: gathering a spot means standing in it while the pack turns
	// round, and 12% is one hit.
	const int PLAYERBOT_LURE_PLAYER_BREAK_HP_PERCENT = 35;
	const int PLAYERBOT_LURE_PLAYER_MAX_HP_LOSS_PERCENT = 30;

	// Leaving a party the way the engine leaves one.
	//
	// `CParty::Quit` takes the member out and leaves the party standing, so a
	// party of two that loses one is a leader alone in a party of one -
	// `GetParty()` still answers, `ManagePlayerBotParty` returns on the first
	// line of its "already in a party" branch, and that bot never looks for
	// another partner as long as it lives. `CInputMain`'s own handler never
	// allows it: with two members, or when the leader is the one leaving, it
	// calls `DeleteParty` instead, which is why a player can never be in a
	// party of one. Measured on the test world before this: 78 bots in parties
	// against 76 distinct leaders, four parties made in forty-five minutes and
	// every one of them decayed within two minutes on the straggler radius -
	// and the Archer's lure role, which needs three in a party, had therefore
	// not run once in five hours.
	bool LeavePlayerBotParty(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPPARTY party = ch->GetParty();
		if (!party)
			return false;
		if (party->GetMemberCount() <= 2 || party->GetLeaderPID() == ch->GetPlayerID())
			CPartyManager::instance().DeleteParty(party);
		else
			party->Quit(ch->GetPlayerID());
		return true;
	}

	const int PLAYERBOT_PARTY_CHALLENGE_MIN_MEMBERS = 3;
	const int PLAYERBOT_PARTY_CHALLENGE_RADIUS = 3000;
	const int PLAYERBOT_PARTY_READY_HP_PERCENT = 55;
	const int PLAYERBOT_PARTY_LEVEL_BONUS_PER_MEMBER = 5;
	// Strong solo builds sometimes play like an experienced Metin2 tank: wake a
	// few separate packs, bring them together and then clear them with the normal
	// melee splash. The limits deliberately favour survival over maximum XP.
	const DWORD PLAYERBOT_MULTI_PULL_MIN_COOLDOWN = 45000;
	const DWORD PLAYERBOT_MULTI_PULL_MAX_COOLDOWN = 90000;
	const DWORD PLAYERBOT_MULTI_PULL_TIMEOUT = 12000;
	const DWORD PLAYERBOT_MULTI_PULL_ACTION_DELAY = 500;
	const int PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT = 70;
	const int PLAYERBOT_MULTI_PULL_START_HP_PERCENT = 90;
	const int PLAYERBOT_MULTI_PULL_MAX_HP_LOSS_PERCENT = 12;
	const int PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS = 14;
	const int PLAYERBOT_MULTI_PULL_SEARCH_RANGE = 2200;
	const int PLAYERBOT_MULTI_PULL_GROUP_SEPARATION = 600;
	const DWORD PLAYERBOT_MERCHANT_WAIT_MIN = 3000;
	const DWORD PLAYERBOT_MERCHANT_WAIT_MAX = 15000;
	const DWORD PLAYERBOT_BLACKSMITH_WAIT_MIN = 6000;
	const DWORD PLAYERBOT_BLACKSMITH_WAIT_MAX = 24000;
	const DWORD PLAYERBOT_TRAINER_WAIT_MIN = 8000;
	const DWORD PLAYERBOT_TRAINER_WAIT_MAX = 18000;
	// The user-measured gate centre is 603,675 => (60300,169900).  Approach it
	// perpendicularly through two safe points instead of pathing diagonally into
	// either gate pillar.
	const long PLAYERBOT_TOWN_GATE_X = 60300;
	const long PLAYERBOT_TOWN_GATE_OUTSIDE_Y = 169400;
	const long PLAYERBOT_TOWN_GATE_INSIDE_Y = 170400;
	const long PLAYERBOT_MISC_MERCHANT_X = 59000;
	const long PLAYERBOT_MISC_MERCHANT_Y = 171300;
	const long PLAYERBOT_BLACKSMITH_X = 59400;
	const long PLAYERBOT_BLACKSMITH_Y = 171600;
	const long PLAYERBOT_WEAPON_MERCHANT_X = 67600;
	const long PLAYERBOT_WEAPON_MERCHANT_Y = 168600;
	const long PLAYERBOT_ARMOR_MERCHANT_X = 67600;
	const long PLAYERBOT_ARMOR_MERCHANT_Y = 164100;
	const long PLAYERBOT_BIOLOGIST_X = 89800;
	const long PLAYERBOT_BIOLOGIST_Y = 182100;
	const long PLAYERBOT_STABLE_BOY_X = 54900;
	const long PLAYERBOT_STABLE_BOY_Y = 163400;
	// How close to the stable keeper's approach point the walk has to end, and
	// the goal snap that stays inside it: a snap wider than the arrival test
	// is a bot that walks its route, arrives at nothing and plans the same
	// route again - the town leg and the portal walk both sprang this.
	const int PLAYERBOT_STABLE_ARRIVE_DISTANCE = 650;
	const int PLAYERBOT_STABLE_SNAP_CELLS = PLAYERBOT_STABLE_ARRIVE_DISTANCE / 100;
	// Verified against locale/english/map/{index,Setting.txt,npc.txt,Town.txt} and
	// share/conf/mob_names.txt. Chunjo uses the empire-specific easy monkey
	// dungeon (map 25); map 107 is a different global dungeon whose coordinates
	// do not match the Bokjung portal target.
	// --- Earning the battle horse ------------------------------------------
	// The stable keeper's quest, with the three things this world cannot
	// support taken out - see playerbot_battle_horse.h for which and why.
	const BYTE PLAYERBOT_BATTLE_HORSE_MIN_LEVEL = 35;
	const BYTE PLAYERBOT_BATTLE_HORSE_FROM_HORSE_LEVEL = 10;
	const int PLAYERBOT_BATTLE_HORSE_KILLS = 100;
	const DWORD PLAYERBOT_BATTLE_HORSE_FEE = 500000;
	// The two archers the stable keeper's quest names, and the wiki with it:
	// Skorpion Lucznik (2105, level 47) and Wezowy Lucznik (2107, level 51).
	// Both stand on the desert, 998 and 760 spawn points of
	// metin2_map_n_desert_01 - the very map the quest sends a player to.
	//
	// Until 2.0.31 this was "the Black Wind band, 401 to 404", on the written
	// claim that 2105 and 2107 were spawned nowhere in this world. Both halves
	// were wrong and one mistake made both: the desert's regen.txt is `r` lines
	// whose last field is a group_group id, so 401-404 are group ids and not
	// monster vnums - the exact field CLAUDE.md warns about. Resolved through
	// the global group_group.txt and group.txt, the desert really does carry
	// 2105 and 2107, while vnums 401-404 are the Black Wind band, which lives
	// on the three second villages (a3/b3/c3) and never sets foot in the
	// desert. So the trial counted kills of monsters no bot on it could ever
	// meet, and every bot read "Zdobywam konia bojowego na pustyni (0/100)"
	// for ever (sosen, 13 September). Measure a spawn table by resolving its
	// groups; never by grepping a number out of regen.txt.
	const DWORD PLAYERBOT_BATTLE_HORSE_MOB_SCORPION_ARCHER = 2105;
	const DWORD PLAYERBOT_BATTLE_HORSE_MOB_SNAKE_ARCHER = 2107;
	// "Zdjecie Konia", taken away, and "Ksiega Opanc. Konia", handed over.
	const DWORD PLAYERBOT_HORSE_PHOTO_VNUM = 50051;
	const DWORD PLAYERBOT_BATTLE_HORSE_BOOK_VNUM = 50052;
	const char* PLAYERBOT_BATTLE_HORSE_KILLS_FLAG = "playerbot.battle_horse_kills";
	// The military horse, the step after the combat one.
	//
	// The operator's shape for it: medals carry the horse to twenty, and the
	// twenty-first level is a trial in the Demon Tower rather than one more
	// medal - with no clock on it, like the desert trial. That is why the map
	// had to move onto game1 at all: 1001-1004 stand nowhere else, and a trial
	// on a map a bot cannot reach is a horse that stops at twenty for ever.
	//
	// Fifty kills against the desert trial's hundred, because a Demon Soldier of
	// fifty-seven is not a Scorpion Archer of thirty-nine and the bot doing this
	// is level fifty by the milestone above.
	const char* PLAYERBOT_MILITARY_HORSE_KILLS_FLAG = "playerbot.military_horse_kills";
	const int PLAYERBOT_MILITARY_HORSE_KILLS = 50;
	const BYTE PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL = 20;
	// The character level the trial asks for. The same number
	// GetPlayerBotNextHorseRequiredLevel already returns for a horse at twenty -
	// named here so the two cannot drift apart.
	const BYTE PLAYERBOT_MILITARY_HORSE_MIN_LEVEL = 50;
	const BYTE PLAYERBOT_MILITARY_HORSE_LEVEL = 21;
	const DWORD PLAYERBOT_MILITARY_HORSE_MOBS[] = { 1001, 1002, 1003, 1004 };

	// The level at which a horse stops being transport and becomes a weapon.
	// Below it a bot always dismounts to fight; at or above it the target
	// decides.
	const BYTE PLAYERBOT_BATTLE_HORSE_LEVEL = 11;
	// And the level of an attack skill at which the saddle stops being worth
	// it. The engine lets a rider cast nothing of its class from any horse:
	// CHARACTER::UseSkill refuses every skill but Sprint below a military
	// horse, and every one but the four horse skills on one, which no bot
	// has. So a warrior or a sura fighting from a battle horse swung its
	// weapon and did nothing else, aura and berserk included ("sura bez
	// skilli na koniu se expi", prodnathin, 23 September). From one attack
	// skill at Master a bot is stronger on foot and fights there; the horse
	// still carries it between fights (PlayerBotSkillsBeatTheSaddle).
	const int PLAYERBOT_SADDLE_SKILL_LEVEL = 20;

	const long PLAYERBOT_MAP_CHUNJO_M1 = 21;
	// Joan's inner town is walled: the misc merchant and the blacksmith stand
	// behind the gate at PLAYERBOT_TOWN_GATE_*, and the town visit walks that
	// gate as a leg of its own (GATE_IN / GATE_OUT). Yongan and Pyongmoo keep
	// the same eight services in open ground - measured on the mt2009
	// server_attr of maps 1 and 41: the blacksmith and the misc merchant sit in
	// the weapon merchant's own walkable component - so they take the direct
	// phases a second village takes. Until 2.0.8 every first village walked
	// Joan's gate coordinates, which on maps 1 and 41 are nowhere, so no
	// Shinsoo or Jinno bot ever reached its blacksmith or misc merchant
	// ("tylko boty z Chunjo ulepszaja ekwipunek", nerrvous_s). Named by map on
	// purpose: this is one town's wall, not a kingdom's shape.
	inline bool IsPlayerBotGatedVillage(long mapIndex)
	{
		return mapIndex == PLAYERBOT_MAP_CHUNJO_M1;
	}
	const long PLAYERBOT_MAP_CHUNJO_M2 = 23;
	const long PLAYERBOT_MAP_CHUNJO_M3 = 24;
	// Chunjo's. The other two kingdoms' easy dungeons are 5 and 45; nothing may
	// name one of them where it means "the easy dungeon" - ask
	// playerbot_empire_rules::GetMonkeyEasyMap(empire) for a bot's own.
	const long PLAYERBOT_MAP_MONKEY_EASY = 25;
	const long PLAYERBOT_MAP_MONKEY_SHINSOO = 5;
	const long PLAYERBOT_MAP_MONKEY_JINNO = 45;

	// Chunjo's four maps keep their names because a thousand lines were written
	// against them, but they are one kingdom of three now and nothing may test
	// a village by its index any more. A rule about "the first village" is a
	// rule about MAP_ROLE_M1, and it has to answer for Shinsoo and Jinno too -
	// a Jinno bot walking to Joan's blacksmith because 21 was written into the
	// town visit is the whole reason this file grew these six questions.
	bool IsPlayerBotM1Map(long mapIndex)
	{
		return playerbot_empire_rules::GetMapRole(mapIndex) ==
				playerbot_empire_rules::MAP_ROLE_M1;
	}

	bool IsPlayerBotM2Map(long mapIndex)
	{
		return playerbot_empire_rules::GetMapRole(mapIndex) ==
				playerbot_empire_rules::MAP_ROLE_M2;
	}

	bool IsPlayerBotM3Map(long mapIndex)
	{
		return playerbot_empire_rules::GetMapRole(mapIndex) ==
				playerbot_empire_rules::MAP_ROLE_M3;
	}

	// Any village: the six maps that have merchants, a blacksmith and a
	// Teleporter. This is the guard a town visit wants.
	bool IsPlayerBotVillageMap(long mapIndex)
	{
		return IsPlayerBotM1Map(mapIndex) || IsPlayerBotM2Map(mapIndex);
	}

	// The map of `role` in the bot's OWN kingdom. The character's empire is the
	// truth here and the map under its feet is not: a Jinno bot standing in
	// Bokjung is a visitor, and sending it "home to M1" means Jinno's M1.
	long GetPlayerBotHomeMap(LPCHARACTER ch, playerbot_empire_rules::EMapRole role)
	{
		return ch ? playerbot_empire_rules::GetHomeMap((int)ch->GetEmpire(), role) : 0;
	}

	// Whether two maps belong to the same kingdom, which is what says a walk
	// from one to the other is a local errand rather than a journey abroad.
	bool IsPlayerBotSameKingdom(long a, long b)
	{
		const int ea = playerbot_empire_rules::GetMapOwnerEmpire(a);
		return ea != 0 && ea == playerbot_empire_rules::GetMapOwnerEmpire(b);
	}
	const long PLAYERBOT_MAP_MONKEY_MEDIUM = 108;
	const long PLAYERBOT_MAP_MONKEY_HARD = 109;
	const long PLAYERBOT_M1_TO_M2_PORTAL_X = 87600;
	const long PLAYERBOT_M1_TO_M2_PORTAL_Y = 215100;
	const long PLAYERBOT_M2_ARRIVAL_X = 111800;
	const long PLAYERBOT_M2_ARRIVAL_Y = 216100;
	// How long a bot may stand at a portal without getting any closer to it
	// before travel gives the tick back. Twenty seconds is far longer than any
	// replan takes and far shorter than the hours four bots spent frozen at the
	// Bokjung teleporter.
	// How long a closed stall keeps taking its sign back, and how often. A stall
	// sign is cleared with PacketAround, which reaches whoever is in view at that
	// instant and nobody else - so a player who walks up a second later sees a
	// bot wearing a shop that no longer exists. Six seconds of repeats covers the
	// approach without turning into chatter: seventy closures in twenty-five
	// minutes across the whole world is what this is spread over.
	const DWORD PLAYERBOT_SHOP_SIGN_CLEAR_WINDOW = 6000;
	const DWORD PLAYERBOT_SHOP_SIGN_CLEAR_INTERVAL = 1500;

	// The material errand. A bot short of a refine material scans its map for
	// the nearest living monster whose DROP_ITEM is that material, and walks
	// towards it when none is within ordinary search range. The scan snapshots
	// every entity on the map, so it is rationed per bot; the range is how far a
	// bot will set off for a monster it cannot yet see.
	const DWORD PLAYERBOT_MATERIAL_SCAN_INTERVAL = 90000;
	// How many map snapshots one manager update may take between all the bots.
	// Same idea as the navigation's heavy-plan budget: the scan copies every
	// entity on the map, and the first version let every bot with a shortage do
	// it in the same tick after a restart - measured at 99.9% of a core.
	const int PLAYERBOT_MATERIAL_SCANS_PER_TICK = 6;
	const int PLAYERBOT_MATERIAL_HUNT_RANGE = 40000;
	// How long the frontier wander keeps walking to the collect-row monster
	// the last scan found, ahead of any hub (StartPlayerBotMaterialHunt,
	// ManagePlayerBotWandering), and how near counts as there.
	const DWORD PLAYERBOT_BIOLOGIST_WALK_STICK_MS = 4 * 60 * 1000;
	const int PLAYERBOT_BIOLOGIST_WALK_ARRIVED = 1400;
	// What a monster carrying a wanted material adds to its target score. Above
	// the sweet-spot level bonus of a fair fight and below the party-objective
	// one, so it wins among equals and loses to an errand somebody is waiting on.
	const int PLAYERBOT_WANTED_DROP_BONUS = 120000;

	// Stall prices from what the market actually paid. Fewer sales than this and
	// the counter asks its prior alone; the band keeps one wild purchase from
	// moving a price more than this many times either way from that prior.
	const size_t PLAYERBOT_SALE_MEMORY = 8;
	const size_t PLAYERBOT_SALE_MIN_SAMPLES = 2;
	const DWORD PLAYERBOT_SALE_PRICE_CAP_MULT = 4;
	const DWORD PLAYERBOT_SALE_PRICE_CAP_FLAT = 20000;
	// What a counter asks, scaled to what the buyers carry. The merchant's
	// price times three was the prior for everything, and the merchant pays
	// pennies: a material four hundred bots were short of stood at six hundred
	// yang on a market whose customers held a million each, which is not a
	// market, it is a giveaway. The median spendable wallet of the bots that
	// shop is measured once a minute with the ledger, and a unit asks this
	// share of it - a refine material fifteen per mille, a spare at +4 to +6
	// fifteen per refine step above +2, anything else ten - and a whole stack
	// never more than this percent, so the stack stays within reach of a bot
	// with the median wallet. The merchant's markup still applies where it is
	// the higher of the two.
	const DWORD PLAYERBOT_MARKET_MATERIAL_WALLET_PERMILLE = 15;
	const DWORD PLAYERBOT_MARKET_GEAR_WALLET_PERMILLE_PER_REFINE = 15;
	const DWORD PLAYERBOT_MARKET_OTHER_WALLET_PERMILLE = 10;
	const DWORD PLAYERBOT_MARKET_STACK_WALLET_PERCENT = 30;
	// What the merchant pays for a shellfish, and the yardstick for the wallet
	// floor below.
	//
	// The wallet says what the market can afford in total; on its own it cannot
	// tell two materials apart, and with a median wallet of 2.6 million every
	// material landed on the same ~38 000. That is why a shellfish the merchant
	// values at 3 000 and a white pearl he values at 12 000 stood on the
	// counters at the same price. The floor is now scaled by what the merchant
	// pays for this particular thing against this yardstick, in hundredths so a
	// material worth a fifth of a shellfish is not rounded to nothing, and only
	// upwards: nothing gets cheaper, and what is genuinely worth more costs
	// more. The cap keeps one expensive material from pricing itself out of
	// every buyer's reach.
	// Opening prices for the goods whose merchant value says nothing about what
	// they are worth here. A skill book costs the merchant a thousand yang
	// whichever skill it teaches, and a pearl's proto price was set for a world
	// with different wallets - the median bot here carries over a million and a
	// half. These are a starting calibration to be corrected by what actually
	// sells, not equilibrium prices: the market memory blends them away as
	// transactions accumulate.
	const DWORD PLAYERBOT_PRIOR_BOOK_AURA = 250000;        // Aura Miecza (4)
	const DWORD PLAYERBOT_PRIOR_BOOK_ENCHANTED_BLADE = 220000; // Czarowane Ostrze (63)
	// A weapon from the level-30 set, whatever its refine. It is the prize the
	// whole market exists for - ScorePlayerBotShopStock puts it above every
	// other line - and it was being priced as scrap: an unrefined one fell into
	// the "under +4" branch and asked the merchant's price times two, fifteen
	// thousand, while a Tiger Fur beside it asked sixty because materials get
	// a share of the median wallet. Reported from the Discord with a proposal
	// of fifteen to twenty times that, and the proposal is right about the
	// order of magnitude: between a +7 (150 000) and a +8 (400 000) of
	// ordinary gear, because a bot of thirty-seven holding six million will
	// pay it and a bot of twenty-two will not, which is as it should be.
	const DWORD PLAYERBOT_PRIOR_LEVEL30_WEAPON = 250000;
	const DWORD PLAYERBOT_PRIOR_BOOK_STRONG_BODY = 180000; // Silne Cialo (19)
	const DWORD PLAYERBOT_PRIOR_BOOK_KEY = 140000;         // inne kluczowe dla buildu
	const DWORD PLAYERBOT_PRIOR_BOOK_ORDINARY = 45000;
	// Iwakura's book prices are in playerbot_price_tables.h with the rest of
	// his sheet, scaled along the same yang-rate curve as every other price
	// there (his v1.0 dropped the books' own x1.1 line). A listing then draws
	// PLAYERBOT_BOOK_PRICE_JITTER_MIN to _MAX percent of it, so two counters
	// never ask the same number; the sale memory does the rest. A skill not in
	// the table keeps PLAYERBOT_PRIOR_BOOK_ORDINARY.
	const int PLAYERBOT_BOOK_PRICE_JITTER_MIN = 80;
	const int PLAYERBOT_BOOK_PRICE_JITTER_MAX = 125;

	// Bumped by hand whenever a price table in this file changes. An open
	// stall keeps the price it was listed at, and the offline service visit
	// repriced one line an hour - so a scroll listed at 9 000 before 2.0.32
	// was still asking it a day later ("pelno w m1 sklepow gdzie Zwoje sa po
	// 9000", Iwakura). A shop whose stamp is behind this number reprices on
	// every service visit instead, until its whole counter has been walked.
	// Since 2.0.72 that holds for a yang rate moved while the core runs and
	// not for this number: the stamp lives in memory, a restart takes every
	// counter as priced by the table it starts with, and a new table only
	// ever arrives with a restart - so a bump here reaches the counters at
	// PLAYERBOT_OFFLINE_REPRICE_MS a slice. Persist the stamp (the core's own
	// directory is the game-var volume) before relying on a bump to move
	// prices fast.
	// 3: Iwakura's price list v1.0 (14 September) - jewellery, boots, shields,
	// ores and the mt2009 materials, one scaling curve for everything.
	// 4: the stamp carries the yang rate as well (GetPlayerBotPriceGeneration),
	// so a rate moved in the panel reprices every stand, not only a new table.
	// 5: a weapon's damage lines are read between the sheet's bands
	// (GetPlayerBotDamageTierPct), so a 19% average asks more than a 10% one.
	// 8: his price list of 20 September - nearly every number moved, most of
	// the materials by about seventy percent, and the herbalist's recipes are
	// priced for the first time (one row for all forty of them).
	const DWORD PLAYERBOT_PRICE_TABLE_VERSION = 8;
	// Community patch 2, point 8: inflation. Every PLAYERBOT_INFLATION_STEP_YANG
	// the world's characters hold between them lifts every price his sheet sets
	// by PLAYERBOT_INFLATION_STEP_PERCENT, on top of the yang-rate curve and in
	// whole steps, the way he wrote it: 2.5 billion is +5%, 5 billion +10%, and
	// so on up. The sum is the database's, asked this often; the ceiling is
	// only arithmetic - a world at GOLD_MAX on every character.
	const long long PLAYERBOT_INFLATION_STEP_YANG = 2500000000LL;
	// The share of browses that go to a person's counter first, and how far
	// over the market's price that counter may ask (percent of it).
	const int PLAYERBOT_MARKET_PERSON_FIRST_PERCENT = 33;
	const int PLAYERBOT_MARKET_PERSON_PRICE_PERCENT = 110;
	const int PLAYERBOT_INFLATION_STEP_PERCENT = 5;
	const int PLAYERBOT_INFLATION_MAX_PERCENT = 100000;
	const DWORD PLAYERBOT_INFLATION_REFRESH_MS = 10 * 60 * 1000;
	// Iwakura's tier list (playerbot_item_tiers.h, 16 September): a family's
	// PvE tier moves the whole equipment score by this much per step from
	// the neutral 3 (tier 6 is +24%, tier 1 is -16%), and a bonus line's PvE
	// tier scales its weight in both the equipment score and the reroll
	// pass. A nudge, not a verdict, on purpose: "zeby na slepo nie zamienial
	// Miedzianych Kolczykow +9 na Ebo +1 bez bonow mimo, ze tabela tak
	// sugeruje" - the refine and the lines stay what decides.
	const int PLAYERBOT_TIER_SCORE_PERCENT = 8;
	const int PLAYERBOT_BONUS_TIER_PERCENT[7] = { 100, 25, 50, 80, 100, 115, 130 };
	// Iwakura's upgrade-material prices ("ULEPSZACZE") and the goods he prices
	// by name are generated into playerbot_price_tables.h from his sheet. A name
	// is not an item: where the game has two vnums under one name (Nieznany
	// Talizman+, Zabie Udka, Nieznane Lekarstwo, Ozdobna Spinka) both carry the
	// price, because the bot prices items.
	// Smart rounding (Iwakura, 13 September): a player puts a round number on a
	// counter, so "1 591 511" reads as a machine and "1 595 000" reads as a
	// person. The step is the price's own order of magnitude over
	// PLAYERBOT_PRICE_ROUND_DIVISOR and the price goes up to the next one, which
	// reproduces all five of his worked examples: 12 555 -> 12 600 (step 50),
	// 401 501 -> 402 000 (500), 1 241 412 -> 1 245 000 (5 000),
	// 11 512 125 -> 11 550 000 (50 000), 121 314 515 -> 121 500 000 (500 000).
	// Each lands inside the range he gave. Below the minimum nothing is
	// rounded: a material at 300 yang is not made prettier by becoming 400.
	const DWORD PLAYERBOT_PRICE_ROUND_MIN = 10000;
	const DWORD PLAYERBOT_PRICE_ROUND_DIVISOR = 200;
	const DWORD PLAYERBOT_PRIOR_PEARL_WHITE = 2000000;
	const DWORD PLAYERBOT_PRIOR_PEARL_BLUE = 3000000;
	const DWORD PLAYERBOT_PRIOR_PEARL_RED = 6000000;
	// And the shell the pearls come out of. It had no prior at all, so it
	// asked the merchant's three thousand times three - "malze wystawione po
	// 7k" - beside pearls asking millions, when what a shell is is a bet on
	// those pearls: half a Stone Piece, a twentieth a white pearl, a twentieth
	// a blue, a hundredth a blood one (char_item.cpp, English locale table).
	// Reported with "should be a hundred thousand at least"; a hundred
	// thousand is about a tenth of the expected pearl value inside and leaves
	// the buyer the better side of the bet, which is what makes it sell.
	const DWORD PLAYERBOT_PRIOR_SHELLFISH = 100000;
	// A horse medal, and everything else the merchant will not buy.
	//
	// item_proto gives 50050 a shop price of zero, so GetPlayerBotNpcSellUnitPrice
	// returns nothing, the markup multiplies nothing, and the counter asked
	// max(1, 0) - one yang - for the one item in this world a bot cannot farm on
	// demand and needs twenty-one of. The Discord watched bots of fifteen to
	// twenty-five put them out at that price. Every chest and casket is in the
	// same position: 50011, 50192 and 50193 all carry a zero price.
	const DWORD PLAYERBOT_PRIOR_HORSE_MEDAL = 400000;
	const DWORD PLAYERBOT_PRIOR_NO_MERCHANT_PRICE = 30000;
	// Under this a number on a counter is not a price, it is an accident - and
	// the accident used to be permanent, see LimitPlayerBotAskStep.
	const DWORD PLAYERBOT_MARKET_ASK_FLOOR = 100;
	const DWORD PLAYERBOT_MARKET_WALLET_REFERENCE_PRICE = 600;
	const DWORD PLAYERBOT_MARKET_WALLET_WORTH_MIN_PERCENT = 100;
	const DWORD PLAYERBOT_MARKET_WALLET_WORTH_MAX_PERCENT = 800;
	// A piece off a counter has to beat what the bot wears, and any spare in
	// its bag for the slot, by this much. Two armours of one vnum and refine
	// differ by their bonus rolls, and "better than worn" bought the second
	// four seconds after the first; a sideways step is not worth the yang.
	const long long PLAYERBOT_MARKET_GEAR_MARGIN_PERCENT = 15;
	const DWORD PLAYERBOT_SALE_RECENT = 600000;
	const DWORD PLAYERBOT_SALE_STALE = 3600000;

	// The market ledger (playerbot_world_memory.h): what the counters hold of
	// a thing and how many bots are short of it, rebuilt this often, and how
	// often it is written to the log.
	const DWORD PLAYERBOT_MARKET_LEDGER_INTERVAL = 60000;
	const DWORD PLAYERBOT_MARKET_REPORT_INTERVAL = 600000;
	// A material goes on a counter only while the counters hold fewer units of
	// it than the bots short of it would buy, with a margin: five units a buyer
	// - one refine's worth and a spare - and half as much again on top. With
	// nobody short of it one stack may stand as a probe; more than that is
	// stock nobody asked for, and it stays in the bag.
	const DWORD PLAYERBOT_MARKET_SUPPLY_PER_BUYER = 5;
	const DWORD PLAYERBOT_MARKET_SUPPLY_MARGIN_PERCENT = 150;
	// ...and that counted only bots. A player is the buyer this ledger cannot
	// see, so the rule above held nearly everything back: on m2zip on 18
	// September 3 366 of the listing decisions of ten minutes said overstock
	// against 255 that listed, the bags held 844 thousand units of material
	// and the counters 105 thousand, Kawalek Klejnotu 114 215 held and 7 to
	// 174 on the counters of a village, and a player's item finder found no
	// counter at all with the Orc Valley's or the desert's materials in any of
	// the three kingdoms ("chomikuja po 40 sztuk", Hiob; "0 bodzi na
	// sklepach", Xewi). So a village's own counters keep this many units of
	// every recipe material a bot there holds beyond its anvil's reserve,
	// whatever the bots are short of: the floor is asked first, of the
	// counters on the map the keeper stands on, and only in a village - away
	// from one there is no counter a floor could be about.
	const DWORD PLAYERBOT_MARKET_LOCAL_FLOOR_UNITS = 50;
	// And a material under that floor goes up ahead of spare gear, which
	// scores 1000 and its plus from +4: a counter adds one line a visit, the
	// top of its list, and a full counter's first free column went to a
	// breastplate. Two days of m2zip's sales were 792 of 11 416 material lines
	// against 210 of some sixteen thousand lines of gear, and the floor stops
	// asking once the village holds PLAYERBOT_MARKET_LOCAL_FLOOR_UNITS - so
	// the gear waits a visit or two, not for good. Still behind a piece with
	// prize lines (1500) and a level-30 weapon (2000).
	const int PLAYERBOT_SHOP_FLOOR_SCORE = 1100;
	// Pricing. The prior counts as this many sales when the market's median is
	// blended in: after four sales the two weigh the same, after the full
	// memory of eight the market has two thirds of the say.
	const DWORD PLAYERBOT_MARKET_ANCHOR_N0 = 4;
	// The regulator: (demand + q0) / (supply + q0) to this power, kept within
	// these bounds. q0 is a typical stack, so one buyer against one counter is
	// not a shortage.
	const DWORD PLAYERBOT_MARKET_REGULATOR_Q0 = 5;
	const double PLAYERBOT_MARKET_REGULATOR_EXPONENT = 0.2;
	const double PLAYERBOT_MARKET_REGULATOR_MIN = 0.75;
	// The ceiling on the shortage premium. At 1.35 a material five hundred bots
	// were short of and no counter carried could ask a third more than one
	// nobody wanted, which is not a market answering a shortage.
	const double PLAYERBOT_MARKET_REGULATOR_MAX = 2.0;
	// How fast the market's ask for a thing may drift: this much per interval
	// since it last moved, up to this many intervals at once; and how long an
	// ask is remembered after the last counter carried the thing.
	const DWORD PLAYERBOT_MARKET_STEP_PERCENT = 5;
	const DWORD PLAYERBOT_MARKET_STEP_INTERVAL = 600000;
	const DWORD PLAYERBOT_MARKET_STEP_MAX_STEPS = 6;
	const DWORD PLAYERBOT_MARKET_ASK_STALE = 3600000;

	const DWORD PLAYERBOT_PORTAL_WALK_TIMEOUT = 20000;
	// What counts as having moved. Below this the bot is standing still, whether
	// the navigation deferred the plan, backed off, or quietly reported success.
	const int PLAYERBOT_PORTAL_WALK_PROGRESS = 150;
	// And the walk has to have been attempted, not merely awaited. The clock
	// above is wall time and runs while the bot is doing something else
	// entirely - fighting, looting, standing at a merchant - so the first travel
	// tick after a busy twenty seconds declared a stall on a bot that had been
	// given exactly one chance to walk. Caught in the act: one tick, a full
	// route, and eighty-eight kilometres still to go.
	const WORD PLAYERBOT_PORTAL_WALK_MIN_TICKS = 8;

	const long PLAYERBOT_M2_TO_M1_PORTAL_X = 113000;
	const long PLAYERBOT_M2_TO_M1_PORTAL_Y = 213600;
	// This share of the bots (by pid, for life) goes home to Joan for its
	// services instead of Bokjung. Every frontier return went to Bokjung, so
	// past thirty the first town saw nobody but anglers and the Biologist's
	// callers ("M1 przy 1000 botow wyglada jak Balmora, a M2 jak Baerim").
	// Joan has every service Bokjung has; the price is the walk back through
	// Bokjung to the Teleporter, which is why it is a share and not a coin.
	const int PLAYERBOT_JOAN_HOME_PER_MILLE = 300;
	const long PLAYERBOT_M1_RETURN_X = 87600;
	const long PLAYERBOT_M1_RETURN_Y = 213100;
	const long PLAYERBOT_M2_MONKEY_PORTAL_X = 161700;
	const long PLAYERBOT_M2_MONKEY_PORTAL_Y = 211900;
	const long PLAYERBOT_MONKEY_EASY_ARRIVAL_X = 852000;
	const long PLAYERBOT_MONKEY_EASY_ARRIVAL_Y = 447700;
	const long PLAYERBOT_MONKEY_RETURN_PORTAL_X = 852000;
	const long PLAYERBOT_MONKEY_RETURN_PORTAL_Y = 447100;
	const long PLAYERBOT_M2_MONKEY_RETURN_X = 161100;
	const long PLAYERBOT_M2_MONKEY_RETURN_Y = 213000;
	// Bokjung has its own Stable Boy.  A medal expedition therefore ends here;
	// there is no artificial M2 -> M1 return trip.
	const long PLAYERBOT_M2_STABLE_BOY_X = 146900;
	const long PLAYERBOT_M2_STABLE_BOY_Y = 232400;
	// Real Bokjung NPC positions from metin2_map_b3/npc.txt. M2 therefore has
	// every routine service needed by a level 20-35 character; only profession
	// trainers and the Biologist still require a trip back to Joan (M1).
	const long PLAYERBOT_M2_WEAPON_MERCHANT_X = 147200;
	const long PLAYERBOT_M2_WEAPON_MERCHANT_Y = 243500;
	const long PLAYERBOT_M2_ARMOR_MERCHANT_X = 148500;
	const long PLAYERBOT_M2_ARMOR_MERCHANT_Y = 242200;
	const long PLAYERBOT_M2_MISC_MERCHANT_X = 141300;
	const long PLAYERBOT_M2_MISC_MERCHANT_Y = 240400;
	const long PLAYERBOT_M2_BLACKSMITH_X = 142000;
	const long PLAYERBOT_M2_BLACKSMITH_Y = 239200;
	// The M2 teleporter leads to Waryong (the infected-animal area commonly
	// called M3).  The return portal is NPC 10021 on map 24.
	const long PLAYERBOT_M2_TO_M3_TELEPORTER_X = 136900;
	const long PLAYERBOT_M2_TO_M3_TELEPORTER_Y = 240300;
	// Town.txt for metin2_map_guild_02 (map 24) points at local (427,92),
	// i.e. global (221900,9200).  The old (179500,1000) was copied from the
	// generic teleporter quest's empire table and lands in the unwalkable north-
	// west border of this map, leaving every arriving bot without a sectree.
	const long PLAYERBOT_M3_ARRIVAL_X = 221900;
	const long PLAYERBOT_M3_ARRIVAL_Y = 9200;
	const long PLAYERBOT_M3_RETURN_PORTAL_X = 222000;
	const long PLAYERBOT_M3_RETURN_PORTAL_Y = 8800;
	const long PLAYERBOT_M2_FROM_M3_X = 145500;
	const long PLAYERBOT_M2_FROM_M3_Y = 240000;
	// Maps opened past Bokjung.  Every coordinate here was read out of
	// locale/english/map/{index,Setting.txt,Town.txt,npc.txt}: the arrival points
	// are the Chunjo entries of Town.txt, the exits are the Teleporter (NPC 9012)
	// each map carries, and departure reuses Bokjung's own Teleporter.
	// Joan's own Teleporter (NPC 9012 in metin2_map_b1/npc.txt).
	// The Teleporter (9012) is a quest, map_warp.quest: it refuses a
	// character of ten or under and charges floor(level / 5) * 1000 yang, a
	// thousand at least, for a warp to Orc Valley, the desert, Sohan or the
	// Demon Tower gate. A bot that leaves through him pays the same.
	const BYTE PLAYERBOT_TELEPORTER_MIN_LEVEL = 11;
	const int PLAYERBOT_TELEPORTER_FEE_PER_FIVE_LEVELS = 1000;
	// How long a gate found on the map is remembered per map and destination.
	const DWORD PLAYERBOT_WARP_NPC_CACHE_MS = 600000;
	const long PLAYERBOT_M1_TELEPORTER_X = 51900;
	const long PLAYERBOT_M1_TELEPORTER_Y = 153600;
	const long PLAYERBOT_MAP_DESERT = 63;
	const long PLAYERBOT_MAP_ORC_VALLEY = 64;
	// Not the map's own empire spawn point, which is where this used to be.
	// map_n_threeway is a three-empire border map and each corner is walled off:
	// from the Chunjo spawn a bot could reach 17 of the map's 532 spawn groups
	// and none of the twelve hunting hubs. Thirteen bots sat there at exactly the
	// entry level, never advancing, while planning routes that could not exist -
	// 7812 of 8259 "unreachable" lines in one session came from that corner.
	//
	// These two are hub coordinates from regen.txt, so they are standable, and
	// they are where the entrance opens onto the map's central island.
	//
	// The rest of the map used to be unreachable from here as well - 161 spawn
	// groups of 532 - because the navigation refused water and every one of the
	// twenty-two bridges in this delta is water with the block bit cleared. With
	// that fixed the whole map is one piece and all 532 groups are reachable, so
	// these coordinates are now simply the way in rather than the only island a
	// bot could use. Measured with tools/analyse_map_bridges.py.
	// Where a Chunjo character actually comes out, which is not where the bots
	// were being put. Orc Valley has four teleporter NPCs in its npc.txt - one
	// per empire at cells (1472,73), (131,746) and (640,1436), and a fourth in
	// the middle of the map at (767,792) that belongs to nobody. The arrival
	// used to be (712,767): the middle one. Every bot in the world therefore
	// materialised on the central island, and since wandering only runs on a
	// tick with nothing to fight - on a map with 4041 spawn points, never - that
	// is where they stayed. Amulet Orka has no spawn within 22000 units of that
	// spot; nor has the Esoteric Guide. Between them 493 bots were short of
	// those two materials while standing on the one island that does not drop
	// them.
	//
	// This is the engine's own answer: Town.txt is read as a general spawn point
	// followed by three empire pairs (SECTREE_MANAGER::LoadMapRegion), and the
	// second pair - empire 2, Chunjo - is cell (144,743). The desert was already
	// set this way, which is how the discrepancy showed up at all.
	const long PLAYERBOT_ORC_VALLEY_ARRIVAL_X = 270400;
	const long PLAYERBOT_ORC_VALLEY_ARRIVAL_Y = 739900;
	const long PLAYERBOT_DESERT_ARRIVAL_X = 221900;
	const long PLAYERBOT_DESERT_ARRIVAL_Y = 502700;
	// Leave through the Chunjo gate NPC beside the arrival point, not through the
	// Teleporter in the middle of the map. The death heatmap showed almost every
	// desert casualty within ~1500 units of that central Teleporter: a bot which
	// had already run out of potions was crossing 75k units of hostile ground to
	// reach it. These gates sit a few steps from where the bot arrived.
	// The bot walks to the exit before it is warped out, so this has to be in the
	// same region as the arrival - an exit on the far side of a wall would strand
	// every bot that ever entered.
	// npc.txt cell (131,746): teleporter 10009, the Chunjo gate, a few steps from
	// where Town.txt puts a Chunjo character down. The desert names its own gate
	// the same way - npc 10010 at cell (149,135) - and that pairing is the one
	// this file follows.
	const long PLAYERBOT_ORC_VALLEY_EXIT_X = 269100;
	const long PLAYERBOT_ORC_VALLEY_EXIT_Y = 740200;
	const long PLAYERBOT_DESERT_EXIT_X = 219700;
	const long PLAYERBOT_DESERT_EXIT_Y = 499900;

	// Two more frontier maps. Mount Sohan (metin2_map_c3) is Black Wind and
	// Wild soldiers at 26 to 36 with three Metin kinds at 25 to 35 - the same
	// band as the desert and the Fanatic islands, so from 26 a bot may go
	// there instead of waiting in Bokjung, and at 30-35 the three maps share
	// the population by pid. The Spider Dungeon (metin2_map_spiderdungeon,
	// "Kuahklo Dong") is knights of 50 to 58 and nothing else: from 48, half
	// of the population takes it instead of Orc Valley. Both used to sit on a
	// core the bots do not run on; m2-render-config moves them. Arrivals are
	// the Chunjo entries of Town.txt, exits the NPC beside each - Sohan's
	// Staruszek (20009) at cell (136,899), the dungeon's Yongbi teleporter
	// (10015) at (88,82) - so a bot never crosses the map to leave.
	const long PLAYERBOT_MAP_SOHAN = 61;
	const long PLAYERBOT_MAP_SPIDER_V1 = 104;
	// The second Spider Dungeon (metin2_map_spiderdungeon_02): poison spiders
	// of 60-68 that never attack first, no stones, a Chuk-Sal at the end of V1
	// who wants a pass to let anyone through, and Pung-Ho at (385,274) to send
	// them back. A bot does not hold the pass and does not talk to Chuk-Sal:
	// like V1 it is reached across the desert to the Kuahlo gate and entered
	// server-side from there, and left the same way. The Elite Spider Queen
	// (2093, level 97, 2.5 million health) stands by the V3 warp every hour
	// and is nobody's raid at these levels - no boss hub.
	const long PLAYERBOT_MAP_SPIDER_V2 = 71;
	// The Hwang Temple, metin2_map_milgyo, base (537600,51200), 102400 square.
	//
	// Read out of the server's own spawn files rather than off a wiki, with
	// tools/analyse_map_spawns.py: 5088 spawn points over nineteen kinds, levels
	// 52 to 61, the median at 56. The west half is the Elite Esoterics of 52-55
	// and is where the map is entered; the east half is the Tree Turtle Soldier
	// (57, 612 points), the Bogey (58) and the Esoteric Tormentor (56). Two boss
	// points every two hours roll among the Esoteric Summoner (54), the Frog
	// General (61) and the Yellow Tiger Spectre (75) - no boss hub here, because
	// a hub needs one named race and that roll has three.
	//
	// It has no Metin stones. What its stone.txt carries is sixteen ore veins -
	// ebony, crystal, amethyst, diamond, white gold, shells, heaven's tears -
	// which is what the wiki says too and is the one thing worth checking twice,
	// since every other frontier's stone.txt means stones.
	//
	// The reason to add it is not the level band, which Sohan and V1 already
	// cover from 48. It is the drops: the Frog Tongue (30060), the Frog Legs
	// (30061), the Leaf (30040), the Unknown Talisman+ (30079) and the Curse
	// Book+ (30080) appear in eighteen refine recipes and on no map this world
	// hosts for bots, and the market ledger has been asking for the first of
	// them with a supply of exactly zero. Nothing had to be taught about them:
	// GetPlayerBotRefineMaterialVnums reads the engine's own recipe table, so
	// they became goods the moment a bot could stand where they drop.
	const long PLAYERBOT_MAP_HWANG = 65;
	// The three maps moved off game2 in m2-render-config so the bots can reach
	// them at all. The Demon Tower (66) is where the Biologist's level-50
	// specimen and the military horse live - 1001-1004 of 57-60 stand nowhere
	// else in this world - and the two forests are ground this world had none
	// of: Trent (67) carries 2301-2305 of 65-71 over 912 spawn points, the Red
	// Forest (68) carries 2311-2315 of 74-82 over 1456. Measured out of their
	// own regen files, not a wiki.
	const long PLAYERBOT_MAP_DEMON_TOWER = 66;
	const long PLAYERBOT_MAP_FOREST = 67;
	const long PLAYERBOT_MAP_RED_FOREST = 68;
	// Doyyumhwaji, the land of fire (metin2_map_n_flame_01), moved off game2
	// for the same reason the three above were. Read out of its own files on
	// the 2.x line (23 September): 783 regen lines carry about 3 400 monsters
	// of 69 to 72 - Sluga Walczacego Tygrysa (2201) and Ognisty Duch (2202) of
	// 69, Walczacy Tygrys (2203) of 70, Plomien (2204) of 71 and Ognisty
	// Wojownik (2205) of 72, every one of the FIRE race - so it fills the band
	// between the Forest (65-71) and the Red Forest (74-82) and is richer than
	// both together. boss.txt puts two elites there every half hour with an
	// escort, Piekielny Zarlacz Dusz (2281, 71) and Plomienny Egzekutor (2282,
	// 72); stone.txt four Metins of Murder (8014, 70) and thirteen ore veins.
	// Each kingdom has its own entrance and its own gate home
	// (playerbot_empire_rules.h), like the valley, the desert and Sohan.
	const long PLAYERBOT_MAP_FIRE_LAND = 62;
	// The Spider Dungeon is entered from the desert, the way the game has it:
	// NPC 10016 "Kuahlo Dong" in the desert's bottom-right corner (cell 1425,
	// 1477 of metin2_map_n_desert_01) sends a character to (600, 4960) in V1,
	// and V1's exit NPC 10015 puts it back on the desert at (3467, 6329). A
	// bot used to warp from Bokjung's teleporter straight into V1 and straight
	// back, which no player can do. Now the trip is two legs with the desert
	// crossed on foot between them - and crossed, not farmed: the bot fights
	// nothing on the way except a stone worth its level, within this reach.
	const long PLAYERBOT_DESERT_V1_GATE_X = 347300;
	const long PLAYERBOT_DESERT_V1_GATE_Y = 634100;
	const long PLAYERBOT_DESERT_FROM_V1_X = 346700;
	const long PLAYERBOT_DESERT_FROM_V1_Y = 632900;
	const int PLAYERBOT_CROSSING_STONE_RANGE = 2500;

	// --- What each map is made of --------------------------------------------
	//
	// battle.cpp CalcAttBonus walks the races as an else-if chain and stops at
	// the first flag the monster carries, so a kill pays exactly one of them;
	// and char.cpp maps an APPLY onto only six of the eleven - ANIMAL, UNDEAD,
	// DEVIL, HUMAN, ORC, MILGYO. INSECT, FIRE, ICE, DESERT and TREE have a POINT
	// and a place in the damage formula but nothing an item can put into them,
	// so on a map made of those a "silny przeciwko" line is decoration.
	//
	// The slots are ordered as the engine tests them. OTHER is the fights that
	// paid nothing, and it is counted rather than dropped: without it a bot on
	// the desert has an empty histogram and falls back on whatever the map
	// aggregate says, instead of the truth, which is "nothing here pays".
	enum EPlayerBotRaceSlot
	{
		PLAYERBOT_RACE_ANIMAL = 0,
		PLAYERBOT_RACE_UNDEAD,
		PLAYERBOT_RACE_DEVIL,
		PLAYERBOT_RACE_HUMAN,
		PLAYERBOT_RACE_ORC,
		PLAYERBOT_RACE_MILGYO,
		PLAYERBOT_RACE_OTHER,
		PLAYERBOT_RACE_SLOTS,
		PLAYERBOT_RACE_NONE = -1
	};

	// The measurement, from tools/analyse_map_races.py: every spawn point of
	// every map a bot may stand on, resolved through group.txt and
	// group_group.txt, counted by the one race that pays. A share, not a flag,
	// because a line that covers 46% of Mount Sohan is worth about half what the
	// same line is worth in a Monkey Dungeon, and a bot should be able to tell.
	//
	// Re-measure rather than edit by hand; a map added to the frontier needs a
	// row here, and with none it simply falls back on what the population has
	// seen, which is the behaviour this table replaced.
	struct TPlayerBotMapRaceRow
	{
		long lMapIndex;
		int iRace;
		int iPercent;
	};
	const TPlayerBotMapRaceRow PLAYERBOT_MAP_RACE_TABLE[] = {
		{ 1, PLAYERBOT_RACE_ANIMAL, 77 },   { 21, PLAYERBOT_RACE_ANIMAL, 77 },
		{ 41, PLAYERBOT_RACE_ANIMAL, 77 },
		{ 3, PLAYERBOT_RACE_HUMAN, 100 },   { 23, PLAYERBOT_RACE_HUMAN, 100 },
		{ 43, PLAYERBOT_RACE_HUMAN, 100 },
		{ 4, PLAYERBOT_RACE_ANIMAL, 100 },  { 24, PLAYERBOT_RACE_ANIMAL, 100 },
		{ 44, PLAYERBOT_RACE_ANIMAL, 100 },
		{ 5, PLAYERBOT_RACE_ANIMAL, 100 },  { 25, PLAYERBOT_RACE_ANIMAL, 100 },
		{ 45, PLAYERBOT_RACE_ANIMAL, 100 }, { 108, PLAYERBOT_RACE_ANIMAL, 100 },
		{ 109, PLAYERBOT_RACE_ANIMAL, 100 },
		{ 61, PLAYERBOT_RACE_UNDEAD, 46 },  // Sohan: the other 54% is ICE
		{ 64, PLAYERBOT_RACE_ORC, 63 },     // Orc Valley: 35% of it is MILGYO
		{ 65, PLAYERBOT_RACE_MILGYO, 68 },  // Hwang
		// 63 Yongbi Desert (DESERT/INSECT), 104 and 71 the Spider Dungeons
		// (INSECT), 62 Doyyumhwaji (every monster FIRE): no row, because no
		// line reaches those races.
	};

	// The race a map pays for, and how much of the map it is. Zero percent means
	// "nothing here", which is a different answer from "not measured".
	int GetPlayerBotMapRace(long mapIndex, int* percentOut)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_MAP_RACE_TABLE) /
				sizeof(PLAYERBOT_MAP_RACE_TABLE[0]); ++i)
		{
			if (PLAYERBOT_MAP_RACE_TABLE[i].lMapIndex != mapIndex)
				continue;
			if (percentOut)
				*percentOut = PLAYERBOT_MAP_RACE_TABLE[i].iPercent;
			return PLAYERBOT_MAP_RACE_TABLE[i].iRace;
		}
		if (percentOut)
			*percentOut = 0;
		return PLAYERBOT_RACE_NONE;
	}
	// An episode of self-defence, so that "it hit me first" cannot become a
	// permanent licence to grind. The clock starts when the bot accepts an
	// attacker as a target and is not renewed by another hit from the same one;
	// the leash is measured from where the episode started. Both are tuning
	// values - measure before trusting them.
	const DWORD PLAYERBOT_DEFENCE_EPISODE_TIME = 10000;
	const int PLAYERBOT_DEFENCE_LEASH = 1000;
	// How far away a party member may be and still be worth defending.
	const int PLAYERBOT_PARTY_DEFENCE_RANGE = 1500;
	// An episode ends for good only when the fighting has actually stopped.
	// Without this a second attacker starts a fresh episode the moment the
	// first one's runs out, and two monsters taking turns are an endless
	// licence to grind - which is exactly what a bound is supposed to prevent.
	const DWORD PLAYERBOT_DEFENCE_QUIET_TIME = 15000;
	// A drop obeys the same level difference as experience: PERCENT_LVDELTA
	// multiplies both. Fifteen levels above a monster leaves one percent of the
	// chance, so "I need this material" must not justify farming something that
	// will effectively never yield it. Lower than the experience floor on
	// purpose - a material is worth more detours than experience is.
	const int PLAYERBOT_MATERIAL_MIN_DROP_PERCENT = 10;
	// The share of a monster's base experience left after the level difference,
	// below which an ordinary monster is not worth a bot's time. A starting
	// heuristic from the audit, not a measurement of experience per hour, and
	// not a rule of the game: quest, material and equipment errands are allowed
	// through it, and self-defence comes before it.
	const int PLAYERBOT_COMBAT_MIN_EXP_PERCENT = 20;
	// A village monster this many levels under the bot is outgrown prey, and
	// the bot does not cross the field for it. The exp table above cannot say
	// this: on the mt2009 line it still pays 90% at six under, and the base a
	// Wild Dog carries is 15 against a Blue Alpha Wolf's 111. With the search
	// range at PLAYERBOT_SEARCH_RANGE a bot at its own band's hub still saw
	// the dogs six kilometres off, and 96 of 115 bots on Joan stood nowhere
	// near any hub, chain-killing whatever was next - a third of every fight
	// six or more levels under the bot, a level in the teens every two hours.
	// What is within PLAYERBOT_OUTGROWN_CHAIN_RANGE is killed on the way, as
	// a player would; beyond it the wander pass gets the tick and walks the
	// bot to its band's hub. The first villages, where the 1-31 spread is,
	// and since 2.0.58 the second: their gates open onto the low ground
	// (Jayang's in its south, Bakra's in its north) and a bot chain-killed
	// outward from the gate for the rest of its life, so the 501-504 half of
	// 29-36 had nobody on it (blasty, 16 September). Defence, quest,
	// material and equipment errands come before it.
	const int PLAYERBOT_VILLAGE_OUTGROWN_LEVELS = 6;
	const int PLAYERBOT_OUTGROWN_CHAIN_RANGE = 800;
	// A second village's hubs fall into three bands - 27, 29-30 and 35 - and
	// a bot under twenty-five matches none of them; the nearest band is the
	// 402/403 edge, of which Jayang has one hub. The M2 wander fills its
	// choice set up to this many from the nearest bands, so the youngest
	// cohort is spread over several rectangles instead of one.
	const int PLAYERBOT_M2_HUB_CHOICES_MIN = 4;
	// A bot above every band of a village (here for an errand) takes the top
	// bands together until there are at least this many hubs - Joan's top
	// band alone is two hubs (CollectPlayerBotM1HubsForLevel).
	const int PLAYERBOT_M1_OUTGROWN_HUB_CHOICES_MIN = 6;
	// How often the monster a bot is already fighting is asked again whether
	// it is still worth fighting. Not every tick: the answer needs the bot's
	// material shortages, which cost a walk of the bag.
	const DWORD PLAYERBOT_COMBAT_RECHECK_INTERVAL = 3000;
	// How close to a world portal a bot walks before its map change is made
	// server-side. See MovePlayerBotToWorldPortal and patch 0008: the engine
	// no longer grabs a bot at the portal, so this only has to cover one
	// tick of running rather than the nine metres it used to.
	const int PLAYERBOT_PORTAL_SWITCH_DISTANCE = 200;
	// The ways a walk step can end, as the portal diagnostic reports them.
	enum EPlayerBotNavOutcome
	{
		PLAYERBOT_NAV_OUT_NONE = 0,
		PLAYERBOT_NAV_OUT_MOVED = 1,        // a waypoint was issued
		PLAYERBOT_NAV_OUT_ARRIVED = 2,      // the route ran out under the bot
		PLAYERBOT_NAV_OUT_BACKOFF = 3,      // waiting out a planning back-off
		PLAYERBOT_NAV_OUT_DEFERRED = 4,     // the tick's planning budget was spent
		PLAYERBOT_NAV_OUT_UNREACHABLE = 5,  // the planner says there is no way
		PLAYERBOT_NAV_OUT_NO_PROGRESS = 6,  // a waypoint that would not come closer
		PLAYERBOT_NAV_OUT_SEGMENT = 7,      // the live world refused the next step
		PLAYERBOT_NAV_OUT_ALIGNED = 8,      // stepped to the cell centre to clear a corner
		PLAYERBOT_NAV_OUT_CORNERED = 9,     // skipped a grazed corner waypoint
		PLAYERBOT_NAV_OUT_ESCAPED = 10,     // stepped off ground nothing can leave
		PLAYERBOT_NAV_OUT_REFUSED = 11,     // Goto itself would not take the order
		PLAYERBOT_NAV_OUT_FORCED = 12       // walked a segment the live world called blocked
	};
	const DWORD PLAYERBOT_CROSSING_STONE_CHECK_INTERVAL = 3000;
	// map_n_snowm_01, base (358400,153600), 153600 square; the town spawn from
	// its Town.txt (cell 768,768). 43 was the second Jinno village and carried
	// soldiers of 26-36; Sohan proper is the Infected of 49-58 in the south and
	// the ice creatures of 62-66 in the north.
	const long PLAYERBOT_SOHAN_ARRIVAL_X = 435200;
	const long PLAYERBOT_SOHAN_ARRIVAL_Y = 230400;
	const long PLAYERBOT_SOHAN_EXIT_X = 435200;
	const long PLAYERBOT_SOHAN_EXIT_Y = 230900;
	const long PLAYERBOT_SPIDER_ARRIVAL_X = 60000;
	const long PLAYERBOT_SPIDER_ARRIVAL_Y = 496600;
	const long PLAYERBOT_SPIDER_EXIT_X = 60000;
	const long PLAYERBOT_SPIDER_EXIT_Y = 494600;
	// V2: the map's own Town.txt cell (384,273) on open ground - 81 of 81 free
	// cells within 200 units - and the exit five hundred south of it, on the
	// same ground; the whole map is one connected component.
	const long PLAYERBOT_SPIDER_V2_ARRIVAL_X = 704050;
	const long PLAYERBOT_SPIDER_V2_ARRIVAL_Y = 462550;
	const long PLAYERBOT_SPIDER_V2_EXIT_X = 704050;
	const long PLAYERBOT_SPIDER_V2_EXIT_Y = 463050;
	// Fifty-four is the operator's number: the weakest spider is sixty and
	// none of them attacks first, so a bot six under is hunting, not hunted.
	const BYTE PLAYERBOT_SPIDER_V2_MIN_LEVEL = 54;
	const BYTE PLAYERBOT_SOHAN_MIN_LEVEL = 48;
	const BYTE PLAYERBOT_SOHAN_MAX_LEVEL = 75;
	// The ice creatures of the north (62-66) are for bots that have outgrown
	// the Infected.
	const BYTE PLAYERBOT_SOHAN_ICE_MIN_LEVEL = 58;
	const BYTE PLAYERBOT_SPIDER_MIN_LEVEL = 48;
	// The arrival is the temple's own Town.txt cell (161,938); the exit is five
	// hundred units south of it. Both were checked against milgyo's server_attr
	// and stand on open ground - eighty-one of eighty-one free cells within two
	// hundred units, which is the radius the portal switch tests.
	const long PLAYERBOT_HWANG_ARRIVAL_X = 553700;
	const long PLAYERBOT_HWANG_ARRIVAL_Y = 145000;
	const long PLAYERBOT_HWANG_EXIT_X = 553700;
	const long PLAYERBOT_HWANG_EXIT_Y = 145500;
	// Fifty-two is where its weakest Elite Esoteric stands, and fifty-five where
	// the east half begins. Nothing below the first has any business here.
	const BYTE PLAYERBOT_HWANG_MIN_LEVEL = 52;
	// The Forest (67) and the Red Forest (68), and the Demon Tower (66) as an
	// errand rather than a frontier.
	//
	// Every point below is a real spawn point out of the map's own regen file,
	// taken from the densest 6400-unit cell and nearest that cell's centre. A
	// spawn point is ground the engine itself puts monsters on, which is the
	// best evidence available here: this machine has no python-lzo, so
	// server_attr could not be decoded to check the cell directly. It was
	// decoded in 2.0.77 (m2-eterpack:dev has lzo): the Forest's and the Demon
	// Tower's two points stand on open ground, the Red Forest's did not - see
	// below - and one hub of each forest stood on a blocked cell.
	//
	// The coordinate rule is the one in "Engine facts": world = BasePosition +
	// cell * 100. It was confirmed the hard way tonight - the production Orc
	// Valley hubs land on real spawn cells at x100 and on nothing at x200.
	const long PLAYERBOT_FOREST_ARRIVAL_X = 316300;
	const long PLAYERBOT_FOREST_ARRIVAL_Y = 16500;
	const long PLAYERBOT_FOREST_EXIT_X = 316300;
	const long PLAYERBOT_FOREST_EXIT_Y = 17000;
	// Trent's own spawns are 65 to 71 (Duch Drzewa through Zle Drzewo), so the
	// band starts where its weakest monster stops being a waste of a trip.
	const BYTE PLAYERBOT_FOREST_MIN_LEVEL = 62;
	// The Red Forest's two points were the ones that check was owed: decoded
	// in 2.0.77, both stood on blocked cells. An arrival there is rescued a
	// cell away by the engine, but the exit's nearest open cell was 625 units
	// off, beyond every snap, so each bot that wanted to leave planned the
	// same unreachable walk every twenty seconds - 657 of the core's 1018
	// "unreachable" lines in half an hour on m2zip, 22 of the 25 bots on the
	// map. Both are cell centres of the map's main walkable area now, with
	// three open cells all round (scratchpad/pick_points_2077.py of session
	// 82d3ab90 is the measurement: the map's own server_attr, BLOCK|OBJECT at
	// the cell centre like the navigation grid, the area the regen stands on).
	const long PLAYERBOT_RED_FOREST_ARRIVAL_X = 1110125;
	const long PLAYERBOT_RED_FOREST_ARRIVAL_Y = 72425;
	const long PLAYERBOT_RED_FOREST_EXIT_X = 1109625;
	const long PLAYERBOT_RED_FOREST_EXIT_Y = 72425;
	// 74 to 82 (Czerw. Duch Drzewa through Czerwone Zle Drzewo).
	const BYTE PLAYERBOT_RED_FOREST_MIN_LEVEL = 71;
	// Doyyumhwaji for a caller with no character: Chunjo's entrance and
	// Chunjo's gate. Every bot asks the kingdom table instead
	// (GetPlayerBotFrontierArrivalFor), which puts each kingdom down where the
	// Teleporter puts its players. Both stand on open ground of the map's one
	// walkable area, measured on its server_attr.
	const long PLAYERBOT_FIRE_LAND_ARRIVAL_X = 597800;
	const long PLAYERBOT_FIRE_LAND_ARRIVAL_Y = 622200;
	const long PLAYERBOT_FIRE_LAND_EXIT_X = 596400;
	const long PLAYERBOT_FIRE_LAND_EXIT_Y = 620400;
	// Its weakest monster is 69; three under it is where the trip stops being
	// a waste, the rule the Forest's band was drawn by. Eight over its
	// strongest is where the Red Forest pays better.
	const BYTE PLAYERBOT_FIRE_LAND_MIN_LEVEL = 66;
	const BYTE PLAYERBOT_FIRE_LAND_MAX_LEVEL = 80;
	// The Demon Tower is not a frontier and has no hub table: a bot goes there
	// for the Biologist's level-50 specimen and comes back. 1001-1004 stand in
	// two clusters and this is the denser one.
	const long PLAYERBOT_DEMON_TOWER_ARRIVAL_X = 143400;
	const long PLAYERBOT_DEMON_TOWER_ARRIVAL_Y = 860100;
	const long PLAYERBOT_DEMON_TOWER_EXIT_X = 143400;
	const long PLAYERBOT_DEMON_TOWER_EXIT_Y = 860600;
	// 1001 is the weakest thing standing there.
	const BYTE PLAYERBOT_DEMON_TOWER_MIN_LEVEL = 57;
	const BYTE PLAYERBOT_HWANG_EAST_MIN_LEVEL = 55;

	// Where a frontier map is entered and where it is left, by map. Every
	// place that used to choose between the valley and the desert with a
	// ternary asks here instead, so a third and fourth map is a row, not a
	// sweep through the sources.
	bool GetPlayerBotFrontierArrival(long mapIndex, long& outX, long& outY)
	{
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_ORC_VALLEY: outX = PLAYERBOT_ORC_VALLEY_ARRIVAL_X; outY = PLAYERBOT_ORC_VALLEY_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_DESERT: outX = PLAYERBOT_DESERT_ARRIVAL_X; outY = PLAYERBOT_DESERT_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_SOHAN: outX = PLAYERBOT_SOHAN_ARRIVAL_X; outY = PLAYERBOT_SOHAN_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_SPIDER_V1: outX = PLAYERBOT_SPIDER_ARRIVAL_X; outY = PLAYERBOT_SPIDER_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_SPIDER_V2: outX = PLAYERBOT_SPIDER_V2_ARRIVAL_X; outY = PLAYERBOT_SPIDER_V2_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_HWANG: outX = PLAYERBOT_HWANG_ARRIVAL_X; outY = PLAYERBOT_HWANG_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_FOREST: outX = PLAYERBOT_FOREST_ARRIVAL_X; outY = PLAYERBOT_FOREST_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_RED_FOREST: outX = PLAYERBOT_RED_FOREST_ARRIVAL_X; outY = PLAYERBOT_RED_FOREST_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_DEMON_TOWER: outX = PLAYERBOT_DEMON_TOWER_ARRIVAL_X; outY = PLAYERBOT_DEMON_TOWER_ARRIVAL_Y; return true;
			case PLAYERBOT_MAP_FIRE_LAND: outX = PLAYERBOT_FIRE_LAND_ARRIVAL_X; outY = PLAYERBOT_FIRE_LAND_ARRIVAL_Y; return true;
			default: return false;
		}
	}

	bool GetPlayerBotFrontierExit(long mapIndex, long& outX, long& outY)
	{
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_ORC_VALLEY: outX = PLAYERBOT_ORC_VALLEY_EXIT_X; outY = PLAYERBOT_ORC_VALLEY_EXIT_Y; return true;
			case PLAYERBOT_MAP_DESERT: outX = PLAYERBOT_DESERT_EXIT_X; outY = PLAYERBOT_DESERT_EXIT_Y; return true;
			case PLAYERBOT_MAP_SOHAN: outX = PLAYERBOT_SOHAN_EXIT_X; outY = PLAYERBOT_SOHAN_EXIT_Y; return true;
			case PLAYERBOT_MAP_SPIDER_V1: outX = PLAYERBOT_SPIDER_EXIT_X; outY = PLAYERBOT_SPIDER_EXIT_Y; return true;
			case PLAYERBOT_MAP_SPIDER_V2: outX = PLAYERBOT_SPIDER_V2_EXIT_X; outY = PLAYERBOT_SPIDER_V2_EXIT_Y; return true;
			case PLAYERBOT_MAP_FOREST: outX = PLAYERBOT_FOREST_EXIT_X; outY = PLAYERBOT_FOREST_EXIT_Y; return true;
			case PLAYERBOT_MAP_RED_FOREST: outX = PLAYERBOT_RED_FOREST_EXIT_X; outY = PLAYERBOT_RED_FOREST_EXIT_Y; return true;
			case PLAYERBOT_MAP_DEMON_TOWER: outX = PLAYERBOT_DEMON_TOWER_EXIT_X; outY = PLAYERBOT_DEMON_TOWER_EXIT_Y; return true;
			case PLAYERBOT_MAP_HWANG: outX = PLAYERBOT_HWANG_EXIT_X; outY = PLAYERBOT_HWANG_EXIT_Y; return true;
			case PLAYERBOT_MAP_FIRE_LAND: outX = PLAYERBOT_FIRE_LAND_EXIT_X; outY = PLAYERBOT_FIRE_LAND_EXIT_Y; return true;
			default: return false;
		}
	}

	// The four maps a bot goes to for good once it has outgrown Bokjung. A
	// mission whose monster is not on one of these, while the bot is, will not
	// be hunted - the bot is not passing through.
	bool IsPlayerBotFrontierMapIndex(long mapIndex)
	{
		// The Demon Tower counts, because every road onto a map runs through
		// this predicate: the travel pass, the arrival and the party rules all
		// ask it, and a map that answers no is a map no bot ever walks onto. It
		// takes one draw in four from fifty-seven up and nothing more, and the
		// thing the operator actually asked to prevent - bots running the
		// dungeon for stones - is prevented where it belongs, in
		// PlayerBotMapHasMetinStones.
		return mapIndex == PLAYERBOT_MAP_ORC_VALLEY || mapIndex == PLAYERBOT_MAP_DESERT ||
				mapIndex == PLAYERBOT_MAP_DEMON_TOWER ||
				mapIndex == PLAYERBOT_MAP_SOHAN || mapIndex == PLAYERBOT_MAP_SPIDER_V1 ||
				mapIndex == PLAYERBOT_MAP_SPIDER_V2 || mapIndex == PLAYERBOT_MAP_HWANG ||
				mapIndex == PLAYERBOT_MAP_FOREST || mapIndex == PLAYERBOT_MAP_RED_FOREST ||
				mapIndex == PLAYERBOT_MAP_FIRE_LAND;
	}

	// Both Spider Dungeons: the ones reached across the desert and entered
	// from the Kuahlo gate, and the ones with no stones.
	bool IsPlayerBotSpiderMap(long mapIndex)
	{
		return mapIndex == PLAYERBOT_MAP_SPIDER_V1 || mapIndex == PLAYERBOT_MAP_SPIDER_V2;
	}

	const char* GetPlayerBotFrontierName(long mapIndex)
	{
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_ORC_VALLEY: return "orc_valley";
			case PLAYERBOT_MAP_DESERT: return "desert";
			case PLAYERBOT_MAP_SOHAN: return "sohan";
			case PLAYERBOT_MAP_SPIDER_V1: return "spider_v1";
			case PLAYERBOT_MAP_SPIDER_V2: return "spider_v2";
			case PLAYERBOT_MAP_HWANG: return "hwang";
			case PLAYERBOT_MAP_FOREST: return "forest";
			case PLAYERBOT_MAP_RED_FOREST: return "red_forest";
			case PLAYERBOT_MAP_DEMON_TOWER: return "demon_tower";
			case PLAYERBOT_MAP_FIRE_LAND: return "fire_land";
			default: return "frontier";
		}
	}
	// Ordinary spawns are levels 18-25 in Orc Valley and 26-30 in the Desert, but
	// the Metins tell a different story: 40/45/50 in the Desert and 45/48/50 in
	// Orc Valley. A stone is only worth breaking between stoneLevel-9 and
	// stoneLevel+10, so Orc Valley starts paying at level 36 and not before -
	// which is why it belongs to the high band even though its monsters do not.
	// Bokjung keeps everyone up to 29.
	const BYTE PLAYERBOT_DESERT_MIN_LEVEL = 30;
	// The desert reaches as far as its own monsters do, which is much further
	// than thirty-six.
	//
	// Counted out of metin2_map_n_desert_01/regen.txt: 14026 spawn points, more
	// than any other map this world hosts and nearly twice Orc Valley's 8122.
	// The Scorpion King at 39 alone stands in 2234 places, the Desert Flying Eye
	// of 37 in 1242, the Poison Spider of 45 in 1548, the Scorpion Archer of 47
	// in 876. Capping the map at thirty-six meant nobody hunted on it past that
	// - the whole band from thirty-six to forty-seven went to Orc Valley - and
	// the richest map in the game was a corridor people walked across on their
	// way to the Spider Dungeon, which is exactly what the panel showed.
	const BYTE PLAYERBOT_DESERT_MAX_LEVEL = 47;
	// One distance decides both halves of this: how far away counts as somewhere
	// else, and how far a forced march goes before the bot may settle again.
	// Twelve thousand is the spacing Orc Valley's hunting hubs were generated
	// at, so it means exactly "one hub over".
	//
	// It has to be that large. An earlier draft reset the anchor whenever the
	// bot strayed thirty metres, which would have let a bot circling one corner
	// of the map for an hour keep claiming it had moved.
	const int PLAYERBOT_RELOCATE_DISTANCE = 12000;
	const DWORD PLAYERBOT_CAMP_TIMEOUT = 300000;
	// How close counts as arrived, and the give-up. A route that cannot be
	// walked must not suppress combat for the rest of the bot's life; three
	// minutes is long enough to cross this delta and short enough that a bot
	// stuck against a wall goes back to hunting.
	const int PLAYERBOT_RELOCATE_ARRIVED = 2000;
	const DWORD PLAYERBOT_RELOCATE_TIMEOUT = 180000;

	const BYTE PLAYERBOT_ORC_VALLEY_MIN_LEVEL = 36;
	const BYTE PLAYERBOT_ORC_VALLEY_MAX_LEVEL = 55;
	// Orc Valley is three maps stacked by level, and the band above treated it
	// as one. The outer islands hold the Esoteric Fanatic (35) and Arahan (38)
	// - the wiki's marked spots, and what a player farms there from the
	// thirties. The three Black Orc camps at (601,625), (774,923) and (933,639)
	// are level 46 S_KNIGHT, a party's work at 40; the central island's
	// Tormentors (49) carry the Curse Book and are a party's work at 45. A bot
	// of 30-35 goes to the Fanatic islands or to the desert by the parity of
	// its pid, so the desert keeps its Black Wind hunters and the horse trial.
	const BYTE PLAYERBOT_ORC_VALLEY_ESOTERIC_MIN_LEVEL = 30;
	const BYTE PLAYERBOT_ORC_VALLEY_ESOTERIC_MAX_LEVEL = 39;
	const BYTE PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL = 40;
	const BYTE PLAYERBOT_ORC_VALLEY_CENTRE_MIN_LEVEL = 45;
	// A camp of level-46 knights is farmed by eight, not six; the engine's own
	// ceiling is PARTY_MAX_MEMBER. Anyone of party level on this map may join
	// one, not only the ten percent party cohort - eight of that cohort at the
	// same level on the same island is a thing that never happens.
	const int PLAYERBOT_ORC_VALLEY_PARTY_MAX = 8;
	// A guild mate counts for twice a remembered friend when a party is being
	// put together, and a leader with a guild picks the camp by the guild's id,
	// so one guild ends up on one camp. That is what the map looked like on the
	// servers this world imitates.
	const int PLAYERBOT_GUILD_PARTY_POINTS = 8;
	// What a Shaman is worth as a partner out on the frontier maps.
	//
	// A Shaman already buffs everyone in its party - Blessing, Dragon Aid,
	// Swiftness, Attack Up and a heal - and does it for nobody at all while it
	// hunts alone, which is how most of them hunt. On the Spider Dungeon and
	// the maps like it, where a bot is fighting monsters within a few levels of
	// itself and a great many do not survive it, one Shaman in the pair is the
	// difference between two characters that hold their ground and two that do
	// not. Worth more than a guild badge, which is why it outweighs it: the
	// guild is who a bot likes, this is who keeps it alive.
	const int PLAYERBOT_PARTY_SHAMAN_POINTS = 14;
	// And out there a bot is less inclined to go it alone. One in four kept to
	// itself everywhere, frontier included; on the frontier that is now one in
	// ten, so pairs actually form on the maps where they matter.
	const int PLAYERBOT_PARTY_SOLO_PERCENT = 25;
	const int PLAYERBOT_PARTY_SOLO_PERCENT_FRONTIER = 10;
	// What share of the population may be in a party at all, in thousandths,
	// at the neutral PARTY weight; the slider scales it - a twentieth at 25,
	// a half at 250. Until 2.0.18 the weight reached nothing but the planner's
	// ranking of the party challenge, and the cohort off the frontier was the
	// party-fighter role alone - a tenth of the population, drawn at login -
	// so "Grupy (PT)" at 25 and at 250 gave the same thirty-seven bots in
	// groups out of a thousand (jaksiezabic, 12 September). The role takes the
	// first hundred places of the draw (GetPlayerBotPartyDraw): the last share
	// the slider takes away and the first it gives back. On the frontier the
	// base is the whole map, as it always was - the camps and bosses there
	// are a party's work - so the neutral weight changes nothing there.
	const int PLAYERBOT_PARTY_COHORT_PER_MILLE = 200;
	const int PLAYERBOT_PARTY_FRONTIER_COHORT_PER_MILLE = 1000;
	// How far a follower may fall behind a leader who is walking to a new camp
	// before it gives the party up. The cohesion radius is for fighting as one
	// formation; a thirty-kilometre relocation with a deferred route in the
	// middle of it is not a reason to disband.
	const int PLAYERBOT_PARTY_STRAGGLER_RADIUS = 9000;

	// The population's memory of where the monsters are: a grid of cells this
	// wide per map, each remembering how many monsters were in reach when a bot
	// looked for something to hit there, and how often a fight started there.
	// Halved every ten minutes so a spot somebody cleared an hour ago is not
	// remembered as full. A hub with fewer looks than this is scored as an
	// average spot - four monsters in reach, which a rich camp beats easily
	// and a crowded one does not, so that unknown ground gets visited.
	const int PLAYERBOT_SPOT_CELL = 6400;
	const DWORD PLAYERBOT_SPOT_DECAY_INTERVAL = 600000;
	const DWORD PLAYERBOT_SPOT_MIN_SAMPLES = 12;
	const int PLAYERBOT_SPOT_UNKNOWN_PERMILLE = 4000;
	const int PLAYERBOT_SPOT_CROWD_RADIUS = 5000;
	// A hub this far away is worth half of one underfoot, and a hub once chosen
	// is kept for this long unless the bot is standing on it with nothing to
	// fight. The first version scored by share alone and re-chose on every
	// wander decision: bots crossed the valley for a slightly better camp,
	// then crossed back - 160 to 334 far plans a minute against 26 to 65
	// before, eight thousand deferrals, and the tick at 57 s of every 60.
	const int PLAYERBOT_HUB_HALF_WORTH_DISTANCE = 20000;
	const DWORD PLAYERBOT_HUB_STICK_TIME = 240000;
	// The Metin expedition. A quarter of the population are Metin hunters by
	// role and the rest broke a stone only when one stood in their way, which
	// is not what a player does: a player decides on an evening of stones and
	// roams the map for them. So every other bot rolls once an hour for a
	// stretch of exactly that - half an hour in which it plans, targets and
	// wanders like a hunter, then goes back to what it was. A quarter of the
	// rolls succeed, so at any moment about one grinder in eight is out for
	// stones; the METIN weight in the panel scales the chance. On the hunting
	// maps the expedition changes hub this often instead of every four
	// minutes, because a stone is found by covering ground.
	const DWORD PLAYERBOT_METIN_EXPEDITION_DURATION = 1800000;
	const DWORD PLAYERBOT_METIN_EXPEDITION_ROLL_INTERVAL = 3600000;
	const int PLAYERBOT_METIN_EXPEDITION_CHANCE_PERCENT = 25;
	const BYTE PLAYERBOT_METIN_EXPEDITION_MIN_LEVEL = 15;
	const DWORD PLAYERBOT_METIN_EXPEDITION_HUB_STICK = 90000;
	const DWORD PLAYERBOT_SPOT_REPORT_INTERVAL = 600000;

	// The Moonlight Treasure Chest and what comes out of it. A chest in the bag
	// is opened on the next pass; the boosters are drunk at the start of a
	// fight and refused by the engine while the last one still runs, so a
	// minute between attempts costs nothing and keeps the log readable.
	const DWORD PLAYERBOT_MOONLIGHT_CHEST_VNUM = 50011;
	// How many of one box a bot has to be holding before the surplus is goods
	// rather than its own supply. Suggested on the Discord: most of what drops
	// should still be opened - that is where the potions and the boosters come
	// from - but an unopened box is the one thing in this market a player can
	// gamble on, and there was never one on a counter.
	const DWORD PLAYERBOT_CHEST_STALL_MIN_STACK = 5;
	// A share of the population trades its resources instead of spending all
	// of them on itself. "Zaden bot nie sprzedaje szkat blasku i zwojow
	// blogoslawienstwa" (sizowski, 13 September), and his own proposal was a
	// proportion rather than a switch: "4 uzywaja do rozwijania postaci, 1
	// sprzedaje - jak prawdziwy gracz". So one bot in five is a trader, drawn
	// by pid the way the scrap keeper is, and the two roles are salted apart.
	// A trader still opens chests and still refines - it simply keeps a much
	// smaller reserve, so the surplus reaches a counter instead of the bag.
	const int PLAYERBOT_RESOURCE_TRADER_PERCENT = 20;
	// What a trader keeps back: two of a chest stack (against five) and one
	// safe refine scroll (against three).
	const DWORD PLAYERBOT_CHEST_TRADER_MIN_STACK = 2;
	const int PLAYERBOT_REFINE_SCROLL_TRADER_KEEP = 1;
	// How many Moonlight chests a trader holds unopened for its counter; past
	// that it opens them like everyone else, so a counter nobody buys from does
	// not fill its bag. Twenty put 3 000 chests on AkhiGubernator's counters in
	// six hours with not one sold (15 September): a trader shows a few, and the
	// rest are for opening.
	const int PLAYERBOT_CHEST_TRADER_HOLD = 6;
	// A bot buys a Moonlight chest off a counter to open it
	// (WantsPlayerBotMoonlightChest): from this level, while it holds fewer than
	// PLAYERBOT_CHEST_BUY_HOLD, with this many free cells and this much gold, and
	// into a bag that takes the chest's whole group. Nothing wanted one before,
	// so every chest a trader listed stayed listed.
	const int PLAYERBOT_CHEST_BUY_MIN_LEVEL = 20;
	const int PLAYERBOT_CHEST_BUY_HOLD = 10;
	const int PLAYERBOT_CHEST_BUY_MIN_FREE_CELLS = 10;
	const long long PLAYERBOT_CHEST_BUY_MIN_GOLD = 1000000LL;
	// ...and spare gold of this many times Iwakura's price for the chest, scaled
	// by the yang rate: the counter asks round that, up to twice it where the
	// ledger says the chests are short, and a bot sent to the market for a chest
	// it could not pay for would walk there for nothing.
	const long long PLAYERBOT_CHEST_BUY_PRICE_MULTIPLE = 3;
	// What the counters keep for the players: no bot buys a chest while the
	// ledger counts this many or fewer on every counter of the world, and a
	// bot that bought one waits this long before the next ("boty wykupuja
	// doslownie WSZYSTKIE bez opamietania", sizowski, 16 September).
	const DWORD PLAYERBOT_CHEST_MARKET_RESERVE = 30;
	const DWORD PLAYERBOT_CHEST_BUY_COOLDOWN = 20 * 60 * 1000;
	// A counter line of Moonlight chests is a pack of this many, cut off the
	// stack (BotOfflinePrepareLine). A stack went up whole, and the counters of
	// the test world carried 22 lines of eleven to thirty chests that no
	// buyer's cap reached (15 September).
	const int PLAYERBOT_CHEST_LINE_UNITS = 5;
	// ...and no more than this many such lines stand on one counter.
	const int PLAYERBOT_CHEST_COUNTER_LINES = 3;
	// A dropper picks the chests up and sells them rather than opening them
	// ("dropki medali nie podnosza szkat. blasku", darkroom22; "dodaj im
	// mozliwosc podnoszenia tego i dawania na sklep", Tieru, 15 September). It
	// keeps this many unopened for its counter and opens the rest, so a counter
	// it seldom serves - a medal dropper's is served out of its dungeon only -
	// does not fill its bag.
	const int PLAYERBOT_CHEST_DROPPER_HOLD = 30;
	// The engine's bag page: INVENTORY_PAGE_COLUMN x INVENTORY_PAGE_ROW on both
	// lines. A giftbox wants three free cells in one column of one page.
	const int PLAYERBOT_BAG_PAGE_COLUMNS = 5;
	const int PLAYERBOT_BAG_PAGE_ROWS = 9;
	// The Forgetting Scroll (ITEM_SKILLFORGET): one level off a skill and the
	// point back. A skill that reached seventeen without turning Master is
	// left there rather than pushed on - every further point is a point the
	// bot never gets back - and a scroll from a counter buys another roll.
	// An armour piece more than this many levels below the bot is outgrown and
	// loses this much score per level past that, so a tier-appropriate piece
	// at a low refine displaces the starter piece at +6 and gets refined.
	const int PLAYERBOT_ARMOR_OUTGROWN_LEVELS = 20;
	// ...as a share of its defence figure per level past that, compounded:
	// each level keeps this much less of what the level before kept, so no
	// piece drops to nothing and a higher tier keeps more. It was a flat
	// fifteen hundred a level, which took the bonus lines with it: a level-18
	// plate rolled with fifteen hundred health lost at fifty to a dragon armour
	// with seven more defence and nothing else. Then it was this much a level
	// up to all of it, and a bot of seventy-four found every armour of level
	// thirty-four and below worth the same single point.
	const long long PLAYERBOT_ARMOR_OUTGROWN_PERCENT_PER_LEVEL = 5;
	const DWORD PLAYERBOT_SKILL_FORGET_SCROLL_VNUM = 70037;
	// Moving a point from a skill the priority list ranks lower to the one it
	// wants next: one Forgetting Book per point, at this price and this pace
	// (sosen: a bot with sixteen in Tapniecie put its new points into Duchowe
	// and left the sixteen where they were). Cheaper than the "stuck at
	// seventeen" book above because it runs from level five, on M1 purses.
	const long long PLAYERBOT_SKILL_REALLOCATE_PRICE = 20000;
	const DWORD PLAYERBOT_SKILL_REALLOCATE_INTERVAL = 30000;
	// No merchant in this world sells the scroll and nothing drops it, so a
	// bot past the old woman's thirty bought it nowhere and a skill stuck at
	// seventeen stayed there for life - 81 bots carried a skill at eighteen or
	// nineteen from before the cap. Above PLAYERBOT_SKILL_RESET_MAX_LEVEL the
	// bot buys one at the item shop's kind of price, straight into the bag,
	// and reads it on the spot; below that level the old woman is cheaper.
	const long long PLAYERBOT_SKILL_FORGET_SCROLL_PRICE = 200000;
	const long long PLAYERBOT_SKILL_FORGET_SCROLL_GOLD_MARGIN = 300000;
	// Scrap keepers: the share of stall keepers (percent, from the panel) that
	// put their low refines on the counter instead of vendoring them, for the
	// player who wants cheap fodder to burn at the blacksmith. A keeper stops
	// hoarding scrap when its bag is down to this many free cells.
	const int PLAYERBOT_SCRAP_KEEP_FREE_CELLS = 20;
	const DWORD PLAYERBOT_SCRAP_PRICE_MULT = 2;
	// After a Metin stone breaks its drops lie in a ring round it, and the
	// pack it summoned is still on the bot. For this long the bot goes for its
	// own drops within this reach anyway - the way a player dashes for them -
	// rather than leaving them to whoever is not fighting.
	const DWORD PLAYERBOT_METIN_LOOT_DASH_TIME = 20000;
	const int PLAYERBOT_METIN_LOOT_DASH_RANGE = 1500;
	// An archer pulls too, but a bow is not a shield: one group, four attackers.
	const int PLAYERBOT_MULTI_PULL_ARCHER_MAX_AGGRESSORS = 4;
	const BYTE PLAYERBOT_SKILL_MASTER_TRY_LEVEL = 17;
	// How long a bot keeps farming its class's level-30 weapon before giving
	// the map up for good. See ShouldPlayerBotVisitM3.
	const BYTE PLAYERBOT_LEVEL30_WEAPON_HUNT_MAX_LEVEL = 40;
	// The old woman south of Joan, and what she does.
	//
	// skill_reset2.quest, NPC 9006: refuses under level five and over thirty,
	// refuses a character with no skill group, charges 10000 + level * 2000,
	// then pc.clear_skill() and pc.set_skill_group(0) - which is exactly what
	// the trainer visit already knows how to follow, because a group of zero is
	// what sends a bot to the trainer in the first place. So the whole feature
	// is one town errand and no new machinery.
	//
	// It is worth doing only for a bot that has run out of moves: a skill at
	// seventeen that will not go Master and no skill points left to put
	// anywhere. Resetting costs every skill level the character has, so a bot
	// with points still in hand should spend those first.
	const long PLAYERBOT_SKILL_RESET_NPC_X = 58800;   // npc.txt cell 588,633 on
	const long PLAYERBOT_SKILL_RESET_NPC_Y = 165700;  // map 21, base (0,102400)
	const BYTE PLAYERBOT_SKILL_RESET_MIN_LEVEL = 5;
	const BYTE PLAYERBOT_SKILL_RESET_MAX_LEVEL = 30;
	const long long PLAYERBOT_SKILL_RESET_BASE_COST = 10000;
	const long long PLAYERBOT_SKILL_RESET_LEVEL_COST = 2000;
	// A wallet cushion, so a reset never leaves a bot unable to buy potions.
	const long long PLAYERBOT_SKILL_RESET_GOLD_MARGIN = 100000;
	const DWORD PLAYERBOT_SKILL_RESET_COOLDOWN = 1800000;   // 30 min between tries
	const DWORD PLAYERBOT_CHEST_INTERVAL = 8000;
	// How long a box the engine has refused is left alone. A refusal can be
	// a bag that happened to be full, so it is a wait rather than a verdict.
	// Wolnych pol, ktore musza byc, zanim bot otworzy skrzynie.
	//
	// GetEmptyInventory(n) zwraca POZYCJE wolnego miejsca na przedmiot o tej
	// wysokosci, a nie ich liczbe - dwa wywolania obok siebie moga wskazac to
	// samo pole i nie rezerwuja niczego. A GiveItemFromSpecialItemGroup wydaje
	// nagrody po kolei przez AutoGiveItem, ktory przy braku miejsca kladzie
	// przedmiot na ziemi i zglasza to jako sukces - wiec nagroda po prostu
	// znikala. Zgloszone jako "przedmioty ze skrzyn wypadaja na ziemie".
	//
	// To zabezpieczenie, nie rozwiazanie: prawdziwa naprawa to wylosowac zestaw
	// raz, sprawdzic miejsce na CALY zestaw i dopiero potem zuzyc skrzynie -
	// a to zmiana w silniku, ktora musi dostac wlasny tryb wydania, zeby nie
	// ruszac zachowania nagrod graczy. Piec pol pokrywa kazdy zestaw, jaki
	// nasza grupa Moonlight potrafi wylosowac (jedna linia na skrzynie), i
	// wiekszosc skrzyn bossow.
	const int PLAYERBOT_CHEST_FREE_CELLS = 5;
	const DWORD PLAYERBOT_CHEST_REFUSED_RETRY = 600000;
	// A Moonlight chest the engine refused is asked for again after a minute:
	// its group always fits a bag that takes it, so a refusal is a busy moment,
	// and ten minutes of it kept stacks of twenty-eight unopened in bags with
	// sixty-nine free cells (LordMicro, 15 September).
	const DWORD PLAYERBOT_CHEST_MOONLIGHT_REFUSED_RETRY = 60000;
	const DWORD PLAYERBOT_BOOSTER_INTERVAL = 60000;
	// How many of one booster a bot keeps when nobody else can have it. A
	// booster that may go neither to a merchant (ANTI_SELL) nor on a counter
	// (ANTI_MYSHOP) - the Dlonie of the Moonlight chest - is worth only what
	// its holder drinks, ten minutes at a time. Past this the merchant visit
	// throws the rest away, or three stacks of two hundred fill a bag the bot
	// can then no longer loot into ("dlonie przebicia i krytyki zalegaja w eq
	// w 3 stakach po 200", uxietoszef).
	const int PLAYERBOT_BOOSTER_KEEP_PER_VNUM = 100;
	// The chest's two boosters, and the two grilled fish that work the same
	// way: a Carp for twenty movement speed, a Rudd for ten dexterity, ten
	// minutes each (item_proto USE_ABILITY_UP).
	const DWORD PLAYERBOT_BOOSTER_VNUMS[] = { 71044, 71045, 27866, 27873 };
	// The polymorph marbles, as ItemProcess_Polymorph names them. Nothing else
	// this world calls ITEM_POLYMORPH takes that branch, and the branch is what
	// gives the transformation its damage bonus (200 + skill, five minutes with
	// no Polymorph skill), so a marble outside this list would be spent for
	// nothing. The monster is in socket 0 and the engine refuses one whose
	// level is at or above the bot's own plus MAX(0, 20 - level*3/10) - which is
	// why a marble is worth keeping for a boss rather than burning on a pack.
	const DWORD PLAYERBOT_POLYMORPH_MARBLE_VNUMS[] = { 70104, 70105, 70106, 70107, 71093 };
	// A transformation is spent on something that takes a while to kill. The
	// engine refuses every skill while polymorphed (char_skill.cpp), so this is
	// a trade: the marble's damage bonus against the whole rotation, and it is
	// only worth it where the rotation is not what wins the fight anyway.
	const int PLAYERBOT_POLYMORPH_BOSS_HP_PERCENT = 90;
	const DWORD PLAYERBOT_POLYMORPH_RETRY_MS = 60000;
	// Since 2.0.27 a booster is recognised by what the engine does with it, not
	// by its vnum: USE_AFFECT with value0 510 is the timed stat buff (attack
	// +10/+15, speed, critical, penetration, the Dragon God set, the experience
	// ring...) and USE_ABILITY_UP the shorter one (green/purple potions, juices,
	// sushi). The list above is only the order the chest boosters come in. The
	// ItemShop copies (76xxx) carry no ANTI_SELL, so a bot handed a Mikstura
	// Ataku +10 from the panel vendored it ("Bot zamiast uzyc i dodac bony to
	// posprzedawal handlarzowi", Pasywny, 13 September).
	const int PLAYERBOT_USE_AFFECT_TIMED_BUFF = 510;
	// Eliksir Slonca and Eliksir Ksiezyca are the engine's auto potions, not
	// experience: ITEM_AUTO_HP_RECOVERY_* and ITEM_AUTO_SP_RECOVERY_* with their
	// reward-box and Brazil copies (unique_item.h, the same on both engines).
	// A use switches a standing recovery affect on - or off, when that affect is
	// already running - and an empty one (socket 1 equal to socket 2) only says
	// AUTOPOTION_IS_EMPTY and still counts as a successful use. This list used
	// to call them experience elixirs, drunk "on sight" on every tick outside a
	// fight, and every bot carried one empty 76004 from the apprentice chest: the
	// whole population used it about once a second, 580 000 log lines an hour on
	// one core, and nothing gained. 39037-39042 share the names and no engine
	// handles them, so they are not here.
	const DWORD PLAYERBOT_AUTO_HP_POTION_VNUMS[] = { 72723, 72724, 72725, 72726, 76021, 76022, 79012 };
	const DWORD PLAYERBOT_AUTO_SP_POTION_VNUMS[] = { 72727, 72728, 72729, 72730, 76004, 76005, 79013 };
	// Once a minute per bot is plenty for something that, once on, stays on.
	const DWORD PLAYERBOT_AUTO_POTION_INTERVAL = 60000;
	// The Demon Tower's own stones, 8015-8019 (Metin Twardosci to Metin
	// Morderstwa), are quest triggers rather than loot. deviltower_zone answers
	// the entry stone's kill with a six-second timer and d.new_jump_all(66), and
	// CDungeon::JumpAll carries that out on the map the killer stands on when the
	// timer fires: every PC there is warped into a new tower. A bot that broke it
	// took the tower's other bots along (nine WarpSets to 660000 at 14:17:11 on
	// the test world, all back out through the sectree rescue), and a bot that
	// left for its village inside those six seconds takes the village instead -
	// which is the shape of "stalem afk pod lochem malp w m2, gdy nagle
	// przeteleportowalo mnie do DT" (sizowski, 14 September). The other four
	// stand only inside the instance. None of them is a bot's to break on its
	// own; climbing with a player they are the floor's objective, no level band
	// (IsPlayerBotDungeonStoneObjective in playerbot_movement.h).
	const DWORD PLAYERBOT_DEVIL_TOWER_STONE_FIRST = 8015;
	const DWORD PLAYERBOT_DEVIL_TOWER_STONE_LAST = 8019;
	bool IsPlayerBotDungeonTriggerStone(DWORD race)
	{
		return race >= PLAYERBOT_DEVIL_TOWER_STONE_FIRST && race <= PLAYERBOT_DEVIL_TOWER_STONE_LAST;
	}
	// A Demon Tower instance: the map's own index times ten thousand plus a
	// serial, the copy the quest makes when the stone breaks.
	bool IsPlayerBotDemonTowerInstance(long lMapIndex)
	{
		return lMapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN &&
				lMapIndex / 10000 == PLAYERBOT_MAP_DEMON_TOWER;
	}
	// The tower's keys a bot carries to where they are used
	// (playerbot_demon_tower.h); the fake Bong-In key is not one of them.
	bool IsPlayerBotDemonTowerKey(DWORD vnum)
	{
		return vnum == PLAYERBOT_TOWER_OPENING_STONE || vnum == PLAYERBOT_TOWER_CHEST_ITEM ||
				vnum == PLAYERBOT_TOWER_MAP_ITEM || vnum == PLAYERBOT_TOWER_KEY_ITEM;
	}
	// How far round a splash skill's caster and its target a Demon Tower stone
	// is looked for before the skill is cast: the skill's own splash range when
	// it has one, this when it does not, plus a margin for a stone at the edge
	// of the blow.
	const int PLAYERBOT_SPLASH_STONE_DEFAULT_RANGE = 1000;
	const int PLAYERBOT_SPLASH_STONE_MARGIN = 500;
	// How far outside its own map a walk may still be asked for before it is
	// taken for another map's coordinates and refused: a random step or a
	// snapped goal a little past the edge still reaches ClampWorld, a point in
	// Orc Valley asked for in Bokjung does not.
	const long PLAYERBOT_NAV_OFF_MAP_MARGIN = 6400;
	// Wykrywacz Kamieni Metin: useless to a bot (it draws on a client), wanted
	// by players - counter goods, never merchant scrap.
	const DWORD PLAYERBOT_METIN_DETECTOR_VNUMS[] = { 27989, 76006 };
	// Fishing, the rest of the chain. A dead fish is grilled on a campfire:
	// the Dried Wood (27600, from the Fisherman) burns for forty seconds as a
	// campfire mob (12000) and takes fish handed to it - alive or dead - and
	// hands back the grilled kind. The grilled potions (Crucian 180, Big
	// Crucian 350, Tenchi 230 at once; Mandarin Fish 180 SP, Catfish 500 SP)
	// go into the potion lists.
	const DWORD PLAYERBOT_CAMPFIRE_VNUM = 27600;
	// Dead fish kept for the campfire; the rest is the merchant's. Kept without
	// a bound, 161 bots of the test world carried 932 dead carp, and one bag's
	// second page was fish (Tieru, 15 September).
	const int PLAYERBOT_DEAD_FISH_KEEP = 10;
	const DWORD PLAYERBOT_CAMPFIRE_MOB_VNUM = 12000;
	const DWORD PLAYERBOT_BAKE_WINDOW = 35000;
	// The race histogram a bot keeps of what it has been fighting: one slot per
	// EPlayerBotRaceSlot, halved every ten minutes, and trusted over the map's
	// own table once it holds this many. Human is in it because all three second
	// villages are a hundred percent human and it used not to be counted at all;
	// so is OTHER, which is every fight that paid no race, because a bot on the
	// desert has to be able to conclude that the answer is none.
	const int PLAYERBOT_RACE_HISTOGRAM_SLOTS = PLAYERBOT_RACE_SLOTS;
	const DWORD PLAYERBOT_RACE_HISTOGRAM_DECAY = 600000;
	const DWORD PLAYERBOT_RACE_HISTOGRAM_MIN_SAMPLES = 20;
	// How much of a bot's fighting a race has to be before a line against it is
	// worth anything. It used to be half, which is right for a map that is one
	// race and wrong for Mount Sohan: the undead are 46% of it and the other 54%
	// is ice, a race no item can be strong against, so "half must agree" threw
	// away the only line that works there.
	const int PLAYERBOT_RACE_WORTH_PERCENT = 25;
	// A hub where a wanted material has been seen to drop is worth half as
	// much again to a bot short of it; a cell with this many fights and no
	// drop of it has told the bot all it needs to know.
	const int PLAYERBOT_SPOT_MATERIAL_BONUS_PERCENT = 50;
	const DWORD PLAYERBOT_SPOT_MATERIAL_BARREN_FIGHTS = 200;
	// Thirty was a batch no bot ever reached: the whole world held 39 dead fish
	// between all its anglers, so the wood was never bought, no fire was ever
	// lit and not one grilled fish existed. A session brings home a handful,
	// and a fire takes any number of them.
	const int PLAYERBOT_BAKE_MIN_FISH = 5;
	const int PLAYERBOT_BAKE_RANGE = 700;
	const DWORD PLAYERBOT_GRILLED_FISH_FIRST_VNUM = 27863;
	// 27883, not 27876. The grilled fish run to Pieczony Zloty Karas, and the
	// seven above the old ceiling are the good ones - the speed and dexterity
	// buffs and the 1800-second Golden Crucian - so every one of them fell
	// through to the junk rule's `return true` and was vendored for pennies.
	const DWORD PLAYERBOT_GRILLED_FISH_LAST_VNUM = 27883;
	// What a shellfish holds, from the engine's own table (char_item.cpp,
	// case 27987): half a Stone Piece, thirty percent nothing, then a white,
	// a blue or a blood pearl. Thousandths. Once the population has opened
	// enough of them, its own count replaces the table.
	const DWORD PLAYERBOT_STONE_PIECE_VNUM = 27990;
	// What a shell actually holds, read out of the engine rather than guessed.
	//
	// char_item.cpp case 27987 rolls 1..100: at or under 50 a Stone Piece, and
	// the rest goes through one of two tables chosen by g_iUseLocale -
	// {80,90,97} when it is false and {95,97,99} when it is true. This world's
	// common.locale says "english", and __LocaleService_Init_English sets
	// g_iUseLocale = TRUE, so the second table is the live one: 45% nothing,
	// 2% white, 2% blue, 1% blood. The numbers here said 10/7/3 - the other
	// table - which made opening a shell look four times more rewarding than it
	// is, and every decision built on that estimate was wrong in the same
	// direction. A server that changes locale changes this; check the flag
	// before trusting the constants.
	const int PLAYERBOT_SHELLFISH_STONE_PERMILLE = 500;
	const int PLAYERBOT_SHELLFISH_WHITE_PERMILLE = 20;
	const int PLAYERBOT_SHELLFISH_BLUE_PERMILLE = 20;
	const int PLAYERBOT_SHELLFISH_RED_PERMILLE = 10;
	const DWORD PLAYERBOT_SHELLFISH_LEARN_SAMPLES = 50;
	// The Blessing Scroll (CHUKBOK_SCROLL to the engine): a refine that fails
	// under it drops the item one level instead of destroying it, at the
	// table's own odds. The blacksmith without one removes the item on every
	// failure - 1584 pieces in one afternoon. Scrolls come from the chest and
	// are scarce, so they are spent where a failure costs most: from +6 up.
	const DWORD PLAYERBOT_BLESSING_SCROLL_VNUM = 25040;
	// Zwoj Boga Smokow (YONGSIN_SCROLL, value0 = 2): three vnums carry it in
	// this world. The operator wants it used ahead of the Blessing Scroll
	// from this plus up. For the record, the engine's table for it
	// (char_item.cpp, hyuniron_prob) is 25% at +7 and 20% at +8 against the
	// blacksmith's 40 and 30, which the Blessing Scroll keeps; both hand the
	// piece back a level down on failure. Measured, not assumed.
	const DWORD PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[] = { 39022, 71032, 76009 };
	// Every scroll DoRefineWithScroll knows, by the engine's own switch:
	// Blessing (25040), Magic Stone (25041, 39001), Blacksmith's Manual
	// (39007, 70039), War God (39014, 71021), Dragon God (39022, 71032,
	// 76009). None of them is merchant scrap. The junk rule had no branch for
	// them and the merchant pays pennies for the one thing a refine above +6
	// cannot be done without: 471 Blessing Scrolls went over that counter in
	// a single day (jaroszv2), while the weapons they were meant for waited.
	bool IsPlayerBotRefineScroll(DWORD vnum)
	{
		switch (vnum)
		{
			case 25040: case 25041: case 39001: case 39007: case 39014:
			case 39022: case 70039: case 71021: case 71032: case 76009:
				return true;
#if defined(PLAYERBOT_ENGINE_MT2009)
			// mt2009's own set (world.item_proto, USE_TUNING): Magiczny Kamien
			// 25042, Podrecznik Kowala 25043, Zwoj Wojny 25044, Zwoj Boga Smokow
			// 25045 and the four Gwarancje 25051-25054.
			case 25042: case 25043: case 25044: case 25045:
			case 25051: case 25052: case 25053: case 25054:
				return true;
#endif
			default:
				return false;
		}
	}
	const BYTE PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS = 7;
	// The operator's word on an item, from playerbot_item_policy.tsv in the
	// spool (playerbot_config.h reads it like the weights): keep - never
	// leaves the bag; stall - counter goods, ahead of everything the scorer
	// would rank it; merchant - scrap for the general merchant; drop - thrown
	// away at the merchant visit without a sale. A line names a vnum or a
	// whole type (type:19). "Boty sprzedaja ulepszacze i marmury handlarzowi"
	// (sizowski, 12 September) is what the file is for: the rules below are
	// the defaults, the file is the operator's override.
	enum EPlayerBotItemPolicy
	{
		PLAYERBOT_ITEM_POLICY_NONE = 0,
		PLAYERBOT_ITEM_POLICY_KEEP = 1,
		PLAYERBOT_ITEM_POLICY_STALL = 2,
		PLAYERBOT_ITEM_POLICY_MERCHANT = 3,
		PLAYERBOT_ITEM_POLICY_DROP = 4
	};
	const int PLAYERBOT_SHOP_POLICY_STALL_SCORE = 900;
	// A polymorph marble is goods, not scrap: it went to the merchant for
	// three hundred yang while the counters sold none. But it is the goods
	// that sell least of all: 6 581 marble lines stood on 1 033 of m2zip's
	// counters on 18 September, up to thirty-one on one, and two days of logs
	// held not one sale of a marble against 792 of a recipe material and 501
	// of a book - while 65 of Bokjung's 85 counters had no cell left for the
	// materials that village had none of. So a marble goes up after the
	// materials and the books, and a counter shows
	// PLAYERBOT_SHOP_MARBLE_LINES of them, never two of one monster; the rest
	// are the merchant's under bag pressure (IsPlayerBotJunkItem).
	const int PLAYERBOT_SHOP_POLYMORPH_SCORE = 380;
	// A bonus stone over the keep: worth a counter slot ahead of a refine
	// material (500), which is the only thing that outranked it and the
	// reason the first build listed none at all - a keeper adds one line a
	// visit, and while it holds any material at all the stone waits behind
	// it for ever. Materials have a market that works (792 sales in two
	// days); the stones had 20 387 units in bags and not one on a counter.
	// The per-vnum line cap is what bounds it from here.
	const int PLAYERBOT_SHOP_BONUS_STONE_SCORE = 520;
	const int PLAYERBOT_SHOP_MARBLE_LINES = 3;
	// And no counter carries more than PLAYERBOT_SHOP_SAME_VNUM_LINES lines of
	// one item. The caps above were each for a kind - a material, a heap, the
	// chests, the scrolls, the marbles - and nothing else had one, while the
	// lines that went up before a cap existed never came down: on 18 September
	// one of m2zip's counters carried 46 lines of Kawalek Lodu, others ten to
	// fourteen of one hair dye or seventeen horse medals, and some 10 800 lines
	// stood over three of one item ("caly sklep jest w matowych lodach",
	// Tieru). The books and the soul stone keep their own cap by skill
	// (IsPlayerBotCountedSingleGoods), a Forgetting Scroll is one skill's, and
	// the operator's "stall" is never second-guessed.
	const int PLAYERBOT_SHOP_SAME_VNUM_LINES = 3;
	// The goods a player crafts or refines further (IsPlayerBotPickupGoods):
	// beside the materials, over the chests and the spare gear.
	const int PLAYERBOT_SHOP_PICKUP_GOODS_SCORE = 520;
	// What Iwakura's sheet prices and no rule of its own placed
	// (IsPlayerBotSheetGoods): the horse and polymorph books and the stone
	// scroll the merchant used to take. Beside a polymorph marble.
	const int PLAYERBOT_SHOP_SHEET_GOODS_SCORE = 600;
	// Offsets tried for a pitch the bot cannot walk to before the stand is
	// put off for a while (the open pass in playerbot_town.h).
	const int PLAYERBOT_SHOP_PITCH_TRIES = 4;
	const BYTE PLAYERBOT_SCROLL_REFINE_MIN_PLUS = 6;
	// A Blessing or Dragon God scroll in the bag is the whole reason to go
	// on: under either the engine never burns the piece (DoRefineWithScroll
	// hands it back a level down, or unchanged), so the personality's fear
	// of +7 no longer applies. Six bots in ten aimed at +6 and stopped there
	// with scrolls in the bag, and the scrolls went up on the counters
	// instead - "mnostwo zwojow na serwerze, a boty ich nie uzywaja". With a
	// scroll GetPlayerBotRefineTarget says this; without one, the old
	// ambition. The first PLAYERBOT_REFINE_SCROLL_KEEP scrolls stay off
	// the counter while a worn piece can still use one.
	const BYTE PLAYERBOT_SCROLL_REFINE_MAX_PLUS = 9;
	const int PLAYERBOT_REFINE_SCROLL_KEEP = 3;

	// The two scrolls the bots refine under: neither burns the piece.
	bool IsPlayerBotSafeRefineScroll(DWORD vnum)
	{
		if (vnum == PLAYERBOT_BLESSING_SCROLL_VNUM)
			return true;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// On mt2009 only the Gwarancja (REFINE_BONUS_SCROLL) burns what it
		// fails. 25041 is a second Blessing Scroll there (value0 0), 25042 the
		// Magic Stone that keeps the level (value0 1), 25043/70039 and 25045 a
		// Blessing with fifteen and ten percent on top (value1). The War God
		// scroll stops at +4 and this count is a ladder to +9, so it is not here.
		if (vnum == 25041 || vnum == 25042 || vnum == 25043 || vnum == 25045 || vnum == 70039)
			return true;
#endif
		for (size_t i = 0; i < sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS) / sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[0]); ++i)
			if (vnum == PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[i])
				return true;
		return false;
	}
	const DWORD PLAYERBOT_SCROLL_REFINE_INTERVAL = 45000;
	// Neither map sells anything, so a visit is bounded and ends in Bokjung.
	const DWORD PLAYERBOT_FRONTIER_MAX_VISIT_TIME = 2400000;
	// ...but it also has to start. Without a floor the bot re-evaluated its needs
	// on the very first tick after arriving, decided it wanted a shop, and turned
	// straight back around: 271 arrivals on the Desert produced 269 departures,
	// several of them within six seconds, and both maps looked empty because
	// every bot on them was mid-bounce.
	const DWORD PLAYERBOT_FRONTIER_MIN_VISIT_TIME = 120000;
	// Bokjung's market strip. Anchored on the coordinate the return-from-M3 leg
	// already uses, so it is known walkable; each keeper gets a stable offset so
	// the stalls line up instead of stacking on one pixel.
	const long PLAYERBOT_M2_MARKET_X = 145500;
	const long PLAYERBOT_M2_MARKET_Y = 240000;
	const int PLAYERBOT_MARKET_SPREAD = 300;
	// Town legs stop when they are close enough, not on the exact pixel. 220 was
	// tight enough that keepers kept walking around their pitch without ever
	// counting as arrived.
	const int PLAYERBOT_MARKET_ARRIVE = 450;
	// Joan's stall circle, around the village guard. The guard is NPC 11002 and
	// map data puts him in cell (633,640); metin2_map_b1's BasePosition is
	// (0,102400) and world = base + cell*100, which is the same arithmetic that
	// gives PLAYERBOT_M1_TELEPORTER its (51900,153600) from cell (519,512). He
	// stands alone - no other NPC within forty cells - so a ring of stalls round
	// him blocks nobody.
	// Cell (634,639) of metin2_map_b1, whose BasePosition is (0,102400): world =
	// base + cell*100.
	const long PLAYERBOT_M1_GUARD_X = 63400;
	const long PLAYERBOT_M1_GUARD_Y = 166300;
	// After the Teleporter refuses a bot for want of yang, how long before
	// it asks again. It asked on every tick before: one bot of fifty-eight
	// with 799 yang against an 11 000 fee was refused 24 000 times a minute,
	// and its status said "Ide na Gore Sohan" all the while. The wait is
	// what lets the town visit and the stall run and earn the fee.
	const DWORD PLAYERBOT_TELEPORTER_RETRY_MS = 300000;
	// The Teleport Ring (70058, level 30): a bot out of potions or a weapon
	// on a frontier map recalls home with it instead of the long, dangerous
	// walk to the exit portal ("musza isc po potki bo sie skonczyly a sa w
	// Dolinie Orkow czy na V1", Tieru). Not consumed; a per-bot clock keeps
	// it to the engine's own 30-minute cooldown.
	const DWORD PLAYERBOT_TELEPORT_RING_VNUM = 70058;
	const BYTE PLAYERBOT_TELEPORT_RING_MIN_LEVEL = 30;
	const DWORD PLAYERBOT_TELEPORT_RING_COOLDOWN_MS = 1800000;
	// A departure held longer than this is reported with what holds it
	// (PLAYERBOT_DEPARTURE: overdue), once per this interval per bot.
	const DWORD PLAYERBOT_DEPARTURE_OVERDUE_MS = 600000;
	// A bot whose next hunting ground lies behind the Teleporter keeps this
	// many fares out of every discretionary purchase, and one that cannot pay
	// the fare may hunt in Bokjung - past the cohort ceiling - until it holds
	// this many. Measured before this: 268 of 362 bots of 40+ in Bokjung held
	// less than one fare, 79 yang the poorest, and the Teleporter was asked
	// 26 000 times a minute by bots that could neither pay nor earn.
	const int PLAYERBOT_TELEPORTER_FARE_RESERVE_COUNT = 3;
	// How long Bokjung's counters are worth a look after Joan had nothing. Long
	// enough that a bot which crossed for nothing is not sent straight back,
	// short enough that Joan stays the first stop.
	const DWORD PLAYERBOT_MARKET_M2_FALLBACK = 600000;
	// Counters standing in Bokjung right now. Recounted by the market ledger
	// once a minute and incremented the moment one opens, so a burst of
	// keepers in the same minute cannot walk past the cap together. A stale
	// count can only be too high, which errs towards sending a keeper to Joan.
	int s_iPlayerBotStallsInM2 = 0;
	// Counters standing on each map right now, kept the same way. What it
	// answers is "is there a market here at all": on a young world nothing
	// can open a stall (mt2009 asks for level fifteen and eight hundred
	// kills), and a bot that set off to browse counters that did not exist
	// was reported as "jakie stragany ogladaja jak zadnego nie ma".
	std::map<long, int> s_mapPlayerBotStallsByMap;

	int GetPlayerBotStallsOnMap(long lMapIndex)
	{
		std::map<long, int>::const_iterator it = s_mapPlayerBotStallsByMap.find(lMapIndex);
		return it == s_mapPlayerBotStallsByMap.end() ? 0 : it->second;
	}
	// The two channels with moves (playerbot_channel_rules.h). A bot on the
	// second channel with business at a shop asks the coordinator to be moved
	// to the shop channel: it asks again this often while it waits, holds in
	// town at most this long (the stability window and a margin - a bot that
	// cannot be moved soon goes back to its life with its request queued),
	// retries a refused errand after this, and a buyer asks at most this often.
	const DWORD PLAYERBOT_SHOP_CHANNEL_REQUEST_RETRY_MS = 120000;
	const DWORD PLAYERBOT_SHOP_CHANNEL_REQUEST_REFRESH_MS = 30000;
	const DWORD PLAYERBOT_SHOP_CHANNEL_WAIT_TIMEOUT_MS = 75000;
	const DWORD PLAYERBOT_SHOP_CHANNEL_BUY_REQUEST_GAP_MS = 120000;
	// A stand that still sells, its owner on the other channel: the owner asks
	// to be moved for a service this long after arriving there, plus up to the
	// spread by pid - the far round of PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS,
	// because a move between channels costs more than a map change.
	const DWORD PLAYERBOT_SHOP_CHANNEL_SERVICE_MIN_MS = 45 * 60 * 1000;
	const DWORD PLAYERBOT_SHOP_CHANNEL_SERVICE_SPREAD_MS = 30 * 60 * 1000;
	// The machinery's clocks: where every bot of a core is, every ten seconds;
	// the table read back every five; the requests sent in one statement every
	// two; the coordinator's census every five, acting at most once a gate. A
	// statement past PLAYERBOT_CHANNEL_MAX_QUEUED waits for the database thread
	// to catch up rather than pile up behind it.
	const DWORD PLAYERBOT_CHANNEL_PRESENCE_INTERVAL = 10000;
	const DWORD PLAYERBOT_CHANNEL_REFRESH_INTERVAL = 5000;
	const DWORD PLAYERBOT_CHANNEL_FLUSH_INTERVAL = 2000;
	const DWORD PLAYERBOT_CHANNEL_COORDINATOR_INTERVAL = 5000;
	const DWORD PLAYERBOT_CHANNEL_MAX_QUEUED = 24;
	const size_t PLAYERBOT_CHANNEL_CHUNK = 400;
	// A request is acted on once it has stood this long, and only while the
	// bot keeps asking (every ask refreshes it); the coordinator acts at most
	// once a gate; a bot that changed channel is not moved out of the shop
	// channel again within the cooldown - kept in the database, so it survives
	// a restart - which is what stops a bot changing channel between every two
	// blows. A bot counts as playing while its core saw it within the last.
	const unsigned int PLAYERBOT_CHANNEL_REQUEST_STABLE_SECONDS = 30;
	const unsigned int PLAYERBOT_CHANNEL_REQUEST_EXPIRE_SECONDS = 1200;
	const unsigned int PLAYERBOT_CHANNEL_BATCH_GATE_SECONDS = 120;
	const unsigned int PLAYERBOT_CHANNEL_MOVE_COOLDOWN_SECONDS = 1200;
	const unsigned int PLAYERBOT_CHANNEL_SEEN_SECONDS = 30;
	// How long before a moved bot may log in on its new channel: its old core
	// reads the change within a refresh and logs it out first, and the P2P
	// table refuses the login while the old core still holds it.
	const unsigned int PLAYERBOT_CHANNEL_READY_OUT_SECONDS = 15;
	const unsigned int PLAYERBOT_CHANNEL_READY_IN_SECONDS = 30;
	// The coordinator moves nobody until both channels have started their
	// bots: the spawn window and two minutes on top, never under five. A
	// census taken while the cohorts are still arriving counts whichever core
	// happened to spawn first - the first test drained the shop channel
	// thirteen seconds after the start, against a split nobody had reached yet.
	const DWORD PLAYERBOT_CHANNEL_WARMUP_MIN = 300000;
	const DWORD PLAYERBOT_CHANNEL_WARMUP_AFTER_WINDOW = 120000;
	const int PLAYERBOT_SHOP_RING_MIN = 400;
	const int PLAYERBOT_SHOP_RING_RADIUS = 1700;
	// The shop bundle (item 50200) carries LIMIT_NONE in item_proto, so the game
	// itself sells stalls from level one. The floor of twenty was ours, and it is
	// why Joan had no market: five of the five hundred bots standing there were
	// above it, while Bokjung held a hundred and twenty.
	const BYTE PLAYERBOT_SHOP_MIN_LEVEL = 1;
	// How many items go on the counter. The engine allows forty
	// (SHOP_HOST_ITEM_MAX_NUM); this is about what a bot plausibly has spare, and
	// every slot costs an inventory scan when a buyer reads the offer.
	const BYTE PLAYERBOT_SHOP_MAX_ITEMS = 8;
	// A trader's counter. It is the bot's occupation rather than a sideline, so it
	// carries far more and keeps the stall up for a proper shift.
	const BYTE PLAYERBOT_SHOP_MERCHANT_ITEMS = 20;
	const DWORD PLAYERBOT_SHOP_MERCHANT_MIN_DURATION = 600000;   // 10 min
	const DWORD PLAYERBOT_SHOP_MERCHANT_MAX_DURATION = 1200000;  // 20 min
	// One bot in six that has no other calling trades for a living. Enough to give
	// each market a few permanent faces without emptying the hunting grounds.
	const DWORD PLAYERBOT_MERCHANT_SHARE = 6;
	// One in this many of the ordinary adventurers becomes a dropper - an M3,
	// M2 or medal one, drawn evenly. The Metin dropper is a third of the metin
	// hunter role instead, because hunting stones is that role's whole day.
	const DWORD PLAYERBOT_DROPPER_SHARE = 8;
	// A dropper stops levelling once it reaches the band it farms.
	//
	// Every drop in this engine is faded by aiPercentByDeltaLev, so a farmer
	// that keeps levelling walks away from its own table: the medal is a kill
	// group and is worth 1 at fifteen levels over the monster, which is how a
	// bot of forty-five came to need 140 trips through the easy Monkey Dungeon
	// for one medal. The operator's rule is that a dropper "ma miec staly level
	// i robil zawsze to samo" - so at its working level it takes the engine's
	// own AFFECT_EXP_BLOCK, which PointChange honours by returning before it
	// adds anything, and goes on dropping and selling for good.
	//
	// The numbers are each personality's own ground: the easy dungeon's monkeys
	// are 22-30, the second village's soldiers 18-36, the guild map's spawns
	// 8-24 with the level-30 weapon farm running to forty, and a stone hunter's
	// book top-up dies fifteen levels over the stone.
	const BYTE PLAYERBOT_EXP_LOCK_METIN_DROPPER = 40;
	const BYTE PLAYERBOT_EXP_LOCK_M3_DROPPER = 30;
	const BYTE PLAYERBOT_EXP_LOCK_M2_DROPPER = 36;
	const BYTE PLAYERBOT_EXP_LOCK_MEDAL_DROPPER = 33;
	// Iwakura's community patch 2, point 4 ("Grinder Lochu Malp", Tier 4).
	// Under the personalities a drawn medal dropper stays one (it used to
	// become a Wanderer, and 26 bots in a thousand farmed medals), and this
	// many in a thousand of the bots with no role of their own are drawn one
	// besides - four and a half times the old count all told. They hold at
	// PLAYERBOT_EXP_LOCK_MEDAL_DROPPER, not at a Grinder's lock.
	const int PLAYERBOT_MEDAL_DROPPER_EXTRA_PER_MILLE = 140;
	// And this share of them stays until the medals it sold pay for its
	// level's weapon and armour at +9 and helmet and shield at +7 - or it
	// wears them already - and then leaves the dungeon for good: the weapon
	// first from level thirty, the armour first under it. Looked at this often.
	const int PLAYERBOT_MEDAL_GOAL_PERCENT = 3;
	const int PLAYERBOT_MEDAL_GOAL_MAIN_PLUS = 9;
	const int PLAYERBOT_MEDAL_GOAL_SIDE_PLUS = 7;
	const BYTE PLAYERBOT_MEDAL_GOAL_WEAPON_FIRST_LEVEL = 30;
	const DWORD PLAYERBOT_MEDAL_GOAL_CHECK_MS = 60 * 1000;
	// A bot is drawn a dropper only while it stands no further than this over
	// its lock. The lock stops experience and cannot take any back, so a bot
	// that had passed its band before droppers existed - or before a restart
	// drew it one - kept the name and farmed a table the engine fades to
	// nothing for it: a level-45 dropper at a level-35 Metin in Bokjung
	// (sizowski, 15 September). The same two levels the operator's medal
	// cohort allows (CPlayerBotManager::SpawnMedalDropperCohort).
	const BYTE PLAYERBOT_DROPPER_OUTGROWN_LEVELS = 2;
	// The last PID of the cohort as the seed first laid it out: Chunjo 4..1503,
	// Shinsoo 1504..2003, Jinno 2004..2503 (generate_seed.py). 2.2.1 appended a
	// thousand Shinsoo and a thousand Jinno identities after it, and the
	// operator's medal droppers are taken from the far end of a kingdom's
	// registry - which the appended ones now are. Searched from the whole
	// registry, every Shinsoo and Jinno dropper already standing at its lock
	// would have become an ordinary bot at the first start and its place gone
	// to a character of level one, hours from the dungeon. The first layout's
	// far end is searched first (CPlayerBotManager::SpawnMedalDropperCohort).
	const DWORD PLAYERBOT_SEED_FIRST_LAYOUT_LAST_PID = 2503;
	// A dropper serves its offline shop once in this long instead of every ten
	// to fifteen minutes. The service is a walk to the village the shop stands
	// in, and it took the medal droppers off the road to the Monkey Dungeon 68
	// times in their first twenty-five minutes after a restart. The counter is
	// restocked more slowly for it, which a bot farming one thing can afford;
	// a stand lasts eight hours.
	const DWORD PLAYERBOT_DROPPER_SHOP_SERVICE_MIN_MS = 2400000;
	const DWORD PLAYERBOT_DROPPER_SHOP_SERVICE_MAX_MS = 3600000;
	// A shop on another map than its keeper is served on a long round too:
	// each such visit is two map changes, and on m2zip on 17 September they
	// were 3405 of 7951 map changes in 95 minutes - the bots of the valley in
	// and out of the first villages every ten to fifteen minutes, which the
	// players read at the gates as bots going round in circles ("kreca sie
	// ciagle pomiedzy tp", gregoszky). On its own map a keeper still serves
	// every ten to fifteen minutes; elsewhere it waits this long since its
	// last service and asks again every PLAYERBOT_OFFLINE_FAR_SERVICE_RETRY_MS.
	const DWORD PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS = 45 * 60 * 1000;
	const DWORD PLAYERBOT_OFFLINE_FAR_SERVICE_RETRY_MS = 5 * 60 * 1000;
	// A renewal of an expired stand the db core has not answered in this long
	// is dropped and asked for again at the next service visit
	// (BotOfflinePoll): it carries no goods, so a second ask cannot double any.
	const DWORD PLAYERBOT_OFFLINE_RENEW_ABANDON_MS = 5 * 60 * 1000;
	// A dropper opens its stall on a third of its town visits, against one in
	// ten for an adventurer and every visit for a merchant: it hunts for a
	// living and sells what the hunt brought, not the other way round.
	const int PLAYERBOT_DROPPER_SHOP_ROLL = 333;
	// A bot with every slot filled and nothing on its ladder left to buy has
	// spares and no use for the yang; three in ten of those keep a stall
	// against one in ten of everyone else.
	const int PLAYERBOT_FULL_GEAR_SHOP_ROLL = 300;
	// A stall stands for a while and then the bot goes back to playing. An hour
	// was long enough that a player watching the market never saw one come down,
	// which read as "the shops never close" even before the tick-ordering bug
	// that genuinely kept some of them open.
	const DWORD PLAYERBOT_SHOP_MIN_DURATION = 600000;    // 10 min
	const DWORD PLAYERBOT_SHOP_MAX_DURATION = 1500000;   // 25 min
	// Why a counter is open. The first four are the exceptions the operator's
	// TRADE slider does not touch - a trader trades, a bot that cannot afford
	// its potions or has no room left sells, a dropper under bag pressure
	// sells - and the last three are the rolls the slider stretches. A stall
	// remembers its reason, shows it in the status ("Prowadze stragan (los)"),
	// and a rolled one re-asks the roll when the weights file changes, spread
	// over PLAYERBOT_SHOP_REEVALUATE_SPREAD_MS so a hundred keepers do not
	// pack up in one second (audit D11: "minimalny suwak, a 180 z 280 botow
	// handluje" - the stalls that stood were never asked again).
	enum EPlayerBotShopReason
	{
		PLAYERBOT_SHOP_REASON_NONE = 0,
		PLAYERBOT_SHOP_REASON_MERCHANT,
		PLAYERBOT_SHOP_REASON_POOR,
		PLAYERBOT_SHOP_REASON_BAG_FULL,
		PLAYERBOT_SHOP_REASON_DROPPER_PRESSURE,
		PLAYERBOT_SHOP_REASON_BOOKS,
		PLAYERBOT_SHOP_REASON_DROPPER_ROLL,
		PLAYERBOT_SHOP_REASON_ROLL,
		PLAYERBOT_SHOP_REASON_SPARE,
		PLAYERBOT_SHOP_REASON_HOARD,
		PLAYERBOT_SHOP_REASON_MAX
	};
	const DWORD PLAYERBOT_SHOP_REEVALUATE_SPREAD_MS = 300000;   // 5 min

	inline bool IsPlayerBotShopReasonRolled(BYTE bReason)
	{
		return bReason == PLAYERBOT_SHOP_REASON_BOOKS ||
				bReason == PLAYERBOT_SHOP_REASON_DROPPER_ROLL ||
				bReason == PLAYERBOT_SHOP_REASON_ROLL ||
				bReason == PLAYERBOT_SHOP_REASON_HOARD;
	}

	inline const char* GetPlayerBotShopReasonName(BYTE bReason)
	{
		switch (bReason)
		{
			case PLAYERBOT_SHOP_REASON_MERCHANT:         return "handlarz";
			case PLAYERBOT_SHOP_REASON_POOR:             return "brak yang na mikstury";
			case PLAYERBOT_SHOP_REASON_BAG_FULL:         return "pelny plecak";
			case PLAYERBOT_SHOP_REASON_DROPPER_PRESSURE: return "dropper, pelny plecak";
			case PLAYERBOT_SHOP_REASON_BOOKS:            return "nadmiar ksiag";
			case PLAYERBOT_SHOP_REASON_DROPPER_ROLL:     return "dropper";
			case PLAYERBOT_SHOP_REASON_ROLL:             return "los";
			case PLAYERBOT_SHOP_REASON_SPARE:            return "zbedny duplikat";
			case PLAYERBOT_SHOP_REASON_HOARD:            return "nadmiar towaru";
			default:                                     return "?";
		}
	}
	// What a bot pays itself for the stall it sets up.
	const DWORD PLAYERBOT_SHOP_BUNDLE_PRICE = 2000;
	const DWORD PLAYERBOT_SHOP_REST_MIN = 1800000;
	const DWORD PLAYERBOT_SHOP_REST_MAX = 5400000;
	// A stand that ran out is followed by another on the same pitch, up to
	// this many in a row, before the rest above. A stall of ten to twenty-five
	// minutes against a rest of thirty to ninety, and a reopening that needed
	// the next town visit to end, meant a fifth of the keepers open at any
	// time: ninety stalls in the minutes after a restart, when every keeper
	// stands where its last stall was, and eleven an hour later ("boty nudza
	// sie handlem"). Two dry stands in a row end the row early - nobody is
	// buying, so the bot goes back to playing.
	const int PLAYERBOT_SHOP_STANDS_IN_ROW = 3;
	const DWORD PLAYERBOT_SHOP_REOPEN_MS = 3000;
	const DWORD PLAYERBOT_HORSE_MEDAL_VNUM = 50050;
	// What a bot keeps back of them whatever else it may do with medals. A horse
	// at exactly ten waiting on the battle-horse trial may spend none (one more
	// medal makes it eleven and no NPC in this world puts that back) and, until
	// now, sell none either: CanPlayerBotSellHorseMedals asked for a level under
	// the next milestone, which a battle-horse candidate is by definition past.
	// So "10 lv konia, ponad 40 medali w plecaku" (Greess, 19 September) was a
	// bag that filled for ever. Two are kept for the ladder that starts again
	// after the trial; the rest are goods like anything else.
	const int PLAYERBOT_HORSE_MEDAL_KEEP = 2;
	const BYTE PLAYERBOT_HORSE_REQUIRED_LEVEL = 25;
	const char* PLAYERBOT_HORSE_MEDALS_FLAG = "playerbot.horse_medals_delivered";
	const char* PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG = "playerbot.horse_medals_looted";
	const char* PLAYERBOT_HORSE_LAST_LOOT_MAP_FLAG = "playerbot.horse_last_loot_map";
	const char* PLAYERBOT_HORSE_LAST_LOOT_TIME_FLAG = "playerbot.horse_last_loot_time";
	const char* PLAYERBOT_HORSE_LAST_DELIVERY_TIME_FLAG = "playerbot.horse_last_delivery_time";
	// Fishing, matched to what the r40250 engine actually does:  the rod occupies
	// WEAR_WEAPON, the bait lives in the rod's socket 2 rather than in the pouch,
	// a cast bites after 10-40 s and then leaves a 6 s window to pull.
	const DWORD PLAYERBOT_FISHING_ROD_VNUM = 27400;   // Wedka+1
	// What a bot pays for the mt2009 fishing pass (unique item 27620, a day
	// of real time) - nothing sells one, it comes out of a quest a bot cannot
	// talk through, so it is created for the price of a rod and a bundle of
	// wood together. Unused on r40250, which has no pass.
	const DWORD PLAYERBOT_FISHING_PASS_PRICE = 50000;
#if defined(PLAYERBOT_ENGINE_MT2009)
	// When the fishing last asked for its pass (EnsurePlayerBotFishingPass), by
	// pid, and how long the equipment pass leaves a worn pass alone after that.
	// The equipment pass put a better unique item into the pass's slot, the
	// fishing put the pass back on its next tick, and the two took turns every
	// second or two: seventy swaps in eleven minutes, and the engine's
	// FAST_ITEM_SWAP check threw the bot out of the game every two minutes
	// (KimTyJestes, Karta Wedkarska against Maska Sabaha, 14 September).
	const DWORD PLAYERBOT_FISHING_PASS_HOLD_MS = 600000;
	std::map<DWORD, DWORD> s_mapPlayerBotFishingPassAskedAt;
	bool IsPlayerBotFishingPassHeld(DWORD dwPID, DWORD dwNow)
	{
		std::map<DWORD, DWORD>::const_iterator it = s_mapPlayerBotFishingPassAskedAt.find(dwPID);
		return it != s_mapPlayerBotFishingPassAskedAt.end() &&
				dwNow - it->second < PLAYERBOT_FISHING_PASS_HOLD_MS;
	}
#endif
	// The level a bot may start fishing at. The mt2009 engine's own
	// CHARACTER::fishing() refused under fifty, and the two gates here refused
	// with it so that nobody walked to a bank it would turn away; the operator
	// asked for thirty, so the engine's number moves with them
	// (playerbotify.py, apply_fishing_min_level) and this is the one place the
	// AI states it.
	const BYTE PLAYERBOT_FISHING_MIN_LEVEL = 30;
	const DWORD PLAYERBOT_FISHING_BAIT_VNUM = 27801;  // Robak
	const DWORD PLAYERBOT_SHELLFISH_VNUM = 27987;     // Malz

	// Mining. The engine has carried the whole mechanism since r40250 and this
	// world spawns none of it - see playerbot_mining.h, which places the veins
	// and keeps them standing.
	//
	// Kilof+0 (world.item_proto 29101) carries LIMIT_LEVEL 30 and shop_buy_price
	// 80000. Deokbae's pick_shop stands on three maps no bot is ever sent to, so
	// the pickaxe is created for the shop's own price the way the fishing pass
	// and the Forgetting Scroll are.
	const DWORD PLAYERBOT_PICKAXE_VNUM = 29101;
	const DWORD PLAYERBOT_PICKAXE_PRICE = 80000;
	const BYTE PLAYERBOT_MINING_MIN_LEVEL = 30;
	// A small share on purpose. A vein pays one roll every half minute, so a
	// crowd at one is a crowd doing nothing; the collector personality, which
	// already keeps things rather than selling them, takes the larger share.
	const int PLAYERBOT_MINING_PERCENT = 6;
	const int PLAYERBOT_MINING_COLLECTOR_PERCENT = 22;
	// A vein deletes itself after 7-15 minutes (kill_ore_load_event), so the
	// sites are swept for gaps once a minute.
	const DWORD PLAYERBOT_ORE_VEIN_CHECK_INTERVAL = 60000;
	const int PLAYERBOT_MINING_ARRIVE = 300;
	// The engine draws 5..15 swings and fires the event 2*count seconds later,
	// so the longest swing is thirty seconds; asking again before it resolves
	// would cancel it.
	const DWORD PLAYERBOT_MINING_SWING_WAIT = 32000;
	const DWORD PLAYERBOT_MINING_SWING_RETRY = 8000;
	const DWORD PLAYERBOT_MINING_SESSION_MIN = 360000;
	const DWORD PLAYERBOT_MINING_SESSION_MAX = 720000;
	const DWORD PLAYERBOT_MINING_REST_MIN = 900000;
	const DWORD PLAYERBOT_MINING_REST_MAX = 2700000;
	// A session broken off by a blow or by standing up after a death is taken
	// up again this long after, instead of after a rest: the fight or the
	// recovery runs in between, and the vein is still there.
	const DWORD PLAYERBOT_MINING_RESUME_AFTER_FIGHT = 45000;
	const DWORD PLAYERBOT_MINING_NO_PICK_RETRY = 1800000;
	// mining::ORE_COUNT_FOR_REFINE. A hundred raw ore is one smelted piece.
	const int PLAYERBOT_ORE_SMELT_COUNT = 100;
	const DWORD PLAYERBOT_ORE_SMELT_FEE = 5000;

	// Two kingdoms meeting on shared ground. Only ever on a frontier map, only
	// between bots, and only while the operator's KINGDOMPVP switch is above
	// zero - see playerbot_config.h, where it defaults to off.
	const DWORD PLAYERBOT_KINGDOM_PVP_INTERVAL = 120000;
	const int PLAYERBOT_KINGDOM_PVP_RANGE = 1500;
	const int PLAYERBOT_KINGDOM_PVP_LEVEL_DELTA = 8;
	// What a shell can hold: Biala / Niebieska / Krwawa Perla.
	const DWORD PLAYERBOT_PEARL_FIRST_VNUM = 27992;
	const DWORD PLAYERBOT_PEARL_LAST_VNUM = 27994;
	// Rybia Osc, what a gutted fish sometimes leaves (fishing::UseFish).
	const DWORD PLAYERBOT_FISH_BONE_VNUM = 27799;
	// How many shells a bot keeps whole. Prying one open is a bet against the
	// shell's own worth: twenty-six recipes consume a shellfish as it is, and
	// that is what it sells for. So the first few are never gambled with and
	// only the surplus is opened.
	const int PLAYERBOT_SHELLFISH_KEEP = 4;
	// Hair dye, the engine's own range: 70201 washes the colour out, 70202 to
	// 70206 set PART_HAIR to vnum-70201. char_item.cpp takes it straight from
	// UseItem with no client involved, and the colour is permanent - which is
	// the point of letting a bot use one.
	const DWORD PLAYERBOT_HAIR_DYE_FIRST_VNUM = 70201;
	const DWORD PLAYERBOT_HAIR_DYE_LAST_VNUM = 70206;
	// The item-shop dyes. The engine's switch does not answer for these, so a
	// bot never tries to use one: they are goods and nothing else.
	const DWORD PLAYERBOT_HAIR_DYE_SHOP_FIRST_VNUM = 71075;
	const DWORD PLAYERBOT_HAIR_DYE_SHOP_LAST_VNUM = 71079;
	// A dye from the water is worth next to nothing: the merchant pays three
	// hundred and most players throw theirs away, while 5 147 of them stood
	// on m2zip's counters on 18 September and 3 525 rode in the bags. A bot
	// throws them away too (DiscardPlayerBotFishedDyes) but for one colour it
	// has yet to use and these few per thousand, drawn by the item, kept for a
	// counter: "a jak juz jakis sprzedaje, to niech bedzie bardzo rzadkie"
	// (Tieru). The item shop's dyes are goods as before.
	const int PLAYERBOT_HAIR_DYE_KEEP_PERMILLE = 30;

	// A hair dye of either kind - one a bot could use, or one it can only sell.
	// Both are worth money to somebody and neither is scrap.
	// The fished range alone, the remover (70201) included.
	bool IsPlayerBotFishedHairDye(DWORD vnum)
	{
		return vnum >= PLAYERBOT_HAIR_DYE_FIRST_VNUM && vnum <= PLAYERBOT_HAIR_DYE_LAST_VNUM;
	}

	bool IsPlayerBotHairDye(DWORD vnum)
	{
		return (vnum >= PLAYERBOT_HAIR_DYE_FIRST_VNUM &&
					vnum <= PLAYERBOT_HAIR_DYE_LAST_VNUM) ||
				(vnum >= PLAYERBOT_HAIR_DYE_SHOP_FIRST_VNUM &&
					vnum <= PLAYERBOT_HAIR_DYE_SHOP_LAST_VNUM);
	}
	const int PLAYERBOT_FISHING_BAIT_BUNDLE = 20;
	// What a partial purchase leaves behind, as a share and not as a sum.
	//
	// Buying what the purse reaches was right - a bot with 556 yang and none of
	// the 800 a bundle costs used to stand at the Rybak buying nothing - but it
	// went too far the other way the moment it worked: the same bot went 556 to
	// 601 to one yang, spending its last coin on worms with nothing left for a
	// potion. A flat reserve cannot fix that, because any reserve large enough
	// to matter is larger than what the bots this helps actually own, and it
	// would refuse the very purchase it was written for. A share always leaves
	// something and never blocks the poor case.
	const int PLAYERBOT_FISHING_TACKLE_SPEND_PERCENT = 90;
	const int PLAYERBOT_FISHING_BAIT_RESTOCK = 5;
	// The Rybak (9009) himself, from map_b1 npc.txt cell (675,539) against
	// BasePosition (0,102400). Tackle is bought here.
	const long PLAYERBOT_FISHERMAN_X = 67500;
	const long PLAYERBOT_FISHERMAN_Y = 156300;
	// The bank the bots actually fish from, a short walk downstream of him. The
	// shoreline here runs north-south with the river to the east, so anglers queue
	// along Y and all face +X. This band -- x 67250..67450, y 156900..157350 --
	// was read out of map_b1's server_attr: every cell in it is standable, and
	// open water starts a little east of it (tools/decode_server_attr.py).
	// Where the anglers stand, measured along the river rather than laid out on
	// a grid.
	//
	// A rectangle was the first attempt and it put half of them on the grass:
	// this river bends, its bank running from x 69900 in the north through
	// 67200 in the middle to 67800 in the south, so any rectangle wide enough
	// to hold fifty people reaches inland to where there is no water at all.
	// A photograph from the Discord showed exactly that - a crowd on the lawn
	// with rods, several metres from the bank.
	//
	// So the stands are a table, the way hunting hubs are a table. Every
	// candidate cell along the river was taken out of map_b1's server_attr,
	// sorted by its distance to open water, and kept only if no already-kept
	// stand was within 150 units: 162 places, each one standable, each
	// within 350 units of water, and none closer to another than a metre and a
	// half. Against a live angler population near sixty that is a bank with
	// room to spare, and the first ones taken are the ones at the water's edge.
	//
	// Every coordinate sits on a navigation cell centre - base + n*50 + 25 -
	// because that is the point CPlayerBotNavigation samples when it decides
	// whether a cell may be stood on. Stands generated on the multiples of
	// fifty instead sat on cell corners, so the grid judged them by a
	// neighbouring sample: some were called blocked, the walk snapped them to
	// the nearest cell it did accept, and two anglers ended up eight units
	// apart on the same one.
	//
	// The last two numbers are a point in the water in front of the stand. A
	// bot used to be turned to face due east, which is right for a north-south
	// bank and wrong everywhere this river turns.
	struct TPlayerBotFishingStandPoint { long x; long y; long waterX; long waterY; };
	const TPlayerBotFishingStandPoint PLAYERBOT_FISHING_STANDS[] = {
		{  69775, 155625,  70825, 156675 }, {  69575, 155675,  70625, 156725 },
		{  70175, 155675,  70625, 156125 }, {  70375, 155675,  70525, 155825 },
		{  69925, 155725,  70525, 156325 }, {  69275, 155775,  70325, 156825 },
		{  69425, 155775,  70475, 156825 }, {  69725, 155825,  70325, 156425 },
		{  70275, 155825,  70425, 155825 }, {  68775, 155875,  69825, 156925 },
		{  68925, 155875,  69975, 156925 }, {  69075, 155875,  70125, 156925 },
		{  70075, 155875,  70225, 156025 }, {  69425, 155925,  70025, 156525 },
		{  69575, 155925,  70175, 156525 }, {  68575, 155975,  69625, 157025 },
		{  69875, 155975,  70025, 156125 }, {  70225, 155975,  70375, 155975 },
		{  70375, 155975,  70525, 155975 }, {  68925, 156025,  69525, 156625 },
		{  69075, 156025,  69675, 156625 }, {  69225, 156025,  69225, 156625 },
		{  68275, 156075,  69325, 157125 }, {  68425, 156075,  69475, 157125 },
		{  69575, 156075,  69725, 156225 }, {  69725, 156075,  69725, 156225 },
		{  68725, 156125,  69325, 156725 }, {  68125, 156175,  69175, 157225 },
		{  69075, 156175,  69225, 156325 }, {  69225, 156175,  69225, 156325 },
		{  69375, 156175,  69375, 156325 }, {  68425, 156225,  69025, 156825 },
		{  68575, 156225,  69175, 156825 }, {  69525, 156225,  69675, 156225 },
		{  67975, 156275,  68725, 157025 }, {  68875, 156275,  69025, 156425 },
		{  68225, 156325,  68225, 156925 }, {  69025, 156325,  69175, 156325 },
		{  69175, 156325,  69325, 156325 }, {  69325, 156325,  69475, 156325 },
		{  67575, 156375,  68625, 157425 }, {  67775, 156375,  68525, 157125 },
		{  68575, 156375,  68725, 156525 }, {  68725, 156375,  68725, 156525 },
		{  69475, 156375,  69625, 156375 }, {  68875, 156425,  69025, 156425 },
		{  68325, 156475,  68325, 156625 }, {  69025, 156475,  69175, 156475 },
		{  69175, 156475,  69325, 156475 }, {  67525, 156525,  68425, 157425 },
		{  67675, 156525,  68425, 157275 }, {  68475, 156525,  68625, 156525 },
		{  68625, 156525,  68775, 156525 }, {  68775, 156575,  68925, 156575 },
		{  67325, 156625,  68225, 157525 }, {  67175, 156775,  68225, 157825 },
		{  67325, 156775,  68225, 157675 }, {  67475, 156775,  67925, 157225 },
		{  67175, 156925,  68225, 157975 }, {  67325, 156925,  67925, 157525 },
		{  67575, 156925,  67725, 156925 }, {  67075, 157075,  68125, 158125 },
		{  67325, 157075,  67925, 157675 }, {  67475, 157075,  67625, 157225 },
		{  67075, 157225,  68125, 158275 }, {  67225, 157225,  67825, 157825 },
		{  67475, 157225,  67625, 157225 }, {  67225, 157375,  67825, 157975 },
		{  67375, 157375,  67525, 157525 }, {  67075, 157425,  68125, 157425 },
		{  67225, 157525,  67825, 157525 }, {  67375, 157525,  67525, 157525 },
		{  67075, 157575,  68125, 157575 }, {  67225, 157675,  67825, 157675 },
		{  67375, 157675,  67525, 157675 }, {  67125, 157825,  68025, 156925 },
		{  67275, 157825,  67725, 157375 }, {  67475, 157925,  67625, 157925 },
		{  67125, 157975,  68025, 157075 }, {  67325, 157975,  67925, 157975 },
		{  67475, 158075,  67625, 158075 }, {  67175, 158125,  68225, 158125 },
		{  67325, 158125,  67925, 158125 }, {  67475, 158225,  67625, 158225 },
		{  67625, 158225,  67775, 158225 }, {  67775, 158225,  67925, 158225 },
		{  68725, 158225,  68875, 158225 }, {  68875, 158225,  69025, 158225 },
		{  69025, 158225,  69175, 158225 }, {  69175, 158225,  69025, 158225 },
		{  69325, 158225,  69325, 158075 }, {  69475, 158225,  69475, 158075 },
		{  69625, 158225,  69625, 158075 }, {  69775, 158225,  69775, 158075 },
		{  69925, 158225,  69925, 158075 }, {  70075, 158225,  70225, 158225 },
		{  67275, 158275,  68025, 158275 }, {  67925, 158325,  68075, 158325 },
		{  68075, 158325,  68225, 158325 }, {  68225, 158325,  68375, 158325 },
		{  68375, 158325,  68525, 158325 }, {  68525, 158325,  68675, 158325 },
		{  70225, 158325,  70225, 158175 }, {  67425, 158375,  67725, 158075 },
		{  67675, 158375,  67825, 158375 }, {  68675, 158375,  68825, 158375 },
		{  68825, 158375,  68975, 158375 }, {  68975, 158375,  68675, 158375 },
		{  69125, 158375,  69125, 158075 }, {  69275, 158375,  68975, 158075 },
		{  69425, 158375,  69425, 157775 }, {  69575, 158375,  69575, 157775 },
		{  69725, 158375,  69725, 157775 }, {  69925, 158375,  70225, 158075 },
		{  70075, 158375,  70075, 158075 }, {  67275, 158425,  68025, 157675 },
		{  70375, 158425,  70375, 158275 }, {  67825, 158475,  67975, 158475 },
		{  67975, 158475,  67825, 158475 }, {  68125, 158475,  68125, 158175 },
		{  68275, 158475,  68275, 158175 }, {  68425, 158475,  68425, 158175 },
		{  70225, 158475,  70525, 158175 }, {  67425, 158525,  68175, 157775 },
		{  67575, 158525,  68025, 158075 }, {  68575, 158525,  68575, 158075 },
		{  68725, 158525,  68725, 158075 }, {  68875, 158525,  68875, 158075 },
		{  69025, 158525,  68575, 158075 }, {  69175, 158525,  69175, 157775 },
		{  69325, 158525,  68575, 157775 }, {  69475, 158525,  68575, 157625 },
		{  69625, 158525,  69625, 157475 }, {  69775, 158525,  70525, 157775 },
		{  69925, 158525,  70675, 157775 }, {  70075, 158525,  70075, 157775 },
		{  67225, 158575,  68125, 157675 }, {  70375, 158575,  70825, 158125 },
		{  67725, 158625,  68175, 158175 }, {  67875, 158625,  67875, 158175 },
		{  68025, 158625,  67575, 158175 }, {  68175, 158625,  67575, 158025 },
		{  68325, 158625,  68325, 157875 }, {  70225, 158625,  70975, 157875 },
		{  67425, 158675,  68325, 157775 }, {  67575, 158675,  68325, 157925 },
		{  68475, 158675,  68475, 157775 }, {  68625, 158675,  68625, 157775 },
		{  68775, 158675,  68775, 157775 }, {  68925, 158675,  68025, 157775 },
		{  69075, 158675,  68175, 157775 }, {  69225, 158675,  68175, 157625 },
		{  70025, 158675,  70925, 157775 }, {  70375, 158725,  71125, 157975 },
		{  67825, 158775,  67825, 157875 }, {  67975, 158775,  67975, 157875 },
		{  68125, 158775,  67225, 157875 }, {  68275, 158775,  67375, 157875 },
		{  70225, 158775,  71125, 157875 }, {  67475, 158825,  68525, 157775 },
		{  67625, 158825,  68675, 157775 }, {  70375, 158875,  71425, 157825 }
	};
	const size_t PLAYERBOT_FISHING_STAND_COUNT =
			sizeof(PLAYERBOT_FISHING_STANDS) / sizeof(PLAYERBOT_FISHING_STANDS[0]);
	// The middle of that table and a radius that covers all of it. Only the
	// status line uses these, for the one question it asks about an angler: is
	// it at the river yet, or still on its way. The stands themselves span
	// x 67050..70400 and y 155600..158850, so nothing smaller reaches the ends.
	const long PLAYERBOT_FISHING_BANK_X = 68725;
	const long PLAYERBOT_FISHING_BANK_Y = 157225;
	const int PLAYERBOT_FISHING_BANK_RADIUS = 2600;
	const DWORD PLAYERBOT_FISHING_STAND_CLAIM = 120000;
	// A point well inside the river, used only to turn the bot to face the water.
	const long PLAYERBOT_FISHING_WATER_X = 68000;
	// Where an angler counts as arrived - and it may never be tighter than
	// PLAYERBOT_NAV_ARRIVAL_DISTANCE, which is where the walk itself stops.
	//
	// This was cut to twenty-five to keep anglers a metre apart and that made a
	// dead zone: MovePlayerBot reports success and stops moving at a hundred
	// units from the goal, the fishing pass kept asking for another step, and
	// the bot stood between the two numbers for ever with stuck=0 and nothing in
	// any log. Measured on the live server at seventy-one and seventy-six units
	// from a destination neither bot ever reached.
	//
	// The consequence is honest and worth stating: with stands a hundred and
	// fifty apart and a hundred units of tolerance at each end, two anglers can
	// still end up close. Spacing them further is a separate change to the stand
	// table, not a number to shave here. The static_assert in
	// playerbot_activities.h keeps this from being lowered again.
	const int PLAYERBOT_FISHING_ARRIVE = 100;
	// The Rybak is a counter, not a cast point: his approach point sits on
	// blocked ground in Yongan and Pyongmoo, the walk snapped it 119-177
	// units away and then tested arrival at the hundred above, so a bot
	// stood "Ide do Rybaka po przynete" for its whole session (seban latino,
	// 17 September). The purchase asks no distance of the NPC at all; the
	// snap stays inside the radius that tests arrival.
	const int PLAYERBOT_FISHING_TACKLE_ARRIVE = 400;
	const int PLAYERBOT_FISHING_TACKLE_SNAP_CELLS = 4;
	// Independently planned route failures before the bank is written off. Six
	// matches the town-service rescue; anything larger is indistinguishable from
	// never giving up at all.
	const int PLAYERBOT_FISHING_STUCK_LIMIT = 6;
	const DWORD PLAYERBOT_FISHING_PROGRESS_LOG = 15000;
	// fishing::Compute() peaks at time step 15 -- about 3.0 s after the bite for
	// the normal and easy tables.  Pulling in a small band around that catches
	// fish reliably without looking frame-perfect.
	const DWORD PLAYERBOT_FISHING_PULL_MIN_DELAY = 2700;
	const DWORD PLAYERBOT_FISHING_PULL_MAX_DELAY = 3300;
	// A cast that never reports a bite (the engine waits 10-40 s) is abandoned so
	// one wedged event cannot park a bot at the water forever.
	const DWORD PLAYERBOT_FISHING_CAST_TIMEOUT = 60000;
	// And a bot that reaches the water and never casts at all.
	//
	// The cast timeout above covers a line that goes in and never bites. Nothing
	// covered the step before it: an angler standing on its bank with bait in
	// the bag and no rod on its back had no clock of any kind, and one was
	// reported standing there for two hours. A session that has not managed a
	// single cast in this long is over; the ordinary rest interval then keeps
	// the bot away from the water until something has changed.
	const DWORD PLAYERBOT_FISHING_NO_CAST_GIVE_UP = 120000;
	const DWORD PLAYERBOT_FISHING_SESSION_MIN = 900000;    // 15 min
	const DWORD PLAYERBOT_FISHING_SESSION_MAX = 2400000;   // 40 min
	const DWORD PLAYERBOT_FISHING_REST_MIN = 2700000;      // 45 min
	const DWORD PLAYERBOT_FISHING_REST_MAX = 7200000;      // 2 h
	const int PLAYERBOT_HORSE_MOUNT_DISTANCE = 1800;
	// No mount within this long of a climb-down, whatever took the bot off.
	// The travel itself no longer climbs down (UpdatePlayerBotTravelMount);
	// what still does wants the ground for a moment - a fight on a transport
	// horse, a duel, a skill, the rod - and the travel put the bot straight
	// back in the saddle: 14 502 of 24 379 mounts in 36 minutes on the test
	// world came within six seconds of a dismount, and 2 136 dismounts an hour
	// in Bokjung alone were each a stop the client shows as a step back
	// ("wariuja, schodza z konia, cofaja sie", sizowski, 15 September).
	const DWORD PLAYERBOT_HORSE_TRAVEL_FLIP_HOLD_MS = 6000;
	const DWORD PLAYERBOT_HORSE_RIDE_RETRY_INTERVAL = 10000;
	const DWORD PLAYERBOT_HORSE_TRAVEL_MIN_DELAY = 30000;
	const DWORD PLAYERBOT_HORSE_TRAVEL_MAX_DELAY = 300000;
	// Holding a target defers world travel, but only for so long. Where the
	// respawn is dense a bot re-acquires one before the next tick, so an
	// unbounded deferral meant the routing code never ran and an arrival area
	// became somewhere a bot could enter but not leave (issue #10).
	const DWORD PLAYERBOT_TRAVEL_FIGHT_GRACE = 30000;
	// What counts as being in the fight rather than merely locked on to it.
	const DWORD PLAYERBOT_TRAVEL_ENGAGED_WINDOW = 5000;
	const int PLAYERBOT_TRAVEL_ENGAGED_RANGE = 800;
	const DWORD PLAYERBOT_WORLD_TRAVEL_MIN_DELAY = 60000;
	const DWORD PLAYERBOT_WORLD_TRAVEL_MAX_DELAY = 360000;
	// Level 22 is past M1's useful experience range.  These bots still leave in a
	// staggered wave, but do not spend another six minutes farming weak mobs after
	// completing their town errands.
	const DWORD PLAYERBOT_LEVEL22_TRAVEL_MIN_DELAY = 15000;
	const DWORD PLAYERBOT_LEVEL22_TRAVEL_MAX_DELAY = 90000;
	// Refining remains important, but it is a planned town run rather than a reason
	// to bounce M2 -> M1 after every newly affordable +1 attempt.
	const DWORD PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY = 720000;
	const DWORD PLAYERBOT_REMOTE_REFINE_RETURN_MAX_DELAY = 1500000;
	const DWORD PLAYERBOT_MONKEY_MAX_VISIT_TIME = 1800000;
	// A medal dropper does not leave the dungeon for medals at all: they are
	// counter stock, not an errand at the stable, and the count the exit reads
	// is the whole bag - at five, a dropper already holding five walked in and
	// straight back out nine seconds later, with nothing to stop it doing so
	// again. A full stack is the number; the half hour above, the potions and a
	// bag with no cell left end the visit.
	const int PLAYERBOT_MEDAL_DROPPER_MEDAL_STOCK = 200;
	// Which Monkey Dungeon a level is sent to. The medal is a "kill" drop group
	// (mob_drop_item.txt: one medal per 550 soldiers, 500 fighters, 200 generals)
	// and CreateDropItem scales every kill-group roll by aiPercentByDeltaLev -
	// 1% of the rate once the killer stands fifteen levels above the monster.
	// So a level-45 bot in the easy dungeon (monkeys of 22-29) needed fifty
	// thousand kills for a medal: 140 trips an hour brought back one. The
	// medium dungeon holds monkeys of 35-42 and the hard one 45-54; the bands
	// keep a bot within ten levels of the room it fights in.
	const BYTE PLAYERBOT_MONKEY_MIN_LEVEL = 18;
	const BYTE PLAYERBOT_MONKEY_MEDIUM_MIN_LEVEL = 33;
	const BYTE PLAYERBOT_MONKEY_HARD_MIN_LEVEL = 46;
	// The harder two dungeons are shared maps and live on the core that carries
	// Chunjo, so under the default split layout a Shinsoo or Jinno bot can
	// never reach either: for them the band above is a band with no map in it.
	// Such a bot keeps working its own kingdom's easy rooms instead, but only
	// while that is still worth a trip - the medal is a kill-group roll and
	// aiPercentByDeltaLev has bottomed out by fifteen levels over the monster,
	// and the easy dungeon's monkeys stop at thirty. Past this, no dungeon.
	const BYTE PLAYERBOT_MONKEY_EASY_FALLBACK_MAX_LEVEL = 40;
	// Past this no bot farms medals in a dungeon at all, the dropper included:
	// the hard dungeon's monkeys run 45 to 54, and ten levels over its generals
	// aiPercentByDeltaLev pays half a roll and fifteen over its soldiers one
	// percent. Such a bot buys its medal from a counter instead - the medal is
	// one of the strategic purchases (PLAYERBOT_STRATEGIC_BUDGET_PERCENT).
	const BYTE PLAYERBOT_MONKEY_MEDAL_MAX_LEVEL = 64;
	// How much more often a bot still short of its battle horse rolls the
	// medal errand, and the most any chance may reach. Measured on the test
	// world on 15 September: 17 of 999 bots in a Monkey Dungeon and none in
	// the medium one, one medal handed in that hour, and of 1177 bots of 35 and
	// up 415 on no horse at all and 8 past horse level ten ("boty nie maja 11
	// poziomu konia, za rzadko chodza na sredni i trudny loch malp").
	const int PLAYERBOT_HORSE_EXPEDITION_NO_COMBAT_HORSE_MULT = 2;
	const int PLAYERBOT_HORSE_EXPEDITION_MAX_CHANCE = 70;
	const DWORD PLAYERBOT_M3_MAX_VISIT_TIME = 1200000;
	// A visit that ran out without the weapon is followed by this long away
	// from M3, drawn by pid, before the door opens again. The exit sent the bot
	// to M2 and the M2 branch sent it straight back ("m3_visit_complete" and
	// "level30_weapon_to_m3" two seconds apart, Champion of urtopy's world, 21
	// September), so a bot short of the weapon never reached the valley for its
	// Biologist row or the Monkey Dungeon for its horse, and its status read
	// the planner's goal over a bot farming something else.
	const DWORD PLAYERBOT_M3_REVISIT_WAIT_MIN_MS = 45 * 60 * 1000;
	const DWORD PLAYERBOT_M3_REVISIT_WAIT_MAX_MS = 90 * 60 * 1000;
	const DWORD PLAYERBOT_MONKEY_REVERSE_PORTAL_BLOCK_TIME = 10000;
	// The third hand. Worn in a unique slot it makes CHARACTER::RewardGold hand
	// a kill's yang straight to the killer instead of scattering coin piles on
	// the ground, which is a bot's whole reason for wanting one: the walk to
	// each pile costs a route plan and the piles it never reaches rot where
	// they fell. The engine asks IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT), and
	// what that group holds on this server is 72016..72018 - not the 71010 the
	// item shop sells, which belongs to no group at all and would do nothing.
	//
	// It is a shop item with a wear clock: ITEM_MANAGER::CreateItem seeds
	// ITEM_SOCKET_UNIQUE_REMAIN_TIME from VALUE0 and unique_expire_event counts
	// it down one minute at a time while the item is worn, so 72018 is three
	// hours of hunting and then nothing. A bot has no item shop to go back to,
	// so its copy is wound back up instead of re-bought - the same answer
	// ManagePlayerBotSkillBooks gives to a book's eighteen-hour wait.
	// The group is 72016..72018; since patch 0010 the bots take these off.
	const DWORD PLAYERBOT_THIRD_HAND_VNUM_FIRST = 72016;
	const DWORD PLAYERBOT_THIRD_HAND_VNUM = 72018;
	const long PLAYERBOT_THIRD_HAND_MINUTES = 525600;
	const long PLAYERBOT_THIRD_HAND_REWIND_BELOW = 10080;
	const DWORD PLAYERBOT_THIRD_HAND_INTERVAL = 300000;
	// Maska Sabaha (72731, 72735) left this world with the Hwang Temple's curse
	// it was worn against (playerbotify apply_hwang_curse_removed, after
	// NerrVoVy's report of 15 September): nothing hands one out, no bot wears
	// one, and the merchant takes the ones still in bags.
	bool IsPlayerBotRetiredItem(DWORD vnum)
	{
		return vnum == 72731 || vnum == 72735;
	}
	// What a bot leaves on the ground, and sells if it has one: Plaszcz
	// Uciekiniera (70048) and Symb. Krola Przepowiedni (70050), uniques of the
	// old Moonlight chest no bot wears or uses. 283 bots of the test world
	// carried 966 capes, and one bag's second page was capes and symbols
	// (Tieru, 15 September: "niech boty tego nie podnosza").
	bool IsPlayerBotLeftOnGroundItem(DWORD vnum)
	{
		// The Demon Tower's fake Bong-In key (playerbot_demon_tower.h): the
		// real one is carried to Sa-Soe, this one is worth nothing to anybody.
		if (vnum == PLAYERBOT_TOWER_FAKE_KEY)
			return true;
		return vnum == 70048 || vnum == 70050;
	}
	// The uniques a bot never wears (playerbot_unique_slots.h). Pierscien
	// Niejawnosci (70007) hides the level over a character's head and Plaszcz
	// Uciekiniera (70048) its alignment title - a player hiding something, not
	// a bot playing. "Bot Toty nie ma widocznego lv, dlaczego?" (Tieru, 15
	// September) was one of eleven bots wearing the ring, put there by the
	// equipment pass because any unique fills an empty unique slot.
	bool IsPlayerBotNeverWornUnique(DWORD vnum)
	{
		return vnum == 70007 || vnum == 70048 || IsPlayerBotRetiredItem(vnum);
	}
	// A ring of experience (the engine's group 10000 and 70005: half as much
	// experience again) and a thief's glove (group 10002: 70043, 72004, 72005;
	// 72006 pays only against bosses and stones, 71016 is used, not worn)
	// count their minutes only while worn - value2 is 0 on every one of them,
	// so unique_expire_event takes a minute a minute from the socket and stops
	// at the unequip. A bot wears them while it hunts and takes them off in
	// town ("pierscienie czy rekawice zaklada sie na slot na x czasu ... oby
	// nie ubierali ich w miescie", Tieru, 15 September).
	const DWORD PLAYERBOT_EXP_RING_VNUMS[] = { 70005, 72001, 72002, 72003, 72049, 72050 };
	const DWORD PLAYERBOT_THIEF_GLOVE_VNUMS[] = { 70043, 72004, 72005 };
	bool IsPlayerBotExpRing(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_EXP_RING_VNUMS) / sizeof(PLAYERBOT_EXP_RING_VNUMS[0]); ++i)
			if (PLAYERBOT_EXP_RING_VNUMS[i] == vnum)
				return true;
		return false;
	}
	bool IsPlayerBotThiefGlove(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_THIEF_GLOVE_VNUMS) / sizeof(PLAYERBOT_THIEF_GLOVE_VNUMS[0]); ++i)
			if (PLAYERBOT_THIEF_GLOVE_VNUMS[i] == vnum)
				return true;
		return false;
	}
	bool IsPlayerBotTimedUnique(DWORD vnum)
	{
		return IsPlayerBotExpRing(vnum) || IsPlayerBotThiefGlove(vnum);
	}
	// How often the unique-slot pass looks, how soon it retries a change the
	// swing window refused, and how long without a blow is no longer hunting.
	const DWORD PLAYERBOT_TIMED_UNIQUE_INTERVAL = 8000;
	const DWORD PLAYERBOT_TIMED_UNIQUE_RETRY_MS = 2000;
	const DWORD PLAYERBOT_TIMED_UNIQUE_IDLE_MS = 180000;
	// How long a bot works one chamber before walking to the portal that leads
	// to the next - and, now that the engine refuses to move a bot through a
	// GOTO door for the same time after the last one moved it, the only way a
	// bot leaves a chamber at all. The value and the measurements behind it
	// live in playerbot_monkey_policy.h, because char.cpp has to read the same
	// number: two copies of it are how a bounce would come back.
	const DWORD PLAYERBOT_MONKEY_CHAMBER_DWELL = playerbot_monkey::kChamberDwellMs;
	// How long the walk to the first room's chosen door may take before the
	// bot gives it up and hunts where it stands - the walking alone, with the
	// time a fight or a retreat held it up left out. The entrance room is not a
	// few thousand units across: one door of the hard dungeon stands seventeen
	// and a half thousand from the arrival point, some twenty-three seconds in
	// the saddle, and a clock that ran through the fights on the way gave up
	// four walks in six that were going the right way. A walk that has had a
	// minute of walking and not arrived is not arriving.
	const DWORD PLAYERBOT_MONKEY_SPREAD_WALK_MS = 60000;
	// And the bound on the intent itself, in wall time, for a bot the corridor's
	// monkeys never let walk: it fights where it stands either way, and after
	// this long it hunts there on its own account.
	const DWORD PLAYERBOT_MONKEY_SPREAD_MAX_MS = 180000;
	// Every kingdom has an easy dungeon of its own and they are three separate
	// maps: metin2_map_monkey_dungeon_11 (5), _12 (25) and _13 (45), at three
	// base positions 76800 apart. Only Chunjo's was ever listed here, so a
	// Shinsoo or Jinno bot walked in through its own gate and then stood in a
	// map this file did not recognise: no chambers, no hubs, no medal. Measured
	// on our own world before the fix - Shinsoo 500 characters and 0 horses,
	// Jinno 500 and 0, Chunjo the only kingdom levelling one at all.
	// All three carry the same Town.txt cell (72,125), which is what the local
	// arrival offset below already encodes.
	const long PLAYERBOT_MONKEY_SHINSOO_BASE_X = 768000;
	const long PLAYERBOT_MONKEY_SHINSOO_BASE_Y = 435200;
	const long PLAYERBOT_MONKEY_CHUNJO_BASE_X = 844800;
	const long PLAYERBOT_MONKEY_CHUNJO_BASE_Y = 435200;
	const long PLAYERBOT_MONKEY_JINNO_BASE_X = 921600;
	const long PLAYERBOT_MONKEY_JINNO_BASE_Y = 435200;
	// The dungeons are one maze: metin2_map_monkey_dungeon2 and _3 carry
	// the same server_attr, the same regen cells and the same GOTO portals as
	// _12, at another base position. Everything placed in the easy dungeon is
	// therefore a local offset, and a dungeon is its base.
	const long PLAYERBOT_MONKEY_MEDIUM_BASE_X = 128000;
	const long PLAYERBOT_MONKEY_MEDIUM_BASE_Y = 640000;
	const long PLAYERBOT_MONKEY_HARD_BASE_X = 128000;
	const long PLAYERBOT_MONKEY_HARD_BASE_Y = 716800;
	// Six cells south of the exit NPC (10070/10073/10075, cell 72,119 and
	// 72,114 - the arrival is where the easy one always was, the portal is
	// the NPC itself).
	const long PLAYERBOT_MONKEY_ARRIVAL_LOCAL_X = 7200;
	const long PLAYERBOT_MONKEY_ARRIVAL_LOCAL_Y = 12500;
	const long PLAYERBOT_MONKEY_RETURN_LOCAL_X = 7200;
	const long PLAYERBOT_MONKEY_RETURN_LOCAL_Y = 11900;

	// All five: the three kingdoms' easy dungeons and the shared harder pair.
	// Asking the kingdom table rather than naming 25 is what stops this file
	// answering "not a dungeon" about two thirds of the world's easy ones.
	bool IsPlayerBotMonkeyMap(long mapIndex)
	{
		return playerbot_empire_rules::IsMonkeyEasyMap(mapIndex) ||
				mapIndex == PLAYERBOT_MAP_MONKEY_MEDIUM ||
				mapIndex == PLAYERBOT_MAP_MONKEY_HARD;
	}

	bool GetPlayerBotMonkeyBase(long mapIndex, long& outX, long& outY)
	{
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_MONKEY_SHINSOO: outX = PLAYERBOT_MONKEY_SHINSOO_BASE_X; outY = PLAYERBOT_MONKEY_SHINSOO_BASE_Y; return true;
			case PLAYERBOT_MAP_MONKEY_EASY: outX = PLAYERBOT_MONKEY_CHUNJO_BASE_X; outY = PLAYERBOT_MONKEY_CHUNJO_BASE_Y; return true;
			case PLAYERBOT_MAP_MONKEY_JINNO: outX = PLAYERBOT_MONKEY_JINNO_BASE_X; outY = PLAYERBOT_MONKEY_JINNO_BASE_Y; return true;
			case PLAYERBOT_MAP_MONKEY_MEDIUM: outX = PLAYERBOT_MONKEY_MEDIUM_BASE_X; outY = PLAYERBOT_MONKEY_MEDIUM_BASE_Y; return true;
			case PLAYERBOT_MAP_MONKEY_HARD: outX = PLAYERBOT_MONKEY_HARD_BASE_X; outY = PLAYERBOT_MONKEY_HARD_BASE_Y; return true;
			default: return false;
		}
	}

	bool GetPlayerBotMonkeyArrival(long mapIndex, long& outX, long& outY)
	{
		long baseX = 0, baseY = 0;
		if (!GetPlayerBotMonkeyBase(mapIndex, baseX, baseY))
			return false;
		outX = baseX + PLAYERBOT_MONKEY_ARRIVAL_LOCAL_X;
		outY = baseY + PLAYERBOT_MONKEY_ARRIVAL_LOCAL_Y;
		return true;
	}

	bool GetPlayerBotMonkeyReturnPortal(long mapIndex, long& outX, long& outY)
	{
		long baseX = 0, baseY = 0;
		if (!GetPlayerBotMonkeyBase(mapIndex, baseX, baseY))
			return false;
		outX = baseX + PLAYERBOT_MONKEY_RETURN_LOCAL_X;
		outY = baseY + PLAYERBOT_MONKEY_RETURN_LOCAL_Y;
		return true;
	}

	// Which of the three rooms a bot of this level belongs in - the band alone,
	// with no map index in it. Naming a map here is what made every kingdom's
	// medal errand point at Chunjo's dungeon: the band is the same everywhere,
	// the map it means is not. GetPlayerBotMonkeyMapFor (playerbot_travel.h)
	// turns a band into this bot's own dungeon, because that needs the bot's
	// kingdom and whether this core hosts the shared pair at all.
	enum EPlayerBotMonkeyBand
	{
		PLAYERBOT_MONKEY_BAND_NONE = 0,
		PLAYERBOT_MONKEY_BAND_EASY,
		PLAYERBOT_MONKEY_BAND_MEDIUM,
		PLAYERBOT_MONKEY_BAND_HARD
	};

	EPlayerBotMonkeyBand GetPlayerBotMonkeyBandForLevel(BYTE level)
	{
		if (level < PLAYERBOT_MONKEY_MIN_LEVEL)
			return PLAYERBOT_MONKEY_BAND_NONE;
		if (level < PLAYERBOT_MONKEY_MEDIUM_MIN_LEVEL)
			return PLAYERBOT_MONKEY_BAND_EASY;
		if (level < PLAYERBOT_MONKEY_HARD_MIN_LEVEL)
			return PLAYERBOT_MONKEY_BAND_MEDIUM;
		return PLAYERBOT_MONKEY_BAND_HARD;
	}

	const char* GetPlayerBotMonkeyName(long mapIndex)
	{
		if (playerbot_empire_rules::IsMonkeyEasyMap(mapIndex))
			return "easy";
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_MONKEY_MEDIUM: return "medium";
			case PLAYERBOT_MAP_MONKEY_HARD: return "hard";
			default: return "monkey";
		}
	}
	const long MAP21_BASE_X = 921600;
	const long MAP21_BASE_Y = 204800;

	// One line on a stall's counter. CShop keeps its own item list private, and a
	// bot browsing the market reads this instead - we are the ones who put the
	// items there, so it is exactly what is on sale.
	// The engine's private-shop grid, as SetShopItems lays it out: five columns,
	// and rows down to SHOP_HOST_ITEM_MAX_NUM (forty) cells - the grid itself has
	// nine rows, but the item vector has forty entries and a slot past them is
	// out of bounds. A line occupies its cell and the cells below it, one per
	// unit of the item's size.
	const int PLAYERBOT_SHOP_GRID_COLUMNS = 5;
	const int PLAYERBOT_SHOP_GRID_ROWS = 8;
	const int PLAYERBOT_SHOP_GRID_CELLS = PLAYERBOT_SHOP_GRID_COLUMNS * PLAYERBOT_SHOP_GRID_ROWS;

	struct TPlayerBotShopOffer
	{
		DWORD dwVnum;
		DWORD dwPrice;
		BYTE bRefine;
		// A stall line is a whole stack, priced as one. What a buyer paid for the
		// line only says something about the material once it is divided by this.
		WORD wCount;
		// The item itself, by id. A line is sold once the engine has moved this
		// item to its buyer, and that is the only fact that says so: the sold
		// stack's vnum and refine are still in the keeper's bag whenever it
		// carries a second stack of the same thing - a bought stack lands in its
		// own cell and never merges - and matching by vnum found that second
		// stack and walked a buyer over to a sold slot.
		DWORD dwItemID;
		// Whether the sale of this line has been written to log.log. The engine
		// logs the buyer's side (SHOP_BUY) and nothing for the keeper, and the
		// keeper is the one whose history a player reads.
		bool bSoldLogged;
		// The skill in a book's socket, kept with the line because the demand
		// signal is read when the item is already gone from the bag - and a
		// skill book is not one commodity (see PlayerBotSaleKey). Zero for
		// everything else.
		DWORD dwSkillVnum;
		// Where the line sits in the engine's shop, which is what CShopManager::Buy
		// indexes by. Not the line's index in the table: a private shop is a grid
		// of five columns and eight rows, a weapon is three cells tall and an
		// armour two, and a line whose cell the item above already covers is
		// dropped by SetShopItems with a "not empty position" in syserr - two
		// hundred and eighty of them in an hour. Numbering the lines 0, 1, 2 put
		// every line from the second row under a weapon or an armour, so the
		// engine had nothing at those slots and refused every buyer who walked
		// over for one: two thousand refusals to five hundred purchases.
		BYTE bSlot;
	};

	struct TPlayerBotBiologistMission
	{
		BYTE requiredLevel;
		const char* questName;
		DWORD itemVnum;
		DWORD mobVnum;
		BYTE requiredCount;
		BYTE acceptPercent;
		DWORD rewardGold;
		DWORD rewardExp;
		const char* itemLabel;
		// The second half of a row, when it has one: the quest waits in
		// key_item for this item, which its own kill hook drops one time in
		// five hundred, and pays the affect and the casket below on hand-in.
		// Zero means the row ends when the specimens are accepted, which is
		// what every herb row does. These used to be three constants named
		// after the Orc Tooth, and a second row with a key could not be
		// expressed at all.
		DWORD keyItemVnum;
		// Which monster's death can drop that key. It is the quest's own kill
		// hook that decides - 631-637 for the Orc Tooth, 701-707 and 731-737
		// for the Curse Book - and the bot only needs one of them to hunt. It
		// was a constant named after the Elite Orc, so a second row's key phase
		// would have sent the bot after the wrong monster entirely.
		DWORD keyMobVnum;
		BYTE rewardPoint;
		int rewardPointValue;
		DWORD rewardBoxVnum;
	};

	// The gold and experience columns are zero on purpose, and that is this
	// world's own answer rather than a simplification: give_reward reads
	// reward_data.lua by quest name, and that file's seventy-nine entries do
	// not include a single biologist quest. So the herb rows pay nothing but
	// the first one's weapon, exactly as they do for a player - see
	// GivePlayerBotBiologistReward. Filling a row in here is all it takes if
	// the quest ever gets a reward_data entry of its own.
	const TPlayerBotBiologistMission PLAYERBOT_BIOLOGIST_MISSIONS[] = {
		{ 4,  "make_herb_lv4",  50701, 173, 5,  90, 0, 0, "Kwiat Brzoskwini", 0, 0, 0, 0, 0 },
		{ 7,  "make_herb_lv7",  50702, 175, 5,  90, 0, 0, "Pokrzywa",         0, 0, 0, 0, 0 },
		{ 10, "make_herb_lv10", 50703, 177, 5,  90, 0, 0, "Kwiat Kaki",       0, 0, 0, 0, 0 },
		{ 15, "make_herb_lv15", 50704, 181, 5,  90, 0, 0, "Korzen Gango",     0, 0, 0, 0, 0 },
		{ 20, "make_herb_lv20", 50705, 182, 10, 80, 0, 0, "Bez",              0, 0, 0, 0, 0 },
		{ 25, "make_herb_lv25", 50706, 183, 10, 70, 0, 0, "Grzyb Tue",        0, 0, 0, 0, 0 },
		// The Orc Tooth. Ten from the Orcs (601) of the valley, one in twenty
		// kills while the quest is open; sixty percent of what is handed in is
		// accepted, the rest is spoiled, as in the quest without the elixir. The
		// quest's twenty-two hours between hand-ins are not kept - a bot hands
		// in what it carries. Then the second half: Jinunggyi's Soul Stone
		// (30220), one in five hundred Elite Orc kills while the quest waits for
		// it, and the reward is the quest's own, ten movement speed for good.
		{ 30, "collect_quest_lv30", 30006, 601, 10, 60, 0, 0, "Zab Orka",
				30220, 631, POINT_MOV_SPEED, 10, 50109 },
		// The chain does not stop at the Orc Tooth: collect_quest_lv30's last
		// state runs lv40, and lv40 runs lv50. Both want fifteen specimens at
		// the same sixty percent, both wait for a key item one kill in five
		// hundred, and both pay a permanent affect and a casket - measured off
		// this world's own quest files, not a wiki.
		//
		// The Curse Book is carried by the Tormentors (706, 756, level 49) of
		// Orc Valley's central island - 68 spawn points each, and the key
		// (30221) comes from the same quest's hook on 701-707 in the valley
		// and 731-737 in Milgyo, both hosted. A bot of forty reaches a monster
		// of forty-nine: PLAYERBOT_MAX_TARGET_LEVEL_DELTA is fifteen.
		// The key names 701, not 706: a row's hunt vnum stands for a family
		// (IsPlayerBotBiologistHuntRace), and the Curse Book's specimen and its
		// key are two different families on the same Tormentor.
		{ 40, "collect_quest_lv40", 30047, 706, 15, 60, 0, 0, "Ksiega Klatw",
				30221, 701, POINT_ATT_SPEED, 5, 50110 },
		// The Demon Souvenir is the row this world cannot finish, and it is
		// here so that it starts working by itself the day that changes. Its
		// specimen (30015) drops from the Demon Soldier (1001) and its key
		// (30222) from 1001-1004, and all four stand on exactly one map in
		// this world: metin2_map_deviltower1, index 66, which game2 hosts
		// while every bot lives on game1 - a map a bot can never reach, since
		// WarpSet needs a client. So 1001 deliberately has no row in
		// PLAYERBOT_HUNTING_MOB_HOMES, and GetActivePlayerBotBiologistMission
		// steps over a row whose monster stands nowhere hosted; give 1001 a
		// row there if the map is ever moved and this one comes alive.
		// The key names 1002 for the same reason: 1001 alone carries the
		// souvenir, 1001-1004 the key.
		{ 50, "collect_quest_lv50", 30015, 1001, 15, 60, 0, 0, "Pamiatka Po Demonie",
				30222, 1002, POINT_DEF_GRADE_BONUS, 60, 50111 }
	};
	const DWORD PLAYERBOT_ORC_TOOTH_VNUM = 30006;
	// How many specimens are worth a walk to Joan.
	//
	// The hand-in was gated on carrying the whole remaining count - ten Orc
	// Teeth in one bag - and almost nobody ever got there: 700 bots held 2219
	// teeth between them, three apiece, and exactly three had ten. Meanwhile the
	// Biologist's counter stood at 0/10 for the entire world. The hand-in itself
	// has always been one specimen at a time with a 60% accept roll, so a
	// partial load was never a problem for the quest - only for the gate in
	// front of it.
	const int PLAYERBOT_BIOLOGIST_MIN_HANDIN = 4;
	// A herb row this far below the bot is one it will never do: the monsters
	// that carry the early specimens stand in Joan and Bokjung, and a bot of
	// forty lives in the valley. The chain is not one quest but seven, so a row
	// can be stepped over rather than blocking every row behind it - which is
	// what the Discord saw: a Sura of forty-two with "Korzen Gango 0/5" as its
	// stated goal, hitting Orcs, for ever.
	const int PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS = 10;
	// A first-village herb row a bot has outgrown is a trip to Joan, and
	// 2.0.60 sent every such bot at once: on the test world the M2 -> M1
	// crossings went from three hundred an hour to 2 766, 580 of 1 099 bots
	// stood in the first villages and the players filmed the crowd riding
	// into the gates ("masa botow na koniach wchodzacych do portalu",
	// "boty 40-50+ expia w m1", 16 September). The rows are still done in
	// order at any level, but a bot that has outgrown a herb row takes the
	// row only when a place in this share of the live population is free -
	// wherever it stands, or the bots already in the villages stay for all
	// six rows - and a place is held for at most
	// PLAYERBOT_BIOLOGIST_HERB_ERRAND_MAX_MS.
	// Seventy per mille since 2.0.70: at 2.5% a bot of seventy-five with four
	// rows left waited hours for a place and the rows were never caught up
	// ("jak mozna bezpiecznie zrobic by boty nadrabialy sobie biologa", Tieru,
	// 17 September). The crowd is still bounded by construction - a share of
	// the live population, which is what 2.0.60 was missing - so this is 70-80
	// bots in the first villages at a time on a world of eleven hundred, not
	// the 580 that filled them then.
	const int PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE = 70;
	const DWORD PLAYERBOT_BIOLOGIST_HERB_ERRAND_MAX_MS = 2 * 60 * 60 * 1000;
	// A trip that has already collected something finishes: the hour used to
	// run out with specimens in the bag and the place went back before the
	// hand-in, which is a trip spent for nothing. While the bag holds any of
	// the row's specimens the place is kept this long instead.
	const DWORD PLAYERBOT_BIOLOGIST_HERB_ERRAND_CARRY_MAX_MS = 3 * 60 * 60 * 1000;
	// And the cheapest catch-up of all: a bot that is in a first village
	// anyway - services, the market, a hand-in - works an outgrown herb row
	// while it is there, without taking a place on the errand, because that
	// adds no map change to the world at all. Bounded per arrival: this long
	// from the first ask on that map, and only again once the bot has been
	// somewhere else (PlayerBotMayWorkHerbRowHere).
	const DWORD PLAYERBOT_BIOLOGIST_HERB_VILLAGE_MS = 10 * 60 * 1000;
	// The same for an outgrown collect row, whose monsters stand in Orc Valley
	// and the Demon Tower: the frontier draw sent every bot with the row open
	// there at once - 997 of 1621 bots in the valley on SIZOWSKI's world and
	// 277 of 1098 on m2zip on 17 September, every other map empty. A row is
	// hours long, so the place is held longer than a herb trip.
	// When a trip ends - its quantum spent or its row finished - the bot goes
	// to the BACK of the queue rather than straight back to the front. The
	// places are a share of the live population and the map that holds them is
	// keyed by pid with no waiting list, so without this the same bots reclaim
	// a place the moment the sweep frees one and everyone else starves
	// ("Ryzyko glodzenia pozostalych", audit of 17 September, A.4/A.6).
	const DWORD PLAYERBOT_BIOLOGIST_ERRAND_COOLDOWN_MS = 30 * 60 * 1000;
	const int PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE = 100;
	const DWORD PLAYERBOT_BIOLOGIST_COLLECT_ERRAND_MAX_MS = 2 * 60 * 60 * 1000;
	// From this row up a specimen is a refine material too - the Orc Tooth,
	// the Curse Book, the Demon Souvenir - and a bot of any level may carry
	// one. Such a row is taken for a hand-in whatever the bot has outgrown,
	// and what the Biologist is still owed stays off the anvil
	// (GetPlayerBotBiologistReserve): "w pierwszej kolejnosci te przedmioty
	// maja trafiac do biologa, dopiero pozniej na sklep lub jako ulepszacz"
	// (Tieru, 15 September). Measured that day on the test world: 978 bots in
	// the Orc Tooth row and not one finished, while 358 bots carried 1484
	// teeth - past forty the row was outgrown and the teeth stayed in the bag.
	const int PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL = 30;
	// The key item, the monster that drops it, the affect and the casket used
	// to be four constants named after the Orc Tooth, read by four different
	// files. They are columns of the table now, so a row carries its own
	// second half and nothing has to be told about it twice.
	const size_t PLAYERBOT_BIOLOGIST_MISSION_COUNT =
			sizeof(PLAYERBOT_BIOLOGIST_MISSIONS) / sizeof(PLAYERBOT_BIOLOGIST_MISSIONS[0]);

	// Herbalism at Baek-Go (playerbot_herbalism.h). The package carries the
	// whole system - the onboarding quest, his special shop 14, 77 rows in
	// world.crafting_proto behind eight levels of recipe knowledge - and until
	// now nothing in this world used any of it: the recipes dropped from Metin
	// stones went to the merchant as an unknown item and the herbs went on the
	// counters as bulk goods. These are the numbers the quest itself uses,
	// read off the shipped files on 17 September.
	const BYTE PLAYERBOT_HERBALISM_MIN_LEVEL = 15;          // herbalism_onboarding
	const DWORD PLAYERBOT_HERBALISM_ONBOARD_FLOWER = 50721;  // Kwiat Brzoskwini
	const int PLAYERBOT_HERBALISM_ONBOARD_COUNT = 10;
	const DWORD PLAYERBOT_HERBALISM_FIRST_RECIPE = 50909;    // Fioletowa Mikstura
	// His shop, bought the way the fishing pass and the Forgetting Scroll are:
	// the counter is a quest window a bot cannot open, so the bottle is created
	// for the price the shop asks (world.shop_special, shop 14).
	// What a bot keeps of each herb for its own board before the rest goes on a
	// counter. A row takes five to fifteen of one herb, so this is a few
	// crafts' worth and no more: the bags hold tens of thousands of the two
	// common ones and the counters are where a player buys the rest.
	const int PLAYERBOT_HERBALISM_HERB_KEEP = 20;
	const DWORD PLAYERBOT_HERBALISM_BOTTLE_M = 50901;
	const DWORD PLAYERBOT_HERBALISM_BOTTLE_S = 50902;
	const DWORD PLAYERBOT_HERBALISM_BOTTLE_D = 50903;
	const int PLAYERBOT_HERBALISM_BOTTLE_PACK = 10;
	const long long PLAYERBOT_HERBALISM_BOTTLE_M_PRICE = 5000;
	const long long PLAYERBOT_HERBALISM_BOTTLE_S_PRICE = 25000;
	const long long PLAYERBOT_HERBALISM_BOTTLE_D_PRICE = 50000;
	// A craft spends the materials whether it succeeds or not (crafting.lua
	// removes them before the roll), so a bot keeps a reserve rather than
	// grinding its purse to nothing on 60% rows.
	const long long PLAYERBOT_HERBALISM_GOLD_RESERVE = 2000000;
	const int PLAYERBOT_HERBALISM_FREE_CELLS = 6;
	// One board visit is one craft and one recipe read: the interval is what
	// keeps a bot from standing at Baek-Go instead of playing.
	const DWORD PLAYERBOT_HERBALISM_VISIT_MIN_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_HERBALISM_VISIT_MAX_MS = 35 * 60 * 1000;
	const DWORD PLAYERBOT_HERBALISM_CRAFTS_PER_VISIT = 3;
	// What a bot keeps for itself before a line goes on the counter. A buff
	// lasts ten minutes and a boss is rarer than that, so a few of each is
	// plenty and the rest is what players have never been able to buy.
	const int PLAYERBOT_HERBALISM_POTION_KEEP = 5;
	// Drinking: only where it pays for the ten minutes it lasts - a boss, a
	// Metin stone, a Demon Tower floor - and never twice inside one fight.
	const DWORD PLAYERBOT_HERBALISM_DRINK_RETRY_MS = 60 * 1000;

	// What a row's hunt vnum means: every monster its item comes from on this
	// world, not the one the quest names. The quest's own hooks and the etc
	// table, read off the files on 17 September: the Orc Tooth from 601 (the
	// hook, 5%) and the Black Orcs 636/656 (etc, 1.17) - 601 stands in the
	// valley on two points, both in boss groups; its key from the hook on
	// 631-637; the Curse Book from the Tormentors 706/756 (etc, 2.70, no hook);
	// its key from the hook on 701-707 and 731-737; the Demon Souvenir from
	// 1001 (etc, 1.26) and its key from the hook on 1001-1004. Naming one vnum
	// made every other carrier worthless experience to a bot past its level,
	// and 129 bots stood in the valley in parties looking for a target. Any
	// other hunt means the monster it names.
	bool IsPlayerBotBiologistHuntRace(DWORD huntMob, DWORD race)
	{
		if (huntMob == 0)
			return false;
		if (race == huntMob)
			return true;
		switch (huntMob)
		{
			case 601: return race == 636 || race == 656;
			case 631: return race >= 632 && race <= 637;
			case 706: return race == 756;
			case 701: return (race >= 702 && race <= 707) || (race >= 731 && race <= 737);
			case 1002: return race == 1001 || race == 1003 || race == 1004;
		}
		return false;
	}

	// The specimens this world gives only through the etc table, by carrier,
	// with the probability ITEM_MANAGER keeps (etc_drop_item.txt times ten
	// thousand, against a range of four million). That roll fades with the
	// level gap - one percent at fifteen levels - and a bot of seventy on the
	// Orc Tooth row had one tooth in about seventeen thousand kills at the
	// world's rate; NotePlayerBotBiologistCarrierKill rolls what the gap took.
	struct TPlayerBotSpecimenCarrier { DWORD itemVnum; DWORD mobVnum; DWORD dropProb; };
	const TPlayerBotSpecimenCarrier PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[] = {
		{ 30006, 636, 11700 }, { 30006, 656, 11700 },
		{ 30047, 706, 27000 }, { 30047, 756, 27000 },
		{ 30015, 1001, 12600 }
	};

	// Canonical ``special.levelup_quest`` entries from questlib.lua.  These are
	// the ordinary Hunting Missions shown to a human player after each level;
	// playerbots use the very same quest flags and kill event, they merely make
	// the menu choice which a fake descriptor cannot click.  The first phase is
	// deliberately bounded to the M1/M2 levels that this AI can currently reach.
	struct TPlayerBotHuntingMission
	{
		DWORD firstMobVnum;
		WORD firstCount;
		DWORD secondMobVnum;
		WORD secondCount;
		BYTE expPercent;
	};

	const BYTE PLAYERBOT_HUNTING_FIRST_LEVEL = 2;
	// The table now runs to 55, which is as far as the hosted maps reach: every
	// row past 25 is questlib's own. Not every row can be done here - the
	// Bestial Arahans, the plagued of the newer Sohan, the strong apes and the
	// demons stand on maps this world does not host - so a bot picks the option
	// that stands where it is, then one that stands anywhere hosted, and a row
	// with neither is passed over rather than left to block every row after
	// it. A row a bot accepted and could not finish inside two hours is passed
	// over the same way: the monster is somewhere the bot is not going.
	const BYTE PLAYERBOT_HUNTING_MAX_LEVEL = 55;
	const int PLAYERBOT_HUNTING_STALL_SECONDS = 7200;
	// A mission this many levels below the bot is outgrown: its monster stands
	// on a map the bot has left for good. Nearly every bot at forty was found
	// holding a mission from fifteen, waiting for a wolf it would never see.
	const int PLAYERBOT_HUNTING_OUTGROWN_LEVELS = 10;
	const TPlayerBotHuntingMission PLAYERBOT_HUNTING_MISSIONS[] = {
		{ 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
		{ 171, 10, 172, 5, 10 }, { 171, 20, 172, 10, 10 },
		{ 172, 15, 173, 5, 10 }, { 173, 10, 174, 10, 10 },
		{ 174, 20, 178, 10, 10 }, { 178, 10, 175, 5, 10 },
		{ 178, 20, 175, 10, 10 }, { 175, 15, 179, 5, 10 },
		{ 175, 20, 179, 10, 10 }, { 179, 10, 180, 5, 10 },
		{ 180, 15, 176, 10, 10 }, { 176, 20, 181, 5, 10 },
		{ 181, 15, 177, 5, 10 }, { 181, 20, 177, 10, 10 },
		{ 177, 15, 184, 5, 10 }, { 177, 20, 184, 10, 10 },
		{ 184, 10, 182, 10, 10 }, { 182, 20, 183, 10, 10 },
		{ 183, 20, 352, 15, 10 }, { 352, 20, 185, 10, 0 },
		{ 185, 25, 354, 10, 0 }, { 354, 20, 451, 40, 0 },
		{ 451, 60, 402, 80, 0 }, { 551, 80, 454, 20, 0 },
		{ 552, 80, 456, 20, 0 }, { 456, 30, 554, 20, 0 },
		{ 651, 35, 554, 30, 0 }, { 651, 40, 652, 30, 0 },
		{ 652, 40, 2102, 30, 0 }, { 652, 50, 2102, 45, 0 },
		{ 653, 45, 2051, 40, 0 }, { 751, 35, 2103, 30, 0 },
		{ 751, 40, 2103, 40, 0 }, { 752, 40, 2052, 30, 0 },
		{ 754, 20, 2106, 20, 0 }, { 773, 30, 2003, 20, 0 },
		{ 774, 40, 2004, 20, 0 }, { 756, 40, 2005, 30, 0 },
		{ 757, 40, 2158, 20, 0 }, { 931, 40, 5123, 25, 0 },
		{ 932, 30, 5123, 30, 0 }, { 932, 40, 2031, 35, 0 },
		{ 933, 40, 2031, 40, 0 }, { 771, 50, 2032, 45, 0 },
		{ 772, 30, 5124, 30, 0 }, { 933, 35, 5125, 30, 0 },
		{ 934, 40, 5125, 35, 0 }, { 773, 40, 2033, 45, 0 },
		{ 774, 40, 5126, 20, 0 }, { 775, 50, 5126, 30, 0 },
		{ 934, 45, 2034, 45, 0 }, { 934, 50, 2034, 50, 0 },
		{ 776, 40, 1001, 30, 0 }, { 777, 40, 1301, 35, 0 }
	};

	// Where a hunting-mission monster stands, among the hosted maps: read out of
	// the regen files of the fourteen maps this world hosts. A vnum with no row
	// stands nowhere a bot can go.
	struct TPlayerBotMobHome { DWORD vnum; long map1; long map2; };
	const TPlayerBotMobHome PLAYERBOT_HUNTING_MOB_HOMES[] = {
		{ 552, 23, 0 }, { 456, 23, 0 }, { 554, 23, 0 },
		// Mount Sohan (61): the Infected the rows from 41 ask for.
		{ 901, 61, 0 }, { 902, 61, 0 }, { 903, 61, 0 }, { 904, 61, 0 }, { 905, 61, 0 }, { 906, 61, 0 },
		{ 931, 61, 0 }, { 932, 61, 0 }, { 933, 61, 0 }, { 934, 61, 0 }, { 935, 61, 0 }, { 936, 61, 0 },
		{ 651, 64, 0 }, { 652, 64, 0 }, { 653, 64, 0 },
		{ 751, 64, 0 }, { 752, 64, 0 }, { 754, 64, 0 }, { 756, 64, 0 }, { 757, 64, 0 },
		{ 2102, 63, 0 }, { 2051, 63, 0 }, { 2052, 63, 0 }, { 2106, 63, 0 },
		{ 2003, 63, 0 }, { 2004, 63, 0 }, { 2005, 63, 0 }, { 2158, 63, 0 },
		{ 2103, 63, 64 },
		{ 2031, 104, 0 }, { 2032, 104, 0 }, { 2033, 104, 0 }, { 2034, 104, 0 },
		// The Biologist's Orc Tooth: the Orc and the Elite Orc of the valley.
		{ 601, 64, 0 }, { 631, 64, 0 },
		// The Biologist's Curse Book: the Tormentors of the valley's central
		// island, measured at 68 spawn points each through the valley's own
		// group_group 306. 756 is already listed above for the hunting rows.
		{ 706, 64, 0 },
		// The Curse Book's key family, named by 701 (IsPlayerBotBiologistHuntRace).
		{ 701, 64, 0 },
		// The Demon Tower, now that game1 hosts it. These four are the whole of
		// the Biologist's level-50 row: 1001 carries the Demon Souvenir and all
		// four carry the key (30222) through the quest's own kill hook. They
		// stand nowhere else in this world, which is why that row was switched
		// off until the map moved.
		{ 1001, 66, 0 }, { 1002, 66, 0 }, { 1003, 66, 0 }, { 1004, 66, 0 }
	};

	// The map a listed monster stands on (its first home), or zero for a
	// village monster and for one the table does not know.
	long GetPlayerBotHuntingMobHome(DWORD vnum)
	{
		if (vnum < 500)
			return 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_HUNTING_MOB_HOMES) / sizeof(PLAYERBOT_HUNTING_MOB_HOMES[0]); ++i)
			if (PLAYERBOT_HUNTING_MOB_HOMES[i].vnum == vnum)
				return PLAYERBOT_HUNTING_MOB_HOMES[i].map1;
		return 0;
	}

	bool IsPlayerBotHuntingMobHosted(DWORD vnum, long lMapIndex = 0)
	{
		// Everything the first twenty-five rows asks for is starter game, and
		// every kingdom has its own: the villages are what host mobs under 500,
		// whichever kingdom the bot belongs to.
		if (vnum < 500)
			return lMapIndex == 0 || IsPlayerBotVillageMap(lMapIndex);
		for (size_t i = 0; i < sizeof(PLAYERBOT_HUNTING_MOB_HOMES) / sizeof(PLAYERBOT_HUNTING_MOB_HOMES[0]); ++i)
		{
			const TPlayerBotMobHome& home = PLAYERBOT_HUNTING_MOB_HOMES[i];
			if (home.vnum != vnum)
				continue;
			return lMapIndex == 0 || home.map1 == lMapIndex || home.map2 == lMapIndex;
		}
		return false;
	}

	struct TPlayerBotMapPoint { long x; long y; };
	// A hunting hub with the level band it is for and whether it is a party's
	// work. A solo bot never picks a party hub; a leader with a party of the
	// challenge size may.
	// wBossRace names the boss a hub exists for, or 0. A boss hub is not
	// scored by what the population has seen there - one monster every half
	// hour is a density of nothing, which is why the Orc Chief's and the Spider
	// Queen's hubs were never chosen in a day of logs - but by whether the boss
	// is standing there now, asked of the sector itself.
	struct TPlayerBotHuntingHub { long x; long y; BYTE bMinLevel; BYTE bMaxLevel; bool bNeedsParty; WORD wBossRace; };
	// How long a "boss alive" answer is trusted, and what a hub with a living
	// boss scores: above any camp, so the crowd (the Orc Chief) or the party
	// (the Spider Queen) goes.
	const DWORD PLAYERBOT_RAID_BOSS_CHECK_INTERVAL = 30000;
	// The Bestial Captain (591, level 42, boss) of Bokjung: metin2_map_b3's
	// boss.txt cell (787,688) on base (102400,204800), every hour, ten cells
	// of spread. Bokjung's wandering has no hub table - it rotates spawn
	// clusters - so the Captain is a detour taken while he stands, by anybody
	// of the band. Nine Tails (1901, level 72, boss) is a Sohan hub row.
	const long PLAYERBOT_M2_CAPTAIN_X = 181100;
	const long PLAYERBOT_M2_CAPTAIN_Y = 273600;
	const BYTE PLAYERBOT_M2_CAPTAIN_MIN_LEVEL = 35;
	const long PLAYERBOT_SOHAN_NINE_TAILS_X = 433300;
	const long PLAYERBOT_SOHAN_NINE_TAILS_Y = 216500;
	const int PLAYERBOT_RAID_WORTH = 100000;
	// How many bots one boss is worth calling out. A boss needs a raid, not a
	// province: past this many already on him the hub is scored like any other
	// ground, so the rest of the band goes on hunting instead of queueing.
	const int PLAYERBOT_RAID_CROWD = 12;
	// A raid of twelve was a ceiling, not a floor. On a map carrying forty
	// bots it left the Spider Queen - level 60, 193 408 hit points - with six
	// of them (a raid nobody called takes half the crowd), while the other
	// thirty-four hunted soldiers within sight of her. "If there are that many
	// of them, all of them should go for her; weak alone, together they can
	// take her." So the room on a boss hub scales with the map: at least the
	// old twelve, or this share of every bot on the map, whichever is more.
	const int PLAYERBOT_RAID_MAP_SHARE_CALLED_PERCENT = 60;
	const int PLAYERBOT_RAID_MAP_SHARE_UNCALLED_PERCENT = 35;
	// And once that many have set out, a bot in reach of the boss puts her
	// above the trash round her. A level-60 boss against a level-48 bot sits
	// at delta twelve, the lowest scoring bucket there is - ten thousand
	// against a soldier's three hundred thousand - so a raider that arrived
	// fought soldiers beside her until she killed it. See the target scorer.
	const int PLAYERBOT_RAID_SWARM_MIN = 3;
	const int PLAYERBOT_RAID_SWARM_TARGET_BONUS = 1500000;
	// How long a guild's call stands. Long enough to walk across a frontier
	// map, short enough that a boss killed five minutes ago stops summoning
	// anybody.
	const DWORD PLAYERBOT_RAID_CALL_TIME = 180000;
	// Exact world coordinates of the two rare M2 enemies from
	// metin2_map_b3/boss.txt (map base 102400,204800). They are the classic
	// level-30 weapon hunt: Bestial Archer (533) and Specialist (534).
	const TPlayerBotMapPoint PLAYERBOT_M2_BESTIAL_HOTSPOTS[2] = {
		{ 132300, 259300 }, // Bestial Archer, local 299,545
		{ 129700, 275100 }  // Bestial Specialist, local 273,703
	};
	const TPlayerBotMapPoint PLAYERBOT_METIN_HOTSPOTS[12] = {
		{ 33400, 211800 }, { 29200, 164500 }, { 32900, 161600 },
		{ 39400, 160900 }, { 39400, 187100 }, { 63500, 204100 },
		{ 62600, 213600 }, { 88600, 201000 }, { 94000, 164300 },
		{ 83700, 119300 }, { 67600, 117700 }, { 63500, 133500 }
	};

	// ---------------------------------------------------------------------
	// The hunting ground of each village, measured per map.
	//
	// Joan's tables above were placed by hand off metin2_map_b1, and for a year
	// they were the only ones there were - so every branch of the wander pass
	// tested for map 21, 23 or 24 and a bot anywhere else fell through to a
	// random walk. Shinsoo and Jinno host the same monsters in the same bands
	// and put them in entirely different places, so none of these numbers can
	// be Chunjo's plus an offset; each row was measured from that map's own
	// regen.txt and stone.txt with tools/generate_wander_hubs.py: the richest
	// 6400-unit squares, kept apart, each landed on a real spawn point that is
	// standable on server_attr and outside the safe zone, with the median
	// monster level within 2500 units as its band.
	//
	// Chunjo's rows are the hand-made tables unchanged. The measurement agrees
	// with them where it can be checked - the three Bokjung bosses come out on
	// the constants this file has always carried - which is what says the rows
	// for the other four villages can be trusted.
	struct TPlayerBotVillageHub { long x; long y; int mobLevel; };

	// Shinsoo M1, metin2_map_a1
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_1[32] = {
		{ 419700, 904500, 25 }, { 425700, 913000, 25 }, { 431500, 912100, 25 },
		{ 426000, 918900, 25 }, { 432800, 904900, 25 }, { 456400, 983000, 9 },
		{ 464400, 910600, 13 }, { 444800, 969600, 9 }, { 422700, 905200, 25 },
		{ 438900, 937900, 7 }, { 433400, 937400, 10 }, { 464000, 937200, 3 },
		{ 443900, 963600, 9 }, { 451600, 981600, 10 }, { 463800, 1008100, 13 },
		{ 451900, 936700, 3 }, { 477000, 980800, 3 }, { 450000, 918800, 9 },
		{ 464000, 988400, 7 }, { 488700, 969900, 3 }, { 476700, 906400, 18 },
		{ 469900, 974200, 3 }, { 432900, 949100, 9 }, { 468500, 905300, 13 },
		{ 427000, 964600, 13 }, { 427400, 925700, 18 }, { 482600, 905000, 24 },
		{ 438000, 912400, 20 }, { 425000, 937900, 20 }, { 490900, 951300, 3 },
		{ 490700, 956200, 3 }, { 458200, 917400, 9 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_METINS_1[12] = {
		{ 470300, 905000 }, { 461900, 967600 }, { 431900, 910800 },
		{ 417200, 903000 }, { 429900, 905000 }, { 443600, 913400 },
		{ 483500, 918200 }, { 491800, 922000 }, { 458800, 930100 },
		{ 439800, 930300 }, { 500600, 930700 }, { 464900, 933200 }
	};

	// Shinsoo M2, metin2_map_a3. tools/generate_wander_hubs.py 3 --count 24
	// --band --spacing 4500: the richest 6400-unit cells of regen.txt, each hub
	// on the real spawn point nearest the cell's centre, standable and outside
	// the safe zone; the third number is the median monster level within 2500
	// units. The first twelve are the table 2.0.8 shipped, which that tool made
	// with --count 12 and no band. Bands here are 27 (the 402/403 edge), 29-30
	// (the Black Wind ground of 27-33) and 35 (the 501-504 ground of 29-36);
	// the tigers of 18-20 are nowhere a majority and get no hub of their own -
	// a bot under 26 takes the nearest band and kills them on the way.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_3[24] = {
		{ 394500, 848100, 30 }, { 355800, 855300, 29 }, { 386500, 855000, 30 },
		{ 368100, 854200, 30 }, { 337000, 899900, 30 }, { 335900, 836400, 35 },
		{ 348700, 860300, 29 }, { 329200, 842600, 35 }, { 323300, 846100, 35 },
		{ 329700, 853900, 35 }, { 329100, 887500, 30 }, { 330100, 836000, 35 },
		{ 329700, 849500, 35 }, { 386800, 835500, 30 }, { 328700, 879800, 30 },
		{ 341300, 910900, 30 }, { 342200, 835900, 35 }, { 341300, 841000, 35 },
		{ 323100, 879900, 29 }, { 368000, 834700, 29 }, { 360900, 853400, 30 },
		{ 331100, 897900, 30 }, { 341700, 905300, 30 }, { 393700, 867400, 27 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_METINS_3[12] = {
		{ 321300, 886400 }, { 387300, 898400 }, { 320700, 829700 },
		{ 361100, 831000 }, { 329200, 836600 }, { 368500, 862900 },
		{ 323900, 865100 }, { 354400, 870300 }, { 345700, 872600 },
		{ 328900, 877400 }, { 392200, 880100 }, { 340000, 880700 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_BESTIALS_3[2] = {
		{ 330100, 875300 }, { 339400, 887400 }
	};

	// Chunjo M2, metin2_map_b3. Until 2.0.58 this was twelve hand-placed
	// points "Bokjung has rotated since before this table had a name"; measured
	// against regen.txt on 16 September, three of them stood two to four
	// kilometres from the nearest spawn rectangle and two more beside fewer
	// than fifteen points, with no band on any. Generated like the other two
	// now, same tool, same arguments.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_23[24] = {
		{ 125800, 264800, 29 }, { 150300, 280600, 30 }, { 177200, 233900, 30 },
		{ 130500, 258900, 27 }, { 163100, 222500, 30 }, { 145600, 285700, 29 },
		{ 123900, 251700, 29 }, { 164000, 273600, 29 }, { 156700, 285500, 29 },
		{ 118400, 265300, 30 }, { 156300, 272000, 30 }, { 176200, 241800, 30 },
		{ 124900, 245500, 27 }, { 171000, 225500, 30 }, { 125800, 271600, 30 },
		{ 187200, 253800, 29 }, { 139800, 251700, 27 }, { 155900, 226000, 29 },
		{ 176400, 226000, 29 }, { 189200, 233200, 35 }, { 168500, 290700, 35 },
		{ 184100, 265200, 29 }, { 191300, 246600, 35 }, { 170200, 272700, 29 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_METINS_23[12] = {
		{ 152600, 225700 }, { 135400, 263200 }, { 161400, 228700 },
		{ 190000, 236700 }, { 141100, 270500 }, { 179400, 273400 },
		{ 154300, 274300 }, { 171200, 287400 }, { 130000, 287500 },
		{ 184800, 289400 }, { 156000, 290900 }, { 179100, 224700 }
	};

	// Jinno M1, metin2_map_c1
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_41[32] = {
		{ 975700, 219600, 25 }, { 987800, 316300, 25 }, { 962900, 221200, 20 },
		{ 968800, 221100, 23 }, { 982600, 222200, 20 }, { 950400, 244700, 3 },
		{ 956600, 233500, 9 }, { 937600, 309500, 13 }, { 983100, 303600, 10 },
		{ 970300, 291000, 3 }, { 956600, 246900, 3 }, { 982800, 272200, 3 },
		{ 988700, 215900, 25 }, { 963800, 240000, 7 }, { 938300, 240000, 9 },
		{ 937800, 296400, 9 }, { 976500, 291400, 6 }, { 993500, 317300, 25 },
		{ 988800, 304200, 13 }, { 989000, 284200, 12 }, { 987700, 233900, 18 },
		{ 986900, 220600, 20 }, { 943500, 284900, 3 }, { 963100, 290900, 3 },
		{ 969500, 216800, 25 }, { 982700, 216900, 25 }, { 989700, 298000, 18 },
		{ 940300, 304200, 12 }, { 956700, 225100, 18 }, { 995100, 277700, 13 },
		{ 983600, 279100, 4 }, { 970400, 296900, 4 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_METINS_41[12] = {
		{ 954200, 231300 }, { 941000, 284300 }, { 936700, 255300 },
		{ 955600, 239500 }, { 945300, 217400 }, { 981800, 218600 },
		{ 992900, 224100 }, { 961100, 233600 }, { 971100, 245600 },
		{ 991000, 246800 }, { 963300, 250100 }, { 956500, 251700 }
	};

	// Jinno M2, metin2_map_c3, the same way. Its band-35 hubs are all in the
	// south (y 283-292k) and Bakra's gate from Pyongmoo is in the north; that
	// half was empty until the band choice below sent the 33+ there.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_43[24] = {
		{ 835800, 246500, 30 }, { 906700, 283800, 35 }, { 905700, 279400, 30 },
		{ 873400, 291600, 35 }, { 834300, 227300, 30 }, { 848300, 290300, 35 },
		{ 834600, 265800, 29 }, { 835200, 231900, 30 }, { 834900, 239100, 30 },
		{ 878900, 272800, 30 }, { 892600, 271000, 30 }, { 898600, 285000, 29 },
		{ 899600, 246200, 27 }, { 841900, 289900, 35 }, { 855000, 272000, 29 },
		{ 878900, 291600, 35 }, { 866800, 278200, 29 }, { 855400, 267100, 30 },
		{ 879900, 232200, 27 }, { 900300, 252900, 27 }, { 848000, 284500, 35 },
		{ 854800, 284500, 35 }, { 905300, 289700, 35 }, { 834800, 220900, 30 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_METINS_43[12] = {
		{ 886100, 218700 }, { 837900, 219400 }, { 860400, 217600 },
		{ 867600, 221200 }, { 854700, 229000 }, { 903300, 245400 },
		{ 883200, 268800 }, { 849300, 269600 }, { 876900, 276100 },
		{ 847800, 286900 }, { 873300, 291300 }, { 851300, 293700 }
	};
	const TPlayerBotMapPoint PLAYERBOT_GROUND_BESTIALS_43[2] = {
		{ 841100, 270400 }, { 861200, 275300 }
	};

	// The three guild maps. Same size, same eight monster types, three
	// different layouts - which is why one table cannot serve all of them.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_4[10] = {
		{ 135600, 35400, 0 }, { 144300, 43200, 0 }, { 142000, 35000, 0 },
		{ 151600, 35400, 0 }, { 150500, 43000, 0 }, { 156900, 23600, 0 },
		{ 137400, 15500, 0 }, { 143700, 15600, 0 }, { 147800, 9700, 0 },
		{ 170500, 40500, 0 }
	};
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_24[10] = {
		{ 189700, 6000, 0 }, { 196900, 7000, 0 }, { 206800, 7800, 0 },
		{ 212600, 9400, 0 }, { 204200, 12400, 0 }, { 209200, 18800, 0 },
		{ 195800, 18100, 0 }, { 187600, 15100, 0 }, { 216000, 15900, 0 },
		{ 201500, 21700, 0 }
	};
	// Jinno's guild map is two pieces that do not join: the south-east fifth
	// (111 536 of 570 456 open cells, fifteen spawn groups) has no way in from
	// the Town.txt point. Its hub, (270400, 46600), was planned "unreachable"
	// from every bot that drew it - Champion of Urtopy's world tried it every
	// few minutes on 21 September - and is replaced by the spawn point of the
	// main piece furthest from the other nine (scratchpad pick_hub44.py).
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_44[10] = {
		{ 260800, 22100, 0 }, { 264300, 23000, 0 }, { 246900, 8600, 0 },
		{ 239300, 13400, 0 }, { 234300, 10900, 0 }, { 259700, 29000, 0 },
		{ 235100, 17400, 0 }, { 243500, 23200, 0 }, { 241000, 8000, 0 },
		{ 241600, 24700, 0 }
	};

	// Iwakura's community patch 2, point 14: the first villages' hunting is
	// the White Oath soldiers (301-304, 331-334, 351-354) and the bears
	// (110-113, 139-142, 180-183), whose drops are refine materials - not the
	// dogs by the gate. These are the hubs of the three tables above whose
	// 2500-unit ring holds at least four of their spawn points and a third of
	// all, measured on m2zip's own regen.txt through group.txt and
	// group_group.txt (the mt2009 share, 22 September); the rest are dogs,
	// wolves, boars and the cursed ground of the youngest. Keyed by map and
	// point, so a table reordered keeps its marks.
	struct TPlayerBotValueHub { long mapIndex; long x; long y; };
	const TPlayerBotValueHub PLAYERBOT_M1_VALUE_HUBS[] = {
		{ 1, 419700, 904500 },
		{ 1, 425700, 913000 },
		{ 1, 431500, 912100 },
		{ 1, 426000, 918900 },
		{ 1, 432800, 904900 },
		{ 1, 464400, 910600 },
		{ 1, 422700, 905200 },
		{ 1, 463800, 1008100 },
		{ 1, 476700, 906400 },
		{ 1, 468500, 905300 },
		{ 1, 427000, 964600 },
		{ 1, 427400, 925700 },
		{ 1, 482600, 905000 },
		{ 1, 438000, 912400 },
		{ 1, 425000, 937900 },
		{ 21, 61600, 133500 },
		{ 21, 59500, 123600 },
		{ 21, 83500, 130000 },
		{ 21, 87200, 147300 },
		{ 21, 84600, 197500 },
		{ 21, 89800, 195300 },
		{ 21, 86700, 209800 },
		{ 21, 61100, 214300 },
		{ 21, 29900, 196400 },
		{ 21, 33500, 209800 },
		{ 21, 30200, 164500 },
		{ 21, 32600, 178200 },
		{ 21, 35000, 135500 },
		{ 21, 28500, 146900 },
		{ 21, 42100, 129300 },
		{ 41, 975700, 219600 },
		{ 41, 987800, 316300 },
		{ 41, 962900, 221200 },
		{ 41, 968800, 221100 },
		{ 41, 982600, 222200 },
		{ 41, 937600, 309500 },
		{ 41, 988700, 215900 },
		{ 41, 993500, 317300 },
		{ 41, 988800, 304200 },
		{ 41, 989000, 284200 },
		{ 41, 987700, 233900 },
		{ 41, 986900, 220600 },
		{ 41, 969500, 216800 },
		{ 41, 982700, 216900 },
		{ 41, 989700, 298000 },
		{ 41, 940300, 304200 },
		{ 41, 956700, 225100 },
		{ 41, 995100, 277700 },
	};
	// How far under the bot's level a valuable hub may be taken when its own
	// band has none: the soldiers and the bears are worth it a few levels down.
	const int PLAYERBOT_M1_VALUE_HUB_UNDER = 6;
	// And the one bot in a hundred of the first village's Grinders that farms
	// materials there for sale until it can buy or make its class's level-30
	// weapon at +8 and an armour of level 18 or 26 at +9 (community patch 2,
	// point 14): it does not advance until it can.
	const int PLAYERBOT_M1_FARMER_PERCENT = 1;
	const int PLAYERBOT_M1_FARMER_WEAPON_PLUS = 8;
	const int PLAYERBOT_M1_FARMER_ARMOUR_PLUS = 9;

	// Joan's eight party camps, by hand, with the level band each was measured
	// at. The other two first villages take the eight densest clusters of their
	// own grinding table, which is what these eight are.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_CAMPS_21[8] = {
		{ 39000, 200200, 9 },  // South-West White Oath Camp
		{ 37000, 168400, 10 }, // West White Oath Camp
		{ 84600, 197500, 12 }, // South-East Bear / Tiger Camp
		{ 61000, 203600, 6 },  // South Dense Boar / Wolf Plains
		{ 80300, 135700, 9 },  // North-East Plateau Camp
		{ 61600, 133500, 12 }, // North Meadow Camp
		{ 35000, 135500, 21 }, // North-West Lykos Territory
		{ 85800, 169700, 3 }   // East Cursed Beast Camp
	};

	// Joan's own thirty-two, kept where they were written.
	const TPlayerBotVillageHub PLAYERBOT_GROUND_HUBS_21[32] = {
		// 1. North Quadrant (Meadows & North Road)
		{ 61600, 133500, 12 }, { 55600, 135200, 12 }, { 70600, 135800, 9 }, { 59500, 123600, 18 },
		// 2. North-East Quadrant (Plateaus & Hills)
		{ 80300, 135700, 9 }, { 83500, 130000, 12 }, { 75500, 143600, 6 }, { 87200, 147300, 12 },
		// 3. East Quadrant (Cursed Animals & Tigers)
		{ 85800, 169700, 3 }, { 80300, 165800, 1 }, { 88600, 162800, 9 }, { 82900, 178300, 3 },
		// 4. South-East Quadrant (Brown Bears & Tiger Groves)
		{ 84600, 197500, 12 }, { 78300, 191000, 3 }, { 89800, 195300, 12 }, { 86700, 209800, 20 },
		// 5. South Quadrant (Wild Boars, Grey Wolves, Tigers)
		{ 61000, 203600, 6 }, { 52700, 194700, 4 }, { 67400, 194700, 3 }, { 61100, 214300, 21 },
		// 6. South-West Quadrant (White Oath Camps & Black Bears)
		{ 39000, 200200, 9 }, { 29900, 196400, 16 }, { 46200, 206200, 10 }, { 33500, 209800, 18 },
		// 7. West Quadrant (Valley of Mi-Jung, White Oath)
		{ 37000, 168400, 10 }, { 30200, 164500, 12 }, { 44700, 165800, 3 }, { 32600, 178200, 12 },
		// 8. North-West Quadrant (Lykos territory, Cursed Wolves)
		{ 35000, 135500, 21 }, { 40600, 145000, 9 }, { 28500, 146900, 12 }, { 42100, 129300, 18 }
	};

	struct TPlayerBotVillageGround
	{
		long mapIndex;
		const TPlayerBotVillageHub* hubs;
		size_t hubCount;
		const TPlayerBotVillageHub* camps;   // party ground, first villages only
		size_t campCount;
		const TPlayerBotMapPoint* metins;
		size_t metinCount;
		const TPlayerBotMapPoint* bestials;  // 533/534, second villages only
		TPlayerBotMapPoint captain;          // 591, second villages only
	};

	const TPlayerBotVillageGround* GetPlayerBotVillageGround(long mapIndex)
	{
		static const TPlayerBotVillageGround rows[] = {
			{ 1, PLAYERBOT_GROUND_HUBS_1, 32, PLAYERBOT_GROUND_HUBS_1, 8,
				PLAYERBOT_GROUND_METINS_1, 12, NULL, { 0, 0 } },
			{ 3, PLAYERBOT_GROUND_HUBS_3, 24, NULL, 0,
				PLAYERBOT_GROUND_METINS_3, 12, PLAYERBOT_GROUND_BESTIALS_3, { 369700, 906200 } },
			{ 4, PLAYERBOT_GROUND_HUBS_4, 10, NULL, 0, NULL, 0, NULL, { 0, 0 } },
			{ 21, PLAYERBOT_GROUND_HUBS_21, 32, PLAYERBOT_GROUND_CAMPS_21, 8,
				PLAYERBOT_METIN_HOTSPOTS, 12, NULL, { 0, 0 } },
			{ 23, PLAYERBOT_GROUND_HUBS_23, 24, NULL, 0,
				PLAYERBOT_GROUND_METINS_23, 12, PLAYERBOT_M2_BESTIAL_HOTSPOTS,
				{ PLAYERBOT_M2_CAPTAIN_X, PLAYERBOT_M2_CAPTAIN_Y } },
			{ 24, PLAYERBOT_GROUND_HUBS_24, 10, NULL, 0, NULL, 0, NULL, { 0, 0 } },
			{ 41, PLAYERBOT_GROUND_HUBS_41, 32, PLAYERBOT_GROUND_HUBS_41, 8,
				PLAYERBOT_GROUND_METINS_41, 12, NULL, { 0, 0 } },
			{ 43, PLAYERBOT_GROUND_HUBS_43, 24, NULL, 0,
				PLAYERBOT_GROUND_METINS_43, 12, PLAYERBOT_GROUND_BESTIALS_43, { 899600, 287800 } },
			{ 44, PLAYERBOT_GROUND_HUBS_44, 10, NULL, 0, NULL, 0, NULL, { 0, 0 } },
		};
		for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
			if (rows[i].mapIndex == mapIndex)
				return &rows[i];
		return NULL;
	}

	// Yongan's bank and Pyongmoo's, measured the same way Joan's was, with
	// tools/generate_fishing_bank.py: a dry standable cell centre within two
	// cells of water, spaced so two anglers never share a tile, each carrying
	// the water point it faces. Both are anchored on that village's own Rybak,
	// because the bait trip and the fishing trip have to be one walk.
	//
	// They are shorter rows than Joan's eighty. That is the map: Yongan's shore
	// near its bait merchant runs about three thousand units. More anglers than
	// stands is a case the claim code already handles - it shares a stand rather
	// than refusing to fish.
	const TPlayerBotFishingStandPoint PLAYERBOT_FISHING_STANDS_1[] = {
		{ 484025, 962025, 484125, 962125 }, { 484125, 962075, 484125, 962175 },
		{ 484225, 962025, 484175, 962125 }, { 484325, 962075, 484225, 962125 },
		{ 484425, 962025, 484325, 962125 }, { 484525, 962075, 484425, 962125 },
		{ 484625, 962025, 484525, 962125 }, { 484725, 962075, 484625, 962125 },
		{ 484825, 962025, 484725, 962125 }, { 484925, 962075, 484825, 962125 },
		{ 485025, 962025, 484925, 962125 }, { 485125, 962075, 485025, 962125 },
		{ 485225, 962025, 485125, 962125 }, { 485325, 962075, 485225, 962125 },
		{ 485425, 962025, 485325, 962125 }, { 485525, 962075, 485425, 962125 },
		{ 485625, 962025, 485525, 962125 }, { 485725, 962075, 485625, 962125 },
		{ 485825, 962025, 485725, 962125 }, { 485925, 962075, 485825, 962125 },
		{ 486025, 962025, 485925, 962125 }
	};
	const TPlayerBotFishingStandPoint PLAYERBOT_FISHING_STANDS_41[] = {
		{ 964275, 252925, 964375, 253025 }, { 964375, 252975, 964425, 253025 },
		{ 964525, 252975, 964425, 253075 }, { 964175, 252975, 964275, 253025 },
		{ 964625, 252925, 964525, 253025 }, { 964125, 253075, 964225, 253025 },
		{ 964025, 253025, 964125, 253125 }, { 963925, 253075, 964025, 253125 },
		{ 963825, 253025, 963925, 253125 }, { 963725, 253075, 963825, 253125 },
		{ 965225, 253025, 965125, 253025 }, { 963625, 253025, 963725, 253125 },
		{ 963575, 253125, 963675, 253125 }, { 965325, 253075, 965225, 253125 },
		{ 965425, 253025, 965325, 253125 }, { 963475, 253175, 963575, 253225 },
		{ 963375, 253125, 963475, 253225 }, { 965525, 253075, 965425, 253125 },
		{ 965625, 253025, 965525, 253125 }, { 963275, 253175, 963375, 253225 },
		{ 965675, 252925, 965725, 253025 }, { 963175, 253125, 963275, 253225 },
		{ 965775, 252975, 965775, 253025 }, { 963075, 253175, 963175, 253225 },
		{ 965875, 252925, 965825, 253025 }, { 965975, 252975, 965875, 253025 },
		{ 963025, 253275, 963125, 253225 }, { 966075, 252925, 965975, 253025 },
		{ 962925, 253225, 963025, 253325 }, { 966175, 252975, 966075, 253025 },
		{ 962825, 253275, 962925, 253325 }, { 966275, 252925, 966175, 253025 },
		{ 966375, 252975, 966275, 253025 }, { 962725, 253225, 962825, 253325 },
		{ 966475, 252925, 966375, 253025 }, { 962625, 253275, 962725, 253325 },
		{ 966525, 253025, 966425, 253025 }, { 962525, 253225, 962625, 253325 },
		{ 962425, 253275, 962525, 253325 }, { 966625, 253075, 966525, 253125 },
		{ 962325, 253225, 962425, 253325 }, { 962275, 253325, 962375, 253325 },
		{ 966725, 253025, 966625, 253125 }, { 966825, 253075, 966725, 253125 },
		{ 966925, 253025, 966825, 253125 }, { 962175, 253375, 962275, 253425 },
		{ 967025, 253075, 966925, 253125 }, { 962075, 253325, 962175, 253425 },
		{ 962075, 253475, 962175, 253475 }, { 967125, 253025, 967025, 253125 },
		{ 961975, 253425, 962075, 253525 }, { 967225, 253075, 967125, 253125 },
		{ 967325, 253025, 967225, 253125 }, { 961875, 253475, 961975, 253525 },
		{ 967425, 253075, 967325, 253125 }, { 961775, 253425, 961875, 253525 },
		{ 961775, 253575, 961875, 253575 }, { 967525, 253025, 967425, 253125 },
		{ 967625, 253075, 967525, 253125 }, { 961675, 253525, 961775, 253625 }
	};

	struct TPlayerBotFishingBank
	{
		long mapIndex;
		const TPlayerBotFishingStandPoint* stands;
		size_t standCount;
		TPlayerBotMapPoint fisherman;   // 9009, the bait and rod merchant
		TPlayerBotMapPoint centre;      // what "am I at the water yet" measures
		int radius;
	};

	const TPlayerBotFishingBank* GetPlayerBotFishingBank(long mapIndex)
	{
		static const TPlayerBotFishingBank rows[] = {
			{ 1, PLAYERBOT_FISHING_STANDS_1,
				sizeof(PLAYERBOT_FISHING_STANDS_1) / sizeof(PLAYERBOT_FISHING_STANDS_1[0]),
				{ 482700, 961900 }, { 485025, 962050 }, 2600 },
			{ 21, PLAYERBOT_FISHING_STANDS, PLAYERBOT_FISHING_STAND_COUNT,
				{ PLAYERBOT_FISHERMAN_X, PLAYERBOT_FISHERMAN_Y },
				{ PLAYERBOT_FISHING_BANK_X, PLAYERBOT_FISHING_BANK_Y },
				PLAYERBOT_FISHING_BANK_RADIUS },
			{ 41, PLAYERBOT_FISHING_STANDS_41,
				sizeof(PLAYERBOT_FISHING_STANDS_41) / sizeof(PLAYERBOT_FISHING_STANDS_41[0]),
				{ 964400, 252800 }, { 964650, 253275 }, 3600 },
		};
		for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
			if (rows[i].mapIndex == mapIndex)
				return &rows[i];
		return NULL;
	}


	enum EPlayerBotRole
	{
		BOT_ROLE_MOB_GRINDER = 0,
		BOT_ROLE_METIN_HUNTER = 1,
		BOT_ROLE_PARTY_FIGHTER = 2
	};

	enum EPlayerBotMerchantCategory
	{
		BOT_MERCHANT_MISC = 0,
		BOT_MERCHANT_WEAPON,
		BOT_MERCHANT_ARMOR
	};

	enum EPlayerBotTownVisitPhase
	{
		BOT_TOWN_PHASE_NONE = 0,
		BOT_TOWN_PHASE_TRAINER,
		BOT_TOWN_PHASE_TRAINER_WAIT,
		BOT_TOWN_PHASE_GATE_IN,
		BOT_TOWN_PHASE_GATE_CROSS_IN,
		BOT_TOWN_PHASE_WEAPON_MERCHANT,
		BOT_TOWN_PHASE_WEAPON_WAIT,
		BOT_TOWN_PHASE_ARMOR_MERCHANT,
		BOT_TOWN_PHASE_ARMOR_WAIT,
		BOT_TOWN_PHASE_MISC_MERCHANT,
		BOT_TOWN_PHASE_MISC_WAIT,
		BOT_TOWN_PHASE_BLACKSMITH,
		BOT_TOWN_PHASE_BLACKSMITH_WAIT,
		BOT_TOWN_PHASE_GATE_OUT,
		BOT_TOWN_PHASE_GATE_CROSS_OUT,
		// Appended, never inserted: the panel's status file carries this value
		// as a number and inserting one shifts every phase after it.
		BOT_TOWN_PHASE_SKILL_RESET,
		BOT_TOWN_PHASE_SKILL_RESET_WAIT,
		BOT_TOWN_PHASE_SAFEBOX,
		BOT_TOWN_PHASE_SAFEBOX_WAIT
	};

	enum EPlayerBotLongTermGoal
	{
		BOT_GOAL_LEVEL_UP = 0,
		BOT_GOAL_SURVIVE,
		BOT_GOAL_CHOOSE_PROFESSION,
		BOT_GOAL_GET_EQUIPMENT,
		BOT_GOAL_RESTOCK,
		BOT_GOAL_REFINE,
		BOT_GOAL_MASTER_SKILL,
		BOT_GOAL_HUNT_METIN,
		BOT_GOAL_PARTY_CHALLENGE,
		BOT_GOAL_BIOLOGIST,
		BOT_GOAL_HUNTING,
		BOT_GOAL_HORSE,
		BOT_GOAL_FISHING
	};

	enum EPlayerBotCurrentAction
	{
		BOT_ACTION_IDLE = 0,
		BOT_ACTION_TRAVEL,
		BOT_ACTION_FIGHT,
		BOT_ACTION_LOOT,
		BOT_ACTION_RECOVER,
		BOT_ACTION_TRAIN,
		BOT_ACTION_SHOP,
		BOT_ACTION_REFINE,
		BOT_ACTION_READ_BOOK,
		BOT_ACTION_SOCKET_STONE,
		BOT_ACTION_PARTY_ASSEMBLE,
		BOT_ACTION_BIOLOGIST,
		BOT_ACTION_STABLE,
		// Standing at an open stall. Distinct from BOT_ACTION_SHOP, which is a
		// visit to an NPC merchant: the panel has to tell the two apart to list
		// who is actually trading. Appended, never inserted - the id goes into
		// the status file the panel reads.
		BOT_ACTION_STALL,
		BOT_ACTION_FISHING,
		// Walking the stall ring looking for something to buy. Distinct from
		// BOT_ACTION_SHOP, which is the NPC merchant round, and from
		// BOT_ACTION_STALL, which is standing behind a counter of one's own.
		BOT_ACTION_MARKET,
		// Bringing monsters to the party. Distinct from BOT_ACTION_FIGHT on
		// purpose: the Archer is not fighting, it tags and runs.
		BOT_ACTION_LURE,
		// Standing about in town with nothing to do. Distinct from
		// BOT_ACTION_RECOVER, which is a bot getting its health back, and from
		// BOT_ACTION_STALL, which is a bot behind a counter. Appended, never
		// inserted - the id goes into the status file the panel reads.
		BOT_ACTION_TOWN_REST,
		// Digging at an ore vein. Appended for the same reason as the one above:
		// both panels read these ids out of playerbot_status.tsv by position.
		BOT_ACTION_MINING
	};

	// Where an Archer is in its course. WAIT_READY is the absence of a session
	// rather than a stage of one, so it is LURE_STAGE_NONE.
	enum EPlayerBotLureStage
	{
		LURE_STAGE_NONE = 0,
		LURE_STAGE_PLAN,
		LURE_STAGE_APPROACH,
		LURE_STAGE_TAG,
		LURE_STAGE_CONFIRM,
		LURE_STAGE_RETURN,
		LURE_STAGE_HANDOFF,
		LURE_STAGE_RECOVER
	};

	enum EPlayerBotPersonality
	{
		BOT_PERSONALITY_STEADY_ADVENTURER = 0,
		BOT_PERSONALITY_METIN_BREAKER,
		BOT_PERSONALITY_TEAM_COMPANION,
		BOT_PERSONALITY_GEAR_SPECIALIST,
		BOT_PERSONALITY_CAREFUL_COLLECTOR,
		// A trader. Every other bot is chasing something - a level, a horse, a
		// better weapon - and they all end up playing the same way. This one keeps
		// a stall because that is what it does, not because it happened to have a
		// spare while passing through town.
		BOT_PERSONALITY_MERCHANT,
		BOT_PERSONALITY_WANDERER,
		// The droppers. Each farms one thing for the market rather than for
		// itself: the Metin dropper keeps the skill books a stone gives instead
		// of vendoring the ones it cannot read, the M3 dropper stays on Waryong
		// for the level-30 weapons whether or not it owns one, the M2 dropper
		// camps the Bestials of Bokjung for theirs, and the medal dropper works
		// the Monkey Dungeon past the point its own horse needs. Appended, never
		// inserted - the id goes into the status file the panel reads.
		BOT_PERSONALITY_METIN_DROPPER,
		BOT_PERSONALITY_M3_DROPPER,
		BOT_PERSONALITY_M2_DROPPER,
		BOT_PERSONALITY_MEDAL_DROPPER
	};

	bool IsPlayerBotDropper(BYTE personality)
	{
		return personality == BOT_PERSONALITY_METIN_DROPPER ||
				personality == BOT_PERSONALITY_M3_DROPPER ||
				personality == BOT_PERSONALITY_M2_DROPPER ||
				personality == BOT_PERSONALITY_MEDAL_DROPPER;
	}

	BYTE GetPlayerBotPersonalityByPID(DWORD dwPID);

	// Iwakura's personality system ("SYSTEM OSOBOWOSCI v2.0", 19 September):
	// playerbot_persona_rules.h is the policy, playerbot_mood.h and
	// playerbot_persona.h the engine's half. The PERSONA key of the weights
	// file switches all of it; off is the world as it was before.
	//
	// A gap between two ticks longer than this is not play: the bot was logged
	// out, or on another core, and its moods must not age by the absence.
	const DWORD PLAYERBOT_PERSONA_TICK_MAX_DT = 10000;
	// How often the mood's clocks are written back to the quest flags. A change
	// of mood is written at once; the clocks only lose up to this much across a
	// restart.
	const DWORD PLAYERBOT_PERSONA_SAVE_INTERVAL = 5 * 60 * 1000;
	// A fight this recent is hunting wherever the bot stands, so the drought
	// clock runs; a village with no fight in it is not.
	const DWORD PLAYERBOT_MOOD_HUNTING_COMBAT_MS = 30000;
	// A fight this recent earns SLABY its pause before the next pack.
	const DWORD PLAYERBOT_MOOD_PAUSE_FIGHT_MS = 6000;
	// SLABY only goes AFK somewhere it will not simply die for it: not in a
	// fight, not with a monster on it, not hurt.
	const int PLAYERBOT_MOOD_AFK_MIN_HP_PERCENT = 70;
	// Community patch 2, point 13: fifty-four weapon families nobody buys at
	// +0..+3 - Iwakura's list, bound to base vnums through the names of the
	// price table (playerbot_price_tables.h) and checked against
	// world.item_proto. The counters of every bot together carry at most
	// PLAYERBOT_JUNK_WEAPON_MARKET_CAP of them; each further one is the
	// merchant's. Ten of the families are level-65 weapons the pickup rule
	// keeps (IsPlayerBotPickupGoods): they are still picked up, and this cap
	// decides where they go.
	const DWORD PLAYERBOT_JUNK_WEAPON_BASES[] = {
		70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 190,
		1040, 1050, 1060, 1070, 1080, 1090, 1100, 1110, 1120,
		2060, 2070, 2080, 2090, 2100, 2110, 2120,
		3060, 3070, 3080, 3090, 3100, 3110, 3120, 3140,
		5050, 5060, 5070, 5080, 5090, 5100, 5130,
		7010, 7020, 7050, 7060, 7070, 7080, 7090, 7100, 7110, 7130, 7140
	};
	const int PLAYERBOT_JUNK_WEAPON_MAX_REFINE = 3;
	const int PLAYERBOT_JUNK_WEAPON_MARKET_CAP = 5;

	// A weapon family is its base vnum plus the refine, 0..9.
	bool IsPlayerBotJunkWeaponVnum(DWORD vnum)
	{
		const DWORD grade = vnum % 10;
		if (grade > (DWORD)PLAYERBOT_JUNK_WEAPON_MAX_REFINE)
			return false;
		const DWORD base = vnum - grade;
		for (size_t i = 0; i < sizeof(PLAYERBOT_JUNK_WEAPON_BASES) / sizeof(PLAYERBOT_JUNK_WEAPON_BASES[0]); ++i)
			if (PLAYERBOT_JUNK_WEAPON_BASES[i] == base)
				return true;
		return false;
	}

	// A bot that has been AFK and was struck puts the next stop off this long.
	const DWORD PLAYERBOT_MOOD_AFK_INTERRUPTED_RETRY = 5 * 60 * 1000;
	// Nor does it leave its own drop lying on the ground to go AFK (Iwakura's
	// community patch 2, point 7): the stop waits while the loot pass has
	// something to take, looked at again this often - and gives up waiting
	// after the second figure, because a drop the walk never reaches must not
	// cancel the habit for good.
	const DWORD PLAYERBOT_MOOD_AFK_LOOT_RETRY_MS = 3000;
	const DWORD PLAYERBOT_MOOD_AFK_LOOT_WAIT_MAX_MS = 90 * 1000;
	// How often the census of personalities and moods is written.
	const DWORD PLAYERBOT_PERSONA_CENSUS_INTERVAL = 10 * 60 * 1000;
	// The quest flags a bot's moods and its Grinder's promise live in, so that
	// a restart, a channel move or a life-schedule rest does not reroll them.
	// Every one is written as its value plus one, so zero is "never written".
	const char* const PLAYERBOT_PERSONA_FLAG_MOOD = "playerbot.persona_mood";
	const char* const PLAYERBOT_PERSONA_FLAG_LOCK = "playerbot.persona_lock";
	const char* const PLAYERBOT_PERSONA_FLAG_LOCK_LEFT = "playerbot.persona_lock_s";
	const char* const PLAYERBOT_PERSONA_FLAG_PLAYED = "playerbot.persona_played_s";
	const char* const PLAYERBOT_PERSONA_FLAG_DROUGHT = "playerbot.persona_drought_s";
	const char* const PLAYERBOT_PERSONA_FLAG_ADVANCED = "playerbot.persona_adv";
	const char* const PLAYERBOT_PERSONA_FLAG_LOCK_LEVEL = "playerbot.persona_lock_lv";
	// Community patch 2, point 2: gave grinding up for good, and the tier its
	// 33% was last rolled at (so a tier is rolled once).
	const char* const PLAYERBOT_PERSONA_FLAG_QUIT = "playerbot.persona_quit";
	const char* const PLAYERBOT_PERSONA_FLAG_QUIT_TIER = "playerbot.persona_quit_tier";
	// Community patch 2, point 4: a goal dropper that has met its goal.
	const char* const PLAYERBOT_PERSONA_FLAG_MEDAL_GOAL = "playerbot.medal_goal";

	// The Grinder and the Conqueror (Zdobywca). A Grinder that meets the Law of
	// Advancement is asked once an hour whether it moves on or stays to push
	// its gear to +8 and +9 first ("moze podjac decyzje o przedluzeniu pobytu"),
	// the chance in percent by its character (GetPlayerBotAdvanceChance).
	const DWORD PLAYERBOT_PERSONA_ADVANCE_ROLL_INTERVAL = 60 * 60 * 1000;
	const int PLAYERBOT_PERSONA_ADVANCE_CHANCE = 60;
	// The first question comes this soon after the law is first met, so a bot
	// that has just finished its gear does not wait an hour to be asked.
	const DWORD PLAYERBOT_PERSONA_ADVANCE_FIRST_ROLL = 2 * 60 * 1000;

	// The gambler (Hazardzista, playerbot_gambler.h). "Duza nadwyzka Yang" is a
	// purse of at least this on Iwakura's scale (ScalePlayerBotIwakuraPrice:
	// two million at the stock rate, sixty at 3000%), of which the session may
	// spend GAMBLE_BUDGET_PERCENT - fees, scrolls, materials and what burns.
	const DWORD PLAYERBOT_GAMBLE_MIN_PURSE_BASE = 2000000;
	// Iwakura's community patch 2, point 10: a Trader back in its first village
	// with its weapon at this plus, its armour at that one and this purse on
	// his scale (the yang rate's curve and the inflation) turns gambler.
	const DWORD PLAYERBOT_GAMBLE_TOWN_PURSE_BASE = 3000000;
	// Community patch 2, point 11: this share of the bots, as Perfectionists,
	// looks at the market for a finished piece - a weapon, an armour, a shield
	// or a helmet at +8 or +9, of its class and at most this many levels under
	// it - before it takes its own to the anvil, and buys it instead. The anvil
	// waits this long for the purchase, and the look is not taken again for
	// the second figure.
	const int PLAYERBOT_READY_GEAR_PERCENT = 15;
	const int PLAYERBOT_READY_GEAR_MIN_PLUS = 8;
	const int PLAYERBOT_READY_GEAR_LEVEL_WINDOW = 10;
	const DWORD PLAYERBOT_READY_GEAR_WAIT_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_READY_GEAR_RECHECK_MS = 60 * 60 * 1000;
	const int PLAYERBOT_GAMBLE_TOWN_WEAPON_PLUS = 7;
	const int PLAYERBOT_GAMBLE_TOWN_ARMOUR_PLUS = 6;
	// A bot that qualifies and does not take it is asked again this much later;
	// one that has gambled rests this long before the next session.
	const DWORD PLAYERBOT_GAMBLE_RETRY_MIN_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_GAMBLE_RETRY_MAX_MS = 60 * 60 * 1000;
	const DWORD PLAYERBOT_GAMBLE_REST_MIN_MS = 3 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_GAMBLE_REST_MAX_MS = 6 * 60 * 60 * 1000;
	// The document ends a session on its budget or its +9 and nothing else;
	// this is only the net under a session something else stranded.
	const DWORD PLAYERBOT_GAMBLE_MAX_MS = 20 * 60 * 1000;
	// "Nastepnie wybiera kolejna osobowosc lecz nie moze to byc Hazardzista":
	// not within this long of a Perfectionist's spell.
	const DWORD PLAYERBOT_GAMBLE_AFTER_PERFECT_MS = 30 * 60 * 1000;
	// One attempt at the anvil every 1.5 to 3 seconds - a player's click.
	const DWORD PLAYERBOT_GAMBLE_STEP_MIN_MS = 1500;
	const DWORD PLAYERBOT_GAMBLE_STEP_MAX_MS = 3000;
	// What the LPP calls valuable: a family his tier list rates 3 or better,
	// in PvE or in PvP. Body armour, helmets and shields are not in that list
	// (he judges them by level and lines) and are taken as they come.
	const int PLAYERBOT_GAMBLE_MIN_TIER = 3;
	// At most this many pieces taken out of the safebox for one session.
	const int PLAYERBOT_GAMBLE_SAFEBOX_TAKE = 4;
	// And at most this many bases in the bag before it stops buying more off
	// the counters ("Jesli brakuje mu bazy lub ulepszaczy, przeszukuje sklepy
	// offline na rynku").
	const int PLAYERBOT_GAMBLE_MARKET_BASES = 3;

	// The stone hunter (Pogromca, playerbot_anti_pk.h and the target section):
	// how often a bot busy with a monster looks round for a stone, and how
	// often one at a stone looks for somebody of another kingdom breaking it.
	const DWORD PLAYERBOT_POGROMCA_PROBE_MS = 3000;
	const DWORD PLAYERBOT_POGROMCA_RIVAL_SCAN_MS = 2000;
	// A stone that has killed the bot more than POGROMCA_MAX_DEATHS times is
	// left alone this long.
	const DWORD PLAYERBOT_POGROMCA_GIVE_UP_MS = 30 * 60 * 1000;
	// The Anti-PK protocol: a player's blow is a fight while it is this recent,
	// and a foe further than this, or in a safe zone, is let go.
	const DWORD PLAYERBOT_ANTIPK_STRUCK_MEMORY_MS = 12000;
	const int PLAYERBOT_ANTIPK_FOE_RANGE = 3000;
	// A party answers for a member struck this recently ("cala grupa rzuca sie
	// na agresora"), from as far as this.
	const DWORD PLAYERBOT_ANTIPK_PARTY_MEMORY_MS = 8000;
	const int PLAYERBOT_ANTIPK_PARTY_RANGE = 2500;
	// And a guild for a member a person has struck (Iwakura's community patch
	// 2, point 15, Amos's idea): every bot of the guild within this of the
	// aggressor drops what it is doing and goes for him, and holds him while
	// he stays within it. The call stays open this long after the last blow,
	// which is what lets a bot twelve kilometres off set out at all.
	const int PLAYERBOT_ANTIPK_GUILD_RANGE = 12000;
	const DWORD PLAYERBOT_ANTIPK_GUILD_MEMORY_MS = 15000;

	// Iwakura's Rybak (playerbot_activities.h): from level thirty, never in a
	// party, and mostly a bad mood's answer - "bardzo duza szansa" for SLABY,
	// "sporadycznie" for NORMALNY, and BARDZO DOBRY has better things to do.
	// The answer is rolled once per window per bot, so the question can be asked
	// every tick without the answer flickering. The FISHING weight scales both.
	const DWORD PLAYERBOT_RYBAK_ROLL_WINDOW_MS = 30 * 60 * 1000;
	const int PLAYERBOT_RYBAK_SLABY_PERCENT = 75;
	const int PLAYERBOT_RYBAK_NORMALNY_PERMILLE = 40;
	// "Faza Rybaka trwa maksymalnie 1 godzine": a bad mood's session runs
	// half an hour to an hour, a good mood's episode is short.
	const DWORD PLAYERBOT_RYBAK_SLABY_SESSION_MIN = 30 * 60 * 1000;
	const DWORD PLAYERBOT_RYBAK_SLABY_SESSION_MAX = 60 * 60 * 1000;
	const DWORD PLAYERBOT_RYBAK_EPISODE_MIN = 10 * 60 * 1000;
	const DWORD PLAYERBOT_RYBAK_EPISODE_MAX = 20 * 60 * 1000;
	const DWORD PLAYERBOT_RYBAK_MAX_SESSION = 60 * 60 * 1000;
	// "Bot otwiera co 5 Malz": shells are opened in fives.
	const int PLAYERBOT_RYBAK_SHELL_BATCH = 5;

	// Iwakura's Gornik (playerbot_mining.h): a bot with a pickaxe digs a vein
	// in sight, until the vein is gone ("Ruda znika z mapy"), and after a fight
	// goes straight back to the same vein ("natychmiast wraca do kopania tej
	// samej rudy"). How often it looks for a vein, the net under a session that
	// outlives its vein, the rest after a vein is dug out, the return after a
	// fight, and the clock of the jewellery work that follows a smelt.
	const DWORD PLAYERBOT_GORNIK_PROBE_MS = 10000;
	const DWORD PLAYERBOT_GORNIK_SESSION_CAP = 20 * 60 * 1000;
	const DWORD PLAYERBOT_GORNIK_REST_MIN = 5 * 60 * 1000;
	const DWORD PLAYERBOT_GORNIK_REST_MAX = 10 * 60 * 1000;
	const DWORD PLAYERBOT_GORNIK_RESUME_MS = 5000;
	const DWORD PLAYERBOT_GORNIK_SOCKET_WORK_MS = 5000;

	// Iwakura's Zielarz (Baek-Go's board, playerbot_herbalism.h): a
	// Conqueror's errand from level forty-five, spending at most a tenth of the
	// purse a visit came with ("nie wykorzystuje w tym celu wiecej niz 10%
	// swoich Yang").
	const int PLAYERBOT_ZIELARZ_MIN_LEVEL = 45;
	const int PLAYERBOT_ZIELARZ_SPEND_PERCENT = 10;
	// And the water's rubbish goes to the Fisherman once the bag is this full.
	const int PLAYERBOT_RYBAK_JUNK_SELL_PERCENT = 70;

	// Iwakura's Towarzysz (playerbot_companions.h): a companion looks for a
	// person to play with as well as a bot ("graczy badz innych botow na
	// zblizonym poziomie, +/- kilka poziomow"). A person is asked by a bot at
	// most once in PLAYERBOT_COMPANION_HUMAN_ASK_GAP by anybody, a refusal -
	// no answer in the engine's ten seconds, or a no - leaves them alone for
	// PLAYERBOT_COMPANION_HUMAN_DECLINED_GAP, the same bot asks the same person
	// once in PLAYERBOT_COMPANION_HUMAN_PAIR_GAP, and a bot asks anybody once
	// in PLAYERBOT_COMPANION_ASK_GAP. The game options' "block party invites"
	// and "block party requests" are the engine's own and refuse it as they
	// refuse a player.
	const int PLAYERBOT_COMPANION_HUMAN_LEVEL_RANGE = 5;
	const int PLAYERBOT_COMPANION_HUMAN_RANGE = 1800;
	const DWORD PLAYERBOT_COMPANION_HUMAN_ASK_GAP = 20 * 60 * 1000;
	const DWORD PLAYERBOT_COMPANION_HUMAN_DECLINED_GAP = 45 * 60 * 1000;
	const DWORD PLAYERBOT_COMPANION_HUMAN_PAIR_GAP = 3 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_COMPANION_ASK_GAP = 10 * 60 * 1000;
	// The engine's invitation lives ten seconds; the answer is read after it.
	const DWORD PLAYERBOT_COMPANION_ASK_ANSWER_MS = 12000;
	// A companion Shaman's pass over its party's buffs.
	const DWORD PLAYERBOT_COMPANION_BUFF_INTERVAL = 3000;

	// Iwakura's Najemnik (playerbot_companions.h). The look round for a bot
	// that keeps dying, from a bot that is hunting; the walk to it, given up
	// after PLAYERBOT_MERC_APPROACH_MS; the distance the offer is made at; how
	// long a bot's distress is remembered; how many contracts a core carries
	// (in thousandths of the live bots, never under PLAYERBOT_MERC_CONTRACTS_MIN);
	// the distance the client keeps from its mercenary; how long a paused
	// contract waits for its mercenary and how long any contract lives,
	// paused or not; and the rests after one - the mercenary's, and the
	// client's before it is carried again.
	const DWORD PLAYERBOT_MERC_SCAN_MIN_MS = 45000;
	const DWORD PLAYERBOT_MERC_SCAN_MAX_MS = 90000;
	const int PLAYERBOT_MERC_MIN_LEVEL = 20;
	const DWORD PLAYERBOT_MERC_APPROACH_MS = 3 * 60 * 1000;
	const int PLAYERBOT_MERC_OFFER_DISTANCE = 1200;
	// "Zauwazy w swoim otoczeniu na mapie": the first contract struck on m2zip
	// had its mercenary walk forty-four kilometres across Orc Valley to make
	// the offer, which is a map's width and not a surrounding.
	const int PLAYERBOT_MERC_NOTICE_RANGE = 20000;
	const DWORD PLAYERBOT_MERC_DISTRESS_MS = 20 * 60 * 1000;
	const int PLAYERBOT_MERC_CONTRACTS_PER_MILLE = 20;
	const int PLAYERBOT_MERC_CONTRACTS_MIN = 2;
	const int PLAYERBOT_MERC_CARRY_RANGE = 2200;
	const DWORD PLAYERBOT_MERC_FOLLOW_INTERVAL = 2000;
	const DWORD PLAYERBOT_MERC_PAUSE_MAX_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_MERC_WALL_MAX_MS = 3 * 60 * 60 * 1000;
	const int PLAYERBOT_MERC_REJOIN_DISTANCE = 1500;
	const DWORD PLAYERBOT_MERC_CHECK_INTERVAL = 5000;
	const DWORD PLAYERBOT_MERC_COOLDOWN_MIN_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_MERC_COOLDOWN_MAX_MS = 40 * 60 * 1000;
	const DWORD PLAYERBOT_MERC_CLIENT_COOLDOWN_MS = 2 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_MERC_REFUSED_COOLDOWN_MS = 5 * 60 * 1000;

	// Iwakura's Useful Items List (playerbot_lpp.h): under this many free
	// single cells in the box the list stops keeping the bag's pieces.
	const int PLAYERBOT_LPP_BOX_MIN_FREE_CELLS = 9;

	// Why a bot is fighting a player (playerbot_anti_pk.h): the status line
	// says it, so it lives here with the state.
	enum EPlayerBotFoeReason
	{
		BOT_FOE_NONE = 0,
		BOT_FOE_STRUCK,       // it struck this bot
		BOT_FOE_PARTY,        // it struck a member of this bot's party
		BOT_FOE_GRUDGE,       // it killed this bot, which has come back for it
		BOT_FOE_STONE_RIVAL,  // another kingdom's, breaking this bot's stone
		BOT_FOE_GUILD         // a person who struck a member of this bot's guild
	};

	// The gambler's plan for one piece (playerbot_gambler.h): the item, the
	// plus it was rolled to reach, and whether a Blessing Scroll has already
	// failed on it - after which it goes back to +7 at the anvil and is sold;
	// and whether the piece is finished with, for sale as it stands.
	struct TPlayerBotGamblePlan
	{
		DWORD dwItemId;
		BYTE bTarget;
		bool bScrollFailed;
		bool bDone;
	};

	// One member of the AI state, with a constructor of its own: it never
	// joins the long initialiser list of TPlayerBotAIState, so -Wreorder has
	// nothing to say about where it stands.
	struct TPlayerBotPersona
	{
		playerbot_persona::TMood mood;
		// The quest flags have been read (they arrive from the db core a moment
		// after the bot enters the game), something worth writing has changed,
		// and the clocks of the last tick and of the next save.
		bool bRestored;
		bool bDirty;
		DWORD dwLastTick;
		DWORD dwNextSave;
		// The personality the last planning pass decided, since when, and when
		// it is next decided. The old personality drawn by pid at login is kept
		// too: under the switch the bot plays by its character (a dropper's
		// becomes the character it leans to), and switching the system off must
		// be able to give it back.
		BYTE bPersona;
		DWORD dwPersonaSince;
		DWORD dwNextDecide;
		BYTE bDrawnPersonality;
		// SLABY's habits: until when the bot pauses between two packs, the
		// fight the last pause answered, and the stop from the keyboard.
		DWORD dwPauseUntil;
		DWORD dwPausedAfterFight;
		DWORD dwAfkUntil;
		DWORD dwNextAfkAt;
		// Since when the stop has been waiting for the bot's own drop.
		DWORD dwAfkLootWaitSince;
		// The plus the class's level-30 weapon had when this town visit began,
		// 0xFF when it had none (GetPlayerBotLevel30Aim).
		BYTE bLevel30VisitStartPlus;
		// The trader's book purse (PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT): what
		// the window began with, what it has spent, and when it began.
		long long llBookBudgetBase;
		long long llBookBudgetSpent;
		DWORD dwBookBudgetSince;
		// The market Perfectionist's look for finished gear
		// (PLAYERBOT_READY_GEAR_PERCENT): until when the anvil waits for the
		// purchase, and when the market was last looked at for it.
		DWORD dwReadyGearWaitUntil;
		DWORD dwReadyGearCheckedAt;
		// The Grinder that gave grinding up (community patch 2, point 2), and
		// the last tier its chance was rolled at.
		bool bQuitGrinding;
		BYTE bQuitRolledTier;
		// The goal dropper's graduation (PLAYERBOT_MEDAL_GOAL_PERCENT) and the
		// clock of its look at the purse.
		bool bMedalGoalDone;
		DWORD dwNextMedalGoalCheck;
		// The Grinder: whether the law is met and the bot has chosen to level
		// (a Conqueror), the level it holds at otherwise (zero until its tier's
		// lock is reached), when it is next asked, and the monster deaths that
		// say a Conqueror has outgrown its gear.
		bool bAdvanced;
		BYTE bLockLevel;
		DWORD dwNextAdvanceRoll;
		playerbot_persona::TDeathWindow deaths;
		// The player-death counter of the engine (PLAYER_STATS_DEATH_FROM_
		// PLAYER_FLAG on mt2009) as last read, so a death can be told apart.
		long long llPlayerDeaths;
		// The Perfectionist's purse: what the bot held when its town visit
		// began, of which the anvil takes at most PERFECT_BUDGET_PERCENT; and
		// when its last Perfectionist spell ended, since the document says the
		// next personality after one may not be the gambler.
		long long llVisitGoldStart;
		DWORD dwPerfectEndedAt;
		// The gambler (playerbot_gambler.h): the session, its purse and what it
		// has spent of the GAMBLE_BUDGET_PERCENT, when it must end at the latest,
		// when the next may start, the next step's clock, what it has done, and
		// one plan per piece on the anvil. The storekeeper is visited once a
		// session, first, for the pieces and scrolls put away there.
		bool bGambling;
		long long llGambleGoldStart;
		long long llGambleSpent;
		DWORD dwGambleUntil;
		DWORD dwNextGambleAt;
		DWORD dwNextGambleStep;
		BYTE bGambleNines;
		BYTE bGambleBurned;
		BYTE bGambleFinished;
		BYTE bGambleDowngraded;
		WORD wGambleAttempts;
		bool bGambleSafeboxChecked;
		BYTE bGambleSafeboxTaken;
		std::vector<TPlayerBotGamblePlan> vecGamblePlans;
		// The Anti-PK protocol (playerbot_anti_pk.h): the last player who
		// struck the bot and when (CHARACTER::Damage tells the manager, mt2009),
		// the character it is fighting and why, the deaths at a player's hand
		// that make it give ground, the ground it gave up and until when, and
		// the hour at the water one capitulation in twelve ends in.
		DWORD dwStruckByVID;
		DWORD dwStruckByPID;
		DWORD dwStruckAt;
		DWORD dwFoeVID;
		BYTE bFoeReason;
		DWORD dwFoeSince;
		DWORD dwNextRivalScan;
		DWORD dwCapitulatedUntil;
		playerbot_persona::TPkDeaths pkDeaths;
		long lAvoidSpotMap;
		long lAvoidSpotX;
		long lAvoidSpotY;
		DWORD dwAvoidSpotUntil;
		DWORD dwFishingSpellUntil;
		// The stone hunter: the stone it is breaking and how often it has died
		// at it, whether it has turned on the stone's pack below 35%, and the
		// clock of its look round for a stone.
		DWORD dwPogromcaStoneVID;
		BYTE bPogromcaDeaths;
		bool bPogromcaClearing;
		DWORD dwNextStoneProbe;
		// The Zielarz's purse at Baek-Go's board, of which a visit spends at
		// most PLAYERBOT_ZIELARZ_SPEND_PERCENT.
		long long llHerbGoldStart;
		// The companion's phase (playerbot_companions.h): its draw against the
		// PARTY slider, until when the draw holds while the bot is solo, the
		// solo stretch after a party, whether it was in a party at the last
		// look, and the person it last asked to play with - who, how and when,
		// and when it may ask anybody again.
		WORD wCompanionDraw;
		DWORD dwCompanionPhaseEnd;
		DWORD dwCompanionBreakUntil;
		bool bWasInParty;
		DWORD dwAskedHumanPid;
		DWORD dwAskedHumanAt;
		BYTE bAskedHow;
		DWORD dwNextHumanAsk;
		// The mercenary: the client it is walking to and until when, the clock
		// of its look round for one, and its rest after a contract.
		DWORD dwMercClientPid;
		DWORD dwMercApproachUntil;
		DWORD dwNextMercScan;
		DWORD dwMercCooldownUntil;
		// The bag's eighty percent as the last planning pass found it: the
		// party finder asks it of every bot in sight, and walking every bag
		// in sight for it would cost more than the rest of the finder.
		bool bBagFull;
		// The Useful Items List (playerbot_lpp.h): what the storekeeper holds
		// of each kept family (a gear family by its +0 vnum, a soul stone by
		// its own) as the last visit found the box, and whether a visit has
		// looked since the bot entered the game. Until one has, the box counts
		// as empty, which keeps more rather than less.
		std::map<DWORD, BYTE> mapLppStored;
		bool bLppStoredKnown;
		// The stored pieces the list let go to the market (by item id), which
		// the dead-stock rule must not send back down.
		std::set<DWORD> setLppReleased;
		// The box had no room left at the last visit: the list stops keeping
		// the bag's pieces (they sell as they always did) until a visit finds
		// room again, or a full box would leave a full bag for good.
		bool bLppBoxFull;

		TPlayerBotPersona() : bRestored(false), bDirty(false), dwLastTick(0), dwNextSave(0),
			bPersona(playerbot_persona::PERSONA_GRINDER), dwPersonaSince(0), dwNextDecide(0),
			bDrawnPersonality(BOT_PERSONALITY_STEADY_ADVENTURER),
			dwPauseUntil(0), dwPausedAfterFight(0), dwAfkUntil(0), dwNextAfkAt(0),
			dwAfkLootWaitSince(0), bLevel30VisitStartPlus(0xFF),
			llBookBudgetBase(0), llBookBudgetSpent(0), dwBookBudgetSince(0),
			dwReadyGearWaitUntil(0), dwReadyGearCheckedAt(0), bQuitGrinding(false), bQuitRolledTier(0),
			bMedalGoalDone(false), dwNextMedalGoalCheck(0),
			bAdvanced(false), bLockLevel(0), dwNextAdvanceRoll(0), llPlayerDeaths(-1),
			llVisitGoldStart(0), dwPerfectEndedAt(0), bGambling(false), llGambleGoldStart(0),
			llGambleSpent(0), dwGambleUntil(0), dwNextGambleAt(0), dwNextGambleStep(0),
			bGambleNines(0), bGambleBurned(0), bGambleFinished(0), bGambleDowngraded(0),
			wGambleAttempts(0), bGambleSafeboxChecked(false), bGambleSafeboxTaken(0),
			dwStruckByVID(0), dwStruckByPID(0), dwStruckAt(0), dwFoeVID(0), bFoeReason(0),
			dwFoeSince(0), dwNextRivalScan(0), dwCapitulatedUntil(0), lAvoidSpotMap(0),
			lAvoidSpotX(0), lAvoidSpotY(0), dwAvoidSpotUntil(0), dwFishingSpellUntil(0),
			dwPogromcaStoneVID(0), bPogromcaDeaths(0), bPogromcaClearing(false),
			dwNextStoneProbe(0), llHerbGoldStart(0),
			wCompanionDraw(playerbot_persona::COMPANION_DRAW_NONE), dwCompanionPhaseEnd(0),
			dwCompanionBreakUntil(0), bWasInParty(false), dwAskedHumanPid(0), dwAskedHumanAt(0),
			bAskedHow(0), dwNextHumanAsk(0), dwMercClientPid(0), dwMercApproachUntil(0),
			dwNextMercScan(0), dwMercCooldownUntil(0), bBagFull(false), bLppStoredKnown(false),
			bLppBoxFull(false) {}
	};

	enum EPlayerBotAmbition
	{
		BOT_AMBITION_LEVEL = 0,
		BOT_AMBITION_EQUIPMENT,
		BOT_AMBITION_METINS,
		BOT_AMBITION_HORSE,
		BOT_AMBITION_BIOLOGIST,
		BOT_AMBITION_SKILLS,
		BOT_AMBITION_TRADE
	};

	// One acquaintance. Affinity only: the specification also wants hostility,
	// from PvP kills and stolen bosses, and this world has neither PvP nor a
	// hook that could honestly attribute a stolen kill - so a hostility counter
	// would be a field that is always zero.
	struct TPlayerBotFriend
	{
		DWORD dwPID;
		int iAffinity;
		DWORD dwLastInteractionTime;
		TPlayerBotFriend() : dwPID(0), iAffinity(0), dwLastInteractionTime(0) {}
	};

	struct TPlayerBotAIState
	{
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		playerbot_offline::State offlineShop;
#endif
		// Iwakura's personality and mood (TPlayerBotPersona above).
		TPlayerBotPersona persona;
		TPlayerBotAIState() :
			dwTargetVID(0),
			dwSpawnTime(0),
			dwLastBotSkillTime(0),
			dwLastKillerVID(0),
			dwLastDeathTime(0),
			lDeathX(0),
			lDeathY(0),
			bDeathCount(0),
			dwNextAttackTime(0),
			dwNextPotionTime(0),
			dwNextManaPotionTime(0),
			dwNextChestTime(0),
			dwNextBoosterTime(0),
			dwNextScrollRefineTime(0),
			dwNextPotionLogTime(0),
			dwDeathDetectedTime(0),
			dwNextReviveAttemptTime(0),
			dwNextGearAttemptTime(0),
			dwNextEquipmentCheckTime(0),
			dwNextStatCheckTime(0),
			dwNextSkillCheckTime(0),
			dwNextSkillBookTime(0),
			dwNextSoulStoneTime(0),
			dwNextThirdHandTime(0),
			dwNextSkillReallocateTime(0),
			dwNextProgressionChestCheckTime(0),
			dwNextBuffCheckTime(0),
			dwNextSkillCastTime(0),
			dwNextGearLogTime(0),
			dwNextPersistTime(0),
			dwNextRecoveryProtectionTime(0),
			dwNextRecoveryHealTime(0),
			dwRetreatStartedTime(0),
			dwNextRetreatMoveTime(0),
			dwRetreatThreatVID(0),
			dwNextRefineCheckTime(0),
			dwNextBonusCheckTime(0),
			dwBonusFocusItem(0),
			dwNextChatTime(0),
			dwLastStatusChatTime(0),
			dwNextStatusProbeTime(0),
			dwLastStatusTargetVID(0),
			dwNextBiologistCheckTime(0),
			dwNextBiologistActionTime(0),
			dwNextHerbalistCheckTime(0),
			dwNextHerbalistActionTime(0),
			dwNextHorseCheckTime(0),
			dwNextHorseActionTime(0),
			dwNextHorseRideCheckTime(0),
			dwNextFishingCheckTime(0),
			dwNextFishingActionTime(0),
			dwFishingCastTime(0),
			dwFishingIdleSince(0),
			dwFishingSessionEndTime(0),
			dwNextFishingProgressLogTime(0),
			dwBakeUntil(0),
			dwNextWorldTravelTime(0),
			dwTravelBlockedSince(0),
			dwNextRemoteRefineReturnTime(0),
			dwDungeonEnteredTime(0),
			dwM3EnteredTime(0),
			dwM3RevisitAfter(0),
			dwFrontierEnteredTime(0),
			dwShopOpenedTime(0),
			dwShopCloseTime(0),
			bShopStandsInRow(0),
			bShopLastStandSold(false),
			bShopOpenReason(0),
			dwShopWeightsGeneration(0),
			dwNextShopKeepTime(0),
			dwNextShoppingTime(0),
			dwMarketTripUntil(0),
			dwMarketBrowseTime(0),
			dwMarketStallVID(0),
			dwNextShopDebugTime(0),
			dwMonkeyReversePortalBlockUntil(0),
			dwMonkeyChamberTime(0),
			dwNextLootPickupTime(0),
			dwNextLootSearchTime(0),
			dwNextLootThreatCheckTime(0),
			dwNextLootCleanupTime(0),
			dwNextInventoryMaintenanceTime(0),
			dwNextWanderTime(0),
			dwNextPartyCheckTime(0),
			dwNextPartyShareTime(0),
			dwPartyExpireTime(0),
			dwNextLureTime(0),
			dwNextMultiPullTime(0),
			dwMultiPullStartedTime(0),
			dwNextMultiPullActionTime(0),
			dwMultiPullTargetVID(0),
			dwNextShopCheckTime(0),
			dwNextSkillResetTime(0),
			dwNextStackMergeTime(0),
			dwEquipPendingSince(0),
			dwEmergencyScavengeUntil(0),
			dwTownWaitUntil(0),
			dwStoneFightStartTime(0),
			dwStoneProgressVID(0),
			dwStoneBrokenTime(0),
			bFightProgressBoss(false),
			dwRaceHistogramStamp(0),
			dwMetinExpeditionUntil(0),
			dwNextMetinExpeditionRoll(0),
			lDesertCrossingTo(0),
			lDesertCrossingX(0),
			lDesertCrossingY(0),
			dwNextCrossingStoneCheck(0),
			dwStoneLastProgressTime(0),
			dwNextStoneProgressCheckTime(0),
			dwNextNavPlanTime(0),
			dwNextNavProgressTime(0),
			dwNextSectreeRescueTime(0),
			dwNavFailedTargetVID(0),
			dwNextGoalPlanTime(0),
			dwGoalStartedTime(0),
			dwActionChangedTime(0),
			dwLastMeaningfulActivityTime(0),
			dwLastCombatActionTime(0),
			iLastStoneHP(0),
			iMultiPullStartHPPercent(0),
			bLastStoneAttackerCount(0),
			bLastPersistedLevel(0),
			bRouteAllowsHorse(false),
			bRouteKeepsHorse(false),
			bRecoveringAfterDeath(false),
			bTacticalRetreat(false),
			bMultiPullActive(false),
			bMultiPullGroups(0),
			bMultiPullDesiredGroups(0),
			bLootThreatNearby(false),
			dwNextBagFullLogTime(0),
			bEquipPending(false),
			bMeleeForStone(false),
			bVisitingShop(false),
			bMarketTrip(false),
			bMarketToJoan(false),
			bTownNeedMisc(false),
			bTownNeedWeaponMerchant(false),
			bTownNeedArmorMerchant(false),
			bTownNeedBlacksmith(false),
			bTownNeedTrainer(false),
			bTownNeedSkillReset(false),
			bTownNeedSafebox(false),
			bVisitingBiologist(false),
			bVisitingHerbalist(false),
			bVisitingStable(false),
			bFishingSession(false),
			bIsFishing(false),
			bTownVisitPhase(BOT_TOWN_PHASE_NONE),
			bComboMotion(MOTION_COMBO_ATTACK_1),
			bStuckCounter(0),
			lLastX(0),
			lDefenceAnchorX(0),
			lDefenceAnchorY(0),
			lLastY(0),
			uRouteIndex(0),
			lRouteDestX(0),
			lRouteDestY(0),
			lRouteMapIndex(0),
			lParkedDestX(0),
			lParkedDestY(0),
			lParkedMapIndex(0),
			lIssuedWaypointX(0),
			lIssuedWaypointY(0),
			lNavProgressX(0),
			lNavProgressY(0),
			iNavLastWaypointDistance(-1),
			iLastMonkeyPortalIndex(-1),
			bNavNoProgressCount(0),
			bNavFailedTargetCount(0),
			bNavDeferredCount(0),
			bBotRole(BOT_ROLE_MOB_GRINDER),
			bPersonality(BOT_PERSONALITY_STEADY_ADVENTURER),
			bAmbition(BOT_AMBITION_LEVEL),
			uMetinHotspotIndex(0),
			bMonkeyChamber(255),
			bMonkeyPrevChamber(255),
			bMonkeySpot(0),
			bLongTermGoal(BOT_GOAL_LEVEL_UP),
			bCurrentAction(BOT_ACTION_IDLE),
			bLastStatusAction(255),
			bLastStatusGoal(255),
			bLastStatusTownPhase(255),
			bLastStatusParty(255),
			dwNextGuildCheckTime(0),
			dwLastKillCreditedVID(0),
			bFoundedGuild(false),
			dwNextGuildExpOfferTime(0),
			dwGuildExpAtLastOffer(0),
			bGuildLevelAtLastOffer(0),
			dwLastGuildPromotionTime(0),
			dwGuildWarEnemyGID(0),
			dwNextGuildWarMoveTime(0),
			dwTowerRaidGuild(0),
			bTowerSummoned(false),
			lTowerInstance(0),
			dwNextTowerMoveTime(0),
			dwNextTowerMasterCheckTime(0),
			bTowerTalkStep(0),
			iDragonCoins(0),
			iDragonMarks(0),
			bDragonBalanceKnown(false),
			bBoughtHairstyle(false),
			dwNextItemShopCheckTime(0),
			dwNextItemShopBuyTime(0),
			dwNextItemShopBalanceTime(0),
			dwNextMaterialScanTime(0),
			dwMaterialHuntVnum(0),
			lBiologistWalkMap(0),
			lBiologistWalkX(0),
			lBiologistWalkY(0),
			dwBiologistWalkUntil(0),
			dwShopSignClearUntil(0),
			dwNextShopSignClearTime(0),
			dwPortalWalkSince(0),
			iPortalWalkBest(0),
			wPortalWalkTicks(0),
			wPortalWalkRouteIndex(0),
			bLastNavOutcome(0),
			bRoutePartial(false),
			dwFightProgressVID(0),
			dwDefenceTargetVID(0),
			dwDefenceEpisodeStart(0),
			dwNextCombatRecheckTime(0),
			dwErrandDoneTime(0),
			dwFightStartTime(0),
			dwFightLastProgressTime(0),
			iLastFightHP(0),
			lCampX(0),
			lCampY(0),
			dwCampSince(0),
			dwRelocateSince(0),
			wHuntingHub(0xffff),
			dwHubChosenTime(0),
			dwTownLingerUntil(0),
			dwTownBrowseUntil(0),
			lTownBrowseX(0),
			lTownBrowseY(0),
			dwFirstNavDeferTime(0),
			dwMarketM2AllowedUntil(0),
			dwServiceRetryAt(0),
			dwServiceSince(0),
			dwDepartureSince(0),
			lDepartureMap(0),
			dwNextDepartureLogTime(0),
			bServicePending(false),
			bLastCombatReason(0),
			dwLureSessionId(0),
			dwLureStageTime(0),
			dwLureCourseTime(0),
			dwLureShotTime(0),
			dwLureNextTime(0),
			dwLureTargetVID(0),
			dwLureReceiverPID(0),
			dwLurePlayerPID(0),
			dwLurePlayerTime(0),
			lLureAnchorX(0),
			lLureAnchorY(0),
			iLureStartHPPercent(0),
			iLureDelivered(0),
			iLureChasing(0),
			bLureStage(LURE_STAGE_NONE),
			bLureGroupsPlanned(0),
			bLureGroupsTagged(0),
			bLureBudget(0),
			bLureTagAttempts(0),
			bLureGoodCourses(0)
		{
		}

		DWORD dwTargetVID;
		DWORD dwSpawnTime;
		DWORD dwLastBotSkillTime;
		DWORD dwLastKillerVID;
		DWORD dwLastDeathTime;
		long lDeathX;
		long lDeathY;
		BYTE bDeathCount;
		DWORD dwNextAttackTime;
		DWORD dwNextPotionTime;
		DWORD dwNextManaPotionTime;
		DWORD dwNextChestTime;
		DWORD dwNextBoosterTime;
		DWORD dwNextScrollRefineTime;
		DWORD dwNextPotionLogTime;
		DWORD dwDeathDetectedTime;
		DWORD dwNextReviveAttemptTime;
		DWORD dwNextGearAttemptTime;
		DWORD dwNextEquipmentCheckTime;
		DWORD dwNextStatCheckTime;
		DWORD dwNextSkillCheckTime;
		DWORD dwNextSkillBookTime;
		DWORD dwNextSoulStoneTime;
		DWORD dwNextThirdHandTime;
		DWORD dwNextSkillReallocateTime;
		DWORD dwNextProgressionChestCheckTime;
		DWORD dwNextBuffCheckTime;
		DWORD dwNextSkillCastTime;
		DWORD dwNextGearLogTime;
		DWORD dwNextPersistTime;
		DWORD dwNextRecoveryProtectionTime;
		DWORD dwNextRecoveryHealTime;
		DWORD dwRetreatStartedTime;
		DWORD dwNextRetreatMoveTime;
		DWORD dwRetreatThreatVID;
		DWORD dwNextRefineCheckTime;
		DWORD dwNextBonusCheckTime;
		// The piece the bonus pass is working on (its item id), kept until it
		// is done or no stone in the bag fits it (ManagePlayerBotBonusReroll).
		DWORD dwBonusFocusItem;
		DWORD dwNextChatTime;
		DWORD dwLastStatusChatTime;
		DWORD dwNextStatusProbeTime;
		DWORD dwLastStatusTargetVID;
		DWORD dwNextBiologistCheckTime;
		DWORD dwNextBiologistActionTime;
		// Baek-Go's board, the same shape as the Biologist's visit above: both
		// NPCs stand in every first village and neither is the other.
		DWORD dwNextHerbalistCheckTime;
		DWORD dwNextHerbalistActionTime;
		DWORD dwNextHorseCheckTime;
		DWORD dwNextHorseActionTime;
		DWORD dwNextHorseRideCheckTime;
		DWORD dwNextFishingCheckTime;
		DWORD dwNextFishingActionTime;
		// When the current line went into the water, so a cast that never reports
		// a bite can be given up on instead of parking the bot at the bank.
		DWORD dwFishingCastTime;
		// When this angler was last ready to fish and did not. Cleared by a cast.
		DWORD dwFishingIdleSince;
		DWORD dwFishingSessionEndTime;
		// A stuck angler used to be invisible: bFishingSession exempts it from the
		// inactivity watchdog, so nothing complained while it stood still for the
		// whole session. This throttles one progress line instead.
		DWORD dwNextFishingProgressLogTime;
		// The campfire is burning and there are fish to hand it.
		DWORD dwBakeUntil;
		DWORD dwNextWorldTravelTime;
		// Since when a live target has been holding world travel back.
		DWORD dwTravelBlockedSince;
		DWORD dwNextRemoteRefineReturnTime;
		DWORD dwDungeonEnteredTime;
		DWORD dwM3EnteredTime;
		// When the M3 door opens again after a visit that ran out without the
		// weapon (PLAYERBOT_M3_REVISIT_WAIT_MIN_MS).
		DWORD dwM3RevisitAfter;
		DWORD dwFrontierEnteredTime;
		DWORD dwShopOpenedTime;
		DWORD dwShopCloseTime;
		// Stands on this pitch since the last rest, and whether the previous
		// one sold anything - see PLAYERBOT_SHOP_STANDS_IN_ROW.
		BYTE bShopStandsInRow;
		bool bShopLastStandSold;
		// EPlayerBotShopReason of the stand that is up, and the weights file
		// generation it was judged under - see ManagePlayerBotShopLifetime.
		BYTE bShopOpenReason;
		DWORD dwShopWeightsGeneration;
		DWORD dwNextShopKeepTime;
		// The two channels with moves: a bot on the second channel waiting to
		// be moved to the shop channel to open a stand (EnsurePlayerBotPrivateShopChannel),
		// since when, when it asks again, and when a buyer may ask next.
		bool bWaitingForShopChannel = false;
		DWORD dwShopChannelWaitStarted = 0;
		DWORD dwNextShopChannelRequestTime = 0;
		DWORD dwNextBuyChannelRequestTime = 0;
		DWORD dwNextShoppingTime;
		DWORD dwProgressionTripNext = 0, dwProgressionTripUntil = 0;
		// The shopping trip: when it must be over, when the counters may be read
		// again, and which keeper the bot is currently walking up to. The stall is
		// held as a VID rather than a position so that a keeper which packs up
		// mid-walk simply stops being found.
		DWORD dwMarketTripUntil;
		DWORD dwMarketBrowseTime;
		DWORD dwMarketStallVID;
		// What this bot is currently selling, in the order the items sit on the
		// counter - an index here is the index CShopManager::Buy expects. Not in
		// the initialiser list: it default-constructs empty, which is the state a
		// bot with no stall is in.
		std::vector<TPlayerBotShopOffer> vecShopOffers;
		// Item id -> stands it came home from unsold. Each stand takes
		// PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT off the asking price, and a
		// piece of gear under the precious refine that nobody wanted for
		// PLAYERBOT_SHOP_UNSOLD_SCRAP_STANDS stands goes to the merchant.
		std::map<DWORD, BYTE> mapStallUnsold;
		// When each counter line was first put up (item id -> tick time), so
		// the age of a piece of stock is known when it is discounted, deposited
		// or scrapped. Pruned with mapStallUnsold.
		std::map<DWORD, DWORD> mapStockFirstListed;
		DWORD dwNextShopDebugTime;
		DWORD dwMonkeyReversePortalBlockUntil;
		// Since when this bot has been working its current Monkey Dungeon chamber.
		DWORD dwMonkeyChamberTime;
		DWORD dwNextLootPickupTime;
		DWORD dwNextLootSearchTime;
		DWORD dwNextLootThreatCheckTime;
		DWORD dwNextLootCleanupTime;
		DWORD dwNextInventoryMaintenanceTime;
		DWORD dwNextWanderTime;
		DWORD dwNextPartyCheckTime;
		DWORD dwNextPartyShareTime;
		DWORD dwPartyExpireTime;
		DWORD dwNextLureTime;
		DWORD dwNextMultiPullTime;
		DWORD dwMultiPullStartedTime;
		DWORD dwNextMultiPullActionTime;
		DWORD dwMultiPullTargetVID;
		DWORD dwNextShopCheckTime;
		// When this bot may next pay the old woman to forget its skills.
		DWORD dwNextSkillResetTime;
		DWORD dwNextStackMergeTime;
		DWORD dwEquipPendingSince;
		DWORD dwEmergencyScavengeUntil;
		DWORD dwTownWaitUntil;
		DWORD dwStoneFightStartTime;
		DWORD dwStoneProgressVID;
		DWORD dwStoneBrokenTime;
		// The monster the fight-progress clock tracks is a boss: its fall opens
		// the loot window a broken stone gets (dwStoneBrokenTime).
		bool bFightProgressBoss;
		// What this bot has fought lately, by race flag; see the world memory.
		WORD awRaceHistogram[PLAYERBOT_RACE_HISTOGRAM_SLOTS] = { 0 };
		DWORD dwRaceHistogramStamp;
		// The Metin expedition: until when this bot hunts stones like a hunter,
		// and when it next rolls for one. See PLAYERBOT_METIN_EXPEDITION_*.
		DWORD dwMetinExpeditionUntil;
		DWORD dwNextMetinExpeditionRoll;
		// The desert crossing: the map and point the bot is really going to,
		// while it walks the desert between two gates. Zero when not crossing.
		long lDesertCrossingTo;
		long lDesertCrossingX;
		long lDesertCrossingY;
		DWORD dwNextCrossingStoneCheck;
		// When this bot last read a book of each skill, for the BOOKS switch.
		DWORD dwStoneLastProgressTime;
		DWORD dwNextStoneProgressCheckTime;
		DWORD dwNextNavPlanTime;
		DWORD dwNextNavProgressTime;
		DWORD dwNextSectreeRescueTime;
		DWORD dwNavFailedTargetVID;
		DWORD dwNextGoalPlanTime;
		DWORD dwGoalStartedTime;
		DWORD dwActionChangedTime;
		DWORD dwLastMeaningfulActivityTime;
		DWORD dwLastCombatActionTime;
		int iLastStoneHP;
		int iMultiPullStartHPPercent;
		BYTE bLastStoneAttackerCount;
		BYTE bLastPersistedLevel;
		bool bRouteAllowsHorse;
		// The portal walk rides up to the gate; the passes that merely continue
		// its route must not climb down a kilometre short of it.
		bool bRouteKeepsHorse;
		bool bRecoveringAfterDeath;
		bool bTacticalRetreat;
		bool bMultiPullActive;
		BYTE bMultiPullGroups;
		BYTE bMultiPullDesiredGroups;
		bool bLootThreatNearby;
		// When the "bag full, nothing on the ground fits" line may be written
		// again for this bot: once a minute, not once per drop.
		DWORD dwNextBagFullLogTime;
		bool bEquipPending;
		// An Archer with a Metin stone for a target has its dagger or sword in
		// hand instead of the bow, and takes the bow back when the stone is
		// gone. See ManagePlayerBotEquipment.
		bool bMeleeForStone;
		bool bVisitingShop;
		// On a shopping trip: walking to the stalls, or standing among them.
		bool bMarketTrip;
		// The trip's first leg is the walk to the Joan gate from Bokjung. A
		// portal walk is continued by the tick's route passes, not by the pass
		// that asked for it, so the shopping pass has to keep asking until the
		// map changes (see ManagePlayerBotShopping).
		bool bMarketToJoan;
		bool bTownNeedMisc;
		bool bTownNeedWeaponMerchant;
		bool bTownNeedArmorMerchant;
		bool bTownNeedBlacksmith;
		bool bTownNeedTrainer;
		bool bTownNeedSkillReset;
		bool bTownNeedSafebox;
		bool bVisitingBiologist;
		bool bVisitingHerbalist;
		bool bVisitingStable;
		// The bot has committed to a fishing trip: it carries a rod in the weapon
		// slot and skips combat and gear swaps until the session ends.
		bool bFishingSession;
		// A line is currently in the water (the engine holds a fishing event).
		bool bIsFishing;
		BYTE bTownVisitPhase;
		BYTE bComboMotion;
		BYTE bStuckCounter;
		long lLastX;
		long lDefenceAnchorX;
		long lDefenceAnchorY;
		long lLastY;
		std::vector<PIXEL_POSITION> vecRoute;
		size_t uRouteIndex;
		long lRouteDestX;
		long lRouteDestY;
		long lRouteMapIndex;
		// The route a fight interrupted, kept so the walk resumes from the
		// nearest waypoint instead of being planned again from scratch. A far
		// plan costs two hundred milliseconds; a valley crossing meets a fight
		// every few seconds.
		std::vector<PIXEL_POSITION> vecParkedRoute;
		long lParkedDestX;
		long lParkedDestY;
		long lParkedMapIndex;
		long lIssuedWaypointX;
		long lIssuedWaypointY;
		long lNavProgressX;
		long lNavProgressY;
		int iNavLastWaypointDistance;
		int iLastMonkeyPortalIndex;
		BYTE bNavNoProgressCount;
		BYTE bNavFailedTargetCount;
		BYTE bNavDeferredCount;
		BYTE bBotRole;
		BYTE bPersonality;
		BYTE bAmbition;
		BYTE uMetinHotspotIndex;
		// The Monkey Dungeon chamber this bot is working, the one it came from
		// (so it walks on rather than back through the portal it arrived by), and
		// which of that chamber's spawn points it is walking to. 255 is "none":
		// a bot outside the dungeon has no chamber.
		BYTE bMonkeyChamber;
		BYTE bMonkeyPrevChamber;
		BYTE bMonkeySpot;
		BYTE bLongTermGoal;
		BYTE bCurrentAction;
		BYTE bLastStatusAction;
		BYTE bLastStatusGoal;
		BYTE bLastStatusTownPhase;
		BYTE bLastStatusParty;
		std::map<DWORD, DWORD> mapFailedLootVIDs;
		std::map<DWORD, DWORD> mapLootSeenSince;
		std::map<DWORD, DWORD> mapFailedStones;
		std::map<DWORD, DWORD> mapFailedTargets;
		std::map<DWORD, DWORD> mapBuffActiveUntil;
		std::vector<PIXEL_POSITION> vecMultiPullCenters;
		// Not in the initialiser list: it default-constructs empty, which is what
		// a bot that has not met anybody yet is.
		std::vector<TPlayerBotFriend> vecFriends;
		DWORD dwNextGuildCheckTime;
		// The last corpse this bot was credited for. The engine has no "you
		// killed it" hook, so a kill is read off a target that has gone from
		// alive to dead under the bot's own blow - and a bot standing over the
		// body must not be credited again on the next tick.
		DWORD dwLastKillCreditedVID;
		// CGuild's constructor adds the master through the database, so
		// GetGuild() is still NULL when the next upkeep pass comes round two
		// minutes later - and the bot would found a second guild under the
		// second name. This is the only thing that knows it already has one.
		bool bFoundedGuild;
		// The guild's share of the bot's experience (ManagePlayerBotGuildExp):
		// when the next offer is due, and the experience and level the last one
		// was measured against.
		DWORD dwNextGuildExpOfferTime;
		DWORD dwGuildExpAtLastOffer;
		BYTE bGuildLevelAtLastOffer;
		// When this bot last left a guild for a stronger one, so it does not
		// hop on every check.
		DWORD dwLastGuildPromotionTime;
		// The guild war: the enemy guild while the bot is at war (zero
		// otherwise), and the clock on its walks to and about the battlefield.
		DWORD dwGuildWarEnemyGID;
		DWORD dwNextGuildWarMoveTime;
		// The Demon Tower (playerbot_demon_tower.h): the raid this bot answered
		// (its guild's id, zero otherwise), whether its human master called it
		// to the ground floor, the instance it is in, the clock on its walks
		// and item uses there, when it next looks for its master, and the
		// step of a dialog (unused since the smith is passed without one).
		DWORD dwTowerRaidGuild;
		bool bTowerSummoned;
		long lTowerInstance;
		DWORD dwNextTowerMoveTime;
		DWORD dwNextTowerMasterCheckTime;
		BYTE bTowerTalkStep;
		// The ItemShop (playerbot_itemshop.h): the account's Dragon Coins and
		// Marks as last read or reckoned, whether they were ever read, the
		// hairstyle bought once, and the three clocks.
		int iDragonCoins;
		int iDragonMarks;
		bool bDragonBalanceKnown;
		bool bBoughtHairstyle;
		DWORD dwNextItemShopCheckTime;
		DWORD dwNextItemShopBuyTime;
		DWORD dwNextItemShopBalanceTime;
		// Where this bot has been standing, since when, and whether it is
		// currently being walked off it. See ManagePlayerBotRelocation.
		// The fight in progress: which monster, since when, the lowest health it
		// has been brought to, and when that last improved. See
		// ShouldPlayerBotAbandonFight.
		// The walk to a portal: when it stopped making progress, and the closest
		// it has been. See MovePlayerBotToWorldPortal.
		// How long to keep taking the stall sign back after a stall closes, and
		// when the next repeat is due. See ManagePlayerBotShopLifetime.
		// The material errand: when this bot may next scan its map, and what it
		// set off after. See StartPlayerBotMaterialHunt.
		DWORD dwNextMaterialScanTime;
		DWORD dwMaterialHuntVnum;
		// Where the collect row's scan last found the row's monsters, and until
		// when the frontier wander walks there first.
		long lBiologistWalkMap;
		long lBiologistWalkX;
		long lBiologistWalkY;
		DWORD dwBiologistWalkUntil;
		DWORD dwShopSignClearUntil;
		DWORD dwNextShopSignClearTime;
		DWORD dwPortalWalkSince;
		int iPortalWalkBest;
		// How many times the portal walk has been asked to make progress since
		// the clock started. The clock is wall time, so a stall says nothing
		// about whether the bot was ever given a tick to walk in.
		WORD wPortalWalkTicks;
		// The route position the portal walk last saw. Walking round a
		// building is progress even while the straight line to the portal
		// does not shorten, and only the route knows that.
		WORD wPortalWalkRouteIndex;
		// Why the last walk step came to nothing. Every way MovePlayerBot can
		// decline is throttled or silent, and three of them look identical from
		// outside: no route, no movement, nothing in any log. Recording which
		// one it was costs a byte and is the difference between a diagnosis and
		// a guess.
		BYTE bLastNavOutcome;
		// The route in hand ends short of its destination on purpose: the
		// corridor search hit its cap and handed back the nearest cell it
		// reached (PLAYERBOT_NAV_MAX_CORRIDOR_EXPANSIONS). Running out of
		// such a route is a replan from there, never an arrival.
		bool bRoutePartial;
		DWORD dwFightProgressVID;
		// The attacker this bot is currently defending itself against, since when,
		// and from where. See PLAYERBOT_DEFENCE_EPISODE_TIME.
		DWORD dwDefenceTargetVID;
		DWORD dwDefenceEpisodeStart;
		DWORD dwNextCombatRecheckTime;
		// When this bot last finished a town errand, so the time it then takes
		// to leave the map can be measured rather than guessed at.
		DWORD dwErrandDoneTime;
		DWORD dwFightStartTime;
		DWORD dwFightLastProgressTime;
		int iLastFightHP;
		long lCampX;
		long lCampY;
		DWORD dwCampSince;
		DWORD dwRelocateSince;
		// Index of the last hub the wander chose on a hub map, so the choice is
		// logged when it changes rather than on every decision.
		WORD wHuntingHub;
		DWORD dwHubChosenTime;

		// Until when this bot is spending time in town rather than leaving the
		// moment its errand is done.
		DWORD dwTownLingerUntil;
		// The next counter this bot strolls to, and when to choose another.
		DWORD dwTownBrowseUntil;
		long lTownBrowseX;
		long lTownBrowseY;
		// When this bot first had a route refused for want of planning budget.
		// The audit asked for the queue age: a deferral that has stood for a
		// minute is a different thing from one that has stood for a second.
		DWORD dwFirstNavDeferTime;
		// Until when this bot may look for goods in Bokjung. Zero means "look in
		// Joan first": a shopper crosses to the quieter market, and only after
		// finding nothing there is Bokjung worth the walk for a while.
		DWORD dwMarketM2AllowedUntil;
		// A town errand that has not finished. Set when the watchdog or a failed
		// visit gives up on the attempt, cleared when a visit completes or the
		// need goes away. While it stands the bot is a customer, not a hunter.
		DWORD dwServiceRetryAt;
		DWORD dwServiceSince;
		// Where this bot means to go once the town is done with it, and since
		// when. Survives the visit and the watchdog: the audit's point was that
		// a reset may drop a stale route but not the intent behind it.
		DWORD dwDepartureSince;
		long lDepartureMap;
		// When the next "departure overdue" line may be written for this bot.
		DWORD dwNextDepartureLogTime;
		bool bServicePending;
		// Why the monster this bot is fighting was allowed - the combat policy's
		// own Reason, kept so the line over the bot's head can say what it is
		// doing *for*, which is the whole point of the audit's status section.
		// Zero until the three-second re-check has run once.
		BYTE bLastCombatReason;

		// The luring course. The session id is what a log line is followed by
		// and what tells one course from the next; the party's own record of who
		// is luring for it lives in playerbot_lure.h, keyed by leader, because a
		// party may only have one lurer and a bot cannot see the other bots'
		// state from here.
		DWORD dwLureSessionId;
		DWORD dwLureStageTime;
		DWORD dwLureCourseTime;
		DWORD dwLureShotTime;
		DWORD dwLureNextTime;
		DWORD dwLureTargetVID;
		DWORD dwLureReceiverPID;
		// A person's standing order: the pid of the player who whispered
		// "luruj", and zero for the bots' own role. It outlives a course -
		// "luruj" is an order, not a request for one pull - and is cleared by
		// "przestan lurowac", by the party ending, by the player leaving the
		// map, and by the order's own deadline.
		DWORD dwLurePlayerPID;
		DWORD dwLurePlayerTime;
		// Where the party was standing when the course began. Everything is
		// measured from here: how far the Archer may go, and where it comes back
		// to - not the receiver's position, which moves during the fight.
		long lLureAnchorX;
		long lLureAnchorY;
		int iLureStartHPPercent;
		int iLureDelivered;
		int iLureChasing;
		BYTE bLureStage;
		BYTE bLureGroupsPlanned;
		BYTE bLureGroupsTagged;
		BYTE bLureBudget;
		BYTE bLureTagAttempts;
		BYTE bLureGoodCourses;
	};

	typedef std::map<DWORD, TPlayerBotAIState> TPlayerBotAIStateMap;

	// Every bot the manager has ever ticked, alive for the life of the process:
	// a bot that logs out keeps its plans, cooldowns and hobby. It lives here
	// rather than in the manager because the subsystems read it too - refining
	// asks a bot for its personality long before the tick reaches it.
	TPlayerBotAIStateMap s_mapPlayerBotAIStates;
	// How many bots stand on each map, rebuilt by the tick before it visits
	// them. The raid cap is the first thing to ask; a hub chooser cannot walk
	// the character manager for the answer on every decision.
	std::map<long, int> s_mapPlayerBotsOnMap;
	int GetPlayerBotsOnMap(long lMapIndex)
	{
		std::map<long, int>::const_iterator it = s_mapPlayerBotsOnMap.find(lMapIndex);
		return it == s_mapPlayerBotsOnMap.end() ? 0 : it->second;
	}

	int GetPlayerBotsAlive()
	{
		int total = 0;
		for (std::map<long, int>::const_iterator it = s_mapPlayerBotsOnMap.begin();
				it != s_mapPlayerBotsOnMap.end(); ++it)
			total += it->second;
		return total;
	}

	// How much of the population the level-30 weapon farm may hold at once.
	// "Everyone past thirty-five without the weapon" was the right rule for a
	// world whose bots are mostly fifty; on a world whose bots are mostly
	// thirty-six it was "sixty percent of the server in M3" with a map to
	// prove it. A share of the living population, never under the minimum,
	// gates the way in; a bot already there stays until the crowd is half
	// again over the share, so the door does not flap.
	const int PLAYERBOT_M3_CROWD_SHARE_PERCENT = 15;
	const int PLAYERBOT_M3_CROWD_MIN = 30;
	const int PLAYERBOT_M3_CROWD_STAY_PERCENT = 150;

	// Whether a hosted map spawns Metin stones at all. The three Monkey
	// Dungeons and the Spider Dungeon ship no stone.txt; every other hosted
	// map carries between six and thirty-seven stone spawns. A bot sent out
	// for stones must not be sent here - "Ide do Lochu Pajakow (cel: Metiny)"
	// was a real status line.
	bool PlayerBotMapHasMetinStones(long mapIndex)
	{
		// The two forests carry no stone.txt at all, so there is nothing there to
		// break. The Demon Tower does carry one (8015), and it is excluded on
		// purpose: the operator asked that bots not run that dungeon until it is
		// worked out properly, and a stone hunter sent inside is exactly how
		// they would start.
		return !IsPlayerBotSpiderMap(mapIndex) && !IsPlayerBotMonkeyMap(mapIndex) &&
				mapIndex != PLAYERBOT_MAP_FOREST && mapIndex != PLAYERBOT_MAP_RED_FOREST &&
				mapIndex != PLAYERBOT_MAP_DEMON_TOWER;
	}

	// Hunting stones right now: by role for life, or by expedition for half an
	// hour. Every rule that used to ask for the role asks this instead.
	bool IsPlayerBotMetinHunting(const TPlayerBotAIState& state, DWORD dwNow)
	{
		return state.bBotRole == BOT_ROLE_METIN_HUNTER ||
				(state.dwMetinExpeditionUntil != 0 && dwNow < state.dwMetinExpeditionUntil);
	}

	// The personality behind a pid, for the rules that get a character and not
	// a state - the travel gates, the stall's scoring. Steady adventurer when
	// the pid is not a bot's.
	BYTE GetPlayerBotPersonalityByPID(DWORD dwPID)
	{
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(dwPID);
		return it == s_mapPlayerBotAIStates.end()
				? (BYTE)BOT_PERSONALITY_STEADY_ADVENTURER : it->second.bPersonality;
	}

	// Changing goal or action is a state transition, so it lives with the
	// state. Every subsystem does it, and each one used to have to be
	// included after whichever file happened to hold these two.
	void SetPlayerBotGoal(LPCHARACTER ch, TPlayerBotAIState& state, BYTE goal, DWORD dwNow)
	{
		if (state.bLongTermGoal == goal)
			return;
		state.bLongTermGoal = goal;
		state.dwGoalStartedTime = dwNow;
		sys_log(0, "PLAYERBOT_GOAL: pid=%u name=%s goal=%u",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?", (unsigned int)goal);
	}

	// A bot the Demon Tower has (playerbot_demon_tower.h): inside an instance,
	// called to a raid, or summoned to its human master on the ground floor.
	// The passes that run above the tower's hook in the tick and can move a
	// bot to another map - the offline shop's service visit, the market trip,
	// the negative-rank rule - stand down for such a bot: the first run lost
	// three raiders to "offline_shop_service" inside two minutes.
	bool IsPlayerBotOnTowerBusiness(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		return (ch && IsPlayerBotDemonTowerInstance(ch->GetMapIndex())) ||
				state.dwTowerRaidGuild != 0 || state.bTowerSummoned;
	}

	void SetPlayerBotAction(TPlayerBotAIState& state, BYTE action, DWORD dwNow)
	{
		if (state.bCurrentAction == action)
			return;
		state.bCurrentAction = action;
		state.dwActionChangedTime = dwNow;
	}
}

#endif
