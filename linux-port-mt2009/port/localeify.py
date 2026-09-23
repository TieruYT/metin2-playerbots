# -*- coding: utf-8 -*-
"""The language texts the client formats, made to take the arguments the root passes.

Usage:  python localeify.py --locale <directory the locale pack was extracted to>

    python tools/eterpack.py --profile mt2009 extract <Klient>/pack/locale <dir>

l0st3k's pack (20 September) brought six languages beside pl and en, and in
all of them, English included, some texts take other %-arguments than the
scripts hand them. A Python % that does not fit raises, and where it raises
decides what breaks. The one that was reported: REFINE_COST "Kosten: %d Yang"
against NumberToMoneyString's "1.000 Yang" is a TypeError in
RefineDialogNew.Open, before Show(). No window opens, and the server, already
in refine mode since it sent the dialog, refuses every move in the bag until
the next login ("nothing happens when I put the item on the blacksmith... the
whole inventory gets bugged out", 23 September; DE, ES, IT, PT, RO and TR all
had it). The rest fail more quietly: a yang pickup's chat line, a screenshot's,
the stat minus buttons, the item shop's buy button, the guild's dragon ghost,
a fish's length, the party skills.

The Polish text is the reference: its arguments are the ones the root passes
(it is what the scripts were written against), so every text of every
language is formatted with them after the edits below, and the script refuses
to write while one of them still raises - the same check, run on the whole
pack, is how these were found. English also has english_gui.py over it, which
already covered four of its five; its file is fixed anyway, because that
overlay is only applied while the language is "en".

Writes client-locale/locale/<lang>/<file> for each file it changes. Exact byte
edits, the files' own code pages and line ends kept (en is LF, the rest CRLF;
tr is cp1254, ro cp1250). Idempotent; re-run after a new locale pack.
"""
import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', 'client-locale', 'locale'))
FILES = ('locale_game.txt', 'locale_interface.txt')

# (key, what follows "KEY\t" on its line now, what it becomes). The rest of
# the line - the type column, a trailing tab, the carriage return - is part of
# what is matched, so a type can be changed as well as a text.
EDITS = {
    'en': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'You have received %d Yang.', b'You have received %s.'),
            # The arguments are (money, units), in that order.
            (b'GUILD_DO_YOU_HEAL_GSP', b'Do you want to restore %d units of Dragon Ghost for %d Yang?',
             b'Do you want to spend %s Yang to restore %d units of Dragon Ghost?'),
            (b'SCREENSHOT_SAVE1', b'is saved in\t', b"Screenshot saved to '%s'.\t"),
            (b'TOOLTIP_UNSEAL_LEFT_TIME', b'', b'Unseals in %d hours and %d minutes.'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'Chance to learn from books upgraded by factor 2.5 .\tSNA',
             b'Chance to learn from books upgraded by factor 2.5 .'),
        ],
    },
    'de': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'Du hast %d Yang erhalten.', b'Du hast %s erhalten.'),
            (b'GUILD_DO_YOU_HEAL_GSP',
             b'Willst du %d Yang verwenden, um %d Einheiten des Drachengeistes wiederherzustellen?',
             b'Willst du %s Yang verwenden, um %d Einheiten des Drachengeistes wiederherzustellen?'),
            (b'REFINE_COST', b'Kosten: %d Yang\t', b'Kosten: %s\t'),
            (b'SCREENSHOT_SAVE1', b'Es wird gespeichert in\t', b"Bildschirmfoto gespeichert in '%s'.\t"),
            (b'STAT_MINUS_CON', b'Vitalit\xe4tsanpassung', b'Vitalit\xe4tsanpassung (verbleibend: %d)'),
            (b'STAT_MINUS_DEX', b'Beweglichkeitsanpassung ', b'Beweglichkeitsanpassung (verbleibend: %d)'),
            (b'STAT_MINUS_INT', b'Intelligenz-Anpassung', b'Intelligenz-Anpassung (verbleibend: %d)'),
            # "Schaden" is damage; the button is strength's.
            (b'STAT_MINUS_STR', b'Schadensanpassung ', b'St\xe4rkeanpassung (verbleibend: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Kaufen\t', b'Kaufen %d (%s)\t'),
            (b'TOOLTIP_APPLY_BLEEDING_REDUCE', b'Widerstand gegen blutende Angriffe: +%d%%\tSNA',
             b'Widerstand gegen blutende Angriffe: +%d%%\tSA'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'Chance, aus B\xfcchern zu lernen, um den Faktor 2,5 erh\xf6ht\tSNA',
             b'Chance, aus B\xfcchern zu lernen, um den Faktor 2,5 erh\xf6ht'),
        ],
    },
    'es': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'Has recibido %d Yang.', b'Has recibido %s.'),
            (b'GUILD_DO_YOU_HEAL_GSP', b'\xbfDeseas restaurar con %d Yang %d el Dragon Fantasma?',
             b'\xbfDeseas restaurar con %s Yang %d el Dragon Fantasma?'),
            (b'REFINE_COST', b'Coste: %d Yang', b'Coste: %s'),
            (b'SCREENSHOT_SAVE1', b'est\xe1 guardada en', b"Captura de pantalla guardada en '%s'."),
            (b'STAT_MINUS_CON', b'Ajuste de vitalidad', b'Ajuste de vitalidad (restantes: %d)'),
            (b'STAT_MINUS_DEX', b'Ajuste de destreza', b'Ajuste de destreza (restantes: %d)'),
            (b'STAT_MINUS_INT', b'Ajuste de inteligencia', b'Ajuste de inteligencia (restantes: %d)'),
            (b'STAT_MINUS_STR', b'Ajuste de fuerza', b'Ajuste de fuerza (restantes: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Comprar', b'Comprar %d (%s)'),
            # "%,0f" is no conversion at all: ',' is not a flag Python knows.
            (b'PARTY_SKILL_ATTACKER', b'Valor de ataque b\xe1sico del atacante +%,0f',
             b'Valor de ataque b\xe1sico del atacante +%.0f'),
            (b'PARTY_SKILL_BERSERKER', b'Velocidad de ataque de Berserker +%,0f', b'Velocidad de ataque de Berserker +%.0f'),
            (b'PARTY_SKILL_BUFFER', b'Bloqueador de Duraci\xf3n de Habilidad +%,0f',
             b'Bloqueador de Duraci\xf3n de Habilidad +%.0f'),
            (b'PARTY_SKILL_DEFENDER', b'Defensa defensor +%,0f', b'Defensa defensor +%.0f'),
            (b'PARTY_SKILL_SKILL_MASTER', b'Max. SP Maesto de Habilidad +%,0f', b'Max. SP Maestro de Habilidad +%.0f'),
            (b'PARTY_SKILL_TANKER', b'Max. HP Luchador con hoja +%,0f', b'Max. HP Luchador con hoja +%.0f'),
            (b'TOOLTIP_FISH_LEN', b'Altura: %,2fcm', b'Longitud: %.2fcm'),
            (b'TOOLTIP_UNSEAL_LEFT_TIME', b'', b'Se desvincula del alma en: %dH %dM'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'Probabilidad de apreder de libros mejorada por factor 2.5 .\tSNA',
             b'Probabilidad de aprender de libros mejorada por factor 2.5 .'),
        ],
    },
    'it': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'Hai ricevuto una %di Yang.', b'Hai ricevuto %s.'),
            (b'GUILD_DO_YOU_HEAL_GSP',
             b'Desideri spendere %d Yang per rigenerare %d unit\xe0 di spirito del drago?',
             b'Desideri spendere %s Yang per rigenerare %d unit\xe0 di spirito del drago?'),
            (b'REFINE_COST', b'Cost: %d Yang\t', b'Costo: %s\t'),
            (b'SCREENSHOT_SAVE1', b'\xc8 salvato in\t', b"Screenshot salvato in '%s'.\t"),
            (b'STAT_MINUS_CON', b'Regolazione della vitalit\xe0', b'Regolazione della vitalit\xe0 (rimanenti: %d)'),
            (b'STAT_MINUS_DEX', b'Adeguamento abilit\xe0', b'Regolazione della destrezza (rimanenti: %d)'),
            (b'STAT_MINUS_INT', b"Regolazione dell'intelligenza", b"Regolazione dell'intelligenza (rimanenti: %d)"),
            # "danno" is damage; the button is strength's.
            (b'STAT_MINUS_STR', b'Adeguamento del danno', b'Regolazione della forza (rimanenti: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Compra\t', b'Compra %d (%s)\t'),
            (b'TOOLTIP_FISH_LEN', b'Lenght: %.2fcm', b'Lunghezza: %.2fcm'),
            (b'TOOLTIP_APPLY_BLEEDING_REDUCE', b"Resistenza all'attacco sanguinante: +%d%%\tSNA",
             b"Resistenza all'attacco sanguinante: +%d%%\tSA"),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'Possibilit\xe0 di imparare dai libri aggiornato del fattore 2.5\tSNA',
             b'Possibilit\xe0 di imparare dai libri aggiornato del fattore 2.5'),
        ],
    },
    'pt': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'Voc\xea recebeu %d Yang.', b'Voc\xea recebeu %s.'),
            (b'GUILD_DO_YOU_HEAL_GSP', b'Deseja restaurar com %d Yang %d o Drag\xe3o Fantasma?',
             b'Deseja restaurar com %s Yang %d o Drag\xe3o Fantasma?'),
            (b'REFINE_COST', b'Coste: %d Yang', b'Custo: %s'),
            (b'SCREENSHOT_SAVE1', b'est\xe1 guardada em', b"Captura de ecr\xe3 guardada em '%s'."),
            (b'STAT_MINUS_CON', b'Ajuste de vitalidade', b'Ajuste de vitalidade (restantes: %d)'),
            (b'STAT_MINUS_DEX', b'Ajuste de destreza', b'Ajuste de destreza (restantes: %d)'),
            (b'STAT_MINUS_INT', b'Ajuste de inteligencia', b'Ajuste de intelig\xeancia (restantes: %d)'),
            (b'STAT_MINUS_STR', b'Ajuste de fuerza', b'Ajuste de for\xe7a (restantes: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Comprar', b'Comprar %d (%s)'),
            (b'PARTY_SKILL_ATTACKER', b'Valor de ataque b\xe1sico do atacante +%,0f',
             b'Valor de ataque b\xe1sico do atacante +%.0f'),
            (b'PARTY_SKILL_BERSERKER', b'Velocidade de ataque de Berserker +%,0f', b'Velocidade de ataque de Berserker +%.0f'),
            (b'PARTY_SKILL_BUFFER', b'Bloqueador de Duraci\xf3n de Habilidad +%,0f',
             b'Bloqueador de Dura\xe7\xe3o de Habilidade +%.0f'),
            (b'PARTY_SKILL_DEFENDER', b'Defesa defensor +%,0f', b'Defesa defensor +%.0f'),
            (b'PARTY_SKILL_SKILL_MASTER', b'Max. SP Maestro de Habilidade +%,0f', b'Max. SP Mestre de Habilidade +%.0f'),
            (b'PARTY_SKILL_TANKER', b'Max. HP Luchador con Folha +%,0f', b'Max. HP Lutador com L\xe2mina +%.0f'),
            (b'TOOLTIP_FISH_LEN', b'Altura: %,2fcm', b'Comprimento: %.2fcm'),
            (b'TOOLTIP_UNSEAL_LEFT_TIME', b'', b'Desvincula-se da alma em: %dH %dM'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS',
             b'Probabilidade de valoriza\xe7\xe3o de livros melhorada por fator 2.5 .\tSNA',
             b'Probabilidade de valoriza\xe7\xe3o de livros melhorada por fator 2.5 .'),
        ],
    },
    'ro': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'Ai primit %d Yang.', b'Ai primit %s.'),
            (b'GUILD_DO_YOU_HEAL_GSP', b'Vrei sa restaurezi cu %d in dragonul fantoma?\t',
             b'Vrei sa folosesti %s Yang pentru a restaura %d unitati de Dragon Fantoma?\t'),
            (b'REFINE_COST', b'Cost: %d yang\t', b'Cost: %s\t'),
            (b'SCREENSHOT_SAVE1', b'Este salvat in\t', b"Captura de ecran a fost salvata in '%s'.\t"),
            (b'STAT_MINUS_CON', b'Reglarea vitalitatii', b'Reglarea vitalitatii (ramase: %d)'),
            (b'STAT_MINUS_DEX', b'Ajustarea abilitatilor', b'Ajustarea dexteritatii (ramase: %d)'),
            (b'STAT_MINUS_INT', b'Ajustarea inteligentei', b'Ajustarea inteligentei (ramase: %d)'),
            (b'STAT_MINUS_STR', b'Reglarea fortei', b'Reglarea fortei (ramase: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Cumpara\t', b'Cumpara %d (%s)\t'),
            (b'TOOLTIP_FISH_LEN', b'Inaltime: %, 2fcm\t', b'Lungime: %.2fcm\t'),
            # The arguments are (name, count, price).
            (b'DO_YOU_SELL_ITEM2', b'Doriti sa vindeti %s cu %s?\t', b'Doriti sa vindeti %s (%s buc.) pentru %s Yang?\t'),
            (b'OPTION_PVPMODE_PROTECT', b'Trebuie sa aveti cel putin nivelul d pentru a schimba modul pvp.\t',
             b'Trebuie sa aveti cel putin nivelul %d pentru a schimba modul pvp.\t'),
            (b'TOOLTIP_APPLY_BLEEDING_REDUCE', b'Rezistenta la Atac Sangeros: +%d%%\tSNA',
             b'Rezistenta la Atac Sangeros: +%d%%\tSA'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'\xaaansa de a \xeenv\xe3\xfea din c\xe3r\xfei de 2,5 ori mai bun\xe3\tSNA',
             b'\xaaansa de a \xeenv\xe3\xfea din c\xe3r\xfei de 2,5 ori mai bun\xe3'),
        ],
        'locale_interface.txt': [
            (b'SHOP_YANG_QUEST', b'Cu siguranta doriti sa obtineti %din magazinul \x84 %s\x94?',
             b'Cu siguranta doriti sa obtineti %s din magazinul \x84 %s\x94?'),
        ],
    },
    'tr': {
        'locale_game.txt': [
            (b'GAME_PICK_MONEY', b'%d Yang Kazand\xfdn.', b'%s Kazand\xfdn.'),
            (b'GUILD_DO_YOU_HEAL_GSP', b'%d Yang kullanarak %d Ejderha Hayaleti y\xfcklemek ister misin?',
             b'%s Yang kullanarak %d Ejderha Hayaleti y\xfcklemek ister misin?'),
            (b'REFINE_COST', b'Y\xfckseltme bedeli: %d Yang', b'Y\xfckseltme bedeli: %s'),
            (b'SCREENSHOT_SAVE1', b'Ekran G\xf6r\xfcnt\xfcs\xfc', b"Ekran g\xf6r\xfcnt\xfcs\xfc '%s' olarak kaydedildi."),
            (b'STAT_MINUS_CON', b'Canl\xfdl\xfdk Ayar\xfd', b'Canl\xfdl\xfdk Ayar\xfd (kalan: %d)'),
            (b'STAT_MINUS_DEX', b'\xc7eviklik Ayar\xfd', b'\xc7eviklik Ayar\xfd (kalan: %d)'),
            (b'STAT_MINUS_INT', b'Zeka Ayar\xfd', b'Zeka Ayar\xfd (kalan: %d)'),
            (b'STAT_MINUS_STR', b'G\xfc\xe7 Ayar\xfd', b'G\xfc\xe7 Ayar\xfd (kalan: %d)'),
            (b'ITEMSHOP_BUY_BUTTON', b'Sat\xfdn Al', b'Sat\xfdn Al %d (%s)'),
            (b'TOOLTIP_APPLY_SKILL_BOOK_BONUS', b'Kitaptan \xf6\xf0renme \xfeans\xfd 2,5 artt\xfd\tSNA',
             b'Kitaptan \xf6\xf0renme \xfeans\xfd 2,5 artt\xfd'),
        ],
        'locale_interface.txt': [
            # uiscriptlocale reads two columns, so this button said "CLOSE".
            (b'CLOSE', b'CLOSE\tKapat', b'Kapat'),
        ],
    },
}

# The code page each language's files are written in; only for reading the
# texts back in the check - the edits are bytes.
ENCODING = {'pl': 'cp1250', 'ro': 'cp1250', 'tr': 'cp1254'}
CONVERSION = re.compile(r'%(?:\([^)]*\))?[#0\- +]*(?:\*|\d+)?(?:\.(?:\*|\d+))?[hlL]?([diouxXeEfFgGcrs%])')


def line_span(data, key):
    """Where the rest of KEY's line starts and ends (end before \\r or \\n)."""
    starts = []
    if data.startswith(key + b'\t'):
        starts.append(len(key) + 1)
    pos = 0
    needle = b'\n' + key + b'\t'
    while True:
        i = data.find(needle, pos)
        if i < 0:
            break
        starts.append(i + len(needle))
        pos = i + 1
    if len(starts) != 1:
        return None, len(starts)
    s = starts[0]
    e = s
    while e < len(data) and data[e:e + 1] not in (b'\r', b'\n'):
        e += 1
    return (s, e), 1


def ends_its_last_line(data):
    """The client's loader cuts the last character of every line it reads.

    localeinfo.LoadLocaleFile takes line[:-1], counting on the newline, and
    the pack's readline hands the last line over without one when the file
    does not end in it. es/locale_game.txt did not: its last line is
    "NEW_AFFECT_AUTO_METIN_FARM<tab>Premium Metinfarm<tab>SNA", the type came
    out as "SN", the loader raised on it, and a client set to Spanish stopped
    with a message box before the login.
    """
    if data.endswith(b'\n'):
        return data
    return data + (b'\r\n' if b'\r\n' in data else b'\n')


FUNCTION_TYPES = ('SA', 'SNA', 'SAA', 'SAN', 'SAAAA')


def unreadable_lines(data):
    """Lines localeinfo.LoadLocaleFile would raise on, read the way the client reads them."""
    text = b'\n'.join(data.split(b'\r\n'))  # pack_file in mode "r"
    lines = text.split(b'\n')
    if lines and lines[-1] == b'':
        lines.pop()
        last_complete = True
    else:
        last_complete = False
    bad = []
    for n, line in enumerate(lines, 1):
        cut = line if (n < len(lines) or last_complete) else line[:-1]
        tokens = cut.split(b'\t')
        if len(tokens) >= 3:
            kind = tokens[2].strip().decode('latin-1')
            if kind and kind not in FUNCTION_TYPES:
                bad.append('line %d: type %r' % (n, kind))
    return bad


def apply_edits(data, lang, name, edits):
    for key, old, new in edits:
        span, count = line_span(data, key)
        if span is None:
            raise SystemExit('localeify: %s/%s: %s is on %d lines, expected one' % (lang, name, key.decode(), count))
        s, e = span
        rest = data[s:e]
        if rest == new:
            continue
        if rest != old:
            raise SystemExit('localeify: %s/%s: %s reads %r, expected %r' % (lang, name, key.decode(), rest, old))
        data = data[:s] + new + data[e:]
    return data


def load(data, lang):
    out = {}
    for raw in data.split(b'\n'):
        line = raw.decode(ENCODING.get(lang, 'cp1252'), 'replace')
        if line.endswith('\r'):
            line = line[:-1]
        tokens = line.split('\t')
        if len(tokens) < 2:
            continue
        out[tokens[0]] = (tokens[1], tokens[2].strip() if len(tokens) >= 3 else '')
    return out


def arguments(text):
    specs = [c for c in CONVERSION.findall(text) if c != '%']
    return tuple('x' if c in 'sr' else 1 for c in specs)


def problems(polish, other):
    """Texts of `other` that would raise, or carry another type, where the Polish one does not."""
    found = []
    for key, (text, kind) in sorted(other.items()):
        if key not in polish:
            continue
        ptext, pkind = polish[key]
        if kind != pkind:
            found.append('%s: type %r, the Polish text has %r' % (key, kind, pkind))
        args = arguments(ptext)
        if not args:
            continue
        try:
            ptext % args
        except Exception:
            continue  # the Polish text does not format either; nothing to hold this one to
        try:
            text % args
        except Exception as e:
            found.append('%s: %r: %s' % (key, text, e))
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--locale', required=True, help='directory holding the extracted locale pack')
    args = parser.parse_args()
    base = os.path.join(args.locale, 'locale')
    if not os.path.isdir(base):
        base = args.locale  # given the locale/ directory itself
    for lang, files in EDITS.items():
        for name in files:
            if not os.path.isfile(os.path.join(base, lang, name)):
                raise SystemExit('localeify: no %s/%s in %s' % (lang, name, base))
    langs = sorted(d for d in os.listdir(base) if d not in ('common', 'pl') and os.path.isdir(os.path.join(base, d)))
    rendered = {}
    remaining = []
    for name in FILES:
        polish = load(io.open(os.path.join(base, 'pl', name), 'rb').read(), 'pl')
        for lang in langs:
            src = os.path.join(base, lang, name)
            if not os.path.isfile(src):
                continue
            original = io.open(src, 'rb').read()
            data = apply_edits(original, lang, name, EDITS.get(lang, {}).get(name, []))
            data = ends_its_last_line(data)
            if data != original:
                rendered[(lang, name)] = data
            where = '%s/%s' % (lang, name)
            remaining += ['%s %s' % (where, p) for p in problems(polish, load(data, lang))]
            if name == 'locale_game.txt':  # uiscriptlocale's loader never raises
                remaining += ['%s %s' % (where, p) for p in unreadable_lines(data)]
    if remaining:
        sys.stderr.write('localeify: texts the client would still trip on:\n  %s\n' % '\n  '.join(remaining))
        raise SystemExit(1)
    for (lang, name), data in sorted(rendered.items()):
        out = os.path.join(OUT, lang, name)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        io.open(out, 'wb').write(data)
        print('localeify: client-locale/locale/%s/%s (%d bytes)' % (lang, name, len(data)))


if __name__ == '__main__':
    main()
