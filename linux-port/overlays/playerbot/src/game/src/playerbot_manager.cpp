#include "stdafx.h"
#include "playerbot_manager.h"
#include "playerbot_empire_rules.h"
#include "playerbot_channel_rules.h"
#include "playerbot_world_rules.h"
#include "playerbot_event_rules.h"
#include "playerbot_stall_rules.h"
#include "playerbot_persona_rules.h"
#include "playerbot_lure_order_rules.h"

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
// CMobManager: the conversation names the mob a hunting mission is after.
#include "mob_manager.h"
#include "log.h"
#include "config.h"
#include "constants.h"
#include "battle.h"
#include "buffer_manager.h"
#include "motion.h"
#include "party.h"
#include "p2p.h"
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

// The names the fragments below were written against, on an engine that
// spells some of them differently. Empty on r40250.
#include "playerbot_engine_compat.h"

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
#include "ikarus_shop_manager.h"
#include "playerbot_offline_policy.h"
#endif
// "Scal i uporzadkuj", the one a player's inventory button asks for too.
#include "playerbot_arrange.h"
// The engine leaves two kinds of request here for the bot's tick to answer: a
// player's party invitation and a duel challenge. Both are inline and
// engine-free, and both belong OUTSIDE the ikashop guard above - the offline
// shop is mt2009's alone, these two are not, and putting them inside it cost a
// compile against r40250 with eleven "has not been declared". They come before
// the fragments because the health potion pass in playerbot_gear.h asks
// whether the bot is in a duel.
#include "playerbot_party_policy.h"
#include "playerbot_pvp_policy.h"
#include "playerbot_monkey_policy.h"
#include "pvp.h"
#include "playerbot_types.h"
#include "playerbot_price_tables.h"
#include "playerbot_item_tiers.h"
#include "playerbot_persona_tables.h"
#include "playerbot_weapon_atlas.h"
#include "playerbot_log.h"
#include "playerbot_config.h"
#include "playerbot_events.h"
// Iwakura's Bot Mood System: the moods and the notes the loot, the chests,
// the fishing and the blacksmith send it - early, so any of them may.
#include "playerbot_mood.h"
#include "playerbot_swing_timing.h"
#include "playerbot_navigation.h"
#include "playerbot_world_memory.h"
#include "playerbot_movement.h"
#include "playerbot_combat_value_policy.h"
#include "playerbot_battle_horse.h"
#include "playerbot_gear.h"
#include "playerbot_consumables.h"
#include "playerbot_activities.h"
#include "playerbot_mining.h"
#include "playerbot_herbalism.h"
#include "playerbot_unique_slots.h"
#include "playerbot_missions.h"
#include "playerbot_skills.h"
#include "playerbot_combat.h"
#include "playerbot_economy.h"
#include "playerbot_progression_needs.h"
#include "playerbot_bonus.h"
#include "playerbot_travel.h"
#include "playerbot_planner.h"
// Which of Iwakura's personalities claims a bot, the Grinder's lock and the
// Law of Advancement, and SLABY's pause and stop.
#include "playerbot_persona.h"
#include "playerbot_guild.h"
// Iwakura's names for a counter and the rules that pick one - pure, and asked
// by the town for a stand's name - then, after the town, what a real counter's
// lines are in their terms: what heads a +7..+9 piece or a soul stone is its
// asking price.
#include "playerbot_shop_name_rules.h"
#include "playerbot_town.h"
// Iwakura's gambler: the session a town visit turns into at its end.
#include "playerbot_gambler.h"
// Iwakura's Useful Items List: what a bot keeps at the storekeeper rather than
// sells, and when it lets it go.
#include "playerbot_lpp.h"
#include "playerbot_shop_signs.h"
#include "playerbot_offline_shop.h"
#include "playerbot_itemshop.h"
#include "playerbot_weapon_goal.h"
#include "playerbot_market.h"
#include "playerbot_offline_market.h"
// Forward declaration: the trade layer falls through to the deterministic
// conversation layer for ordinary whispers.
namespace { bool HandlePlayerBotConversation(LPCHARACTER player, LPCHARACTER bot, const char* text); }
#include "playerbot_chat_trade.h"
#include "playerbot_loot.h"
#include "playerbot_survival.h"
#include "playerbot_wandering.h"
#include "playerbot_status.h"
#include "playerbot_chat_conversation.h"
#include "playerbot_targeting.h"
#include "playerbot_guild_war.h"
// Iwakura's Anti-PK protocol and the stone hunter: the war's fight, turned on
// whoever struck the bot or is breaking its stone for another kingdom.
#include "playerbot_anti_pk.h"
#include "playerbot_demon_tower.h"
// Iwakura's social personalities: the companion's phase and its invitations
// to people, a companion Shaman's party buffs, and the mercenary's contracts.
#include "playerbot_companions.h"
#include "playerbot_lure.h"
#include "playerbot_admin.h"

namespace
{
	LPEVENT s_pkPlayerBotUpdateEvent = NULL;
	// CPlayerBotManager::StartWorldClock, until the first bot's Update runs.
	LPEVENT s_pkPlayerBotWorldEvent = NULL;

	// Defined beside the lock itself, further down.
	BYTE GetPlayerBotExpLockLevel(BYTE personality);

	// A dropper farms one band for good (PLAYERBOT_EXP_LOCK_*), and the lock
	// stops experience without giving any back: a bot drawn a dropper once it
	// had already passed that band farmed a table the engine fades to nothing
	// for it. Such a bot is not drawn one (PLAYERBOT_DROPPER_OUTGROWN_LEVELS),
	// and ManagePlayerBotExpLock lifts the lock from a bot that is no dropper.
	bool IsPlayerBotPastDropperBand(LPCHARACTER ch, BYTE personality)
	{
		const BYTE lockLevel = GetPlayerBotExpLockLevel(personality);
		return ch && lockLevel != 0 &&
				ch->GetLevel() > lockLevel + PLAYERBOT_DROPPER_OUTGROWN_LEVELS;
	}

	BYTE GetPlayerBotStablePersonality(LPCHARACTER ch, BYTE role)
	{
		if (!ch)
			return BOT_PERSONALITY_STEADY_ADVENTURER;
		if (role == BOT_ROLE_PARTY_FIGHTER)
			return BOT_PERSONALITY_TEAM_COMPANION;
		if (role == BOT_ROLE_METIN_HUNTER)
			return (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d444f50U) % 3U) == 0 &&
					!IsPlayerBotPastDropperBand(ch, BOT_PERSONALITY_METIN_DROPPER)
					? BOT_PERSONALITY_METIN_DROPPER : BOT_PERSONALITY_METIN_BREAKER;

		// Iwakura's community patch 2, point 4: under the personalities more
		// of the bots farm medals, four and a half times the old share, drawn
		// before the traders so the extra share is its own.
		if (IsPlayerBotPersonaEnabled() &&
				(int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d444c34U) % 1000U) <
					PLAYERBOT_MEDAL_DROPPER_EXTRA_PER_MILLE &&
				!IsPlayerBotPastDropperBand(ch, BOT_PERSONALITY_MEDAL_DROPPER))
			return BOT_PERSONALITY_MEDAL_DROPPER;

		// Traders are drawn before the rest: a bot that trades for a living is not
		// a variant of an adventurer, it is a different way of playing, and the
		// world was short of one.
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d524348U) %
				PLAYERBOT_MERCHANT_SHARE) == 0)
			return BOT_PERSONALITY_MERCHANT;
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x44524f50U) %
				PLAYERBOT_DROPPER_SHARE) == 0)
		{
			BYTE dropper = BOT_PERSONALITY_MEDAL_DROPPER;
			switch (PlayerBotNavHash(ch->GetPlayerID() ^ 0x4b494e44U) % 3U)
			{
				case 0: dropper = BOT_PERSONALITY_M3_DROPPER; break;
				case 1: dropper = BOT_PERSONALITY_M2_DROPPER; break;
				default: break;
			}
			// Past its band it plays as the adventurer the draw below makes it.
			if (!IsPlayerBotPastDropperBand(ch, dropper))
				return dropper;
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

	// How much of the population the PARTY slider admits to a party, in
	// thousandths: PLAYERBOT_PARTY_COHORT_PER_MILLE at the neutral weight,
	// scaled with it; on the frontier the whole map is the base.
	int GetPlayerBotPartyCohortPerMille(bool bFrontier)
	{
		const long base = bFrontier
				? PLAYERBOT_PARTY_FRONTIER_COHORT_PER_MILLE : PLAYERBOT_PARTY_COHORT_PER_MILLE;
		const long scaled = base * GetPlayerBotWeight(PLAYERBOT_WEIGHT_PARTY) / PLAYERBOT_WEIGHT_NEUTRAL;
		return (int)(scaled < 0 ? 0 : (scaled > 1000 ? 1000 : scaled));
	}

	// A bot's fixed place in the party draw, 0..999: a party fighter in the
	// first hundred, everyone else spread over the other nine hundred. Stable
	// by pid, so moving the slider moves the same bots in and out, and a
	// world on the same setting looks the same tomorrow.
	int GetPlayerBotPartyDraw(DWORD dwPID, const TPlayerBotAIState& state)
	{
		const DWORD hash = PlayerBotNavHash(dwPID ^ 0x50544452U);
		if (state.bBotRole == BOT_ROLE_PARTY_FIGHTER)
			return (int)(hash % 100U);
		return 100 + (int)(hash % 900U);
	}

	// Who may be in a party: the share of the population the PARTY slider
	// says, party fighters first. Until 2.0.18 the slider reached nothing
	// here - off the frontier the cohort was the role alone, a tenth of the
	// population drawn at login - and "Grupy (PT)" at 25 and at 250 gave the
	// same thirty-seven bots in groups out of a thousand (jaksiezabic).
	// On the frontier anyone of camp level - the Black Orc camps are a
	// party's work and eight of a tenth at one level on one island never
	// turns up - and every frontier map, not the valley alone: a map change
	// dissolves a party, so one made in the valley never reached V1 or
	// Sohan, and the Spider Queen and Nine Tails had nobody to fight them.
	bool IsPlayerBotPartyEligible(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch)
			return false;
		const bool bFrontier = IsPlayerBotFrontierMapIndex(ch->GetMapIndex());
		// The camp level is the frontier's own floor; the role has always
		// been above it, and still is.
		if (bFrontier && state.bBotRole != BOT_ROLE_PARTY_FIGHTER &&
				ch->GetLevel() < PLAYERBOT_ORC_VALLEY_PARTY_MIN_LEVEL)
			return false;
		// Under Iwakura's personalities the share is the same slider's, drawn
		// afresh at every companion phase (playerbot_companions.h); a bot on
		// a mercenary's contract is in the contract's party and no other, the
		// few minutes after a party are played alone, and a bag at eighty
		// percent - which ends a companion's party - does not start one: the
		// first measurement had seventeen bots in three minutes joining and
		// leaving for it on the next check.
		if (IsPlayerBotPersonaEnabled() && state.persona.bRestored)
		{
			const TPlayerBotPersona& p = state.persona;
			if (IsPlayerBotOnMercContract(ch->GetPlayerID()) || p.bBagFull ||
					(p.dwCompanionBreakUntil != 0 && get_dword_time() < p.dwCompanionBreakUntil))
				return false;
			return playerbot_persona::IsCompanionDraw(p.wCompanionDraw,
					GetPlayerBotPartyCohortPerMille(bFrontier));
		}
		return GetPlayerBotPartyDraw(ch->GetPlayerID(), state) <
				GetPlayerBotPartyCohortPerMille(bFrontier);
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

	// Is this party a player's rather than the bots' own? The leader's
	// descriptor answers it: a bot's says IsBot, a person's does not.
	bool IsPlayerBotHumanLedParty(LPPARTY party)
	{
		if (!party)
			return false;
		LPCHARACTER leader = party->GetLeaderCharacter();
		if (leader)
			return !leader->GetDesc() || !leader->GetDesc()->IsBot();
		// No character is not no leader. A player's warp is a logout and a
		// login, and for those seconds the party holds only the pid - long
		// enough for the party pass to take a player's party for a bot party
		// and put the cohort rule to it. The engine's own party lines have a
		// bot in sizowski's party at 13:42:40, his character logging in again
		// at 13:42:54 and the bot gone at 13:42:56 (14 September). Every bot
		// is in the registry, so a leader pid it does not know is a person.
		const DWORD leaderPid = party->GetLeaderPID();
		return leaderPid != 0 && !CPlayerBotManager::instance().IsRegisteredBotPID(leaderPid);
	}

	// On the leader's map and inside the distance the follow pass leaves a bot
	// at: where a bot keeping a player company stands on purpose.
	bool IsPlayerBotBesideHumanLeader(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		return leader && leader != ch && leader->GetMapIndex() == ch->GetMapIndex() &&
				DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY()) <=
						PLAYERBOT_PARTY_FOLLOW_DISTANCE;
	}

	// Answering a player's invitation.
	//
	// The engine sends HEADER_GC_PARTY_INVITE to the invitee's descriptor and
	// waits ten seconds for an Accept that a bot has nobody to send. So the
	// engine leaves the invitation in playerbot_party (playerbotify.py puts the
	// call into CHARACTER::PartyInvite) and this runs on the bot's own tick,
	// inside those ten seconds, calling the same method the client's Accept
	// would have reached. The bot never refuses: every condition that could
	// refuse is the engine's own (same kingdom, thirty levels, a free place in a
	// party of eight) and PartyInviteAccept reports those itself.
	void AcceptPlayerBotPartyInvite(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		uint32_t leaderPid = 0, notedAt = 0;
		if (!playerbot_party::TakeInvite(ch->GetPlayerID(), leaderPid, notedAt))
			return;
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(leaderPid);
		if (!leader || leader->IsDead())
			return;
		// Joining is a thing the bot is now doing: an errand it was walking to
		// keeps its own state, but the party check must not run in the same
		// second and weigh a party the bot has not joined yet.
		state.dwNextPartyCheckTime = dwNow + 5000;
		leader->PartyInviteAccept(ch);
		sys_log(0, "PLAYERBOT_PARTY: accepted an invitation pid=%u name=%s leader_pid=%u leader=%s",
				ch->GetPlayerID(), ch->GetName(), leaderPid, leader->GetName());
	}

	// The level a dropper stops at, or zero for everybody else.
	BYTE GetPlayerBotExpLockLevel(BYTE personality)
	{
		switch (personality)
		{
			case BOT_PERSONALITY_METIN_DROPPER: return PLAYERBOT_EXP_LOCK_METIN_DROPPER;
			case BOT_PERSONALITY_M3_DROPPER:    return PLAYERBOT_EXP_LOCK_M3_DROPPER;
			case BOT_PERSONALITY_M2_DROPPER:    return PLAYERBOT_EXP_LOCK_M2_DROPPER;
			case BOT_PERSONALITY_MEDAL_DROPPER: return PLAYERBOT_EXP_LOCK_MEDAL_DROPPER;
			default: return 0;
		}
	}

	// A farmer keeps the level its table pays at. See the constants: every drop
	// in this engine fades with the level gap, so a dropper that goes on
	// levelling farms its way out of its own living. The lock is the engine's
	// AFFECT_EXP_BLOCK, which PointChange checks before it adds any experience,
	// so nothing else has to know about it. It is meant for good, and it is
	// lifted only from a bot that should not carry it any more - one of the
	// operator's medal droppers once that cohort is switched off or given a
	// higher level (CPlayerBotManager::SpawnMedalDropperCohort), since nothing
	// else would ever take it off.
	void ManagePlayerBotExpLock(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		if (!ch)
			return;
		BYTE lockLevel = GetPlayerBotExpLockLevel(state.bPersonality);
		// The operator's medal droppers stop where the operator said.
		const bool cohort = CPlayerBotManager::instance().IsMedalDropperCohortPID(ch->GetPlayerID());
		// Under Iwakura's personalities everybody else holds where its Grinder
		// holds (GetPlayerBotPersonaLockLevel) - and nothing is decided before
		// the bot's quest flags have said where that is, or every spawn would
		// lift a lock and put it back a few seconds later.
		const bool persona = !cohort && IsPlayerBotPersonaEnabled();
		if (persona && !state.persona.bRestored)
			return;
		if (cohort)
			lockLevel = CPlayerBotManager::instance().GetMedalDropperCohortLevel();
		else if (persona && state.bPersonality == BOT_PERSONALITY_MEDAL_DROPPER)
			// The Tier 4 Grinder holds where its medals are worth farming
			// (community patch 2, point 4), not at a tier's lock.
			lockLevel = PLAYERBOT_EXP_LOCK_MEDAL_DROPPER;
		else if (persona)
			lockLevel = GetPlayerBotPersonaLockLevel(ch, state);
		const bool shouldLock = lockLevel != 0 && ch->GetLevel() >= lockLevel;
#if defined(PLAYERBOT_ENGINE_MT2009)
		const bool locked = ch->FindAffect(AFFECT_EXP_BLOCK) != NULL;
		if (locked == shouldLock)
			return;
		if (!shouldLock)
		{
			ch->RemoveAffect(AFFECT_EXP_BLOCK);
			sys_log(0, "PLAYERBOT_AI: exp lock lifted pid=%u name=%s level=%u lock=%u personality=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetLevel(),
					(unsigned)lockLevel, (unsigned)state.bPersonality);
			return;
		}
		ch->AddAffect(AFFECT_EXP_BLOCK, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, true, true);
		sys_log(0, "PLAYERBOT_AI: exp locked for a %s pid=%u name=%s level=%u lock=%u personality=%u",
				persona ? "grinder" : "dropper", ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetLevel(),
				(unsigned)lockLevel, (unsigned)state.bPersonality);
#else
		// r40250 has no AFFECT_EXP_BLOCK at all - PointChange there knows no
		// such affect, so there is nothing to ask it for and a dropper on that
		// line goes on levelling as it always did. Freezing it would need an
		// engine patch of its own, and this feature was asked for on the 2.x
		// world; the shared overlay simply does nothing here.
		(void)shouldLock;
#endif
	}

	// When a marble is worth more than the whole skill rotation.
	//
	// A polymorph marble gives a large flat damage bonus for five minutes and
	// the engine refuses every skill while it lasts (char_skill.cpp), so it is a
	// trade, not an upgrade: worth taking against something that stands there
	// long enough for the bonus to add up and cannot be killed faster by a
	// rotation anyway. That is a boss, at the start of the fight - which is
	// exactly where the players use them.
	//
	// Every refusal the engine can raise is left to the engine (already
	// transformed, in the saddle, a monster too high for the bot's level): none
	// of them spends the marble, and the retry clock keeps a refused one from
	// being tried every tick for the rest of the fight.
	void ManagePlayerBotPolymorph(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotPolymorphRetry;
		if (!ch || ch->IsDead() || ch->IsPolymorphed() || ch->IsRiding())
			return;
		LPCHARACTER victim = ch->GetVictim();
		if (!victim || victim->IsDead() || !victim->IsMonster() ||
				victim->GetMobRank() < MOB_RANK_BOSS)
			return;
		// Early in the fight, or the five minutes are spent on a boss that is
		// nearly down and the bot has thrown a marble away for one hit.
		if (victim->GetMaxHP() <= 0 ||
				(victim->GetHP() * 100) / victim->GetMaxHP() < PLAYERBOT_POLYMORPH_BOSS_HP_PERCENT)
			return;
		std::map<DWORD, DWORD>::const_iterator retry =
				s_mapPlayerBotPolymorphRetry.find(ch->GetPlayerID());
		if (retry != s_mapPlayerBotPolymorphRetry.end() && dwNow < retry->second)
			return;

		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_POLYMORPH || item->GetSocket(0) == 0)
				continue;
			bool known = false;
			for (size_t i = 0; i < sizeof(PLAYERBOT_POLYMORPH_MARBLE_VNUMS) /
					sizeof(PLAYERBOT_POLYMORPH_MARBLE_VNUMS[0]); ++i)
				if (PLAYERBOT_POLYMORPH_MARBLE_VNUMS[i] == item->GetVnum())
					known = true;
			if (!known)
				continue;
			s_mapPlayerBotPolymorphRetry[ch->GetPlayerID()] = dwNow + PLAYERBOT_POLYMORPH_RETRY_MS;
			if (ch->UseItem(TItemPos(INVENTORY, cell)))
			{
				sys_log(0, "PLAYERBOT_AI: polymorphed for a boss pid=%u name=%s marble=%u mob=%u boss=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
						item->GetSocket(0), (unsigned)victim->GetRaceNum());
			}
			return;
		}
	}

	// What a bot holds that is not for fighting, or NULL when it is ready for a
	// duel. A duel is agreed three seconds after the challenge and fought at the
	// top of the tick, above the fishing session, so a bot on the bank took one
	// with its rod out and fought it that way ("bot wzial pvp z innym botem
	// bedac wyposazonym w wedke", Tieru, 15 September). The same answer is asked
	// of a bot that would challenge, of the bot it picks, and of both sides of a
	// duel already under way.
	const char* GetPlayerBotDuelUnreadiness(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return "gone";
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const bool fishing = it != s_mapPlayerBotAIStates.end() && it->second.bFishingSession;
		LPITEM held = ch->GetWear(WEAR_WEAPON);
		if (fishing || (held && held->GetType() == ITEM_ROD))
			return "fishing";
		if (IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) || (held && held->GetType() == ITEM_PICK))
			return "mining";
		if (!held || held->GetType() != ITEM_WEAPON)
			return "no_weapon";
		return NULL;
	}

	// Agreeing to a duel.
	//
	// CPVPManager::Insert is a two-sided agreement, so answering a challenge is
	// the same call the challenger made. The engine recorded the challenge
	// (pvp.cpp, playerbotify.py) because a bot has no client to type /pvp back;
	// this waits the agreed three seconds and then agrees, which is what makes
	// the fight start.
	//
	// A bot refuses only a fight that cannot happen: under PK_PROTECT_LEVEL on
	// either side, or in a safe zone, the engine refuses every blow, so an
	// agreement there was a duel nobody could fight or end ("bot przyjmuje pvp
	// ponizej 15 lvl", "nieskonczone pvp", djariczek). What it will not do
	// either is agree from the floor: a challenge taken at a sliver of health
	// is a free kill, not a duel, and the engine has no rule against it.
	void AcceptPlayerBotPvpChallenge(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		uint32_t challengerPid = 0, seenAt = 0;
		if (!playerbot_pvp::PeekChallenge(ch->GetPlayerID(), challengerPid, seenAt, dwNow))
			return;
		if (ch->IsDead())
		{
			playerbot_pvp::Forget(ch->GetPlayerID());
			return;
		}
		if (dwNow < seenAt + PLAYERBOT_PVP_ACCEPT_DELAY)
			return;
		playerbot_pvp::Forget(ch->GetPlayerID());
		LPCHARACTER challenger = CHARACTER_MANAGER::instance().FindByPID(challengerPid);
		if (!challenger || challenger->IsDead() ||
				challenger->GetMapIndex() != ch->GetMapIndex())
			return;
		const char* refusal = NULL;
		if (ch->GetLevel() < PK_PROTECT_LEVEL || challenger->GetLevel() < PK_PROTECT_LEVEL)
			refusal = "level";
		else if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(challenger->GetMapIndex(), challenger->GetX(), challenger->GetY()))
			refusal = "safe_zone";
		// Nor one fought with a rod or a pickaxe (GetPlayerBotDuelUnreadiness).
		else
			refusal = GetPlayerBotDuelUnreadiness(ch, dwNow);
		if (refusal)
		{
			sys_log(0, "PLAYERBOT_PVP: declined a duel pid=%u name=%s challenger_pid=%u challenger=%s reason=%s level=%u challenger_level=%u",
					ch->GetPlayerID(), ch->GetName(), challengerPid, challenger->GetName(), refusal,
					(unsigned int)ch->GetLevel(), (unsigned int)challenger->GetLevel());
			// A person is told why; a bot has nobody to read it.
			if (challenger->GetDesc() && !challenger->GetDesc()->IsBot())
			{
				if (!strcmp(refusal, "level"))
					challenger->ChatPacket(CHAT_TYPE_INFO, "%s nie przyjmie pojedynku ponizej %d poziomu.",
							ch->GetName(), (int)PK_PROTECT_LEVEL);
				else if (!strcmp(refusal, "safe_zone"))
					challenger->ChatPacket(CHAT_TYPE_INFO, "%s nie walczy w strefie bezpiecznej.", ch->GetName());
				else if (!strcmp(refusal, "fishing"))
					challenger->ChatPacket(CHAT_TYPE_INFO, "%s lowi ryby i nie przyjmie teraz pojedynku.", ch->GetName());
				else if (!strcmp(refusal, "mining"))
					challenger->ChatPacket(CHAT_TYPE_INFO, "%s kopie rude i nie przyjmie teraz pojedynku.", ch->GetName());
				else
					challenger->ChatPacket(CHAT_TYPE_INFO, "%s nie ma broni w reku i nie przyjmie pojedynku.", ch->GetName());
			}
			return;
		}
		CPVPManager::instance().Insert(ch, challenger);
		playerbot_pvp::NoteDuelStarted(ch->GetPlayerID(), challengerPid,
				dwNow + PLAYERBOT_PVP_DUEL_ASSUMED);
		sys_log(0, "PLAYERBOT_PVP: agreed to a duel pid=%u name=%s challenger_pid=%u challenger=%s",
				ch->GetPlayerID(), ch->GetName(), challengerPid, challenger->GetName());
	}

	// An agreed duel, fought before anything else can claim the tick.
	//
	// Choosing the opponent in the target section is the natural place for "who
	// am I hitting", and that is where this was done first - but that section
	// sits below a dozen passes which each end the tick with continue, and a bot
	// that has just agreed to a duel is usually in the middle of one of them.
	// Measured six seconds after an agreement: one of the pair was walking to
	// the weapon merchant and the other looking for a monster, seventy units
	// apart, both at full health. A duel is a commitment to another character,
	// so it belongs where the stun gate belongs - at the top, above the errands.
	//
	// It logs once per opponent rather than per tick: a duel runs for minutes
	// and this pass fires every other second. Without a line of its own the
	// change could not be verified at all - the target log beside it is
	// sys_log level 1, and this core writes none of those.
	bool ManagePlayerBotDuelCombat(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotDuelLogged;
		// When the engine first refused this bot a blow at its foe.
		static std::map<DWORD, DWORD> s_mapPlayerBotDuelRefusedSince;
		if (!ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		if (playerbot_pvp::GetDuelOpponent(pid, dwNow) == 0)
		{
			s_mapPlayerBotDuelRefusedSince.erase(pid);
			s_mapPlayerBotDuelLogged.erase(pid);
			return false;
		}
		LPCHARACTER foe = FindPlayerBotDuelOpponent(ch, dwNow);
		if (!foe)
		{
			// Fallen, gone, or on another map: the fight the engine agreed to is
			// over either way, and a duel still remembered is only a potion ban
			// with nobody to fight. Nothing ended one before this - EndDuel had
			// no caller, so every duel ran its whole bound.
			LPCHARACTER fallen = CHARACTER_MANAGER::instance().FindByPID(
					(DWORD)playerbot_pvp::GetDuelOpponent(pid, dwNow));
			EndPlayerBotDuel(ch, state, dwNow, fallen && fallen->IsDead() ? "foe_fell" : "foe_gone");
			s_mapPlayerBotDuelRefusedSince.erase(pid);
			s_mapPlayerBotDuelLogged.erase(pid);
			return false;
		}
		// A duel under way when the rod came out is ended, not fought with it.
		if (const char* unready = GetPlayerBotDuelUnreadiness(ch, dwNow))
		{
			EndPlayerBotDuel(ch, state, dwNow, unready);
			s_mapPlayerBotDuelRefusedSince.erase(pid);
			s_mapPlayerBotDuelLogged.erase(pid);
			return false;
		}
		// What heals by itself is switched off for the fight: the potion ban
		// alone left an auto potion running inside the engine.
		SwitchOffPlayerBotAutoPotionsForDuel(ch, dwNow);
		// The duel ends where the engine says it cannot be fought, not where
		// PLAYERBOT_PVP_DUEL_ASSUMED runs out. A refusal is normal for the
		// seconds before the other side agrees; past PLAYERBOT_PVP_REFUSED_GIVE_UP
		// it is a fight already won (CPVP::Win takes the loser's agreement
		// back), one under PK_PROTECT_LEVEL, or one standing in a safe zone.
		// A duel is not fought from a transport saddle. CPVPManager::CanAttack
		// refuses every blow from a horse under grade two, and the tick's own
		// dismount waits for a target - which a refused duel never sets - so a
		// bot that agreed in the saddle stayed there, was refused, and gave the
		// duel up to PLAYERBOT_PVP_REFUSED_GIVE_UP without a blow. Before 2.0.41
		// the same blows landed from the saddle anyway ("bocik nawalal hitami z
		// konia ... a ma zwyklego konia", Drip).
		if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "duel");
		const bool bSafe = IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(foe->GetMapIndex(), foe->GetX(), foe->GetY());
		if (bSafe || !CanPlayerBotStrikeCharacter(ch, foe))
		{
			std::map<DWORD, DWORD>::iterator refused = s_mapPlayerBotDuelRefusedSince.find(pid);
			if (refused == s_mapPlayerBotDuelRefusedSince.end())
				s_mapPlayerBotDuelRefusedSince[pid] = dwNow;
			else if (dwNow - refused->second >= PLAYERBOT_PVP_REFUSED_GIVE_UP)
			{
				EndPlayerBotDuel(ch, state, dwNow, bSafe ? "safe_zone" : "engine_refuses");
				s_mapPlayerBotDuelRefusedSince.erase(refused);
				s_mapPlayerBotDuelLogged.erase(pid);
			}
			return false;
		}
		s_mapPlayerBotDuelRefusedSince.erase(pid);
		const int distance = DISTANCE_APPROX(ch->GetX() - foe->GetX(),
				ch->GetY() - foe->GetY());
		// Further than the bot can see is no longer the fight that was agreed.
		if (distance > PLAYERBOT_SEARCH_RANGE)
			return false;

		state.dwTargetVID = (DWORD)foe->GetVID();
		ch->SetVictim(foe);
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		ch->SetRotationToXY(foe->GetX(), foe->GetY());

		const DWORD foePid = foe->GetPlayerID();
		if (s_mapPlayerBotDuelLogged[ch->GetPlayerID()] != foePid)
		{
			s_mapPlayerBotDuelLogged[ch->GetPlayerID()] = foePid;
			sys_log(0, "PLAYERBOT_PVP: fighting the duel pid=%u name=%s foe_pid=%u foe=%s dist=%d hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), foePid, foe->GetName(),
					distance, ch->GetHP(), ch->GetMaxHP());
		}

		// The aura goes up before the first blow. The duel claims the tick above
		// the buff pass, so a warrior fought every duel bare and cut with
		// Trzystronne Ciecie under no visible aura (Tieru, 15 September).
		if (distance <= PLAYERBOT_DUEL_BUFF_RANGE && ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
			return true;

		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = (weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW);
		// A blade swings from where it reaches. The hunt's two hundred and
		// eighty is a monster's size, and two duellists that far apart were
		// seen waving swords at the air between them. A caster casts from
		// further off, and a warrior charges the gap between the two.
		const int combatRange = isBow ? 800 : PLAYERBOT_DUEL_MELEE_RANGE;
		const bool caster = ch->GetJob() == JOB_SHAMAN ||
				(ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2);
		if (distance > combatRange)
		{
			if (!isBow && caster && distance <= PLAYERBOT_DUEL_CASTER_RANGE &&
					dwNow >= state.dwNextSkillCastTime)
			{
				if (ch->IsStateMove())
					ch->Stop();
				if (CastPlayerBotDuelSkill(ch, foe, state, dwNow))
					return true;
			}
			if (!isBow && TryPlayerBotDuelGapCloser(ch, foe, state, dwNow, distance))
				return true;
			MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 4, false, false);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_FIGHTING);
		if (!CastPlayerBotDuelSkill(ch, foe, state, dwNow))
			ExecutePlayerBotBasicAttack(ch, foe, state, dwNow);
		return true;
	}

	// Spreading the visitors of a Monkey Dungeon over its rooms on the way in.
	//
	// The entrance chamber holds 6-7% of a dungeon's spawns - 16 of 234 on map
	// 108, 16 of 256 on 109, 16 of 231 on 25 - and nearly every visiting bot,
	// because a visit is spent there: a bot leaves the moment its medal drops,
	// a median of 41 s, and the wander pass that chooses a door only runs on a
	// tick with nothing to hit. Every monkey of a dungeon carries the medal's
	// kill group, so the odds do not depend on the room. What the pile cost was
	// monsters - roughly fifteen times fewer per bot than the dungeon holds -
	// and the "whole dungeon in one line" players reported.
	//
	// So a bot in its first room of a visit rolls, by pid and weighted by how
	// many spawns each room holds, whether to stay or which door to take, and
	// walks there before it hunts. It yields to anything actually hitting it -
	// the walk is no reason to be killed - and to its own retreat, and resumes
	// when that is over. One door only: a second would meet the engine's door
	// block and the AI's own dwell, which are what keep a bot from being bounced
	// back, and past the first room the ordinary rotation carries it on. A bot
	// the engine has just moved through a door is not sent to another: it would
	// only stand there.
	//
	// The walk has a budget, and the budget is the walking. The entrance room is
	// a long corridor of aggressive monkeys: the first version gave the walk
	// forty seconds of wall time, and of the first six walks two crossed - both
	// at thirty-eight seconds - while the four that gave up had been going the
	// right way and stopping for whatever caught them, one of them fighting for
	// all forty. The time a fight holds the walk up does not count against it,
	// and a wall-clock bound far above that ends an intent the fights never let
	// go of.
	struct TPlayerBotMonkeySpread
	{
		long lMap;
		int iDoor;
		BYTE bFromChamber;
		DWORD dwAssigned;
		// Held-up time already closed, and when the hold now running began.
		DWORD dwHeld;
		DWORD dwHeldSince;
	};

	bool ManagePlayerBotMonkeySpread(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, TPlayerBotMonkeySpread> s_mapSpread;
		if (!ch)
			return false;
		const DWORD pid = ch->GetPlayerID();
		const long mapIndex = ch->GetMapIndex();
		if (!IsPlayerBotMonkeyMap(mapIndex) || ch->IsDead())
		{
			s_mapSpread.erase(pid);
			return false;
		}
		if (state.bMonkeyChamber == 255 ||
				(int)state.bMonkeyChamber >= PLAYERBOT_MONKEY_CHAMBER_COUNT)
			return false;

		std::map<DWORD, TPlayerBotMonkeySpread>::iterator it = s_mapSpread.find(pid);
		if (it != s_mapSpread.end() && it->second.lMap != mapIndex)
		{
			s_mapSpread.erase(it);
			it = s_mapSpread.end();
		}

		if (it == s_mapSpread.end())
		{
			// Only in the first room of a visit: a bot that has crossed once has
			// its way already, chosen here or by the rotation.
			if (state.bMonkeyPrevChamber != 255)
				return false;
			const int chamber = (int)state.bMonkeyChamber;
			TPlayerBotMonkeySpread spread;
			spread.lMap = mapIndex;
			spread.iDoor = -1;
			spread.bFromChamber = state.bMonkeyChamber;
			spread.dwAssigned = dwNow;
			spread.dwHeld = 0;
			spread.dwHeldSince = 0;
			int exitChambers[8];
			int exitDoors[8];
			const int exits = playerbot_monkey::IsGotoCrossingBlocked(pid, dwNow)
					? 0 : GetPlayerBotMonkeyChamberExits(mapIndex, chamber, exitChambers, exitDoors, 8);
			const int stayWeight = (int)PLAYERBOT_MONKEY_CHAMBERS[chamber].bSpotCount;
			int total = stayWeight;
			for (int i = 0; i < exits; ++i)
				total += (int)PLAYERBOT_MONKEY_CHAMBERS[exitChambers[i]].bSpotCount;
			int roll = total > 0
					? (int)(PlayerBotNavHash(pid ^ 0x53505244U ^ (DWORD)mapIndex) % (DWORD)total)
					: 0;
			int toChamber = chamber;
			roll -= stayWeight;
			for (int i = 0; i < exits && roll >= 0; ++i)
			{
				const int weight = (int)PLAYERBOT_MONKEY_CHAMBERS[exitChambers[i]].bSpotCount;
				if (roll < weight)
				{
					spread.iDoor = exitDoors[i];
					toChamber = exitChambers[i];
				}
				roll -= weight;
			}
			long doorX = 0, doorY = 0;
			const int distance = spread.iDoor >= 0 &&
					GetPlayerBotMonkeyDoorPosition(mapIndex, spread.iDoor, doorX, doorY)
					? DISTANCE_APPROX(ch->GetX() - doorX, ch->GetY() - doorY) : -1;
			it = s_mapSpread.insert(std::make_pair(pid, spread)).first;
			sys_log(0, "PLAYERBOT_MONKEY: spread pid=%u name=%s map=%ld from=%d to=%d door=%d exits=%d dist=%d",
					pid, ch->GetName(), mapIndex, chamber, toChamber, spread.iDoor, exits, distance);
		}

		TPlayerBotMonkeySpread& spread = it->second;
		if (spread.iDoor < 0)
			return false;
		const DWORD elapsed = dwNow - spread.dwAssigned;
		const DWORD held = spread.dwHeld +
				(spread.dwHeldSince != 0 ? dwNow - spread.dwHeldSince : 0);
		const DWORD walked = elapsed > held ? elapsed - held : 0;
		if (state.bMonkeyChamber != spread.bFromChamber)
		{
			sys_log(0, "PLAYERBOT_MONKEY: spread crossed pid=%u name=%s map=%ld from=%d to=%d walked=%u elapsed=%u",
					pid, ch->GetName(), mapIndex, (int)spread.bFromChamber, (int)state.bMonkeyChamber,
					(unsigned int)walked, (unsigned int)elapsed);
			spread.iDoor = -1;
			return false;
		}
		if (walked >= PLAYERBOT_MONKEY_SPREAD_WALK_MS || elapsed >= PLAYERBOT_MONKEY_SPREAD_MAX_MS)
		{
			sys_log(0, "PLAYERBOT_MONKEY: spread gave up pid=%u name=%s map=%ld door=%d walked=%u elapsed=%u nav_out=%u",
					pid, ch->GetName(), mapIndex, spread.iDoor,
					(unsigned int)walked, (unsigned int)elapsed, (unsigned int)state.bLastNavOutcome);
			spread.iDoor = -1;
			return false;
		}
		// A fight or a retreat holds the walk up, and stops its clock.
		if (state.bTacticalRetreat || FindPlayerBotEngagedTarget(ch, &state, dwNow))
		{
			if (spread.dwHeldSince == 0)
				spread.dwHeldSince = dwNow;
			return false;
		}
		if (spread.dwHeldSince != 0)
		{
			spread.dwHeld += dwNow - spread.dwHeldSince;
			spread.dwHeldSince = 0;
		}

		long doorX = 0, doorY = 0;
		if (!GetPlayerBotMonkeyDoorPosition(mapIndex, spread.iDoor, doorX, doorY))
		{
			spread.iDoor = -1;
			return false;
		}
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		MovePlayerBot(ch, doorX, doorY, dwNow, 32, true, true);
		return true;
	}

	// Bots challenging one another.
	//
	// Rare on purpose: a duel is something that happens in a world, not the
	// thing the world does. One roll a minute per bot, six in a thousand, and
	// only between two bots standing close, near enough in level for the fight
	// to be a fight, both healthy and neither already in one. The challenged
	// bot answers through the journal above exactly as it would answer a
	// player, so there is one code path for both.
	void ManagePlayerBotPvpChallenge(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotPvpRollNext;
		if (!ch || ch->IsDead() || ch->GetSectree() == NULL)
			return;
		if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return;
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return;
		// Under PK_PROTECT_LEVEL the engine refuses every blow on the kingdom's own
		// maps: a challenge from there is a duel nobody can fight or end.
		if (ch->GetLevel() < PK_PROTECT_LEVEL)
			return;
		// Anything the bot is actually doing outranks picking a fight, and so does
		// a rod or a pickaxe still in its hands.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bMarketTrip || state.bFishingSession || state.bTacticalRetreat ||
				state.bRecoveringAfterDeath || ch->GetMyShop() ||
				GetPlayerBotDuelUnreadiness(ch, dwNow) != NULL)
			return;
		if (ch->GetMaxHP() <= 0 ||
				(ch->GetHP() * 100) / ch->GetMaxHP() < PLAYERBOT_PVP_MIN_HP_PERCENT)
			return;
		std::map<DWORD, DWORD>::const_iterator nextRoll =
				s_mapPlayerBotPvpRollNext.find(ch->GetPlayerID());
		if (nextRoll != s_mapPlayerBotPvpRollNext.end() && dwNow < nextRoll->second)
			return;
		s_mapPlayerBotPvpRollNext[ch->GetPlayerID()] =
				dwNow + PLAYERBOT_PVP_CHALLENGE_INTERVAL + number(0, 15000);
		if (number(1, 1000) > PLAYERBOT_PVP_CHALLENGE_PER_MILLE)
			return;

		struct FFindDuelPartner
		{
			FFindDuelPartner(LPCHARACTER me, DWORD now) :
				m_me(me), m_now(now), m_pFound(NULL) {}
			bool operator()(LPENTITY ent)
			{
				if (m_pFound || !ent || !ent->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(ent);
				if (candidate == m_me || !candidate->IsPC() || candidate->IsDead())
					return false;
				// Bots pick on each other, never on a person: a player who wants
				// a duel with a bot asks for one, and gets it.
				if (!candidate->GetDesc() || !candidate->GetDesc()->IsBot())
					return false;
				if (candidate->GetParty() && candidate->GetParty() == m_me->GetParty())
					return false;
				if (playerbot_pvp::IsInDuel(candidate->GetPlayerID(), m_now))
					return false;
				// Not a bot on the bank with its rod out, nor one at a vein.
				if (GetPlayerBotDuelUnreadiness(candidate, m_now) != NULL)
					return false;
				if (abs((int)candidate->GetLevel() - (int)m_me->GetLevel()) >
						PLAYERBOT_PVP_CHALLENGE_LEVEL_DELTA)
					return false;
				if (candidate->GetLevel() < PK_PROTECT_LEVEL ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;
				if (candidate->GetMaxHP() <= 0 ||
						(candidate->GetHP() * 100) / candidate->GetMaxHP() <
							PLAYERBOT_PVP_MIN_HP_PERCENT)
					return false;
				if (DISTANCE_APPROX(m_me->GetX() - candidate->GetX(),
						m_me->GetY() - candidate->GetY()) > PLAYERBOT_PVP_CHALLENGE_RANGE)
					return false;
				m_pFound = candidate;
				return false;
			}
			LPCHARACTER m_me;
			DWORD m_now;
			LPCHARACTER m_pFound;
		};

		FFindDuelPartner finder(ch, dwNow);
		ch->GetSectree()->ForEachAround(finder);
		if (!finder.m_pFound)
			return;
		// The challenge itself. The other bot's tick agrees three seconds later
		// through the same journal a player's challenge goes through.
		CPVPManager::instance().Insert(ch, finder.m_pFound);
		playerbot_pvp::NoteDuelStarted(ch->GetPlayerID(),
				finder.m_pFound->GetPlayerID(), dwNow + PLAYERBOT_PVP_DUEL_ASSUMED);
		sys_log(0, "PLAYERBOT_PVP: challenged another bot pid=%u name=%s target_pid=%u target=%s",
				ch->GetPlayerID(), ch->GetName(), finder.m_pFound->GetPlayerID(),
				finder.m_pFound->GetName());
	}

	// Two kingdoms meeting on shared ground.
	//
	// Off unless the operator says otherwise - KINGDOMPVP in the weights file
	// is zero by default. This changes how the world behaves towards itself
	// rather than how one bot spends its time, and a world that starts fighting
	// itself because a build shipped is not a world anybody asked for.
	//
	// Built on the duel the bots already fight rather than on the target
	// collector, deliberately. Admitting player characters to that collector
	// means threading them through the combat value policy, the held-target
	// rule, the multi-pull and the party focus - every one of which was written
	// about monsters - for a feature that ships switched off. A duel is bounded
	// by construction: it ends when somebody falls, the health-potion pass
	// already refuses to drink through one, and neither bot can be walked
	// across the map by it. That is "no loops" and "the one that loses gives up
	// and goes back to work" without a leash of its own.
	void ManagePlayerBotKingdomHostility(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotKingdomNext;
		// One comparison for the whole population while the switch is off.
		if (s_iPlayerBotKingdomPvpPercent <= 0)
			return;
		if (!ch || ch->IsDead() || ch->GetSectree() == NULL)
			return;
		// A kingdom's own maps are where its bots shop and train; the frontier
		// is the ground the three share, and the only place this belongs.
		if (!IsPlayerBotFrontierMapIndex(ch->GetMapIndex()))
			return;
		if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return;
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return;
		// The protection under PK_PROTECT_LEVEL holds across kingdoms too
		// (CPVPManager::CanAttack), so the same duel would never land a blow.
		if (ch->GetLevel() < PK_PROTECT_LEVEL)
			return;
		// Who is aggressive is decided by pid, not rolled: a kingdom then has a
		// character rather than a mood, the same bots pick the fights after
		// every restart, and the rest are left alone to hunt - which is what
		// "some aggressive, some neutral" has to mean to be visible at all.
		if ((int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4B494E47U) % 100U) >=
				s_iPlayerBotKingdomPvpPercent)
			return;
		// Anything the bot is actually doing outranks picking a fight.
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bMarketTrip || state.bFishingSession || state.bTacticalRetreat ||
				state.bRecoveringAfterDeath || ch->GetMyShop() ||
				IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) ||
				GetPlayerBotDuelUnreadiness(ch, dwNow) != NULL)
			return;
		if (ch->GetMaxHP() <= 0 ||
				(ch->GetHP() * 100) / ch->GetMaxHP() < PLAYERBOT_PVP_MIN_HP_PERCENT)
			return;
		std::map<DWORD, DWORD>::const_iterator nextRoll =
				s_mapPlayerBotKingdomNext.find(ch->GetPlayerID());
		if (nextRoll != s_mapPlayerBotKingdomNext.end() && dwNow < nextRoll->second)
			return;
		s_mapPlayerBotKingdomNext[ch->GetPlayerID()] =
				dwNow + PLAYERBOT_KINGDOM_PVP_INTERVAL + number(0, 20000);

		struct FFindEnemyKingdomBot
		{
			FFindEnemyKingdomBot(LPCHARACTER me, DWORD now) :
				m_me(me), m_now(now), m_pFound(NULL) {}
			bool operator()(LPENTITY ent)
			{
				if (m_pFound || !ent || !ent->IsType(ENTITY_CHARACTER))
					return false;
				LPCHARACTER candidate = static_cast<LPCHARACTER>(ent);
				if (candidate == m_me || !candidate->IsPC() || candidate->IsDead())
					return false;
				// Never a person. A player who wants to fight a bot challenges
				// one and is answered; this is the world's own quarrel.
				if (!candidate->GetDesc() || !candidate->GetDesc()->IsBot())
					return false;
				if (candidate->GetEmpire() == m_me->GetEmpire())
					return false;
				if (candidate->GetParty() && candidate->GetParty() == m_me->GetParty())
					return false;
				if (playerbot_pvp::IsInDuel(candidate->GetPlayerID(), m_now))
					return false;
				// The same for a quarrel: nobody is set upon with a rod in hand.
				if (GetPlayerBotDuelUnreadiness(candidate, m_now) != NULL)
					return false;
				if (abs((int)candidate->GetLevel() - (int)m_me->GetLevel()) >
						PLAYERBOT_KINGDOM_PVP_LEVEL_DELTA)
					return false;
				if (candidate->GetLevel() < PK_PROTECT_LEVEL ||
						IsPlayerBotSafeZone(candidate->GetMapIndex(), candidate->GetX(), candidate->GetY()))
					return false;
				// A bot on its knees is not a fight. This is also what keeps the
				// loser out of a second quarrel while it walks away from the
				// first one: it is under the health floor until it has rested.
				if (candidate->GetMaxHP() <= 0 ||
						(candidate->GetHP() * 100) / candidate->GetMaxHP() <
							PLAYERBOT_PVP_MIN_HP_PERCENT)
					return false;
				if (DISTANCE_APPROX(m_me->GetX() - candidate->GetX(),
						m_me->GetY() - candidate->GetY()) > PLAYERBOT_KINGDOM_PVP_RANGE)
					return false;
				m_pFound = candidate;
				return false;
			}
			LPCHARACTER m_me;
			DWORD m_now;
			LPCHARACTER m_pFound;
		};

		FFindEnemyKingdomBot finder(ch, dwNow);
		ch->GetSectree()->ForEachAround(finder);
		if (!finder.m_pFound)
			return;
		CPVPManager::instance().Insert(ch, finder.m_pFound);
		playerbot_pvp::NoteDuelStarted(ch->GetPlayerID(),
				finder.m_pFound->GetPlayerID(), dwNow + PLAYERBOT_PVP_DUEL_ASSUMED);
		sys_log(0, "PLAYERBOT_PVP: kingdom quarrel pid=%u name=%s empire=%d target_pid=%u target=%s target_empire=%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), (int)ch->GetEmpire(),
				finder.m_pFound->GetPlayerID(), finder.m_pFound->GetName(),
				(int)finder.m_pFound->GetEmpire(), ch->GetMapIndex());
	}

	// Walking with the player who invited you.
	//
	// Claims the tick when it moves, because the alternative is the wander pass
	// sending the bot to its own hunting hub twenty kilometres away while the
	// player it just joined watches it leave. Combat is not interrupted - the
	// target sections run later and a bot with a victim is kept where it is -
	// and an errand the bot had already begun keeps its own state; this only
	// covers the ordinary case of standing about far from the leader.
	bool ManagePlayerBotFollowHumanLeader(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// The clock lives beside the pass rather than in TPlayerBotAIState: a
		// new field in that struct has to be initialised in declaration order or
		// -Wreorder fires, and this one is nobody else's business.
		static std::map<DWORD, DWORD> s_mapPlayerBotFollowNext;
		if (!ch || ch->IsDead())
			return false;
		std::map<DWORD, DWORD>::const_iterator nextFollow =
				s_mapPlayerBotFollowNext.find(ch->GetPlayerID());
		if (nextFollow != s_mapPlayerBotFollowNext.end() && dwNow < nextFollow->second)
			return false;
		LPPARTY party = ch->GetParty();
		if (!party || !IsPlayerBotHumanLedParty(party))
			return false;
		LPCHARACTER leader = party->GetLeaderCharacter();
		if (!leader || leader->IsDead() || leader == ch)
			return false;
		// A leader on another map is followed there. A warp takes a player by
		// telling the client to reconnect, and a bot has no client, so the move
		// is the one the AI makes for every map change - TransitionPlayerBotMap,
		// onto the leader's own spot - once the leader stands on the new map (a
		// character still warping is on the old one). What stays out of reach:
		// a map this core does not host, a dungeon instance (no navigation grid
		// and no way out a bot knows, and the Demon Tower is one), and a spider
		// map whose desert crossing is already under way, which the transition
		// would otherwise restart from the desert's doorstep on every retry.
		if (leader->GetMapIndex() != ch->GetMapIndex())
		{
			const long leaderMap = leader->GetMapIndex();
			if (leaderMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || leader->IsWarping() ||
					!leader->GetSectree() || !IsPlayerBotMapHostedHere(leaderMap) ||
					state.lDesertCrossingTo == leaderMap)
				return false;
			s_mapPlayerBotFollowNext[ch->GetPlayerID()] = dwNow + PLAYERBOT_PARTY_WARP_FOLLOW_RETRY;
			if (!TransitionPlayerBotMap(ch, state, leaderMap, leader->GetX(), leader->GetY(),
					dwNow, "follow_leader"))
				return false;
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			return true;
		}
		// Fighting something is not standing about.
		if (ch->GetVictim() && !ch->GetVictim()->IsDead())
			return false;
		// Neither is walking out to fetch a pack. A luring course takes the
		// Archer up to PLAYERBOT_LURE_MAX_COURSE_RANGE from the party on
		// purpose, and this pass runs far above it in the tick - so without
		// this it walks the Archer home the moment the course passes the follow
		// distance, and the course walks it out again on the next tick. Same
		// loop as the errands Pabloo's fix stopped in 2.0.49, from the other
		// side. The leader changing map is handled above and ends the course
		// anyway, so only the walk on this map stands down.
		if (state.bLureStage != LURE_STAGE_NONE)
			return false;
		const int dist = DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY());
		if (dist <= PLAYERBOT_PARTY_FOLLOW_DISTANCE)
			return false;
		s_mapPlayerBotFollowNext[ch->GetPlayerID()] = dwNow + PLAYERBOT_PARTY_FOLLOW_INTERVAL;
		// The horse is allowed: a player crossing a map on one leaves a walking
		// bot behind within seconds.
		if (!MovePlayerBot(ch, leader->GetX(), leader->GetY(), dwNow, 8, true, true, false, false))
			return false;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		return true;
	}

	// A bot in a person's party hunts round the person, not round itself. The
	// target search reaches PLAYERBOT_SEARCH_RANGE from the bot, and the follow
	// pass above only walks a bot that is not fighting - so a bot that always
	// found its next monster within six kilometres of itself was never idle,
	// drifted off pack by pack and never came back: "Boty szaman, po dodaniu
	// ich do party, nadal robia swoje, nie ida za wlascicielem party"
	// (SIZOWSKI, 20 September), and a Shaman that far off buffs nobody. A
	// monster further than PLAYERBOT_PARTY_HUMAN_HUNT_RANGE from the person is
	// no target for such a bot, unless it is hitting this bot - defence is
	// never refused - and with nothing to fight the follow pass walks it back.
	bool IsPlayerBotTargetOffHumanLeader(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !ch->GetParty() || !IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		if (target->GetVictim() == ch)
			return false;
		LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
		if (!leader || leader == ch || leader->IsDead() || leader->GetMapIndex() != ch->GetMapIndex())
			return false;
		return DISTANCE_APPROX(target->GetX() - leader->GetX(), target->GetY() - leader->GetY()) >
				PLAYERBOT_PARTY_HUMAN_HUNT_RANGE;
	}

	// A Shaman in a player's party keeps the player's buffs up, as a Shaman in
	// a party of people would. CHARACTER::UseSkill hands a buff that is not
	// SELFONLY to ComputeSkill on the character it is aimed at, and the affect
	// it adds carries the skill's own vnum, so this is the self-buff pass
	// pointed at somebody else: the build's own buff list, what the player has
	// not already got, one cast per tick, and before the bot's own buffs -
	// whose copy of the skill then waits out the cooldown, which is the price a
	// player's Shaman pays as well. Cure is a heal and goes to a player under
	// PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT. A buff reaches 800 to 1000 and
	// the follow pass leaves a bot anywhere inside
	// PLAYERBOT_PARTY_FOLLOW_DISTANCE, so a bot out of reach and not fighting
	// walks up first, and one on a transport horse climbs down first, because
	// the engine refuses every other skill from that saddle. "Nigdy zaden
	// szaman nie uzyl swoich buffow na mnie gdy bylismy w PT" (sizowski,
	// 14 September).
	bool ManagePlayerBotBuffHumanLeader(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotLeaderBuffNext;
		if (!ch || ch->IsDead() || ch->GetJob() != JOB_SHAMAN || ch->GetSkillGroup() == 0)
			return false;
		LPPARTY party = ch->GetParty();
		if (!party || !IsPlayerBotHumanLedParty(party))
			return false;
		DWORD& next = s_mapPlayerBotLeaderBuffNext[ch->GetPlayerID()];
		if (dwNow < next)
			return false;
		next = dwNow + PLAYERBOT_PARTY_LEADER_BUFF_INTERVAL;
		// A visit, a Biologist walk or a stable errand the bot carried into the
		// party is only paused there (the tick skips all three for a player's
		// party), so its flags stay set and must not stop the buffs.
		if (state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession || ch->GetMyShop())
			return false;
		LPCHARACTER leader = party->GetLeaderCharacter();
		if (!leader || leader == ch || leader->IsDead() || leader->GetMapIndex() != ch->GetMapIndex())
			return false;
		const bool fighting = ch->GetVictim() && !ch->GetVictim()->IsDead();
		const bool hunting = fighting || state.dwTargetVID != 0 ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < PLAYERBOT_BUFF_COMBAT_WINDOW);
		const int dist = DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY());
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD vnum = build.dwBuffSkills[i];
			if (vnum == 0 || ch->GetSkillLevel(vnum) == 0)
				continue;
			if (!hunting && !IsPlayerBotOutOfCombatBuff(vnum))
				continue;
			if (vnum == 109) // Cure / Heal
			{
				if (leader->GetMaxHP() <= 0 ||
						(long long)leader->GetHP() * 100 / leader->GetMaxHP() > PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT)
					continue;
			}
			else if (IsPlayerBotBuffAffectOn(leader, vnum))
				continue;
			CSkillProto* proto = CSkillManager::instance().Get(vnum);
			if (!proto || IS_SET(proto->dwFlag, SKILL_FLAG_SELFONLY))
				continue;
			if (proto->dwTargetRange != 0 && dist > (int)proto->dwTargetRange)
			{
				if (fighting)
					continue;
				if (!MovePlayerBot(ch, leader->GetX(), leader->GetY(), dwNow, 8, true, false, false, false))
					continue;
				next = dwNow + PLAYERBOT_PARTY_FOLLOW_INTERVAL;
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				return true;
			}
			if (ch->IsRiding() && !CanPlayerBotEverFightOnHorse(ch))
			{
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "leader_buff");
				next = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
				return true;
			}
			if (!ch->UseSkill(vnum, leader))
				continue;
			SendPlayerBotSkillPacket(ch, vnum);
			state.dwLastBotSkillTime = dwNow;
			state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
			next = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
			sys_log(0, "PLAYERBOT_AI: buffed party leader pid=%u name=%s leader=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), leader->GetName(), vnum);
			return true;
		}
		return false;
	}

	void ManagePlayerBotParty(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->GetSectree() || dwNow < state.dwNextPartyCheckTime)
			return;

		state.dwNextPartyCheckTime = dwNow + PLAYERBOT_PARTY_CHECK_INTERVAL + number(0, 3000);

		LPPARTY pParty = ch->GetParty();
		// Iwakura's companion (playerbot_companions.h): the phase's draw, the
		// minutes alone after a party, and the answer to an invitation the bot
		// made to a person - read whatever party the bot is in now, a person's
		// included, or a yes that put it in one would never be counted.
		const bool bPersona = UpdatePlayerBotCompanionPhase(ch, state, dwNow);
		// A party a player leads is the player's, and none of the rules below
		// are about it. The cohort draw, the five-to-fifteen-minute rotation and
		// the straggler radius all exist to stop bot parties ossifying around
		// one camp; applied to a person's party they would walk the bot back
		// out within a minute of it being invited, which is the opposite of
		// what an invitation means. The player decides when it ends.
		// Its distribution mode included: a bot in a person's party put the
		// party back on parity at every check, whatever the leader had chosen
		// ("boty dodane do PT zawsze same zmieniaja podzial na rowny nawet gdy
		// to nie one sa liderem", Dearminder, 15 September).
		if (pParty && IsPlayerBotHumanLedParty(pParty))
			return;
		// A mercenary's contract keeps its own party, by its own rules.
		if (IsPlayerBotOnMercContract(ch->GetPlayerID()))
			return;
		// Iwakura's companion leaves at eighty percent of its bag and goes to
		// empty it ("opuszcza grupe i naturalnie przechodzi w osobowosc
		// Handlarza"). Asked before the cohort, which a full bag also leaves,
		// so the line says why.
		if (bPersona && pParty && state.persona.bBagFull)
		{
			LeavePlayerBotParty(ch);
			state.dwPartyExpireTime = 0;
			sys_log(0, "PLAYERBOT_AI: left party, bag full pid=%u name=%s",
					ch->GetPlayerID(), ch->GetName());
			return;
		}
		// Party play is an explicit, deterministic cohort. Archer weighting is
		// decided at login, while the total cohort remains close to ten percent.
		if (!IsPlayerBotPartyEligible(ch, state))
		{
			if (pParty)
			{
				LeavePlayerBotParty(ch);
				sys_log(0, "PLAYERBOT_AI: left party outside party cohort pid=%u name=%s",
						ch->GetPlayerID(), ch->GetName());
			}
			state.dwPartyExpireTime = 0;
			state.dwNextPartyCheckTime = dwNow + number(60000, 180000);
			return;
		}

		// A party of one is not a party, and every rule below reads it as one.
		// The world arrives here full of them: the straggler radius takes the
		// member out of a pair and the leader is left holding the record, which
		// nothing in this pass ever looks at again (the leader skips the
		// distance branch, `leader != ch` being false). Dissolve it and go on
		// to the finder in the same check, so a bot stranded this way is back
		// in the pool within a minute rather than for the rest of its life.
		if (pParty && pParty->GetMemberCount() < 2)
		{
			sys_log(0, "PLAYERBOT_AI: dissolved a party of one pid=%u name=%s",
					ch->GetPlayerID(), ch->GetName());
			CPartyManager::instance().DeleteParty(pParty);
			pParty = NULL;
		}

		if (pParty)
		{
			// Iwakura's companion has no timer on its party: the bag (above),
			// the levels drifting apart and the others walking off end it (below),
			// as the document says.
			if (bPersona)
				state.dwPartyExpireTime = 0;
			// Check if party duration expired (dynamic rotation: 5-15 mins)
			if (state.dwPartyExpireTime != 0 && dwNow >= state.dwPartyExpireTime)
			{
				state.dwPartyExpireTime = 0;
				state.dwNextPartyCheckTime = dwNow + number(60000, 180000); // 1-3 min solo before new party
				LeavePlayerBotParty(ch);
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
					LeavePlayerBotParty(ch);
					state.dwNextPartyCheckTime = dwNow + number(30000, 90000);
					sys_log(0, "PLAYERBOT_AI: left party due to distance/level delta pid=%u name=%s leader_pid=%u dist=%d delta=%d",
							ch->GetPlayerID(), ch->GetName(), leader->GetPlayerID(), distToLeader, levelDelta);
					return;
				}
			}

			// Always enforce equal exp distribution
			if (pParty->GetExpDistributionMode() != PARTY_EXP_DISTRIBUTION_PARITY)
				pParty->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			// A companion leading bots with room asks a person nearby too.
			if (bPersona && pParty->GetLeaderPID() == ch->GetPlayerID())
				AskPlayerBotHumanCompanion(ch, state, dwNow);
			return;
		}

		// A stretch of hunting alone, less often where a party is the point.
		// Iwakura's companion is a phase with its own stretches alone, drawn
		// above, and does not also roll this one.
		const int soloPercent = IsPlayerBotFrontierMapIndex(ch->GetMapIndex())
				? PLAYERBOT_PARTY_SOLO_PERCENT_FRONTIER : PLAYERBOT_PARTY_SOLO_PERCENT;
		if (!bPersona && number(1, 100) <= soloPercent)
		{
			state.dwNextPartyCheckTime = dwNow + number(60000, 180000);
			return;
		}

		// A companion asks a person nearby before it looks for bots
		// ("graczy badz innych botow"); the rationing is the person's, not the
		// bot's, so most checks find nobody to ask and go on to the bots.
		if (bPersona && AskPlayerBotHumanCompanion(ch, state, dwNow))
			return;

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

					// The engine's first rule of a party, and the one a bot's own
					// Join never asked: CHARACTER::IsPartyJoinableCondition refuses
					// another kingdom before anything else, so a player cannot group
					// across kingdoms and a bot must not either. On the shared maps
					// the bots of all three stand side by side, and they grouped
					// there as if they were one ("boty z roznych krolestw expia w
					// jednym PT", l0st3k).
					if (candidate->GetEmpire() != m_me->GetEmpire())
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
						if (leader && leader->GetMapIndex() == m_me->GetMapIndex() &&
								leader->GetEmpire() == m_me->GetEmpire())
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
			state.dwPartyExpireTime = bPersona ? 0 : dwNow + number(300000, 900000); // 5 to 15 mins, none for a companion
			LPCHARACTER joinedLeader = finder.m_pTargetParty->GetLeaderCharacter();
			sys_log(0, "PLAYERBOT_AI: joined party pid=%u name=%s members=%d empire=%u leader_empire=%u",
					ch->GetPlayerID(), ch->GetName(), finder.m_pTargetParty->GetMemberCount(),
					(unsigned int)ch->GetEmpire(), joinedLeader ? (unsigned int)joinedLeader->GetEmpire() : 0U);
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
				state.dwPartyExpireTime = bPersona ? 0 : dwNow + number(300000, 900000); // 5 to 15 mins, none for a companion
				RememberPlayerBotEncounter(ch, finder.m_pSoloCandidate,
						PLAYERBOT_FRIEND_PARTY_POINTS, dwNow);
				sys_log(0, "PLAYERBOT_AI: created party pid=%u name=%s partner_pid=%u affinity=%d empire=%u partner_empire=%u",
						ch->GetPlayerID(), ch->GetName(),
						finder.m_pSoloCandidate->GetPlayerID(),
						GetPlayerBotAffinity(state, finder.m_pSoloCandidate->GetPlayerID()),
						(unsigned int)ch->GetEmpire(), (unsigned int)finder.m_pSoloCandidate->GetEmpire());
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
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_METIN)
				continue;

			const DWORD kdVnum = item->GetVnum();
			const int kdPlus = GetPlayerBotSoulStoneGrade(kdVnum);
			const int stoneKind = GetPlayerBotSoulStoneKind(kdVnum);
			// Iwakura's list decides which stones go in at all and which
			// first: his tier, then the grade, then the better piece.
			const int tier = GetPlayerBotSoulStoneSeatTier(kdVnum);
			if (tier <= 0)
				continue;
			LPITEM targetGear = NULL;
			int openSocket = -1;
			if (!FindPlayerBotSoulStoneSocket(ch, stoneKind, (DWORD)item->GetValue(5), &targetGear, &openSocket))
				continue;
			if (!ShouldPlayerBotSeatSoulStone(targetGear, kdPlus))
				continue;
			const int score = tier * 1000 + kdPlus * 100 + targetGear->GetRefineLevel() * 10;
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

		PlayerBotEquipItem(ch, bestGear);
		SetPlayerBotAction(state, BOT_ACTION_SOCKET_STONE, dwNow);
		sys_log(0, "PLAYERBOT_AI: soul stone %s pid=%u name=%s kd_vnum=%u gear_vnum=%u socket=%d now=%u score=%d",
				outcome, ch->GetPlayerID(), ch->GetName(), kdVnum, gearVnum,
				bestSocket, after, bestScore);
	}

	// Sztuka Combo and the Leadership books, read the way the engine's own use
	// path reads them (char_item.cpp, 50301-50306): Combo from level 30 and
	// again from 50, Leadership twenty levels a book. The day the engine puts
	// between two reads of one skill is waved away like the class books' while
	// the BOOKS switch is on; a read costs 20 000 experience, which a bot of
	// thirty has. Combo widens what a swing hits (GetShootMaxTargetCount is
	// 3 + the Combo level) and Leadership is what a party's bonuses ask of
	// their leader. Asked from ManagePlayerBotSkillBooks when no class book
	// is due, on its clock (sizowski, 16 September: "dodanie korzystania z
	// combo i dowodzenia botom").
	bool ReadPlayerBotGeneralSkillBook(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsPolymorphed())
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotGeneralSkillBook(item->GetVnum()) ||
					!CanPlayerBotReadGeneralSkillBookNow(ch, item->GetVnum()))
				continue;
			const DWORD skill = GetPlayerBotGeneralSkillBookSkill(item->GetVnum());
			if (IsPlayerBotFastBooksEnabled() && get_global_time() < ch->GetSkillNextReadTime(skill))
#if defined(PLAYERBOT_ENGINE_MT2009)
				ch->SetSkillNextReadTime(skill, get_global_time(), true);
#else
				ch->SetSkillNextReadTime(skill, get_global_time());
#endif
			if (get_global_time() < ch->GetSkillNextReadTime(skill) &&
					!ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
				return false;
			const BYTE oldLevel = ch->GetSkillLevel(skill);
			const DWORD vnum = item->GetVnum();
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
				return false;
			SetPlayerBotAction(state, BOT_ACTION_READ_BOOK, dwNow);
			sys_log(0, "PLAYERBOT_AI: read general book pid=%u name=%s vnum=%u skill=%u old_level=%u new_level=%u success=%d",
					ch->GetPlayerID(), ch->GetName(), vnum, skill, oldLevel, ch->GetSkillLevel(skill),
					ch->GetSkillLevel(skill) > oldLevel ? 1 : 0);
			return true;
		}
		return false;
	}

	// Whether LearnSkillByBook will read at all: under the level cap it wants
	// PLAYERBOT_BOOK_READ_EXP in hand (FN_should_check_exp - mt2009 waves the
	// cap through, r40250's english locale asks at every level).
	bool PlayerBotHasBookReadExp(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (ch->GetLevel() >= gPlayerMaxLevel)
			return true;
#endif
		return (long long)ch->GetExp() >= PLAYERBOT_BOOK_READ_EXP;
	}

	void ManagePlayerBotSkillBooks(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->GetSkillGroup() == 0 ||
				dwNow < state.dwNextSkillBookTime)
			return;
		state.dwNextSkillBookTime = dwNow + PLAYERBOT_SKILL_BOOK_CHECK_INTERVAL;
		// Short of the experience a read wants, the engine keeps the book and
		// the use still says yes: nothing to try until the bot has hunted.
		if (!PlayerBotHasBookReadExp(ch))
		{
			PlayerBotLogThrottled("book_exp", dwNow,
					"PLAYERBOT_AI: book read waits for experience pid=%u name=%s level=%d exp=%lld need=%d",
					ch->GetPlayerID(), ch->GetName(), (int)ch->GetLevel(),
					(long long)ch->GetExp(), PLAYERBOT_BOOK_READ_EXP);
			return;
		}

		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		int bestCell = -1;
		DWORD bestSkillVnum = 0;
		int bestPriority = INT_MIN;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
				const bool ready = IsPlayerBotFastBooksEnabled() ||
					get_global_time() >= ch->GetSkillNextReadTime(skillVnum) ||
					ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY);
				const bool canUnlock = ch->CountSpecifyItem(71001) || ch->CountSpecifyItem(71094);
				if (!ready && !canUnlock) continue;
				const int priority = (ready ? 100000 : 0) +
					(skillVnum == build.dwPrimaryMaxSkill ? 10000 : 0) + skillLevel;
				if (priority > bestPriority)
				{
					bestPriority = priority;
					bestCell = cell;
					bestSkillVnum = skillVnum;
				}
			}
		}

		if (bestCell < 0 || bestSkillVnum == 0)
		{
			ReadPlayerBotGeneralSkillBook(ch, state, dwNow);
			return;
		}

		if (!IsPlayerBotFastBooksEnabled() && !ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY) &&
				get_global_time() < ch->GetSkillNextReadTime(bestSkillVnum))
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
#if defined(PLAYERBOT_ENGINE_MT2009)
			ch->SetSkillNextReadTime(bestSkillVnum, get_global_time(), true);
#else
			ch->SetSkillNextReadTime(bestSkillVnum, get_global_time());
#endif
		// Still waiting, and no scroll to wave the wait away: asking the engine
		// anyway cost a refusal every eight seconds and a "read" line that read
		// nothing - a hundred and ninety of them in eight minutes.
		if (get_global_time() < ch->GetSkillNextReadTime(bestSkillVnum) &&
				!ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
			return;

		// Read from the saddle: neither LearnSkillByBook nor the book's use asks
		// about a horse, on either engine. The pass used to climb down, return,
		// and read on its next visit eight seconds later - by which time the
		// travel had put the bot back on the horse - so a rider on a long leg was
		// down every eight seconds and mostly read nothing: 5 121 climb-downs in
		// 36 minutes on the test world, one rider down 268 times for 15 books.
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

	// Kamien Duchowy (50513) is the Grand Master's book: one read trains a skill
	// at G1..G10 on towards Perfect Master. The engine's half is
	// LearnGrandMasterSkill (a thirty percent roll, four under the first reads);
	// the rest is training_grandmaster_skill.quest, a dialog a bot cannot
	// answer, so this pass does what the quest does - twelve hours between
	// reads (waved away like the books' while the panel's BOOKS switch is on),
	// the stone spent either way, and the rank the training costs: 1000 plus
	// 500 a grade over G1 on a success, a third to a half of that on a failure,
	// twice as much for a rank already below zero. A bot trains only while the
	// full price leaves its rank at zero or above, so it never walks about with
	// a negative rank - the rank that lets a player hunt it for its gear
	// (ItemDropPenalty). The stones used to go to the merchant for 194 yang,
	// because nothing kept an ITEM_QUEST out of the junk rule ("Boty sprzedaja
	// kamienie zamiast z nich korzystac", mateuszp211, 15 September).
	void ManagePlayerBotGrandMasterTraining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotGrandMasterNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || ch->GetSkillGroup() == 0 ||
				ch->GetExchange() || ch->GetMyShop())
			return;
		DWORD& next = s_mapPlayerBotGrandMasterNext[ch->GetPlayerID()];
		if (dwNow < next)
			return;
		next = dwNow + PLAYERBOT_GRAND_MASTER_CHECK_INTERVAL;

		LPITEM stone = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !stone; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM && !item->isLocked())
				stone = item;
		}
		if (!stone)
			return;

		const char* nextTimeFlag = "training_grandmaster_skill.next_time";
		const int now = get_global_time();
		if (now < ch->GetQuestFlag(nextTimeFlag) && !IsPlayerBotFastBooksEnabled())
			return;

		// The skill the books would pick: the build's primary first, then the
		// highest grade.
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		DWORD skillVnum = 0;
		int bestPriority = INT_MIN;
		for (BYTE i = 0; i < build.bSkillCount; ++i)
		{
			const DWORD vnum = build.dwSkills[i];
			if (vnum == 0 || ch->GetSkillMasterType(vnum) != SKILL_GRAND_MASTER)
				continue;
			const int level = ch->GetSkillLevel(vnum);
			if (level < 30 || level >= 40)
				continue;
			if (ch->GetRealAlignment() < 1000 + 500 * (level - 30)) continue;
			const int priority = (vnum == build.dwPrimaryMaxSkill ? 10000 : 0) + level;
			if (priority > bestPriority)
			{
				bestPriority = priority;
				skillVnum = vnum;
			}
		}
		if (skillVnum == 0)
			return;

		const int level = ch->GetSkillLevel(skillVnum);
		const int rank = ch->GetRealAlignment();
		const int cost = (1000 + 500 * (level - 30)) * (rank < 0 ? 2 : 1);
		if (rank - cost < 0)
		{
			PlayerBotLogThrottled("grand_master_rank", dwNow,
					"PLAYERBOT_AI: grand master training waits for rank pid=%u name=%s skill=%u level=%d rank=%d cost=%d",
					ch->GetPlayerID(), ch->GetName(), skillVnum, level, rank, cost);
			return;
		}

		// item.remove(1): the quest spends the stone before the roll.
		if (stone->GetCount() > 1)
			stone->SetCount(stone->GetCount() - 1);
		else
			ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_GRAND_MASTER_READ");
		ch->SetQuestFlag(nextTimeFlag, now + PLAYERBOT_GRAND_MASTER_TRAIN_SECONDS);
		const bool learned = ch->LearnGrandMasterSkill(skillVnum);
		ch->UpdateAlignment(-(learned ? cost : number(cost / 3, cost / 2)));
		SetPlayerBotAction(state, BOT_ACTION_READ_BOOK, dwNow);
		sys_log(0, "PLAYERBOT_AI: grand master training %s pid=%u name=%s skill=%u level=%d->%d rank=%d->%d",
				learned ? "SUCCESS" : "FAILED", ch->GetPlayerID(), ch->GetName(), skillVnum,
				level, (int)ch->GetSkillLevel(skillVnum), rank, ch->GetRealAlignment());
	}

	// A rank below zero is what lets another player hunt a character for its
	// gear, and Fasolka Zen lifts it by up to its value0 - the engine takes a
	// bean only then. The training above never takes a bot under zero, so this
	// is the net for whatever else might ("boty powinny unikac biegania z
	// negatywna ranga", Tieru, 15 September).
	void ManagePlayerBotZenBeans(LPCHARACTER ch, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotZenBeanNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || ch->GetAlignment() >= 0)
			return;
		DWORD& next = s_mapPlayerBotZenBeanNext[ch->GetPlayerID()];
		if (dwNow < next)
			return;
		next = dwNow + PLAYERBOT_ZEN_BEAN_CHECK_INTERVAL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_ZEN_BEAN_VNUM || item->isLocked())
				continue;
			const int before = ch->GetRealAlignment();
			ch->UseItem(TItemPos(INVENTORY, cell));
			sys_log(0, "PLAYERBOT_AI: zen bean pid=%u name=%s rank=%d->%d",
					ch->GetPlayerID(), ch->GetName(), before, ch->GetRealAlignment());
			return;
		}
	}

	// A rank below zero keeps a bot inside its village's safe ring until it is
	// back. A character with a negative rank drops what it carries when a player
	// kills it, and outside the ring anybody may ("boty powinny unikac biegania
	// z negatywna ranga poza kolem", Tieru, 15 September, and a yes to holding
	// them there with only Fasolka Zen to lift the rank in town). Off its village
	// the bot is carried home to the market pitch, on a village map it walks
	// there, and inside the ring it stands: the shopping pass buys a bean off a
	// counter when one is there (WantsPlayerBotStallItem) and
	// ManagePlayerBotZenBeans eats it. The rest mark it renews is what keeps the
	// inactivity watchdog off a bot standing still on purpose. The Kamien
	// Duchowy never takes a bot under zero, so this is the net for whatever
	// else does.
	bool KeepPlayerBotNegativeRankInTown(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !ch->IsItemLoaded() || ch->GetRealAlignment() >= 0)
			return false;
		// A raider of the Demon Tower finishes the tower first.
		if (IsPlayerBotOnTowerBusiness(ch, state))
			return false;
		const long map = ch->GetMapIndex();
		if (IsPlayerBotSafeZone(map, ch->GetX(), ch->GetY()))
		{
			state.dwTownLingerUntil = dwNow + PLAYERBOT_NEGATIVE_RANK_HOLD_MS;
			if (ch->IsStateMove())
				ch->Stop();
			ClearPlayerBotRoute(state, true);
			SetPlayerBotAction(state, BOT_ACTION_TOWN_REST, dwNow);
			PlayerBotLogThrottled("negative_rank_hold", dwNow,
					"PLAYERBOT_AI: negative rank, holding in town pid=%u name=%s map=%ld rank=%d beans=%d",
					ch->GetPlayerID(), ch->GetName(), map, ch->GetRealAlignment(),
					(int)ch->CountSpecifyItem(PLAYERBOT_ZEN_BEAN_VNUM));
			return true;
		}
		playerbot_empire_rules::TPoint pitch;
		if (IsPlayerBotVillageMap(map) && playerbot_empire_rules::GetTownPitch(map, pitch))
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			// At the pitch is as good as inside the ring, should a pitch ever
			// stand on a cell the ring's attribute misses.
			if (MovePlayerBotTownLeg(ch, state, dwNow, pitch.x, pitch.y,
					PLAYERBOT_NEGATIVE_RANK_PITCH_ARRIVAL))
				state.dwTownLingerUntil = dwNow + PLAYERBOT_NEGATIVE_RANK_HOLD_MS;
			return true;
		}
		long destMap = 0, destX = 0, destY = 0;
		if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M1, destMap, destX, destY) &&
				TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "negative_rank_home"))
			sys_log(0, "PLAYERBOT_AI: negative rank, carried home pid=%u name=%s from=%ld rank=%d",
					ch->GetPlayerID(), ch->GetName(), map, ch->GetRealAlignment());
		return true;
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

	// The party census: counted over the same once-a-minute pass, reported
	// every ten minutes - how many the slider admits, how many are in a
	// party, how many parties. "Suwak Grupy (PT) nic nie robi" was measured
	// from the panel's one number; this is that number with its reasons.
	int s_iPlayerBotPartyCensusBots = 0;
	int s_iPlayerBotPartyCensusEligible = 0;
	int s_iPlayerBotPartyCensusInParty = 0;
	std::set<DWORD> s_setPlayerBotPartyCensusLeaders;
	DWORD s_dwPlayerBotPartyCensusReported = 0;

	void NotePlayerBotPartyCensus(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch)
			return;
		++s_iPlayerBotPartyCensusBots;
		if (IsPlayerBotPartyEligible(ch, state))
			++s_iPlayerBotPartyCensusEligible;
		if (LPPARTY party = ch->GetParty())
		{
			++s_iPlayerBotPartyCensusInParty;
			s_setPlayerBotPartyCensusLeaders.insert(party->GetLeaderPID());
		}
	}

	void ReportPlayerBotPartyCensus()
	{
		const DWORD dwNow = get_dword_time();
		if (s_dwPlayerBotPartyCensusReported == 0 ||
				dwNow - s_dwPlayerBotPartyCensusReported >= 600000)
		{
			s_dwPlayerBotPartyCensusReported = dwNow;
			sys_log(0, "PLAYERBOT_PARTY: census bots=%d eligible=%d in_party=%d parties=%d weight=%d village_per_mille=%d frontier_per_mille=%d",
					s_iPlayerBotPartyCensusBots, s_iPlayerBotPartyCensusEligible,
					s_iPlayerBotPartyCensusInParty, (int)s_setPlayerBotPartyCensusLeaders.size(),
					GetPlayerBotWeight(PLAYERBOT_WEIGHT_PARTY),
					GetPlayerBotPartyCohortPerMille(false), GetPlayerBotPartyCohortPerMille(true));
		}
		s_iPlayerBotPartyCensusBots = 0;
		s_iPlayerBotPartyCensusEligible = 0;
		s_iPlayerBotPartyCensusInParty = 0;
		s_setPlayerBotPartyCensusLeaders.clear();
	}

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
		// waiting for a bite - stillness is the activity, not a symptom. So does
		// a bot beside the player whose party it is: the follow pass only moves
		// it past PLAYERBOT_PARTY_FOLLOW_DISTANCE, and a reset after ninety
		// seconds next to an idle player took it out of the party below -
		// "dodaje boty do PT, a po chwili z niego wychodza" (sizowski, 14
		// September).
		if (moved || foughtRecently || castRecently || state.bFishingSession ||
				IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) ||
				state.dwTownLingerUntil != 0 || IsPlayerBotBesideHumanLeader(ch) ||
				// waiting for a floor's script in the Demon Tower, or for the
				// raid to gather on its ground floor (playerbot_demon_tower.h)
				state.lTowerInstance != 0 || state.dwTowerRaidGuild != 0 || state.bTowerSummoned ||
				// away from the keyboard, or pausing between two packs, in a
				// SLABY mood (playerbot_persona.h): standing still is the point
				(state.persona.dwAfkUntil != 0 && dwNow < state.persona.dwAfkUntil) ||
				(state.persona.dwPauseUntil != 0 && dwNow < state.persona.dwPauseUntil))
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
		// A player's party is not one of those: the player ends it. Nor is a
		// mercenary's contract, which ends by its own clock and its own terms.
		if (ch->GetParty() && !IsPlayerBotHumanLedParty(ch->GetParty()) &&
				!IsPlayerBotOnMercContract(ch->GetPlayerID()))
		{
			LeavePlayerBotParty(ch);
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

	EVENTINFO(playerbot_world_event_info)
	{
		CPlayerBotManager* manager;
	};

	// The world's part of Update on a core no bot has woken yet: the weights
	// file (the chest sliders are read there) and the timed events with their
	// chest gate. Update makes the same two calls on every tick, so once it
	// runs this clock has nothing left to do and ends.
	EVENTFUNC(playerbot_world_event)
	{
		if (s_pkPlayerBotUpdateEvent)
		{
			s_pkPlayerBotWorldEvent = NULL;
			return 0;
		}
		const DWORD dwNow = get_dword_time();
		RefreshPlayerBotWeights(dwNow);
		ManagePlayerBotEvents(dwNow);
#if defined(PLAYERBOT_ENGINE_MT2009)
		// A channel that starts with nobody still learns who is moved to it,
		// and spawns them; the first of them starts Update and ends this.
		playerbot_world_event_info* info = dynamic_cast<playerbot_world_event_info*>(event->info);
		if (info && info->manager)
			info->manager->ChannelClockTick(dwNow);
#endif
		return PASSES_PER_SEC(1);
	}

	CPlayerBotManager s_playerBotManager;
}

CPlayerBotManager::CPlayerBotManager()
	: m_dwNextSpawnBatchTime(0),
	  m_uSpawnBatchSize(0),
	  m_dwSpawnWindowStarted(0),
	  m_uSpawnWindowTotal(0),
	  m_dwNextTopUpTime(0),
	  m_dwNextBanCheckTime(0),
	  m_bRegistryLoaded(false),
	  m_bRegistryAvailable(false)
{
}

CPlayerBotManager::~CPlayerBotManager()
{
	if (s_pkPlayerBotUpdateEvent)
		event_cancel(&s_pkPlayerBotUpdateEvent);
	if (s_pkPlayerBotWorldEvent)
		event_cancel(&s_pkPlayerBotWorldEvent);
#if defined(PLAYERBOT_ENGINE_MT2009)
	if (m_pChannelSql)
	{
		delete m_pChannelSql;
		m_pChannelSql = NULL;
	}
#endif
}

void CPlayerBotManager::StartWorldClock()
{
	if (s_pkPlayerBotUpdateEvent || s_pkPlayerBotWorldEvent)
		return;
	playerbot_world_event_info* info = AllocEventInfo<playerbot_world_event_info>();
	info->manager = this;
	s_pkPlayerBotWorldEvent = event_create(playerbot_world_event, info, PASSES_PER_SEC(1));
	sys_log(0, "PLAYERBOT: world clock started (weights, timed events, chest gate)");
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

	// The two channels with moves: an identity plays on the channel its row
	// gives it, and only from the row's ready time - a bot that has just been
	// moved must be out of its old channel before it is loaded on the new one.
	if (m_bChannelTable)
	{
		TPlayerBotAccountMap::const_iterator owner = m_mapBotAccounts.find(dwPlayerID);
		if (owner == m_mapBotAccounts.end() || owner->second.bChannel != g_bChannel ||
				owner->second.dwReadyAt > (DWORD)get_global_time())
		{
			PlayerBotLogThrottled("spawn_not_ready", get_dword_time(),
					"PLAYERBOT_CHANNEL: refused pid=%u, assigned to channel %u or not ready yet (channel %u here)",
					dwPlayerID, owner == m_mapBotAccounts.end() ? 0U : (unsigned int)owner->second.bChannel,
					(unsigned int)g_bChannel);
			return false;
		}
	}

	if (IsManaged(dwPlayerID) || CHARACTER_MANAGER::instance().FindByPID(dwPlayerID))
		return false;
	// The channel partition (LoadRegisteredBots) is what keeps a bot on one
	// core; this is the belt to it. A pid the P2P table knows is logged in on
	// another core, and two copies of one character would each save over the
	// other and duplicate whatever either of them sold. The table knows only
	// the logins its peers have announced - not the first batch of a start -
	// which is why it is the belt and not the rule.
	if (P2P_MANAGER::instance().FindByPID(dwPlayerID))
	{
		PlayerBotLogThrottled("spawn_elsewhere", get_dword_time(),
				"PLAYERBOT_CHANNEL: refused pid=%u, already logged in on another core (channel %u here)",
				dwPlayerID, (unsigned int)g_bChannel);
		return false;
	}

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
#if defined(PLAYERBOT_ENGINE_MT2009)
		// Every bot holds the premium subscription (the operator's rule for
		// this world: "domyslnie wlacz kazdemu obecnemu i nowemu botowi").
		// The engine reads it once, in SetPlayerProto, from the descriptor's
		// account table - a human's comes from auth's premium_expire - and
		// GetPremiumRemainSeconds answers every PREMIUM_* type from it: the
		// experience and drop bonuses, the extra safebox page, the shop's
		// premium slots, fishing. Five years, well inside a 32-bit time_t.
		table.iPremium = get_global_time() + 5 * 365 * 24 * 3600;
#endif
	}

	m_mapBots.insert(TPlayerBotMap::value_type(dwPlayerID, d));
	m_mapHandles.insert(THandleToPlayerMap::value_type(d->GetHandle(), dwPlayerID));

	TBotPlayerLoadPacket packet;
	packet.player_id = dwPlayerID;
	packet.empire = bEmpire;
#if defined(PLAYERBOT_ENGINE_MT2009)
	// The db core keys the special flags on (pid or aid); a zero aid there
	// matched every bot's own flags at once and the load refused them all.
	packet.account_id = d->GetAccountTable().id;
#endif

	db_clientdesc->DBPacket(HEADER_GD_BOT_PLAYER_LOAD, d->GetHandle(), &packet, sizeof(packet));
	sys_log(0, "PLAYERBOT: requested player load pid=%u empire=%u handle=%u",
			dwPlayerID, bEmpire, d->GetHandle());
	return true;
}

// The bots that have ever kept an offline shop, which live on the first
// channel for good (playerbot_channel_rules.h). The table only grows: every
// core adds the owners it sees before it reads, and apply.sh does the same
// before any core starts. False when the table cannot be read.
static bool LoadPlayerBotChannelPins(std::set<DWORD>& out)
{
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	std::unique_ptr<SQLMsg> create(AccountDB::instance().DirectQuery(
			"CREATE TABLE IF NOT EXISTS player.playerbot_channel_pin ("
			"pid INT UNSIGNED NOT NULL PRIMARY KEY, pinned_at DATETIME NOT NULL) ENGINE=InnoDB"));
	std::unique_ptr<SQLMsg> add(AccountDB::instance().DirectQuery(
			"INSERT IGNORE INTO player.playerbot_channel_pin (pid, pinned_at) "
			"SELECT owner, NOW() FROM player.ikashop_offlineshop"));
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
			"SELECT pid FROM player.playerbot_channel_pin"));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return false;
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		if (row[0])
			str_to_number(pid, row[0]);
		if (pid != 0)
			out.insert(pid);
	}
#else
	// A classic stall is the keeper's own and ends with it: nothing of it is
	// left on a channel to be pinned to.
	(void)out;
#endif
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
	m_setAllRegisteredBots.clear();
	m_mapBotAccounts.clear();

	// The second channel's plan (playerbot_channel_rules.h): the switch and the
	// share from the container's environment, which every core of this world
	// shares, and the pins. Each core registers only its own channel's
	// identities, so nothing downstream - the split, the queues, the top-up,
	// a GM's spawn - can start another channel's bot here.
	m_bSecondChannel = false;
	m_iSecondChannelShare = playerbot_channel_rules::CH2_SHARE_DEFAULT;
	for (int c = 0; c < 3; ++c)
		for (int e = 0; e < 4; ++e)
			m_aChannelIdentities[c][e] = 0;
	const char* secondChannel = std::getenv("M2_PLAYERBOT_CH2");
	if (secondChannel && *secondChannel && std::atoi(secondChannel) != 0)
		m_bSecondChannel = true;
	const char* secondShare = std::getenv("PLAYERBOT_CH2_SHARE");
	if (secondShare && *secondShare)
		m_iSecondChannelShare = playerbot_channel_rules::ClampShare(std::atoi(secondShare));
	// With the second channel on and offline shops in the world, the channels
	// come from the assignment table (the two channels with moves, mt2009):
	// the pins stood every bot that ever kept a shop on the first channel for
	// good, which on a world that has played is nearly every bot.
	m_bChannelTable = false;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	if (m_bSecondChannel)
	{
		// Both tables are the migrator's too (apply.sh); asked for here as
		// well, so a core that starts before a migrator of this version has run
		// still finds them. Idempotent, and at boot, so a blocking query is fine.
		std::unique_ptr<SQLMsg> assignment(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS common.playerbot_channel_assignment ("
				"pid INT UNSIGNED NOT NULL, channel TINYINT UNSIGNED NOT NULL, "
				"active TINYINT(1) NOT NULL DEFAULT 0, requested_channel TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"request_reason VARCHAR(32) NOT NULL DEFAULT '', request_at DATETIME NULL, "
				"ready_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, last_map INT NOT NULL DEFAULT 0, "
				"shop_busy TINYINT(1) NOT NULL DEFAULT 0, last_seen DATETIME NULL, moved_at DATETIME NULL, "
				"updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
				"PRIMARY KEY (pid), KEY channel_seen (channel,last_seen), "
				"KEY requested (requested_channel,request_at)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> control(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS common.playerbot_channel_control ("
				"id TINYINT UNSIGNED NOT NULL, last_batch DATETIME NOT NULL DEFAULT '2000-01-01 00:00:00', "
				"PRIMARY KEY (id)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> controlRow(AccountDB::instance().DirectQuery(
				"INSERT IGNORE INTO common.playerbot_channel_control (id) VALUES (1)"));
		m_bChannelTable = true;
	}
#endif
	std::set<DWORD> pins;
	const bool pinsKnown = m_bChannelTable || !m_bSecondChannel || LoadPlayerBotChannelPins(pins);
	// Without the pins a pinned bot's channel cannot be told, so the second
	// channel takes nobody and the first takes only whom the spread gives it:
	// a bot may then start nowhere, and never twice.
	if (!pinsKnown)
		sys_err("PLAYERBOT_CHANNEL: player.playerbot_channel_pin cannot be read; %s",
				g_bChannel == 1 ? "only the spread's first-channel bots start here"
						: "this channel starts no bots");
	unsigned int otherChannel = 0;

	// The assignment table's row, when there is one, rides with the registry:
	// the channel and the seconds left until the bot may log in there. A pid
	// with no row takes the spread's channel, the same on every core - the
	// coordinator writes that row down (SeedChannelAssignments).
	const std::string registryHead = "SELECT l.pid, a.id, a.login, pi.empire, p.level";
	const std::string registryChannel = m_bChannelTable
			? ", c.channel, COALESCE(GREATEST(0,TIMESTAMPDIFF(SECOND,NOW(),c.ready_at)),0) "
			: " ";
	const std::string registryJoin = m_bChannelTable
			? "LEFT JOIN common.playerbot_channel_assignment AS c ON c.pid=l.pid "
			: "";
	const std::string query = registryHead + registryChannel +
			"FROM common.playerbot_seed_state AS l "
			"JOIN player.player AS p ON p.id=l.pid "
			"JOIN account.account AS a ON a.id=p.account_id "
			"JOIN player.player_index AS pi ON pi.id=a.id " + registryJoin +
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

	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query.c_str()));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() ||
			!msg->Get()->pSQLResult)
	{
		// Fail closed with the table too: a core that guessed its channels
		// could start a bot the other channel holds.
		sys_err("PLAYERBOT_AUTH: registry query failed%s; refusing every bot spawn",
				m_bChannelTable ? " (with the channel assignment table)" : "");
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
			m_setAllRegisteredBots.insert(pid);
			int channel = 0;
			unsigned int readyIn = 0;
			if (m_bChannelTable)
			{
				unsigned int rowChannel = 0;
				if (row[5])
					str_to_number(rowChannel, row[5]);
				if (row[6])
					str_to_number(readyIn, row[6]);
				channel = (rowChannel == 1 || rowChannel == 2) ? (int)rowChannel
						: playerbot_channel_rules::ChannelOf(pid, true, m_iSecondChannelShare, false);
			}
			else
				channel = playerbot_channel_rules::ChannelOf(pid, m_bSecondChannel,
						m_iSecondChannelShare, pins.find(pid) != pins.end());
			++m_aChannelIdentities[channel][empire];
			const bool here = channel == (int)g_bChannel && (m_bChannelTable || pinsKnown || g_bChannel == 1);
			if (here)
				m_setRegisteredBots.insert(pid);
			else
				++otherChannel;
			// With the table every identity keeps its record whatever its
			// channel: a move may bring it here, and it must be known by then.
			if (!here && !m_bChannelTable)
				continue;
			TPlayerBotAccount account;
			account.dwID = 0;
			account.bEmpire = (BYTE)empire;
			if (row[1])
				str_to_number(account.dwID, row[1]);
			if (row[2])
				account.strLogin = row[2];
			// The level the character was saved at, which is what the medal
			// droppers' cohort is chosen by (SpawnMedalDropperCohort).
			unsigned int level = 0;
			if (row[4])
				str_to_number(level, row[4]);
			account.bLevel = (BYTE)std::min<unsigned int>(level, 255);
			account.bChannel = (BYTE)channel;
			account.dwReadyAt = readyIn ? (DWORD)get_global_time() + readyIn : 0;
			m_mapBotAccounts[pid] = account;
		}
	}

	if (m_bSecondChannel || g_bChannel != 1)
		sys_log(0, "PLAYERBOT_CHANNEL: channel=%u second=%d table=%d share=%d here=%u elsewhere=%u "
				"ch1=%d/%d/%d ch2=%d/%d/%d pinned=%u pins_read=%d",
				(unsigned int)g_bChannel, m_bSecondChannel ? 1 : 0, m_bChannelTable ? 1 : 0,
				m_iSecondChannelShare,
				(unsigned int)m_setRegisteredBots.size(), otherChannel,
				m_aChannelIdentities[1][1], m_aChannelIdentities[1][2], m_aChannelIdentities[1][3],
				m_aChannelIdentities[2][1], m_aChannelIdentities[2][2], m_aChannelIdentities[2][3],
				(unsigned int)pins.size(), pinsKnown ? 1 : 0);

	m_bRegistryAvailable = !m_setRegisteredBots.empty();
	if (!m_bRegistryAvailable)
	{
		// A channel the partition gives nobody is not a broken registry.
		if (!m_setAllRegisteredBots.empty())
			sys_log(0, "PLAYERBOT_CHANNEL: channel %u carries no bots", (unsigned int)g_bChannel);
		else
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
	ReportPlayerBotRegistryShortfall((unsigned int)m_setAllRegisteredBots.size());
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
			// Three kingdoms are registered now, so only an empire outside
			// 1..3 is a rejection. Left at "<> 2" this line reported every
			// Shinsoo and Jinno identity as refused - a thousand of two
			// thousand - beside a loader that had just accepted them.
			" SUM(pi.id IS NOT NULL AND pi.empire NOT IN (1,2,3)) "
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
		// With the assignment table every identity is kept, whatever its
		// channel (a move may bring it here); this channel's are counted.
		if (it->second.bChannel != g_bChannel)
			continue;
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

bool CPlayerBotManager::IsRegisteredBotPID(DWORD dwPlayerID) const
{
	// Any channel's bot: this answers "is this character a bot", which a guild
	// master or a party leader on the other channel is just as much.
	return m_bRegistryLoaded &&
			m_setAllRegisteredBots.find(dwPlayerID) != m_setAllRegisteredBots.end();
}

void CPlayerBotManager::SplitForThisChannel(int total, const int* registeredHere, int* want)
{
	LoadRegisteredBots();
	if (!want || !registeredHere)
		return;
	if (!m_bSecondChannel)
	{
		if (g_bChannel != 1)
			for (int e = 0; e < playerbot_empire_rules::EMPIRE_COUNT; ++e)
				want[e] = 0;
		return;
	}
	// A kingdom M2_PLAYERBOT_KINGDOMS=0 leaves out has no share anywhere.
	const char* kingdoms = std::getenv("M2_PLAYERBOT_KINGDOMS");
	const bool chunjoOnly = kingdoms && *kingdoms && std::atoi(kingdoms) == 0;
	int world[playerbot_empire_rules::EMPIRE_COUNT] = { 0, 0, 0, 0 };
	int worldWant[playerbot_empire_rules::EMPIRE_COUNT];
	for (int e = playerbot_empire_rules::EMPIRE_SHINSOO; e <= playerbot_empire_rules::EMPIRE_JINNO; ++e)
		if (!chunjoOnly || e == playerbot_empire_rules::EMPIRE_CHUNJO)
			world[e] = m_aChannelIdentities[1][e] + m_aChannelIdentities[2][e];
	playerbot_empire_rules::SplitPopulation(total, world, worldWant);
	for (int e = 0; e < playerbot_empire_rules::EMPIRE_COUNT; ++e)
		want[e] = 0;
	for (int e = playerbot_empire_rules::EMPIRE_SHINSOO; e <= playerbot_empire_rules::EMPIRE_JINNO; ++e)
	{
		const int here = playerbot_channel_rules::ShareOfTotal(worldWant[e], true,
				m_iSecondChannelShare, (int)g_bChannel, m_aChannelIdentities[2][e]);
		want[e] = std::min(here, std::max(0, registeredHere[e]));
	}
	sys_log(0, "PLAYERBOT: autospawn world=%d, channel %u starts %d/%d/%d (world split %d/%d/%d)",
			total, (unsigned int)g_bChannel, want[1], want[2], want[3],
			worldWant[1], worldWant[2], worldWant[3]);
}

int CPlayerBotManager::ScaleToThisChannel(int total, BYTE bEmpire)
{
	// The plan is read with the registry; a channel with no identities of its
	// own must still learn that it takes nothing, so the load is asked for
	// that whatever it answers.
	LoadRegisteredBots();
	int second = 0;
	if (bEmpire >= 1 && bEmpire <= 3)
		second = m_aChannelIdentities[2][bEmpire];
	else
		for (int e = 1; e <= 3; ++e)
			second += m_aChannelIdentities[2][e];
	return playerbot_channel_rules::ShareOfTotal(total, m_bSecondChannel,
			m_iSecondChannelShare, (int)g_bChannel, second);
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

	const size_t batches = std::max<size_t>(1, m_dwSpawnWindowMs / PLAYERBOT_SPAWN_BATCH_INTERVAL);
	m_uSpawnBatchSize = std::max<size_t>(1,
			(m_setScheduledBots.size() + batches - 1) / batches);
	m_dwSpawnWindowStarted = get_dword_time();
	m_uSpawnWindowTotal = m_setScheduledBots.size();
	m_dwNextSpawnBatchTime = 0;
	sys_log(0, "PLAYERBOT: staggered spawn empire=%u scheduled=%u total=%u batch=%u every=%ums window=%ums",
			(unsigned int)bEmpire, (unsigned int)selected,
			(unsigned int)m_uSpawnWindowTotal, (unsigned int)m_uSpawnBatchSize,
			PLAYERBOT_SPAWN_BATCH_INTERVAL, m_dwSpawnWindowMs);
	SpawnPendingBatch(get_dword_time());
	return selected;
}

// The medal droppers an operator asks for, on top of the population
// (PLAYERBOT_MEDAL_DROPPERS a kingdom, PLAYERBOT_MEDAL_DROPPER_LEVEL): "po 33
// osoby na kazde krolestwo z osobowoscia dropek medali, aby grali w lochu malp
// i mieli zablokowany exp" (Tieru, 15 September). They are taken from the far
// end of the kingdom's registry, where the identities that have never played
// stand, so the ordinary slider - which takes the registry from the front -
// does not reach them until nearly every bot plays, and the same characters
// are chosen after every restart. One saved more than two levels over the lock
// is passed over: two is the margin for a level taken on the tick before the
// lock landed. Scheduled before the ordinary cohort, which steps over them, and
// restored by TopUpMissingBots like the rest.
size_t CPlayerBotManager::SpawnMedalDropperCohort(size_t count, BYTE bEmpire, BYTE bExpLockLevel)
{
	// The operator's number is per kingdom for the world: the first channel
	// starts the cohort, or two channels would start it twice.
	if (g_bChannel != 1)
		return 0;
	if (count == 0 || bEmpire < 1 || bEmpire > 3 || bExpLockLevel == 0 || !LoadRegisteredBots())
		return 0;
	m_bMedalDropperCohortLevel = bExpLockLevel;
	size_t selected = 0;
	for (TRegisteredPlayerBotSet::const_reverse_iterator it = m_setRegisteredBots.rbegin();
			it != m_setRegisteredBots.rend() && selected < count; ++it)
	{
		TPlayerBotAccountMap::const_iterator account = m_mapBotAccounts.find(*it);
		if (account == m_mapBotAccounts.end() || account->second.bEmpire != bEmpire ||
				(int)account->second.bLevel > (int)bExpLockLevel + 2)
			continue;
		if (m_setScheduledBots.find(*it) != m_setScheduledBots.end())
			continue;
		m_setMedalDropperCohort.insert(*it);
		m_dequePendingSpawns.push_back(*it);
		m_setScheduledBots.insert(*it);
		++selected;
	}

	const size_t batches = std::max<size_t>(1, m_dwSpawnWindowMs / PLAYERBOT_SPAWN_BATCH_INTERVAL);
	m_uSpawnBatchSize = std::max<size_t>(1,
			(m_setScheduledBots.size() + batches - 1) / batches);
	m_dwSpawnWindowStarted = get_dword_time();
	m_uSpawnWindowTotal = m_setScheduledBots.size();
	m_dwNextSpawnBatchTime = 0;
	sys_log(0, "PLAYERBOT: medal dropper cohort empire=%u asked=%u scheduled=%u exp_lock=%u",
			(unsigned int)bEmpire, (unsigned int)count, (unsigned int)selected,
			(unsigned int)bExpLockLevel);
	SpawnPendingBatch(get_dword_time());
	return selected;
}

bool CPlayerBotManager::IsMedalDropperCohortPID(DWORD dwPlayerID) const
{
	return m_setMedalDropperCohort.find(dwPlayerID) != m_setMedalDropperCohort.end();
}

BYTE CPlayerBotManager::GetMedalDropperCohortLevel() const
{
	return m_bMedalDropperCohortLevel;
}

// One batch from the queue, if one is due. Called from Update every tick and
// once directly from SpawnRegistered.
void CPlayerBotManager::SpawnPendingBatch(DWORD dwNow)
{
	if (m_dequePendingSpawns.empty() || dwNow < m_dwNextSpawnBatchTime)
		return;
	// Held at the door: the world was made a moment ago and its rates, its
	// respawns and its personalities are still whatever the install shipped
	// with. Nothing is taken off the queue, so letting the bots in from the
	// panel fills the world through the ordinary spawn window (see
	// IsPlayerBotSpawnHeld).
	if (IsPlayerBotSpawnHeld())
		return;
	m_dwNextSpawnBatchTime = dwNow + PLAYERBOT_SPAWN_BATCH_INTERVAL;
	size_t sent = 0;
	while (!m_dequePendingSpawns.empty() && sent < m_uSpawnBatchSize)
	{
		const DWORD pid = m_dequePendingSpawns.front();
		m_dequePendingSpawns.pop_front();
		// A banned bot is dropped from the batch rather than spawned; it stays
		// scheduled, so removing the ban lets a later top-up bring it back.
		if (m_setBannedBots.find(pid) != m_setBannedBots.end())
			continue;
		// A resting one likewise: its rest ends in ManageLifeSchedule.
		if (IsRestingBot(pid))
			continue;
		Spawn(pid, GetRegisteredEmpire(pid));
		++sent;
	}
	if (m_dequePendingSpawns.empty())
		sys_log(0, "PLAYERBOT: staggered spawn complete scheduled=%u over=%ums",
				(unsigned int)m_uSpawnWindowTotal,
				(unsigned int)(dwNow - m_dwSpawnWindowStarted));
}

// The operator's spawn plan. The cohort's window: a minute by default, up to
// PLAYERBOT_SPAWN_WINDOW_MAX_MINUTES, so a thousand bots can take a quarter of
// an hour to come in instead of filling the square in sixty seconds.
void CPlayerBotManager::SetSpawnWindow(DWORD dwWindowMs)
{
	const DWORD maxMs = PLAYERBOT_SPAWN_WINDOW_MAX_MINUTES * 60U * 1000U;
	if (dwWindowMs < PLAYERBOT_SPAWN_WINDOW)
		dwWindowMs = PLAYERBOT_SPAWN_WINDOW;
	else if (dwWindowMs > maxMs)
		dwWindowMs = maxMs;
	m_dwSpawnWindowMs = dwWindowMs;
	sys_log(0, "PLAYERBOT: spawn window %u s", (unsigned int)(dwWindowMs / 1000U));
}

// A second cohort that joins one at a time: the next `count` identities of the
// kingdom after the ones already scheduled, each given its moment spread evenly
// over the window. Nothing is in the world or in m_setScheduledBots until that
// moment, so the top-up neither counts nor hurries them; from it on they are
// the cohort's like the rest. "Dodatkowe 500 botow dolacza stopniowo w ciagu
// 24 godzin" (Tieru, 16 September).
size_t CPlayerBotManager::ScheduleLateJoiners(size_t count, BYTE bEmpire, DWORD dwWindowMs)
{
	if (count == 0 || bEmpire < 1 || bEmpire > 3 || !LoadRegisteredBots())
		return 0;
	const DWORD maxMs = PLAYERBOT_LATE_JOIN_MAX_HOURS * 60U * 60U * 1000U;
	if (dwWindowMs < 60000U)
		dwWindowMs = 60000U;
	else if (dwWindowMs > maxMs)
		dwWindowMs = maxMs;
	std::set<DWORD> waiting;
	for (std::deque<std::pair<DWORD, DWORD> >::const_iterator it = m_dequeLateJoiners.begin();
			it != m_dequeLateJoiners.end(); ++it)
		waiting.insert(it->second);
	std::vector<DWORD> chosen;
	for (TRegisteredPlayerBotSet::const_iterator it = m_setRegisteredBots.begin();
			it != m_setRegisteredBots.end() && chosen.size() < count; ++it)
	{
		if (GetRegisteredEmpire(*it) != bEmpire)
			continue;
		if (m_setScheduledBots.find(*it) != m_setScheduledBots.end() ||
				m_setMedalDropperCohort.find(*it) != m_setMedalDropperCohort.end() ||
				waiting.find(*it) != waiting.end())
			continue;
		chosen.push_back(*it);
	}
	const DWORD dwNow = get_dword_time();
	for (size_t i = 0; i < chosen.size(); ++i)
	{
		// The i-th of n joins at (i+1)/(n+1) of the window: the first not at
		// once, the last not at the very end.
		const DWORD due = dwNow + (DWORD)((unsigned long long)dwWindowMs * (i + 1) / (chosen.size() + 1));
		m_dequeLateJoiners.push_back(std::make_pair(due, chosen[i]));
	}
	std::sort(m_dequeLateJoiners.begin(), m_dequeLateJoiners.end());
	m_uLateJoinersTotal += chosen.size();
	sys_log(0, "PLAYERBOT: late joiners empire=%u scheduled=%u over=%umin first_in=%umin waiting=%u",
			(unsigned int)bEmpire, (unsigned int)chosen.size(),
			(unsigned int)(dwWindowMs / 60000U),
			m_dequeLateJoiners.empty() ? 0U : (unsigned int)((m_dequeLateJoiners.front().first - dwNow) / 60000U),
			(unsigned int)m_dequeLateJoiners.size());
	return chosen.size();
}

void CPlayerBotManager::SpawnLateJoiners(DWORD dwNow)
{
	if (IsPlayerBotSpawnHeld())
		return;
	while (!m_dequeLateJoiners.empty() && (int)(dwNow - m_dequeLateJoiners.front().first) >= 0)
	{
		const DWORD pid = m_dequeLateJoiners.front().second;
		m_dequeLateJoiners.pop_front();
		if (m_setScheduledBots.find(pid) != m_setScheduledBots.end())
			continue;
		m_setScheduledBots.insert(pid);
		// A banned or resting one is scheduled and not spawned: the top-up
		// brings it in when the ban lifts or the rest ends, like anybody's.
		if (m_setBannedBots.find(pid) != m_setBannedBots.end() || IsRestingBot(pid))
			continue;
		Spawn(pid, GetRegisteredEmpire(pid));
		sys_log(0, "PLAYERBOT: late joiner pid=%u empire=%u left=%u of %u",
				pid, (unsigned int)GetRegisteredEmpire(pid),
				(unsigned int)m_dequeLateJoiners.size(), (unsigned int)m_uLateJoinersTotal);
	}
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
		// A banned bot is missing on purpose; leaving it out of the queue keeps
		// the top-up from asking for it every minute only for SpawnPendingBatch
		// to drop it again.
		else if (m_setBannedBots.find(*it) == m_setBannedBots.end() && !IsRestingBot(*it))
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

// Take out whoever a GM has banned, and keep them out.
//
// Reported as "I ban a bot, kick it, and it logs straight back in"
// (mateuszp211): the kick removes the character, TopUpMissingBots counts it
// missing a minute later and re-queues it, and nothing consulted the ban. The
// obvious guard - account.status='BLOCK' - is useless here, because every bot
// account is created BLOCK on purpose so no human can log into one; that column
// says nothing about who a GM banned. account.account_block is the ledger the
// ban actually writes (/block_player -> BanManager::Block), empty until then, so
// a row for a bot's account is an unambiguous "this one is banned" that cannot
// misfire on the 2500 normal bots. Read on the top-up cadence.
void CPlayerBotManager::RefreshBannedBots(DWORD dwNow)
{
	if (m_dwNextBanCheckTime != 0 && dwNow < m_dwNextBanCheckTime)
		return;
	m_dwNextBanCheckTime = dwNow + PLAYERBOT_TOPUP_INTERVAL;
	if (m_setRegisteredBots.empty())
		return;

	// Only our own registered characters, so a ban on a real player's account
	// can never appear here. account_block holds both instant and queued bans
	// (status 1 and 0); either one means banned. Joined to the character, since
	// the manager keys everything by PID.
	const char* query =
			"SELECT p.id FROM account.account_block AS b "
			"JOIN player.player AS p ON p.account_id=b.account_id "
			"JOIN account.account AS a ON a.id=b.account_id "
			"WHERE BINARY a.login LIKE BINARY 'playerbot\\_%'";

	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return; // leave the set as it was rather than unbanning on a hiccup

	std::set<DWORD> banned;
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		if (row[0])
			str_to_number(pid, row[0]);
		// Only PIDs this core actually owns as registered bots.
		if (pid != 0 && m_setRegisteredBots.find(pid) != m_setRegisteredBots.end())
			banned.insert(pid);
	}
	m_setBannedBots.swap(banned);
	if (m_setBannedBots.empty())
		return;

	// Out of the world now, and off the spawn queue so the top-up cannot bring
	// them back. Kept in m_setScheduledBots: unbanning (the row removed) lets the
	// next top-up spawn them again, exactly as if they had just gone missing.
	unsigned int despawned = 0;
	for (std::set<DWORD>::const_iterator it = m_setBannedBots.begin();
			it != m_setBannedBots.end(); ++it)
	{
		if (CHARACTER_MANAGER::instance().FindByPID(*it) != NULL && Despawn(*it))
			++despawned;
	}
	sys_log(0, "PLAYERBOT_AUTH: banned bots=%u despawned=%u",
			(unsigned int)m_setBannedBots.size(), despawned);
}

bool CPlayerBotManager::Despawn(DWORD dwPlayerID)
{
	TPlayerBotMap::iterator it = m_mapBots.find(dwPlayerID);
	if (it == m_mapBots.end())
		return false;

	LPDESC d = it->second;
	m_mapBots.erase(it);
	s_mapPlayerBotAIStates.erase(dwPlayerID);
	// Its F10 history and its remembered level go with it.
	ForgetPlayerBotAdminState(dwPlayerID);
	if (d)
		m_mapHandles.erase(d->GetHandle());

	if (d)
		DESC_MANAGER::instance().DestroyDesc(d);

	sys_log(0, "PLAYERBOT: despawned pid=%u", dwPlayerID);
	return true;
}

bool CPlayerBotManager::IsRestingBot(DWORD dwPlayerID) const
{
	return m_mapLifeRestEnd.find(dwPlayerID) != m_mapLifeRestEnd.end();
}

// "Boty graja jak zywi ludzie" - the LIFE switch of the weights file, off by
// default and experimental: a bot plays for PLAYERBOT_LIFE_SESSION_MIN..MAX,
// logs out, rests for PLAYERBOT_LIFE_REST_MIN..MAX and comes back through the
// top-up, which leaves a resting bot alone (IsRestingBot) the way it leaves a
// banned one. The first session after a start is drawn from half an hour up
// so the log-outs spread over the day instead of the cohort leaving together;
// a bot in a player's party is not logged out from under them, it waits. Its
// offline shop stands on, as any player's does. Switched off, every rest ends
// at once and the top-up fills the world again.
void CPlayerBotManager::ManageLifeSchedule(DWORD dwNow)
{
	if (m_dwNextLifeCheckTime != 0 && dwNow < m_dwNextLifeCheckTime)
		return;
	m_dwNextLifeCheckTime = dwNow + PLAYERBOT_LIFE_CHECK_INTERVAL;
	if (!IsPlayerBotLifeScheduleEnabled())
	{
		if (!m_mapLifeSessionEnd.empty() || !m_mapLifeRestEnd.empty() || !m_setLifeReturning.empty())
		{
			sys_log(0, "PLAYERBOT_LIFE: schedule off, %u resting come back",
					(unsigned int)m_mapLifeRestEnd.size());
			m_mapLifeSessionEnd.clear();
			m_mapLifeRestEnd.clear();
			m_setLifeReturning.clear();
			m_dwNextTopUpTime = 0;
		}
		return;
	}

	std::vector<DWORD> leaving;
	// No more than PLAYERBOT_LIFE_MAX_RESTING_PERCENT of the cohort rests at
	// once; a session that ends past that goes on a little longer. Without the
	// cap the schedule did exactly what it said and a world emptied: every bot
	// starts with the core, every first session ends inside the same six
	// hours, and a rest is three to nine - so at six hours nine in ten of the
	// cohort were resting and only one in ten had come back, and the square
	// looked abandoned for the whole evening. Kuszaa's chart of 21 September
	// is that curve to the bot: 500 a kingdom at 14:30, 40 at 20:40, then
	// back up to 270 and down again ("boty poszly na odpoczynek ale z niego
	// nie wracaja"). They were coming back; too few at a time.
	const size_t maxResting = m_setScheduledBots.size() * PLAYERBOT_LIFE_MAX_RESTING_PERCENT / 100;
	unsigned int heldOn = 0;
	for (TPlayerBotMap::const_iterator it = m_mapBots.begin(); it != m_mapBots.end(); ++it)
	{
		const DWORD pid = it->first;
		std::map<DWORD, DWORD>::iterator session = m_mapLifeSessionEnd.find(pid);
		if (session == m_mapLifeSessionEnd.end())
		{
			// Back from a rest: a whole session. Just started with the world:
			// anything from half an hour, so the first log-outs spread.
			const bool returning = m_setLifeReturning.erase(pid) > 0;
			const DWORD floor = returning ? PLAYERBOT_LIFE_SESSION_MIN_MS : PLAYERBOT_LIFE_FIRST_SESSION_MIN_MS;
			const DWORD spread = PlayerBotNavHash(pid ^ (dwNow / 1000U) ^ 0x4c494645U) %
					(PLAYERBOT_LIFE_SESSION_MAX_MS - floor);
			m_mapLifeSessionEnd[pid] = dwNow + floor + spread;
			continue;
		}
		if ((int)(dwNow - session->second) < 0)
			continue;
		LPCHARACTER ch = it->second ? it->second->GetCharacter() : NULL;
		if (ch && ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
		{
			session->second = dwNow + PLAYERBOT_LIFE_POSTPONE_MS;
			continue;
		}
		if (m_mapLifeRestEnd.size() + leaving.size() >= maxResting)
		{
			// The resting are at the cap: this one plays on until somebody
			// comes back, and asks again after a while drawn by pid so the
			// held do not all leave on the same minute.
			session->second = dwNow + PLAYERBOT_LIFE_HOLD_MIN_MS +
					PlayerBotNavHash(pid ^ (dwNow / 1000U) ^ 0x484f4c44U) %
					(PLAYERBOT_LIFE_HOLD_MAX_MS - PLAYERBOT_LIFE_HOLD_MIN_MS);
			++heldOn;
			continue;
		}
		leaving.push_back(pid);
	}
	for (size_t i = 0; i < leaving.size(); ++i)
	{
		const DWORD pid = leaving[i];
		const DWORD rest = PLAYERBOT_LIFE_REST_MIN_MS +
				PlayerBotNavHash(pid ^ dwNow ^ 0x52455354U) %
				(PLAYERBOT_LIFE_REST_MAX_MS - PLAYERBOT_LIFE_REST_MIN_MS);
		char szName[CHARACTER_NAME_MAX_LEN + 1];
		szName[0] = '\0';
		TPlayerBotMap::const_iterator bot = m_mapBots.find(pid);
		if (bot != m_mapBots.end() && bot->second && bot->second->GetCharacter())
			strlcpy(szName, bot->second->GetCharacter()->GetName(), sizeof(szName));
		m_mapLifeSessionEnd.erase(pid);
		if (!Despawn(pid))
			continue;
		m_mapLifeRestEnd[pid] = dwNow + rest;
		sys_log(0, "PLAYERBOT_LIFE: logged out pid=%u name=%s rest=%umin",
				pid, szName, (unsigned int)(rest / 60000U));
	}
	// Whose rest is over: off the resting list, and the top-up on the next
	// tick brings them back the way it brings anybody back.
	unsigned int back = 0;
	for (std::map<DWORD, DWORD>::iterator it = m_mapLifeRestEnd.begin(); it != m_mapLifeRestEnd.end(); )
	{
		if ((int)(dwNow - it->second) >= 0)
		{
			sys_log(0, "PLAYERBOT_LIFE: back pid=%u", it->first);
			m_setLifeReturning.insert(it->first);
			m_mapLifeRestEnd.erase(it++);
			++back;
		}
		else
			++it;
	}
	if (back > 0)
		m_dwNextTopUpTime = 0;
	// A sessioned bot that left the world for another reason (a ban, a load
	// failure) would keep a stale entry; forget what is not in the world.
	for (std::map<DWORD, DWORD>::iterator it = m_mapLifeSessionEnd.begin(); it != m_mapLifeSessionEnd.end(); )
	{
		if (m_mapBots.find(it->first) == m_mapBots.end())
			m_mapLifeSessionEnd.erase(it++);
		else
			++it;
	}
	if (m_dwNextLifeCensusTime == 0 || dwNow >= m_dwNextLifeCensusTime)
	{
		m_dwNextLifeCensusTime = dwNow + PLAYERBOT_LIFE_CENSUS_INTERVAL;
		sys_log(0, "PLAYERBOT_LIFE: census online=%u resting=%u returning=%u left_now=%u back_now=%u held_on=%u cap=%u",
				(unsigned int)m_mapBots.size(), (unsigned int)m_mapLifeRestEnd.size(),
				(unsigned int)m_setLifeReturning.size(), (unsigned int)leaving.size(), back,
				heldOn, (unsigned int)maxResting);
	}
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
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		// A bot the coordinator moved here came for its stand: the first
		// service is not spread over the next ten minutes like a start's.
		if (m_setChannelMovedIn.erase(dwPID))
			state.offlineShop.nextService = now + 5000;
#endif

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
		state.persona.bDrawnPersonality = GetPlayerBotStablePersonality(
				d->GetCharacter(), state.bBotRole);
		// Under Iwakura's personalities the draw is the bot's character, and a
		// dropper's gives way to the Grinder's tiers (GetPlayerBotCharakter).
		state.bPersonality = IsPlayerBotPersonaEnabled()
				? GetPlayerBotCharakter(state.persona.bDrawnPersonality)
				: state.persona.bDrawnPersonality;
		// The operator's medal droppers are that and nothing else, whatever
		// their pid draws: no party role, no stone hunting.
		if (IsMedalDropperCohortPID(dwPID))
		{
			state.bBotRole = BOT_ROLE_MOB_GRINDER;
			state.bPersonality = BOT_PERSONALITY_MEDAL_DROPPER;
			state.persona.bDrawnPersonality = BOT_PERSONALITY_MEDAL_DROPPER;
		}
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

// The light half of a bot's tick: the next leg of a route already planned and
// the blow at the target already in hand. It plans nothing and looks for
// nothing, which is why it may run for every bot the budgeted pass did not
// reach. Without that, a pass cut by the budget left a bot standing at the end
// of its waypoint until the sweep came round to it again - at 1500 bots on one
// core that was two seconds and more, "dwa kroki i staja" (SIZOWSKI, 18
// September), while a bot's full tick only has to come that often.
static void RunPlayerBotLightTick(LPDESC d, LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
{
	if (!d->IsPhase(PHASE_GAME) || ch->IsDead())
		return;
	// Following an already computed route is cheap; planning one is not, and
	// stays in the full tick.
	if (!state.vecRoute.empty() && state.uRouteIndex < state.vecRoute.size() &&
			state.lRouteMapIndex == ch->GetMapIndex())
		MovePlayerBot(ch, state.lRouteDestX, state.lRouteDestY, dwNow, 32, true,
				state.bRouteAllowsHorse);
	LPCHARACTER quickTarget = state.dwTargetVID != 0
			? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
	ExecutePlayerBotBasicAttack(ch, quickTarget, state, dwNow);
	// This pass lands about half of all killing blows, and the full tick
	// cannot count them later: it replaces a target it finds dead before it
	// reaches the attack block, so the credit there only ever sees a live
	// monster. Without this line the battle horse trial counted roughly every
	// other kill.
	NotePlayerBotBattleHorseKill(ch, state, quickTarget);
}

#if defined(PLAYERBOT_ENGINE_MT2009)
// ---------------------------------------------------------------------------
// The two channels with moves (playerbot_channel_rules.h).
//
// SIZOWSKI's design and most of his code (metin2-ch2-dual-channel, sent on 18
// September and run on his world of 1500 bots since), fitted to this world's
// switch and slider: with the second channel on, a bot's channel is its row
// of common.playerbot_channel_assignment - one row a pid, so one channel a
// pid - and a bot on the second channel with business at a shop asks to be
// moved to the shop channel (RequestShopChannel). The coordinator runs on the
// shop channel's core that hosts Joan: every five seconds a census of the
// bots that play and of who waits, and at most once a gate a step - waiting
// bots straight in while the shop channel is under its cap, one for one
// against free bots of the shop channel at the cap (and a few more out than
// in over it), and with nobody waiting the shop channel eased back to its
// target. Nothing moves until both channels have started their bots. What the operator's slider says
// is the cap and the target (ShopChannelCapPercent), so the slider's 40 is
// his 60 and 50.
//
// A move is a row changed and nothing else. The old core reads the change
// within a refresh and logs the bot out (Despawn saves it like any logout);
// the new one spawns it once the row's ready time has passed - and Spawn's
// P2P test refuses it while the old core still holds it - so the bot is never
// on two cores at once. That was the whole objection to his first patch,
// which swapped bots through the database without a ready time.
//
// Every statement runs on m_pChannelSql, a connection and thread of this
// manager's own: the game thread queues a statement and collects the answer
// on a later tick, and never waits for the database.
// ---------------------------------------------------------------------------
namespace
{
	enum EPlayerBotChannelSql
	{
		PB_CHSQL_ASSIGNMENTS = 1,	// the whole assignment table, read back
		PB_CHSQL_CENSUS,			// the coordinator: the gate, the split, who waits
		PB_CHSQL_PROMOTE,			// waiting bots move straight to the shop channel
		PB_CHSQL_SWAP_OUT,			// free bots of the shop channel step aside...
		PB_CHSQL_SWAP_IN,			// ...and the waiting bots take their places
		PB_CHSQL_DRAIN				// nobody waiting: the shop channel eases back
	};

	struct TPlayerBotChannelSql
	{
		int iKind;
		unsigned int uA, uB, uC, uD;
	};

	// A bot this world counts as playing: its core reported it within the
	// last half minute (both channels report their own).
	std::string PlayerBotChannelSeen()
	{
		return "last_seen>DATE_SUB(NOW(),INTERVAL " +
				std::to_string(PLAYERBOT_CHANNEL_SEEN_SECONDS) + " SECOND)";
	}

	// A request that counts: it has stood long enough, the bot is still asking
	// (every ask refreshes updated_at), and the bot is still playing - a bot
	// that has logged out since (the life schedule's rest) is not moved in.
	std::string PlayerBotChannelRequestReady()
	{
		return "request_at<DATE_SUB(NOW(),INTERVAL " +
				std::to_string(PLAYERBOT_CHANNEL_REQUEST_STABLE_SECONDS) +
				" SECOND) AND updated_at>DATE_SUB(NOW(),INTERVAL " +
				std::to_string(PLAYERBOT_CHANNEL_REQUEST_EXPIRE_SECONDS) + " SECOND) AND " +
				PlayerBotChannelSeen();
	}

	// Who steps aside: bots of the shop channel that play, cost no more than
	// maxCost (MoveCost: a village, an errand and a live stand all cost) and
	// are past the cooldown; the cheapest interruption first, then at random.
	// Easing back takes only a bot with no live stand (cost 0 or 1): an owner
	// sent away would ask to come back for its next service, and the two
	// would chase each other.
	std::string PlayerBotChannelSwapOutQuery(unsigned int want, int maxCost)
	{
		const std::string shop = std::to_string(playerbot_channel_rules::SHOP_CHANNEL);
		const std::string other = std::to_string(3 - playerbot_channel_rules::SHOP_CHANNEL);
		return "UPDATE common.playerbot_channel_assignment SET channel=" + other +
				",requested_channel=0,request_reason='',request_at=NULL,"
				"ready_at=DATE_ADD(NOW(),INTERVAL " + std::to_string(PLAYERBOT_CHANNEL_READY_OUT_SECONDS) +
				" SECOND),moved_at=NOW(),updated_at=NOW() "
				"WHERE channel=" + shop + " AND shop_busy<=" + std::to_string(maxCost) +
				" AND requested_channel=0 AND " + PlayerBotChannelSeen() +
				" AND (moved_at IS NULL OR moved_at<DATE_SUB(NOW(),INTERVAL " +
				std::to_string(PLAYERBOT_CHANNEL_MOVE_COOLDOWN_SECONDS) + " SECOND)) "
				"ORDER BY shop_busy,CRC32(CONCAT(pid,UNIX_TIMESTAMP())) LIMIT " + std::to_string(want);
	}

	// The waiting bots, longest waiting first, onto the shop channel.
	std::string PlayerBotChannelMoveInQuery(unsigned int want, unsigned int readySeconds)
	{
		const std::string shop = std::to_string(playerbot_channel_rules::SHOP_CHANNEL);
		return "UPDATE common.playerbot_channel_assignment SET channel=" + shop +
				",requested_channel=0,request_reason='',request_at=NULL,"
				"ready_at=DATE_ADD(NOW(),INTERVAL " + std::to_string(readySeconds) +
				" SECOND),moved_at=NOW(),updated_at=NOW() "
				"WHERE channel<>" + shop + " AND requested_channel=" + shop +
				" AND " + PlayerBotChannelRequestReady() +
				" ORDER BY request_at,pid LIMIT " + std::to_string(want);
	}

	const char* PLAYERBOT_CHANNEL_GATE_STAMP =
			"UPDATE common.playerbot_channel_control SET last_batch=NOW() WHERE id=1";
}

// Defined in config.cpp (playerbotify.py), which holds the common database's
// credentials: they are locals of the config reader and nothing else keeps them.
bool PlayerBotOpenChannelConnection(CAsyncSQL* pkDest);

void CPlayerBotManager::RunChannelMachinery(DWORD dwNow)
{
	if (!m_bChannelTable)
		return;
	ProcessChannelSql();
	SeedChannelAssignments();
	PublishChannelPresence(dwNow);
	CoordinateChannelSwaps(dwNow);
	RefreshChannelAssignments(dwNow);
	FlushChannelRequests(dwNow);
	SpawnChannelArrivals(dwNow);
}

void CPlayerBotManager::ChannelClockTick(DWORD dwNow)
{
	RunChannelMachinery(dwNow);
	SpawnPendingBatch(dwNow);
}

// Connects on first use. The connection is made by its own thread, so this
// says "not yet" until that has happened and the game thread carries on.
bool CPlayerBotManager::EnsureChannelSql()
{
	if (m_pChannelSql)
		return m_pChannelSql->IsConnected();
	if (m_bChannelSqlFailed || !AccountDB::instance().IsConnected())
		return false;
	CAsyncSQL* pSql = new CAsyncSQL;
	if (!PlayerBotOpenChannelConnection(pSql))
	{
		delete pSql;
		m_bChannelSqlFailed = true;
		sys_err("PLAYERBOT_CHANNEL: cannot start the channel connection; the channels stay as they are");
		return false;
	}
	m_pChannelSql = pSql;
	sys_log(0, "PLAYERBOT_CHANNEL: channel %u database connection started", (unsigned int)g_bChannel);
	return false;
}

void CPlayerBotManager::SendChannelSql(int iKind, unsigned int uA, unsigned int uB, unsigned int uC,
		unsigned int uD, const std::string& strQuery)
{
	TPlayerBotChannelSql* pCtx = new TPlayerBotChannelSql;
	pCtx->iKind = iKind;
	pCtx->uA = uA;
	pCtx->uB = uB;
	pCtx->uC = uC;
	pCtx->uD = uD;
	m_pChannelSql->ReturnQuery(strQuery.c_str(), pCtx);
}

// Collects whatever the database thread has finished. Called every tick; an
// empty queue costs one mutex lock.
void CPlayerBotManager::ProcessChannelSql()
{
	if (!m_pChannelSql)
		return;
	SQLMsg* pMsg = NULL;
	while (m_pChannelSql->PopResult(&pMsg))
	{
		TPlayerBotChannelSql* pCtx = static_cast<TPlayerBotChannelSql*>(pMsg->pvUserData);
		const int iKind = pCtx ? pCtx->iKind : 0;
		if (pMsg->uiSQLErrno != 0)
			sys_err("PLAYERBOT_CHANNEL: statement %d failed (errno %u); retried on the next cycle",
					iKind, pMsg->uiSQLErrno);
		switch (iKind)
		{
			case PB_CHSQL_ASSIGNMENTS:
				OnChannelAssignments(pMsg);
				break;
			case PB_CHSQL_CENSUS:
				OnChannelCensus(pMsg);
				break;
			case PB_CHSQL_PROMOTE:
			case PB_CHSQL_SWAP_OUT:
			case PB_CHSQL_SWAP_IN:
			case PB_CHSQL_DRAIN:
				OnChannelSwapStep(pMsg);
				break;
			default:
				break;
		}
		delete pCtx;
		delete pMsg;
	}
}

// A row for every identity that has none, with the channel every core already
// gives it (the spread): once, from the coordinator. INSERT IGNORE, so a row a
// move has changed stays as it is.
void CPlayerBotManager::SeedChannelAssignments()
{
	if (m_bChannelSeeded || g_bChannel != playerbot_channel_rules::SHOP_CHANNEL ||
			!map_allow_find(playerbot_empire_rules::GetHomeMap(playerbot_empire_rules::EMPIRE_CHUNJO,
					playerbot_empire_rules::MAP_ROLE_M1)))
		return;
	if (!EnsureChannelSql())
		return;
	m_bChannelSeeded = true;
	std::vector<std::pair<DWORD, BYTE> > rows;
	rows.reserve(m_mapBotAccounts.size());
	for (TPlayerBotAccountMap::const_iterator it = m_mapBotAccounts.begin(); it != m_mapBotAccounts.end(); ++it)
		rows.push_back(std::make_pair(it->first, it->second.bChannel));
	for (size_t off = 0; off < rows.size(); off += PLAYERBOT_CHANNEL_CHUNK)
	{
		const size_t end = std::min(rows.size(), off + PLAYERBOT_CHANNEL_CHUNK);
		std::string q = "INSERT IGNORE INTO common.playerbot_channel_assignment (pid,channel,active) VALUES ";
		for (size_t i = off; i < end; ++i)
		{
			if (i > off)
				q += ",";
			q += "(" + std::to_string(rows[i].first) + "," + std::to_string((unsigned int)rows[i].second) + ",1)";
		}
		m_pChannelSql->AsyncQuery(q.c_str());
	}
	// A request is a bot's business in the world that is running, and a bot
	// the start has spawned afresh asks again if it still has any; requests
	// left by the last run would otherwise move a few hundred bots for errands
	// nobody has any more, the moment the warm-up ends.
	m_pChannelSql->AsyncQuery("UPDATE common.playerbot_channel_assignment "
			"SET requested_channel=0,request_reason='',request_at=NULL WHERE requested_channel<>0");
	sys_log(0, "PLAYERBOT_CHANNEL: assignment rows seeded for %u identities", (unsigned int)rows.size());
}

// Where every bot of this core is and what moving it would cost: one
// statement for the lot (a CASE per column, a few hundred pids a statement),
// sent without waiting for the answer. Both channels report, because the
// census counts the bots that play on both; only the shop channel's cost is
// ever read.
void CPlayerBotManager::PublishChannelPresence(DWORD dwNow)
{
	if (m_dwNextChannelPresenceTime && dwNow < m_dwNextChannelPresenceTime)
		return;
	m_dwNextChannelPresenceTime = dwNow + PLAYERBOT_CHANNEL_PRESENCE_INTERVAL;
	if (m_mapBots.empty() || !EnsureChannelSql() || m_pChannelSql->CountQuery() > PLAYERBOT_CHANNEL_MAX_QUEUED)
		return;

	std::vector<DWORD> vecPid;
	std::vector<long> vecMap;
	std::vector<int> vecCost;
	vecPid.reserve(m_mapBots.size());
	vecMap.reserve(m_mapBots.size());
	vecCost.reserve(m_mapBots.size());
	for (TPlayerBotMap::const_iterator i = m_mapBots.begin(); i != m_mapBots.end(); ++i)
	{
		LPCHARACTER ch = i->second ? i->second->GetCharacter() : NULL;
		if (!ch || !i->second->IsPhase(PHASE_GAME))
			continue;
		const DWORD pid = i->first;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
		const TPlayerBotAIState* state = st != s_mapPlayerBotAIStates.end() ? &st->second : NULL;
		// Anything under way but standing about - a fight, a trip, a town
		// visit, a purchase on its way - is busy; idling and resting in town
		// are not.
		bool busy = ch->GetVictim() ||
				(state && ((state->bCurrentAction != BOT_ACTION_IDLE &&
						state->bCurrentAction != BOT_ACTION_TOWN_REST) ||
					state->bFishingSession || state->bTownVisitPhase != BOT_TOWN_PHASE_NONE ||
					state->bMarketToJoan)) ||
				IsPlayerBotMiningNow(pid, dwNow);
		bool liveStand = false;
		// The medal droppers' cohort is the first channel's alone
		// (SpawnMedalDropperCohort): the second channel's core does not know
		// it, so a dropper moved there would become an ordinary bot and the
		// top-up here would never bring it back.
		bool pinned = IsMedalDropperCohortPID(pid) || ch->GetMyShop() != NULL ||
				(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
				ch->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ||
				playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0 ||
				(state && (IsPlayerBotOnTowerBusiness(ch, *state) || state->dwGuildWarEnemyGID != 0));
#if defined(ENABLE_IKASHOP_RENEWAL)
		auto stand = ikashop::GetManager().GetShopByOwnerID(pid);
		liveStand = stand && stand->GetDuration() != 0;
		// A shop operation in flight, and a service visit with the board open.
		pinned = pinned || playerbot_offline::requests.count(pid) != 0 ||
				(state && state->offlineShop.visiting);
		busy = busy || (state && state->offlineShop.buyOwner != 0);
#endif
		vecPid.push_back(pid);
		vecMap.push_back(ch->GetMapIndex());
		vecCost.push_back(playerbot_channel_rules::MoveCost(busy, liveStand, pinned,
				IsPlayerBotVillageMap(ch->GetMapIndex())));
	}

	for (size_t off = 0; off < vecPid.size(); off += PLAYERBOT_CHANNEL_CHUNK)
	{
		const size_t end = std::min(vecPid.size(), off + PLAYERBOT_CHANNEL_CHUNK);
		std::string q;
		q.reserve((end - off) * 64 + 256);
		q += "UPDATE common.playerbot_channel_assignment SET last_map=CASE pid ";
		for (size_t i = off; i < end; ++i)
			q += "WHEN " + std::to_string(vecPid[i]) + " THEN " + std::to_string(vecMap[i]) + " ";
		q += "END, shop_busy=CASE pid ";
		for (size_t i = off; i < end; ++i)
			q += "WHEN " + std::to_string(vecPid[i]) + " THEN " + std::to_string(vecCost[i]) + " ";
		q += "END, last_seen=NOW() WHERE channel=" + std::to_string((unsigned int)g_bChannel) + " AND pid IN (";
		for (size_t i = off; i < end; ++i)
		{
			if (i > off)
				q += ",";
			q += std::to_string(vecPid[i]);
		}
		q += ")";
		m_pChannelSql->AsyncQuery(q.c_str());
	}
}

bool CPlayerBotManager::RequestShopChannel(DWORD dwPlayerID)
{
	if (!m_bChannelTable || g_bChannel == playerbot_channel_rules::SHOP_CHANNEL)
		return false;
	TPlayerBotAccountMap::const_iterator it = m_mapBotAccounts.find(dwPlayerID);
	if (it == m_mapBotAccounts.end() || it->second.bChannel == playerbot_channel_rules::SHOP_CHANNEL)
		return false;
	m_setChannelRequests.insert(dwPlayerID);
	return true;
}

// The requests collected since the last flush, as one statement. The request
// time is kept when the bot already waits for the same move, so the stability
// clock does not restart at every ask.
void CPlayerBotManager::FlushChannelRequests(DWORD dwNow)
{
	if (m_setChannelRequests.empty())
		return;
	if (m_dwNextChannelFlushTime && dwNow < m_dwNextChannelFlushTime)
		return;
	if (!EnsureChannelSql() || m_pChannelSql->CountQuery() > PLAYERBOT_CHANNEL_MAX_QUEUED)
		return;
	m_dwNextChannelFlushTime = dwNow + PLAYERBOT_CHANNEL_FLUSH_INTERVAL;

	const std::string shop = std::to_string(playerbot_channel_rules::SHOP_CHANNEL);
	std::vector<DWORD> vecPid(m_setChannelRequests.begin(), m_setChannelRequests.end());
	m_setChannelRequests.clear();
	for (size_t off = 0; off < vecPid.size(); off += PLAYERBOT_CHANNEL_CHUNK)
	{
		const size_t end = std::min(vecPid.size(), off + PLAYERBOT_CHANNEL_CHUNK);
		std::string q = "UPDATE common.playerbot_channel_assignment "
				"SET request_at=IF(requested_channel=" + shop +
				" AND request_reason='shop' AND request_at IS NOT NULL,request_at,NOW()),"
				"requested_channel=" + shop + ",request_reason='shop',updated_at=NOW() "
				"WHERE channel<>" + shop + " AND pid IN (";
		for (size_t i = off; i < end; ++i)
		{
			if (i > off)
				q += ",";
			q += std::to_string(vecPid[i]);
		}
		q += ")";
		m_pChannelSql->AsyncQuery(q.c_str());
	}
}

// The coordinator: the shop channel's core that hosts Joan, one in the world.
// A census, then - step by step, each on the answer to the last - the moves.
void CPlayerBotManager::CoordinateChannelSwaps(DWORD dwNow)
{
	if (m_bChannelCoordInFlight || g_bChannel != playerbot_channel_rules::SHOP_CHANNEL ||
			!map_allow_find(playerbot_empire_rules::GetHomeMap(playerbot_empire_rules::EMPIRE_CHUNJO,
					playerbot_empire_rules::MAP_ROLE_M1)))
		return;
	if (m_dwNextChannelCoordinatorTime && dwNow < m_dwNextChannelCoordinatorTime)
		return;
	// The warm-up is measured on every call, because the bootstrap may set the
	// spawn window after the first tick of the machinery.
	const DWORD dwWarmUp = std::max<DWORD>(PLAYERBOT_CHANNEL_WARMUP_MIN,
			m_dwSpawnWindowMs + PLAYERBOT_CHANNEL_WARMUP_AFTER_WINDOW);
	if (!m_dwChannelCoordinatorSince)
	{
		m_dwChannelCoordinatorSince = dwNow ? dwNow : 1;
		sys_log(0, "PLAYERBOT_CHANNEL: coordinator waits %u s for both channels to start their bots",
				(unsigned int)(dwWarmUp / 1000));
	}
	if (dwNow - m_dwChannelCoordinatorSince < dwWarmUp)
		return;
	if (!EnsureChannelSql())
		return;
	m_dwNextChannelCoordinatorTime = dwNow + PLAYERBOT_CHANNEL_COORDINATOR_INTERVAL;
	m_bChannelCoordInFlight = true;
	const std::string shop = std::to_string(playerbot_channel_rules::SHOP_CHANNEL);
	SendChannelSql(PB_CHSQL_CENSUS, 0, 0, 0, 0,
			"SELECT COALESCE((SELECT TIMESTAMPDIFF(SECOND,last_batch,NOW()) "
			"FROM common.playerbot_channel_control WHERE id=1),999999),"
			"COALESCE(SUM(" + PlayerBotChannelSeen() + "),0),"
			"COALESCE(SUM(channel=" + shop + " AND " + PlayerBotChannelSeen() + "),0),"
			"COALESCE(SUM(channel<>" + shop + " AND requested_channel=" + shop +
			" AND " + PlayerBotChannelRequestReady() + "),0) "
			"FROM common.playerbot_channel_assignment");
}

void CPlayerBotManager::OnChannelCensus(void* pvMsg)
{
	SQLMsg* pMsg = static_cast<SQLMsg*>(pvMsg);
	MYSQL_ROW r = NULL;
	if (pMsg->uiSQLErrno == 0 && pMsg->Get() && pMsg->Get()->pSQLResult)
		r = mysql_fetch_row(pMsg->Get()->pSQLResult);
	unsigned int gateAge = 0, total = 0, onShopChannel = 0, waiting = 0;
	if (r)
	{
		if (r[0]) str_to_number(gateAge, r[0]);
		if (r[1]) str_to_number(total, r[1]);
		if (r[2]) str_to_number(onShopChannel, r[2]);
		if (r[3]) str_to_number(waiting, r[3]);
	}
	if (!r || total == 0 || gateAge < PLAYERBOT_CHANNEL_BATCH_GATE_SECONDS)
	{
		m_bChannelCoordInFlight = false;
		return;
	}

	const int capPercent = playerbot_channel_rules::ShopChannelCapPercent(m_iSecondChannelShare);
	const int targetPercent = playerbot_channel_rules::ShopChannelTargetPercent(m_iSecondChannelShare);
	const playerbot_channel_rules::TChannelMovePlan plan = playerbot_channel_rules::PlanChannelMoves(
			total, onShopChannel, waiting, capPercent, targetPercent);
	const unsigned int cap = total * (unsigned int)capPercent / 100U;
	unsigned int batch = (total * playerbot_channel_rules::MOVE_BATCH_PERCENT + 99U) / 100U;
	if (batch < 1)
		batch = 1;
	switch (plan.kind)
	{
		case playerbot_channel_rules::MOVE_DRAIN:
			// Easing back from the cap to the target takes only bots with no
			// live stand; over the cap is the slider not being kept, and then
			// anybody not pinned goes, the cheapest first. With most bots
			// behind a stand, the gentle drain found two bots in 686.
			SendChannelSql(PB_CHSQL_DRAIN, 0, plan.count, 0, 0, PlayerBotChannelSwapOutQuery(plan.count,
					plan.overCap ? playerbot_channel_rules::MOVE_COST_PINNED - 1 : 1));
			return;
		case playerbot_channel_rules::MOVE_PROMOTE:
			SendChannelSql(PB_CHSQL_PROMOTE, waiting - plan.count, cap, onShopChannel, batch,
					PlayerBotChannelMoveInQuery(plan.count, PLAYERBOT_CHANNEL_READY_OUT_SECONDS));
			return;
		case playerbot_channel_rules::MOVE_SWAP:
			SendChannelSql(PB_CHSQL_SWAP_OUT, 0, plan.count + plan.extraOut, 0, plan.count,
					PlayerBotChannelSwapOutQuery(plan.count + plan.extraOut,
							playerbot_channel_rules::MOVE_COST_PINNED - 1));
			return;
		default:
			m_bChannelCoordInFlight = false;
			return;
	}
}

// One step of a move finished. PROMOTE: uA waiting bots left, uB the cap,
// uC the shop channel's count before, uD the swap batch. SWAP_OUT: uC how
// many a promotion before it moved, uD how many may take the places of those
// who stepped out (0: all of them; fewer when the shop channel is over its
// cap). SWAP_IN: uA how many stepped out.
void CPlayerBotManager::OnChannelSwapStep(void* pvMsg)
{
	SQLMsg* pMsg = static_cast<SQLMsg*>(pvMsg);
	TPlayerBotChannelSql* pCtx = static_cast<TPlayerBotChannelSql*>(pMsg->pvUserData);
	const unsigned int moved = (pMsg->uiSQLErrno == 0 && pMsg->Get()) ? (unsigned int)pMsg->Get()->uiAffectedRows : 0;
	const unsigned int shop = (unsigned int)playerbot_channel_rules::SHOP_CHANNEL;

	if (pCtx->iKind == PB_CHSQL_DRAIN)
	{
		if (moved > 0)
		{
			sys_log(0, "PLAYERBOT_CHANNEL: %u bots eased from channel %u to keep room", moved, shop);
			m_pChannelSql->AsyncQuery(PLAYERBOT_CHANNEL_GATE_STAMP);
		}
		m_bChannelCoordInFlight = false;
		return;
	}

	if (pCtx->iKind == PB_CHSQL_PROMOTE)
	{
		sys_log(0, "PLAYERBOT_CHANNEL: %u bots moved to channel %u", moved, shop);
		// Some may still wait past the room there was: trade places for those.
		if (pCtx->uA > 0 && pCtx->uC + moved >= pCtx->uB)
		{
			const unsigned int want = std::min(pCtx->uA, pCtx->uD);
			SendChannelSql(PB_CHSQL_SWAP_OUT, 0, want, moved, 0,
					PlayerBotChannelSwapOutQuery(want, playerbot_channel_rules::MOVE_COST_PINNED - 1));
			return;
		}
		if (moved > 0)
			m_pChannelSql->AsyncQuery(PLAYERBOT_CHANNEL_GATE_STAMP);
		m_bChannelCoordInFlight = false;
		return;
	}

	if (pCtx->iKind == PB_CHSQL_SWAP_OUT)
	{
		// As many waiting bots as stepped aside take their places, the longest
		// waiting first - fewer when the swap was also easing the shop channel
		// back under its cap.
		if (moved > 0)
		{
			const unsigned int in = pCtx->uD ? std::min(moved, pCtx->uD) : moved;
			SendChannelSql(PB_CHSQL_SWAP_IN, moved, 0, 0, 0,
					PlayerBotChannelMoveInQuery(in, PLAYERBOT_CHANNEL_READY_IN_SECONDS));
			return;
		}
		// Nobody free right now; the next gate tries again.
		if (pCtx->uC > 0)
			m_pChannelSql->AsyncQuery(PLAYERBOT_CHANNEL_GATE_STAMP);
		m_bChannelCoordInFlight = false;
		return;
	}

	sys_log(0, "PLAYERBOT_CHANNEL: shop swap target=%u outgoing=%u incoming=%u", shop, pCtx->uA, moved);
	m_pChannelSql->AsyncQuery(PLAYERBOT_CHANNEL_GATE_STAMP);
	m_bChannelCoordInFlight = false;
}

void CPlayerBotManager::RefreshChannelAssignments(DWORD dwNow)
{
	if (m_bChannelRefreshInFlight)
		return;
	if (m_dwNextChannelRefreshTime && dwNow < m_dwNextChannelRefreshTime)
		return;
	if (!EnsureChannelSql())
		return;
	m_dwNextChannelRefreshTime = dwNow + PLAYERBOT_CHANNEL_REFRESH_INTERVAL;
	m_bChannelRefreshInFlight = true;
	SendChannelSql(PB_CHSQL_ASSIGNMENTS, 0, 0, 0, 0,
			"SELECT pid,channel,COALESCE(GREATEST(0,TIMESTAMPDIFF(SECOND,NOW(),ready_at)),0) "
			"FROM common.playerbot_channel_assignment");
}

// The table as the database thread read it. A bot now assigned elsewhere
// leaves this core (Despawn saves it like any logout); a bot newly assigned
// here is spawned once its ready time comes (SpawnChannelArrivals). An error
// or an empty answer changes nothing: a database that will not answer must
// not empty the world.
void CPlayerBotManager::OnChannelAssignments(void* pvMsg)
{
	SQLMsg* pMsg = static_cast<SQLMsg*>(pvMsg);
	m_bChannelRefreshInFlight = false;
	if (pMsg->uiSQLErrno != 0 || !pMsg->Get() || !pMsg->Get()->pSQLResult || pMsg->Get()->uiNumRows == 0)
		return;

	const DWORD dwUnixNow = (DWORD)get_global_time();
	MYSQL_ROW r;
	while (NULL != (r = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		unsigned int channel = 0, readyIn = 0;
		if (r[0]) str_to_number(pid, r[0]);
		if (r[1]) str_to_number(channel, r[1]);
		if (r[2]) str_to_number(readyIn, r[2]);
		if (channel != 1 && channel != 2)
			continue;
		TPlayerBotAccountMap::iterator x = m_mapBotAccounts.find(pid);
		if (x == m_mapBotAccounts.end())
			continue;
		const bool wasHere = x->second.bChannel == g_bChannel;
		x->second.bChannel = (BYTE)channel;
		x->second.dwReadyAt = readyIn ? dwUnixNow + readyIn : 0;
		const bool isHere = x->second.bChannel == g_bChannel;
		if (isHere && !wasHere)
			m_setChannelArrivals.insert(pid);
		else if (!isHere)
			m_setChannelArrivals.erase(pid);
	}

	std::vector<DWORD> leave;
	for (TPlayerBotMap::const_iterator i = m_mapBots.begin(); i != m_mapBots.end(); ++i)
	{
		TPlayerBotAccountMap::const_iterator a = m_mapBotAccounts.find(i->first);
		if (a != m_mapBotAccounts.end() && a->second.bChannel != g_bChannel)
			leave.push_back(i->first);
	}
	for (size_t i = 0; i < leave.size(); ++i)
	{
		sys_log(0, "PLAYERBOT_CHANNEL: pid=%u moved to channel %u, logging out here",
				leave[i], (unsigned int)m_mapBotAccounts[leave[i]].bChannel);
		Despawn(leave[i]);
	}
	// Nobody moved away is this channel's to start or to top up any more.
	for (TRegisteredPlayerBotSet::iterator i = m_setRegisteredBots.begin(); i != m_setRegisteredBots.end();)
	{
		TPlayerBotAccountMap::const_iterator a = m_mapBotAccounts.find(*i);
		if (a != m_mapBotAccounts.end() && a->second.bChannel != g_bChannel)
			m_setRegisteredBots.erase(i++);
		else
			++i;
	}
	for (std::set<DWORD>::iterator i = m_setScheduledBots.begin(); i != m_setScheduledBots.end();)
	{
		TPlayerBotAccountMap::const_iterator a = m_mapBotAccounts.find(*i);
		if (a != m_mapBotAccounts.end() && a->second.bChannel != g_bChannel)
			m_setScheduledBots.erase(i++);
		else
			++i;
	}
}

// Bots moved to this channel while it runs: spawned once their ready time has
// passed, and only on the core that hosts their kingdom's first village - the
// same rule the start-up spawn follows.
void CPlayerBotManager::SpawnChannelArrivals(DWORD)
{
	if (m_setChannelArrivals.empty())
		return;
	const DWORD dwUnixNow = (DWORD)get_global_time();
	for (std::set<DWORD>::iterator i = m_setChannelArrivals.begin(); i != m_setChannelArrivals.end();)
	{
		TPlayerBotAccountMap::const_iterator a = m_mapBotAccounts.find(*i);
		if (a == m_mapBotAccounts.end() || a->second.bChannel != g_bChannel)
		{
			m_setChannelArrivals.erase(i++);
			continue;
		}
		const long lVillage = playerbot_empire_rules::GetHomeMap(
				(int)a->second.bEmpire, playerbot_empire_rules::MAP_ROLE_M1);
		if (lVillage == 0 || !map_allow_find(lVillage))
		{
			m_setChannelArrivals.erase(i++);
			continue;
		}
		if (a->second.dwReadyAt > dwUnixNow)
		{
			++i;
			continue;
		}
		m_setRegisteredBots.insert(*i);
		if (m_setScheduledBots.insert(*i).second)
			m_dequePendingSpawns.push_back(*i);
		sys_log(0, "PLAYERBOT_CHANNEL: pid=%u arrives on channel %u", *i, (unsigned int)g_bChannel);
		if (g_bChannel == playerbot_channel_rules::SHOP_CHANNEL)
			m_setChannelMovedIn.insert(*i);
		m_setChannelArrivals.erase(i++);
	}
	// The batch size is set by the start-up spawn; a channel that started with
	// nobody has none yet.
	if (!m_dequePendingSpawns.empty())
		m_uSpawnBatchSize = std::max<size_t>(1, m_uSpawnBatchSize);
}
#endif

void CPlayerBotManager::Update()
{
	const DWORD dwNow = get_dword_time();
	const DWORD dwTickStartUs = PlayerBotClockUs();

	// Whispers waiting for an answer, a bot's own opening line, and the
	// conversation memory's housekeeping (playerbot_chat_conversation.h).
	// Cheap when nobody is talking; while replies are pending the
	// conversation's own short timer does the precise timing.
	PumpPlayerBotConversation(dwNow);

#if defined(PLAYERBOT_ENGINE_MT2009)
	// The two channels with moves: what the database thread has answered,
	// and the next round queued. Nothing here waits for the database.
	RunChannelMachinery(dwNow);
#endif
	// The next batch of the cohort, if one is due - see PLAYERBOT_SPAWN_WINDOW.
	SpawnPendingBatch(dwNow);
	// The second cohort, one at a time over its hours (ScheduleLateJoiners).
	SpawnLateJoiners(dwNow);
	// And a minute apart, whoever is missing from it - and, on the same clock,
	// whoever a GM has banned is taken back out, and whoever the life schedule
	// logs out or lets back in (the LIFE switch) goes before the count.
	RefreshBannedBots(dwNow);
	ManageLifeSchedule(dwNow);
	TopUpMissingBots(dwNow);

	// Once for the whole population: the panel may have moved a weight since
	// the last tick, and every bot planned below must see the same numbers.
	RefreshPlayerBotWeights(dwNow);
	RefreshPlayerBotItemPolicy(dwNow);
	// The PERSONA switch moved: every bot goes back to the personality it
	// drew, or on to the character that draw leans to, on this tick - and so
	// do its ambition and, through ManagePlayerBotExpLock, its lock.
	{
		static int s_iPlayerBotPersonaSwitchSeen = -1;
		const int personaNow = IsPlayerBotPersonaEnabled() ? 1 : 0;
		if (s_iPlayerBotPersonaSwitchSeen != -1 && s_iPlayerBotPersonaSwitchSeen != personaNow)
		{
			unsigned int changed = 0;
			for (TPlayerBotMap::iterator it = m_mapBots.begin(); it != m_mapBots.end(); ++it)
			{
				LPCHARACTER c = it->second ? it->second->GetCharacter() : NULL;
				TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(it->first);
				if (!c || st == s_mapPlayerBotAIStates.end() || IsMedalDropperCohortPID(it->first))
					continue;
				TPlayerBotAIState& s = st->second;
				const BYTE want = personaNow ? GetPlayerBotCharakter(s.persona.bDrawnPersonality)
						: s.persona.bDrawnPersonality;
				if (s.bPersonality == want)
					continue;
				s.bPersonality = want;
				s.bAmbition = GetPlayerBotStableAmbition(c, want);
				++changed;
			}
			sys_log(0, "PLAYERBOT_PERSONA: switch %s, %u characters changed", personaNow ? "on" : "off", changed);
		}
		s_iPlayerBotPersonaSwitchSeen = personaNow;
	}
	BeginPlayerBotPersonaCensus(dwNow);
	ManagePlayerBotNight(dwNow);
	// The timed events: chest windows, rate windows, "activate now" - and the
	// notices that go with them (playerbot_events.h).
	ManagePlayerBotEvents(dwNow);
	// The guilds: the population's strength census and the tier floors, the
	// wars, and the guild report the panel reads (playerbot_guild.h,
	// playerbot_guild_war.h).
	RefreshPlayerBotStrengths(dwNow);
	ManagePlayerBotGuildWars(dwNow);
	ManagePlayerBotTowerRaids(dwNow);
	WritePlayerBotGuildStatus(dwNow);
	WritePlayerBotItemShopCensus(dwNow);
	// The ore veins, once a minute for the whole world. A vein deletes itself
	// after 7-15 minutes and nothing in this world's regen files puts one back -
	// there are no vein spawns on any of its maps at all - so the sites are
	// ours to keep standing.
	MaintainPlayerBotOreVeins(dwNow);

	// Once a minute: what the last minute cost. Read this before tuning any
	// budget - the first version of the material errand was diagnosed from CPU
	// alone and put the whole population's scans in one second.
	if (s_dwPlayerBotLoadReportTime == 0)
		s_dwPlayerBotLoadReportTime = dwNow;
	else if (dwNow - s_dwPlayerBotLoadReportTime >= PLAYERBOT_LOAD_REPORT_INTERVAL)
	{
		// The monsters and Metin stones standing on this core: the one number
		// that shows the /rates page's respawn multipliers at work
		// (m2_mob_count, m2_boss_count; regen.cpp's regen_target_count) - and
		// the NPCs, which they must leave alone: the ore veins and herbs of
		// stone.txt are NPCs spawned by groups of groups, and the first version
		// of the multiplier doubled them. A horse following a dismounted rider
		// is an NPC too (20030 and its kind), and those come and go with the
		// bots, so a horse with a rider is not counted. One walk over the VID
		// map a minute. mt2009 only - r40250's manager does not hand the map
		// out, and the multipliers are not there either.
		unsigned int standingMonsters = 0, standingStones = 0, standingNpcs = 0;
#if defined(PLAYERBOT_ENGINE_MT2009)
		for (const auto& entry : CHARACTER_MANAGER::instance().GetCharacterVIDMap())
		{
			LPCHARACTER other = entry.second;
			if (!other)
				continue;
			if (other->IsStone())
				++standingStones;
			else if (other->IsMonster())
				++standingMonsters;
			else if (other->GetCharType() == CHAR_TYPE_NPC && !other->GetRider())
				++standingNpcs;
		}
#endif
		sys_log(0, "PLAYERBOT_LOAD: bots=%u ticks=%u tick_ms=%u tick_max_ms=%u targets=%u misses=%u target_ms=%u snapshot_ms=%u plans=%u deferred=%u resumed=%u cached=%u plan_ms=%u p64=%u/%ums p256=%u/%ums p1024=%u/%ums pfar=%u/%ums scans=%u scan_ms=%u saves=%u watchdog=%u over=%ums sliced=%u light_ms=%u mobs=%u stones=%u npcs=%u",
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
				(unsigned int)(dwNow - s_dwPlayerBotLoadReportTime), s_uPlayerBotLoadSliced,
				s_uPlayerBotLoadLightUs / 1000,
				standingMonsters, standingStones, standingNpcs);
		for (int b = 0; b < 4; ++b)
			s_uPlayerBotLoadPlanBucket[b] = s_uPlayerBotLoadPlanBucketUs[b] = 0;
		s_uPlayerBotLoadPlanDeferred = s_uPlayerBotLoadPlanResumed = s_uPlayerBotLoadPlanCached = 0;
		s_uPlayerBotLoadSliced = s_uPlayerBotLoadLightUs = 0;
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

	// The tick proper, inside a time budget (PLAYERBOT_TICK_BUDGET_MS_DEFAULT,
	// TICK_MS): a pass that runs out of it leaves the pid it stopped at and
	// the next pass starts there, so every bot is still reached, only less
	// often, and the core's other work - a player's login above all - gets
	// its share of the thread. A sweep is one visit to every bot; the heavy and
	// light ticks below alternate by sweep, not by pass, or a bot reached every
	// other pass would land on the light one every time. The bots a pass does
	// not reach take their light tick after it (RunPlayerBotLightTick), and the
	// pass leaves room for that by what it cost the last time - never less than
	// a quarter of the budget for the full ticks.
	static DWORD s_dwPlayerBotSweep = 0;
	static DWORD s_dwPlayerBotResumePid = 0;
	static DWORD s_dwPlayerBotLightPassUs = 0;
	TPlayerBotMap::iterator itFirst = m_mapBots.begin();
	if (s_dwPlayerBotResumePid != 0)
		itFirst = m_mapBots.lower_bound(s_dwPlayerBotResumePid);
	else
		++s_dwPlayerBotSweep;
	s_dwPlayerBotResumePid = 0;
	const DWORD dwTickBudgetUs = (DWORD)GetPlayerBotTickBudgetMs() * 1000U;
	const DWORD dwFullTickBudgetUs =
			s_dwPlayerBotLightPassUs + dwTickBudgetUs / 4 < dwTickBudgetUs
			? dwTickBudgetUs - s_dwPlayerBotLightPassUs : dwTickBudgetUs / 4;
	unsigned int uPassBots = 0;
	TPlayerBotMap::iterator itStop = m_mapBots.end();
	for (TPlayerBotMap::iterator it = itFirst; it != m_mapBots.end(); ++it)
	{
		if (dwTickBudgetUs != 0 && uPassBots >= PLAYERBOT_TICK_MIN_BOTS &&
				PlayerBotClockUs() - dwTickStartUs > dwFullTickBudgetUs)
		{
			s_dwPlayerBotResumePid = it->first;
			itStop = it;
			++s_uPlayerBotLoadSliced;
			break;
		}
		++uPassBots;

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
		{
			ManagePlayerBotStatusOverhead(ch, state, dwNow);
#if defined(PLAYERBOT_ENGINE_MT2009)
			ManagePlayerBotPersonalityTitle(ch, state, dwNow);
#endif
		}
		// Every bot of the pass, before the light/full split halves them.
		NotePlayerBotPersonaCensus(state, dwNow);

		// Keep expensive decisions staggered over two ticks, but let an already
		// engaged bot continue its basic combo on the intervening tick.  This makes
		// combat look like holding Space without doubling pathfinding/target scans.
		if ((it->first + s_dwPlayerBotSweep) % 2 != 0)
		{
			RunPlayerBotLightTick(d, ch, state, dwNow);
			continue;
		}

		PersistPlayerBot(ch, state, dwNow);
		if (HandleDeath(ch, state, dwNow))
			continue;

		// Stunned is stunned, for a bot as much as for anybody.
		//
		// The engine puts AFFECT_STUN on a playerbot exactly as on a player -
		// battle.cpp's AttackAffect and the Charge branch of char_skill.cpp,
		// both through IMMUNE_STUN and neither of them asking whether the
		// descriptor IsBot - and CHARACTER::CanAttack refuses anyone whose
		// IsStun() is true. This tick simply never asked: the bot kept walking,
		// kept swinging and kept planning through the whole thing, so a player
		// who landed a Charge saw nothing happen at all ("bot po sekundzie juz
		// biegnie dalej", cyfrowy_mat on the Discord). Stop where it stands and
		// let the engine's own stun event be the thing that ends it.
		//
		// No watchdog exemption is needed with it: a stun is seconds and the
		// inactivity reset is ninety of them.
		if (ch->IsStun())
		{
			if (ch->IsStateMove())
				ch->Stop();
			continue;
		}

		if (!d->IsPhase(PHASE_GAME))
			continue;

		// The mood's clocks (playerbot_mood.h): its quest flags read once they
		// have arrived, the rotation, the drought, the end of a lock.
		AdvancePlayerBotMood(ch, state, dwNow);

#if defined(PLAYERBOT_ENGINE_MT2009)
		// A bot that asked to be moved to the shop channel for a stand waits in
		// town until the coordinator moves it: without the hold it starts a hunt
		// inside the request's stability window and is logged out in the middle
		// of the next fight. The timeout is for a coordinator or a database that
		// does not answer - the request stays queued and the bot goes back to
		// its life (EnsurePlayerBotPrivateShopChannel).
		if (state.bWaitingForShopChannel)
		{
			if (g_bChannel == playerbot_channel_rules::SHOP_CHANNEL)
			{
				state.bWaitingForShopChannel = false;
				state.dwShopChannelWaitStarted = 0;
				state.dwNextShopChannelRequestTime = 0;
			}
			else if (state.dwShopChannelWaitStarted != 0 &&
					dwNow - state.dwShopChannelWaitStarted >= PLAYERBOT_SHOP_CHANNEL_WAIT_TIMEOUT_MS)
			{
				state.bWaitingForShopChannel = false;
				state.dwShopChannelWaitStarted = 0;
				state.dwNextShopChannelRequestTime = 0;
				state.dwNextShopKeepTime = dwNow + number(300000, 600000);
				PlayerBotLogThrottled("shop_channel_timeout", dwNow,
						"PLAYERBOT_CHANNEL: pid=%u name=%s waited for the shop channel and goes back to its life",
						ch->GetPlayerID(), ch->GetName());
			}
			else
			{
				if (state.dwNextShopChannelRequestTime == 0 || dwNow >= state.dwNextShopChannelRequestTime)
				{
					RequestShopChannel(ch->GetPlayerID());
					state.dwNextShopChannelRequestTime = dwNow + PLAYERBOT_SHOP_CHANNEL_REQUEST_REFRESH_MS;
				}
				ch->SetVictim(NULL);
				ClearPlayerBotRoute(state, true);
				if (ch->IsStateMove())
					ch->Stop();
				SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
				state.dwLastMeaningfulActivityTime = dwNow;
				continue;
			}
		}
#endif

		// The duel the bot agreed to, ahead of every errand. A challenge is
		// answered within three seconds and then fought; a bot that walks off
		// to the blacksmith instead is what "bot zaakceptowal PvP ale mnie nie
		// bije" was.
		if (ManagePlayerBotDuelCombat(ch, state, dwNow))
			continue;

		// The guild war the bot's guild is in, ahead of every errand: the walk
		// to the kingdom's guild map and the fight there (playerbot_guild_war.h).
		if (ManagePlayerBotGuildWar(ch, state, dwNow))
			continue;

		// A player who struck the bot, or its party, or is breaking its stone
		// for another kingdom (playerbot_anti_pk.h): ahead of every errand,
		// the way a duel is - "natychmiast przerywa swoje dotychczasowe zajecie".
		if (ManagePlayerBotPersonaFoe(ch, state, dwNow))
			continue;

		// Before anything that can claim the tick. An open stall is engine state
		// with a deadline this manager owns, so releasing it must not depend on
		// which subsystem happens to win the tick - that dependency is why stalls
		// were left standing with their sign over the keeper's head.
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (ManagePlayerBotOfflineService(ch, state, dwNow))
			continue;
#endif
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

		// SLABY's stop from the keyboard, between two things and never in the
		// middle of one (playerbot_persona.h). Below the duel, the war and the
		// stand's service, which a player away from the keys would not answer
		// either - but those are a company's, and in company the bot plays
		// NORMALNY and never goes.
		if (ManagePlayerBotMoodAfk(ch, state, dwNow))
			continue;

		if (playerbot_empire_rules::IsKingdomMap(ch->GetMapIndex()) ||
				IsPlayerBotMonkeyMap(ch->GetMapIndex()) ||
				IsPlayerBotFrontierMap(ch->GetMapIndex()))
		{
			const long currentMap = ch->GetMapIndex();
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(
					currentMap);
			navigation.Init(currentMap);
			const bool bOutOfBounds = !navigation.IsInsideWorld(ch->GetX(), ch->GetY());
			const bool bCrossingJoanGate = HasPlayerBotTownGate(currentMap) &&
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
					// The village or guild map's own entry point, whichever
					// kingdom this is. GetPlayerBotHomePoint answers for all
					// twelve; the old code named Joan's square as the default
					// and would have dropped a Jinno bot into Chunjo.
					long fallbackX = 60600;
					long fallbackY = 170900;
					long fallbackMap = 0;
					if (playerbot_empire_rules::IsKingdomMap(currentMap))
						GetPlayerBotHomePoint(ch, currentMap, fallbackMap,
								fallbackX, fallbackY);
					else if (IsPlayerBotMonkeyMap(currentMap))
						GetPlayerBotMonkeyArrival(currentMap, fallbackX, fallbackY);
					else
						GetPlayerBotFrontierArrivalFor(ch, currentMap, fallbackX, fallbackY);
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
		// And, on the way into a dungeon, which room to hunt in - before the
		// target section can pin the bot to the entrance's handful of monkeys.
		if (ManagePlayerBotMonkeySpread(ch, state, dwNow))
			continue;

		if (s_bPlayerBotM2CensusPass)
			NotePlayerBotPartyCensus(ch, state);
		// The census, once a minute: why each level-40 bot in Bokjung is there.
		if (s_bPlayerBotM2CensusPass && ch->GetLevel() >= 40 &&
				IsPlayerBotM2Map(ch->GetMapIndex()))
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
		// The crafting recipes, on a clock of their own: a bot with skill
		// books in the bag never reaches the tail of the pass above.
		ManagePlayerBotCraftRecipes(ch, dwNow);
		ManagePlayerBotSoulStones(ch, state, dwNow);
		ManagePlayerBotGrandMasterTraining(ch, state, dwNow);
		// The vouchers cashed and the shop's goods bought (playerbot_itemshop.h).
		ManagePlayerBotItemShop(ch, state, dwNow);
		ManagePlayerBotZenBeans(ch, dwNow);
		ManagePlayerBotThirdHand(ch, state, dwNow);
		// Rings and gloves on the clock while the bot hunts and off in town,
		// and the uniques a bot never wears off for good.
		ManagePlayerBotUniqueSlots(ch, state, dwNow);
		// The gear pass, early. It used to sit at the bottom of the tick, past
		// the stall, the loot, the horse, the fishing, the travel, the town
		// visit and the wander, each of which claims the tick - so a bot that
		// was always doing one of them never looked at its bag: a warrior of
		// twenty-eight fought with the level-one sword at +6 (attack 60) with
		// a Long Sword +4 (82) in the bag, 234 of 970 bots the same way. Not
		// behind an open counter (the table points at cells), not during a
		// town visit (the blacksmith phase moves gear itself), not with a rod
		// in the hand, not at the stable.
		// ...and not with a pickaxe in it either. The first live run of the
		// mining pass logged one bot re-equipping its pickaxe every thirty-two
		// seconds - exactly the swing cadence - because this pass ran above it
		// and swapped a digging tool out for a sword between two swings. The
		// engine's mining_event asks GetWear(WEAR_WEAPON) for an ITEM_PICK on
		// the tick it fires, so every swing was refused and no ore ever
		// dropped: the same exemption a rod has, for the same reason.
		if (!ch->GetMyShop() && !state.bVisitingShop && !state.bFishingSession &&
				!IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow) &&
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
		// A dropper that has reached its band stops earning experience, and a
		// marble is spent on a boss. Both are cheap tests that end on the first
		// line for everybody they do not concern.
		ManagePlayerBotExpLock(ch, state);
		ManagePlayerBotPolymorph(ch, state, dwNow);
		ManagePlayerBotGuild(ch, state, dwNow);
		// Answered every tick and not on the party pass's own clock: the engine
		// gives an invitation ten seconds to live, and the party pass can be
		// three minutes away.
		AcceptPlayerBotPartyInvite(ch, state, dwNow);
		// A duel is answered on the same cadence and for the same reason: the
		// challenge is somebody else's move and the bot has to be the one that
		// answers it.
		AcceptPlayerBotPvpChallenge(ch, state, dwNow);
		ManagePlayerBotPvpChallenge(ch, state, dwNow);
		ManagePlayerBotKingdomHostility(ch, state, dwNow);
		ManagePlayerBotParty(ch, state, dwNow);
		// Iwakura's mercenary (playerbot_companions.h): a contract's upkeep for
		// either side, the way back to a client after a pause, the client
		// keeping up, and the walk to a bot in distress. Above every errand and
		// the world travel, which a running contract holds on its map; it
		// claims the tick only while it walks, and never in a fight.
		if (ManagePlayerBotMercenary(ch, state, dwNow))
			continue;
		// A bot in a player's party keeps its errands and runs none of them
		// while it is there. A Biologist or a merchant it had been walking to
		// took it away from the player, the follow pass fetched it back, and
		// the two took turns for as long as the party lasted ("[PT] Ide do
		// handlarza bronia (cel: Biolog)" - Pabloo, 15 September, whose fix
		// this is). The world travel has stood down since 2.0.48; the stable,
		// the Biologist, the town visit and the empty-handed recovery stand
		// down below. Fighting does not: a bot in a party is there to fight.
		const bool bHumanLedParty = IsPlayerBotHumanLedParty(ch->GetParty());
		// Whose bot this is, for the errands below. Three ways a bot belongs to
		// a person and only one of them is their party: a companion invites the
		// person into ITS OWN party, so the leader is a bot and every rule
		// written for "a person's party" was silently off for exactly the bots
		// people play with; a mercenary is under contract; and a standing lure
		// order is a job the person gave. That last one is what
		// "PLAYERBOT_LURE: waiting reason=town_visit" is - an order taken and
		// the bot walking to the blacksmith with it (l0st3k, 20 September).
		// IsPlayerBotHeldForCompany already knew the first two.
		const bool bServingPerson = bHumanLedParty || IsPlayerBotHeldForCompany(ch) ||
				state.dwLurePlayerPID != 0;
		// Keeping up with the player comes before the bot's own plans for the
		// tick, or the wander pass walks it out of the party it just joined.
		if (ManagePlayerBotFollowHumanLeader(ch, state, dwNow))
			continue;
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
		// Which of Iwakura's personalities claims the bot, and whether its
		// Grinder has met the Law of Advancement.
		ManagePlayerBotPersona(ch, state, dwNow);

		// Trigger Town Visit (Full inventory, out of potions, or missing weapon)
		// Only trigger when NOT in the middle of fighting an active Metin stone!
		LPCHARACTER curTarget = state.dwTargetVID != 0 ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (curTarget && curTarget->IsStone() && !curTarget->IsDead() &&
				!IsPlayerBotMetinWorthFighting(ch, curTarget) &&
				!IsPlayerBotStoneJoinable(ch, curTarget))
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

		// A boss that fell leaves its casket where it stood, and the raid's next
		// step is "boss down, going back to work": the killer walked off and the
		// Umarly Rozpruwacz's casket lay on the snow (Ciapek, 16 September). The
		// loot window a broken stone gets - PLAYERBOT_METIN_LOOT_DASH_TIME within
		// PLAYERBOT_METIN_LOOT_DASH_RANGE, whatever else is going on - is its.
		if (state.dwFightProgressVID != 0 && state.bFightProgressBoss &&
				(curTarget == NULL || curTarget->IsDead()) &&
				(state.dwStoneBrokenTime == 0 || dwNow - state.dwStoneBrokenTime > PLAYERBOT_METIN_LOOT_DASH_TIME))
		{
			state.dwStoneBrokenTime = dwNow;
			sys_log(0, "PLAYERBOT_LOOT: boss down, loot window pid=%u name=%s map=%ld",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex());
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

		// A negative rank keeps the bot inside its village's safe ring, ahead of
		// the loot, the errands, the travel and the fight below.
		if (!bServingPerson && !state.bMultiPullActive &&
				KeepPlayerBotNegativeRankInTown(ch, state, dwNow))
			continue;

		// Exactly one loot decision per full AI pass. HandleLoot performs a
		// non-blocking, throttled Z-style pickup in combat and returns false, while
		// peaceful loot may take ownership of this tick and walk to the drop.
		if (HandleLoot(ch, state, dwNow))
			continue;

		// The Demon Tower: a raider on its way to the ground floor, and every
		// bot inside an instance whatever brought it there
		// (playerbot_demon_tower.h). Owns the tick the way the guild war does,
		// after the loot so the floors' keys are picked up.
		if (ManagePlayerBotDemonTower(ch, state, dwNow))
			continue;

		// Horse medals are equally real resources: a bot leaves combat, walks to
		if (!bServingPerson && !state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotHorse(ch, state, dwNow))
			continue;

		// A handful of M1 bots fish the riverbank instead of grinding. This owns
		// the whole tick: the rod sits in the weapon slot, so combat and the gear
		// pass below must not run while a session is live.
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotFishing(ch, state, dwNow))
			continue;

		// And a smaller handful digs at the ore veins on the three frontier
		// maps. Owns the tick for the same reason fishing does: the pickaxe
		// sits in the weapon slot, so no combat or gear pass may run under it.
		if (!state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotMining(ch, state, dwNow))
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
		if (!bServingPerson && !state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotBiologist(ch, state, dwNow))
			continue;

		// Baek-Go stands in the same three villages, so his board is the same
		// kind of local errand as the hand-in above and is gated the same way.
		if (!bServingPerson && !state.bMultiPullActive && !bFightingMetin &&
				ManagePlayerBotHerbalist(ch, state, dwNow))
			continue;

		// Missing/progression gear starts the first visit immediately because the
		// shop timer is zero after login.  Once a visit finishes, however, respect
		// its 5-10 minute retry cooldown.  Otherwise a bot that cannot yet afford
		// the next tier loops forever between the weapon and armour merchants and
		// never returns to combat (or to its local party).
		const bool bOnTownMap = IsPlayerBotVillageMap(ch->GetMapIndex());
		if (!bServingPerson && bOnTownMap && !state.bVisitingShop && !state.bMultiPullActive &&
				!bFightingMetin &&
				(bNeedsProfession || dwNow > state.dwNextShopCheckTime))
		{
			size_t occupiedItems = 0;
			size_t occupiedGridCells = 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
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
			// Iwakura's Trader goes at eighty percent ("zapelni sie w co najmniej
			// 80%"): a break is for what the game makes the bot do, and a bag at
			// half is not that. No column of three is still a bag that cannot take
			// a weapon, so it still sends the bot.
			const bool personaOn = IsPlayerBotPersonaEnabled();
			const bool bInventoryFull = (personaOn ? IsPlayerBotBagFull(ch)
					: occupiedGridCells * 100 >= PLAYERBOT_BAG_CELLS * 45) ||
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
			// The gambler's session goes to the anvil as the Perfectionist's does
			// (playerbot_gambler.h).
			const bool bNeedsRefine = HasPlayerBotRefineOpportunity(ch) ||
					IsPlayerBotGambling(state, dwNow);
			const bool bNeedsGearUpgrade = bNeedsCoreGear || NeedsPlayerBotArrows(ch);
			// The Trader's merchant round comes with the eighty percent above,
			// not with a dozen pieces of junk.
			const bool bNeedsSellRun = !personaOn && CountPlayerBotJunkItems(ch) >= 12;
			const bool bNeedsPotionCleanup = HasPlayerBotExcessPotions(ch);

			if (bNeedsProfession || bInventoryFull || bNeedsPotions || bWeaponMissing ||
					bNeedsRefine || bNeedsGearUpgrade || bNeedsSellRun ||
					bNeedsPotionCleanup)
				StartPlayerBotTownVisit(ch, state, dwNow);
		}

		// A visit is an adaptive, persistent route. The bot only visits specialists
		// needed by its current inventory: weapon merchant, armor merchant, Misc
		// Merchant and/or blacksmith. Goals never change in the middle of a route.
		if (!bServingPerson && HandlePlayerBotTownVisit(ch, state, dwNow))
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
			// Every way out of this branch is an errand - a merchant, the world
			// travel, the scavenging wander - so in a player's party the bot
			// stays by the player without one, and a Shaman still buffs.
			if (bHumanLedParty)
			{
				ManagePlayerBotBuffHumanLeader(ch, state, dwNow);
				ch->Stop();
				continue;
			}
			if (state.dwEmergencyScavengeUntil != 0 &&
					dwNow < state.dwEmergencyScavengeUntil &&
					IsPlayerBotM1Map(ch->GetMapIndex()))
			{
				// HandleLoot above collects any ownerless nearby drop. Wander between
				// hunting hubs so the next scans cover new ground instead of idling at
				// the Weapon Merchant forever.
				SetPlayerBotGoal(ch, state, BOT_GOAL_GET_EQUIPMENT, dwNow);
				ManagePlayerBotWandering(ch, state, dwNow);
			}
			else if (IsPlayerBotVillageMap(ch->GetMapIndex()))
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
		// A player's Shaman buffs the player before itself.
		if (ManagePlayerBotBuffHumanLeader(ch, state, dwNow))
			continue;
		// And Iwakura's companion Shaman the rest of its party, persons first.
		if (ManagePlayerBotBuffCompanions(ch, state, dwNow))
			continue;
		// A crafted potion before the buffs: ten minutes of attack value or
		// defence, spent only on a boss or a Metin stone (playerbot_herbalism.h).
		if (DrinkPlayerBotCraftedPotion(ch, curTarget, dwNow))
			continue;
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
			long recoverMap = 0, recoverX = 0, recoverY = 0;
			if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2,
						recoverMap, recoverX, recoverY) &&
					TransitionPlayerBotMap(ch, state, recoverMap, recoverX, recoverY,
						dwNow, "half_warp_recovery"))
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
		// An agreed duel outranks whatever this bot was hunting, the party's
		// focus included: it is a commitment to another character, and it is
		// bounded by construction - PLAYERBOT_PVP_DUEL_ASSUMED, or the moment
		// one of the two falls. Without this the bot agreed and then went back
		// to its monsters, which is what a player sees as being ignored.
		LPCHARACTER duelFoe = FindPlayerBotDuelOpponent(ch, dwNow);
		// Only a foe the engine will let this bot strike - see
		// CanPlayerBotStrikeCharacter and the refusal clock in
		// ManagePlayerBotDuelCombat.
		if (duelFoe && !CanPlayerBotStrikeCharacter(ch, duelFoe))
			duelFoe = NULL;
		if (duelFoe && duelFoe != target &&
				!IsPlayerBotSafeZone(ch->GetMapIndex(), duelFoe->GetX(), duelFoe->GetY()))
		{
			target = duelFoe;
			state.dwTargetVID = (DWORD)target->GetVID();
			ClearPlayerBotRoute(state, true);
		}
		// Iwakura's stone hunter (playerbot_anti_pk.h): the stone in sight that
		// takes the bot off its monster, and the turn on the stone's pack below
		// 35%. A duel foe is the duel's.
		if (!(duelFoe && target == duelFoe))
			ManagePlayerBotPogromcaTarget(ch, state, target, dwNow);
		const bool bTargetIsDuelFoe = (target != NULL && target == duelFoe);
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

		if (!target || target->IsDead() ||
			(!bTargetIsMonster && !bTargetIsStone && !bTargetIsDuelFoe) ||
			(bTargetIsStone && !IsPlayerBotMetinWorthFighting(ch, target)) ||
			// And the same question for an ordinary monster, on a clock: the
			// errand that justified this fight may have finished since it began.
			(bTargetIsMonster && !IsPlayerBotHeldTargetStillWorth(ch, target, state, dwNow)) ||
			!bPartyCanContinue ||
			// In a person's party, round the person (IsPlayerBotTargetOffHumanLeader).
			(!bTargetIsDuelFoe && IsPlayerBotTargetOffHumanLeader(ch, target)) ||
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
				target = FindPlayerBotEngagedTarget(ch, &state, dwNow);
				// An engaged monster is usually self-defence and passes, but the
				// finder also returns what is fighting the party from across the
				// field - so it goes through the same filter as everything else
				// rather than round it.
				if (target && !IsPlayerBotTargetWorthNow(ch, target, state, dwNow))
					target = NULL;
				if (!target)
					target = FindDistributedTarget(ch, state, dwNow);
				// Found round the bot; kept only if it is round the person too.
				if (target && IsPlayerBotTargetOffHumanLeader(ch, target))
					target = NULL;
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
				{
					ReservePlayerBotMetin(ch, target, dwNow);
					int stoneBots = 0, stonePlayers = 0;
					CountPlayerBotStoneAttackersByKind(target, ch, stoneBots, stonePlayers);
					if (stoneBots > 0 || stonePlayers > 0)
						sys_log(0, "PLAYERBOT_METIN: joined a stone pid=%u name=%s level=%u stone=%s stone_level=%u bots_on_it=%d players_on_it=%d",
								ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), target->GetName(),
								target->GetLevel(), stoneBots, stonePlayers);
				}
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
			// A pack just died: in a SLABY mood the bot stands a few seconds
			// before it looks for the next one (playerbot_persona.h).
			if (TakePlayerBotMoodPause(ch, state, dwNow))
				continue;
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

	// The bots the pass above did not reach - before the pid it resumed at and
	// from the pid it stopped at - still take the light half of their tick.
	if (itFirst != m_mapBots.begin() || itStop != m_mapBots.end())
	{
		const DWORD dwLightStartUs = PlayerBotClockUs();
		for (int half = 0; half < 2; ++half)
		{
			TPlayerBotMap::iterator from = half == 0 ? m_mapBots.begin() : itStop;
			TPlayerBotMap::iterator to = half == 0 ? itFirst : m_mapBots.end();
			for (TPlayerBotMap::iterator it = from; it != to; ++it)
			{
				LPDESC d = it->second;
				LPCHARACTER ch = d ? d->GetCharacter() : NULL;
				if (!ch)
					continue;
				RunPlayerBotLightTick(d, ch, s_mapPlayerBotAIStates[it->first], dwNow);
			}
		}
		s_dwPlayerBotLightPassUs = PlayerBotClockUs() - dwLightStartUs;
		s_uPlayerBotLoadLightUs += s_dwPlayerBotLightPassUs;
	}
	else
		s_dwPlayerBotLightPassUs = 0;

	// The census was taken over the pass that has just finished, so it is
	// written here rather than at the top: one line, one minute, every bot of
	// level forty and over standing in Bokjung counted once.
	if (s_bPlayerBotM2CensusPass)
	{
		ReportPlayerBotM2Census();
		ReportPlayerBotPartyCensus();
	}
	ReportPlayerBotPersonaCensus();
	ReportPlayerBotMercCensus(get_dword_time());
	ReportPlayerBotLppCensus(get_dword_time());

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
			// The panels read this file by its header since Iwakura's
			// personalities added four columns (persona, mood, mood_lock,
			// lock_level - 255 while the PERSONA switch is off); the status text
			// stays the last column, because it is the one that may hold spaces.
			fprintf(snapshot, "pid\tpersonality\tambition\trole\tin_party\tgoal\taction\tupdated_ms\tmap\tx\ty\thp\tmax_hp\tpersona\tmood\tmood_lock\tlock_level\tstatus\n");
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

				// The F10 window's "Akcje na zywo" and "Osiagniecia" are fed from
				// here rather than from a pass of their own: the sentence has just
				// been composed and the level is already in hand.
				NotePlayerBotAdminStatus(statusCh->GetPlayerID(), statusText);
				NotePlayerBotAdminLevel(statusCh);

				const bool personaShown = IsPlayerBotPersonaEnabled() && statusState.persona.bRestored;
				const TPlayerBotPersona& shownPersona = statusState.persona;
				fprintf(snapshot, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%ld\t%ld\t%ld\t%d\t%d\t%u\t%u\t%u\t%u\t%s\n",
						statusCh->GetPlayerID(), (unsigned int)statusState.bPersonality,
						(unsigned int)statusState.bAmbition, (unsigned int)statusState.bBotRole,
						statusCh->GetParty() ? 1U : 0U,
						(unsigned int)statusState.bLongTermGoal,
						(unsigned int)statusState.bCurrentAction, (unsigned int)dwNow,
						statusCh->GetMapIndex(), statusCh->GetX(), statusCh->GetY(),
						statusCh->GetHP(), statusCh->GetMaxHP(),
						personaShown ? (unsigned int)shownPersona.bPersona : playerbot_persona::PERSONA_NONE,
						personaShown ? (unsigned int)shownPersona.mood.mood : playerbot_persona::PERSONA_NONE,
						personaShown && playerbot_persona::IsMoodLocked(shownPersona.mood)
							? (unsigned int)shownPersona.mood.lockKind : 0U,
						personaShown && !shownPersona.bAdvanced ? (unsigned int)shownPersona.bLockLevel : 0U,
						statusText);
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

// A GM's /transfer of a bot on this core. The engine's own transfer is a
// WarpSet, which tells a client to reconnect and takes the character off its
// sectree until it does; a bot has nobody to reconnect, so the rescue put it
// back where its own map starts ("robi tp, ale jakby na start mapy" -
// NerrVoVy, Mat and RetroGracz38, 14 September). This is the map change the
// AI makes for every other move, onto the GM's own spot, and the GM is told
// how it went. Nothing holds the bot there afterwards: its next plan is its own.
// A bot's WarpSet (char.cpp through playerbotify.py, apply_bot_warpset): the
// engine's own map change for a player - a dungeon's jump, d.exit_all, a
// quest's pc.warp, a GM's /warp - made server-side, because a bot has no
// client to reconnect. Only onto a map this core hosts, and with the dungeon
// membership Entergame would give a reconnecting player.
bool CPlayerBotManager::WarpBot(LPCHARACTER bot, long x, long y, long lPrivateMapIndex)
{
	if (!bot)
		return false;
	const DWORD dwNow = get_dword_time();
	long lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(x, y);
	if (lPrivateMapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
	{
		if (lMapIndex != 0 && lPrivateMapIndex / 10000 != lMapIndex)
		{
			sys_err("PLAYERBOT_WORLD: warpset pid=%u name=%s private map %ld is not a child of %ld",
					bot->GetPlayerID(), bot->GetName(), lPrivateMapIndex, lMapIndex);
			return false;
		}
		lMapIndex = lPrivateMapIndex;
	}
	TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
	if (lMapIndex == 0 || it == s_mapPlayerBotAIStates.end() || !IsPlayerBotMapHostedHere(lMapIndex))
	{
		sys_log(0, "PLAYERBOT_WORLD: warpset refused pid=%u name=%s to=(%ld,%ld) map=%ld private=%ld from=%ld",
				bot->GetPlayerID(), bot->GetName(), x, y, lMapIndex, lPrivateMapIndex, bot->GetMapIndex());
		return false;
	}
	LPDUNGEON before = bot->GetDungeon();
	if (!TransitionPlayerBotMap(bot, it->second, lMapIndex, x, y, dwNow, "warpset"))
		return false;
	LPDUNGEON after = lMapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
			? CDungeonManager::instance().FindByMapIndex(lMapIndex) : NULL;
	if (before != after)
	{
		if (before)
			bot->SetDungeon(NULL);
		if (after)
			bot->SetDungeon(after);
	}
	return true;
}

bool CPlayerBotManager::TransferBot(LPCHARACTER bot, LPCHARACTER to)
{
	if (!bot || !to)
		return false;
	const DWORD dwNow = get_dword_time();
	const long mapIndex = to->GetMapIndex();
	TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
	const char* refusal = NULL;
	if (it == s_mapPlayerBotAIStates.end())
		refusal = "bot jeszcze nie wszedl do gry";
	else if (bot->IsDead())
		refusal = "bot nie zyje";
	else if (mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
		refusal = "bot nie wejdzie do lochu z osobna instancja";
	else if (!IsPlayerBotMapHostedHere(mapIndex))
		refusal = "tej mapy nie hostuje rdzen bota";
	else if (!TransitionPlayerBotMap(bot, it->second, mapIndex, to->GetX(), to->GetY(),
			dwNow, "gm_transfer"))
		refusal = "bot nie moze stanac na tej mapie";
	if (refusal)
		to->ChatPacket(CHAT_TYPE_INFO, "Nie przeniesiono bota %s: %s.", bot->GetName(), refusal);
	else
		to->ChatPacket(CHAT_TYPE_INFO, "Przeniesiono bota %s do Ciebie.", bot->GetName());
	sys_log(0, "PLAYERBOT_WORLD: gm transfer pid=%u name=%s gm=%s map=%ld pos=(%ld,%ld) result=%s",
			bot->GetPlayerID(), bot->GetName(), to->GetName(), mapIndex, to->GetX(), to->GetY(),
			refusal ? refusal : "ok");
	return refusal == NULL;
}

void CPlayerBotManager::OnGuildInvite(CGuild* guild, LPCHARACTER inviter, LPCHARACTER invitee)
{
	if (!guild || !invitee || !IsRegisteredBotPID(invitee->GetPlayerID()))
		return;
	AcceptPlayerBotGuildInvite(invitee, guild, inviter);
}

// A player's blow at a bot, or at a person in a party (CHARACTER::Damage,
// mt2009 via playerbotify.py): the one thing the engine does not remember
// about a fight, and the one the Anti-PK protocol needs (playerbot_anti_pk.h).
void CPlayerBotManager::OnPlayerStruck(LPCHARACTER victim, LPCHARACTER attacker)
{
	NotePlayerBotStruck(victim, attacker, get_dword_time());
}

// --- The F10 bot-admin window -----------------------------------------------
//
// The summary is counted here and not in playerbot_admin.h because only the
// manager's own book says who is in the world: m_mapBots is private, and the
// snapshot loop above walks it under exactly these two guards. The other two
// are the way through to the anonymous namespace, like the weight functions.
void CPlayerBotManager::GetActivitySummary(size_t& total, size_t& inParty, size_t& stalls) const
{
	total = 0;
	inParty = 0;
	stalls = 0;

	for (TPlayerBotMap::const_iterator it = m_mapBots.begin();
			it != m_mapBots.end(); ++it)
	{
		LPDESC d = it->second;
		LPCHARACTER ch = d ? d->GetCharacter() : NULL;
		if (!ch || !d->IsPhase(PHASE_GAME))
			continue;

		++total;
		if (ch->GetParty())
			++inParty;

		// A counter of its own. On the 2.x line a bot's stall is a native
		// offline shop: the bot opens it and goes back to hunting, so its
		// action is never BOT_ACTION_STALL and counting that alone reported
		// nought while thirty-eight stands were up. The ledger is asked per
		// owner rather than counted whole, because it holds a player's own
		// offline shop too and that is not a bot keeping a stall.
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		auto shop = ikashop::GetManager().GetShopByOwnerID(it->first);
		if (shop && shop->GetDuration() != 0)
			++stalls;
#else
		// The classic stall: the keeper stands behind it, so the action says
		// so. BOT_ACTION_SHOP is the NPC merchant round and BOT_ACTION_MARKET
		// is browsing another bot's counter.
		TPlayerBotAIStateMap::const_iterator ai =
				s_mapPlayerBotAIStates.find(it->first);
		if (ai != s_mapPlayerBotAIStates.end() &&
				ai->second.bCurrentAction == BOT_ACTION_STALL)
			++stalls;
#endif
	}
}

void CPlayerBotManager::GetBotLines(DWORD dwPlayerID, std::vector<std::string>& out) const
{
	GetPlayerBotAdminLines(dwPlayerID, out);
}

bool CPlayerBotManager::GetAchievementWinner(int id, DWORD& dwPID, std::string& strName) const
{
	return GetPlayerBotAdminAchievement(id, dwPID, strName);
}

void CPlayerBotManager::OnPlayerShout(LPCHARACTER ch, const char* szText)
{
	HandlePlayerShoutForTrade(ch, szText);
}

void CPlayerBotManager::OnPlayerWhisper(LPCHARACTER from, LPCHARACTER bot, const char* szText)
{
	HandlePlayerWhisperToBot(from, bot, szText);
}

// --- The F9 panel's two entry points ---------------------------------------
//
// Thin on purpose: everything they do is in playerbot_config.h, above, and the
// only reason these exist is that the fragment lives in this file's anonymous
// namespace and cmd_gm.cpp is a different translation unit.
bool PlayerBotBuildWeightReport(char* szOut, size_t len)
{
	return BuildPlayerBotPanelWeightReport(szOut, len);
}

bool PlayerBotSetWeight(const char* szKey, long value)
{
	return WritePlayerBotPanelWeight(szKey, value);
}
