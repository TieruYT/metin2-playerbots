#ifndef __INC_METIN2_PLAYERBOT_WORLD_RULES_H__
#define __INC_METIN2_PLAYERBOT_WORLD_RULES_H__

// Pure world-travel policy.  Keep decisions which do not need CHARACTER or
// server singletons here so they can be tested without booting a game core.
// The manager remains responsible for performing the chosen transition.
namespace playerbot_world_rules
{
	enum EMonkeyExitDecision
	{
		MONKEY_STAY = 0,
		MONKEY_EXIT_RESTOCK,
		MONKEY_EXIT_MEDAL_READY,
		MONKEY_EXIT_TIMEOUT,
		MONKEY_EXIT_HORSE_COMPLETE
	};

	struct TMonkeyVisitContext
	{
		bool needsEssentialSupply;
		int medalCount;
		int desiredMedalCount;
		bool visitExpired;
		bool canAdvanceHorse;
	};

	inline EMonkeyExitDecision DecideMonkeyExit(const TMonkeyVisitContext& context)
	{
		if (context.needsEssentialSupply)
			return MONKEY_EXIT_RESTOCK;
		if (!context.canAdvanceHorse)
			return MONKEY_EXIT_HORSE_COMPLETE;
		if (context.medalCount >= context.desiredMedalCount)
			return MONKEY_EXIT_MEDAL_READY;
		if (context.visitExpired)
			return MONKEY_EXIT_TIMEOUT;
		return MONKEY_STAY;
	}

	inline bool IsTravelCooldownActive(unsigned int now, unsigned int nextTravelTime)
	{
		return nextTravelTime != 0 && now < nextTravelTime;
	}
}

#endif
