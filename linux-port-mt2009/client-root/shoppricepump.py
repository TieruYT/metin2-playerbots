# The offline shop's "edit the price of similar items", one at a time - and
# then a look at what actually took.
#
# Ctrl + right click on a line of the shop's edit grid reprices every line
# holding the same item, and offlineshopmanage.py used to send one packet per
# line in the frame it was clicked in. The server takes one shop action per
# 200 ms (PulseManager, ePulse::IkaShopTouchItem), so the first line was
# repriced and every other one answered "wait a moment" - reported by
# uxietoszef and again by Nagash on 20 September, with a screenshot of the
# refusals. The same edits leave from here, one every TICK seconds.
#
# That was not the whole of it. "Zmienianie cen dziala ale potrafi ominac nawet
# pare itemow gdy ma sie ich duzo w sklepie" (blastyw, 20 September): spacing
# the packets out is still firing and forgetting, and a quarter of a second of
# the client's clock can arrive inside one 200 ms window of the server's after
# a slow frame or a late packet. One refused edit was then lost for good, and
# on a counter of thirty lines a couple always were.
#
# So the pump reads the reaction rather than counting the shots: when the queue
# drains it waits for the shop list the server sends back, compares the price
# of every line it touched with the price it asked for, and queues the ones
# that did not take again - PASSES times, and then it says in the chat what is
# left rather than leaving somebody to find it by eye.
#
# Python 2.7 as the client has it.

import app
import chat
import constInfo
import ui
import ikashop
import offlineShopBuilder

# A shade over the server's own 200 ms, so a slow frame cannot land two
# packets inside one of its windows.
TICK = 0.25
# How long to let the shop's own list come back before believing what it says.
# The edit is a round trip through the db core, so this is not the tick.
VERIFY_DELAY = 1.5
# How many times a line that did not take is asked for again. Three passes of
# a thirty-line counter is still under a minute, and a line that survives all
# three is not being refused by the pulse.
PASSES = 3


def _shop_prices():
	"""id -> price, as the client's copy of its own counter has it now."""
	out = {}
	try:
		items = constInfo.myshop_data["items"]
	except (AttributeError, KeyError, TypeError):
		return out
	try:
		rows = items.values()
	except AttributeError:
		return out
	for row in rows:
		try:
			out[row["id"]] = row["price"]
		except (KeyError, TypeError):
			continue
	return out


class ShopPricePump(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)
		self.edits = []
		self.asked = []
		self.nextTick = 0.0
		self.verifyAt = 0.0
		self.passesLeft = 0
		self.SetSize(0, 0)
		self.Show()

	def Queue(self, edits):
		# A second click replaces what is left of the first: the lines were
		# read out of the shop as it was then.
		self.edits = list(edits)
		self.asked = list(edits)
		self.nextTick = 0.0
		self.verifyAt = 0.0
		self.passesLeft = PASSES

	def OnUpdate(self):
		if self.edits:
			self._send()
			return
		if self.verifyAt and app.GetTime() >= self.verifyAt:
			self._verify()

	def _send(self):
		now = app.GetTime()
		if now < self.nextTick:
			return
		self.nextTick = now + TICK
		(itemData, itemPrice) = self.edits.pop(0)
		ikashop.SendEditItem(itemData["id"], itemPrice)
		offlineShopBuilder.SetPrivateShopItemPrice(
			itemData["vnum"], itemData["count"], itemPrice, itemData["sockets"])
		if not self.edits:
			self.verifyAt = now + VERIFY_DELAY

	def _verify(self):
		self.verifyAt = 0.0
		if not self.asked:
			return
		live = _shop_prices()
		if not live:
			# Nothing to compare against - the window was closed, or this
			# client build keeps its counter somewhere else. Saying "two lines
			# were missed" on no evidence would be worse than saying nothing.
			self.asked = []
			self.passesLeft = 0
			return
		missed = []
		for (itemData, itemPrice) in self.asked:
			try:
				itemId = itemData["id"]
			except (KeyError, TypeError):
				continue
			# A line that is no longer on the counter was bought or taken off
			# while this ran; it is not a miss.
			if itemId in live and live[itemId] != itemPrice:
				missed.append((itemData, itemPrice))
		if not missed:
			self.asked = []
			self.passesLeft = 0
			return
		if self.passesLeft > 0:
			self.passesLeft -= 1
			self.edits = list(missed)
			self.asked = list(missed)
			self.nextTick = 0.0
			return
		self.asked = []
		try:
			chat.AppendChat(chat.CHAT_TYPE_INFO,
					"Nie udalo sie zmienic ceny %d pozycji - sprobuj jeszcze raz." % len(missed))
		except Exception:
			pass


_pump = None


def Queue(edits):
	global _pump
	if _pump is None:
		_pump = ShopPricePump()
	_pump.Queue(edits)
