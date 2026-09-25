# The taskbar's Towarzysz and Auto Lowy buttons (clientrootify.py): the
# rendered uiscript/taskbar.py executed the way the client's loader executes
# it, at a few screen widths.
#
#     python tests/client_taskbar_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/client_taskbar_test.py).
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))


class Anything(object):
	"""A client module the script reads a constant of: any name is itself."""

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return name

	def GetPublic(self, key):
		return 'public/' + key


for _name in ('uiScriptLocale', 'app', 'flamewindPath'):
	sys.modules[_name] = Anything()


def children(width):
	path = os.path.join(CLIENT_ROOT, 'uiscript', 'taskbar.py')
	with open(path, 'rb') as f:
		source = f.read()
	namespace = {'SCREEN_WIDTH': width, 'SCREEN_HEIGHT': 768}
	exec(compile(source, path, 'exec'), namespace)
	return dict((child.get('name'), child) for child in namespace['window']['children'])


class TaskbarTest(unittest.TestCase):
	def test_the_two_buttons_stand_left_of_the_character_button(self):
		found = children(1024)
		self.assertEqual(found['SidekickButton']['x'], 1024 - 205)
		self.assertEqual(found['AutoHuntButton']['x'], 1024 - 171)
		self.assertEqual(found['CharacterButton']['x'], 1024 - 137)
		self.assertEqual(found['SidekickButton']['y'], found['CharacterButton']['y'])
		self.assertEqual(found['SidekickButton']['default_image'], 'playerbot_ui/sidekick_button_01.tga')

	def test_they_never_reach_the_right_mouse_button(self):
		for width in (940, 1024, 1280, 1920):
			found = children(width)
			if 'SidekickButton' not in found:
				continue
			mouse_right_edge = found['RightMouseButton']['x'] + 32
			self.assertLess(mouse_right_edge, found['SidekickButton']['x'], width)

	def test_a_narrow_bar_goes_without_them(self):
		for width in (800, 939):
			found = children(width)
			self.assertNotIn('SidekickButton', found)
			self.assertNotIn('AutoHuntButton', found)
			self.assertIn('CharacterButton', found)

	def test_the_pictures_are_in_the_root(self):
		for name in ('sidekick', 'autohunt'):
			for state in ('01', '02', '03'):
				path = os.path.join(CLIENT_ROOT, 'playerbot_ui', '%s_button_%s.tga' % (name, state))
				with open(path, 'rb') as f:
					header = bytearray(f.read(18))
				# RLE true colour, 32x32, 32 bits, bottom-left origin: the
				# originals' own format (d:/ymir work/ui/game/taskbar/mall_button_01.tga).
				self.assertEqual((header[2], header[12] | header[13] << 8, header[14] | header[15] << 8,
					header[16], header[17]), (10, 32, 32, 32, 8), path)


if __name__ == '__main__':
	unittest.main()
