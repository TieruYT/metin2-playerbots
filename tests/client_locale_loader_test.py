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
formatted amount is a traceback in the player's face, not a wrong word.

Usage (Python 2.7, the client's own):
    python tests/client_locale_loader_test.py <client-root> <locale dir>

<locale dir> is a pack/locale extraction; it must hold locale/pl and locale/en.
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

    for language in ('pl', 'en', 'de', 'tr'):
        if not os.path.isdir(os.path.join(locale_dir, 'locale', language)):
            print('  %s: brak katalogu, pomijam' % language)
            continue
        opener = stub_engine(locale_dir, language)
        game = run_loader(root, 'localeinfo.py', opener, 'if app.ENABLE_CHEQUE_SYSTEM:')
        ui = run_loader(root, 'uiscriptlocale.py', opener)
        if language == 'en':
            for key, value in english_gui.GAME.items():
                if game.get(key) != value:
                    failures.append('EN GAME %s: %r' % (key, game.get(key)))
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
