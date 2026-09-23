# Auto Lowy without a client: the hunt's decisions against stub client modules.
#
#     python tests/uiautohunt_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/uiautohunt_test.py).
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}


def reset_state():
	STATE.clear()
	STATE.update({
		'now': 100.0, 'chat': [], 'commands': [], 'used': [], 'race': 0, 'group': 1,
		'status': {1: 100, 2: 100, 3: 100, 4: 100}, 'pos': (1000, 1000), 'distance': {},
		'where': {}, 'bag': {}, 'targets': [], 'attack': [], 'picked': [], 'skills': {},
		'cooling': set(), 'active': set(), 'cast': [], 'walks': [], 'rotations': [],
		'toggles': set(), 'bag_reads': 0, 'protos': {}, 'target': 0, 'cleared': 0,
		'dead': set(), 'bow': False, 'circles': [],
	})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class StubWidget(object):
	"""Any client widget: every call it does not know is accepted and does
	nothing, so a window can be built; where it was put and what an edit
	holds are kept for the tests."""

	def __init__(self, *args, **kwargs):
		self.position = None
		self.text = ''
		self.shown = False

	def SetPosition(self, x, y):
		self.position = (x, y)

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

	def GetLocalPosition(self):
		return self.position or (0, 0)

	def SetSliderPos(self, pos):
		self.sliderPos = pos

	def GetSliderPos(self):
		return getattr(self, 'sliderPos', 0.0)

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return lambda *args, **kwargs: None


StubBoard = StubWidget


def read_bag(cell):
	STATE['bag_reads'] += 1
	return STATE['bag'].get(cell, 0)


def select_item(vnum):
	STATE['selected'] = vnum


def proto(index):
	return STATE['protos'].get(STATE.get('selected'), (0, 0, (0,) * 6))[index]


def set_target(vid):
	STATE['targets'].append(vid)
	STATE['target'] = vid


def clear_target():
	STATE['cleared'] += 1
	STATE['target'] = 0


# What the client 2.0.25 exe adds to the player module (clientify
# apply_auto_hunt_circle). The window asks for each with hasattr or under
# AttributeError, so the stub module has none of them unless a test puts
# them there.
NEW_EXE = {
	'IsBowEquipped': lambda: STATE['bow'],
	'IsTargetDead': lambda vid: vid in STATE['dead'],
	'SetAutoHuntRangeCircle': lambda *args: STATE['circles'].append(args),
}


def with_new_exe(test):
	stub = sys.modules['player']
	for name, func in NEW_EXE.items():
		setattr(stub, name, func)

	def remove():
		for name in NEW_EXE:
			delattr(stub, name)
	test.addCleanup(remove)


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
	sys.modules['chat'] = module('chat', CHAT_TYPE_INFO=1, AppendChat=lambda kind, text: STATE['chat'].append(text))
	sys.modules['net'] = module(
		'net',
		SendChatPacket=lambda text: STATE['commands'].append(text),
		SendItemUsePacket=lambda cell: STATE['used'].append(cell),
		SendItemPickUpPacket=lambda vid: STATE['picked'].append(vid),
		GetMainActorRace=lambda: STATE['race'],
		GetMainActorSkillGroup=lambda: STATE['group'])
	sys.modules['player'] = module(
		'player', HP=1, MAX_HP=2, SP=3, MAX_SP=4, INVENTORY_MAX_NUM=90, SLOT_TYPE_INVENTORY=1, SLOT_TYPE_SKILL=2,
		GetStatus=lambda point: STATE['status'][point],
		GetMainCharacterIndex=lambda: 7,
		GetMainCharacterName=lambda: 'Tester',
		GetMainCharacterPosition=lambda: (STATE['pos'][0], STATE['pos'][1], 0),
		GetCharacterDistance=lambda vid: STATE['distance'].get(vid, -1),
		GetItemIndex=read_bag,
		SetTarget=set_target,
		GetTargetVID=lambda: STATE['target'],
		ClearTarget=clear_target,
		SetAttackKeyState=lambda on: STATE['attack'].append(on),
		GetSkillIndex=lambda slot: STATE['skills'].get(slot, 0),
		IsSkillCoolTime=lambda slot: slot in STATE['cooling'],
		IsSkillActive=lambda slot: slot in STATE['active'],
		ClickSkillSlot=lambda slot: STATE['cast'].append(slot))
	sys.modules['chr'] = module(
		'chr',
		GetPixelPosition=lambda vid: STATE['where'][vid],
		MoveToDestPosition=lambda vid, x, y: STATE['walks'].append((x, y)),
		SelectInstance=lambda vid: None,
		SetRotation=lambda degree: STATE['rotations'].append(degree),
		GetNameByVID=lambda vid: 'Wilk')
	sys.modules['skill'] = module('skill', IsToggleSkill=lambda index: index in STATE['toggles'])
	sys.modules['item'] = module(
		'item', ITEM_TYPE_USE=3, USE_POTION=0, USE_POTION_NODELAY=11,
		SelectItem=select_item,
		GetItemType=lambda: proto(0),
		GetItemSubType=lambda: proto(1),
		GetValue=lambda index: proto(2)[index])
	sys.modules['mouseModule'] = module('mouseModule')
	sys.modules['wndMgr'] = module('wndMgr', GetScreenWidth=lambda: 800, GetScreenHeight=lambda: 600)
	ui = module('ui', BoardWithTitleBar=StubBoard, ThinBoard=StubWidget, TextLine=StubWidget,
		Button=StubWidget, SlotWindow=StubWidget, SlotBar=StubWidget, EditLine=StubWidget, Bar=StubWidget,
		SliderBar=StubWidget)
	setattr(ui, '__mem_func__', lambda func: func)
	sys.modules['ui'] = ui


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import uiautohunt  # noqa: E402


def step(hunter, seconds=0.0):
	STATE['now'] += seconds
	hunter.OnUpdate()


def commands(prefix):
	return [text for text in STATE['commands'] if text.startswith(prefix)]


class HelpersTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		uiautohunt._manaItems.clear()

	def test_config_round_trip_ignores_what_it_does_not_know(self):
		config = uiautohunt.DefaultConfig()
		config['range'] = 3000
		config['skill11_slot'] = 5
		config['item8_vnum'] = 70038
		config['loot_armour'] = 0
		text = uiautohunt.ConfigText(config) + 'nonsense=1\nitem0_val=abc\nitem1_val=-7\n'
		loaded = uiautohunt.ConfigFromText(text)
		self.assertEqual(loaded['range'], 3000)
		self.assertEqual(loaded['skill11_slot'], 5)
		self.assertEqual(loaded['item8_vnum'], 70038)
		self.assertEqual(loaded['loot_armour'], 0)
		self.assertEqual(loaded['item0_val'], 60)
		self.assertEqual(loaded['item1_val'], 0)
		self.assertNotIn('nonsense', loaded)

	def test_a_file_from_the_toggle_window_gets_the_pick_up_back(self):
		old = 'range=3000\npickup=0\nloot_weapon=0\nloot_other=0\n'
		loaded = uiautohunt.ConfigFromText(old)
		self.assertEqual(loaded['range'], 3000)
		self.assertEqual(loaded['pickup'], 1)
		self.assertEqual(uiautohunt.LootMask(loaded), 127)
		self.assertEqual(loaded['config_version'], uiautohunt.CONFIG_VERSION)

	def test_a_current_file_keeps_the_pick_up_off(self):
		config = uiautohunt.DefaultConfig()
		config['pickup'] = 0
		loaded = uiautohunt.ConfigFromText(uiautohunt.ConfigText(config))
		self.assertEqual(loaded['pickup'], 0)
		self.assertEqual(uiautohunt.LootMask(loaded), 0)

	def test_a_file_of_the_window_before_moves_into_the_new_slots(self):
		old = ('range=3000\nstones=1\npickup=0\nrevive=0\nrevive_after=25\nreturn=0\n'
			'hp_vnum=27001\nhp_percent=55\nsp_vnum=27004\nsp_percent=35\n'
			'item0_vnum=70038\nitem0_interval=20\nitem1_vnum=0\nitem1_interval=30\nitem2_vnum=71016\nitem2_interval=40\n'
			'skill0_slot=1\nskill0_interval=0\nskill3_slot=4\nskill3_interval=12\n'
			'loot_weapon=0\nloot_armour=1\nconfig_version=2\n')
		loaded = uiautohunt.ConfigFromText(old)
		self.assertEqual((loaded['range'], loaded['stones'], loaded['pickup'], loaded['revive'], loaded['return']), (3000, 1, 0, 0, 0))
		self.assertEqual(loaded['revive_after'], 25)
		self.assertEqual((loaded['skill0_slot'], loaded['skill3_slot'], loaded['skill3_interval']), (1, 4, 12))
		self.assertEqual((loaded['item0_vnum'], loaded['item0_val']), (27001, 55))
		self.assertEqual((loaded['item1_vnum'], loaded['item1_val']), (27004, 35))
		self.assertEqual(loaded['item2_vnum'], 0)
		self.assertEqual((loaded['item6_vnum'], loaded['item6_val']), (70038, 20))
		self.assertEqual(loaded['item7_vnum'], 0)
		self.assertEqual((loaded['item8_vnum'], loaded['item8_val']), (71016, 40))
		self.assertEqual((loaded['loot_weapon'], loaded['loot_armour']), (0, 1))
		self.assertEqual((loaded['attack'], loaded['use_potions'], loaded['use_buffs'], loaded['use_skills']), (1, 1, 1, 1))
		self.assertEqual(loaded['config_version'], uiautohunt.CONFIG_VERSION)

	def test_a_file_of_the_six_item_clock_reads_as_it_was(self):
		# Colide's first window saved six items on a clock under version 4; his
		# second has twelve, and only added keys.
		saved = ('range=3000\nitem6_vnum=70038\nitem6_val=20\nitem11_vnum=71016\nitem11_val=40\n'
			'skill7_slot=3\nconfig_version=4\n')
		loaded = uiautohunt.ConfigFromText(saved)
		self.assertEqual(loaded['range'], 3000)
		self.assertEqual((loaded['item6_vnum'], loaded['item6_val']), (70038, 20))
		self.assertEqual((loaded['item11_vnum'], loaded['item11_val']), (71016, 40))
		self.assertEqual(loaded['skill7_slot'], 3)
		self.assertEqual((loaded['item12_vnum'], loaded['item17_vnum']), (0, 0))
		self.assertEqual((loaded['item12_val'], loaded['item17_val']), (30, 30))

	def test_a_file_of_colides_own_builds_starts_from_the_defaults(self):
		loaded = uiautohunt.ConfigFromText('range=4000\nitem3_vnum=27001\nconfig_version=3\n')
		self.assertEqual(loaded['range'], 2000)
		self.assertEqual(loaded['item3_vnum'], 0)
		self.assertEqual(loaded['config_version'], uiautohunt.CONFIG_VERSION)

	def test_target_vid(self):
		self.assertEqual(uiautohunt.ParseTargetVid('123'), 123)
		self.assertEqual(uiautohunt.ParseTargetVid('0'), 0)
		self.assertEqual(uiautohunt.ParseTargetVid('-5'), 0)
		self.assertEqual(uiautohunt.ParseTargetVid('wilk'), 0)
		self.assertEqual(uiautohunt.ParseTargetVid(None), 0)

	def test_loot_answer(self):
		self.assertEqual(uiautohunt.ParseLoot('7', '100', '200'), (7, 100, 200))
		self.assertEqual(uiautohunt.ParseLoot('0', '100', '200'), (0, 0, 0))
		self.assertEqual(uiautohunt.ParseLoot('7', 'x', '200'), (0, 0, 0))

	def test_loot_mask_follows_the_toggles(self):
		config = uiautohunt.DefaultConfig()
		self.assertEqual(uiautohunt.LootMask(config), 127)
		config['loot_weapon'] = 0
		config['loot_armour'] = 0
		self.assertEqual(uiautohunt.LootMask(config), 124)
		config['pickup'] = 0
		self.assertEqual(uiautohunt.LootMask(config), 0)

	def test_stop_point_and_facing(self):
		self.assertEqual(uiautohunt.StopPoint(0, 0, 1000, 0, 120), (880.0, 0.0))
		self.assertEqual(uiautohunt.StopPoint(0, 0, 100, 0, 120), (0, 0))
		for target in ((0, 100), (100, 0), (-100, 0), (0, -100), (70, 70)):
			degree = uiautohunt.FacingDegree(0, 0, target[0], target[1])
			self.assertTrue(0.0 <= degree <= 360.0, (target, degree))

	def test_config_paths_keep_names_to_letters(self):
		self.assertEqual(uiautohunt.ConfigPath('Ab c/1'), os.path.join('autohunt', 'postacie', 'Ab_c_1.cfg'))
		self.assertEqual(uiautohunt.OldConfigPath('Ab c/1'), os.path.join('autohunt', 'Ab_c_1.cfg'))
		self.assertEqual(uiautohunt.OldestConfigPath('Ab c/1'), 'autohunt_Ab_c_1.cfg')
		self.assertEqual(uiautohunt.GlobalConfigPath(), os.path.join('autohunt', 'config.cfg'))

	def test_mana_items_by_their_table_and_by_the_list(self):
		# USE_POTION restoring mana, USE_POTION restoring health, a mana
		# blessing (value4), a health blessing (value3), something else.
		STATE['protos'].update({
			27052: (3, 0, (0, 100, 0, 0, 0, 0)),
			27001: (3, 0, (300, 0, 0, 0, 0, 0)),
			39012: (3, 11, (0, 0, 0, 0, 100, 0)),
			39011: (3, 11, (0, 0, 0, 100, 0, 0)),
			70038: (3, 10, (60, 20, 0, 0, 0, 0)),
		})
		self.assertTrue(uiautohunt.IsManaItem(27052))
		self.assertFalse(uiautohunt.IsManaItem(27001))
		self.assertTrue(uiautohunt.IsManaItem(39012))
		self.assertFalse(uiautohunt.IsManaItem(39011))
		self.assertFalse(uiautohunt.IsManaItem(70038))
		# Colide's list answers without the table.
		self.assertTrue(uiautohunt.IsManaItem(50021))
		self.assertFalse(uiautohunt.IsManaItem(0))

	def test_the_file_of_2_0_24_is_read_until_the_first_save(self):
		import shutil
		import tempfile
		here = os.getcwd()
		folder = tempfile.mkdtemp()
		try:
			os.chdir(folder)
			os.makedirs('autohunt')
			with open(os.path.join('autohunt', 'Tester.cfg'), 'w') as handle:
				handle.write('range=3000\nskill7_slot=4\nitem9_vnum=70038\nconfig_version=4\n')
			with open('autohunt_Tester.cfg', 'w') as handle:
				handle.write('range=1000\nconfig_version=2\n')
			hunter = uiautohunt.Hunter()
			hunter.configName = 'Tester'
			hunter.LoadConfig()
			self.assertEqual((hunter.config['range'], hunter.config['skill7_slot'], hunter.config['item9_vnum']), (3000, 4, 70038))
			self.assertEqual((hunter.config['mobs'], hunter.config['stones'], hunter.config['bosses']), (1, 1, 0))
		finally:
			os.chdir(here)
			shutil.rmtree(folder, ignore_errors=True)

	def test_settings_go_to_their_folder_and_the_old_file_is_still_read(self):
		import shutil
		import tempfile
		here = os.getcwd()
		folder = tempfile.mkdtemp()
		try:
			os.chdir(folder)
			with open('autohunt_Tester.cfg', 'w') as handle:
				handle.write('range=3000\nhp_vnum=27001\nconfig_version=2\n')
			hunter = uiautohunt.Hunter()
			hunter.configName = 'Tester'
			hunter.LoadConfig()
			self.assertEqual((hunter.config['range'], hunter.config['item0_vnum']), (3000, 27001))
			hunter.config['range'] = 4000
			self.assertTrue(hunter.SaveConfig())
			self.assertTrue(os.path.exists(os.path.join('autohunt', 'postacie', 'Tester.cfg')))
			self.assertTrue(os.path.exists(os.path.join('autohunt', 'config.cfg')))
			again = uiautohunt.Hunter()
			again.configName = 'Tester'
			again.LoadConfig()
			self.assertEqual(again.config['range'], 4000)
		finally:
			os.chdir(here)
			shutil.rmtree(folder, ignore_errors=True)


class HuntTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		uiautohunt._manaItems.clear()
		self.hunter = uiautohunt.Hunter()
		self.hunter.Start()

	def test_asks_the_server_from_the_start_point(self):
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0'])
		self.assertEqual(commands('/autohunt_loot'), ['/autohunt_loot 2000 127 0 0'])
		step(self.hunter, 0.5)
		self.assertEqual(len(commands('/autohunt_target')), 1)
		step(self.hunter, 0.4)
		self.assertEqual(len(commands('/autohunt_target')), 2)
		self.assertEqual(len(commands('/autohunt_loot')), 1)
		step(self.hunter, 0.2)
		self.assertEqual(len(commands('/autohunt_loot')), 2)

	def test_no_loot_question_when_the_pick_up_is_off(self):
		self.hunter.config['pickup'] = 0
		step(self.hunter)
		self.assertEqual(commands('/autohunt_loot'), [])
		self.hunter.OnServerLoot('77', '50', '0')
		step(self.hunter, 1.0)
		self.assertEqual(STATE['picked'], [])

	def test_walks_to_the_target_then_swings(self):
		self.hunter.OnServerTarget('55')
		STATE['where'][55] = (1500, 1000, 0)
		STATE['distance'][55] = 500
		step(self.hunter)
		self.assertEqual(STATE['walks'], [(1380, 1000)])
		self.assertEqual(STATE['attack'], [])
		STATE['distance'][55] = 150
		step(self.hunter, 0.1)
		self.assertEqual(STATE['targets'], [55])
		self.assertEqual(STATE['attack'], [])
		step(self.hunter, 0.1)
		self.assertEqual(STATE['targets'], [55])
		self.assertEqual(STATE['attack'], [True])
		self.assertEqual(len(STATE['rotations']), 1)

	def test_skills_go_on_their_own_clock_fight_or_no_fight(self):
		self.hunter.config['skill0_slot'] = 1
		STATE['skills'][1] = 3
		step(self.hunter)
		self.assertEqual(STATE['cast'], [1])
		step(self.hunter, 1.0)
		self.assertEqual(STATE['cast'], [1])
		step(self.hunter, 0.6)
		self.assertEqual(STATE['cast'], [1, 1])

	def test_casts_the_twelfth_skill_slot(self):
		self.hunter.config['skill11_slot'] = 9
		STATE['skills'][9] = 4
		step(self.hunter)
		self.assertEqual(STATE['cast'], [9])

	def test_no_skills_when_they_are_switched_off(self):
		self.hunter.config['use_skills'] = 0
		self.hunter.config['skill0_slot'] = 1
		STATE['skills'][1] = 3
		step(self.hunter)
		self.assertEqual(STATE['cast'], [])

	def test_with_the_attack_off_no_target_is_asked_for_and_skills_still_go(self):
		self.hunter.config['attack'] = 0
		self.hunter.config['skill0_slot'] = 1
		STATE['skills'][1] = 3
		self.hunter.OnServerTarget('55')
		STATE['distance'][55] = 100
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), [])
		self.assertEqual(self.hunter.targetVid, 0)
		self.assertEqual(STATE['attack'], [])
		self.assertEqual(STATE['cast'], [1])

	def test_a_new_target_in_reach_releases_the_attack_key(self):
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])
		# "Nothing" from the server keeps the one in hand: the server takes
		# a corpse away two or three seconds after it fell.
		self.hunter.OnServerTarget('0')
		self.assertEqual(self.hunter.targetVid, 55)
		self.assertEqual(STATE['attack'], [True])
		self.hunter.OnServerTarget('56')
		self.assertEqual(STATE['attack'], [True, False])
		self.assertEqual(self.hunter.targetVid, 56)

	def test_keeps_walking_to_its_target_when_another_is_named(self):
		# Switching on every answer is what made the hunter turn back and
		# forth between two monsters (Colide, 22 September).
		STATE['where'][55] = (2000, 1000, 0)
		STATE['distance'][55] = 900
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		self.hunter.OnServerTarget('56')
		self.assertEqual(self.hunter.targetVid, 55)
		STATE['distance'][55] = -1
		self.hunter.OnServerTarget('56')
		self.assertEqual(self.hunter.targetVid, 56)

	def test_a_clearly_nearer_monster_ends_a_chase(self):
		# "bila najblizszy target, a nie lapala focus na jednego moba i
		# gonila go" (Buby, 23 September): chasing 55 at 900, the server's 56
		# at 150 is taken, a 57 at 700 is not nearer by the margin.
		STATE['where'][55] = (2000, 1000, 0)
		STATE['distance'][55] = 900
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		STATE['distance'][57] = 700
		self.hunter.OnServerTarget('57')
		self.assertEqual(self.hunter.targetVid, 55)
		STATE['distance'][56] = 150
		self.hunter.OnServerTarget('56')
		self.assertEqual(self.hunter.targetVid, 56)

	def test_walks_to_loot_and_picks_it_up(self):
		self.hunter.OnServerLoot('77', '600', '0')
		step(self.hunter)
		self.assertEqual(STATE['walks'][-1], (1600, 1000))
		self.assertEqual(STATE['picked'], [])
		STATE['pos'] = (1500, 1000)
		step(self.hunter, 0.4)
		self.assertEqual(STATE['picked'], [77])
		self.assertEqual(self.hunter.lootVid, 0)

	def test_takes_loot_at_its_feet_during_a_fight(self):
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		self.hunter.OnServerTarget('55')
		self.hunter.OnServerLoot('77', '50', '0')
		step(self.hunter)
		self.assertEqual(STATE['picked'], [77])
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])

	def test_leaves_loot_it_cannot_reach_alone_for_a_while(self):
		self.hunter.OnServerLoot('77', '2000', '0')
		step(self.hunter)
		step(self.hunter, 6.5)
		self.assertEqual(self.hunter.lootVid, 0)
		self.hunter.OnServerLoot('77', '2000', '0')
		self.assertEqual(self.hunter.lootVid, 0)
		asked = len(commands('/autohunt_loot'))
		step(self.hunter, 2.0)
		self.assertEqual(len(commands('/autohunt_loot')), asked)

	def test_uses_an_item_on_its_clock(self):
		self.hunter.config['item8_vnum'] = 70038
		self.hunter.config['item8_val'] = 5
		STATE['bag'][3] = 70038
		step(self.hunter)
		self.assertEqual(STATE['used'], [3])
		step(self.hunter, 4.0)
		self.assertEqual(STATE['used'], [3])
		step(self.hunter, 1.5)
		self.assertEqual(STATE['used'], [3, 3])

	def test_uses_an_item_of_the_second_clock_row(self):
		self.hunter.config['item17_vnum'] = 71016
		self.hunter.config['item17_val'] = 3
		STATE['bag'][9] = 71016
		step(self.hunter)
		self.assertEqual(STATE['used'], [9])
		step(self.hunter, 3.1)
		self.assertEqual(STATE['used'], [9, 9])

	def test_one_item_a_pass_and_the_rest_a_moment_later(self):
		# The engine takes one item use at a time and refuses the rest, so a
		# frame that sent four ended with one working (Colide, 20 September).
		self.hunter.config['item6_vnum'] = 70038
		self.hunter.config['item6_val'] = 30
		self.hunter.config['item7_vnum'] = 71016
		self.hunter.config['item7_val'] = 30
		self.hunter.config['item8_vnum'] = 27001
		self.hunter.config['item8_val'] = 30
		STATE['bag'][3] = 70038
		STATE['bag'][4] = 71016
		STATE['bag'][5] = 27001
		step(self.hunter)
		self.assertEqual(STATE['used'], [3])
		# Inside the shared interval nothing else goes, however ripe its clock.
		step(self.hunter, 0.05)
		self.assertEqual(STATE['used'], [3])
		step(self.hunter, 0.2)
		self.assertEqual(STATE['used'], [3, 4])
		step(self.hunter, 0.2)
		self.assertEqual(STATE['used'], [3, 4, 5])

	def test_a_missing_item_does_not_hold_the_others_back(self):
		# The slot's own clock is set before the bag is searched, so an item
		# nobody carries is looked for once an interval and never blocks the
		# pass - it is not a use, so it does not spend the shared interval.
		self.hunter.config['item6_vnum'] = 70038
		self.hunter.config['item6_val'] = 30
		self.hunter.config['item7_vnum'] = 71016
		self.hunter.config['item7_val'] = 30
		STATE['bag'][4] = 71016
		step(self.hunter)
		self.assertEqual(STATE['used'], [4])

	def test_no_items_on_a_clock_when_they_are_switched_off(self):
		self.hunter.config['use_buffs'] = 0
		self.hunter.config['item6_vnum'] = 70038
		STATE['bag'][3] = 70038
		step(self.hunter)
		self.assertEqual(STATE['used'], [])

	def test_drinks_below_the_share_and_not_twice_a_second(self):
		self.hunter.config['item0_vnum'] = 27001
		STATE['bag'][5] = 27001
		STATE['status'][1] = 30
		step(self.hunter)
		self.assertEqual(STATE['used'], [5])
		step(self.hunter, 0.5)
		self.assertEqual(STATE['used'], [5])
		step(self.hunter, 0.6)
		self.assertEqual(STATE['used'], [5, 5])

	def test_a_mana_potion_in_the_first_slot_watches_mana(self):
		# The window before Colide's drank the first slot for health whatever
		# lay in it.
		STATE['protos'][27004] = (3, 0, (0, 100, 0, 0, 0, 0))
		self.hunter.config['item0_vnum'] = 27004
		STATE['bag'][6] = 27004
		STATE['status'][1] = 20
		step(self.hunter)
		self.assertEqual(STATE['used'], [])
		STATE['status'][3] = 30
		step(self.hunter, 1.1)
		self.assertEqual(STATE['used'], [6])

	def test_no_potions_when_they_are_switched_off(self):
		self.hunter.config['use_potions'] = 0
		self.hunter.config['item0_vnum'] = 27001
		STATE['bag'][5] = 27001
		STATE['status'][1] = 30
		step(self.hunter)
		self.assertEqual(STATE['used'], [])

	def test_a_missing_potion_is_looked_for_once_a_second(self):
		self.hunter.config['item0_vnum'] = 27001
		STATE['status'][1] = 30
		step(self.hunter)
		reads = STATE['bag_reads']
		self.assertTrue(reads > 0)
		for _ in range(10):
			step(self.hunter, 0.05)
		self.assertEqual(STATE['bag_reads'], reads)
		step(self.hunter, 0.6)
		self.assertTrue(STATE['bag_reads'] > reads)

	def test_stands_up_after_the_wait(self):
		STATE['status'][1] = 0
		step(self.hunter)
		step(self.hunter, 14.0)
		self.assertNotIn('/restart_here', STATE['commands'])
		step(self.hunter, 1.5)
		self.assertIn('/restart_here', STATE['commands'])

	def test_after_standing_up_it_drinks_and_waits_for_its_share(self):
		self.hunter.config['item0_vnum'] = 27001
		STATE['bag'][5] = 27001
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		STATE['status'][1] = 0
		step(self.hunter)
		STATE['status'][1] = 20
		self.hunter.OnServerTarget('55')
		del STATE['commands'][:]
		step(self.hunter, 16.0)
		self.assertTrue(self.hunter.justRevived)
		self.assertEqual(STATE['used'], [5])
		self.assertEqual(STATE['attack'], [])
		self.assertEqual(commands('/autohunt_target'), [])
		STATE['status'][1] = 60
		step(self.hunter, 1.0)
		self.assertFalse(self.hunter.justRevived)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])

	def test_a_share_over_a_hundred_waits_for_full_health_only(self):
		self.hunter.config['revive_hp_percent'] = 250
		self.hunter.justRevived = True
		STATE['status'][1] = 100
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		self.assertFalse(self.hunter.justRevived)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])

	def test_gives_up_on_a_target_it_cannot_reach(self):
		STATE['where'][55] = (2000, 1000, 0)
		STATE['distance'][55] = 900
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		step(self.hunter, 8.5)
		self.assertEqual(self.hunter.targetVid, 0)
		self.assertEqual(STATE['walks'][-1], (1000, 1000))

	def test_names_the_target_it_gave_up_on_for_a_minute(self):
		self.hunter.config['stones'] = 1
		STATE['where'][55] = (2000, 1000, 0)
		STATE['distance'][55] = 900
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0'])
		step(self.hunter, 8.5)
		self.assertEqual(self.hunter.targetVid, 0)
		del STATE['commands'][:]
		step(self.hunter, 2.5)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0 55'])
		del STATE['commands'][:]
		step(self.hunter, 60.0)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0'])

	def test_a_bow_in_the_hand_shoots_from_afar(self):
		with_new_exe(self)
		STATE['bow'] = True
		STATE['where'][55] = (2700, 1000, 0)
		STATE['distance'][55] = 1700
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])
		self.assertEqual(STATE['walks'], [])

	def test_an_archer_with_daggers_in_the_hand_walks_in(self):
		with_new_exe(self)
		STATE['race'] = 5
		STATE['group'] = 2
		STATE['where'][55] = (1700, 1000, 0)
		STATE['distance'][55] = 700
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		self.assertEqual(STATE['attack'], [])
		self.assertEqual(STATE['walks'], [(1580, 1000)])

	def test_an_older_exe_takes_the_archery_school_for_a_bow(self):
		STATE['race'] = 5
		STATE['group'] = 2
		STATE['where'][55] = (1700, 1000, 0)
		STATE['distance'][55] = 700
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])
		self.assertEqual(STATE['walks'], [])

	def test_a_corpse_is_let_go_and_skipped(self):
		with_new_exe(self)
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		self.hunter.OnServerTarget('55')
		step(self.hunter)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])
		STATE['dead'].add(55)
		del STATE['commands'][:]
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True, False])
		self.assertEqual(STATE['target'], 0)
		self.assertEqual(self.hunter.targetVid, 0)
		del STATE['commands'][:]
		step(self.hunter, 0.1)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0 55'])
		del STATE['commands'][:]
		step(self.hunter, 5.5)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0'])

	def test_the_switches_for_what_is_fought(self):
		self.hunter.config['mobs'] = 0
		self.hunter.config['stones'] = 0
		self.hunter.config['bosses'] = 1
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 0 0 0 0 1'])

	def test_walks_back_when_idle_far_from_the_start(self):
		STATE['pos'] = (2000, 1000)
		step(self.hunter)
		self.assertEqual(STATE['walks'], [(1000, 1000)])

	def test_fetches_drops_before_a_far_target(self):
		STATE['where'][55] = (1500, 1000, 0)
		STATE['distance'][55] = 500
		self.hunter.OnServerTarget('55')
		self.hunter.OnServerLoot('77', '700', '0')
		step(self.hunter)
		self.assertEqual(STATE['walks'][-1], (1700, 1000))
		self.assertEqual(STATE['attack'], [])

	def test_a_fight_in_reach_comes_before_drops(self):
		STATE['where'][55] = (1100, 1000, 0)
		STATE['distance'][55] = 100
		self.hunter.OnServerTarget('55')
		self.hunter.OnServerLoot('77', '700', '0')
		step(self.hunter)
		step(self.hunter, 0.1)
		self.assertEqual(STATE['attack'], [True])
		self.assertEqual(STATE['walks'], [])

	def test_picks_up_from_four_hundred_and_fifty(self):
		self.hunter.OnServerLoot('77', '440', '0')
		step(self.hunter)
		self.assertEqual(STATE['picked'], [77])

	def test_the_start_point_goes_as_an_offset(self):
		STATE['pos'] = (1500, 800)
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 -500 200 1 0'])
		self.assertEqual(commands('/autohunt_loot'), ['/autohunt_loot 2000 127 -500 200'])

	def test_without_the_walk_back_the_range_goes_with_the_character(self):
		self.hunter.config['return'] = 0
		STATE['pos'] = (1500, 800)
		step(self.hunter)
		self.assertEqual(commands('/autohunt_target'), ['/autohunt_target 2000 1 0 0 1 0'])
		self.assertEqual(commands('/autohunt_loot'), ['/autohunt_loot 2000 127 0 0'])

	def test_the_loot_answer_is_an_offset_from_the_character(self):
		STATE['pos'] = (5000, 7000)
		self.hunter.OnServerLoot('77', '-300', '400')
		self.assertEqual(self.hunter.lootPos, (4700, 7400))

	def test_destroy_stops_without_a_word(self):
		messages = len(STATE['chat'])
		self.hunter.Destroy()
		self.assertFalse(self.hunter.running)
		self.assertEqual(len(STATE['chat']), messages)


class WindowTest(unittest.TestCase):
	# The windows save their places (autohunt/config.cfg) when they open and
	# close, so every test here runs in a folder of its own.
	def setUp(self):
		import shutil
		import tempfile
		reset_state()
		uiautohunt._manaItems.clear()
		self.here = os.getcwd()
		self.folder = tempfile.mkdtemp()
		os.chdir(self.folder)
		self.addCleanup(shutil.rmtree, self.folder, True)
		self.addCleanup(os.chdir, self.here)

	def test_k_opens_both_windows_side_by_side_and_closes_them(self):
		hunter = uiautohunt.Hunter()
		hunter.ToggleWindow()
		main, loot = hunter.mainWindow, hunter.lootWindow
		self.assertTrue(main.IsShow() and loot.IsShow())
		(mx, my), (lx, ly) = main.position, loot.position
		self.assertEqual(lx - mx, main.WIDTH + 10)
		self.assertEqual(my, ly)
		self.assertTrue(mx + main.WIDTH + 10 + loot.WIDTH <= 800)
		self.assertTrue(my + main.HEIGHT <= 600)
		loot.Close()
		hunter.ToggleWindow()
		self.assertFalse(main.IsShow() or loot.IsShow())
		hunter.ToggleWindow()
		self.assertTrue(main.IsShow() and loot.IsShow())

	def test_the_loot_window_switches_and_reaches(self):
		hunter = uiautohunt.Hunter()
		hunter.ToggleWindow()
		loot = hunter.lootWindow
		loot.OnToggle('loot_weapon')
		self.assertEqual(hunter.config['loot_weapon'], 0)
		self.assertEqual(loot.toggles['loot_weapon'][0].text, 'Bro\xf1: nie')
		loot.OnToggle('bosses')
		self.assertEqual(hunter.config['bosses'], 1)
		self.assertEqual(loot.toggles['bosses'][0].text, 'Bossy: tak')
		loot.rangeSlider.SetSliderPos(0.5)
		loot.OnChangeRange()
		self.assertEqual(hunter.config['range'], 2650)
		self.assertEqual(loot.rangeText.text, 'Zasi\xeag: 2650')

	def test_the_range_is_drawn_while_the_settings_are_open(self):
		with_new_exe(self)
		hunter = uiautohunt.Hunter()
		hunter.ToggleWindow()
		hunter.lootWindow.rangeSlider.SetSliderPos(0.5)
		hunter.lootWindow.OnChangeRange()
		self.assertEqual(STATE['circles'][-1], (2650, 0.0, 0.0, 0))
		hunter.Start()
		step(hunter)
		self.assertEqual(STATE['circles'][-1], (2650, 1000.0, 1000.0, 1))
		hunter.lootWindow.Close()
		self.assertEqual(STATE['circles'][-1], (0,))

	def test_the_windows_come_back_where_they_were_left(self):
		import shutil
		import tempfile
		here = os.getcwd()
		folder = tempfile.mkdtemp()
		try:
			os.chdir(folder)
			hunter = uiautohunt.Hunter()
			hunter.ToggleWindow()
			hunter.mainWindow.SetPosition(20, 30)
			hunter.lootWindow.SetPosition(400, 40)
			hunter.SaveGlobalConfig()
			again = uiautohunt.Hunter()
			again.ToggleWindow()
			self.assertEqual(again.mainWindow.position, (20, 30))
			self.assertEqual(again.lootWindow.position, (400, 40))
		finally:
			os.chdir(here)
			shutil.rmtree(folder, ignore_errors=True)

	def test_the_fight_window_has_a_slot_and_a_field_for_every_item(self):
		hunter = uiautohunt.Hunter()
		hunter.ToggleWindow()
		main = hunter.mainWindow
		for key in uiautohunt.ITEM_EDIT_KEYS:
			self.assertIn(key, main.edits)
		main.edits['item17_val'].SetText('45')
		main.ReadEdits()
		self.assertEqual(hunter.config['item17_val'], 45)

	def test_destroy_takes_both_windows(self):
		hunter = uiautohunt.Hunter()
		hunter.ToggleWindow()
		hunter.Destroy()
		self.assertIsNone(hunter.mainWindow)
		self.assertIsNone(hunter.lootWindow)


if __name__ == '__main__':
	unittest.main()
