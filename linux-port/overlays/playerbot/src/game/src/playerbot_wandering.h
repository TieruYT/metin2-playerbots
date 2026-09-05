#ifndef __INC_METIN2_PLAYERBOT_WANDERING_H__
#define __INC_METIN2_PLAYERBOT_WANDERING_H__

// What a bot does on a hunting map when nothing is asking for its attention.
//
// This is the difference between a populated world and a car park full of
// idling characters, so it is deliberately not "walk to a random point": bots
// work a rotation of hotspots, spread out rather than stack, and keep moving
// through ground they have already cleared.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
	// A hub is only worth walking to if the navigation can actually get there.
	//
	// Orc Valley taught this. Its entrance opens onto one island of a river
	// delta, and while the navigation refused water - which is to say, refused
	// the bridges - all twelve of its hand-picked hubs sat on the far side of a
	// crossing. A bot planned an impossible route, gave up after three tries,
	// advanced to the next hub and planned another impossible route, twelve
	// times, then round again: twelve bots on that one map produced 7812 of the
	// 8259 "unreachable" lines in a session and never reached a hunting ground.
	//
	// That map is whole again, but the check stays, and not only as a memorial:
	// the Monkey Dungeon really is chambered, with 7.5% of its walkable ground
	// reachable from where a bot comes in, and any map may be built that way.
	//
	// Asking first costs a component lookup; the alternative costs a full A*
	// that is guaranteed to fail.
	size_t PickReachablePlayerBotHub(LPCHARACTER ch, const TPlayerBotMapPoint* hubs,
			size_t hubCount, size_t firstIndex, bool& bFoundOut)
	{
		bFoundOut = false;
		if (!ch || !hubs || hubCount == 0)
			return firstIndex;

		CPlayerBotNavigation& navigation =
				CPlayerBotNavigation::instance(ch->GetMapIndex());
		if (!navigation.Init(ch->GetMapIndex()))
			return firstIndex;

		for (size_t step = 0; step < hubCount; ++step)
		{
			const size_t index = (firstIndex + step) % hubCount;
			if (navigation.CanReach(ch->GetX(), ch->GetY(),
					hubs[index].x, hubs[index].y))
			{
				bFoundOut = true;
				return index;
			}
		}
		return firstIndex;
	}

	void ManagePlayerBotWandering(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

	// A frontier map is worked, not squatted on.
	//
	// Wandering is what rotates a bot between hunting hubs, and in the tick it
	// runs only on an update where nothing was worth attacking. Orc Valley has
	// 4041 spawn points, so on that map there is always something in reach and
	// the rotation never came round: bots warped to the arrival point, found a
	// monster, and stayed. Twenty-four bots on the map, twenty-one of them at
	// the two hubs beside the entrance, inside a box thirty kilometres across -
	// while the Elite Orcs that carry the Orc Amulet, two hundred and sixty-five
	// spawns of them, stood on islands nobody visited. The market cannot trade
	// what the world never drops, and the world does not drop what nobody kills.
	//
	// So a bot that has not moved a hub's width in five minutes is walked to the
	// next hub even though there is something here to kill. It has to claim
	// the tick while it walks: otherwise target acquisition picks the monster it
	// has been standing next to and the walk never takes a step.
	bool ManagePlayerBotRelocation(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			state.dwCampSince = 0;
			state.dwRelocateSince = 0;
			return false;
		}
		// A follower goes where its leader goes. Two members deciding separately
		// to walk off is how a party comes apart.
		if (ch->GetParty() && ch->GetParty()->GetLeaderCharacter() != ch)
			return false;

		const int fromCamp = DISTANCE_APPROX(ch->GetX() - state.lCampX,
				ch->GetY() - state.lCampY);

		if (state.dwRelocateSince != 0)
		{
			// Arrived, or long enough trying. Either way this is home now.
			//
			// "Arrived" has to mean the hub, not a fixed number of paces. An
			// earlier draft ended the leg after one hub's width, which on a map
			// this size is halfway to nowhere: the far side of Orc Valley is
			// sixty-four thousand units from the entrance, so a bot needed five
			// separate legs with five minutes of standing still between them.
			// Measured after forty minutes of that: eight of the sixteen hubs
			// had somebody on them and every one of the eight was in the middle.
			const bool bHaveDestination = state.lRouteMapIndex == ch->GetMapIndex() &&
					(state.lRouteDestX != 0 || state.lRouteDestY != 0);
			const bool bArrived = bHaveDestination
					? DISTANCE_APPROX(ch->GetX() - state.lRouteDestX,
							ch->GetY() - state.lRouteDestY) <= PLAYERBOT_RELOCATE_ARRIVED
					: fromCamp >= PLAYERBOT_RELOCATE_DISTANCE;
			if (bArrived ||
					dwNow - state.dwRelocateSince >= PLAYERBOT_RELOCATE_TIMEOUT)
			{
				state.lCampX = ch->GetX();
				state.lCampY = ch->GetY();
				state.dwCampSince = dwNow;
				state.dwRelocateSince = 0;
				return false;
			}
			state.dwNextWanderTime = dwNow;
			ManagePlayerBotWandering(ch, state, dwNow);
			return true;
		}

		if (state.dwCampSince == 0 || fromCamp > PLAYERBOT_RELOCATE_DISTANCE)
		{
			// Already a hub away under its own steam, which is the ordinary case
			// and the whole point. Note where it is and let it hunt.
			state.lCampX = ch->GetX();
			state.lCampY = ch->GetY();
			state.dwCampSince = dwNow;
			return false;
		}
		if (dwNow - state.dwCampSince < PLAYERBOT_CAMP_TIMEOUT)
			return false;

		sys_log(0, "PLAYERBOT_TRAVEL: moving on pid=%u name=%s map=%ld camped=%us pos=(%ld,%ld)",
				ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
				(unsigned int)((dwNow - state.dwCampSince) / 1000),
				ch->GetX(), ch->GetY());
		state.dwRelocateSince = dwNow;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		state.dwNextWanderTime = dwNow;
		ManagePlayerBotWandering(ch, state, dwNow);
		return true;
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
			// coordinate so a hub can never be planted inside an obstacle, and
			// verified walkable and inside the arrival point's region.
			//
			// Orc Valley's list used to hold twelve points inside a 37x35 km box,
			// because that box was the one island a bot could reach while the
			// navigation refused the bridges over the delta. The map's 532 spawn
			// groups are spread over 133x131 km, and the old list reached 128 of
			// them. These sixteen reach 250: generated from regen.txt by spawn
			// density, spaced at least 12000 units apart so they cover the map
			// rather than crowd its busiest corner.
			const TPlayerBotMapPoint orcValleyHubs[16] = {
				{ 315800, 732600 }, { 342600, 729800 }, { 335500, 758000 },
				{ 328000, 743600 }, { 277800, 793500 }, { 347700, 797500 },
				{ 334000, 800200 }, { 343200, 743100 }, { 391700, 696600 },
				{ 330500, 727300 }, { 292200, 751000 }, { 365100, 777800 },
				{ 271500, 683700 }, { 297600, 716400 }, { 302500, 777000 },
				{ 336500, 703600 }
			};
			const TPlayerBotMapPoint desertHubs[12] = {
				{ 291300, 515700 }, { 237500, 525900 }, { 264600, 526100 },
				{ 317900, 526100 }, { 336900, 534300 }, { 245100, 542500 },
				{ 264500, 552300 }, { 327700, 552900 }, { 253900, 570100 },
				{ 327800, 579500 }, { 321600, 582700 }, { 273800, 614900 }
			};
			const bool inDesert = ch->GetMapIndex() == PLAYERBOT_MAP_DESERT;
			const TPlayerBotMapPoint* hubs = inDesert ? desertHubs : orcValleyHubs;
			// Taken from the array rather than written out, so the two lists are
			// free to be different lengths - and to change length again without
			// anybody having to remember three call sites.
			const size_t hubCount = inDesert
					? sizeof(desertHubs) / sizeof(desertHubs[0])
					: sizeof(orcValleyHubs) / sizeof(orcValleyHubs[0]);
			const DWORD pid = ch->GetPlayerID();
			bool bHubReachable = false;
			const size_t hubIndex = PickReachablePlayerBotHub(ch, hubs, hubCount,
					(pid + state.uMetinHotspotIndex) % hubCount, bHubReachable);
			if (!bHubReachable)
			{
				// Nothing on the list can be reached from where this bot is. Work
				// the ground it is standing on instead of replanning routes that
				// cannot exist - the map still has monsters on this side of the
				// wall, and a bot hunting them looks far better than one walking
				// into it.
				targetX = ch->GetX() + number(-1200, 1200);
				targetY = ch->GetY() + number(-1200, 1200);
			}
			else
			{
				long offsetX = 0, offsetY = 0;
				GetPlayerBotStableOffset(pid,
						(inDesert ? 0x44455348U : 0x4f524348U) + (DWORD)hubIndex,
						150, 700, offsetX, offsetY);
				targetX = hubs[hubIndex].x + offsetX;
				targetY = hubs[hubIndex].y + offsetY;
				if (DISTANCE_APPROX(ch->GetX() - targetX, ch->GetY() - targetY) < 1400)
				{
					// Standing on the hub. Wandering only runs when there is
					// nothing left to fight here, so a seven-hundred-unit nudge
					// followed by another ten seconds of waiting is how a bot ends
					// up guarding one respawn for an hour. Take the next hub that
					// can actually be reached from here instead.
					++state.uMetinHotspotIndex;
					bool bNextReachable = false;
					const size_t nextIndex = PickReachablePlayerBotHub(ch, hubs, hubCount,
							(pid + state.uMetinHotspotIndex) % hubCount, bNextReachable);
					if (bNextReachable && nextIndex != hubIndex)
					{
						long nextOffsetX = 0, nextOffsetY = 0;
						GetPlayerBotStableOffset(pid,
								(inDesert ? 0x44455348U : 0x4f524348U) + (DWORD)nextIndex,
								150, 700, nextOffsetX, nextOffsetY);
						targetX = hubs[nextIndex].x + nextOffsetX;
						targetY = hubs[nextIndex].y + nextOffsetY;
					}
					else
					{
						// Walled into a pocket with nowhere else to go. Work the
						// ground here rather than plan a route that cannot exist.
						targetX = ch->GetX() + number(-700, 700);
						targetY = ch->GetY() + number(-700, 700);
					}
				}
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
			bool bRoomReachable = false;
			const size_t roomIndex = PickReachablePlayerBotHub(ch, rooms, 8,
					(pid + state.uMetinHotspotIndex) % 8, bRoomReachable);
			if (!bRoomReachable)
			{
				// A closed door or a corridor the grid does not join. Same answer
				// as on the frontier: hunt where you are.
				targetX = ch->GetX() + number(-450, 450);
				targetY = ch->GetY() + number(-450, 450);
			}
			else
			{
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
}

#endif
