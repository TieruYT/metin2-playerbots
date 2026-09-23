# A bot's status over its head, and not a line in the chat history.
#
# The server (SendPlayerBotOverheadChat in playerbot_status.h) sends a bot's
# status as the command "PlayerBotStatus <vid> <hex>" rather than as talking:
# the client puts every talking packet from a character into the chat history
# beside its text tail (RecvChatPacket), so a town full of bots filled the chat
# window. The text travels as hex because the command parser splits its line on
# spaces; the bytes are the status's own CP1250, shown as they came. The native
# tail replaces the bubble of the same VID and ignores a VID nobody can see.
#
# Written for the client's Python 2.7 without a module it might lack, and
# checked on Python 3 as well (tests/playerbot_status_tail_test.py).

MAX_STATUS_BYTES = 159
HEX_DIGITS = "0123456789abcdefABCDEF"


def decode_status(vid_arg, hex_arg):
	try:
		vid = int(vid_arg)
	except (ValueError, TypeError):
		return None
	if vid <= 0 or vid > 0xffffffff:
		return None
	if not hex_arg or len(hex_arg) > MAX_STATUS_BYTES * 2 or len(hex_arg) % 2:
		return None
	chars = []
	for i in range(0, len(hex_arg), 2):
		pair = hex_arg[i:i + 2]
		if pair[0] not in HEX_DIGITS or pair[1] not in HEX_DIGITS:
			return None
		value = int(pair, 16)
		if value < 32 or value == 127:
			return None
		chars.append(chr(value))
	# RegisterChatTail reads the VID as a signed int and keeps it as a DWORD.
	if vid >= 0x80000000:
		vid -= 0x100000000
	return vid, "".join(chars)


# Since server 2.2.6 the command may carry the same status in English as a
# third word: "PlayerBotStatus <vid> <hex> <hex_en>". A client whose language is
# not Polish draws the English one - English being what a Romanian or a German
# reads before Polish - and a monster's or an item's name in it arrives as
# "{m<vnum>}" or "{i<vnum>}", filled in here from the client's own tables, in
# the client's own language. Anything that does not decode falls back to the
# Polish line, so an older server and a broken word both still show a status.
PLACEHOLDER_OPEN = "{"
PLACEHOLDER_CLOSE = "}"
PLACEHOLDER_MAX = 8


def client_language():
	try:
		import systemSetting
		return systemSetting.GetLanguage()
	except Exception:
		return "pl"


def monster_name(vnum):
	try:
		import nonplayer
		name = nonplayer.GetMonsterName(vnum)
		if name:
			return name
	except Exception:
		pass
	return "?"


def item_name(vnum):
	# item.GetItemName names the selected item and ignores an argument, so
	# the item is selected first, the way every tooltip does it (an unknown
	# vnum selects 60001 and says so in syserr.txt; the server sends none).
	try:
		import item
		item.SelectItem(vnum)
		name = item.GetItemName()
		if name:
			return name
	except Exception:
		pass
	return "?"


def expand_names(text):
	"""Every {m<vnum>} and {i<vnum>} replaced by the client's own name for it."""
	out = []
	i = 0
	while i < len(text):
		start = text.find(PLACEHOLDER_OPEN, i)
		if start < 0:
			out.append(text[i:])
			break
		end = text.find(PLACEHOLDER_CLOSE, start)
		out.append(text[i:start])
		if end < 0:
			out.append(text[start:])
			break
		token = text[start + 1:end]
		kind, digits = token[:1], token[1:]
		if kind in ("m", "i") and digits.isdigit() and len(digits) <= PLACEHOLDER_MAX:
			vnum = int(digits)
			out.append(monster_name(vnum) if kind == "m" else item_name(vnum))
		else:
			out.append(text[start:end + 1])
		i = end + 1
	return "".join(out)


def choose_status(vid_arg, hex_arg, hex_en=None, language=None):
	"""(vid, text) to draw, or None: the English line for a non-Polish client."""
	status = decode_status(vid_arg, hex_arg)
	if status is None:
		return None
	if hex_en is None:
		return status
	if language is None:
		language = client_language()
	if language == "pl":
		return status
	english = decode_status(vid_arg, hex_en)
	if english is None:
		return status
	return english[0], expand_names(english[1])


def show(vid_arg, hex_arg, *rest):
	status = choose_status(vid_arg, hex_arg, rest[0] if rest else None)
	if status is None:
		return
	import textTail
	textTail.RegisterChatTail(status[0], status[1])


# A bot's personality, on its own row between its nickname and its guild name
# (2026-09-16: textTail.AttachPersonality, a new CPythonTextTail row added
# specifically for this - see UserInterface/PythonTextTail.cpp/.h. Before that
# this used AttachTitle and sat where the alignment title (ranga) stands,
# fighting the client over that one spot; the two are independent now, so the
# classic alignment title is never touched and always shows.
#
# The server (ManagePlayerBotPersonalityTitle in playerbot_status.h) sends
# "PlayerBotTitle <vid> <personality>" while a player is near, about every ten
# seconds. AttachPersonality is safe to call every second - unlike AttachTitle
# it isn't fighting anything for its slot - so the keeper, one of game.py's
# updateables, refreshes every bot heard from in the last minute anyway, simply
# to expire ones nobody has heard from in a while. The names are the classic
# panel's, in CP1250 like every name the client draws.

PERSONALITY_TITLES = {
	0: "Wytrwa\xb3y poszukiwacz",
	1: "Pogromca Metin\xf3w",
	2: "Towarzysz dru\xbfyny",
	3: "Mistrz ekwipunku",
	4: "Rozwa\xbfny zbieracz",
	5: "Handlarz",
	6: "W\xeadrowiec",
	7: "Dropek Metin\xf3w",
	8: "Dropek z M3",
	9: "Dropek z M2",
	10: "Dropek medali",
	# Iwakura's personalities (playerbot_persona_rules.h, PERSONA_TITLE_BASE +
	# EPersona): under the server's PERSONA switch the title is the one that
	# claims the bot now, and it changes with what the bot is doing.
	100: "Grinder",
	101: "Zdobywca",
	102: "Handlarz",
	103: "Hazardzista",
	104: "Perfekcjonista",
	105: "Pogromca Metin\xf3w",
	106: "G\xf3rnik",
	107: "Rybak",
	108: "Najemnik",
	109: "Towarzysz",
}

# Iwakura's names are Polish, and the English client shows English ones.
import systemSetting
if systemSetting.GetLanguage() == "en":
	PERSONALITY_TITLES.update({0: 'Persistent Explorer', 1: 'Metin Slayer', 2: 'Team Player', 3: 'Equipment Master', 4: 'Careful Gatherer', 5: 'Trader', 6: 'Wanderer', 7: 'Metin Farmer', 8: 'M3 Farmer', 9: 'M2 Farmer', 10: 'Medal Farmer', 100: 'Grinder', 101: 'Conqueror', 102: 'Trader', 103: 'Gambler', 104: 'Perfectionist', 105: 'Metin Slayer', 106: 'Miner', 107: 'Fisher', 108: 'Mercenary', 109: 'Companion'})

PERSONALITY_COLOURS = {
	0: (0.75, 0.85, 1.0),
	1: (1.0, 0.65, 0.3),
	2: (0.45, 0.9, 0.95),
	3: (1.0, 0.85, 0.35),
	4: (0.7, 0.95, 0.6),
	5: (1.0, 0.95, 0.5),
	6: (0.85, 0.75, 1.0),
	7: (1.0, 0.6, 0.6),
	8: (0.95, 0.7, 0.95),
	9: (0.8, 0.85, 0.65),
	10: (0.95, 0.8, 0.55),
	100: (0.8, 0.8, 0.8),
	101: (0.55, 0.95, 0.55),
	102: (1.0, 0.95, 0.5),
	103: (1.0, 0.55, 0.85),
	104: (1.0, 0.85, 0.35),
	105: (1.0, 0.65, 0.3),
	106: (0.75, 0.65, 0.5),
	107: (0.45, 0.8, 1.0),
	108: (1.0, 0.45, 0.45),
	109: (0.45, 0.9, 0.95),
}

TITLE_REFRESH_SECONDS = 1.0
TITLE_FORGET_SECONDS = 60.0

# The player's own switch, "Tytuly botow" in the game options: whether a bot's
# personality shows in its own row at all (NerrVoVy, 15 September; moved off
# AttachTitle onto its own row 2026-09-16). One line in playerbot_titles.cfg
# beside the client, because systemSetting has no key of its own for it; a
# missing or unreadable file means on. Off, nothing is attached and the keeper
# forgets what it held; textTail.DetachPersonality clears anything already
# showing. Either way the classic alignment title (ranga) is untouched - this
# switch no longer has anything to do with it.
TITLES_CONFIG_FILE = "playerbot_titles.cfg"
TITLES_CONFIG_KEY = "personality_titles"
_titlesEnabled = None


def TitlesConfigText(enabled):
	return "%s=%d\n" % (TITLES_CONFIG_KEY, 1 if enabled else 0)


def TitlesEnabledFromText(text):
	for line in (text or "").splitlines():
		key, sep, value = line.strip().partition("=")
		if sep and key.strip() == TITLES_CONFIG_KEY:
			return value.strip() != "0"
	return True


def TitlesEnabled():
	global _titlesEnabled
	if _titlesEnabled is None:
		try:
			f = open(TITLES_CONFIG_FILE, "r")
			try:
				_titlesEnabled = TitlesEnabledFromText(f.read())
			finally:
				f.close()
		except (IOError, OSError):
			_titlesEnabled = True
	return _titlesEnabled


def SetTitlesEnabled(enabled):
	global _titlesEnabled
	_titlesEnabled = bool(enabled)
	try:
		f = open(TITLES_CONFIG_FILE, "w")
		try:
			f.write(TitlesConfigText(_titlesEnabled))
		finally:
			f.close()
	except (IOError, OSError):
		pass
	if not _titlesEnabled:
		keeper = GetTitleKeeper()
		import textTail
		if hasattr(textTail, "DetachPersonality"):
			for vid in keeper.titles.keys():
				textTail.DetachPersonality(vid)
		keeper.Destroy()
	return _titlesEnabled


def decode_title(vid_arg, personality_arg):
	try:
		vid = int(vid_arg)
		personality = int(personality_arg)
	except (ValueError, TypeError):
		return None
	if vid <= 0 or vid > 0xffffffff or personality not in PERSONALITY_TITLES:
		return None
	# AttachTitle reads the VID as a signed int, like RegisterChatTail.
	if vid >= 0x80000000:
		vid -= 0x100000000
	return vid, personality


def attach_title(vid, personality):
	import textTail
	if not hasattr(textTail, "AttachPersonality"):
		return False
	(r, g, b) = PERSONALITY_COLOURS.get(personality, (1.0, 1.0, 1.0))
	textTail.AttachPersonality(vid, PERSONALITY_TITLES[personality], r, g, b)
	return True


class TitleKeeper(object):
	"""One of game.py's updateables: every title heard from, attached again."""

	def __init__(self):
		self.titles = {}
		self.nextRefresh = 0.0

	def Remember(self, vid, personality, now):
		self.titles[vid] = (personality, now)

	def CanUpdate(self):
		return bool(self.titles)

	def OnUpdate(self):
		if not TitlesEnabled():
			self.titles = {}
			return
		import clientclock
		now = clientclock.Now()
		if now < self.nextRefresh:
			return
		self.nextRefresh = now + TITLE_REFRESH_SECONDS
		for vid, (personality, heard) in list(self.titles.items()):
			if now - heard > TITLE_FORGET_SECONDS:
				del self.titles[vid]
				continue
			attach_title(vid, personality)

	def Destroy(self):
		self.titles = {}


_keeper = None


def GetTitleKeeper():
	global _keeper
	if _keeper is None:
		_keeper = TitleKeeper()
	return _keeper


def show_title(vid_arg, personality_arg):
	if not TitlesEnabled():
		return False
	decoded = decode_title(vid_arg, personality_arg)
	if decoded is None or not attach_title(decoded[0], decoded[1]):
		return False
	import clientclock
	GetTitleKeeper().Remember(decoded[0], decoded[1], clientclock.Now())
	return True
