#include "stdafx.h"
#include "playerbot_manager.h"
#include "playerbot_empire_rules.h"
#include "playerbot_world_rules.h"

#include "char.h"
#include "skill.h"
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
#include "log.h"
#include "config.h"
#include "constants.h"
#include "battle.h"
#include "buffer_manager.h"
#include "motion.h"
#include "party.h"
#include "questmanager.h"
#include "safebox.h"
#include "questpc.h"
#include "refine.h"
#include "sectree.h"
#include "shop.h"
#include "shop_manager.h"
#include "sectree_manager.h"
#include "vector.h"
#include "utils.h"
#include <queue>
#include <set>
#include <deque>
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
#include "playerbot_combat_value_policy.h"
#include "playerbot_battle_horse.h"
#include "playerbot_gear.h"
#include "playerbot_consumables.h"
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
#include "playerbot_chat_trade.h"
#include "playerbot_loot.h"
#include "playerbot_survival.h"
#include "playerbot_wandering.h"
#include "playerbot_status.h"
#include "playerbot_targeting.h"
#include "playerbot_lure.h"

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
			return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d444f50U) % 3U) == 0
					? BOT_PERSONALITY_METIN_DROPPER : BOT_PERSONALITY_METIN_BREAKER;

		// Traders are drawn before the rest: a bot that trades for a living is not
		// a variant of an adventurer, it is a different way of playing, and the
		// world was short of one.
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d524348U) %
				PLAYERBOT_MERCHANT_SHARE) == 0)
			return BOT_PERSONALITY_MERCHANT;
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x44524f50U) %
				PLAYERBOT_DROPPER_SHARE) == 0)
		{
			switch (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4b494e44U) % 3U)
			{
				case 0: return BOT_PERSONALITY_M3_DROPPER;
				case 1: return BOT_PERSONALITY_M2_DROPPER;
				default: return BOT_PERSONALITY_MEDAL_DROPPER;
			}
		}

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
			case BOT_PERSONALITY_METIN_DROPPER:
				return BOT_AMBITION_METINS;
			case BOT_PERSONALITY_M3_DROPPER:
			case BOT_PERSONALITY_M2_DROPPER:
				return BOT_AMBITION_EQUIPMENT;
			case BOT_PERSONALITY_MEDAL_DROPPER:
				return BOT_AMBITION_HORSE;
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

	// Who may be in a party. The ten-percent cohort everywhere; on Orc Valley,
	// anyone of camp level - the Black Orc camps are a party's work and eight
	// of the cohort at one level on one island never turns up.
	bool IsPlayerBotPartyEligible(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (state.bBotRole == BOT_ROLE_PARTY_FIGHTER)
			return true;
		// Every frontier map, not the valley alone: a map change dissolves a
		// party, so one made in the valley never reached V1 or Sohan, and the
		// Spider Queen and Nine Tails - a party's work - had nobody to fight
		// them. Sixteen bots in V1 and not one in a party.
		return ch && IsPlayerBotFrontierMapIndex(ch->GetMapIndex()) &&
				ch->GetLevel() >= PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL;
	}

	int GetPlayerBotPartyDesiredMax(LPCHARACTER ch)
	{
		return (ch && IsPlayerBotFrontierMapIndex(ch->GetMapIndex()))
				? PLAYERBOT_ORC_VALLEY_PARTY_MAX : PLAYERBOT_PARTY_DESIRED_MAX;
	}

	// Does this party already have somebody to cast Blessing?
	bool PlayerBotPartyHasShaman(LPPARTY party)
	{
		if (!party)
			return false;
		struct FFindShaman
		{
			FFindShaman() : m_bFound(false) {}
			void operator()(LPCHARACTER member)
			{
				if (member && member->GetJob() == JOB_SHAMAN)
					m_bFound = true;
			}
			bool m_bFound;
		};
		FFindShaman finder;
		party->ForEachOnlineMember(finder);
		return finder.m_bFound;
	}

	bool ArePlayerBotsGuildMates(LPCHARACTER a, LPCHARACTER b)
	{
		return a && b && a->GetGuild() != NULL && a->GetGuild() == b->GetGuild();
	}

	void ManagePlayerBotParty(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() || dwNow < state.dwNextPartyCheckTime)
			return;

		state.dwNextPartyCheckTime = dwNow + PLAYERBOT_PARTY_CHECK_INTERVAL + number(0, 3000);

		LPPARTY pParty = ch->GetParty();
		// Party play is an explicit, deterministic cohort. Archer weighting is
		// decided at login, while the total cohort remains close to ten percent.
		if (!IsPlayerBotPartyEligible(ch, state))
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
				// A leader walking to a new camp is followed, not left: the follower
				// is on its way, and a deferred route in the middle of thirty
				// kilometres is not a reason to disband. Fifty-seven of fifty-nine
				// break-ups in the first hour of the camps were exactly that walk.
				TPlayerBotAIStateMap::const_iterator leaderState =
						s_mapPlayerBotAIStates.find(leader->GetPlayerID());
				const bool bLeaderRelocating = leaderState != s_mapPlayerBotAIStates.end() &&
						leaderState->second.dwRelocateSince != 0;
				const int stragglerRadius = (bLeaderRelocating || state.bCurrentAction == BOT_ACTION_PARTY_ASSEMBLE)
						? PLAYERBOT_PARTY_STRAGGLER_RADIUS * 4 : PLAYERBOT_PARTY_STRAGGLER_RADIUS;
				if (levelDelta > 6 || leader->GetMapIndex() != ch->GetMapIndex() ||
						distToLeader > stragglerRadius)
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

		// A stretch of hunting alone, less often where a party is the point.
		const int soloPercent = IsPlayerBotFrontierMapIndex(ch->GetMapIndex())
				? PLAYERBOT_PARTY_SOLO_PERCENT_FRONTIER : PLAYERBOT_PARTY_SOLO_PERCENT;
		if (number(1, 100) <= soloPercent)
		{
			state.dwNextPartyCheckTime = dwNow + number(60000, 180000);
			return;
		}

		// Find a nearby bot with an open party or start one
		struct TPartyFinder
		{
			TPartyFinder(LPCHARACTER me, const TPlayerBotAIState& st)
				: m_me(me), m_state(st), m_pTargetParty(NULL),
				  m_pSoloCandidate(NULL), m_iSoloAffinity(-1),
				  m_bTargetPartyGuild(false), m_bTargetPartyShaman(false) {}
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
							!IsPlayerBotPartyEligible(candidate, stateIt->second))
						return true;

					if (abs((int)candidate->GetLevel() - (int)m_me->GetLevel()) > 3)
						return true;

					const int d = DISTANCE_APPROX(m_me->GetX() - candidate->GetX(), m_me->GetY() - candidate->GetY());
					if (d > 1800)
						return true;

					if (!IsPlayerBotPathClear(m_me->GetMapIndex(), m_me->GetX(), m_me->GetY(), candidate->GetX(), candidate->GetY()))
						return true;

					LPPARTY cp = candidate->GetParty();
					if (cp && cp->GetMemberCount() < (DWORD)GetPlayerBotPartyDesiredMax(m_me))
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
								// A guild mate's party is taken at once; any other is
								// kept in hand while the sweep looks for a guild mate's.
								// Out on the frontier a party with a Shaman in it
								// outranks one without, for the same reason a Shaman
								// is worth pairing with in the first place - it is
								// the one job that keeps the others standing.
								const bool bGuild = ArePlayerBotsGuildMates(m_me, leader);
								const bool bWantsShaman =
										m_me->GetJob() != JOB_SHAMAN &&
										IsPlayerBotFrontierMapIndex(m_me->GetMapIndex());
								const bool bShamanParty = bWantsShaman && PlayerBotPartyHasShaman(cp);
								if (bGuild || !m_pTargetParty ||
										(bShamanParty && !m_bTargetPartyShaman && !m_bTargetPartyGuild))
								{
									m_pTargetParty = cp;
									m_bTargetPartyGuild = bGuild;
									m_bTargetPartyShaman = bShamanParty;
								}
								return !bGuild;
							}
						}
					}
					else if (!cp)
					{
						// Whoever it has got on with best, rather than whoever the
						// sector happened to hand over first. A bot that has hunted
						// with somebody before will look for them again.
						// One Shaman in the pair, not two: the buffs land on the
						// party whoever casts them, so a second Shaman adds
						// nothing a first has not already given.
						const bool bPairHasShaman =
								(m_me->GetJob() == JOB_SHAMAN) != (candidate->GetJob() == JOB_SHAMAN);
						const int affinity = GetPlayerBotAffinity(
								m_state, candidate->GetPlayerID()) +
								(ArePlayerBotsGuildMates(m_me, candidate) ? PLAYERBOT_GUILD_PARTY_POINTS : 0) +
								((bPairHasShaman &&
									IsPlayerBotFrontierMapIndex(m_me->GetMapIndex()))
									? PLAYERBOT_PARTY_SHAMAN_POINTS : 0);
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
			bool m_bTargetPartyGuild;
			bool m_bTargetPartyShaman;
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

	// Kamien Duszy: the soul stone that goes into a weapon or armour socket.
	// (Not the Kamien Duchowy that takes a skill past grand master - that is an
	// ITEM_USE and never comes through here. The two are one word apart in
	// Polish and used to be one word here, which is how this was named for the
	// wrong one.)
	//
	// This used to write the stone into the socket with SetSocket and delete the
	// stone: a free, certain insertion. A player gets a 30% roll and, on the
	// other 70%, a cracked stone welded into the socket - ITEM_METIN under
	// UseItemEx in char_item.cpp. Bots were fitting +4 stones at a rate no
	// player could match, out of the same drops, and nothing they owned ever
	// cracked.
	//
	// So the stone is used the way a player uses it. UseItemEx refuses an
	// equipped target, so the gear comes off first and goes back on after; the
	// socket is read back afterwards to learn which way the roll went.
	void ManagePlayerBotSoulStones(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextSoulStoneTime)
			return;
		state.dwNextSoulStoneTime = dwNow + PLAYERBOT_SOUL_STONE_CHECK_INTERVAL;

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
			const int kdPlus = GetPlayerBotSoulStoneGrade(kdVnum);
			const int stoneKind = GetPlayerBotSoulStoneKind(kdVnum);
			const int worth = GetPlayerBotSoulStoneWorth(ch, stoneKind);
			if (worth <= 0)
				continue;
			LPITEM targetGear = NULL;
			int openSocket = -1;
			if (!FindPlayerBotSoulStoneSocket(ch, stoneKind, (DWORD)item->GetValue(5), &targetGear, &openSocket))
				continue;
			if (!ShouldPlayerBotSeatSoulStone(targetGear, kdPlus))
				continue;
			const int score = kdPlus * 100 + targetGear->GetRefineLevel() * 10 + worth;
			if (score > bestScore)
			{
				bestScore = score;
				bestStone = item;
				bestGear = targetGear;
				bestSocket = openSocket;
			}
		}

		if (!bestStone || !bestGear || bestSocket < 0)
			return;

		const DWORD kdVnum = bestStone->GetVnum();
		const DWORD gearVnum = bestGear->GetVnum();
		const WORD stoneCell = bestStone->GetCell();

		// Off, so UseItemEx will look at it; and there has to be somewhere for
		// it to go.
		if (ch->GetEmptyInventory(bestGear->GetSize()) < 0)
			return;
		if (!ch->UnequipItem(bestGear) || bestGear->IsEquipped())
			return;

		// UseItemEx deletes the stone whichever way the roll goes, so nothing
		// below may touch bestStone.
		ch->UseItemEx(bestStone, TItemPos(INVENTORY, bestGear->GetCell()));
		const DWORD after = (DWORD)bestGear->GetSocket(bestSocket);
		const bool stoneGone = ch->GetInventoryItem(stoneCell) == NULL ||
				ch->GetInventoryItem(stoneCell)->GetVnum() != kdVnum;
		const char* outcome = after == kdVnum ? "SUCCESS"
				: after == PLAYERBOT_BROKEN_SOUL_STONE_VNUM ? "CRACKED"
				: stoneGone ? "CONSUMED" : "REFUSED";

		ch->EquipItem(bestGear);
		SetPlayerBotAction(state, BOT_ACTION_SOCKET_STONE, dwNow);
		sys_log(0, "PLAYERBOT_AI: soul stone %s pid=%u name=%s kd_vnum=%u gear_vnum=%u socket=%d now=%u score=%d",
				outcome, ch->GetPlayerID(), ch->GetName(), kdVnum, gearVnum,
				bestSocket, after, bestScore);
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

		// The day's wait the engine puts between two reads of one skill
		// (SKILLBOOK_DELAY_MIN..MAX, eighteen to thirty hours) is waved away
		// entirely while the panel's BOOKS switch is on. The engine's own way
		// round it is an Exorcism Scroll, which a bot rarely has; this is the
		// scroll without the item, and without a wait of its own - what a bot
		// reads is limited by how many books it is holding, not by a clock.
		//
		// The two rules that decide how far a skill actually gets are the
		// engine's and are not touched here: how many successful reads take it
		// to G, and the roll on each read. A bot with a bagful still fails a
		// third of them and still needs ten that land.
		if (IsPlayerBotFastBooksEnabled() &&
				get_global_time() < ch->GetSkillNextReadTime(bestSkillVnum))
			ch->SetSkillNextReadTime(bestSkillVnum, get_global_time());
		// Still waiting, and no scroll to wave the wait away: asking the engine
		// anyway cost a refusal every eight seconds and a "read" line that read
		// nothing - a hundred and ninety of them in eight minutes.
		if (get_global_time() < ch->GetSkillNextReadTime(bestSkillVnum) &&
				!ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
			return;

		// LearnSkillByBook refuses a rider outright, so the one NPC-less
		// errand that still needs the ground is this one.
		if (ch->IsRiding())
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "reading_book");
			return;
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

	// Once a minute, the reasons the level-40 bots in Bokjung are there.
	const char* PLAYERBOT_M2_STAY_REASONS[] = {
		"retreat", "defence", "visit", "errand", "market", "fishing", "stall",
		"quest", "material", "travel", "no_plan", "none"
	};
	const int PLAYERBOT_M2_STAY_REASON_COUNT =
			(int)(sizeof(PLAYERBOT_M2_STAY_REASONS) / sizeof(PLAYERBOT_M2_STAY_REASONS[0]));
	int s_aiPlayerBotM2Stay[PLAYERBOT_M2_STAY_REASON_COUNT] = { 0 };
	DWORD s_dwPlayerBotM2CensusTime = 0;
	bool s_bPlayerBotM2CensusPass = false;

	void NotePlayerBotM2Stay(const char* reason)
	{
		for (int i = 0; i < PLAYERBOT_M2_STAY_REASON_COUNT; ++i)
			if (strcmp(reason, PLAYERBOT_M2_STAY_REASONS[i]) == 0)
			{
				++s_aiPlayerBotM2Stay[i];
				return;
			}
	}

	// One bot, one line, everything the 8 September audit asked to be able to
	// read: what it is for, what is holding it, and what happens next.
	//
	// The census counts; this explains. A few bots a minute rather than all of
	// them, because the point is to be able to follow one bot through a cycle,
	// not to fill the log with a hundred identical lines.
	void ReportPlayerBotM2Why(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		size_t redCount = 0, blueCount = 0;
		CountPlayerBotPotions(ch, redCount, blueCount);
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		sys_log(0, "PLAYERBOT_M2: why pid=%u name=%s level=%u goal=%u grind=%d service=%d service_age_ms=%u retry_in_ms=%d "
				"departure_to=%ld departure_age_ms=%u red=%u blue=%u target=%s target_level=%u combat_reason=%s "
				"route=%u/%u nav_defer=%u nav_wait_ms=%u action=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
				(unsigned int)state.bLongTermGoal,
				IsPlayerBotGrindAllowedHere(ch) ? 1 : 0,
				state.bServicePending ? 1 : 0,
				state.dwServiceSince != 0 ? dwNow - state.dwServiceSince : 0,
				state.dwServiceRetryAt != 0 ? (int)(state.dwServiceRetryAt - dwNow) : -1,
				state.lDepartureMap,
				state.dwDepartureSince != 0 ? dwNow - state.dwDepartureSince : 0,
				(unsigned int)redCount, (unsigned int)blueCount,
				target ? target->GetName() : "-",
				target ? target->GetLevel() : 0,
				playerbot_combat_value::ReasonName(
						(playerbot_combat_value::Reason)state.bLastCombatReason),
				(unsigned int)state.uRouteIndex, (unsigned int)state.vecRoute.size(),
				(unsigned int)state.bNavDeferredCount,
				state.dwFirstNavDeferTime != 0 ? dwNow - state.dwFirstNavDeferTime : 0,
				(unsigned int)state.bCurrentAction);
	}

	void ReportPlayerBotM2Census()
	{
		char line[512];
		int used = 0;
		int total = 0;
		for (int i = 0; i < PLAYERBOT_M2_STAY_REASON_COUNT; ++i)
		{
			total += s_aiPlayerBotM2Stay[i];
			if (s_aiPlayerBotM2Stay[i] == 0)
				continue;
			const int written = snprintf(line + used, sizeof(line) - used, " %s=%d",
					PLAYERBOT_M2_STAY_REASONS[i], s_aiPlayerBotM2Stay[i]);
			if (written > 0 && used + written < (int)sizeof(line))
				used += written;
			s_aiPlayerBotM2Stay[i] = 0;
		}
		line[used] = 0;
		sys_log(0, "PLAYERBOT_M2: census level40plus=%d%s", total, line);
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
		// A bot resting in town stands still on purpose, exactly like an angler
		// waiting for a bite - stillness is the activity, not a symptom.
		if (moved || foughtRecently || castRecently || state.bFishingSession ||
				state.dwTownLingerUntil != 0)
		{
			state.dwLastMeaningfulActivityTime = dwNow;
			state.lLastX = ch->GetX();
			state.lLastY = ch->GetY();
			return false;
		}

		if (dwNow - state.dwLastMeaningfulActivityTime < PLAYERBOT_INACTIVITY_RESET_TIME)
			return false;

		++s_uPlayerBotLoadWatchdog;
		sys_err("PLAYERBOT_WATCHDOG: resetting inactive bot pid=%u name=%s pos=(%ld,%ld) action=%u goal=%u target=%u shop=%d phase=%u bio=%d stable=%d route=%u/%u equip_pending=%d service=%d riding=%d nav_out=%u wander_in=%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY(),
				(unsigned int)state.bCurrentAction, (unsigned int)state.bLongTermGoal,
				state.dwTargetVID, state.bVisitingShop ? 1 : 0,
				(unsigned int)state.bTownVisitPhase, state.bVisitingBiologist ? 1 : 0,
				state.bVisitingStable ? 1 : 0, (unsigned int)state.uRouteIndex,
				(unsigned int)state.vecRoute.size(), state.bEquipPending ? 1 : 0,
				state.bServicePending ? 1 : 0, ch->IsRiding() ? 1 : 0,
				(unsigned int)state.bLastNavOutcome,
				state.dwNextWanderTime > dwNow ? (int)(state.dwNextWanderTime - dwNow) : 0);

		// The errand survives the reset. FinishPlayerBotTownVisit clears the
		// phase and the stuck route - which is what the watchdog is for - but
		// the need that brought the bot to town is handed to SERVICE_RECOVERY
		// rather than to whatever monster is standing nearby, and the bot stays
		// out of ordinary fights until its retry comes round.
		if (state.bVisitingShop)
		{
			const bool stillNeeded = NeedsPlayerBotPotions(ch) ||
					BlocksPlayerBotTravel(ch);
			FinishPlayerBotTownVisit(ch, state, dwNow, false);
			if (stillNeeded)
			{
				if (state.dwServiceSince == 0)
					state.dwServiceSince = dwNow;
				state.bServicePending = true;
				state.dwServiceRetryAt = dwNow + number(
						(int)PLAYERBOT_SERVICE_RETRY_MIN, (int)PLAYERBOT_SERVICE_RETRY_MAX);
				state.dwNextShopCheckTime = state.dwServiceRetryAt;
				sys_log(0, "PLAYERBOT_SERVICE: recovery armed pid=%u name=%s map=%ld retry_in_ms=%u age_ms=%u",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
						state.dwServiceRetryAt - dwNow, dwNow - state.dwServiceSince);
			}
		}

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
		// Deliberately not cleared here: bServicePending, dwServiceRetryAt,
		// dwServiceSince and the departure intent. A reset drops a stale route
		// and a stale target; the reason the bot came to town and the map it
		// means to leave for outlive it.
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
	: m_dwNextSpawnBatchTime(0),
	  m_uSpawnBatchSize(0),
	  m_dwSpawnWindowStarted(0),
	  m_uSpawnWindowTotal(0),
	  m_dwNextTopUpTime(0),
	  m_bRegistryLoaded(false),
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
	if (dwPlayerID == 0)
		return false;

	// The kingdom comes from the registry, never from the caller. A PID whose
	// seeded character is Jinno starts as Jinno or does not start at all -
	// this is the guard that stops a bad call turning a character into a bot
	// of somebody else's empire, and it is why the argument is only checked.
	const BYTE bRegisteredEmpire = GetRegisteredEmpire(dwPlayerID);
	if (bRegisteredEmpire == 0)
		bEmpire = 0;
	else if (bEmpire != 0 && bEmpire != bRegisteredEmpire)
	{
		sys_err("PLAYERBOT_AUTH: refused pid=%u asked empire=%u but the registry says %u",
				dwPlayerID, bEmpire, bRegisteredEmpire);
		return false;
	}
	else
		bEmpire = bRegisteredEmpire;

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

	// The descriptor's account: what the safebox, the login log and the
	// account-keyed packets read. Zero here meant one safebox for every bot.
	TPlayerBotAccountMap::const_iterator account = m_mapBotAccounts.find(dwPlayerID);
	if (account != m_mapBotAccounts.end())
	{
		TAccountTable& table = d->GetAccountTable();
		table.id = account->second.dwID;
		strlcpy(table.login, account->second.strLogin.c_str(), sizeof(table.login));
	}

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
	m_mapBotAccounts.clear();

	const char* query =
			"SELECT l.pid, a.id, a.login, pi.empire "
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
			// Who comes first when the slider asks for more than are playing.
			//
			// By PID alone, "add a hundred and twenty bots" added the hundred and
			// twenty benched veterans with the lowest PIDs - measured: PIDs 4 to
			// 301, fifty-two of them between 5 and 30 and sixty-eight past 31 -
			// while the hundred and sixty-two characters that had never played
			// (level 1 to 4, PIDs 1342 to 1503) sat at the end of the queue and
			// could not be reached by any slider. Three tiers instead: the cohort
			// that has played within the week keeps its place, so a restart
			// brings back the same world; newcomers come next, so growing the
			// slider is how fresh characters enter it; the benched veterans
			// last. A fresh install is one tier and unchanged.
			"AND pi.empire IN (1,2,3) ORDER BY "
			"CASE WHEN p.level>4 AND p.last_play>NOW()-INTERVAL 7 DAY THEN 0 "
			"WHEN p.level<=4 THEN 1 ELSE 2 END, l.pid";

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
		unsigned int empire = 0;
		if (row[3])
			str_to_number(empire, row[3]);
		if (pid != 0 && empire >= 1 && empire <= 3)
		{
			m_setRegisteredBots.insert(pid);
			TPlayerBotAccount account;
			account.dwID = 0;
			account.bEmpire = (BYTE)empire;
			if (row[1])
				str_to_number(account.dwID, row[1]);
			if (row[2])
				account.strLogin = row[2];
			m_mapBotAccounts[pid] = account;
		}
	}

	m_bRegistryAvailable = !m_setRegisteredBots.empty();
	if (!m_bRegistryAvailable)
	{
		sys_err("PLAYERBOT_AUTH: registry has no valid seeded identities; refusing every bot spawn");
		return false;
	}

	int perEmpire[playerbot_empire_rules::EMPIRE_COUNT];
	CountRegisteredPerEmpire(perEmpire, playerbot_empire_rules::EMPIRE_COUNT);
	sys_log(0, "PLAYERBOT_AUTH: loaded %u registered bot identities (shinsoo=%d chunjo=%d jinno=%d)",
			(unsigned int)m_setRegisteredBots.size(),
			perEmpire[playerbot_empire_rules::EMPIRE_SHINSOO],
			perEmpire[playerbot_empire_rules::EMPIRE_CHUNJO],
			perEmpire[playerbot_empire_rules::EMPIRE_JINNO]);
	ReportPlayerBotRegistryShortfall((unsigned int)m_setRegisteredBots.size());
	return true;
}

// Why the cohort is smaller than the seed, in one line.
//
// The query above is a single conjunction: a row that fails any of six
// conditions disappears without a word, and the only number anybody sees is
// the total. An operator who asks the launcher for a thousand bots and gets
// six hundred and fifty has nothing to go on - reported from the Discord
// exactly that way - so the same joins are counted again, one column per
// reason, and the answer is printed once at startup.
//
// LEFT JOINs and conditional sums, because the point is to count what the
// working query threw away. It runs once per process and touches the same
// rows the load already read.
void CPlayerBotManager::ReportPlayerBotRegistryShortfall(unsigned int usable)
{
	const char* query =
			"SELECT COUNT(*),"
			" SUM(l.seed_version<>1 OR l.state NOT IN ('complete','adopted')),"
			" SUM(p.id IS NULL),"
			" SUM(p.id IS NOT NULL AND a.id IS NULL),"
			" SUM(a.id IS NOT NULL AND pi.id IS NULL),"
			" SUM(a.id IS NOT NULL AND BINARY a.login<>BINARY CONCAT('playerbot_',"
			"  LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0'))),"
			" SUM(a.id IS NOT NULL AND BINARY a.social_id<>BINARY CONCAT('9',LPAD(l.pid-3,12,'0'))),"
			" SUM(pi.id IS NOT NULL AND (pi.pid1<>l.pid OR pi.pid2<>0 OR pi.pid3<>0 OR pi.pid4<>0)),"
			" SUM(pi.id IS NOT NULL AND pi.empire<>2) "
			"FROM common.playerbot_seed_state AS l "
			"LEFT JOIN player.player AS p ON p.id=l.pid "
			"LEFT JOIN account.account AS a ON a.id=p.account_id "
			"LEFT JOIN player.player_index AS pi ON pi.id=a.id";

	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return;
	MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
	if (!row)
		return;

	DWORD value[9];
	for (int i = 0; i < 9; ++i)
	{
		value[i] = 0;
		if (row[i])
			str_to_number(value[i], row[i]);
	}
	sys_log(0, "PLAYERBOT_AUTH: registry rows=%u usable=%u rejected: "
			"not_complete=%u no_character=%u no_account=%u no_index=%u "
			"login=%u social_id=%u other_characters=%u wrong_empire=%u",
			(unsigned int)value[0], usable, (unsigned int)value[1],
			(unsigned int)value[2], (unsigned int)value[3], (unsigned int)value[4],
			(unsigned int)value[5], (unsigned int)value[6], (unsigned int)value[7],
			(unsigned int)value[8]);
}

BYTE CPlayerBotManager::GetRegisteredEmpire(DWORD dwPlayerID)
{
	if (!LoadRegisteredBots())
		return 0;
	TPlayerBotAccountMap::const_iterator it = m_mapBotAccounts.find(dwPlayerID);
	return it == m_mapBotAccounts.end() ? 0 : it->second.bEmpire;
}

void CPlayerBotManager::CountRegisteredPerEmpire(int* out, int size)
{
	for (int i = 0; i < size; ++i)
		out[i] = 0;
	// The bootstrap asks for these counts before it asks for any spawn, so this
	// is the call that loads the registry. Without it the split had nothing to
	// divide and every kingdom was allotted nothing - measured: the core came
	// up with no bots at all and not one PLAYERBOT line in the log.
	if (!LoadRegisteredBots())
		return;
	for (TPlayerBotAccountMap::const_iterator it = m_mapBotAccounts.begin();
			it != m_mapBotAccounts.end(); ++it)
	{
		const int empire = (int)it->second.bEmpire;
		if (empire > 0 && empire < size)
			++out[empire];
	}
}

bool CPlayerBotManager::IsRegistered(DWORD dwPlayerID)
{
	return LoadRegisteredBots() &&
			m_setRegisteredBots.find(dwPlayerID) != m_setRegisteredBots.end();
}

// Queues the first `count` registered identities and sends the first batch.
// The rest go out from Update, a batch a second, so the cohort takes
// PLAYERBOT_SPAWN_WINDOW to arrive instead of one second. Returns how many
// were scheduled - the startup line in input_db.cpp prints this as
// registered_started, and it is still the number that will be in the world
// a minute later.
size_t CPlayerBotManager::SpawnRegistered(size_t count, BYTE bEmpire)
{
	if (count == 0 || bEmpire < 1 || bEmpire > 3 || !LoadRegisteredBots())
		return 0;

	// Only this kingdom's identities, and added to whatever is already waiting
	// rather than replacing it: a core that hosted two kingdoms' villages would
	// otherwise throw the first queue away when it asked for the second.
	size_t selected = 0;
	for (TRegisteredPlayerBotSet::const_iterator it = m_setRegisteredBots.begin();
			it != m_setRegisteredBots.end() && selected < count; ++it)
	{
		if (GetRegisteredEmpire(*it) != bEmpire)
			continue;
		if (m_setScheduledBots.find(*it) != m_setScheduledBots.end())
			continue;
		m_dequePendingSpawns.push_back(*it);
		m_setScheduledBots.insert(*it);
		++selected;
	}

	const size_t batches = std::max<size_t>(1, PLAYERBOT_SPAWN_WINDOW / PLAYERBOT_SPAWN_BATCH_INTERVAL);
	m_uSpawnBatchSize = std::max<size_t>(1,
			(m_setScheduledBots.size() + batches - 1) / batches);
	m_dwSpawnWindowStarted = get_dword_time();
	m_uSpawnWindowTotal = m_setScheduledBots.size();
	m_dwNextSpawnBatchTime = 0;
	sys_log(0, "PLAYERBOT: staggered spawn empire=%u scheduled=%u total=%u batch=%u every=%ums window=%ums",
			(unsigned int)bEmpire, (unsigned int)selected,
			(unsigned int)m_uSpawnWindowTotal, (unsigned int)m_uSpawnBatchSize,
			PLAYERBOT_SPAWN_BATCH_INTERVAL, PLAYERBOT_SPAWN_WINDOW);
	SpawnPendingBatch(get_dword_time());
	return selected;
}

// One batch from the queue, if one is due. Called from Update every tick and
// once directly from SpawnRegistered.
void CPlayerBotManager::SpawnPendingBatch(DWORD dwNow)
{
	if (m_dequePendingSpawns.empty() || dwNow < m_dwNextSpawnBatchTime)
		return;
	m_dwNextSpawnBatchTime = dwNow + PLAYERBOT_SPAWN_BATCH_INTERVAL;
	size_t sent = 0;
	while (!m_dequePendingSpawns.empty() && sent < m_uSpawnBatchSize)
	{
		const DWORD pid = m_dequePendingSpawns.front();
		m_dequePendingSpawns.pop_front();
		Spawn(pid, GetRegisteredEmpire(pid));
		++sent;
	}
	if (m_dequePendingSpawns.empty())
		sys_log(0, "PLAYERBOT: staggered spawn complete scheduled=%u over=%ums",
				(unsigned int)m_uSpawnWindowTotal,
				(unsigned int)(dwNow - m_dwSpawnWindowStarted));
}

// Put back whoever the world has lost.
//
// SpawnRegistered fills the queue once and drains it over a minute, and that
// was the whole of it: nothing ever looked again. A bot whose load failed, or
// which left the world later, stayed gone until somebody restarted the server -
// which is what "I asked for a thousand, six hundred and fifty arrived, and an
// hour later I had three hundred and fifty" looks like from the inside.
//
// Bounded by what was actually asked for: only the identities inside the
// original window are considered, so this restores the cohort and never grows
// it. It runs a minute apart and reuses the same staggered queue, so a hundred
// missing bots come back the way they arrived rather than all in one tick.
void CPlayerBotManager::TopUpMissingBots(DWORD dwNow)
{
	if (m_setScheduledBots.empty() || !m_dequePendingSpawns.empty())
		return;
	if (m_dwNextTopUpTime != 0 && dwNow < m_dwNextTopUpTime)
		return;
	m_dwNextTopUpTime = dwNow + PLAYERBOT_TOPUP_INTERVAL;
	if (!LoadRegisteredBots())
		return;

	// Counted against exactly the identities this core asked for. It used to be
	// "the first N of the registry", which is the same set only while the
	// registry holds one kingdom - with three it is somebody else's prefix.
	size_t live = 0;
	std::deque<DWORD> missing;
	for (std::set<DWORD>::const_iterator it = m_setScheduledBots.begin();
			it != m_setScheduledBots.end(); ++it)
	{
		if (CHARACTER_MANAGER::instance().FindByPID(*it) != NULL)
			++live;
		else
			missing.push_back(*it);
	}
	if (missing.empty())
		return;

	m_dequePendingSpawns = missing;
	m_uSpawnBatchSize = std::max<size_t>(1, m_uSpawnBatchSize);
	m_dwNextSpawnBatchTime = 0;
	sys_log(0, "PLAYERBOT: topping up asked=%u live=%u missing=%u",
			(unsigned int)m_setScheduledBots.size(), (unsigned int)live,
			(unsigned int)missing.size());
	SpawnPendingBatch(dwNow);
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

		// A fresh state has every timer at zero, so a bot's first refine, gear
		// pass, shopping decision and bonus check all ran on its first tick -
		// and with the whole population logging in together, on the same tick
		// as everybody else's. Spread them across the login window by pid.
		// Combat, potions and the watchdog are not touched: a bot that arrives
		// among monsters still fights at once.
		const DWORD spread = PlayerBotNavHash(dwPID ^ 0x46495253U) % PLAYERBOT_FIRST_PASS_SPREAD;
		state.dwNextRefineCheckTime = now + spread;
		state.dwNextEquipmentCheckTime = now + spread / 2;
		state.dwNextGearAttemptTime = now + spread / 2;
		state.dwNextShoppingTime = now + spread;
		state.dwNextBonusCheckTime = now + spread;
		state.dwNextSoulStoneTime = now + spread;

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
		state.dwNextSoulStoneTime = now + number(3000, 15000);
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

// The wait for the engine's equipment window, shared by the two places the
// gear pass runs. True while the bot should stand and claim the tick: the
// engine refuses EquipItem within 1.5 s of an attack or a cast, so a piece
// waiting in the bag needs the fighting to stop for a moment. Bounded by
// PLAYERBOT_EQUIP_PENDING_MAX_MS, and a window that never comes is not asked
// for again before PLAYERBOT_EQUIP_PENDING_RETRY_MS.
static bool HoldPlayerBotForEquipWindow(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
{
	if (!state.bEquipPending)
	{
		state.dwEquipPendingSince = 0;
		return false;
	}
	if (state.dwEquipPendingSince == 0)
		state.dwEquipPendingSince = dwNow;
	if (dwNow - state.dwEquipPendingSince > PLAYERBOT_EQUIP_PENDING_MAX_MS)
	{
		PlayerBotLogThrottled("equip_pending_abandoned", dwNow,
				"PLAYERBOT_GEAR: equip window never came pid=%u name=%s map=%ld pos=(%ld,%ld) waited_ms=%u last_attack_ms=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
				dwNow - state.dwEquipPendingSince, dwNow - ch->GetLastAttackTime());
		state.bEquipPending = false;
		state.dwEquipPendingSince = 0;
		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIP_PENDING_RETRY_MS;
		return false;
	}
	state.dwTargetVID = 0;
	ch->SetVictim(NULL);
	ch->Stop();
	return true;
}

void CPlayerBotManager::Update()
{
	const DWORD dwNow = get_dword_time();
	const DWORD dwTickStartUs = PlayerBotClockUs();

	// The next batch of the cohort, if one is due - see PLAYERBOT_SPAWN_WINDOW.
	SpawnPendingBatch(dwNow);
	// And a minute apart, whoever is missing from it.
	TopUpMissingBots(dwNow);

	// Once for the whole population: the panel may have moved a weight since
	// the last tick, and every bot planned below must see the same numbers.
	RefreshPlayerBotWeights(dwNow);
	ManagePlayerBotNight(dwNow);

	static DWORD s_dwTick = 0;
	++s_dwTick;

	// Once a minute: what the last minute cost. Read this before tuning any
	// budget - the first version of the material errand was diagnosed from CPU
	// alone and put the whole population's scans in one second.
	if (s_dwPlayerBotLoadReportTime == 0)
		s_dwPlayerBotLoadReportTime = dwNow;
	else if (dwNow - s_dwPlayerBotLoadReportTime >= PLAYERBOT_LOAD_REPORT_INTERVAL)
	{
		sys_log(0, "PLAYERBOT_LOAD: bots=%u ticks=%u tick_ms=%u tick_max_ms=%u targets=%u misses=%u target_ms=%u snapshot_ms=%u plans=%u deferred=%u resumed=%u cached=%u plan_ms=%u p64=%u/%ums p256=%u/%ums p1024=%u/%ums pfar=%u/%ums scans=%u scan_ms=%u saves=%u watchdog=%u over=%ums",
				(unsigned int)m_mapBots.size(), s_uPlayerBotLoadTicks,
				s_uPlayerBotLoadTickUs / 1000, s_uPlayerBotLoadTickMaxUs / 1000,
				s_uPlayerBotLoadTargetSearches, s_uPlayerBotLoadTargetMisses,
				s_uPlayerBotLoadTargetUs / 1000, s_uPlayerBotLoadSnapshotUs / 1000,
				s_uPlayerBotLoadPlans, s_uPlayerBotLoadPlanDeferred, s_uPlayerBotLoadPlanResumed, s_uPlayerBotLoadPlanCached,
				s_uPlayerBotLoadPlanUs / 1000,
				s_uPlayerBotLoadPlanBucket[0], s_uPlayerBotLoadPlanBucketUs[0] / 1000,
				s_uPlayerBotLoadPlanBucket[1], s_uPlayerBotLoadPlanBucketUs[1] / 1000,
				s_uPlayerBotLoadPlanBucket[2], s_uPlayerBotLoadPlanBucketUs[2] / 1000,
				s_uPlayerBotLoadPlanBucket[3], s_uPlayerBotLoadPlanBucketUs[3] / 1000,
				s_uPlayerBotLoadScans, s_uPlayerBotLoadScanUs / 1000,
				s_uPlayerBotLoadSaves, s_uPlayerBotLoadWatchdog,
				(unsigned int)(dwNow - s_dwPlayerBotLoadReportTime));
		for (int b = 0; b < 4; ++b)
			s_uPlayerBotLoadPlanBucket[b] = s_uPlayerBotLoadPlanBucketUs[b] = 0;
		s_uPlayerBotLoadPlanDeferred = s_uPlayerBotLoadPlanResumed = s_uPlayerBotLoadPlanCached = 0;
		s_uPlayerBotLoadPlans = s_uPlayerBotLoadScans = s_uPlayerBotLoadSaves = s_uPlayerBotLoadWatchdog = 0;
		s_uPlayerBotLoadPlanUs = s_uPlayerBotLoadScanUs = s_uPlayerBotLoadTickUs = s_uPlayerBotLoadTickMaxUs = s_uPlayerBotLoadTicks = 0;
		s_uPlayerBotLoadTargetSearches = s_uPlayerBotLoadTargetMisses = s_uPlayerBotLoadTargetUs = s_uPlayerBotLoadSnapshotUs = 0;
		s_dwPlayerBotLoadReportTime = dwNow;
	}
	ReportPlayerBotSpotMemory(dwNow);
	s_bPlayerBotM2CensusPass = s_dwPlayerBotM2CensusTime == 0 ||
			dwNow - s_dwPlayerBotM2CensusTime >= 60000;
	if (s_bPlayerBotM2CensusPass)
		s_dwPlayerBotM2CensusTime = dwNow;
	RefreshPlayerBotMarketLedger(dwNow);

	// The per-map census the raid cap reads (GetPlayerBotsOnMap), one pass
	// over the descriptors before the tick proper; nothing else counts them.
	s_mapPlayerBotsOnMap.clear();
	for (TPlayerBotMap::iterator it = m_mapBots.begin(); it != m_mapBots.end(); ++it)
	{
		LPCHARACTER c = it->second ? it->second->GetCharacter() : NULL;
		if (c && !c->IsDead())
			++s_mapPlayerBotsOnMap[c->GetMapIndex()];
	}

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
				// This pass lands about half of all killing blows, and the full
				// tick cannot count them later: it replaces a target it finds
				// dead before it reaches the attack block, so the credit there
				// only ever sees a live monster. Without this line the battle
				// horse trial counted roughly every other kill.
				NotePlayerBotBattleHorseKill(ch, state, quickTarget);
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
				IsPlayerBotMonkeyMap(ch->GetMapIndex()) ||
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
					else if (IsPlayerBotMonkeyMap(currentMap))
						GetPlayerBotMonkeyArrival(currentMap, fallbackX, fallbackY);
					else
						GetPlayerBotFrontierArrival(currentMap, fallbackX, fallbackY);
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

		// Before anything may claim the tick: which Monkey Dungeon chamber this
		// bot is in now, since a portal it walked past has already moved it.
		UpdatePlayerBotMonkeyChamber(ch, state, dwNow);

		// The census, once a minute: why each level-40 bot in Bokjung is there.
		if (s_bPlayerBotM2CensusPass && ch->GetLevel() >= 40 &&
				ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2)
		{
			NotePlayerBotM2Stay(ClassifyPlayerBotTownStay(ch, state, dwNow));
			// A rotating handful explains itself in full. The rotation is by
			// minute so following one bot across a cycle is possible without
			// eight hundred lines a minute.
			if ((ch->GetPlayerID() + dwNow / 60000U) % 24U == 0)
				ReportPlayerBotM2Why(ch, state, dwNow);
		}

		// A service that cannot be finished must not hold the bot for ever: past
		// PLAYERBOT_SERVICE_GIVE_UP the recovery is abandoned with a reason, and
		// the ordinary planner has the bot back. Better a bot that hunts than a
		// bot that waits on a merchant it will never reach.
		if (state.bServicePending && state.dwServiceSince != 0 &&
				dwNow - state.dwServiceSince > PLAYERBOT_SERVICE_GIVE_UP)
		{
			sys_log(0, "PLAYERBOT_SERVICE: gave up pid=%u name=%s map=%ld age_ms=%u red_low=%d blocked=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
					dwNow - state.dwServiceSince, NeedsPlayerBotPotions(ch) ? 1 : 0,
					BlocksPlayerBotTravel(ch) ? 1 : 0);
			state.bServicePending = false;
			state.dwServiceRetryAt = 0;
			state.dwServiceSince = 0;
		}
		// The need may simply have gone away - a bot that bought from a counter
		// beside it, or whose gear turned up as loot.
		if (state.bServicePending && !NeedsPlayerBotPotions(ch) &&
				!BlocksPlayerBotTravel(ch))
		{
			state.bServicePending = false;
			state.dwServiceRetryAt = 0;
			state.dwServiceSince = 0;
		}

		// And whether the defence episode is over. It ends when the fighting
		// has actually stopped, not when its timer runs out: otherwise the next
		// attacker starts a fresh one and the bound means nothing.
		if (state.dwDefenceEpisodeStart != 0 &&
				(state.dwLastCombatActionTime == 0 ||
				 dwNow - state.dwLastCombatActionTime > PLAYERBOT_DEFENCE_QUIET_TIME))
		{
			state.dwDefenceEpisodeStart = 0;
			state.dwDefenceTargetVID = 0;
		}

		ManagePlayerBotStats(ch, state, dwNow);
		ManagePlayerBotSkills(ch, state, dwNow);
		if (RescuePlayerBotWithoutSectree(ch, state, dwNow))
			continue;

		if (ManagePlayerBotPrivateShop(ch, state, dwNow))
			continue;

		ManagePlayerBotSkillBooks(ch, state, dwNow);
		ManagePlayerBotSoulStones(ch, state, dwNow);
		ManagePlayerBotThirdHand(ch, state, dwNow);
		// The gear pass, early. It used to sit at the bottom of the tick, past
		// the stall, the loot, the horse, the fishing, the travel, the town
		// visit and the wander, each of which claims the tick - so a bot that
		// was always doing one of them never looked at its bag: a warrior of
		// twenty-eight fought with the level-one sword at +6 (attack 60) with
		// a Long Sword +4 (82) in the bag, 234 of 970 bots the same way. Not
		// behind an open counter (the table points at cells), not during a
		// town visit (the blacksmith phase moves gear itself), not with a rod
		// in the hand, not at the stable.
		if (!ch->GetMyShop() && !state.bVisitingShop && !state.bFishingSession &&
				!state.bVisitingStable)
		{
			if (ManagePlayerBotEquipment(ch, state, dwNow))
				continue;
			if (HoldPlayerBotForEquipWindow(ch, state, dwNow))
				continue;
		}
		// Opening a chest belongs with the other upkeep, not after it. Down at
		// the bottom of the tick - past combat, loot, travel, the town and the
		// wandering, each of which claims the tick - it was reached so rarely
		// that 589 bots sat on 9624 Moonlight chests, the largest stack 106
		// deep, and opened 190 in an hour between them. Their bags were not the
		// problem: 29 cells of 90 in use on average, none above 84.
		ManagePlayerBotChests(ch, state, dwNow);
		ManagePlayerBotStackMerge(ch, state, dwNow);
		// The catch, wherever the bot happens to be standing. It used to be
		// opened only between casts, so an angler that walked away from the bank
		// carried its fish around instead - and a live fish does not stack, so a
		// bag with thirty of them has no room for anything the bot is out there
		// for. One item a tick, like the chests above.
		ProcessPlayerBotCatch(ch);
		ManagePlayerBotHairDye(ch);
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
		RollPlayerBotMetinExpedition(ch, state, dwNow);
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
			// The stone is gone - broken, or abandoned. Either way the loot pass
			// gets its window to go for what lies round it.
			state.dwStoneBrokenTime = dwNow;
			ResetPlayerBotStoneProgress(state);
		}

		// And the same question for an ordinary monster, which until now could
		// hold a bot for as long as the two of them healed at the same rate.
		if (!bFightingMetin && curTarget && !curTarget->IsDead() &&
				ShouldPlayerBotAbandonFight(ch, curTarget, state, dwNow))
			curTarget = NULL;
		else if (curTarget == NULL && state.dwFightProgressVID != 0)
			ResetPlayerBotFightProgress(state);

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

		// Spending time in town once the errand that brought the bot here is
		// done - and above the travel pass, not below it. The rod carries a
		// level limit of thirty, so every angler is old enough for the frontier
		// and the travel pass walked each one straight back out of Joan on the
		// tick its session ended: the rest never got a turn. It claims the tick
		// like fishing does, for a bounded few minutes, and ends the moment
		// anything real wants the bot.
		if (ManagePlayerBotTownLinger(ch, state, dwNow))
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
			for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM it = ch->GetInventoryItem(cell);
				if (it)
				{
					++occupiedItems;
					occupiedGridCells += std::max(1, (int)it->GetSize());
				}
			}

			const bool bWeaponMissing = (ch->GetWear(WEAR_WEAPON) == NULL);
			// Item count is not inventory usage: weapons and armour occupy 2-3
			// vertical cells.  Keep a generous reserve for a high-rate Metin drop and
			// visit town before no contiguous 3-cell slot remains.
			const bool bInventoryFull =
					occupiedGridCells * 100 >= INVENTORY_MAX_NUM * 45 ||
					ch->GetEmptyInventory(3) < 0;
			// The same question the planner asked. It used to be a different one:
			// this counted stacks rather than potions, looked at four red vnums
			// and no blue ones at all, and only fired on an empty belt in a
			// half-full bag - while NeedsPlayerBotPotions, which decides the
			// goal, counts individual potions and answers for red under 150 or
			// blue under 100. So a bot with one stack of 32 reds and 56 blues
			// carried BOT_GOAL_RESTOCK and never began the visit that would end
			// it, and stood in Bokjung fighting whatever walked past instead:
			// 116 bots of level 40 and over were on that map when this was
			// found. The 5-10 minute shop cooldown above still paces the retry,
			// and NeedsPlayerBotPotions wants the money for the trip, so a bot
			// that cannot afford potions does not loop between merchants.
			const bool bNeedsPotions = NeedsPlayerBotPotions(ch);
			const bool bNeedsRefine = HasPlayerBotRefineOpportunity(ch);
			const bool bNeedsGearUpgrade = bNeedsCoreGear || NeedsPlayerBotArrows(ch);
			const bool bNeedsSellRun = CountPlayerBotJunkItems(ch) >= 12;
			const bool bNeedsPotionCleanup = HasPlayerBotExcessPotions(ch);

			if (bNeedsProfession || bInventoryFull || bNeedsPotions || bWeaponMissing ||
					bNeedsRefine || bNeedsGearUpgrade || bNeedsSellRun ||
					bNeedsPotionCleanup)
				StartPlayerBotTownVisit(ch, state, dwNow);
		}

		// A visit is an adaptive, persistent route. The bot only visits specialists
		// needed by its current inventory: weapon merchant, armor merchant, Misc
		// Merchant and/or blacksmith. Goals never change in the middle of a route.
		if (HandlePlayerBotTownVisit(ch, state, dwNow))
			continue;

		// A normal horse is for transport only, so it comes off before buffs
		// and combat: a level-1 horse must never produce a mounted attack. A
		// battle horse is a different animal and stays - this line used to
		// dismount it too, which is why a rider was seen hacking a metin on
		// foot with its horse standing beside it. Whether it actually fights
		// from the saddle is then the target's business, decided where the
		// target is known.
		//
		// Only when there is a fight to get off for. The wander pass at the
		// bottom of the tick mounts for a long leg, and taking the horse away
		// here at the top of the next one, unconditionally, ran every hunting
		// map through a loop: mounted, dismounted, mounted - each of them
		// clearing the route - 133 000 times in twenty-eight minutes across
		// 261 bots, and not one step of the leg walked. That is how 1.30.28
		// came to strand its raiders among the trash ("heading for boss" every
		// few minutes, nobody within three kilometres of the Spider Queen).
		// A rider with no target keeps the saddle; the target section below
		// climbs down the moment it picks one, and the buff and multi-pull
		// passes stay out of the saddle themselves.
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch) &&
				(state.dwTargetVID != 0 || ch->GetVictim() != NULL) &&
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
			else if (ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M1 ||
					ch->GetMapIndex() == PLAYERBOT_MAP_CHUNJO_M2)
			{
				state.dwEmergencyScavengeUntil = 0;
				StartPlayerBotTownVisit(ch, state, dwNow);
				ch->Stop();
			}
			else
			{
				// No merchant on this map, so a town visit cannot start here and
				// "start it and stop" was a bot standing at an arrival point for
				// as long as the map lasted. The world travel knows the way to
				// town (BlocksPlayerBotTravel names the empty bow), and failing
				// that the wander at least walks.
				state.dwEmergencyScavengeUntil = 0;
				if (!ManagePlayerBotWorldTravel(ch, state, dwNow))
					ManagePlayerBotWandering(ch, state, dwNow);
			}
			continue;
		}

		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		UseUtilityPotions(ch, state, dwNow);
		UsePlayerBotBoosters(ch, state, dwNow);
		ManagePlayerBotScrollRefine(ch, state, dwNow);
		// This also catches a bot loaded from the database at critically low HP
		// after a server restart.  Do not let it immediately reacquire a target.
		// One exception to walking away, and it is about what the target is
		// rather than about how much health is left: a Metin stone within a
		// sliver of breaking. See PLAYERBOT_STONE_FINISH_STONE_HP_PERCENT.
		bool bFinishingStone = false;
		if (!state.bRecoveringAfterDeath && ch->GetMaxHP() > 0 &&
				ch->GetHP() * 100 > ch->GetMaxHP() * PLAYERBOT_STONE_FINISH_OWN_HP_PERCENT)
		{
			LPCHARACTER stoneTarget = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			bFinishingStone = stoneTarget && stoneTarget->IsStone() &&
					!stoneTarget->IsDead() && stoneTarget->GetMaxHP() > 0 &&
					stoneTarget->GetHP() * 100 <=
						stoneTarget->GetMaxHP() * PLAYERBOT_STONE_FINISH_STONE_HP_PERCENT;
		}
		if (!bFinishingStone && !state.bRecoveringAfterDeath && ch->GetMaxHP() > 0 &&
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

		// A shield slot is not a core slot for a bow or a two-handed weapon: the
		// engine never fills it, and counting it kept every archer "missing a
		// core slot" for life - which is what armed the pause below for the
		// twelve archers found standing at arrival points, silent, for twenty
		// minutes at a time.
		const bool bMissingCoreWearSlot = ch->GetWear(WEAR_WEAPON) == NULL ||
				ch->GetWear(WEAR_BODY) == NULL ||
				(PlayerBotWantsShield(ch) && ch->GetWear(WEAR_SHIELD) == NULL) ||
				ch->GetWear(WEAR_HEAD) == NULL || ch->GetWear(WEAR_FOOTS) == NULL;
		if (ManagePlayerBotEquipment(ch, state, dwNow))
			continue;
		if (HoldPlayerBotForEquipWindow(ch, state, dwNow))
			continue;
		(void)bMissingCoreWearSlot;

		// A buff is a complete action for this AI update.  Continuing into the
		// attack code used to emit a second skill packet in the very same tick.
		if (ManagePlayerBotCombatBuffs(ch, state, dwNow))
			continue;
		if (HandlePlayerBotMultiPull(ch, state, dwNow))
			continue;
		// The Archer's luring course. It owns movement and the shot for as long
		// as it runs - including the ticks it spends waiting for the bow - so it
		// goes here, before target acquisition and after everything that keeps a
		// bot alive. The multi-pull above can never be running at the same time:
		// it refuses a bot that is in a party, and this one needs five.
		if (HandlePlayerBotLureCourse(ch, state, dwNow))
			continue;
		// Before anything else looks at where this bot is: a half-completed warp
		// leaves the position and the sector disagreeing, and the next logout
		// saves coordinates no login can ever load.
		if (IsPlayerBotPositionOffItsMap(ch))
		{
			sys_log(0, "PLAYERBOT_WORLD: position off its map pid=%u name=%s map=%ld pos=(%ld,%ld) belongs_to=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(),
					ch->GetX(), ch->GetY(),
					SECTREE_MANAGER::instance().GetMapIndex(ch->GetX(), ch->GetY()));
			if (TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_CHUNJO_M2,
					PLAYERBOT_M2_FROM_M3_X, PLAYERBOT_M2_FROM_M3_Y, dwNow,
					"half_warp_recovery"))
				continue;
		}
		// Before target acquisition on purpose: a bot that has stood in the same
		// place for five minutes is walked to the next hunting hub, and it can
		// only do that on a tick where nothing else picks a monster for it.
		if (ManagePlayerBotRelocation(ch, state, dwNow))
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
		// Something ten levels up is not a fight a bot picks - but it is a
		// fight a bot is in, once that something is hitting it. The level cap
		// used to drop the target either way, so a bot set upon by anything
		// strong stood there swinging at nothing and died running. Breaking off
		// is the survival pass's decision and it still outranks this; what the
		// cap decides is what a bot walks up to, not what it answers.
		const bool bPartyCanContinue = !bTargetNeedsParty ||
				(target && target->GetVictim() == ch) ||
				CanPlayerBotPartyChallenge(ch, target, dwNow, NULL);

		if (!target || target->IsDead() || (!bTargetIsMonster && !bTargetIsStone) ||
			(bTargetIsStone && !IsPlayerBotMetinWorthFighting(ch, target)) ||
			// And the same question for an ordinary monster, on a clock: the
			// errand that justified this fight may have finished since it began.
			(bTargetIsMonster && !IsPlayerBotHeldTargetStillWorth(ch, target, state, dwNow)) ||
			!bPartyCanContinue ||
			IsPlayerBotSafeZone(ch->GetMapIndex(), target ? target->GetX() : ch->GetX(),
					target ? target->GetY() : ch->GetY()) ||
			target->GetMapIndex() != ch->GetMapIndex() ||
			DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY()) > PLAYERBOT_SEARCH_RANGE)
		{
			// Finish the group which is already fighting this bot (or its party)
			// before choosing a fresh, possibly distant spawn. This is the server-side
			// equivalent of a player clearing the pulled pack first.
			{
				TPlayerBotLoadTimer targetTimer(s_uPlayerBotLoadTargetUs);
				++s_uPlayerBotLoadTargetSearches;
				target = FindPlayerBotEngagedTarget(ch);
				// An engaged monster is usually self-defence and passes, but the
				// finder also returns what is fighting the party from across the
				// field - so it goes through the same filter as everything else
				// rather than round it.
				if (target && !IsPlayerBotTargetWorthNow(ch, target, state, dwNow))
					target = NULL;
				if (!target)
					target = FindDistributedTarget(ch, state, dwNow);
				if (!target)
					++s_uPlayerBotLoadTargetMisses;
			}
			state.dwTargetVID = target ? (DWORD)target->GetVID() : 0;
			if (target && target->IsMonster())
			{
				RememberPlayerBotSpotFight(ch->GetMapIndex(), target->GetX(), target->GetY(), dwNow);
				// If this one is hitting the bot, the defence episode starts here
				// and nowhere else - a clock that is restarted on every tick, or
				// on every blow, bounds nothing at all.
				NotePlayerBotDefenceEpisode(ch, state, target, dwNow);
			}

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
			// Nothing in sight: the one moment a material errand may take the
			// bot somewhere on purpose instead of the wander picking a hub.
			if (StartPlayerBotMaterialHunt(ch, state, dwNow))
				continue;
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
		// A transport horse is left here, on the tick the target is chosen,
		// and the swing waits for the next one - the way the old top-of-tick
		// dismount spaced them. Never a mounted attack from a level-1 horse.
		else if (ch->IsRiding() &&
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "dismount_for_target"))
			continue;

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

		if (ExecutePlayerBotAttackSkill(ch, target, state, dwNow))
		{
			NotePlayerBotBattleHorseKill(ch, state, target);
			continue;
		}

		ExecutePlayerBotBasicAttack(ch, target, state, dwNow);
		NotePlayerBotBattleHorseKill(ch, state, target);

	}

	// The census was taken over the pass that has just finished, so it is
	// written here rather than at the top: one line, one minute, every bot of
	// level forty and over standing in Bokjung counted once.
	if (s_bPlayerBotM2CensusPass)
		ReportPlayerBotM2Census();

	// Publish one compact, atomic snapshot per game core. The web panel reads
	// these files from the shared read-only game-var volume, so it sees the real
	// AI decision instead of inferring an activity from party membership or PID.
	static DWORD s_dwNextStatusSnapshotTime = 0;
	if (dwNow >= s_dwNextStatusSnapshotTime)
	{
		s_dwNextStatusSnapshotTime = dwNow + PLAYERBOT_STATUS_SNAPSHOT_INTERVAL;
		TPlayerBotLoadTimer snapshotTimer(s_uPlayerBotLoadSnapshotUs);
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

	const DWORD dwTickUs = PlayerBotClockUs() - dwTickStartUs;
	s_uPlayerBotLoadTickUs += dwTickUs;
	if (dwTickUs > s_uPlayerBotLoadTickMaxUs)
		s_uPlayerBotLoadTickMaxUs = dwTickUs;
	++s_uPlayerBotLoadTicks;
}

bool CPlayerBotManager::IsManaged(DWORD dwPlayerID) const
{
	return m_mapBots.find(dwPlayerID) != m_mapBots.end();
}

size_t CPlayerBotManager::GetCount() const
{
	return m_mapBots.size();
}

void CPlayerBotManager::GetAvailableBots(std::vector<DWORD>& out, size_t limit)
{
	out.clear();
	if (!LoadRegisteredBots())
		return;
	for (TRegisteredPlayerBotSet::const_iterator it = m_setRegisteredBots.begin();
			it != m_setRegisteredBots.end() && out.size() < limit; ++it)
		if (m_mapBots.find(*it) == m_mapBots.end())
			out.push_back(*it);
}

void CPlayerBotManager::OnPlayerShout(LPCHARACTER ch, const char* szText)
{
	HandlePlayerShoutForTrade(ch, szText);
}

void CPlayerBotManager::OnPlayerWhisper(LPCHARACTER from, LPCHARACTER bot, const char* szText)
{
	HandlePlayerWhisperToBot(from, bot, szText);
}
