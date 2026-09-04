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
	const int PLAYERBOT_MELEE_RANGE = 250;
	const int PLAYERBOT_MELEE_SPLASH_RANGE = 300;
	const size_t PLAYERBOT_MAX_MELEE_TARGETS = 4;
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
	const DWORD PLAYERBOT_INVENTORY_MAINTENANCE_MIN = 30000;
	const DWORD PLAYERBOT_INVENTORY_MAINTENANCE_MAX = 60000;
	const int PLAYERBOT_POTION_HP_PERCENT = 65;
	const int PLAYERBOT_POTION_SP_PERCENT = 30;
	const int PLAYERBOT_RECOVERY_HP_PERCENT = 75;
	const int PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT = 20;
	const int PLAYERBOT_RECOVERY_REST_HEAL_PERCENT = 5;
	const int PLAYERBOT_RETREAT_START_HP_PERCENT = 35;
	const int PLAYERBOT_RETREAT_END_HP_PERCENT = 65;
	const DWORD PLAYERBOT_RETREAT_MOVE_INTERVAL = 1800;
	const DWORD PLAYERBOT_ATTACK_INTERVAL = 1200;
	const DWORD PLAYERBOT_POTION_INTERVAL = 1000;
	const DWORD PLAYERBOT_REVIVE_DELAY = 11000;
	const DWORD PLAYERBOT_GEAR_RETRY_INTERVAL = 1000;
	const DWORD PLAYERBOT_EQUIPMENT_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_EQUIPMENT_COMBAT_DELAY = 1700;
	const DWORD PLAYERBOT_GEAR_LOG_INTERVAL = 10000;
	const DWORD PLAYERBOT_WOODEN_ARROW_VNUM = 8000;
	const int PLAYERBOT_ARROW_RESTOCK_THRESHOLD = 100;
	const int PLAYERBOT_ARROW_SMALL_BUNDLE = 100;
	const int PLAYERBOT_ARROW_LARGE_BUNDLE = 200;
	const DWORD PLAYERBOT_POTION_LOG_INTERVAL = 10000;
	const DWORD PLAYERBOT_PERSIST_INTERVAL = 30000;
	const DWORD PLAYERBOT_RECOVERY_PROTECTION_INTERVAL = 3000;
	const DWORD PLAYERBOT_RECOVERY_REST_HEAL_INTERVAL = 1000;
	const DWORD PLAYERBOT_BUFF_FALLBACK_DURATION = 60000;
	const DWORD PLAYERBOT_SHAMAN_ATTACK_SKILL_INTERVAL = 6000;
	const DWORD PLAYERBOT_STAT_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_SKILL_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_SKILL_BOOK_CHECK_INTERVAL = 8000;
	const DWORD PLAYERBOT_SPIRIT_STONE_CHECK_INTERVAL = 10000;
	const DWORD PLAYERBOT_PARTY_SHARE_INTERVAL = 20000;
	const DWORD PLAYERBOT_GOAL_PLAN_INTERVAL = 5000;
	const DWORD PLAYERBOT_STATUS_SNAPSHOT_INTERVAL = 2000;
	// A Metin which repeatedly heals all dealt damage is not progress. Sample its
	// lowest observed HP at a deliberately cheap cadence, give a newcomer time to
	// change the outcome, and only then let the bot look for a productive target.
	const DWORD PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL = 4000;
	const DWORD PLAYERBOT_STONE_INITIAL_GRACE = 18000;
	const DWORD PLAYERBOT_STONE_SOLO_STALL_TIMEOUT = 26000;
	const DWORD PLAYERBOT_STONE_GROUP_STALL_TIMEOUT = 42000;
	const DWORD PLAYERBOT_STONE_FAILED_COOLDOWN = 90000;
	const int PLAYERBOT_STONE_SUPPORT_RANGE = 2200;
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
	const BYTE PLAYERBOT_PRECIOUS_REFINE = 6;
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
	// Refine materials go up at a small markup over the merchant price, so a bot
	// that needs one can buy it from a neighbour instead of farming for it.
	const DWORD PLAYERBOT_SHOP_MATERIAL_MARKUP = 3;
	// Browsing someone else's stall.
	const DWORD PLAYERBOT_SHOPPING_INTERVAL_MIN = 120000;
	const DWORD PLAYERBOT_SHOPPING_INTERVAL_MAX = 300000;
	// The engine refuses a purchase beyond 2000, so stay inside that.
	const int PLAYERBOT_SHOPPING_RANGE = 1800;
	// Gold a bot will not spend on the market; potions and gear come first.
	const DWORD PLAYERBOT_SHOPPING_GOLD_FLOOR = 200000;
	const int PLAYERBOT_GEAR_SHARE_RANGE = 2200;
	// Refining only runs while the bot is physically standing at the blacksmith.
	// A real player can click several times during one visit; a three-second cadence
	// permits several attempts without extending the absolute 6-24 s visit.
	const DWORD PLAYERBOT_REFINE_INTERVAL = 3000;
	// Bonus rerolling. Both verified against share/conf/item_proto.txt rather
	// than taken from the feature notes: 71084 is USE_CHANGE_ATTRIBUTE (rerolls
	// every line) and 71085 is USE_ADD_ATTRIBUTE (adds one). Neither can be
	// dropped, sold, traded or put in a stall, so a bot can only ever spend its
	// own gold on them.
	const DWORD PLAYERBOT_BONUS_CHANGE_VNUM = 71084;
	const DWORD PLAYERBOT_BONUS_ADD_VNUM = 71085;
	const DWORD PLAYERBOT_BONUS_STONE_PRICE = 25000;
	// Below this the gear itself is still changing every few levels, so paying to
	// polish its bonus lines is money the bot needs for the next weapon.
	const BYTE PLAYERBOT_BONUS_MIN_LEVEL = 30;
	// What the bot keeps: roughly one strong offensive line, or two decent ones.
	const int PLAYERBOT_BONUS_KEEP_SCORE = 240;
	const int PLAYERBOT_BONUS_STONES_PER_VISIT = 3;
	// Effectively once per town visit. A four-second cadence like the refiner's
	// would let one stop at the blacksmith burn a quarter of a million yang.
	const DWORD PLAYERBOT_BONUS_INTERVAL = 300000;
	// Gold the bot refuses to spend on bonuses; potions and gear come first.
	const DWORD PLAYERBOT_BONUS_GOLD_FLOOR = 120000;
	const DWORD PLAYERBOT_INACTIVITY_RESET_TIME = 90000;
	const DWORD PLAYERBOT_WANDER_INTERVAL = 8000;
	const DWORD PLAYERBOT_PARTY_CHECK_INTERVAL = 10000;
	const int PLAYERBOT_PARTY_DESIRED_MAX = 6;
	const int PLAYERBOT_PARTY_COHESION_RADIUS = 2800;
	const int PLAYERBOT_ARCHER_LURE_MIN_PARTY_MEMBERS = 5;
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
	// Verified against locale/english/map/{index,Setting.txt,npc.txt,Town.txt} and
	// share/conf/mob_names.txt. Chunjo uses the empire-specific easy monkey
	// dungeon (map 25); map 107 is a different global dungeon whose coordinates
	// do not match the Bokjung portal target.
	const long PLAYERBOT_MAP_CHUNJO_M1 = 21;
	const long PLAYERBOT_MAP_CHUNJO_M2 = 23;
	const long PLAYERBOT_MAP_CHUNJO_M3 = 24;
	const long PLAYERBOT_MAP_MONKEY_EASY = 25;
	const long PLAYERBOT_M1_TO_M2_PORTAL_X = 87600;
	const long PLAYERBOT_M1_TO_M2_PORTAL_Y = 215100;
	const long PLAYERBOT_M2_ARRIVAL_X = 111800;
	const long PLAYERBOT_M2_ARRIVAL_Y = 216100;
	const long PLAYERBOT_M2_TO_M1_PORTAL_X = 113000;
	const long PLAYERBOT_M2_TO_M1_PORTAL_Y = 213600;
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
	const long PLAYERBOT_M1_TELEPORTER_X = 51900;
	const long PLAYERBOT_M1_TELEPORTER_Y = 153600;
	const long PLAYERBOT_MAP_DESERT = 63;
	const long PLAYERBOT_MAP_ORC_VALLEY = 64;
	// Not the map's own empire spawn point, which is where this used to be.
	// map_n_threeway is a three-empire border map and each corner is walled off:
	// from the Chunjo spawn a bot can reach 17 of the map's 532 spawn groups, and
	// none of the twelve hunting hubs. Thirteen bots sat there at exactly the
	// entry level, never advancing, while planning routes that could not exist -
	// 7812 of 8259 "unreachable" lines in one session came from that corner.
	//
	// These two are hub coordinates from regen.txt, so they are standable, and
	// they are inside the map's largest connected region: 161 spawn groups, the
	// whole main hunting ground. Verified with tools/analyse_map_reach.py.
	const long PLAYERBOT_ORC_VALLEY_ARRIVAL_X = 327200;
	const long PLAYERBOT_ORC_VALLEY_ARRIVAL_Y = 742300;
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
	const long PLAYERBOT_ORC_VALLEY_EXIT_X = 319200;
	const long PLAYERBOT_ORC_VALLEY_EXIT_Y = 734700;
	const long PLAYERBOT_DESERT_EXIT_X = 219700;
	const long PLAYERBOT_DESERT_EXIT_Y = 499900;
	// Ordinary spawns are levels 18-25 in Orc Valley and 26-30 in the Desert, but
	// the Metins tell a different story: 40/45/50 in the Desert and 45/48/50 in
	// Orc Valley. A stone is only worth breaking between stoneLevel-9 and
	// stoneLevel+10, so Orc Valley starts paying at level 36 and not before -
	// which is why it belongs to the high band even though its monsters do not.
	// Bokjung keeps everyone up to 29.
	const BYTE PLAYERBOT_DESERT_MIN_LEVEL = 30;
	const BYTE PLAYERBOT_DESERT_MAX_LEVEL = 36;
	const BYTE PLAYERBOT_ORC_VALLEY_MIN_LEVEL = 36;
	const BYTE PLAYERBOT_ORC_VALLEY_MAX_LEVEL = 55;
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
	// How far from the middle a stall may stand. A hundred units is a metre here,
	// so this is a market four to seventeen metres across instead of the
	// two-and-a-half-metre huddle it was.
	//
	// It cannot be wider. shop_manager.cpp refuses a purchase beyond 2000 units,
	// so a buyer reaches twenty metres and no further, and PLAYERBOT_SHOPPING_RANGE
	// is 1800 for the same reason. Spread over forty metres the market looked
	// roomy and stopped working: stalls at opposite ends were out of each other's
	// reach and nobody bought anything at all. The ceiling belongs to the engine,
	// not to us.
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
	// The chance, rolled again for every stall a bot puts up, that it chooses Joan
	// over Bokjung. Joan is where the players are - three quarters of the live
	// bots stand on map 21 at any moment - so that is where the stalls belong.
	const DWORD PLAYERBOT_SHOP_M1_SHARE = 90;
	// A stall stands for a while and then the bot goes back to playing. An hour
	// was long enough that a player watching the market never saw one come down,
	// which read as "the shops never close" even before the tick-ordering bug
	// that genuinely kept some of them open.
	const DWORD PLAYERBOT_SHOP_MIN_DURATION = 600000;    // 10 min
	const DWORD PLAYERBOT_SHOP_MAX_DURATION = 1500000;   // 25 min
	// What a bot pays itself for the stall it sets up.
	const DWORD PLAYERBOT_SHOP_BUNDLE_PRICE = 2000;
	const DWORD PLAYERBOT_SHOP_REST_MIN = 1800000;
	const DWORD PLAYERBOT_SHOP_REST_MAX = 5400000;
	const DWORD PLAYERBOT_HORSE_MEDAL_VNUM = 50050;
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
	const DWORD PLAYERBOT_FISHING_BAIT_VNUM = 27801;  // Robak
	const DWORD PLAYERBOT_SHELLFISH_VNUM = 27987;     // Malz
	// What a shell can hold: Biala / Niebieska / Krwawa Perla.
	const DWORD PLAYERBOT_PEARL_FIRST_VNUM = 27992;
	const DWORD PLAYERBOT_PEARL_LAST_VNUM = 27994;
	const int PLAYERBOT_FISHING_BAIT_BUNDLE = 20;
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
	const long PLAYERBOT_FISHING_BANK_X = 67250;
	const long PLAYERBOT_FISHING_BANK_Y = 156900;
	// A point well inside the river, used only to turn the bot to face the water.
	const long PLAYERBOT_FISHING_WATER_X = 68000;
	const int PLAYERBOT_FISHING_ARRIVE = 200;
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
	const DWORD PLAYERBOT_FISHING_SESSION_MIN = 900000;    // 15 min
	const DWORD PLAYERBOT_FISHING_SESSION_MAX = 2400000;   // 40 min
	const DWORD PLAYERBOT_FISHING_REST_MIN = 2700000;      // 45 min
	const DWORD PLAYERBOT_FISHING_REST_MAX = 7200000;      // 2 h
	const int PLAYERBOT_HORSE_MOUNT_DISTANCE = 1800;
	const int PLAYERBOT_HORSE_DISMOUNT_DISTANCE = 1000;
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
	// Who the Easy Monkey Dungeon is worth a trip for. Below this a bot dies to
	// the rooms it has to cross; above it the medals are pocket change against
	// what the same half hour buys on the frontier, and the dungeon was filling
	// up with characters that had nothing left to gain there.
	const BYTE PLAYERBOT_MONKEY_MIN_LEVEL = 18;
	const BYTE PLAYERBOT_MONKEY_MAX_LEVEL = 26;
	const DWORD PLAYERBOT_M3_MAX_VISIT_TIME = 1200000;
	const DWORD PLAYERBOT_MONKEY_REVERSE_PORTAL_BLOCK_TIME = 10000;
	const long PLAYERBOT_MONKEY_EASY_BASE_X = 844800;
	const long PLAYERBOT_MONKEY_EASY_BASE_Y = 435200;
	const long MAP21_BASE_X = 921600;
	const long MAP21_BASE_Y = 204800;

	// One line on a stall's counter. CShop keeps its own item list private, and a
	// bot browsing the market reads this instead - we are the ones who put the
	// items there, so it is exactly what is on sale.
	struct TPlayerBotShopOffer
	{
		DWORD dwVnum;
		DWORD dwPrice;
		BYTE bRefine;
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
	};

	const TPlayerBotBiologistMission PLAYERBOT_BIOLOGIST_MISSIONS[] = {
		{ 4,  "make_herb_lv4",  50701, 173, 5,  90, 1000,  500,    "Kwiat Brzoskwini" },
		{ 7,  "make_herb_lv7",  50702, 175, 5,  90, 3000,  2000,   "Pokrzywa" },
		{ 10, "make_herb_lv10", 50703, 177, 5,  90, 5000,  6500,   "Kwiat Kaki" },
		{ 15, "make_herb_lv15", 50704, 181, 5,  90, 10000, 25000,  "Korzen Gango" },
		{ 20, "make_herb_lv20", 50705, 182, 10, 80, 15000, 95000,  "Bez" },
		{ 25, "make_herb_lv25", 50706, 183, 10, 70, 20000, 200000, "Grzyb Tue" }
	};
	const size_t PLAYERBOT_BIOLOGIST_MISSION_COUNT =
			sizeof(PLAYERBOT_BIOLOGIST_MISSIONS) / sizeof(PLAYERBOT_BIOLOGIST_MISSIONS[0]);

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
	const BYTE PLAYERBOT_HUNTING_MAX_LEVEL = 25;
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
		{ 451, 60, 402, 80, 0 }, { 551, 80, 454, 20, 0 }
	};

	struct TPlayerBotMapPoint { long x; long y; };
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
		BOT_TOWN_PHASE_GATE_CROSS_OUT
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
		BOT_ACTION_FISHING
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
		BOT_PERSONALITY_WANDERER
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

	struct TPlayerBotAIState
	{
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
			dwNextPotionLogTime(0),
			dwDeathDetectedTime(0),
			dwNextReviveAttemptTime(0),
			dwNextGearAttemptTime(0),
			dwNextEquipmentCheckTime(0),
			dwNextStatCheckTime(0),
			dwNextSkillCheckTime(0),
			dwNextSkillBookTime(0),
			dwNextSpiritStoneTime(0),
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
			dwNextChatTime(0),
			dwLastStatusChatTime(0),
			dwNextStatusProbeTime(0),
			dwLastStatusTargetVID(0),
			dwNextBiologistCheckTime(0),
			dwNextBiologistActionTime(0),
			dwNextHorseCheckTime(0),
			dwNextHorseActionTime(0),
			dwNextHorseRideCheckTime(0),
			dwNextFishingCheckTime(0),
			dwNextFishingActionTime(0),
			dwFishingCastTime(0),
			dwFishingSessionEndTime(0),
			dwNextFishingProgressLogTime(0),
			dwNextWorldTravelTime(0),
			dwTravelBlockedSince(0),
			dwNextRemoteRefineReturnTime(0),
			dwDungeonEnteredTime(0),
			dwM3EnteredTime(0),
			dwFrontierEnteredTime(0),
			dwShopOpenedTime(0),
			dwShopCloseTime(0),
			dwNextShopKeepTime(0),
			dwNextShoppingTime(0),
			dwNextShopDebugTime(0),
			dwMonkeyReversePortalBlockUntil(0),
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
			dwEmergencyScavengeUntil(0),
			dwTownWaitUntil(0),
			dwStoneFightStartTime(0),
			dwStoneProgressVID(0),
			dwStoneLastProgressTime(0),
			dwNextStoneProgressCheckTime(0),
			dwNextNavPlanTime(0),
			dwNextNavProgressTime(0),
			dwNextNavErrorLogTime(0),
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
			bRecoveringAfterDeath(false),
			bTacticalRetreat(false),
			bMultiPullActive(false),
			bMultiPullGroups(0),
			bMultiPullDesiredGroups(0),
			bLootThreatNearby(false),
			bEquipPending(false),
			bVisitingShop(false),
			bTownNeedMisc(false),
			bTownNeedWeaponMerchant(false),
			bTownNeedArmorMerchant(false),
			bTownNeedBlacksmith(false),
			bTownNeedTrainer(false),
			bVisitingBiologist(false),
			bVisitingStable(false),
			bFishingSession(false),
			bIsFishing(false),
			bTownVisitPhase(BOT_TOWN_PHASE_NONE),
			bComboMotion(MOTION_COMBO_ATTACK_1),
			bStuckCounter(0),
			lLastX(0),
			lLastY(0),
			uRouteIndex(0),
			lRouteDestX(0),
			lRouteDestY(0),
			lRouteMapIndex(0),
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
			bLongTermGoal(BOT_GOAL_LEVEL_UP),
			bCurrentAction(BOT_ACTION_IDLE),
			bLastStatusAction(255),
			bLastStatusGoal(255),
			bLastStatusTownPhase(255),
			bLastStatusParty(255)
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
		DWORD dwNextPotionLogTime;
		DWORD dwDeathDetectedTime;
		DWORD dwNextReviveAttemptTime;
		DWORD dwNextGearAttemptTime;
		DWORD dwNextEquipmentCheckTime;
		DWORD dwNextStatCheckTime;
		DWORD dwNextSkillCheckTime;
		DWORD dwNextSkillBookTime;
		DWORD dwNextSpiritStoneTime;
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
		DWORD dwNextChatTime;
		DWORD dwLastStatusChatTime;
		DWORD dwNextStatusProbeTime;
		DWORD dwLastStatusTargetVID;
		DWORD dwNextBiologistCheckTime;
		DWORD dwNextBiologistActionTime;
		DWORD dwNextHorseCheckTime;
		DWORD dwNextHorseActionTime;
		DWORD dwNextHorseRideCheckTime;
		DWORD dwNextFishingCheckTime;
		DWORD dwNextFishingActionTime;
		// When the current line went into the water, so a cast that never reports
		// a bite can be given up on instead of parking the bot at the bank.
		DWORD dwFishingCastTime;
		DWORD dwFishingSessionEndTime;
		// A stuck angler used to be invisible: bFishingSession exempts it from the
		// inactivity watchdog, so nothing complained while it stood still for the
		// whole session. This throttles one progress line instead.
		DWORD dwNextFishingProgressLogTime;
		DWORD dwNextWorldTravelTime;
		// Since when a live target has been holding world travel back.
		DWORD dwTravelBlockedSince;
		DWORD dwNextRemoteRefineReturnTime;
		DWORD dwDungeonEnteredTime;
		DWORD dwM3EnteredTime;
		DWORD dwFrontierEnteredTime;
		DWORD dwShopOpenedTime;
		DWORD dwShopCloseTime;
		DWORD dwNextShopKeepTime;
		DWORD dwNextShoppingTime;
		// What this bot is currently selling, in the order the items sit on the
		// counter - an index here is the index CShopManager::Buy expects. Not in
		// the initialiser list: it default-constructs empty, which is the state a
		// bot with no stall is in.
		std::vector<TPlayerBotShopOffer> vecShopOffers;
		DWORD dwNextShopDebugTime;
		DWORD dwMonkeyReversePortalBlockUntil;
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
		DWORD dwEmergencyScavengeUntil;
		DWORD dwTownWaitUntil;
		DWORD dwStoneFightStartTime;
		DWORD dwStoneProgressVID;
		DWORD dwStoneLastProgressTime;
		DWORD dwNextStoneProgressCheckTime;
		DWORD dwNextNavPlanTime;
		DWORD dwNextNavProgressTime;
		DWORD dwNextNavErrorLogTime;
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
		bool bRecoveringAfterDeath;
		bool bTacticalRetreat;
		bool bMultiPullActive;
		BYTE bMultiPullGroups;
		BYTE bMultiPullDesiredGroups;
		bool bLootThreatNearby;
		bool bEquipPending;
		bool bVisitingShop;
		bool bTownNeedMisc;
		bool bTownNeedWeaponMerchant;
		bool bTownNeedArmorMerchant;
		bool bTownNeedBlacksmith;
		bool bTownNeedTrainer;
		bool bVisitingBiologist;
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
		long lLastY;
		std::vector<PIXEL_POSITION> vecRoute;
		size_t uRouteIndex;
		long lRouteDestX;
		long lRouteDestY;
		long lRouteMapIndex;
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
	};

	typedef std::map<DWORD, TPlayerBotAIState> TPlayerBotAIStateMap;

	// Every bot the manager has ever ticked, alive for the life of the process:
	// a bot that logs out keeps its plans, cooldowns and hobby. It lives here
	// rather than in the manager because the subsystems read it too - refining
	// asks a bot for its personality long before the tick reaches it.
	TPlayerBotAIStateMap s_mapPlayerBotAIStates;

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

	void SetPlayerBotAction(TPlayerBotAIState& state, BYTE action, DWORD dwNow)
	{
		if (state.bCurrentAction == action)
			return;
		state.bCurrentAction = action;
		state.dwActionChangedTime = dwNow;
	}
}

#endif
