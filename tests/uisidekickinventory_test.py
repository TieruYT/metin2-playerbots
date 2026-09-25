# The companion's bag and skill windows without a client: what they read from
# the server, what they send back and what they do with the cursor, against stub
# client modules. The widget stubs are strict - a method the stock ui.py does not
# have raises - and they keep positions and sizes, so the windows can be measured
# against an 800x600 screen. The cursor is a copy of the stock mousemodule.py's
# attach and let-go logic, which is what the module has to work with.
#
#     python tests/uisidekickinventory_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7-slim python tests/uisidekickinventory_test.py).
import io
import os
import re
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}

# player.SLOT_TYPE_* of the client (UserInterface/GameType.h, ESlotType).
SLOT_TYPE_NONE = 0
SLOT_TYPE_INVENTORY = 1
SLOT_TYPE_SKILL = 2
SLOT_TYPE_EMOTION = 3
SLOT_TYPE_SHOP = 4
SLOT_TYPE_QUICK_SLOT = 7
SLOT_TYPE_SAFEBOX = 8
SLOT_TYPE_PRIVATE_SHOP = 9
SLOT_TYPE_MALL = 10
SLOT_TYPE_DRAGON_SOUL_INVENTORY = 11

# Sizes the stubs give the pictures and buttons (the .sub files of the etc pack).
IMAGE_SIZES = {
	'd:/ymir work/ui/game/windows/equipment_base.sub': (156, 188),
	'd:/ymir work/ui/game/windows/money_icon.sub': (16, 16),
}
BUTTON_SIZES = {'small': (43, 21), 'middle': (61, 21), 'large': (88, 21), 'xlarge': (180, 25)}

# vnum -> (width, height) in cells, and its item_proto type
ITEM_SIZES = {19: (1, 3), 11209: (1, 2), 12009: (1, 1), 27001: (1, 1), 27002: (1, 1), 50300: (1, 1), 71084: (1, 1)}
ITEM_TYPE_WEAPON = 1
ITEM_TYPE_ARMOR = 2
ITEM_TYPE_USE = 3
ITEM_TYPE_UNIQUE = 16
ITEM_TYPE_SKILLBOOK = 17
ITEM_TYPES = {19: ITEM_TYPE_WEAPON, 11209: ITEM_TYPE_ARMOR, 12009: ITEM_TYPE_ARMOR, 27001: ITEM_TYPE_USE,
	27002: ITEM_TYPE_USE, 50300: ITEM_TYPE_SKILLBOOK, 71084: ITEM_TYPE_USE}

CTRL_KEYS = (29, 157)


def reset_state():
	STATE.clear()
	STATE.update({
		'now': 100.0, 'commands': [], 'questions': [], 'chat': [], 'sounds': [],
		'attachIcons': [], 'icons': {}, 'nextIcon': 1000, 'deleted': [], 'selected': 0,
		'pressed': set(), 'bag': {}, 'widgets': [], 'tooltips': [], 'skillNames': {},
	})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


# ---------------------------------------------------------------- widgets

class StubWindow(object):
	"""ui.Window as far as the windows use it, with geometry kept."""

	def __init__(self, *args, **kwargs):
		self.parent = None
		self.x = 0
		self.y = 0
		self.w = 0
		self.h = 0
		self.align = 'left'
		self.shown = False
		self.flags = []
		STATE['widgets'].append(self)

	def SetParent(self, parent):
		self.parent = parent

	def SetPosition(self, x, y):
		self.x, self.y = x, y

	def SetSize(self, w, h):
		self.w, self.h = w, h

	def GetWidth(self):
		return self.w

	def GetHeight(self):
		return self.h

	def GetGlobalPosition(self):
		x, y = self.x, self.y
		parent = self.parent
		while parent is not None:
			x += parent.x
			y += parent.y
			parent = parent.parent
		return (x, y)

	def SetCenterPosition(self, x=0, y=0):
		self.x = (800 - self.w) // 2 + x
		self.y = (600 - self.h) // 2 + y

	def AddFlag(self, flag):
		self.flags.append(flag)

	def Show(self):
		self.shown = True

	def Hide(self):
		self.shown = False

	def IsShow(self):
		return self.shown

	def SetTop(self):
		pass

	def Root(self):
		widget = self
		while widget.parent is not None:
			widget = widget.parent
		return widget

	def Rect(self):
		"""(left, top, right, bottom) relative to the root window."""
		root = self.Root()
		x, y = self.x, self.y
		parent = self.parent
		while parent is not None and parent is not root:
			x += parent.x
			y += parent.y
			parent = parent.parent
		if self.align == 'center':
			x -= self.w // 2
		elif self.align == 'right':
			x -= self.w
		return (x, y, x + self.w, y + self.h)


class StubBoard(StubWindow):
	def __init__(self, *args, **kwargs):
		StubWindow.__init__(self)
		self.title = ''
		self.closeEvent = None

	def SetTitleName(self, name):
		self.title = name

	def SetCloseEvent(self, event):
		self.closeEvent = event


class StubTextLine(StubWindow):
	def __init__(self, *args, **kwargs):
		StubWindow.__init__(self)
		self.text = ''
		self.color = None
		self.h = 12

	def SetText(self, text):
		self.text = text
		self.w = 6 * len(text)

	def GetText(self):
		return self.text

	def GetTextSize(self):
		return (self.w, self.h)

	def SetHorizontalAlignCenter(self):
		self.align = 'center'

	def SetHorizontalAlignRight(self):
		self.align = 'right'

	def SetPackedFontColor(self, color):
		self.color = color


class StubImageBox(StubWindow):
	def LoadImage(self, name):
		self.image = name
		self.w, self.h = IMAGE_SIZES.get(name, (32, 32))


class StubButton(StubWindow):
	def __init__(self, *args, **kwargs):
		StubWindow.__init__(self)
		self.text = ''
		self.pressed = False
		self.event = None
		self.eventArgs = ()

	def SetUpVisual(self, name):
		self.image = name
		for size, (w, h) in BUTTON_SIZES.items():
			if '/%s_button_' % size in name:
				self.w, self.h = w, h
		if 'tab_button_small' in name:
			self.w, self.h = 32, 19

	def SetOverVisual(self, name):
		pass

	def SetDownVisual(self, name):
		pass

	def SetText(self, text, *args, **kwargs):
		self.text = text

	def SAFE_SetEvent(self, event, *args):
		self.event = event
		self.eventArgs = args

	def Down(self):
		self.pressed = True

	def SetUp(self):
		self.pressed = False


class StubSlotWindow(StubWindow):
	def __init__(self, *args, **kwargs):
		StubWindow.__init__(self)
		self.slots = {}
		self.layout = {}
		self.buttonsShown = set()
		self.buttonImages = None
		self.events = {}
		self.baseImage = None

	def AppendSlot(self, index, x, y, w, h):
		self.layout[index] = (x, y, w, h)

	def SetSlotBaseImage(self, name, r, g, b, a):
		self.baseImage = name

	def _Event(name):
		def setter(self, event):
			self.events[name] = event
		return setter

	SetSelectEmptySlotEvent = _Event('selectEmpty')
	SetSelectItemSlotEvent = _Event('selectItem')
	SetUnselectItemSlotEvent = _Event('unselectItem')
	SetUseSlotEvent = _Event('use')
	SetOverInItemEvent = _Event('overIn')
	SetOverOutItemEvent = _Event('overOut')
	SetPressedSlotButtonEvent = _Event('pressedButton')

	def AppendSlotButton(self, up, over, down):
		self.buttonImages = (up, over, down)

	def HideAllSlotButton(self):
		self.buttonsShown = set()

	def ShowSlotButton(self, slot):
		self.buttonsShown.add(slot)

	def ClearSlot(self, slot):
		self.slots.pop(slot, None)

	def SetItemSlot(self, slot, vnum, count=0, diffuseColor=(1.0, 1.0, 1.0, 1.0), socket=None):
		if not vnum:
			self.slots.pop(slot, None)
			return
		self.slots[slot] = {'vnum': vnum, 'count': count, 'socket': socket, 'mask': None}

	def SetSlotMaskColorRaw(self, slot, r, g, b, a=0.3):
		if slot in self.slots:
			self.slots[slot]['mask'] = (r, g, b, a) if a > 0 else None

	def SetSkillSlotNew(self, slot, skillIndex, grade, level):
		self.slots[slot] = {'skill': skillIndex, 'grade': grade}

	def SetSlotCountNew(self, slot, grade, count):
		if slot in self.slots:
			self.slots[slot]['countNew'] = (grade, count)

	def RefreshSlot(self):
		pass

	def Click(self, kind, slot):
		self.events[kind](slot)


class StubGridSlotWindow(StubSlotWindow):
	def ArrangeSlot(self, start, xCount, yCount, xSize, ySize, xBlank, yBlank):
		self.start = start
		self.w, self.h = xCount * (xSize + xBlank), yCount * (ySize + yBlank)
		for i in range(xCount * yCount):
			self.layout[start + i] = ((i % xCount) * xSize, (i // xCount) * ySize, xSize, ySize)


class StubGauge(StubWindow):
	def MakeGauge(self, width, color):
		self.w, self.h = width, 8

	def SetPercentage(self, current, maximum):
		self.percentage = (current, maximum)


class StubQuestion(object):
	def __init__(self, *args, **kwargs):
		self.accept = None
		self.cancel = None
		self.opened = False
		STATE['questions'].append(self)

	def SetText(self, text):
		self.text = text

	def SetAcceptEvent(self, event):
		self.accept = event

	def SetCancelEvent(self, event):
		self.cancel = event

	def Open(self):
		self.opened = True

	def Close(self):
		self.opened = False


class StubToolTip(object):
	POSITIVE_COLOR = 0xff00ff00

	def __init__(self, *args, **kwargs):
		self.shown = False
		self.items = []
		self.lines = []
		self.forceDisable = True
		STATE['tooltips'].append(self)

	def SetCannotUseItemForceSetDisableColor(self, enable):
		self.forceDisable = enable

	def ClearToolTip(self):
		self.items = []
		self.lines = []

	def AddItemData(self, vnum, metinSlot, attrSlot=0, *args, **kwargs):
		self.items.append((vnum, list(metinSlot), list(attrSlot)))
		self.shown = True

	def AppendSpace(self, size):
		pass

	def AppendTextLine(self, text, color=None, *args, **kwargs):
		self.lines.append(text)

	def ShowToolTip(self):
		self.shown = True

	def HideToolTip(self):
		self.shown = False


# ---------------------------------------------------------------- the cursor

class StubMouseController(object):
	"""mousemodule.py's CMouseController: AttachObject and DeattachObject as the
	stock root has them, the type lists included."""

	def __init__(self):
		self.AttachedIconHandle = 0
		self.AttachedOwner = 0
		self.AttachedFlag = False
		self.AttachedType = 0
		self.AttachedSlotNumber = 0
		self.AttachedItemIndex = 0
		self.AttachedCount = 1
		self.LastAttachedSlotNumber = 0
		self.DeattachObject()

	def AttachObject(self, Owner, Type, SlotNumber, ItemIndex, count=0):
		self.LastAttachedSlotNumber = self.AttachedSlotNumber
		self.AttachedFlag = True
		self.AttachedOwner = Owner
		self.AttachedType = Type
		self.AttachedSlotNumber = SlotNumber
		self.AttachedItemIndex = ItemIndex
		self.AttachedCount = count
		width = 1
		height = 1
		if Type in (SLOT_TYPE_INVENTORY, SLOT_TYPE_PRIVATE_SHOP, SLOT_TYPE_SHOP, SLOT_TYPE_SAFEBOX, SLOT_TYPE_MALL,
				SLOT_TYPE_DRAGON_SOUL_INVENTORY):
			stub_item.SelectItem(self.AttachedItemIndex)
			self.AttachedIconHandle = stub_item.GetIconInstance()
			if not self.AttachedIconHandle:
				self.AttachedIconHandle = 0
				self.DeattachObject()
				return
			(width, height) = stub_item.GetItemSize()
		if not self.AttachedIconHandle:
			self.DeattachObject()
			return
		stub_wndMgr.AttachIcon(self.AttachedType, self.AttachedItemIndex, self.AttachedSlotNumber, width, height)

	def DeattachObject(self):
		self.LastAttachedSlotNumber = self.AttachedSlotNumber
		if self.AttachedIconHandle != 0:
			if self.AttachedType in (SLOT_TYPE_INVENTORY, SLOT_TYPE_PRIVATE_SHOP, SLOT_TYPE_SHOP, SLOT_TYPE_SAFEBOX,
					SLOT_TYPE_MALL):
				stub_item.DeleteIconInstance(self.AttachedIconHandle)
		self.AttachedFlag = False
		self.AttachedType = -1
		self.AttachedItemIndex = -1
		self.AttachedSlotNumber = -1
		self.AttachedIconHandle = 0

	def isAttached(self):
		return self.AttachedFlag

	def GetAttachedType(self):
		if not self.isAttached():
			return SLOT_TYPE_NONE
		return self.AttachedType

	def GetAttachedSlotNumber(self):
		if not self.isAttached():
			return 0
		return self.AttachedSlotNumber

	def GetAttachedItemIndex(self):
		if not self.isAttached():
			return 0
		return self.AttachedItemIndex

	def GetAttachedItemCount(self):
		if not self.isAttached():
			return 0
		return self.AttachedCount


def _select_item(vnum):
	STATE['selected'] = vnum


def _icon_instance():
	handle = STATE['nextIcon']
	STATE['nextIcon'] += 1
	STATE['icons'][handle] = STATE['selected']
	return handle


def _delete_icon(handle):
	if handle not in STATE['icons']:
		raise AssertionError('icon %r deleted twice or never made' % handle)
	del STATE['icons'][handle]
	STATE['deleted'].append(handle)


def _skill_name(vnum, grade=-1):
	if vnum not in STATE['skillNames']:
		raise RuntimeError('skill.GetSkillName - Failed to find skill by %d' % vnum)
	return STATE['skillNames'][vnum]


stub_item = module('item', SelectItem=_select_item, GetIconInstance=_icon_instance, DeleteIconInstance=_delete_icon,
	GetItemSize=lambda: ITEM_SIZES.get(STATE['selected'], (1, 1)),
	GetItemType=lambda: ITEM_TYPES.get(STATE['selected'], ITEM_TYPE_USE),
	ITEM_TYPE_WEAPON=ITEM_TYPE_WEAPON, ITEM_TYPE_ARMOR=ITEM_TYPE_ARMOR, ITEM_TYPE_UNIQUE=ITEM_TYPE_UNIQUE)
stub_wndMgr = module('wndMgr', AttachIcon=lambda *args: STATE['attachIcons'].append(args),
	GetScreenWidth=lambda: 800, GetScreenHeight=lambda: 600)


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'], IsPressed=lambda key: key in STATE['pressed'],
		DIK_LCONTROL=CTRL_KEYS[0], DIK_RCONTROL=CTRL_KEYS[1])
	sys.modules['net'] = module('net', SendChatPacket=lambda text: STATE['commands'].append(text))
	sys.modules['chat'] = module('chat', CHAT_TYPE_INFO=1, AppendChat=lambda kind, text: STATE['chat'].append(text))
	sys.modules['snd'] = module('snd', PlaySound=lambda name: STATE['sounds'].append(name))
	sys.modules['skill'] = module('skill', GetSkillName=_skill_name)
	sys.modules['item'] = stub_item
	sys.modules['wndMgr'] = stub_wndMgr
	sys.modules['player'] = module('player', SLOT_TYPE_NONE=SLOT_TYPE_NONE, SLOT_TYPE_INVENTORY=SLOT_TYPE_INVENTORY,
		SLOT_TYPE_SAFEBOX=SLOT_TYPE_SAFEBOX, ITEM_MONEY=1, INVENTORY_DEFAULT_MAX_NUM=180, INVENTORY_PAGE_SIZE=45,
		METIN_SOCKET_MAX_NUM=3, ATTRIBUTE_SLOT_MAX_NUM=7, GetItemCount=lambda cell: STATE['bag'].get(cell, 0))
	sys.modules['mouseModule'] = module('mouseModule', mouseController=None)
	ui = module('ui', BoardWithTitleBar=StubBoard, ThinBoard=StubWindow, TextLine=StubTextLine, Button=StubButton,
		RadioButton=StubButton, Gauge=StubGauge, ImageBox=StubImageBox, SlotWindow=StubSlotWindow,
		GridSlotWindow=StubGridSlotWindow)
	setattr(ui, '__mem_func__', lambda func: func)
	sys.modules['ui'] = ui
	sys.modules['uiCommon'] = module('uiCommon', QuestionDialog=StubQuestion)
	sys.modules['uiToolTip'] = module('uiToolTip', ItemToolTip=StubToolTip)


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import clientclock  # noqa: E402
import uisidekick  # noqa: E402
import uisidekickinventory as inv  # noqa: E402

mouse = sys.modules['mouseModule']


def hexed(text):
	return ''.join('%02x' % ord(c) for c in text)


def item_line(pos, vnum, count=1, flags=0, sockets=(0, 0, 0), attrs='-', gen=1):
	return [str(gen), str(pos), str(vnum), str(count), str(flags)] + [str(s) for s in sockets] + [attrs]


def full_picture(items, gold=1234567, gen=1, bag_cells=180):
	inv.OnEqBegin('1', str(gen), str(bag_cells), '45')
	for line in items:
		inv.OnEqItem(*line)
	inv.OnEqEnd(str(gen), str(gold))


def click(button):
	button.event(*button.eventArgs)


class Base(unittest.TestCase):
	def setUp(self):
		# The last test's windows go first, with the stubs they were made with,
		# and a player's item a test left on the cursor; then no icon may be left.
		uisidekick.Destroy()
		inv.Destroy()
		if mouse.mouseController is not None:
			mouse.mouseController.DeattachObject()
		self.assertEqual(STATE['icons'], {})
		reset_state()
		clientclock.Reset()
		mouse.mouseController = StubMouseController()

	def advance(self, seconds):
		STATE['now'] += seconds

	def pump(self, window, seconds=0.35):
		self.advance(seconds)
		window.OnUpdate()

	def orders(self):
		return [c for c in STATE['commands'] if c not in ('/towarzysz eq', '/towarzysz umiejetnosci', '/towarzysz okno')]


# ---------------------------------------------------------------- parsing

class ParsingTest(Base):
	def test_attributes_come_as_seven_pairs_or_a_dash(self):
		self.assertEqual(inv.ParseAttrs('-'), [(0, 0)] * 7)
		self.assertEqual(inv.ParseAttrs(''), [(0, 0)] * 7)
		self.assertEqual(inv.ParseAttrs('1:500,7:10,0:0,0:0,0:0,0:0,0:0')[:2], [(1, 500), (7, 10)])
		self.assertEqual(inv.ParseAttrs('72:-5'), [(72, -5)] + [(0, 0)] * 6)
		# A broken pair is an empty line and the rest keep their places.
		self.assertEqual(inv.ParseAttrs('x:1,3:4,5,6:7:8,-1:2')[:5], [(0, 0), (3, 4), (0, 0), (0, 0), (0, 0)])
		self.assertEqual(len(inv.ParseAttrs(','.join(['1:1'] * 12))), 7)

	def test_skill_levels_as_the_game_writes_them(self):
		cases = {0: '0', 1: '1', 17: '17', 19: '19', 20: 'M1', 25: 'M6', 29: 'M10', 30: 'G1', 39: 'G10', 40: 'P'}
		for level, text in cases.items():
			self.assertEqual(inv.SkillLevelText(level), text)
		# A server that sent the step within the grade is read as well.
		self.assertEqual(inv.SkillLevelText(1, 1), 'M1')
		self.assertEqual(inv.SkillLevelText(3, 2), 'G3')
		self.assertEqual(inv.SkillLevelText(0, 3), 'P')
		self.assertEqual(inv.SkillGradeStep(25, 1), (1, 6))
		self.assertEqual(inv.SkillGradeStep(40, 3), (3, 1))

	def test_a_point_goes_to_a_normal_skill_under_seventeen(self):
		self.assertTrue(inv.CanAddSkillPoint(1, 0, 0))
		self.assertTrue(inv.CanAddSkillPoint(3, 16, 0))
		self.assertFalse(inv.CanAddSkillPoint(0, 5, 0))
		self.assertFalse(inv.CanAddSkillPoint(3, 17, 0))
		self.assertFalse(inv.CanAddSkillPoint(3, 20, 1))

	def test_the_owners_marks(self):
		self.assertIsNone(inv.MaskFor(0))
		self.assertEqual(inv.MaskFor(1), inv.MASK_PINNED)
		self.assertEqual(inv.MaskFor(2), inv.MASK_GIFT)
		self.assertEqual(inv.MaskFor(3), inv.MASK_PINNED_GIFT)
		self.assertEqual(inv.MaskFor(4), inv.MASK_UNWANTED)
		self.assertEqual(inv.MaskFor(6), inv.MASK_UNWANTED_GIFT)
		self.assertEqual(inv.ToolTipLines(1004, 1),
			[inv.TEXT_TIP_PINNED, inv.TEXT_TIP_UNPIN, inv.TEXT_TIP_UNEQUIP])
		self.assertEqual(inv.ToolTipLines(5, 4 | 2),
			[inv.TEXT_TIP_UNWANTED, inv.TEXT_TIP_GIFT, inv.TEXT_TIP_UNPIN, inv.TEXT_TIP_EQUIP])
		self.assertEqual(inv.ToolTipLines(5, 0), [inv.TEXT_TIP_EQUIP])
		# "Zaloz" is offered for what goes on a body only; the order goes regardless.
		self.assertEqual(inv.ToolTipLines(5, 0, False), [])
		self.assertEqual(inv.ToolTipLines(1004, 0, False), [inv.TEXT_TIP_UNEQUIP])
		self.assertTrue(inv.IsWearable(19))
		self.assertTrue(inv.IsWearable(12009))
		self.assertFalse(inv.IsWearable(27001))
		# The coordinator's words, in the client's CP1250.
		self.assertEqual(inv.TEXT_TIP_PINNED, 'Za\xb3o\xbfone przez ciebie - towarzysz tego nie zdejmie')
		self.assertEqual(inv.TEXT_TIP_UNWANTED, 'Zdj\xeate przez ciebie - towarzysz sam tego nie za\xb3o\xbfy')
		self.assertEqual(inv.TEXT_TIP_GIFT, 'Prezent od ciebie')

	def test_the_cursor_type_is_nobody_elses(self):
		# player.SLOT_TYPE_* runs 0-11 (SLOT_TYPE_MAX 12); SlotTypeToInvenType and
		# the quick slot packet take it as a BYTE, and the server refuses a
		# quickslot type from QUICKSLOT_TYPE_MAX_NUM (4) up.
		kind = inv.SLOT_TYPE_SIDEKICK
		self.assertNotIn(kind, range(12))
		self.assertGreaterEqual(kind & 0xff, 12)
		self.assertGreaterEqual(kind & 0xff, 4)

	def test_result_texts(self):
		self.assertEqual(inv.ResultText(0, 'Za\xb3o\xbfy to.'), 'Za\xb3o\xbfy to.')
		self.assertEqual(inv.ResultText(3, ''), 'Tam nic nie ma.')
		self.assertEqual(inv.ResultText(9, ''), 'Z\xb3e polecenie.')
		self.assertEqual(inv.ResultText(42, ''), inv.TEXT_REFUSED)
		# The server cuts a text at 150 bytes; the module reads that much and more.
		self.assertEqual(uisidekick.DecodeText(hexed('A' * 150), inv.RESULT_TEXT_BYTES), 'A' * 150)
		self.assertEqual(uisidekick.DecodeText(hexed('A' * 150)), '')


# ---------------------------------------------------------------- the model

class EquipmentModelTest(Base):
	def test_a_whole_picture_then_changes(self):
		full_picture([item_line(3, 19, flags=1), item_line(1004, 19, flags=3), item_line(50, 27001, count=200)])
		model = inv._equipment
		self.assertEqual(model.state, inv.STATE_OK)
		self.assertEqual(sorted(model.items), [3, 50, 1004])
		self.assertEqual(model.items[50]['count'], 200)
		self.assertEqual(model.gold, 1234567)
		self.assertEqual(model.Pages(), 4)
		# A package of changes: the same gen on its Item, Empty and End.
		inv.OnEqItem(*item_line(7, 12009, gen=2))
		inv.OnEqEmpty('2', '3')
		inv.OnEqEnd('2', '99')
		self.assertEqual(sorted(model.items), [7, 50, 1004])
		self.assertEqual(model.gold, 99)

	def test_half_a_picture_is_never_shown(self):
		full_picture([item_line(3, 19)])
		inv.OnEqBegin('1', '5', '180', '45')
		inv.OnEqItem(*item_line(9, 12009, gen=5))
		# Until its End, the old picture stands.
		self.assertEqual(sorted(inv._equipment.items), [3])
		inv.OnEqEnd('5', '10')
		self.assertEqual(sorted(inv._equipment.items), [9])

	def test_a_short_or_broken_line_is_read_as_far_as_it_goes(self):
		full_picture([])
		for line in (['1'], ['1', '2', '3'], ['1', 'x', '19', '1'], ['1', '180', '19', '1'], ['1', '999', '19', '1'],
				['1', '1032', '19', '1'], ['1', '-1', '19', '1']):
			inv.OnEqItem(*line)
		inv.OnEqEnd('1', '0')
		self.assertEqual(inv._equipment.items, {})
		inv.OnEqItem('2', '4', '27001', '0')
		inv.OnEqItem('2', '1031', '19', '1', 'z')
		inv.OnEqEnd('2', 'lots')
		entry = inv._equipment.items[4]
		self.assertEqual((entry['count'], entry['flags'], entry['sockets']), (1, 0, [0, 0, 0]))
		self.assertEqual(entry['attrs'], [(0, 0)] * 7)
		self.assertEqual(inv._equipment.items[1031]['flags'], 0)
		self.assertEqual(inv._equipment.gold, 0)
		# A vnum of nothing empties the place.
		inv.OnEqItem('3', '4', '0', '0')
		inv.OnEqEnd('3', '5')
		self.assertNotIn(4, inv._equipment.items)
		# Lines with nothing at all do no harm.
		inv.OnEqEmpty()
		inv.OnEqEnd()
		inv.OnEqBegin()
		inv.OnEqNone()
		inv.OnEqResult()

	def test_the_bag_as_the_server_measures_it(self):
		full_picture([], bag_cells=90)
		self.assertEqual(inv._equipment.Pages(), 2)
		inv.OnEqBegin('1', '2', '0', '0')
		inv.OnEqEnd('2', '0')
		self.assertEqual((inv._equipment.bagCells, inv._equipment.pageCells), (180, 45))
		inv.OnEqBegin('1', '3', '999', '45')
		inv.OnEqEnd('3', '0')
		self.assertEqual(inv._equipment.bagCells, 180)

	def test_no_companion_and_another_protocol(self):
		full_picture([item_line(3, 19)])
		inv.OnEqNone('1', '1')
		self.assertEqual((inv._equipment.state, inv._equipment.reason), (inv.STATE_NONE, 1))
		self.assertEqual(inv._equipment.items, {})
		self.assertEqual(inv._skills.state, inv.STATE_NONE)
		# Changes for a companion that is gone are not applied.
		inv.OnEqItem(*item_line(3, 19))
		self.assertEqual(inv._equipment.items, {})
		inv.OnEqBegin('2', '1', '180', '45')
		self.assertEqual(inv._equipment.state, inv.STATE_OTHER_PROTOCOL)
		full_picture([item_line(3, 19)])
		self.assertEqual(inv._equipment.state, inv.STATE_OK)
		self.assertEqual(sorted(inv._equipment.items), [3])


class SkillModelTest(Base):
	def test_the_list_and_its_points(self):
		inv.OnSkillBegin('1', '3', '3', '2', '0')
		inv.OnSkill('106', '17', '0')
		inv.OnSkill('107', '25', '1')
		inv.OnSkill('107', '26', '1')
		inv.OnSkill('108', '40', '3')
		inv.OnSkill('0', '5', '0')
		inv.OnSkill('x')
		inv.OnSkillEnd()
		model = inv._skills
		self.assertEqual((model.points, model.job, model.group, model.manual), (3, 3, 2, 0))
		self.assertEqual(model.skills, [(106, 17, 0), (107, 26, 1), (108, 40, 3)])

	def test_nothing_is_taken_outside_a_list(self):
		inv.OnSkill('106', '17', '0')
		inv.OnSkillEnd()
		self.assertEqual(inv._skills.state, inv.STATE_WAITING)
		inv.OnSkillBegin('2', '3', '3', '2', '0')
		self.assertEqual(inv._skills.state, inv.STATE_OTHER_PROTOCOL)
		inv.OnSkillBegin()
		inv.OnSkill()


# ---------------------------------------------------------------- the bag window

class EquipmentWindowTest(Base):
	def setUp(self):
		Base.setUp(self)
		self.window = inv.GetEquipmentWindow()

	def open_with(self, items, **kwargs):
		inv.ToggleEquipmentWindow()
		full_picture(items, **kwargs)
		STATE['commands'] = []
		self.advance(1.0)

	def test_opening_asks_for_the_whole_picture_and_then_polls(self):
		inv.ToggleEquipmentWindow()
		self.assertTrue(self.window.IsShow())
		self.assertEqual(STATE['commands'], ['/towarzysz eq 1'])
		self.assertEqual(self.window.StatusText(), inv.TEXT_WAITING)
		self.pump(self.window, 1.0)
		self.assertEqual(len(STATE['commands']), 1)
		self.pump(self.window, 0.6)
		self.assertEqual(STATE['commands'][-1], '/towarzysz eq')
		# A poll nothing answers is not waited for.
		self.pump(self.window, 1.5)
		self.assertEqual(STATE['commands'][-2:], ['/towarzysz eq', '/towarzysz eq'])
		inv.ToggleEquipmentWindow()
		self.assertFalse(self.window.IsShow())

	def test_the_picture_fills_the_grid_the_gear_and_the_yang(self):
		self.open_with([item_line(3, 19, flags=2), item_line(50, 27001, count=200), item_line(1004, 19, flags=1),
			item_line(1000, 11209, flags=4), item_line(1019, 12009)])
		bag = self.window.bagSlots.slots
		self.assertEqual(sorted(bag), [3])
		self.assertEqual(bag[3]['vnum'], 19)
		self.assertEqual(bag[3]['mask'], inv.MASK_GIFT)
		self.assertEqual(bag[3]['socket'], (0, 0, 0))
		gear = self.window.equipSlots.slots
		self.assertEqual(gear[1004]['mask'], inv.MASK_PINNED)
		self.assertEqual(gear[1000]['mask'], inv.MASK_UNWANTED)
		# A costume comes and is kept, but this window draws no costume slot.
		self.assertNotIn(1019, gear)
		self.assertIn(1019, inv._equipment.items)
		self.assertEqual(self.window.goldLine.text, 'Yang: 1.234.567')
		self.assertEqual(self.window.StatusText(), '')
		# Page II holds cells 45-89: 50 is its sixth, a stack of 200 shows its count.
		click(self.window.tabs[1])
		self.assertEqual(sorted(self.window.bagSlots.slots), [5])
		self.assertEqual(self.window.bagSlots.slots[5]['count'], 200)
		self.assertEqual([t.pressed for t in self.window.tabs], [False, True, False, False])
		# A single item shows no count.
		click(self.window.tabs[0])
		self.assertEqual(bag[3]['count'], 0)

	def test_tabs_follow_the_number_of_pages(self):
		self.open_with([], bag_cells=90)
		self.assertEqual([t.shown for t in self.window.tabs], [True, True, False, False])
		self.window.SetPage(3)
		self.assertEqual(self.window.page, 1)

	def test_moving_in_the_bag_through_the_cursor(self):
		self.open_with([item_line(3, 19), item_line(8, 27001, count=20)])
		self.window.bagSlots.Click('selectItem', 3)
		controller = mouse.mouseController
		self.assertTrue(controller.isAttached())
		self.assertEqual(controller.GetAttachedType(), inv.SLOT_TYPE_SIDEKICK)
		self.assertEqual(controller.GetAttachedSlotNumber(), 3)
		# wndMgr is told the weapon's real size last, one cell by AttachObject first.
		self.assertEqual(STATE['attachIcons'][-2:], [(inv.SLOT_TYPE_SIDEKICK, 19, 3, 1, 1), (inv.SLOT_TYPE_SIDEKICK, 19, 3, 1, 3)])
		self.assertEqual(STATE['sounds'], ['sound/ui/pick.wav'])
		icon = controller.AttachedIconHandle
		self.window.bagSlots.Click('selectEmpty', 10)
		self.assertEqual(self.orders(), ['/towarzysz eq ruch 3 10'])
		self.assertFalse(controller.isAttached())
		# The stock let-go does not delete an icon of a type it does not list:
		# the module does, once.
		self.assertEqual(STATE['deleted'], [icon])
		self.assertEqual(STATE['icons'], {})
		# Onto a stack, and onto its own place (nothing).
		self.advance(0.35)
		self.window.bagSlots.Click('selectItem', 8)
		self.window.bagSlots.Click('selectItem', 3)
		self.assertEqual(self.orders()[-1], '/towarzysz eq ruch 8 3')
		self.advance(0.35)
		self.window.bagSlots.Click('selectItem', 8)
		self.window.bagSlots.Click('selectItem', 8)
		self.assertEqual(len(self.orders()), 2)
		self.assertFalse(controller.isAttached())

	def test_on_and_off_the_body(self):
		self.open_with([item_line(3, 19), item_line(1004, 19, flags=1), item_line(1007, 27002)])
		self.window.bagSlots.Click('selectItem', 3)
		self.window.equipSlots.Click('selectItem', 1004)
		self.assertEqual(self.orders(), ['/towarzysz eq ruch 3 1004'])
		self.advance(0.35)
		self.window.equipSlots.Click('selectItem', 1004)
		self.window.bagSlots.Click('selectEmpty', 12)
		self.assertEqual(self.orders()[-1], '/towarzysz eq ruch 1004 12')
		# Worn onto worn is not an order the server takes.
		self.advance(0.35)
		self.window.equipSlots.Click('selectItem', 1007)
		self.window.equipSlots.Click('selectEmpty', 1008)
		self.assertEqual(len(self.orders()), 2)
		self.assertEqual(self.window.StatusText(), inv.TEXT_WEAR_TO_WEAR)
		self.assertFalse(mouse.mouseController.isAttached())

	def test_a_right_click_puts_on_and_takes_off(self):
		self.open_with([item_line(3, 19), item_line(1004, 19)])
		self.window.bagSlots.Click('unselectItem', 3)
		self.advance(0.35)
		self.window.equipSlots.Click('unselectItem', 1004)
		self.assertEqual(self.orders(), ['/towarzysz eq ruch 3 -1', '/towarzysz eq ruch 1004 -1'])
		# With something on the cursor it only lets go.
		self.advance(0.35)
		self.window.bagSlots.Click('selectItem', 3)
		self.window.equipSlots.Click('unselectItem', 1004)
		self.assertFalse(mouse.mouseController.isAttached())
		self.assertEqual(len(self.orders()), 2)
		# A double click: the first click took it, the second puts it on.
		self.advance(0.35)
		self.window.bagSlots.Click('selectItem', 3)
		self.window.bagSlots.Click('use', 3)
		self.assertEqual(self.orders()[-1], '/towarzysz eq ruch 3 -1')
		self.assertFalse(mouse.mouseController.isAttached())
		self.assertEqual(STATE['icons'], {})
		# A double click whose first click dropped something is that drop and no more.
		self.advance(0.35)
		STATE['bag'] = {12: 1}
		mouse.mouseController.AttachObject(None, SLOT_TYPE_INVENTORY, 12, 27001, 1)
		self.window.bagSlots.Click('selectEmpty', 9)
		self.window.bagSlots.Click('use', 3)
		self.assertEqual(self.orders()[-1], '/towarzysz eq daj 12 9')
		self.advance(0.6)
		self.window.bagSlots.Click('use', 3)
		self.assertEqual(self.orders()[-1], '/towarzysz eq ruch 3 -1')

	def test_the_players_item_is_given(self):
		self.open_with([item_line(3, 19)])
		controller = mouse.mouseController
		STATE['bag'] = {12: 1, 14: 200, 15: 50}
		controller.AttachObject(None, SLOT_TYPE_INVENTORY, 12, 27001, 1)
		self.window.bagSlots.Click('selectEmpty', 20)
		self.assertEqual(self.orders(), ['/towarzysz eq daj 12 20'])
		self.assertFalse(controller.isAttached())
		self.advance(0.35)
		controller.AttachObject(None, SLOT_TYPE_INVENTORY, 14, 27001, 200)
		self.window.equipSlots.Click('selectEmpty', 1000)
		self.assertEqual(self.orders()[-1], '/towarzysz eq daj 14 1000')
		# A split stack, a worn piece and yang are not what "daj" carries.
		self.advance(0.35)
		controller.AttachObject(None, SLOT_TYPE_INVENTORY, 15, 27001, 20)
		self.window.bagSlots.Click('selectEmpty', 21)
		self.assertEqual(self.window.StatusText(), inv.TEXT_WHOLE_STACK)
		controller.AttachObject(None, SLOT_TYPE_INVENTORY, 230, 19, 1)
		self.window.bagSlots.Click('selectItem', 3)
		self.assertEqual(self.window.StatusText(), inv.TEXT_ONLY_FROM_BAG)
		controller.AttachObject(None, SLOT_TYPE_INVENTORY, 12, 1, 5000)
		self.window.bagSlots.Click('selectEmpty', 22)
		self.assertEqual(self.window.StatusText(), inv.TEXT_NO_GOLD)
		self.assertEqual(len(self.orders()), 2)
		self.assertFalse(controller.isAttached())
		# Anything else on the cursor is let go.
		controller.AttachObject(None, SLOT_TYPE_SAFEBOX, 3, 27001, 1)
		self.window.bagSlots.Click('selectEmpty', 22)
		self.assertFalse(controller.isAttached())
		self.assertEqual(len(self.orders()), 2)

	def test_the_companions_item_is_taken_into_the_players_bag(self):
		self.open_with([item_line(3, 19), item_line(1004, 19)])
		self.window.bagSlots.Click('selectItem', 3)
		controller = mouse.mouseController
		# uiinventory.py hands this every drop and lets go itself when it says True.
		self.assertTrue(inv.DropIntoPlayerBag(controller.GetAttachedType(), controller.GetAttachedSlotNumber(), 17))
		controller.DeattachObject()
		self.assertEqual(self.orders(), ['/towarzysz eq wez 3 17'])
		self.window.OnUpdate()
		self.assertEqual(STATE['icons'], {})
		self.advance(0.35)
		self.window.equipSlots.Click('selectItem', 1004)
		# The player's own equipment, costume or horse page: the first free cell.
		self.assertTrue(inv.DropIntoPlayerBag(controller.GetAttachedType(), controller.GetAttachedSlotNumber(), 229))
		self.assertEqual(self.orders()[-1], '/towarzysz eq wez 1004 -1')
		self.assertFalse(inv.DropIntoPlayerBag(SLOT_TYPE_INVENTORY, 3, 17))
		self.assertFalse(inv.DropIntoPlayerBag(SLOT_TYPE_SAFEBOX, 3, 17))
		self.assertEqual(len(self.orders()), 2)

	def test_unpinning(self):
		self.open_with([item_line(1004, 19, flags=1), item_line(5, 27001, flags=4), item_line(6, 27002, flags=2),
			item_line(1000, 11209)])
		STATE['pressed'] = {CTRL_KEYS[0]}
		self.window.equipSlots.Click('selectItem', 1004)
		self.assertFalse(mouse.mouseController.isAttached())
		self.advance(0.35)
		self.window.bagSlots.Click('unselectItem', 5)
		self.assertEqual(self.orders(), ['/towarzysz eq odepnij 1004', '/towarzysz eq odepnij 5'])
		# A gift alone has nothing to unpin: Ctrl + click picks it up as ever.
		self.window.bagSlots.Click('selectItem', 6)
		self.assertTrue(mouse.mouseController.isAttached())
		mouse.mouseController.DeattachObject()
		STATE['pressed'] = set()
		# The button: with the piece on the cursor, or a word on how.
		click(self.window.unpinButton)
		self.assertEqual(self.window.StatusText(), inv.TEXT_UNPIN_HOW)
		self.advance(0.35)
		self.window.equipSlots.Click('selectItem', 1004)
		click(self.window.unpinButton)
		self.assertEqual(self.orders()[-1], '/towarzysz eq odepnij 1004')
		self.assertFalse(mouse.mouseController.isAttached())
		self.window.equipSlots.Click('selectItem', 1000)
		click(self.window.unpinButton)
		self.assertEqual(self.window.StatusText(), inv.TEXT_NOT_PINNED)
		self.assertEqual(len(self.orders()), 3)

	def test_the_tooltip(self):
		self.open_with([item_line(1004, 19, flags=1, sockets=(28001, 0, 0), attrs='7:10,63:5,0:0,0:0,0:0,0:0,0:0'),
			item_line(4, 27001, flags=4)])
		tooltip = self.window.tooltip
		self.assertFalse(tooltip.forceDisable)
		self.window.equipSlots.Click('overIn', 1004)
		self.assertTrue(tooltip.shown)
		self.assertEqual(tooltip.items[-1][0], 19)
		self.assertEqual(tooltip.items[-1][1], [28001, 0, 0])
		self.assertEqual(tooltip.items[-1][2][:2], [(7, 10), (63, 5)])
		self.assertEqual(len(tooltip.items[-1][2]), 7)
		self.assertEqual(tooltip.lines, [inv.TEXT_TIP_PINNED, inv.TEXT_TIP_UNPIN, inv.TEXT_TIP_UNEQUIP])
		self.window.equipSlots.events['overOut']()
		self.assertFalse(tooltip.shown)
		self.window.bagSlots.Click('overIn', 4)
		self.assertEqual(tooltip.lines, [inv.TEXT_TIP_UNWANTED, inv.TEXT_TIP_UNPIN])
		# Nothing over an item while one is on the cursor.
		self.window.bagSlots.Click('selectItem', 4)
		tooltip.HideToolTip()
		self.window.equipSlots.Click('overIn', 1004)
		self.assertFalse(tooltip.shown)

	def test_the_servers_answer_goes_to_the_status_line(self):
		self.open_with([item_line(3, 19)])
		inv.OnEqResult('0', hexed('Za\xb3o\xbfy to, jak tylko sko\xf1czy cios.'))
		self.assertEqual(self.window.StatusText(), 'Za\xb3o\xbfy to, jak tylko sko\xf1czy cios.')
		self.assertEqual(self.window.statusLines[0].color, inv.COLOR_NORMAL)
		inv.OnEqResult('2', hexed('Towarzysz nie mo\xbfe tego nosi\xe6.'))
		self.assertEqual(self.window.statusLines[0].color, inv.COLOR_BAD)
		inv.OnEqResult('3', '-')
		self.assertEqual(self.window.StatusText(), 'Tam nic nie ma.')
		self.assertEqual(STATE['chat'], [])
		# A long answer takes both lines, word by word.
		answer = 'Odpiete. Towarzysz znow sam wybiera, co tam nosi - sprobuj za chwile.'
		inv.OnEqResult('0', hexed(answer))
		self.assertEqual(self.window.StatusText(), answer)
		self.assertTrue(all(line.text for line in self.window.statusLines))
		self.assertEqual(STATE['chat'], [])
		# Longer than both is cut there and goes whole to the chat.
		long_text = 'Towarzysz jest teraz zajety (handel, magazyn albo kowal) - sprobuj za chwile, ' 			'a jak to nie pomoze, zamknij okno i otworz je jeszcze raz.'
		inv.OnEqResult('2', hexed(long_text))
		self.assertTrue(self.window.StatusText().endswith('...'))
		for line in self.window.statusLines:
			self.assertLessEqual(line.w, self.window.WIDTH - 20)
		self.assertEqual(STATE['chat'], [long_text])
		# It gives way to the plain status after a while.
		self.pump(self.window, inv.STATUS_SECONDS + 0.1)
		self.assertEqual(self.window.StatusText(), '')
		# With no window open, the chat.
		self.window.Close()
		inv.OnEqResult('1', hexed('Towarzysza nie ma teraz w grze.'))
		self.assertEqual(STATE['chat'][-1], 'Towarzysza nie ma teraz w grze.')

	def test_an_item_the_ai_moves_is_let_go(self):
		self.open_with([item_line(3, 19), item_line(4, 27001)])
		self.window.bagSlots.Click('selectItem', 3)
		inv.OnEqEmpty('2', '3')
		inv.OnEqItem(*item_line(9, 19, gen=2))
		inv.OnEqEnd('2', '0')
		self.assertFalse(mouse.mouseController.isAttached())
		self.assertEqual(self.window.StatusText(), inv.TEXT_ITEM_MOVED)
		self.assertEqual(STATE['icons'], {})
		# An unrelated change leaves the cursor alone.
		self.window.bagSlots.Click('selectItem', 4)
		inv.OnEqItem(*item_line(9, 12009, gen=3))
		inv.OnEqEnd('3', '0')
		self.assertTrue(mouse.mouseController.isAttached())
		# A companion gone takes it off the cursor, and the window says it is gone.
		inv.OnEqNone('1', '1')
		self.assertFalse(mouse.mouseController.isAttached())
		self.assertEqual(STATE['icons'], {})
		self.assertEqual(self.window.StatusText(), inv.TEXT_NONE[1])

	def test_closing_lets_go_and_deletes_the_icon(self):
		self.open_with([item_line(3, 19)])
		self.window.bagSlots.Click('selectItem', 3)
		self.assertEqual(len(STATE['icons']), 1)
		self.window.OnPressEscapeKey()
		self.assertFalse(self.window.IsShow())
		self.assertFalse(mouse.mouseController.isAttached())
		self.assertEqual(STATE['icons'], {})
		# A player's item on the cursor is not the window's to drop.
		mouse.mouseController.AttachObject(None, SLOT_TYPE_INVENTORY, 12, 27001, 1)
		inv.ToggleEquipmentWindow()
		inv.ToggleEquipmentWindow()
		self.assertTrue(mouse.mouseController.isAttached())

	def test_nothing_is_attached_over_something_else(self):
		self.open_with([item_line(3, 19)])
		controller = mouse.mouseController
		controller.AttachObject(None, SLOT_TYPE_SAFEBOX, 1, 27001, 1)
		self.assertTrue(controller.isAttached())
		self.assertFalse(inv.AttachItem(self.window, 3, 19, 1))
		self.assertEqual(controller.GetAttachedType(), SLOT_TYPE_SAFEBOX)
		controller.DeattachObject()
		# An icon the item table has not got is no attachment either.
		saved = stub_item.GetIconInstance
		stub_item.GetIconInstance = lambda: 0
		try:
			self.assertFalse(inv.AttachItem(self.window, 3, 19, 1))
		finally:
			stub_item.GetIconInstance = saved
		self.assertFalse(controller.isAttached())

	def test_no_companion(self):
		inv.ToggleEquipmentWindow()
		for reason, text in enumerate(inv.TEXT_NONE):
			inv.OnEqNone('1', str(reason))
			self.assertEqual(self.window.StatusText(), text)
		self.assertEqual(self.window.goldLine.text, '')
		inv.OnEqBegin('7', '1', '180', '45')
		self.assertEqual(self.window.StatusText(), inv.TEXT_OTHER_PROTOCOL)


# ---------------------------------------------------------------- the skill window

class SkillWindowTest(Base):
	def setUp(self):
		Base.setUp(self)
		self.window = inv.GetSkillWindow()
		STATE['skillNames'] = {106: 'Ci\xeacie', 107: 'Skok', 108: 'Szar\xbfa'}

	def list(self, points=3, manual=0, skills=((106, 17, 0), (107, 25, 1), (108, 40, 3), (109, 5, 0))):
		inv.OnSkillBegin('1', str(points), '0', '1', str(manual))
		for vnum, level, grade in skills:
			inv.OnSkill(str(vnum), str(level), str(grade))
		inv.OnSkillEnd()

	def test_opening_asks_for_the_list_and_then_polls(self):
		inv.ToggleSkillWindow()
		self.assertEqual(STATE['commands'], ['/towarzysz umiejetnosci'])
		self.pump(self.window, 2.0)
		self.assertEqual(len(STATE['commands']), 1)
		self.pump(self.window, 1.1)
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci')

	def test_the_list(self):
		inv.ToggleSkillWindow()
		self.list()
		w = self.window
		self.assertEqual(w.classLine.text, 'Wojownik (Cia\xb3o)')
		self.assertEqual(w.pointsLine.text, 'Wolne punkty: 3')
		self.assertEqual([line.text for line in w.nameLines[:5]], ['Ci\xeacie', 'Skok', 'Szar\xbfa', 'Umiej\xeatno\x9c\xe6 109', ''])
		self.assertEqual([line.text for line in w.levelLines[:5]], ['17', 'M6', 'P', '5', ''])
		slots = w.skillSlots.slots
		self.assertEqual(slots[1], {'skill': 107, 'grade': 1, 'countNew': (1, 6)})
		self.assertEqual(slots[2]['countNew'], (3, 1))
		self.assertNotIn(4, slots)
		# "+" where a point can go: under seventeen, normal grade.
		self.assertEqual(w.skillSlots.buttonsShown, set([3]))
		self.assertEqual(w.skillSlots.buttonImages[0], 'd:/ymir work/ui/game/windows/btn_plus_up.sub')
		self.advance(0.35)
		w.skillSlots.events['pressedButton'](3)
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci dodaj 109')
		# No points, no "+".
		self.list(points=0)
		self.assertEqual(w.skillSlots.buttonsShown, set())

	def test_who_spends_the_points(self):
		inv.ToggleSkillWindow()
		self.list(manual=0)
		self.assertEqual(self.window.manualButton.text, 'Punkty rozdaj\xea sam: nie')
		self.assertEqual(self.window.StatusText(), inv.TEXT_AI_SPENDS)
		self.advance(0.35)
		click(self.window.manualButton)
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci reczne 1')
		# The first "+" switches it to the owner; the server says so.
		self.advance(0.35)
		self.window.skillSlots.events['pressedButton'](3)
		inv.OnEqResult('0', hexed('Od teraz ty rozdajesz punkty.'))
		self.assertEqual(self.window.StatusText(), 'Od teraz ty rozdajesz punkty.')
		self.list(manual=1)
		self.assertEqual(self.window.manualButton.text, 'Punkty rozdaj\xea sam: tak')
		self.advance(0.35)
		click(self.window.manualButton)
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci reczne 0')

	def test_no_companion_answers_this_window_too(self):
		inv.ToggleSkillWindow()
		self.list()
		inv.OnEqNone('1', '1')
		self.assertEqual(self.window.StatusText(), 'Towarzysz nie jest teraz w grze.')
		self.assertEqual(self.window.nameLines[0].text, '')
		self.assertFalse(self.window.manualButton.shown)
		inv.OnEqNone('1', '2')
		self.assertEqual(self.window.StatusText(), inv.TEXT_NONE[2])

	def test_results_go_to_the_window_that_gave_the_order(self):
		inv.ToggleSkillWindow()
		eq = inv.GetEquipmentWindow()
		inv.ToggleEquipmentWindow()
		self.list()
		full_picture([item_line(3, 19)])
		self.advance(0.35)
		self.window.skillSlots.events['pressedButton'](3)
		inv.OnEqResult('2', hexed('Nie ma punktow.'))
		self.assertEqual(self.window.StatusText(), 'Nie ma punktow.')
		self.assertNotEqual(eq.StatusText(), 'Nie ma punktow.')
		self.advance(0.35)
		eq.bagSlots.Click('unselectItem', 3)
		inv.OnEqResult('0', hexed('Zalozone.'))
		self.assertEqual(eq.StatusText(), 'Zalozone.')
		self.assertEqual(self.window.StatusText(), 'Nie ma punktow.')


# ---------------------------------------------------------------- one queue for all

class QueueTest(Base):
	def test_every_window_shares_the_spacing(self):
		main = uisidekick.GetWindow()
		uisidekick.ToggleWindow()
		inv.ToggleEquipmentWindow()
		inv.ToggleSkillWindow()
		self.assertEqual(STATE['commands'], ['/towarzysz okno 1'])
		times = []
		for i in range(12):
			before = len(STATE['commands'])
			self.advance(0.1)
			for window in (main, inv.GetEquipmentWindow(), inv.GetSkillWindow()):
				window.OnUpdate()
			if len(STATE['commands']) > before:
				times.append(STATE['now'])
		self.assertEqual(STATE['commands'][:3], ['/towarzysz okno 1', '/towarzysz eq 1', '/towarzysz umiejetnosci'])
		gaps = [round(b - a, 3) for a, b in zip(times, times[1:])]
		self.assertTrue(all(gap >= 0.3 for gap in gaps), gaps)
		# Never six in half a second.
		sent = [100.0] + times
		for t in sent:
			self.assertLess(len([u for u in sent if t <= u < t + 0.5]), 6)

	def test_the_keeper_sends_what_a_closed_window_left(self):
		inv.ToggleEquipmentWindow()
		full_picture([item_line(3, 19)])
		window = inv.GetEquipmentWindow()
		window.bagSlots.Click('unselectItem', 3)
		window.Close()
		keeper = uisidekick.GetKeeper()
		self.assertTrue(keeper.CanUpdate())
		self.advance(0.35)
		keeper.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz eq ruch 3 -1')
		self.assertFalse(keeper.CanUpdate())

	def test_the_keepers_take_the_windows_with_the_game(self):
		inv.ToggleEquipmentWindow()
		inv.ToggleSkillWindow()
		full_picture([item_line(3, 19)])
		inv.GetEquipmentWindow().bagSlots.Click('selectItem', 3)
		keeper = inv.GetKeeper()
		self.assertFalse(keeper.CanUpdate())
		keeper.Destroy()
		self.assertIsNone(inv._state['eqWindow'])
		self.assertIsNone(inv._state['skillWindow'])
		self.assertEqual(STATE['icons'], {})
		self.assertEqual(inv._equipment.items, {})
		# The companion's own keeper takes them too.
		inv.GetEquipmentWindow()
		uisidekick.GetKeeper().Destroy()
		self.assertIsNone(inv._state['eqWindow'])

	def test_the_companions_window_opens_both(self):
		main = uisidekick.GetWindow()
		uisidekick.ToggleWindow()
		click(main.inventoryButton)
		click(main.skillsButton)
		self.assertTrue(inv.GetEquipmentWindow().IsShow())
		self.assertTrue(inv.GetSkillWindow().IsShow())
		# Beside it, one to each side where the screen allows.
		self.assertEqual(inv.GetEquipmentWindow().x, main.x + main.WIDTH + 4)
		click(main.inventoryButton)
		self.assertFalse(inv.GetEquipmentWindow().IsShow())


# ---------------------------------------------------------------- the screen

class ScreenTest(Base):
	def check_fits(self, window, width, height):
		self.assertLessEqual(width, 800)
		self.assertLessEqual(height, 600)
		self.assertEqual((window.w, window.h), (width, height))
		for widget in STATE['widgets']:
			if widget is window or widget.Root() is not window:
				continue
			left, top, right, bottom = widget.Rect()
			self.assertTrue(0 <= left and 0 <= top and right <= width and bottom <= height,
				'%s %r at %r outside %dx%d' % (type(widget).__name__, getattr(widget, 'text', ''), (left, top, right, bottom),
					width, height))

	def test_the_bag_window_fits_800x600(self):
		window = inv.GetEquipmentWindow()
		full_picture([item_line(3, 19)], gold=999999999999)
		inv.OnEqResult('2', hexed('Towarzysz nie ma miejsca w torbie.'))
		self.check_fits(window, inv.EquipmentWindow.WIDTH, inv.EquipmentWindow.HEIGHT)

	def test_the_skill_window_fits_800x600(self):
		window = inv.GetSkillWindow()
		STATE['skillNames'] = {106: 'Trzystronne Ci\xeacie'}
		inv.OnSkillBegin('1', '3', '0', '1', '1')
		for vnum in range(106, 106 + inv.MAX_SKILL_ROWS):
			inv.OnSkill(str(vnum), '25', '1')
		inv.OnSkillEnd()
		self.check_fits(window, inv.SkillWindow.WIDTH, inv.SkillWindow.HEIGHT)

	def test_the_companions_window_still_fits(self):
		window = uisidekick.GetWindow()
		self.assertEqual((uisidekick.SidekickWindow.WIDTH, uisidekick.SidekickWindow.HEIGHT), (300, 554))
		self.check_fits(window, 300, 554)

	def test_the_equipment_slots_are_the_players(self):
		window = inv.GetEquipmentWindow()
		layout = window.equipSlots.layout
		self.assertEqual(layout[1004], (3, 3, 32, 96))
		self.assertEqual(layout[1000], (39, 37, 32, 64))
		self.assertEqual(sorted(layout), [1000 + wear for wear in range(11)])
		self.assertEqual(len(window.bagSlots.layout), 45)
		self.assertEqual((window.bagSlots.w, window.bagSlots.h), (160, 288))


# ---------------------------------------------------------------- the rendered stock files

class RenderedRootTest(unittest.TestCase):
	def read(self, name):
		with io.open(os.path.join(CLIENT_ROOT, name), 'rb') as source:
			return source.read().decode('latin-1')

	def test_game_hands_every_answer_on(self):
		game = self.read('game.py')
		answers = {
			'SidekickEqNone': 'OnEqNone', 'SidekickEqBegin': 'OnEqBegin', 'SidekickEqItem': 'OnEqItem',
			'SidekickEqEmpty': 'OnEqEmpty', 'SidekickEqEnd': 'OnEqEnd', 'SidekickEqResult': 'OnEqResult',
			'SidekickSkillBegin': 'OnSkillBegin', 'SidekickSkill': 'OnSkill', 'SidekickSkillEnd': 'OnSkillEnd',
		}
		for command, function in answers.items():
			self.assertEqual(game.count('serverCommandList["%s"] = self.__%s\r\n' % (command, command)), 1, command)
			handler = re.search(r'\tdef __%s\(self, \*args\):\r\n((?:\t\t.*\r\n)+)' % command, game)
			self.assertTrue(handler, command)
			self.assertIn('uisidekickinventory.%s(*args)' % function, handler.group(1))
			self.assertTrue(hasattr(inv, function), function)
		self.assertEqual(game.count('def __KeepSidekickInventory(self):'), 1)

	def test_the_players_bag_takes_the_companions_item_first(self):
		source = self.read('uiinventory.py')
		for method, cell in (('SelectEmptySlot', 'selectedSlotPos'), ('SelectItemSlot', 'itemSlotIndex')):
			body = source.split('\tdef %s(self' % method, 1)[1].split('\r\n\tdef ', 1)[0]
			drop = body.index('uisidekickinventory.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, %s)' % cell)
			self.assertLess(drop, body.index('if player.SLOT_TYPE_INVENTORY == attachedSlotType:'), method)
			self.assertLess(body.index('mouseModule.mouseController.isAttached()'), drop, method)


if __name__ == '__main__':
	unittest.main()
