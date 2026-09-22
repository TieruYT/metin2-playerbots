# The safebox's "Scal i uporzadkuj", and its stacks moved by count across the
# safebox and the bag or inside the safebox (uisafebox.py, uiinventory.py;
# blasty's proposal, 19 September).
#
# The client's safebox packets name cells and no count - SendSafeboxCheckinPacket,
# SendSafeboxCheckoutPacket, SendSafeboxItemMovePacket - so a part of a stack,
# and a stack dropped on the same item, go to the server as a command
# ("/safebox_put", "/safebox_take", "/safebox_move") and come back as
# "SafeboxTransferResult <op> <code> <units>" (game.py). A whole stack dropped
# on a free place still goes by the packet, as it always did. The button in the
# safebox's title bar sends "/safebox_arrange" and hears
# "SafeboxArrangeResult <code> <moved> <merged> <units>". The server's side is
# playerbot_arrange.cpp.
#
# Only the four bag pages take part; the belt, the horse's page and the dragon
# soul window keep the packets. A transfer that works says nothing - the two
# windows show it - and a refusal says why.
#
# The texts are CP1250, the client's own, written as escapes so the file stays
# ASCII. Python 2.7 as the client has it.

import app
import clientclock
import chat
import mouseModule
import net
import player
import safebox

PENDING_TIMEOUT = 5.0

# playerbot_arrange.h, EResult.
RESULT_DONE = 0
RESULT_NOTHING = 1
RESULT_BUSY = 2
RESULT_COOLDOWN = 3
RESULT_NO_LAYOUT = 4
RESULT_DEAD = 5
RESULT_INCONSISTENT = 6
RESULT_UNSUPPORTED = 7
RESULT_BAD_REQUEST = 8
RESULT_NO_SAFEBOX = 9

# playerbot_arrange.h, ETransferOp and ETransferResult.
OP_PUT = 1
OP_TAKE = 2
OP_MOVE = 3

TRANSFER_DONE = 0
TRANSFER_BUSY = 1
TRANSFER_NO_SAFEBOX = 2
TRANSFER_NO_ITEM = 3
TRANSFER_OCCUPIED = 4
TRANSFER_FULL = 5
TRANSFER_REFUSED = 6
TRANSFER_BAD_REQUEST = 7
TRANSFER_COOLDOWN = 8
TRANSFER_UNSUPPORTED = 9
TRANSFER_DEAD = 10

MSG_ARRANGE_DONE = 'Uporz\xb9dkowano magazyn: przestawiono %d, scalono stos\xf3w: %d.'
MSG_ARRANGE_NOTHING = 'Magazyn jest ju\xbf uporz\xb9dkowany.'
MSG_ARRANGE_BUSY = 'Nie mo\xbfna teraz uporz\xb9dkowa\xe6 magazynu - zamknij handel, sklep lub inne okno.'
MSG_ARRANGE_COOLDOWN = 'Odczekaj chwil\xea przed kolejnym porz\xb9dkowaniem.'
MSG_ARRANGE_NO_LAYOUT = 'Nie uda\xb3o si\xea u\xb3o\xbfy\xe6 magazynu - nic nie zmieniono.'
MSG_ARRANGE_DEAD = 'Nie mo\xbfesz porz\xb9dkowa\xe6 magazynu po \x9cmierci.'
MSG_ARRANGE_INCONSISTENT = 'Magazyn jest w nieoczekiwanym stanie - nic nie zmieniono. Zg\xb3o\x9c to na Discordzie.'
MSG_NO_SAFEBOX = 'Magazyn nie jest otwarty.'
MSG_UNSUPPORTED = 'Serwer nie obs\xb3uguje tej funkcji magazynu.'
MSG_ATTACHED = 'Od\xb3\xf3\xbf najpierw przedmiot trzymany kursorem.'
MSG_SHOP = 'Nie mo\xbfna porz\xb9dkowa\xe6 magazynu podczas otwierania sklepu.'

MSG_TRANSFER_BUSY = 'Nie mo\xbfna teraz przenie\x9c\xe6 przedmiotu - zamknij handel, sklep lub inne okno.'
MSG_TRANSFER_NO_ITEM = 'Tego przedmiotu ju\xbf tam nie ma.'
MSG_TRANSFER_OCCUPIED = 'Na tym polu le\xbfy inny przedmiot.'
MSG_TRANSFER_FULL = 'Ten stos jest ju\xbf pe\xb3ny.'
MSG_TRANSFER_REFUSED = 'Tego przedmiotu nie mo\xbfna tam prze\xb3o\xbfy\xe6.'
MSG_TRANSFER_BAD_REQUEST = 'Nie mo\xbfna tam prze\xb3o\xbfy\xe6 przedmiotu.'
MSG_TRANSFER_COOLDOWN = 'Za szybko - spr\xf3buj jeszcze raz.'
MSG_TRANSFER_DEAD = 'Nie mo\xbfesz przek\xb3ada\xe6 przedmiot\xf3w po \x9cmierci.'

_state = {'arrangeUntil': 0.0}


def _int(value):
	try:
		return int(value)
	except (TypeError, ValueError):
		return -1


def BagCells():
	return player.INVENTORY_PAGE_SIZE * player.INVENTORY_PAGE_COUNT


def _IsWhole(count, have):
	return count <= 0 or count >= have


def _Count(count, have):
	if _IsWhole(count, have):
		return 0
	return count


def IsArrangePending():
	return clientclock.Now() < _state['arrangeUntil']


def RequestArrange():
	if IsArrangePending():
		return False
	if mouseModule.mouseController.isAttached():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_ATTACHED)
		return False
	# Asked here and not at import: uisafebox.py imports this module, and the
	# shop builder pulls half the interface in behind it.
	import uiPrivateShopBuilder
	if uiPrivateShopBuilder.IsBuildingPrivateShop():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_SHOP)
		return False
	_state['arrangeUntil'] = clientclock.Now() + PENDING_TIMEOUT
	net.SendChatPacket('/safebox_arrange')
	return True


def DropInSafebox(srcPos, dstPos, count, occupied):
	"""A safebox stack dropped on a safebox cell."""
	if srcPos == dstPos:
		return
	have = safebox.GetItemCount(srcPos)
	if occupied or not _IsWhole(count, have):
		net.SendChatPacket('/safebox_move %d %d %d' % (srcPos, dstPos, _Count(count, have)))
	else:
		net.SendSafeboxItemMovePacket(srcPos, dstPos)


def DropIntoSafebox(invenType, cell, safePos, count, occupied):
	"""A stack of the bag dropped on a safebox cell."""
	if invenType == player.INVENTORY and 0 <= cell < BagCells():
		have = player.GetItemCount(cell)
		if occupied or not _IsWhole(count, have):
			net.SendChatPacket('/safebox_put %d %d %d' % (cell, safePos, _Count(count, have)))
			return
	if occupied:
		# The engine's packet puts nothing on a taken place, and a stack from
		# the belt or the dragon soul window pours nowhere.
		return
	net.SendSafeboxCheckinPacket(invenType, cell, safePos)


def DropIntoBag(safePos, cell, count, occupied):
	"""A safebox stack dropped on a cell of the bag."""
	if 0 <= cell < BagCells():
		have = safebox.GetItemCount(safePos)
		if occupied or not _IsWhole(count, have):
			net.SendChatPacket('/safebox_take %d %d %d' % (safePos, cell, _Count(count, have)))
			return
	if occupied:
		return
	net.SendSafeboxCheckoutPacket(safePos, cell)


def MakePickDialog(acceptEvent):
	"""The count dialog a stack is split with - the one the bag uses."""
	if getattr(app, 'ENABLE_CHEQUE_SYSTEM', 0):
		import uiPickETC
		dialog = uiPickETC.PickETCDialog()
	else:
		import uiPickMoney
		dialog = uiPickMoney.PickMoneyDialog()
	dialog.LoadDialog()
	dialog.SetAcceptEvent(acceptEvent)
	dialog.Hide()
	return dialog


def ArrangeMessage(code, moved, merged):
	if code == RESULT_DONE:
		return MSG_ARRANGE_DONE % (moved, merged)
	if code == RESULT_NOTHING:
		return MSG_ARRANGE_NOTHING
	if code == RESULT_BUSY:
		return MSG_ARRANGE_BUSY
	if code == RESULT_COOLDOWN:
		return MSG_ARRANGE_COOLDOWN
	if code == RESULT_NO_LAYOUT:
		return MSG_ARRANGE_NO_LAYOUT
	if code == RESULT_DEAD:
		return MSG_ARRANGE_DEAD
	if code == RESULT_INCONSISTENT:
		return MSG_ARRANGE_INCONSISTENT
	if code == RESULT_NO_SAFEBOX:
		return MSG_NO_SAFEBOX
	return MSG_UNSUPPORTED


def TransferMessage(code):
	"""What to say about a transfer; None when it worked."""
	if code == TRANSFER_DONE:
		return None
	if code == TRANSFER_BUSY:
		return MSG_TRANSFER_BUSY
	if code == TRANSFER_NO_SAFEBOX:
		return MSG_NO_SAFEBOX
	if code == TRANSFER_NO_ITEM:
		return MSG_TRANSFER_NO_ITEM
	if code == TRANSFER_OCCUPIED:
		return MSG_TRANSFER_OCCUPIED
	if code == TRANSFER_FULL:
		return MSG_TRANSFER_FULL
	if code == TRANSFER_REFUSED:
		return MSG_TRANSFER_REFUSED
	if code == TRANSFER_BAD_REQUEST:
		return MSG_TRANSFER_BAD_REQUEST
	if code == TRANSFER_COOLDOWN:
		return MSG_TRANSFER_COOLDOWN
	if code == TRANSFER_DEAD:
		return MSG_TRANSFER_DEAD
	return MSG_UNSUPPORTED


def OnArrangeResult(code='0', moved='0', merged='0', units='0'):
	_state['arrangeUntil'] = 0.0
	chat.AppendChat(chat.CHAT_TYPE_INFO, ArrangeMessage(_int(code), max(0, _int(moved)), max(0, _int(merged))))


def OnTransferResult(op='0', code='0', units='0'):
	text = TransferMessage(_int(code))
	if text:
		chat.AppendChat(chat.CHAT_TYPE_INFO, text)
