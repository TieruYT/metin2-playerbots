#ifndef __INC_METIN2_PLAYERBOT_NAVIGATION_H__
#define __INC_METIN2_PLAYERBOT_NAVIGATION_H__

// Where a bot may stand, and how it gets from one place to another.
//
// Same kind of file as playerbot_types.h: an implementation fragment, not a
// normal header. It defines objects, it relies on the engine headers
// playerbot_manager.cpp includes above it, and its anonymous namespace is
// deliberately the same one the manager reopens -- in a single translation
// unit those merge. Include it exactly once, from playerbot_manager.cpp.
//
// Nothing here knows what a bot wants. It answers only questions about the
// world: is this cell standable, can these two points reach each other, what
// is the route. That is why it is the one subsystem that can be lifted out
// whole -- it calls nothing above it.

namespace
{
	// ------------------------------------------------------------------------
	// Playerbot navigation
	//
	// The stock CHARACTER::Goto() does not perform collision checks, so every
	// segment handed to it has to be proven safe here first. That is the whole
	// reason this exists: a route is emitted only once each of its segments has
	// been validated against server_attr, at the native 50-unit resolution so
	// that narrow bridges survive and a character standing on a valid cell can
	// always attach to the planning graph.
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

	DWORD PlayerBotNavHash(DWORD value)
	{
		value ^= value >> 16;
		value *= 0x7feb352dU;
		value ^= value >> 15;
		value *= 0x846ca68bU;
		value ^= value >> 16;
		return value;
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
}

#endif
