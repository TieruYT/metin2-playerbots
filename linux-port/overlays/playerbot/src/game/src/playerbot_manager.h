#ifndef __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__
#define __INC_METIN_II_GAME_PLAYERBOT_MANAGER_H__

class CPlayerBotManager : public singleton<CPlayerBotManager>
{
	public:
		CPlayerBotManager();
		~CPlayerBotManager();

		bool	Spawn(DWORD dwPlayerID, BYTE bEmpire);
		size_t	SpawnRegistered(size_t count, BYTE bEmpire);
		bool	Despawn(DWORD dwPlayerID);

		void	OnPlayerLoaded(LPDESC d);
		void	OnLoadFailed(DWORD dwHandle);
		void	OnDescriptorDestroyed(LPDESC d);
		void	Update();

		bool	IsManaged(DWORD dwPlayerID) const;
		bool	IsRegistered(DWORD dwPlayerID);
		size_t	GetCount() const;

	private:
		typedef std::map<DWORD, LPDESC> TPlayerBotMap;
		typedef std::map<DWORD, DWORD> THandleToPlayerMap;
		typedef std::set<DWORD> TRegisteredPlayerBotSet;

		bool	LoadRegisteredBots();

		TPlayerBotMap		m_mapBots;
		THandleToPlayerMap	m_mapHandles;
		TRegisteredPlayerBotSet m_setRegisteredBots;
		bool			m_bRegistryLoaded;
		bool			m_bRegistryAvailable;
};

#endif
