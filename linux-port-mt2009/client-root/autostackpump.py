# The inventory's auto-stack moves, a few at a time.
#
# The button (uiinventory.py, __OnAutoStackButton) sends a move for every pair
# of stacks of one item: 300 moves for 25 stacks, all in the frame it was
# clicked in. The server counts a character's packets per second and closes
# the connection at 300 (CInputMain::Analyze, logged as FLOOD_HEADER_13), which
# threw the player back to the login screen (l0st3k, 15 September). The same
# moves leave MOVES_PER_TICK every TICK seconds from here, sixty a second.
#
# Python 2.7 as the client has it.

import app
import clientclock
import net
import ui
import uiPrivateShopBuilder

MOVES_PER_TICK = 6
TICK = 0.1


class AutoStackPump(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)
		self.moves = []
		self.nextTick = 0.0
		self.SetSize(0, 0)
		self.Show()

	def Queue(self, moves):
		# A second click replaces what is left of the first: the moves were
		# worked out from the bag as it was then.
		self.moves = list(moves)
		self.nextTick = 0.0

	def OnUpdate(self):
		if not self.moves:
			return
		now = clientclock.Now()
		if now < self.nextTick:
			return
		self.nextTick = now + TICK
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			self.moves = []
			return
		for (sourceSlot, destSlot) in self.moves[:MOVES_PER_TICK]:
			net.SendItemMovePacket(sourceSlot, destSlot, 0)
		del self.moves[:MOVES_PER_TICK]


_pump = None


def Queue(moves):
	global _pump
	if _pump is None:
		_pump = AutoStackPump()
	_pump.Queue(moves)
