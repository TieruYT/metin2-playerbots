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
#include "guild.h"
#include "guild_manager.h"
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
#include "shop.h"
#include "shop_manager.h"
#include "sectree_manager.h"
#include "vector.h"
#include "utils.h"
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <sys/stat.h>

extern int passes_per_sec;

// Declared in input_p2p.cpp. ChatPacket would be useless for a bot - it has no
// client descriptor of its own to send to.
extern void SendShout(const char* szText, BYTE bEmpire);

#include "playerbot_types.h"
#include "playerbot_log.h"
#include "playerbot_config.h"
#include "playerbot_swing_timing.h"
#include "playerbot_navigation.h"
#include "playerbot_world_memory.h"
#include "playerbot_movement.h"
#include "playerbot_gear.h"
#include "playerbot_activities.h"
#include "playerbot_missions.h"
#include "playerbot_skills.h"
#include "playerbot_combat.h"
#include "playerbot_economy.h"
#include "playerbot_bonus.h"
#include "playerbot_travel.h"
#include "playerbot_planner.h"
#include "playerbot_guild.h"
#include "playerbot_town.h"
#include "playerbot_market.h"
#include "playerbot_loot.h"
#include "playerbot_survival.h"
#include "playerbot_wandering.h"
#include "playerbot_status.h"
#include "playerbot_targeting.h"

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

		// Traders are drawn before the rest: a bot that trades for a living is not
		// a variant of an adventurer, it is a different way of playing, and the
		// world was short of one.
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d524348U) %
				PLAYERBOT_MERCHANT_SHARE) == 0)
			return BOT_PERSONALITY_MERCHANT;

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
			case BOT_PERSONALITY_MERCHANT:
				return BOT_AMBITION_TRADE;
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
			TPartyFinder(LPCHARACTER me, const TPlayerBotAIState& st)
				: m_me(me), m_state(st), m_pTargetParty(NULL),
				  m_pSoloCandidate(NULL), m_iSoloAffinity(-1) {}
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
					else if (!cp)
					{
						// Whoever it has got on with best, rather than whoever the
						// sector happened to hand over first. A bot that has hunted
						// with somebody before will look for them again.
						const int affinity = GetPlayerBotAffinity(
								m_state, candidate->GetPlayerID());
						if (affinity > m_iSoloAffinity)
						{
							m_iSoloAffinity = affinity;
							m_pSoloCandidate = candidate;
						}
					}
				}
				return true;
			}
			LPCHARACTER m_me;
			const TPlayerBotAIState& m_state;
			LPPARTY m_pTargetParty;
			LPCHARACTER m_pSoloCandidate;
			int m_iSoloAffinity;
		};

		TPartyFinder finder(ch, state);
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
				RememberPlayerBotEncounter(ch, finder.m_pSoloCandidate,
						PLAYERBOT_FRIEND_PARTY_POINTS, dwNow);
				sys_log(0, "PLAYERBOT_AI: created party pid=%u name=%s partner_pid=%u affinity=%d",
						ch->GetPlayerID(), ch->GetName(),
						finder.m_pSoloCandidate->GetPlayerID(),
						GetPlayerBotAffinity(state, finder.m_pSoloCandidate->GetPlayerID()));
			}
		}
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

	// Once for the whole population: the panel may have moved a weight since
	// the last tick, and every bot planned below must see the same numbers.
	RefreshPlayerBotWeights(dwNow);

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

		// Browsing the market. Cheap when there is nothing to buy - it only looks
		// around every couple of minutes - and claims the tick when it buys, so
		// the purchase is never mixed into the same pass as a fight.
		if (ManagePlayerBotShopping(ch, state, dwNow))
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
		ManagePlayerBotGuild(ch, state, dwNow);
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

		// A normal horse is for transport only, so it goes before target
		// selection, buffs and combat: a level-1 horse must never produce a
		// mounted attack. A battle horse is a different animal and stays - this
		// line used to dismount it too, which is why a rider was seen hacking a
		// metin on foot with its horse standing beside it. Whether it actually
		// fights from the saddle is then the target's business, decided where
		// the target is known.
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch) &&
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

		// In range, target known: this is the one place that can say whether the
		// fight itself happens from the saddle. Mount for the ones that should,
		// climb down for the ones that should not - a bot that walked up on foot
		// would otherwise never get back on, however good its horse.
		if (CanPlayerBotEverFightOnHorse(ch))
		{
			const bool wantsSaddle = CanPlayerBotFightOnHorse(ch, target);
			if (wantsSaddle != ch->IsRiding())
				SetPlayerBotRidingForTravel(ch, state, wantsSaddle, dwNow,
						wantsSaddle ? "mounted_combat" : "dismount_for_target");
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
