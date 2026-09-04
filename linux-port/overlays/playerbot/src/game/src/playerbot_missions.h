#ifndef __INC_METIN2_PLAYERBOT_MISSIONS_H__
#define __INC_METIN2_PLAYERBOT_MISSIONS_H__

// The two quest lines a bot runs on its own: the Biologist's collections and
// the official level-up hunt.
//
// Both are ordinarily driven by a quest dialog. A bot has no client to press
// Confirm, so the accepting, the counting and the reward are all done here
// against the same quest flags the real quest reads - which is why this is
// bookkeeping rather than behaviour, and why the flag names matter more than
// the code around them.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_gear.h - a finished mission hands out an item.

namespace
{
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
}

#endif
