#include <cassert>
#include "../linux-port/overlays/playerbot/src/game/src/playerbot_world_rules.h"

using namespace playerbot_world_rules;

int main()
{
	// needsEssentialSupply, needsPotions, medalCount, desiredMedalCount,
	// visitExpired, canAdvanceHorse
	TMonkeyVisitContext context = { false, false, 0, 2, false, true };
	assert(DecideMonkeyExit(context) == MONKEY_STAY);

	context.needsEssentialSupply = true;
	assert(DecideMonkeyExit(context) == MONKEY_EXIT_RESTOCK);
	context.needsEssentialSupply = false;

	// Running dry on red potions has to end the visit too. Without this the
	// dungeon was the one map a potionless bot never left: it stayed and died in
	// a loop until the medal, the 30-minute timeout or a lost weapon released it.
	context.needsPotions = true;
	assert(DecideMonkeyExit(context) == MONKEY_EXIT_RESTOCK);
	context.needsPotions = false;
	assert(DecideMonkeyExit(context) == MONKEY_STAY);

	context.medalCount = 2;
	assert(DecideMonkeyExit(context) == MONKEY_EXIT_MEDAL_READY);

	context.medalCount = 0;
	context.visitExpired = true;
	assert(DecideMonkeyExit(context) == MONKEY_EXIT_TIMEOUT);

	context.visitExpired = false;
	context.canAdvanceHorse = false;
	assert(DecideMonkeyExit(context) == MONKEY_EXIT_HORSE_COMPLETE);

	assert(!IsTravelCooldownActive(1000U, 0U));
	assert(IsTravelCooldownActive(1000U, 1001U));
	assert(!IsTravelCooldownActive(1001U, 1001U));
	return 0;
}
