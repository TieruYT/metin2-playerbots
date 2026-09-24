# Auto Hunt System
# Created by SIZOWSKI (Thank you for the original code! Go subscribe to him on YouTube! https://www.youtube.com/@metin2singleplayer a.k.a "ZAXEP - METIN2 SINGLE PLAYER")
# Modernized by Colide
#
# Auto Lowy 2.0 (Colide, 22 September). The official system's features for
# free (pl-wiki, "System - Auto Lowy"): twelve skills on their own clocks
# (one cast at an enemy waits for the fight - see SELF_SKILLS below),
# six potions each under its own share of health or mana, twelve items on a
# clock, stones, standing up after death and walking back, and a pick-up
# by kind in a window of its own. New in 2.0: three switches for what is
# fought - Moby, Metiny, Bossy - which the server ranks boss, stone,
# monster (do_autohunt_target, playerbotify apply_auto_hunt_categories);
# a corpse is let go the moment this client sees it dead
# (player.IsTargetDead), because the server keeps it two or three seconds
# and named it again; a bow in the hand reaches from afar
# (player.IsBowEquipped); the range is drawn on the ground
# (player.SetAutoHuntRangeCircle) - three functions of the client 2.0.25
# exe, each asked with hasattr or under AttributeError, so an older exe
# hunts without them; the windows keep their places in autohunt/config.cfg
# and every character its own settings in autohunt/postacie/<name>.cfg.
#
# The client cannot list the monsters or the items round its character, so
# it asks the server:
#   "/autohunt_target <range> <stones> <dx> <dy> <mobs> <bosses> [<skip>]"
#     -> "AutoHuntTarget <vid>"
#   "/autohunt_loot <range> <kinds> <dx> <dy>" -> "AutoHuntLoot <vid> <dx> <dy>"
# Every place goes both ways as an offset from the character: this client
# counts positions from its map's corner and the server from the world's.
# Every time is clientclock.Now(): app.GetTime() starts again from zero at
# every warp, and a skill's or an item's next moment taken on the map
# before would have held it for as long as the character played there.
#
# game.py registers the Hunter with its updateables, K opens the windows.
# Python 2.7 as the client has it, and 3 for tests/uiautohunt_test.py.
# Player-visible strings are CP1250 escapes, so the file itself is ASCII.

import app
import chat
import chr
import clientclock
import item
import math
import mouseModule
import net
import os
import player
import skill
import ui
import wndMgr

try:
    xrange
except NameError:
    xrange = range

SKILL_SLOTS = 12
USE_ITEM_SLOTS = 18
ITEM_SLOT_KEYS = tuple('item%d_vnum' % i for i in xrange(USE_ITEM_SLOTS))
ITEM_EDIT_KEYS = tuple('item%d_val' % i for i in xrange(USE_ITEM_SLOTS))

RANGES = (1000, 2000, 3000, 4000)
LOOT_KINDS = (
    ('loot_weapon',    'Bro\xf1',    1 << 0),
    ('loot_armour',    'Zbroje',       1 << 1),
    ('loot_jewellery', 'Ozdoby',       1 << 2),
    ('loot_potion',    'Mikstury',     1 << 3),
    ('loot_book',      'Ksi\xeagi', 1 << 4),
    ('loot_stone',     'Kamienie',     1 << 5),
    ('loot_other',     'Inne',         1 << 6),
)

TARGET_REQUEST_INTERVAL = 0.8
LOOT_REQUEST_INTERVAL = 1.0
MOVE_INTERVAL = 0.35
RETURN_MOVE_INTERVAL = 1.0
FACE_INTERVAL = 0.5
POTION_INTERVAL = 1.0
LOOT_PICK_DISTANCE = 450
LOOT_FIRST_DISTANCE = 900
LOOT_PICK_INTERVAL = 0.6
LOOT_STUCK_SECONDS = 6.0
LOOT_STUCK_PAUSE = 10.0
REVIVE_RETRY = 5.0
REVIVE_MIN_SECONDS = 10
SKILL_MIN_INTERVAL = 1.5
ITEM_MIN_INTERVAL = 1
STATUS_INTERVAL = 0.3
MELEE_REACH = 200
ARCHER_REACH = 2400
STOP_SHORT_SHARE = 0.6
ANCHOR_LEASH = 600
STUCK_SECONDS = 8.0
# While the hunter walks after a target beyond its reach, the server's pick
# is taken only when it stands this much nearer: the nearest monster is hit
# rather than one chased across the map, and two monsters at about the same
# distance do not turn the hunter back and forth (Buby, 23 September).
CHASE_SWITCH_MARGIN = 300
STUCK_PAUSE = 2.0
STUCK_SKIP_SECONDS = 60.0

# A skill cast at an enemy goes only at the monster the hunter is fighting:
# alive, in the client's own hand (player.GetTargetVID) and within reach.
# Anything less the client settles by itself, and badly for a character
# nobody steers. With no live target __UseSkill takes whatever stands under
# the mouse cursor (PythonPlayerSkill.cpp, __ChangeTargetToPickedInstance);
# out of the skill's range it walks there in a straight line, fighting the
# hunter's own walk (__ReserveUseSkill, MODE_USE_SKILL); and Fast Attack then
# puts the character 270 units before that monster's middle, along the
# straight line in three dimensions, with one IsBlock test at the landing and
# no look at what lies between (ActorInstanceMotionEvent.cpp,
# ProcessMotionEventWarp) - over a cliff, through a wall, into a hillside.
# Clicked on its clock from anywhere, that is the likeliest way Buby's Ninja
# came to hang in a night sky with no world round it (23 September).
# What needs no enemy still goes on its own clock, fight or no fight: a
# standing skill and a toggle, which the client casts where the character
# stands, and SELF_SKILLS - the Ninja's Stealth and the Shaman's buffs,
# which the client turns on the caster when the target is a monster.
SELF_SKILLS = (34, 94, 95, 96, 109, 110, 111)
# What skill.IsStandingSkill says of this client's own skills, for an exe or
# a stub that cannot be asked.
STANDING_SKILLS = (3, 4, 18, 19, 47, 49, 62, 63, 64, 65, 77, 78, 79, 93)
# Only a class's skills and the horse's are ever cast at an enemy; a guild
# or a support skill is nobody's.
TARGET_SKILL_RANGES = ((1, 111), (137, 140))
# Fast Attack moves the character by its distance to the target less 270.
# From melee reach that is a short step back onto the ground it has just
# walked over; from the skill's own range of 800 it was a jump of 530 over
# whatever stood between. So it waits for melee reach even when the hunter
# reaches further - an exe older than 2.0.25 takes an archery-school Ninja
# for a bow in the hand whatever it holds.
WARP_SKILLS = (32,)

CONFIG_BASE_DIR = 'autohunt'
CONFIG_CHAR_DIR = os.path.join(CONFIG_BASE_DIR, 'postacie')

DEFAULTS = [
    ('range', 2000), ('stones', 1), ('mobs', 1), ('bosses', 0), ('pickup', 1), 
    ('revive', 1), ('revive_after', 15), ('return', 1),
    ('attack', 1), ('use_potions', 1), ('use_buffs', 1), ('use_skills', 1),
    ('revive_hp_percent', 60),
]
for i in xrange(USE_ITEM_SLOTS):
    DEFAULTS.append(('item%d_vnum' % i, 0))
    DEFAULTS.append(('item%d_val' % i, 60 if i < 6 else 30))
for _index in xrange(SKILL_SLOTS):
    DEFAULTS.append(('skill%d_slot' % _index, 0))
    DEFAULTS.append(('skill%d_interval' % _index, 0))
for _key, _label, _bit in LOOT_KINDS:
    DEFAULTS.append((_key, 1))

CONFIG_VERSION = 5
DEFAULTS.append(('config_version', CONFIG_VERSION))

GLOBAL_DEFAULTS = [
    ('win_main_x', -1), ('win_main_y', -1),
    ('win_loot_x', -1), ('win_loot_y', -1),
]

def DefaultConfig():
    return dict(DEFAULTS)

def ApplyConfigText(config, text):
    for line in text.splitlines():
        key, _, value = line.partition('=')
        key = key.strip()
        if key not in config:
            continue
        try:
            config[key] = max(0, int(value.strip()))
        except ValueError:
            pass
    return config

def ParseConfigText(text):
    """The "key=value" lines of a saved file as numbers, a bad line skipped."""
    values = {}
    for line in text.splitlines():
        key, _, value = line.partition('=')
        key = key.strip()
        if not key:
            continue
        try:
            values[key] = max(0, int(value.strip()))
        except ValueError:
            pass
    return values


def ConfigFromOldValues(values):
    """A file from the window before Colide's (no version, or 2) in this
    one's slots: the six skills where they were, the health and mana potions
    as the first two potion slots with their shares, the three items as the
    first three on a clock, and the pick-up back on (the toggle window saved
    it switched off by mistake)."""
    config = DefaultConfig()
    for key in ('range', 'stones', 'pickup', 'revive', 'revive_after', 'return'):
        if key in values:
            config[key] = values[key]
    for index in xrange(6):
        for key in ('skill%d_slot' % index, 'skill%d_interval' % index):
            if key in values:
                config[key] = values[key]
    for slot, (vnumKey, shareKey) in enumerate((('hp_vnum', 'hp_percent'), ('sp_vnum', 'sp_percent'))):
        config['item%d_vnum' % slot] = values.get(vnumKey, 0)
        if shareKey in values:
            config['item%d_val' % slot] = values[shareKey]
    for index in xrange(3):
        target = 6 + index
        config['item%d_vnum' % target] = values.get('item%d_vnum' % index, 0)
        if 'item%d_interval' % index in values:
            config['item%d_val' % target] = values['item%d_interval' % index]
    for key, label, bit in LOOT_KINDS:
        if key in values:
            config[key] = values[key]
    if 'config_version' not in values:
        config['pickup'] = 1
        for key, label, bit in LOOT_KINDS:
            config[key] = 1
    return config


def ConfigFromText(text):
    """Version 4 (2.0.19-2.0.24) and 5 (2.0 with Moby and Bossy) read as they
    are, the new keys taking their defaults; a file of the window before
    Colide's moves into these slots; version 3, his own first builds, starts
    from the defaults."""
    values = ParseConfigText(text)
    version = values.get('config_version', 0)
    if version >= 4:
        config = ApplyConfigText(DefaultConfig(), text)
    elif version <= 2:
        config = ConfigFromOldValues(values)
    else:
        config = DefaultConfig()
    config['config_version'] = CONFIG_VERSION
    return config

def ConfigText(config):
    return ''.join('%s=%d\n' % (key, config[key]) for key, _ in DEFAULTS)

def GlobalConfigPath():
    return os.path.join(CONFIG_BASE_DIR, 'config.cfg')

def ConfigPath(name):
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return os.path.join(CONFIG_CHAR_DIR, '%s.cfg' % safe)

def OldConfigPath(name):
    """Where 2.0.17-2.0.24 kept a character's settings."""
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return os.path.join(CONFIG_BASE_DIR, '%s.cfg' % safe)


def OldestConfigPath(name):
    """Where the window before Colide's kept them, beside the client."""
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return 'autohunt_%s.cfg' % safe

def ParseTargetVid(value):
    try:
        vid = int(value)
    except (TypeError, ValueError):
        return 0
    return vid if 0 < vid <= 0xffffffff else 0

def ParseLoot(vid, x, y):
    vid = ParseTargetVid(vid)
    if not vid:
        return (0, 0, 0)
    try:
        return (vid, int(x), int(y))
    except (TypeError, ValueError):
        return (0, 0, 0)

def LootMask(config):
    if not config.get('pickup'):
        return 0
    mask = 0
    for key, label, bit in LOOT_KINDS:
        if config.get(key):
            mask |= bit
    return mask

def FacingDegree(fromX, fromY, toX, toY):
    dx = toX - fromX
    dy = toY - fromY
    distance = math.sqrt(dx * dx + dy * dy)
    if distance <= 0:
        return 0.0
    degree = 180.0 * math.acos(max(-1.0, min(1.0, dy / distance))) / math.pi + 180.0
    if fromX >= toX:
        degree = 360.0 - degree
    return degree

def StopPoint(fromX, fromY, toX, toY, short):
    dx = fromX - toX
    dy = fromY - toY
    distance = math.sqrt(dx * dx + dy * dy)
    if distance <= short or distance <= 0:
        return (fromX, fromY)
    return (toX + dx * short / distance, toY + dy * short / distance)

def FindInventoryCell(vnum):
    if not vnum:
        return -1
    for cell in xrange(player.INVENTORY_MAX_NUM):
        if player.GetItemIndex(cell) == vnum:
            return cell
    return -1

MANA_ITEM_VNUMS = (
    27004, 27005, 27006, 27008,
    50815,
    27864, 27867, 27876,
    39012, 71019,
    50021,
)

_manaItems = {}


def IsManaItem(vnum):
    """Whether a potion slot holding ``vnum`` watches mana rather than health:
    a potion whose table restores mana and no health (value1 and value0 of
    USE_POTION and USE_POTION_NODELAY - the blue potions, the fish, the
    sushi, 27052), a blessing that restores a share of mana and none of
    health (value4 and value3), or one of MANA_ITEM_VNUMS. Read once per
    vnum."""
    if vnum in _manaItems:
        return _manaItems[vnum]
    mana = vnum in MANA_ITEM_VNUMS
    if not mana and vnum:
        try:
            item.SelectItem(vnum)
            if item.GetItemType() == item.ITEM_TYPE_USE and item.GetItemSubType() in (item.USE_POTION, item.USE_POTION_NODELAY):
                mana = ((item.GetValue(1) > 0 and item.GetValue(0) <= 0) or
                        (item.GetValue(4) > 0 and item.GetValue(3) <= 0))
        except Exception:
            mana = False
    _manaItems[vnum] = mana
    return mana

def NeedsTarget(skillIndex):
    """Whether a skill is cast at an enemy: a skill of a class or of the horse
    that is neither standing nor a toggle nor one of SELF_SKILLS. The client
    says what is standing and what is a toggle (skill.IsStandingSkill and
    skill.IsToggleSkill, both raising for a skill they do not know);
    STANDING_SKILLS answers where it cannot."""
    inRange = False
    for low, high in TARGET_SKILL_RANGES:
        if low <= skillIndex <= high:
            inRange = True
    if not inRange or skillIndex in SELF_SKILLS or skillIndex in STANDING_SKILLS:
        return False
    for name in ('IsToggleSkill', 'IsStandingSkill'):
        ask = getattr(skill, name, None)
        if ask is None:
            continue
        try:
            if ask(skillIndex):
                return False
        except Exception:
            pass
    return True

def YesNo(value):
    return 'tak' if value else 'nie'

def WlWyl(value):
    return 'W\xa3' if value else 'WY\xa3'


class Hunter(object):
    def __init__(self):
        self.config = DefaultConfig()
        self.config_global = dict(GLOBAL_DEFAULTS)
        self.configName = None
        self.running = False
        self.mainWindow = None
        self.lootWindow = None
        self.isLoaded = False
        self.LoadGlobalConfig()
        self.ResetState()

    def ResetState(self):
        self.targetVid = 0
        self.skipVid = 0
        self.skipUntil = 0.0
        self.attacking = False
        self.anchor = (0, 0)
        self.nextRequest = 0.0
        self.nextMove = 0.0
        self.nextFace = 0.0
        self.nextPotion = 0.0
        self.nextRevive = 0.0
        self.deadSince = 0.0
        self.justRevived = False
        self.approachSince = 0.0
        self.skillNext = [0.0] * SKILL_SLOTS
        self.itemNext = [0.0] * USE_ITEM_SLOTS
        self.lootVid = 0
        self.lootPos = (0, 0)
        self.lootSince = 0.0
        self.lootPausedUntil = 0.0
        self.nextLootRequest = 0.0
        self.nextLootPick = 0.0
        self.nextBuffGlobal = 0.0

    def CanUpdate(self):
        return self.running

    def OnUpdate(self):
        now = clientclock.Now()

        if player.GetStatus(player.HP) <= 0:
            self.WhileDead(now)
            return
            
        if self.deadSince > 0.0:
            self.justRevived = True
            self.deadSince = 0.0

        if self.justRevived:
            maxHP = player.GetStatus(player.MAX_HP)
            curHP = player.GetStatus(player.HP)
            threshold = min(100, self.config.get('revive_hp_percent', 60))
            if maxHP > 0 and curHP * 100 < maxHP * threshold:
                self.HandleItems(now)
                self.CastSkills(now)
                return
            self.justRevived = False
            
        self.HandleItems(now)
        self.CastSkills(now)
        self.AskForLoot(now)
        self.Chase(now)

        if self.lootWindow and self.lootWindow.IsShow():
            try:
                base_range = self.config.get('range', 2000)
                if self.running and self.config.get('return', 0):
                    (ax, ay) = self.anchor
                    player.SetAutoHuntRangeCircle(base_range, float(ax), float(ay), 1)
                else:
                    player.SetAutoHuntRangeCircle(base_range, 0.0, 0.0, 0)
            except AttributeError:
                pass

    def Destroy(self):
        self.Stop(quiet=True)
        if self.mainWindow:
            self.mainWindow.Hide()
            self.mainWindow.Destroy()
            self.mainWindow = None
            
        if self.lootWindow:
            self.lootWindow.Hide()
            self.lootWindow.Destroy()
            self.lootWindow = None
            
        self.isLoaded = False

    def Start(self):
        if self.running:
            return
        self.ResetState()
        (x, y, z) = player.GetMainCharacterPosition()
        self.anchor = (int(x), int(y))
        self.running = True
        chat.AppendChat(chat.CHAT_TYPE_INFO, 'Auto \xa3owy: start, zasi\xeag %d.' % self.config['range'])
        if not LootMask(self.config):
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Auto \xa3owy: podnoszenie jest wy\xb3\xb9czone.')

    def Stop(self, quiet=False):
        if not self.running:
            return
        self.running = False
        self.ReleaseAttack()
        self.targetVid = 0
        self.lootVid = 0
        if not quiet:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Auto \xa3owy: stop.')

    def OnServerTarget(self, value):
        if not self.running or not self.config.get('attack', 1):
            return
            
        new_vid = ParseTargetVid(value)
        if not new_vid:
            return

        if self.targetVid != 0 and new_vid != self.targetVid:
            distance = player.GetCharacterDistance(self.targetVid)
            if distance >= 0:
                reach_limit = self.Reach() + 200 
                if distance > reach_limit:
                    # Chasing: a clearly nearer monster is hit instead.
                    new_distance = player.GetCharacterDistance(new_vid)
                    if new_distance < 0 or new_distance + CHASE_SWITCH_MARGIN > distance:
                        return

        if new_vid != self.targetVid:
            self.ReleaseAttack()
            self.targetVid = new_vid
            self.approachSince = 0.0
            self.nextMove = 0.0

    def OnServerLoot(self, vid, x, y):
        if not self.running or clientclock.Now() < self.lootPausedUntil or not LootMask(self.config):
            return
        (new_vid, dx, dy) = ParseLoot(vid, x, y)
        
        if new_vid != self.lootVid:
            self.lootSince = 0.0
            self.nextMove = 0.0
            
        self.lootVid = new_vid
        (px, py, pz) = player.GetMainCharacterPosition()
        self.lootPos = (int(px) + dx, int(py) + dy)

    def WhileDead(self, now):
        self.ReleaseAttack()
        self.targetVid = 0
        self.lootVid = 0
        if not self.deadSince:
            self.deadSince = now
            return
        wait = max(REVIVE_MIN_SECONDS, self.config['revive_after'])
        if self.config['revive'] and now - self.deadSince >= wait and now >= self.nextRevive:
            self.nextRevive = now + REVIVE_RETRY
            self.justRevived = True
            net.SendChatPacket('/restart_here')

    def HandleItems(self, now):
        if self.config['use_potions'] and now >= self.nextPotion:
            wanted = False
            for i in xrange(6):
                vnum = self.config['item%d_vnum' % i]
                val = self.config['item%d_val' % i]
                if not vnum or val <= 0:
                    continue

                if IsManaItem(vnum):
                    curPoint = player.GetStatus(player.SP)
                    maxPoint = player.GetStatus(player.MAX_SP)
                else:
                    curPoint = player.GetStatus(player.HP)
                    maxPoint = player.GetStatus(player.MAX_HP)

                if maxPoint > 0 and (curPoint * 100) <= (maxPoint * val):
                    # A second before the next look whether or not the bag
                    # still has one: the search walks every cell of four
                    # pages, and on every frame it would cost the frame.
                    wanted = True
                    cell = FindInventoryCell(vnum)
                    if cell >= 0:
                        net.SendItemUsePacket(cell)

            if wanted:
                self.nextPotion = now + POTION_INTERVAL

        if self.config['use_buffs']:
            if now >= self.nextBuffGlobal:
                for i in xrange(6, 18):
                    vnum = self.config['item%d_vnum' % i]
                    interval = self.config['item%d_val' % i]
                    if not vnum or interval <= 0 or now < self.itemNext[i]:
                        continue

                    # The slot's own clock first, so an item nobody carries
                    # is looked for once an interval and not on every frame.
                    self.itemNext[i] = now + max(ITEM_MIN_INTERVAL, interval)
                    cell = FindInventoryCell(vnum)
                    if cell >= 0:
                        net.SendItemUsePacket(cell)
                        self.nextBuffGlobal = now + 0.1
                        break

    def AskForLoot(self, now):
        mask = LootMask(self.config)
        if not mask:
            self.lootVid = 0
            return
        if now < self.nextLootRequest or now < self.lootPausedUntil:
            return
        self.nextLootRequest = now + LOOT_REQUEST_INTERVAL
        (dx, dy) = self.AnchorOffset()
        net.SendChatPacket('/autohunt_loot %d %d %d %d' % (self.config['range'], mask, dx, dy))

    def Chase(self, now):
        if self.config['attack']:
            if now >= self.nextRequest:
                self.nextRequest = now + TARGET_REQUEST_INTERVAL
                (dx, dy) = self.AnchorOffset()
                
                stones_flag = 1 if self.config.get('stones', 0) else 0
                mobs_flag   = 1 if self.config.get('mobs', 1) else 0
                bosses_flag = 1 if self.config.get('bosses', 0) else 0
                command = '/autohunt_target %d %d %d %d %d %d' % (
                    self.config['range'],
                    stones_flag,
                    dx, dy,
                    mobs_flag,
                    bosses_flag
                )
                    
                if self.skipVid and now < self.skipUntil:
                    command += ' %d' % self.skipVid
                net.SendChatPacket(command)
        else:
            self.targetVid = 0

        self.PickNearLoot(now)
        
        vid = self.targetVid

        if vid and hasattr(player, 'IsTargetDead') and player.IsTargetDead(vid):
            if self.attacking:
                self.ReleaseAttack()
            if player.GetTargetVID() != 0:
                player.ClearTarget()
                
            self.skipVid = vid
            self.skipUntil = now + 5.0
            
            self.targetVid = 0
            self.nextRequest = 0
            return

        distance = player.GetCharacterDistance(vid) if vid else -1
        
        if (self.lootVid and (distance < 0 or distance > self.Reach()) and
                self.LootDistance() <= LOOT_FIRST_DISTANCE and self.GoForLoot(now)):
            self.ReleaseAttack()
            return
        
        if distance < 0:
            self.targetVid = 0
            self.ReleaseAttack()
            if player.GetTargetVID() != 0:
                player.ClearTarget()
            if not self.GoForLoot(now):
                self.ReturnToAnchor(now)
            return
            
        reach = self.Reach()
        if distance > reach:
            self.ReleaseAttack()
            if not self.approachSince:
                self.approachSince = now
            elif now - self.approachSince > STUCK_SECONDS:
                self.skipVid = vid
                self.skipUntil = now + STUCK_SKIP_SECONDS
                self.targetVid = 0
                self.approachSince = 0.0
                self.nextRequest = now + STUCK_PAUSE
                self.WalkTo(self.anchor[0], self.anchor[1])
                return
            if now >= self.nextMove:
                self.nextMove = now + MOVE_INTERVAL
                (px, py, pz) = player.GetMainCharacterPosition()
                (tx, ty, tz) = chr.GetPixelPosition(vid)
                (sx, sy) = StopPoint(px, py, tx, ty, reach * STOP_SHORT_SHARE)
                self.WalkTo(sx, sy)
            return
            
        self.approachSince = 0.0
        if now >= self.nextFace:
            self.nextFace = now + FACE_INTERVAL
            self.Face(vid)
            
        current_target = player.GetTargetVID()

        if current_target != vid:
            if self.attacking:
                self.ReleaseAttack()
            if current_target != 0:
                player.ClearTarget()
            if vid != 0:
                player.SetTarget(vid)
        else:
            if not self.attacking:
                player.SetAttackKeyState(True)
                self.attacking = True

    def CastSkills(self, now):
        if not self.config['use_skills']:
            return

        fightDistance = None
        for index in xrange(SKILL_SLOTS):
            slot = self.config['skill%d_slot' % index]
            if not slot or now < self.skillNext[index]:
                continue
            skillIndex = player.GetSkillIndex(slot)
            if not skillIndex or player.IsSkillCoolTime(slot):
                continue
            if skill.IsToggleSkill(skillIndex) and player.IsSkillActive(slot):
                continue
            if NeedsTarget(skillIndex):
                # The slot's clock is left alone, so the skill goes on the
                # first frame the fight is there, and a buff further down
                # still goes on this one.
                if fightDistance is None:
                    fightDistance = self.FightDistance()
                limit = MELEE_REACH if skillIndex in WARP_SKILLS else self.Reach()
                if fightDistance < 0 or fightDistance > limit:
                    continue
            player.ClickSkillSlot(slot)
            self.skillNext[index] = now + max(SKILL_MIN_INTERVAL, float(self.config['skill%d_interval' % index]))
            return

    def FightDistance(self):
        """How far stands the monster a skill would be cast at, or -1 when the
        client would have to find one itself: the hunter's own target, alive,
        and in the client's hand - Chase marks it only within reach, and a
        mark not taken yet or held by a corpse sends the client to the mouse
        cursor. With the attack switched off nothing is fought (and Chase
        clears whatever the client holds), so nothing is cast at an enemy."""
        vid = self.targetVid if self.config['attack'] else 0
        if not vid or player.GetTargetVID() != vid:
            return -1
        if hasattr(player, 'IsTargetDead') and player.IsTargetDead(vid):
            return -1
        return player.GetCharacterDistance(vid)

    def PickNearLoot(self, now):
        if not self.lootVid or now < self.nextLootPick:
            return False
        if self.LootDistance() > LOOT_PICK_DISTANCE:
            return False
        self.nextLootPick = now + LOOT_PICK_INTERVAL
        net.SendItemPickUpPacket(self.lootVid)
        self.lootVid = 0
        self.lootSince = 0.0
        self.nextLootRequest = min(self.nextLootRequest, now + 0.3)
        return True

    def GoForLoot(self, now):
        if not self.lootVid:
            return False
        if self.LootDistance() <= LOOT_PICK_DISTANCE:
            return True
        if not self.lootSince:
            self.lootSince = now
        elif now - self.lootSince > LOOT_STUCK_SECONDS:
            self.lootVid = 0
            self.lootSince = 0.0
            self.lootPausedUntil = now + LOOT_STUCK_PAUSE
            return False
        if now >= self.nextMove:
            self.nextMove = now + MOVE_INTERVAL
            self.WalkTo(self.lootPos[0], self.lootPos[1])
        return True

    def Reach(self):
        if hasattr(player, 'IsBowEquipped'):
            if player.IsBowEquipped():
                return ARCHER_REACH
            return MELEE_REACH
        # An exe older than client 2.0.25 cannot say what is in the hand: a
        # Ninja of the archery school is taken to hold its bow.
        if net.GetMainActorRace() % 4 == 1 and net.GetMainActorSkillGroup() == 2:
            return ARCHER_REACH
        return MELEE_REACH

    def AnchorOffset(self):
        if self.config.get('return', 0):
            (px, py, pz) = player.GetMainCharacterPosition()
            return (self.anchor[0] - int(px), self.anchor[1] - int(py))
        else:
            return (0, 0)

    def LootDistance(self):
        (px, py, pz) = player.GetMainCharacterPosition()
        (lx, ly) = self.lootPos
        return math.sqrt((px - lx) * (px - lx) + (py - ly) * (py - ly))

    def ReturnToAnchor(self, now):
        if not self.config['return'] or now < self.nextMove:
            return
        (px, py, pz) = player.GetMainCharacterPosition()
        (ax, ay) = self.anchor
        if (px - ax) * (px - ax) + (py - ay) * (py - ay) <= ANCHOR_LEASH * ANCHOR_LEASH:
            return
        self.nextMove = now + RETURN_MOVE_INTERVAL
        self.WalkTo(ax, ay)

    def WalkTo(self, x, y):
        chr.MoveToDestPosition(player.GetMainCharacterIndex(), int(x), int(y))

    def Face(self, vid):
        (px, py, pz) = player.GetMainCharacterPosition()
        (tx, ty, tz) = chr.GetPixelPosition(vid)
        chr.SelectInstance(player.GetMainCharacterIndex())
        chr.SetRotation(FacingDegree(px, py, tx, ty))

    def ReleaseAttack(self):
        if self.attacking:
            player.SetAttackKeyState(False)
            self.attacking = False

    def LoadGlobalConfig(self):
        path = GlobalConfigPath()
        if os.path.exists(path):
            try:
                with open(path, 'r') as handle:
                    for line in handle.read().splitlines():
                        key, _, value = line.partition('=')
                        key = key.strip()
                        if key in self.config_global:
                            try:
                                self.config_global[key] = int(value.strip())
                            except ValueError:
                                pass
            except (IOError, OSError):
                pass

    def LoadConfig(self):
        self.config = DefaultConfig()
        
        path = ConfigPath(self.configName)
        for older in (OldConfigPath(self.configName), OldestConfigPath(self.configName)):
            if os.path.exists(path):
                break
            path = older
                
        try:
            with open(path, 'r') as handle:
                self.config = ConfigFromText(handle.read())
        except (IOError, OSError):
            pass

    def SaveGlobalConfig(self):
        if not os.path.exists(CONFIG_BASE_DIR):
            try:
                os.makedirs(CONFIG_BASE_DIR)
            except (IOError, OSError):
                pass
                
        if self.mainWindow and self.lootWindow:
            try:
                mx, my = self.mainWindow.GetLocalPosition()
                lx, ly = self.lootWindow.GetLocalPosition()
                self.config_global['win_main_x'] = int(mx)
                self.config_global['win_main_y'] = int(my)
                self.config_global['win_loot_x'] = int(lx)
                self.config_global['win_loot_y'] = int(ly)
            except RuntimeError:
                pass
                
        try:
            with open(GlobalConfigPath(), 'w') as handle:
                for k, v in self.config_global.items():
                    handle.write('%s=%d\n' % (k, v))
        except (IOError, OSError):
            pass

    def SaveConfig(self):
        if not os.path.exists(CONFIG_CHAR_DIR):
            try:
                os.makedirs(CONFIG_CHAR_DIR)
            except (IOError, OSError):
                pass
                
        self.SaveGlobalConfig()
        
        try:
            with open(ConfigPath(self.configName), 'w') as handle:
                handle.write(ConfigText(self.config))
            return True
        except (IOError, OSError):
            return False

    def CreateWindows(self):
        self.mainWindow = AutoHuntWindow(self)
        self.lootWindow = AutoHuntLootWindow(self)
        
        sw = wndMgr.GetScreenWidth()
        sh = wndMgr.GetScreenHeight()
        
        w1 = self.mainWindow.WIDTH
        h1 = self.mainWindow.HEIGHT
        w2 = self.lootWindow.WIDTH
        
        mx = self.config_global.get('win_main_x', -1)
        my = self.config_global.get('win_main_y', -1)
        lx = self.config_global.get('win_loot_x', -1)
        ly = self.config_global.get('win_loot_y', -1)
        
        if mx < 0 or my < 0 or mx + w1 > sw or my + h1 > sh:
            mx = max(0, (sw - (w1 + 10 + w2)) / 2)
            my = max(0, (sh - h1) / 2)
            lx = mx + w1 + 10
            ly = my
            
        if lx < 0 or ly < 0 or lx + w2 > sw or ly + self.lootWindow.HEIGHT > sh:
            lx = mx + w1 + 10
            ly = my
            
        self.mainWindow.SetPosition(int(mx), int(my))
        self.lootWindow.SetPosition(int(lx), int(ly))

    def ToggleWindow(self):
        if not self.isLoaded:
            self.configName = player.GetMainCharacterName()
            self.LoadConfig()
            self.isLoaded = True
            
        if self.mainWindow is None:
            self.CreateWindows()

        try:
            is_show = self.mainWindow.IsShow()
        except RuntimeError:
            self.mainWindow = None
            self.lootWindow = None
            self.CreateWindows()
            is_show = False

        if is_show:
            self.mainWindow.Close()
            self.lootWindow.Close()
        else:
            self.mainWindow.Refresh()
            self.lootWindow.Refresh()
            self.mainWindow.Show()
            self.lootWindow.Show()
            self.mainWindow.SetTop()
            self.lootWindow.SetTop()
            self.SaveGlobalConfig()


class AutoHuntWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    HEIGHT = 555
    SLOT_STEP = 40
    SLOTS_PER_ROW = 6
    EDIT_W = 34
    EDIT_H = 18

    def __init__(self, hunter):
        ui.BoardWithTitleBar.__init__(self)
        self.hunter = hunter
        self.widgets = []
        self.edits = {}
        self.toggles = {}
        self.nextStatus = 0.0
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.HEIGHT)
        self.SetTitleName('Auto \xa3owy - Walka')
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        SL = 20
        SECTION_GAP = 5
        y = 32

        sk_r1_y = 22
        sk_e1_y = sk_r1_y + 34
        sk_r2_y = sk_e1_y + 18 + 4
        sk_e2_y = sk_r2_y + 34
        sk_h = sk_e2_y + 18 + 7

        skBoard = self._Board(BL, y, BW, sk_h)
        self._Label(skBoard, 14, 4, 'Umiej\xeatno\x9cci')

        self.skillSlots1 = self._Slots(skBoard, SL, sk_r1_y, self.SLOTS_PER_ROW)
        self.skillSlots1.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvSkill1))
        self.skillSlots1.SetSelectItemSlotEvent(ui.__mem_func__(self._EvSkill1))
        self.skillSlots1.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrSkill1))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(skBoard, SL + i * self.SLOT_STEP, sk_e1_y, self.EDIT_W, 'skill%d_interval' % i)

        self.skillSlots2 = self._Slots(skBoard, SL, sk_r2_y, self.SLOTS_PER_ROW)
        self.skillSlots2.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvSkill2))
        self.skillSlots2.SetSelectItemSlotEvent(ui.__mem_func__(self._EvSkill2))
        self.skillSlots2.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrSkill2))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(skBoard, SL + i * self.SLOT_STEP, sk_e2_y, self.EDIT_W, 'skill%d_interval' % (i + self.SLOTS_PER_ROW))

        y += sk_h + SECTION_GAP

        mk_r1_y = 22
        mk_e1_y = mk_r1_y + 34
        mk_h = mk_e1_y + 18 + 7

        mkBoard = self._Board(BL, y, BW, mk_h)
        self._Label(mkBoard, 14, 4, 'Mikstury (% HP / PE)')

        self.itemSlots1 = self._Slots(mkBoard, SL, mk_r1_y, self.SLOTS_PER_ROW)
        self.itemSlots1.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem1))
        self.itemSlots1.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem1))
        self.itemSlots1.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem1))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(mkBoard, SL + i * self.SLOT_STEP, mk_e1_y, self.EDIT_W, ITEM_EDIT_KEYS[i])

        y += mk_h + SECTION_GAP

        od_r1_y = 22
        od_e1_y = od_r1_y + 34
        od_r2_y = od_e1_y + 18 + 4
        od_e2_y = od_r2_y + 34
        od_h = od_e2_y + 18 + 7

        odBoard = self._Board(BL, y, BW, od_h)
        self._Label(odBoard, 14, 4, 'Odpa\xb3y (Sekundy)')

        self.itemSlots2 = self._Slots(odBoard, SL, od_r1_y, self.SLOTS_PER_ROW)
        self.itemSlots2.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem2))
        self.itemSlots2.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem2))
        self.itemSlots2.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem2))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(odBoard, SL + i * self.SLOT_STEP, od_e1_y, self.EDIT_W, ITEM_EDIT_KEYS[i + self.SLOTS_PER_ROW])

        self.itemSlots3 = self._Slots(odBoard, SL, od_r2_y, self.SLOTS_PER_ROW)
        self.itemSlots3.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem3))
        self.itemSlots3.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem3))
        self.itemSlots3.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem3))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(odBoard, SL + i * self.SLOT_STEP, od_e2_y, self.EDIT_W, ITEM_EDIT_KEYS[i + self.SLOTS_PER_ROW * 2])

        y += od_h + SECTION_GAP

        ROW_H = 19
        st_row_start = 22
        settings_rows = [
            ('Atak',           'attack',            'toggle'),
            ('Umiej\xeatno\x9cci', 'use_skills',    'toggle'),
            ('Wskrzeszenie',   'revive',            'toggle'),
            ('HP po wskrz. %', 'revive_hp_percent', 'edit'),
            ('Mikstury',       'use_potions',       'toggle'),
            ('Odpa\xb3y',      'use_buffs',         'toggle'),
            ('Wracaj',         'return',            'toggle'),
        ]
        st_h = st_row_start + 4 * ROW_H + 4

        stBoard = self._Board(BL, y, BW, st_h)
        self._Label(stBoard, 14, 4, 'Ustawienia Walki')

        col_w = (BW - 8) // 2
        for idx, (lbl, key, kind) in enumerate(settings_rows):
            col = idx // 4
            row = idx % 4
            x = 4 + col * col_w
            ry = st_row_start + row * ROW_H
            if kind == 'toggle':
                self._SettingRow(stBoard, x, ry, col_w - 2, ROW_H - 1, lbl, key)
            else:
                self._EditSettingRow(stBoard, x, ry, col_w - 2, ROW_H - 1, lbl, key)

        y += st_h + 4

        bw = 88
        gap = 8
        total = 3 * bw + 2 * gap
        bx = (self.WIDTH - total) // 2

        self.statusText = self._Label(self, bx + 6, y, 'Wy\xb3\xb9czone')
        y += 18

        self._Btn(self, 'large', bx, y, 'Zapisz', self.OnSave)
        bx += bw + gap
        self.startButton = self._Btn(self, 'large', bx, y, 'Start', self.OnStart)
        bx += bw + gap
        self.stopButton = self._Btn(self, 'large', bx, y, 'Zatrzymaj', self.OnStop)

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

    def _Slots(self, parent, x, y, count):
        slots = ui.SlotWindow()
        slots.SetParent(parent)
        slots.SetPosition(x, y)
        slots.SetSize(count * self.SLOT_STEP, 32)
        for i in xrange(count):
            slots.AppendSlot(i, i * self.SLOT_STEP, 0, 32, 32)
        slots.SetSlotBaseImage('d:/ymir work/ui/public/slot_base.sub', 1.0, 1.0, 1.0, 1.0)
        slots.Show()
        self.widgets.append(slots)
        return slots

    def _EditField(self, parent, x, y, w, key):
        bar = ui.SlotBar()
        bar.SetParent(parent)
        bar.SetPosition(x, y)
        bar.SetSize(w, self.EDIT_H)
        bar.Show()
        self.widgets.append(bar)

        edit = ui.EditLine()
        edit.SetParent(bar)
        edit.SetPosition(4, 1)
        edit.SetSize(w - 6, self.EDIT_H - 2)
        edit.SetMax(4)
        edit.SetNumberMode()
        edit.Show()
        self.widgets.append(edit)
        self.edits[key] = edit

    def _SettingRow(self, board, x, y, w, h, label, key):
        bar = ui.Bar()
        bar.SetParent(board)
        bar.SetPosition(x, y)
        bar.SetSize(w, h)
        bar.SetColor(0xC0000000)
        bar.Show()
        self.widgets.append(bar)

        btn_w = 50
        text_area = w - btn_w - 4
        
        tx = x + (text_area // 2)
        lbl_line = self._Label(board, tx, y + (h - 12) // 2, label)
        lbl_line.SetHorizontalAlignCenter()

        btn = ui.Button()
        btn.SetParent(board)
        btn.SetPosition(x + w - btn_w, y)
        btn.SetUpVisual('d:/ymir work/ui/public/small_button_01.sub')
        btn.SetOverVisual('d:/ymir work/ui/public/small_button_02.sub')
        btn.SetDownVisual('d:/ymir work/ui/public/small_button_03.sub')
        btn.SetText('WY\xa3')
        btn.SAFE_SetEvent(self.OnToggle, key)
        btn.Show()
        self.widgets.append(btn)
        self.toggles[key] = (btn, label, True)

    def _EditSettingRow(self, board, x, y, w, h, label, key):
        bar = ui.Bar()
        bar.SetParent(board)
        bar.SetPosition(x, y)
        bar.SetSize(w, h)
        bar.SetColor(0xC0000000)
        bar.Show()
        self.widgets.append(bar)

        edit_w = 34 
        text_area = w - 50 - 4
        
        tx = x + (text_area // 2)
        lbl_line = self._Label(board, tx, y + (h - 12) // 2, label)
        lbl_line.SetHorizontalAlignCenter()

        slotbar = ui.SlotBar()
        slotbar.SetParent(board)
        slotbar.SetPosition(x + w - 46, y + 1)
        slotbar.SetSize(edit_w, h - 2)
        slotbar.Show()
        self.widgets.append(slotbar)

        edit = ui.EditLine()
        edit.SetParent(slotbar)
        edit.SetPosition(4, (h - 2 - 14) // 2)
        edit.SetSize(edit_w - 8, 14)
        edit.SetMax(3)
        edit.SetNumberMode()
        edit.Show()
        self.widgets.append(edit)
        self.edits[key] = edit

    def _EvSkill1(self, idx):
        self.OnSkillSlot(idx)

    def _EvClrSkill1(self, idx):
        self.OnClearSkillSlot(idx)

    def _EvSkill2(self, idx):
        self.OnSkillSlot(idx + self.SLOTS_PER_ROW)

    def _EvClrSkill2(self, idx):
        self.OnClearSkillSlot(idx + self.SLOTS_PER_ROW)

    def _EvItem1(self, idx):
        self.OnItemSlot(idx)

    def _EvClrItem1(self, idx):
        self.OnClearItemSlot(idx)

    def _EvItem2(self, idx):
        self.OnItemSlot(idx + self.SLOTS_PER_ROW)

    def _EvClrItem2(self, idx):
        self.OnClearItemSlot(idx + self.SLOTS_PER_ROW)

    def _EvItem3(self, idx):
        self.OnItemSlot(idx + self.SLOTS_PER_ROW * 2)

    def _EvClrItem3(self, idx):
        self.OnClearItemSlot(idx + self.SLOTS_PER_ROW * 2)

    def Refresh(self):
        config = self.hunter.config
        for key, (btn, label, wyl) in self.toggles.items():
            if wyl:
                btn.SetText(WlWyl(config[key]))
            else:
                btn.SetText('%s: %s' % (label, YesNo(config[key])))
        for key, edit in self.edits.items():
            edit.SetText(str(config[key]))
        self.RefreshSlots()
        self.RefreshStatus()

    def RefreshSlots(self):
        config = self.hunter.config
        for i in xrange(self.SLOTS_PER_ROW):
            slot = config['skill%d_slot' % i]
            si = player.GetSkillIndex(slot) if slot else 0
            if si:
                self.skillSlots1.SetSkillSlotNew(i, si, player.GetSkillGrade(slot), player.GetSkillLevel(slot))
            else:
                self.skillSlots1.ClearSlot(i)
        self.skillSlots1.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            gi = i + self.SLOTS_PER_ROW
            slot = config['skill%d_slot' % gi]
            si = player.GetSkillIndex(slot) if slot else 0
            if si:
                self.skillSlots2.SetSkillSlotNew(i, si, player.GetSkillGrade(slot), player.GetSkillLevel(slot))
            else:
                self.skillSlots2.ClearSlot(i)
        self.skillSlots2.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i]]
            if vnum:
                self.itemSlots1.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots1.ClearSlot(i)
        self.itemSlots1.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i + self.SLOTS_PER_ROW]]
            if vnum:
                self.itemSlots2.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots2.ClearSlot(i)
        self.itemSlots2.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i + self.SLOTS_PER_ROW * 2]]
            if vnum:
                self.itemSlots3.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots3.ClearSlot(i)
        self.itemSlots3.RefreshSlot()

    def RefreshStatus(self):
        hunter = self.hunter
        if not hunter.running:
            self.statusText.SetText('Wy\xb3\xb9czone')
            return
        if hunter.justRevived:
            pct = min(100, hunter.config.get('revive_hp_percent', 60))
            self.statusText.SetText('Czekam na HP (%d%%)' % pct)
            return
        if hunter.targetVid:
            self.statusText.SetText('Cel: %s' % chr.GetNameByVID(hunter.targetVid))
        elif hunter.lootVid:
            self.statusText.SetText('Podnosz\xea przedmiot')
        else:
            self.statusText.SetText('Szukam potwork\xf3w')

    def ReadEdits(self):
        for key, edit in self.edits.items():
            try:
                self.hunter.config[key] = max(0, int(edit.GetText() or 0))
            except ValueError:
                pass

    def OnUpdate(self):
        now = clientclock.Now()
        if now < self.nextStatus:
            return
        self.nextStatus = now + STATUS_INTERVAL
        self.RefreshStatus()

    def OnStart(self):
        self.ReadEdits()
        if not self.hunter.running:
            self.hunter.Start()
        self.RefreshStatus()

    def OnStop(self):
        self.ReadEdits()
        if self.hunter.running:
            self.hunter.Stop()
        self.RefreshStatus()

    def OnToggle(self, key):
        self.ReadEdits()
        self.hunter.config[key] = 0 if self.hunter.config[key] else 1
        self.Refresh()

    def OnSave(self):
        self.ReadEdits()
        if self.hunter.SaveConfig():
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Auto \xa3owy: ustawienia zapisane.')
        else:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Auto \xa3owy: nie uda\xb3o si\xea zapisa\xe6 ustawie\xf1.')

    def OnSkillSlot(self, index):
        attached = self.TakeAttached()
        if attached and attached[0] == player.SLOT_TYPE_SKILL:
            self.hunter.config['skill%d_slot' % index] = attached[1]
            self.RefreshSlots()

    def OnClearSkillSlot(self, index):
        self.hunter.config['skill%d_slot' % index] = 0
        self.RefreshSlots()

    def OnItemSlot(self, index):
        attached = self.TakeAttached()
        if attached and attached[0] == player.SLOT_TYPE_INVENTORY:
            self.hunter.config[ITEM_SLOT_KEYS[index]] = attached[2]
            self.RefreshSlots()

    def OnClearItemSlot(self, index):
        self.hunter.config[ITEM_SLOT_KEYS[index]] = 0
        self.RefreshSlots()

    def TakeAttached(self):
        controller = mouseModule.mouseController
        if not controller.isAttached():
            return None
        attached = (
            controller.GetAttachedType(),
            controller.GetAttachedSlotNumber(),
            controller.GetAttachedItemIndex(),
        )
        controller.DeattachObject()
        return attached

    def Close(self):
        self.ReadEdits()
        if self.hunter:
            self.hunter.SaveGlobalConfig()
        self.Hide()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.hunter = None
        self.widgets = []
        self.edits = {}
        self.toggles = {}


class AutoHuntLootWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    HEIGHT = 245

    def __init__(self, hunter):
        ui.BoardWithTitleBar.__init__(self)
        self.hunter = hunter
        self.widgets = []
        self.toggles = {}
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.HEIGHT)
        self.SetTitleName('Auto \xa3owy - Ustawienia')
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        y = 32

        pd_btn_start = 24
        pd_h = pd_btn_start + 3 * 22 + 8 
        pdBoard = self._Board(BL, y, BW, pd_h)
        self._Label(pdBoard, 14, 4, 'Podnoszenie')
        
        pdy = pd_btn_start
        self._FlagBtn(pdBoard, 4, pdy, 'Podnie\x9c', 'pickup')
        for idx, (key, label, bit) in enumerate(LOOT_KINDS):
            pos = idx + 1
            col = pos % 3
            row = pos // 3
            self._FlagBtn(pdBoard, 4 + col * 92, pdy + row * 22, label, key)
            
        y += pd_h + 5

        tg_btn_start = 24
        tg_h = tg_btn_start + 22 + 8
        tgBoard = self._Board(BL, y, BW, tg_h)
        self._Label(tgBoard, 14, 4, 'Cele do atakowania')
        
        self._FlagBtn(tgBoard, 4 + 0 * 92, tg_btn_start, 'Moby', 'mobs')
        self._FlagBtn(tgBoard, 4 + 1 * 92, tg_btn_start, 'Metiny', 'stones')
        self._FlagBtn(tgBoard, 4 + 2 * 92, tg_btn_start, 'Bossy', 'bosses')
        
        y += tg_h + 10
            
        self.rangeText = self._Label(self, self.WIDTH // 2, y, 'Zasi\xeag: 2000')
        self.rangeText.SetHorizontalAlignCenter()
        
        self.rangeSlider = ui.SliderBar()
        self.rangeSlider.SetParent(self)
        self.rangeSlider.SetWindowHorizontalAlignCenter()
        self.rangeSlider.SetPosition(0, y + 18)
        self.rangeSlider.SetEvent(ui.__mem_func__(self.OnChangeRange))
        self.rangeSlider.Show()
        self.widgets.append(self.rangeSlider)

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

    def _FlagBtn(self, parent, x, y, label, key):
        btn = self._Btn(parent, 'large', x, y, '', self.OnToggle, key)
        self.toggles[key] = (btn, label, False)
        return btn

    def Refresh(self):
        config = self.hunter.config
        
        current_range = max(300, min(5000, config['range']))
        slider_pos = float(current_range - 300) / 4700.0
        self.rangeSlider.SetSliderPos(slider_pos)
        self.rangeText.SetText('Zasi\xeag: %d' % current_range)
        
        for key, (btn, label, wyl) in self.toggles.items():
            btn.SetText('%s: %s' % (label, YesNo(config[key])))

    def OnChangeRange(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
            
        pos = self.rangeSlider.GetSliderPos()
        new_range = int(300 + (pos * 4700))
        self.hunter.config['range'] = new_range
        self.rangeText.SetText('Zasi\xeag: %d' % new_range)
        
        try:
            if self.hunter.running and self.hunter.config.get('return', 0):
                (ax, ay) = self.hunter.anchor
                player.SetAutoHuntRangeCircle(new_range, float(ax), float(ay), 1)
            else:
                player.SetAutoHuntRangeCircle(new_range, 0.0, 0.0, 0)
        except AttributeError:
            pass

    def OnToggle(self, key):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        self.hunter.config[key] = 0 if self.hunter.config[key] else 1
        self.Refresh()

    def Close(self):
        try:
            player.SetAutoHuntRangeCircle(0)
        except AttributeError:
            pass
            
        if self.hunter:
            self.hunter.SaveGlobalConfig()
        self.Hide()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.hunter = None
        self.widgets = []
        self.toggles = {}


_hunter = None

def GetHunter():
    global _hunter
    if _hunter is None:
        _hunter = Hunter()
    return _hunter

def ToggleWindow():
    GetHunter().ToggleWindow()

def OnServerTarget(value):
    GetHunter().OnServerTarget(value)

def OnServerLoot(vid, x, y):
    GetHunter().OnServerLoot(vid, x, y)