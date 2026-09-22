# -*- coding: utf-8 -*-
"""The offline shop's "edit the price of similar items", one packet at a time.

Ctrl + right click repriced every line holding the same item and the client
sent one packet per line in a single frame; the server takes one shop action
per 200 ms, so the first line went through and every other one answered "wait
a moment". shoppricepump.py spaces them out.

Runs on the client's Python 2.7 and on 3:

    python tests/shoppricepump_test.py
"""
import os
import sys
import types

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    '..', 'linux-port-mt2009', 'client-root')

sent = []
prices = []
now = [0.0]


class _Window(object):
    def __init__(self):
        pass

    def SetSize(self, w, h):
        pass

    def Show(self):
        pass


def _stub(name, **members):
    module = types.ModuleType(name)
    for key, value in members.items():
        setattr(module, key, value)
    sys.modules[name] = module
    return module


chats = []
# The client's copy of its own counter: slot -> row. The pump reads it to see
# which of its edits actually took.
shop = {'items': {}}


def _apply(item_id, price):
    for row in shop['items'].values():
        if row['id'] == item_id:
            row['price'] = price


def _shop_edit(item_id, price):
    sent.append((item_id, price))
    # The server answers an edit by refreshing this; a refused one changes
    # nothing, which is exactly what the pump has to notice. On a busy world
    # the answer comes LATE seconds later (the db core's queue).
    if item_id in REFUSE:
        return
    if LATE[0] > 0:
        pending.append((now[0] + LATE[0], item_id, price))
    else:
        _apply(item_id, price)


REFUSE = set()
LATE = [0.0]
pending = []

_stub('app', GetTime=lambda: now[0])
_stub('ui', Window=_Window)
_stub('chat', CHAT_TYPE_INFO=1,
      AppendChat=lambda kind, text: chats.append(text))
_stub('constInfo', myshop_data=shop)
_stub('ikashop', SendEditItem=_shop_edit)
_stub('offlineShopBuilder',
      SetPrivateShopItemPrice=lambda vnum, count, price, sockets: prices.append((vnum, price)))

sys.path.insert(0, ROOT)
import shoppricepump  # noqa: E402

FAILS = []


def check(what, expected, actual):
    if expected == actual:
        print('  OK   %s' % what)
    else:
        print('  FAIL %s: expected %r, got %r' % (what, expected, actual))
        FAILS.append(what)


def item(item_id, vnum=50300):
    return {'id': item_id, 'vnum': vnum, 'count': 1, 'sockets': (0, 0, 0)}


def stock(ids, price=1):
    """Put these lines on the counter, all at the same starting price."""
    shop['items'] = dict((n, {'id': i, 'price': price, 'vnum': 50300,
                              'count': 1, 'sockets': (0, 0, 0)})
                         for n, i in enumerate(ids))


def run_updates(seconds, step=0.05):
    # The client calls OnUpdate every frame; time moves in small steps, and a
    # late answer lands when its time has come.
    ticks = int(seconds / step)
    for _ in range(ticks):
        now[0] += step
        for entry in list(pending):
            if entry[0] <= now[0]:
                pending.remove(entry)
                _apply(entry[1], entry[2])
        shoppricepump._pump.OnUpdate()


print('== five lines leave one at a time ==')
del sent[:]
del prices[:]
now[0] = 100.0
shoppricepump.Queue([(item(i), 250000) for i in range(1, 6)])
shoppricepump._pump.OnUpdate()
check('the first packet goes at once', 1, len(sent))
now[0] += 0.1
shoppricepump._pump.OnUpdate()
check('and nothing follows inside the server window', 1, len(sent))
run_updates(0.2)
check('the second follows after the interval', 2, len(sent))
run_updates(2.0)
check('all five arrive', 5, len(sent))
check('each carries the price', [250000] * 5, [p for (_, p) in sent])
check('every line is a different item', [1, 2, 3, 4, 5], [i for (i, _) in sent])
check("the builder's copy is updated too", 5, len(prices))

print('== the interval is at least the server\'s own ==')
check('a quarter of a second, over the 200 ms limit', True, shoppricepump.TICK >= 0.2)

print('== a second click replaces what is left of the first ==')
del sent[:]
now[0] = 200.0
shoppricepump.Queue([(item(10 + i), 1000) for i in range(5)])
shoppricepump._pump.OnUpdate()
check('the first of the first batch went', 1, len(sent))
shoppricepump.Queue([(item(20), 7777)])
run_updates(2.0)
check('and only the new one follows it', 2, len(sent))
check('with the new price', 7777, sent[-1][1])

print('== an empty queue costs nothing ==')
del sent[:]
REFUSE.clear()
shop['items'] = {}
shoppricepump.Queue([])
run_updates(1.0)
check('nothing is sent', 0, len(sent))

# What blastyw reported: the packets are spaced out and a couple of lines are
# still skipped on a counter with many of the same item. One refused edit used
# to be lost for good; the pump reads the counter back and asks again.
print('== a refused line is asked for again ==')
del sent[:]
del chats[:]
now[0] = 300.0
REFUSE.clear()
REFUSE.add(3)
stock([1, 2, 3, 4, 5])
shoppricepump.Queue([(item(i), 250000) for i in range(1, 6)])
# Five lines leave over a second; the look at the counter waits VERIFY_DELAY
# after the last of them, and a line still at its old price is only looked at
# again until PATIENCE has passed - its answer may still be on its way.
run_updates(1.5)
check('every line was sent once', 5, len(sent))
run_updates(1.5)
check('nothing is sent again inside the patience', 5, len(sent))
run_updates(shoppricepump.PATIENCE)
check('the refused one is sent again', 6, len(sent))
check('and it is the right line', 3, sent[-1][0])
check('nothing is said while it is still being retried', 0, len(chats))

print('== a line the server never takes is reported, not retried for ever ==')
run_updates(60.0)
check('the retries stop', 1, len(chats))
check('and the line is named in the count', True, '1 pozycji' in chats[0])
check('the queue is empty afterwards', 0, len(shoppricepump._pump.edits))

print('== when every line takes, nothing is said ==')
del sent[:]
del chats[:]
now[0] = 400.0
REFUSE.clear()
stock([11, 12, 13])
shoppricepump.Queue([(item(i), 99000) for i in (11, 12, 13)])
run_updates(6.0)
check('three packets', 3, len(sent))
check('no complaint', 0, len(chats))
check('the counter carries the new price', [99000] * 3,
      [r['price'] for r in shop['items'].values()])

print('== a line sold while the pump ran is not a miss ==')
del sent[:]
del chats[:]
now[0] = 500.0
REFUSE.clear()
REFUSE.add(22)
stock([21, 22, 23])
shoppricepump.Queue([(item(i), 5000) for i in (21, 22, 23)])
run_updates(2.0)
# Somebody buys line 22 before the pump looks at the counter again.
shop['items'] = dict((n, r) for n, r in shop['items'].items() if r['id'] != 22)
run_updates(30.0)
check('the sold line is not asked for again', [21, 22, 23], [i for (i, _) in sent])
check('and nothing is said about it', 0, len(chats))

# What blastyw reported against client 2.0.23: every price changed on the
# first click and the chat still said lines had failed - the answers took
# longer than the pump waited, on a world of 1600 bots.
print('== a slow answer is not a refusal ==')
del sent[:]
del chats[:]
now[0] = 550.0
REFUSE.clear()
LATE[0] = 6.0
stock([41, 42, 43, 44])
shoppricepump.Queue([(item(i), 3300) for i in (41, 42, 43, 44)])
run_updates(60.0)
LATE[0] = 0.0
check('each line was sent once', [41, 42, 43, 44], [i for (i, _) in sent])
check('and nothing is said', 0, len(chats))
check('every price took', [3300] * 4, [r['price'] for r in shop['items'].values()])

print('== the counter being unreadable is not a complaint ==')
del sent[:]
del chats[:]
now[0] = 600.0
REFUSE.clear()
shop['items'] = {}
shoppricepump.Queue([(item(31), 1234)])
run_updates(30.0)
check('the edit went', 1, len(sent))
check('and the pump says nothing it cannot back up', 0, len(chats))

print('')
if FAILS:
    print('FAILED: %d' % len(FAILS))
    sys.exit(1)
print('PASS')
