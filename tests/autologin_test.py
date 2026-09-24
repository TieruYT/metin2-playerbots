# Auto Lowy's autologin without a client: the login window's side of it
# against a stub stream, popup and login window.
#
#     python tests/autologin_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/autologin_test.py).
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {'now': 100.0, 'errors': []}


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
sys.modules['dbg'] = module('dbg', TraceError=lambda text: STATE['errors'].append(text))
sys.modules['localeInfo'] = module('localeInfo', UI_CANCEL='Anuluj')
sys.path.insert(0, CLIENT_ROOT)
import autologin  # noqa: E402
import clientclock  # noqa: E402


class Text(object):
	def __init__(self):
		self.text = ''

	def SetText(self, text):
		self.text = text


class Popup(object):
	"""networkModule.PopupDialog as it behaves: Open closes what is open
	first, and every Close runs the close event once."""

	def __init__(self):
		self.shown = False
		self.CloseEvent = 0
		self.message = Text()
		self.button = ''

	def Open(self, message, event=0, button='Anuluj'):
		if self.shown:
			self.Close()
		self.CloseEvent = event
		self.button = button
		self.message.SetText(message)
		self.shown = True

	def Close(self):
		if not self.shown:
			self.CloseEvent = 0
			return
		self.shown = False
		if self.CloseEvent:
			event = self.CloseEvent
			self.CloseEvent = 0
			event()

	def GetChild(self, name):
		return self.message

	def Press(self):
		"""The popup's button, or Escape."""
		self.Close()


class Stream(object):
	def __init__(self):
		self.id = 'konto'
		self.pwd = 'haslo'
		self.isAutoSelect = 0
		self.popupWindow = Popup()


class LoginWindow(object):
	"""intrologin.LoginWindow's part the autologin touches: Connect opens the
	stock "connecting" popup, whose event only moves the focus."""

	def __init__(self, stream):
		self.stream = stream
		self.connectingDialog = None
		self.connects = []

	def Connect(self, id, pwd):
		self.stream.popupWindow.Open('Laczenie', lambda: None, 'Anuluj')
		self.connects.append((id, pwd))


def advance(seconds):
	STATE['now'] += seconds


class AutologinTest(unittest.TestCase):
	def setUp(self):
		STATE['now'] = 100.0
		del STATE['errors'][:]
		clientclock.Reset()
		autologin.Reset()
		self.stream = Stream()
		self.popup = self.stream.popupWindow

	def enter_game(self, armed=True):
		"""The first frame of a game entered by hand, and the Hunter reading
		the character's switch."""
		self.assertEqual(autologin.GameFrame(), autologin.NEW_SESSION)
		autologin.SetArmed(armed)
		self.assertEqual(autologin.GameFrame(), autologin.NOTHING)

	def drop(self, hunting=True):
		"""The game window closes by itself and the login window opens."""
		autologin.NoteGameClosed(hunting)
		advance(1.0)
		win = LoginWindow(self.stream)
		autologin.OnLoginOpen(win)
		return win

	def pump(self, win, seconds=0.0):
		advance(seconds)
		autologin.PumpLogin(win)

	def test_a_drop_logs_in_again_enters_the_character_and_resumes_the_hunt(self):
		self.enter_game()
		win = self.drop()
		self.assertTrue(autologin.IsReconnecting())
		self.assertTrue(self.popup.shown)
		self.assertEqual(self.popup.message.text, 'Autologin: ponownie za 3 s (pr\xf3ba 1)')
		self.pump(win, 1.0)
		self.assertEqual(self.popup.message.text, 'Autologin: ponownie za 2 s (pr\xf3ba 1)')
		self.assertEqual(win.connects, [])
		self.pump(win, 2.0)
		self.assertEqual(win.connects, [('konto', 'haslo')])
		self.assertEqual(self.stream.isAutoSelect, 1)
		# Our popup closed for the try, and not as a Cancel.
		self.assertTrue(autologin.IsReconnecting())
		# The login went through: select, loading, and the game's first frame.
		advance(4.0)
		self.assertEqual(autologin.GameFrame(), autologin.RECONNECTED)
		self.assertEqual(self.stream.isAutoSelect, 0)
		self.assertFalse(autologin.IsReconnecting())
		advance(4.9)
		self.assertEqual(autologin.GameFrame(), autologin.NOTHING)
		advance(0.2)
		self.assertEqual(autologin.GameFrame(), autologin.RESUME)
		advance(10.0)
		self.assertEqual(autologin.GameFrame(), autologin.NOTHING)

	def test_no_resume_when_the_hunt_was_not_running(self):
		self.enter_game()
		win = self.drop(hunting=False)
		self.pump(win, 3.0)
		self.assertEqual(autologin.GameFrame(), autologin.RECONNECTED)
		for _ in range(20):
			advance(1.0)
			self.assertEqual(autologin.GameFrame(), autologin.NOTHING)

	def test_a_refused_connection_is_tried_again_ever_later(self):
		self.enter_game()
		win = self.drop()
		self.pump(win, 3.0)
		waits = []
		for attempt in range(2, 7):
			self.assertTrue(autologin.OnConnectFailure(win))
			self.assertTrue(self.popup.shown)
			text = self.popup.message.text
			waits.append(int(text.split('za ')[1].split(' s')[0]))
			self.assertTrue(text.endswith('(pr\xf3ba %d)' % attempt), text)
			before = len(win.connects)
			self.pump(win, waits[-1] - 0.5)
			self.assertEqual(len(win.connects), before)
			self.pump(win, 0.5)
			self.assertEqual(len(win.connects), before + 1)
		self.assertEqual(waits, [5, 10, 20, 30, 30])

	def test_an_account_still_in_the_game_is_asked_again_in_ten_seconds(self):
		self.enter_game()
		win = self.drop()
		self.pump(win, 3.0)
		self.assertTrue(autologin.OnLoginFailure(win, 'ALREADY'))
		self.assertEqual(self.popup.message.text, 'Autologin: ponownie za 10 s (pr\xf3ba 2)')
		self.pump(win, 10.0)
		self.assertEqual(len(win.connects), 2)

	def test_a_wrong_password_or_an_old_client_ends_it(self):
		for error in ('WRONGPWD', 'UPDATE', 'BLOCK'):
			self.setUp()
			self.enter_game()
			win = self.drop()
			self.pump(win, 3.0)
			self.assertFalse(autologin.OnLoginFailure(win, error))
			self.assertFalse(autologin.IsReconnecting())
			self.assertEqual(self.stream.isAutoSelect, 0)
			self.pump(win, 60.0)
			self.assertEqual(len(win.connects), 1)

	def test_the_players_own_logout_is_no_drop(self):
		self.enter_game()
		autologin.NoteManualExit()
		win = self.drop()
		self.assertFalse(autologin.IsReconnecting())
		self.assertFalse(self.popup.shown)
		self.pump(win, 10.0)
		self.assertEqual(win.connects, [])

	def test_nothing_happens_with_the_switch_off(self):
		self.enter_game(armed=False)
		win = self.drop()
		self.assertFalse(autologin.IsReconnecting())
		self.pump(win, 10.0)
		self.assertEqual(win.connects, [])

	def test_cancel_in_the_popup_hands_the_login_to_the_player(self):
		self.enter_game()
		win = self.drop()
		self.popup.Press()
		self.assertFalse(autologin.IsReconnecting())
		self.assertEqual(self.stream.isAutoSelect, 0)
		self.pump(win, 10.0)
		self.assertEqual(win.connects, [])
		# And the next game entered by hand is an ordinary one.
		self.assertEqual(autologin.GameFrame(), autologin.NEW_SESSION)

	def test_a_try_nobody_answers_is_given_up_after_forty_seconds(self):
		self.enter_game()
		win = self.drop()
		self.pump(win, 3.0)
		self.pump(win, 39.0)
		self.assertFalse(self.popup.shown and self.popup.CloseEvent == autologin.Cancel)
		self.pump(win, 1.0)
		self.assertEqual(self.popup.message.text, 'Autologin: ponownie za 5 s (pr\xf3ba 2)')
		self.pump(win, 5.0)
		self.assertEqual(len(win.connects), 2)

	def test_the_login_window_back_before_the_game_stops_after_three_times(self):
		self.enter_game()
		win = self.drop()
		self.pump(win, 3.0)
		for breaks in range(1, 4):
			# The select or the loading phase fell through to the login.
			win = LoginWindow(self.stream)
			autologin.OnLoginOpen(win)
			self.assertTrue(autologin.IsReconnecting(), breaks)
			self.pump(win, 30.0)
		win = LoginWindow(self.stream)
		autologin.OnLoginOpen(win)
		self.assertFalse(autologin.IsReconnecting())
		self.assertEqual(self.stream.isAutoSelect, 0)

	def test_a_game_that_closed_into_the_select_window_starts_nothing_later(self):
		self.enter_game()
		autologin.NoteGameClosed(True)
		advance(31.0)
		win = LoginWindow(self.stream)
		autologin.OnLoginOpen(win)
		self.assertFalse(autologin.IsReconnecting())
		self.pump(win, 10.0)
		self.assertEqual(win.connects, [])

	def test_a_new_game_disarms_until_the_characters_settings_say_so(self):
		self.enter_game()
		self.assertTrue(autologin.IsArmed())
		autologin.NoteManualExit()
		autologin.NoteGameClosed(False)
		self.assertEqual(autologin.GameFrame(), autologin.NEW_SESSION)
		self.assertFalse(autologin.IsArmed())

	def test_no_login_in_the_stream_means_no_try(self):
		self.enter_game()
		self.stream.pwd = ''
		win = self.drop()
		self.assertFalse(autologin.IsReconnecting())
		self.pump(win, 10.0)
		self.assertEqual(win.connects, [])

	def test_nothing_is_written_anywhere(self):
		# The password lives in the stream alone: the module keeps no copy.
		self.enter_game()
		win = self.drop()
		self.pump(win, 3.0)
		self.assertNotIn('haslo', repr(autologin._s))


if __name__ == '__main__':
	unittest.main()
