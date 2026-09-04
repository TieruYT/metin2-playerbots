#include "stdafx.h"
#include "playerbot_manager.h"
#include "playerbot_world_rules.h"

#include "char.h"
#include "char_manager.h"
#include "cmd.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "db.h"
#include "event.h"
#include "fishing.h"
#include "input.h"
#include "item.h"
#include "item_manager.h"
#include "config.h"
#include "constants.h"
#include "battle.h"
#include "buffer_manager.h"
#include "motion.h"
#include "party.h"
#include "questmanager.h"
#include "questpc.h"
#include "refine.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "vector.h"
#include "utils.h"
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <cstdio>
#include <cstring>

extern int passes_per_sec;

extern void SendShout(const char* szText, BYTE bEmpire);

#include "playerbot_types.h"

namespace
{
	TPlayerBotAIStateMap s_mapPlayerBotAIStates;
	LPEVENT s_pkPlayerBotUpdateEvent = NULL;

	struct TKnownPlayerBotMetin
	{
		TKnownPlayerBotMetin() : lMapIndex(0), lX(0), lY(0), bLevel(0),
			dwLastSeenTime(0), dwReservedByPID(0), dwReserveUntil(0) {}
		long lMapIndex;
		long lX;
		long lY;
		BYTE bLevel;
		DWORD dwLastSeenTime;
		DWORD dwReservedByPID;
		DWORD dwReserveUntil;
	};

	typedef std::map<DWORD, TKnownPlayerBotMetin> TKnownPlayerBotMetinMap;
	TKnownPlayerBotMetinMap s_mapKnownPlayerBotMetins;
	DWORD s_adwPlayerBotMetinHotspotVisits[12] = { 0 };
	DWORD s_adwPlayerBotMetinHotspotFinds[12] = { 0 };
	DWORD s_adwPlayerBotMetinHotspotLastFind[12] = { 0 };

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

	std::string GetPlayerBotBiologistFlag(const TPlayerBotBiologistMission& mission,
			const char* flag)
	{
		return std::string(mission.questName) + "." + flag;
	}

	int GetPlayerBotBiologistStateIndex(size_t missionIndex, const char* stateName)
	{
		static int s_complete[PLAYERBOT_BIOLOGIST_MISSION_COUNT] = { -1, -1, -1, -1, -1, -1 };
		static int s_collecting[PLAYERBOT_BIOLOGIST_MISSION_COUNT] = { -1, -1, -1, -1, -1, -1 };
		if (missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return -1;

		int* cache = strcmp(stateName, "__complete") == 0 ? s_complete : s_collecting;
		if (cache[missionIndex] < 0)
			cache[missionIndex] = quest::CQuestManager::instance().GetQuestStateIndex(
					PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex].questName, stateName);
		return cache[missionIndex];
	}

	bool IsPlayerBotBiologistMissionComplete(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int completeState = GetPlayerBotBiologistStateIndex(missionIndex, "__complete");
		return completeState >= 0 &&
				ch->GetQuestFlag(GetPlayerBotBiologistFlag(mission, "__status")) == completeState;
	}

	const TPlayerBotBiologistMission* GetActivePlayerBotBiologistMission(
			LPCHARACTER ch, size_t* outIndex = NULL)
	{
		if (!ch)
			return NULL;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[i];
			if (ch->GetLevel() < mission.requiredLevel)
				break;
			if (!IsPlayerBotBiologistMissionComplete(ch, i))
			{
				if (outIndex)
					*outIndex = i;
				return &mission;
			}
		}
		return NULL;
	}

	bool HasPlayerBotCompletedEarlyBiologist(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			if (!IsPlayerBotBiologistMissionComplete(ch, i))
				return false;
		}
		return true;
	}

	BYTE GetPlayerBotNextHorseRequiredLevel(BYTE horseLevel)
	{
		if (horseLevel >= 21)
			return 255;
		if (horseLevel >= 20)
			return 50; // military horse milestone
		if (horseLevel >= 10)
			return 35; // combat horse milestone
		return PLAYERBOT_HORSE_REQUIRED_LEVEL;
	}

	bool CanPlayerBotAdvanceHorse(LPCHARACTER ch)
	{
		return ch && ch->GetHorseLevel() < 21 &&
				ch->GetLevel() >= GetPlayerBotNextHorseRequiredLevel(ch->GetHorseLevel());
	}

	bool EnsurePlayerBotBiologistMissionStarted(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int collectingState = GetPlayerBotBiologistStateIndex(missionIndex, "go_to_disciple");
		if (collectingState < 0)
			return false;

		const std::string statusFlag = GetPlayerBotBiologistFlag(mission, "__status");
		if (ch->GetQuestFlag(statusFlag) != collectingState)
		{
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			if (!pc)
				return false;
			pc->SetQuestState(mission.questName, collectingState);
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "collect_count"), 0);
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "drink_drug"), 0);
			sys_log(0, "PLAYERBOT_BIOLOGIST: mission started pid=%u name=%s quest=%s item=%u mob=%u need=%u",
					ch->GetPlayerID(), ch->GetName(), mission.questName,
					mission.itemVnum, mission.mobVnum, mission.requiredCount);
		}
		return true;
	}

	const TPlayerBotHuntingMission* GetActivePlayerBotHuntingMission(
			LPCHARACTER ch, int* outLevel = NULL, int* outSelection = NULL,
			int* outRemaining = NULL)
	{
		if (!ch)
			return NULL;
		const int level = ch->GetQuestFlag("levelup.current");
		if (level < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				level > PLAYERBOT_HUNTING_MAX_LEVEL || level > ch->GetLevel())
			return NULL;

		const int selection = ch->GetQuestFlag("levelup.select") == 2 ? 2 : 1;
		if (outLevel)
			*outLevel = level;
		if (outSelection)
			*outSelection = selection;
		if (outRemaining)
			*outRemaining = std::max(0, ch->GetQuestFlag("levelup.remain"));
		return &PLAYERBOT_HUNTING_MISSIONS[level];
	}

	DWORD GetActivePlayerBotHuntingMobVnum(LPCHARACTER ch, int* outRemaining = NULL)
	{
		int selection = 1;
		int remaining = 0;
		const TPlayerBotHuntingMission* mission = GetActivePlayerBotHuntingMission(
				ch, NULL, &selection, &remaining);
		if (outRemaining)
			*outRemaining = remaining;
		if (!mission || remaining <= 0)
			return 0;
		return selection == 2 ? mission->secondMobVnum : mission->firstMobVnum;
	}

	void GivePlayerBotHuntingReward(LPCHARACTER ch, int missionLevel)
	{
		if (!ch || missionLevel < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				missionLevel > PLAYERBOT_HUNTING_MAX_LEVEL)
			return;

		DWORD rewardItem = 0;
		DWORD rewardCount = 1;
		if (missionLevel == 2)
		{
			const DWORD rewards[] = { 11200, 11400, 11600, 11800 };
			rewardItem = rewards[std::min<int>(ch->GetJob(), JOB_SHAMAN)];
		}
		else if (missionLevel == 3)
		{
			const DWORD rewards[] = { 12200, 12340, 12480, 12620 };
			rewardItem = rewards[std::min<int>(ch->GetJob(), JOB_SHAMAN)];
		}
		else if (missionLevel == 4)
			rewardItem = 13000;
		else if (missionLevel <= 21 || missionLevel == 25)
		{
			const int roll = number(1, 100);
			rewardItem = roll <= 33 ? 27002 : (roll <= 67 ? 27005 : 27114);
			rewardCount = rewardItem == 27114 ? 5 : 10;
		}
		else if (missionLevel >= 22 && missionLevel <= 24)
		{
			const DWORD bases[] = { 15080, 16080, 17080 };
			rewardItem = bases[missionLevel - 22] + number(0, 3) * 20;
		}

		if (rewardItem != 0)
			ch->AutoGiveItem(rewardItem, rewardCount, -1, false);
		if (missionLevel == 12 || missionLevel == 14 || missionLevel == 16 ||
				missionLevel == 18 || missionLevel == 20)
			ch->AutoGiveItem(50083, 1, -1, false);

		int expPercent = PLAYERBOT_HUNTING_MISSIONS[missionLevel].expPercent;
		DWORD rewardGold = 0;
		if (missionLevel >= 21)
		{
			const int goldRoll = number(0, 99);
			rewardGold = goldRoll < 20 ? 10000 :
					(goldRoll < 70 ? 20000 : (goldRoll < 95 ? 40000 :
					(goldRoll < 98 ? 80000 : 100000)));

			const int expRoll = number(0, 98);
			expPercent = expRoll < 9 ? 2 : (expRoll < 23 ? 3 :
					(expRoll < 62 ? 4 : (expRoll < 86 ? 6 :
					(expRoll < 95 ? 8 : 10))));
		}

		if (rewardGold > 0)
			ch->PointChange(POINT_GOLD, rewardGold, true);
		if (expPercent > 0)
		{
			const DWORD rewardExp = (DWORD)(((unsigned long long)
					exp_table[MINMAX(0, missionLevel, PLAYER_EXP_TABLE_MAX)] *
					expPercent) / 100);
			if (rewardExp > 0)
				ch->PointChange(POINT_EXP, rewardExp, true);
		}
	}

	void StartPlayerBotHuntingMission(LPCHARACTER ch, int missionLevel)
	{
		if (!ch || missionLevel < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				missionLevel > PLAYERBOT_HUNTING_MAX_LEVEL || missionLevel > ch->GetLevel())
			return;

		static int s_startState = -1;
		if (s_startState < 0)
			s_startState = quest::CQuestManager::instance().GetQuestStateIndex(
					"levelup", "start");
		quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		if (pc && s_startState >= 0)
			pc->SetQuestState("levelup", s_startState);

		const TPlayerBotHuntingMission& mission =
				PLAYERBOT_HUNTING_MISSIONS[missionLevel];
		// The two official choices are split deterministically, so 350 bots do not
		// all converge on the same species after accepting the same mission.
		const int selection = ((ch->GetPlayerID() + missionLevel) % 2) + 1;
		const int count = selection == 2 ? mission.secondCount : mission.firstCount;
		ch->SetQuestFlag("levelup.current", missionLevel);
		ch->SetQuestFlag("levelup.select", selection);
		ch->SetQuestFlag("levelup.remain", count);
		// levelup.quest decrements kills only after the human has clicked Confirm.
		// A playerbot has no quest UI, so -1 represents that exact accepted state.
		ch->SetQuestFlag("levelup.buttonstate", -1);
		sys_log(0, "PLAYERBOT_HUNTING: accepted pid=%u name=%s mission_level=%d select=%d mob=%u count=%d",
				ch->GetPlayerID(), ch->GetName(), missionLevel, selection,
				selection == 2 ? mission.secondMobVnum : mission.firstMobVnum, count);
	}

	void ManagePlayerBotHuntingProgress(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_HUNTING_FIRST_LEVEL)
			return;

		int current = ch->GetQuestFlag("levelup.current");
		const int completed = std::max(0, ch->GetQuestFlag("levelup.complete"));
		if (current == 0)
		{
			const int next = std::max<int>(PLAYERBOT_HUNTING_FIRST_LEVEL, completed + 1);
			if (next <= PLAYERBOT_HUNTING_MAX_LEVEL && next <= ch->GetLevel())
				StartPlayerBotHuntingMission(ch, next);
			return;
		}

		if (current < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				current > PLAYERBOT_HUNTING_MAX_LEVEL || current > ch->GetLevel())
			return;

		const int remain = ch->GetQuestFlag("levelup.remain");
		if (remain > 0)
		{
			// Existing bots reached buttonstate=1 at login and waited forever for a
			// click. Accept once, preserving a mission already in progress.
			if (ch->GetQuestFlag("levelup.buttonstate") != -1)
			{
				if (remain == (int)PLAYERBOT_HUNTING_MISSIONS[current].firstCount &&
						ch->GetQuestFlag("levelup.select") == 1)
					StartPlayerBotHuntingMission(ch, current);
				else
					ch->SetQuestFlag("levelup.buttonstate", -1);
			}
			return;
		}

		if (completed != current)
		{
			GivePlayerBotHuntingReward(ch, current);
			ch->SetQuestFlag("levelup.complete", current);
			sys_log(0, "PLAYERBOT_HUNTING: completed pid=%u name=%s mission_level=%d",
					ch->GetPlayerID(), ch->GetName(), current);
		}

		const int next = current + 1;
		if (next <= PLAYERBOT_HUNTING_MAX_LEVEL && next <= ch->GetLevel())
			StartPlayerBotHuntingMission(ch, next);
		else
		{
			ch->SetQuestFlag("levelup.current", 0);
			ch->SetQuestFlag("levelup.remain", 0);
			ch->SetQuestFlag("levelup.buttonstate", 0);
		}
	}

	bool LegacyIsPlayerBotPositionBlocked(long lMapIndex, long x, long y)
	{
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		if (!tree)
			return true;

		// Solid walls/cliffs (ATTR_BLOCK) and rivers/water bodies (ATTR_WATER) are impassable
		return tree->IsAttr(x, y, ATTR_BLOCK | ATTR_WATER);
	}

	bool LegacyIsPlayerBotPathClear(long lMapIndex, long startX, long startY, long endX, long endY)
	{
		const int dist = DISTANCE_APPROX(startX - endX, startY - endY);
		if (dist <= 200)
			return !LegacyIsPlayerBotPositionBlocked(lMapIndex, endX, endY);

		const int steps = std::min(15, std::max(2, dist / 250));
		for (int i = 1; i <= steps; ++i)
		{
			const long px = startX + ((endX - startX) * i) / steps;
			const long py = startY + ((endY - startY) * i) / steps;
			if (LegacyIsPlayerBotPositionBlocked(lMapIndex, px, py))
				return false;
		}

		return true;
	}

	const int NAVGRID_W = 512;
	const int NAVGRID_H = 640;
	const int NAVGRID_CELL = 200;
	const long NAVGRID_BASE_Y = 102400;

	class CPlayerBotNavGrid
	{
		public:
			static CPlayerBotNavGrid& instance()
			{
				static CPlayerBotNavGrid s_grid;
				return s_grid;
			}

			CPlayerBotNavGrid() : m_bInitialized(false)
			{
				memset(m_grid, 0, sizeof(m_grid));
			}

			void Init(long lMapIndex = 21)
			{
				if (m_bInitialized)
					return;

				LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
				if (!pkSectreeMap)
					return;

				memset(m_grid, 1, sizeof(m_grid)); // Default all to blocked

				int walkableCount = 0;
				for (int sy = 0; sy < 20; ++sy)
				{
					for (int sx = 0; sx < 16; ++sx)
					{
						DWORD dwPackage = ((16 + sy) << 16) | (DWORD)sx;
						LPSECTREE tree = pkSectreeMap->Find(dwPackage);
						if (!tree || !tree->GetAttributePtr())
							continue;

						int baseGX = sx * 32;
						int baseGY = sy * 32;

						for (int ly = 0; ly < 32; ++ly)
						{
							for (int lx = 0; lx < 32; ++lx)
							{
								long wx = (baseGX + lx) * NAVGRID_CELL + 100;
								long wy = NAVGRID_BASE_Y + (baseGY + ly) * NAVGRID_CELL + 100;

								if (tree->IsAttr(wx, wy, ATTR_BLOCK))
								{
									m_grid[baseGX + lx][baseGY + ly] = 1;
								}
								else if (tree->IsAttr(wx, wy, ATTR_WATER))
								{
									m_grid[baseGX + lx][baseGY + ly] = 2;
								}
								else
								{
									m_grid[baseGX + lx][baseGY + ly] = 0;
									++walkableCount;
								}
							}
						}
					}
				}

				m_bInitialized = true;
				DumpCollisionMapBMP("/opt/metin2/var/map21_collision.bmp");
				sys_log(0, "PLAYERBOT_NAVGRID: loaded %dx%d grid for map %ld (%d walkable cells) and dumped BMP",
						NAVGRID_W, NAVGRID_H, lMapIndex, walkableCount);
			}

			void DumpCollisionMapBMP(const char* szFilename)
			{
				FILE* fp = fopen(szFilename, "wb");
				if (!fp)
					return;

#pragma pack(push, 1)
				struct {
					uint16_t type;
					uint32_t size;
					uint16_t reserved1;
					uint16_t reserved2;
					uint32_t offset;
					uint32_t header_size;
					int32_t  width;
					int32_t  height;
					uint16_t planes;
					uint16_t bpp;
					uint32_t compression;
					uint32_t image_size;
					int32_t  x_ppm;
					int32_t  y_ppm;
					uint32_t clr_used;
					uint32_t clr_important;
				} bmp;
#pragma pack(pop)

				memset(&bmp, 0, sizeof(bmp));
				bmp.type = 0x4D42;
				bmp.header_size = 40;
				bmp.width = NAVGRID_W;
				bmp.height = NAVGRID_H;
				bmp.planes = 1;
				bmp.bpp = 24;
				bmp.offset = sizeof(bmp);
				bmp.image_size = NAVGRID_W * NAVGRID_H * 3;
				bmp.size = bmp.offset + bmp.image_size;

				fwrite(&bmp, sizeof(bmp), 1, fp);

				uint8_t row[NAVGRID_W * 3];
				for (int y = NAVGRID_H - 1; y >= 0; --y)
				{
					for (int x = 0; x < NAVGRID_W; ++x)
					{
						uint8_t r = 34, g = 177, b = 76; // Walkable green
						if (m_grid[x][y] == 1)      { r = 237; g = 28;  b = 36;  } // Red: Blocked
						else if (m_grid[x][y] == 2) { r = 0;   g = 162; b = 232; } // Blue: Water

						long wx = x * NAVGRID_CELL;
						long wy = NAVGRID_BASE_Y + y * NAVGRID_CELL;
						if (wx >= 50000 && wx <= 72000 && wy >= 152000 && wy <= 184000 && m_grid[x][y] == 0)
						{
							r = 255; g = 242; b = 0; // Yellow: City interior
						}

						row[x * 3 + 0] = b;
						row[x * 3 + 1] = g;
						row[x * 3 + 2] = r;
					}
					fwrite(row, sizeof(row), 1, fp);
				}
				fclose(fp);
			}

			bool IsBlocked(int gx, int gy) const
			{
				if (gx < 0 || gx >= NAVGRID_W || gy < 0 || gy >= NAVGRID_H)
					return true;
				return m_grid[gx][gy] != 0;
			}

			bool IsRayClear(int x0, int y0, int x1, int y1) const
			{
				int dx = abs(x1 - x0);
				int dy = abs(y1 - y0);
				int sx = (x0 < x1) ? 1 : -1;
				int sy = (y0 < y1) ? 1 : -1;
				int err = dx - dy;
				int maxSteps = 2000;

				while (--maxSteps > 0)
				{
					if (IsBlocked(x0, y0))
						return false;

					if (x0 == x1 && y0 == y1)
						break;

					int e2 = 2 * err;
					if (e2 > -dy)
					{
						err -= dy;
						x0 += sx;
					}
					if (e2 < dx)
					{
						err += dx;
						y0 += sy;
					}
				}
				return true;
			}

			bool FindGlobalRoute(long startX, long startY, long targetX, long targetY, std::vector<PIXEL_POSITION>& outWaypoints)
			{
				outWaypoints.clear();
				if (!m_bInitialized)
					Init(21);

				if (!m_bInitialized)
				{
					PIXEL_POSITION p; p.x = targetX; p.y = targetY; p.z = 0;
					outWaypoints.push_back(p);
					return true;
				}

				int sgx = std::max(0, std::min(NAVGRID_W - 1, (int)(startX / NAVGRID_CELL)));
				int sgy = std::max(0, std::min(NAVGRID_H - 1, (int)((startY - NAVGRID_BASE_Y) / NAVGRID_CELL)));
				int tgx = std::max(0, std::min(NAVGRID_W - 1, (int)(targetX / NAVGRID_CELL)));
				int tgy = std::max(0, std::min(NAVGRID_H - 1, (int)((targetY - NAVGRID_BASE_Y) / NAVGRID_CELL)));

				if (sgx == tgx && sgy == tgy)
				{
					PIXEL_POSITION p; p.x = targetX; p.y = targetY; p.z = 0;
					outWaypoints.push_back(p);
					return true;
				}

				// If target cell is blocked, search for nearest walkable neighbor
				if (IsBlocked(tgx, tgy))
				{
					bool foundAlt = false;
					for (int r = 1; r <= 8 && !foundAlt; ++r)
					{
						for (int dy = -r; dy <= r && !foundAlt; ++dy)
						{
							for (int dx = -r; dx <= r && !foundAlt; ++dx)
							{
								if (!IsBlocked(tgx + dx, tgy + dy))
								{
									tgx += dx;
									tgy += dy;
									foundAlt = true;
								}
							}
						}
					}
					if (!foundAlt)
						return false;
				}

				static uint16_t s_nodeToken[NAVGRID_W][NAVGRID_H];
				static float s_nodeG[NAVGRID_W][NAVGRID_H];
				static short s_parentX[NAVGRID_W][NAVGRID_H];
				static short s_parentY[NAVGRID_W][NAVGRID_H];
				static uint16_t s_curToken = 0;

				++s_curToken;
				if (s_curToken == 0)
				{
					memset(s_nodeToken, 0, sizeof(s_nodeToken));
					s_curToken = 1;
				}

				struct TNode
				{
					float f;
					short x, y;
					bool operator > (const TNode& o) const { return f > o.f; }
				};

				std::priority_queue<TNode, std::vector<TNode>, std::greater<TNode> > openList;

				s_nodeG[sgx][sgy] = 0.0f;
				s_nodeToken[sgx][sgy] = s_curToken;
				s_parentX[sgx][sgy] = -1;
				s_parentY[sgx][sgy] = -1;

				TNode startNode;
				startNode.x = sgx; startNode.y = sgy;
				startNode.f = hypotf((float)(tgx - sgx), (float)(tgy - sgy));
				openList.push(startNode);

				const int dx[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
				const int dy[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
				const float cost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f };

				int bestX = sgx, bestY = sgy;
				float bestDist = 1e9f;
				int iterations = 0;
				const int MAX_ITERATIONS = 4000;

				while (!openList.empty() && ++iterations < MAX_ITERATIONS)
				{
					TNode cur = openList.top();
					openList.pop();

					if (cur.x == tgx && cur.y == tgy)
					{
						bestX = cur.x; bestY = cur.y;
						break;
					}

					float h = hypotf((float)(tgx - cur.x), (float)(tgy - cur.y));
					if (h < bestDist)
					{
						bestDist = h;
						bestX = cur.x;
						bestY = cur.y;
					}

					for (int i = 0; i < 8; ++i)
					{
						int nx = cur.x + dx[i];
						int ny = cur.y + dy[i];

						if (nx < 0 || nx >= NAVGRID_W || ny < 0 || ny >= NAVGRID_H)
							continue;

						if (m_grid[nx][ny] != 0)
							continue;

						float newG = s_nodeG[cur.x][cur.y] + cost[i];

						if (s_nodeToken[nx][ny] != s_curToken || newG < s_nodeG[nx][ny])
						{
							s_nodeToken[nx][ny] = s_curToken;
							s_nodeG[nx][ny] = newG;
							s_parentX[nx][ny] = cur.x;
							s_parentY[nx][ny] = cur.y;

							TNode nextNode;
							nextNode.x = nx;
							nextNode.y = ny;
							nextNode.f = newG + hypotf((float)(tgx - nx), (float)(tgy - ny));
							openList.push(nextNode);
						}
					}
				}

				// Backtrack raw grid path
				std::vector<std::pair<int, int> > rawPath;
				int cx = bestX, cy = bestY;
				int maxSteps = 2500;
				while (cx != -1 && cy != -1 && !(cx == sgx && cy == sgy) && --maxSteps > 0)
				{
					rawPath.push_back(std::make_pair(cx, cy));
					int px = s_parentX[cx][cy];
					int py = s_parentY[cx][cy];
					if (px == cx && py == cy)
						break;
					cx = px; cy = py;
				}
				rawPath.push_back(std::make_pair(sgx, sgy));
				std::reverse(rawPath.begin(), rawPath.end());

				if (rawPath.size() <= 1)
				{
					PIXEL_POSITION p; p.x = targetX; p.y = targetY; p.z = 0;
					outWaypoints.push_back(p);
					return true;
				}

				// String Pulling (LOS path simplification to corner waypoints)
				size_t curIdx = 0;
				int maxPulling = 200;
				while (curIdx < rawPath.size() - 1 && --maxPulling > 0)
				{
					size_t furthestIdx = curIdx + 1;
					for (size_t nextIdx = rawPath.size() - 1; nextIdx > curIdx; --nextIdx)
					{
						if (IsRayClear(rawPath[curIdx].first, rawPath[curIdx].second,
						               rawPath[nextIdx].first, rawPath[nextIdx].second))
						{
							furthestIdx = nextIdx;
							break;
						}
					}

					PIXEL_POSITION wp;
					if (furthestIdx >= rawPath.size() - 1)
					{
						wp.x = targetX;
						wp.y = targetY;
						wp.z = 0;
						outWaypoints.push_back(wp);
						break;
					}
					else
					{
						wp.x = rawPath[furthestIdx].first * NAVGRID_CELL + 100;
						wp.y = NAVGRID_BASE_Y + rawPath[furthestIdx].second * NAVGRID_CELL + 100;
						wp.z = 0;
						outWaypoints.push_back(wp);
					}
					curIdx = furthestIdx;
				}

				return !outWaypoints.empty();
			}

		private:
			bool m_bInitialized;
			BYTE m_grid[NAVGRID_W][NAVGRID_H];
	};

	bool MovePlayerBotLegacy(LPCHARACTER ch, long destX, long destY, DWORD dwNow, bool bForceUnstuck = false)
	{
		if (!ch || !ch->GetSectree())
			return false;

		const long curX = ch->GetX();
		const long curY = ch->GetY();
		const long mapIndex = ch->GetMapIndex();

		if (mapIndex == 21)
		{
			destX = std::max(22000L, std::min(96000L, destX));
			destY = std::max(110000L, std::min(220000L, destY));
		}

		TPlayerBotAIState& state = s_mapPlayerBotAIStates[ch->GetPlayerID()];

		// 1. Direct sprint: if direct line of sight to destination is clear, sprint directly!
		if (LegacyIsPlayerBotPathClear(mapIndex, curX, curY, destX, destY))
		{
			state.vecRoute.clear();
			state.uRouteIndex = 0;
			state.lRouteDestX = destX;
			state.lRouteDestY = destY;

			ch->SetRotationToXY(destX, destY);
			if (ch->Goto(destX, destY))
			{
				ch->SendMovePacket(FUNC_MOVE, 0, destX, destY, ch->GetCurrentMoveDuration(), dwNow);
				return true;
			}
			return false;
		}

		// 2. Direct path blocked: use Global 2D Grid Pathfinding
		const int destDrift = DISTANCE_APPROX(destX - state.lRouteDestX, destY - state.lRouteDestY);
		if (state.vecRoute.empty() || state.uRouteIndex >= state.vecRoute.size() || destDrift > 800)
		{
			state.vecRoute.clear();
			state.uRouteIndex = 0;
			state.lRouteDestX = destX;
			state.lRouteDestY = destY;
			CPlayerBotNavGrid::instance().FindGlobalRoute(curX, curY, destX, destY, state.vecRoute);
		}

		if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size())
		{
			PIXEL_POSITION targetWp = state.vecRoute[state.uRouteIndex];
			int distToWp = DISTANCE_APPROX(curX - targetWp.x, curY - targetWp.y);
			if (distToWp <= 400 && state.uRouteIndex + 1 < state.vecRoute.size())
			{
				++state.uRouteIndex;
				targetWp = state.vecRoute[state.uRouteIndex];
			}

			ch->SetRotationToXY(targetWp.x, targetWp.y);
			if (ch->Goto(targetWp.x, targetWp.y))
			{
				ch->SendMovePacket(FUNC_MOVE, 0, targetWp.x, targetWp.y, ch->GetCurrentMoveDuration(), dwNow);
				return true;
			}
		}

		// 3. Fallback: single step towards dest
		ch->SetRotationToXY(destX, destY);
		if (ch->Goto(destX, destY))
		{
			ch->SendMovePacket(FUNC_MOVE, 0, destX, destY, ch->GetCurrentMoveDuration(), dwNow);
			return true;
		}

		return false;
	}

	// ------------------------------------------------------------------------
	// Playerbot navigation v2
	//
	// The stock CHARACTER::Goto() does not perform collision checks.  Every
	// segment sent to it therefore has to be proven safe here first.  The old
	// implementation sampled a 200-unit grid at one point per cell and could
	// return partial A* paths as successful routes.  This implementation builds
	// a conservative 100-unit grid from all four native 50-unit server_attr
	// cells, labels connected components and only emits fully validated routes.
	// ------------------------------------------------------------------------

	// Plan at the native server_attr resolution.  Besides preserving narrow
	// bridges and walls, this guarantees that a character standing on a valid
	// native cell can always attach to the planning graph.
	const int PLAYERBOT_NAV_CELL = 50;
	const int PLAYERBOT_NAV_NATIVE_SAMPLE = 50;
	const int PLAYERBOT_NAV_CLUSTER_CELLS = 16;
	const int PLAYERBOT_NAV_MAX_PORTALS_PER_NEIGHBOR = 4;
	const int PLAYERBOT_NAV_MAX_SEGMENT = 700;
	// Must stay below half a native cell.  A wider threshold lets the bot skip a
	// corner waypoint before clearing the wall and then oscillate on replans.
	// The desktop client interpolates a MOVE packet slightly ahead of the
	// authoritative position. Waiting until the server is within only 25 units
	// (25 cm) of a waypoint made the next packet point briefly behind the model,
	// which looked like a one-metre back-step at every small direction change.
	// Switch segments with a modest look-ahead and tolerate small drift of a
	// moving target; SegmentClearWorld still validates every new segment.
	const int PLAYERBOT_NAV_ARRIVAL_DISTANCE = 100;
	const int PLAYERBOT_NAV_GOAL_REPLAN_DISTANCE = 400;
	// This is one global budget for the manager update, not one budget per map.
	// Giving M1, M2, M3 and the Monkey Dungeon 64 searches each multiplied the
	// old M1 load by four. Already built routes still advance every update; only
	// new expensive HPA/A* requests wait for a later staggered slot.
	const int PLAYERBOT_NAV_MAX_HEAVY_PLANS_PER_TICK = 32;
	const int PLAYERBOT_NAV_MAX_EXPANDED_NODES = 120000;
	DWORD s_dwPlayerBotNavBudgetStamp = 0;
	int s_iPlayerBotNavHeavyPlansThisTick = 0;

	enum EPlayerBotNavPlanResult
	{
		PLAYERBOT_NAV_PLAN_FOUND,
		PLAYERBOT_NAV_PLAN_DEFERRED,
		PLAYERBOT_NAV_PLAN_UNREACHABLE
	};

	// Declared in input_p2p.cpp. ChatPacket would be useless here - a bot has no
	// client descriptor of its own to send to.

	// Defined further down, next to the market-stall code; the refine routine
	// above needs it.
	enum EPlayerBotRaceSlot
	{
		PLAYERBOT_RACE_ANIMAL = 0,
		PLAYERBOT_RACE_UNDEAD,
		PLAYERBOT_RACE_DEVIL,
		PLAYERBOT_RACE_ORC,
		PLAYERBOT_RACE_MILGYO,
		PLAYERBOT_RACE_SLOTS,
		PLAYERBOT_RACE_NONE = -1
	};

	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, LPITEM item, int newPlus);

	// What the population has learned about each map: which kind of monster
	// actually lives there. Shared across every bot, because it is a fact about
	// the world rather than about any one character. Feeds equipment scoring, so
	// a race-attack bonus is worth more where that race is what you fight.
	struct TPlayerBotMapRaces
	{
		DWORD dwSamples;
		DWORD dwByRace[PLAYERBOT_RACE_SLOTS];
		TPlayerBotMapRaces() : dwSamples(0) { memset(dwByRace, 0, sizeof(dwByRace)); }
	};
	typedef std::map<long, TPlayerBotMapRaces> TPlayerBotMapRaceMap;
	TPlayerBotMapRaceMap s_mapRaceMemory;

	void RememberPlayerBotMapRace(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !target->IsMonster())
			return;
		TPlayerBotMapRaces& mem = s_mapRaceMemory[ch->GetMapIndex()];
		++mem.dwSamples;
		if (target->IsRaceFlag(RACE_FLAG_ANIMAL)) ++mem.dwByRace[PLAYERBOT_RACE_ANIMAL];
		if (target->IsRaceFlag(RACE_FLAG_UNDEAD)) ++mem.dwByRace[PLAYERBOT_RACE_UNDEAD];
		if (target->IsRaceFlag(RACE_FLAG_DEVIL))  ++mem.dwByRace[PLAYERBOT_RACE_DEVIL];
		if (target->IsRaceFlag(RACE_FLAG_ORC))    ++mem.dwByRace[PLAYERBOT_RACE_ORC];
		if (target->IsRaceFlag(RACE_FLAG_MILGYO)) ++mem.dwByRace[PLAYERBOT_RACE_MILGYO];
	}

	BYTE GetPlayerBotRaceApplyType(int race)
	{
		switch (race)
		{
			case PLAYERBOT_RACE_ANIMAL: return APPLY_ATTBONUS_ANIMAL;
			case PLAYERBOT_RACE_UNDEAD: return APPLY_ATTBONUS_UNDEAD;
			case PLAYERBOT_RACE_DEVIL:  return APPLY_ATTBONUS_DEVIL;
			case PLAYERBOT_RACE_ORC:    return APPLY_ATTBONUS_ORC;
			case PLAYERBOT_RACE_MILGYO: return APPLY_ATTBONUS_MILGYO;
			default: return APPLY_NONE;
		}
	}

	// The race a map is made of, or PLAYERBOT_RACE_NONE while the sample is too
	// small or too mixed to call. A guess made from ten kills is worse than none.
	int GetPlayerBotDominantRace(long mapIndex)
	{
		TPlayerBotMapRaceMap::const_iterator it = s_mapRaceMemory.find(mapIndex);
		if (it == s_mapRaceMemory.end() || it->second.dwSamples < 200)
			return PLAYERBOT_RACE_NONE;
		int best = PLAYERBOT_RACE_NONE;
		DWORD bestCount = 0;
		for (int race = 0; race < PLAYERBOT_RACE_SLOTS; ++race)
		{
			if (it->second.dwByRace[race] > bestCount)
			{
				bestCount = it->second.dwByRace[race];
				best = race;
			}
		}
		// Half the encounters have to agree before this counts as "the" race.
		return (bestCount * 2 >= it->second.dwSamples) ? best : PLAYERBOT_RACE_NONE;
	}

	DWORD PlayerBotNavHash(DWORD value)
	{
		value ^= value >> 16;
		value *= 0x7feb352dU;
		value ^= value >> 15;
		value *= 0x846ca68bU;
		value ^= value >> 16;
		return value;
	}

	BYTE GetPlayerBotStablePersonality(LPCHARACTER ch, BYTE role)
	{
		if (!ch)
			return BOT_PERSONALITY_STEADY_ADVENTURER;
		if (role == BOT_ROLE_PARTY_FIGHTER)
			return BOT_PERSONALITY_TEAM_COMPANION;
		if (role == BOT_ROLE_METIN_HUNTER)
			return BOT_PERSONALITY_METIN_BREAKER;

		switch (PlayerBotNavHash(ch->GetPlayerID() ^ 0x50524f46U) % 4U)
		{
			case 0: return BOT_PERSONALITY_GEAR_SPECIALIST;
			case 1: return BOT_PERSONALITY_CAREFUL_COLLECTOR;
			case 2: return BOT_PERSONALITY_WANDERER;
			default: return BOT_PERSONALITY_STEADY_ADVENTURER;
		}
	}

	BYTE GetPlayerBotStableAmbition(LPCHARACTER ch, BYTE personality)
	{
		if (!ch)
			return BOT_AMBITION_LEVEL;
		switch (personality)
		{
			case BOT_PERSONALITY_METIN_BREAKER:
				return BOT_AMBITION_METINS;
			case BOT_PERSONALITY_GEAR_SPECIALIST:
				return BOT_AMBITION_EQUIPMENT;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return BOT_AMBITION_BIOLOGIST;
			case BOT_PERSONALITY_WANDERER:
				return BOT_AMBITION_HORSE;
			case BOT_PERSONALITY_TEAM_COMPANION:
				return ch->GetJob() == JOB_SHAMAN
						? BOT_AMBITION_SKILLS : BOT_AMBITION_LEVEL;
			default:
				return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x414d4249U) % 5U) == 0
						? BOT_AMBITION_SKILLS : BOT_AMBITION_LEVEL;
		}
	}

	bool IsPlayerBotPositionBlocked(long lMapIndex, long x, long y)
	{
		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		if (!map)
			return true;

		const TMapSetting& setting = map->m_setting;
		if (x < setting.iBaseX || y < setting.iBaseY ||
			x >= setting.iBaseX + setting.iWidth || y >= setting.iBaseY + setting.iHeight)
			return true;

		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		if (!tree || !tree->GetAttributePtr())
			return true;

		return tree->IsAttr(x, y, ATTR_BLOCK | ATTR_WATER | ATTR_OBJECT);
	}

	bool IsPlayerBotSafeZone(long lMapIndex, long x, long y)
	{
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		return tree && tree->GetAttributePtr() && tree->IsAttr(x, y, ATTR_BANPK);
	}

	class CPlayerBotNavigation
	{
		private:
			struct TAbstractEdge
			{
				DWORD toRegion;
				int fromCell;
				int toCell;
				BYTE clearance;
			};

			struct TAbstractRegion
			{
				int clusterX;
				int clusterY;
				std::vector<TAbstractEdge> edges;
			};

		public:
			static CPlayerBotNavigation& instance(long mapIndex = PLAYERBOT_MAP_CHUNJO_M1)
			{
				// Each map owns its grid, component labels, HPA regions and per-tick
				// search budget. A single mutable instance would rebuild millions of
				// cells whenever updates alternated between M1, M2 and the dungeon.
				static std::map<long, CPlayerBotNavigation*> s_navigations;
				std::map<long, CPlayerBotNavigation*>::iterator it =
						s_navigations.find(mapIndex);
				if (it == s_navigations.end())
				{
					CPlayerBotNavigation* navigation = new CPlayerBotNavigation();
					s_navigations.insert(std::make_pair(mapIndex, navigation));
					return *navigation;
				}
				return *it->second;
			}

			CPlayerBotNavigation() :
				m_initialized(false),
				m_mapIndex(0),
				m_baseX(0),
				m_baseY(0),
				m_width(0),
				m_height(0),
				m_searchToken(0),
				m_regionSearchToken(0)
			{
			}

			bool Init(long mapIndex)
			{
				if (mapIndex != PLAYERBOT_MAP_CHUNJO_M1 &&
						mapIndex != PLAYERBOT_MAP_CHUNJO_M2 &&
						mapIndex != PLAYERBOT_MAP_CHUNJO_M3 &&
						mapIndex != PLAYERBOT_MAP_MONKEY_EASY &&
						mapIndex != PLAYERBOT_MAP_ORC_VALLEY &&
						mapIndex != PLAYERBOT_MAP_DESERT)
					return false;

				if (m_initialized && m_mapIndex == mapIndex)
					return true;

				LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
				if (!map)
					return false;

				const TMapSetting& setting = map->m_setting;
				if (setting.iWidth <= 0 || setting.iHeight <= 0)
					return false;

				m_initialized = false;
				m_mapIndex = mapIndex;
				m_baseX = setting.iBaseX;
				m_baseY = setting.iBaseY;
				m_width = (setting.iWidth + PLAYERBOT_NAV_CELL - 1) / PLAYERBOT_NAV_CELL;
				m_height = (setting.iHeight + PLAYERBOT_NAV_CELL - 1) / PLAYERBOT_NAV_CELL;

				const size_t cellCount = (size_t)m_width * (size_t)m_height;
				m_blocked.assign(cellCount, 1);
				m_clearance.assign(cellCount, 0);
				m_component.assign(cellCount, 0);

				size_t walkableCount = 0;
				for (int gy = 0; gy < m_height; ++gy)
				{
					for (int gx = 0; gx < m_width; ++gx)
					{
						bool blocked = false;
						const long cellX = m_baseX + gx * PLAYERBOT_NAV_CELL;
						const long cellY = m_baseY + gy * PLAYERBOT_NAV_CELL;

						// A planning cell is safe only if every native 50x50 cell
						// inside it is safe.  This prevents thin walls and shorelines
						// from disappearing through downsampling.
						for (int oy = PLAYERBOT_NAV_NATIVE_SAMPLE / 2;
								oy < PLAYERBOT_NAV_CELL && !blocked;
								oy += PLAYERBOT_NAV_NATIVE_SAMPLE)
						{
							for (int ox = PLAYERBOT_NAV_NATIVE_SAMPLE / 2;
									ox < PLAYERBOT_NAV_CELL;
									ox += PLAYERBOT_NAV_NATIVE_SAMPLE)
							{
								if (IsPlayerBotPositionBlocked(mapIndex, cellX + ox, cellY + oy))
								{
									blocked = true;
									break;
								}
							}
						}

						const int index = Index(gx, gy);
						m_blocked[index] = blocked ? 1 : 0;
						if (!blocked)
							++walkableCount;
					}
				}

				BuildClearance();
				const DWORD componentCount = BuildComponents();

				m_nodeToken.assign(cellCount, 0);
				m_nodeCost.assign(cellCount, 0);
				m_parent.assign(cellCount, -1);
				m_searchToken = 0;
				const DWORD abstractRegionCount = BuildAbstractRegions();
				m_initialized = true;

				sys_log(0, "PLAYERBOT_NAV: initialized map=%ld base=(%ld,%ld) grid=%dx%d cell=%d walkable=%u components=%u abstract_regions=%u",
						m_mapIndex, m_baseX, m_baseY, m_width, m_height, PLAYERBOT_NAV_CELL,
						(unsigned int)walkableCount, (unsigned int)componentCount,
						(unsigned int)abstractRegionCount);
				return true;
			}

			bool IsInitializedFor(long mapIndex) const
			{
				return m_initialized && m_mapIndex == mapIndex;
			}

			bool IsInsideWorld(long x, long y) const
			{
				return m_initialized && x >= m_baseX && y >= m_baseY &&
					x < m_baseX + m_width * PLAYERBOT_NAV_CELL &&
					y < m_baseY + m_height * PLAYERBOT_NAV_CELL;
			}

			void ClampWorld(long& x, long& y) const
			{
				if (!m_initialized)
					return;
				x = std::max(m_baseX + (long)PLAYERBOT_NAV_CELL,
						std::min(m_baseX + (long)m_width * PLAYERBOT_NAV_CELL - PLAYERBOT_NAV_CELL, x));
				y = std::max(m_baseY + (long)PLAYERBOT_NAV_CELL,
						std::min(m_baseY + (long)m_height * PLAYERBOT_NAV_CELL - PLAYERBOT_NAV_CELL, y));
			}

			bool IsBlockedCell(int gx, int gy) const
			{
				return !IsInsideCell(gx, gy) || m_blocked[Index(gx, gy)] != 0;
			}

			bool SegmentClearWorld(long x0, long y0, long x1, long y1) const
			{
				if (!m_initialized || !IsInsideWorld(x0, y0) || !IsInsideWorld(x1, y1))
					return false;

				int gx0, gy0, gx1, gy1;
				WorldToCell(x0, y0, gx0, gy0);
				WorldToCell(x1, y1, gx1, gy1);

				// Exact supercover traversal for arbitrary endpoints, not merely a
				// Bresenham line between cell centres.  Every native cell touched by
				// the geometric segment is checked against live sectree attributes so
				// even a very short corner crossing and a newly placed ATTR_OBJECT are
				// detected.
				const long deltaX = x1 - x0;
				const long deltaY = y1 - y0;
				const int stepX = deltaX > 0 ? 1 : (deltaX < 0 ? -1 : 0);
				const int stepY = deltaY > 0 ? 1 : (deltaY < 0 ? -1 : 0);
				const long long absDeltaX = llabs((long long)deltaX);
				const long long absDeltaY = llabs((long long)deltaY);
				int gx = gx0;
				int gy = gy0;

				if (IsLiveBlockedCell(gx, gy))
					return false;

				while (gx != gx1 || gy != gy1)
				{
					long long crossX = LLONG_MAX;
					long long crossY = LLONG_MAX;
					if (stepX != 0)
					{
						const long boundaryX = m_baseX +
							(stepX > 0 ? (gx + 1) * PLAYERBOT_NAV_CELL : gx * PLAYERBOT_NAV_CELL);
						crossX = llabs((long long)boundaryX - x0) * absDeltaY;
					}
					if (stepY != 0)
					{
						const long boundaryY = m_baseY +
							(stepY > 0 ? (gy + 1) * PLAYERBOT_NAV_CELL : gy * PLAYERBOT_NAV_CELL);
						crossY = llabs((long long)boundaryY - y0) * absDeltaX;
					}

					if (crossX == crossY)
					{
						// A geometric corner belongs to both side cells.  Requiring both
						// to be clear also forbids diagonal corner cutting.
						if ((stepX != 0 && IsLiveBlockedCell(gx + stepX, gy)) ||
							(stepY != 0 && IsLiveBlockedCell(gx, gy + stepY)))
							return false;
						gx += stepX;
						gy += stepY;
					}
					else if (crossX < crossY)
						gx += stepX;
					else
						gy += stepY;

					if (IsLiveBlockedCell(gx, gy))
						return false;
				}

				return true;
			}

			bool CanReach(long startX, long startY, long targetX, long targetY) const
			{
				if (!m_initialized || !IsInsideWorld(startX, startY) ||
						!IsInsideWorld(targetX, targetY))
					return false;

				int sx, sy, tx, ty;
				WorldToCell(startX, startY, sx, sy);
				WorldToCell(targetX, targetY, tx, ty);
				if (!FindNearestWalkableCell(sx, sy, 4, 0, 0))
					return false;
				const DWORD component = m_component[Index(sx, sy)];
				if (component == 0)
					return false;
				// Resolve the target on its own terrain first and only then compare
				// components. Searching directly for our component near the target
				// would incorrectly bridge a lake or wall.
				if (!FindNearestWalkableCell(tx, ty, 2, 0, 0))
					return false;
				return m_component[Index(tx, ty)] == component;
			}

			DWORD GetComponentAtWorld(long x, long y, int maxRadiusCells = 4) const
			{
				if (!m_initialized || !IsInsideWorld(x, y))
					return 0;
				int gx, gy;
				WorldToCell(x, y, gx, gy);
				if (!FindNearestWalkableCell(gx, gy, maxRadiusCells, 0, 0))
					return 0;
				return m_component[Index(gx, gy)];
			}

			bool FindNearestWalkableWorld(long x, long y, int maxRadiusCells,
					PIXEL_POSITION& out, DWORD seed = 0) const
			{
				if (!m_initialized)
					return false;
				int gx, gy;
				WorldToCell(x, y, gx, gy);
				if (!FindNearestWalkableCell(gx, gy, maxRadiusCells, 0, seed))
					return false;
				CellToWorld(gx, gy, out.x, out.y);
				out.z = 0;
				return true;
			}

			EPlayerBotNavPlanResult FindRoute(long startX, long startY, long targetX, long targetY,
					DWORD seed, DWORD now, int targetSnapRadius, bool flexibleTargetSnap,
					std::vector<PIXEL_POSITION>& outWaypoints)
			{
				outWaypoints.clear();
				if (!m_initialized || !IsInsideWorld(startX, startY) ||
						!IsInsideWorld(targetX, targetY))
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;

				if (s_dwPlayerBotNavBudgetStamp != now)
				{
					s_dwPlayerBotNavBudgetStamp = now;
					s_iPlayerBotNavHeavyPlansThisTick = 0;
				}
				if (s_iPlayerBotNavHeavyPlansThisTick >= PLAYERBOT_NAV_MAX_HEAVY_PLANS_PER_TICK)
					return PLAYERBOT_NAV_PLAN_DEFERRED;
				++s_iPlayerBotNavHeavyPlansThisTick;

				int sx, sy, tx, ty;
				WorldToCell(startX, startY, sx, sy);
				WorldToCell(targetX, targetY, tx, ty);
				if (!FindNearestWalkableCell(sx, sy, 4, 0, seed))
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;

				const DWORD component = m_component[Index(sx, sy)];
				if (component == 0)
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;
				if (flexibleTargetSnap)
				{
					if (!FindNearestWalkableCell(tx, ty, targetSnapRadius, component,
							seed ^ 0x9e3779b9U))
					{
						sys_log(1, "PLAYERBOT_NAV: goal snap failed map=%ld from=(%ld,%ld) to=(%ld,%ld) component=%u radius=%d",
								m_mapIndex, startX, startY, targetX, targetY,
								(unsigned int)component, targetSnapRadius);
						return PLAYERBOT_NAV_PLAN_UNREACHABLE;
					}
				}
				else
				{
					if (!FindNearestWalkableCell(tx, ty, targetSnapRadius, 0,
							seed ^ 0x9e3779b9U))
					{
						sys_log(1, "PLAYERBOT_NAV: strict goal snap failed map=%ld from=(%ld,%ld) to=(%ld,%ld) radius=%d",
								m_mapIndex, startX, startY, targetX, targetY, targetSnapRadius);
						return PLAYERBOT_NAV_PLAN_UNREACHABLE;
					}
					if (m_component[Index(tx, ty)] != component)
					{
						sys_log(1, "PLAYERBOT_NAV: disconnected goal map=%ld from=(%ld,%ld) to=(%ld,%ld) start_component=%u target_component=%u",
								m_mapIndex, startX, startY, targetX, targetY,
								(unsigned int)component, (unsigned int)m_component[Index(tx, ty)]);
						return PLAYERBOT_NAV_PLAN_UNREACHABLE;
					}
				}

				const int startIndex = Index(sx, sy);
				const int targetIndex = Index(tx, ty);
				if (startIndex == targetIndex)
				{
					PIXEL_POSITION point;
					if (!IsPlayerBotPositionBlocked(m_mapIndex, targetX, targetY))
					{
						point.x = targetX;
						point.y = targetY;
					}
					else
						CellToWorld(tx, ty, point.x, point.y);
					point.z = 0;
					outWaypoints.push_back(point);
					return PLAYERBOT_NAV_PLAN_FOUND;
				}

				std::vector<int> rawPath;
				if (!FindHierarchicalRawPath(startIndex, targetIndex, seed, rawPath))
				{
					sys_log(1, "PLAYERBOT_NAV: hierarchical route failed map=%ld from=(%ld,%ld) to=(%ld,%ld) start_region=%u target_region=%u",
							m_mapIndex, startX, startY, targetX, targetY,
							(unsigned int)m_cellRegion[startIndex],
							(unsigned int)m_cellRegion[targetIndex]);
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;
				}

#if 0
				// Retired fine-grid A*.  Kept temporarily beside the HPA rollout so a
				// runtime comparison can be made without restoring an old source file.
				++m_searchToken;
				if (m_searchToken == 0)
				{
					std::fill(m_nodeToken.begin(), m_nodeToken.end(), 0);
					m_searchToken = 1;
				}

				struct TOpenNode
				{
					int f;
					int g;
					int x;
					int y;
					DWORD tie;
				};
				struct TOpenNodeGreater
				{
					bool operator()(const TOpenNode& left, const TOpenNode& right) const
					{
						if (left.f != right.f)
							return left.f > right.f;
						return left.tie > right.tie;
					}
				};

				std::priority_queue<TOpenNode, std::vector<TOpenNode>, TOpenNodeGreater> open;
				m_nodeToken[startIndex] = m_searchToken;
				m_nodeCost[startIndex] = 0;
				m_parent[startIndex] = -1;
				TOpenNode first;
				first.g = 0;
				// Weighted A*: terrain safety is binary and revalidated later, so a
				// modestly greedier heuristic trades only route optimality for a very
				// large reduction in heap work on this five-million-cell map.
				first.f = OctileDistance(sx, sy, tx, ty) * 2;
				first.x = sx;
				first.y = sy;
				first.tie = PlayerBotNavHash(seed ^ (DWORD)startIndex);
				open.push(first);

				const int moveX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const int moveY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
				const int moveCost[8] = { 10, 14, 10, 14, 10, 14, 10, 14 };
				bool found = false;
				int expanded = 0;

				while (!open.empty() && expanded < PLAYERBOT_NAV_MAX_EXPANDED_NODES)
				{
					const TOpenNode current = open.top();
					open.pop();
					const int currentIndex = Index(current.x, current.y);
					if (m_nodeToken[currentIndex] != m_searchToken || current.g != m_nodeCost[currentIndex])
						continue;
					++expanded;

					if (currentIndex == targetIndex)
					{
						found = true;
						break;
					}

					const int directionOffset = (int)(PlayerBotNavHash(seed) & 7U);
					for (int n = 0; n < 8; ++n)
					{
						const int direction = (directionOffset + n) & 7;
						const int nx = current.x + moveX[direction];
						const int ny = current.y + moveY[direction];
						if (IsBlockedCell(nx, ny))
							continue;

						// Never pass diagonally through the corner of two obstacles.
						if (moveX[direction] != 0 && moveY[direction] != 0 &&
								(IsBlockedCell(current.x + moveX[direction], current.y) ||
								 IsBlockedCell(current.x, current.y + moveY[direction])))
							continue;

						const int nextIndex = Index(nx, ny);
						if (m_component[nextIndex] != component)
							continue;

						int wallPenalty = 0;
						if (m_clearance[nextIndex] <= 1) wallPenalty = 8;
						else if (m_clearance[nextIndex] == 2) wallPenalty = 3;
						else if (m_clearance[nextIndex] == 3) wallPenalty = 1;
						const int laneJitter = (int)(PlayerBotNavHash(seed ^ (DWORD)nextIndex) & 1U);
						const int newCost = current.g + moveCost[direction] + wallPenalty + laneJitter;

						if (m_nodeToken[nextIndex] == m_searchToken && newCost >= m_nodeCost[nextIndex])
							continue;

						m_nodeToken[nextIndex] = m_searchToken;
						m_nodeCost[nextIndex] = newCost;
						m_parent[nextIndex] = currentIndex;

						TOpenNode next;
						next.g = newCost;
						next.f = newCost + OctileDistance(nx, ny, tx, ty) * 2;
						next.x = nx;
						next.y = ny;
						next.tie = PlayerBotNavHash(seed ^ (DWORD)nextIndex);
						open.push(next);
					}
				}

				// A search that consumed its node cap must not restart the identical
				// first 120k nodes forever.  Treat it as a bounded failure; the caller
				// can choose another goal while the one-plan-per-tick budget protects
				// the game loop from a CPU spike.
				if (!found)
				{
					sys_err("PLAYERBOT_NAV: bounded search failed map=%ld from=(%ld,%ld) to=(%ld,%ld) expanded=%d frontier=%u",
							m_mapIndex, startX, startY, targetX, targetY, expanded,
							(unsigned int)open.size());
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;
				}

				std::vector<int> rawPath;
				int cursor = targetIndex;
				while (cursor >= 0)
				{
					rawPath.push_back(cursor);
					if (cursor == startIndex)
						break;
					cursor = m_parent[cursor];
				}
				if (rawPath.empty() || rawPath.back() != startIndex)
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;
				std::reverse(rawPath.begin(), rawPath.end());
#endif

				// Conservative string pulling.  Each emitted segment is short and
				// crosses only cells proven safe by a supercover test.  The first
				// segment is special: the character can stand anywhere inside the
				// start cell, not necessarily at its centre.  Validating only centre
				// to centre could therefore create a route whose first waypoint was
				// rejected forever by MovePlayerBot (most visibly near town walls).
				const size_t maxCellsPerSegment = std::max(1, PLAYERBOT_NAV_MAX_SEGMENT / PLAYERBOT_NAV_CELL);
				size_t pathIndex = 0;
				bool validateFromExactStart = true;
				while (pathIndex + 1 < rawPath.size())
				{
					const size_t limit = std::min(rawPath.size() - 1, pathIndex + maxCellsPerSegment);
					size_t furthest = pathIndex;
					int fromX, fromY;
					CellFromIndex(rawPath[pathIndex], fromX, fromY);
					for (size_t candidate = limit; candidate > pathIndex; --candidate)
					{
						int toX, toY;
						CellFromIndex(rawPath[candidate], toX, toY);
						bool clear = false;
						if (validateFromExactStart)
						{
							int toWorldX, toWorldY;
							CellToWorld(toX, toY, toWorldX, toWorldY);
							clear = SegmentClearWorld(startX, startY, toWorldX, toWorldY);
						}
						else
							clear = SegmentClearCells(fromX, fromY, toX, toY);
						if (clear)
						{
							furthest = candidate;
							break;
						}
					}

					if (furthest == pathIndex)
					{
						// The raw path itself is valid, but the exact point inside its
						// first cell may need a tiny alignment move before the first
						// corner can be rounded safely.  Emit that centre explicitly;
						// subsequent segments can then use ordinary cell validation.
						if (!validateFromExactStart)
							return PLAYERBOT_NAV_PLAN_UNREACHABLE;
						PIXEL_POSITION alignment;
						CellToWorld(fromX, fromY, alignment.x, alignment.y);
						alignment.z = 0;
						if (!SegmentClearWorld(startX, startY, alignment.x, alignment.y))
							return PLAYERBOT_NAV_PLAN_UNREACHABLE;
						outWaypoints.push_back(alignment);
						validateFromExactStart = false;
						continue;
					}

					int waypointX, waypointY;
					CellFromIndex(rawPath[furthest], waypointX, waypointY);
					PIXEL_POSITION point;
					CellToWorld(waypointX, waypointY, point.x, point.y);
					point.z = 0;
					outWaypoints.push_back(point);
					pathIndex = furthest;
					validateFromExactStart = false;
				}

				if (outWaypoints.empty())
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;

				// Preserve an exact movable destination only when it lies in the
				// selected goal cell and the last tiny segment remains valid.
				int originalTargetX, originalTargetY;
				WorldToCell(targetX, targetY, originalTargetX, originalTargetY);
				PIXEL_POSITION& last = outWaypoints.back();
				if (originalTargetX == tx && originalTargetY == ty &&
						!IsPlayerBotPositionBlocked(m_mapIndex, targetX, targetY) &&
						SegmentClearWorld(last.x, last.y, targetX, targetY))
				{
					last.x = targetX;
					last.y = targetY;
				}

				return PLAYERBOT_NAV_PLAN_FOUND;
			}

		private:
			int Index(int gx, int gy) const
			{
				return gy * m_width + gx;
			}

			bool IsInsideCell(int gx, int gy) const
			{
				return gx >= 0 && gy >= 0 && gx < m_width && gy < m_height;
			}

			bool IsLiveBlockedCell(int gx, int gy) const
			{
				if (!IsInsideCell(gx, gy))
					return true;
				const long x = m_baseX + gx * PLAYERBOT_NAV_CELL + PLAYERBOT_NAV_CELL / 2;
				const long y = m_baseY + gy * PLAYERBOT_NAV_CELL + PLAYERBOT_NAV_CELL / 2;
				return IsPlayerBotPositionBlocked(m_mapIndex, x, y);
			}

			void WorldToCell(long x, long y, int& gx, int& gy) const
			{
				gx = (int)((x - m_baseX) / PLAYERBOT_NAV_CELL);
				gy = (int)((y - m_baseY) / PLAYERBOT_NAV_CELL);
				gx = std::max(0, std::min(m_width - 1, gx));
				gy = std::max(0, std::min(m_height - 1, gy));
			}

			void CellToWorld(int gx, int gy, int& x, int& y) const
			{
				x = (int)(m_baseX + gx * PLAYERBOT_NAV_CELL + PLAYERBOT_NAV_CELL / 2);
				y = (int)(m_baseY + gy * PLAYERBOT_NAV_CELL + PLAYERBOT_NAV_CELL / 2);
			}

			void CellFromIndex(int index, int& gx, int& gy) const
			{
				gx = index % m_width;
				gy = index / m_width;
			}

			int OctileDistance(int x0, int y0, int x1, int y1) const
			{
				const int dx = abs(x1 - x0);
				const int dy = abs(y1 - y0);
				const int diagonal = std::min(dx, dy);
				return 10 * (dx + dy) - 6 * diagonal;
			}

			bool SegmentClearCells(int x0, int y0, int x1, int y1) const
			{
				if (IsBlockedCell(x0, y0) || IsBlockedCell(x1, y1) ||
						IsLiveBlockedCell(x0, y0) || IsLiveBlockedCell(x1, y1))
					return false;

				const int dx = x1 - x0;
				const int dy = y1 - y0;
				const int nx = abs(dx);
				const int ny = abs(dy);
				const int signX = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
				const int signY = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
				int x = x0;
				int y = y0;
				int ix = 0;
				int iy = 0;

				while (ix < nx || iy < ny)
				{
					const long long decisionX = (long long)(1 + 2 * ix) * ny;
					const long long decisionY = (long long)(1 + 2 * iy) * nx;
					if (decisionX == decisionY)
					{
						// The line crosses a cell corner: both side cells must be
						// clear, otherwise this would be diagonal corner cutting.
						if ((signX != 0 && (IsBlockedCell(x + signX, y) ||
								IsLiveBlockedCell(x + signX, y))) ||
								(signY != 0 && (IsBlockedCell(x, y + signY) ||
								IsLiveBlockedCell(x, y + signY))))
							return false;
						x += signX;
						y += signY;
						++ix;
						++iy;
					}
					else if (decisionX < decisionY)
					{
						x += signX;
						++ix;
					}
					else
					{
						y += signY;
						++iy;
					}

					if (IsBlockedCell(x, y) || IsLiveBlockedCell(x, y))
						return false;
				}
				return true;
			}

			bool FindNearestWalkableCell(int& gx, int& gy, int maxRadius,
					DWORD requiredComponent, DWORD seed) const
			{
				const int originX = gx;
				const int originY = gy;
				for (int radius = 0; radius <= maxRadius; ++radius)
				{
					bool found = false;
					DWORD bestTie = 0xffffffffU;
					int bestX = originX;
					int bestY = originY;
					for (int y = originY - radius; y <= originY + radius; ++y)
					{
						for (int x = originX - radius; x <= originX + radius; ++x)
						{
							if (std::max(abs(x - originX), abs(y - originY)) != radius ||
									IsBlockedCell(x, y) || IsLiveBlockedCell(x, y))
								continue;
							const int index = Index(x, y);
							if (requiredComponent != 0 && m_component[index] != requiredComponent)
								continue;
							const DWORD tie = PlayerBotNavHash(seed ^ (DWORD)index);
							if (!found || tie < bestTie)
							{
								found = true;
								bestTie = tie;
								bestX = x;
								bestY = y;
							}
						}
					}
					if (found)
					{
						gx = bestX;
						gy = bestY;
						return true;
					}
				}
				return false;
			}

			void BuildClearance()
			{
				for (int y = 0; y < m_height; ++y)
				{
					for (int x = 0; x < m_width; ++x)
					{
						const int index = Index(x, y);
						m_clearance[index] = m_blocked[index] ? 0 : 4;
						if (m_blocked[index])
							continue;
						if (x > 0) m_clearance[index] = std::min<BYTE>(m_clearance[index], (BYTE)(m_clearance[Index(x - 1, y)] + 1));
						if (y > 0) m_clearance[index] = std::min<BYTE>(m_clearance[index], (BYTE)(m_clearance[Index(x, y - 1)] + 1));
					}
				}
				for (int y = m_height - 1; y >= 0; --y)
				{
					for (int x = m_width - 1; x >= 0; --x)
					{
						const int index = Index(x, y);
						if (m_blocked[index])
							continue;
						if (x + 1 < m_width) m_clearance[index] = std::min<BYTE>(m_clearance[index], (BYTE)(m_clearance[Index(x + 1, y)] + 1));
						if (y + 1 < m_height) m_clearance[index] = std::min<BYTE>(m_clearance[index], (BYTE)(m_clearance[Index(x, y + 1)] + 1));
					}
				}
			}

			DWORD BuildComponents()
			{
				DWORD component = 0;
				std::vector<int> queue;
				const int moveX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const int moveY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

				for (int y = 0; y < m_height; ++y)
				{
					for (int x = 0; x < m_width; ++x)
					{
						const int firstIndex = Index(x, y);
						if (m_blocked[firstIndex] || m_component[firstIndex] != 0)
							continue;

						++component;
						queue.clear();
						queue.push_back(firstIndex);
						m_component[firstIndex] = component;
						for (size_t head = 0; head < queue.size(); ++head)
						{
							int cx, cy;
							CellFromIndex(queue[head], cx, cy);
							for (int direction = 0; direction < 8; ++direction)
							{
								const int nx = cx + moveX[direction];
								const int ny = cy + moveY[direction];
								if (IsBlockedCell(nx, ny))
									continue;
								if (moveX[direction] != 0 && moveY[direction] != 0 &&
										(IsBlockedCell(cx + moveX[direction], cy) || IsBlockedCell(cx, cy + moveY[direction])))
									continue;
								const int nextIndex = Index(nx, ny);
								if (m_component[nextIndex] != 0)
									continue;
								m_component[nextIndex] = component;
								queue.push_back(nextIndex);
							}
						}
					}
				}
				return component;
			}

			void AddAbstractPortal(DWORD fromRegion, DWORD toRegion,
					int fromCell, int toCell)
			{
				if (fromRegion == 0 || toRegion == 0 || fromRegion == toRegion ||
						fromRegion >= m_regions.size() || toRegion >= m_regions.size())
					return;

				TAbstractRegion& region = m_regions[fromRegion];
				const BYTE clearance = std::min(m_clearance[fromCell], m_clearance[toCell]);
				int candidateX, candidateY;
				CellFromIndex(fromCell, candidateX, candidateY);
				int sameNeighbourCount = 0;
				int worstEdge = -1;
				BYTE worstClearance = 255;

				for (size_t i = 0; i < region.edges.size(); ++i)
				{
					TAbstractEdge& existing = region.edges[i];
					if (existing.toRegion != toRegion)
						continue;
					++sameNeighbourCount;
					int existingX, existingY;
					CellFromIndex(existing.fromCell, existingX, existingY);
					if (abs(existingX - candidateX) + abs(existingY - candidateY) < 4)
					{
						if (clearance > existing.clearance)
						{
							existing.fromCell = fromCell;
							existing.toCell = toCell;
							existing.clearance = clearance;
						}
						return;
					}
					if (existing.clearance < worstClearance)
					{
						worstClearance = existing.clearance;
						worstEdge = (int)i;
					}
				}

				TAbstractEdge edge;
				edge.toRegion = toRegion;
				edge.fromCell = fromCell;
				edge.toCell = toCell;
				edge.clearance = clearance;
				if (sameNeighbourCount < PLAYERBOT_NAV_MAX_PORTALS_PER_NEIGHBOR)
					region.edges.push_back(edge);
				else if (worstEdge >= 0 && clearance > worstClearance)
					region.edges[worstEdge] = edge;
			}

			DWORD BuildAbstractRegions()
			{
				const size_t cellCount = (size_t)m_width * (size_t)m_height;
				m_cellRegion.assign(cellCount, 0);
				m_regions.clear();
				TAbstractRegion unused;
				unused.clusterX = -1;
				unused.clusterY = -1;
				m_regions.push_back(unused);

				const int moveX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const int moveY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
				std::vector<int> queue;
				queue.reserve(PLAYERBOT_NAV_CLUSTER_CELLS * PLAYERBOT_NAV_CLUSTER_CELLS);
				const int clusterWidth = (m_width + PLAYERBOT_NAV_CLUSTER_CELLS - 1) /
						PLAYERBOT_NAV_CLUSTER_CELLS;
				const int clusterHeight = (m_height + PLAYERBOT_NAV_CLUSTER_CELLS - 1) /
						PLAYERBOT_NAV_CLUSTER_CELLS;

				for (int clusterY = 0; clusterY < clusterHeight; ++clusterY)
				{
					const int minY = clusterY * PLAYERBOT_NAV_CLUSTER_CELLS;
					const int maxY = std::min(m_height, minY + PLAYERBOT_NAV_CLUSTER_CELLS);
					for (int clusterX = 0; clusterX < clusterWidth; ++clusterX)
					{
						const int minX = clusterX * PLAYERBOT_NAV_CLUSTER_CELLS;
						const int maxX = std::min(m_width, minX + PLAYERBOT_NAV_CLUSTER_CELLS);
						for (int y = minY; y < maxY; ++y)
						{
							for (int x = minX; x < maxX; ++x)
							{
								const int firstCell = Index(x, y);
								if (m_blocked[firstCell] || m_cellRegion[firstCell] != 0)
									continue;

								const DWORD regionID = (DWORD)m_regions.size();
								TAbstractRegion region;
								region.clusterX = clusterX;
								region.clusterY = clusterY;
								m_regions.push_back(region);
								queue.clear();
								queue.push_back(firstCell);
								m_cellRegion[firstCell] = regionID;

								for (size_t head = 0; head < queue.size(); ++head)
								{
									int currentX, currentY;
									CellFromIndex(queue[head], currentX, currentY);
									for (int direction = 0; direction < 8; ++direction)
									{
										const int nextX = currentX + moveX[direction];
										const int nextY = currentY + moveY[direction];
										if (nextX < minX || nextX >= maxX || nextY < minY || nextY >= maxY ||
												IsBlockedCell(nextX, nextY))
											continue;
										if (moveX[direction] != 0 && moveY[direction] != 0 &&
												(IsBlockedCell(currentX + moveX[direction], currentY) ||
												 IsBlockedCell(currentX, currentY + moveY[direction])))
											continue;
										const int nextCell = Index(nextX, nextY);
										if (m_cellRegion[nextCell] != 0)
											continue;
										m_cellRegion[nextCell] = regionID;
										queue.push_back(nextCell);
									}
								}
							}
						}
					}
				}

				// Every cardinal crossing of a cluster border is a portal candidate.
				// AddAbstractPortal retains several spatially separated alternatives
				// per region pair so bots do not all funnel through one arbitrary cell.
				for (int y = 0; y < m_height; ++y)
				{
					for (int x = 0; x < m_width; ++x)
					{
						const int cell = Index(x, y);
						if (m_blocked[cell])
							continue;
						if (x + 1 < m_width && (x + 1) % PLAYERBOT_NAV_CLUSTER_CELLS == 0)
						{
							const int other = Index(x + 1, y);
							if (!m_blocked[other])
							{
								AddAbstractPortal(m_cellRegion[cell], m_cellRegion[other], cell, other);
								AddAbstractPortal(m_cellRegion[other], m_cellRegion[cell], other, cell);
							}
						}
						if (y + 1 < m_height && (y + 1) % PLAYERBOT_NAV_CLUSTER_CELLS == 0)
						{
							const int other = Index(x, y + 1);
							if (!m_blocked[other])
							{
								AddAbstractPortal(m_cellRegion[cell], m_cellRegion[other], cell, other);
								AddAbstractPortal(m_cellRegion[other], m_cellRegion[cell], other, cell);
							}
						}
					}
				}

				m_regionToken.assign(m_regions.size(), 0);
				m_regionCost.assign(m_regions.size(), 0);
				m_regionParent.assign(m_regions.size(), -1);
				m_regionParentEdge.assign(m_regions.size(), -1);
				m_regionSearchToken = 0;
				return (DWORD)(m_regions.size() - 1);
			}

			uint16_t NextCellSearchToken()
			{
				++m_searchToken;
				if (m_searchToken == 0)
				{
					std::fill(m_nodeToken.begin(), m_nodeToken.end(), 0);
					m_searchToken = 1;
				}
				return m_searchToken;
			}

			bool AppendLocalRegionPath(int startCell, int targetCell, DWORD regionID,
					DWORD seed, std::vector<int>& path)
			{
				if (startCell < 0 || targetCell < 0 || regionID == 0 ||
						m_cellRegion[startCell] != regionID || m_cellRegion[targetCell] != regionID)
					return false;

				if (startCell == targetCell)
				{
					if (path.empty() || path.back() != startCell)
						path.push_back(startCell);
					return true;
				}

				const uint16_t token = NextCellSearchToken();
				std::vector<int> queue;
				queue.reserve(PLAYERBOT_NAV_CLUSTER_CELLS * PLAYERBOT_NAV_CLUSTER_CELLS);
				queue.push_back(startCell);
				m_nodeToken[startCell] = token;
				m_parent[startCell] = -1;

				const int moveX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const int moveY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
				const int directionOffset = (int)(PlayerBotNavHash(seed ^ regionID) & 7U);
				bool found = false;
				for (size_t head = 0; head < queue.size() && !found; ++head)
				{
					int currentX, currentY;
					CellFromIndex(queue[head], currentX, currentY);
					for (int n = 0; n < 8; ++n)
					{
						const int direction = (directionOffset + n) & 7;
						const int nextX = currentX + moveX[direction];
						const int nextY = currentY + moveY[direction];
						if (IsBlockedCell(nextX, nextY) || IsLiveBlockedCell(nextX, nextY))
							continue;
						if (moveX[direction] != 0 && moveY[direction] != 0 &&
								(IsLiveBlockedCell(currentX + moveX[direction], currentY) ||
								 IsLiveBlockedCell(currentX, currentY + moveY[direction])))
							continue;
						const int nextCell = Index(nextX, nextY);
						if (m_cellRegion[nextCell] != regionID || m_nodeToken[nextCell] == token)
							continue;
						m_nodeToken[nextCell] = token;
						m_parent[nextCell] = queue[head];
						queue.push_back(nextCell);
						if (nextCell == targetCell)
						{
							found = true;
							break;
						}
					}
				}

				if (!found)
					return false;

				std::vector<int> localPath;
				for (int cursor = targetCell; cursor >= 0; cursor = m_parent[cursor])
				{
					localPath.push_back(cursor);
					if (cursor == startCell)
						break;
				}
				if (localPath.empty() || localPath.back() != startCell)
					return false;
				std::reverse(localPath.begin(), localPath.end());
				for (size_t i = path.empty() ? 0 : 1; i < localPath.size(); ++i)
					path.push_back(localPath[i]);
				return true;
			}

			int AbstractHeuristic(DWORD fromRegion, DWORD toRegion) const
			{
				const TAbstractRegion& from = m_regions[fromRegion];
				const TAbstractRegion& to = m_regions[toRegion];
				return 10 * (abs(from.clusterX - to.clusterX) + abs(from.clusterY - to.clusterY));
			}

			bool FindFinePathInRegionCorridor(int startCell, int targetCell,
					const std::vector<DWORD>& corridor, DWORD seed, std::vector<int>& path)
			{
				path.clear();
				if (corridor.empty())
					return false;
				std::vector<BYTE> allowed(m_regions.size(), 0);
				for (size_t i = 0; i < corridor.size(); ++i)
				{
					if (corridor[i] == 0 || corridor[i] >= allowed.size())
						return false;
					allowed[corridor[i]] = 1;
				}

				const uint16_t token = NextCellSearchToken();
				struct TFineOpenNode
				{
					int f;
					int g;
					int cell;
					DWORD tie;
				};
				struct TFineOpenGreater
				{
					bool operator()(const TFineOpenNode& left, const TFineOpenNode& right) const
					{
						if (left.f != right.f)
							return left.f > right.f;
						return left.tie > right.tie;
					}
				};

				int targetX, targetY;
				CellFromIndex(targetCell, targetX, targetY);
				std::priority_queue<TFineOpenNode, std::vector<TFineOpenNode>, TFineOpenGreater> open;
				m_nodeToken[startCell] = token;
				m_nodeCost[startCell] = 0;
				m_parent[startCell] = -1;
				int startX, startY;
				CellFromIndex(startCell, startX, startY);
				TFineOpenNode first;
				first.g = 0;
				first.f = OctileDistance(startX, startY, targetX, targetY) * 2;
				first.cell = startCell;
				first.tie = PlayerBotNavHash(seed ^ (DWORD)startCell);
				open.push(first);

				const int moveX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const int moveY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
				const int moveCost[8] = { 10, 14, 10, 14, 10, 14, 10, 14 };
				const int directionOffset = (int)(PlayerBotNavHash(seed) & 7U);
				bool found = false;
				while (!open.empty())
				{
					const TFineOpenNode current = open.top();
					open.pop();
					if (m_nodeToken[current.cell] != token || m_nodeCost[current.cell] != current.g)
						continue;
					if (current.cell == targetCell)
					{
						found = true;
						break;
					}

					int currentX, currentY;
					CellFromIndex(current.cell, currentX, currentY);
					for (int n = 0; n < 8; ++n)
					{
						const int direction = (directionOffset + n) & 7;
						const int nextX = currentX + moveX[direction];
						const int nextY = currentY + moveY[direction];
						if (IsBlockedCell(nextX, nextY) || IsLiveBlockedCell(nextX, nextY))
							continue;
						if (moveX[direction] != 0 && moveY[direction] != 0 &&
								(IsBlockedCell(currentX + moveX[direction], currentY) ||
								 IsBlockedCell(currentX, currentY + moveY[direction]) ||
								 IsLiveBlockedCell(currentX + moveX[direction], currentY) ||
								 IsLiveBlockedCell(currentX, currentY + moveY[direction])))
							continue;
						const int nextCell = Index(nextX, nextY);
						const DWORD nextRegion = m_cellRegion[nextCell];
						if (nextRegion == 0 || nextRegion >= allowed.size() || !allowed[nextRegion])
							continue;

						int wallPenalty = 0;
						if (m_clearance[nextCell] <= 1) wallPenalty = 8;
						else if (m_clearance[nextCell] == 2) wallPenalty = 3;
						else if (m_clearance[nextCell] == 3) wallPenalty = 1;
						const int laneJitter = (int)(PlayerBotNavHash(seed ^ (DWORD)nextCell) & 1U);
						const int newCost = current.g + moveCost[direction] + wallPenalty + laneJitter;
						if (m_nodeToken[nextCell] == token && newCost >= m_nodeCost[nextCell])
							continue;

						m_nodeToken[nextCell] = token;
						m_nodeCost[nextCell] = newCost;
						m_parent[nextCell] = current.cell;
						TFineOpenNode next;
						next.g = newCost;
						next.f = newCost + OctileDistance(nextX, nextY, targetX, targetY) * 2;
						next.cell = nextCell;
						next.tie = PlayerBotNavHash(seed ^ (DWORD)nextCell);
						open.push(next);
					}
				}

				if (!found)
					return false;
				for (int cursor = targetCell; cursor >= 0; cursor = m_parent[cursor])
				{
					path.push_back(cursor);
					if (cursor == startCell)
						break;
				}
				if (path.empty() || path.back() != startCell)
					return false;
				std::reverse(path.begin(), path.end());
				return true;
			}

			bool FindHierarchicalRawPath(int startCell, int targetCell, DWORD seed,
					std::vector<int>& path)
			{
				path.clear();
				if (startCell < 0 || targetCell < 0)
					return false;
				const DWORD startRegion = m_cellRegion[startCell];
				const DWORD targetRegion = m_cellRegion[targetCell];
				if (startRegion == 0 || targetRegion == 0)
					return false;
				if (startRegion == targetRegion)
					return AppendLocalRegionPath(startCell, targetCell, startRegion, seed, path);

				++m_regionSearchToken;
				if (m_regionSearchToken == 0)
				{
					std::fill(m_regionToken.begin(), m_regionToken.end(), 0);
					m_regionSearchToken = 1;
				}

				struct TRegionOpenNode
				{
					int f;
					int g;
					DWORD region;
					DWORD tie;
				};
				struct TRegionOpenGreater
				{
					bool operator()(const TRegionOpenNode& left, const TRegionOpenNode& right) const
					{
						if (left.f != right.f)
							return left.f > right.f;
						return left.tie > right.tie;
					}
				};

				std::priority_queue<TRegionOpenNode, std::vector<TRegionOpenNode>, TRegionOpenGreater> open;
				m_regionToken[startRegion] = m_regionSearchToken;
				m_regionCost[startRegion] = 0;
				m_regionParent[startRegion] = -1;
				m_regionParentEdge[startRegion] = -1;
				TRegionOpenNode first;
				first.g = 0;
				first.f = AbstractHeuristic(startRegion, targetRegion);
				first.region = startRegion;
				first.tie = PlayerBotNavHash(seed ^ startRegion);
				open.push(first);
				bool found = false;

				while (!open.empty())
				{
					const TRegionOpenNode current = open.top();
					open.pop();
					if (m_regionToken[current.region] != m_regionSearchToken ||
							m_regionCost[current.region] != current.g)
						continue;
					if (current.region == targetRegion)
					{
						found = true;
						break;
					}

					const TAbstractRegion& region = m_regions[current.region];
					for (size_t edgeIndex = 0; edgeIndex < region.edges.size(); ++edgeIndex)
					{
						const TAbstractEdge& edge = region.edges[edgeIndex];
						const int laneJitter = (int)(PlayerBotNavHash(seed ^ (DWORD)edge.fromCell) & 3U);
						const int newCost = current.g + 10 + laneJitter;
						if (m_regionToken[edge.toRegion] == m_regionSearchToken &&
								newCost >= m_regionCost[edge.toRegion])
							continue;
						m_regionToken[edge.toRegion] = m_regionSearchToken;
						m_regionCost[edge.toRegion] = newCost;
						m_regionParent[edge.toRegion] = (int)current.region;
						m_regionParentEdge[edge.toRegion] = (int)edgeIndex;
						TRegionOpenNode next;
						next.g = newCost;
						next.f = newCost + AbstractHeuristic(edge.toRegion, targetRegion);
						next.region = edge.toRegion;
						next.tie = PlayerBotNavHash(seed ^ edge.toRegion ^ (DWORD)edge.fromCell);
						open.push(next);
					}
				}

				if (!found)
					return false;

				std::vector<DWORD> corridor;
				for (DWORD cursor = targetRegion; ; )
				{
					corridor.push_back(cursor);
					if (cursor == startRegion)
						break;
					const int parent = m_regionParent[cursor];
					if (parent <= 0)
						return false;
					cursor = (DWORD)parent;
				}
				std::reverse(corridor.begin(), corridor.end());

				// The abstract graph decides which connected local regions form a
				// valid corridor.  A single fine-grained A* then chooses the best
				// real crossings inside that corridor.  This keeps reachability exact
				// without forcing every bot through one arbitrary portal midpoint.
				return FindFinePathInRegionCorridor(startCell, targetCell,
						corridor, seed, path);
			}

			bool m_initialized;
			long m_mapIndex;
			long m_baseX;
			long m_baseY;
			int m_width;
			int m_height;
			std::vector<BYTE> m_blocked;
			std::vector<BYTE> m_clearance;
			std::vector<DWORD> m_component;
			std::vector<uint16_t> m_nodeToken;
			std::vector<int> m_nodeCost;
			std::vector<int> m_parent;
			uint16_t m_searchToken;
			std::vector<DWORD> m_cellRegion;
			std::vector<TAbstractRegion> m_regions;
			std::vector<DWORD> m_regionToken;
			std::vector<int> m_regionCost;
			std::vector<int> m_regionParent;
			std::vector<int> m_regionParentEdge;
			DWORD m_regionSearchToken;
	};

	struct TPlayerBotMonkeyPortal
	{
		int fromLocalX;
		int fromLocalY;
		int toLocalX;
		int toLocalY;
	};

	// The easy Monkey Dungeon is not one continuous walkable maze. Its rooms are
	// joined by native GOTO NPCs (10501..10524), which teleport a nearby character
	// locally without a loading screen. Mirror that directed graph in the planner;
	// the actual teleport is still performed by the normal server NPC event.
	const TPlayerBotMonkeyPortal PLAYERBOT_MONKEY_PORTALS[] = {
		{145, 315, 345, 361}, {80, 308, 106, 547}, {206, 109, 75, 368},
		{320, 238, 89, 746},  {421, 272, 520, 352}, {487, 279, 541, 45},
		{70, 368, 211, 109},  {65, 434, 615, 49},   {67, 498, 284, 705},
		{350, 361, 145, 310}, {210, 495, 553, 285}, {526, 352, 487, 274},
		{528, 415, 626, 291}, {523, 480, 285, 569}, {101, 547, 80, 303},
		{82, 746, 315, 238},  {541, 40, 416, 272},  {615, 44, 72, 498},
		{553, 291, 215, 495}, {626, 296, 523, 415}, {278, 705, 72, 498},
		{280, 569, 518, 480}, {470, 560, 583, 379}, {579, 390, 459, 558}
	};

	bool IsPlayerBotMonkeyReversePortal(int candidateIndex, int previousIndex)
	{
		const size_t portalCount = sizeof(PLAYERBOT_MONKEY_PORTALS) /
				sizeof(PLAYERBOT_MONKEY_PORTALS[0]);
		if (candidateIndex < 0 || previousIndex < 0 ||
				(size_t)candidateIndex >= portalCount || (size_t)previousIndex >= portalCount)
			return false;

		const TPlayerBotMonkeyPortal& candidate = PLAYERBOT_MONKEY_PORTALS[candidateIndex];
		const TPlayerBotMonkeyPortal& previous = PLAYERBOT_MONKEY_PORTALS[previousIndex];
		// Reciprocal GOTO entries deliberately land about five map cells beside
		// one another. Treat that pair as the same doorway for ten seconds: if a
		// target dies while the bot is crossing, the next target scan must not
		// send it straight back through the arrival portal.
		return abs(candidate.fromLocalX - previous.toLocalX) <= 8 &&
				abs(candidate.fromLocalY - previous.toLocalY) <= 8 &&
				abs(candidate.toLocalX - previous.fromLocalX) <= 8 &&
				abs(candidate.toLocalY - previous.fromLocalY) <= 8;
	}

	bool FindPlayerBotMonkeyPortalStep(CPlayerBotNavigation& navigation,
			long startX, long startY, long targetX, long targetY,
			int blockedReverseOfPortal, long& portalX, long& portalY,
			int& selectedPortalIndex)
	{
		const DWORD startComponent = navigation.GetComponentAtWorld(startX, startY, 6);
		const DWORD targetComponent = navigation.GetComponentAtWorld(targetX, targetY, 6);
		if (startComponent == 0 || targetComponent == 0 || startComponent == targetComponent)
			return false;

		std::queue<DWORD> open;
		std::map<DWORD, DWORD> parentComponent;
		std::map<DWORD, int> parentPortal;
		parentComponent[startComponent] = startComponent;
		open.push(startComponent);

		while (!open.empty() && parentComponent.find(targetComponent) == parentComponent.end())
		{
			const DWORD current = open.front();
			open.pop();
			for (size_t i = 0; i < sizeof(PLAYERBOT_MONKEY_PORTALS) /
					sizeof(PLAYERBOT_MONKEY_PORTALS[0]); ++i)
			{
				const TPlayerBotMonkeyPortal& portal = PLAYERBOT_MONKEY_PORTALS[i];
				if (IsPlayerBotMonkeyReversePortal((int)i, blockedReverseOfPortal))
					continue;
				const long fromX = PLAYERBOT_MONKEY_EASY_BASE_X + portal.fromLocalX * 100L;
				const long fromY = PLAYERBOT_MONKEY_EASY_BASE_Y + portal.fromLocalY * 100L;
				const DWORD fromComponent = navigation.GetComponentAtWorld(fromX, fromY, 12);
				if (fromComponent != current)
					continue;

				const long toX = PLAYERBOT_MONKEY_EASY_BASE_X + portal.toLocalX * 100L;
				const long toY = PLAYERBOT_MONKEY_EASY_BASE_Y + portal.toLocalY * 100L;
				const DWORD toComponent = navigation.GetComponentAtWorld(toX, toY, 12);
				if (toComponent == 0 || parentComponent.find(toComponent) != parentComponent.end())
					continue;

				parentComponent[toComponent] = current;
				parentPortal[toComponent] = (int)i;
				open.push(toComponent);
			}
		}

		if (parentComponent.find(targetComponent) == parentComponent.end())
			return false;

		DWORD cursor = targetComponent;
		int firstPortal = -1;
		while (cursor != startComponent)
		{
			std::map<DWORD, int>::const_iterator portalIt = parentPortal.find(cursor);
			std::map<DWORD, DWORD>::const_iterator parentIt = parentComponent.find(cursor);
			if (portalIt == parentPortal.end() || parentIt == parentComponent.end())
				return false;
			firstPortal = portalIt->second;
			cursor = parentIt->second;
		}
		if (firstPortal < 0)
			return false;

		portalX = PLAYERBOT_MONKEY_EASY_BASE_X +
				PLAYERBOT_MONKEY_PORTALS[firstPortal].fromLocalX * 100L;
		portalY = PLAYERBOT_MONKEY_EASY_BASE_Y +
				PLAYERBOT_MONKEY_PORTALS[firstPortal].fromLocalY * 100L;
		selectedPortalIndex = firstPortal;
		return true;
	}

	bool IsPlayerBotPathClear(long lMapIndex, long startX, long startY, long endX, long endY)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMapIndex);
		if (navigation.Init(lMapIndex))
			return navigation.SegmentClearWorld(startX, startY, endX, endY);

		const int distance = DISTANCE_APPROX(endX - startX, endY - startY);
		const int steps = std::max(1, (distance + PLAYERBOT_NAV_NATIVE_SAMPLE - 1) /
				PLAYERBOT_NAV_NATIVE_SAMPLE);
		for (int i = 0; i <= steps; ++i)
		{
			const long x = startX + ((endX - startX) * i) / steps;
			const long y = startY + ((endY - startY) * i) / steps;
			if (IsPlayerBotPositionBlocked(lMapIndex, x, y))
				return false;
		}
		return true;
	}

	bool IsPlayerBotReachable(long lMapIndex, long startX, long startY, long endX, long endY)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMapIndex);
		if (navigation.Init(lMapIndex))
			return navigation.CanReach(startX, startY, endX, endY);
		return IsPlayerBotPathClear(lMapIndex, startX, startY, endX, endY);
	}

	DWORD GetPlayerBotPartyReservationPID(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		LPCHARACTER leader = ch->GetParty() ? ch->GetParty()->GetLeaderCharacter() : NULL;
		return leader ? leader->GetPlayerID() : ch->GetPlayerID();
	}

	void RememberPlayerBotMetin(LPCHARACTER stone, DWORD dwNow)
	{
		if (!stone || !stone->IsStone() || stone->IsDead())
			return;
		const bool bNewDiscovery = s_mapKnownPlayerBotMetins.find(stone->GetVID()) ==
				s_mapKnownPlayerBotMetins.end();
		TKnownPlayerBotMetin& known = s_mapKnownPlayerBotMetins[stone->GetVID()];
		known.lMapIndex = stone->GetMapIndex();
		known.lX = stone->GetX();
		known.lY = stone->GetY();
		known.bLevel = stone->GetLevel();
		known.dwLastSeenTime = dwNow;

		if (bNewDiscovery && stone->GetMapIndex() == 21)
		{
			int nearest = 0;
			int nearestDistance = INT_MAX;
			for (int i = 0; i < 12; ++i)
			{
				const int distance = DISTANCE_APPROX(stone->GetX() - PLAYERBOT_METIN_HOTSPOTS[i].x,
						stone->GetY() - PLAYERBOT_METIN_HOTSPOTS[i].y);
				if (distance < nearestDistance)
				{
					nearest = i;
					nearestDistance = distance;
				}
			}
			++s_adwPlayerBotMetinHotspotFinds[nearest];
			s_adwPlayerBotMetinHotspotLastFind[nearest] = dwNow;
		}
	}

	bool IsPlayerBotMetinWorthFighting(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
			return false;
		// The server drop multiplier still has useful value at a ten-level
		// advantage. Below that it collapses sharply (15% at -11 and 1% at -15),
		// so a level-25 bot should pass level-5/10 stones and keep level-15+.
		return stone->GetLevel() <= ch->GetLevel() + 9 &&
				ch->GetLevel() <= stone->GetLevel() + 10;
	}

	BYTE ChoosePlayerBotMetinHotspot(DWORD playerID, BYTE currentIndex, DWORD dwNow)
	{
		BYTE best = currentIndex % 12;
		int bestScore = INT_MIN;
		// Compare four PID-specific candidates. This learns productive areas while
		// keeping different hunters on different routes instead of one global line.
		for (int option = 0; option < 4; ++option)
		{
			const BYTE index = (BYTE)((currentIndex + option * 3 +
					(PlayerBotNavHash(playerID + option * 101U) % 5U)) % 12);
			const int successRate = (int)((s_adwPlayerBotMetinHotspotFinds[index] + 1) * 1000 /
					(s_adwPlayerBotMetinHotspotVisits[index] + 3));
			const int freshness = s_adwPlayerBotMetinHotspotLastFind[index] != 0 &&
					dwNow - s_adwPlayerBotMetinHotspotLastFind[index] < 300000 ? 250 : 0;
			const int personalJitter = (int)(PlayerBotNavHash(playerID ^ (index * 7919U)) % 350U);
			const int score = successRate + freshness + personalJitter;
			if (score > bestScore)
			{
				bestScore = score;
				best = index;
			}
		}
		return best;
	}

	void ReservePlayerBotMetin(LPCHARACTER ch, LPCHARACTER stone, DWORD dwNow)
	{
		if (!IsPlayerBotMetinWorthFighting(ch, stone))
			return;
		RememberPlayerBotMetin(stone, dwNow);
		TKnownPlayerBotMetin& known = s_mapKnownPlayerBotMetins[stone->GetVID()];
		known.dwReservedByPID = GetPlayerBotPartyReservationPID(ch);
		known.dwReserveUntil = dwNow + 45000;
	}

	void ReleasePlayerBotMetinReservation(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone || ch->GetParty())
			return;
		TKnownPlayerBotMetinMap::iterator it =
				s_mapKnownPlayerBotMetins.find(stone->GetVID());
		if (it == s_mapKnownPlayerBotMetins.end() ||
				it->second.dwReservedByPID != ch->GetPlayerID())
			return;
		it->second.dwReservedByPID = 0;
		it->second.dwReserveUntil = 0;
	}

	LPCHARACTER FindKnownPlayerBotMetin(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return NULL;

		LPCHARACTER best = NULL;
		int bestScore = INT_MIN;
		const DWORD myReservationPID = GetPlayerBotPartyReservationPID(ch);
		for (TKnownPlayerBotMetinMap::iterator it = s_mapKnownPlayerBotMetins.begin();
				it != s_mapKnownPlayerBotMetins.end(); )
		{
			LPCHARACTER stone = CHARACTER_MANAGER::instance().Find(it->first);
			if (!stone || !stone->IsStone() || stone->IsDead() ||
					dwNow - it->second.dwLastSeenTime > 300000)
			{
				s_mapKnownPlayerBotMetins.erase(it++);
				continue;
			}

			RememberPlayerBotMetin(stone, dwNow);
			TKnownPlayerBotMetin& known = it->second;
			++it;
			if (known.lMapIndex != ch->GetMapIndex() ||
					!IsPlayerBotMetinWorthFighting(ch, stone))
				continue;
			if (known.dwReserveUntil > dwNow && known.dwReservedByPID != 0 &&
					known.dwReservedByPID != myReservationPID)
				continue;
			if (!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), known.lX, known.lY))
				continue;

			const int distance = DISTANCE_APPROX(ch->GetX() - known.lX, ch->GetY() - known.lY);
			const int levelDelta = abs((int)ch->GetLevel() - (int)known.bLevel);
			const int score = 500000 - distance * 3 - levelDelta * 10000;
			if (!best || score > bestScore)
			{
				best = stone;
				bestScore = score;
			}
		}

		if (best)
			ReservePlayerBotMetin(ch, best, dwNow);
		return best;
	}

	void ClearPlayerBotRoute(TPlayerBotAIState& state, bool clearGoal)
	{
		state.vecRoute.clear();
		state.uRouteIndex = 0;
		state.lIssuedWaypointX = 0;
		state.lIssuedWaypointY = 0;
		state.iNavLastWaypointDistance = -1;
		state.dwNextNavProgressTime = 0;
		state.bNavNoProgressCount = 0;
		state.bNavDeferredCount = 0;
		if (clearGoal)
		{
			state.lRouteDestX = 0;
			state.lRouteDestY = 0;
			state.lRouteMapIndex = 0;
			state.bRouteAllowsHorse = false;
		}
	}

	bool SetPlayerBotRidingForTravel(LPCHARACTER ch, TPlayerBotAIState& state,
			bool shouldRide, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;

		if (!shouldRide)
		{
			if (!ch->IsRiding())
				return false;

			ch->Stop();
			if (!ch->StopRiding())
				return false;

			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = 0;
			state.dwNextHorseRideCheckTime = dwNow + 1000;
			state.dwLastMeaningfulActivityTime = dwNow;
			sys_log(0, "PLAYERBOT_HORSE: dismounted pid=%u name=%s map=%ld pos=(%ld,%ld) reason=%s",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
					reason ? reason : "?");
			return true;
		}

		if (ch->IsRiding() || ch->GetHorseLevel() == 0 ||
				ch->GetHorseHealth() <= 0 || ch->GetHorseStamina() <= 0 ||
				dwNow < state.dwNextHorseRideCheckTime)
			return false;

		ch->Stop();
		if (!ch->StartRiding())
		{
			state.dwNextHorseRideCheckTime = dwNow + PLAYERBOT_HORSE_RIDE_RETRY_INTERVAL;
			sys_err("PLAYERBOT_HORSE: mount failed pid=%u name=%s horse_level=%u health=%d stamina=%d reason=%s",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetHorseLevel(),
					ch->GetHorseHealth(), ch->GetHorseStamina(), reason ? reason : "?");
			return false;
		}

		ClearPlayerBotRoute(state, false);
		state.dwNextNavPlanTime = 0;
		state.dwNextHorseRideCheckTime = dwNow + 1000;
		state.dwLastMeaningfulActivityTime = dwNow;
		sys_log(0, "PLAYERBOT_HORSE: mounted pid=%u name=%s horse_level=%u map=%ld pos=(%ld,%ld) reason=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetHorseLevel(),
				ch->GetMapIndex(), ch->GetX(), ch->GetY(), reason ? reason : "?");
		return true;
	}

	// A battle horse (level 11+) lets its rider strike from the saddle. Bots that
	// own one should ride into a fight instead of dismounting on the approach, but
	// only when the weapon and target actually make mounted combat sensible.
	bool CanPlayerBotFightOnHorse(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || ch->GetHorseLevel() < 11)
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		if (!weapon || weapon->GetType() != ITEM_WEAPON ||
				weapon->GetSubType() == WEAPON_BOW)
			return false;

		// Against Metins a battle horse is priority #1: the rider keeps hacking the
		// stone from the saddle rather than climbing down for every spot.
		if (target && target->IsStone())
			return true;

		// Warriors and Suras clear mob spots (multi-pull / valour cloak packs) from
		// horseback; ranged and caster jobs still fight on foot.
		if (ch->GetJob() == JOB_WARRIOR || ch->GetJob() == JOB_SURA)
			return true;

		return false;
	}

	void UpdatePlayerBotTravelMount(LPCHARACTER ch, TPlayerBotAIState& state,
			long destX, long destY, bool allowHorse, DWORD dwNow,
			bool fightOnHorse = false)
	{
		if (!ch)
			return;

		// Mounted combat overrides the travel dismount: the bot is closing on a
		// target it may legitimately hit from the saddle, so keep (or take) the
		// horse regardless of how near the destination is. SetPlayerBotRidingForTravel
		// still refuses gracefully when the horse is spent, leaving the bot on foot.
		if (fightOnHorse)
		{
			SetPlayerBotRidingForTravel(ch, state, true, dwNow, "mounted_combat");
			return;
		}

		const int distance = DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY);
		if (!allowHorse || distance <= PLAYERBOT_HORSE_DISMOUNT_DISTANCE)
			SetPlayerBotRidingForTravel(ch, state, false, dwNow,
					allowHorse ? "near_destination" : "on_foot_action");
		else if (distance >= PLAYERBOT_HORSE_MOUNT_DISTANCE)
			SetPlayerBotRidingForTravel(ch, state, true, dwNow, "long_travel");
	}

	void BuildPlayerBotStraightRoute(long startX, long startY, long targetX, long targetY,
			std::vector<PIXEL_POSITION>& route)
	{
		route.clear();
		const int distance = DISTANCE_APPROX(targetX - startX, targetY - startY);
		const int segmentCount = std::max(1, (distance + PLAYERBOT_NAV_MAX_SEGMENT - 1) /
				PLAYERBOT_NAV_MAX_SEGMENT);
		for (int segment = 1; segment <= segmentCount; ++segment)
		{
			PIXEL_POSITION point;
			point.x = startX + ((targetX - startX) * segment) / segmentCount;
			point.y = startY + ((targetY - startY) * segment) / segmentCount;
			point.z = 0;
			route.push_back(point);
		}
	}

	bool MovePlayerBot(LPCHARACTER ch, long destX, long destY, DWORD dwNow,
			int targetSnapRadius = 4, bool flexibleTargetSnap = false,
			bool allowHorse = false, bool fightOnHorse = false)
	{
		if (!ch)
			return false;
		TPlayerBotAIState& state = s_mapPlayerBotAIStates[ch->GetPlayerID()];
		if (!ch->GetSectree())
		{
			if (dwNow >= state.dwNextNavErrorLogTime)
			{
				state.dwNextNavErrorLogTime = dwNow + 10000;
				sys_err("PLAYERBOT_NAV: missing sectree pid=%u name=%s map=%ld pos=(%ld,%ld) dest=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
						destX, destY);
			}
			return false;
		}

		const long mapIndex = ch->GetMapIndex();
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		if (!navigation.Init(mapIndex))
			return false;
		navigation.ClampWorld(destX, destY);

		bool redirectedToMonkeyPortal = false;
		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY &&
				!navigation.CanReach(ch->GetX(), ch->GetY(), destX, destY))
		{
			long portalX = 0, portalY = 0;
			int portalIndex = -1;
			const int blockedReverseOfPortal =
					dwNow < state.dwMonkeyReversePortalBlockUntil
					? state.iLastMonkeyPortalIndex : -1;
			if (FindPlayerBotMonkeyPortalStep(navigation, ch->GetX(), ch->GetY(),
					destX, destY, blockedReverseOfPortal, portalX, portalY, portalIndex))
			{
				destX = portalX;
				destY = portalY;
				state.iLastMonkeyPortalIndex = portalIndex;
				state.dwMonkeyReversePortalBlockUntil =
						dwNow + PLAYERBOT_MONKEY_REVERSE_PORTAL_BLOCK_TIME;
				targetSnapRadius = std::max(targetSnapRadius, 16);
				flexibleTargetSnap = true;
				redirectedToMonkeyPortal = true;
			}
		}

		const int goalDrift = DISTANCE_APPROX(destX - state.lRouteDestX, destY - state.lRouteDestY);
		const bool newGoal = state.lRouteMapIndex != mapIndex ||
				goalDrift > PLAYERBOT_NAV_GOAL_REPLAN_DISTANCE;
		// Keep the travel mode attached to the route itself.  Several lightweight
		// AI passes merely continue the already planned destination and call this
		// function without explicitly requesting a horse.  Treating that default
		// value as a new decision made mounted bots dismount and remount every tick.
		if (newGoal)
			state.bRouteAllowsHorse = allowHorse;
		UpdatePlayerBotTravelMount(ch, state, destX, destY,
				state.bRouteAllowsHorse, dwNow, fightOnHorse);
		if (newGoal)
		{
			ClearPlayerBotRoute(state, false);
			// Goto() safely redirects an active move from the current authoritative
			// position. Stopping first reset the movement state for a single frame
			// and amplified the client's backwards correction.
			state.lRouteMapIndex = mapIndex;
			state.lRouteDestX = destX;
			state.lRouteDestY = destY;
			state.dwNextNavPlanTime = 0;
			state.bStuckCounter = 0;
			if (redirectedToMonkeyPortal)
				sys_log(0, "PLAYERBOT_MONKEY: portal route pid=%u name=%s from=(%ld,%ld) portal=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY);
		}

		// If an old route ended near a moving target, do not keep reporting
		// success while the current requested destination is still far away.
		if (!state.vecRoute.empty() && state.uRouteIndex >= state.vecRoute.size() &&
				DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) > PLAYERBOT_NAV_ARRIVAL_DISTANCE)
			ClearPlayerBotRoute(state, false);

		if (state.vecRoute.empty())
		{
			if (dwNow < state.dwNextNavPlanTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				return true;
			}

			if (navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), destX, destY))
			{
				BuildPlayerBotStraightRoute(ch->GetX(), ch->GetY(), destX, destY, state.vecRoute);
			}
			else
			{
				const DWORD routeSeed = ch->GetPlayerID() ^
						((DWORD)state.bStuckCounter * 0x9e3779b9U);
				const EPlayerBotNavPlanResult planResult = navigation.FindRoute(
						ch->GetX(), ch->GetY(), destX, destY, routeSeed, dwNow,
						targetSnapRadius, flexibleTargetSnap, state.vecRoute);
				if (planResult == PLAYERBOT_NAV_PLAN_DEFERRED)
				{
					if (state.bNavDeferredCount < 255)
						++state.bNavDeferredCount;
					if (state.bNavDeferredCount == 20)
						sys_err("PLAYERBOT_NAV: repeatedly deferred pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld)",
								ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY);
					// Desynchronise retries so the same low PIDs do not consume every
					// planning slot on each pass through the ordered bot map.
					state.dwNextNavPlanTime = dwNow + 750 +
							(PlayerBotNavHash(ch->GetPlayerID()) % 1251U);
					if (ch->IsStateMove())
						ch->Stop();
					return true;
				}
				if (planResult == PLAYERBOT_NAV_PLAN_UNREACHABLE)
				{
					state.bNavDeferredCount = 0;
					state.dwNextNavPlanTime = dwNow + 1500 + (ch->GetPlayerID() % 700);
					if (state.bStuckCounter < 255)
						++state.bStuckCounter;
					if (state.bStuckCounter == 1 || state.bStuckCounter == 3)
						sys_err("PLAYERBOT_NAV: unreachable pid=%u name=%s from=(%ld,%ld) to=(%ld,%ld) failures=%u",
								ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), destX, destY,
								state.bStuckCounter);
					if (ch->IsStateMove())
						ch->Stop();
					return false;
				}
			}
			state.bNavDeferredCount = 0;

			state.uRouteIndex = 0;
			state.lIssuedWaypointX = 0;
			state.lIssuedWaypointY = 0;
			state.lNavProgressX = ch->GetX();
			state.lNavProgressY = ch->GetY();
			state.iNavLastWaypointDistance = -1;
			state.dwNextNavProgressTime = dwNow + 2000;
			state.bNavNoProgressCount = 0;
		}

		while (state.uRouteIndex < state.vecRoute.size())
		{
			const PIXEL_POSITION& waypoint = state.vecRoute[state.uRouteIndex];
			const int waypointDistance = DISTANCE_APPROX(
					ch->GetX() - waypoint.x, ch->GetY() - waypoint.y);
			if (waypointDistance > PLAYERBOT_NAV_ARRIVAL_DISTANCE)
				break;

			// Do not skip a short alignment waypoint when the following segment
			// is still obstructed from the character's exact interpolated point.
			// Moving those few centimetres to the cell centre is what makes the
			// next corner safe; skipping it caused route=0/0 retry loops.
			if (state.uRouteIndex + 1 < state.vecRoute.size())
			{
				const PIXEL_POSITION& nextWaypoint = state.vecRoute[state.uRouteIndex + 1];
				if (!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(),
						nextWaypoint.x, nextWaypoint.y))
					break;
			}
			++state.uRouteIndex;
			state.lIssuedWaypointX = 0;
			state.lIssuedWaypointY = 0;
			state.iNavLastWaypointDistance = -1;
			state.bNavNoProgressCount = 0;
			state.bStuckCounter = 0;
		}

		if (state.uRouteIndex >= state.vecRoute.size())
		{
			ch->Stop();
			return true;
		}

		const PIXEL_POSITION& waypoint = state.vecRoute[state.uRouteIndex];
		const int waypointDistance = DISTANCE_APPROX(
				ch->GetX() - waypoint.x, ch->GetY() - waypoint.y);

		if (dwNow >= state.dwNextNavProgressTime)
		{
			const bool hadProgressBaseline = state.iNavLastWaypointDistance >= 0;
			const bool gotCloser = hadProgressBaseline &&
					waypointDistance + 75 < state.iNavLastWaypointDistance;
			if (hadProgressBaseline &&
					waypointDistance > PLAYERBOT_NAV_ARRIVAL_DISTANCE && !gotCloser)
			{
				if (state.bNavNoProgressCount < 255)
					++state.bNavNoProgressCount;
			}
			else if (hadProgressBaseline)
			{
				state.bNavNoProgressCount = 0;
				state.bStuckCounter = 0;
			}

			state.lNavProgressX = ch->GetX();
			state.lNavProgressY = ch->GetY();
			state.iNavLastWaypointDistance = waypointDistance;
			state.dwNextNavProgressTime = dwNow + 2000;

			if (state.bNavNoProgressCount >= 3)
			{
				sys_err("PLAYERBOT_NAV: no progress, replanning pid=%u name=%s pos=(%ld,%ld) waypoint=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(), waypoint.x, waypoint.y);
				if (state.bStuckCounter < 255)
					++state.bStuckCounter;
				ClearPlayerBotRoute(state, false);
				state.dwNextNavPlanTime = dwNow + 200;
				ch->Stop();
				return false;
			}
		}

		// Dynamic objects may have appeared since the path was planned.
		if (!navigation.SegmentClearWorld(ch->GetX(), ch->GetY(), waypoint.x, waypoint.y))
		{
			if (state.bStuckCounter < 255)
				++state.bStuckCounter;
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = dwNow + 200;
			ch->Stop();
			return false;
		}

		ch->SetRotationToXY(waypoint.x, waypoint.y);
		if (state.lIssuedWaypointX == waypoint.x && state.lIssuedWaypointY == waypoint.y && ch->IsStateMove())
			return true;

		const bool wasMoving = ch->IsStateMove();
		const bool commandAccepted = ch->Goto(waypoint.x, waypoint.y);
		state.lIssuedWaypointX = waypoint.x;
		state.lIssuedWaypointY = waypoint.y;
		if (commandAccepted || (!wasMoving && ch->IsStateMove()))
		{
			ch->SendMovePacket(FUNC_MOVE, 0, waypoint.x, waypoint.y, ch->GetCurrentMoveDuration(), dwNow);
			return true;
		}

		// Goto(false) also means that this exact destination is already active.
		return ch->IsStateMove() || waypointDistance <= PLAYERBOT_NAV_ARRIVAL_DISTANCE;
	}

	void GetPlayerBotStableOffset(DWORD playerID, DWORD salt, int minRadius, int maxRadius,
			long& offsetX, long& offsetY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ salt);
		const int radiusRange = std::max(0, maxRadius - minRadius);
		const int radius = minRadius + (radiusRange > 0 ? (int)(hash % (DWORD)(radiusRange + 1)) : 0);
		const float angle = (float)(PlayerBotNavHash(hash ^ 0xa511e9b3U) % 360U);
		float dx = 0.0f;
		float dy = 0.0f;
		GetDeltaByDegree(angle, (float)radius, &dx, &dy);
		offsetX = (long)dx;
		offsetY = (long)dy;
	}

	bool IsPlayerBotWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;

		if (ch)
		{
			const BYTE subType = item->GetSubType();
			switch (ch->GetJob())
			{
				case JOB_ASSASSIN:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_BOW;
					// Before selecting a profession and on Dagger training, never equip
					// a bow: melee Ninja skills ask CalcMeleeDamage and reject bows.
					return subType == WEAPON_DAGGER || subType == WEAPON_SWORD;
				case JOB_WARRIOR:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_TWO_HANDED || subType == WEAPON_SWORD;
					return subType == WEAPON_SWORD;
				case JOB_SURA:
					return subType == WEAPON_SWORD;
				case JOB_SHAMAN:
					return subType == WEAPON_BELL || subType == WEAPON_FAN;
			}
		}

		switch (item->GetSubType())
		{
			case WEAPON_SWORD:
			case WEAPON_DAGGER:
			case WEAPON_TWO_HANDED:
			case WEAPON_BELL:
			case WEAPON_FAN:
			case WEAPON_MOUNT_SPEAR:
				return true;
			case WEAPON_BOW:
				return true;
		}

		return false;
	}

	bool IsPlayerBotEquipmentCandidate(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsExchanging() || !item->IsEquipable())
			return false;

		// IsEquipable only describes the item type.  The class restrictions live
		// in the anti flags and were previously checked only for sex, so a Warrior
		// could keep (and repeatedly try to equip/refine) entire inventories of
		// Ninja, Sura and Shaman armour.  CanUsedBy is the engine's canonical job
		// anti-flag check and deliberately does not depend on combat state.
		if (!item->CanUsedBy(ch))
			return false;

		if (item->GetType() == ITEM_WEAPON && !IsPlayerBotWeapon(ch, item))
			return false;

		switch (item->GetType())
		{
			case ITEM_WEAPON:
			case ITEM_ARMOR:
			case ITEM_UNIQUE:
			case ITEM_RING:
			case ITEM_BELT:
				break;
			default:
				return false;
		}

		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_MALE) && GET_SEX(ch) == SEX_MALE)
			return false;
		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_FEMALE) && GET_SEX(ch) == SEX_FEMALE)
			return false;

		return true;
	}

	long long ScorePlayerBotApply(BYTE bType, long lValue)
	{
		switch (bType)
		{
			case APPLY_NONE:
			case APPLY_SKILL:
				return 0;
			case APPLY_MAX_HP:
				return (long long)lValue * 10;
			case APPLY_MAX_SP:
				return (long long)lValue * 3;
			case APPLY_CON:
			case APPLY_STR:
			case APPLY_DEX:
			case APPLY_INT:
				return (long long)lValue * 250;
			case APPLY_ATT_SPEED:
				return (long long)lValue * 200;
			case APPLY_MOV_SPEED:
				return (long long)lValue * 100;
			case APPLY_HP_REGEN:
			case APPLY_POTION_BONUS:
				return (long long)lValue * 100;
			case APPLY_POISON_PCT:
			case APPLY_STUN_PCT:
			case APPLY_SLOW_PCT:
				return (long long)lValue * 200;
			case APPLY_CRITICAL_PCT:
			case APPLY_PENETRATE_PCT:
			case APPLY_BLOCK:
			case APPLY_DODGE:
				return (long long)lValue * 400;
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:
			case APPLY_ATTBONUS_MONSTER:
				return (long long)lValue * 300;
			case APPLY_STEAL_HP:
			case APPLY_KILL_HP_RECOVER:
				return (long long)lValue * 250;
			case APPLY_ATT_GRADE_BONUS:
			case APPLY_DEF_GRADE_BONUS:
			case APPLY_DEF_GRADE:
				return (long long)lValue * 300;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
			case APPLY_NORMAL_HIT_DEFEND_BONUS:
				return (long long)lValue * 500;
			case APPLY_IMMUNE_STUN:
			case APPLY_IMMUNE_SLOW:
			case APPLY_IMMUNE_FALL:
				return (long long)lValue * 1000;
			default:
				return (long long)lValue * 50;
		}
	}

	bool IsPlayerBotSpecialLevel30WeaponVnum(DWORD vnum)
	{
		return (vnum >= 290 && vnum <= 299) || (vnum >= 1170 && vnum <= 1179) ||
				(vnum >= 2150 && vnum <= 2159) || (vnum >= 3210 && vnum <= 3219) ||
				(vnum >= 5110 && vnum <= 5119) || (vnum >= 7160 && vnum <= 7169);
	}

	long long GetPlayerBotEquipmentScore(LPITEM item, LPCHARACTER ch = NULL)
	{
		if (!item || !item->GetProto())
			return 0;

		long long score = 1;
		if (item->GetType() == ITEM_WEAPON)
		{
			score += (long long)(item->GetValue(3) + item->GetValue(4) + 2 * item->GetValue(5)) * 1000;
			const DWORD vnum = item->GetVnum();
			const bool specialLevel30 = IsPlayerBotSpecialLevel30WeaponVnum(vnum);
			if (specialLevel30)
				score += 350000; // Average-damage level-30 families stay meaningful.

			if (ch)
			{
				if (ch->GetJob() == JOB_ASSASSIN)
				{
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_BOW)
						score += 500000; // Prefer bows for Archer Ninja
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_DAGGER)
						score += 300000; // Prefer daggers for Dagger Ninja
				}
				else if (ch->GetJob() == JOB_WARRIOR)
				{
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_TWO_HANDED)
						score += 200000; // Prefer two-handed for Mental Warrior
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_SWORD)
						score += 200000; // Prefer sword for Body Warrior
				}
			}
		}
		else if (item->GetType() == ITEM_ARMOR &&
				(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_HEAD ||
				 item->GetSubType() == ARMOR_FOOTS || item->GetSubType() == ARMOR_SHIELD))
		{
			score += (long long)(item->GetValue(1) + 2 * item->GetValue(5)) * 1000;
		}

		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			score += ScorePlayerBotApply(item->GetProto()->aApplies[i].bType, item->GetProto()->aApplies[i].lValue);

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			score += ScorePlayerBotApply(item->GetAttributeType(i), item->GetAttributeValue(i));

		if (item->GetImmuneFlag() != 0)
			score += 1000;

		// A race-attack bonus is only worth carrying where that race is what you
		// actually fight. The population learns which monsters live on each map,
		// so "strong against orcs" counts for far more in Orc Valley than in a
		// place where nothing orcish ever spawns.
		if (ch)
		{
			const int dominant = GetPlayerBotDominantRace(ch->GetMapIndex());
			if (dominant != PLAYERBOT_RACE_NONE)
			{
				const BYTE wanted = GetPlayerBotRaceApplyType(dominant);
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					if (item->GetAttributeType(i) == wanted)
						score += (long long)item->GetAttributeValue(i) * 600;
				}
				for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
				{
					if (item->GetProto()->aApplies[i].bType == wanted)
						score += (long long)item->GetProto()->aApplies[i].lValue * 600;
				}
			}
		}

		return score;
	}

	bool SharePlayerBotOldGearNearby(LPCHARACTER ch, LPITEM oldItem)
	{
		if (!ch || !oldItem || oldItem->IsEquipped() || oldItem->isLocked() ||
				oldItem->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
				!ch->GetSectree())
			return false;

		const int wearCell = oldItem->FindEquipCell(ch);
		if (wearCell < 0)
			return false;

		struct FGearSharer
		{
			LPCHARACTER m_giver;
			LPITEM m_item;
			int m_wearCell;
			LPCHARACTER m_receiver;
			long long m_bestImprovement;
			int m_bestDistance;

			FGearSharer(LPCHARACTER giver, LPITEM item, int wearCell) :
				m_giver(giver), m_item(item), m_wearCell(wearCell), m_receiver(NULL),
				m_bestImprovement(0), m_bestDistance(INT_MAX) {}

			bool operator()(LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;

				LPCHARACTER member = static_cast<LPCHARACTER>(entity);
				if (!member || member == m_giver || member->IsDead() || !member->IsPC() ||
						!member->GetDesc() || !member->GetDesc()->IsBot() ||
						member->GetJob() != m_giver->GetJob() ||
						member->GetSkillGroup() != m_giver->GetSkillGroup() ||
						member->GetLevel() >= m_giver->GetLevel() ||
						m_item->GetLevelLimit() > member->GetLevel())
					return true;

				const int distance = DISTANCE_APPROX(
						m_giver->GetX() - member->GetX(), m_giver->GetY() - member->GetY());
				if (distance > PLAYERBOT_GEAR_SHARE_RANGE ||
						member->GetEmptyInventory(m_item->GetSize()) < 0)
					return true;

				if (!IsPlayerBotEquipmentCandidate(member, m_item))
					return true;

				LPITEM memberOldItem = member->GetWear(m_wearCell);
				const long long newItemScore = GetPlayerBotEquipmentScore(m_item, member);
				long long memberScore = memberOldItem ? GetPlayerBotEquipmentScore(memberOldItem, member) : 0;
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM candidate = member->GetInventoryItem(cell);
					if (!candidate || !IsPlayerBotEquipmentCandidate(member, candidate) ||
							candidate->GetLevelLimit() > member->GetLevel() ||
							candidate->FindEquipCell(member) != m_wearCell)
						continue;
					memberScore = std::max(memberScore,
							GetPlayerBotEquipmentScore(candidate, member));
				}

				const long long improvement = newItemScore - memberScore;
				if (improvement > 0 &&
						(!m_receiver || improvement > m_bestImprovement ||
						 (improvement == m_bestImprovement && distance < m_bestDistance)))
				{
					m_receiver = member;
					m_bestImprovement = improvement;
					m_bestDistance = distance;
				}
				return true;
			}
		};

		FGearSharer sharer(ch, oldItem, wearCell);
		ch->GetSectree()->ForEachAround(sharer);
		if (!sharer.m_receiver)
			return false;

		const int receiverCell = sharer.m_receiver->GetEmptyInventory(oldItem->GetSize());
		if (receiverCell < 0)
			return false;

		const WORD oldCell = oldItem->GetCell();
		const DWORD vnum = oldItem->GetVnum();
		const BYTE refine = oldItem->GetRefineLevel();
		oldItem->RemoveFromCharacter();
		if (oldItem->AddToCharacter(sharer.m_receiver, TItemPos(INVENTORY, receiverCell)))
		{
			sys_log(0, "PLAYERBOT_AI: gifted reserve gear pid=%u name=%s -> target_pid=%u target_name=%s vnum=%u refine=%u improvement=%lld",
					ch->GetPlayerID(), ch->GetName(), sharer.m_receiver->GetPlayerID(),
					sharer.m_receiver->GetName(), vnum, refine, sharer.m_bestImprovement);
			return true;
		}

		oldItem->AddToCharacter(ch, TItemPos(INVENTORY, oldCell));
		return false;
	}

	bool ManagePlayerBotEquipment(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		if (dwNow < state.dwNextEquipmentCheckTime && !state.bEquipPending)
			return false;

		LPITEM bestItem = NULL;
		LPITEM bestOldItem = NULL;
		int bestWearCell = -1;
		long long bestImprovement = 0;
		long long bestScore = 0;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotEquipmentCandidate(ch, item))
				continue;

			const int wearCell = item->FindEquipCell(ch);
			if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
				continue;

			LPITEM oldItem = ch->GetWear(wearCell);
			if (oldItem && IS_SET(oldItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;

			if (!ch->CanEquipNow(item, TItemPos(INVENTORY, cell)))
				continue;

			const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
			const long long oldScore = oldItem ? GetPlayerBotEquipmentScore(oldItem, ch) : 0;
			if (oldItem && itemScore <= oldScore)
				continue;

			const long long improvement = oldItem ? itemScore - oldScore : 1000000000000LL + itemScore;
			if (!bestItem || improvement > bestImprovement)
			{
				bestItem = item;
				bestOldItem = oldItem;
				bestWearCell = wearCell;
				bestImprovement = improvement;
				bestScore = itemScore;
			}
		}

		if (!bestItem)
		{
			state.bEquipPending = false;
			state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;
			return false;
		}

		// Equipping is forbidden for 1.5 seconds after an attack or skill, or right after spawn.
		// Hold bEquipPending and do not disrupt active combat.
		if (dwNow - ch->GetLastAttackTime() <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY ||
			dwNow - state.dwLastBotSkillTime <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY ||
			(state.dwSpawnTime != 0 && dwNow - state.dwSpawnTime <= PLAYERBOT_EQUIPMENT_COMBAT_DELAY))
		{
			state.bEquipPending = true;
			return false;
		}

		state.bEquipPending = false;
		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;

		const DWORD newVnum = bestItem->GetVnum();
		const DWORD oldVnum = bestOldItem ? bestOldItem->GetVnum() : 0;
		const long long oldScore = bestOldItem ? GetPlayerBotEquipmentScore(bestOldItem, ch) : 0;
		// EquipItem's optional integer is a candidate slot for rings/uniques, not a
		// wear slot.  Passing WEAR_WEAPON/WEAR_BODY here made swaps of differently
		// sized items fail.  Put the old item into a genuinely free inventory area,
		// then let FindEquipCell choose the normal destination.
		if (bestOldItem)
		{
			if (ch->GetEmptyInventory(bestOldItem->GetSize()) < 0 ||
					!ch->UnequipItem(bestOldItem))
			{
				state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
				return false;
			}
		}

		if (ch->EquipItem(bestItem))
		{
			sys_log(0, "PLAYERBOT_AI: equipped upgrade pid=%u name=%s wear=%d old_vnum=%u new_vnum=%u old_score=%lld new_score=%lld",
					ch->GetPlayerID(), ch->GetName(), bestWearCell, oldVnum, newVnum, oldScore, bestScore);

			if (bestOldItem)
				SharePlayerBotOldGearNearby(ch, bestOldItem);

			return true;
		}

		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
		sys_err("PLAYERBOT_AI: failed to equip upgrade pid=%u name=%s wear=%d vnum=%u",
				ch->GetPlayerID(), ch->GetName(), bestWearCell, newVnum);
		return false;
	}

	void RestorePlayerBotEquipmentAfterRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		// A blacksmith session can temporarily remove more than one worn item.
		// ManagePlayerBotEquipment intentionally equips only one upgrade per call,
		// so force a short bounded pass before the bot leaves the NPC.  This makes
		// the visible sequence match a real player: remove, refine through the
		// desired + level, then put the best surviving result back on.
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			if (!ManagePlayerBotEquipment(ch, state, dwNow))
				break;
		}
	}

	DWORD GetStarterChestVnum(BYTE bJob)
	{
		switch (bJob)
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 50187;
			case JOB_ASSASSIN:
				return 50212;
			case JOB_SHAMAN:
				return 50213;
		}

		return 0;
	}

	DWORD GetPlayerBotEmergencyWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 10;

		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 10;   // Sword +0
			case JOB_ASSASSIN:
				if (ch->GetSkillGroup() == 2)
					return 2000; // Bow +0
				return 1000; // Dagger +0
			case JOB_SHAMAN:
				return 7000; // Fan +0
		}

		return 10;
	}

	long long GetPlayerBotEmergencyWeaponPrice(LPCHARACTER ch)
	{
		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		return vnum == 7000 ? 600 : 100;
	}

	int GetPlayerBotProtoLevelLimit(const TItemTable* proto)
	{
		if (!proto)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return proto->aLimits[i].lValue;
		return 0;
	}

	DWORD GetPlayerBotProgressionWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD familyBase = 10;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
				familyBase = ch->GetSkillGroup() == 2 ? 3000 : 10;
				break;
			case JOB_ASSASSIN:
				familyBase = ch->GetSkillGroup() == 2 ? 2000 : 1000;
				break;
			case JOB_SURA:
				familyBase = 10;
				break;
			case JOB_SHAMAN:
				familyBase = 7000;
				break;
		}

		DWORD bestVnum = familyBase;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = familyBase + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionArmorVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD baseVnum = 11200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 11400; break;
			case JOB_SURA:     baseVnum = 11600; break;
			case JOB_SHAMAN:   baseVnum = 11800; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionShieldVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 13000;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = 13000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionHelmetVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD baseVnum = 12200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 12340; break;
			case JOB_SURA:     baseVnum = 12480; break;
			case JOB_SHAMAN:   baseVnum = 12620; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionBootsVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 15000;
		int bestLevel = -1;
		for (int tier = 0; tier < 12; ++tier)
		{
			const DWORD candidateVnum = 15000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	bool HasPlayerBotProgressionGear(LPCHARACTER ch, DWORD desiredVnum, int wearCell)
	{
		if (!ch || desiredVnum == 0)
			return false;
		TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		if (!desiredProto)
			return false;

		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : INVENTORY_MAX_NUM;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(wearCell) : ch->GetInventoryItem(index);
				if (!item || !IsPlayerBotEquipmentCandidate(ch, item) ||
						item->FindEquipCell(ch) != wearCell)
					continue;
				if (wearCell == WEAR_WEAPON && !IsPlayerBotWeapon(ch, item))
					continue;
				if (item->GetLevelLimit() >= desiredLevel && item->GetLevelLimit() <= ch->GetLevel())
					return true;
			}
		}
		return false;
	}

	bool NeedsPlayerBotProgressionWeapon(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionWeaponVnum(ch), WEAR_WEAPON);
	}

	bool NeedsPlayerBotProgressionArmor(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionArmorVnum(ch), WEAR_BODY);
	}

	bool NeedsPlayerBotProgressionShield(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionShieldVnum(ch), WEAR_SHIELD);
	}

	bool NeedsPlayerBotProgressionHelmet(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionHelmetVnum(ch), WEAR_HEAD);
	}

	bool NeedsPlayerBotProgressionBoots(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionBootsVnum(ch), WEAR_FOOTS);
	}

	bool IsPlayerBotSpecialLevel30Weapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		return IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum());
	}

	bool HasPlayerBotSpecialLevel30Weapon(LPCHARACTER ch, bool requireAverageDamage)
	{
		if (!ch)
			return false;
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : INVENTORY_MAX_NUM;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(WEAR_WEAPON) : ch->GetInventoryItem(index);
				if (!IsPlayerBotSpecialLevel30Weapon(item) || !IsPlayerBotWeapon(ch, item))
					continue;
				if (!requireAverageDamage)
					return true;
				for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
					if (item->GetAttributeType(attr) == APPLY_NORMAL_HIT_DAMAGE_BONUS &&
							item->GetAttributeValue(attr) > 0)
						return true;
			}
		}
		return false;
	}

	bool HasPlayerBotM3ReadyEquipment(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
		LPITEM boots = ch->GetWear(WEAR_FOOTS);
		if (!weapon || !armor || !shield || !helmet || !boots)
			return false;
		if (ch->GetLevel() >= 20)
			return true;
		return ch->GetLevel() >= 15 && weapon->GetRefineLevel() >= 4 &&
				armor->GetRefineLevel() >= 4 && shield->GetRefineLevel() >= 4;
	}

	bool IsPlayerBotCoreProgressionItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item))
			return false;

		const int wearCell = item->FindEquipCell(ch);
		DWORD desiredVnum = 0;
		if (wearCell == WEAR_WEAPON)
		{
			if (!IsPlayerBotWeapon(ch, item))
				return false;
			desiredVnum = GetPlayerBotProgressionWeaponVnum(ch);
		}
		else if (wearCell == WEAR_BODY)
		{
			desiredVnum = GetPlayerBotProgressionArmorVnum(ch);
		}
		else if (wearCell == WEAR_SHIELD)
		{
			desiredVnum = GetPlayerBotProgressionShieldVnum(ch);
		}
		else if (wearCell == WEAR_HEAD)
		{
			desiredVnum = GetPlayerBotProgressionHelmetVnum(ch);
		}
		else if (wearCell == WEAR_FOOTS)
		{
			desiredVnum = GetPlayerBotProgressionBootsVnum(ch);
		}
		else
		{
			return false;
		}

		const TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		return item->GetLevelLimit() >= desiredLevel &&
				item->GetLevelLimit() <= ch->GetLevel();
	}

	BYTE GetPlayerBotRefineTarget(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;

		// Equipment is a primary progression system, not a side activity. Every bot
		// aims for at least +6, while a stable per-character/per-family personality
		// decides who risks +7, +8 or +9. The actual attempt still goes through
		// DoRefine(false), so every result pays the real fee, consumes real materials
		// and can burn at the normal server success rate.
		const DWORD familyVnum = item->GetVnum() >= item->GetRefineLevel()
				? item->GetVnum() - item->GetRefineLevel() : item->GetVnum();
		const int wearCell = item->FindEquipCell(ch);
		const DWORD seed = ch->GetPlayerID() ^ (familyVnum * 0x9e3779b9U) ^
				((DWORD)(wearCell + 2) * 0x85ebca6bU);
		const DWORD ambition = PlayerBotNavHash(seed ^ 0x52454649U) % 1000U;
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = stateIt != s_mapPlayerBotAIStates.end()
				? stateIt->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// Gear specialists deliberately accept more upgrade risk. Careful collectors
		// still has a small chance to become the lucky +8/+9 outlier, but normally
		// protects the equipment already earned.
		const DWORD plusNineChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 120U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 20U : 50U);
		const DWORD plusEightChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 320U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 90U : 150U);
		const DWORD plusSevenChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 650U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 290U : 400U);
		if (ambition < plusNineChance)
			return 9; // exceptional 5% cohort
		if (ambition < plusEightChance)
			return 8; // another 10%
		if (ambition < plusSevenChance)
			return 7; // another 25%
		return 6;
	}

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category)
	{
		if (!ch || vnum == 0)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) < 0)
			return false;

		long long price = proto->dwShopBuyPrice > 0 ? proto->dwShopBuyPrice : proto->dwGold;
		price = std::max<long long>(100, price);
		if (ch->GetGold() < price)
			return false;

		LPITEM item = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!item)
			return false;
		ch->PointChange(POINT_GOLD, -price);
		sys_log(0, "PLAYERBOT_GEAR: bought progression %s pid=%u name=%s vnum=%u required_level=%d price=%lld",
				category ? category : "gear", ch->GetPlayerID(), ch->GetName(), vnum,
				item->GetLevelLimit(), price);
		return true;
	}

	int CountPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int count = 0;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (worn && worn->GetType() == ITEM_WEAPON && worn->GetSubType() == WEAPON_ARROW)
			count += worn->GetCount();
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW)
				count += item->GetCount();
		}
		return count;
	}

	bool EnsurePlayerBotArrowsEquipped(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (worn && worn->GetType() == ITEM_WEAPON && worn->GetSubType() == WEAPON_ARROW &&
				worn->GetCount() > 0)
			return true;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW &&
					item->GetCount() > 0 && ch->EquipItem(item, WEAR_ARROW))
				return true;
		}
		return false;
	}

	bool NeedsPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2)
			return false;
		return CountPlayerBotArrows(ch) < PLAYERBOT_ARROW_RESTOCK_THRESHOLD;
	}

	long long GetPlayerBotNpcPurchasePrice(const TItemTable* proto, int count)
	{
		if (!proto || count <= 0)
			return 0;
		if (IS_SET(proto->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			return proto->dwGold == 0 ? count : count / proto->dwGold;
		return (long long)proto->dwGold * count;
	}

	DWORD GetPlayerBotNpcSellUnitPrice(LPITEM item)
	{
		if (!item || !item->GetProto() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return 0;

		DWORD price = item->GetShopBuyPrice();
		if (IS_SET(item->GetFlag(), ITEM_FLAG_COUNT_PER_1GOLD))
			price = price == 0 ? 1 : 1 / price;
		price /= 5;
		price -= price * 3 / 100;
		return price;
	}

	enum EPlayerBotPotionSupply
	{
		PLAYERBOT_POTION_SUPPLY_HP = 0,
		PLAYERBOT_POTION_SUPPLY_SP,
		PLAYERBOT_POTION_SUPPLY_GREEN,
		PLAYERBOT_POTION_SUPPLY_PURPLE,
		PLAYERBOT_POTION_SUPPLY_NONE
	};

	EPlayerBotPotionSupply GetPlayerBotPotionSupply(DWORD vnum)
	{
		if (vnum == 27051 || (vnum >= 27001 && vnum <= 27003))
			return PLAYERBOT_POTION_SUPPLY_HP;
		if (vnum == 27052 || (vnum >= 27004 && vnum <= 27006))
			return PLAYERBOT_POTION_SUPPLY_SP;
		if (vnum == 27053 || (vnum >= 27100 && vnum <= 27102))
			return PLAYERBOT_POTION_SUPPLY_GREEN;
		if (vnum == 27054 || (vnum >= 27103 && vnum <= 27105))
			return PLAYERBOT_POTION_SUPPLY_PURPLE;
		return PLAYERBOT_POTION_SUPPLY_NONE;
	}

	DWORD GetPlayerBotPotionSupplyLimit(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		const bool lowLevel = !ch || ch->GetLevel() <= 10;
		const bool mage = ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		switch (supply)
		{
			// A bot with yang in the bank should carry a real belt, not a token
			// one: potions are cheap next to what it earns, and running dry is
			// what sends it home in the middle of a good spot. These are also the
			// limits the excess-potion sale trims down to, so they have to move
			// together with the purchase below.
			case PLAYERBOT_POTION_SUPPLY_HP:     return lowLevel ? 160 : 800;
			case PLAYERBOT_POTION_SUPPLY_SP:     return lowLevel ? (mage ? 100 : 50) : 600;
			case PLAYERBOT_POTION_SUPPLY_GREEN:  return 30;
			case PLAYERBOT_POTION_SUPPLY_PURPLE: return 30;
			default: return 0;
		}
	}

	DWORD CountPlayerBotPotionSupply(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		if (!ch || supply == PLAYERBOT_POTION_SUPPLY_NONE)
			return 0;
		DWORD count = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && GetPlayerBotPotionSupply(item->GetVnum()) == supply)
				count += item->GetCount();
		}
		return count;
	}

	bool HasPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			if (CountPlayerBotPotionSupply(ch, kind) >
					GetPlayerBotPotionSupplyLimit(ch, kind))
				return true;
		}
		return false;
	}

	bool CanMergePlayerBotPotionStacks(LPITEM destination, LPITEM source)
	{
		if (!destination || !source || destination == source ||
				destination->GetVnum() != source->GetVnum() ||
				GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE ||
				!destination->IsStackable() || !source->IsStackable() ||
				IS_SET(destination->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
				IS_SET(source->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			if (destination->GetSocket(socket) != source->GetSocket(socket))
				return false;
		for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			if (destination->GetAttributeType(attr) != source->GetAttributeType(attr) ||
					destination->GetAttributeValue(attr) != source->GetAttributeValue(attr))
				return false;
		return true;
	}

	bool CompactPlayerBotPotionStacks(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		DWORD movedUnits = 0;
		DWORD removedStacks = 0;
		for (WORD destinationCell = 0; destinationCell < INVENTORY_MAX_NUM; ++destinationCell)
		{
			LPITEM destination = ch->GetInventoryItem(destinationCell);
			if (!destination || destination->GetCount() >= 200 ||
					GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE)
				continue;
			for (WORD sourceCell = destinationCell + 1;
					sourceCell < INVENTORY_MAX_NUM && destination->GetCount() < 200;
					++sourceCell)
			{
				LPITEM source = ch->GetInventoryItem(sourceCell);
				if (!CanMergePlayerBotPotionStacks(destination, source))
					continue;
				const DWORD sourceCount = source->GetCount();
				const DWORD transfer = std::min<DWORD>(200 - destination->GetCount(), sourceCount);
				if (transfer == 0)
					continue;
				destination->SetCount(destination->GetCount() + transfer);
				source->SetCount(sourceCount - transfer);
				movedUnits += transfer;
				if (transfer == sourceCount)
					++removedStacks;
			}
		}
		if (movedUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: compacted potions pid=%u name=%s moved=%u freed_stacks=%u",
					ch->GetPlayerID(), ch->GetName(), movedUnits, removedStacks);
		return movedUnits > 0;
	}

	bool SellPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// Sell weaker variants first, while retaining a bounded combat/travel reserve.
		const DWORD saleOrder[] = {
			27051, 27001, 27002, 27003,
			27052, 27004, 27005, 27006,
			27053, 27100, 27101, 27102,
			27054, 27103, 27104, 27105
		};
		DWORD soldUnits = 0;
		long long earnedGold = 0;
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			DWORD total = CountPlayerBotPotionSupply(ch, kind);
			const DWORD keep = GetPlayerBotPotionSupplyLimit(ch, kind);
			if (total <= keep)
				continue;
			DWORD excess = total - keep;
			for (size_t order = 0;
					order < sizeof(saleOrder) / sizeof(saleOrder[0]) && excess > 0; ++order)
			{
				if (GetPlayerBotPotionSupply(saleOrder[order]) != kind)
					continue;
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM && excess > 0; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != saleOrder[order])
						continue;
					const DWORD unitPrice = GetPlayerBotNpcSellUnitPrice(item);
					if (unitPrice == 0)
						continue;
					const DWORD count = std::min<DWORD>(excess, item->GetCount());
					item->SetCount(item->GetCount() - count);
					ch->PointChange(POINT_GOLD, (long long)unitPrice * count);
					excess -= count;
					soldUnits += count;
					earnedGold += (long long)unitPrice * count;
				}
			}
		}
		if (soldUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: sold excess potions pid=%u name=%s units=%u earned=%lld gold=%lld",
					ch->GetPlayerID(), ch->GetName(), soldUnits, earnedGold,
					(long long)ch->GetGold());
		return soldUnits > 0;
	}

	bool RaisePlayerBotEmergencyGold(LPCHARACTER ch, long long requiredGold,
			const char* reason)
	{
		if (!ch || ch->GetGold() >= requiredGold)
			return false;

		// The native NPC shop accepts potions too. Sell only as many surplus units
		// as are required to restore an essential weapon/ammunition purchase. Blue
		// potions go first and both HP/SP reserves remain protected.
		const DWORD potionVnums[] = {
			27004, 27005, 27006, 27052,
			27001, 27002, 27003, 27051
		};
		for (size_t v = 0; v < sizeof(potionVnums) / sizeof(potionVnums[0]); ++v)
		{
			const bool bluePotion = v < 4;
			const DWORD reserve = bluePotion ? 10 : 30;
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != potionVnums[v] || item->GetCount() <= reserve)
					continue;

				const DWORD price = GetPlayerBotNpcSellUnitPrice(item);
				if (price == 0)
					continue;

				const long long deficit = requiredGold - ch->GetGold();
				const DWORD available = item->GetCount() - reserve;
				DWORD count = (DWORD)((deficit + price - 1) / price);
				count = std::max<DWORD>(1, std::min<DWORD>(count, available));
				item->SetCount(item->GetCount() - count);
				ch->PointChange(POINT_GOLD, (long long)price * count);
				sys_log(0, "PLAYERBOT_GEAR: emergency sale pid=%u name=%s reason=%s vnum=%u count=%u earned=%lld total_gold=%lld required=%lld",
						ch->GetPlayerID(), ch->GetName(), reason ? reason : "supply",
						potionVnums[v], count, (long long)price * count,
						(long long)ch->GetGold(), requiredGold);
				if (ch->GetGold() >= requiredGold)
					return true;
			}
		}
		return ch->GetGold() >= requiredGold;
	}

	bool BuyPlayerBotArrowsAtMerchant(LPCHARACTER ch)
	{
		if (!NeedsPlayerBotArrows(ch))
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_WOODEN_ARROW_VNUM);
		if (!proto)
			return false;

		const long long smallPrice = GetPlayerBotNpcPurchasePrice(
				proto, PLAYERBOT_ARROW_SMALL_BUNDLE);
		if (ch->GetGold() < smallPrice)
			RaisePlayerBotEmergencyGold(ch, smallPrice, "arrows");

		int bundle = 0;
		long long price = GetPlayerBotNpcPurchasePrice(
				proto, PLAYERBOT_ARROW_LARGE_BUNDLE);
		if (price > 0 && ch->GetGold() >= price)
			bundle = PLAYERBOT_ARROW_LARGE_BUNDLE;
		else
		{
			price = smallPrice;
			if (price > 0 && ch->GetGold() >= price)
				bundle = PLAYERBOT_ARROW_SMALL_BUNDLE;
		}
		if (bundle == 0)
			return false;

		LPITEM arrows = ch->AutoGiveItem(
				PLAYERBOT_WOODEN_ARROW_VNUM, bundle, -1, false);
		if (!arrows)
			return false;
		ch->PointChange(POINT_GOLD, -price);
		const bool equipped = EnsurePlayerBotArrowsEquipped(ch);
		sys_log(0, "PLAYERBOT_GEAR: bought wooden arrows pid=%u name=%s vnum=%u count=%d price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_WOODEN_ARROW_VNUM,
				bundle, price, equipped ? 1 : 0);
		return true;
	}

	bool EquipFirstAvailablePlayerBotWeapon(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotWeapon(ch, item))
				continue;

			const DWORD vnum = item->GetVnum();
			if (ch->CanEquipNow(item, TItemPos(INVENTORY, cell)) && ch->EquipItem(item))
			{
				sys_log(0, "PLAYERBOT_AI: equipped weapon pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), vnum);
				return ch->GetWear(WEAR_WEAPON) != NULL;
			}
		}

		return false;
	}

	bool BuyPlayerBotEmergencyWeapon(LPCHARACTER ch)
	{
		if (!ch || ch->GetWear(WEAR_WEAPON))
			return ch && ch->GetWear(WEAR_WEAPON);

		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		const long long price = GetPlayerBotEmergencyWeaponPrice(ch);
		if (vnum == 0)
			return false;
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, "weapon");
		if (ch->GetGold() < price)
			return false;

		LPITEM weapon = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!weapon)
			return false;

		ch->PointChange(POINT_GOLD, -price);
		const bool equipped = ch->EquipItem(weapon);

		sys_log(0, "PLAYERBOT_AI: bought emergency weapon pid=%u name=%s vnum=%u price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), vnum, price, equipped ? 1 : 0);
		return equipped;
	}

	bool PrepareWeapon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		LPITEM equippedWeapon = ch->GetWear(WEAR_WEAPON);
		if (equippedWeapon && IsPlayerBotWeapon(ch, equippedWeapon))
		{
			state.dwEmergencyScavengeUntil = 0;
			if (equippedWeapon->GetSubType() == WEAPON_BOW)
				return EnsurePlayerBotArrowsEquipped(ch);
			return true;
		}
		if (equippedWeapon)
		{
			const DWORD wrongVnum = equippedWeapon->GetVnum();
			if (ch->GetEmptyInventory(equippedWeapon->GetSize()) >= 0)
			{
				ch->UnequipItem(equippedWeapon);
				sys_log(0, "PLAYERBOT_AI: unequipped profession-incompatible weapon pid=%u name=%s vnum=%u group=%u",
						ch->GetPlayerID(), ch->GetName(), wrongVnum, ch->GetSkillGroup());
			}
			if (ch->GetWear(WEAR_WEAPON))
				return false;
		}

		if (!ch->IsItemLoaded() || dwNow < state.dwNextGearAttemptTime)
			return false;

		state.dwNextGearAttemptTime = dwNow + PLAYERBOT_GEAR_RETRY_INTERVAL;

		if (EquipFirstAvailablePlayerBotWeapon(ch))
		{
			state.dwEmergencyScavengeUntil = 0;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		const DWORD dwStarterChestVnum = GetStarterChestVnum(ch->GetJob());
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD chestVnum = item->GetVnum();
			const bool classStarterChest = (dwStarterChestVnum != 0 && chestVnum == dwStarterChestVnum);
			const bool progressionChest = (chestVnum >= 50187 && chestVnum <= 50196);
			const int progressionLevel = (chestVnum == 50187) ? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarterChest && (!progressionChest || ch->GetLevel() < progressionLevel))
				continue;

			sys_log(0, "PLAYERBOT_AI: opening weapon recovery chest pid=%u name=%s vnum=%u cell=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, cell);
			ch->UseItem(TItemPos(INVENTORY, cell));
			if (!EquipFirstAvailablePlayerBotWeapon(ch))
				return false;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		// No remote purchase or free fallback.  The update loop first gives the
		// bot a chance to collect an owned weapon drop and then starts a visible
		// trip to the real Weapon Merchant.  Archer arrows follow the same rule.
		if (dwNow >= state.dwNextGearLogTime)
		{
			state.dwNextGearLogTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
			sys_err("PLAYERBOT_AI: idle without weapon pid=%u name=%s expected_chest=%u",
					ch->GetPlayerID(), ch->GetName(), dwStarterChestVnum);
		}

		return false;
	}

	bool ManagePlayerBotProgressionChests(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() ||
			dwNow < state.dwNextProgressionChestCheckTime)
			return false;
		state.dwNextProgressionChestCheckTime = dwNow + 10000 +
				(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);

		// The seed historically supplied one starter chest and the stock
		// give_basic_weapon quest supplied another on first login. Since every
		// apprentice chest contains the next tier, that duplicated the entire
		// progression chain. These boxes are one-per-character rewards: retain one
		// copy of each tier and remove only the artificial duplicates.
		std::map<DWORD, bool> seenProgressionChests;
		DWORD removedChestUnits = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			const bool progression = (vnum >= 50187 && vnum <= 50196) ||
					vnum == 50212 || vnum == 50213;
			if (!progression)
				continue;

			const DWORD count = std::max<DWORD>(1, item->GetCount());
			if (seenProgressionChests.find(vnum) != seenProgressionChests.end())
			{
				removedChestUnits += count;
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DUPLICATE_CHEST");
				continue;
			}

			seenProgressionChests[vnum] = true;
			if (count > 1)
			{
				removedChestUnits += count - 1;
				item->SetCount(1);
			}
		}
		if (removedChestUnits > 0)
			sys_log(0, "PLAYERBOT_GEAR: removed duplicate progression chests pid=%u name=%s units=%u",
					ch->GetPlayerID(), ch->GetName(), removedChestUnits);

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if ((target && !target->IsDead()) ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < 3000))
			return false;

		const DWORD starterVnum = GetStarterChestVnum(ch->GetJob());
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD chestVnum = item->GetVnum();
			const bool classStarter = starterVnum != 0 && chestVnum == starterVnum;
			const bool progression = chestVnum >= 50187 && chestVnum <= 50196;
			const int requiredLevel = chestVnum == 50187
					? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarter && (!progression || ch->GetLevel() < requiredLevel))
				continue;

			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			state.dwNextGearAttemptTime = 0;
			sys_log(0, "PLAYERBOT_GEAR: opened progression chest pid=%u name=%s vnum=%u level=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel());
			return true;
		}
		return false;
	}

	bool UseHealthPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ch->GetMaxHP() <= 0 || ch->GetHP() * 100 > ch->GetMaxHP() * PLAYERBOT_POTION_HP_PERCENT)
			return false;

		if (dwNow < state.dwNextPotionTime)
			return false;

		state.dwNextPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27051 is the beginner red potion supplied by the level-one chest.
		const DWORD redPotionVnums[] = { 27051, 27001, 27002, 27003 };
		for (size_t potionIndex = 0; potionIndex < sizeof(redPotionVnums) / sizeof(redPotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != redPotionVnums[potionIndex])
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used health potion pid=%u name=%s vnum=%u hp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetHP(), ch->GetMaxHP());
					return true;
				}
			}
		}

		if (dwNow >= state.dwNextPotionLogTime)
		{
			state.dwNextPotionLogTime = dwNow + PLAYERBOT_POTION_LOG_INTERVAL;
			sys_log(0, "PLAYERBOT_AI: no usable health potion pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
		}

		return false;
	}

	bool UseManaPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ch->GetMaxSP() <= 0 || ch->GetSP() * 100 > ch->GetMaxSP() * PLAYERBOT_POTION_SP_PERCENT)
			return false;

		if (dwNow < state.dwNextManaPotionTime)
			return false;

		state.dwNextManaPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27052 is the beginner blue potion, followed by standard small, medium, large blue potions.
		const DWORD bluePotionVnums[] = { 27052, 27004, 27005, 27006 };
		for (size_t potionIndex = 0; potionIndex < sizeof(bluePotionVnums) / sizeof(bluePotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != bluePotionVnums[potionIndex])
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used mana potion pid=%u name=%s vnum=%u sp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetSP(), ch->GetMaxSP());
					return true;
				}
			}
		}

		return false;
	}

	bool UseUtilityPotions(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		const bool activeCombat = target && !target->IsDead() &&
				(target->IsMonster() || target->IsStone());
		const bool importantFight = activeCombat && (target->IsStone() ||
				(target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS));
		// One third of ordinary grinders plans a longer session and uses attack-speed
		// potions as well. Every bot uses them for Metins/bosses, but nobody drinks
		// one merely while waiting at an NPC or recovering from death.
		const bool longGrindingSession = activeCombat && target->IsMonster() &&
				state.bLongTermGoal == BOT_GOAL_LEVEL_UP &&
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x47524545U) % 3U) == 0;
		const bool shouldUseGreen = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(importantFight || longGrindingSession);

		// 1. Green Potion (Zielona Mikstura - Attack Speed)
		if (shouldUseGreen && ch->FindAffect(AFFECT_ATT_SPEED) == NULL &&
				state.mapBuffActiveUntil[27102] <= dwNow)
		{
			const DWORD greenPotionVnums[] = { 27102, 27101, 27100, 27053 };
			for (size_t i = 0; i < sizeof(greenPotionVnums) / sizeof(greenPotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != greenPotionVnums[i])
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						// FindAffect is authoritative for the real item duration. This short
						// guard only prevents a broken proto from being consumed every tick.
						state.mapBuffActiveUntil[27102] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used green potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		// 2. Purple Potion (Fioletowa Mikstura - Movement Speed). Use it for travel,
		// loot runs and the approach to a distant target, not while standing at NPCs.
		const bool shouldUsePurple = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(!activeCombat || DISTANCE_APPROX(ch->GetX() - target->GetX(),
					target->GetY() - ch->GetY()) > 500);
		if (shouldUsePurple && ch->FindAffect(AFFECT_MOV_SPEED) == NULL &&
				state.mapBuffActiveUntil[27105] <= dwNow)
		{
			const DWORD purplePotionVnums[] = { 27105, 27104, 27103, 27054 };
			for (size_t i = 0; i < sizeof(purplePotionVnums) / sizeof(purplePotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != purplePotionVnums[i])
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						state.mapBuffActiveUntil[27105] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used purple potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		return false;
	}

	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	bool ManagePlayerBotHorse(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2) || state.bVisitingShop ||
				state.bVisitingBiologist)
			return false;
		if (!state.bVisitingStable && dwNow < state.dwNextHorseCheckTime)
			return false;
		if (!state.bVisitingStable)
			state.dwNextHorseCheckTime = dwNow + 3000;

		const BYTE horseLevel = ch->GetHorseLevel();
		if (!CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			return false;
		}

		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!state.bVisitingStable && victim && !victim->IsDead())
			return false;

		if (!state.bVisitingStable)
		{
			int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
			if (delivered < horseLevel)
			{
				delivered = horseLevel;
				ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
			}
			state.bVisitingStable = true;
			state.dwNextHorseActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_HORSE: going to stable pid=%u name=%s medals=%d horse_level=%u delivered=%d",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM), horseLevel, delivered);
		}

		SetPlayerBotGoal(ch, state, BOT_GOAL_HORSE, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_STABLE, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;
		const long stableX = inM2 ? PLAYERBOT_M2_STABLE_BOY_X : PLAYERBOT_STABLE_BOY_X;
		const long stableY = inM2 ? PLAYERBOT_M2_STABLE_BOY_Y : PLAYERBOT_STABLE_BOY_Y;
		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), stableX, stableY,
				inM2 ? 0x4d324853U : 0x484f5253U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingStable = false;
				state.dwNextHorseCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_HORSE: route failed pid=%u name=%s map=%ld from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "stable_interaction");
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextHorseActionTime == 0)
		{
			state.dwNextHorseActionTime = dwNow + number(5000, 15000);
			return true;
		}
		if (dwNow < state.dwNextHorseActionTime)
			return true;

		if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(15000, 30000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		ch->RemoveSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM, 1);
		int delivered = std::max(0, ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG));
		delivered = std::max(delivered, (int)ch->GetHorseLevel()) + 1;
		delivered = std::min(delivered, 21);
		ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_FLAG, delivered);
		ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_DELIVERY_TIME_FLAG, get_global_time());
		if (ch->GetHorseLevel() < delivered)
			ch->SetHorseLevel(delivered);
		ch->SetSkillLevel(131, 10);

		const char* stage = delivered >= 21 ? "military" :
				(delivered >= 11 ? "combat" : "normal");
		sys_log(0, "PLAYERBOT_HORSE: medal delivered pid=%u name=%s delivered=%d horse_level=%u stage=%s medals_left=%d",
				ch->GetPlayerID(), ch->GetName(), delivered, ch->GetHorseLevel(), stage,
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM));

		if (delivered >= 21 || !CanPlayerBotAdvanceHorse(ch) ||
				ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <= 0)
		{
			state.bVisitingStable = false;
			state.dwNextHorseActionTime = 0;
			state.dwNextHorseCheckTime = dwNow + number(30000, 60000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		state.dwNextHorseActionTime = dwNow + number(5000, 10000);
		return true;
	}

	// A small, stable slice of the M1 population fishes. Careful collectors are the
	// natural anglers -- pearls are a collector's prize -- but a few other
	// personalities join them so the bank is never one archetype deep. The roll is
	// derived from the player id, so a bot keeps the same hobby across restarts.
	// The rod is a weapon and carries a level limit like any other. Asking the
	// item what it needs keeps this honest: the hand-picked level 10 was below
	// the rod's real requirement of 30, so a level-13 bot bought tackle it could
	// never equip and then retried the same failing step for good.
	bool CanPlayerBotUseFishingRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_ROD_VNUM);
		if (!proto)
			return false;
		return GetPlayerBotProtoLevelLimit(proto) <= (int)ch->GetLevel();
	}

	bool IsPlayerBotAngler(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!CanPlayerBotUseFishingRod(ch))
			return false;
		const DWORD roll = PlayerBotNavHash(ch->GetPlayerID() ^ 0x46495348U) % 100U;
		return state.bPersonality == BOT_PERSONALITY_CAREFUL_COLLECTOR
				? roll < 20U : roll < 2U;
	}

	// Anglers spread out along the shoreline rather than stacking on one tile.
	// The band is walked along Y because that is the way this stretch of bank
	// runs; every resulting point is inside the verified standable rectangle.
	void GetPlayerBotFishingStand(DWORD playerID, long& standX, long& standY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ 0x42414e4bU);
		standX = PLAYERBOT_FISHING_BANK_X + (long)(hash % 5U) * 50;
		standY = PLAYERBOT_FISHING_BANK_Y + (long)((hash / 5U) % 10U) * 50;
	}

	bool IsPlayerBotHoldingRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return rod && rod->GetType() == ITEM_ROD;
	}

	bool EquipPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotHoldingRod(ch))
			return true;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_ROD)
				continue;

			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && !ch->UnequipItem(worn))
				return false;
			if (ch->EquipItem(item))
			{
				sys_log(0, "PLAYERBOT_FISHING: rod equipped pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum());
				return true;
			}
		}
		return false;
	}

	// The rod is worthless in a fight, so a finished session always puts the real
	// weapon back before the bot rejoins the grind.
	void StowPlayerBotRod(LPCHARACTER ch)
	{
		if (!ch)
			return;
		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!rod || rod->GetType() != ITEM_ROD)
			return;
		if (!ch->UnequipItem(rod))
			return;
		EquipFirstAvailablePlayerBotWeapon(ch);
	}

	// Bait does not sit in the pouch while fishing: using it moves its value into
	// the rod's socket 2, which is what the engine actually checks before a cast.
	bool BaitPlayerBotRod(LPCHARACTER ch)
	{
		LPITEM rod = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		if (!rod || rod->GetType() != ITEM_ROD)
			return false;
		if (rod->GetSocket(2) != 0)
			return true;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_FISHING_BAIT_VNUM)
				continue;
			ch->UseItem(TItemPos(INVENTORY, cell));
			return rod->GetSocket(2) != 0;
		}
		return false;
	}

	// One item per pass. Gutting a fish and prying a shell open both run through
	// UseItem, which frees the very inventory slot being iterated over.
	bool ProcessPlayerBotCatch(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD vnum = item->GetVnum();
			const bool aliveFish = item->GetType() == ITEM_FISH &&
					item->GetSubType() == FISH_ALIVE;
			if (!aliveFish && vnum != PLAYERBOT_SHELLFISH_VNUM)
				continue;
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				continue;

			sys_log(0, "PLAYERBOT_FISHING: processed catch pid=%u name=%s vnum=%u kind=%s",
					ch->GetPlayerID(), ch->GetName(), vnum,
					aliveFish ? "fish" : "shellfish");
			return true;
		}
		return false;
	}

	bool EndPlayerBotFishingSession(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		if (ch && ch->m_pkFishingEvent)
			ch->fishing_take();

		state.bFishingSession = false;
		state.bIsFishing = false;
		state.dwFishingCastTime = 0;
		state.dwFishingSessionEndTime = 0;
		state.dwNextFishingActionTime = 0;
		state.dwNextFishingCheckTime = dwNow +
				number(PLAYERBOT_FISHING_REST_MIN, PLAYERBOT_FISHING_REST_MAX);
		StowPlayerBotRod(ch);
		ClearPlayerBotRoute(state, true);
		if (ch)
			sys_log(0, "PLAYERBOT_FISHING: session over pid=%u name=%s pearls=%d/%d/%d reason=%s",
					ch->GetPlayerID(), ch->GetName(),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_FIRST_VNUM + 1),
					ch->CountSpecifyItem(PLAYERBOT_PEARL_LAST_VNUM),
					reason ? reason : "?");
		return false;
	}

	// Rod and bait both come from the Rybak, who stands on the bank the bots fish
	// from, so restocking and fishing share one walk.
	bool RestockPlayerBotTackle(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;

		bool bought = false;
		if (!IsPlayerBotHoldingRod(ch) && ch->CountSpecifyItem(PLAYERBOT_FISHING_ROD_VNUM) <= 0)
		{
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_ROD_VNUM);
			if (!proto)
				return false;
			const long long price = GetPlayerBotNpcPurchasePrice(proto, 1);
			if (ch->GetGold() < price)
				RaisePlayerBotEmergencyGold(ch, price, "fishing_rod");
			if (price <= 0 || ch->GetGold() < price)
				return false;
			if (!ch->AutoGiveItem(PLAYERBOT_FISHING_ROD_VNUM, 1, -1, false))
				return false;
			ch->PointChange(POINT_GOLD, -price);
			bought = true;
			sys_log(0, "PLAYERBOT_FISHING: bought rod pid=%u name=%s vnum=%u price=%lld",
					ch->GetPlayerID(), ch->GetName(), PLAYERBOT_FISHING_ROD_VNUM, price);
		}

		if (ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) < PLAYERBOT_FISHING_BAIT_RESTOCK)
		{
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_FISHING_BAIT_VNUM);
			if (!proto)
				return bought;
			const long long price = GetPlayerBotNpcPurchasePrice(proto, PLAYERBOT_FISHING_BAIT_BUNDLE);
			if (ch->GetGold() < price)
				RaisePlayerBotEmergencyGold(ch, price, "fishing_bait");
			if (price <= 0 || ch->GetGold() < price)
				return bought;
			if (!ch->AutoGiveItem(PLAYERBOT_FISHING_BAIT_VNUM, PLAYERBOT_FISHING_BAIT_BUNDLE, -1, false))
				return bought;
			ch->PointChange(POINT_GOLD, -price);
			bought = true;
			sys_log(0, "PLAYERBOT_FISHING: bought bait pid=%u name=%s vnum=%u count=%d price=%lld",
					ch->GetPlayerID(), ch->GetName(), PLAYERBOT_FISHING_BAIT_VNUM,
					PLAYERBOT_FISHING_BAIT_BUNDLE, price);
		}
		return bought;
	}

	bool ManagePlayerBotFishing(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		if (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1)
		{
			// The rod must not travel to a hunting map in the weapon slot.
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "left_m1");
			return false;
		}
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive)
		{
			if (state.bFishingSession)
				EndPlayerBotFishingSession(ch, state, dwNow, "town_errand");
			return false;
		}

		if (!state.bFishingSession)
		{
			if (dwNow < state.dwNextFishingCheckTime || !IsPlayerBotAngler(ch, state))
				return false;
			// Never walk off mid-fight; finish the pack first.
			LPCHARACTER victim = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (victim && !victim->IsDead())
				return false;

			state.bFishingSession = true;
			state.bIsFishing = false;
			state.dwFishingCastTime = 0;
			state.dwNextFishingActionTime = 0;
			state.dwNextFishingProgressLogTime = 0;
			state.dwFishingSessionEndTime = dwNow +
					number(PLAYERBOT_FISHING_SESSION_MIN, PLAYERBOT_FISHING_SESSION_MAX);
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_FISHING: heading for the bank pid=%u name=%s level=%u personality=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bPersonality);
		}

		// A session only ends between casts, so a fish already on the hook is
		// still landed.
		if (dwNow >= state.dwFishingSessionEndTime && !state.bIsFishing)
			return EndPlayerBotFishingSession(ch, state, dwNow, "session_finished");

		SetPlayerBotGoal(ch, state, BOT_GOAL_FISHING, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_FISHING, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Rod first, then worms: both come from the Rybak, who stands a short walk
		// upstream of the bank. Running out of bait sends the bot back to him.
		const bool needsTackle =
				(!IsPlayerBotHoldingRod(ch) &&
					ch->CountSpecifyItem(PLAYERBOT_FISHING_ROD_VNUM) <= 0) ||
				ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
					PLAYERBOT_FISHING_BAIT_RESTOCK;

		long destX = 0, destY = 0;
		if (needsTackle)
		{
			GetPlayerBotNpcApproach(ch->GetPlayerID(), PLAYERBOT_FISHERMAN_X,
					PLAYERBOT_FISHERMAN_Y, 0x46495348U, destX, destY);
		}
		else
		{
			GetPlayerBotFishingStand(ch->GetPlayerID(), destX, destY);
			// The bank spots are hand-picked world coordinates. server_attr is the
			// only authority on whether one is standable, and the town services
			// already learned that a hand-picked point can be a cell the navigation
			// refuses. Snap to a verified walkable cell before walking at it.
			CPlayerBotNavigation& navigation =
					CPlayerBotNavigation::instance(ch->GetMapIndex());
			PIXEL_POSITION bank;
			if (navigation.Init(ch->GetMapIndex()) &&
					navigation.FindNearestWalkableWorld(destX, destY, 12, bank,
							ch->GetPlayerID()))
			{
				destX = bank.x;
				destY = bank.y;
			}
		}

		// bFishingSession exempts a bot from the inactivity watchdog - standing
		// still at the bank is the activity - which also means a session that goes
		// wrong is completely silent. One throttled line says where it actually is.
		if (dwNow >= state.dwNextFishingProgressLogTime)
		{
			state.dwNextFishingProgressLogTime = dwNow + PLAYERBOT_FISHING_PROGRESS_LOG;
			sys_log(0, "PLAYERBOT_FISHING: progress pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) dist=%ld tackle=%d rod=%d bait=%d casting=%d stuck=%u riding=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY,
					(long)DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY),
					needsTackle ? 1 : 0, IsPlayerBotHoldingRod(ch) ? 1 : 0,
					ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM),
					state.bIsFishing ? 1 : 0, (unsigned int)state.bStuckCounter,
					ch->IsRiding() ? 1 : 0);
		}

		// A session that never reaches the water is the worst of both worlds: the
		// bot has paid for tackle, stopped hunting, and walks the same failing
		// approach for as long as the server runs. Observed on a live world - a
		// level-34 bot stood at the Rybak with a rod in its bag and never cast.
		// Give up out loud instead, so the log says which leg failed.
		if (state.dwFishingSessionEndTime != 0 && !state.bIsFishing &&
				dwNow >= state.dwFishingSessionEndTime)
		{
			sys_err("PLAYERBOT_FISHING: never reached the water pid=%u name=%s pos=(%ld,%ld) dest=(%ld,%ld) tackle=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY, needsTackle ? 1 : 0);
			return EndPlayerBotFishingSession(ch, state, dwNow, "never_reached_water");
		}

		if (DISTANCE_APPROX(ch->GetX() - destX, ch->GetY() - destY) >
				PLAYERBOT_FISHING_ARRIVE)
		{
			// Riding there is fine; the line simply cannot go in from a saddle.
			if (MovePlayerBot(ch, destX, destY, dwNow, 16, true, true) ||
					state.bStuckCounter < PLAYERBOT_FISHING_STUCK_LIMIT)
				return true;

			// Out of route. The tackle leg cannot be skipped - only the Rybak sells
			// rods - but the bank can be: fishing() in r40250 asks for a
			// non-blocking tile, a rod of type ITEM_ROD and bait in socket 2, and
			// never looks for water at all (it computes a facing offset and then
			// discards it). Casting where the bot already stands is therefore a
			// real cast, and it beats spending the entire session walking at a
			// bank the navigation cannot reach.
			if (needsTackle ||
					IsPlayerBotPositionBlocked(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			{
				sys_err("PLAYERBOT_FISHING: route failed pid=%u name=%s from=(%ld,%ld) to=(%ld,%ld) tackle=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
						destX, destY, needsTackle ? 1 : 0);
				return EndPlayerBotFishingSession(ch, state, dwNow, "route_failed");
			}

			sys_log(0, "PLAYERBOT_FISHING: bank unreachable, casting in place pid=%u name=%s pos=(%ld,%ld) bank=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
					destX, destY);
			state.bStuckCounter = 0;
			ClearPlayerBotRoute(state, true);
		}

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "fishing");
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_STANDING);

		if (needsTackle)
		{
			if (!RestockPlayerBotTackle(ch, state, dwNow))
				return EndPlayerBotFishingSession(ch, state, dwNow, "cannot_afford_tackle");
			return true;
		}
		if (!EquipPlayerBotRod(ch))
			return EndPlayerBotFishingSession(ch, state, dwNow, "rod_not_equippable");

		if (dwNow < state.dwNextFishingActionTime)
			return true;

		LPITEM rod = ch->GetWear(WEAR_WEAPON);
		if (!state.bIsFishing && rod && rod->GetSocket(2) == 0 && !BaitPlayerBotRod(ch))
		{
			// The pouch ran dry between passes; the walk back to the Rybak is
			// picked up by the tackle check at the top of the next pass.
			state.dwNextFishingActionTime = dwNow + number(1000, 2000);
			return true;
		}

		// The engine holds the whole cast in one event: step 0 is the line in the
		// water, step 1 means a fish is on and starts the 6 s window to pull.
		fishing::fishing_event_info* info = ch->m_pkFishingEvent
				? dynamic_cast<fishing::fishing_event_info*>(ch->m_pkFishingEvent->info)
				: NULL;

		if (!state.bIsFishing || !info)
		{
			if (info)
			{
				// A cast survived from an earlier pass; adopt it rather than
				// stacking a second one.
				state.bIsFishing = true;
				state.dwFishingCastTime = dwNow;
				return true;
			}
			if (state.bIsFishing)
			{
				// The event ended on its own -- the bite window elapsed. The engine
				// already cleared the bait, so the next pass re-baits and recasts.
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				ProcessPlayerBotCatch(ch);
				return true;
			}

			// CHARACTER::fishing() dereferences the sectree map and the tile under
			// the bot without checking either, so never call it blind.
			if (!ch->GetSectree() ||
					!SECTREE_MANAGER::instance().GetMap(ch->GetMapIndex()))
			{
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}

			// Face straight across at the river rather than along the bank: the
			// water lies due east of this stretch.
			ch->SetRotationToXY(PLAYERBOT_FISHING_WATER_X, ch->GetY());
			ch->fishing();
			if (!ch->m_pkFishingEvent)
			{
				// Blocked tile or missing bait; step away and try again shortly.
				state.dwNextFishingActionTime = dwNow + number(4000, 8000);
				return true;
			}
			state.bIsFishing = true;
			state.dwFishingCastTime = dwNow;
			return true;
		}

		if (info->step < 1)
		{
			// Still waiting for a bite. The engine takes 10-40 s; anything past a
			// minute means the event is wedged.
			if (dwNow - state.dwFishingCastTime > PLAYERBOT_FISHING_CAST_TIMEOUT)
			{
				ch->fishing_take();
				state.bIsFishing = false;
				state.dwNextFishingActionTime = dwNow + number(2000, 4000);
				sys_log(0, "PLAYERBOT_FISHING: cast timed out pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			return true;
		}

		// A fish is on. fishing::Compute() peaks around 3 s after the bite, so wait
		// out that band before pulling instead of yanking the rod instantly.
		const DWORD hooked = get_dword_time() - info->hang_time;
		const DWORD pullAt = PLAYERBOT_FISHING_PULL_MIN_DELAY +
				PlayerBotNavHash(ch->GetPlayerID() ^ info->hang_time) %
				(PLAYERBOT_FISHING_PULL_MAX_DELAY - PLAYERBOT_FISHING_PULL_MIN_DELAY + 1U);
		if (hooked < pullAt)
			return true;

		ch->fishing_take();
		state.bIsFishing = false;
		state.dwNextFishingActionTime = dwNow + number(2000, 4000);
		state.dwLastMeaningfulActivityTime = dwNow;
		ProcessPlayerBotCatch(ch);
		sys_log(0, "PLAYERBOT_FISHING: pulled pid=%u name=%s hooked_ms=%u fish=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)hooked, info->fish_id);
		return true;
	}

	bool AllocatePlayerBotStat(LPCHARACTER ch, BYTE statType)
	{
		if (!ch || ch->GetRealPoint(statType) >= 90 || ch->GetPoint(POINT_STAT) <= 0)
			return false;

		ch->SetRealPoint(statType, ch->GetRealPoint(statType) + 1);
		ch->SetPoint(statType, ch->GetPoint(statType) + 1);
		ch->ComputePoints();
		ch->PointChange(statType, 0);

		if (statType == POINT_IQ)
			ch->PointChange(POINT_MAX_HP, 0);
		else if (statType == POINT_HT)
			ch->PointChange(POINT_MAX_SP, 0);

		ch->PointChange(POINT_STAT, -1);
		ch->ComputePoints();
		return true;
	}

	void ManagePlayerBotStats(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetPoint(POINT_STAT) <= 0 || dwNow < state.dwNextStatCheckTime)
			return;

		state.dwNextStatCheckTime = dwNow + PLAYERBOT_STAT_CHECK_INTERVAL;

		while (ch->GetPoint(POINT_STAT) > 0)
		{
			const int ht = ch->GetRealPoint(POINT_HT);
			const int st = ch->GetRealPoint(POINT_ST);
			const int dx = ch->GetRealPoint(POINT_DX);
			const int iq = ch->GetRealPoint(POINT_IQ);

			BYTE targetStat = 0;

			switch (ch->GetJob())
			{
				case JOB_WARRIOR:
					// STR : VIT = 2 : 1 until 90, then DEX
					if (st < 90 && (st < ht * 2 || ht >= 90))
						targetStat = POINT_ST;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (iq < 90)
						targetStat = POINT_IQ;
					break;

				case JOB_ASSASSIN:
					// DEX : VIT = 2 : 1 until 90, then STR
					if (dx < 90 && (dx < ht * 2 || ht >= 90))
						targetStat = POINT_DX;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (st < 90)
						targetStat = POINT_ST;
					else if (iq < 90)
						targetStat = POINT_IQ;
					break;

				case JOB_SURA:
					// INT : VIT = 2 : 1 until 90, then DEX for skill dmg/reduction (or STR)
					if (iq < 90 && (iq < ht * 2 || ht >= 90))
						targetStat = POINT_IQ;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (st < 90)
						targetStat = POINT_ST;
					break;

				case JOB_SHAMAN:
					// INT : VIT = 2 : 1 until 90, then DEX
					if (iq < 90 && (iq < ht * 2 || ht >= 90))
						targetStat = POINT_IQ;
					else if (ht < 90)
						targetStat = POINT_HT;
					else if (dx < 90)
						targetStat = POINT_DX;
					else if (st < 90)
						targetStat = POINT_ST;
					break;

				default:
					if (ht < 90)
						targetStat = POINT_HT;
					else if (st < 90)
						targetStat = POINT_ST;
					break;
			}

			if (targetStat == 0 || !AllocatePlayerBotStat(ch, targetStat))
				break;

			sys_log(0, "PLAYERBOT_AI: allocated stat pid=%u name=%s stat=%u new_val=%d points_left=%d",
					ch->GetPlayerID(), ch->GetName(), targetStat, ch->GetRealPoint(targetStat), ch->GetPoint(POINT_STAT));
		}
	}

	struct TJobSkillBuild
	{
		BYTE bSkillCount;
		DWORD dwSkills[6];
		DWORD dwBuffSkills[3];
		DWORD dwOffensiveSkills[4];
		DWORD dwPrimaryMaxSkill;
	};

	TJobSkillBuild GetPlayerBotSkillBuild(BYTE bJob, BYTE bGroup, DWORD playerID = 0)
	{
		TJobSkillBuild build;
		memset(&build, 0, sizeof(build));

		if (bGroup < 1 || bGroup > 2)
			return build;

		if (bJob == JOB_WARRIOR)
		{
			if (bGroup == 1) // Body
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 1; build.dwSkills[1] = 2; build.dwSkills[2] = 3; build.dwSkills[3] = 4; build.dwSkills[4] = 5;
				build.dwBuffSkills[0] = 4; // Aura of Sword
				build.dwBuffSkills[1] = 3; // Berserk
				build.dwOffensiveSkills[0] = 2; // Sword Spin
				build.dwOffensiveSkills[1] = 1; // Three-Way Cut
				build.dwOffensiveSkills[2] = 5; // Dash
				build.dwPrimaryMaxSkill = 4; // Max Aura first
			}
			else // Mental
			{
				build.bSkillCount = 5;
				// Mental warriors do not all follow one copied guide.  One cohort
				// develops Strong Body -> Bash, another prefers Spirit Strike, and a
				// third brings Stump forward for pack control.
				if (playerID % 3 == 0)
				{
					build.dwSkills[0] = 19; build.dwSkills[1] = 17; build.dwSkills[2] = 18; build.dwSkills[3] = 16; build.dwSkills[4] = 20;
				}
				else if (playerID % 3 == 1)
				{
					build.dwSkills[0] = 16; build.dwSkills[1] = 19; build.dwSkills[2] = 17; build.dwSkills[3] = 18; build.dwSkills[4] = 20;
				}
				else
				{
					build.dwSkills[0] = 19; build.dwSkills[1] = 18; build.dwSkills[2] = 17; build.dwSkills[3] = 16; build.dwSkills[4] = 20;
				}
				build.dwBuffSkills[0] = 19; // Strong Body
				build.dwOffensiveSkills[0] = playerID % 3 == 0 ? 17 : 16;
				build.dwOffensiveSkills[1] = playerID % 3 == 0 ? 16 : 17;
				build.dwOffensiveSkills[2] = 18; // Stump
				build.dwOffensiveSkills[3] = 20; // Sword Strike
				build.dwPrimaryMaxSkill = 19; // Max Strong Body first
			}
		}
		else if (bJob == JOB_ASSASSIN)
		{
			if (bGroup == 1) // Dagger
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 31; build.dwSkills[1] = 32; build.dwSkills[2] = 33; build.dwSkills[3] = 34; build.dwSkills[4] = 35;
				// Stealth is valuable in PvP, but wastes an action and contributes no
				// meaningful PvE damage.  Keep it learned, never auto-cast it on mobs.
				build.dwBuffSkills[0] = 0;
				build.dwOffensiveSkills[0] = 31; // Ambush
				build.dwOffensiveSkills[1] = 33; // Rolling Dagger
				build.dwOffensiveSkills[2] = 32; // Fast Attack
				build.dwOffensiveSkills[3] = 35; // Poison Cloud
				build.dwPrimaryMaxSkill = 31; // Ambush
			}
			else // Archer
			{
				build.bSkillCount = 5;
				build.dwSkills[0] = 46; build.dwSkills[1] = 47; build.dwSkills[2] = 48; build.dwSkills[3] = 49; build.dwSkills[4] = 50;
				build.dwBuffSkills[0] = 49; // Feather Walk
				build.dwOffensiveSkills[0] = 48; // Fire Arrow
				build.dwOffensiveSkills[1] = 50; // Poison Arrow
				build.dwOffensiveSkills[2] = 46; // Repetitive Shot
				build.dwOffensiveSkills[3] = 47; // Arrow Shower
				build.dwPrimaryMaxSkill = 48;
			}
		}
		else if (bJob == JOB_SURA)
		{
			if (bGroup == 1) // Weaponary (WP)
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 61; build.dwSkills[1] = 62; build.dwSkills[2] = 63; build.dwSkills[3] = 64; build.dwSkills[4] = 65; build.dwSkills[5] = 66;
				build.dwBuffSkills[0] = 63; // Enchanted Blade
				build.dwBuffSkills[1] = 64; // Enchanted Armor
				build.dwBuffSkills[2] = 66; // Fear
				build.dwOffensiveSkills[0] = 62; // Dragon Swirl
				build.dwOffensiveSkills[1] = 61; // Finger Strike
				build.dwPrimaryMaxSkill = 63; // Enchanted Blade
			}
			else // Black Magic (BM)
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 76; build.dwSkills[1] = 77; build.dwSkills[2] = 78; build.dwSkills[3] = 79; build.dwSkills[4] = 80; build.dwSkills[5] = 81;
				build.dwBuffSkills[0] = 78; // Flame Spirit (Ognisty Duch / SKILL_MUYEONG)
				build.dwBuffSkills[1] = 79; // Dark Protection
				build.dwOffensiveSkills[0] = 77; // Flame Strike / Explosion
				build.dwOffensiveSkills[1] = 76; // Dark Strike
				build.dwOffensiveSkills[2] = 80; // Spirit Strike
				build.dwOffensiveSkills[3] = 81; // Dark Orb
				build.dwPrimaryMaxSkill = 78; // Keep Flame Spirit active and develop it first
			}
		}
		else if (bJob == JOB_SHAMAN)
		{
			if (bGroup == 1) // Dragon
			{
				build.bSkillCount = 6;
				const bool roarFirst = (playerID % 2) != 0;
				if (roarFirst)
				{
					build.dwSkills[0] = 93; build.dwSkills[1] = 96; build.dwSkills[2] = 92; build.dwSkills[3] = 91; build.dwSkills[4] = 94; build.dwSkills[5] = 95;
				}
				else
				{
					build.dwSkills[0] = 96; build.dwSkills[1] = 93; build.dwSkills[2] = 92; build.dwSkills[3] = 91; build.dwSkills[4] = 94; build.dwSkills[5] = 95;
				}
				build.dwBuffSkills[0] = 96; // Dragon's Aid (Crit)
				build.dwBuffSkills[1] = 94; // Blessing
				build.dwBuffSkills[2] = 95; // Reflect
				build.dwOffensiveSkills[0] = 92; // Shooting Dragon
				build.dwOffensiveSkills[1] = 93; // Dragon Roar
				build.dwOffensiveSkills[2] = 91; // Flying Talisman
				build.dwPrimaryMaxSkill = roarFirst ? 93 : 96;
			}
			else // Healer
			{
				build.bSkillCount = 6;
				build.dwSkills[0] = 106; build.dwSkills[1] = 107; build.dwSkills[2] = 108; build.dwSkills[3] = 109; build.dwSkills[4] = 110; build.dwSkills[5] = 111;
				build.dwBuffSkills[0] = 110; // Swiftness
				build.dwBuffSkills[1] = 111; // Attack Up
				build.dwBuffSkills[2] = 109; // Cure
				build.dwOffensiveSkills[0] = 106; // Lightning Throw
				build.dwOffensiveSkills[1] = 108; // Summon Lightning
				build.dwOffensiveSkills[2] = 107; // Lightning Claw
				build.dwPrimaryMaxSkill = 110; // Swiftness
			}
		}

		return build;
	}

	bool IsPlayerBotBuffActive(LPCHARACTER ch, DWORD buffVnum, DWORD dwNow, const TPlayerBotAIState& state)
	{
		if (!ch) return true;

		// 1. Affect flags (standard Metin2 toggle and buff flags)
		switch (buffVnum)
		{
			case 3:   if (ch->IsAffectFlag(AFF_GEOMGYEONG)) return true; break; // Aura of Sword
			case 4:   if (ch->IsAffectFlag(AFF_JEONGWIHON)) return true; break; // Berserk
			case 19:  if (ch->IsAffectFlag(AFF_CHEONGEUN))  return true; break; // Strong Body
			case 34:  if (ch->IsAffectFlag(AFF_EUNHYUNG))   return true; break; // Stealth
			case 49:  if (ch->IsAffectFlag(AFF_GYEONGGONG)) return true; break; // Feather Walk
			case 63:  if (ch->IsAffectFlag(AFF_GWIGUM))     return true; break; // Enchanted Blade toggle
			case 64:  if (ch->IsAffectFlag(AFF_JUMAGAP))    return true; break; // Enchanted Armor
			case 65:  if (ch->IsAffectFlag(AFF_TERROR))     return true; break; // Fear
			case 78:  if (ch->IsAffectFlag(AFF_MUYEONG))    return true; break; // Flame Spirit toggle
			case 79:  if (ch->IsAffectFlag(AFF_MANASHIELD)) return true; break; // Dark Protection toggle
			case 94:  if (ch->IsAffectFlag(AFF_BOHO))       return true; break; // Blessing
			case 95:  if (ch->IsAffectFlag(AFF_HOSIN))      return true; break; // Reflect
			case 96:  if (ch->IsAffectFlag(AFF_GICHEON))    return true; break; // Dragon Aid
			case 110: if (ch->IsAffectFlag(AFF_KWAESOK))    return true; break; // Swiftness
			case 111: if (ch->IsAffectFlag(AFF_JEUNGRYEOK)) return true; break; // Attack Up
		}

		// 2. Generic FindAffect check
		if (ch->FindAffect(buffVnum) != NULL)
			return true;

		// 3. Fallback timestamp map
		std::map<DWORD, DWORD>::const_iterator it = state.mapBuffActiveUntil.find(buffVnum);
		if (it != state.mapBuffActiveUntil.end() && dwNow < it->second)
			return true;

		return false;
	}

	bool ChoosePlayerBotSkillGroup(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 5)
			return false;
		if (ch->GetSkillGroup() != 0)
			return true;

		const BYTE bGroup = (ch->GetPlayerID() % 2 == 0) ? 1 : 2;
		ch->SetSkillGroup(bGroup);
		ch->ClearSkill();
		sys_log(0, "PLAYERBOT_AI: chosen skill group at trainer pid=%u name=%s job=%u group=%u points=%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetJob(), bGroup, ch->GetPoint(POINT_SKILL));
		return ch->GetSkillGroup() == bGroup;
	}

	void ManagePlayerBotSkills(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetLevel() < 5 || dwNow < state.dwNextSkillCheckTime)
			return;

		state.dwNextSkillCheckTime = dwNow + PLAYERBOT_SKILL_CHECK_INTERVAL;

		// Profession is chosen only after the bot physically visits the matching
		// Chunjo trainer.  Until then the long-term planner owns this goal.
		if (ch->GetSkillGroup() == 0)
			return;

		const BYTE bGroup = ch->GetSkillGroup();
		if (bGroup == 0)
			return;

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), bGroup, ch->GetPlayerID());
		if (build.bSkillCount == 0)
			return;

		// Purge any skill levels that do not belong to this bot's chosen profession
		for (DWORD s = 1; s <= 120; ++s)
		{
			if (ch->GetSkillLevel(s) > 0)
			{
				bool bValid = false;
				for (BYTE k = 0; k < build.bSkillCount; ++k)
				{
					if (build.dwSkills[k] == s)
					{
						bValid = true;
						break;
					}
				}
				if (!bValid)
				{
					ch->SetSkillLevel(s, 0);
				}
			}
		}

		if (ch->GetPoint(POINT_SKILL) <= 0)
			return;

		// First pass: ensure each group skill has at least 1 point to unlock it
		for (BYTE i = 0; i < build.bSkillCount && ch->GetPoint(POINT_SKILL) > 0; ++i)
		{
			const DWORD dwSkillVnum = build.dwSkills[i];
			if (dwSkillVnum != 0 && ch->GetSkillLevel(dwSkillVnum) == 0)
			{
				ch->SkillLevelUp(dwSkillVnum);
				sys_log(0, "PLAYERBOT_AI: unlocked skill pid=%u name=%s vnum=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), dwSkillVnum, ch->GetPoint(POINT_SKILL));
			}
		}

		// Second pass: level primary max skill up to Master (17-20)
		if (build.dwPrimaryMaxSkill != 0 && ch->GetPoint(POINT_SKILL) > 0)
		{
			while (ch->GetPoint(POINT_SKILL) > 0 &&
					ch->GetSkillMasterType(build.dwPrimaryMaxSkill) == SKILL_NORMAL &&
					ch->GetSkillLevel(build.dwPrimaryMaxSkill) < 20)
			{
				const BYTE bOldLevel = ch->GetSkillLevel(build.dwPrimaryMaxSkill);
				ch->SkillLevelUp(build.dwPrimaryMaxSkill);
				if (ch->GetSkillLevel(build.dwPrimaryMaxSkill) == bOldLevel)
					break;

				sys_log(0, "PLAYERBOT_AI: leveled primary skill pid=%u name=%s vnum=%u level=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), build.dwPrimaryMaxSkill, ch->GetSkillLevel(build.dwPrimaryMaxSkill), ch->GetPoint(POINT_SKILL));
			}
		}

		// Third pass: distribute remaining points into secondary skills
		for (BYTE i = 0; i < build.bSkillCount && ch->GetPoint(POINT_SKILL) > 0; ++i)
		{
			const DWORD dwSkillVnum = build.dwSkills[i];
			if (dwSkillVnum == 0 || dwSkillVnum == build.dwPrimaryMaxSkill)
				continue;

			while (ch->GetPoint(POINT_SKILL) > 0 &&
					ch->GetSkillMasterType(dwSkillVnum) == SKILL_NORMAL &&
					ch->GetSkillLevel(dwSkillVnum) < 20)
			{
				const BYTE bOldLevel = ch->GetSkillLevel(dwSkillVnum);
				ch->SkillLevelUp(dwSkillVnum);
				if (ch->GetSkillLevel(dwSkillVnum) == bOldLevel)
					break;

				sys_log(0, "PLAYERBOT_AI: leveled secondary skill pid=%u name=%s vnum=%u level=%u points_left=%d",
						ch->GetPlayerID(), ch->GetName(), dwSkillVnum, ch->GetSkillLevel(dwSkillVnum), ch->GetPoint(POINT_SKILL));
			}
		}
	}

	void SendPlayerBotFlyTargetPacket(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !ch->GetSectree() ||
				ch->GetMapIndex() != target->GetMapIndex())
			return;

		// Only reproduce the visual packet emitted by a real client.  Calling
		// CHARACTER::FlyTarget here would also mutate m_dwFlyTargetID and force us
		// through Shoot(), which applies damage a second time and was the source of
		// the latest archer regression.
		TPacketGCFlyTargeting pack;
		pack.bHeader = HEADER_GC_FLY_TARGETING;
		pack.dwShooterVID = ch->GetVID();
		pack.dwTargetVID = target->GetVID();
		pack.x = target->GetX();
		pack.y = target->GetY();

		ch->PacketAround(&pack, sizeof(TPacketGCFlyTargeting), ch);
	}

	void SendPlayerBotAttackPacket(LPCHARACTER ch, LPCHARACTER target, BYTE comboMotion)
	{
		if (!ch || !ch->GetSectree())
			return;

		ch->OnMove(true);
		ch->ResetStopTime();

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW;
		if (isBow)
		{
			// Bow mode registers only COMBO_ATTACK_1 (bow/attack.msa).  Cycling
			// through 2..4 selects missing motions and leaves the archer frozen.
			comboMotion = MOTION_COMBO_ATTACK_1;
			SendPlayerBotFlyTargetPacket(ch, target);
		}
		else if (comboMotion < MOTION_COMBO_ATTACK_1 || comboMotion > MOTION_COMBO_ATTACK_4)
			comboMotion = MOTION_COMBO_ATTACK_1;

		TPacketGCMove pack;
		pack.bHeader = HEADER_GC_MOVE;
		// This exact COMBO_ATTACK_1 path is the build visually verified by the
		// user for both bow and dagger.  Bow differs only in being pinned to its
		// sole registered combo key instead of cycling through 1..4.
		pack.bFunc = FUNC_COMBO;
		pack.bArg = comboMotion;
		pack.bRot = (BYTE)(ch->GetRotation() / 5);
		pack.dwVID = ch->GetVID();
		pack.lX = ch->GetX();
		pack.lY = ch->GetY();
		pack.dwTime = get_dword_time();
		pack.dwDuration = 0;

		ch->PacketAround(&pack, sizeof(TPacketGCMove));
	}

	BYTE GetPlayerBotSkillMotionIndex(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch)
			return (BYTE)(skillVnum & 0x7F);

		// skilldesc registers the six skills of each profession under motion
		// slots 1..6 (group 1) or 16..21 (group 2), independently of the
		// server-side skill VNUM.  Every mastery grade is one SKILL_GRADEGAP
		// (25 motions) further.  Sending the raw VNUM happened to work for a
		// few Warrior motions, but points Ninja/Sura/Shaman at empty keys.
		if (skillVnum >= 1 && skillVnum <= 111)
		{
			const BYTE baseMotion = (BYTE)(((skillVnum - 1) % 30) + 1);
			int mastery = ch->GetSkillMasterType(skillVnum);
			if (mastery < SKILL_NORMAL)
				mastery = SKILL_NORMAL;
			else if (mastery > SKILL_PERFECT_MASTER)
				mastery = SKILL_PERFECT_MASTER;
			return (BYTE)(baseMotion + mastery * 25);
		}

		return (BYTE)(skillVnum & 0x7F);
	}

	void SendPlayerBotSkillPacket(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || !ch->GetSectree())
			return;

		ch->OnMove();
		ch->ResetStopTime();

		const BYTE motionIndex = GetPlayerBotSkillMotionIndex(ch, skillVnum);
		TPacketGCMove pack;
		pack.bHeader = HEADER_GC_MOVE;
		pack.bFunc = FUNC_SKILL | motionIndex;
		// bArg is the animation loop count, not the N/M/G/P grade.  Zero is the
		// native/default single-play value used by the previously working build.
		pack.bArg = 0;
		pack.bRot = (BYTE)(ch->GetRotation() / 5);
		pack.dwVID = ch->GetVID();
		pack.lX = ch->GetX();
		pack.lY = ch->GetY();
		pack.dwTime = get_dword_time();
		pack.dwDuration = 0;

		ch->PacketAround(&pack, sizeof(TPacketGCMove));
		sys_log(1, "PLAYERBOT_AI: skill motion pid=%u skill=%u motion=%u mastery=%d",
				ch->GetPlayerID(), skillVnum, motionIndex,
				ch->GetSkillMasterType(skillVnum));
	}

	bool ManagePlayerBotCombatBuffs(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetSkillGroup() == 0 || dwNow < state.dwNextBuffCheckTime)
			return false;

		// Do not cast combat buffs if visiting shop or without an active combat target / victim
		if (state.bVisitingShop || (state.dwTargetVID == 0 && ch->GetVictim() == NULL))
			return false;

		if (ch->GetMapIndex() == 21)
		{
			const long townX = 60600;
			const long townY = 170900;
			if (DISTANCE_APPROX(ch->GetX() - townX, ch->GetY() - townY) <= 3000)
				return false; // Inside city center / near town merchants
		}

		state.dwNextBuffCheckTime = dwNow + 5000;

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD buffVnum = build.dwBuffSkills[i];
			if (buffVnum == 0 || ch->GetSkillLevel(buffVnum) == 0)
				continue;

			// Check if buff is currently active (including toggle skills like Enchanted Blade / Flame Spirit)
			if (IsPlayerBotBuffActive(ch, buffVnum, dwNow, state))
				continue;

			if (buffVnum == 109) // Cure / Heal
			{
				if (ch->GetMaxHP() <= 0 || (ch->GetHP() * 100) / ch->GetMaxHP() > 60)
					continue;
			}

			// Self-buff if not active
			if (ch->UseSkill(buffVnum, ch))
			{
				SendPlayerBotSkillPacket(ch, buffVnum);
				state.dwLastBotSkillTime = dwNow;
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				// FindAffect/AFF_* above is authoritative.  Keep a conservative
				// fallback as well, because some client/server skill tables do not
				// expose every buff through the same affect flag.  Cure is instant
				// and therefore only needs its normal skill cooldown.
				state.mapBuffActiveUntil[buffVnum] = dwNow +
						(buffVnum == 109 ? 10000 : PLAYERBOT_BUFF_FALLBACK_DURATION);
				sys_log(0, "PLAYERBOT_AI: activated self buff skill pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), buffVnum);
				return true;
			}

			// Party buffs for Shaman (Blessing, Dragon Aid, Swiftness, Attack Up, Heal)
			if (ch->GetJob() == JOB_SHAMAN && ch->GetParty())
			{
				struct FPartyBuffShaman
				{
					LPCHARACTER m_shaman;
					DWORD m_buffVnum;
					DWORD m_dwNow;
					TPlayerBotAIState& m_state;
					bool m_bApplied;

					FPartyBuffShaman(LPCHARACTER shaman, DWORD buffVnum, DWORD dwNow, TPlayerBotAIState& state) :
						m_shaman(shaman), m_buffVnum(buffVnum), m_dwNow(dwNow), m_state(state), m_bApplied(false)
					{
					}

					void operator()(LPCHARACTER member)
					{
						if (m_bApplied || !member || member == m_shaman || member->IsDead())
							return;

						if (DISTANCE_APPROX(m_shaman->GetX() - member->GetX(), m_shaman->GetY() - member->GetY()) > 2000)
							return;

						if (m_buffVnum == 109) // Cure/Heal
						{
							if (member->GetMaxHP() > 0 && (member->GetHP() * 100) / member->GetMaxHP() <= 60)
							{
								if (m_shaman->UseSkill(m_buffVnum, member))
								{
									SendPlayerBotSkillPacket(m_shaman, m_buffVnum);
									m_state.dwLastBotSkillTime = m_dwNow;
									m_state.dwNextAttackTime = m_dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
									m_bApplied = true;
									sys_log(0, "PLAYERBOT_AI: shaman healed party member pid=%u target_pid=%u",
											m_shaman->GetPlayerID(), member->GetPlayerID());
								}
							}
						}
						else if (member->FindAffect(m_buffVnum) == NULL)
						{
							if (m_shaman->UseSkill(m_buffVnum, member))
							{
								SendPlayerBotSkillPacket(m_shaman, m_buffVnum);
								m_state.dwLastBotSkillTime = m_dwNow;
								m_state.dwNextAttackTime = m_dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
								m_bApplied = true;
								sys_log(0, "PLAYERBOT_AI: shaman buffed party member pid=%u target_pid=%u vnum=%u",
										m_shaman->GetPlayerID(), member->GetPlayerID(), m_buffVnum);
							}
						}
					}
				};

				FPartyBuffShaman buffFunctor(ch, buffVnum, dwNow, state);
				ch->GetParty()->ForEachOnMapMember(buffFunctor, ch->GetMapIndex());
				if (buffFunctor.m_bApplied)
					return true;
			}
		}

		return false;
	}

	bool ExecutePlayerBotAttackSkill(LPCHARACTER ch, LPCHARACTER target, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || ch->GetSkillGroup() == 0 || dwNow < state.dwNextSkillCastTime)
			return false;
		LPITEM archerBow = NULL;
		LPITEM archerArrow = NULL;
		if (ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2)
		{
			if (!EnsurePlayerBotArrowsEquipped(ch) ||
					ch->GetArrowAndBow(&archerBow, &archerArrow, 1) != 1)
				return false;
		}

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwOffensiveSkills) / sizeof(build.dwOffensiveSkills[0]); ++i)
		{
			const DWORD skillVnum = build.dwOffensiveSkills[i];
			if (skillVnum == 0 || ch->GetSkillLevel(skillVnum) == 0)
				continue;

			if (ch->UseSkill(skillVnum, target))
			{
				if (ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2)
					SendPlayerBotFlyTargetPacket(ch, target);

				// Keep the single, proven server-side damage path.  Shoot() would
				// consume the pending target and run a second damage path.  The visual
				// packet follows the same order as the build verified in the client.
				ch->ComputeSkill(skillVnum, target);
				SendPlayerBotSkillPacket(ch, skillVnum);
				if (archerArrow)
					ch->UseArrow(archerArrow, 1);
				state.dwLastBotSkillTime = dwNow;
				state.dwLastCombatActionTime = dwNow;
				// Shamans should weave weapon attacks between spells.  Casting an
				// offensive spell every global AI tick looks like repeated buffing
				// in the client and leaves almost no visible normal attacks.
				state.dwNextSkillCastTime = dwNow +
						(ch->GetJob() == JOB_SHAMAN
						 ? PLAYERBOT_SHAMAN_ATTACK_SKILL_INTERVAL
						 : PLAYERBOT_SKILL_ATTACK_INTERVAL);
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				sys_log(0, "PLAYERBOT_AI: used attack skill pid=%u name=%s vnum=%u target_vid=%u",
						ch->GetPlayerID(), ch->GetName(), skillVnum, target->GetVID());
				return true;
			}
		}

		return false;
	}

	class FPlayerBotPartyCohesion
	{
		public:
			FPlayerBotPartyCohesion(LPCHARACTER leader, int maxDistance) :
				m_leader(leader), m_maxDistance(maxDistance), m_onlineBots(0),
				m_togetherBots(0)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || !member->GetDesc() || !member->GetDesc()->IsBot())
					return;
				++m_onlineBots;
				if (member->GetMapIndex() == m_leader->GetMapIndex() &&
						DISTANCE_APPROX(member->GetX() - m_leader->GetX(),
							member->GetY() - m_leader->GetY()) <= m_maxDistance)
					++m_togetherBots;
			}

			int OnlineBots() const { return m_onlineBots; }
			int TogetherBots() const { return m_togetherBots; }

		private:
			LPCHARACTER m_leader;
			int m_maxDistance;
			int m_onlineBots;
			int m_togetherBots;
	};

	bool IsPlayerBotPartyCohesive(LPCHARACTER ch, int minMembers, int maxDistance)
	{
		if (!ch || !ch->GetParty() || ch->GetParty()->GetMemberCount() < (DWORD)minMembers)
			return false;
		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		if (!leader || leader->GetMapIndex() != ch->GetMapIndex())
			return false;

		FPlayerBotPartyCohesion cohesion(leader, maxDistance);
		ch->GetParty()->ForEachOnlineMember(cohesion);
		return cohesion.OnlineBots() >= minMembers &&
				cohesion.TogetherBots() == cohesion.OnlineBots() &&
				cohesion.OnlineBots() == (int)ch->GetParty()->GetMemberCount();
	}

	bool ExecutePlayerBotArcherLuring(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2 || !ch->GetParty())
			return false;
		// Luring is a group role, not a solo Archer shortcut. Every party member
		// must be online, on this map and inside one local formation; otherwise an
		// Archer could pull for a nominal party scattered across different zones.
		if (!IsPlayerBotPartyCohesive(ch, PLAYERBOT_ARCHER_LURE_MIN_PARTY_MEMBERS,
				PLAYERBOT_PARTY_COHESION_RADIUS))
			return false;

		LPITEM weapon = NULL;
		LPITEM arrow = NULL;
		if (!EnsurePlayerBotArrowsEquipped(ch) ||
				ch->GetArrowAndBow(&weapon, &arrow, 1) != 1)
			return false; // Only lure when equipped with a Bow!

		if (dwNow < state.dwNextLureTime)
			return false;

		state.dwNextLureTime = dwNow + number(3500, 5500);

		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		if (!leader || leader->GetMapIndex() != ch->GetMapIndex())
			return false;

		// Scan for distant mob around party to pull
		if (!ch->GetSectree())
			return false;

		struct TLureCollector
		{
			TLureCollector(LPCHARACTER me) : m_me(me), m_targetVID(0), m_bestDist(99999) {}
			bool operator()(LPENTITY ent)
			{
				if (!ent || !ent->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER mob = static_cast<LPCHARACTER>(ent);
				if (!mob->IsMonster() || mob->IsDead() || mob->GetVictim() != NULL ||
						IsPlayerBotSafeZone(mob->GetMapIndex(), mob->GetX(), mob->GetY()) ||
						mob->GetLevel() > m_me->GetLevel() + 10)
					return false;
				int dist = DISTANCE_APPROX(m_me->GetX() - mob->GetX(), m_me->GetY() - mob->GetY());
				if (dist >= 1200 && dist <= 3000 && dist < m_bestDist &&
						IsPlayerBotReachable(m_me->GetMapIndex(), m_me->GetX(), m_me->GetY(),
							mob->GetX(), mob->GetY()))
				{
					m_bestDist = dist;
					m_targetVID = mob->GetVID();
				}
				return true;
			}
			LPCHARACTER m_me;
			DWORD m_targetVID;
			int m_bestDist;
		};

		TLureCollector collector(ch);
		ch->GetSectree()->ForEachAround(collector);

		if (collector.m_targetVID != 0)
		{
			LPCHARACTER mob = CHARACTER_MANAGER::instance().Find(collector.m_targetVID);
			if (mob && !mob->IsDead())
			{
				ch->SetRotationToXY(mob->GetX(), mob->GetY());
				// Use a normal bow shot for the pull. Fire Arrow is part of the normal
				// offensive rotation and was almost always on its real skill cooldown,
				// which made the old lure silently fail even in a valid six-person PT.
				int damage = CalcArrowDamage(ch, mob, weapon, arrow, false);
				if (damage < 5)
					damage = number(10, 20) + ch->GetLevel() * 2;
				SendPlayerBotAttackPacket(ch, mob, MOTION_COMBO_ATTACK_1);
				mob->Damage(ch, damage, DAMAGE_TYPE_NORMAL);
				ch->UseArrow(arrow, 1);
				mob->SetSyncOwner(ch);
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				state.dwLastCombatActionTime = dwNow;
				sys_log(0, "PLAYERBOT_AI: archer lured distant mob pid=%u name=%s target_vid=%u target=%s damage=%d",
						ch->GetPlayerID(), ch->GetName(), mob->GetVID(), mob->GetName(), damage);
				return true;
			}
		}

		return false;
	}

	PIXEL_POSITION GetPlayerBotGeneralStorePos(long mapIndex)
	{
		PIXEL_POSITION pos;
		pos.x = 0;
		pos.y = 0;
		pos.z = 0;

		if (mapIndex == 21 || mapIndex == 23) // Chunjo M1 / M3
		{
			pos.x = 59000;
			pos.y = 68900;
		}
		else if (mapIndex == 1 || mapIndex == 3) // Shinsoo M1 / M3
		{
			pos.x = 67800;
			pos.y = 56500;
		}
		else if (mapIndex == 41 || mapIndex == 43) // Jinno M1 / M3
		{
			pos.x = 38300;
			pos.y = 69300;
		}

		return pos;
	}

	DWORD GetPlayerBotSkillBookSkillVnum(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_SKILLBOOK)
			return 0;
		return item->GetVnum() == 50300 ? (DWORD)item->GetSocket(0) : (DWORD)item->GetValue(0);
	}

	bool IsPlayerBotOwnSkill(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || skillVnum == 0 || ch->GetSkillGroup() == 0)
			return false;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (BYTE i = 0; i < build.bSkillCount; ++i)
			if (build.dwSkills[i] == skillVnum)
				return true;
		return false;
	}

	bool PlayerBotNeedsRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch || materialVnum == 0)
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}

		for (size_t i = 0; i < gear.size(); ++i)
		{
			LPITEM item = gear[i];
			if (!item || item->GetRefinedVnum() == 0 ||
					item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
			{
				if (recipe->materials[m].vnum == materialVnum &&
						ch->CountSpecifyItem(materialVnum) < recipe->materials[m].count * 2)
					return true;
			}
		}
		return false;
	}

	bool IsPlayerBotJunkItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->isLocked())
			return false;

		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return false;

		const DWORD vnum = item->GetVnum();

		// Level-30 weapons with average/skill damage are strategic market assets.
		// Never vendor them: this also applies when the current owner is below level
		// 30 or belongs to another class. They remain available for future playerbot
		// trading/private shops instead of disappearing for a trivial NPC price.
		if (IsPlayerBotSpecialLevel30Weapon(item))
			return false;

		// Quest progress must survive every merchant visit. In particular, Horse
		// Medals used to look like ordinary miscellaneous loot and could be sold
		// before the world-travel state machine returned the bot to the Stable Boy.
		if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM || (vnum >= 50701 && vnum <= 50706))
			return false;

		// Fishing tackle and the catch worth keeping. Pearls are the entire point
		// of a fishing trip -- they are what carries equipment to +7/+8/+9 -- and a
		// vendored rod would simply have to be bought again for the next session.
		// Ordinary fish and bones stay sellable: that is the angler's pocket money.
		if (item->GetType() == ITEM_ROD || vnum == PLAYERBOT_FISHING_BAIT_VNUM ||
				vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;

		// Arrows are ammunition, not a primary weapon/equipment candidate. Keep all
		// spare stacks for an Archer (including a Ninja which is about to choose the
		// deterministic Bow profession), while other classes may sell accidental
		// arrow drops at the Weapon Merchant.
		if (item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW)
		{
			const bool isOrWillBeArcher = ch->GetJob() == JOB_ASSASSIN &&
					(ch->GetSkillGroup() == 2 ||
					 (ch->GetSkillGroup() == 0 && (ch->GetPlayerID() % 2) != 0));
			return !isOrWillBeArcher;
		}

		if (item->GetType() == ITEM_SKILLBOOK)
		{
			// Keep books for the selected build (also before profession selection).
			// Books for another class/build may first be handed to a party member;
			// if nobody needs them they become normal miscellaneous loot.
			return ch->GetSkillGroup() != 0 &&
					!IsPlayerBotOwnSkill(ch, GetPlayerBotSkillBookSkillVnum(item));
		}

		// Preserve health, mana, green and purple speed potions
		if (vnum == 27051 || vnum == 27001 || vnum == 27002 || vnum == 27003 ||
			vnum == 27052 || vnum == 27004 || vnum == 27005 || vnum == 27006 ||
			(vnum >= 27100 && vnum <= 27105) || vnum == 27053 || vnum == 27054)
			return false;

		// Preserve every Apprentice Chest until the bot can open it. Class-specific
		// first chests use 50212/50213, while later progression boxes use 50187-50196.
		if (vnum == GetStarterChestVnum(ch->GetJob()) ||
				(vnum >= 50187 && vnum <= 50196))
			return false;

		// Preserve only materials on this bot's current two-attempt refine wishlist.
		// Unneeded materials no longer fill the inventory forever.
		if (item->GetType() == ITEM_MATERIAL)
			return !PlayerBotNeedsRefineMaterial(ch, vnum);
		if (vnum >= 30000 && vnum <= 30200)
			return !PlayerBotNeedsRefineMaterial(ch, vnum);
		if (vnum >= 70038 && vnum <= 70060)
			return false;

		// Keep at most one immediately usable upgrade for each wear slot.  The old
		// test kept every item that scored above the currently worn one; at high
		// drop rates that meant dozens of near-identical weapons and armours could
		// never become junk even though only the best one would ever be equipped.
		if (IsPlayerBotEquipmentCandidate(ch, item))
		{
			const int wearCell = item->FindEquipCell(ch);
			if (wearCell >= 0 && wearCell < WEAR_MAX_NUM)
			{
				if (item->GetLevelLimit() > ch->GetLevel())
					return true;

				LPITEM oldItem = ch->GetWear(wearCell);
				const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
				const long long oldScore = oldItem ? GetPlayerBotEquipmentScore(oldItem, ch) : 0;
				if (!oldItem || itemScore > oldScore)
				{
					for (WORD otherCell = 0; otherCell < INVENTORY_MAX_NUM; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}

				// A well-refined item replaced by genuinely stronger progression gear is
				// still valuable to another bot.  Keep only the single best +6-or-higher
				// reserve for this wear slot; the nearby sharing pass will hand the real
				// item (including sockets/attributes) to a lower-level compatible build.
				if (item->GetRefineLevel() >= PLAYERBOT_RESERVE_GEAR_MIN_REFINE)
				{
					for (WORD otherCell = 0; otherCell < INVENTORY_MAX_NUM; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item ||
								other->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
								!IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}
			}
		}

		return true;
	}

	EPlayerBotMerchantCategory GetPlayerBotJunkMerchant(LPITEM item)
	{
		if (!item)
			return BOT_MERCHANT_MISC;

		if (item->GetType() == ITEM_WEAPON)
			return BOT_MERCHANT_WEAPON;
		if (item->GetType() == ITEM_ARMOR || item->GetType() == ITEM_UNIQUE ||
				item->GetType() == ITEM_RING || item->GetType() == ITEM_BELT)
			return BOT_MERCHANT_ARMOR;
		return BOT_MERCHANT_MISC;
	}

	bool HasPlayerBotJunkForMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotJunkItem(ch, item) &&
					GetPlayerBotJunkMerchant(item) == category)
				return true;
		}
		return false;
	}

	size_t CountPlayerBotJunkItems(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return 0;
		size_t count = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			if (IsPlayerBotJunkItem(ch, ch->GetInventoryItem(cell)))
				++count;
		return count;
	}

	bool SellPlayerBotJunkAtMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category,
			const char* merchantName)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		size_t soldCount = 0;
		long long totalSoldGold = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotJunkItem(ch, item) ||
					GetPlayerBotJunkMerchant(item) != category)
				continue;

			DWORD price = item->GetShopBuyPrice();
			if (price == 0)
				price = item->GetProto() ? item->GetProto()->dwGold : 100;
			price = std::max<DWORD>(10, price / 5);
			totalSoldGold += price;
			ch->PointChange(POINT_GOLD, price);
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_SHOP_SELL");
			++soldCount;
		}

		if (soldCount > 0)
		{
			sys_log(0, "PLAYERBOT_AI: sold %u items at %s pid=%u name=%s gold_gained=%lld total_gold=%lld",
					(unsigned int)soldCount, merchantName ? merchantName : "merchant",
					ch->GetPlayerID(), ch->GetName(), totalSoldGold, (long long)ch->GetGold());
		}
		return soldCount > 0;
	}

	bool HasPlayerBotBackupGear(LPCHARACTER ch, BYTE wearCell)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->FindEquipCell(ch) == wearCell)
				return true;
		}

		return false;
	}

	// --- Bonus lines ---------------------------------------------------------
	// Gear is only half a bot's power; the four bonus lines are the other half. A
	// level-appropriate weapon rolled into four resistances is genuinely worse
	// than the one it replaced, and until now nothing ever looked at them.
	//
	// The scoring below is deliberately coarse. It exists to tell "worth keeping"
	// from "roll it again", not to model the damage formula: every line is scored
	// as points-per-typical-roll so that a +2000 HP line and a +15 attack line
	// can be compared at all.
	bool IsPlayerBotCaster(LPCHARACTER ch)
	{
		return ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
	}

	int ScorePlayerBotBonusLine(LPCHARACTER ch, bool bOffensiveSlot, BYTE type, short value)
	{
		// A negative roll exists (movement speed on some sets) and is worth less
		// than nothing, so it must not be able to prop up a bad item's total.
		if (value <= 0)
			return 0;

		switch (type)
		{
			case APPLY_SKILL_DAMAGE_BONUS:      return value * 12;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS: return value * 10;
			case APPLY_CRITICAL_PCT:            return value * 10;
			case APPLY_PENETRATE_PCT:           return value * 10;
			case APPLY_ATTBONUS_MONSTER:        return value * 8;
			case APPLY_ATT_SPEED:               return value * 8;
			case APPLY_STEAL_HP:                return value * 6;
			case APPLY_ATT_GRADE_BONUS:         return bOffensiveSlot ? value * 5 : value * 3;
			case APPLY_CAST_SPEED:              return IsPlayerBotCaster(ch) ? value * 8 : value;
			case APPLY_MAX_HP_PCT:              return value * 15;
			case APPLY_DEF_GRADE_BONUS:         return bOffensiveSlot ? value * 2 : value * 6;
			case APPLY_MOV_SPEED:               return value * 3;
			// Big absolute numbers that have to be scaled down to compare with the
			// percentage lines above.
			case APPLY_MAX_HP:                  return value / 4;
			case APPLY_MAX_SP:                  return IsPlayerBotCaster(ch) ? value / 4 : value / 12;
			// Everything else - resistances, stamina, experience bonus - is real but
			// minor for a bot that only grinds. Never zero: a line is still a line.
			default:                            return value;
		}
	}

	bool IsPlayerBotOffensiveSlot(BYTE wearCell)
	{
		return wearCell == WEAR_WEAPON;
	}

	int ScorePlayerBotItemBonuses(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!ch || !item)
			return 0;
		const bool bOffensive = IsPlayerBotOffensiveSlot(wearCell);
		int score = 0;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			score += ScorePlayerBotBonusLine(ch, bOffensive,
					item->GetAttributeType(i), item->GetAttributeValue(i));
		}
		return score;
	}

	// An item the engine will actually accept a stone on. UseItemEx refuses an
	// equipped item outright ("if (item2->IsEquipped()) return false"), costumes,
	// and anything without an attribute set, so a bot has to take the piece off
	// first - exactly as a player does.
	bool CanPlayerBotRerollItem(LPITEM item)
	{
		return item && item->GetType() != ITEM_COSTUME && !item->isLocked() &&
				!item->IsExchanging() && item->GetAttributeSetIndex() != -1;
	}

	// The stones cannot be dropped, sold, traded or shopped, so there is no market
	// to walk to: the bot pays for one the same way it pays for its stall.
	bool BuyPlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		if (ch->CountSpecifyItem(vnum) > 0)
			return true;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;
		if (ch->GetEmptyInventory(1) < 0)
			return false;
		if (!ch->AutoGiveItem(vnum, 1, -1, false))
			return false;
		ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_BONUS_STONE_PRICE);
		return true;
	}

	bool ConsumePlayerBotBonusStone(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetVnum() != vnum)
				continue;
			if (stone->GetCount() > 1)
				stone->SetCount(stone->GetCount() - 1);
			else
				ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_BONUS");
			return true;
		}
		return false;
	}

	// Worn gear only. Spares in the bag are sold or put in a stall long before
	// they are worth polishing, and rerolling them would spend the gold the bot
	// needs for its next real upgrade.
	bool ManagePlayerBotBonusReroll(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextBonusCheckTime)
			return false;
		state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_INTERVAL;
		if (ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL)
			return false;
		if (ch->GetGold() < (int)(PLAYERBOT_BONUS_GOLD_FLOOR + PLAYERBOT_BONUS_STONE_PRICE))
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		int stonesUsed = 0;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]) &&
				stonesUsed < PLAYERBOT_BONUS_STONES_PER_VISIT; ++i)
		{
			const BYTE wearCell = wearSlots[i];
			LPITEM item = ch->GetWear(wearCell);
			if (!CanPlayerBotRerollItem(item))
				continue;

			const int count = item->GetAttributeCount();
			const int score = ScorePlayerBotItemBonuses(ch, item, wearCell);

			// An empty line is free power: add before rerolling, always. Only once
			// the item is full does the quality of what it rolled start to matter,
			// and USE_CHANGE_ATTRIBUTE needs at least one line to work on anyway.
			const bool bWantAdd = count < 4;
			const bool bWantChange = !bWantAdd && score < PLAYERBOT_BONUS_KEEP_SCORE;
			if (!bWantAdd && !bWantChange)
				continue;

			const DWORD stoneVnum = bWantAdd ? PLAYERBOT_BONUS_ADD_VNUM
					: PLAYERBOT_BONUS_CHANGE_VNUM;
			if (!BuyPlayerBotBonusStone(ch, stoneVnum))
				continue;

			// The piece has to come off for the engine to touch it, and it has to go
			// back on afterwards - a bot walking around with its weapon in the bag
			// would be worse than any bonus line it could win.
			if (!ch->UnequipItem(item))
				continue;

			if (bWantAdd)
				item->AddAttribute();
			else
				item->ChangeAttribute();

			ConsumePlayerBotBonusStone(ch, stoneVnum);
			++stonesUsed;

			const int newScore = ScorePlayerBotItemBonuses(ch, item, wearCell);
			if (!ch->EquipItem(item))
			{
				sys_err("PLAYERBOT_BONUS: could not re-equip pid=%u name=%s vnum=%u slot=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
						(unsigned int)wearCell);
				continue;
			}

			sys_log(0, "PLAYERBOT_BONUS: %s pid=%u name=%s vnum=%u slot=%u lines=%d->%d score=%d->%d gold=%d",
					bWantAdd ? "added" : "rerolled", ch->GetPlayerID(), ch->GetName(),
					item->GetVnum(), (unsigned int)wearCell, count,
					item->GetAttributeCount(), score, newScore,
					(int)(ch->GetGold() / 1000));
		}

		return stonesUsed > 0;
	}

	bool ManagePlayerBotRefining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextRefineCheckTime)
			return false;

		state.dwNextRefineCheckTime = dwNow + PLAYERBOT_REFINE_INTERVAL;

		// Collect all upgradable worn items and inventory candidates
		struct TRefineCandidate
		{
			BYTE wearCell;
			LPITEM item;
			BYTE plusLevel;
			BYTE priority;
		};

		std::vector<TRefineCandidate> candidates;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0)
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = wearSlots[i];
			cand.item = item;
			cand.plusLevel = plusLevel;
			cand.priority = coreProgression ? 0 : 2;
			candidates.push_back(cand);
		}

		// Also collect candidate gear in inventory
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetRefinedVnum() == 0 || !IsPlayerBotEquipmentCandidate(ch, item))
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = 255;
			cand.item = item;
			cand.plusLevel = plusLevel;
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			cand.priority = coreProgression ? 0 : 2;
			candidates.push_back(cand);
		}

		if (candidates.empty())
			return false;

		// Core level-appropriate weapon/body gear comes first. Within the same
		// priority, raise the lowest plus level so both essentials progress evenly.
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			for (size_t j = i + 1; j < candidates.size(); ++j)
			{
				if (candidates[j].priority < candidates[i].priority ||
						(candidates[j].priority == candidates[i].priority &&
						 candidates[j].plusLevel < candidates[i].plusLevel))
				{
					TRefineCandidate tmp = candidates[i];
					candidates[i] = candidates[j];
					candidates[j] = tmp;
				}
			}
		}

		int refinedCount = 0;
		for (size_t i = 0; i < candidates.size() && refinedCount < 2; ++i)
		{
			LPITEM item = candidates[i].item;
			if (!item || item->GetRefinedVnum() == 0)
				continue;

			const DWORD oldVnum = item->GetVnum();
			const DWORD nextVnum = item->GetRefinedVnum();
			const BYTE plusLevel = candidates[i].plusLevel;
			const BYTE wearCell = candidates[i].wearCell;
			const bool hasBackup = (wearCell != 255) ? HasPlayerBotBackupGear(ch, wearCell) : true;

			if (wearCell == WEAR_WEAPON && !hasBackup && plusLevel >= 4 && ch->GetGold() < 5000)
				continue;

			if (plusLevel >= 5 && !hasBackup && ch->GetGold() < 15000)
				continue;

			if (plusLevel == 4 && !hasBackup && number(1, 100) > 75)
				continue;

			// Equipment management after an earlier attempt may have equipped another
			// queued candidate, so inspect its live position instead of trusting the
			// location captured when the list was built.
			if (item->IsEquipped())
			{
				int emptyCell = ch->GetEmptyInventory(item->GetSize());
				if (emptyCell < 0)
					continue;
				if (!ch->UnequipItem(item) || item->IsEquipped())
					continue;
			}

			// DoRefine(false) is the regular blacksmith path: it reads refine_proto,
			// charges the exact fee, consumes every required material and applies the
			// normal success/failure roll.  The return value only says that an attempt
			// was performed, so compare the result item count to log its real outcome.
			const int resultCountBefore = ch->CountSpecifyItem(nextVnum);
			if (ch->DoRefine(item, false))
			{
				const bool success = ch->CountSpecifyItem(nextVnum) > resultCountBefore;
				BroadcastPlayerBotRefineSuccess(ch, item, (int)plusLevel + 1);
				sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u",
						success ? "SUCCESS" : "FAILED_BURNED", ch->GetPlayerID(), ch->GetName(),
						oldVnum, nextVnum, plusLevel + 1);
				++refinedCount;
			}
			else
			{
				sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u (requirements/state)",
						ch->GetPlayerID(), ch->GetName(), oldVnum, plusLevel);
			}

			// Do not equip the result again between consecutive + levels.  Keep it
			// visibly in the inventory for the complete blacksmith session and let
			// the town state equip the final/best result once refining is finished.
		}

		return refinedCount > 0;
	}

	bool ManagePlayerBotMiscMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		CompactPlayerBotPotionStacks(ch);
		SellPlayerBotExcessPotions(ch);

		// Count red and blue potions
		size_t redCount = 0;
		size_t blueCount = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051)
				redCount += item->GetCount();
			else if (vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				blueCount += item->GetCount();
		}

		// Miscellaneous loot belongs to Handlarka. Weapons and wearable equipment
		// are deliberately left for their own specialist merchants.
		SellPlayerBotJunkAtMerchant(ch, BOT_MERCHANT_MISC, "misc_merchant");

		// Economical potion purchase at Handlarka.  Refining is intentionally
		// performed in the separate blacksmith phase after the bot walks there.
		const bool isMage = (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		const BYTE botLvl = ch->GetLevel();

		if (botLvl <= 10)
		{
			if (redCount < 30 && ch->GetGold() >= 300)
			{
				ch->PointChange(POINT_GOLD, -240);
				ch->AutoGiveItem(27001, 30); // Red Potion (S) 30x
			}
			if (isMage && blueCount < 20 && ch->GetGold() >= 400)
			{
				ch->PointChange(POINT_GOLD, -360);
				ch->AutoGiveItem(27004, 15); // Blue Potion (S) 15x
			}
		}
		else
		{
			// Unit prices are the ones the old fixed purchases implied: 20 yang for
			// a Red Potion (M), 32 for a Blue Potion (M).
			const DWORD RED_TARGET = 800;
			const DWORD BLUE_TARGET = 600;
			const DWORD RED_UNIT = 20;
			const DWORD BLUE_UNIT = 32;
			// Standing at the merchant already: fill the belt right up whatever the
			// level, because this costs nothing extra. The decision to make the
			// trip at all lives in NeedsPlayerBotPotions and is far stricter.
			// Never spend more than half the purse, so shopping can't leave the
			// bot unable to afford a refine.
			if (redCount < RED_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(RED_TARGET - redCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / RED_UNIT);
				const DWORD buy = want < affordable ? want : affordable;
				if (buy > 0)
				{
					ch->PointChange(POINT_GOLD, -(int)(buy * RED_UNIT));
					ch->AutoGiveItem(27002, buy);
				}
			}
			// Skills spend SP continuously, so a warrior wants a reserve too. It
			// simply must never be the thing that forbids travelling.
			if (blueCount < BLUE_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(BLUE_TARGET - blueCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / BLUE_UNIT);
				const DWORD buy = want < affordable ? want : affordable;
				if (buy > 0)
				{
					ch->PointChange(POINT_GOLD, -(int)(buy * BLUE_UNIT));
					ch->AutoGiveItem(27005, buy);
				}
			}
		}

		// Even the level-one shoes add movement speed. Missing footwear is therefore
		// a progression problem, not cosmetic equipment.
		if (NeedsPlayerBotProgressionBoots(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionBootsVnum(ch), "boots");

		return true;
	}

	bool ManagePlayerBotWeaponMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_WEAPON, "weapon_merchant");
		bool bought = false;
		const bool isArcher = ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2;
		// A missing weapon is essential, so restore the cheap functional weapon
		// first. With a bow already equipped, ammunition takes priority over a
		// level-tier upgrade: buying a better bow and leaving zero Yang for arrows
		// merely creates a better-equipped idle bot.
		if (!ch->GetWear(WEAR_WEAPON))
			bought = BuyPlayerBotEmergencyWeapon(ch) || bought;
		if (isArcher)
			bought = BuyPlayerBotArrowsAtMerchant(ch) || bought;
		if (NeedsPlayerBotProgressionWeapon(ch) &&
				(!isArcher || CountPlayerBotArrows(ch) >= PLAYERBOT_ARROW_RESTOCK_THRESHOLD))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWeaponVnum(ch), "weapon") || bought;
		return sold || bought;
	}

	bool ManagePlayerBotArmorMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_ARMOR, "armor_merchant");
		bool bought = NeedsPlayerBotProgressionArmor(ch) &&
				BuyPlayerBotProgressionGear(ch,
						GetPlayerBotProgressionArmorVnum(ch), "armor");
		if (NeedsPlayerBotProgressionShield(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionShieldVnum(ch), "shield") || bought;
		if (NeedsPlayerBotProgressionHelmet(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionHelmetVnum(ch), "helmet") || bought;
		return sold || bought;
	}

	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetRefinedVnum() == 0 ||
				item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
			return false;

		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe || ch->GetGold() < ch->ComputeRefineFee(recipe->cost))
			return false;

		for (int i = 0; i < recipe->material_count; ++i)
		{
			if (ch->CountSpecifyItem(recipe->materials[i].vnum) < recipe->materials[i].count)
				return false;
		}
		return true;
	}

	bool HasPlayerBotRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, item) &&
					CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}
		return false;
	}

	bool HasPlayerBotPriorityRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		// Cross-map blacksmith trips are reserved for currently worn essentials.
		// A routine accessory or spare can wait until the next normal M1 visit, but
		// a weapon/body/shield/helmet/boots upgrade should not sit unused in M2/M3.
		const BYTE coreWearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD, WEAR_FOOTS
		};
		for (size_t i = 0; i < sizeof(coreWearSlots) / sizeof(coreWearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(coreWearSlots[i]);
			if (item && CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}
		return false;
	}

	void CountPlayerBotPotions(LPCHARACTER ch, size_t& redCount, size_t& blueCount)
	{
		redCount = 0;
		blueCount = 0;
		if (!ch || !ch->IsItemLoaded())
			return;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051)
				redCount += item->GetCount();
			else if (vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				blueCount += item->GetCount();
		}
	}

	bool NeedsPlayerBotPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		if (ch->GetLevel() <= 10)
			return (redCount < 30 && ch->GetGold() >= 300) ||
					(isMage && blueCount < 20 && ch->GetGold() >= 400);
		// Only a belt that is nearly out is worth crossing a map for. A bot with
		// half its potions left has no business walking away from a good spot -
		// it will fill up anyway the next time something else brings it to town.
		return (redCount < 150 && ch->GetGold() >= 1200) ||
				(blueCount < 100 && ch->GetGold() >= 1200);
	}

	bool NeedsPlayerBotEmergencyPotions(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		const bool isMage = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		// Normal restocking happens at 50/30 (or 10) units. Cross-map travel is
		// justified only by a genuinely short combat reserve, not by one consumed pot.
		//
		// Blue potions restore SP, so an empty belt is an emergency for a caster
		// and nothing at all for a warrior. Treating "no blue potions" as critical
		// for everybody put 210 of 442 warriors and ninjas into a permanent fake
		// emergency: they were always considered one step from being unable to
		// fight, so they abandoned every trip the moment their items finished
		// loading and shuttled straight back to town.
		return redCount < 10 || (isMage && blueCount < 8);
	}

	bool NeedsPlayerBotCriticalTownServices(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// These problems can make continued combat impossible or waste most future
		// drops, so they justify an immediate cross-map return.
		if (ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				ch->GetWear(WEAR_SHIELD) == NULL || ch->GetWear(WEAR_HEAD) == NULL ||
				ch->GetWear(WEAR_FOOTS) == NULL || NeedsPlayerBotEmergencyPotions(ch) ||
				NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0)
			return true;

		size_t occupiedGridCells = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item)
				occupiedGridCells += std::max(1, (int)item->GetSize());
		}
		return occupiedGridCells * 100 >= INVENTORY_MAX_NUM * 45;
	}

	// A missing weapon or body armour, an empty potion belt, no arrows or a full
	// inventory really do stop a bot from playing, and must outrank travelling.
	// A missing helmet, boots or shield only make it a little weaker. Treating
	// those as equally critical trapped a bot for good whenever the town could
	// not sell it the missing piece: it always wanted to shop, so it was never
	// allowed to travel, and it kept farming level-3 wolves in Joan at level 27.
	bool BlocksPlayerBotTravel(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		return ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL ||
				NeedsPlayerBotEmergencyPotions(ch) || NeedsPlayerBotArrows(ch) ||
				ch->GetEmptyInventory(3) < 0;
	}

	bool NeedsPlayerBotM1OnlyServices(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		// Bokjung has no profession trainers or Biologist. Everything else can be
		// handled locally in M2, so only these two real activities justify M2 -> M1.
		if (ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN)
			return true;

		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch);
		if (!mission)
			return false;
		const int accepted = std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, (int)mission->requiredCount - accepted);
		return remaining > 0 && ch->CountSpecifyItem(mission->itemVnum) >= remaining;
	}

	// Above this level Bokjung has nothing left to offer, so nothing there is
	// worth keeping a bot for either.
	const BYTE PLAYERBOT_M2_COHORT_MAX_LEVEL = 35;

	bool IsPlayerBotPastM2Ceiling(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL;
	}

	bool IsPlayerBotM2LevelingCohort(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 20 ||
				ch->GetLevel() > PLAYERBOT_M2_COHORT_MAX_LEVEL)
			return false;
		// Levels 20-21 still have a little useful M1 progression, so retain a small
		// stable minority there. At level 22 every ordinary leveler graduates to M2.
		return ch->GetLevel() >= 22 ||
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d325850U) % 10U) != 0;
	}

	bool ShouldPlayerBotLeaveRemoteMapForRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!HasPlayerBotPriorityRefineOpportunity(ch))
			return false;
		if (state.dwNextRemoteRefineReturnTime == 0)
		{
			const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^
					(dwNow / 60000U) ^ 0x52455455U) %
					(PLAYERBOT_REMOTE_REFINE_RETURN_MAX_DELAY -
					 PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + 1);
			state.dwNextRemoteRefineReturnTime = dwNow +
					PLAYERBOT_REMOTE_REFINE_RETURN_MIN_DELAY + spread;
			return false;
		}
		return dwNow >= state.dwNextRemoteRefineReturnTime;
	}

	bool IsPlayerBotFrontierMap(long mapIndex)
	{
		return mapIndex == PLAYERBOT_MAP_ORC_VALLEY || mapIndex == PLAYERBOT_MAP_DESERT;
	}

	// The map whose ordinary spawns still sit inside this bot's useful level
	// window, or 0 when Bokjung is still the right place for it.
	long GetPlayerBotFrontierMapForLevel(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const BYTE level = ch->GetLevel();
		if (level >= PLAYERBOT_ORC_VALLEY_MIN_LEVEL && level <= PLAYERBOT_ORC_VALLEY_MAX_LEVEL)
			return PLAYERBOT_MAP_ORC_VALLEY;
		if (level >= PLAYERBOT_DESERT_MIN_LEVEL && level <= PLAYERBOT_DESERT_MAX_LEVEL)
			return PLAYERBOT_MAP_DESERT;
		return 0;
	}

	// How far from town a personality is willing to play, in eighths. Personality
	// used to decide only how far a bot would push a refine, so every character
	// hunted in the same places; this is what makes the trait visible in-world.
	BYTE GetPlayerBotFrontierAppetite(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return 7; // explorer: almost always out on the far maps
			case BOT_PERSONALITY_METIN_BREAKER:
				return 6; // both frontier maps carry their own Metin spawns
			case BOT_PERSONALITY_GEAR_SPECIALIST:
				return 6; // Orc Valley is where the level-30 weapons drop
			case BOT_PERSONALITY_TEAM_COMPANION:
				return 4; // stays near the party pool at least half the time
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return 2; // protects what it has earned, keeps a town run short
			default:
				return 5; // steady adventurer
		}
	}

	// Due for a fishing trip: old enough for the rod, and its cooldown is up.
	bool WantsPlayerBotFishingTrip(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!IsPlayerBotAngler(ch, state))
			return false;
		return state.dwNextFishingCheckTime == 0 || dwNow >= state.dwNextFishingCheckTime;
	}

	bool ShouldPlayerBotLeaveForFrontier(LPCHARACTER ch)
	{
		if (!ch || GetPlayerBotFrontierMapForLevel(ch) == 0)
			return false;
		TPlayerBotAIStateMap::const_iterator it =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = it != s_mapPlayerBotAIStates.end()
				? it->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// Above the Bokjung ceiling there is nothing there left to stay behind
		// for, so the reserve rule has nothing to protect and only strands bots.
		if (IsPlayerBotPastM2Ceiling(ch))
			return true;
		// Below it Bokjung must never empty out completely: even the keenest
		// explorer leaves one bot in eight behind for the town, its Bestials
		// and the local party pool.
		const DWORD appetite = GetPlayerBotFrontierAppetite(personality);
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x46524f4eU) % 8U) < appetite;
	}

	// A wanderer settles in for a long session; a careful collector treats the
	// trip as an errand and heads back while it still has potions left.
	DWORD GetPlayerBotFrontierVisitTime(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_WANDERER:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME * 2;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME / 2;
			default:
				return PLAYERBOT_FRONTIER_MAX_VISIT_TIME;
		}
	}

	bool ShouldPlayerBotVisitM3(LPCHARACTER ch)
	{
		if (!HasPlayerBotM3ReadyEquipment(ch) || ch->GetLevel() > 24 ||
				HasPlayerBotSpecialLevel30Weapon(ch, true))
			return false;
		// A stable third of the eligible population farms infected animals for
		// class-specific level-30 weapons. Other bots remain in M2 for Bestials.
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d335850U) % 3U) == 0;
	}

	bool ShouldPlayerBotHuntM2Bestials(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 25 || ch->GetLevel() > 35 ||
				!HasPlayerBotM3ReadyEquipment(ch) ||
				HasPlayerBotSpecialLevel30Weapon(ch, true) ||
				ShouldPlayerBotVisitM3(ch))
			return false;
		// M3 already owns one third of the eligible weapon hunters. Half of the
		// remaining cohort searches the two rare Bestial spawns in M2, while the
		// rest keeps levelling normally instead of camping one pair of enemies.
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x42455354U) % 2U) == 0;
	}

	bool ShouldPlayerBotPursueHorseExpedition(LPCHARACTER ch, DWORD dwNow)
	{
		if (!CanPlayerBotAdvanceHorse(ch))
			return false;

		// A combat horse matters most to Warriors and weapon Suras, but it must be
		// one goal among several rather than a compulsory conveyor belt through the
		// dungeon.  The cohort rotates every 30 minutes and again after every earned
		// horse level.  Eventually every build gets opportunities while most bots
		// continue levelling in M2 at any given time.
		const bool hasCombatHorse = ch->GetHorseLevel() >= 11;
		BYTE chance = 10;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
				chance = hasCombatHorse ? 4 : (ch->GetHorseLevel() == 0 ? 34 : 26);
				break;
			case JOB_SURA:
				// Skill group 1 is Weaponry (WP); group 2 is Black Magic.
				chance = ch->GetSkillGroup() == 1
						? (hasCombatHorse ? 4 : (ch->GetHorseLevel() == 0 ? 32 : 25))
						: (hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 14 : 9));
				break;
			case JOB_ASSASSIN:
				chance = ch->GetSkillGroup() == 2
						? (hasCombatHorse ? 1 : (ch->GetHorseLevel() == 0 ? 6 : 4))
						: (hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 18 : 14));
				break;
			case JOB_SHAMAN:
				chance = hasCombatHorse ? 2 : (ch->GetHorseLevel() == 0 ? 15 : 10);
				break;
		}
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (stateIt != s_mapPlayerBotAIStates.end() &&
				stateIt->second.bAmbition == BOT_AMBITION_HORSE && !hasCombatHorse)
			chance = std::min<BYTE>(55, chance + 15);

		const DWORD window = dwNow / (30U * 60U * 1000U);
		const DWORD seed = ch->GetPlayerID() ^ (window * 0x9e3779b9U) ^
				((DWORD)(ch->GetHorseLevel() + 1) * 0x85ebca6bU);
		return (PlayerBotNavHash(seed ^ 0x484f5253U) % 100U) < chance;
	}

	int GetPlayerBotDesiredHorseMedalStock(LPCHARACTER ch)
	{
		if (!ch)
			return 1;
		// The high-priority builds occasionally prepare the next horse level in the
		// same visit. Other classes leave after one medal, freeing dungeon capacity
		// and returning to ordinary experience progression much sooner.
		const bool highPriority = ch->GetJob() == JOB_WARRIOR ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 1);
		return highPriority
				? 1 + (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d454441U) % 2U)
				: 1;
	}

	bool TransitionPlayerBotMap(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(targetMap);
		if (!navigation.Init(targetMap))
		{
			sys_err("PLAYERBOT_WORLD: target navigation unavailable pid=%u name=%s map=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), targetMap, reason ? reason : "?");
			return false;
		}

		const long oldMap = ch->GetMapIndex();
		const bool wasRiding = ch->IsRiding();
		if (ch->GetParty())
			ch->GetParty()->Quit(ch->GetPlayerID());
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		// A PC mount and the separately summoned horse are two different server
		// entities. StopRiding() summons the latter on the old map, so explicitly
		// remove it before Show(). Otherwise a rider can leave behind an orphaned
		// horse at a dungeon portal (issue #4).
		if (wasRiding)
			ch->StopRiding();
		ch->HorseSummon(false);
		ClearPlayerBotRoute(state, true);
		state.bVisitingShop = false;
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		if (!ch->Show(targetMap, targetX, targetY, 0))
		{
			if (wasRiding && !ch->IsRiding())
				ch->StartRiding();
			sys_err("PLAYERBOT_WORLD: transition failed pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason ? reason : "?");
			return false;
		}
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, targetX, targetY, 0, dwNow);
		if (wasRiding && ch->GetHorseHealth() > 0 && ch->GetHorseStamina() > 0)
			ch->StartRiding();
		ch->Save();
		state.dwNextWanderTime = dwNow + number(1500, 4500);
		state.dwNextHorseRideCheckTime = dwNow + 1000;
		state.dwDungeonEnteredTime = targetMap == PLAYERBOT_MAP_MONKEY_EASY ? dwNow : 0;
		state.dwM3EnteredTime = targetMap == PLAYERBOT_MAP_CHUNJO_M3 ? dwNow : 0;
		state.dwFrontierEnteredTime = IsPlayerBotFrontierMap(targetMap) ? dwNow : 0;
		if (targetMap == PLAYERBOT_MAP_CHUNJO_M3)
			state.dwNextRemoteRefineReturnTime = 0;
		sys_log(0, "PLAYERBOT_WORLD: transitioned pid=%u name=%s from=%ld to=%ld pos=(%ld,%ld) reason=%s",
				ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, targetX, targetY,
				reason ? reason : "?");
		return true;
	}

	bool MovePlayerBotToWorldPortal(LPCHARACTER ch, TPlayerBotAIState& state,
			long portalX, long portalY, long targetMap, long targetX, long targetY,
			DWORD dwNow, const char* reason)
	{
		if (!ch)
			return false;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// Warp NPCs trigger at 300 units and expect a real client reconnect. Switch
		// server-side at 900 units so a bot descriptor never enters that code path,
		// while the character still visibly walks all the way to the portal area.
		if (DISTANCE_APPROX(ch->GetX() - portalX, ch->GetY() - portalY) <= 900)
			return TransitionPlayerBotMap(ch, state, targetMap, targetX, targetY, dwNow, reason);

		MovePlayerBot(ch, portalX, portalY, dwNow, 24, true, true);
		return true;
	}

	// Being in a fight is not the same as having something selected. A bot is in
	// the fight when blows are being exchanged, when the monster is coming for
	// it, or when it already stands within reach. A mob picked out across the
	// field is none of those and must not postpone a decision to leave the map.
	bool IsPlayerBotEngagedWith(LPCHARACTER ch, LPCHARACTER victim,
			const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !victim)
			return false;
		if (victim->GetVictim() == ch)
			return true;
		if (state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime <= PLAYERBOT_TRAVEL_ENGAGED_WINDOW)
			return true;
		return DISTANCE_APPROX(ch->GetX() - victim->GetX(),
				ch->GetY() - victim->GetY()) <= PLAYERBOT_TRAVEL_ENGAGED_RANGE;
	}

	bool ManagePlayerBotWorldTravel(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || state.bVisitingBiologist ||
				state.bVisitingStable || state.bRecoveringAfterDeath || state.bTacticalRetreat)
			return false;

		const long mapIndex = ch->GetMapIndex();
		const bool hasMedal = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0;
		const bool pursuesHorseExpedition =
				ShouldPlayerBotPursueHorseExpedition(ch, dwNow);
		const bool needsHorseExpedition = pursuesHorseExpedition && !hasMedal;
		// GetWear answers NULL for every slot until the item cache has loaded,
		// which is exactly the state a character is in for the first seconds after
		// a map change. Trusting it there made a bot believe it had lost its
		// weapon the moment it arrived somewhere.
		const bool needsEssentialWeaponSupply = ch->IsItemLoaded() &&
				(ch->GetWear(WEAR_WEAPON) == NULL || NeedsPlayerBotArrows(ch));
		const bool m2LevelingCohort = IsPlayerBotM2LevelingCohort(ch);
		const bool wantsM3 = ShouldPlayerBotVisitM3(ch);
		const bool needsCriticalTownServices = NeedsPlayerBotCriticalTownServices(ch);
		const bool needsM1OnlyServices = NeedsPlayerBotM1OnlyServices(ch);
		// M2 has its own blacksmith. Only the remote M3 farm needs to schedule a
		// return to town for equipment progression.
		const bool scheduledRemoteRefine = mapIndex == PLAYERBOT_MAP_CHUNJO_M3 &&
				ShouldPlayerBotLeaveRemoteMapForRefining(ch, state, dwNow);

		// Leaving the Monkey Dungeon is a decision, not a pathfinding exercise.
		// Evaluate it before yielding to an existing victim: a monster near the
		// portal must not keep a finished, timed-out or unequipped bot here forever.
		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY)
		{
			if (state.dwDungeonEnteredTime == 0)
				state.dwDungeonEnteredTime = dwNow;
			const bool visitExpired = dwNow - state.dwDungeonEnteredTime >=
					PLAYERBOT_MONKEY_MAX_VISIT_TIME;
			const int medalCount = ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM);
			playerbot_world_rules::TMonkeyVisitContext context;
			context.needsEssentialSupply = needsEssentialWeaponSupply;
			context.needsPotions = NeedsPlayerBotEmergencyPotions(ch);
			context.medalCount = medalCount;
			context.desiredMedalCount = GetPlayerBotDesiredHorseMedalStock(ch);
			context.visitExpired = visitExpired;
			// Re-evaluate the rotating cohort even inside the dungeon. Bots which are
			// no longer selected finish their current medal (if any) and leave instead
			// of occupying the dungeon until its absolute 30-minute timeout.
			context.canAdvanceHorse = CanPlayerBotAdvanceHorse(ch) &&
					pursuesHorseExpedition;
			const playerbot_world_rules::EMonkeyExitDecision exitDecision =
					playerbot_world_rules::DecideMonkeyExit(context);
			if (exitDecision != playerbot_world_rules::MONKEY_STAY)
			{
				SetPlayerBotGoal(ch, state,
						exitDecision == playerbot_world_rules::MONKEY_EXIT_RESTOCK
						? BOT_GOAL_RESTOCK : BOT_GOAL_HORSE, dwNow);
				const char* reason = "monkey_horse_complete_direct";
				if (exitDecision == playerbot_world_rules::MONKEY_EXIT_RESTOCK)
					reason = "monkey_restock_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_MEDAL_READY)
					reason = "monkey_medal_found_direct";
				else if (exitDecision == playerbot_world_rules::MONKEY_EXIT_TIMEOUT)
					reason = "monkey_timeout_direct";
				const bool transitioned = TransitionPlayerBotMap(ch, state,
						PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_MONKEY_RETURN_X,
						PLAYERBOT_M2_MONKEY_RETURN_Y, dwNow, reason);
				if (transitioned && medalCount == 0)
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return transitioned;
			}
		}

		// M3 is a focused level-30 weapon farm, not a levelling map. A bot which
		// reaches level 25 graduates immediately, even if an old victim is still
		// alive, and resumes normal progression in M2.
		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M3 && ch->GetLevel() > 24)
		{
			SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M3_RETURN_PORTAL_X, PLAYERBOT_M3_RETURN_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X,
					PLAYERBOT_M2_FROM_M3_Y, dwNow, "m3_level_graduated");
		}

		// A bot should not walk away from a fight -- but "holds a target" was
		// standing in for "is fighting", and in a dense respawn those are not the
		// same thing at all. Something is always in range, so this check used to
		// return before the routing below had ever been consulted, and the
		// arrival areas of frontier maps quietly became one-way (issue #10).
		//
		// Three cases now. A Metin already under the hammer is always finished
		// first; ShouldPlayerBotAbandonStone releases a stalled one. A live fight
		// holds travel back, but only for a bounded grace period, so an endless
		// chain of packs can no longer outrank the decision to leave. A target
		// merely selected across the field does not delay anything.
		LPCHARACTER victim = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!victim || victim->IsDead())
			state.dwTravelBlockedSince = 0;
		else
		{
			if (state.dwTravelBlockedSince == 0)
				state.dwTravelBlockedSince = dwNow;
			const bool graceLeft = dwNow - state.dwTravelBlockedSince <
					PLAYERBOT_TRAVEL_FIGHT_GRACE;
			if (victim->IsStone() ||
					(graceLeft && IsPlayerBotEngagedWith(ch, victim, state, dwNow)))
				return false;
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M1)
		{
			// Compact and sell an oversized potion reserve before the first trip to
			// M2. Once the bot is already outside M1, excess potions alone must not
			// drag it back across maps; it can keep levelling until a real restock or
			// inventory visit is needed.
			const bool townVisitRecentlyCompleted = state.dwNextShopCheckTime != 0 &&
					dwNow < state.dwNextShopCheckTime;
			const bool needsAnyRefine = HasPlayerBotRefineOpportunity(ch);
			const bool needsTownPreparation = NeedsPlayerBotPotions(ch) ||
					CountPlayerBotJunkItems(ch) >= 12 || needsAnyRefine;
			// The soft needs get one town visit to be met. If the bot has just
			// been shopping and still wants something, the town cannot supply it,
			// and standing here is worse than moving on.
			if (hasMedal || BlocksPlayerBotTravel(ch) || needsM1OnlyServices ||
					HasPlayerBotExcessPotions(ch) ||
					((needsTownPreparation || needsCriticalTownServices) &&
					 !townVisitRecentlyCompleted))
				return false;
			// The M2 cohort ends at 35, but the frontier maps begin at 30 and run
			// far past it. Testing only the cohort here meant a level 36+ bot fell
			// out of this gate on every tick and stayed in Joan for good, hunting
			// level-3 wolves - which is the "boty bija psy" the Discord keeps
			// reporting. Anything with somewhere better to be gets through.
			if (!needsHorseExpedition && !m2LevelingCohort && !wantsM3 &&
					GetPlayerBotFrontierMapForLevel(ch) == 0 &&
					!IsPlayerBotPastM2Ceiling(ch))
				return false;
			if (state.dwNextWorldTravelTime == 0)
			{
				const bool graduatedFromM1 = ch->GetLevel() >= 22 && !needsHorseExpedition;
				const DWORD minDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MIN_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MIN_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MIN_DELAY);
				const DWORD maxDelay = needsHorseExpedition ? PLAYERBOT_HORSE_TRAVEL_MAX_DELAY :
						(graduatedFromM1 ? PLAYERBOT_LEVEL22_TRAVEL_MAX_DELAY :
						 PLAYERBOT_WORLD_TRAVEL_MAX_DELAY);
				const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x54524156U) %
						(maxDelay - minDelay + 1);
				state.dwNextWorldTravelTime = dwNow + minDelay + spread;
				return false;
			}
			if (dwNow < state.dwNextWorldTravelTime)
				return false;

			// Joan has a Teleporter of its own, so a bot which has already outgrown
			// Bokjung can leave for the frontier directly. Routing it through M2
			// first would queue it behind every town errand in the village, which
			// is what left Orc Valley empty while the Desert filled up from the
			// bots that happened to already be in Bokjung.
			// Let it fish before it is handed the next hunting destination.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
				return false;

			if (!needsHorseExpedition && !wantsM3)
			{
				const long directMap = ShouldPlayerBotLeaveForFrontier(ch)
						? GetPlayerBotFrontierMapForLevel(ch) : 0;
				if (directMap != 0)
				{
					const bool toDesert = directMap == PLAYERBOT_MAP_DESERT;
					SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
					return MovePlayerBotToWorldPortal(ch, state,
							PLAYERBOT_M1_TELEPORTER_X, PLAYERBOT_M1_TELEPORTER_Y,
							directMap,
							toDesert ? PLAYERBOT_DESERT_ARRIVAL_X : PLAYERBOT_ORC_VALLEY_ARRIVAL_X,
							toDesert ? PLAYERBOT_DESERT_ARRIVAL_Y : PLAYERBOT_ORC_VALLEY_ARRIVAL_Y,
							dwNow,
							toDesert ? "m1_direct_to_desert" : "m1_direct_to_orc_valley");
				}
			}

			SetPlayerBotGoal(ch, state, needsHorseExpedition ? BOT_GOAL_HORSE :
					(wantsM3 ? BOT_GOAL_GET_EQUIPMENT : BOT_GOAL_LEVEL_UP), dwNow);
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M1_TO_M2_PORTAL_X, PLAYERBOT_M1_TO_M2_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_ARRIVAL_X, PLAYERBOT_M2_ARRIVAL_Y,
					dwNow, needsHorseExpedition ? "horse_to_m2" : "level_to_m2");
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M2)
		{
			// ManagePlayerBotHorse owns the medal on M2 and walks to the local Stable
			// Boy. Returning to M1 here was the source of the needless three-map trip.
			if (hasMedal)
				return false;

			// Profession trainers and the Biologist only exist in Joan. Routine gear,
			// potion, inventory and refine needs are served by the real Bokjung NPCs.
			if (needsM1OnlyServices)
			{
				SetPlayerBotGoal(ch, state, ch->GetSkillGroup() == 0
						? BOT_GOAL_CHOOSE_PROFESSION : BOT_GOAL_BIOLOGIST, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X,
						PLAYERBOT_M1_RETURN_Y, dwNow, "m1_only_service");
			}
			// Same rule in Bokjung: its own shops own this need, but only until a
			// visit has actually happened. Otherwise a bot the town cannot equip
			// would never reach the frontier maps either.
			if (BlocksPlayerBotTravel(ch))
				return false;
			if (needsCriticalTownServices && (state.dwNextShopCheckTime == 0 ||
					dwNow < state.dwNextShopCheckTime))
				return false; // local M2 town visit owns this need

			if (needsHorseExpedition)
			{
				// Honour the rest period set by a failed/timed-out expedition and
				// stagger fresh M2 populations after a restart. Without this guard a
				// direct exit was followed by an immediate direct re-entry.
				if (state.dwNextWorldTravelTime == 0)
				{
					const DWORD spread = PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d4f4e4bU) %
							(PLAYERBOT_HORSE_TRAVEL_MAX_DELAY -
							 PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + 1);
					state.dwNextWorldTravelTime = dwNow +
							PLAYERBOT_HORSE_TRAVEL_MIN_DELAY + spread;
					return false;
				}
				if (playerbot_world_rules::IsTravelCooldownActive(
						dwNow, state.dwNextWorldTravelTime))
					return false;
				SetPlayerBotGoal(ch, state, BOT_GOAL_HORSE, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_MONKEY_PORTAL_X, PLAYERBOT_M2_MONKEY_PORTAL_Y,
						PLAYERBOT_MAP_MONKEY_EASY, PLAYERBOT_MONKEY_EASY_ARRIVAL_X,
						PLAYERBOT_MONKEY_EASY_ARRIVAL_Y, dwNow, "horse_to_monkey");
			}

			if (wantsM3 && !needsCriticalTownServices)
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M3_TELEPORTER_X, PLAYERBOT_M2_TO_M3_TELEPORTER_Y,
						PLAYERBOT_MAP_CHUNJO_M3, PLAYERBOT_M3_ARRIVAL_X,
						PLAYERBOT_M3_ARRIVAL_Y, dwNow, "level30_weapon_to_m3");
			}

			// The river is in Joan, and every bot old enough to hold a rod has long
			// since left it - so fishing only ever happens if the trip is a real
			// destination. Ranked above the frontier maps: a session is short, and
			// the pearls it brings back are worth more than the hunting it skips.
			if (WantsPlayerBotFishingTrip(ch, state, dwNow))
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_HUNTING, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X,
						PLAYERBOT_M1_RETURN_Y, dwNow, "fishing_to_m1");
			}

			// Bokjung's own spawns stop paying long before the M2 band ends. Bots
			// which have outgrown them move on to Orc Valley and then the Desert
			// instead of grinding monsters they would rather walk past.
			const long frontierMap = ShouldPlayerBotLeaveForFrontier(ch)
					? GetPlayerBotFrontierMapForLevel(ch) : 0;
			if (frontierMap != 0)
			{
				const bool toDesert = frontierMap == PLAYERBOT_MAP_DESERT;
				SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
				return MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M3_TELEPORTER_X, PLAYERBOT_M2_TO_M3_TELEPORTER_Y,
						frontierMap,
						toDesert ? PLAYERBOT_DESERT_ARRIVAL_X : PLAYERBOT_ORC_VALLEY_ARRIVAL_X,
						toDesert ? PLAYERBOT_DESERT_ARRIVAL_Y : PLAYERBOT_ORC_VALLEY_ARRIVAL_Y,
						dwNow, toDesert ? "level_to_desert" : "level_to_orc_valley");
			}

			// Sending a bot back to Joan is only right when it has outgrown
			// nothing yet. Doing it above the ceiling produced an M1 <-> M2 loop:
			// Joan let it leave, Bokjung refused to keep it, and neither ever
			// hunted anything.
			if (!m2LevelingCohort && !IsPlayerBotPastM2Ceiling(ch))
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
				const bool moving = MovePlayerBotToWorldPortal(ch, state,
						PLAYERBOT_M2_TO_M1_PORTAL_X, PLAYERBOT_M2_TO_M1_PORTAL_Y,
						PLAYERBOT_MAP_CHUNJO_M1, PLAYERBOT_M1_RETURN_X, PLAYERBOT_M1_RETURN_Y,
						dwNow, "m2_level_range_complete");
				if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1)
					state.dwNextWorldTravelTime = dwNow + number(300000, 900000);
				return moving;
			}

			return false; // designated M2 leveler: hunt normally
		}

		if (mapIndex == PLAYERBOT_MAP_CHUNJO_M3)
		{
			if (state.dwM3EnteredTime == 0)
				state.dwM3EnteredTime = dwNow;
			const bool visitExpired = dwNow - state.dwM3EnteredTime >= PLAYERBOT_M3_MAX_VISIT_TIME;
			if (!visitExpired && !needsCriticalTownServices && !needsM1OnlyServices &&
					!scheduledRemoteRefine &&
					!HasPlayerBotSpecialLevel30Weapon(ch, true))
				return false;

			SetPlayerBotGoal(ch, state,
					(needsCriticalTownServices || needsM1OnlyServices) ? BOT_GOAL_RESTOCK :
					(scheduledRemoteRefine ? BOT_GOAL_GET_EQUIPMENT : BOT_GOAL_LEVEL_UP), dwNow);
			const char* reason = "m3_weapon_found";
			if (needsCriticalTownServices || needsM1OnlyServices)
				reason = "m3_services_to_m2";
			else if (scheduledRemoteRefine)
				reason = "m3_scheduled_refine_to_m2";
			else if (visitExpired)
				reason = "m3_visit_complete";
			return MovePlayerBotToWorldPortal(ch, state,
					PLAYERBOT_M3_RETURN_PORTAL_X, PLAYERBOT_M3_RETURN_PORTAL_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X,
					PLAYERBOT_M2_FROM_M3_Y, dwNow, reason);
		}

		if (IsPlayerBotFrontierMap(mapIndex))
		{
			if (state.dwFrontierEnteredTime == 0)
				state.dwFrontierEnteredTime = dwNow;
			const DWORD stayed = dwNow - state.dwFrontierEnteredTime;
			const bool visitExpired = stayed >=
					GetPlayerBotFrontierVisitTime(state.bPersonality);
			// Two minutes of actually playing here before anything but a real
			// emergency may send the bot home again.
			const bool settledIn = stayed >= PLAYERBOT_FRONTIER_MIN_VISIT_TIME;
			// Outgrowing the map matters as much as running out of potions: neither
			// Orc Valley nor the Desert has a merchant, a blacksmith or a trainer.
			const bool outOfBand = GetPlayerBotFrontierMapForLevel(ch) != mapIndex;
			// Only a need that actually stops the bot playing is worth the trip
			// home. Sending it back for a helmet the town cannot sell turned the
			// journey into a shuttle: it arrived, saw the same unmet need, and
			// left again without ever fighting anything here.
			// A blocking need still wins immediately - a bot with no weapon left
			// cannot wait out a timer. Everything else waits until the bot has
			// been here long enough for the trip to have been worth making.
			const bool blocked = BlocksPlayerBotTravel(ch);
			const bool needsTown = blocked ||
					(settledIn && (needsM1OnlyServices || needsEssentialWeaponSupply));
			if (!visitExpired && !outOfBand && !needsTown)
				return false;

			SetPlayerBotGoal(ch, state, needsTown ? BOT_GOAL_RESTOCK : BOT_GOAL_LEVEL_UP, dwNow);
			const char* reason = "frontier_visit_complete";
			if (needsTown)
				reason = "frontier_services_to_m2";
			else if (outOfBand)
				reason = "frontier_level_graduated";
			const bool inDesert = mapIndex == PLAYERBOT_MAP_DESERT;
			return MovePlayerBotToWorldPortal(ch, state,
					inDesert ? PLAYERBOT_DESERT_EXIT_X : PLAYERBOT_ORC_VALLEY_EXIT_X,
					inDesert ? PLAYERBOT_DESERT_EXIT_Y : PLAYERBOT_ORC_VALLEY_EXIT_Y,
					PLAYERBOT_MAP_CHUNJO_M2, PLAYERBOT_M2_FROM_M3_X, PLAYERBOT_M2_FROM_M3_Y,
					dwNow, reason);
		}

		if (mapIndex == PLAYERBOT_MAP_MONKEY_EASY)
			return false; // stay and fight; departure was handled before victim yielding

		// Anything else means the bot was moved somewhere no branch above owns:
		// the frontier maps carry real warp NPCs of their own, and walking inside
		// one's trigger radius hands the character to a map the AI has no plan
		// for. Nothing would ever bring it back, so it would sit there for good.
		SetPlayerBotGoal(ch, state, BOT_GOAL_LEVEL_UP, dwNow);
		if (TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_CHUNJO_M2,
				PLAYERBOT_M2_FROM_M3_X, PLAYERBOT_M2_FROM_M3_Y, dwNow, "stranded_recovery"))
		{
			sys_log(0, "PLAYERBOT_WORLD: recovered pid=%u name=%s from unmanaged map=%ld",
					ch->GetPlayerID(), ch->GetName(), mapIndex);
			return true;
		}
		return false;
	}

	BYTE GetPlayerBotFirstInteriorTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedMisc)
			return BOT_TOWN_PHASE_MISC_MERCHANT;
		if (state.bTownNeedBlacksmith)
			return BOT_TOWN_PHASE_BLACKSMITH;
		return BOT_TOWN_PHASE_NONE;
	}

	BYTE GetPlayerBotFirstExteriorTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedTrainer)
			return BOT_TOWN_PHASE_TRAINER;
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
		return BOT_TOWN_PHASE_NONE;
	}

	BYTE GetPlayerBotFirstDirectTownPhase(const TPlayerBotAIState& state)
	{
		if (state.bTownNeedWeaponMerchant)
			return BOT_TOWN_PHASE_WEAPON_MERCHANT;
		if (state.bTownNeedArmorMerchant)
			return BOT_TOWN_PHASE_ARMOR_MERCHANT;
		if (state.bTownNeedMisc)
			return BOT_TOWN_PHASE_MISC_MERCHANT;
		if (state.bTownNeedBlacksmith)
			return BOT_TOWN_PHASE_BLACKSMITH;
		return BOT_TOWN_PHASE_NONE;
	}

	void StartPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return;
		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;

		state.bTownNeedTrainer = !inM2 && ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0 &&
				ch->GetJob() <= JOB_SHAMAN;
		state.bTownNeedMisc = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_MISC) ||
				NeedsPlayerBotPotions(ch) || HasPlayerBotExcessPotions(ch) ||
				NeedsPlayerBotProgressionBoots(ch);
		state.bTownNeedWeaponMerchant = HasPlayerBotJunkForMerchant(
				ch, BOT_MERCHANT_WEAPON) || ch->GetWear(WEAR_WEAPON) == NULL ||
				NeedsPlayerBotProgressionWeapon(ch) || NeedsPlayerBotArrows(ch);
		state.bTownNeedArmorMerchant = HasPlayerBotJunkForMerchant(ch, BOT_MERCHANT_ARMOR) ||
				NeedsPlayerBotProgressionArmor(ch) || NeedsPlayerBotProgressionShield(ch) ||
				NeedsPlayerBotProgressionHelmet(ch);
		state.bTownNeedBlacksmith = HasPlayerBotRefineOpportunity(ch);
		if (!state.bTownNeedTrainer && !state.bTownNeedMisc && !state.bTownNeedWeaponMerchant &&
				!state.bTownNeedArmorMerchant && !state.bTownNeedBlacksmith)
		{
			state.dwNextShopCheckTime = dwNow + number(60000, 120000);
			return;
		}

		state.bVisitingShop = true;
		if (inM2)
		{
			// Bokjung has no decorative gate split: visit only the specialists which
			// are needed and then walk straight back to the local hunting fields.
			state.bTownVisitPhase = GetPlayerBotFirstDirectTownPhase(state);
		}
		else
		{
			const bool alreadyInsideTown = ch->GetX() >= 57000 && ch->GetX() <= 63000 &&
					ch->GetY() >= 170000 && ch->GetY() <= 174000;
			if (alreadyInsideTown)
			{
				state.bTownVisitPhase = GetPlayerBotFirstInteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_OUT;
			}
			else
			{
				state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			}
		}
		state.dwTownWaitUntil = 0;
		state.dwNextShopCheckTime = dwNow + 60000;
		state.dwTargetVID = 0;
		state.bStuckCounter = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
	}

	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY)
	{
		const DWORD hash = PlayerBotNavHash(playerID ^ salt);
		const int lane = (int)(hash % 11U) - 5;
		const int row = (int)((hash / 11U) % 6U);
		approachX = npcX + lane * 90;
		approachY = npcY - 240 - row * 80;
	}

	void GivePlayerBotBiologistReward(LPCHARACTER ch,
			const TPlayerBotBiologistMission& mission)
	{
		if (!ch)
			return;

		DWORD rewardItem = 0;
		switch (mission.requiredLevel)
		{
			case 4:
				rewardItem = ch->GetJob() == JOB_SHAMAN ? 7003 : 13;
				break;
			case 7:
			{
				const DWORD armorRewards[4] = { 11203, 11403, 11603, 11803 };
				if (ch->GetJob() <= JOB_SHAMAN)
					rewardItem = armorRewards[ch->GetJob()];
				break;
			}
			case 10: rewardItem = 16023; break;
			case 15: rewardItem = 17023; break;
			case 20: rewardItem = 14023; break;
			case 25:
			{
				const DWORD helmetRewards[4] = { 12222, 12362, 12502, 12642 };
				if (ch->GetJob() <= JOB_SHAMAN)
					rewardItem = helmetRewards[ch->GetJob()];
				break;
			}
		}

		if (rewardItem != 0)
			ch->AutoGiveItem(rewardItem, 1, -1, false);
		if (mission.rewardGold > 0)
			ch->PointChange(POINT_GOLD, mission.rewardGold);
		if (mission.rewardExp > 0)
			ch->PointChange(POINT_EXP, mission.rewardExp, true);
	}

	bool CompletePlayerBotBiologistMission(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int completeState = GetPlayerBotBiologistStateIndex(missionIndex, "__complete");
		quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		if (!pc || completeState < 0)
			return false;

		GivePlayerBotBiologistReward(ch, mission);
		ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "collect_count"), 0);
		ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "drink_drug"), 0);
		pc->SetQuestState(mission.questName, completeState);
		sys_log(0, "PLAYERBOT_BIOLOGIST: mission complete pid=%u name=%s quest=%s level=%u gold=%u exp=%u",
				ch->GetPlayerID(), ch->GetName(), mission.questName, mission.requiredLevel,
				mission.rewardGold, mission.rewardExp);
		return true;
	}

	bool ManagePlayerBotBiologist(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetMapIndex() != 21 || state.bVisitingShop)
			return false;
		if (!state.bVisitingBiologist && dwNow < state.dwNextBiologistCheckTime)
			return false;
		if (!state.bVisitingBiologist)
			state.dwNextBiologistCheckTime = dwNow + 2000;

		size_t missionIndex = 0;
		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch, &missionIndex);
		if (!mission)
		{
			state.bVisitingBiologist = false;
			return false;
		}
		if (!EnsurePlayerBotBiologistMissionStarted(ch, missionIndex))
			return false;

		const int accepted = std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, (int)mission->requiredCount - accepted);
		const int carried = ch->CountSpecifyItem(mission->itemVnum);
		if (!state.bVisitingBiologist && carried < remaining)
			return false;

		if (!state.bVisitingBiologist)
		{
			state.bVisitingBiologist = true;
			state.dwNextBiologistActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_BIOLOGIST: going to NPC pid=%u name=%s quest=%s carried=%d accepted=%d/%u",
					ch->GetPlayerID(), ch->GetName(), mission->questName,
					carried, accepted, mission->requiredCount);
		}

		SetPlayerBotAction(state, BOT_ACTION_BIOLOGIST, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), PLAYERBOT_BIOLOGIST_X,
				PLAYERBOT_BIOLOGIST_Y, 0x42494f4cU, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingBiologist = false;
				state.dwNextBiologistCheckTime = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_BIOLOGIST: route failed pid=%u name=%s from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		SetPlayerBotRidingForTravel(ch, state, false, dwNow, "biologist_interaction");
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextBiologistActionTime == 0)
		{
			state.dwNextBiologistActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextBiologistActionTime)
			return true;

		if (ch->CountSpecifyItem(mission->itemVnum) <= 0)
		{
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(5000, 12000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		ch->RemoveSpecifyItem(mission->itemVnum, 1);
		const bool acceptedNow = number(1, 100) <= mission->acceptPercent;
		int newAccepted = accepted;
		if (acceptedNow)
		{
			newAccepted = accepted + 1;
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(*mission, "collect_count"), newAccepted);
		}
		sys_log(0, "PLAYERBOT_BIOLOGIST: submitted pid=%u name=%s quest=%s accepted_now=%d progress=%d/%u carried_left=%d",
				ch->GetPlayerID(), ch->GetName(), mission->questName, acceptedNow ? 1 : 0,
				newAccepted, mission->requiredCount, ch->CountSpecifyItem(mission->itemVnum));

		if (newAccepted >= mission->requiredCount &&
				CompletePlayerBotBiologistMission(ch, missionIndex))
		{
			state.bVisitingBiologist = false;
			state.dwNextBiologistActionTime = 0;
			state.dwNextBiologistCheckTime = dwNow + number(10000, 25000);
			ClearPlayerBotRoute(state, true);
			return false;
		}

		state.dwNextBiologistActionTime = dwNow + number(2500, 5000);
		return true;
	}

	void FinishPlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			bool completed)
	{
		state.bVisitingShop = false;
		state.bTownNeedMisc = false;
		state.bTownNeedWeaponMerchant = false;
		state.bTownNeedArmorMerchant = false;
		state.bTownNeedBlacksmith = false;
		state.bTownNeedTrainer = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.dwTownWaitUntil = 0;
		state.dwNextShopCheckTime = dwNow +
			(completed ? number(300000, 600000) : number(60000, 120000));
		state.dwTargetVID = 0;
		state.bStuckCounter = 0;
		if (ch)
		{
			ch->SetVictim(NULL);
			ch->Stop();
			ch->SetPosition(POS_STANDING);
		}
		ClearPlayerBotRoute(state, true);
	}

	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance);

	// A stable tenth of the population runs a market stall - always the same
	// bots, so the market does not move around between restarts. Keeping a shop
	// means not hunting, which is why it stays a minority; and since a keeper
	// only opens when it happens to be in Bokjung with no errand outstanding,
	// the share actually standing at any moment is smaller again.
	bool ShouldPlayerBotKeepShop(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 20)
			return false;
		return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x53484f50U) % 10U) == 0;
	}

	// The first inventory item the bot can legitimately part with. OpenMyShop
	// refuses equipped, locked and ANTI_GIVE/ANTI_MYSHOP items outright, so the
	// same rules are applied here rather than letting the call fail silently.
	LPITEM FindPlayerBotShopItem(LPCHARACTER ch, WORD& cellOut)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const TItemTable* proto = item->GetProto();
			if (!proto || IS_SET(proto->dwAntiFlags,
					ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
				continue;
			// Not the vendor-trash rule: a stall should carry something a player
			// might actually want. Materials and spare loot qualify; the bot's own
			// supplies, weapons and armour do not, so it can never sell the gear
			// or the potions it needs to keep playing.
			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051 ||
					vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				continue;
			if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM || (vnum >= 50701 && vnum <= 50706))
				continue;
			// Spare gear is the most interesting thing a stall can offer, but the
			// bot must never put up the only weapon or armour it owns for a slot
			// it is still walking around empty. Something already worn there means
			// what it carries is genuinely a spare.
			const BYTE type = item->GetType();
			if (type == ITEM_WEAPON || type == ITEM_ARMOR)
			{
				const int wearCell = item->FindEquipCell(ch);
				if (wearCell < 0 || ch->GetWear((BYTE)wearCell) == NULL)
					continue;
			}
			cellOut = cell;
			return item;
		}
		return NULL;
	}

	// A good refine is the one moment worth breaking the bots' silence for. They
	// say nothing when attacked, nothing during PvP, and nothing on a kill -
	// only the blacksmith gets a reaction, and even then rarely.
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, LPITEM item, int newPlus)
	{
		if (!ch || !item || newPlus < 7)
			return;

		static DWORD s_dwLastShoutTime = 0;
		const DWORD dwNow = get_dword_time();
		// One announcement every few minutes for the whole world: the chat should
		// feel inhabited, not flooded.
		if (s_dwLastShoutTime != 0 && dwNow < s_dwLastShoutTime + 180000)
			return;
		if (number(1, 100) > 45)
			return;

		static const char* kPlus7[] = {
			"%s poszedl na +7, kowal dzis laskawy",
			"no i mam +7 na %s, moglo byc gorzej",
			"+7 na %s siadlo za pierwszym razem",
			"udalo sie, %s na +7"
		};
		static const char* kPlus8[] = {
			"%s na +8! rece mi sie trzesly",
			"jest +8 na %s, teraz sie zastanawiam czy pchac dalej",
			"+8 na %s, chyba mam dzis szczescie",
			"weszlo na +8, %s gotowy do roboty"
		};
		static const char* kPlus9[] = {
			"%s NA +9!!! nie wierze",
			"+9 na %s, kto by pomyslal",
			"dziewiatka na %s, dzis stawiam :D",
			"%s +9, chyba wystarczy tych probek na dzis"
		};

		const char** pool = kPlus7;
		if (newPlus >= 9)
			pool = kPlus9;
		else if (newPlus == 8)
			pool = kPlus8;

		char msg[CHAT_MAX_LEN + 1];
		char body[CHAT_MAX_LEN + 1];
		snprintf(body, sizeof(body), pool[number(0, 3)], item->GetName());
		snprintf(msg, sizeof(msg), "%s : %s", ch->GetName(), body);

		s_dwLastShoutTime = dwNow;
		SendShout(msg, ch->GetEmpire());
		sys_log(0, "PLAYERBOT_SHOUT: pid=%u plus=%d text=%s",
				ch->GetPlayerID(), newPlus, msg);
	}

	// Where a bot belongs on each map we manage: the point that map is entered
	// by, and Bokjung for anything else.
	void GetPlayerBotHomePoint(long mapIndex, long& outMap, long& outX, long& outY)
	{
		outMap = PLAYERBOT_MAP_CHUNJO_M2;
		outX = PLAYERBOT_M2_FROM_M3_X;
		outY = PLAYERBOT_M2_FROM_M3_Y;
		switch (mapIndex)
		{
			case PLAYERBOT_MAP_CHUNJO_M1:
				outMap = mapIndex; outX = PLAYERBOT_M1_RETURN_X; outY = PLAYERBOT_M1_RETURN_Y; break;
			case PLAYERBOT_MAP_CHUNJO_M2:
				outMap = mapIndex; outX = PLAYERBOT_M2_ARRIVAL_X; outY = PLAYERBOT_M2_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_CHUNJO_M3:
				outMap = mapIndex; outX = PLAYERBOT_M3_ARRIVAL_X; outY = PLAYERBOT_M3_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_MONKEY_EASY:
				outMap = mapIndex; outX = PLAYERBOT_MONKEY_EASY_ARRIVAL_X; outY = PLAYERBOT_MONKEY_EASY_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_ORC_VALLEY:
				outMap = mapIndex; outX = PLAYERBOT_ORC_VALLEY_ARRIVAL_X; outY = PLAYERBOT_ORC_VALLEY_ARRIVAL_Y; break;
			case PLAYERBOT_MAP_DESERT:
				outMap = mapIndex; outX = PLAYERBOT_DESERT_ARRIVAL_X; outY = PLAYERBOT_DESERT_ARRIVAL_Y; break;
			default: break;
		}
	}

	// A character with no sector, or one standing on a map this core does not
	// host, cannot move at all - and nothing else in the tick can put it back.
	// It is asked again on the next tick and answers the same way, for as long as
	// the server runs: a day of logs held 35k such lines from 45 bots that never
	// took another step, plus the watchdog resetting them 8k times to no effect.
	bool RescuePlayerBotWithoutSectree(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		const long mapIndex = ch->GetMapIndex();
		if (ch->GetSectree() && SECTREE_MANAGER::instance().GetMap(mapIndex) != NULL)
			return false;
		if (state.dwNextSectreeRescueTime != 0 && dwNow < state.dwNextSectreeRescueTime)
			return true; // already tried recently; do not spin
		state.dwNextSectreeRescueTime = dwNow + 30000;

		long homeMap = 0, homeX = 0, homeY = 0;
		GetPlayerBotHomePoint(mapIndex, homeMap, homeX, homeY);
		if (TransitionPlayerBotMap(ch, state, homeMap, homeX, homeY, dwNow, "sectree_rescue"))
		{
			sys_log(0, "PLAYERBOT_RESCUE: pid=%u name=%s had no sectree on map=%ld, moved to map=%ld (%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), mapIndex, homeMap, homeX, homeY);
		}
		return true;
	}

	// A stall is engine state; the deadline that ends it is AI state. Keeping the
	// two in step is the whole job of this pair of helpers, and doing it from a
	// single place is what makes it possible to run the release *before* the
	// subsystems that can claim the tick.
	void ClosePlayerBotShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			const char* reason)
	{
		if (!ch)
			return;
		const bool bHadShop = ch->GetMyShop() != NULL;
		if (bHadShop)
			ch->CloseMyShop();
		state.dwShopOpenedTime = 0;
		state.dwShopCloseTime = 0;
		state.dwNextShopKeepTime = dwNow +
				number(PLAYERBOT_SHOP_REST_MIN, PLAYERBOT_SHOP_REST_MAX);
		if (bHadShop)
			sys_log(0, "PLAYERBOT_SHOP: closed pid=%u name=%s reason=%s",
					ch->GetPlayerID(), ch->GetName(), reason);
	}

	// Runs at the very top of the tick, ahead of the inactivity watchdog and the
	// navigation rescues. Those both "continue", and a keeper that never reached
	// the shop hook could not close its stall: the sign stayed over its head and
	// a rescue was free to teleport it out of the market still wearing it. The
	// stall now outranks them - releasing engine state is not something a bot may
	// be interrupted out of.
	bool ManagePlayerBotShopLifetime(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow)
	{
		if (!ch || !ch->GetMyShop())
			return false;

		// A stall with no deadline can never expire. The shop lives in the engine
		// and the deadline in the AI state, so any path that loses one without the
		// other used to strand the keeper trading forever; give it one instead.
		if (state.dwShopCloseTime == 0)
			state.dwShopCloseTime =
					(state.dwShopOpenedTime != 0 ? state.dwShopOpenedTime : dwNow) +
					PLAYERBOT_SHOP_MIN_DURATION;

		// Whatever else happens, a corpse or a bot that is no longer standing on
		// the market strip has no business still holding a stall.
		const bool bOffPitch = ch->IsDead() ||
				ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2 ||
				DISTANCE_APPROX(ch->GetX() - PLAYERBOT_M2_MARKET_X,
						ch->GetY() - PLAYERBOT_M2_MARKET_Y) >
					PLAYERBOT_MARKET_SPREAD + PLAYERBOT_MARKET_ARRIVE * 2;

		if (dwNow < state.dwShopCloseTime && !bOffPitch)
		{
			// Standing at a stall is the activity, not the absence of one. Without
			// this the 90-second watchdog fired on every keeper, once per stall.
			state.dwLastMeaningfulActivityTime = dwNow;
			state.lLastX = ch->GetX();
			state.lLastY = ch->GetY();
			return true;
		}

		ClosePlayerBotShop(ch, state, dwNow, bOffPitch ? "off_pitch" : "expired");
		return false;
	}

	bool ManagePlayerBotPrivateShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		// The stall's own lifetime is settled earlier in the tick; by the time
		// this runs a keeper either has no shop or has already been held there.
		if (ch->GetMyShop())
			return true;

		if (!ShouldPlayerBotKeepShop(ch))
			return false;
		if (ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2)
			return false;

		// Errands still come first - a stall opened mid-visit would be abandoned
		// on the next tick.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable)
			return false;
		if (state.dwNextShopKeepTime != 0 && dwNow < state.dwNextShopKeepTime)
			return false;
		// ...but "in town with nothing to do" is a state that barely exists: a bot
		// comes to Bokjung *because* it has an errand, and leaves the moment the
		// errand is done. The stall therefore opens right after a completed town
		// visit, while the bot is still standing in the village, instead of waiting
		// for an idle moment that never arrives.
		const bool justFinishedInTown = state.dwNextShopCheckTime != 0 &&
				dwNow < state.dwNextShopCheckTime;
		// A keeper already standing on the market strip counts too. A server
		// restart drops every shop - they live only in memory - and leaves its
		// keeper parked exactly where the stall was, with no errand to bring it
		// back to town and therefore no way to ever reopen.
		const bool alreadyAtPitch = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 &&
				DISTANCE_APPROX(ch->GetX() - PLAYERBOT_M2_MARKET_X,
						ch->GetY() - PLAYERBOT_M2_MARKET_Y) <=
					PLAYERBOT_MARKET_SPREAD + PLAYERBOT_MARKET_ARRIVE;
		if (!justFinishedInTown && !alreadyAtPitch)
			return false;

		WORD cell = 0;
		LPITEM item = FindPlayerBotShopItem(ch, cell);
		if (!item)
		{
			// Nothing worth a stall right now; look again after a hunt rather than
			// re-scanning the whole inventory every tick.
			state.dwNextShopKeepTime = dwNow + number(300000, 600000);
			sys_log(0, "PLAYERBOT_SHOP: nothing to sell pid=%u name=%s",
					ch->GetPlayerID(), ch->GetName());
			return false;
		}

		long offsetX = 0, offsetY = 0;
		GetPlayerBotStableOffset(ch->GetPlayerID(), 0x4d4b5450U,
				120, PLAYERBOT_MARKET_SPREAD, offsetX, offsetY);
		const long stallX = PLAYERBOT_M2_MARKET_X + offsetX;
		const long stallY = PLAYERBOT_M2_MARKET_Y + offsetY;

		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (!MovePlayerBotTownLeg(ch, state, dwNow, stallX, stallY,
				PLAYERBOT_MARKET_ARRIVE))
			return true; // still walking to the pitch

		// OpenMyShop refuses a character whose main part is not its own body, so
		// the horse has to go before the stall can be set up.
		if (ch->IsRiding())
			ch->StopRiding();
		ch->HorseSummon(false);
		ch->SetVictim(NULL);
		ch->Stop();

		const DWORD unit = GetPlayerBotNpcSellUnitPrice(item);
		DWORD price = unit * (DWORD)item->GetCount() * 3U;
		if (price == 0)
			price = 1;

		TShopItemTable table;
		memset(&table, 0, sizeof(table));
		table.vnum = item->GetVnum();
		table.count = item->GetCount();
		table.pos = TItemPos(INVENTORY, cell);
		table.price = price;
		table.display_pos = 0;

		char sign[SHOP_SIGN_MAX_LEN + 1];
		snprintf(sign, sizeof(sign), "%s", ch->GetName());

		// Opening a stall costs a shop bundle, exactly as it does for a player:
		// OpenMyShop consumes one 50200 and refuses outright without it. The other
		// accepted item, the permanent 71049, takes a branch that writes through
		// GetDesc() - a bot has no client descriptor, so that path must be avoided.
		if (ch->CountSpecifyItem(71049) > 0)
			return false;
		if (ch->CountSpecifyItem(50200) == 0)
		{
			// The bot buys its stall like anything else it carries.
			if (ch->GetGold() >= PLAYERBOT_SHOP_BUNDLE_PRICE)
				ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_SHOP_BUNDLE_PRICE);
			ch->AutoGiveItem(50200, 1);
		}

		ch->OpenMyShop(sign, &table, 1);
		if (!ch->GetMyShop())
		{
			// OpenMyShop refuses silently. Report which of its own guards said no,
			// otherwise this is indistinguishable from the stall never being tried.
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			const TItemTable* rp = item->GetProto();
			LPITEM viaPos = ch->GetItem(table.pos);
			sys_log(0, "PLAYERBOT_SHOP: refused pid=%u poly=%d quest=%d vnum=%u anti=%u equipped=%d locked=%d viaPos=%d sign=%d gold=%d",
					ch->GetPlayerID(), ch->IsPolymorphed() ? 1 : 0,
					(pc && pc->IsRunning()) ? 1 : 0, item->GetVnum(),
					rp ? rp->dwAntiFlags : 0, item->IsEquipped() ? 1 : 0,
					item->isLocked() ? 1 : 0, viaPos ? 1 : 0,
					(int)strlen(sign), (int)(ch->GetGold() / 1000));
			state.dwNextShopKeepTime = dwNow + number(60000, 180000);
			return false;
		}

		state.dwShopOpenedTime = dwNow;
		state.dwShopCloseTime = dwNow +
				number(PLAYERBOT_SHOP_MIN_DURATION, PLAYERBOT_SHOP_MAX_DURATION);
		sys_log(0, "PLAYERBOT_SHOP: opened pid=%u name=%s vnum=%u count=%d price=%u pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
				(int)item->GetCount(), price, ch->GetX(), ch->GetY());
		return true;
	}

	bool MovePlayerBotTownLeg(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			long goalX, long goalY, int arrivalDistance)
	{
		if (DISTANCE_APPROX(ch->GetX() - goalX, ch->GetY() - goalY) <= arrivalDistance)
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "town_interaction");
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			ClearPlayerBotRoute(state, true);
			return true;
		}

		// NPCs and gateposts occupy ATTR_OBJECT cells.  A strict four-cell snap can
		// therefore reject a perfectly valid visit when the chosen waiting spot is
		// on the other side of a counter, pillar or another dynamic object.  Town
		// legs may finish at the nearest point of the bot's own walkable component;
		// the larger arrival radii below represent the normal interaction area.
		const bool moveAccepted = MovePlayerBot(ch, goalX, goalY, dwNow, 16, true, true);
		if (!moveAccepted && state.bStuckCounter >= 6)
		{
			sys_err("PLAYERBOT_TOWN: route failed pid=%u name=%s phase=%u from=(%ld,%ld) to=(%ld,%ld)",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
					ch->GetX(), ch->GetY(), goalX, goalY);

			// A handful of decorative town objects are disconnected in server_attr
			// even though the native client can run around them.  Retrying the same
			// component forever left a bot permanently unable to sell.  After six
			// independently planned failures, relocate once to the nearest verified
			// walkable service cell and let the normal arrival/wait phase continue.
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
			PIXEL_POSITION safe;
			if (navigation.Init(ch->GetMapIndex()) &&
					navigation.FindNearestWalkableWorld(goalX, goalY, 16, safe,
							ch->GetPlayerID() ^ (DWORD)state.bTownVisitPhase))
			{
				ClearPlayerBotRoute(state, true);
				state.bStuckCounter = 0;
				ch->Show(ch->GetMapIndex(), safe.x, safe.y, 0);
				ch->Stop();
				ch->SendMovePacket(FUNC_MOVE, 0, safe.x, safe.y, 0, dwNow);
				sys_err("PLAYERBOT_TOWN: service rescue pid=%u name=%s phase=%u to=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bTownVisitPhase,
						safe.x, safe.y);
			}
			else
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
		}
		return false;
	}

	bool MovePlayerBotAcrossTownGate(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, long goalY)
	{
		if (!ch)
			return false;
		const int distance = DISTANCE_APPROX(
				ch->GetX() - PLAYERBOT_TOWN_GATE_X, ch->GetY() - goalY);
		if (distance <= 450)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			ClearPlayerBotRoute(state, true);
			return true;
		}

		// server_attr separates the two sides of Joan's decorative gate into
		// different components even though players can run through its opening.
		// This one verified 5.75 m segment is therefore issued directly, while all
		// ordinary navigation remains collision-aware. It replaces endless A*
		// retries at (603,675) with the same straight run a real player performs.
		if (distance <= 1200)
		{
			ClearPlayerBotRoute(state, true);
			ch->SetRotationToXY(PLAYERBOT_TOWN_GATE_X, goalY);
			if (ch->Goto(PLAYERBOT_TOWN_GATE_X, goalY))
			{
				ch->SendMovePacket(FUNC_MOVE, 0, PLAYERBOT_TOWN_GATE_X, goalY,
						ch->GetCurrentMoveDuration(), dwNow);
				return false;
			}
		}

		return MovePlayerBotTownLeg(ch, state, dwNow,
				PLAYERBOT_TOWN_GATE_X, goalY, 450);
	}

	bool HandlePlayerBotTownVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !state.bVisitingShop ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return false;
		const bool inM2 = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;
		const long weaponNpcX = inM2 ? PLAYERBOT_M2_WEAPON_MERCHANT_X : PLAYERBOT_WEAPON_MERCHANT_X;
		const long weaponNpcY = inM2 ? PLAYERBOT_M2_WEAPON_MERCHANT_Y : PLAYERBOT_WEAPON_MERCHANT_Y;
		const long armorNpcX = inM2 ? PLAYERBOT_M2_ARMOR_MERCHANT_X : PLAYERBOT_ARMOR_MERCHANT_X;
		const long armorNpcY = inM2 ? PLAYERBOT_M2_ARMOR_MERCHANT_Y : PLAYERBOT_ARMOR_MERCHANT_Y;
		const long miscNpcX = inM2 ? PLAYERBOT_M2_MISC_MERCHANT_X : PLAYERBOT_MISC_MERCHANT_X;
		const long miscNpcY = inM2 ? PLAYERBOT_M2_MISC_MERCHANT_Y : PLAYERBOT_MISC_MERCHANT_Y;
		const long blacksmithNpcX = inM2 ? PLAYERBOT_M2_BLACKSMITH_X : PLAYERBOT_BLACKSMITH_X;
		const long blacksmithNpcY = inM2 ? PLAYERBOT_M2_BLACKSMITH_Y : PLAYERBOT_BLACKSMITH_Y;

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_REFINE, dwNow);
		else if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_WAIT ||
				state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_WAIT)
			SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		else
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
		{
			state.bTownVisitPhase = inM2
					? GetPlayerBotFirstDirectTownPhase(state)
					: GetPlayerBotFirstExteriorTownPhase(state);
			if (!inM2 && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
			if (inM2 && state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, true);
				return true;
			}
		}

		// Eight profession trainers stand south of Joan.  Their npc.txt cells are
		// 623/627 (Warrior), 631/635 (Ninja), 645/649 (Sura), 653/657
		// (Shaman); the second coordinate includes map 21's 102400 Y base.
		const BYTE wantedGroup = (ch->GetPlayerID() % 2 == 0) ? 1 : 2;
		const long trainerGroup1X[4] = { 62300, 63100, 64500, 65300 };
		const long trainerGroup2X[4] = { 62700, 63500, 64900, 65700 };
		const BYTE trainerJob = std::min<BYTE>(ch->GetJob(), JOB_SHAMAN);
		const long trainerNpcX = wantedGroup == 1
				? trainerGroup1X[trainerJob] : trainerGroup2X[trainerJob];
		const long trainerNpcY = ch->GetJob() <= JOB_WARRIOR ? 161800 : 161900;
		long trainerX = 0, trainerY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), trainerNpcX, trainerNpcY,
				0x54524149U, trainerX, trainerY);

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			if (MovePlayerBotTownLeg(ch, state, dwNow, trainerX, trainerY, 550))
			{
				if (ChoosePlayerBotSkillGroup(ch))
					state.bTownNeedTrainer = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_TRAINER_WAIT_MIN, PLAYERBOT_TRAINER_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_TRAINER_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: trainer visit pid=%u name=%s job=%u group=%u wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetJob(), ch->GetSkillGroup(),
						state.dwTownWaitUntil - dwNow, ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_TRAINER_WAIT)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAIN, dwNow);
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedWeaponMerchant
						? BOT_TOWN_PHASE_WEAPON_MERCHANT
						: (state.bTownNeedArmorMerchant ? BOT_TOWN_PHASE_ARMOR_MERCHANT
							: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
								? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE));
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long weaponMerchantX = 0, weaponMerchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), weaponNpcX,
				weaponNpcY, 0x57454150U, weaponMerchantX, weaponMerchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					weaponMerchantX, weaponMerchantY, 850))
			{
				ManagePlayerBotWeaponMerchant(ch);
				ManagePlayerBotEquipment(ch, state, dwNow);
				if (!ch->GetWear(WEAR_WEAPON))
				{
					// Nothing sellable was sufficient. Leave the counter after this
					// visit and search nearby hunting fields for ownerless Yang/gear.
					state.dwEmergencyScavengeUntil = dwNow + 120000;
					sys_log(0, "PLAYERBOT_GEAR: emergency scavenging armed pid=%u name=%s until=%u gold=%lld",
							ch->GetPlayerID(), ch->GetName(), state.dwEmergencyScavengeUntil,
							(long long)ch->GetGold());
				}
				state.bTownNeedBlacksmith = state.bTownNeedBlacksmith ||
						HasPlayerBotRefineOpportunity(ch);
				state.bTownNeedWeaponMerchant = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_WEAPON_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: weapon merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_WEAPON_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedArmorMerchant
						? BOT_TOWN_PHASE_ARMOR_MERCHANT
						: (inM2 ? GetPlayerBotFirstDirectTownPhase(state)
							: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
								? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE));
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long armorMerchantX = 0, armorMerchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), armorNpcX,
				armorNpcY, 0x41524d52U, armorMerchantX, armorMerchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					armorMerchantX, armorMerchantY, 850))
			{
				ManagePlayerBotArmorMerchant(ch);
				ManagePlayerBotEquipment(ch, state, dwNow);
				state.bTownNeedBlacksmith = state.bTownNeedBlacksmith ||
						HasPlayerBotRefineOpportunity(ch);
				state.bTownNeedArmorMerchant = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_ARMOR_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: armor merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_ARMOR_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = inM2
						? GetPlayerBotFirstDirectTownPhase(state)
						: ((state.bTownNeedMisc || state.bTownNeedBlacksmith)
							? BOT_TOWN_PHASE_GATE_IN : BOT_TOWN_PHASE_NONE);
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_IN)
		{
			if (inM2)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
				return true;
			}
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_X, PLAYERBOT_TOWN_GATE_OUTSIDE_Y, 1000))
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_CROSS_IN;
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_CROSS_IN)
		{
			if (MovePlayerBotAcrossTownGate(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_INSIDE_Y))
			{
				state.bTownVisitPhase = GetPlayerBotFirstInteriorTownPhase(state);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_OUT;
			}
			return true;
		}

		long merchantX = 0, merchantY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), miscNpcX,
				miscNpcY, 0x4d495343U, merchantX, merchantY);
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_MERCHANT)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow, merchantX, merchantY, 650))
			{
				ManagePlayerBotMiscMerchant(ch);
				state.bTownNeedMisc = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_MERCHANT_WAIT_MIN, PLAYERBOT_MERCHANT_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_MISC_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: misc merchant visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_MISC_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			if (dwNow >= state.dwTownWaitUntil)
			{
				state.bTownVisitPhase = state.bTownNeedBlacksmith
						? BOT_TOWN_PHASE_BLACKSMITH
						: (inM2 ? BOT_TOWN_PHASE_NONE : BOT_TOWN_PHASE_GATE_OUT);
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		long blacksmithX = 0, blacksmithY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), blacksmithNpcX,
				blacksmithNpcY, 0x4b4f574cU, blacksmithX, blacksmithY);
		// Keep the per-PID spread, but halve it specifically at the blacksmith.
		// Together with the tighter arrival radius this keeps every refiner close
		// enough to look like it is actually interacting with the NPC.
		blacksmithX = blacksmithNpcX + (blacksmithX - blacksmithNpcX) / 2;
		blacksmithY = blacksmithNpcY + (blacksmithY - blacksmithNpcY) / 2;
		if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH)
		{
			if (MovePlayerBotTownLeg(ch, state, dwNow, blacksmithX, blacksmithY, 500))
			{
				ManagePlayerBotRefining(ch, state, dwNow);
				// The blacksmith is where a player rerolls bonus lines too, and the
				// bot is already standing still there for six to twenty-four seconds.
				ManagePlayerBotBonusReroll(ch, state, dwNow);
				if (!HasPlayerBotRefineOpportunity(ch))
					RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
				state.bTownNeedBlacksmith = false;
				state.dwTownWaitUntil = dwNow + number(
						PLAYERBOT_BLACKSMITH_WAIT_MIN, PLAYERBOT_BLACKSMITH_WAIT_MAX);
				state.bTownVisitPhase = BOT_TOWN_PHASE_BLACKSMITH_WAIT;
				sys_log(0, "PLAYERBOT_TOWN: blacksmith visit pid=%u name=%s wait_ms=%u pos=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), state.dwTownWaitUntil - dwNow,
						ch->GetX(), ch->GetY());
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_BLACKSMITH_WAIT)
		{
			ch->Stop();
			ch->SetPosition(POS_STANDING);
			// Use the time spent at the NPC like a real player: make further regular
			// refine attempts instead of clicking only once and idling.  This cadence
			// never extends dwTownWaitUntil; the visit has one absolute 6-24 s limit.
			ManagePlayerBotRefining(ch, state, dwNow);
			ManagePlayerBotBonusReroll(ch, state, dwNow);
			if (!HasPlayerBotRefineOpportunity(ch))
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
			if (dwNow >= state.dwTownWaitUntil)
			{
				// Always leave the NPC wearing the best surviving/refined equipment,
				// even if materials, Yang or a failed roll ended the session early.
				RestorePlayerBotEquipmentAfterRefining(ch, state, dwNow);
				state.bTownVisitPhase = inM2
						? BOT_TOWN_PHASE_NONE : BOT_TOWN_PHASE_GATE_OUT;
				state.dwTownWaitUntil = 0;
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_OUT)
		{
			if (inM2)
			{
				FinishPlayerBotTownVisit(ch, state, dwNow, false);
				return true;
			}
			if (MovePlayerBotTownLeg(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_X, PLAYERBOT_TOWN_GATE_INSIDE_Y, 1000))
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_CROSS_OUT;
			return true;
		}

		if (state.bTownVisitPhase == BOT_TOWN_PHASE_GATE_CROSS_OUT)
		{
			if (MovePlayerBotAcrossTownGate(ch, state, dwNow,
					PLAYERBOT_TOWN_GATE_OUTSIDE_Y))
			{
				state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
				ClearPlayerBotRoute(state, true);
				if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
					FinishPlayerBotTownVisit(ch, state, dwNow, true);
			}
			return true;
		}

		FinishPlayerBotTownVisit(ch, state, dwNow, false);
		return true;
	}

	void ManagePlayerBotParty(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() || dwNow < state.dwNextPartyCheckTime)
			return;

		state.dwNextPartyCheckTime = dwNow + PLAYERBOT_PARTY_CHECK_INTERVAL + number(0, 3000);

		LPPARTY pParty = ch->GetParty();
		// Party play is an explicit, deterministic cohort. Archer weighting is
		// decided at login, while the total cohort remains close to ten percent.
		if (state.bBotRole != BOT_ROLE_PARTY_FIGHTER)
		{
			if (pParty)
			{
				pParty->Quit(ch->GetPlayerID());
				sys_log(0, "PLAYERBOT_AI: left party outside party cohort pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			state.dwPartyExpireTime = 0;
			state.dwNextPartyCheckTime = dwNow + number(60000, 180000);
			return;
		}

		if (pParty)
		{
			// Check if party duration expired (dynamic rotation: 5-15 mins)
			if (state.dwPartyExpireTime != 0 && dwNow >= state.dwPartyExpireTime)
			{
				state.dwPartyExpireTime = 0;
				state.dwNextPartyCheckTime = dwNow + number(60000, 180000); // 1-3 min solo before new party
				pParty->Quit(ch->GetPlayerID());
				sys_log(0, "PLAYERBOT_AI: left party after time expired (dynamic rotation) pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
				return;
			}

			LPCHARACTER leader = pParty->GetLeaderCharacter();
			if (leader && leader != ch)
			{
				// A party is one local hunting formation, not a database label shared
				// by bots in separate sectors of the map.
				int levelDelta = abs((int)ch->GetLevel() - (int)leader->GetLevel());
				int distToLeader = DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY());
				if (levelDelta > 6 || leader->GetMapIndex() != ch->GetMapIndex() ||
						distToLeader > PLAYERBOT_PARTY_COHESION_RADIUS)
				{
					pParty->Quit(ch->GetPlayerID());
					state.dwNextPartyCheckTime = dwNow + number(30000, 90000);
					sys_log(0, "PLAYERBOT_AI: left party due to distance/level delta pid=%u name=%s leader_pid=%u dist=%d delta=%d",
							ch->GetPlayerID(), ch->GetName(), leader->GetPlayerID(), distToLeader, levelDelta);
					return;
				}
			}

			// Always enforce equal exp distribution
			if (pParty->GetExpDistributionMode() != PARTY_EXP_DISTRIBUTION_PARITY)
				pParty->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			return;
		}

		// 25% chance for a bot to prefer playing solo for a while
		if (number(1, 100) <= 25)
		{
			state.dwNextPartyCheckTime = dwNow + number(60000, 180000);
			return;
		}

		// Find a nearby bot with an open party or start one
		struct TPartyFinder
		{
			TPartyFinder(LPCHARACTER me) : m_me(me), m_pTargetParty(NULL), m_pSoloCandidate(NULL) {}
			bool operator()(LPENTITY ent)
			{
				if (!ent || !ent->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(ent);
				if (candidate == m_me || candidate->IsMonster() || candidate->IsStone() || candidate->IsDead())
					return false;

				if (candidate->GetDesc() && candidate->GetDesc()->IsBot())
				{
					TPlayerBotAIStateMap::const_iterator stateIt =
							s_mapPlayerBotAIStates.find(candidate->GetPlayerID());
					if (stateIt == s_mapPlayerBotAIStates.end() ||
							stateIt->second.bBotRole != BOT_ROLE_PARTY_FIGHTER)
						return true;

					if (abs((int)candidate->GetLevel() - (int)m_me->GetLevel()) > 3)
						return true;

					const int d = DISTANCE_APPROX(m_me->GetX() - candidate->GetX(), m_me->GetY() - candidate->GetY());
					if (d > 1800)
						return true;

					if (!IsPlayerBotPathClear(m_me->GetMapIndex(), m_me->GetX(), m_me->GetY(), candidate->GetX(), candidate->GetY()))
						return true;

					LPPARTY cp = candidate->GetParty();
					if (cp && cp->GetMemberCount() < PLAYERBOT_PARTY_DESIRED_MAX)
					{
						LPCHARACTER leader = cp->GetLeaderCharacter();
						if (leader && leader->GetMapIndex() == m_me->GetMapIndex())
						{
							int ld = DISTANCE_APPROX(m_me->GetX() - leader->GetX(), m_me->GetY() - leader->GetY());
							if (ld <= 1800 &&
									IsPlayerBotPartyCohesive(candidate, 2,
										PLAYERBOT_PARTY_COHESION_RADIUS) &&
									IsPlayerBotPathClear(m_me->GetMapIndex(), m_me->GetX(), m_me->GetY(), leader->GetX(), leader->GetY()))
							{
								m_pTargetParty = cp;
								return false; // Found existing local party with nearby leader
							}
						}
					}
					else if (!cp && !m_pSoloCandidate)
					{
						m_pSoloCandidate = candidate;
					}
				}
				return true;
			}
			LPCHARACTER m_me;
			LPPARTY m_pTargetParty;
			LPCHARACTER m_pSoloCandidate;
		};

		TPartyFinder finder(ch);
		ch->GetSectree()->ForEachAround(finder);

		if (finder.m_pTargetParty)
		{
			finder.m_pTargetParty->Join(ch->GetPlayerID());
			finder.m_pTargetParty->Link(ch);
			finder.m_pTargetParty->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			state.dwPartyExpireTime = dwNow + number(300000, 900000); // 5 to 15 mins
			sys_log(0, "PLAYERBOT_AI: joined party pid=%u name=%s members=%d",
					ch->GetPlayerID(), ch->GetName(), finder.m_pTargetParty->GetMemberCount());
		}
		else if (finder.m_pSoloCandidate)
		{
			LPPARTY newParty = CPartyManager::instance().CreateParty(ch);
			if (newParty)
			{
				newParty->Link(ch);
				newParty->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
				newParty->Join(finder.m_pSoloCandidate->GetPlayerID());
				newParty->Link(finder.m_pSoloCandidate);
				state.dwPartyExpireTime = dwNow + number(300000, 900000); // 5 to 15 mins
				sys_log(0, "PLAYERBOT_AI: created party pid=%u name=%s partner_pid=%u",
						ch->GetPlayerID(), ch->GetName(), finder.m_pSoloCandidate->GetPlayerID());
			}
		}
	}

	void ManagePlayerBotWandering(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		SetPlayerBotAction(state, ch->GetParty() ? BOT_ACTION_PARTY_ASSEMBLE : BOT_ACTION_TRAVEL, dwNow);

		// Party following is an active movement intent, not a new wander decision.
		// Refresh it on every AI update so followers do not stop for 8-12 seconds
		// between short route segments.
		if (ch->GetParty())
		{
			LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
			if (leader && leader != ch && leader->GetMapIndex() == ch->GetMapIndex())
			{
				int distToLeader = DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY());
				if (distToLeader > 500)
				{
					const int formAngle = (int)((ch->GetPlayerID() * 73) % 360);
					float fx = 0.0f, fy = 0.0f;
					const int formRadius = 250 + (int)(PlayerBotNavHash(ch->GetPlayerID()) % 201U);
					GetDeltaByDegree((float)formAngle, (float)formRadius, &fx, &fy);
					long targetX = leader->GetX() + (long)fx;
					long targetY = leader->GetY() + (long)fy;
					MovePlayerBot(ch, targetX, targetY, dwNow, 16, true);
					state.dwNextWanderTime = dwNow + 1000;
					return;
				}
			}
		}

		// Goto only carries the character to the currently issued waypoint.  An
		// existing multi-segment route must therefore be advanced every AI tick;
		// the wander timer controls choosing a new destination, not following an
		// already chosen one.
		if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size() &&
				state.lRouteMapIndex == ch->GetMapIndex())
		{
			MovePlayerBot(ch, state.lRouteDestX, state.lRouteDestY, dwNow, 32, true);
			return;
		}

		if (dwNow < state.dwNextWanderTime)
			return;

		state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 4000);

		// Define known hunting sectors by map
		long targetX = ch->GetX();
		long targetY = ch->GetY();

		if (ch->GetMapIndex() == 21) // Chunjo M1 (Joan)
		{
			const DWORD pid = ch->GetPlayerID();

			// 1. Role: Metin breaker (25% of bots). These are the in-bounds
			// centres from metin2_map_b1/stone.txt, converted to world coordinates.
			// Four legacy Gemini entries near the southern map edge were manually
			// shifted from stone rows whose centres lie beyond this map's Y limit;
			// runtime attr checks confirmed that the shifted points were blocked.
			if (state.bBotRole == BOT_ROLE_METIN_HUNTER)
			{
				LPCHARACTER knownMetin = FindKnownPlayerBotMetin(ch, dwNow);
				if (knownMetin &&
						DISTANCE_APPROX(ch->GetX() - knownMetin->GetX(), ch->GetY() - knownMetin->GetY()) >
						800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = knownMetin->GetX();
					targetY = knownMetin->GetY();
					if (!MovePlayerBot(ch, targetX, targetY, dwNow, 32, true) && state.bStuckCounter >= 3)
					{
						s_mapKnownPlayerBotMetins.erase(knownMetin->GetVID());
						ClearPlayerBotRoute(state, true);
					}
					return;
				}

				BYTE hIdx = ChoosePlayerBotMetinHotspot(pid, state.uMetinHotspotIndex, dwNow);
				long hx = PLAYERBOT_METIN_HOTSPOTS[hIdx].x;
				long hy = PLAYERBOT_METIN_HOTSPOTS[hIdx].y;
				long hotspotOffsetX = 0, hotspotOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x4d455449U + hIdx, 100, 650,
						hotspotOffsetX, hotspotOffsetY);
				hx += hotspotOffsetX;
				hy += hotspotOffsetY;
				int distToMetinHotspot = DISTANCE_APPROX(ch->GetX() - hx, ch->GetY() - hy);

				if (distToMetinHotspot < 1200)
				{
					// Reached current hotspot: wander in search of stones, then advance to next
					++s_adwPlayerBotMetinHotspotVisits[hIdx];
					state.uMetinHotspotIndex = (state.uMetinHotspotIndex + 1) % 12;
					state.dwNextWanderTime = dwNow + 2000;
					targetX = ch->GetX() + number(-600, 600);
					targetY = ch->GetY() + number(-600, 600);
				}
				else
				{
					// Rove toward next Metin hotspot
					state.dwNextWanderTime = dwNow + 1200;
					targetX = hx;
					targetY = hy;
				}
			}
			// 2. Role: Party Fighter (25% of bots - dense monster camps)
			else if (state.bBotRole == BOT_ROLE_PARTY_FIGHTER)
			{
				// Centres of group-spawn rectangles from metin2_map_b1/regen.txt.
				// The final point is still validated and snapped through server_attr.
				const struct { long x; long y; } partyCamps[8] = {
					{ 39000, 200200 }, // South-West White Oath Camp
					{ 37000, 168400 }, // West White Oath Camp
					{ 84600, 197500 }, // South-East Bear / Tiger Camp
					{ 61000, 203600 }, // South Dense Boar / Wolf Plains
					{ 80300, 135700 }, // North-East Plateau Camp
					{ 61600, 133500 }, // North Meadow Camp
					{ 35000, 135500 }, // North-West Lykos Territory
					{ 85800, 169700 }  // East Cursed Beast Camp
				};

				int campIdx = ((pid / 4) + state.uMetinHotspotIndex) % 8;
				long cx = partyCamps[campIdx].x;
				long cy = partyCamps[campIdx].y;
				long campOffsetX = 0, campOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x43414d50U + campIdx, 350, 1350,
						campOffsetX, campOffsetY);
				cx += campOffsetX;
				cy += campOffsetY;
				int distToCamp = DISTANCE_APPROX(ch->GetX() - cx, ch->GetY() - cy);

				if (distToCamp > 1800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = cx;
					targetY = cy;
				}
				else
				{
					state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 2000);
					targetX = ch->GetX() + number(-800, 800);
					targetY = ch->GetY() + number(-800, 800);
				}
			}
			// 3. Role: Area Mob Grinder (50% of bots - spread across 32 discrete hubs in 8 quadrants)
			else
			{
				// Each hub is the centre of a real group-spawn rectangle from
				// regen.txt, rather than a guessed coordinate.  Rectangle centres
				// still pass through the live attr/same-component validation.
				const struct { long x; long y; } hubs[32] = {
					// 1. North Quadrant (Meadows & North Road)
					{ 61600, 133500 }, { 55600, 135200 }, { 70600, 135800 }, { 59500, 123600 },
					// 2. North-East Quadrant (Plateaus & Hills)
					{ 80300, 135700 }, { 83500, 130000 }, { 75500, 143600 }, { 87200, 147300 },
					// 3. East Quadrant (Cursed Animals & Tigers)
					{ 85800, 169700 }, { 80300, 165800 }, { 88600, 162800 }, { 82900, 178300 },
					// 4. South-East Quadrant (Brown Bears & Tiger Groves)
					{ 84600, 197500 }, { 78300, 191000 }, { 89800, 195300 }, { 86700, 209800 },
					// 5. South Quadrant (Wild Boars, Grey Wolves, Tigers)
					{ 61000, 203600 }, { 52700, 194700 }, { 67400, 194700 }, { 61100, 214300 },
					// 6. South-West Quadrant (White Oath Camps & Black Bears)
					{ 39000, 200200 }, { 29900, 196400 }, { 46200, 206200 }, { 33500, 209800 },
					// 7. West Quadrant (Valley of Mi-Jung, White Oath)
					{ 37000, 168400 }, { 30200, 164500 }, { 44700, 165800 }, { 32600, 178200 },
					// 8. North-West Quadrant (Lykos territory, Cursed Wolves)
					{ 35000, 135500 }, { 40600, 145000 }, { 28500, 146900 }, { 42100, 129300 }
				};

				int hubIdx = ((pid / 2) + state.uMetinHotspotIndex) % 32;
				long hubX = hubs[hubIdx].x;
				long hubY = hubs[hubIdx].y;
				long hubOffsetX = 0, hubOffsetY = 0;
				GetPlayerBotStableOffset(pid, 0x48554200U + hubIdx, 250, 1100,
						hubOffsetX, hubOffsetY);
				hubX += hubOffsetX;
				hubY += hubOffsetY;
				int distToHub = DISTANCE_APPROX(ch->GetX() - hubX, ch->GetY() - hubY);

				if (distToHub > 1800)
				{
					state.dwNextWanderTime = dwNow + 1200;
					targetX = hubX;
					targetY = hubY;
				}
				else
				{
					state.dwNextWanderTime = dwNow + PLAYERBOT_WANDER_INTERVAL + number(0, 2000);
					targetX = ch->GetX() + number(-800, 800);
					targetY = ch->GetY() + number(-800, 800);
				}
			}
		}
		else if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2)
		{
			const DWORD pid = ch->GetPlayerID();
			if (ShouldPlayerBotHuntM2Bestials(ch))
			{
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				const size_t bestialIndex = (pid + state.uMetinHotspotIndex) % 2;
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid, 0x42455354U + (DWORD)bestialIndex,
						100, 450, offsetX, offsetY);
				targetX = PLAYERBOT_M2_BESTIAL_HOTSPOTS[bestialIndex].x + offsetX;
				targetY = PLAYERBOT_M2_BESTIAL_HOTSPOTS[bestialIndex].y + offsetY;
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1000)
				{
					++state.uMetinHotspotIndex;
					state.dwNextWanderTime = dwNow + number(5000, 9000);
					targetX = ch->GetX() + number(-500, 500);
					targetY = ch->GetY() + number(-500, 500);
				}
			}
			else
			{
				// Real spawn clusters from metin2_map_b3/regen.txt. Persistent hub
				// assignment stops the M2 cohort from tracing one identical route.
				const TPlayerBotMapPoint hubs[12] = {
					{ 173800, 218500 }, { 182500, 224300 }, { 188900, 234700 },
					{ 190000, 250200 }, { 187300, 263200 }, { 185500, 278700 },
					{ 175000, 286500 }, { 162200, 288900 }, { 149200, 289900 },
					{ 136900, 287300 }, { 125700, 286800 }, { 116500, 279800 }
				};
				const size_t hubIndex = (pid + state.uMetinHotspotIndex) % 12;
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid, 0x4d324855U + (DWORD)hubIndex,
						150, 700, offsetX, offsetY);
				targetX = hubs[hubIndex].x + offsetX;
				targetY = hubs[hubIndex].y + offsetY;
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1400)
				{
					++state.uMetinHotspotIndex;
					targetX = ch->GetX() + number(-700, 700);
					targetY = ch->GetY() + number(-700, 700);
				}
			}
		}
		else if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M3)
		{
			// Centres of real map24 infected-animal regen rectangles.  Keeping the
			// arrival/return strip out of this set also prevents farming inside the
			// teleporter's BANPK area.
			const TPlayerBotMapPoint hubs[10] = {
				{ 189700, 6000 }, { 196900, 7000 }, { 206800, 7800 },
				{ 212600, 9400 }, { 204200, 12400 }, { 209200, 18800 },
				{ 195800, 18100 }, { 187600, 15100 }, { 216000, 15900 },
				{ 201500, 21700 }
			};
			const DWORD pid = ch->GetPlayerID();
			const size_t hubIndex = (pid + state.uMetinHotspotIndex) % 10;
			long offsetX = 0, offsetY = 0;
			GetPlayerBotStableOffset(pid, 0x4d334855U + (DWORD)hubIndex,
					100, 550, offsetX, offsetY);
			targetX = hubs[hubIndex].x + offsetX;
			targetY = hubs[hubIndex].y + offsetY;
			if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1100)
			{
				++state.uMetinHotspotIndex;
				targetX = ch->GetX() + number(-600, 600);
				targetY = ch->GetY() + number(-600, 600);
			}
		}
		else if (IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			// Densest spawn clusters of each map, snapped onto a real regen.txt
			// coordinate so a hub can never be planted inside an obstacle.
			const TPlayerBotMapPoint orcValleyHubs[12] = {
				{ 347800, 726700 }, { 317000, 726900 }, { 313100, 731700 },
				{ 346400, 733700 }, { 319200, 734700 }, { 337000, 734800 },
				{ 327200, 742300 }, { 332100, 749800 }, { 330800, 758100 },
				{ 336300, 760100 }, { 348300, 797000 }, { 282100, 797600 }
			};
			const TPlayerBotMapPoint desertHubs[12] = {
				{ 291300, 515700 }, { 237500, 525900 }, { 264600, 526100 },
				{ 317900, 526100 }, { 336900, 534300 }, { 245100, 542500 },
				{ 264500, 552300 }, { 327700, 552900 }, { 253900, 570100 },
				{ 327800, 579500 }, { 321600, 582700 }, { 273800, 614900 }
			};
			const bool inDesert = ch->GetMapIndex() == PLAYERBOT_MAP_DESERT;
			const TPlayerBotMapPoint* hubs = inDesert ? desertHubs : orcValleyHubs;
			const DWORD pid = ch->GetPlayerID();
			const size_t hubIndex = (pid + state.uMetinHotspotIndex) % 12;
			long offsetX = 0, offsetY = 0;
			GetPlayerBotStableOffset(pid,
					(inDesert ? 0x44455348U : 0x4f524348U) + (DWORD)hubIndex,
					150, 700, offsetX, offsetY);
			targetX = hubs[hubIndex].x + offsetX;
			targetY = hubs[hubIndex].y + offsetY;
			if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1400)
			{
				++state.uMetinHotspotIndex;
				targetX = ch->GetX() + number(-700, 700);
				targetY = ch->GetY() + number(-700, 700);
			}
		}
		else if (ch->GetMapIndex() == PLAYERBOT_MAP_MONKEY_EASY)
		{
			// Rooms from metin2_map_monkey_dungeon_12/regen.txt. The navigation
			// grid, not straight-line Goto, connects them through the maze corridors.
			const TPlayerBotMapPoint rooms[8] = {
				{ 852300, 454900 }, { 872600, 450800 }, { 889800, 451500 },
				{ 861200, 478600 }, { 873100, 471400 }, { 890800, 470400 },
				{ 860800, 496600 }, { 898600, 443100 }
			};
			const DWORD pid = ch->GetPlayerID();
			const size_t roomIndex = (pid + state.uMetinHotspotIndex) % 8;
			long offsetX = 0, offsetY = 0;
			GetPlayerBotStableOffset(pid, 0x4d4f4e4bU + (DWORD)roomIndex,
					50, 250, offsetX, offsetY);
			targetX = rooms[roomIndex].x + offsetX;
			targetY = rooms[roomIndex].y + offsetY;
			if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 900)
			{
				++state.uMetinHotspotIndex;
				targetX = ch->GetX() + number(-450, 450);
				targetY = ch->GetY() + number(-450, 450);
			}
		}
		else
		{
			// Wander in random nearby direction
			targetX += number(-1500, 1500);
			targetY += number(-1500, 1500);
		}

		if (!MovePlayerBot(ch, targetX, targetY, dwNow, 32, true))
		{
			state.dwNextWanderTime = dwNow + 1500;
			if (state.bStuckCounter >= 3)
			{
				// Abandon a genuinely unreachable region instead of recomputing the
				// identical path forever.  The shared index selects another hotspot,
				// camp or hub depending on the bot's role.
				++state.uMetinHotspotIndex;
				ClearPlayerBotRoute(state, true);
			}
		}
	}

	class FPlayerBotPartyLootOwner
	{
		public:
			FPlayerBotPartyLootOwner(LPITEM item) : m_item(item), m_bFound(false) {}

			void operator () (LPCHARACTER member)
			{
				if (!m_bFound && member && m_item && m_item->IsOwnership(member))
					m_bFound = true;
			}

			bool Found() const { return m_bFound; }

		private:
			LPITEM m_item;
			bool m_bFound;
	};

	bool IsPlayerBotPartyLoot(LPCHARACTER owner, LPITEM item)
	{
		if (!owner || !item)
			return false;
		if (item->IsOwnership(owner))
			return true;
		if (!owner->GetParty() ||
				IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP))
			return false;

		// Metin drops are distributed between party members.  PickupItem already
		// supports a nearby member collecting an item for its assigned owner, so
		// the AI search must expose party-owned drops too.  Previously every bot
		// only saw its own PID and most of a party Metin drop was left behind as
		// soon as the individual owners moved toward another target.
		FPlayerBotPartyLootOwner finder(item);
		owner->GetParty()->ForEachOnlineMember(finder);
		return finder.Found();
	}

	class CCollectPlayerBotLoot
	{
		public:
			CCollectPlayerBotLoot(LPCHARACTER owner, int maxDistance, const std::map<DWORD, DWORD>& failedLoot, DWORD dwNow) :
				m_owner(owner),
				m_maxDistance(maxDistance),
				m_failedLoot(failedLoot),
				m_dwNow(dwNow)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_ITEM))
					return false;

				LPITEM item = static_cast<LPITEM>(entity);
				// Ground items in this source tree retain entity map index 0.
				// Being in one of the owner's neighbouring sectrees is the reliable
				// same-map test; checking item->GetMapIndex() rejects every drop.
				if (!item->GetSectree() || !IsPlayerBotPartyLoot(m_owner, item))
					return false;

				std::map<DWORD, DWORD>::const_iterator fit = m_failedLoot.find(item->GetVID());
				if (fit != m_failedLoot.end() && m_dwNow < fit->second)
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - item->GetX(),
						m_owner->GetY() - item->GetY());
				if (distance <= m_maxDistance)
					m_items.push_back(std::make_pair(distance, item));

				return true;
			}

			void Sort()
			{
				std::sort(m_items.begin(), m_items.end());
			}

			const std::vector<std::pair<int, LPITEM> >& GetItems() const { return m_items; }

		private:
			LPCHARACTER m_owner;
			int m_maxDistance;
			const std::map<DWORD, DWORD>& m_failedLoot;
			DWORD m_dwNow;
			std::vector<std::pair<int, LPITEM> > m_items;
	};

	class CDetectPlayerBotCombatThreat
	{
		public:
			CDetectPlayerBotCombatThreat(LPCHARACTER owner) : m_owner(owner), m_found(false) {}

			bool operator () (LPENTITY entity)
			{
				if (m_found || !entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER mob = static_cast<LPCHARACTER>(entity);
				if (!mob || !mob->IsMonster() || mob->IsDead() ||
						mob->GetMapIndex() != m_owner->GetMapIndex())
					return false;
				LPCHARACTER victim = mob->GetVictim();
				if (victim == m_owner || (victim && m_owner->GetParty() &&
						victim->GetParty() == m_owner->GetParty()))
				{
					if (DISTANCE_APPROX(m_owner->GetX() - mob->GetX(),
							m_owner->GetY() - mob->GetY()) <= 2500)
						m_found = true;
				}
				return false;
			}

			bool Found() const { return m_found; }

		private:
			LPCHARACTER m_owner;
			bool m_found;
	};

	bool TryPlayerBotCombatPickup(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() || dwNow < state.dwNextLootPickupTime)
			return false;

		// Set the throttle before scanning.  An empty floor used to leave the
		// timestamp untouched, so the second HandleLoot call in the same update and
		// every following update repeated a complete nine-sectree snapshot.
		state.dwNextLootPickupTime = dwNow + number(
				PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MIN,
				PLAYERBOT_COMBAT_LOOT_SCAN_INTERVAL_MAX);

		// This is the server equivalent of repeatedly pressing Z: inspect only the
		// immediate pickup circle, never Stop(), never clear the victim and never
		// walk toward an item while a pack is still engaged.
		CCollectPlayerBotLoot collector(ch, PLAYERBOT_PICKUP_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		const std::vector<std::pair<int, LPITEM> >& items = collector.GetItems();
		if (items.empty())
			return false;

		LPITEM pickup = NULL;
		DWORD firstSeen = 0;
		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item || !item->GetSectree())
				continue;
			const DWORD itemVID = item->GetVID();
			std::map<DWORD, DWORD>::iterator seen = state.mapLootSeenSince.find(itemVID);
			if (seen == state.mapLootSeenSince.end())
			{
				state.mapLootSeenSince[itemVID] = dwNow;
				continue;
			}
			const DWORD visibleDelay = PLAYERBOT_LOOT_VISIBLE_DELAY_MIN +
					(PlayerBotNavHash(itemVID ^ ch->GetPlayerID()) %
					 (PLAYERBOT_LOOT_VISIBLE_DELAY_MAX - PLAYERBOT_LOOT_VISIBLE_DELAY_MIN + 1));
			if (dwNow - seen->second >= visibleDelay)
			{
				pickup = item;
				firstSeen = seen->second;
				break;
			}
		}
		if (!pickup)
			return false;

		const DWORD itemVID = pickup->GetVID();
		const DWORD itemVnum = pickup->GetVnum();
		state.dwNextLootPickupTime = dwNow + number(
				PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN, PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX);
		if (ch->PickupItem(itemVID))
		{
			state.mapLootSeenSince.erase(itemVID);
			if (itemVnum == PLAYERBOT_HORSE_MEDAL_VNUM)
			{
				const int looted = std::max(0,
						ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG)) + 1;
				ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG, looted);
				ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_MAP_FLAG, ch->GetMapIndex());
				ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_TIME_FLAG, get_global_time());
			}
			sys_log(1, "PLAYERBOT_AI: combat-Z pickup pid=%u name=%s item_vid=%u vnum=%u visible_ms=%u",
					ch->GetPlayerID(), ch->GetName(), itemVID, itemVnum,
					(unsigned int)(dwNow - firstSeen));
			return true;
		}

		state.mapFailedLootVIDs[itemVID] = dwNow + 5000;
		state.mapLootSeenSince.erase(itemVID);
		return false;
	}

	bool HandleLoot(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree())
			return false;

		// Cleanup must also run for bots which spend minutes in continuous combat.
		// Keep it periodic: walking both maps on every AI tick is unnecessary.
		if (dwNow >= state.dwNextLootCleanupTime)
		{
			state.dwNextLootCleanupTime = dwNow + PLAYERBOT_LOOT_CLEANUP_INTERVAL +
					(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedLootVIDs.begin();
					it != state.mapFailedLootVIDs.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedLootVIDs.erase(it++);
				else
					++it;
			}
			for (std::map<DWORD, DWORD>::iterator it = state.mapLootSeenSince.begin();
					it != state.mapLootSeenSince.end(); )
			{
				if (dwNow - it->second > 120000)
					state.mapLootSeenSince.erase(it++);
				else
					++it;
			}
		}

		LPCHARACTER activeTarget = state.dwTargetVID != 0
			? CHARACTER_MANAGER::instance().Find(state.dwTargetVID)
			: NULL;
		const bool bFightingActiveTarget = activeTarget && !activeTarget->IsDead() &&
				(activeTarget->IsMonster() || activeTarget->IsStone());
		const bool bRecentCombat = state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime < 1800;
		if (!bFightingActiveTarget && (bRecentCombat ||
				dwNow >= state.dwNextLootThreatCheckTime))
		{
			CDetectPlayerBotCombatThreat threat(ch);
			ch->GetSectree()->ForEachAround(threat);
			state.bLootThreatNearby = threat.Found();
			state.dwNextLootThreatCheckTime = dwNow + number(
					PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MIN,
					PLAYERBOT_LOOT_THREAT_SCAN_INTERVAL_MAX);
		}
		// A dead primary target does not mean its group is finished. While either a
		// live target or an attacking pack exists, perform only non-blocking Z pickup.
		// Once the threat scan says the pack is clear, recent combat no longer hides
		// the 25 m loot search: the bot finishes its own drop before choosing a new mob.
		if (bFightingActiveTarget || state.bLootThreatNearby)
		{
			TryPlayerBotCombatPickup(ch, state, dwNow);
			return false;
		}
		if (dwNow < state.dwNextLootSearchTime)
			return false;

		CCollectPlayerBotLoot collector(ch, PLAYERBOT_LOOT_SEARCH_RANGE,
				state.mapFailedLootVIDs, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();
		const std::vector<std::pair<int, LPITEM> >& items = collector.GetItems();
		if (items.empty())
		{
			// An empty 25 m search used to run twice per second for every peaceful
			// bot.  Delay only the next empty-floor query; as soon as an item is seen,
			// the normal 500 ms walking/visibility cadence remains unchanged.
			state.dwNextLootSearchTime = dwNow + number(
					PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MIN,
					PLAYERBOT_EMPTY_LOOT_SCAN_INTERVAL_MAX);
			return false;
		}
		state.dwNextLootSearchTime = 0;
		SetPlayerBotAction(state, BOT_ACTION_LOOT, dwNow);

		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item || !item->GetSectree())
				continue;
			const DWORD itemVID = item->GetVID();
			if (state.mapLootSeenSince.find(itemVID) == state.mapLootSeenSince.end())
				state.mapLootSeenSince[itemVID] = dwNow;
		}

		LPITEM nearest = items.front().second;
		if (!nearest || !nearest->GetSectree())
			return false;
		const DWORD nearestVID = nearest->GetVID();
		const DWORD firstSeen = state.mapLootSeenSince[nearestVID];
		const DWORD visibleDelay = PLAYERBOT_LOOT_VISIBLE_DELAY_MIN +
				(PlayerBotNavHash(nearestVID ^ ch->GetPlayerID()) %
				 (PLAYERBOT_LOOT_VISIBLE_DELAY_MAX - PLAYERBOT_LOOT_VISIBLE_DELAY_MIN + 1));
		const bool visibleLongEnough = dwNow - firstSeen >= visibleDelay;

		if (items.front().first <= PLAYERBOT_PICKUP_RANGE)
		{
			ch->Stop();
			if (!visibleLongEnough || dwNow < state.dwNextLootPickupTime)
				return true;

			const DWORD itemVnum = nearest->GetVnum();
			state.dwNextLootPickupTime = dwNow + number(
					PLAYERBOT_LOOT_PICKUP_INTERVAL_MIN, PLAYERBOT_LOOT_PICKUP_INTERVAL_MAX);
			if (ch->PickupItem(nearestVID))
			{
				state.mapLootSeenSince.erase(nearestVID);
				if (itemVnum == PLAYERBOT_HORSE_MEDAL_VNUM)
				{
					const int looted = std::max(0,
							ch->GetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG)) + 1;
					ch->SetQuestFlag(PLAYERBOT_HORSE_MEDALS_LOOTED_FLAG, looted);
					ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_MAP_FLAG, ch->GetMapIndex());
					ch->SetQuestFlag(PLAYERBOT_HORSE_LAST_LOOT_TIME_FLAG, get_global_time());
					sys_log(0, "PLAYERBOT_HORSE: real medal looted pid=%u name=%s map=%ld total_looted=%d",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), looted);
				}
				sys_log(1, "PLAYERBOT_AI: picked up delayed loot pid=%u name=%s item_vid=%u vnum=%u visible_ms=%u",
						ch->GetPlayerID(), ch->GetName(), nearestVID, itemVnum,
						(unsigned int)(dwNow - firstSeen));
				return true;
			}

			state.mapFailedLootVIDs[nearestVID] = dwNow + 5000;
			state.mapLootSeenSince.erase(nearestVID);
			sys_log(1, "PLAYERBOT_AI: pickup failed pid=%u name=%s item_vid=%u vnum=%u -> retrying in 5s",
					ch->GetPlayerID(), ch->GetName(), nearestVID, itemVnum);
			return true;
		}

		// Filter out items in pickup range that just failed
		std::vector<std::pair<int, LPITEM> > pendingItems;
		for (size_t i = 0; i < items.size(); ++i)
		{
			LPITEM item = items[i].second;
			if (!item)
				continue;
			if (state.mapFailedLootVIDs.find(item->GetVID()) != state.mapFailedLootVIDs.end())
				continue;
			pendingItems.push_back(items[i]);
		}

		if (pendingItems.empty())
			return false;

		nearest = pendingItems.front().second;
		if (!nearest || !nearest->GetSectree())
			return false;

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		if (pendingItems.front().first > PLAYERBOT_PICKUP_RANGE)
		{
			if (!MovePlayerBot(ch, nearest->GetX(), nearest->GetY(), dwNow) &&
					state.bStuckCounter >= 3)
			{
				state.mapFailedLootVIDs[nearest->GetVID()] = dwNow + 30000;
				ClearPlayerBotRoute(state, true);
				return false;
			}
		}
		else
			ch->Stop();

		return true;
	}

	void PersistPlayerBot(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetDesc())
			return;

		const BYTE level = ch->GetLevel();
		if (level == state.bLastPersistedLevel && dwNow < state.dwNextPersistTime)
			return;

		state.bLastPersistedLevel = level;
		state.dwNextPersistTime = dwNow + PLAYERBOT_PERSIST_INTERVAL +
				(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);

		ch->SaveReal();
		ch->FlushDelayedSaveItem();
		const DWORD playerID = ch->GetPlayerID();
		db_clientdesc->DBPacket(HEADER_GD_FLUSH_CACHE, 0, &playerID, sizeof(playerID));
		sys_log(1, "PLAYERBOT_AI: persisted state pid=%u name=%s level=%u exp=%u gold=%lld",
				playerID, ch->GetName(), level, ch->GetExp(), (long long)ch->GetGold());
	}

	void StartPlayerBotTacticalRetreat(LPCHARACTER ch, TPlayerBotAIState& state,
			LPCHARACTER threat, DWORD dwNow)
	{
		if (!ch || state.bRecoveringAfterDeath || state.bTacticalRetreat)
			return;
		state.bTacticalRetreat = true;
		state.dwRetreatStartedTime = dwNow;
		state.dwNextRetreatMoveTime = 0;
		state.dwRetreatThreatVID = threat ? threat->GetVID() : 0;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		SetPlayerBotGoal(ch, state, BOT_GOAL_SURVIVE, dwNow);
		SetPlayerBotAction(state, BOT_ACTION_RECOVER, dwNow);
		sys_log(0, "PLAYERBOT_AI: tactical retreat started pid=%u name=%s hp=%d/%d threat_vid=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP(),
				state.dwRetreatThreatVID);
	}

	bool HandlePlayerBotTacticalRetreat(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !state.bTacticalRetreat)
			return false;
		SetPlayerBotAction(state, BOT_ACTION_RECOVER, dwNow);

		LPCHARACTER threat = state.dwRetreatThreatVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwRetreatThreatVID) : NULL;
		const bool bThreatHasAggro = threat && !threat->IsDead() &&
				threat->GetMapIndex() == ch->GetMapIndex() && threat->GetVictim() == ch;
		const int hpPercent = ch->GetMaxHP() > 0 ? ch->GetHP() * 100 / ch->GetMaxHP() : 100;

		if (!bThreatHasAggro && hpPercent >= PLAYERBOT_RETREAT_END_HP_PERCENT)
		{
			state.bTacticalRetreat = false;
			state.dwRetreatStartedTime = 0;
			state.dwRetreatThreatVID = 0;
			state.dwNextRetreatMoveTime = 0;
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_AI: tactical retreat complete pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
			return false;
		}

		if (!bThreatHasAggro && hpPercent < PLAYERBOT_RETREAT_END_HP_PERCENT &&
				dwNow >= state.dwNextRecoveryHealTime)
		{
			const int heal = std::max(1, ch->GetMaxHP() * 3 / 100);
			ch->PointChange(POINT_HP, std::min(heal, ch->GetMaxHP() - ch->GetHP()));
			state.dwNextRecoveryHealTime = dwNow + 1000;
		}

		if (dwNow < state.dwNextRetreatMoveTime)
			return true;
		state.dwNextRetreatMoveTime = dwNow + PLAYERBOT_RETREAT_MOVE_INTERVAL;

		const long threatX = threat ? threat->GetX() : ch->GetX();
		const long threatY = threat ? threat->GetY() : ch->GetY();
		static const int escapeX[8] = { 1300, 900, 0, -900, -1300, -900, 0, 900 };
		static const int escapeY[8] = { 0, 900, 1300, 900, 0, -900, -1300, -900 };
		long bestX = ch->GetX();
		long bestY = ch->GetY();
		int bestScore = INT_MIN;
		for (int i = 0; i < 8; ++i)
		{
			const long candidateX = ch->GetX() + escapeX[i];
			const long candidateY = ch->GetY() + escapeY[i];
			if (!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), candidateX, candidateY))
				continue;
			const int threatDistance = DISTANCE_APPROX(candidateX - threatX, candidateY - threatY);
			const int jitter = (int)(PlayerBotNavHash(ch->GetPlayerID() ^ (DWORD)i) % 120U);
			if (threatDistance + jitter > bestScore)
			{
				bestScore = threatDistance + jitter;
				bestX = candidateX;
				bestY = candidateY;
			}
		}

		if (bestScore != INT_MIN)
			MovePlayerBot(ch, bestX, bestY, dwNow, 24, true);
		else
			ch->Stop();
		return true;
	}

	bool HandlePostDeathRecovery(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!state.bRecoveringAfterDeath)
			return false;
		SetPlayerBotAction(state, BOT_ACTION_RECOVER, dwNow);

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		const int maxHP = ch->GetMaxHP();
		const int recoveryHP = maxHP > 0
				? (maxHP * PLAYERBOT_RECOVERY_HP_PERCENT + 99) / 100
				: 0;

		// Bot descriptors do not receive the same idle regeneration cadence as
		// a real client.  Previously a bot without red potions could therefore
		// wait forever at 50 HP after restart_here.  Rest-heal it gradually while
		// it is protected and moving away from the death location.
		if (maxHP > 0 && ch->GetHP() < recoveryHP && dwNow >= state.dwNextRecoveryHealTime)
		{
			const int healStep = std::max(1, maxHP * PLAYERBOT_RECOVERY_REST_HEAL_PERCENT / 100);
			const int healAmount = std::min(healStep, recoveryHP - ch->GetHP());
			if (healAmount > 0)
				ch->PointChange(POINT_HP, healAmount);
			state.dwNextRecoveryHealTime = dwNow + PLAYERBOT_RECOVERY_REST_HEAL_INTERVAL;
		}

		if (maxHP > 0 && ch->GetHP() >= recoveryHP)
		{
			state.bRecoveringAfterDeath = false;
			state.dwNextRecoveryProtectionTime = 0;
			state.dwNextRecoveryHealTime = 0;
			sys_log(0, "PLAYERBOT_AI: recovery complete pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
			return false;
		}

		if (dwNow >= state.dwNextRecoveryProtectionTime)
		{
			ch->ReviveInvisible(10);
			state.dwNextRecoveryProtectionTime = dwNow + PLAYERBOT_RECOVERY_PROTECTION_INTERVAL;
		}

		// While recovering invisibly, if we are right inside the death danger zone (< 800 distance),
		// step away from the death spot to a safer position
		if (state.lDeathX != 0 && state.lDeathY != 0)
		{
			const int distFromDeath = DISTANCE_APPROX(ch->GetX() - state.lDeathX, ch->GetY() - state.lDeathY);
			if (distFromDeath < 800)
			{
				static const int kEscapeX[8] = { 1000, 700, 0, -700, -1000, -700, 0, 700 };
				static const int kEscapeY[8] = { 0, 700, 1000, 700, 0, -700, -1000, -700 };
				const BYTE escapeDirection = (BYTE)((ch->GetPlayerID() + state.bDeathCount +
						state.bStuckCounter) % 8);
				const long targetX = state.lDeathX + kEscapeX[escapeDirection];
				const long targetY = state.lDeathY + kEscapeY[escapeDirection];
				MovePlayerBot(ch, targetX, targetY, dwNow, 16, true);
				return true;
			}
		}

		ch->Stop();
		return true;
	}

	bool HandleDeath(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch->IsDead())
		{
			state.dwDeathDetectedTime = 0;
			state.dwNextReviveAttemptTime = 0;
			return false;
		}

		if (state.dwDeathDetectedTime == 0)
		{
			state.bTacticalRetreat = false;
			state.dwRetreatStartedTime = 0;
			state.dwNextRetreatMoveTime = 0;
			state.dwRetreatThreatVID = 0;
			state.dwLastDeathTime = dwNow;
			state.dwLastKillerVID = state.dwTargetVID;
			state.lDeathX = ch->GetX();
			state.lDeathY = ch->GetY();
			++state.bDeathCount;

			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);

			state.bRecoveringAfterDeath = false;
			state.dwNextRecoveryProtectionTime = 0;
			state.dwNextRecoveryHealTime = 0;
			state.dwDeathDetectedTime = dwNow;
			state.dwNextReviveAttemptTime = dwNow + PLAYERBOT_REVIVE_DELAY;
			sys_log(0, "PLAYERBOT_AI: death detected pid=%u name=%s killer_vid=%u death_pos=(%ld,%ld) total_deaths=%u",
					ch->GetPlayerID(), ch->GetName(), state.dwLastKillerVID, state.lDeathX, state.lDeathY, state.bDeathCount);
			return true;
		}

		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		if (dwNow >= state.dwNextReviveAttemptTime)
		{
			state.dwNextReviveAttemptTime = dwNow + 2000;
			interpret_command(ch, "restart_here", strlen("restart_here"));

			if (!ch->IsDead())
			{
				state.bRecoveringAfterDeath = true;
				state.dwNextRecoveryProtectionTime = dwNow + PLAYERBOT_RECOVERY_PROTECTION_INTERVAL;
				state.dwNextRecoveryHealTime = dwNow;
				ch->RemoveBadAffect();
				const int safeInitialHP = ch->GetMaxHP() > 0
						? (ch->GetMaxHP() * PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT + 99) / 100
						: 0;
				if (ch->GetHP() < safeInitialHP)
					ch->PointChange(POINT_HP, safeInitialHP - ch->GetHP());
				ch->ReviveInvisible(10);
				sys_log(0, "PLAYERBOT_AI: revived at same position pid=%u name=%s hp=%d/%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
				state.dwDeathDetectedTime = 0;
				state.dwNextReviveAttemptTime = 0;
			}
		}

		return true;
	}

	struct TPlayerBotPartyStrength
	{
		TPlayerBotPartyStrength() : iReadyMembers(0), iTotalLevels(0), iHighestLevel(0), iChallengeMaxLevel(0) {}

		int iReadyMembers;
		int iTotalLevels;
		int iHighestLevel;
		int iChallengeMaxLevel;
	};

	class FCollectPlayerBotPartyStrength
	{
		public:
			FCollectPlayerBotPartyStrength(LPCHARACTER anchor, DWORD dwNow, TPlayerBotPartyStrength& strength) :
				m_anchor(anchor), m_dwNow(dwNow), m_strength(strength)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || member->IsDead() || member->GetMapIndex() != m_anchor->GetMapIndex() ||
						!member->GetDesc() || !member->GetDesc()->IsBot())
					return;

				if (DISTANCE_APPROX(member->GetX() - m_anchor->GetX(), member->GetY() - m_anchor->GetY()) >
						PLAYERBOT_PARTY_CHALLENGE_RADIUS)
					return;

				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(member->GetPlayerID());
				if (it == s_mapPlayerBotAIStates.end() || it->second.bVisitingShop ||
						it->second.bRecoveringAfterDeath)
					return;

				if (it->second.dwLastDeathTime != 0 && m_dwNow - it->second.dwLastDeathTime < 60000)
					return;

				if (member->GetMaxHP() <= 0 ||
						member->GetHP() * 100 < member->GetMaxHP() * PLAYERBOT_PARTY_READY_HP_PERCENT ||
						member->GetWear(WEAR_WEAPON) == NULL)
					return;

				++m_strength.iReadyMembers;
				m_strength.iTotalLevels += member->GetLevel();
				m_strength.iHighestLevel = std::max(m_strength.iHighestLevel, (int)member->GetLevel());
			}

		private:
			LPCHARACTER m_anchor;
			DWORD m_dwNow;
			TPlayerBotPartyStrength& m_strength;
	};

	bool GetPlayerBotPartyStrength(LPCHARACTER ch, DWORD dwNow, TPlayerBotPartyStrength& strength)
	{
		if (!ch || !ch->GetParty())
			return false;

		FCollectPlayerBotPartyStrength collector(ch, dwNow, strength);
		ch->GetParty()->ForEachOnMapMember(collector, ch->GetMapIndex());

		if (strength.iReadyMembers < PLAYERBOT_PARTY_CHALLENGE_MIN_MEMBERS)
			return false;

		// Two independent limits prevent one strong bot from dragging weak party
		// members into a suicidal fight. Five level-15 bots can challenge level 35,
		// while three such bots remain restricted to normal M1 opponents.
		const int formationLimit = strength.iHighestLevel +
				(strength.iReadyMembers - 1) * PLAYERBOT_PARTY_LEVEL_BONUS_PER_MEMBER;
		const int combinedPowerLimit = strength.iTotalLevels / 2;
		strength.iChallengeMaxLevel = std::min(formationLimit, combinedPowerLimit);
		return strength.iChallengeMaxLevel > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA;
	}

	bool CanPlayerBotPartyChallenge(LPCHARACTER ch, LPCHARACTER target, DWORD dwNow,
			TPlayerBotPartyStrength* outStrength)
	{
		if (!ch || !target || !target->IsMonster() || target->IsDead() ||
				target->GetMapIndex() != ch->GetMapIndex())
			return false;

		TPlayerBotPartyStrength strength;
		if (!GetPlayerBotPartyStrength(ch, dwNow, strength) ||
				target->GetLevel() > strength.iChallengeMaxLevel)
			return false;

		if (outStrength)
			*outStrength = strength;
		return true;
	}

	class FFindPlayerBotPartyFocus
	{
		public:
			FFindPlayerBotPartyFocus(LPCHARACTER owner, DWORD dwNow) :
				m_owner(owner), m_dwNow(dwNow), m_target(NULL), m_bLeaderTarget(false)
			{
			}

			void operator () (LPCHARACTER member)
			{
				if (!member || !member->GetDesc() || !member->GetDesc()->IsBot())
					return;

				TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(member->GetPlayerID());
				if (it == s_mapPlayerBotAIStates.end() || it->second.dwTargetVID == 0)
					return;

				LPCHARACTER candidate = CHARACTER_MANAGER::instance().Find(it->second.dwTargetVID);
				if (!candidate || (!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead() ||
						candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()) ||
						!IsPlayerBotReachable(m_owner->GetMapIndex(),
								m_owner->GetX(), m_owner->GetY(), candidate->GetX(), candidate->GetY()) ||
						DISTANCE_APPROX(candidate->GetX() - m_owner->GetX(), candidate->GetY() - m_owner->GetY()) > PLAYERBOT_PARTY_COHESION_RADIUS ||
						(candidate->IsStone() &&
						 !IsPlayerBotMetinWorthFighting(m_owner, candidate)) ||
						(candidate->IsMonster() &&
							 candidate->GetLevel() > m_owner->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
							 !CanPlayerBotPartyChallenge(m_owner, candidate, m_dwNow, NULL)))
					return;

				const bool bCandidateIsLeaderTarget =
						(m_owner->GetParty() && member == m_owner->GetParty()->GetLeaderCharacter());
				if (!m_target || (bCandidateIsLeaderTarget && !m_bLeaderTarget))
				{
					m_target = candidate;
					m_bLeaderTarget = bCandidateIsLeaderTarget;
				}
			}

			LPCHARACTER GetTarget() const { return m_target; }

		private:
			LPCHARACTER m_owner;
			DWORD m_dwNow;
			LPCHARACTER m_target;
			bool m_bLeaderTarget;
	};

	LPCHARACTER FindPlayerBotPartyFocusTarget(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetParty() || state.bVisitingShop || state.bRecoveringAfterDeath ||
				!IsPlayerBotPartyCohesive(ch, 2, PLAYERBOT_PARTY_COHESION_RADIUS) ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				(state.dwLastDeathTime != 0 && dwNow - state.dwLastDeathTime < 60000) ||
				ch->GetMaxHP() <= 0 ||
				ch->GetHP() * 100 < ch->GetMaxHP() * PLAYERBOT_PARTY_READY_HP_PERCENT)
			return NULL;

		FFindPlayerBotPartyFocus finder(ch, dwNow);
		ch->GetParty()->ForEachOnMapMember(finder, ch->GetMapIndex());
		return finder.GetTarget();
	}

	class CFindPlayerBotEngagedTarget
	{
		public:
			CFindPlayerBotEngagedTarget(LPCHARACTER owner) :
				m_owner(owner), m_target(NULL), m_bestPriority(INT_MIN) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (!candidate || candidate == m_owner || !candidate->IsMonster() ||
						candidate->IsDead() || candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return true;

				LPCHARACTER victim = candidate->GetVictim();
				const bool attacksOwner = victim == m_owner;
				const bool attacksParty = victim && m_owner->GetParty() &&
						victim->GetParty() == m_owner->GetParty();
				if (!attacksOwner && !attacksParty)
					return true;

				const int distance = DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > 2500)
					return true;
				const int priority = (attacksOwner ? 100000 : 50000) - distance;
				if (!m_target || priority > m_bestPriority)
				{
					m_target = candidate;
					m_bestPriority = priority;
				}
				return true;
			}

			LPCHARACTER GetTarget() const { return m_target; }

		private:
			LPCHARACTER m_owner;
			LPCHARACTER m_target;
			int m_bestPriority;
	};

	LPCHARACTER FindPlayerBotEngagedTarget(LPCHARACTER ch)
	{
		if (!ch || !ch->GetSectree() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return NULL;
		CFindPlayerBotEngagedTarget finder(ch);
		ch->GetSectree()->ForEachAround(finder);
		return finder.GetTarget();
	}

	class CCountPlayerBotStoneAttackers
	{
		public:
			CCountPlayerBotStoneAttackers(LPCHARACTER stone) :
				m_stone(stone), m_count(0) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER attacker = static_cast<LPCHARACTER>(entity);
				if (!attacker || !attacker->IsPC() || attacker->IsDead() ||
						attacker->GetMapIndex() != m_stone->GetMapIndex() ||
						DISTANCE_APPROX(attacker->GetX() - m_stone->GetX(),
								attacker->GetY() - m_stone->GetY()) > PLAYERBOT_STONE_SUPPORT_RANGE)
					return true;

				bool attacksStone = attacker->GetVictim() == m_stone;
				TPlayerBotAIStateMap::const_iterator it =
						s_mapPlayerBotAIStates.find(attacker->GetPlayerID());
				if (it != s_mapPlayerBotAIStates.end() &&
						it->second.dwTargetVID == (DWORD)m_stone->GetVID())
					attacksStone = true;
				if (attacksStone && m_count < 255)
					++m_count;
				return true;
			}

			BYTE GetCount() const { return m_count; }

		private:
			LPCHARACTER m_stone;
			BYTE m_count;
	};

	BYTE CountPlayerBotStoneAttackers(LPCHARACTER stone)
	{
		if (!stone || !stone->GetSectree())
			return 0;
		CCountPlayerBotStoneAttackers counter(stone);
		stone->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
	}

	void ResetPlayerBotStoneProgress(TPlayerBotAIState& state)
	{
		state.dwStoneFightStartTime = 0;
		state.dwStoneProgressVID = 0;
		state.dwStoneLastProgressTime = 0;
		state.dwNextStoneProgressCheckTime = 0;
		state.iLastStoneHP = 0;
		state.bLastStoneAttackerCount = 0;
	}

	bool ShouldPlayerBotAbandonStone(LPCHARACTER ch, LPCHARACTER stone,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !stone || !stone->IsStone() || stone->IsDead())
		{
			ResetPlayerBotStoneProgress(state);
			return false;
		}

		if (state.dwStoneProgressVID != (DWORD)stone->GetVID())
		{
			ResetPlayerBotStoneProgress(state);
			state.dwStoneProgressVID = stone->GetVID();
			state.dwStoneFightStartTime = dwNow;
			state.dwStoneLastProgressTime = dwNow;
			state.dwNextStoneProgressCheckTime =
					dwNow + PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL;
			state.iLastStoneHP = stone->GetHP();
			state.bLastStoneAttackerCount = CountPlayerBotStoneAttackers(stone);
			return false;
		}

		if (dwNow < state.dwNextStoneProgressCheckTime)
			return false;
		state.dwNextStoneProgressCheckTime =
				dwNow + PLAYERBOT_STONE_PROGRESS_CHECK_INTERVAL;

		const BYTE attackerCount = CountPlayerBotStoneAttackers(stone);
		// A new helper may turn a regenerative stalemate into real progress. Give the
		// enlarged group a complete observation window instead of abandoning just as
		// help arrives.
		if (attackerCount > state.bLastStoneAttackerCount)
			state.dwStoneLastProgressTime = dwNow;
		state.bLastStoneAttackerCount = attackerCount;

		const int meaningfulDamage = std::max(1, stone->GetMaxHP() / 200);
		if (stone->GetHP() + meaningfulDamage <= state.iLastStoneHP)
		{
			state.iLastStoneHP = stone->GetHP();
			state.dwStoneLastProgressTime = dwNow;
		}

		if (dwNow - state.dwStoneFightStartTime < PLAYERBOT_STONE_INITIAL_GRACE)
			return false;
		const DWORD stallTimeout = attackerCount >= 2
				? PLAYERBOT_STONE_GROUP_STALL_TIMEOUT
				: PLAYERBOT_STONE_SOLO_STALL_TIMEOUT;
		if (dwNow - state.dwStoneLastProgressTime < stallTimeout)
			return false;

		const DWORD failedVID = stone->GetVID();
		const int currentHP = stone->GetHP();
		const int maxHP = stone->GetMaxHP();
		state.mapFailedStones[failedVID] = dwNow + PLAYERBOT_STONE_FAILED_COOLDOWN;
		ReleasePlayerBotMetinReservation(ch, stone);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwNextWanderTime = dwNow + number(1000, 2500);
		sys_log(0, "PLAYERBOT_METIN: abandoned stalled stone pid=%u name=%s stone_vid=%u stone=%s hp=%d/%d best_hp=%d attackers=%u fight_ms=%u stalled_ms=%u cooldown_ms=%u",
				ch->GetPlayerID(), ch->GetName(), failedVID, stone->GetName(),
				currentHP, maxHP, state.iLastStoneHP, (unsigned int)attackerCount,
				(unsigned int)(dwNow - state.dwStoneFightStartTime),
				(unsigned int)(dwNow - state.dwStoneLastProgressTime),
				(unsigned int)PLAYERBOT_STONE_FAILED_COOLDOWN);
		ResetPlayerBotStoneProgress(state);
		return true;
	}

	bool IsTargetClaimedByAnotherBot(LPCHARACTER owner, DWORD dwTargetVID)
	{
		if (!owner || dwTargetVID == 0)
			return false;

		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (it->first == owner->GetPlayerID() || it->second.dwTargetVID != dwTargetVID)
				continue;

			LPCHARACTER claimant = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (owner->GetParty() && claimant && claimant->GetParty() == owner->GetParty())
				continue;

			return true;
		}

		return false;
	}

	struct TTargetCandidate
	{
		DWORD dwVID;
		int distance;
		int level;
		bool bIsStone;
		bool bPriorityObjective;
		int score;

		bool operator < (const TTargetCandidate& other) const
		{
			return score > other.score; // Higher score first
		}
	};

	class CCollectPlayerBotTargets
	{
		public:
			CCollectPlayerBotTargets(LPCHARACTER owner, int maxDistance, int maxLevel,
					int partyChallengeMaxLevel, DWORD desiredMobVnum, DWORD dwAvoidVID,
					long lAvoidX, long lAvoidY, int avoidRadius,
					const std::map<DWORD, DWORD>& failedStones,
					const std::map<DWORD, DWORD>& failedTargets, DWORD dwNow) :
				m_owner(owner),
				m_maxDistance(maxDistance),
				m_maxLevel(maxLevel),
				m_partyChallengeMaxLevel(partyChallengeMaxLevel),
				m_desiredMobVnum(desiredMobVnum),
				m_dwAvoidVID(dwAvoidVID),
				m_lAvoidX(lAvoidX),
				m_lAvoidY(lAvoidY),
				m_avoidRadius(avoidRadius),
				m_failedStones(failedStones),
				m_failedTargets(failedTargets),
				m_dwNow(dwNow),
				m_huntM2Bestials(owner && owner->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 &&
						ShouldPlayerBotHuntM2Bestials(owner))
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || (!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead())
					return false;
				if (IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;

				if (candidate->IsStone())
					RememberPlayerBotMetin(candidate, m_dwNow);

				if (m_dwAvoidVID != 0 && candidate->GetVID() == m_dwAvoidVID)
					return false;

				std::map<DWORD, DWORD>::const_iterator failedTarget = m_failedTargets.find(candidate->GetVID());
				if (failedTarget != m_failedTargets.end() && m_dwNow < failedTarget->second)
					return false;

				// Stones must remain inside the useful drop window and not be in the
				// failed-stone cooldown. This also keeps over-levelled bots away from
				// decorative low Metins which no longer reward their time.
				if (candidate->IsStone())
				{
					if (!IsPlayerBotMetinWorthFighting(m_owner, candidate))
						return false;

					std::map<DWORD, DWORD>::const_iterator stit = m_failedStones.find(candidate->GetVID());
					if (stit != m_failedStones.end() && m_dwNow < stit->second)
						return false;
				}
				else if (candidate->IsMonster())
				{
					if (candidate->GetLevel() > m_maxLevel)
						return false;
				}

				if (candidate->GetMapIndex() != m_owner->GetMapIndex())
					return false;

				if (m_avoidRadius > 0 && m_lAvoidX != 0 && m_lAvoidY != 0)
				{
					const int deathDist = DISTANCE_APPROX(
							m_lAvoidX - candidate->GetX(),
							m_lAvoidY - candidate->GetY());
					if (deathDist <= m_avoidRadius)
						return false;
				}

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > m_maxDistance)
					return false;

				const int botLevel = m_owner->GetLevel();
				const int mobLevel = candidate->GetLevel();
				const int levelDelta = mobLevel - botLevel;
				const bool isQuestTarget = candidate->IsMonster() &&
						m_desiredMobVnum != 0 && candidate->GetRaceNum() == m_desiredMobVnum;
				const bool isBestialWeaponTarget = candidate->IsMonster() &&
						m_huntM2Bestials &&
						(candidate->GetRaceNum() == 533 || candidate->GetRaceNum() == 534);

				// Do not cross a hunting field for obsolete prey, but kill a weaker mob
				// which is already on the route. This makes local grinding look like a
				// player holding Space instead of visibly walking past living packs.
				if (candidate->IsMonster() && !isQuestTarget && botLevel >= 6 &&
						levelDelta <= -3 && candidate->GetVictim() != m_owner &&
						distance > PLAYERBOT_LOCAL_CHAIN_RANGE)
					return false;

				// Component reachability lets the bot route around a wall while still
				// rejecting monsters on disconnected islands or terrain components.
				if (!IsPlayerBotReachable(m_owner->GetMapIndex(), m_owner->GetX(), m_owner->GetY(), candidate->GetX(), candidate->GetY()))
				{
					if (candidate->GetVictim() != m_owner)
						return false;
				}

				TTargetCandidate tc;
				tc.dwVID = candidate->GetVID();
				tc.distance = distance;
				tc.level = mobLevel;
				tc.bIsStone = candidate->IsStone();
				tc.bPriorityObjective = isQuestTarget || isBestialWeaponTarget;

				int baseScore = 0;
				TPlayerBotAIStateMap::iterator sit = s_mapPlayerBotAIStates.find(m_owner->GetPlayerID());
				const bool isMetinHunter = (sit != s_mapPlayerBotAIStates.end() && sit->second.bBotRole == BOT_ROLE_METIN_HUNTER);

				if (candidate->IsStone())
				{
					baseScore = isMetinHunter ? 1500000 : 350000;
				}
				else
				{
					// An active research task is a real alternative to generic levelling.
					// It must outrank a convenient nearby pack, otherwise an over-levelled
					// bot would never return to the alpha wolves required by early quests.
					if (isQuestTarget)
						baseScore += 1800000;
					if (isBestialWeaponTarget)
						baseScore += 1750000;

					// If mob is attacking the bot, give high defense priority
					if (candidate->GetVictim() == m_owner)
					{
						baseScore += 500000;
					}

					if (m_partyChallengeMaxLevel > 0 &&
							mobLevel > botLevel + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
							mobLevel <= m_partyChallengeMaxLevel)
					{
						baseScore += 1200000 + mobLevel * 1000;
					}

					// For dedicated Metin breakers, normal mobs get low score unless attacking
					if (isMetinHunter && candidate->GetVictim() != m_owner)
					{
						baseScore += 5000;
					}
					else
					{
						// Sweet spot: mob level within [-2, +5] of bot level gets HUGE priority
						if (levelDelta >= -2 && levelDelta <= 5)
							baseScore += 300000 + (10 - abs(levelDelta)) * 5000;
						else if (levelDelta > 5 && levelDelta <= 9)
							baseScore += 150000;
						else if (levelDelta < -2 && levelDelta >= -5)
							baseScore += 50000;
						else if (levelDelta < -5)
							baseScore += 5000; // Low score for dogs when high level, but allows killing them along the way
						else
							baseScore += 10000;

						// Priority on appropriate level hunting mobs (Bears, Tigers, White Oath for Lv 10+)
						const DWORD raceVnum = candidate->GetRaceNum();
						if ((botLevel <= 5 && (raceVnum == 101 || raceVnum == 102 || raceVnum == 103)) ||
							(botLevel >= 6 && botLevel <= 10 && (raceVnum == 104 || raceVnum == 106 || raceVnum == 107 || raceVnum == 108 || raceVnum == 109)) ||
							(botLevel >= 11 && (raceVnum >= 110 && raceVnum <= 115 || (raceVnum >= 301 && raceVnum <= 394))))
						{
							baseScore += 80000; // Extra focus on hunting mobs!
						}

						// If another player/bot is already fighting this normal mob, spread out to unengaged mobs
						if (candidate->GetVictim() != NULL && candidate->GetVictim() != m_owner)
						{
							baseScore -= 180000;
						}
					}
				}

				// Distance penalty: only 2 points per unit so level-appropriate mobs within 2000 distance beat low-level dogs
				tc.score = baseScore - (distance * 2);
				m_targets.push_back(tc);

				return true;
			}

			void Sort()
			{
				std::sort(m_targets.begin(), m_targets.end());
			}

			const std::vector<TTargetCandidate>& GetTargets() const { return m_targets; }

		private:
			LPCHARACTER m_owner;
			int m_maxDistance;
			int m_maxLevel;
			int m_partyChallengeMaxLevel;
			DWORD m_desiredMobVnum;
			DWORD m_dwAvoidVID;
			long m_lAvoidX;
			long m_lAvoidY;
			int m_avoidRadius;
			const std::map<DWORD, DWORD>& m_failedStones;
			const std::map<DWORD, DWORD>& m_failedTargets;
			DWORD m_dwNow;
			bool m_huntM2Bestials;
			std::vector<TTargetCandidate> m_targets;
	};

	LPCHARACTER FindDistributedTarget(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return NULL;

		// Clean up expired failed stone entries
		if (!state.mapFailedStones.empty())
		{
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedStones.begin();
					it != state.mapFailedStones.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedStones.erase(it++);
				else
					++it;
			}
		}

		if (!state.mapFailedTargets.empty())
		{
			for (std::map<DWORD, DWORD>::iterator it = state.mapFailedTargets.begin();
					it != state.mapFailedTargets.end(); )
			{
				if (dwNow >= it->second)
					state.mapFailedTargets.erase(it++);
				else
					++it;
			}
		}

		// If bot died recently (< 60 seconds ago), be cautious:
		// 1) Avoid the killer monster VID and death pack zone (800 range around death location).
		// 2) Lower max target level to at most (bot level - 1) to hunt easier, safer mobs.
		const bool bRecentDeath = (state.dwLastDeathTime != 0 && (dwNow - state.dwLastDeathTime < 60000));
		const DWORD dwAvoidVID = bRecentDeath ? state.dwLastKillerVID : 0;
		const long lAvoidX = bRecentDeath ? state.lDeathX : 0;
		const long lAvoidY = bRecentDeath ? state.lDeathY : 0;
		const int avoidRadius = bRecentDeath ? 800 : 0;

		int maxLevel = ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA;
		int partyChallengeMaxLevel = 0;
		TPlayerBotPartyStrength partyStrength;
		if (!bRecentDeath && ch->GetParty() && ch->GetParty()->GetLeaderCharacter() == ch &&
				GetPlayerBotPartyStrength(ch, dwNow, partyStrength))
		{
			partyChallengeMaxLevel = partyStrength.iChallengeMaxLevel;
			maxLevel = std::max(maxLevel, partyChallengeMaxLevel);
		}
		if (bRecentDeath)
		{
			maxLevel = std::max(1, ch->GetLevel() - 1);
		}

		DWORD desiredBiologistMobVnum = 0;
		const TPlayerBotBiologistMission* biologistMission =
				GetActivePlayerBotBiologistMission(ch);
		if (biologistMission && !state.bVisitingBiologist)
		{
			const int accepted = std::max(0, ch->GetQuestFlag(
					GetPlayerBotBiologistFlag(*biologistMission, "collect_count")));
			const int remaining = std::max(0,
					(int)biologistMission->requiredCount - accepted);
			if (ch->CountSpecifyItem(biologistMission->itemVnum) < remaining)
				desiredBiologistMobVnum = biologistMission->mobVnum;
		}
		const DWORD desiredHuntingMobVnum = GetActivePlayerBotHuntingMobVnum(ch);
		DWORD desiredQuestMobVnum = desiredBiologistMobVnum;
		if (desiredHuntingMobVnum != 0)
		{
			// When both activities are open, rotate small deterministic cohorts every
			// two minutes. The world then looks like independent players choosing
			// goals, not one synchronized swarm finishing Biologist first.
			if (desiredQuestMobVnum == 0 ||
					((ch->GetPlayerID() + dwNow / 120000) % 3) == 0)
				desiredQuestMobVnum = desiredHuntingMobVnum;
		}

		const int targetSearchRange = ch->GetParty()
				? PLAYERBOT_PARTY_COHESION_RADIUS : PLAYERBOT_SEARCH_RANGE;
		CCollectPlayerBotTargets collector(ch, targetSearchRange, maxLevel,
				partyChallengeMaxLevel, desiredQuestMobVnum, dwAvoidVID,
				lAvoidX, lAvoidY, avoidRadius, state.mapFailedStones,
				state.mapFailedTargets, dwNow);
		ch->GetSectree()->ForEachAround(collector);
		collector.Sort();

		std::vector<TTargetCandidate> targets = collector.GetTargets();

		// Fallback: If no safer/lower level targets found in range, allow normal level cap but still avoid exact killer
		if (targets.empty() && bRecentDeath)
		{
			CCollectPlayerBotTargets fallbackCollector(ch, targetSearchRange,
					ch->GetLevel(), 0, desiredQuestMobVnum, dwAvoidVID, 0, 0, 0,
					state.mapFailedStones, state.mapFailedTargets, dwNow);
			ch->GetSectree()->ForEachAround(fallbackCollector);
			fallbackCollector.Sort();
			targets = fallbackCollector.GetTargets();
		}

		if (targets.empty())
			return NULL;

		// A ready leader does not roll the party challenge together with ordinary
		// mobs. Pick the best unclaimed elite deterministically; party-to-party
		// claim separation still distributes multiple groups over different elites.
		if (partyChallengeMaxLevel > 0)
		{
			for (size_t i = 0; i < targets.size(); ++i)
			{
				if (targets[i].level > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA &&
						targets[i].level <= partyChallengeMaxLevel &&
						!IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
					return CHARACTER_MANAGER::instance().Find(targets[i].dwVID);
			}
		}

		// Chain ordinary combat into the closest unclaimed pack. A nearby quest or
		// Bestial objective wins over generic prey, but a far-away objective no longer
		// makes the bot walk past mobs at its feet. Metin hunters retain their global
		// stone scoring and reservations below.
		if (state.bBotRole != BOT_ROLE_METIN_HUNTER)
		{
			DWORD closestObjectiveVID = 0;
			DWORD closestLocalVID = 0;
			int closestObjectiveDistance = INT_MAX;
			int closestLocalDistance = INT_MAX;
			for (size_t i = 0; i < targets.size(); ++i)
			{
				if (targets[i].bIsStone || targets[i].distance > PLAYERBOT_LOCAL_CHAIN_RANGE ||
						IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
					continue;
				if (targets[i].bPriorityObjective &&
						targets[i].distance < closestObjectiveDistance)
				{
					closestObjectiveDistance = targets[i].distance;
					closestObjectiveVID = targets[i].dwVID;
				}
				if (targets[i].distance < closestLocalDistance)
				{
					closestLocalDistance = targets[i].distance;
					closestLocalVID = targets[i].dwVID;
				}
			}
			const DWORD chainedVID = closestObjectiveVID != 0
					? closestObjectiveVID : closestLocalVID;
			if (chainedVID != 0)
				return CHARACTER_MANAGER::instance().Find(chainedVID);
		}

		std::vector<DWORD> availableTargets;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			if (!IsTargetClaimedByAnotherBot(ch, targets[i].dwVID))
				availableTargets.push_back(targets[i].dwVID);
		}

		// Prefer a target no other bot has claimed. Randomizing inside a bounded
		// nearest-candidate window spreads bots without sending them across the map.
		const size_t poolSize = availableTargets.empty() ? targets.size() : availableTargets.size();
		const size_t choiceCount = std::min(poolSize, PLAYERBOT_TARGET_CHOICE_WINDOW);
		const size_t choiceIndex = (size_t)number(0, (int)choiceCount - 1);
		const DWORD targetVID = availableTargets.empty()
			? targets[choiceIndex].dwVID
			: availableTargets[choiceIndex];

		return CHARACTER_MANAGER::instance().Find(targetVID);
	}

	class CCollectPlayerBotMeleeTargets
	{
		public:
			CCollectPlayerBotMeleeTargets(LPCHARACTER owner, DWORD primaryVID) :
				m_owner(owner),
				m_primaryVID(primaryVID)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;

				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || candidate->GetVID() == m_primaryVID ||
						(!candidate->IsMonster() && !candidate->IsStone()) || candidate->IsDead())
					return false;

				if (candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()) ||
						(candidate->IsMonster() && candidate->GetLevel() > m_owner->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA))
					return false;

				const int distance = DISTANCE_APPROX(
						m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance <= PLAYERBOT_MELEE_SPLASH_RANGE)
					m_targets.push_back(std::make_pair(distance, candidate->GetVID()));

				return true;
			}

			void Sort()
			{
				std::sort(m_targets.begin(), m_targets.end());
			}

			const std::vector<std::pair<int, DWORD> >& GetTargets() const { return m_targets; }

		private:
			LPCHARACTER m_owner;
			DWORD m_primaryVID;
			std::vector<std::pair<int, DWORD> > m_targets;
	};

	DWORD AttackPlayerBotMeleeGroup(LPCHARACTER ch, LPCHARACTER primary)
	{
		if (!ch || !primary || !ch->GetSectree())
			return 0;

		const bool bIsTargetValid = (primary->IsMonster() || primary->IsStone());
		if (!bIsTargetValid || primary->IsDead())
			return 0;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW);
		LPITEM arrow = NULL;
		if (isBow && ch->GetArrowAndBow(&weapon, &arrow, 1) != 1)
			return 0;

		int iDamage = isBow ? CalcArrowDamage(ch, primary, weapon, arrow, false) : CalcMeleeDamage(ch, primary, false, false);
		if (iDamage < 5)
			iDamage = number(15, 35) + ch->GetLevel() * 4;

		DWORD hitCount = 1;
		primary->Damage(ch, iDamage, DAMAGE_TYPE_NORMAL);
		if (isBow)
			ch->UseArrow(arrow, 1);
		primary->SetSyncOwner(ch);
		if (!primary->IsDead() && primary->CanBeginFight())
			primary->BeginFight(ch);

		if (!isBow)
		{
			CCollectPlayerBotMeleeTargets collector(ch, primary->GetVID());
			ch->GetSectree()->ForEachAround(collector);
			collector.Sort();

			const std::vector<std::pair<int, DWORD> >& targets = collector.GetTargets();
			for (size_t i = 0; i < targets.size() && hitCount < PLAYERBOT_MAX_MELEE_TARGETS; ++i)
			{
				LPCHARACTER secondary = CHARACTER_MANAGER::instance().Find(targets[i].second);
				if (!secondary || secondary->IsDead() || (!secondary->IsMonster() && !secondary->IsStone()))
					continue;

				int iSecDamage = CalcMeleeDamage(ch, secondary, false, false);
				if (iSecDamage < 5)
					iSecDamage = number(12, 28) + ch->GetLevel() * 3;

				secondary->Damage(ch, iSecDamage, DAMAGE_TYPE_NORMAL);
				++hitCount;
			}
		}

		if (!primary->IsDead())
		{
			ch->SetVictim(primary);
			ch->SetRotationToXY(primary->GetX(), primary->GetY());
		}

		return hitCount;
	}

	bool ExecutePlayerBotBasicAttack(LPCHARACTER ch, LPCHARACTER target,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !target || ch->IsDead() || target->IsDead() ||
				state.bVisitingShop || state.bRecoveringAfterDeath ||
				(!target->IsMonster() && !target->IsStone()) ||
				ch->GetMapIndex() != target->GetMapIndex() ||
				IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(target->GetMapIndex(), target->GetX(), target->GetY()))
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		if (!weapon || weapon->GetType() != ITEM_WEAPON)
			return false;

		const bool isBow = weapon->GetSubType() == WEAPON_BOW;
		if (isBow)
		{
			LPITEM bow = NULL;
			LPITEM arrow = NULL;
			if (ch->GetArrowAndBow(&bow, &arrow, 1) != 1)
				return false;
		}
		const int combatRange = isBow ? 800 : 280;
		if (DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY()) > combatRange)
			return false;

		// The light combat pass must never interrupt navigation.  The full AI pass
		// stops the bot as soon as it reaches combat range; subsequent combo hits
		// can then be emitted on every 250 ms manager tick.
		if (ch->IsStateMove() || dwNow < state.dwNextAttackTime)
			return false;

		int attSpeed = ch->GetPoint(POINT_ATT_SPEED);
		if (attSpeed <= 0)
			attSpeed = 100;
		// A normal 100 attack-speed character produces a visible combo step about
		// every half-second.  This is deliberately shared by bows: their old 1 s+
		// pause came from the staggered AI tick, not from the client animation.
		const int hitInterval = std::max(250, 48000 / attSpeed);

		ch->SetPosition(POS_FIGHTING);
		ch->SetVictim(target);
		ch->SetRotationToXY(target->GetX(), target->GetY());
		state.dwNextAttackTime = dwNow + hitInterval;
		state.dwLastCombatActionTime = dwNow;
		SendPlayerBotAttackPacket(ch, target, state.bComboMotion);
		AttackPlayerBotMeleeGroup(ch, target);

		if (isBow)
			state.bComboMotion = MOTION_COMBO_ATTACK_1;
		else
		{
			++state.bComboMotion;
			if (state.bComboMotion > MOTION_COMBO_ATTACK_4)
				state.bComboMotion = MOTION_COMBO_ATTACK_1;
		}
		return true;
	}

	bool IsPlayerBotMultiPullBuild(LPCHARACTER ch, bool* naturalTank)
	{
		if (naturalTank)
			*naturalTank = false;
		if (!ch || ch->GetLevel() < 15 || ch->GetParty() ||
				(ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M1 &&
				 ch->GetMapIndex() != PLAYERBOT_MAP_CHUNJO_M2))
			return false;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
		if (!weapon || !armor || !shield || !helmet ||
				(weapon->GetType() == ITEM_WEAPON &&
				 weapon->GetSubType() == WEAPON_BOW))
			return false;

		const bool isNaturalTank =
				(ch->GetJob() == JOB_WARRIOR && ch->GetSkillGroup() == 2) ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 1);
		const bool isHeavilyArmored = armor->GetRefineLevel() >= 5 &&
				shield->GetRefineLevel() >= 5 && helmet->GetRefineLevel() >= 4;
		if (naturalTank)
			*naturalTank = isNaturalTank;
		return isNaturalTank || isHeavilyArmored;
	}

	class CCountPlayerBotPullAggressors
	{
		public:
			CCountPlayerBotPullAggressors(LPCHARACTER owner) : m_owner(owner), m_count(0) {}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate != m_owner && candidate->IsMonster() &&
						!candidate->IsDead() && candidate->GetVictim() == m_owner &&
						DISTANCE_APPROX(candidate->GetX() - m_owner->GetX(),
								candidate->GetY() - m_owner->GetY()) <= PLAYERBOT_MULTI_PULL_SEARCH_RANGE)
					++m_count;
				return true;
			}

			int GetCount() const { return m_count; }

		private:
			LPCHARACTER m_owner;
			int m_count;
	};

	int CountPlayerBotPullAggressors(LPCHARACTER ch)
	{
		if (!ch || !ch->GetSectree())
			return 0;
		CCountPlayerBotPullAggressors counter(ch);
		ch->GetSectree()->ForEachAround(counter);
		return counter.GetCount();
	}

	class CFindPlayerBotPullTarget
	{
		public:
			CFindPlayerBotPullTarget(LPCHARACTER owner,
					const std::vector<PIXEL_POSITION>& centers) :
				m_owner(owner), m_centers(centers), m_bestVID(0), m_bestScore(INT_MAX)
			{
			}

			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(entity);
				if (candidate == m_owner || !candidate->IsMonster() || candidate->IsStone() ||
						candidate->IsDead() || candidate->GetVictim() != NULL ||
						candidate->GetMobRank() >= MOB_RANK_BOSS ||
						candidate->GetMapIndex() != m_owner->GetMapIndex() ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;

				const int minLevel = std::max(1, (int)m_owner->GetLevel() - 3);
				const int maxLevel = (int)m_owner->GetLevel() + 2;
				if (candidate->GetLevel() < minLevel || candidate->GetLevel() > maxLevel)
					return false;

				const int distance = DISTANCE_APPROX(m_owner->GetX() - candidate->GetX(),
						m_owner->GetY() - candidate->GetY());
				if (distance > PLAYERBOT_MULTI_PULL_SEARCH_RANGE ||
						!IsPlayerBotReachable(m_owner->GetMapIndex(), m_owner->GetX(), m_owner->GetY(),
								candidate->GetX(), candidate->GetY()) ||
						IsTargetClaimedByAnotherBot(m_owner, candidate->GetVID()))
					return false;

				for (size_t i = 0; i < m_centers.size(); ++i)
				{
					if (DISTANCE_APPROX(candidate->GetX() - m_centers[i].x,
							candidate->GetY() - m_centers[i].y) <
							PLAYERBOT_MULTI_PULL_GROUP_SEPARATION)
						return false;
				}

				// A small deterministic jitter distributes simultaneous tanks without
				// sacrificing the preference for a nearby pack.
				const int score = distance + (int)(PlayerBotNavHash(
						m_owner->GetPlayerID() ^ candidate->GetVID()) % 350U);
				if (score < m_bestScore)
				{
					m_bestScore = score;
					m_bestVID = candidate->GetVID();
				}
				return true;
			}

			DWORD GetBestVID() const { return m_bestVID; }

		private:
			LPCHARACTER m_owner;
			const std::vector<PIXEL_POSITION>& m_centers;
			DWORD m_bestVID;
			int m_bestScore;
	};

	LPCHARACTER FindPlayerBotPullTarget(LPCHARACTER ch,
			const std::vector<PIXEL_POSITION>& centers)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		CFindPlayerBotPullTarget finder(ch, centers);
		ch->GetSectree()->ForEachAround(finder);
		return finder.GetBestVID() != 0
				? CHARACTER_MANAGER::instance().Find(finder.GetBestVID()) : NULL;
	}

	void FinishPlayerBotMultiPull(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* reason)
	{
		const BYTE pulledGroups = state.bMultiPullGroups;
		const BYTE desiredGroups = state.bMultiPullDesiredGroups;
		state.bMultiPullActive = false;
		state.bMultiPullGroups = 0;
		state.bMultiPullDesiredGroups = 0;
		state.dwMultiPullStartedTime = 0;
		state.dwNextMultiPullActionTime = 0;
		state.dwMultiPullTargetVID = 0;
		state.vecMultiPullCenters.clear();
		state.dwNextMultiPullTime = dwNow + number(
				PLAYERBOT_MULTI_PULL_MIN_COOLDOWN, PLAYERBOT_MULTI_PULL_MAX_COOLDOWN);

		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch);
		state.dwTargetVID = engaged ? engaged->GetVID() : 0;
		if (engaged)
			ch->SetVictim(engaged);
		else
			ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_PULL: finished pid=%u name=%s groups=%u/%u aggressors=%d hp=%d/%d reason=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)pulledGroups,
				(unsigned int)desiredGroups, CountPlayerBotPullAggressors(ch),
				ch->GetHP(), ch->GetMaxHP(), reason ? reason : "?");
	}

	bool HandlePlayerBotMultiPull(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		bool naturalTank = false;
		const bool buildEligible = IsPlayerBotMultiPullBuild(ch, &naturalTank);
		const bool goalEligible = state.bBotRole == BOT_ROLE_MOB_GRINDER &&
				(state.bLongTermGoal == BOT_GOAL_LEVEL_UP ||
				 state.bLongTermGoal == BOT_GOAL_HUNTING);
		const bool recentlyDied = state.dwLastDeathTime != 0 &&
				dwNow - state.dwLastDeathTime < 120000;
		size_t redPots = 0, bluePots = 0;
		CountPlayerBotPotions(ch, redPots, bluePots);
		const int hpPercent = ch && ch->GetMaxHP() > 0
				? ch->GetHP() * 100 / ch->GetMaxHP() : 0;

		if (!buildEligible || !goalEligible || recentlyDied || redPots < 30 ||
				state.bVisitingShop || state.bRecoveringAfterDeath || state.bTacticalRetreat)
		{
			if (state.bMultiPullActive)
				FinishPlayerBotMultiPull(ch, state, dwNow, "eligibility_lost");
			return false;
		}

		if (!state.bMultiPullActive)
		{
			if (state.dwNextMultiPullTime == 0)
			{
				state.dwNextMultiPullTime = dwNow + 5000 +
						PlayerBotNavHash(ch->GetPlayerID() ^ 0x50554c4cU) % 40000U;
				return false;
			}
			LPCHARACTER current = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (dwNow < state.dwNextMultiPullTime || hpPercent < PLAYERBOT_MULTI_PULL_START_HP_PERCENT ||
					(current && !current->IsDead()) || FindPlayerBotEngagedTarget(ch))
				return false;

			LPCHARACTER first = FindPlayerBotPullTarget(ch, state.vecMultiPullCenters);
			if (!first)
			{
				state.dwNextMultiPullTime = dwNow + number(10000, 20000);
				return false;
			}

			state.bMultiPullActive = true;
			state.bMultiPullGroups = 0;
			state.bMultiPullDesiredGroups = naturalTank
					? (BYTE)(2 + PlayerBotNavHash(ch->GetPlayerID() ^
							(dwNow / 60000U)) % 3U) : 2;
			state.dwMultiPullStartedTime = dwNow;
			state.dwNextMultiPullActionTime = dwNow;
			state.iMultiPullStartHPPercent = hpPercent;
			state.dwMultiPullTargetVID = first->GetVID();
			state.dwTargetVID = first->GetVID();
			state.vecMultiPullCenters.clear();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PULL: started pid=%u name=%s level=%u desired_groups=%u hp=%d/%d natural_tank=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)state.bMultiPullDesiredGroups, ch->GetHP(),
					ch->GetMaxHP(), naturalTank ? 1 : 0);
		}

		const int aggressors = CountPlayerBotPullAggressors(ch);
		if (hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT ||
				state.iMultiPullStartHPPercent - hpPercent >= PLAYERBOT_MULTI_PULL_MAX_HP_LOSS_PERCENT ||
				aggressors >= PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS ||
				dwNow - state.dwMultiPullStartedTime >= PLAYERBOT_MULTI_PULL_TIMEOUT)
		{
			const char* reason = hpPercent <= PLAYERBOT_MULTI_PULL_MIN_HP_PERCENT
					? "low_hp" : (aggressors >= PLAYERBOT_MULTI_PULL_MAX_AGGRESSORS
						? "aggressor_cap" : (dwNow - state.dwMultiPullStartedTime >=
							PLAYERBOT_MULTI_PULL_TIMEOUT ? "timeout" : "hp_loss"));
			FinishPlayerBotMultiPull(ch, state, dwNow, reason);
			return false;
		}

		if (state.bMultiPullGroups >= state.bMultiPullDesiredGroups)
		{
			FinishPlayerBotMultiPull(ch, state, dwNow, "desired_groups_ready");
			return false;
		}

		LPCHARACTER target = state.dwMultiPullTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwMultiPullTargetVID) : NULL;
		if (!target || target->IsDead() || !target->IsMonster() || target->IsStone() ||
				target->GetMapIndex() != ch->GetMapIndex() ||
				(target->GetVictim() != NULL && target->GetVictim() != ch))
		{
			target = FindPlayerBotPullTarget(ch, state.vecMultiPullCenters);
			state.dwMultiPullTargetVID = target ? target->GetVID() : 0;
			state.dwTargetVID = state.dwMultiPullTargetVID;
			ClearPlayerBotRoute(state, true);
			if (!target)
			{
				FinishPlayerBotMultiPull(ch, state, dwNow, "no_fresh_pack");
				return false;
			}
		}

		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		state.dwTargetVID = target->GetVID();
		RememberPlayerBotMapRace(ch, target);
		ch->SetVictim(target);
		// Aggressive packs often wake up as soon as the bot enters their radius. In
		// that case running on is the authentic pull action; attacking would stop to
		// clear the very first pack instead of gathering the planned spot.
		if (target->GetVictim() == ch)
		{
			PIXEL_POSITION center;
			center.x = target->GetX();
			center.y = target->GetY();
			center.z = 0;
			state.vecMultiPullCenters.push_back(center);
			++state.bMultiPullGroups;
			state.dwMultiPullTargetVID = 0;
			state.dwTargetVID = 0;
			state.dwNextMultiPullActionTime = dwNow + PLAYERBOT_MULTI_PULL_ACTION_DELAY;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PULL: aggroed pack pid=%u name=%s groups=%u/%u target=%s aggressors=%d hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bMultiPullGroups,
					(unsigned int)state.bMultiPullDesiredGroups, target->GetName(),
					CountPlayerBotPullAggressors(ch), ch->GetHP(), ch->GetMaxHP());
			return true;
		}
		const int distance = DISTANCE_APPROX(ch->GetX() - target->GetX(),
				ch->GetY() - target->GetY());
		if (distance > PLAYERBOT_MELEE_RANGE)
		{
			// A warrior/sura with a battle horse gathers the valour-cloak spot from
			// the saddle instead of climbing down between packs.
			const bool fightOnHorse = CanPlayerBotFightOnHorse(ch, target);
			MovePlayerBot(ch, target->GetX(), target->GetY(), dwNow, 4, false,
					fightOnHorse, fightOnHorse);
			return true;
		}

		if (dwNow < state.dwNextMultiPullActionTime)
			return true;
		if (ch->IsStateMove())
			ch->Stop();
		if (!ExecutePlayerBotBasicAttack(ch, target, state, dwNow))
			return true;

		PIXEL_POSITION center;
		center.x = target->GetX();
		center.y = target->GetY();
		center.z = 0;
		state.vecMultiPullCenters.push_back(center);
		++state.bMultiPullGroups;
		state.dwMultiPullTargetVID = 0;
		state.dwTargetVID = 0;
		state.dwNextMultiPullActionTime = dwNow + PLAYERBOT_MULTI_PULL_ACTION_DELAY;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_PULL: tagged pack pid=%u name=%s groups=%u/%u target=%s aggressors=%d hp=%d/%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)state.bMultiPullGroups,
				(unsigned int)state.bMultiPullDesiredGroups, target->GetName(),
				CountPlayerBotPullAggressors(ch), ch->GetHP(), ch->GetMaxHP());
		return true;
	}

	class CCheckNearbyHumanPlayer
	{
		public:
			CCheckNearbyHumanPlayer(LPCHARACTER owner, int maxDist) : m_owner(owner), m_maxDist(maxDist), m_bFound(false) {}
			bool operator () (LPENTITY entity)
			{
				if (!entity || !entity->IsType(ENTITY_CHARACTER))
					return true;
				LPCHARACTER ch = static_cast<LPCHARACTER>(entity);
				if (ch && ch->IsPC() && ch->GetDesc() != NULL && ch != m_owner)
				{
					if (DISTANCE_APPROX(m_owner->GetX() - ch->GetX(), m_owner->GetY() - ch->GetY()) <= m_maxDist)
					{
						m_bFound = true;
						return false; // Stop search
					}
				}
				return true;
			}
			LPCHARACTER m_owner;
			int m_maxDist;
			bool m_bFound;
	};

	bool HasPlayerBotUsableSkillBook(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || ch->GetSkillGroup() == 0)
			return false;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			if (IsPlayerBotOwnSkill(ch, skillVnum) &&
					ch->GetSkillMasterType(skillVnum) == SKILL_MASTER &&
					ch->GetSkillLevel(skillVnum) >= 20 && ch->GetSkillLevel(skillVnum) < 30)
				return true;
		}
		return false;
	}

	void PlanPlayerBotLongTermGoal(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextGoalPlanTime)
			return;
		state.dwNextGoalPlanTime = dwNow + PLAYERBOT_GOAL_PLAN_INTERVAL + number(0, 1500);

		const bool canAdvanceHorse = state.bVisitingStable ||
				ShouldPlayerBotPursueHorseExpedition(ch, dwNow);
		const bool hasBiologistMission = GetActivePlayerBotBiologistMission(ch) != NULL;
		const bool hasHuntingMission = GetActivePlayerBotHuntingMission(ch) != NULL;
		const bool canRefine = HasPlayerBotRefineOpportunity(ch);
		const bool canReadBook = HasPlayerBotUsableSkillBook(ch);
		BYTE goal = BOT_GOAL_LEVEL_UP;
		if (state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * 35))
			goal = BOT_GOAL_SURVIVE;
		else if (ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0)
			goal = BOT_GOAL_CHOOSE_PROFESSION;
		else if (ch->GetWear(WEAR_WEAPON) == NULL)
			goal = BOT_GOAL_GET_EQUIPMENT;
		else if (state.bVisitingStable)
			goal = BOT_GOAL_HORSE;
		else if (state.bVisitingShop && state.bTownNeedBlacksmith)
			goal = BOT_GOAL_REFINE;
		else if (NeedsPlayerBotPotions(ch))
			goal = BOT_GOAL_RESTOCK;
		else if (state.bAmbition == BOT_AMBITION_EQUIPMENT && canRefine)
			goal = BOT_GOAL_REFINE;
		else if (state.bAmbition == BOT_AMBITION_SKILLS && canReadBook)
			goal = BOT_GOAL_MASTER_SKILL;
		else if (state.bAmbition == BOT_AMBITION_HORSE && canAdvanceHorse)
			goal = BOT_GOAL_HORSE;
		else if (state.bAmbition == BOT_AMBITION_BIOLOGIST && hasBiologistMission)
			goal = BOT_GOAL_BIOLOGIST;
		else if (state.bAmbition == BOT_AMBITION_METINS &&
				state.bBotRole == BOT_ROLE_METIN_HUNTER)
			goal = BOT_GOAL_HUNT_METIN;
		else if (state.bBotRole == BOT_ROLE_PARTY_FIGHTER && ch->GetParty())
			goal = BOT_GOAL_PARTY_CHALLENGE;
		else if (canAdvanceHorse)
			goal = BOT_GOAL_HORSE;
		else if (hasBiologistMission && ch->GetPlayerID() % 3 != 0)
			goal = BOT_GOAL_BIOLOGIST;
		else if (hasHuntingMission)
			goal = BOT_GOAL_HUNTING;
		else if (hasBiologistMission)
			goal = BOT_GOAL_BIOLOGIST;
		else if (canRefine)
			goal = BOT_GOAL_REFINE;
		else if (canReadBook)
			goal = BOT_GOAL_MASTER_SKILL;
		else if (state.bBotRole == BOT_ROLE_METIN_HUNTER)
			goal = BOT_GOAL_HUNT_METIN;

		SetPlayerBotGoal(ch, state, goal, dwNow);
	}

	const char* GetPlayerBotGoalLabel(BYTE goal)
	{
		switch (goal)
		{
			case BOT_GOAL_SURVIVE: return "regeneracja";
			case BOT_GOAL_CHOOSE_PROFESSION: return "profesja";
			case BOT_GOAL_GET_EQUIPMENT: return "ekwipunek";
			case BOT_GOAL_RESTOCK: return "zapasy";
			case BOT_GOAL_REFINE: return "ulepszanie";
			case BOT_GOAL_MASTER_SKILL: return "rozwoj skilla";
			case BOT_GOAL_HUNT_METIN: return "Metiny";
			case BOT_GOAL_PARTY_CHALLENGE: return "silne moby PT";
			case BOT_GOAL_BIOLOGIST: return "Biolog";
			case BOT_GOAL_HUNTING: return "Polowanie";
			case BOT_GOAL_HORSE: return "rozwoj konia";
			case BOT_GOAL_FISHING: return "lowienie ryb";
			default: return "poziom";
		}
	}

	const char* GetPlayerBotActionLabel(BYTE action)
	{
		switch (action)
		{
			case BOT_ACTION_TRAVEL: return "ide";
			case BOT_ACTION_FIGHT: return "walcze";
			case BOT_ACTION_LOOT: return "zbieram";
			case BOT_ACTION_RECOVER: return "odpoczywam";
			case BOT_ACTION_TRAIN: return "wybieram profesje";
			case BOT_ACTION_SHOP: return "handluje";
			case BOT_ACTION_REFINE: return "ulepszam";
			case BOT_ACTION_READ_BOOK: return "czytam KU";
			case BOT_ACTION_SOCKET_STONE: return "wkladam KD";
			case BOT_ACTION_PARTY_ASSEMBLE: return "zbieram PT";
			case BOT_ACTION_BIOLOGIST: return "robie misje Biologa";
			case BOT_ACTION_STABLE: return "odwiedzam Stajennego";
			default: return "mysle";
		}
	}

	void SendPlayerBotOverheadChat(LPCHARACTER ch, const char* szText)
	{
		if (!ch || !szText || !szText[0] || !ch->GetSectree())
			return;

		char chatbuf[256];
		int len = snprintf(chatbuf, sizeof(chatbuf), "%s : %s", ch->GetName(), szText);
		if (len <= 0)
			return;
		if (len >= (int)sizeof(chatbuf))
			len = sizeof(chatbuf) - 1;
		// The regular talking packet contains its trailing NUL.  Keeping the packet
		// identical to a real player's chat is what makes every native/wasm client
		// render it as a text tail above the bot without a client fork.
		++len;

		TPacketGCChat pack_chat;
		pack_chat.header = HEADER_GC_CHAT;
		pack_chat.size = sizeof(TPacketGCChat) + len;
		pack_chat.type = CHAT_TYPE_TALKING;
		pack_chat.id = ch->GetVID();
		pack_chat.bEmpire = 0;

		TEMP_BUFFER buf;
		buf.write(&pack_chat, sizeof(TPacketGCChat));
		buf.write(chatbuf, len);
		ch->PacketAround(buf.read_peek(), buf.size());
	}

	const char* GetPlayerBotTownStatusLabel(const TPlayerBotAIState& state)
	{
		switch (state.bTownVisitPhase)
		{
			case BOT_TOWN_PHASE_TRAINER: return "Ide po profesje";
			case BOT_TOWN_PHASE_TRAINER_WAIT: return "Wybieram profesje";
			case BOT_TOWN_PHASE_WEAPON_MERCHANT: return "Ide do handlarza bronia";
			case BOT_TOWN_PHASE_WEAPON_WAIT: return "Handluje bronia";
			case BOT_TOWN_PHASE_ARMOR_MERCHANT: return "Ide do handlarza zbroja";
			case BOT_TOWN_PHASE_ARMOR_WAIT: return "Handluje zbroja";
			case BOT_TOWN_PHASE_MISC_MERCHANT: return "Ide do handlarki roznosci";
			case BOT_TOWN_PHASE_MISC_WAIT: return "Kupuje potki i sprzedaje lup";
			case BOT_TOWN_PHASE_BLACKSMITH: return "Ide do kowala";
			case BOT_TOWN_PHASE_BLACKSMITH_WAIT: return "Ulepszam ekwipunek";
			case BOT_TOWN_PHASE_GATE_IN:
			case BOT_TOWN_PHASE_GATE_CROSS_IN: return "Ide do miasta";
			case BOT_TOWN_PHASE_GATE_OUT:
			case BOT_TOWN_PHASE_GATE_CROSS_OUT: return "Wracam na exp";
			default: return "Zalatwiam sprawy w miescie";
		}
	}

	void BuildPlayerBotStatusText(LPCHARACTER ch, const TPlayerBotAIState& state,
			char* status, size_t statusSize)
	{
		if (!ch || !status || statusSize == 0)
			return;

		const char* prefix = ch->GetParty() ? "[PT] " : "";
		const char* goal = GetPlayerBotGoalLabel(state.bLongTermGoal);
		if (state.bVisitingShop)
		{
			snprintf(status, statusSize, "%s%s (cel: %s)", prefix,
					GetPlayerBotTownStatusLabel(state), goal);
			return;
		}

		if (state.bTacticalRetreat)
		{
			snprintf(status, statusSize, "%sUciekam - mam malo HP", prefix);
			return;
		}
		if (state.bRecoveringAfterDeath)
		{
			snprintf(status, statusSize, "%sOdpoczywam po smierci", prefix);
			return;
		}

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		switch (state.bCurrentAction)
		{
			case BOT_ACTION_FIGHT:
				if (target && target->IsStone())
					snprintf(status, statusSize, "%sRozbijam %s", prefix, target->GetName());
				else if (target && target->IsMonster())
				{
					int huntingRemaining = 0;
					const DWORD huntingMob = GetActivePlayerBotHuntingMobVnum(
							ch, &huntingRemaining);
					if (huntingMob != 0 && target->GetRaceNum() == huntingMob)
					{
						snprintf(status, statusSize, "%sPolowanie: %s (zostalo %d)",
								prefix, target->GetName(), huntingRemaining);
						break;
					}
					LPITEM weapon = ch->GetWear(WEAR_WEAPON);
					const bool bow = weapon && weapon->GetType() == ITEM_WEAPON &&
							weapon->GetSubType() == WEAPON_BOW;
					const int range = bow ? 800 : 280;
					const int distance = DISTANCE_APPROX(
							ch->GetX() - target->GetX(), ch->GetY() - target->GetY());
					if (distance > range)
						snprintf(status, statusSize, "%sGonie %s", prefix, target->GetName());
					else
						snprintf(status, statusSize, "%sWalcze z %s", prefix, target->GetName());
				}
				else
					snprintf(status, statusSize, "%sSzukam przeciwnika", prefix);
				break;
			case BOT_ACTION_LOOT:
				snprintf(status, statusSize, "%sPodnosze lup", prefix);
				break;
			case BOT_ACTION_RECOVER:
				snprintf(status, statusSize, "%sRegeneruje HP", prefix);
				break;
			case BOT_ACTION_TRAIN:
				snprintf(status, statusSize, "%sWybieram profesje", prefix);
				break;
			case BOT_ACTION_SHOP:
				snprintf(status, statusSize, "%sHandluje", prefix);
				break;
			case BOT_ACTION_REFINE:
				snprintf(status, statusSize, "%sUlepszam ekwipunek", prefix);
				break;
			case BOT_ACTION_READ_BOOK:
				snprintf(status, statusSize, "%sCzytam ksiege umiejetnosci", prefix);
				break;
			case BOT_ACTION_SOCKET_STONE:
				snprintf(status, statusSize, "%sWkladam kamien duszy", prefix);
				break;
			case BOT_ACTION_PARTY_ASSEMBLE:
				snprintf(status, statusSize, "%sSzukam celu dla grupy", prefix);
				break;
			case BOT_ACTION_BIOLOGIST:
			{
				const TPlayerBotBiologistMission* mission =
						GetActivePlayerBotBiologistMission(ch);
				if (!mission)
					snprintf(status, statusSize, "%sWracam od Biologa", prefix);
				else if (state.bVisitingBiologist &&
						DISTANCE_APPROX(ch->GetX() - PLAYERBOT_BIOLOGIST_X,
								ch->GetY() - PLAYERBOT_BIOLOGIST_Y) > 850)
					snprintf(status, statusSize, "%sIde do Biologa z: %s", prefix, mission->itemLabel);
				else if (state.bVisitingBiologist)
					snprintf(status, statusSize, "%sOddaje Biologowi: %s", prefix, mission->itemLabel);
				else
					snprintf(status, statusSize, "%sZbieram dla Biologa: %s", prefix, mission->itemLabel);
				break;
			}
			case BOT_ACTION_STABLE:
				if (DISTANCE_APPROX(ch->GetX() - PLAYERBOT_STABLE_BOY_X,
						ch->GetY() - PLAYERBOT_STABLE_BOY_Y) > 850)
					snprintf(status, statusSize, "%sIde do Stajennego z medalem", prefix);
				else
					snprintf(status, statusSize, "%sOddaje medal konny (%u/21)", prefix,
							(unsigned int)ch->GetHorseLevel());
				break;
			case BOT_ACTION_FISHING:
				if (ch->CountSpecifyItem(PLAYERBOT_FISHING_BAIT_VNUM) <
						PLAYERBOT_FISHING_BAIT_RESTOCK)
					snprintf(status, statusSize, "%sIde do Rybaka po przynete", prefix);
				else if (DISTANCE_APPROX(ch->GetX() - PLAYERBOT_FISHING_BANK_X,
						ch->GetY() - PLAYERBOT_FISHING_BANK_Y) > 850)
					snprintf(status, statusSize, "%sIde nad rzeke lowic ryby", prefix);
				else if (state.bIsFishing)
					snprintf(status, statusSize, "%sLowie ryby - czekam na branie", prefix);
				else
					snprintf(status, statusSize, "%sZakladam przynete na wedke", prefix);
				break;
			case BOT_ACTION_TRAVEL:
				if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, "%sIde przez portal do M2 po Medal Konny", prefix);
				else if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 &&
						ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) == 0 &&
						state.bLongTermGoal == BOT_GOAL_HORSE)
					snprintf(status, statusSize, "%sIde do Lochu Malp po Medal Konny", prefix);
				else if (ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) > 0)
					snprintf(status, statusSize, "%sIde do najblizszego Stajennego z Medalem", prefix);
				else if (ch->GetMapIndex() == PLAYERBOT_MAP_MONKEY_EASY)
					snprintf(status, statusSize, "%sWychodze z Lochu Malp", prefix);
				else
					snprintf(status, statusSize, "%sSzukam miejsca do expa (cel: %s)", prefix, goal);
				break;
			default:
				snprintf(status, statusSize, "%sPlanuje: %s", prefix, goal);
				break;
		}
	}

	void ManagePlayerBotStatusOverhead(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;

		const BYTE inParty = ch->GetParty() ? 1 : 0;
		const DWORD relevantTargetVID = state.bCurrentAction == BOT_ACTION_FIGHT
				? state.dwTargetVID : 0;
		const BYTE relevantTownPhase = state.bVisitingShop
				? state.bTownVisitPhase : BOT_TOWN_PHASE_NONE;
		const bool changed =
				state.bLastStatusAction != state.bCurrentAction ||
				state.bLastStatusGoal != state.bLongTermGoal ||
				state.bLastStatusTownPhase != relevantTownPhase ||
				state.bLastStatusParty != inParty ||
				state.dwLastStatusTargetVID != relevantTargetVID;
		const bool keepAliveDue = dwNow >= state.dwNextChatTime;
		if (!changed && !keepAliveDue)
			return;
		if (dwNow < state.dwNextStatusProbeTime)
			return;
		if (state.dwLastStatusChatTime != 0 &&
				dwNow - state.dwLastStatusChatTime < 2500)
		{
			state.dwNextStatusProbeTime = state.dwLastStatusChatTime + 2500;
			return;
		}

		// Do not make 350 bots fill the chat window or spend time formatting text
		// nobody can see. A player entering the area gets the current state within
		// three seconds; state changes are otherwise published immediately.
		CCheckNearbyHumanPlayer humanChecker(ch, 2500);
		if (ch->GetSectree())
			ch->GetSectree()->ForEachAround(humanChecker);
		if (!humanChecker.m_bFound)
		{
			state.dwNextStatusProbeTime = dwNow + 3000;
			return;
		}

		char szStatus[160];
		BuildPlayerBotStatusText(ch, state, szStatus, sizeof(szStatus));
		SendPlayerBotOverheadChat(ch, szStatus);
		state.dwLastStatusChatTime = dwNow;
		state.dwNextStatusProbeTime = dwNow + 2500;
		state.dwNextChatTime = dwNow + number(9000, 14000);
		state.bLastStatusAction = state.bCurrentAction;
		state.bLastStatusGoal = state.bLongTermGoal;
		state.bLastStatusTownPhase = relevantTownPhase;
		state.bLastStatusParty = inParty;
		state.dwLastStatusTargetVID = relevantTargetVID;
	}

	void ManagePlayerBotSpiritStones(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextSpiritStoneTime)
			return;
		state.dwNextSpiritStoneTime = dwNow + PLAYERBOT_SPIRIT_STONE_CHECK_INTERVAL;

		LPITEM bestStone = NULL;
		LPITEM bestGear = NULL;
		int bestSocket = -1;
		int bestScore = INT_MIN;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_METIN)
				continue;

			const DWORD kdVnum = item->GetVnum();
			const int kdPlus = (kdVnum % 10);
			const int stoneKind = kdVnum % 100;
			const bool weaponStone = stoneKind >= 30 && stoneKind <= 36;
			const bool armorStone = stoneKind >= 37 && stoneKind <= 43;
			if (!weaponStone && !armorStone)
				continue;

			LPITEM targetGear = weaponStone ? ch->GetWear(WEAR_WEAPON) : ch->GetWear(WEAR_BODY);

			if (!targetGear)
				continue;

			const int gearRefine = targetGear->GetRefineLevel();
			if (kdPlus >= 3 && gearRefine < 6)
				continue;

			for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
			{
				if (targetGear->GetSocket(socketIdx) == 1)
				{
					int score = kdPlus * 100 + targetGear->GetRefineLevel() * 10;
					// PvE stones receive priority over class-vs-class stones in offline M1.
					if (stoneKind == 30 || stoneKind == 31 || stoneKind == 32 ||
							stoneKind == 38 || stoneKind == 40 || stoneKind == 41)
						score += 500;
					if (score > bestScore)
					{
						bestScore = score;
						bestStone = item;
						bestGear = targetGear;
						bestSocket = socketIdx;
					}
					break;
				}
			}
		}

		if (bestStone && bestGear && bestSocket >= 0)
		{
			const DWORD kdVnum = bestStone->GetVnum();
			const DWORD gearVnum = bestGear->GetVnum();
			bestGear->SetSocket(bestSocket, kdVnum);
			ITEM_MANAGER::instance().RemoveItem(bestStone, "PLAYERBOT_KD");
			SetPlayerBotAction(state, BOT_ACTION_SOCKET_STONE, dwNow);
			sys_log(0, "PLAYERBOT_AI: inserted spirit stone pid=%u name=%s kd_vnum=%u into gear_vnum=%u socket=%d score=%d",
					ch->GetPlayerID(), ch->GetName(), kdVnum, gearVnum, bestSocket, bestScore);
		}
	}

	bool SharePlayerBotUsefulItemWithParty(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextPartyShareTime)
			return false;
		state.dwNextPartyShareTime = dwNow + PLAYERBOT_PARTY_SHARE_INTERVAL + number(0, 5000);

		// Reserve equipment sharing is deliberately not restricted to a party.
		// Solo bots that meet in the field may help a lower-level bot of the same
		// class/build, while all other useful-item sharing remains party-only.
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
					!IsPlayerBotEquipmentCandidate(ch, item))
				continue;

			const int wearCell = item->FindEquipCell(ch);
			LPITEM worn = wearCell >= 0 ? ch->GetWear(wearCell) : NULL;
			if (!worn || GetPlayerBotEquipmentScore(item, ch) > GetPlayerBotEquipmentScore(worn, ch))
				continue; // This is the giver's pending upgrade, not a spare.

			if (SharePlayerBotOldGearNearby(ch, item))
				return true;
		}

		if (!ch->GetParty())
			return false;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;

			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			const bool isShareableBook = skillVnum != 0 && !IsPlayerBotOwnSkill(ch, skillVnum);
			const bool isShareableMaterial =
					(item->GetType() == ITEM_MATERIAL ||
					 (item->GetVnum() >= 30000 && item->GetVnum() <= 30200)) &&
					!PlayerBotNeedsRefineMaterial(ch, item->GetVnum());
			if (!isShareableBook && !isShareableMaterial)
				continue;

			struct FUsefulItemReceiver
			{
				LPCHARACTER m_giver;
				LPITEM m_item;
				DWORD m_skillVnum;
				bool m_bMaterial;
				LPCHARACTER m_receiver;

				FUsefulItemReceiver(LPCHARACTER giver, LPITEM item, DWORD skillVnum, bool material) :
					m_giver(giver), m_item(item), m_skillVnum(skillVnum),
					m_bMaterial(material), m_receiver(NULL) {}

				void operator () (LPCHARACTER member)
				{
					if (m_receiver || !member || member == m_giver || member->IsDead() ||
							!member->GetDesc() || !member->GetDesc()->IsBot() ||
							DISTANCE_APPROX(m_giver->GetX() - member->GetX(), m_giver->GetY() - member->GetY()) > 1800 ||
							member->GetEmptyInventory(m_item->GetSize()) < 0)
						return;

					if ((!m_bMaterial && IsPlayerBotOwnSkill(member, m_skillVnum)) ||
							(m_bMaterial && PlayerBotNeedsRefineMaterial(member, m_item->GetVnum())))
						m_receiver = member;
				}
			};

			FUsefulItemReceiver finder(ch, item, skillVnum, isShareableMaterial);
			ch->GetParty()->ForEachOnMapMember(finder, ch->GetMapIndex());
			if (!finder.m_receiver)
				continue;

			const int receiverCell = finder.m_receiver->GetEmptyInventory(item->GetSize());
			const WORD oldCell = item->GetCell();
			const DWORD itemVnum = item->GetVnum();
			item->RemoveFromCharacter();
			if (receiverCell >= 0 && item->AddToCharacter(finder.m_receiver,
					TItemPos(INVENTORY, receiverCell)))
			{
				sys_log(0, "PLAYERBOT_AI: shared useful item pid=%u name=%s -> target_pid=%u target_name=%s vnum=%u kind=%s",
						ch->GetPlayerID(), ch->GetName(), finder.m_receiver->GetPlayerID(),
						finder.m_receiver->GetName(), itemVnum,
						isShareableBook ? "skill_book" : "refine_material");
				return true;
			}

			item->AddToCharacter(ch, TItemPos(INVENTORY, oldCell));
		}
		return false;
	}

	void ManagePlayerBotSkillBooks(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->GetSkillGroup() == 0 ||
				dwNow < state.dwNextSkillBookTime)
			return;
		state.dwNextSkillBookTime = dwNow + PLAYERBOT_SKILL_BOOK_CHECK_INTERVAL;

		if (SharePlayerBotUsefulItemWithParty(ch, state, dwNow))
			return;

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		int bestCell = -1;
		DWORD bestSkillVnum = 0;
		int bestPriority = INT_MIN;

		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_SKILLBOOK)
				continue;

			const DWORD skillVnum = GetPlayerBotSkillBookSkillVnum(item);
			if (!IsPlayerBotOwnSkill(ch, skillVnum))
				continue;

			const BYTE skillLevel = ch->GetSkillLevel(skillVnum);
			const BYTE masterType = ch->GetSkillMasterType(skillVnum);
			if (masterType == SKILL_MASTER && skillLevel >= 20 && skillLevel < 30)
			{
				const int priority = (skillVnum == build.dwPrimaryMaxSkill ? 10000 : 0) + skillLevel;
				if (priority > bestPriority)
				{
					bestPriority = priority;
					bestCell = cell;
					bestSkillVnum = skillVnum;
				}
			}
		}

		if (bestCell < 0 || bestSkillVnum == 0)
			return;

		if (get_global_time() < ch->GetSkillNextReadTime(bestSkillVnum))
		{
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM scroll = ch->GetInventoryItem(cell);
				if (scroll && (scroll->GetVnum() == 71001 || scroll->GetVnum() == 71094))
				{
					ch->UseItem(TItemPos(INVENTORY, cell));
					break;
				}
			}
		}

		const BYTE oldLevel = ch->GetSkillLevel(bestSkillVnum);
		if (ch->UseItem(TItemPos(INVENTORY, bestCell)))
		{
			SetPlayerBotAction(state, BOT_ACTION_READ_BOOK, dwNow);
			sys_log(0, "PLAYERBOT_AI: read skill book pid=%u name=%s skill=%u old_level=%u new_level=%u success=%d",
					ch->GetPlayerID(), ch->GetName(), bestSkillVnum, oldLevel,
					ch->GetSkillLevel(bestSkillVnum),
					ch->GetSkillLevel(bestSkillVnum) > oldLevel ? 1 : 0);
		}
	}

	bool ResetPlayerBotIfInactive(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || state.bRecoveringAfterDeath)
			return false;

		if (state.dwLastMeaningfulActivityTime == 0)
		{
			state.dwLastMeaningfulActivityTime = dwNow;
			state.lLastX = ch->GetX();
			state.lLastY = ch->GetY();
			return false;
		}

		const bool moved = DISTANCE_APPROX(
				ch->GetX() - state.lLastX, ch->GetY() - state.lLastY) >= 150;
		const bool foughtRecently = state.dwLastCombatActionTime != 0 &&
				dwNow - state.dwLastCombatActionTime <= 10000;
		const bool castRecently = state.dwLastBotSkillTime != 0 &&
				dwNow - state.dwLastBotSkillTime <= 10000;
		// An angler stands still on purpose: a single cast can wait 40 s for the
		// bite alone, so stillness at the bank is the activity, not a symptom.
		if (moved || foughtRecently || castRecently || state.bFishingSession)
		{
			state.dwLastMeaningfulActivityTime = dwNow;
			state.lLastX = ch->GetX();
			state.lLastY = ch->GetY();
			return false;
		}

		if (dwNow - state.dwLastMeaningfulActivityTime < PLAYERBOT_INACTIVITY_RESET_TIME)
			return false;

		sys_err("PLAYERBOT_WATCHDOG: resetting inactive bot pid=%u name=%s pos=(%ld,%ld) action=%u goal=%u target=%u shop=%d phase=%u bio=%d stable=%d route=%u/%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
				(unsigned int)state.bCurrentAction, (unsigned int)state.bLongTermGoal,
				state.dwTargetVID, state.bVisitingShop ? 1 : 0,
				(unsigned int)state.bTownVisitPhase, state.bVisitingBiologist ? 1 : 0,
				state.bVisitingStable ? 1 : 0, (unsigned int)state.uRouteIndex,
				(unsigned int)state.vecRoute.size());

		if (state.bVisitingShop)
			FinishPlayerBotTownVisit(ch, state, dwNow, false);

		// A leader and its nearby followers can keep each other in
		// BOT_ACTION_PARTY_ASSEMBLE after a failed shared objective.  Merely
		// clearing the route is not enough: on the next tick they immediately
		// select the same idle party state again.  Break only a party which has
		// already tripped the 90-second inactivity watchdog, then keep this bot
		// solo briefly so it can acquire an independent destination/target.
		if (ch->GetParty())
		{
			ch->GetParty()->Quit(ch->GetPlayerID());
			state.dwPartyExpireTime = 0;
			state.dwNextPartyCheckTime = dwNow + number(60000, 120000);
		}
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		state.bTacticalRetreat = false;
		state.dwRetreatThreatVID = 0;
		state.dwTargetVID = 0;
		state.dwNavFailedTargetVID = 0;
		state.bNavFailedTargetCount = 0;
		state.bStuckCounter = 0;
		state.dwNextBiologistCheckTime = dwNow + 10000;
		state.dwNextHorseCheckTime = dwNow + 10000;
		state.dwNextWanderTime = dwNow;
		state.dwNextGoalPlanTime = 0;
		state.bCurrentAction = BOT_ACTION_IDLE;
		ch->SetVictim(NULL);
		ch->Stop();
		ClearPlayerBotRoute(state, true);
		state.dwLastMeaningfulActivityTime = dwNow;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();
		return true;
	}

	EVENTINFO(playerbot_update_event_info)
	{
		CPlayerBotManager* manager;
	};

	EVENTFUNC(playerbot_update_event)
	{
		playerbot_update_event_info* info = dynamic_cast<playerbot_update_event_info*>(event->info);
		if (!info || !info->manager)
			return 0;

		info->manager->Update();
		return PASSES_PER_SEC(1) / 4;
	}

	CPlayerBotManager s_playerBotManager;
}

CPlayerBotManager::CPlayerBotManager()
	: m_bRegistryLoaded(false),
	  m_bRegistryAvailable(false)
{
}

CPlayerBotManager::~CPlayerBotManager()
{
	if (s_pkPlayerBotUpdateEvent)
		event_cancel(&s_pkPlayerBotUpdateEvent);
}

bool CPlayerBotManager::Spawn(DWORD dwPlayerID, BYTE bEmpire)
{
	if (dwPlayerID == 0 || bEmpire != 2)
		return false;

	// A bot descriptor has no authenticated account session.  Never let a raw
	// PID turn an ordinary player into a server-controlled character: only the
	// immutable cohort written by playerbots_seed.sql may use this load path.
	if (!IsRegistered(dwPlayerID))
	{
		// Expected, not exceptional: every start walks the whole pid range and most
		// of it is not seeded. Writing a SYSERR per pid put 170 lines into every
		// boot for a guard that is working exactly as intended.
		static DWORD s_dwRejectedSpawns = 0;
		static DWORD s_dwNextRejectLog = 0;
		++s_dwRejectedSpawns;
		const DWORD dwRejectNow = get_dword_time();
		if (dwRejectNow >= s_dwNextRejectLog)
		{
			s_dwNextRejectLog = dwRejectNow + 60000;
			sys_log(0, "PLAYERBOT_AUTH: refused %u unregistered spawns so far (last pid=%u empire=%u)",
					s_dwRejectedSpawns, dwPlayerID, bEmpire);
		}
		return false;
	}

	if (IsManaged(dwPlayerID) || CHARACTER_MANAGER::instance().FindByPID(dwPlayerID))
		return false;

	LPDESC d = DESC_MANAGER::instance().CreateBotDesc(bEmpire);
	if (!d)
		return false;

	m_mapBots.insert(TPlayerBotMap::value_type(dwPlayerID, d));
	m_mapHandles.insert(THandleToPlayerMap::value_type(d->GetHandle(), dwPlayerID));

	TBotPlayerLoadPacket packet;
	packet.player_id = dwPlayerID;
	packet.empire = bEmpire;

	db_clientdesc->DBPacket(HEADER_GD_BOT_PLAYER_LOAD, d->GetHandle(), &packet, sizeof(packet));
	sys_log(0, "PLAYERBOT: requested player load pid=%u empire=%u handle=%u",
			dwPlayerID, bEmpire, d->GetHandle());
	return true;
}

bool CPlayerBotManager::LoadRegisteredBots()
{
	if (m_bRegistryLoaded)
		return m_bRegistryAvailable;

	// Fail closed for this process.  A missing/corrupt ledger must leave bots
	// offline instead of falling back to the historical contiguous PID range.
	m_bRegistryLoaded = true;
	m_bRegistryAvailable = false;
	m_setRegisteredBots.clear();

	const char* query =
			"SELECT l.pid "
			"FROM common.playerbot_seed_state AS l "
			"JOIN player.player AS p ON p.id=l.pid "
			"JOIN account.account AS a ON a.id=p.account_id "
			"JOIN player.player_index AS pi ON pi.id=a.id "
			"WHERE l.seed_version=1 "
			"AND l.state IN ('complete','adopted') "
			"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,3,'0')) "
			"AND BINARY a.social_id=BINARY CONCAT('9',LPAD(l.pid-3,12,'0')) "
			"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 "
			"AND pi.empire=2 ORDER BY l.pid";

	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() ||
			!msg->Get()->pSQLResult)
	{
		sys_err("PLAYERBOT_AUTH: registry query failed; refusing every bot spawn");
		return false;
	}

	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		if (row[0])
			str_to_number(pid, row[0]);
		if (pid != 0)
			m_setRegisteredBots.insert(pid);
	}

	m_bRegistryAvailable = !m_setRegisteredBots.empty();
	if (!m_bRegistryAvailable)
	{
		sys_err("PLAYERBOT_AUTH: registry has no valid seeded identities; refusing every bot spawn");
		return false;
	}

	sys_log(0, "PLAYERBOT_AUTH: loaded %u registered bot identities",
			(unsigned int)m_setRegisteredBots.size());
	return true;
}

bool CPlayerBotManager::IsRegistered(DWORD dwPlayerID)
{
	return LoadRegisteredBots() &&
			m_setRegisteredBots.find(dwPlayerID) != m_setRegisteredBots.end();
}

size_t CPlayerBotManager::SpawnRegistered(size_t count, BYTE bEmpire)
{
	if (count == 0 || bEmpire != 2 || !LoadRegisteredBots())
		return 0;

	size_t selected = 0;
	size_t spawned = 0;
	for (TRegisteredPlayerBotSet::const_iterator it = m_setRegisteredBots.begin();
			it != m_setRegisteredBots.end() && selected < count; ++it, ++selected)
	{
		if (Spawn(*it, bEmpire))
			++spawned;
	}
	return spawned;
}

bool CPlayerBotManager::Despawn(DWORD dwPlayerID)
{
	TPlayerBotMap::iterator it = m_mapBots.find(dwPlayerID);
	if (it == m_mapBots.end())
		return false;

	LPDESC d = it->second;
	m_mapBots.erase(it);
	s_mapPlayerBotAIStates.erase(dwPlayerID);
	if (d)
		m_mapHandles.erase(d->GetHandle());

	if (d)
		DESC_MANAGER::instance().DestroyDesc(d);

	sys_log(0, "PLAYERBOT: despawned pid=%u", dwPlayerID);
	return true;
}

void CPlayerBotManager::OnPlayerLoaded(LPDESC d)
{
	if (!d || !d->IsBot() || !d->GetCharacter())
		return;

	CInputLogin input;
	input.Entergame(d, NULL);

	if (d->IsPhase(PHASE_GAME))
	{
		const DWORD dwPID = d->GetCharacter()->GetPlayerID();
		TPlayerBotAIState& state = s_mapPlayerBotAIStates[dwPID];
		state = TPlayerBotAIState();
		const DWORD now = get_dword_time();
		state.dwSpawnTime = now;
		state.dwLastMeaningfulActivityTime = now;
		state.lLastX = d->GetCharacter()->GetX();
		state.lLastY = d->GetCharacter()->GetY();

		// Keep roughly one bot in ten eligible for party play, but deliberately
		// weight Archer builds more heavily: about 30% of Archers and 7% of all
		// other builds. With eight class/build combinations this remains close to
		// the previous global population while making a five-person lure party
		// realistically obtainable.
		const bool isArcher = d->GetCharacter()->GetJob() == JOB_ASSASSIN &&
				d->GetCharacter()->GetSkillGroup() == 2;
		const DWORD partyRoll = PlayerBotNavHash(dwPID ^ 0x50415254U) % 100U;
		if ((isArcher && partyRoll < 30U) || (!isArcher && partyRoll < 7U))
			state.bBotRole = BOT_ROLE_PARTY_FIGHTER;
		else if (dwPID % 4 == 0)
			state.bBotRole = BOT_ROLE_METIN_HUNTER;
		else
			state.bBotRole = BOT_ROLE_MOB_GRINDER;
		state.bPersonality = GetPlayerBotStablePersonality(
				d->GetCharacter(), state.bBotRole);
		state.bAmbition = GetPlayerBotStableAmbition(
				d->GetCharacter(), state.bPersonality);

		state.uMetinHotspotIndex = (BYTE)(dwPID % 16);

		state.dwNextWanderTime = now + number(1000, 10000);
		state.dwNextPartyCheckTime = now + number(15000, 60000);
		state.dwNextStatCheckTime = now + number(1000, 5000);
		state.dwNextSkillCheckTime = now + number(1000, 5000);
		state.dwNextSkillBookTime = now + number(3000, 12000);
		state.dwNextSpiritStoneTime = now + number(3000, 15000);
		state.dwNextInventoryMaintenanceTime = now + number(
				PLAYERBOT_INVENTORY_MAINTENANCE_MIN,
				PLAYERBOT_INVENTORY_MAINTENANCE_MAX);
		state.dwNextPartyShareTime = now + number(10000, 30000);
		state.dwNextGoalPlanTime = now + number(1000, 5000);
		state.dwNextEquipmentCheckTime = now + number(1000, 5000);
		state.dwNextShopCheckTime = now + number(180000, 480000);

		if (!s_pkPlayerBotUpdateEvent)
		{
			CPlayerBotNavigation::instance(d->GetCharacter()->GetMapIndex()).Init(
					d->GetCharacter()->GetMapIndex());
			playerbot_update_event_info* info = AllocEventInfo<playerbot_update_event_info>();
			info->manager = this;
			s_pkPlayerBotUpdateEvent = event_create(playerbot_update_event, info, PASSES_PER_SEC(1));
		}

		sys_log(0, "PLAYERBOT: entered game pid=%u name=%s role=%u personality=%u ambition=%u map=%ld",
				d->GetCharacter()->GetPlayerID(), d->GetCharacter()->GetName(),
				(unsigned int)state.bBotRole, (unsigned int)state.bPersonality,
				(unsigned int)state.bAmbition, d->GetCharacter()->GetMapIndex());
	}
}

void CPlayerBotManager::OnLoadFailed(DWORD dwHandle)
{
	THandleToPlayerMap::iterator it = m_mapHandles.find(dwHandle);
	if (it == m_mapHandles.end())
		return;

	DWORD dwPlayerID = it->second;
	LPDESC d = DESC_MANAGER::instance().FindByHandle(dwHandle);
	m_mapHandles.erase(it);
	m_mapBots.erase(dwPlayerID);
	s_mapPlayerBotAIStates.erase(dwPlayerID);

	if (d)
		DESC_MANAGER::instance().DestroyDesc(d);

	sys_err("PLAYERBOT: player load failed pid=%u handle=%u", dwPlayerID, dwHandle);
}

void CPlayerBotManager::OnDescriptorDestroyed(LPDESC d)
{
	if (!d || !d->IsBot())
		return;

	THandleToPlayerMap::iterator hit = m_mapHandles.find(d->GetHandle());
	if (hit != m_mapHandles.end())
	{
		s_mapPlayerBotAIStates.erase(hit->second);
		m_mapBots.erase(hit->second);
		m_mapHandles.erase(hit);
		return;
	}

	for (TPlayerBotMap::iterator it = m_mapBots.begin(); it != m_mapBots.end(); ++it)
	{
		if (it->second == d)
		{
			s_mapPlayerBotAIStates.erase(it->first);
			m_mapBots.erase(it);
			return;
		}
	}
}

void CPlayerBotManager::Update()
{
	const DWORD dwNow = get_dword_time();

	static DWORD s_dwTick = 0;
	++s_dwTick;

	for (TPlayerBotMap::iterator it = m_mapBots.begin(); it != m_mapBots.end(); ++it)
	{
		LPDESC d = it->second;
		if (!d)
			continue;

		LPCHARACTER ch = d->GetCharacter();
		if (!ch)
			continue;

		TPlayerBotAIState& state = s_mapPlayerBotAIStates[it->first];
		// Actions are set in many branches that deliberately end the current AI
		// tick early. Publishing at the beginning of the next tick keeps the UI
		// independent of those branches and still makes every change visible in
		// at most one second.
		if (d->IsPhase(PHASE_GAME) && !ch->IsDead())
			ManagePlayerBotStatusOverhead(ch, state, dwNow);

		// Keep expensive decisions staggered over two ticks, but let an already
		// engaged bot continue its basic combo on the intervening tick.  This makes
		// combat look like holding Space without doubling pathfinding/target scans.
		if ((it->first + s_dwTick) % 2 != 0)
		{
			if (d->IsPhase(PHASE_GAME) && !ch->IsDead())
			{
				// Heavy target selection/path planning stays staggered, but following an
				// already computed route is cheap. Advancing it every second prevents
				// fast characters from stopping at a 7 m waypoint until their next
				// full AI tick.
				if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size() &&
						state.lRouteMapIndex == ch->GetMapIndex())
					MovePlayerBot(ch, state.lRouteDestX, state.lRouteDestY, dwNow, 32, true,
							state.bRouteAllowsHorse);
				LPCHARACTER quickTarget = state.dwTargetVID != 0
						? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
				ExecutePlayerBotBasicAttack(ch, quickTarget, state, dwNow);
			}
			continue;
		}

		PersistPlayerBot(ch, state, dwNow);
		if (HandleDeath(ch, state, dwNow))
			continue;

		if (!d->IsPhase(PHASE_GAME))
			continue;

		// Before anything that can claim the tick. An open stall is engine state
		// with a deadline this manager owns, so releasing it must not depend on
		// which subsystem happens to win the tick - that dependency is why stalls
		// were left standing with their sign over the keeper's head.
		if (ManagePlayerBotShopLifetime(ch, state, dwNow))
			continue;

		if (ch->IsItemLoaded() && dwNow >= state.dwNextInventoryMaintenanceTime)
		{
			CompactPlayerBotPotionStacks(ch);
			state.dwNextInventoryMaintenanceTime = dwNow + number(
					PLAYERBOT_INVENTORY_MAINTENANCE_MIN,
					PLAYERBOT_INVENTORY_MAINTENANCE_MAX);
		}

		// Independent safety net for stale goals/state machines. It does not move or
		// teleport healthy bots; only 90 seconds without travel, attacks or skills
		// clears transient state so the next tick can choose a fresh goal.
		if (ResetPlayerBotIfInactive(ch, state, dwNow))
			continue;

		if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 ||
				ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2 ||
				ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M3 ||
				ch->GetMapIndex() == PLAYERBOT_MAP_MONKEY_EASY ||
				IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			const long currentMap = ch->GetMapIndex();
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(
					currentMap);
			navigation.Init(currentMap);
			const bool bOutOfBounds = !navigation.IsInsideWorld(ch->GetX(), ch->GetY());
			const bool bCrossingJoanGate = currentMap == PLAYERBOT_MAP_CHUNJO_M1 &&
					state.bVisitingShop && ch->GetX() >= 59500 && ch->GetX() <= 61100 &&
					ch->GetY() >= 169050 && ch->GetY() <= 170750;
			const bool bInsideObstacle = !bOutOfBounds &&
					!bCrossingJoanGate &&
					IsPlayerBotPositionBlocked(currentMap, ch->GetX(), ch->GetY());

			if (bOutOfBounds || bInsideObstacle)
			{
				const long oldX = ch->GetX();
				const long oldY = ch->GetY();
				PIXEL_POSITION safe;
				bool foundSafe = false;
				if (bInsideObstacle)
					foundSafe = navigation.FindNearestWalkableWorld(
							ch->GetX(), ch->GetY(), 20, safe, ch->GetPlayerID());
				if (!foundSafe)
				{
					long fallbackX = 60600;
					long fallbackY = 170900;
					if (currentMap == PLAYERBOT_MAP_CHUNJO_M2)
					{
						fallbackX = PLAYERBOT_M2_ARRIVAL_X;
						fallbackY = PLAYERBOT_M2_ARRIVAL_Y;
					}
					else if (currentMap == PLAYERBOT_MAP_CHUNJO_M3)
					{
						fallbackX = PLAYERBOT_M3_ARRIVAL_X;
						fallbackY = PLAYERBOT_M3_ARRIVAL_Y;
					}
					else if (currentMap == PLAYERBOT_MAP_MONKEY_EASY)
					{
						fallbackX = PLAYERBOT_MONKEY_EASY_ARRIVAL_X;
						fallbackY = PLAYERBOT_MONKEY_EASY_ARRIVAL_Y;
					}
					else if (currentMap == PLAYERBOT_MAP_ORC_VALLEY)
					{
						fallbackX = PLAYERBOT_ORC_VALLEY_ARRIVAL_X;
						fallbackY = PLAYERBOT_ORC_VALLEY_ARRIVAL_Y;
					}
					else if (currentMap == PLAYERBOT_MAP_DESERT)
					{
						fallbackX = PLAYERBOT_DESERT_ARRIVAL_X;
						fallbackY = PLAYERBOT_DESERT_ARRIVAL_Y;
					}
					foundSafe = navigation.FindNearestWalkableWorld(
							fallbackX, fallbackY, 30, safe, ch->GetPlayerID());
				}
				if (!foundSafe)
					continue;

				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
				state.bStuckCounter = 0;
				ClearPlayerBotRoute(state, true);
				state.lLastX = safe.x;
				state.lLastY = safe.y;
				ch->Show(currentMap, safe.x, safe.y, 0);
				ch->Stop();
				ch->SendMovePacket(FUNC_MOVE, 0, safe.x, safe.y, 0, dwNow);
				sys_err("PLAYERBOT_NAV: locally rescued pid=%u name=%s reason=%s from=(%ld,%ld) to=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), bOutOfBounds ? "bounds" : "blocked",
						oldX, oldY, safe.x, safe.y);
				continue;
			}
		}

		ManagePlayerBotStats(ch, state, dwNow);
		ManagePlayerBotSkills(ch, state, dwNow);
		if (RescuePlayerBotWithoutSectree(ch, state, dwNow))
			continue;

		if (ManagePlayerBotPrivateShop(ch, state, dwNow))
			continue;

		ManagePlayerBotSkillBooks(ch, state, dwNow);
		ManagePlayerBotSpiritStones(ch, state, dwNow);
		ManagePlayerBotParty(ch, state, dwNow);
		// The regular levelup.quest opens a selection dialog. A fake descriptor
		// cannot press its Confirm button, so accept/claim that official mission
		// here while leaving kill counting to the normal quest event.
		ManagePlayerBotHuntingProgress(ch);
		// Apprentice Chests are useful even when a weapon is already equipped. Open
		// one eligible box between fights, then let the ordinary equipment scoring
		// choose its best helmet, shield, boots, armour and weapon.
		if (ManagePlayerBotProgressionChests(ch, state, dwNow))
			continue;
		PlanPlayerBotLongTermGoal(ch, state, dwNow);

		// Trigger Town Visit (Full inventory, out of potions, or missing weapon)
		// Only trigger when NOT in the middle of fighting an active Metin stone!
		LPCHARACTER curTarget = state.dwTargetVID != 0 ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (curTarget && curTarget->IsStone() && !curTarget->IsDead() &&
				!IsPlayerBotMetinWorthFighting(ch, curTarget))
		{
			ReleasePlayerBotMetinReservation(ch, curTarget);
			sys_log(0, "PLAYERBOT_METIN: skipped obsolete stone pid=%u name=%s level=%u stone=%s stone_level=%u",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					curTarget->GetName(), curTarget->GetLevel());
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			ResetPlayerBotStoneProgress(state);
			curTarget = NULL;
		}
		bool bFightingMetin = (curTarget && curTarget->IsStone() && !curTarget->IsDead());
		if (bFightingMetin &&
				ShouldPlayerBotAbandonStone(ch, curTarget, state, dwNow))
		{
			curTarget = NULL;
			bFightingMetin = false;
		}
		else if (!bFightingMetin && state.dwStoneProgressVID != 0)
		{
			ResetPlayerBotStoneProgress(state);
		}

		const bool bNeedsProfession = ch->GetLevel() >= 5 && ch->GetSkillGroup() == 0;
		// Losing essential gear at the real blacksmith is urgent. Do not leave the
		// bot fighting with a starter weapon until the ordinary 3-8 minute shop
		// timer expires; begin another visible merchant trip immediately.
		const bool bNeedsCoreGear = ch->IsItemLoaded() &&
				(NeedsPlayerBotProgressionWeapon(ch) ||
				 NeedsPlayerBotProgressionArmor(ch) ||
				 NeedsPlayerBotProgressionShield(ch) ||
				 NeedsPlayerBotProgressionHelmet(ch) ||
				 NeedsPlayerBotProgressionBoots(ch));

		// Exactly one loot decision per full AI pass. HandleLoot performs a
		// non-blocking, throttled Z-style pickup in combat and returns false, while
		// peaceful loot may take ownership of this tick and walk to the drop.
		if (HandleLoot(ch, state, dwNow))
			continue;

		// Horse medals are equally real resources: a bot leaves combat, walks to
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotHorse(ch, state, dwNow))
			continue;

		// A handful of M1 bots fish the riverbank instead of grinding. This owns
		// the whole tick: the rod sits in the weapon slot, so combat and the gear
		// pass below must not run while a session is live.
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotFishing(ch, state, dwNow))
			continue;

		// Move between the real Chunjo portals in controlled, staggered waves.
		// M2, M3 and the empire-specific easy Monkey Dungeon share this core, so
		// map changes remain visible to native desktop clients.
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotWorldTravel(ch, state, dwNow))
			continue;

		// Research is a first-class activity, not an instant reward. A bot that
		// has collected the outstanding specimens walks to Chaegirab and submits
		// them one by one before it resumes hunting.
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotBiologist(ch, state, dwNow))
			continue;

		// Missing/progression gear starts the first visit immediately because the
		// shop timer is zero after login.  Once a visit finishes, however, respect
		// its 5-10 minute retry cooldown.  Otherwise a bot that cannot yet afford
		// the next tier loops forever between the weapon and armour merchants and
		// never returns to combat (or to its local party).
		const bool bOnTownMap = ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 ||
				ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2;
		if (bOnTownMap && !state.bVisitingShop && !state.bMultiPullActive &&
				!bFightingMetin &&
				(bNeedsProfession || dwNow > state.dwNextShopCheckTime))
		{
			size_t occupiedItems = 0;
			size_t occupiedGridCells = 0;
			size_t hpPots = 0;
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM it = ch->GetInventoryItem(cell);
				if (it)
				{
					++occupiedItems;
					occupiedGridCells += std::max(1, (int)it->GetSize());
					const DWORD v = it->GetVnum();
					if (v == 27001 || v == 27002 || v == 27003 || v == 27051)
						++hpPots;
				}
			}

			const bool bWeaponMissing = (ch->GetWear(WEAR_WEAPON) == NULL);
			// Item count is not inventory usage: weapons and armour occupy 2-3
			// vertical cells.  Keep a generous reserve for a high-rate Metin drop and
			// visit town before no contiguous 3-cell slot remains.
			const bool bInventoryFull =
					occupiedGridCells * 100 >= INVENTORY_MAX_NUM * 45 ||
					ch->GetEmptyInventory(3) < 0;
			const bool bOutPotions = (hpPots == 0 && occupiedItems >= 15);
			const bool bNeedsRefine = HasPlayerBotRefineOpportunity(ch);
			const bool bNeedsGearUpgrade = bNeedsCoreGear || NeedsPlayerBotArrows(ch);
			const bool bNeedsSellRun = CountPlayerBotJunkItems(ch) >= 12;
			const bool bNeedsPotionCleanup = HasPlayerBotExcessPotions(ch);

			if (bNeedsProfession || bInventoryFull || bOutPotions || bWeaponMissing ||
					bNeedsRefine || bNeedsGearUpgrade || bNeedsSellRun ||
					bNeedsPotionCleanup)
				StartPlayerBotTownVisit(ch, state, dwNow);
		}

		// A visit is an adaptive, persistent route. The bot only visits specialists
		// needed by its current inventory: weapon merchant, armor merchant, Misc
		// Merchant and/or blacksmith. Goals never change in the middle of a route.
		if (HandlePlayerBotTownVisit(ch, state, dwNow))
			continue;

		// A normal horse is for transport only. Dismount before target selection,
		// buffs and combat so level-1 horses never produce mounted attack attempts.
		// Combat-horse behaviour will be introduced separately at horse level 11.
		if (ch->IsRiding() &&
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "combat_ready"))
			continue;

		if (!PrepareWeapon(ch, state, dwNow))
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (state.dwEmergencyScavengeUntil != 0 &&
					dwNow < state.dwEmergencyScavengeUntil &&
					ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1)
			{
				// HandleLoot above collects any ownerless nearby drop. Wander between
				// hunting hubs so the next scans cover new ground instead of idling at
				// the Weapon Merchant forever.
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				ManagePlayerBotWandering(ch, state, dwNow);
			}
			else
			{
				state.dwEmergencyScavengeUntil = 0;
				StartPlayerBotTownVisit(ch, state, dwNow);
				ch->Stop();
			}
			continue;
		}

		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		UseUtilityPotions(ch, state, dwNow);
		// This also catches a bot loaded from the database at critically low HP
		// after a server restart.  Do not let it immediately reacquire a target.
		if (!state.bRecoveringAfterDeath && ch->GetMaxHP() > 0 &&
				ch->GetHP() * 100 <= ch->GetMaxHP() * PLAYERBOT_RECOVERY_INITIAL_HP_PERCENT)
		{
			state.bRecoveringAfterDeath = true;
			state.dwLastDeathTime = dwNow;
			state.lDeathX = ch->GetX();
			state.lDeathY = ch->GetY();
			state.dwNextRecoveryProtectionTime = 0;
			state.dwNextRecoveryHealTime = dwNow;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_AI: emergency recovery started pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
		}
		if (HandlePostDeathRecovery(ch, state, dwNow))
			continue;

		LPCHARACTER retreatThreat = state.dwRetreatThreatVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwRetreatThreatVID)
				: (state.dwTargetVID != 0 ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL);
		if (!state.bTacticalRetreat && retreatThreat && retreatThreat->IsMonster() &&
				!retreatThreat->IsDead() && ch->GetMaxHP() > 0 &&
				ch->GetHP() * 100 <= ch->GetMaxHP() * PLAYERBOT_RETREAT_START_HP_PERCENT)
			StartPlayerBotTacticalRetreat(ch, state, retreatThreat, dwNow);
		if (HandlePlayerBotTacticalRetreat(ch, state, dwNow))
			continue;

		const bool bMissingCoreWearSlot = ch->GetWear(WEAR_WEAPON) == NULL ||
				ch->GetWear(WEAR_BODY) == NULL || ch->GetWear(WEAR_SHIELD) == NULL ||
				ch->GetWear(WEAR_HEAD) == NULL || ch->GetWear(WEAR_FOOTS) == NULL;
		if (ManagePlayerBotEquipment(ch, state, dwNow))
			continue;
		if (bMissingCoreWearSlot && state.bEquipPending)
		{
			// A continuous attack cadence never left the 1.7 s native equipment
			// window open. Pause only when a usable item for a missing core slot is
			// already waiting in the inventory, then equip it on the next update.
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			continue;
		}

		// A buff is a complete action for this AI update.  Continuing into the
		// attack code used to emit a second skill packet in the very same tick.
		if (ManagePlayerBotCombatBuffs(ch, state, dwNow))
			continue;
		if (HandlePlayerBotMultiPull(ch, state, dwNow))
			continue;

		LPCHARACTER target = state.dwTargetVID != 0
			? CHARACTER_MANAGER::instance().Find(state.dwTargetVID)
			: NULL;

		const bool bRecentDeath = (state.dwLastDeathTime != 0 && (dwNow - state.dwLastDeathTime < 60000));
		LPCHARACTER partyFocus = FindPlayerBotPartyFocusTarget(ch, state, dwNow);
		if (partyFocus && partyFocus != target)
		{
			TPlayerBotPartyStrength partyStrength;
			CanPlayerBotPartyChallenge(ch, partyFocus, dwNow, &partyStrength);
			target = partyFocus;
			state.dwTargetVID = (DWORD)target->GetVID();
			if (target->IsStone())
				ReservePlayerBotMetin(ch, target, dwNow);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_PARTY: assist pid=%u name=%s target_vid=%u target=%s target_level=%u ready=%d power_levels=%d cap=%d",
					ch->GetPlayerID(), ch->GetName(), state.dwTargetVID, target->GetName(),
					target->GetLevel(), partyStrength.iReadyMembers,
					partyStrength.iTotalLevels, partyStrength.iChallengeMaxLevel);
		}
		const bool bTargetIsStone = (target && target->IsStone());
		const bool bTargetIsMonster = (target && target->IsMonster());
		const bool bTargetNeedsParty = bTargetIsMonster &&
				target->GetLevel() > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA;
		const bool bPartyCanContinue = !bTargetNeedsParty ||
				CanPlayerBotPartyChallenge(ch, target, dwNow, NULL);

		if (!target || target->IsDead() || (!bTargetIsMonster && !bTargetIsStone) ||
			(bTargetIsStone && !IsPlayerBotMetinWorthFighting(ch, target)) ||
			!bPartyCanContinue ||
			IsPlayerBotSafeZone(ch->GetMapIndex(), target ? target->GetX() : ch->GetX(),
					target ? target->GetY() : ch->GetY()) ||
			target->GetMapIndex() != ch->GetMapIndex() ||
			DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY()) > PLAYERBOT_SEARCH_RANGE)
		{
			// Finish the group which is already fighting this bot (or its party)
			// before choosing a fresh, possibly distant spawn. This is the server-side
			// equivalent of a player clearing the pulled pack first.
			target = FindPlayerBotEngagedTarget(ch);
			if (!target)
				target = FindDistributedTarget(ch, state, dwNow);
			state.dwTargetVID = target ? (DWORD)target->GetVID() : 0;

			if (target)
			{
				if (target->IsStone())
					ReservePlayerBotMetin(ch, target, dwNow);
				sys_log(1, "PLAYERBOT_AI: target acquired pid=%u name=%s level=%u target_vid=%u target=%s target_level=%u is_stone=%d recent_death=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), state.dwTargetVID,
						target->GetName(), target->GetLevel(), target->IsStone() ? 1 : 0, bRecentDeath ? 1 : 0);
				if (target->IsMonster() && target->GetLevel() > ch->GetLevel() + PLAYERBOT_MAX_TARGET_LEVEL_DELTA)
				{
					TPlayerBotPartyStrength acquiredStrength;
					if (CanPlayerBotPartyChallenge(ch, target, dwNow, &acquiredStrength))
						sys_log(0, "PLAYERBOT_PARTY: leader challenge pid=%u name=%s target_vid=%u target=%s target_level=%u ready=%d power_levels=%d cap=%d",
								ch->GetPlayerID(), ch->GetName(), state.dwTargetVID, target->GetName(), target->GetLevel(),
								acquiredStrength.iReadyMembers, acquiredStrength.iTotalLevels,
								acquiredStrength.iChallengeMaxLevel);
				}
			}
		}

		if (!target)
		{
			ch->SetVictim(NULL);
			// A wander timer chosen before the last fight must not create an idle gap
			// after this pack dies. Existing routes are still advanced first inside
			// ManagePlayerBotWandering; only an idle bot plans a fresh scouting leg.
			state.dwNextWanderTime = dwNow;
			ManagePlayerBotWandering(ch, state, dwNow);
			continue;
		}
		if (state.dwNavFailedTargetVID != 0 &&
				state.dwNavFailedTargetVID != (DWORD)target->GetVID())
		{
			state.dwNavFailedTargetVID = 0;
			state.bNavFailedTargetCount = 0;
		}

		ch->SetVictim(target);
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		ch->SetRotationToXY(target->GetX(), target->GetY());
		const int distance = DISTANCE_APPROX(
				ch->GetX() - target->GetX(),
				ch->GetY() - target->GetY());

		LPITEM equippedWeapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (equippedWeapon && equippedWeapon->GetType() == ITEM_WEAPON && equippedWeapon->GetSubType() == WEAPON_BOW);
		const int combatRange = isBow ? 800 : 280;
		// A battle-horse rider closes on Metins (and, for warriors/suras, mob spots)
		// without dismounting so the fight happens from the saddle. Everyone else
		// keeps the previous on-foot approach.
		const bool fightOnHorse = CanPlayerBotFightOnHorse(ch, target);

		if (distance > combatRange)
		{
			if (!MovePlayerBot(ch, target->GetX(), target->GetY(), dwNow, 4, false,
					fightOnHorse, fightOnHorse))
			{
				const DWORD failedVID = (DWORD)target->GetVID();
				if (state.dwNavFailedTargetVID == failedVID)
				{
					if (state.bNavFailedTargetCount < 255)
						++state.bNavFailedTargetCount;
				}
				else
				{
					state.dwNavFailedTargetVID = failedVID;
					state.bNavFailedTargetCount = 1;
				}

				// A moving monster changes its coordinates often enough to look like
				// a new movement goal.  Count failures by VID instead of by coordinates,
				// otherwise a monster behind a wall can keep one bot busy forever.
				if (state.bNavFailedTargetCount >= 3)
				{
					state.mapFailedTargets[failedVID] = dwNow + 30000;
					state.dwTargetVID = 0;
					state.dwNavFailedTargetVID = 0;
					state.bNavFailedTargetCount = 0;
					ch->SetVictim(NULL);
					ClearPlayerBotRoute(state, true);
				}
				continue;
			}
			continue;
		}
		state.dwNavFailedTargetVID = 0;
		state.bNavFailedTargetCount = 0;

		if (ch->IsStateMove())
			ch->Stop();

		ch->SetPosition(POS_FIGHTING);
		ch->SetRotationToXY(target->GetX(), target->GetY());

		// In a compact party of five or more, an Archer periodically tags one
		// additional nearby pack before returning to the shared focus target.
		if (ExecutePlayerBotArcherLuring(ch, state, dwNow))
			continue;

		if (ExecutePlayerBotAttackSkill(ch, target, state, dwNow))
			continue;

		ExecutePlayerBotBasicAttack(ch, target, state, dwNow);

	}

	// Publish one compact, atomic snapshot per game core. The web panel reads
	// these files from the shared read-only game-var volume, so it sees the real
	// AI decision instead of inferring an activity from party membership or PID.
	static DWORD s_dwNextStatusSnapshotTime = 0;
	if (dwNow >= s_dwNextStatusSnapshotTime)
	{
		s_dwNextStatusSnapshotTime = dwNow + PLAYERBOT_STATUS_SNAPSHOT_INTERVAL;
		const char* tempPath = "playerbot_status.tsv.tmp";
		const char* finalPath = "playerbot_status.tsv";
		FILE* snapshot = fopen(tempPath, "wb");
		if (snapshot)
		{
			fprintf(snapshot, "pid\tpersonality\tambition\trole\tin_party\tgoal\taction\tupdated_ms\tmap\tx\ty\thp\tmax_hp\tstatus\n");
			for (TPlayerBotMap::const_iterator statusIt = m_mapBots.begin();
					statusIt != m_mapBots.end(); ++statusIt)
			{
				LPDESC statusDesc = statusIt->second;
				LPCHARACTER statusCh = statusDesc ? statusDesc->GetCharacter() : NULL;
				TPlayerBotAIStateMap::const_iterator aiIt =
						s_mapPlayerBotAIStates.find(statusIt->first);
				if (!statusCh || !statusDesc->IsPhase(PHASE_GAME) ||
						aiIt == s_mapPlayerBotAIStates.end())
					continue;

				const TPlayerBotAIState& statusState = aiIt->second;
				char statusText[192];
				if (statusCh->IsDead())
					snprintf(statusText, sizeof(statusText), "Nieprzytomny - czekam na wstanie");
				else
					BuildPlayerBotStatusText(statusCh, statusState,
							statusText, sizeof(statusText));
				for (char* p = statusText; *p; ++p)
				{
					if (*p == '\t' || *p == '\r' || *p == '\n')
						*p = ' ';
				}

				fprintf(snapshot, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%ld\t%ld\t%ld\t%d\t%d\t%s\n",
						statusCh->GetPlayerID(), (unsigned int)statusState.bPersonality,
						(unsigned int)statusState.bAmbition, (unsigned int)statusState.bBotRole,
						statusCh->GetParty() ? 1U : 0U,
						(unsigned int)statusState.bLongTermGoal,
						(unsigned int)statusState.bCurrentAction, (unsigned int)dwNow,
						statusCh->GetMapIndex(), statusCh->GetX(), statusCh->GetY(),
						statusCh->GetHP(), statusCh->GetMaxHP(), statusText);
			}
			fflush(snapshot);
			fclose(snapshot);
			if (rename(tempPath, finalPath) != 0)
				remove(tempPath);
		}
	}
}

bool CPlayerBotManager::IsManaged(DWORD dwPlayerID) const
{
	return m_mapBots.find(dwPlayerID) != m_mapBots.end();
}

size_t CPlayerBotManager::GetCount() const
{
	return m_mapBots.size();
}
