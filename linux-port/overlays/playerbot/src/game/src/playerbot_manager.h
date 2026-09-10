#ifndef __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__
#define __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__

#include <set>
#include <deque>

class CPlayerBotManager : public singleton<CPlayerBotManager>
{
	public:
		CPlayerBotManager();
		~CPlayerBotManager();

		bool	Spawn(DWORD dwPlayerID, BYTE bEmpire);
		size_t	SpawnRegistered(size_t count, BYTE bEmpire);
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
		// A player's shout, after the channel has it, and a whisper addressed to
		// a bot. Both from patch 0007 in input_main.cpp; playerbot_chat_trade.h
		// decides whether and which bot answers.
		void	OnPlayerShout(LPCHARACTER ch, const char* szText);
		void	OnPlayerWhisper(LPCHARACTER from, LPCHARACTER bot, const char* szText);

		bool	IsManaged(DWORD dwPlayerID) const;
		bool	IsRegistered(DWORD dwPlayerID);
		size_t	GetCount() const;
		// Registered identities not spawned right now, ascending, at most
		// `limit` of them - the F9 panel's "bots ready to spawn" list.
		void	GetAvailableBots(std::vector<DWORD>& out, size_t limit);

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
		struct TPlayerBotAccount { DWORD dwID; std::string strLogin; BYTE bEmpire; };
		typedef std::map<DWORD, TPlayerBotAccount> TPlayerBotAccountMap;

		bool	LoadRegisteredBots();
		// Says in one line why the cohort is smaller than the seed.
		void	ReportPlayerBotRegistryShortfall(unsigned int usable);
		// Re-queues registered identities that are not in the world.
		void	TopUpMissingBots(DWORD dwNow);

		TPlayerBotMap		m_mapBots;
		THandleToPlayerMap	m_mapHandles;
		TRegisteredPlayerBotSet m_setRegisteredBots;
		TPlayerBotAccountMap	m_mapBotAccounts;
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
		bool			m_bRegistryLoaded;
		bool			m_bRegistryAvailable;
};

#endif
