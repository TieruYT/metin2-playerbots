// The player's own companion - "Towarzysz" (Tieru, 24 September): a bot that
// belongs to one player for good, the way a follower does in Diablo. The
// player picks its class, its sex, its path and its name in the Towarzysz
// quest; from then on it is in the player's party with the experience split
// evenly, walks at the player's side, fights what the player fights and what
// attacks the player, buffs the player, picks the player's drops up into the
// player's own bag, accepts every trade the player offers, and refines and
// shops when the player stands at the blacksmith or a merchant. It logs in and
// out with the player. "Wolna reka" lets it play like any other bot while the
// player is online; "Przywolaj" puts it back at the player's side. It never
// takes a duel, and nobody's duel is its fight.
//
// A companion is an ordinary registered bot identity - a seeded
// playerbot_NNN account, with every guard LoadRegisteredBots keeps - picked
// from the player's kingdom by the race asked for, renamed, raised to the
// player's level at its first spawn, and written into
// player.playerbot_sidekick. Nothing of the population's machinery may start,
// move or log out such an identity (CPlayerBotManager::Spawn refuses it
// unless SpawnSidekick asks, the top-up, the late joiners, the life schedule
// and the channel coordinator step over it): whether it is in the world is its
// owner's presence on this core alone.
//
// A player who fights a player is defended by the Anti-PK protocol, which
// already answers for a person in a party with bots in it
// (playerbot_anti_pk.h, BOT_FOE_PARTY) and runs above this pass in the tick.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_demon_tower.h (the fight is that file's)
// and before playerbot_companions.h, whose IsPlayerBotHeldForCompany asks it.

namespace
{
	enum EPlayerBotSidekickMode
	{
		PLAYERBOT_SIDEKICK_FOLLOW = 0,
		PLAYERBOT_SIDEKICK_FREE = 1,
	};

	// How it fights at the owner's side: the letter's "Jak ma walczyc?" and the
	// whispers "atakuj", "nie atakuj pierwszy" and "nie walcz" (Tieru, 25
	// September: "typu nie atakuj pierwszy").
	enum EPlayerBotSidekickStance
	{
		// Everything near the owner: the owner's target, what hits either of
		// them, and the nearest monster within PLAYERBOT_SIDEKICK_HUNT_RANGE.
		PLAYERBOT_SIDEKICK_STANCE_ATTACK = 0,
		// It starts nothing: what hits the owner or itself, and the owner's own
		// target once that is a fight already - a monster at one of the owner's
		// party, a stone somebody has begun, a guild war's foe.
		PLAYERBOT_SIDEKICK_STANCE_DEFEND = 1,
		// It hits back at what hits it and at nothing else, and takes no
		// monster off a losing owner.
		PLAYERBOT_SIDEKICK_STANCE_PASSIVE = 2,
	};

	// Why a foe was taken up, in the order FindPlayerBotSidekickFoe asks.
	enum EPlayerBotSidekickFoeWhy
	{
		PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET = 0,
		PLAYERBOT_SIDEKICK_FOE_AT_OWNER = 1,
		PLAYERBOT_SIDEKICK_FOE_AT_SELF = 2,
		PLAYERBOT_SIDEKICK_FOE_NEARBY = 3,
	};

	// The table is read again this often, so a companion made on another core
	// - the owner on the second channel, or on a map another core hosts - is
	// known here by the time the owner arrives.
	const DWORD PLAYERBOT_SIDEKICK_RELOAD_MS = 30000;
	// A player who is not in this core's world for this long has logged out or
	// gone to another core, and the companion leaves with it. A warp between
	// two maps of one core is a logout and a login seconds apart, which is why
	// it is not zero.
	const DWORD PLAYERBOT_SIDEKICK_OWNER_GONE_MS = 20000;
	const DWORD PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS = 10000;
	// At the owner's side: it walks up past the first distance, is put beside
	// the owner past the second (a horse outran it, or a wall is in the way),
	// and hunts on its own only within the third of the owner.
	const int PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE = 450;
	const int PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE = 3000;
	const int PLAYERBOT_SIDEKICK_HUNT_RANGE = 1200;
	// What attacks the owner is looked for this far round the owner, and the
	// owner's own target is fought this far from the owner.
	const int PLAYERBOT_SIDEKICK_GUARD_RANGE = 1800;
	const int PLAYERBOT_SIDEKICK_ASSIST_RANGE = 2500;
	// The drops it goes for, and the step at which it takes one. The server
	// hands a pick-up over from 600 (@fixme173), so the step stays inside it.
	const int PLAYERBOT_SIDEKICK_LOOT_RANGE = 1200;
	const int PLAYERBOT_SIDEKICK_PICKUP_RANGE = 250;
	const DWORD PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS = 400;
	// A drop it could not take (the owner's bag full, somebody else's by then)
	// is left alone this long, or the companion stands over it for good.
	const DWORD PLAYERBOT_SIDEKICK_LOOT_FAILED_MS = 30000;
	const DWORD PLAYERBOT_SIDEKICK_LOOT_GIVE_UP_MS = 8000;
	// The blacksmith and the three merchants, when the owner stands this near
	// one of them, and how often the companion does its business there.
	const int PLAYERBOT_SIDEKICK_NPC_RANGE = 900;
	const DWORD PLAYERBOT_SIDEKICK_SERVICE_INTERVAL_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_PARTY_CHECK_MS = 3000;
	const DWORD PLAYERBOT_SIDEKICK_CATCH_UP_MS = 3000;
	// The owner losing a fight: under this share of its health, what is hitting
	// the owner is turned onto the companion - the lure's handover of a monster
	// (playerbot_lure.h), pointed at itself - a few at a time, while the
	// companion's own health holds.
	const int PLAYERBOT_SIDEKICK_PROTECT_HP_PERCENT = 40;
	const int PLAYERBOT_SIDEKICK_PROTECT_SELF_HP_PERCENT = 30;
	const int PLAYERBOT_SIDEKICK_PROTECT_MAX_MONSTERS = 3;
	const DWORD PLAYERBOT_SIDEKICK_PROTECT_INTERVAL_MS = 1500;
	// A name: letters and digits, as a player's is on the character screen.
	const size_t PLAYERBOT_SIDEKICK_NAME_MIN = 3;
	const size_t PLAYERBOT_SIDEKICK_NAME_MAX = 16;
	// An identity saved this recently may still sit in the db core's player
	// cache (g_iPlayerCacheFlushSeconds, seven minutes), and a load from the
	// cache carries the name it had - the rename would not show until the cache
	// let it go, and a SetName after the load would leave the engine's name
	// maps (whispers, P2P) on the old one. A bot in the world saves every few
	// minutes, so this also keeps the pick away from the population's live
	// bots; one that has never played (playtime 0) was never cached at all.
	const int PLAYERBOT_SIDEKICK_IDENTITY_IDLE_MINUTES = 15;
	// A self-test switch, a file in the core's working directory: with it a bot
	// may own a companion, and the core makes one for a bot it picks, so the
	// whole machinery runs on a world with no person in it. Nothing creates the
	// file; off unless an operator does.
	const char* const PLAYERBOT_SIDEKICK_SELFTEST_FILE = "playerbot_sidekick_selftest";
	// How long the self-test waits after a start for the owner of a pair an
	// earlier run made - a bot, which may come in late in the spawn window -
	// before it gives up and makes a new pair. Every new pair renames one more
	// of the population's bots.
	const DWORD PLAYERBOT_SIDEKICK_SELFTEST_OWNER_WAIT_MS = 10 * 60 * 1000;
	// The companion's window in the client (uisidekick.py, the P key; Tieru,
	// 25 September: "GUI do sterowania towarzyszem ... rozne akcje, ktore
	// mozna mu zlecac"): the version of what it is sent.
	const int PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL = 1;
	// "Czekaj tutaj": how far from its spot a waiting companion follows what
	// it fights before it walks back.
	const int PLAYERBOT_SIDEKICK_HOLD_LEASH = 300;
	// "Idz na zakupy": a town visit in its own first village and back to the
	// owner. A visit not begun this long after the arrival is a town with
	// nothing to do, and one still going after the second is called off.
	const DWORD PLAYERBOT_SIDEKICK_ERRAND_START_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_ERRAND_MAX_MS = 8 * 60 * 1000;
	// The window of its bag and skills (uisidekickinventory.py): the version of
	// what it is sent, where a worn position starts (1000 + WEAR_*), the cells
	// of one bag page, the mark of a piece its owner took off, how long the
	// fight holds off for a piece its owner put on, and how many lines one
	// answer may carry - more than a full bag and every slot, so a full picture
	// is one answer and a runaway is still bounded.
	const int PLAYERBOT_SIDEKICK_EQ_PROTOCOL = 1;
	const int PLAYERBOT_SIDEKICK_EQ_WEAR_BASE = 1000;
	const int PLAYERBOT_SIDEKICK_EQ_PAGE_CELLS = 45;
	const BYTE PLAYERBOT_SIDEKICK_PIN_UNWANTED = 255;
	const DWORD PLAYERBOT_SIDEKICK_EQUIP_WAIT_MS = 4000;
	const size_t PLAYERBOT_SIDEKICK_EQ_MAX_LINES = 260;

	// What it picks up at the owner's side.
	enum EPlayerBotSidekickLoot
	{
		PLAYERBOT_SIDEKICK_LOOT_NONE = 0,
		// Only what the owner may take, which the party hands to the owner.
		PLAYERBOT_SIDEKICK_LOOT_OWNERS = 1,
		PLAYERBOT_SIDEKICK_LOOT_ALL = 2,
	};

	struct TPlayerBotSidekick
	{
		DWORD dwOwnerPID;
		DWORD dwSidekickPID;
		BYTE bMode;
		BYTE bStance;
		BYTE bLoot;
		bool bProtect;	// takes what hits a losing owner
		bool bBuffs;	// a Shaman's buffs on the owner
		bool bManualSkills;	// the owner spends its skill points, the AI none
		BYTE bGroup;
		BYTE bLevel;
		bool bSetupDone;
		// This core's clocks.
		DWORD dwOwnerSeenAt;
		DWORD dwNextSpawnTry;
		TPlayerBotSidekick()
			: dwOwnerPID(0), dwSidekickPID(0), bMode(PLAYERBOT_SIDEKICK_FOLLOW),
			  bStance(PLAYERBOT_SIDEKICK_STANCE_ATTACK), bLoot(PLAYERBOT_SIDEKICK_LOOT_ALL), bProtect(true),
			  bBuffs(true), bManualSkills(false), bGroup(0), bLevel(1), bSetupDone(true), dwOwnerSeenAt(0),
			  dwNextSpawnTry(0)
		{
		}
	};
	typedef std::map<DWORD, TPlayerBotSidekick> TPlayerBotSidekickMap;
	TPlayerBotSidekickMap s_mapPlayerBotSidekicks;			// by owner pid
	std::map<DWORD, DWORD> s_mapPlayerBotSidekickOwner;	// companion pid -> owner pid
	DWORD s_dwPlayerBotSidekickNextLoad = 0;
	DWORD s_dwPlayerBotSidekickNextManage = 0;
	bool s_bPlayerBotSidekickTable = false;
	bool s_bPlayerBotSidekickSettingsColumns = false;
	bool s_bPlayerBotSidekickSelfTest = false;
	DWORD s_dwPlayerBotSidekickNextSelfTestCheck = 0;
	DWORD s_dwPlayerBotSidekickSelfTestOwner = 0;
	DWORD s_dwPlayerBotSidekickSelfTestAt = 0;
	int s_iPlayerBotSidekickSelfTestStep = 0;
	DWORD s_dwPlayerBotSidekickSelfTestSince = 0;
	// Whether this run's town visit has begun: the order is asked again while
	// something refuses it (the companion lying down, a map it cannot leave).
	bool s_bPlayerBotSidekickSelfTestErrand = false;
	// Every town visit an owner's order began, so the self-test can tell a
	// begun one from a refused one.
	unsigned int s_uPlayerBotSidekickErrands = 0;
	// The self-test of the window alone (the file's first line "eq"), and the
	// piece it hands over and takes back.
	bool s_bPlayerBotSidekickSelfTestEq = false;
	DWORD s_dwPlayerBotSidekickSelfTestGiftID = 0;
	// The piece the window's self-test took off onto a chosen cell, and the
	// slot a gift was dropped straight onto.
	DWORD s_dwPlayerBotSidekickSelfTestPieceID = 0;
	int s_iPlayerBotSidekickSelfTestGiftWear = -1;
	const int PLAYERBOT_SIDEKICK_EQ_SELFTEST_STEPS = 15;
	bool s_bPlayerBotSidekickPinTable = false;

	// What a companion carries between ticks that nobody else needs.
	struct TPlayerBotSidekickRuntime
	{
		DWORD dwNextPartyCheck;
		DWORD dwNextService;
		DWORD dwNextLoot;
		DWORD dwNextCatchUp;
		DWORD dwLootVID;
		DWORD dwLootSince;
		DWORD dwNextProtect;
		bool bTrading;
		std::set<DWORD> setBagBeforeTrade;
		// What the owner handed it: kept, never sold (IsPlayerBotSidekickGift).
		std::set<DWORD> setGifts;
		std::map<DWORD, DWORD> mapLootFailed;	// item vid -> until
		// The foes it took up, by why (EPlayerBotSidekickFoeWhy): the self-test
		// reads them to see a stance hold.
		DWORD dwLastFoeVID;
		DWORD adwFoes[4];
		// "Czekaj tutaj": the spot it keeps until it is called. Not kept over a
		// logout - it comes back at its owner's side.
		bool bHold;
		long lHoldMap;
		long lHoldX;
		long lHoldY;
		// "Idz na zakupy": the town visit it was sent on, whether one began,
		// and the purse it left with.
		bool bErrand;
		bool bErrandVisit;
		DWORD dwErrandSince;
		long long llErrandGold;
		// What the window last got of its gear.
		DWORD dwGearSent;
		// The owner's hand on its gear (the window): item id -> the wear slot
		// the owner put it on, or PLAYERBOT_SIDEKICK_PIN_UNWANTED for a piece
		// the owner took off.
		std::map<DWORD, BYTE> mapPins;
		// What the bag window was last sent, by position, the batch number and
		// the yang beside it.
		std::map<int, DWORD> mapEqSent;
		DWORD dwEqGen;
		long long llEqGoldSent;
		// A piece the owner put on while a blow was fresh: the fight holds off
		// until it is on or this passes.
		DWORD dwEquipWaitUntil;
		TPlayerBotSidekickRuntime()
			: dwNextPartyCheck(0), dwNextService(0), dwNextLoot(0), dwNextCatchUp(0), dwLootVID(0),
			  dwLootSince(0), dwNextProtect(0), bTrading(false), dwLastFoeVID(0), bHold(false), lHoldMap(0),
			  lHoldX(0), lHoldY(0), bErrand(false), bErrandVisit(false), dwErrandSince(0), llErrandGold(0),
			  dwGearSent(0), dwEqGen(0), llEqGoldSent(-1), dwEquipWaitUntil(0)
		{
			memset(adwFoes, 0, sizeof(adwFoes));
		}
	};
	std::map<DWORD, TPlayerBotSidekickRuntime> s_mapPlayerBotSidekickRuntime;

	// The world's switch: M2_SIDEKICK=0 in .env (the launcher's difficulty
	// window) is the event flag m2_sidekick_off, which the migrator writes at a
	// start. Off, nobody's companion comes into the world and one already in
	// it logs out, the letter is not sent (towarzysz.quest) and the command
	// says why. Asked of the event flag every time, a map lookup.
	bool IsPlayerBotSidekickSwitchedOff()
	{
		return quest::CQuestManager::instance().GetEventFlag("m2_sidekick_off") != 0;
	}

	// ------------------------------------------------------------ the record

	bool EnsurePlayerBotSidekickTable()
	{
		if (s_bPlayerBotSidekickTable)
			return true;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick ("
				"owner_pid INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"mode TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"stance TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"loot TINYINT UNSIGNED NOT NULL DEFAULT 2, "
				"protect TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"buffs TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"skill_group TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"start_level TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"setup_done TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"created_at DATETIME NOT NULL, "
				"UNIQUE KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> gifts(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick_gift ("
				"item_id INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"given_at DATETIME NOT NULL, "
				"KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		s_bPlayerBotSidekickTable = msg.get() && msg->uiSQLErrno == 0 && gifts.get() && gifts->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickTable)
		{
			sys_err("PLAYERBOT_SIDEKICK: the companion tables cannot be created");
			return false;
		}
		// The stance and the window's settings came a day after the table, which
		// the test world already held; without the columns the companions still
		// load, with the defaults, and keep what they are told while the core
		// runs.
		std::unique_ptr<SQLMsg> settings(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick "
				"ADD COLUMN IF NOT EXISTS stance TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER mode, "
				"ADD COLUMN IF NOT EXISTS loot TINYINT UNSIGNED NOT NULL DEFAULT 2 AFTER stance, "
				"ADD COLUMN IF NOT EXISTS protect TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER loot, "
				"ADD COLUMN IF NOT EXISTS buffs TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER protect, "
				"ADD COLUMN IF NOT EXISTS manual_skills TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER buffs"));
		s_bPlayerBotSidekickSettingsColumns = settings.get() && settings->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickSettingsColumns)
			sys_err("PLAYERBOT_SIDEKICK: no settings columns errno=%u", settings.get() ? settings->uiSQLErrno : 0U);
		// What the owner put on or took off in the window. Without the table
		// the marks hold while the core runs.
		std::unique_ptr<SQLMsg> pins(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick_pin ("
				"item_id INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"wear TINYINT UNSIGNED NOT NULL, "
				"pinned_at DATETIME NOT NULL, "
				"KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		s_bPlayerBotSidekickPinTable = pins.get() && pins->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickPinTable)
			sys_err("PLAYERBOT_SIDEKICK: no pin table errno=%u", pins.get() ? pins->uiSQLErrno : 0U);
		// A companion whose owner's character was deleted would be kept out of
		// the population for good: its record goes, and the identity plays on
		// as the bot it was, under the name it was given.
		std::unique_ptr<SQLMsg> orphans(AccountDB::instance().DirectQuery(
				"DELETE s FROM player.playerbot_sidekick AS s LEFT JOIN player.player AS p ON p.id=s.owner_pid "
				"WHERE p.id IS NULL"));
		if (orphans.get() && orphans->uiSQLErrno == 0 && orphans->Get() && orphans->Get()->uiAffectedRows > 0)
			sys_log(0, "PLAYERBOT_SIDEKICK: dropped %u companions of deleted characters",
					(unsigned int)orphans->Get()->uiAffectedRows);
		return true;
	}

	void LoadPlayerBotSidekicks()
	{
		if (!EnsurePlayerBotSidekickTable())
			return;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(s_bPlayerBotSidekickSettingsColumns
				? "SELECT owner_pid, sidekick_pid, mode, skill_group, start_level, setup_done, stance, loot, "
				  "protect, buffs, manual_skills FROM player.playerbot_sidekick"
				: "SELECT owner_pid, sidekick_pid, mode, skill_group, start_level, setup_done, 0, 2, 1, 1, 0 "
				  "FROM player.playerbot_sidekick"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		TPlayerBotSidekickMap fresh;
		std::map<DWORD, DWORD> owners;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TPlayerBotSidekick rec;
			unsigned int mode = 0, group = 0, level = 1, done = 0, stance = 0, loot = 2, protect = 1, buffs = 1,
					manual = 0;
			if (row[0]) str_to_number(rec.dwOwnerPID, row[0]);
			if (row[1]) str_to_number(rec.dwSidekickPID, row[1]);
			if (row[2]) str_to_number(mode, row[2]);
			if (row[3]) str_to_number(group, row[3]);
			if (row[4]) str_to_number(level, row[4]);
			if (row[5]) str_to_number(done, row[5]);
			if (row[6]) str_to_number(stance, row[6]);
			if (row[7]) str_to_number(loot, row[7]);
			if (row[8]) str_to_number(protect, row[8]);
			if (row[9]) str_to_number(buffs, row[9]);
			if (row[10]) str_to_number(manual, row[10]);
			if (rec.dwOwnerPID == 0 || rec.dwSidekickPID == 0)
				continue;
			rec.bMode = mode == PLAYERBOT_SIDEKICK_FREE ? PLAYERBOT_SIDEKICK_FREE : PLAYERBOT_SIDEKICK_FOLLOW;
			rec.bStance = (BYTE)std::min<unsigned int>(stance, PLAYERBOT_SIDEKICK_STANCE_PASSIVE);
			rec.bLoot = (BYTE)std::min<unsigned int>(loot, PLAYERBOT_SIDEKICK_LOOT_ALL);
			rec.bProtect = protect != 0;
			rec.bBuffs = buffs != 0;
			rec.bManualSkills = manual != 0;
			rec.bGroup = (BYTE)std::min<unsigned int>(group, 2);
			rec.bLevel = (BYTE)std::max<unsigned int>(1, std::min<unsigned int>(level, 255));
			rec.bSetupDone = done != 0;
			TPlayerBotSidekickMap::const_iterator old = s_mapPlayerBotSidekicks.find(rec.dwOwnerPID);
			if (old != s_mapPlayerBotSidekicks.end() && old->second.dwSidekickPID == rec.dwSidekickPID)
			{
				rec.dwOwnerSeenAt = old->second.dwOwnerSeenAt;
				rec.dwNextSpawnTry = old->second.dwNextSpawnTry;
				// The setup this core did is newer than the row it wrote.
				rec.bSetupDone = rec.bSetupDone || old->second.bSetupDone;
				// With no columns to keep them in, the settings live as long as
				// the core.
				if (!s_bPlayerBotSidekickSettingsColumns)
				{
					rec.bStance = old->second.bStance;
					rec.bLoot = old->second.bLoot;
					rec.bProtect = old->second.bProtect;
					rec.bBuffs = old->second.bBuffs;
					rec.bManualSkills = old->second.bManualSkills;
				}
			}
			fresh[rec.dwOwnerPID] = rec;
			owners[rec.dwSidekickPID] = rec.dwOwnerPID;
		}
		s_mapPlayerBotSidekicks.swap(fresh);
		s_mapPlayerBotSidekickOwner.swap(owners);
	}

	bool IsPlayerBotSidekickPID(DWORD pid)
	{
		return s_mapPlayerBotSidekickOwner.find(pid) != s_mapPlayerBotSidekickOwner.end();
	}

	// A bot that owns a companion - only under the self-test, since a person's
	// party is none of the party pass's business anyway: its party is the
	// companion's, and the cohort rule must not break it up every minute.
	bool IsPlayerBotSidekickOwnerPID(DWORD pid)
	{
		return s_bPlayerBotSidekickSelfTest && s_mapPlayerBotSidekicks.find(pid) != s_mapPlayerBotSidekicks.end();
	}

	TPlayerBotSidekick* FindPlayerBotSidekickOf(DWORD sidekickPid)
	{
		std::map<DWORD, DWORD>::const_iterator owner = s_mapPlayerBotSidekickOwner.find(sidekickPid);
		if (owner == s_mapPlayerBotSidekickOwner.end())
			return NULL;
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->second);
		return rec == s_mapPlayerBotSidekicks.end() ? NULL : &rec->second;
	}

	// The owner, when it is a person in this core's world (or a bot, under the
	// self-test).
	LPCHARACTER GetPlayerBotSidekickOwnerChar(DWORD ownerPid)
	{
		LPCHARACTER owner = CHARACTER_MANAGER::instance().FindByPID(ownerPid);
		if (!owner || !owner->IsPC() || !owner->GetDesc())
			return NULL;
		if (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest)
			return NULL;
		return owner;
	}

	const TPlayerBotSidekickRuntime* FindPlayerBotSidekickRuntime(DWORD sidekickPid)
	{
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt = s_mapPlayerBotSidekickRuntime.find(sidekickPid);
		return rt == s_mapPlayerBotSidekickRuntime.end() ? NULL : &rt->second;
	}

	// Sent to town by its owner ("Idz na zakupy"): the ordinary town visit
	// runs it, so for that while it is nobody's to hold back.
	bool IsPlayerBotSidekickOnErrand(DWORD sidekickPid)
	{
		const TPlayerBotSidekickRuntime* rt = FindPlayerBotSidekickRuntime(sidekickPid);
		return rt && rt->bErrand;
	}

	// Told to wait where it stands ("Czekaj tutaj").
	bool IsPlayerBotSidekickHolding(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekickRuntime* rt = FindPlayerBotSidekickRuntime(ch->GetPlayerID());
		return rt && rt->bHold;
	}

	// A companion at its owner's side: in follow mode, the owner in this core's
	// world. Everything that asks "is this bot somebody's" asks this too
	// (IsPlayerBotHeldForCompany), so none of the errands take it away - but
	// the one its owner sent it on.
	bool IsPlayerBotSidekickLeashed(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bMode == PLAYERBOT_SIDEKICK_FOLLOW && GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID) != NULL &&
				!IsPlayerBotSidekickOnErrand(ch->GetPlayerID());
	}

	// Whose companion this is, for the line over its head
	// (BuildPlayerBotStatusText): the owner's name while it is at the owner's
	// side, nothing otherwise - let off the leash it plays, and says so, like
	// any bot.
	const char* GetPlayerBotSidekickOwnerName(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return NULL;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW || IsPlayerBotSidekickOnErrand(ch->GetPlayerID()))
			return NULL;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner ? owner->GetName() : NULL;
	}

	// Standing at its owner's side, which the inactivity watchdog reads as
	// stillness on purpose: an owner who stands about, stands about with it -
	// and one told to wait at a spot waits there.
	bool IsPlayerBotSidekickBesideOwner(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW || IsPlayerBotSidekickOnErrand(ch->GetPlayerID()))
			return false;
		if (IsPlayerBotSidekickHolding(ch))
			return true;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner && owner->GetMapIndex() == ch->GetMapIndex() &&
				DISTANCE_APPROX(owner->GetX() - ch->GetX(), owner->GetY() - ch->GetY()) <=
						PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE;
	}

	// Whose companion this is, when the asker is the owner or the leader of
	// the owner's party: the only invitations a companion takes.
	bool IsPlayerBotSidekickInviteFromOwner(LPCHARACTER ch, LPCHARACTER leader)
	{
		const TPlayerBotSidekick* rec = ch ? FindPlayerBotSidekickOf(ch->GetPlayerID()) : NULL;
		if (!rec || !leader)
			return false;
		if (leader->GetPlayerID() == rec->dwOwnerPID)
			return true;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner && owner->GetParty() && owner->GetParty()->GetLeaderPID() == leader->GetPlayerID();
	}

	bool IsPlayerBotSidekickGift(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || s_mapPlayerBotSidekickRuntime.empty())
			return false;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		return rt != s_mapPlayerBotSidekickRuntime.end() &&
				rt->second.setGifts.find(item->GetID()) != rt->second.setGifts.end();
	}

	// The owner's mark on a piece of its companion's (the window, below): the
	// wear slot the owner put it on, PLAYERBOT_SIDEKICK_PIN_UNWANTED for one
	// the owner took off, -1 for none. Asked by the equipment, refine, bonus
	// and junk passes of every bot, so the empty case is one test.
	int GetPlayerBotSidekickPinOf(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || s_mapPlayerBotSidekickRuntime.empty())
			return -1;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		if (rt == s_mapPlayerBotSidekickRuntime.end() || rt->second.mapPins.empty())
			return -1;
		std::map<DWORD, BYTE>::const_iterator pin = rt->second.mapPins.find(item->GetID());
		return pin == rt->second.mapPins.end() ? -1 : (int)pin->second;
	}

	// Put on by its owner: kept on, never refined, its lines never changed.
	bool IsPlayerBotSidekickPinned(LPCHARACTER ch, LPITEM item)
	{
		const int pin = GetPlayerBotSidekickPinOf(ch, item);
		return pin >= 0 && pin != PLAYERBOT_SIDEKICK_PIN_UNWANTED;
	}

	// Taken off by its owner: the AI never puts it back on.
	bool IsPlayerBotSidekickUnwanted(LPCHARACTER ch, LPITEM item)
	{
		return GetPlayerBotSidekickPinOf(ch, item) == PLAYERBOT_SIDEKICK_PIN_UNWANTED;
	}

	// A piece its owner put on that waits in the bag - refused for the moment
	// (a blow in the last second and a half), or taken off by something since
	// (a fishing session with the leash let go) - with the slot it goes to.
	// askEngine: only one the engine would let on now.
	LPITEM FindPlayerBotSidekickPinnedInBag(LPCHARACTER ch, int& wearCell, bool askEngine)
	{
		wearCell = -1;
		if (!ch || s_mapPlayerBotSidekickRuntime.empty())
			return NULL;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		if (rt == s_mapPlayerBotSidekickRuntime.end() || rt->second.mapPins.empty())
			return NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			std::map<DWORD, BYTE>::const_iterator pin = rt->second.mapPins.find(item->GetID());
			if (pin == rt->second.mapPins.end() || pin->second == PLAYERBOT_SIDEKICK_PIN_UNWANTED ||
					pin->second >= WEAR_MAX_NUM)
				continue;
			LPITEM worn = ch->GetWear(pin->second);
			if (worn && IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;
			if (askEngine && (item->isLocked() || item->IsExchanging() ||
					!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell))))
				continue;
			wearCell = pin->second;
			return item;
		}
		return NULL;
	}

	// The owner spends its skill points (the window's "reczne"), and the skill
	// pass leaves them alone.
	bool IsPlayerBotSidekickManualSkills(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bManualSkills;
	}

	void SetPlayerBotSidekickFlag(DWORD ownerPid, const char* flag, int value)
	{
		quest::PC* pc = quest::CQuestManager::instance().GetPC(ownerPid);
		if (pc)
			pc->SetFlag(flag, value);
	}

	void SayPlayerBotSidekick(LPCHARACTER owner, const char* text)
	{
		if (owner && owner->GetDesc() && !owner->GetDesc()->IsBot())
			owner->ChatPacket(CHAT_TYPE_INFO, "[Towarzysz] %s", text);
		// The self-test's owner is a bot with no client, and an order it gave
		// that was refused would otherwise leave no trace at all.
		else if (owner && s_bPlayerBotSidekickSelfTest)
			sys_log(0, "PLAYERBOT_SIDEKICK: says owner=%u \"%s\"", owner->GetPlayerID(), text);
	}

	const char* GetPlayerBotSidekickStanceName(BYTE stance)
	{
		switch (stance)
		{
			case PLAYERBOT_SIDEKICK_STANCE_DEFEND: return "nie atakuje pierwszy";
			case PLAYERBOT_SIDEKICK_STANCE_PASSIVE: return "nie walczy, tylko sie broni";
			default: return "atakuje wszystko w poblizu";
		}
	}

	// --------------------------------------------------------- being born

	const char* GetPlayerBotSidekickClassName(BYTE race)
	{
		switch (race % 4)
		{
			case 0: return "Wojownik";
			case 1: return "Ninja";
			case 2: return "Sura";
			default: return "Szaman";
		}
	}

	// The owner's level on the companion, the way pc.set_level makes a level
	// (questlua_pc.cpp): the points the levels bring, the random health and
	// mana, full bars.
	void RaisePlayerBotSidekickLevel(LPCHARACTER ch, int target)
	{
		target = MINMAX(1, target, gPlayerMaxLevel);
		const int oldLevel = ch->GetLevel();
		if (target <= oldLevel)
			return;
		ch->PointChange(POINT_SKILL, target - oldLevel);
		ch->PointChange(POINT_SUB_SKILL, target < 10 ? 0 : target - MAX(oldLevel, 9));
		ch->PointChange(POINT_STAT, (target - oldLevel) * 3 + ch->GetPoint(POINT_LEVEL_STEP));
		ch->PointChange(POINT_LEVEL, target - oldLevel);
		ch->SetRandomHP((target - 1) * number(JobInitialPoints[ch->GetJob()].hp_per_lv_begin,
				JobInitialPoints[ch->GetJob()].hp_per_lv_end));
		ch->SetRandomSP((target - 1) * number(JobInitialPoints[ch->GetJob()].sp_per_lv_begin,
				JobInitialPoints[ch->GetJob()].sp_per_lv_end));
		ch->ComputePoints();
		ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
		ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
		ch->PointsPacket();
		ch->SkillLevelPacket();
	}

	// A free identity of the race asked for in the owner's kingdom: every
	// guard of the registry (LoadRegisteredBots), nobody's companion yet, out
	// of every world and out of the db core's cache, not one of the operator's
	// medal droppers, with no offline shop of its own. The level nearest the
	// owner's from below first, so the companion is raised rather than stands
	// over the owner.
	DWORD PickPlayerBotSidekickIdentity(BYTE empire, BYTE race, int ownerLevel)
	{
		char query[1600];
		snprintf(query, sizeof(query),
				"SELECT l.pid FROM common.playerbot_seed_state AS l "
				"JOIN player.player AS p ON p.id=l.pid "
				"JOIN account.account AS a ON a.id=p.account_id "
				"JOIN player.player_index AS pi ON pi.id=a.id "
				"WHERE l.seed_version=1 AND l.state IN ('complete','adopted') "
				"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
				"AND BINARY a.social_id=BINARY CONCAT('9',LPAD(l.pid-3,12,'0')) "
				"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 "
				"AND pi.empire=%u AND p.job=%u "
				// Never played (playtime 0: never in any world, so never in the
				// cache) or saved long enough ago. last_play alone refused every
				// identity of a world seeded minutes ago, which is exactly when a
				// new player meets the letter: the column defaults to the moment
				// the seed wrote the row.
				"AND (p.playtime = 0 OR p.last_play < NOW() - INTERVAL %d MINUTE) "
				"AND l.pid NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick) "
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
				"AND l.pid NOT IN (SELECT owner FROM player.ikashop_offlineshop) "
#endif
				"ORDER BY (p.level > %d), ABS(CAST(p.level AS SIGNED) - %d), l.pid DESC LIMIT 120",
				(unsigned int)empire, (unsigned int)race, PLAYERBOT_SIDEKICK_IDENTITY_IDLE_MINUTES,
				ownerLevel, ownerLevel);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_err("PLAYERBOT_SIDEKICK: identity query failed errno=%u", msg.get() ? msg->uiSQLErrno : 0U);
			return 0;
		}
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0;
			if (row[0])
				str_to_number(pid, row[0]);
			if (pid == 0 || !CPlayerBotManager::instance().IsRegisteredBotPID(pid) ||
					CPlayerBotManager::instance().IsManaged(pid) ||
					CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) ||
					CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid))
				continue;
			return pid;
		}
		return 0;
	}

	bool IsPlayerBotSidekickNameAllowed(const char* name)
	{
		const size_t length = name ? strlen(name) : 0;
		if (length < PLAYERBOT_SIDEKICK_NAME_MIN || length > PLAYERBOT_SIDEKICK_NAME_MAX)
			return false;
		for (size_t i = 0; i < length; ++i)
		{
			const char c = name[i];
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
				return false;
		}
		// The engine's own rules for a character's name, and the ban list.
		return check_name && check_name(name) && !CBanwordManager::instance().CheckString(name, length);
	}

	bool CreatePlayerBotSidekick(LPCHARACTER owner, int race, int group, const char* name)
	{
		if (!owner || !EnsurePlayerBotSidekickTable())
			return false;
		const DWORD ownerPid = owner->GetPlayerID();
		// Another core may have made one a moment ago; the table decides.
		LoadPlayerBotSidekicks();
		if (s_mapPlayerBotSidekicks.find(ownerPid) != s_mapPlayerBotSidekicks.end())
		{
			SetPlayerBotSidekickFlag(ownerPid, "towarzysz.created", 1);
			SayPlayerBotSidekick(owner, "Masz juz towarzysza.");
			return false;
		}
		if (race < 0 || race > 7 || group < 1 || group > 2)
		{
			SayPlayerBotSidekick(owner, "Nie ma takiej klasy albo sciezki.");
			return false;
		}
		if (!IsPlayerBotSidekickNameAllowed(name))
		{
			SayPlayerBotSidekick(owner, "Ten nick nie pasuje: od 3 do 16 liter i cyfr, bez polskich znakow i spacji.");
			return false;
		}
		{
			char query[256];
			snprintf(query, sizeof(query), "SELECT COUNT(*) FROM player.player WHERE name='%s'", name);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			MYSQL_ROW row = NULL;
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult ||
					!(row = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				SayPlayerBotSidekick(owner, "Nie udalo sie sprawdzic nicku, sprobuj za chwile.");
				return false;
			}
			unsigned int taken = 0;
			if (row[0])
				str_to_number(taken, row[0]);
			if (taken != 0)
			{
				SayPlayerBotSidekick(owner, "Ten nick jest juz zajety - wybierz inny.");
				return false;
			}
		}
		const DWORD pid = PickPlayerBotSidekickIdentity(owner->GetEmpire(), (BYTE)race, owner->GetLevel());
		if (pid == 0)
		{
			SayPlayerBotSidekick(owner, "Nie ma teraz wolnej postaci tej klasy w twoim krolestwie - wybierz inna albo sprobuj pozniej.");
			sys_log(0, "PLAYERBOT_SIDEKICK: no identity for owner=%u race=%d empire=%u",
					ownerPid, race, (unsigned int)owner->GetEmpire());
			return false;
		}
		char query[512];
		// The seed's name stays in the name history as this identity's own, so
		// the name pool reads the new one as a person's choice and keeps it at
		// every later start, 'restore' included (generate_seed.py).
		snprintf(query, sizeof(query),
				"INSERT IGNORE INTO common.playerbot_name_history (pid, seed_name, human_name, pool_version, renamed_at) "
				"SELECT id, name, name, 'sidekick', NOW() FROM player.player WHERE id=%u", pid);
		std::unique_ptr<SQLMsg> history(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "UPDATE player.player SET name='%s' WHERE id=%u", name, pid);
		std::unique_ptr<SQLMsg> rename(AccountDB::instance().DirectQuery(query));
		if (!rename.get() || rename->uiSQLErrno != 0)
		{
			SayPlayerBotSidekick(owner, "Nie udalo sie nadac nicku - sprobuj innego.");
			return false;
		}
		const int level = MINMAX(1, owner->GetLevel(), 255);
		snprintf(query, sizeof(query),
				"INSERT INTO player.playerbot_sidekick (owner_pid, sidekick_pid, mode, skill_group, start_level, setup_done, created_at) "
				"VALUES (%u, %u, 0, %d, %d, 0, NOW())", ownerPid, pid, group, level);
		std::unique_ptr<SQLMsg> insert(AccountDB::instance().DirectQuery(query));
		if (!insert.get() || insert->uiSQLErrno != 0)
		{
			SayPlayerBotSidekick(owner, "Nie udalo sie zapisac towarzysza - sprobuj za chwile.");
			return false;
		}
		TPlayerBotSidekick rec;
		rec.dwOwnerPID = ownerPid;
		rec.dwSidekickPID = pid;
		rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
		rec.bGroup = (BYTE)group;
		rec.bLevel = (BYTE)level;
		rec.bSetupDone = false;
		rec.dwOwnerSeenAt = get_dword_time();
		s_mapPlayerBotSidekicks[ownerPid] = rec;
		s_mapPlayerBotSidekickOwner[pid] = ownerPid;
		SetPlayerBotSidekickFlag(ownerPid, "towarzysz.created", 1);
		char text[192];
		snprintf(text, sizeof(text), "%s (%s) dolacza do ciebie - chwila i bedzie przy tobie.",
				name, GetPlayerBotSidekickClassName((BYTE)race));
		SayPlayerBotSidekick(owner, text);
		sys_log(0, "PLAYERBOT_SIDEKICK: created owner=%u owner_name=%s pid=%u name=%s race=%d group=%d level=%d",
				ownerPid, owner->GetName(), pid, name, race, group, level);
		CPlayerBotManager::instance().SpawnSidekick(pid);
		return true;
	}

	// ---------------------------------------------------------- the party

	// In the owner's party, "exp leci nam po rowno". A party the owner leads,
	// or none, which is made for the owner the way the engine makes one when
	// its first invitation is accepted (CHARACTER::PartyInviteAccept); the
	// split is set once, when this makes the party - after that it is the
	// leader's. Another player's party is its leader's to fill: the companion
	// takes an invitation from that leader (AcceptPlayerBotPartyInvite) and
	// otherwise follows its owner outside it. And not while the owner is in a
	// dungeon, where the engine refuses a new member too (PERR_DUNGEON).
	void KeepPlayerBotSidekickInParty(LPCHARACTER ch, LPCHARACTER owner, DWORD dwNow)
	{
		LPPARTY party = owner->GetParty();
		if (party && ch->GetParty() == party)
			return;
		if (owner->GetDungeon())
			return;
		if (party && party->GetLeaderPID() != owner->GetPlayerID())
		{
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			PlayerBotLogThrottled("sidekick_party_other", dwNow,
					"PLAYERBOT_SIDEKICK: owner is in another leader's party pid=%u name=%s owner=%u leader=%u",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), party->GetLeaderPID());
			return;
		}
		if (party && party->GetMemberCount() >= PARTY_MAX_MEMBER)
		{
			PlayerBotLogThrottled("sidekick_party_full", dwNow,
					"PLAYERBOT_SIDEKICK: owner's party is full pid=%u name=%s owner=%u",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID());
			return;
		}
		if (ch->GetParty())
			LeavePlayerBotParty(ch);
		if (!party)
		{
			party = CPartyManager::instance().CreateParty(owner);
			if (!party)
				return;
			party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			sys_log(0, "PLAYERBOT_SIDEKICK: made a party for owner=%u name=%s",
					owner->GetPlayerID(), owner->GetName());
		}
		party->Join(ch->GetPlayerID());
		party->Link(ch);
		sys_log(0, "PLAYERBOT_SIDEKICK: joined the owner's party pid=%u name=%s owner=%u members=%d",
				ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), (int)party->GetMemberCount());
	}

	// ------------------------------------------------------ coming and going

	// A point beside the owner, a little to one side by pid, on ground a bot
	// may stand on.
	void GetPlayerBotSidekickSpot(LPCHARACTER ch, LPCHARACTER owner, long& x, long& y)
	{
		const DWORD h = PlayerBotNavHash(ch->GetPlayerID() ^ 0x534b5053U);
		x = owner->GetX() + (long)(h % 301) - 150;
		y = owner->GetY() + (long)((h >> 9) % 301) - 150;
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(owner->GetMapIndex(), x, y);
		if (!tree || tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT))
		{
			x = owner->GetX();
			y = owner->GetY();
		}
	}

	// Put the companion beside its owner, on the owner's map. Not
	// TransitionPlayerBotMap: that one refuses a map with no navigation grid
	// and turns a warp into the Spider Dungeon into a desert crossing, and a
	// companion goes wherever its owner went - a dungeon instance included,
	// with the membership Entergame would give a reconnecting player (as
	// CPlayerBotManager::WarpBot does).
	bool PlacePlayerBotSidekick(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow,
			const char* reason)
	{
		long x = 0, y = 0;
		GetPlayerBotSidekickSpot(ch, owner, x, y);
		const long targetMap = owner->GetMapIndex();
		const long oldMap = ch->GetMapIndex();
		const bool wasRiding = ch->IsRiding();
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		if (wasRiding)
			ch->StopRiding();
		ch->HorseSummon(false);
		ClearPlayerBotRoute(state, true);
		state.bVisitingShop = false;
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		state.bTacticalRetreat = false;
		state.lDesertCrossingTo = 0;
		LPDUNGEON before = ch->GetDungeon();
		if (!ch->Show(targetMap, x, y, 0))
		{
			PlayerBotLogThrottled("sidekick_place_failed", dwNow,
					"PLAYERBOT_SIDEKICK: cannot stand beside the owner pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason);
			return false;
		}
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
		LPDUNGEON after = targetMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
				? CDungeonManager::instance().FindByMapIndex(targetMap) : NULL;
		if (before != after)
		{
			if (before)
				ch->SetDungeon(NULL);
			if (after)
				ch->SetDungeon(after);
		}
		CPlayerBotNavigation::instance(targetMap).Init(targetMap);
		state.lLastX = x;
		state.lLastY = y;
		state.dwLastMeaningfulActivityTime = dwNow;
		if (oldMap != targetMap)
		{
			ch->Save();
			sys_log(0, "PLAYERBOT_SIDEKICK: beside the owner pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason);
		}
		return true;
	}

	// A walk on a map with no navigation grid (a dungeon the bots never go to):
	// the server moves a bot in a straight line, with nothing in the way.
	void WalkPlayerBotSidekickDirect(LPCHARACTER ch, long x, long y, DWORD dwNow)
	{
		if (ch->Goto(x, y))
			ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
	}

	void WalkPlayerBotSidekick(LPCHARACTER ch, long x, long y, DWORD dwNow, bool allowHorse)
	{
		if (CPlayerBotNavigation::instance(ch->GetMapIndex()).Init(ch->GetMapIndex()) &&
				MovePlayerBot(ch, x, y, dwNow, 4, true, allowHorse, false, allowHorse))
			return;
		WalkPlayerBotSidekickDirect(ch, x, y, dwNow);
	}

	void LoadPlayerBotSidekickGifts(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt)
	{
		char query[512];
		// Forget what is gone for a day: sold by the owner's hand, burnt, or
		// taken back. A day, because the db core writes an item's new owner
		// minutes after the trade.
		snprintf(query, sizeof(query),
				"DELETE g FROM player.playerbot_sidekick_gift AS g LEFT JOIN player.item AS i ON i.id=g.item_id "
				"WHERE g.sidekick_pid=%u AND i.id IS NULL AND g.given_at < NOW() - INTERVAL 1 DAY", sidekickPid);
		std::unique_ptr<SQLMsg> prune(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "SELECT item_id FROM player.playerbot_sidekick_gift WHERE sidekick_pid=%u",
				sidekickPid);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD id = 0;
			if (row[0])
				str_to_number(id, row[0]);
			if (id != 0)
				rt.setGifts.insert(id);
		}
		if (!s_bPlayerBotSidekickPinTable)
			return;
		// The window's marks, forgotten the same way: a day after the piece is
		// nobody's or somebody else's (the owner takes a piece back through the
		// window, which clears its mark at once).
		snprintf(query, sizeof(query),
				"DELETE p FROM player.playerbot_sidekick_pin AS p LEFT JOIN player.item AS i "
				"ON i.id=p.item_id AND i.owner_id=p.sidekick_pid "
				"WHERE p.sidekick_pid=%u AND i.id IS NULL AND p.pinned_at < NOW() - INTERVAL 1 DAY", sidekickPid);
		std::unique_ptr<SQLMsg> prunePins(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "SELECT item_id, wear FROM player.playerbot_sidekick_pin WHERE sidekick_pid=%u",
				sidekickPid);
		std::unique_ptr<SQLMsg> pins(AccountDB::instance().DirectQuery(query));
		if (!pins.get() || pins->uiSQLErrno != 0 || !pins->Get() || !pins->Get()->pSQLResult)
			return;
		while (NULL != (row = mysql_fetch_row(pins->Get()->pSQLResult)))
		{
			DWORD id = 0;
			unsigned int wear = PLAYERBOT_SIDEKICK_PIN_UNWANTED;
			if (row[0])
				str_to_number(id, row[0]);
			if (row[1])
				str_to_number(wear, row[1]);
			if (id != 0 && (wear < WEAR_MAX_NUM || wear == PLAYERBOT_SIDEKICK_PIN_UNWANTED))
				rt.mapPins[id] = (BYTE)wear;
		}
	}

	void OnPlayerBotSidekickLoaded(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec)
			return;
		// The player's, not the population's: no stone role, no party role, and
		// the character of a steady adventurer when it is let off the leash.
		state.bBotRole = BOT_ROLE_MOB_GRINDER;
		state.bPersonality = BOT_PERSONALITY_STEADY_ADVENTURER;
		state.persona.bDrawnPersonality = BOT_PERSONALITY_STEADY_ADVENTURER;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[ch->GetPlayerID()];
		rt = TPlayerBotSidekickRuntime();
		LoadPlayerBotSidekickGifts(ch->GetPlayerID(), rt);
		if (!rec->bSetupDone)
		{
			// The owner's level at its creation, then the path chosen - which a
			// companion under level five takes at five
			// (KeepPlayerBotSidekickPath).
			RaisePlayerBotSidekickLevel(ch, rec->bLevel);
			if (rec->bGroup != 0 && ch->GetLevel() >= 5 && ch->GetSkillGroup() != rec->bGroup)
			{
				ch->ClearSkill();
				ch->SetSkillGroup(rec->bGroup);
			}
			ch->Save();
			rec->bSetupDone = true;
			DBManager::instance().Query("UPDATE player.playerbot_sidekick SET setup_done=1 WHERE sidekick_pid=%u",
					ch->GetPlayerID());
			sys_log(0, "PLAYERBOT_SIDEKICK: set up pid=%u name=%s level=%d group=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), (unsigned int)ch->GetSkillGroup());
		}
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		if (owner && owner->GetSectree() && rec->bMode == PLAYERBOT_SIDEKICK_FOLLOW)
		{
			PlacePlayerBotSidekick(ch, state, owner, dwNow, "sidekick_arrives");
			KeepPlayerBotSidekickInParty(ch, owner, dwNow);
			char text[128];
			snprintf(text, sizeof(text), "%s jest przy tobie.", ch->GetName());
			SayPlayerBotSidekick(owner, text);
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: entered pid=%u name=%s owner=%u mode=%u level=%d gifts=%u",
				ch->GetPlayerID(), ch->GetName(), rec->dwOwnerPID, (unsigned int)rec->bMode, ch->GetLevel(),
				(unsigned int)rt.setGifts.size());
	}

	// The orders, below.
	void ReportPlayerBotSidekick(LPCHARACTER owner, const TPlayerBotSidekick& rec);
	void SummonPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow);
	void FreePlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec);
	void DismissPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick rec);
	bool HandlePlayerBotSidekickWhisper(LPCHARACTER from, LPCHARACTER bot, const char* text);
	void HandlePlayerBotSidekickCommand(LPCHARACTER ch, const char* argument);
	void SendPlayerBotSidekickWindow(LPCHARACTER owner, bool fullGear);
	DWORD GetPlayerBotSidekickSkillBase(LPCHARACTER sk);
	std::string GetPlayerBotSidekickWearRefusal(LPCHARACTER sk, LPITEM item, LPITEM replacing);
	std::string GetPlayerBotSidekickHandOverRefusal(LPITEM item);

	// Where a piece of the companion's stands now: a bag cell, 1000 + the
	// wear slot, or -1 when it has left both.
	int FindPlayerBotSidekickSelfTestPiece(LPCHARACTER ch, DWORD id)
	{
		if (!id)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetID() == id)
				return cell;
		}
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM item = ch->GetWear(wear);
			if (item && item->GetID() == id)
				return PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear;
		}
		return -1;
	}

	// The bag window's self-test (the self-test file's first line "eq"): one
	// of the window's orders every pass of the self-test (half a minute), on
	// the pair it makes or takes up, each answered in the log - where
	// SendPlayerBotSidekickCommand writes every command while a self-test
	// runs. A move in the bag, a piece handed over and taken back, the body
	// armour taken off (and left off by the AI) and put on again (pinned), the
	// skills, the pin undone; then the paths that move items the other way
	// round - two pieces trading places, a stack poured on another, a piece
	// taken off onto a chosen cell, a piece the companion can wear taken to
	// the owner, handed back straight onto its slot, taken back off the slot
	// and given back to the bag - and a skill point spent, each measured
	// before and after (the units of a kind, where a piece ended up). The
	// owner in a self-test is a bot, so the piece that goes round is the
	// companion's own, and each round trip is made in one pass: given half a
	// minute, the owner's own equipment pass put a necklace on.
	void StepPlayerBotSidekickEqSelfTest(LPCHARACTER owner, LPCHARACTER sk, int step)
	{
		char order[96];
		int from = -1;
		int to = -1;
		switch (step)
		{
			case 1:
				HandlePlayerBotSidekickCommand(owner, "eq 1");
				break;
			case 2:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetSize() == 1 && !item->isLocked())
						from = cell;
				}
				for (int cell = PLAYERBOT_BAG_CELLS - 1; cell >= 0 && to < 0; --cell)
					if (!sk->GetInventoryItem(cell) && sk->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
						to = cell;
				snprintf(order, sizeof(order), "eq ruch %d %d", from, to);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 3:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = owner->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetSize() == 1 && !item->isLocked() &&
							!item->IsExchanging() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
					{
						from = cell;
						s_dwPlayerBotSidekickSelfTestGiftID = item->GetID();
					}
				}
				snprintf(order, sizeof(order), "eq daj %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 4:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetID() == s_dwPlayerBotSidekickSelfTestGiftID)
						from = cell;
				}
				snprintf(order, sizeof(order), "eq wez %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 5:
				snprintf(order, sizeof(order), "eq ruch %d -1", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 6:
			{
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && IsPlayerBotSidekickUnwanted(sk, item))
						from = cell;
				}
				LPITEM body = sk->GetWear(WEAR_BODY);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test body_now=%u unwanted_cell=%d", body ? body->GetVnum() : 0U,
						from);
				snprintf(order, sizeof(order), "eq ruch %d %d", from, PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			}
			case 7:
			{
				LPITEM body = sk->GetWear(WEAR_BODY);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test body=%u pinned=%d points=%d", body ? body->GetVnum() : 0U,
						body && IsPlayerBotSidekickPinned(sk, body) ? 1 : 0, (int)sk->GetPoint(POINT_SKILL));
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci");
				const DWORD base = GetPlayerBotSidekickSkillBase(sk);
				if (base != 0 && sk->GetPoint(POINT_SKILL) > 0)
				{
					snprintf(order, sizeof(order), "umiejetnosci dodaj %u", base);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci reczne 0");
				break;
			}
			case 8:
				snprintf(order, sizeof(order), "eq odepnij %d", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				HandlePlayerBotSidekickCommand(owner, "eq");
				break;
			case 9:
			{
				// Two pieces of one size and two kinds trade places.
				int a = -1;
				int b = -1;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && b < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (!item || item->GetCell() != cell || item->isLocked())
						continue;
					LPITEM first = a >= 0 ? sk->GetInventoryItem(a) : NULL;
					if (!first)
						a = cell;
					else if (first->GetSize() == item->GetSize() && first->GetVnum() != item->GetVnum())
						b = cell;
				}
				if (a < 0 || b < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test swap: no pair");
					break;
				}
				const DWORD idA = sk->GetInventoryItem(a)->GetID();
				const DWORD idB = sk->GetInventoryItem(b)->GetID();
				snprintf(order, sizeof(order), "eq ruch %d %d", a, b);
				HandlePlayerBotSidekickCommand(owner, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test swap a=%d b=%d a_now=%d b_now=%d", a, b,
						FindPlayerBotSidekickSelfTestPiece(sk, idA), FindPlayerBotSidekickSelfTestPiece(sk, idB));
				break;
			}
			case 10:
			{
				// A stack poured on another of its kind with room to spare; the
				// units of the kind are the same afterwards.
				int into = -1;
				for (WORD i = 0; i < PLAYERBOT_BAG_CELLS && into < 0; ++i)
				{
					LPITEM x = sk->GetInventoryItem(i);
					if (!x || x->GetCell() != i || !x->IsStackable() || x->isLocked() ||
							(int)x->GetCount() >= PlayerBotMaxStack(x))
						continue;
					for (WORD j = 0; j < PLAYERBOT_BAG_CELLS; ++j)
					{
						LPITEM y = sk->GetInventoryItem(j);
						if (j != i && y && y->GetCell() == j && y->GetVnum() == x->GetVnum() && !y->isLocked())
						{
							from = j;
							into = i;
							break;
						}
					}
				}
				if (into < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test pour: no two stacks of a kind with room");
					break;
				}
				const DWORD vnum = sk->GetInventoryItem(into)->GetVnum();
				const int unitsBefore = sk->CountSpecifyItem(vnum);
				const int intoBefore = (int)sk->GetInventoryItem(into)->GetCount();
				const int fromBefore = (int)sk->GetInventoryItem(from)->GetCount();
				snprintf(order, sizeof(order), "eq ruch %d %d", from, into);
				HandlePlayerBotSidekickCommand(owner, order);
				LPITEM intoNow = sk->GetInventoryItem(into);
				LPITEM fromNow = sk->GetInventoryItem(from);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test pour vnum=%u from=%d(%d->%d) into=%d(%d->%d) units=%d->%d",
						vnum, from, fromBefore, fromNow && fromNow->GetVnum() == vnum ? (int)fromNow->GetCount() : 0,
						into, intoBefore, intoNow ? (int)intoNow->GetCount() : -1, unitsBefore,
						sk->CountSpecifyItem(vnum));
				break;
			}
			case 11:
			{
				// A worn piece taken off onto a cell chosen for it.
				static const int kinds[] = { WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
				LPITEM piece = NULL;
				int wear = -1;
				for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]) && !piece; ++k)
					if ((piece = sk->GetWear(kinds[k])) != NULL)
						wear = kinds[k];
				if (!piece)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take-off: nothing worn to take off");
					break;
				}
				for (int cell = PLAYERBOT_BAG_CELLS - 1; cell >= 0 && to < 0; --cell)
					if (!sk->GetInventoryItem(cell) && sk->IsEmptyItemGrid(TItemPos(INVENTORY, cell), piece->GetSize()))
						to = cell;
				s_dwPlayerBotSidekickSelfTestPieceID = piece->GetID();
				snprintf(order, sizeof(order), "eq ruch %d %d", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear, to);
				HandlePlayerBotSidekickCommand(owner, order);
				LPITEM landed = to >= 0 ? sk->GetInventoryItem(to) : NULL;
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take-off wear=%d vnum=%u to=%d now=%d unwanted=%d", wear,
						piece->GetVnum(), to, FindPlayerBotSidekickSelfTestPiece(sk, s_dwPlayerBotSidekickSelfTestPieceID),
						landed && landed->GetID() == s_dwPlayerBotSidekickSelfTestPieceID &&
						IsPlayerBotSidekickUnwanted(sk, landed) ? 1 : 0);
				break;
			}
			case 12:
			{
				// A piece the companion can wear, from its bag to the owner's and
				// straight back onto its slot in the same pass - a bot owner's own
				// equipment pass would put it on otherwise: given, worn, pinned.
				int wear = -1;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (!item || item->GetCell() != cell || !item->IsEquipable() || item->isLocked() ||
							item->IsExchanging() || !GetPlayerBotSidekickHandOverRefusal(item).empty())
						continue;
					const int cellWear = item->FindEquipCell(sk);
					if (cellWear < 0 || cellWear >= WEAR_MAX_NUM ||
							!GetPlayerBotSidekickWearRefusal(sk, item, sk->GetWear(cellWear)).empty())
						continue;
					from = cell;
					wear = cellWear;
					s_dwPlayerBotSidekickSelfTestGiftID = item->GetID();
				}
				if (from < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test gift to a slot: nothing in the bag it can wear");
					break;
				}
				s_iPlayerBotSidekickSelfTestGiftWear = wear;
				const DWORD id = s_dwPlayerBotSidekickSelfTestGiftID;
				snprintf(order, sizeof(order), "eq wez %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				const int ownerAt = FindPlayerBotSidekickSelfTestPiece(owner, id);
				LPITEM before = sk->GetWear(wear);
				const DWORD beforeID = before ? before->GetID() : 0;
				if (ownerAt >= 0 && ownerAt < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				{
					snprintf(order, sizeof(order), "eq daj %d %d", ownerAt, PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				LPITEM worn = sk->GetWear(wear);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test gift to a slot from=%d owner_had=%d wear=%d gift_now=%d worn_is_gift=%d pinned=%d replaced_now=%d owner_now=%d",
						from, ownerAt, wear, FindPlayerBotSidekickSelfTestPiece(sk, id),
						worn && worn->GetID() == id ? 1 : 0, worn && IsPlayerBotSidekickPinned(sk, worn) ? 1 : 0,
						FindPlayerBotSidekickSelfTestPiece(sk, beforeID), FindPlayerBotSidekickSelfTestPiece(owner, id));
				break;
			}
			case 13:
			{
				// Taken back off the slot and given back to the bag, in one pass.
				const DWORD id = s_dwPlayerBotSidekickSelfTestGiftID;
				const int at = FindPlayerBotSidekickSelfTestPiece(sk, id);
				if (at < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take off the slot: the companion has not got the piece");
					break;
				}
				snprintf(order, sizeof(order), "eq wez %d -1", at);
				HandlePlayerBotSidekickCommand(owner, order);
				const int ownerAt = FindPlayerBotSidekickSelfTestPiece(owner, id);
				const int companionAfterTake = FindPlayerBotSidekickSelfTestPiece(sk, id);
				if (ownerAt >= 0 && ownerAt < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				{
					snprintf(order, sizeof(order), "eq daj %d -1", ownerAt);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test took the piece off the slot from=%d companion_after_take=%d owner_had=%d companion_now=%d owner_now=%d",
						at, companionAfterTake, ownerAt, FindPlayerBotSidekickSelfTestPiece(sk, id),
						FindPlayerBotSidekickSelfTestPiece(owner, id));
				break;
			}
			case 14:
			{
				// A point spent by the owner - the first makes the points the
				// owner's. A companion with none is given one for the test.
				const DWORD base = GetPlayerBotSidekickSkillBase(sk);
				DWORD pick = 0;
				int low = 99;
				for (DWORD vnum = base; base != 0 && vnum < base + 6; ++vnum)
					if (CSkillManager::instance().Get(vnum) && sk->GetSkillMasterType(vnum) == SKILL_NORMAL &&
							sk->GetSkillLevel(vnum) < 17 && sk->GetSkillLevel(vnum) < low)
					{
						pick = vnum;
						low = sk->GetSkillLevel(vnum);
					}
				if (!pick)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point: no skill of the path takes a point");
					break;
				}
				if (sk->GetPoint(POINT_SKILL) <= 0)
				{
					sk->PointChange(POINT_SKILL, 1);
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point: one point given for the test");
				}
				const int pointsBefore = sk->GetPoint(POINT_SKILL);
				snprintf(order, sizeof(order), "umiejetnosci dodaj %u", pick);
				HandlePlayerBotSidekickCommand(owner, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point vnum=%u level=%d->%d points=%d->%d manual=%d", pick,
						low, (int)sk->GetSkillLevel(pick), pointsBefore, (int)sk->GetPoint(POINT_SKILL),
						IsPlayerBotSidekickManualSkills(sk) ? 1 : 0);
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci reczne 0");
				break;
			}
			case 15:
			{
				// The piece left in the bag at step eleven is the AI's again.
				const int at = FindPlayerBotSidekickSelfTestPiece(sk, s_dwPlayerBotSidekickSelfTestPieceID);
				if (at >= 0)
				{
					snprintf(order, sizeof(order), "eq odepnij %d", at);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				HandlePlayerBotSidekickCommand(owner, "eq");
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test done");
				break;
			}
		}
	}

	// The self-test's orders, one every ninety seconds after the companion is
	// made: a trade from the owner with one item in it, the leash let go, the
	// call back, a report, the whispers, the three stances, then the window's
	// orders - the spot kept, the snapshot, the switches, a town visit - and,
	// after an hour, the dismissal: every path a person's letter and window
	// take, on a world with no person in it.
	void StepPlayerBotSidekickSelfTest(DWORD dwNow)
	{
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(s_dwPlayerBotSidekickSelfTestOwner);
		LPCHARACTER owner = CHARACTER_MANAGER::instance().FindByPID(s_dwPlayerBotSidekickSelfTestOwner);
		if (rec == s_mapPlayerBotSidekicks.end() || !owner)
			return;
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		const DWORD age = dwNow - s_dwPlayerBotSidekickSelfTestAt;
		// The window's test goes one order a pass, once the companion is in
		// the world with its bag loaded.
		if (s_bPlayerBotSidekickSelfTestEq)
		{
			if (!sk || !sk->IsItemLoaded() || !CPlayerBotManager::instance().IsManaged(sk->GetPlayerID()))
				return;
			if (s_iPlayerBotSidekickSelfTestStep >= PLAYERBOT_SIDEKICK_EQ_SELFTEST_STEPS)
				return;
			const int eqStep = ++s_iPlayerBotSidekickSelfTestStep;
			sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test step=%d owner=%u sidekick=%u", eqStep, owner->GetPlayerID(),
					sk->GetPlayerID());
			StepPlayerBotSidekickEqSelfTest(owner, sk, eqStep);
			return;
		}
		const int step = (int)(age / 90000U);
		if (step <= s_iPlayerBotSidekickSelfTestStep)
			return;
		s_iPlayerBotSidekickSelfTestStep = step;
		const TPlayerBotSidekickRuntime* rtNow = FindPlayerBotSidekickRuntime(rec->second.dwSidekickPID);
		sys_log(0, "PLAYERBOT_SIDEKICK: self-test step=%d owner=%u sidekick=%u here=%d map=%ld/%ld dist=%d party=%d hold=%d spot=%d errand=%d visit=%d",
				step, owner->GetPlayerID(), rec->second.dwSidekickPID, sk ? 1 : 0, owner->GetMapIndex(),
				sk ? sk->GetMapIndex() : 0L,
				sk ? DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY()) : -1,
				sk && sk->GetParty() && sk->GetParty() == owner->GetParty() ? 1 : 0,
				rtNow && rtNow->bHold ? 1 : 0,
				sk && rtNow && rtNow->bHold ? DISTANCE_APPROX(sk->GetX() - rtNow->lHoldX, sk->GetY() - rtNow->lHoldY) : -1,
				rtNow && rtNow->bErrand ? 1 : 0, rtNow && rtNow->bErrandVisit ? 1 : 0);
		if (step == 1 && sk && !owner->GetExchange() && !sk->GetExchange() &&
				owner->GetMapIndex() == sk->GetMapIndex() && owner->ExchangeStart(sk) && owner->GetExchange())
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = owner->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || item->GetSize() != 1 || item->isLocked() ||
						IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
					continue;
				const bool added = owner->GetExchange()->AddItem(TItemPos(INVENTORY, cell), 0);
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test trade item=%u vnum=%u added=%d", item->GetID(),
						item->GetVnum(), added ? 1 : 0);
				break;
			}
			if (owner->GetExchange())
				owner->GetExchange()->Accept(true);
		}
		else if (step == 2)
			FreePlayerBotSidekick(owner, rec->second);
		else if (step == 4)
			SummonPlayerBotSidekick(owner, rec->second, dwNow);
		else if (step == 5)
			ReportPlayerBotSidekick(owner, rec->second);
		// The same three orders by whisper, as an owner would write them.
		else if (step == 6 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"graj sam\" handled=%d",
					HandlePlayerBotSidekickWhisper(owner, sk, "graj sam") ? 1 : 0);
		else if (step == 8 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"chodz do mnie\" handled=%d",
					HandlePlayerBotSidekickWhisper(owner, sk, "chodz do mnie") ? 1 : 0);
		else if (step == 9 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"ile masz lvl\" handled=%d (expected 0)",
					HandlePlayerBotSidekickWhisper(owner, sk, "ile masz lvl") ? 1 : 0);
		// The three stances, each held for four and a half minutes, with what it
		// took up under the one before: nothing "nearby" under the second and
		// the third, nothing but what hit it under the third.
		else if ((step == 10 || step == 13 || step == 16 || step == 18) && sk)
		{
			TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test foes stance=%u owner_target=%u at_owner=%u at_self=%u nearby=%u",
					(unsigned int)rec->second.bStance, rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET],
					rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_AT_OWNER], rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_AT_SELF],
					rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_NEARBY]);
			memset(rt.adwFoes, 0, sizeof(rt.adwFoes));
			const char* order = step == 10 ? "nie atakuj pierwszy" : step == 13 ? "nie walcz" : step == 16 ? "atakuj" : NULL;
			if (order)
			{
				const bool handled = HandlePlayerBotSidekickWhisper(owner, sk, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"%s\" handled=%d stance=%u", order, handled ? 1 : 0,
						(unsigned int)rec->second.bStance);
			}
		}
		// The window's orders: waiting at a spot while the owner goes on,
		// the snapshot the window is sent, its switches, the call back, and a
		// town visit there and back.
		else if (step == 19)
			HandlePlayerBotSidekickCommand(owner, "czekaj");
		else if (step == 20 || step == 22)
			SendPlayerBotSidekickWindow(owner, true);
		else if (step == 21)
		{
			HandlePlayerBotSidekickCommand(owner, "zbieraj 1");
			HandlePlayerBotSidekickCommand(owner, "ochrona 0");
			HandlePlayerBotSidekickCommand(owner, "buffy 0");
		}
		else if (step == 23)
		{
			HandlePlayerBotSidekickCommand(owner, "zbieraj 2");
			HandlePlayerBotSidekickCommand(owner, "ochrona 1");
			HandlePlayerBotSidekickCommand(owner, "buffy 1");
			if (sk)
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"chodz\" handled=%d",
						HandlePlayerBotSidekickWhisper(owner, sk, "chodz") ? 1 : 0);
		}
		// Asked again at every step until it begins: a companion lying down
		// at the moment of the order says "Najpierw musze wstac" and does
		// nothing, which on 25 September was the whole of the first try.
		else if (step >= 24 && step <= 29 && !s_bPlayerBotSidekickSelfTestErrand)
		{
			const unsigned int before = s_uPlayerBotSidekickErrands;
			HandlePlayerBotSidekickCommand(owner, "zakupy");
			s_bPlayerBotSidekickSelfTestErrand = s_uPlayerBotSidekickErrands != before;
		}
		else if (step == 40)
			DismissPlayerBotSidekick(owner, rec->second);
	}

	// The self-test: a bot the core picks owns a companion made for it.
	void RunPlayerBotSidekickSelfTest(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotSidekickNextSelfTestCheck)
			return;
		s_dwPlayerBotSidekickNextSelfTestCheck = dwNow + 30000;
		struct stat st;
		const bool on = stat(PLAYERBOT_SIDEKICK_SELFTEST_FILE, &st) == 0;
		if (on != s_bPlayerBotSidekickSelfTest)
		{
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test %s", on ? "on" : "off");
			s_dwPlayerBotSidekickSelfTestSince = dwNow;
		}
		s_bPlayerBotSidekickSelfTest = on;
		if (!on)
			return;
		// The file's first line "eq": the bag window's orders alone.
		{
			char line[16] = "";
			FILE* file = fopen(PLAYERBOT_SIDEKICK_SELFTEST_FILE, "r");
			if (file)
			{
				if (!fgets(line, sizeof(line), file))
					line[0] = 0;
				fclose(file);
			}
			const bool eq = !strncmp(line, "eq", 2);
			if (eq != s_bPlayerBotSidekickSelfTestEq)
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test of the bag window %s", eq ? "on" : "off");
			s_bPlayerBotSidekickSelfTestEq = eq;
		}
		if (s_dwPlayerBotSidekickSelfTestOwner != 0)
		{
			StepPlayerBotSidekickSelfTest(dwNow);
			return;
		}
		// A pair an earlier run made - its owner a bot - is taken up again
		// after a restart rather than a second one made, its owner waited for
		// while the spawn window may still bring it in.
		bool ownerComing = false;
		for (TPlayerBotSidekickMap::const_iterator it = s_mapPlayerBotSidekicks.begin();
				it != s_mapPlayerBotSidekicks.end(); ++it)
		{
			if (!CPlayerBotManager::instance().IsRegisteredBotPID(it->first))
				continue;
			if (!CHARACTER_MANAGER::instance().FindByPID(it->first))
			{
				ownerComing = true;
				continue;
			}
			s_dwPlayerBotSidekickSelfTestOwner = it->first;
			s_dwPlayerBotSidekickSelfTestAt = dwNow;
			s_iPlayerBotSidekickSelfTestStep = 0;
			s_bPlayerBotSidekickSelfTestErrand = false;
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test takes up owner=%u sidekick=%u", it->first,
					it->second.dwSidekickPID);
			return;
		}
		if (ownerComing && dwNow - s_dwPlayerBotSidekickSelfTestSince < PLAYERBOT_SIDEKICK_SELFTEST_OWNER_WAIT_MS)
			return;
		// An owner that is a bot of ten or more with no companion and not one
		// itself, on a village map; the companion of the other class.
		const CHARACTER_MANAGER::NAME_MAP& pcs = CHARACTER_MANAGER::instance().GetPCMap();
		for (CHARACTER_MANAGER::NAME_MAP::const_iterator it = pcs.begin(); it != pcs.end(); ++it)
		{
			LPCHARACTER c = it->second;
			if (!c || !c->GetDesc() || !c->GetDesc()->IsBot() || c->GetLevel() < 10 || c->IsDead() ||
					IsPlayerBotSidekickPID(c->GetPlayerID()) ||
					s_mapPlayerBotSidekicks.find(c->GetPlayerID()) != s_mapPlayerBotSidekicks.end() ||
					!IsPlayerBotVillageMap(c->GetMapIndex()))
				continue;
			char name[32];
			snprintf(name, sizeof(name), "TestKompan%u", (unsigned int)(dwNow % 1000));
			const int race = c->GetJob() == JOB_SHAMAN ? 0 : 7;
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test owner=%u name=%s level=%d", c->GetPlayerID(), c->GetName(),
					c->GetLevel());
			s_dwPlayerBotSidekickSelfTestOwner = c->GetPlayerID();
			s_dwPlayerBotSidekickSelfTestAt = dwNow;
			s_iPlayerBotSidekickSelfTestStep = 0;
			s_bPlayerBotSidekickSelfTestErrand = false;
			CreatePlayerBotSidekick(c, race, 1 + (int)(c->GetPlayerID() % 2), name);
			return;
		}
	}

	// Once a second for the whole core: the table, and every companion in or
	// out of the world by its owner's presence here.
	void ManagePlayerBotSidekicks(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotSidekickNextManage)
			return;
		s_dwPlayerBotSidekickNextManage = dwNow + 1000;
		if (dwNow >= s_dwPlayerBotSidekickNextLoad)
		{
			s_dwPlayerBotSidekickNextLoad = dwNow + PLAYERBOT_SIDEKICK_RELOAD_MS;
			LoadPlayerBotSidekicks();
		}
		RunPlayerBotSidekickSelfTest(dwNow);
		const bool off = IsPlayerBotSidekickSwitchedOff();
		for (TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.begin(); it != s_mapPlayerBotSidekicks.end(); ++it)
		{
			TPlayerBotSidekick& rec = it->second;
			LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
			const bool here = CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID);
			// Switched off for the world: the companion leaves as it leaves with
			// a gone owner, and nobody's is spawned. The record stays, so it
			// comes back as it was when the switch is on again.
			if (off)
			{
				if (here)
				{
					LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
					if (sk)
					{
						if (sk->GetParty())
							LeavePlayerBotParty(sk);
						sk->Save();
					}
					sys_log(0, "PLAYERBOT_SIDEKICK: companions switched off, logging out pid=%u owner=%u",
							rec.dwSidekickPID, rec.dwOwnerPID);
					CPlayerBotManager::instance().Despawn(rec.dwSidekickPID);
					s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
				}
				continue;
			}
			if (owner && owner->GetSectree())
			{
				rec.dwOwnerSeenAt = dwNow;
				if (!here && dwNow >= rec.dwNextSpawnTry)
				{
					rec.dwNextSpawnTry = dwNow + PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS;
					// Still in another core's world: that core lets it go once it
					// sees its owner gone, and a later try here spawns it.
					if (!P2P_MANAGER::instance().FindByPID(rec.dwSidekickPID) &&
							!CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID))
						CPlayerBotManager::instance().SpawnSidekick(rec.dwSidekickPID);
				}
			}
			// A record made in this very pass carries get_dword_time(), later
			// than dwNow: seen just now, not four billion milliseconds ago.
			else if (here && (rec.dwOwnerSeenAt == 0 ||
					(dwNow >= rec.dwOwnerSeenAt && dwNow - rec.dwOwnerSeenAt >= PLAYERBOT_SIDEKICK_OWNER_GONE_MS)))
			{
				LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
				if (sk)
				{
					if (sk->GetParty())
						LeavePlayerBotParty(sk);
					sk->Save();
				}
				sys_log(0, "PLAYERBOT_SIDEKICK: owner gone, logging out pid=%u owner=%u", rec.dwSidekickPID,
						rec.dwOwnerPID);
				CPlayerBotManager::instance().Despawn(rec.dwSidekickPID);
				s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
			}
		}
	}

	// ------------------------------------------------------------ the orders

	void ReportPlayerBotSidekick(LPCHARACTER owner, const TPlayerBotSidekick& rec)
	{
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		const char* mode = rec.bMode == PLAYERBOT_SIDEKICK_FREE ? "wolna reka" : "przy tobie";
		char text[320];
		if (!sk)
			snprintf(text, sizeof(text), "Twoj towarzysz za chwile bedzie w grze (tryb: %s, walka: %s).", mode,
					GetPlayerBotSidekickStanceName(rec.bStance));
		else
			snprintf(text, sizeof(text), "%s - poziom %d, zycie %d%%, %s, tryb: %s, walka: %s.",
					sk->GetName(), sk->GetLevel(),
					sk->GetMaxHP() > 0 ? (int)((long long)sk->GetHP() * 100 / sk->GetMaxHP()) : 0,
					sk->GetMapIndex() == owner->GetMapIndex() ? "na twojej mapie" : "na innej mapie", mode,
					GetPlayerBotSidekickStanceName(rec.bStance));
		SayPlayerBotSidekick(owner, text);
	}

	// The stance, kept in the record at once: the table is read again every
	// thirty seconds, and an order written behind that read would be undone by
	// it. Answers with the companion's words for it.
	const char* SetPlayerBotSidekickStance(TPlayerBotSidekick& rec, BYTE stance)
	{
		stance = std::min<BYTE>(stance, PLAYERBOT_SIDEKICK_STANCE_PASSIVE);
		if (rec.bStance != stance)
		{
			rec.bStance = stance;
			if (s_bPlayerBotSidekickSettingsColumns)
			{
				char query[160];
				snprintf(query, sizeof(query), "UPDATE player.playerbot_sidekick SET stance=%u WHERE owner_pid=%u",
						(unsigned int)stance, rec.dwOwnerPID);
				std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: stance owner=%u pid=%u stance=%u", rec.dwOwnerPID, rec.dwSidekickPID,
					(unsigned int)stance);
		}
		switch (stance)
		{
			case PLAYERBOT_SIDEKICK_STANCE_DEFEND:
				return "Dobra, nie zaczynam walki. Bronie ciebie i siebie, a pomagam, kiedy ty juz walczysz.";
			case PLAYERBOT_SIDEKICK_STANCE_PASSIVE:
				return "Dobra, nie walcze. Oddam tylko temu, kto mnie uderzy.";
			default:
				return "Dobra, bije wszystko, co sie do nas zblizy.";
		}
	}

	bool PlayerBotSidekickHeard(const char* folded, const char* phrase);

	// The stance a whisper asks for, or -1. The longer phrases first: "nie
	// atakuj pierwszy" holds "nie atakuj" and "atakuj pierwszy" both.
	int GetPlayerBotSidekickStanceHeard(const char* folded)
	{
		static const char* const defendWords[] = { "nie atakuj pierwszy", "nie bij pierwszy", "nie zaczynaj",
				"nie zaczepiaj", "bron mnie", "obronnie", "defensywnie" };
		static const char* const passiveWords[] = { "nie walcz", "nie atakuj", "nie bij", "tylko sie bron",
				"pasywnie", "biernie" };
		static const char* const attackWords[] = { "atakuj wszystko", "bij wszystko", "atakuj pierwszy", "atakuj",
				"agresywnie", "walcz" };
		for (size_t i = 0; i < sizeof(defendWords) / sizeof(defendWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, defendWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_DEFEND;
		for (size_t i = 0; i < sizeof(passiveWords) / sizeof(passiveWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, passiveWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_PASSIVE;
		for (size_t i = 0; i < sizeof(attackWords) / sizeof(attackWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, attackWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_ATTACK;
		return -1;
	}

	void SummonPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
		{
			rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
			DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=0 WHERE owner_pid=%u", rec.dwOwnerPID);
			sys_log(0, "PLAYERBOT_SIDEKICK: summoned back owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
		}
		// Waiting at a spot, or sent to town: the call ends either.
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		const bool wasOnErrand = rtIt != s_mapPlayerBotSidekickRuntime.end() && rtIt->second.bErrand;
		if (rtIt != s_mapPlayerBotSidekickRuntime.end())
		{
			rtIt->second.bHold = false;
			rtIt->second.bErrand = false;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		if (!sk || st == s_mapPlayerBotAIStates.end() || !owner->GetSectree())
		{
			SayPlayerBotSidekick(owner, "Juz ide - pojawie sie przy tobie za chwile.");
			return;
		}
		if (sk->IsDead())
		{
			SayPlayerBotSidekick(owner, "Najpierw musze wstac - zaraz bede.");
			return;
		}
		// Whatever it was doing on its own ends here.
		TPlayerBotAIState& state = st->second;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		state.bFishingSession = false;
		if (wasOnErrand)
			state.bVisitingShop = false;
		PlacePlayerBotSidekick(sk, state, owner, dwNow, "sidekick_summoned");
		KeepPlayerBotSidekickInParty(sk, owner, dwNow);
		char text[128];
		snprintf(text, sizeof(text), "%s: jestem!", sk->GetName());
		SayPlayerBotSidekick(owner, text);
	}

	void FreePlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec)
	{
		// Let off the leash in town, it goes on with the visit as its own.
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		if (rtIt != s_mapPlayerBotSidekickRuntime.end())
		{
			rtIt->second.bHold = false;
			rtIt->second.bErrand = false;
		}
		rec.bMode = PLAYERBOT_SIDEKICK_FREE;
		DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=1 WHERE owner_pid=%u", rec.dwOwnerPID);
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		if (sk && sk->GetParty() && owner->GetParty() == sk->GetParty())
			LeavePlayerBotParty(sk);
		SayPlayerBotSidekick(owner, "Ide expic po swojemu. Zawolaj mnie z listu Towarzysz, kiedy bede potrzebny.");
		sys_log(0, "PLAYERBOT_SIDEKICK: let off the leash owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
	}

	void DismissPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick rec)
	{
		DBManager::instance().Query("DELETE FROM player.playerbot_sidekick WHERE owner_pid=%u", rec.dwOwnerPID);
		DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_gift WHERE sidekick_pid=%u", rec.dwSidekickPID);
		if (s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE sidekick_pid=%u",
					rec.dwSidekickPID);
		s_mapPlayerBotSidekicks.erase(rec.dwOwnerPID);
		s_mapPlayerBotSidekickOwner.erase(rec.dwSidekickPID);
		s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
		SetPlayerBotSidekickFlag(rec.dwOwnerPID, "towarzysz.created", 0);
		if (CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID))
		{
			LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
			if (sk && sk->GetParty())
				LeavePlayerBotParty(sk);
			CPlayerBotManager::instance().Despawn(rec.dwSidekickPID);
		}
		SayPlayerBotSidekick(owner, "Towarzysz odszedl. Nowego mozesz wybrac w liscie Towarzysz.");
		sys_log(0, "PLAYERBOT_SIDEKICK: dismissed owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
	}

	// One of the window's switches, kept in the record at once and in the table
	// when it has the column (see SetPlayerBotSidekickStance for why at once).
	void SetPlayerBotSidekickSetting(const TPlayerBotSidekick& rec, const char* column, unsigned int value)
	{
		if (s_bPlayerBotSidekickSettingsColumns)
		{
			char query[192];
			snprintf(query, sizeof(query), "UPDATE player.playerbot_sidekick SET %s=%u WHERE owner_pid=%u", column, value,
					rec.dwOwnerPID);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: setting owner=%u pid=%u %s=%u", rec.dwOwnerPID, rec.dwSidekickPID, column, value);
	}

	// The companion in this core's world with its state, or NULL with the owner
	// told why not.
	LPCHARACTER FindPlayerBotSidekickForOrder(LPCHARACTER owner, const TPlayerBotSidekick& rec,
			TPlayerBotAIState** state)
	{
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		if (!sk || st == s_mapPlayerBotAIStates.end() || !CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID))
		{
			SayPlayerBotSidekick(owner, "Jeszcze mnie nie ma w grze - chwila.");
			return NULL;
		}
		if (sk->IsDead())
		{
			SayPlayerBotSidekick(owner, "Najpierw musze wstac.");
			return NULL;
		}
		*state = &st->second;
		return sk;
	}

	// "Czekaj tutaj": it stays where its owner stands now - put there first
	// when it is anywhere else - and keeps the spot until it is called.
	void HoldPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Jestem na zakupach. Zawolaj mnie, jesli mam wrocic wczesniej.");
			return;
		}
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
		{
			rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
			DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=0 WHERE owner_pid=%u", rec.dwOwnerPID);
		}
		if (!owner->GetSectree())
			return;
		if (sk->GetMapIndex() != owner->GetMapIndex() ||
				DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY()) > PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE)
		{
			PlacePlayerBotSidekick(sk, *state, owner, dwNow, "sidekick_hold");
			KeepPlayerBotSidekickInParty(sk, owner, dwNow);
		}
		rt.bHold = true;
		rt.lHoldMap = sk->GetMapIndex();
		rt.lHoldX = sk->GetX();
		rt.lHoldY = sk->GetY();
		SayPlayerBotSidekick(owner, "Czekam tutaj. Zawolaj mnie, kiedy bede potrzebny.");
		sys_log(0, "PLAYERBOT_SIDEKICK: holds pid=%u owner=%u map=%ld x=%ld y=%ld", rec.dwSidekickPID, rec.dwOwnerPID,
				rt.lHoldMap, rt.lHoldX, rt.lHoldY);
	}

	// Back from town - done, called off, or with nothing to do - beside the
	// owner, with what it spent and what it carries.
	void EndPlayerBotSidekickErrand(LPCHARACTER sk, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow, const char* why)
	{
		const long long spent = rt.llErrandGold - (long long)sk->GetGold();
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(sk, red, blue);
		const bool visited = rt.bErrandVisit;
		rt.bErrand = false;
		rt.bErrandVisit = false;
		state.bVisitingShop = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		state.bFishingSession = false;
		char text[192];
		if (!visited)
			snprintf(text, sizeof(text), "Nie mialem nic do zalatwienia w miescie - wracam.");
		else
			snprintf(text, sizeof(text), "Wracam z zakupow: wydalem %lld yang, mam %u czerwonych i %u niebieskich mikstur.",
					spent > 0 ? spent : 0LL, (unsigned int)red, (unsigned int)blue);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (owner)
		{
			SayPlayerBotSidekick(owner, text);
			if (owner->GetSectree() && !sk->IsDead())
			{
				PlacePlayerBotSidekick(sk, state, owner, dwNow, "sidekick_errand_back");
				KeepPlayerBotSidekickInParty(sk, owner, dwNow);
			}
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: errand over pid=%u owner=%u why=%s visited=%d spent=%lld red=%u blue=%u",
				rec.dwSidekickPID, rec.dwOwnerPID, why, visited ? 1 : 0, spent, (unsigned int)red, (unsigned int)blue);
	}

	// "Idz na zakupy": at its owner's side it never goes to town by itself, so
	// its potions run out and its junk fills the bag. This takes it to its own
	// kingdom's first village, where every service stands, and runs the town
	// visit any bot runs - the merchant, the potions, the blacksmith, the
	// storekeeper, whatever it needs - then brings it back
	// (ManagePlayerBotSidekickErrand).
	void SendPlayerBotSidekickShopping(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Juz jestem na zakupach.");
			return;
		}
		if (rec.bMode == PLAYERBOT_SIDEKICK_FREE)
		{
			SayPlayerBotSidekick(owner, "Gram teraz po swojemu, zakupy robie sam. Zawolaj mnie najpierw.");
			return;
		}
		long map = 0, x = 0, y = 0;
		if (!GetPlayerBotVillageReturn(sk, playerbot_empire_rules::MAP_ROLE_M1, map, x, y) ||
				!IsPlayerBotMapHostedHere(map))
		{
			SayPlayerBotSidekick(owner, "Stad nie dojde do swojego miasta.");
			return;
		}
		rt.bHold = false;
		if (sk->GetParty())
			LeavePlayerBotParty(sk);
		if (sk->GetMapIndex() != map && !TransitionPlayerBotMap(sk, *state, map, x, y, dwNow, "sidekick_errand"))
		{
			KeepPlayerBotSidekickInParty(sk, owner, dwNow);
			SayPlayerBotSidekick(owner, "Nie udalo mi sie dojsc do miasta.");
			return;
		}
		rt.bErrand = true;
		rt.bErrandVisit = false;
		rt.dwErrandSince = dwNow;
		rt.llErrandGold = (long long)sk->GetGold();
		state->bVisitingShop = false;
		state->dwNextShopCheckTime = 0;
		StartPlayerBotTownVisit(sk, *state, dwNow);
		rt.bErrandVisit = state->bVisitingShop;
		++s_uPlayerBotSidekickErrands;
		sys_log(0, "PLAYERBOT_SIDEKICK: errand pid=%u owner=%u map=%ld visit=%d gold=%lld", rec.dwSidekickPID,
				rec.dwOwnerPID, map, rt.bErrandVisit ? 1 : 0, rt.llErrandGold);
		if (!rt.bErrandVisit)
		{
			EndPlayerBotSidekickErrand(sk, *state, rec, rt, dwNow, "nothing");
			return;
		}
		char text[160];
		snprintf(text, sizeof(text), "Ide na zakupy %s. Wroce, jak skoncze.", playerbot_conv::GetMapWords(map).to);
		SayPlayerBotSidekick(owner, text);
	}

	// ------------------------------------------------------------ the window

	// A text for the window: hex of its bytes, "-" for none. The client splits
	// a command on its spaces, and names have them.
	std::string EncodePlayerBotSidekickText(const char* text, size_t maxBytes)
	{
		static const char kDigits[] = "0123456789abcdef";
		std::string out;
		size_t n = 0;
		for (const unsigned char* p = (const unsigned char*)(text ? text : ""); *p && n < maxBytes; ++p, ++n)
		{
			const unsigned char c = (*p < 32 || *p == 127) ? '?' : *p;
			out += kDigits[c >> 4];
			out += kDigits[c & 15];
		}
		return out.empty() ? std::string("-") : out;
	}

	// Every command to the window goes through here: refused rather than cut
	// when it would not fit CHARACTER::ChatPacket's buffer, and written to the
	// log under the self-test, whose owner is a bot with no client to see it.
	void SendPlayerBotSidekickCommand(LPCHARACTER owner, const char* format, ...)
	{
		char text[CHAT_MAX_LEN + 1];
		va_list args;
		va_start(args, format);
		const int len = vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		if (len < 0 || len >= (int)sizeof(text))
		{
			sys_err("PLAYERBOT_SIDEKICK: window command too long (%d) for %s", len, owner->GetName());
			return;
		}
		owner->ChatPacket(CHAT_TYPE_COMMAND, "%s", text);
		if (s_bPlayerBotSidekickSelfTest)
			sys_log(0, "PLAYERBOT_SIDEKICK: window %s", text);
	}

	// What the window says it is doing.
	void DescribePlayerBotSidekickDoing(LPCHARACTER sk, const TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			const TPlayerBotSidekickRuntime* rt, char* out, size_t size)
	{
		if (sk->IsDead())
		{
			snprintf(out, size, "lezy - zaraz wstanie");
			return;
		}
		if (rt && rt->bErrand)
		{
			snprintf(out, size, "robi zakupy %s", playerbot_conv::GetMapWords(sk->GetMapIndex()).at);
			return;
		}
		if (rec.bMode == PLAYERBOT_SIDEKICK_FREE)
		{
			char status[128] = "";
			BuildPlayerBotStatusText(sk, state, status, sizeof(status), false);
			const char* text = status;
			if (!strncmp(text, "[PT] ", 5))
				text += 5;
			snprintf(out, size, "%s", *text ? text : "gra po swojemu");
			return;
		}
		const char* what = "stoi przy tobie";
		if (state.bCurrentAction == BOT_ACTION_FIGHT)
			what = "walczy";
		else if (state.bCurrentAction == BOT_ACTION_LOOT)
			what = "zbiera drop";
		else if (state.bCurrentAction == BOT_ACTION_REFINE)
			what = "ulepsza u kowala";
		else if (state.bCurrentAction == BOT_ACTION_SHOP)
			what = "u handlarza";
		else if (state.bRecoveringAfterDeath)
			what = "wraca do sil";
		else if (rt && rt->bHold)
			what = "czeka w miejscu";
		else if (state.bCurrentAction == BOT_ACTION_TRAVEL)
			what = "idzie do ciebie";
		snprintf(out, size, "%s", what);
	}

	// The window's snapshot, in three kinds of command, each far under the
	// 512 bytes CHARACTER::ChatPacket formats into (and past which it would
	// read beyond its buffer): the numbers, the three texts, and the worn
	// gear piece by piece, only when it changed unless the window asks for all
	// of it (it does when it opens).
	void SendPlayerBotSidekickWindow(LPCHARACTER owner, bool fullGear)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (it == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickInfo %d 0", PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL);
			return;
		}
		const TPlayerBotSidekick& rec = it->second;
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		TPlayerBotSidekickRuntime* rt = rtIt == s_mapPlayerBotSidekickRuntime.end() ? NULL : &rtIt->second;
		const bool inWorld = sk && st != s_mapPlayerBotAIStates.end() &&
				CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID);
		int where = 0;
		long dist = 0;
		int expPercent = 0;
		size_t red = 0, blue = 0;
		if (inWorld)
		{
			where = sk->GetMapIndex() == owner->GetMapIndex() ? 1 : 2;
			if (where == 1)
				dist = DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY());
			const long long next = (long long)sk->GetNextExp();
			if (next > 0)
				expPercent = (int)MINMAX(0LL, (long long)sk->GetExp() * 100 / next, 100LL);
			CountPlayerBotPotions(sk, red, blue);
		}
		int mode = rec.bMode == PLAYERBOT_SIDEKICK_FREE ? 1 : 0;
		if (rt && rt->bErrand)
			mode = 3;
		else if (rt && rt->bHold && mode == 0)
			mode = 2;
		SendPlayerBotSidekickCommand(owner,
				"SidekickInfo %d 1 %d %d %d %d %d %d %d %d %d %ld %d %u %u %d %d %lld %u %u %d",
				PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL,
				inWorld ? (int)sk->GetRaceNum() : -1, inWorld ? (int)sk->GetSkillGroup() : 0,
				inWorld ? sk->GetLevel() : 0, expPercent,
				inWorld ? (int)sk->GetHP() : 0, inWorld ? (int)sk->GetMaxHP() : 0,
				inWorld ? (int)sk->GetSP() : 0, inWorld ? (int)sk->GetMaxSP() : 0,
				where, dist, mode, (unsigned int)rec.bStance, (unsigned int)rec.bLoot, rec.bProtect ? 1 : 0,
				rec.bBuffs ? 1 : 0, inWorld ? (long long)sk->GetGold() : 0LL, (unsigned int)red, (unsigned int)blue,
				inWorld && sk->IsDead() ? 1 : 0);
		char doing[96] = "";
		char place[64] = "";
		if (inWorld)
		{
			DescribePlayerBotSidekickDoing(sk, st->second, rec, rt, doing, sizeof(doing));
			long map = sk->GetMapIndex();
			if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
				map /= 10000;
			const playerbot_conv::TMapWords& words = playerbot_conv::GetMapWords(map);
			if (*words.name)
				snprintf(place, sizeof(place), "%s", words.name);
			else
				snprintf(place, sizeof(place), "mapa %ld", map);
		}
		else
			snprintf(doing, sizeof(doing), "za chwile bedzie w grze");
		SendPlayerBotSidekickCommand(owner, "SidekickNames %s %s %s",
				EncodePlayerBotSidekickText(inWorld ? sk->GetName() : "", 24).c_str(),
				EncodePlayerBotSidekickText(place, 40).c_str(),
				EncodePlayerBotSidekickText(doing, 60).c_str());
		if (!inWorld)
			return;
		static const BYTE kWear[8] = { WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_FOOTS, WEAR_WRIST,
				WEAR_NECK, WEAR_EAR };
		DWORD hash = 2166136261U;
		for (int i = 0; i < 8; ++i)
		{
			LPITEM item = sk->GetWear(kWear[i]);
			hash = (hash ^ (item ? item->GetID() : 0U)) * 16777619U;
			hash = (hash ^ (item ? item->GetVnum() : 0U)) * 16777619U;
		}
		if (!fullGear && rt && rt->dwGearSent == hash)
			return;
		for (int i = 0; i < 8; ++i)
		{
			LPITEM item = sk->GetWear(kWear[i]);
			SendPlayerBotSidekickCommand(owner, "SidekickGear %d %s", i,
					EncodePlayerBotSidekickText(item ? item->GetName() : "", 40).c_str());
		}
		if (rt)
			rt->dwGearSent = hash;
	}

	// ------------------------------------------------ its bag and skills, in a window
	//
	// The companion's bag and worn gear in a window of the owner's client
	// (uisidekickinventory.py; Tieru, 25 September: "widoczny jej ekwipunek, ze
	// mozesz itemy przenosic normalnie dowoli"), and its skills beside it. What
	// the owner does there is the owner's word over the AI's:
	// - a piece the owner puts on is pinned (IsPlayerBotSidekickPinned): the
	//   equipment pass never takes it off or ranks anything against it, puts it
	//   back first when something else took it off, the blacksmith does not
	//   refine it and no change stone touches its lines (an added line loses
	//   nothing, so the add stone still may) - the bell with +12 INT that the
	//   equipment pass kept in the bag for a heavier one (GoracyDelfin, 25
	//   September);
	// - a piece the owner takes off is never put back on by the AI
	//   (IsPlayerBotSidekickUnwanted), or the next equipment pass would undo
	//   the owner's hand;
	// - what crosses between the two bags crosses the way a trade moves it
	//   (CExchange::Done), with a trade's refusals: nothing ITEM_ANTIFLAG_GIVE,
	//   nothing locked or in a trade, and a window that is busy says so.
	// The positions are the protocol's: a bag cell, or 1000 + WEAR_* for a worn
	// piece; -1 is "wherever it fits".

	bool IsPlayerBotSidekickEqBagPos(int pos)
	{
		return pos >= 0 && pos < PLAYERBOT_BAG_CELLS;
	}

	bool IsPlayerBotSidekickEqWearPos(int pos)
	{
		return pos >= PLAYERBOT_SIDEKICK_EQ_WEAR_BASE && pos < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_MAX_NUM;
	}

	// A position as the window writes it; INT_MIN for nothing a window writes.
	int ParsePlayerBotSidekickEqPos(const char* text)
	{
		if (!text || !*text)
			return INT_MIN;
		for (const char* p = text + (*text == '-' ? 1 : 0); *p; ++p)
			if (!isdigit((unsigned char)*p))
				return INT_MIN;
		int pos = INT_MIN;
		str_to_number(pos, text);
		if (pos == -1 || IsPlayerBotSidekickEqBagPos(pos) || IsPlayerBotSidekickEqWearPos(pos))
			return pos;
		return INT_MIN;
	}

	void SetPlayerBotSidekickPin(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId, BYTE wear)
	{
		// One piece a slot: a new one there frees the one pinned before.
		if (wear != PLAYERBOT_SIDEKICK_PIN_UNWANTED)
			for (std::map<DWORD, BYTE>::iterator it = rt.mapPins.begin(); it != rt.mapPins.end();)
			{
				if (it->first != itemId && it->second == wear)
				{
					if (s_bPlayerBotSidekickPinTable)
						DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE item_id=%u", it->first);
					rt.mapPins.erase(it++);
				}
				else
					++it;
			}
		rt.mapPins[itemId] = wear;
		if (s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("REPLACE INTO player.playerbot_sidekick_pin (item_id, sidekick_pid, wear, pinned_at) "
					"VALUES (%u, %u, %u, NOW())", itemId, sidekickPid, (unsigned int)wear);
	}

	void ClearPlayerBotSidekickPin(TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		if (rt.mapPins.erase(itemId) && s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE item_id=%u", itemId);
	}

	void AddPlayerBotSidekickGift(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		if (rt.setGifts.insert(itemId).second)
			DBManager::instance().Query("INSERT IGNORE INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at) "
					"VALUES (%u, %u, NOW())", itemId, sidekickPid);
	}

	void ClearPlayerBotSidekickGift(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		if (rt.setGifts.erase(itemId))
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_gift WHERE item_id=%u AND sidekick_pid=%u",
					itemId, sidekickPid);
	}

	// The window's marks on a piece: 1 put on by the owner, 2 the owner's gift,
	// 4 taken off by the owner.
	int GetPlayerBotSidekickEqFlags(const TPlayerBotSidekickRuntime& rt, LPITEM item)
	{
		int flags = 0;
		std::map<DWORD, BYTE>::const_iterator pin = rt.mapPins.find(item->GetID());
		if (pin != rt.mapPins.end())
			flags |= pin->second == PLAYERBOT_SIDEKICK_PIN_UNWANTED ? 4 : 1;
		if (rt.setGifts.find(item->GetID()) != rt.setGifts.end())
			flags |= 2;
		return flags;
	}

	DWORD HashPlayerBotSidekickEqItem(LPITEM item, int flags)
	{
		DWORD hash = 2166136261U;
		const DWORD parts[4] = { item->GetID(), item->GetVnum(), (DWORD)item->GetCount(), (DWORD)flags };
		for (int i = 0; i < 4; ++i)
			hash = (hash ^ parts[i]) * 16777619U;
		for (int i = 0; i < 3 && i < ITEM_SOCKET_MAX_NUM; ++i)
			hash = (hash ^ (DWORD)item->GetSocket(i)) * 16777619U;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			hash = (hash ^ (DWORD)item->GetAttributeType(i)) * 16777619U;
			hash = (hash ^ (DWORD)item->GetAttributeValue(i)) * 16777619U;
		}
		return hash == 0 ? 1U : hash;
	}

	void SendPlayerBotSidekickEqItem(LPCHARACTER owner, DWORD gen, int pos, LPITEM item, int flags)
	{
		char attrs[192] = "-";
		bool any = false;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0)
				any = true;
		if (any)
		{
			size_t n = 0;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM && n + 24 < sizeof(attrs); ++i)
				n += snprintf(attrs + n, sizeof(attrs) - n, "%s%u:%d", i ? "," : "",
						(unsigned int)item->GetAttributeType(i), (int)item->GetAttributeValue(i));
		}
		long sockets[3] = { 0, 0, 0 };
		for (int i = 0; i < 3 && i < ITEM_SOCKET_MAX_NUM; ++i)
			sockets[i] = (long)item->GetSocket(i);
		SendPlayerBotSidekickCommand(owner, "SidekickEqItem %u %d %u %u %d %ld %ld %ld %s", gen, pos, item->GetVnum(),
				(unsigned int)item->GetCount(), flags, sockets[0], sockets[1], sockets[2], attrs);
	}

	struct TPlayerBotSidekickEqEntry
	{
		LPITEM item;
		int flags;
		DWORD hash;
	};

	// The bag and the worn gear as the window is sent them: the whole picture
	// when it opens (or after the companion came back into the world), and
	// after that only what changed, nothing at all when nothing did. What the
	// window was sent is remembered by position (mapEqSent).
	void SendPlayerBotSidekickEq(LPCHARACTER owner, bool full)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		if (IsPlayerBotSidekickSwitchedOff())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 2", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (it == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(it->second.dwSidekickPID);
		if (!sk || !sk->IsItemLoaded() || !CPlayerBotManager::instance().IsManaged(it->second.dwSidekickPID))
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
		std::map<int, TPlayerBotSidekickEqEntry> now;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = sk->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			TPlayerBotSidekickEqEntry& e = now[(int)cell];
			e.item = item;
			e.flags = GetPlayerBotSidekickEqFlags(rt, item);
			e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
		}
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM item = sk->GetWear(wear);
			if (!item)
				continue;
			TPlayerBotSidekickEqEntry& e = now[PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear];
			e.item = item;
			e.flags = GetPlayerBotSidekickEqFlags(rt, item);
			e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
		}
		const long long gold = (long long)sk->GetGold();
		const bool begin = full || rt.dwEqGen == 0;
		if (!begin)
		{
			bool changed = gold != rt.llEqGoldSent || now.size() != rt.mapEqSent.size();
			for (std::map<int, TPlayerBotSidekickEqEntry>::const_iterator e = now.begin(); !changed && e != now.end(); ++e)
			{
				std::map<int, DWORD>::const_iterator sent = rt.mapEqSent.find(e->first);
				changed = sent == rt.mapEqSent.end() || sent->second != e->second.hash;
			}
			if (!changed)
				return;
		}
		++rt.dwEqGen;
		if (begin)
		{
			rt.mapEqSent.clear();
			SendPlayerBotSidekickCommand(owner, "SidekickEqBegin %d %u %d %d", PLAYERBOT_SIDEKICK_EQ_PROTOCOL, rt.dwEqGen,
					PLAYERBOT_BAG_CELLS, PLAYERBOT_SIDEKICK_EQ_PAGE_CELLS);
		}
		size_t lines = 0;
		for (std::map<int, DWORD>::iterator sent = rt.mapEqSent.begin(); sent != rt.mapEqSent.end();)
		{
			if (now.find(sent->first) != now.end())
			{
				++sent;
				continue;
			}
			if (lines >= PLAYERBOT_SIDEKICK_EQ_MAX_LINES)
				break;
			SendPlayerBotSidekickCommand(owner, "SidekickEqEmpty %u %d", rt.dwEqGen, sent->first);
			++lines;
			rt.mapEqSent.erase(sent++);
		}
		for (std::map<int, TPlayerBotSidekickEqEntry>::const_iterator e = now.begin(); e != now.end(); ++e)
		{
			std::map<int, DWORD>::iterator sent = rt.mapEqSent.find(e->first);
			if (sent != rt.mapEqSent.end() && sent->second == e->second.hash)
				continue;
			// What does not fit this answer goes with the next, which the window
			// asks for in a second and a half.
			if (lines >= PLAYERBOT_SIDEKICK_EQ_MAX_LINES)
				break;
			SendPlayerBotSidekickEqItem(owner, rt.dwEqGen, e->first, e->second.item, e->second.flags);
			++lines;
			rt.mapEqSent[e->first] = e->second.hash;
		}
		SendPlayerBotSidekickCommand(owner, "SidekickEqEnd %u %lld", rt.dwEqGen, gold);
		rt.llEqGoldSent = gold;
	}

	void AnswerPlayerBotSidekickEq(LPCHARACTER owner, int code, const char* text)
	{
		SendPlayerBotSidekickCommand(owner, "SidekickEqResult %d %s", code,
				EncodePlayerBotSidekickText(text, 150).c_str());
	}

	// Why the companion cannot wear a piece at all - its type, a class, a sex,
	// a level or a stat the item asks for, a unique of a group it already
	// wears - in the owner's words; empty when it can, whatever the moment
	// says (a blow a second ago). The engine says it to the companion's chat,
	// which nobody reads. replacing: the piece a unique would take the place
	// of, which is no obstacle.
	std::string GetPlayerBotSidekickWearRefusal(LPCHARACTER sk, LPITEM item, LPITEM replacing)
	{
		if (!item->IsEquipable() || item->IsDragonSoul() || item->FindEquipCell(sk) < 0)
			return "Tego nie da sie zalozyc.";
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (item->GetType() == ITEM_COSTUME && item->GetSubType() != COSTUME_HAIR)
			return "Kostiumy sa na tym serwerze wylaczone.";
#endif
		if (!item->CanUsedBy(sk))
			return "To nie jest dla klasy towarzysza.";
		if ((IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE) && GET_SEX(sk) == SEX_MALE) ||
				(IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE) && GET_SEX(sk) == SEX_FEMALE))
			return "To nie jest dla plci towarzysza.";
		char text[128];
		const TItemTable* proto = item->GetProto();
		for (int i = 0; proto && i < ITEM_LIMIT_MAX_NUM; ++i)
		{
			const long limit = proto->aLimits[i].lValue;
			const char* what = NULL;
			switch (proto->aLimits[i].bType)
			{
				case LIMIT_LEVEL:
					if (sk->GetLevel() < limit)
					{
						snprintf(text, sizeof(text), "Towarzysz ma za niski poziom (trzeba %ld, ma %d).", limit,
								sk->GetLevel());
						return text;
					}
					break;
				case LIMIT_STR: if (sk->GetPoint(POINT_ST) < limit) what = "sily"; break;
				case LIMIT_INT: if (sk->GetPoint(POINT_IQ) < limit) what = "inteligencji"; break;
				case LIMIT_DEX: if (sk->GetPoint(POINT_DX) < limit) what = "zrecznosci"; break;
				case LIMIT_CON: if (sk->GetPoint(POINT_HT) < limit) what = "witalnosci"; break;
			}
			if (what)
			{
				snprintf(text, sizeof(text), "Towarzysz ma za malo %s (trzeba %ld).", what, limit);
				return text;
			}
		}
		if (item->GetWearFlag() & WEARABLE_UNIQUE)
			for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2; ++wear)
			{
				LPITEM worn = sk->GetWear(wear);
				if (worn && worn != item && worn != replacing && worn->IsSameSpecialGroup(item))
					return "Towarzysz nosi juz cos z tej samej grupy.";
			}
		return std::string();
	}

	// Puts a piece of the companion's bag on for its owner, and pins it there.
	// A piece the engine will not let on for the moment - a blow or a skill in
	// the last second and a half, which it asks of every equip - is pinned all
	// the same and goes on at the equipment pass's first chance, the fight held
	// off for it (dwEquipWaitUntil).
	int EquipPlayerBotSidekickForOwner(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotAIState& state,
			TPlayerBotSidekickRuntime& rt, LPITEM item, int wantWear, std::string& answer)
	{
		if (item->IsEquipped())
		{
			answer = "Towarzysz juz to nosi.";
			return 0;
		}
		const int wear = item->FindEquipCell(sk);
		const bool unique = wear == WEAR_UNIQUE1 || wear == WEAR_UNIQUE2;
		const bool uniqueSlot = wantWear == WEAR_UNIQUE1 || wantWear == WEAR_UNIQUE2;
		if (wantWear >= 0 && wear >= 0 && wear != wantWear && !(unique && uniqueSlot))
		{
			answer = "To nie pasuje w to miejsce.";
			return 2;
		}
		const int slot = unique && uniqueSlot ? wantWear : wear;
		LPITEM old = slot >= 0 ? sk->GetWear(slot) : NULL;
		const std::string refusal = GetPlayerBotSidekickWearRefusal(sk, item, old);
		if (!refusal.empty())
		{
			answer = refusal;
			return 2;
		}
		if (old && IS_SET(old->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			answer = "Tego, co tam nosi, nie da sie zdjac.";
			return 2;
		}
		SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, item->GetID(), (BYTE)slot);
		const DWORD now = get_dword_time();
		const bool blowFresh = now - sk->GetLastAttackTime() <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY ||
				now - state.dwLastBotSkillTime <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY;
		bool worn = false;
		if (!blowFresh && !item->isLocked() && !item->IsExchanging())
		{
			// A ring or a glove goes where the owner put it: the engine takes the
			// first free unique slot whatever it is asked (CItem::FindEquipCell),
			// so the slot asked for is emptied first.
			if (unique && old && old->IsEquipped() && sk->GetEmptyInventory(old->GetSize()) >= 0)
				sk->UnequipItem(old);
			// Anything else: the engine swaps a worn piece into the new one's
			// cell when it fits there; one that does not goes to a free cell.
			worn = PlayerBotEquipItem(sk, item) && item->IsEquipped();
			if (!worn && old && old->IsEquipped() && sk->GetEmptyInventory(old->GetSize()) >= 0 &&
					sk->UnequipItem(old) && !old->IsEquipped())
				worn = PlayerBotEquipItem(sk, item) && item->IsEquipped();
		}
		if (worn)
		{
			// Pinned where it went: a unique finds its own slot.
			const int wentTo = (int)item->GetCell() - INVENTORY_MAX_NUM;
			if (wentTo >= 0 && wentTo < WEAR_MAX_NUM && wentTo != slot)
				SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, item->GetID(), (BYTE)wentTo);
			LogManager::instance().ItemLog(sk, item, "PLAYERBOT_SIDEKICK_WEAR", owner->GetName());
			FlushPlayerBotItemRow(item);
			FlushPlayerBotItemRow(old);
			answer = "Zalozone. Tego towarzysz sam nie zdejmie.";
			return 0;
		}
		rt.dwEquipWaitUntil = now + PLAYERBOT_SIDEKICK_EQUIP_WAIT_MS;
		state.bEquipPending = true;
		state.dwNextEquipmentCheckTime = 0;
		answer = "Zalozy to, jak tylko skonczy cios.";
		return 0;
	}

	// Takes a worn piece off for its owner, to a cell or to the first that
	// fits, and marks it: the AI does not put it back on.
	int UnequipPlayerBotSidekickForOwner(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int wear,
			int toCell, std::string& answer)
	{
		LPITEM worn = sk->GetWear(wear);
		if (!worn)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE) || worn->isLocked())
		{
			answer = "Tego nie da sie zdjac.";
			return 2;
		}
		bool done = false;
		if (toCell >= 0)
			done = sk->MoveItem(TItemPos(INVENTORY, INVENTORY_MAX_NUM + wear), TItemPos(INVENTORY, toCell),
					worn->GetCount()) && !worn->IsEquipped();
		else
			done = sk->GetEmptyInventory(worn->GetSize()) >= 0 && sk->UnequipItem(worn) && !worn->IsEquipped();
		if (!done)
		{
			answer = sk->GetEmptyInventory(worn->GetSize()) < 0 ? "Towarzysz nie ma miejsca w torbie." :
					"Nie da sie tego teraz zdjac - sprobuj za chwile.";
			return 2;
		}
		SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, worn->GetID(), PLAYERBOT_SIDEKICK_PIN_UNWANTED);
		LogManager::instance().ItemLog(sk, worn, "PLAYERBOT_SIDEKICK_UNWEAR", owner->GetName());
		FlushPlayerBotItemRow(worn);
		answer = "Zdjete. Towarzysz sam tego nie zalozy (odepnij, zeby znow mogl).";
		return 0;
	}

	// A move inside its bag, as the owner's own bag moves: onto an empty place,
	// into a stack of the same thing, or - which the engine's own move does
	// not do - swapped with a piece of the same size lying there.
	int MovePlayerBotSidekickBagItem(LPCHARACTER sk, int from, int to, std::string& answer)
	{
		LPITEM item = sk->GetInventoryItem(from);
		if (!item || item->GetCell() != from)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (from == to)
			return 0;
		if (item->isLocked() || item->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		LPITEM other = sk->GetInventoryItem(to);
		const bool sameStack = other && other != item && other->GetVnum() == item->GetVnum() && item->IsStackable();
		if (other && other != item && other->GetCell() == to && !sameStack && other->GetSize() == item->GetSize())
		{
			if (other->isLocked() || other->IsExchanging())
			{
				answer = "Ten przedmiot jest teraz zajety.";
				return 2;
			}
			item->RemoveFromCharacter();
			other->RemoveFromCharacter();
			item->AddToCharacter(sk, TItemPos(INVENTORY, to));
			other->AddToCharacter(sk, TItemPos(INVENTORY, from));
			answer = "Zamienione miejscami.";
			return 0;
		}
		const DWORD before = sameStack ? (DWORD)other->GetCount() : 0;
		if (!sk->MoveItem(TItemPos(INVENTORY, from), TItemPos(INVENTORY, to), item->GetCount()))
		{
			answer = "Tam nie ma miejsca.";
			return 2;
		}
		if (sameStack && (DWORD)other->GetCount() == before)
		{
			answer = "Ten stos jest juz pelny.";
			return 2;
		}
		answer = sameStack ? "Polaczone." : "Przeniesione.";
		return 0;
	}

	// A stack dropped on a stack of the same thing, poured the way the
	// engine's own move pours one: the same vnum and sockets, up to the item's
	// own ceiling. The units moved; -1 when the two are not one thing. A
	// source emptied is removed, with its quickslot.
	int PourPlayerBotSidekickStack(LPCHARACTER from, LPITEM item, LPITEM into, const char* why)
	{
		if (!into || into == item || into->GetVnum() != item->GetVnum() || !into->IsStackable() ||
				IS_SET(into->GetAntiFlag(), ITEM_ANTIFLAG_STACK) || into->isLocked() || into->IsExchanging())
			return -1;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (into->GetSocket(i) != item->GetSocket(i))
				return -1;
		const int room = PlayerBotMaxStack(into) - (int)into->GetCount();
		const int moved = std::min(room, (int)item->GetCount());
		if (moved <= 0)
			return 0;
		into->SetCount(into->GetCount() + moved);
		if (moved >= (int)item->GetCount())
		{
			from->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
			ITEM_MANAGER::instance().RemoveItem(item, why);
		}
		else
			item->SetCount(item->GetCount() - moved);
		return moved;
	}

	// Why a piece may not cross between the two bags, in the owner's words;
	// empty when it may. A trade's own refusals.
	std::string GetPlayerBotSidekickHandOverRefusal(LPITEM item)
	{
		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
			return "Tego nie da sie przekazac (tak jak w handlu).";
		if (item->isLocked() || item->IsExchanging())
			return "Ten przedmiot jest teraz zajety.";
		if (item->IsDragonSoul())
			return "Kamieni smoka nie da sie tu przekazac.";
		return std::string();
	}

	// Moves a piece from one character's bag to a cell of the other's, as
	// CExchange::Done does.
	void HandPlayerBotSidekickItemOver(LPCHARACTER from, LPCHARACTER to, LPITEM item, int cell)
	{
		from->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
		item->RemoveFromCharacter();
		item->AddToCharacter(to, TItemPos(INVENTORY, cell));
		ITEM_MANAGER::instance().FlushDelayedSave(item);
	}

	// Where a piece of this size goes in a character's bag: the cell asked
	// for when it is free, the first free one for -1. -1 when there is none.
	int FindPlayerBotSidekickHandOverCell(LPCHARACTER ch, LPITEM item, int cell)
	{
		if (cell < 0)
			return ch->GetEmptyInventory(item->GetSize());
		return ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), item->GetSize()) ? cell : -1;
	}

	// From the owner's bag to its companion: onto a cell, a stack or a slot.
	// What it gives is its gift (never sold, IsPlayerBotSidekickGift); what it
	// gives straight onto a slot is put on and pinned.
	int GivePlayerBotSidekickItem(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotAIState& state,
			TPlayerBotSidekickRuntime& rt, int fromCell, int to, std::string& answer)
	{
		LPITEM item = IsPlayerBotSidekickEqBagPos(fromCell) ? owner->GetInventoryItem(fromCell) : NULL;
		if (!item || item->GetCell() != fromCell || item->GetWindow() != INVENTORY || item->IsEquipped())
		{
			answer = "Nie ma tego w twojej torbie.";
			return 3;
		}
		answer = GetPlayerBotSidekickHandOverRefusal(item);
		if (!answer.empty())
			return 2;
		const bool toWear = IsPlayerBotSidekickEqWearPos(to);
		const int wantWear = toWear ? to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE : -1;
		if (toWear)
		{
			// What the companion could never wear stays with the owner.
			const int wear = item->FindEquipCell(sk);
			const bool unique = (wear == WEAR_UNIQUE1 || wear == WEAR_UNIQUE2) &&
					(wantWear == WEAR_UNIQUE1 || wantWear == WEAR_UNIQUE2);
			if (wear >= 0 && wear != wantWear && !unique)
			{
				answer = "To nie pasuje w to miejsce.";
				return 2;
			}
			answer = GetPlayerBotSidekickWearRefusal(sk, item, sk->GetWear(wantWear));
			if (!answer.empty())
				return 2;
		}
		const char* name = item->GetName();
		std::string pieceName = name ? name : "";
		const DWORD vnum = item->GetVnum();
		// Onto a stack of the same thing, as far as it takes.
		LPITEM stack = IsPlayerBotSidekickEqBagPos(to) ? sk->GetInventoryItem(to) : NULL;
		if (stack && stack->GetCell() == to)
		{
			const DWORD count = (DWORD)item->GetCount();
			const int poured = PourPlayerBotSidekickStack(owner, item, stack, "PLAYERBOT_SIDEKICK_GIVE");
			if (poured > 0)
			{
				AddPlayerBotSidekickGift(sk->GetPlayerID(), rt, stack->GetID());
				LogManager::instance().ItemLog(sk, stack, "PLAYERBOT_GIFT_IN", owner->GetName());
				char text[160];
				snprintf(text, sizeof(text), "Dolozone do stosu towarzysza: %d z %u.", poured, (unsigned int)count);
				answer = text;
				return 0;
			}
			if (poured == 0)
			{
				answer = "Ten stos jest juz pelny.";
				return 2;
			}
		}
		const int cell = FindPlayerBotSidekickHandOverCell(sk, item, IsPlayerBotSidekickEqBagPos(to) ? to : -1);
		if (cell < 0)
		{
			answer = IsPlayerBotSidekickEqBagPos(to) ? "Tam nie ma miejsca." : "Towarzysz nie ma miejsca w torbie.";
			return 2;
		}
		HandPlayerBotSidekickItemOver(owner, sk, item, cell);
		AddPlayerBotSidekickGift(sk->GetPlayerID(), rt, item->GetID());
		ClearPlayerBotSidekickPin(rt, item->GetID());
		LogManager::instance().ItemLog(sk, item, "PLAYERBOT_GIFT_IN", owner->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: given pid=%u owner=%u item=%u vnum=%u cell=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), item->GetID(), vnum, cell);
		if (toWear)
		{
			std::string worn;
			const int code = EquipPlayerBotSidekickForOwner(owner, sk, state, rt, item, wantWear, worn);
			answer = std::string("Dane: ") + pieceName + ". " + worn;
			return code;
		}
		state.dwNextEquipmentCheckTime = 0;
		answer = std::string("Dane towarzyszowi: ") + pieceName + ".";
		return 0;
	}

	// From the companion to its owner's bag: a bag piece or a worn one (taken
	// off first). The owner's mark and gift go with it.
	int TakePlayerBotSidekickItem(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int from,
			int toCell, std::string& answer)
	{
		const bool fromWear = IsPlayerBotSidekickEqWearPos(from);
		LPITEM item = fromWear ? sk->GetWear(from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				: (IsPlayerBotSidekickEqBagPos(from) ? sk->GetInventoryItem(from) : NULL);
		if (!item || (!fromWear && item->GetCell() != from))
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		answer = GetPlayerBotSidekickHandOverRefusal(item);
		if (!answer.empty())
			return 2;
		if (fromWear && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			answer = "Tego nie da sie zdjac.";
			return 2;
		}
		std::string pieceName = item->GetName() ? item->GetName() : "";
		// Onto a stack of the owner's of the same thing, as far as it takes.
		LPITEM stack = IsPlayerBotSidekickEqBagPos(toCell) ? owner->GetInventoryItem(toCell) : NULL;
		if (!fromWear && stack && stack->GetCell() == toCell)
		{
			const DWORD id = item->GetID();
			const DWORD count = (DWORD)item->GetCount();
			const int poured = PourPlayerBotSidekickStack(sk, item, stack, "PLAYERBOT_SIDEKICK_TAKE");
			if (poured > 0)
			{
				if (poured >= (int)count)
				{
					ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
					ClearPlayerBotSidekickPin(rt, id);
				}
				LogManager::instance().ItemLog(owner, stack, "PLAYERBOT_SIDEKICK_TAKE", sk->GetName());
				char text[160];
				snprintf(text, sizeof(text), "Dolozone do twojego stosu: %d z %u.", poured, (unsigned int)count);
				answer = text;
				return 0;
			}
			if (poured == 0)
			{
				answer = "Twoj stos jest juz pelny.";
				return 2;
			}
		}
		const int cell = FindPlayerBotSidekickHandOverCell(owner, item, IsPlayerBotSidekickEqBagPos(toCell) ? toCell : -1);
		if (cell < 0)
		{
			answer = IsPlayerBotSidekickEqBagPos(toCell) ? "Tam nie ma miejsca." : "Nie masz miejsca w torbie.";
			return 2;
		}
		// A worn piece comes off into the companion's bag first: the engine
		// unequips a piece only there (CItem::RemoveFromCharacter would leave
		// it with two owners).
		if (fromWear && (sk->GetEmptyInventory(item->GetSize()) < 0 || !sk->UnequipItem(item) || item->IsEquipped()))
		{
			answer = sk->GetEmptyInventory(item->GetSize()) < 0 ?
					"Towarzysz nie ma miejsca w torbie, zeby to zdjac - wez najpierw cos z jego torby." :
					"Nie da sie tego teraz zdjac - sprobuj za chwile.";
			return 2;
		}
		const DWORD id = item->GetID();
		HandPlayerBotSidekickItemOver(sk, owner, item, cell);
		ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
		ClearPlayerBotSidekickPin(rt, id);
		LogManager::instance().ItemLog(owner, item, "PLAYERBOT_SIDEKICK_TAKE", sk->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: taken pid=%u owner=%u item=%u vnum=%u worn=%d cell=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), id, item->GetVnum(), fromWear ? 1 : 0, cell);
		answer = std::string("Wziete od towarzysza: ") + pieceName + ".";
		return 0;
	}

	// The window's orders on the bag: "eq" (what changed), "eq 1" (all of
	// it), "eq ruch <z> <na>", "eq daj <twoja komorka> <na>", "eq wez <z>
	// <twoja komorka>", "eq odepnij <pozycja>". Each is answered with one
	// SidekickEqResult and then what changed.
	void HandlePlayerBotSidekickEqCommand(LPCHARACTER owner, const char* op, const char* a, const char* b)
	{
		if (!*op || !strcmp(op, "1"))
		{
			SendPlayerBotSidekickEq(owner, *op != 0);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec->second.dwSidekickPID);
		if (!sk || !sk->IsItemLoaded() || st == s_mapPlayerBotAIStates.end() ||
				!CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysza nie ma teraz w grze.");
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
		TPlayerBotAIState& state = st->second;
		const int from = ParsePlayerBotSidekickEqPos(a);
		const int to = ParsePlayerBotSidekickEqPos(b);
		std::string answer;
		int code = 9;
		const bool twoBags = !strcmp(op, "daj") || !strcmp(op, "wez");
		if (!strcmp(op, "ruch") || twoBags || !strcmp(op, "odepnij"))
		{
			if (from == INT_MIN || (strcmp(op, "odepnij") && to == INT_MIN))
				answer = "Zle miejsce.";
			// A trade, a counter, the safebox, the anvil: the engine's own
			// "busy", for both of them.
			else if (!sk->CanHandleItem())
				answer = "Towarzysz jest teraz zajety (handel, magazyn albo kowal) - sprobuj za chwile.";
			else if (twoBags && !owner->CanHandleItem())
				answer = "Zamknij najpierw handel, sklep albo magazyn.";
			else if (!strcmp(op, "ruch"))
			{
				if (IsPlayerBotSidekickEqBagPos(from) && IsPlayerBotSidekickEqBagPos(to))
					code = MovePlayerBotSidekickBagItem(sk, from, to, answer);
				else if (IsPlayerBotSidekickEqBagPos(from) && (to == -1 || IsPlayerBotSidekickEqWearPos(to)))
				{
					LPITEM item = sk->GetInventoryItem(from);
					if (!item || item->GetCell() != from)
					{
						answer = "Tam nic nie ma.";
						code = 3;
					}
					else
						code = EquipPlayerBotSidekickForOwner(owner, sk, state, rt, item,
								to == -1 ? -1 : to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE, answer);
				}
				else if (IsPlayerBotSidekickEqWearPos(from) && (to == -1 || IsPlayerBotSidekickEqBagPos(to)))
					code = UnequipPlayerBotSidekickForOwner(owner, sk, rt, from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE, to,
							answer);
				else
					answer = "Przeciagnij to do torby towarzysza.";
			}
			else if (!strcmp(op, "daj"))
			{
				if (!IsPlayerBotSidekickEqBagPos(from))
					answer = "Zle miejsce.";
				else
					code = GivePlayerBotSidekickItem(owner, sk, state, rt, from, to, answer);
			}
			else if (!strcmp(op, "wez"))
			{
				if (from == -1 || !(to == -1 || IsPlayerBotSidekickEqBagPos(to)))
					answer = "Zle miejsce.";
				else
					code = TakePlayerBotSidekickItem(owner, sk, rt, from, to, answer);
			}
			else
			{
				LPITEM item = IsPlayerBotSidekickEqWearPos(from) ? sk->GetWear(from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
						: (IsPlayerBotSidekickEqBagPos(from) ? sk->GetInventoryItem(from) : NULL);
				if (!item || (IsPlayerBotSidekickEqBagPos(from) && item->GetCell() != from))
				{
					answer = "Tam nic nie ma.";
					code = 3;
				}
				else
				{
					const bool unwanted = IsPlayerBotSidekickUnwanted(sk, item);
					ClearPlayerBotSidekickPin(rt, item->GetID());
					state.dwNextEquipmentCheckTime = 0;
					answer = unwanted ? "Towarzysz moze to znow zalozyc sam." :
							"Odpiete. Towarzysz znow sam wybiera, co tam nosi.";
					code = 0;
				}
			}
		}
		else
			answer = "Nieznane polecenie okna.";
		sys_log(0, "PLAYERBOT_SIDEKICK: eq %s owner=%u pid=%u from=%s to=%s code=%d", op, owner->GetPlayerID(),
				sk->GetPlayerID(), a, b, code);
		AnswerPlayerBotSidekickEq(owner, code, answer.c_str());
		SendPlayerBotSidekickEq(owner, false);
	}

	// The six skills of the path it walks: from one, thirty-one, sixty-one and
	// ninety-one by class, the second path fifteen further on - the rule of
	// skill_proto and of every client's skill window. 0 with no path.
	DWORD GetPlayerBotSidekickSkillBase(LPCHARACTER sk)
	{
		static const DWORD bases[4] = { 1, 31, 61, 91 };
		const int job = sk->GetJob();
		const int group = sk->GetSkillGroup();
		if (job < 0 || job > 3 || group < 1 || group > 2)
			return 0;
		return bases[job] + (DWORD)(group - 1) * 15;
	}

	void SendPlayerBotSidekickSkills(LPCHARACTER owner)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		if (!sk || !CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		SendPlayerBotSidekickCommand(owner, "SidekickSkillBegin %d %d %d %d %d", PLAYERBOT_SIDEKICK_EQ_PROTOCOL,
				(int)sk->GetPoint(POINT_SKILL), (int)sk->GetJob(), (int)sk->GetSkillGroup(),
				rec->second.bManualSkills ? 1 : 0);
		const DWORD base = GetPlayerBotSidekickSkillBase(sk);
		for (DWORD vnum = base; base != 0 && vnum < base + 6; ++vnum)
			if (CSkillManager::instance().Get(vnum))
				SendPlayerBotSidekickCommand(owner, "SidekickSkill %u %d %d", vnum, (int)sk->GetSkillLevel(vnum),
						(int)sk->GetSkillMasterType(vnum));
		SendPlayerBotSidekickCommand(owner, "SidekickSkillEnd");
	}

	void SetPlayerBotSidekickManualSkills(TPlayerBotSidekick& rec, bool manual)
	{
		rec.bManualSkills = manual;
		SetPlayerBotSidekickSetting(rec, "manual_skills", manual ? 1U : 0U);
	}

	// "umiejetnosci" (the list), "umiejetnosci dodaj <vnum>" (one point there -
	// and from then on the owner spends them, or the skill pass would move the
	// point to its own build), "umiejetnosci reczne <0|1>".
	void HandlePlayerBotSidekickSkillCommand(LPCHARACTER owner, const char* op, const char* a)
	{
		if (!*op)
		{
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		LPCHARACTER sk = rec == s_mapPlayerBotSidekicks.end() ? NULL :
				CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		if (!sk || !CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysza nie ma teraz w grze.");
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		std::string answer;
		int code = 2;
		char text[160];
		if (!strcmp(op, "reczne") && (!strcmp(a, "0") || !strcmp(a, "1")))
		{
			SetPlayerBotSidekickManualSkills(rec->second, !strcmp(a, "1"));
			answer = rec->second.bManualSkills ? "Punkty umiejetnosci rozdajesz teraz ty." :
					"Punkty umiejetnosci rozdaje znow towarzysz.";
			code = 0;
		}
		else if (!strcmp(op, "dodaj"))
		{
			DWORD vnum = 0;
			str_to_number(vnum, a);
			const DWORD base = GetPlayerBotSidekickSkillBase(sk);
			if (base == 0)
				answer = "Towarzysz nie ma jeszcze sciezki (dostanie ja na 5 poziomie).";
			else if (vnum < base || vnum >= base + 6 || !CSkillManager::instance().Get(vnum))
				answer = "To nie jest umiejetnosc towarzysza.";
			else if (sk->GetPoint(POINT_SKILL) <= 0)
				answer = "Towarzysz nie ma wolnych punktow umiejetnosci.";
			else if (sk->GetSkillMasterType(vnum) != SKILL_NORMAL)
				answer = "Te umiejetnosc rozwijaja juz tylko ksiegi i kamienie duchowe.";
			else if (sk->GetSkillLevel(vnum) >= 17)
				answer = "Na 17 poziomie umiejetnosc czeka na mistrza - dalej tylko ksiegi albo reset u Starszej Pani.";
			else
			{
				const int before = sk->GetSkillLevel(vnum);
				const int typeBefore = sk->GetSkillMasterType(vnum);
				sk->SkillLevelUp(vnum);
				const int after = sk->GetSkillLevel(vnum);
				if (after > before || sk->GetSkillMasterType(vnum) != typeBefore)
				{
					if (!rec->second.bManualSkills)
						SetPlayerBotSidekickManualSkills(rec->second, true);
					snprintf(text, sizeof(text), "Umiejetnosc na poziomie %d%s. Punkty rozdajesz teraz ty.", after,
							sk->GetSkillMasterType(vnum) != typeBefore ? " - mistrz!" : "");
					answer = text;
					code = 0;
				}
				else
					answer = "Nie udalo sie - poziom towarzysza jest za niski na te umiejetnosc.";
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: skill up owner=%u pid=%u vnum=%u code=%d level=%d points=%d",
					owner->GetPlayerID(), sk->GetPlayerID(), vnum, code, (int)sk->GetSkillLevel(vnum),
					(int)sk->GetPoint(POINT_SKILL));
		}
		else
			answer = "Nieznane polecenie okna.";
		AnswerPlayerBotSidekickEq(owner, code, answer.c_str());
		SendPlayerBotSidekickSkills(owner);
	}

	// Whether the folded whisper holds the phrase as whole words.
	bool PlayerBotSidekickHeard(const char* folded, const char* phrase)
	{
		const size_t n = strlen(phrase);
		for (const char* p = strstr(folded, phrase); p; p = strstr(p + 1, phrase))
		{
			const bool startOk = p == folded || !isalnum((unsigned char)p[-1]);
			const bool endOk = !isalnum((unsigned char)p[n]);
			if (startOk && endOk)
				return true;
		}
		return false;
	}

	// The owner's whisper to its own companion: the letter's menu without the
	// letter - "chodz", "graj sam", "czekaj", "zakupy", "stan", and how it
	// fights ("atakuj", "nie atakuj pierwszy", "nie walcz"; one whisper may
	// carry a stance and an order). Anything else is a conversation, and the
	// conversation layer answers it as it answers anybody
	// (playerbot_chat_conversation.h).
	bool HandlePlayerBotSidekickWhisper(LPCHARACTER from, LPCHARACTER bot, const char* text)
	{
		if (!from || !bot || !text || s_mapPlayerBotSidekickOwner.empty())
			return false;
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(bot->GetPlayerID());
		if (!rec || rec->dwOwnerPID != from->GetPlayerID())
			return false;
		char folded[160];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		bool handled = false;
		const int stance = GetPlayerBotSidekickStanceHeard(folded);
		if (stance >= 0)
		{
			SendPlayerBotWhisper(bot, from, SetPlayerBotSidekickStance(*rec, (BYTE)stance));
			handled = true;
		}
		static const char* const summonWords[] = { "chodz", "do mnie", "wracaj", "wroc", "za mna", "przywolaj",
				"tutaj", "come", "follow" };
		static const char* const freeWords[] = { "graj sam", "graj po swojemu", "wolna reka", "idz expic",
				"expij sam", "idz sam", "go play" };
		// Before the call, whose "tutaj" is in "czekaj tutaj".
		static const char* const holdWords[] = { "czekaj", "zaczekaj", "poczekaj", "zostan", "stoj", "wait" };
		// "Zrob miejsce w eq" is what a player writes when the bag is full
		// (a player's screenshot of 25 September: the companion answered it with
		// talk, "Zero, EQ pelne"): the errand is where the merchant takes the junk.
		static const char* const errandWords[] = { "zakupy", "na zakupy", "do miasta", "idz do miasta",
				"zrob miejsce", "oproznij", "wyczysc eq", "sprzedaj smieci" };
		for (size_t i = 0; i < sizeof(freeWords) / sizeof(freeWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, freeWords[i]))
			{
				SendPlayerBotWhisper(bot, from, rec->bMode == PLAYERBOT_SIDEKICK_FREE ?
						"Juz gram po swojemu. Napisz \"chodz\", kiedy bede potrzebny." :
						"Dobra, ide expic po swojemu. Napisz \"chodz\", kiedy bede potrzebny.");
				if (rec->bMode != PLAYERBOT_SIDEKICK_FREE)
					FreePlayerBotSidekick(from, *rec);
				return true;
			}
		for (size_t i = 0; i < sizeof(errandWords) / sizeof(errandWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, errandWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Dobra, zaraz zobacze, co trzeba kupic.");
				SendPlayerBotSidekickShopping(from, *rec, get_dword_time());
				return true;
			}
		for (size_t i = 0; i < sizeof(holdWords) / sizeof(holdWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, holdWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Dobra, czekam.");
				HoldPlayerBotSidekick(from, *rec, get_dword_time());
				return true;
			}
		for (size_t i = 0; i < sizeof(summonWords) / sizeof(summonWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, summonWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Juz ide!");
				SummonPlayerBotSidekick(from, *rec, get_dword_time());
				return true;
			}
		if (PlayerBotSidekickHeard(folded, "stan") || PlayerBotSidekickHeard(folded, "status"))
		{
			ReportPlayerBotSidekick(from, *rec);
			return true;
		}
		return handled;
	}

	// /towarzysz stworz <rasa 0-7> <sciezka 1-2> <nick> | przywolaj | wolny | czekaj | zakupy | stan
	//            | walka <0 atakuj, 1 nie atakuj pierwszy, 2 nie walcz> | zbieraj <0 nic, 1 twoj, 2 wszystko>
	//            | ochrona <0|1> | buffy <0|1> | okno [1] | odprawa tak
	//            | eq [1 | ruch <z> <na> | daj <z> <na> | wez <z> <na> | odepnij <pozycja>]
	//            | umiejetnosci [dodaj <vnum> | reczne <0|1>]
	void HandlePlayerBotSidekickCommand(LPCHARACTER ch, const char* argument)
	{
		if (!ch || !ch->GetDesc() || (ch->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		if (!EnsurePlayerBotSidekickTable())
		{
			SayPlayerBotSidekick(ch, "Towarzysze sa teraz niedostepni - baza nie odpowiada.");
			return;
		}
		char sub[32] = "";
		char a1[32] = "";
		char a2[32] = "";
		char a3[64] = "";
		const char* rest = one_argument(argument ? argument : "", sub, sizeof(sub));
		rest = one_argument(rest, a1, sizeof(a1));
		rest = one_argument(rest, a2, sizeof(a2));
		one_argument(rest, a3, sizeof(a3));
		const DWORD dwNow = get_dword_time();
		// The world's switch: the window is told (a third answer, "2"), and
		// everything else - a new companion included - is refused.
		if (IsPlayerBotSidekickSwitchedOff())
		{
			if (!strcmp(sub, "okno"))
				SendPlayerBotSidekickCommand(ch, "SidekickInfo %d 2", PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL);
			else if (!strcmp(sub, "eq") || !strcmp(sub, "umiejetnosci"))
				SendPlayerBotSidekickCommand(ch, "SidekickEqNone %d 2", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			else
				SayPlayerBotSidekick(ch, "Towarzysze sa wylaczeni na tym serwerze (wlacza je wlasciciel serwera w launcherze).");
			return;
		}
		if (!strcmp(sub, "stworz"))
		{
			int race = -1, group = 0;
			str_to_number(race, a1);
			str_to_number(group, a2);
			CreatePlayerBotSidekick(ch, race, group, a3);
			return;
		}
		// The window asks every second and a half while it is open, and for the
		// whole gear when it opens ("okno 1"); with no companion it is told so.
		if (!strcmp(sub, "okno"))
		{
			SendPlayerBotSidekickWindow(ch, !strcmp(a1, "1"));
			return;
		}
		// The bag window and the skills window (uisidekickinventory.py): each
		// says for itself that there is no companion.
		if (!strcmp(sub, "eq"))
		{
			HandlePlayerBotSidekickEqCommand(ch, a1, a2, a3);
			return;
		}
		if (!strcmp(sub, "umiejetnosci"))
		{
			HandlePlayerBotSidekickSkillCommand(ch, a1, a2);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(ch->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SetPlayerBotSidekickFlag(ch->GetPlayerID(), "towarzysz.created", 0);
			SayPlayerBotSidekick(ch, "Nie masz jeszcze towarzysza - wybierz go w liscie Towarzysz.");
			return;
		}
		if (!strcmp(sub, "przywolaj"))
			SummonPlayerBotSidekick(ch, rec->second, dwNow);
		else if (!strcmp(sub, "wolny"))
			FreePlayerBotSidekick(ch, rec->second);
		else if (!strcmp(sub, "czekaj"))
			HoldPlayerBotSidekick(ch, rec->second, dwNow);
		else if (!strcmp(sub, "zakupy"))
			SendPlayerBotSidekickShopping(ch, rec->second, dwNow);
		else if (!strcmp(sub, "zbieraj") || !strcmp(sub, "ochrona") || !strcmp(sub, "buffy"))
		{
			int value = -1;
			if (*a1)
				str_to_number(value, a1);
			TPlayerBotSidekick& r = rec->second;
			if (!strcmp(sub, "zbieraj") && value >= PLAYERBOT_SIDEKICK_LOOT_NONE && value <= PLAYERBOT_SIDEKICK_LOOT_ALL)
			{
				r.bLoot = (BYTE)value;
				SetPlayerBotSidekickSetting(r, "loot", (unsigned int)value);
				SayPlayerBotSidekick(ch, value == PLAYERBOT_SIDEKICK_LOOT_ALL ? "Zbieram caly drop: twoj i swoj." :
						value == PLAYERBOT_SIDEKICK_LOOT_OWNERS ? "Zbieram tylko twoj drop." : "Nie zbieram dropu.");
			}
			else if (!strcmp(sub, "ochrona") && (value == 0 || value == 1))
			{
				r.bProtect = value == 1;
				SetPlayerBotSidekickSetting(r, "protect", (unsigned int)value);
				SayPlayerBotSidekick(ch, r.bProtect ? "Gdy bedziesz ginac, sciagne na siebie potwory." :
						"Nie bede sciagac z ciebie potworow.");
			}
			else if (!strcmp(sub, "buffy") && (value == 0 || value == 1))
			{
				r.bBuffs = value == 1;
				SetPlayerBotSidekickSetting(r, "buffs", (unsigned int)value);
				SayPlayerBotSidekick(ch, r.bBuffs ? "Bede cie buffowac (jesli umiem)." : "Nie bede cie buffowac.");
			}
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz zbieraj 0|1|2, /towarzysz ochrona 0|1, /towarzysz buffy 0|1");
		}
		else if (!strcmp(sub, "walka"))
		{
			int stance = -1;
			if (!strcmp(a1, "atakuj") || !strcmp(a1, "atak"))
				stance = PLAYERBOT_SIDEKICK_STANCE_ATTACK;
			else if (!strcmp(a1, "obrona") || !strcmp(a1, "bron"))
				stance = PLAYERBOT_SIDEKICK_STANCE_DEFEND;
			else if (!strcmp(a1, "spokoj") || !strcmp(a1, "nie"))
				stance = PLAYERBOT_SIDEKICK_STANCE_PASSIVE;
			else if (*a1)
				str_to_number(stance, a1);
			if (stance < PLAYERBOT_SIDEKICK_STANCE_ATTACK || stance > PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
			{
				char text[192];
				snprintf(text, sizeof(text), "Teraz: %s. Zmien: /towarzysz walka atakuj | obrona | spokoj",
						GetPlayerBotSidekickStanceName(rec->second.bStance));
				SayPlayerBotSidekick(ch, text);
			}
			else
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickStance(rec->second, (BYTE)stance));
		}
		else if (!strcmp(sub, "odprawa"))
		{
			if (!strcmp(a1, "tak"))
				DismissPlayerBotSidekick(ch, rec->second);
			else
				SayPlayerBotSidekick(ch, "Na pewno? Wpisz: /towarzysz odprawa tak");
		}
		else
			ReportPlayerBotSidekick(ch, rec->second);
	}

	// ------------------------------------------------------ at the owner's side

	// Every trade the owner opens is accepted once the owner has accepted it,
	// and anybody else's is closed at once. What the owner put in is the
	// owner's gift: never sold, never on a counter, never scrap
	// (IsPlayerBotJunkItem), and worn when the equipment pass finds it better -
	// the same judgement it makes of everything, so the two never take turns.
	bool HandlePlayerBotSidekickTrade(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		CExchange* exchange = ch->GetExchange();
		if (!exchange)
		{
			if (!rt.bTrading)
				return false;
			rt.bTrading = false;
			int gifts = 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell ||
						rt.setBagBeforeTrade.find(item->GetID()) != rt.setBagBeforeTrade.end())
					continue;
				if (rt.setGifts.insert(item->GetID()).second)
				{
					DBManager::instance().Query(
							"INSERT IGNORE INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at) "
							"VALUES (%u, %u, NOW())", item->GetID(), ch->GetPlayerID());
					++gifts;
				}
			}
			rt.setBagBeforeTrade.clear();
			if (gifts > 0)
			{
				SayPlayerBotSidekick(GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID),
						"Dzieki! Zachowam to, a co lepsze od mojego, zaraz zaloze.");
				state.dwNextEquipmentCheckTime = 0;
				sys_log(0, "PLAYERBOT_SIDEKICK: gifts pid=%u name=%s items=%d", ch->GetPlayerID(), ch->GetName(),
						gifts);
			}
			return false;
		}
		CExchange* other = exchange->GetCompany();
		LPCHARACTER partner = other ? other->GetOwner() : NULL;
		if (!partner || partner->GetPlayerID() != rec.dwOwnerPID)
		{
			exchange->Cancel();
			return true;
		}
		if (!rt.bTrading)
		{
			rt.bTrading = true;
			rt.setBagBeforeTrade.clear();
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (item && item->GetCell() == cell)
					rt.setBagBeforeTrade.insert(item->GetID());
			}
		}
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		// Only after the owner: accepting first would close the trade on
		// whatever was in the window at that moment. The trade may end inside
		// this call, and the two exchanges with it.
		if (other->GetAcceptStatus() && !exchange->GetAcceptStatus())
			exchange->Accept(true);
		return true;
	}

	// What the companion fights: the owner's own target, then what is hitting
	// the owner, then what is hitting itself, then - with nothing of that - the
	// nearest monster within PLAYERBOT_SIDEKICK_HUNT_RANGE of the owner. A
	// monster only, and a stone only when the owner hits it: a person is the
	// owner's business, a duel above all, and the Anti-PK protocol answers a
	// person who strikes the owner. A boss is fought when it fights one of the
	// two, never looked for.
	// The ranges are measured from a centre: the owner at its side, the spot
	// when it was told to wait ("Czekaj tutaj"), where the owner - when it
	// is on that map at all - may be far away.
	struct FPlayerBotSidekickFoes
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		long centreX;
		long centreY;
		long mapIndex;
		LPCHARACTER onOwner;
		int onOwnerDist;
		LPCHARACTER onSelf;
		int onSelfDist;
		LPCHARACTER idle;
		int idleDist;

		FPlayerBotSidekickFoes(LPCHARACTER s, LPCHARACTER o, long x, long y, long map)
			: self(s), owner(o), centreX(x), centreY(y), mapIndex(map), onOwner(NULL), onOwnerDist(INT_MAX),
			  onSelf(NULL), onSelfDist(INT_MAX), idle(NULL), idleDist(INT_MAX)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c == self || c == owner || c->IsDead() || !c->IsMonster() || c->GetMapIndex() != mapIndex)
				return;
			const int fromOwner = DISTANCE_APPROX(c->GetX() - centreX, c->GetY() - centreY);
			if (fromOwner > PLAYERBOT_SIDEKICK_GUARD_RANGE)
				return;
			const int fromSelf = DISTANCE_APPROX(c->GetX() - self->GetX(), c->GetY() - self->GetY());
			if (owner && c->GetVictim() == owner)
			{
				if (fromOwner < onOwnerDist && battle_is_attackable(self, c))
				{
					onOwner = c;
					onOwnerDist = fromOwner;
				}
				return;
			}
			if (c->GetVictim() == self)
			{
				if (fromSelf < onSelfDist && battle_is_attackable(self, c))
				{
					onSelf = c;
					onSelfDist = fromSelf;
				}
				return;
			}
			if (fromOwner > PLAYERBOT_SIDEKICK_HUNT_RANGE || c->GetMobRank() >= MOB_RANK_BOSS)
				return;
			if (fromSelf < idleDist && battle_is_attackable(self, c))
			{
				idle = c;
				idleDist = fromSelf;
			}
		}
	};

	// A character the owner is at war with: its guild and the owner's are in a
	// guild war. The one kind of person the companion strikes on its own
	// initiative, and only the one its owner has in hand.
	bool IsPlayerBotSidekickWarFoe(LPCHARACTER owner, LPCHARACTER target)
	{
		if (!owner || !target || !target->IsPC() || !owner->GetGuild() || !target->GetGuild())
			return false;
		return owner->GetGuild() != target->GetGuild() && owner->GetGuild()->UnderWar(target->GetGuild()->GetID());
	}

	// What is hitting a losing owner turns on the companion.
	struct FPlayerBotSidekickTaunt
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		int taken;

		FPlayerBotSidekickTaunt(LPCHARACTER s, LPCHARACTER o) : self(s), owner(o), taken(0)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (taken >= PLAYERBOT_SIDEKICK_PROTECT_MAX_MONSTERS || !ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER monster = (LPCHARACTER)ent;
			if (monster->IsDead() || !monster->IsMonster() || monster->GetVictim() != owner ||
					monster->GetMapIndex() != owner->GetMapIndex() ||
					DISTANCE_APPROX(monster->GetX() - owner->GetX(), monster->GetY() - owner->GetY()) >
							PLAYERBOT_SIDEKICK_GUARD_RANGE ||
					!battle_is_attackable(monster, self))
				return;
			monster->UpdateAggrPoint(self, DAMAGE_TYPE_SPECIAL, monster->GetMaxHP());
			monster->SetVictim(self);
			++taken;
		}
	};

	void ProtectPlayerBotSidekickOwner(LPCHARACTER ch, LPCHARACTER owner, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (dwNow < rt.dwNextProtect || owner->IsDead() || owner->GetMaxHP() <= 0 || ch->GetMaxHP() <= 0 ||
				(long long)owner->GetHP() * 100 >= (long long)owner->GetMaxHP() * PLAYERBOT_SIDEKICK_PROTECT_HP_PERCENT ||
				(long long)ch->GetHP() * 100 < (long long)ch->GetMaxHP() * PLAYERBOT_SIDEKICK_PROTECT_SELF_HP_PERCENT ||
				!owner->GetSectree())
			return;
		rt.dwNextProtect = dwNow + PLAYERBOT_SIDEKICK_PROTECT_INTERVAL_MS;
		FPlayerBotSidekickTaunt taunt(ch, owner);
		owner->GetSectree()->ForEachAround(taunt);
		if (taunt.taken > 0)
			PlayerBotLogThrottled("sidekick_protect", dwNow,
					"PLAYERBOT_SIDEKICK: took the owner's attackers pid=%u name=%s owner=%u owner_hp=%d/%d taken=%d",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), owner->GetHP(), owner->GetMaxHP(),
					taunt.taken);
	}

	// The owner's target is a fight already: a monster at the owner, at the
	// companion or at somebody of the owner's party, a stone somebody has begun
	// (a stone takes no victim), a guild war's foe. Merely clicked on, it is
	// not - which is what "nie atakuj pierwszy" asks.
	bool IsPlayerBotSidekickFightUnderWay(LPCHARACTER ch, LPCHARACTER owner, LPCHARACTER target)
	{
		if (IsPlayerBotSidekickWarFoe(owner, target))
			return true;
		if (target->IsStone())
			return target->GetHP() < target->GetMaxHP();
		LPCHARACTER victim = target->GetVictim();
		return victim && (victim == owner || victim == ch ||
				(owner->GetParty() && victim->GetParty() == owner->GetParty()));
	}

	LPCHARACTER FindPlayerBotSidekickFoe(LPCHARACTER ch, LPCHARACTER owner, BYTE stance, int& why)
	{
		LPCHARACTER target = stance == PLAYERBOT_SIDEKICK_STANCE_PASSIVE ? NULL : owner->GetTarget();
		if (target && target != ch && !target->IsDead() &&
				(target->IsMonster() || target->IsStone() || IsPlayerBotSidekickWarFoe(owner, target)) &&
				target->GetMapIndex() == owner->GetMapIndex() &&
				DISTANCE_APPROX(target->GetX() - owner->GetX(), target->GetY() - owner->GetY()) <=
						PLAYERBOT_SIDEKICK_ASSIST_RANGE &&
				(stance == PLAYERBOT_SIDEKICK_STANCE_ATTACK || IsPlayerBotSidekickFightUnderWay(ch, owner, target)) &&
				battle_is_attackable(ch, target))
		{
			why = PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET;
			return target;
		}
		if (!owner->GetSectree())
			return NULL;
		FPlayerBotSidekickFoes foes(ch, owner, owner->GetX(), owner->GetY(), owner->GetMapIndex());
		owner->GetSectree()->ForEachAround(foes);
		if (foes.onOwner && stance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
		{
			why = PLAYERBOT_SIDEKICK_FOE_AT_OWNER;
			return foes.onOwner;
		}
		if (foes.onSelf)
		{
			why = PLAYERBOT_SIDEKICK_FOE_AT_SELF;
			return foes.onSelf;
		}
		if (foes.idle && stance == PLAYERBOT_SIDEKICK_STANCE_ATTACK)
		{
			why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
			return foes.idle;
		}
		return NULL;
	}

	// The drops worth going for: the owner's, which the engine hands to the
	// owner when a party member picks them up (CHARACTER::PickupItem, the party
	// branch), and what is the companion's own to take.
	struct FPlayerBotSidekickLoot
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		const TPlayerBotSidekickRuntime& rt;
		DWORD now;
		BYTE mode;	// EPlayerBotSidekickLoot
		LPITEM best;
		int bestDist;

		FPlayerBotSidekickLoot(LPCHARACTER s, LPCHARACTER o, const TPlayerBotSidekickRuntime& r, DWORD n, BYTE m)
			: self(s), owner(o), rt(r), now(n), mode(m), best(NULL), bestDist(INT_MAX)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_ITEM))
				return;
			LPITEM item = (LPITEM)ent;
			if (!item->GetSectree() || item->GetOwner())
				return;
			const int d = DISTANCE_APPROX(item->GetX() - self->GetX(), item->GetY() - self->GetY());
			if (d > PLAYERBOT_SIDEKICK_LOOT_RANGE || d >= bestDist)
				return;
			std::map<DWORD, DWORD>::const_iterator failed = rt.mapLootFailed.find(item->GetVID());
			if (failed != rt.mapLootFailed.end() && (int)(now - failed->second) < 0)
				return;
			const bool ownersOnly = owner && item->IsOwnership(owner) && !item->IsOwnership(self);
			if (ownersOnly)
			{
				// The party branch hands over only what may change hands.
				if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) ||
						self->GetParty() == NULL || self->GetParty() != owner->GetParty())
					return;
			}
			// "Tylko moj" in the window: the owner's drops, none of its own.
			else if (mode != PLAYERBOT_SIDEKICK_LOOT_ALL || !item->IsOwnership(self) ||
					IsPlayerBotLootBeneathBot(self, item) || !PlayerBotBagTakesDrop(self, item))
				return;
			best = item;
			bestDist = d;
		}
	};

	bool PickUpPlayerBotSidekickLoot(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner,
			TPlayerBotSidekickRuntime& rt, BYTE lootMode, DWORD dwNow)
	{
		if (!ch->GetSectree() || lootMode == PLAYERBOT_SIDEKICK_LOOT_NONE)
			return false;
		if (rt.mapLootFailed.size() > 64)
		{
			for (std::map<DWORD, DWORD>::iterator it = rt.mapLootFailed.begin(); it != rt.mapLootFailed.end(); )
			{
				if ((int)(dwNow - it->second) >= 0)
					rt.mapLootFailed.erase(it++);
				else
					++it;
			}
		}
		FPlayerBotSidekickLoot loot(ch, owner, rt, dwNow, lootMode);
		ch->GetSectree()->ForEachAround(loot);
		if (!loot.best)
		{
			rt.dwLootVID = 0;
			return false;
		}
		const DWORD vid = loot.best->GetVID();
		// The same drop for longer than a walk to it takes: something refuses
		// it (the owner's bag, a pick-up clock), and it is left alone a while.
		if (rt.dwLootVID != vid)
		{
			rt.dwLootVID = vid;
			rt.dwLootSince = dwNow;
		}
		else if (dwNow - rt.dwLootSince > PLAYERBOT_SIDEKICK_LOOT_GIVE_UP_MS)
		{
			rt.mapLootFailed[vid] = dwNow + PLAYERBOT_SIDEKICK_LOOT_FAILED_MS;
			rt.dwLootVID = 0;
			return false;
		}
		SetPlayerBotAction(state, BOT_ACTION_LOOT, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		if (loot.bestDist > PLAYERBOT_SIDEKICK_PICKUP_RANGE)
		{
			WalkPlayerBotSidekick(ch, loot.best->GetX(), loot.best->GetY(), dwNow, false);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->PickupItem(vid);
		return true;
	}

	// The blacksmith and the merchants where the owner stands: the refine pass,
	// the junk sold and the merchants' shopping, as a town visit would do them -
	// "najlepiej wtedy, kiedy stoimy przy kowalu".
	struct FPlayerBotSidekickNpcs
	{
		LPCHARACTER owner;
		bool blacksmith;
		bool weapons;
		bool armour;
		bool misc;

		explicit FPlayerBotSidekickNpcs(LPCHARACTER o)
			: owner(o), blacksmith(false), weapons(false), armour(false), misc(false)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (!c->IsNPC() || DISTANCE_APPROX(c->GetX() - owner->GetX(), c->GetY() - owner->GetY()) >
					PLAYERBOT_SIDEKICK_NPC_RANGE)
				return;
			switch (c->GetRaceNum())
			{
				case 20016: blacksmith = true; break;
				case 9001: weapons = true; break;
				case 9002: armour = true; break;
				case 9003: misc = true; break;
				default: break;
			}
		}
	};

	bool ServePlayerBotSidekickAtNpcs(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow)
	{
		if (!owner->GetSectree() || ch->GetExchange())
			return false;
		FPlayerBotSidekickNpcs npcs(owner);
		owner->GetSectree()->ForEachAround(npcs);
		if (!npcs.blacksmith && !npcs.weapons && !npcs.armour && !npcs.misc)
			return false;
		bool did = false;
		if (npcs.weapons)
			did = ManagePlayerBotWeaponMerchant(ch) || did;
		if (npcs.armour)
			did = ManagePlayerBotArmorMerchant(ch) || did;
		if (npcs.misc)
			did = ManagePlayerBotMiscMerchant(ch) || did;
		if (npcs.blacksmith)
		{
			state.dwNextRefineCheckTime = 0;
			did = ManagePlayerBotRefining(ch, state, dwNow) || did;
		}
		if (did)
		{
			SetPlayerBotAction(state, npcs.blacksmith ? BOT_ACTION_REFINE : BOT_ACTION_SHOP, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			sys_log(0, "PLAYERBOT_SIDEKICK: served at the npcs pid=%u name=%s smith=%d weapons=%d armour=%d misc=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), npcs.blacksmith ? 1 : 0, npcs.weapons ? 1 : 0,
					npcs.armour ? 1 : 0, npcs.misc ? 1 : 0, (long long)ch->GetGold());
		}
		return did;
	}

	// Staying alive at the owner's side: the potions, and after a death the
	// recovery the tick gives every bot - but standing up and healing beside
	// the owner rather than walking off from where it fell
	// (HandlePostDeathRecovery walks away from lDeathX/lDeathY). No breaking
	// off at a fifth of its health either: the owner is the one who retreats,
	// and the companion goes with the owner.
	bool KeepPlayerBotSidekickAlive(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow)
	{
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		state.bTacticalRetreat = false;
		if (!state.bRecoveringAfterDeath)
			return false;
		state.lDeathX = 0;
		state.lDeathY = 0;
		if (!HandlePostDeathRecovery(ch, state, dwNow))
			return false;
		// One told to wait heals where it fell and walks back to its spot after.
		if (!owner || IsPlayerBotSidekickHolding(ch))
			return true;
		const int dist = DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY());
		if (owner->GetMapIndex() == ch->GetMapIndex() && dist > PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE)
		{
			long x = 0, y = 0;
			GetPlayerBotSidekickSpot(ch, owner, x, y);
			WalkPlayerBotSidekick(ch, x, y, dwNow, false);
		}
		return true;
	}

	// Defined in playerbot_manager.cpp, after the fragments: a Shaman's buffs
	// cast on a person, the owner here (ManagePlayerBotBuffHumanLeader's pass).
	bool ManagePlayerBotBuffPerson(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, DWORD dwNow,
			bool mayWalk);

	// The owner's buffs come before the companion's own and before the next
	// blow. They were cast only when nothing was left to fight, and in the
	// default stance a companion hunting beside its owner always has a
	// monster: the fight put its buffs on itself (FightPlayerBotTowerObjective)
	// and the owner went without them for as long as the hunt lasted - "zaczela
	// stawiac wylacznie na siebie" (Teivos, 25 September). A buff out of reach
	// waits for the fight's end rather than walking the companion off its foe.
	bool BuffPlayerBotSidekickOwner(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			LPCHARACTER owner, DWORD dwNow, bool mayWalk)
	{
		if (!rec.bBuffs || !owner || owner->IsDead() || owner->GetMapIndex() != ch->GetMapIndex())
			return false;
		return ManagePlayerBotBuffPerson(ch, state, owner, dwNow, mayWalk);
	}

	// The path the owner chose, the moment the engine allows one:
	// CHARACTER::SetSkillGroup refuses a character under level five
	// (char_skill.cpp), so a companion made at the start of the game takes it at
	// five, wherever it stands. Never the trainer's, which draws a path by pid
	// (playerbot_skills.h), and back again after the old woman's reset.
	void KeepPlayerBotSidekickPath(LPCHARACTER ch, const TPlayerBotSidekick& rec)
	{
		if (rec.bGroup == 0 || ch->GetLevel() < 5 || ch->GetSkillGroup() == rec.bGroup)
			return;
		const BYTE before = ch->GetSkillGroup();
		if (before != 0)
			ch->ClearSkill();
		ch->SetSkillGroup(rec.bGroup);
		sys_log(0, "PLAYERBOT_SIDEKICK: path pid=%u name=%s level=%d group=%u->%u", ch->GetPlayerID(), ch->GetName(),
				ch->GetLevel(), (unsigned int)before, (unsigned int)ch->GetSkillGroup());
	}

	// A fight where it stands - at its owner's side, or at the spot it keeps -
	// with the straight walk a map with no navigation grid needs.
	bool FightPlayerBotSidekickFoe(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekickRuntime& rt,
			LPCHARACTER foe, int why, DWORD dwNow)
	{
		if (foe->GetVID() != rt.dwLastFoeVID)
		{
			rt.dwLastFoeVID = foe->GetVID();
			++rt.adwFoes[MINMAX(0, why, 3)];
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		const int foeDist = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		if (foeDist > PLAYERBOT_DUEL_MELEE_RANGE &&
				!CPlayerBotNavigation::instance(ch->GetMapIndex()).Init(ch->GetMapIndex()) &&
				dwNow >= state.dwNextTowerMoveTime)
		{
			state.dwNextTowerMoveTime = dwNow + 1000;
			WalkPlayerBotSidekickDirect(ch, foe->GetX(), foe->GetY(), dwNow);
		}
		return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
	}

	// "Czekaj tutaj": it keeps its spot. What comes at it is fought, and - in
	// the default stance - what stands near the spot; the owner, when it is
	// on this map, is defended there as at its side. The drops within reach
	// are picked up, and it walks back to the spot when a fight took it off.
	bool ManagePlayerBotSidekickHold(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, LPCHARACTER owner, DWORD dwNow)
	{
		if (ch->GetMapIndex() != rt.lHoldMap || !ch->GetSectree())
		{
			// Something took it off the map (a warp, a dungeon's end): its spot
			// is gone with it, and it goes back to its owner.
			rt.bHold = false;
			return true;
		}
		LPCHARACTER sameMapOwner = owner && owner->GetMapIndex() == ch->GetMapIndex() ? owner : NULL;
		if (sameMapOwner && dwNow >= rt.dwNextPartyCheck)
		{
			rt.dwNextPartyCheck = dwNow + PLAYERBOT_SIDEKICK_PARTY_CHECK_MS;
			KeepPlayerBotSidekickInParty(ch, sameMapOwner, dwNow);
		}
		FPlayerBotSidekickFoes foes(ch, sameMapOwner, rt.lHoldX, rt.lHoldY, rt.lHoldMap);
		ch->GetSectree()->ForEachAround(foes);
		LPCHARACTER foe = NULL;
		int why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
		if (foes.onOwner && rec.bStance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
		{
			foe = foes.onOwner;
			why = PLAYERBOT_SIDEKICK_FOE_AT_OWNER;
		}
		else if (foes.onSelf)
		{
			foe = foes.onSelf;
			why = PLAYERBOT_SIDEKICK_FOE_AT_SELF;
		}
		else if (foes.idle && rec.bStance == PLAYERBOT_SIDEKICK_STANCE_ATTACK)
			foe = foes.idle;
		// The owner near the spot is buffed as at its side, but a waiting
		// companion never walks to it for that.
		if (sameMapOwner && BuffPlayerBotSidekickOwner(ch, state, rec, sameMapOwner, dwNow, false))
			return true;
		if (foe)
			return FightPlayerBotSidekickFoe(ch, state, rt, foe, why, dwNow);
		if (state.dwTargetVID != 0)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
		}
		if (dwNow >= rt.dwNextLoot)
		{
			rt.dwNextLoot = dwNow + PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS;
			if (PickUpPlayerBotSidekickLoot(ch, state, sameMapOwner, rt, rec.bLoot, dwNow))
				return true;
		}
		const int away = DISTANCE_APPROX(ch->GetX() - rt.lHoldX, ch->GetY() - rt.lHoldY);
		if (away > PLAYERBOT_SIDEKICK_HOLD_LEASH)
		{
			WalkPlayerBotSidekick(ch, rt.lHoldX, rt.lHoldY, dwNow, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		ManagePlayerBotCombatBuffs(ch, state, dwNow, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// "Idz na zakupy" under way: the ordinary town visit runs in the rest of
	// the tick, and this only says when it is over. A visit that never began
	// (nothing to do) and one that has run too long end it too.
	bool ManagePlayerBotSidekickErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (!rt.bErrandVisit && state.bVisitingShop)
			rt.bErrandVisit = true;
		// The order may be newer than this tick's clock: a command handled in
		// the same pass (the self-test's) stamps get_dword_time(), later than
		// the dwNow the pass began with, and the unsigned difference wrapped
		// round to four billion - "errand over why=timeout" in the second it
		// began.
		const DWORD elapsed = dwNow >= rt.dwErrandSince ? dwNow - rt.dwErrandSince : 0;
		if (rt.bErrandVisit)
		{
			if (state.bVisitingShop && elapsed < PLAYERBOT_SIDEKICK_ERRAND_MAX_MS)
				return false;
			EndPlayerBotSidekickErrand(ch, state, rec, rt, dwNow, state.bVisitingShop ? "timeout" : "done");
			return true;
		}
		if (elapsed < PLAYERBOT_SIDEKICK_ERRAND_START_MS)
			return false;
		EndPlayerBotSidekickErrand(ch, state, rec, rt, dwNow, "nothing");
		return true;
	}

	// The companion's part of the tick. The trade in either mode; at its
	// owner's side everything else too, and then it owns the tick, so neither
	// the wander nor the target search takes it anywhere the owner is not.
	// Above it in the tick run the duel (it never takes one), the guild war,
	// the Anti-PK protocol, and the upkeep - stats, skills, books, the gear
	// pass, chests - exactly as for any bot.
	bool ManagePlayerBotSidekick(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec)
			return false;
		KeepPlayerBotSidekickPath(ch, *rec);
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[ch->GetPlayerID()];
		if (HandlePlayerBotSidekickTrade(ch, state, *rec, rt, dwNow))
			return true;
		if (rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW)
			return false;
		if (rt.bErrand)
			return ManagePlayerBotSidekickErrand(ch, state, *rec, rt, dwNow);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		if (!owner || !owner->GetSectree())
		{
			// Keeping a spot, it keeps it while its owner warps.
			if (rt.bHold)
			{
				if (KeepPlayerBotSidekickAlive(ch, state, NULL, dwNow))
					return true;
				return ManagePlayerBotSidekickHold(ch, state, *rec, rt, NULL, dwNow);
			}
			// A warp: it waits where it is for the owner to come back into this
			// world (ManagePlayerBotSidekicks lets it go if the owner does not).
			if (ch->IsStateMove())
				ch->Stop();
			ch->SetVictim(NULL);
			state.dwTargetVID = 0;
			state.dwLastMeaningfulActivityTime = dwNow;
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}
		// The personality pass sits below this one in the tick, so at the
		// owner's side it runs here or never - and the title over the
		// companion's head read "Grinder", decided before it joined the party,
		// where it should read Towarzysz (Tieru's screenshots of 25 September).
		ManagePlayerBotPersona(ch, state, dwNow);
		if (KeepPlayerBotSidekickAlive(ch, state, owner, dwNow))
			return true;
		// A piece its owner put on waits out the second and a half after a blow
		// that the engine asks of every equip: the fight holds off that long, and
		// the equipment pass above puts it on.
		if (rt.dwEquipWaitUntil != 0)
		{
			int pinnedWear = -1;
			if (dwNow < rt.dwEquipWaitUntil && FindPlayerBotSidekickPinnedInBag(ch, pinnedWear, false))
			{
				if (ch->IsStateMove())
					ch->Stop();
				state.bEquipPending = true;
				state.dwLastMeaningfulActivityTime = dwNow;
				return true;
			}
			rt.dwEquipWaitUntil = 0;
		}
		if (rt.bHold)
			return ManagePlayerBotSidekickHold(ch, state, *rec, rt, owner, dwNow);
		// Another map of this core, or the owner outran it: beside the owner.
		// A dungeon instance too - the owner is its guide, which a bot on its
		// own never has.
		const bool otherMap = owner->GetMapIndex() != ch->GetMapIndex();
		const int dist = otherMap ? INT_MAX
				: DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY());
		if (otherMap || dist > PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE)
		{
			if (owner->IsWarping() || owner->IsDead() || dwNow < rt.dwNextCatchUp)
				return true;
			rt.dwNextCatchUp = dwNow + PLAYERBOT_SIDEKICK_CATCH_UP_MS;
			PlacePlayerBotSidekick(ch, state, owner, dwNow, otherMap ? "sidekick_follow" : "sidekick_catch_up");
			return true;
		}
		if (dwNow >= rt.dwNextPartyCheck)
		{
			rt.dwNextPartyCheck = dwNow + PLAYERBOT_SIDEKICK_PARTY_CHECK_MS;
			KeepPlayerBotSidekickInParty(ch, owner, dwNow);
		}
		if (rec->bStance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE && rec->bProtect)
			ProtectPlayerBotSidekickOwner(ch, owner, rt, dwNow);
		int why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
		LPCHARACTER foe = owner->IsDead() ? NULL : FindPlayerBotSidekickFoe(ch, owner, rec->bStance, why);
		if (foe)
		{
			if (BuffPlayerBotSidekickOwner(ch, state, *rec, owner, dwNow, false))
				return true;
			return FightPlayerBotSidekickFoe(ch, state, rt, foe, why, dwNow);
		}
		if (state.dwTargetVID != 0)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
		}
		if (dist <= PLAYERBOT_SIDEKICK_NPC_RANGE + PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE && dwNow >= rt.dwNextService)
		{
			rt.dwNextService = dwNow + PLAYERBOT_SIDEKICK_SERVICE_INTERVAL_MS;
			if (ServePlayerBotSidekickAtNpcs(ch, state, owner, dwNow))
				return true;
		}
		if (dwNow >= rt.dwNextLoot)
		{
			rt.dwNextLoot = dwNow + PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS;
			if (PickUpPlayerBotSidekickLoot(ch, state, owner, rt, rec->bLoot, dwNow))
				return true;
		}
		if (dist > PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE)
		{
			long x = 0, y = 0;
			GetPlayerBotSidekickSpot(ch, owner, x, y);
			WalkPlayerBotSidekick(ch, x, y, dwNow, dist > 1500);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			return true;
		}
		// Beside the owner with nothing to fight: the owner's buffs, its own,
		// and standing where it is.
		if (ch->IsStateMove() && dist <= PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE / 2)
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		if (BuffPlayerBotSidekickOwner(ch, state, *rec, owner, dwNow, true))
			return true;
		ManagePlayerBotCombatBuffs(ch, state, dwNow, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}
}
