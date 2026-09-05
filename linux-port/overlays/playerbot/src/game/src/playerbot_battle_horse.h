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
//   * The quest counts kills of mobs 2105 and 2107. Neither is spawned anywhere
//     in this server's maps - the desert here is stocked with the Black Wind
//     band, 401 to 404 - so the trial counts those instead. It is the same
//     desert the quest sends a player to and the same one bots already hunt.
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

	// Out in the desert working on it.
	bool IsPlayerBotOnBattleHorseTrial(LPCHARACTER ch)
	{
		return IsPlayerBotBattleHorseCandidate(ch) &&
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
		return vnum >= PLAYERBOT_BATTLE_HORSE_MOB_FIRST &&
				vnum <= PLAYERBOT_BATTLE_HORSE_MOB_LAST;
	}

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
	int GetPlayerBotReservedGold(LPCHARACTER ch)
	{
		return IsPlayerBotBattleHorseEarned(ch) ? (int)PLAYERBOT_BATTLE_HORSE_FEE : 0;
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

		ch->PointChange(POINT_GOLD, -(int)PLAYERBOT_BATTLE_HORSE_FEE);
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
