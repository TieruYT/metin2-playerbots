# -*- coding: utf-8 -*-
"""The client's locale loaders, run for real with the engine modules stubbed.

Eight languages ship in the locale pack since 2.0.23 and the English interface
is an overlay: localeinfo.py and uiscriptlocale.py apply english_gui.GAME/UI
only while systemSetting.GetLanguage() is "en", after the shared files have
been read. Two things have to hold and neither is visible from a screenshot:

  * for "en", every value of english_gui really wins - it is applied after the
    locale files, not before, and a key the EN locale_interface.txt happens to
    carry must not put Polish back;
  * for every other language, nothing moved. The overlay adds keys through
    setdefault with the Polish text, so PL/DE/TR read exactly what they read
    before the multilanguage work went in.

The format strings are checked too, because a %d where the caller hands a
formatted amount is a traceback in the player's face, not a wrong word - and
for every language in the pack, since 23 September: REFINE_COST took a number
in six of them, the refine window raised before it opened, and the server,
already in refine mode, locked those players' bags until the next login.
Every language is loaded through the client's own loader (a line it cannot
read raises there as it does in the client: es/locale_game.txt had no newline
at its end and stopped a Spanish client before the login), the call sites the
root formats are formatted with the arguments the scripts pass, and every text
whose Polish counterpart formats with its own arguments has to format with them
too.

Usage (Python 2.7, the client's own):
    python tests/client_locale_loader_test.py <client-root> <locale dir>

<locale dir> is a pack/locale extraction with linux-port-mt2009/client-locale
copied over it; it must hold locale/pl and locale/en.
Derived from the loader test Codex wrote for the English GUI work.
"""
import os
import sys
import types


def stub_engine(locale_dir, language):
    class App(types.ModuleType):
        def __getattr__(self, name):
            if name.startswith('ENABLE_'):
                return name in ('ENABLE_LOCALE_COMMON', 'ENABLE_IKASHOP_RENEWAL')
            raise AttributeError(name)
    app = App('app')
    app.GetLocalePath = lambda: 'locale/pl'
    app.GetDefaultCodePage = lambda: 1250
    app.GetLocaleServiceName = lambda: 'EUROPE'
    sys.modules['app'] = app
    pack = types.ModuleType('pack')
    pack.Exist = lambda path: os.path.exists(os.path.join(locale_dir, path))
    sys.modules['pack'] = pack
    sys.modules['flamewindPath'] = types.ModuleType('flamewindPath')
    # The loader's own error path: a line it cannot read is a message box and
    # a re-raise in the client, a line on stdout and the same re-raise here.
    dbg = types.ModuleType('dbg')
    dbg.LogBox = lambda text, *rest: sys.stdout.write('  LogBox: %r\n' % text)
    sys.modules['dbg'] = dbg
    setting = types.ModuleType('systemSetting')
    setting.GetLanguage = lambda: language
    sys.modules['systemSetting'] = setting
    return lambda path, mode='r': open(os.path.join(locale_dir, path), 'rU')


def run_loader(root, name, opener, cut=None):
    text = open(os.path.join(root, name), 'rb').read()
    if cut:
        text = text.split(cut)[0]
    ns = {'open': opener}
    exec compile(text, name, 'exec') in ns
    return ns


# The call sites whose arguments are fixed by the root's scripts, formatted
# exactly as the scripts format them. REFINE_COST is the one that locked a bag:
# RefineDialogNew.Open raised on it before Show(), and the server, already in
# refine mode, refused every move until the next login (23 September).
CALL_SITES = (
    ('GAME', 'REFINE_COST', '1.000 Yang'),                    # uirefine.RefineDialogNew.Open
    ('GAME', 'REFINE_SUCCESS_PROBALITY', 90),                 # uirefine
    ('GAME', 'GAME_PICK_MONEY', '1.000 Yang'),                # game.OnGoldUpdate / OnPickMoney
    ('GAME', 'SCREENSHOT_SAVE1', 'screenshot.jpg'),           # game.SaveScreen
    ('GAME', 'GUILD_DO_YOU_HEAL_GSP', ('1.000', 5)),          # uiguild: (money, units)
    ('GAME', 'STAT_MINUS_CON', 3),                            # localeinfo STAT_TOOLTIP_DICT
    ('GAME', 'STAT_MINUS_DEX', 3),
    ('GAME', 'STAT_MINUS_INT', 3),
    ('GAME', 'STAT_MINUS_STR', 3),
    ('GAME', 'ITEMSHOP_BUY_BUTTON', (2, '200 SM')),           # uiitemshop
    ('GAME', 'DO_YOU_SELL_ITEM2', ('Miecz', 2, '1.000')),     # localeinfo.DO_YOU_SELL_ITEM
    ('GAME', 'OPTION_PVPMODE_PROTECT', 15),                   # game / uigameoption / uioption
    ('GAME', 'TOOLTIP_UNSEAL_LEFT_TIME', (1, 30)),            # uitooltip
    ('GAME', 'TOOLTIP_FISH_LEN', 12.5),                       # uitooltip
    ('GAME', 'PARTY_SKILL_ATTACKER', 10.0),                   # uitooltip party skills
    ('GAME', 'PARTY_SKILL_BERSERKER', 10.0),
    ('GAME', 'PARTY_SKILL_BUFFER', 10.0),
    ('GAME', 'PARTY_SKILL_DEFENDER', 10.0),
    ('GAME', 'PARTY_SKILL_SKILL_MASTER', 10.0),
    ('GAME', 'PARTY_SKILL_TANKER', 10.0),
    ('UI', 'SYSTEM_VERSION', (1, 1, 0, '')),
)


def check_call_sites(language, game, ui):
    failures = []
    for where, key, args in CALL_SITES:
        table = game if where == 'GAME' else ui
        if key not in table:
            continue
        try:
            table[key] % args
        except Exception, exc:                                   # noqa: E722
            failures.append('%s %s %s %r: %s' % (language, where, key, table[key], exc))
    return failures


def arguments_of(text):
    import re
    specs = re.findall(r'%(?:\([^)]*\))?[#0\- +]*(?:\*|\d+)?(?:\.(?:\*|\d+))?[hlL]?([diouxXeEfFgGcrs%])', text)
    return tuple('x' if c in 'sr' else 1 for c in specs if c != '%')


def check_against_polish(language, game, ui, polish):
    """Every text the Polish one formats with its own arguments has to format too.

    Loaded through the client's own loader, so a typed line (SA, SNA...) is the
    function the scripts call and a plain one the string they format.
    """
    failures = []
    for table, reference, where in ((game, polish[0], 'GAME'), (ui, polish[1], 'UI')):
        for key, ref in reference.items():
            value = table.get(key)
            if value is None or not isinstance(ref, str):
                if callable(ref) and not callable(value):
                    failures.append('%s %s %s: a text where Polish has a function' % (language, where, key))
                continue
            args = arguments_of(ref)
            if not args or not isinstance(value, str):
                continue
            try:
                ref % args
            except Exception:                                    # noqa: E722
                continue
            try:
                value % args
            except Exception, exc:                               # noqa: E722
                failures.append('%s %s %s %r: %s' % (language, where, key, value, exc))
    return failures


def main():
    root = sys.argv[1]
    locale_dir = sys.argv[2]
    base = sys.argv[3] if len(sys.argv) > 3 else None
    sys.path.insert(0, root)
    import english_gui

    failures = []
    for folder, _dirs, files in os.walk(root):
        for name in files:
            if name.endswith('.py'):
                path = os.path.join(folder, name)
                compile(open(path, 'rb').read(), path, 'exec')

    languages = sorted(d for d in os.listdir(os.path.join(locale_dir, 'locale'))
                       if d != 'common' and os.path.isdir(os.path.join(locale_dir, 'locale', d)))
    polish = None
    for language in ['pl'] + [l for l in languages if l != 'pl']:
        opener = stub_engine(locale_dir, language)
        game = run_loader(root, 'localeinfo.py', opener, 'if app.ENABLE_CHEQUE_SYSTEM:')
        ui = run_loader(root, 'uiscriptlocale.py', opener)
        if polish is None:
            polish = (game, ui)
        failures += check_call_sites(language, game, ui)
        failures += check_against_polish(language, game, ui, polish)
        if language == 'en':
            for key, value in english_gui.GAME.items():
                got = game.get(key)
                # A typed line stays the function the scripts call
                # (WHISPER_ERROR[mode](name)); its English text is what the
                # function returns.
                if callable(got):
                    if '%' in value.replace('%%', ''):
                        continue
                    got = got(None)
                if got != value:
                    failures.append('EN GAME %s: %r' % (key, got))
            for key, value in english_gui.UI.items():
                if ui.get(key) != value:
                    failures.append('EN UI %s: %r' % (key, ui.get(key)))
            # Formaty, ktore wolaja miejsca w kodzie klienta.
            try:
                game['GAME_PICK_MONEY'] % '1,000 Yang'
                game['SCREENSHOT_SAVE1'] % 'test.jpg'
                game['REFINE_COST'] % '1,000 Yang'
                ui['SYSTEM_VERSION'] % (1, 1, 0, '')
            except Exception, exc:                                   # noqa: E722
                failures.append('EN format: %s' % exc)
        elif base:
            # Nic sie nie ruszylo w pozostalych jezykach.
            og = run_loader(base, 'localeinfo.py', opener, 'if app.ENABLE_CHEQUE_SYSTEM:')
            ou = run_loader(base, 'uiscriptlocale.py', opener)
            for old, new, what in ((og, game, 'GAME'), (ou, ui, 'UI')):
                for key, value in old.items():
                    if isinstance(value, str) and new.get(key) != value:
                        failures.append('%s %s %s: %r != %r' % (language, what, key, new.get(key), value))
        print('  %s: loader OK' % language)

    if failures:
        print('\nBLEDY (%d):' % len(failures))
        for f in failures[:20]:
            print('  ' + f)
        sys.exit(1)
    print('client_locale_loader_test: PASS')


if __name__ == '__main__':
    main()
