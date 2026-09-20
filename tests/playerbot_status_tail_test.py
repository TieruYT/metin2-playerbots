# -*- coding: utf-8 -*-
"""playerbot_status_tail.py - the client root's half of a bot's status bubble.

    docker run --rm -v "$(pwd -W)":/w -w /w python:2.7-slim python tests/playerbot_status_tail_test.py
    docker run --rm -v "$(pwd -W)":/w -w /w python:3.12-slim python tests/playerbot_status_tail_test.py

The client runs Python 2.7, so the module is written for it and checked on both.
"""
import os
import sys
import types
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'linux-port-mt2009', 'client-root'))
# The module asks the engine which language the client runs in, to show the
# personality names in it (client 2.0.23). Polish here, so the values below
# are the ones an ordinary Polish client draws.
_setting = types.ModuleType('systemSetting')
_setting.GetLanguage = lambda: 'pl'
sys.modules['systemSetting'] = _setting
import playerbot_status_tail as status


def hexed(raw):
    return ''.join('%02x' % b for b in bytearray(raw))


def as_bytes(text):
    if isinstance(text, type(u'')):
        return bytearray(text, 'latin-1')
    return bytearray(text)


class StatusTailTest(unittest.TestCase):
    def test_cp1250_spaces_and_colon_arrive_as_sent(self):
        raw = u'Idę do kowala: żółć, ąęśćń!'.encode('cp1250')
        vid, text = status.decode_status('42', hexed(raw))
        self.assertEqual(vid, 42)
        self.assertEqual(as_bytes(text), bytearray(raw))

    def test_upper_case_hex_is_accepted(self):
        self.assertEqual(status.decode_status('7', '4142')[1], 'AB')

    def test_refused(self):
        for vid, text in [('0', '61'), ('-1', '61'), ('4294967296', '61'), ('x', '61'),
                          (None, '61'), ('1', ''), ('1', None), ('1', 'f'), ('1', 'zz'),
                          ('1', '+1'), ('1', ' 1'), ('1', '00'), ('1', '0a'), ('1', '1f'),
                          ('1', '7f'), ('1', '61' * 160)]:
            self.assertIsNone(status.decode_status(vid, text), (vid, text))

    def test_limits(self):
        self.assertEqual(status.decode_status('4294967295', '61'), (-1, 'a'))
        self.assertEqual(status.decode_status('2147483648', '61'), (-2147483648, 'a'))
        self.assertEqual(status.decode_status('1', '61' * 159), (1, 'a' * 159))

    def test_only_the_native_tail_is_called(self):
        calls = []
        native = types.ModuleType('textTail')
        native.RegisterChatTail = lambda vid, text: calls.append((vid, text))
        old = sys.modules.get('textTail')
        sys.modules['textTail'] = native
        try:
            status.show('42', '6162')
            status.show('42', '6364')
            status.show('42', 'xx')
            status.show('0', '6162')
            self.assertEqual(calls, [(42, 'ab'), (42, 'cd')])
        finally:
            if old is None:
                del sys.modules['textTail']
            else:
                sys.modules['textTail'] = old


class TitleTest(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.now = [100.0]
        native = types.ModuleType('textTail')
        native.AttachPersonality = lambda vid, text, r, g, b: self.calls.append((vid, text))
        clock = types.ModuleType('app')
        clock.GetTime = lambda: self.now[0]
        self.saved = dict((name, sys.modules.get(name)) for name in ('textTail', 'app'))
        sys.modules['textTail'] = native
        sys.modules['app'] = clock
        status._keeper = None

    def tearDown(self):
        for name, module in self.saved.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module
        status._keeper = None

    def test_decoding(self):
        self.assertEqual(status.decode_title('42', '3'), (42, 3))
        self.assertEqual(status.decode_title('4294967295', '10'), (-1, 10))
        for vid, personality in [('0', '1'), ('42', '11'), ('42', '-1'), ('x', '1'),
                                 ('42', None), ('4294967296', '1')]:
            self.assertIsNone(status.decode_title(vid, personality), (vid, personality))

    def test_every_personality_has_a_title_and_a_colour(self):
        # The old eleven, and Iwakura's ten personalities at 100 + EPersona.
        expected = list(range(11)) + list(range(100, 110))
        self.assertEqual(sorted(status.PERSONALITY_TITLES), expected)
        self.assertEqual(sorted(status.PERSONALITY_COLOURS), expected)
        self.assertEqual(status.decode_title('42', '105'), (42, 105))
        self.assertIsNone(status.decode_title('42', '110'))
        self.assertIsNone(status.decode_title('42', '255'))

    def test_attached_and_kept_for_a_minute(self):
        self.assertTrue(status.show_title('42', '1'))
        self.assertEqual(self.calls, [(42, status.PERSONALITY_TITLES[1])])
        keeper = status.GetTitleKeeper()
        self.assertTrue(keeper.CanUpdate())
        self.now[0] += 1.5
        keeper.OnUpdate()
        self.assertEqual(len(self.calls), 2)
        self.now[0] += 0.5
        keeper.OnUpdate()
        self.assertEqual(len(self.calls), 2)
        self.now[0] += 61.0
        keeper.OnUpdate()
        self.assertEqual(len(self.calls), 2)
        self.assertFalse(keeper.CanUpdate())

    def test_a_client_without_attach_personality_draws_nothing(self):
        # The personality has its own row since client 2.0.13 (l0st3k's
        # AttachPersonality); an older exe has no such row to draw in.
        del sys.modules['textTail'].AttachPersonality
        self.assertFalse(status.show_title('42', '1'))
        self.assertEqual(self.calls, [])


class TitleSwitchTest(unittest.TestCase):
    """The player's "Tytuly botow" switch, kept in playerbot_titles.cfg."""

    def setUp(self):
        import tempfile
        self.calls = []
        native = types.ModuleType('textTail')
        native.AttachPersonality = lambda vid, text, r, g, b: self.calls.append((vid, text))
        clock = types.ModuleType('app')
        clock.GetTime = lambda: 100.0
        self.saved = dict((name, sys.modules.get(name)) for name in ('textTail', 'app'))
        sys.modules['textTail'] = native
        sys.modules['app'] = clock
        self.tmp = tempfile.mkdtemp()
        self.savedFile = status.TITLES_CONFIG_FILE
        status.TITLES_CONFIG_FILE = os.path.join(self.tmp, 'playerbot_titles.cfg')
        status._titlesEnabled = None
        status._keeper = None

    def tearDown(self):
        import shutil
        for name, module in self.saved.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module
        status.TITLES_CONFIG_FILE = self.savedFile
        status._titlesEnabled = None
        status._keeper = None
        shutil.rmtree(self.tmp, True)

    def test_on_without_a_file(self):
        self.assertTrue(status.TitlesEnabled())
        self.assertTrue(status.show_title('42', '1'))
        self.assertEqual(self.calls, [(42, status.PERSONALITY_TITLES[1])])

    def test_off_attaches_nothing_and_the_keeper_forgets(self):
        self.assertTrue(status.show_title('42', '1'))
        keeper = status.GetTitleKeeper()
        self.assertTrue(keeper.CanUpdate())
        self.assertFalse(status.SetTitlesEnabled(False))
        self.assertFalse(keeper.CanUpdate())
        self.assertFalse(status.show_title('43', '2'))
        keeper.OnUpdate()
        self.assertEqual(self.calls, [(42, status.PERSONALITY_TITLES[1])])
        self.assertTrue(status.SetTitlesEnabled(True))
        self.assertTrue(status.show_title('43', '2'))
        self.assertEqual(self.calls[-1], (43, status.PERSONALITY_TITLES[2]))

    def test_the_choice_survives_a_restart(self):
        status.SetTitlesEnabled(False)
        status._titlesEnabled = None   # a new client process reads the file
        self.assertFalse(status.TitlesEnabled())
        status.SetTitlesEnabled(True)
        status._titlesEnabled = None
        self.assertTrue(status.TitlesEnabled())
        f = open(status.TITLES_CONFIG_FILE)
        try:
            self.assertEqual(f.read().strip(), 'personality_titles=1')
        finally:
            f.close()

    def test_a_file_the_client_cannot_write_still_switches_for_the_session(self):
        status.TITLES_CONFIG_FILE = os.path.join(self.tmp, 'no', 'such', 'dir', 'x.cfg')
        self.assertFalse(status.SetTitlesEnabled(False))
        self.assertFalse(status.TitlesEnabled())
        status._titlesEnabled = None
        self.assertTrue(status.TitlesEnabled())

    def test_text_forms(self):
        self.assertTrue(status.TitlesEnabledFromText(''))
        self.assertTrue(status.TitlesEnabledFromText(None))
        self.assertTrue(status.TitlesEnabledFromText('personality_titles=1\n'))
        self.assertFalse(status.TitlesEnabledFromText(' personality_titles = 0 \r\n'))
        self.assertTrue(status.TitlesEnabledFromText('other=0\n'))
        self.assertEqual(status.TitlesConfigText(False), 'personality_titles=0\n')


if __name__ == '__main__':
    unittest.main()
