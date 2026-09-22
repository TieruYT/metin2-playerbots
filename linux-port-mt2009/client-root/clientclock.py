# A clock for our client scripts that never runs backwards.
#
# app.GetTime() is CPythonApplication's global time, read off CTimer, and
# CPythonNetworkStream's handshake calls CTimer::SetBaseTime() on every
# connection to a core - at the login and at every warp - so it starts again
# from zero on each map. A deadline a script took before a warp ("not again
# for half a second", "the answer is due in five") was then as far in the
# future as the character had played on the map before, and the ` key picked
# up nothing in M2 and the valley after an hour in M1 (GoracyDelfin,
# 20 September): it worked only where the character had not been long yet.
#
# Now() is app.GetTime() plus whatever the clock has lost at its resets, so a
# script's own times keep their order across a warp. Every caller shares the
# one offset, which is all a module-level value in the client can be.
#
# Python 2.7 as the client has it.

_state = {'last': None, 'offset': 0.0}


def Now():
	# Imported here, as the other modules do, so a test's stub is the one read.
	import app
	raw = app.GetTime()
	last = _state['last']
	if last is not None and raw < last:
		_state['offset'] += last - raw
	_state['last'] = raw
	return raw + _state['offset']


def Reset():
	"""For the tests: forget what the clock has seen."""
	_state['last'] = None
	_state['offset'] = 0.0
