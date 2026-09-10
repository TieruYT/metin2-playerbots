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
	// How far the goal of a portal walk may be snapped, in cells.
	//
	// It was twenty-four - twelve hundred world units - against a switch
	// distance of two hundred, so a route could legitimately end a thousand
	// units short of the portal and the arrival test could never pass. The bot
	// stood at the end of its route, the walk timed out after twenty seconds,
	// and the next tick planned the same thing again: "portal walk stalled ...
	// distance=323" a hundred times a minute, bots piled at the Bokjung
	// teleporter, and of a hundred and fifteen that set off across the desert
	// for the Spider Dungeon not one ever arrived.
	//
	// This is the trap MovePlayerBotTownLeg already sprang once and the same
	// rule closes it: a snapped goal must stay inside the radius that tests
	// arrival. Half of that radius, so the walk still has somewhere to land if
	// the exact cell is occupied - every portal point in this world stands on
	// open ground, checked against server_attr, so it rarely has to.
	const int PLAYERBOT_PORTAL_SNAP_CELLS =
			PLAYERBOT_PORTAL_SWITCH_DISTANCE / PLAYERBOT_NAV_CELL / 2;
	// What a step through water costs on top of the ordinary ten. High enough
	// that a bot walks round a lake it could wade across, low enough that a
	// twelve-cell bridge - which is the only way off an island - is never worth
	// refusing: crossing one costs 240 against a detour measured in thousands.
	const int PLAYERBOT_NAV_WATER_PENALTY = 20;
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
	// How far to look for ground a character stuck inside scenery can step onto.
	// Three cells is a hundred and fifty units - wide enough for the doorway,
	// the plinth and the shop awning bots were found welded to, and narrow
	// enough that the step is a step and not a teleport.
	const int PLAYERBOT_NAV_ESCAPE_CELLS = 3;
	const int PLAYERBOT_NAV_GOAL_REPLAN_DISTANCE = 400;
	// A parked route is worth keeping from this many waypoints left, and is
	// resumed from a waypoint within this reach of where the fight ended.
	const size_t PLAYERBOT_NAV_PARK_MIN_WAYPOINTS = 6;
	const int PLAYERBOT_NAV_RESUME_DISTANCE = 2500;
	// This is one global budget for the manager update, not one budget per map.
	// Giving M1, M2, M3 and the Monkey Dungeon 64 searches each multiplied the
	// old M1 load by four. Already built routes still advance every update; only
	// new expensive HPA/A* requests wait for a later staggered slot.
	const int PLAYERBOT_NAV_MAX_HEAVY_PLANS_PER_TICK = 32;
	// What a plan costs against that budget, by distance bucket - because the
	// comment below says a count is the wrong unit and the code then charged
	// every plan one slot anyway.
	//
	// Measured over a minute at 839 bots: 7086 of 7440 plans were sub-64-cell
	// hops to the next monster, costing 41 milliseconds between them, while 148
	// long ones cost eleven of the twelve seconds. The hops filled the tick and
	// the long plans were the ones turned away - half of every request deferred,
	// and the walks to a portal starved outright: a bot was found waiting
	// thirteen minutes for a route, standing at the Sohan exit mounting and
	// dismounting its horse every twenty seconds. A hop is free now and distance
	// pays; the microsecond budget below still bounds the whole tick.
	const int PLAYERBOT_NAV_PLAN_COST[4] = { 0, 1, 3, 8 };
	// A request nobody has answered for this many attempts stops queueing behind
	// the count. It still waits for the microsecond budget and for the far-plan
	// minute, so this cannot open a hole - it only stops one bot being last in
	// the queue for ever, which is what a fair queue exists to prevent and what
	// this world did not have.
	const BYTE PLAYERBOT_NAV_STARVED_ATTEMPTS = 20;
	const int PLAYERBOT_NAV_MAX_EXPANDED_NODES = 120000;
	// A count is the wrong unit for the budget: a hop to the next monster plans
	// in a fifth of a millisecond and a crossing of Orc Valley in eighty, so
	// thirty-two of the latter held the whole core for half a second and every
	// player on it felt the tick. The clock is the budget now, alongside the
	// count: once a tick has spent this long planning, the rest of the requests
	// take the deferred path and come back within two seconds. Fifty
	// milliseconds four times a second is a fifth of the core at the most;
	// the population asks for a tenth in the steady state, so this binds only
	// in the minute after a start, which is exactly when it should.
	const DWORD PLAYERBOT_NAV_PLAN_TIME_BUDGET_US = 50000;
	// The tick budget cannot stop a plan that has already started, and a far
	// plan on Orc Valley runs 150-700 ms: a hundred and twenty of them a
	// minute were twenty-five seconds of every sixty at 850 bots, with the
	// tick at 28 s and the core at 55%. Past this many far plans in a minute
	// the rest take the deferred path - the bot stands a second or two and
	// asks again - so the worst minute costs what this many plans cost.
	const int PLAYERBOT_NAV_MAX_FAR_PLANS_PER_MINUTE = 80;
	// The corridor search weighs its heuristic double, which is already a
	// greedy search. A corridor this many regions long - sixteen cells each,
	// so forty is a walk of six hundred cells - takes triple: the route bends
	// a little more and the search expands a third as much.
	const int PLAYERBOT_NAV_GREEDY_CORRIDOR_REGIONS = 40;
	const int PLAYERBOT_NAV_GREEDY_WEIGHT = 3;
	// The route cache. A far plan on Orc Valley costs 150-250 ms and most of
	// them are the same trip - the entrance to a hub, a hub to the exit, hub
	// to hub - asked for by bot after bot from within a few hundred units of
	// the same spot. Start and goal are quantised to this many cells, a hit
	// must stand within this reach of the cached start, and a route is kept
	// this long: the grid it was planned on is static, only the live objects
	// the walk checks for could have moved.
	// Keyed by the goal alone, and joined wherever the bot stands nearest to
	// the line: the goals are a few dozen hubs and exits, the starts are
	// everywhere, and a route to a hub is as good from its middle as from its
	// start. Several routes are kept per goal so the join is from a route that
	// came from this side.
	const int PLAYERBOT_NAV_CACHE_QUANTUM_CELLS = 24;
	const int PLAYERBOT_NAV_CACHE_MIN_CELLS = 256;
	// Sixteen routes a goal and half an hour, up from six and ten minutes:
	// the same four hub goals took half the far plans in a minute, and six
	// routes cover six approaches to a hub that is walked to from every
	// island. A route is a few hundred waypoints; fifteen hundred of them are
	// a couple of megabytes.
	const int PLAYERBOT_NAV_CACHE_JOIN_DISTANCE = 2400;
	const DWORD PLAYERBOT_NAV_CACHE_TTL = 1800000;
	const size_t PLAYERBOT_NAV_CACHE_PER_GOAL = 16;
	const size_t PLAYERBOT_NAV_CACHE_LIMIT = 1500;
	DWORD s_dwPlayerBotNavBudgetStamp = 0;
	int s_iPlayerBotNavHeavyPlansThisTick = 0;
	DWORD s_uPlayerBotNavPlanUsThisTick = 0;
	DWORD s_dwPlayerBotNavFarMinuteStamp = 0;
	int s_iPlayerBotNavFarPlansThisMinute = 0;
	// Which budget refused the last plan. Three different limits returned the
	// same DEFERRED and the log could not tell them apart, so a deferral was
	// read more than once as a destination with no route to it.
	const char* s_szPlayerBotNavDeferReason = "none";

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

		// ATTR_BLOCK is the map's statement that nothing may stand here, and it
		// is the only one the engine itself tests for movement. ATTR_WATER
		// describes the terrain: a river carries both bits, a bridge deck over
		// that river carries water without block, and so does a shallow shore.
		// Refusing water refused the bridges - see IsPlayerBotPositionWater.
		return tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT);
	}

	// Passable, but wet. Orc Valley is islands in a delta and its twenty-two
	// bridges are the only cells joining them; the desert's shallows are eighty
	// thousand cells of water nobody needs to wade through. Both are walkable and
	// only one of them should be attractive, so this feeds a path cost rather
	// than a wall.
	bool IsPlayerBotPositionWater(long lMapIndex, long x, long y)
	{
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		if (!tree || !tree->GetAttributePtr())
			return false;
		return tree->IsAttr(x, y, ATTR_WATER) &&
				!tree->IsAttr(x, y, ATTR_BLOCK);
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
				// Every kingdom's own four maps, not only Chunjo's: a Shinsoo bot
				// standing on map 1 with no grid here cannot plan a step, and the
				// whole of its local life is on 1, 3, 4 and 5. IsKingdomMap covers
				// all twelve; the shared maps are named one by one below because
				// only some of them are ours to walk.
				if (!playerbot_empire_rules::IsKingdomMap(mapIndex) &&
						!IsPlayerBotMonkeyMap(mapIndex) &&
						mapIndex != PLAYERBOT_MAP_ORC_VALLEY &&
						mapIndex != PLAYERBOT_MAP_DESERT &&
						mapIndex != PLAYERBOT_MAP_SOHAN &&
						mapIndex != PLAYERBOT_MAP_SPIDER_V1 &&
						mapIndex != PLAYERBOT_MAP_SPIDER_V2 &&
						mapIndex != PLAYERBOT_MAP_HWANG)
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
				m_water.assign(cellCount, 0);
				m_clearance.assign(cellCount, 0);
				m_component.assign(cellCount, 0);

				size_t walkableCount = 0;
				for (int gy = 0; gy < m_height; ++gy)
				{
					for (int gx = 0; gx < m_width; ++gx)
					{
						bool blocked = false;
						bool wet = false;
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
								if (IsPlayerBotPositionWater(mapIndex, cellX + ox, cellY + oy))
									wet = true;
							}
						}

						const int index = Index(gx, gy);
						m_blocked[index] = blocked ? 1 : 0;
						m_water[index] = (!blocked && wet) ? 1 : 0;
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

			// Is the character standing somewhere no step can be taken from?
			//
			// SegmentClearWorld tests the character's own cell before anything
			// else and gives up on it, so a bot whose cell the live world calls
			// blocked cannot walk anywhere at all: the planner reads the static
			// grid, plans a route out perfectly happily, and the first waypoint
			// is refused. The route is dropped and replanned two hundred
			// milliseconds later, identically, for as long as the bot lives.
			// Measured at four different portals on three maps: forty-two
			// refusals in twenty seconds, no movement, and not one line in any
			// log. The two grids disagree wherever something was placed after
			// the static one was built, and near a portal that is common -
			// portals stand against scenery.
			//
			// Returns a nearby cell that both grids call free, so the caller can
			// step off before asking for a route again.
			bool FindEscapeFromBlockedCell(long x, long y, long& outX, long& outY) const
			{
				if (!m_initialized || !IsInsideWorld(x, y))
					return false;
				int gx, gy;
				WorldToCell(x, y, gx, gy);
				if (!IsLiveBlockedCell(gx, gy))
					return false;

				for (int radius = 1; radius <= PLAYERBOT_NAV_ESCAPE_CELLS; ++radius)
				{
					for (int dy = -radius; dy <= radius; ++dy)
					{
						for (int dx = -radius; dx <= radius; ++dx)
						{
							if (std::max(abs(dx), abs(dy)) != radius)
								continue;
							const int cx = gx + dx;
							const int cy = gy + dy;
							if (!IsInsideCell(cx, cy) || IsBlockedCell(cx, cy) ||
									IsLiveBlockedCell(cx, cy))
								continue;
							int wx = 0, wy = 0;
							CellToWorld(cx, cy, wx, wy);
							outX = wx;
							outY = wy;
							return true;
						}
					}
				}
				return false;
			}

			// The middle of the cell a point falls in - the point the planner
			// actually planned from and to.
			void CellCentreWorld(long x, long y, long& outX, long& outY) const
			{
				int gx = 0, gy = 0;
				WorldToCell(x, y, gx, gy);
				int wx = 0, wy = 0;
				CellToWorld(gx, gy, wx, wy);
				outX = wx;
				outY = wy;
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

			// A far plan is the one thing the load line cannot attribute: it costs
			// a hundred times a near one, and only the destination and the price
			// say which subsystem asked for it and whether the search failed.
			EPlayerBotNavPlanResult FindRoute(long startX, long startY, long targetX, long targetY,
					DWORD seed, DWORD now, int targetSnapRadius, bool flexibleTargetSnap,
					std::vector<PIXEL_POSITION>& outWaypoints, bool starved = false)
			{
				const DWORD farUsBefore = s_uPlayerBotLoadPlanBucketUs[3];
				const DWORD farCountBefore = s_uPlayerBotLoadPlanBucket[3];
				const EPlayerBotNavPlanResult result = FindRouteInner(startX, startY, targetX, targetY,
						seed, now, targetSnapRadius, flexibleTargetSnap, outWaypoints, starved);
				if (s_uPlayerBotLoadPlanBucket[3] != farCountBefore)
					sys_log(0, "PLAYERBOT_NAV: far plan map=%ld from=(%ld,%ld) to=(%ld,%ld) result=%s cost_ms=%u waypoints=%u",
							m_mapIndex, startX, startY, targetX, targetY,
							result == PLAYERBOT_NAV_PLAN_FOUND ? "found" :
							(result == PLAYERBOT_NAV_PLAN_DEFERRED ? "deferred" : "unreachable"),
							(unsigned int)((s_uPlayerBotLoadPlanBucketUs[3] - farUsBefore) / 1000),
							(unsigned int)outWaypoints.size());
				return result;
			}

			EPlayerBotNavPlanResult FindRouteInner(long startX, long startY, long targetX, long targetY,
					DWORD seed, DWORD now, int targetSnapRadius, bool flexibleTargetSnap,
					std::vector<PIXEL_POSITION>& outWaypoints, bool starved)
			{
				outWaypoints.clear();
				if (!m_initialized || !IsInsideWorld(startX, startY) ||
						!IsInsideWorld(targetX, targetY))
					return PLAYERBOT_NAV_PLAN_UNREACHABLE;

				// The cache first, before the budget: a hit costs one segment check
				// and must not wait its turn behind the plans it makes unnecessary.
				int cacheSx, cacheSy, cacheTx, cacheTy;
				WorldToCell(startX, startY, cacheSx, cacheSy);
				WorldToCell(targetX, targetY, cacheTx, cacheTy);
				const int cacheCells = std::max(std::abs(cacheSx - cacheTx), std::abs(cacheSy - cacheTy));
				const bool cacheable = cacheCells >= PLAYERBOT_NAV_CACHE_MIN_CELLS;
				(void)cacheSx;
				(void)cacheSy;
				const int cacheKey = (cacheTx / PLAYERBOT_NAV_CACHE_QUANTUM_CELLS) * 4096 +
						cacheTy / PLAYERBOT_NAV_CACHE_QUANTUM_CELLS;
				if (cacheable)
				{
					std::map<int, std::vector<TCachedRoute> >::iterator hit = m_routeCache.find(cacheKey);
					if (hit != m_routeCache.end())
					{
						std::vector<TCachedRoute>& routes = hit->second;
						for (size_t r = 0; r < routes.size(); ++r)
						{
							const TCachedRoute& cached = routes[r];
							if (now - cached.dwStamp >= PLAYERBOT_NAV_CACHE_TTL)
								continue;
							// The nearest waypoint of this route, and it has to be a
							// straight step from where the bot stands; the rest of the
							// line was proven cell by cell when it was planned.
							size_t bestIndex = cached.waypoints.size();
							int bestDistance = PLAYERBOT_NAV_CACHE_JOIN_DISTANCE;
							for (size_t w = 0; w < cached.waypoints.size(); ++w)
							{
								const int distance = DISTANCE_APPROX(startX - cached.waypoints[w].x,
										startY - cached.waypoints[w].y);
								if (distance < bestDistance)
								{
									bestDistance = distance;
									bestIndex = w;
								}
							}
							if (bestIndex >= cached.waypoints.size() ||
									!SegmentClearWorld(startX, startY, cached.waypoints[bestIndex].x,
											cached.waypoints[bestIndex].y))
								continue;
							outWaypoints.assign(cached.waypoints.begin() + bestIndex, cached.waypoints.end());
							++s_uPlayerBotLoadPlanCached;
							return PLAYERBOT_NAV_PLAN_FOUND;
						}
					}
				}

				if (s_dwPlayerBotNavBudgetStamp != now)
				{
					s_dwPlayerBotNavBudgetStamp = now;
					s_iPlayerBotNavHeavyPlansThisTick = 0;
					s_uPlayerBotNavPlanUsThisTick = 0;
				}
				int sx, sy, tx, ty;
				WorldToCell(startX, startY, sx, sy);
				WorldToCell(targetX, targetY, tx, ty);
				const int planCells = std::max(std::abs(sx - tx), std::abs(sy - ty));
				const int planBucket = planCells < 64 ? 0 : planCells < 256 ? 1 : planCells < 1024 ? 2 : 3;
				if (now - s_dwPlayerBotNavFarMinuteStamp >= 60000)
				{
					s_dwPlayerBotNavFarMinuteStamp = now;
					s_iPlayerBotNavFarPlansThisMinute = 0;
				}
				// Three different budgets end in the same answer, and the log
				// could not tell them apart - so "deferred" was read as "no way
				// there" more than once. Say which one it was.
				const int planCost = PLAYERBOT_NAV_PLAN_COST[planBucket];
				const bool overCount = planCost > 0 && !starved &&
						s_iPlayerBotNavHeavyPlansThisTick + planCost >
							PLAYERBOT_NAV_MAX_HEAVY_PLANS_PER_TICK;
				if (overCount ||
						s_uPlayerBotNavPlanUsThisTick >= PLAYERBOT_NAV_PLAN_TIME_BUDGET_US ||
						(planBucket == 3 &&
						 s_iPlayerBotNavFarPlansThisMinute >= PLAYERBOT_NAV_MAX_FAR_PLANS_PER_MINUTE))
				{
					s_szPlayerBotNavDeferReason = overCount
								? "plans_per_tick"
								: (s_uPlayerBotNavPlanUsThisTick >= PLAYERBOT_NAV_PLAN_TIME_BUDGET_US
									? "plan_time_budget" : "far_plans_per_minute");
					++s_uPlayerBotLoadPlanDeferred;
					return PLAYERBOT_NAV_PLAN_DEFERRED;
				}
				if (planBucket == 3)
					++s_iPlayerBotNavFarPlansThisMinute;
				s_iPlayerBotNavHeavyPlansThisTick += planCost;
				++s_uPlayerBotLoadPlans;
				TPlayerBotLoadTimer planTickTimer(s_uPlayerBotNavPlanUsThisTick);

				++s_uPlayerBotLoadPlanBucket[planBucket];
				TPlayerBotLoadTimer planTimer(s_uPlayerBotLoadPlanUs);
				TPlayerBotLoadTimer planBucketTimer(s_uPlayerBotLoadPlanBucketUs[planBucket]);
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
						const int waterPenalty =
								m_water[nextIndex] ? PLAYERBOT_NAV_WATER_PENALTY : 0;
						const int laneJitter = (int)(PlayerBotNavHash(seed ^ (DWORD)nextIndex) & 1U);
						const int newCost = current.g + moveCost[direction] + wallPenalty +
								waterPenalty + laneJitter;

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

				if (cacheable)
				{
					// Full: drop a whole goal's routes, the first in key order - a plain
					// rule on the plans that already cost two hundred milliseconds.
					if (m_routeCacheCount >= PLAYERBOT_NAV_CACHE_LIMIT && !m_routeCache.empty())
					{
						m_routeCacheCount -= m_routeCache.begin()->second.size();
						m_routeCache.erase(m_routeCache.begin());
					}
					std::vector<TCachedRoute>& routes = m_routeCache[cacheKey];
					if (routes.size() >= PLAYERBOT_NAV_CACHE_PER_GOAL)
					{
						routes.erase(routes.begin());
						--m_routeCacheCount;
					}
					TCachedRoute entry;
					entry.startX = startX;
					entry.startY = startY;
					entry.dwStamp = now;
					entry.waypoints = outWaypoints;
					routes.push_back(entry);
					++m_routeCacheCount;
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

			// The engine's own answer for the cell's centre point - the same point,
			// and the same bits, that m_blocked was built from when the grid was made.
			// The planner therefore never asks this: a corridor search put the
			// question to the sectree tree up to twenty-four times for every node it
			// expanded, and a thousand plans a minute spent thirty-one seconds of
			// every sixty answering it. Only the walk keeps asking (SegmentClearWorld,
			// once per tick for the segment in front of the bot) so that anything
			// placed after the grid was built still stops a bot before it walks
			// into it, and the stuck counter then asks for a new plan.
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
						IsBlockedCell(x0, y0) || IsBlockedCell(x1, y1))
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
								IsBlockedCell(x + signX, y))) ||
								(signY != 0 && (IsBlockedCell(x, y + signY) ||
								IsBlockedCell(x, y + signY))))
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

					if (IsBlockedCell(x, y))
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
									IsBlockedCell(x, y))
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
						if (IsBlockedCell(nextX, nextY))
							continue;
						if (moveX[direction] != 0 && moveY[direction] != 0 &&
								(IsBlockedCell(currentX + moveX[direction], currentY) ||
								 IsBlockedCell(currentX, currentY + moveY[direction])))
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

				// See PLAYERBOT_NAV_GREEDY_CORRIDOR_REGIONS.
				const int heuristicWeight =
						corridor.size() >= (size_t)PLAYERBOT_NAV_GREEDY_CORRIDOR_REGIONS
						? PLAYERBOT_NAV_GREEDY_WEIGHT : 2;
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
				first.f = OctileDistance(startX, startY, targetX, targetY) * heuristicWeight;
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
						if (IsBlockedCell(nextX, nextY))
							continue;
						if (moveX[direction] != 0 && moveY[direction] != 0 &&
								(IsBlockedCell(currentX + moveX[direction], currentY) ||
								 IsBlockedCell(currentX, currentY + moveY[direction])))
							continue;
						const int nextCell = Index(nextX, nextY);
						const DWORD nextRegion = m_cellRegion[nextCell];
						if (nextRegion == 0 || nextRegion >= allowed.size() || !allowed[nextRegion])
							continue;

						int wallPenalty = 0;
						if (m_clearance[nextCell] <= 1) wallPenalty = 8;
						else if (m_clearance[nextCell] == 2) wallPenalty = 3;
						else if (m_clearance[nextCell] == 3) wallPenalty = 1;
						const int waterPenalty =
								m_water[nextCell] ? PLAYERBOT_NAV_WATER_PENALTY : 0;
						const int laneJitter = (int)(PlayerBotNavHash(seed ^ (DWORD)nextCell) & 1U);
						const int newCost = current.g + moveCost[direction] + wallPenalty +
								waterPenalty + laneJitter;
						if (m_nodeToken[nextCell] == token && newCost >= m_nodeCost[nextCell])
							continue;

						m_nodeToken[nextCell] = token;
						m_nodeCost[nextCell] = newCost;
						m_parent[nextCell] = current.cell;
						TFineOpenNode next;
						next.g = newCost;
						next.f = newCost + OctileDistance(nextX, nextY, targetX, targetY) * heuristicWeight;
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
			// Walkable, but water: a bridge deck or a shallow. Costs extra to cross
			// so that dry ground wins wherever there is a choice.
			std::vector<BYTE> m_water;
			std::vector<BYTE> m_clearance;
			std::vector<DWORD> m_component;
			std::vector<uint16_t> m_nodeToken;
			std::vector<int> m_nodeCost;
			std::vector<int> m_parent;
			uint16_t m_searchToken;
			std::vector<DWORD> m_cellRegion;
			struct TCachedRoute
			{
				long startX;
				long startY;
				DWORD dwStamp;
				std::vector<PIXEL_POSITION> waypoints;
			};
			std::map<int, std::vector<TCachedRoute> > m_routeCache;
			size_t m_routeCacheCount = 0;
			std::vector<TAbstractRegion> m_regions;
			std::vector<DWORD> m_regionToken;
			std::vector<int> m_regionCost;
			std::vector<int> m_regionParent;
			std::vector<int> m_regionParentEdge;
			DWORD m_regionSearchToken;
	};
}

#endif
