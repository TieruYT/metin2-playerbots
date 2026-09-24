# Autologin for Auto Lowy: after a dropped game the login window logs the
# same account in again, the select window enters the same character, and a
# hunt that was running goes on (Tieru, 24 September: "wprowadz funkcje auto
# loginu do auto lowow ... jako checkbox"). The switch is a row of the Auto
# Lowy window, saved per character with its other settings. Nothing else is
# written anywhere: the login and the password are the ones the network
# stream already keeps from the last login (networkModule.MainStream.id and
# pwd, which nothing clears), so the feature ends with the client and no
# password reaches a file.
#
# "Dropped" is a game phase that closed without the player asking for it:
# Hunter.Destroy tells NoteGameClosed when the game window closes, and the
# system menu's logout and change-character buttons call NoteManualExit first
# (clientrootify.py, uisystem.py). The exe opens the login window for every
# disconnect in the game - CPythonNetworkStream::OnRemoteDisconnect calls
# SetLoginPhase - so a server restart, a crash, a GM's /dc and a lost line all
# end in OnLoginOpen (intrologin.py) with the stream's address still set.
#
# The first try is three seconds after the login window opens, then five, ten,
# twenty and thirty between tries for as long as it takes: a server being
# updated is minutes away. The popup counts down and its button ends it. An
# account the server still holds ("ALREADY" - a dropped session lives about a
# minute) is asked again in ten seconds; a wrong password, a blocked account
# or a client older than the server ends it, and the stock message shows. A
# try the account connector never answers - it tells Python nothing when the
# line drops in the handshake - is given up after forty seconds.
#
# The select window enters the character by the stock auto-select
# (stream.isAutoSelect and the slot the stream kept from the last StartGame),
# set only for the connection this module makes and cleared on the first frame
# of the game or when the autologin stops.
#
# Python 2.7 as the client has it, and 3 for tests/autologin_test.py.
# Player-visible strings are CP1250 escapes, so the file itself is ASCII.

import clientclock

FIRST_DELAY = 3.0
RETRY_DELAYS = (5.0, 10.0, 20.0, 30.0)
ALREADY_DELAY = 10.0
CONNECT_TIMEOUT = 40.0
# The login window back before the game was: the connection held and the
# select or the loading phase fell through. A few of those are a server still
# starting; more is a way back to the login the player took, or a loop.
MAX_BREAKS = 3
RESUME_DELAY = 5.0
# A drop opens the login window a curtain's fade after the game closed. A game
# that closed into the select window (a way out nobody told NoteManualExit
# about) must not start an autologin when the player reaches the login later.
FROM_GAME_WINDOW = 30.0
RETRY_ERRORS = ('ALREADY', 'FULL', 'SHUTDOWN', 'FAILURE', 'MAINTENA', 'NOTAVAIL')

# What GameFrame says about a frame of the game phase.
NOTHING = 0
NEW_SESSION = 1
RECONNECTED = 2
RESUME = 3

_s = {}


def Reset():
	"""Everything forgotten - the state of a client just started. For the tests too."""
	_s.clear()
	_s.update({
		'armed': False,       # the switch, for the character in play
		'inGame': False,      # a game phase is open
		'manualExit': False,  # the player logged out or changed character
		'fromGame': False,    # the game closed by itself and the login has not opened yet
		'closedAt': 0.0,
		'reconnecting': False,
		'connecting': False,
		'connectSince': 0.0,
		'attempt': 0,
		'breaks': 0,
		'nextTry': 0.0,
		'resumeHunt': False,  # the hunt was running when the game dropped
		'resumeAt': 0.0,
		'popup': False,       # our popup is the one open
		'shownLeft': -1,
		'stream': None,
	})


Reset()


def _Log(text):
	try:
		import dbg
		dbg.TraceError('AUTOLOGIN: %s' % text)
	except Exception:
		pass


def SetArmed(on):
	_s['armed'] = bool(on)


def IsArmed():
	return _s['armed']


def IsReconnecting():
	return _s['reconnecting']


def NoteManualExit():
	"""The system menu's logout or change of character: the next login window
	is the player's own."""
	_s['manualExit'] = True


def NoteGameClosed(huntRunning):
	"""The game window closed (Hunter.Destroy): a drop unless the player asked
	for it, and only while the switch is on."""
	_s['inGame'] = False
	if _s['manualExit'] or not _s['armed']:
		_s['fromGame'] = False
		_s['resumeHunt'] = False
	else:
		_s['fromGame'] = True
		_s['closedAt'] = clientclock.Now()
		_s['resumeHunt'] = bool(huntRunning)
	_s['manualExit'] = False


def DelayResume(seconds):
	_s['resumeAt'] = clientclock.Now() + seconds


def GameFrame():
	"""Every frame of a game phase (Hunter.CanUpdate). NEW_SESSION or
	RECONNECTED on the first one, RESUME once when a paused hunt is due."""
	try:
		now = clientclock.Now()
		if not _s['inGame']:
			_s['inGame'] = True
			_s['manualExit'] = False
			_s['fromGame'] = False
			_ResetAutoSelect()
			if _s['reconnecting']:
				_s['reconnecting'] = False
				_s['connecting'] = False
				_s['attempt'] = 0
				_s['breaks'] = 0
				_s['resumeAt'] = (now + RESUME_DELAY) if _s['resumeHunt'] else 0.0
				_s['resumeHunt'] = False
				return RECONNECTED
			# Another character, or the same one entered by hand: its own
			# switch decides, read with its settings (Hunter.LoadConfig).
			_s['armed'] = False
			_s['resumeAt'] = 0.0
			_s['resumeHunt'] = False
			return NEW_SESSION
		if _s['resumeAt'] and now >= _s['resumeAt']:
			_s['resumeAt'] = 0.0
			return RESUME
	except Exception as error:
		_Log('GameFrame: %s' % error)
	return NOTHING


def _ResetAutoSelect():
	stream = _s['stream']
	if stream is not None:
		try:
			stream.isAutoSelect = 0
		except Exception:
			pass


def _Stop(reason):
	if _s['reconnecting']:
		_Log('stopped: %s (after %d tries)' % (reason, _s['attempt']))
	_s['reconnecting'] = False
	_s['connecting'] = False
	_s['resumeHunt'] = False
	_ResetAutoSelect()


def Cancel():
	"""The popup's button, or Escape: the player takes the login over."""
	_s['popup'] = False
	if _s['reconnecting']:
		_Stop('cancelled')


def _Popup(win):
	try:
		return win.stream.popupWindow
	except Exception:
		return None


def _ClosePopupQuietly(win):
	# The popup runs its close event on every Close, and ours is Cancel: a
	# popup closed for the next try must not end the autologin.
	popup = _Popup(win)
	if popup:
		try:
			popup.CloseEvent = 0
			popup.Close()
		except Exception:
			pass
	_s['popup'] = False


def _CloseConnectingDialog(win):
	dialog = getattr(win, 'connectingDialog', None)
	if dialog:
		try:
			dialog.Close()
		except Exception:
			pass
		win.connectingDialog = None


def _Left(now):
	return max(1, int(_s['nextTry'] - now + 0.999))


def WaitText(left, attempt):
	return 'Autologin: ponownie za %d s (pr\xf3ba %d)' % (left, attempt)


def _ShowWaiting(win, now):
	popup = _Popup(win)
	if not popup:
		return
	try:
		import localeInfo
		cancel = localeInfo.UI_CANCEL
	except Exception:
		cancel = 'Anuluj'
	left = _Left(now)
	_s['shownLeft'] = left
	popup.Open(WaitText(left, _s['attempt'] + 1), Cancel, cancel)
	_s['popup'] = True


def _Schedule(win, now, delay):
	_s['nextTry'] = now + delay
	_ClosePopupQuietly(win)
	_ShowWaiting(win, now)


def _Backoff():
	index = min(max(_s['attempt'], 1), len(RETRY_DELAYS)) - 1
	return RETRY_DELAYS[index]


def OnLoginOpen(win):
	"""The end of LoginWindow.Open."""
	try:
		_s['stream'] = win.stream
		now = clientclock.Now()
		if _s['fromGame']:
			_s['fromGame'] = False
			if now - _s['closedAt'] > FROM_GAME_WINDOW:
				_s['resumeHunt'] = False
				return
			if not getattr(win.stream, 'id', '') or not getattr(win.stream, 'pwd', ''):
				return
			_s['reconnecting'] = True
			_s['connecting'] = False
			_s['attempt'] = 0
			_s['breaks'] = 0
			_Schedule(win, now, FIRST_DELAY)
		elif _s['reconnecting']:
			_s['breaks'] += 1
			_s['connecting'] = False
			if _s['breaks'] > MAX_BREAKS:
				_Stop('the login window came back %d times' % _s['breaks'])
				return
			_Schedule(win, now, _Backoff())
	except Exception as error:
		_Log('OnLoginOpen: %s' % error)


def PumpLogin(win):
	"""Every frame of the login window (LoginWindow.OnUpdate)."""
	try:
		if not _s['reconnecting']:
			return
		now = clientclock.Now()
		if _s['connecting']:
			if now - _s['connectSince'] >= CONNECT_TIMEOUT:
				_s['connecting'] = False
				_Log('try %d unanswered' % _s['attempt'])
				_Schedule(win, now, _Backoff())
			return
		if now >= _s['nextTry']:
			stream = win.stream
			if not getattr(stream, 'id', '') or not getattr(stream, 'pwd', ''):
				_ClosePopupQuietly(win)
				_Stop('no login in the stream')
				return
			_ClosePopupQuietly(win)
			stream.isAutoSelect = 1
			_s['attempt'] += 1
			_s['connecting'] = True
			_s['connectSince'] = now
			win.Connect(stream.id, stream.pwd)
			return
		if not _s['popup']:
			_ShowWaiting(win, now)
			return
		left = _Left(now)
		if left != _s['shownLeft']:
			_s['shownLeft'] = left
			popup = _Popup(win)
			if popup:
				popup.GetChild('message').SetText(WaitText(left, _s['attempt'] + 1))
	except Exception as error:
		_Log('PumpLogin: %s' % error)


def OnConnectFailure(win):
	"""The top of LoginWindow.OnConnectFailure: True when the failure was one
	of our tries and the next is scheduled."""
	try:
		if not _s['reconnecting']:
			return False
		_s['connecting'] = False
		_CloseConnectingDialog(win)
		_Schedule(win, clientclock.Now(), _Backoff())
		return True
	except Exception as error:
		_Log('OnConnectFailure: %s' % error)
		return False


def OnLoginFailure(win, error):
	"""The top of LoginWindow.OnLoginFailure: True when it is retried, False
	to let the stock message show (and then the autologin has stopped)."""
	try:
		if not _s['reconnecting']:
			return False
		_s['connecting'] = False
		if error in RETRY_ERRORS:
			_CloseConnectingDialog(win)
			delay = ALREADY_DELAY if error == 'ALREADY' else _Backoff()
			_Schedule(win, clientclock.Now(), delay)
			return True
		_Stop('login refused: %s' % error)
		return False
	except Exception as failure:
		_Log('OnLoginFailure: %s' % failure)
		return False
