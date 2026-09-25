# The companion's window without a client: what it reads from the server and
# what it sends back, against stub client modules.
#
#     python tests/uisidekick_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/uisidekick_test.py).
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}


def reset_state():
	STATE.clear()
	STATE.update({'now': 100.0, 'commands': [], 'questions': []})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class StubWidget(object):
	"""Any client widget: what the window asks of it is kept, the rest is
	accepted and does nothing."""

	def __init__(self, *args, **kwargs):
		self.text = ''
		self.shown = False
		self.pressed = False
		self.event = None
		self.eventArgs = ()
		self.percentage = None

	def SetText(self, text):
		self.text = text

	def GetText(self):
		return self.text

	def Show(self):
		self.shown = True

	def Hide(self):
		self.shown = False

	def IsShow(self):
		return self.shown

	def Down(self):
		self.pressed = True

	def SetUp(self):
		self.pressed = False

	def SAFE_SetEvent(self, event, *args):
		self.event = event
		self.eventArgs = args

	def SetPercentage(self, current, maximum):
		self.percentage = (current, maximum)

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return lambda *args, **kwargs: None


class StubQuestion(StubWidget):
	def __init__(self, *args, **kwargs):
		StubWidget.__init__(self)
		self.accept = None
		self.cancel = None
		self.opened = False
		STATE['questions'].append(self)

	def SetAcceptEvent(self, event):
		self.accept = event

	def SetCancelEvent(self, event):
		self.cancel = event

	def Open(self):
		self.opened = True

	def Close(self):
		self.opened = False


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
	sys.modules['net'] = module('net', SendChatPacket=lambda text: STATE['commands'].append(text))
	ui = module('ui', BoardWithTitleBar=StubWidget, ThinBoard=StubWidget, TextLine=StubWidget,
		Button=StubWidget, Gauge=StubWidget)
	setattr(ui, '__mem_func__', lambda func: func)
	sys.modules['ui'] = ui
	sys.modules['uiCommon'] = module('uiCommon', QuestionDialog=StubQuestion)


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import clientclock  # noqa: E402
import uisidekick  # noqa: E402


def hexed(text):
	return ''.join('%02x' % ord(c) for c in text)


def info_line(**over):
	values = dict(race=5, group=2, level=42, exp=37, hp=900, maxhp=1800, sp=100, maxsp=400, where=1, dist=350,
		mode=0, stance=1, loot=2, protect=1, buffs=0, gold=1234567, red=120, blue=40, dead=0)
	values.update(over)
	order = ('race', 'group', 'level', 'exp', 'hp', 'maxhp', 'sp', 'maxsp', 'where', 'dist',
		'mode', 'stance', 'loot', 'protect', 'buffs', 'gold', 'red', 'blue', 'dead')
	return ['1', '1'] + [str(values[k]) for k in order]


def click(button):
	button.event(*button.eventArgs)


class ProtocolTest(unittest.TestCase):
	def test_the_three_answers_of_the_first_word(self):
		self.assertEqual(uisidekick.ParseInfo(['1', '0']), {'has': False})
		self.assertEqual(uisidekick.ParseInfo(['1', '2']), {'has': False, 'off': True})
		info = uisidekick.ParseInfo(info_line())
		self.assertTrue(info['has'])
		self.assertEqual((info['race'], info['level'], info['stance'], info['loot']), (5, 42, 1, 2))
		self.assertEqual(info['gold'], 1234567)

	def test_another_protocol_or_a_short_line_is_ignored(self):
		self.assertIsNone(uisidekick.ParseInfo(['2', '1'] + info_line()[2:]))
		self.assertIsNone(uisidekick.ParseInfo(['1', '1', '5', '2']))
		self.assertIsNone(uisidekick.ParseInfo([]))

	def test_texts_come_as_hex_and_nothing_else_gets_through(self):
		self.assertEqual(uisidekick.DecodeText(hexed('Dolina Ork\xf3w')), 'Dolina Ork\xf3w')
		self.assertEqual(uisidekick.DecodeText('-'), '')
		self.assertEqual(uisidekick.DecodeText('4a6f61'), 'Joa')
		self.assertEqual(uisidekick.DecodeText('4a6f6'), '')  # odd length
		self.assertEqual(uisidekick.DecodeText('zz'), '')
		self.assertEqual(uisidekick.DecodeText('0a41'), '?A')  # a control character is shown as '?'
		self.assertEqual(uisidekick.DecodeText('41' * 65), '')  # longer than the server sends

	def test_numbers_as_the_client_writes_them(self):
		self.assertEqual(uisidekick.FormatGold(0), '0')
		self.assertEqual(uisidekick.FormatGold(999), '999')
		self.assertEqual(uisidekick.FormatGold(1234567), '1.234.567')
		self.assertEqual(uisidekick.ClassText(5, 2), 'Ninja (\xa3ucznik)')
		self.assertEqual(uisidekick.ClassText(3, 0), 'Szaman')
		self.assertEqual(uisidekick.PlaceText({'where': 1, 'dist': 350}, 'Joan'), 'Joan, obok ciebie')
		self.assertEqual(uisidekick.PlaceText({'where': 1, 'dist': 4200}, 'Joan'), 'Joan, 42 m od ciebie')
		self.assertEqual(uisidekick.PlaceText({'where': 2}, 'Dolina Ork\xf3w'), 'Dolina Ork\xf3w (inna mapa)')
		self.assertEqual(uisidekick.PlaceText({'where': 0}, ''), 'poza gr\xb9')


class WindowTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		clientclock.Reset()
		uisidekick.Destroy()
		self.window = uisidekick.GetWindow()

	def test_opening_asks_for_the_whole_gear_and_then_polls(self):
		uisidekick.ToggleWindow()
		self.assertTrue(self.window.IsShow())
		self.assertEqual(STATE['commands'], ['/towarzysz okno 1'])
		STATE['now'] += 1.0
		self.window.OnUpdate()
		self.assertEqual(len(STATE['commands']), 1)
		STATE['now'] += 0.6
		self.window.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz okno')
		uisidekick.ToggleWindow()
		self.assertFalse(self.window.IsShow())

	def test_the_snapshot_fills_the_window(self):
		uisidekick.OnServerInfo(*info_line())
		uisidekick.OnServerNames(hexed('Wojtek'), hexed('Joan'), hexed('walczy'))
		uisidekick.OnServerGear('0', hexed('Miecz+7'))
		self.assertIn('Wojtek', self.window.nameLine.text)
		self.assertIn('Lv 42', self.window.nameLine.text)
		self.assertEqual(self.window.classLine.text, 'Ninja (\xa3ucznik)')
		self.assertEqual(self.window.hpGauge.percentage, (900, 1800))
		self.assertEqual(self.window.hpText.text, '900 / 1800')
		self.assertIn('1.234.567', self.window.expLine.text)
		self.assertEqual(self.window.placeLine.text, 'Gdzie: Joan, obok ciebie')
		self.assertEqual(self.window.doingLine.text, 'Teraz: walczy')
		self.assertEqual(self.window.gearLines[0].text, 'Miecz+7')
		self.assertEqual(self.window.gearLines[1].text, '-')
		# The stance and the loot set are the buttons held down.
		self.assertEqual([b.pressed for b in self.window.stanceButtons], [False, True, False])
		self.assertEqual([b.pressed for b in self.window.lootButtons], [False, False, True])
		self.assertEqual(self.window.buffButton.text, 'Buffy: nie')
		self.assertTrue(self.window.summonButton.pressed)

	def test_a_waiting_companion_shows_the_wait_held_down(self):
		uisidekick.OnServerInfo(*info_line(mode=2))
		self.assertTrue(self.window.holdButton.pressed)
		self.assertFalse(self.window.summonButton.pressed)
		self.assertIn('czeka', self.window.statusLine.text)

	def test_no_companion_and_a_world_without_them(self):
		uisidekick.OnServerInfo('1', '0')
		self.assertEqual(self.window.nameLine.text, 'Nie masz jeszcze towarzysza.')
		uisidekick.OnServerInfo('1', '2')
		self.assertIn('wy\xb3\xb9czeni', self.window.nameLine.text)

	def test_orders_are_the_letters_commands(self):
		uisidekick.OnServerInfo(*info_line())
		click(self.window.stanceButtons[2])
		self.assertEqual(STATE['commands'], ['/towarzysz walka 2'])
		STATE['now'] += 0.5
		click(self.window.lootButtons[1])
		self.assertEqual(STATE['commands'][-1], '/towarzysz zbieraj 1')
		STATE['now'] += 0.5
		click(self.window.protectButton)
		self.assertEqual(STATE['commands'][-1], '/towarzysz ochrona 0')
		STATE['now'] += 0.5
		click(self.window.buffButton)
		self.assertEqual(STATE['commands'][-1], '/towarzysz buffy 1')
		STATE['now'] += 0.5
		click(self.window.holdButton)
		self.assertEqual(STATE['commands'][-1], '/towarzysz czekaj')

	def test_a_burst_of_clicks_is_spaced(self):
		# The server drops a sixth command in half a second without a word.
		click(self.window.summonButton)
		click(self.window.freeButton)
		click(self.window.holdButton)
		self.assertEqual(STATE['commands'], ['/towarzysz przywolaj'])
		STATE['now'] += 0.3
		self.window.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz wolny')
		STATE['now'] += 0.1
		self.window.OnUpdate()
		self.assertEqual(len(STATE['commands']), 2)
		STATE['now'] += 0.3
		self.window.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz czekaj')

	def test_dismissing_asks_first(self):
		self.window.OnDismiss()
		self.assertEqual(STATE['commands'], [])
		question = STATE['questions'][-1]
		self.assertTrue(question.opened)
		question.cancel()
		self.assertFalse(question.opened)
		self.assertEqual(STATE['commands'], [])
		self.window.OnDismiss()
		STATE['questions'][-1].accept()
		self.assertEqual(STATE['commands'], ['/towarzysz odprawa tak'])

	def test_the_keeper_takes_the_window_with_the_game(self):
		keeper = uisidekick.GetKeeper()
		self.assertFalse(keeper.CanUpdate())
		keeper.Destroy()
		self.assertIsNone(uisidekick._window['window'])
		# The next game window builds a new one.
		self.assertIsNot(uisidekick.GetWindow(), self.window)


if __name__ == '__main__':
	unittest.main()
