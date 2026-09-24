#ifndef __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__
#define __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__

#include <set>
#include <deque>

class CGuild;
class CAsyncSQL;

class CPlayerBotManager : public singleton<CPlayerBotManager>
{
	public:
		CPlayerBotManager();
		~CPlayerBotManager();

		bool	Spawn(DWORD dwPlayerID, BYTE bEmpire);
		size_t	SpawnRegistered(size_t count, BYTE bEmpire);
		// The operator's medal droppers, scheduled on top of the population from
		// the far end of a kingdom's registry, and the level their experience
		// stops at (PLAYERBOT_MEDAL_DROPPERS, PLAYERBOT_MEDAL_DROPPER_LEVEL).
		size_t	SpawnMedalDropperCohort(size_t count, BYTE bEmpire, BYTE bExpLockLevel);
		bool	IsMedalDropperCohortPID(DWORD dwPlayerID) const;
		BYTE	GetMedalDropperCohortLevel() const;
		// The kingdom a registered PID belongs to, 0 when it is not registered.
		BYTE	GetRegisteredEmpire(DWORD dwPlayerID);
		// How many identities each kingdom has, indexed by empire (0 unused).
		// The bootstrap needs this before it can split one budget three ways.
		void	CountRegisteredPerEmpire(int* out, int size);
		void	SpawnPendingBatch(DWORD dwNow);
		bool	Despawn(DWORD dwPlayerID);

		void	OnPlayerLoaded(LPDESC d);
		void	OnLoadFailed(DWORD dwHandle);
		void	OnDescriptorDestroyed(LPDESC d);
		void	Update();
		// The part of Update that is about the world and not about bots - the
		// weights file and the timed events with their chest gate - on a clock
		// of its own from the moment a core knows its maps (input_db.cpp,
		// MapLocations, through playerbotify.py). A core that hosts no bot has
		// no Update, and its chests used to drop whatever the events said; this
		// stops by itself once the first bot's Update is running.
		void	StartWorldClock();
		// A player's shout, after the channel has it, and a whisper addressed to
		// a bot. Both from patch 0007 in input_main.cpp; playerbot_chat_trade.h
		// decides whether and which bot answers.
		void	OnPlayerShout(LPCHARACTER ch, const char* szText);
		void	OnPlayerWhisper(LPCHARACTER from, LPCHARACTER bot, const char* szText);

		bool	IsManaged(DWORD dwPlayerID) const;
		bool	IsRegistered(DWORD dwPlayerID);
		// The same question answered from the registry as it is, never by
		// loading it: false until the bootstrap has loaded it. For callers
		// that may run before that and must not trigger the load (p2p.cpp).
		bool	IsRegisteredBotPID(DWORD dwPlayerID) const;
		size_t	GetCount() const;
		// Registered identities not spawned right now, ascending, at most
		// `limit` of them - the F9 panel's "bots ready to spawn" list.
		void	GetAvailableBots(std::vector<DWORD>& out, size_t limit);
		// A GM's /transfer of a bot on this core (cmd_gm.cpp, playerbotify.py):
		// the map change the AI makes itself, onto the GM's spot, with the
		// answer in the GM's chat. The engine's WarpSet only takes a bot off its
		// sectree, and the rescue puts it back at its own map's start.
		bool	TransferBot(LPCHARACTER bot, LPCHARACTER to);
		// A bot's WarpSet (char.cpp, playerbotify.py): the engine's map change
		// for a player made server-side for a bot - a dungeon's jump, an exit,
		// a quest's warp. False when this core does not host the map.
		bool	WarpBot(LPCHARACTER bot, long x, long y, long lPrivateMapIndex);
		// A player invited a bot into a guild (CGuild::Invite, mt2009 via
		// playerbotify.py): answered on the spot, while the invitation lives.
		void	OnGuildInvite(CGuild* guild, LPCHARACTER inviter, LPCHARACTER invitee);
		// A guild master's /war (cmd_general.cpp, mt2009 via playerbotify.py):
		// on a bot guild it is a field war the bots answer themselves, declared
		// here past the engine's rules for players; true when it was one.
		bool	OnPlayerWarRequest(LPCHARACTER ch, CGuild* mine, CGuild* opponent);
		// Every war declaration the db core hands a core (CInputDB::GuildWar,
		// mt2009 via playerbotify.py): a player's on a bot guild waits for the
		// bots' answer (playerbot_guild_war.h).
		void	OnGuildWarDeclared(DWORD dwGuildFrom, DWORD dwGuildTo, BYTE bType);
		// A player struck a bot, or a person in a party (CHARACTER::Damage,
		// mt2009 via playerbotify.py): the Anti-PK protocol's only source of
		// who is attacking a bot - the engine keeps no record of it.
		void	OnPlayerStruck(LPCHARACTER victim, LPCHARACTER attacker);

		// The operator's spawn plan (input_db.cpp through playerbotify.py): the
		// window the cohort arrives over, and a second cohort that joins one at
		// a time over hours - scheduled here, spawned from Update.
		void	SetSpawnWindow(DWORD dwWindowMs);
		size_t	ScheduleLateJoiners(size_t count, BYTE bEmpire, DWORD dwWindowMs);
		// The second channel (M2_PLAYERBOT_CH2, playerbot_channel_rules.h): how
		// much of the operator's number this core's channel starts - of the
		// whole world's, or of one kingdom's when bEmpire is given. The
		// identities themselves are split when the registry loads: this core
		// only ever registers its own channel's.
		int	ScaleToThisChannel(int total, BYTE bEmpire = 0);
		// The bootstrap's split made channel-aware: the world's number between
		// the kingdoms over every channel's identities - so a kingdom has the
		// same share with the second channel on as off - and each kingdom's
		// part between the channels, capped by this channel's identities.
		// With the second channel off, want[] is left as it is on channel 1
		// and emptied on any other.
		void	SplitForThisChannel(int total, const int* registeredHere, int* want);
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The two channels with moves (playerbot_channel_rules.h): with the
		// second channel on, a bot's channel is its row of
		// common.playerbot_channel_assignment, and a bot on the second channel
		// with business at a shop - its own stand, a stand to open, another's
		// counter - asks here to be moved to the shop channel. Queued, and sent
		// with the others in one statement from Update; never a query of its
		// own. False when it cannot apply (not this core's bot, already there,
		// no table).
		bool	RequestShopChannel(DWORD dwPlayerID);
		bool	IsChannelTableMode() const { return m_bChannelTable; }
		// The machinery's turn on a core no bot has woken yet (the world clock
		// calls it): a channel that starts with nobody still learns who is
		// moved to it, and spawns them.
		void	ChannelClockTick(DWORD dwNow);
#endif

		// The three things the F10 bot-admin window asks for. The data behind
		// the last two lives in playerbot_admin.h, inside the anonymous
		// namespace of playerbot_manager.cpp that no engine translation unit
		// can see - these are the way through, exactly like the two weight
		// functions below.
		void	GetActivitySummary(size_t& total, size_t& inParty, size_t& stalls) const;
		void	GetBotLines(DWORD dwPlayerID, std::vector<std::string>& out) const;
		bool	GetAchievementWinner(int id, DWORD& dwPID, std::string& strName) const;

	private:
		typedef std::map<DWORD, LPDESC> TPlayerBotMap;
		typedef std::map<DWORD, DWORD> THandleToPlayerMap;
		typedef std::set<DWORD> TRegisteredPlayerBotSet;
		// The account behind a registered bot: id and login, from the same
		// registry query. A bot's descriptor is created without one, and the
		// engine keys the safebox by the descriptor's account id - so with it
		// left at zero every bot deposited into one shared box under account 0.
		// The kingdom is part of the identity, not something a caller may pass
		// in: Spawn takes it from here, so nothing can start a registered PID
		// into an empire its character does not belong to.
		// bChannel is the channel the identity plays on, and dwReadyAt the unix
		// time from which it may log in there: a bot moved between channels
		// must be out of its old one first (the two channels with moves).
		struct TPlayerBotAccount { DWORD dwID; std::string strLogin; BYTE bEmpire; BYTE bLevel;
				BYTE bChannel = 1; DWORD dwReadyAt = 0; };
		typedef std::map<DWORD, TPlayerBotAccount> TPlayerBotAccountMap;

		bool	LoadRegisteredBots();
		// Says in one line why the cohort is smaller than the seed.
		void	ReportPlayerBotRegistryShortfall(unsigned int usable);
		// Re-queues registered identities that are not in the world.
		void	TopUpMissingBots(DWORD dwNow);
		// Which registered bots a GM has banned, and taking them out of the
		// world. Every bot account is status='BLOCK' by design (so no human can
		// log into one), so that column cannot tell a banned bot from a normal
		// one - only the ban ledger account.account_block can, and it is empty
		// until somebody runs /block_player. A banned bot is despawned here and
		// SpawnPendingBatch/TopUpMissingBots never bring it back, so a ban is no
		// longer undone by the top-up a minute later (mateuszp211).
		void	RefreshBannedBots(DWORD dwNow);
		// The late joiners whose moment has come (ScheduleLateJoiners).
		void	SpawnLateJoiners(DWORD dwNow);
		// "Boty graja jak zywi ludzie": sessions, log-outs and the rests the
		// top-up must not cut short (the LIFE switch of the weights file).
		void	ManageLifeSchedule(DWORD dwNow);
		bool	IsRestingBot(DWORD dwPlayerID) const;

#if defined(PLAYERBOT_ENGINE_MT2009)
		// The two channels with moves. Every statement below runs on a
		// connection and a thread of its own (m_pChannelSql): the game thread
		// only queues one and collects the answer on a later tick, so a slow
		// database costs the moves a little latency and a player's login
		// nothing (SIZOWSKI's first version queried on the game thread, and a
		// slow database threw players out at the character screen).
		void	RunChannelMachinery(DWORD dwNow);
		bool	EnsureChannelSql();
		void	SendChannelSql(int iKind, unsigned int uA, unsigned int uB, unsigned int uC,
				unsigned int uD, const std::string& strQuery);
		void	ProcessChannelSql();
		void	OnChannelAssignments(void* pMsg);
		void	OnChannelCensus(void* pMsg);
		void	OnChannelSwapStep(void* pMsg);
		void	SeedChannelAssignments();
		void	PublishChannelPresence(DWORD dwNow);
		void	FlushChannelRequests(DWORD dwNow);
		void	CoordinateChannelSwaps(DWORD dwNow);
		void	RefreshChannelAssignments(DWORD dwNow);
		void	SpawnChannelArrivals(DWORD dwNow);

		CAsyncSQL*		m_pChannelSql = NULL;
		bool			m_bChannelSqlFailed = false;
		bool			m_bChannelSeeded = false;
		bool			m_bChannelRefreshInFlight = false;
		bool			m_bChannelCoordInFlight = false;
		DWORD			m_dwNextChannelRefreshTime = 0;
		DWORD			m_dwNextChannelCoordinatorTime = 0;
		DWORD			m_dwNextChannelPresenceTime = 0;
		DWORD			m_dwNextChannelFlushTime = 0;
		DWORD			m_dwChannelCoordinatorSince = 0;
		std::set<DWORD>		m_setChannelRequests;
		std::set<DWORD>		m_setChannelArrivals;
		// Arrivals on the shop channel, until they load (OnPlayerLoaded).
		std::set<DWORD>		m_setChannelMovedIn;
#endif

		TPlayerBotMap		m_mapBots;
		THandleToPlayerMap	m_mapHandles;
		// This channel's identities - what may be spawned here - and every
		// identity whatever its channel, which is what "is this pid a bot"
		// asks (IsRegisteredBotPID: the guilds, the parties, the P2P count).
		TRegisteredPlayerBotSet m_setRegisteredBots;
		TRegisteredPlayerBotSet m_setAllRegisteredBots;
		TPlayerBotAccountMap	m_mapBotAccounts;
		// The second channel's plan, read with the registry: the switch, the
		// share, and the identities each channel (1, 2) holds per kingdom.
		bool			m_bSecondChannel = false;
		int			m_iSecondChannelShare = 40;
		int			m_aChannelIdentities[3][4] = {};
		// Whether the channels come from the assignment table (mt2009 with the
		// second channel on) rather than the spread and the pins.
		bool			m_bChannelTable = false;
		// Spawns still to be sent, and when the next batch goes. Filled by
		// SpawnRegistered, drained by Update, see PLAYERBOT_SPAWN_WINDOW.
		std::deque<DWORD>	m_dequePendingSpawns;
		// Exactly which identities this core asked for. TopUpMissingBots counts
		// the world against this, not against "the first N of the registry" -
		// with three kingdoms in one registry that prefix is somebody else's.
		std::set<DWORD>		m_setScheduledBots;
		DWORD			m_dwNextSpawnBatchTime;
		size_t			m_uSpawnBatchSize;
		DWORD			m_dwSpawnWindowStarted;
		size_t			m_uSpawnWindowTotal;
		// When to count the world again and re-queue whoever is missing.
		DWORD			m_dwNextTopUpTime;
		// Registered PIDs a GM has banned (account.account_block), and when to
		// read that ledger next. Kept out of the world and out of the spawn queue.
		std::set<DWORD>		m_setBannedBots;
		DWORD			m_dwNextBanCheckTime;
		bool			m_bRegistryLoaded;
		bool			m_bRegistryAvailable;
		// The medal droppers' cohort and its level (SpawnMedalDropperCohort).
		std::set<DWORD>		m_setMedalDropperCohort;
		BYTE			m_bMedalDropperCohortLevel = 0;
		// The spawn plan: how long the cohort takes to arrive, and who joins
		// later - (when, pid) ascending, spawned by SpawnLateJoiners.
		DWORD			m_dwSpawnWindowMs = 60000;
		std::deque<std::pair<DWORD, DWORD> >	m_dequeLateJoiners;
		size_t			m_uLateJoinersTotal = 0;
		// The life schedule: when each live bot's session ends, until when a
		// logged-out bot rests (kept out of the world and out of the top-up),
		// and who is on the way back from a rest.
		std::map<DWORD, DWORD>	m_mapLifeSessionEnd;
		std::map<DWORD, DWORD>	m_mapLifeRestEnd;
		std::set<DWORD>		m_setLifeReturning;
		DWORD			m_dwNextLifeCheckTime = 0;
		DWORD			m_dwNextLifeCensusTime = 0;
};

// The AI weights, for the F9 GM panel's "Sterowanie Serwerem" tab.
//
// They live in playerbot_weights.tsv, which the web panel writes and the core
// re-reads every five seconds; the client panel is a second writer of the same
// file. The reader, the writer and the bounds are all in playerbot_config.h -
// inside the anonymous namespace of playerbot_manager.cpp, which no engine
// translation unit can see - so cmd_gm.cpp reaches them through these two.
//
// The report is the seventeen values the panel expects, "|"-joined, in the
// order the client zips its rows against by position; -1 means "no file has
// set this" and is only ever the two chest keys. Setting refuses a name this
// core does not know rather than appending it.
bool PlayerBotBuildWeightReport(char* szOut, size_t len);
bool PlayerBotSetWeight(const char* szKey, long value);

#endif
