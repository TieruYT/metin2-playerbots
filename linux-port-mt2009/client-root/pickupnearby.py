# "Podnies caly drop" - the ` key (game.py). Z stays the classic single
# pickup; ` asks the server once, and the server picks up every item within
# the pickup range that is this character's or its party's, nearest first,
# the way the Z key picks up one (CHARACTER::PickupNearbyItems,
# /pickup_nearby). A held key repeats; the server allows one batch every half
# second and the client asks no more often than that, so the command limit
# (five in half a second) is never what stops it.
#
# Python 2.7 as the client has it.

import app
import clientclock
import net

MIN_INTERVAL = 0.5

_state = {'next': 0.0}


def Request():
	now = clientclock.Now()
	if now < _state['next']:
		return False
	_state['next'] = now + MIN_INTERVAL
	net.SendChatPacket('/pickup_nearby')
	return True
