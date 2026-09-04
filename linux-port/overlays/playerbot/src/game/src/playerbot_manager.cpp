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

// Declared in input_p2p.cpp. ChatPacket would be useless for a bot - it has no
// client descriptor of its own to send to.
extern void SendShout(const char* szText, BYTE bEmpire);

#include "playerbot_types.h"
#include "playerbot_navigation.h"
#include "playerbot_world_memory.h"
#include "playerbot_movement.h"
#include "playerbot_gear.h"
#include "playerbot_activities.h"
#include "playerbot_missions.h"
#include "playerbot_skills.h"
#include "playerbot_combat.h"
#include "playerbot_economy.h"
#include "playerbot_travel.h"
#include "playerbot_town.h"

namespace
{
	LPEVENT s_pkPlayerBotUpdateEvent = NULL;

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
			// LPAD shortens rather than pads when the value is already longer
			// than the width, so LPAD(1001,3,'0') is '100' - the login of a
			// different bot. Every identity past PID 1002 was therefore rejected
			// in silence, and the cohort could never grow beyond a thousand no
			// matter how many characters the seed created. Pad to three, never
			// below the number's own length, which is what the generator's
			// "playerbot_%03d" means.
			"AND BINARY a.login=BINARY CONCAT('playerbot_',"
			"LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
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
