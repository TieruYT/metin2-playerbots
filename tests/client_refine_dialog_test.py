# -*- coding: utf-8 -*-
"""uirefine.RefineDialogNew.Open, run with the engine modules stubbed.

The server enters refine mode when it sends the refine dialog and refuses every
move in the bag until the window answers with a refine or the cancel (255, 255).
A window that raised before Show() never answered: in six languages REFINE_COST
took a number where NumberToMoneyString hands a string, and every one of those
players lost the bag at the blacksmith until the next login (23 September).
Two things have to hold for the root's uirefine.py:

  * a text that does not take the arguments costs the text, not the window:
    Open shows the window with the plain amount and sends nothing;
  * anything else that raises in Open sends the cancel before the error goes
    on to syserr.txt, and leaves the window hidden.

Usage (Python 2.7, the client's own):
    python tests/client_refine_dialog_test.py <client-root>
"""
import os
import sys
import types


class Recorder(object):
    def __init__(self):
        self.calls = []

    def __getattr__(self, name):
        if name.startswith('__'):
            raise AttributeError(name)

        def call(*args, **kwargs):
            self.calls.append((name, args))
            return 0
        return call


class Widget(Recorder):
    def GetWidth(self):
        return 100

    def GetHeight(self):
        return 100

    def GetLocalPosition(self):
        return (0, 0)


def stub_modules(refine_cost):
    net = types.ModuleType('net')
    net.sent = []
    net.SendRefinePacket = lambda pos, kind: net.sent.append((pos, kind))
    sys.modules['net'] = net

    ui = types.ModuleType('ui')

    class ScriptWindow(object):
        def __init__(self, *args):
            self.shown = False

        def __del__(self):
            pass

        def Show(self):
            self.shown = True

        def Hide(self):
            self.shown = False

        def SetTop(self):
            pass

        def SetSize(self, w, h):
            pass

        def SetPosition(self, x, y):
            pass

        def GetLocalPosition(self):
            return (0, 0)
    ui.ScriptWindow = ScriptWindow
    ui.__mem_func__ = lambda f: f
    ui.WindowDestroy = lambda f: f  # a decorator on Destroy
    sys.modules['ui'] = ui

    locale = types.ModuleType('localeInfo')
    locale.REFINE_SUCCESS_PROBALITY = 'Chance %d%%'
    locale.REFINE_COST = refine_cost
    locale.NumberToMoneyString = lambda n: '%d Yang' % n
    sys.modules['localeInfo'] = locale

    player = types.ModuleType('player')
    player.METIN_SOCKET_MAX_NUM = 6
    player.ATTRIBUTE_SLOT_MAX_NUM = 7
    player.GetItemMetinSocket = lambda pos, i: 0
    player.GetItemAttribute = lambda pos, i: (0, 0)
    sys.modules['player'] = player

    item = types.ModuleType('item')
    item.SelectItem = lambda vnum: None
    item.GetIconImageFileName = lambda: 'icon.tga'
    item.GetItemSize = lambda: (1, 2)
    sys.modules['item'] = item

    const = types.ModuleType('constInfo')
    const.IS_UNIQUE_70LEVEL_WEAPON = lambda vnum: False
    sys.modules['constInfo'] = const

    events = types.ModuleType('eventManager')

    class EventManager(object):
        def send_event(self, *args):
            pass
    events.EventManager = EventManager
    sys.modules['eventManager'] = events

    for name in ('app', 'uiToolTip', 'mouseModule', 'uiCommon', 'utils', 'colorInfo'):
        sys.modules[name] = types.ModuleType(name)
    return net


def new_dialog(uirefine, tooltip):
    dialog = uirefine.RefineDialogNew.__new__(uirefine.RefineDialogNew)
    sys.modules['ui'].ScriptWindow.__init__(dialog)
    dialog.isLoaded = True
    dialog._RefineDialogNew__Initialize = lambda: None
    dialog.probText = Widget()
    dialog.costText = Widget()
    dialog.toolTip = tooltip
    dialog.itemImage = Widget()
    dialog.slotList = [Widget(), Widget(), Widget()]
    dialog.dialogHeight = 0
    dialog.UpdateDialog = lambda: None
    return dialog


def load(root, refine_cost):
    net = stub_modules(refine_cost)
    sys.modules.pop('uirefine', None)
    sys.path.insert(0, root)
    try:
        import uirefine
    finally:
        sys.path.pop(0)
    return net, uirefine


def main():
    root = sys.argv[1]
    failures = []

    # A language whose REFINE_COST takes a number: the window still opens.
    net, uirefine = load(root, 'Kosten: %d Yang')
    dialog = new_dialog(uirefine, Widget())
    dialog.Open(5, 1001, 1000, 90, 0)
    cost = [args for name, args in dialog.costText.calls if name == 'SetText']
    if not dialog.shown:
        failures.append('a REFINE_COST taking a number still keeps the window shut')
    if cost != [('1000 Yang',)]:
        failures.append('the cost fell back to %r, not the plain amount' % (cost,))
    if net.sent:
        failures.append('an opened window sent %r' % (net.sent,))

    # Anything else raising in Open: the cancel goes to the server, the error on.
    net, uirefine = load(root, 'Cost: %s')

    class BrokenTooltip(Widget):
        def AddRefineItemData(self, *args):
            raise ValueError('a tooltip that cannot draw')
    dialog = new_dialog(uirefine, BrokenTooltip())
    try:
        dialog.Open(5, 1001, 1000, 90, 0)
        failures.append('an error in Open was swallowed')
    except ValueError:
        pass
    if net.sent != [(255, 255)]:
        failures.append('a window that failed to open sent %r, not the cancel' % (net.sent,))
    if dialog.shown:
        failures.append('a window that failed to open is shown')

    if failures:
        print('BLEDY (%d):' % len(failures))
        for f in failures:
            print('  ' + f)
        sys.exit(1)
    print('client_refine_dialog_test: PASS')


if __name__ == '__main__':
    main()
