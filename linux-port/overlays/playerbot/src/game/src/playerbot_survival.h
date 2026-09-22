#ifndef __INC_METIN2_PLAYERBOT_SURVIVAL_H__
#define __INC_METIN2_PLAYERBOT_SURVIVAL_H__

// Staying alive and coming back: saving progress, breaking off a losing fight,
// and the walk back after dying.
//
// Death is the one state the rest of the AI must not act during - a corpse has
// no route, no target and no errand - which is why it is handled before every
// other subsystem in the tick and returns straight away.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once.

namespace
{
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
		++s_uPlayerBotLoadSaves;
		ch->FlushDelayedSaveItem();
		const DWORD playerID = ch->GetPlayerID();
#if !defined(PLAYERBOT_ENGINE_MT2009)
		// mt2009 has no cache-flush packet; its db core writes the player
		// cache out on its own clock, so a panel reads the save a little later.
		db_clientdesc->DBPacket(HEADER_GD_FLUSH_CACHE, 0, &playerID, sizeof(playerID));
#endif
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

		// Two ways out that do not depend on the monster changing its mind: the
		// bot is far enough away, or it has been running long enough. Without
		// them the only exit was the threat dropping aggro, and a monster that
		// cannot reach the bot never does.
		const int threatDistance = threat
				? DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY())
				: PLAYERBOT_RETREAT_SAFE_DISTANCE;
		const bool escaped = threatDistance >= PLAYERBOT_RETREAT_SAFE_DISTANCE;
		const bool ranLongEnough = state.dwRetreatStartedTime != 0 &&
				dwNow - state.dwRetreatStartedTime >= PLAYERBOT_RETREAT_MAX_MS;
		if ((!bThreatHasAggro && hpPercent >= PLAYERBOT_RETREAT_END_HP_PERCENT) ||
				escaped || ranLongEnough)
		{
			if (escaped || ranLongEnough)
				sys_log(0, "PLAYERBOT_AI: tactical retreat over pid=%u name=%s hp=%d/%d dist=%d reason=%s",
						ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP(),
						threatDistance, escaped ? "escaped" : "timeout");
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
			// The same five seconds do_restart gives a player. Ten made the bot
			// disappear for twice as long as anybody else does after standing up,
			// which is the half of dying that did not look like a player's.
			ch->ReviveInvisible(5);
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
				// The first of the eight ways out the bot's own ground joins,
				// starting from the one its pid draws. In a Monkey Dungeon
				// chamber most of them are the wall, and a walk planned into
				// the wall was planned again on every tick until the bot had
				// healed: the ten loudest lines of game1's syserr on
				// 23 September were that one unreachable plan, a thousand
				// units east of where medal droppers had died.
				const BYTE firstDirection = (BYTE)((ch->GetPlayerID() + state.bDeathCount +
						state.bStuckCounter) % 8);
				for (BYTE k = 0; k < 8; ++k)
				{
					const BYTE escapeDirection = (BYTE)((firstDirection + k) % 8);
					const long targetX = state.lDeathX + kEscapeX[escapeDirection];
					const long targetY = state.lDeathY + kEscapeY[escapeDirection];
					if (!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), targetX, targetY))
						continue;
					// The walk back from the respawn point is the longest routine
					// journey a bot makes, and it was made on foot.
					MovePlayerBot(ch, targetX, targetY, dwNow, 16, true, true);
					return true;
				}
				// No way out joins: the bot stays, invisible and healing, where
				// it stood up.
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
			// A bot that falls in a duel has lost it, whoever struck last, and
			// the duel ends on the tick it falls. It used to end only once the
			// bot had stood up again and been refused its blows for
			// PLAYERBOT_PVP_REFUSED_GIVE_UP, still counting itself in the duel
			// - and before 2.0.41, when those blows were not refused, going on
			// hitting a winner whose client would not let them hit back ("wali
			// jakas zemste, gdzie nie moge mu oddac", Drip).
			if (playerbot_pvp::GetDuelOpponent(ch->GetPlayerID(), dwNow) != 0)
				EndPlayerBotDuel(ch, state, dwNow, "lost");
			state.lDeathX = ch->GetX();
			state.lDeathY = ch->GetY();
			++state.bDeathCount;
			// A Conqueror dying to monsters too often has outgrown its gear
			// (playerbot_persona.h).
			NotePlayerBotPersonaDeath(ch, state, dwNow);

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
				// The same five seconds do_restart gives a player. Ten made the bot
			// disappear for twice as long as anybody else does after standing up,
			// which is the half of dying that did not look like a player's.
			ch->ReviveInvisible(5);
				sys_log(0, "PLAYERBOT_AI: revived at same position pid=%u name=%s hp=%d/%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
				state.dwDeathDetectedTime = 0;
				state.dwNextReviveAttemptTime = 0;
			}
		}

		return true;
	}
}

#endif
