# The companion's window ("Towarzysz", playerbot_sidekick.h on the server):
# what it is doing and has on it, and the orders a player gives it, on the P
# key and from the Towarzysz letter - Tieru's request of 25 September, "GUI na
# podstawie AutoLowow, ale do sterowania towarzyszem".
#
# The server answers "/towarzysz okno" (and "okno 1" for the whole gear, which
# the window asks for when it opens) with three commands, each far under the
# 512 bytes CHARACTER::ChatPacket formats into:
#
#   SidekickInfo <protocol> 0                       - no companion
#   SidekickInfo <protocol> 2                       - companions switched off
#                                                     in this world (M2_SIDEKICK)
#   SidekickInfo <protocol> 1 <race> <group> <level> <exp%> <hp> <maxhp> <sp>
#                <maxsp> <where> <dist> <mode> <stance> <loot> <protect>
#                <buffs> <gold> <red> <blue> <dead>
#   SidekickNames <name> <place> <doing>            - hex of the CP1250 bytes
#   SidekickGear <slot 0-7> <name>                  - hex, only when changed
#
# where: 0 not in the game, 1 on the owner's map, 2 elsewhere; mode: 0 at the
# owner's side, 1 free, 2 waiting, 3 shopping; stance: 0 attacks everything,
# 1 attacks nobody first, 2 does not fight; loot: 0 nothing, 1 the owner's,
# 2 everything. The orders are the letter's own commands, so the window adds
# nothing the server did not already take from the quest: przywolaj, wolny,
# czekaj, zakupy, stan, walka N, zbieraj N, ochrona N, buffy N, odprawa tak.
#
# Python 2.7 as the client has it; the Polish letters are CP1250 escapes.

import clientclock
import net
import ui

PROTOCOL = 1
POLL_INTERVAL = 1.5
# The server drops a sixth command in half a second (ENABLE_ANTI_CMD_FLOOD)
# without a word, so a burst of clicks is spaced.
COMMAND_SPACING = 0.3
HEX_DIGITS = '0123456789abcdefABCDEF'
MAX_TEXT_BYTES = 64

JOBS = ('Wojownik', 'Ninja', 'Sura', 'Szaman')
PATHS = (
	('Cia\xb3o', 'Umys\xb3'),
	('Skrytob\xf3jca', '\xa3ucznik'),
	('Bro\xf1', 'Czarna magia'),
	('Smok', 'Leczenie'),
)
MODES = ('przy tobie', 'wolna r\xeaka', 'czeka w miejscu', 'robi zakupy')
STANCES = ('Atakuj', 'Nie 1. atak', 'Nie walcz')
STANCE_HINTS = ('bije wszystko w pobli\xbfu', 'nie zaczyna, broni ciebie i siebie', 'nie walczy wcale')
LOOTS = ('Nic', 'Tw\xf3j drop', 'Wszystko')
GEAR_LABELS = ('Bro\xf1', 'Zbroja', 'He\xb3m', 'Tarcza', 'Buty', 'Bransoleta', 'Naszyjnik', 'Kolczyki')

GAUGE_HP = 'red'
GAUGE_SP = 'pink'


def DecodeText(value):
	"""The server's hex of CP1250 bytes; '' for '-' or anything malformed."""
	if not value or value == '-' or len(value) % 2 or len(value) > MAX_TEXT_BYTES * 2:
		return ''
	chars = []
	for i in range(0, len(value), 2):
		pair = value[i:i + 2]
		if pair[0] not in HEX_DIGITS or pair[1] not in HEX_DIGITS:
			return ''
		code = int(pair, 16)
		chars.append(chr(code) if code >= 32 and code != 127 else '?')
	return ''.join(chars)


def ParseInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def ParseInfo(args):
	"""SidekickInfo's words after the command, as a dict; None when the
	protocol is another one or the line is short. 'has' is False with no
	companion."""
	if len(args) < 2 or ParseInt(args[0], -1) != PROTOCOL:
		return None
	state = ParseInt(args[1])
	if state == 0:
		return {'has': False}
	if state == 2:
		return {'has': False, 'off': True}
	names = ('race', 'group', 'level', 'exp', 'hp', 'maxhp', 'sp', 'maxsp', 'where', 'dist',
			'mode', 'stance', 'loot', 'protect', 'buffs', 'gold', 'red', 'blue', 'dead')
	values = args[2:]
	if len(values) < len(names):
		return None
	info = {'has': True}
	for i, name in enumerate(names):
		info[name] = ParseInt(values[i])
	return info


def FormatGold(value):
	"""1234567 -> '1.234.567', as the client writes yang."""
	text = str(max(0, value))
	parts = []
	while len(text) > 3:
		parts.insert(0, text[-3:])
		text = text[:-3]
	parts.insert(0, text)
	return '.'.join(parts)


def ClassText(race, group):
	if race < 0:
		return ''
	job = race % 4
	text = JOBS[job]
	if group in (1, 2):
		text += ' (%s)' % PATHS[job][group - 1]
	return text


def PlaceText(info, place):
	where = info.get('where', 0)
	if where == 0:
		return 'poza gr\xb9'
	if where == 2:
		return '%s (inna mapa)' % (place or 'inna mapa')
	dist = info.get('dist', 0)
	if dist < 1000:
		near = 'obok ciebie'
	else:
		near = '%d m od ciebie' % (dist // 100)
	return '%s, %s' % (place, near) if place else near


class SidekickWindow(ui.BoardWithTitleBar):
	WIDTH = 300
	HEIGHT = 548

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.info = None
		self.names = ('', '', '')
		self.gear = [''] * len(GEAR_LABELS)
		self.nextPoll = 0.0
		self.nextCommand = 0.0
		self.pending = []
		self.question = None
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetTitleName('Towarzysz')
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.Build()
		self.RefreshAll()

	# -------------------------------------------------------------- building

	def Build(self):
		# Five boards and a status line in 548 pixels, so the window fits an
		# 800x600 screen beside the game as Auto Lowy's does.
		BL = 10
		BW = self.WIDTH - 2 * BL
		ROW = 15
		y = 32

		stBoard = self._Board(BL, y, BW, 146)
		self._Label(stBoard, 14, 4, 'Posta\xe6')
		self.nameLine = self._Label(stBoard, 10, 20, '')
		self.classLine = self._Label(stBoard, 10, 20 + ROW, '')
		self._Label(stBoard, 10, 52, 'HP')
		self.hpGauge = self._Gauge(stBoard, 36, 55, 150, GAUGE_HP)
		self.hpText = self._Label(stBoard, 194, 52, '')
		self._Label(stBoard, 10, 52 + ROW, 'PE')
		self.spGauge = self._Gauge(stBoard, 36, 55 + ROW, 150, GAUGE_SP)
		self.spText = self._Label(stBoard, 194, 52 + ROW, '')
		self.expLine = self._Label(stBoard, 10, 84, '')
		self.potionLine = self._Label(stBoard, 10, 84 + ROW, '')
		self.placeLine = self._Label(stBoard, 10, 84 + 2 * ROW, '')
		self.doingLine = self._Label(stBoard, 10, 84 + 3 * ROW, '')
		y += 146 + 4

		# Three large buttons (88 wide) a row, 92 apart, in the board's 280.
		orBoard = self._Board(BL, y, BW, 66)
		self._Label(orBoard, 14, 4, 'Polecenia')
		self.summonButton = self._Btn(orBoard, 'large', 6, 20, 'Przywo\xb3aj', self.OnOrder, 'przywolaj')
		self.holdButton = self._Btn(orBoard, 'large', 98, 20, 'Czekaj tu', self.OnOrder, 'czekaj')
		self.freeButton = self._Btn(orBoard, 'large', 190, 20, 'Wolna r\xeaka', self.OnOrder, 'wolny')
		self._Btn(orBoard, 'large', 6, 42, 'Na zakupy', self.OnOrder, 'zakupy')
		self._Btn(orBoard, 'large', 98, 42, 'Raport', self.OnOrder, 'stan')
		self._Btn(orBoard, 'large', 190, 42, 'Odpraw', self.OnDismiss)
		y += 66 + 4

		wkBoard = self._Board(BL, y, BW, 60)
		self._Label(wkBoard, 14, 4, 'Walka')
		self.stanceButtons = []
		for i, text in enumerate(STANCES):
			self.stanceButtons.append(self._Btn(wkBoard, 'large', 6 + i * 92, 20, text, self.OnStance, i))
		self.stanceHint = self._Label(wkBoard, 10, 42, '')
		y += 60 + 4

		dpBoard = self._Board(BL, y, BW, 66)
		self._Label(dpBoard, 14, 4, 'Drop i wsparcie')
		self.lootButtons = []
		for i, text in enumerate(LOOTS):
			self.lootButtons.append(self._Btn(dpBoard, 'large', 6 + i * 92, 20, text, self.OnLoot, i))
		self.protectButton = self._Btn(dpBoard, 'large', 6, 42, '', self.OnProtect)
		self.buffButton = self._Btn(dpBoard, 'large', 98, 42, '', self.OnBuffs)
		y += 66 + 4

		eqBoard = self._Board(BL, y, BW, 20 + len(GEAR_LABELS) * 14 + 2)
		self._Label(eqBoard, 14, 4, 'Ekwipunek')
		self.gearLines = []
		for i, label in enumerate(GEAR_LABELS):
			self._Label(eqBoard, 10, 20 + i * 14, label + ':')
			self.gearLines.append(self._Label(eqBoard, 86, 20 + i * 14, '-'))
		y += 20 + len(GEAR_LABELS) * 14 + 2 + 4

		self.statusLine = self._Label(self, self.WIDTH // 2, y, '')
		self.statusLine.SetHorizontalAlignCenter()

	def _Board(self, x, y, w, h):
		board = ui.ThinBoard()
		board.SetParent(self)
		board.SetPosition(x, y)
		board.SetSize(w, h)
		board.Show()
		self.widgets.append(board)
		return board

	def _Label(self, parent, x, y, text):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def _Btn(self, parent, size, x, y, text, event, *args):
		button = ui.Button()
		button.SetParent(parent)
		button.SetPosition(x, y)
		button.SetUpVisual('d:/ymir work/ui/public/%s_button_01.sub' % size)
		button.SetOverVisual('d:/ymir work/ui/public/%s_button_02.sub' % size)
		button.SetDownVisual('d:/ymir work/ui/public/%s_button_03.sub' % size)
		button.SetText(text)
		button.SAFE_SetEvent(event, *args)
		button.Show()
		self.widgets.append(button)
		return button

	def _Gauge(self, parent, x, y, width, color):
		gauge = ui.Gauge()
		gauge.SetParent(parent)
		gauge.SetPosition(x, y)
		gauge.MakeGauge(width, color)
		gauge.Show()
		self.widgets.append(gauge)
		return gauge

	# -------------------------------------------------------------- the server

	def OnServerInfo(self, args):
		info = ParseInfo(args)
		if info is None:
			return
		self.info = info
		self.RefreshAll()

	def OnServerNames(self, name='-', place='-', doing='-'):
		self.names = (DecodeText(name), DecodeText(place), DecodeText(doing))
		self.RefreshAll()

	def OnServerGear(self, slot='0', name='-'):
		index = ParseInt(slot, -1)
		if 0 <= index < len(self.gear):
			self.gear[index] = DecodeText(name)
			self.gearLines[index].SetText(self.gear[index] or '-')

	# -------------------------------------------------------------- showing

	def RefreshAll(self):
		info = self.info
		if not info:
			self.statusLine.SetText('Czekam na odpowied\x9f serwera...')
			return
		if not info.get('has'):
			if info.get('off'):
				self.nameLine.SetText('Towarzysze s\xb9 wy\xb3\xb9czeni na tym serwerze.')
				self.classLine.SetText('W\xb3\xb9cza je w\xb3a\x9cciciel serwera w launcherze.')
			else:
				self.nameLine.SetText('Nie masz jeszcze towarzysza.')
				self.classLine.SetText('Wybierz go w li\x9ccie "Towarzysz".')
			for line in (self.hpText, self.spText, self.expLine, self.potionLine, self.placeLine, self.doingLine):
				line.SetText('')
			self.hpGauge.SetPercentage(0, 1)
			self.spGauge.SetPercentage(0, 1)
			self.stanceHint.SetText('')
			self.statusLine.SetText('')
			return
		name, place, doing = self.names
		if info['race'] < 0:
			self.nameLine.SetText(name or 'Towarzysz')
			self.classLine.SetText('Za chwil\xea b\xeadzie w grze.')
		else:
			self.nameLine.SetText('%s   Lv %d' % (name or 'Towarzysz', info['level']))
			self.classLine.SetText(ClassText(info['race'], info['group']))
		maxHP = max(1, info['maxhp'])
		maxSP = max(1, info['maxsp'])
		self.hpGauge.SetPercentage(max(0, min(info['hp'], maxHP)), maxHP)
		self.spGauge.SetPercentage(max(0, min(info['sp'], maxSP)), maxSP)
		self.hpText.SetText('%d / %d' % (max(0, info['hp']), info['maxhp']))
		self.spText.SetText('%d / %d' % (max(0, info['sp']), info['maxsp']))
		self.expLine.SetText('Do\x9cwiadczenie: %d%%   Yang: %s' % (info['exp'], FormatGold(info['gold'])))
		self.potionLine.SetText('Mikstury: %d czerw. / %d nieb.' % (info['red'], info['blue']))
		self.placeLine.SetText('Gdzie: %s' % PlaceText(info, place))
		mode = info['mode'] if 0 <= info['mode'] < len(MODES) else 0
		if info['dead']:
			self.doingLine.SetText('Teraz: le\xbfy, zaraz wstanie')
		else:
			self.doingLine.SetText('Teraz: %s' % (doing or MODES[mode]))
		self.SetPressed(self.stanceButtons, info['stance'])
		self.SetPressed(self.lootButtons, info['loot'])
		stance = info['stance'] if 0 <= info['stance'] < len(STANCE_HINTS) else 0
		self.stanceHint.SetText(STANCE_HINTS[stance])
		self.protectButton.SetText('Ochrona: %s' % ('tak' if info['protect'] else 'nie'))
		self.buffButton.SetText('Buffy: %s' % ('tak' if info['buffs'] else 'nie'))
		# The summon, the free hand and the wait are the three states the
		# companion is in outside an errand: the one it is in stays down.
		self.SetPressed((self.summonButton, self.freeButton, self.holdButton), mode if mode < 3 else -1)
		self.statusLine.SetText('Tryb: %s' % MODES[mode])

	def SetPressed(self, buttons, index):
		# A button held down says which one is set, the way the game's own
		# radio groups do it.
		for i, button in enumerate(buttons):
			if i == index:
				button.Down()
			else:
				button.SetUp()

	# -------------------------------------------------------------- orders

	def SendCommand(self, text):
		now = clientclock.Now()
		if now < self.nextCommand:
			self.pending.append(text)
			return
		self.nextCommand = now + COMMAND_SPACING
		net.SendChatPacket('/towarzysz ' + text)

	def OnOrder(self, order):
		self.SendCommand(order)
		# The answer comes back at the next look; asked for at once, the
		# window shows the new mode without waiting for the poll.
		self.nextPoll = 0.0

	def OnStance(self, stance):
		self.SendCommand('walka %d' % stance)
		self.nextPoll = 0.0

	def OnLoot(self, loot):
		self.SendCommand('zbieraj %d' % loot)
		self.nextPoll = 0.0

	def OnProtect(self):
		protect = self.info.get('protect', 1) if self.info else 1
		self.SendCommand('ochrona %d' % (0 if protect else 1))
		self.nextPoll = 0.0

	def OnBuffs(self):
		buffs = self.info.get('buffs', 1) if self.info else 1
		self.SendCommand('buffy %d' % (0 if buffs else 1))
		self.nextPoll = 0.0

	def OnDismiss(self):
		import uiCommon
		question = uiCommon.QuestionDialog()
		question.SetText('Odprawi\xe6 towarzysza na dobre? Tego nie da si\xea cofn\xb9\xe6.')
		question.SetAcceptEvent(ui.__mem_func__(self.OnDismissAccept))
		question.SetCancelEvent(ui.__mem_func__(self.OnDismissCancel))
		question.Open()
		self.question = question

	def OnDismissAccept(self):
		self.SendCommand('odprawa tak')
		self.OnDismissCancel()
		self.nextPoll = 0.0

	def OnDismissCancel(self):
		if self.question:
			self.question.Close()
		self.question = None

	# -------------------------------------------------------------- the clock

	def OnUpdate(self):
		now = clientclock.Now()
		if self.pending and now >= self.nextCommand:
			text = self.pending.pop(0)
			self.nextCommand = now + COMMAND_SPACING
			net.SendChatPacket('/towarzysz ' + text)
			return
		if now >= self.nextPoll and now >= self.nextCommand:
			self.nextPoll = now + POLL_INTERVAL
			self.nextCommand = now + COMMAND_SPACING
			net.SendChatPacket('/towarzysz okno')

	def Open(self):
		self.Show()
		self.SetTop()
		now = clientclock.Now()
		self.nextPoll = now + POLL_INTERVAL
		self.nextCommand = now + COMMAND_SPACING
		net.SendChatPacket('/towarzysz okno 1')

	def Close(self):
		self.OnDismissCancel()
		self.pending = []
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.OnDismissCancel()
		self.Hide()
		self.widgets = []


_window = {'window': None}


def GetWindow():
	if _window['window'] is None:
		window = SidekickWindow()
		window.SetCenterPosition()
		_window['window'] = window
	return _window['window']


def ToggleWindow():
	window = GetWindow()
	if window.IsShow():
		window.Close()
	else:
		window.Open()


def OpenWindow():
	window = GetWindow()
	if not window.IsShow():
		window.Open()


def OnServerInfo(*args):
	# Answers that come while the window is closed are kept all the same, so
	# it opens on what the server last said.
	GetWindow().OnServerInfo(args)


def OnServerNames(name='-', place='-', doing='-', *rest):
	GetWindow().OnServerNames(name, place, doing)


def OnServerGear(slot='0', name='-', *rest):
	GetWindow().OnServerGear(slot, name)


def Destroy():
	window = _window['window']
	if window is not None:
		window.Destroy()
	_window['window'] = None


class Keeper(object):
	"""One of the game's updateables, for its Destroy alone: the game window's
	Close destroys every updateable, and the companion's window goes with it
	rather than stand over the character select. The window updates itself
	(its own OnUpdate while shown)."""

	def CanUpdate(self):
		return False

	def OnUpdate(self):
		pass

	def Destroy(self):
		Destroy()


def GetKeeper():
	return Keeper()
