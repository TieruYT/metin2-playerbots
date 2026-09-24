#ifndef __INC_METIN2_PLAYERBOT_BATTLE_HORSE_H__
#define __INC_METIN2_PLAYERBOT_BATTLE_HORSE_H__

// Earning the battle horse.
//
// The stable keeper's own quest (horse_upgrade) is the reference and most of it
// is kept: character level 35, a horse already at the cap of 10, a hundred kills
// out in the desert, five hundred thousand yang at the end, and the armoured
// horse book handed over while the ordinary horse's paper is taken away.
//
// Three things in it could not be kept, and each is a fact about this world
// rather than a preference:
//
//   * The quest gives thirty minutes and fails you at the end of them. A bot
//     hunts in a straight line for hours and has nobody to be disappointed by a
//     failure, so there is no clock: it kills until it is done.
//   * The quest then makes you wait eight to sixteen hours for the horse to be
//     made ready. That wait exists to slow a person down between play sessions;
//     for a population that never logs off it is only a pause in a log file.
//
// The medal item the quest consumes to begin, and the horse photograph it
// consumes at the end, are not required. There is one medal in this entire
// world and not a single photograph, and no path by which a bot could ever get
// one - requiring them would mean shipping a system that can never run.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, before playerbot_activities.h - the stable visit hands the
// horse over - and before playerbot_travel.h, which has to send a bot on the
// trial to the desert whatever its level says.

namespace
{
	// Where the trial stands, as a number kept on the character so it survives a
	// restart the way the Biologist's progress does.
	int GetPlayerBotBattleHorseKills(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		return std::max(0, ch->GetQuestFlag(PLAYERBOT_BATTLE_HORSE_KILLS_FLAG));
	}

	// playerbot_travel.h; the trial is asked about long before the travel is
	// included.
	bool IsPlayerBotMapHostedHere(long mapIndex);

	// A trial is open only on the core that hosts its map. A bot cannot
	// cross to a map its core does not host, and on a split world the
	// desert and the Demon Tower are on one core each: 58 Shinsoo and 42
	// Jinno bots stood in their second villages reading "Zdobywam konia
	// bojowego na pustyni (0/100)" (seban latino, 16 September) - the
	// frontier draw answered the desert and was filtered to nothing, and
	// since 2.0.61 the Biologist yielded to the trial as well, so those
	// bots had neither. The answer is kept per map because the target
	// collector asks it per candidate monster; the maps are loaded before
	// the first bot ticks.
	bool IsPlayerBotHorseTrialOpenHere(long trialMap)
	{
		static std::map<long, bool> s_mapTrialHosted;
		std::map<long, bool>::iterator it = s_mapTrialHosted.find(trialMap);
		if (it == s_mapTrialHosted.end())
			it = s_mapTrialHosted.insert(std::make_pair(trialMap, IsPlayerBotMapHostedHere(trialMap))).first;
		return it->second;
	}

	// Everything the stable keeper checks before it will talk about a battle
	// horse, minus the two items this world cannot supply.
	bool IsPlayerBotBattleHorseCandidate(LPCHARACTER ch)
	{
		return ch &&
				ch->GetLevel() >= PLAYERBOT_BATTLE_HORSE_MIN_LEVEL &&
				ch->GetHorseLevel() == PLAYERBOT_BATTLE_HORSE_FROM_HORSE_LEVEL &&
				// horse.is_dead() in the quest is exactly this test.
				ch->GetHorseHealth() > 0;
	}

	// A dropper is a drop character and takes no trial - the operator's rule of
	// 15 September, which the Biologist and the guild already follow. The two
	// "on trial" predicates below did not ask, so a Metin dropper of thirty-six
	// with a horse at ten was on the battle trial as far as every reader was
	// concerned: the frontier draw pointed it at the desert, and its status read
	// "Zdobywam konia bojowego na pustyni (0/100)" from the guild map it farms
	// (GG1249125 and MORDEGAPOTEGA, Urtopy, 18 September). The stable keeper's
	// side (IsPlayerBotBattleHorseEarned) is left alone: a horse already earned
	// is still handed over.
	bool IsPlayerBotTrialExempt(LPCHARACTER ch)
	{
		return ch && IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID()));
	}

	// Out in the desert working on it.
	bool IsPlayerBotOnBattleHorseTrial(LPCHARACTER ch)
	{
		return IsPlayerBotHorseTrialOpenHere(PLAYERBOT_MAP_DESERT) &&
				IsPlayerBotBattleHorseCandidate(ch) &&
				!IsPlayerBotTrialExempt(ch) &&
				GetPlayerBotBattleHorseKills(ch) < PLAYERBOT_BATTLE_HORSE_KILLS;
	}

	// Done killing; the horse is waiting at the stable.
	bool IsPlayerBotBattleHorseEarned(LPCHARACTER ch)
	{
		return IsPlayerBotBattleHorseCandidate(ch) &&
				GetPlayerBotBattleHorseKills(ch) >= PLAYERBOT_BATTLE_HORSE_KILLS;
	}

	bool IsPlayerBotBattleHorseTrialMob(DWORD vnum)
	{
		return vnum == PLAYERBOT_BATTLE_HORSE_MOB_SNAKE_ARCHER ||
				vnum == PLAYERBOT_BATTLE_HORSE_MOB_SCORPION_ARCHER;
	}

	// What a bot on the battle trial hunts out in the desert, as the hunt the
	// material errand walks to (StartPlayerBotMaterialHunt): the scorpion
	// archer, whose family takes the snake archer too
	// (IsPlayerBotBiologistHuntRace). The archers are a quest target once in
	// sight, but the hub choice goes by density and sent the trial's bots of
	// thirty-five to forty among the desert's spiders. The archers stand in the
	// middle of the map, 0 to 3 of them within reach of nine of its twelve
	// hubs, and 38 bots on the trial made one kill an hour each between them
	// (m2zip, 24 September; "boty nie wykonuja misji", rakso7064).
	DWORD GetPlayerBotHorseTrialHuntMob(LPCHARACTER ch)
	{
		return ch && ch->GetMapIndex() == PLAYERBOT_MAP_DESERT && IsPlayerBotOnBattleHorseTrial(ch)
				? PLAYERBOT_BATTLE_HORSE_MOB_SCORPION_ARCHER : 0;
	}

	// The military horse: the same shape one step up.
	//
	// Medals carry a horse to twenty and stop there; the twenty-first level is a
	// trial in the Demon Tower, with no clock on it, exactly as the combat horse
	// is a trial in the desert. That is what the operator asked for, and it is
	// why map 66 had to be moved onto the core the bots live on - 1001-1004
	// stand nowhere else in this world, so before the move this trial could
	// never have been started, let alone finished.
	int GetPlayerBotMilitaryHorseKills(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		return std::max(0, ch->GetQuestFlag(PLAYERBOT_MILITARY_HORSE_KILLS_FLAG));
	}

	bool IsPlayerBotMilitaryHorseCandidate(LPCHARACTER ch)
	{
		return ch &&
				ch->GetLevel() >= PLAYERBOT_MILITARY_HORSE_MIN_LEVEL &&
				ch->GetHorseLevel() == PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL &&
				ch->GetHorseHealth() > 0;
	}

	bool IsPlayerBotOnMilitaryHorseTrial(LPCHARACTER ch)
	{
		return IsPlayerBotHorseTrialOpenHere(PLAYERBOT_MAP_DEMON_TOWER) &&
				IsPlayerBotMilitaryHorseCandidate(ch) &&
				!IsPlayerBotTrialExempt(ch) &&
				GetPlayerBotMilitaryHorseKills(ch) < PLAYERBOT_MILITARY_HORSE_KILLS;
	}

	bool IsPlayerBotMilitaryHorseEarned(LPCHARACTER ch)
	{
		return IsPlayerBotMilitaryHorseCandidate(ch) &&
				GetPlayerBotMilitaryHorseKills(ch) >= PLAYERBOT_MILITARY_HORSE_KILLS;
	}

	bool IsPlayerBotMilitaryHorseTrialMob(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_MILITARY_HORSE_MOBS) /
				sizeof(PLAYERBOT_MILITARY_HORSE_MOBS[0]); ++i)
			if (PLAYERBOT_MILITARY_HORSE_MOBS[i] == vnum)
				return true;
		return false;
	}

	// The Biologist's share of a kill (playerbot_missions.h, later in the
	// include order).
	void NotePlayerBotBiologistCarrierKill(LPCHARACTER ch, LPCHARACTER target);

	// Called wherever a bot has just swung at something. The engine has no hook
	// that says "you killed this", so the kill is read off the target the tick
	// after the blow: still the bot's pointer, now dead. The VID is remembered
	// so that a corpse the bot is still standing over cannot be counted twice.
	void NotePlayerBotBattleHorseKill(LPCHARACTER ch, TPlayerBotAIState& state,
			LPCHARACTER target)
	{
		if (!ch || !target || !target->IsDead() || !target->IsMonster())
			return;
		const DWORD vid = (DWORD)target->GetVID();
		if (state.dwLastKillCreditedVID == vid)
			return;
		state.dwLastKillCreditedVID = vid;
		// Under the same guard, so a corpse is one roll.
		NotePlayerBotBiologistCarrierKill(ch, target);

		// The military trial is credited from the same place and under the same
		// VID guard. A second hook of its own would have had to share
		// dwLastKillCreditedVID with this one, and whichever ran first would
		// have eaten the other's kill.
		if (IsPlayerBotOnMilitaryHorseTrial(ch) &&
				IsPlayerBotMilitaryHorseTrialMob(target->GetRaceNum()))
		{
			const int demonKills = GetPlayerBotMilitaryHorseKills(ch) + 1;
			ch->SetQuestFlag(PLAYERBOT_MILITARY_HORSE_KILLS_FLAG, demonKills);
			if (demonKills >= PLAYERBOT_MILITARY_HORSE_KILLS)
				sys_log(0, "PLAYERBOT_HORSE: military trial complete pid=%u name=%s kills=%d",
						ch->GetPlayerID(), ch->GetName(), demonKills);
			else if (demonKills % 10 == 0)
				sys_log(0, "PLAYERBOT_HORSE: military trial pid=%u name=%s kills=%d/%d",
						ch->GetPlayerID(), ch->GetName(), demonKills,
						PLAYERBOT_MILITARY_HORSE_KILLS);
			return;
		}

		if (!IsPlayerBotOnBattleHorseTrial(ch) ||
				!IsPlayerBotBattleHorseTrialMob(target->GetRaceNum()))
			return;

		const int kills = GetPlayerBotBattleHorseKills(ch) + 1;
		ch->SetQuestFlag(PLAYERBOT_BATTLE_HORSE_KILLS_FLAG, kills);
		if (kills >= PLAYERBOT_BATTLE_HORSE_KILLS)
			sys_log(0, "PLAYERBOT_HORSE: battle trial complete pid=%u name=%s kills=%d",
					ch->GetPlayerID(), ch->GetName(), kills);
		else if (kills % 25 == 0)
			sys_log(0, "PLAYERBOT_HORSE: battle trial pid=%u name=%s kills=%d/%d",
					ch->GetPlayerID(), ch->GetName(), kills, PLAYERBOT_BATTLE_HORSE_KILLS);
	}

	// Yang this bot must not spend on anything else, because something it has
	// already worked for is waiting to be paid for. Every discretionary spender
	// subtracts this before deciding it can afford itself.
	// Defined in playerbot_travel.h, which every spender is included before.
	long GetPlayerBotFrontierMapForLevel(LPCHARACTER ch);

	// The Teleporter's fare as map_warp.quest computes it: a thousand per five
	// levels, at least a thousand. Duplicated from playerbot_travel.h, which
	// is included after every spender that asks for the reserve.
	int GetPlayerBotTeleporterFareEstimate(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		return std::max(PLAYERBOT_TELEPORTER_FEE_PER_FIVE_LEVELS,
				((int)ch->GetLevel() / 5) * PLAYERBOT_TELEPORTER_FEE_PER_FIVE_LEVELS);
	}

	int GetPlayerBotReservedGold(LPCHARACTER ch)
	{
		int reserved = IsPlayerBotBattleHorseEarned(ch) ? (int)PLAYERBOT_BATTLE_HORSE_FEE : 0;
		// A bot whose hunting ground is a frontier map gets there through the
		// Teleporter and back through it after every town errand. Spending the
		// fare on a refine left 268 of 362 bots of 40+ stranded in Bokjung,
		// where the cohort ceiling forbids the hunting that would earn it back.
		if (ch && GetPlayerBotFrontierMapForLevel(ch) != 0)
			reserved += GetPlayerBotTeleporterFareEstimate(ch) * PLAYERBOT_TELEPORTER_FARE_RESERVE_COUNT;
		return reserved;
	}

	// The stable keeper's side of it. Everything here is what the quest's `buy`
	// state does, in the same order: take the money, take the ordinary horse's
	// paper, advance the horse, hand over the armoured horse's book.
	bool CollectPlayerBotBattleHorse(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotBattleHorseEarned(ch))
			return false;
		if (ch->GetGold() < (int)PLAYERBOT_BATTLE_HORSE_FEE)
			return false;

		PlayerBotChangeGold(ch, -(int)PLAYERBOT_BATTLE_HORSE_FEE);
		// The ordinary horse's paper goes back, as it does for a player. No bot
		// has one - nothing in this world hands them out - so this is here for
		// the day something does, not because it fires today.
		if (ch->CountSpecifyItem(PLAYERBOT_HORSE_PHOTO_VNUM) > 0)
			ch->RemoveSpecifyItem(PLAYERBOT_HORSE_PHOTO_VNUM, 1);

		// horse.advance() is SetHorseLevel + ComputePoints + SkillLevelPacket,
		// and the quest dismounts and remounts around it so the rider is sitting
		// on the animal it just became.
		const bool wasRiding = ch->IsRiding();
		if (wasRiding)
			ch->StopRiding();
		ch->SetHorseLevel(ch->GetHorseLevel() + 1);
		ch->ComputePoints();
		ch->SkillLevelPacket();
		if (wasRiding)
			ch->StartRiding();

		ch->AutoGiveItem(PLAYERBOT_BATTLE_HORSE_BOOK_VNUM, 1, -1, false);
		// The counter is left where it is. It cannot start another trial: the
		// candidate test wants a horse at exactly ten, and this one is eleven.
		sys_log(0, "PLAYERBOT_HORSE: battle horse collected pid=%u name=%s horse_level=%u gold=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetHorseLevel(),
				(int)(ch->GetGold() / 1000));
		return true;
	}
}

#endif
